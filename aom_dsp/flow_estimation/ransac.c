/*
 * Copyright (c) 2022, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 3-Clause Clear License
 * and the Alliance for Open Media Patent License 1.0. If the BSD 3-Clause Clear
 * License was not distributed with this source code in the LICENSE file, you
 * can obtain it at aomedia.org/license/software-license/bsd-3-c-c/.  If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * aomedia.org/license/patent-license/.
 */

#include <memory.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdbool.h>
#include <assert.h>

#include "aom_dsp/flow_estimation/ransac.h"
#include "aom_dsp/mathutils.h"
#include "aom_mem/aom_mem.h"

// TODO(rachelbarker): Remove dependence on code in av1/encoder/
#include "av1/encoder/random.h"

#define MAX_MINPTS 4
#define MAX_DEGENERATE_ITER 10
#define MINPTS_MULTIPLIER 5

#define INLIER_THRESHOLD 1.25
#define INLIER_THRESHOLD_SQUARED (INLIER_THRESHOLD * INLIER_THRESHOLD)
#define MIN_TRIALS 20

// Flag to enable functions for finding TRANSLATION type models.
//
// These modes are not considered currently due to a spec bug (see comments
// in gm_get_motion_vector() in av1/common/mv.h). Thus we don't need to compile
// the corresponding search functions, but it is nice to keep the source around
// but disabled, for completeness.
#define ALLOW_TRANSLATION_MODELS 0

////////////////////////////////////////////////////////////////////////////////
// ransac
typedef bool (*IsDegenerateFunc)(double *p);
typedef bool (*FindTransformationFunc)(int points, const double *points1,
                                       const double *points2, double *params);
typedef void (*ProjectPointsFunc)(const double *mat, const double *points,
                                  double *proj, int n, int stride_points,
                                  int stride_proj);

#if ALLOW_TRANSLATION_MODELS
static void project_points_translation(const double *mat, const double *points,
                                       double *proj, int n, int stride_points,
                                       int stride_proj) {
  int i;
  for (i = 0; i < n; ++i) {
    const double x = *(points++), y = *(points++);
    *(proj++) = x + mat[0];
    *(proj++) = y + mat[1];
    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}
#endif  // ALLOW_TRANSLATION_MODELS

static void project_points_affine(const double *mat, const double *points,
                                  double *proj, int n, int stride_points,
                                  int stride_proj) {
  int i;
  for (i = 0; i < n; ++i) {
    const double x = *(points++), y = *(points++);
    *(proj++) = mat[2] * x + mat[3] * y + mat[0];
    *(proj++) = mat[4] * x + mat[5] * y + mat[1];
    points += stride_points - 2;
    proj += stride_proj - 2;
  }
}

#if ALLOW_TRANSLATION_MODELS
static bool find_translation(int np, const double *pts1, const double *pts2,
                             double *params) {
  double sumx = 0;
  double sumy = 0;

  for (int i = 0; i < np; ++i) {
    double dx = *(pts2++);
    double dy = *(pts2++);
    double sx = *(pts1++);
    double sy = *(pts1++);

    sumx += dx - sx;
    sumy += dy - sy;
  }

  params[0] = sumx / np;
  params[1] = sumy / np;
  params[2] = 1;
  params[3] = 0;
  params[4] = 0;
  params[5] = 1;
  return true;
}
#endif  // ALLOW_TRANSLATION_MODELS

static bool find_rotzoom(int np, const double *pts1, const double *pts2,
                         double *params) {
  const int n = 4;    // Size of least-squares problem
  double mat[4 * 4];  // Accumulator for A'A
  double y[4];        // Accumulator for A'b
  double x[4];        // Output vector
  double a[4];        // Single row of A
  double b;           // Single element of b

  ls_init(mat, y, n);
  for (int i = 0; i < np; ++i) {
    double dx = *(pts2++);
    double dy = *(pts2++);
    double sx = *(pts1++);
    double sy = *(pts1++);

    a[0] = sx;
    a[1] = sy;
    a[2] = 1;
    a[3] = 0;
    b = dx;
    ls_accumulate(mat, y, a, b, n);

    a[0] = sy;
    a[1] = -sx;
    a[2] = 0;
    a[3] = 1;
    b = dy;
    ls_accumulate(mat, y, a, b, n);
  }

  if (!ls_solve(mat, y, x, n)) {
    return false;
  }

  // Rearrange least squares result to form output model
  params[0] = x[2];
  params[1] = x[3];
  params[2] = x[0];
  params[3] = x[1];
  params[4] = -params[3];
  params[5] = params[2];

  return true;
}

// TODO(rachelbarker): As the x and y equations are decoupled in find_affine(),
// the least-squares problem can be split this into two 3-dimensional problems,
// which should be faster to solve.
static bool find_affine(int np, const double *pts1, const double *pts2,
                        double *params) {
  const int n = 6;    // Size of least-squares problem
  double mat[6 * 6];  // Accumulator for A'A
  double y[6];        // Accumulator for A'b
  double x[6];        // Output vector
  double a[6];        // Single row of A
  double b;           // Single element of b

  for (int i = 0; i < np; ++i) {
    double dx = *(pts2++);
    double dy = *(pts2++);
    double sx = *(pts1++);
    double sy = *(pts1++);

    a[0] = sx;
    a[1] = sy;
    a[2] = 0;
    a[3] = 0;
    a[4] = 1;
    a[5] = 0;
    b = dx;
    ls_accumulate(mat, y, a, b, n);

    a[0] = 0;
    a[1] = 0;
    a[2] = sx;
    a[3] = sy;
    a[4] = 0;
    a[5] = 1;
    b = dy;
    ls_accumulate(mat, y, a, b, n);
  }

  if (!ls_solve(mat, y, x, n)) {
    return false;
  }

  // Rearrange least squares result to form output model
  params[0] = mat[4];
  params[1] = mat[5];
  params[2] = mat[0];
  params[3] = mat[1];
  params[4] = mat[2];
  params[5] = mat[3];

  return true;
}

// Returns true on success, false if not enough points provided
static bool get_rand_indices(int npoints, int minpts, int *indices,
                             unsigned int *seed) {
  int i, j;
  int ptr = lcg_rand16(seed) % npoints;
  if (minpts > npoints) return false;
  indices[0] = ptr;
  ptr = (ptr == npoints - 1 ? 0 : ptr + 1);
  i = 1;
  while (i < minpts) {
    int index = lcg_rand16(seed) % npoints;
    while (index) {
      ptr = (ptr == npoints - 1 ? 0 : ptr + 1);
      for (j = 0; j < i; ++j) {
        if (indices[j] == ptr) break;
      }
      if (j == i) index--;
    }
    indices[i++] = ptr;
  }
  return true;
}

typedef struct {
  int num_inliers;
  double variance;
  int *inlier_indices;
} RANSAC_MOTION;

// Return -1 if 'a' is a better motion, 1 if 'b' is better, 0 otherwise.
static int compare_motions(const void *arg_a, const void *arg_b) {
  const RANSAC_MOTION *motion_a = (RANSAC_MOTION *)arg_a;
  const RANSAC_MOTION *motion_b = (RANSAC_MOTION *)arg_b;

  if (motion_a->num_inliers > motion_b->num_inliers) return -1;
  if (motion_a->num_inliers < motion_b->num_inliers) return 1;
  if (motion_a->variance < motion_b->variance) return -1;
  if (motion_a->variance > motion_b->variance) return 1;
  return 0;
}

static bool is_better_motion(const RANSAC_MOTION *motion_a,
                             const RANSAC_MOTION *motion_b) {
  return compare_motions(motion_a, motion_b) < 0;
}

static void copy_points_at_indices(double *dest, const double *src,
                                   const int *indices, int num_points) {
  for (int i = 0; i < num_points; ++i) {
    const int index = indices[i];
    dest[i * 2] = src[index * 2];
    dest[i * 2 + 1] = src[index * 2 + 1];
  }
}

static const double kInfiniteVariance = 1e12;

static void clear_motion(RANSAC_MOTION *motion, int num_points) {
  motion->num_inliers = 0;
  motion->variance = kInfiniteVariance;
  memset(motion->inlier_indices, 0,
         sizeof(*motion->inlier_indices) * num_points);
}

// Returns true on success, false on error
static bool ransac_internal(const Correspondence *matched_points, int npoints,
                            MotionModel *params_by_motion,
                            int num_desired_motions, int minpts,
                            IsDegenerateFunc is_degenerate,
                            FindTransformationFunc find_transformation,
                            ProjectPointsFunc projectpoints) {
  int trial_count = 0;
  int i = 0;
  bool ret_val = true;

  unsigned int seed = (unsigned int)npoints;

  int indices[MAX_MINPTS] = { 0 };

  double *points1, *points2;
  double *corners1, *corners2;
  double *image1_coord;

  // Store information for the num_desired_motions best transformations found
  // and the worst motion among them, as well as the motion currently under
  // consideration.
  RANSAC_MOTION *motions, *worst_kept_motion = NULL;
  RANSAC_MOTION current_motion;

  // Store the parameters and the indices of the inlier points for the motion
  // currently under consideration.
  double params_this_motion[MAX_PARAMDIM];

  for (i = 0; i < num_desired_motions; ++i) {
    params_by_motion[i].num_inliers = 0;
  }
  if (npoints < minpts * MINPTS_MULTIPLIER || npoints == 0) {
    return false;
  }

  points1 = (double *)aom_malloc(sizeof(*points1) * npoints * 2);
  points2 = (double *)aom_malloc(sizeof(*points2) * npoints * 2);
  corners1 = (double *)aom_malloc(sizeof(*corners1) * npoints * 2);
  corners2 = (double *)aom_malloc(sizeof(*corners2) * npoints * 2);
  image1_coord = (double *)aom_malloc(sizeof(*image1_coord) * npoints * 2);

  motions =
      (RANSAC_MOTION *)aom_malloc(sizeof(RANSAC_MOTION) * num_desired_motions);
  for (i = 0; i < num_desired_motions; ++i) {
    motions[i].inlier_indices =
        (int *)aom_malloc(sizeof(*motions->inlier_indices) * npoints);
    clear_motion(motions + i, npoints);
  }
  current_motion.inlier_indices =
      (int *)aom_malloc(sizeof(*current_motion.inlier_indices) * npoints);
  clear_motion(&current_motion, npoints);

  worst_kept_motion = motions;

  if (!(points1 && points2 && corners1 && corners2 && image1_coord && motions &&
        current_motion.inlier_indices)) {
    ret_val = false;
    goto finish_ransac;
  }

  for (i = 0; i < npoints; ++i) {
    corners1[2 * i + 0] = matched_points[i].x;
    corners1[2 * i + 1] = matched_points[i].y;
    corners2[2 * i + 0] = matched_points[i].rx;
    corners2[2 * i + 1] = matched_points[i].ry;
  }

  while (MIN_TRIALS > trial_count) {
    double sum_distance = 0.0;
    double sum_distance_squared = 0.0;

    clear_motion(&current_motion, npoints);

    int degenerate = 1;
    int num_degenerate_iter = 0;

    while (degenerate) {
      num_degenerate_iter++;
      if (!get_rand_indices(npoints, minpts, indices, &seed)) {
        ret_val = false;
        goto finish_ransac;
      }

      copy_points_at_indices(points1, corners1, indices, minpts);
      copy_points_at_indices(points2, corners2, indices, minpts);

      degenerate = is_degenerate(points1);
      if (num_degenerate_iter > MAX_DEGENERATE_ITER) {
        ret_val = false;
        goto finish_ransac;
      }
    }

    if (!find_transformation(minpts, points1, points2, params_this_motion)) {
      trial_count++;
      continue;
    }

    projectpoints(params_this_motion, corners1, image1_coord, npoints, 2, 2);

    for (i = 0; i < npoints; ++i) {
      double dx = image1_coord[i * 2] - corners2[i * 2];
      double dy = image1_coord[i * 2 + 1] - corners2[i * 2 + 1];
      double distance_squared = dx * dx + dy * dy;

      if (distance_squared < INLIER_THRESHOLD_SQUARED) {
        current_motion.inlier_indices[current_motion.num_inliers++] = i;
        sum_distance += sqrt(distance_squared);
        sum_distance_squared += distance_squared;
      }
    }

    if (current_motion.num_inliers >= worst_kept_motion->num_inliers &&
        current_motion.num_inliers > 1) {
      double mean_distance;
      mean_distance = sum_distance / ((double)current_motion.num_inliers);
      current_motion.variance =
          sum_distance_squared / ((double)current_motion.num_inliers - 1.0) -
          mean_distance * mean_distance * ((double)current_motion.num_inliers) /
              ((double)current_motion.num_inliers - 1.0);
      if (is_better_motion(&current_motion, worst_kept_motion)) {
        // This motion is better than the worst currently kept motion. Remember
        // the inlier points and variance. The parameters for each kept motion
        // will be recomputed later using only the inliers.
        worst_kept_motion->num_inliers = current_motion.num_inliers;
        worst_kept_motion->variance = current_motion.variance;
        memcpy(worst_kept_motion->inlier_indices, current_motion.inlier_indices,
               sizeof(*current_motion.inlier_indices) * npoints);
        assert(npoints > 0);
        // Determine the new worst kept motion and its num_inliers and variance.
        for (i = 0; i < num_desired_motions; ++i) {
          if (is_better_motion(worst_kept_motion, &motions[i])) {
            worst_kept_motion = &motions[i];
          }
        }
      }
    }
    trial_count++;
  }

  // Sort the motions, best first.
  qsort(motions, num_desired_motions, sizeof(RANSAC_MOTION), compare_motions);

  // Recompute the motions using only the inliers.
  for (i = 0; i < num_desired_motions; ++i) {
    if (motions[i].num_inliers >= minpts) {
      int num_inliers = motions[i].num_inliers;
      copy_points_at_indices(points1, corners1, motions[i].inlier_indices,
                             num_inliers);
      copy_points_at_indices(points2, corners2, motions[i].inlier_indices,
                             num_inliers);

      find_transformation(num_inliers, points1, points2,
                          params_by_motion[i].params);

      // Populate inliers array
      for (int j = 0; j < num_inliers; j++) {
        int index = motions[i].inlier_indices[j];
        const Correspondence *corr = &matched_points[index];
        params_by_motion[i].inliers[2 * j + 0] = (int)rint(corr->x);
        params_by_motion[i].inliers[2 * j + 1] = (int)rint(corr->y);
      }
    }
    params_by_motion[i].num_inliers = motions[i].num_inliers;
  }

finish_ransac:
  aom_free(points1);
  aom_free(points2);
  aom_free(corners1);
  aom_free(corners2);
  aom_free(image1_coord);
  aom_free(current_motion.inlier_indices);
  if (motions) {
    for (i = 0; i < num_desired_motions; ++i) {
      aom_free(motions[i].inlier_indices);
    }
    aom_free(motions);
  }

  return ret_val;
}

static bool is_collinear3(double *p1, double *p2, double *p3) {
  static const double collinear_eps = 1e-3;
  const double v =
      (p2[0] - p1[0]) * (p3[1] - p1[1]) - (p2[1] - p1[1]) * (p3[0] - p1[0]);
  return fabs(v) < collinear_eps;
}

#if ALLOW_TRANSLATION_MODELS
static bool is_degenerate_translation(double *p) {
  return (p[0] - p[2]) * (p[0] - p[2]) + (p[1] - p[3]) * (p[1] - p[3]) <= 2;
}
#endif  // ALLOW_TRANSLATION_MODELS

static bool is_degenerate_affine(double *p) {
  return is_collinear3(p, p + 2, p + 4);
}

static IsDegenerateFunc is_degenerate[TRANS_TYPES] = {
  NULL,  // IDENTITY
#if ALLOW_TRANSLATION_MODELS
  is_degenerate_translation,  // TRANSLATION
#else
  NULL,  // TRANSLATION
#endif                   // ALLOW_TRANSLATION_MODELS
  is_degenerate_affine,  // ROTZOOM
  is_degenerate_affine,  // AFFINE
};

static FindTransformationFunc find_transform[TRANS_TYPES] = {
  NULL,  // IDENTITY
#if ALLOW_TRANSLATION_MODELS
  find_translation,  // TRANSLATION
#else
  NULL,  // TRANSLATION
#endif           // ALLOW_TRANSLATION_MODELS
  find_rotzoom,  // ROTZOOM
  find_affine,   // AFFINE
};

static ProjectPointsFunc project_points[TRANS_TYPES] = {
  NULL,  // IDENTITY
#if ALLOW_TRANSLATION_MODELS
  project_points_translation,  // TRANSLATION
#else
  NULL,  // TRANSLATION
#endif                    // ALLOW_TRANSLATION_MODELS
  project_points_affine,  // ROTZOOM
  project_points_affine,  // AFFINE
};

// Returns true on success, false on error
bool ransac(const Correspondence *matched_points, int npoints,
            TransformationType type, MotionModel *params_by_motion,
            int num_desired_motions) {
#if ALLOW_TRANSLATION_MODELS
  assert(type > IDENTITY && type < TRANS_TYPES);
#else
  assert(type > TRANSLATION && type < TRANS_TYPES);
#endif  // ALLOW_TRANSLATION_MODELS

  int minpts = 3;

  return ransac_internal(matched_points, npoints, params_by_motion,
                         num_desired_motions, minpts, is_degenerate[type],
                         find_transform[type], project_points[type]);
}

// Fit a specified type of motion model to a set of correspondences.
// The input consists of `np` points, where pts1 stores the source position
// and pts2 stores the destination position for each correspondence.
// The resulting model is stored in `mat`.
// Returns true on success, false on error
//
// Note: The input points lists may be modified during processing
bool aom_fit_motion_model(TransformationType type, int np, const double *pts1,
                          const double *pts2, double *mat) {
#if ALLOW_TRANSLATION_MODELS
  assert(type > IDENTITY && type < TRANS_TYPES);
#else
  assert(type > TRANSLATION && type < TRANS_TYPES);
#endif  // ALLOW_TRANSLATION_MODELS
  return find_transform[type](np, pts1, pts2, mat);
}
