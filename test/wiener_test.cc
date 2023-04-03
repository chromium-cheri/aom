/*
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tuple>
#include <utility>
#include <vector>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "test/register_state_check.h"
#include "test/acm_random.h"
#include "test/util.h"

#include "config/aom_config.h"
#include "config/aom_dsp_rtcd.h"

#include "aom/aom_integer.h"
#include "aom_ports/aom_timer.h"
#include "av1/encoder/pickrst.h"

#define MAX_WIENER_BLOCK 384
#define MAX_DATA_BLOCK (MAX_WIENER_BLOCK + WIENER_WIN)

// 8-bit-depth tests
namespace wiener_lowbd {

// C implementation of the algorithm implmented by the SIMD code.
// This is a little more efficient than the version in av1_compute_stats_c().
static void compute_stats_win_opt_c(int wiener_win, const uint8_t *dgd,
                                    const uint8_t *src, int h_start, int h_end,
                                    int v_start, int v_end, int dgd_stride,
                                    int src_stride, int64_t *M, int64_t *H,
                                    int use_downsampled_wiener_stats) {
  ASSERT_TRUE(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);
  int i, j, k, l, m, n;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  uint8_t avg = find_average(dgd, h_start, h_end, v_start, v_end, dgd_stride);
  int downsample_factor =
      use_downsampled_wiener_stats ? WIENER_STATS_DOWNSAMPLE_FACTOR : 1;

  std::vector<std::vector<int64_t> > M_int(wiener_win,
                                           std::vector<int64_t>(wiener_win, 0));
  std::vector<std::vector<int64_t> > H_int(
      wiener_win * wiener_win, std::vector<int64_t>(wiener_win * 8, 0));
  std::vector<std::vector<int32_t> > sumY(wiener_win,
                                          std::vector<int32_t>(wiener_win, 0));
  int32_t sumX = 0;
  const uint8_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  for (i = v_start; i < v_end; i = i + downsample_factor) {
    if (use_downsampled_wiener_stats &&
        (v_end - i < WIENER_STATS_DOWNSAMPLE_FACTOR)) {
      downsample_factor = v_end - i;
    }
    int32_t sumX_row_i32 = 0;
    std::vector<std::vector<int32_t> > sumY_row(
        wiener_win, std::vector<int32_t>(wiener_win, 0));
    std::vector<std::vector<int32_t> > M_row_i32(
        wiener_win, std::vector<int32_t>(wiener_win, 0));
    std::vector<std::vector<int32_t> > H_row_i32(
        wiener_win * wiener_win, std::vector<int32_t>(wiener_win * 8, 0));
    const int h_end_even = h_end & ~1;
    const int has_odd_pixel = h_end & 1;
    for (j = h_start; j < h_end_even; j += 2) {
      const uint8_t X1 = src[i * src_stride + j];
      const uint8_t X2 = src[i * src_stride + j + 1];
      sumX_row_i32 += X1 + X2;

      const uint8_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint8_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int32_t *H_int_temp = &H_row_i32[(l * wiener_win + k)][0];
          const uint8_t D1 = dgd_ijkl[0];
          const uint8_t D2 = dgd_ijkl[1];
          sumY_row[k][l] += D1 + D2;
          M_row_i32[l][k] += D1 * X1 + D2 * X2;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m] +
                                       D2 * dgd_ij[n + dgd_stride * m + 1];
            }
          }
        }
      }
    }
    // If the width is odd, add in the final pixel
    if (has_odd_pixel) {
      const uint8_t X1 = src[i * src_stride + j];
      sumX_row_i32 += X1;

      const uint8_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint8_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int32_t *H_int_temp = &H_row_i32[(l * wiener_win + k)][0];
          const uint8_t D1 = dgd_ijkl[0];
          sumY_row[k][l] += D1;
          M_row_i32[l][k] += D1 * X1;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m];
            }
          }
        }
      }
    }

    sumX += sumX_row_i32 * downsample_factor;
    // Scale M matrix based on the downsampling factor
    for (k = 0; k < wiener_win; ++k) {
      for (l = 0; l < wiener_win; ++l) {
        sumY[k][l] += sumY_row[k][l] * downsample_factor;
        M_int[k][l] += (int64_t)M_row_i32[k][l] * downsample_factor;
      }
    }
    // Scale H matrix based on the downsampling factor
    for (k = 0; k < wiener_win * wiener_win; ++k) {
      for (l = 0; l < wiener_win * 8; ++l) {
        H_int[k][l] += (int64_t)H_row_i32[k][l] * downsample_factor;
      }
    }
  }

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      M[l * wiener_win + k] =
          M_int[l][k] + avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]);
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H[(l * wiener_win + k) * wiener_win2 + m * wiener_win + n] =
              H_int[(l * wiener_win + k)][n * 8 + m] + avg_square_sum -
              (int64_t)avg * (sumY[k][l] + sumY[n][m]);
        }
      }
    }
  }
}

void compute_stats_opt_c(int wiener_win, const uint8_t *dgd, const uint8_t *src,
                         int h_start, int h_end, int v_start, int v_end,
                         int dgd_stride, int src_stride, int64_t *M, int64_t *H,
                         int use_downsampled_wiener_stats) {
  if (wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_win_opt_c(wiener_win, dgd, src, h_start, h_end, v_start,
                            v_end, dgd_stride, src_stride, M, H,
                            use_downsampled_wiener_stats);
  } else {
    av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                        dgd_stride, src_stride, M, H,
                        use_downsampled_wiener_stats);
  }
}

static const int kIterations = 100;
typedef void (*compute_stats_Func)(int wiener_win, const uint8_t *dgd,
                                   const uint8_t *src, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, int64_t *M, int64_t *H,
                                   int use_downsampled_wiener_stats);

////////////////////////////////////////////////////////////////////////////////
// 8 bit
////////////////////////////////////////////////////////////////////////////////

typedef std::tuple<const compute_stats_Func> WienerTestParam;

class WienerTest : public ::testing::TestWithParam<WienerTestParam> {
 public:
  virtual void SetUp() {
    src_buf = (uint8_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*src_buf));
    ASSERT_NE(src_buf, nullptr);
    dgd_buf = (uint8_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*dgd_buf));
    ASSERT_NE(dgd_buf, nullptr);
    target_func_ = GET_PARAM(0);
  }
  virtual void TearDown() {
    aom_free(src_buf);
    aom_free(dgd_buf);
  }
  void RunWienerTest(const int32_t wiener_win, int32_t run_times);
  void RunWienerTest_ExtremeValues(const int32_t wiener_win);

 private:
  compute_stats_Func target_func_;
  libaom_test::ACMRandom rng_;
  uint8_t *src_buf;
  uint8_t *dgd_buf;
};

void WienerTest::RunWienerTest(const int32_t wiener_win, int32_t run_times) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  // Note(rachelbarker):
  // The SIMD code requires `h_start` to be even, but can otherwise
  // deal with any values of `h_end`, `v_start`, `v_end`. We cover this
  // entire range, even though (at the time of writing) `h_start` and `v_start`
  // will always be multiples of 64 when called from non-test code.
  // If in future any new requirements are added, these lines will
  // need changing.
  int h_start = (rng_.Rand16() % (MAX_WIENER_BLOCK / 2)) & ~1;
  int h_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (h_start > h_end) std::swap(h_start, h_end);
  int v_start = rng_.Rand16() % (MAX_WIENER_BLOCK / 2);
  int v_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (v_start > v_end) std::swap(v_start, v_end);
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = run_times == 1 ? kIterations : 2;
  const int max_value_downsample_stats = 1;

  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = rng_.Rand8();
      src_buf[i] = rng_.Rand8();
    }
    uint8_t *dgd = dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin;
    uint8_t *src = src_buf;
    for (int use_downsampled_stats = 0;
         use_downsampled_stats <= max_value_downsample_stats;
         use_downsampled_stats++) {
      aom_usec_timer timer;
      aom_usec_timer_start(&timer);
      for (int i = 0; i < run_times; ++i) {
        av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start,
                            v_end, dgd_stride, src_stride, M_ref, H_ref,
                            use_downsampled_stats);
      }
      aom_usec_timer_mark(&timer);
      const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      aom_usec_timer_start(&timer);
      for (int i = 0; i < run_times; ++i) {
        target_func_(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                     dgd_stride, src_stride, M_test, H_test,
                     use_downsampled_stats);
      }
      aom_usec_timer_mark(&timer);
      const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
      if (run_times > 10) {
        printf("win %d %3dx%-3d:%7.2f/%7.2fns", wiener_win, h_end, v_end, time1,
               time2);
        printf("(%3.2f)\n", time1 / time2);
      }
      int failed = 0;
      for (int i = 0; i < wiener_win2; ++i) {
        if (M_ref[i] != M_test[i]) {
          failed = 1;
          printf("win %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, M_ref[i], M_test[i]);
          break;
        }
      }
      for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
        if (H_ref[i] != H_test[i]) {
          failed = 1;
          printf("win %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, H_ref[i], H_test[i]);
          break;
        }
      }
      ASSERT_EQ(failed, 0);
    }
  }
}

void WienerTest::RunWienerTest_ExtremeValues(const int32_t wiener_win) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  const int h_start = 16;
  const int h_end = MAX_WIENER_BLOCK;
  const int v_start = 16;
  const int v_end = MAX_WIENER_BLOCK;
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = 1;
  const int max_value_downsample_stats = 1;

  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = 255;
      src_buf[i] = 255;
    }
    uint8_t *dgd = dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin;
    uint8_t *src = src_buf;
    for (int use_downsampled_stats = 0;
         use_downsampled_stats <= max_value_downsample_stats;
         use_downsampled_stats++) {
      av1_compute_stats_c(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                          dgd_stride, src_stride, M_ref, H_ref,
                          use_downsampled_stats);

      target_func_(wiener_win, dgd, src, h_start, h_end, v_start, v_end,
                   dgd_stride, src_stride, M_test, H_test,
                   use_downsampled_stats);

      int failed = 0;
      for (int i = 0; i < wiener_win2; ++i) {
        if (M_ref[i] != M_test[i]) {
          failed = 1;
          printf("win %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, M_ref[i], M_test[i]);
          break;
        }
      }
      for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
        if (H_ref[i] != H_test[i]) {
          failed = 1;
          printf("win %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64 " \n",
                 wiener_win, iter, i, H_ref[i], H_test[i]);
          break;
        }
      }
      ASSERT_EQ(failed, 0);
    }
  }
}

TEST_P(WienerTest, RandomValues) {
  RunWienerTest(WIENER_WIN, 1);
  RunWienerTest(WIENER_WIN_CHROMA, 1);
}

TEST_P(WienerTest, ExtremeValues) {
  RunWienerTest_ExtremeValues(WIENER_WIN);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA);
}

TEST_P(WienerTest, DISABLED_Speed) {
  RunWienerTest(WIENER_WIN, 200);
  RunWienerTest(WIENER_WIN_CHROMA, 200);
}

INSTANTIATE_TEST_SUITE_P(C, WienerTest, ::testing::Values(compute_stats_opt_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, WienerTest,
                         ::testing::Values(av1_compute_stats_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2

INSTANTIATE_TEST_SUITE_P(AVX2, WienerTest,
                         ::testing::Values(av1_compute_stats_avx2));
#endif  // HAVE_AVX2

}  // namespace wiener_lowbd

#if CONFIG_AV1_HIGHBITDEPTH
// High bit-depth tests:
namespace wiener_highbd {

static void compute_stats_highbd_win_opt_c(int wiener_win, const uint8_t *dgd8,
                                           const uint8_t *src8, int h_start,
                                           int h_end, int v_start, int v_end,
                                           int dgd_stride, int src_stride,
                                           int64_t *M, int64_t *H,
                                           aom_bit_depth_t bit_depth) {
  ASSERT_TRUE(wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA);
  int i, j, k, l, m, n;
  const int pixel_count = (h_end - h_start) * (v_end - v_start);
  const int wiener_win2 = wiener_win * wiener_win;
  const int wiener_halfwin = (wiener_win >> 1);
  const uint16_t *src = CONVERT_TO_SHORTPTR(src8);
  const uint16_t *dgd = CONVERT_TO_SHORTPTR(dgd8);
  const uint16_t avg =
      find_average_highbd(dgd, h_start, h_end, v_start, v_end, dgd_stride);

  std::vector<std::vector<int64_t> > M_int(wiener_win,
                                           std::vector<int64_t>(wiener_win, 0));
  std::vector<std::vector<int64_t> > H_int(
      wiener_win * wiener_win, std::vector<int64_t>(wiener_win * 8, 0));
  std::vector<std::vector<int32_t> > sumY(wiener_win,
                                          std::vector<int32_t>(wiener_win, 0));

  memset(M, 0, sizeof(*M) * wiener_win2);
  memset(H, 0, sizeof(*H) * wiener_win2 * wiener_win2);

  int64_t sumX = 0;
  const uint16_t *dgd_win = dgd - wiener_halfwin * dgd_stride - wiener_halfwin;

  // Main loop handles two pixels at a time
  // We can assume that h_start is even, since it will always be aligned to
  // a tile edge + some number of restoration units, and both of those will
  // be 64-pixel aligned.
  // However, at the edge of the image, h_end may be odd, so we need to handle
  // that case correctly.
  assert(h_start % 2 == 0);
  for (i = v_start; i < v_end; i++) {
    const int h_end_even = h_end & ~1;
    const int has_odd_pixel = h_end & 1;
    for (j = h_start; j < h_end_even; j += 2) {
      const uint16_t X1 = src[i * src_stride + j];
      const uint16_t X2 = src[i * src_stride + j + 1];
      sumX += X1 + X2;

      const uint16_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint16_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int64_t *H_int_temp = &H_int[(l * wiener_win + k)][0];
          const uint16_t D1 = dgd_ijkl[0];
          const uint16_t D2 = dgd_ijkl[1];
          sumY[k][l] += D1 + D2;
          M_int[l][k] += D1 * X1 + D2 * X2;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m] +
                                       D2 * dgd_ij[n + dgd_stride * m + 1];
            }
          }
        }
      }
    }
    // If the width is odd, add in the final pixel
    if (has_odd_pixel) {
      const uint16_t X1 = src[i * src_stride + j];
      sumX += X1;

      const uint16_t *dgd_ij = dgd_win + i * dgd_stride + j;
      for (k = 0; k < wiener_win; k++) {
        for (l = 0; l < wiener_win; l++) {
          const uint16_t *dgd_ijkl = dgd_ij + k * dgd_stride + l;
          int64_t *H_int_temp = &H_int[(l * wiener_win + k)][0];
          const uint16_t D1 = dgd_ijkl[0];
          sumY[k][l] += D1;
          M_int[l][k] += D1 * X1;
          for (m = 0; m < wiener_win; m++) {
            for (n = 0; n < wiener_win; n++) {
              H_int_temp[m * 8 + n] += D1 * dgd_ij[n + dgd_stride * m];
            }
          }
        }
      }
    }
  }

  uint8_t bit_depth_divider = 1;
  if (bit_depth == AOM_BITS_12)
    bit_depth_divider = 16;
  else if (bit_depth == AOM_BITS_10)
    bit_depth_divider = 4;

  const int64_t avg_square_sum = (int64_t)avg * (int64_t)avg * pixel_count;
  for (k = 0; k < wiener_win; k++) {
    for (l = 0; l < wiener_win; l++) {
      M[l * wiener_win + k] =
          (M_int[l][k] +
           (avg_square_sum - (int64_t)avg * (sumX + sumY[k][l]))) /
          bit_depth_divider;
      for (m = 0; m < wiener_win; m++) {
        for (n = 0; n < wiener_win; n++) {
          H[(l * wiener_win + k) * wiener_win2 + m * wiener_win + n] =
              (H_int[(l * wiener_win + k)][n * 8 + m] +
               (avg_square_sum - (int64_t)avg * (sumY[k][l] + sumY[n][m]))) /
              bit_depth_divider;
        }
      }
    }
  }
}

void compute_stats_highbd_opt_c(int wiener_win, const uint8_t *dgd,
                                const uint8_t *src, int h_start, int h_end,
                                int v_start, int v_end, int dgd_stride,
                                int src_stride, int64_t *M, int64_t *H,
                                aom_bit_depth_t bit_depth) {
  if (wiener_win == WIENER_WIN || wiener_win == WIENER_WIN_CHROMA) {
    compute_stats_highbd_win_opt_c(wiener_win, dgd, src, h_start, h_end,
                                   v_start, v_end, dgd_stride, src_stride, M, H,
                                   bit_depth);
  } else {
    av1_compute_stats_highbd_c(wiener_win, dgd, src, h_start, h_end, v_start,
                               v_end, dgd_stride, src_stride, M, H, bit_depth);
  }
}

static const int kIterations = 100;
typedef void (*compute_stats_Func)(int wiener_win, const uint8_t *dgd,
                                   const uint8_t *src, int h_start, int h_end,
                                   int v_start, int v_end, int dgd_stride,
                                   int src_stride, int64_t *M, int64_t *H,
                                   aom_bit_depth_t bit_depth);

typedef std::tuple<const compute_stats_Func> WienerTestParam;

class WienerTestHighbd : public ::testing::TestWithParam<WienerTestParam> {
 public:
  virtual void SetUp() {
    src_buf = (uint16_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*src_buf));
    ASSERT_NE(src_buf, nullptr);
    dgd_buf = (uint16_t *)aom_memalign(
        32, MAX_DATA_BLOCK * MAX_DATA_BLOCK * sizeof(*dgd_buf));
    ASSERT_NE(dgd_buf, nullptr);
    target_func_ = GET_PARAM(0);
  }
  virtual void TearDown() {
    aom_free(src_buf);
    aom_free(dgd_buf);
  }
  void RunWienerTest(const int32_t wiener_win, int32_t run_times,
                     aom_bit_depth_t bit_depth);
  void RunWienerTest_ExtremeValues(const int32_t wiener_win,
                                   aom_bit_depth_t bit_depth);

 private:
  compute_stats_Func target_func_;
  libaom_test::ACMRandom rng_;
  uint16_t *src_buf;
  uint16_t *dgd_buf;
};

void WienerTestHighbd::RunWienerTest(const int32_t wiener_win,
                                     int32_t run_times,
                                     aom_bit_depth_t bit_depth) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  // Note(rachelbarker):
  // The SIMD code requires `h_start` to be even, but can otherwise
  // deal with any values of `h_end`, `v_start`, `v_end`. We cover this
  // entire range, even though (at the time of writing) `h_start` and `v_start`
  // will always be multiples of 64 when called from non-test code.
  // If in future any new requirements are added, these lines will
  // need changing.
  int h_start = (rng_.Rand16() % (MAX_WIENER_BLOCK / 2)) & ~1;
  int h_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (h_start > h_end) std::swap(h_start, h_end);
  int v_start = rng_.Rand16() % (MAX_WIENER_BLOCK / 2);
  int v_end = run_times != 1 ? 256 : (rng_.Rand16() % MAX_WIENER_BLOCK);
  if (v_start > v_end) std::swap(v_start, v_end);
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = run_times == 1 ? kIterations : 2;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = rng_.Rand16() % (1 << bit_depth);
      src_buf[i] = rng_.Rand16() % (1 << bit_depth);
    }
    const uint8_t *dgd8 = CONVERT_TO_BYTEPTR(
        dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin);
    const uint8_t *src8 = CONVERT_TO_BYTEPTR(src_buf);

    aom_usec_timer timer;
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      av1_compute_stats_highbd_c(wiener_win, dgd8, src8, h_start, h_end,
                                 v_start, v_end, dgd_stride, src_stride, M_ref,
                                 H_ref, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time1 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    aom_usec_timer_start(&timer);
    for (int i = 0; i < run_times; ++i) {
      target_func_(wiener_win, dgd8, src8, h_start, h_end, v_start, v_end,
                   dgd_stride, src_stride, M_test, H_test, bit_depth);
    }
    aom_usec_timer_mark(&timer);
    const double time2 = static_cast<double>(aom_usec_timer_elapsed(&timer));
    if (run_times > 10) {
      printf("win %d bd %d %3dx%-3d:%7.2f/%7.2fns", wiener_win, bit_depth,
             h_end, v_end, time1, time2);
      printf("(%3.2f)\n", time1 / time2);
    }
    int failed = 0;
    for (int i = 0; i < wiener_win2; ++i) {
      if (M_ref[i] != M_test[i]) {
        failed = 1;
        printf("win %d bd %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, M_ref[i], M_test[i]);
        break;
      }
    }
    for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
      if (H_ref[i] != H_test[i]) {
        failed = 1;
        printf("win %d bd %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, H_ref[i], H_test[i]);
        break;
      }
    }
    ASSERT_EQ(failed, 0);
  }
}

void WienerTestHighbd::RunWienerTest_ExtremeValues(const int32_t wiener_win,
                                                   aom_bit_depth_t bit_depth) {
  const int32_t wiener_halfwin = wiener_win >> 1;
  const int32_t wiener_win2 = wiener_win * wiener_win;
  DECLARE_ALIGNED(32, int64_t, M_ref[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_ref[WIENER_WIN2 * WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, M_test[WIENER_WIN2]);
  DECLARE_ALIGNED(32, int64_t, H_test[WIENER_WIN2 * WIENER_WIN2]);
  const int h_start = 16;
  const int h_end = MAX_WIENER_BLOCK;
  const int v_start = 16;
  const int v_end = MAX_WIENER_BLOCK;
  const int dgd_stride = h_end;
  const int src_stride = MAX_DATA_BLOCK;
  const int iters = 1;
  for (int iter = 0; iter < iters && !HasFatalFailure(); ++iter) {
    for (int i = 0; i < MAX_DATA_BLOCK * MAX_DATA_BLOCK; ++i) {
      dgd_buf[i] = ((uint16_t)1 << bit_depth) - 1;
      src_buf[i] = ((uint16_t)1 << bit_depth) - 1;
    }
    const uint8_t *dgd8 = CONVERT_TO_BYTEPTR(
        dgd_buf + wiener_halfwin * MAX_DATA_BLOCK + wiener_halfwin);
    const uint8_t *src8 = CONVERT_TO_BYTEPTR(src_buf);

    av1_compute_stats_highbd_c(wiener_win, dgd8, src8, h_start, h_end, v_start,
                               v_end, dgd_stride, src_stride, M_ref, H_ref,
                               bit_depth);

    target_func_(wiener_win, dgd8, src8, h_start, h_end, v_start, v_end,
                 dgd_stride, src_stride, M_test, H_test, bit_depth);

    int failed = 0;
    for (int i = 0; i < wiener_win2; ++i) {
      if (M_ref[i] != M_test[i]) {
        failed = 1;
        printf("win %d bd %d M iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, M_ref[i], M_test[i]);
        break;
      }
    }
    for (int i = 0; i < wiener_win2 * wiener_win2; ++i) {
      if (H_ref[i] != H_test[i]) {
        failed = 1;
        printf("win %d bd %d H iter %d [%4d] ref %6" PRId64 " test %6" PRId64
               " \n",
               wiener_win, bit_depth, iter, i, H_ref[i], H_test[i]);
        break;
      }
    }
    ASSERT_EQ(failed, 0);
  }
}

TEST_P(WienerTestHighbd, RandomValues) {
  RunWienerTest(WIENER_WIN, 1, AOM_BITS_8);
  RunWienerTest(WIENER_WIN_CHROMA, 1, AOM_BITS_8);
  RunWienerTest(WIENER_WIN, 1, AOM_BITS_10);
  RunWienerTest(WIENER_WIN_CHROMA, 1, AOM_BITS_10);
  RunWienerTest(WIENER_WIN, 1, AOM_BITS_12);
  RunWienerTest(WIENER_WIN_CHROMA, 1, AOM_BITS_12);
}

TEST_P(WienerTestHighbd, ExtremeValues) {
  RunWienerTest_ExtremeValues(WIENER_WIN, AOM_BITS_8);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA, AOM_BITS_8);
  RunWienerTest_ExtremeValues(WIENER_WIN, AOM_BITS_10);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA, AOM_BITS_10);
  RunWienerTest_ExtremeValues(WIENER_WIN, AOM_BITS_12);
  RunWienerTest_ExtremeValues(WIENER_WIN_CHROMA, AOM_BITS_12);
}

TEST_P(WienerTestHighbd, DISABLED_Speed) {
  RunWienerTest(WIENER_WIN, 200, AOM_BITS_8);
  RunWienerTest(WIENER_WIN_CHROMA, 200, AOM_BITS_8);
  RunWienerTest(WIENER_WIN, 200, AOM_BITS_10);
  RunWienerTest(WIENER_WIN_CHROMA, 200, AOM_BITS_10);
  RunWienerTest(WIENER_WIN, 200, AOM_BITS_12);
  RunWienerTest(WIENER_WIN_CHROMA, 200, AOM_BITS_12);
}

INSTANTIATE_TEST_SUITE_P(C, WienerTestHighbd,
                         ::testing::Values(compute_stats_highbd_opt_c));

#if HAVE_SSE4_1
INSTANTIATE_TEST_SUITE_P(SSE4_1, WienerTestHighbd,
                         ::testing::Values(av1_compute_stats_highbd_sse4_1));
#endif  // HAVE_SSE4_1

#if HAVE_AVX2
INSTANTIATE_TEST_SUITE_P(AVX2, WienerTestHighbd,
                         ::testing::Values(av1_compute_stats_highbd_avx2));
#endif  // HAVE_AVX2

// A test that reproduces b/274668506: signed integer overflow in
// update_a_sep_sym().
TEST(SearchWienerTest, DISABLED_10bitSignedIntegerOverflowInUpdateASepSym) {
  constexpr int kWidth = 427;
  constexpr int kHeight = 1;
  static const uint16_t buffer[3 * kWidth * kHeight] = {
    // Y plane:
    0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,

    // U plane:
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023,

    // V plane:
    0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0,
    1023, 0, 1023, 0, 1023, 0, 1023, 0, 1023, 0
  };
  unsigned char *img_data =
      reinterpret_cast<unsigned char *>(const_cast<uint16_t *>(buffer));

  aom_image_t img;
  EXPECT_EQ(
      aom_img_wrap(&img, AOM_IMG_FMT_I44416, kWidth, kHeight, 1, img_data),
      &img);
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 1;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.g_threads = 61;
  cfg.rc_min_quantizer = 2;
  cfg.rc_max_quantizer = 20;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 11), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 4), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 3), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM),
            AOM_CODEC_OK);

  // Encode frame
  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(aom_codec_encode(&enc, nullptr, 0, 1, 0), AOM_CODEC_OK);
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

// A test that reproduces b/272139363: signed integer overflow in
// update_b_sep_sym().
TEST(SearchWienerTest, 10bitSignedIntegerOverflowInUpdateBSepSym) {
  constexpr int kWidth = 34;
  constexpr int kHeight = 3;
  static const uint16_t buffer[3 * kWidth * kHeight] = {
    // Y plane:
    61, 765, 674, 188, 367, 944, 153, 275, 906, 433, 154, 51, 8, 855, 186, 154,
    392, 0, 634, 3, 690, 1023, 1023, 1023, 1023, 1023, 1023, 8, 1, 64, 426, 0,
    100, 344, 944, 816, 816, 33, 1023, 1023, 1023, 1023, 295, 1023, 1023, 1023,
    1023, 1023, 1023, 1015, 1023, 231, 1020, 254, 439, 439, 894, 439, 150, 1019,
    1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 385, 320, 575,
    682, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 511, 699, 987, 3, 140,
    661, 120, 33, 143, 0, 0, 0, 3, 40, 625, 585, 16, 579, 160, 867,
    // U plane:
    739, 646, 13, 603, 7, 328, 91, 32, 488, 870, 330, 330, 330, 330, 330, 330,
    109, 330, 330, 330, 3, 545, 945, 249, 35, 561, 801, 32, 931, 639, 801, 91,
    1023, 827, 844, 948, 631, 894, 854, 601, 432, 504, 85, 1, 0, 0, 89, 89, 0,
    0, 0, 0, 0, 0, 432, 801, 382, 4, 0, 0, 2, 89, 89, 89, 89, 89, 89, 384, 0, 0,
    0, 0, 0, 0, 0, 1023, 1019, 1, 3, 691, 575, 691, 691, 691, 691, 691, 691,
    691, 691, 691, 691, 691, 84, 527, 4, 485, 8, 682, 698, 340, 1015, 706,
    // V plane:
    49, 10, 28, 1023, 1023, 1023, 0, 32, 32, 872, 114, 1003, 1023, 57, 477, 999,
    1023, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309, 309,
    9, 418, 418, 418, 418, 418, 418, 0, 0, 0, 1023, 4, 5, 0, 0, 1023, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 64, 0, 155, 709, 3, 331, 807, 633, 1023,
    1018, 646, 886, 991, 692, 915, 294, 0, 35, 2, 0, 471, 643, 770, 346, 176,
    32, 329, 322, 302, 61, 765, 674, 188, 367, 944, 153, 275, 906, 433, 154
  };
  unsigned char *img_data =
      reinterpret_cast<unsigned char *>(const_cast<uint16_t *>(buffer));

  aom_image_t img;
  EXPECT_EQ(&img, aom_img_wrap(&img, AOM_IMG_FMT_I44416, kWidth, kHeight, 1,
                               img_data));
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA));
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 1;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.rc_min_quantizer = 3;
  cfg.rc_max_quantizer = 54;
  aom_codec_ctx_t enc;
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 28));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 3));
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_control(&enc, AOME_SET_CPUUSED, 0));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1));
  EXPECT_EQ(AOM_CODEC_OK,
            aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM));

  // Encode frame
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, &img, 0, 1, 0));
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(AOM_CODEC_OK, aom_codec_encode(&enc, nullptr, 0, 1, 0));
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(AOM_CODEC_OK, aom_codec_destroy(&enc));
}

// A test that reproduces b/259173819: signed integer overflow in
// linsolve_wiener().
TEST(SearchWienerTest, 10bitSignedIntegerOverflowInLinsolveWiener) {
  constexpr int kWidth = 3;
  constexpr int kHeight = 3;
  static const uint16_t buffer[3 * kWidth * kHeight] = {
    // Y plane:
    81, 81, 1023, 1020, 81, 1023, 81, 128, 0,
    // U plane:
    273, 273, 273, 273, 273, 273, 273, 273, 273,
    // V plane:
    273, 273, 273, 273, 273, 273, 516, 81, 81
  };
  unsigned char *img_data =
      reinterpret_cast<unsigned char *>(const_cast<uint16_t *>(buffer));

  aom_image_t img;
  EXPECT_EQ(
      aom_img_wrap(&img, AOM_IMG_FMT_I44416, kWidth, kHeight, 1, img_data),
      &img);
  img.cp = AOM_CICP_CP_UNSPECIFIED;
  img.tc = AOM_CICP_TC_UNSPECIFIED;
  img.mc = AOM_CICP_MC_UNSPECIFIED;
  img.range = AOM_CR_FULL_RANGE;

  aom_codec_iface_t *iface = aom_codec_av1_cx();
  aom_codec_enc_cfg_t cfg;
  EXPECT_EQ(aom_codec_enc_config_default(iface, &cfg, AOM_USAGE_ALL_INTRA),
            AOM_CODEC_OK);
  cfg.rc_end_usage = AOM_Q;
  cfg.g_profile = 1;
  cfg.g_bit_depth = AOM_BITS_10;
  cfg.g_input_bit_depth = 10;
  cfg.g_w = kWidth;
  cfg.g_h = kHeight;
  cfg.g_limit = 1;
  cfg.g_lag_in_frames = 0;
  cfg.kf_mode = AOM_KF_DISABLED;
  cfg.kf_max_dist = 0;
  cfg.g_threads = 21;
  cfg.rc_min_quantizer = 16;
  cfg.rc_max_quantizer = 54;
  aom_codec_ctx_t enc;
  EXPECT_EQ(aom_codec_enc_init(&enc, iface, &cfg, AOM_CODEC_USE_HIGHBITDEPTH),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CQ_LEVEL, 35), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_ROW_MT, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_ROWS, 2), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_TILE_COLUMNS, 5), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_CPUUSED, 1), AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_COLOR_RANGE, AOM_CR_FULL_RANGE),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AV1E_SET_SKIP_POSTPROC_FILTERING, 1),
            AOM_CODEC_OK);
  EXPECT_EQ(aom_codec_control(&enc, AOME_SET_TUNING, AOM_TUNE_SSIM),
            AOM_CODEC_OK);

  // Encode frame
  EXPECT_EQ(aom_codec_encode(&enc, &img, 0, 1, 0), AOM_CODEC_OK);
  aom_codec_iter_t iter = nullptr;
  const aom_codec_cx_pkt_t *pkt = aom_codec_get_cx_data(&enc, &iter);
  ASSERT_NE(pkt, nullptr);
  EXPECT_EQ(pkt->kind, AOM_CODEC_CX_FRAME_PKT);
  // pkt->data.frame.flags is 0x1f0011.
  EXPECT_EQ(pkt->data.frame.flags & AOM_FRAME_IS_KEY, AOM_FRAME_IS_KEY);
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  // Flush encoder
  EXPECT_EQ(aom_codec_encode(&enc, nullptr, 0, 1, 0), AOM_CODEC_OK);
  iter = nullptr;
  pkt = aom_codec_get_cx_data(&enc, &iter);
  EXPECT_EQ(pkt, nullptr);

  EXPECT_EQ(aom_codec_destroy(&enc), AOM_CODEC_OK);
}

}  // namespace wiener_highbd
#endif  // CONFIG_AV1_HIGHBITDEPTH
