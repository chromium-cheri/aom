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

#include <ctime>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "./av1_rtcd.h"
#include "test/acm_random.h"
#include "test/clear_system_state.h"
#include "test/register_state_check.h"
#include "test/util.h"

#include "av1/common/mv.h"
#include "av1/common/restoration.h"

namespace {

using std::tr1::tuple;
using std::tr1::make_tuple;
using libaom_test::ACMRandom;

typedef tuple<> FilterTestParam;

class AV1SelfguidedFilterTest
    : public ::testing::TestWithParam<FilterTestParam> {
 public:
  virtual ~AV1SelfguidedFilterTest() {}
  virtual void SetUp() {}

  virtual void TearDown() { libaom_test::ClearSystemState(); }

 protected:
  void RunSpeedTest() {
    const int w = 256, h = 256;
    const int NUM_ITERS = 2000;
    int i, j;

    uint8_t *input = new uint8_t[w * h];
    uint8_t *output = new uint8_t[w * h];
    int32_t *tmpbuf = (int32_t *)aom_malloc(RESTORATION_TMPBUF_SIZE);
    memset(tmpbuf, 0, RESTORATION_TMPBUF_SIZE);

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    for (i = 0; i < h; ++i)
      for (j = 0; j < w; ++j) input[i * w + j] = rnd.Rand16() & 0xFF;

    int xqd[2] = {
      SGRPROJ_PRJ_MIN0 +
          rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 - SGRPROJ_PRJ_MIN0),
      SGRPROJ_PRJ_MIN1 +
          rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 - SGRPROJ_PRJ_MIN1)
    };
    // Fix a parameter set, since the speed depends slightly on r.
    // Change this to test different combinations of values of r.
    int eps = 15;

    av1_loop_restoration_precal();

    std::clock_t start = std::clock();
    for (i = 0; i < NUM_ITERS; ++i) {
      apply_selfguided_restoration_c(input, w, h, w, 8, eps, xqd, output, w,
                                     tmpbuf);
    }
    std::clock_t end = std::clock();
    double elapsed = ((end - start) / (double)CLOCKS_PER_SEC);

    printf("%5d %dx%d blocks in %7.3fs = %7.3fus/block\n", NUM_ITERS, w, h,
           elapsed, elapsed * 1000000. / NUM_ITERS);

    aom_free(tmpbuf);
    delete[] input;
    delete[] output;
  }

  void RunCorrectnessTest() {
    const int w = 256, h = 256, stride = 672, out_stride = 672;
    const int NUM_ITERS = 81;
    int i, j, k;

    uint8_t *input = new uint8_t[stride * (h + 16)];
    uint8_t *output = new uint8_t[out_stride * (h + 16)];
    uint8_t *output2 = new uint8_t[out_stride * (h + 16)];
    int32_t *tmpbuf = (int32_t *)aom_malloc(RESTORATION_TMPBUF_SIZE);
    memset(tmpbuf, 0, RESTORATION_TMPBUF_SIZE);

    ACMRandom rnd(ACMRandom::DeterministicSeed());

    av1_loop_restoration_precal();

    for (i = 0; i < NUM_ITERS; ++i) {
      for (j = 0; j < h; ++j)
        for (k = 0; k < w; ++k) input[j * stride + k] = rnd.Rand16() & 0xFF;

      int xqd[2] = {
        SGRPROJ_PRJ_MIN0 +
            rnd.PseudoUniform(SGRPROJ_PRJ_MAX0 + 1 - SGRPROJ_PRJ_MIN0),
        SGRPROJ_PRJ_MIN1 +
            rnd.PseudoUniform(SGRPROJ_PRJ_MAX1 + 1 - SGRPROJ_PRJ_MIN1)
      };
      int eps = rnd.PseudoUniform(1 << SGRPROJ_PARAMS_BITS);

      // Test various tile sizes around 256x256
      int test_w = w + 4 - (i / 9);
      int test_h = h + 4 - (i % 9);

      apply_selfguided_restoration(input, test_w, test_h, stride, 8, eps, xqd,
                                   output, out_stride, tmpbuf);
      apply_selfguided_restoration_c(input, test_w, test_h, stride, 8, eps, xqd,
                                     output2, out_stride, tmpbuf);
      for (j = 0; j < test_h; ++j)
        for (k = 0; k < test_w; ++k)
          ASSERT_EQ(output[j * out_stride + k], output2[j * out_stride + k]);
    }

    aom_free(tmpbuf);
    delete[] input;
    delete[] output;
    delete[] output2;
  }
};

TEST_P(AV1SelfguidedFilterTest, SpeedTest) { RunSpeedTest(); }
TEST_P(AV1SelfguidedFilterTest, CorrectnessTest) { RunCorrectnessTest(); }

const FilterTestParam params[] = { make_tuple() };

#if HAVE_SSE4_1
INSTANTIATE_TEST_CASE_P(SSE4_1, AV1SelfguidedFilterTest,
                        ::testing::ValuesIn(params));
#endif

}  // namespace
