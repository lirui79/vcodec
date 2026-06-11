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

/* This file declares common structures/functions etc used in decoder internals,
   other than APIs. */

#ifndef COMMON_FUNCTION_H
#define COMMON_FUNCTION_H

#include "basetype.h"
#include "sw_util.h"
#include "dwl.h"
#include "ppu.h"
#include "decapicommon.h"

/* obsoleted structure/enum removed from decapicommon.h, which are not used as
   part of API anymore. */

/**
 * DPB flags to control reference picture format etc.
 * \ingroup common_group
 */
enum DecDpbFlags {
  /** \brief Reference frame format is raster scan.[Not supported] */
  DEC_REF_FRM_RASTER_SCAN = 0x0,
  /** \brief Reference frame format is tiled (Default) */
  DEC_REF_FRM_TILED_DEFAULT = 0x1,
  /** \brief Flag to allow SW to use DPB field ordering on interlaced content.[Not supported] */
  DEC_DPB_ALLOW_FIELD_ORDERING = 0x40000000
};

/**
 * Error information.
 * \ingroup common_group
 */
enum DecErrorInfo {
  DEC_NO_ERROR = 0x0,
  /** HW EC only */
  DEC_NALU_HEADER_ERROR = 0x1,
  DEC_SLICE_HEADER_ERROR = 0x2,
  DEC_SLICE_DATA_ERROR = 0x4,
  DEC_SYNC_WORD_ERROR = 0x8,
  DEC_TRAILING_BITS_ERROR = 0x10,
  DEC_POLLING_STREAM_LEN_ERROR = 0x20,
  /** All use */
  DEC_FRAME_ERROR = 0x100,
  DEC_REF_ERROR = 0x200
};

enum SeqErrorState {
  SEQ_CLEAN = 0x0,
  SEQ_DIRTY = 1,
};

typedef struct OutputPicInfo_ {
  u32 id;
  u32 error_ratio;
  u32 cycles;
  u32 is_field_pic;
  u32 is_bottom_field;
  enum DecPicCodingType pic_code_type;
  enum DecErrorInfo error_info;
  u32 pic_id;
} OutputPicInfo;

/* function: generate fake table */
#ifdef USE_FAKE_RFC_TABLE
void GenerateFakeRFCTable(u8 *cmp_tble_addr,
                          u32 pic_width_in_cbsy,
                          u32 pic_height_in_cbsy,
                          u32 pic_width_in_cbsc,
                          u32 pic_height_in_cbsc,
                          u32 bit_depth,
                          u32 buffer_align);
#endif

/* dec mode */
#define DEC_MODE_H264      0
#define DEC_MODE_MPEG4     1
#define DEC_MODE_H263      2
#define DEC_MODE_JPEG      3
#define DEC_MODE_VC1       4
#define DEC_MODE_MPEG2     5
#define DEC_MODE_MPEG1     6
#define DEC_MODE_VP6       7
#define DEC_MODE_RV        8
#define DEC_MODE_VP7       9
#define DEC_MODE_VP8       10
#define DEC_MODE_AVS       11
#define DEC_MODE_HEVC      12
#define DEC_MODE_VP9       13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2      16
#define DEC_MODE_AV1       17
#define DEC_MODE_VVC       18

#ifdef FPGA_PERF_AND_BW
typedef struct DWLPerfInfo DecPerfInfo;

void DecPerfInfoCount(const void *instance, u32 core_id, DecPerfInfo* perf_info, u64 pic_size, u32 bit_depth);
void AveragePerfInfoPrint(DecPerfInfo *perf_info);
#endif

#ifdef OPEN_HWCFG_TO_CLIENT
void GetHwConfig(const void *hw_feature, struct DecHwConfig *hw_cfg);
#endif

#endif /* COMMON_FUNCTION_H */
