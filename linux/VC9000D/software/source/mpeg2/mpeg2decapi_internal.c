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

#include "basetype.h"
#include "mpeg2hwd_container.h"
#include "mpeg2hwd_cfg.h"
#include "mpeg2decapi.h"
#include "mpeg2hwd_debug.h"
#include "mpeg2hwd_utils.h"
#include "mpeg2decapi_internal.h"
#include "regdrv.h"
#include "dec_log.h"
#ifdef MPEG2_ASIC_TRACE
#include "mpeg2asicdbgtrace.h"
#endif

#include "bqueue.h"
#include "input_queue.h"

static u32 Mpeg2MarkOutputPicInfo(Mpeg2DecContainer *dec_cont, u32 pic_id, u32 *nbr_err_mbs);
/*------------------------------------------------------------------------------

    5.1 Function name:  mpeg2API_InitDataStructures()

        Purpose:        Initialize Data Structures in Mpeg2DecContainer.

        Input:          Mpeg2DecContainer *dec_cont

        Output:         u32

------------------------------------------------------------------------------*/
void mpeg2API_InitDataStructures(Mpeg2DecContainer * dec_cont) {
  /*
   *  have to be initialized into 1 to enable
   *  decoding stream without VO-headers
   */
  dec_cont->Hdrs.video_format = 5;
  dec_cont->Hdrs.transfer_characteristics = 1;
  dec_cont->Hdrs.matrix_coefficients = 1;
  dec_cont->Hdrs.color_primaries = 1;
  dec_cont->Hdrs.progressive_sequence = 1;
  dec_cont->Hdrs.picture_structure = 3;
  dec_cont->Hdrs.frame_pred_frame_dct = 1;
  dec_cont->Hdrs.first_field_in_frame = 0;
  dec_cont->Hdrs.field_index = -1;
  dec_cont->Hdrs.field_out_index = 1;
  dec_cont->Hdrs.video_range = 0;
  dec_cont->StrmStorage.vp_qp = 1;
  dec_cont->ApiStorage.first_headers = 1;
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
  dec_cont->StrmStorage.error_in_hdr = 0;
}

/*------------------------------------------------------------------------------

    5.5 Function name:  mpeg2DecTimeCode();

        Purpose:        Write time data to output

        Input:          Mpeg2DecContainer *dec_cont, struct DecTime *time_code

        Output:         void

------------------------------------------------------------------------------*/

void mpeg2DecTimeCode(Mpeg2DecContainer * dec_cont, struct DecTime * time_code) {

#define DEC_FRAMED dec_cont->FrameDesc
#define DEC_HDRS dec_cont->Hdrs
#define DEC_STST dec_cont->StrmStorage
#define DEC_SVDS dec_cont->SvDesc

  ASSERT(dec_cont);
  ASSERT(time_code);

  time_code->hours = DEC_FRAMED.time_code_hours;
  time_code->minutes = DEC_FRAMED.time_code_minutes;
  time_code->seconds = DEC_FRAMED.time_code_seconds;
  time_code->pictures = DEC_FRAMED.frame_time_pictures;

#undef DEC_FRAMED
#undef DEC_HDRS
#undef DEC_STST
#undef DEC_SVDS

}

/*------------------------------------------------------------------------------

    x.x Function name:  mpeg2AllocateBuffers

        Purpose:        Allocate memory

        Input:          Mpeg2DecContainer *dec_cont

        Output:         DEC_MEMFAIL/DEC_OK

------------------------------------------------------------------------------*/
enum DecRet mpeg2AllocateBuffers(Mpeg2DecContainer * dec_cont) {
#define DEC_FRAMED dec_cont->FrameDesc

  u32 i;
  i32 ret = 0;
  u32 size_tmp = 0;
  u32 buffers = 0;

  ASSERT(DEC_FRAMED.total_mb_in_frame != 0);

  /* allocate Q-table buffer */
  dec_cont->ApiStorage.p_qtable_base.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(dec_cont->ApiStorage.p_qtable_base.mem_type, DWL_MEM_USAGE_IN_QTABLE,
                dec_cont->secure_mode);
  ret |= DWLMallocLinear(dec_cont->dwl, MPEG2DEC_TABLE_SIZE,
                         &dec_cont->ApiStorage.p_qtable_base);
  if(ret)
    return (DEC_MEMFAIL);

  APITRACEDEBUG("Q-table: %p, 0x%11x\n",
                  (void*) dec_cont->ApiStorage.p_qtable_base.virtual_address,
                  dec_cont->ApiStorage.p_qtable_base.bus_address);

  /* Calculate minimum amount of buffers */
  buffers = 3;
  dec_cont->StrmStorage.num_buffers = dec_cont->StrmStorage.max_num_buffers;
  if( dec_cont->StrmStorage.num_buffers < buffers )
    dec_cont->StrmStorage.num_buffers = buffers;
  ret = BqueueInit2(&dec_cont->StrmStorage.bq,
                    dec_cont->StrmStorage.num_buffers );
  if( ret != HANTRO_OK )
    return DEC_MEMFAIL;

  if (dec_cont->pp_enabled) {
    /* Reference images */
    dec_cont->n_int_buf_size = size_tmp = mpeg2GetRefFrmSize(dec_cont);
    for(i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      dec_cont->StrmStorage.p_pic_buf[i].data.mem_type = DWL_MEM_TYPE_DPB |
                                                         DWL_MEM_TYPE_DMA_DEVICE_ONLY;
      SET_MEM_USAGE(dec_cont->StrmStorage.p_pic_buf[i].data.mem_type,
                    DWL_MEM_USAGE_OUT_REFERENCE,
                    dec_cont->secure_mode);
      ret |= DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                             &dec_cont->StrmStorage.p_pic_buf[i].data);

      APITRACEDEBUG("PicBuffer[%d]: %p, 0x%11x\n",
                      i,
                      (void*) dec_cont->StrmStorage.p_pic_buf[i].data.
                      virtual_address,
                      dec_cont->StrmStorage.p_pic_buf[i].data.bus_address);

      if(dec_cont->StrmStorage.p_pic_buf[i].data.bus_address == 0) {
        return (DEC_MEMFAIL);
      }
    }
    /* initialize first picture buffer (work_out is 1 for the first picture)
     * grey, may be used as reference in certain error cases */
    DWLLinearMemset(dec_cont->dwl, &dec_cont->StrmStorage.p_pic_buf[1].data,
      0, 128, 384 * dec_cont->FrameDesc.total_mb_in_frame);
  }

  if(ret) {
    return (DEC_MEMFAIL);
  }

  /*
   *  dec_cont->MbSetDesc
   */

  return DEC_OK;

#undef DEC_FRAMED
}

/*------------------------------------------------------------------------------

    x.x Function name:  mpeg2DecAllocExtraBPic

        Purpose:        Allocate memory

        Input:          Mpeg2DecContainer *dec_cont

        Output:         DEC_MEMFAIL/DEC_OK

------------------------------------------------------------------------------*/
enum DecRet mpeg2DecAllocExtraBPic(Mpeg2DecContainer * dec_cont) {
  i32 ret = 0;
  u32 size_tmp = 0;
  u32 extra_buffer = 0;

  /* If we already have enough buffers, do nothing. */
  if( dec_cont->StrmStorage.num_buffers >= 3)
    return DEC_OK;

  dec_cont->StrmStorage.num_buffers = 3;

  size_tmp =
    (MPEG2API_DEC_FRAME_BUFF_SIZE * dec_cont->FrameDesc.total_mb_in_frame * 4);
  BqueueRelease2(&dec_cont->StrmStorage.bq);
  ret = BqueueInit2(&dec_cont->StrmStorage.bq,
                    dec_cont->StrmStorage.num_buffers );
  if(ret != HANTRO_OK)
    return (DEC_MEMFAIL);


  dec_cont->StrmStorage.p_pic_buf[2].data.mem_type = DWL_MEM_TYPE_DPB |
                                                     DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(dec_cont->StrmStorage.p_pic_buf[2].data.mem_type,
                DWL_MEM_USAGE_OUT_REFERENCE,
                dec_cont->secure_mode);
  ret = DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                        &dec_cont->StrmStorage.p_pic_buf[2].data);
  if(dec_cont->StrmStorage.p_pic_buf[2].data.bus_address == 0 || ret) {
    return (DEC_MEMFAIL);
  }
  if (dec_cont->pp_enabled) {
    /* Add PP output buffers. */
    struct DWLLinearMem pp_buffer = {0};
    u32 pp_width, pp_height, pp_stride, pp_buff_size;

    pp_width = (dec_cont->FrameDesc.frame_width * 16) >> dec_cont->dscale_shift_x;
    pp_height = (dec_cont->FrameDesc.frame_height * 16) >> dec_cont->dscale_shift_y;
    pp_stride = ((pp_width + 15) >> 4) << 4;
    pp_buff_size = pp_stride * pp_height * 3 / 2;
#ifdef SUPPORT_DMA
      pp_buffer.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_TO_HOST;
#endif
    SET_MEM_USAGE(pp_buffer.mem_type, DWL_MEM_USAGE_OUT_PP,
                  dec_cont->secure_mode);
    if(DWLMallocLinear(dec_cont->dwl, pp_buff_size, &pp_buffer) != 0)
      return (DEC_MEMFAIL);

    dec_cont->StrmStorage.pp_buffer[2] = pp_buffer;
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, &pp_buffer);
  }

  /* Allocate "extra" extra B buffer */
  if(extra_buffer) {
    dec_cont->StrmStorage.p_pic_buf[3].data.mem_type = DWL_MEM_TYPE_DPB |
                                                       DWL_MEM_TYPE_DMA_DEVICE_ONLY;
    SET_MEM_USAGE(dec_cont->StrmStorage.p_pic_buf[3].data.mem_type,
                  DWL_MEM_USAGE_OUT_REFERENCE,
                  dec_cont->secure_mode);
    ret = DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                          &dec_cont->StrmStorage.p_pic_buf[3].data);
    if(dec_cont->StrmStorage.p_pic_buf[3].data.bus_address == 0 || ret) {
      return (DEC_MEMFAIL);
    }
    if (dec_cont->pp_enabled) {
      /* Add PP output buffers. */
      struct DWLLinearMem pp_buffer = {0};
      u32 pp_width, pp_height, pp_stride, pp_buff_size;

      pp_width = (dec_cont->FrameDesc.frame_width * 16) >> dec_cont->dscale_shift_x;
      pp_height = (dec_cont->FrameDesc.frame_height * 16) >> dec_cont->dscale_shift_y;
      pp_stride = ((pp_width + 15) >> 4) << 4;
      pp_buff_size = pp_stride * pp_height * 3 / 2;
#ifdef SUPPORT_DMA
      pp_buffer.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_TO_HOST;
#endif
      SET_MEM_USAGE(pp_buffer.mem_type, DWL_MEM_USAGE_OUT_PP,
                    dec_cont->secure_mode);
      if(DWLMallocLinear(dec_cont->dwl, pp_buff_size, &pp_buffer) != 0)
        return (DEC_MEMFAIL);

      dec_cont->StrmStorage.pp_buffer[3] = pp_buffer;
      InputQueueAddBuffer(dec_cont->pp_buffer_queue, &pp_buffer);
    }
  }

  return (DEC_OK);
}

/*------------------------------------------------------------------------------

    x.x Function name:  mpeg2HandleQTables

        Purpose:        Copy Q-tables to HW buffer

        Input:          Mpeg2DecContainer *dec_cont

        Output:

------------------------------------------------------------------------------*/
void mpeg2HandleQTables(Mpeg2DecContainer * dec_cont) {
  u32 i = 0;
  u32 shifter = 32;
  u32 table_word = 0;
  u32 *p_table_base = NULL;

  ASSERT(dec_cont->ApiStorage.p_qtable_base.virtual_address);
  ASSERT(dec_cont->ApiStorage.p_qtable_base.bus_address);

  p_table_base = dec_cont->ApiStorage.p_qtable_base.virtual_address;

  /* QP tables for intra */
  for(i = 0; i < 64; i++) {
    shifter -= 8;
    if(shifter == 24)
      table_word = (dec_cont->Hdrs.q_table_intra[i] << shifter);
    else
      table_word |= (dec_cont->Hdrs.q_table_intra[i] << shifter);

    if(shifter == 0) {
      *(p_table_base) = table_word;
      p_table_base++;
      shifter = 32;
    }
  }

  /* QP tables for non-intra */
  for(i = 0; i < 64; i++) {
    shifter -= 8;
    if(shifter == 24)
      table_word = (dec_cont->Hdrs.q_table_non_intra[i] << shifter);
    else
      table_word |= (dec_cont->Hdrs.q_table_non_intra[i] << shifter);

    if(shifter == 0) {
      *(p_table_base) = table_word;
      p_table_base++;
      shifter = 32;
    }
  }

  DWLDMATransData(dec_cont->dwl, &dec_cont->ApiStorage.p_qtable_base, 0,
                  dec_cont->ApiStorage.p_qtable_base.size, HOST_TO_DEVICE);
}

/*------------------------------------------------------------------------------

    x.x Function name:  mpeg2HandleMpeg1Parameters

        Purpose:        Set MPEG-1 parameters

        Input:          Mpeg2DecContainer *dec_cont

        Output:

------------------------------------------------------------------------------*/

void mpeg2HandleMpeg1Parameters(Mpeg2DecContainer * dec_cont) {
  ASSERT(!dec_cont->Hdrs.mpeg2_stream);

  dec_cont->Hdrs.progressive_sequence = 1;
  dec_cont->Hdrs.chroma_format = 1;
  dec_cont->Hdrs.frame_rate_extension_n = dec_cont->Hdrs.frame_rate_extension_d = 0;
  dec_cont->Hdrs.intra_dc_precision = 0;
  dec_cont->Hdrs.picture_structure = 3;
  dec_cont->Hdrs.frame_pred_frame_dct = 1;
  dec_cont->Hdrs.concealment_motion_vectors = 0;
  dec_cont->Hdrs.intra_vlc_format = 0;
  dec_cont->Hdrs.alternate_scan = 0;
  dec_cont->Hdrs.repeat_first_field = 0;
  dec_cont->Hdrs.chroma420_type = 1;
  dec_cont->Hdrs.progressive_frame = 1;

}

/*------------------------------------------------------------------------------

   x.x Function name:  mpeg2DecCheckSupport

       Purpose:        Check picture sizes etc

       Input:          Mpeg2DecContainer *dec_cont

       Output:         MPEG2DEC_STRMERROR/DEC_OK

------------------------------------------------------------------------------*/

enum DecRet mpeg2DecCheckSupport(Mpeg2DecContainer *dec_cont) {
#define DEC_FRAMED dec_cont->FrameDesc

  if(DEC_FRAMED.total_mb_in_frame > MPEG2API_DEC_MBS) {
    APITRACEERR("Maximum number of macroblocks exceeded %u \n",
                    DEC_FRAMED.total_mb_in_frame);
    return DEC_STREAM_NOT_SUPPORTED;
  }

  return DEC_OK;

#undef DEC_FRAMED
}

/*------------------------------------------------------------------------------

    x.x Function name:  mpeg2DecPreparePicReturn

        Purpose:        Prepare return values for PIC returns
                        For use after HW start

        Input:          Mpeg2DecContainer *dec_cont
                        struct DecOutput *outData    currently used out

        Output:         void

------------------------------------------------------------------------------*/

void mpeg2DecPreparePicReturn(Mpeg2DecContainer * dec_cont) {

#define DEC_FRAMED dec_cont->FrameDesc
#define DEC_MBSD dec_cont->MbSetDesc
#define DEC_STRM dec_cont->StrmDesc
#define DEC_HDRS dec_cont->Hdrs
#define DEC_SVDS dec_cont->SvDesc
#define API_STOR dec_cont->ApiStorage

  ASSERT(dec_cont != NULL);

  if(dec_cont->Hdrs.interlaced) {
    if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE) {
      dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].
      ff[dec_cont->Hdrs.field_index] =
        dec_cont->Hdrs.first_field_in_frame;
    } else {
      dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[0] =
        1;
      dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[1] =
        0;
    }
  } else {
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[0] = 0;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[1] = 0;
  }

  /* update index */
  if(dec_cont->Hdrs.field_index == 1)
    dec_cont->Hdrs.field_index = -1;

  if(dec_cont->Hdrs.first_field_in_frame == 1)
    dec_cont->Hdrs.first_field_in_frame = -1;

  return;

#undef DEC_FRAMED
#undef DEC_MBSD
#undef DEC_STRM
#undef DEC_HDRS
#undef DEC_SVDS
#undef API_STOR

}

/*------------------------------------------------------------------------------

   x.x Function name:  mpeg2DecAspectRatio

       Purpose:        Set aspect ratio values for GetInfo

       Input:          Mpeg2DecContainer *dec_cont
                       Mpeg2DecInfo * dec_info    pointer to DecInfo

       Output:         void

------------------------------------------------------------------------------*/

void mpeg2DecAspectRatio(Mpeg2DecContainer * dec_cont, Mpeg2DecInfo * dec_info) {

  APITRACEDEBUG("SAR %d\n", dec_cont->Hdrs.aspect_ratio_info);

  /* If forbidden or reserved */
  if(dec_cont->Hdrs.aspect_ratio_info == 0 ||
      dec_cont->Hdrs.aspect_ratio_info > 4) {
    dec_info->display_aspect_ratio = 0;
    return;
  }

  switch (dec_cont->Hdrs.aspect_ratio_info) {
  case 0x2:  /* 0010 4:3 */
    dec_info->display_aspect_ratio = DEC_4_3;
    break;

  case 0x3:  /* 0011 16:9 */
    dec_info->display_aspect_ratio = DEC_16_9;
    break;

  case 0x4:  /* 0100 2.21:1 */
    dec_info->display_aspect_ratio = DEC_2_21_1;
    break;

  default:   /* Square 0001 1/1 */
    dec_info->display_aspect_ratio = DEC_1_1;
    break;
  }

  /* TODO!  "DAR" */
  APITRACEDEBUG("DAR %d\n", dec_cont->Hdrs.aspect_ratio_info);

  return;
}

/*------------------------------------------------------------------------------
Function name:  Mpeg2DecBufferPicture
Purpose:        Handles picture buffering
Input:          dec_cont, pic_id, error_concealment, cycles_per_mb
Output:         DEC_STRM_PROCESSED / DEC_PIC_DECODED
------------------------------------------------------------------------------*/
enum DecRet Mpeg2DecBufferPicture(Mpeg2DecContainer *dec_cont, u32 pic_id,
                                  u32 error_concealment, u32 cycles_per_mb) {
  i32 i, j;
  u32 pic_type, coding_type, work_out;

  ASSERT(dec_cont);
  ASSERT(dec_cont->StrmStorage.out_count <=
         dec_cont->StrmStorage.num_buffers - 1);
  coding_type = dec_cont->Hdrs.picture_coding_type;
  if(coding_type != BFRAME) {  /* Buffer I or P picture */
    i = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    if(i >= 16)
      i -= 16;
  } else { /* Buffer B picture */
    j = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    i = j - 1;
    if(j >= 16) j -= 16;
    if(i >= 16) i -= 16; /* Added check due to max 2 pic latency */
    // if(i < 0)   i += 16;
    i %= 16;
    dec_cont->StrmStorage.out_buf[j] = dec_cont->StrmStorage.out_buf[i];
  }

  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  work_out = dec_cont->StrmStorage.work_out;

  /* 1. assign the output info to p_pic_buf with work_out,
   * some info will be used to set regs, like the pic_type
   */
  dec_cont->StrmStorage.out_buf[i] = work_out;

  if(coding_type == IFRAME)
    pic_type = DEC_PIC_TYPE_I;
  else if(coding_type == PFRAME)
    pic_type = DEC_PIC_TYPE_P;
  else if(coding_type == BFRAME)
    pic_type = DEC_PIC_TYPE_B;
  else
    pic_type = DEC_PIC_TYPE_D;

  if((dec_cont->FrameDesc.field_coding_type[0] == IFRAME &&
      dec_cont->FrameDesc.field_coding_type[1] == PFRAME) ||
      (dec_cont->FrameDesc.field_coding_type[0] == PFRAME &&
       dec_cont->FrameDesc.field_coding_type[1] == IFRAME)) {
    p_pic_buf[work_out].pic_code_type[0] = DEC_PIC_TYPE_I;
    p_pic_buf[work_out].pic_code_type[1] = DEC_PIC_TYPE_P;
    p_pic_buf[work_out].pic_type = 1;
  } else {
    p_pic_buf[work_out].pic_code_type[0] = pic_type;
    p_pic_buf[work_out].pic_code_type[1] = pic_type;
    p_pic_buf[work_out].pic_type = (coding_type == IFRAME);
  }
  APITRACEDEBUG("Field pictype [%d/%d]\n", dec_cont->FrameDesc.field_coding_type[0], dec_cont->FrameDesc.field_coding_type[1]);
  mpeg2DecTimeCode(dec_cont, &p_pic_buf[work_out].time_code);

  /* 2. mark the output info, decide if output*/ //TODO
  u32 discard_error_pic = 0, nbr_err_mbs = 0;
  enum DecErrorInfo work0_error_info = p_pic_buf[dec_cont->StrmStorage.work0].error_info;
  enum DecErrorInfo work1_error_info = p_pic_buf[dec_cont->StrmStorage.work0].error_info ||
                                       p_pic_buf[dec_cont->StrmStorage.work1].error_info;
  if ((error_concealment==HANTRO_TRUE) ||
      (coding_type==PFRAME && work0_error_info != DEC_NO_ERROR) ||
      (coding_type==BFRAME && work1_error_info != DEC_NO_ERROR)) {
    discard_error_pic = Mpeg2MarkOutputPicInfo(dec_cont, pic_id, &nbr_err_mbs);
    if (discard_error_pic) {
      /* maybe the pp_data hasn't been assigned yet */
      if (dec_cont->pp_enabled && p_pic_buf[work_out].pp_data)
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_pic_buf[work_out].pp_data)));
      BqueuePictureRelease(&dec_cont->StrmStorage.bq, work_out); //TODO: whether need?
      return DEC_DISCARD_INTERNAL;
    }
  }

  /* 3. these info are determined only after step 2 */
  p_pic_buf[work_out].pic_id = pic_id;
  p_pic_buf[work_out].is_inter = (coding_type != IFRAME);
  p_pic_buf[work_out].tiled_mode = dec_cont->tiled_reference_enable;
  p_pic_buf[work_out].tf = dec_cont->Hdrs.top_field_first;
  p_pic_buf[work_out].rff = dec_cont->Hdrs.repeat_first_field;
  p_pic_buf[work_out].rfc = dec_cont->Hdrs.repeat_frame_count;
  p_pic_buf[work_out].ps = dec_cont->Hdrs.picture_structure;
  if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE)
    p_pic_buf[work_out].nbr_err_mbs = nbr_err_mbs / 2;
  else
    p_pic_buf[work_out].nbr_err_mbs = nbr_err_mbs;
  p_pic_buf[work_out].cycles_per_mb = cycles_per_mb;
  p_pic_buf[work_out].error_info = dec_cont->error_info;
  dec_cont->StrmStorage.out_count++;
  dec_cont->fullness = dec_cont->StrmStorage.out_count;

  return DEC_PIC_DECODED;
}

/*------------------------------------------------------------------------------

    x.x Function name:  mpeg2FreeBuffers

        Purpose:        Allocate memory

        Input:          Mpeg2DecContainer *dec_cont

        Output:         DEC_MEMFAIL/DEC_OK

------------------------------------------------------------------------------*/
void mpeg2FreeBuffers(Mpeg2DecContainer * dec_cont) {
  u32 i;
#define DEC_FRAMED dec_cont->FrameDesc
  BqueueRelease2(&dec_cont->StrmStorage.bq);

  if(dec_cont->ApiStorage.p_qtable_base.virtual_address != NULL) {
    DWLFreeLinear(dec_cont->dwl, &dec_cont->ApiStorage.p_qtable_base);
    dec_cont->ApiStorage.p_qtable_base.virtual_address = NULL;
  }
  if (dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      if(DWL_DEVMEM_VAILD(dec_cont->StrmStorage.p_pic_buf[i].data)) {
        DWLFreeRefFrm(dec_cont->dwl,
                      &dec_cont->StrmStorage.p_pic_buf[i].data);
        dec_cont->StrmStorage.p_pic_buf[i].data.virtual_address = NULL;
        dec_cont->StrmStorage.p_pic_buf[i].data.bus_address = 0;
      }
    }
  }
#undef DEC_FRAMED
}

u32 Mpeg2MarkOutputPicInfo(Mpeg2DecContainer *dec_cont, u32 pic_id, u32 *nbr_err_mbs)
{
  u32 discard_error_pic = 0, error_ratio = 0;
  struct ErrorPicInfo error_pic_info;
  u32 work_out = dec_cont->StrmStorage.work_out;
  u32 work0 = dec_cont->StrmStorage.work0;
  u32 work1 = dec_cont->StrmStorage.work1;
  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  picture_t *current_out = &p_pic_buf[work_out];

  /* 1. error_info of current frame . */
  error_pic_info.error_x = GetDecRegister(dec_cont->mpeg2_regs, HWIF_MB_LOCATION_X);
  error_pic_info.error_y = GetDecRegister(dec_cont->mpeg2_regs, HWIF_MB_LOCATION_Y);
  error_pic_info.pic_width_in_ctb  = dec_cont->FrameDesc.frame_width;
  error_pic_info.pic_height_in_ctb = dec_cont->FrameDesc.frame_height;
  error_pic_info.log2_ctb_size = 4;
  error_pic_info.tile_info.num_tile_rows    = 1; /* mpeg2 no tile */
  error_pic_info.tile_info.num_tile_columns = 1;
  *nbr_err_mbs = GetErrorCtbCount(&error_pic_info);

  /* the error can be ignored. */
  if (*nbr_err_mbs == 0)
    dec_cont->error_info = DEC_NO_ERROR;
  error_ratio = (*nbr_err_mbs) * EC_ROUND_COEFF /
                  (dec_cont->FrameDesc.frame_width * dec_cont->FrameDesc.frame_height);

  /* 2. error_info of reference frame */
  u32 max_error_ratio = 0;
  if (dec_cont->Hdrs.picture_coding_type == BFRAME) {
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR ||
        p_pic_buf[work1].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;

    max_error_ratio = MAX(p_pic_buf[work0].error_ratio, p_pic_buf[work1].error_ratio);
  }
  else if (dec_cont->Hdrs.picture_coding_type == PFRAME) {
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;

    max_error_ratio = MAX(p_pic_buf[work0].error_ratio, error_ratio);
  }
  error_ratio = MAX(max_error_ratio, error_ratio);

  current_out->error_info = dec_cont->error_info;
  current_out->error_ratio = error_ratio;
  // current_out->pic_id = pic_id;
  /* 3. complete output pic EC policy */
  discard_error_pic = (((dec_cont->error_policy & DEC_EC_OUT_NO_ERROR) &&
                        (current_out->error_info != DEC_NO_ERROR)) ||
                       ((dec_cont->error_policy & DEC_EC_OUT_DECISION) &&
                        (current_out->error_info != DEC_NO_ERROR) &&
                        (current_out->error_ratio > dec_cont->error_ratio * 100)));
  if (current_out->error_info != DEC_NO_ERROR) {
    ECErrorInfoReturn(current_out->error_info, current_out->error_ratio, current_out->pic_id);
  }

  return discard_error_pic;
}
