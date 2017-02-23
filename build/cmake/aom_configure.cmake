##
## Copyright (c) 2016, Alliance for Open Media. All rights reserved
##
## This source code is subject to the terms of the BSD 2 Clause License and
## the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
## was not distributed with this source code in the LICENSE file, you can
## obtain it at www.aomedia.org/license/software. If the Alliance for Open
## Media Patent License 1.0 was not distributed with this source code in the
## PATENTS file, you can obtain it at www.aomedia.org/license/patent.
##
cmake_minimum_required(VERSION 3.2)

include(FindGit)
include(FindPerl)

include("${AOM_ROOT}/build/cmake/aom_config_defaults.cmake")
include("${AOM_ROOT}/build/cmake/compiler_flags.cmake")
include("${AOM_ROOT}/build/cmake/compiler_tests.cmake")

# Detect target CPU.
if (NOT AOM_TARGET_CPU)
  if ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "AMD64" OR
      "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    if (${CMAKE_SIZEOF_VOID_P} EQUAL 4)
      set(AOM_TARGET_CPU "x86")
    elseif (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
      set(AOM_TARGET_CPU "x86_64")
    else ()
      message(FATAL_ERROR
              "--- Unexpected pointer size (${CMAKE_SIZEOF_VOID_P}) for\n"
              "      CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME}\n"
              "      CMAKE_SYSTEM_PROCESSOR=${CMAKE_SYSTEM_PROCESSOR}\n"
              "      CMAKE_GENERATOR=${CMAKE_GENERATOR}\n")
    endif ()
  elseif ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "i386" OR
          "${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86")
    set(AOM_TARGET_CPU "x86")
  endif ()
endif ()

# Store the args passed to cmake, the generator used, and the target CPU.
get_cmake_property(cmake_cache_vars CACHE_VARIABLES)
foreach(cache_var ${cmake_cache_vars})
  get_property(cache_var_helpstring CACHE ${cache_var} PROPERTY HELPSTRING)
  if(cache_var_helpstring STREQUAL
     "No help, variable specified on the command line.")
   set(AOM_CMAKE_CONFIG "${AOM_CMAKE_CONFIG} -D${cache_var}=${${cache_var}}")
  endif()
endforeach()

string(STRIP "${AOM_CMAKE_CONFIG}" AOM_CMAKE_CONFIG)
set(AOM_CMAKE_CONFIG "CMAKE_GENERATOR=${CMAKE_GENERATOR} ${AOM_CMAKE_CONFIG}")
set(AOM_CMAKE_CONFIG "AOM_TARGET_CPU=${AOM_TARGET_CPU} ${AOM_CMAKE_CONFIG}")

message("--- aom_configure: Detected CPU: ${AOM_TARGET_CPU}")
set(AOM_TARGET_SYSTEM ${CMAKE_SYSTEM_NAME})

if (NOT EXISTS "${AOM_ROOT}/build/cmake/targets/${AOM_TARGET_CPU}.cmake")
  message(FATAL_ERROR "No RTCD template for ${AOM_TARGET_CPU}. Create one, or "
          "add -DAOM_TARGET_CPU=generic to your cmake command line for a "
          "generic build of libaom and tools.")
endif ()

if ("${AOM_TARGET_CPU}" STREQUAL "x86" OR "${AOM_TARGET_CPU}" STREQUAL "x86_64")
  find_program(YASM_EXECUTABLE yasm $ENV{YASM_PATH})
  if (NOT YASM_EXECUTABLE)
    message(WARNING "Unable to find yasm, using generic as target CPU.")
    set(AOM_TARGET_CPU "generic")
  endif ()
endif ()

include("${AOM_ROOT}/build/cmake/targets/${AOM_TARGET_CPU}.cmake")

# Test compiler flags.
if (MSVC)
  add_compiler_flag_if_supported("/W3")
  # Disable MSVC warnings that suggest making code non-portable.
  add_compiler_flag_if_supported("/wd4996")
  if (ENABLE_WERROR)
    add_compiler_flag_if_supported("/WX")
  endif ()
else ()
  require_c_flag("-std=c99" YES)
  add_compiler_flag_if_supported("-Wall")
  add_compiler_flag_if_supported("-Wdisabled-optimization")
  add_compiler_flag_if_supported("-Wextra")
  add_compiler_flag_if_supported("-Wfloat-conversion")
  add_compiler_flag_if_supported("-Wimplicit-function-declaration")
  add_compiler_flag_if_supported("-Wpointer-arith")
  add_compiler_flag_if_supported("-Wsign-compare")
  add_compiler_flag_if_supported("-Wtype-limits")
  add_compiler_flag_if_supported("-Wuninitialized")
  add_compiler_flag_if_supported("-Wunused")
  add_compiler_flag_if_supported("-Wvla")

  # Add -Wshadow only for C files to avoid massive gtest warning spam.
  add_c_flag_if_supported("-Wshadow")

  if (ENABLE_WERROR)
    add_compiler_flag_if_supported("-Werror")
  endif ()
  # Flag(s) added here negate CMake defaults and produce build output similar
  # to the existing configure/make build system.
  add_compiler_flag_if_supported("-Wno-unused-function")
endif ()

# Test compiler support.
aom_get_inline("INLINE")

# TODO(tomfinegan): aom_ports_check is legacy; HAVE_AOM_PORTS is not used
# anywhere in the aom sources. To be removed after parity with the legacy
# build system stops being important.
aom_check_source_compiles("aom_ports_check"
                          "#include \"${AOM_ROOT}/aom/aom_integer.h\""
                          HAVE_AOM_PORTS)
aom_check_source_compiles("pthread_check" "#include <pthread.h>" HAVE_PTHREAD_H)
aom_check_source_compiles("unistd_check" "#include <unistd.h>" HAVE_UNISTD_H)

# TODO(tomfinegan): consume trailing whitespace after configure_file() when
# target platform check produces empty INLINE and RESTRICT values (aka empty
# values require special casing).
configure_file("${AOM_ROOT}/build/cmake/aom_config.h.cmake"
               "${AOM_CONFIG_DIR}/aom_config.h")
configure_file("${AOM_ROOT}/build/cmake/aom_config.asm.cmake"
               "${AOM_CONFIG_DIR}/aom_config.asm")

# Read the current git hash.
find_package(Git)
set(AOM_GIT_DESCRIPTION)
set(AOM_GIT_HASH)
if (GIT_FOUND)
  # TODO(tomfinegan): Add build rule so users don't have to re-run cmake to
  # create accurately versioned cmake builds.
  execute_process(COMMAND ${GIT_EXECUTABLE}
                  --git-dir=${AOM_ROOT}/.git rev-parse HEAD
                  OUTPUT_VARIABLE AOM_GIT_HASH)
  execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir=${AOM_ROOT}/.git describe
                  OUTPUT_VARIABLE AOM_GIT_DESCRIPTION ERROR_QUIET)
  # Consume the newline at the end of the git output.
  string(STRIP "${AOM_GIT_HASH}" AOM_GIT_HASH)
  string(STRIP "${AOM_GIT_DESCRIPTION}" AOM_GIT_DESCRIPTION)
endif ()

configure_file("${AOM_ROOT}/build/cmake/aom_config.c.cmake"
               "${AOM_CONFIG_DIR}/aom_config.c")

# Find Perl and generate the RTCD sources.
find_package(Perl)
if (NOT PERL_FOUND)
  message(FATAL_ERROR "Perl is required to build libaom.")
endif ()
configure_file(
  "${AOM_ROOT}/build/cmake/targets/rtcd_templates/${AOM_ARCH}.rtcd.cmake"
  "${AOM_CONFIG_DIR}/${AOM_ARCH}.rtcd")

set(AOM_RTCD_CONFIG_FILE_LIST
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd_defs.pl"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.pl"
    "${AOM_ROOT}/av1/common/av1_rtcd_defs.pl")
set(AOM_RTCD_HEADER_FILE_LIST
    "${AOM_CONFIG_DIR}/aom_dsp_rtcd.h"
    "${AOM_CONFIG_DIR}/aom_scale_rtcd.h"
    "${AOM_CONFIG_DIR}/av1_rtcd.h")
set(AOM_RTCD_SOURCE_FILE_LIST
    "${AOM_ROOT}/aom_dsp/aom_dsp_rtcd.c"
    "${AOM_ROOT}/aom_scale/aom_scale_rtcd.c"
    "${AOM_ROOT}/av1/common/av1_rtcd.c")
set(AOM_RTCD_SYMBOL_LIST aom_dsp_rtcd aom_scale_rtcd av1_rtcd)
list(LENGTH AOM_RTCD_SYMBOL_LIST AOM_RTCD_CUSTOM_COMMAND_COUNT)
math(EXPR AOM_RTCD_CUSTOM_COMMAND_COUNT "${AOM_RTCD_CUSTOM_COMMAND_COUNT} - 1")

foreach(NUM RANGE ${AOM_RTCD_CUSTOM_COMMAND_COUNT})
  list(GET AOM_RTCD_CONFIG_FILE_LIST ${NUM} AOM_RTCD_CONFIG_FILE)
  list(GET AOM_RTCD_HEADER_FILE_LIST ${NUM} AOM_RTCD_HEADER_FILE)
  list(GET AOM_RTCD_SOURCE_FILE_LIST ${NUM} AOM_RTCD_SOURCE_FILE)
  list(GET AOM_RTCD_SYMBOL_LIST ${NUM} AOM_RTCD_SYMBOL)
  execute_process(
    COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/make/rtcd.pl"
      --arch=${AOM_ARCH} --sym=${AOM_RTCD_SYMBOL}
      --config=${AOM_CONFIG_DIR}/${AOM_ARCH}.rtcd ${AOM_RTCD_CONFIG_FILE}
    OUTPUT_FILE ${AOM_RTCD_HEADER_FILE})
endforeach()

function (add_rtcd_build_step config output source symbol)
  add_custom_command(
    OUTPUT ${output}
    COMMAND ${PERL_EXECUTABLE}
    ARGS "${AOM_ROOT}/build/make/rtcd.pl" --arch=${AOM_ARCH} --sym=${symbol}
      --config=${AOM_CONFIG_DIR}/${AOM_ARCH}.rtcd ${config} > ${output}
    DEPENDS ${config}
    COMMENT "Generating ${output}"
    WORKING_DIRECTORY ${AOM_CONFIG_DIR}
    VERBATIM)
  set_property(SOURCE ${source} PROPERTY OBJECT_DEPENDS ${output})
  set_property(SOURCE ${output} PROPERTY GENERATED)
endfunction ()

# Generate aom_version.h.
if ("${AOM_GIT_DESCRIPTION}" STREQUAL "")
  set(AOM_GIT_DESCRIPTION "${AOM_ROOT}/CHANGELOG")
endif ()
execute_process(
  COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/cmake/aom_version.pl"
  --version_data=${AOM_GIT_DESCRIPTION}
  --version_filename=${AOM_CONFIG_DIR}/aom_version.h)
