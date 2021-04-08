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

#include "av1/common/av1_common_int.h"
#include "av1/common/blockd.h"
#include "av1/common/mvref_common.h"
#include "av1/common/obmc.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"

// This function will determine whether or not to create a warped
// prediction.
int av1_allow_warp(const MB_MODE_INFO *const mbmi,
#if CONFIG_EXT_ROTATION
                   const MACROBLOCKD *xd,
#endif  // CONFIG_EXT_ROTATION
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
    if (final_warp_params != NULL) {
#if CONFIG_EXT_ROTATION
      if (globalmv_rotation_allowed(xd) && mbmi->rot_flag) {
        memcpy(final_warp_params, &mbmi->wm_params, sizeof(*final_warp_params));
        return 1;
      }
#endif  // CONFIG_EXT_ROTATION
      memcpy(final_warp_params, gm_params, sizeof(*final_warp_params));
    }
    return 1;
  }
#if CONFIG_EXT_ROTATION
  else if (simple_translation_rotation_allowed(mbmi) && mbmi->rot_flag) {
    if (final_warp_params != NULL) {
      memcpy(final_warp_params, &mbmi->wm_params, sizeof(*final_warp_params));
      return 1;
    }
  }
#endif  // CONFIG_EXT_ROTATION
  return 0;
}

void av1_init_inter_params(InterPredParams *inter_pred_params, int block_width,
                           int block_height, int pix_row, int pix_col,
                           int subsampling_x, int subsampling_y, int bit_depth,
                           int use_hbd_buf, int is_intrabc,
                           const struct scale_factors *sf,
                           const struct buf_2d *ref_buf,
#if CONFIG_REMOVE_DUAL_FILTER
                           InterpFilter interp_filter
#else
                           int_interpfilters interp_filters
#endif  // CONFIG_REMOVE_DUAL_FILTER
) {
  inter_pred_params->block_width = block_width;
  inter_pred_params->block_height = block_height;
#if CONFIG_OPTFLOW_REFINEMENT
  inter_pred_params->orig_width = block_width;
  inter_pred_params->orig_height = block_height;
#endif  // CONFIG_OPTFLOW_REFINEMENT
  inter_pred_params->pix_row = pix_row;
  inter_pred_params->pix_col = pix_col;
  inter_pred_params->subsampling_x = subsampling_x;
  inter_pred_params->subsampling_y = subsampling_y;
  inter_pred_params->bit_depth = bit_depth;
  inter_pred_params->use_hbd_buf = use_hbd_buf;
  inter_pred_params->is_intrabc = is_intrabc;
  inter_pred_params->scale_factors = sf;
  inter_pred_params->ref_frame_buf = *ref_buf;
  inter_pred_params->mode = TRANSLATION_PRED;
  inter_pred_params->comp_mode = UNIFORM_SINGLE;

  if (is_intrabc) {
    inter_pred_params->interp_filter_params[0] = &av1_intrabc_filter_params;
    inter_pred_params->interp_filter_params[1] = &av1_intrabc_filter_params;
  } else {
    inter_pred_params->interp_filter_params[0] =
        av1_get_interp_filter_params_with_block_size(
#if CONFIG_REMOVE_DUAL_FILTER
            interp_filter,
#else
            interp_filters.as_filters.x_filter,
#endif  // CONFIG_REMOVE_DUAL_FILTER
            block_width);
    inter_pred_params->interp_filter_params[1] =
        av1_get_interp_filter_params_with_block_size(
#if CONFIG_REMOVE_DUAL_FILTER
            interp_filter,
#else
            interp_filters.as_filters.y_filter,
#endif  // CONFIG_REMOVE_DUAL_FILTER
            block_height);
  }
}

void av1_init_comp_mode(InterPredParams *inter_pred_params) {
  inter_pred_params->comp_mode = UNIFORM_COMP;
}

void av1_init_warp_params(InterPredParams *inter_pred_params,
                          const WarpTypesAllowed *warp_types, int ref,
                          const MACROBLOCKD *xd, MB_MODE_INFO *mi) {
  if (inter_pred_params->block_height < 8 || inter_pred_params->block_width < 8)
    return;

  if (xd->cur_frame_force_integer_mv) return;

#if CONFIG_EXT_ROTATION
  if (mi->motion_mode == SIMPLE_TRANSLATION && mi->rot_flag) {
    if (globalmv_rotation_allowed(xd)) {
      memcpy(&mi->wm_params, &xd->global_motion[mi->ref_frame[0]],
             sizeof(WarpedMotionParams));
    } else if (simple_translation_rotation_allowed(mi)) {
      mi->wm_params.wmtype = IDENTITY;
    } else {
      mi->rot_flag = 0;
      mi->rotation = 0;
    }
    const int center_x = (xd->mi_col + (xd->width / 2)) * MI_SIZE;
    const int center_y = (xd->mi_row + (xd->height / 2)) * MI_SIZE;
    av1_warp_rotation(mi, mi->rotation, center_x, center_y);
    if (!av1_get_shear_params(&mi->wm_params)) {
      mi->rot_flag = 0;
    }
  }
#endif  // CONFIG_EXT_ROTATION
  if (av1_allow_warp(mi,
#if CONFIG_EXT_ROTATION
                     xd,
#endif  // CONFIG_EXT_ROTATION
                     warp_types, &xd->global_motion[mi->ref_frame[ref]], 0,
                     inter_pred_params->scale_factors,
                     &inter_pred_params->warp_params))
    inter_pred_params->mode = WARP_PRED;
}

void av1_make_inter_predictor(const uint8_t *src, int src_stride, uint8_t *dst,
                              int dst_stride,
                              InterPredParams *inter_pred_params,
                              const SubpelParams *subpel_params) {
  assert(IMPLIES(inter_pred_params->conv_params.is_compound,
                 inter_pred_params->conv_params.dst != NULL));

  // TODO(jingning): av1_warp_plane() can be further cleaned up.
  if (inter_pred_params->mode == WARP_PRED) {
    av1_warp_plane(
        &inter_pred_params->warp_params, inter_pred_params->use_hbd_buf,
        inter_pred_params->bit_depth, inter_pred_params->ref_frame_buf.buf0,
        inter_pred_params->ref_frame_buf.width,
        inter_pred_params->ref_frame_buf.height,
        inter_pred_params->ref_frame_buf.stride, dst,
        inter_pred_params->pix_col, inter_pred_params->pix_row,
        inter_pred_params->block_width, inter_pred_params->block_height,
        dst_stride, inter_pred_params->subsampling_x,
        inter_pred_params->subsampling_y, &inter_pred_params->conv_params);
  } else if (inter_pred_params->mode == TRANSLATION_PRED) {
    if (inter_pred_params->use_hbd_buf) {
      highbd_inter_predictor(src, src_stride, dst, dst_stride, subpel_params,
                             inter_pred_params->block_width,
                             inter_pred_params->block_height,
                             &inter_pred_params->conv_params,
                             inter_pred_params->interp_filter_params,
                             inter_pred_params->bit_depth);
    } else {
      inter_predictor(src, src_stride, dst, dst_stride, subpel_params,
                      inter_pred_params->block_width,
                      inter_pred_params->block_height,
                      &inter_pred_params->conv_params,
                      inter_pred_params->interp_filter_params);
    }
  }
}

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

static AOM_INLINE void shift_copy(const uint8_t *src, uint8_t *dst, int shift,
                                  int width) {
  if (shift >= 0) {
    memcpy(dst + shift, src, width - shift);
    memset(dst, src[0], shift);
  } else {
    shift = -shift;
    memcpy(dst, src + shift, width - shift);
    memset(dst + width - shift, src[width - 1], shift);
  }
}

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
};
/* clang-format on */

// [negative][direction]
DECLARE_ALIGNED(
    16, static uint8_t,
    wedge_mask_obl[2][WEDGE_DIRECTIONS][MASK_MASTER_SIZE * MASK_MASTER_SIZE]);

// 4 * MAX_WEDGE_SQUARE is an easy to compute and fairly tight upper bound
// on the sum of all mask sizes up to an including MAX_WEDGE_SQUARE.
DECLARE_ALIGNED(16, static uint8_t,
                wedge_mask_buf[2 * MAX_WEDGE_TYPES * 4 * MAX_WEDGE_SQUARE]);

DECLARE_ALIGNED(16, static uint8_t,
                smooth_interintra_mask_buf[INTERINTRA_MODES][BLOCK_SIZES_ALL]
                                          [MAX_WEDGE_SQUARE]);

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
  { MAX_WEDGE_TYPES, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_8X8],
    wedge_masks[BLOCK_8X8] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_hgtw, wedge_signflip_lookup[BLOCK_8X16],
    wedge_masks[BLOCK_8X16] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_hltw, wedge_signflip_lookup[BLOCK_16X8],
    wedge_masks[BLOCK_16X8] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_16X16],
    wedge_masks[BLOCK_16X16] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_hgtw, wedge_signflip_lookup[BLOCK_16X32],
    wedge_masks[BLOCK_16X32] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_hltw, wedge_signflip_lookup[BLOCK_32X16],
    wedge_masks[BLOCK_32X16] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_heqw, wedge_signflip_lookup[BLOCK_32X32],
    wedge_masks[BLOCK_32X32] },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
  { MAX_WEDGE_TYPES, wedge_codebook_16_hgtw, wedge_signflip_lookup[BLOCK_8X32],
    wedge_masks[BLOCK_8X32] },
  { MAX_WEDGE_TYPES, wedge_codebook_16_hltw, wedge_signflip_lookup[BLOCK_32X8],
    wedge_masks[BLOCK_32X8] },
  { 0, NULL, NULL, NULL },
  { 0, NULL, NULL, NULL },
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

  assert(wedge_index >= 0 && wedge_index < get_wedge_types_lookup(sb_type));
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

static AOM_INLINE void diffwtd_mask_d16(
    uint8_t *mask, int which_inverse, int mask_base, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
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
}

void av1_build_compound_diffwtd_mask_d16_c(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const CONV_BUF_TYPE *src0,
    int src0_stride, const CONV_BUF_TYPE *src1, int src1_stride, int h, int w,
    ConvolveParams *conv_params, int bd) {
  switch (mask_type) {
    case DIFFWTD_38:
      diffwtd_mask_d16(mask, 0, 38, src0, src0_stride, src1, src1_stride, h, w,
                       conv_params, bd);
      break;
    case DIFFWTD_38_INV:
      diffwtd_mask_d16(mask, 1, 38, src0, src0_stride, src1, src1_stride, h, w,
                       conv_params, bd);
      break;
    default: assert(0);
  }
}

static AOM_INLINE void diffwtd_mask(uint8_t *mask, int which_inverse,
                                    int mask_base, const uint8_t *src0,
                                    int src0_stride, const uint8_t *src1,
                                    int src1_stride, int h, int w) {
  int i, j, m, diff;
  for (i = 0; i < h; ++i) {
    for (j = 0; j < w; ++j) {
      diff =
          abs((int)src0[i * src0_stride + j] - (int)src1[i * src1_stride + j]);
      m = clamp(mask_base + (diff / DIFF_FACTOR), 0, AOM_BLEND_A64_MAX_ALPHA);
      mask[i * w + j] = which_inverse ? AOM_BLEND_A64_MAX_ALPHA - m : m;
    }
  }
}

void av1_build_compound_diffwtd_mask_c(uint8_t *mask,
                                       DIFFWTD_MASK_TYPE mask_type,
                                       const uint8_t *src0, int src0_stride,
                                       const uint8_t *src1, int src1_stride,
                                       int h, int w) {
  switch (mask_type) {
    case DIFFWTD_38:
      diffwtd_mask(mask, 0, 38, src0, src0_stride, src1, src1_stride, h, w);
      break;
    case DIFFWTD_38_INV:
      diffwtd_mask(mask, 1, 38, src0, src0_stride, src1, src1_stride, h, w);
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
    case DIFFWTD_38:
      diffwtd_mask_highbd(mask, 0, 38, CONVERT_TO_SHORTPTR(src0), src0_stride,
                          CONVERT_TO_SHORTPTR(src1), src1_stride, h, w, bd);
      break;
    case DIFFWTD_38_INV:
      diffwtd_mask_highbd(mask, 1, 38, CONVERT_TO_SHORTPTR(src0), src0_stride,
                          CONVERT_TO_SHORTPTR(src1), src1_stride, h, w, bd);
      break;
    default: assert(0);
  }
}

static AOM_INLINE void init_wedge_master_masks() {
  int i, j;
  const int w = MASK_MASTER_SIZE;
  const int h = MASK_MASTER_SIZE;
  const int stride = MASK_MASTER_STRIDE;
  // Note: index [0] stores the masters, and [1] its complement.
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

static AOM_INLINE void init_wedge_masks() {
  uint8_t *dst = wedge_mask_buf;
  BLOCK_SIZE bsize;
  memset(wedge_masks, 0, sizeof(wedge_masks));
  for (bsize = BLOCK_4X4; bsize < BLOCK_SIZES_ALL; ++bsize) {
    const wedge_params_type *wedge_params = &av1_wedge_params_lookup[bsize];
    const int wtypes = wedge_params->wedge_types;
    if (wtypes == 0) continue;
    const uint8_t *mask;
    const int bw = block_size_wide[bsize];
    const int bh = block_size_high[bsize];
    int w;
    for (w = 0; w < wtypes; ++w) {
      mask = get_wedge_mask_inplace(w, 0, bsize);
      aom_convolve_copy(mask, MASK_MASTER_STRIDE, dst, bw /* dst_stride */, bw,
                        bh);
      wedge_params->masks[0][w] = dst;
      dst += bw * bh;

      mask = get_wedge_mask_inplace(w, 1, bsize);
      aom_convolve_copy(mask, MASK_MASTER_STRIDE, dst, bw /* dst_stride */, bw,
                        bh);
      wedge_params->masks[1][w] = dst;
      dst += bw * bh;
    }
    assert(sizeof(wedge_mask_buf) >= (size_t)(dst - wedge_mask_buf));
  }
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
    8,  8,  4,  4,  2, 2
};
/* clang-format on */

static AOM_INLINE void build_smooth_interintra_mask(uint8_t *mask, int stride,
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
    default:
      for (i = 0; i < bh; ++i) {
        memset(mask, 32, bw * sizeof(mask[0]));
        mask += stride;
      }
      break;
  }
}

static AOM_INLINE void init_smooth_interintra_masks() {
  for (int m = 0; m < INTERINTRA_MODES; ++m) {
    for (int bs = 0; bs < BLOCK_SIZES_ALL; ++bs) {
      const int bw = block_size_wide[bs];
      const int bh = block_size_high[bs];
      if (bw > MAX_WEDGE_SIZE || bh > MAX_WEDGE_SIZE) continue;
      build_smooth_interintra_mask(smooth_interintra_mask_buf[m][bs], bw, bs,
                                   m);
    }
  }
}

#if CONFIG_OPTFLOW_REFINEMENT

// Whether to refine chroma MV or not
#define OPFL_REFINE_CHROMA 0

// Use downsampled gradient arrays to compute MV offsets
#define OPFL_DOWNSAMP_QUINCUNX 0

// Apply bilinear and bicubic interpolation for subpel gradient to avoid
// calls of build_one_inter_predictor function. Bicubic interpolation
// brings better quality but the speed results are neutral. As such, bilinear
// interpolation is used by default for a better trade-off between quality
// and complexity.
#define OPFL_BILINEAR_GRAD 0
#define OPFL_BICUBIC_GRAD 0

// Delta to use for computing gradients in bits, with 0 referring to
// integer-pel. The actual delta value used from the 1/8-pel original MVs
// is 2^(3 - SUBPEL_GRAD_DELTA_BITS). The max value of this macro is 3.
#define SUBPEL_GRAD_DELTA_BITS 3

// Bilinear and bicubic coefficients. Note that, at boundary, we apply
// coefficients that are doubled because spatial distance between the two
// interpolated pixels is halved. In other words, instead of computing
//   coeff * (v[delta] - v[-delta]) / (2 * delta),
// we are practically computing
//   coeff * (v[delta] - v[0]) / (2 * delta).
// Thus, coeff is doubled to get a better gradient quality.
#if OPFL_BILINEAR_GRAD
static const int bilinear_bits = 3;
static const int16_t coeffs_bilinear[4][2] = {
  { 8, 16 },  // delta = 1 (SUBPEL_GRAD_DELTA_BITS = 0)
  { 4, 8 },   // delta = 0.5 (SUBPEL_GRAD_DELTA_BITS = 1)
  { 2, 4 },   // delta = 0.25 (SUBPEL_GRAD_DELTA_BITS = 2)
  { 1, 2 },   // delta = 0.125 (SUBPEL_GRAD_DELTA_BITS = 3)
};
#endif
#if OPFL_BICUBIC_GRAD
static const int bicubic_bits = 7;
static const int16_t coeffs_bicubic[4][2][2] = {
  { { 128, 256 }, { 0, 0 } },    // delta = 1 (SUBPEL_GRAD_DELTA_BITS = 0)
  { { 80, 160 }, { -8, -16 } },  // delta = 0.5 (SUBPEL_GRAD_DELTA_BITS = 1)
  { { 42, 84 }, { -5, -10 } },   // delta = 0.25 (SUBPEL_GRAD_DELTA_BITS = 2)
  { { 21, 42 }, { -3, -6 } },    // delta = 0.125 (SUBPEL_GRAD_DELTA_BITS = 3)
};
#endif

// Note: grad_prec_bits param returned correspond to the precision
// of the gradient information in bits assuming gradient
// computed at unit pixel step normalization is 0 scale.
// Negative values indicate gradient returned at reduced precision, and
// positive values indicate gradient returned at higher precision.
int av1_compute_subpel_gradients_highbd(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, const MB_MODE_INFO *mi,
    int bw, int bh, int mi_x, int mi_y, uint8_t **mc_buf,
    CalcSubpelParamsFunc calc_subpel_params_func, int ref, uint16_t *pred_dst16,
    int *grad_prec_bits, int16_t *x_grad, int16_t *y_grad) {
  assert(cm->seq_params.order_hint_info.enable_order_hint);
  *grad_prec_bits = INT_MAX;

  uint8_t *pred_dst = CONVERT_TO_BYTEPTR(pred_dst16);
  // Compute distance between the current frame and reference
  const int cur_frame_index = cm->cur_frame->order_hint;
  const RefCntBuffer *const ref_buf = get_ref_frame_buf(cm, mi->ref_frame[ref]);
  assert(ref_buf != NULL);
  const int ref_index = ref_buf->order_hint;
  // Find the distance in display order between the current frame and each
  // reference
  const int r_dist = get_relative_dist(&cm->seq_params.order_hint_info,
                                       cur_frame_index, ref_index);

  // Do references one at a time
  const int is_compound = 0;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  struct buf_2d *const dst_buf = &pd->dst;

  int is_global[2] = { 0, 0 };
  const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
  is_global[ref] = is_global_mv_block(mi, wm->wmtype);
  const WarpTypesAllowed warp_types = { is_global[ref],
                                        mi->motion_mode == WARPED_CAUSAL };
  const struct scale_factors *const sf =
      mi->use_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref];

  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  int row_start = plane ? (mi->chroma_ref_info.mi_row_chroma_base - mi_row) : 0;
  int col_start = plane ? (mi->chroma_ref_info.mi_col_chroma_base - mi_col) : 0;
  const int pre_x = (mi_x + MI_SIZE * col_start) >> pd->subsampling_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> pd->subsampling_y;

  struct buf_2d *const pre_buf = mi->use_intrabc ? dst_buf : &pd->pre[ref];

  InterPredParams inter_pred_params;
  av1_init_inter_params(&inter_pred_params, bw, bh, pre_y, pre_x,
                        pd->subsampling_x, pd->subsampling_y, xd->bd,
                        is_cur_buf_hbd(xd), mi->use_intrabc, sf, pre_buf,
#if CONFIG_REMOVE_DUAL_FILTER
                        mi->interp_fltr);
#else
                        mi->interp_filters);
#endif  // CONFIG_REMOVE_DUAL_FILTER

  inter_pred_params.conv_params = get_conv_params_no_round(
      0, plane, xd->tmp_conv_dst, MAX_SB_SIZE, is_compound, xd->bd);
#if !CONFIG_REMOVE_DIST_WTD_COMP
  av1_dist_wtd_comp_weight_assign(
      cm, mi, 0, &inter_pred_params.conv_params.fwd_offset,
      &inter_pred_params.conv_params.bck_offset, is_compound);
#endif  // !CONFIG_REMOVE_DIST_WTD_COMP

  av1_init_warp_params(&inter_pred_params, &warp_types, ref, xd, mi);
  // TODO(sarahparker) make compatible with warped modes
  if (inter_pred_params.mode == WARP_PRED || !r_dist) return 0;

  // Original predictor
  const MV mv_orig = mi->mv[ref].as_mv;
  assert(mi->interinter_comp.type == COMPOUND_AVERAGE);
  av1_build_one_inter_predictor(pred_dst, bw, &mv_orig, &inter_pred_params, xd,
                                mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);

  // Reuse pixels in pred_dst to compute gradients
#if OPFL_BILINEAR_GRAD
  int id_next, id_prev, is_boundary;
  int16_t temp = 0;
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      // Subtract interpolated pixel at (i, j+delta) by the one at (i, j-delta)
      id_next = AOMMIN(j + 1, bw - 1);
      id_prev = AOMMAX(j - 1, 0);
      is_boundary = (j + 1 > bw - 1 || j - 1 < 0);
      temp = coeffs_bilinear[SUBPEL_GRAD_DELTA_BITS][is_boundary] *
             ((int16_t)pred_dst16[i * bw + id_next] -
              (int16_t)pred_dst16[i * bw + id_prev]);
      x_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bilinear_bits);
      // Subtract interpolated pixel at (i+delta, j) by the one at (i-delta, j)
      id_next = AOMMIN(i + 1, bh - 1);
      id_prev = AOMMAX(i - 1, 0);
      is_boundary = (i + 1 > bh - 1 || i - 1 < 0);
      temp = coeffs_bilinear[SUBPEL_GRAD_DELTA_BITS][is_boundary] *
             ((int16_t)pred_dst16[id_next * bw + j] -
              (int16_t)pred_dst16[id_prev * bw + j]);
      y_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bilinear_bits);
    }
  }
#elif OPFL_BICUBIC_GRAD
  int id_prev, id_prev2, id_next, id_next2, is_boundary;
  int16_t temp = 0;
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      // Subtract interpolated pixel at (i, j+delta) by the one at (i, j-delta)
      id_prev = AOMMAX(j - 1, 0);
      id_prev2 = AOMMAX(j - 2, 0);
      id_next = AOMMIN(j + 1, bw - 1);
      id_next2 = AOMMIN(j + 2, bw - 1);
      is_boundary = (j + 1 > bw - 1 || j - 1 < 0);
      temp = coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][0][is_boundary] *
                 ((int16_t)pred_dst16[i * bw + id_next] -
                  (int16_t)pred_dst16[i * bw + id_prev]) +
             coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][1][is_boundary] *
                 ((int16_t)pred_dst16[i * bw + id_next2] -
                  (int16_t)pred_dst16[i * bw + id_prev2]);
      x_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bicubic_bits);
      // Subtract interpolated pixel at (i+delta, j) by the one at (i-delta, j)
      id_prev = AOMMAX(i - 1, 0);
      id_prev2 = AOMMAX(i - 2, 0);
      id_next = AOMMIN(i + 1, bh - 1);
      id_next2 = AOMMIN(i + 2, bh - 1);
      is_boundary = (i + 1 > bh - 1 || i - 1 < 0);
      temp = coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][0][is_boundary] *
                 ((int16_t)pred_dst16[id_next * bw + j] -
                  (int16_t)pred_dst16[id_prev * bw + j]) +
             coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][1][is_boundary] *
                 ((int16_t)pred_dst16[id_next2 * bw + j] -
                  (int16_t)pred_dst16[id_prev2 * bw + j]);
      y_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bicubic_bits);
    }
  }
#else
  MV mv_modified = mv_orig;
  uint16_t tmp_buf1[MAX_SB_SIZE * MAX_SB_SIZE] = { 0 };
  uint16_t tmp_buf2[MAX_SB_SIZE * MAX_SB_SIZE] = { 0 };
  uint8_t *tmp_buf1_8 = CONVERT_TO_BYTEPTR(tmp_buf1);
  uint8_t *tmp_buf2_8 = CONVERT_TO_BYTEPTR(tmp_buf2);
  // X gradient
  // Get predictor to the left
  mv_modified.col = mv_orig.col - (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  mv_modified.row = mv_orig.row;
  av1_build_one_inter_predictor(tmp_buf1_8, bw, &mv_modified,
                                &inter_pred_params, xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Get predictor to the right
  mv_modified.col = mv_orig.col + (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  mv_modified.row = mv_orig.row;
  av1_build_one_inter_predictor(tmp_buf2_8, bw, &mv_modified,
                                &inter_pred_params, xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Compute difference.
  // Note since the deltas are at +2^g/8 and -2^g/8 subpel locations
  // (g = 3 - SUBPEL_GRAD_DELTA_BITS), the actual unit pel gradient is
  // 4/2^g = 2^(2-g) times the difference. Therefore the gradient returned
  // is at reduced precision by 2-g bits. That explains the grad_prec_bits
  // return value of g-2 at the end of this function.
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      x_grad[i * bw + j] =
          (int16_t)tmp_buf2[i * bw + j] - (int16_t)tmp_buf1[i * bw + j];
    }
  }

  // Y gradient
  // Get predictor below
  mv_modified.col = mv_orig.col;
  mv_modified.row = mv_orig.row - (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  av1_build_one_inter_predictor(tmp_buf1_8, bw, &mv_modified,
                                &inter_pred_params, xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Get predictor above
  mv_modified.col = mv_orig.col;
  mv_modified.row = mv_orig.row + (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  av1_build_one_inter_predictor(tmp_buf2_8, bw, &mv_modified,
                                &inter_pred_params, xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Compute difference.
  // Note since the deltas are at +2^g/8 and -2^g/8 subpel locations
  // (g = 3 - SUBPEL_GRAD_DELTA_BITS), the actual unit pel gradient is
  // 4/2^g = 2^(2-g) times the difference. Therefore the gradient returned
  // is at reduced precision by 2-g bits. That explains the grad_prec_bits
  // return value of g-2 at the end of this function.
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      y_grad[i * bw + j] =
          (int16_t)tmp_buf2[i * bw + j] - (int16_t)tmp_buf1[i * bw + j];
    }
  }
#endif  // OPFL_BILINEAR_GRAD/OPFL_BICUBIC_GRAD
  *grad_prec_bits = 3 - SUBPEL_GRAD_DELTA_BITS - 2;
  return r_dist;
}

// Note: grad_prec_bits param returned correspond to the precision
// of the gradient information in bits assuming gradient
// computed at unit pixel step normalization is 0 scale.
// Negative values indicate gradient returned at reduced precision, and
// positive values indicate gradient returned at higher precision.
int av1_compute_subpel_gradients_lowbd(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, const MB_MODE_INFO *mi,
    int bw, int bh, int mi_x, int mi_y, uint8_t **mc_buf,
    CalcSubpelParamsFunc calc_subpel_params_func, int ref, uint8_t *pred_dst,
    int *grad_prec_bits, int16_t *x_grad, int16_t *y_grad) {
  assert(cm->seq_params.order_hint_info.enable_order_hint);
  *grad_prec_bits = INT_MAX;

  // Compute distance between the current frame and reference
  const int cur_frame_index = cm->cur_frame->order_hint;
  const RefCntBuffer *const ref_buf = get_ref_frame_buf(cm, mi->ref_frame[ref]);
  assert(ref_buf != NULL);
  const int ref_index = ref_buf->order_hint;
  // Find the distance in display order between the current frame and each
  // reference
  const int r_dist = get_relative_dist(&cm->seq_params.order_hint_info,
                                       cur_frame_index, ref_index);

  // Do references one at a time
  const int is_compound = 0;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  struct buf_2d *const dst_buf = &pd->dst;

  int is_global[2] = { 0, 0 };
  const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
  is_global[ref] = is_global_mv_block(mi, wm->wmtype);
  const WarpTypesAllowed warp_types = { is_global[ref],
                                        mi->motion_mode == WARPED_CAUSAL };
  const struct scale_factors *const sf =
      mi->use_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref];

  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  int row_start = plane ? (mi->chroma_ref_info.mi_row_chroma_base - mi_row) : 0;
  int col_start = plane ? (mi->chroma_ref_info.mi_col_chroma_base - mi_col) : 0;
  const int pre_x = (mi_x + MI_SIZE * col_start) >> pd->subsampling_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> pd->subsampling_y;

  struct buf_2d *const pre_buf = mi->use_intrabc ? dst_buf : &pd->pre[ref];

  InterPredParams inter_pred_params;
  av1_init_inter_params(&inter_pred_params, bw, bh, pre_y, pre_x,
                        pd->subsampling_x, pd->subsampling_y, xd->bd,
                        is_cur_buf_hbd(xd), mi->use_intrabc, sf, pre_buf,
#if CONFIG_REMOVE_DUAL_FILTER
                        mi->interp_fltr);
#else
                        mi->interp_filters);
#endif  // CONFIG_REMOVE_DUAL_FILTER

  inter_pred_params.conv_params = get_conv_params_no_round(
      0, plane, xd->tmp_conv_dst, MAX_SB_SIZE, is_compound, xd->bd);
#if !CONFIG_REMOVE_DIST_WTD_COMP
  av1_dist_wtd_comp_weight_assign(
      cm, mi, 0, &inter_pred_params.conv_params.fwd_offset,
      &inter_pred_params.conv_params.bck_offset, is_compound);
#endif  // !CONFIG_REMOVE_DIST_WTD_COMP

  av1_init_warp_params(&inter_pred_params, &warp_types, ref, xd, mi);
  // TODO(sarahparker) make compatible with warped modes
  if (inter_pred_params.mode == WARP_PRED || !r_dist) return 0;

  // Original predictor
  const MV mv_orig = mi->mv[ref].as_mv;
  assert(mi->interinter_comp.type == COMPOUND_AVERAGE);
  av1_build_one_inter_predictor(pred_dst, bw, &mv_orig, &inter_pred_params, xd,
                                mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);

  // Reuse pixels in pred_dst to compute gradients
#if OPFL_BILINEAR_GRAD
  int id_next, id_prev, is_boundary;
  int16_t temp = 0;
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      // Subtract interpolated pixel (i, j+delta) by (i, j-delta)
      id_next = AOMMIN(j + 1, bw - 1);
      id_prev = AOMMAX(j - 1, 0);
      is_boundary = (j + 1 > bw - 1 || j - 1 < 0);
      temp = coeffs_bilinear[SUBPEL_GRAD_DELTA_BITS][is_boundary] *
             ((int16_t)pred_dst[i * bw + id_next] -
              (int16_t)pred_dst[i * bw + id_prev]);
      x_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bilinear_bits);
      // Subtract interpolated pixels (i+delta, j) by (i-delta, j)
      id_next = AOMMIN(i + 1, bh - 1);
      id_prev = AOMMAX(i - 1, 0);
      is_boundary = (i + 1 > bh - 1 || i - 1 < 0);
      temp = coeffs_bilinear[SUBPEL_GRAD_DELTA_BITS][is_boundary] *
             ((int16_t)pred_dst[id_next * bw + j] -
              (int16_t)pred_dst[id_prev * bw + j]);
      y_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bilinear_bits);
    }
  }
#elif OPFL_BICUBIC_GRAD
  int id_prev, id_prev2, id_next, id_next2, is_boundary;
  int16_t temp = 0;
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      // Subtract interpolated pixel at (i, j+delta) by the one at (i, j-delta)
      id_prev = AOMMAX(j - 1, 0);
      id_prev2 = AOMMAX(j - 2, 0);
      id_next = AOMMIN(j + 1, bw - 1);
      id_next2 = AOMMIN(j + 2, bw - 1);
      is_boundary = (j + 1 > bw - 1 || j - 1 < 0);
      temp = coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][0][is_boundary] *
                 ((int16_t)pred_dst[i * bw + id_next] -
                  (int16_t)pred_dst[i * bw + id_prev]) +
             coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][1][is_boundary] *
                 ((int16_t)pred_dst[i * bw + id_next2] -
                  (int16_t)pred_dst[i * bw + id_prev2]);
      x_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bicubic_bits);
      // Subtract interpolated pixel at (i+delta, j) by the one at (i-delta, j)
      id_prev = AOMMAX(i - 1, 0);
      id_prev2 = AOMMAX(i - 2, 0);
      id_next = AOMMIN(i + 1, bh - 1);
      id_next2 = AOMMIN(i + 2, bh - 1);
      is_boundary = (i + 1 > bh - 1 || i - 1 < 0);
      temp = coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][0][is_boundary] *
                 ((int16_t)pred_dst[id_next * bw + j] -
                  (int16_t)pred_dst[id_prev * bw + j]) +
             coeffs_bicubic[SUBPEL_GRAD_DELTA_BITS][1][is_boundary] *
                 ((int16_t)pred_dst[id_next2 * bw + j] -
                  (int16_t)pred_dst[id_prev2 * bw + j]);
      y_grad[i * bw + j] = ROUND_POWER_OF_TWO_SIGNED(temp, bicubic_bits);
    }
  }
#else
  MV mv_modified = mv_orig;
  uint8_t tmp_buf1[MAX_SB_SIZE * MAX_SB_SIZE] = { 0 };
  uint8_t tmp_buf2[MAX_SB_SIZE * MAX_SB_SIZE] = { 0 };
  // X gradient
  // Get predictor to the left
  mv_modified.col = mv_orig.col - (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  mv_modified.row = mv_orig.row;
  av1_build_one_inter_predictor(tmp_buf1, bw, &mv_modified, &inter_pred_params,
                                xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Get predictor to the right
  mv_modified.col = mv_orig.col + (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  mv_modified.row = mv_orig.row;
  av1_build_one_inter_predictor(tmp_buf2, bw, &mv_modified, &inter_pred_params,
                                xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Compute difference.
  // Note since the deltas are at +2^g/8 and -2^g/8 subpel locations
  // (g = 3 - SUBPEL_GRAD_DELTA_BITS), the actual unit pel gradient is
  // 4/2^g = 2^(2-g) times the difference. Therefore the gradient returned
  // is at reduced precision by 2-g bits. That explains the grad_prec_bits
  // return value of g-2 at the end of this function.
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      x_grad[i * bw + j] =
          (int16_t)tmp_buf2[i * bw + j] - (int16_t)tmp_buf1[i * bw + j];
    }
  }

  // Y gradient
  // Get predictor below
  mv_modified.col = mv_orig.col;
  mv_modified.row = mv_orig.row - (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  av1_build_one_inter_predictor(tmp_buf1, bw, &mv_modified, &inter_pred_params,
                                xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Get predictor above
  mv_modified.col = mv_orig.col;
  mv_modified.row = mv_orig.row + (1 << (3 - SUBPEL_GRAD_DELTA_BITS));
  av1_build_one_inter_predictor(tmp_buf2, bw, &mv_modified, &inter_pred_params,
                                xd, mi_x, mi_y, ref, mc_buf,
                                calc_subpel_params_func);
  // Compute difference.
  // Note since the deltas are at +2^g/8 and -2^g/8 subpel locations
  // (g = 3 - SUBPEL_GRAD_DELTA_BITS), the actual unit pel gradient is
  // 4/2^g = 2^(2-g) times the difference. Therefore the gradient returned
  // is at reduced precision by 2-g bits. That explains the grad_prec_bits
  // return value of g-2 at the end of this function.
  for (int i = 0; i < bh; i++) {
    for (int j = 0; j < bw; j++) {
      y_grad[i * bw + j] =
          (int16_t)tmp_buf2[i * bw + j] - (int16_t)tmp_buf1[i * bw + j];
    }
  }
#endif  // OPFL_BILINEAR_GRAD/OPFL_BICUBIC_GRAD
  *grad_prec_bits = 3 - SUBPEL_GRAD_DELTA_BITS - 2;
  return r_dist;
}

// Optical flow based mv refinement computation function:
//
// p0, pstride0: predictor 0 and its stride
// p1, pstride1: predictor 1 and its stride
// gx0, gy0: x and y gradients for p0
// gx1, gy1: x and y gradients for p1
// gstride: stride for all the gradients assumed to be the same
// bw, bh: block dumensions
// d0: distance of p0 to current frame, where +ve value refers to
//     p0 before the current frame.
// d1: distance of p1 to current frame, where +ve value refers to
//     p1 after the current frame.
// max_prec_bits: maximum offset in bits
// vx0, vy0: output high resolution mv offset for p0
// vx1, vy1: output high resolution mv offset for p1

void av1_opfl_mv_refinement_lowbd(const uint8_t *p0, int pstride0,
                                  const uint8_t *p1, int pstride1,
                                  const int16_t *gx0, const int16_t *gy0,
                                  const int16_t *gx1, const int16_t *gy1,
                                  int gstride, int bw, int bh, int d0, int d1,
                                  int grad_prec_bits, int mv_prec_bits,
                                  int *vx0, int *vy0, int *vx1, int *vy1) {
  int64_t su2 = 0;
  int64_t suv = 0;
  int64_t sv2 = 0;
  int64_t suw = 0;
  int64_t svw = 0;
  for (int i = 0; i < bh; ++i) {
    for (int j = 0; j < bw; ++j) {
#if OPFL_DOWNSAMP_QUINCUNX
      if ((i + j) % 2 == 1) continue;
#endif
      const int u = d0 * gx0[i * gstride + j] - d1 * gx1[i * gstride + j];
      const int v = d0 * gy0[i * gstride + j] - d1 * gy1[i * gstride + j];
      const int w = d0 * (p0[i * pstride0 + j] - p1[i * pstride1 + j]);
      su2 += (u * u);
      suv += (u * v);
      sv2 += (v * v);
      suw += (u * w);
      svw += (v * w);
    }
  }
  int bits = mv_prec_bits + grad_prec_bits;
  const int64_t D = su2 * sv2 - suv * suv;
  const int64_t Px = (suv * svw - sv2 * suw) * (1 << bits);
  const int64_t Py = (suv * suw - su2 * svw) * (1 << bits);

  if (D == 0) return;
  *vx0 = (int)DIVIDE_AND_ROUND_SIGNED(Px, D);
  *vy0 = (int)DIVIDE_AND_ROUND_SIGNED(Py, D);
  const int tx1 = (*vx0) * d1;
  const int ty1 = (*vy0) * d1;
  *vx1 = (int)DIVIDE_AND_ROUND_SIGNED(tx1, d0);
  *vy1 = (int)DIVIDE_AND_ROUND_SIGNED(ty1, d0);
}

void av1_opfl_mv_refinement_highbd(const uint16_t *p0, int pstride0,
                                   const uint16_t *p1, int pstride1,
                                   const int16_t *gx0, const int16_t *gy0,
                                   const int16_t *gx1, const int16_t *gy1,
                                   int gstride, int bw, int bh, int d0, int d1,
                                   int grad_prec_bits, int mv_prec_bits,
                                   int *vx0, int *vy0, int *vx1, int *vy1) {
  int64_t su2 = 0;
  int64_t suv = 0;
  int64_t sv2 = 0;
  int64_t suw = 0;
  int64_t svw = 0;
  for (int i = 0; i < bh; ++i) {
    for (int j = 0; j < bw; ++j) {
#if OPFL_DOWNSAMP_QUINCUNX
      if ((i + j) % 2 == 1) continue;
#endif
      const int u = d0 * gx0[i * gstride + j] - d1 * gx1[i * gstride + j];
      const int v = d0 * gy0[i * gstride + j] - d1 * gy1[i * gstride + j];
      const int w = d0 * (p0[i * pstride0 + j] - p1[i * pstride1 + j]);
      su2 += (u * u);
      suv += (u * v);
      sv2 += (v * v);
      suw += (u * w);
      svw += (v * w);
    }
  }
  int bits = mv_prec_bits + grad_prec_bits;
  const int64_t D = su2 * sv2 - suv * suv;
  const int64_t Px = (suv * svw - sv2 * suw) * (1 << bits);
  const int64_t Py = (suv * suw - su2 * svw) * (1 << bits);

  if (D == 0) return;
  *vx0 = (int)DIVIDE_AND_ROUND_SIGNED(Px, D);
  *vy0 = (int)DIVIDE_AND_ROUND_SIGNED(Py, D);
  const int tx1 = (*vx0) * d1;
  const int ty1 = (*vy0) * d1;
  *vx1 = (int)DIVIDE_AND_ROUND_SIGNED(tx1, d0);
  *vy1 = (int)DIVIDE_AND_ROUND_SIGNED(ty1, d0);
}

#if USE_OF_NXN
// Function to compute optical flow offsets in nxn blocks
int opfl_mv_refinement_nxn_highbd(const uint16_t *p0, int pstride0,
                                  const uint16_t *p1, int pstride1,
                                  const int16_t *gx0, const int16_t *gy0,
                                  const int16_t *gx1, const int16_t *gy1,
                                  int gstride, int bw, int bh, int n, int d0,
                                  int d1, int grad_prec_bits, int mv_prec_bits,
                                  int *vx0, int *vy0, int *vx1, int *vy1) {
  assert(bw % n == 0 && bh % n == 0);
  int n_blocks = 0;
  for (int i = 0; i < bh; i += n) {
    for (int j = 0; j < bw; j += n) {
      av1_opfl_mv_refinement_highbd(
          p0 + (i * pstride0 + j), pstride0, p1 + (i * pstride1 + j), pstride1,
          gx0 + (i * gstride + j), gy0 + (i * gstride + j),
          gx1 + (i * gstride + j), gy1 + (i * gstride + j), gstride, n, n, d0,
          d1, grad_prec_bits, mv_prec_bits, vx0 + n_blocks, vy0 + n_blocks,
          vx1 + n_blocks, vy1 + n_blocks);
      n_blocks++;
    }
  }
  return n_blocks;
}

// Function to compute optical flow offsets in nxn blocks
int opfl_mv_refinement_nxn_lowbd(const uint8_t *p0, int pstride0,
                                 const uint8_t *p1, int pstride1,
                                 const int16_t *gx0, const int16_t *gy0,
                                 const int16_t *gx1, const int16_t *gy1,
                                 int gstride, int bw, int bh, int n, int d0,
                                 int d1, int grad_prec_bits, int mv_prec_bits,
                                 int *vx0, int *vy0, int *vx1, int *vy1) {
  assert(bw % n == 0 && bh % n == 0);
  int n_blocks = 0;
  for (int i = 0; i < bh; i += n) {
    for (int j = 0; j < bw; j += n) {
      av1_opfl_mv_refinement_lowbd(
          p0 + (i * pstride0 + j), pstride0, p1 + (i * pstride1 + j), pstride1,
          gx0 + (i * gstride + j), gy0 + (i * gstride + j),
          gx1 + (i * gstride + j), gy1 + (i * gstride + j), gstride, n, n, d0,
          d1, grad_prec_bits, mv_prec_bits, vx0 + n_blocks, vy0 + n_blocks,
          vx1 + n_blocks, vy1 + n_blocks);
      n_blocks++;
    }
  }
  return n_blocks;
}
#endif  // USE_OF_NXN

static int get_optflow_based_mv_highbd(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, MB_MODE_INFO *mbmi,
    int bw, int bh, int mi_x, int mi_y, uint8_t **mc_buf,
    CalcSubpelParamsFunc calc_subpel_params_func) {
  // Arrays to hold optical flow offsets. If the experiment USE_OF_NXN is off,
  // these will only be length 1
  int vx0[N_OF_OFFSETS] = { 0 };
  int vx1[N_OF_OFFSETS] = { 0 };
  int vy0[N_OF_OFFSETS] = { 0 };
  int vy1[N_OF_OFFSETS] = { 0 };
  const int target_prec = MV_REFINE_PREC_BITS;
  // Convert output MV to 1/16th pel
  assert(MV_REFINE_PREC_BITS >= 3);
  for (int mvi = 0; mvi < N_OF_OFFSETS; mvi++) {
    mbmi->mv_refined[mvi * 2].as_mv.row *= 1 << (MV_REFINE_PREC_BITS - 3);
    mbmi->mv_refined[mvi * 2].as_mv.col *= 1 << (MV_REFINE_PREC_BITS - 3);
    mbmi->mv_refined[mvi * 2 + 1].as_mv.row *= 1 << (MV_REFINE_PREC_BITS - 3);
    mbmi->mv_refined[mvi * 2 + 1].as_mv.col *= 1 << (MV_REFINE_PREC_BITS - 3);
  }

  // Allocate gradient and prediction buffers
  int16_t *g0 = aom_malloc(2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g0));
  memset(g0, 0, 2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g0));
  uint16_t *dst0 = aom_malloc(MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst0));
  memset(dst0, 0, MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst0));
  int16_t *g1 = aom_malloc(2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g1));
  memset(g1, 0, 2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g1));
  uint16_t *dst1 = aom_malloc(MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst1));
  memset(dst1, 0, MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst1));

  int16_t *gx0 = g0;
  int16_t *gy0 = g0 + (MAX_SB_SIZE * MAX_SB_SIZE);
  int16_t *gx1 = g1;
  int16_t *gy1 = g1 + (MAX_SB_SIZE * MAX_SB_SIZE);
  int n_blocks = 1;
  int grad_prec_bits;

  // Compute gradients and predictor for P0
  int d0 = av1_compute_subpel_gradients_highbd(
      cm, xd, plane, mbmi, bw, bh, mi_x, mi_y, mc_buf, calc_subpel_params_func,
      0, dst0, &grad_prec_bits, gx0, gy0);
  if (d0 == 0) goto exit_refinement;

  // Compute gradients and predictor for P1
  int d1 = av1_compute_subpel_gradients_highbd(
      cm, xd, plane, mbmi, bw, bh, mi_x, mi_y, mc_buf, calc_subpel_params_func,
      1, dst1, &grad_prec_bits, gx1, gy1);
  if (d1 == 0) goto exit_refinement;

#if USE_OF_NXN
  int n = (bh <= 16 && bw <= 16) ? OF_MIN_BSIZE : OF_BSIZE;
  n_blocks = opfl_mv_refinement_nxn_highbd(
      dst0, bw, dst1, bw, gx0, gy0, gx1, gy1, bw, bw, bh, n, d0, d1,
      grad_prec_bits, target_prec, vx0, vy0, vx1, vy1);
#else
  av1_opfl_mv_refinement_highbd(dst0, bw, dst1, bw, gx0, gy0, gx1, gy1, bw, bw,
                                bh, d0, d1, grad_prec_bits, target_prec, vx0,
                                vy0, vx1, vy1);
#endif  // USE_OF_NXN

  for (int i = 0; i < n_blocks; i++) {
    mbmi->mv_refined[i * 2].as_mv.row += vy0[i];
    mbmi->mv_refined[i * 2].as_mv.col += vx0[i];
    mbmi->mv_refined[i * 2 + 1].as_mv.row += vy1[i];
    mbmi->mv_refined[i * 2 + 1].as_mv.col += vx1[i];
  }

exit_refinement:
  aom_free(g0);
  aom_free(dst0);
  aom_free(g1);
  aom_free(dst1);
  return target_prec;
}

static int get_optflow_based_mv_lowbd(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, MB_MODE_INFO *mbmi,
    int bw, int bh, int mi_x, int mi_y, uint8_t **mc_buf,
    CalcSubpelParamsFunc calc_subpel_params_func) {
  // Arrays to hold optical flow offsets. If the experiment USE_OF_NXN is off,
  // these will only be lenght 1
  int vx0[N_OF_OFFSETS] = { 0 };
  int vx1[N_OF_OFFSETS] = { 0 };
  int vy0[N_OF_OFFSETS] = { 0 };
  int vy1[N_OF_OFFSETS] = { 0 };
  const int target_prec = MV_REFINE_PREC_BITS;
  // Convert output MV to 1/16th pel
  assert(MV_REFINE_PREC_BITS >= 3);
  for (int mvi = 0; mvi < N_OF_OFFSETS; mvi++) {
    mbmi->mv_refined[mvi * 2].as_mv.row *= 1 << (MV_REFINE_PREC_BITS - 3);
    mbmi->mv_refined[mvi * 2].as_mv.col *= 1 << (MV_REFINE_PREC_BITS - 3);
    mbmi->mv_refined[mvi * 2 + 1].as_mv.row *= 1 << (MV_REFINE_PREC_BITS - 3);
    mbmi->mv_refined[mvi * 2 + 1].as_mv.col *= 1 << (MV_REFINE_PREC_BITS - 3);
  }

  // Allocate gradient and prediction buffers
  int16_t *g0 = aom_malloc(2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g0));
  memset(g0, 0, 2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g0));
  uint8_t *dst0 = aom_malloc(MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst0));
  memset(dst0, 0, MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst0));
  int16_t *g1 = aom_malloc(2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g1));
  memset(g1, 0, 2 * MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*g1));
  uint8_t *dst1 = aom_malloc(MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst1));
  memset(dst1, 0, MAX_SB_SIZE * MAX_SB_SIZE * sizeof(*dst1));

  int16_t *gx0 = g0;
  int16_t *gy0 = g0 + (MAX_SB_SIZE * MAX_SB_SIZE);
  int16_t *gx1 = g1;
  int16_t *gy1 = g1 + (MAX_SB_SIZE * MAX_SB_SIZE);
  int n_blocks = 1;
  int grad_prec_bits;

  // Compute gradients and predictor for P0
  int d0 = av1_compute_subpel_gradients_lowbd(
      cm, xd, plane, mbmi, bw, bh, mi_x, mi_y, mc_buf, calc_subpel_params_func,
      0, dst0, &grad_prec_bits, gx0, gy0);
  if (d0 == 0) goto exit_refinement;

  // Compute gradients and predictor for P1
  int d1 = av1_compute_subpel_gradients_lowbd(
      cm, xd, plane, mbmi, bw, bh, mi_x, mi_y, mc_buf, calc_subpel_params_func,
      1, dst1, &grad_prec_bits, gx1, gy1);
  if (d1 == 0) goto exit_refinement;

#if USE_OF_NXN
  int n = (bh <= 16 && bw <= 16) ? OF_MIN_BSIZE : OF_BSIZE;
  n_blocks = opfl_mv_refinement_nxn_lowbd(
      dst0, bw, dst1, bw, gx0, gy0, gx1, gy1, bw, bw, bh, n, d0, d1,
      grad_prec_bits, target_prec, vx0, vy0, vx1, vy1);
#else
  av1_opfl_mv_refinement_lowbd(dst0, bw, dst1, bw, gx0, gy0, gx1, gy1, bw, bw,
                               bh, d0, d1, grad_prec_bits, target_prec, vx0,
                               vy0, vx1, vy1);
#endif  // USE_OF_NXN

  for (int i = 0; i < n_blocks; i++) {
    mbmi->mv_refined[i * 2].as_mv.row += vy0[i];
    mbmi->mv_refined[i * 2].as_mv.col += vx0[i];
    mbmi->mv_refined[i * 2 + 1].as_mv.row += vy1[i];
    mbmi->mv_refined[i * 2 + 1].as_mv.col += vx1[i];
  }

exit_refinement:
  aom_free(g0);
  aom_free(dst0);
  aom_free(g1);
  aom_free(dst1);
  return target_prec;
}

// Refine MV using optical flow. The final output MV will be in 1/16
// precision.
int av1_get_optflow_based_mv(const AV1_COMMON *cm, MACROBLOCKD *xd, int plane,
                             MB_MODE_INFO *mbmi, int bw, int bh, int mi_x,
                             int mi_y, uint8_t **mc_buf,
                             CalcSubpelParamsFunc calc_subpel_params_func) {
  if (is_cur_buf_hbd(xd))
    return get_optflow_based_mv_highbd(cm, xd, plane, mbmi, bw, bh, mi_x, mi_y,
                                       mc_buf, calc_subpel_params_func);
  return get_optflow_based_mv_lowbd(cm, xd, plane, mbmi, bw, bh, mi_x, mi_y,
                                    mc_buf, calc_subpel_params_func);
}

#if USE_OF_NXN
// Makes the interpredictor for the region by dividing it up into nxn blocks
// and running the interpredictor code on each one.
void make_inter_pred_of_nxn(uint8_t *dst, int dst_stride,
                            int_mv *const mv_refined,
                            InterPredParams *inter_pred_params, MACROBLOCKD *xd,
                            int mi_x, int mi_y, int ref, uint8_t **mc_buf,
                            CalcSubpelParamsFunc calc_subpel_params_func, int n,
                            SubpelParams *subpel_params) {
  int n_blocks = 0;
  int w = inter_pred_params->orig_width;
  int h = inter_pred_params->orig_height;
  assert(w % n == 0);
  assert(h % n == 0);
  CONV_BUF_TYPE *orig_conv_dst = inter_pred_params->conv_params.dst;
  inter_pred_params->block_width = n;
  inter_pred_params->block_height = n;

  uint8_t *pre;
  int src_stride = 0;

  // Process whole nxn blocks.
  for (int j = 0; j <= h - n; j += n) {
    for (int i = 0; i <= w - n; i += n) {
      calc_subpel_params_func(&(mv_refined[n_blocks * 2 + ref].as_mv),
                              inter_pred_params, xd, mi_x + i, mi_y + j, ref, 1,
                              mc_buf, &pre, subpel_params, &src_stride);
      av1_make_inter_predictor(pre, src_stride, dst, dst_stride,
                               inter_pred_params, subpel_params);
      n_blocks++;
      dst += n;
      inter_pred_params->conv_params.dst += n;
      inter_pred_params->pix_col += n;
    }
    dst -= w;
    inter_pred_params->conv_params.dst -= w;
    inter_pred_params->pix_col -= w;

    dst += n * dst_stride;
    inter_pred_params->conv_params.dst +=
        n * inter_pred_params->conv_params.dst_stride;
    inter_pred_params->pix_row += n;
  }

  inter_pred_params->conv_params.dst = orig_conv_dst;
}
#endif  // USE_OF_NXN

void av1_build_optflow_inter_predictor(
    uint8_t *dst, int dst_stride, int plane, int_mv *const mv_refined,
    InterPredParams *inter_pred_params, MACROBLOCKD *xd, int mi_x, int mi_y,
    int ref, uint8_t **mc_buf, CalcSubpelParamsFunc calc_subpel_params_func) {
  SubpelParams subpel_params;
#if USE_OF_NXN
  int n = plane ? OF_BSIZE / 2
                : ((inter_pred_params->block_height <= 16 &&
                    inter_pred_params->block_width <= 16)
                       ? OF_MIN_BSIZE
                       : OF_BSIZE);
  make_inter_pred_of_nxn(dst, dst_stride, mv_refined, inter_pred_params, xd,
                         mi_x, mi_y, ref, mc_buf, calc_subpel_params_func, n,
                         &subpel_params);
#else
  uint8_t *src;
  int src_stride;
  calc_subpel_params_func(&(mv_refined[ref].as_mv), inter_pred_params, xd, mi_x,
                          mi_y, ref, 1, mc_buf, &src, &subpel_params,
                          &src_stride);
  av1_make_inter_predictor(src, src_stride, dst, dst_stride, inter_pred_params,
                           &subpel_params);
#endif  // USE_OF_NXN
}
#endif  // CONFIG_OPTFLOW_REFINEMENT

// Equation of line: f(x, y) = a[0]*(x - a[2]*w/8) + a[1]*(y - a[3]*h/8) = 0
void av1_init_wedge_masks() {
  init_wedge_master_masks();
  init_wedge_masks();
  init_smooth_interintra_masks();
}

static AOM_INLINE void build_masked_compound_no_round(
    uint8_t *dst, int dst_stride, const CONV_BUF_TYPE *src0, int src0_stride,
    const CONV_BUF_TYPE *src1, int src1_stride,
    const INTERINTER_COMPOUND_DATA *const comp_data, BLOCK_SIZE sb_type, int h,
    int w, InterPredParams *inter_pred_params) {
  const int ssy = inter_pred_params->subsampling_y;
  const int ssx = inter_pred_params->subsampling_x;
  const uint8_t *mask = av1_get_compound_type_mask(comp_data, sb_type);
  const int mask_stride = block_size_wide[sb_type];
  if (inter_pred_params->use_hbd_buf) {
    aom_highbd_blend_a64_d16_mask(dst, dst_stride, src0, src0_stride, src1,
                                  src1_stride, mask, mask_stride, w, h, ssx,
                                  ssy, &inter_pred_params->conv_params,
                                  inter_pred_params->bit_depth);
  } else {
    aom_lowbd_blend_a64_d16_mask(dst, dst_stride, src0, src0_stride, src1,
                                 src1_stride, mask, mask_stride, w, h, ssx, ssy,
                                 &inter_pred_params->conv_params);
  }
}

static void make_masked_inter_predictor(const uint8_t *pre, int pre_stride,
                                        uint8_t *dst, int dst_stride,
                                        InterPredParams *inter_pred_params,
                                        const SubpelParams *subpel_params) {
  const INTERINTER_COMPOUND_DATA *comp_data = &inter_pred_params->mask_comp;
  BLOCK_SIZE sb_type = inter_pred_params->sb_type;

  // We're going to call av1_make_inter_predictor to generate a prediction into
  // a temporary buffer, then will blend that temporary buffer with that from
  // the other reference.
  DECLARE_ALIGNED(32, uint8_t, tmp_buf[2 * MAX_SB_SQUARE]);
  uint8_t *tmp_dst =
      inter_pred_params->use_hbd_buf ? CONVERT_TO_BYTEPTR(tmp_buf) : tmp_buf;

  const int tmp_buf_stride = MAX_SB_SIZE;
  CONV_BUF_TYPE *org_dst = inter_pred_params->conv_params.dst;
  int org_dst_stride = inter_pred_params->conv_params.dst_stride;
  CONV_BUF_TYPE *tmp_buf16 = (CONV_BUF_TYPE *)tmp_buf;
  inter_pred_params->conv_params.dst = tmp_buf16;
  inter_pred_params->conv_params.dst_stride = tmp_buf_stride;
  assert(inter_pred_params->conv_params.do_average == 0);

  // This will generate a prediction in tmp_buf for the second reference
  av1_make_inter_predictor(pre, pre_stride, tmp_dst, MAX_SB_SIZE,
                           inter_pred_params, subpel_params);

  if (!inter_pred_params->conv_params.plane &&
      comp_data->type == COMPOUND_DIFFWTD) {
    av1_build_compound_diffwtd_mask_d16(
        comp_data->seg_mask, comp_data->mask_type, org_dst, org_dst_stride,
        tmp_buf16, tmp_buf_stride, inter_pred_params->block_height,
        inter_pred_params->block_width, &inter_pred_params->conv_params,
        inter_pred_params->bit_depth);
  }
  build_masked_compound_no_round(
      dst, dst_stride, org_dst, org_dst_stride, tmp_buf16, tmp_buf_stride,
      comp_data, sb_type, inter_pred_params->block_height,
      inter_pred_params->block_width, inter_pred_params);
}

void av1_build_one_inter_predictor(
    uint8_t *dst, int dst_stride, const MV *const src_mv,
    InterPredParams *inter_pred_params, MACROBLOCKD *xd, int mi_x, int mi_y,
    int ref, uint8_t **mc_buf, CalcSubpelParamsFunc calc_subpel_params_func) {
  SubpelParams subpel_params;
  uint8_t *src;
  int src_stride;
  calc_subpel_params_func(src_mv, inter_pred_params, xd, mi_x, mi_y, ref,
#if CONFIG_OPTFLOW_REFINEMENT
                          0, /* use_optflow_refinement */
#endif                       // CONFIG_OPTFLOW_REFINEMENT
                          mc_buf, &src, &subpel_params, &src_stride);

  if (inter_pred_params->comp_mode == UNIFORM_SINGLE ||
      inter_pred_params->comp_mode == UNIFORM_COMP) {
    av1_make_inter_predictor(src, src_stride, dst, dst_stride,
                             inter_pred_params, &subpel_params);
  } else {
    make_masked_inter_predictor(src, src_stride, dst, dst_stride,
                                inter_pred_params, &subpel_params);
  }
}

#if !CONFIG_REMOVE_DIST_WTD_COMP
void av1_dist_wtd_comp_weight_assign(const AV1_COMMON *cm,
                                     const MB_MODE_INFO *mbmi, int order_idx,
                                     int *fwd_offset, int *bck_offset,
                                     int is_compound) {
  assert(fwd_offset != NULL && bck_offset != NULL);
  if (!is_compound || mbmi->compound_idx) {
    *fwd_offset = 1 << (DIST_PRECISION_BITS - 1);
    *bck_offset = 1 << (DIST_PRECISION_BITS - 1);
    return;
  }

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
#endif  // !CONFIG_REMOVE_DIST_WTD_COMP

// True if the following hold:
//  1. Not intrabc and not build_for_obmc
//  2. At least one dimension is size 4 with subsampling
//  3. If sub-sampled, none of the previous blocks around the sub-sample
//     are intrabc or inter-blocks
static bool is_sub8x8_inter(const MACROBLOCKD *xd, const MB_MODE_INFO *mi,
                            int plane, int is_intrabc, int build_for_obmc) {
  if (is_intrabc || build_for_obmc) {
    return false;
  }

  if (!(plane && (mi->sb_type != mi->chroma_ref_info.bsize_base))) return false;

  // For sub8x8 chroma blocks, we may be covering more than one luma block's
  // worth of pixels. Thus (mi_x, mi_y) may not be the correct coordinates for
  // the top-left corner of the prediction source - the correct top-left corner
  // is at (pre_x, pre_y).
  const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
  const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
  const int row_start =
      plane ? mi->chroma_ref_info.mi_row_chroma_base - mi_row : 0;
  const int col_start =
      plane ? mi->chroma_ref_info.mi_col_chroma_base - mi_col : 0;

  for (int row = row_start; row <= 0; ++row) {
    for (int col = col_start; col <= 0; ++col) {
      const MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
      if (!is_inter_block(this_mbmi)) return false;
      if (is_intrabc_block(this_mbmi)) return false;
    }
  }
  return true;
}

static void build_inter_predictors_sub8x8(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, const MB_MODE_INFO *mi,
    int mi_x, int mi_y, uint8_t **mc_buf,
    CalcSubpelParamsFunc calc_subpel_params_func) {
  const BLOCK_SIZE bsize = mi->sb_type;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const bool ss_x = pd->subsampling_x;
  const bool ss_y = pd->subsampling_y;
  const int b4_w = block_size_wide[bsize] >> ss_x;
  const int b4_h = block_size_high[bsize] >> ss_y;
  const BLOCK_SIZE plane_bsize =
      plane ? mi->chroma_ref_info.bsize_base : mi->sb_type;
  const int b8_w = block_size_wide[plane_bsize] >> ss_x;
  const int b8_h = block_size_high[plane_bsize] >> ss_y;
  assert(!is_intrabc_block(mi));

  // For sub8x8 chroma blocks, we may be covering more than one luma block's
  // worth of pixels. Thus (mi_x, mi_y) may not be the correct coordinates for
  // the top-left corner of the prediction source - the correct top-left corner
  // is at (pre_x, pre_y).
  const int row_start =
      plane ? (mi->chroma_ref_info.mi_row_chroma_base - xd->mi_row) : 0;
  const int col_start =
      plane ? (mi->chroma_ref_info.mi_col_chroma_base - xd->mi_col) : 0;
  const int pre_x = (mi_x + MI_SIZE * col_start) >> ss_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> ss_y;

  int row = row_start;
  for (int y = 0; y < b8_h; y += b4_h) {
    int col = col_start;
    for (int x = 0; x < b8_w; x += b4_w) {
      MB_MODE_INFO *this_mbmi = xd->mi[row * xd->mi_stride + col];
#if CONFIG_EXT_RECUR_PARTITIONS
      // TODO(yuec): enabling compound prediction in none sub8x8 mbs in the
      // group
      bool is_compound = 0;
#else
      bool is_compound = has_second_ref(this_mbmi);
#endif  // CONFIG_EXT_RECUR_PARTITIONS
      struct buf_2d *const dst_buf = &pd->dst;
      uint8_t *dst = dst_buf->buf + dst_buf->stride * y + x;
      int ref = 0;
      const RefCntBuffer *ref_buf =
          get_ref_frame_buf(cm, this_mbmi->ref_frame[ref]);
      const struct scale_factors *ref_scale_factors =
          get_ref_scale_factors_const(cm, this_mbmi->ref_frame[ref]);
      const struct scale_factors *const sf = ref_scale_factors;
      const struct buf_2d pre_buf = {
        NULL,
        (plane == 1) ? ref_buf->buf.u_buffer : ref_buf->buf.v_buffer,
        ref_buf->buf.uv_crop_width,
        ref_buf->buf.uv_crop_height,
        ref_buf->buf.uv_stride,
      };

      const MV mv = this_mbmi->mv[ref].as_mv;
      InterPredParams inter_pred_params;
      av1_init_inter_params(&inter_pred_params, b4_w, b4_h, pre_y + y,
                            pre_x + x, pd->subsampling_x, pd->subsampling_y,
                            xd->bd, is_cur_buf_hbd(xd), mi->use_intrabc, sf,
                            &pre_buf,
#if CONFIG_REMOVE_DUAL_FILTER
                            this_mbmi->interp_fltr
#else
                            this_mbmi->interp_filters
#endif  // CONFIG_REMOVE_DUAL_FILTER
      );
      inter_pred_params.conv_params =
          get_conv_params_no_round(ref, plane, NULL, 0, is_compound, xd->bd);

      av1_build_one_inter_predictor(dst, dst_buf->stride, &mv,
                                    &inter_pred_params, xd, mi_x + x, mi_y + y,
                                    ref, mc_buf, calc_subpel_params_func);

      col += mi_size_wide[bsize];
    }
    row += mi_size_high[bsize];
  }
}

static void build_inter_predictors_8x8_and_bigger(
    const AV1_COMMON *cm, MACROBLOCKD *xd, int plane, MB_MODE_INFO *mi,
    int build_for_obmc, int bw, int bh, int mi_x, int mi_y, uint8_t **mc_buf,
    CalcSubpelParamsFunc calc_subpel_params_func) {
  const int is_compound = has_second_ref(mi);
  const int is_intrabc = is_intrabc_block(mi);
  assert(IMPLIES(is_intrabc, !is_compound));
  struct macroblockd_plane *const pd = &xd->plane[plane];
  struct buf_2d *const dst_buf = &pd->dst;
  uint8_t *const dst = dst_buf->buf;

  int is_global[2] = { 0, 0 };
  for (int ref = 0; ref < 1 + is_compound; ++ref) {
    const WarpedMotionParams *const wm = &xd->global_motion[mi->ref_frame[ref]];
    is_global[ref] = is_global_mv_block(mi, wm->wmtype);
  }

  int row_start = 0;
  int col_start = 0;
  if (!build_for_obmc) {
    const int mi_row = -xd->mb_to_top_edge >> (3 + MI_SIZE_LOG2);
    const int mi_col = -xd->mb_to_left_edge >> (3 + MI_SIZE_LOG2);
    row_start = plane ? (mi->chroma_ref_info.mi_row_chroma_base - mi_row) : 0;
    col_start = plane ? (mi->chroma_ref_info.mi_col_chroma_base - mi_col) : 0;
  }
  const int pre_x = (mi_x + MI_SIZE * col_start) >> pd->subsampling_x;
  const int pre_y = (mi_y + MI_SIZE * row_start) >> pd->subsampling_y;

#if CONFIG_OPTFLOW_REFINEMENT
  const int use_optflow_refinement = (mi->mode > NEW_NEWMV) && is_compound;
  assert(IMPLIES(use_optflow_refinement, !build_for_obmc));

  if (use_optflow_refinement && plane == 0) {
    // Initialize refined mv
    const MV mv0 = mi->mv[0].as_mv;
    const MV mv1 = mi->mv[1].as_mv;
    for (int mvi = 0; mvi < N_OF_OFFSETS; mvi++) {
      mi->mv_refined[mvi * 2].as_mv = mv0;
      mi->mv_refined[mvi * 2 + 1].as_mv = mv1;
    }
    av1_get_optflow_based_mv(cm, xd, plane, mi, bw, bh, mi_x, mi_y, mc_buf,
                             calc_subpel_params_func);
  }
#endif  // CONFIG_OPTFLOW_REFINEMENT

  for (int ref = 0; ref < 1 + is_compound; ++ref) {
    const struct scale_factors *const sf =
        is_intrabc ? &cm->sf_identity : xd->block_ref_scale_factors[ref];
    struct buf_2d *const pre_buf = is_intrabc ? dst_buf : &pd->pre[ref];
    const MV mv = mi->mv[ref].as_mv;
    const WarpTypesAllowed warp_types = { is_global[ref],
                                          mi->motion_mode == WARPED_CAUSAL };

    InterPredParams inter_pred_params;
    av1_init_inter_params(&inter_pred_params, bw, bh, pre_y, pre_x,
                          pd->subsampling_x, pd->subsampling_y, xd->bd,
                          is_cur_buf_hbd(xd), mi->use_intrabc, sf, pre_buf,
#if CONFIG_REMOVE_DUAL_FILTER
                          mi->interp_fltr
#else
                          mi->interp_filters
#endif  // CONFIG_REMOVE_DUAL_FILTER
    );
    if (is_compound) av1_init_comp_mode(&inter_pred_params);
    inter_pred_params.conv_params = get_conv_params_no_round(
        ref, plane, xd->tmp_conv_dst, MAX_SB_SIZE, is_compound, xd->bd);

#if !CONFIG_REMOVE_DIST_WTD_COMP
    av1_dist_wtd_comp_weight_assign(
        cm, mi, 0, &inter_pred_params.conv_params.fwd_offset,
        &inter_pred_params.conv_params.bck_offset, is_compound);
#endif  // !CONFIG_REMOVE_DIST_WTD_COMP

    if (!build_for_obmc)
      av1_init_warp_params(&inter_pred_params, &warp_types, ref, xd, mi);

    if (is_masked_compound_type(mi->interinter_comp.type)) {
      inter_pred_params.sb_type = mi->sb_type;
      inter_pred_params.mask_comp = mi->interinter_comp;
      if (ref == 1) {
        inter_pred_params.conv_params.do_average = 0;
        inter_pred_params.comp_mode = MASK_COMP;
      }
      // Assign physical buffer.
      inter_pred_params.mask_comp.seg_mask = xd->seg_mask;
    }

#if CONFIG_OPTFLOW_REFINEMENT
#if OPFL_REFINE_CHROMA
    // For luma, always apply offset MVs. For chroma, use the MVs derived for
    // luma if luma subblock size is 8x8 (i.e., chroma block size > 8x8),
    // and otherwise apply normal compound average.
    if (use_optflow_refinement && (plane == 0 || bh > 8 || bw > 8)) {
#else
    if (use_optflow_refinement && plane == 0) {
#endif
      av1_build_optflow_inter_predictor(
          dst, dst_buf->stride, plane, mi->mv_refined, &inter_pred_params, xd,
          mi_x, mi_y, ref, mc_buf, calc_subpel_params_func);
      continue;
    }
#endif  // CONFIG_OPTFLOW_REFINEMENT
    av1_build_one_inter_predictor(dst, dst_buf->stride, &mv, &inter_pred_params,
                                  xd, mi_x, mi_y, ref, mc_buf,
                                  calc_subpel_params_func);
  }
}

void av1_build_inter_predictors(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                int plane, MB_MODE_INFO *mi, int build_for_obmc,
                                int bw, int bh, int mi_x, int mi_y,
                                uint8_t **mc_buf,
                                CalcSubpelParamsFunc calc_subpel_params_func) {
  if (is_sub8x8_inter(xd, mi, plane, is_intrabc_block(mi), build_for_obmc)) {
#if !CONFIG_EXT_RECUR_PARTITIONS
    assert(bw < 8 || bh < 8);
#endif  // !CONFIG_EXT_RECUR_PARTITIONS
    build_inter_predictors_sub8x8(cm, xd, plane, mi, mi_x, mi_y, mc_buf,
                                  calc_subpel_params_func);
  } else {
    build_inter_predictors_8x8_and_bigger(cm, xd, plane, mi, build_for_obmc, bw,
                                          bh, mi_x, mi_y, mc_buf,
                                          calc_subpel_params_func);
  }
}

void av1_setup_dst_planes(struct macroblockd_plane *planes,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const int plane_start, const int plane_end,
                          const CHROMA_REF_INFO *chr_ref_info) {
  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  for (int i = plane_start; i < AOMMIN(plane_end, MAX_MB_PLANE); ++i) {
    struct macroblockd_plane *const pd = &planes[i];
    const int is_uv = i > 0;
    setup_pred_plane(&pd->dst, src->buffers[i], src->crop_widths[is_uv],
                     src->crop_heights[is_uv], src->strides[is_uv], mi_row,
                     mi_col, NULL, pd->subsampling_x, pd->subsampling_y,
                     chr_ref_info);
  }
}

void av1_setup_pre_planes(MACROBLOCKD *xd, int idx,
                          const YV12_BUFFER_CONFIG *src, int mi_row, int mi_col,
                          const struct scale_factors *sf, const int num_planes,
                          const CHROMA_REF_INFO *chr_ref_info) {
  if (src != NULL) {
    // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
    // the static analysis warnings.
    for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
      struct macroblockd_plane *const pd = &xd->plane[i];
      const int is_uv = i > 0;
      setup_pred_plane(&pd->pre[idx], src->buffers[i], src->crop_widths[is_uv],
                       src->crop_heights[is_uv], src->strides[is_uv], mi_row,
                       mi_col, sf, pd->subsampling_x, pd->subsampling_y,
                       chr_ref_info);
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

static INLINE void increment_int_ptr(MACROBLOCKD *xd, int rel_mi_row,
                                     int rel_mi_col, uint8_t op_mi_size,
                                     int dir, MB_MODE_INFO *mi, void *fun_ctxt,
                                     const int num_planes) {
  (void)xd;
  (void)rel_mi_row;
  (void)rel_mi_col;
  (void)op_mi_size;
  (void)dir;
  (void)mi;
  ++*(int *)fun_ctxt;
  (void)num_planes;
}

void av1_count_overlappable_neighbors(const AV1_COMMON *cm, MACROBLOCKD *xd) {
  MB_MODE_INFO *mbmi = xd->mi[0];

  mbmi->overlappable_neighbors[0] = 0;
  mbmi->overlappable_neighbors[1] = 0;

  if (!is_motion_variation_allowed_bsize(mbmi->sb_type, xd->mi_row, xd->mi_col))
    return;

  foreach_overlappable_nb_above(cm, xd, INT_MAX, increment_int_ptr,
                                &mbmi->overlappable_neighbors[0]);
  foreach_overlappable_nb_left(cm, xd, INT_MAX, increment_int_ptr,
                               &mbmi->overlappable_neighbors[1]);
}

// HW does not support < 4x4 prediction. To limit the bandwidth requirement, if
// block-size of current plane is smaller than 8x8, always only blend with the
// left neighbor(s) (skip blending with the above side).
#define DISABLE_CHROMA_U8X8_OBMC 0  // 0: one-sided obmc; 1: disable

int av1_skip_u4x4_pred_in_obmc(BLOCK_SIZE bsize,
                               const struct macroblockd_plane *pd, int dir) {
  const BLOCK_SIZE bsize_plane =
      get_plane_block_size(bsize, pd->subsampling_x, pd->subsampling_y);
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
#if CONFIG_NEW_REF_SIGNALING
  mbmi->ref_frame_nrs[1] = -1;
#endif  // CONFIG_NEW_REF_SIGNALING
  mbmi->interinter_comp.type = COMPOUND_AVERAGE;

  return;
}

struct obmc_inter_pred_ctxt {
  uint8_t **adjacent;
  int *adjacent_stride;
};

static INLINE void build_obmc_inter_pred_above(
    MACROBLOCKD *xd, int rel_mi_row, int rel_mi_col, uint8_t op_mi_size,
    int dir, MB_MODE_INFO *above_mi, void *fun_ctxt, const int num_planes) {
  (void)above_mi;
  (void)rel_mi_row;
  (void)dir;
  struct obmc_inter_pred_ctxt *ctxt = (struct obmc_inter_pred_ctxt *)fun_ctxt;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int overlap =
      AOMMIN(block_size_high[bsize], block_size_high[BLOCK_64X64]) >> 1;

  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = (op_mi_size * MI_SIZE) >> pd->subsampling_x;
    const int bh = overlap >> pd->subsampling_y;
    const int plane_col = (rel_mi_col * MI_SIZE) >> pd->subsampling_x;

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 0)) continue;

    const int dst_stride = pd->dst.stride;
    uint8_t *const dst = &pd->dst.buf[plane_col];
    const int tmp_stride = ctxt->adjacent_stride[plane];
    const uint8_t *const tmp = &ctxt->adjacent[plane][plane_col];
    const uint8_t *const mask = av1_get_obmc_mask(bh);
    const int is_hbd = is_cur_buf_hbd(xd);
    if (is_hbd)
      aom_highbd_blend_a64_vmask(dst, dst_stride, dst, dst_stride, tmp,
                                 tmp_stride, mask, bw, bh, xd->bd);
    else
      aom_blend_a64_vmask(dst, dst_stride, dst, dst_stride, tmp, tmp_stride,
                          mask, bw, bh);
  }
}

static INLINE void build_obmc_inter_pred_left(
    MACROBLOCKD *xd, int rel_mi_row, int rel_mi_col, uint8_t op_mi_size,
    int dir, MB_MODE_INFO *left_mi, void *fun_ctxt, const int num_planes) {
  (void)left_mi;
  (void)rel_mi_col;
  (void)dir;
  struct obmc_inter_pred_ctxt *ctxt = (struct obmc_inter_pred_ctxt *)fun_ctxt;
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;
  const int overlap =
      AOMMIN(block_size_wide[bsize], block_size_wide[BLOCK_64X64]) >> 1;

  for (int plane = 0; plane < num_planes; ++plane) {
    const struct macroblockd_plane *pd = &xd->plane[plane];
    const int bw = overlap >> pd->subsampling_x;
    const int bh = (op_mi_size * MI_SIZE) >> pd->subsampling_y;
    const int plane_row = (rel_mi_row * MI_SIZE) >> pd->subsampling_y;

    if (av1_skip_u4x4_pred_in_obmc(bsize, pd, 1)) continue;

    const int dst_stride = pd->dst.stride;
    uint8_t *const dst = &pd->dst.buf[plane_row * dst_stride];
    const int tmp_stride = ctxt->adjacent_stride[plane];
    const uint8_t *const tmp = &ctxt->adjacent[plane][plane_row * tmp_stride];
    const uint8_t *const mask = av1_get_obmc_mask(bw);

    const int is_hbd = is_cur_buf_hbd(xd);
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
                                     uint8_t *above[MAX_MB_PLANE],
                                     int above_stride[MAX_MB_PLANE],
                                     uint8_t *left[MAX_MB_PLANE],
                                     int left_stride[MAX_MB_PLANE]) {
  const BLOCK_SIZE bsize = xd->mi[0]->sb_type;

  // handle above row
  struct obmc_inter_pred_ctxt ctxt_above = { above, above_stride };
  foreach_overlappable_nb_above(cm, xd,
                                max_neighbor_obmc[mi_size_wide_log2[bsize]],
                                build_obmc_inter_pred_above, &ctxt_above);

  // handle left column
  struct obmc_inter_pred_ctxt ctxt_left = { left, left_stride };
  foreach_overlappable_nb_left(cm, xd,
                               max_neighbor_obmc[mi_size_high_log2[bsize]],
                               build_obmc_inter_pred_left, &ctxt_left);
}

void av1_setup_obmc_dst_bufs(MACROBLOCKD *xd, uint8_t **dst_buf1,
                             uint8_t **dst_buf2) {
  if (is_cur_buf_hbd(xd)) {
    int len = sizeof(uint16_t);
    dst_buf1[0] = CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[0]);
    dst_buf1[1] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE * len);
    dst_buf1[2] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE * 2 * len);
    dst_buf2[0] = CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[1]);
    dst_buf2[1] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE * len);
    dst_buf2[2] =
        CONVERT_TO_BYTEPTR(xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE * 2 * len);
  } else {
    dst_buf1[0] = xd->tmp_obmc_bufs[0];
    dst_buf1[1] = xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE;
    dst_buf1[2] = xd->tmp_obmc_bufs[0] + MAX_SB_SQUARE * 2;
    dst_buf2[0] = xd->tmp_obmc_bufs[1];
    dst_buf2[1] = xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE;
    dst_buf2[2] = xd->tmp_obmc_bufs[1] + MAX_SB_SQUARE * 2;
  }
}

void av1_setup_build_prediction_by_above_pred(
    MACROBLOCKD *xd, int rel_mi_col, uint8_t above_mi_width,
    MB_MODE_INFO *above_mbmi, struct build_prediction_ctxt *ctxt,
    const int num_planes) {
  const int above_mi_col = xd->mi_col + rel_mi_col;

  av1_modify_neighbor_predictor_for_obmc(above_mbmi);

  for (int j = 0; j < num_planes; ++j) {
    struct macroblockd_plane *const pd = &xd->plane[j];
    setup_pred_plane(&pd->dst, ctxt->tmp_buf[j], ctxt->tmp_width[j],
                     ctxt->tmp_height[j], ctxt->tmp_stride[j], 0, rel_mi_col,
                     NULL, pd->subsampling_x, pd->subsampling_y, NULL);
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
    av1_setup_pre_planes(xd, ref, &ref_buf->buf, xd->mi_row, above_mi_col, sf,
                         num_planes, NULL);
  }

  xd->mb_to_left_edge = 8 * MI_SIZE * (-above_mi_col);
  xd->mb_to_right_edge =
      ctxt->mb_to_far_edge +
      (xd->width - rel_mi_col - above_mi_width) * MI_SIZE * 8;
}

void av1_setup_build_prediction_by_left_pred(MACROBLOCKD *xd, int rel_mi_row,
                                             uint8_t left_mi_height,
                                             MB_MODE_INFO *left_mbmi,
                                             struct build_prediction_ctxt *ctxt,
                                             const int num_planes) {
  const int left_mi_row = xd->mi_row + rel_mi_row;

  av1_modify_neighbor_predictor_for_obmc(left_mbmi);

  for (int j = 0; j < num_planes; ++j) {
    struct macroblockd_plane *const pd = &xd->plane[j];
    setup_pred_plane(&pd->dst, ctxt->tmp_buf[j], ctxt->tmp_width[j],
                     ctxt->tmp_height[j], ctxt->tmp_stride[j], rel_mi_row, 0,
                     NULL, pd->subsampling_x, pd->subsampling_y, NULL);
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
    av1_setup_pre_planes(xd, ref, &ref_buf->buf, left_mi_row, xd->mi_col,
                         ref_scale_factors, num_planes, NULL);
  }

  xd->mb_to_top_edge = GET_MV_SUBPEL(MI_SIZE * (-left_mi_row));
  xd->mb_to_bottom_edge =
      ctxt->mb_to_far_edge +
      GET_MV_SUBPEL((xd->height - rel_mi_row - left_mi_height) * MI_SIZE);
}

static AOM_INLINE void combine_interintra(
    INTERINTRA_MODE mode, int8_t use_wedge_interintra, int8_t wedge_index,
    int8_t wedge_sign, BLOCK_SIZE bsize, BLOCK_SIZE plane_bsize,
    uint8_t *comppred, int compstride, const uint8_t *interpred,
    int interstride, const uint8_t *intrapred, int intrastride) {
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];

  if (use_wedge_interintra) {
    if (av1_is_wedge_used(bsize)) {
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

  const uint8_t *mask = smooth_interintra_mask_buf[mode][plane_bsize];
  aom_blend_a64_mask(comppred, compstride, intrapred, intrastride, interpred,
                     interstride, mask, bw, bw, bh, 0, 0);
}

static AOM_INLINE void combine_interintra_highbd(
    INTERINTRA_MODE mode, int8_t use_wedge_interintra, int8_t wedge_index,
    int8_t wedge_sign, BLOCK_SIZE bsize, BLOCK_SIZE plane_bsize,
    uint8_t *comppred8, int compstride, const uint8_t *interpred8,
    int interstride, const uint8_t *intrapred8, int intrastride, int bd) {
  const int bw = block_size_wide[plane_bsize];
  const int bh = block_size_high[plane_bsize];

  if (use_wedge_interintra) {
    if (av1_is_wedge_used(bsize)) {
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
                                               MACROBLOCKD *xd, int plane,
                                               const BUFFER_SET *ctx,
                                               uint8_t *dst, int dst_stride) {
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int ssx = xd->plane[plane].subsampling_x;
  const int ssy = xd->plane[plane].subsampling_y;
  BLOCK_SIZE plane_bsize = get_mb_plane_block_size(xd->mi[0], plane, ssx, ssy);
  PREDICTION_MODE mode = interintra_to_intra_mode[xd->mi[0]->interintra_mode];
  assert(xd->mi[0]->angle_delta[PLANE_TYPE_Y] == 0);
  assert(xd->mi[0]->angle_delta[PLANE_TYPE_UV] == 0);
  assert(xd->mi[0]->filter_intra_mode_info.use_filter_intra == 0);
  assert(xd->mi[0]->use_intrabc == 0);

  av1_predict_intra_block(
      cm, xd, pd->width, pd->height, max_txsize_rect_lookup[plane_bsize], mode,
      0, 0, FILTER_INTRA_MODES,
#if CONFIG_DERIVED_INTRA_MODE
      0,
#endif  // CONFIG_DERIVED_INTRA_MODE
      ctx->plane[plane], ctx->stride[plane], dst, dst_stride, 0, 0, plane);
}

void av1_combine_interintra(MACROBLOCKD *xd, BLOCK_SIZE bsize, int plane,
                            const uint8_t *inter_pred, int inter_stride,
                            const uint8_t *intra_pred, int intra_stride) {
  const int ssx = xd->plane[plane].subsampling_x;
  const int ssy = xd->plane[plane].subsampling_y;
  const BLOCK_SIZE plane_bsize =
      get_mb_plane_block_size(xd->mi[0], plane, ssx, ssy);

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
void av1_build_interintra_predictor(const AV1_COMMON *cm, MACROBLOCKD *xd,
                                    uint8_t *pred, int stride,
                                    const BUFFER_SET *ctx, int plane,
                                    BLOCK_SIZE bsize) {
  assert(bsize < BLOCK_SIZES_ALL);
  if (is_cur_buf_hbd(xd)) {
    DECLARE_ALIGNED(16, uint16_t, intrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(
        cm, xd, plane, ctx, CONVERT_TO_BYTEPTR(intrapredictor), MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, plane, pred, stride,
                           CONVERT_TO_BYTEPTR(intrapredictor), MAX_SB_SIZE);
  } else {
    DECLARE_ALIGNED(16, uint8_t, intrapredictor[MAX_SB_SQUARE]);
    av1_build_intra_predictors_for_interintra(cm, xd, plane, ctx,
                                              intrapredictor, MAX_SB_SIZE);
    av1_combine_interintra(xd, bsize, plane, pred, stride, intrapredictor,
                           MAX_SB_SIZE);
  }
}
