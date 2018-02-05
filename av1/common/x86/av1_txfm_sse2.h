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
#ifndef AV1_COMMON_X86_AV1_TXFM_SSE2_H_
#define AV1_COMMON_X86_AV1_TXFM_SSE2_H_

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"
#include "aom/aom_integer.h"
#include "av1/common/av1_txfm.h"
#include "av1/common/x86/transpose_sse2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define pair_set_epi16(a, b)                                            \
  _mm_set_epi16((int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a), \
                (int16_t)(b), (int16_t)(a), (int16_t)(b), (int16_t)(a))

#define btf_16_sse2(w0, w1, in0, in1, out0, out1) \
  {                                               \
    __m128i t0 = _mm_unpacklo_epi16(in0, in1);    \
    __m128i t1 = _mm_unpackhi_epi16(in0, in1);    \
    __m128i u0 = _mm_madd_epi16(t0, w0);          \
    __m128i u1 = _mm_madd_epi16(t1, w0);          \
    __m128i v0 = _mm_madd_epi16(t0, w1);          \
    __m128i v1 = _mm_madd_epi16(t1, w1);          \
                                                  \
    __m128i a0 = _mm_add_epi32(u0, __rounding);   \
    __m128i a1 = _mm_add_epi32(u1, __rounding);   \
    __m128i b0 = _mm_add_epi32(v0, __rounding);   \
    __m128i b1 = _mm_add_epi32(v1, __rounding);   \
                                                  \
    __m128i c0 = _mm_srai_epi32(a0, cos_bit);     \
    __m128i c1 = _mm_srai_epi32(a1, cos_bit);     \
    __m128i d0 = _mm_srai_epi32(b0, cos_bit);     \
    __m128i d1 = _mm_srai_epi32(b1, cos_bit);     \
                                                  \
    out0 = _mm_packs_epi32(c0, c1);               \
    out1 = _mm_packs_epi32(d0, d1);               \
  }

static INLINE __m128i load_16bit_to_16bit(const int16_t *a) {
  return _mm_load_si128((const __m128i *)a);
}

static INLINE __m128i load_32bit_to_16bit(const int32_t *a) {
  const __m128i a_low = _mm_load_si128((const __m128i *)a);
  return _mm_packs_epi32(a_low, *(const __m128i *)(a + 4));
}

// Store 8 16 bit values. If the destination is 32 bits then sign extend the
// values by multiplying by 1.
static INLINE void store_16bit_to_32bit(__m128i a, int32_t *b) {
  const __m128i one = _mm_set1_epi16(1);
  const __m128i a_hi = _mm_mulhi_epi16(a, one);
  const __m128i a_lo = _mm_mullo_epi16(a, one);
  const __m128i a_1 = _mm_unpacklo_epi16(a_lo, a_hi);
  const __m128i a_2 = _mm_unpackhi_epi16(a_lo, a_hi);
  _mm_store_si128((__m128i *)(b), a_1);
  _mm_store_si128((__m128i *)(b + 4), a_2);
}

static INLINE void load_buffer_16bit_to_16bit_8x8(const int16_t *in,
                                                  __m128i *out) {
  for (int i = 0; i < 8; ++i) {
    out[i] = load_16bit_to_16bit(in + i * 8);
  }
}

static INLINE void load_buffer_32bit_to_16bit_8x8(const int32_t *in,
                                                  __m128i *out) {
  for (int i = 0; i < 8; ++i) {
    out[i] = load_32bit_to_16bit(in + i * 8);
  }
}

static INLINE void store_buffer_16bit_to_32bit_8x8(const __m128i *in,
                                                   int32_t *out) {
  for (int i = 0; i < 8; ++i) {
    store_16bit_to_32bit(in[i], out + i * 8);
  }
}

static INLINE void store_buffer_16bit_to_16bit_8x8(const __m128i *in,
                                                   int16_t *out) {
  for (int i = 0; i < 8; ++i) {
    _mm_store_si128((__m128i *)(out + i * 8), in[i]);
  }
}

static INLINE void round_shift_16bit_8x8(__m128i *in, int bit) {
  if (bit < 0) {
    bit = -bit;
    __m128i rounding = _mm_set1_epi16(1 << (bit - 1));
    for (int i = 0; i < 8; ++i) {
      in[i] = _mm_adds_epi16(in[i], rounding);
      in[i] = _mm_srai_epi16(in[i], bit);
    }
  } else if (bit > 0) {
    for (int i = 0; i < 8; ++i) {
      in[i] = _mm_slli_epi16(in[i], bit);
    }
  }
}

void av1_fwd_txfm2d_8x8_sse2(const int16_t *input, int32_t *output, int stride,
                             TX_TYPE tx_type, int bd);
void av1_fwd_txfm2d_8x8_c(const int16_t *input, int32_t *output, int stride,
                          TX_TYPE tx_type, int bd);

#ifdef __cplusplus
}
#endif  // __cplusplus
#endif  // AV1_COMMON_X86_AV1_TXFM_SSE2_H_
