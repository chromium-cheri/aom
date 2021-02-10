/*
 *  Copyright (c) 2019, Alliance for Open Media. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <math.h>

#include "av1/encoder/encoder.h"

static void swap_ptr(void *a, void *b) {
  void **a_p = (void **)a;
  void **b_p = (void **)b;
  void *c = *a_p;
  *a_p = *b_p;
  *b_p = c;
}

void av1_init_layer_context(AV1_COMP *const cpi) {
  AV1_COMMON *const cm = &cpi->common;
  const AV1EncoderConfig *const oxcf = &cpi->oxcf;
  SVC *const svc = &cpi->svc;
  int mi_rows = cpi->common.mi_params.mi_rows;
  int mi_cols = cpi->common.mi_params.mi_cols;
  svc->base_framerate = 30.0;
  svc->current_superframe = 0;
  svc->force_zero_mode_spatial_ref = 1;
  svc->num_encoded_top_layer = 0;

  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      RATE_CONTROL *const lrc = &lc->rc;
      lrc->ni_av_qi = oxcf->rc_cfg.worst_allowed_q;
      lrc->total_actual_bits = 0;
      lrc->ni_tot_qi = 0;
      lrc->tot_q = 0.0;
      lrc->avg_q = 0.0;
      lrc->ni_frames = 0;
      lrc->decimation_count = 0;
      lrc->decimation_factor = 0;
      lrc->worst_quality = av1_quantizer_to_qindex(lc->max_q);
      lrc->best_quality = av1_quantizer_to_qindex(lc->min_q);
      for (int i = 0; i < RATE_FACTOR_LEVELS; ++i) {
        lrc->rate_correction_factors[i] = 1.0;
      }
      lc->target_bandwidth = lc->layer_target_bitrate;
      lrc->last_q[INTER_FRAME] = lrc->worst_quality;
      lrc->avg_frame_qindex[INTER_FRAME] = lrc->worst_quality;
      lrc->avg_frame_qindex[KEY_FRAME] = lrc->worst_quality;
      lrc->buffer_level =
          oxcf->rc_cfg.starting_buffer_level_ms * lc->target_bandwidth / 1000;
      lrc->bits_off_target = lrc->buffer_level;
      // Initialize the cyclic refresh parameters. If spatial layers are used
      // (i.e., ss_number_layers > 1), these need to be updated per spatial
      // layer. Cyclic refresh is only applied on base temporal layer.
      if (svc->number_spatial_layers > 1 && tl == 0) {
        size_t last_coded_q_map_size;
        lc->sb_index = 0;
        lc->actual_num_seg1_blocks = 0;
        lc->actual_num_seg2_blocks = 0;
        lc->counter_encode_maxq_scene_change = 0;
        if (lc->map) aom_free(lc->map);
        CHECK_MEM_ERROR(cm, lc->map,
                        aom_malloc(mi_rows * mi_cols * sizeof(*lc->map)));
        memset(lc->map, 0, mi_rows * mi_cols);
        last_coded_q_map_size =
            mi_rows * mi_cols * sizeof(*lc->last_coded_q_map);
        if (lc->last_coded_q_map) aom_free(lc->last_coded_q_map);
        CHECK_MEM_ERROR(cm, lc->last_coded_q_map,
                        aom_malloc(last_coded_q_map_size));
        assert(MAXQ <= 255);
        memset(lc->last_coded_q_map, MAXQ, last_coded_q_map_size);
      }
    }
    svc->downsample_filter_type[sl] = BILINEAR;
    svc->downsample_filter_phase[sl] = 8;
    svc->max_threads[sl] = 1;
    svc->tile_columns[sl] = 0;
    svc->tile_rows[sl] = 0;
  }
  if (svc->number_spatial_layers == 3) {
    svc->downsample_filter_type[0] = EIGHTTAP_SMOOTH;
  }
}

// Update the layer context from a change_config() call.
void av1_update_layer_context_change_config(AV1_COMP *const cpi,
                                            const int64_t target_bandwidth) {
  const RATE_CONTROL *const rc = &cpi->rc;
  SVC *const svc = &cpi->svc;
  int layer = 0;
  int64_t spatial_layer_target = 0;
  float bitrate_alloc = 1.0;

  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      svc->layer_context[layer].target_bandwidth = lc->layer_target_bitrate;
    }
    spatial_layer_target = svc->layer_context[layer].target_bandwidth;
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      LAYER_CONTEXT *const lc =
          &svc->layer_context[sl * svc->number_temporal_layers + tl];
      RATE_CONTROL *const lrc = &lc->rc;
      lc->spatial_layer_target_bandwidth = spatial_layer_target;
      bitrate_alloc = (float)lc->target_bandwidth / target_bandwidth;
      lrc->starting_buffer_level =
          (int64_t)(rc->starting_buffer_level * bitrate_alloc);
      lrc->optimal_buffer_level =
          (int64_t)(rc->optimal_buffer_level * bitrate_alloc);
      lrc->maximum_buffer_size =
          (int64_t)(rc->maximum_buffer_size * bitrate_alloc);
      lrc->bits_off_target =
          AOMMIN(lrc->bits_off_target, lrc->maximum_buffer_size);
      lrc->buffer_level = AOMMIN(lrc->buffer_level, lrc->maximum_buffer_size);
      lc->framerate = cpi->framerate / lc->framerate_factor;
      lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
      lrc->max_frame_bandwidth = rc->max_frame_bandwidth;
      lrc->worst_quality = av1_quantizer_to_qindex(lc->max_q);
      lrc->best_quality = av1_quantizer_to_qindex(lc->min_q);
    }
    // The user settings of mult-threading are only applied to top
    // spatial layer for now, keep lower spatial layers at 1 thread.
    if (sl == svc->number_spatial_layers - 1) {
      svc->max_threads[sl] = cpi->oxcf.max_threads;
      svc->tile_columns[sl] = cpi->oxcf.tile_cfg.tile_columns;
      svc->tile_rows[sl] = cpi->oxcf.tile_cfg.tile_rows;
    } else {
      svc->max_threads[sl] = 1;
      svc->tile_columns[sl] = 0;
      svc->tile_rows[sl] = 0;
    }
  }
}

/*!\brief Return layer context for current layer.
 *
 * \ingroup rate_control
 * \param[in]       cpi   Top level encoder structure
 *
 * \return LAYER_CONTEXT for current layer.
 */
static LAYER_CONTEXT *get_layer_context(AV1_COMP *const cpi) {
  return &cpi->svc.layer_context[cpi->svc.spatial_layer_id *
                                     cpi->svc.number_temporal_layers +
                                 cpi->svc.temporal_layer_id];
}

void av1_update_temporal_layer_framerate(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  RATE_CONTROL *const lrc = &lc->rc;
  const int tl = svc->temporal_layer_id;
  lc->framerate = cpi->framerate / lc->framerate_factor;
  lrc->avg_frame_bandwidth = (int)(lc->target_bandwidth / lc->framerate);
  lrc->max_frame_bandwidth = cpi->rc.max_frame_bandwidth;
  // Update the average layer frame size (non-cumulative per-frame-bw).
  if (tl == 0) {
    lc->avg_frame_size = lrc->avg_frame_bandwidth;
  } else {
    int prev_layer = svc->spatial_layer_id * svc->number_temporal_layers +
                     svc->temporal_layer_id - 1;
    LAYER_CONTEXT *const lcprev = &svc->layer_context[prev_layer];
    const double prev_layer_framerate =
        cpi->framerate / lcprev->framerate_factor;
    const int64_t prev_layer_target_bandwidth = lcprev->layer_target_bitrate;
    lc->avg_frame_size =
        (int)((lc->target_bandwidth - prev_layer_target_bandwidth) /
              (lc->framerate - prev_layer_framerate));
  }
}

void av1_restore_layer_context(AV1_COMP *const cpi) {
  GF_GROUP *const gf_group = &cpi->gf_group;
  SVC *const svc = &cpi->svc;
  const AV1_COMMON *const cm = &cpi->common;
  LAYER_CONTEXT *const lc = get_layer_context(cpi);
  const int old_frame_since_key = cpi->rc.frames_since_key;
  const int old_frame_to_key = cpi->rc.frames_to_key;
  // Restore layer rate control.
  cpi->rc = lc->rc;
  cpi->oxcf.rc_cfg.target_bandwidth = lc->target_bandwidth;
  gf_group->index = 0;
  cpi->mv_search_params.max_mv_magnitude = lc->max_mv_magnitude;
  if (cpi->mv_search_params.max_mv_magnitude == 0)
    cpi->mv_search_params.max_mv_magnitude = AOMMAX(cm->width, cm->height);
  // Reset the frames_since_key and frames_to_key counters to their values
  // before the layer restore. Keep these defined for the stream (not layer).
  cpi->rc.frames_since_key = old_frame_since_key;
  cpi->rc.frames_to_key = old_frame_to_key;
  // For spatial-svc, allow cyclic-refresh to be applied on the spatial layers,
  // for the base temporal layer.
  if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
      svc->number_spatial_layers > 1 && svc->temporal_layer_id == 0) {
    CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
    swap_ptr(&cr->map, &lc->map);
    swap_ptr(&cr->last_coded_q_map, &lc->last_coded_q_map);
    cr->sb_index = lc->sb_index;
    cr->actual_num_seg1_blocks = lc->actual_num_seg1_blocks;
    cr->actual_num_seg2_blocks = lc->actual_num_seg2_blocks;
  }
  svc->skip_mvsearch_last = 0;
  svc->skip_mvsearch_gf = 0;
  // For each reference (LAST/GOLDEN) set the skip_mvsearch_last/gf frame flags.
  // This is to skip searching mv for that reference if it was last
  // refreshed (i.e., buffer slot holding that reference was refreshed) on the
  // previous spatial layer(s) at the same time (current_superframe).
  if (svc->external_ref_frame_config && svc->force_zero_mode_spatial_ref) {
    int ref_frame_idx = svc->ref_idx[LAST_FRAME - 1];
    if (svc->buffer_time_index[ref_frame_idx] == svc->current_superframe &&
        svc->buffer_spatial_layer[ref_frame_idx] <= svc->spatial_layer_id - 1)
      svc->skip_mvsearch_last = 1;
    ref_frame_idx = svc->ref_idx[GOLDEN_FRAME - 1];
    if (svc->buffer_time_index[ref_frame_idx] == svc->current_superframe &&
        svc->buffer_spatial_layer[ref_frame_idx] <= svc->spatial_layer_id - 1)
      svc->skip_mvsearch_gf = 1;
  }
}

void av1_save_layer_context(AV1_COMP *const cpi) {
  GF_GROUP *const gf_group = &cpi->gf_group;
  SVC *const svc = &cpi->svc;
  const AV1_COMMON *const cm = &cpi->common;
  LAYER_CONTEXT *lc = get_layer_context(cpi);
  lc->rc = cpi->rc;
  lc->target_bandwidth = (int)cpi->oxcf.rc_cfg.target_bandwidth;
  lc->group_index = gf_group->index;
  lc->max_mv_magnitude = cpi->mv_search_params.max_mv_magnitude;
  if (svc->spatial_layer_id == 0) svc->base_framerate = cpi->framerate;
  // For spatial-svc, allow cyclic-refresh to be applied on the spatial layers,
  // for the base temporal layer.
  if (cpi->oxcf.q_cfg.aq_mode == CYCLIC_REFRESH_AQ &&
      cpi->svc.number_spatial_layers > 1 && svc->temporal_layer_id == 0) {
    CYCLIC_REFRESH *const cr = cpi->cyclic_refresh;
    signed char *temp = lc->map;
    uint8_t *temp2 = lc->last_coded_q_map;
    lc->map = cr->map;
    cr->map = temp;
    lc->last_coded_q_map = cr->last_coded_q_map;
    cr->last_coded_q_map = temp2;
    lc->sb_index = cr->sb_index;
    lc->actual_num_seg1_blocks = cr->actual_num_seg1_blocks;
    lc->actual_num_seg2_blocks = cr->actual_num_seg2_blocks;
  }
  // For any buffer slot that is refreshed, update it with
  // the spatial_layer_id and the current_superframe.
  if (cpi->common.current_frame.frame_type == KEY_FRAME) {
    // All slots are refreshed on KEY.
    for (unsigned int i = 0; i < REF_FRAMES; i++) {
      svc->buffer_time_index[i] = svc->current_superframe;
      svc->buffer_spatial_layer[i] = svc->spatial_layer_id;
    }
  } else if (cpi->svc.external_ref_frame_config) {
    for (unsigned int i = 0; i < INTER_REFS_PER_FRAME; i++) {
      int ref_frame_map_idx = svc->ref_idx[i];
      if (cpi->svc.refresh[ref_frame_map_idx]) {
        svc->buffer_time_index[ref_frame_map_idx] = svc->current_superframe;
        svc->buffer_spatial_layer[ref_frame_map_idx] = svc->spatial_layer_id;
      }
    }
  }
  for (unsigned int i = 0; i < REF_FRAMES; i++) {
    if (frame_is_intra_only(cm) ||
        cm->current_frame.refresh_frame_flags & (1 << i)) {
      svc->spatial_layer_fb[i] = svc->spatial_layer_id;
      svc->temporal_layer_fb[i] = svc->temporal_layer_id;
    }
  }
  if (svc->spatial_layer_id == svc->number_spatial_layers - 1)
    svc->current_superframe++;
}

int av1_svc_primary_ref_frame(const AV1_COMP *const cpi) {
  const SVC *const svc = &cpi->svc;
  const AV1_COMMON *const cm = &cpi->common;
  int wanted_fb = -1;
  int primary_ref_frame = PRIMARY_REF_NONE;
  for (unsigned int i = 0; i < REF_FRAMES; i++) {
    if (svc->spatial_layer_fb[i] == svc->spatial_layer_id &&
        svc->temporal_layer_fb[i] == svc->temporal_layer_id) {
      wanted_fb = i;
      break;
    }
  }
  if (wanted_fb != -1) {
    for (int ref_frame = LAST_FRAME; ref_frame <= ALTREF_FRAME; ref_frame++) {
      if (get_ref_frame_map_idx(cm, ref_frame) == wanted_fb) {
        primary_ref_frame = ref_frame - LAST_FRAME;
        break;
      }
    }
  }
  return primary_ref_frame;
}

void av1_free_svc_cyclic_refresh(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      int layer = LAYER_IDS_TO_IDX(sl, tl, svc->number_temporal_layers);
      LAYER_CONTEXT *const lc = &svc->layer_context[layer];
      if (lc->map) aom_free(lc->map);
      if (lc->last_coded_q_map) aom_free(lc->last_coded_q_map);
    }
  }
}

void av1_svc_reset_temporal_layers(AV1_COMP *const cpi, int is_key) {
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  for (int sl = 0; sl < svc->number_spatial_layers; ++sl) {
    for (int tl = 0; tl < svc->number_temporal_layers; ++tl) {
      lc = &cpi->svc.layer_context[sl * svc->number_temporal_layers + tl];
      if (is_key) lc->frames_from_key_frame = 0;
    }
  }
  av1_update_temporal_layer_framerate(cpi);
  av1_restore_layer_context(cpi);
}

void av1_get_layer_resolution(const int width_org, const int height_org,
                              const int num, const int den, int *width_out,
                              int *height_out) {
  int w, h;
  if (width_out == NULL || height_out == NULL || den == 0) return;
  w = width_org * num / den;
  h = height_org * num / den;
  // Make height and width even.
  w += w % 2;
  h += h % 2;
  *width_out = w;
  *height_out = h;
}

void av1_one_pass_cbr_svc_start_layer(AV1_COMP *const cpi) {
  SVC *const svc = &cpi->svc;
  LAYER_CONTEXT *lc = NULL;
  int width = 0, height = 0;
  lc = &svc->layer_context[svc->spatial_layer_id * svc->number_temporal_layers +
                           svc->temporal_layer_id];
  av1_get_layer_resolution(cpi->oxcf.frm_dim_cfg.width,
                           cpi->oxcf.frm_dim_cfg.height, lc->scaling_factor_num,
                           lc->scaling_factor_den, &width, &height);
  // Use Eightap_smooth for low resolutions.
  if (width * height <= 320 * 240)
    svc->downsample_filter_type[svc->spatial_layer_id] = EIGHTTAP_SMOOTH;

  cpi->common.width = width;
  cpi->common.height = height;
  update_frame_size(cpi);
}

void av1_svc_set_mt_per_spatial_layer(AV1_COMP *const cpi) {
  int sl = cpi->svc.spatial_layer_id;
  cpi->oxcf.max_threads = cpi->svc.max_threads[sl];
  cpi->oxcf.tile_cfg.tile_columns = cpi->svc.tile_columns[sl];
  cpi->oxcf.tile_cfg.tile_rows = cpi->svc.tile_rows[sl];
}
