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

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include <assert.h>
#include <string.h>

#include "av1/common/common.h"
#include "av1/common/enums.h"

#define MAX_SUB8X8_VAL (8)

// Forward declaration of AV1_COMMON, in order to avoid creating a cyclic
// dependency by importing av1/common/onyxc_int.h
typedef struct AV1Common AV1_COMMON;

// Forward declaration of MACROBLOCK, in order to avoid creating a cyclic
// dependency by importing av1/common/blockd.h
typedef struct macroblockd MACROBLOCKD;

typedef struct {
  // The CfL prediction buffer is used in two steps
  //   1. Stores Q3 reconstructed luma pixels
  //   2. Stores Q3 AC contributions (step1 - tx block avg)
  int16_t pred_buf_q3[MAX_SB_SQUARE];

  // Height and width currently used in the CfL prediction buffer.
  int buf_height, buf_width;

  // Height and width of the chroma prediction block currently associated with
  // this context
  int uv_height, uv_width;

  int are_parameters_computed;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  // Block level DC_PRED for each chromatic plane
  int dc_pred[CFL_PRED_PLANES];

  int mi_row, mi_col;

  // Whether the reconstructed luma pixels need to be stored
  int store_y;

#if CONFIG_CB4X4
  int is_chroma_reference;
#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
  // The prediction used for sub8x8 blocks originates from multiple luma blocks,
  // this array is used to validate that cfl_store() is called only once for
  // each luma block
  uint8_t sub8x8_val[MAX_SUB8X8_VAL];
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
#endif  // CONFIG_CB4X4
} CFL_CTX;

static INLINE int get_scaled_luma_q0(int alpha_q3, int16_t pred_buf_q3) {
  int scaled_luma_q6 = alpha_q3 * pred_buf_q3;
  return ROUND_POWER_OF_TWO_SIGNED(scaled_luma_q6, 6);
}

#if CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG
static INLINE void cfl_clear_sub8x8_val(CFL_CTX *cfl) {
  memset(cfl->sub8x8_val, 0, sizeof(cfl->sub8x8_val));
}
#endif  // CONFIG_CHROMA_SUB8X8 && CONFIG_DEBUG

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm);

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, int plane);

void cfl_store_block(MACROBLOCKD *const xd, BLOCK_SIZE bsize, TX_SIZE tx_size);

void cfl_store_tx(MACROBLOCKD *const xd, int row, int col, TX_SIZE tx_size,
                  BLOCK_SIZE bsize);

void cfl_compute_parameters(MACROBLOCKD *const xd, TX_SIZE tx_size);

#endif  // AV1_COMMON_CFL_H_
