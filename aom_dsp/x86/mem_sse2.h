/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AOM_DSP_X86_MEM_SSE2_H_
#define AOM_DSP_X86_MEM_SSE2_H_

#include <emmintrin.h>  // SSE2

#include "./aom_config.h"
#include "aom/aom_integer.h"

static INLINE __m128i loadh_epi64(const void *const src, const __m128i s) {
  return _mm_castps_si128(
      _mm_loadh_pi(_mm_castsi128_ps(s), (const __m64 *)src));
}

static INLINE __m128i load_8bit_4x2_to_1_sse2(const uint8_t *const src,
                                              const ptrdiff_t stride) {
  const __m128i s0 = _mm_cvtsi32_si128(*(const int *)(src + 0 * stride));
  const __m128i s1 = _mm_cvtsi32_si128(*(const int *)(src + 1 * stride));
  return _mm_unpacklo_epi32(s0, s1);
}

static INLINE void store_8bit_4x4_sse2(const __m128i *const src,
                                       uint8_t *const dst,
                                       const ptrdiff_t stride) {
  *(int *)(dst + 0 * stride) = _mm_cvtsi128_si32(src[0]);
  *(int *)(dst + 1 * stride) = _mm_cvtsi128_si32(src[1]);
  *(int *)(dst + 2 * stride) = _mm_cvtsi128_si32(src[2]);
  *(int *)(dst + 3 * stride) = _mm_cvtsi128_si32(src[3]);
}

static INLINE void store_8bit_4x4_from_1_reg_sse2(const __m128i src,
                                                  uint8_t *const dst,
                                                  const ptrdiff_t stride) {
  __m128i s[4];

  s[0] = src;
  s[1] = _mm_srli_si128(src, 4);
  s[2] = _mm_srli_si128(src, 8);
  s[3] = _mm_srli_si128(src, 12);
  store_8bit_4x4_sse2(s, dst, stride);
}

#endif  // AOM_DSP_X86_MEM_SSE2_H_
