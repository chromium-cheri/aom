/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/scan.h"
#include "av1/common/blockd.h"
#include "av1/common/idct.h"
#include "av1/common/pred_common.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/cost.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/subexp.h"
#include "av1/encoder/tokenize.h"

void av1_alloc_txb_buf(AV1_COMP *cpi) {
#if 0
  AV1_COMMON *cm = &cpi->common;
  int mi_block_size = 1 << MI_SIZE_LOG2;
  // TODO(angiebird): Make sure cm->subsampling_x/y is set correctly, and then
  // use precise buffer size according to cm->subsampling_x/y
  int pixel_stride = mi_block_size * cm->mi_cols;
  int pixel_height = mi_block_size * cm->mi_rows;
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    CHECK_MEM_ERROR(
        cm, cpi->tcoeff_buf[i],
        aom_malloc(sizeof(*cpi->tcoeff_buf[i]) * pixel_stride * pixel_height));
  }
#else
  (void)cpi;
#endif
}

void av1_free_txb_buf(AV1_COMP *cpi) {
#if 0
  int i;
  for (i = 0; i < MAX_MB_PLANE; ++i) {
    aom_free(cpi->tcoeff_buf[i]);
  }
#else
  (void)cpi;
#endif
}

static void write_golomb(aom_writer *w, int level) {
  int x = level + 1;
  int i = x;
  int length = 0;

  while (i) {
    i >>= 1;
    ++length;
  }
  assert(length > 0);

  for (i = 0; i < length - 1; ++i) aom_write_bit(w, 0);

  for (i = length - 1; i >= 0; --i) aom_write_bit(w, (x >> i) & 0x01);
}

void av1_write_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *xd,
                          aom_writer *w, int block, int plane,
                          const tran_low_t *tcoeff, uint16_t eob,
                          TXB_CTX *txb_ctx) {
  aom_prob *nz_map;
  aom_prob *eob_flag;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_SIZE tx_size = get_tx_size(plane, xd);
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  const int16_t *scan = scan_order->scan;
  int c;
  int is_nz;
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  const int seg_eob = tx_size_2d[tx_size];
  uint8_t txb_mask[32 * 32] = { 0 };
  uint16_t update_eob = 0;

  aom_write(w, eob == 0, cm->fc->txb_skip[tx_size][txb_ctx->txb_skip_ctx]);

  if (eob == 0) return;
#if CONFIG_TXK_SEL
  av1_write_tx_type(cm, xd, block, plane, w);
#endif

  nz_map = cm->fc->nz_map[tx_size][plane_type];
  eob_flag = cm->fc->eob_flag[tx_size][plane_type];

  for (c = 0; c < eob; ++c) {
    int coeff_ctx = get_nz_map_ctx(tcoeff, txb_mask, scan[c], bwl);
    int eob_ctx = get_eob_ctx(tcoeff, scan[c], bwl);

    tran_low_t v = tcoeff[scan[c]];
    is_nz = (v != 0);

    if (c == seg_eob - 1) break;

    aom_write(w, is_nz, nz_map[coeff_ctx]);

    if (is_nz) {
      aom_write(w, c == (eob - 1), eob_flag[eob_ctx]);
    }
    txb_mask[scan[c]] = 1;
  }

  int i;
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    aom_prob *coeff_base = cm->fc->coeff_base[tx_size][plane_type][i];

    update_eob = 0;
    for (c = eob - 1; c >= 0; --c) {
      tran_low_t v = tcoeff[scan[c]];
      tran_low_t level = abs(v);
      int sign = (v < 0) ? 1 : 0;
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(tcoeff, scan[c], bwl, i + 1);

      if (level == i + 1) {
        aom_write(w, 1, coeff_base[ctx]);
        if (c == 0) {
          aom_write(w, sign, cm->fc->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
        } else {
          aom_write_bit(w, sign);
        }
        continue;
      }
      aom_write(w, 0, coeff_base[ctx]);
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t v = tcoeff[scan[c]];
    tran_low_t level = abs(v);
    int sign = (v < 0) ? 1 : 0;
    int idx;
    int ctx;

    if (level <= NUM_BASE_LEVELS) continue;

    if (c == 0) {
      aom_write(w, sign, cm->fc->dc_sign[plane_type][txb_ctx->dc_sign_ctx]);
    } else {
      aom_write_bit(w, sign);
    }

    // level is above 1.
    ctx = get_br_ctx(tcoeff, scan[c], bwl);
    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (level == (idx + 1 + NUM_BASE_LEVELS)) {
        aom_write(w, 1, cm->fc->coeff_lps[tx_size][plane_type][ctx]);
        break;
      }
      aom_write(w, 0, cm->fc->coeff_lps[tx_size][plane_type][ctx]);
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // use 0-th order Golomb code to handle the residual level.
    write_golomb(w, level - COEFF_BASE_RANGE - 1 - NUM_BASE_LEVELS);
  }
}

void av1_write_coeffs_mb(const AV1_COMMON *const cm, MACROBLOCK *x,
                         aom_writer *w, int plane) {
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  BLOCK_SIZE bsize = mbmi->sb_type;
  struct macroblockd_plane *pd = &xd->plane[plane];

#if CONFIG_CB4X4
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);
#else
  const BLOCK_SIZE plane_bsize =
      get_plane_block_size(AOMMAX(bsize, BLOCK_8X8), pd);
#endif
  const int max_blocks_wide = max_block_wide(xd, plane_bsize, plane);
  const int max_blocks_high = max_block_high(xd, plane_bsize, plane);
  TX_SIZE tx_size = get_tx_size(plane, xd);
  const int bkw = tx_size_wide_unit[tx_size];
  const int bkh = tx_size_high_unit[tx_size];
  const int step = tx_size_wide_unit[tx_size] * tx_size_high_unit[tx_size];
  int row, col;
  int block = 0;
  for (row = 0; row < max_blocks_high; row += bkh) {
    for (col = 0; col < max_blocks_wide; col += bkw) {
      tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
      uint16_t eob = x->mbmi_ext->eobs[plane][block];
      TXB_CTX txb_ctx = { x->mbmi_ext->txb_skip_ctx[plane][block],
                          x->mbmi_ext->dc_sign_ctx[plane][block] };
      av1_write_coeffs_txb(cm, xd, w, block, plane, tcoeff, eob, &txb_ctx);
      block += step;
    }
  }
}

static INLINE void get_base_ctx_set(const tran_low_t *tcoeffs,
                                    int c,  // raster order
                                    const int bwl,
                                    int ctx_set[NUM_BASE_LEVELS]) {
  const int row = c >> bwl;
  const int col = c - (row << bwl);
  const int stride = 1 << bwl;
  int mag[NUM_BASE_LEVELS] = { 0 };
  int idx;
  tran_low_t abs_coeff;
  int i;

  for (idx = 0; idx < BASE_CONTEXT_POSITION_NUM; ++idx) {
    int ref_row = row + base_ref_offset[idx][0];
    int ref_col = col + base_ref_offset[idx][1];
    int pos = (ref_row << bwl) + ref_col;

    if (ref_row < 0 || ref_col < 0 || ref_row >= stride || ref_col >= stride)
      continue;

    abs_coeff = abs(tcoeffs[pos]);

    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      ctx_set[i] += abs_coeff > i;
      if (base_ref_offset[idx][0] >= 0 && base_ref_offset[idx][1] >= 0)
        mag[i] |= abs_coeff > (i + 1);
    }
  }

  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    ctx_set[i] = (ctx_set[i] + 1) >> 1;

    if (row == 0 && col == 0)
      ctx_set[i] = (ctx_set[i] << 1) + mag[i];
    else if (row == 0)
      ctx_set[i] = 8 + (ctx_set[i] << 1) + mag[i];
    else if (col == 0)
      ctx_set[i] = 18 + (ctx_set[i] << 1) + mag[i];
    else
      ctx_set[i] = 28 + (ctx_set[i] << 1) + mag[i];
  }
  return;
}

static INLINE int get_br_cost(tran_low_t abs_qc, int ctx,
                              const aom_prob *coeff_lps) {
  const tran_low_t min_level = 1 + NUM_BASE_LEVELS;
  const tran_low_t max_level = 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE;
  if (abs_qc >= min_level) {
    const int cost0 = av1_cost_bit(coeff_lps[ctx], 0);
    const int cost1 = av1_cost_bit(coeff_lps[ctx], 1);
    if (abs_qc >= max_level)
      return COEFF_BASE_RANGE * cost0;
    else
      return (abs_qc - min_level) * cost0 + cost1;
  } else {
    return 0;
  }
}

static INLINE int get_base_cost(tran_low_t abs_qc, int ctx,
                                aom_prob (*coeff_base)[COEFF_BASE_CONTEXTS],
                                int base_idx) {
  const int level = base_idx + 1;
  if (abs_qc < level)
    return 0;
  else
    return av1_cost_bit(coeff_base[base_idx][ctx], abs_qc == level);
}

int av1_cost_coeffs_txb(const AV1_COMP *const cpi, MACROBLOCK *x, int plane,
                        int block, TXB_CTX *txb_ctx) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCKD *const xd = &x->e_mbd;
  const TX_SIZE tx_size = get_tx_size(plane, xd);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  const struct macroblock_plane *p = &x->plane[plane];
  const int eob = p->eobs[block];
  const tran_low_t *const qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  int c, cost;
  const int seg_eob = AOMMIN(eob, tx_size_2d[tx_size] - 1);
  int txb_skip_ctx = txb_ctx->txb_skip_ctx;
  aom_prob *nz_map = xd->fc->nz_map[tx_size][plane_type];

  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  // txb_mask is only initialized for once here. After that, it will be set when
  // coding zero map and then reset when coding level 1 info.
  uint8_t txb_mask[32 * 32] = { 0 };
  aom_prob(*coeff_base)[COEFF_BASE_CONTEXTS] =
      xd->fc->coeff_base[tx_size][plane_type];

  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  const int16_t *scan = scan_order->scan;

  cost = 0;

  if (eob == 0) {
    cost = av1_cost_bit(xd->fc->txb_skip[tx_size][txb_skip_ctx], 1);
    return cost;
  }

  cost = av1_cost_bit(xd->fc->txb_skip[tx_size][txb_skip_ctx], 0);

#if CONFIG_TXK_SEL
  cost += av1_tx_type_cost(cpi, xd, mbmi->sb_type, plane, tx_size, tx_type);
#endif

  for (c = 0; c < eob; ++c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);
    int level = abs(v);

    if (c < seg_eob) {
      int coeff_ctx = get_nz_map_ctx(qcoeff, txb_mask, scan[c], bwl);
      cost += av1_cost_bit(nz_map[coeff_ctx], is_nz);
    }

    if (is_nz) {
      int ctx_ls[NUM_BASE_LEVELS] = { 0 };
      int sign = (v < 0) ? 1 : 0;

      // sign bit cost
      if (c == 0) {
        int dc_sign_ctx = txb_ctx->dc_sign_ctx;

        cost += av1_cost_bit(xd->fc->dc_sign[plane_type][dc_sign_ctx], sign);
      } else {
        cost += av1_cost_bit(128, sign);
      }

      get_base_ctx_set(qcoeff, scan[c], bwl, ctx_ls);

      int i;
      for (i = 0; i < NUM_BASE_LEVELS; ++i) {
        if (level <= i) continue;

        if (level == i + 1) {
          cost += av1_cost_bit(coeff_base[i][ctx_ls[i]], 1);
          continue;
        }
        cost += av1_cost_bit(coeff_base[i][ctx_ls[i]], 0);
      }

      if (level > NUM_BASE_LEVELS) {
        int idx;
        int ctx;

        ctx = get_br_ctx(qcoeff, scan[c], bwl);

        for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
          if (level == (idx + 1 + NUM_BASE_LEVELS)) {
            cost +=
                av1_cost_bit(xd->fc->coeff_lps[tx_size][plane_type][ctx], 1);
            break;
          }
          cost += av1_cost_bit(xd->fc->coeff_lps[tx_size][plane_type][ctx], 0);
        }

        if (idx >= COEFF_BASE_RANGE) {
          // residual cost
          int r = level - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
          int ri = r;
          int length = 0;

          while (ri) {
            ri >>= 1;
            ++length;
          }

          for (ri = 0; ri < length - 1; ++ri) cost += av1_cost_bit(128, 0);

          for (ri = length - 1; ri >= 0; --ri)
            cost += av1_cost_bit(128, (r >> ri) & 0x01);
        }
      }

      if (c < seg_eob) {
        int eob_ctx = get_eob_ctx(qcoeff, scan[c], bwl);
        cost += av1_cost_bit(xd->fc->eob_flag[tx_size][plane_type][eob_ctx],
                             c == (eob - 1));
      }
    }

    txb_mask[scan[c]] = 1;
  }

  return cost;
}

static INLINE int has_base(tran_low_t qc, int base_idx) {
  const int level = base_idx + 1;
  return abs(qc) >= level;
}

static void gen_base_count_mag_arr(int (*base_count_arr)[MAX_TX_SQUARE],
                                   int (*base_mag_arr)[2],
                                   const tran_low_t *qcoeff, int stride,
                                   int eob, const int16_t *scan) {
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    if (!has_base(qcoeff[coeff_idx], 0)) continue;
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    int *mag = base_mag_arr[coeff_idx];
    get_mag(mag, qcoeff, stride, row, col, base_ref_offset,
            BASE_CONTEXT_POSITION_NUM);
    for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
      if (!has_base(qcoeff[coeff_idx], i)) continue;
      int *count = base_count_arr[i] + coeff_idx;
      *count = get_level_count(qcoeff, stride, row, col, i, base_ref_offset,
                               BASE_CONTEXT_POSITION_NUM);
    }
  }
}

static void gen_nz_count_arr(int(*nz_count_arr), const tran_low_t *qcoeff,
                             int stride, int eob,
                             const SCAN_ORDER *scan_order) {
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    nz_count_arr[coeff_idx] = get_nz_count(qcoeff, stride, row, col, iscan);
  }
}

static void gen_nz_ctx_arr(int (*nz_ctx_arr)[2], int(*nz_count_arr),
                           const tran_low_t *qcoeff, int bwl, int eob,
                           const SCAN_ORDER *scan_order) {
  const int16_t *scan = scan_order->scan;
  const int16_t *iscan = scan_order->iscan;
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    const int count = nz_count_arr[coeff_idx];
    nz_ctx_arr[coeff_idx][0] =
        get_nz_map_ctx_from_count(count, qcoeff, coeff_idx, bwl, iscan);
  }
}

static void gen_base_ctx_arr(int (*base_ctx_arr)[MAX_TX_SQUARE][2],
                             int (*base_count_arr)[MAX_TX_SQUARE],
                             int (*base_mag_arr)[2], const tran_low_t *qcoeff,
                             int stride, int eob, const int16_t *scan) {
  (void)qcoeff;
  for (int i = 0; i < NUM_BASE_LEVELS; ++i) {
    for (int c = 0; c < eob; ++c) {
      const int coeff_idx = scan[c];  // raster order
      if (!has_base(qcoeff[coeff_idx], i)) continue;
      const int row = coeff_idx / stride;
      const int col = coeff_idx % stride;
      const int count = base_count_arr[i][coeff_idx];
      const int *mag = base_mag_arr[coeff_idx];
      const int level = i + 1;
      base_ctx_arr[i][coeff_idx][0] =
          get_base_ctx_from_count_mag(row, col, count, mag[0], level);
    }
  }
}

static INLINE int has_br(tran_low_t qc) {
  return abs(qc) >= 1 + NUM_BASE_LEVELS;
}

static void gen_br_count_mag_arr(int *br_count_arr, int (*br_mag_arr)[2],
                                 const tran_low_t *qcoeff, int stride, int eob,
                                 const int16_t *scan) {
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    if (!has_br(qcoeff[coeff_idx])) continue;
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    int *count = br_count_arr + coeff_idx;
    int *mag = br_mag_arr[coeff_idx];
    *count = get_level_count(qcoeff, stride, row, col, NUM_BASE_LEVELS,
                             br_ref_offset, BR_CONTEXT_POSITION_NUM);
    get_mag(mag, qcoeff, stride, row, col, br_ref_offset,
            BR_CONTEXT_POSITION_NUM);
  }
}

static void gen_br_ctx_arr(int (*br_ctx_arr)[2], const int *br_count_arr,
                           int (*br_mag_arr)[2], const tran_low_t *qcoeff,
                           int stride, int eob, const int16_t *scan) {
  (void)qcoeff;
  for (int c = 0; c < eob; ++c) {
    const int coeff_idx = scan[c];  // raster order
    if (!has_br(qcoeff[coeff_idx])) continue;
    const int row = coeff_idx / stride;
    const int col = coeff_idx % stride;
    const int count = br_count_arr[coeff_idx];
    const int *mag = br_mag_arr[coeff_idx];
    br_ctx_arr[coeff_idx][0] =
        get_br_ctx_from_count_mag(row, col, count, mag[0]);
  }
}

static INLINE int get_sign_bit_cost(tran_low_t qc, int coeff_idx,
                                    const aom_prob *dc_sign_prob,
                                    int dc_sign_ctx) {
  const int sign = (qc < 0) ? 1 : 0;
  // sign bit cost
  if (coeff_idx == 0) {
    return av1_cost_bit(dc_sign_prob[dc_sign_ctx], sign);
  } else {
    return av1_cost_bit(128, sign);
  }
}
static INLINE int get_golomb_cost(int abs_qc) {
  if (abs_qc >= 1 + NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
    // residual cost
    int r = abs_qc - COEFF_BASE_RANGE - NUM_BASE_LEVELS;
    int ri = r;
    int length = 0;

    while (ri) {
      ri >>= 1;
      ++length;
    }

    return av1_cost_literal(2 * length - 1);
  } else {
    return 0;
  }
}

// TODO(angiebird): add static once this function is called
void gen_txb_cache(TxbCache *txb_cache, TxbInfo *txb_info) {
  const int16_t *scan = txb_info->scan_order->scan;
  gen_nz_count_arr(txb_cache->nz_count_arr, txb_info->qcoeff, txb_info->stride,
                   txb_info->eob, txb_info->scan_order);
  gen_nz_ctx_arr(txb_cache->nz_ctx_arr, txb_cache->nz_count_arr,
                 txb_info->qcoeff, txb_info->bwl, txb_info->eob,
                 txb_info->scan_order);
  gen_base_count_mag_arr(txb_cache->base_count_arr, txb_cache->base_mag_arr,
                         txb_info->qcoeff, txb_info->stride, txb_info->eob,
                         scan);
  gen_base_ctx_arr(txb_cache->base_ctx_arr, txb_cache->base_count_arr,
                   txb_cache->base_mag_arr, txb_info->qcoeff, txb_info->stride,
                   txb_info->eob, scan);
  gen_br_count_mag_arr(txb_cache->br_count_arr, txb_cache->br_mag_arr,
                       txb_info->qcoeff, txb_info->stride, txb_info->eob, scan);
  gen_br_ctx_arr(txb_cache->br_ctx_arr, txb_cache->br_count_arr,
                 txb_cache->br_mag_arr, txb_info->qcoeff, txb_info->stride,
                 txb_info->eob, scan);
}

static int get_coeff_cost(tran_low_t qc, int scan_idx, TxbInfo *txb_info,
                          TxbProbs *txb_probs) {
  const TXB_CTX *txb_ctx = txb_info->txb_ctx;
  const int is_nz = (qc != 0);
  const tran_low_t abs_qc = abs(qc);
  int cost = 0;
  const int16_t *scan = txb_info->scan_order->scan;
  const int16_t *iscan = txb_info->scan_order->iscan;

  if (scan_idx < txb_info->seg_eob) {
    int coeff_ctx =
        get_nz_map_ctx2(txb_info->qcoeff, scan[scan_idx], txb_info->bwl, iscan);
    cost += av1_cost_bit(txb_probs->nz_map[coeff_ctx], is_nz);
  }

  if (is_nz) {
    cost += get_sign_bit_cost(qc, scan_idx, txb_probs->dc_sign_prob,
                              txb_ctx->dc_sign_ctx);

    int ctx_ls[NUM_BASE_LEVELS] = { 0 };
    get_base_ctx_set(txb_info->qcoeff, scan[scan_idx], txb_info->bwl, ctx_ls);

    int i;
    for (i = 0; i < NUM_BASE_LEVELS; ++i) {
      cost += get_base_cost(abs_qc, ctx_ls[i], txb_probs->coeff_base, i);
    }

    if (abs_qc > NUM_BASE_LEVELS) {
      int ctx = get_br_ctx(txb_info->qcoeff, scan[scan_idx], txb_info->bwl);
      cost += get_br_cost(abs_qc, ctx, txb_probs->coeff_lps);
      cost += get_golomb_cost(abs_qc);
    }

    if (scan_idx < txb_info->seg_eob) {
      int eob_ctx =
          get_eob_ctx(txb_info->qcoeff, scan[scan_idx], txb_info->bwl);
      cost += av1_cost_bit(txb_probs->eob_flag[eob_ctx],
                           scan_idx == (txb_info->eob - 1));
    }
  }
  return cost;
}

// TODO(angiebird): make this static once it's called
int get_txb_cost(TxbInfo *txb_info, TxbProbs *txb_probs) {
  int cost = 0;
  int txb_skip_ctx = txb_info->txb_ctx->txb_skip_ctx;
  const int16_t *scan = txb_info->scan_order->scan;
  if (txb_info->eob == 0) {
    cost = av1_cost_bit(txb_probs->txb_skip[txb_skip_ctx], 1);
    return cost;
  }
  cost = av1_cost_bit(txb_probs->txb_skip[txb_skip_ctx], 0);
  for (int c = 0; c < txb_info->eob; ++c) {
    tran_low_t qc = txb_info->qcoeff[scan[c]];
    int coeff_cost = get_coeff_cost(qc, c, txb_info, txb_probs);
    cost += coeff_cost;
  }
  return cost;
}

int av1_get_txb_entropy_context(const tran_low_t *qcoeff,
                                const SCAN_ORDER *scan_order, int eob) {
  const int16_t *scan = scan_order->scan;
  int cul_level = 0;
  int c;
  for (c = 0; c < eob; ++c) {
    cul_level += abs(qcoeff[scan[c]]);
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);
  set_dc_sign(&cul_level, qcoeff[0]);

  return cul_level;
}

void av1_update_txb_context_b(int plane, int block, int blk_row, int blk_col,
                              BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                              void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  const uint16_t eob = p->eobs[block];
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  const PLANE_TYPE plane_type = pd->plane_type;
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  (void)plane_bsize;

  int cul_level = av1_get_txb_entropy_context(qcoeff, scan_order, eob);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, blk_col, blk_row);
}

void av1_update_and_record_txb_context(int plane, int block, int blk_row,
                                       int blk_col, BLOCK_SIZE plane_bsize,
                                       TX_SIZE tx_size, void *arg) {
  struct tokenize_b_args *const args = arg;
  const AV1_COMP *cpi = args->cpi;
  const AV1_COMMON *cm = &cpi->common;
  ThreadData *const td = args->td;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  struct macroblock_plane *p = &x->plane[plane];
  struct macroblockd_plane *pd = &xd->plane[plane];
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  int eob = p->eobs[block], update_eob = 0;
  const PLANE_TYPE plane_type = pd->plane_type;
  const tran_low_t *qcoeff = BLOCK_OFFSET(p->qcoeff, block);
  tran_low_t *tcoeff = BLOCK_OFFSET(x->mbmi_ext->tcoeff[plane], block);
  const int segment_id = mbmi->segment_id;
  const TX_TYPE tx_type = get_tx_type(plane_type, xd, block, tx_size);
  const SCAN_ORDER *const scan_order =
      get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
  const int16_t *scan = scan_order->scan;
  const int seg_eob = get_tx_eob(&cpi->common.seg, segment_id, tx_size);
  int c, i;
  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + blk_col,
              pd->left_context + blk_row, &txb_ctx);
  const int bwl = b_width_log2_lookup[txsize_to_bsize[tx_size]] + 2;
  int cul_level = 0;
  unsigned int(*nz_map_count)[SIG_COEF_CONTEXTS][2];
  uint8_t txb_mask[32 * 32] = { 0 };

  nz_map_count = &td->counts->nz_map[tx_size][plane_type];

  memcpy(tcoeff, qcoeff, sizeof(*tcoeff) * seg_eob);

  ++td->counts->txb_skip[tx_size][txb_ctx.txb_skip_ctx][eob == 0];
  x->mbmi_ext->txb_skip_ctx[plane][block] = txb_ctx.txb_skip_ctx;

  x->mbmi_ext->eobs[plane][block] = eob;

  if (eob == 0) {
    av1_set_contexts(xd, pd, plane, tx_size, 0, blk_col, blk_row);
    return;
  }

#if CONFIG_TXK_SEL
  av1_update_tx_type_count(cm, xd, block, plane, mbmi->sb_type, tx_size,
                           td->counts);
#endif

  for (c = 0; c < eob; ++c) {
    tran_low_t v = qcoeff[scan[c]];
    int is_nz = (v != 0);
    int coeff_ctx = get_nz_map_ctx(tcoeff, txb_mask, scan[c], bwl);
    int eob_ctx = get_eob_ctx(tcoeff, scan[c], bwl);

    if (c == seg_eob - 1) break;

    ++(*nz_map_count)[coeff_ctx][is_nz];

    if (is_nz) {
      ++td->counts->eob_flag[tx_size][plane_type][eob_ctx][c == (eob - 1)];
    }
    txb_mask[scan[c]] = 1;
  }

  // Reverse process order to handle coefficient level and sign.
  for (i = 0; i < NUM_BASE_LEVELS; ++i) {
    update_eob = 0;
    for (c = eob - 1; c >= 0; --c) {
      tran_low_t v = qcoeff[scan[c]];
      tran_low_t level = abs(v);
      int ctx;

      if (level <= i) continue;

      ctx = get_base_ctx(tcoeff, scan[c], bwl, i + 1);

      if (level == i + 1) {
        ++td->counts->coeff_base[tx_size][plane_type][i][ctx][1];
        if (c == 0) {
          int dc_sign_ctx = txb_ctx.dc_sign_ctx;

          ++td->counts->dc_sign[plane_type][dc_sign_ctx][v < 0];
          x->mbmi_ext->dc_sign_ctx[plane][block] = dc_sign_ctx;
        }
        cul_level += level;
        continue;
      }
      ++td->counts->coeff_base[tx_size][plane_type][i][ctx][0];
      update_eob = AOMMAX(update_eob, c);
    }
  }

  for (c = update_eob; c >= 0; --c) {
    tran_low_t v = qcoeff[scan[c]];
    tran_low_t level = abs(v);
    int idx;
    int ctx;

    if (level <= NUM_BASE_LEVELS) continue;

    cul_level += level;
    if (c == 0) {
      int dc_sign_ctx = txb_ctx.dc_sign_ctx;

      ++td->counts->dc_sign[plane_type][dc_sign_ctx][v < 0];
      x->mbmi_ext->dc_sign_ctx[plane][block] = dc_sign_ctx;
    }

    // level is above 1.
    ctx = get_br_ctx(tcoeff, scan[c], bwl);
    for (idx = 0; idx < COEFF_BASE_RANGE; ++idx) {
      if (level == (idx + 1 + NUM_BASE_LEVELS)) {
        ++td->counts->coeff_lps[tx_size][plane_type][ctx][1];
        break;
      }
      ++td->counts->coeff_lps[tx_size][plane_type][ctx][0];
    }
    if (idx < COEFF_BASE_RANGE) continue;

    // use 0-th order Golomb code to handle the residual level.
  }

  cul_level = AOMMIN(COEFF_CONTEXT_MASK, cul_level);

  // DC value
  set_dc_sign(&cul_level, tcoeff[0]);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, blk_col, blk_row);

#if CONFIG_ADAPT_SCAN
  // Since dqcoeff is not available here, we pass qcoeff into
  // av1_update_scan_count_facade(). The update behavior should be the same
  // because av1_update_scan_count_facade() only cares if coefficients are zero
  // or not.
  av1_update_scan_count_facade((AV1_COMMON *)cm, td->counts, tx_size, tx_type,
                               qcoeff, eob);
#endif
}

void av1_update_txb_context(const AV1_COMP *cpi, ThreadData *td,
                            RUN_TYPE dry_run, BLOCK_SIZE bsize, int *rate,
                            int mi_row, int mi_col) {
  const AV1_COMMON *const cm = &cpi->common;
  MACROBLOCK *const x = &td->mb;
  MACROBLOCKD *const xd = &x->e_mbd;
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int ctx = av1_get_skip_context(xd);
  const int skip_inc =
      !segfeature_active(&cm->seg, mbmi->segment_id, SEG_LVL_SKIP);
  struct tokenize_b_args arg = { cpi, td, NULL, 0 };
  (void)rate;
  (void)mi_row;
  (void)mi_col;
  if (mbmi->skip) {
    if (!dry_run) td->counts->skip[ctx][1] += skip_inc;
    av1_reset_skip_context(xd, mi_row, mi_col, bsize);
    return;
  }

  if (!dry_run) {
    td->counts->skip[ctx][0] += skip_inc;
    av1_foreach_transformed_block(xd, bsize, mi_row, mi_col,
                                  av1_update_and_record_txb_context, &arg);
  } else if (dry_run == DRY_RUN_NORMAL) {
    av1_foreach_transformed_block(xd, bsize, mi_row, mi_col,
                                  av1_update_txb_context_b, &arg);
  } else {
    printf("DRY_RUN_COSTCOEFFS is not supported yet\n");
    assert(0);
  }
}

static void find_new_prob(unsigned int *branch_cnt, aom_prob *oldp,
                          int *savings, int *update, aom_writer *const bc) {
  const aom_prob upd = DIFF_UPDATE_PROB;
  int u = 0;
  aom_prob newp = get_binary_prob(branch_cnt[0], branch_cnt[1]);
  int s = av1_prob_diff_update_savings_search(branch_cnt, *oldp, &newp, upd, 1);

  if (s > 0 && newp != *oldp) u = 1;

  if (u)
    *savings += s - (int)(av1_cost_zero(upd));  // TODO(jingning): 1?
  else
    *savings -= (int)(av1_cost_zero(upd));

  if (update) {
    ++update[u];
    return;
  }

  aom_write(bc, u, upd);
  if (u) {
    /* send/use new probability */
    av1_write_prob_diff_update(bc, newp, *oldp);
    *oldp = newp;
  }
}

static void write_txb_probs(aom_writer *const bc, AV1_COMP *cpi,
                            TX_SIZE tx_size) {
  FRAME_CONTEXT *fc = cpi->common.fc;
  FRAME_COUNTS *counts = cpi->td.counts;
  int savings = 0;
  int update[2] = { 0, 0 };
  int plane, ctx, level;

  for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx) {
    find_new_prob(counts->txb_skip[tx_size][ctx], &fc->txb_skip[tx_size][ctx],
                  &savings, update, bc);
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->nz_map[tx_size][plane][ctx],
                    &fc->nz_map[tx_size][plane][ctx], &savings, update, bc);
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->eob_flag[tx_size][plane][ctx],
                    &fc->eob_flag[tx_size][plane][ctx], &savings, update, bc);
    }
  }

  for (level = 0; level < NUM_BASE_LEVELS; ++level) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx) {
        find_new_prob(counts->coeff_base[tx_size][plane][level][ctx],
                      &fc->coeff_base[tx_size][plane][level][ctx], &savings,
                      update, bc);
      }
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
      find_new_prob(counts->coeff_lps[tx_size][plane][ctx],
                    &fc->coeff_lps[tx_size][plane][ctx], &savings, update, bc);
    }
  }

  // Decide if to update the model for this tx_size
  if (update[1] == 0 || savings < 0) {
    aom_write_bit(bc, 0);
    return;
  }
  aom_write_bit(bc, 1);

  for (ctx = 0; ctx < TXB_SKIP_CONTEXTS; ++ctx) {
    find_new_prob(counts->txb_skip[tx_size][ctx], &fc->txb_skip[tx_size][ctx],
                  &savings, NULL, bc);
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < SIG_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->nz_map[tx_size][plane][ctx],
                    &fc->nz_map[tx_size][plane][ctx], &savings, NULL, bc);
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < EOB_COEF_CONTEXTS; ++ctx) {
      find_new_prob(counts->eob_flag[tx_size][plane][ctx],
                    &fc->eob_flag[tx_size][plane][ctx], &savings, NULL, bc);
    }
  }

  for (level = 0; level < NUM_BASE_LEVELS; ++level) {
    for (plane = 0; plane < PLANE_TYPES; ++plane) {
      for (ctx = 0; ctx < COEFF_BASE_CONTEXTS; ++ctx) {
        find_new_prob(counts->coeff_base[tx_size][plane][level][ctx],
                      &fc->coeff_base[tx_size][plane][level][ctx], &savings,
                      NULL, bc);
      }
    }
  }

  for (plane = 0; plane < PLANE_TYPES; ++plane) {
    for (ctx = 0; ctx < LEVEL_CONTEXTS; ++ctx) {
      find_new_prob(counts->coeff_lps[tx_size][plane][ctx],
                    &fc->coeff_lps[tx_size][plane][ctx], &savings, NULL, bc);
    }
  }
}

void av1_write_txb_probs(AV1_COMP *cpi, aom_writer *w) {
  const TX_MODE tx_mode = cpi->common.tx_mode;
  const TX_SIZE max_tx_size = tx_mode_to_biggest_tx_size[tx_mode];
  TX_SIZE tx_size;
  int ctx, plane;

  for (plane = 0; plane < PLANE_TYPES; ++plane)
    for (ctx = 0; ctx < DC_SIGN_CONTEXTS; ++ctx)
      av1_cond_prob_diff_update(w, &cpi->common.fc->dc_sign[plane][ctx],
                                cpi->td.counts->dc_sign[plane][ctx], 1);

  for (tx_size = TX_4X4; tx_size <= max_tx_size; ++tx_size)
    write_txb_probs(w, cpi, tx_size);
}

#if CONFIG_TXK_SEL
int64_t av1_search_txk_type(const AV1_COMP *cpi, MACROBLOCK *x, int plane,
                            int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size,
                            const ENTROPY_CONTEXT *a, const ENTROPY_CONTEXT *l,
                            int use_fast_coef_costing, RD_STATS *rd_stats) {
  const AV1_COMMON *cm = &cpi->common;
  MACROBLOCKD *xd = &x->e_mbd;
  MB_MODE_INFO *mbmi = &xd->mi[0]->mbmi;
  TX_TYPE txk_start = DCT_DCT;
  TX_TYPE txk_end = TX_TYPES - 1;
  TX_TYPE best_tx_type = txk_start;
  int64_t best_rd = INT64_MAX;
  const int coeff_ctx = combine_entropy_contexts(*a, *l);
  TX_TYPE tx_type;
  for (tx_type = txk_start; tx_type <= txk_end; ++tx_type) {
    if (plane == 0) mbmi->txk_type[block] = tx_type;
    TX_TYPE ref_tx_type =
        get_tx_type(get_plane_type(plane), xd, block, tx_size);
    if (tx_type != ref_tx_type) {
      // use get_tx_type() to check if the tx_type is valid for the current mode
      // if it's not, we skip it here.
      continue;
    }
    RD_STATS this_rd_stats;
    av1_invalid_rd_stats(&this_rd_stats);
    av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                    coeff_ctx, AV1_XFORM_QUANT_FP);
    av1_optimize_b(cm, x, plane, block, tx_size, coeff_ctx);
    av1_dist_block(cpi, x, plane, plane_bsize, block, blk_row, blk_col, tx_size,
                   &this_rd_stats.dist, &this_rd_stats.sse,
                   OUTPUT_HAS_PREDICTED_PIXELS);
    const SCAN_ORDER *scan_order =
        get_scan(cm, tx_size, tx_type, is_inter_block(mbmi));
    this_rd_stats.rate = av1_cost_coeffs(
        cpi, x, plane, block, tx_size, scan_order, a, l, use_fast_coef_costing);
    int rd =
        RDCOST(x->rdmult, x->rddiv, this_rd_stats.rate, this_rd_stats.dist);
    if (rd < best_rd) {
      best_rd = rd;
      *rd_stats = this_rd_stats;
      best_tx_type = tx_type;
    }
  }
  if (plane == 0) mbmi->txk_type[block] = best_tx_type;
  // TODO(angiebird): Instead of re-call av1_xform_quant and av1_optimize_b,
  // copy the best result in the above tx_type search for loop
  av1_xform_quant(cm, x, plane, block, blk_row, blk_col, plane_bsize, tx_size,
                  coeff_ctx, AV1_XFORM_QUANT_FP);
  av1_optimize_b(cm, x, plane, block, tx_size, coeff_ctx);
  if (!is_inter_block(mbmi)) {
    // intra mode needs decoded result such that the next transform block
    // can use it for prediction.
    av1_inverse_transform_block_facade(xd, plane, block, blk_row, blk_col,
                                       x->plane[plane].eobs[block]);
  }
  return best_rd;
}
#endif  // CONFIG_TXK_SEL
