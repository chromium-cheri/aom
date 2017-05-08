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

#ifndef ENCODETXB_H_
#define ENCODETXB_H_

#include "./aom_config.h"
#include "av1/common/blockd.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/txb_common.h"
#include "av1/encoder/block.h"
#include "av1/encoder/encoder.h"
#include "aom_dsp/bitwriter.h"
#ifdef __cplusplus
extern "C" {
#endif
void av1_alloc_txb_buf(Av1Comp *cpi);
void av1_free_txb_buf(Av1Comp *cpi);
int av1_cost_coeffs_txb(const Av1Comp *const cpi, Macroblock *x, int plane,
                        int block, TxbCtx *TxbCtx);
void av1_write_coeffs_txb(const Av1Common *const cm, Macroblockd *xd,
                          AomWriter *w, int block, int plane,
                          const TranLowT *tcoeff, uint16_t eob, TxbCtx *TxbCtx);
void av1_write_coeffs_mb(const Av1Common *const cm, Macroblock *x, AomWriter *w,
                         int plane);
int av1_get_txb_entropy_context(const TranLowT *qcoeff,
                                const ScanOrder *scan_order, int eob);
void av1_update_txb_context(const Av1Comp *cpi, ThreadData *td, RunType dry_run,
                            BlockSize bsize, int *rate, const int mi_row,
                            const int mi_col);
void av1_write_txb_probs(Av1Comp *cpi, AomWriter *w);

#if CONFIG_TXK_SEL
int64_t av1_search_txk_type(const Av1Comp *cpi, Macroblock *x, int plane,
                            int block, int blk_row, int blk_col,
                            BlockSize plane_bsize, TxSize tx_size,
                            const EntropyContext *a, const EntropyContext *l,
                            int use_fast_coef_costing, RdStats *rd_stats);
#endif
#ifdef __cplusplus
}
#endif

#endif  // COEFFS_CODING_H_
