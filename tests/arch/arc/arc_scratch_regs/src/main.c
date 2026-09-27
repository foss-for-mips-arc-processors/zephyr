/*
 * Copyright (c) 2026 Synopsys.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * The ARC port carries r30 (and r58/r59 when the accumulator is present) in
 * struct _callee_saved_stack, but those registers are caller-saved under the C
 * ABI. They are saved only after the interrupt exit path has already called
 * into the scheduler, and they are restored before the switch-in path has
 * finished calling into C. Anything those C calls leave behind therefore
 * becomes, or destroys, thread state.
 *
 * Rather than wait for the compiler to allocate r30 on its own, these tests
 * clobber it deliberately from inside each window, using the user tracing
 * hooks that the kernel already calls from exactly those two places:
 *
 *   __wrap_z_arch_get_next_switch_handle() after the real call returns
 *       i.e. between the scheduler call and _irq_store_old_thread_callee_regs
 *
 *   sys_trace_thread_switched_in_user()
 *       z_thread_mark_switched_in(), which the switch paths bl into after
 *       _load_callee_saved_regs has already restored the incoming thread
 */

#include <zephyr/kernel.h>
#include <zephyr/ztest.h>

#define SCRATCH_MAGIC  ((uintptr_t)0xC0FFEE01)
#define SCRATCH_POISON ((uintptr_t)0xDEADBEEF)

#define PREEMPTIONS  8
#define KICK_PERIOD  K_MSEC(1)
#define STACK_SIZE   2048

#define VICTIM_PRIO    5
#define PREEMPTOR_PRIO 2

uintptr_t arc_spin_holding_scratch(volatile uint32_t *flag, uintptr_t value);
void arc_clobber_scratch(uintptr_t value);

/* Override the __weak stub in subsys/tracing/user/tracing_user.c */
void sys_trace_thread_switched_in_user(void);

enum poison_point {
	POISON_NONE,
	POISON_SWITCHED_OUT,
	POISON_SWITCHED_IN,
};

static volatile enum poison_point poison_at = POISON_NONE;

static K_THREAD_STACK_DEFINE(victim_stack, STACK_SIZE);
static K_THREAD_STACK_DEFINE(preemptor_stack, STACK_SIZE);
static struct k_thread victim_thread;
static struct k_thread preemptor_thread;

/* Binary sem would drop timer kicks while the preemptor is still running. */
static K_SEM_DEFINE(kick_sem, 0, PREEMPTIONS);
static K_SEM_DEFINE(victim_done, 0, 1);

static volatile uint32_t release_flag;
static volatile uintptr_t observed;
static bool scenario_threads_active;
static bool poison_out_applied;
/* True only while victim is inside arc_spin_holding_scratch(). */
static volatile bool victim_holding_scratch;

/*
 * Both hooks only fire for the victim, which is the one thread built to
 * tolerate a destroyed r30. Poisoning every switch would corrupt the test
 * harness itself and turn a clean assertion failure into a crash.
 */
void *__real_z_arch_get_next_switch_handle(struct k_thread **old_thread);

void *__wrap_z_arch_get_next_switch_handle(struct k_thread **old_thread)
{
	void *next = __real_z_arch_get_next_switch_handle(old_thread);

	/*
	 * Clobber here, not in sys_trace_thread_switched_out_user(): the tracing
	 * hook runs inside z_get_next_switch_handle() while the scheduler spinlock
	 * is still held. Trashing caller-saved regs there can hang the kernel.
	 * This matches the assembly window after "bl z_arch_get_next_switch_handle"
	 * and before _irq_store_old_thread_callee_regs.
	 */
	if (poison_at == POISON_SWITCHED_OUT && victim_holding_scratch &&
	    !poison_out_applied && old_thread != NULL &&
	    *old_thread == &victim_thread) {
		arc_clobber_scratch(SCRATCH_POISON);
		poison_out_applied = true;
	}

	return next;
}

void sys_trace_thread_switched_in_user(void)
{
	/* _current has already been updated to the incoming thread */
	if (poison_at == POISON_SWITCHED_IN && victim_holding_scratch &&
	    k_current_get() == &victim_thread) {
		arc_clobber_scratch(SCRATCH_POISON);
	}
}

static void kick_timer_fn(struct k_timer *timer)
{
	ARG_UNUSED(timer);

	/* ISR context: readies a higher priority thread, so the interrupt
	 * exit path has to take the context switch branch.
	 */
	k_sem_give(&kick_sem);
}

static K_TIMER_DEFINE(kick_timer, kick_timer_fn, NULL);

static void victim_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	victim_holding_scratch = true;
	observed = arc_spin_holding_scratch(&release_flag, SCRATCH_MAGIC);
	victim_holding_scratch = false;
	k_sem_give(&victim_done);
}

static void preemptor_fn(void *a, void *b, void *c)
{
	ARG_UNUSED(a);
	ARG_UNUSED(b);
	ARG_UNUSED(c);

	for (int i = 0; i < PREEMPTIONS; i++) {
		k_sem_take(&kick_sem, K_FOREVER);
	}

	release_flag = 1;
	k_thread_abort(k_current_get());
}

static void threads_abort_and_join(void)
{
	k_timer_stop(&kick_timer);
	poison_at = POISON_NONE;

	if (!scenario_threads_active) {
		return;
	}

	k_thread_abort(&preemptor_thread);
	k_thread_abort(&victim_thread);
	(void)k_thread_join(&preemptor_thread, K_SECONDS(1));
	(void)k_thread_join(&victim_thread, K_SECONDS(1));
	scenario_threads_active = false;

	/* Let abort/teardown finish on all CPUs before reusing the structs. */
	k_msleep(10);
}

static uintptr_t run_scenario(enum poison_point where)
{
	threads_abort_and_join();

	release_flag = 0;
	observed = 0;
	poison_at = where;
	poison_out_applied = false;
	victim_holding_scratch = false;
	k_sem_reset(&kick_sem);
	k_sem_reset(&victim_done);

	k_thread_create(&victim_thread, victim_stack,
			K_THREAD_STACK_SIZEOF(victim_stack),
			victim_fn, NULL, NULL, NULL,
			VICTIM_PRIO, 0, K_FOREVER);
	k_thread_create(&preemptor_thread, preemptor_stack,
			K_THREAD_STACK_SIZEOF(preemptor_stack),
			preemptor_fn, NULL, NULL, NULL,
			PREEMPTOR_PRIO, 0, K_FOREVER);

#if defined(CONFIG_SMP) && defined(CONFIG_SCHED_CPU_MASK)
	/*
	 * Timer ISRs and the IRQ-exit context switch run on the CPU that took
	 * the interrupt. A victim spinning on another core is not preempted by
	 * those IRQs, so the kick/preemptor pattern never makes progress.
	 *
	 * Pin before k_thread_start(): cpu_mask changes are rejected on runnable
	 * threads.
	 */
	zassert_ok(k_thread_cpu_pin(&victim_thread, 0), NULL);
	zassert_ok(k_thread_cpu_pin(&preemptor_thread, 0), NULL);
#endif

	k_thread_start(&victim_thread);
	k_thread_start(&preemptor_thread);

	scenario_threads_active = true;
	k_timer_start(&kick_timer, KICK_PERIOD, KICK_PERIOD);

	int victim_rc = k_sem_take(&victim_done, K_SECONDS(5));

	threads_abort_and_join();

	zassert_ok(victim_rc, "victim thread never completed its spin loop");

	return observed;
}

/*
 * Control case. Nothing poisons anything, so this passes on a healthy port and
 * on a broken one alike, unless the compiler happens to allocate r30 inside
 * the scheduler by itself. A failure here is the bug reproducing naturally.
 */
/* ztest runs cases in link order; keep the control case first. */
ZTEST(arc_scratch_regs, test_scratch_regs_01_undisturbed)
{
	uintptr_t got = run_scenario(POISON_NONE);

	zassert_equal(got, SCRATCH_MAGIC,
		      "r30 changed across preemption with no injected clobber: "
		      "expected 0x%lx, got 0x%lx",
		      (unsigned long)SCRATCH_MAGIC, (unsigned long)got);
}

/*
 * Outgoing path: the scheduler clobbers r30 before
 * _irq_store_old_thread_callee_regs writes the preempted thread's context.
 */
ZTEST(arc_scratch_regs, test_scratch_regs_02_survive_scheduler_call)
{
	uintptr_t got = run_scenario(POISON_SWITCHED_OUT);

	zassert_equal(got, SCRATCH_MAGIC,
		      "r30 clobbered inside z_arch_get_next_switch_handle() was "
		      "saved into the preempted thread: expected 0x%lx, got 0x%lx",
		      (unsigned long)SCRATCH_MAGIC, (unsigned long)got);
}

/*
 * Incoming path: r30 is restored by _load_callee_saved_regs and then destroyed
 * by C code that still runs before control returns to the thread.
 */
ZTEST(arc_scratch_regs, test_scratch_regs_03_survive_switch_in)
{
	uintptr_t got = run_scenario(POISON_SWITCHED_IN);

	zassert_equal(got, SCRATCH_MAGIC,
		      "r30 clobbered after _load_callee_saved_regs never reached "
		      "the resumed thread: expected 0x%lx, got 0x%lx",
		      (unsigned long)SCRATCH_MAGIC, (unsigned long)got);
}

ZTEST_SUITE(arc_scratch_regs, NULL, NULL, NULL, NULL, NULL);
