/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef __AV1HWD_DECODER_H__
#define __AV1HWD_DECODER_H__

#include "av1_commondec.h"
#include "basetype.h"

#include "av1_film_grain_dec.h"
#include "av1_global_motion.h"
#include "av1_obu.h"

#include "av1decapi.h"
#include "sw_util.h"

#define AV1_ACTIVE_REFS 3
#define AV1_ACTIVE_REFS_EX 7
#define AV1_REF_LIST_SIZE 8

#define DEC_8190_MODE_AV1 0x09U

#define AV1HWDEC_HW_RESERVED 0x0100
#define AV1HWDEC_SYSTEM_ERROR 0x0200
#define AV1HWDEC_SYSTEM_TIMEOUT 0x0300
#define AV1HWDEC_STREAM_ERROR 0x0400

#define MAX_NBR_OF_DCT_PARTITIONS (8)

#ifdef SUPPORT_VCMD_M2M
struct CDFsInfo {
  u32 load_num;
  struct CDF_INFO cdfs_load_info[MAX_VCMD_M2M_NUM];/* save CDFs trans info before decode */
  u32 save_num;
  struct CDF_INFO cdfs_save_info[MAX_VCMD_M2M_NUM]; /* save CDFs trans info after decode */
};
#endif

struct Av1Decoder {
  u32 dec_mode;

  /* Current frame dimensions */
  u32 width;
  u32 height;
  u32 scaled_width;
  u32 scaled_height;
  u32 last_width;
  u32 last_superres_width;
  u32 last_height;

  u32 vp_version;
  u32 vp_profile;

  u32 bit_depth;
  u32 key_frame;
  u32 prev_is_key_frame;
  u32 scaling_active;
  u32 resolution_change;

  u32 superres_width;
  u8 superres_is_scaled;
  u8 scale_denom_minus9;
  u32 superres_luma_step;
  u32 superres_chroma_step;
  u32 superres_luma_step_invra;
  u32 superres_chroma_step_invra;
  u32 superres_init_luma_subpel_x;
  u32 superres_init_chroma_subpel_x;

  /* DCT coefficient partitions */
  u32 offset_to_dct_parts;
  u32 dct_partition_offsets[MAX_NBR_OF_DCT_PARTITIONS];

  enum DecColorSpace color_space;
  enum DecVideoRange color_range;
  u32 clamping;
  u32 error_resilient;
  u32 show_frame;
  u32 prev_show_frame;
  u32 show_existing_frame;
  u32 show_existing_frame_index;
  u32 intra_only;
  u32 subsampling_x;
  u32 subsampling_y;

  u32 frame_context_idx;
  u32 active_ref_idx[ALLOWED_REFS_PER_FRAME_EX];
  i32 ref_frame_id[NUM_REF_FRAMES];
  bool valid_for_referencing[NUM_REF_FRAMES];
  u32 refresh_frame_flags;
  u32 refresh_entropy_probs;
  u32 frame_parallel_decoding;
  u32 reset_frame_context;

  u32 ref_frame_sign_bias[MAX_REF_FRAMES_EX];
  i32 loop_filter_level;
  u32 loop_filter_sharpness;
#ifdef SUPPORT_VCMD_M2M
  struct CDFsInfo cdfs_info;
#endif

  /* Quantization parameters */
  i32 qp_yac, qp_ydc, qp_y2_ac, qp_y2_dc, qp_ch_ac, qp_ch_dc;
  u32 using_qmatrix;
  u32 qm_y, qm_u, qm_v;

  /* From here down, frame-to-frame persisting stuff */

  u32 lossless[MAX_MB_SEGMENTS];
  u32 transform_mode;
  u32 allow_high_precision_mv;
  u32 allow_comp_inter_inter;
  u32 mcomp_filter_type;
  u32 pred_filter_mode;
  u32 comp_pred_mode;
  u32 comp_fixed_ref;
  u32 comp_var_ref[2];
  u32 log2_tile_columns;
  u32 log2_tile_rows;
  u32 tile_sz_mag;
  u32 reduced_tx_set_used;

  struct Av1EntropyProbs entropy;
  struct AV1CDFs *cdfs;
  struct MvCDFs *cdfs_ndvc;
  addr_t cdfs_addr;
  addr_t cdfs_ndvc_addr;
  struct AV1CDFs *default_cdfs;
  addr_t default_cdfs_addr;
  struct MvCDFs *default_cdfs_ndvc;
  addr_t default_cdfs_ndvc_addr;
  struct AV1CDFs *cdfs_last;
  addr_t cdfs_last_addr;
  struct Av1EntropyProbs entropy_last[NUM_FRAME_CONTEXTS];
  struct Av1AdaptiveEntropyProbs prev_ctx;
  struct Av1EntropyCounts ctx_ctr;

  /* Segment and macroblock specific values */
  u32 segment_enabled;
  u32 segment_map_update;
  u32 segment_map_temporal_update;
  u32 segment_feature_mode; /* ABS data or delta data */
  u32 segment_feature_enable[MAX_MB_SEGMENTS][SEG_AV1_LVL_MAX];
  i32 segment_feature_data[MAX_MB_SEGMENTS][SEG_AV1_LVL_MAX];
  u32 mode_ref_lf_enabled;
  i32 mb_ref_lf_delta[MAX_REF_LF_DELTAS_EX];
  i32 prev_ref_lf_delta[NUM_REF_FRAMES][MAX_REF_LF_DELTAS_EX];
  i32 prev_mode_lf_delta[NUM_REF_FRAMES][MAX_MODE_LF_DELTAS];
  i32 mb_mode_lf_delta[MAX_MODE_LF_DELTAS];
  i32 ref_frame_map[NUM_REF_FRAMES];
  u32 existing_ref_map;
  u32 reset_frame_flags;

  u32 frame_tag_size;
  u32 tile_group_hdr_size;
  u32 n_tile_groups;

  /* Value to remember last frames prediction for hits into most
   * probable reference frame */
  u32 refbu_pred_hits;

  u32 probs_decoded;

  // screen content controls
  u32 allow_screen_content_tools;
  u32 allow_intrabc;

  struct WarpedMotionParams models[GM_GLOBAL_MODELS_PER_FRAME];
  u8 av1_allow_interintra;
  u8 av1_delta_q_present;
  u8 av1_delta_q_res_log;
  u8 av1_allow_masked_compound;

  u8 warp_type;

  u8 cdef_damping;
  u8 cdef_bits;
  u8 cdef_luma_primary_strength[16];
  u8 cdef_luma_secondary_strength[16];
  u8 cdef_chroma_primary_strength[16];
  u8 cdef_chroma_secondary_strength[16];

  u8 lr_type[3];       // 0: NONE, 1: WIENER, 2: SGR, 3: SWITCHABLE
  u8 lr_unit_size[3];  // 0: 32,   1: 64,     2: 128, 3: 256

  u8 apply_grain;
  u8 update_parameters;
  u8 film_grain_params_ref_idx;
  struct Av1FilmGrainParams fg_params;
  struct AV1FilmGrainMemory fgsmem;

  u32 num_bits_w;
  u32 num_bits_h;
  u32 max_width;
  u32 max_height;
  u32 frame_id_numbers_present_flag;
  u32 delta_frame_id_length;
  u32 frame_id_length;
  u32 sb_size;
  u32 enable_dual_filter;
  u32 enable_order_hint;
  u32 enable_jnt_comp;
  u32 force_screen_content_tools;
  u32 force_integer_mv;
  u32 monochrome;
  u32 color_primaries;
  u32 transfer_characteristics;
  u32 matrix_coefficients;
  u32 timing_info_present_flag;
  u32 num_units_in_tick;
  u32 time_scale;
  u32 equal_picture_interval;
  u32 num_ticks_per_picture;
  u32 separate_uv_delta_q;
  u32 chroma_sample_position;

  i32 display_frame_id;
  u32 frame_type;
  u32 enable_intra_edge_filter;
  u32 enable_filter_intra;
  u32 frm_force_integer_mv;
  i32 curr_frame_id;
  u32 frame_offset;
  u32 current_video_frame;
  u32 primary_ref_frame;
  i32 qp_cv_dc;
  i32 qp_cv_ac;
  i32 loop_filter_level_r;
  i32 loop_filter_level_u;
  i32 loop_filter_level_v;
  u32 delta_lf_present_flag;
  u32 av1_delta_lf_res_log;
  u32 av1_delta_lf_multi;
  u32 switchable_motion_mode;
  u32 use_ref_frame_mvs;

  u32 frame_refs_short_signaling;
  i32 order_hint_bits_minus1;

  u32 still_picture;
  u32 reduced_still_picture_hdr;
  u32 operating_points_cnt;
  u32 operating_point_idc[32];
  u32 level[32];
  u32 seq_tier[32];
  u32 enable_superres;
  u32 enable_cdef;
  u32 enable_restoration;
  u32 enable_ref_frame_mvs;
  u32 enable_interintra_compound;
  u32 enable_masked_compound;
  u32 enable_warped_motion;
  u32 initial_display_delay_present_flag;
  u32 initial_display_delay_present[32];
  u32 initial_display_delay[32];
  u32 decoder_model_info_present_flag;
  u32 decoder_model_present[32];
  u32 decoder_buffer_delay[32];
  u32 encoder_buffer_delay[32];
  u32 low_delay_mode_flag[32];
  u32 film_grain_params_present;
  u32 showable_frame;
  u32 disable_cdf_update;
  u32 bitrate_scale;
  u32 buffer_size_scale;
  u32 buffer_delay_length;
  u32 num_units_in_decoding_tick;
  u32 buffer_removal_time_length;
  u32 buffer_removal_time[32];
  u32 frame_presentation_time_length;
  u32 frame_presentation_time;
  u32 tile_width;
  u32 tile_height;
  u32 uniform_tile_spacing;
  u32 allow_warped_motion;
  u32 context_update_tile_id;
  u32 tile_transpose;
  u32 oppoints;
  u32 buffer_removal_delay_present_flag;
  u32 allow_ref_frame_mvs;
  u32 frame_tag_decoded;
  u32 frame_size;
  u8 skip_mode_flag;
  u8 skip_ref0;
  u8 skip_ref1;
  u8 preskip_segid;
  u8 last_active_seg;
  u8 av1_tile_cols;
  u8 av1_tile_rows;
  u8 tile_col_start_sb[AV1_MAX_TILE_COLS + 1];
  u8 tile_row_start_sb[AV1_MAX_TILE_ROWS + 1];
  u32 tile_col_width_sb[AV1_MAX_TILE_COLS];
  u32 tile_row_height_sb[AV1_MAX_TILE_ROWS];
  u32 tile_offset_start[AV1_MAX_TILES];
  u32 tile_offset_end[AV1_MAX_TILES];
  i16 tile_start_concealment;
  i16 tile_start;
  i16 tile_end;
  obuHeader_t obu_hdr;
  u32 current_operating_point;
  bool not_valid_tile_dimension;
  u32 input_sequence_num;
  u32 input_same_seqheadr;
  obuSequenceHeaderInput_t *seq_hdr[16];
  obuSequenceHeaderInput_t seq_frame_hdr;
  obuSequenceHeader_t obu_seq_hdr_checked;
};

struct DecAsicBuffers;

void Av1ResetDecoder(struct Av1Decoder *dec);

#endif /* __AV1HWD_BOOL_H__ */
