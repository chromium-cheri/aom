/*
 *  Copyright (c) 2024, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef AOM_AOM_DSP_ARM_AOM_CONVOLVE8_NEON_H_
#define AOM_AOM_DSP_ARM_AOM_CONVOLVE8_NEON_H_

#include <arm_neon.h>

#include "config/aom_config.h"
#include "aom_dsp/arm/mem_neon.h"

static INLINE void convolve8_horiz_2tap_neon(const uint8_t *src,
                                             ptrdiff_t src_stride, uint8_t *dst,
                                             ptrdiff_t dst_stride,
                                             const int16_t *filter_x, int w,
                                             int h) {
  // Bilinear filter values are all positive.
  const uint8x8_t f0 = vdup_n_u8((uint8_t)filter_x[3]);
  const uint8x8_t f1 = vdup_n_u8((uint8_t)filter_x[4]);

  if (w == 4) {
    do {
      uint8x8_t s0 =
          load_unaligned_u8(src + 0 * src_stride + 0, (int)src_stride);
      uint8x8_t s1 =
          load_unaligned_u8(src + 0 * src_stride + 1, (int)src_stride);
      uint8x8_t s2 =
          load_unaligned_u8(src + 2 * src_stride + 0, (int)src_stride);
      uint8x8_t s3 =
          load_unaligned_u8(src + 2 * src_stride + 1, (int)src_stride);

      uint16x8_t sum0 = vmull_u8(s0, f0);
      sum0 = vmlal_u8(sum0, s1, f1);
      uint16x8_t sum1 = vmull_u8(s2, f0);
      sum1 = vmlal_u8(sum1, s3, f1);

      uint8x8_t d0 = vqrshrn_n_u16(sum0, FILTER_BITS);
      uint8x8_t d1 = vqrshrn_n_u16(sum1, FILTER_BITS);

      store_u8x4_strided_x2(dst + 0 * dst_stride, dst_stride, d0);
      store_u8x4_strided_x2(dst + 2 * dst_stride, dst_stride, d1);

      src += 4 * src_stride;
      dst += 4 * dst_stride;
      h -= 4;
    } while (h > 0);
  } else if (w == 8) {
    do {
      uint8x8_t s0 = vld1_u8(src + 0 * src_stride + 0);
      uint8x8_t s1 = vld1_u8(src + 0 * src_stride + 1);
      uint8x8_t s2 = vld1_u8(src + 1 * src_stride + 0);
      uint8x8_t s3 = vld1_u8(src + 1 * src_stride + 1);

      uint16x8_t sum0 = vmull_u8(s0, f0);
      sum0 = vmlal_u8(sum0, s1, f1);
      uint16x8_t sum1 = vmull_u8(s2, f0);
      sum1 = vmlal_u8(sum1, s3, f1);

      uint8x8_t d0 = vqrshrn_n_u16(sum0, FILTER_BITS);
      uint8x8_t d1 = vqrshrn_n_u16(sum1, FILTER_BITS);

      vst1_u8(dst + 0 * dst_stride, d0);
      vst1_u8(dst + 1 * dst_stride, d1);

      src += 2 * src_stride;
      dst += 2 * dst_stride;
      h -= 2;
    } while (h > 0);
  } else {
    do {
      int width = w;
      const uint8_t *s = src;
      uint8_t *d = dst;

      do {
        uint8x16_t s0 = vld1q_u8(s + 0);
        uint8x16_t s1 = vld1q_u8(s + 1);

        uint16x8_t sum0 = vmull_u8(vget_low_u8(s0), f0);
        sum0 = vmlal_u8(sum0, vget_low_u8(s1), f1);
        uint16x8_t sum1 = vmull_u8(vget_high_u8(s0), f0);
        sum1 = vmlal_u8(sum1, vget_high_u8(s1), f1);

        uint8x8_t d0 = vqrshrn_n_u16(sum0, FILTER_BITS);
        uint8x8_t d1 = vqrshrn_n_u16(sum1, FILTER_BITS);

        vst1q_u8(d, vcombine_u8(d0, d1));

        s += 16;
        d += 16;
        width -= 16;
      } while (width != 0);
      src += src_stride;
      dst += dst_stride;
    } while (--h > 0);
  }
}

#endif  // AOM_AOM_DSP_ARM_AOM_CONVOLVE8_NEON_H_
