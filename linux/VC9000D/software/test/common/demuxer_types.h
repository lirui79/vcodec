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

#ifndef __DEMUXER_TYPES_H__
#define __DEMUXER_TYPES_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "basetype.h"

/* Enumeration for the bitstream format outputted from the reader */
#define BITSTREAM_PP_INPUT 0x00
#define BITSTREAM_VP7 0x01
#define BITSTREAM_VP8 0x02
#define BITSTREAM_VP9 0x03
#define BITSTREAM_WEBP 0x04
#define BITSTREAM_H264 0x05
#define BITSTREAM_HEVC 0x06
#define BITSTREAM_AVS2 0x07
#define BITSTREAM_AV1 0x08
#define BITSTREAM_AV1_ANNEXB 0x09
#define BITSTREAM_AV1_OBU 0x0A
#define BITSTREAM_MPEG4 0x0B
#define BITSTREAM_MPEG2 0x0C
#define BITSTREAM_SORENSON 0x0D
#define BITSTREAM_VVC 0x0E
#define BITSTREAM_JPEG 0x0F
#define BITSTREAM_AVS 0x10
#define BITSTREAM_AVS3 0x11
#define BITSTREAM_RM 0x12   /* RV9/10 */
#define BITSTREAM_VC1 0x13
#define BITSTREAM_VP6 0x14
#define BITSTREAM_RV8 0x15   /* RV8 */

/* Generic demuxer interface. */
typedef const void* DemuxerOpenFunc(const char* fname, char* index_path, u32 mode, u32 low_latency);
typedef int DemuxerIdentifyFormatFunc(const void* inst);
typedef void DemuxerHeadersDecoded(const void* inst);
typedef int DemuxerReadPacketFunc(const void* inst, u8* buffer, u8* stream[2], i32* size, u8 rb);
typedef void DemuxerCloseFunc(const void* inst);
typedef struct Demuxer_ {
  const void* inst;
  DemuxerOpenFunc* Open;
  DemuxerIdentifyFormatFunc* GetVideoFormat;
  DemuxerHeadersDecoded* HeadersDecoded;
  DemuxerReadPacketFunc* ReadPacket;
  DemuxerCloseFunc* Close;
} Demuxer;

#ifdef __cplusplus
}
#endif

#endif /* __DEMUXER_TYPES_H__ */
