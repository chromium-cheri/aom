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
cmake_minimum_required(VERSION 3.5)

set(REQUIRED_ARGS
    "AOM_ROOT" "AOM_CONFIG_DIR" "GIT_EXECUTABLE" "PERL_EXECUTABLE")

foreach (arg ${REQUIRED_ARGS})
  if ("${${arg}}" STREQUAL "")
    message(FATAL_ERROR "${arg} must not be empty.")
  endif ()
endforeach ()

include("${AOM_ROOT}/build/cmake/util.cmake")

# Generate the version string for this run.
unset(aom_version)
execute_process(COMMAND ${GIT_EXECUTABLE} --git-dir=${AOM_ROOT}/.git describe
                OUTPUT_VARIABLE aom_version ERROR_QUIET)

if ("${aom_version}" STREQUAL "")
  set(aom_version "${AOM_ROOT}/CHANGELOG")
else ()
  string(STRIP "${aom_version}" aom_version)
endif ()

unset(last_aom_version)
if (EXISTS "${AOM_CONFIG_DIR}/aom_version.h")
  extract_version_string("${AOM_CONFIG_DIR}/aom_version.h" last_aom_version)
endif ()

if (NOT "${aom_version}" STREQUAL "${last_aom_version}")
  execute_process(
    COMMAND ${PERL_EXECUTABLE} "${AOM_ROOT}/build/cmake/aom_version.pl"
    --version_data=${aom_version}
    --version_filename=${AOM_CONFIG_DIR}/aom_version.h
    VERBATIM)
endif ()
