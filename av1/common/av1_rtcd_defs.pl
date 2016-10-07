sub av1_common_forward_decls() {
print <<EOF
/*
 * AV1
 */

#include "aom/aom_integer.h"
#include "av1/common/common.h"
#include "av1/common/enums.h"

struct macroblockd;

/* Encoder forward decls */
struct macroblock;
struct aom_variance_vtable;
struct search_site_config;
struct mv;
union int_mv;
struct yv12_buffer_config;
struct InterpFilterParams;
typedef int16_t od_dering_in;
EOF
}
forward_decls qw/av1_common_forward_decls/;

# x86inc.asm had specific constraints. break it out so it's easy to disable.
# zero all the variables to avoid tricky else conditions.
$mmx_x86inc = $sse_x86inc = $sse2_x86inc = $ssse3_x86inc = $avx_x86inc =
  $avx2_x86inc = '';
$mmx_x86_64_x86inc = $sse_x86_64_x86inc = $sse2_x86_64_x86inc =
  $ssse3_x86_64_x86inc = $avx_x86_64_x86inc = $avx2_x86_64_x86inc = '';
if (aom_config("CONFIG_USE_X86INC") eq "yes") {
  $mmx_x86inc = 'mmx';
  $sse_x86inc = 'sse';
  $sse2_x86inc = 'sse2';
  $ssse3_x86inc = 'ssse3';
  $avx_x86inc = 'avx';
  $avx2_x86inc = 'avx2';
  if ($opts{arch} eq "x86_64") {
    $mmx_x86_64_x86inc = 'mmx';
    $sse_x86_64_x86inc = 'sse';
    $sse2_x86_64_x86inc = 'sse2';
    $ssse3_x86_64_x86inc = 'ssse3';
    $avx_x86_64_x86inc = 'avx';
    $avx2_x86_64_x86inc = 'avx2';
  }
}

# functions that are 64 bit only.
$mmx_x86_64 = $sse2_x86_64 = $ssse3_x86_64 = $avx_x86_64 = $avx2_x86_64 = '';
if ($opts{arch} eq "x86_64") {
  $mmx_x86_64 = 'mmx';
  $sse2_x86_64 = 'sse2';
  $ssse3_x86_64 = 'ssse3';
  $avx_x86_64 = 'avx';
  $avx2_x86_64 = 'avx2';
}

add_proto qw/void av1_convolve_horiz/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const struct InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg";
specialize qw/av1_convolve_horiz ssse3/;

add_proto qw/void av1_convolve_vert/, "const uint8_t *src, int src_stride, uint8_t *dst, int dst_stride, int w, int h, const struct InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg";
specialize qw/av1_convolve_vert ssse3/;

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_highbd_convolve_horiz/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const struct InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd";
  specialize qw/av1_highbd_convolve_horiz sse4_1/;
  add_proto qw/void av1_highbd_convolve_vert/, "const uint16_t *src, int src_stride, uint16_t *dst, int dst_stride, int w, int h, const struct InterpFilterParams fp, const int subpel_x_q4, int x_step_q4, int avg, int bd";
  specialize qw/av1_highbd_convolve_vert sse4_1/;
}

#
# dct
#
if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  # Note as optimized versions of these functions are added we need to add a check to ensure
  # that when CONFIG_EMULATE_HARDWARE is on, it defaults to the C versions only.
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void av1_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht4x4_16_add/;

    add_proto qw/void av1_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht8x8_64_add/;

    add_proto qw/void av1_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
    specialize qw/av1_iht16x16_256_add/;
  } else {
    add_proto qw/void av1_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht4x4_16_add sse2/;

    add_proto qw/void av1_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht8x8_64_add sse2/;

    add_proto qw/void av1_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
    specialize qw/av1_iht16x16_256_add/;
  }
} else {
  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void av1_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht4x4_16_add/;

    add_proto qw/void av1_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht8x8_64_add/;

    add_proto qw/void av1_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
    specialize qw/av1_iht16x16_256_add/;
  } else {
    add_proto qw/void av1_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht4x4_16_add sse2 neon dspr2 msa/;

    add_proto qw/void av1_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type";
    specialize qw/av1_iht8x8_64_add sse2 neon dspr2 msa/;

    add_proto qw/void av1_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type";
    specialize qw/av1_iht16x16_256_add sse2 dspr2 msa/;
  }
}

# High bitdepth functions
if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  #
  # Sub Pixel Filters
  #
  add_proto qw/void av1_highbd_convolve_copy/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve_copy/;

  add_proto qw/void av1_highbd_convolve_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve_avg/;

  add_proto qw/void av1_highbd_convolve8/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_horiz/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_vert/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_avg/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_avg/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_avg_horiz/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_avg_horiz/, "$sse2_x86_64";

  add_proto qw/void av1_highbd_convolve8_avg_vert/, "const uint8_t *src, ptrdiff_t src_stride, uint8_t *dst, ptrdiff_t dst_stride, const int16_t *filter_x, int x_step_q4, const int16_t *filter_y, int y_step_q4, int w, int h, int bps";
  specialize qw/av1_highbd_convolve8_avg_vert/, "$sse2_x86_64";

  #
  # dct
  #
  # Note as optimized versions of these functions are added we need to add a check to ensure
  # that when CONFIG_EMULATE_HARDWARE is on, it defaults to the C versions only.
  add_proto qw/void av1_highbd_iht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd";
  specialize qw/av1_highbd_iht4x4_16_add/;

  add_proto qw/void av1_highbd_iht8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int tx_type, int bd";
  specialize qw/av1_highbd_iht8x8_64_add/;

  add_proto qw/void av1_highbd_iht16x16_256_add/, "const tran_low_t *input, uint8_t *output, int pitch, int tx_type, int bd";
  specialize qw/av1_highbd_iht16x16_256_add/;
}

#
# Encoder functions below this point.
#
if (aom_config("CONFIG_AV1_ENCODER") eq "yes") {

# ENCODEMB INVOKE

if (aom_config("CONFIG_AOM_QM") eq "yes") {
  if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
    # the transform coefficients are held in 32-bit
    # values, so the assembler code for  av1_block_error can no longer be used.
    add_proto qw/int64_t av1_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
    specialize qw/av1_block_error/;

    add_proto qw/void av1_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void av1_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void av1_fdct8x8_quant/, "const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t *iqm_ptr";
    specialize qw/av1_fdct8x8_quant/;
  } else {
    add_proto qw/int64_t av1_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
    specialize qw/av1_block_error avx2 msa/, "$sse2_x86inc";

    add_proto qw/int64_t av1_block_error_fp/, "const int16_t *coeff, const int16_t *dqcoeff, int block_size";
    specialize qw/av1_block_error_fp neon/, "$sse2_x86inc";

    add_proto qw/void av1_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void av1_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t *iqm_ptr";

    add_proto qw/void av1_fdct8x8_quant/, "const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t *iqm_ptr";
  }
} else {
  if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
    # the transform coefficients are held in 32-bit
    # values, so the assembler code for  av1_block_error can no longer be used.
    add_proto qw/int64_t av1_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
    specialize qw/av1_block_error/;

    add_proto qw/void av1_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_quantize_fp/;

    add_proto qw/void av1_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_quantize_fp_32x32/;

    add_proto qw/void av1_fdct8x8_quant/, "const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_fdct8x8_quant/;
  } else {
    add_proto qw/int64_t av1_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz";
    specialize qw/av1_block_error avx2 msa/, "$sse2_x86inc";

    add_proto qw/int64_t av1_block_error_fp/, "const int16_t *coeff, const int16_t *dqcoeff, int block_size";
    specialize qw/av1_block_error_fp neon/, "$sse2_x86inc";

    add_proto qw/void av1_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_quantize_fp neon sse2/, "$ssse3_x86_64_x86inc";

    add_proto qw/void av1_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_quantize_fp_32x32/, "$ssse3_x86_64_x86inc";

    add_proto qw/void av1_fdct8x8_quant/, "const int16_t *input, int stride, tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_fdct8x8_quant sse2 ssse3 neon/;
  }

}


# fdct functions

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  add_proto qw/void av1_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_fht4x4 sse2/;

  add_proto qw/void av1_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_fht8x8 sse2/;

  add_proto qw/void av1_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_fht16x16 sse2/;

  add_proto qw/void av1_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/av1_fwht4x4/, "$sse2_x86inc";
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void av1_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4/;

    add_proto qw/void av1_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4_1/;

    add_proto qw/void av1_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8/;

    add_proto qw/void av1_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8_1/;

    add_proto qw/void av1_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16/;

    add_proto qw/void av1_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16_1/;

    add_proto qw/void av1_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32/;

    add_proto qw/void av1_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_rd/;

    add_proto qw/void av1_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_1/;

    add_proto qw/void av1_highbd_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct4x4/;

    add_proto qw/void av1_highbd_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct8x8/;

    add_proto qw/void av1_highbd_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct8x8_1/;

    add_proto qw/void av1_highbd_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct16x16/;

    add_proto qw/void av1_highbd_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct16x16_1/;

    add_proto qw/void av1_highbd_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct32x32/;

    add_proto qw/void av1_highbd_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct32x32_rd/;

    add_proto qw/void av1_highbd_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct32x32_1/;
  } else {
    add_proto qw/void av1_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4 sse2/;

    add_proto qw/void av1_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4_1 sse2/;

    add_proto qw/void av1_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8 sse2/;

    add_proto qw/void av1_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8_1 sse2/;

    add_proto qw/void av1_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16 sse2/;

    add_proto qw/void av1_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16_1 sse2/;

    add_proto qw/void av1_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32 sse2/;

    add_proto qw/void av1_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_rd sse2/;

    add_proto qw/void av1_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_1 sse2/;

    add_proto qw/void av1_highbd_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct4x4 sse2/;

    add_proto qw/void av1_highbd_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct8x8 sse2/;

    add_proto qw/void av1_highbd_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct8x8_1/;

    add_proto qw/void av1_highbd_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct16x16 sse2/;

    add_proto qw/void av1_highbd_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct16x16_1/;

    add_proto qw/void av1_highbd_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct32x32 sse2/;

    add_proto qw/void av1_highbd_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct32x32_rd sse2/;

    add_proto qw/void av1_highbd_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_highbd_fdct32x32_1/;
  }
} else {
  add_proto qw/void av1_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_fht4x4 sse2 msa/;

  add_proto qw/void av1_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_fht8x8 sse2 msa/;

  add_proto qw/void av1_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_fht16x16 sse2 msa/;

  add_proto qw/void av1_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/av1_fwht4x4 msa/, "$sse2_x86inc";
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void av1_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4/;

    add_proto qw/void av1_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4_1/;

    add_proto qw/void av1_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8/;

    add_proto qw/void av1_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8_1/;

    add_proto qw/void av1_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16/;

    add_proto qw/void av1_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16_1/;

    add_proto qw/void av1_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32/;

    add_proto qw/void av1_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_rd/;

    add_proto qw/void av1_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_1/;
  } else {
    add_proto qw/void av1_fdct4x4/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4 sse2/;

    add_proto qw/void av1_fdct4x4_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct4x4_1 sse2/;

    add_proto qw/void av1_fdct8x8/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8 sse2/;

    add_proto qw/void av1_fdct8x8_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct8x8_1 sse2/;

    add_proto qw/void av1_fdct16x16/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16 sse2/;

    add_proto qw/void av1_fdct16x16_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct16x16_1 sse2/;

    add_proto qw/void av1_fdct32x32/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32 sse2/;

    add_proto qw/void av1_fdct32x32_rd/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_rd sse2/;

    add_proto qw/void av1_fdct32x32_1/, "const int16_t *input, tran_low_t *output, int stride";
    specialize qw/av1_fdct32x32_1 sse2/;
  }
}

# Inverse transform
if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {
  # Note as optimized versions of these functions are added we need to add a check to ensure
  # that when CONFIG_EMULATE_HARDWARE is on, it defaults to the C versions only.
  add_proto qw/void av1_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct4x4_1_add/;

  add_proto qw/void av1_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct4x4_16_add/;

  add_proto qw/void av1_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct8x8_1_add/;

  add_proto qw/void av1_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct8x8_64_add/;

  add_proto qw/void av1_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct8x8_12_add/;

  add_proto qw/void av1_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct16x16_1_add/;

  add_proto qw/void av1_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct16x16_256_add/;

  add_proto qw/void av1_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct16x16_10_add/;

  add_proto qw/void av1_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct32x32_1024_add/;

  add_proto qw/void av1_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct32x32_34_add/;

  add_proto qw/void av1_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_idct32x32_1_add/;

  add_proto qw/void av1_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_iwht4x4_1_add/;

  add_proto qw/void av1_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
  specialize qw/av1_iwht4x4_16_add/;

  add_proto qw/void av1_highbd_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_idct4x4_1_add/;

  add_proto qw/void av1_highbd_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_idct8x8_1_add/;

  add_proto qw/void av1_highbd_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_idct16x16_1_add/;

  add_proto qw/void av1_highbd_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_idct32x32_1024_add/;

  add_proto qw/void av1_highbd_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_idct32x32_34_add/;

  add_proto qw/void av1_highbd_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_idct32x32_1_add/;

  add_proto qw/void av1_highbd_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_iwht4x4_1_add/;

  add_proto qw/void av1_highbd_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
  specialize qw/av1_highbd_iwht4x4_16_add/;

  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void av1_highbd_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct4x4_16_add/;

    add_proto qw/void av1_highbd_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct8x8_64_add/;

    add_proto qw/void av1_highbd_idct8x8_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct8x8_10_add/;

    add_proto qw/void av1_highbd_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct16x16_256_add/;

    add_proto qw/void av1_highbd_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct16x16_10_add/;
  } else {
    add_proto qw/void av1_highbd_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct4x4_16_add sse2/;

    add_proto qw/void av1_highbd_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct8x8_64_add sse2/;

    add_proto qw/void av1_highbd_idct8x8_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct8x8_10_add sse2/;

    add_proto qw/void av1_highbd_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct16x16_256_add sse2/;

    add_proto qw/void av1_highbd_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride, int bd";
    specialize qw/av1_highbd_idct16x16_10_add sse2/;
  }  # CONFIG_EMULATE_HARDWARE
} else {
  # Force C versions if CONFIG_EMULATE_HARDWARE is 1
  if (aom_config("CONFIG_EMULATE_HARDWARE") eq "yes") {
    add_proto qw/void av1_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct4x4_1_add/;

    add_proto qw/void av1_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct4x4_16_add/;

    add_proto qw/void av1_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct8x8_1_add/;

    add_proto qw/void av1_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct8x8_64_add/;

    add_proto qw/void av1_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct8x8_12_add/;

    add_proto qw/void av1_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct16x16_1_add/;

    add_proto qw/void av1_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct16x16_256_add/;

    add_proto qw/void av1_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct16x16_10_add/;

    add_proto qw/void av1_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct32x32_1024_add/;

    add_proto qw/void av1_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct32x32_34_add/;

    add_proto qw/void av1_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct32x32_1_add/;

    add_proto qw/void av1_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_iwht4x4_1_add/;

    add_proto qw/void av1_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_iwht4x4_16_add/;
  } else {
    add_proto qw/void av1_idct4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct4x4_1_add sse2/;

    add_proto qw/void av1_idct4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct4x4_16_add sse2/;

    add_proto qw/void av1_idct8x8_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct8x8_1_add sse2/;

    add_proto qw/void av1_idct8x8_64_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct8x8_64_add sse2/;

    add_proto qw/void av1_idct8x8_12_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct8x8_12_add sse2/;

    add_proto qw/void av1_idct16x16_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct16x16_1_add sse2/;

    add_proto qw/void av1_idct16x16_256_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct16x16_256_add sse2/;

    add_proto qw/void av1_idct16x16_10_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct16x16_10_add sse2/;

    add_proto qw/void av1_idct32x32_1024_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct32x32_1024_add sse2/;

    add_proto qw/void av1_idct32x32_34_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct32x32_34_add sse2/;

    add_proto qw/void av1_idct32x32_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_idct32x32_1_add sse2/;

    add_proto qw/void av1_iwht4x4_1_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_iwht4x4_1_add/;

    add_proto qw/void av1_iwht4x4_16_add/, "const tran_low_t *input, uint8_t *dest, int dest_stride";
    specialize qw/av1_iwht4x4_16_add/;
  }  # CONFIG_EMULATE_HARDWARE
}  # CONFIG_AOM_HIGHBITDEPTH

#
# Motion search
#
add_proto qw/int av1_full_search_sad/, "const struct macroblock *x, const struct mv *ref_mv, int sad_per_bit, int distance, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv, struct mv *best_mv";
specialize qw/av1_full_search_sad sse3 sse4_1/;
$av1_full_search_sad_sse3=av1_full_search_sadx3;
$av1_full_search_sad_sse4_1=av1_full_search_sadx8;

add_proto qw/int av1_diamond_search_sad/, "const struct macroblock *x, const struct search_site_config *cfg,  struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/av1_diamond_search_sad/;

add_proto qw/int av1_full_range_search/, "const struct macroblock *x, const struct search_site_config *cfg, struct mv *ref_mv, struct mv *best_mv, int search_param, int sad_per_bit, int *num00, const struct aom_variance_vtable *fn_ptr, const struct mv *center_mv";
specialize qw/av1_full_range_search/;

add_proto qw/void av1_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";
specialize qw/av1_temporal_filter_apply sse2 msa/;

if (aom_config("CONFIG_AOM_HIGHBITDEPTH") eq "yes") {

  # ENCODEMB INVOKE

  add_proto qw/int64_t av1_highbd_block_error/, "const tran_low_t *coeff, const tran_low_t *dqcoeff, intptr_t block_size, int64_t *ssz, int bd";
  specialize qw/av1_highbd_block_error sse2/;

  if (aom_config("CONFIG_AOM_QM") eq "yes") {
    add_proto qw/void av1_highbd_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr";

    add_proto qw/void av1_highbd_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan, const qm_val_t * qm_ptr, const qm_val_t * iqm_ptr";
  } else {
    add_proto qw/void av1_highbd_quantize_fp/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_highbd_quantize_fp/;

    add_proto qw/void av1_highbd_quantize_fp_32x32/, "const tran_low_t *coeff_ptr, intptr_t n_coeffs, int skip_block, const int16_t *zbin_ptr, const int16_t *round_ptr, const int16_t *quant_ptr, const int16_t *quant_shift_ptr, tran_low_t *qcoeff_ptr, tran_low_t *dqcoeff_ptr, const int16_t *dequant_ptr, uint16_t *eob_ptr, const int16_t *scan, const int16_t *iscan";
    specialize qw/av1_highbd_quantize_fp_32x32/;

  }

  # fdct functions
  add_proto qw/void av1_highbd_fht4x4/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_highbd_fht4x4/;

  add_proto qw/void av1_highbd_fht8x8/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_highbd_fht8x8/;

  add_proto qw/void av1_highbd_fht16x16/, "const int16_t *input, tran_low_t *output, int stride, int tx_type";
  specialize qw/av1_highbd_fht16x16/;

  add_proto qw/void av1_highbd_fwht4x4/, "const int16_t *input, tran_low_t *output, int stride";
  specialize qw/av1_highbd_fwht4x4/;

  add_proto qw/void av1_highbd_temporal_filter_apply/, "uint8_t *frame1, unsigned int stride, uint8_t *frame2, unsigned int block_width, unsigned int block_height, int strength, int filter_weight, unsigned int *accumulator, uint16_t *count";
  specialize qw/av1_highbd_temporal_filter_apply/;

}
# End av1_high encoder functions

}
# end encoder functions

# Deringing Functions

if (aom_config("CONFIG_DERING") eq "yes") {
  add_proto qw/int od_dir_find8/, "const od_dering_in *img, int stride, int32_t *var, int coeff_shift";
  specialize qw/od_dir_find8 sse4_1/;

  add_proto qw/int od_filter_dering_direction_4x4/, "int16_t *y, int ystride, const int16_t *in, int threshold, int dir";
  specialize qw/od_filter_dering_direction_4x4 sse4_1/;

  add_proto qw/int od_filter_dering_direction_8x8/, "int16_t *y, int ystride, const int16_t *in, int threshold, int dir";
  specialize qw/od_filter_dering_direction_8x8 sse4_1/;

  add_proto qw/void od_filter_dering_orthogonal_4x4/, "int16_t *y, int ystride, const int16_t *in, int threshold, int dir";
  specialize qw/od_filter_dering_orthogonal_4x4 sse4_1/;

  add_proto qw/void od_filter_dering_orthogonal_8x8/, "int16_t *y, int ystride, const int16_t *in, int threshold, int dir";
  specialize qw/od_filter_dering_orthogonal_8x8 sse4_1/;
}

1;
