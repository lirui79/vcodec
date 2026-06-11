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

#ifndef __AV1HWD_BOOL_H__
#define __AV1HWD_BOOL_H__

#include "basetype.h"

// #define STREAM_TRACE_EN

#ifdef STREAM_TRACE_EN
#include <stdio.h>
#define STREAM_TRACE(x, y) printf("STREAM_TRACE: %-30s-%9d\n", x, y);
#define AV1DEC_DEBUG(x) printf x
#else
#define STREAM_TRACE(x, y)
#define AV1DEC_DEBUG(x)
#endif

#define CHECK_END_OF_STREAM(s) \
  if ((s) == END_OF_STREAM) return (s)

struct VpBoolCoder {
  u32 lowvalue;
  u32 range;
  u32 value;
  i32 count;
  u32 pos;
  const u8 *buffer;
  u32 BitCounter;
  u32 stream_end_pos;
  u32 strm_error;
};

extern void Av1BoolStart(struct VpBoolCoder *bc, const u8 *buffer, u32 len);
extern u32 Av1DecodeBool(struct VpBoolCoder *bc, i32 probability);
extern u32 Av1DecodeBool128(struct VpBoolCoder *bc);
extern void Av1BoolStop(struct VpBoolCoder *bc);

u32 Av1DecodeSubExp(struct VpBoolCoder *bc, u32 k, u32 num_syms);
u32 Av1ReadBits(struct VpBoolCoder *br, i32 bits);

#endif /* __AV1HWD_BOOL_H__ */
