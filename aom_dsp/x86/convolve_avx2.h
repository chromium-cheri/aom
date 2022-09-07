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

#ifndef AOM_AOM_DSP_X86_CONVOLVE_AVX2_H_
#define AOM_AOM_DSP_X86_CONVOLVE_AVX2_H_

#include "aom_ports/mem.h"

#include "aom_dsp/aom_filter.h"
#include "aom_dsp/x86/mem_avx2.h"
#include "aom_dsp/x86/mem_sse2.h"
#include "aom_dsp/x86/mem_sse4_1.h"

// filters for 16
DECLARE_ALIGNED(32, static const uint8_t, filt_global_avx2[]) = {
  0,  1,  1,  2,  2, 3,  3,  4,  4,  5,  5,  6,  6,  7,  7,  8,  0,  1,  1,
  2,  2,  3,  3,  4, 4,  5,  5,  6,  6,  7,  7,  8,  2,  3,  3,  4,  4,  5,
  5,  6,  6,  7,  7, 8,  8,  9,  9,  10, 2,  3,  3,  4,  4,  5,  5,  6,  6,
  7,  7,  8,  8,  9, 9,  10, 4,  5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10,
  10, 11, 11, 12, 4, 5,  5,  6,  6,  7,  7,  8,  8,  9,  9,  10, 10, 11, 11,
  12, 6,  7,  7,  8, 8,  9,  9,  10, 10, 11, 11, 12, 12, 13, 13, 14, 6,  7,
  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

DECLARE_ALIGNED(32, static const uint8_t, filt_d4_global_avx2[]) = {
  0, 1, 2, 3,  1, 2, 3, 4, 2, 3, 4, 5, 3, 4, 5, 6, 0, 1, 2, 3,  1, 2,
  3, 4, 2, 3,  4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8, 6, 7,  8, 9,
  7, 8, 9, 10, 4, 5, 6, 7, 5, 6, 7, 8, 6, 7, 8, 9, 7, 8, 9, 10,
};

DECLARE_ALIGNED(32, static const uint8_t, filt4_d4_global_avx2[]) = {
  2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8,
  2, 3, 4, 5, 3, 4, 5, 6, 4, 5, 6, 7, 5, 6, 7, 8,
};

DECLARE_ALIGNED(32, static const uint8_t, filt_center_global_avx2[32]) = {
  3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 8, 255, 9, 255, 10, 255,
  3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 8, 255, 9, 255, 10, 255
};

DECLARE_ALIGNED(32, static const uint8_t,
                filt1_global_avx2[32]) = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5,
                                           6, 6, 7, 7, 8, 0, 1, 1, 2, 2, 3,
                                           3, 4, 4, 5, 5, 6, 6, 7, 7, 8 };

DECLARE_ALIGNED(32, static const uint8_t,
                filt2_global_avx2[32]) = { 2, 3, 3, 4, 4,  5, 5, 6, 6, 7, 7,
                                           8, 8, 9, 9, 10, 2, 3, 3, 4, 4, 5,
                                           5, 6, 6, 7, 7,  8, 8, 9, 9, 10 };

DECLARE_ALIGNED(32, static const uint8_t, filt3_global_avx2[32]) = {
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12,
  4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12
};

DECLARE_ALIGNED(32, static const uint8_t, filt4_global_avx2[32]) = {
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14,
  6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14
};

#define CONVOLVE_SR_HORIZONTAL_FILTER_4TAP                                     \
  for (i = 0; i < (im_h - 2); i += 2) {                                        \
    __m256i data = _mm256_castsi128_si256(                                     \
        _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));           \
    data = _mm256_inserti128_si256(                                            \
        data,                                                                  \
        _mm_loadu_si128(                                                       \
            (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),           \
        1);                                                                    \
    __m256i res = convolve_lowbd_x_4tap(data, coeffs_h + 1, filt);             \
    res =                                                                      \
        _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h); \
    _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);              \
  }                                                                            \
  __m256i data_1 = _mm256_castsi128_si256(                                     \
      _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));             \
  __m256i res = convolve_lowbd_x_4tap(data_1, coeffs_h + 1, filt);             \
  res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h); \
  _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);

#define CONVOLVE_SR_VERTICAL_FILTER_4TAP                                      \
  __m256i s[6];                                                               \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));  \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));  \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));  \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));  \
                                                                              \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                 \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                 \
  s[3] = _mm256_unpackhi_epi16(src_0, src_1);                                 \
  s[4] = _mm256_unpackhi_epi16(src_2, src_3);                                 \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * im_stride];                           \
    const __m256i s4 = _mm256_loadu_si256((__m256i *)(data + 4 * im_stride)); \
    const __m256i s5 = _mm256_loadu_si256((__m256i *)(data + 5 * im_stride)); \
    s[2] = _mm256_unpacklo_epi16(s4, s5);                                     \
    s[5] = _mm256_unpackhi_epi16(s4, s5);                                     \
                                                                              \
    __m256i res_a = convolve_4tap(s, coeffs_v + 1);                           \
    __m256i res_b = convolve_4tap(s + 3, coeffs_v + 1);                       \
                                                                              \
    res_a =                                                                   \
        _mm256_sra_epi32(_mm256_add_epi32(res_a, sum_round_v), sum_shift_v);  \
    res_b =                                                                   \
        _mm256_sra_epi32(_mm256_add_epi32(res_b, sum_round_v), sum_shift_v);  \
    const __m256i res_a_round = _mm256_sra_epi32(                             \
        _mm256_add_epi32(res_a, round_const_v), round_shift_v);               \
    const __m256i res_b_round = _mm256_sra_epi32(                             \
        _mm256_add_epi32(res_b, round_const_v), round_shift_v);               \
    const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);   \
    const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);         \
    const __m128i res_0 = _mm256_castsi256_si128(res_8b);                     \
    const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);                \
                                                                              \
    __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];                 \
    __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];    \
    if (w - j > 4) {                                                          \
      _mm_storel_epi64(p_0, res_0);                                           \
      _mm_storel_epi64(p_1, res_1);                                           \
    } else if (w == 4) {                                                      \
      xx_storel_32(p_0, res_0);                                               \
      xx_storel_32(p_1, res_1);                                               \
    } else {                                                                  \
      *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);                  \
      *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);                  \
    }                                                                         \
                                                                              \
    s[0] = s[1];                                                              \
    s[1] = s[2];                                                              \
    s[3] = s[4];                                                              \
    s[4] = s[5];                                                              \
  }

#define CONVOLVE_SR_HORIZONTAL_FILTER_6TAP                                     \
  for (i = 0; i < (im_h - 2); i += 2) {                                        \
    __m256i data = _mm256_castsi128_si256(                                     \
        _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));           \
    data = _mm256_inserti128_si256(                                            \
        data,                                                                  \
        _mm_loadu_si128(                                                       \
            (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),           \
        1);                                                                    \
                                                                               \
    __m256i res = convolve_lowbd_x_6tap(data, coeffs_h, filt);                 \
    res =                                                                      \
        _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h); \
    _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);              \
  }                                                                            \
                                                                               \
  __m256i data_1 = _mm256_castsi128_si256(                                     \
      _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));             \
                                                                               \
  __m256i res = convolve_lowbd_x_6tap(data_1, coeffs_h, filt);                 \
                                                                               \
  res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h); \
                                                                               \
  _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);

#define CONVOLVE_SR_VERTICAL_FILTER_6TAP                                      \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));  \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));  \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));  \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));  \
                                                                              \
  __m256i s[8];                                                               \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                 \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                 \
                                                                              \
  s[3] = _mm256_unpackhi_epi16(src_0, src_1);                                 \
  s[4] = _mm256_unpackhi_epi16(src_2, src_3);                                 \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * im_stride];                           \
                                                                              \
    const __m256i s6 = _mm256_loadu_si256((__m256i *)(data + 4 * im_stride)); \
    const __m256i s7 = _mm256_loadu_si256((__m256i *)(data + 5 * im_stride)); \
                                                                              \
    s[2] = _mm256_unpacklo_epi16(s6, s7);                                     \
    s[5] = _mm256_unpackhi_epi16(s6, s7);                                     \
                                                                              \
    __m256i res_a = convolve_6tap(s, coeffs_v);                               \
    __m256i res_b = convolve_6tap(s + 3, coeffs_v);                           \
                                                                              \
    res_a =                                                                   \
        _mm256_sra_epi32(_mm256_add_epi32(res_a, sum_round_v), sum_shift_v);  \
    res_b =                                                                   \
        _mm256_sra_epi32(_mm256_add_epi32(res_b, sum_round_v), sum_shift_v);  \
                                                                              \
    const __m256i res_a_round = _mm256_sra_epi32(                             \
        _mm256_add_epi32(res_a, round_const_v), round_shift_v);               \
    const __m256i res_b_round = _mm256_sra_epi32(                             \
        _mm256_add_epi32(res_b, round_const_v), round_shift_v);               \
                                                                              \
    const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);   \
    const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);         \
                                                                              \
    const __m128i res_0 = _mm256_castsi256_si128(res_8b);                     \
    const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);                \
                                                                              \
    __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];                 \
    __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];    \
    if (w - j > 4) {                                                          \
      _mm_storel_epi64(p_0, res_0);                                           \
      _mm_storel_epi64(p_1, res_1);                                           \
    } else if (w == 4) {                                                      \
      xx_storel_32(p_0, res_0);                                               \
      xx_storel_32(p_1, res_1);                                               \
    } else {                                                                  \
      *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);                  \
      *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);                  \
    }                                                                         \
                                                                              \
    s[0] = s[1];                                                              \
    s[1] = s[2];                                                              \
                                                                              \
    s[3] = s[4];                                                              \
    s[4] = s[5];                                                              \
  }

#define CONVOLVE_SR_HORIZONTAL_FILTER_8TAP                                     \
  for (i = 0; i < (im_h - 2); i += 2) {                                        \
    __m256i data = _mm256_castsi128_si256(                                     \
        _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));           \
    data = _mm256_inserti128_si256(                                            \
        data,                                                                  \
        _mm_loadu_si128(                                                       \
            (__m128i *)&src_ptr[(i * src_stride) + j + src_stride]),           \
        1);                                                                    \
                                                                               \
    __m256i res = convolve_lowbd_x(data, coeffs_h, filt);                      \
    res =                                                                      \
        _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h); \
    _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);              \
  }                                                                            \
                                                                               \
  __m256i data_1 = _mm256_castsi128_si256(                                     \
      _mm_loadu_si128((__m128i *)&src_ptr[(i * src_stride) + j]));             \
                                                                               \
  __m256i res = convolve_lowbd_x(data_1, coeffs_h, filt);                      \
                                                                               \
  res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h), round_shift_h); \
                                                                               \
  _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);

#define CONVOLVE_SR_VERTICAL_FILTER_8TAP                                      \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));  \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));  \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));  \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));  \
  __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));  \
  __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));  \
                                                                              \
  __m256i s[8];                                                               \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                 \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                 \
  s[2] = _mm256_unpacklo_epi16(src_4, src_5);                                 \
                                                                              \
  s[4] = _mm256_unpackhi_epi16(src_0, src_1);                                 \
  s[5] = _mm256_unpackhi_epi16(src_2, src_3);                                 \
  s[6] = _mm256_unpackhi_epi16(src_4, src_5);                                 \
                                                                              \
  for (i = 0; i < h; i += 2) {                                                \
    const int16_t *data = &im_block[i * im_stride];                           \
                                                                              \
    const __m256i s6 = _mm256_loadu_si256((__m256i *)(data + 6 * im_stride)); \
    const __m256i s7 = _mm256_loadu_si256((__m256i *)(data + 7 * im_stride)); \
                                                                              \
    s[3] = _mm256_unpacklo_epi16(s6, s7);                                     \
    s[7] = _mm256_unpackhi_epi16(s6, s7);                                     \
                                                                              \
    __m256i res_a = convolve(s, coeffs_v);                                    \
    __m256i res_b = convolve(s + 4, coeffs_v);                                \
                                                                              \
    res_a =                                                                   \
        _mm256_sra_epi32(_mm256_add_epi32(res_a, sum_round_v), sum_shift_v);  \
    res_b =                                                                   \
        _mm256_sra_epi32(_mm256_add_epi32(res_b, sum_round_v), sum_shift_v);  \
                                                                              \
    const __m256i res_a_round = _mm256_sra_epi32(                             \
        _mm256_add_epi32(res_a, round_const_v), round_shift_v);               \
    const __m256i res_b_round = _mm256_sra_epi32(                             \
        _mm256_add_epi32(res_b, round_const_v), round_shift_v);               \
                                                                              \
    const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);   \
    const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);         \
                                                                              \
    const __m128i res_0 = _mm256_castsi256_si128(res_8b);                     \
    const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);                \
                                                                              \
    __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];                 \
    __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];    \
    if (w - j > 4) {                                                          \
      _mm_storel_epi64(p_0, res_0);                                           \
      _mm_storel_epi64(p_1, res_1);                                           \
    } else if (w == 4) {                                                      \
      xx_storel_32(p_0, res_0);                                               \
      xx_storel_32(p_1, res_1);                                               \
    } else {                                                                  \
      *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);                  \
      *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);                  \
    }                                                                         \
                                                                              \
    s[0] = s[1];                                                              \
    s[1] = s[2];                                                              \
    s[2] = s[3];                                                              \
                                                                              \
    s[4] = s[5];                                                              \
    s[5] = s[6];                                                              \
    s[6] = s[7];                                                              \
  }

#define CONVOLVE_SR_HORIZONTAL_FILTER_12TAP                                    \
  const __m256i v_zero = _mm256_setzero_si256();                               \
  __m256i s[12];                                                               \
  if (w <= 4) {                                                                \
    for (i = 0; i < im_h; i += 2) {                                            \
      const __m256i data = _mm256_permute2x128_si256(                          \
          _mm256_castsi128_si256(                                              \
              _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride + j]))),     \
          _mm256_castsi128_si256(_mm_loadu_si128(                              \
              (__m128i *)(&src_ptr[i * src_stride + src_stride + j]))),        \
          0x20);                                                               \
      const __m256i s_16l = _mm256_unpacklo_epi8(data, v_zero);                \
      const __m256i s_16h = _mm256_unpackhi_epi8(data, v_zero);                \
      const __m256i s_ll = _mm256_unpacklo_epi16(s_16l, s_16l);                \
      const __m256i s_lh = _mm256_unpackhi_epi16(s_16l, s_16l);                \
                                                                               \
      const __m256i s_hl = _mm256_unpacklo_epi16(s_16h, s_16h);                \
      const __m256i s_hh = _mm256_unpackhi_epi16(s_16h, s_16h);                \
                                                                               \
      s[0] = _mm256_alignr_epi8(s_lh, s_ll, 2);                                \
      s[1] = _mm256_alignr_epi8(s_lh, s_ll, 10);                               \
      s[2] = _mm256_alignr_epi8(s_hl, s_lh, 2);                                \
      s[3] = _mm256_alignr_epi8(s_hl, s_lh, 10);                               \
      s[4] = _mm256_alignr_epi8(s_hh, s_hl, 2);                                \
      s[5] = _mm256_alignr_epi8(s_hh, s_hl, 10);                               \
                                                                               \
      const __m256i res_lo = convolve_12taps(s, coeffs_h);                     \
                                                                               \
      __m256i res_32b_lo = _mm256_sra_epi32(                                   \
          _mm256_add_epi32(res_lo, round_const_h12), round_shift_h12);         \
      __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);         \
      const __m128i res_0 = _mm256_extracti128_si256(res_16b_lo, 0);           \
      const __m128i res_1 = _mm256_extracti128_si256(res_16b_lo, 1);           \
      if (w > 2) {                                                             \
        _mm_storel_epi64((__m128i *)&im_block[i * im_stride], res_0);          \
        _mm_storel_epi64((__m128i *)&im_block[i * im_stride + im_stride],      \
                         res_1);                                               \
      } else {                                                                 \
        uint32_t horiz_2;                                                      \
        horiz_2 = (uint32_t)_mm_cvtsi128_si32(res_0);                          \
        im_block[i * im_stride] = (uint16_t)horiz_2;                           \
        im_block[i * im_stride + 1] = (uint16_t)(horiz_2 >> 16);               \
        horiz_2 = (uint32_t)_mm_cvtsi128_si32(res_1);                          \
        im_block[i * im_stride + im_stride] = (uint16_t)horiz_2;               \
        im_block[i * im_stride + im_stride + 1] = (uint16_t)(horiz_2 >> 16);   \
      }                                                                        \
    }                                                                          \
  } else {                                                                     \
    for (i = 0; i < im_h; i++) {                                               \
      const __m256i data = _mm256_permute2x128_si256(                          \
          _mm256_castsi128_si256(                                              \
              _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride + j]))),     \
          _mm256_castsi128_si256(                                              \
              _mm_loadu_si128((__m128i *)(&src_ptr[i * src_stride + j + 4]))), \
          0x20);                                                               \
      const __m256i s_16l = _mm256_unpacklo_epi8(data, v_zero);                \
      const __m256i s_16h = _mm256_unpackhi_epi8(data, v_zero);                \
                                                                               \
      const __m256i s_ll = _mm256_unpacklo_epi16(s_16l, s_16l);                \
      const __m256i s_lh = _mm256_unpackhi_epi16(s_16l, s_16l);                \
                                                                               \
      const __m256i s_hl = _mm256_unpacklo_epi16(s_16h, s_16h);                \
      const __m256i s_hh = _mm256_unpackhi_epi16(s_16h, s_16h);                \
                                                                               \
      s[0] = _mm256_alignr_epi8(s_lh, s_ll, 2);                                \
      s[1] = _mm256_alignr_epi8(s_lh, s_ll, 10);                               \
      s[2] = _mm256_alignr_epi8(s_hl, s_lh, 2);                                \
      s[3] = _mm256_alignr_epi8(s_hl, s_lh, 10);                               \
      s[4] = _mm256_alignr_epi8(s_hh, s_hl, 2);                                \
      s[5] = _mm256_alignr_epi8(s_hh, s_hl, 10);                               \
                                                                               \
      const __m256i res_lo = convolve_12taps(s, coeffs_h);                     \
                                                                               \
      __m256i res_32b_lo = _mm256_sra_epi32(                                   \
          _mm256_add_epi32(res_lo, round_const_h12), round_shift_h12);         \
                                                                               \
      __m256i res_16b_lo = _mm256_packs_epi32(res_32b_lo, res_32b_lo);         \
      _mm_store_si128((__m128i *)&im_block[i * im_stride],                     \
                      _mm256_extracti128_si256(                                \
                          _mm256_permute4x64_epi64(res_16b_lo, 0x88), 0));     \
    }                                                                          \
  }

#define CONVOLVE_SR_VERTICAL_FILTER_12TAP                                      \
  __m256i src_0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));   \
  __m256i src_1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));   \
  __m256i src_2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));   \
  __m256i src_3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));   \
  __m256i src_4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));   \
  __m256i src_5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));   \
  __m256i src_6 = _mm256_loadu_si256((__m256i *)(im_block + 6 * im_stride));   \
  __m256i src_7 = _mm256_loadu_si256((__m256i *)(im_block + 7 * im_stride));   \
  __m256i src_8 = _mm256_loadu_si256((__m256i *)(im_block + 8 * im_stride));   \
  __m256i src_9 = _mm256_loadu_si256((__m256i *)(im_block + 9 * im_stride));   \
                                                                               \
  s[0] = _mm256_unpacklo_epi16(src_0, src_1);                                  \
  s[1] = _mm256_unpacklo_epi16(src_2, src_3);                                  \
  s[2] = _mm256_unpacklo_epi16(src_4, src_5);                                  \
  s[3] = _mm256_unpacklo_epi16(src_6, src_7);                                  \
  s[4] = _mm256_unpacklo_epi16(src_8, src_9);                                  \
                                                                               \
  s[6] = _mm256_unpackhi_epi16(src_0, src_1);                                  \
  s[7] = _mm256_unpackhi_epi16(src_2, src_3);                                  \
  s[8] = _mm256_unpackhi_epi16(src_4, src_5);                                  \
  s[9] = _mm256_unpackhi_epi16(src_6, src_7);                                  \
  s[10] = _mm256_unpackhi_epi16(src_8, src_9);                                 \
                                                                               \
  for (i = 0; i < h; i += 2) {                                                 \
    const int16_t *data = &im_block[i * im_stride];                            \
                                                                               \
    const __m256i s6 = _mm256_loadu_si256((__m256i *)(data + 10 * im_stride)); \
    const __m256i s7 = _mm256_loadu_si256((__m256i *)(data + 11 * im_stride)); \
                                                                               \
    s[5] = _mm256_unpacklo_epi16(s6, s7);                                      \
    s[11] = _mm256_unpackhi_epi16(s6, s7);                                     \
                                                                               \
    __m256i res_a = convolve_12taps(s, coeffs_v);                              \
    __m256i res_b = convolve_12taps(s + 6, coeffs_v);                          \
                                                                               \
    res_a =                                                                    \
        _mm256_sra_epi32(_mm256_add_epi32(res_a, sum_round_v), sum_shift_v);   \
    res_b =                                                                    \
        _mm256_sra_epi32(_mm256_add_epi32(res_b, sum_round_v), sum_shift_v);   \
                                                                               \
    const __m256i res_a_round = _mm256_sra_epi32(                              \
        _mm256_add_epi32(res_a, round_const_v), round_shift_v);                \
    const __m256i res_b_round = _mm256_sra_epi32(                              \
        _mm256_add_epi32(res_b, round_const_v), round_shift_v);                \
                                                                               \
    const __m256i res_16bit = _mm256_packs_epi32(res_a_round, res_b_round);    \
    const __m256i res_8b = _mm256_packus_epi16(res_16bit, res_16bit);          \
                                                                               \
    const __m128i res_0 = _mm256_castsi256_si128(res_8b);                      \
    const __m128i res_1 = _mm256_extracti128_si256(res_8b, 1);                 \
                                                                               \
    __m128i *const p_0 = (__m128i *)&dst[i * dst_stride + j];                  \
    __m128i *const p_1 = (__m128i *)&dst[i * dst_stride + j + dst_stride];     \
    if (w - j > 4) {                                                           \
      _mm_storel_epi64(p_0, res_0);                                            \
      _mm_storel_epi64(p_1, res_1);                                            \
    } else if (w == 4) {                                                       \
      xx_storel_32(p_0, res_0);                                                \
      xx_storel_32(p_1, res_1);                                                \
    } else {                                                                   \
      *(uint16_t *)p_0 = (uint16_t)_mm_cvtsi128_si32(res_0);                   \
      *(uint16_t *)p_1 = (uint16_t)_mm_cvtsi128_si32(res_1);                   \
    }                                                                          \
                                                                               \
    s[0] = s[1];                                                               \
    s[1] = s[2];                                                               \
    s[2] = s[3];                                                               \
    s[3] = s[4];                                                               \
    s[4] = s[5];                                                               \
                                                                               \
    s[6] = s[7];                                                               \
    s[7] = s[8];                                                               \
    s[8] = s[9];                                                               \
    s[9] = s[10];                                                              \
    s[10] = s[11];                                                             \
  }

#define DIST_WTD_CONVOLVE_HORIZONTAL_FILTER_8TAP                        \
  do {                                                                  \
    for (i = 0; i < im_h; i += 2) {                                     \
      __m256i data =                                                    \
          _mm256_castsi128_si256(_mm_loadu_si128((__m128i *)src_h));    \
      if (i + 1 < im_h)                                                 \
        data = _mm256_inserti128_si256(                                 \
            data, _mm_loadu_si128((__m128i *)(src_h + src_stride)), 1); \
      src_h += (src_stride << 1);                                       \
      __m256i res = convolve_lowbd_x(data, coeffs_x, filt);             \
                                                                        \
      res = _mm256_sra_epi16(_mm256_add_epi16(res, round_const_h),      \
                             round_shift_h);                            \
                                                                        \
      _mm256_store_si256((__m256i *)&im_block[i * im_stride], res);     \
    }                                                                   \
  } while (0)

#define DIST_WTD_CONVOLVE_VERTICAL_FILTER_8TAP                                 \
  do {                                                                         \
    __m256i s[8];                                                              \
    __m256i s0 = _mm256_loadu_si256((__m256i *)(im_block + 0 * im_stride));    \
    __m256i s1 = _mm256_loadu_si256((__m256i *)(im_block + 1 * im_stride));    \
    __m256i s2 = _mm256_loadu_si256((__m256i *)(im_block + 2 * im_stride));    \
    __m256i s3 = _mm256_loadu_si256((__m256i *)(im_block + 3 * im_stride));    \
    __m256i s4 = _mm256_loadu_si256((__m256i *)(im_block + 4 * im_stride));    \
    __m256i s5 = _mm256_loadu_si256((__m256i *)(im_block + 5 * im_stride));    \
                                                                               \
    s[0] = _mm256_unpacklo_epi16(s0, s1);                                      \
    s[1] = _mm256_unpacklo_epi16(s2, s3);                                      \
    s[2] = _mm256_unpacklo_epi16(s4, s5);                                      \
                                                                               \
    s[4] = _mm256_unpackhi_epi16(s0, s1);                                      \
    s[5] = _mm256_unpackhi_epi16(s2, s3);                                      \
    s[6] = _mm256_unpackhi_epi16(s4, s5);                                      \
                                                                               \
    for (i = 0; i < h; i += 2) {                                               \
      const int16_t *data = &im_block[i * im_stride];                          \
                                                                               \
      const __m256i s6 =                                                       \
          _mm256_loadu_si256((__m256i *)(data + 6 * im_stride));               \
      const __m256i s7 =                                                       \
          _mm256_loadu_si256((__m256i *)(data + 7 * im_stride));               \
                                                                               \
      s[3] = _mm256_unpacklo_epi16(s6, s7);                                    \
      s[7] = _mm256_unpackhi_epi16(s6, s7);                                    \
                                                                               \
      const __m256i res_a = convolve(s, coeffs_y);                             \
      const __m256i res_a_round = _mm256_sra_epi32(                            \
          _mm256_add_epi32(res_a, round_const_v), round_shift_v);              \
                                                                               \
      if (w - j > 4) {                                                         \
        const __m256i res_b = convolve(s + 4, coeffs_y);                       \
        const __m256i res_b_round = _mm256_sra_epi32(                          \
            _mm256_add_epi32(res_b, round_const_v), round_shift_v);            \
        const __m256i res_16b = _mm256_packs_epi32(res_a_round, res_b_round);  \
        const __m256i res_unsigned = _mm256_add_epi16(res_16b, offset_const);  \
                                                                               \
        if (do_average) {                                                      \
          const __m256i data_ref_0 =                                           \
              load_line2_avx2(&dst[i * dst_stride + j],                        \
                              &dst[i * dst_stride + j + dst_stride]);          \
          const __m256i comp_avg_res = comp_avg(&data_ref_0, &res_unsigned,    \
                                                &wt, use_dist_wtd_comp_avg);   \
                                                                               \
          const __m256i round_result = convolve_rounding(                      \
              &comp_avg_res, &offset_const, &rounding_const, rounding_shift);  \
                                                                               \
          const __m256i res_8 =                                                \
              _mm256_packus_epi16(round_result, round_result);                 \
          const __m128i res_0 = _mm256_castsi256_si128(res_8);                 \
          const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);            \
                                                                               \
          _mm_storel_epi64((__m128i *)(&dst0[i * dst_stride0 + j]), res_0);    \
          _mm_storel_epi64(                                                    \
              (__m128i *)((&dst0[i * dst_stride0 + j + dst_stride0])), res_1); \
        } else {                                                               \
          const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);          \
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);       \
                                                                               \
          const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);     \
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),  \
                          res_1);                                              \
        }                                                                      \
      } else {                                                                 \
        const __m256i res_16b = _mm256_packs_epi32(res_a_round, res_a_round);  \
        const __m256i res_unsigned = _mm256_add_epi16(res_16b, offset_const);  \
                                                                               \
        if (do_average) {                                                      \
          const __m256i data_ref_0 =                                           \
              load_line2_avx2(&dst[i * dst_stride + j],                        \
                              &dst[i * dst_stride + j + dst_stride]);          \
                                                                               \
          const __m256i comp_avg_res = comp_avg(&data_ref_0, &res_unsigned,    \
                                                &wt, use_dist_wtd_comp_avg);   \
                                                                               \
          const __m256i round_result = convolve_rounding(                      \
              &comp_avg_res, &offset_const, &rounding_const, rounding_shift);  \
                                                                               \
          const __m256i res_8 =                                                \
              _mm256_packus_epi16(round_result, round_result);                 \
          const __m128i res_0 = _mm256_castsi256_si128(res_8);                 \
          const __m128i res_1 = _mm256_extracti128_si256(res_8, 1);            \
                                                                               \
          *(int *)(&dst0[i * dst_stride0 + j]) = _mm_cvtsi128_si32(res_0);     \
          *(int *)(&dst0[i * dst_stride0 + j + dst_stride0]) =                 \
              _mm_cvtsi128_si32(res_1);                                        \
                                                                               \
        } else {                                                               \
          const __m128i res_0 = _mm256_castsi256_si128(res_unsigned);          \
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j]), res_0);       \
                                                                               \
          const __m128i res_1 = _mm256_extracti128_si256(res_unsigned, 1);     \
          _mm_store_si128((__m128i *)(&dst[i * dst_stride + j + dst_stride]),  \
                          res_1);                                              \
        }                                                                      \
      }                                                                        \
                                                                               \
      s[0] = s[1];                                                             \
      s[1] = s[2];                                                             \
      s[2] = s[3];                                                             \
                                                                               \
      s[4] = s[5];                                                             \
      s[5] = s[6];                                                             \
      s[6] = s[7];                                                             \
    }                                                                          \
  } while (0)

static INLINE void prepare_coeffs_lowbd(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0e0cu));
}

static INLINE void prepare_coeffs_6t_lowbd(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((int16_t)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0402u));
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0806u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0c0au));
}

static INLINE void prepare_coeffs_6t(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)(filter + 1));
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
}

static INLINE void prepare_coeffs(const InterpFilterParams *const filter_params,
                                  const int subpel_q4,
                                  __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi32(coeff, 0xff);
}

static INLINE void prepare_coeffs_12taps(
    const InterpFilterParams *const filter_params, const int subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi32(coeff, 0xff);
  // coeffs 8 9 10 11 0 0 0 0
  coeff_8 = _mm_loadl_epi64((__m128i *)(filter + 8));
  coeff = _mm256_broadcastq_epi64(coeff_8);
  coeffs[4] = _mm256_shuffle_epi32(coeff, 0x00);  // coeffs 8 9 8 9 8 9 8 9
  coeffs[5] = _mm256_shuffle_epi32(coeff, 0x55);  // coeffs 10 11 10 11.. 10 11
}

static INLINE __m256i convolve_lowbd(const __m256i *const s,
                                     const __m256i *const coeffs) {
  const __m256i res_01 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[2], coeffs[2]);
  const __m256i res_67 = _mm256_maddubs_epi16(s[3], coeffs[3]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res = _mm256_add_epi16(_mm256_add_epi16(res_01, res_45),
                                       _mm256_add_epi16(res_23, res_67));

  return res;
}

static INLINE __m256i convolve_lowbd_6tap(const __m256i *const s,
                                          const __m256i *const coeffs) {
  const __m256i res_01 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(s[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[2], coeffs[2]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res =
      _mm256_add_epi16(_mm256_add_epi16(res_01, res_45), res_23);

  return res;
}

static INLINE __m256i convolve_lowbd_4tap(const __m256i *const s,
                                          const __m256i *const coeffs) {
  const __m256i res_23 = _mm256_maddubs_epi16(s[0], coeffs[0]);
  const __m256i res_45 = _mm256_maddubs_epi16(s[1], coeffs[1]);

  // order: 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15
  const __m256i res = _mm256_add_epi16(res_45, res_23);

  return res;
}

static INLINE __m256i convolve_6tap(const __m256i *const s,
                                    const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);

  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1), res_2);

  return res;
}

static INLINE __m256i convolve_12taps(const __m256i *const s,
                                      const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_madd_epi16(s[3], coeffs[3]);
  const __m256i res_4 = _mm256_madd_epi16(s[4], coeffs[4]);
  const __m256i res_5 = _mm256_madd_epi16(s[5], coeffs[5]);

  const __m256i res1 = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                        _mm256_add_epi32(res_2, res_3));
  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_4, res_5), res1);

  return res;
}

static INLINE __m256i convolve(const __m256i *const s,
                               const __m256i *const coeffs) {
  const __m256i res_0 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_1 = _mm256_madd_epi16(s[1], coeffs[1]);
  const __m256i res_2 = _mm256_madd_epi16(s[2], coeffs[2]);
  const __m256i res_3 = _mm256_madd_epi16(s[3], coeffs[3]);

  const __m256i res = _mm256_add_epi32(_mm256_add_epi32(res_0, res_1),
                                       _mm256_add_epi32(res_2, res_3));

  return res;
}

static INLINE __m256i convolve_4tap(const __m256i *const s,
                                    const __m256i *const coeffs) {
  const __m256i res_1 = _mm256_madd_epi16(s[0], coeffs[0]);
  const __m256i res_2 = _mm256_madd_epi16(s[1], coeffs[1]);

  const __m256i res = _mm256_add_epi32(res_1, res_2);
  return res;
}

static INLINE __m256i convolve_lowbd_x(const __m256i data,
                                       const __m256i *const coeffs,
                                       const __m256i *const filt) {
  __m256i s[4];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);
  s[2] = _mm256_shuffle_epi8(data, filt[2]);
  s[3] = _mm256_shuffle_epi8(data, filt[3]);

  return convolve_lowbd(s, coeffs);
}

static INLINE __m256i convolve_lowbd_x_6tap(const __m256i data,
                                            const __m256i *const coeffs,
                                            const __m256i *const filt) {
  __m256i s[4];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);
  s[2] = _mm256_shuffle_epi8(data, filt[2]);

  return convolve_lowbd_6tap(s, coeffs);
}

static INLINE __m256i convolve_lowbd_x_4tap(const __m256i data,
                                            const __m256i *const coeffs,
                                            const __m256i *const filt) {
  __m256i s[2];

  s[0] = _mm256_shuffle_epi8(data, filt[0]);
  s[1] = _mm256_shuffle_epi8(data, filt[1]);

  return convolve_lowbd_4tap(s, coeffs);
}

static INLINE void add_store_aligned_256(CONV_BUF_TYPE *const dst,
                                         const __m256i *const res,
                                         const int do_average) {
  __m256i d;
  if (do_average) {
    d = _mm256_load_si256((__m256i *)dst);
    d = _mm256_add_epi32(d, *res);
    d = _mm256_srai_epi32(d, 1);
  } else {
    d = *res;
  }
  _mm256_store_si256((__m256i *)dst, d);
}

static INLINE __m256i comp_avg(const __m256i *const data_ref_0,
                               const __m256i *const res_unsigned,
                               const __m256i *const wt,
                               const int use_dist_wtd_comp_avg) {
  __m256i res;
  if (use_dist_wtd_comp_avg) {
    const __m256i data_lo = _mm256_unpacklo_epi16(*data_ref_0, *res_unsigned);
    const __m256i data_hi = _mm256_unpackhi_epi16(*data_ref_0, *res_unsigned);

    const __m256i wt_res_lo = _mm256_madd_epi16(data_lo, *wt);
    const __m256i wt_res_hi = _mm256_madd_epi16(data_hi, *wt);

    const __m256i res_lo = _mm256_srai_epi32(wt_res_lo, DIST_PRECISION_BITS);
    const __m256i res_hi = _mm256_srai_epi32(wt_res_hi, DIST_PRECISION_BITS);

    res = _mm256_packs_epi32(res_lo, res_hi);
  } else {
    const __m256i wt_res = _mm256_add_epi16(*data_ref_0, *res_unsigned);
    res = _mm256_srai_epi16(wt_res, 1);
  }
  return res;
}

static INLINE __m256i convolve_rounding(const __m256i *const res_unsigned,
                                        const __m256i *const offset_const,
                                        const __m256i *const round_const,
                                        const int round_shift) {
  const __m256i res_signed = _mm256_sub_epi16(*res_unsigned, *offset_const);
  const __m256i res_round = _mm256_srai_epi16(
      _mm256_add_epi16(res_signed, *round_const), round_shift);
  return res_round;
}

static INLINE __m256i highbd_comp_avg(const __m256i *const data_ref_0,
                                      const __m256i *const res_unsigned,
                                      const __m256i *const wt0,
                                      const __m256i *const wt1,
                                      const int use_dist_wtd_comp_avg) {
  __m256i res;
  if (use_dist_wtd_comp_avg) {
    const __m256i wt0_res = _mm256_mullo_epi32(*data_ref_0, *wt0);
    const __m256i wt1_res = _mm256_mullo_epi32(*res_unsigned, *wt1);
    const __m256i wt_res = _mm256_add_epi32(wt0_res, wt1_res);
    res = _mm256_srai_epi32(wt_res, DIST_PRECISION_BITS);
  } else {
    const __m256i wt_res = _mm256_add_epi32(*data_ref_0, *res_unsigned);
    res = _mm256_srai_epi32(wt_res, 1);
  }
  return res;
}

static INLINE __m256i highbd_convolve_rounding(
    const __m256i *const res_unsigned, const __m256i *const offset_const,
    const __m256i *const round_const, const int round_shift) {
  const __m256i res_signed = _mm256_sub_epi32(*res_unsigned, *offset_const);
  const __m256i res_round = _mm256_srai_epi32(
      _mm256_add_epi32(res_signed, *round_const), round_shift);

  return res_round;
}

static INLINE void populate_coeffs_4tap_avx2(const __m128i coeffs_128,
                                             __m256i coeffs[2]) {
  const __m256i coeffs_256 = _mm256_broadcastsi128_si256(coeffs_128);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0a08u));
}

static INLINE void populate_coeffs_6tap_avx2(const __m128i coeffs_128,
                                             __m256i coeffs[3]) {
  const __m256i coeffs_256 = _mm256_broadcastsi128_si256(coeffs_128);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0402u));
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0806u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0C0Au));
}

static INLINE void populate_coeffs_8tap_avx2(const __m128i coeffs_128,
                                             __m256i coeffs[4]) {
  const __m256i coeffs_256 = _mm256_broadcastsi128_si256(coeffs_128);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi8(coeffs_256, _mm256_set1_epi16(0x0e0cu));
}

static INLINE void prepare_half_coeffs_2tap_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [1] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_cvtsi32_si128(*(const int32_t *)(filter + 3));

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 3 4 3 4 3 4 3 4
  *coeffs = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0200u));
}

static INLINE void prepare_half_coeffs_4tap_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [2] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0a08u));
}

static INLINE void prepare_half_coeffs_6tap_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [3] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0402u));
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0806u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0C0Au));
}

static INLINE void prepare_half_coeffs_8tap_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0200u));
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0604u));
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0a08u));
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm_shuffle_epi8(coeffs_1, _mm_set1_epi16(0x0e0cu));
}

static INLINE void prepare_half_coeffs_2tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [1] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_cvtsi32_si128(*(const int32_t *)(filter + 3));
  const __m256i filter_coeffs = _mm256_broadcastsi128_si256(coeffs_8);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));

  const __m256i coeffs_1 = _mm256_srai_epi16(filter_coeffs, 1);

  // coeffs 3 4 3 4 3 4 3 4
  *coeffs = _mm256_shuffle_epi8(coeffs_1, _mm256_set1_epi16(0x0200u));
}

static INLINE void prepare_half_coeffs_4tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [2] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));
  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);
  populate_coeffs_4tap_avx2(coeffs_1, coeffs);
}

static INLINE void prepare_half_coeffs_6tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [3] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));
  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);
  populate_coeffs_6tap_avx2(coeffs_1, coeffs);
}

static INLINE void prepare_half_coeffs_8tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);

  // right shift all filter co-efficients by 1 to reduce the bits required.
  // This extra right shift will be taken care of at the end while rounding
  // the result.
  // Since all filter co-efficients are even, this change will not affect the
  // end result
  assert(_mm_test_all_zeros(_mm_and_si128(coeffs_8, _mm_set1_epi16(1)),
                            _mm_set1_epi16((short)0xffff)));
  const __m128i coeffs_1 = _mm_srai_epi16(coeffs_8, 1);
  populate_coeffs_8tap_avx2(coeffs_1, coeffs);
}

static INLINE void prepare_coeffs_2tap_sse2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [1] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff = _mm_cvtsi32_si128(*(const int32_t *)(filter + 3));

  // coeffs 3 4 3 4 3 4 3 4
  coeffs[0] = _mm_shuffle_epi32(coeff, 0x00);
}

static INLINE void prepare_coeffs_4tap_sse2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [2] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff = _mm_loadu_si128((__m128i *)filter);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm_shuffle_epi32(coeff, 0xaa);
}

static INLINE void prepare_coeffs_6tap_ssse3(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [3] */) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeff = _mm_loadu_si128((__m128i *)filter);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm_shuffle_epi8(coeff, _mm_set1_epi32(0x05040302u));
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm_shuffle_epi8(coeff, _mm_set1_epi32(0x09080706u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm_shuffle_epi8(coeff, _mm_set1_epi32(0x0D0C0B0Au));
}

static INLINE void prepare_coeffs_8tap_sse2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m128i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff = _mm_loadu_si128((__m128i *)filter);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm_shuffle_epi32(coeff, 0xff);
}

static INLINE void prepare_coeffs_2tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [1] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_cvtsi32_si128(*(const int32_t *)(filter + 3));
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 3 4 3 4 3 4 3 4
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
}

static INLINE void prepare_coeffs_4tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [2] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 2 3 2 3 2 3 2 3
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0xaa);
}

static INLINE void prepare_coeffs_6tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [3]*/) {
  const int16_t *const filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);
  const __m128i coeffs_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(coeffs_8);

  // coeffs 1 2 1 2 1 2 1 2
  coeffs[0] = _mm256_shuffle_epi8(coeff, _mm256_set1_epi32(0x05040302u));
  // coeffs 3 4 3 4 3 4 3 4
  coeffs[1] = _mm256_shuffle_epi8(coeff, _mm256_set1_epi32(0x09080706u));
  // coeffs 5 6 5 6 5 6 5 6
  coeffs[2] = _mm256_shuffle_epi8(coeff, _mm256_set1_epi32(0x0D0C0B0Au));
}

static INLINE void prepare_coeffs_8tap_avx2(
    const InterpFilterParams *const filter_params, const int32_t subpel_q4,
    __m256i *const coeffs /* [4] */) {
  const int16_t *filter = av1_get_interp_filter_subpel_kernel(
      filter_params, subpel_q4 & SUBPEL_MASK);

  const __m128i coeff_8 = _mm_loadu_si128((__m128i *)filter);
  const __m256i coeff = _mm256_broadcastsi128_si256(coeff_8);

  // coeffs 0 1 0 1 0 1 0 1
  coeffs[0] = _mm256_shuffle_epi32(coeff, 0x00);
  // coeffs 2 3 2 3 2 3 2 3
  coeffs[1] = _mm256_shuffle_epi32(coeff, 0x55);
  // coeffs 4 5 4 5 4 5 4 5
  coeffs[2] = _mm256_shuffle_epi32(coeff, 0xaa);
  // coeffs 6 7 6 7 6 7 6 7
  coeffs[3] = _mm256_shuffle_epi32(coeff, 0xff);
}

static INLINE void load_16bit_5rows_avx2(const int16_t *const src,
                                         const ptrdiff_t stride,
                                         __m256i dst[5]) {
  dst[0] = _mm256_loadu_si256((__m256i *)(src + 0 * stride));
  dst[1] = _mm256_loadu_si256((__m256i *)(src + 1 * stride));
  dst[2] = _mm256_loadu_si256((__m256i *)(src + 2 * stride));
  dst[3] = _mm256_loadu_si256((__m256i *)(src + 3 * stride));
  dst[4] = _mm256_loadu_si256((__m256i *)(src + 4 * stride));
}

static INLINE void load_16bit_7rows_avx2(const int16_t *const src,
                                         const ptrdiff_t stride,
                                         __m256i dst[7]) {
  dst[0] = _mm256_loadu_si256((__m256i *)(src + 0 * stride));
  dst[1] = _mm256_loadu_si256((__m256i *)(src + 1 * stride));
  dst[2] = _mm256_loadu_si256((__m256i *)(src + 2 * stride));
  dst[3] = _mm256_loadu_si256((__m256i *)(src + 3 * stride));
  dst[4] = _mm256_loadu_si256((__m256i *)(src + 4 * stride));
  dst[5] = _mm256_loadu_si256((__m256i *)(src + 5 * stride));
  dst[6] = _mm256_loadu_si256((__m256i *)(src + 6 * stride));
}

static AOM_FORCE_INLINE void load_16bit_8rows_avx2(const int16_t *const src,
                                                   const ptrdiff_t stride,
                                                   __m256i dst[8]) {
  dst[0] = _mm256_loadu_si256((__m256i *)(src + 0 * stride));
  dst[1] = _mm256_loadu_si256((__m256i *)(src + 1 * stride));
  dst[2] = _mm256_loadu_si256((__m256i *)(src + 2 * stride));
  dst[3] = _mm256_loadu_si256((__m256i *)(src + 3 * stride));
  dst[4] = _mm256_loadu_si256((__m256i *)(src + 4 * stride));
  dst[5] = _mm256_loadu_si256((__m256i *)(src + 5 * stride));
  dst[6] = _mm256_loadu_si256((__m256i *)(src + 6 * stride));
  dst[7] = _mm256_loadu_si256((__m256i *)(src + 7 * stride));
}

static AOM_FORCE_INLINE void loadu_unpack_16bit_5rows_avx2(
    const int16_t *const src, const ptrdiff_t stride, __m256i s_256[5],
    __m256i ss_256[5], __m256i tt_256[5]) {
  s_256[0] = _mm256_loadu_si256((__m256i *)(src + 0 * stride));
  s_256[1] = _mm256_loadu_si256((__m256i *)(src + 1 * stride));
  s_256[2] = _mm256_loadu_si256((__m256i *)(src + 2 * stride));
  s_256[3] = _mm256_loadu_si256((__m256i *)(src + 3 * stride));
  s_256[4] = _mm256_loadu_si256((__m256i *)(src + 4 * stride));

  ss_256[0] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(s_256[2], s_256[3]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[0], s_256[1]);
  ss_256[4] = _mm256_unpackhi_epi16(s_256[2], s_256[3]);

  tt_256[0] = _mm256_unpacklo_epi16(s_256[1], s_256[2]);
  tt_256[1] = _mm256_unpacklo_epi16(s_256[3], s_256[4]);
  tt_256[3] = _mm256_unpackhi_epi16(s_256[1], s_256[2]);
  tt_256[4] = _mm256_unpackhi_epi16(s_256[3], s_256[4]);
}

static AOM_FORCE_INLINE void loadu_unpack_16bit_3rows_avx2(
    const int16_t *const src, const ptrdiff_t stride, __m256i s_256[3],
    __m256i ss_256[3], __m256i tt_256[3]) {
  s_256[0] = _mm256_loadu_si256((__m256i *)(src + 0 * stride));
  s_256[1] = _mm256_loadu_si256((__m256i *)(src + 1 * stride));
  s_256[2] = _mm256_loadu_si256((__m256i *)(src + 2 * stride));

  ss_256[0] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  ss_256[2] = _mm256_unpackhi_epi16(s_256[0], s_256[1]);

  tt_256[0] = _mm256_unpacklo_epi16(s_256[1], s_256[2]);
  tt_256[2] = _mm256_unpackhi_epi16(s_256[1], s_256[2]);
}

static INLINE void convolve_8tap_unpack_avx2(const __m256i s[6],
                                             __m256i ss[7]) {
  ss[0] = _mm256_unpacklo_epi16(s[0], s[1]);
  ss[1] = _mm256_unpacklo_epi16(s[2], s[3]);
  ss[2] = _mm256_unpacklo_epi16(s[4], s[5]);
  ss[4] = _mm256_unpackhi_epi16(s[0], s[1]);
  ss[5] = _mm256_unpackhi_epi16(s[2], s[3]);
  ss[6] = _mm256_unpackhi_epi16(s[4], s[5]);
}

static INLINE __m128i convolve_2tap_ssse3(const __m128i ss[1],
                                          const __m128i coeffs[1]) {
  return _mm_maddubs_epi16(ss[0], coeffs[0]);
}

static INLINE __m128i convolve_4tap_ssse3(const __m128i ss[2],
                                          const __m128i coeffs[2]) {
  const __m128i res_23 = _mm_maddubs_epi16(ss[0], coeffs[0]);
  const __m128i res_45 = _mm_maddubs_epi16(ss[1], coeffs[1]);
  return _mm_add_epi16(res_23, res_45);
}

static INLINE __m128i convolve_6tap_ssse3(const __m128i ss[3],
                                          const __m128i coeffs[3]) {
  const __m128i res_12 = _mm_maddubs_epi16(ss[0], coeffs[0]);
  const __m128i res_34 = _mm_maddubs_epi16(ss[1], coeffs[1]);
  const __m128i res_56 = _mm_maddubs_epi16(ss[2], coeffs[2]);
  const __m128i res_1256 = _mm_add_epi16(res_12, res_56);
  return _mm_add_epi16(res_1256, res_34);
}

static INLINE __m128i convolve_8tap_ssse3(const __m128i ss[4],
                                          const __m128i coeffs[4]) {
  const __m128i res_01 = _mm_maddubs_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_maddubs_epi16(ss[1], coeffs[1]);
  const __m128i res_45 = _mm_maddubs_epi16(ss[2], coeffs[2]);
  const __m128i res_67 = _mm_maddubs_epi16(ss[3], coeffs[3]);
  const __m128i res_0145 = _mm_add_epi16(res_01, res_45);
  const __m128i res_2367 = _mm_add_epi16(res_23, res_67);
  return _mm_add_epi16(res_0145, res_2367);
}

static INLINE __m256i convolve_2tap_avx2(const __m256i ss[1],
                                         const __m256i coeffs[1]) {
  return _mm256_maddubs_epi16(ss[0], coeffs[0]);
}

static INLINE __m256i convolve_4tap_avx2(const __m256i ss[2],
                                         const __m256i coeffs[2]) {
  const __m256i res_23 = _mm256_maddubs_epi16(ss[0], coeffs[0]);
  const __m256i res_45 = _mm256_maddubs_epi16(ss[1], coeffs[1]);
  return _mm256_add_epi16(res_23, res_45);
}

static INLINE __m256i convolve_6tap_avx2(const __m256i ss[3],
                                         const __m256i coeffs[3]) {
  const __m256i res_01 = _mm256_maddubs_epi16(ss[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(ss[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(ss[2], coeffs[2]);
  const __m256i res_0145 = _mm256_add_epi16(res_01, res_45);
  return _mm256_add_epi16(res_0145, res_23);
}

static INLINE __m256i convolve_8tap_avx2(const __m256i ss[4],
                                         const __m256i coeffs[4]) {
  const __m256i res_01 = _mm256_maddubs_epi16(ss[0], coeffs[0]);
  const __m256i res_23 = _mm256_maddubs_epi16(ss[1], coeffs[1]);
  const __m256i res_45 = _mm256_maddubs_epi16(ss[2], coeffs[2]);
  const __m256i res_67 = _mm256_maddubs_epi16(ss[3], coeffs[3]);
  const __m256i res_0145 = _mm256_add_epi16(res_01, res_45);
  const __m256i res_2367 = _mm256_add_epi16(res_23, res_67);
  return _mm256_add_epi16(res_0145, res_2367);
}

static INLINE __m128i convolve16_2tap_sse2(const __m128i ss[1],
                                           const __m128i coeffs[1]) {
  return _mm_madd_epi16(ss[0], coeffs[0]);
}

static INLINE __m128i convolve16_4tap_sse2(const __m128i ss[2],
                                           const __m128i coeffs[2]) {
  const __m128i res_01 = _mm_madd_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_madd_epi16(ss[1], coeffs[1]);
  return _mm_add_epi32(res_01, res_23);
}

static INLINE __m128i convolve16_6tap_sse2(const __m128i ss[3],
                                           const __m128i coeffs[3]) {
  const __m128i res_01 = _mm_madd_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_madd_epi16(ss[1], coeffs[1]);
  const __m128i res_45 = _mm_madd_epi16(ss[2], coeffs[2]);
  const __m128i res_0123 = _mm_add_epi32(res_01, res_23);
  return _mm_add_epi32(res_0123, res_45);
}

static INLINE __m128i convolve16_8tap_sse2(const __m128i ss[4],
                                           const __m128i coeffs[4]) {
  const __m128i res_01 = _mm_madd_epi16(ss[0], coeffs[0]);
  const __m128i res_23 = _mm_madd_epi16(ss[1], coeffs[1]);
  const __m128i res_45 = _mm_madd_epi16(ss[2], coeffs[2]);
  const __m128i res_67 = _mm_madd_epi16(ss[3], coeffs[3]);
  const __m128i res_0123 = _mm_add_epi32(res_01, res_23);
  const __m128i res_4567 = _mm_add_epi32(res_45, res_67);
  return _mm_add_epi32(res_0123, res_4567);
}

static INLINE __m256i convolve16_2tap_avx2(const __m256i ss[1],
                                           const __m256i coeffs[1]) {
  return _mm256_madd_epi16(ss[0], coeffs[0]);
}

static INLINE __m256i convolve16_4tap_avx2(const __m256i ss[2],
                                           const __m256i coeffs[2]) {
  const __m256i res_1 = _mm256_madd_epi16(ss[0], coeffs[0]);
  const __m256i res_2 = _mm256_madd_epi16(ss[1], coeffs[1]);
  return _mm256_add_epi32(res_1, res_2);
}

static INLINE __m256i convolve16_6tap_avx2(const __m256i ss[3],
                                           const __m256i coeffs[3]) {
  const __m256i res_01 = _mm256_madd_epi16(ss[0], coeffs[0]);
  const __m256i res_23 = _mm256_madd_epi16(ss[1], coeffs[1]);
  const __m256i res_45 = _mm256_madd_epi16(ss[2], coeffs[2]);
  const __m256i res_0123 = _mm256_add_epi32(res_01, res_23);
  return _mm256_add_epi32(res_0123, res_45);
}

static INLINE __m256i convolve16_8tap_avx2(const __m256i ss[4],
                                           const __m256i coeffs[4]) {
  const __m256i res_01 = _mm256_madd_epi16(ss[0], coeffs[0]);
  const __m256i res_23 = _mm256_madd_epi16(ss[1], coeffs[1]);
  const __m256i res_45 = _mm256_madd_epi16(ss[2], coeffs[2]);
  const __m256i res_67 = _mm256_madd_epi16(ss[3], coeffs[3]);
  const __m256i res_0123 = _mm256_add_epi32(res_01, res_23);
  const __m256i res_4567 = _mm256_add_epi32(res_45, res_67);
  return _mm256_add_epi32(res_0123, res_4567);
}

static INLINE __m256i x_convolve_4tap_avx2(const __m256i data,
                                           const __m256i coeffs[2],
                                           const __m256i filt[2]) {
  __m256i ss[2];

  ss[0] = _mm256_shuffle_epi8(data, filt[0]);
  ss[1] = _mm256_shuffle_epi8(data, filt[1]);

  return convolve_4tap_avx2(ss, coeffs);
}

static INLINE __m256i x_convolve_6tap_avx2(const __m256i data,
                                           const __m256i coeffs[3],
                                           const __m256i filt[3]) {
  __m256i ss[3];

  ss[0] = _mm256_shuffle_epi8(data, filt[0]);
  ss[1] = _mm256_shuffle_epi8(data, filt[1]);
  ss[2] = _mm256_shuffle_epi8(data, filt[2]);

  return convolve_6tap_avx2(ss, coeffs);
}

static INLINE __m256i x_convolve_8tap_avx2(const __m256i data,
                                           const __m256i coeffs[4],
                                           const __m256i filt[4]) {
  __m256i ss[4];

  ss[0] = _mm256_shuffle_epi8(data, filt[0]);
  ss[1] = _mm256_shuffle_epi8(data, filt[1]);
  ss[2] = _mm256_shuffle_epi8(data, filt[2]);
  ss[3] = _mm256_shuffle_epi8(data, filt[3]);

  return convolve_8tap_avx2(ss, coeffs);
}

static INLINE __m256i sr_y_round_avx2(const __m256i src) {
  const __m256i round = _mm256_set1_epi16(32);
  const __m256i dst = _mm256_add_epi16(src, round);
  return _mm256_srai_epi16(dst, FILTER_BITS - 1);
}

static INLINE __m128i xy_x_round_sse2(const __m128i src) {
  const __m128i round = _mm_set1_epi16(2);
  const __m128i dst = _mm_add_epi16(src, round);
  return _mm_srai_epi16(dst, 2);
}

static INLINE __m256i xy_x_round_avx2(const __m256i src) {
  const __m256i round = _mm256_set1_epi16(2);
  const __m256i dst = _mm256_add_epi16(src, round);
  return _mm256_srai_epi16(dst, 2);
}

static INLINE void xy_x_round_store_2x2_sse2(const __m128i res,
                                             int16_t *const dst) {
  const __m128i d = xy_x_round_sse2(res);
  _mm_storel_epi64((__m128i *)dst, d);
}

static INLINE void xy_x_round_store_4x2_sse2(const __m128i res,
                                             int16_t *const dst) {
  const __m128i d = xy_x_round_sse2(res);
  _mm_storeu_si128((__m128i *)dst, d);
}

static INLINE void xy_x_round_store_8x2_sse2(const __m128i res[2],
                                             int16_t *const dst) {
  __m128i r[2];

  r[0] = xy_x_round_sse2(res[0]);
  r[1] = xy_x_round_sse2(res[1]);
  _mm_storeu_si128((__m128i *)dst, r[0]);
  _mm_storeu_si128((__m128i *)(dst + 8), r[1]);
}

static INLINE void xy_x_round_store_8x2_avx2(const __m256i res,
                                             int16_t *const dst) {
  const __m256i d = xy_x_round_avx2(res);
  _mm256_storeu_si256((__m256i *)dst, d);
}

static INLINE void xy_x_round_store_32_avx2(const __m256i res[2],
                                            int16_t *const dst) {
  __m256i r[2];

  r[0] = xy_x_round_avx2(res[0]);
  r[1] = xy_x_round_avx2(res[1]);
  const __m256i d0 =
      _mm256_inserti128_si256(r[0], _mm256_castsi256_si128(r[1]), 1);
  const __m256i d1 =
      _mm256_inserti128_si256(r[1], _mm256_extracti128_si256(r[0], 1), 0);
  _mm256_storeu_si256((__m256i *)dst, d0);
  _mm256_storeu_si256((__m256i *)(dst + 16), d1);
}

static INLINE __m128i xy_y_round_sse2(const __m128i src) {
  const __m128i round = _mm_set1_epi32(1024);
  const __m128i dst = _mm_add_epi32(src, round);
  return _mm_srai_epi32(dst, 11);
}

static INLINE __m128i xy_y_round_half_pel_sse2(const __m128i src) {
  const __m128i round = _mm_set1_epi16(16);
  const __m128i dst = _mm_add_epi16(src, round);
  return _mm_srai_epi16(dst, 5);
}

static INLINE __m256i xy_y_round_avx2(const __m256i src) {
  const __m256i round = _mm256_set1_epi32(1024);
  const __m256i dst = _mm256_add_epi32(src, round);
  return _mm256_srai_epi32(dst, 11);
}

static INLINE __m256i xy_y_round_16_avx2(const __m256i r[2]) {
  const __m256i r0 = xy_y_round_avx2(r[0]);
  const __m256i r1 = xy_y_round_avx2(r[1]);
  return _mm256_packs_epi32(r0, r1);
}

static INLINE __m256i xy_y_round_half_pel_avx2(const __m256i src) {
  const __m256i round = _mm256_set1_epi16(16);
  const __m256i dst = _mm256_add_epi16(src, round);
  return _mm256_srai_epi16(dst, 5);
}

static INLINE void pack_store_2x2_sse2(const __m128i res, uint8_t *const dst,
                                       const ptrdiff_t stride) {
  const __m128i d = _mm_packus_epi16(res, res);
  *(int16_t *)dst = (int16_t)_mm_cvtsi128_si32(d);
  *(int16_t *)(dst + stride) = (int16_t)_mm_extract_epi16(d, 1);
}

static INLINE void pack_store_4x2_sse2(const __m128i res, uint8_t *const dst,
                                       const ptrdiff_t stride) {
  const __m128i d = _mm_packus_epi16(res, res);
  store_u8_4x2_sse2(d, dst, stride);
}

static INLINE void pack_store_4x2_avx2(const __m256i res, uint8_t *const dst,
                                       const ptrdiff_t stride) {
  const __m256i d = _mm256_packus_epi16(res, res);
  const __m128i d0 = _mm256_castsi256_si128(d);
  const __m128i d1 = _mm256_extracti128_si256(d, 1);

  xx_storel_32(dst, d0);
  xx_storel_32(dst + stride, d1);
}

static INLINE void pack_store_8x2_avx2(const __m256i res, uint8_t *const dst,
                                       const ptrdiff_t stride) {
  const __m256i d = _mm256_packus_epi16(res, res);
  const __m128i d0 = _mm256_castsi256_si128(d);
  const __m128i d1 = _mm256_extracti128_si256(d, 1);
  _mm_storel_epi64((__m128i *)dst, d0);
  _mm_storel_epi64((__m128i *)(dst + stride), d1);
}

static INLINE void pack_store_16x2_avx2(const __m256i res0, const __m256i res1,
                                        uint8_t *const dst,
                                        const ptrdiff_t stride) {
  const __m256i d = _mm256_packus_epi16(res0, res1);
  storeu_u8_16x2_avx2(d, dst, stride);
}

static INLINE void xy_y_pack_store_16x2_avx2(const __m256i res0,
                                             const __m256i res1,
                                             uint8_t *const dst,
                                             const ptrdiff_t stride) {
  const __m256i t = _mm256_packus_epi16(res0, res1);
  const __m256i d = _mm256_permute4x64_epi64(t, 0xD8);
  storeu_u8_16x2_avx2(d, dst, stride);
}

static INLINE void pack_store_32_avx2(const __m256i res0, const __m256i res1,
                                      uint8_t *const dst) {
  const __m256i t = _mm256_packus_epi16(res0, res1);
  const __m256i d = _mm256_permute4x64_epi64(t, 0xD8);
  _mm256_storeu_si256((__m256i *)dst, d);
}

static INLINE void xy_y_round_store_2x2_sse2(const __m128i res,
                                             uint8_t *const dst,
                                             const ptrdiff_t stride) {
  const __m128i r = xy_y_round_sse2(res);
  const __m128i rr = _mm_packs_epi32(r, r);
  pack_store_2x2_sse2(rr, dst, stride);
}

static INLINE void xy_y_round_store_4x2_avx2(const __m256i res,
                                             uint8_t *const dst,
                                             const ptrdiff_t stride) {
  const __m256i r = xy_y_round_avx2(res);
  const __m256i rr = _mm256_packs_epi32(r, r);
  pack_store_4x2_avx2(rr, dst, stride);
}

static INLINE void xy_y_pack_store_32_avx2(const __m256i res0,
                                           const __m256i res1,
                                           uint8_t *const dst) {
  const __m256i d = _mm256_packus_epi16(res0, res1);
  // d = _mm256_permute4x64_epi64(d, 0xD8);
  _mm256_storeu_si256((__m256i *)dst, d);
}

static INLINE void xy_y_round_store_32_avx2(const __m256i r0[2],
                                            const __m256i r1[2],
                                            uint8_t *const dst) {
  const __m256i ra = xy_y_round_16_avx2(r0);
  const __m256i rb = xy_y_round_16_avx2(r1);
  xy_y_pack_store_32_avx2(ra, rb, dst);
}

static INLINE void convolve_store_32_avx2(const __m256i res0,
                                          const __m256i res1,
                                          uint8_t *const dst) {
  const __m256i d = _mm256_packus_epi16(res0, res1);
  _mm256_storeu_si256((__m256i *)dst, d);
}

static INLINE __m128i sr_x_round_sse2(const __m128i src) {
  const __m128i round = _mm_set1_epi16(34);
  const __m128i dst = _mm_add_epi16(src, round);
  return _mm_srai_epi16(dst, 6);
}

static INLINE __m256i sr_x_round_avx2(const __m256i src) {
  const __m256i round = _mm256_set1_epi16(34);
  const __m256i dst = _mm256_add_epi16(src, round);
  return _mm256_srai_epi16(dst, 6);
}

static INLINE __m128i sr_y_round_sse2(const __m128i src) {
  const __m128i round = _mm_set1_epi16(32);
  const __m128i dst = _mm_add_epi16(src, round);
  return _mm_srai_epi16(dst, FILTER_BITS - 1);
}

static INLINE void sr_x_round_store_8x2_avx2(const __m256i res,
                                             uint8_t *const dst,
                                             const ptrdiff_t dst_stride) {
  const __m256i r = sr_x_round_avx2(res);
  pack_store_8x2_avx2(r, dst, dst_stride);
}

static INLINE void sr_x_round_store_16x2_avx2(const __m256i res[2],
                                              uint8_t *const dst,
                                              const ptrdiff_t dst_stride) {
  __m256i r[2];

  r[0] = sr_x_round_avx2(res[0]);
  r[1] = sr_x_round_avx2(res[1]);
  pack_store_16x2_avx2(r[0], r[1], dst, dst_stride);
}

static INLINE void sr_x_round_store_32_avx2(const __m256i res[2],
                                            uint8_t *const dst) {
  __m256i r[2];

  r[0] = sr_x_round_avx2(res[0]);
  r[1] = sr_x_round_avx2(res[1]);
  convolve_store_32_avx2(r[0], r[1], dst);
}

static INLINE void sr_y_round_store_8x2_avx2(const __m256i res,
                                             uint8_t *const dst,
                                             const ptrdiff_t dst_stride) {
  const __m256i r = sr_y_round_avx2(res);
  pack_store_8x2_avx2(r, dst, dst_stride);
}

static INLINE void sr_y_round_store_16x2_avx2(const __m256i res[2],
                                              uint8_t *const dst,
                                              const ptrdiff_t dst_stride) {
  __m256i r[2];

  r[0] = sr_y_round_avx2(res[0]);
  r[1] = sr_y_round_avx2(res[1]);
  pack_store_16x2_avx2(r[0], r[1], dst, dst_stride);
}

static INLINE void sr_y_2tap_32_avg_avx2(const uint8_t *const src,
                                         const __m256i s0, __m256i *const s1,
                                         uint8_t *const dst) {
  *s1 = _mm256_loadu_si256((__m256i *)src);
  const __m256i d = _mm256_avg_epu8(s0, *s1);
  _mm256_storeu_si256((__m256i *)dst, d);
}

static INLINE void sr_x_2tap_32_avg_avx2(const uint8_t *const src,
                                         uint8_t *const dst) {
  const __m256i s0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i s1 = _mm256_loadu_si256((__m256i *)(src + 1));
  const __m256i d = _mm256_avg_epu8(s0, s1);
  _mm256_storeu_si256((__m256i *)dst, d);
}

static INLINE __m128i x_convolve_2tap_2x2_sse4_1(const uint8_t *const src,
                                                 const ptrdiff_t stride,
                                                 const __m128i coeffs[1]) {
  const __m128i sfl =
      _mm_setr_epi8(0, 1, 1, 2, 4, 5, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i s_128 = load_u8_4x2_sse4_1(src, stride);
  const __m128i ss = _mm_shuffle_epi8(s_128, sfl);
  return convolve_2tap_ssse3(&ss, coeffs);
}

static INLINE __m128i x_convolve_2tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[1]) {
  const __m128i sfl =
      _mm_setr_epi8(0, 1, 1, 2, 2, 3, 3, 4, 8, 9, 9, 10, 10, 11, 11, 12);
  const __m128i s_128 = load_u8_8x2_sse2(src, stride);
  const __m128i ss = _mm_shuffle_epi8(s_128, sfl);
  return convolve_2tap_ssse3(&ss, coeffs);
}

static INLINE void x_convolve_2tap_8x2_ssse3(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m128i coeffs[1],
                                             __m128i r[2]) {
  __m128i ss[2];
  const __m128i s00 = _mm_loadu_si128((__m128i *)src);
  const __m128i s10 = _mm_loadu_si128((__m128i *)(src + stride));
  const __m128i s01 = _mm_srli_si128(s00, 1);
  const __m128i s11 = _mm_srli_si128(s10, 1);
  ss[0] = _mm_unpacklo_epi8(s00, s01);
  ss[1] = _mm_unpacklo_epi8(s10, s11);

  r[0] = convolve_2tap_ssse3(&ss[0], coeffs);
  r[1] = convolve_2tap_ssse3(&ss[1], coeffs);
}

static INLINE __m256i x_convolve_2tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[1]) {
  __m128i s_128[2][2];
  __m256i s_256[2];

  s_128[0][0] = _mm_loadu_si128((__m128i *)src);
  s_128[1][0] = _mm_loadu_si128((__m128i *)(src + stride));
  s_128[0][1] = _mm_srli_si128(s_128[0][0], 1);
  s_128[1][1] = _mm_srli_si128(s_128[1][0], 1);
  s_256[0] = _mm256_setr_m128i(s_128[0][0], s_128[1][0]);
  s_256[1] = _mm256_setr_m128i(s_128[0][1], s_128[1][1]);
  const __m256i ss = _mm256_unpacklo_epi8(s_256[0], s_256[1]);
  return convolve_2tap_avx2(&ss, coeffs);
}

static INLINE void x_convolve_2tap_16x2_avx2(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[1],
                                             __m256i r[2]) {
  const __m256i s0_256 = loadu_8bit_16x2_avx2(src, stride);
  const __m256i s1_256 = loadu_8bit_16x2_avx2(src + 1, stride);
  const __m256i s0 = _mm256_unpacklo_epi8(s0_256, s1_256);
  const __m256i s1 = _mm256_unpackhi_epi8(s0_256, s1_256);
  r[0] = convolve_2tap_avx2(&s0, coeffs);
  r[1] = convolve_2tap_avx2(&s1, coeffs);
}

static INLINE void x_convolve_2tap_32_avx2(const uint8_t *const src,
                                           const __m256i coeffs[1],
                                           __m256i r[2]) {
  const __m256i s0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i s1 = _mm256_loadu_si256((__m256i *)(src + 1));
  const __m256i ss0 = _mm256_unpacklo_epi8(s0, s1);
  const __m256i ss1 = _mm256_unpackhi_epi8(s0, s1);

  r[0] = convolve_2tap_avx2(&ss0, coeffs);
  r[1] = convolve_2tap_avx2(&ss1, coeffs);
}

static INLINE __m128i x_convolve_4tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[2]) {
  const __m128i sfl0 =
      _mm_setr_epi8(0, 1, 1, 2, 8, 9, 9, 10, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i sfl1 =
      _mm_setr_epi8(2, 3, 3, 4, 10, 11, 11, 12, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i s = load_u8_8x2_sse2(src, stride);
  __m128i ss[2];

  ss[0] = _mm_shuffle_epi8(s, sfl0);
  ss[1] = _mm_shuffle_epi8(s, sfl1);
  return convolve_4tap_ssse3(ss, coeffs);
}

static INLINE __m128i x_convolve_4tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[2]) {
  const __m128i s = load_u8_8x2_sse2(src, stride);
  const __m128i sfl0 =
      _mm_setr_epi8(0, 1, 1, 2, 2, 3, 3, 4, 8, 9, 9, 10, 10, 11, 11, 12);
  const __m128i sfl1 =
      _mm_setr_epi8(2, 3, 3, 4, 4, 5, 5, 6, 10, 11, 11, 12, 12, 13, 13, 14);
  __m128i ss[2];

  ss[0] = _mm_shuffle_epi8(s, sfl0);
  ss[1] = _mm_shuffle_epi8(s, sfl1);
  return convolve_4tap_ssse3(ss, coeffs);
}

static INLINE __m256i x_convolve_4tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[2],
                                               const __m256i filt[2]) {
  const __m256i s_256 = loadu_8bit_16x2_avx2(src, stride);
  return x_convolve_4tap_avx2(s_256, coeffs, filt);
}

static INLINE void x_convolve_4tap_16x2_avx2(const uint8_t *const src,
                                             const int32_t src_stride,
                                             const __m256i coeffs[2],
                                             const __m256i filt[2],
                                             __m256i r[2]) {
  r[0] = x_convolve_4tap_8x2_avx2(src + 0, src_stride, coeffs, filt);
  r[1] = x_convolve_4tap_8x2_avx2(src + 8, src_stride, coeffs, filt);
}

static INLINE void x_convolve_4tap_32_avx2(const uint8_t *const src,
                                           const __m256i coeffs[2],
                                           const __m256i filt[2],
                                           __m256i r[2]) {
  const __m256i s0_256 = _mm256_loadu_si256((__m256i *)src);
  const __m256i s1_256 = _mm256_loadu_si256((__m256i *)(src + 8));

  r[0] = x_convolve_4tap_avx2(s0_256, coeffs, filt);
  r[1] = x_convolve_4tap_avx2(s1_256, coeffs, filt);
}

static INLINE __m128i x_convolve_6tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[3]) {
  const __m128i sfl0 =
      _mm_setr_epi8(0, 1, 1, 2, 8, 9, 9, 10, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i sfl1 =
      _mm_setr_epi8(2, 3, 3, 4, 10, 11, 11, 12, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i sfl2 =
      _mm_setr_epi8(4, 5, 5, 6, 12, 13, 13, 14, 0, 0, 0, 0, 0, 0, 0, 0);

  const __m128i s = load_u8_8x2_sse2(src, stride);
  __m128i ss[3];

  ss[0] = _mm_shuffle_epi8(s, sfl0);
  ss[1] = _mm_shuffle_epi8(s, sfl1);
  ss[2] = _mm_shuffle_epi8(s, sfl2);
  return convolve_6tap_ssse3(ss, coeffs);
}

static INLINE __m128i x_convolve_6tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[3]) {
  const __m128i s = load_u8_8x2_sse2(src, stride);
  const __m128i sfl0 =
      _mm_setr_epi8(0, 1, 1, 2, 8, 9, 9, 10, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i sfl1 =
      _mm_setr_epi8(2, 3, 3, 4, 10, 11, 11, 12, 0, 0, 0, 0, 0, 0, 0, 0);
  const __m128i sfl2 =
      _mm_setr_epi8(4, 5, 5, 6, 12, 13, 13, 14, 0, 0, 0, 0, 0, 0, 0, 0);
  __m128i ss[3];

  ss[0] = _mm_shuffle_epi8(s, sfl0);
  ss[1] = _mm_shuffle_epi8(s, sfl1);
  ss[2] = _mm_shuffle_epi8(s, sfl2);
  return convolve_6tap_ssse3(ss, coeffs);
}

static INLINE __m256i x_convolve_6tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[3],
                                               const __m256i filt[3]) {
  const __m256i s_256 = loadu_8bit_16x2_avx2(src, stride);
  return x_convolve_6tap_avx2(s_256, coeffs, filt);
}

static INLINE void x_convolve_6tap_16x2_avx2(const uint8_t *const src,
                                             const int32_t src_stride,
                                             const __m256i coeffs[3],
                                             const __m256i filt[3],
                                             __m256i r[2]) {
  r[0] = x_convolve_6tap_8x2_avx2(src + 0, src_stride, coeffs, filt);
  r[1] = x_convolve_6tap_8x2_avx2(src + 8, src_stride, coeffs, filt);
}

static INLINE void x_convolve_6tap_32_avx2(const uint8_t *const src,
                                           const __m256i coeffs[3],
                                           const __m256i filt[3],
                                           __m256i r[2]) {
  const __m256i s0_256 = _mm256_loadu_si256((__m256i *)src);
  const __m256i s1_256 = _mm256_loadu_si256((__m256i *)(src + 8));

  r[0] = x_convolve_6tap_avx2(s0_256, coeffs, filt);
  r[1] = x_convolve_6tap_avx2(s1_256, coeffs, filt);
}

static INLINE __m256i x_convolve_8tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[4],
                                               const __m256i filt[4]) {
  const __m256i s_256 = loadu_8bit_16x2_avx2(src, stride);
  return x_convolve_8tap_avx2(s_256, coeffs, filt);
}

static AOM_FORCE_INLINE void x_convolve_8tap_16x2_avx2(const uint8_t *const src,
                                                       const int32_t src_stride,
                                                       const __m256i coeffs[4],
                                                       const __m256i filt[4],
                                                       __m256i r[2]) {
  r[0] = x_convolve_8tap_8x2_avx2(src + 0, src_stride, coeffs, filt);
  r[1] = x_convolve_8tap_8x2_avx2(src + 8, src_stride, coeffs, filt);
}

static AOM_FORCE_INLINE void x_convolve_8tap_32_avx2(const uint8_t *const src,
                                                     const __m256i coeffs[4],
                                                     const __m256i filt[4],
                                                     __m256i r[2]) {
  const __m256i s0_256 = _mm256_loadu_si256((__m256i *)src);
  const __m256i s1_256 = _mm256_loadu_si256((__m256i *)(src + 8));

  r[0] = x_convolve_8tap_avx2(s0_256, coeffs, filt);
  r[1] = x_convolve_8tap_avx2(s1_256, coeffs, filt);
}

static INLINE __m128i y_convolve_2tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[1],
                                                __m128i s_16[2]) {
  __m128i s_128[2];

  s_16[1] = _mm_cvtsi32_si128(*(int16_t *)(src + stride));
  s_128[0] = _mm_unpacklo_epi16(s_16[0], s_16[1]);
  s_16[0] = _mm_cvtsi32_si128(*(int16_t *)(src + 2 * stride));
  s_128[1] = _mm_unpacklo_epi16(s_16[1], s_16[0]);
  const __m128i ss = _mm_unpacklo_epi8(s_128[0], s_128[1]);
  return convolve_2tap_ssse3(&ss, coeffs);
}

static INLINE __m128i y_convolve_2tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[1],
                                                __m128i s_32[2]) {
  __m128i s_128[2];

  s_32[1] = _mm_cvtsi32_si128(*(int32_t *)(src + stride));
  s_128[0] = _mm_unpacklo_epi32(s_32[0], s_32[1]);
  s_32[0] = _mm_cvtsi32_si128(*(int32_t *)(src + 2 * stride));
  s_128[1] = _mm_unpacklo_epi32(s_32[1], s_32[0]);
  const __m128i ss = _mm_unpacklo_epi8(s_128[0], s_128[1]);
  return convolve_2tap_ssse3(&ss, coeffs);
}

static INLINE __m256i y_convolve_2tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[1],
                                               __m128i s_64[2]) {
  __m256i s_256[2];

  s_64[1] = _mm_loadl_epi64((__m128i *)(src + stride));
  s_256[0] = _mm256_setr_m128i(s_64[0], s_64[1]);
  s_64[0] = _mm_loadl_epi64((__m128i *)(src + 2 * stride));
  s_256[1] = _mm256_setr_m128i(s_64[1], s_64[0]);
  const __m256i ss = _mm256_unpacklo_epi8(s_256[0], s_256[1]);
  return convolve_2tap_avx2(&ss, coeffs);
}

static INLINE void y_convolve_2tap_16x2_avx2(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[1],
                                             __m128i s_128[2], __m256i r[2]) {
  __m256i s_256[2];

  s_128[1] = _mm_loadu_si128((__m128i *)(src + stride));
  s_256[0] = _mm256_setr_m128i(s_128[0], s_128[1]);
  s_128[0] = _mm_loadu_si128((__m128i *)(src + 2 * stride));
  s_256[1] = _mm256_setr_m128i(s_128[1], s_128[0]);
  const __m256i ss0 = _mm256_unpacklo_epi8(s_256[0], s_256[1]);
  const __m256i ss1 = _mm256_unpackhi_epi8(s_256[0], s_256[1]);
  r[0] = convolve_2tap_avx2(&ss0, coeffs);
  r[1] = convolve_2tap_avx2(&ss1, coeffs);
}

static INLINE void y_convolve_2tap_32_avx2(const uint8_t *const src,
                                           const __m256i coeffs[1],
                                           const __m256i s0, __m256i *const s1,
                                           __m256i r[2]) {
  *s1 = _mm256_loadu_si256((__m256i *)src);
  const __m256i ss0 = _mm256_unpacklo_epi8(s0, *s1);
  const __m256i ss1 = _mm256_unpackhi_epi8(s0, *s1);
  r[0] = convolve_2tap_avx2(&ss0, coeffs);
  r[1] = convolve_2tap_avx2(&ss1, coeffs);
}

static INLINE __m128i y_convolve_4tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[2],
                                                __m128i s_16[4],
                                                __m128i ss_128[2]) {
  s_16[3] = _mm_cvtsi32_si128(*(int16_t *)(src + stride));
  const __m128i src23 = _mm_unpacklo_epi16(s_16[2], s_16[3]);
  s_16[2] = _mm_cvtsi32_si128(*(int16_t *)(src + 2 * stride));
  const __m128i src34 = _mm_unpacklo_epi16(s_16[3], s_16[2]);
  ss_128[1] = _mm_unpacklo_epi8(src23, src34);
  return convolve_4tap_ssse3(ss_128, coeffs);
}

static INLINE __m128i y_convolve_4tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[2],
                                                __m128i s_32[4],
                                                __m128i ss_128[2]) {
  s_32[3] = _mm_cvtsi32_si128(*(int32_t *)(src + stride));
  const __m128i src23 = _mm_unpacklo_epi32(s_32[2], s_32[3]);
  s_32[2] = _mm_cvtsi32_si128(*(int32_t *)(src + 2 * stride));
  const __m128i src34 = _mm_unpacklo_epi32(s_32[3], s_32[2]);
  ss_128[1] = _mm_unpacklo_epi8(src23, src34);
  return convolve_4tap_ssse3(ss_128, coeffs);
}

static INLINE __m256i y_convolve_4tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[2],
                                               __m128i s_64[4],
                                               __m256i ss_256[2]) {
  s_64[3] = _mm_loadl_epi64((__m128i *)(src + stride));
  const __m256i src23 = _mm256_setr_m128i(s_64[2], s_64[3]);
  s_64[2] = _mm_loadl_epi64((__m128i *)(src + 2 * stride));
  const __m256i src34 = _mm256_setr_m128i(s_64[3], s_64[2]);
  ss_256[1] = _mm256_unpacklo_epi8(src23, src34);
  return convolve_4tap_avx2(ss_256, coeffs);
}

static INLINE void y_convolve_4tap_16x2_avx2(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[2],
                                             __m128i s_128[4],
                                             __m256i ss_256[4], __m256i r[2]) {
  s_128[3] = _mm_loadu_si128((__m128i *)(src + stride));
  const __m256i src23 = _mm256_setr_m128i(s_128[2], s_128[3]);
  s_128[2] = _mm_loadu_si128((__m128i *)(src + 2 * stride));
  const __m256i src34 = _mm256_setr_m128i(s_128[3], s_128[2]);
  ss_256[1] = _mm256_unpacklo_epi8(src23, src34);
  ss_256[3] = _mm256_unpackhi_epi8(src23, src34);
  r[0] = convolve_4tap_avx2(ss_256, coeffs);
  r[1] = convolve_4tap_avx2(ss_256 + 2, coeffs);
}

static INLINE __m128i y_convolve_6tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[3],
                                                __m128i s_16[6],
                                                __m128i ss_128[3]) {
  s_16[5] = _mm_cvtsi32_si128(*(int16_t *)(src + 3 * stride));
  const __m128i src45 = _mm_unpacklo_epi16(s_16[4], s_16[5]);
  s_16[4] = _mm_cvtsi32_si128(*(int16_t *)(src + 4 * stride));
  const __m128i src56 = _mm_unpacklo_epi16(s_16[5], s_16[4]);
  ss_128[2] = _mm_unpacklo_epi8(src45, src56);
  return convolve_6tap_ssse3(ss_128, coeffs);
}

static INLINE void y_convolve_4tap_32x2_avx2(
    const uint8_t *const src, const ptrdiff_t stride, const __m256i coeffs[2],
    __m256i s_256[4], __m256i ss_256[4], __m256i tt_256[4], __m256i r[4]) {
  s_256[3] = _mm256_loadu_si256((__m256i *)(src + 1 * stride));
  ss_256[1] = _mm256_unpacklo_epi8(s_256[2], s_256[3]);
  ss_256[3] = _mm256_unpackhi_epi8(s_256[2], s_256[3]);
  s_256[2] = _mm256_loadu_si256((__m256i *)(src + 2 * stride));
  tt_256[1] = _mm256_unpacklo_epi8(s_256[3], s_256[2]);
  tt_256[3] = _mm256_unpackhi_epi8(s_256[3], s_256[2]);
  r[0] = convolve_4tap_avx2(ss_256 + 0, coeffs);
  r[1] = convolve_4tap_avx2(ss_256 + 2, coeffs);
  r[2] = convolve_4tap_avx2(tt_256 + 0, coeffs);
  r[3] = convolve_4tap_avx2(tt_256 + 2, coeffs);
}

static INLINE __m128i y_convolve_6tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[3],
                                                __m128i s_32[6],
                                                __m128i ss_128[3]) {
  s_32[5] = _mm_cvtsi32_si128(*(int32_t *)(src + 3 * stride));
  const __m128i src45 = _mm_unpacklo_epi32(s_32[4], s_32[5]);
  s_32[4] = _mm_cvtsi32_si128(*(int32_t *)(src + 4 * stride));
  const __m128i src56 = _mm_unpacklo_epi32(s_32[5], s_32[4]);
  ss_128[2] = _mm_unpacklo_epi8(src45, src56);
  return convolve_6tap_ssse3(ss_128, coeffs);
}

static INLINE __m256i y_convolve_6tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[3],
                                               __m128i s_64[6],
                                               __m256i ss_256[3]) {
  s_64[5] = _mm_loadl_epi64((__m128i *)(src + 3 * stride));
  const __m256i src45 = _mm256_setr_m128i(s_64[4], s_64[5]);
  s_64[4] = _mm_loadl_epi64((__m128i *)(src + 4 * stride));
  const __m256i src56 = _mm256_setr_m128i(s_64[5], s_64[4]);
  ss_256[2] = _mm256_unpacklo_epi8(src45, src56);
  return convolve_6tap_avx2(ss_256, coeffs);
}

static INLINE void y_convolve_6tap_16x2_avx2(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[3],
                                             __m128i s_128[6],
                                             __m256i ss_256[6], __m256i r[2]) {
  s_128[5] = _mm_loadu_si128((__m128i *)(src + 3 * stride));
  const __m256i src45 = _mm256_setr_m128i(s_128[4], s_128[5]);
  s_128[4] = _mm_loadu_si128((__m128i *)(src + 4 * stride));
  const __m256i src56 = _mm256_setr_m128i(s_128[5], s_128[4]);
  ss_256[2] = _mm256_unpacklo_epi8(src45, src56);
  ss_256[5] = _mm256_unpackhi_epi8(src45, src56);
  r[0] = convolve_6tap_avx2(ss_256, coeffs);
  r[1] = convolve_6tap_avx2(ss_256 + 3, coeffs);
}

static INLINE void y_convolve_6tap_32x2_avx2(
    const uint8_t *const src, const ptrdiff_t stride, const __m256i coeffs[3],
    __m256i s_256[6], __m256i ss_256[6], __m256i tt_256[6], __m256i r[4]) {
  s_256[5] = _mm256_loadu_si256((__m256i *)(src + 3 * stride));
  ss_256[2] = _mm256_unpacklo_epi8(s_256[4], s_256[5]);
  ss_256[5] = _mm256_unpackhi_epi8(s_256[4], s_256[5]);
  s_256[4] = _mm256_loadu_si256((__m256i *)(src + 4 * stride));
  tt_256[2] = _mm256_unpacklo_epi8(s_256[5], s_256[4]);
  tt_256[5] = _mm256_unpackhi_epi8(s_256[5], s_256[4]);
  r[0] = convolve_6tap_avx2(ss_256 + 0, coeffs);
  r[1] = convolve_6tap_avx2(ss_256 + 3, coeffs);
  r[2] = convolve_6tap_avx2(tt_256 + 0, coeffs);
  r[3] = convolve_6tap_avx2(tt_256 + 3, coeffs);
}

static INLINE __m128i y_convolve_8tap_2x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[4],
                                                __m128i s_16[8],
                                                __m128i ss_128[4]) {
  s_16[7] = _mm_cvtsi32_si128(*(int16_t *)(src + 7 * stride));
  const __m128i src67 = _mm_unpacklo_epi16(s_16[6], s_16[7]);
  s_16[6] = _mm_cvtsi32_si128(*(int16_t *)(src + 8 * stride));
  const __m128i src78 = _mm_unpacklo_epi16(s_16[7], s_16[6]);
  ss_128[3] = _mm_unpacklo_epi8(src67, src78);
  return convolve_8tap_ssse3(ss_128, coeffs);
}

static INLINE __m128i y_convolve_8tap_4x2_ssse3(const uint8_t *const src,
                                                const ptrdiff_t stride,
                                                const __m128i coeffs[4],
                                                __m128i s_32[8],
                                                __m128i ss_128[4]) {
  s_32[7] = _mm_cvtsi32_si128(*(int32_t *)(src + 7 * stride));
  const __m128i src67 = _mm_unpacklo_epi32(s_32[6], s_32[7]);
  s_32[6] = _mm_cvtsi32_si128(*(int32_t *)(src + 8 * stride));
  const __m128i src78 = _mm_unpacklo_epi32(s_32[7], s_32[6]);
  ss_128[3] = _mm_unpacklo_epi8(src67, src78);
  return convolve_8tap_ssse3(ss_128, coeffs);
}

static INLINE __m256i y_convolve_8tap_8x2_avx2(const uint8_t *const src,
                                               const ptrdiff_t stride,
                                               const __m256i coeffs[4],
                                               __m128i s_64[8],
                                               __m256i ss_256[4]) {
  s_64[7] = _mm_loadl_epi64((__m128i *)(src + 7 * stride));
  const __m256i src67 = _mm256_setr_m128i(s_64[6], s_64[7]);
  s_64[6] = _mm_loadl_epi64((__m128i *)(src + 8 * stride));
  const __m256i src78 = _mm256_setr_m128i(s_64[7], s_64[6]);
  ss_256[3] = _mm256_unpacklo_epi8(src67, src78);
  return convolve_8tap_avx2(ss_256, coeffs);
}

static INLINE void y_convolve_8tap_16x2_avx2(const uint8_t *const src,
                                             const ptrdiff_t stride,
                                             const __m256i coeffs[4],
                                             __m128i s_128[8],
                                             __m256i ss_256[8], __m256i r[2]) {
  s_128[7] = _mm_loadu_si128((__m128i *)(src + 7 * stride));
  const __m256i src67 = _mm256_setr_m128i(s_128[6], s_128[7]);
  s_128[6] = _mm_loadu_si128((__m128i *)(src + 8 * stride));
  const __m256i src78 = _mm256_setr_m128i(s_128[7], s_128[6]);
  ss_256[3] = _mm256_unpacklo_epi8(src67, src78);
  ss_256[7] = _mm256_unpackhi_epi8(src67, src78);
  r[0] = convolve_8tap_avx2(ss_256, coeffs);
  r[1] = convolve_8tap_avx2(ss_256 + 4, coeffs);
}

static INLINE void y_convolve_8tap_32x2_avx2(
    const uint8_t *const src, const ptrdiff_t stride, const __m256i coeffs[4],
    __m256i s_256[8], __m256i ss_256[8], __m256i tt_256[8], __m256i r[4]) {
  s_256[7] = _mm256_loadu_si256((__m256i *)(src + 7 * stride));
  ss_256[3] = _mm256_unpacklo_epi8(s_256[6], s_256[7]);
  ss_256[7] = _mm256_unpackhi_epi8(s_256[6], s_256[7]);
  s_256[6] = _mm256_loadu_si256((__m256i *)(src + 8 * stride));
  tt_256[3] = _mm256_unpacklo_epi8(s_256[7], s_256[6]);
  tt_256[7] = _mm256_unpackhi_epi8(s_256[7], s_256[6]);
  r[0] = convolve_8tap_avx2(ss_256 + 0, coeffs);
  r[1] = convolve_8tap_avx2(ss_256 + 4, coeffs);
  r[2] = convolve_8tap_avx2(tt_256 + 0, coeffs);
  r[3] = convolve_8tap_avx2(tt_256 + 4, coeffs);
}

static INLINE void xy_x_convolve_2tap_32_avx2(const uint8_t *const src,
                                              const __m256i coeffs[1],
                                              __m256i r[2]) {
  const __m256i s0 = _mm256_loadu_si256((__m256i *)src);
  const __m256i s1 = _mm256_loadu_si256((__m256i *)(src + 1));
  const __m256i ss0 = _mm256_unpacklo_epi8(s0, s1);
  const __m256i ss1 = _mm256_unpackhi_epi8(s0, s1);

  r[0] = convolve_2tap_avx2(&ss0, coeffs);
  r[1] = convolve_2tap_avx2(&ss1, coeffs);
}

static INLINE void xy_x_2tap_32_avx2(const uint8_t *const src,
                                     const __m256i coeffs[1],
                                     int16_t *const dst) {
  __m256i r[2];

  xy_x_convolve_2tap_32_avx2(src, coeffs, r);
  const __m256i d0 = xy_x_round_avx2(r[0]);
  const __m256i d1 = xy_x_round_avx2(r[1]);
  _mm256_storeu_si256((__m256i *)dst, d0);
  _mm256_storeu_si256((__m256i *)(dst + 16), d1);
}

static INLINE void xy_x_4tap_32_avx2(const uint8_t *const src,
                                     const __m256i coeffs[2],
                                     const __m256i filt[2],
                                     int16_t *const dst) {
  __m256i r[2];

  x_convolve_4tap_32_avx2(src, coeffs, filt, r);
  const __m256i d0 = xy_x_round_avx2(r[0]);
  const __m256i d1 = xy_x_round_avx2(r[1]);
  _mm256_storeu_si256((__m256i *)dst, d0);
  _mm256_storeu_si256((__m256i *)(dst + 16), d1);
}

static INLINE void xy_x_6tap_32_avx2(const uint8_t *const src,
                                     const __m256i coeffs[3],
                                     const __m256i filt[3],
                                     int16_t *const dst) {
  __m256i r[2];

  x_convolve_6tap_32_avx2(src, coeffs, filt, r);
  const __m256i d0 = xy_x_round_avx2(r[0]);
  const __m256i d1 = xy_x_round_avx2(r[1]);
  _mm256_storeu_si256((__m256i *)dst, d0);
  _mm256_storeu_si256((__m256i *)(dst + 16), d1);
}

static INLINE void xy_x_8tap_32_avx2(const uint8_t *const src,
                                     const __m256i coeffs[4],
                                     const __m256i filt[4],
                                     int16_t *const dst) {
  __m256i r[2];

  x_convolve_8tap_32_avx2(src, coeffs, filt, r);
  const __m256i d0 = xy_x_round_avx2(r[0]);
  const __m256i d1 = xy_x_round_avx2(r[1]);
  _mm256_storeu_si256((__m256i *)dst, d0);
  _mm256_storeu_si256((__m256i *)(dst + 16), d1);
}

static INLINE __m128i xy_y_convolve_2tap_2x2_sse2(const int16_t *const src,
                                                  __m128i s_32[2],
                                                  const __m128i coeffs[1]) {
  __m128i s_128[2];

  s_32[1] = _mm_cvtsi32_si128(*(int32_t *)(src + 2));
  s_128[0] = _mm_unpacklo_epi32(s_32[0], s_32[1]);
  s_32[0] = _mm_cvtsi32_si128(*(int32_t *)(src + 2 * 2));
  s_128[1] = _mm_unpacklo_epi32(s_32[1], s_32[0]);
  const __m128i ss = _mm_unpacklo_epi16(s_128[0], s_128[1]);
  return convolve16_2tap_sse2(&ss, coeffs);
}

static INLINE __m128i xy_y_convolve_2tap_2x2_half_pel_sse2(
    const int16_t *const src, __m128i s_32[2]) {
  __m128i s_128[2];

  s_32[1] = _mm_cvtsi32_si128(*(int32_t *)(src + 2));
  s_128[0] = _mm_unpacklo_epi32(s_32[0], s_32[1]);
  s_32[0] = _mm_cvtsi32_si128(*(int32_t *)(src + 2 * 2));
  s_128[1] = _mm_unpacklo_epi32(s_32[1], s_32[0]);
  return _mm_add_epi16(s_128[0], s_128[1]);
}

static INLINE void xy_y_convolve_2tap_4x2_sse2(const int16_t *const src,
                                               __m128i s_64[2],
                                               const __m128i coeffs[1],
                                               __m128i r[2]) {
  __m128i s_128[2];

  s_64[1] = _mm_loadl_epi64((__m128i *)(src + 4));
  s_128[0] = _mm_unpacklo_epi64(s_64[0], s_64[1]);
  s_64[0] = _mm_loadl_epi64((__m128i *)(src + 2 * 4));
  s_128[1] = _mm_unpacklo_epi64(s_64[1], s_64[0]);
  const __m128i ss0 = _mm_unpacklo_epi16(s_128[0], s_128[1]);
  const __m128i ss1 = _mm_unpackhi_epi16(s_128[0], s_128[1]);
  r[0] = convolve16_2tap_sse2(&ss0, coeffs);
  r[1] = convolve16_2tap_sse2(&ss1, coeffs);
}

static INLINE __m128i xy_y_convolve_2tap_4x2_half_pel_sse2(
    const int16_t *const src, __m128i s_64[2]) {
  __m128i s_128[2];

  s_64[1] = _mm_loadl_epi64((__m128i *)(src + 4));
  s_128[0] = _mm_unpacklo_epi64(s_64[0], s_64[1]);
  s_64[0] = _mm_loadl_epi64((__m128i *)(src + 2 * 4));
  s_128[1] = _mm_unpacklo_epi64(s_64[1], s_64[0]);
  return _mm_add_epi16(s_128[0], s_128[1]);
}

static INLINE void xy_y_convolve_2tap_16_avx2(const __m256i s0,
                                              const __m256i s1,
                                              const __m256i coeffs[1],
                                              __m256i r[2]) {
  const __m256i ss0 = _mm256_unpacklo_epi16(s0, s1);
  const __m256i ss1 = _mm256_unpackhi_epi16(s0, s1);
  r[0] = convolve16_2tap_avx2(&ss0, coeffs);
  r[1] = convolve16_2tap_avx2(&ss1, coeffs);
}

static INLINE void xy_y_convolve_2tap_8x2_avx2(const int16_t *const src,
                                               __m128i s_128[2],
                                               const __m256i coeffs[1],
                                               __m256i r[2]) {
  __m256i s_256[2];
  s_128[1] = _mm_loadu_si128((__m128i *)(src + 8));
  s_256[0] = _mm256_setr_m128i(s_128[0], s_128[1]);
  s_128[0] = _mm_loadu_si128((__m128i *)(src + 2 * 8));
  s_256[1] = _mm256_setr_m128i(s_128[1], s_128[0]);
  xy_y_convolve_2tap_16_avx2(s_256[0], s_256[1], coeffs, r);
}

static INLINE __m256i xy_y_convolve_2tap_8x2_half_pel_avx2(
    const int16_t *const src, __m128i s_128[2]) {
  __m256i s_256[2];
  s_128[1] = _mm_loadu_si128((__m128i *)(src + 8));
  s_256[0] = _mm256_setr_m128i(s_128[0], s_128[1]);
  s_128[0] = _mm_loadu_si128((__m128i *)(src + 2 * 8));
  s_256[1] = _mm256_setr_m128i(s_128[1], s_128[0]);
  return _mm256_add_epi16(s_256[0], s_256[1]);
}

static INLINE void xy_y_convolve_2tap_16x2_half_pel_avx2(
    const int16_t *const src, __m256i s_256[2], __m256i r[2]) {
  s_256[1] = _mm256_loadu_si256((__m256i *)(src + 16));
  r[0] = _mm256_add_epi16(s_256[0], s_256[1]);
  s_256[0] = _mm256_loadu_si256((__m256i *)(src + 2 * 16));
  r[1] = _mm256_add_epi16(s_256[1], s_256[0]);
}

static INLINE void xy_y_store_16x2_avx2(const __m256i r[2], uint8_t *const dst,
                                        const ptrdiff_t stride) {
  const __m256i t = _mm256_packus_epi16(r[0], r[1]);
  const __m256i d = _mm256_permute4x64_epi64(t, 0xD8);
  storeu_u8_16x2_avx2(d, dst, stride);
}

static INLINE void xy_y_convolve_2tap_16x2_avx2(const int16_t *const src,
                                                __m256i s[2],
                                                const __m256i coeffs[1],
                                                __m256i r[4]) {
  s[1] = _mm256_loadu_si256((__m256i *)(src + 16));
  xy_y_convolve_2tap_16_avx2(s[0], s[1], coeffs, r + 0);
  s[0] = _mm256_loadu_si256((__m256i *)(src + 2 * 16));
  xy_y_convolve_2tap_16_avx2(s[1], s[0], coeffs, r + 2);
}

static INLINE void xy_y_convolve_2tap_32_avx2(const int16_t *const src,
                                              const __m256i s0[2],
                                              __m256i s1[2],
                                              const __m256i coeffs[1],
                                              __m256i r[4]) {
  s1[0] = _mm256_loadu_si256((__m256i *)src);
  s1[1] = _mm256_loadu_si256((__m256i *)(src + 16));
  xy_y_convolve_2tap_16_avx2(s0[0], s1[0], coeffs, r + 0);
  xy_y_convolve_2tap_16_avx2(s0[1], s1[1], coeffs, r + 2);
}

static INLINE void xy_y_convolve_2tap_32_all_avx2(const int16_t *const src,
                                                  const __m256i s0[2],
                                                  __m256i s1[2],
                                                  const __m256i coeffs[1],
                                                  uint8_t *const dst) {
  __m256i r[4];

  xy_y_convolve_2tap_32_avx2(src, s0, s1, coeffs, r);
  xy_y_round_store_32_avx2(r + 0, r + 2, dst);
}

static INLINE void xy_y_convolve_2tap_half_pel_32_avx2(const int16_t *const src,
                                                       const __m256i s0[2],
                                                       __m256i s1[2],
                                                       __m256i r[2]) {
  s1[0] = _mm256_loadu_si256((__m256i *)src);
  s1[1] = _mm256_loadu_si256((__m256i *)(src + 16));
  r[0] = _mm256_add_epi16(s0[0], s1[0]);
  r[1] = _mm256_add_epi16(s0[1], s1[1]);
}

static INLINE void xy_y_convolve_2tap_half_pel_32_all_avx2(
    const int16_t *const src, const __m256i s0[2], __m256i s1[2],
    uint8_t *const dst) {
  __m256i r[2];

  xy_y_convolve_2tap_half_pel_32_avx2(src, s0, s1, r);
  r[0] = xy_y_round_half_pel_avx2(r[0]);
  r[1] = xy_y_round_half_pel_avx2(r[1]);
  xy_y_pack_store_32_avx2(r[0], r[1], dst);
}

static INLINE __m128i xy_y_convolve_4tap_2x2_sse2(const int16_t *const src,
                                                  __m128i s_32[4],
                                                  __m128i ss_128[2],
                                                  const __m128i coeffs[2]) {
  s_32[3] = _mm_cvtsi32_si128(*(int32_t *)(src + 3 * 2));
  const __m128i src23 = _mm_unpacklo_epi32(s_32[2], s_32[3]);
  s_32[2] = _mm_cvtsi32_si128(*(int32_t *)(src + 4 * 2));
  const __m128i src34 = _mm_unpacklo_epi32(s_32[3], s_32[2]);
  ss_128[1] = _mm_unpacklo_epi16(src23, src34);
  const __m128i r = convolve16_4tap_sse2(ss_128, coeffs);
  ss_128[0] = ss_128[1];
  return r;
}

static INLINE __m256i xy_y_convolve_4tap_4x2_avx2(const int16_t *const src,
                                                  __m128i s_64[4],
                                                  __m256i ss_256[2],
                                                  const __m256i coeffs[2]) {
  __m256i s_256[2];
  s_64[3] = _mm_loadl_epi64((__m128i *)(src + 3 * 4));
  s_256[0] = _mm256_setr_m128i(s_64[2], s_64[3]);
  s_64[2] = _mm_loadl_epi64((__m128i *)(src + 4 * 4));
  s_256[1] = _mm256_setr_m128i(s_64[3], s_64[2]);
  ss_256[1] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  const __m256i r = convolve16_4tap_avx2(ss_256, coeffs);
  ss_256[0] = ss_256[1];
  return r;
}

static INLINE void xy_y_convolve_4tap_16_avx2(const __m256i *const ss,
                                              const __m256i coeffs[2],
                                              __m256i r[2]) {
  r[0] = convolve16_4tap_avx2(ss, coeffs);
  r[1] = convolve16_4tap_avx2(ss + 2, coeffs);
}

static INLINE void xy_y_convolve_4tap_8x2_avx2(const int16_t *const src,
                                               __m256i ss_256[4],
                                               const __m256i coeffs[2],
                                               __m256i r[2]) {
  __m256i s_256[2];
  s_256[0] = _mm256_loadu_si256((__m256i *)(src + 2 * 8));
  s_256[1] = _mm256_loadu_si256((__m256i *)(src + 3 * 8));
  ss_256[1] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[0], s_256[1]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r);
  ss_256[0] = ss_256[1];
  ss_256[2] = ss_256[3];
}

static INLINE void xy_y_convolve_4tap_8x2_half_pel_avx2(
    const int16_t *const src, const __m256i coeffs[1], __m256i s_256[4],
    __m256i r[2]) {
  __m256i a_256[2];
  s_256[2] = _mm256_loadu_si256((__m256i *)(src + 2 * 8));
  s_256[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 8));
  a_256[0] = _mm256_add_epi16(s_256[0], s_256[3]);
  a_256[1] = _mm256_add_epi16(s_256[1], s_256[2]);
  xy_y_convolve_2tap_16_avx2(a_256[0], a_256[1], coeffs, r);
  s_256[0] = s_256[2];
  s_256[1] = s_256[3];
}

static INLINE void xy_y_convolve_4tap_16x2_avx2(
    const int16_t *const src, __m256i s_256[4], __m256i ss_256[4],
    __m256i tt_256[4], const __m256i coeffs[2], __m256i r[4]) {
  s_256[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 16));
  ss_256[1] = _mm256_unpacklo_epi16(s_256[2], s_256[3]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[2], s_256[3]);
  s_256[2] = _mm256_loadu_si256((__m256i *)(src + 4 * 16));
  tt_256[1] = _mm256_unpacklo_epi16(s_256[3], s_256[2]);
  tt_256[3] = _mm256_unpackhi_epi16(s_256[3], s_256[2]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r + 0);
  xy_y_convolve_4tap_16_avx2(tt_256, coeffs, r + 2);
  ss_256[0] = ss_256[1];
  ss_256[2] = ss_256[3];
  tt_256[0] = tt_256[1];
  tt_256[2] = tt_256[3];
}

static INLINE void xy_y_convolve_4tap_32x2_avx2(
    const int16_t *const src, const ptrdiff_t stride, __m256i s_256[4],
    __m256i ss_256[4], __m256i tt_256[4], const __m256i coeffs[2],
    __m256i r[4]) {
  s_256[3] = _mm256_loadu_si256((__m256i *)(src + 3 * stride));
  ss_256[1] = _mm256_unpacklo_epi16(s_256[2], s_256[3]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[2], s_256[3]);
  s_256[2] = _mm256_loadu_si256((__m256i *)(src + 4 * stride));
  tt_256[1] = _mm256_unpacklo_epi16(s_256[3], s_256[2]);
  tt_256[3] = _mm256_unpackhi_epi16(s_256[3], s_256[2]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r + 0);
  xy_y_convolve_4tap_16_avx2(tt_256, coeffs, r + 2);
  ss_256[0] = ss_256[1];
  ss_256[2] = ss_256[3];
  tt_256[0] = tt_256[1];
  tt_256[2] = tt_256[3];
}

static INLINE void xy_y_convolve_4tap_16x2_half_pelavx2(
    const int16_t *const src, __m256i s_256[5], const __m256i coeffs[1],
    __m256i r[4]) {
  __m256i a_256[2];

  s_256[3] = _mm256_loadu_si256((__m256i *)(src + 3 * 16));
  s_256[4] = _mm256_loadu_si256((__m256i *)(src + 4 * 16));

  a_256[0] = _mm256_add_epi16(s_256[0], s_256[3]);
  a_256[1] = _mm256_add_epi16(s_256[1], s_256[2]);
  xy_y_convolve_2tap_16_avx2(a_256[0], a_256[1], coeffs, r + 0);

  a_256[0] = _mm256_add_epi16(s_256[1], s_256[4]);
  a_256[1] = _mm256_add_epi16(s_256[2], s_256[3]);
  xy_y_convolve_2tap_16_avx2(a_256[0], a_256[1], coeffs, r + 2);

  s_256[0] = s_256[2];
  s_256[1] = s_256[3];
  s_256[2] = s_256[4];
}

static INLINE __m128i xy_y_convolve_6tap_2x2_sse2(const int16_t *const src,
                                                  __m128i s_32[6],
                                                  __m128i ss_128[3],
                                                  const __m128i coeffs[3]) {
  s_32[5] = _mm_cvtsi32_si128(*(int32_t *)(src + 5 * 2));
  const __m128i src45 = _mm_unpacklo_epi32(s_32[4], s_32[5]);
  s_32[4] = _mm_cvtsi32_si128(*(int32_t *)(src + 6 * 2));
  const __m128i src56 = _mm_unpacklo_epi32(s_32[5], s_32[4]);
  ss_128[2] = _mm_unpacklo_epi16(src45, src56);
  const __m128i r = convolve16_6tap_sse2(ss_128, coeffs);
  ss_128[0] = ss_128[1];
  ss_128[1] = ss_128[2];
  return r;
}

static INLINE __m256i xy_y_convolve_6tap_4x2_avx2(const int16_t *const src,
                                                  __m128i s_64[6],
                                                  __m256i ss_256[3],
                                                  const __m256i coeffs[3]) {
  __m256i s_256[2];
  s_64[5] = _mm_loadl_epi64((__m128i *)(src + 5 * 4));
  s_256[0] = _mm256_setr_m128i(s_64[4], s_64[5]);
  s_64[4] = _mm_loadl_epi64((__m128i *)(src + 6 * 4));
  s_256[1] = _mm256_setr_m128i(s_64[5], s_64[4]);
  ss_256[2] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  const __m256i r = convolve16_6tap_avx2(ss_256, coeffs);
  ss_256[0] = ss_256[1];
  ss_256[1] = ss_256[2];
  return r;
}

static INLINE void xy_y_convolve_6tap_16_avx2(const __m256i ss[6],
                                              const __m256i coeffs[3],
                                              __m256i r[2]) {
  r[0] = convolve16_6tap_avx2(ss, coeffs);
  r[1] = convolve16_6tap_avx2(ss + 3, coeffs);
}

static INLINE void xy_y_convolve_6tap_8x2_avx2(const int16_t *const src,
                                               __m256i ss_256[6],
                                               const __m256i coeffs[3],
                                               __m256i r[2]) {
  __m256i s_256[2];
  s_256[0] = _mm256_loadu_si256((__m256i *)(src + 4 * 8));
  s_256[1] = _mm256_loadu_si256((__m256i *)(src + 5 * 8));
  ss_256[2] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  ss_256[5] = _mm256_unpackhi_epi16(s_256[0], s_256[1]);
  xy_y_convolve_6tap_16_avx2(ss_256, coeffs, r);
  ss_256[0] = ss_256[1];
  ss_256[1] = ss_256[2];
  ss_256[3] = ss_256[4];
  ss_256[4] = ss_256[5];
}

static INLINE void xy_y_convolve_6tap_8x2_half_pel_avx2(
    const int16_t *const src, const __m256i coeffs[2], __m256i s_256[6],
    __m256i r[2]) {
  __m256i a_256[2], ss_256[4];
  s_256[4] = _mm256_loadu_si256((__m256i *)(src + 4 * 8));
  s_256[5] = _mm256_loadu_si256((__m256i *)(src + 5 * 8));
  a_256[0] = _mm256_add_epi16(s_256[0], s_256[5]);
  a_256[1] = _mm256_add_epi16(s_256[1], s_256[4]);
  ss_256[0] = _mm256_unpacklo_epi16(a_256[0], a_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(s_256[2], s_256[3]);
  ss_256[2] = _mm256_unpackhi_epi16(a_256[0], a_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[2], s_256[3]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r);
  s_256[0] = s_256[2];
  s_256[1] = s_256[3];
  s_256[2] = s_256[4];
  s_256[3] = s_256[5];
}

static INLINE void xy_y_convolve_6tap_16x2_avx2(
    const int16_t *const src, const ptrdiff_t stride, __m256i s_256[6],
    __m256i ss_256[6], __m256i tt_256[6], const __m256i coeffs[3],
    __m256i r[4]) {
  s_256[5] = _mm256_loadu_si256((__m256i *)(src + 5 * stride));
  ss_256[2] = _mm256_unpacklo_epi16(s_256[4], s_256[5]);
  ss_256[5] = _mm256_unpackhi_epi16(s_256[4], s_256[5]);
  s_256[4] = _mm256_loadu_si256((__m256i *)(src + 6 * stride));
  tt_256[2] = _mm256_unpacklo_epi16(s_256[5], s_256[4]);
  tt_256[5] = _mm256_unpackhi_epi16(s_256[5], s_256[4]);

  xy_y_convolve_6tap_16_avx2(ss_256, coeffs, r + 0);
  xy_y_convolve_6tap_16_avx2(tt_256, coeffs, r + 2);

  ss_256[0] = ss_256[1];
  ss_256[1] = ss_256[2];
  ss_256[3] = ss_256[4];
  ss_256[4] = ss_256[5];

  tt_256[0] = tt_256[1];
  tt_256[1] = tt_256[2];
  tt_256[3] = tt_256[4];
  tt_256[4] = tt_256[5];
}

static INLINE void xy_y_convolve_6tap_16x2_half_pel_avx2(
    const int16_t *const src, const ptrdiff_t stride, __m256i s_256[6],
    __m256i ss_256[4], const __m256i coeffs[2], __m256i r[4]) {
  __m256i a_256[2];

  s_256[5] = _mm256_loadu_si256((__m256i *)(src + 5 * stride));
  a_256[0] = _mm256_add_epi16(s_256[0], s_256[5]);
  a_256[1] = _mm256_add_epi16(s_256[1], s_256[4]);
  ss_256[0] = _mm256_unpacklo_epi16(a_256[0], a_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(s_256[2], s_256[3]);
  ss_256[2] = _mm256_unpackhi_epi16(a_256[0], a_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[2], s_256[3]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r + 0);

  a_256[1] = _mm256_add_epi16(s_256[2], s_256[5]);
  s_256[0] = s_256[2];
  s_256[2] = s_256[4];
  s_256[4] = _mm256_loadu_si256((__m256i *)(src + 6 * stride));
  a_256[0] = _mm256_add_epi16(s_256[1], s_256[4]);
  s_256[1] = s_256[3];
  s_256[3] = s_256[5];
  ss_256[0] = _mm256_unpacklo_epi16(a_256[0], a_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(s_256[1], s_256[2]);
  ss_256[2] = _mm256_unpackhi_epi16(a_256[0], a_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(s_256[1], s_256[2]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r + 2);
}

static INLINE __m128i xy_y_convolve_8tap_2x2_sse2(const int16_t *const src,
                                                  __m128i s_32[8],
                                                  __m128i ss_128[4],
                                                  const __m128i coeffs[4]) {
  s_32[7] = _mm_cvtsi32_si128(*(int32_t *)(src + 7 * 2));
  const __m128i src67 = _mm_unpacklo_epi32(s_32[6], s_32[7]);
  s_32[6] = _mm_cvtsi32_si128(*(int32_t *)(src + 8 * 2));
  const __m128i src78 = _mm_unpacklo_epi32(s_32[7], s_32[6]);
  ss_128[3] = _mm_unpacklo_epi16(src67, src78);
  const __m128i r = convolve16_8tap_sse2(ss_128, coeffs);
  ss_128[0] = ss_128[1];
  ss_128[1] = ss_128[2];
  ss_128[2] = ss_128[3];
  return r;
}

static INLINE __m256i xy_y_convolve_8tap_4x2_avx2(const int16_t *const src,
                                                  __m128i s_64[8],
                                                  __m256i ss_256[4],
                                                  const __m256i coeffs[4]) {
  __m256i s_256[2];
  s_64[7] = _mm_loadl_epi64((__m128i *)(src + 7 * 4));
  s_256[0] = _mm256_setr_m128i(s_64[6], s_64[7]);
  s_64[6] = _mm_loadl_epi64((__m128i *)(src + 8 * 4));
  s_256[1] = _mm256_setr_m128i(s_64[7], s_64[6]);
  ss_256[3] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  const __m256i r = convolve16_8tap_avx2(ss_256, coeffs);
  ss_256[0] = ss_256[1];
  ss_256[1] = ss_256[2];
  ss_256[2] = ss_256[3];
  return r;
}

static INLINE void xy_y_convolve_8tap_16_avx2(const __m256i *const ss,
                                              const __m256i coeffs[4],
                                              __m256i r[2]) {
  r[0] = convolve16_8tap_avx2(ss, coeffs);
  r[1] = convolve16_8tap_avx2(ss + 4, coeffs);
}

static INLINE void xy_y_convolve_8tap_8x2_avx2(const int16_t *const src,
                                               __m256i ss_256[8],
                                               const __m256i coeffs[4],
                                               __m256i r[2]) {
  __m256i s_256[2];
  s_256[0] = _mm256_loadu_si256((__m256i *)(src + 6 * 8));
  s_256[1] = _mm256_loadu_si256((__m256i *)(src + 7 * 8));
  ss_256[3] = _mm256_unpacklo_epi16(s_256[0], s_256[1]);
  ss_256[7] = _mm256_unpackhi_epi16(s_256[0], s_256[1]);
  xy_y_convolve_8tap_16_avx2(ss_256, coeffs, r);
  ss_256[0] = ss_256[1];
  ss_256[1] = ss_256[2];
  ss_256[2] = ss_256[3];
  ss_256[4] = ss_256[5];
  ss_256[5] = ss_256[6];
  ss_256[6] = ss_256[7];
}

static INLINE void xy_y_convolve_8tap_8x2_half_pel_avx2(
    const int16_t *const src, const __m256i coeffs[2], __m256i s_256[8],
    __m256i r[2]) {
  __m256i a_256[4], ss_256[4];

  s_256[6] = _mm256_loadu_si256((__m256i *)(src + 6 * 8));
  s_256[7] = _mm256_loadu_si256((__m256i *)(src + 7 * 8));
  a_256[0] = _mm256_add_epi16(s_256[0], s_256[7]);
  a_256[1] = _mm256_add_epi16(s_256[1], s_256[6]);
  a_256[2] = _mm256_add_epi16(s_256[2], s_256[5]);
  a_256[3] = _mm256_add_epi16(s_256[3], s_256[4]);
  ss_256[0] = _mm256_unpacklo_epi16(a_256[0], a_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(a_256[2], a_256[3]);
  ss_256[2] = _mm256_unpackhi_epi16(a_256[0], a_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(a_256[2], a_256[3]);
  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r);
  s_256[0] = s_256[2];
  s_256[1] = s_256[3];
  s_256[2] = s_256[4];
  s_256[3] = s_256[5];
  s_256[4] = s_256[6];
  s_256[5] = s_256[7];
}

static AOM_FORCE_INLINE void xy_y_convolve_8tap_16x2_avx2(
    const int16_t *const src, const ptrdiff_t stride, const __m256i coeffs[4],
    __m256i s_256[8], __m256i ss_256[8], __m256i tt_256[8], __m256i r[4]) {
  s_256[7] = _mm256_loadu_si256((__m256i *)(src + 7 * stride));
  ss_256[3] = _mm256_unpacklo_epi16(s_256[6], s_256[7]);
  ss_256[7] = _mm256_unpackhi_epi16(s_256[6], s_256[7]);
  s_256[6] = _mm256_loadu_si256((__m256i *)(src + 8 * stride));
  tt_256[3] = _mm256_unpacklo_epi16(s_256[7], s_256[6]);
  tt_256[7] = _mm256_unpackhi_epi16(s_256[7], s_256[6]);

  xy_y_convolve_8tap_16_avx2(ss_256, coeffs, r + 0);
  xy_y_convolve_8tap_16_avx2(tt_256, coeffs, r + 2);

  ss_256[0] = ss_256[1];
  ss_256[1] = ss_256[2];
  ss_256[2] = ss_256[3];
  ss_256[4] = ss_256[5];
  ss_256[5] = ss_256[6];
  ss_256[6] = ss_256[7];

  tt_256[0] = tt_256[1];
  tt_256[1] = tt_256[2];
  tt_256[2] = tt_256[3];
  tt_256[4] = tt_256[5];
  tt_256[5] = tt_256[6];
  tt_256[6] = tt_256[7];
}

static INLINE void xy_y_convolve_8tap_16x2_half_pel_avx2(
    const int16_t *const src, const ptrdiff_t stride, const __m256i coeffs[4],
    __m256i s_256[8], __m256i r[4]) {
  __m256i a_256[4], ss_256[4];
  s_256[7] = _mm256_loadu_si256((__m256i *)(src + 7 * stride));

  a_256[0] = _mm256_add_epi16(s_256[0], s_256[7]);
  a_256[1] = _mm256_add_epi16(s_256[1], s_256[6]);
  a_256[2] = _mm256_add_epi16(s_256[2], s_256[5]);
  a_256[3] = _mm256_add_epi16(s_256[3], s_256[4]);
  ss_256[0] = _mm256_unpacklo_epi16(a_256[0], a_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(a_256[2], a_256[3]);
  ss_256[2] = _mm256_unpackhi_epi16(a_256[0], a_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(a_256[2], a_256[3]);

  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r + 0);

  a_256[1] = _mm256_add_epi16(s_256[2], s_256[7]);
  a_256[2] = _mm256_add_epi16(s_256[3], s_256[6]);
  a_256[3] = _mm256_add_epi16(s_256[4], s_256[5]);
  s_256[0] = s_256[2];
  s_256[2] = s_256[4];
  s_256[4] = s_256[6];
  s_256[6] = _mm256_loadu_si256((__m256i *)(src + 8 * stride));

  a_256[0] = _mm256_add_epi16(s_256[1], s_256[6]);
  s_256[1] = s_256[3];
  s_256[3] = s_256[5];
  s_256[5] = s_256[7];
  ss_256[0] = _mm256_unpacklo_epi16(a_256[0], a_256[1]);
  ss_256[1] = _mm256_unpacklo_epi16(a_256[2], a_256[3]);
  ss_256[2] = _mm256_unpackhi_epi16(a_256[0], a_256[1]);
  ss_256[3] = _mm256_unpackhi_epi16(a_256[2], a_256[3]);

  xy_y_convolve_4tap_16_avx2(ss_256, coeffs, r + 2);
}

static INLINE void xy_y_round_store_8x2_avx2(const __m256i res[2],
                                             uint8_t *const dst,
                                             const ptrdiff_t stride) {
  const __m256i r = xy_y_round_16_avx2(res);
  pack_store_8x2_avx2(r, dst, stride);
}

static INLINE void xy_y_round_store_16x2_avx2(const __m256i res[4],
                                              uint8_t *const dst,
                                              const ptrdiff_t stride) {
  const __m256i r0 = xy_y_round_16_avx2(res + 0);
  const __m256i r1 = xy_y_round_16_avx2(res + 2);
  xy_y_pack_store_16x2_avx2(r0, r1, dst, stride);
}

#endif  // AOM_AOM_DSP_X86_CONVOLVE_AVX2_H_
