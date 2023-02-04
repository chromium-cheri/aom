/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "av1/encoder/encoder.h"
#include "av1/encoder/encoder_utils.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/rdopt.h"
#include "av1/encoder/saliency_map.h"

// This funtion is to extract red/green/blue channels, and calculate intensity =
// (r+g+b)/3.
void av1_get_color_intensity(YV12_BUFFER_CONFIG *src, double *Cr, double *Cg,
                             double *Cb, double *Intensity) {
  const uint8_t *y = src->buffers[0];
  const uint8_t *u = src->buffers[1];
  const uint8_t *v = src->buffers[2];

  const int y_height = src->crop_heights[0];
  const int y_width = src->crop_widths[0];
  const int y_stride = src->strides[0];
  const int c_stride = src->strides[1];

  for (int i = 0; i < y_height; i++) {
    for (int j = 0; j < y_width; j++) {
      Cr[i * y_width + j] =
          fclamp((double)y[i * y_stride + j] +
                     1.370 * (double)(v[(i / 2) * c_stride + (j / 2)] - 128),
                 0, 255);
      Cg[i * y_width + j] =
          fclamp((double)y[i * y_stride + j] -
                     0.698 * (double)(u[(i / 2) * c_stride + (j / 2)] - 128) -
                     0.337 * (double)(v[(i / 2) * c_stride + (j / 2)] - 128),
                 0, 255);
      Cb[i * y_width + j] =
          fclamp((double)y[i * y_stride + j] +
                     1.732 * (double)(u[(i / 2) * c_stride + (j / 2)] - 128),
                 0, 255);

      assert(Cr[i * y_width + j] >= 0 && Cr[i * y_width + j] <= 255);
      assert(Cg[i * y_width + j] >= 0 && Cg[i * y_width + j] <= 255);
      assert(Cb[i * y_width + j] >= 0 && Cb[i * y_width + j] <= 255);

      Intensity[i * y_width + j] =
          (double)(Cr[i * y_width + j] + Cg[i * y_width + j] +
                   Cb[i * y_width + j]) /
          3;
      assert(Intensity[i * y_width + j] >= 0 &&
             Intensity[i * y_width + j] <= 255);

      Intensity[i * y_width + j] /= 256;
      Cr[i * y_width + j] /= 256;
      Cg[i * y_width + j] /= 256;
      Cb[i * y_width + j] /= 256;
    }
  }
}

double convolve_map(const double *filter, const double *img, const int size) {
  double result = 0;
  for (int i = 0; i < size; i++) {
    result += filter[i] * img[i];
  }
  return result;
}

// This function is to decimate the map by half, and apply Guassian filter on
// top of the reduced map.
void decimate_map(double *map, int height, int width, int stride,
                  double *reduced_map) {
  const int new_width = width / 2;
  const int window_size = 5;
  const double gaussian_filter[25] = {
    1. / 256, 1.0 / 64, 3. / 128, 1. / 64,  1. / 256, 1. / 64, 1. / 16,
    3. / 32,  1. / 16,  1. / 64,  3. / 128, 3. / 32,  9. / 64, 3. / 32,
    3. / 128, 1. / 64,  1. / 16,  3. / 32,  1. / 16,  1. / 64, 1. / 256,
    1. / 64,  3. / 128, 1. / 64,  1. / 256
  };

  double map_region[25];
  for (int y = 0; y < height - 1; y += 2) {
    for (int x = 0; x < width - 1; x += 2) {
      int i = 0;
      for (int yy = y - window_size / 2; yy <= y + window_size / 2; yy++) {
        for (int xx = x - window_size / 2; xx <= x + window_size / 2; xx++) {
          int yvalue = yy;
          int xvalue = xx;
          // copied values outside the boundary
          if (yvalue < 0) yvalue = 0;
          if (xvalue < 0) xvalue = 0;
          if (yvalue >= height) yvalue = height - 1;
          if (xvalue >= width) xvalue = width - 1;
          map_region[i++] = map[yvalue * stride + xvalue];
        }
      }
      reduced_map[(y / 2) * new_width + (x / 2)] = (double)convolve_map(
          gaussian_filter, map_region, window_size * window_size);
    }
  }
}

// This function is to upscale the map from in_level size to out_level size.
// Note that the map at "level-1" will upscale the map at "level" by x2.
void upscale_map(double *input, int in_level, double *output, int out_level,
                 int height[9], int width[9]) {
  for (int level = in_level; level > out_level; level--) {
    const int cur_width = width[level];
    const int cur_height = height[level];
    const int cur_stride = width[level];

    double *original =
        (double *)malloc(cur_width * cur_height * sizeof(double));

    if (level == in_level) {
      memcpy(original, input, cur_width * cur_height * sizeof(double));
    } else {
      memcpy(original, output, cur_width * cur_height * sizeof(double));
    }

    if (level > 0) {
      int h_upscale = height[level - 1];
      int w_upscale = width[level - 1];
      int s_upscale = width[level - 1];

      double *upscale =
          (double *)malloc(h_upscale * w_upscale * sizeof(double));

      int ii = 0;
      int jj = 0;

      for (int i = 0; i < h_upscale; ++i) {
        for (int j = 0; j < w_upscale; ++j) {
          ii = i / 2;
          jj = j / 2;

          if (jj >= cur_width) {
            jj = cur_width - 1;
          }
          if (ii >= cur_height) {
            ii = cur_height - 1;
          }

          upscale[j + i * s_upscale] = (double)original[jj + ii * cur_stride];
        }
      }
      memcpy(output, upscale, h_upscale * w_upscale * sizeof(double));
      free(upscale);
    }

    free(original);
  }
}

// This function is calculate the differences between a fine scale c and a
// coaser scale s yield the feature maps. c \in {2, 3, 4}, and s = c + delta,
// where delta \in {3, 4}.
void Center_surround_diff(double *input[9], saliency_feature_map *output[6],
                          int height[9], int width[9]) {
  int j = 0;
  for (int k = 2; k < 5; k++) {
    int cur_height = height[k];
    int cur_width = width[k];

    double *intermediate_map =
        (double *)malloc(cur_height * cur_width * sizeof(double));

    output[j]->buf = (double *)malloc(cur_height * cur_width * sizeof(double));
    output[j + 1]->buf =
        (double *)malloc(cur_height * cur_width * sizeof(double));

    output[j]->height = output[j + 1]->height = cur_height;
    output[j]->width = output[j + 1]->width = cur_width;

    upscale_map(input[k + 3], k + 3, intermediate_map, k, height, width);

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j]->buf[r * cur_width + c] =
            fabs((double)(input[k][r * cur_width + c] -
                          intermediate_map[r * cur_width + c]));
      }
    }

    upscale_map(input[k + 4], k + 4, intermediate_map, k, height, width);

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j + 1]->buf[r * cur_width + c] = fabs(
            input[k][r * cur_width + c] - intermediate_map[r * cur_width + c]);
      }
    }

    free(intermediate_map);
    j += 2;
  }
}

// For color channels, the differences is calculated based on "color
// double-opponency". For example, the RG feature map is constructed between a
// fine scale c of R-G component and a coaser scale s of G-R component.
void Center_surround_diff_RGB(double *input_1[9], double *input_2[9],
                              saliency_feature_map *output[6], int height[9],
                              int width[9]) {
  int j = 0;
  for (int k = 2; k < 5; k++) {
    int cur_height = height[k];
    int cur_width = width[k];

    double *intermediate_map =
        (double *)malloc(cur_height * cur_width * sizeof(double));

    output[j]->buf = (double *)malloc(cur_height * cur_width * sizeof(double));
    output[j + 1]->buf =
        (double *)malloc(cur_height * cur_width * sizeof(double));

    output[j]->height = output[j + 1]->height = cur_height;
    output[j]->width = output[j + 1]->width = cur_width;

    upscale_map(input_2[k + 3], k + 3, intermediate_map, k, height, width);

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j]->buf[r * cur_width + c] =
            fabs((double)(input_1[k][r * cur_width + c] -
                          intermediate_map[r * cur_width + c]));
      }
    }

    upscale_map(input_2[k + 4], k + 4, intermediate_map, k, height, width);

    for (int r = 0; r < cur_height; r++) {
      for (int c = 0; c < cur_width; c++) {
        output[j + 1]->buf[r * cur_width + c] =
            fabs(input_1[k][r * cur_width + c] -
                 intermediate_map[r * cur_width + c]);
      }
    }

    free(intermediate_map);
    j += 2;
  }
}

// This function is to generate Gaussian pyramid images with indexes from 0 to
// 8, and construct the feature maps from calculating the center-surround
// differences.
void Gaussian_Pyramid(saliency_feature_map *dst[6], double *src, int width,
                      int height) {
  double *GaussianMap[9];  // scale = 9
  int pyr_width[9];
  int pyr_height[9];

  GaussianMap[0] = (double *)malloc(width * height * sizeof(double));
  memcpy(GaussianMap[0], src, width * height * sizeof(double));

  pyr_width[0] = width;
  pyr_height[0] = height;

  for (int i = 1; i < 9; i++) {
    int stride = pyr_width[i - 1];
    int new_width = pyr_width[i - 1] / 2;
    int new_height = pyr_height[i - 1] / 2;

    GaussianMap[i] = (double *)malloc(new_width * new_height * sizeof(double));
    decimate_map(GaussianMap[i - 1], pyr_height[i - 1], pyr_width[i - 1],
                 stride, GaussianMap[i]);

    pyr_width[i] = new_width;
    pyr_height[i] = new_height;
  }

  Center_surround_diff(GaussianMap, dst, pyr_height, pyr_width);

  for (int i = 0; i < 9; i++) {
    if (GaussianMap[i]) {
      free(GaussianMap[i]);
    }
  }
}

void Gaussian_Pyramid_RGB(saliency_feature_map *dst[6], double *src_1,
                          double *src_2, int width, int height) {
  double *GaussianMap[2][9];  // scale = 9
  int pyr_width[9];
  int pyr_height[9];
  double *src[2];

  src[0] = src_1;
  src[1] = src_2;

  for (int k = 0; k < 2; k++) {
    GaussianMap[k][0] = (double *)malloc(width * height * sizeof(double));
    memcpy(GaussianMap[k][0], src[k], width * height * sizeof(double));

    pyr_width[0] = width;
    pyr_height[0] = height;

    for (int i = 1; i < 9; i++) {
      int stride = pyr_width[i - 1];
      int new_width = pyr_width[i - 1] / 2;
      int new_height = pyr_height[i - 1] / 2;

      GaussianMap[k][i] =
          (double *)malloc(new_width * new_height * sizeof(double));
      decimate_map(GaussianMap[k][i - 1], pyr_height[i - 1], pyr_width[i - 1],
                   stride, GaussianMap[k][i]);

      pyr_width[i] = new_width;
      pyr_height[i] = new_height;
    }
  }

  Center_surround_diff_RGB(GaussianMap[0], GaussianMap[1], dst, pyr_height,
                           pyr_width);

  for (int i = 0; i < 9; i++) {
    if (GaussianMap[0][i]) {
      free(GaussianMap[0][i]);
    }
    if (GaussianMap[1][i]) {
      free(GaussianMap[1][i]);
    }
  }
}

void Get_Feature_Map_Intensity(saliency_feature_map *I_map[6],
                               double *Intensity, int width, int height) {
  Gaussian_Pyramid(I_map, Intensity, width, height);
}

void Get_Feature_Map_RGB(saliency_feature_map *RG_map[6],
                         saliency_feature_map *BY_map[6], double *Cr,
                         double *Cg, double *Cb, int width, int height) {
  double *R, *G, *B, *Y, *RGMat, *BYMat, *GRMat, *YBMat;

  R = (double *)malloc(height * width * sizeof(double));
  G = (double *)malloc(height * width * sizeof(double));
  B = (double *)malloc(height * width * sizeof(double));
  Y = (double *)malloc(height * width * sizeof(double));
  RGMat = (double *)malloc(height * width * sizeof(double));
  BYMat = (double *)malloc(height * width * sizeof(double));
  GRMat = (double *)malloc(height * width * sizeof(double));
  YBMat = (double *)malloc(height * width * sizeof(double));

  for (int i = 0; i < height; i++) {
    for (int j = 0; j < width; j++) {
      R[i * width + j] =
          Cr[i * width + j] - (Cg[i * width + j] + Cb[i * width + j]) / 2;
      G[i * width + j] =
          Cg[i * width + j] - (Cr[i * width + j] + Cb[i * width + j]) / 2;
      B[i * width + j] =
          Cb[i * width + j] - (Cr[i * width + j] + Cg[i * width + j]) / 2;
      Y[i * width + j] = (Cr[i * width + j] + Cg[i * width + j]) / 2 -
                         fabs(Cr[i * width + j] - Cg[i * width + j]) / 2 -
                         Cb[i * width + j];

      R[i * width + j] = AOMMAX(0, R[i * width + j]);
      G[i * width + j] = AOMMAX(0, G[i * width + j]);
      B[i * width + j] = AOMMAX(0, B[i * width + j]);
      Y[i * width + j] = AOMMAX(0, Y[i * width + j]);

      RGMat[i * width + j] = R[i * width + j] - G[i * width + j];
      BYMat[i * width + j] = B[i * width + j] - Y[i * width + j];
      GRMat[i * width + j] = G[i * width + j] - R[i * width + j];
      YBMat[i * width + j] = Y[i * width + j] - B[i * width + j];
    }
  }

  Gaussian_Pyramid_RGB(RG_map, RGMat, GRMat, width, height);
  Gaussian_Pyramid_RGB(BY_map, BYMat, YBMat, width, height);

  free(R);
  free(G);
  free(B);
  free(Y);
  free(RGMat);
  free(BYMat);
  free(GRMat);
  free(YBMat);
}

void Filter2D_9x9(double *input, double *output, const double kernel[9][9],
                  int width, int height) {
  const int window_size = 9;
  double img_section[81];
  for (int y = 0; y <= height - 1; y++) {
    for (int x = 0; x <= width - 1; x++) {
      int i = 0;
      for (int yy = y - window_size / 2; yy <= y + window_size / 2; yy++) {
        for (int xx = x - window_size / 2; xx <= x + window_size / 2; xx++) {
          int yvalue = yy;
          int xvalue = xx;
          // copied pixels outside the boundary
          if (yvalue < 0) yvalue = 0;
          if (xvalue < 0) xvalue = 0;
          if (yvalue >= height) yvalue = height - 1;
          if (xvalue >= width) xvalue = width - 1;
          img_section[i++] = input[yvalue * width + xvalue];
        }
      }

      output[y * width + x] = 0;
      for (int k = 0; k < window_size; k++) {
        for (int l = 0; l < window_size; l++) {
          output[y * width + x] +=
              kernel[k][l] * img_section[k * window_size + l];
        }
      }
    }
  }
}

void Get_Feature_Map_Orientation(saliency_feature_map *dst[24],
                                 double *Intensity, int width, int height) {
  double *GaussianMap[9];
  int pyr_width[9];
  int pyr_height[9];

  GaussianMap[0] = (double *)malloc(width * height * sizeof(double));
  memcpy(GaussianMap[0], Intensity, width * height * sizeof(double));

  pyr_width[0] = width;
  pyr_height[0] = height;

  for (int i = 1; i < 9; i++) {
    int stride = pyr_width[i - 1];
    int new_width = pyr_width[i - 1] / 2;
    int new_height = pyr_height[i - 1] / 2;

    GaussianMap[i] = (double *)malloc(new_width * new_height * sizeof(double));
    decimate_map(GaussianMap[i - 1], pyr_height[i - 1], pyr_width[i - 1],
                 stride, GaussianMap[i]);

    pyr_width[i] = new_width;
    pyr_height[i] = new_height;
  }

  double *tempGaborOutput0[9], *tempGaborOutput45[9], *tempGaborOutput90[9],
      *tempGaborOutput135[9];

  for (int i = 2; i < 9; i++) {
    const int cur_height = pyr_height[i];
    const int cur_width = pyr_width[i];
    tempGaborOutput0[i] =
        (double *)malloc(cur_height * cur_width * sizeof(double));
    tempGaborOutput45[i] =
        (double *)malloc(cur_height * cur_width * sizeof(double));
    tempGaborOutput90[i] =
        (double *)malloc(cur_height * cur_width * sizeof(double));
    tempGaborOutput135[i] =
        (double *)malloc(cur_height * cur_width * sizeof(double));
    Filter2D_9x9(GaussianMap[i], tempGaborOutput0[i], GaborFilter0, cur_width,
                 cur_height);
    Filter2D_9x9(GaussianMap[i], tempGaborOutput45[i], GaborFilter45, cur_width,
                 cur_height);
    Filter2D_9x9(GaussianMap[i], tempGaborOutput90[i], GaborFilter90, cur_width,
                 cur_height);
    Filter2D_9x9(GaussianMap[i], tempGaborOutput135[i], GaborFilter135,
                 cur_width, cur_height);
  }

  for (int i = 0; i < 9; i++) {
    if (GaussianMap[i]) {
      free(GaussianMap[i]);
    }
  }

  saliency_feature_map *tmp0[6], *tmp45[6], *tmp90[6], *tmp135[6];

  for (int i = 0; i < 6; i++) {
    tmp0[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
    tmp45[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
    tmp90[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
    tmp135[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  }

  Center_surround_diff(tempGaborOutput0, tmp0, pyr_height, pyr_width);
  Center_surround_diff(tempGaborOutput45, tmp45, pyr_height, pyr_width);
  Center_surround_diff(tempGaborOutput90, tmp90, pyr_height, pyr_width);
  Center_surround_diff(tempGaborOutput135, tmp135, pyr_height, pyr_width);

  for (int i = 2; i < 9; i++) {
    free(tempGaborOutput0[i]);
    free(tempGaborOutput45[i]);
    free(tempGaborOutput90[i]);
    free(tempGaborOutput135[i]);
  }

  for (int i = 0; i < 6; i++) {
    dst[i] = tmp0[i];
    dst[i + 6] = tmp45[i];
    dst[i + 12] = tmp90[i];
    dst[i + 18] = tmp135[i];
  }
}

void FindMinMax(saliency_feature_map *input, double *maxx, double *minn) {
  *minn = *maxx = input->buf[0];

  for (int i = 0; i < input->height; i++) {
    for (int j = 0; j < input->width; j++) {
      *minn = fmin(input->buf[i * input->width + j], *minn);
      *maxx = fmax(input->buf[i * input->width + j], *maxx);
    }
  }
}

double AverageLocalMax(saliency_feature_map *input, int stepsize) {
  int numlocal = 0;
  double lmaxmean = 0, lmax = 0, dummy = 0;
  saliency_feature_map localMap;
  localMap.height = stepsize;
  localMap.width = stepsize;
  localMap.buf = (double *)malloc(stepsize * stepsize * sizeof(double));

  for (int y = 0; y < input->height - stepsize; y += stepsize) {
    for (int x = 0; x < input->width - stepsize; x += stepsize) {
      for (int i = 0; i < stepsize; i++) {
        for (int j = 0; j < stepsize; j++) {
          localMap.buf[i * stepsize + j] =
              input->buf[(y + i) * input->width + x + j];
        }
      }

      FindMinMax(&localMap, &dummy, &lmax);
      lmaxmean += lmax;
      numlocal++;
    }
  }

  free(localMap.buf);

  return lmaxmean / numlocal;
}

saliency_feature_map *MinMaxNormalize(saliency_feature_map *input) {
  double maxx, minn;

  FindMinMax(input, &maxx, &minn);

  saliency_feature_map *result =
      (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  result->buf = (double *)malloc(input->width * input->height * sizeof(double));
  result->height = input->height;
  result->width = input->width;

  for (int i = 0; i < input->height; i++) {
    for (int j = 0; j < input->width; j++) {
      if (maxx != minn) {
        result->buf[i * input->width + j] =
            input->buf[i * input->width + j] / (maxx - minn) +
            minn / (minn - maxx);
      } else {
        result->buf[i * input->width + j] =
            input->buf[i * input->width + j] - minn;
      }
    }
  }

  return result;
}

saliency_feature_map *Nomalization_Operator(saliency_feature_map *input,
                                            int stepsize) {
  saliency_feature_map *result =
      (saliency_feature_map *)malloc(sizeof(saliency_feature_map));

  result->buf = (double *)malloc(input->width * input->height * sizeof(double));
  result->height = input->height;
  result->width = input->width;

  saliency_feature_map *tempResult = MinMaxNormalize(input);

  double lmaxmean = AverageLocalMax(tempResult, stepsize);
  double normCoeff = (1 - lmaxmean) * (1 - lmaxmean);

  for (int i = 0; i < input->height; i++) {
    for (int j = 0; j < input->width; j++) {
      result->buf[i * input->width + j] =
          tempResult->buf[i * input->width + j] * normCoeff;
    }
  }

  free(tempResult->buf);
  free(tempResult);

  return result;
}

void normalizeFM(saliency_feature_map *input[6],
                 saliency_feature_map *output[6], int width, int height,
                 int num_FM) {
  int pyr_height[9];
  int pyr_width[9];

  pyr_height[0] = height;
  pyr_width[0] = width;

  for (int i = 1; i < 9; i++) {
    int new_width = pyr_width[i - 1] / 2;
    int new_height = pyr_height[i - 1] / 2;

    pyr_width[i] = new_width;
    pyr_height[i] = new_height;
  }

  double *tmp = (double *)malloc(width * height * sizeof(double));
  for (int i = 0; i < num_FM; i++) {
    saliency_feature_map *normalizedmatrix = Nomalization_Operator(input[i], 8);
    output[i]->buf = (double *)malloc(width * height * sizeof(double));
    output[i]->height = height;
    output[i]->width = width;

    upscale_map(normalizedmatrix->buf, (i / 2) + 2, tmp, 0, pyr_height,
                pyr_width);
    memcpy(output[i]->buf, (double *)tmp, width * height * sizeof(double));
    free(normalizedmatrix->buf);
  }
  free(tmp);
}

saliency_feature_map *NormalizedMap(saliency_feature_map *input[6], int width,
                                    int height) {
  int num_FM = 6;

  saliency_feature_map *Ninput[6];
  for (int i = 0; i < 6; i++) {
    Ninput[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  }
  normalizeFM(input, Ninput, width, height, num_FM);

  saliency_feature_map *output =
      (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  output->buf = (double *)malloc(width * height * sizeof(double));
  output->height = height;
  output->width = width;
  memset(output->buf, 0, width * height * sizeof(double));

  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      for (int i = 0; i < num_FM; i++) {
        output->buf[r * width + c] += Ninput[i]->buf[r * width + c];
      }
    }
  }

  for (int i = 0; i < num_FM; i++) {
    free(Ninput[i]->buf);
    free(Ninput[i]);
  }

  return Nomalization_Operator(output, 8);
}

saliency_feature_map *NormalizedMap_RGB(saliency_feature_map *RG_map[6],
                                        saliency_feature_map *BY_map[6],
                                        int width, int height) {
  saliency_feature_map *CCM_RG = NormalizedMap(RG_map, width, height);
  saliency_feature_map *CCM_BY = NormalizedMap(BY_map, width, height);

  saliency_feature_map *CCM =
      (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  CCM->buf = (double *)malloc(width * height * sizeof(double));
  CCM->height = height;
  CCM->width = width;

  for (int r = 0; r < height; r++) {
    for (int c = 0; c < width; c++) {
      CCM->buf[r * width + c] =
          CCM_RG->buf[r * width + c] + CCM_BY->buf[r * width + c];
    }
  }

  free(CCM_RG->buf);
  free(CCM_BY->buf);
  free(CCM_RG);
  free(CCM_BY);

  return Nomalization_Operator(CCM, 8);
}

saliency_feature_map *NormalizedMap_Orientation(
    saliency_feature_map *Orientation_map[24], int width, int height) {
  int num_FMs_perAngle = 6;

  saliency_feature_map *OFM0[6];
  saliency_feature_map *OFM45[6];
  saliency_feature_map *OFM90[6];
  saliency_feature_map *OFM135[6];

  for (int i = 0; i < num_FMs_perAngle; i++) {
    OFM0[i] = Orientation_map[0 * num_FMs_perAngle + i];
    OFM45[i] = Orientation_map[1 * num_FMs_perAngle + i];
    OFM90[i] = Orientation_map[2 * num_FMs_perAngle + i];
    OFM135[i] = Orientation_map[3 * num_FMs_perAngle + i];
  }

  // extract conspicuity map for each angle
  saliency_feature_map *NOFM[4];
  NOFM[0] = NormalizedMap(OFM0, width, height);
  NOFM[1] = NormalizedMap(OFM45, width, height);
  NOFM[2] = NormalizedMap(OFM90, width, height);
  NOFM[3] = NormalizedMap(OFM135, width, height);

  for (int i = 0; i < 4; i++) {
    NOFM[i]->height = height;
    NOFM[i]->width = width;
  }

  saliency_feature_map *OCM =
      (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  OCM->buf = (double *)malloc(width * height * sizeof(double));
  OCM->height = height;
  OCM->width = width;
  memset(OCM->buf, 0, width * height * sizeof(double));

  for (int i = 0; i < 4; i++) {
    for (int r = 0; r < height; r++) {
      for (int c = 0; c < width; c++) {
        OCM->buf[r * width + c] += NOFM[i]->buf[r * width + c];
      }
    }
    free(NOFM[i]->buf);
    free(NOFM[i]);
  }

  return Nomalization_Operator(OCM, 8);
}

// Set pixel level saliency mask based on Itti-Koch algorithm
void set_saliency_map(AV1_COMP *cpi) {
  AV1_COMMON *const cm = &cpi->common;

  int frm_width = cm->width;
  int frm_height = cm->height;

  double *Cr, *Cg, *Cb, *Intensity;
  Cr = (double *)malloc(frm_width * frm_height * sizeof(double));
  Cg = (double *)malloc(frm_width * frm_height * sizeof(double));
  Cb = (double *)malloc(frm_width * frm_height * sizeof(double));
  Intensity = (double *)malloc(frm_width * frm_height * sizeof(double));

  // Extract red / green / blue channels and Intensity component
  av1_get_color_intensity(cpi->source, Cr, Cg, Cb, Intensity);

  // Feature Map Extraction
  // Intensity map
  saliency_feature_map *I_map[6];
  for (int i = 0; i < 6; i++) {
    I_map[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  }

  Get_Feature_Map_Intensity(I_map, Intensity, frm_width, frm_height);

  // RGB map
  saliency_feature_map *RG_map[6], *BY_map[6];
  for (int i = 0; i < 6; i++) {
    RG_map[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
    BY_map[i] = (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  }
  Get_Feature_Map_RGB(RG_map, BY_map, Cr, Cg, Cb, frm_width, frm_height);

  // Orientation map
  saliency_feature_map *Orientation_map[24];
  for (int i = 0; i < 24; i++) {
    Orientation_map[i] =
        (saliency_feature_map *)malloc(sizeof(saliency_feature_map));
  }
  Get_Feature_Map_Orientation(Orientation_map, Intensity, frm_width,
                              frm_height);

  free(Cr);
  free(Cg);
  free(Cb);
  free(Intensity);

  // Consticuity map generation
  saliency_feature_map *INM = NormalizedMap(I_map, frm_width, frm_height);
  saliency_feature_map *CNM =
      NormalizedMap_RGB(RG_map, BY_map, frm_width, frm_height);
  saliency_feature_map *ONM =
      NormalizedMap_Orientation(Orientation_map, frm_width, frm_height);

  for (int i = 0; i < 6; i++) {
    free(I_map[i]->buf);
    free(RG_map[i]->buf);
    free(BY_map[i]->buf);
    free(I_map[i]);
    free(RG_map[i]);
    free(BY_map[i]);
  }

  for (int i = 0; i < 24; i++) {
    free(Orientation_map[i]->buf);
    free(Orientation_map[i]);
  }

  // Pixel level saliency map
  saliency_feature_map *Saliency_Map =
      (saliency_feature_map *)malloc(sizeof(saliency_feature_map));

  Saliency_Map->buf = (double *)malloc(frm_width * frm_height * sizeof(double));
  Saliency_Map->height = frm_height;
  Saliency_Map->width = frm_width;

  double w_intensity, w_color, w_orient;

  w_intensity = w_color = w_orient = (double)1 / 3;

  for (int r = 0; r < frm_height; r++) {
    for (int c = 0; c < frm_width; c++) {
      Saliency_Map->buf[r * frm_width + c] =
          (w_intensity * INM->buf[r * frm_width + c] +
           w_color * CNM->buf[r * frm_width + c] +
           w_orient * ONM->buf[r * frm_width + c]);
    }
  }

  memcpy(cpi->saliency_map, Saliency_Map->buf,
         frm_width * frm_height * sizeof(double));

  free(INM->buf);
  free(INM);

  free(CNM->buf);
  free(CNM);

  free(ONM->buf);
  free(ONM);

  free(Saliency_Map->buf);
  free(Saliency_Map);
}
