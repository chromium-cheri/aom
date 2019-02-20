/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/common/alloccommon.h"
#include "av1/common/cdef.h"
#include "av1/common/filter.h"
#include "av1/common/idct.h"
#include "av1/common/reconinter.h"
#include "av1/common/reconintra.h"
#include "av1/common/resize.h"

#include "av1/encoder/aq_complexity.h"
#include "av1/encoder/aq_cyclicrefresh.h"
#include "av1/encoder/aq_variance.h"
#include "av1/encoder/bitstream.h"
#include "av1/encoder/context_tree.h"
#include "av1/encoder/encodeframe.h"
#include "av1/encoder/encodemv.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/encodetxb.h"
#include "av1/encoder/ethread.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/grain_test_vectors.h"
#include "av1/encoder/hash_motion.h"
#include "av1/encoder/mbgraph.h"
#include "av1/encoder/picklpf.h"
#include "av1/encoder/pickrst.h"
#include "av1/encoder/random.h"
#include "av1/encoder/ratectrl.h"
#include "av1/encoder/rd.h"
#include "av1/encoder/segmentation.h"
#include "av1/encoder/speed_features.h"
#include "av1/encoder/temporal_filter.h"

#include <stdint.h>

#ifndef CALL_TENSORFLOW
#define CALL_TENSORFLOW

uint8_t **call_tensorflow(uint8_t *ppp, int height, int width, int stride,
                          FRAME_TYPE frame_type);
uint8_t **block_call_tensorflow(uint8_t *ppp, int cur_buf_height,
                                int cur_buf_width, int stride,
                                FRAME_TYPE frame_type);
uint16_t **call_tensorflow_hbd(uint16_t *ppp, int height, int width, int stride,
                               FRAME_TYPE frame_type);
uint16_t **block_call_tensorflow_hbd(uint16_t *ppp, int cur_buf_height,
                                     int cur_buf_width, int stride,
                                     FRAME_TYPE frame_type);

#endif  // CALL_TENSORFLOW
