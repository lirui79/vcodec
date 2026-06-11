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

#ifndef __FILM_GRAIN_DEC_H__
#define __FILM_GRAIN_DEC_H__

#include "sw_stream.h"

struct Av1DecContainer;

struct Av1FilmGrainParams {
  u8 scaling_points_y[14][2];
  u8 num_y_points;
  u8 scaling_points_cb[10][2];
  u8 num_cb_points;
  u8 scaling_points_cr[10][2];
  u8 num_cr_points;
  u8 scaling_shift;
  u8 ar_coeff_lag;
  u8 ar_coeffs_y[24];
  u8 ar_coeffs_cb[25];
  u8 ar_coeffs_cr[25];
  u8 ar_coeff_shift;
  u8 cb_mult;
  u8 cb_luma_mult;
  u16 cb_offset;
  u8 cr_mult;
  u8 cr_luma_mult;
  u16 cr_offset;
  u8 overlap_flag;
  u8 clip_to_restricted_range;
  u8 chroma_scaling_from_luma;
  u8 grain_scale_shift;
  u16 random_seed;
};

u32 DecodeFilmGrainParams(struct Av1DecContainer *dec_cont,
                          struct StrmData *rb);

#endif  // __FILM_GRAIN_DEC_H__
