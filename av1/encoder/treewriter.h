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

#ifndef AV1_ENCODER_TREEWRITER_H_
#define AV1_ENCODER_TREEWRITER_H_

#include "aom_dsp/bitwriter.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_tree_probs_from_distribution(AomTree tree,
                                      unsigned int branch_ct[/* n - 1 */][2],
                                      const unsigned int num_events[/* n */]);

struct Av1Token {
  int value;
  int len;
};

void av1_tokens_from_tree(struct Av1Token *, const AomTreeIndex *);

static INLINE void av1_write_token(AomWriter *w, const AomTreeIndex *tree,
                                   const AomProb *probs,
                                   const struct Av1Token *token) {
  aom_write_tree(w, tree, probs, token->value, token->len, 0);
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_ENCODER_TREEWRITER_H_
