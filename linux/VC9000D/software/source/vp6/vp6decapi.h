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

#ifndef __VP6DECAPI_H__
#define __VP6DECAPI_H__

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

/* decoder  output Frame format */
typedef enum VP6DecOutFormat_ {
  VP6DEC_SEMIPLANAR_YUV420 = 0x020001,
  VP6DEC_TILED_YUV420 = 0x020002
} VP6DecOutFormat;

/* Input structure */
typedef struct VP6DecInput_ {
  const u8 *stream;   /* Pointer to the input data */
  addr_t stream_bus_address;   /* DMA bus address of the input stream */
  u32 data_len;         /* Number of bytes to be decoded         */
  u32 pic_id;
  u32 dec_ctrl;
} VP6DecInput;

#define VP6_SCALE_MAINTAIN_ASPECT_RATIO     0
#define VP6_SCALE_TO_FIT                    1
#define VP6_SCALE_CENTER                    2
#define VP6_SCALE_OTHER                     3

/* stream info filled by VP6DecGetInfo */
typedef struct VP6DecInfo_ {
  u32 vp6_version;
  u32 vp6_profile;
  u32 pic_buff_size;
  u32 frame_width;      /* coded width */
  u32 frame_height;     /* coded height */
  u32 scaled_width;     /* scaled width of the displayed video */
  u32 scaled_height;    /* scaled height of the displayed video */
  u32 scaling_mode;     /* way to scale the frame to output */
  enum DecDpbMode dpb_mode;             /* DPB mode; frame, or field interlaced */
  VP6DecOutFormat output_format;   /* format of the output frame */
} VP6DecInfo;

/* Output structure for VP6DecNextPicture */
typedef struct VP6DecPicture_ {
  u32 pic_id;           /* Identifier of the Frame to be displayed */
  u32 decode_id;
  u32 pic_coding_type;   /* Picture coding type */
  u32 is_intra_frame;    /* Indicates if Frame is an Intra Frame */
  u32 is_golden_frame;   /* Indicates if Frame is a Golden reference Frame */
  u32 nbr_of_err_mbs;     /* Number of concealed MB's in the frame  */
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
  u32 error_info;
  u32 error_ratio;
  struct VP6OutputInfo {
    u32 frame_width;        /* pixels width of the frame as stored in memory */
    u32 frame_height;       /* pixel height of the frame as stored in memory */
    u32 coded_width;
    u32 coded_height;
    const u32 *p_output_frame;    /* Pointer to the frame */
    addr_t output_frame_bus_address;  /* DMA bus address of the output frame buffer */
    const u32 *output_picture_chroma;  /* Pointer to chroma data */
    addr_t output_picture_chroma_bus_address; /* DMA bus address of the chroma data */
    u32 pic_stride;
    u32 pic_stride_ch;
    enum DecPictureFormat output_format;
    struct DecCropParams crop_params; /**< Cropping parameters for the picture */
    struct DWLLinearMem dec400_luma_table;           /**< Buffer properties *//*sunny add for tile status address*/
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
} VP6DecPicture;

struct VP6DecConfig {
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  DecPicAlignment align;
  u32 num_frame_buffers;
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  u32 error_ratio;
  enum DecErrorHandling error_handling;
  enum DecDecoderMode decoder_mode;
  u32 misc_ctrl;
};
/* Decoder instance */
typedef const void *VP6DecInst;

/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

enum DecRet VP6DecInit(VP6DecInst * dec_inst, const void *dwl, struct VP6DecConfig *dec_cfg);

void VP6DecRelease(VP6DecInst dec_inst);

enum DecRet VP6DecDecode(VP6DecInst dec_inst,
                       const VP6DecInput * input, struct DecOutput * output);

enum DecRet VP6DecNextPicture(VP6DecInst dec_inst, VP6DecPicture * output);

enum DecRet VP6DecPictureConsumed(VP6DecInst dec_inst, VP6DecPicture * output);

enum DecRet VP6DecEndOfStream(VP6DecInst dec_inst);

enum DecRet VP6DecAbort(VP6DecInst dec_inst);

enum DecRet VP6DecAbortAfter(VP6DecInst dec_inst);

enum DecRet VP6DecGetInfo(VP6DecInst dec_inst, VP6DecInfo * dec_info);

enum DecRet VP6DecPeek(VP6DecInst dec_inst, VP6DecPicture * output);
enum DecRet VP6DecGetBufferInfo(VP6DecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet VP6DecAddBuffer(VP6DecInst dec_inst, struct DWLLinearMem *info);

enum DecRet VP6DecSetInfo(VP6DecInst dec_inst, struct VP6DecConfig *dec_cfg);
/*------------------------------------------------------------------------------
    Prototype of the API trace funtion. Traces all API entries and returns.
    This must be implemented by the application using the decoder API!
    Argument:
        string - trace message, a null terminated string
------------------------------------------------------------------------------*/
void VP6DecTrace(const char *string);

#ifdef __cplusplus
}
#endif

#endif                       /* __VP6DECAPI_H__ */
