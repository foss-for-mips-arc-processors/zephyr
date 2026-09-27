# Copyright (c) 2026 Synopsys
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(arc_scratch_regs)

target_sources(app PRIVATE src/main.c src/scratch_helpers.S)

# Poison after z_arch_get_next_switch_handle() returns, matching the real
# window before _irq_store_old_thread_callee_regs runs.
target_link_options(app PRIVATE -Wl,--wrap=z_arch_get_next_switch_handle)
