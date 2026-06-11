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

#ifndef __COMMON_SINK_H__
#define __COMMON_SINK_H__

#include <stdio.h>
#include "basetype.h"
#include "decapi.h"
#include "dectypes.h"
#include "demuxer_types.h"
#include "command_line_parser.h"
#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

/* Generic yuv writing interface. */
typedef const void* YuvsinkOpenFunc(const char** fname);
typedef void YuvsinkWritePictureFunc(const void* inst, struct DecPicture *pic, int index);
typedef void YuvsinkCloseFunc(const void* inst);

typedef struct YuvSink_ {
  const void* inst;
  YuvsinkOpenFunc* Open;
  YuvsinkWritePictureFunc* WritePicture;
  YuvsinkCloseFunc* Close;
} YuvSink;

struct OutFileInfo {
  u32 bitstream_format;
  u32 pic_width;
  u32 pic_height;
  u32 bit_depth;
  bool is_interlaced;
  bool is_thumbnail; /* jpeg only */
  u32 crop_flag;
};

enum OutMode {
  OUT_YUV = 0,
  OUT_MD5 = 1,
};

/* for sink instance */
void* CreateYuvSink(struct TestParams *test_params);
void ReleaseYuvSink(YuvSink* yuvsink);
/* for output file name */
void GenerateOutputFileName(struct TestParams *test_params, struct OutFileInfo *info);
void PrintOutputFileName(struct TestParams *test_params);
void FreeOutputFileName(struct TestParams *test_params);
/* for write data */
void CommonWriteOnePic(FILE *file[2], u32 md5, struct DecPicture *pic, void *ct);

#endif /*__COMMON_SINK_H__*/