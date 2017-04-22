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

#ifndef AV1_ENCODER_PICKLPF_H_
#define AV1_ENCODER_PICKLPF_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "av1/encoder/encoder.h"

struct yv12_buffer_config;
struct Av1Comp;
int av1_get_max_filter_level(const Av1Comp *cpi);
int av1_search_filter_level(const YV12_BUFFER_CONFIG *sd, Av1Comp *cpi,
                            int partial_frame, double *err);
void av1_pick_filter_level(const struct yv12_buffer_config *sd,
                           struct Av1Comp *cpi, LPF_PICK_METHOD method);
#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_PICKLPF_H_
