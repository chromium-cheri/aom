/*
 * Copyright (c) 2021, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include "aom/aom_codec.h"
#include "aom/aomdx.h"
#include "aom_mem/aom_mem.h"
#include "av1/av1_iface_common.h"
#include "av1/encoder/encoder.h"
#include "av1/encoder/firstpass.h"
#include "av1/encoder/thirdpass.h"
#include "av1/common/blockd.h"

#if CONFIG_THREE_PASS
#include "common/ivfdec.h"
#endif

#if CONFIG_THREE_PASS
static void setup_two_pass_stream_input(
    struct AvxInputContext **input_ctx_ptr, const char *input_file_name,
    struct aom_internal_error_info *err_info) {
  FILE *infile;
  infile = fopen(input_file_name, "rb");
  if (!infile) {
    aom_internal_error(err_info, AOM_CODEC_INVALID_PARAM,
                       "Failed to open input file '%s'.", input_file_name);
  }
  struct AvxInputContext *aom_input_ctx = aom_malloc(sizeof(*aom_input_ctx));
  if (!aom_input_ctx) {
    fclose(infile);
    aom_internal_error(err_info, AOM_CODEC_MEM_ERROR,
                       "Failed to allocate memory for third-pass context.");
  }
  memset(aom_input_ctx, 0, sizeof(*aom_input_ctx));
  aom_input_ctx->filename = input_file_name;
  aom_input_ctx->file = infile;

  if (file_is_ivf(aom_input_ctx)) {
    aom_input_ctx->file_type = FILE_TYPE_IVF;
  } else {
    fclose(infile);
    aom_free(aom_input_ctx);
    aom_internal_error(err_info, AOM_CODEC_INVALID_PARAM,
                       "Unrecognized input file type.");
  }
  *input_ctx_ptr = aom_input_ctx;
}

static void init_third_pass(THIRD_PASS_DEC_CTX *ctx) {
  if (!ctx->input_ctx) {
    if (ctx->input_file_name == NULL) {
      aom_internal_error(ctx->err_info, AOM_CODEC_INVALID_PARAM,
                         "No third pass input specified.");
    }
    setup_two_pass_stream_input(&ctx->input_ctx, ctx->input_file_name,
                                ctx->err_info);
  }

#if CONFIG_AV1_DECODER
  if (!ctx->decoder.iface) {
    aom_codec_iface_t *decoder_iface = &aom_codec_av1_inspect_algo;
    if (aom_codec_dec_init(&ctx->decoder, decoder_iface, NULL, 0)) {
      aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                         "Failed to initialize decoder.");
    }
  }
#else
  aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                     "To utilize three-pass encoding, libaom must be built "
                     "with CONFIG_AV1_DECODER=1.");
#endif
}
#endif  // CONFIG_THREE_PASS

// Return 0: success
//        1: cannot read because this is end of file
//       -1: failure to read the frame
static int read_frame(THIRD_PASS_DEC_CTX *ctx) {
#if CONFIG_THREE_PASS
  if (!ctx->input_ctx || !ctx->decoder.iface) {
    init_third_pass(ctx);
  }
  if (!ctx->have_frame) {
    if (ivf_read_frame(ctx->input_ctx->file, &ctx->buf, &ctx->bytes_in_buffer,
                       &ctx->buffer_size, NULL) != 0) {
      if (feof(ctx->input_ctx->file)) {
        return 1;
      } else {
        return -1;
      }
    }
    ctx->frame = ctx->buf;
    ctx->end_frame = ctx->frame + ctx->bytes_in_buffer;
    ctx->have_frame = 1;
  }
#else
  aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                     "Cannot parse bitstream without CONFIG_THREE_PASS.");
#endif
  Av1DecodeReturn adr;
  if (aom_codec_decode(&ctx->decoder, ctx->frame,
                       (unsigned int)ctx->bytes_in_buffer,
                       &adr) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to decode frame for third pass.");
  }
  ctx->frame = adr.buf;
  ctx->bytes_in_buffer = ctx->end_frame - ctx->frame;
  if (ctx->frame == ctx->end_frame) ctx->have_frame = 0;
  return 0;
}

// This function gets the information needed from the recently decoded frame,
// via various decoder APIs, and saves the info into ctx->frame_info.
// Return 0: success
//        1: cannot read because this is end of file
//       -1: failure to read the frame
static int get_frame_info(THIRD_PASS_DEC_CTX *ctx) {
  int ret = read_frame(ctx);
  if (ret != 0) return ret;
  int cur = ctx->frame_info_count;
  if (cur >= MAX_THIRD_PASS_BUF) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Third pass frame info ran out of available slots.");
  }
  int frame_type_flags = 0;
  if (aom_codec_control(&ctx->decoder, AOMD_GET_FRAME_FLAGS,
                        &frame_type_flags) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read frame flags.");
  }
  if (frame_type_flags & AOM_FRAME_IS_KEY) {
    ctx->frame_info[cur].frame_type = KEY_FRAME;
  } else if (frame_type_flags & AOM_FRAME_IS_INTRAONLY) {
    ctx->frame_info[cur].frame_type = INTRA_ONLY_FRAME;
  } else if (frame_type_flags & AOM_FRAME_IS_SWITCH) {
    ctx->frame_info[cur].frame_type = S_FRAME;
  } else {
    ctx->frame_info[cur].frame_type = INTER_FRAME;
  }

  // Get frame base q idx
  if (aom_codec_control(&ctx->decoder, AOMD_GET_BASE_Q_IDX,
                        &ctx->frame_info[cur].base_q_idx) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read base q index.");
  }

  // Get show existing frame flag
  if (aom_codec_control(&ctx->decoder, AOMD_GET_SHOW_EXISTING_FRAME_FLAG,
                        &ctx->frame_info[cur].is_show_existing_frame) !=
      AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read show existing frame flag.");
  }

  // Get show frame flag
  if (aom_codec_control(&ctx->decoder, AOMD_GET_SHOW_FRAME_FLAG,
                        &ctx->frame_info[cur].is_show_frame) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read show frame flag.");
  }

  // Get order hint
  if (aom_codec_control(&ctx->decoder, AOMD_GET_ORDER_HINT,
                        &ctx->frame_info[cur].order_hint) != AOM_CODEC_OK) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Failed to read order hint.");
  }
  ctx->frame_info_count++;
  return 0;
}

#define USE_SECOND_PASS_FILE 1

#if !USE_SECOND_PASS_FILE
// Parse the frames in the gop and determine the last frame of the current GOP.
// Decode more frames if necessary. The variable max_num is the maximum static
// GOP length if we detect an IPPP structure, and it is expected that max_mum >=
// MAX_GF_INTERVAL.
static void get_current_gop_end(THIRD_PASS_DEC_CTX *ctx, int max_num,
                                int *last_idx) {
  assert(max_num >= MAX_GF_INTERVAL);
  *last_idx = 0;
  int cur_idx = 0;
  int arf_order_hint = -1;
  int num_show_frames = 0;
  while (num_show_frames < max_num) {
    assert(cur_idx < MAX_THIRD_PASS_BUF);
    // Read in from bitstream if needed.
    if (cur_idx >= ctx->frame_info_count) {
      int ret = get_frame_info(ctx);
      if (ret == 1) {
        // At the end of the file, GOP ends in the prev frame.
        if (arf_order_hint >= 0) {
          aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                             "Failed to derive GOP length.");
        }
        *last_idx = cur_idx - 1;
        return;
      }
      if (ret < 0) {
        aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                           "Failed to read frame for third pass.");
      }
    }

    // TODO(bohanli): verify that fwd_kf works here.
    if (ctx->frame_info[cur_idx].frame_type == KEY_FRAME &&
        ctx->frame_info[cur_idx].is_show_frame) {
      if (cur_idx != 0) {
        // If this is a key frame and is not the first kf in this kf group, we
        // have reached the next key frame. Stop here.
        *last_idx = cur_idx - 1;
        return;
      }
    } else if (!ctx->frame_info[cur_idx].is_show_frame &&
               arf_order_hint == -1) {
      // If this is an arf (the first no show)
      if (num_show_frames <= 1) {
        // This is an arf and we should end the GOP with its overlay.
        arf_order_hint = ctx->frame_info[cur_idx].order_hint;
      } else {
        // There are multiple show frames before the this arf, so we treat the
        // frames previous to this arf as a GOP.
        *last_idx = cur_idx - 1;
        return;
      }
    } else if (arf_order_hint >= 0 && ctx->frame_info[cur_idx].order_hint ==
                                          (unsigned int)arf_order_hint) {
      // If this is the overlay/show existing of the arf
      assert(ctx->frame_info[cur_idx].is_show_frame);
      *last_idx = cur_idx;
      return;
    } else {
      // This frame is part of the GOP.
      if (ctx->frame_info[cur_idx].is_show_frame) num_show_frames++;
    }
    cur_idx++;
  }
  // This is a long IPPP GOP and we will use a length of max_num here.
  assert(arf_order_hint < 0);
  *last_idx = max_num - 1;
  return;
}
#endif

static AOM_INLINE void read_gop_frames(THIRD_PASS_DEC_CTX *ctx) {
  int cur_idx = 0;
  while (cur_idx < ctx->gop_info.num_frames) {
    assert(cur_idx < MAX_THIRD_PASS_BUF);
    // Read in from bitstream if needed.
    if (cur_idx >= ctx->frame_info_count) {
      int ret = get_frame_info(ctx);
      if (ret != 0) {
        aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                           "Failed to read frame for third pass.");
      }
    }
    cur_idx++;
  }
  return;
}

void av1_set_gop_third_pass(THIRD_PASS_DEC_CTX *ctx) {
  // Read in future frames in the current GOP.
  read_gop_frames(ctx);

  int gf_len = 0;
  // Check the GOP length against the value read from second_pass_file
  for (int i = 0; i < ctx->gop_info.num_frames; i++) {
    if (ctx->frame_info[i].is_show_frame) gf_len++;
  }

  if (gf_len != ctx->gop_info.gf_length) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "Mismatch in third pass GOP length!");
  }
}

void av1_pop_third_pass_info(THIRD_PASS_DEC_CTX *ctx) {
  if (ctx->frame_info_count == 0) {
    aom_internal_error(ctx->err_info, AOM_CODEC_ERROR,
                       "No available frame info for third pass.");
  }
  ctx->frame_info_count--;
  for (int i = 0; i < ctx->frame_info_count; i++) {
    ctx->frame_info[i] = ctx->frame_info[i + 1];
  }
}

void av1_init_thirdpass_ctx(AV1_COMMON *cm, THIRD_PASS_DEC_CTX **ctx,
                            const char *file) {
  av1_free_thirdpass_ctx(*ctx);
  CHECK_MEM_ERROR(cm, *ctx, aom_calloc(1, sizeof(**ctx)));
  THIRD_PASS_DEC_CTX *ctx_ptr = *ctx;
  ctx_ptr->input_file_name = file;
  ctx_ptr->prev_gop_end = -1;
  ctx_ptr->err_info = cm->error;
}

void av1_free_thirdpass_ctx(THIRD_PASS_DEC_CTX *ctx) {
  if (ctx == NULL) return;
  if (ctx->decoder.iface) {
    aom_codec_destroy(&ctx->decoder);
  }
#if CONFIG_THREE_PASS
  if (ctx->input_ctx && ctx->input_ctx->file) fclose(ctx->input_ctx->file);
  aom_free(ctx->input_ctx);
#endif
  if (ctx->buf) free(ctx->buf);
  aom_free(ctx);
}

void av1_write_second_pass_gop_info(AV1_COMP *cpi) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  const GF_GROUP *const gf_group = &cpi->ppi->gf_group;
  const PRIMARY_RATE_CONTROL *const p_rc = &cpi->ppi->p_rc;

  if (oxcf->pass == AOM_RC_SECOND_PASS && oxcf->second_pass_file) {
    // Write the GOP length in a file.
    if (!cpi->second_pass_file_stream) {
      cpi->second_pass_file_stream = fopen(cpi->oxcf.second_pass_file, "wb");
      if (!cpi->second_pass_file_stream) {
        aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                           "Could not open second pass file!");
      }
    }

    THIRD_PASS_GOP_INFO gop_info;

    gop_info.num_frames = gf_group->size;
    gop_info.use_arf = (gf_group->arf_index >= 0);
    gop_info.gf_length = p_rc->baseline_gf_interval;

    size_t c =
        fwrite(&gop_info, sizeof(gop_info), 1, cpi->second_pass_file_stream);
    if (c < 1) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not write to second pass file!");
    }
  }
}

void av1_read_second_pass_gop_info(AV1_COMP *cpi,
                                   THIRD_PASS_GOP_INFO *gop_info) {
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;

  if (oxcf->pass == AOM_RC_THIRD_PASS) {
    if (oxcf->second_pass_file == NULL) {
      aom_internal_error(cpi->common.error, AOM_CODEC_INVALID_PARAM,
                         "No second pass file specified for the third pass!");
    }
    // Write the GOP length in a file.
    if (!cpi->second_pass_file_stream) {
      cpi->second_pass_file_stream = fopen(cpi->oxcf.second_pass_file, "rb");
      if (!cpi->second_pass_file_stream) {
        aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                           "Could not open second pass file!");
      }
    }

    size_t c =
        fread(gop_info, sizeof(*gop_info), 1, cpi->second_pass_file_stream);
    if (c < 1) {
      aom_internal_error(cpi->common.error, AOM_CODEC_ERROR,
                         "Could not read from second pass file!");
    }
  }
}
