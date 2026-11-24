# SPDX-License-Identifier: Apache-2.0

zephyr_sources_ifdef(CONFIG_SOC_RMX_UART_DEVICE_PMA soc.c)

set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/riscv/common/linker.ld CACHE INTERNAL "")
