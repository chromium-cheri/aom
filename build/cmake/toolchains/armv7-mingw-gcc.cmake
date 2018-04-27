#
# Copyright (c) 2018, Alliance for Open Media. All rights reserved
#
# This source code is subject to the terms of the BSD 2 Clause License and the
# Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License was
# not distributed with this source code in the LICENSE file, you can obtain it
# at www.aomedia.org/license/software. If the Alliance for Open Media Patent
# License 1.0 was not distributed with this source code in the PATENTS file, you
# can obtain it at www.aomedia.org/license/patent.
#
if(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_MINGW_GCC_CMAKE_)
  return()
endif() # AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_MINGW_GCC_CMAKE_
set(AOM_BUILD_CMAKE_TOOLCHAINS_ARMV7_MINGW_GCC_CMAKE_ 1)

set(CMAKE_SYSTEM_PROCESSOR "armv7")
set(CMAKE_SYSTEM_NAME "Windows")

if("${CROSS}" STREQUAL "")
  set(CROSS armv7-w64-mingw32-)
endif()

set(CMAKE_C_COMPILER ${CROSS}gcc)
set(CMAKE_CXX_COMPILER ${CROSS}g++)

# Assembler sources must be converted for armv7-mingw-gcc targets.
set(AOM_ADS2GAS_REQUIRED 1)
set(AOM_ADS2GAS "${CMAKE_CURRENT_SOURCE_DIR}/build/make/ads2gas.pl")
set(AOM_ADS2GAS_OPTS "-thumb;-noelf")
set(AOM_GAS_EXT "S")

# No runtime cpu detect for armv7-mingw-gcc.
set(CONFIG_RUNTIME_CPU_DETECT 0 CACHE NUMBER "")

# Disable the use of the gtest's CMake support.
set(AOM_DISABLE_GTEST_CMAKE 1)
