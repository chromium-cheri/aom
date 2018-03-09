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

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include "av1/common/blockd.h"

static INLINE CFL_ALLOWED_TYPE is_cfl_allowed(const MB_MODE_INFO *mbmi) {
  const BLOCK_SIZE bsize = mbmi->sb_type;
  assert(bsize < BLOCK_SIZES_ALL);
  return (CFL_ALLOWED_TYPE)(block_size_wide[bsize] <= 32 &&
                            block_size_high[bsize] <= 32);
}

static INLINE int get_scaled_luma_q0(int alpha_q3, int16_t pred_buf_q3) {
  int scaled_luma_q6 = alpha_q3 * pred_buf_q3;
  return ROUND_POWER_OF_TWO_SIGNED(scaled_luma_q6, 6);
}

static INLINE CFL_PRED_TYPE get_cfl_pred_type(PLANE_TYPE plane) {
  assert(plane > 0);
  return (CFL_PRED_TYPE)(plane - 1);
}

void cfl_predict_block(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                       TX_SIZE tx_size, int plane);

void cfl_store_block(MACROBLOCKD *const xd, BLOCK_SIZE bsize, TX_SIZE tx_size);

void cfl_store_tx(MACROBLOCKD *const xd, int row, int col, TX_SIZE tx_size,
                  BLOCK_SIZE bsize);

void cfl_store_dc_pred(MACROBLOCKD *const xd, const uint8_t *input,
                       CFL_PRED_TYPE pred_plane, int width);

void cfl_load_dc_pred(MACROBLOCKD *const xd, uint8_t *dst, int dst_stride,
                      TX_SIZE tx_size, CFL_PRED_TYPE pred_plane);

// Null function used for invalid tx_sizes
void cfl_subsample_lbd_null(const uint8_t *input, int input_stride,
                            int16_t *output_q3);

// Null function used for invalid tx_sizes
void cfl_subsample_hbd_null(const uint16_t *input, int input_stride,
                            int16_t *output_q3);

// Allows the CFL_SUBSAMPLE function to switch types depending on the bitdepth.
#define CFL_SUBSAMPLE_INPUT_TYPE_lbd_ const uint8_t *input
#define CFL_SUBSAMPLE_INPUT_TYPE_hbd_ const uint16_t *input

// Declare a size-specific wrapper for the size-generic function. The compiler
// will inline the size generic function in here, the advantage is that the size
// will be constant allowing for loop unrolling and other constant propagated
// goodness.
#define CFL_SUBSAMPLE(arch, sub, bd, width, height)                            \
  void subsample_##bd##_##sub##_##width##x##height##_##arch(                   \
      CFL_SUBSAMPLE_INPUT_TYPE_##bd##_, int input_stride,                      \
      int16_t *output_q3) {                                                    \
    cfl_luma_subsampling_##sub##_##bd##_##arch(input, input_stride, output_q3, \
                                               width, height);                 \
  }

// Declare size-specific wrappers for all valid CfL sizes.
#define CFL_SUBSAMPLE_FUNCTIONS(arch, sub, bd) \
  CFL_SUBSAMPLE(arch, sub, bd, 4, 4)           \
  CFL_SUBSAMPLE(arch, sub, bd, 8, 8)           \
  CFL_SUBSAMPLE(arch, sub, bd, 16, 16)         \
  CFL_SUBSAMPLE(arch, sub, bd, 32, 32)         \
  CFL_SUBSAMPLE(arch, sub, bd, 4, 8)           \
  CFL_SUBSAMPLE(arch, sub, bd, 8, 4)           \
  CFL_SUBSAMPLE(arch, sub, bd, 8, 16)          \
  CFL_SUBSAMPLE(arch, sub, bd, 16, 8)          \
  CFL_SUBSAMPLE(arch, sub, bd, 16, 32)         \
  CFL_SUBSAMPLE(arch, sub, bd, 32, 16)         \
  CFL_SUBSAMPLE(arch, sub, bd, 4, 16)          \
  CFL_SUBSAMPLE(arch, sub, bd, 16, 4)          \
  CFL_SUBSAMPLE(arch, sub, bd, 8, 32)          \
  CFL_SUBSAMPLE(arch, sub, bd, 32, 8)

// Declare an architecture-specific array of function pointers for size-specific
// wrappers.
#define CFL_SUBSAMPLE_FUNCTION_ARRAY(arch, sub, bd)                       \
  static const cfl_subsample_##bd##_fn subfn_##sub[TX_SIZES_ALL] = {      \
    subsample_##bd##_##sub##_4x4_##arch,   /* 4x4 */                      \
    subsample_##bd##_##sub##_8x8_##arch,   /* 8x8 */                      \
    subsample_##bd##_##sub##_16x16_##arch, /* 16x16 */                    \
    subsample_##bd##_##sub##_32x32_##arch, /* 32x32 */                    \
    cfl_subsample_##bd##_null,             /* 64x64 (invalid CFL size) */ \
    subsample_##bd##_##sub##_4x8_##arch,   /* 4x8 */                      \
    subsample_##bd##_##sub##_8x4_##arch,   /* 8x4 */                      \
    subsample_##bd##_##sub##_8x16_##arch,  /* 8x16 */                     \
    subsample_##bd##_##sub##_16x8_##arch,  /* 16x8 */                     \
    subsample_##bd##_##sub##_16x32_##arch, /* 16x32 */                    \
    subsample_##bd##_##sub##_32x16_##arch, /* 32x16 */                    \
    cfl_subsample_##bd##_null,             /* 32x64 (invalid CFL size) */ \
    cfl_subsample_##bd##_null,             /* 64x32 (invalid CFL size) */ \
    subsample_##bd##_##sub##_4x16_##arch,  /* 4x16  */                    \
    subsample_##bd##_##sub##_16x4_##arch,  /* 16x4  */                    \
    subsample_##bd##_##sub##_8x32_##arch,  /* 8x32  */                    \
    subsample_##bd##_##sub##_32x8_##arch,  /* 32x8  */                    \
    cfl_subsample_##bd##_null,             /* 16x64 (invalid CFL size) */ \
    cfl_subsample_##bd##_null,             /* 64x16 (invalid CFL size) */ \
  };

// The RTCD script does not support passing in the an array, so we wrap it in
// this function.
#define CFL_GET_SUBSAMPLE_FUNCTION(arch)                        \
  CFL_SUBSAMPLE_FUNCTIONS(arch, 420, lbd)                       \
  cfl_subsample_lbd_fn cfl_get_luma_subsampling_420_lbd_##arch( \
      TX_SIZE tx_size) {                                        \
    CFL_SUBSAMPLE_FUNCTION_ARRAY(arch, 420, lbd)                \
    return subfn_420[tx_size];                                  \
  }

// Null function used for invalid tx_sizes
static INLINE void cfl_subtract_average_null(int16_t *pred_buf_q3) {
  (void)pred_buf_q3;
  assert(0);
}

// Declare a size-specific wrapper for the size-generic function. The compiler
// will inline the size generic function in here, the advantage is that the size
// will be constant allowing for loop unrolling and other constant propagated
// goodness.
#define CFL_SUB_AVG_X(arch, width, height, round_offset, num_pel_log2)      \
  void subtract_average_##width##x##height##_##arch(int16_t *pred_buf_q3) { \
    subtract_average_##arch(pred_buf_q3, width, height, round_offset,       \
                            num_pel_log2);                                  \
  }

// Declare size-specific wrappers for all valid CfL sizes.
#define CFL_SUB_AVG_FN(arch)                                                \
  CFL_SUB_AVG_X(arch, 4, 4, 8, 4)                                           \
  CFL_SUB_AVG_X(arch, 4, 8, 16, 5)                                          \
  CFL_SUB_AVG_X(arch, 4, 16, 32, 6)                                         \
  CFL_SUB_AVG_X(arch, 8, 4, 16, 5)                                          \
  CFL_SUB_AVG_X(arch, 8, 8, 32, 6)                                          \
  CFL_SUB_AVG_X(arch, 8, 16, 64, 7)                                         \
  CFL_SUB_AVG_X(arch, 8, 32, 128, 8)                                        \
  CFL_SUB_AVG_X(arch, 16, 4, 32, 6)                                         \
  CFL_SUB_AVG_X(arch, 16, 8, 64, 7)                                         \
  CFL_SUB_AVG_X(arch, 16, 16, 128, 8)                                       \
  CFL_SUB_AVG_X(arch, 16, 32, 256, 9)                                       \
  CFL_SUB_AVG_X(arch, 32, 8, 128, 8)                                        \
  CFL_SUB_AVG_X(arch, 32, 16, 256, 9)                                       \
  CFL_SUB_AVG_X(arch, 32, 32, 512, 10)                                      \
  cfl_subtract_average_fn get_subtract_average_fn_##arch(TX_SIZE tx_size) { \
    static const cfl_subtract_average_fn sub_avg[TX_SIZES_ALL] = {          \
      subtract_average_4x4_##arch,   /* 4x4 */                              \
      subtract_average_8x8_##arch,   /* 8x8 */                              \
      subtract_average_16x16_##arch, /* 16x16 */                            \
      subtract_average_32x32_##arch, /* 32x32 */                            \
      cfl_subtract_average_null,     /* 64x64 (invalid CFL size) */         \
      subtract_average_4x8_##arch,   /* 4x8 */                              \
      subtract_average_8x4_##arch,   /* 8x4 */                              \
      subtract_average_8x16_##arch,  /* 8x16 */                             \
      subtract_average_16x8_##arch,  /* 16x8 */                             \
      subtract_average_16x32_##arch, /* 16x32 */                            \
      subtract_average_32x16_##arch, /* 32x16 */                            \
      cfl_subtract_average_null,     /* 32x64 (invalid CFL size) */         \
      cfl_subtract_average_null,     /* 64x32 (invalid CFL size) */         \
      subtract_average_4x16_##arch,  /* 4x16 (invalid CFL size) */          \
      subtract_average_16x4_##arch,  /* 16x4 (invalid CFL size) */          \
      subtract_average_8x32_##arch,  /* 8x32 (invalid CFL size) */          \
      subtract_average_32x8_##arch,  /* 32x8 (invalid CFL size) */          \
      cfl_subtract_average_null,     /* 16x64 (invalid CFL size) */         \
      cfl_subtract_average_null,     /* 64x16 (invalid CFL size) */         \
    };                                                                      \
    /* Modulo TX_SIZES_ALL to ensure that an attacker won't be able to */   \
    /* index the function pointer array out of bounds. */                   \
    return sub_avg[tx_size % TX_SIZES_ALL];                                 \
  }

#define CFL_PREDICT_LBD_X(width, arch)                                         \
  void cfl_predict_lbd_##width##_##arch(const int16_t *pred_buf_q3,            \
                                        uint8_t *dst, int dst_stride,          \
                                        TX_SIZE tx_size, int alpha_q3) {       \
    cfl_predict_lbd_x(pred_buf_q3, dst, dst_stride, tx_size, alpha_q3, width); \
  }

#define CFL_PREDICT_HBD_X(width, arch)                                     \
  void cfl_predict_hbd_##width##_##arch(                                   \
      const int16_t *pred_buf_q3, uint16_t *dst, int dst_stride,           \
      TX_SIZE tx_size, int alpha_q3, int bd) {                             \
    cfl_predict_hbd_x(pred_buf_q3, dst, dst_stride, tx_size, alpha_q3, bd, \
                      width);                                              \
  }
#endif  // AV1_COMMON_CFL_H_
