/*
 *
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
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

#include "av1/common/resize.h"
#include "av1/common/arm/mem_neon.h"
#include "av1/common/arm/transpose_neon.h"
#include "config/av1_rtcd.h"
#include "config/aom_scale_rtcd.h"

static INLINE uint8x8_t convolve8_8(const int16x8_t s0, const int16x8_t s1,
                                    const int16x8_t s2, const int16x8_t s3,
                                    const int16x8_t s4, const int16x8_t s5,
                                    const int16x8_t s6, const int16x8_t s7,
                                    const int16x8_t filters,
                                    const int16x8_t filter3,
                                    const int16x8_t filter4) {
  const int16x4_t filters_lo = vget_low_s16(filters);
  const int16x4_t filters_hi = vget_high_s16(filters);
  int16x8_t sum;

  sum = vmulq_lane_s16(s0, filters_lo, 0);
  sum = vmlaq_lane_s16(sum, s1, filters_lo, 1);
  sum = vmlaq_lane_s16(sum, s2, filters_lo, 2);
  sum = vmlaq_lane_s16(sum, s5, filters_hi, 1);
  sum = vmlaq_lane_s16(sum, s6, filters_hi, 2);
  sum = vmlaq_lane_s16(sum, s7, filters_hi, 3);
  sum = vqaddq_s16(sum, vmulq_s16(s3, filter3));
  sum = vqaddq_s16(sum, vmulq_s16(s4, filter4));
  return vqrshrun_n_s16(sum, 7);
}

static INLINE uint8x8_t scale_filter_8(const uint8x8_t *const s,
                                       const int16x8_t filters) {
  const int16x8_t filter3 = vdupq_lane_s16(vget_low_s16(filters), 3);
  const int16x8_t filter4 = vdupq_lane_s16(vget_high_s16(filters), 0);
  int16x8_t ss[8];

  ss[0] = vreinterpretq_s16_u16(vmovl_u8(s[0]));
  ss[1] = vreinterpretq_s16_u16(vmovl_u8(s[1]));
  ss[2] = vreinterpretq_s16_u16(vmovl_u8(s[2]));
  ss[3] = vreinterpretq_s16_u16(vmovl_u8(s[3]));
  ss[4] = vreinterpretq_s16_u16(vmovl_u8(s[4]));
  ss[5] = vreinterpretq_s16_u16(vmovl_u8(s[5]));
  ss[6] = vreinterpretq_s16_u16(vmovl_u8(s[6]));
  ss[7] = vreinterpretq_s16_u16(vmovl_u8(s[7]));

  return convolve8_8(ss[0], ss[1], ss[2], ss[3], ss[4], ss[5], ss[6], ss[7],
                     filters, filter3, filter4);
}

static INLINE void scale_plane_2_to_1_phase_0(const uint8_t *src,
                                              const int src_stride,
                                              uint8_t *dst,
                                              const int dst_stride, const int w,
                                              const int h) {
  const int max_width = (w + 15) & ~15;
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      const uint8x16x2_t s = vld2q_u8(src);
      vst1q_u8(dst, s.val[0]);
      src += 32;
      dst += 16;
      x -= 16;
    } while (x);
    src += 2 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static INLINE void scale_plane_4_to_1_phase_0(const uint8_t *src,
                                              const int src_stride,
                                              uint8_t *dst,
                                              const int dst_stride, const int w,
                                              const int h) {
  const int max_width = (w + 15) & ~15;
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      const uint8x16x4_t s = vld4q_u8(src);
      vst1q_u8(dst, s.val[0]);
      src += 64;
      dst += 16;
      x -= 16;
    } while (x);
    src += 4 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static INLINE void scale_plane_bilinear_kernel(
    const uint8x16_t in0, const uint8x16_t in1, const uint8x16_t in2,
    const uint8x16_t in3, const uint8x8_t coef0, const uint8x8_t coef1,
    uint8_t *const dst) {
  const uint16x8_t h0 = vmull_u8(vget_low_u8(in0), coef0);
  const uint16x8_t h1 = vmull_u8(vget_high_u8(in0), coef0);
  const uint16x8_t h2 = vmull_u8(vget_low_u8(in2), coef0);
  const uint16x8_t h3 = vmull_u8(vget_high_u8(in2), coef0);
  const uint16x8_t h4 = vmlal_u8(h0, vget_low_u8(in1), coef1);
  const uint16x8_t h5 = vmlal_u8(h1, vget_high_u8(in1), coef1);
  const uint16x8_t h6 = vmlal_u8(h2, vget_low_u8(in3), coef1);
  const uint16x8_t h7 = vmlal_u8(h3, vget_high_u8(in3), coef1);

  const uint8x8_t hor0 = vrshrn_n_u16(h4, 7);  // temp: 00 01 02 03 04 05 06 07
  const uint8x8_t hor1 = vrshrn_n_u16(h5, 7);  // temp: 08 09 0A 0B 0C 0D 0E 0F
  const uint8x8_t hor2 = vrshrn_n_u16(h6, 7);  // temp: 10 11 12 13 14 15 16 17
  const uint8x8_t hor3 = vrshrn_n_u16(h7, 7);  // temp: 18 19 1A 1B 1C 1D 1E 1F
  const uint16x8_t v0 = vmull_u8(hor0, coef0);
  const uint16x8_t v1 = vmull_u8(hor1, coef0);
  const uint16x8_t v2 = vmlal_u8(v0, hor2, coef1);
  const uint16x8_t v3 = vmlal_u8(v1, hor3, coef1);
  // dst: 0 1 2 3 4 5 6 7  8 9 A B C D E F
  const uint8x16_t d = vcombine_u8(vrshrn_n_u16(v2, 7), vrshrn_n_u16(v3, 7));
  vst1q_u8(dst, d);
}

static INLINE void scale_plane_2_to_1_bilinear(
    const uint8_t *const src, const int src_stride, uint8_t *dst,
    const int dst_stride, const int w, const int h, const int16_t c0,
    const int16_t c1) {
  const int max_width = (w + 15) & ~15;
  const uint8_t *src0 = src;
  const uint8_t *src1 = src + src_stride;
  const uint8x8_t coef0 = vdup_n_u8(c0);
  const uint8x8_t coef1 = vdup_n_u8(c1);
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      // 000 002 004 006 008 00A 00C 00E  010 012 014 016 018 01A 01C 01E
      // 001 003 005 007 009 00B 00D 00F  011 013 015 017 019 01B 01D 01F
      const uint8x16x2_t s0 = vld2q_u8(src0);
      // 100 102 104 106 108 10A 10C 10E  110 112 114 116 118 11A 11C 11E
      // 101 103 105 107 109 10B 10D 10F  111 113 115 117 119 11B 11D 11F
      const uint8x16x2_t s1 = vld2q_u8(src1);
      scale_plane_bilinear_kernel(s0.val[0], s0.val[1], s1.val[0], s1.val[1],
                                  coef0, coef1, dst);
      src0 += 32;
      src1 += 32;
      dst += 16;
      x -= 16;
    } while (x);
    src0 += 2 * (src_stride - max_width);
    src1 += 2 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static INLINE void scale_plane_4_to_1_bilinear(
    const uint8_t *const src, const int src_stride, uint8_t *dst,
    const int dst_stride, const int w, const int h, const int16_t c0,
    const int16_t c1) {
  const int max_width = (w + 15) & ~15;
  const uint8_t *src0 = src;
  const uint8_t *src1 = src + src_stride;
  const uint8x8_t coef0 = vdup_n_u8(c0);
  const uint8x8_t coef1 = vdup_n_u8(c1);
  int y = h;

  assert(w && h);

  do {
    int x = max_width;
    do {
      // (*) -- useless
      // 000 004 008 00C 010 014 018 01C  020 024 028 02C 030 034 038 03C
      // 001 005 009 00D 011 015 019 01D  021 025 029 02D 031 035 039 03D
      // 002 006 00A 00E 012 016 01A 01E  022 026 02A 02E 032 036 03A 03E (*)
      // 003 007 00B 00F 013 017 01B 01F  023 027 02B 02F 033 037 03B 03F (*)
      const uint8x16x4_t s0 = vld4q_u8(src0);
      // 100 104 108 10C 110 114 118 11C  120 124 128 12C 130 134 138 13C
      // 101 105 109 10D 111 115 119 11D  121 125 129 12D 131 135 139 13D
      // 102 106 10A 10E 112 116 11A 11E  122 126 12A 12E 132 136 13A 13E (*)
      // 103 107 10B 10F 113 117 11B 11F  123 127 12B 12F 133 137 13B 13F (*)
      const uint8x16x4_t s1 = vld4q_u8(src1);
      scale_plane_bilinear_kernel(s0.val[0], s0.val[1], s1.val[0], s1.val[1],
                                  coef0, coef1, dst);
      src0 += 64;
      src1 += 64;
      dst += 16;
      x -= 16;
    } while (x);
    src0 += 4 * (src_stride - max_width);
    src1 += 4 * (src_stride - max_width);
    dst += dst_stride - max_width;
  } while (--y);
}

static void scale_plane_2_to_1_general(const uint8_t *src, const int src_stride,
                                       uint8_t *dst, const int dst_stride,
                                       const int w, const int h,
                                       const int16_t *const coef,
                                       uint8_t *const temp_buffer) {
  const int width_hor = (w + 3) & ~3;
  const int width_ver = (w + 7) & ~7;
  const int height_hor = (2 * h + SUBPEL_TAPS - 2 + 7) & ~7;
  const int height_ver = (h + 3) & ~3;
  const int16x8_t filters = vld1q_s16(coef);
  int x, y = height_hor;
  uint8_t *t = temp_buffer;
  uint8x8_t s[14], d[4];

  assert(w && h);

  src -= (SUBPEL_TAPS / 2 - 1) * src_stride + SUBPEL_TAPS / 2 + 1;

  // horizontal 4x8
  // Note: processing 4x8 is about 20% faster than processing row by row using
  // vld4_u8().
  do {
    load_u8_8x8(src + 2, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                &s[6], &s[7]);
    transpose_u8_8x8(&s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6], &s[7]);
    x = width_hor;

    do {
      src += 8;
      load_u8_8x8(src, src_stride, &s[6], &s[7], &s[8], &s[9], &s[10], &s[11],
                  &s[12], &s[13]);
      transpose_u8_8x8(&s[6], &s[7], &s[8], &s[9], &s[10], &s[11], &s[12],
                       &s[13]);

      d[0] = scale_filter_8(&s[0], filters);  // 00 10 20 30 40 50 60 70
      d[1] = scale_filter_8(&s[2], filters);  // 01 11 21 31 41 51 61 71
      d[2] = scale_filter_8(&s[4], filters);  // 02 12 22 32 42 52 62 72
      d[3] = scale_filter_8(&s[6], filters);  // 03 13 23 33 43 53 63 73
      // 00 01 02 03 40 41 42 43
      // 10 11 12 13 50 51 52 53
      // 20 21 22 23 60 61 62 63
      // 30 31 32 33 70 71 72 73
      transpose_u8_8x4(&d[0], &d[1], &d[2], &d[3]);
      vst1_lane_u32((uint32_t *)(t + 0 * width_hor), vreinterpret_u32_u8(d[0]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 1 * width_hor), vreinterpret_u32_u8(d[1]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 2 * width_hor), vreinterpret_u32_u8(d[2]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 3 * width_hor), vreinterpret_u32_u8(d[3]),
                    0);
      vst1_lane_u32((uint32_t *)(t + 4 * width_hor), vreinterpret_u32_u8(d[0]),
                    1);
      vst1_lane_u32((uint32_t *)(t + 5 * width_hor), vreinterpret_u32_u8(d[1]),
                    1);
      vst1_lane_u32((uint32_t *)(t + 6 * width_hor), vreinterpret_u32_u8(d[2]),
                    1);
      vst1_lane_u32((uint32_t *)(t + 7 * width_hor), vreinterpret_u32_u8(d[3]),
                    1);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];
      s[4] = s[12];
      s[5] = s[13];

      t += 4;
      x -= 4;
    } while (x);
    src += 8 * src_stride - 2 * width_hor;
    t += 7 * width_hor;
    y -= 8;
  } while (y);

  // vertical 8x4
  x = width_ver;
  t = temp_buffer;
  do {
    load_u8_8x8(t, width_hor, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5], &s[6],
                &s[7]);
    t += 6 * width_hor;
    y = height_ver;

    do {
      load_u8_8x8(t, width_hor, &s[6], &s[7], &s[8], &s[9], &s[10], &s[11],
                  &s[12], &s[13]);
      t += 8 * width_hor;

      d[0] = scale_filter_8(&s[0], filters);  // 00 01 02 03 04 05 06 07
      d[1] = scale_filter_8(&s[2], filters);  // 10 11 12 13 14 15 16 17
      d[2] = scale_filter_8(&s[4], filters);  // 20 21 22 23 24 25 26 27
      d[3] = scale_filter_8(&s[6], filters);  // 30 31 32 33 34 35 36 37
      vst1_u8(dst + 0 * dst_stride, d[0]);
      vst1_u8(dst + 1 * dst_stride, d[1]);
      vst1_u8(dst + 2 * dst_stride, d[2]);
      vst1_u8(dst + 3 * dst_stride, d[3]);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];
      s[4] = s[12];
      s[5] = s[13];

      dst += 4 * dst_stride;
      y -= 4;
    } while (y);
    t -= width_hor * (2 * height_ver + 6);
    t += 8;
    dst -= height_ver * dst_stride;
    dst += 8;
    x -= 8;
  } while (x);
}

static void scale_plane_4_to_1_general(const uint8_t *src, const int src_stride,
                                       uint8_t *dst, const int dst_stride,
                                       const int w, const int h,
                                       const int16_t *const coef,
                                       uint8_t *const temp_buffer) {
  const int width_hor = (w + 1) & ~1;
  const int width_ver = (w + 7) & ~7;
  const int height_hor = (4 * h + SUBPEL_TAPS - 2 + 7) & ~7;
  const int height_ver = (h + 1) & ~1;
  const int16x8_t filters = vld1q_s16(coef);
  int x, y = height_hor;
  uint8_t *t = temp_buffer;
  uint8x8_t s[12], d[2];

  assert(w && h);

  src -= (SUBPEL_TAPS / 2 - 1) * src_stride + SUBPEL_TAPS / 2 + 3;

  // horizontal 2x8
  // Note: processing 2x8 is about 20% faster than processing row by row using
  // vld4_u8().
  do {
    load_u8_8x8(src + 4, src_stride, &s[0], &s[1], &s[2], &s[3], &s[4], &s[5],
                &s[6], &s[7]);
    transpose_u8_4x8(&s[0], &s[1], &s[2], &s[3], s[4], s[5], s[6], s[7]);
    x = width_hor;

    do {
      uint8x8x2_t dd;
      src += 8;
      load_u8_8x8(src, src_stride, &s[4], &s[5], &s[6], &s[7], &s[8], &s[9],
                  &s[10], &s[11]);
      transpose_u8_8x8(&s[4], &s[5], &s[6], &s[7], &s[8], &s[9], &s[10],
                       &s[11]);

      d[0] = scale_filter_8(&s[0], filters);  // 00 10 20 30 40 50 60 70
      d[1] = scale_filter_8(&s[4], filters);  // 01 11 21 31 41 51 61 71
      // dd.val[0]: 00 01 20 21 40 41 60 61
      // dd.val[1]: 10 11 30 31 50 51 70 71
      dd = vtrn_u8(d[0], d[1]);
      vst1_lane_u16((uint16_t *)(t + 0 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 0);
      vst1_lane_u16((uint16_t *)(t + 1 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 0);
      vst1_lane_u16((uint16_t *)(t + 2 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 1);
      vst1_lane_u16((uint16_t *)(t + 3 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 1);
      vst1_lane_u16((uint16_t *)(t + 4 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 2);
      vst1_lane_u16((uint16_t *)(t + 5 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 2);
      vst1_lane_u16((uint16_t *)(t + 6 * width_hor),
                    vreinterpret_u16_u8(dd.val[0]), 3);
      vst1_lane_u16((uint16_t *)(t + 7 * width_hor),
                    vreinterpret_u16_u8(dd.val[1]), 3);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];

      t += 2;
      x -= 2;
    } while (x);
    src += 8 * src_stride - 4 * width_hor;
    t += 7 * width_hor;
    y -= 8;
  } while (y);

  // vertical 8x2
  x = width_ver;
  t = temp_buffer;
  do {
    load_u8_8x4(t, width_hor, &s[0], &s[1], &s[2], &s[3]);
    t += 4 * width_hor;
    y = height_ver;

    do {
      load_u8_8x8(t, width_hor, &s[4], &s[5], &s[6], &s[7], &s[8], &s[9],
                  &s[10], &s[11]);
      t += 8 * width_hor;

      d[0] = scale_filter_8(&s[0], filters);  // 00 01 02 03 04 05 06 07
      d[1] = scale_filter_8(&s[4], filters);  // 10 11 12 13 14 15 16 17
      vst1_u8(dst + 0 * dst_stride, d[0]);
      vst1_u8(dst + 1 * dst_stride, d[1]);

      s[0] = s[8];
      s[1] = s[9];
      s[2] = s[10];
      s[3] = s[11];

      dst += 2 * dst_stride;
      y -= 2;
    } while (y);
    t -= width_hor * (4 * height_ver + 4);
    t += 8;
    dst -= height_ver * dst_stride;
    dst += 8;
    x -= 8;
  } while (x);
}

void av1_resize_and_extend_frame_neon(const YV12_BUFFER_CONFIG *src,
                                      YV12_BUFFER_CONFIG *dst,
                                      const InterpFilter filter,
                                      const int phase, const int num_planes) {
  // We use AOMMIN(num_planes, MAX_MB_PLANE) instead of num_planes to quiet
  // the static analysis warnings.
  for (int i = 0; i < AOMMIN(num_planes, MAX_MB_PLANE); ++i) {
    const int is_uv = i > 0;
    const int src_w = src->crop_widths[is_uv];
    const int src_h = src->crop_heights[is_uv];
    const int dst_w = dst->crop_widths[is_uv];
    const int dst_h = dst->crop_heights[is_uv];

    if (2 * dst_w == src_w && 2 * dst_h == src_h) {
      if (phase == 0) {
        scale_plane_2_to_1_phase_0(src->buffers[i], src->strides[is_uv],
                                   dst->buffers[i], dst->strides[is_uv], dst_w,
                                   dst_h);
      } else if (filter == BILINEAR) {
        const int16_t c0 = av1_bilinear_filters[phase][3];
        const int16_t c1 = av1_bilinear_filters[phase][4];
        scale_plane_2_to_1_bilinear(src->buffers[i], src->strides[is_uv],
                                    dst->buffers[i], dst->strides[is_uv], dst_w,
                                    dst_h, c0, c1);
      } else {
        const int buffer_stride = (dst_w + 3) & ~3;
        const int buffer_height = (2 * dst_h + SUBPEL_TAPS - 2 + 7) & ~7;
        uint8_t *const temp_buffer =
            (uint8_t *)malloc(buffer_stride * buffer_height);
        if (temp_buffer) {
          const InterpKernel *interp_kernel =
              (const InterpKernel *)av1_interp_filter_params_list[filter]
                  .filter_ptr;
          scale_plane_2_to_1_general(src->buffers[i], src->strides[is_uv],
                                     dst->buffers[i], dst->strides[is_uv],
                                     dst_w, dst_h, interp_kernel[phase],
                                     temp_buffer);
          free(temp_buffer);
        }
      }
    } else if (4 * dst_w == src_w && 4 * dst_h == src_h) {
      if (phase == 0) {
        scale_plane_4_to_1_phase_0(src->buffers[i], src->strides[is_uv],
                                   dst->buffers[i], dst->strides[is_uv], dst_w,
                                   dst_h);
      } else if (filter == BILINEAR) {
        const int16_t c0 = av1_bilinear_filters[phase][3];
        const int16_t c1 = av1_bilinear_filters[phase][4];
        scale_plane_4_to_1_bilinear(src->buffers[i], src->strides[is_uv],
                                    dst->buffers[i], dst->strides[is_uv], dst_w,
                                    dst_h, c0, c1);
      } else {
        const int buffer_stride = (dst_w + 1) & ~1;
        const int buffer_height = (4 * dst_h + SUBPEL_TAPS - 2 + 7) & ~7;
        uint8_t *const temp_buffer =
            (uint8_t *)malloc(buffer_stride * buffer_height);
        if (temp_buffer) {
          const InterpKernel *interp_kernel =
              (const InterpKernel *)av1_interp_filter_params_list[filter]
                  .filter_ptr;
          scale_plane_4_to_1_general(src->buffers[i], src->strides[is_uv],
                                     dst->buffers[i], dst->strides[is_uv],
                                     dst_w, dst_h, interp_kernel[phase],
                                     temp_buffer);
          free(temp_buffer);
        }
      }
    } else {
      av1_resize_plane(src->buffers[i], src_h, src_w, src->strides[is_uv],
                       dst->buffers[i], dst_h, dst_w, dst->strides[is_uv]);
    }
    aom_extend_frame_borders(dst, num_planes);
  }
}
