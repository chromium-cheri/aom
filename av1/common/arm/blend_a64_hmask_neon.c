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

#include <arm_neon.h>
#include <assert.h>

#include "aom/aom_integer.h"
#include "aom_dsp/blend.h"
#include "aom_ports/mem.h"
#include "av1/common/arm/mem_neon.h"

void aom_blend_a64_hmask_neon(uint8_t *dst, uint32_t dst_stride,
                              const uint8_t *src0, uint32_t src0_stride,
                              const uint8_t *src1, uint32_t src1_stride,
                              const uint8_t *mask, int h, int w) {
  assert(IMPLIES(src0 == dst, src0_stride == dst_stride));
  assert(IMPLIES(src1 == dst, src1_stride == dst_stride));

  assert(h >= 1);
  assert(w >= 1);
  assert(IS_POWER_OF_TWO(h));
  assert(IS_POWER_OF_TWO(w));
  uint8x8_t m, max_minus_m, tmp0, tmp1;
  uint8x16_t m_q, max_minus_m_q, tmp0_q, tmp1_q, res_q;
  uint16x8_t res, res_low, res_high;
  const uint8x8_t vdup_64 = vdup_n_u8((uint8_t)64);

  if (!(w & 0x0F)) {
    for (int i = 0; i < h; ++i) {
      int j;
      for (j = 0; j < w; j += 16) {
        const uint8x16_t vdup_64_q = vdupq_n_u8((uint8_t)64);
        tmp0_q = vld1q_u8(src0);
        tmp1_q = vld1q_u8(src1);
        m_q = vld1q_u8(mask);
        max_minus_m_q = vsubq_u8(vdup_64_q, m_q);
        res_low = vmull_u8(vget_low_u8(m_q), vget_low_u8(tmp0_q));
        res_low =
            vmlal_u8(res_low, vget_low_u8(max_minus_m_q), vget_low_u8(tmp1_q));
        res_high = vmull_u8(vget_high_u8(m_q), vget_high_u8(tmp0_q));
        res_high = vmlal_u8(res_high, vget_high_u8(max_minus_m_q),
                            vget_high_u8(tmp1_q));
        res_q = vcombine_u8(vrshrn_n_u16(res_low, AOM_BLEND_A64_ROUND_BITS),
                            vrshrn_n_u16(res_high, AOM_BLEND_A64_ROUND_BITS));
        vst1q_u8(dst, res_q);
        src0 += 16;
        src1 += 16;
        dst += 16;
        mask += 16;
      }
      src0 += src0_stride - w;
      src1 += src1_stride - w;
      dst += dst_stride - w;
      mask -= w;
    }
  } else if (!(w & 0x07)) {
    for (int i = 0; i < h; ++i) {
      tmp0 = vld1_u8(src0);
      tmp1 = vld1_u8(src1);
      m = vld1_u8(mask);
      max_minus_m = vsub_u8(vdup_64, m);
      res = vmull_u8(m, tmp0);
      res = vmlal_u8(res, max_minus_m, tmp1);
      vst1_u8(dst, vrshrn_n_u16(res, AOM_BLEND_A64_ROUND_BITS));
      src0 += src0_stride;
      src1 += src1_stride;
      dst += dst_stride;
    }
  } else if (!(w & 0x03)) {
    for (int i = 0; i < h; i += 2) {
      m = vreinterpret_u8_u32(
          vld1_lane_u32((uint32_t *)(mask), vreinterpret_u32_u8(m), 0));
      m = vreinterpret_u8_u32(
          vld1_lane_u32((uint32_t *)(mask), vreinterpret_u32_u8(m), 1));
      max_minus_m = vsub_u8(vdup_64, m);
      tmp0 = vreinterpret_u8_u32(
          vld1_lane_u32((uint32_t *)(src0 + (0 * src0_stride)),
                        vreinterpret_u32_u8(tmp0), 0));
      tmp0 = vreinterpret_u8_u32(
          vld1_lane_u32((uint32_t *)(src0 + (1 * src0_stride)),
                        vreinterpret_u32_u8(tmp0), 1));
      tmp1 = vreinterpret_u8_u32(
          vld1_lane_u32((uint32_t *)(src1 + (0 * src1_stride)),
                        vreinterpret_u32_u8(tmp1), 0));
      tmp1 = vreinterpret_u8_u32(
          vld1_lane_u32((uint32_t *)(src1 + (1 * src1_stride)),
                        vreinterpret_u32_u8(tmp1), 1));
      res = vmull_u8(m, tmp0);
      res = vmlal_u8(res, max_minus_m, tmp1);
      vst1_lane_u32(
          (uint32_t *)(dst + (0 * dst_stride)),
          vreinterpret_u32_u8(vrshrn_n_u16(res, AOM_BLEND_A64_ROUND_BITS)), 0);
      vst1_lane_u32(
          (uint32_t *)(dst + (1 * dst_stride)),
          vreinterpret_u32_u8(vrshrn_n_u16(res, AOM_BLEND_A64_ROUND_BITS)), 1);
      src0 += (2 * src0_stride);
      src1 += (2 * src1_stride);
      dst += (2 * dst_stride);
    }
  } else if (!(w & 0x01)) {
    for (int i = 0; i < h; i += 2) {
      m = vreinterpret_u8_u16(
          vld1_lane_u16((uint16_t *)(mask), vreinterpret_u16_u8(m), 0));
      m = vreinterpret_u8_u16(
          vld1_lane_u16((uint16_t *)(mask), vreinterpret_u16_u8(m), 1));
      max_minus_m = vsub_u8(vdup_64, m);
      tmp0 = vreinterpret_u8_u16(
          vld1_lane_u16((uint16_t *)(src0 + (0 * src0_stride)),
                        vreinterpret_u16_u8(tmp0), 0));
      tmp0 = vreinterpret_u8_u16(
          vld1_lane_u16((uint16_t *)(src0 + (1 * src0_stride)),
                        vreinterpret_u16_u8(tmp0), 1));
      tmp1 = vreinterpret_u8_u16(
          vld1_lane_u16((uint16_t *)(src1 + (0 * src1_stride)),
                        vreinterpret_u16_u8(tmp1), 0));
      tmp1 = vreinterpret_u8_u16(
          vld1_lane_u16((uint16_t *)(src1 + (1 * src1_stride)),
                        vreinterpret_u16_u8(tmp1), 1));
      res = vmull_u8(m, tmp0);
      res = vmlal_u8(res, max_minus_m, tmp1);
      vst1_lane_u16(
          (uint16_t *)(dst + (0 * dst_stride)),
          vreinterpret_u16_u8(vrshrn_n_u16(res, AOM_BLEND_A64_ROUND_BITS)), 0);
      vst1_lane_u16(
          (uint16_t *)(dst + (1 * dst_stride)),
          vreinterpret_u16_u8(vrshrn_n_u16(res, AOM_BLEND_A64_ROUND_BITS)), 1);
      src0 += (2 * src0_stride);
      src1 += (2 * src1_stride);
      dst += (2 * dst_stride);
    }
  } else {
    for (int i = 0; i < h; ++i) {
      dst[i * dst_stride] =
          AOM_BLEND_A64(mask[0], src0[i * src0_stride], src1[i * src1_stride]);
    }
  }
}
