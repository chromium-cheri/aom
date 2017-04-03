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

#include "test/warp_filter_test_util.h"

using std::tr1::tuple;
using std::tr1::make_tuple;
using std::vector;
using libaom_test::ACMRandom;
using libaom_test::AV1WarpFilter::AV1WarpFilterTest;
using libaom_test::AV1WarpFilter::WarpTestParam;

::testing::internal::ParamGenerator<WarpTestParam>
libaom_test::AV1WarpFilter::GetDefaultParams() {
  const WarpTestParam defaultParams[] = {
    make_tuple(4, 4, 50000),  make_tuple(8, 8, 50000),
    make_tuple(64, 64, 1000), make_tuple(4, 16, 20000),
    make_tuple(32, 8, 10000),
  };
  return ::testing::ValuesIn(defaultParams);
}

AV1WarpFilterTest::~AV1WarpFilterTest() {}
void AV1WarpFilterTest::SetUp() { rnd_.Reset(ACMRandom::DeterministicSeed()); }

void AV1WarpFilterTest::TearDown() { libaom_test::ClearSystemState(); }

int32_t AV1WarpFilterTest::random_param(int bits) {
  // 1 in 8 chance of generating zero (arbitrarily chosen)
  if (((rnd_.Rand8()) & 7) == 0) return 0;
  // Otherwise, enerate uniform values in the range
  // [-(1 << bits), 1] U [1, 1<<bits]
  int32_t v = 1 + (rnd_.Rand16() & ((1 << bits) - 1));
  if ((rnd_.Rand8()) & 1) return -v;
  return v;
}
void AV1WarpFilterTest::generate_model(int32_t *mat, int32_t *alpha,
                                       int32_t *beta, int32_t *gamma,
                                       int32_t *delta) {
  while (1) {
    mat[0] = random_param(WARPEDMODEL_PREC_BITS + 6);
    mat[1] = random_param(WARPEDMODEL_PREC_BITS + 6);
    mat[2] = (random_param(WARPEDMODEL_PREC_BITS - 3)) +
             (1 << WARPEDMODEL_PREC_BITS);
    mat[3] = random_param(WARPEDMODEL_PREC_BITS - 3);
    // 50/50 chance of generating ROTZOOM vs. AFFINE models
    if (rnd_.Rand8() & 1) {
      // AFFINE
      mat[4] = random_param(WARPEDMODEL_PREC_BITS - 3);
      mat[5] = (random_param(WARPEDMODEL_PREC_BITS - 3)) +
               (1 << WARPEDMODEL_PREC_BITS);
    } else {
      mat[4] = -mat[3];
      mat[5] = mat[2];
    }

    // Calculate the derived parameters and check that they are suitable
    // for the warp filter.
    assert(mat[2] != 0);

    *alpha = mat[2] - (1 << WARPEDMODEL_PREC_BITS);
    *beta = mat[3];
    *gamma = ((int64_t)mat[4] << WARPEDMODEL_PREC_BITS) / mat[2];
    *delta = mat[5] - (((int64_t)mat[3] * mat[4] + (mat[2] / 2)) / mat[2]) -
             (1 << WARPEDMODEL_PREC_BITS);

    if ((4 * abs(*alpha) + 7 * abs(*beta) > (1 << WARPEDMODEL_PREC_BITS)) ||
        (4 * abs(*gamma) + 7 * abs(*delta) > (1 << WARPEDMODEL_PREC_BITS)))
      continue;

    // We have a valid model, so finish
    return;
  }
}

void AV1WarpFilterTest::RunCheckOutput(warp_affine_func test_impl) {
  const int w = 128, h = 128;
  const int border = 16;
  const int stride = w + 2 * border;
  const int out_w = GET_PARAM(0), out_h = GET_PARAM(1);
  const int num_iters = GET_PARAM(2);
  int i, j, sub_x, sub_y;

  uint8_t *input_ = new uint8_t[h * stride];
  uint8_t *input = input_ + border;
  uint8_t *output = new uint8_t[out_w * out_h];
  uint8_t *output2 = new uint8_t[out_w * out_h];
  int32_t mat[8], alpha, beta, gamma, delta;

  // Generate an input block and extend its borders horizontally
  for (i = 0; i < h; ++i)
    for (j = 0; j < w; ++j) input[i * stride + j] = rnd_.Rand8();
  for (i = 0; i < h; ++i) {
    memset(input + i * stride - border, input[i * stride], border);
    memset(input + i * stride + w, input[i * stride + (w - 1)], border);
  }

  /* Try different sizes of prediction block */
  for (i = 0; i < num_iters; ++i) {
    for (sub_x = 0; sub_x < 2; ++sub_x)
      for (sub_y = 0; sub_y < 2; ++sub_y) {
        generate_model(mat, &alpha, &beta, &gamma, &delta);
        av1_warp_affine_c(mat, input, w, h, stride, output, 32, 32, out_w,
                          out_h, out_w, sub_x, sub_y, 0, alpha, beta, gamma,
                          delta);
        test_impl(mat, input, w, h, stride, output2, 32, 32, out_w, out_h,
                  out_w, sub_x, sub_y, 0, alpha, beta, gamma, delta);

        for (j = 0; j < out_w * out_h; ++j)
          ASSERT_EQ(output[j], output2[j])
              << "Pixel mismatch at index " << j << " = (" << (j % out_w)
              << ", " << (j / out_w) << ") on iteration " << i;
      }
  }
}
