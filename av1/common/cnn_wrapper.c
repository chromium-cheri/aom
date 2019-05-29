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

#include <stdint.h>
#include <stdio.h>

#include "av1/common/cnn.h"
#include "av1/common/cnn_wrapper.h"
#include "av1/common/onyxc_int.h"

#include "av1/models/intra_frame_model/qp12.h"
#include "av1/models/intra_frame_model/qp22.h"
#include "av1/models/intra_frame_model/qp32.h"
#include "av1/models/intra_frame_model/qp43.h"
#include "av1/models/intra_frame_model/qp53.h"
#include "av1/models/intra_frame_model/qp63.h"

static void av1_restore_cnn_plane_Y_wrapper(
    AV1_COMMON *cm, int plane, const CNN_THREAD_DATA *thread_data) {
  // TODO(logangw): Add infrastructure to choose models.
  int qindex = cm->base_qindex;
  if (qindex <= MIN_CNN_Q_INDEX) {
    return;
  } else if (qindex < 68) {
    av1_restore_cnn_plane(cm, &intra_model_12, plane, thread_data);
  } else if (qindex < 108) {
    av1_restore_cnn_plane(cm, &intra_frame_model_qp22, plane, thread_data);
  } else if (qindex < 148) {
    av1_restore_cnn_plane(cm, &intra_frame_model_qp32, plane, thread_data);
  } else if (qindex < 192) {
    av1_restore_cnn_plane(cm, &intra_frame_model_qp43, plane, thread_data);
  } else if (qindex < 232) {
    av1_restore_cnn_plane(cm, &intra_frame_model_qp53, plane, thread_data);
  } else {
    av1_restore_cnn_plane(cm, &intra_frame_model_qp63, plane, thread_data);
  }
}

void av1_encode_restore_cnn(AV1_COMMON *cm, AVxWorker *workers,
                            int num_workers) {
  // TODO(logangw): Add mechanism to restore AOM_PLANE_U and AOM_PLANE_V.
  const CNN_THREAD_DATA thread_data = { num_workers, workers };
  av1_restore_cnn_plane_Y_wrapper(cm, AOM_PLANE_Y, &thread_data);
}

void av1_decode_restore_cnn(AV1_COMMON *cm, AVxWorker *workers,
                            int num_workers) {
  const CNN_THREAD_DATA thread_data = { num_workers, workers };
  av1_restore_cnn_plane_Y_wrapper(cm, AOM_PLANE_Y, &thread_data);
}
