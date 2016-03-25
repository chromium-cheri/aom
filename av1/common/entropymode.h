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

#ifndef VP10_COMMON_ENTROPYMODE_H_
#define VP10_COMMON_ENTROPYMODE_H_

#include "av1/common/entropy.h"
#include "av1/common/entropymv.h"
#include "av1/common/filter.h"
#include "av1/common/seg_common.h"
#include "aom_dsp/aom_filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLOCK_SIZE_GROUPS 4

#define TX_SIZE_CONTEXTS 2

#define INTER_OFFSET(mode) ((mode)-NEARESTMV)

struct VP10Common;

struct tx_probs {
  aom_prob p32x32[TX_SIZE_CONTEXTS][TX_SIZES - 1];
  aom_prob p16x16[TX_SIZE_CONTEXTS][TX_SIZES - 2];
  aom_prob p8x8[TX_SIZE_CONTEXTS][TX_SIZES - 3];
};

struct tx_counts {
  unsigned int p32x32[TX_SIZE_CONTEXTS][TX_SIZES];
  unsigned int p16x16[TX_SIZE_CONTEXTS][TX_SIZES - 1];
  unsigned int p8x8[TX_SIZE_CONTEXTS][TX_SIZES - 2];
  unsigned int tx_totals[TX_SIZES];
};

struct seg_counts {
  unsigned int tree_total[MAX_SEGMENTS];
  unsigned int tree_mispred[MAX_SEGMENTS];
  unsigned int pred[PREDICTION_PROBS][2];
};

typedef struct frame_contexts {
  aom_prob y_mode_prob[BLOCK_SIZE_GROUPS][INTRA_MODES - 1];
  aom_prob uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
  aom_prob partition_prob[PARTITION_CONTEXTS][PARTITION_TYPES - 1];
  vp10_coeff_probs_model coef_probs[TX_SIZES][PLANE_TYPES];
  aom_prob
      switchable_interp_prob[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS -
                                                         1];
  aom_prob inter_mode_probs[INTER_MODE_CONTEXTS][INTER_MODES - 1];
  aom_prob intra_inter_prob[INTRA_INTER_CONTEXTS];
  aom_prob comp_inter_prob[COMP_INTER_CONTEXTS];
  aom_prob single_ref_prob[REF_CONTEXTS][2];
  aom_prob comp_ref_prob[REF_CONTEXTS];
  struct tx_probs tx_probs;
  aom_prob skip_probs[SKIP_CONTEXTS];
  nmv_context nmvc;
#if CONFIG_MISC_FIXES
  struct segmentation_probs seg;
#endif
  aom_prob intra_ext_tx_prob[EXT_TX_SIZES][TX_TYPES][TX_TYPES - 1];
  aom_prob inter_ext_tx_prob[EXT_TX_SIZES][TX_TYPES - 1];
  int initialized;
} FRAME_CONTEXT;

typedef struct FRAME_COUNTS {
  unsigned int kf_y_mode[INTRA_MODES][INTRA_MODES][INTRA_MODES];
  unsigned int y_mode[BLOCK_SIZE_GROUPS][INTRA_MODES];
  unsigned int uv_mode[INTRA_MODES][INTRA_MODES];
  unsigned int partition[PARTITION_CONTEXTS][PARTITION_TYPES];
  vp10_coeff_count_model coef[TX_SIZES][PLANE_TYPES];
  unsigned int
      eob_branch[TX_SIZES][PLANE_TYPES][REF_TYPES][COEF_BANDS][COEFF_CONTEXTS];
  unsigned int
      switchable_interp[SWITCHABLE_FILTER_CONTEXTS][SWITCHABLE_FILTERS];
  unsigned int inter_mode[INTER_MODE_CONTEXTS][INTER_MODES];
  unsigned int intra_inter[INTRA_INTER_CONTEXTS][2];
  unsigned int comp_inter[COMP_INTER_CONTEXTS][2];
  unsigned int single_ref[REF_CONTEXTS][2][2];
  unsigned int comp_ref[REF_CONTEXTS][2];
  struct tx_counts tx;
  unsigned int skip[SKIP_CONTEXTS][2];
  nmv_context_counts mv;
#if CONFIG_MISC_FIXES
  struct seg_counts seg;
#endif
  unsigned int intra_ext_tx[EXT_TX_SIZES][TX_TYPES][TX_TYPES];
  unsigned int inter_ext_tx[EXT_TX_SIZES][TX_TYPES];
} FRAME_COUNTS;

extern const aom_prob
    vp10_kf_y_mode_prob[INTRA_MODES][INTRA_MODES][INTRA_MODES - 1];
#if !CONFIG_MISC_FIXES
extern const aom_prob vp10_kf_uv_mode_prob[INTRA_MODES][INTRA_MODES - 1];
extern const aom_prob
    vp10_kf_partition_probs[PARTITION_CONTEXTS][PARTITION_TYPES - 1];
#endif

extern const aom_tree_index vp10_intra_mode_tree[TREE_SIZE(INTRA_MODES)];
extern const aom_tree_index vp10_inter_mode_tree[TREE_SIZE(INTER_MODES)];
extern const aom_tree_index vp10_partition_tree[TREE_SIZE(PARTITION_TYPES)];
extern const aom_tree_index
    vp10_switchable_interp_tree[TREE_SIZE(SWITCHABLE_FILTERS)];

void vp10_setup_past_independence(struct VP10Common *cm);

void vp10_adapt_intra_frame_probs(struct VP10Common *cm);
void vp10_adapt_inter_frame_probs(struct VP10Common *cm);

void vp10_tx_counts_to_branch_counts_32x32(const unsigned int *tx_count_32x32p,
                                           unsigned int (*ct_32x32p)[2]);
void vp10_tx_counts_to_branch_counts_16x16(const unsigned int *tx_count_16x16p,
                                           unsigned int (*ct_16x16p)[2]);
void vp10_tx_counts_to_branch_counts_8x8(const unsigned int *tx_count_8x8p,
                                         unsigned int (*ct_8x8p)[2]);

extern const aom_tree_index vp10_ext_tx_tree[TREE_SIZE(TX_TYPES)];

static INLINE int vp10_ceil_log2(int n) {
  int i = 1, p = 2;
  while (p < n) {
    i++;
    p = p << 1;
  }
  return i;
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // VP10_COMMON_ENTROPYMODE_H_
