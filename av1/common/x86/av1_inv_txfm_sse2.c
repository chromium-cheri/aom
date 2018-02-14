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

#include "./aom_config.h"
#include "./av1_rtcd.h"
#include "av1/common/av1_inv_txfm1d_cfg.h"
#include "av1/common/x86/av1_txfm_sse2.h"

void idct4_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);

  // stage 1
  __m128i x1[4];
  x1[0] = input[0];
  x1[1] = input[2];
  x1[2] = input[1];
  x1[3] = input[3];

  // stage 2
  __m128i x2[4];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x1[0], x1[1], x2[0], x2[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x1[2], x1[3], x2[2], x2[3]);

  // stage 3
  output[0] = _mm_adds_epi16(x2[0], x2[3]);
  output[3] = _mm_subs_epi16(x2[0], x2[3]);
  output[1] = _mm_adds_epi16(x2[1], x2[2]);
  output[2] = _mm_subs_epi16(x2[1], x2[2]);
}

void idct8_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x1[8];
  x1[0] = input[0];
  x1[1] = input[4];
  x1[2] = input[2];
  x1[3] = input[6];
  x1[4] = input[1];
  x1[5] = input[5];
  x1[6] = input[3];
  x1[7] = input[7];

  // stage 2
  __m128i x2[8];
  x2[0] = x1[0];
  x2[1] = x1[1];
  x2[2] = x1[2];
  x2[3] = x1[3];
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x1[4], x1[7], x2[4], x2[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x1[5], x1[6], x2[5], x2[6]);

  // stage 3
  __m128i x3[8];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x2[0], x2[1], x3[0], x3[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x2[2], x2[3], x3[2], x3[3]);
  x3[4] = _mm_adds_epi16(x2[4], x2[5]);
  x3[5] = _mm_subs_epi16(x2[4], x2[5]);
  x3[6] = _mm_subs_epi16(x2[7], x2[6]);
  x3[7] = _mm_adds_epi16(x2[6], x2[7]);

  // stage 4
  __m128i x4[8];
  x4[0] = _mm_adds_epi16(x3[0], x3[3]);
  x4[3] = _mm_subs_epi16(x3[0], x3[3]);
  x4[1] = _mm_adds_epi16(x3[1], x3[2]);
  x4[2] = _mm_subs_epi16(x3[1], x3[2]);
  x4[4] = x3[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x3[5], x3[6], x4[5], x4[6]);
  x4[7] = x3[7];

  // stage 5
  output[0] = _mm_adds_epi16(x4[0], x4[7]);
  output[7] = _mm_subs_epi16(x4[0], x4[7]);
  output[1] = _mm_adds_epi16(x4[1], x4[6]);
  output[6] = _mm_subs_epi16(x4[1], x4[6]);
  output[2] = _mm_adds_epi16(x4[2], x4[5]);
  output[5] = _mm_subs_epi16(x4[2], x4[5]);
  output[3] = _mm_adds_epi16(x4[3], x4[4]);
  output[4] = _mm_subs_epi16(x4[3], x4[4]);
}

void idct16_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x1[16];
  x1[0] = input[0];
  x1[1] = input[8];
  x1[2] = input[4];
  x1[3] = input[12];
  x1[4] = input[2];
  x1[5] = input[10];
  x1[6] = input[6];
  x1[7] = input[14];
  x1[8] = input[1];
  x1[9] = input[9];
  x1[10] = input[5];
  x1[11] = input[13];
  x1[12] = input[3];
  x1[13] = input[11];
  x1[14] = input[7];
  x1[15] = input[15];

  // stage 2
  __m128i x2[16];
  x2[0] = x1[0];
  x2[1] = x1[1];
  x2[2] = x1[2];
  x2[3] = x1[3];
  x2[4] = x1[4];
  x2[5] = x1[5];
  x2[6] = x1[6];
  x2[7] = x1[7];
  btf_16_sse2(cospi_p60_m04, cospi_p04_p60, x1[8], x1[15], x2[8], x2[15]);
  btf_16_sse2(cospi_p28_m36, cospi_p36_p28, x1[9], x1[14], x2[9], x2[14]);
  btf_16_sse2(cospi_p44_m20, cospi_p20_p44, x1[10], x1[13], x2[10], x2[13]);
  btf_16_sse2(cospi_p12_m52, cospi_p52_p12, x1[11], x1[12], x2[11], x2[12]);

  // stage 3
  __m128i x3[16];
  x3[0] = x2[0];
  x3[1] = x2[1];
  x3[2] = x2[2];
  x3[3] = x2[3];
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x2[4], x2[7], x3[4], x3[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x2[5], x2[6], x3[5], x3[6]);
  x3[8] = _mm_adds_epi16(x2[8], x2[9]);
  x3[9] = _mm_subs_epi16(x2[8], x2[9]);
  x3[10] = _mm_subs_epi16(x2[11], x2[10]);
  x3[11] = _mm_adds_epi16(x2[10], x2[11]);
  x3[12] = _mm_adds_epi16(x2[12], x2[13]);
  x3[13] = _mm_subs_epi16(x2[12], x2[13]);
  x3[14] = _mm_subs_epi16(x2[15], x2[14]);
  x3[15] = _mm_adds_epi16(x2[14], x2[15]);

  // stage 4
  __m128i x4[16];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x3[0], x3[1], x4[0], x4[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x3[2], x3[3], x4[2], x4[3]);
  x4[4] = _mm_adds_epi16(x3[4], x3[5]);
  x4[5] = _mm_subs_epi16(x3[4], x3[5]);
  x4[6] = _mm_subs_epi16(x3[7], x3[6]);
  x4[7] = _mm_adds_epi16(x3[6], x3[7]);
  x4[8] = x3[8];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x3[9], x3[14], x4[9], x4[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x3[10], x3[13], x4[10], x4[13]);
  x4[11] = x3[11];
  x4[12] = x3[12];
  x4[15] = x3[15];

  // stage 5
  __m128i x5[16];
  x5[0] = _mm_adds_epi16(x4[0], x4[3]);
  x5[3] = _mm_subs_epi16(x4[0], x4[3]);
  x5[1] = _mm_adds_epi16(x4[1], x4[2]);
  x5[2] = _mm_subs_epi16(x4[1], x4[2]);
  x5[4] = x4[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x4[5], x4[6], x5[5], x5[6]);
  x5[7] = x4[7];
  x5[8] = _mm_adds_epi16(x4[8], x4[11]);
  x5[11] = _mm_subs_epi16(x4[8], x4[11]);
  x5[9] = _mm_adds_epi16(x4[9], x4[10]);
  x5[10] = _mm_subs_epi16(x4[9], x4[10]);
  x5[12] = _mm_subs_epi16(x4[15], x4[12]);
  x5[15] = _mm_adds_epi16(x4[12], x4[15]);
  x5[13] = _mm_subs_epi16(x4[14], x4[13]);
  x5[14] = _mm_adds_epi16(x4[13], x4[14]);

  // stage 6
  __m128i x6[16];
  x6[0] = _mm_adds_epi16(x5[0], x5[7]);
  x6[7] = _mm_subs_epi16(x5[0], x5[7]);
  x6[1] = _mm_adds_epi16(x5[1], x5[6]);
  x6[6] = _mm_subs_epi16(x5[1], x5[6]);
  x6[2] = _mm_adds_epi16(x5[2], x5[5]);
  x6[5] = _mm_subs_epi16(x5[2], x5[5]);
  x6[3] = _mm_adds_epi16(x5[3], x5[4]);
  x6[4] = _mm_subs_epi16(x5[3], x5[4]);
  x6[8] = x5[8];
  x6[9] = x5[9];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x5[10], x5[13], x6[10], x6[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x5[11], x5[12], x6[11], x6[12]);
  x6[14] = x5[14];
  x6[15] = x5[15];

  // stage 7
  output[0] = _mm_adds_epi16(x6[0], x6[15]);
  output[15] = _mm_subs_epi16(x6[0], x6[15]);
  output[1] = _mm_adds_epi16(x6[1], x6[14]);
  output[14] = _mm_subs_epi16(x6[1], x6[14]);
  output[2] = _mm_adds_epi16(x6[2], x6[13]);
  output[13] = _mm_subs_epi16(x6[2], x6[13]);
  output[3] = _mm_adds_epi16(x6[3], x6[12]);
  output[12] = _mm_subs_epi16(x6[3], x6[12]);
  output[4] = _mm_adds_epi16(x6[4], x6[11]);
  output[11] = _mm_subs_epi16(x6[4], x6[11]);
  output[5] = _mm_adds_epi16(x6[5], x6[10]);
  output[10] = _mm_subs_epi16(x6[5], x6[10]);
  output[6] = _mm_adds_epi16(x6[6], x6[9]);
  output[9] = _mm_subs_epi16(x6[6], x6[9]);
  output[7] = _mm_adds_epi16(x6[7], x6[8]);
  output[8] = _mm_subs_epi16(x6[7], x6[8]);
}

void idct32_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);
  __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x1[32];
  x1[0] = input[0];
  x1[1] = input[16];
  x1[2] = input[8];
  x1[3] = input[24];
  x1[4] = input[4];
  x1[5] = input[20];
  x1[6] = input[12];
  x1[7] = input[28];
  x1[8] = input[2];
  x1[9] = input[18];
  x1[10] = input[10];
  x1[11] = input[26];
  x1[12] = input[6];
  x1[13] = input[22];
  x1[14] = input[14];
  x1[15] = input[30];
  x1[16] = input[1];
  x1[17] = input[17];
  x1[18] = input[9];
  x1[19] = input[25];
  x1[20] = input[5];
  x1[21] = input[21];
  x1[22] = input[13];
  x1[23] = input[29];
  x1[24] = input[3];
  x1[25] = input[19];
  x1[26] = input[11];
  x1[27] = input[27];
  x1[28] = input[7];
  x1[29] = input[23];
  x1[30] = input[15];
  x1[31] = input[31];

  // stage 2
  __m128i x2[32];
  x2[0] = x1[0];
  x2[1] = x1[1];
  x2[2] = x1[2];
  x2[3] = x1[3];
  x2[4] = x1[4];
  x2[5] = x1[5];
  x2[6] = x1[6];
  x2[7] = x1[7];
  x2[8] = x1[8];
  x2[9] = x1[9];
  x2[10] = x1[10];
  x2[11] = x1[11];
  x2[12] = x1[12];
  x2[13] = x1[13];
  x2[14] = x1[14];
  x2[15] = x1[15];
  btf_16_sse2(cospi_p62_m02, cospi_p02_p62, x1[16], x1[31], x2[16], x2[31]);
  btf_16_sse2(cospi_p30_m34, cospi_p34_p30, x1[17], x1[30], x2[17], x2[30]);
  btf_16_sse2(cospi_p46_m18, cospi_p18_p46, x1[18], x1[29], x2[18], x2[29]);
  btf_16_sse2(cospi_p14_m50, cospi_p50_p14, x1[19], x1[28], x2[19], x2[28]);
  btf_16_sse2(cospi_p54_m10, cospi_p10_p54, x1[20], x1[27], x2[20], x2[27]);
  btf_16_sse2(cospi_p22_m42, cospi_p42_p22, x1[21], x1[26], x2[21], x2[26]);
  btf_16_sse2(cospi_p38_m26, cospi_p26_p38, x1[22], x1[25], x2[22], x2[25]);
  btf_16_sse2(cospi_p06_m58, cospi_p58_p06, x1[23], x1[24], x2[23], x2[24]);

  // stage 3
  __m128i x3[32];
  x3[0] = x2[0];
  x3[1] = x2[1];
  x3[2] = x2[2];
  x3[3] = x2[3];
  x3[4] = x2[4];
  x3[5] = x2[5];
  x3[6] = x2[6];
  x3[7] = x2[7];
  btf_16_sse2(cospi_p60_m04, cospi_p04_p60, x2[8], x2[15], x3[8], x3[15]);
  btf_16_sse2(cospi_p28_m36, cospi_p36_p28, x2[9], x2[14], x3[9], x3[14]);
  btf_16_sse2(cospi_p44_m20, cospi_p20_p44, x2[10], x2[13], x3[10], x3[13]);
  btf_16_sse2(cospi_p12_m52, cospi_p52_p12, x2[11], x2[12], x3[11], x3[12]);
  x3[16] = _mm_adds_epi16(x2[16], x2[17]);
  x3[17] = _mm_subs_epi16(x2[16], x2[17]);
  x3[18] = _mm_subs_epi16(x2[19], x2[18]);
  x3[19] = _mm_adds_epi16(x2[18], x2[19]);
  x3[20] = _mm_adds_epi16(x2[20], x2[21]);
  x3[21] = _mm_subs_epi16(x2[20], x2[21]);
  x3[22] = _mm_subs_epi16(x2[23], x2[22]);
  x3[23] = _mm_adds_epi16(x2[22], x2[23]);
  x3[24] = _mm_adds_epi16(x2[24], x2[25]);
  x3[25] = _mm_subs_epi16(x2[24], x2[25]);
  x3[26] = _mm_subs_epi16(x2[27], x2[26]);
  x3[27] = _mm_adds_epi16(x2[26], x2[27]);
  x3[28] = _mm_adds_epi16(x2[28], x2[29]);
  x3[29] = _mm_subs_epi16(x2[28], x2[29]);
  x3[30] = _mm_subs_epi16(x2[31], x2[30]);
  x3[31] = _mm_adds_epi16(x2[30], x2[31]);

  // stage 4
  __m128i x4[32];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x3[4], x3[7], x4[4], x4[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x3[5], x3[6], x4[5], x4[6]);
  x4[8] = _mm_adds_epi16(x3[8], x3[9]);
  x4[9] = _mm_subs_epi16(x3[8], x3[9]);
  x4[10] = _mm_subs_epi16(x3[11], x3[10]);
  x4[11] = _mm_adds_epi16(x3[10], x3[11]);
  x4[12] = _mm_adds_epi16(x3[12], x3[13]);
  x4[13] = _mm_subs_epi16(x3[12], x3[13]);
  x4[14] = _mm_subs_epi16(x3[15], x3[14]);
  x4[15] = _mm_adds_epi16(x3[14], x3[15]);
  x4[16] = x3[16];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x3[17], x3[30], x4[17], x4[30]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x3[18], x3[29], x4[18], x4[29]);
  x4[19] = x3[19];
  x4[20] = x3[20];
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x3[21], x3[26], x4[21], x4[26]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x3[22], x3[25], x4[22], x4[25]);
  x4[23] = x3[23];
  x4[24] = x3[24];
  x4[27] = x3[27];
  x4[28] = x3[28];
  x4[31] = x3[31];

  // stage 5
  __m128i x5[32];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x4[0], x4[1], x5[0], x5[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x4[2], x4[3], x5[2], x5[3]);
  x5[4] = _mm_adds_epi16(x4[4], x4[5]);
  x5[5] = _mm_subs_epi16(x4[4], x4[5]);
  x5[6] = _mm_subs_epi16(x4[7], x4[6]);
  x5[7] = _mm_adds_epi16(x4[6], x4[7]);
  x5[8] = x4[8];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x4[9], x4[14], x5[9], x5[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x4[10], x4[13], x5[10], x5[13]);
  x5[11] = x4[11];
  x5[12] = x4[12];
  x5[15] = x4[15];
  x5[16] = _mm_adds_epi16(x4[16], x4[19]);
  x5[19] = _mm_subs_epi16(x4[16], x4[19]);
  x5[17] = _mm_adds_epi16(x4[17], x4[18]);
  x5[18] = _mm_subs_epi16(x4[17], x4[18]);
  x5[20] = _mm_subs_epi16(x4[23], x4[20]);
  x5[23] = _mm_adds_epi16(x4[20], x4[23]);
  x5[21] = _mm_subs_epi16(x4[22], x4[21]);
  x5[22] = _mm_adds_epi16(x4[21], x4[22]);
  x5[24] = _mm_adds_epi16(x4[24], x4[27]);
  x5[27] = _mm_subs_epi16(x4[24], x4[27]);
  x5[25] = _mm_adds_epi16(x4[25], x4[26]);
  x5[26] = _mm_subs_epi16(x4[25], x4[26]);
  x5[28] = _mm_subs_epi16(x4[31], x4[28]);
  x5[31] = _mm_adds_epi16(x4[28], x4[31]);
  x5[29] = _mm_subs_epi16(x4[30], x4[29]);
  x5[30] = _mm_adds_epi16(x4[29], x4[30]);

  // stage 6
  __m128i x6[32];
  x6[0] = _mm_adds_epi16(x5[0], x5[3]);
  x6[3] = _mm_subs_epi16(x5[0], x5[3]);
  x6[1] = _mm_adds_epi16(x5[1], x5[2]);
  x6[2] = _mm_subs_epi16(x5[1], x5[2]);
  x6[4] = x5[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x5[5], x5[6], x6[5], x6[6]);
  x6[7] = x5[7];
  x6[8] = _mm_adds_epi16(x5[8], x5[11]);
  x6[11] = _mm_subs_epi16(x5[8], x5[11]);
  x6[9] = _mm_adds_epi16(x5[9], x5[10]);
  x6[10] = _mm_subs_epi16(x5[9], x5[10]);
  x6[12] = _mm_subs_epi16(x5[15], x5[12]);
  x6[15] = _mm_adds_epi16(x5[12], x5[15]);
  x6[13] = _mm_subs_epi16(x5[14], x5[13]);
  x6[14] = _mm_adds_epi16(x5[13], x5[14]);
  x6[16] = x5[16];
  x6[17] = x5[17];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x5[18], x5[29], x6[18], x6[29]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x5[19], x5[28], x6[19], x6[28]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x5[20], x5[27], x6[20], x6[27]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x5[21], x5[26], x6[21], x6[26]);
  x6[22] = x5[22];
  x6[23] = x5[23];
  x6[24] = x5[24];
  x6[25] = x5[25];
  x6[30] = x5[30];
  x6[31] = x5[31];

  // stage 7
  __m128i x7[32];
  x7[0] = _mm_adds_epi16(x6[0], x6[7]);
  x7[7] = _mm_subs_epi16(x6[0], x6[7]);
  x7[1] = _mm_adds_epi16(x6[1], x6[6]);
  x7[6] = _mm_subs_epi16(x6[1], x6[6]);
  x7[2] = _mm_adds_epi16(x6[2], x6[5]);
  x7[5] = _mm_subs_epi16(x6[2], x6[5]);
  x7[3] = _mm_adds_epi16(x6[3], x6[4]);
  x7[4] = _mm_subs_epi16(x6[3], x6[4]);
  x7[8] = x6[8];
  x7[9] = x6[9];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x6[10], x6[13], x7[10], x7[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x6[11], x6[12], x7[11], x7[12]);
  x7[14] = x6[14];
  x7[15] = x6[15];
  x7[16] = _mm_adds_epi16(x6[16], x6[23]);
  x7[23] = _mm_subs_epi16(x6[16], x6[23]);
  x7[17] = _mm_adds_epi16(x6[17], x6[22]);
  x7[22] = _mm_subs_epi16(x6[17], x6[22]);
  x7[18] = _mm_adds_epi16(x6[18], x6[21]);
  x7[21] = _mm_subs_epi16(x6[18], x6[21]);
  x7[19] = _mm_adds_epi16(x6[19], x6[20]);
  x7[20] = _mm_subs_epi16(x6[19], x6[20]);
  x7[24] = _mm_subs_epi16(x6[31], x6[24]);
  x7[31] = _mm_adds_epi16(x6[24], x6[31]);
  x7[25] = _mm_subs_epi16(x6[30], x6[25]);
  x7[30] = _mm_adds_epi16(x6[25], x6[30]);
  x7[26] = _mm_subs_epi16(x6[29], x6[26]);
  x7[29] = _mm_adds_epi16(x6[26], x6[29]);
  x7[27] = _mm_subs_epi16(x6[28], x6[27]);
  x7[28] = _mm_adds_epi16(x6[27], x6[28]);

  // stage 8
  __m128i x8[32];
  x8[0] = _mm_adds_epi16(x7[0], x7[15]);
  x8[15] = _mm_subs_epi16(x7[0], x7[15]);
  x8[1] = _mm_adds_epi16(x7[1], x7[14]);
  x8[14] = _mm_subs_epi16(x7[1], x7[14]);
  x8[2] = _mm_adds_epi16(x7[2], x7[13]);
  x8[13] = _mm_subs_epi16(x7[2], x7[13]);
  x8[3] = _mm_adds_epi16(x7[3], x7[12]);
  x8[12] = _mm_subs_epi16(x7[3], x7[12]);
  x8[4] = _mm_adds_epi16(x7[4], x7[11]);
  x8[11] = _mm_subs_epi16(x7[4], x7[11]);
  x8[5] = _mm_adds_epi16(x7[5], x7[10]);
  x8[10] = _mm_subs_epi16(x7[5], x7[10]);
  x8[6] = _mm_adds_epi16(x7[6], x7[9]);
  x8[9] = _mm_subs_epi16(x7[6], x7[9]);
  x8[7] = _mm_adds_epi16(x7[7], x7[8]);
  x8[8] = _mm_subs_epi16(x7[7], x7[8]);
  x8[16] = x7[16];
  x8[17] = x7[17];
  x8[18] = x7[18];
  x8[19] = x7[19];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x7[20], x7[27], x8[20], x8[27]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x7[21], x7[26], x8[21], x8[26]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x7[22], x7[25], x8[22], x8[25]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x7[23], x7[24], x8[23], x8[24]);
  x8[28] = x7[28];
  x8[29] = x7[29];
  x8[30] = x7[30];
  x8[31] = x7[31];

  // stage 9
  output[0] = _mm_adds_epi16(x8[0], x8[31]);
  output[31] = _mm_subs_epi16(x8[0], x8[31]);
  output[1] = _mm_adds_epi16(x8[1], x8[30]);
  output[30] = _mm_subs_epi16(x8[1], x8[30]);
  output[2] = _mm_adds_epi16(x8[2], x8[29]);
  output[29] = _mm_subs_epi16(x8[2], x8[29]);
  output[3] = _mm_adds_epi16(x8[3], x8[28]);
  output[28] = _mm_subs_epi16(x8[3], x8[28]);
  output[4] = _mm_adds_epi16(x8[4], x8[27]);
  output[27] = _mm_subs_epi16(x8[4], x8[27]);
  output[5] = _mm_adds_epi16(x8[5], x8[26]);
  output[26] = _mm_subs_epi16(x8[5], x8[26]);
  output[6] = _mm_adds_epi16(x8[6], x8[25]);
  output[25] = _mm_subs_epi16(x8[6], x8[25]);
  output[7] = _mm_adds_epi16(x8[7], x8[24]);
  output[24] = _mm_subs_epi16(x8[7], x8[24]);
  output[8] = _mm_adds_epi16(x8[8], x8[23]);
  output[23] = _mm_subs_epi16(x8[8], x8[23]);
  output[9] = _mm_adds_epi16(x8[9], x8[22]);
  output[22] = _mm_subs_epi16(x8[9], x8[22]);
  output[10] = _mm_adds_epi16(x8[10], x8[21]);
  output[21] = _mm_subs_epi16(x8[10], x8[21]);
  output[11] = _mm_adds_epi16(x8[11], x8[20]);
  output[20] = _mm_subs_epi16(x8[11], x8[20]);
  output[12] = _mm_adds_epi16(x8[12], x8[19]);
  output[19] = _mm_subs_epi16(x8[12], x8[19]);
  output[13] = _mm_adds_epi16(x8[13], x8[18]);
  output[18] = _mm_subs_epi16(x8[13], x8[18]);
  output[14] = _mm_adds_epi16(x8[14], x8[17]);
  output[17] = _mm_subs_epi16(x8[14], x8[17]);
  output[15] = _mm_adds_epi16(x8[15], x8[16]);
  output[16] = _mm_subs_epi16(x8[15], x8[16]);
}

void idct64_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p63_m01 = pair_set_epi16(cospi[63], -cospi[1]);
  __m128i cospi_p01_p63 = pair_set_epi16(cospi[1], cospi[63]);
  __m128i cospi_p31_m33 = pair_set_epi16(cospi[31], -cospi[33]);
  __m128i cospi_p33_p31 = pair_set_epi16(cospi[33], cospi[31]);
  __m128i cospi_p47_m17 = pair_set_epi16(cospi[47], -cospi[17]);
  __m128i cospi_p17_p47 = pair_set_epi16(cospi[17], cospi[47]);
  __m128i cospi_p15_m49 = pair_set_epi16(cospi[15], -cospi[49]);
  __m128i cospi_p49_p15 = pair_set_epi16(cospi[49], cospi[15]);
  __m128i cospi_p55_m09 = pair_set_epi16(cospi[55], -cospi[9]);
  __m128i cospi_p09_p55 = pair_set_epi16(cospi[9], cospi[55]);
  __m128i cospi_p23_m41 = pair_set_epi16(cospi[23], -cospi[41]);
  __m128i cospi_p41_p23 = pair_set_epi16(cospi[41], cospi[23]);
  __m128i cospi_p39_m25 = pair_set_epi16(cospi[39], -cospi[25]);
  __m128i cospi_p25_p39 = pair_set_epi16(cospi[25], cospi[39]);
  __m128i cospi_p07_m57 = pair_set_epi16(cospi[7], -cospi[57]);
  __m128i cospi_p57_p07 = pair_set_epi16(cospi[57], cospi[7]);
  __m128i cospi_p59_m05 = pair_set_epi16(cospi[59], -cospi[5]);
  __m128i cospi_p05_p59 = pair_set_epi16(cospi[5], cospi[59]);
  __m128i cospi_p27_m37 = pair_set_epi16(cospi[27], -cospi[37]);
  __m128i cospi_p37_p27 = pair_set_epi16(cospi[37], cospi[27]);
  __m128i cospi_p43_m21 = pair_set_epi16(cospi[43], -cospi[21]);
  __m128i cospi_p21_p43 = pair_set_epi16(cospi[21], cospi[43]);
  __m128i cospi_p11_m53 = pair_set_epi16(cospi[11], -cospi[53]);
  __m128i cospi_p53_p11 = pair_set_epi16(cospi[53], cospi[11]);
  __m128i cospi_p51_m13 = pair_set_epi16(cospi[51], -cospi[13]);
  __m128i cospi_p13_p51 = pair_set_epi16(cospi[13], cospi[51]);
  __m128i cospi_p19_m45 = pair_set_epi16(cospi[19], -cospi[45]);
  __m128i cospi_p45_p19 = pair_set_epi16(cospi[45], cospi[19]);
  __m128i cospi_p35_m29 = pair_set_epi16(cospi[35], -cospi[29]);
  __m128i cospi_p29_p35 = pair_set_epi16(cospi[29], cospi[35]);
  __m128i cospi_p03_m61 = pair_set_epi16(cospi[3], -cospi[61]);
  __m128i cospi_p61_p03 = pair_set_epi16(cospi[61], cospi[3]);
  __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);
  __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  __m128i cospi_m04_p60 = pair_set_epi16(-cospi[4], cospi[60]);
  __m128i cospi_p60_p04 = pair_set_epi16(cospi[60], cospi[4]);
  __m128i cospi_m60_m04 = pair_set_epi16(-cospi[60], -cospi[4]);
  __m128i cospi_m36_p28 = pair_set_epi16(-cospi[36], cospi[28]);
  __m128i cospi_p28_p36 = pair_set_epi16(cospi[28], cospi[36]);
  __m128i cospi_m28_m36 = pair_set_epi16(-cospi[28], -cospi[36]);
  __m128i cospi_m20_p44 = pair_set_epi16(-cospi[20], cospi[44]);
  __m128i cospi_p44_p20 = pair_set_epi16(cospi[44], cospi[20]);
  __m128i cospi_m44_m20 = pair_set_epi16(-cospi[44], -cospi[20]);
  __m128i cospi_m52_p12 = pair_set_epi16(-cospi[52], cospi[12]);
  __m128i cospi_p12_p52 = pair_set_epi16(cospi[12], cospi[52]);
  __m128i cospi_m12_m52 = pair_set_epi16(-cospi[12], -cospi[52]);
  __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  __m128i cospi_m08_p56 = pair_set_epi16(-cospi[8], cospi[56]);
  __m128i cospi_p56_p08 = pair_set_epi16(cospi[56], cospi[8]);
  __m128i cospi_m56_m08 = pair_set_epi16(-cospi[56], -cospi[8]);
  __m128i cospi_m40_p24 = pair_set_epi16(-cospi[40], cospi[24]);
  __m128i cospi_p24_p40 = pair_set_epi16(cospi[24], cospi[40]);
  __m128i cospi_m24_m40 = pair_set_epi16(-cospi[24], -cospi[40]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_m16_p48 = pair_set_epi16(-cospi[16], cospi[48]);
  __m128i cospi_p48_p16 = pair_set_epi16(cospi[48], cospi[16]);
  __m128i cospi_m48_m16 = pair_set_epi16(-cospi[48], -cospi[16]);
  __m128i cospi_m32_p32 = pair_set_epi16(-cospi[32], cospi[32]);

  // stage 1
  __m128i x1[64];
  x1[0] = input[0];
  x1[1] = input[32];
  x1[2] = input[16];
  x1[3] = input[48];
  x1[4] = input[8];
  x1[5] = input[40];
  x1[6] = input[24];
  x1[7] = input[56];
  x1[8] = input[4];
  x1[9] = input[36];
  x1[10] = input[20];
  x1[11] = input[52];
  x1[12] = input[12];
  x1[13] = input[44];
  x1[14] = input[28];
  x1[15] = input[60];
  x1[16] = input[2];
  x1[17] = input[34];
  x1[18] = input[18];
  x1[19] = input[50];
  x1[20] = input[10];
  x1[21] = input[42];
  x1[22] = input[26];
  x1[23] = input[58];
  x1[24] = input[6];
  x1[25] = input[38];
  x1[26] = input[22];
  x1[27] = input[54];
  x1[28] = input[14];
  x1[29] = input[46];
  x1[30] = input[30];
  x1[31] = input[62];
  x1[32] = input[1];
  x1[33] = input[33];
  x1[34] = input[17];
  x1[35] = input[49];
  x1[36] = input[9];
  x1[37] = input[41];
  x1[38] = input[25];
  x1[39] = input[57];
  x1[40] = input[5];
  x1[41] = input[37];
  x1[42] = input[21];
  x1[43] = input[53];
  x1[44] = input[13];
  x1[45] = input[45];
  x1[46] = input[29];
  x1[47] = input[61];
  x1[48] = input[3];
  x1[49] = input[35];
  x1[50] = input[19];
  x1[51] = input[51];
  x1[52] = input[11];
  x1[53] = input[43];
  x1[54] = input[27];
  x1[55] = input[59];
  x1[56] = input[7];
  x1[57] = input[39];
  x1[58] = input[23];
  x1[59] = input[55];
  x1[60] = input[15];
  x1[61] = input[47];
  x1[62] = input[31];
  x1[63] = input[63];

  // stage 2
  __m128i x2[64];
  x2[0] = x1[0];
  x2[1] = x1[1];
  x2[2] = x1[2];
  x2[3] = x1[3];
  x2[4] = x1[4];
  x2[5] = x1[5];
  x2[6] = x1[6];
  x2[7] = x1[7];
  x2[8] = x1[8];
  x2[9] = x1[9];
  x2[10] = x1[10];
  x2[11] = x1[11];
  x2[12] = x1[12];
  x2[13] = x1[13];
  x2[14] = x1[14];
  x2[15] = x1[15];
  x2[16] = x1[16];
  x2[17] = x1[17];
  x2[18] = x1[18];
  x2[19] = x1[19];
  x2[20] = x1[20];
  x2[21] = x1[21];
  x2[22] = x1[22];
  x2[23] = x1[23];
  x2[24] = x1[24];
  x2[25] = x1[25];
  x2[26] = x1[26];
  x2[27] = x1[27];
  x2[28] = x1[28];
  x2[29] = x1[29];
  x2[30] = x1[30];
  x2[31] = x1[31];
  btf_16_sse2(cospi_p63_m01, cospi_p01_p63, x1[32], x1[63], x2[32], x2[63]);
  btf_16_sse2(cospi_p31_m33, cospi_p33_p31, x1[33], x1[62], x2[33], x2[62]);
  btf_16_sse2(cospi_p47_m17, cospi_p17_p47, x1[34], x1[61], x2[34], x2[61]);
  btf_16_sse2(cospi_p15_m49, cospi_p49_p15, x1[35], x1[60], x2[35], x2[60]);
  btf_16_sse2(cospi_p55_m09, cospi_p09_p55, x1[36], x1[59], x2[36], x2[59]);
  btf_16_sse2(cospi_p23_m41, cospi_p41_p23, x1[37], x1[58], x2[37], x2[58]);
  btf_16_sse2(cospi_p39_m25, cospi_p25_p39, x1[38], x1[57], x2[38], x2[57]);
  btf_16_sse2(cospi_p07_m57, cospi_p57_p07, x1[39], x1[56], x2[39], x2[56]);
  btf_16_sse2(cospi_p59_m05, cospi_p05_p59, x1[40], x1[55], x2[40], x2[55]);
  btf_16_sse2(cospi_p27_m37, cospi_p37_p27, x1[41], x1[54], x2[41], x2[54]);
  btf_16_sse2(cospi_p43_m21, cospi_p21_p43, x1[42], x1[53], x2[42], x2[53]);
  btf_16_sse2(cospi_p11_m53, cospi_p53_p11, x1[43], x1[52], x2[43], x2[52]);
  btf_16_sse2(cospi_p51_m13, cospi_p13_p51, x1[44], x1[51], x2[44], x2[51]);
  btf_16_sse2(cospi_p19_m45, cospi_p45_p19, x1[45], x1[50], x2[45], x2[50]);
  btf_16_sse2(cospi_p35_m29, cospi_p29_p35, x1[46], x1[49], x2[46], x2[49]);
  btf_16_sse2(cospi_p03_m61, cospi_p61_p03, x1[47], x1[48], x2[47], x2[48]);

  // stage 3
  __m128i x3[64];
  x3[0] = x2[0];
  x3[1] = x2[1];
  x3[2] = x2[2];
  x3[3] = x2[3];
  x3[4] = x2[4];
  x3[5] = x2[5];
  x3[6] = x2[6];
  x3[7] = x2[7];
  x3[8] = x2[8];
  x3[9] = x2[9];
  x3[10] = x2[10];
  x3[11] = x2[11];
  x3[12] = x2[12];
  x3[13] = x2[13];
  x3[14] = x2[14];
  x3[15] = x2[15];
  btf_16_sse2(cospi_p62_m02, cospi_p02_p62, x2[16], x2[31], x3[16], x3[31]);
  btf_16_sse2(cospi_p30_m34, cospi_p34_p30, x2[17], x2[30], x3[17], x3[30]);
  btf_16_sse2(cospi_p46_m18, cospi_p18_p46, x2[18], x2[29], x3[18], x3[29]);
  btf_16_sse2(cospi_p14_m50, cospi_p50_p14, x2[19], x2[28], x3[19], x3[28]);
  btf_16_sse2(cospi_p54_m10, cospi_p10_p54, x2[20], x2[27], x3[20], x3[27]);
  btf_16_sse2(cospi_p22_m42, cospi_p42_p22, x2[21], x2[26], x3[21], x3[26]);
  btf_16_sse2(cospi_p38_m26, cospi_p26_p38, x2[22], x2[25], x3[22], x3[25]);
  btf_16_sse2(cospi_p06_m58, cospi_p58_p06, x2[23], x2[24], x3[23], x3[24]);
  x3[32] = _mm_adds_epi16(x2[32], x2[33]);
  x3[33] = _mm_subs_epi16(x2[32], x2[33]);
  x3[34] = _mm_subs_epi16(x2[35], x2[34]);
  x3[35] = _mm_adds_epi16(x2[34], x2[35]);
  x3[36] = _mm_adds_epi16(x2[36], x2[37]);
  x3[37] = _mm_subs_epi16(x2[36], x2[37]);
  x3[38] = _mm_subs_epi16(x2[39], x2[38]);
  x3[39] = _mm_adds_epi16(x2[38], x2[39]);
  x3[40] = _mm_adds_epi16(x2[40], x2[41]);
  x3[41] = _mm_subs_epi16(x2[40], x2[41]);
  x3[42] = _mm_subs_epi16(x2[43], x2[42]);
  x3[43] = _mm_adds_epi16(x2[42], x2[43]);
  x3[44] = _mm_adds_epi16(x2[44], x2[45]);
  x3[45] = _mm_subs_epi16(x2[44], x2[45]);
  x3[46] = _mm_subs_epi16(x2[47], x2[46]);
  x3[47] = _mm_adds_epi16(x2[46], x2[47]);
  x3[48] = _mm_adds_epi16(x2[48], x2[49]);
  x3[49] = _mm_subs_epi16(x2[48], x2[49]);
  x3[50] = _mm_subs_epi16(x2[51], x2[50]);
  x3[51] = _mm_adds_epi16(x2[50], x2[51]);
  x3[52] = _mm_adds_epi16(x2[52], x2[53]);
  x3[53] = _mm_subs_epi16(x2[52], x2[53]);
  x3[54] = _mm_subs_epi16(x2[55], x2[54]);
  x3[55] = _mm_adds_epi16(x2[54], x2[55]);
  x3[56] = _mm_adds_epi16(x2[56], x2[57]);
  x3[57] = _mm_subs_epi16(x2[56], x2[57]);
  x3[58] = _mm_subs_epi16(x2[59], x2[58]);
  x3[59] = _mm_adds_epi16(x2[58], x2[59]);
  x3[60] = _mm_adds_epi16(x2[60], x2[61]);
  x3[61] = _mm_subs_epi16(x2[60], x2[61]);
  x3[62] = _mm_subs_epi16(x2[63], x2[62]);
  x3[63] = _mm_adds_epi16(x2[62], x2[63]);

  // stage 4
  __m128i x4[64];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  x4[4] = x3[4];
  x4[5] = x3[5];
  x4[6] = x3[6];
  x4[7] = x3[7];
  btf_16_sse2(cospi_p60_m04, cospi_p04_p60, x3[8], x3[15], x4[8], x4[15]);
  btf_16_sse2(cospi_p28_m36, cospi_p36_p28, x3[9], x3[14], x4[9], x4[14]);
  btf_16_sse2(cospi_p44_m20, cospi_p20_p44, x3[10], x3[13], x4[10], x4[13]);
  btf_16_sse2(cospi_p12_m52, cospi_p52_p12, x3[11], x3[12], x4[11], x4[12]);
  x4[16] = _mm_adds_epi16(x3[16], x3[17]);
  x4[17] = _mm_subs_epi16(x3[16], x3[17]);
  x4[18] = _mm_subs_epi16(x3[19], x3[18]);
  x4[19] = _mm_adds_epi16(x3[18], x3[19]);
  x4[20] = _mm_adds_epi16(x3[20], x3[21]);
  x4[21] = _mm_subs_epi16(x3[20], x3[21]);
  x4[22] = _mm_subs_epi16(x3[23], x3[22]);
  x4[23] = _mm_adds_epi16(x3[22], x3[23]);
  x4[24] = _mm_adds_epi16(x3[24], x3[25]);
  x4[25] = _mm_subs_epi16(x3[24], x3[25]);
  x4[26] = _mm_subs_epi16(x3[27], x3[26]);
  x4[27] = _mm_adds_epi16(x3[26], x3[27]);
  x4[28] = _mm_adds_epi16(x3[28], x3[29]);
  x4[29] = _mm_subs_epi16(x3[28], x3[29]);
  x4[30] = _mm_subs_epi16(x3[31], x3[30]);
  x4[31] = _mm_adds_epi16(x3[30], x3[31]);
  x4[32] = x3[32];
  btf_16_sse2(cospi_m04_p60, cospi_p60_p04, x3[33], x3[62], x4[33], x4[62]);
  btf_16_sse2(cospi_m60_m04, cospi_m04_p60, x3[34], x3[61], x4[34], x4[61]);
  x4[35] = x3[35];
  x4[36] = x3[36];
  btf_16_sse2(cospi_m36_p28, cospi_p28_p36, x3[37], x3[58], x4[37], x4[58]);
  btf_16_sse2(cospi_m28_m36, cospi_m36_p28, x3[38], x3[57], x4[38], x4[57]);
  x4[39] = x3[39];
  x4[40] = x3[40];
  btf_16_sse2(cospi_m20_p44, cospi_p44_p20, x3[41], x3[54], x4[41], x4[54]);
  btf_16_sse2(cospi_m44_m20, cospi_m20_p44, x3[42], x3[53], x4[42], x4[53]);
  x4[43] = x3[43];
  x4[44] = x3[44];
  btf_16_sse2(cospi_m52_p12, cospi_p12_p52, x3[45], x3[50], x4[45], x4[50]);
  btf_16_sse2(cospi_m12_m52, cospi_m52_p12, x3[46], x3[49], x4[46], x4[49]);
  x4[47] = x3[47];
  x4[48] = x3[48];
  x4[51] = x3[51];
  x4[52] = x3[52];
  x4[55] = x3[55];
  x4[56] = x3[56];
  x4[59] = x3[59];
  x4[60] = x3[60];
  x4[63] = x3[63];

  // stage 5
  __m128i x5[64];
  x5[0] = x4[0];
  x5[1] = x4[1];
  x5[2] = x4[2];
  x5[3] = x4[3];
  btf_16_sse2(cospi_p56_m08, cospi_p08_p56, x4[4], x4[7], x5[4], x5[7]);
  btf_16_sse2(cospi_p24_m40, cospi_p40_p24, x4[5], x4[6], x5[5], x5[6]);
  x5[8] = _mm_adds_epi16(x4[8], x4[9]);
  x5[9] = _mm_subs_epi16(x4[8], x4[9]);
  x5[10] = _mm_subs_epi16(x4[11], x4[10]);
  x5[11] = _mm_adds_epi16(x4[10], x4[11]);
  x5[12] = _mm_adds_epi16(x4[12], x4[13]);
  x5[13] = _mm_subs_epi16(x4[12], x4[13]);
  x5[14] = _mm_subs_epi16(x4[15], x4[14]);
  x5[15] = _mm_adds_epi16(x4[14], x4[15]);
  x5[16] = x4[16];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x4[17], x4[30], x5[17], x5[30]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x4[18], x4[29], x5[18], x5[29]);
  x5[19] = x4[19];
  x5[20] = x4[20];
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x4[21], x4[26], x5[21], x5[26]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x4[22], x4[25], x5[22], x5[25]);
  x5[23] = x4[23];
  x5[24] = x4[24];
  x5[27] = x4[27];
  x5[28] = x4[28];
  x5[31] = x4[31];
  x5[32] = _mm_adds_epi16(x4[32], x4[35]);
  x5[35] = _mm_subs_epi16(x4[32], x4[35]);
  x5[33] = _mm_adds_epi16(x4[33], x4[34]);
  x5[34] = _mm_subs_epi16(x4[33], x4[34]);
  x5[36] = _mm_subs_epi16(x4[39], x4[36]);
  x5[39] = _mm_adds_epi16(x4[36], x4[39]);
  x5[37] = _mm_subs_epi16(x4[38], x4[37]);
  x5[38] = _mm_adds_epi16(x4[37], x4[38]);
  x5[40] = _mm_adds_epi16(x4[40], x4[43]);
  x5[43] = _mm_subs_epi16(x4[40], x4[43]);
  x5[41] = _mm_adds_epi16(x4[41], x4[42]);
  x5[42] = _mm_subs_epi16(x4[41], x4[42]);
  x5[44] = _mm_subs_epi16(x4[47], x4[44]);
  x5[47] = _mm_adds_epi16(x4[44], x4[47]);
  x5[45] = _mm_subs_epi16(x4[46], x4[45]);
  x5[46] = _mm_adds_epi16(x4[45], x4[46]);
  x5[48] = _mm_adds_epi16(x4[48], x4[51]);
  x5[51] = _mm_subs_epi16(x4[48], x4[51]);
  x5[49] = _mm_adds_epi16(x4[49], x4[50]);
  x5[50] = _mm_subs_epi16(x4[49], x4[50]);
  x5[52] = _mm_subs_epi16(x4[55], x4[52]);
  x5[55] = _mm_adds_epi16(x4[52], x4[55]);
  x5[53] = _mm_subs_epi16(x4[54], x4[53]);
  x5[54] = _mm_adds_epi16(x4[53], x4[54]);
  x5[56] = _mm_adds_epi16(x4[56], x4[59]);
  x5[59] = _mm_subs_epi16(x4[56], x4[59]);
  x5[57] = _mm_adds_epi16(x4[57], x4[58]);
  x5[58] = _mm_subs_epi16(x4[57], x4[58]);
  x5[60] = _mm_subs_epi16(x4[63], x4[60]);
  x5[63] = _mm_adds_epi16(x4[60], x4[63]);
  x5[61] = _mm_subs_epi16(x4[62], x4[61]);
  x5[62] = _mm_adds_epi16(x4[61], x4[62]);

  // stage 6
  __m128i x6[64];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x5[0], x5[1], x6[0], x6[1]);
  btf_16_sse2(cospi_p48_m16, cospi_p16_p48, x5[2], x5[3], x6[2], x6[3]);
  x6[4] = _mm_adds_epi16(x5[4], x5[5]);
  x6[5] = _mm_subs_epi16(x5[4], x5[5]);
  x6[6] = _mm_subs_epi16(x5[7], x5[6]);
  x6[7] = _mm_adds_epi16(x5[6], x5[7]);
  x6[8] = x5[8];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x5[9], x5[14], x6[9], x6[14]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x5[10], x5[13], x6[10], x6[13]);
  x6[11] = x5[11];
  x6[12] = x5[12];
  x6[15] = x5[15];
  x6[16] = _mm_adds_epi16(x5[16], x5[19]);
  x6[19] = _mm_subs_epi16(x5[16], x5[19]);
  x6[17] = _mm_adds_epi16(x5[17], x5[18]);
  x6[18] = _mm_subs_epi16(x5[17], x5[18]);
  x6[20] = _mm_subs_epi16(x5[23], x5[20]);
  x6[23] = _mm_adds_epi16(x5[20], x5[23]);
  x6[21] = _mm_subs_epi16(x5[22], x5[21]);
  x6[22] = _mm_adds_epi16(x5[21], x5[22]);
  x6[24] = _mm_adds_epi16(x5[24], x5[27]);
  x6[27] = _mm_subs_epi16(x5[24], x5[27]);
  x6[25] = _mm_adds_epi16(x5[25], x5[26]);
  x6[26] = _mm_subs_epi16(x5[25], x5[26]);
  x6[28] = _mm_subs_epi16(x5[31], x5[28]);
  x6[31] = _mm_adds_epi16(x5[28], x5[31]);
  x6[29] = _mm_subs_epi16(x5[30], x5[29]);
  x6[30] = _mm_adds_epi16(x5[29], x5[30]);
  x6[32] = x5[32];
  x6[33] = x5[33];
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x5[34], x5[61], x6[34], x6[61]);
  btf_16_sse2(cospi_m08_p56, cospi_p56_p08, x5[35], x5[60], x6[35], x6[60]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x5[36], x5[59], x6[36], x6[59]);
  btf_16_sse2(cospi_m56_m08, cospi_m08_p56, x5[37], x5[58], x6[37], x6[58]);
  x6[38] = x5[38];
  x6[39] = x5[39];
  x6[40] = x5[40];
  x6[41] = x5[41];
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x5[42], x5[53], x6[42], x6[53]);
  btf_16_sse2(cospi_m40_p24, cospi_p24_p40, x5[43], x5[52], x6[43], x6[52]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x5[44], x5[51], x6[44], x6[51]);
  btf_16_sse2(cospi_m24_m40, cospi_m40_p24, x5[45], x5[50], x6[45], x6[50]);
  x6[46] = x5[46];
  x6[47] = x5[47];
  x6[48] = x5[48];
  x6[49] = x5[49];
  x6[54] = x5[54];
  x6[55] = x5[55];
  x6[56] = x5[56];
  x6[57] = x5[57];
  x6[62] = x5[62];
  x6[63] = x5[63];

  // stage 7
  __m128i x7[64];
  x7[0] = _mm_adds_epi16(x6[0], x6[3]);
  x7[3] = _mm_subs_epi16(x6[0], x6[3]);
  x7[1] = _mm_adds_epi16(x6[1], x6[2]);
  x7[2] = _mm_subs_epi16(x6[1], x6[2]);
  x7[4] = x6[4];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x6[5], x6[6], x7[5], x7[6]);
  x7[7] = x6[7];
  x7[8] = _mm_adds_epi16(x6[8], x6[11]);
  x7[11] = _mm_subs_epi16(x6[8], x6[11]);
  x7[9] = _mm_adds_epi16(x6[9], x6[10]);
  x7[10] = _mm_subs_epi16(x6[9], x6[10]);
  x7[12] = _mm_subs_epi16(x6[15], x6[12]);
  x7[15] = _mm_adds_epi16(x6[12], x6[15]);
  x7[13] = _mm_subs_epi16(x6[14], x6[13]);
  x7[14] = _mm_adds_epi16(x6[13], x6[14]);
  x7[16] = x6[16];
  x7[17] = x6[17];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x6[18], x6[29], x7[18], x7[29]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x6[19], x6[28], x7[19], x7[28]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x6[20], x6[27], x7[20], x7[27]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x6[21], x6[26], x7[21], x7[26]);
  x7[22] = x6[22];
  x7[23] = x6[23];
  x7[24] = x6[24];
  x7[25] = x6[25];
  x7[30] = x6[30];
  x7[31] = x6[31];
  x7[32] = _mm_adds_epi16(x6[32], x6[39]);
  x7[39] = _mm_subs_epi16(x6[32], x6[39]);
  x7[33] = _mm_adds_epi16(x6[33], x6[38]);
  x7[38] = _mm_subs_epi16(x6[33], x6[38]);
  x7[34] = _mm_adds_epi16(x6[34], x6[37]);
  x7[37] = _mm_subs_epi16(x6[34], x6[37]);
  x7[35] = _mm_adds_epi16(x6[35], x6[36]);
  x7[36] = _mm_subs_epi16(x6[35], x6[36]);
  x7[40] = _mm_subs_epi16(x6[47], x6[40]);
  x7[47] = _mm_adds_epi16(x6[40], x6[47]);
  x7[41] = _mm_subs_epi16(x6[46], x6[41]);
  x7[46] = _mm_adds_epi16(x6[41], x6[46]);
  x7[42] = _mm_subs_epi16(x6[45], x6[42]);
  x7[45] = _mm_adds_epi16(x6[42], x6[45]);
  x7[43] = _mm_subs_epi16(x6[44], x6[43]);
  x7[44] = _mm_adds_epi16(x6[43], x6[44]);
  x7[48] = _mm_adds_epi16(x6[48], x6[55]);
  x7[55] = _mm_subs_epi16(x6[48], x6[55]);
  x7[49] = _mm_adds_epi16(x6[49], x6[54]);
  x7[54] = _mm_subs_epi16(x6[49], x6[54]);
  x7[50] = _mm_adds_epi16(x6[50], x6[53]);
  x7[53] = _mm_subs_epi16(x6[50], x6[53]);
  x7[51] = _mm_adds_epi16(x6[51], x6[52]);
  x7[52] = _mm_subs_epi16(x6[51], x6[52]);
  x7[56] = _mm_subs_epi16(x6[63], x6[56]);
  x7[63] = _mm_adds_epi16(x6[56], x6[63]);
  x7[57] = _mm_subs_epi16(x6[62], x6[57]);
  x7[62] = _mm_adds_epi16(x6[57], x6[62]);
  x7[58] = _mm_subs_epi16(x6[61], x6[58]);
  x7[61] = _mm_adds_epi16(x6[58], x6[61]);
  x7[59] = _mm_subs_epi16(x6[60], x6[59]);
  x7[60] = _mm_adds_epi16(x6[59], x6[60]);

  // stage 8
  __m128i x8[64];
  x8[0] = _mm_adds_epi16(x7[0], x7[7]);
  x8[7] = _mm_subs_epi16(x7[0], x7[7]);
  x8[1] = _mm_adds_epi16(x7[1], x7[6]);
  x8[6] = _mm_subs_epi16(x7[1], x7[6]);
  x8[2] = _mm_adds_epi16(x7[2], x7[5]);
  x8[5] = _mm_subs_epi16(x7[2], x7[5]);
  x8[3] = _mm_adds_epi16(x7[3], x7[4]);
  x8[4] = _mm_subs_epi16(x7[3], x7[4]);
  x8[8] = x7[8];
  x8[9] = x7[9];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x7[10], x7[13], x8[10], x8[13]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x7[11], x7[12], x8[11], x8[12]);
  x8[14] = x7[14];
  x8[15] = x7[15];
  x8[16] = _mm_adds_epi16(x7[16], x7[23]);
  x8[23] = _mm_subs_epi16(x7[16], x7[23]);
  x8[17] = _mm_adds_epi16(x7[17], x7[22]);
  x8[22] = _mm_subs_epi16(x7[17], x7[22]);
  x8[18] = _mm_adds_epi16(x7[18], x7[21]);
  x8[21] = _mm_subs_epi16(x7[18], x7[21]);
  x8[19] = _mm_adds_epi16(x7[19], x7[20]);
  x8[20] = _mm_subs_epi16(x7[19], x7[20]);
  x8[24] = _mm_subs_epi16(x7[31], x7[24]);
  x8[31] = _mm_adds_epi16(x7[24], x7[31]);
  x8[25] = _mm_subs_epi16(x7[30], x7[25]);
  x8[30] = _mm_adds_epi16(x7[25], x7[30]);
  x8[26] = _mm_subs_epi16(x7[29], x7[26]);
  x8[29] = _mm_adds_epi16(x7[26], x7[29]);
  x8[27] = _mm_subs_epi16(x7[28], x7[27]);
  x8[28] = _mm_adds_epi16(x7[27], x7[28]);
  x8[32] = x7[32];
  x8[33] = x7[33];
  x8[34] = x7[34];
  x8[35] = x7[35];
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x7[36], x7[59], x8[36], x8[59]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x7[37], x7[58], x8[37], x8[58]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x7[38], x7[57], x8[38], x8[57]);
  btf_16_sse2(cospi_m16_p48, cospi_p48_p16, x7[39], x7[56], x8[39], x8[56]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x7[40], x7[55], x8[40], x8[55]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x7[41], x7[54], x8[41], x8[54]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x7[42], x7[53], x8[42], x8[53]);
  btf_16_sse2(cospi_m48_m16, cospi_m16_p48, x7[43], x7[52], x8[43], x8[52]);
  x8[44] = x7[44];
  x8[45] = x7[45];
  x8[46] = x7[46];
  x8[47] = x7[47];
  x8[48] = x7[48];
  x8[49] = x7[49];
  x8[50] = x7[50];
  x8[51] = x7[51];
  x8[60] = x7[60];
  x8[61] = x7[61];
  x8[62] = x7[62];
  x8[63] = x7[63];

  // stage 9
  __m128i x9[64];
  x9[0] = _mm_adds_epi16(x8[0], x8[15]);
  x9[15] = _mm_subs_epi16(x8[0], x8[15]);
  x9[1] = _mm_adds_epi16(x8[1], x8[14]);
  x9[14] = _mm_subs_epi16(x8[1], x8[14]);
  x9[2] = _mm_adds_epi16(x8[2], x8[13]);
  x9[13] = _mm_subs_epi16(x8[2], x8[13]);
  x9[3] = _mm_adds_epi16(x8[3], x8[12]);
  x9[12] = _mm_subs_epi16(x8[3], x8[12]);
  x9[4] = _mm_adds_epi16(x8[4], x8[11]);
  x9[11] = _mm_subs_epi16(x8[4], x8[11]);
  x9[5] = _mm_adds_epi16(x8[5], x8[10]);
  x9[10] = _mm_subs_epi16(x8[5], x8[10]);
  x9[6] = _mm_adds_epi16(x8[6], x8[9]);
  x9[9] = _mm_subs_epi16(x8[6], x8[9]);
  x9[7] = _mm_adds_epi16(x8[7], x8[8]);
  x9[8] = _mm_subs_epi16(x8[7], x8[8]);
  x9[16] = x8[16];
  x9[17] = x8[17];
  x9[18] = x8[18];
  x9[19] = x8[19];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x8[20], x8[27], x9[20], x9[27]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x8[21], x8[26], x9[21], x9[26]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x8[22], x8[25], x9[22], x9[25]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x8[23], x8[24], x9[23], x9[24]);
  x9[28] = x8[28];
  x9[29] = x8[29];
  x9[30] = x8[30];
  x9[31] = x8[31];
  x9[32] = _mm_adds_epi16(x8[32], x8[47]);
  x9[47] = _mm_subs_epi16(x8[32], x8[47]);
  x9[33] = _mm_adds_epi16(x8[33], x8[46]);
  x9[46] = _mm_subs_epi16(x8[33], x8[46]);
  x9[34] = _mm_adds_epi16(x8[34], x8[45]);
  x9[45] = _mm_subs_epi16(x8[34], x8[45]);
  x9[35] = _mm_adds_epi16(x8[35], x8[44]);
  x9[44] = _mm_subs_epi16(x8[35], x8[44]);
  x9[36] = _mm_adds_epi16(x8[36], x8[43]);
  x9[43] = _mm_subs_epi16(x8[36], x8[43]);
  x9[37] = _mm_adds_epi16(x8[37], x8[42]);
  x9[42] = _mm_subs_epi16(x8[37], x8[42]);
  x9[38] = _mm_adds_epi16(x8[38], x8[41]);
  x9[41] = _mm_subs_epi16(x8[38], x8[41]);
  x9[39] = _mm_adds_epi16(x8[39], x8[40]);
  x9[40] = _mm_subs_epi16(x8[39], x8[40]);
  x9[48] = _mm_subs_epi16(x8[63], x8[48]);
  x9[63] = _mm_adds_epi16(x8[48], x8[63]);
  x9[49] = _mm_subs_epi16(x8[62], x8[49]);
  x9[62] = _mm_adds_epi16(x8[49], x8[62]);
  x9[50] = _mm_subs_epi16(x8[61], x8[50]);
  x9[61] = _mm_adds_epi16(x8[50], x8[61]);
  x9[51] = _mm_subs_epi16(x8[60], x8[51]);
  x9[60] = _mm_adds_epi16(x8[51], x8[60]);
  x9[52] = _mm_subs_epi16(x8[59], x8[52]);
  x9[59] = _mm_adds_epi16(x8[52], x8[59]);
  x9[53] = _mm_subs_epi16(x8[58], x8[53]);
  x9[58] = _mm_adds_epi16(x8[53], x8[58]);
  x9[54] = _mm_subs_epi16(x8[57], x8[54]);
  x9[57] = _mm_adds_epi16(x8[54], x8[57]);
  x9[55] = _mm_subs_epi16(x8[56], x8[55]);
  x9[56] = _mm_adds_epi16(x8[55], x8[56]);

  // stage 10
  __m128i x10[64];
  x10[0] = _mm_adds_epi16(x9[0], x9[31]);
  x10[31] = _mm_subs_epi16(x9[0], x9[31]);
  x10[1] = _mm_adds_epi16(x9[1], x9[30]);
  x10[30] = _mm_subs_epi16(x9[1], x9[30]);
  x10[2] = _mm_adds_epi16(x9[2], x9[29]);
  x10[29] = _mm_subs_epi16(x9[2], x9[29]);
  x10[3] = _mm_adds_epi16(x9[3], x9[28]);
  x10[28] = _mm_subs_epi16(x9[3], x9[28]);
  x10[4] = _mm_adds_epi16(x9[4], x9[27]);
  x10[27] = _mm_subs_epi16(x9[4], x9[27]);
  x10[5] = _mm_adds_epi16(x9[5], x9[26]);
  x10[26] = _mm_subs_epi16(x9[5], x9[26]);
  x10[6] = _mm_adds_epi16(x9[6], x9[25]);
  x10[25] = _mm_subs_epi16(x9[6], x9[25]);
  x10[7] = _mm_adds_epi16(x9[7], x9[24]);
  x10[24] = _mm_subs_epi16(x9[7], x9[24]);
  x10[8] = _mm_adds_epi16(x9[8], x9[23]);
  x10[23] = _mm_subs_epi16(x9[8], x9[23]);
  x10[9] = _mm_adds_epi16(x9[9], x9[22]);
  x10[22] = _mm_subs_epi16(x9[9], x9[22]);
  x10[10] = _mm_adds_epi16(x9[10], x9[21]);
  x10[21] = _mm_subs_epi16(x9[10], x9[21]);
  x10[11] = _mm_adds_epi16(x9[11], x9[20]);
  x10[20] = _mm_subs_epi16(x9[11], x9[20]);
  x10[12] = _mm_adds_epi16(x9[12], x9[19]);
  x10[19] = _mm_subs_epi16(x9[12], x9[19]);
  x10[13] = _mm_adds_epi16(x9[13], x9[18]);
  x10[18] = _mm_subs_epi16(x9[13], x9[18]);
  x10[14] = _mm_adds_epi16(x9[14], x9[17]);
  x10[17] = _mm_subs_epi16(x9[14], x9[17]);
  x10[15] = _mm_adds_epi16(x9[15], x9[16]);
  x10[16] = _mm_subs_epi16(x9[15], x9[16]);
  x10[32] = x9[32];
  x10[33] = x9[33];
  x10[34] = x9[34];
  x10[35] = x9[35];
  x10[36] = x9[36];
  x10[37] = x9[37];
  x10[38] = x9[38];
  x10[39] = x9[39];
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[40], x9[55], x10[40], x10[55]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[41], x9[54], x10[41], x10[54]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[42], x9[53], x10[42], x10[53]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[43], x9[52], x10[43], x10[52]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[44], x9[51], x10[44], x10[51]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[45], x9[50], x10[45], x10[50]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[46], x9[49], x10[46], x10[49]);
  btf_16_sse2(cospi_m32_p32, cospi_p32_p32, x9[47], x9[48], x10[47], x10[48]);
  x10[56] = x9[56];
  x10[57] = x9[57];
  x10[58] = x9[58];
  x10[59] = x9[59];
  x10[60] = x9[60];
  x10[61] = x9[61];
  x10[62] = x9[62];
  x10[63] = x9[63];

  // stage 11
  output[0] = _mm_adds_epi16(x10[0], x10[63]);
  output[63] = _mm_subs_epi16(x10[0], x10[63]);
  output[1] = _mm_adds_epi16(x10[1], x10[62]);
  output[62] = _mm_subs_epi16(x10[1], x10[62]);
  output[2] = _mm_adds_epi16(x10[2], x10[61]);
  output[61] = _mm_subs_epi16(x10[2], x10[61]);
  output[3] = _mm_adds_epi16(x10[3], x10[60]);
  output[60] = _mm_subs_epi16(x10[3], x10[60]);
  output[4] = _mm_adds_epi16(x10[4], x10[59]);
  output[59] = _mm_subs_epi16(x10[4], x10[59]);
  output[5] = _mm_adds_epi16(x10[5], x10[58]);
  output[58] = _mm_subs_epi16(x10[5], x10[58]);
  output[6] = _mm_adds_epi16(x10[6], x10[57]);
  output[57] = _mm_subs_epi16(x10[6], x10[57]);
  output[7] = _mm_adds_epi16(x10[7], x10[56]);
  output[56] = _mm_subs_epi16(x10[7], x10[56]);
  output[8] = _mm_adds_epi16(x10[8], x10[55]);
  output[55] = _mm_subs_epi16(x10[8], x10[55]);
  output[9] = _mm_adds_epi16(x10[9], x10[54]);
  output[54] = _mm_subs_epi16(x10[9], x10[54]);
  output[10] = _mm_adds_epi16(x10[10], x10[53]);
  output[53] = _mm_subs_epi16(x10[10], x10[53]);
  output[11] = _mm_adds_epi16(x10[11], x10[52]);
  output[52] = _mm_subs_epi16(x10[11], x10[52]);
  output[12] = _mm_adds_epi16(x10[12], x10[51]);
  output[51] = _mm_subs_epi16(x10[12], x10[51]);
  output[13] = _mm_adds_epi16(x10[13], x10[50]);
  output[50] = _mm_subs_epi16(x10[13], x10[50]);
  output[14] = _mm_adds_epi16(x10[14], x10[49]);
  output[49] = _mm_subs_epi16(x10[14], x10[49]);
  output[15] = _mm_adds_epi16(x10[15], x10[48]);
  output[48] = _mm_subs_epi16(x10[15], x10[48]);
  output[16] = _mm_adds_epi16(x10[16], x10[47]);
  output[47] = _mm_subs_epi16(x10[16], x10[47]);
  output[17] = _mm_adds_epi16(x10[17], x10[46]);
  output[46] = _mm_subs_epi16(x10[17], x10[46]);
  output[18] = _mm_adds_epi16(x10[18], x10[45]);
  output[45] = _mm_subs_epi16(x10[18], x10[45]);
  output[19] = _mm_adds_epi16(x10[19], x10[44]);
  output[44] = _mm_subs_epi16(x10[19], x10[44]);
  output[20] = _mm_adds_epi16(x10[20], x10[43]);
  output[43] = _mm_subs_epi16(x10[20], x10[43]);
  output[21] = _mm_adds_epi16(x10[21], x10[42]);
  output[42] = _mm_subs_epi16(x10[21], x10[42]);
  output[22] = _mm_adds_epi16(x10[22], x10[41]);
  output[41] = _mm_subs_epi16(x10[22], x10[41]);
  output[23] = _mm_adds_epi16(x10[23], x10[40]);
  output[40] = _mm_subs_epi16(x10[23], x10[40]);
  output[24] = _mm_adds_epi16(x10[24], x10[39]);
  output[39] = _mm_subs_epi16(x10[24], x10[39]);
  output[25] = _mm_adds_epi16(x10[25], x10[38]);
  output[38] = _mm_subs_epi16(x10[25], x10[38]);
  output[26] = _mm_adds_epi16(x10[26], x10[37]);
  output[37] = _mm_subs_epi16(x10[26], x10[37]);
  output[27] = _mm_adds_epi16(x10[27], x10[36]);
  output[36] = _mm_subs_epi16(x10[27], x10[36]);
  output[28] = _mm_adds_epi16(x10[28], x10[35]);
  output[35] = _mm_subs_epi16(x10[28], x10[35]);
  output[29] = _mm_adds_epi16(x10[29], x10[34]);
  output[34] = _mm_subs_epi16(x10[29], x10[34]);
  output[30] = _mm_adds_epi16(x10[30], x10[33]);
  output[33] = _mm_subs_epi16(x10[30], x10[33]);
  output[31] = _mm_adds_epi16(x10[31], x10[32]);
  output[32] = _mm_subs_epi16(x10[31], x10[32]);
}

void iadst4_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *sinpi = sinpi_arr(INV_COS_BIT);
  const __m128i sinpi_p01_p04 = pair_set_epi16(sinpi[1], sinpi[4]);
  const __m128i sinpi_p02_m01 = pair_set_epi16(sinpi[2], -sinpi[1]);
  const __m128i sinpi_p03_p02 = pair_set_epi16(sinpi[3], sinpi[2]);
  const __m128i sinpi_p03_m04 = pair_set_epi16(sinpi[3], -sinpi[4]);
  const __m128i sinpi_p03_m03 = pair_set_epi16(sinpi[3], -sinpi[3]);
  const __m128i sinpi_0_p03 = pair_set_epi16(0, sinpi[3]);
  const __m128i sinpi_0_p02 = pair_set_epi16(0, sinpi[2]);
  const __m128i sinpi_p03_p04 = pair_set_epi16(sinpi[3], sinpi[4]);
  __m128i x0[4];
  x0[0] = input[0];
  x0[1] = input[1];
  x0[2] = input[2];
  x0[3] = input[3];

  __m128i u[4];
  u[0] = _mm_unpacklo_epi16(x0[0], x0[2]);
  u[1] = _mm_unpackhi_epi16(x0[0], x0[2]);
  u[2] = _mm_unpacklo_epi16(x0[1], x0[3]);
  u[3] = _mm_unpackhi_epi16(x0[1], x0[3]);

  __m128i x1[16];
  x1[0] = _mm_madd_epi16(u[0], sinpi_p01_p04);  // x0*sin1 + x2*sin4
  x1[1] = _mm_madd_epi16(u[1], sinpi_p01_p04);
  x1[2] = _mm_madd_epi16(u[0], sinpi_p02_m01);  // x0*sin2 - x2*sin1
  x1[3] = _mm_madd_epi16(u[1], sinpi_p02_m01);
  x1[4] = _mm_madd_epi16(u[2], sinpi_p03_p02);  // x1*sin3 + x3*sin2
  x1[5] = _mm_madd_epi16(u[3], sinpi_p03_p02);
  x1[6] = _mm_madd_epi16(u[2], sinpi_p03_m04);  // x1*sin3 - x3*sin4
  x1[7] = _mm_madd_epi16(u[3], sinpi_p03_m04);
  x1[8] = _mm_madd_epi16(u[0], sinpi_p03_m03);  // x0*sin3 - x2*sin3
  x1[9] = _mm_madd_epi16(u[1], sinpi_p03_m03);
  x1[10] = _mm_madd_epi16(u[2], sinpi_0_p03);  // x2*sin3
  x1[11] = _mm_madd_epi16(u[3], sinpi_0_p03);
  x1[12] = _mm_madd_epi16(u[2], sinpi_0_p02);  // x3*sin2
  x1[13] = _mm_madd_epi16(u[3], sinpi_0_p02);
  x1[14] = _mm_madd_epi16(u[2], sinpi_p03_p04);  // x1*sin3 + x3*sin4
  x1[15] = _mm_madd_epi16(u[3], sinpi_p03_p04);

  __m128i x2[8];
  x2[0] = _mm_add_epi32(x1[0], x1[4]);  // x0*sin1 + x2*sin4 + x1*sin3 + x3*sin2
  x2[1] = _mm_add_epi32(x1[1], x1[5]);
  x2[2] = _mm_add_epi32(x1[2], x1[6]);  // x0*sin2 - x2*sin1 +x1*sin3 - x3*sin4
  x2[3] = _mm_add_epi32(x1[3], x1[7]);
  x2[4] = _mm_add_epi32(x1[8], x1[10]);  // x0*sin3 - x2*sin3 +x3*sin3
  x2[5] = _mm_add_epi32(x1[9], x1[11]);
  x2[6] = _mm_add_epi32(x1[0], x1[2]);  // x0*sin1 + x2*sin4 + x0*sin2 - x2*sin1
  x2[7] = _mm_add_epi32(x1[1], x1[3]);
  // x0*sin1 + x2*sin4 + x3*sin2 + x0*sin2 - x2*sin1
  x2[6] = _mm_add_epi32(x2[6], x1[12]);
  x2[7] = _mm_add_epi32(x2[7], x1[13]);
  // x0*sin1 + x2*sin4 + x3*sin2 + x0*sin2 - x2*sin1
  x2[6] = _mm_sub_epi32(x2[6], x1[14]);
  x2[7] = _mm_sub_epi32(x2[7], x1[15]);

  const __m128i rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));
  for (int i = 0; i < 4; ++i) {
    __m128i out0 = _mm_add_epi32(x2[2 * i], rounding);
    __m128i out1 = _mm_add_epi32(x2[2 * i + 1], rounding);
    out0 = _mm_srai_epi32(out0, INV_COS_BIT);
    out1 = _mm_srai_epi32(out1, INV_COS_BIT);
    output[i] = _mm_packs_epi32(out0, out1);
  }
}

void iadst8_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p04_p60 = pair_set_epi16(cospi[4], cospi[60]);
  __m128i cospi_p60_m04 = pair_set_epi16(cospi[60], -cospi[4]);
  __m128i cospi_p20_p44 = pair_set_epi16(cospi[20], cospi[44]);
  __m128i cospi_p44_m20 = pair_set_epi16(cospi[44], -cospi[20]);
  __m128i cospi_p36_p28 = pair_set_epi16(cospi[36], cospi[28]);
  __m128i cospi_p28_m36 = pair_set_epi16(cospi[28], -cospi[36]);
  __m128i cospi_p52_p12 = pair_set_epi16(cospi[52], cospi[12]);
  __m128i cospi_p12_m52 = pair_set_epi16(cospi[12], -cospi[52]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m128i x1[8];
  x1[0] = input[7];
  x1[1] = input[0];
  x1[2] = input[5];
  x1[3] = input[2];
  x1[4] = input[3];
  x1[5] = input[4];
  x1[6] = input[1];
  x1[7] = input[6];

  // stage 2
  __m128i x2[8];
  btf_16_sse2(cospi_p04_p60, cospi_p60_m04, x1[0], x1[1], x2[0], x2[1]);
  btf_16_sse2(cospi_p20_p44, cospi_p44_m20, x1[2], x1[3], x2[2], x2[3]);
  btf_16_sse2(cospi_p36_p28, cospi_p28_m36, x1[4], x1[5], x2[4], x2[5]);
  btf_16_sse2(cospi_p52_p12, cospi_p12_m52, x1[6], x1[7], x2[6], x2[7]);

  // stage 3
  __m128i x3[8];
  x3[0] = _mm_adds_epi16(x2[0], x2[4]);
  x3[4] = _mm_subs_epi16(x2[0], x2[4]);
  x3[1] = _mm_adds_epi16(x2[1], x2[5]);
  x3[5] = _mm_subs_epi16(x2[1], x2[5]);
  x3[2] = _mm_adds_epi16(x2[2], x2[6]);
  x3[6] = _mm_subs_epi16(x2[2], x2[6]);
  x3[3] = _mm_adds_epi16(x2[3], x2[7]);
  x3[7] = _mm_subs_epi16(x2[3], x2[7]);

  // stage 4
  __m128i x4[8];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x3[4], x3[5], x4[4], x4[5]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x3[6], x3[7], x4[6], x4[7]);

  // stage 5
  __m128i x5[8];
  x5[0] = _mm_adds_epi16(x4[0], x4[2]);
  x5[2] = _mm_subs_epi16(x4[0], x4[2]);
  x5[1] = _mm_adds_epi16(x4[1], x4[3]);
  x5[3] = _mm_subs_epi16(x4[1], x4[3]);
  x5[4] = _mm_adds_epi16(x4[4], x4[6]);
  x5[6] = _mm_subs_epi16(x4[4], x4[6]);
  x5[5] = _mm_adds_epi16(x4[5], x4[7]);
  x5[7] = _mm_subs_epi16(x4[5], x4[7]);

  // stage 6
  __m128i x6[8];
  x6[0] = x5[0];
  x6[1] = x5[1];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x5[2], x5[3], x6[2], x6[3]);
  x6[4] = x5[4];
  x6[5] = x5[5];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x5[6], x5[7], x6[6], x6[7]);

  // stage 7
  output[0] = x6[0];
  output[1] = _mm_subs_epi16(__zero, x6[4]);
  output[2] = x6[6];
  output[3] = _mm_subs_epi16(__zero, x6[2]);
  output[4] = x6[3];
  output[5] = _mm_subs_epi16(__zero, x6[7]);
  output[6] = x6[5];
  output[7] = _mm_subs_epi16(__zero, x6[1]);
}

void iadst16_new_sse2(const __m128i *input, __m128i *output, int8_t cos_bit) {
  (void)(cos_bit);
  const int32_t *cospi = cospi_arr(INV_COS_BIT);
  const __m128i __zero = _mm_setzero_si128();
  const __m128i __rounding = _mm_set1_epi32(1 << (INV_COS_BIT - 1));

  __m128i cospi_p02_p62 = pair_set_epi16(cospi[2], cospi[62]);
  __m128i cospi_p62_m02 = pair_set_epi16(cospi[62], -cospi[2]);
  __m128i cospi_p10_p54 = pair_set_epi16(cospi[10], cospi[54]);
  __m128i cospi_p54_m10 = pair_set_epi16(cospi[54], -cospi[10]);
  __m128i cospi_p18_p46 = pair_set_epi16(cospi[18], cospi[46]);
  __m128i cospi_p46_m18 = pair_set_epi16(cospi[46], -cospi[18]);
  __m128i cospi_p26_p38 = pair_set_epi16(cospi[26], cospi[38]);
  __m128i cospi_p38_m26 = pair_set_epi16(cospi[38], -cospi[26]);
  __m128i cospi_p34_p30 = pair_set_epi16(cospi[34], cospi[30]);
  __m128i cospi_p30_m34 = pair_set_epi16(cospi[30], -cospi[34]);
  __m128i cospi_p42_p22 = pair_set_epi16(cospi[42], cospi[22]);
  __m128i cospi_p22_m42 = pair_set_epi16(cospi[22], -cospi[42]);
  __m128i cospi_p50_p14 = pair_set_epi16(cospi[50], cospi[14]);
  __m128i cospi_p14_m50 = pair_set_epi16(cospi[14], -cospi[50]);
  __m128i cospi_p58_p06 = pair_set_epi16(cospi[58], cospi[6]);
  __m128i cospi_p06_m58 = pair_set_epi16(cospi[6], -cospi[58]);
  __m128i cospi_p08_p56 = pair_set_epi16(cospi[8], cospi[56]);
  __m128i cospi_p56_m08 = pair_set_epi16(cospi[56], -cospi[8]);
  __m128i cospi_p40_p24 = pair_set_epi16(cospi[40], cospi[24]);
  __m128i cospi_p24_m40 = pair_set_epi16(cospi[24], -cospi[40]);
  __m128i cospi_m56_p08 = pair_set_epi16(-cospi[56], cospi[8]);
  __m128i cospi_m24_p40 = pair_set_epi16(-cospi[24], cospi[40]);
  __m128i cospi_p16_p48 = pair_set_epi16(cospi[16], cospi[48]);
  __m128i cospi_p48_m16 = pair_set_epi16(cospi[48], -cospi[16]);
  __m128i cospi_m48_p16 = pair_set_epi16(-cospi[48], cospi[16]);
  __m128i cospi_p32_p32 = pair_set_epi16(cospi[32], cospi[32]);
  __m128i cospi_p32_m32 = pair_set_epi16(cospi[32], -cospi[32]);

  // stage 1
  __m128i x1[16];
  x1[0] = input[15];
  x1[1] = input[0];
  x1[2] = input[13];
  x1[3] = input[2];
  x1[4] = input[11];
  x1[5] = input[4];
  x1[6] = input[9];
  x1[7] = input[6];
  x1[8] = input[7];
  x1[9] = input[8];
  x1[10] = input[5];
  x1[11] = input[10];
  x1[12] = input[3];
  x1[13] = input[12];
  x1[14] = input[1];
  x1[15] = input[14];

  // stage 2
  __m128i x2[16];
  btf_16_sse2(cospi_p02_p62, cospi_p62_m02, x1[0], x1[1], x2[0], x2[1]);
  btf_16_sse2(cospi_p10_p54, cospi_p54_m10, x1[2], x1[3], x2[2], x2[3]);
  btf_16_sse2(cospi_p18_p46, cospi_p46_m18, x1[4], x1[5], x2[4], x2[5]);
  btf_16_sse2(cospi_p26_p38, cospi_p38_m26, x1[6], x1[7], x2[6], x2[7]);
  btf_16_sse2(cospi_p34_p30, cospi_p30_m34, x1[8], x1[9], x2[8], x2[9]);
  btf_16_sse2(cospi_p42_p22, cospi_p22_m42, x1[10], x1[11], x2[10], x2[11]);
  btf_16_sse2(cospi_p50_p14, cospi_p14_m50, x1[12], x1[13], x2[12], x2[13]);
  btf_16_sse2(cospi_p58_p06, cospi_p06_m58, x1[14], x1[15], x2[14], x2[15]);

  // stage 3
  __m128i x3[16];
  x3[0] = _mm_adds_epi16(x2[0], x2[8]);
  x3[8] = _mm_subs_epi16(x2[0], x2[8]);
  x3[1] = _mm_adds_epi16(x2[1], x2[9]);
  x3[9] = _mm_subs_epi16(x2[1], x2[9]);
  x3[2] = _mm_adds_epi16(x2[2], x2[10]);
  x3[10] = _mm_subs_epi16(x2[2], x2[10]);
  x3[3] = _mm_adds_epi16(x2[3], x2[11]);
  x3[11] = _mm_subs_epi16(x2[3], x2[11]);
  x3[4] = _mm_adds_epi16(x2[4], x2[12]);
  x3[12] = _mm_subs_epi16(x2[4], x2[12]);
  x3[5] = _mm_adds_epi16(x2[5], x2[13]);
  x3[13] = _mm_subs_epi16(x2[5], x2[13]);
  x3[6] = _mm_adds_epi16(x2[6], x2[14]);
  x3[14] = _mm_subs_epi16(x2[6], x2[14]);
  x3[7] = _mm_adds_epi16(x2[7], x2[15]);
  x3[15] = _mm_subs_epi16(x2[7], x2[15]);

  // stage 4
  __m128i x4[16];
  x4[0] = x3[0];
  x4[1] = x3[1];
  x4[2] = x3[2];
  x4[3] = x3[3];
  x4[4] = x3[4];
  x4[5] = x3[5];
  x4[6] = x3[6];
  x4[7] = x3[7];
  btf_16_sse2(cospi_p08_p56, cospi_p56_m08, x3[8], x3[9], x4[8], x4[9]);
  btf_16_sse2(cospi_p40_p24, cospi_p24_m40, x3[10], x3[11], x4[10], x4[11]);
  btf_16_sse2(cospi_m56_p08, cospi_p08_p56, x3[12], x3[13], x4[12], x4[13]);
  btf_16_sse2(cospi_m24_p40, cospi_p40_p24, x3[14], x3[15], x4[14], x4[15]);

  // stage 5
  __m128i x5[16];
  x5[0] = _mm_adds_epi16(x4[0], x4[4]);
  x5[4] = _mm_subs_epi16(x4[0], x4[4]);
  x5[1] = _mm_adds_epi16(x4[1], x4[5]);
  x5[5] = _mm_subs_epi16(x4[1], x4[5]);
  x5[2] = _mm_adds_epi16(x4[2], x4[6]);
  x5[6] = _mm_subs_epi16(x4[2], x4[6]);
  x5[3] = _mm_adds_epi16(x4[3], x4[7]);
  x5[7] = _mm_subs_epi16(x4[3], x4[7]);
  x5[8] = _mm_adds_epi16(x4[8], x4[12]);
  x5[12] = _mm_subs_epi16(x4[8], x4[12]);
  x5[9] = _mm_adds_epi16(x4[9], x4[13]);
  x5[13] = _mm_subs_epi16(x4[9], x4[13]);
  x5[10] = _mm_adds_epi16(x4[10], x4[14]);
  x5[14] = _mm_subs_epi16(x4[10], x4[14]);
  x5[11] = _mm_adds_epi16(x4[11], x4[15]);
  x5[15] = _mm_subs_epi16(x4[11], x4[15]);

  // stage 6
  __m128i x6[16];
  x6[0] = x5[0];
  x6[1] = x5[1];
  x6[2] = x5[2];
  x6[3] = x5[3];
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x5[4], x5[5], x6[4], x6[5]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x5[6], x5[7], x6[6], x6[7]);
  x6[8] = x5[8];
  x6[9] = x5[9];
  x6[10] = x5[10];
  x6[11] = x5[11];
  btf_16_sse2(cospi_p16_p48, cospi_p48_m16, x5[12], x5[13], x6[12], x6[13]);
  btf_16_sse2(cospi_m48_p16, cospi_p16_p48, x5[14], x5[15], x6[14], x6[15]);

  // stage 7
  __m128i x7[16];
  x7[0] = _mm_adds_epi16(x6[0], x6[2]);
  x7[2] = _mm_subs_epi16(x6[0], x6[2]);
  x7[1] = _mm_adds_epi16(x6[1], x6[3]);
  x7[3] = _mm_subs_epi16(x6[1], x6[3]);
  x7[4] = _mm_adds_epi16(x6[4], x6[6]);
  x7[6] = _mm_subs_epi16(x6[4], x6[6]);
  x7[5] = _mm_adds_epi16(x6[5], x6[7]);
  x7[7] = _mm_subs_epi16(x6[5], x6[7]);
  x7[8] = _mm_adds_epi16(x6[8], x6[10]);
  x7[10] = _mm_subs_epi16(x6[8], x6[10]);
  x7[9] = _mm_adds_epi16(x6[9], x6[11]);
  x7[11] = _mm_subs_epi16(x6[9], x6[11]);
  x7[12] = _mm_adds_epi16(x6[12], x6[14]);
  x7[14] = _mm_subs_epi16(x6[12], x6[14]);
  x7[13] = _mm_adds_epi16(x6[13], x6[15]);
  x7[15] = _mm_subs_epi16(x6[13], x6[15]);

  // stage 8
  __m128i x8[16];
  x8[0] = x7[0];
  x8[1] = x7[1];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x7[2], x7[3], x8[2], x8[3]);
  x8[4] = x7[4];
  x8[5] = x7[5];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x7[6], x7[7], x8[6], x8[7]);
  x8[8] = x7[8];
  x8[9] = x7[9];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x7[10], x7[11], x8[10], x8[11]);
  x8[12] = x7[12];
  x8[13] = x7[13];
  btf_16_sse2(cospi_p32_p32, cospi_p32_m32, x7[14], x7[15], x8[14], x8[15]);

  // stage 9
  output[0] = x8[0];
  output[1] = _mm_subs_epi16(__zero, x8[8]);
  output[2] = x8[12];
  output[3] = _mm_subs_epi16(__zero, x8[4]);
  output[4] = x8[6];
  output[5] = _mm_subs_epi16(__zero, x8[14]);
  output[6] = x8[10];
  output[7] = _mm_subs_epi16(__zero, x8[2]);
  output[8] = x8[3];
  output[9] = _mm_subs_epi16(__zero, x8[11]);
  output[10] = x8[15];
  output[11] = _mm_subs_epi16(__zero, x8[7]);
  output[12] = x8[5];
  output[13] = _mm_subs_epi16(__zero, x8[13]);
  output[14] = x8[9];
  output[15] = _mm_subs_epi16(__zero, x8[1]);
}

static void iidentity4_new_sse2(const __m128i *input, __m128i *output,
                                int8_t cos_bit) {
  (void)cos_bit;
  const __m128i scale = _mm_set1_epi16(NewSqrt2);
  const __m128i rounding = _mm_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i scale_rounding = _mm_unpacklo_epi16(scale, rounding);
  for (int i = 0; i < 4; ++i) {
    __m128i a_lo = _mm_unpacklo_epi16(input[i], one);
    __m128i a_hi = _mm_unpackhi_epi16(input[i], one);
    __m128i b_lo = _mm_madd_epi16(a_lo, scale_rounding);
    __m128i b_hi = _mm_madd_epi16(a_hi, scale_rounding);
    __m128i c_lo = _mm_srai_epi32(b_lo, NewSqrt2Bits);
    __m128i c_hi = _mm_srai_epi32(b_hi, NewSqrt2Bits);
    output[i] = _mm_packs_epi32(c_lo, c_hi);
  }
}

static void iidentity8_new_sse2(const __m128i *input, __m128i *output,
                                int8_t cos_bit) {
  (void)cos_bit;
  for (int i = 0; i < 8; ++i) {
    output[i] = _mm_adds_epi16(input[i], input[i]);
  }
}

static void iidentity16_new_sse2(const __m128i *input, __m128i *output,
                                 int8_t cos_bit) {
  (void)cos_bit;
  const __m128i scale = _mm_set1_epi16(2 * NewSqrt2);
  const __m128i rounding = _mm_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i scale_rounding = _mm_unpacklo_epi16(scale, rounding);
  for (int i = 0; i < 16; ++i) {
    __m128i a_lo = _mm_unpacklo_epi16(input[i], one);
    __m128i a_hi = _mm_unpackhi_epi16(input[i], one);
    __m128i b_lo = _mm_madd_epi16(a_lo, scale_rounding);
    __m128i b_hi = _mm_madd_epi16(a_hi, scale_rounding);
    __m128i c_lo = _mm_srai_epi32(b_lo, NewSqrt2Bits);
    __m128i c_hi = _mm_srai_epi32(b_hi, NewSqrt2Bits);
    output[i] = _mm_packs_epi32(c_lo, c_hi);
  }
}

static void iidentity32_new_sse2(const __m128i *input, __m128i *output,
                                 int8_t cos_bit) {
  (void)cos_bit;
  for (int i = 0; i < 32; ++i) {
    output[i] = _mm_adds_epi16(input[i], input[i]);
    output[i] = _mm_adds_epi16(output[i], output[i]);
  }
}

#if CONFIG_TX64X64
static void iidentity64_new_sse2(const __m128i *input, __m128i *output,
                                 int8_t cos_bit) {
  (void)cos_bit;
  const __m128i scale = _mm_set1_epi16(4 * NewSqrt2);
  const __m128i rounding = _mm_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i scale_rounding = _mm_unpacklo_epi16(scale, rounding);
  for (int i = 0; i < 64; ++i) {
    __m128i a_lo = _mm_unpacklo_epi16(input[i], one);
    __m128i a_hi = _mm_unpackhi_epi16(input[i], one);
    __m128i b_lo = _mm_madd_epi16(a_lo, scale_rounding);
    __m128i b_hi = _mm_madd_epi16(a_hi, scale_rounding);
    __m128i c_lo = _mm_srai_epi32(b_lo, NewSqrt2Bits);
    __m128i c_hi = _mm_srai_epi32(b_hi, NewSqrt2Bits);
    output[i] = _mm_packs_epi32(c_lo, c_hi);
  }
}
#endif

static INLINE __m128i lowbd_get_recon_8x8_sse2(const __m128i pred,
                                               __m128i res) {
  const __m128i zero = _mm_setzero_si128();
  __m128i x0 = _mm_adds_epi16(res, _mm_unpacklo_epi8(pred, zero));
  return _mm_packus_epi16(x0, x0);
}

static INLINE void lowbd_write_buffer_4xn_sse2(__m128i *in, uint8_t *output,
                                               int stride, int flipud,
                                               const int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  const __m128i zero = _mm_setzero_si128();
  for (int i = 0; i < height; ++i, j += step) {
    const __m128i v = _mm_cvtsi32_si128(*((uint32_t *)(output + i * stride)));
    __m128i u = _mm_adds_epi16(in[j], _mm_unpacklo_epi8(v, zero));
    u = _mm_packus_epi16(u, zero);
    *((uint32_t *)(output + i * stride)) = _mm_cvtsi128_si32(u);
  }
}

static INLINE void lowbd_write_buffer_8xn_sse2(__m128i *in, uint8_t *output,
                                               int stride, int flipud,
                                               const int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    const __m128i v = _mm_loadl_epi64((__m128i const *)(output + i * stride));
    const __m128i u = lowbd_get_recon_8x8_sse2(v, in[j]);
    _mm_storel_epi64((__m128i *)(output + i * stride), u);
  }
}

static const transform_1d_sse2 lowbd_txfm_all_1d_arr[TX_SIZES][TX_TYPES_1D] = {
  { idct4_new_sse2, iadst4_new_sse2, iadst4_new_sse2, iidentity4_new_sse2 },
  { idct8_new_sse2, iadst8_new_sse2, iadst8_new_sse2, iidentity8_new_sse2 },
  { idct16_new_sse2, iadst16_new_sse2, iadst16_new_sse2, iidentity16_new_sse2 },
  { idct32_new_sse2, NULL, NULL, iidentity32_new_sse2 },
#if CONFIG_TX64X64
  { idct64_new_sse2, NULL, NULL, iidentity64_new_sse2 },
#endif
};

// TODO(binpengsmail@gmail.com): Replace 1D txfm functions with functions which
// process 4 pixels at one time. Currently use functions which process 8 pixels
// at one time.
void av1_lowbd_inv_txfm2d_add_4x4_sse2(const int32_t *input, uint8_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf[4];
  const TX_SIZE tx_size = TX_4X4;
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit_w4(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_4x4(buf, buf);
  row_txfm(buf, buf, cos_bit_row);
  if (lr_flip) {
    __m128i temp[4];
    flip_buf_sse2(buf, temp, txfm_size_col);
    transpose_16bit_4x4(temp, buf);
  } else {
    transpose_16bit_4x4(buf, buf);
  }
  col_txfm(buf, buf, cos_bit_col);
  round_shift_16bit(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_4xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

void av1_lowbd_inv_txfm2d_add_8x8_sse2(const int32_t *input, uint8_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf[8];
  const TX_SIZE tx_size = TX_8X8;
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_8x8(buf, buf);
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, 8, shift[0]);
  if (lr_flip) {
    __m128i temp[8];
    flip_buf_sse2(buf, temp, 8);
    transpose_16bit_8x8(temp, buf);
  } else {
    transpose_16bit_8x8(buf, buf);
  }
  col_txfm(buf, buf, cos_bit_col);
  round_shift_16bit(buf, 8, shift[1]);
  lowbd_write_buffer_8xn_sse2(buf, output, stride, ud_flip, 8);
}

static INLINE __m128i lowbd_get_recon_16x16_sse2(const __m128i pred,
                                                 __m128i res0, __m128i res1) {
  const __m128i zero = _mm_setzero_si128();
  __m128i x0 = _mm_unpacklo_epi8(pred, zero);
  __m128i x1 = _mm_unpackhi_epi8(pred, zero);
  x0 = _mm_adds_epi16(res0, x0);
  x1 = _mm_adds_epi16(res1, x1);
  return _mm_packus_epi16(x0, x1);
}

static void lowbd_write_buffer_16xn_sse2(__m128i *in, uint8_t *output,
                                         int stride, int flipud, int height) {
  int j = flipud ? (height - 1) : 0;
  const int step = flipud ? -1 : 1;
  for (int i = 0; i < height; ++i, j += step) {
    __m128i v = _mm_loadu_si128((__m128i const *)(output + i * stride));
    __m128i u = lowbd_get_recon_16x16_sse2(v, in[j], in[j + height]);
    _mm_storeu_si128((__m128i *)(output + i * stride), u);
  }
}

static INLINE void round_shift_sse2(const __m128i *input, __m128i *output,
                                    int size) {
  const __m128i scale = _mm_set1_epi16(NewInvSqrt2);
  const __m128i rounding = _mm_set1_epi16(1 << (NewSqrt2Bits - 1));
  const __m128i one = _mm_set1_epi16(1);
  const __m128i scale_rounding = _mm_unpacklo_epi16(scale, rounding);
  for (int i = 0; i < size; ++i) {
    __m128i a_lo = _mm_unpacklo_epi16(input[i], one);
    __m128i a_hi = _mm_unpackhi_epi16(input[i], one);
    __m128i b_lo = _mm_madd_epi16(a_lo, scale_rounding);
    __m128i b_hi = _mm_madd_epi16(a_hi, scale_rounding);
    __m128i c_lo = _mm_srai_epi32(b_lo, NewSqrt2Bits);
    __m128i c_hi = _mm_srai_epi32(b_hi, NewSqrt2Bits);
    output[i] = _mm_packs_epi32(c_lo, c_hi);
  }
}

static INLINE void lowbd_inv_txfm2d_add_internal_sse2(const int32_t *input,
                                                      uint8_t *output,
                                                      int stride,
                                                      TX_TYPE tx_type,
                                                      TX_SIZE tx_size) {
  __m128i buf1[64 * 8];
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;
  const int buf_size_h_div8 = txfm_size_row >> 3;
  const int rect_type = get_rect_tx_log_ratio(txfm_size_col, txfm_size_row);

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  assert(col_txfm != NULL);
  assert(row_txfm != NULL);
  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  for (int i = 0; i < buf_size_h_div8; i++) {
    __m128i buf0[64];
    const int32_t *input_row = input + i * txfm_size_col * 8;
    for (int j = 0; j < buf_size_w_div8; ++j) {
      __m128i *buf0_cur = buf0 + j * 8;
      load_buffer_32bit_to_16bit(input_row + j * 8, txfm_size_col, buf0_cur, 8);
      transpose_16bit_8x8(buf0_cur, buf0_cur);
    }
    if (rect_type == 1 || rect_type == -1) {
      round_shift_sse2(buf0, buf0, txfm_size_col);  // rect special code
    }
    row_txfm(buf0, buf0, cos_bit_row);
    round_shift_16bit(buf0, txfm_size_col, shift[0]);
    __m128i *_buf1 = buf1 + i * 8;
    if (lr_flip) {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        __m128i temp[8];
        flip_buf_sse2(buf0 + 8 * j, temp, 8);
        transpose_16bit_8x8(temp,
                            _buf1 + txfm_size_row * (buf_size_w_div8 - 1 - j));
      }
    } else {
      for (int j = 0; j < buf_size_w_div8; ++j) {
        transpose_16bit_8x8(buf0 + 8 * j, _buf1 + txfm_size_row * j);
      }
    }
  }
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf1 + i * txfm_size_row, buf1 + i * txfm_size_row, cos_bit_col);
    round_shift_16bit(buf1 + i * txfm_size_row, txfm_size_row, shift[1]);
  }

  if (txfm_size_col >= 16) {
    for (int i = 0; i < (txfm_size_col >> 4); i++) {
      lowbd_write_buffer_16xn_sse2(buf1 + i * txfm_size_row * 2,
                                   output + 16 * i, stride, ud_flip,
                                   txfm_size_row);
    }
  } else if (txfm_size_col == 8) {
    lowbd_write_buffer_8xn_sse2(buf1, output, stride, ud_flip, txfm_size_row);
  }
}

void av1_lowbd_inv_txfm2d_add_16x16_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_16X16);
}

void av1_lowbd_inv_txfm2d_add_32x32_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_32X32);
}

#if CONFIG_TX64X64
void av1_lowbd_inv_txfm2d_add_64x64_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  // TODO(binpengsmail@gmail.com):
  // Potential optimization to take advantage of zeros outside
  // of the top-left block, and same for all other TX_SIZE with 64

  // Remap 32x32 input into a modified 64x64 by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  int32_t mod_input[64 * 64];
  for (int row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  memset(mod_input + 32 * 64, 0, 32 * 64 * sizeof(*mod_input));
  lowbd_inv_txfm2d_add_internal_sse2(mod_input, output, stride, tx_type,
                                     TX_64X64);
}
#endif

void av1_lowbd_inv_txfm2d_add_4x8_sse2(const int32_t *input, uint8_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf[8];
  const TX_SIZE tx_size = TX_4X8;
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit_w4(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_4x8(buf, buf);
  round_shift_sse2(buf, buf, txfm_size_col);  // rect special code
  row_txfm(buf, buf, cos_bit_row);
  // round_shift_16bit(buf, txfm_size_col, shift[0]);// shift[0] is 0
  if (lr_flip) {
    __m128i temp[4];
    flip_buf_sse2(buf, temp, txfm_size_col);
    transpose_16bit_8x4(temp, buf);
  } else {
    transpose_16bit_8x4(buf, buf);
  }
  col_txfm(buf, buf, cos_bit_col);
  round_shift_16bit(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_4xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

void av1_lowbd_inv_txfm2d_add_8x4_sse2(const int32_t *input, uint8_t *output,
                                       int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf[8];
  const TX_SIZE tx_size = TX_8X4;
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  load_buffer_32bit_to_16bit(input, txfm_size_col, buf, txfm_size_row);
  transpose_16bit_8x4(buf, buf);
  round_shift_sse2(buf, buf, txfm_size_col);  // rect special code
  row_txfm(buf, buf, cos_bit_row);
  // round_shift_16bit(buf, txfm_size_col, shift[0]); // shift[0] is 0
  if (lr_flip) {
    __m128i temp[8];
    flip_buf_sse2(buf, temp, txfm_size_col);
    transpose_16bit_4x8(temp, buf);
  } else {
    transpose_16bit_4x8(buf, buf);
  }
  col_txfm(buf, buf, cos_bit_col);
  round_shift_16bit(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_8xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

void av1_lowbd_inv_txfm2d_add_8x16_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_8X16);
}

void av1_lowbd_inv_txfm2d_add_16x8_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_16X8);
}

void av1_lowbd_inv_txfm2d_add_16x32_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_16X32);
}

void av1_lowbd_inv_txfm2d_add_32x16_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_32X16);
}

#if CONFIG_TX64X64
void av1_lowbd_inv_txfm2d_add_32x64_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  // Remap 32x32 input into a modified 32x64 input by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, int32_t, mod_input[32 * 64]);
  memcpy(mod_input, input, 32 * 32 * sizeof(*mod_input));
  memset(mod_input + 32 * 32, 0, 32 * 32 * sizeof(*mod_input));
  lowbd_inv_txfm2d_add_internal_sse2(mod_input, output, stride, tx_type,
                                     TX_32X64);
}

void av1_lowbd_inv_txfm2d_add_64x32_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  // Remap 32x32 input into a modified 64x32 by:
  // - Copying over these values in top-left 32x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, int32_t, mod_input[64 * 32]);
  for (int row = 0; row < 32; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  lowbd_inv_txfm2d_add_internal_sse2(mod_input, output, stride, tx_type,
                                     TX_64X32);
}
#endif

void av1_lowbd_inv_txfm2d_add_4x16_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf[16];
  const TX_SIZE tx_size = TX_4X16;
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);

  const int row_one_loop = 8;
  for (int i = 0; i < 2; ++i) {
    const int32_t *input_cur = input + i * txfm_size_col * row_one_loop;
    __m128i *buf_cur = buf + i * row_one_loop;
    load_buffer_32bit_to_16bit_w4(input_cur, txfm_size_col, buf_cur,
                                  row_one_loop);
    transpose_16bit_4x8(buf_cur, buf_cur);
    row_txfm(buf_cur, buf_cur, cos_bit_row);
    round_shift_16bit(buf_cur, row_one_loop, shift[0]);
    if (lr_flip) {
      __m128i temp[8];
      flip_buf_sse2(buf_cur, temp, txfm_size_col);
      transpose_16bit_8x4(temp, buf_cur);
    } else {
      transpose_16bit_8x4(buf_cur, buf_cur);
    }
  }
  col_txfm(buf, buf, cos_bit_col);
  round_shift_16bit(buf, txfm_size_row, shift[1]);
  lowbd_write_buffer_4xn_sse2(buf, output, stride, ud_flip, txfm_size_row);
}

void av1_lowbd_inv_txfm2d_add_16x4_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  __m128i buf[16];
  const TX_SIZE tx_size = TX_16X4;
  const int8_t *shift = inv_txfm_shift_ls[tx_size];
  const int txw_idx = get_txw_idx(tx_size);
  const int txh_idx = get_txh_idx(tx_size);
  const int cos_bit_row = inv_cos_bit_row[txw_idx][txh_idx];
  const int cos_bit_col = inv_cos_bit_col[txw_idx][txh_idx];
  const int txfm_size_col = tx_size_wide[tx_size];
  const int txfm_size_row = tx_size_high[tx_size];
  const int buf_size_w_div8 = txfm_size_col >> 3;

  const transform_1d_sse2 row_txfm =
      lowbd_txfm_all_1d_arr[txw_idx][htx_tab[tx_type]];
  const transform_1d_sse2 col_txfm =
      lowbd_txfm_all_1d_arr[txh_idx][vtx_tab[tx_type]];

  int ud_flip, lr_flip;
  get_flip_cfg(tx_type, &ud_flip, &lr_flip);
  const int row_one_loop = 8;
  for (int i = 0; i < buf_size_w_div8; ++i) {
    const int32_t *input_cur = input + i * row_one_loop;
    __m128i *buf_cur = buf + i * row_one_loop;
    load_buffer_32bit_to_16bit(input_cur, txfm_size_col, buf_cur,
                               txfm_size_row);
    transpose_16bit_8x4(buf_cur, buf_cur);
  }
  row_txfm(buf, buf, cos_bit_row);
  round_shift_16bit(buf, txfm_size_col, shift[0]);
  if (lr_flip) {
    __m128i temp[16];
    flip_buf_sse2(buf, temp, 16);
    transpose_16bit_4x8(temp, buf);
    transpose_16bit_4x8(temp + 8, buf + 8);
  } else {
    transpose_16bit_4x8(buf, buf);
    transpose_16bit_4x8(buf + row_one_loop, buf + row_one_loop);
  }
  for (int i = 0; i < buf_size_w_div8; i++) {
    col_txfm(buf + i * row_one_loop, buf + i * row_one_loop, cos_bit_col);
    round_shift_16bit(buf + i * row_one_loop, txfm_size_row, shift[1]);
  }
  lowbd_write_buffer_8xn_sse2(buf, output, stride, ud_flip, 4);
  lowbd_write_buffer_8xn_sse2(buf + 8, output + 8, stride, ud_flip, 4);
}

void av1_lowbd_inv_txfm2d_add_8x32_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_8X32);
}

void av1_lowbd_inv_txfm2d_add_32x8_sse2(const int32_t *input, uint8_t *output,
                                        int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  lowbd_inv_txfm2d_add_internal_sse2(input, output, stride, tx_type, TX_32X8);
}
#if CONFIG_TX64X64
void av1_lowbd_inv_txfm2d_add_16x64_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  // Remap 16x32 input into a modified 16x64 input by:
  // - Copying over these values in top-left 16x32 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, int32_t, mod_input[16 * 64]);
  memcpy(mod_input, input, 16 * 32 * sizeof(*mod_input));
  memset(mod_input + 16 * 32, 0, 16 * 32 * sizeof(*mod_input));
  lowbd_inv_txfm2d_add_internal_sse2(mod_input, output, stride, tx_type,
                                     TX_16X64);
}

void av1_lowbd_inv_txfm2d_add_64x16_sse2(const int32_t *input, uint8_t *output,
                                         int stride, TX_TYPE tx_type, int bd) {
  (void)bd;
  // Remap 32x16 input into a modified 64x16 by:
  // - Copying over these values in top-left 32x16 locations.
  // - Setting the rest of the locations to 0.
  DECLARE_ALIGNED(32, int32_t, mod_input[64 * 16]);
  for (int row = 0; row < 16; ++row) {
    memcpy(mod_input + row * 64, input + row * 32, 32 * sizeof(*mod_input));
    memset(mod_input + row * 64 + 32, 0, 32 * sizeof(*mod_input));
  }
  lowbd_inv_txfm2d_add_internal_sse2(mod_input, output, stride, tx_type,
                                     TX_64X16);
}
#endif

typedef void (*inv_txfm_func)(const int32_t *input, uint8_t *output, int stride,
                              TX_TYPE tx_type, int bd);

static inv_txfm_func inv_txfm_func_ls[TX_SIZES_ALL] = {
  av1_lowbd_inv_txfm2d_add_4x4_sse2,    // 4x4
  av1_lowbd_inv_txfm2d_add_8x8_sse2,    // 8x8
  av1_lowbd_inv_txfm2d_add_16x16_sse2,  // 16x16
  av1_lowbd_inv_txfm2d_add_32x32_sse2,  // 32x32
#if CONFIG_TX64X64
  av1_lowbd_inv_txfm2d_add_64x64_sse2,  // 64x64
#endif                                  // CONFIG_TX64X64
  av1_lowbd_inv_txfm2d_add_4x8_sse2,    // 4x8
  av1_lowbd_inv_txfm2d_add_8x4_sse2,    // 8x4
  av1_lowbd_inv_txfm2d_add_8x16_sse2,   // 8x16
  av1_lowbd_inv_txfm2d_add_16x8_sse2,   // 16x8
  av1_lowbd_inv_txfm2d_add_16x32_sse2,  // 16x32
  av1_lowbd_inv_txfm2d_add_32x16_sse2,  // 32x16
#if CONFIG_TX64X64
  av1_lowbd_inv_txfm2d_add_32x64_sse2,  // 32x64
  av1_lowbd_inv_txfm2d_add_64x32_sse2,  // 64x32
#endif                                  // CONFIG_TX64X64
  av1_lowbd_inv_txfm2d_add_4x16_sse2,   // 4x16
  av1_lowbd_inv_txfm2d_add_16x4_sse2,   // 16x4
  av1_lowbd_inv_txfm2d_add_8x32_sse2,   // 8x32
  av1_lowbd_inv_txfm2d_add_32x8_sse2,   // 32x8
#if CONFIG_TX64X64
  av1_lowbd_inv_txfm2d_add_16x64_sse2,  // 16x64
  av1_lowbd_inv_txfm2d_add_64x16_sse2,  // 64x16
#endif                                  // CONFIG_TX64X64
};

void av1_inv_txfm_add_sse2(const tran_low_t *dqcoeff, uint8_t *dst, int stride,
                           const TxfmParam *txfm_param) {
  const TX_TYPE tx_type = txfm_param->tx_type;
  const inv_txfm_func inv_func = inv_txfm_func_ls[txfm_param->tx_size];
  if (inv_func != NULL && (!txfm_param->lossless)) {
    inv_func(dqcoeff, dst, stride, tx_type, txfm_param->bd);
  } else {
    av1_inv_txfm_add_c(dqcoeff, dst, stride, txfm_param);
  }
}
