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

#if !defined(_pvq_encoder_H)
# define _pvq_encoder_H (1)
# include "aom_dsp/bitwriter.h"
# include "aom_dsp/entenc.h"
# include "av1/common/blockd.h"
# include "av1/common/pvq.h"
# include "av1/encoder/encint.h"

void aom_write_symbol_pvq(AomWriter *w, int symb, AomCdfProb *cdf,
    int nsymbs);

void aom_encode_band_pvq_splits(AomWriter *w, OdPvqCodewordCtx *adapt,
 const int *y, int n, int k, int level);

void aom_laplace_encode_special(AomWriter *w, int x, unsigned decay);

void pvq_encode_partition(AomWriter *w,
                                 int qg,
                                 int theta,
                                 const OdCoeff *in,
                                 int n,
                                 int k,
                                 GenericEncoder model[3],
                                 OdAdaptCtx *adapt,
                                 int *exg,
                                 int *ext,
                                 int cdf_ctx,
                                 int is_keyframe,
                                 int code_skip,
                                 int skip_rest,
                                 int encode_flip,
                                 int flip);

PvqSkipType od_pvq_encode(DaalaEncCtx *enc, OdCoeff *ref,
    const OdCoeff *in, OdCoeff *out, int q_dc, int q_ac, int pli, int bs,
    const OdVal16 *beta, int is_keyframe,
    const int16_t *qm, const int16_t *qm_inv, int speed,
    PvqInfo *pvq_info);

#endif
