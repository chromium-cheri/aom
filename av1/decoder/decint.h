/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

/* clang-format off */

#if !defined(_decint_H)
# define _decint_H (1)
# include "av1/common/pvq_state.h"
# include "aom_dsp/bitreader.h"
# include "aom_dsp/entdec.h"

typedef struct DaalaDecCtx DaalaDecCtx;

typedef struct DaalaDecCtx OdDecCtx;


struct DaalaDecCtx {
  /* Stores context-adaptive CDFs for PVQ. */
  OdState state;
  /* AOM entropy decoder. */
  AomReader *r;
  int use_activity_masking;
  /* Mode of quantization matrice : FLAT (0) or HVS (1) */
  int qm;
};

#endif
