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

#ifndef VP9DECAPI_H
#define VP9DECAPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "commonfunction.h"

/*------------------------------------------------------------------------------
    API type definitions
------------------------------------------------------------------------------*/

/* Decoder instance */
typedef const void *Vp9DecInst;

/* Input structure */
struct Vp9DecInput {
  u8 *stream;             /* Pointer to the input */
  addr_t stream_bus_address; /* DMA bus address of the input stream buffer */
  u32 data_len;           /* Number of bytes to be decoded         */
  u32 frame_len;          /* temp data, real data len for current frame */
  u8 *buffer;
  addr_t buffer_bus_address; /* DMA bus address of the input stream buffer */
  u32 buff_len;           /* Stream buffer byte length         */
  u32 pic_id;             /* Identifier for the picture to be decoded */
  void *p_user_data;      /* Pointer to user data to be passed in multicore callback */
  u32 dec_ctrl;
};

/* stream info filled by Vp9DecGetInfo */
struct Vp9DecInfo {
  u32 vp_version;
  u32 vp_profile;
  enum DecPictureFormat output_format;
  u32 bit_depth;
  u32 coded_width;   /* coded width */
  u32 coded_height;  /* coded height */
  u32 frame_width;   /* pixels width of the frame as stored in memory */
  u32 frame_height;  /* pixel height of the frame as stored in memory */
  u32 scaled_width;  /* scaled width of the displayed video */
  u32 scaled_height; /* scaled height of the displayed video */
  u32 dpb_mode;      /* DPB mode; frame, or field interlaced */
  u32 pic_buff_size; /* number of picture buffers allocated&used by decoder */
  u32 multi_buff_pp_size; /* number of picture buffers needed in
                             decoder+postprocessor multibuffer mode */
  u32 pic_stride;    /* Byte width of the pixel as stored in memory */
};

/* Output structure for Vp9DecNextPicture */
struct Vp9DecPicture {
  u32 coded_width;  /* coded width of the picture */
  u32 coded_height; /* coded height of the picture */
  u32 frame_width;  /**< pixels width of the frame as stored in memory */
  u32 frame_height; /**< pixel height of the frame as stored in memory */
  u32 pic_id;                    /* Identifier of the Frame to be displayed */
  u32 decode_id;
  u32 display_id;
  u32 is_intra_frame;            /* Indicates if Frame is an Intra Frame */
  u32 is_golden_frame; /* Indicates if Frame is a Golden reference Frame */
  u32 error_ratio;     /**< The ratio of error SBs to whole picture SBs.(100 times) */
  enum DecErrorInfo error_info; /**< Indicates that picture is error */
  u32 num_slice_rows;
  u32 cycles_per_mb;   /* Avarage cycle count per macroblock */
#ifdef FPGA_PERF_AND_BW
  u32 bitrate_4k60fps;  /**< The bitrate of frame*/
  u32 bwrd_in_fs;    /**< Read bandwidth of frame*/
  u32 bwwr_in_fs;    /**< Write bandwidth of frame*/
#endif
  u32 bit_depth_luma;
  u32 bit_depth_chroma;
  u32 num_tile_columns;
  const u32 *output_rfc_luma_base;   /* Pointer to the rfc table */
  addr_t output_rfc_luma_bus_address;
  const u32 *output_rfc_chroma_base; /* Pointer to the rfc chroma table */
  addr_t output_rfc_chroma_bus_address;

  u32 use_video_compressor;
  u32 ref_pic_stride;
  u32 ref_pic_ch_stride;
  struct Vp9OutputInfo {
    u32 frame_width;  /* pixels width of the frame as stored in memory */
    u32 frame_height; /* pixel height of the frame as stored in memory */
    const u32 *output_luma_base;   /* Pointer to the picture */
    addr_t output_luma_bus_address;   /* Bus address of the luminance component */
    const u32 *output_chroma_base; /* Pointer to the picture */
    addr_t output_chroma_bus_address;   /* Bus address of the luminance component */
    enum DecPictureFormat output_format;
    u32 pic_stride;       /* Byte width of the picture as stored in memory */
    u32 pic_stride_ch;
    u32 out_bit_depth;
    struct DWLLinearMem dec400_luma_table;
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
};

struct Vp9DecConfig {
  // u32 use_video_freeze_concealment;
  enum DecErrorHandling error_handling;
  u32 error_ratio;
  u32 num_frame_buffers;
  u32 use_video_compressor;
  DecPicAlignment align;
  enum DecDecoderMode decoder_mode;
  u32 use_adaptive_buffers;
  u32 guard_size;
  PpUnitConfig ppu_cfg[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  DelogoConfig delogo_params[2];
  struct DecMCConfig mcinit_cfg;
  /** Number of clock cycles to wait between multi-core sync polls. */
  u32 multicore_poll_period;
  /** The tile decoding order, 0: original stream order (raster order), 1: transposed order
   * (The parameter is obsolete) */
  u32 tile_transpose;
  u32 misc_ctrl;
};

/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

enum DecRet Vp9DecInit(Vp9DecInst *dec_inst, const void *dwl, struct Vp9DecConfig *dec_cfg);

void Vp9DecRelease(Vp9DecInst dec_inst);

enum DecRet Vp9DecSetInfo(Vp9DecInst dec_inst, struct Vp9DecConfig *dec_cfg);

enum DecRet Vp9DecDecode(Vp9DecInst dec_inst, const struct Vp9DecInput *input,
                         struct DecOutput *output);

enum DecRet Vp9DecNextPicture(Vp9DecInst dec_inst,
                              struct Vp9DecPicture *output);

enum DecRet Vp9DecPictureConsumed(Vp9DecInst dec_inst,
                                  const struct Vp9DecPicture *picture);

enum DecRet Vp9DecEndOfStream(Vp9DecInst dec_inst);

enum DecRet Vp9DecGetInfo(Vp9DecInst dec_inst, struct Vp9DecInfo *dec_info);

enum DecRet Vp9DecAddBuffer(Vp9DecInst dec_inst, struct DWLLinearMem *info);

enum DecRet Vp9DecGetBufferInfo(Vp9DecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet Vp9DecAbort(Vp9DecInst dec_inst);

enum DecRet Vp9DecAbortAfter(Vp9DecInst dec_inst);

void Vp9DecUpdateStrmInfoCtrl(Vp9DecInst dec_inst, struct strmInfo info);

void Vp9DecUpdateTileInfo(Vp9DecInst dec_inst);

enum DecRet Vp9DecUseExtraFrmBuffers(Vp9DecInst dec_inst, u32 n);

#ifdef __cplusplus
}
#endif

#endif /* VP9DECAPI_H */
