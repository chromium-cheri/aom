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

#ifndef AV1_ENCODER_ENCODEMB_H_
#define AV1_ENCODER_ENCODEMB_H_

#include "./aom_config.h"
#include "av1/common/onyxc_int.h"
#include "av1/encoder/block.h"

#ifdef __cplusplus
extern "C" {
#endif

struct encode_b_args {
  AV1_COMMON *cm;
  MACROBLOCK *x;
  struct optimize_ctx *ctx;
  int8_t *skip;
};
void av1_encode_sb(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
void av1_encode_sby_pass1(AV1_COMMON *cm, MACROBLOCK *x, BLOCK_SIZE bsize);
void av1_xform_quant_fp(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                        int block, int blk_row, int blk_col,
                        BLOCK_SIZE plane_bsize, TX_SIZE tx_size);
void av1_xform_quant_dc(MACROBLOCK *x, int plane, int block, int blk_row,
                        int blk_col, BLOCK_SIZE plane_bsize, TX_SIZE tx_size);
void av1_xform_quant(const AV1_COMMON *const cm, MACROBLOCK *x, int plane,
                     int block, int blk_row, int blk_col,
                     BLOCK_SIZE plane_bsize, TX_SIZE tx_size);

void av1_subtract_plane(MACROBLOCK *x, BLOCK_SIZE bsize, int plane);

void av1_encode_block_intra(int plane, int block, int blk_row, int blk_col,
                            BLOCK_SIZE plane_bsize, TX_SIZE tx_size, void *arg);

void av1_encode_intra_block_plane(AV1_COMMON *cm, MACROBLOCK *x,
                                  BLOCK_SIZE bsize, int plane);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODEMB_H_
