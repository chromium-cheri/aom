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

#include "aom_mem/aom_mem.h"

#include "av1/common/reconinter.h"
#include "av1/common/scan.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/seg_common.h"
#include "av1/common/txb_common.h"

#if CONFIG_INTRA_ENTROPY
// Applies the ReLu activation to one fc layer
// output[i] = Max(input[i],0.0f)
static float *nn_relu(const float *input, FC_LAYER_EM *layer) {
  for (int i = 0; i < layer->num_outputs; ++i) {
    layer->output[i] = AOMMAX(input[i], 0.0f);
  }

  return layer->output;
}

static int *qnn_relu(const int *input, QFC_LAYER_EM *layer) {
  for (int i = 0; i < layer->num_outputs; ++i) {
    layer->output[i] = AOMMAX(input[i], 0);
  }

  return layer->output;
}

// Applies the Sigmoid activation to one fc layer
// output[i] = 1/(1+exp(input[i]))
static float *nn_sigmoid(const float *input, FC_LAYER_EM *layer) {
  for (int i = 0; i < layer->num_outputs; ++i) {
    const float tmp = AOMMIN(AOMMAX(input[i], -10.0f), 10.0f);
    layer->output[i] = 1.0f / (1.0f + expf(-tmp));
  }

  return layer->output;
}

// Forward prediction in one fc layer, used in function av1_nn_predict_V2
static float *nn_fc_forward(const float *input, FC_LAYER_EM *layer) {
  const float *weights = layer->weights;
  const float *bias = layer->bias;
  assert(layer->num_outputs < EM_MAX_NODES);
  // fc
  for (int node = 0; node < layer->num_outputs; ++node) {
    float val = bias[node];
    for (int i = 0; i < layer->num_inputs; ++i) val += weights[i] * input[i];
    layer->output[node] = val;
    weights += layer->num_inputs;
  }

  // activation
  switch (layer->activation) {
    case ACTN_NONE: return layer->output;
    case ACTN_RELU: return nn_relu(layer->output, layer);
    case ACTN_SIGMOID: return nn_sigmoid(layer->output, layer);
    default:
      assert(0 && "Unknown activation");  // Unknown activation
      return NULL;
  }
}

static int *qnn_fc_forward(const int *input, QFC_LAYER_EM *layer, int scale) {
  const int *weights = layer->weights;
  const int *bias = layer->bias;
  assert(layer->num_outputs < EM_MAX_NODES);
  // fc
  for (int node = 0; node < layer->num_outputs; ++node) {
    int val = bias[node];
    for (int i = 0; i < layer->num_inputs; ++i) {
      val += ((weights[i] * input[i]) >> scale);
    }
    layer->output[node] = val;
    weights += layer->num_inputs;
  }

  // activation
  switch (layer->activation) {
    case ACTN_NONE: return layer->output;
    case ACTN_RELU: return qnn_relu(layer->output, layer);
    case ACTN_SIGMOID: assert(0 && "Unknown activation");  // TODO
    default:
      assert(0 && "Unknown activation");  // Unknown activation
      return NULL;
  }
}

void av1_nn_predict_em(const float *feature, NN_CONFIG_EM *nn_config,
                       float *output) {
  const float *input_nodes = feature;
  const int num_layers = nn_config->num_hidden_layers;
  assert(num_layers <= EM_MAX_HLAYERS);
  // Copy the input feature to the buffer
  av1_copy_array(nn_config->feature, feature, nn_config->layer[0].num_inputs);

  // Propagate the layers.
  int num_inputs = nn_config->layer[0].num_inputs;
  for (int i = 0; i <= num_layers; ++i) {
    assert(num_inputs == nn_config->layer[i].num_inputs);
    input_nodes = nn_fc_forward(input_nodes, nn_config->layer + i);
    num_inputs = nn_config->layer[i].num_outputs;
  }
  (void)num_inputs;

  // Final layer
  assert(nn_config->layer[num_layers].num_outputs == nn_config->num_logits);
  switch (nn_config->loss) {
    case SOFTMAX_CROSS_ENTROPY_LOSS:
      if (nn_config->num_logits == 1) {
        // sigmoid
        const float tmp = AOMMIN(AOMMAX(input_nodes[0], -10.0f), 10.0f);
        nn_config->output[0] = 1.0f / (1.0f + expf(-tmp));
      } else {
        // softmax
        av1_nn_softmax_em(input_nodes, nn_config->output,
                          nn_config->num_logits);
      }
      break;
    default:
      av1_copy_array(nn_config->output, input_nodes, nn_config->num_logits);
  }
  // Copy the final layer output
  av1_copy_array(output, nn_config->output, nn_config->num_logits);
}

void av1_qnn_predict_em(const int *feature, QNN_CONFIG_EM *nn_config,
                        float *output) {
  const int *input_nodes = feature;
  const int num_layers = nn_config->num_hidden_layers;
  assert(num_layers <= EM_MAX_HLAYERS);
  // Copy the input feature to the buffer
  av1_copy_array(nn_config->feature, feature, nn_config->layer[0].num_inputs);

  // Propagate the layers.
  int num_inputs = nn_config->layer[0].num_inputs;
  for (int i = 0; i <= num_layers; ++i) {
    assert(num_inputs == nn_config->layer[i].num_inputs);
    input_nodes =
        qnn_fc_forward(input_nodes, nn_config->layer + i, nn_config->scale);
    num_inputs = nn_config->layer[i].num_outputs;
  }
  (void)num_inputs;

  // Final layer
  assert(nn_config->layer[num_layers].num_outputs == nn_config->num_logits);
  float fscale = nn_config->fscale;
  switch (nn_config->loss) {
    case SOFTMAX_CROSS_ENTROPY_LOSS:
      if (nn_config->num_logits == 1) {
        // sigmoid
        float tmp = (float)input_nodes[0] / fscale;
        tmp = AOMMIN(AOMMAX(tmp, -10.0f), 10.0f);
        nn_config->output[0] = 1.0f / (1.0f + expf(-tmp));
      } else {
        // softmax
        for (int i = 0; i < nn_config->num_logits; ++i) {
          nn_config->output[i] = (float)input_nodes[i] / fscale;
        }
        av1_nn_softmax_em(nn_config->output, nn_config->output,
                          nn_config->num_logits);
      }
      break;
    default:
      for (int i = 0; i < nn_config->num_logits; ++i) {
        nn_config->output[i] = (float)input_nodes[i] / fscale;
      }
  }
  // Copy the final layer output
  av1_copy_array(output, nn_config->output, nn_config->num_logits);
}

/***************************Backprop for gradient******************************/
// Backprop for ReLU activation
static void nn_relu_back(float *dX_out, FC_LAYER_EM *layer) {
  const float *dY = layer->dY;
  for (int i = 0; i < layer->num_outputs; ++i)
    dX_out[i] = layer->output[i] > 0.0f ? dY[i] : 0.0f;
}

static void qnn_relu_back(int *dX_out, QFC_LAYER_EM *layer) {
  const int *dY = layer->dY;
  for (int i = 0; i < layer->num_outputs; ++i)
    dX_out[i] = layer->output[i] > 0 ? dY[i] : 0;
}

// Backprop for sigmoid activation
static void nn_sigmoid_back(float *dX_out, FC_LAYER_EM *layer) {
  const float *dY = layer->dY;
  for (int i = 0; i < layer->num_outputs; ++i)
    dX_out[i] =
        dY[i] * layer->output[i] * (1 - layer->output[i]);  // dX=dY*sigmoid(X)
}

// Backprop for softmax cross entropy loss
static void nn_softmax_cross_entropy_loss_back(float *dX_out,
                                               const float *output,
                                               const int num_outputs,
                                               const int label) {
  if (num_outputs == 1) {
    // sigmoid
    assert(label < 2);  // label [0,1]
    dX_out[0] = output[0] - (float)label;
  } else {
    // softmax
    assert(num_outputs > label);  // label [0,1,... num_logits-1]
    av1_copy_array(dX_out, output, num_outputs);
    dX_out[label] -= 1;
  }
}

static void qnn_softmax_cross_entropy_loss_back(int *dX_out,
                                                const float *output,
                                                const int num_outputs,
                                                const int label, int scale) {
  float fscale = 1 << scale;
  if (num_outputs == 1) {
    // sigmoid
    assert(label < 2);  // label [0,1]
    dX_out[0] = (int)(output[0] * fscale) - (label << scale);
  } else {
    // softmax
    assert(num_outputs > label);  // label [0,1,... num_logits-1]
    for (int i = 0; i < num_outputs; ++i) {
      dX_out[i] = (int)(output[i] * fscale);
    }
    dX_out[label] -= (1 << scale);
  }
}

// Backprop in one fc layer, used in function av1_nn_backprop
static void nn_fc_backward(const float *X, float *dX_out, FC_LAYER_EM *layer) {
  // backprop on activation
  float dY_fc[EM_MAX_NODES] = { 0 };  // dY for fc
  switch (layer->activation) {
    case ACTN_NONE:  // no activation, dY_fc <-- dY
      av1_copy_array(dY_fc, layer->dY, layer->num_outputs);
      break;
    case ACTN_RELU: nn_relu_back(dY_fc, layer); break;
    case ACTN_SIGMOID: nn_sigmoid_back(dY_fc, layer); break;
    default: assert(0 && "Unknown activation");  // Unknown activation
  }

  // backprop on fc
  // gradient of W, b
  float *dW = layer->dW;
  float *db = layer->db;
  for (int j = 0; j < layer->num_outputs; ++j) {
    for (int i = 0; i < layer->num_inputs; ++i) dW[i] += dY_fc[j] * X[i];
    db[j] += dY_fc[j];
    dW += layer->num_inputs;
  }

  // gradient of the input, i.e., the output of last layer
  if (dX_out) {
    for (int i = 0; i < layer->num_inputs; ++i) {
      float *w = layer->weights + i;
      float val = 0.0f;
      for (int j = 0; j < layer->num_outputs; ++j) {
        val += dY_fc[j] * w[j * layer->num_inputs];
      }
      dX_out[i] = val;
    }
  }
}

static void qnn_fc_backward(const int *X, int *dX_out, QFC_LAYER_EM *layer,
                            int scale) {
  // backprop on activation
  int dY_fc[EM_MAX_NODES] = { 0 };  // dY for fc
  switch (layer->activation) {
    case ACTN_NONE:  // no activation, dY_fc <-- dY
      av1_copy_array(dY_fc, layer->dY, layer->num_outputs);
      break;
    case ACTN_RELU: qnn_relu_back(dY_fc, layer); break;
    case ACTN_SIGMOID: assert(0 && "Unknown activation"); break;  // TODO
    default: assert(0 && "Unknown activation");  // Unknown activation
  }

  // backprop on fc
  // gradient of W, b
  int *dW = layer->dW;
  int *db = layer->db;
  for (int j = 0; j < layer->num_outputs; ++j) {
    for (int i = 0; i < layer->num_inputs; ++i) {
      dW[i] += ((dY_fc[j] * X[i]) >> scale);
    }
    db[j] += dY_fc[j];
    dW += layer->num_inputs;
  }

  // gradient of the input, i.e., the output of last layer
  if (dX_out) {
    for (int i = 0; i < layer->num_inputs; ++i) {
      int *w = layer->weights + i;
      int val = 0;
      for (int j = 0; j < layer->num_outputs; ++j) {
        val += ((dY_fc[j] * w[j * layer->num_inputs]) >> scale);
      }
      dX_out[i] = val;
    }
  }
}

void av1_nn_backprop_em(NN_CONFIG_EM *nn_config, const int label) {
  const int num_layers = nn_config->num_hidden_layers;

  // loss layer
  switch (nn_config->loss) {
    case SOFTMAX_CROSS_ENTROPY_LOSS:
      nn_softmax_cross_entropy_loss_back(nn_config->layer[num_layers].dY,
                                         nn_config->output,
                                         nn_config->num_logits, label);
      break;
    default: assert(0 && "Unknown loss");  // Unknown loss
  }

  // hidden fc layer
  FC_LAYER_EM *layer_ptr = nn_config->layer + num_layers;
  for (int i = 0; i < num_layers; ++i) {
    nn_fc_backward(layer_ptr[-1].output, layer_ptr[-1].dY, layer_ptr);
    layer_ptr -= 1;
  }

  nn_fc_backward(nn_config->feature, NULL,
                 layer_ptr);  // first layer (no dX for feature)
}

void av1_qnn_backprop_em(QNN_CONFIG_EM *nn_config, const int label) {
  const int num_layers = nn_config->num_hidden_layers;
  int scale = nn_config->scale;

  // loss layer
  switch (nn_config->loss) {
    case SOFTMAX_CROSS_ENTROPY_LOSS:
      qnn_softmax_cross_entropy_loss_back(nn_config->layer[num_layers].dY,
                                          nn_config->output,
                                          nn_config->num_logits, label, scale);
      break;
    default: assert(0 && "Unknown loss");  // Unknown loss
  }

  // hidden fc layer
  QFC_LAYER_EM *layer_ptr = nn_config->layer + num_layers;
  for (int i = 0; i < num_layers; ++i) {
    qnn_fc_backward(layer_ptr[-1].output, layer_ptr[-1].dY, layer_ptr, scale);
    layer_ptr -= 1;
  }

  qnn_fc_backward(nn_config->feature, NULL, layer_ptr,
                  scale);  // first layer (no dX for feature)
}

void av1_nn_update_em(NN_CONFIG_EM *nn_config, float lr) {
  const int num_layers = nn_config->num_hidden_layers;

  // Update the weights
  for (int i = 0; i <= num_layers; ++i) {
    FC_LAYER_EM *layer = nn_config->layer + i;
    for (int j = 0; j < layer->num_inputs * layer->num_outputs; ++j) {
      layer->weights[j] -= (lr * layer->dW[j]);
      layer->dW[j] = 0.f;
    }
    for (int j = 0; j < layer->num_outputs; ++j) {
      layer->bias[j] -= (lr * layer->db[j]);
      layer->db[j] = 0.f;
    }
  }
}

void av1_qnn_update_em(QNN_CONFIG_EM *nn_config, int inv_lr) {
  const int num_layers = nn_config->num_hidden_layers;

  // Update the weights
  for (int i = 0; i <= num_layers; ++i) {
    QFC_LAYER_EM *layer = nn_config->layer + i;
    for (int j = 0; j < layer->num_inputs * layer->num_outputs; ++j) {
      layer->weights[j] -= (layer->dW[j] / inv_lr);
      layer->dW[j] = 0;
    }
    for (int j = 0; j < layer->num_outputs; ++j) {
      layer->bias[j] -= (layer->db[j] / inv_lr);
      layer->db[j] = 0;
    }
  }
}

void av1_nn_softmax_em(const float *input, float *output, int n) {
  // Softmax function is invariant to adding the same constant
  // to all input values, so we subtract the maximum input to avoid
  // possible overflow.
  float max_inp = input[0];
  for (int i = 1; i < n; i++) max_inp = AOMMAX(max_inp, input[i]);
  float sum_out = 0.0f;
  for (int i = 0; i < n; i++) {
    // Clamp to range [-10.0, 0.0] to prevent FE_UNDERFLOW errors.
    const float normalized_input = AOMMAX(input[i] - max_inp, -10.0f);
    output[i] = expf(normalized_input);
    sum_out += output[i];
  }
  for (int i = 0; i < n; i++) output[i] /= sum_out;
}
#endif  // CONFIG_INTRA_ENTROPY

static const aom_cdf_prob
    default_kf_y_mode_cdf[KF_MODE_CONTEXTS][KF_MODE_CONTEXTS][CDF_SIZE(
        INTRA_MODES)] = {
      { { AOM_CDF13(15588, 17027, 19338, 20218, 20682, 21110, 21825, 23244,
                    24189, 28165, 29093, 30466) },
        { AOM_CDF13(12016, 18066, 19516, 20303, 20719, 21444, 21888, 23032,
                    24434, 28658, 30172, 31409) },
        { AOM_CDF13(10052, 10771, 22296, 22788, 23055, 23239, 24133, 25620,
                    26160, 29336, 29929, 31567) },
        { AOM_CDF13(14091, 15406, 16442, 18808, 19136, 19546, 19998, 22096,
                    24746, 29585, 30958, 32462) },
        { AOM_CDF13(12122, 13265, 15603, 16501, 18609, 20033, 22391, 25583,
                    26437, 30261, 31073, 32475) } },
      { { AOM_CDF13(10023, 19585, 20848, 21440, 21832, 22760, 23089, 24023,
                    25381, 29014, 30482, 31436) },
        { AOM_CDF13(5983, 24099, 24560, 24886, 25066, 25795, 25913, 26423,
                    27610, 29905, 31276, 31794) },
        { AOM_CDF13(7444, 12781, 20177, 20728, 21077, 21607, 22170, 23405,
                    24469, 27915, 29090, 30492) },
        { AOM_CDF13(8537, 14689, 15432, 17087, 17408, 18172, 18408, 19825,
                    24649, 29153, 31096, 32210) },
        { AOM_CDF13(7543, 14231, 15496, 16195, 17905, 20717, 21984, 24516,
                    26001, 29675, 30981, 31994) } },
      { { AOM_CDF13(12613, 13591, 21383, 22004, 22312, 22577, 23401, 25055,
                    25729, 29538, 30305, 32077) },
        { AOM_CDF13(9687, 13470, 18506, 19230, 19604, 20147, 20695, 22062,
                    23219, 27743, 29211, 30907) },
        { AOM_CDF13(6183, 6505, 26024, 26252, 26366, 26434, 27082, 28354, 28555,
                    30467, 30794, 32086) },
        { AOM_CDF13(10718, 11734, 14954, 17224, 17565, 17924, 18561, 21523,
                    23878, 28975, 30287, 32252) },
        { AOM_CDF13(9194, 9858, 16501, 17263, 18424, 19171, 21563, 25961, 26561,
                    30072, 30737, 32463) } },
      { { AOM_CDF13(12602, 14399, 15488, 18381, 18778, 19315, 19724, 21419,
                    25060, 29696, 30917, 32409) },
        { AOM_CDF13(8203, 13821, 14524, 17105, 17439, 18131, 18404, 19468,
                    25225, 29485, 31158, 32342) },
        { AOM_CDF13(8451, 9731, 15004, 17643, 18012, 18425, 19070, 21538, 24605,
                    29118, 30078, 32018) },
        { AOM_CDF13(7714, 9048, 9516, 16667, 16817, 16994, 17153, 18767, 26743,
                    30389, 31536, 32528) },
        { AOM_CDF13(8843, 10280, 11496, 15317, 16652, 17943, 19108, 22718,
                    25769, 29953, 30983, 32485) } },
      { { AOM_CDF13(12578, 13671, 15979, 16834, 19075, 20913, 22989, 25449,
                    26219, 30214, 31150, 32477) },
        { AOM_CDF13(9563, 13626, 15080, 15892, 17756, 20863, 22207, 24236,
                    25380, 29653, 31143, 32277) },
        { AOM_CDF13(8356, 8901, 17616, 18256, 19350, 20106, 22598, 25947, 26466,
                    29900, 30523, 32261) },
        { AOM_CDF13(10835, 11815, 13124, 16042, 17018, 18039, 18947, 22753,
                    24615, 29489, 30883, 32482) },
        { AOM_CDF13(7618, 8288, 9859, 10509, 15386, 18657, 22903, 28776, 29180,
                    31355, 31802, 32593) } }
    };

#if CONFIG_INTRA_ENTROPY
#if INTRA_MODEL < 0
float lr = 0.1f;
int inv_lr = 10;
#else
float lr = 0.01f;
#endif  // INTRA_MODEL

#if INTRA_MODEL == -1
#if QNN
static const int intra_y_mode_qlayer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  9,     4,     -73,   433,   489,   586,   383,   433,   172,   297,   264,
  436,   411,   547,   96,    288,   316,   339,   192,   220,   205,   294,
  202,   510,   -306,  -274,  9,     186,   -3,    159,   39,    46,    179,
  92,    15,    202,   -37,   12,    26,    14,    -64,   -153,  -950,  66,
  1280,  195,   -1027, -1423, -1035, -1378, -295,  270,   1017,  688,   -123,
  -859,  -1235, -913,  79,    -28,   101,   333,   -76,   -431,  203,   -138,
  610,   -59,   -398,  -320,  346,   -431,  -256,  -464,  -33,   260,   -369,
  -180,  -163,  -47,   -127,  153,   -122,  -92,   -454,  -866,  -1298, -723,
  -114,  605,   1000,  141,   -939,  -1173, -849,  -1223, -1180, -585,  1176,
  -260,  121,   31,    277,   -213,  -282,  80,    180,   17,    -286,  494,
  -58,   -239,  -456,  157,   -503,  -470,  -129,  -284,  537,   -288,  -320,
  -114,  95,    -116,  -46,   -334,  1801,  311,   -1348, -1665, -1713, -1532,
  -1126, 999,   1275,  475,   -729,  -693,  -766,  -848,  -1216, 272,   -104,
  -165,  -157,  -491,  -524,  -543,  -269,  22,    142,   108,   -474,  -57,
  -160,  -74,   435,   -139,  -254,  -193,  -395,  159,   -225,  22,    34,
  12,    -168,  -7,    -1119, -927,  -749,  991,   1668,  892,   -643,  -1610,
  -1081, -820,  -699,  453,   1511,  1403,  -765,  -1539, -773,  -442,  -540,
  401,   1058,  710,   -558,  -1286, -635,  -50,   635,   -215,  -453,  -295,
  -17,   600,   -210,  -293,  -445,  -111,  591,   -61,   -27,   -92,   411,
  505,   -1070, -362,  -7,    1745,  610,   -613,  -891,  -1401, -677,  -201,
  6,     1512,  1296,  -179,  -1380, -1487, -353,  -57,   -212,  935,   349,
  -696,  -642,  -921,  -112,  -636,  191,   -249,  -155,  -281,  -162,  526,
  -96,   -160,  -457,  -119,  456,   46,    -54,   89,    228,   358,   -816,
  -1335, -1371, -839,  738,   1705,  88,    -283,  -817,  -900,  -870,  -903,
  -9,    1415,  58,    -408,  -478,  -529,  -619,  -861,  -182,  959,   -178,
  -248,  -99,   412,   510,   -119,  -476,  -185,  -180,  457,   -289,  -386,
  -346,  -202,  350,   -115,  19,    -172,  180,   214,   334,   -894,  -1297,
  -1013, -764,  -490,  -3,    1516,  723,   -712,  -1126, -1689, -1824, -1453,
  -410,  1930,  -78,   -321,  26,    -679,  -619,  -449,  -100,  422,   563,
  790,   176,   -213,  -207,  -98,   -295,  75,    -40,   48,    26,    -27,
  315,   19,    132,   -25,   107,   -30,   827,   1962,  -451,  -1134, -1436,
  -1482, -1332, -731,  509,   1298,  -83,   64,    -247,  -715,  -1293, -868,
  -30,   190,   -158,  -236,  -363,  -562,  -164,  -361,  346,   -47,   -365,
  -60,   12,    -55,   461,   -95,   -209,  -132,  -375,  209,   -178,  89,
  30,    120,   -34,   154,   38,    38,    -397,  72,    24,    2,     -11,
  239,   122,   191,   -117,  112,   95,    12,    -380,  126,   48,    46,
  -70,   -60,   -82,   -65,   -106,  193,   -684,  -686,  -77,   108,   -39,
  115,   109,   -20,   51,    -21,   -110,  186,   -151,  165,   133,   157,
  138,   17,    -49,   339,   115,   -10,   -495,  -728,  -389,  -545,  153,
  496,   96,    137,   -188,  -559,  -707,  -605,  -12,   1,     -82,   -91,
  -326,  -527,  -232,  -329,  -44,   -327,  -29,   -89,   79,    -105,  -104,
  -242,  -23,   86,    -316,  38,    -236,  13,    -31,   71,    -69,   -74,
  -52,   -566,  -1050, -619,  -365,  170,   18,    766,   -73,   -445,  -708,
  -659,  -574,  -116,  6,     536,   -159,  -278,  -357,  -507,  -482,  -116,
  -218,  340,   268,   289,   481,   7,     -334,  137,   -167,  -152,  -104,
  -282,  2,     -103,  -263,  9,     13,    11,    -60,   -163,  -896,  -1059,
  1176,  -882,  -658,  -924,  253,   -1316, -1037, -925,  334,   -835,  -748,
  -909,  881,   -1496, -148,  -430,  169,   -449,  -180,  -471,  378,   -573,
  306,   -103,  -659,  4,     -284,  -334,  -646,  -1117, -41,   -187,  -434,
  -556,  -1070, -626,  -591,  -508,  -616,  -890
};
static const int intra_y_mode_qlayer0_bias[EM_MAX_NODES] = {
  298, 293, 333, -309, -373, -360, -333, -205, -261, -21, -232, -273, -67
};
#else
static const float intra_y_mode_layer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  0.00891136f,  0.00448677f,  -0.07158924f, 0.42295805f,  0.47840416f,
  0.57323265f,  0.37488768f,  0.42292890f,  0.16871572f,  0.29100293f,
  0.25786224f,  0.42624229f,  0.40197086f,  0.53502864f,  0.09411472f,
  0.28190407f,  0.30910286f,  0.33119452f,  0.18766232f,  0.21524280f,
  0.20106503f,  0.28786480f,  0.19817564f,  0.49877715f,  -0.29907933f,
  -0.26842743f, 0.00926891f,  0.18231317f,  -0.00365180f, 0.15566926f,
  0.03832328f,  0.04541985f,  0.17509077f,  0.09036700f,  0.01530945f,
  0.19731523f,  -0.03661079f, 0.01230415f,  0.02558661f,  0.01426749f,
  -0.06326565f, -0.14966016f, -0.92808694f, 0.06505899f,  1.25079894f,
  0.19108222f,  -1.00354040f, -1.39029396f, -1.01075995f, -1.34577286f,
  -0.28859696f, 0.26444209f,  0.99388283f,  0.67274141f,  -0.12017152f,
  -0.83931637f, -1.20682859f, -0.89169800f, 0.07782683f,  -0.02748541f,
  0.09957591f,  0.32540962f,  -0.07495455f, -0.42092732f, 0.19879004f,
  -0.13540675f, 0.59581769f,  -0.05825166f, -0.38870674f, -0.31298757f,
  0.33823910f,  -0.42104796f, -0.25041014f, -0.45334718f, -0.03236195f,
  0.25412852f,  -0.36091277f, -0.17622392f, -0.15965253f, -0.04638092f,
  -0.12465355f, 0.14954859f,  -0.12009545f, -0.09000438f, -0.44411096f,
  -0.84659523f, -1.26845992f, -0.70637035f, -0.11170146f, 0.59109503f,
  0.97700387f,  0.13849409f,  -0.91770697f, -1.14567626f, -0.82962245f,
  -1.19509697f, -1.15313172f, -0.57158291f, 1.14858651f,  -0.25391266f,
  0.11889692f,  0.03112431f,  0.27085546f,  -0.20822668f, -0.27558786f,
  0.07842726f,  0.17647111f,  0.01702438f,  -0.27943605f, 0.48292372f,
  -0.05720560f, -0.23399422f, -0.44568905f, 0.15334238f,  -0.49160022f,
  -0.45928943f, -0.12639602f, -0.27807567f, 0.52452940f,  -0.28139770f,
  -0.31346872f, -0.11156041f, 0.09353821f,  -0.11329396f, -0.04584008f,
  -0.32683906f, 1.75888574f,  0.30382821f,  -1.31643534f, -1.62674809f,
  -1.67331910f, -1.49699688f, -1.10024440f, 0.97597933f,  1.24577427f,
  0.46467781f,  -0.71254092f, -0.67734975f, -0.74827379f, -0.82868284f,
  -1.18758655f, 0.26613745f,  -0.10230985f, -0.16136412f, -0.15357856f,
  -0.47984332f, -0.51216781f, -0.53118801f, -0.26279572f, 0.02191047f,
  0.13925996f,  0.10550470f,  -0.46318066f, -0.05619933f, -0.15693128f,
  -0.07239312f, 0.42512244f,  -0.13626347f, -0.24877925f, -0.18861005f,
  -0.38604006f, 0.15587530f,  -0.22038461f, 0.02195697f,  0.03373314f,
  0.01200556f,  -0.16466641f, -0.00736970f, -1.09353817f, -0.90564680f,
  -0.73194546f, 0.96871060f,  1.62947845f,  0.87127674f,  -0.62825733f,
  -1.57245731f, -1.05655050f, -0.80171710f, -0.68284184f, 0.44260854f,
  1.47572482f,  1.37033415f,  -0.74796695f, -1.50372112f, -0.75535512f,
  -0.43197176f, -0.52757335f, 0.39225155f,  1.03384173f,  0.69354999f,
  -0.54510546f, -1.25629807f, -0.62098336f, -0.04921636f, 0.62028784f,
  -0.21061614f, -0.44308731f, -0.28898138f, -0.01706618f, 0.58670974f,
  -0.20585412f, -0.28633019f, -0.43507910f, -0.10907175f, 0.57740927f,
  -0.06003548f, -0.02695742f, -0.09038537f, 0.40180430f,  0.49412271f,
  -1.04503918f, -0.35388720f, -0.00761194f, 1.70443583f,  0.59648246f,
  -0.59943628f, -0.87077689f, -1.36863339f, -0.66181684f, -0.19700275f,
  0.00616077f,  1.47729158f,  1.26601529f,  -0.17570682f, -1.34770584f,
  -1.45229959f, -0.34494102f, -0.05650169f, -0.20733315f, 0.91386694f,
  0.34135336f,  -0.68063074f, -0.62764406f, -0.90033084f, -0.10994961f,
  -0.62149799f, 0.18665671f,  -0.24317433f, -0.15157509f, -0.27513087f,
  -0.15906759f, 0.51405376f,  -0.09421763f, -0.15704915f, -0.44725236f,
  -0.11711276f, 0.44575822f,  0.04513112f,  -0.05329650f, 0.08727889f,
  0.22298366f,  0.34970418f,  -0.79737407f, -1.30443990f, -1.33970964f,
  -0.81949657f, 0.72167599f,  1.66592979f,  0.08683792f,  -0.27731565f,
  -0.79830402f, -0.87922752f, -0.85023600f, -0.88192397f, -0.00919716f,
  1.38254142f,  0.05734617f,  -0.39929974f, -0.46691939f, -0.51678884f,
  -0.60449910f, -0.84119874f, -0.17838341f, 0.93698096f,  -0.17448738f,
  -0.24229233f, -0.09729295f, 0.40236792f,  0.49833086f,  -0.11681543f,
  -0.46517700f, -0.18129516f, -0.17653221f, 0.44697613f,  -0.28228876f,
  -0.37751621f, -0.33877134f, -0.19739681f, 0.34185058f,  -0.11253671f,
  0.01908785f,  -0.16836387f, 0.17668119f,  0.20929278f,  0.32656804f,
  -0.87369150f, -1.26675344f, -0.98954850f, -0.74650443f, -0.47914591f,
  -0.00385847f, 1.48123252f,  0.70618933f,  -0.69577461f, -1.09998608f,
  -1.64974749f, -1.78171337f, -1.41978884f, -0.40064397f, 1.88534391f,
  -0.07657219f, -0.31360132f, 0.02611069f,  -0.66405582f, -0.60533226f,
  -0.43934107f, -0.09766542f, 0.41263530f,  0.54984570f,  0.77169782f,
  0.17277351f,  -0.20836149f, -0.20279060f, -0.09648295f, -0.28872508f,
  0.07402404f,  -0.03909138f, 0.04766829f,  0.02594596f,  -0.02690133f,
  0.30830768f,  0.01888199f,  0.12932031f,  -0.02527335f, 0.10507181f,
  -0.02946114f, 0.80832261f,  1.91603696f,  -0.44066349f, -1.10779071f,
  -1.40298152f, -1.44743145f, -1.30106485f, -0.71455866f, 0.49803030f,
  1.26803374f,  -0.08106134f, 0.06332952f,  -0.24175675f, -0.69858032f,
  -1.26274371f, -0.84804606f, -0.02951756f, 0.18572608f,  -0.15498766f,
  -0.23099661f, -0.35504794f, -0.54895753f, -0.16081119f, -0.35339540f,
  0.33881110f,  -0.04610762f, -0.35684592f, -0.05904930f, 0.01228989f,
  -0.05446180f, 0.45063204f,  -0.09320518f, -0.20435880f, -0.12928987f,
  -0.36717626f, 0.20475039f,  -0.17422642f, 0.08738855f,  0.02950018f,
  0.11752611f,  -0.03326761f, 0.15107208f,  0.03748794f,  0.03739676f,
  -0.38810992f, 0.07061553f,  0.02366208f,  0.00229787f,  -0.01095525f,
  0.23412189f,  0.11990335f,  0.18721098f,  -0.11514752f, 0.10980922f,
  0.09339395f,  0.01266348f,  -0.37124008f, 0.12340029f,  0.04763960f,
  0.04555776f,  -0.06910342f, -0.05931063f, -0.08100924f, -0.06407562f,
  -0.10359126f, 0.18904111f,  -0.66891015f, -0.67057478f, -0.07566668f,
  0.10617658f,  -0.03812894f, 0.11311945f,  0.10700525f,  -0.01970005f,
  0.04980572f,  -0.02132299f, -0.10830808f, 0.18196918f,  -0.14820726f,
  0.16177922f,  0.13042317f,  0.15396141f,  0.13491498f,  0.01706358f,
  -0.04807405f, 0.33110505f,  0.11316054f,  -0.01017215f, -0.48371330f,
  -0.71095765f, -0.38056061f, -0.53283566f, 0.14959827f,  0.48459339f,
  0.09414775f,  0.13434352f,  -0.18408643f, -0.54670203f, -0.69068992f,
  -0.59142393f, -0.01211883f, 0.00136429f,  -0.08098315f, -0.08903217f,
  -0.31847966f, -0.51524538f, -0.22704943f, -0.32174638f, -0.04334387f,
  -0.31945556f, -0.02859028f, -0.08713368f, 0.07726861f,  -0.10296666f,
  -0.10213533f, -0.23708141f, -0.02333682f, 0.08411486f,  -0.30929777f,
  0.03750140f,  -0.23049986f, 0.01349622f,  -0.03027610f, 0.06979409f,
  -0.06739203f, -0.07239801f, -0.05144362f, -0.55283099f, -1.02631605f,
  -0.60545647f, -0.35721537f, 0.16642649f,  0.01840148f,  0.74865562f,
  -0.07214901f, -0.43490404f, -0.69196141f, -0.64429355f, -0.56081355f,
  -0.11410324f, 0.00650611f,  0.52359116f,  -0.15617745f, -0.27214575f,
  -0.34950313f, -0.49601862f, -0.47096694f, -0.11336641f, -0.21351469f,
  0.33239669f,  0.26252869f,  0.28264564f,  0.47001326f,  0.00777478f,
  -0.32676828f, 0.13421483f,  -0.16309717f, -0.14873925f, -0.10171954f,
  -0.27570680f, 0.00264391f,  -0.10072701f, -0.25737929f, 0.00888844f,
  0.01302767f,  0.01099187f,  -0.05927843f, -0.15974462f, -0.87535900f,
  -1.03486395f, 1.14924955f,  -0.86139095f, -0.64316398f, -0.90272886f,
  0.24782632f,  -1.28585386f, -1.01300800f, -0.90389568f, 0.32697833f,
  -0.81594467f, -0.73126745f, -0.88831306f, 0.86061603f,  -1.46099842f,
  -0.14457881f, -0.42000213f, 0.16511787f,  -0.43893662f, -0.17594650f,
  -0.46019340f, 0.36923212f,  -0.56047863f, 0.29941931f,  -0.10080066f,
  -0.64410645f, 0.00405417f,  -0.27774882f, -0.32704988f, -0.63111925f,
  -1.09119618f, -0.04036587f, -0.18350835f, -0.42456576f, -0.54308987f,
  -1.04569161f, -0.61217678f, -0.57757348f, -0.49611390f, -0.60246390f,
  -0.87008017f,
};
static const float intra_y_mode_layer0_bias[EM_MAX_NODES] = {
  0.29163626f,  0.28653988f,  0.32602435f,  -0.30192518f, -0.36489797f,
  -0.35177949f, -0.32528833f, -0.20111403f, -0.25495365f, -0.02075642f,
  -0.22739601f, -0.26749593f, -0.06544726f,
};
#endif  // QNN
#elif INTRA_MODEL == 0
static const float intra_y_mode_layer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  1.30896795f,   2.09412193f,   0.33283857f,   -1.76670516f,  -1.69598281f,
  -0.30515718f,  -0.65710253f,  0.07573591f,   1.10326910f,   1.22165215f,
  0.29685083f,   -0.43429789f,  -0.84372181f,  -0.41350251f,  -0.25062785f,
  0.01051332f,   0.30882484f,   0.24972662f,   0.26481935f,   -0.03709844f,
  -0.00163173f,  0.38097689f,   0.41508660f,   0.19193593f,   0.53981608f,
  0.91475362f,   -0.41450384f,  0.47891566f,   0.67828560f,   0.54009295f,
  0.74686688f,   0.22601314f,   -0.61547583f,  -0.49089950f,  -0.49970603f,
  -0.45766181f,  -0.71079165f,  -0.00383741f,  0.00689562f,   0.01975504f,
  -0.02150093f,  0.03174472f,   0.69115710f,   1.27592635f,   0.22694871f,
  0.69552267f,   -0.58366138f,  -0.99326605f,  -0.34809804f,  0.71564180f,
  -0.46435630f,  -0.28954890f,  -0.61519384f,  -0.69541651f,  -1.04036427f,
  -0.98249429f,  0.96697193f,   -0.25969765f,  -0.02778664f,  -0.33796245f,
  0.15089087f,   0.17208064f,   0.48558334f,   0.36550343f,   0.51304841f,
  -0.15286776f,  1.06236804f,   -52.28427124f, -16.84325218f, 0.35464314f,
  0.52232480f,   0.33151451f,   0.38635293f,   0.43116552f,   0.03138081f,
  0.04372348f,   -0.03665810f,  0.06744806f,   -0.02333582f,  -0.33482116f,
  -0.39766783f,  -0.39644074f,  -0.28438905f,  -0.34006339f,  0.02439460f,
  0.29564288f,   0.49737737f,   -0.21402943f,  0.35671976f,   -2.02658939f,
  0.11373481f,   0.39156783f,   1.49954271f,   1.10636389f,   -0.09091280f,
  -3.87052298f,  -7.86859322f,  -7.73388052f,  0.65577877f,   2.26687527f,
  0.07997464f,   -0.17954570f,  0.39934927f,   0.21037264f,   0.88823348f,
  -0.70462471f,  0.27144998f,   -0.61701846f,  -1.64132881f,  0.81048447f,
  -8.76458549f,  -0.07761838f,  -0.08951559f,  -0.11623919f,  -0.01558601f,
  0.18107152f,   0.13442566f,   0.20958145f,   0.14684856f,   0.09036513f,
  0.50326216f,   -0.07629660f,  -0.13536088f,  -0.17583935f,  0.20300445f,
  -0.05649211f,  0.80695844f,   0.21492858f,   0.73235172f,   -0.53012216f,
  -0.49736208f,  -0.75752109f,  -0.10354231f,  0.56127542f,   -0.06441861f,
  0.37236682f,   -0.81015193f,  -0.26297972f,  0.20046775f,   0.46327946f,
  0.61395961f,   0.42180753f,   -0.31651005f,  0.04738922f,   0.20270196f,
  0.22417746f,   0.36245459f,   0.20141757f,   0.00296179f,   -0.13493264f,
  -55.56344986f, 1.72986305f,   0.01706203f,   -0.23718509f,  -0.34629828f,
  -0.25655907f,  -0.32119584f,  -0.31119165f,  0.23421329f,   0.38174531f,
  0.27721453f,   0.24945526f,   0.47963771f,   -0.07316421f,  -0.06696744f,
  -0.09992512f,  -0.06825850f,  0.02937076f,   -0.86968023f,  -0.33205053f,
  -0.20727663f,  0.55790782f,   0.05450330f,   -1.07876635f,  -1.16607809f,
  -1.25647032f,  -0.46973017f,  -0.13927206f,  -0.02616777f,  0.77115643f,
  0.86724716f,   -0.08095260f,  -1.54893613f,  -1.07079172f,  -0.26746246f,
  -0.25678891f,  -0.36655203f,  0.24541102f,   0.22087938f,   -0.24797539f,
  -0.95336086f,  -0.58313882f,  -2.38816047f,  -1.31085157f,  -0.92718530f,
  -0.92981458f,  -0.54698294f,  -0.62899506f,  -0.59244943f,  -0.61179876f,
  -0.24277227f,  -0.09745613f,  -0.09305053f,  -0.21911488f,  -0.05198245f,
  -0.55823255f,  -0.44346830f,  -0.13723561f,  -0.38447747f,  -0.33696482f,
  -0.31524280f,  0.28394008f,   1.06715763f,   0.58091605f,   -0.22438209f,
  -0.26161852f,  -0.13438946f,  -0.35170335f,  0.44047180f,   0.43416318f,
  1.03592622f,   0.98030013f,   0.23747285f,   -0.35113284f,  -0.33220971f,
  0.28132889f,   0.44073701f,   0.25900194f,   0.56185949f,   0.71478897f,
  0.08845533f,   -0.09321080f,  0.40673363f,   0.17236570f,   -0.01760110f,
  1.76567864f,   0.28417301f,   -0.77220994f,  -0.01161629f,  -0.70951456f,
  -0.67317551f,  -0.52697957f,  0.55083489f,   0.93675059f,   0.58238459f,
  0.42789385f,   0.97766018f,   -0.24178270f,  -0.25605109f,  -0.11186121f,
  -0.17197771f,  -0.07322877f,  -0.94234031f,  -0.47358653f,  0.02925214f,
  0.32888722f,   1.58423042f,   1.64975739f,   0.25439388f,   -1.35042679f,
  -1.02456057f,  -0.49803865f,  0.12175005f,   0.19458695f,   1.31132364f,
  1.86400688f,   0.36755824f,   -1.58345568f,  -0.04530181f,  -0.05975876f,
  -0.17112032f,  -0.00220456f,  0.54810870f,   0.80279875f,   0.09589985f,
  -0.49918383f,  -1.04267490f,  1.16107142f,   0.37781581f,   0.58584625f,
  0.52214444f,   0.46855921f,   0.89911503f,   1.19059777f,   -0.39075834f,
  -0.38266540f,  -0.41163382f,  -0.32197061f,  0.34667996f,   -0.21351185f,
  -0.11448930f,  -0.21834850f,  0.16276641f,   0.25288919f,   0.88861370f,
  -0.51851672f,  -0.38925931f,  -0.28500307f,  -0.49856982f,  -0.06409120f,
  -0.12181898f,  2.02990651f,   1.22522891f,   -0.02296225f,  -0.36940801f,
  -0.78528148f,  -0.81417638f,  -0.03370872f,  0.27398008f,   2.15906310f,
  0.11102708f,   0.03385111f,   0.12932117f,   -0.13670860f,  -0.00315404f,
  0.39775106f,   0.12613194f,   0.92525053f,   1.56815970f,   0.42635563f,
  0.35762829f,   -0.15533178f,  -0.03668910f,  -0.20823173f,  -0.10898326f,
  0.27371061f,   0.40842786f,   0.47936159f,   0.38217250f,   0.44272575f,
  0.68524861f,   -0.25028875f,  -0.21582618f,  -0.29378602f,  -0.22120276f,
  -0.15937138f,  -0.27000362f,  0.56055313f,   0.07590971f,   0.55276531f,
  0.06302992f,   -0.09888934f,  -0.56261021f,  -0.36721519f,  0.55289239f,
  -0.04068530f,  0.08000503f,   -0.22736274f,  -0.49380368f,  -0.41950226f,
  0.36688006f,   0.63097370f,   -0.18321614f,  0.11652667f,   0.04782382f,
  0.26176316f,   0.36221173f,   0.35431191f,   0.23864283f,   -0.26232302f,
  3.30924559f,   -29.50685501f, 1.54637265f,   0.05842972f,   0.13503104f,
  -0.00461596f,  0.07016118f,   0.25332451f,   -0.29203013f,  -0.40652141f,
  -0.16703017f,  -0.55010629f,  -0.39259923f,  0.34945482f,   0.40059194f,
  0.32668796f,   0.38317618f,   0.50167996f,   0.12677814f,   -0.48592871f,
  1.98353755f,   0.11376312f,   -0.15747151f,  -0.35974219f,  0.42230293f,
  0.14773969f,   0.29690880f,   -0.00330997f,  1.30453861f,   0.13089190f,
  -0.32636112f,  -0.05234614f,  0.79892570f,   0.44884488f,   0.50411355f,
  0.17089805f,   0.73770583f,   0.19193706f,   0.14634047f,   0.22189647f,
  0.99974632f,   0.60086501f,   0.32533598f,   -0.02472137f,  -0.04667218f,
  -0.72658360f,  -0.64157057f,  -1.16123438f,  -1.27223837f,  -1.70550120f,
  -0.56532878f,  -0.45181927f,  -1.10292935f,  -1.12272274f,  -1.14630151f,
  -0.23875885f,  -0.32310897f,  -0.19413407f,  -0.51155812f,  -0.47628009f,
  2.15703416f,   0.22224610f,   -0.91223437f,  -2.04715610f,  -6.34309721f,
  -6.99551010f,  0.76896667f,   1.52270842f,   0.26556891f,   -0.06443588f,
  0.21207353f,   0.60045046f,   0.97089386f,   -0.09387694f,  0.53116256f,
  -0.48450217f,  -0.80421299f,  -0.04350608f,  0.65216869f,   0.15533473f,
  -0.13975596f,  -0.90356255f,  0.20643322f,   -0.67623210f,  0.89116526f,
  -1.35485494f,  -0.34752637f,  0.09249599f,   -0.12206010f,  0.11281671f,
  -0.05147024f,  -0.18465851f,  0.33000600f,   0.31416693f,   0.65824431f,
  0.07762956f,   0.42621648f,   -0.08969871f,  0.09117105f,   -0.01422282f,
  -0.08079787f,  0.08064511f,   0.74993801f,   0.30768877f,   -0.46225110f,
  0.99244702f,   0.36318785f,   -0.00675672f,  -0.22221953f,  0.03718410f,
  0.66266239f,   -1.96372843f,  -0.99133027f,  -6.17415285f,  -3.97943807f,
  -2.98690009f,  1.38950658f,   -1.56293762f,  0.67828858f,   0.27167958f,
  0.07066533f,   0.49078661f,   0.73398799f,   -0.15395026f,  -0.13720000f,
  -0.19093171f,  -3.43998981f,  2.91550732f,   -0.77259725f,  -0.41039670f,
  -0.24577877f,  -0.33160138f,  -0.05657055f,  -0.41704440f,  0.54639900f,
  0.48698837f,   0.79411107f,   0.93743503f,   0.10083015f,   -0.52746910f,
  -0.51913375f,  -0.60023785f,  -0.42488909f,  -0.68080574f,  0.15073912f,
  0.05867955f,   -0.16532870f,  -0.81384331f,  -0.05705089f,  1.07500780f,
  1.41107845f,   0.57906562f,   -0.33015302f,  -0.40246743f,  0.16967545f,
  -0.63656396f,  -0.70253712f,  0.28344029f,   1.42367208f,   0.33593431f,
  0.39491716f,   0.38032180f,   0.70728761f,   -0.16711655f,  -0.32531956f,
  0.54414117f,   0.77481312f,   0.26237014f,   0.60059988f,   0.53370887f,
  0.00288443f,   -0.30381390f,  -0.19789071f,  -0.00051281f,  -0.43709794f,
  -0.33616954f,  -0.04245600f,  0.00351203f,   0.55297667f,   -0.21485299f,
  0.06140987f,   -0.32780781f,  -0.16996671f,  -0.28536469f,  -0.27128336f,
  -0.33029684f,  -1.01469243f,  1.51723886f,   1.12074077f,   0.43510088f,
  -0.68805325f,  -0.45129094f,  -2.23878217f,  0.76228851f,   -2.28013468f,
  1.07211840f,   -0.34496063f,  1.17765224f,   0.81740791f,   0.20327865f,
  1.50969231f,   -3.20398974f,  -1.30073988f,  1.00111198f,   0.74657482f,
  0.48337999f,   -0.02971230f,  -0.73405439f,  -0.37507883f,  -0.11920927f,
  1.32575214f,   1.97973311f,   3.00901031f,   -1.52193046f,  -1.31069922f,
  -3.55871463f,  -4.14448071f,  -3.07306933f,  0.46643472f,   0.41547418f,
  -1.54641914f,  -4.01341057f,  -3.35915518f,  -1.87704885f,  -1.76544201f,
  -1.49813855f,  -5.15139723f,  -1.71751916f,  -0.18955562f,  0.36802807f,
  1.41220665f,   0.46807832f,   -1.64471209f,  -4.54719400f,  -2.40570831f,
  1.42541003f,   0.15325388f,   0.74448401f,   -0.35065079f,  -0.45678297f,
  0.26248741f,   1.06406116f,   -0.68676394f,  1.39135897f,   0.28695107f,
  0.44442874f,   -0.36472905f,  0.03320441f,   0.05374834f,   0.18130915f,
  -0.08325198f,  1.56076586f,   4.07922745f,   -2.64871264f,  -0.96675569f,
  -0.51420993f,  -0.53131920f,  -0.63479728f,  -0.38243932f,  0.56704688f,
  -0.45763999f,  -0.74209410f,  -0.38417065f,  -0.32668889f,  -0.12429646f,
  0.21073395f,   0.22607914f,   0.17757094f,   0.19820836f,   0.33296058f,
  -0.33281201f,  -2.48175073f,  1.00945568f,   -2.09828115f,  0.99693388f,
  0.35379624f,   -0.54270947f,  -0.27459109f,  -0.59786206f,  -0.96914274f,
  -0.50902557f,  -0.03501211f,  0.24525487f,   -0.39974791f,  -0.57770562f,
  -1.75844419f,  0.16160190f,   -0.47275162f,  -0.86596805f,  -0.26848617f,
  0.03091109f,   -0.67036623f,  -0.76310331f,  -1.26851451f,  0.43435475f,
  -2.84213138f,  -0.35972804f,  0.03769260f,   -2.70891643f,  0.02833674f,
  -2.03524613f,  -2.11163807f,  -1.82060409f,  -2.18860030f,  -1.55582047f,
  -1.86038601f,  -3.21394992f,  -1.47186828f,  -1.53627527f,  -1.38239598f,
  -2.14696956f,  -2.15700650f,
};
static const float intra_y_mode_layer0_bias[EM_MAX_NODES] = {
  0.50167596f,  -0.08335415f, -0.00691969f, -0.10834799f,
  -0.48612863f, 0.30673707f,  0.07386331f,  0.10394136f,
  -0.15613076f, 0.86791241f,  -0.14518929f, -0.02071429f,
  0.53995448f,  -0.30169111f, 0.10304673f,  -0.52734178f,
};
static const float intra_y_mode_layer1_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  -0.11681971f, -0.26892683f, -0.20537648f, -0.32074016f, 0.34524268f,
  0.11867570f,  -0.06229602f, 0.09802279f,  -0.16284329f, 0.15133937f,
  0.01581969f,  -0.23912507f, 0.04856881f,  -1.18665659f, 0.00592961f,
  0.23714563f,  0.12306449f,  0.28210810f,  -0.23131335f, -0.53191978f,
  0.67233366f,  1.09172380f,  -0.15658900f, -0.39918324f, 0.19642301f,
  0.47750497f,  0.25321347f,  -0.13878678f, -0.22332434f, -1.57403791f,
  0.44716686f,  -1.05864167f, -0.24045683f, -0.76822257f, -0.09681815f,
  0.36310774f,  -0.57736874f, -0.03534552f, -0.24051522f, 0.03953006f,
  0.28905150f,  0.03243127f,  -0.00291695f, 0.20611143f,  1.09787190f,
  -1.62746298f, 0.00134248f,  0.00646137f,  0.78242522f,  0.32615367f,
  0.12666689f,  -0.86708671f, -0.06539314f, -0.00429153f, 0.16320778f,
  0.67291045f,  -0.87406403f, 0.00009826f,  0.62577963f,  -0.42693308f,
  -0.44051337f, -1.64998674f, -0.15617408f, 0.28527117f,  -0.38765320f,
  -0.63021660f, -0.28601092f, 0.92080355f,  0.27236724f,  0.37056509f,
  1.00912321f,  0.13489608f,  0.98354256f,  -0.34892231f, 0.22327067f,
  -0.33828124f, -0.51922065f, -0.81589079f, -0.25595221f, -0.85340679f,
  -0.56555676f, 0.38284570f,  -0.21648857f, -0.01472592f, 0.74027610f,
  1.04627943f,  0.29623982f,  -0.30353868f, 0.70806235f,  -0.49340829f,
  0.42685196f,  -0.17810948f, -0.70195502f, -1.13010657f, 0.21585551f,
  -0.55075365f, -0.12864679f, -0.40429538f, -0.34658474f, 0.85248071f,
  -0.61330926f, -0.06202400f, 0.80969387f,  0.63350558f,  0.38021675f,
  -0.09070056f, -0.20646197f, -0.74236751f, 0.38396302f,  -1.28724575f,
  -0.28887177f, 0.04894404f,  -0.06103147f, -0.56343383f, 0.37213075f,
  -0.15940158f, -0.47521356f, 0.34700027f,  -0.17073812f, 1.04086506f,
  -0.08156278f, -0.05405118f, 0.16007812f,  -0.43113142f, 0.34605184f,
  -1.16284561f, -0.25247890f, -1.07007992f, 0.95544285f,  0.45981553f,
  0.05672260f,  -0.64878821f, 0.18288119f,  0.39362088f,  0.10165212f,
  -0.21604419f, -0.32717854f, -0.28535572f, 0.40726596f,  -0.50627446f,
  -0.33368060f, -0.72662526f, 0.25036210f,  -0.75623047f, -0.01974592f,
  -0.11175799f, -0.03540947f, -0.40185571f, 0.13939297f,  0.10607217f,
  -0.12538704f, 0.07105321f,  -0.36102888f, 0.03802232f,  0.09468170f,
  -0.31337997f, -0.06351251f, -1.18249583f, 0.03039459f,  0.25172889f,
  0.14746176f,  0.09412323f,  -0.00160459f, -0.47952384f, 0.10089367f,
  0.48366231f,  -0.18695349f, -0.16367838f, -0.19784576f, 0.11910936f,
  0.19349112f,  -0.26822156f, -0.13550444f, -0.83402663f, 0.16768840f,
  -0.46425241f, -0.00949856f, -0.29935548f, -0.25273621f, -0.31233543f,
  -0.36734834f, -0.02988117f, -0.14707136f, 0.49602193f,  -0.30126551f,
  0.06440746f,  -0.07707611f, -0.20058876f, 0.39143902f,  -1.43313110f,
  -0.10973874f, 0.24381918f,  0.03163423f,  -0.52064371f, 0.06388164f,
  -0.24321778f, 0.02883280f,  -0.18513037f, 0.05492806f,  -0.52755010f,
  0.13730772f,  1.33103323f,  0.27014309f,  0.43292153f,  0.40292996f,
  -0.50741494f, 0.56909096f,  0.34983036f,
};
static const float intra_y_mode_layer1_bias[EM_MAX_NODES] = {
  2.53566766f, -0.26495367f, -0.19500263f, -1.27383018f, -0.45754233f,
  0.19924060f, -1.40389228f, -1.31922054f, -0.62359804f, 1.45344317f,
  0.11334381f, -0.36379102f, -1.47851610f,
};
#elif INTRA_MODEL == 1
static const float intra_y_mode_layer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  3.25421119f,  0.83241528f,  -0.34194693f, 1.06626964f,  1.70176160f,
  1.08237302f,  0.21909708f,  1.49587238f,  0.28995937f,  -1.81523871f,
  -0.83506685f, -1.28771019f, -2.80867767f, -2.59195185f, 1.04452908f,
  -1.55363846f, 0.14894012f,  0.39010122f,  0.60705024f,  0.61095548f,
  1.02606237f,  0.31847697f,  -0.31753951f, -0.17291339f, -1.75113940f,
  -3.08065701f, -0.44917801f, -4.10130072f, -2.24794054f, 0.06618322f,
  0.04589318f,  -1.94953263f, -1.25251997f, -2.06857109f, -3.25152636f,
  -2.43252254f, -1.28154850f, -0.15315209f, 0.85094565f,  -0.66256464f,
  -0.15999143f, 0.05830003f,  -0.22936732f, -0.98942024f, -0.27505162f,
  0.63055259f,  0.38244197f,  -0.39942655f, -1.32355118f, -3.43195891f,
  0.66692019f,  -3.97899914f, -2.45767784f, -0.66401720f, -1.06409740f,
  -0.96229470f, -1.34632933f, -1.67345822f, -0.00679390f, -1.52498519f,
  -0.21714428f, -0.09536602f, -0.31254593f, -0.53290969f, -1.61087966f,
  -1.97972298f, -0.34853858f, -1.99351835f, -0.26065490f, -0.33794600f,
  -0.51932037f, -0.95920062f, -0.23353997f, -0.08324266f, -0.68349385f,
  0.80656248f,  0.75443375f,  0.22604932f,  -0.30363911f, 0.26964378f,
  -0.25518602f, -0.56460977f, -0.26576823f, 0.29342613f,  0.21805587f,
  -0.36539561f, -1.06408000f, -0.39913115f, -0.19441223f, -0.28190216f,
  -0.26354057f, -0.48106682f, -0.51619059f, -0.42825261f, -0.35898715f,
  -0.23746753f, -0.69246769f, -0.32127795f, 0.04527389f,  0.07646767f,
  0.86208528f,  1.52357388f,  -0.31446928f, -1.58508742f, -1.00891662f,
  -1.12402534f, -0.06254670f, 0.69117188f,  2.25844765f,  2.18795919f,
  0.07083157f,  -1.16923010f, -1.51494741f, -0.22204661f, 0.21619561f,
  0.74370873f,  1.33262527f,  1.70802188f,  0.42497265f,  -0.56148291f,
  1.77942121f,  2.45370388f,  -0.08208816f, -1.68692493f, -2.30178666f,
  -1.25355852f, -0.25438401f, 0.33359438f,  0.88980800f,  1.58314943f,
  0.00402840f,  -0.59007388f, -0.50459176f, -0.19714071f, 0.09664732f,
  -0.39533046f, -0.13963586f, 0.03554891f,  0.05180809f,  -0.04829287f,
  0.10662482f,  0.17209661f,  0.13669175f,  -0.22618794f, -0.14867736f,
  0.29160652f,  0.85884947f,  -0.88982099f, -1.54587388f, -0.77862102f,
  -0.37374827f, -0.28783011f, 0.29075885f,  0.45095131f,  0.87517864f,
  -0.45451021f, -0.63363767f, -0.11917807f, -0.01956690f, 0.12327891f,
  0.01886181f,  -0.07804146f, 0.23229422f,  -0.01422363f, -0.10498187f,
  0.19954905f,  0.27580088f,  0.02445294f,  0.74356884f,  -0.36650881f,
  -0.48562115f, -0.63996500f, -0.06584426f, 0.64499289f,  0.39033177f,
  1.28353095f,  0.42623815f,  -0.46956891f, -0.55120420f, -1.54874647f,
  -1.19253325f, -0.09486591f, 0.47623089f,  1.07171428f,  0.04035379f,
  0.13441092f,  0.20077878f,  -0.17253096f, 0.04670176f,  0.53005749f,
  0.14769654f,  0.35389423f,  -0.50966781f, 0.85345072f,  0.81105536f,
  1.11541820f,  0.19641626f,  -0.55897945f, -0.88012040f, -0.97506863f,
  -0.61572558f, 0.44058543f,  0.53096402f,  1.41706944f,  1.19883788f,
  0.13136031f,  -1.16558778f, -0.74829876f, 0.16956183f,  0.14075102f,
  -0.01606538f, 0.49301273f,  0.30474845f,  -0.06741105f, -0.45839998f,
  0.00381321f,  -0.98547673f, -0.91761792f, -0.84759879f, -1.08656871f,
  -1.31743503f, -1.23888731f, 0.07737465f,  -0.98958433f, -0.44031405f,
  -0.15687080f, 0.31408367f,  -0.55273664f, -1.38787496f, -1.57434750f,
  0.08346355f,  -0.71427333f, -0.78247374f, -0.33268726f, -1.01756990f,
  -0.50164986f, -0.66022146f, -0.05005776f, -0.23442619f, -0.93997264f,
  -0.05905229f, -0.03731308f, -0.36776504f, -0.07967450f, -0.10948557f,
  -0.32955837f, 0.04276897f,  0.10008411f,  -0.29963335f, -0.51218283f,
  -0.14072908f, -0.52133745f, -0.36681116f, -0.14871947f, 0.03321882f,
  -0.04429314f, -0.60307360f, -0.11100763f, -0.38996693f, -0.57227498f,
  -0.43243694f, -0.15016703f, -0.39099315f, -0.17310677f, 0.50781864f,
  -0.10307688f, 0.21237759f,  0.98018909f,  0.63218993f,  0.24136724f,
  -0.25668883f, 0.54788917f,  0.99449408f,  0.62861180f,  0.73939747f,
  1.34054518f,  1.08930945f,  0.50279760f,  -0.09587779f, 0.79570293f,
  0.21639907f,  0.22355966f,  0.18365347f,  0.30124575f,  0.17422490f,
  -0.00095180f, -0.06037733f, 0.35840055f,  0.17369150f,  0.35852003f,
  -0.04580922f, 0.03481253f,  1.06932640f,  1.76674902f,  0.16657165f,
  -0.41489807f, -0.30910334f, -0.81129014f, -0.33103663f, 0.98906696f,
  2.49599242f,  1.59897745f,  -1.73723626f, -1.56911981f, -1.16977656f,
  -0.29015324f, -0.33488548f, -0.72977608f, 0.44539714f,  0.61964017f,
  0.00812741f,  -0.49303642f, -0.37104511f, -0.15975946f, -0.29068294f,
  0.17431180f,  -1.14951098f, -0.73643124f, 0.20176785f,  -0.47844148f,
  -0.69647211f, -0.20410083f, -0.04146540f, 0.34940478f,  -0.69478703f,
  -0.20168151f, -0.11569377f, -0.41936159f, -0.88762301f, -0.42022845f,
  -0.16972865f, -0.08620989f, -1.01715386f, -0.80652952f, -0.11056732f,
  -0.58517927f, -0.75228751f, -1.50805354f, 0.59454501f,  -0.84970361f,
  -1.54800105f, -1.15418899f, -0.66930723f, -0.16584654f, -0.75598997f,
  -1.31521833f, -0.06565727f, -0.95205116f, -0.75257790f, -0.54419488f,
  -0.20672868f, -0.00317522f, -0.60407227f, -0.76605761f, -0.22166237f,
  -0.59571117f, -0.28089228f, -0.80104572f, -0.41444075f, -0.22658449f,
  -0.93642777f, -0.13044715f, -0.31354961f, -0.01171641f, -0.39794129f,
  -2.45608497f, -0.15870455f, 0.04484084f,  0.94060278f,  0.63031036f,
  0.11562033f,  -2.87541676f, -5.26478291f, -5.25950575f, 0.75914472f,
  2.24783635f,  0.04764418f,  -0.13378850f, 0.25342330f,  0.24942495f,
  0.61018962f,  -0.38615897f, 0.32759959f,  -0.22135888f,
};
static const float intra_y_mode_layer0_bias[EM_MAX_NODES] = {
  -0.81747979f, -0.42135012f, -0.79227108f, -0.82322061f,
  -0.1560079f,  0.03117871f,  0.49010596f,  0.13116579f,
  -0.54993922f, -0.7149772f,  -0.23662245f, 0.03821357f,
  -0.40952027f, -0.46162742f, -0.74024546f, -0.03166655f,
};
static const float intra_y_mode_layer1_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  -0.42374155f, 0.04469313f,  0.27568659f,  0.24399059f,  0.44666657f,
  0.54714739f,  -0.05765932f, 0.97564876f,  -0.08219855f, -0.05308634f,
  -0.29883644f, 0.07887050f,  0.67924124f,  0.10922588f,  0.03000334f,
  -0.11508043f, -0.07508633f, -0.21689466f, -0.63507611f, 0.72922260f,
  0.49717811f,  0.49677527f,  1.27523613f,  -0.09392757f, 0.53843564f,
  -0.29074165f, -0.15928090f, 0.69556022f,  0.38707802f,  0.22757551f,
  0.02435612f,  0.26965711f,  -0.03389324f, 0.99883568f,  -0.40022510f,
  -1.07812798f, 0.11771070f,  0.13191527f,  -0.33853132f, 1.30803800f,
  -0.76541710f, 0.05068886f,  -0.15734182f, -1.50259686f, 1.01394260f,
  0.01844882f,  -0.16017148f, 0.00365675f,  -0.24295457f, -0.10837650f,
  -0.16442834f, 1.00503731f,  0.55746198f,  1.66833937f,  0.02790450f,
  1.58542454f,  -0.65815592f, 0.02907153f,  -0.22326998f, 1.44894159f,
  0.62588948f,  -0.39868730f, 0.19732240f,  0.28307000f,  -0.25391942f,
  -1.53303480f, -0.46522763f, 1.19928741f,  2.14147091f,  0.61497545f,
  -1.10417068f, 0.14178593f,  -0.06792924f, -0.78491366f, 0.06373447f,
  0.77991676f,  -0.64481199f, -0.49667436f, -0.57413274f, 0.62936550f,
  -0.06420507f, -0.76614314f, -2.04949498f, 1.96328628f,  1.27865183f,
  0.50700104f,  -0.53500682f, -0.35716763f, 0.96526140f,  -0.08792775f,
  0.13908722f,  1.22653162f,  -0.38924623f, -0.02473259f, -0.29565445f,
  0.87909335f,  -0.69016820f, -1.28030133f, 0.39411297f,  -0.23394127f,
  1.60523021f,  0.45932889f,  -0.66690910f, 1.63639104f,  -0.85252142f,
  -1.42000675f, -0.44120467f, -0.14705649f, -0.23317231f, -0.15399484f,
  -0.42765459f, -0.39513302f, -0.56129110f, -1.62720096f, -0.37345424f,
  0.22233449f,  0.35729158f,  0.68891120f,  -0.05479694f, 2.06022477f,
  -0.83371496f, 0.13597731f,  -0.59289378f, 0.55378741f,  0.88158697f,
  -0.18289325f, -0.09380151f, 0.85405302f,  -0.50186628f, -0.86051232f,
  -1.60094547f, 0.80944461f,  0.55609846f,  1.84033251f,  0.15552863f,
  0.50249040f,  0.23430762f,  0.07278537f,  -0.03785618f, 0.78152335f,
  0.48668283f,  -0.45564592f, -0.99100739f, 0.49889421f,  -0.52616268f,
  -0.14335151f, -1.44116461f, 0.42696729f,  0.44389191f,  0.77284449f,
  -0.11848417f, 1.02764523f,  -0.01070085f, -0.15349568f, 0.23139083f,
  0.22216496f,  0.59136367f,  -0.01235987f, -0.19279720f, 0.00734764f,
  -0.36865816f, 0.04657710f,  -0.66522646f, 0.63181424f,  0.43552345f,
  0.81595367f,  0.22135331f,  0.45675370f,  0.15368122f,  0.33907372f,
  -0.21098903f, 0.38545710f,  0.38918656f,  -0.09236888f, 0.03113179f,
  0.24959771f,  -0.51837540f, -0.42470467f, -0.80518049f, -0.06552513f,
  0.25294402f,  0.61048514f,  -0.25203300f, 1.53990638f,  -0.30499443f,
  -0.49513021f, -0.55948836f, -0.13484660f, 0.81041646f,  -0.07034243f,
  -0.17999026f, -0.21550696f, 0.54660398f,  0.87353003f,  0.42699662f,
  0.48015857f,  0.36120617f,  -0.19195901f, 0.94503778f,  0.12072270f,
  -0.04736423f, 0.01011357f,  0.20518620f,  -1.01894653f, 0.67051882f,
  -0.51721150f, 0.83390629f,  0.31698337f,
};
static const float intra_y_mode_layer1_bias[EM_MAX_NODES] = {
  1.5879339f,   -1.31331193f, 1.76149082f,  -3.5566237f,  -1.46776271f,
  -1.81602716f, -0.97854549f, -1.72096097f, -2.21768355f, 0.14365752f,
  -1.13605404f, -0.56143326f, -0.10940339f,
};
#else
static const float intra_y_mode_layer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  -1.45904231f, -1.48105717f,  -1.21905005f, -0.99425846f, -0.72013646f,
  -1.56753266f, 0.46365318f,   -2.34078264f, -0.87981874f, -1.21731257f,
  0.07834810f,  -0.74270242f,  0.17536204f,  -0.72889155f, 0.21613096f,
  -1.30746901f, -0.07189807f,  -0.30920359f, -0.42360586f, -0.32976595f,
  0.61125225f,  -0.40401664f,  -0.46380937f, -0.48241124f, -0.09169739f,
  -1.24035454f, -1.57400179f,  -0.97853023f, -1.27388859f, -0.75463551f,
  -1.21233249f, -1.97610152f,  -0.62432706f, -1.55898738f, -1.89152431f,
  -1.96191299f, -2.05822253f,  -1.45473003f, -1.81352878f, -0.54101545f,
  -0.90537024f, -1.30576289f,  0.06403468f,  -0.70273584f, -2.53216124f,
  -0.58717757f, -0.11313074f,  1.40061498f,  -2.58524656f, -0.68063539f,
  -4.91367531f, -10.02827549f, -9.03116035f, 0.76935238f,  1.97608054f,
  -0.51992458f, -0.56555337f,  -0.03795741f, -0.32126662f, 0.52860099f,
  -1.34571314f, -0.70605963f,  -1.34000051f, -1.32553613f, -1.33942246f,
  -1.47521114f, -2.10679388f,  -1.17385614f, -0.85995871f, -0.61459953f,
  0.26572818f,  -1.17996180f,  -0.07220210f, -1.14785123f, -0.96550763f,
  -1.22856188f, -0.65851319f,  -1.07622349f, -0.83777046f, -0.55306429f,
  -0.90124726f, -0.27407822f,  0.16872092f,  0.55917865f,  -0.86760408f,
  -0.54142058f, -1.16709852f,  -1.45499396f, 0.18313704f,  -1.65019953f,
  -2.29931808f, -4.19926739f,  0.10529259f,  -4.61856174f, -0.04307833f,
  -0.59742057f, -0.46929240f,  -1.14224887f, -0.30266514f, -0.28411716f,
  -0.36379224f, -0.31734818f,  -0.80896217f, -1.22959530f, -2.15131521f,
  -1.71344125f, -1.98720849f,  -1.39354014f, 1.09155226f,  -1.48082018f,
  0.02515547f,  -1.40482438f,  -1.17067552f, -1.58625710f, -1.41049290f,
  -2.15874267f, -1.61811435f,  -0.34545803f, 0.78807765f,  -0.71978003f,
  -0.08999127f, 0.07285585f,   0.26820156f,  -0.40555477f, -0.29835227f,
  0.30797306f,  -0.78120542f,  0.41445079f,  -1.31266987f, -2.12173104f,
  -0.54677927f, -0.19051997f,  0.39945528f,  -0.71934950f, 0.90256792f,
  -0.97902769f, 0.14004853f,   -0.28767022f, 0.79102987f,  -1.03011096f,
  -0.63821375f, -0.65209419f,  -1.52184415f, -1.20338511f, -1.25468421f,
  -1.16853249f, -1.42602909f,  -0.20473200f, -0.14303091f, -0.10580762f,
  0.01428980f,  -1.25742543f,  -1.32055807f, -0.65327257f, -0.44505909f,
  -1.32543874f, 1.94643092f,   1.87589169f,  0.32148400f,  -0.69779652f,
  -1.48155522f, -1.20345926f,  -0.27647004f, 1.03214788f,  1.49910200f,
  1.28993666f,  0.31756896f,   -0.06225171f, -0.54086399f, -0.70473665f,
  -0.41427040f, 0.74429977f,   0.12236108f,  0.16991034f,  0.25040323f,
  0.08337947f,  0.08082067f,   -0.08977598f, 0.30201960f,  0.20201366f,
  0.07056814f,  0.44908699f,   0.08169246f,  0.68542141f,  0.06993009f,
  -0.44261643f, -0.15720040f,  -0.47039536f, -0.01401667f, -0.31736282f,
  0.13427383f,  0.10634028f,   0.12322506f,  0.08509185f,  0.17527771f,
  -0.67737269f, 0.18603288f,   -0.75827229f, -0.69913775f, -0.67075622f,
  -0.80798292f, 0.01205517f,   -1.04758894f, -1.05295146f, -0.30262572f,
  -0.69123507f, -1.23463559f,  -1.08715987f, -0.63051444f, 0.22698002f,
  -1.77023900f, -0.63371313f,  -0.71319920f, -0.51517761f, -1.17043364f,
  -1.02911007f, -0.76834995f,  -0.36862943f, -1.05103183f, -0.14299597f,
  -0.30609399f, -0.45070440f,  -0.05047188f, -0.63855636f, -0.38136876f,
  -0.78729701f, -0.22207496f,  -0.38511688f, -0.78247929f, -0.10711611f,
  -0.08622702f, -0.36725995f,  -0.37470078f, -0.70657730f, 0.94432807f,
  0.24861705f,  0.36621404f,   -0.51548427f, 1.44962597f,  1.57111800f,
  0.33339670f,  -0.62807965f,  -0.07388350f, 0.08841269f,  0.07688162f,
  -0.28936625f, 1.19294739f,   2.05320096f,  1.24433792f,  -1.06842184f,
  -0.37696266f, -0.04237649f,  -0.05741963f, 0.06791978f,  0.91586196f,
  1.16186714f,  0.31778958f,   -0.66934991f, -0.45147017f, -0.64182824f,
  -0.59231180f, -0.11110062f,  0.01052870f,  -0.82691777f, -1.10826874f,
  -0.57979113f, -0.86748672f,  -0.03923493f, -0.26230037f, -0.06855845f,
  -0.30031675f, 0.23460877f,   0.30104795f,  0.84243172f,  -0.24144349f,
  -0.45243755f, -0.52152723f,  -0.41698185f, 0.30053082f,  0.68186176f,
  1.47991014f,  0.95449501f,   -0.04336617f, -0.07995772f, -0.72112912f,
  -1.20757151f, -0.45515931f,  0.92047822f,  1.95170188f,  0.13433404f,
  0.10795156f,  0.39200461f,   -0.14888336f, -0.20561767f, 0.20753154f,
  0.29099193f,  0.33768553f,   -0.32457024f, -0.12124801f, 0.08418679f,
  -0.26245648f, -0.01649242f,  0.23688087f,  0.38284302f,  0.93868810f,
  0.26027548f,  0.71215528f,   -0.18241829f, -0.09118931f, -0.19812977f,
  -0.12744801f, -0.16879137f,  -0.44842029f, -0.03475569f, 0.28520176f,
  -0.14168346f, -0.56583887f,  -0.17663740f, -0.02617569f, -0.63746828f,
  -0.46981820f, -0.26949576f,  0.16695768f,  0.10286554f,  -0.01183783f,
  -0.21836290f, -0.11748483f,  -0.49374655f, 0.14844672f,  0.07224528f,
  0.07121197f,  -0.07259566f,  -0.10112333f, -0.23010527f, -0.02656167f,
  -0.00794917f, 0.04760697f,   -0.86393464f, -0.35055268f, -0.39786911f,
  -0.29389781f, -0.25428373f,  -0.38877568f, -0.51012838f, -0.47995979f,
  -0.61365521f, -0.48434979f,  -0.61427999f, -0.54096609f, -0.43227407f,
  -0.39616668f, 0.51425374f,   1.21060336f,  -0.22155872f, -0.11397154f,
  -0.05492590f, 0.23796363f,   0.87754458f,  -0.12305974f, -0.62936151f,
  -0.26581055f, -0.28777012f,  -0.41304696f, -0.79631907f, -1.07484996f,
  0.72291040f,  -1.08635712f,  0.22072069f,  0.20871794f,  0.51496536f,
  0.27579325f,  -0.05819914f,  0.04034151f,  0.33654529f,  -0.32365879f,
  -0.46976402f, -0.40896347f,  0.21888506f,  -0.42444447f, -0.72130120f,
  -0.28043541f, -0.27253419f,  0.85488528f,  -0.36375463f, -0.39555112f,
  0.07618652f,  0.13990177f,   0.05681707f,  0.12783647f,  0.00419408f,
  -0.07057954f, 0.01839282f,   -0.22774628f, -0.01272502f, -0.11383644f,
  -0.03375350f, -0.13321915f,  -0.05252256f, -0.33830693f, -0.47847399f,
  0.01873126f,  -0.17184362f,  0.02147043f,  -0.13174082f, -0.23532701f,
  0.05030607f,  -0.06316029f,  -0.24683212f, -0.08938917f, -0.30919096f,
  -0.14153945f, 0.06481177f,   0.10843518f,  -0.00461874f, -0.39339405f,
  -0.35760519f, -0.13615033f,  -0.05201279f, -0.03415191f, -0.02487582f,
  -0.34341985f, -0.02335851f,  -0.03940320f, -0.13777860f, 0.07224696f,
  -0.15402044f, -0.01574576f,  0.05711249f,  -0.15794431f, -0.31876820f,
  -0.56052440f, 1.77273190f,   -0.08813277f, -0.63364750f, -0.73659134f,
  0.29481056f,  -0.10078918f,  -0.03713036f, -0.17818230f, 0.83652967f,
  -0.13953424f, -0.93425483f,  -0.93360186f, 0.40044478f,  0.44334430f,
  0.73380154f,  0.47190824f,   0.80925435f,  0.34919488f,  0.08616193f,
  0.09081233f,  0.99436957f,   0.68509305f,  -0.32032922f, 0.32922935f,
  -0.89512533f, -0.91166121f,  -1.20900249f, 0.19991639f,  0.55633819f,
  -0.34032184f, -0.52032197f,  -0.38245502f, -0.85019875f, -0.88302112f,
  -0.77674437f, -1.01869226f,  -1.00603712f, -1.11396658f, -1.18580019f,
  1.39060783f,  -1.31458092f,  -0.92829978f, -1.32855618f, -0.67915946f,
  -0.89629608f, -0.40164569f,  -0.78271639f, -0.07462444f, -0.51630503f,
  -0.31475058f, -0.44318217f,  0.22135335f,  -0.73206234f, -0.25059855f,
  -0.53965491f, -0.77412808f,  -0.27824122f, 0.26692495f,  -0.41266707f,
  -0.15925130f, -0.77606577f,  -0.42885864f, -1.79122651f, -1.98211825f,
  -2.13811111f, -2.27496099f,  0.02845302f,  -1.43462145f, -1.67414725f,
  -1.68713295f, -2.30566549f,  -1.63063061f, -1.94002640f, -2.30850697f,
  -2.27252865f, -2.05349064f,  1.57358956f,  -0.84293872f, -0.67026788f,
  -0.36590576f, 0.00732946f,   -1.78258979f, -0.03746874f, -0.57257193f,
  0.17116521f,  -0.79355401f,  -0.74386477f, -1.42072535f, -1.61911213f,
  -3.26982546f, 0.77972859f,   -0.71090490f, -0.18702148f, -1.59927535f,
  -0.01843349f, -0.47373638f,  1.18899059f,  -0.54764712f, -0.25994197f,
  -1.60281837f, -0.95346391f,  -1.99469924f, -3.11177778f, -1.22362888f,
  -2.93521404f, -0.76245683f,  -1.47658098f, -3.29232502f, -1.13874686f,
  -3.29918957f, -2.00906301f,  -2.09293914f, -2.71770501f, -2.34981585f,
  -2.22041368f, -0.40968072f,  -0.04421870f, -0.08389571f, -0.33121794f,
  0.03904647f,  -0.20723945f,  -0.07148035f, -0.12424246f, -0.11284031f,
  -0.32323083f, -0.50810808f,  0.02136525f,  -0.04755452f, -0.03495969f,
  0.04544474f,  -0.56383401f,  0.19065890f,  0.10168589f,  -0.37838408f,
  -0.13657948f, -0.25745562f,  -0.02159294f, -0.40112621f, -0.38554782f,
  0.10332856f,  -0.41718456f,  -0.26752931f, -0.37573954f, -0.18383676f,
  -0.25614542f, -0.38737795f,  -0.06835440f, 0.05612886f,  -0.03641499f,
  -0.04897336f, 0.05466718f,   -0.07656178f, -0.01457837f, -0.37258980f,
  -0.56241089f, -0.33274400f,  0.80351615f,  1.63614178f,  0.92618144f,
  0.27213046f,  0.17515330f,   -0.18749511f, 0.20447215f,  -0.06538982f,
  0.60279638f,  1.02056050f,   0.96651769f,  0.30594927f,  -0.12677683f,
  0.62678844f,  0.14303780f,   0.14967024f,  0.17635408f,  0.86288977f,
  0.61769640f,  0.17471129f,   0.06851760f,  -0.12891319f, -0.59147525f,
  0.29809082f,  -0.56252706f,  -0.40332860f, 0.37940052f,  0.28126720f,
  0.76832980f,  0.31018534f,   0.14555162f,  1.28664815f,  0.26686695f,
  0.27992848f,  0.35287923f,   0.45072189f,  0.60338104f,
};
static const float intra_y_mode_layer0_bias[EM_MAX_NODES] = {
  -0.24560522f, -0.79116881f, -0.47544709f, -0.63300556f,
  0.42558470f,  -0.46104285f, 0.10670213f,  0.29043028f,
  -0.25997034f, 0.55744708f,  -0.15084648f, 0.92882532f,
  -0.22239538f, -0.24566145f, -0.30977240f, 0.00175375f,
};
static const float intra_y_mode_layer1_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  -0.12866934f, -0.10941806f, 0.78816420f,  -0.01823271f, 0.13654146f,
  -0.19293427f, -0.10828722f, 0.45242783f,  0.17006442f,  0.08048618f,
  -0.14254619f, -0.15443747f, -0.62864536f, -1.85453272f, -0.08233352f,
  0.27988368f,  -0.72773927f, -0.03602586f, -0.24358675f, 0.38964480f,
  0.71451563f,  -0.03054424f, -0.34524295f, -0.20176093f, -0.10723791f,
  0.52807254f,  0.15299982f,  0.43199983f,  -0.05398680f, -2.85947895f,
  -0.03950660f, 0.89936638f,  0.11222982f,  0.75170791f,  -1.24566031f,
  0.36745530f,  -0.26809889f, -0.24064636f, 0.07265630f,  0.99856067f,
  0.02120079f,  0.66546482f,  -0.09439635f, -0.12466125f, -0.56591547f,
  -1.60015237f, 0.25927788f,  0.09283152f,  0.00432288f,  -0.02909680f,
  -0.72984666f, -0.95569754f, 1.12233865f,  -0.73104835f, 0.20121717f,
  0.68497014f,  -0.30681831f, -0.15108538f, 0.15539958f,  -0.38845369f,
  -0.09379038f, -1.13714552f, 0.24007492f,  0.14498658f,  -0.47328705f,
  -0.38462457f, -0.57606649f, -1.32744324f, -0.04647223f, 0.00889751f,
  0.60021585f,  0.04383603f,  -0.58205599f, -0.01011064f, 0.25521109f,
  -0.76818776f, -0.88303405f, -1.73317766f, -0.22053948f, 0.97651410f,
  -0.65360904f, 0.08315254f,  -1.06781065f, -0.39658937f, 0.27888799f,
  -0.37365144f, -0.21611187f, -0.25263256f, -0.34808311f, 0.23793310f,
  -0.06312750f, -0.56291354f, -1.05248296f, -1.93804634f, -0.48632118f,
  1.27603471f,  -0.57281822f, -0.95765394f, -0.59266877f, 0.08777265f,
  -0.30527663f, -0.22658594f, 0.69228935f,  0.96983385f,  -0.24438104f,
  -0.31844756f, -0.36455259f, -0.54692990f, -0.76766235f, -2.16712856f,
  0.25359720f,  0.37397265f,  -0.32310319f, 0.47387955f,  -0.65325302f,
  0.54036009f,  0.29149750f,  -0.45319521f, -0.10031398f, 1.49635625f,
  -0.47643268f, -0.34082723f, 0.14192954f,  -0.22503275f, -0.79819703f,
  -0.39438763f, 0.23093644f,  0.40362725f,  -0.11169641f, -0.22510320f,
  -0.56856191f, -1.46960664f, 1.25765169f,  -0.21893220f, 0.03850168f,
  -0.14129627f, -0.28361130f, 0.48052990f,  -0.45908988f, -0.35332558f,
  0.06339334f,  -1.41138911f, -0.09044325f, 0.47887143f,  0.44952404f,
  -0.41209242f, -0.71412331f, 0.09881080f,  0.30625165f,  0.33509856f,
  -0.18840769f, 0.43447670f,  -0.18100876f, 0.11858132f,  -0.24733737f,
  -0.22767016f, -0.05256226f, -1.66609788f, -0.27894202f, 0.34209484f,
  -0.43505368f, 0.04486898f,  -0.52303851f, 0.01344791f,  0.53621799f,
  -0.45152330f, -0.25292349f, 0.16017683f,  -0.22264160f, 0.29818246f,
  0.02721108f,  -0.01830507f, -0.66462940f, -0.58196497f, -0.40863588f,
  0.49606630f,  0.12200271f,  -0.61697334f, -0.82078922f, 0.64616096f,
  0.09986723f,  0.28397635f,  -0.09165329f, 0.83687824f,  -0.31587797f,
  0.05776133f,  -0.39087692f, -0.27794957f, -0.78622723f, -0.79831588f,
  -0.00045928f, 0.14394432f,  -0.03447319f, -0.04910774f, -0.95224953f,
  -1.03382027f, -0.12115201f, 0.09992490f,  0.28975016f,  0.10087698f,
  0.32233241f,  0.50984049f,  0.29606858f,  0.77050823f,  0.96349055f,
  -0.10954856f, -0.27029502f, -0.25025713f,
};
static const float intra_y_mode_layer1_bias[EM_MAX_NODES] = {
  1.98832178f,  -1.24645507f, -0.02891726f, -1.95236504f, -0.97414410f,
  -1.25962698f, -0.82913953f, -1.60418522f, -1.47659051f, 0.55838144f,
  -0.90775734f, -0.47331291f, -0.62228924f,
};
#endif  // INTRA_MODEL

#if INTRA_MODEL < 0
#if QNN
static const int intra_uv_mode_qlayer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  127,   -40,   374,   -81,   459,   235,   651,   351,   515,   131,   113,
  798,   65,    195,   -59,   423,   40,    -521,  761,   -896,  -724,  61,
  -161,  -287,  -131,  252,   -128,  763,   672,   628,   -1472, -453,  17,
  567,   98,    -536,  -785,  -552,  -709,  84,    -404,  -27,   520,   438,
  -179,  -472,  -614,  0,     41,    -1094, 1510,  -2371, -1840, -2147, -1052,
  -2286, -2285, -1172, -1251, -936,  -1294, -950,  -402,  -310,  -438,  -308,
  -112,  181,   535,   -369,  57,    -611,  -647,  -577,  -770,  -789,  -278,
  604,   -31,   165,   -822,  -2371, 1427,  -2086, -2058, -2306, -927,  -1102,
  -2328, -1200, -1385, -759,  -1096, 747,   165,   -781,  -984,  -1078, -1031,
  -1022, -119,  -467,  362,   79,    -603,  -730,  -797,  -801,  -947,  20,
  85,    -882,  -1593, -1680, 1939,  -1668, -1674, -1502, -97,   31,    -423,
  -600,  -402,  -1498, -405,  -467,  -478,  702,   556,   358,   -762,  -612,
  -224,  -465,  -511,  -903,  351,   783,   811,   -715,  6,     359,   -582,
  -1652, -1901, -1149, 2023,  42,    -90,   -1513, -1346, -1086, -959,  -1026,
  -1924, -628,  -478,  -76,   1112,  247,   -413,  -912,  -786,  -269,  -624,
  -337,  -563,  1224,  766,   -66,   -1093, 19,    149,   -833,  -848,  -2011,
  -1382, -247,  1981,  -1545, -1750, -1409, -879,  -825,  -971,  -1827, -681,
  -680,  -825,  -235,  345,   982,   -499,  -750,  -286,  -779,  -688,  -958,
  -617,  -2,    704,   -208,  -48,   597,   -557,  -1846, -1148, -1550, -129,
  -1476, 2127,  -1256, -1766, -1069, -1010, -905,  -1892, 279,   -514,  -975,
  -700,  -404,  54,    -44,   1043,  -571,  761,   -245,  -1026, -544,  -162,
  219,   320,   -27,   -1195, -294,  -1667, -361,  -172,  -1209, -1587, -1099,
  1539,  -1103, -97,   -343,  -135,  -1606, 236,   1058,  -208,  -676,  -900,
  -902,  -1018, -525,  -166,  15,    600,   -374,  -357,  -454,  -559,  -941,
  14,    191,   -965,  -934,  -1893, 251,   -1789, -1646, -1580, -1203, 1836,
  -403,  -375,  -572,  -1632, 132,   172,   108,   283,   70,    183,   -210,
  154,   -118,  243,   194,   -199,  200,   173,   338,   58,    -5,    447,
  396,   -548,  -817,  161,   -445,  -310,  -286,  203,   65,    737,   456,
  444,   -1197, -392,  -192,  -147,  -198,  -519,  -441,  -341,  -313,  -280,
  -148,  8,     -483,  -198,  -377,  -318,  -360,  14,    698,   -3,    -1427,
  294,   -632,  -1244, -1164, -649,  -41,   -1043, 96,    1028,  -181,  -722,
  -182,  -224,  -117,  -211,  -358,  -264,  -467,  -112,  -382,  -327,  -285,
  -151,  -289,  -365,  -198,  -61,   -17,   406,   -45,   661,   -1397, -381,
  -1177, -605,  -1062, -1008, -158,  122,   -277,  922,   -758,  -213,  -66,
  488,   -238,  -422,  -447,  255,   -225,  5,     -296,  -53,   287,   -408,
  -554,  -480,  431,   -73,   17,    -246,  -792,  -907,  -1296, -2191, -1793,
  -1821, -1320, -1065, -526,  -137,  -398,  1658,  355,   311,   -457,  310,
  387,   523,   -190,  600,   56,    432,   515,   -335,  399,   468,   480,
  -255,  58,    -280,  276,   -315,  -175,  220,   -280,  -158,  -64,   240,
  19,    346,   240,   360,   -320
};
static const int intra_uv_mode_qlayer0_bias[EM_MAX_NODES] = {
  419, 148, 322, -584, -367, -424, -444, -181, -445, 108, -327, -283, 22, 168
};
#else
static const float intra_uv_mode_layer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  0.12402871f,  -0.03929875f, 0.36596400f,  -0.07938571f, 0.44878167f,
  0.23021385f,  0.63632131f,  0.34315693f,  0.50308263f,  0.12867078f,
  0.11130674f,  0.77988297f,  0.06417712f,  0.19077560f,  -0.05831737f,
  0.41384721f,  0.03985662f,  -0.50893170f, 0.74363548f,  -0.87550986f,
  -0.70711148f, 0.06032980f,  -0.15738775f, -0.28071937f, -0.12878937f,
  0.24673125f,  -0.12501241f, 0.74580938f,  0.65641356f,  0.61332113f,
  -1.43825436f, -0.44283772f, 0.01680536f,  0.55390954f,  0.09599128f,
  -0.52435088f, -0.76743245f, -0.53968322f, -0.69289470f, 0.08288545f,
  -0.39513123f, -0.02687692f, 0.50821048f,  0.42811826f,  -0.17532572f,
  -0.46137205f, -0.60033745f, -0.00085125f, 0.04058453f,  -1.06848705f,
  1.47544098f,  -2.31605601f, -1.79731369f, -2.09757209f, -1.02734685f,
  -2.23253942f, -2.23165131f, -1.14544523f, -1.22204721f, -0.91479164f,
  -1.26433229f, -0.92837995f, -0.39286733f, -0.30306965f, -0.42797801f,
  -0.30088520f, -0.10959357f, 0.17686196f,  0.52316642f,  -0.36124781f,
  0.05588505f,  -0.59701741f, -0.63227695f, -0.56406784f, -0.75258392f,
  -0.77053577f, -0.27200893f, 0.59032679f,  -0.03034093f, 0.16183151f,
  -0.80340558f, -2.31553340f, 1.39446807f,  -2.03798532f, -2.01002455f,
  -2.25249100f, -0.90592390f, -1.07636273f, -2.27414083f, -1.17200267f,
  -1.35266638f, -0.74139255f, -1.07088172f, 0.73039442f,  0.16119771f,
  -0.76310480f, -0.96189302f, -1.05289102f, -1.00757074f, -0.99881727f,
  -0.11687569f, -0.45693684f, 0.35436177f,  0.07783467f,  -0.58931023f,
  -0.71314168f, -0.77839643f, -0.78279990f, -0.92483133f, 0.02011398f,
  0.08395430f,  -0.86226541f, -1.55594468f, -1.64099336f, 1.89452517f,
  -1.62969279f, -1.63517904f, -1.46759641f, -0.09494729f, 0.03036384f,
  -0.41313040f, -0.58596551f, -0.39259049f, -1.46383548f, -0.39568818f,
  -0.45622265f, -0.46692327f, 0.68605477f,  0.54392606f,  0.34966618f,
  -0.74424416f, -0.59829086f, -0.21916671f, -0.45480689f, -0.49943250f,
  -0.88192856f, 0.34320757f,  0.76537216f,  0.79285985f,  -0.69903576f,
  0.00622503f,  0.35139444f,  -0.56905264f, -1.61392093f, -1.85691237f,
  -1.12237167f, 1.97576988f,  0.04136098f,  -0.08860544f, -1.47833121f,
  -1.31485868f, -1.06064928f, -0.93718624f, -1.00250280f, -1.87949467f,
  -0.61374307f, -0.46771994f, -0.07473613f, 1.08672249f,  0.24196818f,
  -0.40356553f, -0.89119226f, -0.76812911f, -0.26301232f, -0.60971504f,
  -0.32932544f, -0.55018377f, 1.19607866f,  0.74857402f,  -0.06499760f,
  -1.06821609f, 0.01912315f,  0.14584364f,  -0.81420678f, -0.82892340f,
  -1.96423936f, -1.35031235f, -0.24165779f, 1.93522048f,  -1.50907385f,
  -1.70906579f, -1.37617683f, -0.85869962f, -0.80608046f, -0.94876856f,
  -1.78426480f, -0.66552210f, -0.66422945f, -0.80660295f, -0.22981134f,
  0.33727181f,  0.95961803f,  -0.48783514f, -0.73268259f, -0.27936959f,
  -0.76101077f, -0.67258501f, -0.93628287f, -0.60258675f, -0.00272783f,
  0.68773162f,  -0.20395121f, -0.04751784f, 0.58397341f,  -0.54445702f,
  -1.80279756f, -1.12185979f, -1.51423383f, -0.12644707f, -1.44236457f,
  2.07753921f,  -1.22657681f, -1.72536922f, -1.04404104f, -0.98728126f,
  -0.88415390f, -1.84825969f, 0.27328718f,  -0.50196368f, -0.95237064f,
  -0.68409699f, -0.39534184f, 0.05320294f,  -0.04390695f, 1.01894093f,
  -0.55822754f, 0.74350286f,  -0.23975432f, -1.00195479f, -0.53202486f,
  -0.15851398f, 0.21430893f,  0.31251612f,  -0.02665514f, -1.16739750f,
  -0.28757134f, -1.62885773f, -0.35271245f, -0.16836807f, -1.18135738f,
  -1.55052674f, -1.07405770f, 1.50361967f,  -1.07755756f, -0.09559821f,
  -0.33561242f, -0.13259739f, -1.56852305f, 0.23106579f,  1.03378236f,
  -0.20361935f, -0.66109818f, -0.87950730f, -0.88108718f, -0.99435139f,
  -0.51316780f, -0.16286118f, 0.01503937f,  0.58680403f,  -0.36591315f,
  -0.34939125f, -0.44371462f, -0.54622632f, -0.91966009f, 0.01447169f,
  0.18713856f,  -0.94325054f, -0.91258132f, -1.84881938f, 0.24574322f,
  -1.74782979f, -1.60744739f, -1.54314959f, -1.17566526f, 1.79356802f,
  -0.39355996f, -0.36641598f, -0.55901676f, -1.59391093f, 0.12926508f,
  0.16884325f,  0.10592209f,  0.27715158f,  0.06862926f,  0.17930977f,
  -0.20525105f, 0.15093988f,  -0.11532474f, 0.23779586f,  0.19000189f,
  -0.19506425f, 0.19556873f,  0.16957068f,  0.33085167f,  0.05738912f,
  -0.00513178f, 0.43703225f,  0.38703507f,  -0.53575063f, -0.79800367f,
  0.15800059f,  -0.43463072f, -0.30366394f, -0.27936623f, 0.19909428f,
  0.06395348f,  0.72007459f,  0.44546968f,  0.43417883f,  -1.16897595f,
  -0.38377044f, -0.18801978f, -0.14442980f, -0.19399375f, -0.50703061f,
  -0.43088287f, -0.33335650f, -0.30657256f, -0.27378196f, -0.14484969f,
  0.00806807f,  -0.47235215f, -0.19406974f, -0.36883724f, -0.31099114f,
  -0.35173556f, 0.01377719f,  0.68213427f,  -0.00383256f, -1.39428616f,
  0.28717321f,  -0.61798871f, -1.21486318f, -1.13749814f, -0.63400376f,
  -0.04090768f, -1.01887870f, 0.09406894f,  1.00427926f,  -0.17728916f,
  -0.70582408f, -0.17833798f, -0.21927579f, -0.11521580f, -0.20701924f,
  -0.34963408f, -0.25786725f, -0.45614982f, -0.10985988f, -0.37317932f,
  -0.31984380f, -0.27913392f, -0.14754701f, -0.28232935f, -0.35689670f,
  -0.19351959f, -0.06043091f, -0.01736251f, 0.39652133f,  -0.04490271f,
  0.64610171f,  -1.36451507f, -0.37264016f, -1.15025878f, -0.59097153f,
  -1.03740013f, -0.98445386f, -0.15429929f, 0.11985902f,  -0.27068526f,
  0.90089488f,  -0.74025238f, -0.20876490f, -0.06502233f, 0.47702166f,
  -0.23324436f, -0.41291025f, -0.43686554f, 0.24976204f,  -0.21991676f,
  0.00498751f,  -0.28980547f, -0.05269759f, 0.28027838f,  -0.39930674f,
  -0.54142779f, -0.46901166f, 0.42130089f,  -0.07192902f, 0.01661034f,
  -0.24098501f, -0.77375114f, -0.88616598f, -1.26581442f, -2.14021277f,
  -1.75161040f, -1.77855253f, -1.28929615f, -1.04018474f, -0.51460266f,
  -0.13399349f, -0.38906997f, 1.61957860f,  0.34722441f,  0.30467570f,
  -0.44711319f, 0.30279619f,  0.37871268f,  0.51116621f,  -0.18622541f,
  0.58653408f,  0.05476259f,  0.42189121f,  0.50298405f,  -0.32771653f,
  0.39003730f,  0.45713833f,  0.46933004f,  -0.24965021f, 0.05674341f,
  -0.27428845f, 0.27018315f,  -0.30800790f, -0.17168075f, 0.21494877f,
  -0.27435708f, -0.15491369f, -0.06341664f, 0.23531802f,  0.01922343f,
  0.33817795f,  0.23492523f,  0.35229778f,  -0.31334904f,
};
static const float intra_uv_mode_layer0_bias[EM_MAX_NODES] = {
  0.41000897f,  0.14504482f,  0.31459823f,  -0.57099664f, -0.35888141f,
  -0.41459736f, -0.43401498f, -0.17684162f, -0.43496770f, 0.10555134f,
  -0.31986049f, -0.27649805f, 0.02212156f,  0.16451526f,
};
#endif  // QNN
#else
static const float intra_uv_mode_layer0_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  -0.88319570f,  -0.96808380f, 0.20029698f,  0.61564237f,  0.62172204f,
  -0.09045262f,  -0.15791443f, -1.14759815f, 0.11905042f,  -1.04120028f,
  -0.65996403f,  0.28801590f,  1.12395680f,  1.18378031f,  0.20501007f,
  -0.18477310f,  0.09667519f,  0.29048321f,  0.01515922f,  0.26054123f,
  -4.19469976f,  -3.39059258f, 0.81853807f,  2.01778913f,  -0.33076349f,
  -3.42551708f,  -2.06261921f, 0.13082494f,  0.38825038f,  -2.65976596f,
  0.24714212f,   -0.04579130f, -0.31328568f, -0.30575961f, -0.81971741f,
  -0.55829024f,  -0.26365274f, 0.34413931f,  0.33507425f,  0.24995944f,
  0.53341740f,   0.22631928f,  -0.00388879f, -0.46096647f, -0.32586515f,
  -0.07379799f,  0.62410456f,  0.01517987f,  -0.73220736f, 0.19064221f,
  -1.58490169f,  0.95073789f,  0.46703771f,  -2.17081356f, -1.67808068f,
  0.07849235f,   1.76847994f,  0.02539463f,  0.40225834f,  0.42804861f,
  0.04480518f,   -2.57151127f, -0.64177513f, -0.84385914f, -0.54859471f,
  -0.94681793f,  -0.56284738f, -0.54281753f, -0.41506147f, -0.71770865f,
  -8.46369267f,  0.25959435f,  0.20999303f,  0.20284574f,  0.67661858f,
  1.11230862f,   0.67176342f,  0.23758931f,  0.02868665f,  -5.08091211f,
  0.78394610f,   -2.62286901f, 0.60085493f,  -2.15743828f, 0.76220000f,
  -2.47915769f,  0.34771714f,  -3.92158675f, -2.38103390f, 0.60398197f,
  0.74221373f,   0.66642582f,  0.68576390f,  -0.26031536f, -0.16259514f,
  0.00626599f,   -0.04344397f, -0.13435313f, -0.39726481f, -0.29721159f,
  -0.39772952f,  0.54582220f,  0.45093155f,  0.37131193f,  0.60661161f,
  0.58540660f,   0.48274145f,  0.43891773f,  0.44966295f,  0.00316198f,
  -0.22214095f,  -3.14298654f, 1.84436059f,  -2.12393045f, -2.40356922f,
  -1.62165976f,  -1.54818773f, -2.13695812f, -3.00800323f, -2.89923167f,
  -3.36073422f,  -1.70435619f, -3.89171672f, -1.66683042f, -0.07692014f,
  -0.17174110f,  0.03574500f,  0.32355663f,  -0.26351219f, -0.33742237f,
  -0.23081017f,  -0.56055886f, -0.07266840f, -0.07938100f, -0.43191445f,
  -0.44414663f,  -0.06052558f, -0.19558364f, -0.01235540f, -0.07464408f,
  -0.07529971f,  0.45877159f,  -0.35245228f, -0.84046268f, -1.26309180f,
  -0.35011590f,  2.66011667f,  2.04182124f,  0.01893831f,  1.74980438f,
  -0.85087299f,  -0.49009171f, 0.80659914f,  -0.34819573f, -0.78016794f,
  -0.30772135f,  -0.01237415f, -0.07515348f, 0.16856404f,  0.32003865f,
  -0.14894979f,  -0.16741478f, -0.53264928f, -0.22175848f, -0.70033181f,
  -0.56369811f,  -0.34550425f, -0.46054071f, -0.53182226f, -0.54904640f,
  -0.21694821f,  -0.02328763f, -0.14613205f, -0.72275001f, -1.08431911f,
  1.59791660f,   -1.45981109f, -1.47759938f, -1.69571006f, -1.38061333f,
  -1.01171958f,  -1.65563750f, -0.95000637f, 0.62908906f,  -1.75649190f,
  2.90227795f,   0.21568698f,  -0.19273494f, -0.12452848f, -0.60654324f,
  -0.50994289f,  -0.59578800f, -0.34451240f, -0.43971911f, -0.18087673f,
  -0.02852433f,  -0.34019643f, -0.13934958f, -0.35819992f, -0.27484244f,
  -0.21175560f,  0.01133029f,  -0.01481021f, 0.38088319f,  -1.50686347f,
  -0.80507720f,  -1.90451217f, 2.36175251f,  -0.57633364f, -0.68935561f,
  0.11069413f,   0.86722541f,  1.73493934f,  1.11320627f,  -0.92113101f,
  1.70989132f,   -0.61228067f, -0.58492339f, -0.68956202f, -1.06368601f,
  -0.97350407f,  -0.69726968f, -0.43992543f, -0.50834215f, -0.25905675f,
  -1.51157045f,  -0.21060432f, -0.23577580f, -0.87516445f, -0.32364142f,
  -0.14796841f,  0.04648991f,  -0.43365085f, -1.42344368f, -37.83842850f,
  0.03880321f,   -0.72152847f, -0.05464779f, -0.33020982f, -0.96125841f,
  -1.22758853f,  0.32569689f,  -0.21947299f, -1.35198569f, 0.29508564f,
  -0.14918467f,  0.63290870f,  0.64021742f,  -0.83258981f, -0.85285664f,
  -1.33061206f,  -1.00109708f, -0.96991009f, -0.55695838f, -0.63877457f,
  -0.43650317f,  -2.18803024f, -0.09452824f, -0.06712142f, -0.58739269f,
  0.07133394f,   0.21643221f,  0.46592432f,  -0.35446727f, -1.56468129f,
  -45.46573257f, 0.48253971f,  0.58895499f,  0.27331799f,  -1.53977895f,
  -1.22336328f,  -0.37778169f, -2.04815698f, -3.24189854f, -1.11027181f,
  0.91477036f,   0.65702051f,  0.82277149f,  -2.29022646f, -0.34437189f,
  -0.29037678f,  -0.09947637f, 0.08151767f,  0.57492048f,  0.68178689f,
  0.18124373f,   -0.69049782f, 0.36232242f,  -0.58106399f, -0.50685346f,
  -0.03433619f,  -0.09221681f, 0.52399951f,  0.56477416f,  0.22041254f,
  0.00503419f,   0.57215583f,  0.03849587f,  -1.79147291f, 1.02374482f,
  -1.73018038f,  1.64108479f,  0.22529399f,  1.79331291f,  0.41232949f,
  -1.69565129f,  -0.10622260f, 0.13540123f,  -0.08745020f, -1.67855525f,
  -0.21929306f,  -0.10025978f, -0.60816514f, -1.00439632f, -0.43587336f,
  -0.67009795f,  -0.05523772f, -0.12605444f, 1.07578218f,  0.12909982f,
  0.83184212f,   1.40284884f,  0.07458597f,  -1.88771653f, -4.29449797f,
  -0.75722581f,  0.06688527f,  1.47510135f,  0.60741788f,  -0.12486117f,
  0.02104888f,   -0.03609499f, -0.17981489f, 0.03961810f,  -0.33815551f,
  -2.49199009f,  0.16497555f,  0.14279079f,  0.06625128f,  0.11474069f,
  0.30832717f,   -0.27879667f, -0.50976115f, -0.73528683f, -0.77560687f,
  -0.37146470f,  -0.10000181f, -0.15021020f, -0.13265578f, -1.01957119f,
  -0.42124131f,  -0.34385678f, -0.68712968f, -0.43332288f, -0.43641633f,
  -0.16848743f,  -0.62063724f, -2.37886119f, -3.58489513f, -0.06130514f,
  -0.34500557f,  0.36161456f,  0.06702515f,  -0.23415729f, -0.57417673f,
  -0.29713267f,  -0.01733693f, 0.30799195f,  0.46706688f,  -0.19913588f,
  0.59958768f,   0.11142783f,  -0.35523328f, -0.83854824f, -0.38825333f,
  -0.21066061f,  0.34351075f,  -0.22967348f, -0.31374413f, 0.77534097f,
  -0.09310844f,  0.45447397f,  -3.52565742f, 0.13305941f,  -5.38092756f,
  0.16318570f,   -0.20337565f, 0.57576615f,  0.14739028f,  -13.51003170f,
  0.53855157f,   -1.53120053f, -6.85232067f, 0.19582644f,  0.36748993f,
  -2.89327312f,  0.13576463f,  0.52715683f,  -0.79425639f, 0.48631492f,
  0.41358709f,   0.34086904f,  1.01033723f,  -0.76293683f, -0.38217765f,
  0.56371742f,   -0.64692825f, -1.42412674f, -0.81726575f, 0.54949558f,
  -1.11337709f,  -0.20818788f, -0.86399132f, -0.40174463f, 0.52540857f,
  -0.55601883f,  -1.05591071f, -0.20198363f, 0.84047395f,  -0.07473359f,
  0.22884981f,   0.77278656f,  0.17579235f,  0.31050774f,  -1.72881567f,
  0.12468022f,   -1.89261925f, 0.03944425f,  0.44040102f,  0.67188501f,
  0.76918417f,   0.36162725f,  1.38786483f,  0.10537743f,  -0.20940316f,
  -0.05140867f,  -0.13438898f, -0.19932657f, -0.08577275f, -0.12652662f,
  -0.17514907f,  -0.32072470f, -0.08101110f, -0.37870625f, -0.36095989f,
  -0.25992352f,  -0.26180622f, -0.23751736f, -0.23561344f, -0.29262671f,
  -0.02914349f,  -0.21506962f, -2.28895712f, -1.13885236f, -3.75423908f,
  -1.86791182f,  -0.97013593f, 1.00838423f,  -2.60359049f, -1.80958748f,
  1.08668435f,   -2.35276628f, -1.70901072f, -2.37556815f, 1.85696185f,
  0.17416890f,   0.54309916f,  0.22596854f,  -0.48490116f, -0.70737112f,
  -0.69260639f,  -0.27813095f, -0.23798716f, -0.04330804f, 0.27957809f,
  0.38119477f,   -0.07441421f, -0.36816972f, -0.28221726f, -0.21936646f,
  -0.12274464f,  -0.01543404f, -0.13423836f, 0.40842748f,  0.43226993f,
  -1.80917013f,  1.55415320f,  0.35635498f,  -1.11825001f, -0.68384320f,
  1.29726064f,   2.00753736f,  0.89246500f,  1.32452369f,  0.31665304f,
  1.21811831f,
};
static const float intra_uv_mode_layer0_bias[EM_MAX_NODES] = {
  0.37351567f,  0.18207699f,  0.16914076f,  -0.28013960f,
  -0.65980566f, 0.36943561f,  -0.35009328f, -0.56021720f,
  -0.68889558f, 0.05403420f,  0.25728512f,  -0.77400327f,
  0.21643718f,  -0.05055284f, 0.11696668f,  -0.16335754f,
};
static const float intra_uv_mode_layer1_weights[EM_MAX_NODES * EM_MAX_NODES] = {
  0.61191076f,  0.27767769f,  0.72925180f,  -0.34926373f, -0.18989867f,
  -0.60676491f, -0.27112475f, 0.01455507f,  0.29065996f,  0.33891961f,
  0.38369554f,  -0.36820170f, 0.50457948f,  0.22499098f,  -0.51947880f,
  0.27089241f,  0.65973097f,  -0.92342782f, 0.08520066f,  1.44300151f,
  0.20998508f,  -0.19098234f, -0.85297227f, -0.66294032f, -0.01798616f,
  -0.65565389f, 0.25698712f,  -0.15516500f, -0.16048834f, 0.31126237f,
  -0.12637049f, 0.63711441f,  -0.21124816f, 0.72423887f,  -0.02916464f,
  -0.62500888f, -0.18622933f, 1.12045741f,  -0.41754135f, -0.24280116f,
  -0.03926597f, 0.56292480f,  -0.05640042f, -0.03515873f, -0.23159045f,
  0.38734159f,  -1.07011294f, -1.00329661f, 0.07895157f,  0.23558487f,
  -0.54938501f, -0.27765220f, 0.53784519f,  -0.11479253f, 1.16833651f,
  -1.87919867f, -1.12115920f, -0.42343184f, 0.29190943f,  -0.16286966f,
  0.08157352f,  -0.74606895f, -1.55427110f, 0.78449982f,  0.29257107f,
  -1.03701031f, 0.15114070f,  -0.58641666f, 1.21128488f,  -0.63668728f,
  -0.40426171f, -0.91975200f, -0.11259757f, 0.71098697f,  -0.31331834f,
  -0.03643197f, -0.00273548f, -0.40376398f, -0.91835010f, 0.03041170f,
  1.15596724f,  -0.67125064f, -0.27772480f, -0.15022194f, 0.75657964f,
  -0.85402870f, -0.43998313f, -1.44768548f, -0.77602017f, -0.43796626f,
  -0.19989780f, -0.16280027f, -0.27114141f, -0.49223852f, 0.33561775f,
  -0.17384775f, -0.31759727f, -0.41789585f, -0.35567087f, -0.68441057f,
  -0.59401214f, -1.12404180f, -0.95974898f, -1.02861512f, -0.66321474f,
  1.72527730f,  -0.35586202f, -0.29363146f, -0.10123435f, -0.12484195f,
  0.36951596f,  -0.25635532f, -0.43637925f, 1.20126057f,  0.77468395f,
  -0.27186784f, 0.80126858f,  -0.31046757f, 0.10387573f,  1.98786652f,
  2.21821070f,  -0.12345061f, -0.49153081f, 1.43069029f,  0.29578710f,
  -0.46890774f, -0.04196711f, -0.27824146f, -0.23031458f, -0.48184586f,
  -0.46968010f, 0.13611336f,  -0.11191103f, -1.11462748f, -0.13555850f,
  -1.23152292f, -0.15275079f, 0.01788915f,  0.15711573f,  -0.09180369f,
  -0.56679696f, -0.17855860f, 0.75494772f,  1.40256977f,  0.34949023f,
  -0.03565224f, -0.25840601f, -0.33906808f, 0.05003415f,  -0.47190619f,
  -0.32756346f, -0.10165177f, 0.35836589f,  0.04052243f,  -0.19110575f,
  0.07493998f,  0.00610683f,  0.03043868f,  -0.56074232f, 0.34847644f,
  0.43969023f,  0.10328959f,  -0.29127949f, -0.60716885f, -0.13185447f,
  0.49813518f,  -0.76086164f, -1.22769380f, -0.35345691f, 0.07742767f,
  -0.04720307f, -0.55542499f, -0.21192645f, 0.08106764f,  -1.67480206f,
  0.61635768f,  0.19012810f,  -0.22988044f, -0.23699653f, 0.54334688f,
  0.12743533f,  -0.31004998f, 0.17684972f,  -0.76373714f, -0.06825176f,
  -0.17324458f, -0.09321886f, -0.15467811f, 0.02089228f,  0.31863856f,
  0.03003708f,  -0.17773160f, 0.21156009f,  -0.13384010f, 0.06267833f,
  0.11660756f,  -0.26549768f, 0.56745249f,  -0.86303264f, -0.27105144f,
  0.12287134f,  -0.30826381f, 0.07714237f,  0.13396999f,  0.25519139f,
  0.65956694f,  0.03934136f,  0.23277369f,  0.08715345f,  0.19726141f,
  0.19471689f,  -0.13259538f, 0.21168733f,  -0.30168825f, -0.02062701f,
  -0.75379980f, 0.38113356f,  0.15173414f,  -0.08904698f, 0.04091260f,
  -0.25273520f, -0.53353715f, 0.10138044f,  -0.08994483f,
};
static const float intra_uv_mode_layer1_bias[EM_MAX_NODES] = {
  0.87762195f,  -1.17760456f, -1.05695379f, -2.35511160f, -0.88644844f,
  -1.21162772f, -1.27903116f, -0.90914112f, -1.65604413f, 0.83844930f,
  -0.86388034f, -0.52990150f, -0.53841406f, 0.96045518f,
};
#endif  // INTRA_MODEL
#endif  // CONFIG_INTRA_ENTROPY

static const aom_cdf_prob default_angle_delta_cdf[DIRECTIONAL_MODES][CDF_SIZE(
    2 * MAX_ANGLE_DELTA + 1)] = {
  { AOM_CDF7(2180, 5032, 7567, 22776, 26989, 30217) },
  { AOM_CDF7(2301, 5608, 8801, 23487, 26974, 30330) },
  { AOM_CDF7(3780, 11018, 13699, 19354, 23083, 31286) },
  { AOM_CDF7(4581, 11226, 15147, 17138, 21834, 28397) },
  { AOM_CDF7(1737, 10927, 14509, 19588, 22745, 28823) },
  { AOM_CDF7(2664, 10176, 12485, 17650, 21600, 30495) },
  { AOM_CDF7(2240, 11096, 15453, 20341, 22561, 28917) },
  { AOM_CDF7(3605, 10428, 12459, 17676, 21244, 30655) }
};

static const aom_cdf_prob default_if_y_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
    INTRA_MODES)] = { { AOM_CDF13(22801, 23489, 24293, 24756, 25601, 26123,
                                  26606, 27418, 27945, 29228, 29685, 30349) },
                      { AOM_CDF13(18673, 19845, 22631, 23318, 23950, 24649,
                                  25527, 27364, 28152, 29701, 29984, 30852) },
                      { AOM_CDF13(19770, 20979, 23396, 23939, 24241, 24654,
                                  25136, 27073, 27830, 29360, 29730, 30659) },
                      { AOM_CDF13(20155, 21301, 22838, 23178, 23261, 23533,
                                  23703, 24804, 25352, 26575, 27016, 28049) } };

static const aom_cdf_prob
    default_uv_mode_cdf[CFL_ALLOWED_TYPES][INTRA_MODES][CDF_SIZE(
        UV_INTRA_MODES)] = {
      { { AOM_CDF13(22631, 24152, 25378, 25661, 25986, 26520, 27055, 27923,
                    28244, 30059, 30941, 31961) },
        { AOM_CDF13(9513, 26881, 26973, 27046, 27118, 27664, 27739, 27824,
                    28359, 29505, 29800, 31796) },
        { AOM_CDF13(9845, 9915, 28663, 28704, 28757, 28780, 29198, 29822, 29854,
                    30764, 31777, 32029) },
        { AOM_CDF13(13639, 13897, 14171, 25331, 25606, 25727, 25953, 27148,
                    28577, 30612, 31355, 32493) },
        { AOM_CDF13(9764, 9835, 9930, 9954, 25386, 27053, 27958, 28148, 28243,
                    31101, 31744, 32363) },
        { AOM_CDF13(11825, 13589, 13677, 13720, 15048, 29213, 29301, 29458,
                    29711, 31161, 31441, 32550) },
        { AOM_CDF13(14175, 14399, 16608, 16821, 17718, 17775, 28551, 30200,
                    30245, 31837, 32342, 32667) },
        { AOM_CDF13(12885, 13038, 14978, 15590, 15673, 15748, 16176, 29128,
                    29267, 30643, 31961, 32461) },
        { AOM_CDF13(12026, 13661, 13874, 15305, 15490, 15726, 15995, 16273,
                    28443, 30388, 30767, 32416) },
        { AOM_CDF13(19052, 19840, 20579, 20916, 21150, 21467, 21885, 22719,
                    23174, 28861, 30379, 32175) },
        { AOM_CDF13(18627, 19649, 20974, 21219, 21492, 21816, 22199, 23119,
                    23527, 27053, 31397, 32148) },
        { AOM_CDF13(17026, 19004, 19997, 20339, 20586, 21103, 21349, 21907,
                    22482, 25896, 26541, 31819) },
        { AOM_CDF13(12124, 13759, 14959, 14992, 15007, 15051, 15078, 15166,
                    15255, 15753, 16039, 16606) } },
      { { AOM_CDF14(10407, 11208, 12900, 13181, 13823, 14175, 14899, 15656,
                    15986, 20086, 20995, 22455, 24212) },
        { AOM_CDF14(4532, 19780, 20057, 20215, 20428, 21071, 21199, 21451,
                    22099, 24228, 24693, 27032, 29472) },
        { AOM_CDF14(5273, 5379, 20177, 20270, 20385, 20439, 20949, 21695, 21774,
                    23138, 24256, 24703, 26679) },
        { AOM_CDF14(6740, 7167, 7662, 14152, 14536, 14785, 15034, 16741, 18371,
                    21520, 22206, 23389, 24182) },
        { AOM_CDF14(4987, 5368, 5928, 6068, 19114, 20315, 21857, 22253, 22411,
                    24911, 25380, 26027, 26376) },
        { AOM_CDF14(5370, 6889, 7247, 7393, 9498, 21114, 21402, 21753, 21981,
                    24780, 25386, 26517, 27176) },
        { AOM_CDF14(4816, 4961, 7204, 7326, 8765, 8930, 20169, 20682, 20803,
                    23188, 23763, 24455, 24940) },
        { AOM_CDF14(6608, 6740, 8529, 9049, 9257, 9356, 9735, 18827, 19059,
                    22336, 23204, 23964, 24793) },
        { AOM_CDF14(5998, 7419, 7781, 8933, 9255, 9549, 9753, 10417, 18898,
                    22494, 23139, 24764, 25989) },
        { AOM_CDF14(10660, 11298, 12550, 12957, 13322, 13624, 14040, 15004,
                    15534, 20714, 21789, 23443, 24861) },
        { AOM_CDF14(10522, 11530, 12552, 12963, 13378, 13779, 14245, 15235,
                    15902, 20102, 22696, 23774, 25838) },
        { AOM_CDF14(10099, 10691, 12639, 13049, 13386, 13665, 14125, 15163,
                    15636, 19676, 20474, 23519, 25208) },
        { AOM_CDF14(3144, 5087, 7382, 7504, 7593, 7690, 7801, 8064, 8232, 9248,
                    9875, 10521, 29048) } }
    };

static const aom_cdf_prob default_partition_cdf[PARTITION_CONTEXTS][CDF_SIZE(
    EXT_PARTITION_TYPES)] = {
  { AOM_CDF4(19132, 25510, 30392) },
  { AOM_CDF4(13928, 19855, 28540) },
  { AOM_CDF4(12522, 23679, 28629) },
  { AOM_CDF4(9896, 18783, 25853) },
  { AOM_CDF10(15597, 20929, 24571, 26706, 27664, 28821, 29601, 30571, 31902) },
  { AOM_CDF10(7925, 11043, 16785, 22470, 23971, 25043, 26651, 28701, 29834) },
  { AOM_CDF10(5414, 13269, 15111, 20488, 22360, 24500, 25537, 26336, 32117) },
  { AOM_CDF10(2662, 6362, 8614, 20860, 23053, 24778, 26436, 27829, 31171) },
  { AOM_CDF10(18462, 20920, 23124, 27647, 28227, 29049, 29519, 30178, 31544) },
  { AOM_CDF10(7689, 9060, 12056, 24992, 25660, 26182, 26951, 28041, 29052) },
  { AOM_CDF10(6015, 9009, 10062, 24544, 25409, 26545, 27071, 27526, 32047) },
  { AOM_CDF10(1394, 2208, 2796, 28614, 29061, 29466, 29840, 30185, 31899) },
  { AOM_CDF10(20137, 21547, 23078, 29566, 29837, 30261, 30524, 30892, 31724) },
  { AOM_CDF10(6732, 7490, 9497, 27944, 28250, 28515, 28969, 29630, 30104) },
  { AOM_CDF10(5945, 7663, 8348, 28683, 29117, 29749, 30064, 30298, 32238) },
  { AOM_CDF10(870, 1212, 1487, 31198, 31394, 31574, 31743, 31881, 32332) },
  { AOM_CDF8(27899, 28219, 28529, 32484, 32539, 32619, 32639) },
  { AOM_CDF8(6607, 6990, 8268, 32060, 32219, 32338, 32371) },
  { AOM_CDF8(5429, 6676, 7122, 32027, 32227, 32531, 32582) },
  { AOM_CDF8(711, 966, 1172, 32448, 32538, 32617, 32664) },
};

#if CONFIG_MODE_DEP_TX
static const aom_cdf_prob default_intra_ext_tx_cdf
    [EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES]
    [CDF_SIZE(TX_TYPES_NOMDTX)] = {
#else
static const aom_cdf_prob default_intra_ext_tx_cdf
    [EXT_TX_SETS_INTRA][EXT_TX_SIZES][INTRA_MODES][CDF_SIZE(TX_TYPES)] = {
#endif
      {
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
          {
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
              { 0 },
          },
      },
      {
          {
              { AOM_CDF7(1535, 8035, 9461, 12751, 23467, 27825) },
              { AOM_CDF7(564, 3335, 9709, 10870, 18143, 28094) },
              { AOM_CDF7(672, 3247, 3676, 11982, 19415, 23127) },
              { AOM_CDF7(5279, 13885, 15487, 18044, 23527, 30252) },
              { AOM_CDF7(4423, 6074, 7985, 10416, 25693, 29298) },
              { AOM_CDF7(1486, 4241, 9460, 10662, 16456, 27694) },
              { AOM_CDF7(439, 2838, 3522, 6737, 18058, 23754) },
              { AOM_CDF7(1190, 4233, 4855, 11670, 20281, 24377) },
              { AOM_CDF7(1045, 4312, 8647, 10159, 18644, 29335) },
              { AOM_CDF7(202, 3734, 4747, 7298, 17127, 24016) },
              { AOM_CDF7(447, 4312, 6819, 8884, 16010, 23858) },
              { AOM_CDF7(277, 4369, 5255, 8905, 16465, 22271) },
              { AOM_CDF7(3409, 5436, 10599, 15599, 19687, 24040) },
          },
          {
              { AOM_CDF7(1870, 13742, 14530, 16498, 23770, 27698) },
              { AOM_CDF7(326, 8796, 14632, 15079, 19272, 27486) },
              { AOM_CDF7(484, 7576, 7712, 14443, 19159, 22591) },
              { AOM_CDF7(1126, 15340, 15895, 17023, 20896, 30279) },
              { AOM_CDF7(655, 4854, 5249, 5913, 22099, 27138) },
              { AOM_CDF7(1299, 6458, 8885, 9290, 14851, 25497) },
              { AOM_CDF7(311, 5295, 5552, 6885, 16107, 22672) },
              { AOM_CDF7(883, 8059, 8270, 11258, 17289, 21549) },
              { AOM_CDF7(741, 7580, 9318, 10345, 16688, 29046) },
              { AOM_CDF7(110, 7406, 7915, 9195, 16041, 23329) },
              { AOM_CDF7(363, 7974, 9357, 10673, 15629, 24474) },
              { AOM_CDF7(153, 7647, 8112, 9936, 15307, 19996) },
              { AOM_CDF7(3511, 6332, 11165, 15335, 19323, 23594) },
          },
          {
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
          },
          {
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
              { AOM_CDF7(4681, 9362, 14043, 18725, 23406, 28087) },
          },
      },
      {
          {
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
          },
          {
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
          },
          {
              { AOM_CDF5(1127, 12814, 22772, 27483) },
              { AOM_CDF5(145, 6761, 11980, 26667) },
              { AOM_CDF5(362, 5887, 11678, 16725) },
              { AOM_CDF5(385, 15213, 18587, 30693) },
              { AOM_CDF5(25, 2914, 23134, 27903) },
              { AOM_CDF5(60, 4470, 11749, 23991) },
              { AOM_CDF5(37, 3332, 14511, 21448) },
              { AOM_CDF5(157, 6320, 13036, 17439) },
              { AOM_CDF5(119, 6719, 12906, 29396) },
              { AOM_CDF5(47, 5537, 12576, 21499) },
              { AOM_CDF5(269, 6076, 11258, 23115) },
              { AOM_CDF5(83, 5615, 12001, 17228) },
              { AOM_CDF5(1968, 5556, 12023, 18547) },
          },
          {
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
              { AOM_CDF5(6554, 13107, 19661, 26214) },
          },
      },
    };

#if CONFIG_MODE_DEP_TX
static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(
        TX_TYPES_NOMDTX)] = {
#else
static const aom_cdf_prob
    default_inter_ext_tx_cdf[EXT_TX_SETS_INTER][EXT_TX_SIZES][CDF_SIZE(
        TX_TYPES)] = {
#endif
      {
          { 0 },
          { 0 },
          { 0 },
          { 0 },
      },
      {
          { AOM_CDF16(4458, 5560, 7695, 9709, 13330, 14789, 17537, 20266, 21504,
                      22848, 23934, 25474, 27727, 28915, 30631) },
          { AOM_CDF16(1645, 2573, 4778, 5711, 7807, 8622, 10522, 15357, 17674,
                      20408, 22517, 25010, 27116, 28856, 30749) },
          { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                      20480, 22528, 24576, 26624, 28672, 30720) },
          { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                      20480, 22528, 24576, 26624, 28672, 30720) },
      },
      {
          { AOM_CDF12(2731, 5461, 8192, 10923, 13653, 16384, 19115, 21845,
                      24576, 27307, 30037) },
          { AOM_CDF12(2731, 5461, 8192, 10923, 13653, 16384, 19115, 21845,
                      24576, 27307, 30037) },
          { AOM_CDF12(770, 2421, 5225, 12907, 15819, 18927, 21561, 24089, 26595,
                      28526, 30529) },
          { AOM_CDF12(2731, 5461, 8192, 10923, 13653, 16384, 19115, 21845,
                      24576, 27307, 30037) },
      },
      {
          { AOM_CDF2(16384) },
          { AOM_CDF2(4167) },
          { AOM_CDF2(1998) },
          { AOM_CDF2(748) },
      },
    };

#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTER
static const aom_cdf_prob
    default_mdtx_type_inter_cdf[EXT_TX_SIZES][CDF_SIZE(MDTX_TYPES_INTER)] = {
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
      { AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672) },
    };

static const aom_cdf_prob default_use_mdtx_inter_cdf[EXT_TX_SIZES]
                                                    [CDF_SIZE(2)] = {
                                                      { AOM_CDF2(16384) },
                                                      { AOM_CDF2(16384) },
                                                      { AOM_CDF2(16384) },
                                                      { AOM_CDF2(16384) },
                                                    };
#endif  // USE_MDTX_INTER
#if USE_MDTX_INTRA
static const aom_cdf_prob
    default_mdtx_type_intra_cdf[EXT_TX_SIZES][INTRA_MODES]
                               [CDF_SIZE(MDTX_TYPES_INTRA)] = {
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                                 {
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                     { AOM_CDF3(10923, 21845) },
                                 },
                               };

static const aom_cdf_prob default_use_mdtx_intra_cdf[EXT_TX_SIZES][INTRA_MODES]
                                                    [CDF_SIZE(2)] = {
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      },
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      },
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      },
                                                      {
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                          { AOM_CDF2(16384) },
                                                      }
                                                    };
#endif  // USE_MDTX_INTRA
#endif

static const aom_cdf_prob default_cfl_sign_cdf[CDF_SIZE(CFL_JOINT_SIGNS)] = {
  AOM_CDF8(1418, 2123, 13340, 18405, 26972, 28343, 32294)
};

static const aom_cdf_prob
    default_cfl_alpha_cdf[CFL_ALPHA_CONTEXTS][CDF_SIZE(CFL_ALPHABET_SIZE)] = {
      { AOM_CDF16(7637, 20719, 31401, 32481, 32657, 32688, 32692, 32696, 32700,
                  32704, 32708, 32712, 32716, 32720, 32724) },
      { AOM_CDF16(14365, 23603, 28135, 31168, 32167, 32395, 32487, 32573, 32620,
                  32647, 32668, 32672, 32676, 32680, 32684) },
      { AOM_CDF16(11532, 22380, 28445, 31360, 32349, 32523, 32584, 32649, 32673,
                  32677, 32681, 32685, 32689, 32693, 32697) },
      { AOM_CDF16(26990, 31402, 32282, 32571, 32692, 32696, 32700, 32704, 32708,
                  32712, 32716, 32720, 32724, 32728, 32732) },
      { AOM_CDF16(17248, 26058, 28904, 30608, 31305, 31877, 32126, 32321, 32394,
                  32464, 32516, 32560, 32576, 32593, 32622) },
      { AOM_CDF16(14738, 21678, 25779, 27901, 29024, 30302, 30980, 31843, 32144,
                  32413, 32520, 32594, 32622, 32656, 32660) }
    };

static const aom_cdf_prob
    default_switchable_interp_cdf[SWITCHABLE_FILTER_CONTEXTS][CDF_SIZE(
        SWITCHABLE_FILTERS)] = {
      { AOM_CDF3(31935, 32720) }, { AOM_CDF3(5568, 32719) },
      { AOM_CDF3(422, 2938) },    { AOM_CDF3(28244, 32608) },
      { AOM_CDF3(31206, 31953) }, { AOM_CDF3(4862, 32121) },
      { AOM_CDF3(770, 1152) },    { AOM_CDF3(20889, 25637) },
      { AOM_CDF3(31910, 32724) }, { AOM_CDF3(4120, 32712) },
      { AOM_CDF3(305, 2247) },    { AOM_CDF3(27403, 32636) },
      { AOM_CDF3(31022, 32009) }, { AOM_CDF3(2963, 32093) },
      { AOM_CDF3(601, 943) },     { AOM_CDF3(14969, 21398) }
    };

static const aom_cdf_prob default_newmv_cdf[NEWMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(24035) }, { AOM_CDF2(16630) }, { AOM_CDF2(15339) },
            { AOM_CDF2(8386) },  { AOM_CDF2(12222) }, { AOM_CDF2(4676) } };

static const aom_cdf_prob default_zeromv_cdf[GLOBALMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(2175) }, { AOM_CDF2(1054) } };

static const aom_cdf_prob default_refmv_cdf[REFMV_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(23974) }, { AOM_CDF2(24188) }, { AOM_CDF2(17848) },
            { AOM_CDF2(28622) }, { AOM_CDF2(24312) }, { AOM_CDF2(19923) } };

static const aom_cdf_prob default_drl_cdf[DRL_MODE_CONTEXTS][CDF_SIZE(2)] = {
  { AOM_CDF2(13104) }, { AOM_CDF2(24560) }, { AOM_CDF2(18945) }
};

static const aom_cdf_prob
    default_inter_compound_mode_cdf[INTER_MODE_CONTEXTS][CDF_SIZE(
        INTER_COMPOUND_MODES)] = {
      { AOM_CDF8(7760, 13823, 15808, 17641, 19156, 20666, 26891) },
      { AOM_CDF8(10730, 19452, 21145, 22749, 24039, 25131, 28724) },
      { AOM_CDF8(10664, 20221, 21588, 22906, 24295, 25387, 28436) },
      { AOM_CDF8(13298, 16984, 20471, 24182, 25067, 25736, 26422) },
      { AOM_CDF8(18904, 23325, 25242, 27432, 27898, 28258, 30758) },
      { AOM_CDF8(10725, 17454, 20124, 22820, 24195, 25168, 26046) },
      { AOM_CDF8(17125, 24273, 25814, 27492, 28214, 28704, 30592) },
      { AOM_CDF8(13046, 23214, 24505, 25942, 27435, 28442, 29330) }
    };

static const aom_cdf_prob default_interintra_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
    2)] = { { AOM_CDF2(16384) },
            { AOM_CDF2(26887) },
            { AOM_CDF2(27597) },
            { AOM_CDF2(30237) } };

static const aom_cdf_prob
    default_interintra_mode_cdf[BLOCK_SIZE_GROUPS][CDF_SIZE(
        INTERINTRA_MODES)] = { { AOM_CDF4(8192, 16384, 24576) },
                               { AOM_CDF4(1875, 11082, 27332) },
                               { AOM_CDF4(2473, 9996, 26388) },
                               { AOM_CDF4(4238, 11537, 25926) } };

static const aom_cdf_prob
    default_wedge_interintra_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(20036) }, { AOM_CDF2(24957) }, { AOM_CDF2(26704) },
      { AOM_CDF2(27530) }, { AOM_CDF2(29564) }, { AOM_CDF2(29444) },
      { AOM_CDF2(26872) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
    };

static const aom_cdf_prob default_compound_type_cdf[BLOCK_SIZES_ALL][CDF_SIZE(
    MASKED_COMPOUND_TYPES)] = {
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(23431) }, { AOM_CDF2(13171) }, { AOM_CDF2(11470) },
  { AOM_CDF2(9770) },  { AOM_CDF2(9100) },  { AOM_CDF2(8233) },
  { AOM_CDF2(6172) },  { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(11820) }, { AOM_CDF2(7701) },  { AOM_CDF2(16384) },
  { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
};

static const aom_cdf_prob
    default_wedge_idx_cdf[BLOCK_SIZES_ALL][CDF_SIZE(16)] = {
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2438, 4440, 6599, 8663, 11005, 12874, 15751, 18094, 20359,
                  22362, 24127, 25702, 27752, 29450, 31171) },
      { AOM_CDF16(806, 3266, 6005, 6738, 7218, 7367, 7771, 14588, 16323, 17367,
                  18452, 19422, 22839, 26127, 29629) },
      { AOM_CDF16(2779, 3738, 4683, 7213, 7775, 8017, 8655, 14357, 17939, 21332,
                  24520, 27470, 29456, 30529, 31656) },
      { AOM_CDF16(1684, 3625, 5675, 7108, 9302, 11274, 14429, 17144, 19163,
                  20961, 22884, 24471, 26719, 28714, 30877) },
      { AOM_CDF16(1142, 3491, 6277, 7314, 8089, 8355, 9023, 13624, 15369, 16730,
                  18114, 19313, 22521, 26012, 29550) },
      { AOM_CDF16(2742, 4195, 5727, 8035, 8980, 9336, 10146, 14124, 17270,
                  20533, 23434, 25972, 27944, 29570, 31416) },
      { AOM_CDF16(1727, 3948, 6101, 7796, 9841, 12344, 15766, 18944, 20638,
                  22038, 23963, 25311, 26988, 28766, 31012) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(154, 987, 1925, 2051, 2088, 2111, 2151, 23033, 23703, 24284,
                  24985, 25684, 27259, 28883, 30911) },
      { AOM_CDF16(1135, 1322, 1493, 2635, 2696, 2737, 2770, 21016, 22935, 25057,
                  27251, 29173, 30089, 30960, 31933) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
      { AOM_CDF16(2048, 4096, 6144, 8192, 10240, 12288, 14336, 16384, 18432,
                  20480, 22528, 24576, 26624, 28672, 30720) },
#endif  // CONFIG_FLEX_PARTITION
    };

static const aom_cdf_prob
    default_motion_mode_cdf[BLOCK_SIZES_ALL][CDF_SIZE(MOTION_MODES)] = {
      { AOM_CDF3(10923, 21845) }, { AOM_CDF3(10923, 21845) },
      { AOM_CDF3(10923, 21845) }, { AOM_CDF3(7651, 24760) },
      { AOM_CDF3(4738, 24765) },  { AOM_CDF3(5391, 25528) },
      { AOM_CDF3(19419, 26810) }, { AOM_CDF3(5123, 23606) },
      { AOM_CDF3(11606, 24308) }, { AOM_CDF3(26260, 29116) },
      { AOM_CDF3(20360, 28062) }, { AOM_CDF3(21679, 26830) },
      { AOM_CDF3(29516, 30701) }, { AOM_CDF3(28898, 30397) },
      { AOM_CDF3(30878, 31335) }, { AOM_CDF3(32507, 32558) },
      { AOM_CDF3(10923, 21845) }, { AOM_CDF3(10923, 21845) },
      { AOM_CDF3(28799, 31390) }, { AOM_CDF3(26431, 30774) },
      { AOM_CDF3(28973, 31594) }, { AOM_CDF3(29742, 31203) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF3(16384, 24576) }, { AOM_CDF3(16384, 24576) },
      { AOM_CDF3(16384, 27000) }, { AOM_CDF3(16384, 27000) },
      { AOM_CDF3(16384, 27000) }, { AOM_CDF3(16384, 27000) },
#endif  // CONFIG_FLEX_PARTITION
    };

static const aom_cdf_prob default_obmc_cdf[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
  { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(10437) }, { AOM_CDF2(9371) },  { AOM_CDF2(9301) },
  { AOM_CDF2(17432) }, { AOM_CDF2(14423) }, { AOM_CDF2(15142) },
  { AOM_CDF2(25817) }, { AOM_CDF2(22823) }, { AOM_CDF2(22083) },
  { AOM_CDF2(30128) }, { AOM_CDF2(31014) }, { AOM_CDF2(31560) },
  { AOM_CDF2(32638) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
  { AOM_CDF2(23664) }, { AOM_CDF2(20901) }, { AOM_CDF2(24008) },
  { AOM_CDF2(26879) },
#if CONFIG_FLEX_PARTITION
  { AOM_CDF2(24000) }, { AOM_CDF2(24000) }, { AOM_CDF2(24000) },
  { AOM_CDF2(24000) }, { AOM_CDF2(24000) }, { AOM_CDF2(24000) },
#endif  // CONFIG_FLEX_PARTITION
};

static const aom_cdf_prob default_intra_inter_cdf[INTRA_INTER_CONTEXTS]
                                                 [CDF_SIZE(2)] = {
                                                   { AOM_CDF2(806) },
                                                   { AOM_CDF2(16662) },
                                                   { AOM_CDF2(20186) },
                                                   { AOM_CDF2(26538) }
                                                 };

static const aom_cdf_prob default_comp_inter_cdf[COMP_INTER_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(26828) },
            { AOM_CDF2(24035) },
            { AOM_CDF2(12031) },
            { AOM_CDF2(10640) },
            { AOM_CDF2(2901) } };

static const aom_cdf_prob default_comp_ref_type_cdf[COMP_REF_TYPE_CONTEXTS]
                                                   [CDF_SIZE(2)] = {
                                                     { AOM_CDF2(1198) },
                                                     { AOM_CDF2(2070) },
                                                     { AOM_CDF2(9166) },
                                                     { AOM_CDF2(7499) },
                                                     { AOM_CDF2(22475) }
                                                   };

static const aom_cdf_prob
    default_uni_comp_ref_cdf[UNI_COMP_REF_CONTEXTS][UNIDIR_COMP_REFS -
                                                    1][CDF_SIZE(2)] = {
      { { AOM_CDF2(5284) }, { AOM_CDF2(3865) }, { AOM_CDF2(3128) } },
      { { AOM_CDF2(23152) }, { AOM_CDF2(14173) }, { AOM_CDF2(15270) } },
      { { AOM_CDF2(31774) }, { AOM_CDF2(25120) }, { AOM_CDF2(26710) } }
    };

static const aom_cdf_prob default_single_ref_cdf[REF_CONTEXTS][SINGLE_REFS - 1]
                                                [CDF_SIZE(2)] = {
                                                  { { AOM_CDF2(4897) },
                                                    { AOM_CDF2(1555) },
                                                    { AOM_CDF2(4236) },
                                                    { AOM_CDF2(8650) },
                                                    { AOM_CDF2(904) },
                                                    { AOM_CDF2(1444) } },
                                                  { { AOM_CDF2(16973) },
                                                    { AOM_CDF2(16751) },
                                                    { AOM_CDF2(19647) },
                                                    { AOM_CDF2(24773) },
                                                    { AOM_CDF2(11014) },
                                                    { AOM_CDF2(15087) } },
                                                  { { AOM_CDF2(29744) },
                                                    { AOM_CDF2(30279) },
                                                    { AOM_CDF2(31194) },
                                                    { AOM_CDF2(31895) },
                                                    { AOM_CDF2(26875) },
                                                    { AOM_CDF2(30304) } }
                                                };

static const aom_cdf_prob
    default_comp_ref_cdf[REF_CONTEXTS][FWD_REFS - 1][CDF_SIZE(2)] = {
      { { AOM_CDF2(4946) }, { AOM_CDF2(9468) }, { AOM_CDF2(1503) } },
      { { AOM_CDF2(19891) }, { AOM_CDF2(22441) }, { AOM_CDF2(15160) } },
      { { AOM_CDF2(30731) }, { AOM_CDF2(31059) }, { AOM_CDF2(27544) } }
    };

static const aom_cdf_prob
    default_comp_bwdref_cdf[REF_CONTEXTS][BWD_REFS - 1][CDF_SIZE(2)] = {
      { { AOM_CDF2(2235) }, { AOM_CDF2(1423) } },
      { { AOM_CDF2(17182) }, { AOM_CDF2(15175) } },
      { { AOM_CDF2(30606) }, { AOM_CDF2(30489) } }
    };

static const aom_cdf_prob
    default_palette_y_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)] = {
      { AOM_CDF7(7952, 13000, 18149, 21478, 25527, 29241) },
      { AOM_CDF7(7139, 11421, 16195, 19544, 23666, 28073) },
      { AOM_CDF7(7788, 12741, 17325, 20500, 24315, 28530) },
      { AOM_CDF7(8271, 14064, 18246, 21564, 25071, 28533) },
      { AOM_CDF7(12725, 19180, 21863, 24839, 27535, 30120) },
      { AOM_CDF7(9711, 14888, 16923, 21052, 25661, 27875) },
      { AOM_CDF7(14940, 20797, 21678, 24186, 27033, 28999) }
    };

static const aom_cdf_prob
    default_palette_uv_size_cdf[PALATTE_BSIZE_CTXS][CDF_SIZE(PALETTE_SIZES)] = {
      { AOM_CDF7(8713, 19979, 27128, 29609, 31331, 32272) },
      { AOM_CDF7(5839, 15573, 23581, 26947, 29848, 31700) },
      { AOM_CDF7(4426, 11260, 17999, 21483, 25863, 29430) },
      { AOM_CDF7(3228, 9464, 14993, 18089, 22523, 27420) },
      { AOM_CDF7(3768, 8886, 13091, 17852, 22495, 27207) },
      { AOM_CDF7(2464, 8451, 12861, 21632, 25525, 28555) },
      { AOM_CDF7(1269, 5435, 10433, 18963, 21700, 25865) }
    };

static const aom_cdf_prob default_palette_y_mode_cdf
    [PALATTE_BSIZE_CTXS][PALETTE_Y_MODE_CONTEXTS][CDF_SIZE(2)] = {
      { { AOM_CDF2(31676) }, { AOM_CDF2(3419) }, { AOM_CDF2(1261) } },
      { { AOM_CDF2(31912) }, { AOM_CDF2(2859) }, { AOM_CDF2(980) } },
      { { AOM_CDF2(31823) }, { AOM_CDF2(3400) }, { AOM_CDF2(781) } },
      { { AOM_CDF2(32030) }, { AOM_CDF2(3561) }, { AOM_CDF2(904) } },
      { { AOM_CDF2(32309) }, { AOM_CDF2(7337) }, { AOM_CDF2(1462) } },
      { { AOM_CDF2(32265) }, { AOM_CDF2(4015) }, { AOM_CDF2(1521) } },
      { { AOM_CDF2(32450) }, { AOM_CDF2(7946) }, { AOM_CDF2(129) } }
    };

static const aom_cdf_prob
    default_palette_uv_mode_cdf[PALETTE_UV_MODE_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(32461) }, { AOM_CDF2(21488) }
    };

static const aom_cdf_prob default_palette_y_color_index_cdf
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)] = {
      {
          { AOM_CDF2(28710) },
          { AOM_CDF2(16384) },
          { AOM_CDF2(10553) },
          { AOM_CDF2(27036) },
          { AOM_CDF2(31603) },
      },
      {
          { AOM_CDF3(27877, 30490) },
          { AOM_CDF3(11532, 25697) },
          { AOM_CDF3(6544, 30234) },
          { AOM_CDF3(23018, 28072) },
          { AOM_CDF3(31915, 32385) },
      },
      {
          { AOM_CDF4(25572, 28046, 30045) },
          { AOM_CDF4(9478, 21590, 27256) },
          { AOM_CDF4(7248, 26837, 29824) },
          { AOM_CDF4(19167, 24486, 28349) },
          { AOM_CDF4(31400, 31825, 32250) },
      },
      {
          { AOM_CDF5(24779, 26955, 28576, 30282) },
          { AOM_CDF5(8669, 20364, 24073, 28093) },
          { AOM_CDF5(4255, 27565, 29377, 31067) },
          { AOM_CDF5(19864, 23674, 26716, 29530) },
          { AOM_CDF5(31646, 31893, 32147, 32426) },
      },
      {
          { AOM_CDF6(23132, 25407, 26970, 28435, 30073) },
          { AOM_CDF6(7443, 17242, 20717, 24762, 27982) },
          { AOM_CDF6(6300, 24862, 26944, 28784, 30671) },
          { AOM_CDF6(18916, 22895, 25267, 27435, 29652) },
          { AOM_CDF6(31270, 31550, 31808, 32059, 32353) },
      },
      {
          { AOM_CDF7(23105, 25199, 26464, 27684, 28931, 30318) },
          { AOM_CDF7(6950, 15447, 18952, 22681, 25567, 28563) },
          { AOM_CDF7(7560, 23474, 25490, 27203, 28921, 30708) },
          { AOM_CDF7(18544, 22373, 24457, 26195, 28119, 30045) },
          { AOM_CDF7(31198, 31451, 31670, 31882, 32123, 32391) },
      },
      {
          { AOM_CDF8(21689, 23883, 25163, 26352, 27506, 28827, 30195) },
          { AOM_CDF8(6892, 15385, 17840, 21606, 24287, 26753, 29204) },
          { AOM_CDF8(5651, 23182, 25042, 26518, 27982, 29392, 30900) },
          { AOM_CDF8(19349, 22578, 24418, 25994, 27524, 29031, 30448) },
          { AOM_CDF8(31028, 31270, 31504, 31705, 31927, 32153, 32392) },
      },
    };

static const aom_cdf_prob default_palette_uv_color_index_cdf
    [PALETTE_SIZES][PALETTE_COLOR_INDEX_CONTEXTS][CDF_SIZE(PALETTE_COLORS)] = {
      {
          { AOM_CDF2(29089) },
          { AOM_CDF2(16384) },
          { AOM_CDF2(8713) },
          { AOM_CDF2(29257) },
          { AOM_CDF2(31610) },
      },
      {
          { AOM_CDF3(25257, 29145) },
          { AOM_CDF3(12287, 27293) },
          { AOM_CDF3(7033, 27960) },
          { AOM_CDF3(20145, 25405) },
          { AOM_CDF3(30608, 31639) },
      },
      {
          { AOM_CDF4(24210, 27175, 29903) },
          { AOM_CDF4(9888, 22386, 27214) },
          { AOM_CDF4(5901, 26053, 29293) },
          { AOM_CDF4(18318, 22152, 28333) },
          { AOM_CDF4(30459, 31136, 31926) },
      },
      {
          { AOM_CDF5(22980, 25479, 27781, 29986) },
          { AOM_CDF5(8413, 21408, 24859, 28874) },
          { AOM_CDF5(2257, 29449, 30594, 31598) },
          { AOM_CDF5(19189, 21202, 25915, 28620) },
          { AOM_CDF5(31844, 32044, 32281, 32518) },
      },
      {
          { AOM_CDF6(22217, 24567, 26637, 28683, 30548) },
          { AOM_CDF6(7307, 16406, 19636, 24632, 28424) },
          { AOM_CDF6(4441, 25064, 26879, 28942, 30919) },
          { AOM_CDF6(17210, 20528, 23319, 26750, 29582) },
          { AOM_CDF6(30674, 30953, 31396, 31735, 32207) },
      },
      {
          { AOM_CDF7(21239, 23168, 25044, 26962, 28705, 30506) },
          { AOM_CDF7(6545, 15012, 18004, 21817, 25503, 28701) },
          { AOM_CDF7(3448, 26295, 27437, 28704, 30126, 31442) },
          { AOM_CDF7(15889, 18323, 21704, 24698, 26976, 29690) },
          { AOM_CDF7(30988, 31204, 31479, 31734, 31983, 32325) },
      },
      {
          { AOM_CDF8(21442, 23288, 24758, 26246, 27649, 28980, 30563) },
          { AOM_CDF8(5863, 14933, 17552, 20668, 23683, 26411, 29273) },
          { AOM_CDF8(3415, 25810, 26877, 27990, 29223, 30394, 31618) },
          { AOM_CDF8(17965, 20084, 22232, 23974, 26274, 28402, 30390) },
          { AOM_CDF8(31190, 31329, 31516, 31679, 31825, 32026, 32322) },
      },
    };

#if CONFIG_NEW_TX_PARTITION
static const aom_cdf_prob
    default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(
        TX_PARTITION_TYPES)] = {
      { AOM_CDF2(28581) }, { AOM_CDF2(23846) }, { AOM_CDF2(20847) },
      { AOM_CDF2(24315) }, { AOM_CDF2(18196) }, { AOM_CDF2(12133) },
      { AOM_CDF2(18791) }, { AOM_CDF2(10887) }, { AOM_CDF2(11005) },
      { AOM_CDF2(27179) }, { AOM_CDF2(20004) }, { AOM_CDF2(11281) },
      { AOM_CDF2(26549) }, { AOM_CDF2(19308) }, { AOM_CDF2(14224) },
      { AOM_CDF2(28015) }, { AOM_CDF2(21546) }, { AOM_CDF2(14400) },
      { AOM_CDF2(28165) }, { AOM_CDF2(22401) }, { AOM_CDF2(16088) }
    };
#else
static const aom_cdf_prob
    default_txfm_partition_cdf[TXFM_PARTITION_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(28581) }, { AOM_CDF2(23846) }, { AOM_CDF2(20847) },
      { AOM_CDF2(24315) }, { AOM_CDF2(18196) }, { AOM_CDF2(12133) },
      { AOM_CDF2(18791) }, { AOM_CDF2(10887) }, { AOM_CDF2(11005) },
      { AOM_CDF2(27179) }, { AOM_CDF2(20004) }, { AOM_CDF2(11281) },
      { AOM_CDF2(26549) }, { AOM_CDF2(19308) }, { AOM_CDF2(14224) },
      { AOM_CDF2(28015) }, { AOM_CDF2(21546) }, { AOM_CDF2(14400) },
      { AOM_CDF2(28165) }, { AOM_CDF2(22401) }, { AOM_CDF2(16088) }
    };
#endif  // CONFIG_NEW_TX_PARTITION

static const aom_cdf_prob default_skip_cdfs[SKIP_CONTEXTS][CDF_SIZE(2)] = {
  { AOM_CDF2(31671) }, { AOM_CDF2(16515) }, { AOM_CDF2(4576) }
};

static const aom_cdf_prob default_skip_mode_cdfs[SKIP_MODE_CONTEXTS][CDF_SIZE(
    2)] = { { AOM_CDF2(32621) }, { AOM_CDF2(20708) }, { AOM_CDF2(8127) } };

static const aom_cdf_prob
    default_compound_idx_cdfs[COMP_INDEX_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(18244) }, { AOM_CDF2(12865) }, { AOM_CDF2(7053) },
      { AOM_CDF2(13259) }, { AOM_CDF2(9334) },  { AOM_CDF2(4644) }
    };

static const aom_cdf_prob
    default_comp_group_idx_cdfs[COMP_GROUP_IDX_CONTEXTS][CDF_SIZE(2)] = {
      { AOM_CDF2(26607) }, { AOM_CDF2(22891) }, { AOM_CDF2(18840) },
      { AOM_CDF2(24594) }, { AOM_CDF2(19934) }, { AOM_CDF2(22674) }
    };

static const aom_cdf_prob default_intrabc_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    30531) };

static const aom_cdf_prob default_filter_intra_mode_cdf[CDF_SIZE(
    FILTER_INTRA_MODES)] = { AOM_CDF5(8949, 12776, 17211, 29558) };

static const aom_cdf_prob
    default_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
      { AOM_CDF2(4621) },  { AOM_CDF2(6743) },  { AOM_CDF2(5893) },
      { AOM_CDF2(7866) },  { AOM_CDF2(12551) }, { AOM_CDF2(9394) },
      { AOM_CDF2(12408) }, { AOM_CDF2(14301) }, { AOM_CDF2(12756) },
      { AOM_CDF2(22343) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(12770) }, { AOM_CDF2(10368) },
      { AOM_CDF2(20229) }, { AOM_CDF2(18101) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
    };

#if CONFIG_ADAPT_FILTER_INTRA
static const aom_cdf_prob default_adapt_filter_intra_mode_cdf[CDF_SIZE(
    USED_ADAPT_FILTER_INTRA_MODES)] =
#if USED_ADAPT_FILTER_INTRA_MODES == 3
    { AOM_CDF3(10922, 10922) };
#elif USED_ADAPT_FILTER_INTRA_MODES == 5
    { AOM_CDF5(6553, 13106, 19659, 26212) };
#elif USED_ADAPT_FILTER_INTRA_MODES == 7
    { AOM_CDF7(4681, 9362, 14043, 18724, 23405, 28086) };
#endif

static const aom_cdf_prob
    default_adapt_filter_intra_cdfs[BLOCK_SIZES_ALL][CDF_SIZE(2)] = {
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) },
#if CONFIG_FLEX_PARTITION
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
      { AOM_CDF2(16384) }, { AOM_CDF2(16384) }, { AOM_CDF2(16384) },
#endif  // CONFIG_FLEX_PARTITION
    };
#endif  // CONFIG_ADAPT_FILTER_INTRA

#if CONFIG_LOOP_RESTORE_CNN
static const aom_cdf_prob default_switchable_restore_cdf[CDF_SIZE(
    RESTORE_SWITCHABLE_TYPES)] = { AOM_CDF4(6000, 14000, 22500) };
#else
static const aom_cdf_prob default_switchable_restore_cdf[CDF_SIZE(
    RESTORE_SWITCHABLE_TYPES)] = { AOM_CDF3(9413, 22581) };
#endif  // CONFIG_LOOP_RESTORE_CNN

static const aom_cdf_prob default_wiener_restore_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    11570) };

static const aom_cdf_prob default_sgrproj_restore_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    16855) };

#if CONFIG_LOOP_RESTORE_CNN
static const aom_cdf_prob default_cnn_restore_cdf[CDF_SIZE(2)] = { AOM_CDF2(
    20000) };
#endif  // CONFIG_LOOP_RESTORE_CNN

static const aom_cdf_prob default_delta_q_cdf[CDF_SIZE(DELTA_Q_PROBS + 1)] = {
  AOM_CDF4(28160, 32120, 32677)
};

static const aom_cdf_prob default_delta_lf_multi_cdf[FRAME_LF_COUNT][CDF_SIZE(
    DELTA_LF_PROBS + 1)] = { { AOM_CDF4(28160, 32120, 32677) },
                             { AOM_CDF4(28160, 32120, 32677) },
                             { AOM_CDF4(28160, 32120, 32677) },
                             { AOM_CDF4(28160, 32120, 32677) } };
static const aom_cdf_prob default_delta_lf_cdf[CDF_SIZE(DELTA_LF_PROBS + 1)] = {
  AOM_CDF4(28160, 32120, 32677)
};

// FIXME(someone) need real defaults here
static const aom_cdf_prob default_seg_tree_cdf[CDF_SIZE(MAX_SEGMENTS)] = {
  AOM_CDF8(4096, 8192, 12288, 16384, 20480, 24576, 28672)
};

static const aom_cdf_prob
    default_segment_pred_cdf[SEG_TEMPORAL_PRED_CTXS][CDF_SIZE(2)] = {
      { AOM_CDF2(128 * 128) }, { AOM_CDF2(128 * 128) }, { AOM_CDF2(128 * 128) }
    };

static const aom_cdf_prob
    default_spatial_pred_seg_tree_cdf[SPATIAL_PREDICTION_PROBS][CDF_SIZE(
        MAX_SEGMENTS)] = {
      {
          AOM_CDF8(5622, 7893, 16093, 18233, 27809, 28373, 32533),
      },
      {
          AOM_CDF8(14274, 18230, 22557, 24935, 29980, 30851, 32344),
      },
      {
          AOM_CDF8(27527, 28487, 28723, 28890, 32397, 32647, 32679),
      },
    };

static const aom_cdf_prob default_tx_size_cdf[MAX_TX_CATS][TX_SIZE_CONTEXTS]
                                             [CDF_SIZE(MAX_TX_DEPTH + 1)] = {
                                               { { AOM_CDF2(19968) },
                                                 { AOM_CDF2(19968) },
                                                 { AOM_CDF2(24320) } },
                                               { { AOM_CDF3(12272, 30172) },
                                                 { AOM_CDF3(12272, 30172) },
                                                 { AOM_CDF3(18677, 30848) } },
                                               { { AOM_CDF3(12986, 15180) },
                                                 { AOM_CDF3(12986, 15180) },
                                                 { AOM_CDF3(24302, 25602) } },
                                               { { AOM_CDF3(5782, 11475) },
                                                 { AOM_CDF3(5782, 11475) },
                                                 { AOM_CDF3(16803, 22759) } },
                                             };

#define MAX_COLOR_CONTEXT_HASH 8
// Negative values are invalid
static const int palette_color_index_context_lookup[MAX_COLOR_CONTEXT_HASH +
                                                    1] = { -1, -1, 0, -1, -1,
                                                           4,  3,  2, 1 };

#define NUM_PALETTE_NEIGHBORS 3  // left, top-left and top.
int av1_get_palette_color_index_context(const uint8_t *color_map, int stride,
                                        int r, int c, int palette_size,
                                        uint8_t *color_order, int *color_idx) {
  assert(palette_size <= PALETTE_MAX_SIZE);
  assert(r > 0 || c > 0);

  // Get color indices of neighbors.
  int color_neighbors[NUM_PALETTE_NEIGHBORS];
  color_neighbors[0] = (c - 1 >= 0) ? color_map[r * stride + c - 1] : -1;
  color_neighbors[1] =
      (c - 1 >= 0 && r - 1 >= 0) ? color_map[(r - 1) * stride + c - 1] : -1;
  color_neighbors[2] = (r - 1 >= 0) ? color_map[(r - 1) * stride + c] : -1;

  // The +10 below should not be needed. But we get a warning "array subscript
  // is above array bounds [-Werror=array-bounds]" without it, possibly due to
  // this (or similar) bug: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
  int scores[PALETTE_MAX_SIZE + 10] = { 0 };
  int i;
  static const int weights[NUM_PALETTE_NEIGHBORS] = { 2, 1, 2 };
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    if (color_neighbors[i] >= 0) {
      scores[color_neighbors[i]] += weights[i];
    }
  }

  int inverse_color_order[PALETTE_MAX_SIZE];
  for (i = 0; i < PALETTE_MAX_SIZE; ++i) {
    color_order[i] = i;
    inverse_color_order[i] = i;
  }

  // Get the top NUM_PALETTE_NEIGHBORS scores (sorted from large to small).
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    int max = scores[i];
    int max_idx = i;
    for (int j = i + 1; j < palette_size; ++j) {
      if (scores[j] > max) {
        max = scores[j];
        max_idx = j;
      }
    }
    if (max_idx != i) {
      // Move the score at index 'max_idx' to index 'i', and shift the scores
      // from 'i' to 'max_idx - 1' by 1.
      const int max_score = scores[max_idx];
      const uint8_t max_color_order = color_order[max_idx];
      for (int k = max_idx; k > i; --k) {
        scores[k] = scores[k - 1];
        color_order[k] = color_order[k - 1];
        inverse_color_order[color_order[k]] = k;
      }
      scores[i] = max_score;
      color_order[i] = max_color_order;
      inverse_color_order[color_order[i]] = i;
    }
  }

  if (color_idx != NULL)
    *color_idx = inverse_color_order[color_map[r * stride + c]];

  // Get hash value of context.
  int color_index_ctx_hash = 0;
  static const int hash_multipliers[NUM_PALETTE_NEIGHBORS] = { 1, 2, 2 };
  for (i = 0; i < NUM_PALETTE_NEIGHBORS; ++i) {
    color_index_ctx_hash += scores[i] * hash_multipliers[i];
  }
  assert(color_index_ctx_hash > 0);
  assert(color_index_ctx_hash <= MAX_COLOR_CONTEXT_HASH);

  // Lookup context from hash.
  const int color_index_ctx =
      palette_color_index_context_lookup[color_index_ctx_hash];
  assert(color_index_ctx >= 0);
  assert(color_index_ctx < PALETTE_COLOR_INDEX_CONTEXTS);
  return color_index_ctx;
}
#undef NUM_PALETTE_NEIGHBORS
#undef MAX_COLOR_CONTEXT_HASH

static void init_mode_probs(FRAME_CONTEXT *fc) {
  av1_copy(fc->palette_y_size_cdf, default_palette_y_size_cdf);
  av1_copy(fc->palette_uv_size_cdf, default_palette_uv_size_cdf);
  av1_copy(fc->palette_y_color_index_cdf, default_palette_y_color_index_cdf);
  av1_copy(fc->palette_uv_color_index_cdf, default_palette_uv_color_index_cdf);
  av1_copy(fc->kf_y_cdf, default_kf_y_mode_cdf);
#if CONFIG_INTRA_ENTROPY
  // Intra Y mode model
  fc->av1_intra_y_mode.lr = lr;
  fc->av1_intra_y_mode_q.inv_lr = inv_lr;
  fc->av1_intra_y_mode_q.scale = 10;
  fc->av1_intra_y_mode_q.fscale = 1 << 10;
#if INTRA_MODEL < 0
#if QNN
  fc->av1_intra_y_mode_q.num_hidden_layers = 0;
  fc->av1_intra_y_mode_q.layer[0].num_inputs = 42;
  fc->av1_intra_y_mode_q.layer[0].num_outputs = INTRA_MODES;
  fc->av1_intra_y_mode_q.layer[0].activation = ACTN_NONE;
  fc->av1_intra_y_mode_q.num_logits = INTRA_MODES;
  fc->av1_intra_y_mode_q.loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  av1_copy(fc->av1_intra_y_mode_q.layer[0].weights,
           intra_y_mode_qlayer0_weights);
  av1_copy(fc->av1_intra_y_mode_q.layer[0].bias, intra_y_mode_qlayer0_bias);
  av1_zero_array(fc->av1_intra_y_mode_q.feature, EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_y_mode_q.layer[0].output, EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_y_mode_q.layer[0].dY, EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_y_mode_q.layer[0].dW,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_y_mode_q.layer[0].db, EM_MAX_NODES);
#else
  float arr[20000] = { 0.f };
  fc->av1_intra_y_mode.num_hidden_layers = 0;
  fc->av1_intra_y_mode.layer[0].num_inputs = 42;
  fc->av1_intra_y_mode.layer[0].num_outputs = INTRA_MODES;
  fc->av1_intra_y_mode.layer[0].activation = ACTN_NONE;
  fc->av1_intra_y_mode.num_logits = INTRA_MODES;
  fc->av1_intra_y_mode.loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  av1_copy(fc->av1_intra_y_mode.layer[0].weights, intra_y_mode_layer0_weights);
  av1_copy(fc->av1_intra_y_mode.layer[0].bias, intra_y_mode_layer0_bias);
  av1_copy_array(fc->av1_intra_y_mode.feature, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].output, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].dY, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].dW, arr,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].db, arr, EM_MAX_NODES);
#endif  // QNN
#else
  float arr[20000] = { 0.f };
  fc->av1_intra_y_mode.num_hidden_layers = 1;
#if INTRA_MODEL == 0
  fc->av1_intra_y_mode.layer[0].num_inputs = 42;
#elif INTRA_MODEL == 1
  fc->av1_intra_y_mode.layer[0].num_inputs = 24;
#else
  fc->av1_intra_y_mode.layer[0].num_inputs = 39;
#endif  // INTRA_MODEL
  fc->av1_intra_y_mode.layer[0].num_outputs = 16;
  fc->av1_intra_y_mode.layer[0].activation = ACTN_RELU;
  fc->av1_intra_y_mode.layer[1].num_inputs = 16;
  fc->av1_intra_y_mode.layer[1].num_outputs = INTRA_MODES;
  fc->av1_intra_y_mode.layer[1].activation = ACTN_NONE;
  fc->av1_intra_y_mode.num_logits = INTRA_MODES;
  fc->av1_intra_y_mode.loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  av1_copy(fc->av1_intra_y_mode.layer[0].weights, intra_y_mode_layer0_weights);
  av1_copy(fc->av1_intra_y_mode.layer[0].bias, intra_y_mode_layer0_bias);
  av1_copy(fc->av1_intra_y_mode.layer[1].weights, intra_y_mode_layer1_weights);
  av1_copy(fc->av1_intra_y_mode.layer[1].bias, intra_y_mode_layer1_bias);
  av1_copy_array(fc->av1_intra_y_mode.feature, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].output, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].dY, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].dW, arr,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[0].db, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[1].output, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[1].dY, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[1].dW, arr,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_y_mode.layer[1].db, arr, EM_MAX_NODES);
#endif  // INTRA_MODEL
  // Intra UV mode model
  fc->av1_intra_uv_mode.lr = lr;
  fc->av1_intra_uv_mode_q.inv_lr = inv_lr;
  fc->av1_intra_uv_mode_q.scale = 10;
  fc->av1_intra_uv_mode_q.fscale = 1 << 10;
#if INTRA_MODEL < 0
#if QNN
  fc->av1_intra_uv_mode_q.num_hidden_layers = 0;
  fc->av1_intra_uv_mode_q.layer[0].num_inputs = 31;
  fc->av1_intra_uv_mode_q.layer[0].num_outputs = UV_INTRA_MODES;
  fc->av1_intra_uv_mode_q.layer[0].activation = ACTN_NONE;
  fc->av1_intra_uv_mode_q.num_logits = UV_INTRA_MODES;
  fc->av1_intra_uv_mode_q.loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  av1_copy(fc->av1_intra_uv_mode_q.layer[0].weights,
           intra_uv_mode_qlayer0_weights);
  av1_copy(fc->av1_intra_uv_mode_q.layer[0].bias, intra_uv_mode_qlayer0_bias);
  av1_zero_array(fc->av1_intra_uv_mode_q.feature, EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_uv_mode_q.layer[0].output, EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_uv_mode_q.layer[0].dY, EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_uv_mode_q.layer[0].dW,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_zero_array(fc->av1_intra_uv_mode_q.layer[0].db, EM_MAX_NODES);
#else
  fc->av1_intra_uv_mode.num_hidden_layers = 0;
  fc->av1_intra_uv_mode.layer[0].num_inputs = 31;
  fc->av1_intra_uv_mode.layer[0].num_outputs = UV_INTRA_MODES;
  fc->av1_intra_uv_mode.layer[0].activation = ACTN_NONE;
  fc->av1_intra_uv_mode.num_logits = UV_INTRA_MODES;
  fc->av1_intra_uv_mode.loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  av1_copy(fc->av1_intra_uv_mode.layer[0].weights,
           intra_uv_mode_layer0_weights);
  av1_copy(fc->av1_intra_uv_mode.layer[0].bias, intra_uv_mode_layer0_bias);
  av1_copy_array(fc->av1_intra_uv_mode.feature, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].output, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].dY, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].dW, arr,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].db, arr, EM_MAX_NODES);
#endif  // QNN
#else
  fc->av1_intra_uv_mode.num_hidden_layers = 1;
  fc->av1_intra_uv_mode.layer[0].num_inputs = 31;
  fc->av1_intra_uv_mode.layer[0].num_outputs = 16;
  fc->av1_intra_uv_mode.layer[0].activation = ACTN_RELU;
  fc->av1_intra_uv_mode.layer[1].num_inputs = 16;
  fc->av1_intra_uv_mode.layer[1].num_outputs = UV_INTRA_MODES;
  fc->av1_intra_uv_mode.layer[1].activation = ACTN_NONE;
  fc->av1_intra_uv_mode.num_logits = UV_INTRA_MODES;
  fc->av1_intra_uv_mode.loss = SOFTMAX_CROSS_ENTROPY_LOSS;
  av1_copy(fc->av1_intra_uv_mode.layer[0].weights,
           intra_uv_mode_layer0_weights);
  av1_copy(fc->av1_intra_uv_mode.layer[0].bias, intra_uv_mode_layer0_bias);
  av1_copy(fc->av1_intra_uv_mode.layer[1].weights,
           intra_uv_mode_layer1_weights);
  av1_copy(fc->av1_intra_uv_mode.layer[1].bias, intra_uv_mode_layer1_bias);
  av1_copy_array(fc->av1_intra_uv_mode.feature, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].output, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].dY, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].dW, arr,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[0].db, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[1].output, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[1].dY, arr, EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[1].dW, arr,
                 EM_MAX_NODES * EM_MAX_NODES);
  av1_copy_array(fc->av1_intra_uv_mode.layer[1].db, arr, EM_MAX_NODES);
#endif  // INTRA_MODEL
#endif  // CONFIG_INTRA_ENTROPY
  av1_copy(fc->angle_delta_cdf, default_angle_delta_cdf);
  av1_copy(fc->comp_inter_cdf, default_comp_inter_cdf);
  av1_copy(fc->comp_ref_type_cdf, default_comp_ref_type_cdf);
  av1_copy(fc->uni_comp_ref_cdf, default_uni_comp_ref_cdf);
  av1_copy(fc->palette_y_mode_cdf, default_palette_y_mode_cdf);
  av1_copy(fc->palette_uv_mode_cdf, default_palette_uv_mode_cdf);
  av1_copy(fc->comp_ref_cdf, default_comp_ref_cdf);
  av1_copy(fc->comp_bwdref_cdf, default_comp_bwdref_cdf);
  av1_copy(fc->single_ref_cdf, default_single_ref_cdf);
  av1_copy(fc->txfm_partition_cdf, default_txfm_partition_cdf);
  av1_copy(fc->compound_index_cdf, default_compound_idx_cdfs);
  av1_copy(fc->comp_group_idx_cdf, default_comp_group_idx_cdfs);
  av1_copy(fc->newmv_cdf, default_newmv_cdf);
  av1_copy(fc->zeromv_cdf, default_zeromv_cdf);
  av1_copy(fc->refmv_cdf, default_refmv_cdf);
  av1_copy(fc->drl_cdf, default_drl_cdf);
  av1_copy(fc->motion_mode_cdf, default_motion_mode_cdf);
  av1_copy(fc->obmc_cdf, default_obmc_cdf);
  av1_copy(fc->inter_compound_mode_cdf, default_inter_compound_mode_cdf);
  av1_copy(fc->compound_type_cdf, default_compound_type_cdf);
  av1_copy(fc->wedge_idx_cdf, default_wedge_idx_cdf);
  av1_copy(fc->interintra_cdf, default_interintra_cdf);
  av1_copy(fc->wedge_interintra_cdf, default_wedge_interintra_cdf);
  av1_copy(fc->interintra_mode_cdf, default_interintra_mode_cdf);
  av1_copy(fc->seg.pred_cdf, default_segment_pred_cdf);
  av1_copy(fc->seg.tree_cdf, default_seg_tree_cdf);
  av1_copy(fc->filter_intra_cdfs, default_filter_intra_cdfs);
  av1_copy(fc->filter_intra_mode_cdf, default_filter_intra_mode_cdf);
#if CONFIG_ADAPT_FILTER_INTRA
  av1_copy(fc->adapt_filter_intra_cdfs, default_adapt_filter_intra_cdfs);
  av1_copy(fc->adapt_filter_intra_mode_cdf,
           default_adapt_filter_intra_mode_cdf);
#endif
  av1_copy(fc->switchable_restore_cdf, default_switchable_restore_cdf);
  av1_copy(fc->wiener_restore_cdf, default_wiener_restore_cdf);
  av1_copy(fc->sgrproj_restore_cdf, default_sgrproj_restore_cdf);
#if CONFIG_LOOP_RESTORE_CNN
  av1_copy(fc->cnn_restore_cdf, default_cnn_restore_cdf);
#endif  // CONFIG_LOOP_RESTORE_CNN
  av1_copy(fc->y_mode_cdf, default_if_y_mode_cdf);
  av1_copy(fc->uv_mode_cdf, default_uv_mode_cdf);
  av1_copy(fc->switchable_interp_cdf, default_switchable_interp_cdf);
  av1_copy(fc->partition_cdf, default_partition_cdf);
  av1_copy(fc->intra_ext_tx_cdf, default_intra_ext_tx_cdf);
  av1_copy(fc->inter_ext_tx_cdf, default_inter_ext_tx_cdf);
#if CONFIG_MODE_DEP_TX
#if USE_MDTX_INTER
  av1_copy(fc->mdtx_type_inter_cdf, default_mdtx_type_inter_cdf);
  av1_copy(fc->use_mdtx_inter_cdf, default_use_mdtx_inter_cdf);
#endif
#if USE_MDTX_INTRA
  av1_copy(fc->mdtx_type_intra_cdf, default_mdtx_type_intra_cdf);
  av1_copy(fc->use_mdtx_intra_cdf, default_use_mdtx_intra_cdf);
#endif
#endif
  av1_copy(fc->skip_mode_cdfs, default_skip_mode_cdfs);
  av1_copy(fc->skip_cdfs, default_skip_cdfs);
  av1_copy(fc->intra_inter_cdf, default_intra_inter_cdf);
  for (int i = 0; i < SPATIAL_PREDICTION_PROBS; i++)
    av1_copy(fc->seg.spatial_pred_seg_cdf[i],
             default_spatial_pred_seg_tree_cdf[i]);
  av1_copy(fc->tx_size_cdf, default_tx_size_cdf);
  av1_copy(fc->delta_q_cdf, default_delta_q_cdf);
  av1_copy(fc->delta_lf_cdf, default_delta_lf_cdf);
  av1_copy(fc->delta_lf_multi_cdf, default_delta_lf_multi_cdf);
  av1_copy(fc->cfl_sign_cdf, default_cfl_sign_cdf);
  av1_copy(fc->cfl_alpha_cdf, default_cfl_alpha_cdf);
  av1_copy(fc->intrabc_cdf, default_intrabc_cdf);
}

void av1_set_default_ref_deltas(int8_t *ref_deltas) {
  assert(ref_deltas != NULL);

  ref_deltas[INTRA_FRAME] = 1;
  ref_deltas[LAST_FRAME] = 0;
  ref_deltas[LAST2_FRAME] = ref_deltas[LAST_FRAME];
  ref_deltas[LAST3_FRAME] = ref_deltas[LAST_FRAME];
  ref_deltas[BWDREF_FRAME] = ref_deltas[LAST_FRAME];
  ref_deltas[GOLDEN_FRAME] = -1;
  ref_deltas[ALTREF2_FRAME] = -1;
  ref_deltas[ALTREF_FRAME] = -1;
}

void av1_set_default_mode_deltas(int8_t *mode_deltas) {
  assert(mode_deltas != NULL);

  mode_deltas[0] = 0;
  mode_deltas[1] = 0;
}

static void set_default_lf_deltas(struct loopfilter *lf) {
  lf->mode_ref_delta_enabled = 1;
  lf->mode_ref_delta_update = 1;

  av1_set_default_ref_deltas(lf->ref_deltas);
  av1_set_default_mode_deltas(lf->mode_deltas);
}

void av1_setup_frame_contexts(AV1_COMMON *cm) {
  // Store the frame context into a special slot (not associated with any
  // reference buffer), so that we can set up cm->pre_fc correctly later
  // This function must ONLY be called when cm->fc has been initialized with
  // default probs, either by av1_setup_past_independence or after manually
  // initializing them
  *cm->default_frame_context = *cm->fc;
  // TODO(jack.haughton@argondesign.com): don't think this should be necessary,
  // but could do with fuller testing
  if (cm->large_scale_tile) {
    for (int i = LAST_FRAME; i <= ALTREF_FRAME; ++i) {
      RefCntBuffer *const buf = get_ref_frame_buf(cm, i);
      if (buf != NULL) buf->frame_context = *cm->fc;
    }
    for (int i = 0; i < FRAME_BUFFERS; ++i)
      cm->buffer_pool->frame_bufs[i].frame_context = *cm->fc;
  }
}

void av1_setup_past_independence(AV1_COMMON *cm) {
  // Reset the segment feature data to the default stats:
  // Features disabled, 0, with delta coding (Default state).
  av1_clearall_segfeatures(&cm->seg);

  if (cm->cur_frame->seg_map)
    memset(cm->cur_frame->seg_map, 0, (cm->mi_rows * cm->mi_cols));

  // reset mode ref deltas
  av1_set_default_ref_deltas(cm->cur_frame->ref_deltas);
  av1_set_default_mode_deltas(cm->cur_frame->mode_deltas);
  set_default_lf_deltas(&cm->lf);

  av1_default_coef_probs(cm);
  init_mode_probs(cm->fc);
  av1_init_mv_probs(cm);
  cm->fc->initialized = 1;
  av1_setup_frame_contexts(cm);

  // prev_mi will only be allocated in encoder.
  if (frame_is_intra_only(cm) && cm->prev_mi)
    memset(cm->prev_mi, 0, cm->mi_stride * cm->mi_rows * sizeof(*cm->prev_mi));
}
