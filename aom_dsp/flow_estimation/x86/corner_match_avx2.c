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

#include <math.h>

#include <immintrin.h>
#include "config/aom_dsp_rtcd.h"

#include "aom_ports/mem.h"
#include "aom_dsp/flow_estimation/corner_match.h"

DECLARE_ALIGNED(32, static const uint16_t, ones_array[16]) = { 1, 1, 1, 1, 1, 1,
                                                               1, 1, 1, 1, 1, 1,
                                                               1, 1, 1, 1 };

#if MATCH_SZ != 16
#error "Need to apply pixel mask in corner_match_avx2.c if MATCH_SZ != 16"
#endif

/* Compute mean and standard deviation of pixels in a window of size
   MATCH_SZ by MATCH_SZ centered at (x, y).
   Store results into *mean and *one_over_stddev

   Note: The output of this function is scaled by MATCH_SZ, as in
   *mean = MATCH_SZ * <true mean> and
   *one_over_stddev = 1 / (MATCH_SZ * <true stddev>)

   Combined with the fact that we return 1/stddev rather than the standard
   deviation itself, this allows us to completely avoid divisions in
   aom_compute_correlation, which is much hotter than this function is.
*/
void aom_compute_mean_stddev_avx2(const unsigned char *frame, int stride, int x,
                                  int y, double *mean,
                                  double *one_over_stddev) {
  int i, stride_i = 0;
  const __m256i ones = _mm256_load_si256((__m256i *)ones_array);
  __m128i v1;
  __m256i v1_1;
  __m256i sum_vec = _mm256_setzero_si256();
  __m256i sumsq_vec = _mm256_setzero_si256();

  frame += (y - MATCH_SZ_BY2) * stride + (x - MATCH_SZ_BY2);

  for (i = 0; i < MATCH_SZ; ++i) {
    v1 = _mm_loadu_si128((__m128i *)&frame[stride_i]);
    v1_1 = _mm256_cvtepu8_epi16(v1);

    sum_vec = _mm256_add_epi16(sum_vec, v1_1);
    sumsq_vec = _mm256_add_epi32(sumsq_vec, _mm256_madd_epi16(v1_1, v1_1));
    stride_i += stride;
  }

  // Sum sum_vec into a single value
  // Use madd to sum 16x16-bit intermediates to 8x32-bit intermediates,
  // then sum these
  const __m256i partial_sum_1 = _mm256_madd_epi16(sum_vec, ones);
  const __m128i partial_sum_2 =
      _mm_add_epi32(_mm256_extracti128_si256(partial_sum_1, 0),
                    _mm256_extracti128_si256(partial_sum_1, 1));
  const int sum = _mm_extract_epi32(partial_sum_2, 0) +
                  _mm_extract_epi32(partial_sum_2, 1) +
                  _mm_extract_epi32(partial_sum_2, 2) +
                  _mm_extract_epi32(partial_sum_2, 3);

  // Sum sumsq_vec into a single value
  const __m128i partial_sumsq =
      _mm_add_epi32(_mm256_extracti128_si256(sumsq_vec, 0),
                    _mm256_extracti128_si256(sumsq_vec, 1));
  const int sumsq = _mm_extract_epi32(partial_sumsq, 0) +
                    _mm_extract_epi32(partial_sumsq, 1) +
                    _mm_extract_epi32(partial_sumsq, 2) +
                    _mm_extract_epi32(partial_sumsq, 3);

  *mean = (double)sum / MATCH_SZ;
  *one_over_stddev = 1.0 / sqrt(sumsq - (*mean) * (*mean));
}

/* Compute corr(frame1, frame2) over a window of size MATCH_SZ by MATCH_SZ.
   To save on computation, the mean and (1 divided by the) standard deviation
   of the window in each frame are precomputed and passed into this function
   as arguments.
*/
double aom_compute_correlation_avx2(const unsigned char *frame1, int stride1,
                                    int x1, int y1, double mean1,
                                    double one_over_stddev1,
                                    const unsigned char *frame2, int stride2,
                                    int x2, int y2, double mean2,
                                    double one_over_stddev2) {
  int i, stride1_i = 0, stride2_i = 0;
  __m256i v1_1, v2_1;
  __m128i v1, v2;
  __m256i cross_vec = _mm256_setzero_si256();

  frame1 += (y1 - MATCH_SZ_BY2) * stride1 + (x1 - MATCH_SZ_BY2);
  frame2 += (y2 - MATCH_SZ_BY2) * stride2 + (x2 - MATCH_SZ_BY2);

  for (i = 0; i < MATCH_SZ; ++i) {
    v1 = _mm_loadu_si128((__m128i *)&frame1[stride1_i]);
    v1_1 = _mm256_cvtepu8_epi16(v1);
    v2 = _mm_loadu_si128((__m128i *)&frame2[stride2_i]);
    v2_1 = _mm256_cvtepu8_epi16(v2);

    cross_vec = _mm256_add_epi32(cross_vec, _mm256_madd_epi16(v1_1, v2_1));
    stride1_i += stride1;
    stride2_i += stride2;
  }

  // Sum cross_vec into a single value
  __m128i tmp = _mm_add_epi32(_mm256_extracti128_si256(cross_vec, 0),
                              _mm256_extracti128_si256(cross_vec, 1));
  int cross = _mm_extract_epi32(tmp, 0) + _mm_extract_epi32(tmp, 1) +
              _mm_extract_epi32(tmp, 2) + _mm_extract_epi32(tmp, 3);

  // Note: In theory, the calculations here "should" be
  //   covariance = cross / N^2 - mean1 * mean2
  //   correlation = covariance / (stddev1 * stddev2).
  //
  // However, because of the scaling in aom_compute_mean_stddev, the
  // lines below actually calculate
  //   covariance * N^2 = cross - (mean1 * N) * (mean2 * N)
  //   correlation = (covariance * N^2) / ((stddev1 * N) * (stddev2 * N))
  //
  // ie. we have removed the need for a division, and still end up with the
  // correct unscaled correlation (ie, in the range [-1, +1])
  double covariance = cross - mean1 * mean2;
  double correlation = covariance * (one_over_stddev1 * one_over_stddev2);
  return correlation;
}
