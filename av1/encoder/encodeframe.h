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

#ifndef AV1_ENCODER_ENCODEFRAME_H_
#define AV1_ENCODER_ENCODEFRAME_H_

#include "aom/aom_integer.h"
#include "av1/common/blockd.h"
#include "av1/common/enums.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Macroblock;
struct Yv12BufferConfig;
struct Av1Comp;
struct ThreadData;

// Constants used in SOURCE_VAR_BASED_PARTITION
#define VAR_HIST_MAX_BG_VAR 1000
#define VAR_HIST_FACTOR 10
#define VAR_HIST_BINS (VAR_HIST_MAX_BG_VAR / VAR_HIST_FACTOR + 1)
#define VAR_HIST_LARGE_CUT_OFF 75
#define VAR_HIST_SMALL_CUT_OFF 45

void av1_setup_src_planes(struct Macroblock *x,
                          const struct Yv12BufferConfig *src, int mi_row,
                          int mi_col);

void av1_encode_frame(struct Av1Comp *cpi);

void av1_init_tile_data(struct Av1Comp *cpi);
void av1_encode_tile(struct Av1Comp *cpi, struct ThreadData *td, int tile_row,
                     int tile_col);

void av1_set_variance_partition_thresholds(struct Av1Comp *cpi, int q);

void av1_update_tx_type_count(const struct AV1Common *cm, Macroblockd *xd,
#if CONFIG_TXK_SEL
                              int block, int plane,
#endif
                              BlockSize bsize, TxSize tx_size,
                              FrameCounts *counts);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_ENCODEFRAME_H_
