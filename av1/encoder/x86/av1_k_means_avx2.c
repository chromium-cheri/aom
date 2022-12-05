/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <immintrin.h>  // AVX2

#include "config/aom_dsp_rtcd.h"
#include "aom_dsp/x86/synonyms.h"

static int64_t k_means_horizontal_sum_avx2(__m256i a) {
  const __m128i low = _mm256_castsi256_si128(a);
  const __m128i high = _mm256_extracti128_si256(a, 1);
  const __m128i sum = _mm_add_epi64(low, high);
  const __m128i sum_high = _mm_unpackhi_epi64(sum, sum);
  int64_t res;
  _mm_storel_epi64((__m128i *)&res, _mm_add_epi64(sum, sum_high));
  return res;
}

void av1_calc_indices_dim1_avx2(const int *data, const int *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  __m256i dist[PALETTE_MAX_SIZE];
  const __m256i v_zero = _mm256_setzero_si256();
  __m256i sum = _mm256_setzero_si256();

  for (int i = 0; i < n; i += 8) {
    __m256i ind = _mm256_loadu_si256((__m256i *)data);
    for (int j = 0; j < k; j++) {
      __m256i cent = _mm256_set1_epi32(centroids[j]);
      __m256i d1 = _mm256_sub_epi32(ind, cent);
      dist[j] = _mm256_abs_epi32(d1);
    }

    ind = _mm256_setzero_si256();
    for (int j = 1; j < k; j++) {
      __m256i cmp = _mm256_cmpgt_epi32(dist[0], dist[j]);
      __m256i dist1 = _mm256_andnot_si256(cmp, dist[0]);
      __m256i dist2 = _mm256_and_si256(cmp, dist[j]);
      dist[0] = _mm256_or_si256(dist1, dist2);
      __m256i ind1 = _mm256_set1_epi32(j);
      ind = _mm256_or_si256(_mm256_andnot_si256(cmp, ind),
                            _mm256_and_si256(cmp, ind1));
    }

    __m256i p1 = _mm256_packus_epi32(ind, v_zero);
    __m256i px = _mm256_permute4x64_epi64(p1, 0x58);
    __m256i p2 = _mm256_packus_epi16(px, v_zero);
    __m128i d1 = _mm256_extracti128_si256(p2, 0);

    _mm_storel_epi64((__m128i *)indices, d1);

    if (total_dist) {
      // Convert to 64 bit and add to sum.
      dist[0] = _mm256_mullo_epi32(dist[0], dist[0]);
      const __m256i dist1 = _mm256_unpacklo_epi32(dist[0], v_zero);
      const __m256i dist2 = _mm256_unpackhi_epi32(dist[0], v_zero);
      sum = _mm256_add_epi64(sum, dist1);
      sum = _mm256_add_epi64(sum, dist2);
    }

    indices += 8;
    data += 8;
  }
  if (total_dist) {
    *total_dist = k_means_horizontal_sum_avx2(sum);
  }
}

void av1_calc_indices_dim2_avx2(const int *data, const int *centroids,
                                uint8_t *indices, int64_t *total_dist, int n,
                                int k) {
  __m256i dist[PALETTE_MAX_SIZE];
  const __m256i v_zero = _mm256_setzero_si256();
  const __m256i v_permute = _mm256_setr_epi32(0, 1, 4, 5, 2, 3, 6, 7);
  __m256i sum = _mm256_setzero_si256();

  for (int i = 0; i < n; i += 8) {
    __m256i ind1 = _mm256_loadu_si256((__m256i *)data);
    __m256i ind2 = _mm256_loadu_si256((__m256i *)(data + 8));
    for (int j = 0; j < k; j++) {
      __m128i cent0 = _mm_loadl_epi64((__m128i const *)&centroids[2 * j]);
      __m256i cent1 = _mm256_inserti128_si256(v_zero, cent0, 0);
      cent1 = _mm256_inserti128_si256(cent1, cent0, 1);
      __m256i cent = _mm256_unpacklo_epi64(cent1, cent1);
      __m256i d1 = _mm256_sub_epi32(ind1, cent);
      __m256i d2 = _mm256_sub_epi32(ind2, cent);
      __m256i d3 = _mm256_mullo_epi32(d1, d1);
      __m256i d4 = _mm256_mullo_epi32(d2, d2);
      __m256i d5 = _mm256_hadd_epi32(d3, d4);
      dist[j] = _mm256_permutevar8x32_epi32(d5, v_permute);
    }

    __m256i ind = _mm256_setzero_si256();
    for (int j = 1; j < k; j++) {
      __m256i cmp = _mm256_cmpgt_epi32(dist[0], dist[j]);
      __m256i dist1 = _mm256_andnot_si256(cmp, dist[0]);
      __m256i dist2 = _mm256_and_si256(cmp, dist[j]);
      dist[0] = _mm256_or_si256(dist1, dist2);
      ind1 = _mm256_set1_epi32(j);
      ind = _mm256_or_si256(_mm256_andnot_si256(cmp, ind),
                            _mm256_and_si256(cmp, ind1));
    }

    __m256i p1 = _mm256_packus_epi32(ind, v_zero);
    __m256i px = _mm256_permute4x64_epi64(p1, 0x58);
    __m256i p2 = _mm256_packus_epi16(px, v_zero);
    __m128i d1 = _mm256_extracti128_si256(p2, 0);

    _mm_storel_epi64((__m128i *)indices, d1);

    if (total_dist) {
      // Convert to 64 bit and add to sum.
      const __m256i dist1 = _mm256_unpacklo_epi32(dist[0], v_zero);
      const __m256i dist2 = _mm256_unpackhi_epi32(dist[0], v_zero);
      sum = _mm256_add_epi64(sum, dist1);
      sum = _mm256_add_epi64(sum, dist2);
    }

    indices += 8;
    data += 16;
  }
  if (total_dist) {
    *total_dist = k_means_horizontal_sum_avx2(sum);
  }
}
