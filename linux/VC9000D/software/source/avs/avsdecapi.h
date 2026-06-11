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

#ifndef __AVSDECAPI_H__
#define __AVSDECAPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "dwl.h"
#include "dectypes.h"
#include "commonfunction.h"

/*------------------------------------------------------------------------------
    API type definitions
------------------------------------------------------------------------------*/

/* decoder output picture format */
typedef enum {
  AVSDEC_SEMIPLANAR_YUV420 = 0x020001,
  AVSDEC_TILED_YUV420 = 0x020002
} AvsDecOutFormat;

/* Decoder instance */
typedef void *AvsDecInst;

/* Input structure */
typedef struct {
  u8 *stream;         /* Pointer to stream to be decoded              */
  addr_t stream_bus_address;   /* DMA bus address of the input stream */
  u32 data_len;         /* Number of bytes to be decoded                */
  u32 pic_id;
  enum DecSkipFrameMode skip_frame; /* Skip some frames when decoding. */
  u32 dec_ctrl;
} AvsDecInput;

/* stream info filled by AvsDecGetInfo */
typedef struct {
  u32 frame_width;
  u32 frame_height;
  u32 coded_width;
  u32 coded_height;
  u32 profile_id;
  u32 level_id;
  u32 display_aspect_ratio;
  u32 video_format;
  u32 video_range;
  u32 interlaced_sequence;
  enum DecDpbMode dpb_mode;         /* DPB mode; frame, or field interlaced */
  u32 pic_buff_size;
  u32 multi_buff_pp_size;
  AvsDecOutFormat output_format;
} AvsDecInfo;

typedef struct {
  u32 key_picture;
  u32 pic_id;
  u32 decode_id;
  u32 pic_coding_type;
  u32 interlaced;
  u32 field_picture;
  u32 top_field;
  u32 first_field;
  u32 repeat_first_field;
  u32 repeat_frame_count;
  u32 number_of_err_mbs;
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
  struct DecTime time_code;
  struct AvsOutputInfo {
    u8 *output_picture;
    addr_t output_picture_bus_address;
    u8 *output_picture_chroma;
    addr_t output_picture_chroma_bus_address;
    u32 frame_width;
    u32 frame_height;
    u32 coded_width;
    u32 coded_height;
    u32 pic_stride;
    u32 pic_stride_ch;
    enum DecPictureFormat output_format;
    struct DecCropParams crop_params; /**< Cropping parameters for the picture */
    struct DWLLinearMem dec400_luma_table;           /**< Buffer properties *//*sunny add for tile status address*/
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
} AvsDecPicture;

struct AvsDecConfig {
  enum DecErrorHandling error_handling;
  u32 error_ratio;
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  DecPicAlignment align;
  u32 num_frame_buffers;
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  enum DecDecoderMode decoder_mode;
  u32 misc_ctrl;
};
/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

enum DecRet AvsDecInit(AvsDecInst * dec_inst, const void *dwl, struct AvsDecConfig *dec_cfg);

enum DecRet AvsDecDecode(AvsDecInst dec_inst,
                       AvsDecInput * input,
                       struct DecOutput * output);

enum DecRet AvsDecGetInfo(AvsDecInst dec_inst, AvsDecInfo * dec_info);

enum DecRet AvsDecNextPicture(AvsDecInst dec_inst, AvsDecPicture * picture);

enum DecRet AvsDecPictureConsumed(AvsDecInst dec_inst,
                                AvsDecPicture * picture);

enum DecRet AvsDecEndOfStream(AvsDecInst dec_inst);

void AvsDecRelease(AvsDecInst dec_inst);

enum DecRet AvsDecPeek(AvsDecInst dec_inst, AvsDecPicture * picture);
enum DecRet AvsDecGetBufferInfo(AvsDecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet AvsDecAddBuffer(AvsDecInst dec_inst, struct DWLLinearMem *info);
enum DecRet AvsDecAbort(AvsDecInst dec_inst);

enum DecRet AvsDecAbortAfter(AvsDecInst dec_inst);

enum DecRet AvsDecSetInfo(AvsDecInst dec_inst, struct AvsDecConfig *dec_cfg);

/*------------------------------------------------------------------------------
    Prototype of the API trace funtion. Traces all API entries and returns.
    This must be implemented by the application using the decoder API!
    Argument:
        string - trace message, a null terminated string
------------------------------------------------------------------------------*/
void AvsDecTrace(const char *string);

#ifdef __cplusplus
}
#endif

#endif                       /* __AVSDECAPI_H__ */
