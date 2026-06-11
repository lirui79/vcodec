/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#include "basetype.h"
#include "dwl.h"

struct LLStrmInfo {
  u32 strm_status_in_buffer;/* flag for using ddr_low_latency model */
  u32 *strm_status_addr;/* buffer used to set info for using ddr_low_latency model */
  u32 updated_reg;
  addr_t ll_strm_bus_address; /* strm bus address in low latency mode */
  u32 ll_strm_len; /* strm length in low latency mode */
  u32 update_reg_flag; /* the flag indicate if length register need to be updated or not */
  u32 tmp_length; /* used to update hwLength in low latency mode */
  u32 first_update; /* the flag indicate if length register is updated first time or not */
  struct DWLLinearMem strm_status; /* buffer for HW to sync stream status in low latency mode */
  struct strmInfo stream_info; /* struct to sync stream status for different thread in sw */
#ifdef SUPPORT_DMA
  const void *dwl_inst; /* dwl inst for DMA to transfer data */
#endif
};

struct LLTileInfo {
  u32 *tile_status_addr; /* buffer used to set tile info for low_latency model */
  u8 update_done; /* tile info update done for current frame */
  u8 update_rdy; /* all resource rdy for update tile info for current frame */
  u32 tile_size; /* Cumulative tile size for current frame */
  u32 cur_rows; /* current row for current tile_id */
  u32 cur_cols; /* current col for current tile_id */
  u32 tile_rows; /* total tile rows for current frame */
  u32 tile_cols; /* total tile cols for current frame */
  u32 tile_id; /* current tile id */
  u8 *strm_next_address; /* tile hrd address in strm */
  u8 *tileinfo; /* virtural address tile info buffer */
  u8 *buf_address; /* input buffer virtual address */
  u32 buf_len; /* input buffer length */
  u32 frame_len; /* current frame length */
  /* vp9 only */
  u32 h_sbs; /* height for current frame in sb */
  u32 w_sbs; /* weight for current frame in sb */
  u32 prev_w; /* previous weight */
  u32 prev_h; /* previous height */
  u32 tmp_h; /* currnet height */
  /* av1 only */
  bool last_tile_group; /* last tile group for current frame */
  u8 obu_hrd; /* next data is obu hrd */
  u8 tile_group_hrd; /* next data is tile group hrd */
  u32 payload_len_lost; /* payload len lost for current obu */
  u32 payload_len; /* payload len for current obu */
  u8 first_tile; /* first tile group */
  u32 read_bits; /* read_bits in first tile group */
};

void SwUpdateStrmInfoCtrl(struct LLStrmInfo *llstrminfo, u32 *len_update);
void SwUpdateTileInfoCtrl(struct LLTileInfo *lltileinfo);
