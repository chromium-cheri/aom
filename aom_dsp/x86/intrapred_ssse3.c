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

#include <tmmintrin.h>

#include "config/aom_dsp_rtcd.h"

#include "aom_dsp/intrapred_common.h"

// -----------------------------------------------------------------------------
// PAETH_PRED

// Return 8 16-bit pixels in one row
static INLINE __m128i paeth_8x1_pred(const __m128i *left, const __m128i *top,
                                     const __m128i *topleft) {
  const __m128i base = _mm_sub_epi16(_mm_add_epi16(*top, *left), *topleft);

  __m128i pl = _mm_abs_epi16(_mm_sub_epi16(base, *left));
  __m128i pt = _mm_abs_epi16(_mm_sub_epi16(base, *top));
  __m128i ptl = _mm_abs_epi16(_mm_sub_epi16(base, *topleft));

  __m128i mask1 = _mm_cmpgt_epi16(pl, pt);
  mask1 = _mm_or_si128(mask1, _mm_cmpgt_epi16(pl, ptl));
  __m128i mask2 = _mm_cmpgt_epi16(pt, ptl);

  pl = _mm_andnot_si128(mask1, *left);

  ptl = _mm_and_si128(mask2, *topleft);
  pt = _mm_andnot_si128(mask2, *top);
  pt = _mm_or_si128(pt, ptl);
  pt = _mm_and_si128(mask1, pt);

  return _mm_or_si128(pl, pt);
}

void aom_paeth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(int *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(int *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_cvtsi32_si128(((const int *)above)[0]);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    *(int *)dst = _mm_cvtsi128_si32(_mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                   const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

    _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i t = _mm_loadl_epi64((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i t16 = _mm_unpacklo_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 2; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i row = paeth_8x1_pred(&l16, &t16, &tl16);

      _mm_storel_epi64((__m128i *)dst, _mm_packus_epi16(row, row));
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

// Return 16 8-bit pixels in one row
static INLINE __m128i paeth_16x1_pred(const __m128i *left, const __m128i *top0,
                                      const __m128i *top1,
                                      const __m128i *topleft) {
  const __m128i p0 = paeth_8x1_pred(left, top0, topleft);
  const __m128i p1 = paeth_8x1_pred(left, top1, topleft);
  return _mm_packus_epi16(p0, p1);
}

void aom_paeth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_cvtsi32_si128(((const int *)left)[0]);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  for (int i = 0; i < 4; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i l = _mm_loadl_epi64((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 8; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);

  int i;
  for (i = 0; i < 16; ++i) {
    const __m128i l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i l = _mm_load_si128((const __m128i *)left);
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  l = _mm_load_si128((const __m128i *)(left + 16));
  rep = _mm_set1_epi16((short)0x8000);
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);

    _mm_store_si128((__m128i *)dst, row);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i t = _mm_load_si128((const __m128i *)above);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top0 = _mm_unpacklo_epi8(t, zero);
  const __m128i top1 = _mm_unpackhi_epi8(t, zero);
  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);

  for (int j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (int i = 0; i < 16; ++i) {
      const __m128i l16 = _mm_shuffle_epi8(l, rep);
      const __m128i row = paeth_16x1_pred(&l16, &top0, &top1, &tl16);
      _mm_store_si128((__m128i *)dst, row);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  const __m128i l = _mm_loadl_epi64((const __m128i *)left);
  __m128i l16;

  for (int i = 0; i < 8; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i l16;

  int i;
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }

  rep = _mm_set1_epi16((short)0x8000);
  l = _mm_load_si128((const __m128i *)(left + 16));
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

    _mm_store_si128((__m128i *)dst, r32l);
    _mm_store_si128((__m128i *)(dst + 16), r32h);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

void aom_paeth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i, j;
  for (j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (i = 0; i < 16; ++i) {
      l16 = _mm_shuffle_epi8(l, rep);
      const __m128i r32l = paeth_16x1_pred(&l16, &al, &ah, &tl16);
      const __m128i r32h = paeth_16x1_pred(&l16, &bl, &bh, &tl16);

      _mm_store_si128((__m128i *)dst, r32l);
      _mm_store_si128((__m128i *)(dst + 16), r32h);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i, j;
  for (j = 0; j < 2; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (i = 0; i < 16; ++i) {
      l16 = _mm_shuffle_epi8(l, rep);
      const __m128i r0 = paeth_16x1_pred(&l16, &al, &ah, &tl16);
      const __m128i r1 = paeth_16x1_pred(&l16, &bl, &bh, &tl16);
      const __m128i r2 = paeth_16x1_pred(&l16, &cl, &ch, &tl16);
      const __m128i r3 = paeth_16x1_pred(&l16, &dl, &dh, &tl16);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i, j;
  for (j = 0; j < 4; ++j) {
    const __m128i l = _mm_load_si128((const __m128i *)(left + j * 16));
    __m128i rep = _mm_set1_epi16((short)0x8000);
    for (i = 0; i < 16; ++i) {
      l16 = _mm_shuffle_epi8(l, rep);
      const __m128i r0 = paeth_16x1_pred(&l16, &al, &ah, &tl16);
      const __m128i r1 = paeth_16x1_pred(&l16, &bl, &bh, &tl16);
      const __m128i r2 = paeth_16x1_pred(&l16, &cl, &ch, &tl16);
      const __m128i r3 = paeth_16x1_pred(&l16, &dl, &dh, &tl16);

      _mm_store_si128((__m128i *)dst, r0);
      _mm_store_si128((__m128i *)(dst + 16), r1);
      _mm_store_si128((__m128i *)(dst + 32), r2);
      _mm_store_si128((__m128i *)(dst + 48), r3);
      dst += stride;
      rep = _mm_add_epi16(rep, one);
    }
  }
}

void aom_paeth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  const __m128i a = _mm_load_si128((const __m128i *)above);
  const __m128i b = _mm_load_si128((const __m128i *)(above + 16));
  const __m128i c = _mm_load_si128((const __m128i *)(above + 32));
  const __m128i d = _mm_load_si128((const __m128i *)(above + 48));
  const __m128i zero = _mm_setzero_si128();
  const __m128i al = _mm_unpacklo_epi8(a, zero);
  const __m128i ah = _mm_unpackhi_epi8(a, zero);
  const __m128i bl = _mm_unpacklo_epi8(b, zero);
  const __m128i bh = _mm_unpackhi_epi8(b, zero);
  const __m128i cl = _mm_unpacklo_epi8(c, zero);
  const __m128i ch = _mm_unpackhi_epi8(c, zero);
  const __m128i dl = _mm_unpacklo_epi8(d, zero);
  const __m128i dh = _mm_unpackhi_epi8(d, zero);

  const __m128i tl16 = _mm_set1_epi16((int16_t)above[-1]);
  const __m128i one = _mm_set1_epi16(1);
  __m128i l16;

  int i;
  const __m128i l = _mm_load_si128((const __m128i *)left);
  __m128i rep = _mm_set1_epi16((short)0x8000);
  for (i = 0; i < 16; ++i) {
    l16 = _mm_shuffle_epi8(l, rep);
    const __m128i r0 = paeth_16x1_pred(&l16, &al, &ah, &tl16);
    const __m128i r1 = paeth_16x1_pred(&l16, &bl, &bh, &tl16);
    const __m128i r2 = paeth_16x1_pred(&l16, &cl, &ch, &tl16);
    const __m128i r3 = paeth_16x1_pred(&l16, &dl, &dh, &tl16);

    _mm_store_si128((__m128i *)dst, r0);
    _mm_store_si128((__m128i *)(dst + 16), r1);
    _mm_store_si128((__m128i *)(dst + 32), r2);
    _mm_store_si128((__m128i *)(dst + 48), r3);
    dst += stride;
    rep = _mm_add_epi16(rep, one);
  }
}

// -----------------------------------------------------------------------------
// SMOOTH_PRED

// pixels[0]: above and below_pred interleave vector
// pixels[1]: left vector
// pixels[2]: right_pred vector
static INLINE void load_pixel_w4(const uint8_t *above, const uint8_t *left,
                                 int height, __m128i *pixels) {
  __m128i d = _mm_cvtsi32_si128(((const int *)above)[0]);
  if (height == 4)
    pixels[1] = _mm_cvtsi32_si128(((const int *)left)[0]);
  else if (height == 8)
    pixels[1] = _mm_loadl_epi64(((const __m128i *)left));
  else
    pixels[1] = _mm_loadu_si128(((const __m128i *)left));

  pixels[2] = _mm_set1_epi16((int16_t)above[3]);

  const __m128i bp = _mm_set1_epi16((int16_t)left[height - 1]);
  const __m128i zero = _mm_setzero_si128();
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], second half for height = 16 only
// weight_h[3]: same as [1], second half for height = 16 only
// weight_w[0]: weights_w and scale - weights_w interleave vector
static INLINE void load_weight_w4(int height, __m128i *weight_h,
                                  __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i t = _mm_cvtsi32_si128(((const int *)smooth_weights)[0]);
  weight_h[0] = _mm_unpacklo_epi8(t, zero);
  weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
  weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);

  if (height == 8) {
    const __m128i weight = _mm_loadl_epi64((const __m128i *)&smooth_weights[4]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
  } else if (height == 16) {
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  }
}

static INLINE void smooth_pred_4xh(const __m128i *pixel, const __m128i *wh,
                                   const __m128i *ww, int h, uint8_t *dst,
                                   ptrdiff_t stride, int second_half) {
  const __m128i round = _mm_set1_epi32((1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i rep = second_half ? _mm_set1_epi16((short)0x8008)
                            : _mm_set1_epi16((short)0x8000);
  __m128i d = _mm_set1_epi16(0x100);

  for (int i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s = _mm_madd_epi16(pixel[0], wh_sc);

    __m128i b = _mm_shuffle_epi8(pixel[1], rep);
    b = _mm_unpacklo_epi16(b, pixel[2]);
    __m128i sum = _mm_madd_epi16(b, ww[0]);

    sum = _mm_add_epi32(s, sum);
    sum = _mm_add_epi32(sum, round);
    sum = _mm_srai_epi32(sum, 1 + SMOOTH_WEIGHT_LOG2_SCALE);

    sum = _mm_shuffle_epi8(sum, gat);
    *(int *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 4, pixels);

  __m128i wh[4], ww[2];
  load_weight_w4(4, wh, ww);

  smooth_pred_4xh(pixels, wh, ww, 4, dst, stride, 0);
}

void aom_smooth_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 8, pixels);

  __m128i wh[4], ww[2];
  load_weight_w4(8, wh, ww);

  smooth_pred_4xh(pixels, wh, ww, 8, dst, stride, 0);
}

void aom_smooth_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[3];
  load_pixel_w4(above, left, 16, pixels);

  __m128i wh[4], ww[2];
  load_weight_w4(16, wh, ww);

  smooth_pred_4xh(pixels, wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_4xh(pixels, &wh[2], ww, 8, dst, stride, 1);
}

// pixels[0]: above and below_pred interleave vector, first half
// pixels[1]: above and below_pred interleave vector, second half
// pixels[2]: left vector
// pixels[3]: right_pred vector
// pixels[4]: above and below_pred interleave vector, first half
// pixels[5]: above and below_pred interleave vector, second half
// pixels[6]: left vector + 16
// pixels[7]: right_pred vector
static INLINE void load_pixel_w8(const uint8_t *above, const uint8_t *left,
                                 int height, __m128i *pixels) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i bp = _mm_set1_epi16((int16_t)left[height - 1]);
  __m128i d = _mm_loadl_epi64((const __m128i *)above);
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
  pixels[1] = _mm_unpackhi_epi16(d, bp);

  pixels[3] = _mm_set1_epi16((int16_t)above[7]);

  if (height == 4) {
    pixels[2] = _mm_cvtsi32_si128(((const int *)left)[0]);
  } else if (height == 8) {
    pixels[2] = _mm_loadl_epi64((const __m128i *)left);
  } else if (height == 16) {
    pixels[2] = _mm_load_si128((const __m128i *)left);
  } else {
    pixels[2] = _mm_load_si128((const __m128i *)left);
    pixels[4] = pixels[0];
    pixels[5] = pixels[1];
    pixels[6] = _mm_load_si128((const __m128i *)(left + 16));
    pixels[7] = pixels[3];
  }
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], offset 8
// weight_h[3]: same as [1], offset 8
// weight_h[4]: same as [0], offset 16
// weight_h[5]: same as [1], offset 16
// weight_h[6]: same as [0], offset 24
// weight_h[7]: same as [1], offset 24
// weight_w[0]: weights_w and scale - weights_w interleave vector, first half
// weight_w[1]: weights_w and scale - weights_w interleave vector, second half
static INLINE void load_weight_w8(int height, __m128i *weight_h,
                                  __m128i *weight_w) {
  const __m128i zero = _mm_setzero_si128();
  const int we_offset = height < 8 ? 0 : 4;
  __m128i we = _mm_loadu_si128((const __m128i *)&smooth_weights[we_offset]);
  weight_h[0] = _mm_unpacklo_epi8(we, zero);
  const __m128i d = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  weight_h[1] = _mm_sub_epi16(d, weight_h[0]);

  if (height == 4) {
    we = _mm_srli_si128(we, 4);
    __m128i tmp1 = _mm_unpacklo_epi8(we, zero);
    __m128i tmp2 = _mm_sub_epi16(d, tmp1);
    weight_w[0] = _mm_unpacklo_epi16(tmp1, tmp2);
    weight_w[1] = _mm_unpackhi_epi16(tmp1, tmp2);
  } else {
    weight_w[0] = _mm_unpacklo_epi16(weight_h[0], weight_h[1]);
    weight_w[1] = _mm_unpackhi_epi16(weight_h[0], weight_h[1]);
  }

  if (height == 16) {
    we = _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weight_h[0] = _mm_unpacklo_epi8(we, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(we, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  } else if (height == 32) {
    const __m128i weight_lo =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28]);
    weight_h[0] = _mm_unpacklo_epi8(weight_lo, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight_lo, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
    const __m128i weight_hi =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28 + 16]);
    weight_h[4] = _mm_unpacklo_epi8(weight_hi, zero);
    weight_h[5] = _mm_sub_epi16(d, weight_h[4]);
    weight_h[6] = _mm_unpackhi_epi8(weight_hi, zero);
    weight_h[7] = _mm_sub_epi16(d, weight_h[6]);
  }
}

static INLINE void smooth_pred_8xh(const __m128i *pixels, const __m128i *wh,
                                   const __m128i *ww, int h, uint8_t *dst,
                                   ptrdiff_t stride, int second_half) {
  const __m128i round = _mm_set1_epi32((1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);

  __m128i rep = second_half ? _mm_set1_epi16((short)0x8008)
                            : _mm_set1_epi16((short)0x8000);
  __m128i d = _mm_set1_epi16(0x100);

  int i;
  for (i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s0 = _mm_madd_epi16(pixels[0], wh_sc);
    __m128i s1 = _mm_madd_epi16(pixels[1], wh_sc);

    __m128i b = _mm_shuffle_epi8(pixels[2], rep);
    b = _mm_unpacklo_epi16(b, pixels[3]);
    __m128i sum0 = _mm_madd_epi16(b, ww[0]);
    __m128i sum1 = _mm_madd_epi16(b, ww[1]);

    s0 = _mm_add_epi32(s0, sum0);
    s0 = _mm_add_epi32(s0, round);
    s0 = _mm_srai_epi32(s0, 1 + SMOOTH_WEIGHT_LOG2_SCALE);

    s1 = _mm_add_epi32(s1, sum1);
    s1 = _mm_add_epi32(s1, round);
    s1 = _mm_srai_epi32(s1, 1 + SMOOTH_WEIGHT_LOG2_SCALE);

    sum0 = _mm_packus_epi16(s0, s1);
    sum0 = _mm_shuffle_epi8(sum0, gat);
    _mm_storel_epi64((__m128i *)dst, sum0);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 4, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(4, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 4, dst, stride, 0);
}

void aom_smooth_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                    const uint8_t *above, const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 8, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(8, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 8, dst, stride, 0);
}

void aom_smooth_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_w8(above, left, 16, pixels);

  __m128i wh[4], ww[2];
  load_weight_w8(16, wh, ww);

  smooth_pred_8xh(pixels, wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(pixels, &wh[2], ww, 8, dst, stride, 1);
}

void aom_smooth_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  __m128i pixels[8];
  load_pixel_w8(above, left, 32, pixels);

  __m128i wh[8], ww[2];
  load_weight_w8(32, wh, ww);

  smooth_pred_8xh(&pixels[0], wh, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(&pixels[0], &wh[2], ww, 8, dst, stride, 1);
  dst += stride << 3;
  smooth_pred_8xh(&pixels[4], &wh[4], ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_pred_8xh(&pixels[4], &wh[6], ww, 8, dst, stride, 1);
}

static INLINE void smooth_predictor_wxh(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left, uint32_t bw,
                                        uint32_t bh) {
  const uint8_t *const sm_weights_w = smooth_weights + bw - 4;
  const uint8_t *const sm_weights_h = smooth_weights + bh - 4;
  const __m128i zero = _mm_setzero_si128();
  const __m128i scale_value =
      _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i bottom_left = _mm_cvtsi32_si128(left[bh - 1]);
  const __m128i dup16 = _mm_set1_epi32(0x01000100);
  const __m128i top_right =
      _mm_shuffle_epi8(_mm_cvtsi32_si128(above[bw - 1]), dup16);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i round =
      _mm_set1_epi32((uint16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));

  for (uint32_t y = 0; y < bh; ++y) {
    const __m128i weights_y = _mm_cvtsi32_si128(sm_weights_h[y]);
    const __m128i left_y = _mm_cvtsi32_si128(left[y]);
    const __m128i scale_m_weights_y = _mm_sub_epi16(scale_value, weights_y);
    __m128i pred_scaled_bl = _mm_mullo_epi16(scale_m_weights_y, bottom_left);
    const __m128i wl_y =
        _mm_shuffle_epi32(_mm_unpacklo_epi16(weights_y, left_y), 0);
    pred_scaled_bl = _mm_add_epi32(pred_scaled_bl, round);
    pred_scaled_bl = _mm_shuffle_epi32(pred_scaled_bl, 0);

    for (uint32_t x = 0; x < bw; x += 8) {
      const __m128i top_x = _mm_loadl_epi64((const __m128i *)(above + x));
      const __m128i weights_x =
          _mm_loadl_epi64((const __m128i *)(sm_weights_w + x));
      const __m128i tw_x = _mm_unpacklo_epi8(top_x, weights_x);
      const __m128i tw_x_lo = _mm_unpacklo_epi8(tw_x, zero);
      const __m128i tw_x_hi = _mm_unpackhi_epi8(tw_x, zero);

      __m128i pred_lo = _mm_madd_epi16(tw_x_lo, wl_y);
      __m128i pred_hi = _mm_madd_epi16(tw_x_hi, wl_y);

      const __m128i scale_m_weights_x =
          _mm_sub_epi16(scale_value, _mm_unpacklo_epi8(weights_x, zero));
      const __m128i swxtr = _mm_mullo_epi16(scale_m_weights_x, top_right);
      const __m128i swxtr_lo = _mm_unpacklo_epi16(swxtr, zero);
      const __m128i swxtr_hi = _mm_unpackhi_epi16(swxtr, zero);

      pred_lo = _mm_add_epi32(pred_lo, pred_scaled_bl);
      pred_hi = _mm_add_epi32(pred_hi, pred_scaled_bl);

      pred_lo = _mm_add_epi32(pred_lo, swxtr_lo);
      pred_hi = _mm_add_epi32(pred_hi, swxtr_hi);

      pred_lo = _mm_srai_epi32(pred_lo, (1 + SMOOTH_WEIGHT_LOG2_SCALE));
      pred_hi = _mm_srai_epi32(pred_hi, (1 + SMOOTH_WEIGHT_LOG2_SCALE));

      __m128i pred = _mm_packus_epi16(pred_lo, pred_hi);
      pred = _mm_shuffle_epi8(pred, gat);
      _mm_storel_epi64((__m128i *)(dst + x), pred);
    }
    dst += stride;
  }
}

void aom_smooth_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 4);
}

void aom_smooth_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 8);
}

void aom_smooth_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 16);
}

void aom_smooth_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 32);
}

void aom_smooth_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                     const uint8_t *above,
                                     const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 8);
}

void aom_smooth_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 16);
}

void aom_smooth_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 32);
}

void aom_smooth_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 32, 64);
}

void aom_smooth_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 64, 64);
}

void aom_smooth_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 64, 32);
}

void aom_smooth_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 64, 16);
}

void aom_smooth_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  smooth_predictor_wxh(dst, stride, above, left, 16, 64);
}

// -----------------------------------------------------------------------------
// SMOOTH_V_PRED

// pixels[0]: above and below_pred interleave vector
static INLINE void load_pixel_v_w4(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  const __m128i zero = _mm_setzero_si128();
  __m128i d = _mm_cvtsi32_si128(((const int *)above)[0]);
  const __m128i bp = _mm_set1_epi16((int16_t)left[height - 1]);
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
}

// weights[0]: weights_h vector
// weights[1]: scale - weights_h vector
static INLINE void load_weight_v_w4(int height, __m128i *weights) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));

  if (height == 4) {
    const __m128i weight = _mm_cvtsi32_si128(((const int *)smooth_weights)[0]);
    weights[0] = _mm_unpacklo_epi8(weight, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
  } else if (height == 8) {
    const __m128i weight = _mm_loadl_epi64((const __m128i *)&smooth_weights[4]);
    weights[0] = _mm_unpacklo_epi8(weight, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
  } else {
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weights[0] = _mm_unpacklo_epi8(weight, zero);
    weights[1] = _mm_sub_epi16(d, weights[0]);
    weights[2] = _mm_unpackhi_epi8(weight, zero);
    weights[3] = _mm_sub_epi16(d, weights[2]);
  }
}

static INLINE void smooth_v_pred_4xh(const __m128i *pixel,
                                     const __m128i *weight, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i d = _mm_set1_epi16(0x100);

  for (int i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(weight[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(weight[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i sum = _mm_madd_epi16(pixel[0], wh_sc);
    sum = _mm_add_epi32(sum, pred_round);
    sum = _mm_srai_epi32(sum, SMOOTH_WEIGHT_LOG2_SCALE);
    sum = _mm_shuffle_epi8(sum, gat);
    *(int *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;
    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_v_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels;
  load_pixel_v_w4(above, left, 4, &pixels);

  __m128i weights[2];
  load_weight_v_w4(4, weights);

  smooth_v_pred_4xh(&pixels, weights, 4, dst, stride);
}

void aom_smooth_v_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels;
  load_pixel_v_w4(above, left, 8, &pixels);

  __m128i weights[2];
  load_weight_v_w4(8, weights);

  smooth_v_pred_4xh(&pixels, weights, 8, dst, stride);
}

void aom_smooth_v_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels;
  load_pixel_v_w4(above, left, 16, &pixels);

  __m128i weights[4];
  load_weight_v_w4(16, weights);

  smooth_v_pred_4xh(&pixels, weights, 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_4xh(&pixels, &weights[2], 8, dst, stride);
}

// pixels[0]: above and below_pred interleave vector, first half
// pixels[1]: above and below_pred interleave vector, second half
static INLINE void load_pixel_v_w8(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  const __m128i zero = _mm_setzero_si128();
  __m128i d = _mm_loadl_epi64((const __m128i *)above);
  const __m128i bp = _mm_set1_epi16((int16_t)left[height - 1]);
  d = _mm_unpacklo_epi8(d, zero);
  pixels[0] = _mm_unpacklo_epi16(d, bp);
  pixels[1] = _mm_unpackhi_epi16(d, bp);
}

// weight_h[0]: weight_h vector
// weight_h[1]: scale - weight_h vector
// weight_h[2]: same as [0], offset 8
// weight_h[3]: same as [1], offset 8
// weight_h[4]: same as [0], offset 16
// weight_h[5]: same as [1], offset 16
// weight_h[6]: same as [0], offset 24
// weight_h[7]: same as [1], offset 24
static INLINE void load_weight_v_w8(int height, __m128i *weight_h) {
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));

  if (height < 16) {
    const int offset = height < 8 ? 0 : 4;
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[offset]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
  } else if (height == 16) {
    const __m128i weight =
        _mm_loadu_si128((const __m128i *)&smooth_weights[12]);
    weight_h[0] = _mm_unpacklo_epi8(weight, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
  } else {
    const __m128i weight_lo =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28]);
    weight_h[0] = _mm_unpacklo_epi8(weight_lo, zero);
    weight_h[1] = _mm_sub_epi16(d, weight_h[0]);
    weight_h[2] = _mm_unpackhi_epi8(weight_lo, zero);
    weight_h[3] = _mm_sub_epi16(d, weight_h[2]);
    const __m128i weight_hi =
        _mm_loadu_si128((const __m128i *)&smooth_weights[28 + 16]);
    weight_h[4] = _mm_unpacklo_epi8(weight_hi, zero);
    weight_h[5] = _mm_sub_epi16(d, weight_h[4]);
    weight_h[6] = _mm_unpackhi_epi8(weight_hi, zero);
    weight_h[7] = _mm_sub_epi16(d, weight_h[6]);
  }
}

static INLINE void smooth_v_pred_8xh(const __m128i *pixels, const __m128i *wh,
                                     int h, uint8_t *dst, ptrdiff_t stride) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i inc = _mm_set1_epi16(0x202);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  __m128i d = _mm_set1_epi16(0x100);

  for (int i = 0; i < h; ++i) {
    const __m128i wg_wg = _mm_shuffle_epi8(wh[0], d);
    const __m128i sc_sc = _mm_shuffle_epi8(wh[1], d);
    const __m128i wh_sc = _mm_unpacklo_epi16(wg_wg, sc_sc);
    __m128i s0 = _mm_madd_epi16(pixels[0], wh_sc);
    __m128i s1 = _mm_madd_epi16(pixels[1], wh_sc);

    s0 = _mm_add_epi32(s0, pred_round);
    s0 = _mm_srai_epi32(s0, SMOOTH_WEIGHT_LOG2_SCALE);

    s1 = _mm_add_epi32(s1, pred_round);
    s1 = _mm_srai_epi32(s1, SMOOTH_WEIGHT_LOG2_SCALE);

    __m128i sum01 = _mm_packus_epi16(s0, s1);
    sum01 = _mm_shuffle_epi8(sum01, gat);
    _mm_storel_epi64((__m128i *)dst, sum01);
    dst += stride;

    d = _mm_add_epi16(d, inc);
  }
}

void aom_smooth_v_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 4, pixels);

  __m128i wh[2];
  load_weight_v_w8(4, wh);

  smooth_v_pred_8xh(pixels, wh, 4, dst, stride);
}

void aom_smooth_v_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 8, pixels);

  __m128i wh[2];
  load_weight_v_w8(8, wh);

  smooth_v_pred_8xh(pixels, wh, 8, dst, stride);
}

void aom_smooth_v_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 16, pixels);

  __m128i wh[4];
  load_weight_v_w8(16, wh);

  smooth_v_pred_8xh(pixels, wh, 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[2], 8, dst, stride);
}

void aom_smooth_v_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_v_w8(above, left, 32, pixels);

  __m128i wh[8];
  load_weight_v_w8(32, wh);

  smooth_v_pred_8xh(pixels, &wh[0], 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[2], 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[4], 8, dst, stride);
  dst += stride << 3;
  smooth_v_pred_8xh(pixels, &wh[6], 8, dst, stride);
}

static INLINE void smooth_v_predictor_wxh(uint8_t *dst, ptrdiff_t stride,
                                          const uint8_t *above,
                                          const uint8_t *left, uint32_t bw,
                                          uint32_t bh) {
  const uint8_t *const sm_weights_h = smooth_weights + bh - 4;
  const __m128i zero = _mm_setzero_si128();
  const __m128i scale_value =
      _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i dup16 = _mm_set1_epi32(0x01000100);
  const __m128i bottom_left =
      _mm_shuffle_epi8(_mm_cvtsi32_si128(left[bh - 1]), dup16);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  const __m128i round =
      _mm_set1_epi32((uint16_t)(1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));

  for (uint32_t y = 0; y < bh; ++y) {
    const __m128i weights_y = _mm_cvtsi32_si128(sm_weights_h[y]);
    const __m128i scale_m_weights_y =
        _mm_shuffle_epi8(_mm_sub_epi16(scale_value, weights_y), dup16);
    const __m128i wl_y =
        _mm_shuffle_epi32(_mm_unpacklo_epi16(weights_y, bottom_left), 0);

    for (uint32_t x = 0; x < bw; x += 8) {
      const __m128i top_x = _mm_loadl_epi64((const __m128i *)(above + x));
      // 8 -> 16
      const __m128i tw_x = _mm_unpacklo_epi8(top_x, zero);
      const __m128i tw_x_lo = _mm_unpacklo_epi16(tw_x, scale_m_weights_y);
      const __m128i tw_x_hi = _mm_unpackhi_epi16(tw_x, scale_m_weights_y);
      // top_x * weights_y + scale_m_weights_y * bottom_left
      __m128i pred_lo = _mm_madd_epi16(tw_x_lo, wl_y);
      __m128i pred_hi = _mm_madd_epi16(tw_x_hi, wl_y);

      pred_lo = _mm_add_epi32(pred_lo, round);
      pred_hi = _mm_add_epi32(pred_hi, round);
      pred_lo = _mm_srai_epi32(pred_lo, SMOOTH_WEIGHT_LOG2_SCALE);
      pred_hi = _mm_srai_epi32(pred_hi, SMOOTH_WEIGHT_LOG2_SCALE);

      __m128i pred = _mm_packus_epi16(pred_lo, pred_hi);
      pred = _mm_shuffle_epi8(pred, gat);
      _mm_storel_epi64((__m128i *)(dst + x), pred);
    }
    dst += stride;
  }
}

// TODO(slavarnway): Visual Studio only supports restrict when /std:c11
// (available in 2019+) or greater is specified; __restrict can be used in that
// case. This should be moved to rtcd and used consistently between the
// function declarations and definitions to avoid warnings in Visual Studio
// when defining LIBAOM_RESTRICT to restrict or __restrict.
#if defined(_MSC_VER)
#define LIBAOM_RESTRICT
#else
#define LIBAOM_RESTRICT restrict
#endif

static AOM_FORCE_INLINE __m128i Load4(const void *src) {
  // With new compilers such as clang 8.0.0 we can use the new _mm_loadu_si32
  // intrinsic. Both _mm_loadu_si32(src) and the code here are compiled into a
  // movss instruction.
  //
  // Until compiler support of _mm_loadu_si32 is widespread, use of
  // _mm_loadu_si32 is banned.
  int val;
  memcpy(&val, src, sizeof(val));
  return _mm_cvtsi32_si128(val);
}

static AOM_FORCE_INLINE __m128i LoadLo8(const void *a) {
  return _mm_loadl_epi64((const __m128i *)(a));
}

static AOM_FORCE_INLINE __m128i LoadUnaligned16(const void *a) {
  return _mm_loadu_si128((const __m128i *)(a));
}

static AOM_FORCE_INLINE void StoreUnaligned16(void *a, const __m128i v) {
  _mm_storeu_si128((__m128i *)(a), v);
}

static AOM_FORCE_INLINE __m128i cvtepu8_epi16(__m128i x) {
  return _mm_unpacklo_epi8((x), _mm_setzero_si128());
}

// For Horizontal, pixels1 and pixels2 are the same repeated value. For
// Vertical, weights1 and weights2 are the same, and scaled_corner1 and
// scaled_corner2 are the same.
static AOM_FORCE_INLINE void write_smooth_directional_sum16(
    uint8_t *LIBAOM_RESTRICT dest, const __m128i pixels1, const __m128i pixels2,
    const __m128i weights1, const __m128i weights2,
    const __m128i scaled_corner1, const __m128i scaled_corner2,
    const __m128i round) {
  const __m128i weighted_px1 = _mm_mullo_epi16(pixels1, weights1);
  const __m128i weighted_px2 = _mm_mullo_epi16(pixels2, weights2);
  const __m128i pred_sum1 = _mm_add_epi16(scaled_corner1, weighted_px1);
  const __m128i pred_sum2 = _mm_add_epi16(scaled_corner2, weighted_px2);
  // Equivalent to RightShiftWithRounding(pred[x][y], 8).
  const __m128i pred1 = _mm_srli_epi16(_mm_add_epi16(pred_sum1, round), 8);
  const __m128i pred2 = _mm_srli_epi16(_mm_add_epi16(pred_sum2, round), 8);
  StoreUnaligned16(dest, _mm_packus_epi16(pred1, pred2));
}

#if 0
void aom_smooth_v_predictor_16x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 4);
}

void aom_smooth_v_predictor_16x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 8);
}

void aom_smooth_v_predictor_16x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 16);
}

void aom_smooth_v_predictor_16x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 32);
}

void aom_smooth_v_predictor_16x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 16, 64);
}
#else
void aom_smooth_v_predictor_16x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i bottom_left = _mm_set1_epi16(left_column[3]);
  const __m128i weights = cvtepu8_epi16(Load4(smooth_weights));
  const __m128i scale = _mm_set1_epi16(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  const __m128i inverted_weights = _mm_sub_epi16(scale, weights);
  const __m128i scaled_bottom_left =
      _mm_mullo_epi16(inverted_weights, bottom_left);
  const __m128i round = _mm_set1_epi16(128);
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i top_lo = cvtepu8_epi16(top);
  const __m128i top_hi = cvtepu8_epi16(_mm_srli_si128(top, 8));

  __m128i y_select = _mm_set1_epi32(0x01000100);
  __m128i weights_y = _mm_shuffle_epi8(weights, y_select);
  __m128i scaled_bottom_left_y = _mm_shuffle_epi8(scaled_bottom_left, y_select);
  write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                 scaled_bottom_left_y, scaled_bottom_left_y,
                                 round);
  dst += stride;
  y_select = _mm_set1_epi32(0x03020302);
  weights_y = _mm_shuffle_epi8(weights, y_select);
  scaled_bottom_left_y = _mm_shuffle_epi8(scaled_bottom_left, y_select);
  write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                 scaled_bottom_left_y, scaled_bottom_left_y,
                                 round);
  dst += stride;
  y_select = _mm_set1_epi32(0x05040504);
  weights_y = _mm_shuffle_epi8(weights, y_select);
  scaled_bottom_left_y = _mm_shuffle_epi8(scaled_bottom_left, y_select);
  write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                 scaled_bottom_left_y, scaled_bottom_left_y,
                                 round);
  dst += stride;
  y_select = _mm_set1_epi32(0x07060706);
  weights_y = _mm_shuffle_epi8(weights, y_select);
  scaled_bottom_left_y = _mm_shuffle_epi8(scaled_bottom_left, y_select);
  write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                 scaled_bottom_left_y, scaled_bottom_left_y,
                                 round);
}

void aom_smooth_v_predictor_16x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i bottom_left = _mm_set1_epi16(left_column[7]);
  const __m128i weights = cvtepu8_epi16(LoadLo8(smooth_weights + 4));
  const __m128i scale = _mm_set1_epi16(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  const __m128i inverted_weights = _mm_sub_epi16(scale, weights);
  const __m128i scaled_bottom_left =
      _mm_mullo_epi16(inverted_weights, bottom_left);
  const __m128i round = _mm_set1_epi16(128);
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i top_lo = cvtepu8_epi16(top);
  const __m128i top_hi = cvtepu8_epi16(_mm_srli_si128(top, 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
}

void aom_smooth_v_predictor_16x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i bottom_left = _mm_set1_epi16(left_column[15]);
  const __m128i zero = _mm_setzero_si128();
  const __m128i scale = _mm_set1_epi16(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  const __m128i weights_lo = cvtepu8_epi16(weights);
  const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
  const __m128i inverted_weights_lo = _mm_sub_epi16(scale, weights_lo);
  const __m128i inverted_weights_hi = _mm_sub_epi16(scale, weights_hi);
  const __m128i scaled_bottom_left_lo =
      _mm_mullo_epi16(inverted_weights_lo, bottom_left);
  const __m128i scaled_bottom_left_hi =
      _mm_mullo_epi16(inverted_weights_hi, bottom_left);
  const __m128i round = _mm_set1_epi16(128);

  const __m128i top = LoadUnaligned16(top_row);
  const __m128i top_lo = cvtepu8_epi16(top);
  const __m128i top_hi = _mm_unpackhi_epi8(top, zero);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights_lo, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left_lo, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights_hi, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left_hi, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
}

void aom_smooth_v_predictor_16x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i bottom_left = _mm_set1_epi16(left_column[31]);
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  const __m128i scale = _mm_set1_epi16(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  const __m128i zero = _mm_setzero_si128();
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = _mm_unpackhi_epi8(weights_lo, zero);
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = _mm_unpackhi_epi8(weights_hi, zero);
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_bottom_left1 =
      _mm_mullo_epi16(inverted_weights1, bottom_left);
  const __m128i scaled_bottom_left2 =
      _mm_mullo_epi16(inverted_weights2, bottom_left);
  const __m128i scaled_bottom_left3 =
      _mm_mullo_epi16(inverted_weights3, bottom_left);
  const __m128i scaled_bottom_left4 =
      _mm_mullo_epi16(inverted_weights4, bottom_left);
  const __m128i round = _mm_set1_epi16(128);

  const __m128i top = LoadUnaligned16(top_row);
  const __m128i top_lo = cvtepu8_epi16(top);
  const __m128i top_hi = _mm_unpackhi_epi8(top, zero);
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights1, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left1, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights2, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left2, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights3, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left3, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i weights_y = _mm_shuffle_epi8(weights4, y_select);
    const __m128i scaled_bottom_left_y =
        _mm_shuffle_epi8(scaled_bottom_left4, y_select);
    write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                   scaled_bottom_left_y, scaled_bottom_left_y,
                                   round);
    dst += stride;
  }
}

void aom_smooth_v_predictor_16x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dst, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const __m128i bottom_left = _mm_set1_epi16(left_column[63]);
  const __m128i scale = _mm_set1_epi16(1 << SMOOTH_WEIGHT_LOG2_SCALE);
  const __m128i round = _mm_set1_epi16(128);
  const __m128i zero = _mm_setzero_si128();
  const __m128i top = LoadUnaligned16(top_row);
  const __m128i top_lo = cvtepu8_epi16(top);
  const __m128i top_hi = _mm_unpackhi_epi8(top, zero);
  const uint8_t *weights_base_ptr = smooth_weights + 60;
  for (int left_offset = 0; left_offset < 64; left_offset += 16) {
    const __m128i weights = LoadUnaligned16(weights_base_ptr + left_offset);
    const __m128i weights_lo = cvtepu8_epi16(weights);
    const __m128i weights_hi = _mm_unpackhi_epi8(weights, zero);
    const __m128i inverted_weights_lo = _mm_sub_epi16(scale, weights_lo);
    const __m128i inverted_weights_hi = _mm_sub_epi16(scale, weights_hi);
    const __m128i scaled_bottom_left_lo =
        _mm_mullo_epi16(inverted_weights_lo, bottom_left);
    const __m128i scaled_bottom_left_hi =
        _mm_mullo_epi16(inverted_weights_hi, bottom_left);

    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i y_select = _mm_set1_epi32(y_mask);
      const __m128i weights_y = _mm_shuffle_epi8(weights_lo, y_select);
      const __m128i scaled_bottom_left_y =
          _mm_shuffle_epi8(scaled_bottom_left_lo, y_select);
      write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                     scaled_bottom_left_y, scaled_bottom_left_y,
                                     round);
      dst += stride;
    }
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i y_select = _mm_set1_epi32(y_mask);
      const __m128i weights_y = _mm_shuffle_epi8(weights_hi, y_select);
      const __m128i scaled_bottom_left_y =
          _mm_shuffle_epi8(scaled_bottom_left_hi, y_select);
      write_smooth_directional_sum16(dst, top_lo, top_hi, weights_y, weights_y,
                                     scaled_bottom_left_y, scaled_bottom_left_y,
                                     round);
      dst += stride;
    }
  }
}
#endif

void aom_smooth_v_predictor_32x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 8);
}

void aom_smooth_v_predictor_32x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 16);
}

void aom_smooth_v_predictor_32x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 32);
}

void aom_smooth_v_predictor_32x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 32, 64);
}

void aom_smooth_v_predictor_64x64_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 64, 64);
}

void aom_smooth_v_predictor_64x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 64, 32);
}

void aom_smooth_v_predictor_64x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                        const uint8_t *above,
                                        const uint8_t *left) {
  smooth_v_predictor_wxh(dst, stride, above, left, 64, 16);
}

// -----------------------------------------------------------------------------
// SMOOTH_H_PRED

// pixels[0]: left vector
// pixels[1]: right_pred vector
static INLINE void load_pixel_h_w4(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  if (height == 4)
    pixels[0] = _mm_cvtsi32_si128(((const int *)left)[0]);
  else if (height == 8)
    pixels[0] = _mm_loadl_epi64(((const __m128i *)left));
  else
    pixels[0] = _mm_loadu_si128(((const __m128i *)left));
  pixels[1] = _mm_set1_epi16((int16_t)above[3]);
}

// weights[0]: weights_w and scale - weights_w interleave vector
static INLINE void load_weight_h_w4(int height, __m128i *weights) {
  (void)height;
  const __m128i t = _mm_loadu_si128((const __m128i *)&smooth_weights[0]);
  const __m128i zero = _mm_setzero_si128();

  const __m128i weights_0 = _mm_unpacklo_epi8(t, zero);
  const __m128i d = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights_1 = _mm_sub_epi16(d, weights_0);
  weights[0] = _mm_unpacklo_epi16(weights_0, weights_1);
}

static INLINE void smooth_h_pred_4xh(const __m128i *pixel,
                                     const __m128i *weight, int h, uint8_t *dst,
                                     ptrdiff_t stride) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i gat = _mm_set1_epi32(0xc080400);
  __m128i rep = _mm_set1_epi16((short)0x8000);

  for (int i = 0; i < h; ++i) {
    __m128i b = _mm_shuffle_epi8(pixel[0], rep);
    b = _mm_unpacklo_epi16(b, pixel[1]);
    __m128i sum = _mm_madd_epi16(b, weight[0]);

    sum = _mm_add_epi32(sum, pred_round);
    sum = _mm_srai_epi32(sum, SMOOTH_WEIGHT_LOG2_SCALE);

    sum = _mm_shuffle_epi8(sum, gat);
    *(int *)dst = _mm_cvtsi128_si32(sum);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
  }
}

void aom_smooth_h_predictor_4x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w4(above, left, 4, pixels);

  __m128i weights;
  load_weight_h_w4(4, &weights);

  smooth_h_pred_4xh(pixels, &weights, 4, dst, stride);
}

void aom_smooth_h_predictor_4x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w4(above, left, 8, pixels);

  __m128i weights;
  load_weight_h_w4(8, &weights);

  smooth_h_pred_4xh(pixels, &weights, 8, dst, stride);
}

void aom_smooth_h_predictor_4x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w4(above, left, 16, pixels);

  __m128i weights;
  load_weight_h_w4(8, &weights);

  smooth_h_pred_4xh(pixels, &weights, 8, dst, stride);
  dst += stride << 3;

  pixels[0] = _mm_srli_si128(pixels[0], 8);
  smooth_h_pred_4xh(pixels, &weights, 8, dst, stride);
}

// pixels[0]: left vector
// pixels[1]: right_pred vector
// pixels[2]: left vector + 16
// pixels[3]: right_pred vector
static INLINE void load_pixel_h_w8(const uint8_t *above, const uint8_t *left,
                                   int height, __m128i *pixels) {
  pixels[1] = _mm_set1_epi16((int16_t)above[7]);

  if (height == 4) {
    pixels[0] = _mm_cvtsi32_si128(((const int *)left)[0]);
  } else if (height == 8) {
    pixels[0] = _mm_loadl_epi64((const __m128i *)left);
  } else if (height == 16) {
    pixels[0] = _mm_load_si128((const __m128i *)left);
  } else {
    pixels[0] = _mm_load_si128((const __m128i *)left);
    pixels[2] = _mm_load_si128((const __m128i *)(left + 16));
    pixels[3] = pixels[1];
  }
}

// weight_w[0]: weights_w and scale - weights_w interleave vector, first half
// weight_w[1]: weights_w and scale - weights_w interleave vector, second half
static INLINE void load_weight_h_w8(int height, __m128i *weight_w) {
  (void)height;
  const __m128i zero = _mm_setzero_si128();
  const __m128i d = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i we = _mm_loadu_si128((const __m128i *)&smooth_weights[4]);
  const __m128i tmp1 = _mm_unpacklo_epi8(we, zero);
  const __m128i tmp2 = _mm_sub_epi16(d, tmp1);
  weight_w[0] = _mm_unpacklo_epi16(tmp1, tmp2);
  weight_w[1] = _mm_unpackhi_epi16(tmp1, tmp2);
}

static INLINE void smooth_h_pred_8xh(const __m128i *pixels, const __m128i *ww,
                                     int h, uint8_t *dst, ptrdiff_t stride,
                                     int second_half) {
  const __m128i pred_round =
      _mm_set1_epi32((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i gat = _mm_set_epi32(0, 0, 0xe0c0a08, 0x6040200);
  __m128i rep = second_half ? _mm_set1_epi16((short)0x8008)
                            : _mm_set1_epi16((short)0x8000);

  for (int i = 0; i < h; ++i) {
    __m128i b = _mm_shuffle_epi8(pixels[0], rep);
    b = _mm_unpacklo_epi16(b, pixels[1]);
    __m128i sum0 = _mm_madd_epi16(b, ww[0]);
    __m128i sum1 = _mm_madd_epi16(b, ww[1]);

    sum0 = _mm_add_epi32(sum0, pred_round);
    sum0 = _mm_srai_epi32(sum0, SMOOTH_WEIGHT_LOG2_SCALE);

    sum1 = _mm_add_epi32(sum1, pred_round);
    sum1 = _mm_srai_epi32(sum1, SMOOTH_WEIGHT_LOG2_SCALE);

    sum0 = _mm_packus_epi16(sum0, sum1);
    sum0 = _mm_shuffle_epi8(sum0, gat);
    _mm_storel_epi64((__m128i *)dst, sum0);
    dst += stride;

    rep = _mm_add_epi16(rep, one);
  }
}

void aom_smooth_h_predictor_8x4_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w8(above, left, 4, pixels);

  __m128i ww[2];
  load_weight_h_w8(4, ww);

  smooth_h_pred_8xh(pixels, ww, 4, dst, stride, 0);
}

void aom_smooth_h_predictor_8x8_ssse3(uint8_t *dst, ptrdiff_t stride,
                                      const uint8_t *above,
                                      const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w8(above, left, 8, pixels);

  __m128i ww[2];
  load_weight_h_w8(8, ww);

  smooth_h_pred_8xh(pixels, ww, 8, dst, stride, 0);
}

void aom_smooth_h_predictor_8x16_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[2];
  load_pixel_h_w8(above, left, 16, pixels);

  __m128i ww[2];
  load_weight_h_w8(16, ww);

  smooth_h_pred_8xh(pixels, ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_h_pred_8xh(pixels, ww, 8, dst, stride, 1);
}

void aom_smooth_h_predictor_8x32_ssse3(uint8_t *dst, ptrdiff_t stride,
                                       const uint8_t *above,
                                       const uint8_t *left) {
  __m128i pixels[4];
  load_pixel_h_w8(above, left, 32, pixels);

  __m128i ww[2];
  load_weight_h_w8(32, ww);

  smooth_h_pred_8xh(&pixels[0], ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_h_pred_8xh(&pixels[0], ww, 8, dst, stride, 1);
  dst += stride << 3;
  smooth_h_pred_8xh(&pixels[2], ww, 8, dst, stride, 0);
  dst += stride << 3;
  smooth_h_pred_8xh(&pixels[2], ww, 8, dst, stride, 1);
}

void aom_smooth_h_predictor_16x4_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[15]);
  const __m128i left = cvtepu8_epi16(Load4(left_column));
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  __m128i y_mask = _mm_set1_epi32(0x01000100);
  __m128i left_y = _mm_shuffle_epi8(left, y_mask);
  uint8_t *dst = (uint8_t *)dest;
  write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                 scaled_top_right1, scaled_top_right2, scale);
  dst += stride;
  y_mask = _mm_set1_epi32(0x03020302);
  left_y = _mm_shuffle_epi8(left, y_mask);
  write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                 scaled_top_right1, scaled_top_right2, scale);
  dst += stride;
  y_mask = _mm_set1_epi32(0x05040504);
  left_y = _mm_shuffle_epi8(left, y_mask);
  write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                 scaled_top_right1, scaled_top_right2, scale);
  dst += stride;
  y_mask = _mm_set1_epi32(0x07060706);
  left_y = _mm_shuffle_epi8(left, y_mask);
  write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                 scaled_top_right1, scaled_top_right2, scale);
}

void aom_smooth_h_predictor_16x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[15]);
  const __m128i left = cvtepu8_epi16(LoadLo8(left_column));
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  __m128i scale = _mm_set1_epi16(256);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  scale = _mm_set1_epi16(128);
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_16x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[15]);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  __m128i scale = _mm_set1_epi16(256);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  scale = _mm_set1_epi16(128);
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  __m128i left = cvtepu8_epi16(LoadLo8(left_column));
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_16x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[15]);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  __m128i scale = _mm_set1_epi16(256);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  scale = _mm_set1_epi16(128);
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  __m128i left = cvtepu8_epi16(LoadLo8(left_column));
  uint8_t *dst = dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 16));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 24));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_16x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[15]);
  const __m128i weights = LoadUnaligned16(smooth_weights + 12);
  __m128i scale = _mm_set1_epi16(256);
  const __m128i weights1 = cvtepu8_epi16(weights);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  scale = _mm_set1_epi16(128);
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  uint8_t *dst = (uint8_t *)dest;
  for (int left_offset = 0; left_offset < 64; left_offset += 8) {
    const __m128i left = cvtepu8_epi16(LoadLo8(left_ptr + left_offset));
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i y_select = _mm_set1_epi32(y_mask);
      const __m128i left_y = _mm_shuffle_epi8(left, y_select);
      write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                     scaled_top_right1, scaled_top_right2,
                                     scale);
      dst += stride;
    }
  }
}

void aom_smooth_h_predictor_32x8_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[31]);
  const __m128i left = cvtepu8_epi16(LoadLo8(left_column));
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_hi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_32x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[31]);
  const __m128i left1 = cvtepu8_epi16(LoadLo8(left_column));
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_hi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left1, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
  const __m128i left2 =
      cvtepu8_epi16(LoadLo8((const uint8_t *)left_column + 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left2, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_32x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[31]);
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_hi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  __m128i left = cvtepu8_epi16(LoadLo8(left_column));
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 16));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
  left = cvtepu8_epi16(LoadLo8(left_ptr + 24));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_32x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[31]);
  const __m128i weights_lo = LoadUnaligned16(smooth_weights + 28);
  const __m128i weights_hi = LoadUnaligned16(smooth_weights + 44);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_hi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_hi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  uint8_t *dst = (uint8_t *)dest;
  for (int left_offset = 0; left_offset < 64; left_offset += 8) {
    const __m128i left = cvtepu8_epi16(LoadLo8(left_ptr + left_offset));
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i y_select = _mm_set1_epi32(y_mask);
      const __m128i left_y = _mm_shuffle_epi8(left, y_select);
      write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                     scaled_top_right1, scaled_top_right2,
                                     scale);
      write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3,
                                     weights4, scaled_top_right3,
                                     scaled_top_right4, scale);
      dst += stride;
    }
  }
}

void aom_smooth_h_predictor_64x16_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[63]);
  const __m128i left1 = cvtepu8_epi16(LoadLo8(left_column));
  const __m128i weights_lolo = LoadUnaligned16(smooth_weights + 60);
  const __m128i weights_lohi = LoadUnaligned16(smooth_weights + 76);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lolo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lolo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_lohi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_lohi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  const __m128i weights_hilo = LoadUnaligned16(smooth_weights + 92);
  const __m128i weights_hihi = LoadUnaligned16(smooth_weights + 108);
  const __m128i weights5 = cvtepu8_epi16(weights_hilo);
  const __m128i weights6 = cvtepu8_epi16(_mm_srli_si128(weights_hilo, 8));
  const __m128i weights7 = cvtepu8_epi16(weights_hihi);
  const __m128i weights8 = cvtepu8_epi16(_mm_srli_si128(weights_hihi, 8));
  const __m128i inverted_weights5 = _mm_sub_epi16(scale, weights5);
  const __m128i inverted_weights6 = _mm_sub_epi16(scale, weights6);
  const __m128i inverted_weights7 = _mm_sub_epi16(scale, weights7);
  const __m128i inverted_weights8 = _mm_sub_epi16(scale, weights8);
  const __m128i scaled_top_right5 =
      _mm_mullo_epi16(inverted_weights5, top_right);
  const __m128i scaled_top_right6 =
      _mm_mullo_epi16(inverted_weights6, top_right);
  const __m128i scaled_top_right7 =
      _mm_mullo_epi16(inverted_weights7, top_right);
  const __m128i scaled_top_right8 =
      _mm_mullo_epi16(inverted_weights8, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left1, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5, weights6,
                                   scaled_top_right5, scaled_top_right6, scale);
    write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7, weights8,
                                   scaled_top_right7, scaled_top_right8, scale);
    dst += stride;
  }
  const __m128i left2 = cvtepu8_epi16(LoadLo8(left_ptr + 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    __m128i y_select = _mm_set1_epi32(y_mask);
    __m128i left_y = _mm_shuffle_epi8(left2, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5, weights6,
                                   scaled_top_right5, scaled_top_right6, scale);
    write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7, weights8,
                                   scaled_top_right7, scaled_top_right8, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_64x32_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[63]);
  const __m128i left1 = cvtepu8_epi16(LoadLo8(left_column));
  const __m128i weights_lolo = LoadUnaligned16(smooth_weights + 60);
  const __m128i weights_lohi = LoadUnaligned16(smooth_weights + 76);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lolo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lolo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_lohi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_lohi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  const __m128i weights_hilo = LoadUnaligned16(smooth_weights + 92);
  const __m128i weights_hihi = LoadUnaligned16(smooth_weights + 108);
  const __m128i weights5 = cvtepu8_epi16(weights_hilo);
  const __m128i weights6 = cvtepu8_epi16(_mm_srli_si128(weights_hilo, 8));
  const __m128i weights7 = cvtepu8_epi16(weights_hihi);
  const __m128i weights8 = cvtepu8_epi16(_mm_srli_si128(weights_hihi, 8));
  const __m128i inverted_weights5 = _mm_sub_epi16(scale, weights5);
  const __m128i inverted_weights6 = _mm_sub_epi16(scale, weights6);
  const __m128i inverted_weights7 = _mm_sub_epi16(scale, weights7);
  const __m128i inverted_weights8 = _mm_sub_epi16(scale, weights8);
  const __m128i scaled_top_right5 =
      _mm_mullo_epi16(inverted_weights5, top_right);
  const __m128i scaled_top_right6 =
      _mm_mullo_epi16(inverted_weights6, top_right);
  const __m128i scaled_top_right7 =
      _mm_mullo_epi16(inverted_weights7, top_right);
  const __m128i scaled_top_right8 =
      _mm_mullo_epi16(inverted_weights8, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  uint8_t *dst = (uint8_t *)dest;
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left1, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5, weights6,
                                   scaled_top_right5, scaled_top_right6, scale);
    write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7, weights8,
                                   scaled_top_right7, scaled_top_right8, scale);
    dst += stride;
  }
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  const __m128i left2 = cvtepu8_epi16(LoadLo8(left_ptr + 8));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left2, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5, weights6,
                                   scaled_top_right5, scaled_top_right6, scale);
    write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7, weights8,
                                   scaled_top_right7, scaled_top_right8, scale);
    dst += stride;
  }
  const __m128i left3 = cvtepu8_epi16(LoadLo8(left_ptr + 16));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left3, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5, weights6,
                                   scaled_top_right5, scaled_top_right6, scale);
    write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7, weights8,
                                   scaled_top_right7, scaled_top_right8, scale);
    dst += stride;
  }
  const __m128i left4 = cvtepu8_epi16(LoadLo8(left_ptr + 24));
  for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
    const __m128i y_select = _mm_set1_epi32(y_mask);
    const __m128i left_y = _mm_shuffle_epi8(left4, y_select);
    write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                   scaled_top_right1, scaled_top_right2, scale);
    write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3, weights4,
                                   scaled_top_right3, scaled_top_right4, scale);
    write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5, weights6,
                                   scaled_top_right5, scaled_top_right6, scale);
    write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7, weights8,
                                   scaled_top_right7, scaled_top_right8, scale);
    dst += stride;
  }
}

void aom_smooth_h_predictor_64x64_ssse3(
    uint8_t *LIBAOM_RESTRICT dest, ptrdiff_t stride,
    const uint8_t *LIBAOM_RESTRICT top_row,
    const uint8_t *LIBAOM_RESTRICT left_column) {
  const uint8_t *const top = (const uint8_t *)top_row;
  const __m128i top_right = _mm_set1_epi16(top[63]);
  const __m128i weights_lolo = LoadUnaligned16(smooth_weights + 60);
  const __m128i weights_lohi = LoadUnaligned16(smooth_weights + 76);
  __m128i scale = _mm_set1_epi16((int16_t)(1 << SMOOTH_WEIGHT_LOG2_SCALE));
  const __m128i weights1 = cvtepu8_epi16(weights_lolo);
  const __m128i weights2 = cvtepu8_epi16(_mm_srli_si128(weights_lolo, 8));
  const __m128i weights3 = cvtepu8_epi16(weights_lohi);
  const __m128i weights4 = cvtepu8_epi16(_mm_srli_si128(weights_lohi, 8));
  const __m128i inverted_weights1 = _mm_sub_epi16(scale, weights1);
  const __m128i inverted_weights2 = _mm_sub_epi16(scale, weights2);
  const __m128i inverted_weights3 = _mm_sub_epi16(scale, weights3);
  const __m128i inverted_weights4 = _mm_sub_epi16(scale, weights4);
  const __m128i scaled_top_right1 =
      _mm_mullo_epi16(inverted_weights1, top_right);
  const __m128i scaled_top_right2 =
      _mm_mullo_epi16(inverted_weights2, top_right);
  const __m128i scaled_top_right3 =
      _mm_mullo_epi16(inverted_weights3, top_right);
  const __m128i scaled_top_right4 =
      _mm_mullo_epi16(inverted_weights4, top_right);
  const __m128i weights_hilo = LoadUnaligned16(smooth_weights + 92);
  const __m128i weights_hihi = LoadUnaligned16(smooth_weights + 108);
  const __m128i weights5 = cvtepu8_epi16(weights_hilo);
  const __m128i weights6 = cvtepu8_epi16(_mm_srli_si128(weights_hilo, 8));
  const __m128i weights7 = cvtepu8_epi16(weights_hihi);
  const __m128i weights8 = cvtepu8_epi16(_mm_srli_si128(weights_hihi, 8));
  const __m128i inverted_weights5 = _mm_sub_epi16(scale, weights5);
  const __m128i inverted_weights6 = _mm_sub_epi16(scale, weights6);
  const __m128i inverted_weights7 = _mm_sub_epi16(scale, weights7);
  const __m128i inverted_weights8 = _mm_sub_epi16(scale, weights8);
  const __m128i scaled_top_right5 =
      _mm_mullo_epi16(inverted_weights5, top_right);
  const __m128i scaled_top_right6 =
      _mm_mullo_epi16(inverted_weights6, top_right);
  const __m128i scaled_top_right7 =
      _mm_mullo_epi16(inverted_weights7, top_right);
  const __m128i scaled_top_right8 =
      _mm_mullo_epi16(inverted_weights8, top_right);
  scale = _mm_set1_epi16((1 << (SMOOTH_WEIGHT_LOG2_SCALE - 1)));
  const uint8_t *const left_ptr = (const uint8_t *)left_column;
  uint8_t *dst = (uint8_t *)dest;
  for (int left_offset = 0; left_offset < 64; left_offset += 8) {
    const __m128i left = cvtepu8_epi16(LoadLo8(left_ptr + left_offset));
    for (int y_mask = 0x01000100; y_mask < 0x0F0E0F0F; y_mask += 0x02020202) {
      const __m128i y_select = _mm_set1_epi32(y_mask);
      const __m128i left_y = _mm_shuffle_epi8(left, y_select);
      write_smooth_directional_sum16(dst, left_y, left_y, weights1, weights2,
                                     scaled_top_right1, scaled_top_right2,
                                     scale);
      write_smooth_directional_sum16(dst + 16, left_y, left_y, weights3,
                                     weights4, scaled_top_right3,
                                     scaled_top_right4, scale);
      write_smooth_directional_sum16(dst + 32, left_y, left_y, weights5,
                                     weights6, scaled_top_right5,
                                     scaled_top_right6, scale);
      write_smooth_directional_sum16(dst + 48, left_y, left_y, weights7,
                                     weights8, scaled_top_right7,
                                     scaled_top_right8, scale);
      dst += stride;
    }
  }
}
