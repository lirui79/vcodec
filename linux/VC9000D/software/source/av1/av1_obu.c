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

#include "av1_obu.h"
#include "av1decapi.h"
#include "basetype.h"
#include "decapicommon.h"
#include "dwl.h"
#include "sw_stream.h"
#include "sw_util.h"
#include "version.h"

int leb128(struct StrmData* rb, int* len) {
  int s = 0;
  for (int i = 0; i < 8; i++) {
    u8 b = SwGetBits(rb, 8);
    s |= ((u64)(b & 0x7f)) << (i * 7);
    if (!(b & 0x80)) {
      *len = i + 1;
      break;
    }
  }
  return s;
}

int ReadObuHeader(struct StrmData* rb, obuHeader_t* hdr, bool annexb,
                  int annexb_size, bool size_check, u32 heif_mode) {
  // const u8* local = data_start;
  // if (local == data_end) return HANTRO_NOK;
  u32 read_bits = rb->strm_buff_read_bits;
  u32 b = SwGetBits(rb, 8);
  if (b == END_OF_STREAM) return HANTRO_NOK;

  u8 b0 = (u8)b;
  if (b0 & 1) {
    // Forbidden bit. Must not be set.
    return HANTRO_NOK;
  }
  hdr->type = (b0 >> 3) & 0xf;
  hdr->has_extension = (b0 >> 2) & 1;
  hdr->has_size_field = (b0 >> 1) & 1;

  if (!hdr->has_size_field && !annexb) {
    // section 5 obu streams must have obu_size field set.
    return HANTRO_NOK;
  }

  if (b0 >> 7) {
    // obu_reserved_1bit must be set to 0.
    return HANTRO_NOK;
  }

  if (hdr->has_extension) {
    // if (local == data_end) return HANTRO_NOK;
    b = SwGetBits(rb, 8);
    if (b == END_OF_STREAM) return HANTRO_NOK;

    u8 b1 = (u8)b;
    if (b1 & 0x7) {
      // extension_header_reserved_3bits must be set to 0.
      return HANTRO_NOK;
    }
    hdr->temporal_layer_id = (b1 >> 5) & 0x7;
    hdr->spatial_layer_id = (b1 >> 3) & 0x3;
    if (heif_mode && (hdr->temporal_layer_id || hdr->spatial_layer_id))
      return HANTRO_NOK;
  }

  int size = 0;
  if (hdr->has_size_field) {
    int l = 0;
    size = leb128(rb, &l);
    u32 bits_left = (i32)rb->strm_data_size * 8 - (i32)rb->strm_buff_read_bits;
    if (rb->stream_info->low_latency)
      size_check = 0;
    if (size < 0 || (size_check && size * 8 > bits_left)) return HANTRO_NOK;
#if 0
    while (l > 0) {
      SwFlushBits(rb, 8);
      l--;
    }
#endif
    // local += l;
  } else {
    if (!annexb) return HANTRO_NOK;
    size = annexb_size - (hdr->has_extension ? 2 : 1);
  }

  hdr->header_size = (rb->strm_buff_read_bits - read_bits) >> 3;
  hdr->total_size = size + hdr->header_size;
  hdr->payload_size = size;

  return HANTRO_OK;
}
