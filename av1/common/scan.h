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

<<<<<<< HEAD   (fd601e Merge "Rename av1_convolve.[hc] to convolve.[hc]" into nextg)
#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"
#include "av1/common/blockd.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NEIGHBORS 2

extern const SCAN_ORDER av1_default_scan_orders[TX_SIZES];
extern const SCAN_ORDER av1_intra_scan_orders[TX_SIZES][TX_TYPES];

#if CONFIG_ADAPT_SCAN
void av1_update_scan_prob(AV1_COMMON *cm, TX_SIZE tx_size, TX_TYPE tx_type,
                          int rate_16);
void av1_update_scan_count_facade(AV1_COMMON *cm, TX_SIZE tx_size,
                                  TX_TYPE tx_type, const tran_low_t *dqcoeffs,
                                  int max_scan);

// embed r + c and coeff_idx info with nonzero probabilities. When sorting the
// nonzero probabilities, if there is a tie, the coefficient with smaller r + c
// will be scanned first
void av1_augment_prob(uint32_t *prob, int size, int tx1d_size);

// apply quick sort on nonzero probabilities to obtain a sort order
void av1_update_sort_order(TX_SIZE tx_size, const uint32_t *non_zero_prob,
                           int16_t *sort_order);

// apply topological sort on the nonzero probabilities sorting order to
// guarantee each to-be-scanned coefficient's upper and left coefficient will be
// scanned before the to-be-scanned coefficient.
void av1_update_scan_order(TX_SIZE tx_size, int16_t *sort_order, int16_t *scan,
                           int16_t *iscan);

// For each coeff_idx in scan[], update its above and left neighbors in
// neighbors[] accordingly.
void av1_update_neighbors(int tx_size, const int16_t *scan,
                          const int16_t *iscan, int16_t *neighbors);
void av1_update_scan_order_facade(AV1_COMMON *cm, TX_SIZE tx_size,
                                  TX_TYPE tx_type);
void av1_init_scan_order(AV1_COMMON *cm);
#endif

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >>
         1;
}

static INLINE const SCAN_ORDER *get_intra_scan(TX_SIZE tx_size,
                                               TX_TYPE tx_type) {
  return &av1_intra_scan_orders[tx_size][tx_type];
}

#if CONFIG_EXT_TX
extern const SCAN_ORDER av1_inter_scan_orders[TX_SIZES_ALL][TX_TYPES];

static INLINE const SCAN_ORDER *get_inter_scan(TX_SIZE tx_size,
                                               TX_TYPE tx_type) {
  return &av1_inter_scan_orders[tx_size][tx_type];
}
#endif  // CONFIG_EXT_TX

static INLINE const SCAN_ORDER *get_scan(const AV1_COMMON *cm, TX_SIZE tx_size,
                                         TX_TYPE tx_type, int is_inter) {
#if CONFIG_ADAPT_SCAN
  (void)is_inter;
  return &cm->fc->sc[tx_size][tx_type];
#else  // CONFIG_ADAPT_SCAN
  (void)cm;
#if CONFIG_EXT_TX
  return is_inter ? &av1_inter_scan_orders[tx_size][tx_type]
                  : &av1_intra_scan_orders[tx_size][tx_type];
#else
  (void)is_inter;
  return &av1_intra_scan_orders[tx_size][tx_type];
#endif  // CONFIG_EXT_TX
#endif  // CONFIG_ADAPT_SCAN
=======
#include "av1/common/blockd.h"
#include "av1/common/entropymode.h"
#include "av1/common/enums.h"
#include "av1/common/onyxc_int.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_NEIGHBORS 2

extern const SCAN_ORDER av1_default_scan_orders[TX_SIZES];
extern const SCAN_ORDER av1_scan_orders[TX_SIZES][TX_TYPES];

#if CONFIG_ADAPT_SCAN
void av1_update_scan_prob(AV1_COMMON *cm, TX_SIZE tx_size, TX_TYPE tx_type,
                          int rate_16);
void av1_update_scan_count_facade(AV1_COMMON *cm, TX_SIZE tx_size,
                                  TX_TYPE tx_type, const tran_low_t *dqcoeffs,
                                  int max_scan);

// embed r + c and coeff_idx info with nonzero probabilities. When sorting the
// nonzero probabilities, if there is a tie, the coefficient with smaller r + c
// will be scanned first
void av1_augment_prob(uint32_t *prob, int size, int tx1d_size);

// apply quick sort on nonzero probabilities to obtain a sort order
void av1_update_sort_order(TX_SIZE tx_size, const uint32_t *non_zero_prob,
                           int16_t *sort_order);

// apply topological sort on the nonzero probabilities sorting order to
// guarantee each to-be-scanned coefficient's upper and left coefficient will be
// scanned before the to-be-scanned coefficient.
void av1_update_scan_order(TX_SIZE tx_size, int16_t *sort_order, int16_t *scan,
                           int16_t *iscan);

// For each coeff_idx in scan[], update its above and left neighbors in
// neighbors[] accordingly.
void av1_update_neighbors(int tx_size, const int16_t *scan,
                          const int16_t *iscan, int16_t *neighbors);
void av1_update_scan_order_facade(AV1_COMMON *cm, TX_SIZE tx_size,
                                  TX_TYPE tx_type);
void av1_init_scan_order(AV1_COMMON *cm);
#endif

static INLINE int get_coef_context(const int16_t *neighbors,
                                   const uint8_t *token_cache, int c) {
  return (1 + token_cache[neighbors[MAX_NEIGHBORS * c + 0]] +
          token_cache[neighbors[MAX_NEIGHBORS * c + 1]]) >>
         1;
}

static INLINE const SCAN_ORDER *get_scan(const AV1_COMMON *const cm,
                                         TX_SIZE tx_size, TX_TYPE tx_type) {
#if CONFIG_ADAPT_SCAN
  return &cm->fc->sc[tx_size][tx_type];
#endif
  (void)cm;
  return &av1_scan_orders[tx_size][tx_type];
>>>>>>> BRANCH (0fcd3e cmake support: A starting point.)
}

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // AV1_COMMON_SCAN_H_
