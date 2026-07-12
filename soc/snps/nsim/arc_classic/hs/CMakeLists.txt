# SPDX-License-Identifier: Apache-2.0

if(COMPILER STREQUAL gcc)
  # GNU compiler options
  zephyr_compile_options(-mcpu=${GCC_M_CPU})

  if(CONFIG_ISA_ARCV2)
    # ISA_ARCV2 & 32BIT
    zephyr_compile_options(-mno-sdata)
    zephyr_compile_options_ifdef(CONFIG_CPU_ARCHS -mmpy-option=plus_qmacw)
    zephyr_compile_options_ifdef(CONFIG_FPU -mfpu=fpud_all)
  endif()
else()
  # MWDT compiler options

  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS -arcv2hs -core2 -Xatomic
    -Xll64 -Xdiv_rem=radix4 -Xunaligned -Xcode_density -Xswap -Xbitscan -Xmpy_option=qmpyh
    -Xshift_assist -Xbarrel_shifter -Xfpud_div -Xfpu_mac -Xrtc -Xtimer0 -Xtimer1)

  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS -Hlib=hs38_full)

  # Repurposed to model Autotalks Tekton "HS48x2" (ARC HS4x dual-core) as
  # closely as possible for nSIM runs. CPU options taken verbatim from the
  # Autotalks .tcf 'ccac.arg' (minus CCM/ECC codegen which needs the exact
  # Autotalks Harvard memory map).
  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS_SMP -arcv2hs -core4 -Xdual_issue -uarch_rev=3:2
    -Hrgf_num_banks=2 -Hrgf_banked_regs=32 -HL -Xatomic -Xll64 -Xunaligned -Xdiv_rem=radix4
    -Xcode_density -Xswap -Xbitscan -Xmpy_option=qmpyh -Xshift_assist -Xbarrel_shifter
    -Xfastmath_div -Xfastmath_sqrt -Xfastmath_sat -Xfastmath_trig -Xfpud_div -Xfpu_mac -Xfpu_hp
    -Xtimer0 -Xtimer1 -Xrtc -Xstack_check -dcache=32768,64,2,a -Hld_cycles=2 -Xlpb_size=64)

  # HS48 core with a System-Level (L2) Cache -> hs48_slc_full runtime library.
  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS_SMP -Hlib=hs48_slc_full)

  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS_MPUV6 -arcv2hs -core2 -Xatomic
    -Xll64 -Xdiv_rem=radix4 -Xunaligned -Xcode_density -Xswap -Xbitscan -Xmpy_option=qmpyh
    -Xshift_assist -Xbarrel_shifter -Xfpud_div -Xfpu_mac -Xrtc -Xtimer0 -Xtimer1)

  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS_MPUV6 -Hlib=hs38_full)

  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS5X -arcv3hs -core0 -Xdual_issue -uarch_rev=0:0
    -HL -Hlpc_width=0 -Xatomic=2 -Xll64 -Xunaligned -Xdiv_rem=radix4 -Xmpy_option=qmpyh
    -Xtimer0 -Xtimer1 -Xrtc -dcache=32768,64,2,a -Hld_cycles=1)

  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS5X -Hlib=hs58_full)

  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS5X_SMP -arcv3hs -core0 -Xdual_issue -uarch_rev=0:0
    -HL -Hlpc_width=0 -Xatomic=2 -Xll64 -Xunaligned -Xdiv_rem=radix4 -Xmpy_option=qmpyh -Xtimer0
    -Xtimer1 -Xrtc -dcache=32768,64,2,a -Hld_cycles=1)

  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS5X_SMP -Hlib=hs58_full)

  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS6X -arc64 -core0 -uarch_rev=0:0 -HL -Xatomic=2
    -Xunaligned -Xmpy_cycles=3 -Xtimer0 -Xtimer1 -Xrtc -dcache=32768,64,2,a -Hld_cycles=1)

  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS6X -Hlib=hs68_full_zephyr)

  zephyr_compile_options_ifdef(CONFIG_SOC_NSIM_HS6X_SMP -arc64 -core0 -uarch_rev=0:0 -HL -Xatomic=2
    -Xunaligned -Xmpy_cycles=3 -Xtimer0 -Xtimer1 -Xrtc -dcache=32768,64,2,a -Hld_cycles=1)

  zephyr_ld_option_ifdef(CONFIG_SOC_NSIM_HS6X_SMP -Hlib=hs68_full_zephyr)
endif()
