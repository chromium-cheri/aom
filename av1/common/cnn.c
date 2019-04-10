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

#include "aom_dsp/aom_dsp_common.h"
#include "av1/common/cnn.h"
#include "av1/common/onyxc_int.h"

#define CLAMPINDEX(a, hi) ((a) < 0 ? 0 : ((a) >= (hi) ? ((hi)-1) : (a)))

typedef float (*activation_fn)(float);

static float softsign(float x) { return x / (float)(fabsf(x) + 1.0); }

static float relu(float x) { return (x < 0) ? 0 : x; }

static float identity(float x) { return x; }

typedef struct {
  int allocsize;
  int channels;
  int width, height, stride;
  float *buf[CNN_MAX_CHANNELS];
} TENSOR;

static void init_tensor(TENSOR *tensor) { memset(tensor, 0, sizeof(*tensor)); }

static void free_tensor(TENSOR *tensor) {
  if (tensor->allocsize) {
    aom_free(tensor->buf[0]);
    tensor->buf[0] = NULL;
    tensor->allocsize = 0;
  }
}

static void realloc_tensor(TENSOR *tensor, int channels, int width,
                           int height) {
  const int newallocsize = channels * width * height;
  if (tensor->allocsize < newallocsize) {
    free_tensor(tensor);
    tensor->buf[0] =
        (float *)aom_malloc(sizeof(*tensor->buf[0]) * newallocsize);
    tensor->allocsize = newallocsize;
  }
  tensor->width = width;
  tensor->height = height;
  tensor->stride = width;
  tensor->channels = channels;
  for (int c = 1; c < channels; ++c)
    tensor->buf[c] = &tensor->buf[0][c * width * height];
}

static void copy_tensor(const TENSOR *src, int copy_channels, int dst_offset,
                        TENSOR *dst) {
  assert(src->width == dst->width);
  assert(src->height == dst->height);
  assert(copy_channels <= src->channels);
  if (src->stride == dst->width && dst->stride == dst->width) {
    memcpy(dst->buf[dst_offset], src->buf[0],
           sizeof(*dst->buf[dst_offset]) * src->width * src->height *
               copy_channels);
  } else {
    for (int c = 0; c < copy_channels; ++c) {
      for (int r = 0; r < dst->height; ++r) {
        memcpy(&dst->buf[dst_offset + c][r * dst->stride],
               &src->buf[c][r * src->stride],
               dst->width * sizeof(*dst->buf[c]));
      }
    }
  }
}

static void assign_tensor(TENSOR *tensor, const float *buf[CNN_MAX_CHANNELS],
                          int channels, int width, int height, int stride) {
  tensor->allocsize = 0;
  tensor->channels = channels;
  tensor->width = width;
  tensor->height = height;
  tensor->stride = stride;
  if (buf) {
    for (int c = 0; c < channels; ++c) tensor->buf[c] = (float *)buf[c];
  } else {
    for (int c = 0; c < channels; ++c) tensor->buf[c] = NULL;
  }
}

static void swap_tensor(TENSOR *t1, TENSOR *t2) {
  TENSOR t = *t1;
  *t1 = *t2;
  *t2 = t;
}

// The concatenated tensor goes into dst with first the channels in
// original dst followed by the channels in the src
static void concat_tensor(const TENSOR *src, TENSOR *dst) {
  assert(src->width == dst->width);
  assert(src->height == dst->height);

  const int dst_channels = dst->channels;
  const int channels = dst->channels + src->channels;
  const int newallocsize = channels * dst->width * dst->height;
  if (dst->allocsize < newallocsize) {
    TENSOR t;
    init_tensor(&t);
    // allocate new buffers and copy first the dst channels
    realloc_tensor(&t, channels, dst->width, dst->height);
    copy_tensor(dst, dst->channels, 0, &t);
    // Swap the tensors and free the old buffers
    swap_tensor(dst, &t);
    free_tensor(&t);
  }
  for (int c = 1; c < channels; ++c)
    dst->buf[c] = &dst->buf[0][c * dst->width * dst->height];
  // Copy the channels in src after the first dst_channels channels.
  copy_tensor(src, src->channels, dst_channels, dst);
}

int check_tensor_equal_dims(TENSOR *t1, TENSOR *t2) {
  return (t1->width == t2->width && t1->height == t2->height);
}

int check_tensor_equal_size(TENSOR *t1, TENSOR *t2) {
  return (t1->channels == t2->channels && t1->width == t2->width &&
          t1->height == t2->height);
}

static void find_layer_output_size(int in_width, int in_height,
                                   const CNN_LAYER_CONFIG *layer_config,
                                   int *out_width, int *out_height) {
  if (!layer_config->deconvolve) {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
      case PADDING_SAME_REPLICATE:
        *out_width = (in_width + layer_config->skip_width - 1) /
                     layer_config->skip_width;
        *out_height = (in_height + layer_config->skip_height - 1) /
                      layer_config->skip_height;
        break;
      case PADDING_VALID:
        *out_width =
            (in_width - layer_config->filter_width + layer_config->skip_width) /
            layer_config->skip_width;
        *out_height = (in_height - layer_config->filter_height +
                       layer_config->skip_height) /
                      layer_config->skip_height;
        break;
      default: assert(0 && "Unknown padding type");
    }
  } else {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
      case PADDING_SAME_REPLICATE:
        *out_width = in_width * layer_config->skip_width;
        *out_height = in_height * layer_config->skip_height;
        break;
      case PADDING_VALID:
        *out_width = (in_width - 1) * layer_config->skip_width +
                     layer_config->filter_width;
        *out_height = (in_height - 1) * layer_config->skip_height +
                      layer_config->filter_height;
        break;
      default: assert(0 && "Unknown padding type");
    }
  }
}

void av1_find_cnn_output_size(int in_width, int in_height,
                              const CNN_CONFIG *cnn_config, int *out_width,
                              int *out_height) {
  int i_width = in_width + cnn_config->ext_width * 2;
  int i_height = in_height + cnn_config->ext_height * 2;
  for (int i = 0; i < cnn_config->num_layers; ++i) {
    int o_width = 0, o_height = 0;
    find_layer_output_size(i_width, i_height, &cnn_config->layer_config[i],
                           &o_width, &o_height);
    i_width = o_width;
    i_height = o_height;
  }
  *out_width = i_width;
  *out_height = i_height;
}

activation_fn get_activation(ACTIVATION layer_activation) {
  switch (layer_activation) {
    case NONE: return identity;
    case RELU: return relu;
    case SOFTSIGN: return softsign;
    default: assert(0 && "Unknown padding type"); return NULL;
  }
}

static int get_start_shift_convolve(int width, int filt_width, int stride) {
  const int mod = (width % stride);
  const int filt_off = (filt_width - 1) / 2;
  const int dif = (mod ? mod - 1 : stride - 1);
  return AOMMIN((dif + (filt_width % 2)) / 2, filt_off);
}

void av1_cnn_add_c(float **output, int channels, int width, int height,
                   int stride, const float **add) {
  for (int c = 0; c < channels; ++c) {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j)
        output[c][i * stride + j] += add[c][i * stride + j];
  }
}

void av1_cnn_activate_c(float **output, int channels, int width, int height,
                        int stride, ACTIVATION layer_activation) {
  activation_fn activation = get_activation(layer_activation);
  for (int c = 0; c < channels; ++c) {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j)
        output[c][i * stride + j] = activation(output[c][i * stride + j]);
  }
}

static void copy_active_tensor_to_branches(const TENSOR *layer_active_tensor,
                                           const CNN_LAYER_CONFIG *layer_config,
                                           int branch, TENSOR branch_output[]) {
  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    if ((layer_config->input_to_branches & (1 << b)) && b != branch) {
      // Copy layer's active tensor to output tensor of branch b if set in
      // mask. The output becomes the input of the first layer of the branch
      // because the layer of the branch is not the first layer.
      int copy_channels = layer_active_tensor->channels;
      realloc_tensor(&branch_output[b], copy_channels,
                     layer_active_tensor->width, layer_active_tensor->height);
      copy_tensor(layer_active_tensor, copy_channels, 0, &branch_output[b]);
    }
  }
}

void av1_cnn_convolve_c(const float **input, int in_width, int in_height,
                        int in_stride, const CNN_LAYER_CONFIG *layer_config,
                        float **output, int out_stride) {
  assert(!layer_config->deconvolve);

  const int cstep = layer_config->in_channels * layer_config->out_channels;
  const int filter_height_half = layer_config->filter_height >> 1;
  const int filter_width_half = layer_config->filter_width >> 1;

  if (layer_config->maxpool &&
      (layer_config->skip_height > 1 || layer_config->skip_width > 1)) {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
        for (int i = 0; i < layer_config->out_channels; ++i) {
          for (int h = 0, u = 0; h < in_height;
               h += layer_config->skip_height, ++u) {
            for (int w = 0, v = 0; w < in_width;
                 w += layer_config->skip_width, ++v) {
              for (int hh = h;
                   hh < AOMMIN(in_height, h + layer_config->skip_height);
                   ++hh) {
                for (int ww = w;
                     ww < AOMMIN(in_width, w + layer_config->skip_width);
                     ++ww) {
                  float sum = layer_config->bias[i];
                  for (int k = 0; k < layer_config->in_channels; ++k) {
                    int off = k * layer_config->out_channels + i;
                    for (int l = 0; l < layer_config->filter_height; ++l) {
                      const int ii = hh + l - filter_height_half;
                      for (int m = 0; m < layer_config->filter_width;
                           ++m, off += cstep) {
                        const int jj = ww + m - filter_width_half;
                        if (ii < 0 || ii >= in_height || jj < 0 ||
                            jj >= in_width)
                          continue;
                        sum += layer_config->weights[off] *
                               input[k][ii * in_stride + jj];
                      }
                    }
                  }
                  const float a = sum;
                  if (h == hh && w == ww)
                    output[i][u * out_stride + v] = a;
                  else
                    output[i][u * out_stride + v] =
                        AOMMAX(output[i][u * out_stride + v], a);
                }
              }
            }
          }
        }
        break;
      case PADDING_SAME_REPLICATE:
        for (int i = 0; i < layer_config->out_channels; ++i) {
          for (int h = 0, u = 0; h < in_height;
               h += layer_config->skip_height, ++u) {
            for (int w = 0, v = 0; w < in_width;
                 w += layer_config->skip_width, ++v) {
              for (int hh = h;
                   hh < AOMMIN(in_height, h + layer_config->skip_height);
                   ++hh) {
                for (int ww = w;
                     ww < AOMMIN(in_width, w + layer_config->skip_width);
                     ++ww) {
                  float sum = layer_config->bias[i];
                  for (int k = 0; k < layer_config->in_channels; ++k) {
                    int off = k * layer_config->out_channels + i;
                    for (int l = 0; l < layer_config->filter_height; ++l) {
                      const int ii =
                          CLAMPINDEX(hh + l - filter_height_half, in_height);
                      for (int m = 0; m < layer_config->filter_width;
                           ++m, off += cstep) {
                        const int jj =
                            CLAMPINDEX(ww + m - filter_width_half, in_width);
                        assert(ii >= 0 && ii < in_height && jj >= 0 &&
                               jj < in_width);
                        sum += layer_config->weights[off] *
                               input[k][ii * in_stride + jj];
                      }
                    }
                  }
                  const float a = sum;
                  if (h == hh && w == ww)
                    output[i][u * out_stride + v] = a;
                  else
                    output[i][u * out_stride + v] =
                        AOMMAX(output[i][u * out_stride + v], a);
                }
              }
            }
          }
        }
        break;
      case PADDING_VALID:
        for (int i = 0; i < layer_config->out_channels; ++i) {
          for (int h = 0, u = 0;
               h < in_height - layer_config->filter_height + 1;
               h += layer_config->skip_height, ++u) {
            for (int w = 0, v = 0;
                 w < in_width - layer_config->filter_width + 1;
                 w += layer_config->skip_width, ++v) {
              for (int hh = h;
                   hh < AOMMIN(in_height, h + layer_config->skip_height);
                   ++hh) {
                for (int ww = w;
                     ww < AOMMIN(in_width, w + layer_config->skip_width);
                     ++ww) {
                  float sum = layer_config->bias[i];
                  for (int k = 0; k < layer_config->in_channels; ++k) {
                    int off = k * layer_config->out_channels + i;
                    for (int l = 0; l < layer_config->filter_height; ++l) {
                      const int ii = hh + l;
                      for (int m = 0; m < layer_config->filter_width;
                           ++m, off += cstep) {
                        const int jj = ww + m;
                        assert(ii >= 0 && ii < in_height && jj >= 0 &&
                               jj < in_width);
                        sum += layer_config->weights[off] *
                               input[k][ii * in_stride + jj];
                      }
                    }
                  }
                  const float a = sum;
                  if (h == hh && w == ww)
                    output[i][u * out_stride + v] = a;
                  else
                    output[i][u * out_stride + v] =
                        AOMMAX(output[i][u * out_stride + v], a);
                }
              }
            }
          }
        }
        break;
      default: assert(0 && "Unknown padding type");
    }
  } else {
    switch (layer_config->pad) {
      case PADDING_SAME_ZERO:
        for (int i = 0; i < layer_config->out_channels; ++i) {
          for (int h = get_start_shift_convolve(in_height,
                                                layer_config->filter_height,
                                                layer_config->skip_height),
                   u = 0;
               h < in_height; h += layer_config->skip_height, ++u) {
            for (int w = get_start_shift_convolve(in_width,
                                                  layer_config->filter_width,
                                                  layer_config->skip_width),
                     v = 0;
                 w < in_width; w += layer_config->skip_width, ++v) {
              float sum = layer_config->bias[i];
              for (int k = 0; k < layer_config->in_channels; ++k) {
                int off = k * layer_config->out_channels + i;
                for (int l = 0; l < layer_config->filter_height; ++l) {
                  const int ii = h + l - filter_height_half +
                                 (layer_config->filter_height - 1) % 2;
                  for (int m = 0; m < layer_config->filter_width;
                       ++m, off += cstep) {
                    const int jj = w + m - filter_width_half +
                                   (layer_config->filter_width - 1) % 2;
                    if (ii < 0 || ii >= in_height || jj < 0 || jj >= in_width)
                      continue;
                    sum += layer_config->weights[off] *
                           input[k][ii * in_stride + jj];
                  }
                }
              }
              output[i][u * out_stride + v] = sum;
            }
          }
        }
        break;
      case PADDING_SAME_REPLICATE:
        for (int i = 0; i < layer_config->out_channels; ++i) {
          for (int h = get_start_shift_convolve(in_height,
                                                layer_config->filter_height,
                                                layer_config->skip_height),
                   u = 0;
               h < in_height; h += layer_config->skip_height, ++u) {
            for (int w = get_start_shift_convolve(in_width,
                                                  layer_config->filter_width,
                                                  layer_config->skip_width),
                     v = 0;
                 w < in_width; w += layer_config->skip_width, ++v) {
              float sum = layer_config->bias[i];
              for (int k = 0; k < layer_config->in_channels; ++k) {
                int off = k * layer_config->out_channels + i;
                for (int l = 0; l < layer_config->filter_height; ++l) {
                  const int ii =
                      CLAMPINDEX(h + l - filter_height_half +
                                     (layer_config->filter_height - 1) % 2,
                                 in_height);
                  for (int m = 0; m < layer_config->filter_width;
                       ++m, off += cstep) {
                    const int jj =
                        CLAMPINDEX(w + m - filter_width_half +
                                       (layer_config->filter_width - 1) % 2,
                                   in_width);
                    assert(ii >= 0 && ii < in_height && jj >= 0 &&
                           jj < in_width);
                    sum += layer_config->weights[off] *
                           input[k][ii * in_stride + jj];
                  }
                }
              }
              output[i][u * out_stride + v] = sum;
            }
          }
        }
        break;
      case PADDING_VALID:
        for (int i = 0; i < layer_config->out_channels; ++i) {
          for (int h = 0, u = 0;
               h < in_height - layer_config->filter_height + 1;
               h += layer_config->skip_height, ++u) {
            for (int w = 0, v = 0;
                 w < in_width - layer_config->filter_width + 1;
                 w += layer_config->skip_width, ++v) {
              float sum = layer_config->bias[i];
              for (int k = 0; k < layer_config->in_channels; ++k) {
                int off = k * layer_config->out_channels + i;
                for (int l = 0; l < layer_config->filter_height; ++l) {
                  const int ii = h + l;
                  for (int m = 0; m < layer_config->filter_width;
                       ++m, off += cstep) {
                    const int jj = w + m;
                    assert(ii >= 0 && ii < in_height && jj >= 0 &&
                           jj < in_width);
                    sum += layer_config->weights[off] *
                           input[k][ii * in_stride + jj];
                  }
                }
              }
              output[i][u * out_stride + v] = sum;
            }
          }
        }
        break;
      default: assert(0 && "Unknown padding type");
    }
  }
}

static int get_start_shift_deconvolve(int filt_width, int stride) {
  const int dif = AOMMAX(filt_width - stride, 0);
  return dif / 2;
}

void av1_cnn_deconvolve_c(const float **input, int in_width, int in_height,
                          int in_stride, const CNN_LAYER_CONFIG *layer_config,
                          float **output, int out_stride) {
  assert(layer_config->deconvolve);

  const int cstep = layer_config->in_channels * layer_config->out_channels;

  int out_width = 0;
  int out_height = 0;
  find_layer_output_size(in_width, in_height, layer_config, &out_width,
                         &out_height);
  switch (layer_config->pad) {
    case PADDING_SAME_ZERO:
      for (int i = 0; i < layer_config->out_channels; ++i) {
        for (int u = 0; u < out_height; ++u) {
          for (int v = 0; v < out_width; ++v) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int h =
                    u - l +
                    get_start_shift_deconvolve(layer_config->filter_height,
                                               layer_config->skip_height);
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int w =
                      v - m +
                      get_start_shift_deconvolve(layer_config->filter_width,
                                                 layer_config->skip_width);
                  if ((h % layer_config->skip_height) != 0 ||
                      (w % layer_config->skip_width) != 0)
                    continue;
                  const int ii = h / layer_config->skip_height;
                  const int jj = w / layer_config->skip_width;
                  if (ii < 0 || ii >= in_height || jj < 0 || jj >= in_width)
                    continue;
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            output[i][u * out_stride + v] = sum;
          }
        }
      }
      break;
    case PADDING_SAME_REPLICATE:
      for (int i = 0; i < layer_config->out_channels; ++i) {
        for (int u = 0; u < out_height; ++u) {
          for (int v = 0; v < out_width; ++v) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int h =
                    u - l +
                    get_start_shift_deconvolve(layer_config->filter_height,
                                               layer_config->skip_height);
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int w =
                      v - m +
                      get_start_shift_deconvolve(layer_config->filter_width,
                                                 layer_config->skip_width);
                  if ((h % layer_config->skip_height) != 0 ||
                      (w % layer_config->skip_width) != 0)
                    continue;
                  const int ii =
                      CLAMPINDEX(h / layer_config->skip_height, in_height);
                  const int jj =
                      CLAMPINDEX(w / layer_config->skip_width, in_width);
                  assert(ii >= 0 && ii < in_height && jj >= 0 && jj < in_width);
                  continue;
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            output[i][u * out_stride + v] = sum;
          }
        }
      }
      break;
    case PADDING_VALID:
      for (int i = 0; i < layer_config->out_channels; ++i) {
        for (int u = 0; u < out_height; ++u) {
          for (int v = 0; v < out_width; ++v) {
            float sum = layer_config->bias[i];
            for (int k = 0; k < layer_config->in_channels; ++k) {
              int off = k * layer_config->out_channels + i;
              for (int l = 0; l < layer_config->filter_height; ++l) {
                const int h = u - l;
                for (int m = 0; m < layer_config->filter_width;
                     ++m, off += cstep) {
                  const int w = v - m;
                  if ((h % layer_config->skip_height) != 0 ||
                      (w % layer_config->skip_width) != 0)
                    continue;
                  const int ii = h / layer_config->skip_height;
                  const int jj = w / layer_config->skip_width;
                  if (ii < 0 || ii >= in_height || jj < 0 || jj >= in_width)
                    continue;
                  sum += layer_config->weights[off] *
                         input[k][ii * in_stride + jj];
                }
              }
            }
            output[i][u * out_stride + v] = sum;
          }
        }
      }
      break;
    default: assert(0 && "Unknown padding type");
  }
}

void av1_cnn_predict_c(const float **input, int in_width, int in_height,
                       int in_stride, const CNN_CONFIG *cnn_config,
                       float **output, int out_stride) {
  TENSOR tensor1[CNN_MAX_BRANCHES] = { 0 };
  TENSOR tensor2[CNN_MAX_BRANCHES] = { 0 };

  int i_width, i_height;
  int o_width = 0, o_height = 0;
  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    init_tensor(&tensor1[b]);
    init_tensor(&tensor2[b]);
  }

  for (int layer = 0; layer < cnn_config->num_layers; ++layer) {
    const int branch = cnn_config->layer_config[layer].branch;
    if (layer == 0) {       // First layer
      assert(branch == 0);  // First layer must be primary branch
      assign_tensor(&tensor1[branch], input,
                    cnn_config->layer_config[layer].in_channels, in_width,
                    in_height, in_stride);
      find_layer_output_size(in_width, in_height,
                             &cnn_config->layer_config[layer], &o_width,
                             &o_height);
      if (cnn_config->num_layers == 1) {  // single layer case
        assign_tensor(&tensor2[branch], (const float **)output,
                      cnn_config->layer_config[layer].out_channels, o_width,
                      o_height, out_stride);
      } else {  // more than one layer case
        realloc_tensor(&tensor2[branch],
                       cnn_config->layer_config[layer].out_channels, o_width,
                       o_height);
      }
    } else {  // Non-first layer
      // Swap tensor1 and tensor2
      swap_tensor(&tensor1[branch], &tensor2[branch]);

      i_width = o_width;
      i_height = o_height;
      find_layer_output_size(i_width, i_height,
                             &cnn_config->layer_config[layer], &o_width,
                             &o_height);
      if (layer < cnn_config->num_layers - 1) {  // Non-last layer
        realloc_tensor(&tensor2[branch],
                       cnn_config->layer_config[layer].out_channels, o_width,
                       o_height);
      } else {                // Last layer
        assert(branch == 0);  // Last layer must be primary branch
        free_tensor(&tensor2[branch]);
        assign_tensor(&tensor2[branch], (const float **)output,
                      cnn_config->layer_config[layer].out_channels, o_width,
                      o_height, out_stride);
      }
    }

    // If we are combining branches make sure that the branch to combine
    // is different from the current branch.
    assert(IMPLIES(
        cnn_config->layer_config[layer].branch_combine_type != BRANCH_NOC,
        !(cnn_config->layer_config[layer].branches_to_combine &
          (1 << branch))));

    if (cnn_config->layer_config[layer].branch_copy_mode == COPY_INPUT) {
      copy_active_tensor_to_branches(
          &tensor1[branch], &cnn_config->layer_config[layer], branch, tensor2);
    }
    // Check consistency of input and output channels
    assert(tensor1[branch].channels ==
           cnn_config->layer_config[layer].in_channels);
    assert(tensor2[branch].channels ==
           cnn_config->layer_config[layer].out_channels);

    // Convolve/Deconvolve
    if (!cnn_config->layer_config[layer].deconvolve) {
      av1_cnn_convolve((const float **)tensor1[branch].buf,
                       tensor1[branch].width, tensor1[branch].height,
                       tensor1[branch].stride, &cnn_config->layer_config[layer],
                       tensor2[branch].buf, tensor2[branch].stride);
    } else {
      av1_cnn_deconvolve((const float **)tensor1[branch].buf,
                         tensor1[branch].width, tensor1[branch].height,
                         tensor1[branch].stride,
                         &cnn_config->layer_config[layer], tensor2[branch].buf,
                         tensor2[branch].stride);
    }

    if (cnn_config->layer_config[layer].branch_copy_mode == COPY_OUTPUT) {
      copy_active_tensor_to_branches(
          &tensor2[branch], &cnn_config->layer_config[layer], branch, tensor2);
    }

    // Add tensors from other branches if needed
    if (cnn_config->layer_config[layer].branch_combine_type == BRANCH_ADD) {
      for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
        if ((cnn_config->layer_config[layer].branches_to_combine & (1 << b)) &&
            b != branch) {
          assert(check_tensor_equal_size(&tensor2[b], &tensor2[branch]));
          av1_cnn_add(tensor2[branch].buf, tensor2[branch].channels,
                      tensor2[branch].width, tensor2[branch].height,
                      tensor2[branch].stride, (const float **)tensor2[b].buf);
        }
      }
    }

    // Non-linearity
    if (cnn_config->layer_config[layer].activation != IDENTITY)
      av1_cnn_activate(tensor2[branch].buf, tensor2[branch].channels,
                       tensor2[branch].width, tensor2[branch].height,
                       tensor2[branch].stride,
                       cnn_config->layer_config[layer].activation);

    // Concatenate tensors
    if (cnn_config->layer_config[layer].branch_combine_type == BRANCH_CAT) {
      for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
        if ((cnn_config->layer_config[layer].branches_to_combine & (1 << b)) &&
            b != branch) {
          assert(check_tensor_equal_dims(&tensor2[b], &tensor2[branch]));
          concat_tensor(&tensor2[b], &tensor2[branch]);
        }
      }
    }

    if (cnn_config->layer_config[layer].branch_copy_mode == COPY_COMBINED) {
      copy_active_tensor_to_branches(
          &tensor2[branch], &cnn_config->layer_config[layer], branch, tensor2);
    }
  }

  for (int b = 0; b < CNN_MAX_BRANCHES; ++b) {
    free_tensor(&tensor1[b]);
    free_tensor(&tensor2[b]);
  }
}

// Assume output already has proper allocation
// Assume input image buffers all have same resolution and strides
void av1_cnn_predict_img(uint8_t **dgd, int width, int height, int stride,
                         const CNN_CONFIG *cnn_config, float **output,
                         int out_stride) {
  const float max_val = 255.0;
  int out_width = 0;
  int out_height = 0;
  av1_find_cnn_output_size(width, height, cnn_config, &out_width, &out_height);

  int in_width = width + 2 * cnn_config->ext_width;
  int in_height = height + 2 * cnn_config->ext_height;
  int in_channels = cnn_config->layer_config[0].in_channels;
  float *inputs[CNN_MAX_CHANNELS];
  float *input_ =
      (float *)aom_malloc(in_width * in_height * in_channels * sizeof(*input_));
  const int in_stride = in_width;

  for (int c = 0; c < in_channels; ++c) {
    inputs[c] = input_ + c * in_stride * in_height;
    float *input =
        inputs[c] + cnn_config->ext_height * in_stride + cnn_config->ext_width;

    if (cnn_config->strict_bounds) {
      for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
      // extend left and right
      for (int i = 0; i < height; ++i) {
        for (int j = -cnn_config->ext_width; j < 0; ++j)
          input[i * in_stride + j] = input[i * in_stride];
        for (int j = width; j < width + cnn_config->ext_width; ++j)
          input[i * in_stride + j] = input[i * in_stride + width - 1];
      }
      // extend top and bottom
      for (int i = -cnn_config->ext_height; i < 0; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[-cnn_config->ext_width], in_width * sizeof(*input));
      for (int i = height; i < height + cnn_config->ext_height; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[(height - 1) * in_stride - cnn_config->ext_width],
               in_width * sizeof(*input));
    } else {
      for (int i = -cnn_config->ext_height; i < height + cnn_config->ext_height;
           ++i)
        for (int j = -cnn_config->ext_width; j < width + cnn_config->ext_width;
             ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
    }
  }
  av1_cnn_predict((const float **)inputs, in_width, in_height, in_stride,
                  cnn_config, output, out_stride);
  aom_free(input_);
}

// Assume output already has proper allocation
// Assume input image buffers all have same resolution and strides
void av1_cnn_predict_img_highbd(uint16_t **dgd, int width, int height,
                                int stride, const CNN_CONFIG *cnn_config,
                                int bit_depth, float **output, int out_stride) {
  const float max_val = (float)((1 << bit_depth) - 1);
  int out_width = 0;
  int out_height = 0;
  av1_find_cnn_output_size(width, height, cnn_config, &out_width, &out_height);

  int in_width = width + 2 * cnn_config->ext_width;
  int in_height = height + 2 * cnn_config->ext_height;
  int in_channels = cnn_config->layer_config[0].in_channels;
  float *inputs[CNN_MAX_CHANNELS];
  float *input_ =
      (float *)aom_malloc(in_width * in_height * in_channels * sizeof(*input_));
  const int in_stride = in_width;

  for (int c = 0; c < in_channels; ++c) {
    inputs[c] = input_ + c * in_stride * in_height;
    float *input =
        inputs[c] + cnn_config->ext_height * in_stride + cnn_config->ext_width;

    if (cnn_config->strict_bounds) {
      for (int i = 0; i < height; ++i)
        for (int j = 0; j < width; ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
      // extend left and right
      for (int i = 0; i < height; ++i) {
        for (int j = -cnn_config->ext_width; j < 0; ++j)
          input[i * in_stride + j] = input[i * in_stride];
        for (int j = width; j < width + cnn_config->ext_width; ++j)
          input[i * in_stride + j] = input[i * in_stride + width - 1];
      }
      // extend top and bottom
      for (int i = -cnn_config->ext_height; i < 0; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[-cnn_config->ext_width], in_width * sizeof(*input));
      for (int i = height; i < height + cnn_config->ext_height; ++i)
        memcpy(&input[i * in_stride - cnn_config->ext_width],
               &input[(height - 1) * in_stride - cnn_config->ext_width],
               in_width * sizeof(*input));
    } else {
      for (int i = -cnn_config->ext_height; i < height + cnn_config->ext_height;
           ++i)
        for (int j = -cnn_config->ext_width; j < width + cnn_config->ext_width;
             ++j)
          input[i * in_stride + j] = (float)dgd[c][i * stride + j] / max_val;
    }
  }
  av1_cnn_predict((const float **)inputs, width, height, in_stride, cnn_config,
                  output, out_stride);
  aom_free(input_);
}

void av1_restore_cnn_img(uint8_t *dgd, int width, int height, int stride,
                         const CNN_CONFIG *cnn_config) {
  const float max_val = 255;
  int out_width = 0;
  int out_height = 0;
  av1_find_cnn_output_size(width, height, cnn_config, &out_width, &out_height);
  assert(out_width == width);
  assert(out_height == height);

  const int out_stride = width;
  float *output = (float *)aom_malloc(width * height * sizeof(*output));
  av1_cnn_predict_img(&dgd, width, height, stride, cnn_config, &output,
                      out_stride);

  if (cnn_config->is_residue) {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j) {
        const int residue = (int)(output[i * out_stride + j] * max_val + 0.5);
        dgd[i * stride + j] = clip_pixel(dgd[i * stride + j] + residue);
      }
  } else {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j)
        dgd[i * stride + j] =
            clip_pixel((int)(output[i * out_stride + j] * max_val + 0.5));
  }
  aom_free(output);
}

void av1_restore_cnn_img_highbd(uint16_t *dgd, int width, int height,
                                int stride, const CNN_CONFIG *cnn_config,
                                int bit_depth) {
  const float max_val = (float)((1 << bit_depth) - 1);
  int out_width = 0;
  int out_height = 0;
  av1_find_cnn_output_size(width, height, cnn_config, &out_width, &out_height);
  assert(out_width == width);
  assert(out_height == height);

  float *output = (float *)aom_malloc(width * height * sizeof(*output));
  const int out_stride = width;
  av1_cnn_predict_img_highbd(&dgd, width, height, stride, cnn_config, bit_depth,
                             &output, out_stride);

  if (cnn_config->is_residue) {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j) {
        const int residue = (int)(output[i * out_stride + j] * max_val + 0.5);
        dgd[i * stride + j] +=
            clip_pixel_highbd(dgd[i * stride + j] + residue, bit_depth);
      }
  } else {
    for (int i = 0; i < height; ++i)
      for (int j = 0; j < width; ++j)
        dgd[i * stride + j] = clip_pixel_highbd(
            (int)(output[i * out_stride + j] * max_val + 0.5), bit_depth);
  }
  aom_free(output);
}

void av1_restore_cnn_plane_part(AV1_COMMON *cm, const CNN_CONFIG *cnn_config,
                                int plane, int start_x, int start_y, int width,
                                int height) {
  YV12_BUFFER_CONFIG *buf = &cm->cur_frame->buf;

  assert(start_x >= 0 && start_x + width <= buf->y_crop_width);
  assert(start_y >= 0 && start_y + height <= buf->y_crop_height);

  int offset = 0, part_width = 0, part_height = 0;
  switch (plane) {
    case AOM_PLANE_Y:
      part_width = width;
      part_height = height;
      offset = start_y * buf->y_stride + start_x;
      break;
    case AOM_PLANE_U:
    case AOM_PLANE_V:
      part_width = width >> buf->subsampling_x;
      part_height = height >> buf->subsampling_y;
      offset = (start_y >> buf->subsampling_y) * buf->uv_stride +
               (start_x >> buf->subsampling_x);
      break;
    default: assert(0 && "Invalid plane index");
  }
  if (cm->seq_params.use_highbitdepth) {
    switch (plane) {
      case AOM_PLANE_Y:
        av1_restore_cnn_img_highbd(CONVERT_TO_SHORTPTR(buf->y_buffer + offset),
                                   part_width, part_height, buf->y_stride,
                                   cnn_config, cm->seq_params.bit_depth);
        break;
      case AOM_PLANE_U:
        av1_restore_cnn_img_highbd(CONVERT_TO_SHORTPTR(buf->u_buffer + offset),
                                   part_width, part_height, buf->uv_stride,
                                   cnn_config, cm->seq_params.bit_depth);
        break;
      case AOM_PLANE_V:
        av1_restore_cnn_img_highbd(CONVERT_TO_SHORTPTR(buf->v_buffer + offset),
                                   part_width, part_height, buf->uv_stride,
                                   cnn_config, cm->seq_params.bit_depth);
        break;
      default: assert(0 && "Invalid plane index");
    }
  } else {
    assert(cm->seq_params.bit_depth == 8);
    switch (plane) {
      case AOM_PLANE_Y:
        av1_restore_cnn_img(buf->y_buffer + offset, part_width, part_height,
                            buf->y_stride, cnn_config);
        break;
      case AOM_PLANE_U:
        av1_restore_cnn_img(buf->u_buffer + offset, part_width, part_height,
                            buf->uv_stride, cnn_config);
        break;
      case AOM_PLANE_V:
        av1_restore_cnn_img(buf->v_buffer + offset, part_width, part_height,
                            buf->uv_stride, cnn_config);
        break;
      default: assert(0 && "Invalid plane index");
    }
  }
}

void av1_restore_cnn_plane(AV1_COMMON *cm, const CNN_CONFIG *cnn_config,
                           int plane) {
  YV12_BUFFER_CONFIG *buf = &cm->cur_frame->buf;
  if (cm->seq_params.use_highbitdepth) {
    switch (plane) {
      case AOM_PLANE_Y:
        av1_restore_cnn_img_highbd(CONVERT_TO_SHORTPTR(buf->y_buffer),
                                   buf->y_crop_width, buf->y_crop_height,
                                   buf->y_stride, cnn_config,
                                   cm->seq_params.bit_depth);
        break;
      case AOM_PLANE_U:
        av1_restore_cnn_img_highbd(CONVERT_TO_SHORTPTR(buf->u_buffer),
                                   buf->uv_crop_width, buf->uv_crop_height,
                                   buf->uv_stride, cnn_config,
                                   cm->seq_params.bit_depth);
        break;
      case AOM_PLANE_V:
        av1_restore_cnn_img_highbd(CONVERT_TO_SHORTPTR(buf->v_buffer),
                                   buf->uv_crop_width, buf->uv_crop_height,
                                   buf->uv_stride, cnn_config,
                                   cm->seq_params.bit_depth);
        break;
      default: assert(0 && "Invalid plane index");
    }
  } else {
    assert(cm->seq_params.bit_depth == 8);
    switch (plane) {
      case AOM_PLANE_Y:
        av1_restore_cnn_img(buf->y_buffer, buf->y_crop_width,
                            buf->y_crop_height, buf->y_stride, cnn_config);
        break;
      case AOM_PLANE_U:
        av1_restore_cnn_img(buf->u_buffer, buf->uv_crop_width,
                            buf->uv_crop_height, buf->uv_stride, cnn_config);
        break;
      case AOM_PLANE_V:
        av1_restore_cnn_img(buf->v_buffer, buf->uv_crop_width,
                            buf->uv_crop_height, buf->uv_stride, cnn_config);
        break;
      default: assert(0 && "Invalid plane index");
    }
  }
}
