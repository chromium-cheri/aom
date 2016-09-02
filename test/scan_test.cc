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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "av1/common/scan.h"
#include "test/acm_random.h"
#include "third_party/googletest/src/include/gtest/gtest.h"

using libaom_test::ACMRandom;

namespace {

TEST(scan_test, augment_prob) {
  int tx1d_size = 4;
  uint32_t prob[16] = { 8, 8, 7, 7, 8, 8, 4, 2, 3, 3, 2, 2, 2, 2, 2, 2 };
  uint32_t ref_prob[16] = { 8, 8, 7, 7, 8, 8, 4, 2, 3, 3, 2, 2, 2, 2, 2, 2 };
  augment_prob(prob, tx1d_size, tx1d_size);
  for (int r = 0; r < tx1d_size; ++r) {
    for (int c = 0; c < tx1d_size; ++c) {
      int idx = r * tx1d_size + c;
      EXPECT_EQ(ref_prob[idx], prob[idx] >> 16);
    }
  }

  int mask = (1 << 10) - 1;
  for (int r = 0; r < tx1d_size; ++r) {
    for (int c = 0; c < tx1d_size; ++c) {
      int idx = r * tx1d_size + c;
      EXPECT_EQ(idx, mask ^ (prob[r * tx1d_size + c] & mask));
    }
  }
}

TEST(scan_test, update_sort_order) {
  int tx_size = TX_4X4;
  uint32_t prob[16] = { 8, 8, 7, 7, 8, 8, 4, 2, 3, 3, 2, 2, 2, 2, 2, 2 };
  int16_t ref_sort_order[16] = { 0, 1,  4, 5,  2,  3,  6,  8,
                                 9, 12, 7, 10, 13, 11, 14, 15 };
  int16_t sort_order[16];
  update_sort_order(tx_size, prob, sort_order);
  for (int i = 0; i < 16; ++i) EXPECT_EQ(ref_sort_order[i], sort_order[i]);
}

TEST(scan_test, update_scan_order) {
  int tx_size = TX_4X4;
  uint32_t prob[16] = { 4, 5, 7, 4, 5, 6, 8, 2, 3, 3, 2, 2, 2, 2, 2, 2 };
  int16_t sort_order[16];
  int16_t scan[16];
  int16_t iscan[16];
  int16_t ref_iscan[16] = {
    0, 1, 2, 6, 3, 4, 5, 10, 7, 8, 11, 13, 9, 12, 14, 15
  };

  update_sort_order(tx_size, prob, sort_order);
  update_scan_order(tx_size, sort_order, scan, iscan);

  for (int i = 0; i < 16; ++i) EXPECT_EQ(ref_iscan[i], iscan[i]);

  for (int i = 0; i < 16; ++i) EXPECT_EQ(i, scan[ref_iscan[i]]);
}

TEST(scan_test, update_neighbors) {
  int tx_size = TX_4X4;
  // raster order
  int16_t scan[16] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 };
  int16_t nb[(16 + 1) * 2];
  int16_t ref_nb[(16 + 1) * 2] = { 0, 0, 0,  0, 1,  1,  2,  2,  0, 0, 4,  1,
                                   5, 2, 6,  3, 4,  4,  8,  5,  9, 6, 10, 7,
                                   8, 8, 12, 9, 13, 10, 14, 11, 0, 0 };

  // raster order's scan and iscan are the same
  update_neighbors(tx_size, scan, scan, nb);
  for (int i = 0; i < (16 + 1) * 2; ++i) {
    EXPECT_EQ(ref_nb[i], nb[i]);
  }
}

}  // namespace
