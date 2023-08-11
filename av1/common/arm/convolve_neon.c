/*
 *
 * Copyright (c) 2018, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <arm_neon.h>

#include "config/aom_config.h"
#include "config/av1_rtcd.h"

#include "aom_dsp/aom_dsp_common.h"
#include "aom_dsp/arm/mem_neon.h"
#include "aom_dsp/arm/transpose_neon.h"
#include "aom_ports/mem.h"
#include "av1/common/convolve.h"
#include "av1/common/filter.h"
#include "av1/common/arm/convolve_neon.h"

static INLINE int16x4_t convolve12_4_x(const int16x4_t s0, const int16x4_t s1,
                                       const int16x4_t s2, const int16x4_t s3,
                                       const int16x4_t s4, const int16x4_t s5,
                                       const int16x4_t s6, const int16x4_t s7,
                                       const int16x4_t s8, const int16x4_t s9,
                                       const int16x4_t s10, const int16x4_t s11,
                                       const int16x8_t x_filter_0_7,
                                       const int16x4_t x_filter_8_11,
                                       const int32x4_t horiz_const) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum = horiz_const;
  sum = vmlal_lane_s16(sum, s0, x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, x_filter_8_11, 3);

  return vqrshrn_n_s32(sum, FILTER_BITS);
}

static INLINE void convolve_x_sr_12tap_neon(const uint8_t *src_ptr,
                                            int src_stride, uint8_t *dst_ptr,
                                            const int dst_stride, int w, int h,
                                            const int16_t *x_filter_ptr) {
  const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
  const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);

  // A shim of 1 << (ROUND0_BITS - 1) enables us to use a single rounding right
  // shift by FILTER_BITS - instead of a first rounding right shift by
  // ROUND0_BITS, followed by second rounding right shift by FILTER_BITS -
  // ROUND0_BITS.
  const int32x4_t horiz_const = vdupq_n_s32(1 << (ROUND0_BITS - 1));

#if AOM_ARCH_AARCH64
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    uint8x8_t t0, t1, t2, t3;
    load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

    load_u8_8x4(s + 8, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    s += 11;

    do {
      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      int16x4_t s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      int16x4_t s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      int16x4_t s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      int16x4_t d0 =
          convolve12_4_x(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                         x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d1 =
          convolve12_4_x(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                         x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d2 =
          convolve12_4_x(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                         x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d3 =
          convolve12_4_x(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
                         x_filter_0_7, x_filter_8_11, horiz_const);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);

      uint8x8_t d01 = vqmovun_s16(vcombine_s16(d0, d1));
      uint8x8_t d23 = vqmovun_s16(vcombine_s16(d2, d3));

      if (w == 2) {
        store_u8_2x1(d + 0 * dst_stride, d01, 0);
        store_u8_2x1(d + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(d + 2 * dst_stride, d23, 0);
          store_u8_2x1(d + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(d + 0 * dst_stride, d01, 0);
        store_u8_4x1(d + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(d + 2 * dst_stride, d23, 0);
          store_u8_4x1(d + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    h -= 4;
  } while (h > 0);

#else   // !AOM_ARCH_AARCH64
  do {
    const uint8_t *s = src_ptr;
    uint8_t *d = dst_ptr;
    int width = w;

    do {
      uint8x16_t t0 = vld1q_u8(s);
      int16x8_t tt0 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(t0)));
      int16x8_t tt8 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(t0)));

      int16x4_t s0 = vget_low_s16(tt0);
      int16x4_t s4 = vget_high_s16(tt0);
      int16x4_t s8 = vget_low_s16(tt8);
      int16x4_t s12 = vget_high_s16(tt8);

      int16x4_t s1 = vext_s16(s0, s4, 1);    //  a1  a2  a3  a4
      int16x4_t s2 = vext_s16(s0, s4, 2);    //  a2  a3  a4  a5
      int16x4_t s3 = vext_s16(s0, s4, 3);    //  a3  a4  a5  a6
      int16x4_t s5 = vext_s16(s4, s8, 1);    //  a5  a6  a7  a8
      int16x4_t s6 = vext_s16(s4, s8, 2);    //  a6  a7  a8  a9
      int16x4_t s7 = vext_s16(s4, s8, 3);    //  a7  a8  a9 a10
      int16x4_t s9 = vext_s16(s8, s12, 1);   //  a9 a10 a11 a12
      int16x4_t s10 = vext_s16(s8, s12, 2);  // a10 a11 a12 a13
      int16x4_t s11 = vext_s16(s8, s12, 3);  // a11 a12 a13 a14

      int16x4_t d0 =
          convolve12_4_x(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                         x_filter_0_7, x_filter_8_11, horiz_const);

      uint8x8_t dd0 = vqmovun_s16(vcombine_s16(d0, vdup_n_s16(0)));

      if (w == 2) {
        store_u8_2x1(d, dd0, 0);
      } else {
        store_u8_4x1(d, dd0, 0);
      }

      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
#endif  // AOM_ARCH_AARCH64
}

static INLINE uint8x8_t convolve4_4_x(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t filter,
                                      const int16x4_t horiz_const) {
  int16x4_t sum = horiz_const;
  sum = vmla_lane_s16(sum, s0, filter, 0);
  sum = vmla_lane_s16(sum, s1, filter, 1);
  sum = vmla_lane_s16(sum, s2, filter, 2);
  sum = vmla_lane_s16(sum, s3, filter, 3);

  // We halved the convolution filter values so - 1 from the right shift.
  return vqrshrun_n_s16(vcombine_s16(sum, vdup_n_s16(0)), FILTER_BITS - 1);
}

static INLINE uint8x8_t convolve8_8_x(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t s6, const int16x8_t s7,
                                      const int16x8_t filter,
                                      const int16x8_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so - 1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

void av1_convolve_x_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_x,
                            const int subpel_x_qn,
                            ConvolveParams *conv_params) {
  (void)conv_params;
  const uint8_t horiz_offset = filter_params_x->taps / 2 - 1;
  src -= horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);

  if (filter_params_x->taps > 8) {
    convolve_x_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             x_filter_ptr);
    return;
  }

  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use a single
  // rounding right shift by FILTER_BITS - instead of a first rounding right
  // shift by ROUND0_BITS, followed by second rounding right shift by
  // FILTER_BITS - ROUND0_BITS.
  // The outermost -1 is needed because we will halve the filter values.
  const int16x8_t horiz_const = vdupq_n_s16(1 << ((ROUND0_BITS - 1) - 1));

  if (w <= 4) {
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16x4_t x_filter = vshr_n_s16(vld1_s16(x_filter_ptr + 2), 1);

    src += 2;

    do {
      uint8x8_t t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
      int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));

      int16x4_t s1 = vext_s16(s0, s4, 1);  // a1 a2 a3 a4
      int16x4_t s2 = vext_s16(s0, s4, 2);  // a2 a3 a4 a5
      int16x4_t s3 = vext_s16(s0, s4, 3);  // a3 a4 a5 a6

      uint8x8_t d0 =
          convolve4_4_x(s0, s1, s2, s3, x_filter, vget_low_s16(horiz_const));

      if (w == 4) {
        store_u8_4x1(dst, d0, 0);
      } else if (w == 2) {
        store_u8_2x1(dst, d0, 0);
      }

      src += src_stride;
      dst += dst_stride;
    } while (--h != 0);
  } else {
    // Filter values are even so halve to reduce precision requirements.
    const int16x8_t x_filter = vshrq_n_s16(vld1q_s16(x_filter_ptr), 1);

#if AOM_ARCH_AARCH64
    while (h >= 8) {
      uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
      load_u8_8x8(src, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      int width = w;
      const uint8_t *s = src + 7;
      uint8_t *d = dst;

      __builtin_prefetch(d + 0 * dst_stride);
      __builtin_prefetch(d + 1 * dst_stride);
      __builtin_prefetch(d + 2 * dst_stride);
      __builtin_prefetch(d + 3 * dst_stride);
      __builtin_prefetch(d + 4 * dst_stride);
      __builtin_prefetch(d + 5 * dst_stride);
      __builtin_prefetch(d + 6 * dst_stride);
      __builtin_prefetch(d + 7 * dst_stride);

      do {
        uint8x8_t t8, t9, t10, t11, t12, t13, t14;
        load_u8_8x8(s, src_stride, &t7, &t8, &t9, &t10, &t11, &t12, &t13, &t14);

        transpose_u8_8x8(&t7, &t8, &t9, &t10, &t11, &t12, &t13, &t14);
        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
        int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
        int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));
        int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t11));
        int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t12));
        int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t13));
        int16x8_t s14 = vreinterpretq_s16_u16(vmovl_u8(t14));

        uint8x8_t d0 = convolve8_8_x(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                     horiz_const);
        uint8x8_t d1 = convolve8_8_x(s1, s2, s3, s4, s5, s6, s7, s8, x_filter,
                                     horiz_const);
        uint8x8_t d2 = convolve8_8_x(s2, s3, s4, s5, s6, s7, s8, s9, x_filter,
                                     horiz_const);
        uint8x8_t d3 = convolve8_8_x(s3, s4, s5, s6, s7, s8, s9, s10, x_filter,
                                     horiz_const);
        uint8x8_t d4 = convolve8_8_x(s4, s5, s6, s7, s8, s9, s10, s11, x_filter,
                                     horiz_const);
        uint8x8_t d5 = convolve8_8_x(s5, s6, s7, s8, s9, s10, s11, s12,
                                     x_filter, horiz_const);
        uint8x8_t d6 = convolve8_8_x(s6, s7, s8, s9, s10, s11, s12, s13,
                                     x_filter, horiz_const);
        uint8x8_t d7 = convolve8_8_x(s7, s8, s9, s10, s11, s12, s13, s14,
                                     x_filter, horiz_const);

        transpose_u8_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        store_u8_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src += 8 * src_stride;
      dst += 8 * dst_stride;
      h -= 8;
    }
#endif  // !AOM_ARCH_AARCH64

    while (h-- != 0) {
      uint8x8_t t0 = vld1_u8(src);  // a0 a1 a2 a3 a4 a5 a6 a7
      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));

      int width = w;
      const uint8_t *s = src + 8;
      uint8_t *d = dst;

      __builtin_prefetch(d);

      do {
        uint8x8_t t8 = vld1_u8(s);  // a8 a9 a10 a11 a12 a13 a14 a15
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));

        int16x8_t s1 = vextq_s16(s0, s8, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
        int16x8_t s2 = vextq_s16(s0, s8, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
        int16x8_t s3 = vextq_s16(s0, s8, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
        int16x8_t s4 = vextq_s16(s0, s8, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
        int16x8_t s5 = vextq_s16(s0, s8, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
        int16x8_t s6 = vextq_s16(s0, s8, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
        int16x8_t s7 = vextq_s16(s0, s8, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

        uint8x8_t d0 = convolve8_8_x(s0, s1, s2, s3, s4, s5, s6, s7, x_filter,
                                     horiz_const);

        vst1_u8(d, d0);

        s0 = s8;
        s += 8;
        d += 8;
        width -= 8;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    }
  }
}

static INLINE int16x4_t convolve6_4_y(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x8_t y_filter_0_7) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  // Filter values at indices 0 and 7 are 0.
  int16x4_t sum = vmul_lane_s16(s0, y_filter_0_3, 1);
  sum = vmla_lane_s16(sum, s1, y_filter_0_3, 2);
  sum = vmla_lane_s16(sum, s2, y_filter_0_3, 3);
  sum = vmla_lane_s16(sum, s3, y_filter_4_7, 0);
  sum = vmla_lane_s16(sum, s4, y_filter_4_7, 1);
  sum = vmla_lane_s16(sum, s5, y_filter_4_7, 2);

  return sum;
}

static INLINE uint8x8_t convolve6_8_y(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t y_filters) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filters);
  const int16x4_t y_filter_hi = vget_high_s16(y_filters);

  // Filter values at indices 0 and 7 are 0.
  int16x8_t sum = vmulq_lane_s16(s0, y_filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s1, y_filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s2, y_filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s3, y_filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s4, y_filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s5, y_filter_hi, 2);
  // We halved the convolution filter values so -1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_y_sr_6tap_neon(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16x8_t y_filter) {
  if (w <= 4) {
    uint8x8_t t0 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
    uint8x8_t t1 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
    uint8x8_t t2 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
    uint8x8_t t3 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);
    uint8x8_t t4 = load_unaligned_u8_4x1(src_ptr + 4 * src_stride);

    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));

    src_ptr += 5 * src_stride;

    do {
#if AOM_ARCH_AARCH64
      uint8x8_t t5 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
      uint8x8_t t6 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
      uint8x8_t t7 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
      uint8x8_t t8 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);

      int16x4_t s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
      int16x4_t s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));
      int16x4_t s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t7)));
      int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t8)));

      int16x4_t d0 = convolve6_4_y(s0, s1, s2, s3, s4, s5, y_filter);
      int16x4_t d1 = convolve6_4_y(s1, s2, s3, s4, s5, s6, y_filter);
      int16x4_t d2 = convolve6_4_y(s2, s3, s4, s5, s6, s7, y_filter);
      int16x4_t d3 = convolve6_4_y(s3, s4, s5, s6, s7, s8, y_filter);

      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
#else   // !AOM_ARCH_AARCH64
      uint8x8_t t5 = load_unaligned_u8_4x1(src_ptr);
      int16x4_t s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));

      int16x4_t d0 = convolve6_4_y(s0, s1, s2, s3, s4, s5, y_filter);
      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 =
          vqrshrun_n_s16(vcombine_s16(d0, vdup_n_s16(0)), FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(dst_ptr, d01, 0);
      } else {
        store_u8_4x1(dst_ptr, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      h--;
#endif  // AOM_ARCH_AARCH64
    } while (h > 0);

  } else {
    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      uint8x8_t t0, t1, t2, t3, t4;
      load_u8_8x5(s, src_stride, &t0, &t1, &t2, &t3, &t4);

      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));

      s += 5 * src_stride;

      do {
#if AOM_ARCH_AARCH64
        uint8x8_t t5, t6, t7, t8;
        load_u8_8x4(s, src_stride, &t5, &t6, &t7, &t8);

        int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
        int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));

        uint8x8_t d0 = convolve6_8_y(s0, s1, s2, s3, s4, s5, y_filter);
        uint8x8_t d1 = convolve6_8_y(s1, s2, s3, s4, s5, s6, y_filter);
        uint8x8_t d2 = convolve6_8_y(s2, s3, s4, s5, s6, s7, y_filter);
        uint8x8_t d3 = convolve6_8_y(s3, s4, s5, s6, s7, s8, y_filter);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !AOM_ARCH_AARCH64
        int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));

        uint8x8_t d0 = convolve6_8_y(s0, s1, s2, s3, s4, s5, y_filter);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // AOM_ARCH_AARCH64
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE int16x4_t convolve8_4_y(const int16x4_t s0, const int16x4_t s1,
                                      const int16x4_t s2, const int16x4_t s3,
                                      const int16x4_t s4, const int16x4_t s5,
                                      const int16x4_t s6, const int16x4_t s7,
                                      const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x4_t sum = vmul_lane_s16(s0, filter_lo, 0);
  sum = vmla_lane_s16(sum, s1, filter_lo, 1);
  sum = vmla_lane_s16(sum, s2, filter_lo, 2);
  sum = vmla_lane_s16(sum, s3, filter_lo, 3);
  sum = vmla_lane_s16(sum, s4, filter_hi, 0);
  sum = vmla_lane_s16(sum, s5, filter_hi, 1);
  sum = vmla_lane_s16(sum, s6, filter_hi, 2);
  sum = vmla_lane_s16(sum, s7, filter_hi, 3);

  return sum;
}

static INLINE uint8x8_t convolve8_8_y(const int16x8_t s0, const int16x8_t s1,
                                      const int16x8_t s2, const int16x8_t s3,
                                      const int16x8_t s4, const int16x8_t s5,
                                      const int16x8_t s6, const int16x8_t s7,
                                      const int16x8_t filter) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = vmulq_lane_s16(s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so -1 from the right shift.
  return vqrshrun_n_s16(sum, FILTER_BITS - 1);
}

static INLINE void convolve_y_sr_8tap_neon(const uint8_t *src_ptr,
                                           int src_stride, uint8_t *dst_ptr,
                                           const int dst_stride, int w, int h,
                                           const int16x8_t y_filter) {
  if (w <= 4) {
    uint8x8_t t0 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
    uint8x8_t t1 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
    uint8x8_t t2 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
    uint8x8_t t3 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);
    uint8x8_t t4 = load_unaligned_u8_4x1(src_ptr + 4 * src_stride);
    uint8x8_t t5 = load_unaligned_u8_4x1(src_ptr + 5 * src_stride);
    uint8x8_t t6 = load_unaligned_u8_4x1(src_ptr + 6 * src_stride);

    int16x4_t s0 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t0)));
    int16x4_t s1 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t1)));
    int16x4_t s2 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t2)));
    int16x4_t s3 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t3)));
    int16x4_t s4 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t4)));
    int16x4_t s5 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t5)));
    int16x4_t s6 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t6)));

    src_ptr += 7 * src_stride;

    do {
#if AOM_ARCH_AARCH64
      uint8x8_t t7 = load_unaligned_u8_4x1(src_ptr + 0 * src_stride);
      uint8x8_t t8 = load_unaligned_u8_4x1(src_ptr + 1 * src_stride);
      uint8x8_t t9 = load_unaligned_u8_4x1(src_ptr + 2 * src_stride);
      uint8x8_t t10 = load_unaligned_u8_4x1(src_ptr + 3 * src_stride);

      int16x4_t s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t7)));
      int16x4_t s8 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t8)));
      int16x4_t s9 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t9)));
      int16x4_t s10 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t10)));

      int16x4_t d0 = convolve8_4_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      int16x4_t d1 = convolve8_4_y(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      int16x4_t d2 = convolve8_4_y(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      int16x4_t d3 = convolve8_4_y(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS - 1);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS - 1);

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
#else   // !AOM_ARCH_AARCH64
      uint8x8_t t7 = load_unaligned_u8_4x1(src_ptr);
      int16x4_t s7 = vreinterpret_s16_u16(vget_low_u16(vmovl_u8(t7)));

      int16x4_t d0 = convolve8_4_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      // We halved the convolution filter values so -1 from the right shift.
      uint8x8_t d01 =
          vqrshrun_n_s16(vcombine_s16(d0, vdup_n_s16(0)), FILTER_BITS - 1);

      if (w == 4) {
        store_u8_4x1(dst_ptr, d01, 0);
      } else if (w == 2) {
        store_u8_2x1(dst_ptr, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
      s6 = s7;
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      h--;
#endif  // AOM_ARCH_AARCH64
    } while (h > 0);
  } else {
    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      uint8x8_t t0, t1, t2, t3, t4, t5, t6;
      load_u8_8x7(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6);

      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      s += 7 * src_stride;

      do {
#if AOM_ARCH_AARCH64
        uint8x8_t t7, t8, t9, t10;
        load_u8_8x4(s, src_stride, &t7, &t8, &t9, &t10);

        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
        int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
        int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));

        uint8x8_t d0 = convolve8_8_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
        uint8x8_t d1 = convolve8_8_y(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
        uint8x8_t d2 = convolve8_8_y(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
        uint8x8_t d3 = convolve8_8_y(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !AOM_ARCH_AARCH64
        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(s)));

        uint8x8_t d0 = convolve8_8_y(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);

        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // AOM_ARCH_AARCH64
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE int16x4_t convolve12_4_y(const int16x4_t s0, const int16x4_t s1,
                                       const int16x4_t s2, const int16x4_t s3,
                                       const int16x4_t s4, const int16x4_t s5,
                                       const int16x4_t s6, const int16x4_t s7,
                                       const int16x4_t s8, const int16x4_t s9,
                                       const int16x4_t s10, const int16x4_t s11,
                                       const int16x8_t y_filter_0_7,
                                       const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x4_t sum;

  sum = vmul_lane_s16(s0, y_filter_0_3, 0);
  sum = vmla_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmla_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmla_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmla_lane_s16(sum, s4, y_filter_4_7, 0);

  sum = vmla_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmla_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmla_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmla_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmla_lane_s16(sum, s11, y_filter_8_11, 3);

  // Saturating addition is required for the largest filter taps to avoid
  // overflow (while staying in 16-bit elements.)
  sum = vqadd_s16(sum, vmul_lane_s16(s5, y_filter_4_7, 1));
  sum = vqadd_s16(sum, vmul_lane_s16(s6, y_filter_4_7, 2));

  return sum;
}

static INLINE uint8x8_t convolve12_8_y(const int16x8_t s0, const int16x8_t s1,
                                       const int16x8_t s2, const int16x8_t s3,
                                       const int16x8_t s4, const int16x8_t s5,
                                       const int16x8_t s6, const int16x8_t s7,
                                       const int16x8_t s8, const int16x8_t s9,
                                       const int16x8_t s10, const int16x8_t s11,
                                       const int16x8_t y_filter_0_7,
                                       const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, y_filter_0_3, 0);
  sum = vmlaq_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlaq_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlaq_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlaq_lane_s16(sum, s4, y_filter_4_7, 0);

  sum = vmlaq_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmlaq_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmlaq_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmlaq_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmlaq_lane_s16(sum, s11, y_filter_8_11, 3);

  // Saturating addition is required for the largest filter taps to avoid
  // overflow (while staying in 16-bit elements.)
  sum = vqaddq_s16(sum, vmulq_lane_s16(s5, y_filter_4_7, 1));
  sum = vqaddq_s16(sum, vmulq_lane_s16(s6, y_filter_4_7, 2));

  return vqrshrun_n_s16(sum, FILTER_BITS);
}

static INLINE void convolve_y_sr_12tap_neon(const uint8_t *src_ptr,
                                            int src_stride, uint8_t *dst_ptr,
                                            int dst_stride, int w, int h,
                                            const int16_t *y_filter_ptr) {
  const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
  const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

  if (w <= 4) {
    uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
    load_u8_8x11(src_ptr, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7,
                 &t8, &t9, &t10);
    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t4)));
    int16x4_t s5 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t5)));
    int16x4_t s6 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t6)));
    int16x4_t s7 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t7)));
    int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t8)));
    int16x4_t s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t9)));
    int16x4_t s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t10)));

    src_ptr += 11 * src_stride;

    do {
      uint8x8_t t11, t12, t13, t14;
      load_u8_8x4(src_ptr, src_stride, &t11, &t12, &t13, &t14);

      int16x4_t s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t11)));
      int16x4_t s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t12)));
      int16x4_t s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t13)));
      int16x4_t s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t14)));

      int16x4_t d0 = convolve12_4_y(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, y_filter_0_7, y_filter_8_11);
      int16x4_t d1 = convolve12_4_y(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                    s11, s12, y_filter_0_7, y_filter_8_11);
      int16x4_t d2 = convolve12_4_y(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                    s12, s13, y_filter_0_7, y_filter_8_11);
      int16x4_t d3 = convolve12_4_y(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                    s13, s14, y_filter_0_7, y_filter_8_11);

      uint8x8_t d01 = vqrshrun_n_s16(vcombine_s16(d0, d1), FILTER_BITS);
      uint8x8_t d23 = vqrshrun_n_s16(vcombine_s16(d2, d3), FILTER_BITS);

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h > 0);

  } else {
    do {
      const uint8_t *s = src_ptr;
      uint8_t *d = dst_ptr;
      int height = h;

      uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7, t8, t9, t10;
      load_u8_8x11(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7, &t8,
                   &t9, &t10);
      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));
      int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t7));
      int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t8));
      int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t9));
      int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t10));

      s += 11 * src_stride;

      do {
        uint8x8_t t11, t12, t13, t14;
        load_u8_8x4(s, src_stride, &t11, &t12, &t13, &t14);

        int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t11));
        int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t12));
        int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t13));
        int16x8_t s14 = vreinterpretq_s16_u16(vmovl_u8(t14));

        uint8x8_t d0 = convolve12_8_y(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                      s10, s11, y_filter_0_7, y_filter_8_11);
        uint8x8_t d1 = convolve12_8_y(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                      s11, s12, y_filter_0_7, y_filter_8_11);
        uint8x8_t d2 = convolve12_8_y(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                      s12, s13, y_filter_0_7, y_filter_8_11);
        uint8x8_t d3 = convolve12_8_y(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                                      s13, s14, y_filter_0_7, y_filter_8_11);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s7 = s11;
        s8 = s12;
        s9 = s13;
        s10 = s14;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

void av1_convolve_y_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                            int dst_stride, int w, int h,
                            const InterpFilterParams *filter_params_y,
                            const int subpel_y_qn) {
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
  const int vert_offset = clamped_y_taps / 2 - 1;

  src -= vert_offset * src_stride;

  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (y_filter_taps > 8) {
    convolve_y_sr_12tap_neon(src, src_stride, dst, dst_stride, w, h,
                             y_filter_ptr);
    return;
  }

  // Filter values are even so halve to reduce precision requirements.
  const int16x8_t y_filter = vshrq_n_s16(vld1q_s16(y_filter_ptr), 1);

  if (y_filter_taps < 8) {
    convolve_y_sr_6tap_neon(src, src_stride, dst, dst_stride, w, h, y_filter);
  } else {
    convolve_y_sr_8tap_neon(src, src_stride, dst, dst_stride, w, h, y_filter);
  }
}

#if AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE int16x4_t convolve12_4_2d_h(uint8x16_t samples,
                                          const int8x16_t filters,
                                          const uint8x16x3_t permute_tbl,
                                          int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum;

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  // First 4 output values.
  sum = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum = vusdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vusdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  // Narrow and re-pack.
  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve12_8_2d_h(uint8x16_t samples0,
                                          uint8x16_t samples1,
                                          const int8x16_t filters,
                                          const uint8x16x3_t permute_tbl,
                                          const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[4];
  int32x4_t sum[2];

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples0, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples0, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples0, permute_tbl.val[2]);
  // {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 }
  permuted_samples[3] = vqtbl1q_u8(samples1, permute_tbl.val[2]);

  // First 4 output values.
  sum[0] = vusdotq_laneq_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vusdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  // Second 4 output values.
  sum[1] = vusdotq_laneq_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vusdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  // Narrow and re-pack.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS),
                      vshrn_n_s32(sum[1], ROUND0_BITS));
}

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;

  // Special case the following no-op filter as 128 won't fit into the
  // 8-bit signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(x_filter_0_7, 5) == 128) {
    const uint16x8_t horiz_const = vdupq_n_u16((1 << (bd - 1)));
    // Undo the horizontal offset in the calling function.
    src_ptr += 5;

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x8_t s0 = vld1_u8(s);
        uint16x8_t d0 = vaddw_u8(horiz_const, s0);
        d0 = vshlq_n_u16(d0, FILTER_BITS - ROUND0_BITS);
        // Store 8 elements to avoid additional branches. This is safe if the
        // actual block width is < 8 because the intermediate buffer is large
        // enough to accommodate 128x128 blocks.
        vst1q_s16(d, vreinterpretq_s16_u16(d0));

        d += 8;
        s += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);

  } else {
    // Narrow filter values to 8-bit.
    const int16x8x2_t x_filter_s16 = {
      { x_filter_0_7, vcombine_s16(x_filter_8_11, vdup_n_s16(0)) }
    };
    const int8x16_t x_filter = vcombine_s8(vmovn_s16(x_filter_s16.val[0]),
                                           vmovn_s16(x_filter_s16.val[1]));
    // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
    // - which are generally faster than rounding shifts on modern CPUs.
    const int32x4_t horiz_const =
        vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);

    if (w <= 4) {
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

        int16x4_t d0 =
            convolve12_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
        int16x4_t d1 =
            convolve12_4_2d_h(s1, x_filter, permute_tbl, horiz_const);
        int16x4_t d2 =
            convolve12_4_2d_h(s2, x_filter, permute_tbl, horiz_const);
        int16x4_t d3 =
            convolve12_4_2d_h(s3, x_filter, permute_tbl, horiz_const);

        // Store 4 elements per row to avoid additional branches. (Safe.)
        store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h > 4);

      do {
        uint8x16_t s0 = vld1q_u8(src_ptr);
        int16x4_t d0 =
            convolve12_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
        // Store 4 elements to avoid additional branches. Safe as noted above.
        vst1_s16(dst_ptr, d0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--h != 0);

    } else {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2], s1[2], s2[2], s3[2];
          load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
          load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

          int16x8_t d0 = convolve12_8_2d_h(s0[0], s0[1], x_filter, permute_tbl,
                                           horiz_const);
          int16x8_t d1 = convolve12_8_2d_h(s1[0], s1[1], x_filter, permute_tbl,
                                           horiz_const);
          int16x8_t d2 = convolve12_8_2d_h(s2[0], s2[1], x_filter, permute_tbl,
                                           horiz_const);
          int16x8_t d3 = convolve12_8_2d_h(s3[0], s3[1], x_filter, permute_tbl,
                                           horiz_const);

          store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h > 4);

      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2];
          s0[0] = vld1q_u8(s);
          s0[1] = vld1q_u8(s + 4);
          int16x8_t d0 = convolve12_8_2d_h(s0[0], s0[1], x_filter, permute_tbl,
                                           horiz_const);
          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--h != 0);
    }
  }
}

#elif AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD)

static INLINE int16x4_t convolve12_4_2d_h(uint8x16_t samples,
                                          const int8x16_t filters,
                                          const int32x4_t correction,
                                          const uint8x16_t range_limit,
                                          const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum;

  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  // Accumulate dot product into 'correction' to account for range clamp.
  // First 4 output values.
  sum = vdotq_laneq_s32(correction, permuted_samples[0], filters, 0);
  sum = vdotq_laneq_s32(sum, permuted_samples[1], filters, 1);
  sum = vdotq_laneq_s32(sum, permuted_samples[2], filters, 2);

  // Narrow and re-pack.
  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE int16x8_t convolve12_8_2d_h(uint8x16_t samples0,
                                          uint8x16_t samples1,
                                          const int8x16_t filters,
                                          const int32x4_t correction,
                                          const uint8x16_t range_limit,
                                          const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples[2], permuted_samples[4];
  int32x4_t sum[2];

  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  clamped_samples[0] = vreinterpretq_s8_u8(vsubq_u8(samples0, range_limit));
  clamped_samples[1] = vreinterpretq_s8_u8(vsubq_u8(samples1, range_limit));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_s8(clamped_samples[0], permute_tbl.val[2]);
  // {12, 13, 14, 15, 13, 14, 15, 16, 14, 15, 16, 17, 15, 16, 17, 18 }
  permuted_samples[3] = vqtbl1q_s8(clamped_samples[1], permute_tbl.val[2]);

  // Accumulate dot product into 'correction' to account for range clamp.
  // First 4 output values.
  sum[0] = vdotq_laneq_s32(correction, permuted_samples[0], filters, 0);
  sum[0] = vdotq_laneq_s32(sum[0], permuted_samples[1], filters, 1);
  sum[0] = vdotq_laneq_s32(sum[0], permuted_samples[2], filters, 2);
  // Second 4 output values.
  sum[1] = vdotq_laneq_s32(correction, permuted_samples[1], filters, 0);
  sum[1] = vdotq_laneq_s32(sum[1], permuted_samples[2], filters, 1);
  sum[1] = vdotq_laneq_s32(sum[1], permuted_samples[3], filters, 2);

  // Narrow and re-pack.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS),
                      vshrn_n_s32(sum[1], ROUND0_BITS));
}

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;

  // Special case the following no-op filter as 128 won't fit into the 8-bit
  // signed dot-product instruction:
  // { 0, 0, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0 }
  if (vgetq_lane_s16(x_filter_0_7, 5) == 128) {
    const uint16x8_t horiz_const = vdupq_n_u16((1 << (bd - 1)));
    // Undo the horizontal offset in the calling function.
    src_ptr += 5;

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x8_t s0 = vld1_u8(s);
        uint16x8_t d0 = vaddw_u8(horiz_const, s0);
        d0 = vshlq_n_u16(d0, FILTER_BITS - ROUND0_BITS);
        // Store 8 elements to avoid additional branches. This is safe if the
        // actual block width is < 8 because the intermediate buffer is large
        // enough to accommodate 128x128 blocks.
        vst1q_s16(d, vreinterpretq_s16_u16(d0));

        d += 8;
        s += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--h != 0);

  } else {
    // Narrow filter values to 8-bit.
    const int16x8x2_t x_filter_s16 = {
      { x_filter_0_7, vcombine_s16(x_filter_8_11, vdup_n_s16(0)) }
    };
    const int8x16_t x_filter = vcombine_s8(vmovn_s16(x_filter_s16.val[0]),
                                           vmovn_s16(x_filter_s16.val[1]));

    // This shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts
    // - which are generally faster than rounding shifts on modern CPUs.
    const int32_t horiz_const =
        ((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));
    // Dot product constants.
    const int32x4_t correct_tmp =
        vaddq_s32(vpaddlq_s16(vshlq_n_s16(x_filter_s16.val[0], 7)),
                  vpaddlq_s16(vshlq_n_s16(x_filter_s16.val[1], 7)));
    const int32x4_t correction =
        vdupq_n_s32(vaddvq_s32(correct_tmp) + horiz_const);
    const uint8x16_t range_limit = vdupq_n_u8(128);
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);

    if (w <= 4) {
      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

        int16x4_t d0 = convolve12_4_2d_h(s0, x_filter, correction, range_limit,
                                         permute_tbl);
        int16x4_t d1 = convolve12_4_2d_h(s1, x_filter, correction, range_limit,
                                         permute_tbl);
        int16x4_t d2 = convolve12_4_2d_h(s2, x_filter, correction, range_limit,
                                         permute_tbl);
        int16x4_t d3 = convolve12_4_2d_h(s3, x_filter, correction, range_limit,
                                         permute_tbl);

        // Store 4 elements per row to avoid additional branches. (Safe.)
        store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h > 4);

      do {
        uint8x16_t s0 = vld1q_u8(src_ptr);
        int16x4_t d0 = convolve12_4_2d_h(s0, x_filter, correction, range_limit,
                                         permute_tbl);
        // Store 4 elements to avoid additional branches. (Safe if w == 2.)
        vst1_s16(dst_ptr, d0);

        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--h != 0);

    } else {
      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2], s1[2], s2[2], s3[2];
          load_u8_16x4(s, src_stride, &s0[0], &s1[0], &s2[0], &s3[0]);
          load_u8_16x4(s + 4, src_stride, &s0[1], &s1[1], &s2[1], &s3[1]);

          int16x8_t d0 = convolve12_8_2d_h(s0[0], s0[1], x_filter, correction,
                                           range_limit, permute_tbl);
          int16x8_t d1 = convolve12_8_2d_h(s1[0], s1[1], x_filter, correction,
                                           range_limit, permute_tbl);
          int16x8_t d2 = convolve12_8_2d_h(s2[0], s2[1], x_filter, correction,
                                           range_limit, permute_tbl);
          int16x8_t d3 = convolve12_8_2d_h(s3[0], s3[1], x_filter, correction,
                                           range_limit, permute_tbl);

          store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src_ptr += 4 * src_stride;
        dst_ptr += 4 * dst_stride;
        h -= 4;
      } while (h > 4);

      do {
        const uint8_t *s = src_ptr;
        int16_t *d = dst_ptr;
        int width = w;

        do {
          uint8x16_t s0[2];
          s0[0] = vld1q_u8(s);
          s0[1] = vld1q_u8(s + 4);
          int16x8_t d0 = convolve12_8_2d_h(s0[0], s0[1], x_filter, correction,
                                           range_limit, permute_tbl);
          vst1q_s16(d, d0);

          s += 8;
          d += 8;
          width -= 8;
        } while (width > 0);
        src_ptr += src_stride;
        dst_ptr += dst_stride;
      } while (--h != 0);
    }
  }
}

#else  // !(AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD))

static INLINE int16x4_t
convolve12_4_2d_h(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                  const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                  const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
                  const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
                  const int16x8_t x_filter_0_7, const int16x4_t x_filter_8_11,
                  const int32x4_t horiz_const) {
  const int16x4_t x_filter_0_3 = vget_low_s16(x_filter_0_7);
  const int16x4_t x_filter_4_7 = vget_high_s16(x_filter_0_7);

  int32x4_t sum = horiz_const;
  sum = vmlal_lane_s16(sum, s0, x_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, x_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, x_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, x_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, x_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, x_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, x_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, x_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, x_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, x_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, x_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, x_filter_8_11, 3);

  return vshrn_n_s32(sum, ROUND0_BITS);
}

static INLINE void convolve_2d_sr_horiz_12tap_neon(
    const uint8_t *src_ptr, int src_stride, int16_t *dst_ptr,
    const int dst_stride, int w, int h, const int16x8_t x_filter_0_7,
    const int16x4_t x_filter_8_11) {
  const int bd = 8;
  // A shim of 1 << (ROUND0_BITS - 1) enables us to use non-rounding shifts -
  // which are generally faster than rounding shifts on modern CPUs.
  const int32x4_t horiz_const =
      vdupq_n_s32((1 << (bd + FILTER_BITS - 1)) + (1 << (ROUND0_BITS - 1)));

#if AOM_ARCH_AARCH64
  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    uint8x8_t t0, t1, t2, t3;
    load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
    int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
    int16x4_t s7 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

    load_u8_8x4(s + 8, src_stride, &t0, &t1, &t2, &t3);
    transpose_u8_8x4(&t0, &t1, &t2, &t3);

    int16x4_t s8 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
    int16x4_t s9 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
    int16x4_t s10 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

    s += 11;

    do {
      load_u8_8x4(s, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      int16x4_t s11 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s12 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      int16x4_t s13 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      int16x4_t s14 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));

      int16x4_t d0 =
          convolve12_4_2d_h(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d1 =
          convolve12_4_2d_h(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d2 =
          convolve12_4_2d_h(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      int16x4_t d3 =
          convolve12_4_2d_h(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
                            x_filter_0_7, x_filter_8_11, horiz_const);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);
      // Store 4 elements per row to avoid additional branches. This is safe if
      // the actual block width is < 4 because the intermediate buffer is large
      // enough to accommodate 128x128 blocks.
      store_s16_4x4(d, dst_stride, d0, d1, d2, d3);

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);
    src_ptr += 4 * src_stride;
    dst_ptr += 4 * dst_stride;
    h -= 4;
  } while (h > 4);
#endif  // AOM_ARCH_AARCH64

  do {
    const uint8_t *s = src_ptr;
    int16_t *d = dst_ptr;
    int width = w;

    do {
      uint8x16_t t0 = vld1q_u8(s);
      int16x8_t tt0 = vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(t0)));
      int16x8_t tt1 = vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(t0)));

      int16x4_t s0 = vget_low_s16(tt0);
      int16x4_t s4 = vget_high_s16(tt0);
      int16x4_t s8 = vget_low_s16(tt1);
      int16x4_t s12 = vget_high_s16(tt1);

      int16x4_t s1 = vext_s16(s0, s4, 1);    //  a1  a2  a3  a4
      int16x4_t s2 = vext_s16(s0, s4, 2);    //  a2  a3  a4  a5
      int16x4_t s3 = vext_s16(s0, s4, 3);    //  a3  a4  a5  a6
      int16x4_t s5 = vext_s16(s4, s8, 1);    //  a5  a6  a7  a8
      int16x4_t s6 = vext_s16(s4, s8, 2);    //  a6  a7  a8  a9
      int16x4_t s7 = vext_s16(s4, s8, 3);    //  a7  a8  a9 a10
      int16x4_t s9 = vext_s16(s8, s12, 1);   //  a9 a10 a11 a12
      int16x4_t s10 = vext_s16(s8, s12, 2);  // a10 a11 a12 a13
      int16x4_t s11 = vext_s16(s8, s12, 3);  // a11 a12 a13 a14

      int16x4_t d0 =
          convolve12_4_2d_h(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                            x_filter_0_7, x_filter_8_11, horiz_const);
      // Store 4 elements to avoid additional branches. (Safe as noted above.)
      vst1_s16(d, d0);

      s += 4;
      d += 4;
      width -= 4;
    } while (width > 0);
    src_ptr += src_stride;
    dst_ptr += dst_stride;
  } while (--h != 0);
}

#endif  // AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD)

#if AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_MATMUL_INT8)

static INLINE int16x4_t convolve4_4_2d_h(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const uint8x16_t permute_tbl,
                                         const int32x4_t horiz_const) {
  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  uint8x16_t permuted_samples = vqtbl1q_u8(samples, permute_tbl);

  // First 4 output values.
  int32x4_t sum = vusdotq_lane_s32(horiz_const, permuted_samples, filters, 0);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrn_n_s32(sum, ROUND0_BITS - 1);
}

static INLINE int16x8_t convolve8_8_2d_h(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const uint8x16x3_t permute_tbl,
                                         const int32x4_t horiz_const) {
  uint8x16_t permuted_samples[3];
  int32x4_t sum[2];

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_u8(samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_u8(samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_u8(samples, permute_tbl.val[2]);

  // First 4 output values.
  sum[0] = vusdotq_lane_s32(horiz_const, permuted_samples[0], filters, 0);
  sum[0] = vusdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  // Second 4 output values.
  sum[1] = vusdotq_lane_s32(horiz_const, permuted_samples[1], filters, 0);
  sum[1] = vusdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                      vshrn_n_s32(sum[1], ROUND0_BITS - 1));
}

static INLINE void convolve_2d_sr_horiz_neon(const uint8_t *src, int src_stride,
                                             int16_t *im_block, int im_stride,
                                             int w, int im_h,
                                             const int16_t *x_filter_ptr) {
  const int bd = 8;
  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // The outermost -1 is needed because we halved the filter values.
  const int32x4_t horiz_const = vdupq_n_s32((1 << (bd + FILTER_BITS - 2)) +
                                            (1 << ((ROUND0_BITS - 1) - 1)));

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  if (w <= 4) {
    const uint8x16_t permute_tbl = vld1q_u8(dot_prod_permute_tbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 = convolve4_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      int16x4_t d1 = convolve4_4_2d_h(s1, x_filter, permute_tbl, horiz_const);
      int16x4_t d2 = convolve4_4_2d_h(s2, x_filter, permute_tbl, horiz_const);
      int16x4_t d3 = convolve4_4_2d_h(s3, x_filter, permute_tbl, horiz_const);

      // Store 4 elements per row to avoid additional branches. This is safe if
      // the actual block width is < 4 because the intermediate buffer is large
      // enough to accommodate 128x128 blocks.
      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);
      int16x4_t d0 = convolve4_4_2d_h(s0, x_filter, permute_tbl, horiz_const);
      // Store 4 elements to avoid additional branches. (Safe if w == 2.)
      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(vld1q_s16(x_filter_ptr), 1);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        int16x8_t d1 = convolve8_8_2d_h(s1, x_filter, permute_tbl, horiz_const);
        int16x8_t d2 = convolve8_8_2d_h(s2, x_filter, permute_tbl, horiz_const);
        int16x8_t d3 = convolve8_8_2d_h(s3, x_filter, permute_tbl, horiz_const);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0 = vld1q_u8(s);
        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, permute_tbl, horiz_const);
        vst1q_s16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

#elif AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD)

static INLINE int16x4_t convolve4_4_2d_h(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16_t permute_tbl) {
  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  int8x16_t clamped_samples =
      vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  int8x16_t permuted_samples = vqtbl1q_s8(clamped_samples, permute_tbl);

  // Accumulate dot product into 'correction' to account for range clamp.
  int32x4_t sum = vdotq_lane_s32(correction, permuted_samples, filters, 0);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrn_n_s32(sum, ROUND0_BITS - 1);
}

static INLINE int16x8_t convolve8_8_2d_h(uint8x16_t samples,
                                         const int8x8_t filters,
                                         const int32x4_t correction,
                                         const uint8x16_t range_limit,
                                         const uint8x16x3_t permute_tbl) {
  int8x16_t clamped_samples, permuted_samples[3];
  int32x4_t sum[2];

  // Clamp sample range to [-128, 127] for 8-bit signed dot product.
  clamped_samples = vreinterpretq_s8_u8(vsubq_u8(samples, range_limit));

  // Permute samples ready for dot product.
  // { 0,  1,  2,  3,  1,  2,  3,  4,  2,  3,  4,  5,  3,  4,  5,  6 }
  permuted_samples[0] = vqtbl1q_s8(clamped_samples, permute_tbl.val[0]);
  // { 4,  5,  6,  7,  5,  6,  7,  8,  6,  7,  8,  9,  7,  8,  9, 10 }
  permuted_samples[1] = vqtbl1q_s8(clamped_samples, permute_tbl.val[1]);
  // { 8,  9, 10, 11,  9, 10, 11, 12, 10, 11, 12, 13, 11, 12, 13, 14 }
  permuted_samples[2] = vqtbl1q_s8(clamped_samples, permute_tbl.val[2]);

  // Accumulate dot product into 'correction' to account for range clamp.
  // First 4 output values.
  sum[0] = vdotq_lane_s32(correction, permuted_samples[0], filters, 0);
  sum[0] = vdotq_lane_s32(sum[0], permuted_samples[1], filters, 1);
  // Second 4 output values.
  sum[1] = vdotq_lane_s32(correction, permuted_samples[1], filters, 0);
  sum[1] = vdotq_lane_s32(sum[1], permuted_samples[2], filters, 1);

  // Narrow and re-pack.
  // We halved the convolution filter values so -1 from the right shift.
  return vcombine_s16(vshrn_n_s32(sum[0], ROUND0_BITS - 1),
                      vshrn_n_s32(sum[1], ROUND0_BITS - 1));
}

static INLINE void convolve_2d_sr_horiz_neon(const uint8_t *src, int src_stride,
                                             int16_t *im_block, int im_stride,
                                             int w, int im_h,
                                             const int16_t *x_filter_ptr) {
  const int bd = 8;
  // This shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
  // shifts - which are generally faster than rounding shifts on modern CPUs.
  // The outermost -1 is needed because we halved the filter values.
  const int32_t horiz_const =
      ((1 << (bd + FILTER_BITS - 2)) + (1 << ((ROUND0_BITS - 1) - 1)));
  // Dot product constants.
  const int16x8_t x_filter_s16 = vld1q_s16(x_filter_ptr);
  const int32_t correction_s32 =
      vaddlvq_s16(vshlq_n_s16(x_filter_s16, FILTER_BITS - 1));
  const int32x4_t correction = vdupq_n_s32(correction_s32 + horiz_const);
  const uint8x16_t range_limit = vdupq_n_u8(128);

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  if (w <= 4) {
    const uint8x16_t permute_tbl = vld1q_u8(dot_prod_permute_tbl);
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter =
        vshrn_n_s16(vcombine_s16(vld1_s16(x_filter_ptr + 2), vdup_n_s16(0)), 1);

    src_ptr += 2;

    do {
      uint8x16_t s0, s1, s2, s3;
      load_u8_16x4(src_ptr, src_stride, &s0, &s1, &s2, &s3);

      int16x4_t d0 =
          convolve4_4_2d_h(s0, x_filter, correction, range_limit, permute_tbl);
      int16x4_t d1 =
          convolve4_4_2d_h(s1, x_filter, correction, range_limit, permute_tbl);
      int16x4_t d2 =
          convolve4_4_2d_h(s2, x_filter, correction, range_limit, permute_tbl);
      int16x4_t d3 =
          convolve4_4_2d_h(s3, x_filter, correction, range_limit, permute_tbl);

      // Store 4 elements per row to avoid additional branches. This is safe if
      // the actual block width is < 4 because the intermediate buffer is large
      // enough to accommodate 128x128 blocks.
      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);

    do {
      uint8x16_t s0 = vld1q_u8(src_ptr);
      int16x4_t d0 =
          convolve4_4_2d_h(s0, x_filter, correction, range_limit, permute_tbl);
      // Store 4 elements to avoid additional branches. (Safe if w == 2.)
      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  } else {
    const uint8x16x3_t permute_tbl = vld1q_u8_x3(dot_prod_permute_tbl);
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int8x8_t x_filter = vshrn_n_s16(x_filter_s16, 1);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0, s1, s2, s3;
        load_u8_16x4(s, src_stride, &s0, &s1, &s2, &s3);

        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, correction, range_limit,
                                        permute_tbl);
        int16x8_t d1 = convolve8_8_2d_h(s1, x_filter, correction, range_limit,
                                        permute_tbl);
        int16x8_t d2 = convolve8_8_2d_h(s2, x_filter, correction, range_limit,
                                        permute_tbl);
        int16x8_t d3 = convolve8_8_2d_h(s3, x_filter, correction, range_limit,
                                        permute_tbl);

        store_s16_8x4(d, dst_stride, d0, d1, d2, d3);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height >= 4);

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      do {
        uint8x16_t s0 = vld1q_u8(s);
        int16x8_t d0 = convolve8_8_2d_h(s0, x_filter, correction, range_limit,
                                        permute_tbl);
        vst1q_s16(d, d0);

        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

#else  // !(AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD))

static INLINE int16x4_t convolve4_4_2d_h(const int16x4_t s0, const int16x4_t s1,
                                         const int16x4_t s2, const int16x4_t s3,
                                         const int16x4_t filter,
                                         const int16x4_t horiz_const) {
  int16x4_t sum = horiz_const;
  sum = vmla_lane_s16(sum, s0, filter, 0);
  sum = vmla_lane_s16(sum, s1, filter, 1);
  sum = vmla_lane_s16(sum, s2, filter, 2);
  sum = vmla_lane_s16(sum, s3, filter, 3);

  // We halved the convolution filter values so -1 from the right shift.
  return vshr_n_s16(sum, ROUND0_BITS - 1);
}

static INLINE int16x8_t convolve8_8_2d_h(const int16x8_t s0, const int16x8_t s1,
                                         const int16x8_t s2, const int16x8_t s3,
                                         const int16x8_t s4, const int16x8_t s5,
                                         const int16x8_t s6, const int16x8_t s7,
                                         const int16x8_t filter,
                                         const int16x8_t horiz_const) {
  const int16x4_t filter_lo = vget_low_s16(filter);
  const int16x4_t filter_hi = vget_high_s16(filter);

  int16x8_t sum = horiz_const;
  sum = vmlaq_lane_s16(sum, s0, filter_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filter_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filter_lo, 2);
  sum = vmlaq_lane_s16(sum, s3, filter_lo, 3);
  sum = vmlaq_lane_s16(sum, s4, filter_hi, 0);
  sum = vmlaq_lane_s16(sum, s5, filter_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filter_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filter_hi, 3);

  // We halved the convolution filter values so -1 from the right shift.
  return vshrq_n_s16(sum, ROUND0_BITS - 1);
}

static INLINE void convolve_2d_sr_horiz_neon(const uint8_t *src, int src_stride,
                                             int16_t *im_block, int im_stride,
                                             int w, int im_h,
                                             const int16_t *x_filter_ptr) {
  const int bd = 8;

  const uint8_t *src_ptr = src;
  int16_t *dst_ptr = im_block;
  int dst_stride = im_stride;
  int height = im_h;

  if (w <= 4) {
    // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
    // shifts - which are generally faster than rounding shifts on modern CPUs.
    // (The extra -1 is needed because we halved the filter values.)
    const int16x4_t horiz_const = vdup_n_s16((1 << (bd + FILTER_BITS - 2)) +
                                             (1 << ((ROUND0_BITS - 1) - 1)));
    // 4-tap filters are used for blocks having width <= 4.
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16x4_t x_filter = vshr_n_s16(vld1_s16(x_filter_ptr + 2), 1);

    src_ptr += 2;

#if AOM_ARCH_AARCH64
    do {
      uint8x8_t t0, t1, t2, t3;
      load_u8_8x4(src_ptr, src_stride, &t0, &t1, &t2, &t3);
      transpose_u8_8x4(&t0, &t1, &t2, &t3);

      int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s1 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      int16x4_t s2 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));
      int16x4_t s3 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t3)));
      int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s5 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t1)));
      int16x4_t s6 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t2)));

      int16x4_t d0 = convolve4_4_2d_h(s0, s1, s2, s3, x_filter, horiz_const);
      int16x4_t d1 = convolve4_4_2d_h(s1, s2, s3, s4, x_filter, horiz_const);
      int16x4_t d2 = convolve4_4_2d_h(s2, s3, s4, s5, x_filter, horiz_const);
      int16x4_t d3 = convolve4_4_2d_h(s3, s4, s5, s6, x_filter, horiz_const);

      transpose_s16_4x4d(&d0, &d1, &d2, &d3);
      // Store 4 elements per row to avoid additional branches. This is safe if
      // the actual block width is < 4 because the intermediate buffer is large
      // enough to accommodate 128x128 blocks.
      store_s16_4x4(dst_ptr, dst_stride, d0, d1, d2, d3);

      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      height -= 4;
    } while (height > 4);
#endif  // AOM_ARCH_AARCH64

    do {
      uint8x8_t t0 = vld1_u8(src_ptr);  // a0 a1 a2 a3 a4 a5 a6 a7
      int16x4_t s0 = vget_low_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));
      int16x4_t s4 = vget_high_s16(vreinterpretq_s16_u16(vmovl_u8(t0)));

      int16x4_t s1 = vext_s16(s0, s4, 1);  // a1 a2 a3 a4
      int16x4_t s2 = vext_s16(s0, s4, 2);  // a2 a3 a4 a5
      int16x4_t s3 = vext_s16(s0, s4, 3);  // a3 a4 a5 a6

      int16x4_t d0 = convolve4_4_2d_h(s0, s1, s2, s3, x_filter, horiz_const);
      // Store 4 elements to avoid additional branches. (Safe if w == 2.)
      vst1_s16(dst_ptr, d0);

      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  } else {
    // A shim of 1 << ((ROUND0_BITS - 1) - 1) enables us to use non-rounding
    // shifts - which are generally faster than rounding shifts on modern CPUs.
    // (The extra -1 is needed because we halved the filter values.)
    const int16x8_t horiz_const = vdupq_n_s16((1 << (bd + FILTER_BITS - 2)) +
                                              (1 << ((ROUND0_BITS - 1) - 1)));
    // Filter values are even, so halve to reduce intermediate precision reqs.
    const int16x8_t x_filter = vshrq_n_s16(vld1q_s16(x_filter_ptr), 1);

#if AOM_ARCH_AARCH64
    while (height > 8) {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      uint8x8_t t0, t1, t2, t3, t4, t5, t6, t7;
      load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);
      transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));
      int16x8_t s1 = vreinterpretq_s16_u16(vmovl_u8(t1));
      int16x8_t s2 = vreinterpretq_s16_u16(vmovl_u8(t2));
      int16x8_t s3 = vreinterpretq_s16_u16(vmovl_u8(t3));
      int16x8_t s4 = vreinterpretq_s16_u16(vmovl_u8(t4));
      int16x8_t s5 = vreinterpretq_s16_u16(vmovl_u8(t5));
      int16x8_t s6 = vreinterpretq_s16_u16(vmovl_u8(t6));

      s += 7;

      do {
        load_u8_8x8(s, src_stride, &t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        transpose_u8_8x8(&t0, &t1, &t2, &t3, &t4, &t5, &t6, &t7);

        int16x8_t s7 = vreinterpretq_s16_u16(vmovl_u8(t0));
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t1));
        int16x8_t s9 = vreinterpretq_s16_u16(vmovl_u8(t2));
        int16x8_t s10 = vreinterpretq_s16_u16(vmovl_u8(t3));
        int16x8_t s11 = vreinterpretq_s16_u16(vmovl_u8(t4));
        int16x8_t s12 = vreinterpretq_s16_u16(vmovl_u8(t5));
        int16x8_t s13 = vreinterpretq_s16_u16(vmovl_u8(t6));
        int16x8_t s14 = vreinterpretq_s16_u16(vmovl_u8(t7));

        int16x8_t d0 = convolve8_8_2d_h(s0, s1, s2, s3, s4, s5, s6, s7,
                                        x_filter, horiz_const);
        int16x8_t d1 = convolve8_8_2d_h(s1, s2, s3, s4, s5, s6, s7, s8,
                                        x_filter, horiz_const);
        int16x8_t d2 = convolve8_8_2d_h(s2, s3, s4, s5, s6, s7, s8, s9,
                                        x_filter, horiz_const);
        int16x8_t d3 = convolve8_8_2d_h(s3, s4, s5, s6, s7, s8, s9, s10,
                                        x_filter, horiz_const);
        int16x8_t d4 = convolve8_8_2d_h(s4, s5, s6, s7, s8, s9, s10, s11,
                                        x_filter, horiz_const);
        int16x8_t d5 = convolve8_8_2d_h(s5, s6, s7, s8, s9, s10, s11, s12,
                                        x_filter, horiz_const);
        int16x8_t d6 = convolve8_8_2d_h(s6, s7, s8, s9, s10, s11, s12, s13,
                                        x_filter, horiz_const);
        int16x8_t d7 = convolve8_8_2d_h(s7, s8, s9, s10, s11, s12, s13, s14,
                                        x_filter, horiz_const);

        transpose_s16_8x8(&d0, &d1, &d2, &d3, &d4, &d5, &d6, &d7);

        store_s16_8x8(d, dst_stride, d0, d1, d2, d3, d4, d5, d6, d7);

        s0 = s8;
        s1 = s9;
        s2 = s10;
        s3 = s11;
        s4 = s12;
        s5 = s13;
        s6 = s14;
        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += 8 * src_stride;
      dst_ptr += 8 * dst_stride;
      height -= 8;
    }
#endif  // AOM_ARCH_AARCH64

    do {
      const uint8_t *s = src_ptr;
      int16_t *d = dst_ptr;
      int width = w;

      uint8x8_t t0 = vld1_u8(s);  // a0 a1 a2 a3 a4 a5 a6 a7
      int16x8_t s0 = vreinterpretq_s16_u16(vmovl_u8(t0));

      do {
        uint8x8_t t1 = vld1_u8(s + 8);  // a8 a9 a10 a11 a12 a13 a14 a15
        int16x8_t s8 = vreinterpretq_s16_u16(vmovl_u8(t1));

        int16x8_t s1 = vextq_s16(s0, s8, 1);  // a1 a2 a3 a4 a5 a6 a7 a8
        int16x8_t s2 = vextq_s16(s0, s8, 2);  // a2 a3 a4 a5 a6 a7 a8 a9
        int16x8_t s3 = vextq_s16(s0, s8, 3);  // a3 a4 a5 a6 a7 a8 a9 a10
        int16x8_t s4 = vextq_s16(s0, s8, 4);  // a4 a5 a6 a7 a8 a9 a10 a11
        int16x8_t s5 = vextq_s16(s0, s8, 5);  // a5 a6 a7 a8 a9 a10 a11 a12
        int16x8_t s6 = vextq_s16(s0, s8, 6);  // a6 a7 a8 a9 a10 a11 a12 a13
        int16x8_t s7 = vextq_s16(s0, s8, 7);  // a7 a8 a9 a10 a11 a12 a13 a14

        int16x8_t d0 = convolve8_8_2d_h(s0, s1, s2, s3, s4, s5, s6, s7,
                                        x_filter, horiz_const);

        vst1q_s16(d, d0);

        s0 = s8;
        s += 8;
        d += 8;
        width -= 8;
      } while (width > 0);
      src_ptr += src_stride;
      dst_ptr += dst_stride;
    } while (--height != 0);
  }
}

#endif  // AOM_ARCH_AARCH64 && defined(__ARM_FEATURE_DOTPROD)

static INLINE int32x4_t
convolve12_4_2d_v(const int16x4_t s0, const int16x4_t s1, const int16x4_t s2,
                  const int16x4_t s3, const int16x4_t s4, const int16x4_t s5,
                  const int16x4_t s6, const int16x4_t s7, const int16x4_t s8,
                  const int16x4_t s9, const int16x4_t s10, const int16x4_t s11,
                  const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_0_3, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_0_3, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_0_3, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_0_3, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_4_7, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_4_7, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_4_7, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_4_7, 3);
  sum = vmlal_lane_s16(sum, s8, y_filter_8_11, 0);
  sum = vmlal_lane_s16(sum, s9, y_filter_8_11, 1);
  sum = vmlal_lane_s16(sum, s10, y_filter_8_11, 2);
  sum = vmlal_lane_s16(sum, s11, y_filter_8_11, 3);

  return sum;
}

static INLINE uint8x8_t
convolve12_8_2d_v(const int16x8_t s0, const int16x8_t s1, const int16x8_t s2,
                  const int16x8_t s3, const int16x8_t s4, const int16x8_t s5,
                  const int16x8_t s6, const int16x8_t s7, const int16x8_t s8,
                  const int16x8_t s9, const int16x8_t s10, const int16x8_t s11,
                  const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11,
                  const int16x8_t sub_const) {
  const int16x4_t y_filter_0_3 = vget_low_s16(y_filter_0_7);
  const int16x4_t y_filter_4_7 = vget_high_s16(y_filter_0_7);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_0_3, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_0_3, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_0_3, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_0_3, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_4_7, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_4_7, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_4_7, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_4_7, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s8), y_filter_8_11, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s9), y_filter_8_11, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s10), y_filter_8_11, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s11), y_filter_8_11, 3);

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_0_3, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_0_3, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_0_3, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_0_3, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_4_7, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_4_7, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_4_7, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_4_7, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s8), y_filter_8_11, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s9), y_filter_8_11, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s10), y_filter_8_11, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s11), y_filter_8_11, 3);

  int16x8_t res =
      vcombine_s16(vqrshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS),
                   vqrshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS));
  res = vsubq_s16(res, sub_const);

  return vqmovun_s16(res);
}

static INLINE void convolve_2d_sr_vert_12tap_neon(
    int16_t *src_ptr, int src_stride, uint8_t *dst_ptr, int dst_stride, int w,
    int h, const int16x8_t y_filter_0_7, const int16x4_t y_filter_8_11) {
  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
    load_s16_4x11(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7,
                  &s8, &s9, &s10);
    src_ptr += 11 * src_stride;

    do {
      int16x4_t s11, s12, s13, s14;
      load_s16_4x4(src_ptr, src_stride, &s11, &s12, &s13, &s14);

      int32x4_t d0 = convolve12_4_2d_v(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9,
                                       s10, s11, y_filter_0_7, y_filter_8_11);
      int32x4_t d1 = convolve12_4_2d_v(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10,
                                       s11, s12, y_filter_0_7, y_filter_8_11);
      int32x4_t d2 = convolve12_4_2d_v(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                                       s12, s13, y_filter_0_7, y_filter_8_11);
      int32x4_t d3 =
          convolve12_4_2d_v(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13, s14,
                            y_filter_0_7, y_filter_8_11);

      int16x8_t dd01 =
          vcombine_s16(vqrshrn_n_s32(d0, 2 * FILTER_BITS - ROUND0_BITS),
                       vqrshrn_n_s32(d1, 2 * FILTER_BITS - ROUND0_BITS));
      int16x8_t dd23 =
          vcombine_s16(vqrshrn_n_s32(d2, 2 * FILTER_BITS - ROUND0_BITS),
                       vqrshrn_n_s32(d3, 2 * FILTER_BITS - ROUND0_BITS));

      dd01 = vsubq_s16(dd01, sub_const);
      dd23 = vsubq_s16(dd23, sub_const);

      uint8x8_t d01 = vqmovun_s16(dd01);
      uint8x8_t d23 = vqmovun_s16(dd23);

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      s7 = s11;
      s8 = s12;
      s9 = s13;
      s10 = s14;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
    } while (h > 0);

  } else {
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10;
      load_s16_8x11(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6, &s7, &s8,
                    &s9, &s10);
      s += 11 * src_stride;

      do {
        int16x8_t s11, s12, s13, s14;
        load_s16_8x4(s, src_stride, &s11, &s12, &s13, &s14);

        uint8x8_t d0 =
            convolve12_8_2d_v(s0, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11,
                              y_filter_0_7, y_filter_8_11, sub_const);
        uint8x8_t d1 =
            convolve12_8_2d_v(s1, s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                              y_filter_0_7, y_filter_8_11, sub_const);
        uint8x8_t d2 =
            convolve12_8_2d_v(s2, s3, s4, s5, s6, s7, s8, s9, s10, s11, s12,
                              s13, y_filter_0_7, y_filter_8_11, sub_const);
        uint8x8_t d3 =
            convolve12_8_2d_v(s3, s4, s5, s6, s7, s8, s9, s10, s11, s12, s13,
                              s14, y_filter_0_7, y_filter_8_11, sub_const);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s7 = s11;
        s8 = s12;
        s9 = s13;
        s10 = s14;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE int16x4_t convolve8_4_2d_v(const int16x4_t s0, const int16x4_t s1,
                                         const int16x4_t s2, const int16x4_t s3,
                                         const int16x4_t s4, const int16x4_t s5,
                                         const int16x4_t s6, const int16x4_t s7,
                                         const int16x8_t y_filter) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_lo, 0);
  sum = vmlal_lane_s16(sum, s1, y_filter_lo, 1);
  sum = vmlal_lane_s16(sum, s2, y_filter_lo, 2);
  sum = vmlal_lane_s16(sum, s3, y_filter_lo, 3);
  sum = vmlal_lane_s16(sum, s4, y_filter_hi, 0);
  sum = vmlal_lane_s16(sum, s5, y_filter_hi, 1);
  sum = vmlal_lane_s16(sum, s6, y_filter_hi, 2);
  sum = vmlal_lane_s16(sum, s7, y_filter_hi, 3);

  return vqrshrn_n_s32(sum, 2 * FILTER_BITS - ROUND0_BITS);
}

static INLINE uint8x8_t convolve8_8_2d_v(const int16x8_t s0, const int16x8_t s1,
                                         const int16x8_t s2, const int16x8_t s3,
                                         const int16x8_t s4, const int16x8_t s5,
                                         const int16x8_t s6, const int16x8_t s7,
                                         const int16x8_t y_filter,
                                         const int16x8_t sub_const) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_lo, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s6), y_filter_hi, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s7), y_filter_hi, 3);

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_lo, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s6), y_filter_hi, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s7), y_filter_hi, 3);

  int16x8_t res =
      vcombine_s16(vqrshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS),
                   vqrshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS));
  res = vsubq_s16(res, sub_const);

  return vqmovun_s16(res);
}

static INLINE void convolve_2d_sr_vert_8tap_neon(int16_t *src_ptr,
                                                 int src_stride,
                                                 uint8_t *dst_ptr,
                                                 int dst_stride, int w, int h,
                                                 const int16x8_t y_filter) {
  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4, s5, s6;
    load_s16_4x7(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
    src_ptr += 7 * src_stride;

    do {
#if AOM_ARCH_AARCH64
      int16x4_t s7, s8, s9, s10;
      load_s16_4x4(src_ptr, src_stride, &s7, &s8, &s9, &s10);

      int16x4_t d0 = convolve8_4_2d_v(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      int16x4_t d1 = convolve8_4_2d_v(s1, s2, s3, s4, s5, s6, s7, s8, y_filter);
      int16x4_t d2 = convolve8_4_2d_v(s2, s3, s4, s5, s6, s7, s8, s9, y_filter);
      int16x4_t d3 =
          convolve8_4_2d_v(s3, s4, s5, s6, s7, s8, s9, s10, y_filter);

      uint8x8_t d01 = vqmovun_s16(vsubq_s16(vcombine_s16(d0, d1), sub_const));
      uint8x8_t d23 = vqmovun_s16(vsubq_s16(vcombine_s16(d2, d3), sub_const));

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      s5 = s9;
      s6 = s10;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
#else   // !AOM_ARCH_AARCH64
      int16x4_t s7 = vld1_s16(src_ptr);
      int16x4_t d0 = convolve8_4_2d_v(s0, s1, s2, s3, s4, s5, s6, s7, y_filter);
      uint8x8_t d01 =
          vqmovun_s16(vsubq_s16(vcombine_s16(d0, vdup_n_s16(0)), sub_const));

      if (w == 2) {
        store_u8_2x1(dst_ptr, d01, 0);
      } else {
        store_u8_4x1(dst_ptr, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      s5 = s6;
      s6 = s7;
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      h--;
#endif  // AOM_ARCH_AARCH64
    } while (h > 0);
  } else {
    // Width is a multiple of 8 and height is a multiple of 4.
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4, s5, s6;
      load_s16_8x7(s, src_stride, &s0, &s1, &s2, &s3, &s4, &s5, &s6);
      s += 7 * src_stride;

      do {
#if AOM_ARCH_AARCH64
        int16x8_t s7, s8, s9, s10;
        load_s16_8x4(s, src_stride, &s7, &s8, &s9, &s10);

        uint8x8_t d0 = convolve8_8_2d_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                        y_filter, sub_const);
        uint8x8_t d1 = convolve8_8_2d_v(s1, s2, s3, s4, s5, s6, s7, s8,
                                        y_filter, sub_const);
        uint8x8_t d2 = convolve8_8_2d_v(s2, s3, s4, s5, s6, s7, s8, s9,
                                        y_filter, sub_const);
        uint8x8_t d3 = convolve8_8_2d_v(s3, s4, s5, s6, s7, s8, s9, s10,
                                        y_filter, sub_const);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s5 = s9;
        s6 = s10;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !AOM_ARCH_AARCH64
        int16x8_t s7 = vld1q_s16(s);
        uint8x8_t d0 = convolve8_8_2d_v(s0, s1, s2, s3, s4, s5, s6, s7,
                                        y_filter, sub_const);
        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s5 = s6;
        s6 = s7;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // AOM_ARCH_AARCH64
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

static INLINE int16x4_t convolve6_4_2d_v(const int16x4_t s0, const int16x4_t s1,
                                         const int16x4_t s2, const int16x4_t s3,
                                         const int16x4_t s4, const int16x4_t s5,
                                         const int16x8_t y_filter) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum = vmull_lane_s16(s0, y_filter_lo, 1);
  sum = vmlal_lane_s16(sum, s1, y_filter_lo, 2);
  sum = vmlal_lane_s16(sum, s2, y_filter_lo, 3);
  sum = vmlal_lane_s16(sum, s3, y_filter_hi, 0);
  sum = vmlal_lane_s16(sum, s4, y_filter_hi, 1);
  sum = vmlal_lane_s16(sum, s5, y_filter_hi, 2);

  return vqrshrn_n_s32(sum, 2 * FILTER_BITS - ROUND0_BITS);
}

static INLINE uint8x8_t convolve6_8_2d_v(const int16x8_t s0, const int16x8_t s1,
                                         const int16x8_t s2, const int16x8_t s3,
                                         const int16x8_t s4, const int16x8_t s5,
                                         const int16x8_t y_filter,
                                         const int16x8_t sub_const) {
  const int16x4_t y_filter_lo = vget_low_s16(y_filter);
  const int16x4_t y_filter_hi = vget_high_s16(y_filter);

  int32x4_t sum0 = vmull_lane_s16(vget_low_s16(s0), y_filter_lo, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s1), y_filter_lo, 2);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s2), y_filter_lo, 3);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s3), y_filter_hi, 0);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s4), y_filter_hi, 1);
  sum0 = vmlal_lane_s16(sum0, vget_low_s16(s5), y_filter_hi, 2);

  int32x4_t sum1 = vmull_lane_s16(vget_high_s16(s0), y_filter_lo, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s1), y_filter_lo, 2);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s2), y_filter_lo, 3);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s3), y_filter_hi, 0);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s4), y_filter_hi, 1);
  sum1 = vmlal_lane_s16(sum1, vget_high_s16(s5), y_filter_hi, 2);

  int16x8_t res =
      vcombine_s16(vqrshrn_n_s32(sum0, 2 * FILTER_BITS - ROUND0_BITS),
                   vqrshrn_n_s32(sum1, 2 * FILTER_BITS - ROUND0_BITS));
  res = vsubq_s16(res, sub_const);

  return vqmovun_s16(res);
}

static INLINE void convolve_2d_sr_vert_6tap_neon(int16_t *src_ptr,
                                                 int src_stride,
                                                 uint8_t *dst_ptr,
                                                 int dst_stride, int w, int h,
                                                 const int16x8_t y_filter) {
  const int bd = 8;
  const int16x8_t sub_const = vdupq_n_s16(1 << (bd - 1));

  if (w <= 4) {
    int16x4_t s0, s1, s2, s3, s4;
    load_s16_4x5(src_ptr, src_stride, &s0, &s1, &s2, &s3, &s4);
    src_ptr += 5 * src_stride;

    do {
#if AOM_ARCH_AARCH64
      int16x4_t s5, s6, s7, s8;
      load_s16_4x4(src_ptr, src_stride, &s5, &s6, &s7, &s8);

      int16x4_t d0 = convolve6_4_2d_v(s0, s1, s2, s3, s4, s5, y_filter);
      int16x4_t d1 = convolve6_4_2d_v(s1, s2, s3, s4, s5, s6, y_filter);
      int16x4_t d2 = convolve6_4_2d_v(s2, s3, s4, s5, s6, s7, y_filter);
      int16x4_t d3 = convolve6_4_2d_v(s3, s4, s5, s6, s7, s8, y_filter);

      uint8x8_t d01 = vqmovun_s16(vsubq_s16(vcombine_s16(d0, d1), sub_const));
      uint8x8_t d23 = vqmovun_s16(vsubq_s16(vcombine_s16(d2, d3), sub_const));

      if (w == 2) {
        store_u8_2x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_2x1(dst_ptr + 1 * dst_stride, d01, 2);
        if (h != 2) {
          store_u8_2x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_2x1(dst_ptr + 3 * dst_stride, d23, 2);
        }
      } else {
        store_u8_4x1(dst_ptr + 0 * dst_stride, d01, 0);
        store_u8_4x1(dst_ptr + 1 * dst_stride, d01, 1);
        if (h != 2) {
          store_u8_4x1(dst_ptr + 2 * dst_stride, d23, 0);
          store_u8_4x1(dst_ptr + 3 * dst_stride, d23, 1);
        }
      }

      s0 = s4;
      s1 = s5;
      s2 = s6;
      s3 = s7;
      s4 = s8;
      src_ptr += 4 * src_stride;
      dst_ptr += 4 * dst_stride;
      h -= 4;
#else   // !AOM_ARCH_AARCH64
      int16x4_t s5 = vld1_s16(src_ptr);
      int16x4_t d0 = convolve6_4_2d_v(s0, s1, s2, s3, s4, s5, y_filter);
      uint8x8_t d01 =
          vqmovun_s16(vsubq_s16(vcombine_s16(d0, vdup_n_s16(0)), sub_const));

      if (w == 2) {
        store_u8_2x1(dst_ptr, d01, 0);
      } else {
        store_u8_4x1(dst_ptr, d01, 0);
      }

      s0 = s1;
      s1 = s2;
      s2 = s3;
      s3 = s4;
      s4 = s5;
      src_ptr += src_stride;
      dst_ptr += dst_stride;
      h--;
#endif  // AOM_ARCH_AARCH64
    } while (h > 0);
  } else {
    // Width is a multiple of 8 and height is a multiple of 4.
    do {
      int height = h;
      int16_t *s = src_ptr;
      uint8_t *d = dst_ptr;

      int16x8_t s0, s1, s2, s3, s4;
      load_s16_8x5(s, src_stride, &s0, &s1, &s2, &s3, &s4);
      s += 5 * src_stride;

      do {
#if AOM_ARCH_AARCH64
        int16x8_t s5, s6, s7, s8;
        load_s16_8x4(s, src_stride, &s5, &s6, &s7, &s8);

        uint8x8_t d0 =
            convolve6_8_2d_v(s0, s1, s2, s3, s4, s5, y_filter, sub_const);
        uint8x8_t d1 =
            convolve6_8_2d_v(s1, s2, s3, s4, s5, s6, y_filter, sub_const);
        uint8x8_t d2 =
            convolve6_8_2d_v(s2, s3, s4, s5, s6, s7, y_filter, sub_const);
        uint8x8_t d3 =
            convolve6_8_2d_v(s3, s4, s5, s6, s7, s8, y_filter, sub_const);

        if (h != 2) {
          store_u8_8x4(d, dst_stride, d0, d1, d2, d3);
        } else {
          store_u8_8x2(d, dst_stride, d0, d1);
        }

        s0 = s4;
        s1 = s5;
        s2 = s6;
        s3 = s7;
        s4 = s8;
        s += 4 * src_stride;
        d += 4 * dst_stride;
        height -= 4;
#else   // !AOM_ARCH_AARCH64
        int16x8_t s5 = vld1q_s16(s);
        uint8x8_t d0 =
            convolve6_8_2d_v(s0, s1, s2, s3, s4, s5, y_filter, sub_const);
        vst1_u8(d, d0);

        s0 = s1;
        s1 = s2;
        s2 = s3;
        s3 = s4;
        s4 = s5;
        s += src_stride;
        d += dst_stride;
        height--;
#endif  // AOM_ARCH_AARCH64
      } while (height > 0);
      src_ptr += 8;
      dst_ptr += 8;
      w -= 8;
    } while (w > 0);
  }
}

void av1_convolve_2d_sr_neon(const uint8_t *src, int src_stride, uint8_t *dst,
                             int dst_stride, int w, int h,
                             const InterpFilterParams *filter_params_x,
                             const InterpFilterParams *filter_params_y,
                             const int subpel_x_qn, const int subpel_y_qn,
                             ConvolveParams *conv_params) {
  (void)conv_params;
  const int y_filter_taps = get_filter_tap(filter_params_y, subpel_y_qn);
  const int clamped_y_taps = y_filter_taps < 6 ? 6 : y_filter_taps;
  const int im_h = h + clamped_y_taps - 1;
  const int im_stride = MAX_SB_SIZE;
  const int vert_offset = clamped_y_taps / 2 - 1;
  const int horiz_offset = filter_params_x->taps / 2 - 1;
  const uint8_t *src_ptr = src - vert_offset * src_stride - horiz_offset;

  const int16_t *x_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_x, subpel_x_qn & SUBPEL_MASK);
  const int16_t *y_filter_ptr = av1_get_interp_filter_subpel_kernel(
      filter_params_y, subpel_y_qn & SUBPEL_MASK);

  if (filter_params_x->taps > 8) {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + MAX_FILTER_TAP - 1) * MAX_SB_SIZE]);

    const int16x8_t x_filter_0_7 = vld1q_s16(x_filter_ptr);
    const int16x4_t x_filter_8_11 = vld1_s16(x_filter_ptr + 8);
    const int16x8_t y_filter_0_7 = vld1q_s16(y_filter_ptr);
    const int16x4_t y_filter_8_11 = vld1_s16(y_filter_ptr + 8);

    convolve_2d_sr_horiz_12tap_neon(src_ptr, src_stride, im_block, im_stride, w,
                                    im_h, x_filter_0_7, x_filter_8_11);

    convolve_2d_sr_vert_12tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                   y_filter_0_7, y_filter_8_11);
  } else {
    DECLARE_ALIGNED(16, int16_t,
                    im_block[(MAX_SB_SIZE + HORIZ_EXTRA_ROWS) * MAX_SB_SIZE]);

    convolve_2d_sr_horiz_neon(src_ptr, src_stride, im_block, im_stride, w, im_h,
                              x_filter_ptr);

    const int16x8_t y_filter = vld1q_s16(y_filter_ptr);

    if (clamped_y_taps <= 6) {
      convolve_2d_sr_vert_6tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    } else {
      convolve_2d_sr_vert_8tap_neon(im_block, im_stride, dst, dst_stride, w, h,
                                    y_filter);
    }
  }
}
