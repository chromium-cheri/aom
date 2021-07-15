/*
 * Copyright (c) 2020, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <ostream>

#include "aom/aom_codec.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"
#include "test/codec_factory.h"
#include "test/encode_test_driver.h"
#include "test/i420_video_source.h"
#include "test/util.h"

namespace {
typedef struct {
  const unsigned int min_kf_dist;
  const unsigned int max_kf_dist;
} kfIntervalParam;

const kfIntervalParam kfTestParams[] = {
  { 1, 1 }, { 0, 10 }, { 10, 10 }, { 0, 30 }, { 30, 30 }
};

std::ostream &operator<<(std::ostream &os, const kfIntervalParam &test_arg) {
  return os << "kfIntervalParam { min_kf_dist:" << test_arg.min_kf_dist
            << " max_kf_dist:" << test_arg.max_kf_dist << " }";
}

// This class is used to test the presence of forward key frame.
class KeyFrameIntervalTestLarge
    : public ::libaom_test::CodecTestWith3Params<libaom_test::TestMode,
                                                 kfIntervalParam, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  KeyFrameIntervalTestLarge()
      : EncoderTest(GET_PARAM(0)), encoding_mode_(GET_PARAM(1)),
        kf_dist_param_(GET_PARAM(2)), end_usage_check_(GET_PARAM(3)) {
    kf_dist_ = -1;
    is_kf_interval_violated_ = false;
  }
  virtual ~KeyFrameIntervalTestLarge() {}

  virtual void SetUp() {
    InitializeConfig(encoding_mode_);
    const aom_rational timebase = { 1, 30 };
    cfg_.g_timebase = timebase;
    cfg_.rc_end_usage = end_usage_check_;
    cfg_.g_threads = 1;
    cfg_.kf_min_dist = kf_dist_param_.min_kf_dist;
    cfg_.kf_max_dist = kf_dist_param_.max_kf_dist;
    cfg_.g_lag_in_frames = 19;
  }

  virtual bool DoDecode() const { return 1; }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, 5);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF, 1);
    }
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
      int frame_flags = 0;
      AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_FRAME_FLAGS,
                                    &frame_flags);
      if (kf_dist_ != -1) {
        kf_dist_++;
        if (kf_dist_ > (int)kf_dist_param_.max_kf_dist) {
          is_kf_interval_violated_ = true;
        }
      }
      if ((frame_flags & AOM_FRAME_IS_KEY) ==
          static_cast<aom_codec_frame_flags_t>(AOM_FRAME_IS_KEY)) {
        if (kf_dist_ != -1 && kf_dist_ < (int)kf_dist_param_.min_kf_dist) {
          is_kf_interval_violated_ = true;
        }
        kf_dist_ = 0;
      }
    }
    return AOM_CODEC_OK == res_dec;
  }

  ::libaom_test::TestMode encoding_mode_;
  const kfIntervalParam kf_dist_param_;
  int kf_dist_;
  bool is_kf_interval_violated_;
  aom_rc_mode end_usage_check_;
};

// Because valgrind builds take a very long time to run, use a lower
// resolution video for valgrind runs.
const char *TestFileName() {
#if AOM_VALGRIND_BUILD
  return "hantro_collage_w176h144.yuv";
#else
  return "hantro_collage_w352h288.yuv";
#endif  // AOM_VALGRIND_BUILD
}

int TestFileWidth() {
#if AOM_VALGRIND_BUILD
  return 176;
#else
  return 352;
#endif  // AOM_VALGRIND_BUILD
}

int TestFileHeight() {
#if AOM_VALGRIND_BUILD
  return 144;
#else
  return 288;
#endif  // AOM_VALGRIND_BUILD
}

TEST_P(KeyFrameIntervalTestLarge, KeyFrameIntervalTest) {
  libaom_test::I420VideoSource video(TestFileName(), TestFileWidth(),
                                     TestFileHeight(), cfg_.g_timebase.den,
                                     cfg_.g_timebase.num, 0, 75);
  ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
  ASSERT_EQ(is_kf_interval_violated_, false) << kf_dist_param_;
}

typedef struct {
  libaom_test::TestMode encoding_mode;
  int cpu_used;
  int auto_alt_ref;
  int fwd_kf_enabled;
} TestEncodeParam;

const TestEncodeParam kEncodeVectors[] = {
  { ::libaom_test::kRealTime, 7, 0, 0 },
  { ::libaom_test::kRealTime, 9, 0, 0 },
  { ::libaom_test::kOnePassGood, 2, 0, 0 },
  { ::libaom_test::kOnePassGood, 2, 0, 1 },
  { ::libaom_test::kOnePassGood, 2, 1, 0 },
  { ::libaom_test::kOnePassGood, 2, 1, 1 },
  { ::libaom_test::kOnePassGood, 5, 0, 0 },
  { ::libaom_test::kOnePassGood, 5, 0, 1 },
  { ::libaom_test::kOnePassGood, 5, 1, 0 },
  { ::libaom_test::kOnePassGood, 5, 1, 1 },
  { ::libaom_test::kTwoPassGood, 2, 0, 0 },
  { ::libaom_test::kTwoPassGood, 2, 0, 1 },
  { ::libaom_test::kTwoPassGood, 2, 1, 0 },
  { ::libaom_test::kTwoPassGood, 2, 1, 1 },
  { ::libaom_test::kTwoPassGood, 5, 0, 0 },
  { ::libaom_test::kTwoPassGood, 5, 0, 1 },
  { ::libaom_test::kTwoPassGood, 5, 1, 0 },
  { ::libaom_test::kTwoPassGood, 5, 1, 1 },
};

// This class tests for presence and placement of application forced key frames.
class ForcedKeyTestLarge
    : public ::libaom_test::CodecTestWith2Params<TestEncodeParam, aom_rc_mode>,
      public ::libaom_test::EncoderTest {
 protected:
  ForcedKeyTestLarge()
      : EncoderTest(GET_PARAM(0)), test_encode_param_(GET_PARAM(1)),
        rc_end_usage_(GET_PARAM(2)) {
    forced_kf_frame_num_ = 1;
    frame_num_ = 0;
    is_kf_placement_violated_ = false;
  }
  virtual ~ForcedKeyTestLarge() {}

  virtual void SetUp() {
    InitializeConfig(test_encode_param_.encoding_mode);
    cfg_.rc_end_usage = rc_end_usage_;
    cfg_.g_threads = 0;
    cfg_.kf_max_dist = 30;
    cfg_.kf_min_dist = 0;
    cfg_.fwd_kf_enabled = test_encode_param_.fwd_kf_enabled;
  }

  virtual void PreEncodeFrameHook(::libaom_test::VideoSource *video,
                                  ::libaom_test::Encoder *encoder) {
    if (video->frame() == 0) {
      encoder->Control(AOME_SET_CPUUSED, test_encode_param_.cpu_used);
      encoder->Control(AOME_SET_ENABLEAUTOALTREF,
                       test_encode_param_.auto_alt_ref);
#if CONFIG_AV1_ENCODER
      // override test default for tile columns if necessary.
      if (GET_PARAM(0) == &libaom_test::kAV1) {
        encoder->Control(AV1E_SET_TILE_COLUMNS, 6);
      }
#endif
    }
    frame_flags_ =
        ((int)video->frame() == forced_kf_frame_num_) ? AOM_EFLAG_FORCE_KF : 0;
  }

  virtual bool HandleDecodeResult(const aom_codec_err_t res_dec,
                                  libaom_test::Decoder *decoder) {
    EXPECT_EQ(AOM_CODEC_OK, res_dec) << decoder->DecodeError();
    if (AOM_CODEC_OK == res_dec) {
      if ((int)frame_num_ == forced_kf_frame_num_) {
        aom_codec_ctx_t *ctx_dec = decoder->GetDecoder();
        int frame_flags = 0;
        AOM_CODEC_CONTROL_TYPECHECKED(ctx_dec, AOMD_GET_FRAME_FLAGS,
                                      &frame_flags);
        if ((frame_flags & AOM_FRAME_IS_KEY) !=
            static_cast<aom_codec_frame_flags_t>(AOM_FRAME_IS_KEY)) {
          is_kf_placement_violated_ = true;
        }
      }
      ++frame_num_;
    }
    return AOM_CODEC_OK == res_dec;
  }

  TestEncodeParam test_encode_param_;
  aom_rc_mode rc_end_usage_;
  int forced_kf_frame_num_;
  unsigned int frame_num_;
  bool is_kf_placement_violated_;
};

TEST_P(ForcedKeyTestLarge, Frame1IsKey) {
  const aom_rational timebase = { 1, 30 };
  int lag_values[] = { 3, 15, 25, -1 };

  if (test_encode_param_.encoding_mode == ::libaom_test::kRealTime) {
    lag_values[1] = 0;
    lag_values[2] = -1;
    lag_values[3] = -1;
  }
  forced_kf_frame_num_ = 1;
  for (int i = 0; lag_values[i] != -1; ++i) {
    frame_num_ = 0;
    cfg_.g_lag_in_frames = lag_values[i];
    is_kf_placement_violated_ = false;
    libaom_test::I420VideoSource video(
        TestFileName(), TestFileWidth(), TestFileHeight(), timebase.den,
        timebase.num, 0, test_encode_param_.fwd_kf_enabled ? 60 : 30);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_EQ(is_kf_placement_violated_, false)
        << "Frame #" << frame_num_ << " isn't a keyframe!";
  }
}

// This class checks the presence and placement of application
// forced key frames.
TEST_P(ForcedKeyTestLarge, ForcedFrameIsKey) {
  const aom_rational timebase = { 1, 30 };
  const int test_values[] = { 3, 15, 25, -1 };

  cfg_.g_lag_in_frames = 0;
  for (int i = 0; test_values[i] != -1; ++i) {
    frame_num_ = 0;
    forced_kf_frame_num_ = test_values[i] - 1;
    if (test_encode_param_.encoding_mode != ::libaom_test::kRealTime)
      cfg_.g_lag_in_frames = test_values[i];
    is_kf_placement_violated_ = false;
    libaom_test::I420VideoSource video(
        TestFileName(), TestFileWidth(), TestFileHeight(), timebase.den,
        timebase.num, 0, test_encode_param_.fwd_kf_enabled ? 60 : 30);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_EQ(is_kf_placement_violated_, false)
        << "Frame #" << frame_num_ << " isn't a keyframe!";

    // Two pass and single pass CBR are currently segfaulting for the case when
    // forced kf is placed after lag in frames.
    // TODO(anyone): Enable(uncomment) below test once above bug is fixed.
    //    frame_num_ = 0;
    //    forced_kf_frame_num_ = test_values[i] + 1;
    //    if (test_encode_param_.encoding_mode != ::libaom_test::kRealTime)
    //      cfg_.g_lag_in_frames = test_values[i];
    //    is_kf_placement_violated_ = false;
    //    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    //    ASSERT_EQ(is_kf_placement_violated_, false)
    //    << "Frame #" << frame_num_ << " isn't a keyframe!";
  }
}

TEST_P(ForcedKeyTestLarge, ForcedFrameIsKeyCornerCases) {
  const aom_rational timebase = { 1, 30 };
  const int kf_offsets[] = { -2, -1, 1, 2, 0 };
  if (test_encode_param_.encoding_mode != ::libaom_test::kRealTime)
    cfg_.g_lag_in_frames = 35;
  else
    cfg_.g_lag_in_frames = 0;

  for (int i = 0; kf_offsets[i] != 0; ++i) {
    frame_num_ = 0;
    forced_kf_frame_num_ = (int)cfg_.kf_max_dist + kf_offsets[i];
    forced_kf_frame_num_ = forced_kf_frame_num_ > 0 ? forced_kf_frame_num_ : 1;
    is_kf_placement_violated_ = false;
    libaom_test::I420VideoSource video(
        TestFileName(), TestFileWidth(), TestFileHeight(), timebase.den,
        timebase.num, 0, test_encode_param_.fwd_kf_enabled ? 60 : 30);
    ASSERT_NO_FATAL_FAILURE(RunLoop(&video));
    ASSERT_EQ(is_kf_placement_violated_, false)
        << "Frame #" << frame_num_ << " isn't a keyframe!";
  }
}

AV1_INSTANTIATE_TEST_SUITE(KeyFrameIntervalTestLarge,
                           testing::Values(::libaom_test::kOnePassGood,
                                           ::libaom_test::kTwoPassGood),
                           ::testing::ValuesIn(kfTestParams),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CBR, AOM_CQ));

// TODO(anyone): Add CBR to list of rc_modes once forced kf placement after
// lag in frames bug is fixed.
AV1_INSTANTIATE_TEST_SUITE(ForcedKeyTestLarge,
                           ::testing::ValuesIn(kEncodeVectors),
                           ::testing::Values(AOM_Q, AOM_VBR, AOM_CQ));
}  // namespace
