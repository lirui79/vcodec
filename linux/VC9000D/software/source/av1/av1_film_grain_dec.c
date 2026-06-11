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
-- Description : Read film grain params from frame header                     --
------------------------------------------------------------------------------*/

#include "av1_film_grain_dec.h"

#include "av1hwd_bool.h"
#include "av1hwd_container.h"
#include "av1hwd_decoder.h"

static u32 ResetGrainParams(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  dec->apply_grain = 0;
  dec->update_parameters = 0;
  for (int i = 0; i < 14; ++i) {
    for (int j = 0; j < 2; ++j) {
      dec->fg_params.scaling_points_y[i][j] = 0;
    }
  }
  dec->fg_params.num_y_points = 0;
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 2; ++j) {
      dec->fg_params.scaling_points_cb[i][j] = 0;
    }
  }
  dec->fg_params.num_cb_points = 0;
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 2; ++j) {
      dec->fg_params.scaling_points_cr[i][j] = 0;
    }
  }
  dec->fg_params.num_cr_points = 0;
  dec->fg_params.scaling_shift = 0;
  dec->fg_params.ar_coeff_lag = 0;
  for (int i = 0; i < 24; ++i) {
    dec->fg_params.ar_coeffs_y[i] = 0;
  }
  for (int i = 0; i < 25; ++i) {
    dec->fg_params.ar_coeffs_cb[i] = 0;
  }
  for (int i = 0; i < 25; ++i) {
    dec->fg_params.ar_coeffs_cr[i] = 0;
  }
  dec->fg_params.ar_coeff_shift = 0;
  dec->fg_params.cb_mult = 0;
  dec->fg_params.cb_luma_mult = 0;
  dec->fg_params.cb_offset = 0;
  dec->fg_params.cr_mult = 0;
  dec->fg_params.cr_luma_mult = 0;
  dec->fg_params.cr_offset = 0;
  dec->fg_params.overlap_flag = 0;
  dec->fg_params.clip_to_restricted_range = 0;
  dec->fg_params.chroma_scaling_from_luma = 0;
  dec->fg_params.grain_scale_shift = 0;
  dec->fg_params.random_seed = 0;
  return HANTRO_OK;
}

#ifdef ASIC_TRACE_SUPPORT
void WriteFilmGrainParamsToFile(struct Av1DecContainer *dec_cont, struct Av1FilmGrainParams *fg_params) {

  if (!dec_cont->film_grain_params_fid) return;
  struct Av1FilmGrainParams fg_params_output = *fg_params;

  fwrite(&fg_params_output, 1, sizeof(fg_params_output), dec_cont->film_grain_params_fid);

}
#endif

// Film Grain Synthesis
u32 DecodeFilmGrainParams(struct Av1DecContainer *dec_cont,
                          struct StrmData *rb) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  if (!dec->film_grain_params_present ||
      (!dec->show_frame && !dec->showable_frame)) {
    ResetGrainParams(dec_cont);
    return HANTRO_OK;
  }
  dec->apply_grain = SwGetBits(rb, 1);
  if (!dec->apply_grain) {
    ResetGrainParams(dec_cont);
    return HANTRO_OK;
  }
  u16 random_seed = SwGetBits(rb, 16);
  dec->fg_params.random_seed = random_seed;
  if (dec->frame_type == 1) {
    dec->update_parameters = SwGetBits(rb, 1);
  } else {
    dec->update_parameters = 1;
  }
  if (!dec->update_parameters) {
    int film_grain_params_ref_idx = SwGetBits(rb, 3);
    dec->film_grain_params_ref_idx = film_grain_params_ref_idx;
    dec->fg_params.random_seed = random_seed;
    return HANTRO_OK;
  }

  dec->fg_params.num_y_points = SwGetBits(rb, 4);
  // for (int i = 0; i < 16; ++i) {
  for (int i = 0; i < 14; ++i) {
    if (i == dec->fg_params.num_y_points) break;
    dec->fg_params.scaling_points_y[i][0] = SwGetBits(rb, 8);
    dec->fg_params.scaling_points_y[i][1] = SwGetBits(rb, 8);
  }
  if (dec->monochrome) {
    dec->fg_params.chroma_scaling_from_luma = 0;
  } else {
    dec->fg_params.chroma_scaling_from_luma = SwGetBits(rb, 1);
  }
  if (dec->monochrome || dec->fg_params.chroma_scaling_from_luma ||
      (dec->subsampling_x && dec->subsampling_y &&
       !dec->fg_params.num_y_points)) {
    dec->fg_params.num_cb_points = 0;
    dec->fg_params.num_cr_points = 0;
  } else {
    dec->fg_params.num_cb_points = SwGetBits(rb, 4);
    for (int i = 0; i < 10; ++i) {
      if (i == dec->fg_params.num_cb_points) break;
      dec->fg_params.scaling_points_cb[i][0] = SwGetBits(rb, 8);
      dec->fg_params.scaling_points_cb[i][1] = SwGetBits(rb, 8);
    }
    dec->fg_params.num_cr_points = SwGetBits(rb, 4);
    for (int i = 0; i < 10; ++i) {
      if (i == dec->fg_params.num_cr_points) break;
      dec->fg_params.scaling_points_cr[i][0] = SwGetBits(rb, 8);
      dec->fg_params.scaling_points_cr[i][1] = SwGetBits(rb, 8);
    }
  }
  dec->fg_params.scaling_shift = SwGetBits(rb, 2);
  dec->fg_params.ar_coeff_lag = SwGetBits(rb, 2);
  int num_pos_luma =
      2 * dec->fg_params.ar_coeff_lag * (dec->fg_params.ar_coeff_lag + 1);
  int num_pos_chroma = num_pos_luma;
  if (dec->fg_params.num_y_points > 0) {
    ++num_pos_chroma;
  }
  if (dec->fg_params.num_y_points) {
    for (int i = 0; i < 24; i++) {
      if (i == num_pos_luma) break;
      dec->fg_params.ar_coeffs_y[i] = SwGetBits(rb, 8);
    }
  }
  if (dec->fg_params.num_cb_points || dec->fg_params.chroma_scaling_from_luma) {
    for (int i = 0; i < 25; i++) {
      if (i == num_pos_chroma) break;
      dec->fg_params.ar_coeffs_cb[i] = SwGetBits(rb, 8);
    }
  }
  if (dec->fg_params.num_cr_points || dec->fg_params.chroma_scaling_from_luma) {
    for (int i = 0; i < 25; i++) {
      if (i == num_pos_chroma) break;
      dec->fg_params.ar_coeffs_cr[i] = SwGetBits(rb, 8);
    }
  }
  dec->fg_params.ar_coeff_shift = SwGetBits(rb, 2);
  dec->fg_params.grain_scale_shift = SwGetBits(rb, 2);
  if (dec->fg_params.num_cb_points) {
    dec->fg_params.cb_mult = SwGetBits(rb, 8);
    dec->fg_params.cb_luma_mult = SwGetBits(rb, 8);
    dec->fg_params.cb_offset = SwGetBits(rb, 9);
  }
  if (dec->fg_params.num_cr_points) {
    dec->fg_params.cr_mult = SwGetBits(rb, 8);
    dec->fg_params.cr_luma_mult = SwGetBits(rb, 8);
    dec->fg_params.cr_offset = SwGetBits(rb, 9);
  }
  dec->fg_params.overlap_flag = SwGetBits(rb, 1);
  dec->fg_params.clip_to_restricted_range = SwGetBits(rb, 1);

#ifdef ASIC_TRACE_SUPPORT
  WriteFilmGrainParamsToFile(dec_cont, &dec->fg_params);
#endif

  return HANTRO_OK;
}
