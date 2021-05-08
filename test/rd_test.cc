/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <math.h>
#include <algorithm>
#include <vector>

#include "av1/common/quant_common.h"
#include "av1/encoder/rd.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace {

TEST(RdTest, GetDeltaqOffsetValueTest1) {
  int bit_depth = 8;
  double beta = 4;
  int q_index = 29;
  int dc_q_step =
      av1_dc_quant_QTX(q_index, 0, static_cast<aom_bit_depth_t>(bit_depth));
  EXPECT_EQ(dc_q_step, 32);

  int ref_new_dc_q_step = (int)round(dc_q_step / sqrt(beta));
  EXPECT_EQ(ref_new_dc_q_step, 16);

  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  int new_dc_q_step = av1_dc_quant_QTX(q_index, delta_q,
                                       static_cast<aom_bit_depth_t>(bit_depth));

  EXPECT_EQ(new_dc_q_step, ref_new_dc_q_step);
}

TEST(RdTest, GetDeltaqOffsetValueTest2) {
  int bit_depth = 8;
  double beta = 1.0 / 4.0;
  int q_index = 29;
  int dc_q_step =
      av1_dc_quant_QTX(q_index, 0, static_cast<aom_bit_depth_t>(bit_depth));
  EXPECT_EQ(dc_q_step, 32);

  int ref_new_dc_q_step = (int)round(dc_q_step / sqrt(beta));
  EXPECT_EQ(ref_new_dc_q_step, 64);

  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  int new_dc_q_step = av1_dc_quant_QTX(q_index, delta_q,
                                       static_cast<aom_bit_depth_t>(bit_depth));

  EXPECT_EQ(new_dc_q_step, ref_new_dc_q_step);
}

TEST(RdTest, GetDeltaqOffsetBoundaryTest1) {
  int bit_depth = 8;
  double beta = 0.000000001;
  int q_index = 254;
  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  EXPECT_EQ(q_index + delta_q, 255);
}

TEST(RdTest, GetDeltaqOffsetBoundaryTest2) {
  int bit_depth = 8;
  double beta = 0.000000001;
  int q_index = 255;
  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  EXPECT_EQ(q_index + delta_q, 255);
}

TEST(RdTest, GetDeltaqOffsetBoundaryTest3) {
  int bit_depth = 8;
  double beta = 4;
  int q_index = 0;
  int delta_q = av1_get_deltaq_offset(bit_depth, q_index, beta);
  EXPECT_EQ(delta_q, 0);
}
}  // namespace
