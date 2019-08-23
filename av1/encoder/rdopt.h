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

#ifndef AOM_AV1_ENCODER_RDOPT_H_
#define AOM_AV1_ENCODER_RDOPT_H_

#include <stdbool.h>

#include "av1/common/blockd.h"
#include "av1/common/txb_common.h"

#include "av1/encoder/block.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_REF_MV_SEARCH 3
#define INTER_INTRA_RD_THRESH_SCALE 9
#define INTER_INTRA_RD_THRESH_SHIFT 4
#define COMP_TYPE_RD_THRESH_SCALE 11
#define COMP_TYPE_RD_THRESH_SHIFT 4

struct TileInfo;
struct macroblock;
struct RD_STATS;

enum {
  DEFAULT_EVAL = 0,
  MODE_EVAL,
  WINNER_MODE_EVAL,
} UENUM1BYTE(MODE_EVAL_TYPE);

#if CONFIG_RD_DEBUG
static INLINE void av1_update_txb_coeff_cost(RD_STATS *rd_stats, int plane,
                                             TX_SIZE tx_size, int blk_row,
                                             int blk_col, int txb_coeff_cost) {
  (void)blk_row;
  (void)blk_col;
  (void)tx_size;
  rd_stats->txb_coeff_cost[plane] += txb_coeff_cost;

  {
    const int txb_h = tx_size_high_unit[tx_size];
    const int txb_w = tx_size_wide_unit[tx_size];
    int idx, idy;
    for (idy = 0; idy < txb_h; ++idy)
      for (idx = 0; idx < txb_w; ++idx)
        rd_stats->txb_coeff_cost_map[plane][blk_row + idy][blk_col + idx] = 0;

    rd_stats->txb_coeff_cost_map[plane][blk_row][blk_col] = txb_coeff_cost;
  }
  assert(blk_row < TXB_COEFF_COST_MAP_SIZE);
  assert(blk_col < TXB_COEFF_COST_MAP_SIZE);
}
#endif

// Returns the number of colors in 'src'.
int av1_count_colors(const uint8_t *src, int stride, int rows, int cols,
                     int *val_count);
// Same as av1_count_colors(), but for high-bitdepth mode.
int av1_count_colors_highbd(const uint8_t *src8, int stride, int rows, int cols,
                            int bit_depth, int *val_count);

#if CONFIG_DIST_8X8
int64_t av1_dist_8x8(const struct AV1_COMP *const cpi, const MACROBLOCK *x,
                     const uint8_t *src, int src_stride, const uint8_t *dst,
                     int dst_stride, const BLOCK_SIZE tx_bsize, int bsw,
                     int bsh, int visible_w, int visible_h, int qindex);
#endif

static INLINE int av1_cost_skip_txb(MACROBLOCK *x, const TXB_CTX *const txb_ctx,
                                    int plane, TX_SIZE tx_size) {
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const LV_MAP_COEFF_COST *const coeff_costs =
      &x->coeff_costs[txs_ctx][plane_type];
  return coeff_costs->txb_skip_cost[txb_ctx->txb_skip_ctx][1];
}

static INLINE int av1_cost_coeffs(const AV1_COMMON *const cm, MACROBLOCK *x,
                                  int plane, int block, TX_SIZE tx_size,
                                  const TX_TYPE tx_type,
                                  const TXB_CTX *const txb_ctx,
                                  int use_fast_coef_costing) {
#if TXCOEFF_COST_TIMER
  struct aom_usec_timer timer;
  aom_usec_timer_start(&timer);
#endif
  (void)use_fast_coef_costing;
  const int cost =
      av1_cost_coeffs_txb(cm, x, plane, block, tx_size, tx_type, txb_ctx);
#if TXCOEFF_COST_TIMER
  AV1_COMMON *tmp_cm = (AV1_COMMON *)&cpi->common;
  aom_usec_timer_mark(&timer);
  const int64_t elapsed_time = aom_usec_timer_elapsed(&timer);
  tmp_cm->txcoeff_cost_timer += elapsed_time;
  ++tmp_cm->txcoeff_cost_count;
#endif
  return cost;
}

void av1_rd_pick_intra_mode_sb(const struct AV1_COMP *cpi, struct macroblock *x,
                               int mi_row, int mi_col, struct RD_STATS *rd_cost,
                               BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                               int64_t best_rd);

unsigned int av1_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                           const struct buf_2d *ref,
                                           BLOCK_SIZE bs);
unsigned int av1_high_get_sby_perpixel_variance(const struct AV1_COMP *cpi,
                                                const struct buf_2d *ref,
                                                BLOCK_SIZE bs, int bd);

#if !CONFIG_REALTIME_ONLY
void av1_rd_pick_inter_mode_sb(struct AV1_COMP *cpi,
                               struct TileDataEnc *tile_data,
                               struct macroblock *x, int mi_row, int mi_col,
                               struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                               PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);
#endif

void av1_fast_nonrd_pick_inter_mode_sb(struct AV1_COMP *cpi,
                                       struct TileDataEnc *tile_data,
                                       struct macroblock *x, int mi_row,
                                       int mi_col, struct RD_STATS *rd_cost,
                                       BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx,
                                       int64_t best_rd_so_far);

void av1_nonrd_pick_inter_mode_sb(struct AV1_COMP *cpi,
                                  struct TileDataEnc *tile_data,
                                  struct macroblock *x, int mi_row, int mi_col,
                                  struct RD_STATS *rd_cost, BLOCK_SIZE bsize,
                                  PICK_MODE_CONTEXT *ctx,
                                  int64_t best_rd_so_far);

void av1_rd_pick_inter_mode_sb_seg_skip(
    const struct AV1_COMP *cpi, struct TileDataEnc *tile_data,
    struct macroblock *x, int mi_row, int mi_col, struct RD_STATS *rd_cost,
    BLOCK_SIZE bsize, PICK_MODE_CONTEXT *ctx, int64_t best_rd_so_far);

// The best edge strength seen in the block, as well as the best x and y
// components of edge strength seen.
typedef struct {
  uint16_t magnitude;
  uint16_t x;
  uint16_t y;
} EdgeInfo;

/** Returns an integer indicating the strength of the edge.
 * 0 means no edge found, 556 is the strength of a solid black/white edge,
 * and the number may range higher if the signal is even stronger (e.g., on a
 * corner). high_bd is a bool indicating the source should be treated
 * as a 16-bit array. bd is the bit depth.
 */
EdgeInfo av1_edge_exists(const uint8_t *src, int src_stride, int w, int h,
                         bool high_bd, int bd);

/** Applies a Gaussian blur with sigma = 1.3. Used by av1_edge_exists and
 * tests.
 */
void av1_gaussian_blur(const uint8_t *src, int src_stride, int w, int h,
                       uint8_t *dst, bool high_bd, int bd);

/* Applies standard 3x3 Sobel matrix. */
typedef struct {
  int16_t x;
  int16_t y;
} sobel_xy;

sobel_xy av1_sobel(const uint8_t *input, int stride, int i, int j,
                   bool high_bd);

void av1_inter_mode_data_init(struct TileDataEnc *tile_data);
void av1_inter_mode_data_fit(TileDataEnc *tile_data, int rdmult);

typedef int64_t (*pick_interinter_mask_type)(
    const AV1_COMP *const cpi, MACROBLOCK *x, const BLOCK_SIZE bsize,
    const uint8_t *const p0, const uint8_t *const p1,
    const int16_t *const residual1, const int16_t *const diff10);

static INLINE int av1_encoder_get_relative_dist(const OrderHintInfo *oh, int a,
                                                int b) {
  if (!oh->enable_order_hint) return 0;

  assert(a >= 0 && b >= 0);
  return (a - b);
}

// This function will return number of mi's in a superblock.
static INLINE int av1_get_sb_mi_size(const AV1_COMMON *const cm) {
  const int mi_alloc_size_1d = mi_size_wide[cm->mi_alloc_bsize];
  int sb_mi_rows =
      (mi_size_wide[cm->seq_params.sb_size] + mi_alloc_size_1d - 1) /
      mi_alloc_size_1d;
  assert(mi_size_wide[cm->seq_params.sb_size] ==
         mi_size_high[cm->seq_params.sb_size]);
  int sb_mi_size = sb_mi_rows * sb_mi_rows;

  return sb_mi_size;
}

// This function will copy usable ref_mv_stack[ref_frame][4] and
// weight[ref_frame][4] information from ref_mv_stack[ref_frame][8] and
// weight[ref_frame][8].
static INLINE void av1_copy_usable_ref_mv_stack_and_weight(
    const MACROBLOCKD *xd, MB_MODE_INFO_EXT *const mbmi_ext,
    MV_REFERENCE_FRAME ref_frame) {
  memcpy(mbmi_ext->weight[ref_frame], xd->weight[ref_frame],
         USABLE_REF_MV_STACK_SIZE * sizeof(xd->weight[0][0]));
  memcpy(mbmi_ext->ref_mv_stack[ref_frame], xd->ref_mv_stack[ref_frame],
         USABLE_REF_MV_STACK_SIZE * sizeof(xd->ref_mv_stack[0][0]));
}

static TX_MODE select_tx_mode(
    const AV1_COMP *cpi, const TX_SIZE_SEARCH_METHOD tx_size_search_method) {
  if (cpi->common.coded_lossless) return ONLY_4X4;
  if (tx_size_search_method == USE_LARGESTALL)
    return TX_MODE_LARGEST;
  else if (tx_size_search_method == USE_FULL_RD ||
           tx_size_search_method == USE_FAST_RD)
    return TX_MODE_SELECT;
  else
    return cpi->common.tx_mode;
}

static INLINE void set_tx_size_search_method(
    const struct AV1_COMP *cpi, MACROBLOCK *x,
    int enable_winner_mode_for_tx_size_srch, int is_winner_mode) {
  // Populate transform size search method/transform mode appropriately
  if (enable_winner_mode_for_tx_size_srch && !is_winner_mode) {
    x->tx_size_search_method = USE_LARGESTALL;
  } else {
    x->tx_size_search_method = cpi->sf.tx_size_search_method;
  }
  x->tx_mode = select_tx_mode(cpi, x->tx_size_search_method);
}

static INLINE int is_winner_mode_processing_enabled(
    const struct AV1_COMP *cpi, MB_MODE_INFO *const mbmi,
    const PREDICTION_MODE best_mode) {
  const SPEED_FEATURES *sf = &cpi->sf;
  if (is_inter_block(mbmi)) {
    if (is_inter_mode(best_mode) &&
        sf->tx_type_search.fast_inter_tx_type_search &&
        !cpi->oxcf.use_inter_dct_only)
      return 1;
    if (!is_inter_mode(best_mode) &&
        sf->tx_type_search.fast_intra_tx_type_search &&
        !cpi->oxcf.use_intra_default_tx_only && !cpi->oxcf.use_intra_dct_only)
      return 1;
  } else {
    if (sf->tx_type_search.fast_intra_tx_type_search &&
        !cpi->oxcf.use_intra_default_tx_only)
      return 1;
  }

  if (sf->enable_winner_mode_for_coeff_opt &&
      cpi->optimize_seg_arr[mbmi->segment_id] != NO_TRELLIS_OPT &&
      cpi->optimize_seg_arr[mbmi->segment_id] != FINAL_PASS_TRELLIS_OPT)
    return 1;
  if (sf->enable_winner_mode_for_tx_size_srch) return 1;
  return 0;
}

static INLINE void set_final_winner_mode_params(const struct AV1_COMP *cpi,
                                                MACROBLOCK *x,
                                                MODE_EVAL_TYPE mode_eval_type) {
  const SPEED_FEATURES *sf = &cpi->sf;

  switch (mode_eval_type) {
    case DEFAULT_EVAL:
      x->coeff_opt_dist_threshold =
          get_rd_opt_coeff_thresh(cpi->coeff_opt_dist_threshold, 0, 0);
      set_tx_size_search_method(cpi, x, 0, 0);
      break;
    case MODE_EVAL:
      x->use_default_intra_tx_type =
          (cpi->sf.tx_type_search.fast_intra_tx_type_search ||
           cpi->oxcf.use_intra_default_tx_only);
      x->use_default_inter_tx_type =
          cpi->sf.tx_type_search.fast_inter_tx_type_search;

      x->coeff_opt_dist_threshold =
          get_rd_opt_coeff_thresh(cpi->coeff_opt_dist_threshold,
                                  sf->enable_winner_mode_for_coeff_opt, 0);
      set_tx_size_search_method(cpi, x, sf->enable_winner_mode_for_tx_size_srch,
                                0);
      break;
    case WINNER_MODE_EVAL:
      x->use_default_inter_tx_type = 0;
      x->use_default_intra_tx_type = 0;
      x->coeff_opt_dist_threshold =
          get_rd_opt_coeff_thresh(cpi->coeff_opt_dist_threshold,
                                  sf->enable_winner_mode_for_coeff_opt, 1);
      set_tx_size_search_method(cpi, x, sf->enable_winner_mode_for_tx_size_srch,
                                1);
      break;
    default: assert(0);
  }
}
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AOM_AV1_ENCODER_RDOPT_H_
