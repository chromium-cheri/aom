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

#include "av1/decoder/decodetxb.h"

#include "aom_ports/mem.h"
#include "av1/common/idct.h"
#include "av1/common/scan.h"
#include "av1/common/txb_common.h"
#include "av1/decoder/decodemv.h"

#define ACCT_STR __func__

static int read_golomb(MACROBLOCKD *xd, aom_reader *r) {
  int x = 1;
  int length = 0;
  int i = 0;

  while (!i) {
    i = aom_read_bit(r, ACCT_STR);
    ++length;
    if (length >= 32) {
      aom_internal_error(xd->error_info, AOM_CODEC_CORRUPT_FRAME,
                         "Invalid length in read_golomb");
      break;
    }
  }

  for (i = 0; i < length - 1; ++i) {
    x <<= 1;
    x += aom_read_bit(r, ACCT_STR);
  }

  return x - 1;
}

static INLINE int rec_eob_pos(const int eob_token, const int extra) {
  int eob = k_eob_group_start[eob_token];
  if (eob > 2) {
    eob += extra;
  }
  return eob;
}

static INLINE int get_dqv(const int16_t *dequant, int coeff_idx,
                          const qm_val_t *iqmatrix) {
  int dqv = dequant[!!coeff_idx];
#if CONFIG_AOM_QM
  if (iqmatrix != NULL)
    dqv =
        ((iqmatrix[coeff_idx] * dqv) + (1 << (AOM_QM_BITS - 1))) >> AOM_QM_BITS;
#else
  (void)iqmatrix;
#endif
  return dqv;
}

static INLINE void read_coeffs_reverse_2d(aom_reader *r, TX_SIZE tx_size,
                                          int start_si, int end_si,
                                          const int16_t *scan, int bwl,
                                          uint8_t *levels,
                                          base_cdf_arr base_cdf,
                                          br_cdf_arr br_cdf) {
  for (int c = end_si; c >= start_si; --c) {
    const int pos = scan[c];
    const int coeff_ctx = get_lower_levels_ctx_2d(levels, pos, bwl, tx_size);
    const int nsymbs = 4;
    int level = aom_read_symbol(r, base_cdf[coeff_ctx], nsymbs, ACCT_STR);
    if (level > NUM_BASE_LEVELS) {
      const int br_ctx = get_br_ctx_2d(levels, pos, bwl);
      aom_cdf_prob *cdf = br_cdf[br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = aom_read_symbol(r, cdf, BR_CDF_SIZE, ACCT_STR);
        level += k;
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    levels[get_padded_idx(pos, bwl)] = level;
  }
}

static INLINE void read_coeffs_reverse(aom_reader *r, TX_SIZE tx_size,
                                       TX_TYPE tx_type, int start_si,
                                       int end_si, const int16_t *scan, int bwl,
                                       uint8_t *levels, base_cdf_arr base_cdf,
                                       br_cdf_arr br_cdf) {
  for (int c = end_si; c >= start_si; --c) {
    const int pos = scan[c];
    const int coeff_ctx =
        get_lower_levels_ctx(levels, pos, bwl, tx_size, tx_type);
    const int nsymbs = 4;
    int level = aom_read_symbol(r, base_cdf[coeff_ctx], nsymbs, ACCT_STR);
    if (level > NUM_BASE_LEVELS) {
      const int br_ctx = get_br_ctx(levels, pos, bwl, tx_type);
      aom_cdf_prob *cdf = br_cdf[br_ctx];
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = aom_read_symbol(r, cdf, BR_CDF_SIZE, ACCT_STR);
        level += k;
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    levels[get_padded_idx(pos, bwl)] = level;
  }
}

uint8_t av1_read_coeffs_txb(const AV1_COMMON *const cm, MACROBLOCKD *const xd,
                            aom_reader *const r, const int blk_row,
                            const int blk_col, const int plane,
                            const TXB_CTX *const txb_ctx, const TX_SIZE tx_size,
                            int16_t *const max_scan_line, int *const eob) {
  FRAME_CONTEXT *const ec_ctx = xd->tile_ctx;
  const int32_t max_value = (1 << (7 + xd->bd)) - 1;
  const int32_t min_value = -(1 << (7 + xd->bd));
  const TX_SIZE txs_ctx = get_txsize_entropy_ctx(tx_size);
  const PLANE_TYPE plane_type = get_plane_type(plane);
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  const int seg_eob = av1_get_max_eob(tx_size);
  int num_updates = 0;
  struct macroblockd_plane *const pd = &xd->plane[plane];
  const int16_t *const dequant = pd->seg_dequant_QTX[mbmi->segment_id];
  tran_low_t *const tcoeffs = pd->dqcoeff;
  const int shift = av1_get_tx_scale(tx_size);
  const int bwl = get_txb_bwl(tx_size);
  const int width = get_txb_wide(tx_size);
  const int height = get_txb_high(tx_size);
  int cul_level = 0;
  uint8_t levels_buf[TX_PAD_2D];
  uint8_t *const levels = set_levels(levels_buf, width);
  uint16_t update_pos[MAX_TX_SQUARE];
  uint8_t update_sign[MAX_TX_SQUARE];

  const int all_zero = aom_read_symbol(
      r, ec_ctx->txb_skip_cdf[txs_ctx][txb_ctx->txb_skip_ctx], 2, ACCT_STR);
  // printf("txb_skip: %d %2d\n", txs_ctx, txb_ctx->txb_skip_ctx);
  *eob = 0;
  if (all_zero) {
    *max_scan_line = 0;
#if CONFIG_TXK_SEL
    if (plane == 0) {
      const int txk_type_idx =
          av1_get_txk_type_index(mbmi->sb_type, blk_row, blk_col);
      mbmi->txk_type[txk_type_idx] = DCT_DCT;
    }
#endif
    return 0;
  }

  memset(levels_buf, 0,
         sizeof(*levels_buf) *
             ((width + TX_PAD_HOR) * (height + TX_PAD_VER) + TX_PAD_END));

  (void)blk_row;
  (void)blk_col;
#if CONFIG_TXK_SEL
  av1_read_tx_type(cm, xd, blk_row, blk_col, plane, tx_size, r);
#endif
  const TX_TYPE tx_type = av1_get_tx_type(plane_type, xd, blk_row, blk_col,
                                          tx_size, cm->reduced_tx_set_used);
#if CONFIG_AOM_QM
  const TX_SIZE qm_tx_size = av1_get_adjusted_tx_size(tx_size);
  const qm_val_t *iqmatrix =
      IS_2D_TRANSFORM(tx_type)
          ? pd->seg_iqmatrix[mbmi->segment_id][qm_tx_size]
          : cm->giqmatrix[NUM_QM_LEVELS - 1][0][qm_tx_size];
#else
  const qm_val_t *iqmatrix = NULL;
#endif
  (void)iqmatrix;
  const SCAN_ORDER *const scan_order = get_scan(cm, tx_size, tx_type, mbmi);
  const int16_t *const scan = scan_order->scan;
  int dummy;
  const int max_eob_pt = get_eob_pos_token(seg_eob, &dummy);
  int eob_extra = 0;
  int eob_pt = 1;

  (void)max_eob_pt;
  const int eob_multi_size = txsize_log2_minus4[tx_size];
  const int eob_multi_ctx = (tx_type_to_class[tx_type] == TX_CLASS_2D) ? 0 : 1;
  switch (eob_multi_size) {
    case 0:
      eob_pt =
          aom_read_symbol(r, ec_ctx->eob_flag_cdf16[plane_type][eob_multi_ctx],
                          5, ACCT_STR) +
          1;
      break;
    case 1:
      eob_pt =
          aom_read_symbol(r, ec_ctx->eob_flag_cdf32[plane_type][eob_multi_ctx],
                          6, ACCT_STR) +
          1;
      break;
    case 2:
      eob_pt =
          aom_read_symbol(r, ec_ctx->eob_flag_cdf64[plane_type][eob_multi_ctx],
                          7, ACCT_STR) +
          1;
      break;
    case 3:
      eob_pt =
          aom_read_symbol(r, ec_ctx->eob_flag_cdf128[plane_type][eob_multi_ctx],
                          8, ACCT_STR) +
          1;
      break;
    case 4:
      eob_pt =
          aom_read_symbol(r, ec_ctx->eob_flag_cdf256[plane_type][eob_multi_ctx],
                          9, ACCT_STR) +
          1;
      break;
    case 5:
      eob_pt =
          aom_read_symbol(r, ec_ctx->eob_flag_cdf512[plane_type][eob_multi_ctx],
                          10, ACCT_STR) +
          1;
      break;
    case 6:
    default:
      eob_pt = aom_read_symbol(
                   r, ec_ctx->eob_flag_cdf1024[plane_type][eob_multi_ctx], 11,
                   ACCT_STR) +
               1;
      break;
  }

  if (k_eob_offset_bits[eob_pt] > 0) {
    int bit = aom_read_symbol(
        r, ec_ctx->eob_extra_cdf[txs_ctx][plane_type][eob_pt], 2, ACCT_STR);
    if (bit) {
      eob_extra += (1 << (k_eob_offset_bits[eob_pt] - 1));
    }

    for (int i = 1; i < k_eob_offset_bits[eob_pt]; i++) {
      bit = aom_read_bit(r, ACCT_STR);
      if (bit) {
        eob_extra += (1 << (k_eob_offset_bits[eob_pt] - 1 - i));
      }
    }
  }
  *eob = rec_eob_pos(eob_pt, eob_extra);

  {
    // Read the non-zero coefficient with scan index eob-1
    // TODO(angiebird): Put this into a function
    const int c = *eob - 1;
    const int pos = scan[c];
    const int coeff_ctx = get_lower_levels_ctx_eob(bwl, height, c);
    const int nsymbs = 3;
    aom_cdf_prob *cdf =
        ec_ctx->coeff_base_eob_cdf[txs_ctx][plane_type][coeff_ctx];
    int level = aom_read_symbol(r, cdf, nsymbs, ACCT_STR) + 1;
    if (level > NUM_BASE_LEVELS) {
      const int br_ctx = get_br_ctx(levels, pos, bwl, tx_type);
      for (int idx = 0; idx < COEFF_BASE_RANGE; idx += BR_CDF_SIZE - 1) {
        const int k = aom_read_symbol(
            r,
            ec_ctx->coeff_br_cdf[AOMMIN(txs_ctx, TX_32X32)][plane_type][br_ctx],
            BR_CDF_SIZE, ACCT_STR);
        level += k;
        if (k < BR_CDF_SIZE - 1) break;
      }
    }
    levels[get_padded_idx(pos, bwl)] = level;
  }
  base_cdf_arr base_cdf = ec_ctx->coeff_base_cdf[txs_ctx][plane_type];
  br_cdf_arr br_cdf =
      ec_ctx->coeff_br_cdf[AOMMIN(txs_ctx, TX_32X32)][plane_type];
  const TX_CLASS tx_class = tx_type_to_class[tx_type];
  if (tx_class == TX_CLASS_2D) {
    read_coeffs_reverse_2d(r, tx_size, 0, *eob - 1 - 1, scan, bwl, levels,
                           base_cdf, br_cdf);
  } else {
    read_coeffs_reverse(r, tx_size, tx_type, 0, *eob - 1 - 1, scan, bwl, levels,
                        base_cdf, br_cdf);
  }

  // Loop to decode all signs in the transform block,
  // starting with the sign of the DC (if applicable)
  for (int c = 0; c < *eob; ++c) {
    const int pos = scan[c];
    uint8_t sign;
    tran_low_t level = levels[get_padded_idx(pos, bwl)];
    if (level) {
      *max_scan_line = AOMMAX(*max_scan_line, pos);
      if (c == 0) {
        const int dc_sign_ctx = txb_ctx->dc_sign_ctx;
        sign = aom_read_symbol(r, ec_ctx->dc_sign_cdf[plane_type][dc_sign_ctx],
                               2, ACCT_STR);
      } else {
        sign = aom_read_bit(r, ACCT_STR);
      }
      if (level > NUM_BASE_LEVELS + COEFF_BASE_RANGE) {
        // the quantized coeff with golomb residue is stored in tcoeffs because
        // levels doesn't have enough bits got store the residue
        update_pos[num_updates] = pos;
        update_sign[num_updates] = sign;
        ++num_updates;
      } else {
        cul_level += level;
        tran_low_t dq_coeff;
        dq_coeff = level * get_dqv(dequant, scan[c], iqmatrix);
        dq_coeff = dq_coeff >> shift;
        if (sign) {
          dq_coeff = -dq_coeff;
        }
        tcoeffs[pos] = clamp(dq_coeff, min_value, max_value);
      }
    }
  }

  for (int i = 0; i < num_updates; ++i) {
    const int pos = update_pos[i];
    const int sign = update_sign[i];
    const int level = levels[get_padded_idx(pos, bwl)] + read_golomb(xd, r);
    cul_level += level;
    tran_low_t dq_coeff;
    dq_coeff = level * get_dqv(dequant, pos, iqmatrix);
    dq_coeff = dq_coeff >> shift;
    if (sign) {
      dq_coeff = -dq_coeff;
    }
    tcoeffs[pos] = clamp(dq_coeff, min_value, max_value);
  }

  cul_level = AOMMIN(63, cul_level);

  // DC value
  set_dc_sign(&cul_level, tcoeffs[0]);

  return cul_level;
}

uint8_t av1_read_coeffs_txb_facade(const AV1_COMMON *const cm,
                                   MACROBLOCKD *const xd, aom_reader *const r,
                                   const int row, const int col,
                                   const int plane, const TX_SIZE tx_size,
                                   int16_t *const max_scan_line,
                                   int *const eob) {
  MB_MODE_INFO *const mbmi = &xd->mi[0]->mbmi;
  struct macroblockd_plane *const pd = &xd->plane[plane];

  const BLOCK_SIZE bsize = mbmi->sb_type;
  const BLOCK_SIZE plane_bsize = get_plane_block_size(bsize, pd);

  TXB_CTX txb_ctx;
  get_txb_ctx(plane_bsize, tx_size, plane, pd->above_context + col,
              pd->left_context + row, &txb_ctx);
  uint8_t cul_level = av1_read_coeffs_txb(cm, xd, r, row, col, plane, &txb_ctx,
                                          tx_size, max_scan_line, eob);
  av1_set_contexts(xd, pd, plane, tx_size, cul_level, col, row);
  return cul_level;
}
