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

#ifndef VC1DECAPI_H
#define VC1DECAPI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "dwl.h"

/*------------------------------------------------------------------------------
    API type definitions
------------------------------------------------------------------------------*/
/*
*   Decoder instance is used for identifying subsequent calls to api
*   functions.
*/
typedef void *VC1DecInst;

/*
*  Decoder input structure.
*  This is a container to pass data to the Decoder.
*/
typedef struct {

  const u8* stream;     /* Pointer to the video stream. Decoder does
                                  not change the contents of stream buffer.*/
  addr_t stream_bus_address;  /* DMA bus address of the input stream */
  u32 stream_size;        /* Number of bytes in the stream buffer.*/
  u32 pic_id;             /**< User-defined identifier to bind into
                                 *  decoded picture. */
  enum DecSkipFrameMode skip_frame; /* Skip some frames when decoding. */
  u32 dec_ctrl;
} VC1DecInput;

/* decoder  output picture format */
typedef enum {
  VC1DEC_SEMIPLANAR_YUV420 = 0x020001,
  VC1DEC_TILED_YUV420 = 0x020002
} VC1DecOutFormat;

typedef struct {
  VC1DecOutFormat output_format; /* format of the output picture */
  u32 max_coded_width;
  u32 max_coded_height;
  u32 coded_width;
  u32 coded_height;
  u32 par_width;
  u32 par_height;
  u32 frame_rate_numerator;
  u32 frame_rate_denominator;
  u32 interlaced_sequence;
  enum DecDpbMode dpb_mode;         /* DPB mode; frame, or field interlaced */
  u32 buf_release_flag;
  u32 multi_buff_pp_size;
} VC1DecInfo;

/*
*   Decoder output structure.
*   This is a container for Decoder output data like decoded picture and its
*   dimensions.
*/
typedef struct {
  u32 key_picture;
  u32 pic_id;
  u32 decode_id[2];
  u32 pic_coding_type[2];
  u32 range_red_frm;

  u32 range_map_yflag;
  u32 range_map_y;
  u32 range_map_uv_flag;
  u32 range_map_uv;

  u32 interlaced;
  u32 field_picture;
  u32 top_field;

  u32 first_field;
  u32 repeat_first_field;
  u32 repeat_frame_count;

  u32 number_of_err_mbs;
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
  u32 error_ratio;   /**< Current picture error ratio */
  u32 error_info;    /**< Current picture error info */
  u32 anchor_picture;
  struct VC1OutputInfo {
    u32 frame_width;
    u32 frame_height;
    u32 coded_width;
    u32 coded_height;
    const u8 *output_picture;
    addr_t output_picture_bus_address;
    const u8 *output_picture_chroma;
    addr_t output_picture_chroma_bus_address;
    u32 pic_stride;
    u32 pic_stride_ch;
    enum DecPictureFormat output_format;
    struct DecCropParams crop_params; /**< Cropping parameters for the picture */
    struct DWLLinearMem dec400_luma_table;           /**< Buffer properties *//*sunny add for tile status address*/
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
} VC1DecPicture;

struct VC1DecConfig {
  enum DecErrorHandling error_handling;
  u32 error_ratio;
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  DecPicAlignment align;
  u32 num_frame_buffers;
  struct DecMetaData *p_meta_data;
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  enum DecDecoderMode decoder_mode;
  u32 misc_ctrl;
};
/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

enum DecRet VC1DecInit(VC1DecInst* dec_inst, const void *dwl, struct VC1DecConfig *dec_cfg);

enum DecRet VC1DecDecode( VC1DecInst dec_inst,
                        const VC1DecInput* input,
                        struct DecOutput* output);

void VC1DecRelease(VC1DecInst dec_inst);

enum DecRet VC1DecGetInfo(VC1DecInst dec_inst, VC1DecInfo * dec_info);

enum DecRet VC1DecUnpackMetaData( const u8 *p_buffer, u32 buffer_size,
                                struct DecMetaData *p_meta_data );

enum DecRet VC1DecNextPicture(VC1DecInst  dec_inst, VC1DecPicture *picture);

enum DecRet VC1DecPictureConsumed(VC1DecInst  dec_inst,
                                VC1DecPicture *picture);

enum DecRet VC1DecEndOfStream(VC1DecInst dec_inst);

enum DecRet VC1DecAbort(VC1DecInst dec_inst);

enum DecRet VC1DecAbortAfter(VC1DecInst dec_inst);

enum DecRet VC1DecPeek(VC1DecInst  dec_inst, VC1DecPicture *picture);

enum DecRet VC1DecGetBufferInfo(VC1DecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet VC1DecAddBuffer(VC1DecInst dec_inst, struct DWLLinearMem *info);

enum DecRet VC1DecSetInfo(VC1DecInst dec_inst, struct VC1DecConfig *dec_cfg);

/*------------------------------------------------------------------------------
    Prototype of the API trace funtion. Traces all API entries and returns.
    This must be implemented by the application using the decoder API!
    Argument:
        string - trace message, a null terminated string
------------------------------------------------------------------------------*/
void VC1DecTrace(const char *string);

#ifdef __cplusplus
}
#endif

#endif /* VC1DECAPI_H */

