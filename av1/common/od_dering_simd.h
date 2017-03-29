/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "./av1_rtcd.h"
#include "./od_dering.h"

/* partial A is a 16-bit vector of the form:
   [x8 x7 x6 x5 x4 x3 x2 x1] and partial B has the form:
   [0  y1 y2 y3 y4 y5 y6 y7].
   This function computes (x1^2+y1^2)*C1 + (x2^2+y2^2)*C2 + ...
   (x7^2+y2^7)*C7 + (x8^2+0^2)*C8 where the C1..C8 constants are in const1
   and const2. */
static INLINE v128 fold_mul_and_sum(v128 partiala, v128 partialb, v128 const1,
                                    v128 const2) {
  v128 tmp;
  /* Reverse partial B. */
  partialb = v128_shuffle_8(
      partialb, v128_from_32(0x0f0e0100, 0x03020504, 0x07060908, 0x0b0a0d0c));
  /* Interleave the x and y values of identical indices and pair x8 with 0. */
  tmp = partiala;
  partiala = v128_ziplo_16(partialb, partiala);
  partialb = v128_ziphi_16(partialb, tmp);
  /* Square and add the corresponding x and y values. */
  partiala = v128_madd_s16(partiala, partiala);
  partialb = v128_madd_s16(partialb, partialb);
  /* Multiply by constant. */
  partiala = v128_mullo_s32(partiala, const1);
  partialb = v128_mullo_s32(partialb, const2);
  /* Sum all results. */
  partiala = v128_add_32(partiala, partialb);
  return partiala;
}

static INLINE v128 hsum4(v128 x0, v128 x1, v128 x2, v128 x3) {
  v128 t0, t1, t2, t3;
  t0 = v128_ziplo_32(x1, x0);
  t1 = v128_ziplo_32(x3, x2);
  t2 = v128_ziphi_32(x1, x0);
  t3 = v128_ziphi_32(x3, x2);
  x0 = v128_ziplo_64(t1, t0);
  x1 = v128_ziphi_64(t1, t0);
  x2 = v128_ziplo_64(t3, t2);
  x3 = v128_ziphi_64(t3, t2);
  return v128_add_32(v128_add_32(x0, x1), v128_add_32(x2, x3));
}

/* Computes cost for directions 0, 5, 6 and 7. We can call this function again
   to compute the remaining directions. */
static INLINE void compute_directions(v128 lines[8], int32_t tmp_cost1[4]) {
  v128 partial4a, partial4b, partial5a, partial5b, partial7a, partial7b;
  v128 partial6;
  v128 tmp;
  /* Partial sums for lines 0 and 1. */
  partial4a = v128_shl_n_byte(lines[0], 14);
  partial4b = v128_shr_n_byte(lines[0], 2);
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[1], 12));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[1], 4));
  tmp = v128_add_16(lines[0], lines[1]);
  partial5a = v128_shl_n_byte(tmp, 10);
  partial5b = v128_shr_n_byte(tmp, 6);
  partial7a = v128_shl_n_byte(tmp, 4);
  partial7b = v128_shr_n_byte(tmp, 12);
  partial6 = tmp;

  /* Partial sums for lines 2 and 3. */
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[2], 10));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[2], 6));
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[3], 8));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[3], 8));
  tmp = v128_add_16(lines[2], lines[3]);
  partial5a = v128_add_16(partial5a, v128_shl_n_byte(tmp, 8));
  partial5b = v128_add_16(partial5b, v128_shr_n_byte(tmp, 8));
  partial7a = v128_add_16(partial7a, v128_shl_n_byte(tmp, 6));
  partial7b = v128_add_16(partial7b, v128_shr_n_byte(tmp, 10));
  partial6 = v128_add_16(partial6, tmp);

  /* Partial sums for lines 4 and 5. */
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[4], 6));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[4], 10));
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[5], 4));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[5], 12));
  tmp = v128_add_16(lines[4], lines[5]);
  partial5a = v128_add_16(partial5a, v128_shl_n_byte(tmp, 6));
  partial5b = v128_add_16(partial5b, v128_shr_n_byte(tmp, 10));
  partial7a = v128_add_16(partial7a, v128_shl_n_byte(tmp, 8));
  partial7b = v128_add_16(partial7b, v128_shr_n_byte(tmp, 8));
  partial6 = v128_add_16(partial6, tmp);

  /* Partial sums for lines 6 and 7. */
  partial4a = v128_add_16(partial4a, v128_shl_n_byte(lines[6], 2));
  partial4b = v128_add_16(partial4b, v128_shr_n_byte(lines[6], 14));
  partial4a = v128_add_16(partial4a, lines[7]);
  tmp = v128_add_16(lines[6], lines[7]);
  partial5a = v128_add_16(partial5a, v128_shl_n_byte(tmp, 4));
  partial5b = v128_add_16(partial5b, v128_shr_n_byte(tmp, 12));
  partial7a = v128_add_16(partial7a, v128_shl_n_byte(tmp, 10));
  partial7b = v128_add_16(partial7b, v128_shr_n_byte(tmp, 6));
  partial6 = v128_add_16(partial6, tmp);

  /* Compute costs in terms of partial sums. */
  partial4a =
      fold_mul_and_sum(partial4a, partial4b, v128_from_32(210, 280, 420, 840),
                       v128_from_32(105, 120, 140, 168));
  partial7a =
      fold_mul_and_sum(partial7a, partial7b, v128_from_32(210, 420, 0, 0),
                       v128_from_32(105, 105, 105, 140));
  partial5a =
      fold_mul_and_sum(partial5a, partial5b, v128_from_32(210, 420, 0, 0),
                       v128_from_32(105, 105, 105, 140));
  partial6 = v128_madd_s16(partial6, partial6);
  partial6 = v128_mullo_s32(partial6, v128_dup_32(105));

  partial4a = hsum4(partial4a, partial5a, partial6, partial7a);
  v128_store_unaligned(tmp_cost1, partial4a);
}

/* transpose and reverse the order of the lines -- equivalent to a 90-degree
   counter-clockwise rotation of the pixels. */
static INLINE void array_reverse_transpose_8x8(v128 *in, v128 *res) {
  const v128 tr0_0 = v128_ziplo_16(in[1], in[0]);
  const v128 tr0_1 = v128_ziplo_16(in[3], in[2]);
  const v128 tr0_2 = v128_ziphi_16(in[1], in[0]);
  const v128 tr0_3 = v128_ziphi_16(in[3], in[2]);
  const v128 tr0_4 = v128_ziplo_16(in[5], in[4]);
  const v128 tr0_5 = v128_ziplo_16(in[7], in[6]);
  const v128 tr0_6 = v128_ziphi_16(in[5], in[4]);
  const v128 tr0_7 = v128_ziphi_16(in[7], in[6]);

  const v128 tr1_0 = v128_ziplo_32(tr0_1, tr0_0);
  const v128 tr1_1 = v128_ziplo_32(tr0_5, tr0_4);
  const v128 tr1_2 = v128_ziphi_32(tr0_1, tr0_0);
  const v128 tr1_3 = v128_ziphi_32(tr0_5, tr0_4);
  const v128 tr1_4 = v128_ziplo_32(tr0_3, tr0_2);
  const v128 tr1_5 = v128_ziplo_32(tr0_7, tr0_6);
  const v128 tr1_6 = v128_ziphi_32(tr0_3, tr0_2);
  const v128 tr1_7 = v128_ziphi_32(tr0_7, tr0_6);

  res[7] = v128_ziplo_64(tr1_1, tr1_0);
  res[6] = v128_ziphi_64(tr1_1, tr1_0);
  res[5] = v128_ziplo_64(tr1_3, tr1_2);
  res[4] = v128_ziphi_64(tr1_3, tr1_2);
  res[3] = v128_ziplo_64(tr1_5, tr1_4);
  res[2] = v128_ziphi_64(tr1_5, tr1_4);
  res[1] = v128_ziplo_64(tr1_7, tr1_6);
  res[0] = v128_ziphi_64(tr1_7, tr1_6);
}

int SIMD_FUNC(od_dir_find8)(const od_dering_in *img, int stride, int32_t *var,
                            int coeff_shift) {
  int i;
  int32_t cost[8];
  int32_t best_cost = 0;
  int best_dir = 0;
  v128 lines[8];
  for (i = 0; i < 8; i++) {
    lines[i] = v128_load_unaligned(&img[i * stride]);
    lines[i] =
        v128_sub_16(v128_shr_s16(lines[i], coeff_shift), v128_dup_16(128));
  }

  /* Compute "mostly vertical" directions. */
  compute_directions(lines, cost + 4);

  array_reverse_transpose_8x8(lines, lines);

  /* Compute "mostly horizontal" directions. */
  compute_directions(lines, cost);

  for (i = 0; i < 8; i++) {
    if (cost[i] > best_cost) {
      best_cost = cost[i];
      best_dir = i;
    }
  }

  /* Difference between the optimal variance and the variance along the
     orthogonal direction. Again, the sum(x^2) terms cancel out. */
  *var = best_cost - cost[(best_dir + 4) & 7];
  /* We'd normally divide by 840, but dividing by 1024 is close enough
     for what we're going to do with this. */
  *var >>= 10;
  return best_dir;
}

static INLINE v128 od_cmplt_abs_epi16(v128 in, v128 threshold) {
  return v128_cmplt_s16(v128_abs_s16(in), threshold);
}

int SIMD_FUNC(od_filter_dering_direction_4x4)(uint16_t *y, int ystride,
                                              const uint16_t *in, int threshold,
                                              int dir) {
  int i;
  v128 sum;
  v128 p;
  v128 cmp;
  v128 row;
  v128 res;
  v128 tmp;
  v128 thresh;
  v128 total_abs;
  int off1, off2;
  off1 = OD_DIRECTION_OFFSETS_TABLE[dir][0];
  off2 = OD_DIRECTION_OFFSETS_TABLE[dir][1];
  total_abs = v128_zero();
  thresh = v128_dup_16(threshold);
  for (i = 0; i < 4; i += 2) {
    sum = v128_zero();
    row = v128_from_v64(v64_load_aligned(&in[(i + 1) * OD_FILT_BSTRIDE]),
                        v64_load_aligned(&in[i * OD_FILT_BSTRIDE]));

    /*p = in[i*OD_FILT_BSTRIDE + offset] - row*/
    tmp = v128_from_v64(v64_load_aligned(&in[(i + 1) * OD_FILT_BSTRIDE + off1]),
                        v64_load_aligned(&in[i * OD_FILT_BSTRIDE + off1]));
    p = v128_sub_16(tmp, row);
    /*if (abs(p) < thresh) sum += taps[k]*p*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_shl_n_16(p, 2);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);
    /*p = in[i*OD_FILT_BSTRIDE - offset] - row*/
    tmp = v128_from_v64(v64_load_aligned(&in[(i + 1) * OD_FILT_BSTRIDE - off1]),
                        v64_load_aligned(&in[i * OD_FILT_BSTRIDE - off1]));
    p = v128_sub_16(tmp, row);
    /*if (abs(p) < thresh) sum += taps[k]*p1*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_shl_n_16(p, 2);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*p = in[i*OD_FILT_BSTRIDE + offset] - row*/
    tmp = v128_from_v64(v64_load_aligned(&in[(i + 1) * OD_FILT_BSTRIDE + off2]),
                        v64_load_aligned(&in[i * OD_FILT_BSTRIDE + off2]));
    p = v128_sub_16(tmp, row);
    /*if (abs(p) < thresh) sum += taps[k]*p*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);
    /*p = in[i*OD_FILT_BSTRIDE - offset] - row*/
    tmp = v128_from_v64(v64_load_aligned(&in[(i + 1) * OD_FILT_BSTRIDE - off2]),
                        v64_load_aligned(&in[i * OD_FILT_BSTRIDE - off2]));
    p = v128_sub_16(tmp, row);
    /*if (abs(p) < thresh) sum += taps[k]*p1*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*res = row + ((sum + 8) >> 4)*/
    res = v128_add_16(sum, v128_dup_16(8));
    res = v128_shr_n_s16(res, 4);
    total_abs = v128_add_16(total_abs, v128_abs_s16(res));
    res = v128_add_16(row, res);
    v64_store_aligned(&y[i * ystride], v128_low_v64(res));
    v64_store_aligned(&y[(i + 1) * ystride], v128_high_v64(res));
  }
  return (v128_dotp_s16(total_abs, v128_dup_16(1)) + 2) >> 2;
}

int SIMD_FUNC(od_filter_dering_direction_8x8)(uint16_t *y, int ystride,
                                              const uint16_t *in, int threshold,
                                              int dir) {
  int i;
  v128 sum;
  v128 p;
  v128 cmp;
  v128 row;
  v128 res;
  v128 thresh;
  v128 total_abs;
  int off1, off2, off3;
  off1 = OD_DIRECTION_OFFSETS_TABLE[dir][0];
  off2 = OD_DIRECTION_OFFSETS_TABLE[dir][1];
  off3 = OD_DIRECTION_OFFSETS_TABLE[dir][2];
  total_abs = v128_zero();
  thresh = v128_dup_16(threshold);
  for (i = 0; i < 8; i++) {
    sum = v128_zero();
    row = v128_load_unaligned(&in[i * OD_FILT_BSTRIDE]);

    /*p = in[i*OD_FILT_BSTRIDE + offset] - row*/
    p = v128_sub_16(v128_load_unaligned(&in[i * OD_FILT_BSTRIDE + off1]), row);
    /*if (abs(p) < thresh) sum += taps[k]*p*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_add_16(p, v128_shl_n_16(p, 1));
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*p = in[i*OD_FILT_BSTRIDE - offset] - row*/
    p = v128_sub_16(v128_load_unaligned(&in[i * OD_FILT_BSTRIDE - off1]), row);
    /*if (abs(p) < thresh) sum += taps[k]*p1*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_add_16(p, v128_shl_n_16(p, 1));
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*p = in[i*OD_FILT_BSTRIDE + offset] - row*/
    p = v128_sub_16(v128_load_unaligned(&in[i * OD_FILT_BSTRIDE + off2]), row);
    /*if (abs(p) < thresh) sum += taps[k]*p*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_shl_n_16(p, 1);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*p = in[i*OD_FILT_BSTRIDE - offset] - row*/
    p = v128_sub_16(v128_load_unaligned(&in[i * OD_FILT_BSTRIDE - off2]), row);
    /*if (abs(p) < thresh) sum += taps[k]*p1*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_shl_n_16(p, 1);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*p = in[i*OD_FILT_BSTRIDE + offset] - row*/
    p = v128_sub_16(v128_load_unaligned(&in[i * OD_FILT_BSTRIDE + off3]), row);
    /*if (abs(p) < thresh) sum += taps[k]*p*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*p = in[i*OD_FILT_BSTRIDE - offset] - row*/
    p = v128_sub_16(v128_load_unaligned(&in[i * OD_FILT_BSTRIDE - off3]), row);
    /*if (abs(p) < thresh) sum += taps[k]*p1*/
    cmp = od_cmplt_abs_epi16(p, thresh);
    p = v128_and(p, cmp);
    sum = v128_add_16(sum, p);

    /*res = row + ((sum + 8) >> 4)*/
    res = v128_add_16(sum, v128_dup_16(8));
    res = v128_shr_n_s16(res, 4);
    total_abs = v128_add_16(total_abs, v128_abs_s16(res));
    res = v128_add_16(row, res);
    v128_store_unaligned(&y[i * ystride], res);
  }
  return (v128_dotp_s16(total_abs, v128_dup_16(1)) + 8) >> 4;
}
