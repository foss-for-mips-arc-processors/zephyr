/*
 * Copyright (c) 2026 Synopsys.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/toolchain.h>
#include <zephyr/linker/sections.h>
#include <zephyr/arch/arc/asm-compat/assembler.h>

GTEXT(arc_spin_holding_scratch)
GTEXT(arc_clobber_scratch)

/*
 * uintptr_t arc_spin_holding_scratch(volatile uint32_t *flag, uintptr_t value);
 *
 * Put @value in r30 and spin until *@flag becomes non-zero, then return r30.
 *
 * r30 stays live across the whole loop, so an interrupt taken anywhere inside
 * it preempts a thread whose r30 is part of its live state. Written in
 * assembly so neither toolchain can decide to spill it.
 */
SECTION_FUNC(TEXT, arc_spin_holding_scratch)
	MOVR r30, r1
arc_scratch_spin_wait:
	ld r2, [r0]
	breq r2, 0, arc_scratch_spin_wait
	MOVR r0, r30
	j_s [blink]

/*
 * void arc_clobber_scratch(uintptr_t value);
 *
 * Overwrite every caller-saved register that the ARC port carries inside
 * struct _callee_saved_stack. This stands in for whatever the compiler would
 * have left in these registers on its own.
 */
SECTION_FUNC(TEXT, arc_clobber_scratch)
	MOVR r30, r0
#ifdef CONFIG_ARC_HAS_ACCL_REGS
	MOVR r58, r0
#ifndef CONFIG_64BIT
	MOVR r59, r0
#endif /* !CONFIG_64BIT */
#endif /* CONFIG_ARC_HAS_ACCL_REGS */
	j_s [blink]
