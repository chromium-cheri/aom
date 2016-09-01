/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_COMMON_DATA_H_
#define AV1_COMMON_COMMON_DATA_H_

#include "av1/common/enums.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log 2 conversion lookup tables for block width and height
static const uint8_t b_width_log2_lookup[BLOCK_SIZES] = { 0, 0, 1, 1, 1, 2, 2,
                                                          2, 3, 3, 3, 4, 4 };
static const uint8_t b_height_log2_lookup[BLOCK_SIZES] = { 0, 1, 0, 1, 2, 1, 2,
                                                           3, 2, 3, 4, 3, 4 };
static const uint8_t num_4x4_blocks_wide_lookup[BLOCK_SIZES] = {
  1, 1, 2, 2, 2, 4, 4, 4, 8, 8, 8, 16, 16
};
static const uint8_t num_4x4_blocks_high_lookup[BLOCK_SIZES] = {
  1, 2, 1, 2, 4, 2, 4, 8, 4, 8, 16, 8, 16
};
// Log 2 conversion lookup tables for modeinfo width and height
static const uint8_t mi_width_log2_lookup[BLOCK_SIZES] = { 0, 0, 0, 0, 0, 1, 1,
                                                           1, 2, 2, 2, 3, 3 };
static const uint8_t mi_height_log2_lookup[BLOCK_SIZES] = { 0, 0, 0, 0, 1, 0, 1,
                                                            2, 1, 2, 3, 2, 3 };
static const uint8_t num_8x8_blocks_wide_lookup[BLOCK_SIZES] = {
  1, 1, 1, 1, 1, 2, 2, 2, 4, 4, 4, 8, 8
};
static const uint8_t num_8x8_blocks_high_lookup[BLOCK_SIZES] = {
  1, 1, 1, 1, 2, 1, 2, 4, 2, 4, 8, 4, 8
};

// AOMMIN(3, AOMMIN(b_width_log2(bsize), b_height_log2(bsize)))
static const uint8_t size_group_lookup[BLOCK_SIZES] = { 0, 0, 0, 1, 1, 1, 2,
                                                        2, 2, 3, 3, 3, 3 };

static const uint8_t num_pels_log2_lookup[BLOCK_SIZES] = {
  4, 5, 5, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12
};

static const PARTITION_TYPE partition_lookup[][BLOCK_SIZES] = {
  { // 4X4
    // 4X4, 4X8,8X4,8X8,8X16,16X8,16X16,16X32,32X16,32X32,32X64,64X32,64X64
    PARTITION_NONE, PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID },
  { // 8X8
    // 4X4, 4X8,8X4,8X8,8X16,16X8,16X16,16X32,32X16,32X32,32X64,64X32,64X64
    PARTITION_SPLIT, PARTITION_VERT, PARTITION_HORZ, PARTITION_NONE,
    PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID },
  { // 16X16
    // 4X4, 4X8,8X4,8X8,8X16,16X8,16X16,16X32,32X16,32X32,32X64,64X32,64X64
    PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT,
    PARTITION_VERT, PARTITION_HORZ, PARTITION_NONE, PARTITION_INVALID,
    PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID },
  { // 32X32
    // 4X4, 4X8,8X4,8X8,8X16,16X8,16X16,16X32,32X16,32X32,32X64,64X32,64X64
    PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT,
    PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_VERT,
    PARTITION_HORZ, PARTITION_NONE, PARTITION_INVALID, PARTITION_INVALID,
    PARTITION_INVALID },
  { // 64X64
    // 4X4, 4X8,8X4,8X8,8X16,16X8,16X16,16X32,32X16,32X32,32X64,64X32,64X64
    PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT,
    PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_SPLIT,
    PARTITION_SPLIT, PARTITION_SPLIT, PARTITION_VERT, PARTITION_HORZ,
    PARTITION_NONE }
};

static const BLOCK_SIZE subsize_lookup[PARTITION_TYPES][BLOCK_SIZES] = {
  {
      // PARTITION_NONE
      BLOCK_4X4, BLOCK_4X8, BLOCK_8X4, BLOCK_8X8, BLOCK_8X16, BLOCK_16X8,
      BLOCK_16X16, BLOCK_16X32, BLOCK_32X16, BLOCK_32X32, BLOCK_32X64,
      BLOCK_64X32, BLOCK_64X64,
  },
  {
      // PARTITION_HORZ
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_16X8, BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32,
  },
  {
      // PARTITION_VERT
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_8X16, BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64,
  },
  {
      // PARTITION_SPLIT
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4, BLOCK_INVALID,
      BLOCK_INVALID, BLOCK_8X8, BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
      BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32,
  }
};

// transform block size in pixels
static const int tx_size_1d[TX_SIZES] = { 4, 8, 16, 32 };

static const uint8_t tx_size_1d_log2[TX_SIZES] = { 2, 3, 4, 5 };

static const int tx_size_1d_in_unit[TX_SIZES] = { 1, 2, 4, 8 };

// TODO(jingning): Temporary table during the construction.
static const int tx_size_1d_in_unit_log2[TX_SIZES] = { 0, 1, 2, 3 };

static const TX_SIZE max_txsize_lookup[BLOCK_SIZES] = {
  TX_4X4,   TX_4X4,   TX_4X4,   TX_8X8,   TX_8X8,   TX_8X8,  TX_16X16,
  TX_16X16, TX_16X16, TX_32X32, TX_32X32, TX_32X32, TX_32X32
};

static const BLOCK_SIZE txsize_to_bsize[TX_SIZES] = {
  BLOCK_4X4,    // TX_4X4
  BLOCK_8X8,    // TX_8X8
  BLOCK_16X16,  // TX_16X16
  BLOCK_32X32,  // TX_32X32
};

static const TX_SIZE tx_mode_to_biggest_tx_size[TX_MODES] = {
  TX_4X4,    // ONLY_4X4
  TX_8X8,    // ALLOW_8X8
  TX_16X16,  // ALLOW_16X16
  TX_32X32,  // ALLOW_32X32
  TX_32X32,  // TX_MODE_SELECT
};

static const BLOCK_SIZE ss_size_lookup[BLOCK_SIZES][2][2] = {
  //  ss_x == 0    ss_x == 0        ss_x == 1      ss_x == 1
  //  ss_y == 0    ss_y == 1        ss_y == 0      ss_y == 1
  { { BLOCK_4X4, BLOCK_INVALID }, { BLOCK_INVALID, BLOCK_INVALID } },
  { { BLOCK_4X8, BLOCK_4X4 }, { BLOCK_INVALID, BLOCK_INVALID } },
  { { BLOCK_8X4, BLOCK_INVALID }, { BLOCK_4X4, BLOCK_INVALID } },
  { { BLOCK_8X8, BLOCK_8X4 }, { BLOCK_4X8, BLOCK_4X4 } },
  { { BLOCK_8X16, BLOCK_8X8 }, { BLOCK_INVALID, BLOCK_4X8 } },
  { { BLOCK_16X8, BLOCK_INVALID }, { BLOCK_8X8, BLOCK_8X4 } },
  { { BLOCK_16X16, BLOCK_16X8 }, { BLOCK_8X16, BLOCK_8X8 } },
  { { BLOCK_16X32, BLOCK_16X16 }, { BLOCK_INVALID, BLOCK_8X16 } },
  { { BLOCK_32X16, BLOCK_INVALID }, { BLOCK_16X16, BLOCK_16X8 } },
  { { BLOCK_32X32, BLOCK_32X16 }, { BLOCK_16X32, BLOCK_16X16 } },
  { { BLOCK_32X64, BLOCK_32X32 }, { BLOCK_INVALID, BLOCK_16X32 } },
  { { BLOCK_64X32, BLOCK_INVALID }, { BLOCK_32X32, BLOCK_32X16 } },
  { { BLOCK_64X64, BLOCK_64X32 }, { BLOCK_32X64, BLOCK_32X32 } },
};

// Generates 4 bit field in which each bit set to 1 represents
// a blocksize partition  1111 means we split 64x64, 32x32, 16x16
// and 8x8.  1000 means we just split the 64x64 to 32x32
static const struct {
  PARTITION_CONTEXT above;
  PARTITION_CONTEXT left;
} partition_context_lookup[BLOCK_SIZES] = {
  { 15, 15 },  // 4X4   - {0b1111, 0b1111}
  { 15, 14 },  // 4X8   - {0b1111, 0b1110}
  { 14, 15 },  // 8X4   - {0b1110, 0b1111}
  { 14, 14 },  // 8X8   - {0b1110, 0b1110}
  { 14, 12 },  // 8X16  - {0b1110, 0b1100}
  { 12, 14 },  // 16X8  - {0b1100, 0b1110}
  { 12, 12 },  // 16X16 - {0b1100, 0b1100}
  { 12, 8 },   // 16X32 - {0b1100, 0b1000}
  { 8, 12 },   // 32X16 - {0b1000, 0b1100}
  { 8, 8 },    // 32X32 - {0b1000, 0b1000}
  { 8, 0 },    // 32X64 - {0b1000, 0b0000}
  { 0, 8 },    // 64X32 - {0b0000, 0b1000}
  { 0, 0 },    // 64X64 - {0b0000, 0b0000}
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_COMMON_DATA_H_
