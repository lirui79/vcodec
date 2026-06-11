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

#ifndef _AV1_OBU_H_
#define _AV1_OBU_H_

#include "av1decapi.h"
#include "basetype.h"
#include "sw_stream.h"

enum ObuType {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_FRAME = 6,
  OBU_REDUNDANT_FRAME_HEADER = 7,
  OBU_PADDING = 15,
};

typedef struct {
  int type;
  int header_size;
  int total_size;
  int payload_size;
  int has_extension;
  int has_size_field;
  int temporal_layer_id;
  int spatial_layer_id;
} obuHeader_t;
// add the following for error concealment, carl.
typedef struct {
  u32 sequencer_ok;
  u32 vp_profile;

  u32 still_picture;
  u32 reduced_still_picture_hdr;

  u32 timing_info_present_flag;

  u32 num_units_in_tick;
  u32 time_scale;
  u32 equal_picture_interval;
  u32 num_ticks_per_picture;

  u32 decoder_model_info_present_flag;

  u32 buffer_delay_length;
  u32 num_units_in_decoding_tick;
  u32 buffer_removal_time_length;

  u32 frame_presentation_time_length;

  u32 initial_display_delay_present_flag;

  u32 operating_points_cnt;
  u32 operating_point_idc[32];
  u32 level[32];
  u32 seq_tier[32];

  u32 initial_display_delay_present[32];
  u32 initial_display_delay[32];

  u32 num_bits_w;
  u32 num_bits_h;
  u32 max_width;
  u32 max_height;

  u32 frame_id_numbers_present_flag;
  u32 delta_frame_id_length;

  u32 frame_id_length;
  u32 sb_size;
  u32 enable_filter_intra;
  u32 enable_intra_edge_filter;

  u32 enable_interintra_compound;
  u32 enable_masked_compound;
  u32 enable_warped_motion;

  u32 enable_dual_filter;
  u32 enable_order_hint;

  u32 enable_jnt_comp;
  u32 enable_ref_frame_mvs;
  u32 force_screen_content_tools;

  u32 force_integer_mv;
  i32 order_hint_bits_minus1;

  u32 enable_superres;
  u32 enable_cdef;
  u32 enable_restoration;
  u32 bit_depth;

  u32 monochrome;
  u32 color_primaries;

  u32 transfer_characteristics;
  u32 matrix_coefficients;

  enum DecVideoRange color_range;
  u32 subsampling_x;
  u32 subsampling_y;
  u32 chroma_sample_position;

  u32 separate_uv_delta_q;

  u32 film_grain_params_present;

} obuSequenceHeader_t;

typedef struct {
  u32 vp_profile;

  u32 still_picture;
  u32 reduced_still_picture_hdr;

  u32 timing_info_present_flag;

  u32 num_units_in_tick;
  u32 time_scale;
  u32 equal_picture_interval;
  u32 num_ticks_per_picture;

  u32 decoder_model_info_present_flag;

  u32 buffer_delay_length;
  u32 num_units_in_decoding_tick;
  u32 buffer_removal_time_length;

  u32 frame_presentation_time_length;

  u32 initial_display_delay_present_flag;

  u32 operating_points_cnt;
  u32 operating_point_idc[32];
  u32 level[32];
  u32 seq_tier[32];

  u32 initial_display_delay_present[32];
  u32 initial_display_delay[32];

  u32 num_bits_w;
  u32 num_bits_h;
  u32 max_width;
  u32 max_height;

  u32 frame_id_numbers_present_flag;
  u32 delta_frame_id_length;

  u32 frame_id_length;
  u32 sb_size;
  u32 enable_filter_intra;
  u32 enable_intra_edge_filter;

  u32 enable_interintra_compound;
  u32 enable_masked_compound;
  u32 enable_warped_motion;

  u32 enable_dual_filter;
  u32 enable_order_hint;

  u32 enable_jnt_comp;
  u32 enable_ref_frame_mvs;
  u32 force_screen_content_tools;

  u32 force_integer_mv;
  i32 order_hint_bits_minus1;

  u32 enable_superres;
  u32 enable_cdef;
  u32 enable_restoration;
  u32 bit_depth;

  u32 monochrome;
  u32 color_primaries;

  u32 transfer_characteristics;
  u32 matrix_coefficients;

  enum DecVideoRange color_range;
  u32 subsampling_x;
  u32 subsampling_y;
  u32 chroma_sample_position;

  u32 separate_uv_delta_q;

  u32 film_grain_params_present;

} obuSequenceHeaderInput_t;

int ReadObuHeader(struct StrmData *rb, obuHeader_t *hdr, bool annexb, int size,
                  bool size_check, u32 heif_mode);
int leb128(struct StrmData *rb, int *len);

#endif  // _AV1_OBU_H_
