/*
 * Copyright (c) 2026 GlobalFoundries Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/arch/riscv/csr.h>
#include <zephyr/devicetree.h>
#include <zephyr/platform/hooks.h>
#include <zephyr/sys/util.h>
#include <zephyr/toolchain.h>

#define ARCV_PMACFG0  0x7ce
#define ARCV_PMAADDR0 0x7d8

#define ARCV_PMA_DEVICE_NON_IDEMPOTENT 0x1U
#define ARCV_PMA_MIN_SIZE              0x1000U

#define ARCV_PMA_NAPOT(base, size) (((base) >> 2) | (((size) >> 3) - 1U))

#define UART_BASE DT_REG_ADDR(DT_NODELABEL(uart0))
#define UART_SIZE DT_REG_SIZE(DT_NODELABEL(uart0))

BUILD_ASSERT(IS_POWER_OF_TWO(UART_SIZE) && (UART_SIZE >= ARCV_PMA_MIN_SIZE),
	     "uart0 size must be a power of two and at least 4 KB for a PMA NAPOT region");
BUILD_ASSERT((UART_BASE & (UART_SIZE - 1U)) == 0U,
	     "uart0 base must be aligned to its size for a PMA NAPOT region");

void soc_early_init_hook(void)
{
	csr_write(ARCV_PMAADDR0, ARCV_PMA_NAPOT(UART_BASE, UART_SIZE));
	/* pmacfg0 holds entries 0 to 3, this also leaves entries 1 to 3 off */
	csr_write(ARCV_PMACFG0, ARCV_PMA_DEVICE_NON_IDEMPOTENT);
}
