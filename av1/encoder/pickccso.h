

#ifndef AOM_AV1_ENCODER_PICKCCF_H_
#define AOM_AV1_ENCODER_PICKCCF_H_

#include "av1/common/ccso.h"
#include "av1/encoder/speed_features.h"

#ifdef __cplusplus
extern "C" {
#endif

#if CCSO_SCALE_LAMBDA
static INLINE double clamp_dbl(double value, double low, double high) {
  return value < low ? low : (value > high ? high : value);
}
#endif

void ccso_search(AV1_COMMON *cm, MACROBLOCKD *xd, int rdmult,
                 uint16_t *ext_rec_y, uint16_t *rec_yuv[3],
                 uint16_t *org_yuv[3]);

void compute_distortion(const uint16_t *org, int org_stride,
                        const uint8_t *rec8, const uint16_t *rec16,
                        int rec_stride, int height, int width,
                        uint64_t *distortion_buf, int distortion_buf_stride,
                        uint64_t *total_distortion);

void derive_ccso_filter(AV1_COMMON *cm, const int plane, MACROBLOCKD *xd,
                        uint16_t *org_uv, uint16_t *ext_rec_y, uint16_t *rec_uv,
                        int rdmult);

void derive_blk_md(AV1_COMMON *cm, MACROBLOCKD *xd, uint64_t *unfiltered_dist,
                   uint64_t *training_dist, bool *m_filter_control,
                   uint64_t *cur_total_dist, int *cur_total_rate,
                   bool *filter_enable, int rdmult);

void compute_total_error(MACROBLOCKD *xd, uint16_t *ext_rec_luma,
                         const uint16_t *org_chroma, uint16_t *rec_yuv_16,
                         uint8_t quanStep, uint8_t ext_filter_support);

void derive_lut_offset(int8_t *temp_filter_offset);

#ifdef __cplusplus
}  // extern "C"
#endif
#endif  // AOM_AV1_ENCODER_PICKCCF_H_
