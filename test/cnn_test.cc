/*
 * Copyright (c) 2019, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

#include "config/av1_rtcd.h"

#include "av1/encoder/cnn.h"

#define SQR(x) ((x) * (x))

// Best possible pixelwise guarenteed preicison given each float has at most
// 3 specified decimals.
#define PIXELWISE_FLOAT_TOL 1E-2

#define MSE_FLOAT_TOL 1E-6
#define MSE_INT_TOL 0

namespace {

class CNNTest : public ::testing::Test {
 protected:
  static void RunCNNTest(int image_width, int image_height, float *input,
                         float *expected, CNN_CONFIG *cnn_config, int in_stride,
                         CNN_THREAD_DATA *thread_data, double tolerance) {
    int out_width, out_height, out_channels;
    av1_find_cnn_output_size(image_width, image_height, cnn_config, &out_width,
                             &out_height, &out_channels);

    const int out_size = out_width * out_height;
    const int out_stride = out_width;

    float *output_ =
        (float *)aom_malloc(sizeof(*output_) * out_size * out_channels);
    float *output[CNN_MAX_CHANNELS] = { nullptr };
    for (int channel = 0; channel < out_channels; ++channel) {
      output[channel] = output_ + (channel * out_size);
    }

    av1_cnn_predict((const float **)&input, image_width, image_height,
                    in_stride, cnn_config, thread_data, output, out_stride);

    double mse = 0;
    for (int channel = 0; channel < out_channels; ++channel) {
      for (int i = 0; i < out_size; ++i) {
        int index = channel * out_size + i;
        EXPECT_NEAR(expected[index], output[channel][i], PIXELWISE_FLOAT_TOL)
            << index << ": " << expected[index] << "/" << output[channel][i]
            << std::endl;
        mse += SQR(expected[index] - output[channel][i]);
      }
    }
    mse /= (out_size * out_channels);
    EXPECT_LE(mse, tolerance);

    aom_free(output_);
  }

  static void AssignLayerWeightsBiases(CNN_CONFIG *cnn_config, float *weights,
                                       float *bias) {
    size_t weight_offset = 0;
    size_t bias_offset = 0;
    for (int layer = 0; layer < cnn_config->num_layers; ++layer) {
      CNN_LAYER_CONFIG *layer_config = &cnn_config->layer_config[layer];
      layer_config->weights = weights + weight_offset;
      layer_config->bias = bias + bias_offset;
      weight_offset += layer_config->filter_width *
                       layer_config->filter_height * layer_config->in_channels *
                       layer_config->out_channels;
      bias_offset += layer_config->out_channels;

      ASSERT_NE(layer_config->weights, nullptr);
      ASSERT_NE(layer_config->bias, nullptr);
    }
  }
};

}  // namespace

TEST_F(CNNTest, TestMultilayerConvolution) {
  int image_height = 16;
  int image_width = 16;
  int filter_height = 5;
  int filter_width = 4;

  float input[] = {
    -3, 1,  -3, 2,  -2, -2, 2,  -2, 1,  -2, -3, 1,  2,  2,  2,  -2, 0,  1,  -1,
    -3, -1, -1, 1,  0,  -3, 1,  0,  -1, 1,  0,  0,  -3, -3, -3, 0,  2,  1,  -1,
    2,  0,  1,  -3, -1, 2,  2,  1,  -2, 0,  -1, 0,  -2, -2, -1, 1,  0,  0,  0,
    -2, -2, -2, 1,  1,  -2, 1,  1,  -2, -2, 1,  -2, -1, -2, -3, 2,  -3, -1, 1,
    0,  -2, -2, -2, 1,  -2, -2, -1, -1, 2,  2,  2,  -1, 1,  -3, -3, 0,  2,  0,
    2,  1,  -3, -3, 1,  2,  2,  1,  -2, -3, 0,  -3, 0,  -3, -2, 0,  1,  1,  0,
    -3, 2,  -1, 2,  1,  0,  1,  -2, 1,  -1, -1, 2,  0,  -2, -3, 1,  1,  -2, -1,
    -3, -3, -1, 0,  -3, -2, 0,  0,  1,  0,  -3, -2, -1, 1,  0,  2,  1,  0,  -3,
    -2, -3, -3, -1, 0,  -2, 2,  -1, -3, 0,  -1, -1, 2,  0,  -3, -2, -1, 0,  0,
    1,  -2, 1,  2,  1,  2,  2,  -3, 2,  -1, 0,  0,  -1, 0,  2,  2,  -1, 2,  -2,
    1,  1,  -3, -3, 1,  -1, -1, -2, 2,  -2, -2, 2,  -1, -3, 2,  -3, 1,  -1, -1,
    -3, 1,  -1, 1,  0,  -3, -3, 1,  -3, -3, 0,  2,  2,  -2, -1, 2,  0,  2,  1,
    -1, -3, 0,  0,  -1, -1, 1,  0,  2,  0,  -3, 2,  1,  0,  1,  -3, 2,  -3, -3,
    -1, -3, -3, 2,  0,  2,  -2, 1,  -1,
  };

  float weights[] = {
    -2, 2,  -2, 2,  -1, -3, 2,  2,  0,  0,  -3, -1, -2, -3, 1,  -1, 0,  0,  0,
    2,  -2, 2,  -2, -3, 1,  1,  1,  -3, -1, 0,  1,  2,  -2, 0,  -1, -3, -1, -2,
    2,  -3, -3, 1,  -2, -3, 0,  2,  1,  -3, -3, -1, -3, -2, -1, -3, -1, -3, -2,
    -1, -3, -1, -2, -2, -3, 2,  0,  -3, 0,  -3, -3, 1,  -3, -1, 0,  -1, 1,  1,
    -1, 1,  -2, 0,  2,  0,  -3, 1,  -1, -1, 2,  0,  1,  -3, -3, 1,  2,  -3, -3,
    1,  -3, 2,  0,  -3, 1,  2,  2,  -2, -1, -2, 1,  1,  0,  -2, -2, 1,  2,  -1,
    -3, 1,  -2, 2,  -3, -2, -3, 2,  1,  0,  -2, 0,  1,  -3, 2,  -2, -2, 0,  2,
    -3, 2,  0,  0,  1,  -2, 1,  1,  -2, -1, -2, 1,  -2, 0,  -2, -2, 0,  -1, -1,
    -3, -3, -3, 1,  -3, -2, 2,  -1, 2,  0,  2,  -2, 2,  -2, 1,  -3, -3, -1, 0,
    2,  2,  1,  -1, -3, -1, -3, 2,  1,  -2, 0,  -3, -1, -3, -1, 2,  1,  0,  2,
    -1, 1,  0,  1,  2,  -1, -2, 2,  1,  -3, -1, -3, 0,  1,  -2, 0,  -2, -3, 0,
    -2, 2,  2,  0,  0,  2,  -3, 2,  -3, -2, 1,  2,  -3, -3, -1, -3, 0,  -3, -3,
    -2, -2, -2, 0,  0,  1,  0,  0,  -1, 0,  0,  -3, 0,  -3, -1, -2, 1,  -2, -1,
    2,  -2, 0,  0,  1,  0,  -2, -1, 0,  -3, 1,  0,  -1, -3, 1,  -1, 1,  -1, -3,
    1,  0,  1,  1,  -1, 2,  2,  0,  0,  1,  -3, 2,  -2, -2, -3, -2, -1, -2, 2,
    0,  2,  -2, -3, -1, -3, 2,  2,  -1, 2,  2,  -1, 0,  -3, 1,
  };

  float bias[] = {
    1, -1, 0, 1, 1, 1, -2,
  };

  float expected_same[] = {
    -1125, 2926,  6406,  631,   -1244, 97,    -1454, 2526,  1065,  3292,  3464,
    2553,  -330,  532,   1038,  1182,  -402,  3758,  3392,  9854,  4365,  1408,
    4736,  3134,  3838,  2409,  3221,  4350,  6750,  4045,  815,   1188,  2959,
    9802,  9590,  4572,  5740,  4253,  1701,  7974,  7012,  6854,  7093,  3907,
    4539,  3886,  4267,  3505,  465,   7824,  9219,  10026, 7968,  957,   2295,
    5594,  10811, 9641,  5950,  10043, 8783,  3132,  1421,  1110,  4108,  13929,
    10660, -84,   -61,   3932,  -180,  6811,  13393, 15147, 15640, 9337,  6961,
    3808,  1604,  1398,  1047,  6739,  10144, 6517,  4698,  2678,  7389,  2595,
    5248,  12075, 11272, 13951, 8820,  1090,  2199,  2206,  2788,  12116, 6683,
    2612,  -291,  3183,  9414,  12316, 14524, 12333, 13208, 7832,  4664,  4657,
    3534,  1298,  -666,  4250,  7707,  9103,  5760,  688,   9571,  15782, 14203,
    14878, 17339, 14684, 8690,  5671,  875,   1429,  1531,  6173,  2984,  5558,
    2996,  7928,  6733,  16117, 15262, 12757, 7980,  3923,  4795,  5973,  2051,
    455,   -1922, 1816,  5906,  3321,  10908, 10910, 7377,  12204, 12809, 11195,
    7451,  6666,  74,    -1645, -35,   -391,  3813,  7324,  892,   1656,  6095,
    12193, 14648, 12156, 14663, 10251, 10325, 7821,  3925,  323,   697,   442,
    1324,  4669,  7002,  5485,  5171,  5086,  10582, 11053, 9709,  11353, 8543,
    5256,  2873,  235,   -628,  1496,  1878,  -867,  3420,  6865,  5937,  10182,
    13277, 10069, 10789, 5998,  624,   -2082, 4417,  1258,  -1080, -819,  -1430,
    1033,  5220,  6335,  8471,  8980,  11908, 14430, 12584, 8404,  1576,  -803,
    985,   1481,  1367,  -193,  873,   3684,  2288,  6676,  9477,  11155, 9602,
    9707,  10507, 4739,  3174,  -575,  -178,  3002,  1710,  423,   -477,  554,
    3088,  2029,  5113,  5000,  3771,  6090,  5365,  1185,  2855,  399,   -312,
    -1577, 176,   955,
  };

  float expected_replicate[] = {
    13768, 13528, 12999, 6906,  4618,  4043,  2611,  9955,  6685,  4776,  2753,
    1036,  3063,  4544,  5183,  7349,  12451, 12501, 9131,  12753, 8908,  4058,
    6299,  7542,  7115,  3307,  3360,  3543,  9754,  7808,  5991,  9019,  14320,
    14919, 12492, 6871,  7373,  3336,  2085,  10604, 9377,  6882,  5009,  3103,
    6220,  6278,  7588,  10196, 11045, 11563, 11842, 11911, 8279,  2030,  1858,
    6368,  12123, 9909,  6347,  10345, 9365,  4038,  1673,  3051,  16492, 16649,
    12276, 408,   -301,  4122,  -654,  7864,  14038, 15279, 15315, 9744,  8243,
    5298,  746,   380,   9824,  9124,  10895, 6640,  4712,  2669,  6980,  2759,
    5385,  12345, 11336, 13129, 8600,  2370,  3682,  5219,  12407, 13123, 6784,
    2612,  -291,  3183,  9414,  12316, 14524, 12333, 13397, 7543,  3916,  4153,
    4477,  4314,  7983,  8418,  9163,  9103,  5760,  688,   9571,  15782, 14203,
    14878, 17718, 14570, 7940,  6642,  5094,  7133,  9964,  10219, 3224,  5558,
    2996,  7928,  6733,  16117, 15262, 12757, 7958,  4401,  5187,  5476,  5529,
    6055,  2206,  3909,  6015,  3321,  10908, 10910, 7377,  12204, 12809, 11195,
    6967,  6840,  481,   -1600, 274,   1,     10373, 8514,  1123,  2117,  6758,
    12736, 16223, 13585, 15988, 11771, 10600, 7918,  4156,  2840,  3111,  3287,
    6359,  7652,  8813,  6530,  6967,  7789,  13671, 13990, 13247, 13241, 9836,
    5251,  3024,  2313,  1834,  4187,  2637,  -1312, 2139,  7378,  7665,  11933,
    15591, 15314, 15678, 9531,  2820,  -1516, 3400,  1314,  22,    363,   -2896,
    -898,  5906,  7308,  10650, 12975, 16978, 20370, 18817, 12381, 4118,  -861,
    -137,  236,   1802,  1632,  -350,  2334,  3400,  8680,  14064, 18216, 18675,
    21765, 22871, 11491, 4937,  -1555, -11,   1669,  2392,  3265,  -5254, -217,
    5001,  8063,  13444, 18884, 19706, 22794, 21064, 9545,  6689,  -7,    289,
    -2021, 504,   2347,
  };

  float expected_valid[] = {
    2612,  -291,  3183,  9414,  12316, 14524, 12333, 9103,  5760,  688,
    9571,  15782, 14203, 14878, 5558,  2996,  7928,  6733,  16117, 15262,
    12757, 3321,  10908, 10910, 7377,  12204, 12809, 11195,
  };

  CNN_CONFIG cnn_config = { .num_layers = 3,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            {
                                {
                                    .in_channels = 1,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 3,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = 3,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 3,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = 3,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 1,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                            } };

  // Weights and biases need to be specified separately because
  // of the offset.
  AssignLayerWeightsBiases(&cnn_config, weights, bias);

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected_same, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  for (int i = 0; i < cnn_config.num_layers; ++i) {
    cnn_config.layer_config[i].pad = PADDING_SAME_REPLICATE;
  }

  RunCNNTest(image_width, image_height, input, expected_replicate, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  for (int i = 0; i < cnn_config.num_layers; ++i) {
    cnn_config.layer_config[i].pad = PADDING_VALID;
  }

  RunCNNTest(image_width, image_height, input, expected_valid, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestRELUSingleLayer) {
  int image_width = 8;
  int image_height = 8;
  int filter_height = 5;
  int filter_width = 4;
  float input[] = {
    0, -2, -3, 1,  -1, 2,  -2, 1,  -3, -1, 0,  1,  -2, -3, -2, -2,
    1, -3, 2,  -3, -1, -1, 2,  0,  -2, -3, 0,  -2, -3, 1,  -1, -1,
    2, -2, 0,  -2, -3, -3, 1,  1,  -1, 1,  0,  1,  -3, 0,  2,  2,
    0, -3, 1,  -3, 2,  -2, 1,  -1, -1, -2, -3, -2, -1, -3, -2, -1,
  };
  float expected_same[] = {
    9,  0,  1,  1,  0,  3,  0,  19, 0,  12, 10, 0,  0,  0,  5, 0,
    0,  18, 21, 7,  19, 4,  3,  0,  0,  9,  16, 0,  11, 16, 0, 11,
    12, 2,  0,  11, 0,  16, 6,  0,  8,  22, 13, 10, 12, 0,  0, 0,
    0,  1,  2,  12, 29, 6,  10, 0,  13, 0,  0,  5,  8,  10, 0, 0,
  };
  float expected_replicate[] = {
    18, 17, 12, 2,  0,  0,  5,  11, 0,  17, 22, 6,  0,  0,  17, 0,
    0,  18, 21, 7,  19, 4,  3,  5,  3,  9,  16, 0,  11, 16, 0,  3,
    3,  2,  0,  11, 0,  16, 6,  0,  17, 22, 13, 10, 12, 0,  0,  0,
    0,  4,  1,  10, 30, 7,  10, 0,  23, 8,  0,  13, 15, 19, 8,  10,
  };
  float expected_valid[] = {
    18, 21, 7, 19, 4, 9, 16, 0, 11, 16, 2, 0, 11, 0, 16, 22, 13, 10, 12, 0,
  };
  float weights[] = {
    -2, -3, 1, 2, 2, -2, -3, 0, -3, 2, 2, -3, -3, -2, 0, 1, 2, 0, -1, -1,
  };
  float bias[] = { -3 };

  CNN_CONFIG cnn_config = { .num_layers = 1,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                .in_channels = 1,
                                .filter_width = filter_width,
                                .filter_height = filter_height,
                                .out_channels = 1,
                                .skip_width = 1,
                                .skip_height = 1,
                                .maxpool = 0,
                                .weights = weights,
                                .bias = bias,
                                .pad = PADDING_SAME_ZERO,
                                .activation = RELU,
                                .deconvolve = 0,
                                .branch = 0,
                                .branch_copy_type = BRANCH_NO_COPY,
                                .branch_combine_type = BRANCH_NOC,
                                .branch_config = {},
                                .bn_params = {},
                            } } };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected_same, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].pad = PADDING_SAME_REPLICATE;

  RunCNNTest(image_width, image_height, input, expected_replicate, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].pad = PADDING_VALID;

  RunCNNTest(image_width, image_height, input, expected_valid, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestVaryingStridesVaryingDimImages) {
  float weights[] = {
    1,  -5, -3, -4, -1, 1,  2,  -3, 2,  2,  -1, 1,  -5, 1,  1,
    -3, -5, 3,  1,  4,  -2, -5, -2, -3, -5, 0,  -1, -5, 2,  -2,
    -2, 1,  -2, -4, 1,  3,  -2, 2,  0,  -3, 2,  -3, -2, -3,
  };
  float bias[] = { 2 };

  CNN_CONFIG cnn_config = { .num_layers = 1,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            {
                                {
                                    .in_channels = 1,
                                    .filter_width = 4,
                                    .filter_height = 11,
                                    .out_channels = 1,
                                    .skip_width = 7,
                                    .skip_height = 6,
                                    .maxpool = 0,
                                    .weights = weights,
                                    .bias = bias,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                            } };

  int image_height = 24;
  int image_width = 17;
  float input[] = {
    -1, -3, 4,  4,  -5, 4,  3,  -5, -1, -3, 4,  -4, 2,  -3, 3,  -5, 2,  -1, -5,
    1,  -1, 3,  1,  -3, -3, 4,  0,  2,  -3, -5, -5, -4, 0,  -5, -2, -3, -1, -2,
    2,  -5, 4,  4,  0,  -4, -3, 1,  -3, -5, -4, -4, 1,  -2, -3, 3,  -3, -3, -1,
    -5, -5, -2, 3,  1,  -1, -5, -5, 1,  -4, -2, -1, -2, -4, -4, 2,  -2, 2,  1,
    -2, -4, -1, 1,  -2, -5, 3,  -2, -1, -1, -5, -3, 1,  -2, -2, -3, -1, -2, -4,
    -2, 1,  -4, -1, 4,  3,  -4, 0,  4,  2,  2,  4,  -3, -5, 2,  2,  1,  -1, -4,
    -2, 1,  3,  2,  0,  4,  -1, -3, 2,  1,  -4, 2,  2,  -4, -2, 0,  -2, -1, 4,
    4,  2,  3,  -4, 2,  -4, -5, 4,  -1, -3, -1, 0,  -4, 1,  3,  -1, -3, -5, 3,
    -2, -4, 1,  2,  -2, -3, -3, -5, 1,  -3, -1, 0,  -1, 3,  -4, -1, -5, -5, 1,
    0,  0,  -2, -2, 2,  -2, 0,  0,  2,  0,  -3, 0,  -1, -4, -4, -1, 3,  -4, -4,
    -1, 0,  -5, -3, -2, 4,  -3, -4, -4, 0,  -5, 1,  -2, -3, -3, -4, 4,  3,  4,
    3,  3,  -1, 3,  1,  -3, -2, 3,  3,  0,  2,  -4, -3, 2,  2,  0,  -2, 4,  -2,
    2,  -2, -1, -4, -2, 2,  -4, 3,  -1, 4,  1,  1,  4,  -1, -4, -4, 1,  1,  -2,
    4,  -1, 3,  2,  -3, 4,  3,  1,  4,  0,  -4, 2,  0,  2,  4,  -2, -2, 4,  2,
    -1, -2, 1,  -3, 2,  3,  -5, -3, 4,  4,  2,  -5, -4, -5, -2, -4, 2,  0,  2,
    -5, 4,  -4, -2, -5, 2,  1,  0,  4,  1,  -2, -3, -4, -3, -4, 3,  3,  2,  0,
    -3, 1,  -5, 4,  0,  4,  -1, 3,  -5, -5, -2, -1, -1, 4,  3,  3,  4,  3,  -4,
    4,  -3, -3, -1, -4, -1, -4, -1, -2, 4,  -2, -4, 4,  4,  -3, -4, -1, 1,  2,
    -1, -2, -2, 3,  2,  2,  -3, 0,  -1, 0,  3,  2,  -5, 0,  -4, 0,  0,  2,  -4,
    -1, -1, 0,  -2, 0,  1,  0,  0,  4,  -5, -1, -5, 2,  -1, 0,  2,  -1, 1,  3,
    -3, -5, -2, -3, 4,  -2, -2, -1, -3, -4, -1, -2, -4, 1,  4,  -3, -2, -1, 3,
    -3, -2, 3,  2,  1,  -4, -3, -5, 1,
  };
  float expected_1[] = {
    41, -26, 5, 76, 13, 83, -21, 53, -54, -14, 21, 121,
  };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected_1, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].skip_width = 6;
  cnn_config.layer_config[0].skip_height = 7;

  float expected_2[] = {
    21, -50, 41, 20, 72, 127, -21, 103, 62, -37, 83, -3,
  };
  RunCNNTest(image_width, image_height, input, expected_2, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].skip_width = 3;
  cnn_config.layer_config[0].skip_height = 10;

  float expected_3[] = {
    -26, -21, -35, 69, 49,  4,  -51, -43, -56,
    -41, 15,  -44, 40, -62, 63, 38,  27,  47,
  };
  RunCNNTest(image_width, image_height, input, expected_3, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].skip_width = 10;
  cnn_config.layer_config[0].skip_height = 3;

  float expected_4[] = {
    21, 49, 28, 87, 50, 40, 102, 81, 58, 85, 51, 66, 36, 19, -37, -45,
  };

  RunCNNTest(image_width, image_height, input, expected_4, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestMaxPool) {
  int image_width = 8;
  int image_height = 8;
  int stride = 3;
  float input[] = {
    1,  -4, -4, 8, 0, 7, -5, -2, 8, 2, 2, 8,  5,  -1, -1, 9,
    -3, 0,  -2, 0, 6, 3, -4, 8,  7, 8, 7, -1, 4,  -1, 0,  2,
    -5, -2, 8,  5, 5, 4, 2,  7,  4, 6, 2, 8,  8,  -4, -3, -4,
    -3, -1, 2,  3, 3, 6, -5, 8,  9, 5, 0, -2, -1, 6,  5,  7,
  };

  float expected[] = {
    49, 58, 70, 68, 68, 70, 48, 57, 88,
  };

  float weights[] = {
    3, 1, 3, 4, -1, 5, -2, 1, -4,
  };

  float bias[] = {
    -3,
  };

  CNN_CONFIG cnn_config = { .num_layers = 1,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                .in_channels = 1,
                                .filter_width = 3,
                                .filter_height = 3,
                                .out_channels = 1,
                                .skip_width = stride,
                                .skip_height = stride,
                                .maxpool = 1,
                                .weights = weights,
                                .bias = bias,
                                .pad = PADDING_SAME_ZERO,
                                .activation = NONE,
                                .deconvolve = 0,
                                .branch = 0,
                                .branch_copy_type = BRANCH_NO_COPY,
                                .branch_combine_type = BRANCH_NOC,
                                .branch_config = {},
                                .bn_params = {},
                            } } };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestDeconvolveNonActivationSingleLayerSingleKernel) {
  int image_width = 4;
  int image_height = 7;
  float input[] = {
    9,  6,   181, 9,  218, 30, 80,  108, 68,  216, 70, 128, 179, 228,
    33, 212, 34,  14, 48,  27, 230, 23,  202, 113, 80, 56,  122, 112,
  };

  float expected_1_same[] = {
    15,   -30,  36,   -525,  377, -193, 558, 531,  6,   -24,  -15,  124,
    166,  -561, -356, -754,  -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
    433,  -311, 711,  381,   247, -317, 453, 129,  215, -627, -409, -885,
    17,   -255, -55,  -647,  -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
    133,  -719, 633,  -225,  785, 191,  463, 79,   65,  9,    77,   -853,
    -365, -949, -15,  -667,  -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
    355,  -866, 990,  207,   747, 12,   520, -116, 176, -312, -133, -1370,
    -426, -802, 143,  -771,  -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
    65,   -79,  127,  -59,   135, -90,  195, 114,  31,  -91,  -57,  -133,
    17,   -176, -72,  -276,  -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
    457,  -302, 733,  58,    470, -475, 829, 490,  227, -670, -440, -790,
    153,  -588, -294, -1150, -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
    157,  -251, 349,  -185,  409, -293, 587, 251,  77,  -187, -107, -369,
    7,    -481, -135, -827,  -3,  -3,   -3,  -3,   -3,  -3,   -3,   -3,
  };
  float expected_1_valid[] = {
    -30,  15,   -30,  36,   -525,  377,  -193,  558,  531,  24,   24,   6,
    6,    -24,  -15,  124,  166,   -561, -356,  -754, -21,  -39,  -3,   -3,
    -3,   -3,   -3,   -3,   -3,    -3,   -3,    -3,   -3,   -657, 433,  -311,
    711,  381,  247,  -317, 453,   129,  321,   321,  215,  215,  -627, -409,
    -885, 17,   -255, -55,  -647,  -219, -435,  -3,   -3,   -3,   -3,   -3,
    -3,   -3,   -3,   -3,   -3,    -3,   -207,  133,  -719, 633,  -225, 785,
    191,  463,  79,   381,  381,   65,   65,    9,    77,   -853, -365, -949,
    -15,  -667, -259, -515, -3,    -3,   -3,    -3,   -3,   -3,   -3,   -3,
    -3,   -3,   -3,   -540, 355,   -866, 990,   207,  747,  12,   520,  -116,
    633,  633,  176,  176,  -312,  -133, -1370, -426, -802, 143,  -771, -427,
    -851, -3,   -3,   -3,   -3,    -3,   -3,    -3,   -3,   -3,   -3,   -3,
    -105, 65,   -79,  127,  -59,   135,  -90,   195,  114,  78,   78,   31,
    31,   -91,  -57,  -133, 17,    -176, -72,   -276, -57,  -111, -3,   -3,
    -3,   -3,   -3,   -3,   -3,    -3,   -3,    -3,   -3,   -693, 457,  -302,
    733,  58,   470,  -475, 829,   490,  336,   336,  227,  227,  -670, -440,
    -790, 153,  -588, -294, -1150, -229, -455,  -3,   -3,   -3,   -3,   -3,
    -3,   -3,   -3,   -3,   -3,    -3,   -243,  157,  -251, 349,  -185, 409,
    -293, 587,  251,  333,  333,   77,   77,    -187, -107, -369, 7,    -481,
    -135, -827, -227, -451,
  };
  float weights_1[] = { -3, 2, -1, 3, 3, 1, 1, -3, -2, -4 };
  float bias_1[] = { -3 };

  CNN_CONFIG cnn_config = { .num_layers = 1,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                .in_channels = 1,
                                .filter_width = 5,
                                .filter_height = 2,
                                .out_channels = 1,
                                .skip_width = 2,
                                .skip_height = 3,
                                .maxpool = 0,
                                .weights = weights_1,
                                .bias = bias_1,
                                .pad = PADDING_SAME_ZERO,
                                .activation = NONE,
                                .deconvolve = 1,
                                .branch = 0,
                                .branch_copy_type = BRANCH_NO_COPY,
                                .branch_combine_type = BRANCH_NOC,
                                .branch_config = {},
                                .bn_params = {},
                            } } };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected_1_same, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  // Change padding to valid
  cnn_config.layer_config[0].pad = PADDING_VALID;

  RunCNNTest(image_width, image_height, input, expected_1_valid, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  float expected_12_same[] = {
    15,  -12,  6,    36,   -9,   -528, 377,  -184, 513,  558,  -12,  24,
    6,   -30,  -15,  -33,  -21,  166,  154,  -546, -356, -718, -30,  -21,
    433, -221, 561,  711,  -33,  -153, 247,  -83,  -87,  453,  -111, 321,
    215, -657, -409, -845, -93,  17,   -43,  -243, -55,  -215, -327, -219,
    133, -71,  -447, 633,  -219, 435,  785,  -73,  -177, 463,  -131, 381,
    65,  -207, 77,   -59,  -651, -365, -797, -213, -15,  -155, -387, -259,
    355, -182, -150, 990,  -231, 582,  747,  -36,  -540, 520,  -215, 633,
    176, -540, -133, -491, -687, -426, -882, -102, 143,  77,   -639, -427,
    65,  -37,  57,   127,  -17,  -105, 135,  -51,  60,   195,  -30,  78,
    31,  -105, -57,  -125, -45,  17,   -11,  -147, -72,  -168, -84,  -57,
    457, -233, 618,  733,  -26,  -540, 470,  -205, 264,  829,  -116, 336,
    227, -693, -440, -900, -72,  153,  107,  -609, -294, -698, -342, -229,
    157, -83,  69,   349,  -59,  -201, 409,  -125, 27,   587,  -115, 333,
    77,  -243, -107, -267, -171, 7,    -105, -369, -135, -379, -339, -227,
  };
  float expected_12_valid[] = {
    -30,  15,   -12,  6,    36,   -9,   -528, 377,  -184, 513,  558,  -12,
    24,   24,   6,    6,    -30,  -15,  -33,  -21,  166,  154,  -546, -356,
    -718, -30,  -21,  -39,  -657, 433,  -221, 561,  711,  -33,  -153, 247,
    -83,  -87,  453,  -111, 321,  321,  215,  215,  -657, -409, -845, -93,
    17,   -43,  -243, -55,  -215, -327, -219, -435, -207, 133,  -71,  -447,
    633,  -219, 435,  785,  -73,  -177, 463,  -131, 381,  381,  65,   65,
    -207, 77,   -59,  -651, -365, -797, -213, -15,  -155, -387, -259, -515,
    -540, 355,  -182, -150, 990,  -231, 582,  747,  -36,  -540, 520,  -215,
    633,  633,  176,  176,  -540, -133, -491, -687, -426, -882, -102, 143,
    77,   -639, -427, -851, -105, 65,   -37,  57,   127,  -17,  -105, 135,
    -51,  60,   195,  -30,  78,   78,   31,   31,   -105, -57,  -125, -45,
    17,   -11,  -147, -72,  -168, -84,  -57,  -111, -693, 457,  -233, 618,
    733,  -26,  -540, 470,  -205, 264,  829,  -116, 336,  336,  227,  227,
    -693, -440, -900, -72,  153,  107,  -609, -294, -698, -342, -229, -455,
    -243, 157,  -83,  69,   349,  -59,  -201, 409,  -125, 27,   587,  -115,
    333,  333,  77,   77,   -243, -107, -267, -171, 7,    -105, -369, -135,
    -379, -339, -227, -451,
  };

  // Change skip_width, skip_height to {2, 3}
  cnn_config.layer_config[0].skip_width = 3;
  cnn_config.layer_config[0].skip_height = 2;
  // Set padding to same
  cnn_config.layer_config[0].pad = PADDING_SAME_ZERO;

  RunCNNTest(image_width, image_height, input, expected_12_same, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  // Change padding to valid
  cnn_config.layer_config[0].pad = PADDING_VALID;
  RunCNNTest(image_width, image_height, input, expected_12_valid, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].filter_width = 4;
  cnn_config.layer_config[0].filter_height = 3;
  float weights_2[] = { -1, -3, -1, -3, 0, 2, -2, 4, 3, 0, 1, 4 };
  float bias_2[] = { -4 };
  cnn_config.layer_config[0].weights = weights_2;
  cnn_config.layer_config[0].bias = bias_2;

  cnn_config.layer_config[0].skip_width = 5;
  cnn_config.layer_config[0].skip_height = 2;
  float expected_2_same[] = {
    -13,  -31,  -13,  -31,  -4,   -10,  -22,  -10,  -22,  -4,   -185, -547,
    -185, -547, -4,   -13,  -31,  -13,  -31,  -4,   -4,   14,   -22,  32,
    -4,   -4,   8,    -16,  20,   -4,   -4,   358,  -366, 720,  -4,   -4,
    14,   -22,  32,   -4,   -195, -658, -213, -622, -4,   -16,  -94,  -28,
    -70,  -4,   459,  -244, 97,   480,  -4,   -85,  -328, -103, -292, -4,
    -4,   432,  -440, 868,  -4,   -4,   56,   -64,  116,  -4,   -4,   156,
    -164, 316,  -4,   -4,   212,  -220, 428,  -4,   582,  -208, 146,  664,
    -4,   -130, -652, -190, -532, -4,   166,  -214, 6,    106,  -4,   192,
    -388, -24,  44,   -4,   -4,   132,  -140, 268,  -4,   -4,   428,  -436,
    860,  -4,   -4,   136,  -144, 276,  -4,   -4,   252,  -260, 508,  -4,
    21,   -541, -115, -269, -4,   416,  -688, -16,  176,  -4,   173,  -103,
    33,   177,  -4,   168,  -640, -88,  -128, -4,   -4,   354,  -362, 712,
    -4,   -4,   452,  -460, 908,  -4,   -4,   62,   -70,  128,  -4,   -4,
    420,  -428, 844,  -4,   499,  -106, 141,  610,  -4,   666,  -46,  210,
    866,  -4,   47,   -148, -19,  -16,  -4,   605,  -85,  181,  763,  -4,
    -4,   64,   -72,  132,  -4,   -4,   24,   -32,  52,   -4,   -4,   92,
    -100, 188,  -4,   -4,   50,   -58,  104,  -4,   -132, -694, -200, -558,
    -4,   15,   -73,  -13,  -17,  -4,   -62,  -610, -158, -418, -4,   -36,
    -343, -90,  -235, -4,   -4,   456,  -464, 916,  -4,   -4,   42,   -50,
    88,   -4,   -4,   400,  -408, 804,  -4,   -4,   222,  -230, 448,  -4,
    606,  -244, 146,  676,  -4,   9,    -172, -37,  -80,  -4,   480,  -370,
    76,   438,  -4,   223,  -340, -3,   112,  -4,   -4,   156,  -164, 316,
    -4,   -4,   108,  -116, 220,  -4,   -4,   240,  -248, 484,  -4,   -4,
    220,  -228, 444,  -4,
  };
  float expected_2_valid[] = {
    -13,  -31,  -13,  -31,  -4,   -10,  -22,  -10,  -22,  -4,   -185, -547,
    -185, -547, -4,   -13,  -31,  -13,  -31,  -4,   14,   -22,  32,   -4,
    -4,   8,    -16,  20,   -4,   -4,   358,  -366, 720,  -4,   -4,   14,
    -22,  32,   -195, -658, -213, -622, -4,   -16,  -94,  -28,  -70,  -4,
    459,  -244, 97,   480,  -4,   -85,  -328, -103, -292, -4,   432,  -440,
    868,  -4,   -4,   56,   -64,  116,  -4,   -4,   156,  -164, 316,  -4,
    -4,   212,  -220, 428,  582,  -208, 146,  664,  -4,   -130, -652, -190,
    -532, -4,   166,  -214, 6,    106,  -4,   192,  -388, -24,  44,   -4,
    132,  -140, 268,  -4,   -4,   428,  -436, 860,  -4,   -4,   136,  -144,
    276,  -4,   -4,   252,  -260, 508,  21,   -541, -115, -269, -4,   416,
    -688, -16,  176,  -4,   173,  -103, 33,   177,  -4,   168,  -640, -88,
    -128, -4,   354,  -362, 712,  -4,   -4,   452,  -460, 908,  -4,   -4,
    62,   -70,  128,  -4,   -4,   420,  -428, 844,  499,  -106, 141,  610,
    -4,   666,  -46,  210,  866,  -4,   47,   -148, -19,  -16,  -4,   605,
    -85,  181,  763,  -4,   64,   -72,  132,  -4,   -4,   24,   -32,  52,
    -4,   -4,   92,   -100, 188,  -4,   -4,   50,   -58,  104,  -132, -694,
    -200, -558, -4,   15,   -73,  -13,  -17,  -4,   -62,  -610, -158, -418,
    -4,   -36,  -343, -90,  -235, -4,   456,  -464, 916,  -4,   -4,   42,
    -50,  88,   -4,   -4,   400,  -408, 804,  -4,   -4,   222,  -230, 448,
    606,  -244, 146,  676,  -4,   9,    -172, -37,  -80,  -4,   480,  -370,
    76,   438,  -4,   223,  -340, -3,   112,  -4,   156,  -164, 316,  -4,
    -4,   108,  -116, 220,  -4,   -4,   240,  -248, 484,  -4,   -4,   220,
    -228, 444,  236,  -4,   76,   316,  -4,   164,  -4,   52,   220,  -4,
    362,  -4,   118,  484,  -4,   332,  -4,   108,  444,
  };
  // Set padding to same
  cnn_config.layer_config[0].pad = PADDING_SAME_ZERO;

  RunCNNTest(image_width, image_height, input, expected_2_same, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].pad = PADDING_VALID;

  RunCNNTest(image_width, image_height, input, expected_2_valid, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].skip_width = 2;
  cnn_config.layer_config[0].skip_height = 5;
  float expected_21_same[] = {
    -31,  -19,  -49,   -191, -565, -194, -574, -13,  14,   -22,  44,   -16,
    382,  -366, 738,   -22,  -4,   23,   32,   545,  20,   204,  720,  5,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,   -658, -252, -748, -114, -334, -192, -568, -112,
    432,  -440, 928,   -64,  276,  -164, 532,  -220, -4,   304,  868,  266,
    116,  400,  316,   104,  -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -208, -288, -856, -290,
    -862, -202, -598,  -132, 132,  -140, 700,  -436, 1000, -144, 532,  -260,
    -4,   712,  268,   422,  860,  450,  276,  124,  -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -541, -411, -1225, -265, -787, -249, -739, -216, 354,  -362, 1168, -460,
    974,  -70,  552,   -428, -4,   859,  712,  323,  908,  665,  128,  208,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,   -106, -52,  -148, -66,  -190, -79,  -229, -31,
    64,   -72,  160,   -32,  148,  -100, 242,  -58,  -4,   72,   132,  154,
    52,   125,  188,   23,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -694, -257, -763, -229,
    -679, -319, -949,  -117, 456,  -464, 962,  -50,  492,  -408, 1030, -230,
    -4,   295,  916,   625,  88,   537,  804,  109,  -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -244, -140, -412,  -182, -538, -238, -706, -116, 156,  -164, 428,  -116,
    464,  -248, 708,   -228, -4,   244,  316,  418,  220,  454,  484,  108,
    -4,   -4,   -4,    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,
    -4,   -4,   -4,    -4,
  };
  float expected_21_valid[] = {
    -13,  -31,  -19,  -49,  -191, -565, -194, -574, -13,  -31,   -4,   14,
    -22,  44,   -16,  382,  -366, 738,  -22,  32,   23,   -4,    23,   32,
    545,  20,   204,  720,  5,    32,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -222, -658, -252, -748, -114, -334, -192, -568,  -112, -328,
    -4,   432,  -440, 928,  -64,  276,  -164, 532,  -220, 428,   650,  -4,
    304,  868,  266,  116,  400,  316,  104,  428,  -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -72,  -208, -288, -856, -290, -862,  -202, -598,
    -132, -388, -4,   132,  -140, 700,  -436, 1000, -144, 532,   -260, 508,
    200,  -4,   712,  268,  422,  860,  450,  276,  124,  508,   -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -183, -541, -411, -1225, -265, -787,
    -249, -739, -216, -640, -4,   354,  -362, 1168, -460, 974,   -70,  552,
    -428, 844,  533,  -4,   859,  712,  323,  908,  665,  128,   208,  844,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -38,  -106,  -52,  -148,
    -66,  -190, -79,  -229, -31,  -85,  -4,   64,   -72,  160,   -32,  148,
    -100, 242,  -58,  104,  98,   -4,   72,   132,  154,  52,    125,  188,
    23,   104,  -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -234, -694,
    -257, -763, -229, -679, -319, -949, -117, -343, -4,   456,   -464, 962,
    -50,  492,  -408, 1030, -230, 448,  686,  -4,   295,  916,   625,  88,
    537,  804,  109,  448,  -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,   -4,    -4,   -4,
    -84,  -244, -140, -412, -182, -538, -238, -706, -116, -340,  -4,   156,
    -164, 428,  -116, 464,  -248, 708,  -228, 444,  236,  -4,    244,  316,
    418,  220,  454,  484,  108,  444,
  };

  cnn_config.layer_config[0].pad = PADDING_SAME_ZERO;

  RunCNNTest(image_width, image_height, input, expected_21_same, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);

  cnn_config.layer_config[0].pad = PADDING_VALID;

  RunCNNTest(image_width, image_height, input, expected_21_valid, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestLargeKernelsAndStrides) {
  float input_10x11[] = {
    4,  4,  2,  4,  2,  -5, -2, 3, -1, 0,  0,  1,  2,  0,  -5, -2, -5, 1,  -3,
    -1, 4,  -3, 2,  -2, 1,  0,  1, -3, -3, -4, -2, -2, 1,  -4, -1, 4,  1,  -4,
    -4, -4, 3,  2,  -5, 3,  -5, 1, 2,  -4, 1,  -1, 3,  4,  -2, 3,  -3, 3,  0,
    2,  -4, -5, -5, -2, -1, -2, 1, 1,  1,  -2, 4,  -5, 4,  -1, -1, 2,  3,  -4,
    2,  2,  3,  0,  0,  1,  0,  3, 2,  3,  1,  -2, 3,  -4, 3,  2,  4,  -2, 0,
    4,  -4, 1,  -3, -3, -3, -5, 1, -3, -5, 0,  4,  -1, -3, 2,
  };

  float weights_10x11[] = {
    -3, 4,  -4, -3, -5, 1,  -2, 3,  1,  -4, -4, 0,  -1, 0,  3,  1,  -3, -2, 0,
    -1, 1,  3,  -4, -4, -3, -3, -2, 4,  3,  -5, 4,  2,  -3, 4,  -2, -1, 2,  -1,
    -5, 0,  -3, 0,  3,  -5, -5, 3,  -4, -1, -5, 3,  4,  0,  4,  -5, 2,  -1, 2,
    -1, -1, -1, -5, 0,  -4, 3,  -1, 1,  1,  -1, 3,  2,  -5, -4, 0,  -4, 4,  -5,
    -3, 4,  -5, 2,  -5, -4, -4, -1, 3,  3,  0,  2,  -4, 1,  -2, 1,  1,  0,  3,
    -2, 0,  1,  2,  4,  -3, -1, -5, -5, 2,  -4, 1,  1,  2,  -4, -2, -2, 2,  1,
    3,  4,  -5, 1,  -1, -3, -3, -1, -2, -5, 1,  -1, 0,  1,  4,  4,  0,  0,  4,
    -3, -1, -5, -3, 0,  1,  1,  1,  -5, 3,  4,  3,  -5, 3,  -2, -2, 0,  -4, 0,
    0,  -2, 1,  -4, -1, 0,  -5, -2, -2, -5, -3, -3, 1,  1,  -3, 2,  4,  2,  4,
    -4, -3, 3,  1,  1,  3,  -4, 4,  -2, -3, -3, -3, -3, -4, -2, 3,  -5, 2,  4,
    -1, -4, -4, 4,  -2, -1, 3,  -3, -4, -4, -2, 4,  1,  0,  2,  -1, 4,  -3, 1,
    4,  -3, 4,  4,  0,  -4, 3,  -2, -3, 2,  3,  -1, -3, 2,  1,  4,  -2, -3, 1,
    4,  -2, 2,  -2, -5, -2, 1,  4,  -1, -4, 4,  -5, 2,  -5, -4, -1, -2, 3,  1,
    2,  1,  -5, 1,  -5, -4, -1, -2, 2,  -2, -4, -3, -2, -2, 4,  -1, 2,  2,  -4,
    2,  -2, 4,  -4, -2, -2, 1,  -1, 1,  1,  1,  -4, -5, -2, 3,  -4, -1, 3,  -2,
    3,  2,  -5, -4, 0,  3,  -2, -4, -5, 3,  -2, -4, 2,  -2, 1,  -4, 0,  2,  -5,
    1,  -4, -1, -1, 4,  -5, -4, 0,  -5, -4, -3, -5, -4, 0,  2,  0,  -4, 2,  -2,
    1,  1,  -3, 2,  0,  -4, 0,  -4, 1,  0,  -5, -1, -1, -1, -5, 4,  2,  2,  -4,
    3,  -2, -2, 2,  -3, -2, -1, 2,  -4, -5, 2,  -2, -4, -5, -5, -1, 2,  -1, 0,
    -5, -2, -2, -5, 0,  1,  -1, -5, 0,  3,  2,  3,  0,  -3, -2, 0,  -5, -1, -2,
    2,  -4, -1, 2,  2,  -5, 2,  -4, 0,  3,  -3, 1,  0,  0,  1,  -5, -3, 1,  -1,
    0,  -4, -3, 2,  -4, -4, 4,  -1, 0,  1,  2,  -4, -5, 4,  -2, 1,  -4, -4, -3,
    -1, -1, 1,  -1, -4, -1, -4, -3, 2,  -1, -2, -4, 1,  1,  0,  -2, 0,  -4, 3,
    -3, 0,  -4, -1, -4, 2,  -1, -2, -5, -1, -2, -3, 3,  -1, 0,  -3, 0,  1,  -5,
    1,  -5, 0,  1,
  };

  float bias_10x11[] = { 3 };

  float expected_10x11[] = {
    118,
  };

  CNN_CONFIG cnn_config = { .num_layers = 1,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                .in_channels = 1,
                                .filter_width = 23,
                                .filter_height = 20,
                                .out_channels = 1,
                                .skip_width = 15,
                                .skip_height = 20,
                                .maxpool = 0,
                                .weights = weights_10x11,
                                .bias = bias_10x11,
                                .pad = PADDING_SAME_ZERO,
                                .activation = NONE,
                                .deconvolve = 0,
                                .branch = 0,
                                .branch_copy_type = BRANCH_NO_COPY,
                                .branch_combine_type = BRANCH_NOC,
                                .branch_config = {},
                                .bn_params = {},
                            } } };

  int image_height = 10;
  int image_width = 11;

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input_10x11, expected_10x11,
             &cnn_config, image_width, &thread_data, MSE_INT_TOL);

  float input_11x10[] = {
    -2, -2, 3,  -5, -1, -3, 1,  3,  2,  1,  1,  -5, 4,  1,  3,  -5, 3,  -3, -5,
    0,  -1, -3, -3, 1,  1,  -5, -1, -5, -5, -3, 0,  1,  -3, -1, -3, -3, 0,  3,
    4,  -4, -1, 3,  -3, -1, -3, 1,  -3, -2, -1, -4, -3, 2,  -4, 1,  -4, -1, -3,
    -5, -1, 2,  3,  0,  2,  2,  -5, 4,  1,  2,  -1, -4, 4,  -4, -4, 0,  -1, 1,
    -1, 1,  -3, -3, -2, 1,  2,  4,  4,  4,  -3, -3, 0,  1,  0,  1,  4,  1,  3,
    4,  -3, -2, -4, 4,  2,  0,  3,  4,  -1, 2,  -2, 1,  -3, -2,
  };

  float weights_11x10[] = {
    4,  -1, 1,  -1, 2,  4,  3,  3,  -4, 3,  -5, 1,  -1, -1, -2, -2, 0,  2,  -3,
    -2, 3,  -5, -1, 0,  -1, -2, -2, -1, 2,  4,  3,  1,  0,  0,  -3, 3,  -4, -1,
    -5, 4,  -2, -2, 1,  2,  -1, -3, 1,  2,  -5, 1,  -3, 3,  3,  0,  -4, -4, -5,
    -3, -4, -4, 4,  -2, 4,  4,  -2, 2,  -5, -1, -2, -5, -1, 4,  -3, 3,  -2, 0,
    -4, -3, 0,  -1, -2, 4,  2,  0,  -2, -5, -4, 1,  4,  -4, -2, 2,  -2, 1,  1,
    -4, 1,  -4, -4, -2, 4,  2,  -1, -5, -5, 1,  -3, -3, 3,  -3, -5, -3, 4,  -1,
    -1, -3, 0,  -4, 3,  -1, 0,  -2, 0,  -5, -2, -5, 2,  0,  -5, 2,  3,  -2, 2,
    4,  -1, 1,  -3, 2,  3,  2,  0,  -5, -4, -5, 2,  1,  1,  -1, -2, 3,  4,  2,
    -2, 4,  -2, 3,  1,  -4, -3, -1, 4,  4,  -3, -5, -2, 2,  0,  3,  -2, 3,  -1,
    -4, 0,  -2, 0,  3,  4,  -2, -3, -2, 0,  3,  4,  2,  -4, 0,  1,  2,  2,  -1,
    -1, 4,  1,  4,  -2, -1, -1, -5, 1,  -3, 3,  3,  -1, -4, 3,  -5, 0,  0,  -1,
    -4, -1, -2, 4,  -2, 3,  3,  -3, 1,  -1, 2,  -1, 4,  4,  -2, -2, 4,  -2, 0,
    3,  -3, -5, -1, -2, 4,  -4, 2,  -4, 0,  -2, 3,  -3, 2,  2,  -2, -5, -1, 4,
    3,  -2, -1, 3,  3,  -1, 3,  0,  -3, 0,  4,  2,  0,  -1, 4,  1,  1,  2,  1,
    3,  1,  1,  1,  -3, -5, -4, 4,  -4, 2,  0,  0,  -4, 1,  4,  -5, 4,  4,  0,
    1,  0,  -2, -4, -4, -3, 0,  1,  -5, 4,  0,  -3, -2, -4, 2,  4,  1,  -5, 1,
    -4, 1,  0,  -3, -3, 0,  2,  -5, 4,  3,  -2, -5, 3,  1,  -1, 0,  3,  -2, -2,
    3,  -2, -5, 4,  1,  -2, 2,  -1, 0,  4,  0,  -5, 3,  -2, 1,  2,  1,  -5, -3,
    -2, -5, 4,  -4, 0,  3,  2,  -1, -4, -1, 2,  1,  -2, 3,  -1, -4, 2,  0,  -3,
    1,  -1, 2,  -5, -4, -1, -5, 1,  4,  3,  4,  2,  -3, 1,  -5, -1, 3,  0,  -1,
    -4, 3,  4,  -5, 4,  4,  -3, 2,  -3, -1, -3, -5, -3, 2,  -3, -2, 1,  1,  0,
    -5, 3,  2,  1,  -5, 1,  1,  1,  3,  4,  -4, -1, -2, 0,  -5, -3, -5, -2, -4,
    3,  3,  3,  4,  0,  -4, -1, -5, 0,  -3, 1,  4,  4,  -4, 4,  -5, -5, -1, -2,
    -5, 3,  -4, 4,  3,  0,  -3, 2,  -2, 0,  0,  4,  4,  0,  -2, 1,  -1, -3, 2,
    -1, 1,  -3, -5,
  };

  float bias_11x10[] = {
    -5,
  };

  float expected_11x10[] = {
    36,  -84,  95,   45,  18,   46,   77,  -54, -99,  -149, 66,  49,  161, 11,
    39,  61,   -66,  61,  4,    -3,   34,  -44, -23,  31,   64,  29,  47,  72,
    -27, -27,  121,  -3,  100,  1,    30,  -78, -12,  -89,  -59, 8,   -16, 112,
    91,  -102, -26,  -4,  30,   54,   4,   -84, -24,  -58,  27,  -53, -33, 5,
    53,  -26,  63,   50,  -103, -130, -23, 6,   -104, -207, 73,  23,  77,  132,
    38,  32,   -130, -44, -60,  7,    27,  176, 45,   -32,  -2,  99,  -97, 63,
    69,  126,  47,   63,  136,  -57,  5,   16,  -40,  -157, 8,   38,  -44, -10,
    91,  7,    122,  140, 30,   -105, 4,   -1,  113,  64,   180, 141,
  };

  cnn_config.layer_config[0].weights = weights_11x10;
  cnn_config.layer_config[0].bias = bias_11x10;
  cnn_config.layer_config[0].filter_width = 20;
  cnn_config.layer_config[0].filter_height = 23;
  cnn_config.layer_config[0].skip_width = 1;
  cnn_config.layer_config[0].skip_height = 1;
  image_height = 11;
  image_width = 10;

  RunCNNTest(image_width, image_height, input_11x10, expected_11x10,
             &cnn_config, image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestSoftsignSingleLayer) {
  int image_width = 8;
  int image_height = 8;
  int filter_height = 5;
  int filter_width = 4;
  float input[] = {
    -0.5220f, 0.8410f,  -0.8990f, -0.0090f, 0.6710f,  -0.9470f, -0.8240f,
    -0.0870f, 0.5380f,  0.4750f,  0.570f,   -0.3760f, -0.6960f, -0.5940f,
    -0.3830f, 0.080f,   -0.0980f, -0.4940f, -0.4030f, 0.9460f,  -0.6020f,
    0.4220f,  0.6190f,  0.6640f,  -0.9210f, -0.1470f, -0.2480f, -0.1120f,
    -0.580f,  -0.0650f, 0.3330f,  0.9860f,  -0.7430f, 0.7610f,  0.4840f,
    0.1030f,  0.9570f,  0.6120f,  -0.5240f, -0.1220f, -0.5850f, -0.270f,
    0.7840f,  -0.9790f, 0.7290f,  -0.30f,   -0.6460f, 0.0780f,  0.4750f,
    -0.0510f, 0.4550f,  0.3850f,  -0.7230f, 0.4460f,  -0.6260f, -0.810f,
    0.8720f,  -0.2120f, -0.580f,  -0.9510f, -0.8430f, -0.1340f, -0.0850f,
    0.9190f,
  };
  float expected_same[] = {
    0.430f,   0.660f,  0.5510f,  -0.610f,  0.450f,  -0.1610f, 0.0520f,  0.3240f,
    0.6820f,  0.3820f, 0.6360f,  0.7480f,  0.3080f, 0.090f,   0.3910f,  0.1730f,
    0.340f,   0.6660f, -0.4990f, 0.4280f,  0.1540f, 0.120f,   0.4670f,  0.6150f,
    -0.3880f, 0.7590f, 0.4190f,  0.7350f,  0.5310f, -0.5160f, -0.1760f, 0.6790f,
    -0.6780f, 0.5470f, 0.5750f,  -0.6420f, 0.7210f, -0.4620f, 0.5430f,  0.770f,
    -0.1990f, 0.3950f, 0.7860f,  -0.4380f, 0.7540f, 0.2640f,  -0.6430f, 0.4510f,
    -0.1260f, 0.1590f, -0.2110f, -0.0560f, 0.6570f, 0.680f,   0.5870f,  0.4720f,
    0.4040f,  0.3630f, 0.670f,   0.2360f,  0.410f,  0.6980f,  -0.5350f, 0.3940f,
  };
  float expected_replicate[] = {
    0.540f,   0.7230f,  -0.3530f, -0.2130f, 0.7440f,  -0.4470f, -0.6260f,
    -0.2050f, 0.7230f,  0.4630f,  0.5920f,  0.7440f,  0.6080f,  0.3130f,
    -0.5670f, -0.4720f, 0.5480f,  0.6660f,  -0.4990f, 0.4280f,  0.1540f,
    0.120f,   0.3390f,  0.6090f,  0.4160f,  0.7590f,  0.4190f,  0.7350f,
    0.5310f,  -0.5160f, -0.490f,  0.4450f,  -0.610f,  0.5470f,  0.5750f,
    -0.6420f, 0.7210f,  -0.4620f, 0.3150f,  0.7370f,  -0.5820f, 0.3950f,
    0.7860f,  -0.4380f, 0.7540f,  0.2640f,  -0.7430f, -0.5340f, -0.6270f,
    0.4430f,  0.4730f,  0.4570f,  0.7450f,  0.630f,   0.2620f,  0.3140f,
    -0.1840f, 0.1810f,  0.7210f,  0.2760f,  0.6430f,  0.6720f,  -0.4390f,
    0.2040f,
  };
  float expected_valid[] = {
    0.6660f,  -0.4990f, 0.4280f,  0.1540f,  0.120f,  0.7590f,  0.4190f,
    0.7350f,  0.5310f,  -0.5160f, 0.5470f,  0.5750f, -0.6420f, 0.7210f,
    -0.4620f, 0.3950f,  0.7860f,  -0.4380f, 0.7540f, 0.2640f,
  };
  float weights[] = {
    0.6210f,  0.3710f,  -0.2770f, -0.7230f, -0.2450f, 0.6770f,  0.3080f,
    -0.9880f, -0.080f,  0.7190f,  -0.6760f, -0.0170f, -0.8970f, 0.8260f,
    0.7390f,  -0.4550f, -0.4260f, -0.6330f, 0.0880f,  -0.9390f,
  };
  float bias[] = {
    0.750f,
  };

  CNN_CONFIG cnn_config = { .num_layers = 1,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                .in_channels = 1,
                                .filter_width = filter_width,
                                .filter_height = filter_height,
                                .out_channels = 1,
                                .skip_width = 1,
                                .skip_height = 1,
                                .maxpool = 0,
                                .weights = weights,
                                .bias = bias,
                                .pad = PADDING_SAME_ZERO,
                                .activation = SOFTSIGN,
                                .deconvolve = 0,
                                .branch = 0,
                                .branch_copy_type = BRANCH_NO_COPY,
                                .branch_combine_type = BRANCH_NOC,
                                .branch_config = {},
                                .bn_params = {},
                            } } };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected_same, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);

  cnn_config.layer_config[0].pad = PADDING_SAME_REPLICATE;

  RunCNNTest(image_width, image_height, input, expected_replicate, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);

  cnn_config.layer_config[0].pad = PADDING_VALID;

  RunCNNTest(image_width, image_height, input, expected_valid, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);
}

TEST_F(CNNTest, TestBranchTensorAdd) {
  int filter_width = 2;
  int filter_height = 3;

  int image_width = 4;
  int image_height = 4;

  float input[] = {
    -3, -2, -2, 0, -1, 3, 2, -2, 1, 3, 4, 0, 2, -5, -4, 0,
  };

  float weights[] = {
    -3, -1, 4,  -1, -3, 3,  3,  0,  2,  0,  3,  2,  4,  4, 4,  -5, 1, -4,
    2,  -4, 1,  -3, 0,  4,  -5, 4,  0,  -4, -3, -1, 0,  0, -2, 0,  0, 2,
    -5, -1, 1,  -3, 3,  4,  3,  0,  1,  -1, 1,  1,  2,  4, -2, -5, 2, -2,
    3,  -2, 4,  -1, 0,  2,  3,  2,  -2, -1, -3, 1,  3,  4, -1, -3, 0, -4,
    4,  2,  -3, -3, -1, 0,  1,  0,  3,  3,  -3, 0,  3,  2, -5, -3, 4, -5,
    3,  -1, -1, -3, 0,  1,  -1, -4, 2,  4,  -1, 4,  -1, 1, 3,  4,  4, 4,
    0,  -1, -3, -3, -3, -3, 2,  -3, -2, 2,  3,  -3,
  };

  float bias[] = {
    3, 4, -1, -1, 2, 1, -2, 1, 4, 1, 3,
  };

  float expected[] = {
    -11502, -4101, -3424, 668,   -17950, -5470, -5504, 626,
    4835,   446,   1779,  -3483, 3679,   -4214, 4578,  -105,
  };

  int channels = 2;

  CNN_CONFIG cnn_config = { .num_layers = 6,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                  .in_channels = 1,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = weights,
                                  .bias = bias,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_INPUT,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {
                                  .input_to_branches = 0x02,
                                  .channels_to_copy = 0,
                                  .branches_to_combine = 0x00,
                                },
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 1,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 1,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_ADD,
                                  .branch_config = {
                                  .input_to_branches = 0x00,
                                  .channels_to_copy = 0,
                                  .branches_to_combine = 0x02,
                                },
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = 1,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              } } };

  // Weights and biases need to be specified separately because
  // of the offset.
  AssignLayerWeightsBiases(&cnn_config, weights, bias);

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestBranchTensorConcatenation) {
  int filter_width = 2;
  int filter_height = 3;

  int image_width = 4;
  int image_height = 4;

  float input[] = {
    -3, -2, -2, 0, -1, 3, 2, -2, 1, 3, 4, 0, 2, -5, -4, 0,
  };

  float weights[] = {
    3,  0,  2,  0,  2,  3,  1,  -3, 1,  -5, -3, 0,  -4, 4,  0,  -5, 0,  -5, -1,
    -2, -5, 0,  -3, 2,  -4, 2,  0,  2,  -1, 0,  -4, 3,  0,  0,  -1, -5, 2,  -1,
    4,  -4, -2, -3, -3, 3,  4,  -2, -1, -4, -1, 4,  4,  -1, 4,  3,  -4, 2,  -2,
    -4, -3, -2, 3,  -3, -5, -1, 3,  -2, 4,  1,  -4, -3, -5, -5, -3, 4,  -2, -2,
    -1, -5, -5, 0,  -1, -2, -3, 3,  -4, -5, 2,  -3, 1,  0,  -5, 2,  2,  -2, 0,
    2,  2,  -2, 4,  2,  2,  0,  1,  -5, -3, 0,  2,  -2, 1,  2,  -5, 2,  3,  3,
    -1, 3,  0,  -3, 3,  -4, -4, 3,  3,  -4, -2, 2,  -2, 2,  -2, -1, 3,  0,
  };

  float bias[] = {
    -3, -5, 4, -4, -3, -2, 0, 3, -4, 4, -3,
  };

  float expected[] = {
    -33533, -32087, -6741,  -2124, 39979, 41453, 14034, 689,
    -22611, -42203, -14882, -239,  15781, 15963, 9524,  837,
  };

  int channels = 2;

  CNN_CONFIG cnn_config = { .num_layers = 6,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            { {
                                  .in_channels = 1,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = weights,
                                  .bias = bias,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_INPUT,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {
                                  .input_to_branches = 0x02,
                                  .channels_to_copy = 0,
                                  .branches_to_combine = 0x00,
                                },
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 1,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 1,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = channels,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_CAT,
                                  .branch_config = {
                                  .input_to_branches = 0x00,
                                  .channels_to_copy = 0,
                                  .branches_to_combine = 0x02,
                                },
                                  .bn_params = {},
                              },
                              {
                                  .in_channels = channels + channels,
                                  .filter_width = filter_width,
                                  .filter_height = filter_height,
                                  .out_channels = 1,
                                  .skip_width = 1,
                                  .skip_height = 1,
                                  .maxpool = 0,
                                  .weights = nullptr,
                                  .bias = nullptr,
                                  .pad = PADDING_SAME_ZERO,
                                  .activation = NONE,
                                  .deconvolve = 0,
                                  .branch = 0,
                                  .branch_copy_type = BRANCH_NO_COPY,
                                  .branch_combine_type = BRANCH_NOC,
                                  .branch_config = {},
                                  .bn_params = {},
                              } } };

  // Weights and biases need to be specified separately because
  // of the offset.
  AssignLayerWeightsBiases(&cnn_config, weights, bias);

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

// TODO(logangw): Add test to test all combinations of branch_copy_type.

TEST_F(CNNTest, TestBranchCombinations) {
  int filter_width = 2;
  int filter_height = 3;

  int image_width = 4;
  int image_height = 4;

  float input[] = {
    3, 2, -5, -4, 4, -2, -4, -3, 4, 2, -3, 2, -3, 1, -5, -1,
  };

  float weights[] = {
    2,  3,  0,  4,  4,  3,  1,  0,  1,  -5, 4,  -3, 3,  0,  4,  -1, -1, -5,
    2,  1,  -3, -5, 3,  -1, -3, -2, 0,  -2, 3,  0,  -2, -4, -2, -2, 2,  -5,
    4,  -5, 0,  1,  -5, -4, -3, -4, 2,  -2, 1,  0,  3,  -2, -4, 3,  4,  -4,
    -1, -1, -3, -2, -2, -1, 2,  0,  2,  -1, 2,  -4, -4, -1, 2,  0,  3,  -2,
    -2, 3,  -3, 4,  -2, 4,  3,  4,  1,  0,  -2, -3, -5, 1,  -3, 2,  0,  -2,
    -2, -1, -1, -5, -2, -3, -1, 3,  3,  4,  4,  0,  2,  1,  3,  -3, 2,  -5,
    -5, 1,  -5, -1, 3,  3,  2,  -4, -1, 3,  -4, -2, -5, -2, 1,  3,  2,  2,
    -5, -2, -3, -1, -2, -4, -1, -2, 2,  1,  -4, -4, 2,  0,  2,  0,  2,  -3,
    -2, -4, 4,  0,  1,  -3, -5, 4,  -1, 2,  3,  -5, -1, 0,  4,  -1, -1, 3,
    -1, -3, 3,  1,  4,  3,  4,  3,  -4, -5, -1, 3,  3,  -4, 3,  1,  3,  -5,
    3,  4,  -5, 4,  2,  -1, -5, 2,  1,  0,  4,  0,  -3, 2,  0,  2,  -2, 1,
    -1, -2, -1, -5, 4,  3,  3,  -2, 2,  4,  -5, -5, -3, -2, 4,  0,  -4, 1,
  };

  float bias[] = {
    -1, 4, 0, 2, 2, -2, 0, -4, -5, -1, 1, -2, 3, 0, 4, -2, 1, 0, 0,
  };

  float expected[] = {
    149496, 15553,  -24193, -20956, 134094, 86432,  -68283, -6366,
    -53031, 133739, 67407,  -13539, -53205, -58635, -20033, 1979,
  };

  int channels = 2;

  CNN_CONFIG cnn_config = { .num_layers = 10,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            {
                                {
                                    .in_channels = 1,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = weights,
                                    .bias = bias,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_INPUT,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {
                                    .input_to_branches = 0x06,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x00,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 2,
                                    .branch_copy_type = BRANCH_OUTPUT,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {
                                    .input_to_branches = 0x08,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x00,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 3,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 2,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_ADD,
                                    .branch_config = {
                                    .input_to_branches = 0x00,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x08,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 2,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 1,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 1,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_ADD,
                                    .branch_config = {
                                    .input_to_branches = 0x00,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x0C,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = channels,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_ADD,
                                    .branch_config = {
                                    .input_to_branches = 0x00,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x02,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = channels,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 1,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                            } };

  // Weights and biases need to be specified separately because
  // of the offset.
  AssignLayerWeightsBiases(&cnn_config, weights, bias);

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestSplittingTensors) {
  int filter_width = 2;
  int filter_height = 3;

  int image_width = 4;
  int image_height = 4;

  float input[] = {
    -1, -1, 2, 1, 3, 2, 4, -3, -4, -2, 2, -3, 1, -3, 4, -2,
  };

  float weights[] = {
    -4, 1,  0,  2,  3,  4,  4,  -4, -5, -3, 2,  2,  -4, -3, 3,  2,
    4,  -4, -3, -4, -4, 1,  -3, -5, -3, 4,  2,  -2, 2,  -1, -4, -1,
    -2, -3, 1,  1,  0,  -5, -1, 3,  3,  -5, -3, 0,  -3, 1,  -3, -1,
    1,  -3, -2, -2, 4,  -2, 0,  1,  2,  2,  -4, 2,  4,  0,  -5, -2,
    4,  4,  -5, 1,  0,  2,  -2, -5, -5, -3, -5, -5, 4,  -3, 0,  0,
    -4, -4, 0,  -5, -4, 0,  0,  -3, -5, -3, -1, 2,  -1, 4,  -1, 2,
  };

  float bias[] = {
    -4, -2, -3, -3, 3, 1, -2,
  };

  float expected[] = {
    530,  -762,  1469, 777,  849,   -771, -1698, 600,
    -658, -1821, 98,   -668, -1798, 30,   887,   -971,
  };

  CNN_CONFIG cnn_config = { .num_layers = 3,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            {
                                {
                                    .in_channels = 1,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 4,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_OUTPUT,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {
                                    .input_to_branches = 0x02,
                                    .channels_to_copy = 2,
                                    .branches_to_combine = 0x00,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = 4,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 2,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_CAT,
                                    .branch_config = {
                                    .input_to_branches = 0x00,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x02,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = 4,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 1,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = nullptr,
                                    .bias = nullptr,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {},
                                    .bn_params = {},
                                },
                            } };

  // Weights and biases need to be specified separately because
  // of the offset.
  AssignLayerWeightsBiases(&cnn_config, weights, bias);

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_INT_TOL);
}

TEST_F(CNNTest, TestOutputChannelsCount) {
  int filter_width = 1;
  int filter_height = 1;

  int image_width = 2;
  int image_height = 2;

  float input[] = { 0, 0, 0, 0 };

  float weights[] = { 0, 0, 0, 0, 0, 0, 0, 0 };

  float bias[] = { 0, 0, 0, 0, 0, 0 };

  float expected[] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  };

  CNN_CONFIG cnn_config = { .num_layers = 3,
                            .is_residue = 0,
                            .ext_width = 0,
                            .ext_height = 0,
                            .strict_bounds = 0,
                            {
                                {
                                    .in_channels = 1,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 2,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = weights,
                                    .bias = bias,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_INPUT,
                                    .branch_combine_type = BRANCH_NOC,
                                    .branch_config = {
                                    .input_to_branches = 0x06,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x00,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = 1,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 2,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = weights,
                                    .bias = bias,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 2,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_CAT,
                                    .branch_config = {
                                    .input_to_branches = 0x00,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x03,
                                  },
                                    .bn_params = {},
                                },
                                {
                                    .in_channels = 2,
                                    .filter_width = filter_width,
                                    .filter_height = filter_height,
                                    .out_channels = 2,
                                    .skip_width = 1,
                                    .skip_height = 1,
                                    .maxpool = 0,
                                    .weights = weights,
                                    .bias = bias,
                                    .pad = PADDING_SAME_ZERO,
                                    .activation = NONE,
                                    .deconvolve = 0,
                                    .branch = 0,
                                    .branch_copy_type = BRANCH_NO_COPY,
                                    .branch_combine_type = BRANCH_CAT,
                                    .branch_config = {
                                    .input_to_branches = 0x00,
                                    .channels_to_copy = 0,
                                    .branches_to_combine = 0x04,
                                  },
                                    .bn_params = {},
                                },
                            } };

  // Weights and biases need to be specified separately because
  // of the offset.
  AssignLayerWeightsBiases(&cnn_config, weights, bias);

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);
}

TEST_F(CNNTest, TestBatchNorm) {
  int image_width = 28;
  int image_height = 28;
  int filter_height = 7;
  int filter_width = 7;
  float input[] = {
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0117647f,  0.0705882f,  0.0705882f,  0.0705882f,
    0.494118f,  0.533333f,  0.686275f,   0.101961f,   0.65098f,    1.0f,
    0.968627f,  0.498039f,  0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.117647f,   0.141176f,   0.368627f,   0.603922f,
    0.666667f,  0.992157f,  0.992157f,   0.992157f,   0.992157f,   0.992157f,
    0.882353f,  0.67451f,   0.992157f,   0.94902f,    0.764706f,   0.25098f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.192157f,
    0.933333f,  0.992157f,  0.992157f,   0.992157f,   0.992157f,   0.992157f,
    0.992157f,  0.992157f,  0.992157f,   0.984314f,   0.364706f,   0.321569f,
    0.321569f,  0.219608f,  0.152941f,   0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0705882f,  0.858824f,   0.992157f,
    0.992157f,  0.992157f,  0.992157f,   0.992157f,   0.776471f,   0.713725f,
    0.968627f,  0.945098f,  0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.313725f,   0.611765f,   0.419608f,   0.992157f,
    0.992157f,  0.803922f,  0.0431373f,  0.0f,        0.168627f,   0.603922f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.054902f,  0.00392157f, 0.603922f,   0.992157f,   0.352941f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.545098f,  0.992157f,   0.745098f,   0.00784314f, 0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0431373f,
    0.745098f,  0.992157f,  0.27451f,    0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.137255f,   0.945098f,
    0.882353f,  0.627451f,  0.423529f,   0.00392157f, 0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.317647f,   0.941176f,   0.992157f,
    0.992157f,  0.466667f,  0.0980392f,  0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.176471f,   0.729412f,   0.992157f,   0.992157f,
    0.588235f,  0.105882f,  0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0627451f, 0.364706f,   0.988235f,   0.992157f,   0.733333f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.976471f,  0.992157f,   0.976471f,   0.25098f,    0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.180392f,   0.509804f,   0.717647f,   0.992157f,
    0.992157f,  0.811765f,  0.00784314f, 0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.152941f,   0.580392f,
    0.898039f,  0.992157f,  0.992157f,   0.992157f,   0.980392f,   0.713725f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0941176f, 0.447059f,  0.866667f,   0.992157f,   0.992157f,   0.992157f,
    0.992157f,  0.788235f,  0.305882f,   0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0901961f,  0.258824f,   0.835294f,   0.992157f,
    0.992157f,  0.992157f,  0.992157f,   0.776471f,   0.317647f,   0.00784314f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0705882f,  0.670588f,
    0.858824f,  0.992157f,  0.992157f,   0.992157f,   0.992157f,   0.764706f,
    0.313725f,  0.0352941f, 0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.215686f,  0.67451f,   0.886275f,   0.992157f,   0.992157f,   0.992157f,
    0.992157f,  0.956863f,  0.521569f,   0.0431373f,  0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.533333f,   0.992157f,
    0.992157f,  0.992157f,  0.831373f,   0.529412f,   0.517647f,   0.0627451f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f,        0.0f,        0.0f,
    0.0f,       0.0f,       0.0f,        0.0f
  };
  float expected[] = {
    -0.836424f, -0.857365f, -1.62739f,  -1.62739f,  -0.836424f, 5.40742f,
    0.920853f,  -0.692567f, -0.836424f, -0.534405f, -1.62739f,  -0.836424f,
    1.32602f,   1.36312f,   0.112766f,  -0.836424f, -0.192962f, 1.56975f,
    2.45777f,   0.944414f,  -0.192962f, -1.5519f,   -1.5519f,   -0.554006f,
    -0.192962f, 1.4231f,    -1.5519f,   -0.192962f, 1.3661f,    -1.5519f,
    -1.5519f,   -0.192962f, -0.843708f, -0.359025f, -0.843708f, -0.843708f,
    -0.843708f, 4.53065f,   0.0429584f, -0.796804f, -0.843708f, 0.3473f,
    -0.843708f, -0.843708f, -0.114439f, 3.14817f,   0.0811934f, -0.843708f
  };
  float kernel[] = {
    0.119643f,    -0.237864f,   0.0462892f,   0.0502297f,   -0.0134528f,
    0.146347f,    0.153133f,    0.0513307f,   0.0752369f,   0.0135557f,
    -0.111434f,   0.0941854f,   0.0788362f,   0.0299412f,   0.111762f,
    0.144066f,    0.00431504f,  -0.0177954f,  0.0738092f,   -0.0344215f,
    0.0832582f,   0.053989f,    -0.112691f,   0.0962145f,   0.0186525f,
    -0.00660205f, -0.111962f,   -0.126801f,   -0.231625f,   0.17309f,
    0.0748875f,   -0.179569f,   -0.00513812f, -0.156579f,   -0.147322f,
    0.184168f,    0.189308f,    -0.200359f,   -0.0156733f,  0.140649f,
    0.0858496f,   -0.0263217f,  -0.0740749f,  -0.112563f,   0.107528f,
    0.0609729f,   -0.221625f,   0.0769944f,   -0.00900815f, -0.00136441f,
    -0.0236521f,  -0.0418025f,  -0.00286299f, 0.12241f,     0.0964093f,
    -0.0150897f,  0.0532171f,   0.0625916f,   0.116939f,    0.118024f,
    0.161918f,    -0.00909767f, 0.100897f,    -0.054563f,   -0.175179f,
    -0.0687892f,  0.00734235f,  0.109833f,    -0.113776f,   0.0595405f,
    -0.170255f,   0.0124815f,   -0.0363301f,  -0.0127038f,  0.0445554f,
    -0.0729894f,  0.107428f,    -0.0341417f,  0.132619f,    0.00984557f,
    -0.00443654f, 0.202929f,    0.0945134f,   0.0148725f,   0.00998574f,
    -0.0226449f,  0.0478197f,   -0.0793442f,  0.0707599f,   -0.084225f,
    0.0865795f,   0.071104f,    -0.047894f,   0.0838322f,   0.0635493f,
    -0.00370265f, -0.157247f,   -0.0289622f,  -0.0590963f,  0.13207f,
    0.00468011f,  -0.0345372f,  0.217939f,    0.18861f,     -0.0290393f,
    -0.0440664f,  0.0126197f,   -0.129132f,   -0.124943f,   0.0968156f,
    -0.0853643f,  -0.182305f,   0.00461618f,  -0.147095f,   -0.230282f,
    0.00856019f,  0.0278893f,   -0.0300229f,  0.0417871f,   0.0804717f,
    -0.0768571f,  -0.0397085f,  -0.0601096f,  0.100901f,    -0.0184926f,
    0.0350673f,   0.0971094f,   -0.0171837f,  -0.289644f,   -0.0899041f,
    0.08998f,     -0.160319f,   -0.0195103f,  0.0392167f,   -0.137864f,
    -0.0136294f,  0.0330886f,   -0.0409244f,  -0.092533f,   -0.0427934f,
    -0.191144f,   -0.0969461f,  0.112035f,    0.138611f,    0.128717f,
    0.191184f,    0.197462f
  };
  float bias[] = { 0.186703f, 0.204358f, -0.0230452f };

  float bn_gamma[] = { 1.32173f, 1.26171f, 1.21966f };
  float bn_beta[] = { -0.232595f, -0.222652f, -0.232209f };
  float bn_mean[] = { 0.329233f, 0.199894f, 0.12389f };
  float bn_std[] = { 0.311986f, 0.189737f, 0.247104f };

  CNN_BATCHNORM_PARAMS bn_params = {
    .bn_gamma = bn_gamma,
    .bn_beta = bn_beta,
    .bn_mean = bn_mean,
    .bn_std = bn_std,
  };

  CNN_CONFIG cnn_config = {
    .num_layers = 1,
    .is_residue = 0,
    .ext_width = 0,
    .ext_height = 0,
    .strict_bounds = 0,
    {
        {
            .in_channels = 1,
            .filter_width = filter_width,
            .filter_height = filter_height,
            .out_channels = 3,
            .skip_width = 7,
            .skip_height = 7,
            .maxpool = 0,
            .weights = kernel,
            .bias = bias,
            .pad = PADDING_VALID,
            .activation = RELU,
            .deconvolve = 0,
            .branch = 0,
            .branch_copy_type = BRANCH_NO_COPY,
            .branch_combine_type = BRANCH_NOC,
            .branch_config = {},
            .bn_params = bn_params,
        },
    },
  };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);
}

TEST_F(CNNTest, TestMultithreading) {
  int image_height = 2;
  int image_width = 2;
  int filter_height = 3;
  int filter_width = 3;

  float input[] = {
    -2,
    4,
    1,
    0,
  };

  float weights[] = {
    -4, 2, -2, 0,  -4, 4, -3, -3, -3, -1, 1,  0,  -5, -3, 0, -5, 0, 0,
    -1, 0, 2,  -5, 0,  1, 4,  2,  1,  0,  -2, -1, -5, -3, 2, -2, 1, -5,
  };

  float bias[] = {
    -4,
    -3,
    -2,
    3,
  };

  float expected[] = {
    2, 10, -8, -17, -24, 5, -15, 6, -5, -5, 7, -10, 4, 13, 9, -14,
  };

  CNN_CONFIG cnn_config = {
    .num_layers = 1,
    .is_residue = 0,
    .ext_width = 0,
    .ext_height = 0,
    .strict_bounds = 0,
    {
        {
            .in_channels = 1,
            .filter_width = filter_width,
            .filter_height = filter_height,
            .out_channels = 4,
            .skip_width = 1,
            .skip_height = 1,
            .maxpool = 0,
            .weights = weights,
            .bias = bias,
            .pad = PADDING_SAME_ZERO,
            .activation = NONE,
            .deconvolve = 0,
            .branch = 0,
            .branch_copy_type = BRANCH_NO_COPY,
            .branch_combine_type = BRANCH_NOC,
            .branch_config = {},
            .bn_params = {},
        },
    },
  };

  CNN_THREAD_DATA thread_data = { 1, NULL };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);

  const AVxWorkerInterface *const winterface = aom_get_worker_interface();
  AVxWorker workers[4];

  for (int i = 0; i < 4; ++i) {
    winterface->init(&workers[i]);
  }

  thread_data = { 4, workers };

  RunCNNTest(image_width, image_height, input, expected, &cnn_config,
             image_width, &thread_data, MSE_FLOAT_TOL);

  for (int i = 0; i < 4; ++i) {
    winterface->end(&workers[i]);
  }
}
