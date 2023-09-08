/*
 * Copyright (c) 2023, Alliance for Open Media. All rights reserved
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

#include "aom_dsp/arm/sum_neon.h"

// Assumes adequate padding (7 pixel to the right) of p exists at all call
// sites.
void av1_highbd_filter_intra_edge_neon(uint16_t *p, int sz, int strength) {
  if (!strength) return;

  assert(sz >= 0 && sz <= 129);
  assert(strength >= 0 && strength <= 3);

  // 141 = max sz val + left padding + right padding = 129 + 1 + 11
  uint16_t edge[141];
  memcpy(edge + 1, p, sz * sizeof(*p));

  // Populate extra space appropriately.
  edge[0] = edge[1];
  edge[sz + 1] = edge[sz];
  edge[sz + 2] = edge[sz];

  // Don't overwrite first pixel.
  uint16_t *dst = p + 1;

  if (strength == 1) {  // Filter: {4, 8, 4}.
    const uint16_t *src = edge + 1;

    while (sz > 0) {
      uint16x8_t s0 = vld1q_u16(src);
      uint16x8_t s1 = vld1q_u16(src + 1);
      uint16x8_t s2 = vld1q_u16(src + 2);

      // Make use of the identity:
      // (4*a + 8*b + 4*c) >> 4 == (a + (b << 1) + c) >> 2
      uint16x8_t t0 = vaddq_u16(s0, s2);
      uint16x8_t t1 = vaddq_u16(s1, s1);
      uint16x8_t sum = vaddq_u16(t0, t1);
      uint16x8_t res = vrshrq_n_u16(sum, 2);

      vst1q_u16(dst, res);

      src += 8;
      dst += 8;
      sz -= 8;
    }
  } else if (strength == 2) {  // Filter: {5, 6, 5}.
    const uint16_t *src = edge + 1;

    const uint16x8x3_t filter = { { vdupq_n_u16(5), vdupq_n_u16(6),
                                    vdupq_n_u16(5) } };
    while (sz > 0) {
      uint16x8_t s0 = vld1q_u16(src);
      uint16x8_t s1 = vld1q_u16(src + 1);
      uint16x8_t s2 = vld1q_u16(src + 2);

      uint16x8_t accum = vmulq_u16(s0, filter.val[0]);
      accum = vmlaq_u16(accum, s1, filter.val[1]);
      accum = vmlaq_u16(accum, s2, filter.val[2]);
      uint16x8_t res = vrshrq_n_u16(accum, 4);

      vst1q_u16(dst, res);

      src += 8;
      dst += 8;
      sz -= 8;
    }
  } else {  // Filter {2, 4, 4, 4, 2}.
    const uint16_t *src = edge;

    while (sz > 0) {
      uint16x8_t s0 = vld1q_u16(src);
      uint16x8_t s1 = vld1q_u16(src + 1);
      uint16x8_t s2 = vld1q_u16(src + 2);
      uint16x8_t s3 = vld1q_u16(src + 3);
      uint16x8_t s4 = vld1q_u16(src + 4);

      // Make use of the identity:
      // (2*a + 4*b + 4*c + 4*d + 2*e) >> 4 == (a + ((b + c + d) << 1) + e) >> 3
      uint16x8_t t0 = vaddq_u16(s0, s4);
      uint16x8_t t1 = vaddq_u16(s1, s2);
      t1 = vaddq_u16(t1, s3);
      t1 = vaddq_u16(t1, t1);
      uint16x8_t sum = vaddq_u16(t0, t1);
      uint16x8_t res = vrshrq_n_u16(sum, 3);

      vst1q_u16(dst, res);

      src += 8;
      dst += 8;
      sz -= 8;
    }
  }
}
