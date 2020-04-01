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

#include <limits.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/binary_codes_writer.h"
#include "aom_ports/mem.h"
#include "aom_ports/aom_timer.h"
#include "aom_ports/system_state.h"

#if CONFIG_MISMATCH_DEBUG
#include "aom_util/debug_util.h"
#endif  // CONFIG_MISMATCH_DEBUG

#include "av1/common/cfl.h"
#include "av1/common/common.h"
#include "av1/common/entropy.h"
#include "av1/common/entropymode.h"
#include "av1/common/idct.h"
#include "av1/common/mv.h"
#include "av1/common/mvref_common.h"
#include "av1/common/pred_common.h"
#include "av1/common/quant_common.h"
#include "av1/common/reconintra.h"
#include "av1/common/reconinter.h"
#include "av1/common/seg_common.h"
#include "av1/common/tile_common.h"
#include "av1/common/warped_motion.h"

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/corner_detect.h"
#include "av1/encoder/global_motion.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemb.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/extend.h"
#include "av1/encoder/ml.h"
#include "av1/encoder/partition_strategy.h"
#if !CONFIG_REALTIME_ONLY
#include "av1/encoder/partition_model_weights.h"
#endif
#include "av1/encoder/rd.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/reconinter_enc.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/tokenize.h"
#include "av1/encoder/tpl_model.h"
#include "av1/encoder/var_based_part.h"
#include "av1/encoder/tpl_model.h"

// This is used as a reference when computing the source variance for the
//  purposes of activity masking.
// Eventually this should be replaced by custom no-reference routines,
//  which will be faster.
const uint8_t AV1_VAR_OFFS[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128
};

static const uint16_t AV1_HIGH_VAR_OFFS_8[MAX_SB_SIZE] = {
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
  128, 128, 128, 128, 128, 128, 128, 128
};

static const uint16_t AV1_HIGH_VAR_OFFS_10[MAX_SB_SIZE] = {
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4,
  128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4, 128 * 4
};

static const uint16_t AV1_HIGH_VAR_OFFS_12[MAX_SB_SIZE] = {
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16, 128 * 16,
  128 * 16, 128 * 16
};

typedef struct {
  ENTROPY_CONTEXT a[MAX_MIB_SIZE * MAX_MB_PLANE];
  ENTROPY_CONTEXT l[MAX_MIB_SIZE * MAX_MB_PLANE];
  PARTITION_CONTEXT sa[MAX_MIB_SIZE];
  PARTITION_CONTEXT sl[MAX_MIB_SIZE];
  TXFM_CONTEXT *p_ta;
  TXFM_CONTEXT *p_tl;
  TXFM_CONTEXT ta[MAX_MIB_SIZE];
  TXFM_CONTEXT tl[MAX_MIB_SIZE];
} RD_SEARCH_MACROBLOCK_CONTEXT;

enum { PICK_MODE_RD = 0, PICK_MODE_NONRD, PICK_MODE_FAST_NONRD };

enum {
  SB_SINGLE_PASS,  // Single pass encoding: all ctxs get updated normally
  SB_DRY_PASS,     // First pass of multi-pass: does not update the ctxs
  SB_WET_PASS      // Second pass of multi-pass: finalize and update the ctx
} UENUM1BYTE(SB_MULTI_PASS_MODE);

// This struct is used to store the statistics used by sb-level multi-pass
// encoding. Currently, this is only used to make a copy of the state before we
// perform the first pass
typedef struct SB_FIRST_PASS_STATS {
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  RD_COUNTS rd_count;

  int split_count;
  FRAME_COUNTS fc;
  InterModeRdModel inter_mode_rd_models[BLOCK_SIZES_ALL];
  int thresh_freq_fact[BLOCK_SIZES_ALL][MAX_MODES];
  int m_search_count;
  int ex_search_count;
  int current_qindex;

#if CONFIG_INTERNAL_STATS
  unsigned int mode_chosen_counts[MAX_MODES];
#endif  // CONFIG_INTERNAL_STATS
} SB_FIRST_PASS_STATS;

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                              BLOCK_SIZE bsize, int *rate);
#if !CONFIG_REALTIME_ONLY
static bool rd_pick_partition(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TOKENEXTRA **tp, int mi_row, int mi_col, BLOCK_SIZE bsize,
    BLOCK_SIZE max_sq_part, BLOCK_SIZE min_sq_part, RD_STATS *rd_cost,
    RD_STATS best_rdc, PC_TREE *pc_tree, SIMPLE_MOTION_DATA_TREE *sms_tree,
    int64_t *none_rd, SB_MULTI_PASS_MODE multi_pass_mode);
#endif  // !CONFIG_REALTIME_ONLY

unsigned int av1_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs) {
  unsigned int sse;
  const unsigned int var =
      cpi->fn_ptr[bs].vf(ref->buf, ref->stride, AV1_VAR_OFFS, 0, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

unsigned int av1_high_get_sby_perpixel_variance(const AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd) {
  unsigned int var, sse;
  switch (bd) {
    case 10:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_10), 0, &sse);
      break;
    case 12:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_12), 0, &sse);
      break;
    case 8:
    default:
      var =
          cpi->fn_ptr[bs].vf(ref->buf, ref->stride,
                             CONVERT_TO_BYTEPTR(AV1_HIGH_VAR_OFFS_8), 0, &sse);
      break;
  }
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

#if !CONFIG_REALTIME_ONLY
static unsigned int get_sby_perpixel_diff_variance(const AV1_COMP *const cpi,
                                                   const struct buf_2d *ref,
                                                   int mi_row, int mi_col,
                                                   BLOCK_SIZE bs) {
  unsigned int sse, var;
  uint8_t *last_y;
  const YV12_BUFFER_CONFIG *last =
      get_ref_frame_yv12_buf(&cpi->common, LAST_FRAME);

  assert(last != NULL);
  last_y =
      &last->y_buffer[mi_row * MI_SIZE * last->y_stride + mi_col * MI_SIZE];
  var = cpi->fn_ptr[bs].vf(ref->buf, ref->stride, last_y, last->y_stride, &sse);
  return ROUND_POWER_OF_TWO(var, num_pels_log2_lookup[bs]);
}

static BLOCK_SIZE get_rd_var_based_fixed_partition(AV1_COMP *cpi, MACROBLOCK *x,
                                                   int mi_row, int mi_col) {
  unsigned int var = get_sby_perpixel_diff_variance(
      cpi, &x->plane[0].src, mi_row, mi_col, BLOCK_64X64);
  if (var < 8)
    return BLOCK_64X64;
  else if (var < 128)
    return BLOCK_32X32;
  else if (var < 2048)
    return BLOCK_16X16;
  else
    return BLOCK_8X8;
}
#endif  // !CONFIG_REALTIME_ONLY

static int set_deltaq_rdmult(const AV1_COMP *const cpi, MACROBLOCKD *const xd) {
  const AV1_COMMON *const cm = &cpi->common;

  return av1_compute_rd_mult(
      cpi, cm->base_qindex + xd->delta_qindex + cm->y_dc_delta_q);
}

static void set_ssim_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                            const BLOCK_SIZE bsize, const int mi_row,
                            const int mi_col, int *const rdmult) {
  const AV1_COMMON *const cm = &cpi->common;

  const int bsize_base = BLOCK_16X16;
  const int num_mi_w = mi_size_wide[bsize_base];
  const int num_mi_h = mi_size_high[bsize_base];
  const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
  const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
  const int num_bcols = (mi_size_wide[bsize] + num_mi_w - 1) / num_mi_w;
  const int num_brows = (mi_size_high[bsize] + num_mi_h - 1) / num_mi_h;
  int row, col;
  double num_of_mi = 0.0;
  double geom_mean_of_scale = 0.0;

  assert(cpi->oxcf.tuning == AOM_TUNE_SSIM);

  aom_clear_system_state();
  for (row = mi_row / num_mi_w;
       row < num_rows && row < mi_row / num_mi_w + num_brows; ++row) {
    for (col = mi_col / num_mi_h;
         col < num_cols && col < mi_col / num_mi_h + num_bcols; ++col) {
      const int index = row * num_cols + col;
      geom_mean_of_scale += log(cpi->ssim_rdmult_scaling_factors[index]);
      num_of_mi += 1.0;
    }
  }
  geom_mean_of_scale = exp(geom_mean_of_scale / num_of_mi);

  *rdmult = (int)((double)(*rdmult) * geom_mean_of_scale + 0.5);
  *rdmult = AOMMAX(*rdmult, 0);
  set_error_per_bit(x, *rdmult);
  aom_clear_system_state();
}

static int get_hier_tpl_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               const BLOCK_SIZE bsize, const int mi_row,
                               const int mi_col, int orig_rdmult) {
  const AV1_COMMON *const cm = &cpi->common;
  assert(IMPLIES(cpi->gf_group.size > 0,
                 cpi->gf_group.index < cpi->gf_group.size));
  const int tpl_idx = cpi->gf_group.index;
  const TplDepFrame *tpl_frame = &cpi->tpl_frame[tpl_idx];
  MACROBLOCKD *const xd = &x->e_mbd;
  const int deltaq_rdmult = set_deltaq_rdmult(cpi, xd);
  if (cpi->tpl_model_pass == 1) {
    assert(cpi->oxcf.enable_tpl_model == 2);
    return deltaq_rdmult;
  }
  if (tpl_frame->is_valid == 0) return deltaq_rdmult;
  if (!is_frame_tpl_eligible((AV1_COMP *)cpi)) return deltaq_rdmult;
  if (tpl_idx >= MAX_LAG_BUFFERS) return deltaq_rdmult;
  if (cpi->oxcf.superres_mode != SUPERRES_NONE) return deltaq_rdmult;
  if (cpi->oxcf.aq_mode != NO_AQ) return deltaq_rdmult;

  const int bsize_base = BLOCK_16X16;
  const int num_mi_w = mi_size_wide[bsize_base];
  const int num_mi_h = mi_size_high[bsize_base];
  const int num_cols = (cm->mi_cols + num_mi_w - 1) / num_mi_w;
  const int num_rows = (cm->mi_rows + num_mi_h - 1) / num_mi_h;
  const int num_bcols = (mi_size_wide[bsize] + num_mi_w - 1) / num_mi_w;
  const int num_brows = (mi_size_high[bsize] + num_mi_h - 1) / num_mi_h;
  int row, col;
  double base_block_count = 0.0;
  double geom_mean_of_scale = 0.0;
  aom_clear_system_state();
  for (row = mi_row / num_mi_w;
       row < num_rows && row < mi_row / num_mi_w + num_brows; ++row) {
    for (col = mi_col / num_mi_h;
         col < num_cols && col < mi_col / num_mi_h + num_bcols; ++col) {
      const int index = row * num_cols + col;
      geom_mean_of_scale += log(cpi->tpl_sb_rdmult_scaling_factors[index]);
      base_block_count += 1.0;
    }
  }
  geom_mean_of_scale = exp(geom_mean_of_scale / base_block_count);
  int rdmult = (int)((double)orig_rdmult * geom_mean_of_scale + 0.5);
  rdmult = AOMMAX(rdmult, 0);
  set_error_per_bit(x, rdmult);
  aom_clear_system_state();
  if (bsize == cm->seq_params.sb_size) {
    const int rdmult_sb = set_deltaq_rdmult(cpi, xd);
    assert(rdmult_sb == rdmult);
    (void)rdmult_sb;
  }
  return rdmult;
}

static int set_segment_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                              int8_t segment_id) {
  const AV1_COMMON *const cm = &cpi->common;
  av1_init_plane_quantizers(cpi, x, segment_id);
  aom_clear_system_state();
  int segment_qindex = av1_get_qindex(&cm->seg, segment_id, cm->base_qindex);
  return av1_compute_rd_mult(cpi, segment_qindex + cm->y_dc_delta_q);
}

static void setup_block_rdmult(const AV1_COMP *const cpi, MACROBLOCK *const x,
                               int mi_row, int mi_col, BLOCK_SIZE bsize,
                               AQ_MODE aq_mode, MB_MODE_INFO *mbmi) {
  x->rdmult = cpi->rd.RDMULT;

  if (aq_mode != NO_AQ) {
    assert(mbmi != NULL);
    if (aq_mode == VARIANCE_AQ) {
      if (cpi->vaq_refresh) {
        const int energy = bsize <= BLOCK_16X16
                               ? x->mb_energy
                               : av1_log_block_var(cpi, x, bsize);
        mbmi->segment_id = energy;
      }
      x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
    } else if (aq_mode == COMPLEXITY_AQ) {
      x->rdmult = set_segment_rdmult(cpi, x, mbmi->segment_id);
    } else if (aq_mode == CYCLIC_REFRESH_AQ) {
      // If segment is boosted, use rdmult for that segment.
      if (cyclic_refresh_segment_id_boosted(mbmi->segment_id))
        x->rdmult = av1_cyclic_refresh_get_rdmult(cpi->cyclic_refresh);
    }
  }

  const AV1_COMMON *const cm = &cpi->common;
  if (cm->delta_q_info.delta_q_present_flag) {
    x->rdmult = get_hier_tpl_rdmult(cpi, x, bsize, mi_row, mi_col, x->rdmult);
  }

  if (cpi->oxcf.tuning == AOM_TUNE_SSIM) {
    set_ssim_rdmult(cpi, x, bsize, mi_row, mi_col, &x->rdmult);
  }
}

static void set_offsets_without_segment_id(
    const AV1_COMP *const cpi, const TileInfo *const tile, MACROBLOCK *const x,
    int mi_row, int mi_col, BLOCK_SIZE bsize,
    const CHROMA_REF_INFO *chr_ref_info) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCKD *const xd = &x->e_mbd;
  assert(bsize < BLOCK_SIZES_ALL);
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  set_mode_info_offsets(cpi, x, xd, mi_row, mi_col);
  set_skip_context(xd, mi_row, mi_col, num_planes, chr_ref_info);
  xd->above_txfm_context = cm->above_txfm_context[tile->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  // Set up destination pointers.
  av1_setup_dst_planes(xd->plane, &cm->cur_frame->buf, mi_row, mi_col, 0,
                       num_planes, chr_ref_info);

  // Set up limit values for MV components.
  // Mv beyond the range do not produce new/different prediction block.
  x->mv_limits.row_min =
      -(((mi_row + mi_height) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.col_min = -(((mi_col + mi_width) * MI_SIZE) + AOM_INTERP_EXTEND);
  x->mv_limits.row_max = (cm->mi_rows - mi_row) * MI_SIZE + AOM_INTERP_EXTEND;
  x->mv_limits.col_max = (cm->mi_cols - mi_col) * MI_SIZE + AOM_INTERP_EXTEND;

  set_plane_n4(xd, bsize, num_planes, chr_ref_info);

  // Set up distance of MB to edge of frame in 1/8th pel units.
#if !CONFIG_EXT_PARTITIONS && !CONFIG_EXT_RECUR_PARTITIONS
  assert(!(mi_col & (mi_width - 1)) && !(mi_row & (mi_height - 1)));
#endif  // !CONFIG_EXT_PARTITIONS && !CONFIG_EXT_RECUR_PARTITIONS
  set_mi_row_col(xd, tile, mi_row, mi_height, mi_col, mi_width, cm->mi_rows,
                 cm->mi_cols, chr_ref_info);

  // Set up source buffers.
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes,
                       chr_ref_info);

  // required by av1_append_sub8x8_mvs_for_idx() and av1_find_best_ref_mvs()
  xd->tile = *tile;

  xd->cfl.mi_row = mi_row;
  xd->cfl.mi_col = mi_col;
}

static void set_offsets(const AV1_COMP *const cpi, const TileInfo *const tile,
                        MACROBLOCK *const x, int mi_row, int mi_col,
                        BLOCK_SIZE bsize, CHROMA_REF_INFO *chr_ref_info) {
  const AV1_COMMON *const cm = &cpi->common;
  const struct segmentation *const seg = &cm->seg;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;

  set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize,
                                 chr_ref_info);

  // Setup segment ID.
  mbmi = xd->mi[0];
  mbmi->segment_id = 0;
  if (seg->enabled) {
    if (seg->enabled && !cpi->vaq_refresh) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mbmi->segment_id =
          map ? get_segment_id(cm, map, bsize, mi_row, mi_col) : 0;
    }
    av1_init_plane_quantizers(cpi, x, mbmi->segment_id);
  }
}

static void update_filter_type_count(FRAME_COUNTS *counts,
                                     const MACROBLOCKD *xd,
                                     const MB_MODE_INFO *mbmi) {
  int dir;
  for (dir = 0; dir < 2; ++dir) {
    const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
    InterpFilter filter = av1_extract_interp_filter(mbmi->interp_filters, dir);
    if (counts) ++counts->switchable_interp[ctx][filter];
  }
}

static void update_filter_type_cdf(uint8_t allow_update_cdf,
                                   const MACROBLOCKD *xd,
                                   const MB_MODE_INFO *mbmi) {
  int dir;
  for (dir = 0; dir < 2; ++dir) {
    const int ctx = av1_get_pred_context_switchable_interp(xd, dir);
    InterpFilter filter = av1_extract_interp_filter(mbmi->interp_filters, dir);
    if (allow_update_cdf) {
      update_cdf(xd->tile_ctx->switchable_interp_cdf[ctx], filter,
                 SWITCHABLE_FILTERS);
    }
  }
}

static void update_global_motion_used(PREDICTION_MODE mode, BLOCK_SIZE bsize,
                                      const MB_MODE_INFO *mbmi,
                                      RD_COUNTS *rdc) {
  if (mode == GLOBALMV || mode == GLOBAL_GLOBALMV) {
    const int num_4x4s = mi_size_wide[bsize] * mi_size_high[bsize];
    int ref;
    for (ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
      rdc->global_motion_used[mbmi->ref_frame[ref]] += num_4x4s;
    }
  }
}

#if CONFIG_FLEX_MVRES
static void update_reduced_mv_precision_used(const AV1_COMMON *const cm,
                                             const MB_MODE_INFO *mbmi,
                                             RD_COUNTS *rdc) {
  if (!is_pb_mv_precision_active(cm, mbmi->mode, mbmi->max_mv_precision))
    return;
  assert(av1_get_mbmi_mv_precision(cm, mbmi) == mbmi->pb_mv_precision);
  rdc->reduced_mv_precision_used[mbmi->max_mv_precision -
                                 mbmi->pb_mv_precision]++;
}
#endif  // CONFIG_FLEX_MVRES

static void reset_tx_size(MACROBLOCK *x, MB_MODE_INFO *mbmi,
                          const TX_MODE tx_mode) {
  MACROBLOCKD *const xd = &x->e_mbd;
  if (xd->lossless[mbmi->segment_id]) {
    mbmi->tx_size = TX_4X4;
  } else if (tx_mode != TX_MODE_SELECT) {
    mbmi->tx_size = tx_size_from_tx_mode(mbmi->sb_type, tx_mode);
  } else {
    BLOCK_SIZE bsize = mbmi->sb_type;
    TX_SIZE min_tx_size = depth_to_tx_size(MAX_TX_DEPTH, bsize);
    mbmi->tx_size = (TX_SIZE)TXSIZEMAX(mbmi->tx_size, min_tx_size);
  }
  if (is_inter_block(mbmi)) {
    memset(mbmi->inter_tx_size, mbmi->tx_size, sizeof(mbmi->inter_tx_size));
  }
  memset(mbmi->txk_type, DCT_DCT, sizeof(mbmi->txk_type[0]) * TXK_TYPE_BUF_LEN);
  av1_zero(x->blk_skip);
  x->skip = 0;
}

static void update_state(const AV1_COMP *const cpi, ThreadData *td,
                         const PICK_MODE_CONTEXT *const ctx, int mi_row,
                         int mi_col, BLOCK_SIZE bsize, RUN_TYPE dry_run) {
  int i, x_idx, y;
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  RD_COUNTS *const rdc = &td->rd_counts;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const MB_MODE_INFO *const mi = &ctx->mic;
  MB_MODE_INFO *const mi_addr = xd->mi[0];
  const struct segmentation *const seg = &cm->seg;
  const int bw = mi_size_wide[mi->sb_type];
  const int bh = mi_size_high[mi->sb_type];
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];

  assert(mi->sb_type == bsize);

  *mi_addr = *mi;
  *x->mbmi_ext = ctx->mbmi_ext;

  memcpy(x->blk_skip, ctx->blk_skip, sizeof(x->blk_skip[0]) * ctx->num_4x4_blk);

  x->skip = ctx->rd_stats.skip;

  // If segmentation in use
  if (seg->enabled) {
    // For in frame complexity AQ copy the segment id from the segment map.
    if (cpi->oxcf.aq_mode == COMPLEXITY_AQ) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      mi_addr->segment_id =
          map ? get_segment_id(cm, map, bsize, mi_row, mi_col) : 0;
      reset_tx_size(x, mi_addr, cm->tx_mode);
    }
    // Else for cyclic refresh mode update the segment map, set the segment id
    // and then update the quantizer.
    if (cpi->oxcf.aq_mode == CYCLIC_REFRESH_AQ) {
      av1_cyclic_refresh_update_segment(cpi, mi_addr, mi_row, mi_col, bsize,
                                        ctx->rd_stats.rate, ctx->rd_stats.dist,
                                        x->skip);
    }
    if (mi_addr->uv_mode == UV_CFL_PRED && !is_cfl_allowed(xd))
      mi_addr->uv_mode = UV_DC_PRED;
  }

  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }
  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];
  // Restore the coding context of the MB to that that was in place
  // when the mode was picked for it
  for (y = 0; y < mi_height; y++)
    for (x_idx = 0; x_idx < mi_width; x_idx++)
      if ((xd->mb_to_right_edge >> (3 + MI_SIZE_LOG2)) + mi_width > x_idx &&
          (xd->mb_to_bottom_edge >> (3 + MI_SIZE_LOG2)) + mi_height > y) {
        xd->mi[x_idx + y * mis] = mi_addr;
      }

  if (cpi->oxcf.aq_mode) av1_init_plane_quantizers(cpi, x, mi_addr->segment_id);

  if (dry_run) return;

#if CONFIG_INTERNAL_STATS
  {
    unsigned int *const mode_chosen_counts =
        (unsigned int *)cpi->mode_chosen_counts;  // Cast const away.
    if (frame_is_intra_only(cm)) {
      static const int kf_mode_index[] = {
        THR_DC /*DC_PRED*/,
        THR_V_PRED /*V_PRED*/,
        THR_H_PRED /*H_PRED*/,
        THR_D45_PRED /*D45_PRED*/,
        THR_D135_PRED /*D135_PRED*/,
        THR_D113_PRED /*D113_PRED*/,
        THR_D157_PRED /*D157_PRED*/,
        THR_D203_PRED /*D203_PRED*/,
        THR_D67_PRED /*D67_PRED*/,
        THR_SMOOTH,   /*SMOOTH_PRED*/
        THR_SMOOTH_V, /*SMOOTH_V_PRED*/
        THR_SMOOTH_H, /*SMOOTH_H_PRED*/
        THR_PAETH /*PAETH_PRED*/,
      };
      ++mode_chosen_counts[kf_mode_index[mi_addr->mode]];
    } else {
      // Note how often each mode chosen as best
      ++mode_chosen_counts[ctx->best_mode_index];
    }
  }
#endif
  if (!frame_is_intra_only(cm)) {
    if (is_inter_block(mi_addr)) {
      // TODO(sarahparker): global motion stats need to be handled per-tile
      // to be compatible with tile-based threading.
      update_global_motion_used(mi_addr->mode, bsize, mi_addr, rdc);
#if CONFIG_FLEX_MVRES
      update_reduced_mv_precision_used(cm, mi_addr, rdc);
#endif  // CONFIG_FLEX_MVRES
    }

    if (cm->interp_filter == SWITCHABLE &&
        mi_addr->motion_mode != WARPED_CAUSAL &&
        !is_nontrans_global_motion(xd, xd->mi[0])) {
      update_filter_type_count(td->counts, xd, mi_addr);
    }

    rdc->comp_pred_diff[SINGLE_REFERENCE] += ctx->single_pred_diff;
    rdc->comp_pred_diff[COMPOUND_REFERENCE] += ctx->comp_pred_diff;
    rdc->comp_pred_diff[REFERENCE_MODE_SELECT] += ctx->hybrid_pred_diff;
  }

  const int x_mis = AOMMIN(bw, cm->mi_cols - mi_col);
  const int y_mis = AOMMIN(bh, cm->mi_rows - mi_row);
  if (cm->seq_params.order_hint_info.enable_ref_frame_mvs)
    av1_copy_frame_mvs(cm, mi, mi_row, mi_col, x_mis, y_mis);
}

void av1_setup_src_planes(MACROBLOCK *x, const YV12_BUFFER_CONFIG *src,
                          int mi_row, int mi_col, const int num_planes,
                          const CHROMA_REF_INFO *chr_ref_info) {
  // Set current frame pointer.
  x->e_mbd.cur_buf = src;

  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); i++) {
    const int is_uv = i > 0;
    setup_pred_plane(&x->plane[i].src, src->buffers[i], src->crop_widths[is_uv],
                     src->crop_heights[is_uv], src->strides[is_uv], mi_row,
                     mi_col, NULL, x->e_mbd.plane[i].subsampling_x,
                     x->e_mbd.plane[i].subsampling_y, i > 0, chr_ref_info);
  }
}

static EdgeInfo edge_info(const struct buf_2d *ref, const BLOCK_SIZE bsize,
                          const bool high_bd, const int bd) {
  const int width = block_size_wide[bsize];
  const int height = block_size_high[bsize];
  // Implementation requires width to be a multiple of 8. It also requires
  // height to be a multiple of 4, but this is always the case.
  assert(height % 4 == 0);
  if (width % 8 != 0) {
    EdgeInfo ei = { .magnitude = 0, .x = 0, .y = 0 };
    return ei;
  }
  return av1_edge_exists(ref->buf, ref->stride, width, height, high_bd, bd);
}

static int use_pb_simple_motion_pred_sse(const AV1_COMP *const cpi) {
  // TODO(debargha, yuec): Not in use, need to implement a speed feature
  // utilizing this data point, and replace '0' by the corresponding speed
  // feature flag.
  return 0 && !frame_is_intra_only(&cpi->common);
}

static void pick_sb_modes(AV1_COMP *const cpi, TileDataEnc *tile_data,
                          MACROBLOCK *const x, int mi_row, int mi_col,
                          RD_STATS *rd_cost, PARTITION_TYPE partition,
                          BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                          RD_STATS best_rd, int pick_mode_type) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi;
  MB_MODE_INFO *ctx_mbmi = &ctx->mic;
  struct macroblock_plane *const p = x->plane;
  struct macroblockd_plane *const pd = xd->plane;
  const AQ_MODE aq_mode = cpi->oxcf.aq_mode;
  int i;

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, rd_pick_sb_modes_time);
#endif

  if (best_rd.rdcost < 0) {
    ctx->rd_stats.rdcost = INT64_MAX;
    ctx->rd_stats.skip = 0;
    av1_invalid_rd_stats(rd_cost);
    return;
  }

  aom_clear_system_state();

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize, &ctx->chroma_ref_info);

  mbmi = xd->mi[0];

  if (ctx->rd_mode_is_ready) {
    assert(ctx_mbmi->sb_type == bsize);
    assert(ctx_mbmi->partition == partition);
    *mbmi = *ctx_mbmi;
    rd_cost->rate = ctx->rd_stats.rate;
    rd_cost->dist = ctx->rd_stats.dist;
    rd_cost->rdcost = ctx->rd_stats.rdcost;
  } else {
    mbmi->sb_type = bsize;
    mbmi->partition = partition;
  }

  mbmi->chroma_ref_info = ctx->chroma_ref_info;

#if CONFIG_RD_DEBUG
  mbmi->mi_row = mi_row;
  mbmi->mi_col = mi_col;
#endif

  for (i = 0; i < num_planes; ++i) {
    p[i].coeff = ctx->coeff[i];
    p[i].qcoeff = ctx->qcoeff[i];
    pd[i].dqcoeff = ctx->dqcoeff[i];
    p[i].eobs = ctx->eobs[i];
    p[i].txb_entropy_ctx = ctx->txb_entropy_ctx[i];
  }

  for (i = 0; i < 2; ++i) pd[i].color_index_map = ctx->color_index_map[i];

  if (!ctx->rd_mode_is_ready) {
    ctx->skippable = 0;

    // Set to zero to make sure we do not use the previous encoded frame stats
    mbmi->skip = 0;

    // Reset skip mode flag.
    mbmi->skip_mode = 0;
  }

  x->skip_chroma_rd = !mbmi->chroma_ref_info.is_chroma_ref;

  if (ctx->rd_mode_is_ready) {
    x->skip = ctx->rd_stats.skip;
    *x->mbmi_ext = ctx->mbmi_ext;
    return;
  }

  if (is_cur_buf_hbd(xd)) {
    x->source_variance = av1_high_get_sby_perpixel_variance(
        cpi, &x->plane[0].src, bsize, xd->bd);
  } else {
    x->source_variance =
        av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
  }
  if (use_pb_simple_motion_pred_sse(cpi)) {
    const MV ref_mv_full = { .row = 0, .col = 0 };
    unsigned int var = 0;
    av1_simple_motion_sse_var(cpi, x, mi_row, mi_col, bsize, ref_mv_full, 0,
                              &x->simple_motion_pred_sse, &var);
  }

  // If the threshold for disabling wedge search is zero, it means the feature
  // should not be used. Use a value that will always succeed in the check.
  if (cpi->sf.disable_wedge_search_edge_thresh == 0) {
    x->edge_strength = UINT16_MAX;
    x->edge_strength_x = UINT16_MAX;
    x->edge_strength_y = UINT16_MAX;
  } else {
    EdgeInfo ei =
        edge_info(&x->plane[0].src, bsize, is_cur_buf_hbd(xd), xd->bd);
    x->edge_strength = ei.magnitude;
    x->edge_strength_x = ei.x;
    x->edge_strength_y = ei.y;
  }

  // Default initialization of the threshold for R-D optimization of
  // coefficients for mode decision
  x->coeff_opt_dist_threshold =
      get_rd_opt_coeff_thresh(cpi->coeff_opt_dist_threshold, 0, 0);

  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, aq_mode, mbmi);
  // Set error per bit for current rdmult
  set_error_per_bit(x, x->rdmult);
  av1_rd_cost_update(x->rdmult, &best_rd);

#if CONFIG_DERIVED_INTRA_MODE
  mbmi->use_derived_intra_mode[0] = 0;
  mbmi->use_derived_intra_mode[1] = 0;
#endif  // CONFIG_DERIVED_INTRA_MODE

  // Find best coding mode & reconstruct the MB so it is available
  // as a predictor for MBs that follow in the SB
  if (frame_is_intra_only(cm)) {
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, av1_rd_pick_intra_mode_sb_time);
#endif
    av1_rd_pick_intra_mode_sb(cpi, x, rd_cost, bsize, ctx, best_rd.rdcost);
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, av1_rd_pick_intra_mode_sb_time);
#endif
  } else {
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, av1_rd_pick_inter_mode_sb_time);
#endif
    if (segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      av1_rd_pick_inter_mode_sb_seg_skip(cpi, tile_data, x, mi_row, mi_col,
                                         rd_cost, bsize, ctx, best_rd.rdcost);
    } else {
      // TODO(kyslov): do the same for pick_intra_mode and
      //               pick_inter_mode_sb_seg_skip
      switch (pick_mode_type) {
#if !CONFIG_REALTIME_ONLY
        case PICK_MODE_RD:
          av1_rd_pick_inter_mode_sb(cpi, tile_data, x, rd_cost, bsize, ctx,
                                    best_rd.rdcost);
          break;
#endif
        case PICK_MODE_NONRD:
          av1_nonrd_pick_inter_mode_sb(cpi, tile_data, x, rd_cost, bsize, ctx,
                                       best_rd.rdcost);
          break;
        case PICK_MODE_FAST_NONRD:
          av1_fast_nonrd_pick_inter_mode_sb(cpi, tile_data, x, rd_cost, bsize,
                                            ctx, best_rd.rdcost);
          break;
        default: assert(0 && "Unknown pick mode type.");
      }
    }
#if CONFIG_COLLECT_COMPONENT_TIMING
    end_timing(cpi, av1_rd_pick_inter_mode_sb_time);
#endif
  }

  // Examine the resulting rate and for AQ mode 2 make a segment choice.
  if ((rd_cost->rate != INT_MAX) && (aq_mode == COMPLEXITY_AQ) &&
      (bsize >= BLOCK_16X16) &&
      (cm->current_frame.frame_type == KEY_FRAME ||
       cpi->refresh_alt_ref_frame || cpi->refresh_alt2_ref_frame ||
       (cpi->refresh_golden_frame && !cpi->rc.is_src_frame_alt_ref))) {
    av1_caq_select_segment(cpi, x, bsize, mi_row, mi_col, rd_cost->rate);
  }

  x->rdmult = orig_rdmult;

  // TODO(jingning) The rate-distortion optimization flow needs to be
  // refactored to provide proper exit/return handle.
  if (rd_cost->rate == INT_MAX) rd_cost->rdcost = INT64_MAX;

  ctx->rd_stats.rate = rd_cost->rate;
  ctx->rd_stats.dist = rd_cost->dist;
  ctx->rd_stats.rdcost = rd_cost->rdcost;

#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, rd_pick_sb_modes_time);
#endif
}

#if CONFIG_NEW_INTER_MODES
static void update_drl_index_stats(FRAME_CONTEXT *fc, FRAME_COUNTS *counts,
                                   const AV1_COMMON *cm,
                                   const MB_MODE_INFO *mbmi,
                                   const MB_MODE_INFO_EXT *mbmi_ext,
                                   int16_t mode_ctx, uint8_t allow_update_cdf) {
  (void)cm;
#if !CONFIG_ENTROPY_STATS
  (void)counts;
#endif  // !CONFIG_ENTROPY_STATS
  assert(have_drl_index(mbmi->mode));
  uint8_t ref_frame_type = av1_ref_frame_type(mbmi->ref_frame);
#if CONFIG_FLEX_MVRES && ADJUST_DRL_FLEX_MVRES
  if (mbmi->pb_mv_precision < mbmi->max_mv_precision &&
      (mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV)) {
    assert(mbmi->ref_mv_idx_adj < mbmi_ext->ref_mv_count_adj);
    assert(mbmi->ref_mv_idx_adj < MAX_DRL_BITS + 1);
    int range_adj = AOMMIN(mbmi_ext->ref_mv_count_adj - 1, MAX_DRL_BITS);
    for (int idx = 0; idx < range_adj; ++idx) {
      aom_cdf_prob *drl_cdf =
          av1_get_drl_cdf(mode_ctx, fc, mbmi->mode, mbmi_ext->weight_adj, idx);
      if (allow_update_cdf) update_cdf(drl_cdf, mbmi->ref_mv_idx_adj != idx, 2);
      if (mbmi->ref_mv_idx_adj == idx) break;
    }
    return;
  }
#endif  // CONFIG_FLEX_MVRES && ADJUST_DRL_FLEX_MVRES
  assert(mbmi->ref_mv_idx < MAX_DRL_BITS + 1);
  int range = AOMMIN(mbmi_ext->ref_mv_count[ref_frame_type] - 1, MAX_DRL_BITS);
  for (int idx = 0; idx < range; ++idx) {
    aom_cdf_prob *drl_cdf = av1_get_drl_cdf(
        mode_ctx, fc, mbmi->mode, mbmi_ext->weight[ref_frame_type], idx);
#if CONFIG_ENTROPY_STATS
    int drl_ctx = av1_drl_ctx(mbmi_ext->weight[ref_frame_type], idx);
    switch (mbmi->ref_mv_idx) {
      case 0: counts->drl0_mode[drl_ctx][mbmi->ref_mv_idx != idx]++; break;
      case 1: counts->drl1_mode[drl_ctx][mbmi->ref_mv_idx != idx]++; break;
      default: counts->drl2_mode[drl_ctx][mbmi->ref_mv_idx != idx]++; break;
    }
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) update_cdf(drl_cdf, mbmi->ref_mv_idx != idx, 2);
    if (mbmi->ref_mv_idx == idx) break;
  }
}
#endif  // CONFIG_NEW_INTER_MODES

static void update_inter_mode_stats(FRAME_CONTEXT *fc, FRAME_COUNTS *counts,
                                    PREDICTION_MODE mode, int16_t mode_context,
                                    uint8_t allow_update_cdf) {
  (void)counts;

  int16_t mode_ctx = mode_context & NEWMV_CTX_MASK;
  if (mode == NEWMV) {
#if CONFIG_ENTROPY_STATS
    ++counts->newmv_mode[mode_ctx][0];
#endif
    if (allow_update_cdf) update_cdf(fc->newmv_cdf[mode_ctx], 0, 2);
    return;
  } else {
#if CONFIG_ENTROPY_STATS
    ++counts->newmv_mode[mode_ctx][1];
#endif
    if (allow_update_cdf) update_cdf(fc->newmv_cdf[mode_ctx], 1, 2);

    mode_ctx = (mode_context >> GLOBALMV_OFFSET) & GLOBALMV_CTX_MASK;
    if (mode == GLOBALMV) {
#if CONFIG_ENTROPY_STATS
      ++counts->zeromv_mode[mode_ctx][0];
#endif
      if (allow_update_cdf) update_cdf(fc->zeromv_cdf[mode_ctx], 0, 2);
      return;
    } else {
#if CONFIG_ENTROPY_STATS
      ++counts->zeromv_mode[mode_ctx][1];
#endif
      if (allow_update_cdf) update_cdf(fc->zeromv_cdf[mode_ctx], 1, 2);
#if !CONFIG_NEW_INTER_MODES
      mode_ctx = (mode_context >> REFMV_OFFSET) & REFMV_CTX_MASK;
#if CONFIG_ENTROPY_STATS
      ++counts->refmv_mode[mode_ctx][mode != NEARESTMV];
#endif
      if (allow_update_cdf) {
        update_cdf(fc->refmv_cdf[mode_ctx], mode != NEARESTMV, 2);
      }
#endif  // !CONFIG_NEW_INTER_MODES
    }
  }
}

static void update_palette_cdf(MACROBLOCKD *xd, const MB_MODE_INFO *const mbmi,
                               FRAME_COUNTS *counts, uint8_t allow_update_cdf) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
  const int palette_bsize_ctx = av1_get_palette_bsize_ctx(bsize);

  (void)counts;

  if (mbmi->mode == DC_PRED) {
    const int n = pmi->palette_size[0];
    const int palette_mode_ctx = av1_get_palette_mode_ctx(xd);

#if CONFIG_ENTROPY_STATS
    ++counts->palette_y_mode[palette_bsize_ctx][palette_mode_ctx][n > 0];
#endif
    if (allow_update_cdf)
      update_cdf(fc->palette_y_mode_cdf[palette_bsize_ctx][palette_mode_ctx],
                 n > 0, 2);
    if (n > 0) {
#if CONFIG_ENTROPY_STATS
      ++counts->palette_y_size[palette_bsize_ctx][n - PALETTE_MIN_SIZE];
#endif
      if (allow_update_cdf) {
        update_cdf(fc->palette_y_size_cdf[palette_bsize_ctx],
                   n - PALETTE_MIN_SIZE, PALETTE_SIZES);
      }
    }
  }

  if (mbmi->uv_mode == UV_DC_PRED) {
    const int n = pmi->palette_size[1];
    const int palette_uv_mode_ctx = (pmi->palette_size[0] > 0);

#if CONFIG_ENTROPY_STATS
    ++counts->palette_uv_mode[palette_uv_mode_ctx][n > 0];
#endif
    if (allow_update_cdf)
      update_cdf(fc->palette_uv_mode_cdf[palette_uv_mode_ctx], n > 0, 2);

    if (n > 0) {
#if CONFIG_ENTROPY_STATS
      ++counts->palette_uv_size[palette_bsize_ctx][n - PALETTE_MIN_SIZE];
#endif
      if (allow_update_cdf) {
        update_cdf(fc->palette_uv_size_cdf[palette_bsize_ctx],
                   n - PALETTE_MIN_SIZE, PALETTE_SIZES);
      }
    }
  }
}

static void sum_intra_stats(const AV1_COMMON *const cm, FRAME_COUNTS *counts,
                            MACROBLOCKD *xd, const MB_MODE_INFO *const mbmi,
                            const MB_MODE_INFO *above_mi,
                            const MB_MODE_INFO *left_mi,
#if CONFIG_INTRA_ENTROPY
                            const MB_MODE_INFO *aboveleft_mi,
#endif  // CONFIG_INTRA_ENTROPY
                            const int intraonly, uint8_t allow_update_cdf) {
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const PREDICTION_MODE y_mode = mbmi->mode;
  const UV_PREDICTION_MODE uv_mode = mbmi->uv_mode;
  (void)counts;
  const BLOCK_SIZE bsize = mbmi->sb_type;

  if (intraonly) {
#if CONFIG_ENTROPY_STATS
    const PREDICTION_MODE above = av1_above_block_mode(above_mi);
    const PREDICTION_MODE left = av1_left_block_mode(left_mi);
    const int above_ctx = intra_mode_context[above];
    const int left_ctx = intra_mode_context[left];
    ++counts->kf_y_mode[above_ctx][left_ctx][y_mode];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
#if CONFIG_INTRA_ENTROPY
      NN_CONFIG_EM *nn_model = &(fc->intra_y_mode);
      av1_get_intra_block_feature(nn_model->sparse_features,
                                  nn_model->dense_features, above_mi, left_mi,
                                  aboveleft_mi);
      av1_nn_predict_em(nn_model);
      av1_nn_backprop_em(nn_model, y_mode);
      av1_nn_update_em(nn_model, nn_model->lr);
#else
#if CONFIG_DERIVED_INTRA_MODE
      const int is_dr_mode = av1_is_directional_mode(y_mode);
      update_cdf(get_kf_is_dr_mode_cdf(fc, above_mi, left_mi), is_dr_mode, 2);
      if (is_dr_mode) {
        int update_intra_mode;
        if (av1_enable_derived_intra_mode(xd, bsize)) {
          update_intra_mode = !mbmi->use_derived_intra_mode[0];
          if (allow_update_cdf) {
            update_cdf(get_derived_intra_mode_cdf(fc, above_mi, left_mi, 0),
                       mbmi->use_derived_intra_mode[0], 2);
          }
        } else {
          update_intra_mode = 1;
        }
        if (update_intra_mode) {
          update_cdf(get_kf_dr_mode_cdf(fc, above_mi, left_mi),
                     dr_mode_to_index[y_mode], DIRECTIONAL_MODES);
        }
      } else {
        update_cdf(get_kf_none_dr_mode_cdf(fc, above_mi, left_mi),
                   none_dr_mode_to_index[y_mode], NONE_DIRECTIONAL_MODES);
      }
#else
      update_cdf(get_y_mode_cdf(fc, above_mi, left_mi), y_mode, INTRA_MODES);
#endif  // CONFIG_DERIVED_INTRA_MODE
#endif  // CONFIG_INTRA_ENTROPY
    }
  } else {
#if CONFIG_ENTROPY_STATS
    ++counts->y_mode[size_group_lookup[bsize]][y_mode];
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
#if CONFIG_DERIVED_INTRA_MODE
      const int ctx = size_group_lookup[bsize];
      const int is_dr_mode = av1_is_directional_mode(y_mode);
      update_cdf(fc->bf_is_dr_mode_cdf[ctx], is_dr_mode, 2);
      if (is_dr_mode) {
        if (av1_enable_derived_intra_mode(xd, bsize)) {
          update_cdf(get_derived_intra_mode_cdf(fc, above_mi, left_mi, 0),
                     mbmi->use_derived_intra_mode[0], 2);
        } else {
          assert(!mbmi->use_derived_intra_mode[0]);
        }
        if (!mbmi->use_derived_intra_mode[0]) {
          update_cdf(fc->bf_dr_mode_cdf[ctx], dr_mode_to_index[y_mode],
                     DIRECTIONAL_MODES);
        }
      } else {
        update_cdf(fc->bf_none_dr_mode_cdf[ctx], none_dr_mode_to_index[y_mode],
                   NONE_DIRECTIONAL_MODES);
      }
#else
      update_cdf(fc->y_mode_cdf[size_group_lookup[bsize]], y_mode, INTRA_MODES);
#endif  // CONFIG_DERIVED_INTRA_MODE
    }
  }

  if (av1_filter_intra_allowed(cm, mbmi)) {
    const int use_filter_intra_mode =
        mbmi->filter_intra_mode_info.use_filter_intra;
#if CONFIG_ENTROPY_STATS
    ++counts->filter_intra[mbmi->sb_type][use_filter_intra_mode];
    if (use_filter_intra_mode) {
      ++counts
            ->filter_intra_mode[mbmi->filter_intra_mode_info.filter_intra_mode];
    }
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(fc->filter_intra_cdfs[mbmi->sb_type], use_filter_intra_mode,
                 2);
      if (use_filter_intra_mode) {
        update_cdf(fc->filter_intra_mode_cdf,
                   mbmi->filter_intra_mode_info.filter_intra_mode,
                   FILTER_INTRA_MODES);
      }
    }
  }
#if CONFIG_ADAPT_FILTER_INTRA
  if (av1_adapt_filter_intra_allowed(cm, mbmi)) {
    const int use_adapt_filter_intra_mode =
        mbmi->adapt_filter_intra_mode_info.use_adapt_filter_intra;
#if CONFIG_ENTROPY_STATS
    ++counts->adapt_filter_intra[mbmi->sb_type][use_adapt_filter_intra_mode];
    if (use_adapt_filter_intra_mode) {
      ++counts->adapt_filter_intra_mode[mbmi->adapt_filter_intra_mode_info
                                            .adapt_filter_intra_mode];
    }
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf) {
      update_cdf(fc->adapt_filter_intra_cdfs[mbmi->sb_type],
                 use_adapt_filter_intra_mode, 2);
      if (use_adapt_filter_intra_mode) {
        update_cdf(fc->adapt_filter_intra_mode_cdf,
                   mbmi->adapt_filter_intra_mode_info.adapt_filter_intra_mode,
                   USED_ADAPT_FILTER_INTRA_MODES);
      }
    }
  }
#endif  // CONFIG_ADAPT_FILTER_INTRA
  if (av1_is_directional_mode(mbmi->mode) && av1_use_angle_delta(bsize)) {
#if CONFIG_ENTROPY_STATS
    ++counts->angle_delta[mbmi->mode - V_PRED]
                         [mbmi->angle_delta[PLANE_TYPE_Y] + MAX_ANGLE_DELTA];
#endif
    if (allow_update_cdf) {
#if CONFIG_DERIVED_INTRA_MODE
      if (!mbmi->use_derived_intra_mode[0])
#endif  // CONFIG_DERIVED_INTRA_MODE
      {
        update_cdf(fc->angle_delta_cdf[mbmi->mode - V_PRED],
                   mbmi->angle_delta[PLANE_TYPE_Y] + MAX_ANGLE_DELTA,
                   2 * MAX_ANGLE_DELTA + 1);
      }
    }
  }

  if (!mbmi->chroma_ref_info.is_chroma_ref) return;

#if CONFIG_ENTROPY_STATS
  ++counts->uv_mode[is_cfl_allowed(xd)][y_mode][uv_mode];
#endif  // CONFIG_ENTROPY_STATS
  if (allow_update_cdf) {
#if CONFIG_INTRA_ENTROPY
    NN_CONFIG_EM *nn_model = &(fc->intra_uv_mode);
    av1_get_intra_uv_block_feature(nn_model->sparse_features,
                                   nn_model->dense_features, y_mode,
                                   is_cfl_allowed(xd), above_mi, left_mi);
    av1_nn_predict_em(nn_model);
    av1_nn_backprop_em(nn_model, uv_mode);
    av1_nn_update_em(nn_model, nn_model->lr);
#else
#if CONFIG_DERIVED_INTRA_MODE
    if (av1_enable_derived_intra_mode(xd, bsize)) {
      update_cdf(fc->uv_derived_intra_mode_cdf[mbmi->use_derived_intra_mode[0]],
                 mbmi->use_derived_intra_mode[1], 2);
    }
    if (!mbmi->use_derived_intra_mode[1]) {
      const CFL_ALLOWED_TYPE cfl_allowed = is_cfl_allowed(xd);
      update_cdf(fc->uv_mode_cdf[cfl_allowed][y_mode], uv_mode,
                 UV_INTRA_MODES - !cfl_allowed);
    }
#else
    const CFL_ALLOWED_TYPE cfl_allowed = is_cfl_allowed(xd);
    update_cdf(fc->uv_mode_cdf[cfl_allowed][y_mode], uv_mode,
               UV_INTRA_MODES - !cfl_allowed);
#endif  // CONFIG_DERIVED_INTRA_MODE
#endif  // CONFIG_INTRA_ENTROPY
  }
  if (uv_mode == UV_CFL_PRED) {
    const int8_t joint_sign = mbmi->cfl_alpha_signs;
    const uint8_t idx = mbmi->cfl_alpha_idx;

#if CONFIG_ENTROPY_STATS
    ++counts->cfl_sign[joint_sign];
#endif
    if (allow_update_cdf)
      update_cdf(fc->cfl_sign_cdf, joint_sign, CFL_JOINT_SIGNS);
    if (CFL_SIGN_U(joint_sign) != CFL_SIGN_ZERO) {
      aom_cdf_prob *cdf_u = fc->cfl_alpha_cdf[CFL_CONTEXT_U(joint_sign)];

#if CONFIG_ENTROPY_STATS
      ++counts->cfl_alpha[CFL_CONTEXT_U(joint_sign)][CFL_IDX_U(idx)];
#endif
      if (allow_update_cdf)
        update_cdf(cdf_u, CFL_IDX_U(idx), CFL_ALPHABET_SIZE);
    }
    if (CFL_SIGN_V(joint_sign) != CFL_SIGN_ZERO) {
      aom_cdf_prob *cdf_v = fc->cfl_alpha_cdf[CFL_CONTEXT_V(joint_sign)];

#if CONFIG_ENTROPY_STATS
      ++counts->cfl_alpha[CFL_CONTEXT_V(joint_sign)][CFL_IDX_V(idx)];
#endif
      if (allow_update_cdf)
        update_cdf(cdf_v, CFL_IDX_V(idx), CFL_ALPHABET_SIZE);
    }
  }
  if (av1_is_directional_mode(get_uv_mode(uv_mode)) &&
#if CONFIG_DERIVED_INTRA_MODE
      !mbmi->use_derived_intra_mode[1] &&
#endif  // CONFIG_DERIVED_INTRA_MODE
      av1_use_angle_delta(bsize)) {
#if CONFIG_ENTROPY_STATS
    ++counts->angle_delta[uv_mode - UV_V_PRED]
                         [mbmi->angle_delta[PLANE_TYPE_UV] + MAX_ANGLE_DELTA];
#endif
    if (allow_update_cdf) {
      update_cdf(fc->angle_delta_cdf[uv_mode - UV_V_PRED],
                 mbmi->angle_delta[PLANE_TYPE_UV] + MAX_ANGLE_DELTA,
                 2 * MAX_ANGLE_DELTA + 1);
    }
  }
  if (av1_allow_palette(cm->allow_screen_content_tools, bsize))
    update_palette_cdf(xd, mbmi, counts, allow_update_cdf);
}

static void update_stats(const AV1_COMMON *const cm, TileDataEnc *tile_data,
                         ThreadData *td, int mi_row, int mi_col) {
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const MB_MODE_INFO *const mbmi = xd->mi[0];
  const MB_MODE_INFO_EXT *const mbmi_ext = x->mbmi_ext;
  const CurrentFrame *const current_frame = &cm->current_frame;
  const BLOCK_SIZE bsize = mbmi->sb_type;
  FRAME_CONTEXT *fc = xd->tile_ctx;
  const uint8_t allow_update_cdf = tile_data->allow_update_cdf;

  // delta quant applies to both intra and inter
  const int super_block_upper_left =
      ((mi_row & (cm->seq_params.mib_size - 1)) == 0) &&
      ((mi_col & (cm->seq_params.mib_size - 1)) == 0);

  const int seg_ref_active =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_REF_FRAME);

  if (current_frame->skip_mode_info.skip_mode_flag && !seg_ref_active &&
      is_comp_ref_allowed(bsize)) {
    const int skip_mode_ctx = av1_get_skip_mode_context(xd);
#if CONFIG_ENTROPY_STATS
    td->counts->skip_mode[skip_mode_ctx][mbmi->skip_mode]++;
#endif  // CONFIG_ENTROPY_STATS
    if (allow_update_cdf)
      update_cdf(fc->skip_mode_cdfs[skip_mode_ctx], mbmi->skip_mode, 2);
  }

  if (!mbmi->skip_mode) {
    if (!seg_ref_active) {
      const int skip_ctx = av1_get_skip_context(xd);
#if CONFIG_ENTROPY_STATS
      td->counts->skip[skip_ctx][mbmi->skip]++;
#endif  // CONFIG_ENTROPY_STATS
      if (allow_update_cdf) update_cdf(fc->skip_cdfs[skip_ctx], mbmi->skip, 2);
    }
  }

  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  if (delta_q_info->delta_q_present_flag &&
      (bsize != cm->seq_params.sb_size || !mbmi->skip) &&
      super_block_upper_left) {
#if CONFIG_ENTROPY_STATS
    const int dq =
        (mbmi->current_qindex - xd->current_qindex) / delta_q_info->delta_q_res;
    const int absdq = abs(dq);
    for (int i = 0; i < AOMMIN(absdq, DELTA_Q_SMALL); ++i) {
      td->counts->delta_q[i][1]++;
    }
    if (absdq < DELTA_Q_SMALL) td->counts->delta_q[absdq][0]++;
#endif  // CONFIG_ENTROPY_STATS
    xd->current_qindex = mbmi->current_qindex;
    if (delta_q_info->delta_lf_present_flag) {
      if (delta_q_info->delta_lf_multi) {
        const int frame_lf_count =
            av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
        for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
#if CONFIG_ENTROPY_STATS
          const int delta_lf = (mbmi->delta_lf[lf_id] - xd->delta_lf[lf_id]) /
                               delta_q_info->delta_lf_res;
          const int abs_delta_lf = abs(delta_lf);
          for (int i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
            td->counts->delta_lf_multi[lf_id][i][1]++;
          }
          if (abs_delta_lf < DELTA_LF_SMALL)
            td->counts->delta_lf_multi[lf_id][abs_delta_lf][0]++;
#endif  // CONFIG_ENTROPY_STATS
          xd->delta_lf[lf_id] = mbmi->delta_lf[lf_id];
        }
      } else {
#if CONFIG_ENTROPY_STATS
        const int delta_lf =
            (mbmi->delta_lf_from_base - xd->delta_lf_from_base) /
            delta_q_info->delta_lf_res;
        const int abs_delta_lf = abs(delta_lf);
        for (int i = 0; i < AOMMIN(abs_delta_lf, DELTA_LF_SMALL); ++i) {
          td->counts->delta_lf[i][1]++;
        }
        if (abs_delta_lf < DELTA_LF_SMALL)
          td->counts->delta_lf[abs_delta_lf][0]++;
#endif  // CONFIG_ENTROPY_STATS
        xd->delta_lf_from_base = mbmi->delta_lf_from_base;
      }
    }
  }

  if (!is_inter_block(mbmi)) {
    sum_intra_stats(cm, td->counts, xd, mbmi, xd->above_mbmi, xd->left_mbmi,
#if CONFIG_INTRA_ENTROPY
                    xd->aboveleft_mbmi,
#endif  // CONFIG_INTRA_ENTROPY
                    frame_is_intra_only(cm), allow_update_cdf);
  }

  if (av1_allow_intrabc(cm)) {
    if (allow_update_cdf)
      update_cdf(fc->intrabc_cdf, is_intrabc_block(mbmi), 2);
#if CONFIG_ENTROPY_STATS
    ++td->counts->intrabc[is_intrabc_block(mbmi)];
#endif  // CONFIG_ENTROPY_STATS
  }

  if (!frame_is_intra_only(cm)) {
    RD_COUNTS *rdc = &td->rd_counts;

    FRAME_COUNTS *const counts = td->counts;

    if (mbmi->skip_mode) {
      rdc->skip_mode_used_flag = 1;
      if (current_frame->reference_mode == REFERENCE_MODE_SELECT) {
        assert(has_second_ref(mbmi));
        rdc->compound_ref_used_flag = 1;
      }
      set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
      return;
    }

    const int inter_block = is_inter_block(mbmi);

    if (!seg_ref_active) {
#if CONFIG_ENTROPY_STATS
      counts->intra_inter[av1_get_intra_inter_context(xd)][inter_block]++;
#endif  // CONFIG_ENTROPY_STATS
      if (allow_update_cdf) {
        update_cdf(fc->intra_inter_cdf[av1_get_intra_inter_context(xd)],
                   inter_block, 2);
      }
      // If the segment reference feature is enabled we have only a single
      // reference frame allowed for the segment so exclude it from
      // the reference frame counts used to work out probabilities.
      if (inter_block) {
        const MV_REFERENCE_FRAME ref0 = mbmi->ref_frame[0];
        const MV_REFERENCE_FRAME ref1 = mbmi->ref_frame[1];

        av1_collect_neighbors_ref_counts(xd);

        if (current_frame->reference_mode == REFERENCE_MODE_SELECT) {
          if (has_second_ref(mbmi))
            // This flag is also updated for 4x4 blocks
            rdc->compound_ref_used_flag = 1;
          if (is_comp_ref_allowed(bsize)) {
#if CONFIG_ENTROPY_STATS
            counts->comp_inter[av1_get_reference_mode_context(xd)]
                              [has_second_ref(mbmi)]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(av1_get_reference_mode_cdf(xd), has_second_ref(mbmi),
                         2);
            }
          }
        }

        int update_single_ref_cdf = 1;
#if CONFIG_MISC_CHANGES
        if (cm->only_one_ref_available) update_single_ref_cdf = 0;
#endif  // CONFIG_MISC_CHANGES
        if (has_second_ref(mbmi)) {
          const COMP_REFERENCE_TYPE comp_ref_type = has_uni_comp_refs(mbmi)
                                                        ? UNIDIR_COMP_REFERENCE
                                                        : BIDIR_COMP_REFERENCE;
          if (allow_update_cdf) {
            update_cdf(av1_get_comp_reference_type_cdf(xd), comp_ref_type,
                       COMP_REFERENCE_TYPES);
          }
#if CONFIG_ENTROPY_STATS
          counts->comp_ref_type[av1_get_comp_reference_type_context(xd)]
                               [comp_ref_type]++;
#endif  // CONFIG_ENTROPY_STATS

          if (comp_ref_type == UNIDIR_COMP_REFERENCE) {
            const int bit = (ref0 == BWDREF_FRAME);
            if (allow_update_cdf)
              update_cdf(av1_get_pred_cdf_uni_comp_ref_p(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
            counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p(xd)][0]
                                [bit]++;
#endif  // CONFIG_ENTROPY_STATS
            if (!bit) {
              const int bit1 = (ref1 == LAST3_FRAME || ref1 == GOLDEN_FRAME);
              if (allow_update_cdf)
                update_cdf(av1_get_pred_cdf_uni_comp_ref_p1(xd), bit1, 2);
#if CONFIG_ENTROPY_STATS
              counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p1(xd)][1]
                                  [bit1]++;
#endif  // CONFIG_ENTROPY_STATS
              if (bit1) {
                if (allow_update_cdf) {
                  update_cdf(av1_get_pred_cdf_uni_comp_ref_p2(xd),
                             ref1 == GOLDEN_FRAME, 2);
                }
#if CONFIG_ENTROPY_STATS
                counts->uni_comp_ref[av1_get_pred_context_uni_comp_ref_p2(xd)]
                                    [2][ref1 == GOLDEN_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
              }
            }
          } else {
            const int bit = (ref0 == GOLDEN_FRAME || ref0 == LAST3_FRAME);
            if (allow_update_cdf)
              update_cdf(av1_get_pred_cdf_comp_ref_p(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
            counts->comp_ref[av1_get_pred_context_comp_ref_p(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
            if (!bit) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_comp_ref_p1(xd),
                           ref0 == LAST2_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->comp_ref[av1_get_pred_context_comp_ref_p1(xd)][1]
                              [ref0 == LAST2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            } else {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_comp_ref_p2(xd),
                           ref0 == GOLDEN_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->comp_ref[av1_get_pred_context_comp_ref_p2(xd)][2]
                              [ref0 == GOLDEN_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
            if (allow_update_cdf) {
              update_cdf(av1_get_pred_cdf_comp_bwdref_p(xd),
                         ref1 == ALTREF_FRAME, 2);
            }
#if CONFIG_ENTROPY_STATS
            counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p(xd)][0]
                               [ref1 == ALTREF_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            if (ref1 != ALTREF_FRAME) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_comp_bwdref_p1(xd),
                           ref1 == ALTREF2_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->comp_bwdref[av1_get_pred_context_comp_bwdref_p1(xd)][1]
                                 [ref1 == ALTREF2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          }
        } else if (update_single_ref_cdf) {
          const int bit = (ref0 >= BWDREF_FRAME);
          if (allow_update_cdf)
            update_cdf(av1_get_pred_cdf_single_ref_p1(xd), bit, 2);
#if CONFIG_ENTROPY_STATS
          counts->single_ref[av1_get_pred_context_single_ref_p1(xd)][0][bit]++;
#endif  // CONFIG_ENTROPY_STATS
          if (bit) {
            assert(ref0 <= ALTREF_FRAME);
            if (allow_update_cdf) {
              update_cdf(av1_get_pred_cdf_single_ref_p2(xd),
                         ref0 == ALTREF_FRAME, 2);
            }
#if CONFIG_ENTROPY_STATS
            counts->single_ref[av1_get_pred_context_single_ref_p2(xd)][1]
                              [ref0 == ALTREF_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            if (ref0 != ALTREF_FRAME) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_single_ref_p6(xd),
                           ref0 == ALTREF2_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->single_ref[av1_get_pred_context_single_ref_p6(xd)][5]
                                [ref0 == ALTREF2_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          } else {
            const int bit1 = !(ref0 == LAST2_FRAME || ref0 == LAST_FRAME);
            if (allow_update_cdf)
              update_cdf(av1_get_pred_cdf_single_ref_p3(xd), bit1, 2);
#if CONFIG_ENTROPY_STATS
            counts
                ->single_ref[av1_get_pred_context_single_ref_p3(xd)][2][bit1]++;
#endif  // CONFIG_ENTROPY_STATS
            if (!bit1) {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_single_ref_p4(xd),
                           ref0 != LAST_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->single_ref[av1_get_pred_context_single_ref_p4(xd)][3]
                                [ref0 != LAST_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            } else {
              if (allow_update_cdf) {
                update_cdf(av1_get_pred_cdf_single_ref_p5(xd),
                           ref0 != LAST3_FRAME, 2);
              }
#if CONFIG_ENTROPY_STATS
              counts->single_ref[av1_get_pred_context_single_ref_p5(xd)][4]
                                [ref0 != LAST3_FRAME]++;
#endif  // CONFIG_ENTROPY_STATS
            }
          }
        }

        if (cm->seq_params.enable_interintra_compound &&
            is_interintra_allowed(mbmi)) {
          const int bsize_group = size_group_lookup[bsize];
          if (mbmi->ref_frame[1] == INTRA_FRAME) {
#if CONFIG_ENTROPY_STATS
            counts->interintra[bsize_group][1]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf)
              update_cdf(fc->interintra_cdf[bsize_group], 1, 2);
#if CONFIG_ENTROPY_STATS
            counts->interintra_mode[bsize_group][mbmi->interintra_mode]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
#if CONFIG_DERIVED_INTRA_MODE
              if (av1_enable_derived_intra_mode(xd, bsize)) {
                update_cdf(get_derived_intra_mode_cdf(fc, xd->above_mbmi,
                                                      xd->left_mbmi, 1),
                           mbmi->use_derived_intra_mode[0], 2);
              }
              if (!mbmi->use_derived_intra_mode[0])
#endif  // CONFIG_DERIVED_INTRA_MODE
              {
                update_cdf(fc->interintra_mode_cdf[bsize_group],
                           mbmi->interintra_mode, INTERINTRA_MODES);
              }
            }
            if (is_interintra_wedge_used(bsize)) {
#if CONFIG_ENTROPY_STATS
              counts->wedge_interintra[bsize][mbmi->use_wedge_interintra]++;
#endif  // CONFIG_ENTROPY_STATS
              if (allow_update_cdf) {
                update_cdf(fc->wedge_interintra_cdf[bsize],
                           mbmi->use_wedge_interintra, 2);
              }
              if (mbmi->use_wedge_interintra) {
#if CONFIG_ENTROPY_STATS
                counts->wedge_idx[bsize][mbmi->interintra_wedge_index]++;
#endif  // CONFIG_ENTROPY_STATS
                if (allow_update_cdf) {
                  update_cdf(fc->wedge_idx_cdf[bsize],
                             mbmi->interintra_wedge_index, 16);
                }
              }
            }
          } else {
#if CONFIG_ENTROPY_STATS
            counts->interintra[bsize_group][0]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf)
              update_cdf(fc->interintra_cdf[bsize_group], 0, 2);
          }
        }

        set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
        const MOTION_MODE motion_allowed =
            cm->switchable_motion_mode
                ? motion_mode_allowed(xd->global_motion, xd, mbmi,
                                      cm->allow_warped_motion)
                : SIMPLE_TRANSLATION;
        if (mbmi->ref_frame[1] != INTRA_FRAME) {
          if (motion_allowed == WARPED_CAUSAL) {
#if CONFIG_ENTROPY_STATS
            counts->motion_mode[bsize][mbmi->motion_mode]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(fc->motion_mode_cdf[bsize], mbmi->motion_mode,
                         MOTION_MODES);
            }
          } else if (motion_allowed == OBMC_CAUSAL) {
#if CONFIG_ENTROPY_STATS
            counts->obmc[bsize][mbmi->motion_mode == OBMC_CAUSAL]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(fc->obmc_cdf[bsize], mbmi->motion_mode == OBMC_CAUSAL,
                         2);
            }
          }
        }

        if (has_second_ref(mbmi)) {
          assert(current_frame->reference_mode != SINGLE_REFERENCE &&
                 is_inter_compound_mode(mbmi->mode) &&
                 mbmi->motion_mode == SIMPLE_TRANSLATION);

          const int masked_compound_used =
              is_any_masked_compound_used(bsize) &&
              cm->seq_params.enable_masked_compound;
          if (masked_compound_used) {
            const int comp_group_idx_ctx = get_comp_group_idx_context(xd);
#if CONFIG_ENTROPY_STATS
            ++counts->comp_group_idx[comp_group_idx_ctx][mbmi->comp_group_idx];
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(fc->comp_group_idx_cdf[comp_group_idx_ctx],
                         mbmi->comp_group_idx, 2);
            }
          }

          if (mbmi->comp_group_idx == 0) {
            const int comp_index_ctx = get_comp_index_context(cm, xd);
#if CONFIG_ENTROPY_STATS
            ++counts->compound_index[comp_index_ctx][mbmi->compound_idx];
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(fc->compound_index_cdf[comp_index_ctx],
                         mbmi->compound_idx, 2);
            }
          } else {
            assert(masked_compound_used);
            if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#if CONFIG_ENTROPY_STATS
              ++counts->compound_type[bsize][mbmi->interinter_comp.type -
                                             COMPOUND_WEDGE];
#endif  // CONFIG_ENTROPY_STATS
              if (allow_update_cdf) {
                update_cdf(fc->compound_type_cdf[bsize],
                           mbmi->interinter_comp.type - COMPOUND_WEDGE,
                           MASKED_COMPOUND_TYPES);
              }
            }
          }
        }
        if (mbmi->interinter_comp.type == COMPOUND_WEDGE) {
          if (is_interinter_compound_used(COMPOUND_WEDGE, bsize)) {
#if CONFIG_ENTROPY_STATS
            counts->wedge_idx[bsize][mbmi->interinter_comp.wedge_index]++;
#endif  // CONFIG_ENTROPY_STATS
            if (allow_update_cdf) {
              update_cdf(fc->wedge_idx_cdf[bsize],
                         mbmi->interinter_comp.wedge_index, 16);
            }
          }
        }
      }
    }

    if (inter_block && cm->interp_filter == SWITCHABLE &&
        mbmi->motion_mode != WARPED_CAUSAL &&
        !is_nontrans_global_motion(xd, xd->mi[0])) {
      update_filter_type_cdf(allow_update_cdf, xd, mbmi);
    }

    if (inter_block &&
        !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP)) {
      int16_t mode_ctx;
      const PREDICTION_MODE mode = mbmi->mode;

      mode_ctx =
          av1_mode_context_analyzer(mbmi_ext->mode_context, mbmi->ref_frame);
      if (has_second_ref(mbmi)) {
#if CONFIG_ENTROPY_STATS
        ++counts->inter_compound_mode[mode_ctx][INTER_COMPOUND_OFFSET(mode)];
#endif  // CONFIG_ENTROPY_STATS
        if (allow_update_cdf)
          update_cdf(fc->inter_compound_mode_cdf[mode_ctx],
                     INTER_COMPOUND_OFFSET(mode), INTER_COMPOUND_MODES);
      } else {
        update_inter_mode_stats(fc, counts, mode, mode_ctx, allow_update_cdf);
      }
      const int new_mv = mbmi->mode == NEWMV || mbmi->mode == NEW_NEWMV;

#if CONFIG_NEW_INTER_MODES
      if (have_drl_index(mbmi->mode)) {
        update_drl_index_stats(fc, counts, cm, mbmi, mbmi_ext, mode_ctx,
                               allow_update_cdf);
      }
      if (have_newmv_in_inter_mode(mbmi->mode)) {
#if CONFIG_FLEX_MVRES
        if (allow_update_cdf &&
            is_pb_mv_precision_active(cm, mbmi->mode, mbmi->max_mv_precision)) {
          const int down_ctx = av1_get_pb_mv_precision_down_context(cm, xd);
          int down = mbmi->max_mv_precision - mbmi->pb_mv_precision;
#if DISALLOW_ONE_DOWN_FLEX_MVRES == 2
          assert((down & 1) == 0);
          const int nsymbs = 2;
          down >>= 1;
#elif DISALLOW_ONE_DOWN_FLEX_MVRES == 1
          assert(down != 1);
          const int nsymbs = mbmi->max_mv_precision;
          down -= (down > 0);
#else
          const int nsymbs = mbmi->max_mv_precision + 1;
#endif  // DISALLOW_ONE_DOWN_FLEX_MVRES
          update_cdf(fc->pb_mv_precision_cdf[down_ctx][mbmi->max_mv_precision -
                                                       MV_SUBPEL_QTR_PRECISION],
                     down, nsymbs);
        }
        assert(mbmi->pb_mv_precision == av1_get_mbmi_mv_precision(cm, mbmi));
#endif  // CONFIG_FLEX_MVRES
        if (new_mv) {
          for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
            const int_mv ref_mv = av1_get_ref_mv(x, ref);
            av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                                mbmi->pb_mv_precision);
          }
        } else {
          const int ref =
              mbmi->mode == NEAR_NEWMV;  // Find which half of the compound
                                         // reference has NEWMV
          const int_mv ref_mv = av1_get_ref_mv(x, ref);
          av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                              mbmi->pb_mv_precision);
        }
      }
#else
      if (have_newmv_in_inter_mode(mbmi->mode)) {
#if CONFIG_FLEX_MVRES
        assert(mbmi->pb_mv_precision <= mbmi->max_mv_precision);
        assert(mbmi->max_mv_precision == xd->sbi->sb_mv_precision);
        if (allow_update_cdf &&
            is_pb_mv_precision_active(cm, mbmi->mode, mbmi->max_mv_precision)) {
          const int down_ctx = av1_get_pb_mv_precision_down_context(cm, xd);
          int down = mbmi->max_mv_precision - mbmi->pb_mv_precision;
#if DISALLOW_ONE_DOWN_FLEX_MVRES == 2
          assert((down & 1) == 0);
          const int nsymbs = 2;
          down >>= 1;
#elif DISALLOW_ONE_DOWN_FLEX_MVRES == 1
          assert(down != 1);
          const int nsymbs = mbmi->max_mv_precision;
          down -= (down > 0);
#else
          const int nsymbs = mbmi->max_mv_precision + 1;
#endif  // DISALLOW_ONE_DOWN_FLEX_MVRES
          update_cdf(fc->pb_mv_precision_cdf[down_ctx][mbmi->max_mv_precision -
                                                       MV_SUBPEL_QTR_PRECISION],
                     down, nsymbs);
        }
        assert(mbmi->pb_mv_precision == av1_get_mbmi_mv_precision(cm, mbmi));
#endif  // CONFIG_FLEX_MVRES

        if (new_mv) {
          for (int ref = 0; ref < 1 + has_second_ref(mbmi); ++ref) {
            const int_mv ref_mv = av1_get_ref_mv(x, ref);
            av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                                mbmi->pb_mv_precision);
          }
        } else {
          const int ref =
              (mbmi->mode == NEAREST_NEWMV || mbmi->mode == NEAR_NEWMV);
          const int_mv ref_mv = av1_get_ref_mv(x, ref);
          av1_update_mv_stats(&mbmi->mv[ref].as_mv, &ref_mv.as_mv, &fc->nmvc,
                              mbmi->pb_mv_precision);
        }
      }
#endif  // CONFIG_NEW_INTER_MODES
    }
  }
}

#if !CONFIG_REALTIME_ONLY
static void restore_context(const AV1_COMMON *cm, MACROBLOCK *x,
                            const RD_SEARCH_MACROBLOCK_CONTEXT *ctx, int mi_row,
                            int mi_col, BLOCK_SIZE bsize,
                            const int num_planes) {
  MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];
  for (p = 0; p < num_planes; p++) {
    int tx_col = mi_col;
    int tx_row = mi_row & MAX_MIB_MASK;
    memcpy(xd->above_context[p] + (tx_col >> xd->plane[p].subsampling_x),
           ctx->a + num_4x4_blocks_wide * p,
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
               xd->plane[p].subsampling_x);
    memcpy(xd->left_context[p] + (tx_row >> xd->plane[p].subsampling_y),
           ctx->l + num_4x4_blocks_high * p,
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
               xd->plane[p].subsampling_y);
  }
  memcpy(xd->above_seg_context + mi_col, ctx->sa,
         sizeof(*xd->above_seg_context) * mi_width);
  memcpy(xd->left_seg_context + (mi_row & MAX_MIB_MASK), ctx->sl,
         sizeof(xd->left_seg_context[0]) * mi_height);
  xd->above_txfm_context = ctx->p_ta;
  xd->left_txfm_context = ctx->p_tl;
  memcpy(xd->above_txfm_context, ctx->ta,
         sizeof(*xd->above_txfm_context) * mi_width);
  memcpy(xd->left_txfm_context, ctx->tl,
         sizeof(*xd->left_txfm_context) * mi_height);

  av1_mark_block_as_not_coded(xd, mi_row, mi_col, bsize,
                              cm->seq_params.sb_size);
}

static void save_context(const MACROBLOCK *x, RD_SEARCH_MACROBLOCK_CONTEXT *ctx,
                         int mi_row, int mi_col, BLOCK_SIZE bsize,
                         const int num_planes) {
  const MACROBLOCKD *xd = &x->e_mbd;
  int p;
  const int num_4x4_blocks_wide =
      block_size_wide[bsize] >> tx_size_wide_log2[0];
  const int num_4x4_blocks_high =
      block_size_high[bsize] >> tx_size_high_log2[0];
  int mi_width = mi_size_wide[bsize];
  int mi_height = mi_size_high[bsize];

  // buffer the above/left context information of the block in search.
  for (p = 0; p < num_planes; ++p) {
    int tx_col = mi_col;
    int tx_row = mi_row & MAX_MIB_MASK;
    memcpy(ctx->a + num_4x4_blocks_wide * p,
           xd->above_context[p] + (tx_col >> xd->plane[p].subsampling_x),
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_wide) >>
               xd->plane[p].subsampling_x);
    memcpy(ctx->l + num_4x4_blocks_high * p,
           xd->left_context[p] + (tx_row >> xd->plane[p].subsampling_y),
           (sizeof(ENTROPY_CONTEXT) * num_4x4_blocks_high) >>
               xd->plane[p].subsampling_y);
  }
  memcpy(ctx->sa, xd->above_seg_context + mi_col,
         sizeof(*xd->above_seg_context) * mi_width);
  memcpy(ctx->sl, xd->left_seg_context + (mi_row & MAX_MIB_MASK),
         sizeof(xd->left_seg_context[0]) * mi_height);
  memcpy(ctx->ta, xd->above_txfm_context,
         sizeof(*xd->above_txfm_context) * mi_width);
  memcpy(ctx->tl, xd->left_txfm_context,
         sizeof(*xd->left_txfm_context) * mi_height);
  ctx->p_ta = xd->above_txfm_context;
  ctx->p_tl = xd->left_txfm_context;
}
#endif  // !CONFIG_REALTIME_ONLY

static void encode_b(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                     ThreadData *td, TOKENEXTRA **tp, int mi_row, int mi_col,
                     RUN_TYPE dry_run, BLOCK_SIZE bsize,
                     PARTITION_TYPE partition,
                     const PICK_MODE_CONTEXT *const ctx, int *rate) {
  TileInfo *const tile = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;

  assert(bsize == ctx->mic.sb_type);
  set_offsets_without_segment_id(cpi, tile, x, mi_row, mi_col, bsize,
                                 &ctx->chroma_ref_info);
  const int origin_mult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);
  MB_MODE_INFO *mbmi = xd->mi[0];
  mbmi->partition = partition;
  update_state(cpi, td, ctx, mi_row, mi_col, bsize, dry_run);

  if (!dry_run) {
    x->mbmi_ext->cb_offset = x->cb_offset;
    assert(x->cb_offset <
           (1 << num_pels_log2_lookup[cpi->common.seq_params.sb_size]));
  }

  encode_superblock(cpi, tile_data, td, tp, dry_run, bsize, rate);

  if (!dry_run) {
    x->cb_offset += block_size_wide[bsize] * block_size_high[bsize];
    if (bsize == cpi->common.seq_params.sb_size && mbmi->skip == 1 &&
        cpi->common.delta_q_info.delta_lf_present_flag) {
      const int frame_lf_count = av1_num_planes(&cpi->common) > 1
                                     ? FRAME_LF_COUNT
                                     : FRAME_LF_COUNT - 2;
      for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id)
        mbmi->delta_lf[lf_id] = xd->delta_lf[lf_id];
      mbmi->delta_lf_from_base = xd->delta_lf_from_base;
    }
    if (has_second_ref(mbmi)) {
      if (mbmi->compound_idx == 0 ||
          mbmi->interinter_comp.type == COMPOUND_AVERAGE)
        mbmi->comp_group_idx = 0;
      else
        mbmi->comp_group_idx = 1;
    }
    update_stats(&cpi->common, tile_data, td, mi_row, mi_col);
  }
  x->rdmult = origin_mult;
}

#if !CONFIG_REALTIME_ONLY
static void encode_sb(const AV1_COMP *const cpi, ThreadData *td,
                      TileDataEnc *tile_data, TOKENEXTRA **tp, int mi_row,
                      int mi_col, RUN_TYPE dry_run, BLOCK_SIZE bsize,
                      PC_TREE *pc_tree, PARTITION_TREE *ptree, int *rate) {
  assert(bsize < BLOCK_SIZES_ALL);

  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  assert(bsize < BLOCK_SIZES_ALL);
  const int hbs_w = mi_size_wide[bsize] / 2;
  const int hbs_h = mi_size_high[bsize] / 2;
  const int qbs_w = mi_size_wide[bsize] / 4;
  const int qbs_h = mi_size_high[bsize] / 4;
  const int is_partition_root = is_partition_point(bsize);
  const int ctx = is_partition_root
                      ? partition_plane_context(xd, mi_row, mi_col, bsize)
                      : -1;
  const PARTITION_TYPE partition = pc_tree->partitioning;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
#if !CONFIG_EXT_RECUR_PARTITIONS
  const BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;
  if (subsize == BLOCK_INVALID) return;

  if (!dry_run && ctx >= 0) {
    const int has_rows = (mi_row + hbs_h) < cm->mi_rows;
    const int has_cols = (mi_col + hbs_w) < cm->mi_cols;

#if CONFIG_EXT_RECUR_PARTITIONS
    if (is_square_block(bsize)) {
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      if (has_rows && has_cols) {
#if CONFIG_ENTROPY_STATS
        td->counts->partition[ctx][partition]++;
#endif

        if (tile_data->allow_update_cdf) {
          FRAME_CONTEXT *fc = xd->tile_ctx;
          update_cdf(fc->partition_cdf[ctx], partition,
                     partition_cdf_length(bsize));
        }
      }
#if CONFIG_EXT_RECUR_PARTITIONS
    } else {
      const PARTITION_TYPE_REC p_rec =
          get_symbol_from_partition_rec_block(bsize, partition);
#if CONFIG_ENTROPY_STATS
      td->counts->partition_rec[ctx][p_rec]++;
#endif

      if (tile_data->allow_update_cdf) {
        FRAME_CONTEXT *fc = xd->tile_ctx;
        update_cdf(fc->partition_rec_cdf[ctx], p_rec, PARTITION_TYPES_REC);
      }
    }
#endif  // CONFIG_EXT_RECUR_PARTITIONS
  }

  PARTITION_TREE *sub_tree[4] = { NULL, NULL, NULL, NULL };
  if (!dry_run) {
    assert(ptree);

    ptree->partition = partition;
    ptree->bsize = bsize;
    ptree->mi_row = mi_row;
    ptree->mi_col = mi_col;
    PARTITION_TREE *parent = ptree->parent;
    const int ss_x = xd->plane[1].subsampling_x;
    const int ss_y = xd->plane[1].subsampling_y;
    set_chroma_ref_info(
        mi_row, mi_col, ptree->index, bsize, &ptree->chroma_ref_info,
        parent ? &parent->chroma_ref_info : NULL,
        parent ? parent->bsize : BLOCK_INVALID,
        parent ? parent->partition : PARTITION_NONE, ss_x, ss_y);

    switch (partition) {
      case PARTITION_SPLIT:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
        ptree->sub_tree[2] = av1_alloc_ptree_node(ptree, 2);
        ptree->sub_tree[3] = av1_alloc_ptree_node(ptree, 3);
        break;
#if !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_PARTITIONS
      case PARTITION_HORZ_A:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
        break;
      case PARTITION_HORZ_B:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 1);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 2);
        break;
      case PARTITION_VERT_A:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
        break;
      case PARTITION_VERT_B:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 1);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 2);
        break;
#endif  // CONFIG_EXT_PARTITIONS
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_RECUR_PARTITIONS
      case PARTITION_HORZ:
      case PARTITION_VERT:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
        break;
#if CONFIG_EXT_PARTITIONS
      case PARTITION_HORZ_3:
      case PARTITION_VERT_3:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
        ptree->sub_tree[2] = av1_alloc_ptree_node(ptree, 2);
        break;
#else
      case PARTITION_HORZ_4:
      case PARTITION_VERT_4:
        ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
        ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
        ptree->sub_tree[2] = av1_alloc_ptree_node(ptree, 2);
        ptree->sub_tree[3] = av1_alloc_ptree_node(ptree, 3);
        break;
#endif  // CONFIG_EXT_PARTITIONS
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      default: break;
    }
    for (int i = 0; i < 4; ++i) sub_tree[i] = ptree->sub_tree[i];
  }

  switch (partition) {
    case PARTITION_NONE:
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->none, rate);
      break;
    case PARTITION_VERT:
#if CONFIG_EXT_RECUR_PARTITIONS
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->vertical[0], sub_tree[0], rate);
      if (mi_col + hbs_w < cm->mi_cols) {
        encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs_w, dry_run,
                  subsize, pc_tree->vertical[1], sub_tree[1], rate);
      }
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->vertical[0], rate);
      if (mi_col + hbs_w < cm->mi_cols) {
        encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs_w, dry_run,
                 subsize, partition, pc_tree->vertical[1], rate);
      }
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      break;
    case PARTITION_HORZ:
#if CONFIG_EXT_RECUR_PARTITIONS
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->horizontal[0], sub_tree[0], rate);
      if (mi_row + hbs_h < cm->mi_rows) {
        encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col, dry_run,
                  subsize, pc_tree->horizontal[1], sub_tree[1], rate);
      }
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->horizontal[0], rate);
      if (mi_row + hbs_h < cm->mi_rows) {
        encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col, dry_run,
                 subsize, partition, pc_tree->horizontal[1], rate);
      }
#endif  // CONFIG_EXT_RECUR_PARTITIONS

      break;
    case PARTITION_SPLIT:
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->split[0], sub_tree[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs_w, dry_run,
                subsize, pc_tree->split[1], sub_tree[1], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col, dry_run,
                subsize, pc_tree->split[2], sub_tree[2], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col + hbs_w, dry_run,
                subsize, pc_tree->split[3], sub_tree[3], rate);
      break;
#if !CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_HORZ_A:
#if CONFIG_EXT_PARTITIONS
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, bsize2,
                pc_tree->horza_split[0], sub_tree[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs_w, dry_run, bsize2,
                pc_tree->horza_split[1], sub_tree[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col, dry_run, subsize,
               partition, pc_tree->horza_rec, rate);
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, pc_tree->horizontala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs_w, dry_run, bsize2,
               partition, pc_tree->horizontala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col, dry_run, subsize,
               partition, pc_tree->horizontala[2], rate);
#endif  // CONFIG_EXT_PARTITIONS
      break;
    case PARTITION_HORZ_B:
#if CONFIG_EXT_PARTITIONS
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->horzb_rec, rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col, dry_run, bsize2,
                pc_tree->horzb_split[0], sub_tree[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col + hbs_w, dry_run,
                bsize2, pc_tree->horzb_split[1], sub_tree[1], rate);
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->horizontalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col, dry_run, bsize2,
               partition, pc_tree->horizontalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col + hbs_w, dry_run,
               bsize2, partition, pc_tree->horizontalb[2], rate);
#endif  // CONFIG_EXT_PARTITIONS
      break;
    case PARTITION_VERT_A:
#if CONFIG_EXT_PARTITIONS
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, bsize2,
                pc_tree->verta_split[0], sub_tree[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col, dry_run, bsize2,
                pc_tree->verta_split[1], sub_tree[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs_w, dry_run, subsize,
               partition, pc_tree->verta_rec, rate);
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, bsize2,
               partition, pc_tree->verticala[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col, dry_run, bsize2,
               partition, pc_tree->verticala[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs_w, dry_run, subsize,
               partition, pc_tree->verticala[2], rate);
#endif  // CONFIG_EXT_PARTITIONS
      break;
    case PARTITION_VERT_B:
#if CONFIG_EXT_PARTITIONS
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->vertb_rec, rate);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + hbs_w, dry_run, bsize2,
                pc_tree->vertb_split[0], sub_tree[0], rate);
      encode_sb(cpi, td, tile_data, tp, mi_row + hbs_h, mi_col + hbs_w, dry_run,
                bsize2, pc_tree->vertb_split[1], sub_tree[1], rate);
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->verticalb[0], rate);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs_w, dry_run, bsize2,
               partition, pc_tree->verticalb[1], rate);
      encode_b(cpi, tile_data, td, tp, mi_row + hbs_h, mi_col + hbs_w, dry_run,
               bsize2, partition, pc_tree->verticalb[2], rate);
#endif  // CONFIG_EXT_PARTITIONS
      break;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_PARTITIONS
    case PARTITION_HORZ_3: {
      const BLOCK_SIZE bsize3 = get_partition_subsize(bsize, PARTITION_HORZ);
#if CONFIG_EXT_RECUR_PARTITIONS
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->horizontal3[0], sub_tree[0], rate);
      if (mi_row + qbs_h >= cm->mi_rows) break;
      encode_sb(cpi, td, tile_data, tp, mi_row + qbs_h, mi_col, dry_run, bsize3,
                pc_tree->horizontal3[1], sub_tree[1], rate);
      if (mi_row + 3 * qbs_h >= cm->mi_rows) break;
      encode_sb(cpi, td, tile_data, tp, mi_row + 3 * qbs_h, mi_col, dry_run,
                subsize, pc_tree->horizontal3[2], sub_tree[2], rate);
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->horizontal3[0], rate);
      if (mi_row + qbs_h >= cm->mi_rows) break;
      encode_b(cpi, tile_data, td, tp, mi_row + qbs_h, mi_col, dry_run, bsize3,
               partition, pc_tree->horizontal3[1], rate);
      if (mi_row + 3 * qbs_h >= cm->mi_rows) break;
      encode_b(cpi, tile_data, td, tp, mi_row + 3 * qbs_h, mi_col, dry_run,
               subsize, partition, pc_tree->horizontal3[2], rate);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      break;
    }
    case PARTITION_VERT_3: {
      const BLOCK_SIZE bsize3 = get_partition_subsize(bsize, PARTITION_VERT);
#if CONFIG_EXT_RECUR_PARTITIONS
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, dry_run, subsize,
                pc_tree->vertical3[0], sub_tree[0], rate);
      if (mi_col + qbs_w >= cm->mi_cols) break;
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + qbs_w, dry_run, bsize3,
                pc_tree->vertical3[1], sub_tree[1], rate);
      if (mi_col + 3 * qbs_w >= cm->mi_cols) break;
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col + 3 * qbs_w, dry_run,
                subsize, pc_tree->vertical3[2], sub_tree[2], rate);
#else
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, dry_run, subsize,
               partition, pc_tree->vertical3[0], rate);
      if (mi_col + qbs_w >= cm->mi_cols) break;
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + qbs_w, dry_run, bsize3,
               partition, pc_tree->vertical3[1], rate);
      if (mi_col + 3 * qbs_w >= cm->mi_cols) break;
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col + 3 * qbs_w, dry_run,
               subsize, partition, pc_tree->vertical3[2], rate);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      break;
    }
#else
    case PARTITION_HORZ_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_row = mi_row + i * qbs_h;
        if (i > 0 && this_mi_row >= cm->mi_rows) break;

#if CONFIG_EXT_RECUR_PARTITIONS
        encode_sb(cpi, td, tile_data, tp, this_mi_row, mi_col, dry_run, subsize,
                  pc_tree->horizontal4[i], sub_tree[i], rate);
#else
        encode_b(cpi, tile_data, td, tp, this_mi_row, mi_col, dry_run, subsize,
                 partition, pc_tree->horizontal4[i], rate);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      }
      break;
    case PARTITION_VERT_4:
      for (int i = 0; i < 4; ++i) {
        int this_mi_col = mi_col + i * qbs_w;
        if (i > 0 && this_mi_col >= cm->mi_cols) break;

#if CONFIG_EXT_RECUR_PARTITIONS
        encode_sb(cpi, td, tile_data, tp, mi_row, this_mi_col, dry_run, subsize,
                  pc_tree->vertical4[i], sub_tree[i], rate);
#else
        encode_b(cpi, tile_data, td, tp, mi_row, this_mi_col, dry_run, subsize,
                 partition, pc_tree->vertical4[i], rate);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      }
      break;
#endif  // CONFIG_EXT_PARTITIONS
    default: assert(0 && "Invalid partition type."); break;
  }

  if (ptree) ptree->is_settled = 1;
  update_ext_partition_context(xd, mi_row, mi_col, subsize, bsize, partition);
}

static void set_partial_sb_partition(const AV1_COMMON *const cm,
                                     MB_MODE_INFO *mi, int bh_in, int bw_in,
                                     int mi_rows_remaining,
                                     int mi_cols_remaining, BLOCK_SIZE bsize,
                                     MB_MODE_INFO **mib) {
  int bh = bh_in;
  int r, c;
  for (r = 0; r < cm->seq_params.mib_size; r += bh) {
    int bw = bw_in;
    for (c = 0; c < cm->seq_params.mib_size; c += bw) {
      const int index = r * cm->mi_stride + c;
      mib[index] = mi + index;
      mib[index]->sb_type = find_partition_size(
          bsize, mi_rows_remaining - r, mi_cols_remaining - c, &bh, &bw);
    }
  }
}

// This function attempts to set all mode info entries in a given superblock
// to the same block partition size.
// However, at the bottom and right borders of the image the requested size
// may not be allowed in which case this code attempts to choose the largest
// allowable partition.
static void set_fixed_partitioning(AV1_COMP *cpi, const TileInfo *const tile,
                                   MB_MODE_INFO **mib, int mi_row, int mi_col,
                                   BLOCK_SIZE bsize) {
  AV1_COMMON *const cm = &cpi->common;
  const int mi_rows_remaining = tile->mi_row_end - mi_row;
  const int mi_cols_remaining = tile->mi_col_end - mi_col;
  int block_row, block_col;
  MB_MODE_INFO *const mi_upper_left = cm->mi + mi_row * cm->mi_stride + mi_col;
  int bh = mi_size_high[bsize];
  int bw = mi_size_wide[bsize];

  assert((mi_rows_remaining > 0) && (mi_cols_remaining > 0));

  // Apply the requested partition size to the SB if it is all "in image"
  if ((mi_cols_remaining >= cm->seq_params.mib_size) &&
      (mi_rows_remaining >= cm->seq_params.mib_size)) {
    for (block_row = 0; block_row < cm->seq_params.mib_size; block_row += bh) {
      for (block_col = 0; block_col < cm->seq_params.mib_size;
           block_col += bw) {
        int index = block_row * cm->mi_stride + block_col;
        mib[index] = mi_upper_left + index;
        mib[index]->sb_type = bsize;
      }
    }
  } else {
    // Else this is a partial SB.
    set_partial_sb_partition(cm, mi_upper_left, bh, bw, mi_rows_remaining,
                             mi_cols_remaining, bsize, mib);
  }
}

static void rd_use_partition(AV1_COMP *cpi, ThreadData *td,
                             TileDataEnc *tile_data, MB_MODE_INFO **mib,
                             TOKENEXTRA **tp, int mi_row, int mi_col,
                             BLOCK_SIZE bsize, int *rate, int64_t *dist,
                             int do_recon, PC_TREE *pc_tree) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int ss_x = xd->plane[1].subsampling_x;
  const int ss_y = xd->plane[1].subsampling_y;
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  const int pl = (bsize >= BLOCK_8X8)
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  RD_STATS last_part_rdc, none_rdc, chosen_rdc, invalid_rdc;
  BLOCK_SIZE sub_subsize = BLOCK_4X4;
  int splits_below = 0;
  BLOCK_SIZE bs_type = mib[0]->sb_type;
  int do_partition_search = 1;

  if (pc_tree->none == NULL) {
    pc_tree->none =
        av1_alloc_pmc(cm, mi_row, mi_col, bsize, pc_tree, PARTITION_NONE, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
  }
  PICK_MODE_CONTEXT *ctx_none = pc_tree->none;

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  av1_invalid_rd_stats(&last_part_rdc);
  av1_invalid_rd_stats(&none_rdc);
  av1_invalid_rd_stats(&chosen_rdc);
  av1_invalid_rd_stats(&invalid_rdc);

  pc_tree->partitioning = partition;

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize,
                &pc_tree->chroma_ref_info);
    x->mb_energy = av1_log_block_var(cpi, x, bsize);
  }

  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);

  if (do_partition_search &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      cpi->sf.adjust_partitioning_from_last_frame) {
    // Check if any of the sub blocks are further split.
    if (partition == PARTITION_SPLIT && subsize > BLOCK_8X8) {
      sub_subsize = get_partition_subsize(subsize, PARTITION_SPLIT);
      splits_below = 1;
      for (int i = 0; i < 4; i++) {
        int jj = i >> 1, ii = i & 0x01;
        MB_MODE_INFO *this_mi = mib[jj * hbs * cm->mi_stride + ii * hbs];
        if (this_mi && this_mi->sb_type >= sub_subsize) {
          splits_below = 0;
        }
      }
    }

    // If partition is not none try none unless each of the 4 splits are split
    // even further..
    if (partition != PARTITION_NONE && !splits_below &&
        mi_row + hbs < cm->mi_rows && mi_col + hbs < cm->mi_cols) {
      pc_tree->partitioning = PARTITION_NONE;
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &none_rdc,
                    PARTITION_NONE, bsize, ctx_none, invalid_rdc, PICK_MODE_RD);

      if (none_rdc.rate < INT_MAX) {
        none_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
        none_rdc.rdcost = RDCOST(x->rdmult, none_rdc.rate, none_rdc.dist);
      }

      restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      mib[0]->sb_type = bs_type;
      pc_tree->partitioning = partition;
    }
  }

  for (int i = 0; i < 4; ++i) {
    pc_tree->split[i] = av1_alloc_pc_tree_node(
        mi_row + (i >> 1) * hbs, mi_col + (i & 1) * hbs, subsize, pc_tree,
        PARTITION_SPLIT, i, i == 3, ss_x, ss_y);
  }
  switch (partition) {
    case PARTITION_NONE:
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                    PARTITION_NONE, bsize, ctx_none, invalid_rdc, PICK_MODE_RD);
      break;
    case PARTITION_HORZ:
#if CONFIG_EXT_RECUR_PARTITIONS
      pc_tree->horizontal[0] = av1_alloc_pc_tree_node(
          mi_row, mi_col, subsize, pc_tree, PARTITION_HORZ, 0, 0, ss_x, ss_y);
      pc_tree->horizontal[1] =
          av1_alloc_pc_tree_node(mi_row + hbs, mi_col, subsize, pc_tree,
                                 PARTITION_HORZ, 1, 1, ss_x, ss_y);
      rd_use_partition(cpi, td, tile_data, mib, tp, mi_row, mi_col, subsize,
                       &last_part_rdc.rate, &last_part_rdc.dist, 1,
                       pc_tree->horizontal[0]);
#else
      for (int i = 0; i < 2; ++i) {
        pc_tree->horizontal[i] =
            av1_alloc_pmc(cm, mi_row + hbs * i, mi_col, subsize, pc_tree,
                          PARTITION_HORZ, i, ss_x, ss_y, &td->shared_coeff_buf);
      }
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                    PARTITION_HORZ, subsize, pc_tree->horizontal[0],
                    invalid_rdc, PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_row + hbs < cm->mi_rows) {
        RD_STATS tmp_rdc;
        av1_init_rd_stats(&tmp_rdc);
#if CONFIG_EXT_RECUR_PARTITIONS
        rd_use_partition(cpi, td, tile_data, mib + hbs * cm->mi_stride, tp,
                         mi_row + hbs, mi_col, subsize, &tmp_rdc.rate,
                         &tmp_rdc.dist, 0, pc_tree->horizontal[1]);
#else
        const PICK_MODE_CONTEXT *const ctx_h = pc_tree->horizontal[0];
        update_state(cpi, td, ctx_h, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize,
                          NULL);
        pick_sb_modes(cpi, tile_data, x, mi_row + hbs, mi_col, &tmp_rdc,
                      PARTITION_HORZ, subsize, pc_tree->horizontal[1],
                      invalid_rdc, PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_VERT:
#if CONFIG_EXT_RECUR_PARTITIONS
      pc_tree->vertical[0] = av1_alloc_pc_tree_node(
          mi_row, mi_col, subsize, pc_tree, PARTITION_VERT, 0, 0, ss_x, ss_y);
      pc_tree->vertical[1] =
          av1_alloc_pc_tree_node(mi_row, mi_col + hbs, subsize, pc_tree,
                                 PARTITION_VERT, 1, 1, ss_x, ss_y);
      rd_use_partition(cpi, td, tile_data, mib, tp, mi_row, mi_col, subsize,
                       &last_part_rdc.rate, &last_part_rdc.dist, 1,
                       pc_tree->vertical[0]);
#else
      for (int i = 0; i < 2; ++i) {
        pc_tree->vertical[i] =
            av1_alloc_pmc(cm, mi_row, mi_col + hbs * i, subsize, pc_tree,
                          PARTITION_VERT, i, ss_x, ss_y, &td->shared_coeff_buf);
      }
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &last_part_rdc,
                    PARTITION_VERT, subsize, pc_tree->vertical[0], invalid_rdc,
                    PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      if (last_part_rdc.rate != INT_MAX && bsize >= BLOCK_8X8 &&
          mi_col + hbs < cm->mi_cols) {
        RD_STATS tmp_rdc;
        av1_init_rd_stats(&tmp_rdc);

#if CONFIG_EXT_RECUR_PARTITIONS
        rd_use_partition(cpi, td, tile_data, mib + hbs, tp, mi_row,
                         mi_col + hbs, subsize, &tmp_rdc.rate, &tmp_rdc.dist, 0,
                         pc_tree->vertical[1]);
#else
        const PICK_MODE_CONTEXT *const ctx_v = pc_tree->vertical[0];
        update_state(cpi, td, ctx_v, mi_row, mi_col, subsize, 1);
        encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize,
                          NULL);
        pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + hbs, &tmp_rdc,
                      PARTITION_VERT, subsize,
                      pc_tree->vertical[bsize > BLOCK_8X8], invalid_rdc,
                      PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
        last_part_rdc.rdcost += tmp_rdc.rdcost;
      }
      break;
    case PARTITION_SPLIT:
      last_part_rdc.rate = 0;
      last_part_rdc.dist = 0;
      last_part_rdc.rdcost = 0;
      for (int i = 0; i < 4; i++) {
        int x_idx = (i & 1) * hbs;
        int y_idx = (i >> 1) * hbs;
        int jj = i >> 1, ii = i & 0x01;
        RD_STATS tmp_rdc;
        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        av1_init_rd_stats(&tmp_rdc);
        rd_use_partition(cpi, td, tile_data,
                         mib + jj * hbs * cm->mi_stride + ii * hbs, tp,
                         mi_row + y_idx, mi_col + x_idx, subsize, &tmp_rdc.rate,
                         &tmp_rdc.dist, i != 3, pc_tree->split[i]);
        if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
          av1_invalid_rd_stats(&last_part_rdc);
          break;
        }
        last_part_rdc.rate += tmp_rdc.rate;
        last_part_rdc.dist += tmp_rdc.dist;
      }
      break;
#if !CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_PARTITIONS
    case PARTITION_HORZ_3:
    case PARTITION_VERT_3:
#else
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4:
#endif  // CONFIG_EXT_PARTITIONS
      assert(0 && "Cannot handle extended partition types");
    default: assert(0); break;
  }

  if (last_part_rdc.rate < INT_MAX) {
    last_part_rdc.rate += x->partition_cost[pl][partition];
    last_part_rdc.rdcost =
        RDCOST(x->rdmult, last_part_rdc.rate, last_part_rdc.dist);
  }

  if (do_partition_search && cpi->sf.adjust_partitioning_from_last_frame &&
      cpi->sf.partition_search_type == SEARCH_PARTITION &&
      partition != PARTITION_SPLIT && bsize > BLOCK_8X8 &&
      (mi_row + bs < cm->mi_rows || mi_row + hbs == cm->mi_rows) &&
      (mi_col + bs < cm->mi_cols || mi_col + hbs == cm->mi_cols)) {
    BLOCK_SIZE split_subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    chosen_rdc.rate = 0;
    chosen_rdc.dist = 0;

    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
    pc_tree->partitioning = PARTITION_SPLIT;

    // Split partition.
    for (int i = 0; i < 4; i++) {
      int x_idx = (i & 1) * hbs;
      int y_idx = (i >> 1) * hbs;
      RD_STATS tmp_rdc;

      if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
        continue;

      save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      pc_tree->split[i]->partitioning = PARTITION_NONE;
      if (pc_tree->split[i]->none != NULL)
        pc_tree->split[i]->none =
            av1_alloc_pmc(cm, mi_row + y_idx, mi_col + x_idx, split_subsize,
                          pc_tree->split[i], PARTITION_NONE, 0, ss_x, ss_y,
                          &td->shared_coeff_buf);
      pick_sb_modes(cpi, tile_data, x, mi_row + y_idx, mi_col + x_idx, &tmp_rdc,
                    PARTITION_SPLIT, split_subsize, pc_tree->split[i]->none,
                    invalid_rdc, PICK_MODE_RD);

      restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
      if (tmp_rdc.rate == INT_MAX || tmp_rdc.dist == INT64_MAX) {
        av1_invalid_rd_stats(&chosen_rdc);
        break;
      }

      chosen_rdc.rate += tmp_rdc.rate;
      chosen_rdc.dist += tmp_rdc.dist;

      if (i != 3)
        encode_sb(cpi, td, tile_data, tp, mi_row + y_idx, mi_col + x_idx,
                  DRY_RUN_NORMAL, split_subsize, pc_tree->split[i], NULL, NULL);

      chosen_rdc.rate += x->partition_cost[pl][PARTITION_NONE];
    }
    if (chosen_rdc.rate < INT_MAX) {
      chosen_rdc.rate += x->partition_cost[pl][PARTITION_SPLIT];
      chosen_rdc.rdcost = RDCOST(x->rdmult, chosen_rdc.rate, chosen_rdc.dist);
    }
  }

  // If last_part is better set the partitioning to that.
  if (last_part_rdc.rdcost < chosen_rdc.rdcost) {
    mib[0]->sb_type = bsize;
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = partition;
    chosen_rdc = last_part_rdc;
  }
  // If none was better set the partitioning to that.
  if (none_rdc.rdcost < chosen_rdc.rdcost) {
    if (bsize >= BLOCK_8X8) pc_tree->partitioning = PARTITION_NONE;
    chosen_rdc = none_rdc;
  }

  restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  // We must have chosen a partitioning and encoding or we'll fail later on.
  // No other opportunities for success.
  if (bsize == cm->seq_params.sb_size)
    assert(chosen_rdc.rate < INT_MAX && chosen_rdc.dist < INT64_MAX);

  if (do_recon) {
    if (bsize == cm->seq_params.sb_size) {
      // NOTE: To get estimate for rate due to the tokens, use:
      // int rate_coeffs = 0;
      // encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_COSTCOEFFS,
      //           bsize, pc_tree, &rate_coeffs);
      x->cb_offset = 0;
      av1_reset_ptree_in_sbi(xd->sbi);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, OUTPUT_ENABLED, bsize,
                pc_tree, xd->sbi->ptree_root, NULL);
    } else {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL, NULL);
    }
  }

  *rate = chosen_rdc.rate;
  *dist = chosen_rdc.dist;
  x->rdmult = orig_rdmult;
}
#endif  // !CONFIG_REALTIME_ONLY

static void nonrd_use_partition(AV1_COMP *cpi, ThreadData *td,
                                TileDataEnc *tile_data, MB_MODE_INFO **mib,
                                TOKENEXTRA **tp, int mi_row, int mi_col,
                                BLOCK_SIZE bsize, PC_TREE *pc_tree,
                                PARTITION_TREE *ptree) {
  AV1_COMMON *const cm = &cpi->common;
  TileInfo *const tile_info = &tile_data->tile_info;
  const SPEED_FEATURES *const sf = &cpi->sf;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int ss_x = xd->plane[1].subsampling_x;
  const int ss_y = xd->plane[1].subsampling_y;
  // Only square blocks from 8x8 to 128x128 are supported
  assert(bsize >= BLOCK_8X8 && bsize <= BLOCK_128X128);
  const int bs = mi_size_wide[bsize];
  const int hbs = bs / 2;
  const PARTITION_TYPE partition =
      (bsize >= BLOCK_8X8) ? get_partition(cm, mi_row, mi_col, bsize)
                           : PARTITION_NONE;
  const BLOCK_SIZE subsize = get_partition_subsize(bsize, partition);
  RD_STATS dummy_cost;
  av1_invalid_rd_stats(&dummy_cost);
  RD_STATS invalid_rd;
  av1_invalid_rd_stats(&invalid_rd);

  if (mi_row >= cm->mi_rows || mi_col >= cm->mi_cols) return;

  assert(mi_size_wide[bsize] == mi_size_high[bsize]);

  pc_tree->partitioning = partition;

  assert(ptree);
  ptree->partition = partition;
  ptree->bsize = bsize;
  ptree->mi_row = mi_row;
  ptree->mi_col = mi_col;
  PARTITION_TREE *parent = ptree->parent;
  set_chroma_ref_info(mi_row, mi_col, ptree->index, bsize,
                      &ptree->chroma_ref_info,
                      parent ? &parent->chroma_ref_info : NULL,
                      parent ? parent->bsize : BLOCK_INVALID,
                      parent ? parent->partition : PARTITION_NONE, ss_x, ss_y);

  // int num_splittable_sub_blocks = 0;
  switch (partition) {
    case PARTITION_SPLIT:
      ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
      ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
      ptree->sub_tree[2] = av1_alloc_ptree_node(ptree, 2);
      ptree->sub_tree[3] = av1_alloc_ptree_node(ptree, 3);
      break;
#if !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_PARTITIONS
    case PARTITION_HORZ_A:
      ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
      ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
      break;
    case PARTITION_HORZ_B:
      ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 1);
      ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 2);
      break;
    case PARTITION_VERT_A:
      ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
      ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
      break;
    case PARTITION_VERT_B:
      ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 1);
      ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 2);
      break;
#endif  // CONFIG_EXT_PARTITIONS
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_HORZ:
    case PARTITION_VERT:
      ptree->sub_tree[0] = av1_alloc_ptree_node(ptree, 0);
      ptree->sub_tree[1] = av1_alloc_ptree_node(ptree, 1);
      break;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    default: break;
  }

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  switch (partition) {
    case PARTITION_NONE:
      pc_tree->none =
          av1_alloc_pmc(cm, mi_row, mi_col, bsize, pc_tree, PARTITION_NONE, 0,
                        ss_x, ss_y, &td->shared_coeff_buf);
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &dummy_cost,
                    PARTITION_NONE, bsize, pc_tree->none, invalid_rd,
                    sf->use_fast_nonrd_pick_mode ? PICK_MODE_FAST_NONRD
                                                 : PICK_MODE_NONRD);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, 0, bsize, partition,
               pc_tree->none, NULL);
      break;
    case PARTITION_VERT:
#if CONFIG_EXT_RECUR_PARTITIONS
      ptree->sub_tree[0]->partition = PARTITION_NONE;
      ptree->sub_tree[1]->partition = PARTITION_NONE;

      pc_tree->vertical[0] = av1_alloc_pc_tree_node(
          mi_row, mi_col, subsize, pc_tree, PARTITION_VERT, 0, 0, ss_x, ss_y);
      pc_tree->vertical[1] =
          av1_alloc_pc_tree_node(mi_row, mi_col + hbs, subsize, pc_tree,
                                 PARTITION_VERT, 1, 1, ss_x, ss_y);

      nonrd_use_partition(cpi, td, tile_data, mib, tp, mi_row, mi_col, subsize,
                          pc_tree->vertical[0], ptree->sub_tree[0]);
#else
      for (int i = 0; i < 2; ++i) {
        pc_tree->vertical[i] =
            av1_alloc_pmc(cm, mi_row, mi_col + hbs * i, subsize, pc_tree,
                          PARTITION_VERT, i, ss_x, ss_y, &td->shared_coeff_buf);
      }
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &dummy_cost,
                    PARTITION_VERT, subsize, pc_tree->vertical[0], invalid_rd,
                    sf->use_fast_nonrd_pick_mode ? PICK_MODE_FAST_NONRD
                                                 : PICK_MODE_NONRD);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, 0, subsize,
               PARTITION_VERT, pc_tree->vertical[0], NULL);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      if (mi_col + hbs < cm->mi_cols && bsize > BLOCK_8X8) {
#if CONFIG_EXT_RECUR_PARTITIONS
        nonrd_use_partition(cpi, td, tile_data, mib + hbs, tp, mi_row,
                            mi_col + hbs, subsize, pc_tree->vertical[1],
                            ptree->sub_tree[1]);
#else
        pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + hbs, &dummy_cost,
                      PARTITION_VERT, subsize, pc_tree->vertical[1], invalid_rd,
                      sf->use_fast_nonrd_pick_mode ? PICK_MODE_FAST_NONRD
                                                   : PICK_MODE_NONRD);
        encode_b(cpi, tile_data, td, tp, mi_row, mi_col + hbs, 0, subsize,
                 PARTITION_VERT, pc_tree->vertical[1], NULL);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      }
      break;
    case PARTITION_HORZ:
#if CONFIG_EXT_RECUR_PARTITIONS
      ptree->sub_tree[0]->partition = PARTITION_NONE;
      ptree->sub_tree[1]->partition = PARTITION_NONE;

      pc_tree->horizontal[0] = av1_alloc_pc_tree_node(
          mi_row, mi_col, subsize, pc_tree, PARTITION_HORZ, 0, 0, ss_x, ss_y);
      pc_tree->horizontal[1] =
          av1_alloc_pc_tree_node(mi_row + hbs, mi_col, subsize, pc_tree,
                                 PARTITION_HORZ, 1, 1, ss_x, ss_y);

      nonrd_use_partition(cpi, td, tile_data, mib, tp, mi_row, mi_col, subsize,
                          pc_tree->horizontal[0], ptree->sub_tree[0]);
#else
      for (int i = 0; i < 2; ++i) {
        pc_tree->horizontal[i] =
            av1_alloc_pmc(cm, mi_row + hbs * i, mi_col, subsize, pc_tree,
                          PARTITION_HORZ, i, ss_x, ss_y, &td->shared_coeff_buf);
      }
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &dummy_cost,
                    PARTITION_HORZ, subsize, pc_tree->horizontal[0], invalid_rd,
                    sf->use_fast_nonrd_pick_mode ? PICK_MODE_FAST_NONRD
                                                 : PICK_MODE_NONRD);
      encode_b(cpi, tile_data, td, tp, mi_row, mi_col, 0, subsize,
               PARTITION_HORZ, pc_tree->horizontal[0], NULL);
#endif  // CONFIG_EXT_RECUR_PARTITIONS

      if (mi_row + hbs < cm->mi_rows && bsize > BLOCK_8X8) {
#if CONFIG_EXT_RECUR_PARTITIONS
        nonrd_use_partition(cpi, td, tile_data, mib + hbs * cm->mi_stride, tp,
                            mi_row + hbs, mi_col, subsize,
                            pc_tree->horizontal[1], ptree->sub_tree[1]);
#else
        pick_sb_modes(cpi, tile_data, x, mi_row + hbs, mi_col, &dummy_cost,
                      PARTITION_HORZ, subsize, pc_tree->horizontal[1],
                      invalid_rd,
                      sf->use_fast_nonrd_pick_mode ? PICK_MODE_FAST_NONRD
                                                   : PICK_MODE_NONRD);
        encode_b(cpi, tile_data, td, tp, mi_row + hbs, mi_col, 0, subsize,
                 PARTITION_HORZ, pc_tree->horizontal[1], NULL);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      }
      break;
    case PARTITION_SPLIT:
      for (int i = 0; i < 4; i++) {
        int x_idx = (i & 1) * hbs;
        int y_idx = (i >> 1) * hbs;
        int jj = i >> 1, ii = i & 0x01;

        if ((mi_row + y_idx >= cm->mi_rows) || (mi_col + x_idx >= cm->mi_cols))
          continue;

        pc_tree->split[i] = av1_alloc_pc_tree_node(
            mi_row + y_idx, mi_col + x_idx, subsize, pc_tree, PARTITION_SPLIT,
            i, i == 3, ss_x, ss_y);
        nonrd_use_partition(cpi, td, tile_data,
                            mib + jj * hbs * cm->mi_stride + ii * hbs, tp,
                            mi_row + y_idx, mi_col + x_idx, subsize,
                            pc_tree->split[i], ptree->sub_tree[i]);
      }
      break;
#if !CONFIG_EXT_RECUR_PARTITIONS
    case PARTITION_VERT_A:
    case PARTITION_VERT_B:
    case PARTITION_HORZ_A:
    case PARTITION_HORZ_B:
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_PARTITIONS
    case PARTITION_HORZ_3:
    case PARTITION_VERT_3:
#else
    case PARTITION_HORZ_4:
    case PARTITION_VERT_4:
#endif  // CONFIG_EXT_PARTITIONS
      assert(0 && "Cannot handle extended partition types");
    default: assert(0); break;
  }

  ptree->is_settled = 1;
  if (partition != PARTITION_SPLIT || bsize == BLOCK_8X8)
    update_partition_context(xd, mi_row, mi_col, subsize, bsize);
}

#if !CONFIG_REALTIME_ONLY
static const FIRSTPASS_STATS *read_one_frame_stats(const TWO_PASS *p, int frm) {
  assert(frm >= 0);
  if (frm < 0 ||
      p->stats_buf_ctx->stats_in_start + frm > p->stats_buf_ctx->stats_in_end) {
    return NULL;
  }

  return &p->stats_buf_ctx->stats_in_start[frm];
}
// Checks to see if a super block is on a horizontal image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
static int active_h_edge(const AV1_COMP *cpi, int mi_row, int mi_step) {
  int top_edge = 0;
  int bottom_edge = cpi->common.mi_rows;
  int is_active_h_edge = 0;

  // For two pass account for any formatting bars detected.
  if (is_stat_consumption_stage_twopass(cpi)) {
    const AV1_COMMON *const cm = &cpi->common;
    const FIRSTPASS_STATS *const this_frame_stats = read_one_frame_stats(
        &cpi->twopass, cm->current_frame.display_order_hint);
    if (this_frame_stats == NULL) return AOM_CODEC_ERROR;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    top_edge += (int)(this_frame_stats->inactive_zone_rows * 4);

    bottom_edge -= (int)(this_frame_stats->inactive_zone_rows * 4);
    bottom_edge = AOMMAX(top_edge, bottom_edge);
  }

  if (((top_edge >= mi_row) && (top_edge < (mi_row + mi_step))) ||
      ((bottom_edge >= mi_row) && (bottom_edge < (mi_row + mi_step)))) {
    is_active_h_edge = 1;
  }
  return is_active_h_edge;
}

// Checks to see if a super block is on a vertical image edge.
// In most cases this is the "real" edge unless there are formatting
// bars embedded in the stream.
static int active_v_edge(const AV1_COMP *cpi, int mi_col, int mi_step) {
  int left_edge = 0;
  int right_edge = cpi->common.mi_cols;
  int is_active_v_edge = 0;

  // For two pass account for any formatting bars detected.
  if (is_stat_consumption_stage_twopass(cpi)) {
    const AV1_COMMON *const cm = &cpi->common;
    const FIRSTPASS_STATS *const this_frame_stats = read_one_frame_stats(
        &cpi->twopass, cm->current_frame.display_order_hint);
    if (this_frame_stats == NULL) return AOM_CODEC_ERROR;

    // The inactive region is specified in MBs not mi units.
    // The image edge is in the following MB row.
    left_edge += (int)(this_frame_stats->inactive_zone_cols * 4);

    right_edge -= (int)(this_frame_stats->inactive_zone_cols * 4);
    right_edge = AOMMAX(left_edge, right_edge);
  }

  if (((left_edge >= mi_col) && (left_edge < (mi_col + mi_step))) ||
      ((right_edge >= mi_col) && (right_edge < (mi_col + mi_step)))) {
    is_active_v_edge = 1;
  }
  return is_active_v_edge;
}
#endif  // !CONFIG_REALTIME_ONLY

static INLINE void store_pred_mv(MACROBLOCK *x, PICK_MODE_CONTEXT *ctx) {
  memcpy(ctx->pred_mv, x->pred_mv, sizeof(x->pred_mv));
}

static INLINE void load_pred_mv(MACROBLOCK *x,
                                const PICK_MODE_CONTEXT *const ctx) {
  memcpy(x->pred_mv, ctx->pred_mv, sizeof(x->pred_mv));
}

#if !CONFIG_REALTIME_ONLY
#if !CONFIG_EXT_RECUR_PARTITIONS
// Try searching for an encoding for the given subblock. Returns zero if the
// rdcost is already too high (to tell the caller not to bother searching for
// encodings of further subblocks)
static int rd_try_subblock(AV1_COMP *const cpi, ThreadData *td,
                           TileDataEnc *tile_data, TOKENEXTRA **tp, int is_last,
                           int mi_row, int mi_col, BLOCK_SIZE subsize,
                           RD_STATS best_rdcost, RD_STATS *sum_rdc,
                           PARTITION_TYPE partition,
                           PICK_MODE_CONTEXT *prev_ctx,
                           PICK_MODE_CONTEXT *this_ctx) {
  MACROBLOCK *const x = &td->mb;
  const int orig_mult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, subsize, NO_AQ, NULL);

  av1_rd_cost_update(x->rdmult, &best_rdcost);
  if (cpi->sf.adaptive_motion_search) load_pred_mv(x, prev_ctx);

  RD_STATS rdcost_remaining;
  av1_rd_stats_subtraction(x->rdmult, &best_rdcost, sum_rdc, &rdcost_remaining);
  RD_STATS this_rdc;
  pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc, partition,
                subsize, this_ctx, rdcost_remaining, PICK_MODE_RD);

  if (this_rdc.rate == INT_MAX) {
    sum_rdc->rdcost = INT64_MAX;
  } else {
    sum_rdc->rate += this_rdc.rate;
    sum_rdc->dist += this_rdc.dist;
    av1_rd_cost_update(x->rdmult, sum_rdc);
  }

  if (sum_rdc->rdcost >= best_rdcost.rdcost) {
    x->rdmult = orig_mult;
    return 0;
  }

  if (!is_last) {
    update_state(cpi, td, this_ctx, mi_row, mi_col, subsize, 1);
    encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize, NULL);
  }

  x->rdmult = orig_mult;
  return 1;
}
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

#if CONFIG_EXT_RECUR_PARTITIONS || CONFIG_EXT_PARTITIONS
typedef struct {
  SIMPLE_MOTION_DATA_TREE *sms_tree;
  PC_TREE *pc_tree;
  PICK_MODE_CONTEXT *ctx;
  int mi_row;
  int mi_col;
  BLOCK_SIZE bsize;
  PARTITION_TYPE partition;
  int is_last_subblock;
  int is_splittable;
  int max_sq_part;
  int min_sq_part;
} SUBBLOCK_RDO_DATA;

// Try searching for an encoding for the given subblock. Returns zero if the
// rdcost is already too high (to tell the caller not to bother searching for
// encodings of further subblocks)
static int rd_try_subblock_new(AV1_COMP *const cpi, ThreadData *td,
                               TileDataEnc *tile_data, TOKENEXTRA **tp,
                               SUBBLOCK_RDO_DATA *rdo_data,
                               RD_STATS best_rdcost, RD_STATS *sum_rdc,
                               SB_MULTI_PASS_MODE multi_pass_mode) {
  MACROBLOCK *const x = &td->mb;
  const int orig_mult = x->rdmult;
  const int mi_row = rdo_data->mi_row;
  const int mi_col = rdo_data->mi_col;
  const BLOCK_SIZE bsize = rdo_data->bsize;

  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);

  av1_rd_cost_update(x->rdmult, &best_rdcost);
  // TODO(yuec): figure out prev_ctx to allow better preloaded mv as in
  // if (cpi->sf.adaptive_motion_search) load_pred_mv(x, prev_ctx);

  RD_STATS rdcost_remaining;
  av1_rd_stats_subtraction(x->rdmult, &best_rdcost, sum_rdc, &rdcost_remaining);
  RD_STATS this_rdc;

  if (rdo_data->is_splittable) {
    if (!rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, bsize,
                           rdo_data->max_sq_part, rdo_data->min_sq_part,
                           &this_rdc, rdcost_remaining, rdo_data->pc_tree,
                           rdo_data->sms_tree, NULL, multi_pass_mode))
      return 0;
  } else {
    pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc,
                  rdo_data->partition, bsize, rdo_data->ctx, rdcost_remaining,
                  PICK_MODE_RD);
  }

  if (this_rdc.rate == INT_MAX) {
    sum_rdc->rdcost = INT64_MAX;
  } else {
    sum_rdc->rate += this_rdc.rate;
    sum_rdc->dist += this_rdc.dist;
    av1_rd_cost_update(x->rdmult, sum_rdc);
  }

  if (sum_rdc->rdcost >= best_rdcost.rdcost) {
    x->rdmult = orig_mult;
    return 0;
  }

  if (!rdo_data->is_last_subblock && !rdo_data->is_splittable) {
    update_state(cpi, td, rdo_data->ctx, mi_row, mi_col, bsize, 1);
    encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, bsize, NULL);
  }

  x->rdmult = orig_mult;
  return 1;
}
#endif  // CONFIG_EXT_RECUR_PARTITIONS || CONFIG_EXT_PARTITIONS

#if !CONFIG_EXT_RECUR_PARTITIONS
#if CONFIG_EXT_PARTITIONS
static bool rd_test_partition_abpart(AV1_COMP *const cpi, ThreadData *td,
                                     TileDataEnc *tile_data, TOKENEXTRA **tp,
                                     PC_TREE *pc_tree, RD_STATS *best_rdc,
                                     int mi_row, int mi_col, BLOCK_SIZE bsize,
                                     PARTITION_TYPE partition,
                                     SUBBLOCK_RDO_DATA *subblocks[3],
                                     SB_MULTI_PASS_MODE multi_pass_mode) {
  const MACROBLOCK *const x = &td->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  RD_STATS sum_rdc;
  av1_init_rd_stats(&sum_rdc);
  sum_rdc.rate = x->partition_cost[pl][partition];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);
  if (!rd_try_subblock_new(cpi, td, tile_data, tp, subblocks[0], *best_rdc,
                           &sum_rdc, multi_pass_mode))
    return false;

  if (!rd_try_subblock_new(cpi, td, tile_data, tp, subblocks[1], *best_rdc,
                           &sum_rdc, multi_pass_mode))
    return false;

  if (!rd_try_subblock_new(cpi, td, tile_data, tp, subblocks[2], *best_rdc,
                           &sum_rdc, multi_pass_mode))
    return false;

  av1_rd_cost_update(x->rdmult, &sum_rdc);
  if (sum_rdc.rdcost >= best_rdc->rdcost) return false;
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
  if (sum_rdc.rdcost >= best_rdc->rdcost) return false;

  *best_rdc = sum_rdc;
  pc_tree->partitioning = partition;
  return true;
}
#else
static bool rd_test_partition3(AV1_COMP *const cpi, ThreadData *td,
                               TileDataEnc *tile_data, TOKENEXTRA **tp,
                               PC_TREE *pc_tree, RD_STATS *best_rdc,
                               PICK_MODE_CONTEXT *ctxs[3],
                               PICK_MODE_CONTEXT *ctx, int mi_row, int mi_col,
                               BLOCK_SIZE bsize, PARTITION_TYPE partition,
                               int mi_row0, int mi_col0, BLOCK_SIZE subsize0,
                               int mi_row1, int mi_col1, BLOCK_SIZE subsize1,
                               int mi_row2, int mi_col2, BLOCK_SIZE subsize2) {
  const MACROBLOCK *const x = &td->mb;
  const MACROBLOCKD *const xd = &x->e_mbd;
  const int pl = partition_plane_context(xd, mi_row, mi_col, bsize);
  RD_STATS sum_rdc;
  av1_init_rd_stats(&sum_rdc);
  sum_rdc.rate = x->partition_cost[pl][partition];
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);
  if (!rd_try_subblock(cpi, td, tile_data, tp, 0, mi_row0, mi_col0, subsize0,
                       *best_rdc, &sum_rdc, partition, ctx, ctxs[0]))
    return false;

  if (!rd_try_subblock(cpi, td, tile_data, tp, 0, mi_row1, mi_col1, subsize1,
                       *best_rdc, &sum_rdc, partition, ctxs[0], ctxs[1]))
    return false;

  if (!rd_try_subblock(cpi, td, tile_data, tp, 1, mi_row2, mi_col2, subsize2,
                       *best_rdc, &sum_rdc, partition, ctxs[1], ctxs[2]))
    return false;

  av1_rd_cost_update(x->rdmult, &sum_rdc);
  if (sum_rdc.rdcost >= best_rdc->rdcost) return false;
  sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
  if (sum_rdc.rdcost >= best_rdc->rdcost) return false;

  *best_rdc = sum_rdc;
  pc_tree->partitioning = partition;
  return true;
}
#endif  // CONFIG_EXT_PARTITIONS
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

static void reset_simple_motion_tree_partition(
    SIMPLE_MOTION_DATA_TREE *sms_tree, BLOCK_SIZE bsize) {
  sms_tree->partitioning = PARTITION_NONE;

  if (bsize >= BLOCK_8X8) {
    BLOCK_SIZE subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
    for (int idx = 0; idx < 4; ++idx)
      reset_simple_motion_tree_partition(sms_tree->split[idx], subsize);
  }
}

// Record the ref frames that have been selected by square partition blocks.
static void update_picked_ref_frames_mask(MACROBLOCK *const x, int ref_type,
                                          BLOCK_SIZE bsize, int mib_size,
                                          int mi_row, int mi_col) {
  const int sb_size_mask = mib_size - 1;
  const int mi_row_in_sb = mi_row & sb_size_mask;
  const int mi_col_in_sb = mi_col & sb_size_mask;
  const int mi_size_w = mi_size_wide[bsize];
  const int mi_size_h = mi_size_high[bsize];
  for (int i = mi_row_in_sb; i < mi_row_in_sb + mi_size_h; ++i) {
    for (int j = mi_col_in_sb; j < mi_col_in_sb + mi_size_w; ++j) {
      x->picked_ref_frames_mask[i * 32 + j] |= 1 << ref_type;
    }
  }
}

// TODO(jinging,jimbankoski,rbultje): properly skip partition types that are
// unlikely to be selected depending on previous rate-distortion optimization
// results, for encoding speed-up.
static bool rd_pick_partition(
    AV1_COMP *const cpi, ThreadData *td, TileDataEnc *tile_data,
    TOKENEXTRA **tp, int mi_row, int mi_col, BLOCK_SIZE bsize,
    BLOCK_SIZE max_sq_part, BLOCK_SIZE min_sq_part, RD_STATS *rd_cost,
    RD_STATS best_rdc, PC_TREE *pc_tree, SIMPLE_MOTION_DATA_TREE *sms_tree,
    int64_t *none_rd, SB_MULTI_PASS_MODE multi_pass_mode) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const int mi_step_w = mi_size_wide[bsize] / 2;
  const int mi_step_h = mi_size_high[bsize] / 2;
  const int ss_x = xd->plane[1].subsampling_x;
  const int ss_y = xd->plane[1].subsampling_y;
  RD_SEARCH_MACROBLOCK_CONTEXT x_ctx;
  const TOKENEXTRA *const tp_orig = *tp;
  int tmp_partition_cost[PARTITION_TYPES];
  BLOCK_SIZE subsize;
  RD_STATS this_rdc, sum_rdc;
  const int is_block_splittable = is_partition_point(bsize);
  int do_square_split = is_block_splittable && is_square_block(bsize);
  const int pl = is_block_splittable
                     ? partition_plane_context(xd, mi_row, mi_col, bsize)
                     : 0;
  const int *partition_cost =
      pl >= 0 ? x->partition_cost[pl] : x->partition_cost[0];

  int do_rectangular_split = cpi->oxcf.enable_rect_partitions;
  int64_t cur_none_rd = 0;
  int64_t split_rd[4] = { 0, 0, 0, 0 };
  int64_t horz_rd[2] = { 0, 0 };
  int64_t vert_rd[2] = { 0, 0 };
  int prune_horz = 0;
  int prune_vert = 0;
  int terminate_partition_search = 0;

#if !CONFIG_EXT_RECUR_PARTITIONS
  int split_ctx_is_ready[2] = { 0, 0 };
  int horz_ctx_is_ready = 0;
  int vert_ctx_is_ready = 0;
  BLOCK_SIZE bsize2 = get_partition_subsize(bsize, PARTITION_SPLIT);
  (void)split_ctx_is_ready;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

#if CONFIG_EXT_RECUR_PARTITIONS
  if (sms_tree != NULL)
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    sms_tree->partitioning = PARTITION_NONE;

  bool found_best_partition = false;
  if (best_rdc.rdcost < 0) {
    av1_invalid_rd_stats(rd_cost);
    return found_best_partition;
  }

#if CONFIG_EXT_RECUR_PARTITIONS && CONFIG_EXT_PARTITIONS
  // Check whether there is a counterpart pc_tree node with the same size
  // and the same neighboring context at the same location but from a different
  // partition path. If yes directly copy the RDO decision made for the
  // counterpart.
  PC_TREE *counterpart_block = av1_look_for_counterpart_block(pc_tree);
  if (counterpart_block) {
    if (counterpart_block->rd_cost.rate != INT_MAX) {
      av1_copy_pc_tree_recursive(cm, pc_tree, counterpart_block, ss_x, ss_y,
                                 &td->shared_coeff_buf, num_planes);
      *rd_cost = pc_tree->rd_cost;
      assert(bsize != cm->seq_params.sb_size);
      if (bsize == cm->seq_params.sb_size) exit(0);

      if (!pc_tree->is_last_subblock) {
        encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                  pc_tree, NULL, NULL);
      }
      return true;
    } else {
      av1_invalid_rd_stats(rd_cost);
      return false;
    }
  }
#endif  // CONFIG_EXT_RECUR_PARTITIONS && CONFIG_EXT_PARTITIONS

  if (frame_is_intra_only(cm) && bsize == BLOCK_64X64) {
    x->quad_tree_idx = 0;
    x->cnn_output_valid = 0;
  }

  if (bsize == cm->seq_params.sb_size) x->must_find_valid_partition = 0;

  // Override skipping rectangular partition operations for edge blocks
  const int has_rows = (mi_row + mi_step_h < cm->mi_rows);
  const int has_cols = (mi_col + mi_step_w < cm->mi_cols);

  if (none_rd) *none_rd = 0;
  CHROMA_REF_INFO tmp_chr_ref_info = { 1, 0, mi_row, mi_col, bsize, bsize };

  const BLOCK_SIZE subsize_horz = get_partition_subsize(bsize, PARTITION_HORZ);
  int is_chroma_size_valid_horz = 0;
  if (subsize_horz < BLOCK_SIZES_ALL) {
    set_chroma_ref_info(mi_row, mi_col, 0, subsize_horz, &tmp_chr_ref_info,
                        &pc_tree->chroma_ref_info, bsize, PARTITION_HORZ, ss_x,
                        ss_y);
    is_chroma_size_valid_horz =
        get_plane_block_size(tmp_chr_ref_info.bsize_base, ss_x, ss_y) !=
        BLOCK_INVALID;
  }

  const BLOCK_SIZE subsize_vert = get_partition_subsize(bsize, PARTITION_VERT);
  int is_chroma_size_valid_vert = 0;
  if (subsize_vert < BLOCK_SIZES_ALL) {
    set_chroma_ref_info(mi_row, mi_col, 0, subsize_vert, &tmp_chr_ref_info,
                        &pc_tree->chroma_ref_info, bsize, PARTITION_VERT, ss_x,
                        ss_y);
    is_chroma_size_valid_vert =
        get_plane_block_size(tmp_chr_ref_info.bsize_base, ss_x, ss_y) !=
        BLOCK_INVALID;
  }

  int partition_none_allowed = has_rows && has_cols;
  int partition_horz_allowed =
      has_cols && is_partition_valid(bsize, PARTITION_HORZ) &&
      cpi->oxcf.enable_rect_partitions && is_chroma_size_valid_horz;
  int partition_vert_allowed =
      has_rows && is_partition_valid(bsize, PARTITION_VERT) &&
      cpi->oxcf.enable_rect_partitions && is_chroma_size_valid_vert;

  (void)*tp_orig;

#if CONFIG_COLLECT_PARTITION_STATS
  int partition_decisions[EXT_PARTITION_TYPES] = { 0 };
  int partition_attempts[EXT_PARTITION_TYPES] = { 0 };
  int64_t partition_times[EXT_PARTITION_TYPES] = { 0 };
  struct aom_usec_timer partition_timer = { 0 };
  int partition_timer_on = 0;
#if CONFIG_COLLECT_PARTITION_STATS == 2
  PartitionStats *part_stats = &cpi->partition_stats;
#endif
#endif

  // Override partition costs at the edges of the frame in the same
  // way as in read_partition (see decodeframe.c)
  if (!(has_rows && has_cols)) {
    assert(is_block_splittable && pl >= 0);
    const aom_cdf_prob *partition_cdf = cm->fc->partition_cdf[pl];
    const int max_cost = av1_cost_symbol(0);
    for (int i = 0; i < PARTITION_TYPES; ++i) tmp_partition_cost[i] = max_cost;
    if (has_cols) {
      // At the bottom, the two possibilities are HORZ and SPLIT
      aom_cdf_prob bot_cdf[2];
      partition_gather_vert_alike(bot_cdf, partition_cdf, bsize);
      static const int bot_inv_map[2] = { PARTITION_HORZ, PARTITION_SPLIT };
      av1_cost_tokens_from_cdf(tmp_partition_cost, bot_cdf, bot_inv_map);
    } else if (has_rows) {
      // At the right, the two possibilities are VERT and SPLIT
      aom_cdf_prob rhs_cdf[2];
      partition_gather_horz_alike(rhs_cdf, partition_cdf, bsize);
      static const int rhs_inv_map[2] = { PARTITION_VERT, PARTITION_SPLIT };
      av1_cost_tokens_from_cdf(tmp_partition_cost, rhs_cdf, rhs_inv_map);
    } else {
      // At the bottom right, we always split
      tmp_partition_cost[PARTITION_SPLIT] = 0;
    }

    partition_cost = tmp_partition_cost;
  }

#ifndef NDEBUG
  // Nothing should rely on the default value of this array (which is just
  // leftover from encoding the previous block. Setting it to fixed pattern
  // when debugging.
  // bit 0, 1, 2 are blk_skip of each plane
  // bit 4, 5, 6 are initialization checking of each plane
  memset(x->blk_skip, 0x77, sizeof(x->blk_skip));
#endif  // NDEBUG

#if !CONFIG_EXT_RECUR_PARTITIONS
  assert(block_size_wide[bsize] == block_size_high[bsize]);
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

  av1_init_rd_stats(&this_rdc);

  set_offsets(cpi, tile_info, x, mi_row, mi_col, bsize,
              &pc_tree->chroma_ref_info);

  // Save rdmult before it might be changed, so it can be restored later.
  const int orig_rdmult = x->rdmult;
  setup_block_rdmult(cpi, x, mi_row, mi_col, bsize, NO_AQ, NULL);

  av1_rd_cost_update(x->rdmult, &best_rdc);

  if (bsize == BLOCK_16X16 && cpi->vaq_refresh)
    x->mb_energy = av1_log_block_var(cpi, x, bsize);

  if (bsize > cpi->sf.use_square_partition_only_threshold) {
    partition_horz_allowed &= !has_rows;
    partition_vert_allowed &= !has_cols;
  }

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  save_context(x, &x_ctx, mi_row, mi_col, bsize, num_planes);

  const int try_intra_cnn_split =
      frame_is_intra_only(cm) && cpi->sf.intra_cnn_split &&
      cm->seq_params.sb_size >= BLOCK_64X64 && bsize <= BLOCK_64X64 &&
      bsize >= BLOCK_8X8 && mi_row + mi_size_high[bsize] <= cm->mi_rows &&
      mi_col + mi_size_wide[bsize] <= cm->mi_cols;

  if (try_intra_cnn_split) {
    av1_intra_mode_cnn_partition(
        &cpi->common, x, bsize, x->quad_tree_idx, &partition_none_allowed,
        &partition_horz_allowed, &partition_vert_allowed, &do_rectangular_split,
        &do_square_split);
  }

  // Use simple_motion_search to prune partitions. This must be done prior to
  // PARTITION_SPLIT to propagate the initial mvs to a smaller blocksize.
  const int try_split_only =
      cpi->sf.simple_motion_search_split && do_square_split &&
      bsize >= BLOCK_8X8 && mi_row + mi_size_high[bsize] <= cm->mi_rows &&
      mi_col + mi_size_wide[bsize] <= cm->mi_cols && !frame_is_intra_only(cm) &&
      !av1_superres_scaled(cm);

#if CONFIG_EXT_RECUR_PARTITIONS
  if (try_split_only && sms_tree) {
#else
  if (try_split_only) {
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    av1_simple_motion_search_based_split(
        cpi, x, sms_tree, mi_row, mi_col, bsize, &partition_none_allowed,
        &partition_horz_allowed, &partition_vert_allowed, &do_rectangular_split,
        &do_square_split);
  }

  const int try_prune_rect =
      cpi->sf.simple_motion_search_prune_rect && !frame_is_intra_only(cm) &&
      do_rectangular_split &&
      (do_square_split || partition_none_allowed ||
       (prune_horz && prune_vert)) &&
      (partition_horz_allowed || partition_vert_allowed) && bsize >= BLOCK_8X8;

#if CONFIG_EXT_RECUR_PARTITIONS
  if (try_prune_rect && sms_tree) {
#else
  if (try_prune_rect) {
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    av1_simple_motion_search_prune_part(
        cpi, x, sms_tree, mi_row, mi_col, bsize, &partition_none_allowed,
        &partition_horz_allowed, &partition_vert_allowed, &do_square_split,
        &do_rectangular_split, &prune_horz, &prune_vert);
  }

  // Max and min square partition levels are defined as the partition nodes that
  // the recursive function rd_pick_partition() can reach. To implement this:
  // only PARTITION_NONE is allowed if the current node equals min_sq_part,
  // only PARTITION_SPLIT is allowed if the current node exceeds max_sq_part.
  assert(block_size_wide[min_sq_part] == block_size_high[min_sq_part]);
  assert(block_size_wide[max_sq_part] == block_size_high[max_sq_part]);
  assert(min_sq_part <= max_sq_part);
  const int max_partition_size = block_size_wide[max_sq_part];
  const int min_partition_size = block_size_wide[min_sq_part];
  const int blksize = block_size_wide[bsize];
  assert(min_partition_size <= max_partition_size);
  const int is_le_min_sq_part = blksize <= min_partition_size;
  const int is_gt_max_sq_part = blksize > max_partition_size;
  if (is_gt_max_sq_part) {
    // If current block size is larger than max, only allow split.
    partition_none_allowed = 0;
    partition_horz_allowed = 0;
    partition_vert_allowed = 0;
    do_square_split = 1;
  } else if (is_le_min_sq_part) {
    // If current block size is less or equal to min, only allow none if valid
    // block large enough; only allow split otherwise.
    partition_horz_allowed = 0;
    partition_vert_allowed = 0;
    // only disable square split when current block is not at the picture
    // boundary. otherwise, inherit the square split flag from previous logic
    if (has_rows && has_cols) do_square_split = 0;
    partition_none_allowed = !do_square_split;
  }

BEGIN_PARTITION_SEARCH:
  if (x->must_find_valid_partition) {
    do_square_split = is_block_splittable && is_square_block(bsize);
    partition_none_allowed = has_rows && has_cols;
    partition_horz_allowed =
        has_cols && is_partition_valid(bsize, PARTITION_HORZ) &&
        cpi->oxcf.enable_rect_partitions && is_chroma_size_valid_horz;
    partition_vert_allowed =
        has_rows && is_partition_valid(bsize, PARTITION_VERT) &&
        cpi->oxcf.enable_rect_partitions && is_chroma_size_valid_vert;
    terminate_partition_search = 0;
#if CONFIG_EXT_RECUR_PARTITIONS
    if (!is_square_block(bsize)) {
      if (!partition_horz_allowed && !partition_vert_allowed &&
          !partition_none_allowed) {
        if (block_size_wide[bsize] > block_size_high[bsize])
          partition_vert_allowed = 1;
        else
          partition_horz_allowed = 1;
      }
    }
#endif
  }

  // Partition block source pixel variance.
  unsigned int pb_source_variance = UINT_MAX;

  // Partition block sse after simple motion compensation, not in use now,
  // but will be used for upcoming speed features
  unsigned int pb_simple_motion_pred_sse = UINT_MAX;
  (void)pb_simple_motion_pred_sse;

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8) {
    if (block_size_high[bsize] <= 8) partition_horz_allowed = 0;
    if (block_size_wide[bsize] <= 8) partition_vert_allowed = 0;
    if (block_size_high[bsize] <= 8 || block_size_wide[bsize] <= 8)
      do_square_split = 0;
  }
#endif

  // PARTITION_NONE
  pc_tree->none =
      av1_alloc_pmc(cm, mi_row, mi_col, bsize, pc_tree, PARTITION_NONE, 0, ss_x,
                    ss_y, &td->shared_coeff_buf);
  PICK_MODE_CONTEXT *ctx_none = pc_tree->none;
  if (is_le_min_sq_part && has_rows && has_cols) partition_none_allowed = 1;
  int64_t part_none_rd = INT64_MAX;
  if (!terminate_partition_search && partition_none_allowed &&
      !is_gt_max_sq_part) {
    int pt_cost = 0;
    if (is_block_splittable) {
      pt_cost = partition_cost[PARTITION_NONE] < INT_MAX
                    ? partition_cost[PARTITION_NONE]
                    : 0;
    }
    RD_STATS partition_rdcost;
    av1_init_rd_stats(&partition_rdcost);
    partition_rdcost.rate = pt_cost;
    av1_rd_cost_update(x->rdmult, &partition_rdcost);
    RD_STATS best_remain_rdcost;
    av1_rd_stats_subtraction(x->rdmult, &best_rdc, &partition_rdcost,
                             &best_remain_rdcost);
#if CONFIG_COLLECT_PARTITION_STATS
    if (best_remain_rdcost >= 0) {
      partition_attempts[PARTITION_NONE] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
    pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc, PARTITION_NONE,
                  bsize, ctx_none, best_remain_rdcost, PICK_MODE_RD);
    av1_rd_cost_update(x->rdmult, &this_rdc);
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_NONE] += time;
      partition_timer_on = 0;
    }
#endif
    pb_source_variance = x->source_variance;
    pb_simple_motion_pred_sse = x->simple_motion_pred_sse;
    if (none_rd) *none_rd = this_rdc.rdcost;
    cur_none_rd = this_rdc.rdcost;
    if (this_rdc.rate != INT_MAX) {
      if (cpi->sf.prune_ref_frame_for_rect_partitions) {
        const int ref_type = av1_ref_frame_type(ctx_none->mic.ref_frame);
        update_picked_ref_frames_mask(x, ref_type, bsize,
                                      cm->seq_params.mib_size, mi_row, mi_col);
      }
      if (is_block_splittable) {
        this_rdc.rate += pt_cost;
        this_rdc.rdcost = RDCOST(x->rdmult, this_rdc.rate, this_rdc.dist);
      }

      part_none_rd = this_rdc.rdcost;
      if (this_rdc.rdcost < best_rdc.rdcost) {
        // Adjust dist breakout threshold according to the partition size.
        const int64_t dist_breakout_thr =
            cpi->sf.partition_search_breakout_dist_thr >>
            ((2 * (MAX_SB_SIZE_LOG2 - 2)) -
             (mi_size_wide_log2[bsize] + mi_size_high_log2[bsize]));
        const int rate_breakout_thr =
            cpi->sf.partition_search_breakout_rate_thr *
            num_pels_log2_lookup[bsize];

        best_rdc = this_rdc;
        found_best_partition = true;
        pc_tree->partitioning = PARTITION_NONE;

        if (!frame_is_intra_only(cm) &&
            (do_square_split || do_rectangular_split) &&
            !x->e_mbd.lossless[xd->mi[0]->segment_id] && ctx_none->skippable) {
          const int use_ml_based_breakout =
#if CONFIG_EXT_RECUR_PARTITIONS
              is_square_block(bsize) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
              bsize <= cpi->sf.use_square_partition_only_threshold &&
              bsize > BLOCK_4X4 && xd->bd == 8;
          if (use_ml_based_breakout) {
            if (av1_ml_predict_breakout(cpi, bsize, x, &this_rdc,
                                        pb_source_variance)) {
              do_square_split = 0;
              do_rectangular_split = 0;
            }
          }

          // If all y, u, v transform blocks in this partition are skippable,
          // and the dist & rate are within the thresholds, the partition
          // search is terminated for current branch of the partition search
          // tree. The dist & rate thresholds are set to 0 at speed 0 to
          // disable the early termination at that speed.
          if (best_rdc.dist < dist_breakout_thr &&
              best_rdc.rate < rate_breakout_thr) {
            do_square_split = 0;
            do_rectangular_split = 0;
          }
        }

        if (cpi->sf.simple_motion_search_early_term_none && cm->show_frame &&
            !frame_is_intra_only(cm) && bsize >= BLOCK_16X16 &&
#if CONFIG_EXT_RECUR_PARTITIONS
            sms_tree &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
            mi_row + mi_step_h < cm->mi_rows &&
            mi_col + mi_step_w < cm->mi_cols && this_rdc.rdcost < INT64_MAX &&
            this_rdc.rdcost >= 0 && this_rdc.rate < INT_MAX &&
            this_rdc.rate >= 0 && (do_square_split || do_rectangular_split)) {
          av1_simple_motion_search_early_term_none(cpi, x, sms_tree, mi_row,
                                                   mi_col, bsize, &this_rdc,
                                                   &terminate_partition_search);
        }
      }
    }

    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // store estimated motion vector
  if (cpi->sf.adaptive_motion_search) store_pred_mv(x, ctx_none);

  // PARTITION_SPLIT
  int64_t part_split_rd = INT64_MAX;
  subsize = get_partition_subsize(bsize, PARTITION_SPLIT);
#if CONFIG_EXT_RECUR_PARTITIONS
  if (((!terminate_partition_search && do_square_split) || is_gt_max_sq_part) &&
      is_partition_valid(bsize, PARTITION_SPLIT)) {
#else
  if ((!terminate_partition_search && do_square_split) || is_gt_max_sq_part) {
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    for (int i = 0; i < 4; ++i) {
      const int x_idx = (i & 1) * mi_step_w;
      const int y_idx = (i >> 1) * mi_step_h;
      pc_tree->split[i] = av1_alloc_pc_tree_node(
          mi_row + y_idx, mi_col + x_idx, subsize, pc_tree, PARTITION_SPLIT, i,
          i == 3, ss_x, ss_y);
    }
    av1_init_rd_stats(&sum_rdc);
    sum_rdc.rate = partition_cost[PARTITION_SPLIT];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);

    int idx;
#if CONFIG_COLLECT_PARTITION_STATS
    if (best_rdc.rdcost - sum_rdc.rdcost >= 0) {
      partition_attempts[PARTITION_SPLIT] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
    for (idx = 0; idx < 4 && sum_rdc.rdcost < best_rdc.rdcost; ++idx) {
      const int x_idx = (idx & 1) * mi_step_w;
      const int y_idx = (idx >> 1) * mi_step_h;

      if (mi_row + y_idx >= cm->mi_rows || mi_col + x_idx >= cm->mi_cols)
        continue;

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      int64_t *p_split_rd = &split_rd[idx];

      RD_STATS best_remain_rdcost;
      av1_rd_stats_subtraction(x->rdmult, &best_rdc, &sum_rdc,
                               &best_remain_rdcost);

      int curr_quad_tree_idx = 0;
      if (frame_is_intra_only(cm) && bsize <= BLOCK_64X64) {
        curr_quad_tree_idx = x->quad_tree_idx;
        x->quad_tree_idx = 4 * curr_quad_tree_idx + idx + 1;
      }
      if (!rd_pick_partition(cpi, td, tile_data, tp, mi_row + y_idx,
                             mi_col + x_idx, subsize, max_sq_part, min_sq_part,
                             &this_rdc, best_remain_rdcost, pc_tree->split[idx],
#if CONFIG_EXT_RECUR_PARTITIONS
                             sms_tree ? sms_tree->split[idx] : NULL, p_split_rd,
                             multi_pass_mode)) {
#else
                             sms_tree->split[idx], p_split_rd,
                             multi_pass_mode)) {
#endif  // CONFIG_EXT_RECUR_PARTITIONS
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }
      if (frame_is_intra_only(cm) && bsize <= BLOCK_64X64) {
        x->quad_tree_idx = curr_quad_tree_idx;
      }

      sum_rdc.rate += this_rdc.rate;
      sum_rdc.dist += this_rdc.dist;
      av1_rd_cost_update(x->rdmult, &sum_rdc);
#if !CONFIG_EXT_RECUR_PARTITIONS
      if (idx <= 1 && (bsize <= BLOCK_8X8 ||
                       pc_tree->split[idx]->partitioning == PARTITION_NONE)) {
        const MB_MODE_INFO *const mbmi = &pc_tree->split[idx]->none->mic;
        const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
        // Neither palette mode nor cfl predicted
        if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
          if (mbmi->uv_mode != UV_CFL_PRED) split_ctx_is_ready[idx] = 1;
        }
      }
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
    }
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_SPLIT] += time;
      partition_timer_on = 0;
    }
#endif
    const int reached_last_index = (idx == 4);

    part_split_rd = sum_rdc.rdcost;
    if (reached_last_index && sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        found_best_partition = true;
        pc_tree->partitioning = PARTITION_SPLIT;
      }
    } else if (cpi->sf.less_rectangular_check_level > 0) {
      // Skip rectangular partition test when partition type none gives better
      // rd than partition type split.
      if (cpi->sf.less_rectangular_check_level == 2 || idx <= 2) {
        const int partition_none_valid = cur_none_rd > 0;
        const int partition_none_better = cur_none_rd < sum_rdc.rdcost;
        do_rectangular_split &=
            !(partition_none_valid && partition_none_better);
      }
    }

    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }  // if (do_split)

  if (cpi->sf.ml_early_term_after_part_split_level &&
      !frame_is_intra_only(cm) && !terminate_partition_search &&
#if CONFIG_EXT_RECUR_PARTITIONS
      sms_tree &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      do_rectangular_split &&
      (partition_horz_allowed || partition_vert_allowed)) {
    av1_ml_early_term_after_split(cpi, x, sms_tree, bsize, best_rdc.rdcost,
                                  part_none_rd, part_split_rd, split_rd, mi_row,
                                  mi_col, &terminate_partition_search);
  }

  if (!cpi->sf.ml_early_term_after_part_split_level &&
      cpi->sf.ml_prune_rect_partition && !frame_is_intra_only(cm) &&
      (partition_horz_allowed || partition_vert_allowed) &&
      !(prune_horz || prune_vert) && !terminate_partition_search) {
    av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes,
                         &pc_tree->chroma_ref_info);
    av1_ml_prune_rect_partition(cpi, x, bsize, best_rdc.rdcost, cur_none_rd,
                                split_rd, &prune_horz, &prune_vert);
  }

  // PARTITION_HORZ
  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !partition_horz_allowed));
  if (!terminate_partition_search && partition_horz_allowed && !prune_horz &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_HORZ) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      (do_rectangular_split || active_h_edge(cpi, mi_row, mi_step_h)) &&
      !is_gt_max_sq_part) {
    av1_init_rd_stats(&sum_rdc);
    subsize = get_partition_subsize(bsize, PARTITION_HORZ);
#if CONFIG_EXT_RECUR_PARTITIONS
    pc_tree->horizontal[0] = av1_alloc_pc_tree_node(
        mi_row, mi_col, subsize, pc_tree, PARTITION_HORZ, 0, 0, ss_x, ss_y);
    pc_tree->horizontal[1] =
        av1_alloc_pc_tree_node(mi_row + mi_step_h, mi_col, subsize, pc_tree,
                               PARTITION_HORZ, 1, 1, ss_x, ss_y);
#else
    for (int i = 0; i < 2; ++i) {
      pc_tree->horizontal[i] =
          av1_alloc_pmc(cm, mi_row + mi_step_h * i, mi_col, subsize, pc_tree,
                        PARTITION_HORZ, i, ss_x, ss_y, &td->shared_coeff_buf);
    }
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);
    sum_rdc.rate = partition_cost[PARTITION_HORZ];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);
    RD_STATS best_remain_rdcost;
    av1_rd_stats_subtraction(x->rdmult, &best_rdc, &sum_rdc,
                             &best_remain_rdcost);
#if CONFIG_COLLECT_PARTITION_STATS
    if (best_remain_rdcost >= 0) {
      partition_attempts[PARTITION_HORZ] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
#if CONFIG_EXT_RECUR_PARTITIONS
    rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, subsize,
                      max_sq_part, min_sq_part, &this_rdc, best_remain_rdcost,
                      pc_tree->horizontal[0], NULL, NULL, multi_pass_mode);
#else
    pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc, PARTITION_HORZ,
                  subsize, pc_tree->horizontal[0], best_remain_rdcost,
                  PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    av1_rd_cost_update(x->rdmult, &this_rdc);

    if (this_rdc.rate == INT_MAX) {
      sum_rdc.rdcost = INT64_MAX;
    } else {
      sum_rdc.rate += this_rdc.rate;
      sum_rdc.dist += this_rdc.dist;
      av1_rd_cost_update(x->rdmult, &sum_rdc);
    }
    horz_rd[0] = this_rdc.rdcost;

    if (sum_rdc.rdcost < best_rdc.rdcost && has_rows) {
#if CONFIG_EXT_RECUR_PARTITIONS
      const PICK_MODE_CONTEXT *const ctx_h = pc_tree->horizontal[0]->none;
#else
      const PICK_MODE_CONTEXT *const ctx_h = pc_tree->horizontal[0];
      const MB_MODE_INFO *const mbmi = &pc_tree->horizontal[0]->mic;
      const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
      // Neither palette mode nor cfl predicted
      if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
        if (mbmi->uv_mode != UV_CFL_PRED) horz_ctx_is_ready = 1;
      }
      update_state(cpi, td, ctx_h, mi_row, mi_col, subsize, 1);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize, NULL);
#endif  // CONFIG_EXT_RECUR_PARTITIONS

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_h);

      av1_rd_stats_subtraction(x->rdmult, &best_rdc, &sum_rdc,
                               &best_remain_rdcost);

#if CONFIG_EXT_RECUR_PARTITIONS
      rd_pick_partition(cpi, td, tile_data, tp, mi_row + mi_step_h, mi_col,
                        subsize, max_sq_part, min_sq_part, &this_rdc,
                        best_remain_rdcost, pc_tree->horizontal[1], NULL, NULL,
                        multi_pass_mode);
#else
      pick_sb_modes(cpi, tile_data, x, mi_row + mi_step_h, mi_col, &this_rdc,
                    PARTITION_HORZ, subsize, pc_tree->horizontal[1],
                    best_remain_rdcost, PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      av1_rd_cost_update(x->rdmult, &this_rdc);
      horz_rd[1] = this_rdc.rdcost;

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        av1_rd_cost_update(x->rdmult, &sum_rdc);
      }
    }
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_HORZ] += time;
      partition_timer_on = 0;
    }
#endif

    if (sum_rdc.rdcost < best_rdc.rdcost) {
      sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, sum_rdc.dist);
      if (sum_rdc.rdcost < best_rdc.rdcost) {
        best_rdc = sum_rdc;
        found_best_partition = true;
        pc_tree->partitioning = PARTITION_HORZ;
      }
    }

    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT
  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !partition_vert_allowed));
  if (!terminate_partition_search && partition_vert_allowed && !prune_vert &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_VERT) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      (do_rectangular_split || active_v_edge(cpi, mi_col, mi_step_w)) &&
      !is_gt_max_sq_part) {
    av1_init_rd_stats(&sum_rdc);
    subsize = get_partition_subsize(bsize, PARTITION_VERT);
#if CONFIG_EXT_RECUR_PARTITIONS
    pc_tree->vertical[0] = av1_alloc_pc_tree_node(
        mi_row, mi_col, subsize, pc_tree, PARTITION_VERT, 0, 0, ss_x, ss_y);
    pc_tree->vertical[1] =
        av1_alloc_pc_tree_node(mi_row, mi_col + mi_step_w, subsize, pc_tree,
                               PARTITION_VERT, 1, 1, ss_x, ss_y);
#else
    for (int i = 0; i < 2; ++i) {
      pc_tree->vertical[i] =
          av1_alloc_pmc(cm, mi_row, mi_col + mi_step_w * i, subsize, pc_tree,
                        PARTITION_VERT, i, ss_x, ss_y, &td->shared_coeff_buf);
    }
#endif  // CONFIG_EXT_RECUR_PARTITIONS

    if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

    sum_rdc.rate = partition_cost[PARTITION_VERT];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);
    RD_STATS best_remain_rdcost;
    av1_rd_stats_subtraction(x->rdmult, &best_rdc, &sum_rdc,
                             &best_remain_rdcost);
#if CONFIG_COLLECT_PARTITION_STATS
    if (best_remain_rdcost >= 0) {
      partition_attempts[PARTITION_VERT] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
#if CONFIG_EXT_RECUR_PARTITIONS
    rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, subsize,
                      max_sq_part, min_sq_part, &this_rdc, best_remain_rdcost,
                      pc_tree->vertical[0], NULL, NULL, multi_pass_mode);
#else
    pick_sb_modes(cpi, tile_data, x, mi_row, mi_col, &this_rdc, PARTITION_VERT,
                  subsize, pc_tree->vertical[0], best_remain_rdcost,
                  PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    av1_rd_cost_update(x->rdmult, &this_rdc);

    if (this_rdc.rate == INT_MAX) {
      sum_rdc.rdcost = INT64_MAX;
    } else {
      sum_rdc.rate += this_rdc.rate;
      sum_rdc.dist += this_rdc.dist;
      av1_rd_cost_update(x->rdmult, &sum_rdc);
    }
    vert_rd[0] = this_rdc.rdcost;
    if (sum_rdc.rdcost < best_rdc.rdcost && has_cols) {
#if !CONFIG_EXT_RECUR_PARTITIONS
      const MB_MODE_INFO *const mbmi = &pc_tree->vertical[0]->mic;
      const PALETTE_MODE_INFO *const pmi = &mbmi->palette_mode_info;
      // Neither palette mode nor cfl predicted
      if (pmi->palette_size[0] == 0 && pmi->palette_size[1] == 0) {
        if (mbmi->uv_mode != UV_CFL_PRED) vert_ctx_is_ready = 1;
      }
      update_state(cpi, td, pc_tree->vertical[0], mi_row, mi_col, subsize, 1);
      encode_superblock(cpi, tile_data, td, tp, DRY_RUN_NORMAL, subsize, NULL);
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

      if (cpi->sf.adaptive_motion_search) load_pred_mv(x, ctx_none);

      av1_rd_stats_subtraction(x->rdmult, &best_rdc, &sum_rdc,
                               &best_remain_rdcost);
#if CONFIG_EXT_RECUR_PARTITIONS
      rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col + mi_step_w,
                        subsize, max_sq_part, min_sq_part, &this_rdc,
                        best_remain_rdcost, pc_tree->vertical[1], NULL, NULL,
                        multi_pass_mode);
#else
      pick_sb_modes(cpi, tile_data, x, mi_row, mi_col + mi_step_w, &this_rdc,
                    PARTITION_VERT, subsize, pc_tree->vertical[1],
                    best_remain_rdcost, PICK_MODE_RD);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      av1_rd_cost_update(x->rdmult, &this_rdc);
      vert_rd[1] = this_rdc.rdcost;

      if (this_rdc.rate == INT_MAX) {
        sum_rdc.rdcost = INT64_MAX;
      } else {
        sum_rdc.rate += this_rdc.rate;
        sum_rdc.dist += this_rdc.dist;
        av1_rd_cost_update(x->rdmult, &sum_rdc);
      }
    }
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_VERT] += time;
      partition_timer_on = 0;
    }
#endif

    av1_rd_cost_update(x->rdmult, &sum_rdc);
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      best_rdc = sum_rdc;
      found_best_partition = true;
      pc_tree->partitioning = PARTITION_VERT;
    }

    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  if (pb_source_variance == UINT_MAX) {
    av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes,
                         &pc_tree->chroma_ref_info);
    if (is_cur_buf_hbd(xd)) {
      pb_source_variance = av1_high_get_sby_perpixel_variance(
          cpi, &x->plane[0].src, bsize, xd->bd);
    } else {
      pb_source_variance =
          av1_get_sby_perpixel_variance(cpi, &x->plane[0].src, bsize);
    }
  }

  if (use_pb_simple_motion_pred_sse(cpi) &&
      pb_simple_motion_pred_sse == UINT_MAX) {
    const MV ref_mv_full = { .row = 0, .col = 0 };
    unsigned int var = 0;

    av1_simple_motion_sse_var(cpi, x, mi_row, mi_col, bsize, ref_mv_full, 0,
                              &pb_simple_motion_pred_sse, &var);
  }

  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !do_rectangular_split));

  const int ext_partition_allowed =
      do_rectangular_split && bsize > BLOCK_8X8 && has_rows && has_cols;

  // The standard AB partitions are allowed whenever ext-partition-types are
  // allowed
  int horzab_partition_allowed =
      ext_partition_allowed & cpi->oxcf.enable_ab_partitions;
  int vertab_partition_allowed =
      ext_partition_allowed & cpi->oxcf.enable_ab_partitions;

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8) {
    if (block_size_high[bsize] <= 8 || block_size_wide[bsize] <= 8) {
      horzab_partition_allowed = 0;
      vertab_partition_allowed = 0;
    }
  }
#endif

  if (cpi->sf.prune_ext_partition_types_search_level) {
    if (cpi->sf.prune_ext_partition_types_search_level == 1) {
      // TODO(debargha,huisu@google.com): may need to tune the threshold for
      // pb_source_variance.
      horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                   (pc_tree->partitioning == PARTITION_NONE &&
                                    pb_source_variance < 32) ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
      vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                   (pc_tree->partitioning == PARTITION_NONE &&
                                    pb_source_variance < 32) ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
    } else {
      horzab_partition_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
      vertab_partition_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
                                   pc_tree->partitioning == PARTITION_SPLIT);
    }
    horz_rd[0] = (horz_rd[0] < INT64_MAX ? horz_rd[0] : 0);
    horz_rd[1] = (horz_rd[1] < INT64_MAX ? horz_rd[1] : 0);
    vert_rd[0] = (vert_rd[0] < INT64_MAX ? vert_rd[0] : 0);
    vert_rd[1] = (vert_rd[1] < INT64_MAX ? vert_rd[1] : 0);
    split_rd[0] = (split_rd[0] < INT64_MAX ? split_rd[0] : 0);
    split_rd[1] = (split_rd[1] < INT64_MAX ? split_rd[1] : 0);
    split_rd[2] = (split_rd[2] < INT64_MAX ? split_rd[2] : 0);
    split_rd[3] = (split_rd[3] < INT64_MAX ? split_rd[3] : 0);
  }
  int horza_partition_allowed = horzab_partition_allowed;
  int horzb_partition_allowed = horzab_partition_allowed;
  if (cpi->sf.prune_ext_partition_types_search_level) {
    const int64_t horz_a_rd = horz_rd[1] + split_rd[0] + split_rd[1];
    const int64_t horz_b_rd = horz_rd[0] + split_rd[2] + split_rd[3];
    switch (cpi->sf.prune_ext_partition_types_search_level) {
      case 1:
        horza_partition_allowed &= (horz_a_rd / 16 * 14 < best_rdc.rdcost);
        horzb_partition_allowed &= (horz_b_rd / 16 * 14 < best_rdc.rdcost);
        break;
      case 2:
      default:
        horza_partition_allowed &= (horz_a_rd / 16 * 15 < best_rdc.rdcost);
        horzb_partition_allowed &= (horz_b_rd / 16 * 15 < best_rdc.rdcost);
        break;
    }
  }

  int verta_partition_allowed = vertab_partition_allowed;
  int vertb_partition_allowed = vertab_partition_allowed;
  if (cpi->sf.prune_ext_partition_types_search_level) {
    const int64_t vert_a_rd = vert_rd[1] + split_rd[0] + split_rd[2];
    const int64_t vert_b_rd = vert_rd[0] + split_rd[1] + split_rd[3];
    switch (cpi->sf.prune_ext_partition_types_search_level) {
      case 1:
        verta_partition_allowed &= (vert_a_rd / 16 * 14 < best_rdc.rdcost);
        vertb_partition_allowed &= (vert_b_rd / 16 * 14 < best_rdc.rdcost);
        break;
      case 2:
      default:
        verta_partition_allowed &= (vert_a_rd / 16 * 15 < best_rdc.rdcost);
        vertb_partition_allowed &= (vert_b_rd / 16 * 15 < best_rdc.rdcost);
        break;
    }
  }

  if (cpi->sf.ml_prune_ab_partition && ext_partition_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_square_block(bsize) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      partition_horz_allowed && partition_vert_allowed) {
    // TODO(huisu@google.com): x->source_variance may not be the current
    // block's variance. The correct one to use is pb_source_variance. Need to
    // re-train the model to fix it.
    av1_ml_prune_ab_partition(
        bsize, pc_tree->partitioning, get_unsigned_bits(x->source_variance),
        best_rdc.rdcost, horz_rd, vert_rd, split_rd, &horza_partition_allowed,
        &horzb_partition_allowed, &verta_partition_allowed,
        &vertb_partition_allowed);
  }

#if !CONFIG_EXT_RECUR_PARTITIONS
  horza_partition_allowed &= cpi->oxcf.enable_ab_partitions;
  horzb_partition_allowed &= cpi->oxcf.enable_ab_partitions;
  verta_partition_allowed &= cpi->oxcf.enable_ab_partitions;
  vertb_partition_allowed &= cpi->oxcf.enable_ab_partitions;

  // PARTITION_HORZ_A
  if (!terminate_partition_search && partition_horz_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_HORZ_A) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      horza_partition_allowed && !is_gt_max_sq_part) {
    subsize = get_partition_subsize(bsize, PARTITION_HORZ_A);

#if CONFIG_EXT_PARTITIONS
    pc_tree->horza_split[0] = av1_alloc_pc_tree_node(
        mi_row, mi_col, bsize2, pc_tree, PARTITION_HORZ_A, 0, 0, ss_x, ss_y);
    pc_tree->horza_split[1] =
        av1_alloc_pc_tree_node(mi_row, mi_col + mi_step_w, bsize2, pc_tree,
                               PARTITION_HORZ_A, 1, 0, ss_x, ss_y);
    pc_tree->horza_rec =
        av1_alloc_pmc(cm, mi_row + mi_step_h, mi_col, subsize, pc_tree,
                      PARTITION_HORZ_A, 2, ss_x, ss_y, &td->shared_coeff_buf);

    pc_tree->horza_rec->rd_mode_is_ready = 0;
    // TODO(yuec): implement tree copy to reduce duplicated RD search

    SUBBLOCK_RDO_DATA subblock0 = { sms_tree ? sms_tree->split[0] : NULL,
                                    pc_tree->horza_split[0],
                                    NULL,
                                    mi_row,
                                    mi_col,
                                    bsize2,
                                    PARTITION_HORZ_A,
                                    0,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA subblock1 = { sms_tree ? sms_tree->split[1] : NULL,
                                    pc_tree->horza_split[1],
                                    NULL,
                                    mi_row,
                                    mi_col + mi_step_w,
                                    bsize2,
                                    PARTITION_HORZ_A,
                                    0,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA subblock2 = {
      NULL,   NULL,        pc_tree->horza_rec, mi_row + mi_step_h,
      mi_col, subsize,     PARTITION_HORZ_A,   1,
      0,      max_sq_part, min_sq_part
    };
    SUBBLOCK_RDO_DATA *subblocks[3] = { &subblock0, &subblock1, &subblock2 };
#else
    pc_tree->horizontala[0] =
        av1_alloc_pmc(cm, mi_row, mi_col, bsize2, pc_tree, PARTITION_HORZ_A, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horizontala[1] =
        av1_alloc_pmc(cm, mi_row, mi_col + mi_step_w, bsize2, pc_tree,
                      PARTITION_HORZ_A, 1, ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horizontala[2] =
        av1_alloc_pmc(cm, mi_row + mi_step_h, mi_col, subsize, pc_tree,
                      PARTITION_HORZ_A, 2, ss_x, ss_y, &td->shared_coeff_buf);

    pc_tree->horizontala[0]->rd_mode_is_ready = 0;
    pc_tree->horizontala[1]->rd_mode_is_ready = 0;
    pc_tree->horizontala[2]->rd_mode_is_ready = 0;
    if (split_ctx_is_ready[0]) {
      av1_copy_tree_context(pc_tree->horizontala[0], pc_tree->split[0]->none);
      pc_tree->horizontala[0]->mic.partition = PARTITION_HORZ_A;
      pc_tree->horizontala[0]->rd_mode_is_ready = 1;
      if (split_ctx_is_ready[1]) {
        av1_copy_tree_context(pc_tree->horizontala[1], pc_tree->split[1]->none);
        pc_tree->horizontala[1]->mic.partition = PARTITION_HORZ_A;
        pc_tree->horizontala[1]->rd_mode_is_ready = 1;
      }
    }
#endif  // CONFIG_EXT_PARTITIONS
#if CONFIG_COLLECT_PARTITION_STATS
    {
      RD_STATS tmp_sum_rdc;
      av1_init_rd_stats(&tmp_sum_rdc);
      tmp_sum_rdc.rate = x->partition_cost[pl][PARTITION_HORZ_A];
      tmp_sum_rdc.rdcost = RDCOST(x->rdmult, tmp_sum_rdc.rate, 0);
      if (best_rdc.rdcost - tmp_sum_rdc.rdcost >= 0) {
        partition_attempts[PARTITION_HORZ_A] += 1;
        aom_usec_timer_start(&partition_timer);
        partition_timer_on = 1;
      }
    }
#endif
#if CONFIG_EXT_PARTITIONS
    found_best_partition |= rd_test_partition_abpart(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, mi_row, mi_col, bsize,
        PARTITION_HORZ_A, subblocks, multi_pass_mode);
#else
    found_best_partition |= rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->horizontala,
        ctx_none, mi_row, mi_col, bsize, PARTITION_HORZ_A, mi_row, mi_col,
        bsize2, mi_row, mi_col + mi_step_w, bsize2, mi_row + mi_step_h, mi_col,
        subsize);
#endif  // CONFIG_EXT_PARTITIONS
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_HORZ_A] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
  // PARTITION_HORZ_B
  if (!terminate_partition_search && partition_horz_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_HORZ_B) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      horzb_partition_allowed && !is_gt_max_sq_part) {
    subsize = get_partition_subsize(bsize, PARTITION_HORZ_B);

#if CONFIG_EXT_PARTITIONS
    pc_tree->horzb_rec =
        av1_alloc_pmc(cm, mi_row, mi_col, subsize, pc_tree, PARTITION_HORZ_B, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horzb_split[0] =
        av1_alloc_pc_tree_node(mi_row + mi_step_h, mi_col, bsize2, pc_tree,
                               PARTITION_HORZ_B, 1, 0, ss_x, ss_y);
    pc_tree->horzb_split[1] =
        av1_alloc_pc_tree_node(mi_row + mi_step_h, mi_col + mi_step_w, bsize2,
                               pc_tree, PARTITION_HORZ_B, 2, 1, ss_x, ss_y);

    pc_tree->horzb_rec->rd_mode_is_ready = 0;
    if (horz_ctx_is_ready) {
      av1_copy_tree_context(pc_tree->horzb_rec, pc_tree->horizontal[0]);
      pc_tree->horzb_rec->mic.partition = PARTITION_HORZ_B;
      pc_tree->horzb_rec->rd_mode_is_ready = 1;
    }

    SUBBLOCK_RDO_DATA subblock0 = {
      NULL,   NULL,        pc_tree->horzb_rec, mi_row,
      mi_col, subsize,     PARTITION_HORZ_B,   0,
      0,      max_sq_part, min_sq_part
    };
    SUBBLOCK_RDO_DATA subblock1 = { sms_tree ? sms_tree->split[2] : NULL,
                                    pc_tree->horzb_split[0],
                                    NULL,
                                    mi_row + mi_step_h,
                                    mi_col,
                                    bsize2,
                                    PARTITION_HORZ_B,
                                    0,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA subblock2 = { sms_tree ? sms_tree->split[3] : NULL,
                                    pc_tree->horzb_split[1],
                                    NULL,
                                    mi_row + mi_step_h,
                                    mi_col + mi_step_w,
                                    bsize2,
                                    PARTITION_HORZ_B,
                                    1,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA *subblocks[3] = { &subblock0, &subblock1, &subblock2 };
#else
    pc_tree->horizontalb[0] =
        av1_alloc_pmc(cm, mi_row, mi_col, subsize, pc_tree, PARTITION_HORZ_B, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horizontalb[1] =
        av1_alloc_pmc(cm, mi_row + mi_step_h, mi_col, bsize2, pc_tree,
                      PARTITION_HORZ_B, 1, ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horizontalb[2] = av1_alloc_pmc(
        cm, mi_row + mi_step_h, mi_col + mi_step_w, bsize2, pc_tree,
        PARTITION_HORZ_B, 2, ss_x, ss_y, &td->shared_coeff_buf);

    pc_tree->horizontalb[0]->rd_mode_is_ready = 0;
    pc_tree->horizontalb[1]->rd_mode_is_ready = 0;
    pc_tree->horizontalb[2]->rd_mode_is_ready = 0;
    if (horz_ctx_is_ready) {
#if CONFIG_EXT_RECUR_PARTITIONS
      av1_copy_tree_context(pc_tree->horizontalb[0],
                            pc_tree->horizontal[0]->none);
#else
      av1_copy_tree_context(pc_tree->horizontalb[0], pc_tree->horizontal[0]);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      pc_tree->horizontalb[0]->mic.partition = PARTITION_HORZ_B;
      pc_tree->horizontalb[0]->rd_mode_is_ready = 1;
    }
#endif  // CONFIG_EXT_PARTITIONS
#if CONFIG_COLLECT_PARTITION_STATS
    {
      RD_STATS tmp_sum_rdc;
      av1_init_rd_stats(&tmp_sum_rdc);
      tmp_sum_rdc.rate = x->partition_cost[pl][PARTITION_HORZ_B];
      tmp_sum_rdc.rdcost = RDCOST(x->rdmult, tmp_sum_rdc.rate, 0);
      if (best_rdc.rdcost - tmp_sum_rdc.rdcost >= 0) {
        partition_attempts[PARTITION_HORZ_B] += 1;
        aom_usec_timer_start(&partition_timer);
        partition_timer_on = 1;
      }
    }
#endif
#if CONFIG_EXT_PARTITIONS
    found_best_partition |= rd_test_partition_abpart(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, mi_row, mi_col, bsize,
        PARTITION_HORZ_B, subblocks, multi_pass_mode);
#else
    found_best_partition |= rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->horizontalb,
        ctx_none, mi_row, mi_col, bsize, PARTITION_HORZ_B, mi_row, mi_col,
        subsize, mi_row + mi_step_h, mi_col, bsize2, mi_row + mi_step_h,
        mi_col + mi_step_w, bsize2);
#endif  // CONFIG_EXT_PARTITIONS

#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_HORZ_B] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT_A
  if (!terminate_partition_search && partition_vert_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_VERT_A) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      verta_partition_allowed && !is_gt_max_sq_part) {
    subsize = get_partition_subsize(bsize, PARTITION_VERT_A);

#if CONFIG_EXT_PARTITIONS
    pc_tree->verta_split[0] = av1_alloc_pc_tree_node(
        mi_row, mi_col, bsize2, pc_tree, PARTITION_VERT_A, 0, 0, ss_x, ss_y);
    pc_tree->verta_split[1] =
        av1_alloc_pc_tree_node(mi_row + mi_step_h, mi_col, bsize2, pc_tree,
                               PARTITION_VERT_A, 1, 0, ss_x, ss_y);
    pc_tree->verta_rec =
        av1_alloc_pmc(cm, mi_row, mi_col + mi_step_w, subsize, pc_tree,
                      PARTITION_VERT_A, 2, ss_x, ss_y, &td->shared_coeff_buf);

    pc_tree->verta_rec->rd_mode_is_ready = 0;

    SUBBLOCK_RDO_DATA subblock0 = { sms_tree ? sms_tree->split[0] : NULL,
                                    pc_tree->verta_split[0],
                                    NULL,
                                    mi_row,
                                    mi_col,
                                    bsize2,
                                    PARTITION_VERT_A,
                                    0,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA subblock1 = { sms_tree ? sms_tree->split[2] : NULL,
                                    pc_tree->verta_split[1],
                                    NULL,
                                    mi_row + mi_step_h,
                                    mi_col,
                                    bsize2,
                                    PARTITION_VERT_A,
                                    0,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA subblock2 = { NULL,
                                    NULL,
                                    pc_tree->verta_rec,
                                    mi_row,
                                    mi_col + mi_step_w,
                                    subsize,
                                    PARTITION_VERT_A,
                                    1,
                                    0,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA *subblocks[3] = { &subblock0, &subblock1, &subblock2 };
#else
    pc_tree->verticala[0] =
        av1_alloc_pmc(cm, mi_row, mi_col, bsize2, pc_tree, PARTITION_VERT_A, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->verticala[1] =
        av1_alloc_pmc(cm, mi_row + mi_step_h, mi_col, bsize2, pc_tree,
                      PARTITION_VERT_A, 1, ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->verticala[2] =
        av1_alloc_pmc(cm, mi_row, mi_col + mi_step_w, subsize, pc_tree,
                      PARTITION_VERT_A, 2, ss_x, ss_y, &td->shared_coeff_buf);

    pc_tree->verticala[0]->rd_mode_is_ready = 0;
    pc_tree->verticala[1]->rd_mode_is_ready = 0;
    pc_tree->verticala[2]->rd_mode_is_ready = 0;
    if (split_ctx_is_ready[0]) {
      av1_copy_tree_context(pc_tree->verticala[0], pc_tree->split[0]->none);
      pc_tree->verticala[0]->mic.partition = PARTITION_VERT_A;
      pc_tree->verticala[0]->rd_mode_is_ready = 1;
    }
#endif
#if CONFIG_COLLECT_PARTITION_STATS
    {
      RD_STATS tmp_sum_rdc;
      av1_init_rd_stats(&tmp_sum_rdc);
      tmp_sum_rdc.rate = x->partition_cost[pl][PARTITION_VERT_A];
      tmp_sum_rdc.rdcost = RDCOST(x->rdmult, tmp_sum_rdc.rate, 0);
      if (best_rdc.rdcost - tmp_sum_rdc.rdcost >= 0) {
        partition_attempts[PARTITION_VERT_A] += 1;
        aom_usec_timer_start(&partition_timer);
        partition_timer_on = 1;
      }
    }
#endif
#if CONFIG_EXT_PARTITIONS
    found_best_partition |= rd_test_partition_abpart(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, mi_row, mi_col, bsize,
        PARTITION_VERT_A, subblocks, multi_pass_mode);
#else
    found_best_partition |= rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->verticala,
        ctx_none, mi_row, mi_col, bsize, PARTITION_VERT_A, mi_row, mi_col,
        bsize2, mi_row + mi_step_h, mi_col, bsize2, mi_row, mi_col + mi_step_w,
        subsize);
#endif  // CONFIG_EXT_PARTITIONS
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_VERT_A] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
  // PARTITION_VERT_B
  if (!terminate_partition_search && partition_vert_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_VERT_B) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      vertb_partition_allowed && !is_gt_max_sq_part) {
    subsize = get_partition_subsize(bsize, PARTITION_VERT_B);

#if CONFIG_EXT_PARTITIONS
    pc_tree->vertb_rec =
        av1_alloc_pmc(cm, mi_row, mi_col, subsize, pc_tree, PARTITION_VERT_B, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->vertb_split[0] =
        av1_alloc_pc_tree_node(mi_row, mi_col + mi_step_w, bsize2, pc_tree,
                               PARTITION_VERT_B, 1, 0, ss_x, ss_y);
    pc_tree->vertb_split[1] =
        av1_alloc_pc_tree_node(mi_row + mi_step_h, mi_col + mi_step_w, bsize2,
                               pc_tree, PARTITION_VERT_B, 2, 1, ss_x, ss_y);

    pc_tree->vertb_rec->rd_mode_is_ready = 0;
    if (vert_ctx_is_ready) {
      av1_copy_tree_context(pc_tree->vertb_rec, pc_tree->vertical[0]);
      pc_tree->vertb_rec->mic.partition = PARTITION_VERT_B;
      pc_tree->vertb_rec->rd_mode_is_ready = 1;
    }

    SUBBLOCK_RDO_DATA subblock0 = {
      NULL,   NULL,        pc_tree->vertb_rec, mi_row,
      mi_col, subsize,     PARTITION_VERT_B,   0,
      0,      max_sq_part, min_sq_part
    };
    SUBBLOCK_RDO_DATA subblock1 = { sms_tree ? sms_tree->split[1] : NULL,
                                    pc_tree->vertb_split[0],
                                    NULL,
                                    mi_row,
                                    mi_col + mi_step_w,
                                    bsize2,
                                    PARTITION_VERT_B,
                                    0,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA subblock2 = { sms_tree ? sms_tree->split[3] : NULL,
                                    pc_tree->vertb_split[1],
                                    NULL,
                                    mi_row + mi_step_h,
                                    mi_col + mi_step_w,
                                    bsize2,
                                    PARTITION_VERT_B,
                                    1,
                                    1,
                                    max_sq_part,
                                    min_sq_part };
    SUBBLOCK_RDO_DATA *subblocks[3] = { &subblock0, &subblock1, &subblock2 };
#else
    pc_tree->verticalb[0] =
        av1_alloc_pmc(cm, mi_row, mi_col, subsize, pc_tree, PARTITION_VERT_B, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->verticalb[1] =
        av1_alloc_pmc(cm, mi_row, mi_col + mi_step_w, bsize2, pc_tree,
                      PARTITION_VERT_B, 1, ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->verticalb[2] = av1_alloc_pmc(
        cm, mi_row + mi_step_h, mi_col + mi_step_w, bsize2, pc_tree,
        PARTITION_VERT_B, 2, ss_x, ss_y, &td->shared_coeff_buf);

    pc_tree->verticalb[0]->rd_mode_is_ready = 0;
    pc_tree->verticalb[1]->rd_mode_is_ready = 0;
    pc_tree->verticalb[2]->rd_mode_is_ready = 0;
    if (vert_ctx_is_ready) {
#if CONFIG_EXT_RECUR_PARTITIONS
      av1_copy_tree_context(pc_tree->verticalb[0], pc_tree->vertical[0]->none);
#else
      av1_copy_tree_context(pc_tree->verticalb[0], pc_tree->vertical[0]);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      pc_tree->verticalb[0]->mic.partition = PARTITION_VERT_B;
      pc_tree->verticalb[0]->rd_mode_is_ready = 1;
    }
#endif  // CONFIG_EXT_PARTITIONS
#if CONFIG_COLLECT_PARTITION_STATS
    {
      RD_STATS tmp_sum_rdc;
      av1_init_rd_stats(&tmp_sum_rdc);
      tmp_sum_rdc.rate = x->partition_cost[pl][PARTITION_VERT_B];
      tmp_sum_rdc.rdcost = RDCOST(x->rdmult, tmp_sum_rdc.rate, 0);
      if (!frame_is_intra_only(cm) &&
          best_rdc.rdcost - tmp_sum_rdc.rdcost >= 0) {
        partition_attempts[PARTITION_VERT_B] += 1;
        aom_usec_timer_start(&partition_timer);
        partition_timer_on = 1;
      }
    }
#endif
#if CONFIG_EXT_PARTITIONS
    found_best_partition |= rd_test_partition_abpart(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, mi_row, mi_col, bsize,
        PARTITION_VERT_B, subblocks, multi_pass_mode);
#else
    found_best_partition |= rd_test_partition3(
        cpi, td, tile_data, tp, pc_tree, &best_rdc, pc_tree->verticalb,
        ctx_none, mi_row, mi_col, bsize, PARTITION_VERT_B, mi_row, mi_col,
        subsize, mi_row, mi_col + mi_step_w, bsize2, mi_row + mi_step_h,
        mi_col + mi_step_w, bsize2);
#endif  // CONFIG_EXT_PARTITIONS
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_VERT_B] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

#if CONFIG_EXT_PARTITIONS
  // partition3_allowed is 1 if we can use a PARTITION_HORZ_3 or
  // PARTITION_VERT_3 for this block. This is almost the same as
  // ext_partition_allowed, except that we don't allow 128x32 or 32x128
  // blocks, so we require that bsize is not BLOCK_128X128.
  int partition3_allowed = cpi->oxcf.enable_1to3_partitions &&
                           ext_partition_allowed && bsize != BLOCK_128X128;

  const BLOCK_SIZE subsize_horz3 =
      get_partition_subsize(bsize, PARTITION_HORZ_3);
  int is_chroma_size_valid_horz3 = 0;
  if (subsize_horz3 < BLOCK_SIZES_ALL) {
    set_chroma_ref_info(mi_row, mi_col, 0, subsize_horz3, &tmp_chr_ref_info,
                        &pc_tree->chroma_ref_info, bsize, PARTITION_HORZ_3,
                        ss_x, ss_y);
    is_chroma_size_valid_horz3 =
        get_plane_block_size(tmp_chr_ref_info.bsize_base, ss_x, ss_y) !=
        BLOCK_INVALID;
  }

  const BLOCK_SIZE subsize_vert3 =
      get_partition_subsize(bsize, PARTITION_VERT_3);
  int is_chroma_size_valid_vert3 = 0;
  if (subsize_vert3 < BLOCK_SIZES_ALL) {
    set_chroma_ref_info(mi_row, mi_col, 0, subsize_vert3, &tmp_chr_ref_info,
                        &pc_tree->chroma_ref_info, bsize, PARTITION_VERT_3,
                        ss_x, ss_y);
    is_chroma_size_valid_vert3 =
        get_plane_block_size(tmp_chr_ref_info.bsize_base, ss_x, ss_y) !=
        BLOCK_INVALID;
  }

  int partition_horz3_allowed = partition3_allowed && partition_horz_allowed &&
                                is_chroma_size_valid_horz3;
  int partition_vert3_allowed = partition3_allowed && partition_vert_allowed &&
                                is_chroma_size_valid_vert3;
  if (cpi->sf.prune_ext_partition_types_search_level == 2) {
    partition_horz3_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
#if !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_HORZ_A ||
                                pc_tree->partitioning == PARTITION_HORZ_B ||
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
    partition_vert3_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
#if !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_VERT_A ||
                                pc_tree->partitioning == PARTITION_VERT_B ||
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
  }

  partition3_allowed &= (partition_horz3_allowed || partition_vert3_allowed);

  // TODO(urvang): Rename speed feature, and change behavior / make it work.
  // currently, it's a hack (still dividing into 4 subparts to get score).
  if (cpi->sf.ml_prune_4_partition && partition3_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_square_block(bsize) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      partition_horz_allowed && partition_vert_allowed) {
    av1_ml_prune_4_partition(cpi, x, bsize, pc_tree->partitioning,
                             best_rdc.rdcost, horz_rd, vert_rd, split_rd,
                             &partition_horz3_allowed, &partition_vert3_allowed,
                             pb_source_variance, mi_row, mi_col);
  }

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8) {
    if (block_size_high[bsize] <= 16 || block_size_wide[bsize] <= 16) {
      partition_horz3_allowed = 0;
      partition_vert3_allowed = 0;
    }
  }
#endif

  if (blksize < (min_partition_size << 2)) {
    partition_horz3_allowed = 0;
    partition_vert3_allowed = 0;
  }

  // PARTITION_HORZ_3
  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !partition_horz3_allowed));
  if (!terminate_partition_search && partition_horz3_allowed && has_rows &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_HORZ_3) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      (do_rectangular_split || active_h_edge(cpi, mi_row, mi_step_h)) &&
      !is_gt_max_sq_part) {
    av1_init_rd_stats(&sum_rdc);
    const int quarter_step = mi_size_high[bsize] / 4;
#if !CONFIG_EXT_RECUR_PARTITIONS
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

    subsize = get_partition_subsize(bsize, PARTITION_HORZ_3);

    sum_rdc.rate = partition_cost[PARTITION_HORZ_3];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);

#if CONFIG_COLLECT_PARTITION_STATS
    if (best_rdc.rdcost - sum_rdc.rdcost >= 0) {
      partition_attempts[PARTITION_HORZ_3] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
    const BLOCK_SIZE bsize3 = get_partition_subsize(bsize, PARTITION_HORZ);
    const int step_multipliers[3] = { 0, 1, 2 };
    const BLOCK_SIZE subblock_sizes[3] = { subsize, bsize3, subsize };

#if CONFIG_EXT_RECUR_PARTITIONS
    pc_tree->horizontal3[0] =
        av1_alloc_pc_tree_node(mi_row, mi_col, subblock_sizes[0], pc_tree,
                               PARTITION_HORZ_3, 0, 0, ss_x, ss_y);
    pc_tree->horizontal3[1] =
        av1_alloc_pc_tree_node(mi_row + quarter_step, mi_col, subblock_sizes[1],
                               pc_tree, PARTITION_HORZ_3, 1, 0, ss_x, ss_y);
    pc_tree->horizontal3[2] = av1_alloc_pc_tree_node(
        mi_row + quarter_step * 3, mi_col, subblock_sizes[2], pc_tree,
        PARTITION_HORZ_3, 2, 1, ss_x, ss_y);
#else
    pc_tree->horizontal3[0] =
        av1_alloc_pmc(cm, mi_row, mi_col, subsize, pc_tree, PARTITION_HORZ_3, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horizontal3[1] =
        av1_alloc_pmc(cm, mi_row + quarter_step, mi_col, bsize3, pc_tree,
                      PARTITION_HORZ_3, 1, ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->horizontal3[2] =
        av1_alloc_pmc(cm, mi_row + quarter_step * 3, mi_col, subsize, pc_tree,
                      PARTITION_HORZ_3, 2, ss_x, ss_y, &td->shared_coeff_buf);
#endif  // CONFIG_EXT_RECUR_PARTITIONS

    int this_mi_row = mi_row;
    for (int i = 0; i < 3; ++i) {
      this_mi_row += quarter_step * step_multipliers[i];

      if (i > 0 && this_mi_row >= cm->mi_rows) break;

#if CONFIG_EXT_RECUR_PARTITIONS
      SUBBLOCK_RDO_DATA rdo_data = { NULL,
                                     pc_tree->horizontal3[i],
                                     NULL,
                                     this_mi_row,
                                     mi_col,
                                     subblock_sizes[i],
                                     PARTITION_HORZ_3,
                                     i == 2,
                                     1,
                                     max_sq_part,
                                     min_sq_part };

      if (!rd_try_subblock_new(cpi, td, tile_data, tp, &rdo_data, best_rdc,
                               &sum_rdc, multi_pass_mode)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }
#else
      PICK_MODE_CONTEXT *ctx_this = pc_tree->horizontal3[i];

      ctx_this->rd_mode_is_ready = 0;
      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 2), this_mi_row,
                           mi_col, subblock_sizes[i], best_rdc, &sum_rdc,
                           PARTITION_HORZ_3, ctx_prev, ctx_this)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }
      ctx_prev = ctx_this;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    }

    av1_rd_cost_update(x->rdmult, &sum_rdc);
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      best_rdc = sum_rdc;
      found_best_partition = true;
      pc_tree->partitioning = PARTITION_HORZ_3;
    }

#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_HORZ_3] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT_3
  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !partition_vert3_allowed));
  if (!terminate_partition_search && partition_vert3_allowed && has_cols &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_VERT_3) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      (do_rectangular_split || active_v_edge(cpi, mi_row, mi_step_h)) &&
      !is_gt_max_sq_part) {
    av1_init_rd_stats(&sum_rdc);
    const int quarter_step = mi_size_wide[bsize] / 4;
#if !CONFIG_EXT_RECUR_PARTITIONS
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

    subsize = get_partition_subsize(bsize, PARTITION_VERT_3);
    sum_rdc.rate = partition_cost[PARTITION_VERT_3];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);

#if CONFIG_COLLECT_PARTITION_STATS
    if (best_rdc.rdcost - sum_rdc.rdcost >= 0) {
      partition_attempts[PARTITION_VERT_3] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif

    const BLOCK_SIZE bsize3 = get_partition_subsize(bsize, PARTITION_VERT);
    const int step_multipliers[3] = { 0, 1, 2 };
    const BLOCK_SIZE subblock_sizes[3] = { subsize, bsize3, subsize };

#if CONFIG_EXT_RECUR_PARTITIONS
    pc_tree->vertical3[0] =
        av1_alloc_pc_tree_node(mi_row, mi_col, subblock_sizes[0], pc_tree,
                               PARTITION_VERT_3, 0, 0, ss_x, ss_y);
    pc_tree->vertical3[1] =
        av1_alloc_pc_tree_node(mi_row, mi_col + quarter_step, subblock_sizes[1],
                               pc_tree, PARTITION_VERT_3, 1, 0, ss_x, ss_y);
    pc_tree->vertical3[2] = av1_alloc_pc_tree_node(
        mi_row, mi_col + quarter_step * 3, subblock_sizes[2], pc_tree,
        PARTITION_VERT_3, 2, 1, ss_x, ss_y);
#else
    pc_tree->vertical3[0] =
        av1_alloc_pmc(cm, mi_row, mi_col, subsize, pc_tree, PARTITION_VERT_3, 0,
                      ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->vertical3[1] =
        av1_alloc_pmc(cm, mi_row, mi_col + quarter_step, bsize3, pc_tree,
                      PARTITION_VERT_3, 1, ss_x, ss_y, &td->shared_coeff_buf);
    pc_tree->vertical3[2] =
        av1_alloc_pmc(cm, mi_row, mi_col + quarter_step * 3, subsize, pc_tree,
                      PARTITION_VERT_3, 2, ss_x, ss_y, &td->shared_coeff_buf);
#endif  // CONFIG_EXT_RECUR_PARTITIONS

    int this_mi_col = mi_col;
    for (int i = 0; i < 3; ++i) {
      this_mi_col += quarter_step * step_multipliers[i];

      if (i > 0 && this_mi_col >= cm->mi_cols) break;

#if CONFIG_EXT_RECUR_PARTITIONS
      SUBBLOCK_RDO_DATA rdo_data = { NULL,
                                     pc_tree->vertical3[i],
                                     NULL,
                                     mi_row,
                                     this_mi_col,
                                     subblock_sizes[i],
                                     PARTITION_VERT_3,
                                     i == 2,
                                     1,
                                     max_sq_part,
                                     min_sq_part };

      if (!rd_try_subblock_new(cpi, td, tile_data, tp, &rdo_data, best_rdc,
                               &sum_rdc, multi_pass_mode)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }
#else
      PICK_MODE_CONTEXT *ctx_this = pc_tree->vertical3[i];

      ctx_this->rd_mode_is_ready = 0;
      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 2), mi_row,
                           this_mi_col, subblock_sizes[i], best_rdc, &sum_rdc,
                           PARTITION_VERT_3, ctx_prev, ctx_this)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }

      ctx_prev = ctx_this;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    }

    av1_rd_cost_update(x->rdmult, &sum_rdc);
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      best_rdc = sum_rdc;
      found_best_partition = true;
      pc_tree->partitioning = PARTITION_VERT_3;
    }
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_VERT_3] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
#else
  // partition4_allowed is 1 if we can use a PARTITION_HORZ_4 or
  // PARTITION_VERT_4 for this block. This is almost the same as
  // ext_partition_allowed, except that we don't allow 128x32 or 32x128
  // blocks, so we require that bsize is not BLOCK_128X128.
  const int partition4_allowed = cpi->oxcf.enable_1to4_partitions &&
                                 ext_partition_allowed &&
                                 bsize != BLOCK_128X128;

  const BLOCK_SIZE subsize_horz4 =
      get_partition_subsize(bsize, PARTITION_HORZ_4);
  int is_chroma_size_valid_horz4 = 0;
  if (subsize_horz4 < BLOCK_SIZES_ALL) {
    set_chroma_ref_info(mi_row, mi_col, 0, subsize_horz4, &tmp_chr_ref_info,
                        &pc_tree->chroma_ref_info, bsize, PARTITION_HORZ_4,
                        ss_x, ss_y);
    is_chroma_size_valid_horz4 =
        get_plane_block_size(tmp_chr_ref_info.bsize_base, ss_x, ss_y) !=
        BLOCK_INVALID;
  }

  const BLOCK_SIZE subsize_vert4 =
      get_partition_subsize(bsize, PARTITION_VERT_4);
  int is_chroma_size_valid_vert4 = 0;
  if (subsize_vert4 < BLOCK_SIZES_ALL) {
    set_chroma_ref_info(mi_row, mi_col, 0, subsize_vert4, &tmp_chr_ref_info,
                        &pc_tree->chroma_ref_info, bsize, PARTITION_VERT_4,
                        ss_x, ss_y);
    is_chroma_size_valid_vert4 =
        get_plane_block_size(tmp_chr_ref_info.bsize_base, ss_x, ss_y) !=
        BLOCK_INVALID;
  }

  int partition_horz4_allowed = partition4_allowed && partition_horz_allowed &&
                                is_chroma_size_valid_horz4;
  int partition_vert4_allowed = partition4_allowed && partition_vert_allowed &&
                                is_chroma_size_valid_vert4;
  if (cpi->sf.prune_ext_partition_types_search_level == 2) {
    partition_horz4_allowed &= (pc_tree->partitioning == PARTITION_HORZ ||
#if !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_HORZ_A ||
                                pc_tree->partitioning == PARTITION_HORZ_B ||
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
    partition_vert4_allowed &= (pc_tree->partitioning == PARTITION_VERT ||
#if !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_VERT_A ||
                                pc_tree->partitioning == PARTITION_VERT_B ||
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
                                pc_tree->partitioning == PARTITION_SPLIT ||
                                pc_tree->partitioning == PARTITION_NONE);
  }
  if (cpi->sf.ml_prune_4_partition && partition4_allowed &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_square_block(bsize) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      partition_horz_allowed && partition_vert_allowed) {
    av1_ml_prune_4_partition(cpi, x, bsize, pc_tree->partitioning,
                             best_rdc.rdcost, horz_rd, vert_rd, split_rd,
                             &partition_horz4_allowed, &partition_vert4_allowed,
                             pb_source_variance, mi_row, mi_col);
  }

#if CONFIG_DIST_8X8
  if (x->using_dist_8x8) {
    if (block_size_high[bsize] <= 16 || block_size_wide[bsize] <= 16) {
      partition_horz4_allowed = 0;
      partition_vert4_allowed = 0;
    }
  }
#endif

  if (blksize < (min_partition_size << 2)) {
    partition_horz4_allowed = 0;
    partition_vert4_allowed = 0;
  }

  // PARTITION_HORZ_4
  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !partition_horz4_allowed));
  if (!terminate_partition_search && partition_horz4_allowed && has_rows &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_HORZ_4) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      (do_rectangular_split || active_h_edge(cpi, mi_row, mi_step_h)) &&
      !is_gt_max_sq_part) {
    av1_init_rd_stats(&sum_rdc);
    const int quarter_step = mi_size_high[bsize] / 4;
#if !CONFIG_EXT_RECUR_PARTITIONS
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

    subsize = get_partition_subsize(bsize, PARTITION_HORZ_4);
    sum_rdc.rate = partition_cost[PARTITION_HORZ_4];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);

    for (int i = 0; i < 4; ++i) {
#if CONFIG_EXT_RECUR_PARTITIONS
      pc_tree->horizontal4[i] = av1_alloc_pc_tree_node(
          mi_row + i * quarter_step, mi_col, subsize, pc_tree, PARTITION_HORZ_4,
          i, i == 3, ss_x, ss_y);
#else
      pc_tree->horizontal4[i] =
          av1_alloc_pmc(cm, mi_row + i * quarter_step, mi_col, subsize, pc_tree,
                        PARTITION_HORZ_4, i, ss_x, ss_y, &td->shared_coeff_buf);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    }

#if CONFIG_COLLECT_PARTITION_STATS
    if (best_rdc.rdcost - sum_rdc.rdcost >= 0) {
      partition_attempts[PARTITION_HORZ_4] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
    for (int i = 0; i < 4; ++i) {
      const int this_mi_row = mi_row + i * quarter_step;

      if (i > 0 && this_mi_row >= cm->mi_rows) break;

#if CONFIG_EXT_RECUR_PARTITIONS
      SUBBLOCK_RDO_DATA rdo_data = {
        NULL,       pc_tree->horizontal4[i], NULL,   this_mi_row, mi_col,
        subsize,    PARTITION_HORZ_4,        i == 3, 1,           max_sq_part,
        min_sq_part
      };

      if (!rd_try_subblock_new(cpi, td, tile_data, tp, &rdo_data, best_rdc,
                               &sum_rdc, multi_pass_mode)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }
#else
      PICK_MODE_CONTEXT *ctx_this = pc_tree->horizontal4[i];

      ctx_this->rd_mode_is_ready = 0;
      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 3), this_mi_row,
                           mi_col, subsize, best_rdc, &sum_rdc,
                           PARTITION_HORZ_4, ctx_prev, ctx_this)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }

      ctx_prev = ctx_this;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    }

    av1_rd_cost_update(x->rdmult, &sum_rdc);
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      best_rdc = sum_rdc;
      found_best_partition = true;
      pc_tree->partitioning = PARTITION_HORZ_4;
    }

#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_HORZ_4] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }

  // PARTITION_VERT_4
  assert(IMPLIES(!cpi->oxcf.enable_rect_partitions, !partition_vert4_allowed));
  if (!terminate_partition_search && partition_vert4_allowed && has_cols &&
#if CONFIG_EXT_RECUR_PARTITIONS
      is_partition_valid(bsize, PARTITION_VERT_4) &&
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      (do_rectangular_split || active_v_edge(cpi, mi_row, mi_step_h)) &&
      !is_gt_max_sq_part) {
    av1_init_rd_stats(&sum_rdc);
    const int quarter_step = mi_size_wide[bsize] / 4;
#if !CONFIG_EXT_RECUR_PARTITIONS
    PICK_MODE_CONTEXT *ctx_prev = ctx_none;
#endif  // !CONFIG_EXT_RECUR_PARTITIONS

    subsize = get_partition_subsize(bsize, PARTITION_VERT_4);
    sum_rdc.rate = partition_cost[PARTITION_VERT_4];
    sum_rdc.rdcost = RDCOST(x->rdmult, sum_rdc.rate, 0);

    for (int i = 0; i < 4; ++i) {
#if CONFIG_EXT_RECUR_PARTITIONS
      pc_tree->vertical4[i] = av1_alloc_pc_tree_node(
          mi_row, mi_col + i * quarter_step, subsize, pc_tree, PARTITION_VERT_4,
          i, i == 3, ss_x, ss_y);
#else
      pc_tree->vertical4[i] =
          av1_alloc_pmc(cm, mi_row, mi_col + i * quarter_step, subsize, pc_tree,
                        PARTITION_VERT_4, i, ss_x, ss_y, &td->shared_coeff_buf);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    }

#if CONFIG_COLLECT_PARTITION_STATS
    if (best_rdc.rdcost - sum_rdc.rdcost >= 0) {
      partition_attempts[PARTITION_VERT_4] += 1;
      aom_usec_timer_start(&partition_timer);
      partition_timer_on = 1;
    }
#endif
    for (int i = 0; i < 4; ++i) {
      const int this_mi_col = mi_col + i * quarter_step;

      if (i > 0 && this_mi_col >= cm->mi_cols) break;

#if CONFIG_EXT_RECUR_PARTITIONS
      SUBBLOCK_RDO_DATA rdo_data = {
        NULL,       pc_tree->vertical4[i], NULL,   mi_row, this_mi_col,
        subsize,    PARTITION_VERT_4,      i == 3, 1,      max_sq_part,
        min_sq_part
      };

      if (!rd_try_subblock_new(cpi, td, tile_data, tp, &rdo_data, best_rdc,
                               &sum_rdc, multi_pass_mode)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }
#else
      PICK_MODE_CONTEXT *ctx_this = pc_tree->vertical4[i];

      ctx_this->rd_mode_is_ready = 0;
      if (!rd_try_subblock(cpi, td, tile_data, tp, (i == 3), mi_row,
                           this_mi_col, subsize, best_rdc, &sum_rdc,
                           PARTITION_VERT_4, ctx_prev, ctx_this)) {
        av1_invalid_rd_stats(&sum_rdc);
        break;
      }

      ctx_prev = ctx_this;
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    }

    av1_rd_cost_update(x->rdmult, &sum_rdc);
    if (sum_rdc.rdcost < best_rdc.rdcost) {
      best_rdc = sum_rdc;
      found_best_partition = true;
      pc_tree->partitioning = PARTITION_VERT_4;
    }
#if CONFIG_COLLECT_PARTITION_STATS
    if (partition_timer_on) {
      aom_usec_timer_mark(&partition_timer);
      int64_t time = aom_usec_timer_elapsed(&partition_timer);
      partition_times[PARTITION_VERT_4] += time;
      partition_timer_on = 0;
    }
#endif
    restore_context(cm, x, &x_ctx, mi_row, mi_col, bsize, num_planes);
  }
#endif  // CONFIG_EXT_PARTITIONS

#if CONFIG_EXT_RECUR_PARTITIONS
  if (sms_tree)
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    sms_tree->partitioning = pc_tree->partitioning;
  if (bsize == cm->seq_params.sb_size && !found_best_partition) {
    // Did not find a valid partition, go back and search again, with less
    // constraint on which partition types to search.
    x->must_find_valid_partition = 1;
#if CONFIG_COLLECT_PARTITION_STATS == 2
    part_stats->partition_redo += 1;
#endif
    goto BEGIN_PARTITION_SEARCH;
  }

  *rd_cost = best_rdc;
  pc_tree->rd_cost = best_rdc;
  if (!found_best_partition) av1_invalid_rd_stats(&pc_tree->rd_cost);

#if CONFIG_COLLECT_PARTITION_STATS
  if (best_rdc.rate < INT_MAX && best_rdc.dist < INT64_MAX) {
    partition_decisions[pc_tree->partitioning] += 1;
  }
#endif

#if CONFIG_COLLECT_PARTITION_STATS == 1
  // If CONFIG_COLLECT_PARTITION_STATS is 1, then print out the stats for each
  // prediction block
  FILE *f = fopen("data.csv", "a");
  fprintf(f, "%d,%d,%d,", bsize, cm->show_frame, frame_is_intra_only(cm));
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    fprintf(f, "%d,", partition_decisions[idx]);
  }
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    fprintf(f, "%d,", partition_attempts[idx]);
  }
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    fprintf(f, "%ld,", partition_times[idx]);
  }
  fprintf(f, "\n");
  fclose(f);
#endif

#if CONFIG_COLLECT_PARTITION_STATS == 2
  // If CONFIG_COLLECTION_PARTITION_STATS is 2, then we print out the stats for
  // the whole clip. So we need to pass the information upstream to the encoder
  const int bsize_idx = av1_get_bsize_idx_for_part_stats(bsize);
  int *agg_attempts = part_stats->partition_attempts[bsize_idx];
  int *agg_decisions = part_stats->partition_decisions[bsize_idx];
  int64_t *agg_times = part_stats->partition_times[bsize_idx];
  for (int idx = 0; idx < EXT_PARTITION_TYPES; idx++) {
    agg_attempts[idx] += partition_attempts[idx];
    agg_decisions[idx] += partition_decisions[idx];
    agg_times[idx] += partition_times[idx];
  }
#endif

#if CONFIG_EXT_RECUR_PARTITIONS
  if (sms_tree)
#endif  // CONFIG_EXT_RECUR_PARTITIONS
    sms_tree->partitioning = pc_tree->partitioning;
  int pc_tree_dealloc = 0;
  if (found_best_partition) {
    if (bsize == cm->seq_params.sb_size) {
      const int emit_output = multi_pass_mode != SB_DRY_PASS;
      const RUN_TYPE run_type = emit_output ? OUTPUT_ENABLED : DRY_RUN_NORMAL;

      x->cb_offset = 0;

      av1_reset_ptree_in_sbi(xd->sbi);
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, run_type, bsize,
                pc_tree, xd->sbi->ptree_root, NULL);
      av1_free_pc_tree_recursive(pc_tree, num_planes, 0, 0);
      pc_tree_dealloc = 1;
    } else if (!pc_tree->is_last_subblock) {
      encode_sb(cpi, td, tile_data, tp, mi_row, mi_col, DRY_RUN_NORMAL, bsize,
                pc_tree, NULL, NULL);
    }
  }

  if (!pc_tree_dealloc) {
    av1_free_pc_tree_recursive(pc_tree, num_planes, 1, 1);
  }

  if (bsize == cm->seq_params.sb_size) {
    assert(best_rdc.rate < INT_MAX);
    assert(best_rdc.dist < INT64_MAX);
  } else {
    assert(tp_orig == *tp);
  }

  x->rdmult = orig_rdmult;
  return found_best_partition;
}
#endif  // !CONFIG_REALTIME_ONLY
#undef NUM_SIMPLE_MOTION_FEATURES

#if !CONFIG_REALTIME_ONLY
static int get_rdmult_delta(AV1_COMP *cpi, BLOCK_SIZE bsize, int analysis_type,
                            int mi_row, int mi_col, int orig_rdmult) {
  AV1_COMMON *const cm = &cpi->common;
  assert(IMPLIES(cpi->gf_group.size > 0,
                 cpi->gf_group.index < cpi->gf_group.size));
  const int tpl_idx = cpi->gf_group.index;
  TplDepFrame *tpl_frame = &cpi->tpl_frame[tpl_idx];
  TplDepStats *tpl_stats = tpl_frame->tpl_stats_ptr;
  int tpl_stride = tpl_frame->stride;
  int64_t intra_cost = 0;
  int64_t mc_dep_cost = 0;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  if (tpl_frame->is_valid == 0) return orig_rdmult;

  if (!is_frame_tpl_eligible(cpi)) return orig_rdmult;

  if (cpi->gf_group.index >= MAX_LAG_BUFFERS) return orig_rdmult;

  int64_t mc_count = 0, mc_saved = 0;
  int mi_count = 0;
  const int mi_col_sr =
      av1_coded_to_superres_mi(mi_col, cm->superres_scale_denominator);
  const int mi_col_end_sr = av1_coded_to_superres_mi(
      mi_col + mi_wide, cm->superres_scale_denominator);
  const int mi_cols_sr = av1_pixels_to_mi(cm->superres_upscaled_width);
  const int step = 1 << cpi->tpl_stats_block_mis_log2;
  for (int row = mi_row; row < mi_row + mi_high; row += step) {
    for (int col = mi_col_sr; col < mi_col_end_sr; col += step) {
      if (row >= cm->mi_rows || col >= mi_cols_sr) continue;
      TplDepStats *this_stats =
          &tpl_stats[av1_tpl_ptr_pos(cpi, row, col, tpl_stride)];
      int64_t mc_dep_delta =
          RDCOST(tpl_frame->base_rdmult, this_stats->mc_dep_rate,
                 this_stats->mc_dep_dist);
      intra_cost += this_stats->recrf_dist << RDDIV_BITS;
      mc_dep_cost += (this_stats->recrf_dist << RDDIV_BITS) + mc_dep_delta;
      mc_count += this_stats->mc_count;
      mc_saved += this_stats->mc_saved;
      mi_count++;
    }
  }

  aom_clear_system_state();

  double beta = 1.0;
  if (analysis_type == 0) {
    if (mc_dep_cost > 0 && intra_cost > 0) {
      const double r0 = cpi->rd.r0;
      const double rk = (double)intra_cost / mc_dep_cost;
      beta = (r0 / rk);
    }
  } else if (analysis_type == 1) {
    const double mc_count_base = (mi_count * cpi->rd.mc_count_base);
    beta = (mc_count + 1.0) / (mc_count_base + 1.0);
    beta = pow(beta, 0.5);
  } else if (analysis_type == 2) {
    const double mc_saved_base = (mi_count * cpi->rd.mc_saved_base);
    beta = (mc_saved + 1.0) / (mc_saved_base + 1.0);
    beta = pow(beta, 0.5);
  }

  int rdmult = av1_get_adaptive_rdmult(cpi, beta);

  aom_clear_system_state();

  rdmult = AOMMIN(rdmult, orig_rdmult * 3 / 2);
  rdmult = AOMMAX(rdmult, orig_rdmult * 1 / 2);

  rdmult = AOMMAX(1, rdmult);

  return rdmult;
}
#endif  // !CONFIG_REALTIME_ONLY

// analysis_type 0: Use mc_dep_cost and intra_cost
// analysis_type 1: Use count of best inter predictor chosen
// analysis_type 2: Use cost reduction from intra to inter for best inter
//                  predictor chosen
static int get_q_for_deltaq_objective(AV1_COMP *const cpi, BLOCK_SIZE bsize,
                                      int analysis_type, int mi_row,
                                      int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  assert(IMPLIES(cpi->gf_group.size > 0,
                 cpi->gf_group.index < cpi->gf_group.size));
  const int tpl_idx = cpi->gf_group.index;
  TplDepFrame *tpl_frame = &cpi->tpl_frame[tpl_idx];
  TplDepStats *tpl_stats = tpl_frame->tpl_stats_ptr;
  int tpl_stride = tpl_frame->stride;
  int64_t intra_cost = 0;
  int64_t mc_dep_cost = 0;
  const int mi_wide = mi_size_wide[bsize];
  const int mi_high = mi_size_high[bsize];

  if (cpi->tpl_model_pass == 1) {
    assert(cpi->oxcf.enable_tpl_model == 2);
    return cm->base_qindex;
  }

  if (tpl_frame->is_valid == 0) return cm->base_qindex;

  if (!is_frame_tpl_eligible(cpi)) return cm->base_qindex;

  if (cpi->gf_group.index >= MAX_LAG_BUFFERS) return cm->base_qindex;

  int64_t mc_count = 0, mc_saved = 0;
  int mi_count = 0;
  const int mi_col_sr =
      av1_coded_to_superres_mi(mi_col, cm->superres_scale_denominator);
  const int mi_col_end_sr = av1_coded_to_superres_mi(
      mi_col + mi_wide, cm->superres_scale_denominator);
  const int mi_cols_sr = av1_pixels_to_mi(cm->superres_upscaled_width);
  const int step = 1 << cpi->tpl_stats_block_mis_log2;
  for (int row = mi_row; row < mi_row + mi_high; row += step) {
    for (int col = mi_col_sr; col < mi_col_end_sr; col += step) {
      if (row >= cm->mi_rows || col >= mi_cols_sr) continue;
      TplDepStats *this_stats =
          &tpl_stats[av1_tpl_ptr_pos(cpi, row, col, tpl_stride)];
      int64_t mc_dep_delta =
          RDCOST(tpl_frame->base_rdmult, this_stats->mc_dep_rate,
                 this_stats->mc_dep_dist);
      intra_cost += this_stats->recrf_dist << RDDIV_BITS;
      mc_dep_cost += (this_stats->recrf_dist << RDDIV_BITS) + mc_dep_delta;
      mc_count += this_stats->mc_count;
      mc_saved += this_stats->mc_saved;
      mi_count++;
    }
  }

  aom_clear_system_state();

  int offset = 0;
  double beta = 1.0;
  if (analysis_type == 0) {
    if (mc_dep_cost > 0 && intra_cost > 0) {
      const double r0 = cpi->rd.r0;
      const double rk = (double)intra_cost / mc_dep_cost;
      beta = (r0 / rk);
      assert(beta > 0.0);
    }
  } else if (analysis_type == 1) {
    const double mc_count_base = (mi_count * cpi->rd.mc_count_base);
    beta = (mc_count + 1.0) / (mc_count_base + 1.0);
    beta = pow(beta, 0.5);
  } else if (analysis_type == 2) {
    const double mc_saved_base = (mi_count * cpi->rd.mc_saved_base);
    beta = (mc_saved + 1.0) / (mc_saved_base + 1.0);
    beta = pow(beta, 0.5);
  }
  offset = (7 * av1_get_deltaq_offset(cpi, cm->base_qindex, beta)) / 8;
  // printf("[%d/%d]: beta %g offset %d\n", pyr_lev_from_top,
  //        cpi->gf_group.pyramid_height, beta, offset);

  aom_clear_system_state();

  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  offset = AOMMIN(offset, delta_q_info->delta_q_res * 9 - 1);
  offset = AOMMAX(offset, -delta_q_info->delta_q_res * 9 + 1);
  int qindex = cm->base_qindex + offset;
  qindex = AOMMIN(qindex, MAXQ);
  qindex = AOMMAX(qindex, MINQ);

  return qindex;
}

static void setup_delta_q(AV1_COMP *const cpi, ThreadData *td,
                          MACROBLOCK *const x, const TileInfo *const tile_info,
                          int mi_row, int mi_col, int num_planes) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const DeltaQInfo *const delta_q_info = &cm->delta_q_info;
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;
  const int mib_size = cm->seq_params.mib_size;

  // Delta-q modulation based on variance
  av1_setup_src_planes(x, cpi->source, mi_row, mi_col, num_planes, NULL);

  int current_qindex = cm->base_qindex;
  if (cm->delta_q_info.delta_q_present_flag) {
    if (cpi->oxcf.deltaq_mode == DELTA_Q_PERCEPTUAL) {
      if (DELTA_Q_PERCEPTUAL_MODULATION == 1) {
        const int block_wavelet_energy_level =
            av1_block_wavelet_energy_level(cpi, x, sb_size);
        x->sb_energy_level = block_wavelet_energy_level;
        current_qindex = av1_compute_q_from_energy_level_deltaq_mode(
            cpi, block_wavelet_energy_level);
      } else {
        const int block_var_level = av1_log_block_var(cpi, x, sb_size);
        x->sb_energy_level = block_var_level;
        current_qindex =
            av1_compute_q_from_energy_level_deltaq_mode(cpi, block_var_level);
      }
    } else if (cpi->oxcf.deltaq_mode == DELTA_Q_OBJECTIVE) {
      assert(cpi->oxcf.enable_tpl_model);
      // Setup deltaq based on tpl stats
      current_qindex =
          get_q_for_deltaq_objective(cpi, sb_size, 0, mi_row, mi_col);
    }
  }

  const int qmask = ~(delta_q_info->delta_q_res - 1);
  current_qindex = clamp(current_qindex, delta_q_info->delta_q_res,
                         256 - delta_q_info->delta_q_res);

  const int sign_deltaq_index =
      current_qindex - xd->current_qindex >= 0 ? 1 : -1;

  const int deltaq_deadzone = delta_q_info->delta_q_res / 4;
  int abs_deltaq_index = abs(current_qindex - xd->current_qindex);
  abs_deltaq_index = (abs_deltaq_index + deltaq_deadzone) & qmask;
  current_qindex = xd->current_qindex + sign_deltaq_index * abs_deltaq_index;
  current_qindex = AOMMAX(current_qindex, MINQ + 1);
  assert(current_qindex > 0);

  xd->delta_qindex = current_qindex - cm->base_qindex;

  CHROMA_REF_INFO sb_chr_ref_info = { 1, 0, mi_row, mi_col, sb_size, sb_size };
  set_offsets(cpi, tile_info, x, mi_row, mi_col, sb_size, &sb_chr_ref_info);
  xd->mi[0]->current_qindex = current_qindex;
  av1_init_plane_quantizers(cpi, x, xd->mi[0]->segment_id);

  // keep track of any non-zero delta-q used
  td->deltaq_used |= (xd->delta_qindex != 0);

  if (cm->delta_q_info.delta_q_present_flag && cpi->oxcf.deltalf_mode) {
    const int lfmask = ~(delta_q_info->delta_lf_res - 1);
    const int delta_lf_from_base =
        ((xd->delta_qindex / 2 + delta_q_info->delta_lf_res / 2) & lfmask);

    // pre-set the delta lf for loop filter. Note that this value is set
    // before mi is assigned for each block in current superblock
    for (int j = 0; j < AOMMIN(mib_size, cm->mi_rows - mi_row); j++) {
      for (int k = 0; k < AOMMIN(mib_size, cm->mi_cols - mi_col); k++) {
        cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)].delta_lf_from_base =
            (int8_t)clamp(delta_lf_from_base, -MAX_LOOP_FILTER,
                          MAX_LOOP_FILTER);
        const int frame_lf_count =
            av1_num_planes(cm) > 1 ? FRAME_LF_COUNT : FRAME_LF_COUNT - 2;
        for (int lf_id = 0; lf_id < frame_lf_count; ++lf_id) {
          cm->mi[(mi_row + j) * cm->mi_stride + (mi_col + k)].delta_lf[lf_id] =
              (int8_t)clamp(delta_lf_from_base, -MAX_LOOP_FILTER,
                            MAX_LOOP_FILTER);
        }
      }
    }
  }
}

#define AVG_CDF_WEIGHT_LEFT 3
#define AVG_CDF_WEIGHT_TOP_RIGHT 1

static void avg_cdf_symbol(aom_cdf_prob *cdf_ptr_left, aom_cdf_prob *cdf_ptr_tr,
                           int num_cdfs, int cdf_stride, int nsymbs,
                           int wt_left, int wt_tr) {
  for (int i = 0; i < num_cdfs; i++) {
    for (int j = 0; j <= nsymbs; j++) {
      cdf_ptr_left[i * cdf_stride + j] =
          (aom_cdf_prob)(((int)cdf_ptr_left[i * cdf_stride + j] * wt_left +
                          (int)cdf_ptr_tr[i * cdf_stride + j] * wt_tr +
                          ((wt_left + wt_tr) / 2)) /
                         (wt_left + wt_tr));
      assert(cdf_ptr_left[i * cdf_stride + j] >= 0 &&
             cdf_ptr_left[i * cdf_stride + j] < CDF_PROB_TOP);
    }
  }
}

#define AVERAGE_CDF(cname_left, cname_tr, nsymbs) \
  AVG_CDF_STRIDE(cname_left, cname_tr, nsymbs, CDF_SIZE(nsymbs))

#define AVG_CDF_STRIDE(cname_left, cname_tr, nsymbs, cdf_stride)           \
  do {                                                                     \
    aom_cdf_prob *cdf_ptr_left = (aom_cdf_prob *)cname_left;               \
    aom_cdf_prob *cdf_ptr_tr = (aom_cdf_prob *)cname_tr;                   \
    int array_size = (int)sizeof(cname_left) / sizeof(aom_cdf_prob);       \
    int num_cdfs = array_size / cdf_stride;                                \
    avg_cdf_symbol(cdf_ptr_left, cdf_ptr_tr, num_cdfs, cdf_stride, nsymbs, \
                   wt_left, wt_tr);                                        \
  } while (0)

static void avg_nmv(nmv_context *nmv_left, nmv_context *nmv_tr, int wt_left,
                    int wt_tr) {
  AVERAGE_CDF(nmv_left->joints_cdf, nmv_tr->joints_cdf, 4);
  for (int i = 0; i < 2; i++) {
    AVERAGE_CDF(nmv_left->comps[i].classes_cdf, nmv_tr->comps[i].classes_cdf,
                MV_CLASSES);
#if CONFIG_FLEX_MVRES
    AVERAGE_CDF(nmv_left->comps[i].class0_fp_cdf,
                nmv_tr->comps[i].class0_fp_cdf, 2);
    AVERAGE_CDF(nmv_left->comps[i].fp_cdf, nmv_tr->comps[i].fp_cdf, 2);
#else
    AVERAGE_CDF(nmv_left->comps[i].class0_fp_cdf,
                nmv_tr->comps[i].class0_fp_cdf, MV_FP_SIZE);
    AVERAGE_CDF(nmv_left->comps[i].fp_cdf, nmv_tr->comps[i].fp_cdf, MV_FP_SIZE);
#endif  // CONFIG_FLEX_MVRES
    AVERAGE_CDF(nmv_left->comps[i].sign_cdf, nmv_tr->comps[i].sign_cdf, 2);
    AVERAGE_CDF(nmv_left->comps[i].class0_hp_cdf,
                nmv_tr->comps[i].class0_hp_cdf, 2);
    AVERAGE_CDF(nmv_left->comps[i].hp_cdf, nmv_tr->comps[i].hp_cdf, 2);
    AVERAGE_CDF(nmv_left->comps[i].class0_cdf, nmv_tr->comps[i].class0_cdf,
                CLASS0_SIZE);
    AVERAGE_CDF(nmv_left->comps[i].bits_cdf, nmv_tr->comps[i].bits_cdf, 2);
  }
}

// In case of row-based multi-threading of encoder, since we always
// keep a top - right sync, we can average the top - right SB's CDFs and
// the left SB's CDFs and use the same for current SB's encoding to
// improve the performance. This function facilitates the averaging
// of CDF and used only when row-mt is enabled in encoder.
static void avg_cdf_symbols(FRAME_CONTEXT *ctx_left, FRAME_CONTEXT *ctx_tr,
                            int wt_left, int wt_tr) {
  AVERAGE_CDF(ctx_left->txb_skip_cdf, ctx_tr->txb_skip_cdf, 2);
  AVERAGE_CDF(ctx_left->eob_extra_cdf, ctx_tr->eob_extra_cdf, 2);
  AVERAGE_CDF(ctx_left->dc_sign_cdf, ctx_tr->dc_sign_cdf, 2);
  AVERAGE_CDF(ctx_left->eob_flag_cdf16, ctx_tr->eob_flag_cdf16, 5);
  AVERAGE_CDF(ctx_left->eob_flag_cdf32, ctx_tr->eob_flag_cdf32, 6);
  AVERAGE_CDF(ctx_left->eob_flag_cdf64, ctx_tr->eob_flag_cdf64, 7);
  AVERAGE_CDF(ctx_left->eob_flag_cdf128, ctx_tr->eob_flag_cdf128, 8);
  AVERAGE_CDF(ctx_left->eob_flag_cdf256, ctx_tr->eob_flag_cdf256, 9);
  AVERAGE_CDF(ctx_left->eob_flag_cdf512, ctx_tr->eob_flag_cdf512, 10);
  AVERAGE_CDF(ctx_left->eob_flag_cdf1024, ctx_tr->eob_flag_cdf1024, 11);
  AVERAGE_CDF(ctx_left->coeff_base_eob_cdf, ctx_tr->coeff_base_eob_cdf, 3);
  AVERAGE_CDF(ctx_left->coeff_base_cdf, ctx_tr->coeff_base_cdf, 4);
  AVERAGE_CDF(ctx_left->coeff_br_cdf, ctx_tr->coeff_br_cdf, BR_CDF_SIZE);
  AVERAGE_CDF(ctx_left->newmv_cdf, ctx_tr->newmv_cdf, 2);
  AVERAGE_CDF(ctx_left->zeromv_cdf, ctx_tr->zeromv_cdf, 2);
#if CONFIG_NEW_INTER_MODES
  AVERAGE_CDF(ctx_left->drl0_cdf, ctx_tr->drl0_cdf, 2);
  AVERAGE_CDF(ctx_left->drl1_cdf, ctx_tr->drl1_cdf, 2);
  AVERAGE_CDF(ctx_left->drl2_cdf, ctx_tr->drl2_cdf, 2);
#else
  AVERAGE_CDF(ctx_left->refmv_cdf, ctx_tr->refmv_cdf, 2);
  AVERAGE_CDF(ctx_left->drl_cdf, ctx_tr->drl_cdf, 2);
#endif  // CONFIG_NEW_INTER_MODES
  AVERAGE_CDF(ctx_left->inter_compound_mode_cdf,
              ctx_tr->inter_compound_mode_cdf, INTER_COMPOUND_MODES);
  AVERAGE_CDF(ctx_left->compound_type_cdf, ctx_tr->compound_type_cdf,
              MASKED_COMPOUND_TYPES);
  AVERAGE_CDF(ctx_left->wedge_idx_cdf, ctx_tr->wedge_idx_cdf, 16);
  AVERAGE_CDF(ctx_left->interintra_cdf, ctx_tr->interintra_cdf, 2);
  AVERAGE_CDF(ctx_left->wedge_interintra_cdf, ctx_tr->wedge_interintra_cdf, 2);
  AVERAGE_CDF(ctx_left->interintra_mode_cdf, ctx_tr->interintra_mode_cdf,
              INTERINTRA_MODES);
  AVERAGE_CDF(ctx_left->motion_mode_cdf, ctx_tr->motion_mode_cdf, MOTION_MODES);
  AVERAGE_CDF(ctx_left->obmc_cdf, ctx_tr->obmc_cdf, 2);
  AVERAGE_CDF(ctx_left->palette_y_size_cdf, ctx_tr->palette_y_size_cdf,
              PALETTE_SIZES);
  AVERAGE_CDF(ctx_left->palette_uv_size_cdf, ctx_tr->palette_uv_size_cdf,
              PALETTE_SIZES);
  for (int j = 0; j < PALETTE_SIZES; j++) {
    int nsymbs = j + PALETTE_MIN_SIZE;
    AVG_CDF_STRIDE(ctx_left->palette_y_color_index_cdf[j],
                   ctx_tr->palette_y_color_index_cdf[j], nsymbs,
                   CDF_SIZE(PALETTE_COLORS));
    AVG_CDF_STRIDE(ctx_left->palette_uv_color_index_cdf[j],
                   ctx_tr->palette_uv_color_index_cdf[j], nsymbs,
                   CDF_SIZE(PALETTE_COLORS));
  }
  AVERAGE_CDF(ctx_left->palette_y_mode_cdf, ctx_tr->palette_y_mode_cdf, 2);
  AVERAGE_CDF(ctx_left->palette_uv_mode_cdf, ctx_tr->palette_uv_mode_cdf, 2);
  AVERAGE_CDF(ctx_left->comp_inter_cdf, ctx_tr->comp_inter_cdf, 2);
  AVERAGE_CDF(ctx_left->single_ref_cdf, ctx_tr->single_ref_cdf, 2);
  AVERAGE_CDF(ctx_left->comp_ref_type_cdf, ctx_tr->comp_ref_type_cdf, 2);
  AVERAGE_CDF(ctx_left->uni_comp_ref_cdf, ctx_tr->uni_comp_ref_cdf, 2);
  AVERAGE_CDF(ctx_left->comp_ref_cdf, ctx_tr->comp_ref_cdf, 2);
  AVERAGE_CDF(ctx_left->comp_bwdref_cdf, ctx_tr->comp_bwdref_cdf, 2);
#if CONFIG_NEW_TX_PARTITION
  // Square blocks
  AVERAGE_CDF(ctx_left->txfm_partition_cdf[0], ctx_tr->txfm_partition_cdf[0],
              TX_PARTITION_TYPES);
  // Rectangular blocks
  AVERAGE_CDF(ctx_left->txfm_partition_cdf[1], ctx_tr->txfm_partition_cdf[1],
              TX_PARTITION_TYPES);
#else   // CONFIG_NEW_TX_PARTITION
  AVERAGE_CDF(ctx_left->txfm_partition_cdf, ctx_tr->txfm_partition_cdf, 2);
#endif  // CONFIG_NEW_TX_PARTITION
  AVERAGE_CDF(ctx_left->compound_index_cdf, ctx_tr->compound_index_cdf, 2);
  AVERAGE_CDF(ctx_left->comp_group_idx_cdf, ctx_tr->comp_group_idx_cdf, 2);
  AVERAGE_CDF(ctx_left->skip_mode_cdfs, ctx_tr->skip_mode_cdfs, 2);
  AVERAGE_CDF(ctx_left->skip_cdfs, ctx_tr->skip_cdfs, 2);
  AVERAGE_CDF(ctx_left->intra_inter_cdf, ctx_tr->intra_inter_cdf, 2);
  avg_nmv(&ctx_left->nmvc, &ctx_tr->nmvc, wt_left, wt_tr);
  avg_nmv(&ctx_left->ndvc, &ctx_tr->ndvc, wt_left, wt_tr);
  AVERAGE_CDF(ctx_left->intrabc_cdf, ctx_tr->intrabc_cdf, 2);
#if CONFIG_DERIVED_INTRA_MODE
  AVERAGE_CDF(ctx_left->derived_intra_mode_cdf, ctx_tr->derived_intra_mode_cdf,
              2);
  AVERAGE_CDF(ctx_left->uv_derived_intra_mode_cdf,
              ctx_tr->uv_derived_intra_mode_cdf, 2);
#endif  // CONFIG_DERIVED_INTRA_MODE
  AVERAGE_CDF(ctx_left->seg.tree_cdf, ctx_tr->seg.tree_cdf, MAX_SEGMENTS);
  AVERAGE_CDF(ctx_left->seg.pred_cdf, ctx_tr->seg.pred_cdf, 2);
  AVERAGE_CDF(ctx_left->seg.spatial_pred_seg_cdf,
              ctx_tr->seg.spatial_pred_seg_cdf, MAX_SEGMENTS);
  AVERAGE_CDF(ctx_left->filter_intra_cdfs, ctx_tr->filter_intra_cdfs, 2);
  AVERAGE_CDF(ctx_left->filter_intra_mode_cdf, ctx_tr->filter_intra_mode_cdf,
              FILTER_INTRA_MODES);
  AVERAGE_CDF(ctx_left->switchable_restore_cdf, ctx_tr->switchable_restore_cdf,
              RESTORE_SWITCHABLE_TYPES);
  AVERAGE_CDF(ctx_left->wiener_restore_cdf, ctx_tr->wiener_restore_cdf, 2);
  AVERAGE_CDF(ctx_left->sgrproj_restore_cdf, ctx_tr->sgrproj_restore_cdf, 2);
#if CONFIG_WIENER_NONSEP
  AVERAGE_CDF(ctx_left->wiener_nonsep_restore_cdf,
              ctx_tr->wiener_nonsep_restore_cdf, 2);
#endif  // CONFIG_WIENER_NONSEP
#if CONFIG_DERIVED_INTRA_MODE
  AVERAGE_CDF(ctx_left->bf_is_dr_mode_cdf, ctx_tr->bf_is_dr_mode_cdf, 2);
  AVERAGE_CDF(ctx_left->bf_dr_mode_cdf, ctx_tr->bf_dr_mode_cdf,
              DIRECTIONAL_MODES);
  AVERAGE_CDF(ctx_left->bf_none_dr_mode_cdf, ctx_tr->bf_none_dr_mode_cdf,
              NONE_DIRECTIONAL_MODES);
#else
  AVERAGE_CDF(ctx_left->y_mode_cdf, ctx_tr->y_mode_cdf, INTRA_MODES);
#endif  // CONFIG_DERIVED_INTRA_MODE
#if !CONFIG_INTRA_ENTROPY
  AVG_CDF_STRIDE(ctx_left->uv_mode_cdf[0], ctx_tr->uv_mode_cdf[0],
                 UV_INTRA_MODES - 1, CDF_SIZE(UV_INTRA_MODES));
  AVERAGE_CDF(ctx_left->uv_mode_cdf[1], ctx_tr->uv_mode_cdf[1], UV_INTRA_MODES);
#if CONFIG_DERIVED_INTRA_MODE
  AVERAGE_CDF(ctx_left->kf_is_dr_mode_cdf, ctx_tr->kf_is_dr_mode_cdf, 2);
  AVERAGE_CDF(ctx_left->kf_dr_mode_cdf, ctx_tr->kf_dr_mode_cdf,
              DIRECTIONAL_MODES);
  AVERAGE_CDF(ctx_left->kf_none_dr_mode_cdf, ctx_tr->kf_none_dr_mode_cdf,
              NONE_DIRECTIONAL_MODES);
#else
  AVERAGE_CDF(ctx_left->kf_y_cdf, ctx_tr->kf_y_cdf, INTRA_MODES);
#endif  // CONFIG_DERIVED_INTRA_MODE
#endif  // !CONFIG_INTRA_ENTROPY
  for (int i = 0; i < PARTITION_CONTEXTS; i++) {
    if (i < 4) {
      AVG_CDF_STRIDE(ctx_left->partition_cdf[i], ctx_tr->partition_cdf[i], 4,
                     CDF_SIZE(10));
    } else if (i < 16) {
      AVERAGE_CDF(ctx_left->partition_cdf[i], ctx_tr->partition_cdf[i], 10);
    } else {
      AVG_CDF_STRIDE(ctx_left->partition_cdf[i], ctx_tr->partition_cdf[i], 8,
                     CDF_SIZE(10));
    }
  }
#if CONFIG_EXT_RECUR_PARTITIONS
  for (int i = 0; i < PARTITION_CONTEXTS_REC; ++i) {
    AVERAGE_CDF(ctx_left->partition_rec_cdf[i], ctx_tr->partition_rec_cdf[i],
                4);
  }
#endif  // CONFIG_EXT_RECUR_PARTITIONS
  AVERAGE_CDF(ctx_left->switchable_interp_cdf, ctx_tr->switchable_interp_cdf,
              SWITCHABLE_FILTERS);
#if CONFIG_FLEX_MVRES
  for (int p = MV_SUBPEL_QTR_PRECISION; p < MV_SUBPEL_PRECISIONS; ++p) {
    for (int j = 0; j < MV_PREC_DOWN_CONTEXTS; ++j) {
#if DISALLOW_ONE_DOWN_FLEX_MVRES == 2
      AVG_CDF_STRIDE(
          ctx_left->pb_mv_precision_cdf[j][p - MV_SUBPEL_QTR_PRECISION],
          ctx_tr->pb_mv_precision_cdf[j][p - MV_SUBPEL_QTR_PRECISION], 2,
          CDF_SIZE(2));
#elif DISALLOW_ONE_DOWN_FLEX_MVRES == 1
      AVG_CDF_STRIDE(
          ctx_left->pb_mv_precision_cdf[j][p - MV_SUBPEL_QTR_PRECISION],
          ctx_tr->pb_mv_precision_cdf[j][p - MV_SUBPEL_QTR_PRECISION], p,
          CDF_SIZE(MV_SUBPEL_PRECISIONS - 1));
#else
      AVG_CDF_STRIDE(
          ctx_left->pb_mv_precision_cdf[j][p - MV_SUBPEL_QTR_PRECISION],
          ctx_tr->pb_mv_precision_cdf[j][p - MV_SUBPEL_QTR_PRECISION], p + 1,
          CDF_SIZE(MV_SUBPEL_PRECISIONS));
#endif  // DISALLOW_ONE_DOWN_FLEX_MVRES
    }
  }
#endif  // CONFIG_FLEX_MVRES
  AVERAGE_CDF(ctx_left->angle_delta_cdf, ctx_tr->angle_delta_cdf,
              2 * MAX_ANGLE_DELTA + 1);
#if CONFIG_NEW_TX_PARTITION
  AVERAGE_CDF(ctx_left->tx_size_cdf[0], ctx_tr->tx_size_cdf[0],
              TX_PARTITION_TYPES_INTRA);
  AVERAGE_CDF(ctx_left->tx_size_cdf[1], ctx_tr->tx_size_cdf[1],
              TX_PARTITION_TYPES_INTRA);
#else
  AVG_CDF_STRIDE(ctx_left->tx_size_cdf[0], ctx_tr->tx_size_cdf[0], MAX_TX_DEPTH,
                 CDF_SIZE(MAX_TX_DEPTH + 1));
  AVERAGE_CDF(ctx_left->tx_size_cdf[1], ctx_tr->tx_size_cdf[1],
              MAX_TX_DEPTH + 1);
  AVERAGE_CDF(ctx_left->tx_size_cdf[2], ctx_tr->tx_size_cdf[2],
              MAX_TX_DEPTH + 1);
  AVERAGE_CDF(ctx_left->tx_size_cdf[3], ctx_tr->tx_size_cdf[3],
              MAX_TX_DEPTH + 1);
#endif  // CONFIG_NEW_TX_PARTITION
  AVERAGE_CDF(ctx_left->delta_q_cdf, ctx_tr->delta_q_cdf, DELTA_Q_PROBS + 1);
  AVERAGE_CDF(ctx_left->delta_lf_cdf, ctx_tr->delta_lf_cdf, DELTA_LF_PROBS + 1);
  for (int i = 0; i < FRAME_LF_COUNT; i++) {
    AVERAGE_CDF(ctx_left->delta_lf_multi_cdf[i], ctx_tr->delta_lf_multi_cdf[i],
                DELTA_LF_PROBS + 1);
  }
  AVG_CDF_STRIDE(ctx_left->intra_ext_tx_cdf[1], ctx_tr->intra_ext_tx_cdf[1], 7,
                 CDF_SIZE(TX_TYPES));
  AVG_CDF_STRIDE(ctx_left->intra_ext_tx_cdf[2], ctx_tr->intra_ext_tx_cdf[2], 5,
                 CDF_SIZE(TX_TYPES));
  AVG_CDF_STRIDE(ctx_left->inter_ext_tx_cdf[1], ctx_tr->inter_ext_tx_cdf[1], 16,
                 CDF_SIZE(TX_TYPES));
  AVG_CDF_STRIDE(ctx_left->inter_ext_tx_cdf[2], ctx_tr->inter_ext_tx_cdf[2], 12,
                 CDF_SIZE(TX_TYPES));
  AVG_CDF_STRIDE(ctx_left->inter_ext_tx_cdf[3], ctx_tr->inter_ext_tx_cdf[3], 2,
                 CDF_SIZE(TX_TYPES));
  AVERAGE_CDF(ctx_left->cfl_sign_cdf, ctx_tr->cfl_sign_cdf, CFL_JOINT_SIGNS);
  AVERAGE_CDF(ctx_left->cfl_alpha_cdf, ctx_tr->cfl_alpha_cdf,
              CFL_ALPHABET_SIZE);
}

#if !CONFIG_REALTIME_ONLY
static AOM_INLINE void adjust_rdmult_tpl_model(AV1_COMP *cpi, MACROBLOCK *x,
                                               int mi_row, int mi_col) {
  const BLOCK_SIZE sb_size = cpi->common.seq_params.sb_size;
  const int orig_rdmult = cpi->rd.RDMULT;

  if (cpi->tpl_model_pass == 1) {
    assert(cpi->oxcf.enable_tpl_model == 2);
    x->rdmult = orig_rdmult;
    return;
  }

  assert(IMPLIES(cpi->gf_group.size > 0,
                 cpi->gf_group.index < cpi->gf_group.size));
  const int gf_group_index = cpi->gf_group.index;
  if (cpi->oxcf.enable_tpl_model && cpi->oxcf.aq_mode == NO_AQ &&
      cpi->oxcf.deltaq_mode == NO_DELTA_Q && gf_group_index > 0 &&
      cpi->gf_group.update_type[gf_group_index] == ARF_UPDATE) {
    const int dr =
        get_rdmult_delta(cpi, sb_size, 0, mi_row, mi_col, orig_rdmult);
    x->rdmult = dr;
  }
}
#endif

static AOM_INLINE void encode_nonrd_sb(AV1_COMP *cpi, ThreadData *td,
                                       TileDataEnc *tile_data, TOKENEXTRA **tp,
                                       const int mi_row, const int mi_col,
                                       CHROMA_REF_INFO *sb_chr_ref_info) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO **mi = cm->mi_grid_base + get_mi_grid_idx(cm, mi_row, mi_col);

  const TileInfo *const tile_info = &tile_data->tile_info;
  const int num_planes = av1_num_planes(cm);
  const int ss_x = xd->plane[1].subsampling_x;
  const int ss_y = xd->plane[1].subsampling_y;
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;

  setup_delta_q(cpi, td, x, tile_info, mi_row, mi_col, num_planes);
  set_offsets_without_segment_id(cpi, tile_info, x, mi_row, mi_col, sb_size,
                                 sb_chr_ref_info);
  av1_choose_var_based_partitioning(cpi, tile_info, x, mi_row, mi_col);
  td->mb.cb_offset = 0;
  PC_TREE *pc_root = av1_alloc_pc_tree_node(mi_row, mi_col, sb_size, NULL,
                                            PARTITION_NONE, 0, 1, ss_x, ss_y);
  av1_reset_ptree_in_sbi(xd->sbi);
  xd->sbi->sb_mv_precision = cm->fr_mv_precision;
  nonrd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, sb_size,
                      pc_root, xd->sbi->ptree_root);
  av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0);
}

#if !CONFIG_REALTIME_ONLY
// Memset the mbmis at the current superblock to 0
static INLINE void reset_mbmi(AV1_COMMON *cm, int mi_row, int mi_col) {
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;
  // size of sb in unit of mi_grid (BLOCK_4X4)
  const int sb_size_mi = mi_size_wide[sb_size];
  assert(cm->mi_stride % sb_size_mi == 0 &&
         "mi is not allocated as a multiple of sb!");

  const int mi_rows = mi_size_high[sb_size];
  for (int cur_mi_row = 0; cur_mi_row < mi_rows; cur_mi_row++) {
    const int mi_cols =
        AOMMIN(sb_size_mi, cm->mi_stride - (mi_col + sb_size_mi));
    assert(get_mi_grid_idx(cm, 0, mi_col + mi_cols) < cm->mi_stride);
    const int mi_grid_idx = get_mi_grid_idx(cm, mi_row + cur_mi_row, mi_col);
    memset(&cm->mi_grid_base[mi_grid_idx], 0,
           mi_cols * sizeof(*cm->mi_grid_base));
    memset(&cm->mi[mi_grid_idx], 0, mi_cols * sizeof(*cm->mi));
  }
}

static INLINE void backup_sb_state(SB_FIRST_PASS_STATS *sb_fp_stats,
                                   const AV1_COMP *cpi, ThreadData *td,
                                   const TileDataEnc *tile_data, int mi_row,
                                   int mi_col) {
  MACROBLOCK *x = &td->mb;
  MACROBLOCKD *xd = &x->e_mbd;
  const TileInfo *tile_info = &tile_data->tile_info;

  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;

  xd->above_txfm_context = cm->above_txfm_context[tile_info->tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);
  save_context(x, &sb_fp_stats->x_ctx, mi_row, mi_col, sb_size, num_planes);

  sb_fp_stats->rd_count = cpi->td.rd_counts;
  sb_fp_stats->split_count = cpi->td.mb.txb_split_count;

  sb_fp_stats->fc = *td->counts;

  memcpy(sb_fp_stats->inter_mode_rd_models, tile_data->inter_mode_rd_models,
         sizeof(sb_fp_stats->inter_mode_rd_models));

  memcpy(sb_fp_stats->thresh_freq_fact, x->thresh_freq_fact,
         sizeof(sb_fp_stats->thresh_freq_fact));

  sb_fp_stats->m_search_count = *x->m_search_count_ptr;
  sb_fp_stats->ex_search_count = *x->ex_search_count_ptr;

  const int mi_idx = get_mi_grid_idx(cm, mi_row, mi_col);
  sb_fp_stats->current_qindex = cm->mi[mi_idx].current_qindex;

#if CONFIG_INTERNAL_STATS
  memcpy(sb_fp_stats->mode_chosen_counts, cpi->mode_chosen_counts,
         sizeof(sb_fp_stats->mode_chosen_counts));
#endif  // CONFIG_INTERNAL_STATS
}

static INLINE void restore_sb_state(const SB_FIRST_PASS_STATS *sb_fp_stats,
                                    AV1_COMP *cpi, ThreadData *td,
                                    TileDataEnc *tile_data, int mi_row,
                                    int mi_col) {
  MACROBLOCK *x = &td->mb;

  const AV1_COMMON *cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;

  restore_context(cm, x, &sb_fp_stats->x_ctx, mi_row, mi_col, sb_size,
                  num_planes);

  cpi->td.rd_counts = sb_fp_stats->rd_count;
  cpi->td.mb.txb_split_count = sb_fp_stats->split_count;

  *td->counts = sb_fp_stats->fc;

  memcpy(tile_data->inter_mode_rd_models, sb_fp_stats->inter_mode_rd_models,
         sizeof(sb_fp_stats->inter_mode_rd_models));
  memcpy(x->thresh_freq_fact, sb_fp_stats->thresh_freq_fact,
         sizeof(sb_fp_stats->thresh_freq_fact));

  *x->m_search_count_ptr = sb_fp_stats->m_search_count;
  *x->ex_search_count_ptr = sb_fp_stats->ex_search_count;

  const int mi_idx = get_mi_grid_idx(cm, mi_row, mi_col);
  cm->mi[mi_idx].current_qindex = sb_fp_stats->current_qindex;

#if CONFIG_INTERNAL_STATS
  memcpy(cpi->mode_chosen_counts, sb_fp_stats->mode_chosen_counts,
         sizeof(sb_fp_stats->mode_chosen_counts));
#endif  // CONFIG_INTERNAL_STATS
}

// This function initializes the stats for encode_rd_sb.
static INLINE void init_encode_rd_sb(AV1_COMP *cpi, ThreadData *td,
                                     const TileDataEnc *tile_data,
                                     PC_TREE **pc_root,
                                     SIMPLE_MOTION_DATA_TREE *sms_root,
                                     RD_STATS *rd_cost, int mi_row, int mi_col,
                                     int gather_tpl_data) {
  const AV1_COMMON *cm = &cpi->common;
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;
  const TileInfo *tile_info = &tile_data->tile_info;
  MACROBLOCK *x = &td->mb;

  if (!*pc_root) {
    const MACROBLOCKD *xd = &x->e_mbd;
    const int ss_x = xd->plane[1].subsampling_x;
    const int ss_y = xd->plane[1].subsampling_y;
    *pc_root = av1_alloc_pc_tree_node(mi_row, mi_col, sb_size, NULL,
                                      PARTITION_NONE, 0, 1, ss_x, ss_y);
  }

  x->sb_energy_level = 0;
  x->cnn_output_valid = 0;
  if (gather_tpl_data) {
    if (cm->delta_q_info.delta_q_present_flag) {
      const int num_planes = av1_num_planes(cm);
      setup_delta_q(cpi, td, x, tile_info, mi_row, mi_col, num_planes);
      av1_tpl_rdmult_setup_sb(cpi, x, sb_size, mi_row, mi_col);
    }
    if (cpi->oxcf.enable_tpl_model) {
      adjust_rdmult_tpl_model(cpi, x, mi_row, mi_col);
    }
  }

  // Reset hash state for transform/mode rd hash information
  reset_hash_records(x, 0);
  av1_zero(x->picked_ref_frames_mask);
  av1_zero(x->pred_mv);
  av1_invalid_rd_stats(rd_cost);

  const SPEED_FEATURES *sf = &cpi->sf;
  const int use_simple_motion_search =
      (sf->simple_motion_search_split || sf->simple_motion_search_prune_rect ||
       sf->simple_motion_search_early_term_none ||
       sf->ml_early_term_after_part_split_level) &&
      !frame_is_intra_only(cm);
  if (use_simple_motion_search) {
    reset_simple_motion_tree_partition(sms_root, sb_size);
    init_simple_motion_search_mvs(sms_root);
  }
}

static AOM_INLINE void get_max_min_partition_size(AV1_COMP *cpi, ThreadData *td,
                                                  BLOCK_SIZE *max_sq_size,
                                                  BLOCK_SIZE *min_sq_size,
                                                  int mi_row, int mi_col) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCK *x = &td->mb;
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;

  switch (cpi->oxcf.max_partition_size) {
    case 4: *max_sq_size = BLOCK_4X4; break;
    case 8: *max_sq_size = BLOCK_8X8; break;
    case 16: *max_sq_size = BLOCK_16X16; break;
    case 32: *max_sq_size = BLOCK_32X32; break;
    case 64: *max_sq_size = BLOCK_64X64; break;
    case 128: *max_sq_size = BLOCK_128X128; break;
    default: assert(0); break;
  }
  *max_sq_size = AOMMIN(*max_sq_size, sb_size);

  switch (cpi->oxcf.min_partition_size) {
    case 4: *min_sq_size = BLOCK_4X4; break;
    case 8: *min_sq_size = BLOCK_8X8; break;
    case 16: *min_sq_size = BLOCK_16X16; break;
    case 32: *min_sq_size = BLOCK_32X32; break;
    case 64: *min_sq_size = BLOCK_64X64; break;
    case 128: *min_sq_size = BLOCK_128X128; break;
    default: assert(0); break;
  }

  if (use_auto_max_partition(cpi, sb_size, mi_row, mi_col)) {
    float features[FEATURE_SIZE_MAX_MIN_PART_PRED] = { 0.0f };

    av1_get_max_min_partition_features(cpi, x, mi_row, mi_col, features);
    *max_sq_size =
        AOMMIN(av1_predict_max_partition(cpi, x, features), *max_sq_size);
  }

  *min_sq_size = AOMMIN(*min_sq_size, *max_sq_size);
}

static AOM_INLINE void encode_rd_sb(AV1_COMP *cpi, ThreadData *td,
                                    TileDataEnc *tile_data, TOKENEXTRA **tp,
                                    const int mi_row, const int mi_col,
                                    const int seg_skip,
                                    CHROMA_REF_INFO *sb_chr_ref_info) {
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  const SPEED_FEATURES *const sf = &cpi->sf;
  const TileInfo *const tile_info = &tile_data->tile_info;
  MB_MODE_INFO **mi = cm->mi_grid_base + get_mi_grid_idx(cm, mi_row, mi_col);
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;
  const int num_planes = av1_num_planes(cm);
  const int mib_size_log2 = cm->seq_params.mib_size_log2;

  SIMPLE_MOTION_DATA_TREE *sms_root =
      td->sms_root[mib_size_log2 - MIN_MIB_SIZE_LOG2];
  PC_TREE *pc_root = NULL;

  int dummy_rate;
  int64_t dummy_dist;
  RD_STATS dummy_rdc;

  init_encode_rd_sb(cpi, td, tile_data, &pc_root, sms_root, &dummy_rdc, mi_row,
                    mi_col, 1);

  if (sf->partition_search_type == FIXED_PARTITION || seg_skip) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, sb_size, sb_chr_ref_info);
    const BLOCK_SIZE bsize = seg_skip ? sb_size : sf->always_this_block_size;
    set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
    rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, sb_size,
                     &dummy_rate, &dummy_dist, 1, pc_root);
    av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0);
  } else if (cpi->partition_search_skippable_frame) {
    set_offsets(cpi, tile_info, x, mi_row, mi_col, sb_size, sb_chr_ref_info);
    const BLOCK_SIZE bsize =
        get_rd_var_based_fixed_partition(cpi, x, mi_row, mi_col);
    set_fixed_partitioning(cpi, tile_info, mi, mi_row, mi_col, bsize);
    rd_use_partition(cpi, td, tile_data, mi, tp, mi_row, mi_col, sb_size,
                     &dummy_rate, &dummy_dist, 1, pc_root);
    av1_free_pc_tree_recursive(pc_root, num_planes, 0, 0);
  } else {
    // Note: since rd_pick_partition searches all blocks recursively under
    // CONFIG_EXT_PARTITIONS, we deallocate the memory inside rd_pick_partition
    // instead of outside.
#if CONFIG_COLLECT_COMPONENT_TIMING
    start_timing(cpi, rd_pick_partition_time);
#endif
    BLOCK_SIZE max_sq_size = BLOCK_INVALID, min_sq_size = BLOCK_INVALID;
    get_max_min_partition_size(cpi, td, &max_sq_size, &min_sq_size, mi_row,
                               mi_col);

    const int num_passes = cpi->oxcf.sb_multipass_unit_test ? 2 : 1;

    MvSubpelPrecision best_prec = cm->fr_mv_precision;
#if CONFIG_FLEX_MVRES
    if (num_passes == 1) {
      SB_FIRST_PASS_STATS sb_fp_stats;
      backup_sb_state(&sb_fp_stats, cpi, td, tile_data, mi_row, mi_col);

      int64_t best_rdc = INT64_MAX;
      if (!frame_is_intra_only(cm) && cm->use_sb_mv_precision) {
        for (MvSubpelPrecision mv_prec = MV_SUBPEL_NONE;
             mv_prec <= cm->fr_mv_precision; mv_prec++) {
          if (!pc_root) {
            init_encode_rd_sb(cpi, td, tile_data, &pc_root, sms_root,
                              &dummy_rdc, mi_row, mi_col, 0);
            reset_mbmi(&cpi->common, mi_row, mi_col);
            restore_sb_state(&sb_fp_stats, cpi, td, tile_data, mi_row, mi_col);
          }

          x->e_mbd.sbi->sb_mv_precision = mv_prec;

          rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, sb_size,
                            max_sq_size, min_sq_size, &dummy_rdc, dummy_rdc,
                            pc_root, sms_root, NULL, SB_DRY_PASS);
          pc_root = NULL;
          if (dummy_rdc.rdcost < best_rdc) {
            best_rdc = dummy_rdc.rdcost;
            best_prec = mv_prec;
          }
        }
      }

      if (!pc_root) {
        init_encode_rd_sb(cpi, td, tile_data, &pc_root, sms_root, &dummy_rdc,
                          mi_row, mi_col, 0);
        reset_mbmi(&cpi->common, mi_row, mi_col);
        restore_sb_state(&sb_fp_stats, cpi, td, tile_data, mi_row, mi_col);
      }

#endif
      x->e_mbd.sbi->sb_mv_precision = best_prec;

      rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, sb_size,
                        max_sq_size, min_sq_size, &dummy_rdc, dummy_rdc,
                        pc_root, sms_root, NULL, SB_WET_PASS);
      pc_root = NULL;
    } else {
      // First pass
      SB_FIRST_PASS_STATS sb_fp_stats;
      backup_sb_state(&sb_fp_stats, cpi, td, tile_data, mi_row, mi_col);
      rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, sb_size,
                        max_sq_size, min_sq_size, &dummy_rdc, dummy_rdc,
                        pc_root, sms_root, NULL, SB_DRY_PASS);

      // Second pass
      pc_root = NULL;
      init_encode_rd_sb(cpi, td, tile_data, &pc_root, sms_root, &dummy_rdc,
                        mi_row, mi_col, 0);
      reset_mbmi(&cpi->common, mi_row, mi_col);

      restore_sb_state(&sb_fp_stats, cpi, td, tile_data, mi_row, mi_col);

      rd_pick_partition(cpi, td, tile_data, tp, mi_row, mi_col, sb_size,
                        max_sq_size, min_sq_size, &dummy_rdc, dummy_rdc,
                        pc_root, sms_root, NULL, SB_WET_PASS);
    }
  }
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, rd_pick_partition_time);
#endif

  // TODO(angiebird): Let inter_mode_rd_model_estimation support multi-tile.
  if (cpi->sf.inter_mode_rd_model_estimation == 1 && cm->tile_cols == 1 &&
      cm->tile_rows == 1) {
    av1_inter_mode_data_fit(tile_data, x->rdmult);
  }
}
#endif  // !CONFIG_REALTIME_ONLY

static AOM_INLINE void set_cost_upd_freq(AV1_COMP *cpi, ThreadData *td,
                                         const TileInfo *const tile_info,
                                         const int mi_row, const int mi_col) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  switch (cpi->oxcf.coeff_cost_upd_freq) {
    case COST_UPD_TILE:  // Tile level
      if (mi_row != tile_info->mi_row_start) break;
      AOM_FALLTHROUGH_INTENDED;
    case COST_UPD_SBROW:  // SB row level in tile
      if (mi_col != tile_info->mi_col_start) break;
      AOM_FALLTHROUGH_INTENDED;
    case COST_UPD_SB:  // SB level
      av1_fill_coeff_costs(&td->mb, xd->tile_ctx, num_planes);
      break;
    default: assert(0);
  }

  switch (cpi->oxcf.mode_cost_upd_freq) {
    case COST_UPD_TILE:  // Tile level
      if (mi_row != tile_info->mi_row_start) break;
      AOM_FALLTHROUGH_INTENDED;
    case COST_UPD_SBROW:  // SB row level in tile
      if (mi_col != tile_info->mi_col_start) break;
      AOM_FALLTHROUGH_INTENDED;
    case COST_UPD_SB:  // SB level
      av1_fill_mode_rates(cm, x, xd->tile_ctx);
      break;
    default: assert(0);
  }

  switch (cpi->oxcf.mv_cost_upd_freq) {
    case COST_UPD_TILE:  // Tile level
      if (mi_row != tile_info->mi_row_start) break;
      AOM_FALLTHROUGH_INTENDED;
    case COST_UPD_SBROW:  // SB row level in tile
      if (mi_col != tile_info->mi_col_start) break;
      AOM_FALLTHROUGH_INTENDED;
    case COST_UPD_SB:  // SB level
      av1_fill_mv_costs(xd->tile_ctx, cm, x);
      break;
    default: assert(0);
  }
}

static void encode_sb_row(AV1_COMP *cpi, ThreadData *td, TileDataEnc *tile_data,
                          int mi_row, TOKENEXTRA **tp, int use_nonrd_mode) {
  AV1_COMMON *const cm = &cpi->common;
  const TileInfo *const tile_info = &tile_data->tile_info;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  const SPEED_FEATURES *const sf = &cpi->sf;
  const int sb_cols_in_tile = av1_get_sb_cols_in_tile(cm, tile_data->tile_info);
  const BLOCK_SIZE sb_size = cm->seq_params.sb_size;
  const int mib_size = cm->seq_params.mib_size;
  const int mib_size_log2 = cm->seq_params.mib_size_log2;
  const int sb_row = (mi_row - tile_info->mi_row_start) >> mib_size_log2;

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, encode_sb_time);
#endif

  // Initialize the left context for the new SB row
  av1_zero_left_context(xd);

  // Reset delta for every tile
  if (mi_row == tile_info->mi_row_start || cpi->row_mt) {
    if (cm->delta_q_info.delta_q_present_flag)
      xd->current_qindex = cm->base_qindex;
    if (cm->delta_q_info.delta_lf_present_flag) {
      av1_reset_loop_filter_delta(xd, av1_num_planes(cm));
    }
  }
  reset_thresh_freq_fact(x);

  // Code each SB in the row
  for (int mi_col = tile_info->mi_col_start, sb_col_in_tile = 0;
       mi_col < tile_info->mi_col_end; mi_col += mib_size, sb_col_in_tile++) {
    (*(cpi->row_mt_sync_read_ptr))(&tile_data->row_mt_sync, sb_row,
                                   sb_col_in_tile);

    av1_reset_is_mi_coded_map(xd, cm->seq_params.mib_size);
    av1_set_sb_info(cm, xd, mi_row, mi_col);

    if (tile_data->allow_update_cdf && (cpi->row_mt == 1) &&
        (tile_info->mi_row_start != mi_row)) {
      if ((tile_info->mi_col_start == mi_col)) {
        // restore frame context of 1st column sb
        memcpy(xd->tile_ctx, x->row_ctx, sizeof(*xd->tile_ctx));
      } else {
        int wt_left = AVG_CDF_WEIGHT_LEFT;
        int wt_tr = AVG_CDF_WEIGHT_TOP_RIGHT;
        if (tile_info->mi_col_end > (mi_col + mib_size))
          avg_cdf_symbols(xd->tile_ctx, x->row_ctx + sb_col_in_tile, wt_left,
                          wt_tr);
        else
          avg_cdf_symbols(xd->tile_ctx, x->row_ctx + sb_col_in_tile - 1,
                          wt_left, wt_tr);
      }
    }

    set_cost_upd_freq(cpi, td, tile_info, mi_row, mi_col);

    xd->cur_frame_force_integer_mv = cm->cur_frame_force_integer_mv;
    td->mb.cb_coef_buff = av1_get_cb_coeff_buffer(cpi, mi_row, mi_col);
    x->source_variance = UINT_MAX;
    x->simple_motion_pred_sse = UINT_MAX;

    const struct segmentation *const seg = &cm->seg;
    int seg_skip = 0;
    if (seg->enabled) {
      const uint8_t *const map =
          seg->update_map ? cpi->segmentation_map : cm->last_frame_seg_map;
      const int segment_id =
          map ? get_segment_id(cm, map, sb_size, mi_row, mi_col) : 0;
      seg_skip = segfeature_active(seg, segment_id, SEG_LVL_SKIP);
    }

    CHROMA_REF_INFO sb_chr_ref_info = {
      1, 0, mi_row, mi_col, sb_size, sb_size
    };
    if (!(sf->partition_search_type == FIXED_PARTITION || seg_skip) &&
        !cpi->partition_search_skippable_frame &&
        sf->partition_search_type == VAR_BASED_PARTITION && use_nonrd_mode) {
      // Realtime non-rd path
      encode_nonrd_sb(cpi, td, tile_data, tp, mi_row, mi_col, &sb_chr_ref_info);
    } else {
#if !CONFIG_REALTIME_ONLY
      // Non-realtime rd path
      encode_rd_sb(cpi, td, tile_data, tp, mi_row, mi_col, seg_skip,
                   &sb_chr_ref_info);
#endif  // !CONFIG_REALTIME_ONLY
    }
    if (tile_data->allow_update_cdf && (cpi->row_mt == 1) &&
        (tile_info->mi_row_end > (mi_row + mib_size))) {
      if (sb_cols_in_tile == 1)
        memcpy(x->row_ctx, xd->tile_ctx, sizeof(*xd->tile_ctx));
      else if (sb_col_in_tile >= 1)
        memcpy(x->row_ctx + sb_col_in_tile - 1, xd->tile_ctx,
               sizeof(*xd->tile_ctx));
    }
    (*(cpi->row_mt_sync_write_ptr))(&tile_data->row_mt_sync, sb_row,
                                    sb_col_in_tile, sb_cols_in_tile);
  }

#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, encode_sb_time);
#endif
}

static void init_encode_frame_mb_context(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &cpi->td.mb;
  MACROBLOCKD *const xd = &x->e_mbd;

  // Copy data over into macro block data structures.
  CHROMA_REF_INFO sb_chr_ref_info = {
    1, 0, 0, 0, cm->seq_params.sb_size, cm->seq_params.sb_size
  };
  av1_setup_src_planes(x, cpi->source, 0, 0, num_planes, &sb_chr_ref_info);

  av1_setup_block_planes(xd, cm->seq_params.subsampling_x,
                         cm->seq_params.subsampling_y, num_planes);
}

static MV_REFERENCE_FRAME get_frame_type(const AV1_COMP *cpi) {
  if (frame_is_intra_only(&cpi->common)) {
    return INTRA_FRAME;
  } else if ((cpi->rc.is_src_frame_alt_ref && cpi->refresh_golden_frame) ||
             cpi->rc.is_src_frame_internal_arf) {
    // We will not update the golden frame with an internal overlay frame
    return ALTREF_FRAME;
  } else if (cpi->refresh_golden_frame || cpi->refresh_alt2_ref_frame ||
             cpi->refresh_alt_ref_frame) {
    return GOLDEN_FRAME;
  } else {
    return LAST_FRAME;
  }
}

static TX_MODE select_tx_mode(const AV1_COMP *cpi) {
  if (cpi->common.coded_lossless) return ONLY_4X4;
  if (cpi->sf.tx_size_search_method == USE_LARGESTALL)
    return TX_MODE_LARGEST;
  else if (cpi->sf.tx_size_search_method == USE_FULL_RD ||
           cpi->sf.tx_size_search_method == USE_FAST_RD)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
}

void av1_alloc_tile_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;

  if (cpi->tile_data != NULL) aom_free(cpi->tile_data);
  CHECK_MEM_ERROR(
      cm, cpi->tile_data,
      aom_memalign(32, tile_cols * tile_rows * sizeof(*cpi->tile_data)));
  cpi->allocated_tiles = tile_cols * tile_rows;
}

void av1_init_tile_data(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int tile_col, tile_row;
  TOKENEXTRA *pre_tok = cpi->tile_tok[0][0];
  TOKENLIST *tplist = cpi->tplist[0][0];
  unsigned int tile_tok = 0;
  int tplist_count = 0;

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileDataEnc *const tile_data =
          &cpi->tile_data[tile_row * tile_cols + tile_col];
      TileInfo *const tile_info = &tile_data->tile_info;
      av1_tile_init(tile_info, cm, tile_row, tile_col);

      cpi->tile_tok[tile_row][tile_col] = pre_tok + tile_tok;
      pre_tok = cpi->tile_tok[tile_row][tile_col];
      tile_tok = allocated_tokens(
          *tile_info, cm->seq_params.mib_size_log2 + MI_SIZE_LOG2, num_planes);
      cpi->tplist[tile_row][tile_col] = tplist + tplist_count;
      tplist = cpi->tplist[tile_row][tile_col];
      tplist_count = av1_get_sb_rows_in_tile(cm, tile_data->tile_info);
      tile_data->allow_update_cdf = !cm->large_scale_tile;
      tile_data->allow_update_cdf =
          tile_data->allow_update_cdf && !cm->disable_cdf_update;
      tile_data->tctx = *cm->fc;
#if CONFIG_INTRA_ENTROPY
      av1_config_entropy_models(&tile_data->tctx);
#endif  // CONFIG_INTRA_ENTROPY
    }
  }
}

void av1_encode_sb_row(AV1_COMP *cpi, ThreadData *td, int tile_row,
                       int tile_col, int mi_row) {
  AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  const int tile_cols = cm->tile_cols;
  TileDataEnc *this_tile = &cpi->tile_data[tile_row * tile_cols + tile_col];
  const TileInfo *const tile_info = &this_tile->tile_info;
  TOKENEXTRA *tok = NULL;
  const int sb_row_in_tile =
      (mi_row - tile_info->mi_row_start) >> cm->seq_params.mib_size_log2;
  const int tile_mb_cols =
      (tile_info->mi_col_end - tile_info->mi_col_start + 2) >> 2;
  const int num_mb_rows_in_sb =
      ((1 << (cm->seq_params.mib_size_log2 + MI_SIZE_LOG2)) + 8) >> 4;

  get_start_tok(cpi, tile_row, tile_col, mi_row, &tok,
                cm->seq_params.mib_size_log2 + MI_SIZE_LOG2, num_planes);
  cpi->tplist[tile_row][tile_col][sb_row_in_tile].start = tok;

  encode_sb_row(cpi, td, this_tile, mi_row, &tok, cpi->sf.use_nonrd_pick_mode);

  cpi->tplist[tile_row][tile_col][sb_row_in_tile].stop = tok;
  cpi->tplist[tile_row][tile_col][sb_row_in_tile].count =
      (unsigned int)(cpi->tplist[tile_row][tile_col][sb_row_in_tile].stop -
                     cpi->tplist[tile_row][tile_col][sb_row_in_tile].start);

  assert(
      (unsigned int)(tok -
                     cpi->tplist[tile_row][tile_col][sb_row_in_tile].start) <=
      get_token_alloc(num_mb_rows_in_sb, tile_mb_cols,
                      cm->seq_params.mib_size_log2 + MI_SIZE_LOG2, num_planes));

  (void)tile_mb_cols;
  (void)num_mb_rows_in_sb;
}

void av1_encode_tile(AV1_COMP *cpi, ThreadData *td, int tile_row,
                     int tile_col) {
  AV1_COMMON *const cm = &cpi->common;
  TileDataEnc *const this_tile =
      &cpi->tile_data[tile_row * cm->tile_cols + tile_col];
  const TileInfo *const tile_info = &this_tile->tile_info;
  int mi_row;

  av1_inter_mode_data_init(this_tile);

  av1_zero_above_context(cm, &td->mb.e_mbd, tile_info->mi_col_start,
                         tile_info->mi_col_end, tile_row);
  av1_init_above_context(cm, &td->mb.e_mbd, tile_row);

  // Set up pointers to per thread motion search counters.
  this_tile->m_search_count = 0;   // Count of motion search hits.
  this_tile->ex_search_count = 0;  // Exhaustive mesh search hits.
  td->mb.m_search_count_ptr = &this_tile->m_search_count;
  td->mb.ex_search_count_ptr = &this_tile->ex_search_count;

  cfl_init(&td->mb.e_mbd.cfl, &cm->seq_params);

  av1_crc32c_calculator_init(&td->mb.mb_rd_record.crc_calculator);

  for (mi_row = tile_info->mi_row_start; mi_row < tile_info->mi_row_end;
       mi_row += cm->seq_params.mib_size) {
    av1_encode_sb_row(cpi, td, tile_row, tile_col, mi_row);
  }
}

static void encode_tiles(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const int tile_cols = cm->tile_cols;
  const int tile_rows = cm->tile_rows;
  int tile_col, tile_row;

  if (cpi->tile_data == NULL || cpi->allocated_tiles < tile_cols * tile_rows)
    av1_alloc_tile_data(cpi);

  av1_init_tile_data(cpi);

  for (tile_row = 0; tile_row < tile_rows; ++tile_row) {
    for (tile_col = 0; tile_col < tile_cols; ++tile_col) {
      TileDataEnc *const this_tile =
          &cpi->tile_data[tile_row * cm->tile_cols + tile_col];
      cpi->td.intrabc_used = 0;
      cpi->td.deltaq_used = 0;
      cpi->td.mb.e_mbd.tile_ctx = &this_tile->tctx;
      cpi->td.mb.tile_pb_ctx = &this_tile->tctx;
      av1_encode_tile(cpi, &cpi->td, tile_row, tile_col);
      cpi->intrabc_used |= cpi->td.intrabc_used;
      cpi->deltaq_used |= cpi->td.deltaq_used;
    }
  }
}

#define GLOBAL_TRANS_TYPES_ENC 3  // highest motion model to search
static int gm_get_params_cost(const WarpedMotionParams *gm,
                              const WarpedMotionParams *ref_gm,
                              MvSubpelPrecision precision) {
#if CONFIG_FLEX_MVRES
  const int precision_reduce = MV_SUBPEL_EIGHTH_PRECISION - precision;
#else
  const int precision_reduce =
      AOMMIN(1, MV_SUBPEL_EIGHTH_PRECISION - precision);
#endif  // CONFIG_FLEX_MVRES
  int params_cost = 0;
  int trans_bits, trans_prec_diff;
  switch (gm->wmtype) {
    case AFFINE:
    case ROTZOOM:
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS),
          (gm->wmmat[2] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          GM_ALPHA_MAX + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[3] >> GM_ALPHA_PREC_DIFF),
          (gm->wmmat[3] >> GM_ALPHA_PREC_DIFF));
      if (gm->wmtype >= AFFINE) {
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[4] >> GM_ALPHA_PREC_DIFF),
            (gm->wmmat[4] >> GM_ALPHA_PREC_DIFF));
        params_cost += aom_count_signed_primitive_refsubexpfin(
            GM_ALPHA_MAX + 1, SUBEXPFIN_K,
            (ref_gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                (1 << GM_ALPHA_PREC_BITS),
            (gm->wmmat[5] >> GM_ALPHA_PREC_DIFF) - (1 << GM_ALPHA_PREC_BITS));
      }
      AOM_FALLTHROUGH_INTENDED;
    case TRANSLATION:
      trans_bits = (gm->wmtype == TRANSLATION)
                       ? GM_ABS_TRANS_ONLY_BITS - precision_reduce
                       : GM_ABS_TRANS_BITS;
      trans_prec_diff = (gm->wmtype == TRANSLATION)
                            ? GM_TRANS_ONLY_PREC_DIFF + precision_reduce
                            : GM_TRANS_PREC_DIFF;
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[0] >> trans_prec_diff),
          (gm->wmmat[0] >> trans_prec_diff));
      params_cost += aom_count_signed_primitive_refsubexpfin(
          (1 << trans_bits) + 1, SUBEXPFIN_K,
          (ref_gm->wmmat[1] >> trans_prec_diff),
          (gm->wmmat[1] >> trans_prec_diff));
      AOM_FALLTHROUGH_INTENDED;
    case IDENTITY: break;
    default: assert(0);
  }
  return (params_cost << AV1_PROB_COST_SHIFT);
}

static int do_gm_search_logic(SPEED_FEATURES *const sf, int num_refs_using_gm,
                              int frame) {
  (void)num_refs_using_gm;
  (void)frame;
  switch (sf->gm_search_type) {
    case GM_FULL_SEARCH: return 1;
    case GM_REDUCED_REF_SEARCH_SKIP_L2_L3:
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME);
    case GM_REDUCED_REF_SEARCH_SKIP_L2_L3_ARF2:
      return !(frame == LAST2_FRAME || frame == LAST3_FRAME ||
               (frame == ALTREF2_FRAME));
    case GM_DISABLE_SEARCH: return 0;
    default: assert(0);
  }
  return 1;
}

// Set the relative distance of a reference frame w.r.t. current frame
static void set_rel_frame_dist(AV1_COMP *cpi) {
  const AV1_COMMON *const cm = &cpi->common;
  const SPEED_FEATURES *const sf = &cpi->sf;
  const OrderHintInfo *const order_hint_info = &cm->seq_params.order_hint_info;
  MV_REFERENCE_FRAME ref_frame;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    cpi->ref_relative_dist[ref_frame - LAST_FRAME] = 0;
    if (sf->alt_ref_search_fp) {
      int dist = av1_encoder_get_relative_dist(
          order_hint_info,
          cm->cur_frame->ref_display_order_hint[ref_frame - LAST_FRAME],
          cm->current_frame.display_order_hint);
      cpi->ref_relative_dist[ref_frame - LAST_FRAME] = dist;
    }
  }
}

// Enforce the number of references for each arbitrary frame based on user
// options and speed.
static void enforce_max_ref_frames(AV1_COMP *cpi) {
  MV_REFERENCE_FRAME ref_frame;
  int total_valid_refs = 0;
  for (ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ++ref_frame) {
    if (cpi->ref_frame_flags & av1_ref_frame_flag_list[ref_frame]) {
      total_valid_refs++;
    }
  }

  const int max_allowed_refs = get_max_allowed_ref_frames(cpi);

  for (int i = 0; i < 4 && total_valid_refs > max_allowed_refs; ++i) {
    const MV_REFERENCE_FRAME ref_frame_to_disable = disable_order[i];

    if (!(cpi->ref_frame_flags &
          av1_ref_frame_flag_list[ref_frame_to_disable])) {
      continue;
    }

    switch (ref_frame_to_disable) {
      case LAST3_FRAME: cpi->ref_frame_flags &= ~AOM_LAST3_FLAG; break;
      case LAST2_FRAME: cpi->ref_frame_flags &= ~AOM_LAST2_FLAG; break;
      case ALTREF2_FRAME: cpi->ref_frame_flags &= ~AOM_ALT2_FLAG; break;
      case GOLDEN_FRAME: cpi->ref_frame_flags &= ~AOM_GOLD_FLAG; break;
      default: assert(0);
    }
    --total_valid_refs;
  }
  assert(total_valid_refs <= max_allowed_refs);
}

static INLINE int av1_refs_are_one_sided(const AV1_COMMON *cm) {
  assert(!frame_is_intra_only(cm));

  int one_sided_refs = 1;
  for (int ref = LAST_FRAME; ref <= ALTREF_FRAME; ++ref) {
    const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref);
    if (buf == NULL) continue;

    const int ref_display_order_hint = buf->display_order_hint;
    if (av1_encoder_get_relative_dist(
            &cm->seq_params.order_hint_info, ref_display_order_hint,
            (int)cm->current_frame.display_order_hint) > 0) {
      one_sided_refs = 0;  // bwd reference
      break;
    }
  }
  return one_sided_refs;
}

static INLINE void get_skip_mode_ref_offsets(const AV1_COMMON *cm,
                                             int ref_order_hint[2]) {
  const SkipModeInfo *const skip_mode_info = &cm->current_frame.skip_mode_info;
  ref_order_hint[0] = ref_order_hint[1] = 0;
  if (!skip_mode_info->skip_mode_allowed) return;

  const RefCntBuffer *const buf_0 =
      get_ref_frame_buf(cm, LAST_FRAME + skip_mode_info->ref_frame_idx_0);
  const RefCntBuffer *const buf_1 =
      get_ref_frame_buf(cm, LAST_FRAME + skip_mode_info->ref_frame_idx_1);
  assert(buf_0 != NULL && buf_1 != NULL);

  ref_order_hint[0] = buf_0->order_hint;
  ref_order_hint[1] = buf_1->order_hint;
}

static int check_skip_mode_enabled(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;

  av1_setup_skip_mode_allowed(cm);
  if (!cm->current_frame.skip_mode_info.skip_mode_allowed) return 0;

  // Turn off skip mode if the temporal distances of the reference pair to the
  // current frame are different by more than 1 frame.
  const int cur_offset = (int)cm->current_frame.order_hint;
  int ref_offset[2];
  get_skip_mode_ref_offsets(cm, ref_offset);
  const int cur_to_ref0 = get_relative_dist(&cm->seq_params.order_hint_info,
                                            cur_offset, ref_offset[0]);
  const int cur_to_ref1 = abs(get_relative_dist(&cm->seq_params.order_hint_info,
                                                cur_offset, ref_offset[1]));
  if (abs(cur_to_ref0 - cur_to_ref1) > 1) return 0;

  // High Latency: Turn off skip mode if all refs are fwd.
  if (cpi->all_one_sided_refs && cpi->oxcf.lag_in_frames > 0) return 0;

  static const int flag_list[REF_FRAMES] = { 0,
                                             AOM_LAST_FLAG,
                                             AOM_LAST2_FLAG,
                                             AOM_LAST3_FLAG,
                                             AOM_GOLD_FLAG,
                                             AOM_BWD_FLAG,
                                             AOM_ALT2_FLAG,
                                             AOM_ALT_FLAG };
  const int ref_frame[2] = {
    cm->current_frame.skip_mode_info.ref_frame_idx_0 + LAST_FRAME,
    cm->current_frame.skip_mode_info.ref_frame_idx_1 + LAST_FRAME
  };
  if (!(cpi->ref_frame_flags & flag_list[ref_frame[0]]) ||
      !(cpi->ref_frame_flags & flag_list[ref_frame[1]]))
    return 0;

  return 1;
}

// Function to decide if we can skip the global motion parameter computation
// for a particular ref frame
static INLINE int skip_gm_frame(AV1_COMMON *const cm, int ref_frame) {
  if ((ref_frame == LAST3_FRAME || ref_frame == LAST2_FRAME) &&
      cm->global_motion[GOLDEN_FRAME].wmtype != IDENTITY) {
    return get_relative_dist(
               &cm->seq_params.order_hint_info,
               cm->cur_frame->ref_order_hints[ref_frame - LAST_FRAME],
               cm->cur_frame->ref_order_hints[GOLDEN_FRAME - LAST_FRAME]) <= 0;
  }
  return 0;
}

static void set_default_interp_skip_flags(AV1_COMP *cpi) {
  const int num_planes = av1_num_planes(&cpi->common);
  cpi->default_interp_skip_flags = (num_planes == 1)
                                       ? INTERP_SKIP_LUMA_EVAL_CHROMA
                                       : INTERP_SKIP_LUMA_SKIP_CHROMA;
}

static void encode_frame_internal(AV1_COMP *cpi) {
  ThreadData *const td = &cpi->td;
  MACROBLOCK *const x = &td->mb;
  AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  RD_COUNTS *const rdc = &cpi->td.rd_counts;
  int i;

  x->min_partition_size = AOMMIN(x->min_partition_size, cm->seq_params.sb_size);
  x->max_partition_size = AOMMIN(x->max_partition_size, cm->seq_params.sb_size);
#if CONFIG_DIST_8X8
  x->using_dist_8x8 = cpi->oxcf.using_dist_8x8;
  x->tune_metric = cpi->oxcf.tuning;
#endif

  if (!cpi->sf.use_nonrd_pick_mode) {
    cm->setup_mi(cm);
  }

  xd->mi = cm->mi_grid_base;
  xd->mi[0] = cm->mi;

  av1_zero(*td->counts);
  av1_zero(rdc->comp_pred_diff);

  // Reset the flag.
  cpi->intrabc_used = 0;
  // Need to disable intrabc when superres is selected
  if (av1_superres_scaled(cm)) {
    cm->allow_intrabc = 0;
  }

  cm->allow_intrabc &= (cpi->oxcf.enable_intrabc);

  if (!is_stat_generation_stage(cpi) && av1_use_hash_me(cm) &&
      !cpi->sf.use_nonrd_pick_mode) {
    // add to hash table
    const int pic_width = cpi->source->y_crop_width;
    const int pic_height = cpi->source->y_crop_height;
    uint32_t *block_hash_values[2][2];
    int8_t *is_block_same[2][3];
    int k, j;

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        CHECK_MEM_ERROR(cm, block_hash_values[k][j],
                        aom_malloc(sizeof(uint32_t) * pic_width * pic_height));
      }

      for (j = 0; j < 3; j++) {
        CHECK_MEM_ERROR(cm, is_block_same[k][j],
                        aom_malloc(sizeof(int8_t) * pic_width * pic_height));
      }
    }

#if CONFIG_DEBUG
    cm->cur_frame->hash_table.has_content++;
#endif
    av1_hash_table_create(&cm->cur_frame->hash_table);
    av1_generate_block_2x2_hash_value(cpi->source, block_hash_values[0],
                                      is_block_same[0], &cpi->td.mb);
    av1_generate_block_hash_value(cpi->source, 4, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1], &cpi->td.mb);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 4);
    av1_generate_block_hash_value(cpi->source, 8, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0], &cpi->td.mb);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 8);
    av1_generate_block_hash_value(cpi->source, 16, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1], &cpi->td.mb);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 16);
    av1_generate_block_hash_value(cpi->source, 32, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0], &cpi->td.mb);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 32);
    av1_generate_block_hash_value(cpi->source, 64, block_hash_values[0],
                                  block_hash_values[1], is_block_same[0],
                                  is_block_same[1], &cpi->td.mb);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[1], is_block_same[1][2],
        pic_width, pic_height, 64);

    av1_generate_block_hash_value(cpi->source, 128, block_hash_values[1],
                                  block_hash_values[0], is_block_same[1],
                                  is_block_same[0], &cpi->td.mb);
    av1_add_to_hash_map_by_row_with_precal_data(
        &cm->cur_frame->hash_table, block_hash_values[0], is_block_same[0][2],
        pic_width, pic_height, 128);

    for (k = 0; k < 2; k++) {
      for (j = 0; j < 2; j++) {
        aom_free(block_hash_values[k][j]);
      }

      for (j = 0; j < 3; j++) {
        aom_free(is_block_same[k][j]);
      }
    }
  }

  for (i = 0; i < MAX_SEGMENTS; ++i) {
    const int qindex = cm->seg.enabled
                           ? av1_get_qindex(&cm->seg, i, cm->base_qindex)
                           : cm->base_qindex;
    xd->lossless[i] = qindex == 0 && cm->y_dc_delta_q == 0 &&
                      cm->u_dc_delta_q == 0 && cm->u_ac_delta_q == 0 &&
                      cm->v_dc_delta_q == 0 && cm->v_ac_delta_q == 0;
    if (xd->lossless[i]) cpi->has_lossless_segment = 1;
    xd->qindex[i] = qindex;
    if (xd->lossless[i]) {
      cpi->optimize_seg_arr[i] = NO_TRELLIS_OPT;
    } else {
      cpi->optimize_seg_arr[i] = cpi->sf.optimize_coefficients;
    }
  }
  cm->coded_lossless = is_coded_lossless(cm, xd);
  cm->all_lossless = cm->coded_lossless && !av1_superres_scaled(cm);

  cm->tx_mode = select_tx_mode(cpi);

  // Fix delta q resolution for the moment
  cm->delta_q_info.delta_q_res = 0;
  if (cpi->oxcf.deltaq_mode == DELTA_Q_OBJECTIVE)
    cm->delta_q_info.delta_q_res = DEFAULT_DELTA_Q_RES_OBJECTIVE;
  else if (cpi->oxcf.deltaq_mode == DELTA_Q_PERCEPTUAL)
    cm->delta_q_info.delta_q_res = DEFAULT_DELTA_Q_RES_PERCEPTUAL;
  // Set delta_q_present_flag before it is used for the first time
  cm->delta_q_info.delta_lf_res = DEFAULT_DELTA_LF_RES;
  cm->delta_q_info.delta_q_present_flag = cpi->oxcf.deltaq_mode != NO_DELTA_Q;

  // Turn off cm->delta_q_info.delta_q_present_flag if objective delta_q is used
  // for ineligible frames. That effectively will turn off row_mt usage.
  // Note objective delta_q and tpl eligible frames are only altref frames
  // currently.
  if (cm->delta_q_info.delta_q_present_flag) {
    if (cpi->oxcf.deltaq_mode == DELTA_Q_OBJECTIVE &&
        !is_frame_tpl_eligible(cpi))
      cm->delta_q_info.delta_q_present_flag = 0;
  }

  // Reset delta_q_used flag
  cpi->deltaq_used = 0;

  cm->delta_q_info.delta_lf_present_flag =
      cm->delta_q_info.delta_q_present_flag && cpi->oxcf.deltalf_mode;
  cm->delta_q_info.delta_lf_multi = DEFAULT_DELTA_LF_MULTI;

  // update delta_q_present_flag and delta_lf_present_flag based on
  // base_qindex
  cm->delta_q_info.delta_q_present_flag &= cm->base_qindex > 0;
  cm->delta_q_info.delta_lf_present_flag &= cm->base_qindex > 0;

  av1_frame_init_quantizer(cpi);
  av1_initialize_rd_consts(cpi);
  av1_initialize_me_consts(cpi, x, cm->base_qindex);

  init_encode_frame_mb_context(cpi);
  set_default_interp_skip_flags(cpi);
  if (cm->prev_frame)
    cm->last_frame_seg_map = cm->prev_frame->seg_map;
  else
    cm->last_frame_seg_map = NULL;
  if (cm->allow_intrabc || cm->coded_lossless) {
    av1_set_default_ref_deltas(cm->lf.ref_deltas);
    av1_set_default_mode_deltas(cm->lf.mode_deltas);
  } else if (cm->prev_frame) {
    memcpy(cm->lf.ref_deltas, cm->prev_frame->ref_deltas, REF_FRAMES);
    memcpy(cm->lf.mode_deltas, cm->prev_frame->mode_deltas, MAX_MODE_LF_DELTAS);
  }
  memcpy(cm->cur_frame->ref_deltas, cm->lf.ref_deltas, REF_FRAMES);
  memcpy(cm->cur_frame->mode_deltas, cm->lf.mode_deltas, MAX_MODE_LF_DELTAS);

  x->txb_split_count = 0;
#if CONFIG_SPEED_STATS
  x->tx_search_count = 0;
#endif  // CONFIG_SPEED_STATS

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, av1_compute_global_motion_time);
#endif
  av1_zero(rdc->global_motion_used);
  av1_zero(cpi->gmparams_cost);
#if CONFIG_FLEX_MVRES
  av1_zero(rdc->reduced_mv_precision_used);
#endif  // CONFIG_FLEX_MVRES
  if (cpi->common.current_frame.frame_type == INTER_FRAME && cpi->source &&
      cpi->oxcf.enable_global_motion && !cpi->global_motion_search_done) {
    YV12_BUFFER_CONFIG *ref_buf[REF_FRAMES];
    int frame;
    MotionModel params_by_motion[RANSAC_NUM_MOTIONS];
    for (int m = 0; m < RANSAC_NUM_MOTIONS; m++) {
      memset(&params_by_motion[m], 0, sizeof(params_by_motion[m]));
      params_by_motion[m].inliers =
          aom_malloc(sizeof(*(params_by_motion[m].inliers)) * 2 * MAX_CORNERS);
    }

    const double *params_this_motion;
    int inliers_by_motion[RANSAC_NUM_MOTIONS];
    WarpedMotionParams tmp_wm_params;
    // clang-format off
    static const double kIdentityParams[MAX_PARAMDIM - 1] = {
      0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0
    };
    // clang-format on
    int num_refs_using_gm = 0;
    int num_frm_corners = -1;
    int frm_corners[2 * MAX_CORNERS];
    unsigned char *frm_buffer = cpi->source->y_buffer;
    if (cpi->source->flags & YV12_FLAG_HIGHBITDEPTH) {
      // The frame buffer is 16-bit, so we need to convert to 8 bits for the
      // following code. We cache the result until the frame is released.
      frm_buffer =
          av1_downconvert_frame(cpi->source, cpi->common.seq_params.bit_depth);
    }
    const int segment_map_w =
        (cpi->source->y_width + WARP_ERROR_BLOCK) >> WARP_ERROR_BLOCK_LOG;
    const int segment_map_h =
        (cpi->source->y_height + WARP_ERROR_BLOCK) >> WARP_ERROR_BLOCK_LOG;

    uint8_t *segment_map =
        aom_malloc(sizeof(*segment_map) * segment_map_w * segment_map_h);
    memset(segment_map, 0,
           sizeof(*segment_map) * segment_map_w * segment_map_h);

    for (frame = ALTREF_FRAME; frame >= LAST_FRAME; --frame) {
      const MV_REFERENCE_FRAME ref_frame[2] = { frame, NONE_FRAME };
      ref_buf[frame] = NULL;
      RefCntBuffer *buf = get_ref_frame_buf(cm, frame);
      if (buf != NULL) ref_buf[frame] = &buf->buf;
      int pframe;
      cm->global_motion[frame] = default_warp_params;
      const WarpedMotionParams *ref_params =
          cm->prev_frame ? &cm->prev_frame->global_motion[frame]
                         : &default_warp_params;
      // check for duplicate buffer
      for (pframe = ALTREF_FRAME; pframe > frame; --pframe) {
        if (ref_buf[frame] == ref_buf[pframe]) break;
      }
      if (pframe > frame) {
        memcpy(&cm->global_motion[frame], &cm->global_motion[pframe],
               sizeof(WarpedMotionParams));
      } else if (ref_buf[frame] &&
                 ref_buf[frame]->y_crop_width == cpi->source->y_crop_width &&
                 ref_buf[frame]->y_crop_height == cpi->source->y_crop_height &&
                 do_gm_search_logic(&cpi->sf, num_refs_using_gm, frame) &&
                 !prune_ref_by_selective_ref_frame(
                     cpi, ref_frame, cm->cur_frame->ref_display_order_hint,
                     cm->current_frame.display_order_hint) &&
                 !(cpi->sf.selective_ref_gm && skip_gm_frame(cm, frame))) {
        if (num_frm_corners < 0) {
          // compute interest points using FAST features
          num_frm_corners = av1_fast_corner_detect(
              frm_buffer, cpi->source->y_width, cpi->source->y_height,
              cpi->source->y_stride, frm_corners, MAX_CORNERS);
        }
        TransformationType model;

        aom_clear_system_state();

        // TODO(sarahparker, debargha): Explore do_adaptive_gm_estimation = 1
        const int do_adaptive_gm_estimation = 0;

        const int ref_frame_dist = get_relative_dist(
            &cm->seq_params.order_hint_info, cm->current_frame.order_hint,
            cm->cur_frame->ref_order_hints[frame - LAST_FRAME]);
        const GlobalMotionEstimationType gm_estimation_type =
            cm->seq_params.order_hint_info.enable_order_hint &&
                    abs(ref_frame_dist) <= 2 && do_adaptive_gm_estimation
                ? GLOBAL_MOTION_DISFLOW_BASED
                : GLOBAL_MOTION_FEATURE_BASED;
        for (model = ROTZOOM; model < GLOBAL_TRANS_TYPES_ENC; ++model) {
          int64_t best_warp_error = INT64_MAX;
          // Initially set all params to identity.
          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            memcpy(params_by_motion[i].params, kIdentityParams,
                   (MAX_PARAMDIM - 1) * sizeof(*(params_by_motion[i].params)));
          }

          av1_compute_global_motion(
              model, frm_buffer, cpi->source->y_width, cpi->source->y_height,
              cpi->source->y_stride, frm_corners, num_frm_corners,
              ref_buf[frame], cpi->common.seq_params.bit_depth,
              gm_estimation_type, inliers_by_motion, params_by_motion,
              RANSAC_NUM_MOTIONS);

          for (i = 0; i < RANSAC_NUM_MOTIONS; ++i) {
            if (inliers_by_motion[i] == 0) continue;

            params_this_motion = params_by_motion[i].params;
            av1_convert_model_to_params(params_this_motion, &tmp_wm_params);

            if (tmp_wm_params.wmtype != IDENTITY) {
              av1_compute_feature_segmentation_map(
                  segment_map, segment_map_w, segment_map_h,
                  params_by_motion[i].inliers, params_by_motion[i].num_inliers);

              const int64_t warp_error = av1_refine_integerized_param(
                  &tmp_wm_params, tmp_wm_params.wmtype, is_cur_buf_hbd(xd),
                  xd->bd, ref_buf[frame]->y_buffer, ref_buf[frame]->y_width,
                  ref_buf[frame]->y_height, ref_buf[frame]->y_stride,
                  cpi->source->y_buffer, cpi->source->y_width,
                  cpi->source->y_height, cpi->source->y_stride, 5,
                  best_warp_error, segment_map, segment_map_w);
              if (warp_error < best_warp_error) {
                best_warp_error = warp_error;
                // Save the wm_params modified by
                // av1_refine_integerized_param() rather than motion index to
                // avoid rerunning refine() below.
                memcpy(&(cm->global_motion[frame]), &tmp_wm_params,
                       sizeof(WarpedMotionParams));
              }
            }
          }
          if (cm->global_motion[frame].wmtype <= AFFINE)
            if (!av1_get_shear_params(&cm->global_motion[frame]))
              cm->global_motion[frame] = default_warp_params;

          if (cm->global_motion[frame].wmtype == TRANSLATION) {
            cm->global_motion[frame].wmmat[0] =
                convert_to_trans_prec(cm->fr_mv_precision,
                                      cm->global_motion[frame].wmmat[0]) *
                GM_TRANS_ONLY_DECODE_FACTOR;
            cm->global_motion[frame].wmmat[1] =
                convert_to_trans_prec(cm->fr_mv_precision,
                                      cm->global_motion[frame].wmmat[1]) *
                GM_TRANS_ONLY_DECODE_FACTOR;
          }

          if (cm->global_motion[frame].wmtype == IDENTITY) continue;

          const int64_t ref_frame_error = av1_segmented_frame_error(
              is_cur_buf_hbd(xd), xd->bd, ref_buf[frame]->y_buffer,
              ref_buf[frame]->y_stride, cpi->source->y_buffer,
              cpi->source->y_width, cpi->source->y_height,
              cpi->source->y_stride, segment_map, segment_map_w);

          if (ref_frame_error == 0) continue;

          // If the best error advantage found doesn't meet the threshold for
          // this motion type, revert to IDENTITY.
          if (!av1_is_enough_erroradvantage(
                  (double)best_warp_error / ref_frame_error,
                  gm_get_params_cost(&cm->global_motion[frame], ref_params,
                                     cm->fr_mv_precision),
                  cpi->sf.gm_erroradv_type)) {
            cm->global_motion[frame] = default_warp_params;
          }
          if (cm->global_motion[frame].wmtype != IDENTITY) break;
        }
        aom_clear_system_state();
      }
      if (cm->global_motion[frame].wmtype != IDENTITY) num_refs_using_gm++;
      cpi->gmparams_cost[frame] =
          gm_get_params_cost(&cm->global_motion[frame], ref_params,
                             cm->fr_mv_precision) +
          cpi->gmtype_cost[cm->global_motion[frame].wmtype] -
          cpi->gmtype_cost[IDENTITY];
    }
    aom_free(segment_map);
    // clear disabled ref_frames
    for (frame = LAST_FRAME; frame <= ALTREF_FRAME; ++frame) {
      const int ref_disabled =
          !(cpi->ref_frame_flags & av1_ref_frame_flag_list[frame]);
      if (ref_disabled && cpi->sf.recode_loop != DISALLOW_RECODE) {
        cpi->gmparams_cost[frame] = 0;
        cm->global_motion[frame] = default_warp_params;
      }
    }
    cpi->global_motion_search_done = 1;
    for (int m = 0; m < RANSAC_NUM_MOTIONS; m++) {
      aom_free(params_by_motion[m].inliers);
    }
  }
  memcpy(cm->cur_frame->global_motion, cm->global_motion,
         REF_FRAMES * sizeof(WarpedMotionParams));
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, av1_compute_global_motion_time);
#endif

#if CONFIG_COLLECT_COMPONENT_TIMING
  start_timing(cpi, av1_setup_motion_field_time);
#endif
  if (cm->allow_ref_frame_mvs) av1_setup_motion_field(cm);
#if CONFIG_COLLECT_COMPONENT_TIMING
  end_timing(cpi, av1_setup_motion_field_time);
#endif

  cpi->all_one_sided_refs =
      frame_is_intra_only(cm) ? 0 : av1_refs_are_one_sided(cm);

  cm->current_frame.skip_mode_info.skip_mode_flag =
      check_skip_mode_enabled(cpi);

  cpi->row_mt_sync_read_ptr = av1_row_mt_sync_read_dummy;
  cpi->row_mt_sync_write_ptr = av1_row_mt_sync_write_dummy;
  cpi->row_mt = 0;

  if (cpi->oxcf.row_mt && (cpi->oxcf.max_threads > 1)) {
    cpi->row_mt = 1;
    cpi->row_mt_sync_read_ptr = av1_row_mt_sync_read;
    cpi->row_mt_sync_write_ptr = av1_row_mt_sync_write;
    av1_encode_tiles_row_mt(cpi);
  } else {
    if (AOMMIN(cpi->oxcf.max_threads, cm->tile_cols * cm->tile_rows) > 1)
      av1_encode_tiles_mt(cpi);
    else
      encode_tiles(cpi);
  }

  // If intrabc is allowed but never selected, reset the allow_intrabc flag.
  if (cm->allow_intrabc && !cpi->intrabc_used) cm->allow_intrabc = 0;
  if (cm->allow_intrabc) cm->delta_q_info.delta_lf_present_flag = 0;

  if (cm->delta_q_info.delta_q_present_flag && cpi->deltaq_used == 0) {
    cm->delta_q_info.delta_q_present_flag = 0;
  }
}

#define CHECK_PRECOMPUTED_REF_FRAME_MAP 0

void av1_encode_frame(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;
  CurrentFrame *const current_frame = &cm->current_frame;
  const int num_planes = av1_num_planes(cm);
  // Indicates whether or not to use a default reduced set for ext-tx
  // rather than the potential full set of 16 transforms
  cm->reduced_tx_set_used = cpi->oxcf.reduced_tx_type_set;

  // Make sure segment_id is no larger than last_active_segid.
  if (cm->seg.enabled && cm->seg.update_map) {
    const int mi_rows = cm->mi_rows;
    const int mi_cols = cm->mi_cols;
    const int last_active_segid = cm->seg.last_active_segid;
    uint8_t *map = cpi->segmentation_map;
    for (int mi_row = 0; mi_row < mi_rows; ++mi_row) {
      for (int mi_col = 0; mi_col < mi_cols; ++mi_col) {
        map[mi_col] = AOMMIN(map[mi_col], last_active_segid);
      }
      map += mi_cols;
    }
  }

  av1_setup_frame_buf_refs(cm);
  enforce_max_ref_frames(cpi);
  set_rel_frame_dist(cpi);
  av1_setup_frame_sign_bias(cm);

#if CHECK_PRECOMPUTED_REF_FRAME_MAP
  GF_GROUP *gf_group = &cpi->gf_group;
  // TODO(yuec): The check is disabled on OVERLAY frames for now, because info
  // in cpi->gf_group has been refreshed for the next GOP when the check is
  // performed for OVERLAY frames. Since we have not support inter-GOP ref
  // frame map computation, the precomputed ref map for an OVERLAY frame is all
  // -1 at this point (although it is meaning before gf_group is refreshed).
  if (!frame_is_intra_only(cm) && gf_group->index != 0) {
    const RefCntBuffer *const golden_buf = get_ref_frame_buf(cm, GOLDEN_FRAME);

    if (golden_buf) {
      const int golden_order_hint = golden_buf->order_hint;

      for (int ref = LAST_FRAME; ref < EXTREF_FRAME; ++ref) {
        const RefCntBuffer *const buf = get_ref_frame_buf(cm, ref);
        const int ref_disp_idx_precomputed =
            gf_group->ref_frame_disp_idx[gf_group->index][ref - LAST_FRAME];

        (void)ref_disp_idx_precomputed;

        if (buf != NULL) {
          const int ref_disp_idx =
              get_relative_dist(&cm->seq_params.order_hint_info,
                                buf->order_hint, golden_order_hint);

          if (ref_disp_idx >= 0)
            assert(ref_disp_idx == ref_disp_idx_precomputed);
          else
            assert(ref_disp_idx_precomputed == -1);
        } else {
          assert(ref_disp_idx_precomputed == -1);
        }
      }
    }
  }
#endif

#if CONFIG_MISMATCH_DEBUG
  mismatch_reset_frame(num_planes);
#else
  (void)num_planes;
#endif

  if (cpi->sf.frame_parameter_update) {
    int i;
    RD_OPT *const rd_opt = &cpi->rd;
    RD_COUNTS *const rdc = &cpi->td.rd_counts;

    // This code does a single RD pass over the whole frame assuming
    // either compound, single or hybrid prediction as per whatever has
    // worked best for that type of frame in the past.
    // It also predicts whether another coding mode would have worked
    // better than this coding mode. If that is the case, it remembers
    // that for subsequent frames.
    // It does the same analysis for transform size selection also.
    //
    // TODO(zoeliu): To investigate whether a frame_type other than
    // INTRA/ALTREF/GOLDEN/LAST needs to be specified seperately.
    const MV_REFERENCE_FRAME frame_type = get_frame_type(cpi);
    int64_t *const mode_thrs = rd_opt->prediction_type_threshes[frame_type];
    const int is_alt_ref = frame_type == ALTREF_FRAME;

    /* prediction (compound, single or hybrid) mode selection */
    // NOTE: "is_alt_ref" is true only for OVERLAY/INTNL_OVERLAY frames
    int use_single_ref_only = is_alt_ref || frame_is_intra_only(cm);
#if CONFIG_MISC_CHANGES
    use_single_ref_only = use_single_ref_only || cm->only_one_ref_available;
#endif  // CONFIG_MISC_CHANGES
    if (use_single_ref_only) {
      current_frame->reference_mode = SINGLE_REFERENCE;
    } else {
      current_frame->reference_mode = REFERENCE_MODE_SELECT;
    }

    cm->interp_filter = SWITCHABLE;
    if (cm->large_scale_tile) cm->interp_filter = EIGHTTAP_REGULAR;

    cm->switchable_motion_mode = 1;

    rdc->compound_ref_used_flag = 0;
    rdc->skip_mode_used_flag = 0;

    encode_frame_internal(cpi);

    for (i = 0; i < REFERENCE_MODES; ++i)
      mode_thrs[i] = (mode_thrs[i] + rdc->comp_pred_diff[i] / cm->MBs) / 2;

    if (current_frame->reference_mode == REFERENCE_MODE_SELECT) {
      // Use a flag that includes 4x4 blocks
      if (rdc->compound_ref_used_flag == 0) {
        current_frame->reference_mode = SINGLE_REFERENCE;
#if CONFIG_ENTROPY_STATS
        av1_zero(cpi->td.counts->comp_inter);
#endif  // CONFIG_ENTROPY_STATS
      }
    }
    // Re-check on the skip mode status as reference mode may have been
    // changed.
    SkipModeInfo *const skip_mode_info = &current_frame->skip_mode_info;
    if (frame_is_intra_only(cm) ||
        current_frame->reference_mode == SINGLE_REFERENCE) {
      skip_mode_info->skip_mode_allowed = 0;
      skip_mode_info->skip_mode_flag = 0;
    }
    if (skip_mode_info->skip_mode_flag && rdc->skip_mode_used_flag == 0)
      skip_mode_info->skip_mode_flag = 0;

    if (!cm->large_scale_tile) {
      if (cm->tx_mode == TX_MODE_SELECT && cpi->td.mb.txb_split_count == 0)
        cm->tx_mode = TX_MODE_LARGEST;
    }
  } else {
    encode_frame_internal(cpi);
  }
}

static void update_txfm_count(MACROBLOCK *x, MACROBLOCKD *xd,
                              FRAME_COUNTS *counts, TX_SIZE tx_size, int depth,
                              int blk_row, int blk_col,
                              uint8_t allow_update_cdf) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  int ctx = txfm_partition_context(xd->above_txfm_context + blk_col,
                                   xd->left_txfm_context + blk_row,
                                   mbmi->sb_type, tx_size);
  const int txb_size_index = av1_get_txb_size_index(bsize, blk_row, blk_col);

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;
  assert(tx_size > TX_4X4);
#if CONFIG_NEW_TX_PARTITION
  (void)depth;
  (void)counts;
  TX_SIZE sub_txs[MAX_TX_PARTITIONS] = { 0 };
  get_tx_partition_sizes(mbmi->partition_type[txb_size_index], tx_size,
                         sub_txs);
  // TODO(sarahparker) This assumes all of the tx sizes in the partition scheme
  // are the same size. This will need to be adjusted to deal with the case
  // where they can be different.
  TX_SIZE this_size = sub_txs[0];
  assert(mbmi->inter_tx_size[txb_size_index] == this_size);
  if (mbmi->partition_type[txb_size_index] != TX_PARTITION_NONE)
    ++x->txb_split_count;

  const int is_rect = is_rect_tx(tx_size);
#if CONFIG_ENTROPY_STATS
  ++counts->txfm_partition[is_rect][ctx][mbmi->partition_type[txb_size_index]];
#endif  // CONFIG_ENTROPY_STATS
  if (allow_update_cdf)
    update_cdf(xd->tile_ctx->txfm_partition_cdf[is_rect][ctx],
               mbmi->partition_type[txb_size_index], TX_PARTITION_TYPES);

  mbmi->tx_size = this_size;
  txfm_partition_update(xd->above_txfm_context + blk_col,
                        xd->left_txfm_context + blk_row, this_size, tx_size);
#else  // CONFIG_NEW_TX_PARTITION
  if (depth == MAX_VARTX_DEPTH) {
    // Don't add to counts in this case
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
    return;
  }

  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[txb_size_index];
  if (tx_size == plane_tx_size) {
#if CONFIG_ENTROPY_STATS
    ++counts->txfm_partition[ctx][0];
#endif
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 0, 2);
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);
  } else {
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];

#if CONFIG_ENTROPY_STATS
    ++counts->txfm_partition[ctx][1];
#endif
    if (allow_update_cdf)
      update_cdf(xd->tile_ctx->txfm_partition_cdf[ctx], 1, 2);
    ++x->txb_split_count;

    if (sub_txs == TX_4X4) {
      mbmi->inter_tx_size[txb_size_index] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }

    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        int offsetr = row;
        int offsetc = col;

        update_txfm_count(x, xd, counts, sub_txs, depth + 1, blk_row + offsetr,
                          blk_col + offsetc, allow_update_cdf);
      }
    }
  }
#endif  // CONFIG_NEW_TX_PARTITION
}

static void tx_partition_count_update(const AV1_COMMON *const cm, MACROBLOCK *x,
                                      BLOCK_SIZE plane_bsize, int mi_row,
                                      int mi_col, FRAME_COUNTS *td_counts,
                                      uint8_t allow_update_cdf) {
  MACROBLOCKD *xd = &x->e_mbd;
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context = cm->above_txfm_context[xd->tile.tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      update_txfm_count(x, xd, td_counts, max_tx_size, 0, idy, idx,
                        allow_update_cdf);
}

static void set_txfm_context(MACROBLOCKD *xd, TX_SIZE tx_size, int blk_row,
                             int blk_col) {
  MB_MODE_INFO *mbmi = xd->mi[0];
  const BLOCK_SIZE bsize = mbmi->sb_type;
  const int max_blocks_high = max_block_high(xd, bsize, 0);
  const int max_blocks_wide = max_block_wide(xd, bsize, 0);
  const int txb_size_index = av1_get_txb_size_index(bsize, blk_row, blk_col);
  const TX_SIZE plane_tx_size = mbmi->inter_tx_size[txb_size_index];

  if (blk_row >= max_blocks_high || blk_col >= max_blocks_wide) return;

  if (tx_size == plane_tx_size) {
    mbmi->tx_size = tx_size;
    txfm_partition_update(xd->above_txfm_context + blk_col,
                          xd->left_txfm_context + blk_row, tx_size, tx_size);

  } else {
#if CONFIG_NEW_TX_PARTITION
    TX_SIZE sub_txs[MAX_TX_PARTITIONS] = { 0 };
    const int index = av1_get_txb_size_index(bsize, blk_row, blk_col);
    get_tx_partition_sizes(mbmi->partition_type[index], tx_size, sub_txs);
    int cur_partition = 0;
    int bsw = 0, bsh = 0;
    for (int r = 0; r < tx_size_high_unit[tx_size]; r += bsh) {
      for (int c = 0; c < tx_size_wide_unit[tx_size]; c += bsw) {
        const TX_SIZE sub_tx = sub_txs[cur_partition];
        bsw = tx_size_wide_unit[sub_tx];
        bsh = tx_size_high_unit[sub_tx];
        const int offsetr = blk_row + r;
        const int offsetc = blk_col + c;
        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;
        mbmi->tx_size = sub_tx;
        txfm_partition_update(xd->above_txfm_context + blk_col,
                              xd->left_txfm_context + blk_row, sub_tx, sub_tx);
        cur_partition++;
      }
    }
#else
    if (tx_size == TX_8X8) {
      mbmi->inter_tx_size[txb_size_index] = TX_4X4;
      mbmi->tx_size = TX_4X4;
      txfm_partition_update(xd->above_txfm_context + blk_col,
                            xd->left_txfm_context + blk_row, TX_4X4, tx_size);
      return;
    }
    const TX_SIZE sub_txs = sub_tx_size_map[tx_size];
    const int bsw = tx_size_wide_unit[sub_txs];
    const int bsh = tx_size_high_unit[sub_txs];
    for (int row = 0; row < tx_size_high_unit[tx_size]; row += bsh) {
      for (int col = 0; col < tx_size_wide_unit[tx_size]; col += bsw) {
        const int offsetr = blk_row + row;
        const int offsetc = blk_col + col;
        if (offsetr >= max_blocks_high || offsetc >= max_blocks_wide) continue;
        set_txfm_context(xd, sub_txs, offsetr, offsetc);
      }
    }
#endif  // CONFIG_NEW_TX_PARTITION
  }
}

static void tx_partition_set_contexts(const AV1_COMMON *const cm,
                                      MACROBLOCKD *xd, BLOCK_SIZE plane_bsize,
                                      int mi_row, int mi_col) {
  const int mi_width = block_size_wide[plane_bsize] >> tx_size_wide_log2[0];
  const int mi_height = block_size_high[plane_bsize] >> tx_size_high_log2[0];
  const TX_SIZE max_tx_size = get_vartx_max_txsize(xd, plane_bsize, 0);
  const int bh = tx_size_high_unit[max_tx_size];
  const int bw = tx_size_wide_unit[max_tx_size];
  int idx, idy;

  xd->above_txfm_context = cm->above_txfm_context[xd->tile.tile_row] + mi_col;
  xd->left_txfm_context =
      xd->left_txfm_context_buffer + (mi_row & MAX_MIB_MASK);

  for (idy = 0; idy < mi_height; idy += bh)
    for (idx = 0; idx < mi_width; idx += bw)
      set_txfm_context(xd, max_tx_size, idy, idx);
}

static void encode_superblock(const AV1_COMP *const cpi, TileDataEnc *tile_data,
                              ThreadData *td, TOKENEXTRA **t, RUN_TYPE dry_run,
                              BLOCK_SIZE bsize, int *rate) {
  const AV1_COMMON *const cm = &cpi->common;
  const int num_planes = av1_num_planes(cm);
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO **mi_4x4 = xd->mi;
  MB_MODE_INFO *mbmi = mi_4x4[0];
  const int seg_skip =
      segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  const int mis = cm->mi_stride;
  const int mi_width = mi_size_wide[bsize];
  const int mi_height = mi_size_high[bsize];
  const int is_inter = is_inter_block(mbmi);
  const int mi_row = xd->mi_row;
  const int mi_col = xd->mi_col;

  if (!is_inter) {
#if CONFIG_DERIVED_INTRA_MODE
    if (mbmi->use_derived_intra_mode[0] || mbmi->use_derived_intra_mode[1]) {
      uint8_t derived_angle;
      const int derived_mode =
          av1_get_derived_intra_mode(xd, bsize, &derived_angle);
      if (mbmi->use_derived_intra_mode[0]) {
        mbmi->mode = derived_mode;
        mbmi->derived_angle = derived_angle;
      }
      if (mbmi->use_derived_intra_mode[1]) {
        mbmi->uv_mode = derived_mode;
        mbmi->derived_angle = derived_angle;
      }
    }
#endif  // CONFIG_DERIVED_INTRA_MODE
    xd->cfl.is_chroma_reference = mbmi->chroma_ref_info.is_chroma_ref;
    xd->cfl.store_y = store_cfl_required(cm, xd);
    mbmi->skip = 1;
    for (int plane = 0; plane < num_planes; ++plane) {
      av1_encode_intra_block_plane(cpi, x, bsize, plane, dry_run,
                                   cpi->optimize_seg_arr[mbmi->segment_id]);
    }

    // If there is at least one lossless segment, force the skip for intra
    // block to be 0, in order to avoid the segment_id to be changed by in
    // write_segment_id().
    if (!cpi->common.seg.segid_preskip && cpi->common.seg.update_map &&
        cpi->has_lossless_segment)
      mbmi->skip = 0;

    xd->cfl.store_y = 0;
    if (av1_allow_palette(cm->allow_screen_content_tools, bsize)) {
      for (int plane = 0; plane < AOMMIN(2, num_planes); ++plane) {
        if (mbmi->palette_mode_info.palette_size[plane] > 0) {
          if (!dry_run) {
            av1_tokenize_color_map(x, plane, t, bsize, mbmi->tx_size,
                                   PALETTE_MAP, tile_data->allow_update_cdf,
                                   td->counts);
          } else if (dry_run == DRY_RUN_COSTCOEFFS) {
            rate +=
                av1_cost_color_map(x, plane, bsize, mbmi->tx_size, PALETTE_MAP);
          }
        }
      }
    }

    av1_update_txb_context(cpi, td, dry_run, bsize, rate, mi_row, mi_col,
                           tile_data->allow_update_cdf);
  } else {
    int ref;
    const int is_compound = has_second_ref(mbmi);

    set_ref_ptrs(cm, xd, mbmi->ref_frame[0], mbmi->ref_frame[1]);
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      const YV12_BUFFER_CONFIG *cfg =
          get_ref_frame_yv12_buf(cm, mbmi->ref_frame[ref]);
      assert(IMPLIES(!is_intrabc_block(mbmi), cfg));
      av1_setup_pre_planes(xd, ref, cfg, mi_row, mi_col,
                           xd->block_ref_scale_factors[ref], num_planes,
                           &mbmi->chroma_ref_info);
    }
    assert(av1_check_newmv_joint_nonzero(cm, x));
    av1_enc_build_inter_predictor(cm, xd, mi_row, mi_col, NULL, bsize, 0,
                                  av1_num_planes(cm) - 1);
    if (mbmi->motion_mode == OBMC_CAUSAL) {
      assert(cpi->oxcf.enable_obmc == 1);
      av1_build_obmc_inter_predictors_sb(cm, xd);
    }

#if CONFIG_MISMATCH_DEBUG
    if (dry_run == OUTPUT_ENABLED) {
      for (int plane = 0; plane < num_planes; ++plane) {
        const struct macroblockd_plane *pd = &xd->plane[plane];
        int pixel_c, pixel_r;
        mi_to_pixel_loc(&pixel_c, &pixel_r, mi_col, mi_row, 0, 0,
                        pd->subsampling_x, pd->subsampling_y);
        if (plane && !mbmi->chroma_ref_info.is_chroma_ref) continue;
        mismatch_record_block_pre(pd->dst.buf, pd->dst.stride,
                                  cm->current_frame.order_hint, plane, pixel_c,
                                  pixel_r, pd->width, pd->height,
                                  xd->cur_buf->flags & YV12_FLAG_HIGHBITDEPTH);
      }
    }
#else
    (void)num_planes;
#endif

    av1_encode_sb(cpi, x, mi_row, mi_col, dry_run);
    av1_tokenize_sb_tx_size(cpi, td, t, dry_run, mi_row, mi_col, bsize, rate,
                            tile_data->allow_update_cdf);
  }

#if CONFIG_INTRA_ENTROPY && !CONFIG_USE_SMALL_MODEL
  if (frame_is_intra_only(cm)) {
    av1_get_gradient_hist(xd, mbmi, bsize);
    av1_get_recon_var(xd, mbmi, bsize);
  }
#endif  // CONFIG_INTRA_ENTROPY

  if (!dry_run) {
    if (av1_allow_intrabc(cm) && is_intrabc_block(mbmi)) td->intrabc_used = 1;
    if (cm->tx_mode == TX_MODE_SELECT && !xd->lossless[mbmi->segment_id] &&
        mbmi->sb_type > BLOCK_4X4 && !(is_inter && (mbmi->skip || seg_skip))) {
      if (is_inter) {
        tx_partition_count_update(cm, x, bsize, mi_row, mi_col, td->counts,
                                  tile_data->allow_update_cdf);
      } else {
        const TX_SIZE max_tx_size = max_txsize_rect_lookup[bsize];
        if (mbmi->tx_size != max_tx_size) ++x->txb_split_count;
        if (block_signals_txsize(bsize)) {
          const int tx_size_ctx = get_tx_size_context(xd);
#if CONFIG_NEW_TX_PARTITION
          const int is_rect = is_rect_tx(max_tx_size);
          if (tile_data->allow_update_cdf)
            update_cdf(xd->tile_ctx->tx_size_cdf[is_rect][tx_size_ctx],
                       mbmi->partition_type[0], TX_PARTITION_TYPES_INTRA);
#if CONFIG_ENTROPY_STATS
          ++td->counts
                ->intra_tx_size[is_rect][tx_size_ctx][mbmi->partition_type[0]];
#endif
#else  // CONFIG_NEW_TX_PARTITION
          const int32_t tx_size_cat = bsize_to_tx_size_cat(bsize);
          const int depth = tx_size_to_depth(mbmi->tx_size, bsize);
          const int max_depths = bsize_to_max_depth(bsize);

          if (tile_data->allow_update_cdf)
            update_cdf(xd->tile_ctx->tx_size_cdf[tx_size_cat][tx_size_ctx],
                       depth, max_depths + 1);
#if CONFIG_ENTROPY_STATS
          ++td->counts->intra_tx_size[tx_size_cat][tx_size_ctx][depth];
#endif
#endif  // CONFIG_NEW_TX_PARTITION
        }
      }
      assert(IMPLIES(is_rect_tx(mbmi->tx_size), is_rect_tx_allowed(xd, mbmi)));
    } else {
      int i, j;
      TX_SIZE intra_tx_size;
      // The new intra coding scheme requires no change of transform size
      if (is_inter) {
        if (xd->lossless[mbmi->segment_id]) {
          intra_tx_size = TX_4X4;
        } else {
          intra_tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode);
        }
      } else {
        intra_tx_size = mbmi->tx_size;
      }

      for (j = 0; j < mi_height; j++)
        for (i = 0; i < mi_width; i++)
          if (mi_col + i < cm->mi_cols && mi_row + j < cm->mi_rows)
            mi_4x4[mis * j + i]->tx_size = intra_tx_size;

      if (intra_tx_size != max_txsize_rect_lookup[bsize]) ++x->txb_split_count;
    }
  }

  if (cm->tx_mode == TX_MODE_SELECT && block_signals_txsize(mbmi->sb_type) &&
      is_inter && !(mbmi->skip || seg_skip) &&
      !xd->lossless[mbmi->segment_id]) {
    if (dry_run) tx_partition_set_contexts(cm, xd, bsize, mi_row, mi_col);
  } else {
    TX_SIZE tx_size = mbmi->tx_size;
    // The new intra coding scheme requires no change of transform size
    if (is_inter) {
      if (xd->lossless[mbmi->segment_id]) {
        tx_size = TX_4X4;
      } else {
        tx_size = tx_size_from_tx_mode(bsize, cm->tx_mode);
      }
    } else {
      tx_size = (bsize > BLOCK_4X4) ? tx_size : TX_4X4;
    }
    mbmi->tx_size = tx_size;
    set_txfm_ctxs(tx_size, xd->n4_w, xd->n4_h,
                  (mbmi->skip || seg_skip) && is_inter_block(mbmi), xd);
  }
  if (is_inter_block(mbmi) && !mbmi->chroma_ref_info.is_chroma_ref &&
      is_cfl_allowed(xd)) {
    cfl_store_block(xd, mbmi->sb_type, mbmi->tx_size);
  }

  av1_mark_block_as_coded(xd, mi_row, mi_col, bsize, cm->seq_params.sb_size);
}
