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

#include <emmintrin.h>  // SSE2

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "av1/common/blockd.h"

static AOM_FORCE_INLINE void diffwtd_mask_highbd(
    uint8_t *mask, int which_inverse, int mask_base, const uint16_t *src0,
    int src0_stride, const uint16_t *src1, int src1_stride, int h, int w,
    const unsigned int bd) {
  assert(bd >= 8);
  assert(w >= 8 && (w % 8) == 0);
  const __m128i x0 = _mm_setzero_si128();
  const __m128i xAOM_BLEND_A64_MAX_ALPHA =
      _mm_set1_epi16(AOM_BLEND_A64_MAX_ALPHA);
  const __m128i xmask_base = _mm_set1_epi16(mask_base);
  if (bd == 8) {
    if (which_inverse) {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 8) {
          __m128i s0 = _mm_loadu_si128((const __m128i *)&src0[j]);
          __m128i s1 = _mm_loadu_si128((const __m128i *)&src1[j]);
          __m128i diff = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(s0, s1)),
                                        DIFF_FACTOR_LOG2);
          __m128i m =
              _mm_min_epi16(_mm_max_epi16(x0, _mm_add_epi16(diff, xmask_base)),
                            xAOM_BLEND_A64_MAX_ALPHA);
          m = _mm_sub_epi16(xAOM_BLEND_A64_MAX_ALPHA, m);
          m = _mm_packus_epi16(m, m);
          _mm_storel_epi64((__m128i *)&mask[j], m);
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    } else {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 8) {
          __m128i s0 = _mm_loadu_si128((const __m128i *)&src0[j]);
          __m128i s1 = _mm_loadu_si128((const __m128i *)&src1[j]);
          __m128i diff = _mm_srai_epi16(_mm_abs_epi16(_mm_sub_epi16(s0, s1)),
                                        DIFF_FACTOR_LOG2);
          __m128i m =
              _mm_min_epi16(_mm_max_epi16(x0, _mm_add_epi16(diff, xmask_base)),
                            xAOM_BLEND_A64_MAX_ALPHA);
          m = _mm_packus_epi16(m, m);
          _mm_storel_epi64((__m128i *)&mask[j], m);
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    }
  } else {
    const __m128i xshift = _mm_set1_epi64x(bd - 8 + DIFF_FACTOR_LOG2);
    if (which_inverse) {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 8) {
          __m128i s0 = _mm_loadu_si128((const __m128i *)&src0[j]);
          __m128i s1 = _mm_loadu_si128((const __m128i *)&src1[j]);
          __m128i diff =
              _mm_sra_epi16(_mm_abs_epi16(_mm_sub_epi16(s0, s1)), xshift);
          __m128i m =
              _mm_min_epi16(_mm_max_epi16(x0, _mm_add_epi16(diff, xmask_base)),
                            xAOM_BLEND_A64_MAX_ALPHA);
          m = _mm_sub_epi16(xAOM_BLEND_A64_MAX_ALPHA, m);
          m = _mm_packus_epi16(m, m);
          _mm_storel_epi64((__m128i *)&mask[j], m);
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    } else {
      for (int i = 0; i < h; ++i) {
        for (int j = 0; j < w; j += 8) {
          __m128i s0 = _mm_loadu_si128((const __m128i *)&src0[j]);
          __m128i s1 = _mm_loadu_si128((const __m128i *)&src1[j]);
          __m128i diff =
              _mm_sra_epi16(_mm_abs_epi16(_mm_sub_epi16(s0, s1)), xshift);
          __m128i m =
              _mm_min_epi16(_mm_max_epi16(x0, _mm_add_epi16(diff, xmask_base)),
                            xAOM_BLEND_A64_MAX_ALPHA);
          m = _mm_packus_epi16(m, m);
          _mm_storel_epi64((__m128i *)&mask[j], m);
        }
        src0 += src0_stride;
        src1 += src1_stride;
        mask += w;
      }
    }
  }
}

void av1_build_compound_diffwtd_mask_highbd_sse2(
    uint8_t *mask, DIFFWTD_MASK_TYPE mask_type, const uint8_t *src0,
    int src0_stride, const uint8_t *src1, int src1_stride, int h, int w,
    int bd) {
  if (w < 8) {
    av1_build_compound_diffwtd_mask_highbd_c(mask, mask_type, src0, src0_stride,
                                             src1, src1_stride, h, w, bd);
  } else {
    switch (mask_type) {
      case DIFFWTD_38:
        diffwtd_mask_highbd(mask, 0, 38, CONVERT_TO_SHORTPTR(src0), src0_stride,
                            CONVERT_TO_SHORTPTR(src1), src1_stride, h, w, bd);
        break;
      case DIFFWTD_38_INV:
        diffwtd_mask_highbd(mask, 1, 38, CONVERT_TO_SHORTPTR(src0), src0_stride,
                            CONVERT_TO_SHORTPTR(src1), src1_stride, h, w, bd);
        break;
      default: assert(0);
    }
  }
}
