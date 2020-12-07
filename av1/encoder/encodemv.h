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

#ifndef AOM_AV1_ENCODER_ENCODEMV_H_
#define AOM_AV1_ENCODER_ENCODEMV_H_

#include "av1/encoder/encoder.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_encode_mv(AV1_COMP *cpi, aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx, MvSubpelPrecision precision);

void av1_update_mv_stats(const MV *mv, const MV *ref, nmv_context *mvctx,
                         MvSubpelPrecision precision);

void av1_build_nmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context *mvctx,
                              MvSubpelPrecision precision);
void av1_build_dmv_cost_table(int *mvjoint, int *mvcost[2],
                              const nmv_context *mvctx,
                              MvSubpelPrecision precision);

void av1_update_mv_count(ThreadData *td);

void av1_encode_dv(aom_writer *w, const MV *mv, const MV *ref,
                   nmv_context *mvctx);
int_mv av1_get_ref_mv(const MACROBLOCK *x, int ref_idx);

int_mv av1_get_ref_mv_from_stack(int ref_idx,
                                 const MV_REFERENCE_FRAME *ref_frame,
                                 int ref_mv_idx,
                                 const MB_MODE_INFO_EXT *mbmi_ext);

#if CONFIG_NEW_INTER_MODES
int_mv av1_find_best_ref_mv_from_stack(MvSubpelPrecision precision,
                                       const MB_MODE_INFO_EXT *mbmi_ext,
                                       MV_REFERENCE_FRAME ref_frame);
#else
void av1_find_best_ref_mvs_from_stack(MvSubpelPrecision precision,
                                      const MB_MODE_INFO_EXT *mbmi_ext,
                                      MV_REFERENCE_FRAME ref_frame,
                                      int_mv *nearest_mv, int_mv *near_mv);
#endif  // CONFIG_NEW_INTER_MODES

static INLINE MV_JOINT_TYPE av1_get_mv_joint(const MV *mv) {
  if (mv->row == 0) {
    return mv->col == 0 ? MV_JOINT_ZERO : MV_JOINT_HNZVZ;
  } else {
    return mv->col == 0 ? MV_JOINT_HZVNZ : MV_JOINT_HNZVNZ;
  }
}

static INLINE int av1_check_newmv_joint_nonzero(const AV1_COMMON *cm,
                                                MACROBLOCK *const x) {
  (void)cm;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = xd->mi[0];
  const PREDICTION_MODE this_mode = mbmi->mode;
  MvSubpelPrecision precision = mbmi->max_mv_precision;
#if CONFIG_FLEX_MVRES
  if (is_pb_mv_precision_active(cm, this_mode, mbmi->max_mv_precision))
    precision = av1_get_mbmi_mv_precision(cm, mbmi);
#endif  // CONFIG_FLEX_MVRES
#if CONFIG_OPTFLOW_REFINEMENT
  if (this_mode == NEW_NEWMV || this_mode == NEW_NEWMV_OPTFLOW) {
#else
  if (this_mode == NEW_NEWMV) {
#endif
    int_mv ref_mv_0 = av1_get_ref_mv(x, 0);
    int_mv ref_mv_1 = av1_get_ref_mv(x, 1);
    lower_mv_precision(&ref_mv_0.as_mv, precision);
    lower_mv_precision(&ref_mv_1.as_mv, precision);
    if (mbmi->mv[0].as_int == ref_mv_0.as_int ||
        mbmi->mv[1].as_int == ref_mv_1.as_int) {
      return 0;
    }
#if CONFIG_NEW_INTER_MODES
#if CONFIG_OPTFLOW_REFINEMENT
  } else if (this_mode == NEAR_NEWMV || this_mode == NEAR_NEWMV_OPTFLOW) {
#else
  } else if (this_mode == NEAR_NEWMV) {
#endif
#else
  } else if (this_mode == NEAREST_NEWMV || this_mode == NEAR_NEWMV) {
#endif  // CONFIG_NEW_INTER_MODES
    int_mv ref_mv_1 = av1_get_ref_mv(x, 1);
    lower_mv_precision(&ref_mv_1.as_mv, precision);
    if (mbmi->mv[1].as_int == ref_mv_1.as_int) {
      return 0;
    }
#if CONFIG_NEW_INTER_MODES
#if CONFIG_OPTFLOW_REFINEMENT
  } else if (this_mode == NEW_NEARMV || this_mode == NEW_NEARMV_OPTFLOW) {
#else
  } else if (this_mode == NEW_NEARMV) {
#endif
#else
  } else if (this_mode == NEW_NEARESTMV || this_mode == NEW_NEARMV) {
#endif  // CONFIG_NEW_INTER_MODES
    int_mv ref_mv_0 = av1_get_ref_mv(x, 0);
    lower_mv_precision(&ref_mv_0.as_mv, precision);
    if (mbmi->mv[0].as_int == ref_mv_0.as_int) {
      return 0;
    }
  } else if (this_mode == NEWMV) {
    int_mv ref_mv_0 = av1_get_ref_mv(x, 0);
    lower_mv_precision(&ref_mv_0.as_mv, precision);
    if (mbmi->mv[0].as_int == ref_mv_0.as_int) {
      return 0;
    }
  }
  return 1;
}

static INLINE int av1_mv_class_base(MV_CLASS_TYPE c) {
  return c ? CLASS0_SIZE << (c + 2) : 0;
}

// If n != 0, returns the floor of log base 2 of n. If n == 0, returns 0.
static INLINE uint8_t av1_log_in_base_2(unsigned int n) {
  // get_msb() is only valid when n != 0.
  return n == 0 ? 0 : get_msb(n);
}

static INLINE MV_CLASS_TYPE av1_get_mv_class(int z, int *offset) {
  const MV_CLASS_TYPE c = (z >= CLASS0_SIZE * 4096)
                              ? MV_CLASS_10
                              : (MV_CLASS_TYPE)av1_log_in_base_2(z >> 3);
  if (offset) *offset = z - mv_class_base(c);
  return c;
}
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_ENCODEMV_H_
