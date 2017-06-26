/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#define _POSIX_C_SOURCE 200112L  // rand_r()
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

#include "av1/encoder/bgsprite.h"

#include "aom_mem/aom_mem.h"
#include "av1/common/mv.h"
#include "av1/common/warped_motion.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/global_motion.h"
#include "av1/encoder/mathutils.h"

#define TRANSFORM_MAT_DIM 3

typedef struct {
#if CONFIG_HIGHBITDEPTH
  uint16_t y;
  uint16_t u;
  uint16_t v;
#else
  uint8_t y;
  uint8_t u;
  uint8_t v;
#endif  // CONFIG_HIGHBITDEPTH
} YuvPixel;

// Maps to convert from matrix form to param vector form.
static const int params_to_matrix_map[] = { 2, 3, 0, 4, 5, 1, 6, 7 };
static const int matrix_to_params_map[] = { 2, 5, 0, 1, 3, 4, 6, 7 };

// Convert the parameter array to a 3x3 matrix form.
static void params_to_matrix(const double *const params, double *target) {
  for (int i = 0; i < MAX_PARAMDIM - 1; i++) {
    assert(params_to_matrix_map[i] < MAX_PARAMDIM - 1);
    target[i] = params[params_to_matrix_map[i]];
  }
  target[8] = 1;
}

// Convert a 3x3 matrix to a parameter array form.
static void matrix_to_params(const double *const matrix, double *target) {
  for (int i = 0; i < MAX_PARAMDIM - 1; i++) {
    assert(matrix_to_params_map[i] < MAX_PARAMDIM - 1);
    target[i] = matrix[matrix_to_params_map[i]];
  }
}

// Do matrix multiplication on params.
static void multiply_params(double *const m1, double *const m2,
                            double *target) {
  double m1_matrix[MAX_PARAMDIM];
  double m2_matrix[MAX_PARAMDIM];
  double result[MAX_PARAMDIM];

  params_to_matrix(m1, m1_matrix);
  params_to_matrix(m2, m2_matrix);
  multiply_mat(m2_matrix, m1_matrix, result, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, TRANSFORM_MAT_DIM);
  matrix_to_params(result, target);
}

// Finds x and y limits of a single transformed image.
// Width and height are the size of the input video.
static void find_frame_limit(int width, int height,
                             const double *const transform, int *x_min,
                             int *x_max, int *y_min, int *y_max) {
  double transform_matrix[MAX_PARAMDIM];
  double xy_matrix[3] = { 0, 0, 1 };
  double uv_matrix[3] = { 0 };
// Macro used to update frame limits based on transformed coordinates.
#define UPDATELIMITS(u, v, x_min, x_max, y_min, y_max) \
  {                                                    \
    if ((int)ceil(u) > *x_max) {                       \
      *x_max = (int)ceil(u);                           \
    }                                                  \
    if ((int)floor(u) < *x_min) {                      \
      *x_min = (int)floor(u);                          \
    }                                                  \
    if ((int)ceil(v) > *y_max) {                       \
      *y_max = (int)ceil(v);                           \
    }                                                  \
    if ((int)floor(v) < *y_min) {                      \
      *y_min = (int)floor(v);                          \
    }                                                  \
  }

  params_to_matrix(transform, transform_matrix);
  xy_matrix[0] = 0;
  xy_matrix[1] = 0;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  *x_max = (int)ceil(uv_matrix[0]);
  *x_min = (int)floor(uv_matrix[0]);
  *y_max = (int)ceil(uv_matrix[1]);
  *y_min = (int)floor(uv_matrix[1]);

  xy_matrix[0] = width;
  xy_matrix[1] = 0;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  UPDATELIMITS(uv_matrix[0], uv_matrix[1], x_min, x_max, y_min, y_max);

  xy_matrix[0] = width;
  xy_matrix[1] = height;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  UPDATELIMITS(uv_matrix[0], uv_matrix[1], x_min, x_max, y_min, y_max);

  xy_matrix[0] = 0;
  xy_matrix[1] = height;
  multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
               TRANSFORM_MAT_DIM, 1);
  UPDATELIMITS(uv_matrix[0], uv_matrix[1], x_min, x_max, y_min, y_max);

#undef UPDATELIMITS
}

// Finds x and y limits for arrays. Also finds the overall max and minimums
static void find_limits(int width, int height, const double **const params,
                        int num_frames, int *x_min, int *x_max, int *y_min,
                        int *y_max, int *pano_x_min, int *pano_x_max,
                        int *pano_y_min, int *pano_y_max) {
  *pano_x_max = INT_MIN;
  *pano_x_min = INT_MAX;
  *pano_y_max = INT_MIN;
  *pano_y_min = INT_MAX;
  for (int i = 0; i < num_frames; ++i) {
    find_frame_limit(width, height, (const double *const)params[i], &x_min[i],
                     &x_max[i], &y_min[i], &y_max[i]);
    if (x_max[i] > *pano_x_max) {
      *pano_x_max = x_max[i];
    }
    if (x_min[i] < *pano_x_min) {
      *pano_x_min = x_min[i];
    }
    if (y_max[i] > *pano_y_max) {
      *pano_y_max = y_max[i];
    }
    if (y_min[i] < *pano_y_min) {
      *pano_y_min = y_min[i];
    }
  }
}

// Inverts a 3x3 matrix that is in the parameter form.
static void invert_params(const double *const params, double *target) {
  double temp[MAX_PARAMDIM] = { 0 };
  params_to_matrix(params, temp);

  // Find determinant of matrix (expansion by minors).
  const double det = temp[0] * ((temp[4] * temp[8]) - (temp[5] * temp[7])) -
                     temp[1] * ((temp[3] * temp[8]) - (temp[5] * temp[6])) +
                     temp[2] * ((temp[3] * temp[7]) - (temp[4] * temp[6]));
  assert(det != 0);

  // inverse is transpose of cofactor * 1/det.
  double inverse[MAX_PARAMDIM] = { 0 };
  inverse[0] = (temp[4] * temp[8] - temp[7] * temp[5]) / det;
  inverse[1] = (temp[2] * temp[7] - temp[1] * temp[8]) / det;
  inverse[2] = (temp[1] * temp[5] - temp[2] * temp[4]) / det;
  inverse[3] = (temp[5] * temp[6] - temp[3] * temp[8]) / det;
  inverse[4] = (temp[0] * temp[8] - temp[2] * temp[6]) / det;
  inverse[5] = (temp[3] * temp[2] - temp[0] * temp[5]) / det;
  inverse[6] = (temp[3] * temp[7] - temp[6] * temp[4]) / det;
  inverse[7] = (temp[6] * temp[1] - temp[0] * temp[7]) / det;
  inverse[8] = (temp[0] * temp[4] - temp[3] * temp[1]) / det;

  matrix_to_params(inverse, target);
}

// swap_yuvs two YuvPixels.
static void swap_yuv(YuvPixel *a, YuvPixel *b) {
  const YuvPixel temp = *b;
  *b = *a;
  *a = temp;
}

// Partitions array to find pivot index in qselect.
static int partition(YuvPixel arr[], int left, int right, int pivot_idx) {
  YuvPixel pivot = arr[pivot_idx];

  // Move pivot to the end.
  swap_yuv(&arr[pivot_idx], &arr[right]);

  int p_idx = left;
  for (int i = left; i < right; ++i) {
    if (arr[i].y <= pivot.y) {
      swap_yuv(&arr[i], &arr[p_idx]);
      p_idx++;
    }
  }

  swap_yuv(&arr[p_idx], &arr[right]);

  return p_idx;
}

// Returns the kth element in array, partially sorted in place (quickselect).
static YuvPixel qselect(YuvPixel arr[], int left, int right, int k) {
  if (left >= right) {
    return arr[left];
  }
  unsigned int seed = time(NULL);
  int pivot_idx = left + rand_r(&seed) % (right - left + 1);
  pivot_idx = partition(arr, left, right, pivot_idx);

  if (k == pivot_idx) {
    return arr[k];
  } else if (k < pivot_idx) {
    return qselect(arr, left, pivot_idx - 1, k);
  } else {
    return qselect(arr, pivot_idx + 1, right, k);
  }
}

// Stitches images together to create ARF and stores it in 'panorama'.
static void stitch_images(YV12_BUFFER_CONFIG **const frames,
                          const int num_frames, const double **const params,
                          const int *const x_min, const int *const x_max,
                          const int *const y_min, const int *const y_max,
                          int pano_x_min, int pano_x_max, int pano_y_min,
                          int pano_y_max, YV12_BUFFER_CONFIG *panorama) {
  const int width = pano_x_max - pano_x_min + 1;
  const int height = pano_y_max - pano_y_min + 1;

  // Create temp_pano[y][x][num_frames] stack of pixel values
  YuvPixel ***temp_pano = aom_malloc(height * sizeof(*temp_pano));
  for (int i = 0; i < height; ++i) {
    temp_pano[i] = aom_malloc(width * sizeof(**temp_pano));
    for (int j = 0; j < width; ++j) {
      temp_pano[i][j] = aom_malloc(num_frames * sizeof(***temp_pano));
    }
  }
  // Create count[y][x] to count how many values in stack for median filtering
  int **count = aom_malloc(height * sizeof(*count));
  for (int i = 0; i < height; ++i) {
    count[i] = aom_calloc(width, sizeof(**count));  // counts initialized to 0
  }

  // Re-sample images onto panorama (pre-median filtering).
  const int x_offset = -pano_x_min;
  const int y_offset = -pano_y_min;
  const int frame_width = frames[0]->y_width;
  const int frame_height = frames[0]->y_height;
  for (int i = 0; i < num_frames; ++i) {
    // Find transforms from panorama coordinate system back to single image
    // coordinate system for sampling.
    int transformed_width = x_max[i] - x_min[i] + 1;
    int transformed_height = y_max[i] - y_min[i] + 1;

    double transform_matrix[MAX_PARAMDIM];
    double transform_params[MAX_PARAMDIM - 1];
    invert_params(params[i], transform_params);
    params_to_matrix(transform_params, transform_matrix);

#if CONFIG_HIGHBITDEPTH
    const uint16_t *y_buffer16 = CONVERT_TO_SHORTPTR(frames[i]->y_buffer);
    const uint16_t *u_buffer16 = CONVERT_TO_SHORTPTR(frames[i]->u_buffer);
    const uint16_t *v_buffer16 = CONVERT_TO_SHORTPTR(frames[i]->v_buffer);
#endif

    for (int y = 0; y < transformed_height; ++y) {
      for (int x = 0; x < transformed_width; ++x) {
        // Do transform.
        double xy_matrix[3] = { x + x_min[i], y + y_min[i], 1 };
        double uv_matrix[3] = { 0 };
        multiply_mat(transform_matrix, xy_matrix, uv_matrix, TRANSFORM_MAT_DIM,
                     TRANSFORM_MAT_DIM, 1);
        int image_x = (int)round(uv_matrix[0]);
        int image_y = (int)round(uv_matrix[1]);

        // Check if valid point in original image.
        if (image_x >= 0 && image_x < frame_width && image_y >= 0 &&
            image_y < frame_height) {
          // Place in panorama stack.
          int pano_x = x + x_min[i] + x_offset;
          int pano_y = y + y_min[i] + y_offset;

          int ychannel_idx = image_y * frames[i]->y_stride + image_x;
          int uvchannel_idx =
              (image_y >> frames[i]->subsampling_y) * frames[i]->uv_stride +
              (image_x >> frames[i]->subsampling_x);
#if CONFIG_HIGHBITDEPTH
          if (frames[i]->flags & YV12_FLAG_HIGHBITDEPTH) {
            temp_pano[pano_y][pano_x][count[pano_y][pano_x]].y =
                y_buffer16[ychannel_idx];
            temp_pano[pano_y][pano_x][count[pano_y][pano_x]].u =
                u_buffer16[uvchannel_idx];
            temp_pano[pano_y][pano_x][count[pano_y][pano_x]].v =
                v_buffer16[uvchannel_idx];
          } else {
#endif  // CONFIG_HIGHBITDEPTH
            temp_pano[pano_y][pano_x][count[pano_y][pano_x]].y =
                frames[i]->y_buffer[ychannel_idx];
            temp_pano[pano_y][pano_x][count[pano_y][pano_x]].u =
                frames[i]->u_buffer[uvchannel_idx];
            temp_pano[pano_y][pano_x][count[pano_y][pano_x]].v =
                frames[i]->v_buffer[uvchannel_idx];

            // Update count.
            count[pano_y][pano_x]++;
#if CONFIG_HIGHBITDEPTH
          }
#endif  // CONFIG_HIGHBITDEPTH
        }
      }
    }
  }

  // Apply median filtering using quickselect.
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      if (count[y][x] == 0) {
        // Just make the pixel black.
        // TODO(toddnguyen): Color the pixel with nearest neighbor
      } else {
        // Find
        const int median_idx = (int)floor(count[y][x] / 2);
        YuvPixel median =
            qselect(temp_pano[y][x], 0, count[y][x] - 1, median_idx);

        // Make the median value the 0th index for UV subsampling later
        temp_pano[y][x][0] = median;
        assert(median.y == temp_pano[y][x][0].y &&
               median.u == temp_pano[y][x][0].u &&
               median.v == temp_pano[y][x][0].v);
      }
    }
  }

  // NOTE(toddnguyen): Right now the ARF in the cpi struct is fixed size at the
  // same size as the frames. For now, we crop the generated panorama.
  assert(panorama->y_width < width && panorama->y_height < height);
  const int crop_x_offset = (width - panorama->y_width) / 2;
  const int crop_y_offset = (height - panorama->y_height) / 2;

#if CONFIG_HIGHBITDEPTH
  if (panorama->flags & YV12_FLAG_HIGHBITDEPTH) {
    // Use median Y value.
    uint16_t *pano_y_buffer16 = CONVERT_TO_SHORTPTR(panorama->y_buffer);
    for (int y = 0; y < panorama->y_height; ++y) {
      for (int x = 0; x < panorama->y_width; ++x) {
        const int ychannel_idx = y * panorama->y_stride + x;
        if (count[y + crop_y_offset][x + crop_x_offset] > 0) {
          pano_y_buffer16[ychannel_idx] =
              temp_pano[y + crop_y_offset][x + crop_x_offset][0].y;
        } else {
          pano_y_buffer16[ychannel_idx] = 0;
        }
      }
    }

    // UV subsampling with median UV values
    uint16_t *pano_u_buffer16 = CONVERT_TO_SHORTPTR(panorama->u_buffer);
    uint16_t *pano_v_buffer16 = CONVERT_TO_SHORTPTR(panorama->v_buffer);

    for (int y = 0; y < panorama->uv_height; ++y) {
      for (int x = 0; x < panorama->uv_width; ++x) {
        uint32_t avg_count = 0;
        uint32_t u_sum = 0;
        uint32_t v_sum = 0;

        // Look at surrounding pixels for subsampling
        for (int s_x = 0; s_x < panorama->subsampling_x + 1; ++s_x) {
          for (int s_y = 0; s_y < panorama->subsampling_y + 1; ++s_y) {
            int y_sample = crop_y_offset + (y << panorama->subsampling_y) + s_y;
            int x_sample = crop_x_offset + (x << panorama->subsampling_x) + s_x;
            if (y_sample > 0 && y_sample < height && x_sample > 0 &&
                x_sample < width && count[y_sample][x_sample] > 0) {
              u_sum += temp_pano[y_sample][x_sample][0].u;
              v_sum += temp_pano[y_sample][x_sample][0].v;
              avg_count++;
            }
          }
        }

        const int uvchannel_idx = y * panorama->uv_stride + x;
        if (avg_count != 0) {
          pano_u_buffer16[uvchannel_idx] = (uint16_t)OD_DIVU(u_sum, avg_count);
          pano_v_buffer16[uvchannel_idx] = (uint16_t)OD_DIVU(v_sum, avg_count);
        } else {
          pano_u_buffer16[uvchannel_idx] = 0;
          pano_v_buffer16[uvchannel_idx] = 0;
        }
      }
    }
  } else {
#endif  // CONFIG_HIGHBITDEPTH
    // Use median Y value.
    for (int y = 0; y < panorama->y_height; ++y) {
      for (int x = 0; x < panorama->y_width; ++x) {
        const int ychannel_idx = y * panorama->y_stride + x;
        if (count[y + crop_y_offset][x + crop_x_offset] > 0) {
          panorama->y_buffer[ychannel_idx] =
              temp_pano[y + crop_y_offset][x + crop_x_offset][0].y;
        } else {
          panorama->y_buffer[ychannel_idx] = 0;
        }
      }
    }

    // UV subsampling with median UV values
    for (int y = 0; y < panorama->uv_height; ++y) {
      for (int x = 0; x < panorama->uv_width; ++x) {
        uint16_t avg_count = 0;
        uint16_t u_sum = 0;
        uint16_t v_sum = 0;

        // Look at surrounding pixels for subsampling
        for (int s_x = 0; s_x < panorama->subsampling_x + 1; ++s_x) {
          for (int s_y = 0; s_y < panorama->subsampling_y + 1; ++s_y) {
            int y_sample = crop_y_offset + (y << panorama->subsampling_y) + s_y;
            int x_sample = crop_x_offset + (x << panorama->subsampling_x) + s_x;
            if (y_sample > 0 && y_sample < height && x_sample > 0 &&
                x_sample < width && count[y_sample][x_sample] > 0) {
              u_sum += temp_pano[y_sample][x_sample][0].u;
              v_sum += temp_pano[y_sample][x_sample][0].v;
              avg_count++;
            }
          }
        }

        const int uvchannel_idx = y * panorama->uv_stride + x;
        if (avg_count != 0) {
          panorama->u_buffer[uvchannel_idx] =
              (uint8_t)OD_DIVU(u_sum, avg_count);
          panorama->v_buffer[uvchannel_idx] =
              (uint8_t)OD_DIVU(v_sum, avg_count);
        } else {
          panorama->u_buffer[uvchannel_idx] = 0;
          panorama->v_buffer[uvchannel_idx] = 0;
        }
      }
    }
#if CONFIG_HIGHBITDEPTH
  }
#endif  // CONFIG_HIGHBITDEPTH

  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < width; ++j) {
      aom_free(temp_pano[i][j]);
    }
    aom_free(temp_pano[i]);
    aom_free(count[i]);
  }
  aom_free(count);
  aom_free(temp_pano);
}

int av1_background_sprite(AV1_COMP *cpi, int distance) {
  YV12_BUFFER_CONFIG *frames[MAX_LAG_BUFFERS] = { NULL };
  int inliers_by_motion[RANSAC_NUM_MOTIONS];
  static const double identity_params[MAX_PARAMDIM - 1] = {
    0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0
  };

  // Get frames to be included in background sprite.
  frames[0] = cpi->source;
  for (int frame = 0; frame < distance; ++frame) {
    struct lookahead_entry *buf = av1_lookahead_peek(cpi->lookahead, frame);
    frames[frame + 1] = &buf->img;
  }

  // Allocate empty arrays for parameters between frames.
  double **params = aom_malloc((distance + 1) * sizeof(*params));
  for (int i = 0; i < distance + 1; ++i) {
    params[i] = aom_malloc(sizeof(identity_params));
    memcpy(params[i], identity_params, sizeof(identity_params));
  }

  // Use global motion to find affine transformations between frames.
  // params[i] will have the transform from frame[i] to frame[i-1].
  // params[0] will have the identity matrix because it has no previous frame.
  TransformationType model = AFFINE;
  for (int frame = 0; frame < distance; ++frame) {
    const int global_motion_ret = compute_global_motion_feature_based(
        model, frames[frame + 1], frames[frame],
#if CONFIG_HIGHBITDEPTH
        cpi->common.bit_depth,
#endif  // CONFIG_HIGHBITDEPTH
        inliers_by_motion, params[frame + 1], RANSAC_NUM_MOTIONS);

    // Quit if global motion had an error.
    if (global_motion_ret == 0) {
      for (int i = 0; i < distance + 1; ++i) {
        aom_free(params[i]);
      }
      aom_free(params);
      return 1;
    }
  }

  // Compound the transformation parameters.
  for (int i = 1; i < distance + 1; ++i) {
    multiply_params(params[i - 1], params[i], params[i]);
  }

  // Compute frame limits for final stitched images.
  int pano_x_max = INT_MIN;
  int pano_x_min = INT_MAX;
  int pano_y_max = INT_MIN;
  int pano_y_min = INT_MAX;
  int *x_max = aom_malloc((distance + 1) * sizeof(*x_max));
  int *x_min = aom_malloc((distance + 1) * sizeof(*x_min));
  int *y_max = aom_malloc((distance + 1) * sizeof(*y_max));
  int *y_min = aom_malloc((distance + 1) * sizeof(*y_min));

  find_limits(cpi->initial_width, cpi->initial_height,
              (const double **const)params, distance + 1, x_min, x_max, y_min,
              y_max, &pano_x_min, &pano_x_max, &pano_y_min, &pano_y_max);

  // Estimate center image based on frame limits.
  const double pano_center_x = (pano_x_max + pano_x_min) / 2;
  const double pano_center_y = (pano_y_max + pano_y_min) / 2;
  double nearest_distance = DBL_MAX;
  int center_idx = -1;
  for (int i = 0; i < distance + 1; ++i) {
    const double image_center_x = (x_max[i] + x_min[i]) / 2;
    const double image_center_y = (y_max[i] + y_min[i]) / 2;
    const double distance_from_center = pow(pano_center_x - image_center_x, 2) +
                                        pow(pano_center_y + image_center_y, 2);
    if (distance_from_center < nearest_distance) {
      center_idx = i;
      nearest_distance = distance_from_center;
    }
  }
  assert(center_idx != -1);

  // Recompute transformations to adjust to center image.
  // Invert center image's transform.
  double inverse[MAX_PARAMDIM - 1] = { 0 };
  invert_params(params[center_idx], inverse);

  // Multiply the inverse to all transformation parameters.
  for (int i = 0; i < distance + 1; ++i) {
    multiply_params(inverse, params[i], params[i]);
  }

  // Recompute frame limits for new adjusted center.
  find_limits(cpi->initial_width, cpi->initial_height,
              (const double **const)params, distance + 1, x_min, x_max, y_min,
              y_max, &pano_x_min, &pano_x_max, &pano_y_min, &pano_y_max);

  // Stitch Images.
  stitch_images(frames, distance + 1, (const double **const)params, x_min,
                x_max, y_min, y_max, pano_x_min, pano_x_max, pano_y_min,
                pano_y_max, &cpi->alt_ref_buffer);

  // Free memory.
  for (int i = 0; i < distance + 1; ++i) {
    aom_free(params[i]);
  }
  aom_free(params);
  aom_free(x_max);
  aom_free(x_min);
  aom_free(y_max);
  aom_free(y_min);

  return 0;
}
