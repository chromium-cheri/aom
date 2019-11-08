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

#include <assert.h>
#include <stdio.h>
#include <limits.h>

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"
#include "config/aom_scale_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"

#include "av1/common/blockd.h"
#include "av1/common/mvref_common.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/obmc.h"

#define USE_PRECOMPUTED_WEDGE_MASK 1
#define USE_PRECOMPUTED_WEDGE_SIGN 1

#if CONFIG_CTX_ADAPT_LOG_WEIGHT
#define LOG_K_SIZE 1
#define LOG_WEIGHT_0 43
#define LOG_WEIGHT_1 40
#define DIFFLOG_THR 3
static const double log_k[9] = { 0.1004,  -0.0234, 0.1004,  -0.0234, -0.3079,
                                 -0.0234, 0.1004,  -0.0234, 0.1004 };

double kernel_correlation(const CONV_BUF_TYPE *src, int stride, int h, int w,
                          int cr, int cc, const double *kernel, int ws,
                          ConvolveParams *conv_params) {
  double crr = 0;
  const int ks = 2 * ws + 1;
  const int bd = 8;
  const int offset_bits = bd + 2 * FILTER_BITS - conv_params->round_0;
  const int round_offset = (1 << (offset_bits - conv_params->round_1)) +
                           (1 << (offset_bits - conv_params->round_1 - 1));
  const int round_bits =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1;
  const uint16_t max = (1 << round_bits);
  const uint16_t min = round_offset;
  for (int i = -ws; i <= ws; ++i) {
    for (int j = -ws; j <= ws; ++j) {
      int r = (cr + i < 0) ? 0 : (cr + i >= h) ? h - 1 : cr + i;
      int c = (cc + j < 0) ? 0 : (cc + j >= w) ? w - 1 : cc + j;
      double s = (src[r * stride + c] - min) / ((double)max);
      crr += s * kernel[(i + 1) * ks + (j + 1)];
    }
  }
  return crr;
}

double kernel_correlation_uint8(const uint8_t *src, int stride, int h, int w,
                                int cr, int cc, const double *kernel, int ws) {
  double crr = 0;
  const int ks = 2 * ws + 1;
  for (int i = -ws; i <= ws; ++i) {
    for (int j = -ws; j <= ws; ++j) {
      int r = (cr + i < 0) ? 0 : (cr + i >= h) ? h - 1 : cr + i;
      int c = (cc + j < 0) ? 0 : (cc + j >= w) ? w - 1 : cc + j;
      crr += src[r * stride + c] * kernel[(i + ws) * ks + (j + ws)];
    }
  }
  return crr;
}

double *gen_correlation(const CONV_BUF_TYPE *src, int stride, int h, int w,
                        const double *kernel, int ws,
                        ConvolveParams *conv_params) {
  double *R;
  R = malloc(h * w * sizeof(*R));
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      R[i * w + j] =
          kernel_correlation(src, stride, h, w, i, j, kernel, ws, conv_params);
    }
  }
  return R;
}

double *gen_correlation_uint8(const uint8_t *src, int stride, int h, int w,
                              const double *kernel, int ws) {
  double *R;
  R = malloc(h * w * sizeof(*R));
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      R[i * w + j] =
          kernel_correlation_uint8(src, stride, h, w, i, j, kernel, ws);
    }
  }
  return R;
}
#endif  // CONFIG_CTX_ADAPT_LOG_WEIGHT

#if CONFIG_DIFFWTD_42
#define DIFFWTD_MASK_VAL 42
#define NORMAL_MASK DIFFWTD_42
#define INVERSE_MASK DIFFWTD_42_INV
#else
#define DIFFWTD_MASK_VAL 38
#define NORMAL_MASK DIFFWTD_38
#define INVERSE_MASK DIFFWTD_38_INV
#endif  // CONFIG_DIFFWTD_42

// This function will determine whether or not to create a warped
// prediction.
int av1_allow_warp(const MB_MODE_INFO *const mbmi,
                   const WarpTypesAllowed *const warp_types,
                   const WarpedMotionParams *const gm_params,
                   int build_for_obmc, const struct scale_factors *const sf,
                   WarpedMotionParams *final_warp_params) {
  // Note: As per the spec, we must test the fixed point scales here, which are
  // at a higher precision (1 << 14) than the xs and ys in subpel_params (that
  // have 1 << 10 precision).
  if (av1_is_scaled(sf)) return 0;

  if (final_warp_params != NULL) *final_warp_params = default_warp_params;

  if (build_for_obmc) return 0;

  if (warp_types->local_warp_allowed && !mbmi->wm_params.invalid) {
    if (final_warp_params != NULL)
      memcpy(final_warp_params, &mbmi->wm_params, sizeof(*final_warp_params));
    return 1;
  } else if (warp_types->global_warp_allowed && !gm_params->invalid) {
    if (final_warp_params != NULL)
      memcpy(final_warp_params, gm_params, sizeof(*final_warp_params));
    return 1;
  }

  return 0;
}

void av1_make_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride, const SubpelParams *subpel_params,
                              const struct scale_factors *sf, int w, int h,
                              ConvolveParams *conv_params,
                              int_interpfilters interp_filters,
                              const WarpTypesAllowed *warp_types, int p_col,
                              int p_row, int plane, int ref,
                              const MB_MODE_INFO *mi, int build_for_obmc,
                              const MACROBLOCKD *xd, int can_use_previous) {
  // Make sure the selected motion mode is valid for this configuration
  assert_motion_mode_valid(mi->motion_mode, xd->global_motion, xd, mi,
                           can_use_previous);
  assert(IMPLIES(conv_params->is_compound, conv_params->dst != NULL));

  WarpedMotionParams final_warp_params;
  const int do_warp =
      (w >= 8 && h >= 8 &&
       av1_allow_warp(mi, warp_types, &xd->global_motion[mi->ref_frame[ref]],
                      build_for_obmc, sf, &final_warp_params));
  const int is_intrabc = mi->use_intrabc;
  assert(IMPLIES(is_intrabc, !do_warp));

  if (do_warp && xd->cur_frame_force_integer_mv == 0) {
    const struct macroblockd_plane *const pd = &xd->plane[plane];
    const struct buf_2d *const pre_buf = &pd->pre[ref];
    av1_warp_plane(&final_warp_params, is_cur_buf_hbd(xd), xd->bd,
                   pre_buf->buf0, pre_buf->width, pre_buf->height,
                   pre_buf->stride, dst, p_col, p_row, w, h, dst_stride,
                   pd->subsampling_x, pd->subsampling_y, conv_params);
  } else if (is_cur_buf_hbd(xd)) {
    highbd_inter_predictor(src, src_stride, dst, dst_stride, subpel_params, sf,
                           w, h, conv_params, interp_filters, is_intrabc,
                           xd->bd);
  } else {
    inter_predictor(src, src_stride, dst, dst_stride, subpel_params, sf, w, h,
                    conv_params, interp_filters, is_intrabc);
  }
}

void av1_build_inter_predictors(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, const MB_MODE_INFO *mi,
    int build_for_obmc, int bw, int bh, int mi_x, int mi_y,
    CalcSubpelParamsFunc calc_subpel_params_func,
    const void *const calc_subpel_params_func_args) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  int is_compound = has_second_ref(mi);
  int ref;
  const int is_intrabc = is_intrabc_block(mi);
  assert(IMPLIES(is_intrabc, !is_compound));
  int is_global[2] = { 0, 0 };
  for (ref = 0; ref < 1 + is_compound; ++ref) {
    const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
    is_global[ref] = is_global_mv_block(mi, wm->wmtype);
  }

  const BLOCK_SIZE bsize = mi->sb_type;
  const int ss_x = pd->subsampling_x;
  const int ss_y = pd->subsampling_y;
  int sub8x8_inter = (block_size_wide[bsize] < 8 && ss_x) ||
                     (block_size_high[bsize] < 8 && ss_y);

#if CONFIG_EXT_PARTITIONS
  if (sub8x8_inter && bw == 8 && bh == 8) {
    // 3rd sub-block of a 16x16 VERT3 or HORZ3 partition.
    sub8x8_inter = 0;
  }
#endif  // CONFIG_EXT_PARTITIONS

  if (is_intrabc) sub8x8_inter = 0;

  // For sub8x8 chroma blocks, we may be covering more than one luma block's
  // worth of pixels. Thus (mi_x, mi_y) may not be the correct coordinates for
  // the top-left corner of the prediction source - the correct top-left
  // corner is at (pre_x, pre_y).
  int row_start = 0;
  int col_start = 0;
  if (!build_for_obmc) {
    const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
    const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
    int mi_row_offset, mi_col_offset;
    get_mi_row_col_offsets(mi_row, mi_col, ss_x, ss_y, mi_size_wide[bsize],
                           mi_size_high[bsize], &mi_row_offset, &mi_col_offset);
    row_start = -mi_row_offset;
    col_start = -mi_col_offset;
  }
  const int pre_x = (mi_x + MI_SIZE * col_start) >> ss_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> ss_y;

  sub8x8_inter = sub8x8_inter && !build_for_obmc;
  if (sub8x8_inter) {
    for (int row = row_start; row <= 0 && sub8x8_inter; ++row) {
      for (int col = col_start; col <= 0; ++col) {
        const MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
        if (!is_inter_block(this_mbmi)) sub8x8_inter = 0;
        if (is_intrabc_block(this_mbmi)) sub8x8_inter = 0;
      }
    }
  }

  if (sub8x8_inter) {
    // block size
    const int b4_w = block_size_wide[bsize] >> ss_x;
    const int b4_h = block_size_high[bsize] >> ss_y;
    const BLOCK_SIZE plane_bsize = scale_chroma_bsize(
        bsize, ss_x, ss_y, mi_y >> MI_SIZE_LOG2, mi_x >> MI_SIZE_LOG2);
    const int b8_w = block_size_wide[plane_bsize] >> ss_x;
    const int b8_h = block_size_high[plane_bsize] >> ss_y;
    assert(!is_compound);

    const struct buf_2d orig_pred_buf[2] = { pd->pre[0], pd->pre[1] };

    int row = row_start;
    for (int y = 0; y < b8_h; y += b4_h) {
      int col = col_start;
      for (int x = 0; x < b8_w; x += b4_w) {
        MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
        is_compound = has_second_ref(this_mbmi);
        int tmp_dst_stride = 8;
        assert(bw < 8 || bh < 8);
        ConvolveParams conv_params = get_conv_params_no_round(
            0, plane, xd->tmp_conv_dst, tmp_dst_stride, is_compound, xd->bd);
        conv_params.use_dist_wtd_comp_avg = 0;
        struct buf_2d *const dst_buf = &pd->dst;
        uint8_t *dst = dst_buf->buf + dst_buf->stride * y + x;

        ref = 0;
        const RefCntBuffer *ref_buf =
            get_ref_frame_buf(cm, this_mbmi->ref_frame[ref]);
        const struct scale_factors *ref_scale_factors =
            get_ref_scale_factors_const(cm, this_mbmi->ref_frame[ref]);

        pd->pre[ref].buf0 =
            (plane == 1) ? ref_buf->buf.u_buffer : ref_buf->buf.v_buffer;
        pd->pre[ref].buf =
            pd->pre[ref].buf0 + scaled_buffer_offset(pre_x, pre_y,
                                                     ref_buf->buf.uv_stride,
                                                     ref_scale_factors);
        pd->pre[ref].width = ref_buf->buf.uv_crop_width;
        pd->pre[ref].height = ref_buf->buf.uv_crop_height;
        pd->pre[ref].stride = ref_buf->buf.uv_stride;

        const struct scale_factors *const sf =
            is_intrabc ? &cm->sf_identity : ref_scale_factors;
        struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];

        const MV mv = this_mbmi->mv[ref].as_mv;

        const WarpTypesAllowed warp_types = {
          is_global[ref], this_mbmi->motion_mode == WARPED_CAUSAL
        };

        uint8_t *pre;
        SubpelParams subpel_params;
        int src_stride;
        calc_subpel_params_func(xd, sf, &mv, plane, pre_x, pre_y, x, y, pre_buf,
                                bw, bh, &warp_types, ref,
                                calc_subpel_params_func_args, &pre,
                                &subpel_params, &src_stride);

        conv_params.do_average = ref;
        if (is_masked_compound_type(mi->interinter_comp.type)) {
          // masked compound type has its own average mechanism
          conv_params.do_average = 0;
        }

        av1_make_inter_predictor(
            pre, src_stride, dst, dst_buf->stride, &subpel_params, sf, b4_w,
            b4_h, &conv_params, this_mbmi->interp_filters, &warp_types,
            (mi_x >> pd->subsampling_x) + x, (mi_y >> pd->subsampling_y) + y,
            plane, ref, mi, build_for_obmc, xd, cm->allow_warped_motion);

        ++col;
      }
      ++row;
    }

    for (ref = 0; ref < 2; ++ref) pd->pre[ref] = orig_pred_buf[ref];
    return;
  }

  {
    ConvolveParams conv_params = get_conv_params_no_round(
        0, plane, xd->tmp_conv_dst, MAX_SB_SIZE, is_compound, xd->bd);
    av1_dist_wtd_comp_weight_assign(
        cm, mi, 0, &conv_params.fwd_offset, &conv_params.bck_offset,
        &conv_params.use_dist_wtd_comp_avg, is_compound);

    struct buf_2d *const dst_buf = &pd->dst;
    uint8_t *const dst = dst_buf->buf;
    for (ref = 0; ref < 1 + is_compound; ++ref) {
      const struct scale_factors *const sf =
          is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref];
      struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];
      const MV mv = mi->mv[ref].as_mv;
      const WarpTypesAllowed warp_types = { is_global[ref],
                                            mi->motion_mode == WARPED_CAUSAL };

      uint8_t *pre;
      SubpelParams subpel_params;
      int src_stride;
      calc_subpel_params_func(
          xd, sf, &mv, plane, pre_x, pre_y, 0, 0, pre_buf, bw, bh, &warp_types,
          ref, calc_subpel_params_func_args, &pre, &subpel_params, &src_stride);

      if (ref && is_masked_compound_type(mi->interinter_comp.type)) {
        // masked compound type has its own average mechanism
        conv_params.do_average = 0;
        av1_make_masked_inter_predictor(
            pre, src_stride, dst, dst_buf->stride, &subpel_params, sf, bw, bh,
            &conv_params, mi->interp_filters, plane, &warp_types,
            mi_x >> pd->subsampling_x, mi_y >> pd->subsampling_y, ref, xd,
            cm->allow_warped_motion);
      } else {
        conv_params.do_average = ref;
        av1_make_inter_predictor(
            pre, src_stride, dst, dst_buf->stride, &subpel_params, sf, bw, bh,
            &conv_params, mi->interp_filters, &warp_types,
            mi_x >> pd->subsampling_x, mi_y >> pd->subsampling_y, plane, ref,
            mi, build_for_obmc, xd, cm->allow_warped_motion);
      }
    }
  }
}

#if USE_PRECOMPUTED_WEDGE_MASK
static const uint8_t wedge_master_oblique_odd[MASK_MASTER_SIZE] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  2,  6,  18,
  37, 53, 60, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};
static const uint8_t wedge_master_oblique_even[MASK_MASTER_SIZE] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  4,  11, 27,
  46, 58, 62, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};
static const uint8_t wedge_master_vertical[MASK_MASTER_SIZE] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  2,  7,  21,
  43, 57, 62, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
  64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};

static void shift_copy(const uint8_t *src, uint8_t *dst, int shift, int width) {
  if (shift >= 0) {
    memcpy(dst + shift, src, width - shift);
    memset(dst, src[0], shift);
  } else {
    shift = -shift;
    memcpy(dst, src + shift, width - shift);
    memset(dst + width - shift, src[width - 1], shift);
  }
}
#endif  // USE_PRECOMPUTED_WEDGE_MASK

#if USE_PRECOMPUTED_WEDGE_SIGN
/* clang-format off */
DECLARE_ALIGNED(16, static uint8_t,
                wedge_signflip_lookup[BLOCK_SIZES_ALL][MAX_WEDGE_TYPES]) = {
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 1, 1, 1, 1, 0, 1, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, },
  { 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0, 1, },
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
#if CONFIG_FLEX_PARTITION
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, },  // not used
#endif  // CONFIG_FLEX_PARTITION
};
/* clang-format on */
#else
DECLARE_ALIGNED(16, static uint8_t,
                wedge_signflip_lookup[BLOCK_SIZES_ALL][MAX_WEDGE_TYPES]);
#endif  // USE_PRECOMPUTED_WEDGE_SIGN

// [negative][direction]
DECLARE_ALIGNED(
    16, static uint8_t,
    wedge_mask_obl[2][WEDGE_DIRECTIONS][MASK_MASTER_SIZE * MASK_MASTER_SIZE]);

// 4 * MAX_WEDGE_SQUARE is an easy to compute and fairly tight upper bound
// on the sum of all mask sizes up to an including MAX_WEDGE_SQUARE.
DECLARE_ALIGNED(16, static uint8_t,
                wedge_mask_buf[2 * MAX_WEDGE_TYPES * 4 * MAX_WEDGE_SQUARE]);

static wedge_masks_type wedge_masks[BLOCK_SIZES_ALL][2];

static const wedge_code_type wedge_codebook_16_hgtw[16] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 6 }, { WEDGE_VERTICAL, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static const wedge_code_type wedge_codebook_16_hltw[16] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 4, 4 },
  { WEDGE_VERTICAL, 6, 4 },   { WEDGE_HORIZONTAL, 4, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

static const wedge_code_type wedge_codebook_16_heqw[16] = {
  { WEDGE_OBLIQUE27, 4, 4 },  { WEDGE_OBLIQUE63, 4, 4 },
  { WEDGE_OBLIQUE117, 4, 4 }, { WEDGE_OBLIQUE153, 4, 4 },
  { WEDGE_HORIZONTAL, 4, 2 }, { WEDGE_HORIZONTAL, 4, 6 },
  { WEDGE_VERTICAL, 2, 4 },   { WEDGE_VERTICAL, 6, 4 },
  { WEDGE_OBLIQUE27, 4, 2 },  { WEDGE_OBLIQUE27, 4, 6 },
  { WEDGE_OBLIQUE153, 4, 2 }, { WEDGE_OBLIQUE153, 4, 6 },
  { WEDGE_OBLIQUE63, 2, 4 },  { WEDGE_OBLIQUE63, 6, 4 },
  { WEDGE_OBLIQUE117, 2, 4 }, { WEDGE_OBLIQUE117, 6, 4 },
};

const wedge_params_type av1_wedge_params_lookup[BLOCK_SIZES_ALL] = {
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 4, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_8X8],
    wedge_masks[BLOCK_8X8] },
  { 4, wedge_codebook_16_hgtw, wedge_signflip_lookup[BLOCK_8X16],
    wedge_masks[BLOCK_8X16] },
  { 4, wedge_codebook_16_hltw, wedge_signflip_lookup[BLOCK_16X8],
    wedge_masks[BLOCK_16X8] },
  { 4, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_16X16],
    wedge_masks[BLOCK_16X16] },
  { 4, wedge_codebook_16_hgtw, wedge_signflip_lookup[BLOCK_16X32],
    wedge_masks[BLOCK_16X32] },
  { 4, wedge_codebook_16_hltw, wedge_signflip_lookup[BLOCK_32X16],
    wedge_masks[BLOCK_32X16] },
  { 4, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_32X32],
    wedge_masks[BLOCK_32X32] },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 4, wedge_codebook_16_hgtw, wedge_signflip_lookup[BLOCK_8X32],
    wedge_masks[BLOCK_8X32] },
  { 4, wedge_codebook_16_hltw, wedge_signflip_lookup[BLOCK_32X8],
    wedge_masks[BLOCK_32X8] },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
#if CONFIG_FLEX_PARTITION
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
#endif  // CONFIG_FLEX_PARTITION
};

static const uint8_t *get_wedge_mask_inplace(int wedge_index, int neg,
                                             BLOCK_SIZE sb_type) {
  const uint8_t *master;
  const int bh = block_size_high[sb_type];
  const int bw = block_size_wide[sb_type];
  const wedge_code_type *a =
      av1_wedge_params_lookup[sb_type].codebook + wedge_index;
  int woff, hoff;
  const uint8_t wsignflip =
      av1_wedge_params_lookup[sb_type].signflip[wedge_index];

  assert(wedge_index >= 0 &&
         wedge_index < (1 << get_wedge_bits_lookup(sb_type)));
  woff = (a->x_offset * bw) >> 3;
  hoff = (a->y_offset * bh) >> 3;
  master = wedge_mask_obl[neg ^ wsignflip][a->direction] +
           MASK_MASTER_STRIDE * (MASK_MASTER_SIZE / 2 - hoff) +
           MASK_MASTER_SIZE / 2 - woff;
  return master;
}

const uint8_t *av1_get_compound_type_mask(
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type) {
  assert(is_masked_compound_type(comp_data->type));
  (void)sb_type;
  switch (comp_data->type) {
    case COMPOUND_WEDGE:
      return av1_get_contiguous_soft_mask(comp_data->wedge_index,
                                          comp_data->wedge_sign, sb_type);
    case COMPOUND_DIFFWTD: return comp_data->seg_mask;
    default: assert(0); return NULL;
  }
}

static void diffwtd_mask_d16(uint8_t *mask, int which_inverse, int mask_base,
                             const CONV_BUF_TYPE *src0, int src0_stride,
                             const CONV_BUF_TYPE *src1, int src1_stride, int h,
                             int w, ConvolveParams *conv_params, int bd) {
#if CONFIG_CTX_ADAPT_LOG_WEIGHT
  (void)bd;
  (void)mask_base;
  double *R0 = NULL;
  double *R1 = NULL;
  int m;
  const CONV_BUF_TYPE *pred0 = which_inverse ? src1 : src0;
  const CONV_BUF_TYPE *pred1 = which_inverse ? src0 : src1;
  int stride0 = which_inverse ? src1_stride : src0_stride;
  int stride1 = which_inverse ? src0_stride : src1_stride;

  R0 = gen_correlation(pred0, stride0, h, w, log_k, LOG_K_SIZE, conv_params);
  R1 = gen_correlation(pred1, stride1, h, w, log_k, LOG_K_SIZE, conv_params);
  int i, j;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      double_t edge_diff = fabs(R0[i * w + j]) - fabs(R1[i * w + j]);
      if (edge_diff < DIFFLOG_THR) {
        m = LOG_WEIGHT_1;
      } else {
        m = LOG_WEIGHT_0;
      }
      mask[i * w + j] = which_inverse ? AOM_BLEND_A64_MAX_ALPHA - m : m;
    }
  }
  free(R0);
  free(R1);
#else
  int round =
      2 * FILTER_BITS - conv_params->round_0 - conv_params->round_1 + (bd - 8);
  int i, j, m, diff;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      diff = abs(src0[i * src0_stride + j] - src1[i * src1_stride + j]);
      diff = ROUND_POWER_OF_TWO(diff, round);
      m = clamp(mask_base + (diff / DIFF_FACTOR), 0, AOM_BLEND_A64_MAX_ALPHA);
      mask[i * w + j] = which_inverse ? AOM_BLEND_A64_MAX_ALPHA - m : m;
    }
  }
#endif  // CONFIG_CTX_ADAPT_LOG_WEIGHT
}

void av1_build_compound_diffwtd_mask_d16_c(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
  switch (mask_type) {
    case NORMAL_MASK:
      diffwtd_mask_d16(mask, 0, DIFFWTD_MASK_VAL, src0, src0_stride, src1,
                       src1_stride, h, w, conv_params, bd);
      break;
    case INVERSE_MASK:
      diffwtd_mask_d16(mask, 1, DIFFWTD_MASK_VAL, src0, src0_stride, src1,
                       src1_stride, h, w, conv_params, bd);
      break;
    default: assert(0);
  }
}

static void diffwtd_mask(uint8_t *mask, int which_inverse, int mask_base,
                         const uint8_t *src0, int src0_stride,
                         const uint8_t *src1, int src1_stride, int h, int w) {
#if CONFIG_CTX_ADAPT_LOG_WEIGHT
  (void)mask_base;
  double *R0 = NULL;
  double *R1 = NULL;
  int m;
  const uint8_t *pred0 = which_inverse ? src1 : src0;
  const uint8_t *pred1 = which_inverse ? src0 : src1;
  int stride0 = which_inverse ? src1_stride : src0_stride;
  int stride1 = which_inverse ? src0_stride : src1_stride;
  R0 = gen_correlation_uint8(pred0, stride0, h, w, log_k, LOG_K_SIZE);
  R1 = gen_correlation_uint8(pred1, stride1, h, w, log_k, LOG_K_SIZE);
  for (int i = 0; i < h; ++i) {
    for (int j = 0; j < w; ++j) {
      // this one works better
      double_t edge_diff = fabs(R0[i * w + j]) - fabs(R1[i * w + j]);
      if (edge_diff < DIFFLOG_THR) {
        m = LOG_WEIGHT_1;
      } else {
        m = LOG_WEIGHT_0;
      }
      mask[i * w + j] = which_inverse ? AOM_BLEND_A64_MAX_ALPHA - m : m;
    }
  }
  free(R0);
  free(R1);
#else
  int i, j, m, diff;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      diff =
          abs((int)src0[i * src0_stride + j] - (int)src1[i * src1_stride + j]);
      m = clamp(mask_base + (diff / DIFF_FACTOR), 0, AOM_BLEND_A64_MAX_ALPHA);
      mask[i * w + j] = which_inverse ? AOM_BLEND_A64_MAX_ALPHA - m : m;
    }
  }
#endif  // CONFIG_CTX_ADAPT_LOG_WEIGHT
}

void av1_build_compound_diffwtd_mask_c(uint8_t *mask,
                                       DIFFWTD_MASK_TYPE mask_type,
                                       const uint8_t *src0, int src0_stride,
                                       const uint8_t *src1, int src1_stride,
                                       int h, int w) {
  switch (mask_type) {
    case NORMAL_MASK:
      diffwtd_mask(mask, 0, DIFFWTD_MASK_VAL, src0, src0_stride, src1,
                   src1_stride, h, w);
      break;
    case INVERSE_MASK:
      diffwtd_mask(mask, 1, DIFFWTD_MASK_VAL, src0, src0_stride, src1,
                   src1_stride, h, w);
      break;
    default: assert(0);
  }
}

static AOM_FORCE_INLINE void diffwtd_mask_highbd(
    uint8_t *mask, int which_inverse, int mask_base, const uint16_t *src0,
    int src0_stride, const uint16_t *src1, int src1_stride, int h, int w,
    const unsigned int bd) {
  assert(bd >= 8);
  if (bd == 8) {
    if (which_inverse) {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
          int diff = abs((int)src0[j] - (int)src1[j]) / DIFF_FACTOR;
          unsigned int m = negative_to_zero(mask_base + diff);
          m = AOMMIN(m, AOM_BLEND_A64_MAX_ALPHA);
          mask[j] = AOM_BLEND_A64_MAX_ALPHA - m;
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    } else {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
          int diff = abs((int)src0[j] - (int)src1[j]) / DIFF_FACTOR;
          unsigned int m = negative_to_zero(mask_base + diff);
          m = AOMMIN(m, AOM_BLEND_A64_MAX_ALPHA);
          mask[j] = m;
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    }
  } else {
    const unsigned int bd_shift = bd - 8;
    if (which_inverse) {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
          int diff =
              (abs((int)src0[j] - (int)src1[j]) >> bd_shift) / DIFF_FACTOR;
          unsigned int m = negative_to_zero(mask_base + diff);
          m = AOMMIN(m, AOM_BLEND_A64_MAX_ALPHA);
          mask[j] = AOM_BLEND_A64_MAX_ALPHA - m;
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    } else {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; ++j) {
          int diff =
              (abs((int)src0[j] - (int)src1[j]) >> bd_shift) / DIFF_FACTOR;
          unsigned int m = negative_to_zero(mask_base + diff);
          m = AOMMIN(m, AOM_BLEND_A64_MAX_ALPHA);
          mask[j] = m;
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    }
  }
}

void av1_build_compound_diffwtd_mask_highbd_c(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0,
    int src0_stride, const uint8_t *src1, int src1_stride, int h, int w,
    int bd) {
  switch (mask_type) {
    case NORMAL_MASK:
      diffwtd_mask_highbd(mask, 0, DIFFWTD_MASK_VAL, CONVERT_TO_SHORTPTR(src0),
                          src0_stride, CONVERT_TO_SHORTPTR(src1), src1_stride,
                          h, w, bd);
      break;
    case INVERSE_MASK:
      diffwtd_mask_highbd(mask, 1, DIFFWTD_MASK_VAL, CONVERT_TO_SHORTPTR(src0),
                          src0_stride, CONVERT_TO_SHORTPTR(src1), src1_stride,
                          h, w, bd);
      break;
    default: assert(0);
  }
}

static void init_wedge_master_masks() {
  int i, j;
  const int w = MASK_MASTER_SIZE;
  const int h = MASK_MASTER_SIZE;
  const int stride = MASK_MASTER_STRIDE;
// Note: index [0] stores the masters, and [1] its complement.
#if USE_PRECOMPUTED_WEDGE_MASK
  // Generate prototype by shifting the masters
  int shift = h / 4;
  for (i = 0; i < h; i += 2) {
    shift_copy(wedge_master_oblique_even,
               &wedge_mask_obl[0][WEDGE_OBLIQUE63][i * stride], shift,
               MASK_MASTER_SIZE);
    shift--;
    shift_copy(wedge_master_oblique_odd,
               &wedge_mask_obl[0][WEDGE_OBLIQUE63][(i + 1) * stride], shift,
               MASK_MASTER_SIZE);
    memcpy(&wedge_mask_obl[0][WEDGE_VERTICAL][i * stride],
           wedge_master_vertical,
           MASK_MASTER_SIZE * sizeof(wedge_master_vertical[0]));
    memcpy(&wedge_mask_obl[0][WEDGE_VERTICAL][(i + 1) * stride],
           wedge_master_vertical,
           MASK_MASTER_SIZE * sizeof(wedge_master_vertical[0]));
  }
#else
  static const double smoother_param = 2.85;
  const int a[2] = { 2, 1 };
  const double asqrt = sqrt(a[0] * a[0] + a[1] * a[1]);
  for (i = 0; i < h; i++) {
    for (j = 0; j < w; ++j) {
      int x = (2 * j + 1 - w);
      int y = (2 * i + 1 - h);
      double d = (a[0] * x + a[1] * y) / asqrt;
      const int msk = (int)rint((1.0 + tanh(d / smoother_param)) * 32);
      wedge_mask_obl[0][WEDGE_OBLIQUE63][i * stride + j] = msk;
      const int mskx = (int)rint((1.0 + tanh(x / smoother_param)) * 32);
      wedge_mask_obl[0][WEDGE_VERTICAL][i * stride + j] = mskx;
    }
  }
#endif  // USE_PRECOMPUTED_WEDGE_MASK
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      const int msk = wedge_mask_obl[0][WEDGE_OBLIQUE63][i * stride + j];
      wedge_mask_obl[0][WEDGE_OBLIQUE27][j * stride + i] = msk;
      wedge_mask_obl[0][WEDGE_OBLIQUE117][i * stride + w - 1 - j] =
          wedge_mask_obl[0][WEDGE_OBLIQUE153][(w - 1 - j) * stride + i] =
              (1 << WEDGE_WEIGHT_BITS) - msk;
      wedge_mask_obl[1][WEDGE_OBLIQUE63][i * stride + j] =
          wedge_mask_obl[1][WEDGE_OBLIQUE27][j * stride + i] =
              (1 << WEDGE_WEIGHT_BITS) - msk;
      wedge_mask_obl[1][WEDGE_OBLIQUE117][i * stride + w - 1 - j] =
          wedge_mask_obl[1][WEDGE_OBLIQUE153][(w - 1 - j) * stride + i] = msk;
      const int mskx = wedge_mask_obl[0][WEDGE_VERTICAL][i * stride + j];
      wedge_mask_obl[0][WEDGE_HORIZONTAL][j * stride + i] = mskx;
      wedge_mask_obl[1][WEDGE_VERTICAL][i * stride + j] =
          wedge_mask_obl[1][WEDGE_HORIZONTAL][j * stride + i] =
              (1 << WEDGE_WEIGHT_BITS) - mskx;
    }
  }
}

#if !USE_PRECOMPUTED_WEDGE_SIGN
// If the signs for the wedges for various blocksizes are
// inconsistent flip the sign flag. Do it only once for every
// wedge codebook.
static void init_wedge_signs() {
  BLOCK_SIZE sb_type;
  memset(wedge_signflip_lookup, 0, sizeof(wedge_signflip_lookup));
  for (sb_type = BLOCK_4X4; sb_type < BLOCK_SIZES_ALL; ++sb_type) {
    const int bw = block_size_wide[sb_type];
    const int bh = block_size_high[sb_type];
    const wedge_params_type wedge_params = av1_wedge_params_lookup[sb_type];
    const int wbits = wedge_params.bits;
    const int wtypes = 1 << wbits;
    int i, w;
    if (wbits) {
      for (w = 0; w < wtypes; ++w) {
        // Get the mask master, i.e. index [0]
        const uint8_t *mask = get_wedge_mask_inplace(w, 0, sb_type);
        int avg = 0;
        for (i = 0; i < bw; ++i) avg += mask[i];
        for (i = 1; i < bh; ++i) avg += mask[i * MASK_MASTER_STRIDE];
        avg = (avg + (bw + bh - 1) / 2) / (bw + bh - 1);
        // Default sign of this wedge is 1 if the average < 32, 0 otherwise.
        // If default sign is 1:
        //   If sign requested is 0, we need to flip the sign and return
        //   the complement i.e. index [1] instead. If sign requested is 1
        //   we need to flip the sign and return index [0] instead.
        // If default sign is 0:
        //   If sign requested is 0, we need to return index [0] the master
        //   if sign requested is 1, we need to return the complement index [1]
        //   instead.
        wedge_params.signflip[w] = (avg < 32);
      }
    }
  }
}
#endif  // !USE_PRECOMPUTED_WEDGE_SIGN

static void init_wedge_masks() {
  uint8_t *dst = wedge_mask_buf;
  BLOCK_SIZE bsize;
  memset(wedge_masks, 0, sizeof(wedge_masks));
  for (bsize = BLOCK_4X4; bsize < BLOCK_SIZES_ALL; ++bsize) {
    const uint8_t *mask;
    const int bw = block_size_wide[bsize];
    const int bh = block_size_high[bsize];
    const wedge_params_type *wedge_params = &av1_wedge_params_lookup[bsize];
    const int wbits = wedge_params->bits;
    const int wtypes = 1 << wbits;
    int w;
    if (wbits == 0) continue;
    for (w = 0; w < wtypes; ++w) {
      mask = get_wedge_mask_inplace(w, 0, bsize);
      aom_convolve_copy(mask, MASK_MASTER_STRIDE, dst, bw, NULL, 0, NULL, 0, bw,
                        bh);
      wedge_params->masks[0][w] = dst;
      dst += bw * bh;

      mask = get_wedge_mask_inplace(w, 1, bsize);
      aom_convolve_copy(mask, MASK_MASTER_STRIDE, dst, bw, NULL, 0, NULL, 0, bw,
                        bh);
      wedge_params->masks[1][w] = dst;
      dst += bw * bh;
    }
    assert(sizeof(wedge_mask_buf) >= (size_t)(dst - wedge_mask_buf));
  }
}

// Equation of line: f(x, y) = a[0]*(x - a[2]*w/8) + a[1]*(y - a[3]*h/8) = 0
void av1_init_wedge_masks() {
  init_wedge_master_masks();
#if !USE_PRECOMPUTED_WEDGE_SIGN
  init_wedge_signs();
#endif  // !USE_PRECOMPUTED_WEDGE_SIGN
  init_wedge_masks();
}

static void build_masked_compound_no_round(
    uint8_t *dst, int dst_stride, const CONV_BUF_TYPE *src0, int src0_stride,
    const CONV_BUF_TYPE *src1, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w, ConvolveParams *conv_params, MACROBLOCKD *xd) {
  // Derive subsampling from h and w passed in. May be refactored to
  // pass in subsampling factors directly.
  const int subh = (2 << mi_size_high_log2[sb_type]) == h;
  const int subw = (2 << mi_size_wide_log2[sb_type]) == w;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type);
  if (is_cur_buf_hbd(xd)) {
    aom_highbd_blend_a64_d16_mask(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, block_size_wide[sb_type],
                                  w, h, subw, subh, conv_params, xd->bd);
  } else {
    aom_lowbd_blend_a64_d16_mask(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, block_size_wide[sb_type], w,
                                 h, subw, subh, conv_params);
  }
}

void av1_make_masked_inter_predictor(
    const uint8_t *pre, int pre_stride, uint8_t *dst, int dst_stride,
    const SubpelParams *subpel_params, const struct scale_factors *sf, int w,
    int h, ConvolveParams *conv_params, int_interpfilters interp_filters,
    int plane, const WarpTypesAllowed *warp_types, int p_col, int p_row,
    int ref, MACROBLOCKD *xd, int can_use_previous) {
  MB_MODE_INFO *mi = xd->mi[0];
  (void)dst;
  (void)dst_stride;
  mi->interinter_comp.seg_mask = xd->seg_mask;
  const INTERINTER_COMPOUND_DATA *comp_data = &mi->interinter_comp;

// We're going to call av1_make_inter_predictor to generate a prediction into
// a temporary buffer, then will blend that temporary buffer with that from
// the other reference.
//
#define INTER_PRED_BYTES_PER_PIXEL 2

  DECLARE_ALIGNED(32, uint8_t,
                  tmp_buf[INTER_PRED_BYTES_PER_PIXEL * MAX_SB_SQUARE]);
#undef INTER_PRED_BYTES_PER_PIXEL

  uint8_t *tmp_dst = get_buf_by_bd(xd, tmp_buf);

  const int tmp_buf_stride = MAX_SB_SIZE;
  CONV_BUF_TYPE *org_dst = conv_params->dst;
  int org_dst_stride = conv_params->dst_stride;
  CONV_BUF_TYPE *tmp_buf16 = (CONV_BUF_TYPE *)tmp_buf;
  conv_params->dst = tmp_buf16;
  conv_params->dst_stride = tmp_buf_stride;
  assert(conv_params->do_average == 0);

  // This will generate a prediction in tmp_buf for the second reference
  av1_make_inter_predictor(pre, pre_stride, tmp_dst, MAX_SB_SIZE, subpel_params,
                           sf, w, h, conv_params, interp_filters, warp_types,
                           p_col, p_row, plane, ref, mi, 0, xd,
                           can_use_previous);

  if (!plane && comp_data->type == COMPOUND_DIFFWTD) {
#if CONFIG_CTX_ADAPT_LOG_WEIGHT || CONFIG_DIFFWTD_42
    av1_build_compound_diffwtd_mask_d16_c(
        comp_data->seg_mask, comp_data->mask_type, org_dst, org_dst_stride,
        tmp_buf16, tmp_buf_stride, h, w, conv_params, xd->bd);
#else
    av1_build_compound_diffwtd_mask_d16(
        comp_data->seg_mask, comp_data->mask_type, org_dst, org_dst_stride,
        tmp_buf16, tmp_buf_stride, h, w, conv_params, xd->bd);
#endif  // CONFIG_CTX_ADAPT_LOG_WEIGHT || CONFIG_DIFFWTD_42
  }
  build_masked_compound_no_round(dst, dst_stride, org_dst, org_dst_stride,
                                 tmp_buf16, tmp_buf_stride, comp_data,
                                 mi->sb_type, h, w, conv_params, xd);
}

void av1_dist_wtd_comp_weight_assign(const AV1_COMMON *cm,
                                     const MB_MODE_INFO *mbmi, int order_idx,
                                     int *fwd_offset, int *bck_offset,
                                     int *use_dist_wtd_comp_avg,
                                     int is_compound) {
  assert(fwd_offset != NULL && bck_offset != NULL);
  if (!is_compound || mbmi->compound_idx) {
    *use_dist_wtd_comp_avg = 0;
    return;
  }

  *use_dist_wtd_comp_avg = 1;
  const RefCntBuffer *const bck_buf = get_ref_frame_buf(cm, mbmi->ref_frame[0]);
  const RefCntBuffer *const fwd_buf = get_ref_frame_buf(cm, mbmi->ref_frame[1]);
  const int cur_frame_index = cm->cur_frame->order_hint;
  int bck_frame_index = 0, fwd_frame_index = 0;

  if (bck_buf != NULL) bck_frame_index = bck_buf->order_hint;
  if (fwd_buf != NULL) fwd_frame_index = fwd_buf->order_hint;

  int d0 = clamp(abs(get_relative_dist(&cm->seq_params.order_hint_info,
                                       fwd_frame_index, cur_frame_index)),
                 0, MAX_FRAME_DISTANCE);
  int d1 = clamp(abs(get_relative_dist(&cm->seq_params.order_hint_info,
                                       cur_frame_index, bck_frame_index)),
                 0, MAX_FRAME_DISTANCE);

  const int order = d0 <= d1;

  if (d0 == 0 || d1 == 0) {
    *fwd_offset = quant_dist_lookup_table[order_idx][3][order];
    *bck_offset = quant_dist_lookup_table[order_idx][3][1 - order];
    return;
  }

  int i;
  for (i = 0; i < 3; ++i) {
    int c0 = quant_dist_weight[i][order];
    int c1 = quant_dist_weight[i][!order];
    int d0_c0 = d0 * c0;
    int d1_c1 = d1 * c1;
    if ((d0 > d1 && d0_c0 < d1_c1) || (d0 <= d1 && d0_c0 > d1_c1)) break;
  }

  *fwd_offset = quant_dist_lookup_table[order_idx][i][order];
  *bck_offset = quant_dist_lookup_table[order_idx][i][1 - order];
}

void av1_setup_dst_planes(struct macroblockd_plane *planes, BLOCK_SIZE bsize,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const int plane_start, const int plane_end) {
  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  for (int i = plane_start; i < AOMMIN(plane_end, MAX_MB_PLANE); ++i) {
    struct macroblockd_plane *const pd = &planes[i];
    const int is_uv = i > 0;
    setup_pred_plane(&pd->dst, bsize, src->buffers[i], src->crop_widths[is_uv],
                     src->crop_heights[is_uv], src->strides[is_uv], mi_row,
                     mi_col, NULL, pd->subsampling_x, pd->subsampling_y);
  }
}

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf,
                          const int num_planes) {
  if (src != NULL) {
    // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
    // the static analysis warnings.
    for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
      struct macroblockd_plane *const pd = &xd->plane[i];
      const int is_uv = i > 0;
      setup_pred_plane(&pd->pre[idx], xd->mi[0]->sb_type, src->buffers[i],
                       src->crop_widths[is_uv], src->crop_heights[is_uv],
                       src->strides[is_uv], mi_row, mi_col, sf,
                       pd->subsampling_x, pd->subsampling_y);
    }
  }
}

// obmc_mask_N[overlap_position]
static const uint8_t obmc_mask_1[1] = { 64 };
DECLARE_ALIGNED(2, static const uint8_t, obmc_mask_2[2]) = { 45, 64 };

DECLARE_ALIGNED(4, static const uint8_t, obmc_mask_4[4]) = { 39, 50, 59, 64 };

static const uint8_t obmc_mask_8[8] = { 36, 42, 48, 53, 57, 61, 64, 64 };

static const uint8_t obmc_mask_16[16] = { 34, 37, 40, 43, 46, 49, 52, 54,
                                          56, 58, 60, 61, 64, 64, 64, 64 };

static const uint8_t obmc_mask_32[32] = { 33, 35, 36, 38, 40, 41, 43, 44,
                                          45, 47, 48, 50, 51, 52, 53, 55,
                                          56, 57, 58, 59, 60, 60, 61, 62,
                                          64, 64, 64, 64, 64, 64, 64, 64 };

static const uint8_t obmc_mask_64[64] = {
  33, 34, 35, 35, 36, 37, 38, 39, 40, 40, 41, 42, 43, 44, 44, 44,
  45, 46, 47, 47, 48, 49, 50, 51, 51, 51, 52, 52, 53, 54, 55, 56,
  56, 56, 57, 57, 58, 58, 59, 60, 60, 60, 60, 60, 61, 62, 62, 62,
  62, 62, 63, 63, 63, 63, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
};

const uint8_t *av1_get_obmc_mask(int length) {
  switch (length) {
    case 1: return obmc_mask_1;
    case 2: return obmc_mask_2;
    case 4: return obmc_mask_4;
    case 8: return obmc_mask_8;
    case 16: return obmc_mask_16;
    case 32: return obmc_mask_32;
    case 64: return obmc_mask_64;
    default: assert(0); return NULL;
  }
}

static INLINE void increment_int_ptr(MACROBLOCKD *xd, int rel_mi_rc,
                                     uint8_t mi_hw, MB_MODE_INFO *mi,
                                     void *fun_ctxt, const int num_planes) {
  (void)xd;
  (void)rel_mi_rc;
  (void)mi_hw;
  (void)mi;
  ++*(int *)fun_ctxt;
  (void)num_planes;
}

void av1_count_overlappable_neighbors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                      int mi_row, int mi_col) {
  MB_MODE_INFO *mbmi = xd->mi[0];

  mbmi->overlappable_neighbors[0] = 0;
  mbmi->overlappable_neighbors[1] = 0;

  if (!is_motion_variation_allowed_bsize(mbmi->sb_type, mi_row, mi_col)) return;

  foreach_overlappable_nb_above(cm, xd, mi_col, INT_MAX, increment_int_ptr,
                                &mbmi->overlappable_neighbors[0]);
  foreach_overlappable_nb_left(cm, xd, mi_row, INT_MAX, increment_int_ptr,
                               &mbmi->overlappable_neighbors[1]);
}

// HW does not support < 4x4 prediction. To limit the bandwidth requirement, if
// block-size of current plane is smaller than 8x8, always only blend with the
// left neighbor(s) (skip blending with the above side).
#define DISABLE_CHROMA_U8X8_OBMC 0  // 0: one-sided obmc; 1: disable

int av1_skip_u4x4_pred_in_obmc(int mi_row, int mi_col, BLOCK_SIZE bsize,
                               const struct macroblockd_plane *pd, int dir) {
  assert(is_motion_variation_allowed_bsize(bsize, mi_row, mi_col));

  const BLOCK_SIZE bsize_plane = get_plane_block_size(
      mi_row, mi_col, bsize, pd->subsampling_x, pd->subsampling_y);
  switch (bsize_plane) {
#if DISABLE_CHROMA_U8X8_OBMC
    case BLOCK_4X4:
    case BLOCK_8X4:
    case BLOCK_4X8: return 1; break;
#else
    case BLOCK_4X4:
    case BLOCK_8X4:
    case BLOCK_4X8: return dir == 0; break;
#endif
    default: return 0;
  }
}

void av1_modify_neighbor_predictor_for_obmc(MB_MODE_INFO *mbmi) {
  mbmi->ref_frame[1] = NONE_FRAME;
  mbmi->interinter_comp.type = COMPOUND_AVERAGE;

  return;
}

struct obmc_inter_pred_ctxt {
  uint8_t **adjacent;
  int *adjacent_stride;
};

static INLINE void build_obmc_inter_pred_above(MACROBLOCKD *xd, int rel_mi_col,
                                               uint8_t above_mi_width,
                                               MB_MODE_INFO *above_mi,
                                               void *fun_ctxt,
                                               const int num_planes) {
  (void)above_mi;
  struct obmc_inter_pred_ctxt *ctxt = (struct obmc_inter_pred_ctxt *)fun_ctxt;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int is_hbd = is_cur_buf_hbd(xd);
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const int overlap =
      AOMMIN(block_size_high[bsize], block_size_high[BLOCK_64X64]) >> 1;

  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = (above_mi_width * MI_SIZE) >> pd->subsampling_x;
    const int bh = overlap >> pd->subsampling_y;
    const int plane_col = (rel_mi_col * MI_SIZE) >> pd->subsampling_x;

    if (av1_skip_u4x4_pred_in_obmc(mi_row, mi_col, bsize, pd, 0)) continue;

    const int dst_stride = pd->dst.stride;
    uint8_t *const dst = &pd->dst.buf[plane_col];
    const int tmp_stride = ctxt->adjacent_stride[plane];
    const uint8_t *const tmp = &ctxt->adjacent[plane][plane_col];
    const uint8_t *const mask = av1_get_obmc_mask(bh);

    if (is_hbd)
      aom_highbd_blend_a64_vmask(dst, dst_stride, dst, dst_stride, tmp,
                                 tmp_stride, mask, bw, bh, xd->bd);
    else
      aom_blend_a64_vmask(dst, dst_stride, dst, dst_stride, tmp, tmp_stride,
                          mask, bw, bh);
  }
}

static INLINE void build_obmc_inter_pred_left(MACROBLOCKD *xd, int rel_mi_row,
                                              uint8_t left_mi_height,
                                              MB_MODE_INFO *left_mi,
                                              void *fun_ctxt,
                                              const int num_planes) {
  (void)left_mi;
  struct obmc_inter_pred_ctxt *ctxt = (struct obmc_inter_pred_ctxt *)fun_ctxt;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int overlap =
      AOMMIN(block_size_wide[bsize], block_size_wide[BLOCK_64X64]) >> 1;
  const int is_hbd = is_cur_buf_hbd(xd);
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);

  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = overlap >> pd->subsampling_x;
    const int bh = (left_mi_height * MI_SIZE) >> pd->subsampling_y;
    const int plane_row = (rel_mi_row * MI_SIZE) >> pd->subsampling_y;

    if (av1_skip_u4x4_pred_in_obmc(mi_row, mi_col, bsize, pd, 1)) continue;

    const int dst_stride = pd->dst.stride;
    uint8_t *const dst = &pd->dst.buf[plane_row * dst_stride];
    const int tmp_stride = ctxt->adjacent_stride[plane];
    const uint8_t *const tmp = &ctxt->adjacent[plane][plane_row * tmp_stride];
    const uint8_t *const mask = av1_get_obmc_mask(bw);

    if (is_hbd)
      aom_highbd_blend_a64_hmask(dst, dst_stride, dst, dst_stride, tmp,
                                 tmp_stride, mask, bw, bh, xd->bd);
    else
      aom_blend_a64_hmask(dst, dst_stride, dst, dst_stride, tmp, tmp_stride,
                          mask, bw, bh);
  }
}

// This function combines motion compensated predictions that are generated by
// top/left neighboring blocks' inter predictors with the regular inter
// prediction. We assume the original prediction (bmc) is stored in
// xd->plane[].dst.buf
void av1_build_obmc_inter_prediction(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                     int mi_row, int mi_col,
                                     uint8_t *above[MAX_MB_PLANE],
                                     int above_stride[MAX_MB_PLANE],
                                     uint8_t *left[MAX_MB_PLANE],
                                     int left_stride[MAX_MB_PLANE]) {
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;

  // handle above row
  struct obmc_inter_pred_ctxt ctxt_above = { above, above_stride };
  foreach_overlappable_nb_above(cm, xd, mi_col,
                                max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                build_obmc_inter_pred_above, &ctxt_above);

  // handle left column
  struct obmc_inter_pred_ctxt ctxt_left = { left, left_stride };
  foreach_overlappable_nb_left(cm, xd, mi_row,
                               max_neighbor_obmc[mi_size_high_log2[bsize]],
                               build_obmc_inter_pred_left, &ctxt_left);
}

void av1_setup_build_prediction_by_above_pred(
    MACROBLOCKD *xd, int rel_mi_col, uint8_t above_mi_width,
    MB_MODE_INFO *above_mbmi, struct build_prediction_ctxt *ctxt,
    const int num_planes) {
  const BLOCK_SIZE a_bsize = AOMMAX(BLOCK_8X8, above_mbmi->sb_type);
  const int above_mi_col = ctxt->mi_col + rel_mi_col;

  av1_modify_neighbor_predictor_for_obmc(above_mbmi);

  for (int j = 0; j < num_planes; ++j) {
    struct macroblockd_plane *const pd = &xd->plane[j];
    setup_pred_plane(&pd->dst, a_bsize, ctxt->tmp_buf[j], ctxt->tmp_width[j],
                     ctxt->tmp_height[j], ctxt->tmp_stride[j], 0, rel_mi_col,
                     NULL, pd->subsampling_x, pd->subsampling_y);
  }

  const int num_refs = 1 + has_second_ref(above_mbmi);

  for (int ref = 0; ref < num_refs; ++ref) {
    const MV_REFERENCE_FRAME frame = above_mbmi->ref_frame[ref];

    const RefCntBuffer *const ref_buf = get_ref_frame_buf(ctxt->cm, frame);
    const struct scale_factors *const sf =
        get_ref_scale_factors_const(ctxt->cm, frame);
    xd->block_ref_scale_factors[ref] = sf;
    if ((!av1_is_valid_scale(sf)))
      aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                         "Reference frame has invalid dimensions");
    av1_setup_pre_planes(xd, ref, &ref_buf->buf, ctxt->mi_row, above_mi_col, sf,
                         num_planes);
  }

  xd->mb_to_left_edge = 8 * MI_SIZE * (-above_mi_col);
  xd->mb_to_right_edge = ctxt->mb_to_far_edge +
                         (xd->n4_w - rel_mi_col - above_mi_width) * MI_SIZE * 8;
}

void av1_setup_build_prediction_by_left_pred(MACROBLOCKD *xd, int rel_mi_row,
                                             uint8_t left_mi_height,
                                             MB_MODE_INFO *left_mbmi,
                                             struct build_prediction_ctxt *ctxt,
                                             const int num_planes) {
  const BLOCK_SIZE l_bsize = AOMMAX(BLOCK_8X8, left_mbmi->sb_type);
  const int left_mi_row = ctxt->mi_row + rel_mi_row;

  av1_modify_neighbor_predictor_for_obmc(left_mbmi);

  for (int j = 0; j < num_planes; ++j) {
    struct macroblockd_plane *const pd = &xd->plane[j];
    setup_pred_plane(&pd->dst, l_bsize, ctxt->tmp_buf[j], ctxt->tmp_width[j],
                     ctxt->tmp_height[j], ctxt->tmp_stride[j], rel_mi_row, 0,
                     NULL, pd->subsampling_x, pd->subsampling_y);
  }

  const int num_refs = 1 + has_second_ref(left_mbmi);

  for (int ref = 0; ref < num_refs; ++ref) {
    const MV_REFERENCE_FRAME frame = left_mbmi->ref_frame[ref];

    const RefCntBuffer *const ref_buf = get_ref_frame_buf(ctxt->cm, frame);
    const struct scale_factors *const ref_scale_factors =
        get_ref_scale_factors_const(ctxt->cm, frame);

    xd->block_ref_scale_factors[ref] = ref_scale_factors;
    if ((!av1_is_valid_scale(ref_scale_factors)))
      aom_internal_error(xd->error_info, AOM_CODEC_UNSUP_BITSTREAM,
                         "Reference frame has invalid dimensions");
    av1_setup_pre_planes(xd, ref, &ref_buf->buf, left_mi_row, ctxt->mi_col,
                         ref_scale_factors, num_planes);
  }

  xd->mb_to_top_edge = 8 * MI_SIZE * (-left_mi_row);
  xd->mb_to_bottom_edge =
      ctxt->mb_to_far_edge +
      (xd->n4_h - rel_mi_row - left_mi_height) * MI_SIZE * 8;
}

/* clang-format off */
static const uint8_t ii_weights1d[MAX_SB_SIZE] = {
  60, 58, 56, 54, 52, 50, 48, 47, 45, 44, 42, 41, 39, 38, 37, 35, 34, 33, 32,
  31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 22, 21, 20, 19, 19, 18, 18, 17, 16,
  16, 15, 15, 14, 14, 13, 13, 12, 12, 12, 11, 11, 10, 10, 10,  9,  9,  9,  8,
  8,  8,  8,  7,  7,  7,  7,  6,  6,  6,  6,  6,  5,  5,  5,  5,  5,  4,  4,
  4,  4,  4,  4,  4,  4,  3,  3,  3,  3,  3,  3,  3,  3,  3,  2,  2,  2,  2,
  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  1,  1,  1,  1,  1,  1,  1,
  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1
};
static uint8_t ii_size_scales[BLOCK_SIZES_ALL] = {
    32, 16, 16, 16, 8, 8, 8, 4,
    4,  4,  2,  2,  2, 1, 1, 1,
    8,  8,  4,  4,  2, 2,
#if CONFIG_FLEX_PARTITION
    8,  8,  4,  4,  8, 8,
#endif  // CONFIG_FLEX_PARTITION
};
/* clang-format on */

static void build_smooth_interintra_mask(uint8_t *mask, int stride,
                                         BLOCK_SIZE plane_bsize,
                                         INTERINTRA_MODE mode) {
  int i, j;
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];
  const int size_scale = ii_size_scales[plane_bsize];

  switch (mode) {
    case II_V_PRED:
      for (i = 0; i < bh; ++i) {
        memset(mask, ii_weights1d[i * size_scale], bw * sizeof(mask[0]));
        mask += stride;
      }
      break;

    case II_H_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j) mask[j] = ii_weights1d[j * size_scale];
        mask += stride;
      }
      break;

    case II_SMOOTH_PRED:
      for (i = 0; i < bh; ++i) {
        for (j = 0; j < bw; ++j)
          mask[j] = ii_weights1d[(i < j ? i : j) * size_scale];
        mask += stride;
      }
      break;

    case II_DC_PRED:
#if CONFIG_ILLUM_MCOMP
    case II_ILLUM_MCOMP_PRED:
#endif  // CONFIG_ILLUM_MCOMP
    default:
      for (i = 0; i < bh; ++i) {
        memset(mask, 32, bw * sizeof(mask[0]));
        mask += stride;
      }
      break;
  }
}

static void combine_interintra(INTERINTRA_MODE mode,
                               int8_t use_wedge_interintra, int8_t wedge_index,
                               int8_t wedge_sign, BLOCK_SIZE bsize,
                               BLOCK_SIZE plane_bsize, uint8_t *comppred,
                               int compstride, const uint8_t *interpred,
                               int interstride, const uint8_t *intrapred,
                               int intrastride) {
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];

  if (use_wedge_interintra) {
    if (is_interintra_wedge_used(bsize)) {
      const uint8_t *mask =
          av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
      const int subw = 2 * mi_size_wide[bsize] == bw;
      const int subh = 2 * mi_size_high[bsize] == bh;
      aom_blend_a64_mask(comppred, compstride, intrapred, intrastride,
                         interpred, interstride, mask, block_size_wide[bsize],
                         bw, bh, subw, subh);
    }
    return;
  }

  uint8_t mask[MAX_SB_SQUARE];
  build_smooth_interintra_mask(mask, bw, plane_bsize, mode);
  aom_blend_a64_mask(comppred, compstride, intrapred, intrastride, interpred,
                     interstride, mask, bw, bw, bh, 0, 0);
}

static void combine_interintra_highbd(
    INTERINTRA_MODE mode, int8_t use_wedge_interintra, int8_t wedge_index,
    int8_t wedge_sign, BLOCK_SIZE bsize, BLOCK_SIZE plane_bsize,
    uint8_t *comppred8, int compstride, const uint8_t *interpred8,
    int interstride, const uint8_t *intrapred8, int intrastride, int bd) {
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];

  if (use_wedge_interintra) {
    if (is_interintra_wedge_used(bsize)) {
      const uint8_t *mask =
          av1_get_contiguous_soft_mask(wedge_index, wedge_sign, bsize);
      const int subh = 2 * mi_size_high[bsize] == bh;
      const int subw = 2 * mi_size_wide[bsize] == bw;
      aom_highbd_blend_a64_mask(comppred8, compstride, intrapred8, intrastride,
                                interpred8, interstride, mask,
                                block_size_wide[bsize], bw, bh, subw, subh, bd);
    }
    return;
  }

  uint8_t mask[MAX_SB_SQUARE];
  build_smooth_interintra_mask(mask, bw, plane_bsize, mode);
  aom_highbd_blend_a64_mask(comppred8, compstride, intrapred8, intrastride,
                            interpred8, interstride, mask, bw, bw, bh, 0, 0,
                            bd);
}

void av1_build_intra_predictors_for_interintra(const AV1_COMMON *cm,
                                               MACROBLOCKD *xd,
                                               BLOCK_SIZE bsize, int plane,
                                               const BUFFER_SET *ctx,
                                               uint8_t *dst, int dst_stride) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int ssx = xd->plane[plane].subsampling_x;
  const int ssy = xd->plane[plane].subsampling_y;
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  BLOCK_SIZE plane_bsize =
      get_plane_block_size(mi_row, mi_col, bsize, ssx, ssy);
  PREDICTION_MODE mode = interintra_to_intra_mode[xd->mi[0]->interintra_mode];
  assert(xd->mi[0]->angle_delta[PLANE_TYPE_Y] == 0);
  assert(xd->mi[0]->angle_delta[PLANE_TYPE_UV] == 0);
  assert(xd->mi[0]->filter_intra_mode_info.use_filter_intra == 0);
  assert(xd->mi[0]->use_intrabc == 0);
  assert(plane_bsize < BLOCK_SIZES_ALL);

  av1_predict_intra_block(
      cm, xd, pd->width, pd->height, max_txsize_rect_lookup[plane_bsize], mode,
      0, 0, FILTER_INTRA_MODES,
#if CONFIG_ADAPT_FILTER_INTRA
      ADAPT_FILTER_INTRA_MODES,
#endif
      ctx->plane[plane], ctx->stride[plane], dst, dst_stride, 0, 0, plane);
}

void av1_combine_interintra(MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                            const uint8_t *inter_pred, int inter_stride,
                            const uint8_t *intra_pred, int intra_stride) {
  const int ssx = xd->plane[plane].subsampling_x;
  const int ssy = xd->plane[plane].subsampling_y;
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(mi_row, mi_col, bsize, ssx, ssy);
  if (is_cur_buf_hbd(xd)) {
    combine_interintra_highbd(
        xd->mi[0]->interintra_mode, xd->mi[0]->use_wedge_interintra,
        xd->mi[0]->interintra_wedge_index, INTERINTRA_WEDGE_SIGN, bsize,
        plane_bsize, xd->plane[plane].dst.buf, xd->plane[plane].dst.stride,
        inter_pred, inter_stride, intra_pred, intra_stride, xd->bd);
    return;
  }
  combine_interintra(
      xd->mi[0]->interintra_mode, xd->mi[0]->use_wedge_interintra,
      xd->mi[0]->interintra_wedge_index, INTERINTRA_WEDGE_SIGN, bsize,
      plane_bsize, xd->plane[plane].dst.buf, xd->plane[plane].dst.stride,
      inter_pred, inter_stride, intra_pred, intra_stride);
}

// build interintra_predictors for one plane
void av1_build_interintra_predictors_sbp(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                         uint8_t *pred, int stride,
                                         const BUFFER_SET *ctx, int plane,
                                         BLOCK_SIZE bsize) {
  assert(bsize < BLOCK_SIZES_ALL);
  if (is_cur_buf_hbd(xd)) {
    DECLARE_ALIGNED(16, uint16_t, intrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(
        cm, xd, bsize, plane, ctx, CONVERT_TO_BYTEPTR(intrapredictor),
        MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, plane, pred, stride,
                           CONVERT_TO_BYTEPTR(intrapredictor), MAX_SB_SIZE);
  } else {
    DECLARE_ALIGNED(16, uint8_t, intrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(cm, xd, bsize, plane, ctx,
                                              intrapredictor, MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, plane, pred, stride, intrapredictor,
                           MAX_SB_SIZE);
  }
}

void av1_build_interintra_predictors_sbuv(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                          uint8_t *upred, uint8_t *vpred,
                                          int ustride, int vstride,
                                          const BUFFER_SET *ctx,
                                          BLOCK_SIZE bsize) {
  av1_build_interintra_predictors_sbp(cm, xd, upred, ustride, ctx, 1, bsize);
  av1_build_interintra_predictors_sbp(cm, xd, vpred, vstride, ctx, 2, bsize);
}
