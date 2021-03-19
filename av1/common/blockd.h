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

#ifndef AOM_AV1_COMMON_BLOCKD_H_
#define AOM_AV1_COMMON_BLOCKD_H_

#include "config/aom_config.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_ports/mem.h"
#include "aom_scale/yv12config.h"

#include "av1/common/chroma.h"
#include "av1/common/common_data.h"
#include "av1/common/quant_common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/mv.h"
#include "av1/common/scale.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"

#if CONFIG_INTRA_ENTROPY
#include "av1/common/intra_entropy_models.h"
#include "av1/common/nn_em.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define USE_B_QUANT_NO_TRELLIS 1

#define MAX_MB_PLANE 3

#define MAX_DIFFWTD_MASK_BITS 1

#define INTERINTRA_WEDGE_SIGN 0

// DIFFWTD_MASK_TYPES should not surpass 1 << MAX_DIFFWTD_MASK_BITS
enum {
#if CONFIG_DIFFWTD_42
  DIFFWTD_42 = 0,
  DIFFWTD_42_INV,
#else
  DIFFWTD_38 = 0,
  DIFFWTD_38_INV,
#endif  // CONFIG_DIFFWTD_42
  DIFFWTD_MASK_TYPES,
} UENUM1BYTE(DIFFWTD_MASK_TYPE);

enum {
  KEY_FRAME = 0,
  INTER_FRAME = 1,
  INTRA_ONLY_FRAME = 2,  // replaces intra-only
  S_FRAME = 3,
  FRAME_TYPES,
} UENUM1BYTE(FRAME_TYPE);

static INLINE int is_comp_ref_allowed(BLOCK_SIZE bsize) {
  return AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
}

static INLINE int is_intra_mode(PREDICTION_MODE mode) {
  return mode < INTRA_MODE_END;
}

static INLINE int is_inter_mode(PREDICTION_MODE mode) {
  return mode >= INTER_MODE_START && mode < INTER_MODE_END;
}

typedef struct {
  uint8_t *plane[MAX_MB_PLANE];
  int stride[MAX_MB_PLANE];
} BUFFER_SET;

static INLINE int is_square_block(BLOCK_SIZE bsize) {
  return block_size_high[bsize] == block_size_wide[bsize];
}

static INLINE int is_partition_point(BLOCK_SIZE bsize) {
#if CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_FLEX_PARTITION
  return bsize != BLOCK_4X4;
#else   // CONFIG_FLEX_PARTITION
  return bsize != BLOCK_4X4 && bsize < BLOCK_SIZES;
#endif  // CONFIG_FLEX_PARTITION
#else
  return is_square_block(bsize) && bsize >= BLOCK_8X8 && bsize < BLOCK_SIZES;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
}

static INLINE int get_sqr_bsize_idx(BLOCK_SIZE bsize) {
  switch (bsize) {
    case BLOCK_4X4: return 0;
    case BLOCK_8X8: return 1;
    case BLOCK_16X16: return 2;
    case BLOCK_32X32: return 3;
    case BLOCK_64X64: return 4;
    case BLOCK_128X128: return 5;
    default: return SQR_BLOCK_SIZES;
  }
}

// For a square block size 'bsize', returns the size of the sub-blocks used by
// the given partition type. If the partition produces sub-blocks of different
// sizes, then the function returns the largest sub-block size.
// Implements the Partition_Subsize lookup table in the spec (Section 9.3.
// Conversion tables).
// Note: the input block size should be square.
// Otherwise it's considered invalid.
static INLINE BLOCK_SIZE get_partition_subsize(BLOCK_SIZE bsize,
                                               PARTITION_TYPE partition) {
  if (partition == PARTITION_INVALID) {
    return BLOCK_INVALID;
  } else {
#if CONFIG_EXT_RECUR_PARTITIONS
    if (is_partition_point(bsize))
      return subsize_lookup[partition][bsize];
    else
      return partition == PARTITION_NONE ? bsize : BLOCK_INVALID;
#else
    const int sqr_bsize_idx = get_sqr_bsize_idx(bsize);
    return sqr_bsize_idx >= SQR_BLOCK_SIZES
               ? BLOCK_INVALID
               : subsize_lookup[partition][sqr_bsize_idx];
#endif  // CONFIG_EXT_RECUR_PARTITIONS
  }
}

static INLINE int is_partition_valid(BLOCK_SIZE bsize, PARTITION_TYPE p) {
#if CONFIG_EXT_RECUR_PARTITIONS && !KEEP_PARTITION_SPLIT
  if (p == PARTITION_SPLIT) return 0;
#endif  // CONFIG_EXT_RECUR_PARTITIONS && !KEEP_PARTITION_SPLIT
  if (is_partition_point(bsize))
    return get_partition_subsize(bsize, p) < BLOCK_SIZES_ALL;
  else
    return p == PARTITION_NONE;
}

static INLINE int is_inter_singleref_mode(PREDICTION_MODE mode) {
  return mode >= SINGLE_INTER_MODE_START && mode < SINGLE_INTER_MODE_END;
}
static INLINE int is_inter_compound_mode(PREDICTION_MODE mode) {
  return mode >= COMP_INTER_MODE_START && mode < COMP_INTER_MODE_END;
}

static INLINE PREDICTION_MODE compound_ref0_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,           // DC_PRED
    MB_MODE_COUNT,           // V_PRED
    MB_MODE_COUNT,           // H_PRED
    MB_MODE_COUNT,           // D45_PRED
    MB_MODE_COUNT,           // D135_PRED
    MB_MODE_COUNT,           // D113_PRED
    MB_MODE_COUNT,           // D157_PRED
    MB_MODE_COUNT,           // D203_PRED
    MB_MODE_COUNT,           // D67_PRED
    MB_MODE_COUNT,           // SMOOTH_PRED
    MB_MODE_COUNT,           // SMOOTH_V_PRED
    MB_MODE_COUNT,           // SMOOTH_H_PRED
    MB_MODE_COUNT,           // PAETH_PRED
#if !CONFIG_NEW_INTER_MODES  //
    MB_MODE_COUNT,           // NEARESTMV
#endif                       // !CONFIG_NEW_INTER_MODES
    MB_MODE_COUNT,           // NEARMV
    MB_MODE_COUNT,           // GLOBALMV
    MB_MODE_COUNT,           // NEWMV
#if !CONFIG_NEW_INTER_MODES  //
    NEARESTMV,               // NEAREST_NEARESTMV
#endif                       // !CONFIG_NEW_INTER_MODES
    NEARMV,                  // NEAR_NEARMV
#if !CONFIG_NEW_INTER_MODES  //
    NEARESTMV,               // NEAREST_NEWMV
    NEWMV,                   // NEW_NEARESTMV
#endif                       // !CONFIG_NEW_INTER_MODES
    NEARMV,                  // NEAR_NEWMV
    NEWMV,                   // NEW_NEARMV
    GLOBALMV,                // GLOBAL_GLOBALMV
    NEWMV,                   // NEW_NEWMV
#if CONFIG_OPTFLOW_REFINEMENT
    NEARMV,    // NEAR_NEARMV_OPTFLOW
    NEARMV,    // NEAR_NEWMV_OPTFLOW
    NEWMV,     // NEW_NEARMV_OPTFLOW
    GLOBALMV,  // GLOBAL_GLOBALMV_OPTFLOW
    NEWMV,     // NEW_NEWMV_OPTFLOW
#endif         // CONFIG_OPTFLOW_REFINEMENT
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
  assert(is_inter_compound_mode(mode));
  return lut[mode];
}

static INLINE PREDICTION_MODE compound_ref1_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,           // DC_PRED
    MB_MODE_COUNT,           // V_PRED
    MB_MODE_COUNT,           // H_PRED
    MB_MODE_COUNT,           // D45_PRED
    MB_MODE_COUNT,           // D135_PRED
    MB_MODE_COUNT,           // D113_PRED
    MB_MODE_COUNT,           // D157_PRED
    MB_MODE_COUNT,           // D203_PRED
    MB_MODE_COUNT,           // D67_PRED
    MB_MODE_COUNT,           // SMOOTH_PRED
    MB_MODE_COUNT,           // SMOOTH_V_PRED
    MB_MODE_COUNT,           // SMOOTH_H_PRED
    MB_MODE_COUNT,           // PAETH_PRED
#if !CONFIG_NEW_INTER_MODES  //
    MB_MODE_COUNT,           // NEARESTMV
#endif                       // !CONFIG_NEW_INTER_MODES
    MB_MODE_COUNT,           // NEARMV
    MB_MODE_COUNT,           // GLOBALMV
    MB_MODE_COUNT,           // NEWMV
#if !CONFIG_NEW_INTER_MODES  //
    NEARESTMV,               // NEAREST_NEARESTMV
#endif                       // !CONFIG_NEW_INTER_MODES
    NEARMV,                  // NEAR_NEARMV
#if !CONFIG_NEW_INTER_MODES  //
    NEWMV,                   // NEAREST_NEWMV
    NEARESTMV,               // NEW_NEARESTMV
#endif                       // !CONFIG_NEW_INTER_MODES
    NEWMV,                   // NEAR_NEWMV
    NEARMV,                  // NEW_NEARMV
    GLOBALMV,                // GLOBAL_GLOBALMV
    NEWMV,                   // NEW_NEWMV
#if CONFIG_OPTFLOW_REFINEMENT
    NEARMV,    // NEAR_NEARMV_OPTFLOW
    NEWMV,     // NEAR_NEWMV_OPTFLOW
    NEARMV,    // NEW_NEARMV_OPTFLOW
    GLOBALMV,  // GLOBAL_GLOBALMV_OPTFLOW
    NEWMV,     // NEW_NEWMV_OPTFLOW
#endif         // CONFIG_OPTFLOW_REFINEMENT
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
  assert(is_inter_compound_mode(mode));
  return lut[mode];
}

static INLINE int have_nearmv_in_inter_mode(PREDICTION_MODE mode) {
#if CONFIG_OPTFLOW_REFINEMENT
  return (mode == NEARMV || mode == NEAR_NEARMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV || mode == NEAR_NEARMV_OPTFLOW ||
          mode == NEAR_NEWMV_OPTFLOW || mode == NEW_NEARMV_OPTFLOW);
#else
  return (mode == NEARMV || mode == NEAR_NEARMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV);
#endif  // CONFIG_OPTFLOW_REFINEMENT
}

#if CONFIG_NEW_INTER_MODES
static INLINE int have_newmv_in_inter_mode(PREDICTION_MODE mode) {
#if CONFIG_OPTFLOW_REFINEMENT
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV || mode == NEAR_NEWMV_OPTFLOW ||
          mode == NEW_NEARMV_OPTFLOW || mode == NEW_NEWMV_OPTFLOW);
#else
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV);
#endif  // CONFIG_OPTFLOW_REFINEMENT
}
static INLINE int have_drl_index(PREDICTION_MODE mode) {
  return have_nearmv_in_inter_mode(mode) || have_newmv_in_inter_mode(mode);
}
#else
static INLINE int have_newmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAREST_NEWMV ||
          mode == NEW_NEARESTMV || mode == NEAR_NEWMV || mode == NEW_NEARMV);
}

static INLINE int have_drl_index(PREDICTION_MODE mode) {
  return have_nearmv_in_inter_mode(mode) || mode == NEWMV || mode == NEW_NEWMV;
}
#endif  // CONFIG_NEW_INTER_MODES

static INLINE int is_masked_compound_type(COMPOUND_TYPE type) {
  return (type == COMPOUND_WEDGE || type == COMPOUND_DIFFWTD);
}

/* For keyframes, intra block modes are predicted by the (already decoded)
   modes for the Y blocks to the left and above us; for interframes, there
   is a single probability table. */

typedef struct {
  // Value of base colors for Y, U, and V
  uint16_t palette_colors[3 * PALETTE_MAX_SIZE];
  // Number of base colors for Y (0) and UV (1)
  uint8_t palette_size[2];
} PALETTE_MODE_INFO;

typedef struct {
  FILTER_INTRA_MODE filter_intra_mode;
  uint8_t use_filter_intra;
} FILTER_INTRA_MODE_INFO;

static const PREDICTION_MODE fimode_to_intradir[FILTER_INTRA_MODES] = {
  DC_PRED, V_PRED, H_PRED, D157_PRED, DC_PRED
};

#if CONFIG_ADAPT_FILTER_INTRA
typedef struct {
  uint8_t use_adapt_filter_intra;
  ADAPT_FILTER_INTRA_MODE adapt_filter_intra_mode;
} ADAPT_FILTER_INTRA_MODE_INFO;

static const PREDICTION_MODE afimode_to_intradir[ADAPT_FILTER_INTRA_MODES] = {
  D135_PRED, D203_PRED, D67_PRED, D203_PRED, D67_PRED, D203_PRED, D67_PRED
};
#endif  // CONFIG_ADAPT_FILTER_INTRA

#if CONFIG_RD_DEBUG
#define TXB_COEFF_COST_MAP_SIZE (MAX_MIB_SIZE)
#endif

typedef struct RD_STATS {
  int rate;
  int64_t dist;
  // Please be careful of using rdcost, it's not guaranteed to be set all the
  // time.
  // TODO(angiebird): Create a set of functions to manipulate the RD_STATS. In
  // these functions, make sure rdcost is always up-to-date according to
  // rate/dist.
  int64_t rdcost;
  int64_t sse;
  int skip;  // sse should equal to dist when skip == 1
  int zero_rate;
#if CONFIG_RD_DEBUG
  int txb_coeff_cost[MAX_MB_PLANE];
  int txb_coeff_cost_map[MAX_MB_PLANE][TXB_COEFF_COST_MAP_SIZE]
                        [TXB_COEFF_COST_MAP_SIZE];
#endif  // CONFIG_RD_DEBUG
} RD_STATS;

// This struct is used to group function args that are commonly
// sent together in functions related to interinter compound modes
typedef struct {
  uint8_t *seg_mask;
  int8_t wedge_index;
  int8_t wedge_sign;
  DIFFWTD_MASK_TYPE mask_type;
  COMPOUND_TYPE type;
} INTERINTER_COMPOUND_DATA;

#define INTER_TX_SIZE_BUF_LEN 16
#define TXK_TYPE_BUF_LEN 64

typedef struct CHROMA_REF_INFO {
  int is_chroma_ref;
  int offset_started;
  int mi_row_chroma_base;
  int mi_col_chroma_base;
  BLOCK_SIZE bsize;
  BLOCK_SIZE bsize_base;
} CHROMA_REF_INFO;

static INLINE void initialize_chr_ref_info(int mi_row, int mi_col,
                                           BLOCK_SIZE bsize,
                                           CHROMA_REF_INFO *info) {
  info->is_chroma_ref = 1;
  info->offset_started = 0;
  info->mi_row_chroma_base = mi_row;
  info->mi_col_chroma_base = mi_col;
  info->bsize = bsize;
  info->bsize_base = bsize;
}

// Decide whether a block needs coding multiple chroma coding blocks in it at
// once to get around sub-4x4 coding.
static INLINE int have_nz_chroma_ref_offset(BLOCK_SIZE bsize,
                                            PARTITION_TYPE partition,
                                            int subsampling_x,
                                            int subsampling_y) {
  const int bw = block_size_wide[bsize] >> subsampling_x;
  const int bh = block_size_high[bsize] >> subsampling_y;
  const int bw_less_than_4 = bw < 4;
  const int bh_less_than_4 = bh < 4;
  const int hbw_less_than_4 = bw < 8;
  const int hbh_less_than_4 = bh < 8;
  const int qbw_less_than_4 = bw < 16;
  const int qbh_less_than_4 = bh < 16;

  switch (partition) {
    case PARTITION_NONE: return bw_less_than_4 || bh_less_than_4;
    case PARTITION_HORZ: return bw_less_than_4 || hbh_less_than_4;
    case PARTITION_VERT: return hbw_less_than_4 || bh_less_than_4;
    case PARTITION_SPLIT: return hbw_less_than_4 || hbh_less_than_4;
#if CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_HORZ_3: return bw_less_than_4 || qbh_less_than_4;
    case PARTITION_VERT_3: return qbw_less_than_4 || bh_less_than_4;
#else
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_VERT_A:
    case PARTITION_VERT_B: return hbw_less_than_4 || hbh_less_than_4;
    case PARTITION_HORZ_4: return bw_less_than_4 || qbh_less_than_4;
    case PARTITION_VERT_4: return qbw_less_than_4 || bh_less_than_4;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    default:
      assert(0 && "Invalid partition type!");
      return 0;
      break;
  }
}

// Decide whether a subblock is the main chroma reference when its parent block
// needs coding multiple chroma coding blocks at once. The function returns a
// flag indicating whether the mode info used for the combined chroma block is
// located in the subblock.
static INLINE int is_sub_partition_chroma_ref(PARTITION_TYPE partition,
                                              int index, BLOCK_SIZE bsize,
                                              BLOCK_SIZE parent_bsize, int ss_x,
                                              int ss_y, int is_offset_started) {
  (void)is_offset_started;
  (void)parent_bsize;
  const int bw = block_size_wide[bsize];
  const int bh = block_size_high[bsize];
  const int pw = bw >> ss_x;
  const int ph = bh >> ss_y;
  const int pw_less_than_4 = pw < 4;
  const int ph_less_than_4 = ph < 4;

  switch (partition) {
    case PARTITION_NONE: return 1;
    case PARTITION_HORZ:
    case PARTITION_VERT: return index == 1;
    case PARTITION_SPLIT:
      if (is_offset_started) {
        return index == 3;
      } else {
        if (pw_less_than_4 && ph_less_than_4)
          return index == 3;
        else if (pw_less_than_4)
          return index == 1 || index == 3;
        else if (ph_less_than_4)
          return index == 2 || index == 3;
        else
          return 1;
      }
#if CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_VERT_3:
    case PARTITION_HORZ_3: return index == 2;
#else
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
      if (is_offset_started) {
        return index == 2;
      } else {
        const int smallest_w = block_size_wide[parent_bsize] >> (ss_x + 1);
        const int smallest_h = block_size_high[parent_bsize] >> (ss_y + 1);
        const int smallest_w_less_than_4 = smallest_w < 4;
        const int smallest_h_less_than_4 = smallest_h < 4;

        if (smallest_w_less_than_4 && smallest_h_less_than_4) {
          return index == 2;
        } else if (smallest_w_less_than_4) {
          if (partition == PARTITION_VERT_A || partition == PARTITION_VERT_B) {
            return index == 2;
          } else if (partition == PARTITION_HORZ_A) {
            return index == 1 || index == 2;
          } else {
            return index == 0 || index == 2;
          }
        } else if (smallest_h_less_than_4) {
          if (partition == PARTITION_HORZ_A || partition == PARTITION_HORZ_B) {
            return index == 2;
          } else if (partition == PARTITION_VERT_A) {
            return index == 1 || index == 2;
          } else {
            return index == 0 || index == 2;
          }
        } else {
          return 1;
        }
      }
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4:
      if (is_offset_started) {
        return index == 3;
      } else {
        if ((partition == PARTITION_HORZ_4 && ph_less_than_4) ||
            (partition == PARTITION_VERT_4 && pw_less_than_4)) {
          return index == 1 || index == 3;
        } else {
          return 1;
        }
      }
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    default:
      assert(0 && "Invalid partition type!");
      return 0;
      break;
  }
}

static INLINE void set_chroma_ref_offset_size(
    int mi_row, int mi_col, PARTITION_TYPE partition, BLOCK_SIZE bsize,
    BLOCK_SIZE parent_bsize, int ss_x, int ss_y, CHROMA_REF_INFO *info,
    const CHROMA_REF_INFO *parent_info) {
  const int pw = block_size_wide[bsize] >> ss_x;
  const int ph = block_size_high[bsize] >> ss_y;
  const int pw_less_than_4 = pw < 4;
  const int ph_less_than_4 = ph < 4;
#if !CONFIG_EXT_RECUR_PARTITIONS
  const int hppw = block_size_wide[parent_bsize] >> (ss_x + 1);
  const int hpph = block_size_high[parent_bsize] >> (ss_y + 1);
  const int hppw_less_than_4 = hppw < 4;
  const int hpph_less_than_4 = hpph < 4;
  const int mi_row_mid_point =
      parent_info->mi_row_chroma_base + (mi_size_high[parent_bsize] >> 1);
  const int mi_col_mid_point =
      parent_info->mi_col_chroma_base + (mi_size_wide[parent_bsize] >> 1);
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

  assert(parent_info->offset_started == 0);

  switch (partition) {
    case PARTITION_NONE:
    case PARTITION_HORZ:
    case PARTITION_VERT:
#if CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_VERT_3:
    case PARTITION_HORZ_3:
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
      info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
      info->bsize_base = parent_bsize;
      break;
    case PARTITION_SPLIT:
      if (pw_less_than_4 && ph_less_than_4) {
        info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
        info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
        info->bsize_base = parent_bsize;
      } else if (pw_less_than_4) {
        info->bsize_base = get_partition_subsize(parent_bsize, PARTITION_HORZ);
        info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
        if (mi_row == parent_info->mi_row_chroma_base) {
          info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
        } else {
          info->mi_row_chroma_base =
              parent_info->mi_row_chroma_base + mi_size_high[bsize];
        }
      } else {
        assert(ph_less_than_4);
        info->bsize_base = get_partition_subsize(parent_bsize, PARTITION_VERT);
        info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
        if (mi_col == parent_info->mi_col_chroma_base) {
          info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
        } else {
          info->mi_col_chroma_base =
              parent_info->mi_col_chroma_base + mi_size_wide[bsize];
        }
      }
      break;
#if !CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
      if ((hppw_less_than_4 && hpph_less_than_4) ||
          (hppw_less_than_4 &&
           (partition == PARTITION_VERT_A || partition == PARTITION_VERT_B)) ||
          (hpph_less_than_4 &&
           (partition == PARTITION_HORZ_A || partition == PARTITION_HORZ_B))) {
        info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
        info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
        info->bsize_base = parent_bsize;
      } else if (hppw_less_than_4) {
        info->bsize_base = get_partition_subsize(parent_bsize, PARTITION_HORZ);
        info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
        if (mi_row == parent_info->mi_row_chroma_base) {
          info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
        } else {
          info->mi_row_chroma_base = parent_info->mi_row_chroma_base +
                                     (mi_size_high[parent_bsize] >> 1);
        }
      } else {
        assert(hpph_less_than_4);

        info->bsize_base = get_partition_subsize(parent_bsize, PARTITION_VERT);
        info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
        if (mi_col == parent_info->mi_col_chroma_base) {
          info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
        } else {
          info->mi_col_chroma_base = parent_info->mi_col_chroma_base +
                                     (mi_size_wide[parent_bsize] >> 1);
        }
      }
      break;
    case PARTITION_HORZ_4:
      info->bsize_base = get_partition_subsize(parent_bsize, PARTITION_HORZ);
      info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
      if (mi_row < mi_row_mid_point) {
        info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
      } else {
        info->mi_row_chroma_base = mi_row_mid_point;
      }
      break;
    case PARTITION_VERT_4:
      info->bsize_base = get_partition_subsize(parent_bsize, PARTITION_VERT);
      info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
      if (mi_col < mi_col_mid_point) {
        info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
      } else {
        info->mi_col_chroma_base = mi_col_mid_point;
      }
      break;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
    default: assert(0 && "Invalid partition type!"); break;
  }
}

static INLINE void set_chroma_ref_info(int mi_row, int mi_col, int index,
                                       BLOCK_SIZE bsize, CHROMA_REF_INFO *info,
                                       const CHROMA_REF_INFO *parent_info,
                                       BLOCK_SIZE parent_bsize,
                                       PARTITION_TYPE parent_partition,
                                       int ss_x, int ss_y) {
  assert(bsize < BLOCK_SIZES_ALL);
  initialize_chr_ref_info(mi_row, mi_col, bsize, info);

  if (parent_info == NULL) return;

  if (parent_info->is_chroma_ref) {
    if (parent_info->offset_started) {
      if (is_sub_partition_chroma_ref(parent_partition, index, bsize,
                                      parent_bsize, ss_x, ss_y, 1)) {
        info->is_chroma_ref = 1;
      } else {
        info->is_chroma_ref = 0;
      }
      info->offset_started = 1;
      info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
      info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
      info->bsize_base = parent_info->bsize_base;
    } else if (have_nz_chroma_ref_offset(parent_bsize, parent_partition, ss_x,
                                         ss_y)) {
      info->offset_started = 1;
      info->is_chroma_ref = is_sub_partition_chroma_ref(
          parent_partition, index, bsize, parent_bsize, ss_x, ss_y, 0);
      set_chroma_ref_offset_size(mi_row, mi_col, parent_partition, bsize,
                                 parent_bsize, ss_x, ss_y, info, parent_info);
    }
  } else {
    info->is_chroma_ref = 0;
    info->offset_started = 1;
    info->mi_row_chroma_base = parent_info->mi_row_chroma_base;
    info->mi_col_chroma_base = parent_info->mi_col_chroma_base;
    info->bsize_base = parent_info->bsize_base;
  }
}

#if CONFIG_DERIVED_INTRA_MODE
#define FUSION_MODE 0
#if FUSION_MODE
#define NUM_DERIVED_INTRA_MODES 1
#define DERIVED_INTRA_FUSION_SHIFT 7
#endif  // FUSION_MODE
#endif  // CONFIG_DERIVED_INTRA_MODE

// Macros for optical flow experiment where offsets are added in nXn blocks
// rather than adding a single offset to the entire prediction unit.
#if CONFIG_OPTFLOW_REFINEMENT
#define USE_OF_NXN 1
#if USE_OF_NXN
#define OF_MIN_BSIZE_LOG2 2
#define OF_BSIZE_LOG2 3
// Block size to use to divide up the prediction unit
#define OF_MIN_BSIZE (1 << OF_MIN_BSIZE_LOG2)
#define OF_BSIZE (1 << OF_BSIZE_LOG2)
#define N_OF_OFFSETS_1D (1 << (MAX_SB_SIZE_LOG2 - OF_MIN_BSIZE_LOG2))
// Maximum number of offsets to be computed
#define N_OF_OFFSETS (N_OF_OFFSETS_1D * N_OF_OFFSETS_1D)
#else
#define N_OF_OFFSETS 1
#endif  // USE_OF_NXN
#endif  // CONFIG_OPTFLOW_REFINEMENT

// This structure now relates to 4x4 block regions.
typedef struct MB_MODE_INFO {
  // interinter members
  INTERINTER_COMPOUND_DATA interinter_comp;
  WarpedMotionParams wm_params;
  int_mv mv[2];
  int current_qindex;
  // Only for INTER blocks
  int_interpfilters interp_filters;
  MvSubpelPrecision max_mv_precision;
  MvSubpelPrecision pb_mv_precision;
  // TODO(debargha): Consolidate these flags
#if CONFIG_RD_DEBUG
  RD_STATS rd_stats;
  int mi_row;
  int mi_col;
#endif
#if CONFIG_INSPECTION
  int16_t tx_skip[TXK_TYPE_BUF_LEN];
#endif
  PALETTE_MODE_INFO palette_mode_info;
  // Common for both INTER and INTRA blocks
  BLOCK_SIZE sb_type;
  PREDICTION_MODE mode;
  // Only for INTRA blocks
  UV_PREDICTION_MODE uv_mode;
  // interintra members
  INTERINTRA_MODE interintra_mode;
#if CONFIG_ADAPT_FILTER_INTRA
  ADAPT_FILTER_INTRA_MODE_INFO adapt_filter_intra_mode_info;
#endif
#if CONFIG_NEW_TX_PARTITION
  TX_PARTITION_TYPE partition_type[INTER_TX_SIZE_BUF_LEN];
#endif  // CONFIG_NEW_TX_PARTITION
  MOTION_MODE motion_mode;
  PARTITION_TYPE partition;
  TX_TYPE txk_type[TXK_TYPE_BUF_LEN];
  MV_REFERENCE_FRAME ref_frame[2];
  FILTER_INTRA_MODE_INFO filter_intra_mode_info;
  int8_t skip;
  uint8_t inter_tx_size[INTER_TX_SIZE_BUF_LEN];
  TX_SIZE tx_size;
  int8_t delta_lf_from_base;
  int8_t delta_lf[FRAME_LF_COUNT];
  int8_t interintra_wedge_index;
  // The actual prediction angle is the base angle + (angle_delta * step).
  int8_t angle_delta[PLANE_TYPES];
  /* deringing gain *per-superblock* */
  // Joint sign of alpha Cb and alpha Cr
  int8_t cfl_alpha_signs;
  // Index of the alpha Cb and alpha Cr combination
  uint8_t cfl_alpha_idx;
  uint8_t num_proj_ref;
  uint8_t overlappable_neighbors[2];
  // If comp_group_idx=0, indicate if dist_wtd_comp(0) or avg_comp(1) is used.
  uint8_t compound_idx;
  uint8_t use_wedge_interintra : 1;
  uint8_t segment_id : 3;
  uint8_t seg_id_predicted : 1;  // valid only when temporal_update is enabled
  uint8_t skip_mode : 1;
  uint8_t use_intrabc : 1;
#if CONFIG_EXT_IBC_MODES
  uint8_t ibc_mode : 3;
#endif  // CONFIG_EXT_IBC_MODES
  CHROMA_REF_INFO chroma_ref_info;
#if CONFIG_NEW_INTER_MODES && MAX_DRL_BITS > 3
  uint8_t ref_mv_idx : 3;
#else
  uint8_t ref_mv_idx : 2;
#endif  // CONFIG_NEW_INTER_MODES && MAX_DRL_BITS> 3
#if CONFIG_FLEX_MVRES
#if CONFIG_NEW_INTER_MODES && MAX_DRL_BITS > 3
  uint8_t ref_mv_idx_adj : 3;
#else
  uint8_t ref_mv_idx_adj : 2;
#endif  // CONFIG_NEW_INTER_MODES && MAX_DRL_BITS > 3
#endif  // CONFIG_FLEX_MVRES
  // Indicate if masked compound is used(1) or not(0).
  uint8_t comp_group_idx : 1;
  int8_t cdef_strength : 4;
#if CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL
  uint64_t y_gradient_hist[8];
  int64_t y_recon_var;  // Variance of reconstructed Y values.
#endif                  // CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL
#if CONFIG_DERIVED_INTRA_MODE
  uint8_t use_derived_intra_mode[2];
  uint8_t derived_angle;
#if FUSION_MODE
  uint8_t derived_intra_angles[NUM_DERIVED_INTRA_MODES];
  uint8_t derived_intra_weights[NUM_DERIVED_INTRA_MODES];
#endif  // FUSION_MODE
#endif  // CONFIG_DERIVED_INTRA_MODE
#if CONFIG_DERIVED_MV
  int derived_mv_allowed;
  int use_derived_mv;
  MV derived_mv[2];
#endif  // CONFIG_DERIVED_MV
#if CONFIG_OPTFLOW_REFINEMENT
  int_mv mv_refined[2 * N_OF_OFFSETS];
#endif  // CONFIG_OPTFLOW_REFINEMENT
#if CONFIG_DSPL_RESIDUAL
  // dspl_type stores the partition level downsampling decision for the
  // CONFIG_DSPL_RESIDUAL experiment
  DSPL_TYPE dspl_type;
#endif  // CONFIG_DSPL_RESIDUAL
#if CONFIG_NN_RECON
  int use_nn_recon;
#endif  // CONFIG_NN_RECON
} MB_MODE_INFO;

typedef struct PARTITION_TREE {
  struct PARTITION_TREE *parent;
  struct PARTITION_TREE *sub_tree[4];
  PARTITION_TYPE partition;
  BLOCK_SIZE bsize;
  int is_settled;
  int mi_row;
  int mi_col;
  int index;
  CHROMA_REF_INFO chroma_ref_info;
} PARTITION_TREE;

typedef struct SB_INFO {
  int mi_row;
  int mi_col;
  PARTITION_TREE *ptree_root;

  MvSubpelPrecision sb_mv_precision;
#if CONFIG_DSPL_RESIDUAL
  // allow_dspl_residual == 1 allows the encoder to pick, for each partition,
  // whether to downsample residuals before coding or not. allow_dspl_residual
  // == 0 forces each partition to pick the no downsampling option.
  int8_t allow_dspl_residual;
#endif  // CONFIG_DSPL_RESIDUAL
} SB_INFO;

PARTITION_TREE *av1_alloc_ptree_node(PARTITION_TREE *parent, int index);
void av1_free_ptree_recursive(PARTITION_TREE *ptree);
void av1_reset_ptree_in_sbi(SB_INFO *sbi);

#if CONFIG_EXT_RECUR_PARTITIONS
static INLINE PARTITION_TYPE get_partition_from_symbol_rec_block(
    BLOCK_SIZE bsize, PARTITION_TYPE_REC partition_rec) {
  if (block_size_wide[bsize] > block_size_high[bsize])
    return partition_map_from_symbol_block_wgth[partition_rec];
  else if (block_size_high[bsize] > block_size_wide[bsize])
    return partition_map_from_symbol_block_hgtw[partition_rec];
  else
    return PARTITION_INVALID;
}

static INLINE PARTITION_TYPE_REC get_symbol_from_partition_rec_block(
    BLOCK_SIZE bsize, PARTITION_TYPE partition) {
  if (block_size_wide[bsize] > block_size_high[bsize])
    return symbol_map_from_partition_block_wgth[partition];
  else if (block_size_high[bsize] > block_size_wide[bsize])
    return symbol_map_from_partition_block_hgtw[partition];
  else
    return PARTITION_INVALID_REC;
}
#endif  // CONFIG_EXT_RECUR_PARTITIONS

static INLINE int has_second_ref(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[1] > INTRA_FRAME;
}

static INLINE int is_intrabc_block(const MB_MODE_INFO *mbmi) {
  // Intrabc implies this is not a compound mode.
  assert(IMPLIES(mbmi->use_intrabc, !has_second_ref(mbmi)));
  return mbmi->use_intrabc;
}

static INLINE PREDICTION_MODE get_uv_mode(UV_PREDICTION_MODE mode) {
  assert(mode < UV_INTRA_MODES);
  static const PREDICTION_MODE uv2y[] = {
    DC_PRED,        // UV_DC_PRED
    V_PRED,         // UV_V_PRED
    H_PRED,         // UV_H_PRED
    D45_PRED,       // UV_D45_PRED
    D135_PRED,      // UV_D135_PRED
    D113_PRED,      // UV_D113_PRED
    D157_PRED,      // UV_D157_PRED
    D203_PRED,      // UV_D203_PRED
    D67_PRED,       // UV_D67_PRED
    SMOOTH_PRED,    // UV_SMOOTH_PRED
    SMOOTH_V_PRED,  // UV_SMOOTH_V_PRED
    SMOOTH_H_PRED,  // UV_SMOOTH_H_PRED
    PAETH_PRED,     // UV_PAETH_PRED
    DC_PRED,        // UV_CFL_PRED
    INTRA_INVALID,  // UV_INTRA_MODES
    INTRA_INVALID,  // UV_MODE_INVALID
  };
  return uv2y[mode];
}

static INLINE int is_inter_block(const MB_MODE_INFO *mbmi) {
  return is_intrabc_block(mbmi) || mbmi->ref_frame[0] > INTRA_FRAME;
}

static INLINE int has_uni_comp_refs(const MB_MODE_INFO *mbmi) {
  return has_second_ref(mbmi) && (!((mbmi->ref_frame[0] >= BWDREF_FRAME) ^
                                    (mbmi->ref_frame[1] >= BWDREF_FRAME)));
}

static INLINE MV_REFERENCE_FRAME comp_ref0(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST_FRAME,     // LAST_LAST2_FRAMES,
    LAST_FRAME,     // LAST_LAST3_FRAMES,
    LAST_FRAME,     // LAST_GOLDEN_FRAMES,
    BWDREF_FRAME,   // BWDREF_ALTREF_FRAMES,
    LAST2_FRAME,    // LAST2_LAST3_FRAMES
    LAST2_FRAME,    // LAST2_GOLDEN_FRAMES,
    LAST3_FRAME,    // LAST3_GOLDEN_FRAMES,
    BWDREF_FRAME,   // BWDREF_ALTREF2_FRAMES,
    ALTREF2_FRAME,  // ALTREF2_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == TOTAL_UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

static INLINE MV_REFERENCE_FRAME comp_ref1(int ref_idx) {
  static const MV_REFERENCE_FRAME lut[] = {
    LAST2_FRAME,    // LAST_LAST2_FRAMES,
    LAST3_FRAME,    // LAST_LAST3_FRAMES,
    GOLDEN_FRAME,   // LAST_GOLDEN_FRAMES,
    ALTREF_FRAME,   // BWDREF_ALTREF_FRAMES,
    LAST3_FRAME,    // LAST2_LAST3_FRAMES
    GOLDEN_FRAME,   // LAST2_GOLDEN_FRAMES,
    GOLDEN_FRAME,   // LAST3_GOLDEN_FRAMES,
    ALTREF2_FRAME,  // BWDREF_ALTREF2_FRAMES,
    ALTREF_FRAME,   // ALTREF2_ALTREF_FRAMES,
  };
  assert(NELEMENTS(lut) == TOTAL_UNIDIR_COMP_REFS);
  return lut[ref_idx];
}

PREDICTION_MODE av1_left_block_mode(const MB_MODE_INFO *left_mi);

PREDICTION_MODE av1_above_block_mode(const MB_MODE_INFO *above_mi);

static INLINE int is_global_mv_block(const MB_MODE_INFO *const mbmi,
                                     TransformationType type) {
  const PREDICTION_MODE mode = mbmi->mode;

#if CONFIG_EXT_WARP && CONFIG_SUB8X8_WARP
  return ((mode == GLOBALMV || mode == GLOBAL_GLOBALMV) && type > TRANSLATION);
#else
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int block_size_allowed =
      AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
  return (mode == GLOBALMV || mode == GLOBAL_GLOBALMV) && type > TRANSLATION &&
         block_size_allowed;
#endif  // CONFIG_EXT_WARP && CONFIG_SUB8X8_WARP
}

#if CONFIG_MISMATCH_DEBUG
static INLINE void mi_to_pixel_loc(int *pixel_c, int *pixel_r, int mi_col,
                                   int mi_row, int tx_blk_col, int tx_blk_row,
                                   int subsampling_x, int subsampling_y) {
  *pixel_c = ((mi_col >> subsampling_x) << MI_SIZE_LOG2) +
             (tx_blk_col << tx_size_wide_log2[0]);
  *pixel_r = ((mi_row >> subsampling_y) << MI_SIZE_LOG2) +
             (tx_blk_row << tx_size_high_log2[0]);
}
#endif

enum { MV_PRECISION_Q3, MV_PRECISION_Q4 } UENUM1BYTE(mv_precision);

struct buf_2d {
  uint8_t *buf;
  uint8_t *buf0;
  int width;
  int height;
  int stride;
};

typedef struct eob_info {
  uint16_t eob;
  uint16_t max_scan_line;
} eob_info;

typedef struct {
  DECLARE_ALIGNED(32, tran_low_t, dqcoeff[MAX_MB_PLANE][MAX_SB_SQUARE]);
  eob_info eob_data[MAX_MB_PLANE]
                   [MAX_SB_SQUARE / (TX_SIZE_W_MIN * TX_SIZE_H_MIN)];
  DECLARE_ALIGNED(16, uint8_t, color_index_map[2][MAX_SB_SQUARE]);
} CB_BUFFER;

typedef struct macroblockd_plane {
  tran_low_t *dqcoeff;
  tran_low_t *dqcoeff_block;
  eob_info *eob_data;
  PLANE_TYPE plane_type;
  int subsampling_x;
  int subsampling_y;
  struct buf_2d dst;
  struct buf_2d pre[2];
  ENTROPY_CONTEXT *above_context;
  ENTROPY_CONTEXT *left_context;

  // The dequantizers below are true dequantizers used only in the
  // dequantization process.  They have the same coefficient
  // shift/scale as TX.
#if CONFIG_EXTQUANT
#if CONFIG_DSPL_RESIDUAL
  int32_t seg_dequant_QTX[DSPL_END][MAX_SEGMENTS][2];
#else
  int32_t seg_dequant_QTX[MAX_SEGMENTS][2];
#endif  // CONFIG_DSPL_RESIDUAL
#else
#if CONFIG_DSPL_RESIDUAL
  int16_t seg_dequant_QTX[DSPL_END][MAX_SEGMENTS][2];
#else
  int16_t seg_dequant_QTX[MAX_SEGMENTS][2];
#endif  // CONFIG_DSPL_RESIDUAL
#endif
  uint8_t *color_index_map;

  // block size in pixels
  uint8_t width, height;

  qm_val_t *seg_iqmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
  qm_val_t *seg_qmatrix[MAX_SEGMENTS][TX_SIZES_ALL];
} MACROBLOCKD_PLANE;

#define BLOCK_OFFSET(x, i) \
  ((x) + (i) * (1 << (tx_size_wide_log2[0] + tx_size_high_log2[0])))

typedef struct {
  DECLARE_ALIGNED(16, InterpKernel, vfilter);
  DECLARE_ALIGNED(16, InterpKernel, hfilter);
} WienerInfo;

typedef struct {
  int ep;
  int xqd[2];
} SgrprojInfo;

#if CONFIG_CNN_CRLC_GUIDED
typedef struct {
  int xqd[2];
} CRLCUnitInfo;
#endif  // CONFIG_CNN_CRLC_GUIDED

#if CONFIG_LOOP_RESTORE_CNN
typedef struct {
  FRAME_TYPE frame_type;
  int base_qindex;
  bool is_luma;
} CNNInfo;
#endif  // CONFIG_LOOP_RESTORE_CNN

#if CONFIG_WIENER_NONSEP
#define WIENERNS_YUV_MAX 24
typedef struct {
  DECLARE_ALIGNED(16, int16_t, nsfilter[WIENERNS_YUV_MAX]);
} WienerNonsepInfo;
#endif  // CONFIG_WIENER_NONSEP

#if CONFIG_DEBUG
#define CFL_SUB8X8_VAL_MI_SIZE (4)
#define CFL_SUB8X8_VAL_MI_SQUARE \
  (CFL_SUB8X8_VAL_MI_SIZE * CFL_SUB8X8_VAL_MI_SIZE)
#endif  // CONFIG_DEBUG
#define CFL_MAX_BLOCK_SIZE (BLOCK_32X32)
#define CFL_BUF_LINE (32)
#define CFL_BUF_LINE_I128 (CFL_BUF_LINE >> 3)
#define CFL_BUF_LINE_I256 (CFL_BUF_LINE >> 4)
#define CFL_BUF_SQUARE (CFL_BUF_LINE * CFL_BUF_LINE)
typedef struct cfl_ctx {
  // Q3 reconstructed luma pixels (only Q2 is required, but Q3 is used to avoid
  // shifts)
  uint16_t recon_buf_q3[CFL_BUF_SQUARE];
  // Q3 AC contributions (reconstructed luma pixels - tx block avg)
  int16_t ac_buf_q3[CFL_BUF_SQUARE];

  // Cache the DC_PRED when performing RDO, so it does not have to be recomputed
  // for every scaling parameter
  int dc_pred_is_cached[CFL_PRED_PLANES];
  // The DC_PRED cache is disable when decoding
  int use_dc_pred_cache;
  // Only cache the first row of the DC_PRED
  int16_t dc_pred_cache[CFL_PRED_PLANES][CFL_BUF_LINE];

  // Height and width currently used in the CfL prediction buffer.
  int buf_height, buf_width;

  int are_parameters_computed;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  int mi_row, mi_col;

  // Whether the reconstructed luma pixels need to be stored
  int store_y;

#if CONFIG_DEBUG
  int rate;
#endif  // CONFIG_DEBUG

  int is_chroma_reference;
} CFL_CTX;

typedef struct dist_wtd_comp_params {
  int use_dist_wtd_comp_avg;
  int fwd_offset;
  int bck_offset;
} DIST_WTD_COMP_PARAMS;

struct scale_factors;

#if CONFIG_EXT_REFMV || CONFIG_ENHANCED_WARPED_MOTION
#define MAX_REF_LOC_STACK_SIZE (MAX_REF_MV_STACK_SIZE << 1)

typedef struct location_info {
  int16_t x;
  int16_t y;
  int_mv this_mv;  // Only used for single frame prediction
} LOCATION_INFO;
#endif  // CONFIG_EXT_REFMV || CONFIG_ENHANCED_WARPED_MOTION

typedef struct {
  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  uint16_t ref_mv_weight[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
#if CONFIG_FLEX_MVRES
  uint8_t ref_mv_count_adj;
  CANDIDATE_MV ref_mv_stack_adj[MAX_REF_MV_STACK_SIZE];
  uint16_t ref_mv_weight_adj[MAX_REF_MV_STACK_SIZE];
#endif  // CONFIG_FLEX_MVRES
#if CONFIG_EXT_REFMV || CONFIG_ENHANCED_WARPED_MOTION
  LOCATION_INFO ref_mv_location_stack[MODE_CTX_REF_FRAMES]
                                     [MAX_REF_LOC_STACK_SIZE];
  uint8_t ref_mv_location_count[MODE_CTX_REF_FRAMES];
#endif  // CONFIG_EXT_REFMV || CONFIG_ENHANCED_WARPED_MOTION
} REF_MV_INFO;

#if CONFIG_REF_MV_BANK
#define REF_MV_BANK_SIZE 8
typedef struct {
  int rmb_queue_count[MODE_CTX_REF_FRAMES];
  int rmb_queue_start_idx[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV rmb_queue_buffer[MODE_CTX_REF_FRAMES][REF_MV_BANK_SIZE];
} REF_MV_BANK;
#endif  // CONFIG_REF_MV_BANK

// Most/all of the pointers are mere pointers to actual arrays are allocated
// elsewhere. This is mostly for coding convenience.
typedef struct macroblockd {
  struct macroblockd_plane plane[MAX_MB_PLANE];

  TileInfo tile;

  int mi_stride;

  MB_MODE_INFO **mi;
  MB_MODE_INFO *left_mbmi;
  MB_MODE_INFO *above_mbmi;
#if CONFIG_INTRA_ENTROPY
  MB_MODE_INFO *aboveleft_mbmi;
#endif  // CONFIG_INTRA_ENTROPY
  MB_MODE_INFO *chroma_left_mbmi;
  MB_MODE_INFO *chroma_above_mbmi;

  SB_INFO *sbi;

  int up_available;
  int left_available;
  int chroma_up_available;
  int chroma_left_available;

  /* Distance of MB away from frame edges in subpixels (1/8th pixel)  */
  int mb_to_left_edge;
  int mb_to_right_edge;
  int mb_to_top_edge;
  int mb_to_bottom_edge;

  /* An array for recording whether an mi(4x4) is coded. Reset at sb level */
  uint8_t is_mi_coded[1024];
  int is_mi_coded_stride;

  int mi_row;
  int mi_col;

  /* pointers to reference frame scale factors */
  const struct scale_factors *block_ref_scale_factors[2];

  /* pointer to current frame */
  const YV12_BUFFER_CONFIG *cur_buf;

#if CONFIG_EXT_IBC_MODES
  uint16_t *ibc_pred;
#endif  // CONFIG_EXT_IBC_MODES

  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][MAX_MIB_SIZE];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[MAX_MIB_SIZE];

  TXFM_CONTEXT *above_txfm_context;
  TXFM_CONTEXT *left_txfm_context;
  TXFM_CONTEXT left_txfm_context_buffer[MAX_MIB_SIZE];

  WienerInfo wiener_info[MAX_MB_PLANE];
  SgrprojInfo sgrproj_info[MAX_MB_PLANE];

#if CONFIG_CNN_CRLC_GUIDED
  CRLCUnitInfo crlc_unitinfo[MAX_MB_PLANE];
#endif  // CONFIG_CNN_CRLC_GUIDED
#if CONFIG_WIENER_NONSEP
  WienerNonsepInfo wiener_nonsep_info[MAX_MB_PLANE];
#endif  // CONFIG_WIENER_NONSEP

  // block dimension in the unit of mode_info.
  uint8_t n4_w, n4_h;

  REF_MV_INFO ref_mv_info;

#if CONFIG_REF_MV_BANK
  REF_MV_BANK ref_mv_bank_left;
  // TODO(huisu): 32 is enough to support frame width up to 32 * 64 = 2048.
  // Ideally this should be allocated dynamically based on frame size.
  REF_MV_BANK ref_mv_bank_above[32];
  REF_MV_BANK *ref_mv_bank_left_pt;
  REF_MV_BANK *ref_mv_bank_above_pt;
#endif  // CONFIG_REF_MV_BANK

  uint8_t is_sec_rect;

  // Counts of each reference frame in the above and left neighboring blocks.
  // NOTE: Take into account both single and comp references.
  uint8_t neighbors_ref_counts[REF_FRAMES];

  FRAME_CONTEXT *tile_ctx;
  /* Bit depth: 8, 10, 12 */
  int bd;

  int qindex[MAX_SEGMENTS];
  int lossless[MAX_SEGMENTS];
  int corrupted;
  int cur_frame_force_integer_mv;
  // same with that in AV1_COMMON
  struct aom_internal_error_info *error_info;
  const WarpedMotionParams *global_motion;
  int delta_qindex;
  int current_qindex;
  // Since actual frame level loop filtering level value is not available
  // at the beginning of the tile (only available during actual filtering)
  // at encoder side.we record the delta_lf (against the frame level loop
  // filtering level) and code the delta between previous superblock's delta
  // lf and current delta lf. It is equivalent to the delta between previous
  // superblock's actual lf and current lf.
  int8_t delta_lf_from_base;
  // For this experiment, we have four frame filter levels for different plane
  // and direction. So, to support the per superblock update, we need to add
  // a few more params as below.
  // 0: delta loop filter level for y plane vertical
  // 1: delta loop filter level for y plane horizontal
  // 2: delta loop filter level for u plane
  // 3: delta loop filter level for v plane
  // To make it consistent with the reference to each filter level in segment,
  // we need to -1, since
  // SEG_LVL_ALT_LF_Y_V = 1;
  // SEG_LVL_ALT_LF_Y_H = 2;
  // SEG_LVL_ALT_LF_U   = 3;
  // SEG_LVL_ALT_LF_V   = 4;
  int8_t delta_lf[FRAME_LF_COUNT];
  int cdef_preset[4];

  DECLARE_ALIGNED(16, uint8_t, seg_mask[2 * MAX_SB_SQUARE]);
  uint8_t *mc_buf[2];
  CFL_CTX cfl;

  DIST_WTD_COMP_PARAMS jcp_param;

  uint16_t cb_offset[MAX_MB_PLANE];
  uint16_t txb_offset[MAX_MB_PLANE];
  uint16_t color_index_map_offset[2];

  CONV_BUF_TYPE *tmp_conv_dst;
  uint8_t *tmp_obmc_bufs[2];
} MACROBLOCKD;

static INLINE int is_cur_buf_hbd(const MACROBLOCKD *xd) {
  return xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH ? 1 : 0;
}

static INLINE uint8_t *get_buf_by_bd(const MACROBLOCKD *xd, uint8_t *buf16) {
  return (xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH)
             ? CONVERT_TO_BYTEPTR(buf16)
             : buf16;
}

static TX_TYPE intra_mode_to_tx_type(const MB_MODE_INFO *mbmi,
                                     PLANE_TYPE plane_type) {
  static const TX_TYPE _intra_mode_to_tx_type[INTRA_MODES] = {
    DCT_DCT,    // DC_PRED
    ADST_DCT,   // V_PRED
    DCT_ADST,   // H_PRED
    DCT_DCT,    // D45_PRED
    ADST_ADST,  // D135_PRED
    ADST_DCT,   // D113_PRED
    DCT_ADST,   // D157_PRED
    DCT_ADST,   // D203_PRED
    ADST_DCT,   // D67_PRED
    ADST_ADST,  // SMOOTH_PRED
    ADST_DCT,   // SMOOTH_V_PRED
    DCT_ADST,   // SMOOTH_H_PRED
    ADST_ADST,  // PAETH_PRED
  };
  const PREDICTION_MODE mode =
      (plane_type == PLANE_TYPE_Y) ? mbmi->mode : get_uv_mode(mbmi->uv_mode);
  assert(mode < INTRA_MODES);
  return _intra_mode_to_tx_type[mode];
}

static INLINE int is_rect_tx(TX_SIZE tx_size) { return tx_size >= TX_SIZES; }

static INLINE int block_signals_txsize(BLOCK_SIZE bsize) {
  return bsize > BLOCK_4X4;
}

// Number of transform types in each set type
static const int av1_num_ext_tx_set[EXT_TX_SET_TYPES] = {
  1, 2, 5, 7, 12, 16,
};

#if CONFIG_MODE_DEP_INTRA_TX && CONFIG_MODE_DEP_INTER_TX
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
#if CONFIG_MODE_DEP_NONSEP_INTRA_TX
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
    0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
#else
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0,
    0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
#endif  // CONFIG_MODE_DEP_NONSEP_INTRA_TX
};
#elif CONFIG_MODE_DEP_INTRA_TX
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
#if CONFIG_MODE_DEP_NONSEP_INTRA_TX
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
#else
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0 },
#endif  // CONFIG_MODE_DEP_NONSEP_INTRA_TX
};
#elif CONFIG_MODE_DEP_INTER_TX
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};
#else
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};
#endif  // CONFIG_MODE_DEP_INTRA_TX && CONFIG_MODE_DEP_INTER_TX

static const uint16_t av1_reduced_intra_tx_used_flag[INTRA_MODES] = {
  0x080F,  // DC_PRED:       0000 1000 0000 1111
  0x040F,  // V_PRED:        0000 0100 0000 1111
  0x080F,  // H_PRED:        0000 1000 0000 1111
  0x020F,  // D45_PRED:      0000 0010 0000 1111
  0x080F,  // D135_PRED:     0000 1000 0000 1111
  0x040F,  // D113_PRED:     0000 0100 0000 1111
  0x080F,  // D157_PRED:     0000 1000 0000 1111
  0x080F,  // D203_PRED:     0000 1000 0000 1111
  0x040F,  // D67_PRED:      0000 0100 0000 1111
  0x080F,  // SMOOTH_PRED:   0000 1000 0000 1111
  0x040F,  // SMOOTH_V_PRED: 0000 0100 0000 1111
  0x080F,  // SMOOTH_H_PRED: 0000 1000 0000 1111
  0x0C0E,  // PAETH_PRED:    0000 1100 0000 1110
};

static const uint16_t av1_ext_tx_used_flag[EXT_TX_SET_TYPES] = {
  0x0001,  // 0000 0000 0000 0001
  0x0201,  // 0000 0010 0000 0001
  0x020F,  // 0000 0010 0000 1111
  0x0E0F,  // 0000 1110 0000 1111
  0x0FFF,  // 0000 1111 1111 1111
  0xFFFF,  // 1111 1111 1111 1111
};

#if CONFIG_MODE_DEP_INTRA_TX && CONFIG_MODE_DEP_NONSEP_INTRA_TX
static INLINE int use_nstx(TX_TYPE tx_type, TX_SIZE tx_size,
                           PREDICTION_MODE mode) {
  (void)mode;
  if (tx_type != MDTX_INTRA_4) return 0;
  int is_valid_nstx_size = tx_size == TX_4X4;
#if !CONFIG_MODE_DEP_NONSEP_SEC_INTRA_TX
  is_valid_nstx_size |=
      (tx_size == TX_8X8 || tx_size == TX_4X8 || tx_size == TX_8X4);
#endif  // !CONFIG_MODE_DEP_NONSEP_SEC_INTRA_TX
  return is_valid_nstx_size;
}

static INLINE int use_nsst(TX_TYPE tx_type, TX_SIZE tx_size,
                           PREDICTION_MODE mode) {
  (void)mode;
  if (tx_type != MDTX_INTRA_4) return 0;
  return ((tx_size == TX_4X8) | (tx_size == TX_8X4) | (tx_size == TX_8X8));
}
#endif

#if CONFIG_DST_32X32
static INLINE TxSetType av1_get_ext_tx_set_type(TX_SIZE tx_size, int is_inter,
                                                int use_reduced_set) {
  const TX_SIZE tx_size_sqr_up = txsize_sqr_up_map[tx_size];
  if (tx_size_sqr_up >= TX_64X64) return EXT_TX_SET_DCTONLY;
  if (use_reduced_set)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DTT4_IDTX;
  const TX_SIZE tx_size_sqr = txsize_sqr_map[tx_size];
  if (is_inter) {
    return (((tx_size_sqr == TX_16X16) || (tx_size_sqr == TX_32X32))
                ? EXT_TX_SET_DTT9_IDTX_1DDCT
#if CONFIG_MODE_DEP_INTER_TX
                : EXT_TX_SET_ALL16_MDTX8);
#else
                : EXT_TX_SET_ALL16);
#endif
  } else {
    return (((tx_size_sqr == TX_16X16) || (tx_size_sqr == TX_32X32))
                ? EXT_TX_SET_DTT4_IDTX
#if CONFIG_MODE_DEP_INTRA_TX
                : EXT_TX_SET_DTT4_IDTX_1DDCT_MDTX4);
#else
                : EXT_TX_SET_DTT4_IDTX_1DDCT);
#endif
  }
}
#else
static INLINE TxSetType av1_get_ext_tx_set_type(TX_SIZE tx_size, int is_inter,
                                                int use_reduced_set) {
  const TX_SIZE tx_size_sqr_up = txsize_sqr_up_map[tx_size];
  if (tx_size_sqr_up > TX_32X32) return EXT_TX_SET_DCTONLY;
  if (tx_size_sqr_up == TX_32X32)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DCTONLY;
  if (use_reduced_set)
    return is_inter ? EXT_TX_SET_DCT_IDTX : EXT_TX_SET_DTT4_IDTX;
  const TX_SIZE tx_size_sqr = txsize_sqr_map[tx_size];
  if (is_inter) {
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT9_IDTX_1DDCT
#if CONFIG_MODE_DEP_INTER_TX
                                    : EXT_TX_SET_ALL16_MDTX8);
#else
                                    : EXT_TX_SET_ALL16);
#endif  // CONFIG_MODE_DEP_INTER_TX
  } else {
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT4_IDTX
#if CONFIG_MODE_DEP_INTRA_TX
                                    : EXT_TX_SET_DTT4_IDTX_1DDCT_MDTX4);
#else
                                    : EXT_TX_SET_DTT4_IDTX_1DDCT);
#endif  // CONFIG_MODE_DEP_INTRA_TX
  }
}
#endif  // CONFIG_DST_32X32
// Maps tx set types to the indices.
static const int ext_tx_set_index[2][EXT_TX_SET_TYPES] = {
  { // Intra
    0, -1, 2, 1, -1, -1 },
  { // Inter
    0, 3, -1, -1, 2, 1 },
};

static INLINE int get_ext_tx_set(TX_SIZE tx_size, int is_inter,
                                 int use_reduced_set) {
  const TxSetType set_type =
      av1_get_ext_tx_set_type(tx_size, is_inter, use_reduced_set);
  return ext_tx_set_index[is_inter][set_type];
}

static INLINE int get_ext_tx_types(TX_SIZE tx_size, int is_inter,
                                   int use_reduced_set) {
  const int set_type =
      av1_get_ext_tx_set_type(tx_size, is_inter, use_reduced_set);
  return av1_num_ext_tx_set[set_type];
}

#define TXSIZEMAX(t1, t2) (tx_size_2d[(t1)] >= tx_size_2d[(t2)] ? (t1) : (t2))
#define TXSIZEMIN(t1, t2) (tx_size_2d[(t1)] <= tx_size_2d[(t2)] ? (t1) : (t2))

static INLINE TX_SIZE tx_size_from_tx_mode(BLOCK_SIZE bsize, TX_MODE tx_mode) {
  const TX_SIZE largest_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  const TX_SIZE max_rect_tx_size = max_txsize_rect_lookup[bsize];
  if (bsize == BLOCK_4X4)
    return AOMMIN(max_txsize_lookup[bsize], largest_tx_size);
  if (txsize_sqr_map[max_rect_tx_size] <= largest_tx_size)
    return max_rect_tx_size;
  else
    return largest_tx_size;
}

static const uint8_t mode_to_angle_map[] = {
  0, 90, 180, 45, 135, 113, 157, 203, 67, 0, 0, 0, 0,
};

// Converts block_index for given transform size to index of the block in raster
// order.
static INLINE int av1_block_index_to_raster_order(TX_SIZE tx_size,
                                                  int block_idx) {
  // For transform size 4x8, the possible block_idx values are 0 & 2, because
  // block_idx values are incremented in steps of size 'tx_width_unit x
  // tx_height_unit'. But, for this transform size, block_idx = 2 corresponds to
  // block number 1 in raster order, inside an 8x8 MI block.
  // For any other transform size, the two indices are equivalent.
  return (tx_size == TX_4X8 && block_idx == 2) ? 1 : block_idx;
}

// Inverse of above function.
// Note: only implemented for transform sizes 4x4, 4x8 and 8x4 right now.
static INLINE int av1_raster_order_to_block_index(TX_SIZE tx_size,
                                                  int raster_order) {
  assert(tx_size == TX_4X4 || tx_size == TX_4X8 || tx_size == TX_8X4);
  // We ensure that block indices are 0 & 2 if tx size is 4x8 or 8x4.
  return (tx_size == TX_4X4) ? raster_order : (raster_order > 0) ? 2 : 0;
}

static INLINE TX_TYPE get_default_tx_type(PLANE_TYPE plane_type,
                                          const MACROBLOCKD *xd,
                                          TX_SIZE tx_size,
                                          int is_screen_content_type) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];

  if (is_inter_block(mbmi) || plane_type != PLANE_TYPE_Y ||
      xd->lossless[mbmi->segment_id] || tx_size >= TX_32X32 ||
      is_screen_content_type)
    return DCT_DCT;

  return intra_mode_to_tx_type(mbmi, plane_type);
}

// Implements the get_plane_residual_size() function in the spec (Section
// 5.11.38. Get plane residual size function).
static INLINE BLOCK_SIZE get_plane_block_size(BLOCK_SIZE bsize,
                                              int subsampling_x,
                                              int subsampling_y) {
  if (bsize == BLOCK_INVALID) return BLOCK_INVALID;
  assert(subsampling_x >= 0 && subsampling_x < 2);
  assert(subsampling_y >= 0 && subsampling_y < 2);
  return ss_size_lookup[bsize][subsampling_x][subsampling_y];
}

static INLINE int av1_get_txb_size_index(BLOCK_SIZE bsize, int blk_row,
                                         int blk_col) {
  assert(bsize < BLOCK_SIZES_ALL);
  TX_SIZE txs = max_txsize_rect_lookup[bsize];
  for (int level = 0; level < MAX_VARTX_DEPTH - 1; ++level)
    txs = sub_tx_size_map[txs];
  const int tx_w_log2 = tx_size_wide_log2[txs] - MI_SIZE_LOG2;
  const int tx_h_log2 = tx_size_high_log2[txs] - MI_SIZE_LOG2;
  const int bw_log2 = mi_size_wide_log2[bsize];
  const int stride_log2 = bw_log2 - tx_w_log2;
  const int index =
      ((blk_row >> tx_h_log2) << stride_log2) + (blk_col >> tx_w_log2);
  assert(index < INTER_TX_SIZE_BUF_LEN);
  return index;
}

static INLINE int av1_get_txk_type_index(BLOCK_SIZE bsize, int blk_row,
                                         int blk_col) {
  assert(bsize < BLOCK_SIZES_ALL);
  TX_SIZE txs = max_txsize_rect_lookup[bsize];
#if CONFIG_NEW_TX_PARTITION
  // Get smallest possible sub_tx size
  txs = smallest_sub_tx_size_map[txs];
#else
  for (int level = 0; level < MAX_VARTX_DEPTH; ++level)
    txs = sub_tx_size_map[txs];
#endif
  const int tx_w_log2 = tx_size_wide_log2[txs] - MI_SIZE_LOG2;
  const int tx_h_log2 = tx_size_high_log2[txs] - MI_SIZE_LOG2;
  const int bw_uint_log2 = mi_size_wide_log2[bsize];
  const int stride_log2 = bw_uint_log2 - tx_w_log2;
  const int index =
      ((blk_row >> tx_h_log2) << stride_log2) + (blk_col >> tx_w_log2);
  assert(index < TXK_TYPE_BUF_LEN);
  return index;
}

static INLINE void update_txk_array(TX_TYPE *txk_type, BLOCK_SIZE bsize,
                                    int blk_row, int blk_col, TX_SIZE tx_size,
                                    TX_TYPE tx_type) {
  const int txk_type_idx = av1_get_txk_type_index(bsize, blk_row, blk_col);
  txk_type[txk_type_idx] = tx_type;

  const int txw = tx_size_wide_unit[tx_size];
  const int txh = tx_size_high_unit[tx_size];
  // The 16x16 unit is due to the constraint from tx_64x64 which sets the
  // maximum tx size for chroma as 32x32. Coupled with 4x1 transform block
  // size, the constraint takes effect in 32x16 / 16x32 size too. To solve
  // the intricacy, cover all the 16x16 units inside a 64 level transform.
  if (txw == tx_size_wide_unit[TX_64X64] ||
      txh == tx_size_high_unit[TX_64X64]) {
    const int tx_unit = tx_size_wide_unit[TX_16X16];
    for (int idy = 0; idy < txh; idy += tx_unit) {
      for (int idx = 0; idx < txw; idx += tx_unit) {
        const int this_index =
            av1_get_txk_type_index(bsize, blk_row + idy, blk_col + idx);
        txk_type[this_index] = tx_type;
      }
    }
  }
}

static INLINE TX_TYPE av1_get_tx_type(PLANE_TYPE plane_type,
                                      const MACROBLOCKD *xd, int blk_row,
                                      int blk_col, TX_SIZE tx_size,
                                      int reduced_tx_set) {
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const struct macroblockd_plane *const pd = &xd->plane[plane_type];
  const TxSetType tx_set_type =
      av1_get_ext_tx_set_type(tx_size, is_inter_block(mbmi), reduced_tx_set);

  TX_TYPE tx_type;
  if (xd->lossless[mbmi->segment_id] || txsize_sqr_up_map[tx_size] > TX_32X32) {
    tx_type = DCT_DCT;
  } else {
    if (plane_type == PLANE_TYPE_Y) {
      const int txk_type_idx =
          av1_get_txk_type_index(mbmi->sb_type, blk_row, blk_col);
      tx_type = mbmi->txk_type[txk_type_idx];
    } else if (is_inter_block(mbmi)) {
      // scale back to y plane's coordinate
      blk_row <<= pd->subsampling_y;
      blk_col <<= pd->subsampling_x;
      const int txk_type_idx =
          av1_get_txk_type_index(mbmi->sb_type, blk_row, blk_col);
      tx_type = mbmi->txk_type[txk_type_idx];
    } else {
      // In intra mode, uv planes don't share the same prediction mode as y
      // plane, so the tx_type should not be shared
      tx_type = intra_mode_to_tx_type(mbmi, PLANE_TYPE_UV);
    }
  }
  assert(tx_type < TX_TYPES);
  if (!av1_ext_tx_used[tx_set_type][tx_type]) return DCT_DCT;
  return tx_type;
}

void av1_setup_block_planes(MACROBLOCKD *xd, int ss_x, int ss_y,
                            const int num_planes);

static INLINE int bsize_to_max_depth(BLOCK_SIZE bsize) {
  TX_SIZE tx_size = max_txsize_rect_lookup[bsize];
  int depth = 0;
  while (depth < MAX_TX_DEPTH && tx_size != TX_4X4) {
    depth++;
    tx_size = sub_tx_size_map[tx_size];
  }
  return depth;
}

static INLINE int bsize_to_tx_size_cat(BLOCK_SIZE bsize) {
  TX_SIZE tx_size = max_txsize_rect_lookup[bsize];
  assert(tx_size != TX_4X4);
  int depth = 0;
  while (tx_size != TX_4X4) {
    depth++;
    tx_size = sub_tx_size_map[tx_size];
    assert(depth < 10);
  }
  assert(depth <= MAX_TX_CATS);
  return depth - 1;
}

static INLINE TX_SIZE depth_to_tx_size(int depth, BLOCK_SIZE bsize) {
  TX_SIZE max_tx_size = max_txsize_rect_lookup[bsize];
  TX_SIZE tx_size = max_tx_size;
  for (int d = 0; d < depth; ++d) tx_size = sub_tx_size_map[tx_size];
  return tx_size;
}

static INLINE TX_SIZE av1_get_adjusted_tx_size(TX_SIZE tx_size) {
  switch (tx_size) {
    case TX_64X64:
    case TX_64X32:
    case TX_32X64: return TX_32X32;
    case TX_64X16: return TX_32X16;
    case TX_16X64: return TX_16X32;
#if CONFIG_FLEX_PARTITION
    case TX_64X8: return TX_32X8;
    case TX_8X64: return TX_8X32;
    case TX_64X4: return TX_32X4;
    case TX_4X64: return TX_4X32;
#endif  // CONFIG_FLEX_PARTITION
    default: return tx_size;
  }
}

static INLINE TX_SIZE av1_get_max_uv_txsize(BLOCK_SIZE bsize, int subsampling_x,
                                            int subsampling_y) {
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(bsize, subsampling_x, subsampling_y);
  assert(plane_bsize < BLOCK_SIZES_ALL);
  const TX_SIZE uv_tx = max_txsize_rect_lookup[plane_bsize];
  return av1_get_adjusted_tx_size(uv_tx);
}

static INLINE TX_SIZE av1_get_tx_size(int plane, const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = xd->mi[0];
  if (xd->lossless[mbmi->segment_id]) return TX_4X4;
  if (plane == 0) return mbmi->tx_size;
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  const BLOCK_SIZE bsize =
      plane ? mbmi->chroma_ref_info.bsize_base : mbmi->sb_type;

  return av1_get_max_uv_txsize(bsize, pd->subsampling_x, pd->subsampling_y);
}

void av1_reset_skip_context(MACROBLOCKD *xd, BLOCK_SIZE bsize,
                            const int num_planes);

void av1_reset_loop_filter_delta(MACROBLOCKD *xd, int num_planes);

void av1_reset_loop_restoration(MACROBLOCKD *xd, const int num_planes);

typedef void (*foreach_transformed_block_visitor)(int plane, int block,
                                                  int blk_row, int blk_col,
                                                  BLOCK_SIZE plane_bsize,
                                                  TX_SIZE tx_size, void *arg);

void av1_set_contexts(const MACROBLOCKD *xd, struct macroblockd_plane *pd,
                      int plane, BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                      int has_eob, int aoff, int loff);

#define MAX_INTERINTRA_SB_SQUARE 32 * 32
static INLINE int is_interintra_mode(const MB_MODE_INFO *mbmi) {
  return (mbmi->ref_frame[0] > INTRA_FRAME &&
          mbmi->ref_frame[1] == INTRA_FRAME);
}

static INLINE int is_interintra_allowed_bsize(const BLOCK_SIZE bsize) {
  return (bsize >= BLOCK_8X8) && (bsize <= BLOCK_32X32);
}

static INLINE int is_interintra_allowed_mode(const PREDICTION_MODE mode) {
  return (mode >= SINGLE_INTER_MODE_START) && (mode < SINGLE_INTER_MODE_END);
}

static INLINE int is_interintra_allowed_ref(const MV_REFERENCE_FRAME rf[2]) {
  return (rf[0] > INTRA_FRAME) && (rf[1] <= INTRA_FRAME);
}

static INLINE int is_interintra_allowed(const MB_MODE_INFO *mbmi) {
  return is_interintra_allowed_bsize(mbmi->sb_type) &&
         is_interintra_allowed_mode(mbmi->mode) &&
         is_interintra_allowed_ref(mbmi->ref_frame);
}

static INLINE int is_interintra_allowed_bsize_group(int group) {
  int i;
  for (i = 0; i < BLOCK_SIZES_ALL; i++) {
    if (size_group_lookup[i] == group &&
        is_interintra_allowed_bsize((BLOCK_SIZE)i)) {
      return 1;
    }
  }
  return 0;
}

static INLINE int is_interintra_pred(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[0] > INTRA_FRAME &&
         mbmi->ref_frame[1] == INTRA_FRAME && is_interintra_allowed(mbmi);
}

static INLINE int get_vartx_max_txsize(const MACROBLOCKD *xd, BLOCK_SIZE bsize,
                                       int plane) {
  if (xd->lossless[xd->mi[0]->segment_id]) return TX_4X4;
  assert(bsize < BLOCK_SIZES_ALL);
  const TX_SIZE max_txsize = max_txsize_rect_lookup[bsize];
  if (plane == 0) return max_txsize;            // luma
  return av1_get_adjusted_tx_size(max_txsize);  // chroma
}

static INLINE int is_motion_variation_allowed_bsize(BLOCK_SIZE bsize,
                                                    int mi_row, int mi_col) {
  assert(bsize < BLOCK_SIZES_ALL);
  if (AOMMIN(block_size_wide[bsize], block_size_high[bsize]) < 8) {
    return 0;
  }
#if CONFIG_EXT_RECUR_PARTITIONS
  // TODO(urvang): Enable this special case, if we make OBMC work.
  // TODO(yuec): Enable this case when the alignment issue is fixed. There
  // will be memory leak in global above_pred_buff and left_pred_buff if
  // the restriction on mi_row and mi_col is removed.
  if ((mi_row & 0x01) || (mi_col & 0x01)) {
    return 0;
  }
#else
  assert(!(mi_row & 0x01) && !(mi_col & 0x01));
  (void)mi_row;
  (void)mi_col;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
  return 1;
}

static INLINE int is_motion_variation_allowed_compound(
    const MB_MODE_INFO *mbmi) {
  if (!has_second_ref(mbmi))
    return 1;
  else
    return 0;
}

// input: log2 of length, 0(4), 1(8), ...
static const int max_neighbor_obmc[6] = { 0, 1, 2, 3, 4, 4 };

static INLINE int check_num_overlappable_neighbors(const MB_MODE_INFO *mbmi) {
  return !(mbmi->overlappable_neighbors[0] == 0 &&
           mbmi->overlappable_neighbors[1] == 0);
}

static INLINE MOTION_MODE_SET av1_get_allowed_motion_mode_set(
    const WarpedMotionParams *gm_params, const MACROBLOCKD *xd,
    const MB_MODE_INFO *mbmi, int allow_warped_motion) {
  if (xd->cur_frame_force_integer_mv == 0) {
    const TransformationType gm_type = gm_params[mbmi->ref_frame[0]].wmtype;
    if (is_global_mv_block(mbmi, gm_type)) return ONLY_SIMPLE_TRANSLATION;
  }
  if (!is_inter_mode(mbmi->mode) || mbmi->ref_frame[1] == INTRA_FRAME ||
      !is_motion_variation_allowed_compound(mbmi) ||
      !check_num_overlappable_neighbors(mbmi)) {
    return ONLY_SIMPLE_TRANSLATION;
  }
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const int is_motion_variation_allowed =
      is_motion_variation_allowed_bsize(mbmi->sb_type, mi_row, mi_col);
  const int allow_obmc = is_motion_variation_allowed;
  int allow_warp = allow_warped_motion && mbmi->num_proj_ref >= 1;
#if !CONFIG_EXT_WARP && CONFIG_SUB8X8_WARP
  allow_warp = allow_warp && is_motion_variation_allowed;
#endif  // CONFIG_EXT_WARP && CONFIG_SUB8X8_WARP
  allow_warp = allow_warp && !av1_is_scaled(xd->block_ref_scale_factors[0]) &&
               !xd->cur_frame_force_integer_mv;
  return (allow_warp << 1) | allow_obmc;
}

static INLINE void assert_motion_mode_valid(MOTION_MODE mode,
                                            const WarpedMotionParams *gm_params,
                                            const MACROBLOCKD *xd,
                                            const MB_MODE_INFO *mbmi,
                                            int allow_warped_motion) {
  if (mode == SIMPLE_TRANSLATION) return;

  const MOTION_MODE_SET mode_set =
      av1_get_allowed_motion_mode_set(gm_params, xd, mbmi, allow_warped_motion);
  (void)mode_set;
  assert((1 << (mode - 1)) & mode_set);
}

static INLINE int is_neighbor_overlappable(const MB_MODE_INFO *mbmi) {
  return (is_inter_block(mbmi));
}

static INLINE int av1_allow_palette(int allow_screen_content_tools,
                                    BLOCK_SIZE sb_type) {
  assert(sb_type < BLOCK_SIZES_ALL);
  return allow_screen_content_tools && block_size_wide[sb_type] <= 64 &&
         block_size_high[sb_type] <= 64 && sb_type >= BLOCK_8X8;
}

// Returns sub-sampled dimensions of the given block.
// The output values for 'rows_within_bounds' and 'cols_within_bounds' will
// differ from 'height' and 'width' when part of the block is outside the
// right
// and/or bottom image boundary.
static INLINE void av1_get_block_dimensions(BLOCK_SIZE bsize, int plane,
                                            const MACROBLOCKD *xd, int *width,
                                            int *height,
                                            int *rows_within_bounds,
                                            int *cols_within_bounds) {
  const struct macroblockd_plane *const pd = &xd->plane[plane];
  bsize = plane ? xd->mi[0]->chroma_ref_info.bsize_base : bsize;
  const int block_height = block_size_high[bsize];
  const int block_width = block_size_wide[bsize];
  const int block_rows = (xd->mb_to_bottom_edge >= 0)
                             ? block_height
                             : (xd->mb_to_bottom_edge >> 3) + block_height;
  const int block_cols = (xd->mb_to_right_edge >= 0)
                             ? block_width
                             : (xd->mb_to_right_edge >> 3) + block_width;
  assert(IMPLIES(plane == PLANE_TYPE_Y, pd->subsampling_x == 0));
  assert(IMPLIES(plane == PLANE_TYPE_Y, pd->subsampling_y == 0));
  assert(block_width >= block_cols);
  assert(block_height >= block_rows);
  const int plane_block_width = block_width >> pd->subsampling_x;
  const int plane_block_height = block_height >> pd->subsampling_y;
  // Special handling for chroma sub8x8.
  const int is_chroma_sub8_x = plane > 0 && plane_block_width < 4;
  const int is_chroma_sub8_y = plane > 0 && plane_block_height < 4;
  if (width) *width = plane_block_width + 2 * is_chroma_sub8_x;
  if (height) *height = plane_block_height + 2 * is_chroma_sub8_y;
  if (rows_within_bounds) {
    *rows_within_bounds =
        (block_rows >> pd->subsampling_y) + 2 * is_chroma_sub8_y;
  }
  if (cols_within_bounds) {
    *cols_within_bounds =
        (block_cols >> pd->subsampling_x) + 2 * is_chroma_sub8_x;
  }
}

/* clang-format off */
typedef aom_cdf_prob (*MapCdf)[PALETTE_COLOR_INDEX_CONTEXTS]
                              [CDF_SIZE(PALETTE_COLORS)];
typedef const int (*ColorCost)[PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS]
                              [PALETTE_COLORS];
/* clang-format on */

typedef struct {
  int rows;
  int cols;
  int n_colors;
  int plane_width;
  int plane_height;
  uint8_t *color_map;
  MapCdf map_cdf;
  ColorCost color_cost;
} Av1ColorMapParam;

static INLINE int is_nontrans_global_motion(const MACROBLOCKD *xd,
                                            const MB_MODE_INFO *mbmi) {
  int ref;

  // First check if all modes are GLOBALMV
  if (mbmi->mode != GLOBALMV && mbmi->mode != GLOBAL_GLOBALMV) return 0;

#if !(CONFIG_EXT_WARP && CONFIG_SUB8X8_WARP)
  if (AOMMIN(mi_size_wide[mbmi->sb_type], mi_size_high[mbmi->sb_type]) < 2)
    return 0;
#endif  // !(CONFIG_EXT_WARP && CONFIG_SUB8X8_WARP)

  // Now check if all global motion is non translational
  for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
    if (xd->global_motion[mbmi->ref_frame[ref]].wmtype == TRANSLATION) return 0;
  }
  return 1;
}

static INLINE PLANE_TYPE get_plane_type(int plane) {
  return (plane == 0) ? PLANE_TYPE_Y : PLANE_TYPE_UV;
}

static INLINE int av1_get_max_eob(TX_SIZE tx_size) {
  if (tx_size == TX_64X64 || tx_size == TX_64X32 || tx_size == TX_32X64) {
    return 1024;
  }
  if (tx_size == TX_16X64 || tx_size == TX_64X16) {
    return 512;
  }
#if CONFIG_FLEX_PARTITION
  if (tx_size == TX_8X64 || tx_size == TX_64X8) {
    return 256;
  }
  if (tx_size == TX_4X64 || tx_size == TX_64X4) {
    return 128;
  }
#endif  // CONFIG_FLEX_PARTITION
  return tx_size_2d[tx_size];
}

static INLINE int av1_pixels_to_mi(int pixels) {
  return ALIGN_POWER_OF_TWO(pixels, 3) >> MI_SIZE_LOG2;
}

static INLINE int is_cfl_allowed_bsize(BLOCK_SIZE bsize) {
  return (CFL_ALLOWED_TYPE)(block_size_wide[bsize] <= 32 &&
                            block_size_high[bsize] <= 32);
}

// Can we use CfL for the current block?
static INLINE CFL_ALLOWED_TYPE is_cfl_allowed(const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(bsize < BLOCK_SIZES_ALL);
  if (xd->lossless[mbmi->segment_id]) {
    // In lossless, CfL is available when the partition size is equal to the
    // transform size.
    const int ssx = xd->plane[AOM_PLANE_U].subsampling_x;
    const int ssy = xd->plane[AOM_PLANE_U].subsampling_y;
    const int plane_bsize =
        get_plane_block_size(xd->mi[0]->chroma_ref_info.bsize_base, ssx, ssy);
    return (CFL_ALLOWED_TYPE)(plane_bsize == BLOCK_4X4);
  }
  // Spec: CfL is available to luma partitions lesser than or equal to 32x32
  return is_cfl_allowed_bsize(bsize);
}

// Calculate unit width and height for processing coefficients this plane, to
// ensure processing correct number of block rows and cols.
void av1_get_unit_width_height_coeff(const MACROBLOCKD *const xd, int plane,
                                     BLOCK_SIZE plane_bsize, int row_plane,
                                     int col_plane, int *unit_width,
                                     int *unit_height);

void av1_reset_is_mi_coded_map(MACROBLOCKD *xd, int stride);
void av1_mark_block_as_coded(MACROBLOCKD *xd, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, BLOCK_SIZE sb_size);
void av1_mark_block_as_not_coded(MACROBLOCKD *xd, int mi_row, int mi_col,
                                 BLOCK_SIZE bsize, BLOCK_SIZE sb_size);

#if CONFIG_INTRA_ENTROPY
// Calculate histogram of gradient orientations of the reconstructed pixel
// values in current coding block.
void av1_get_gradient_hist(const MACROBLOCKD *const xd,
                           MB_MODE_INFO *const mbmi, BLOCK_SIZE bsize);
// Calculate variance of the reconstructed pixel values in current coding block.
void av1_get_recon_var(const MACROBLOCKD *const xd, MB_MODE_INFO *const mbmi,
                       BLOCK_SIZE bsize);

void av1_get_intra_block_feature(int *sparse_features, float *dense_features,
                                 const MB_MODE_INFO *above_mi,
                                 const MB_MODE_INFO *left_mi,
                                 const MB_MODE_INFO *aboveleft_mi);

void av1_get_intra_uv_block_feature(int *sparse_features, float *dense_features,
                                    PREDICTION_MODE cur_y_mode,
                                    int is_cfl_allowed,
                                    const MB_MODE_INFO *above_mi,
                                    const MB_MODE_INFO *left_mi);

void av1_pdf2icdf(float *pdf, aom_cdf_prob *cdf, int nsymbs);

void av1_get_kf_y_mode_cdf_ml(const MACROBLOCKD *xd, aom_cdf_prob *cdf);

void av1_get_uv_mode_cdf_ml(const MACROBLOCKD *xd, PREDICTION_MODE y_mode,
                            aom_cdf_prob *cdf);
#endif  // CONFIG_INTRA_ENTROPY

#if CONFIG_DERIVED_INTRA_MODE
int av1_enable_derived_intra_mode(const MACROBLOCKD *xd, int bsize);
int av1_get_derived_intra_mode(const MACROBLOCKD *xd, int bsize,
                               MB_MODE_INFO *mbmi);
#endif  // CONFIG_DERIVED_INTRA_MODE

#if CONFIG_MODE_DEP_INTRA_TX || CONFIG_MODE_DEP_INTER_TX

#define MODE_DEP_INTER_TX_MODES 2
#define MODE_DEP_INTER_TX_MODE_START 64

// Transform mode to be used for mode dependent transforms
static INLINE int get_mode_dep_txfm_mode(const MB_MODE_INFO *const mbmi) {
  if (is_intra_mode(mbmi->mode)) return mbmi->mode;
  // Inter modes start from 64.
  return MODE_DEP_INTER_TX_MODE_START + is_inter_compound_mode(mbmi->mode);
}

// whether it is an intra mode from the txfm_mode
static INLINE int is_intra_mode_dep_txfm_mode(int txfm_mode) {
  // Intra txfm modes have the same values as mbmi modes
  return is_intra_mode(txfm_mode);
}

static INLINE int intra_mode_dep_txfm_mode(int txfm_mode) { return txfm_mode; }

static INLINE int inter_mode_dep_txfm_mode(int txfm_mode) {
  return txfm_mode - MODE_DEP_INTER_TX_MODE_START;
}

#else
static INLINE int get_mode_dep_txfm_mode(const MB_MODE_INFO *const mbmi) {
  return mbmi->mode;
}
#endif  // CONFIG_MODE_DEP_INTRA_TX || CONFIG_MODE_DEP_INTER_TX

#if CONFIG_SKIP_INTERP_FILTER
static INLINE int av1_mv_has_subpel(const MB_MODE_INFO *mbmi, int dir,
                                    int enable_dual) {
  int has_subpel = 0;
  for (int d = 0; d < 2; ++d) {
    // If dual filter is enabled, we only check the specified direction;
    // otherwise we check both directions.
    if (enable_dual && d != dir) continue;
    for (int i = 0; i < 1 + has_second_ref(mbmi); ++i) {
      if (d == 0) {
        if (mbmi->mv[i].as_mv.row & 7) has_subpel = 1;
      } else {
        if (mbmi->mv[i].as_mv.col & 7) has_subpel = 1;
      }
    }
  }

  return has_subpel;
}
#endif  // CONFIG_SKIP_INTERP_FILTER

#if CONFIG_FLEX_MVRES && ADJUST_DRL_FLEX_MVRES
static INLINE int av1_use_adjust_drl(const MB_MODE_INFO *mbmi) {
  if (mbmi->pb_mv_precision >= mbmi->max_mv_precision) return 0;
  const PREDICTION_MODE mode = mbmi->mode;
  return mode == NEWMV ||
#if CONFIG_OPTFLOW_REFINEMENT
         mode == NEW_NEWMV_OPTFLOW ||
#endif  // CONFIG_OPTFLOW_REFINEMENT
         mode == NEW_NEWMV;
}
#endif  // CONFIG_FLEX_MVRES && ADJUST_DRL_FLEX_MVRES

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_BLOCKD_H_
