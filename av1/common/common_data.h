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

#ifndef AOM_AV1_COMMON_COMMON_DATA_H_
#define AOM_AV1_COMMON_COMMON_DATA_H_

#include "av1/common/enums.h"
#include "aom/aom_integer.h"
#include "aom_dsp/aom_dsp_common.h"

#ifdef __cplusplus
extern "C" {
#endif

// Log 2 conversion lookup tables in units of mode info (4x4).
// The Mi_Width_Log2 table in the spec (Section 9.3. Conversion tables).
static const uint8_t mi_size_wide_log2[BLOCK_SIZES_ALL] = {
  0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5, 5, 0, 2, 1, 3, 2, 4,
#if CONFIG_FLEX_PARTITION
  0, 3, 1, 4, 0, 4,
#endif  // CONFIG_FLEX_PARTITION
};
// The Mi_Height_Log2 table in the spec (Section 9.3. Conversion tables).
static const uint8_t mi_size_high_log2[BLOCK_SIZES_ALL] = {
  0, 1, 0, 1, 2, 1, 2, 3, 2, 3, 4, 3, 4, 5, 4, 5, 2, 0, 3, 1, 4, 2,
#if CONFIG_FLEX_PARTITION
  3, 0, 4, 1, 4, 0,
#endif  // CONFIG_FLEX_PARTITION
};

// Width/height lookup tables in units of mode info (4x4).
// The Num_4x4_Blocks_Wide table in the spec (Section 9.3. Conversion tables).
static const uint8_t mi_size_wide[BLOCK_SIZES_ALL] = {
  1, 1, 2, 2,  2, 4,  4, 4, 8, 8, 8, 16, 16, 16, 32, 32, 1, 4, 2, 8, 4, 16,
#if CONFIG_FLEX_PARTITION
  1, 8, 2, 16, 1, 16,
#endif  // CONFIG_FLEX_PARTITION
};

// The Num_4x4_Blocks_High table in the spec (Section 9.3. Conversion tables).
static const uint8_t mi_size_high[BLOCK_SIZES_ALL] = {
  1, 2, 1,  2, 4,  2, 4, 8, 4, 8, 16, 8, 16, 32, 16, 32, 4, 1, 8, 2, 16, 4,
#if CONFIG_FLEX_PARTITION
  8, 1, 16, 2, 16, 1,
#endif  // CONFIG_FLEX_PARTITION
};

// Width/height lookup tables in units of samples.
// The Block_Width table in the spec (Section 9.3. Conversion tables).
static const uint8_t block_size_wide[BLOCK_SIZES_ALL] = {
  4,  4,  8,  8,   8,   16, 16, 16, 32, 32, 32,
  64, 64, 64, 128, 128, 4,  16, 8,  32, 16, 64,
#if CONFIG_FLEX_PARTITION
  4,  32, 8,  64,  4,   64,
#endif  // CONFIG_FLEX_PARTITION
};

// The Block_Height table in the spec (Section 9.3. Conversion tables).
static const uint8_t block_size_high[BLOCK_SIZES_ALL] = {
  4,  8,  4,   8,  16,  8,  16, 32, 16, 32, 64,
  32, 64, 128, 64, 128, 16, 4,  32, 8,  64, 16,
#if CONFIG_FLEX_PARTITION
  32, 4,  64,  8,  64,  4,
#endif  // CONFIG_FLEX_PARTITION
};

// Maps a block size to a context.
// The Size_Group table in the spec (Section 9.3. Conversion tables).
// AOMMIN(3, AOMMIN(mi_size_wide_log2(bsize), mi_size_high_log2(bsize)))
static const uint8_t size_group_lookup[BLOCK_SIZES_ALL] = {
  0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 0, 0, 1, 1, 2, 2,
#if CONFIG_FLEX_PARTITION
  1, 1, 2, 2, 2, 2,
#endif  // CONFIG_FLEX_PARTITION
};

static const uint8_t num_pels_log2_lookup[BLOCK_SIZES_ALL] = {
  4, 5, 5, 6, 7, 7, 8, 9, 9, 10, 11, 11, 12, 13, 13, 14, 6, 6, 8, 8, 10, 10,
#if CONFIG_FLEX_PARTITION
  7, 7, 9, 9, 8, 8,
#endif  // CONFIG_FLEX_PARTITION
};

#if CONFIG_EXT_RECUR_PARTITIONS
static const PARTITION_TYPE
    partition_map_from_symbol_block_wgth[PARTITION_TYPES_REC] = {
      PARTITION_NONE,
      PARTITION_VERT,
      PARTITION_HORZ,
#if CONFIG_EXT_PARTITIONS
      PARTITION_VERT_3,
#else
      PARTITION_VERT_4,
#endif  // CONFIG_EXT_PARTITIONS
    };

static const PARTITION_TYPE_REC
    symbol_map_from_partition_block_wgth[EXT_PARTITION_TYPES] = {
      PARTITION_NONE_REC,      PARTITION_SHORT_SIDE_REC,
      PARTITION_LONG_SIDE_REC, PARTITION_INVALID_REC,
      PARTITION_INVALID_REC,   PARTITION_INVALID_REC,
      PARTITION_INVALID_REC,   PARTITION_INVALID_REC,
      PARTITION_INVALID_REC,   PARTITION_MULTI_WAY_REC,
    };

static const PARTITION_TYPE
    partition_map_from_symbol_block_hgtw[PARTITION_TYPES_REC] = {
      PARTITION_NONE,
      PARTITION_HORZ,
      PARTITION_VERT,
#if CONFIG_EXT_PARTITIONS
      PARTITION_HORZ_3,
#else
      PARTITION_HORZ_4,
#endif  // CONFIG_EXT_PARTITIONS
    };

static const PARTITION_TYPE_REC
    symbol_map_from_partition_block_hgtw[EXT_PARTITION_TYPES] = {
      PARTITION_NONE_REC,    PARTITION_LONG_SIDE_REC, PARTITION_SHORT_SIDE_REC,
      PARTITION_INVALID_REC, PARTITION_INVALID_REC,   PARTITION_INVALID_REC,
      PARTITION_INVALID_REC, PARTITION_INVALID_REC,   PARTITION_MULTI_WAY_REC,
      PARTITION_INVALID_REC,
    };

/* clang-format off */
// This table covers all square blocks and 1:2/2:1 rectangular blocks
static const BLOCK_SIZE subsize_lookup[EXT_PARTITION_TYPES][BLOCK_SIZES] = {
  {     // PARTITION_NONE
    BLOCK_4X4, BLOCK_4X8, BLOCK_8X4, BLOCK_8X8, BLOCK_8X16, BLOCK_16X8,
    BLOCK_16X16, BLOCK_16X32, BLOCK_32X16, BLOCK_32X32, BLOCK_32X64,
    BLOCK_64X32, BLOCK_64X64, BLOCK_64X128, BLOCK_128X64, BLOCK_128X128
  }, {  // PARTITION_HORZ
    BLOCK_INVALID, BLOCK_4X4, BLOCK_INVALID, BLOCK_8X4, BLOCK_8X8, BLOCK_16X4,
    BLOCK_16X8, BLOCK_16X16, BLOCK_32X8, BLOCK_32X16, BLOCK_32X32, BLOCK_64X16,
    BLOCK_64X32, BLOCK_64X64, BLOCK_INVALID, BLOCK_128X64
  }, {  // PARTITION_VERT
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4, BLOCK_4X8, BLOCK_4X16, BLOCK_8X8,
    BLOCK_8X16, BLOCK_8X32, BLOCK_16X16, BLOCK_16X32, BLOCK_16X64, BLOCK_32X32,
    BLOCK_32X64, BLOCK_INVALID, BLOCK_64X64, BLOCK_64X128
  }, {  // PARTITION_SPLIT
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X4, BLOCK_INVALID,
    BLOCK_INVALID, BLOCK_8X8, BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X16,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X32, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_64X64
  }, {  // PARTITION_HORZ_A
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4, BLOCK_INVALID,
    BLOCK_INVALID, BLOCK_16X8, BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_128X64
  }, {  // PARTITION_HORZ_B
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4, BLOCK_INVALID,
    BLOCK_INVALID, BLOCK_16X8, BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X16,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_64X32, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_128X64
  }, {  // PARTITION_VERT_A
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8, BLOCK_INVALID,
    BLOCK_INVALID, BLOCK_8X16, BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_64X128
  }, {  // PARTITION_VERT_B
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X8, BLOCK_INVALID,
    BLOCK_INVALID, BLOCK_8X16, BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X32,
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_32X64, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_64X128
#if CONFIG_EXT_PARTITIONS
  }, {  // PARTITION_HORZ_3
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
    BLOCK_INVALID, BLOCK_16X4, BLOCK_16X8, BLOCK_INVALID, BLOCK_32X8,
    BLOCK_32X16, BLOCK_INVALID, BLOCK_64X16, BLOCK_64X32, BLOCK_INVALID,
    BLOCK_INVALID
  }, {  // PARTITION_VERT_3
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_4X8, BLOCK_4X16, BLOCK_INVALID, BLOCK_8X16, BLOCK_8X32, BLOCK_INVALID,
    BLOCK_16X32, BLOCK_16X64, BLOCK_INVALID, BLOCK_32X64, BLOCK_INVALID
  }
#else
}, {  // PARTITION_HORZ_4
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_8X4,
    BLOCK_INVALID, BLOCK_16X4, BLOCK_16X8, BLOCK_INVALID, BLOCK_32X8,
    BLOCK_32X16, BLOCK_INVALID, BLOCK_64X16, BLOCK_64X32, BLOCK_INVALID,
    BLOCK_INVALID
  }, {  // PARTITION_VERT_4
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID, BLOCK_INVALID,
    BLOCK_4X8, BLOCK_4X16, BLOCK_INVALID, BLOCK_8X16, BLOCK_8X32, BLOCK_INVALID,
    BLOCK_16X32, BLOCK_16X64, BLOCK_INVALID, BLOCK_32X64, BLOCK_INVALID
  }
#endif  // CONFIG_EXT_PARTITIONS
};
#else  // CONFIG_EXT_RECUR_PARTITIONS
/* clang-format off */
// A compressed version of the Partition_Subsize table in the spec (9.3.
// Conversion tables), for square block sizes only.
static const BLOCK_SIZE subsize_lookup[EXT_PARTITION_TYPES][SQR_BLOCK_SIZES] = {
  {     // PARTITION_NONE
    BLOCK_4X4, BLOCK_8X8, BLOCK_16X16,
    BLOCK_32X32, BLOCK_64X64, BLOCK_128X128
  }, {  // PARTITION_HORZ
    BLOCK_INVALID, BLOCK_8X4, BLOCK_16X8,
    BLOCK_32X16, BLOCK_64X32, BLOCK_128X64
  }, {  // PARTITION_VERT
    BLOCK_INVALID, BLOCK_4X8, BLOCK_8X16,
    BLOCK_16X32, BLOCK_32X64, BLOCK_64X128
  }, {  // PARTITION_SPLIT
    BLOCK_INVALID, BLOCK_4X4, BLOCK_8X8,
    BLOCK_16X16, BLOCK_32X32, BLOCK_64X64
  }, {  // PARTITION_HORZ_A
    BLOCK_INVALID, BLOCK_8X4, BLOCK_16X8,
    BLOCK_32X16, BLOCK_64X32, BLOCK_128X64
  }, {  // PARTITION_HORZ_B
    BLOCK_INVALID, BLOCK_8X4, BLOCK_16X8,
    BLOCK_32X16, BLOCK_64X32, BLOCK_128X64
  }, {  // PARTITION_VERT_A
    BLOCK_INVALID, BLOCK_4X8, BLOCK_8X16,
    BLOCK_16X32, BLOCK_32X64, BLOCK_64X128
  }, {  // PARTITION_VERT_B
    BLOCK_INVALID, BLOCK_4X8, BLOCK_8X16,
    BLOCK_16X32, BLOCK_32X64, BLOCK_64X128
#if CONFIG_EXT_PARTITIONS
  }, {  // PARTITION_HORZ_3
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X4,
    BLOCK_32X8, BLOCK_64X16, BLOCK_INVALID
  }, {  // PARTITION_VERT_3
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
    BLOCK_8X32, BLOCK_16X64, BLOCK_INVALID
  }
#else
}, {  // PARTITION_HORZ_4
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_16X4,
    BLOCK_32X8, BLOCK_64X16, BLOCK_INVALID
  }, {  // PARTITION_VERT_4
    BLOCK_INVALID, BLOCK_INVALID, BLOCK_4X16,
    BLOCK_8X32, BLOCK_16X64, BLOCK_INVALID
  }
#endif  // CONFIG_EXT_PARTITIONS
};
#endif  // CONFIG_EXT_RECUR_PARTITIONS

static const TX_SIZE max_txsize_lookup[BLOCK_SIZES_ALL] = {
  //                   4X4
                       TX_4X4,
  // 4X8,    8X4,      8X8
  TX_4X4,    TX_4X4,   TX_8X8,
  // 8X16,   16X8,     16X16
  TX_8X8,    TX_8X8,   TX_16X16,
  // 16X32,  32X16,    32X32
  TX_16X16,  TX_16X16, TX_32X32,
  // 32X64,  64X32,
  TX_32X32,  TX_32X32,
  // 64X64
  TX_64X64,
  // 64x128, 128x64,   128x128
  TX_64X64,  TX_64X64, TX_64X64,
  // 4x16,   16x4,     8x32
  TX_4X4,    TX_4X4,   TX_8X8,
  // 32x8,   16x64,    64x16
  TX_8X8,    TX_16X16, TX_16X16,
#if CONFIG_FLEX_PARTITION
  // 4x32,   32x4,     8x64
  TX_4X4,    TX_4X4,   TX_8X8,
  // 64x8,   4x64,     64x4
  TX_8X8,    TX_4X4,   TX_4X4,
#endif  // CONFIG_FLEX_PARTITION
};

static const TX_SIZE max_txsize_rect_lookup[BLOCK_SIZES_ALL] = {
      // 4X4
      TX_4X4,
      // 4X8,    8X4,      8X8
      TX_4X8,    TX_8X4,   TX_8X8,
      // 8X16,   16X8,     16X16
      TX_8X16,   TX_16X8,  TX_16X16,
      // 16X32,  32X16,    32X32
      TX_16X32,  TX_32X16, TX_32X32,
      // 32X64,  64X32,
      TX_32X64,  TX_64X32,
      // 64X64
      TX_64X64,
      // 64x128, 128x64,   128x128
      TX_64X64,  TX_64X64, TX_64X64,
      // 4x16,   16x4,
      TX_4X16,   TX_16X4,
      // 8x32,   32x8
      TX_8X32,   TX_32X8,
      // 16x64,  64x16
      TX_16X64,  TX_64X16,
#if CONFIG_FLEX_PARTITION
      // 4x32,   32x4,
      TX_4X32,   TX_32X4,
      // 8x64,   64x8
      TX_8X64,   TX_64X8,
      // 4x64,   64x4
      TX_4X64,   TX_64X4
#endif  // CONFIG_FLEX_PARTITION
};

static const TX_TYPE_1D vtx_tab[TX_TYPES] = {
  DCT_1D,      ADST_1D,  DCT_1D,      ADST_1D,
  FLIPADST_1D, DCT_1D,   FLIPADST_1D, ADST_1D, FLIPADST_1D, IDTX_1D,
  DCT_1D,      IDTX_1D,  ADST_1D,     IDTX_1D, FLIPADST_1D, IDTX_1D,
#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTRA
  MDTX1_1D,    MDTX1_1D, DCT_1D,
#if USE_NST_INTRA
  NSTX,
#endif
#endif
#if USE_MDTX_INTER
  MDTX1_1D,    MDTX1_1D, DCT_1D,      MDTX1_1D, MDTX1_1D,   DCT_1D,
  MDTX1_1D,    MDTX1_1D,
#endif
#endif
};

static const TX_TYPE_1D htx_tab[TX_TYPES] = {
  DCT_1D,   DCT_1D,      ADST_1D,     ADST_1D,
  DCT_1D,   FLIPADST_1D, FLIPADST_1D, FLIPADST_1D, ADST_1D, IDTX_1D,
  IDTX_1D,  DCT_1D,      IDTX_1D,     ADST_1D,     IDTX_1D, FLIPADST_1D,
#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTRA
  MDTX1_1D, DCT_1D,      MDTX1_1D,
#if USE_NST_INTRA
  NSTX,
#endif
#endif
#if USE_MDTX_INTER
  MDTX1_1D, DCT_1D,      MDTX1_1D,    MDTX1_1D,    DCT_1D,  MDTX1_1D,
  MDTX1_1D, MDTX1_1D,
#endif
#endif
};

#define TXSIZE_CAT_INVALID (-1)

/* clang-format on */

#if CONFIG_NEW_TX_PARTITION
// Smallest sub_tx size units. Used to compute the index in the
// tx type map.
static const TX_SIZE smallest_sub_tx_size_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_4X4,    // TX_8X8
  TX_4X4,    // TX_16X16
  TX_8X8,    // TX_32X32
  TX_16X16,  // TX_64X64
  TX_4X4,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_4X4,    // TX_8X16
  TX_4X4,    // TX_16X8
  TX_4X8,    // TX_16X32
  TX_8X4,    // TX_32X16
  TX_8X16,   // TX_32X64
  TX_16X8,   // TX_64X32
  TX_4X4,    // TX_4X16
  TX_4X4,    // TX_16X4
  TX_4X8,    // TX_8X32
  TX_8X4,    // TX_32X8
  TX_4X16,   // TX_16X64
  TX_16X4,   // TX_64X16
#if CONFIG_FLEX_PARTITION
  TX_4X8,   // TX_4X32
  TX_8X4,   // TX_32X4
  TX_4X16,  // TX_8X64
  TX_16X4,  // TX_64X8
  TX_4X16,  // TX_4X64
  TX_16X4,  // TX_64X4
#endif      // CONFIG_FLEX_PARTITION
};
#endif

static const TX_SIZE sub_tx_size_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_4X4,    // TX_8X8
  TX_8X8,    // TX_16X16
  TX_16X16,  // TX_32X32
  TX_32X32,  // TX_64X64
  TX_4X4,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_8X8,    // TX_8X16
  TX_8X8,    // TX_16X8
  TX_16X16,  // TX_16X32
  TX_16X16,  // TX_32X16
  TX_32X32,  // TX_32X64
  TX_32X32,  // TX_64X32
  TX_4X8,    // TX_4X16
  TX_8X4,    // TX_16X4
  TX_8X16,   // TX_8X32
  TX_16X8,   // TX_32X8
  TX_16X32,  // TX_16X64
  TX_32X16,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  TX_4X16,  // TX_4X32
  TX_16X4,  // TX_32X4
  TX_8X32,  // TX_8X64
  TX_32X8,  // TX_64X8
  TX_4X32,  // TX_4X64
  TX_32X4,  // TX_64X4
#endif      // CONFIG_FLEX_PARTITION
};

static const TX_SIZE txsize_horz_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
  TX_64X64,  // TX_64X64
  TX_4X4,    // TX_4X8
  TX_8X8,    // TX_8X4
  TX_8X8,    // TX_8X16
  TX_16X16,  // TX_16X8
  TX_16X16,  // TX_16X32
  TX_32X32,  // TX_32X16
  TX_32X32,  // TX_32X64
  TX_64X64,  // TX_64X32
  TX_4X4,    // TX_4X16
  TX_16X16,  // TX_16X4
  TX_8X8,    // TX_8X32
  TX_32X32,  // TX_32X8
  TX_16X16,  // TX_16X64
  TX_64X64,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  TX_4X4,    // TX_4X32
  TX_32X32,  // TX_32X4
  TX_8X8,    // TX_8X64
  TX_64X64,  // TX_64X8
  TX_4X4,    // TX_4X64
  TX_64X64,  // TX_64X4
#endif       // CONFIG_FLEX_PARTITION
};

static const TX_SIZE txsize_vert_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
  TX_64X64,  // TX_64X64
  TX_8X8,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_16X16,  // TX_8X16
  TX_8X8,    // TX_16X8
  TX_32X32,  // TX_16X32
  TX_16X16,  // TX_32X16
  TX_64X64,  // TX_32X64
  TX_32X32,  // TX_64X32
  TX_16X16,  // TX_4X16
  TX_4X4,    // TX_16X4
  TX_32X32,  // TX_8X32
  TX_8X8,    // TX_32X8
  TX_64X64,  // TX_16X64
  TX_16X16,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  TX_32X32,  // TX_4X32
  TX_4X4,    // TX_32X4
  TX_64X64,  // TX_8X64
  TX_8X8,    // TX_64X8
  TX_64X64,  // TX_4X64
  TX_4X4,    // TX_64X4
#endif       // CONFIG_FLEX_PARTITION
};

#define TX_SIZE_W_MIN 4

// Transform block width in pixels
static const int tx_size_wide[TX_SIZES_ALL] = {
  4, 8,  16, 32, 64, 4,  8, 8, 16, 16, 32, 32, 64, 4, 16, 8, 32, 16, 64,
#if CONFIG_FLEX_PARTITION
  4, 32, 8,  64, 4,  64,
#endif  // CONFIG_FLEX_PARTITION
};

#define TX_SIZE_H_MIN 4

// Transform block height in pixels
static const int tx_size_high[TX_SIZES_ALL] = {
  4,  8, 16, 32, 64, 8, 4, 16, 8, 32, 16, 64, 32, 16, 4, 32, 8, 64, 16,
#if CONFIG_FLEX_PARTITION
  32, 4, 64, 8,  64, 4,
#endif  // CONFIG_FLEX_PARTITION
};

// Transform block width in unit
static const int tx_size_wide_unit[TX_SIZES_ALL] = {
  1, 2, 4, 8,  16, 1,  2, 2, 4, 4, 8, 8, 16, 1, 4, 2, 8, 4, 16,
#if CONFIG_FLEX_PARTITION
  1, 8, 2, 16, 1,  16,
#endif  // CONFIG_FLEX_PARTITION
};

// Transform block height in unit
static const int tx_size_high_unit[TX_SIZES_ALL] = {
  1, 2, 4,  8, 16, 2, 1, 4, 2, 8, 4, 16, 8, 4, 1, 8, 2, 16, 4,
#if CONFIG_FLEX_PARTITION
  8, 1, 16, 2, 16, 1,
#endif  // CONFIG_FLEX_PARTITION
};

// Transform block width in log2
static const int tx_size_wide_log2[TX_SIZES_ALL] = {
  2, 3, 4, 5, 6, 2, 3, 3, 4, 4, 5, 5, 6, 2, 4, 3, 5, 4, 6,
#if CONFIG_FLEX_PARTITION
  2, 5, 3, 6, 2, 6,
#endif  // CONFIG_FLEX_PARTITION
};

// Transform block height in log2
static const int tx_size_high_log2[TX_SIZES_ALL] = {
  2, 3, 4, 5, 6, 3, 2, 4, 3, 5, 4, 6, 5, 4, 2, 5, 3, 6, 4,
#if CONFIG_FLEX_PARTITION
  5, 2, 6, 3, 6, 2,
#endif  // CONFIG_FLEX_PARTITION
};

static const int tx_size_2d[TX_SIZES_ALL + 1] = {
  16,  64,   256,  1024, 4096, 32,  32,  128,  128,  512,
  512, 2048, 2048, 64,   64,   256, 256, 1024, 1024,
#if CONFIG_FLEX_PARTITION
  128, 128,  512,  512,  256,  256,
#endif  // CONFIG_FLEX_PARTITION
};

static const BLOCK_SIZE txsize_to_bsize[TX_SIZES_ALL] = {
  BLOCK_4X4,    // TX_4X4
  BLOCK_8X8,    // TX_8X8
  BLOCK_16X16,  // TX_16X16
  BLOCK_32X32,  // TX_32X32
  BLOCK_64X64,  // TX_64X64
  BLOCK_4X8,    // TX_4X8
  BLOCK_8X4,    // TX_8X4
  BLOCK_8X16,   // TX_8X16
  BLOCK_16X8,   // TX_16X8
  BLOCK_16X32,  // TX_16X32
  BLOCK_32X16,  // TX_32X16
  BLOCK_32X64,  // TX_32X64
  BLOCK_64X32,  // TX_64X32
  BLOCK_4X16,   // TX_4X16
  BLOCK_16X4,   // TX_16X4
  BLOCK_8X32,   // TX_8X32
  BLOCK_32X8,   // TX_32X8
  BLOCK_16X64,  // TX_16X64
  BLOCK_64X16,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  BLOCK_4X32,  // TX_4X32
  BLOCK_32X4,  // TX_32X4
  BLOCK_8X64,  // TX_8X64
  BLOCK_64X8,  // TX_64X8
  BLOCK_4X64,  // TX_4X64
  BLOCK_64X4,  // TX_64X4
#endif         // CONFIG_FLEX_PARTITION
};

static const TX_SIZE txsize_sqr_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
  TX_64X64,  // TX_64X64
  TX_4X4,    // TX_4X8
  TX_4X4,    // TX_8X4
  TX_8X8,    // TX_8X16
  TX_8X8,    // TX_16X8
  TX_16X16,  // TX_16X32
  TX_16X16,  // TX_32X16
  TX_32X32,  // TX_32X64
  TX_32X32,  // TX_64X32
  TX_4X4,    // TX_4X16
  TX_4X4,    // TX_16X4
  TX_8X8,    // TX_8X32
  TX_8X8,    // TX_32X8
  TX_16X16,  // TX_16X64
  TX_16X16,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  TX_4X4,  // TX_4X32
  TX_4X4,  // TX_32X4
  TX_8X8,  // TX_8X64
  TX_8X8,  // TX_64X8
  TX_4X4,  // TX_4X64
  TX_4X4,  // TX_64X4
#endif     // CONFIG_FLEX_PARTITION
};

static const TX_SIZE txsize_sqr_up_map[TX_SIZES_ALL] = {
  TX_4X4,    // TX_4X4
  TX_8X8,    // TX_8X8
  TX_16X16,  // TX_16X16
  TX_32X32,  // TX_32X32
  TX_64X64,  // TX_64X64
  TX_8X8,    // TX_4X8
  TX_8X8,    // TX_8X4
  TX_16X16,  // TX_8X16
  TX_16X16,  // TX_16X8
  TX_32X32,  // TX_16X32
  TX_32X32,  // TX_32X16
  TX_64X64,  // TX_32X64
  TX_64X64,  // TX_64X32
  TX_16X16,  // TX_4X16
  TX_16X16,  // TX_16X4
  TX_32X32,  // TX_8X32
  TX_32X32,  // TX_32X8
  TX_64X64,  // TX_16X64
  TX_64X64,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  TX_32X32,  // TX_4X32
  TX_32X32,  // TX_32X4
  TX_64X64,  // TX_8X64
  TX_64X64,  // TX_64X8
  TX_64X64,  // TX_4X64
  TX_64X64,  // TX_64X4
#endif       // CONFIG_FLEX_PARTITION
};

static const int8_t txsize_log2_minus4[TX_SIZES_ALL] = {
  0,  // TX_4X4
  2,  // TX_8X8
  4,  // TX_16X16
  6,  // TX_32X32
  6,  // TX_64X64
  1,  // TX_4X8
  1,  // TX_8X4
  3,  // TX_8X16
  3,  // TX_16X8
  5,  // TX_16X32
  5,  // TX_32X16
  6,  // TX_32X64
  6,  // TX_64X32
  2,  // TX_4X16
  2,  // TX_16X4
  4,  // TX_8X32
  4,  // TX_32X8
  5,  // TX_16X64
  5,  // TX_64X16
#if CONFIG_FLEX_PARTITION
  3,    // TX_4X32
  3,    // TX_32X4
  4,    // TX_8X64
  4,    // TX_64X8
  3,    // TX_4X64
  3,    // TX_64X4
#endif  // CONFIG_FLEX_PARTITION
};

/* clang-format off */
static const TX_SIZE tx_mode_to_biggest_tx_size[TX_MODES] = {
  TX_4X4,    // ONLY_4X4
  TX_64X64,  // TX_MODE_LARGEST
  TX_64X64,  // TX_MODE_SELECT
};

// The Subsampled_Size table in the spec (Section 5.11.38. Get plane residual
// size function).
static const BLOCK_SIZE ss_size_lookup[BLOCK_SIZES_ALL][2][2] = {
  //  ss_x == 0      ss_x == 0          ss_x == 1      ss_x == 1
  //  ss_y == 0      ss_y == 1          ss_y == 0      ss_y == 1
  { { BLOCK_4X4,     BLOCK_4X4 },     { BLOCK_4X4,     BLOCK_4X4 } },
  { { BLOCK_4X8,     BLOCK_4X4 },     { BLOCK_INVALID, BLOCK_4X4 } },
  { { BLOCK_8X4,     BLOCK_INVALID }, { BLOCK_4X4,     BLOCK_4X4 } },
  { { BLOCK_8X8,     BLOCK_8X4 },     { BLOCK_4X8,     BLOCK_4X4 } },
  { { BLOCK_8X16,    BLOCK_8X8 },     { BLOCK_INVALID, BLOCK_4X8 } },
  { { BLOCK_16X8,    BLOCK_INVALID }, { BLOCK_8X8,     BLOCK_8X4 } },
  { { BLOCK_16X16,   BLOCK_16X8 },    { BLOCK_8X16,    BLOCK_8X8 } },
  { { BLOCK_16X32,   BLOCK_16X16 },   { BLOCK_INVALID, BLOCK_8X16 } },
  { { BLOCK_32X16,   BLOCK_INVALID }, { BLOCK_16X16,   BLOCK_16X8 } },
  { { BLOCK_32X32,   BLOCK_32X16 },   { BLOCK_16X32,   BLOCK_16X16 } },
  { { BLOCK_32X64,   BLOCK_32X32 },   { BLOCK_INVALID, BLOCK_16X32 } },
  { { BLOCK_64X32,   BLOCK_INVALID }, { BLOCK_32X32,   BLOCK_32X16 } },
  { { BLOCK_64X64,   BLOCK_64X32 },   { BLOCK_32X64,   BLOCK_32X32 } },
  { { BLOCK_64X128,  BLOCK_64X64 },   { BLOCK_INVALID, BLOCK_32X64 } },
  { { BLOCK_128X64,  BLOCK_INVALID }, { BLOCK_64X64,   BLOCK_64X32 } },
  { { BLOCK_128X128, BLOCK_128X64 },  { BLOCK_64X128,  BLOCK_64X64 } },
  { { BLOCK_4X16,    BLOCK_4X8 },     { BLOCK_INVALID, BLOCK_4X8 } },
  { { BLOCK_16X4,    BLOCK_INVALID }, { BLOCK_8X4,     BLOCK_8X4 } },
  { { BLOCK_8X32,    BLOCK_8X16 },    { BLOCK_INVALID, BLOCK_4X16 } },
  { { BLOCK_32X8,    BLOCK_INVALID }, { BLOCK_16X8,    BLOCK_16X4 } },
  { { BLOCK_16X64,   BLOCK_16X32 },   { BLOCK_INVALID, BLOCK_8X32 } },
  { { BLOCK_64X16,   BLOCK_INVALID }, { BLOCK_32X16,   BLOCK_32X8 } },
#if CONFIG_FLEX_PARTITION
  { { BLOCK_4X32,    BLOCK_4X16 },    { BLOCK_INVALID, BLOCK_4X16 } },
  { { BLOCK_32X4,    BLOCK_INVALID }, { BLOCK_16X4,    BLOCK_16X4 } },
  { { BLOCK_8X64,    BLOCK_8X32 },    { BLOCK_4X64,    BLOCK_4X32 } },
  { { BLOCK_64X8,    BLOCK_64X4 },    { BLOCK_32X8,    BLOCK_32X4 } },
  { { BLOCK_4X64,    BLOCK_4X32 },    { BLOCK_INVALID, BLOCK_4X32 } },
  { { BLOCK_64X4,    BLOCK_INVALID }, { BLOCK_32X4,    BLOCK_32X4 } },
#endif  // CONFIG_FLEX_PARTITION
};
/* clang-format on */

// Generates 5 bit field in which each bit set to 1 represents
// a blocksize partition  11111 means we split 128x128, 64x64, 32x32, 16x16
// and 8x8.  10000 means we just split the 128x128 to 64x64
/* clang-format off */
static const struct {
  PARTITION_CONTEXT above;
  PARTITION_CONTEXT left;
} partition_context_lookup[BLOCK_SIZES_ALL] = {
  { 31, 31 },  // 4X4   - {0b11111, 0b11111}
  { 31, 30 },  // 4X8   - {0b11111, 0b11110}
  { 30, 31 },  // 8X4   - {0b11110, 0b11111}
  { 30, 30 },  // 8X8   - {0b11110, 0b11110}
  { 30, 28 },  // 8X16  - {0b11110, 0b11100}
  { 28, 30 },  // 16X8  - {0b11100, 0b11110}
  { 28, 28 },  // 16X16 - {0b11100, 0b11100}
  { 28, 24 },  // 16X32 - {0b11100, 0b11000}
  { 24, 28 },  // 32X16 - {0b11000, 0b11100}
  { 24, 24 },  // 32X32 - {0b11000, 0b11000}
  { 24, 16 },  // 32X64 - {0b11000, 0b10000}
  { 16, 24 },  // 64X32 - {0b10000, 0b11000}
  { 16, 16 },  // 64X64 - {0b10000, 0b10000}
  { 16, 0 },   // 64X128- {0b10000, 0b00000}
  { 0, 16 },   // 128X64- {0b00000, 0b10000}
  { 0, 0 },    // 128X128-{0b00000, 0b00000}
  { 31, 28 },  // 4X16  - {0b11111, 0b11100}
  { 28, 31 },  // 16X4  - {0b11100, 0b11111}
  { 30, 24 },  // 8X32  - {0b11110, 0b11000}
  { 24, 30 },  // 32X8  - {0b11000, 0b11110}
  { 28, 16 },  // 16X64 - {0b11100, 0b10000}
  { 16, 28 },  // 64X16 - {0b10000, 0b11100}
#if CONFIG_FLEX_PARTITION
  { 31, 24 },  // 4X32  - {0b11111, 0b11000}
  { 24, 31 },  // 32X4  - {0b11000, 0b11111}
  { 30, 16 },  // 8X64  - {0b11110, 0b10000}
  { 16, 30 },  // 64X8  - {0b10000, 0b11110}
  { 31, 16 },  // 4X64  - {0b11110, 0b10000}
  { 16, 31 },  // 64X4  - {0b10000, 0b11110}
#endif  // CONFIG_FLEX_PARTITION
};
/* clang-format on */

#if CONFIG_DERIVED_INTRA_MODE
static const int dr_mode_to_index[INTRA_MODES] = {
  -1, 0, 1, 2, 3, 4, 5, 6, 7, -1, -1, -1, -1,
};

static const int dr_index_to_mode[DIRECTIONAL_MODES] = {
  V_PRED,    H_PRED,    D45_PRED,  D135_PRED,
  D113_PRED, D157_PRED, D203_PRED, D67_PRED,
};

static const int none_dr_mode_to_index[INTRA_MODES] = {
  0, -1, -1, -1, -1, -1, -1, -1, -1, 1, 2, 3, 4,
};

static const int none_dr_index_to_mode[NONE_DIRECTIONAL_MODES] = {
  DC_PRED, SMOOTH_PRED, SMOOTH_V_PRED, SMOOTH_H_PRED, PAETH_PRED
};

static const int intra_mode_context[INTRA_MODES] = {
  0, 1, 3, 1, 2, 2, 2, 3, 1, 4, 4, 4, 5,
};
#else
static const int intra_mode_context[INTRA_MODES] = {
  0, 1, 2, 3, 4, 4, 4, 4, 3, 0, 1, 2, 0,
};
#endif  // CONFIG_DERIVED_INTRA_MODE

// Note: this is also used in unit tests. So whenever one changes the table,
// the unit tests need to be changed accordingly.
static const int quant_dist_weight[4][2] = {
  { 2, 3 }, { 2, 5 }, { 2, 7 }, { 1, MAX_FRAME_DISTANCE }
};
static const int quant_dist_lookup_table[2][4][2] = {
  { { 9, 7 }, { 11, 5 }, { 12, 4 }, { 13, 3 } },
  { { 7, 9 }, { 5, 11 }, { 4, 12 }, { 3, 13 } },
};

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_COMMON_DATA_H_
