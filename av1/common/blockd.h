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

static INLINE int is_inter_mode(PREDICTION_MODE mode) {
  return mode >= INTER_MODE_START && mode < INTER_MODE_END;
}

typedef struct {
  uint8_t *plane[MAX_MB_PLANE];
  int stride[MAX_MB_PLANE];
} BUFFER_SET;

static INLINE int is_inter_singleref_mode(PREDICTION_MODE mode) {
  return mode >= SINGLE_INTER_MODE_START && mode < SINGLE_INTER_MODE_END;
}
static INLINE int is_inter_compound_mode(PREDICTION_MODE mode) {
  return mode >= COMP_INTER_MODE_START && mode < COMP_INTER_MODE_END;
}

static INLINE PREDICTION_MODE compound_ref0_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,  // DC_PRED
    MB_MODE_COUNT,  // V_PRED
    MB_MODE_COUNT,  // H_PRED
    MB_MODE_COUNT,  // D45_PRED
    MB_MODE_COUNT,  // D135_PRED
    MB_MODE_COUNT,  // D113_PRED
    MB_MODE_COUNT,  // D157_PRED
    MB_MODE_COUNT,  // D203_PRED
    MB_MODE_COUNT,  // D67_PRED
    MB_MODE_COUNT,  // SMOOTH_PRED
    MB_MODE_COUNT,  // SMOOTH_V_PRED
    MB_MODE_COUNT,  // SMOOTH_H_PRED
    MB_MODE_COUNT,  // PAETH_PRED
    MB_MODE_COUNT,  // NEARESTMV
    MB_MODE_COUNT,  // NEARMV
    MB_MODE_COUNT,  // GLOBALMV
    MB_MODE_COUNT,  // NEWMV
    NEARESTMV,      // NEAREST_NEARESTMV
    NEARMV,         // NEAR_NEARMV
#if !CONFIG_NEW_INTER_MODES
    NEARESTMV,  // NEAREST_NEWMV
    NEWMV,      // NEW_NEARESTMV
#endif          // !CONFIG_NEW_INTER_MODES
    NEARMV,     // NEAR_NEWMV
    NEWMV,      // NEW_NEARMV
    GLOBALMV,   // GLOBAL_GLOBALMV
    NEWMV,      // NEW_NEWMV
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
  assert(is_inter_compound_mode(mode));
  return lut[mode];
}

static INLINE PREDICTION_MODE compound_ref1_mode(PREDICTION_MODE mode) {
  static PREDICTION_MODE lut[] = {
    MB_MODE_COUNT,  // DC_PRED
    MB_MODE_COUNT,  // V_PRED
    MB_MODE_COUNT,  // H_PRED
    MB_MODE_COUNT,  // D45_PRED
    MB_MODE_COUNT,  // D135_PRED
    MB_MODE_COUNT,  // D113_PRED
    MB_MODE_COUNT,  // D157_PRED
    MB_MODE_COUNT,  // D203_PRED
    MB_MODE_COUNT,  // D67_PRED
    MB_MODE_COUNT,  // SMOOTH_PRED
    MB_MODE_COUNT,  // SMOOTH_V_PRED
    MB_MODE_COUNT,  // SMOOTH_H_PRED
    MB_MODE_COUNT,  // PAETH_PRED
    MB_MODE_COUNT,  // NEARESTMV
    MB_MODE_COUNT,  // NEARMV
    MB_MODE_COUNT,  // GLOBALMV
    MB_MODE_COUNT,  // NEWMV
    NEARESTMV,      // NEAREST_NEARESTMV
    NEARMV,         // NEAR_NEARMV
#if !CONFIG_NEW_INTER_MODES
    NEWMV,      // NEAREST_NEWMV
    NEARESTMV,  // NEW_NEARESTMV
#endif          // !CONFIG_NEW_INTER_MODES
    NEWMV,      // NEAR_NEWMV
    NEARMV,     // NEW_NEARMV
    GLOBALMV,   // GLOBAL_GLOBALMV
    NEWMV,      // NEW_NEWMV
  };
  assert(NELEMENTS(lut) == MB_MODE_COUNT);
  assert(is_inter_compound_mode(mode));
  return lut[mode];
}

static INLINE int have_nearmv_in_inter_mode(PREDICTION_MODE mode) {
  return (mode == NEARMV || mode == NEAR_NEARMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV);
}

static INLINE int have_newmv_in_inter_mode(PREDICTION_MODE mode) {
#if CONFIG_NEW_INTER_MODES
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAR_NEWMV ||
          mode == NEW_NEARMV);
#else
  return (mode == NEWMV || mode == NEW_NEWMV || mode == NEAREST_NEWMV ||
          mode == NEW_NEARESTMV || mode == NEAR_NEWMV || mode == NEW_NEARMV);
#endif  // CONFIG_NEW_INTER_MODES
}

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
  MvSubpelPrecision mv_precision;
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
  uint8_t ref_mv_idx : 2;
#if CONFIG_FLEX_MVRES
  uint8_t ref_mv_idx_adj : 2;
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
#endif  // CONFIG_DERIVED_INTRA_MODE
} MB_MODE_INFO;

typedef struct PARTITION_TREE {
  struct PARTITION_TREE *sub_tree[4];
  PARTITION_TYPE partition;
  BLOCK_SIZE bsize;
  int is_settled;
  int mi_row;
  int mi_col;
} PARTITION_TREE;

typedef struct SB_INFO {
  int mi_row;
  int mi_col;
  PARTITION_TREE *ptree_root;
} SB_INFO;

PARTITION_TREE *av1_alloc_ptree_node(void);
void av1_free_ptree_recursive(PARTITION_TREE *ptree);
void av1_reset_ptree_in_sbi(SB_INFO *sbi);

static INLINE int is_intrabc_block(const MB_MODE_INFO *mbmi) {
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

static INLINE int has_second_ref(const MB_MODE_INFO *mbmi) {
  return mbmi->ref_frame[1] > INTRA_FRAME;
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
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int block_size_allowed =
      AOMMIN(block_size_wide[bsize], block_size_high[bsize]) >= 8;
  return (mode == GLOBALMV || mode == GLOBAL_GLOBALMV) && type > TRANSLATION &&
         block_size_allowed;
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
  int16_t seg_dequant_QTX[MAX_SEGMENTS][2];
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

#if CONFIG_LOOP_RESTORE_CNN
typedef struct {
  FRAME_TYPE frame_type;
  int base_qindex;
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

  /* pointers to reference frame scale factors */
  const struct scale_factors *block_ref_scale_factors[2];

  /* pointer to current frame */
  const YV12_BUFFER_CONFIG *cur_buf;

  ENTROPY_CONTEXT *above_context[MAX_MB_PLANE];
  ENTROPY_CONTEXT left_context[MAX_MB_PLANE][MAX_MIB_SIZE];

  PARTITION_CONTEXT *above_seg_context;
  PARTITION_CONTEXT left_seg_context[MAX_MIB_SIZE];

  TXFM_CONTEXT *above_txfm_context;
  TXFM_CONTEXT *left_txfm_context;
  TXFM_CONTEXT left_txfm_context_buffer[MAX_MIB_SIZE];

  WienerInfo wiener_info[MAX_MB_PLANE];
  SgrprojInfo sgrproj_info[MAX_MB_PLANE];
#if CONFIG_WIENER_NONSEP
  WienerNonsepInfo wiener_nonsep_info[MAX_MB_PLANE];
#endif  // CONFIG_WIENER_NONSEP

  // block dimension in the unit of mode_info.
  uint8_t n4_w, n4_h;

  uint8_t ref_mv_count[MODE_CTX_REF_FRAMES];
  CANDIDATE_MV ref_mv_stack[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
  uint16_t weight[MODE_CTX_REF_FRAMES][MAX_REF_MV_STACK_SIZE];
#if CONFIG_FLEX_MVRES
  uint8_t ref_mv_count_adj;
  CANDIDATE_MV ref_mv_stack_adj[MAX_REF_MV_STACK_SIZE];
  uint16_t weight_adj[MAX_REF_MV_STACK_SIZE];
#endif  // CONFIG_FLEX_MVRES
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
    const int sqr_bsize_idx = get_sqr_bsize_idx(bsize);
    return sqr_bsize_idx >= SQR_BLOCK_SIZES
               ? BLOCK_INVALID
               : subsize_lookup[partition][sqr_bsize_idx];
  }
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

#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTRA && USE_MDTX_INTER
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
#if USE_NST_INTRA
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
#endif
};
#elif USE_MDTX_INTRA
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
#if USE_NST_INTRA
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
#endif
};
#elif USE_MDTX_INTER
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};
#endif
#else
static const int av1_ext_tx_used[EXT_TX_SET_TYPES][TX_TYPES] = {
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
  { 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
};
#endif

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

#if CONFIG_MODE_DEP_TX && USE_MDTX_INTRA && USE_NST_INTRA
static INLINE int use_nstx(TX_TYPE tx_type, TX_SIZE tx_size,
                           PREDICTION_MODE mode) {
  // TODO(kslu) change this to turn off mdtx for some modes
  (void)mode;
  if (tx_type != MDTX_INTRA_4) return 0;
  int is_valid_nstx_size = tx_size == TX_4X4;
#if USE_NST_8X8
  is_valid_nstx_size |= tx_size == TX_8X8;
#endif
#if USE_NST_4X8_8X4
  is_valid_nstx_size |= (tx_size == TX_4X8 || tx_size == TX_8X4);
#endif
  return is_valid_nstx_size;
}
#endif

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
#if CONFIG_MODE_DEP_TX && USE_MDTX_INTER
                                    : EXT_TX_SET_ALL16_MDTX8);
#else
                                    : EXT_TX_SET_ALL16);
#endif
  } else {
    return (tx_size_sqr == TX_16X16 ? EXT_TX_SET_DTT4_IDTX
#if CONFIG_MODE_DEP_TX && USE_MDTX_INTRA
                                    : EXT_TX_SET_DTT4_IDTX_1DDCT_MDTX4);
#else
                                    : EXT_TX_SET_DTT4_IDTX_1DDCT);
#endif
  }
}

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
static INLINE BLOCK_SIZE get_plane_block_size(int mi_row, int mi_col,
                                              BLOCK_SIZE bsize,
                                              int subsampling_x,
                                              int subsampling_y) {
  if (bsize == BLOCK_INVALID) return BLOCK_INVALID;
  assert(subsampling_x >= 0 && subsampling_x < 2);
  assert(subsampling_y >= 0 && subsampling_y < 2);
  bsize =
      scale_chroma_bsize(bsize, subsampling_x, subsampling_y, mi_row, mi_col);
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

static INLINE TX_SIZE av1_get_max_uv_txsize(int mi_row, int mi_col,
                                            BLOCK_SIZE bsize, int subsampling_x,
                                            int subsampling_y) {
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mi_row, mi_col, bsize, subsampling_x, subsampling_y);
  assert(plane_bsize < BLOCK_SIZES_ALL);
  const TX_SIZE uv_tx = max_txsize_rect_lookup[plane_bsize];
  return av1_get_adjusted_tx_size(uv_tx);
}

static INLINE TX_SIZE av1_get_tx_size(int plane, const MACROBLOCKD *xd) {
  const MB_MODE_INFO *mbmi = xd->mi[0];
  if (xd->lossless[mbmi->segment_id]) return TX_4X4;
  if (plane == 0) return mbmi->tx_size;
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const MACROBLOCKD_PLANE *pd = &xd->plane[plane];
  const BLOCK_SIZE bsize = scale_chroma_bsize(
      mbmi->sb_type, pd->subsampling_x, pd->subsampling_y, mi_row, mi_col);
  return av1_get_max_uv_txsize(mi_row, mi_col, bsize, pd->subsampling_x,
                               pd->subsampling_y);
}

void av1_reset_skip_context(MACROBLOCKD *xd, int mi_row, int mi_col,
                            BLOCK_SIZE bsize, const int num_planes);

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
#if CONFIG_EXT_PARTITIONS
  // TODO(urvang): Enable this special case, if we make OBMC work.
  if ((mi_row & 0x01) || (mi_col & 0x01)) {
    assert((block_size_wide[bsize] == 8 && block_size_high[bsize] == 16) ||
           (block_size_wide[bsize] == 16 && block_size_high[bsize] == 8));
    return 0;
  }
#else
  assert(!(mi_row & 0x01) && !(mi_col & 0x01));
  (void)mi_row;
  (void)mi_col;
#endif  // CONFIG_EXT_PARTITIONS
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

static INLINE MOTION_MODE
motion_mode_allowed(const WarpedMotionParams *gm_params, const MACROBLOCKD *xd,
                    const MB_MODE_INFO *mbmi, int allow_warped_motion) {
  if (xd->cur_frame_force_integer_mv == 0) {
    const TransformationType gm_type = gm_params[mbmi->ref_frame[0]].wmtype;
    if (is_global_mv_block(mbmi, gm_type)) return SIMPLE_TRANSLATION;
  }
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  if (is_motion_variation_allowed_bsize(mbmi->sb_type, mi_row, mi_col) &&
      is_inter_mode(mbmi->mode) && mbmi->ref_frame[1] != INTRA_FRAME &&
      is_motion_variation_allowed_compound(mbmi)) {
    if (!check_num_overlappable_neighbors(mbmi)) return SIMPLE_TRANSLATION;
    assert(!has_second_ref(mbmi));
    if (mbmi->num_proj_ref >= 1 &&
        (allow_warped_motion &&
         !av1_is_scaled(xd->block_ref_scale_factors[0]))) {
      if (xd->cur_frame_force_integer_mv) {
        return OBMC_CAUSAL;
      }
      return WARPED_CAUSAL;
    }
    return OBMC_CAUSAL;
  } else {
    return SIMPLE_TRANSLATION;
  }
}

static INLINE void assert_motion_mode_valid(MOTION_MODE mode,
                                            const WarpedMotionParams *gm_params,
                                            const MACROBLOCKD *xd,
                                            const MB_MODE_INFO *mbmi,
                                            int allow_warped_motion) {
  const MOTION_MODE last_motion_mode_allowed =
      motion_mode_allowed(gm_params, xd, mbmi, allow_warped_motion);

  // Check that the input mode is not illegal
  if (last_motion_mode_allowed < mode)
    assert(0 && "Illegal motion mode selected");
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
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  bsize = scale_chroma_bsize(bsize, pd->subsampling_x, pd->subsampling_y,
                             mi_row, mi_col);
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

  if (AOMMIN(mi_size_wide[mbmi->sb_type], mi_size_high[mbmi->sb_type]) < 2)
    return 0;

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
    const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
    const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
    const int plane_bsize =
        get_plane_block_size(mi_row, mi_col, bsize, ssx, ssy);
    return (CFL_ALLOWED_TYPE)(plane_bsize == BLOCK_4X4);
  }
  // Spec: CfL is available to luma partitions lesser than or equal to 32x32
  return (CFL_ALLOWED_TYPE)(block_size_wide[bsize] <= 32 &&
                            block_size_high[bsize] <= 32);
}

// Calculate unit width and height for processing coefficients this plane, to
// ensure processing correct number of block rows and cols.
void av1_get_unit_width_height_coeff(const MACROBLOCKD *const xd, int plane,
                                     int mi_row, int mi_col,
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
                               uint8_t *derived_angle);
#endif  // CONFIG_DERIVED_INTRA_MODE

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_COMMON_BLOCKD_H_
