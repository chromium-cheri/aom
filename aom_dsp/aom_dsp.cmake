##
## Copyright (c) 2017, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
set(AOM_DSP_COMMON_SOURCES
    "${AOM_ROOT}/aom_dsp/aom_convolve.c"
    "${AOM_ROOT}/aom_dsp/aom_convolve.h"
    "${AOM_ROOT}/aom_dsp/aom_dsp_common.h"
    "${AOM_ROOT}/aom_dsp/aom_filter.h"
    "${AOM_ROOT}/aom_dsp/aom_simd.h"
    "${AOM_ROOT}/aom_dsp/aom_simd_inline.h"
    "${AOM_ROOT}/aom_dsp/blend.h"
    "${AOM_ROOT}/aom_dsp/blend_a64_hmask.c"
    "${AOM_ROOT}/aom_dsp/blend_a64_mask.c"
    "${AOM_ROOT}/aom_dsp/blend_a64_vmask.c"
    "${AOM_ROOT}/aom_dsp/intrapred.c"
    "${AOM_ROOT}/aom_dsp/loopfilter.c"
    "${AOM_ROOT}/aom_dsp/prob.c"
    "${AOM_ROOT}/aom_dsp/prob.h"
    "${AOM_ROOT}/aom_dsp/sad.c"
    "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v128_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v256_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics.h"
    "${AOM_ROOT}/aom_dsp/simd/v64_intrinsics_c.h"
    "${AOM_ROOT}/aom_dsp/subtract.c"
    "${AOM_ROOT}/aom_dsp/txfm_common.h"
    "${AOM_ROOT}/aom_dsp/x86/txfm_common_intrin.h")

set(AOM_DSP_COMMON_ASM_SSE2
    "${AOM_ROOT}/aom_dsp/x86/aom_convolve_copy_sse2.asm"
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_sse2.asm"
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_bilinear_sse2.asm"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_sse2.asm")

set(AOM_DSP_COMMON_INTRIN_SSE2
    "${AOM_ROOT}/aom_dsp/x86/aom_asm_stubs.c"
    "${AOM_ROOT}/aom_dsp/x86/convolve.h"
    "${AOM_ROOT}/aom_dsp/x86/txfm_common_sse2.h"
    "${AOM_ROOT}/aom_dsp/x86/loopfilter_sse2.c")

set(AOM_DSP_COMMON_ASM_SSSE3
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_ssse3.asm"
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_bilinear_ssse3.asm"
    "${AOM_ROOT}/aom_dsp/x86/intrapred_ssse3.asm")

set(AOM_DSP_COMMON_INTRIN_SSSE3
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_ssse3.c")

set(AOM_DSP_COMMON_INTRIN_SSE4_1
    "${AOM_ROOT}/aom_dsp/x86/blend_a64_hmask_sse4.c"
    "${AOM_ROOT}/aom_dsp/x86/blend_a64_mask_sse4.c"
    "${AOM_ROOT}/aom_dsp/x86/blend_a64_vmask_sse4.c")

set(AOM_DSP_COMMON_AVX2_INTRIN
    "${AOM_ROOT}/aom_dsp/x86/aom_subpixel_8t_intrin_avx2.c"
    "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_avx2.c"
    "${AOM_ROOT}/aom_dsp/x86/loopfilter_avx2.c")

if (CONFIG_AOM_HIGHBITDEPTH)
  set(AOM_DSP_COMMON_ASM_SSE2
      ${AOM_DSP_COMMON_ASM_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/highbd_intrapred_sse2.asm")

  set(AOM_DSP_COMMON_INTRIN_SSE2
      ${AOM_DSP_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/highbd_loopfilter_sse2.c")
endif ()

if (CONFIG_ANS)
  set(AOM_DSP_COMMON_SOURCES
      ${AOM_DSP_COMMON_SOURCES}
      "${AOM_ROOT}/aom_dsp/ans.h")
elseif (CONFIG_DAALA_EC)
  set(AOM_DSP_COMMON_SOURCES
      ${AOM_DSP_COMMON_SOURCES}
      "${AOM_ROOT}/aom_dsp/entcode.c"
      "${AOM_ROOT}/aom_dsp/entcode.h")
endif ()

if (CONFIG_AV1)
  set(AOM_DSP_COMMON_SOURCES
      ${AOM_DSP_COMMON_SOURCES}
      "${AOM_ROOT}/aom_dsp/inv_txfm.c"
      "${AOM_ROOT}/aom_dsp/inv_txfm.h")

  set(AOM_DSP_COMMON_ASM_SSE2
      ${AOM_DSP_COMMON_ASM_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/inv_wht_sse2.asm")

  set(AOM_DSP_COMMON_INTRIN_SSE2
      ${AOM_DSP_COMMON_INTRIN_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/inv_txfm_sse2.c"
      "${AOM_ROOT}/aom_dsp/x86/inv_txfm_sse2.h")

  set(AOM_DSP_COMMON_ASM_SSSE3_X86_64
      ${AOM_DSP_COMMON_ASM_SSSE3_X86_64}
      "${AOM_ROOT}/aom_dsp/x86/inv_txfm_ssse3_x86_64.asm")
endif ()

if (CONFIG_DECODERS)
  set(AOM_DSP_DECODER_SOURCES
      "${AOM_ROOT}/aom_dsp/bitreader.h"
      "${AOM_ROOT}/aom_dsp/bitreader_buffer.c"
      "${AOM_ROOT}/aom_dsp/bitreader_buffer.h")

  if (CONFIG_ANS)
    set(AOM_DSP_DECODER_SOURCES
        ${AOM_DSP_DECODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/ansreader.h")
  elseif (CONFIG_DAALA_EC)
    set(AOM_DSP_DECODER_SOURCES
        ${AOM_DSP_DECODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/daalaboolreader.c"
        "${AOM_ROOT}/aom_dsp/daalaboolreader.h"
        "${AOM_ROOT}/aom_dsp/entdec.c"
        "${AOM_ROOT}/aom_dsp/entdec.h")
  else ()
    set(AOM_DSP_DECODER_SOURCES
        ${AOM_DSP_DECODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/dkboolreader.c"
        "${AOM_ROOT}/aom_dsp/dkboolreader.h")
  endif ()
endif ()

if (CONFIG_ENCODERS)
  set(AOM_DSP_ENCODER_SOURCES
      "${AOM_ROOT}/aom_dsp/bitwriter.h"
      "${AOM_ROOT}/aom_dsp/bitwriter_buffer.c"
      "${AOM_ROOT}/aom_dsp/bitwriter_buffer.h"
      "${AOM_ROOT}/aom_dsp/psnr.c"
      "${AOM_ROOT}/aom_dsp/psnr.h"
      "${AOM_ROOT}/aom_dsp/variance.c"
      "${AOM_ROOT}/aom_dsp/variance.h")

  set(AOM_DSP_ENCODER_ASM_SSE2
      ${AOM_DSP_ENCODER_ASM_SSE2}
      "${AOM_ROOT}/aom_dsp/x86/halfpix_variance_impl_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/sad4d_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/sad_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/subtract_sse2.asm"
      "${AOM_ROOT}/aom_dsp/x86/subpel_variance_sse2.asm")

  set(AOM_DSP_ENCODER_INTRIN_SSE2
      "${AOM_ROOT}/aom_dsp/x86/quantize_sse2.c")

  set(AOM_DSP_ENCODER_ASM_SSSE3
      "${AOM_ROOT}/aom_dsp/x86/sad_ssse3.asm")

  set(AOM_DSP_ENCODER_ASM_SSSE3_X86_64
      "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_ssse3_x86_64.asm"
      "${AOM_ROOT}/aom_dsp/x86/ssim_opt_x86_64.asm")

  set(AOM_DSP_ENCODER_INTRIN_SSE3 "${AOM_ROOT}/aom_dsp/x86/sad_sse3.asm")
  set(AOM_DSP_ENCODER_ASM_SSE4_1 "${AOM_ROOT}/aom_dsp/x86/sad_sse4.asm")

  set(AOM_DSP_ENCODER_AVX2_INTRIN
      "${AOM_ROOT}/aom_dsp/x86/sad4d_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/sad_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/sad_impl_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/variance_avx2.c"
      "${AOM_ROOT}/aom_dsp/x86/variance_impl_avx2.c")

  if (CONFIG_AV1_ENCODER)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/avg.c"
        "${AOM_ROOT}/aom_dsp/fwd_txfm.c"
        "${AOM_ROOT}/aom_dsp/fwd_txfm.h"
        "${AOM_ROOT}/aom_dsp/quantize.c"
        "${AOM_ROOT}/aom_dsp/quantize.h"
        "${AOM_ROOT}/aom_dsp/sum_squares.c")

    set(AOM_DSP_ENCODER_INTRIN_SSE2
        ${AOM_DSP_ENCODER_INTRIN_SSE2}
        "${AOM_ROOT}/aom_dsp/x86/avg_intrin_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/fwd_dct32_8cols_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/fwd_dct32x32_impl_sse2.h"
        "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_impl_sse2.h"
        "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/fwd_txfm_sse2.h"
        "${AOM_ROOT}/aom_dsp/x86/halfpix_variance_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/variance_sse2.c"
        "${AOM_ROOT}/aom_dsp/x86/sum_squares_sse2.c")

    set(AOM_DSP_ENCODER_INTRIN_SSSE3
        ${AOM_DSP_ENCODER_INTRIN_SSSE3}
        "${AOM_ROOT}/aom_dsp/x86/masked_sad_intrin_ssse3.c"
        "${AOM_ROOT}/aom_dsp/x86/masked_variance_intrin_ssse3.c")

    set(AOM_DSP_ENCODER_ASM_SSSE3_X86_64
        ${AOM_DSP_ENCODER_ASM_SSSE3_X86_64}
        "${AOM_ROOT}/aom_dsp/x86/avg_ssse3_x86_64.asm"
        "${AOM_ROOT}/aom_dsp/x86/quantize_ssse3_x86_64.asm")

    set(AOM_DSP_ENCODER_AVX_ASM_X86_64
        ${AOM_DSP_ENCODER_AVX_ASM_X86_64}
        "${AOM_ROOT}/aom_dsp/x86/quantize_avx_x86_64.asm")

    if (CONFIG_AOM_HIGHBITDEPTH)
      set(AOM_DSP_ENCODER_INTRIN_SSE2
          ${AOM_DSP_ENCODER_INTRIN_SSE2}
          "${AOM_ROOT}/aom_dsp/x86/highbd_quantize_intrin_sse2.c"
          "${AOM_ROOT}/aom_dsp/x86/highbd_subtract_sse2.c")
    endif ()
  endif ()

  if (CONFIG_AOM_HIGHBITDEPTH)
    set(AOM_DSP_ENCODER_ASM_SSE2
        ${AOM_DSP_ENCODER_ASM_SSE2}
        "${AOM_ROOT}/aom_dsp/x86/highbd_sad4d_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/highbd_sad_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/highbd_subpel_variance_impl_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/highbd_variance_impl_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/aom_high_subpixel_8t_sse2.asm"
        "${AOM_ROOT}/aom_dsp/x86/aom_high_subpixel_bilinear_sse2.asm")

    set(AOM_DSP_ENCODER_INTRIN_SSE2
        ${AOM_DSP_ENCODER_INTRIN_SSE2}
        "${AOM_ROOT}/aom_dsp/x86/highbd_variance_sse2.c")

    set(AOM_DSP_ENCODER_INTRIN_SSE4_1
        ${AOM_DSP_ENCODER_INTRIN_SSE4_1}
        "${AOM_ROOT}/aom_dsp/x86/highbd_variance_sse4.c")

    set(AOM_DSP_ENCODER_AVX2_INTRIN
        ${AOM_DSP_ENCODER_AVX2_INTRIN}
        "${AOM_ROOT}/aom_dsp/x86/sad_highbd_avx2.c")
  endif ()

  if (CONFIG_ANS)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/answriter.h"
        "${AOM_ROOT}/aom_dsp/buf_ans.c"
        "${AOM_ROOT}/aom_dsp/buf_ans.h")
  elseif (CONFIG_DAALA_EC)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/daalaboolwriter.c"
        "${AOM_ROOT}/aom_dsp/daalaboolwriter.h"
        "${AOM_ROOT}/aom_dsp/entenc.c"
        "${AOM_ROOT}/aom_dsp/entenc.h")
  else ()
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/dkboolwriter.c"
        "${AOM_ROOT}/aom_dsp/dkboolwriter.h")
  endif ()

  if (CONFIG_INTERNAL_STATS)
    set(AOM_DSP_ENCODER_SOURCES
        ${AOM_DSP_ENCODER_SOURCES}
        "${AOM_ROOT}/aom_dsp/fastssim.c"
        "${AOM_ROOT}/aom_dsp/psnrhvs.c"
        "${AOM_ROOT}/aom_dsp/ssim.c"
        "${AOM_ROOT}/aom_dsp/ssim.h")
  endif ()
endif ()

if (CONFIG_MOTION_VAR)
  set(AOM_DSP_ENCODER_INTRIN_SSE4_1
      ${AOM_DSP_ENCODER_INTRIN_SSE4_1}
      "${AOM_ROOT}/aom_dsp/x86/obmc_sad_sse4.c"
      "${AOM_ROOT}/aom_dsp/x86/obmc_variance_sse4.c")

  set(AOM_DSP_ENCODER_UNIT_TEST_INTRIN_SSE4_1
      ${AOM_DSP_ENCODER_UNIT_TEST_INTRIN_SSE4_1}
      "${AOM_ROOT}/test/obmc_sad_test.cc"
      "${AOM_ROOT}/test/obmc_variance_test.cc")
endif ()

set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_dsp_common)

if (CONFIG_DECODERS)
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_dsp_decoder)
endif ()
if (CONFIG_ENCODERS)
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} aom_dsp_encoder)
endif ()

# Creates aom_dsp build targets. Must not be called until after libaom target
# has been created.
function (setup_aom_dsp_targets)
  add_library(aom_dsp_common OBJECT ${AOM_DSP_COMMON_SOURCES})
  target_sources(aom PUBLIC $<TARGET_OBJECTS:aom_dsp_common>)

  if (CONFIG_DECODERS)
    add_library(aom_dsp_decoder OBJECT ${AOM_DSP_DECODER_SOURCES})
    target_sources(aom PUBLIC $<TARGET_OBJECTS:aom_dsp_decoder>)
  endif ()

  if (CONFIG_ENCODERS)
    add_library(aom_dsp_encoder OBJECT ${AOM_DSP_ENCODER_SOURCES})
    target_sources(aom PUBLIC $<TARGET_OBJECTS:aom_dsp_encoder>)
  endif ()

  if (HAVE_SSE2)
    add_asm_library("aom_dsp_common_sse2" "AOM_DSP_COMMON_ASM_SSE2" "aom")
    add_intrinsics_object_library("-msse2" "sse2" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSE2")
    if (CONFIG_ENCODERS)
      add_asm_library("aom_dsp_encoder_sse2" "AOM_DSP_ENCODER_ASM_SSE2" "aom")
      add_intrinsics_object_library("-msse2" "sse2" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_SSE2")
    endif()
  endif ()

  if (HAVE_SSE3 AND CONFIG_ENCODERS)
    add_asm_library("aom_dsp_encoder_sse3" "AOM_DSP_ENCODER_INTRIN_SSE3" "aom")
  endif ()

  if (HAVE_SSSE3)
    if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
      list(APPEND AOM_DSP_COMMON_ASM_SSSE3
           ${AOM_DSP_COMMON_ASM_SSSE3_X86_64})
    endif ()

    add_asm_library("aom_dsp_common_ssse3" "AOM_DSP_COMMON_ASM_SSSE3" "aom")
    add_intrinsics_object_library("-mssse3" "ssse3" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSSE3")

     if (CONFIG_ENCODERS)
      if ("${AOM_TARGET_CPU}" STREQUAL "x86_64")
        list(APPEND AOM_DSP_ENCODER_ASM_SSSE3
             ${AOM_DSP_ENCODER_ASM_SSSE3_X86_64})
      endif ()
      add_asm_library("aom_dsp_encoder_ssse3" "AOM_DSP_ENCODER_ASM_SSSE3" "aom")
      add_intrinsics_object_library("-mssse3" "ssse3" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_INTRIN_SSSE3")
    endif ()
  endif ()

  if (HAVE_SSE4_1)
    add_intrinsics_object_library("-msse4.1" "sse4_1" "aom_dsp_common"
                                  "AOM_DSP_COMMON_INTRIN_SSE4_1")
    if (CONFIG_ENCODERS)
      if (AOM_DSP_ENCODER_INTRIN_SSE4_1)
        add_intrinsics_object_library("-msse4.1" "sse4_1" "aom_dsp_encoder"
                                      "AOM_DSP_ENCODER_INTRIN_SSE4_1")
      endif ()
      add_asm_library("aom_dsp_encoder_sse4_1" "AOM_DSP_ENCODER_ASM_SSE4_1"
                      "aom")
    endif ()
  endif ()

  if (HAVE_AVX AND "${AOM_TARGET_CPU}" STREQUAL "x86_64")
    add_asm_library("aom_dsp_encoder_avx" "AOM_DSP_ENCODER_AVX_ASM_X86_64"
                    "aom")
  endif ()

  if (HAVE_AVX2)
    add_intrinsics_object_library("-mavx2" "avx2" "aom_dsp_common"
                                  "AOM_DSP_COMMON_AVX2_INTRIN")
    if (CONFIG_ENCODERS)
      add_intrinsics_object_library("-mavx2" "avx2" "aom_dsp_encoder"
                                    "AOM_DSP_ENCODER_AVX2_INTRIN")
    endif ()
  endif ()

  # Pass the new lib targets up to the parent scope instance of
  # $AOM_LIB_TARGETS.
  set(AOM_LIB_TARGETS ${AOM_LIB_TARGETS} PARENT_SCOPE)
endfunction ()

# Sets up aom_dsp test targets. The test_libaom target must be created before
# this function is called.
function (setup_aom_dsp_test_targets)
  if (HAVE_SSE_4_1)
    if (CONFIG_ENCODERS)
      if (CONFIG_UNIT_TESTS AND AOM_DSP_ENCODER_UNIT_TEST_INTRIN_SSE4_1)
        add_intrinsics_source_to_target("-msse4.1" "test_libaom"
          "AOM_DSP_ENCODER_UNIT_TEST_INTRIN_SSE4_1")
      endif ()
    endif ()
  endif ()
endfunction ()
