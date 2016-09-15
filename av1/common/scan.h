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

#ifndef AV1_COMMON_SCAN_H_
#define AV1_COMMON_SCAN_H_

#include "aom/aom_integer.h"
#include "aom_ports/mem.h"

#include "av1/common/blockd.h"
#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NEIGHBORS 2

typedef struct {
  const int16_t *scan;
  const int16_t *iscan;
  const int16_t *neighbors;
} SCAN_ORDER;

extern const SCAN_ORDER av1_default_scan_orders[TX_SIZES];
extern const SCAN_ORDER av1_scan_orders[TX_SIZES][TX_TYPES];

#if CONFIG_ADAPT_SCAN
void update_scan_prob(AV1_COMMON *cm, TX_SIZE tx_size, TX_TYPE tx_type,
                      int rate_16);
#endif

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >>
         1;
}

static INLINE const SCAN_ORDER *get_scan(TX_SIZE tx_size, TX_TYPE tx_type) {
  return &av1_scan_orders[tx_size][tx_type];
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_SCAN_H_
