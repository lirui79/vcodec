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

#ifndef __RVDECAPI_H__
#define __RVDECAPI_H__

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

/* decoder output picture format */
typedef enum {
  RVDEC_SEMIPLANAR_YUV420 = 0x020001,
  RVDEC_TILED_YUV420 = 0x020002
} RvDecOutFormat;

/* Decoder instance */
typedef void *RvDecInst;

/* Input structure */
typedef struct {
  u8 *stream;             /* Pointer to stream to be decoded              */
  addr_t stream_bus_address;    /* DMA bus address of the input stream */
  u32 data_len;             /* Number of bytes to be decoded                */
  u32 pic_id;
  u32 timestamp;       /* timestamp of current picture from rv frame header.
                              * NOTE: timestamp of a B-frame should be adjusted referring
                * to its forward reference frame's timestamp */

  u32 slice_info_num;    /* The number of slice offset entries. */
  struct DecSliceInfo *slice_info;     /* Pointer to the slice_info.
                                         * It contains offset value of each slice
                                         * in the data buffer, including start point "0"
                                         * and end point "data_len" */
  enum DecSkipFrameMode skip_frame; /* Skip some frames when decoding. */
  u32 dec_ctrl;
} RvDecInput;

/* stream info filled by RvDecGetInfo */
typedef struct {
  u32 frame_width;
  u32 frame_height;
  u32 coded_width;
  u32 coded_height;
  u32 multi_buff_pp_size;
  u32 pic_buff_size;
  enum DecDpbMode dpb_mode;         /* DPB mode; frame, or field interlaced */
  RvDecOutFormat output_format;

  u32 out_pic_coded_width;
  u32 out_pic_coded_height;
  u32 out_pic_stat;
} RvDecInfo;

typedef struct {
  u32 key_picture;
  u32 pic_id;
  u32 decode_id;
  u32 pic_coding_type;
  u32 number_of_err_mbs;
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
  u32 error_info;
  u32 error_ratio;
  struct RvOutputInfo {
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
} RvDecPicture;

struct RvDecConfig {
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  DecPicAlignment align;
  u32 frame_code_length;
  u32 *frame_sizes;
  u32 rv_version;
  u32 max_frame_width;
  u32 max_frame_height;
  u32 num_frame_buffers;
  u32 error_ratio;
  enum DecErrorHandling error_handling;
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  enum DecDecoderMode decoder_mode;
  u32 misc_ctrl;
};
/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

enum DecRet RvDecInit(RvDecInst * dec_inst, const void *dwl, struct RvDecConfig *dec_cfg);

enum DecRet RvDecDecode(RvDecInst dec_inst,
                     RvDecInput * input,
                     struct DecOutput * output);

enum DecRet RvDecGetInfo(RvDecInst dec_inst, RvDecInfo * dec_info);

enum DecRet RvDecNextPicture(RvDecInst dec_inst, RvDecPicture *picture);

enum DecRet RvDecPictureConsumed(RvDecInst dec_inst,
                              RvDecPicture * picture);

enum DecRet RvDecEndOfStream(RvDecInst dec_inst);

void RvDecRelease(RvDecInst dec_inst);

enum DecRet RvDecPeek(RvDecInst dec_inst, RvDecPicture * picture);
enum DecRet RvDecGetBufferInfo(RvDecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet RvDecAddBuffer(RvDecInst dec_inst, struct DWLLinearMem *info);
enum DecRet RvDecSetInfo(RvDecInst dec_inst, struct RvDecConfig *dec_cfg);
enum DecRet RvDecAbort(RvDecInst dec_inst);

enum DecRet RvDecAbortAfter(RvDecInst dec_inst);

/*------------------------------------------------------------------------------
    Prototype of the API trace funtion. Traces all API entries and returns.
    This must be implemented by the application using the decoder API!
    Argument:
        string - trace message, a null terminated string
------------------------------------------------------------------------------*/
//void RvDecTrace(const char *string);

#ifdef __cplusplus
}
#endif

#endif                       /* __RVDECAPI_H__ */
