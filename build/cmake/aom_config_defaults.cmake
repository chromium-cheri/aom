#
# Copyright (c) 2016, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.

include("${AOM_ROOT}/build/cmake/util.cmake")

# This file sets default values for libaom configuration variables. All libaom
# config variables are added to the CMake variable cache via the macros provided
# in util.cmake.

#
# The variables in this section of the file are detected at configuration time,
# but can be overridden via the use of CONFIG_* and ENABLE_* values also defined
# in this file.
#

set_aom_detect_var(INLINE "" "Sets INLINE value for current target.")

# CPUs.
set_aom_detect_var(ARCH_ARM 0 "Enables ARM architecture.")
set_aom_detect_var(ARCH_MIPS 0 "Enables MIPS architecture.")
set_aom_detect_var(ARCH_PPC 0 "Enables PPC architecture.")
set_aom_detect_var(ARCH_X86 0 "Enables X86 architecture.")
set_aom_detect_var(ARCH_X86_64 0 "Enables X86_64 architecture.")

# ARM feature flags.
set_aom_detect_var(HAVE_NEON 0 "Enables NEON intrinsics optimizations.")

# MIPS feature flags.
set_aom_detect_var(HAVE_DSPR2 0 "Enables DSPR2 optimizations.")
set_aom_detect_var(HAVE_MIPS32 0 "Enables MIPS32 optimizations.")
set_aom_detect_var(HAVE_MIPS64 0 "Enables MIPS64 optimizations. ")
set_aom_detect_var(HAVE_MSA 0 "Enables MSA optimizations.")

# PPC feature flags.
set_aom_detect_var(HAVE_VSX 0 "Enables VSX optimizations.")

# x86/x86_64 feature flags.
set_aom_detect_var(HAVE_AVX 0 "Enables AVX optimizations.")
set_aom_detect_var(HAVE_AVX2 0 "Enables AVX2 optimizations.")
set_aom_detect_var(HAVE_MMX 0 "Enables MMX optimizations. ")
set_aom_detect_var(HAVE_SSE 0 "Enables SSE optimizations.")
set_aom_detect_var(HAVE_SSE2 0 "Enables SSE2 optimizations.")
set_aom_detect_var(HAVE_SSE3 0 "Enables SSE3 optimizations.")
set_aom_detect_var(HAVE_SSE4_1 0 "Enables SSE 4.1 optimizations.")
set_aom_detect_var(HAVE_SSE4_2 0 "Enables SSE 4.2 optimizations.")
set_aom_detect_var(HAVE_SSSE3 0 "Enables SSSE3 optimizations.")

# Flags describing the build environment.
set_aom_detect_var(HAVE_FEXCEPT 0
                   "Internal flag, GNU fenv.h present for target.")
set_aom_detect_var(HAVE_PTHREAD_H 0 "Internal flag, target pthread support.")
set_aom_detect_var(HAVE_UNISTD_H 0
                   "Internal flag, unistd.h present for target.")
set_aom_detect_var(HAVE_WXWIDGETS 0 "WxWidgets present.")

#
# Variables in this section can be set from the CMake command line or from
# within the CMake GUI. The variables control libaom features.
#

# Build configuration flags.
set_aom_config_var(AOM_RTCD_FLAGS ""
                   "Arguments to pass to rtcd.pl. Separate with ';'")
set_aom_config_var(CONFIG_AV1_DECODER 1 "Enable AV1 decoder.")
set_aom_config_var(CONFIG_AV1_ENCODER 1 "Enable AV1 encoder.")
set_aom_config_var(CONFIG_BIG_ENDIAN 0 "Internal flag.")
set_aom_config_var(CONFIG_GCC 0 "Building with GCC (detect).")
set_aom_config_var(CONFIG_GCOV 0 "Enable gcov support.")
set_aom_config_var(CONFIG_GPROF 0 "Enable gprof support.")
set_aom_config_var(CONFIG_LIBYUV 1 "Enables libyuv scaling/conversion support.")

set_aom_config_var(CONFIG_MULTITHREAD 1 "Multithread support.")
set_aom_config_var(CONFIG_OS_SUPPORT 0 "Internal flag.")
set_aom_config_var(CONFIG_PIC 0 "Build with PIC enabled.")
set_aom_config_var(CONFIG_RUNTIME_CPU_DETECT 1 "Runtime CPU detection support.")
set_aom_config_var(CONFIG_SHARED 0 "Build shared libs.")
set_aom_config_var(CONFIG_WEBM_IO 1 "Enables WebM support.")
set_aom_config_var(CONFIG_MVP_LESS_LINE_BUF 0 "Enable less line buffer for MVP")
set_aom_config_var(CONFIG_SMVP_IMPROVEMENT 0 "Enable SMVP improvement")
set_aom_config_var(CONFIG_TMVP_IMPROVEMENT 0 "Enable TMVP improvement")

# Debugging flags.
set_aom_config_var(CONFIG_DEBUG 0 "Debug build flag.")
set_aom_config_var(CONFIG_MISMATCH_DEBUG 0 "Mismatch debugging flag.")

# AV1 feature flags.
set_aom_config_var(CONFIG_ACCOUNTING 0 "Enables bit accounting.")
set_aom_config_var(CONFIG_ANALYZER 0 "Enables bit stream analyzer.")
set_aom_config_var(CONFIG_COEFFICIENT_RANGE_CHECKING 0
                   "Coefficient range check.")
set_aom_config_var(CONFIG_DENOISE 1
                   "Denoise/noise modeling support in encoder.")
set_aom_config_var(CONFIG_INSPECTION 0 "Enables bitstream inspection.")
set_aom_config_var(CONFIG_INTERNAL_STATS 0 "Enables internal encoder stats.")
set_aom_config_var(CONFIG_MAX_DECODE_PROFILE 2
                   "Max profile to support decoding.")
set_aom_config_var(CONFIG_NORMAL_TILE_MODE 0 "Only enables normal tile mode.")
set_aom_config_var(CONFIG_SIZE_LIMIT 0 "Limit max decode width/height.")
set_aom_config_var(CONFIG_SPATIAL_RESAMPLING 1 "Spatial resampling.")
set_aom_config_var(DECODE_HEIGHT_LIMIT 0 "Set limit for decode height.")
set_aom_config_var(DECODE_WIDTH_LIMIT 0 "Set limit for decode width.")
set_aom_config_var(CONFIG_TUNE_VMAF 0 "Enable encoding tuning for VMAF.")
set_aom_config_var(CONFIG_USE_VMAF_RC 0 "Use libvmaf_rc tune for VMAF_NEG.")

# AV1 experiment flags.
set_aom_config_var(CONFIG_SPEED_STATS 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_COLLECT_RD_STATS 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_DIST_8X8 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_ENTROPY_STATS 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_INTER_STATS_ONLY 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_BITSTREAM_DEBUG 0
                   "AV1 experiment flag for bitstream debugging.")
set_aom_config_var(CONFIG_RD_DEBUG 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_SHARP_SETTINGS 0 "AV1 experiment flag.")
set_aom_config_var(CONFIG_DISABLE_FULL_PIXEL_SPLIT_8X8 1
                   "Disable full_pixel_motion_search_based_split on BLOCK_8X8.")
set_aom_config_var(CONFIG_COLLECT_PARTITION_STATS 0
                   "Collect stats on partition decisions.")
set_aom_config_var(CONFIG_COLLECT_COMPONENT_TIMING 0
                   "Collect encoding component timing information.")
set_aom_config_var(CONFIG_LPF_MASK 0
                   "Enable the use loop filter bitmasks for optimizations.")
set_aom_config_var(CONFIG_REALTIME_ONLY 0
                   "Build for RTC-only to reduce binary size.")
set_aom_config_var(CONFIG_AV1_TEMPORAL_DENOISING 0
                   "Build with temporal denoising support.")
set_aom_config_var(CONFIG_NN_V2 0 "Fully-connected neural nets ver.2.")
set_aom_config_var(CONFIG_OPTICAL_FLOW_API 0
                   "AV1 experiment flag for optical flow API.")

# AV2 experiment flags.
set_aom_config_var(CONFIG_DERIVED_INTRA_MODE 0 NUMBER
                   "AV2 derived intra mode experiment flag")
set_aom_config_var(CONFIG_REF_MV_BANK 0 NUMBER
                   "AV2 ref mv bank experiment flag")
set_aom_config_var(CONFIG_REMOVE_DIST_WTD_COMP 1
                   "AV2 experiment flag to remove dist_wtd_comp tool.")
set_aom_config_var(CONFIG_REMOVE_DUAL_FILTER 1
                   "AV2 experiment flag to remove dual filter.")
set_aom_config_var(CONFIG_NEW_REF_SIGNALING 0
                   "AV2 experiment flag for the new reference syntax.")
set_aom_config_var(CONFIG_EXTQUANT 0
                   "AV2 extended quantization experiment flag")
set_aom_config_var(CONFIG_GM_MODEL_CODING 0
                   "AV2 global motion model compression flag.")
set_aom_config_var(CONFIG_NEW_TX_PARTITION 0
                   "AV2 new transform partitions experiment flag.")
set_aom_config_var(CONFIG_COMPOUND_WARP_SAMPLES 0 NUMBER
                   "AV2 compound warped motion samples experiment flag")
set_aom_config_var(CONFIG_NEW_INTER_MODES 0 NUMBER
                   "AV2 inter mode consolidation experiment flag")
set_aom_config_var(CONFIG_EXT_ROTATION 0
                   "AV2 warping motion rotation experiment flag.")
set_aom_config_var(CONFIG_FLEX_MVRES 0 NUMBER
                   "AV2 flexible mv precision experiment flag")
set_aom_config_var(CONFIG_EXT_RECUR_PARTITIONS 0 NUMBER
                   "AV2 Fully recursive partitions experiment flag")
set_aom_config_var(CONFIG_OPTFLOW_REFINEMENT 0
                   "AV2 optical flow based MV refinement experimental flag")

#
# Variables in this section control optional features of the build system.
#
set_aom_option_var(ENABLE_CCACHE "Enable ccache support." OFF)
set_aom_option_var(ENABLE_DECODE_PERF_TESTS "Enables decoder performance tests"
                   OFF)
set_aom_option_var(ENABLE_DISTCC "Enable distcc support." OFF)
set_aom_option_var(ENABLE_DOCS
                   "Enable documentation generation (doxygen required)." ON)
set_aom_option_var(ENABLE_ENCODE_PERF_TESTS "Enables encoder performance tests"
                   OFF)
set_aom_option_var(ENABLE_EXAMPLES "Enables build of example code." ON)
set_aom_option_var(ENABLE_GOMA "Enable goma support." OFF)
set_aom_option_var(
  ENABLE_IDE_TEST_HOSTING
  "Enables running tests within IDEs like Visual Studio and Xcode." OFF)
set_aom_option_var(ENABLE_NASM "Use nasm instead of yasm for x86 assembly." OFF)
set_aom_option_var(ENABLE_TESTDATA "Enables unit test data download targets."
                   ON)
set_aom_option_var(ENABLE_TESTS "Enables unit tests." ON)
set_aom_option_var(ENABLE_TOOLS "Enable applications in tools sub directory."
                   ON)
set_aom_option_var(ENABLE_WERROR "Converts warnings to errors at compile time."
                   OFF)

# ARM assembly/intrinsics flags.
set_aom_option_var(ENABLE_NEON "Enables NEON optimizations on ARM targets." ON)

# MIPS assembly/intrinsics flags.
set_aom_option_var(ENABLE_DSPR2 "Enables DSPR2 optimizations on MIPS targets."
                   OFF)
set_aom_option_var(ENABLE_MSA "Enables MSA optimizations on MIPS targets." OFF)

# VSX intrinsics flags.
set_aom_option_var(ENABLE_VSX "Enables VSX optimizations on PowerPC targets."
                   ON)

# x86/x86_64 assembly/intrinsics flags.
set_aom_option_var(ENABLE_MMX "Enables MMX optimizations on x86/x86_64 targets."
                   ON)
set_aom_option_var(ENABLE_SSE "Enables SSE optimizations on x86/x86_64 targets."
                   ON)
set_aom_option_var(ENABLE_SSE2
                   "Enables SSE2 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE3
                   "Enables SSE3 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSSE3
                   "Enables SSSE3 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE4_1
                   "Enables SSE4_1 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_SSE4_2
                   "Enables SSE4_2 optimizations on x86/x86_64 targets." ON)
set_aom_option_var(ENABLE_AVX "Enables AVX optimizations on x86/x86_64 targets."
                   ON)
set_aom_option_var(ENABLE_AVX2
                   "Enables AVX2 optimizations on x86/x86_64 targets." ON)
