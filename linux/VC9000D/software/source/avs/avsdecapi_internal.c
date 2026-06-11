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
#include "avs_container.h"
#include "avs_cfg.h"
#include "avsdecapi.h"
#include "avs_utils.h"
#include "avsdecapi_internal.h"
#include "regdrv.h"
#include "sw_util.h"
#include "errorhandling.h"
#include "dec_log.h"

static u32 AvsMarkOutputPicInfo(DecContainer *dec_cont, u32 pic_id, u32 *nbr_err_mbs);
/*------------------------------------------------------------------------------

    5.1 Function name:  AvsAPI_InitDataStructures()

        Purpose:        Initialize Data Structures in DecContainer.

        Input:          DecContainer *dec_cont

        Output:         u32

------------------------------------------------------------------------------*/
void AvsAPI_InitDataStructures(DecContainer * dec_cont) {
  /*
   *  have to be initialized into 1 to enable
   *  decoding stream without VO-headers
   */
  dec_cont->Hdrs.video_format = 5;
  dec_cont->Hdrs.transfer_characteristics = 1;
  dec_cont->Hdrs.matrix_coefficients = 1;
  dec_cont->Hdrs.progressive_sequence = 1;
  dec_cont->Hdrs.picture_structure = 3;
  dec_cont->StrmStorage.field_out_index = 1;
  dec_cont->Hdrs.sample_range = 0;
  dec_cont->ApiStorage.first_headers = 1;
  dec_cont->StrmStorage.work_out = INVALID_ANCHOR_PICTURE;
  dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work1 =
                                  INVALID_ANCHOR_PICTURE;
}

/*------------------------------------------------------------------------------

    5.5 Function name:  AvsDecTimeCode();

        Purpose:        Write time data to output

        Input:          DecContainer *dec_cont, struct DecTime *time_code

        Output:         void

------------------------------------------------------------------------------*/

void AvsDecTimeCode(DecContainer * dec_cont, struct DecTime * time_code) {


  ASSERT(dec_cont);
  ASSERT(time_code);

  time_code->hours = dec_cont->Hdrs.time_code.hours;
  time_code->minutes = dec_cont->Hdrs.time_code.minutes;
  time_code->seconds = dec_cont->Hdrs.time_code.seconds;
  time_code->pictures = dec_cont->Hdrs.time_code.picture;

}

/*------------------------------------------------------------------------------

    x.x Function name:  AvsAllocateBuffers

        Purpose:        Allocate memory

        Input:          DecContainer *dec_cont

        Output:         DEC_MEMFAIL/DEC_OK

------------------------------------------------------------------------------*/
enum DecRet AvsAllocateBuffers(DecContainer * dec_cont) {

  u32 size_tmp = 0;
  u32 i;
  i32 ret = 0;
  u32 buffers = 0;

  ASSERT(dec_cont->StrmStorage.total_mbs_in_frame != 0);

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
    dec_cont->n_int_buf_size = size_tmp = AvsGetRefFrmSize(dec_cont);
    for(i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      dec_cont->StrmStorage.p_pic_buf[i].data.mem_type = DWL_MEM_TYPE_DPB | DWL_MEM_TYPE_DMA_DEVICE_ONLY;
      SET_MEM_USAGE(dec_cont->StrmStorage.p_pic_buf[i].data.mem_type,
                    DWL_MEM_USAGE_OUT_REFERENCE, dec_cont->secure_mode);
      ret |= DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                             &dec_cont->StrmStorage.p_pic_buf[i].data);

      APITRACEDEBUG("PicBuffer[%d]: %p, %x\n",
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
    u32 set_value = 0;
#ifndef ASIC_TRACE_SUPPORT
    set_value = 128;
#endif
    DWLLinearMemset(dec_cont->dwl, &dec_cont->StrmStorage.p_pic_buf[1].data,
      0, set_value, dec_cont->StrmStorage.p_pic_buf[1].data.logical_size);

  }

  dec_cont->StrmStorage.direct_mvs.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(dec_cont->StrmStorage.direct_mvs.mem_type, DWL_MEM_USAGE_TMP_DIRMV, dec_cont->secure_mode);
  u32 total_mbs_in_frame = NEXT_MULTIPLE(dec_cont->StrmStorage.total_mbs_in_frame, 2);
  ret |= DWLMallocLinear(dec_cont->dwl,
                         NEXT_MULTIPLE(((total_mbs_in_frame / 2 + 1)&
                             ~0x1) * 4 * sizeof(u32), MAX(16, ALIGN(dec_cont->align))) * 2,
                         &dec_cont->StrmStorage.direct_mvs);

  if(ret) {
    return (DEC_MEMFAIL);
  }

  return DEC_OK;

}

/*------------------------------------------------------------------------------

   x.x Function name:  AvsDecCheckSupport

       Purpose:        Check picture sizes etc

       Input:          DecContainer *dec_cont

       Output:         AVSDEC_STRMERROR/DEC_OK

------------------------------------------------------------------------------*/

enum DecRet AvsDecCheckSupport(DecContainer * dec_cont) {
  if(dec_cont->StrmStorage.total_mbs_in_frame > AVSAPI_DEC_MBS) {
    APITRACEDEBUG("Maximum number of macroblocks exceeded: %d \n",
                  dec_cont->StrmStorage.total_mbs_in_frame);
    return DEC_STREAM_NOT_SUPPORTED;
  }

  return DEC_OK;

}

/*------------------------------------------------------------------------------

    x.x Function name:  AvsDecPreparePicReturn

        Purpose:        Prepare return values for PIC returns
                        For use after HW start

        Input:          DecContainer *dec_cont
                        struct DecOutput *outData    currently used out

        Output:         void

------------------------------------------------------------------------------*/

void AvsDecPreparePicReturn(DecContainer * dec_cont) {

  ASSERT(dec_cont != NULL);

  if(!dec_cont->Hdrs.progressive_sequence) {
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[0] = 1;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[1] = 0;
  } else {
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[0] = 0;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].ff[1] = 0;
  }

  return;

}

/*------------------------------------------------------------------------------

   x.x Function name:  AvsDecAspectRatio

       Purpose:        Set aspect ratio values for GetInfo

       Input:          DecContainer *dec_cont
                       AvsDecInfo * dec_info    pointer to DecInfo

       Output:         void

------------------------------------------------------------------------------*/

void AvsDecAspectRatio(DecContainer * dec_cont, AvsDecInfo * dec_info) {

  APITRACEDEBUG("SAR %d\n", dec_cont->Hdrs.aspect_ratio);

  /* If forbidden or reserved */
  if(dec_cont->Hdrs.aspect_ratio == 0 || dec_cont->Hdrs.aspect_ratio > 4) {
    dec_info->display_aspect_ratio = 0;
    return;
  }

  switch (dec_cont->Hdrs.aspect_ratio) {
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

}
/*------------------------------------------------------------------------------
Function name:  AvsDecBufferPicture
Purpose:        Handles picture buffering
Input:          dec_cont, pic_id, error_concealment, cycles_per_mb
Output:         DEC_STRM_PROCESSED / DEC_PIC_DECODED
------------------------------------------------------------------------------*/
enum DecRet AvsDecBufferPicture(DecContainer * dec_cont, u32 pic_id,
                         u32 error_concealment, u32 cycles_per_mb) {
  i32 i, j;
  u32 pic_type, coding_type, work_out;

  ASSERT(dec_cont);
  ASSERT(dec_cont->StrmStorage.out_count <= 16);

  coding_type = dec_cont->Hdrs.pic_coding_type;
  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  work_out = dec_cont->StrmStorage.work_out;

  /* 1. mark the output info, decide if output*/
  u32 discard_error_pic = 0, nbr_err_mbs = 0;
  enum DecErrorInfo work0_error_info = p_pic_buf[dec_cont->StrmStorage.work0].error_info;
  enum DecErrorInfo work1_error_info = p_pic_buf[dec_cont->StrmStorage.work0].error_info ||
                                       p_pic_buf[dec_cont->StrmStorage.work1].error_info;
  if ((error_concealment==HANTRO_TRUE) ||
      (coding_type==PFRAME && work0_error_info != DEC_NO_ERROR) ||
      (coding_type==BFRAME && work1_error_info != DEC_NO_ERROR)) {
    discard_error_pic = AvsMarkOutputPicInfo(dec_cont, pic_id, &nbr_err_mbs);
    if (discard_error_pic) {
      /* maybe the pp_data hasn't been assigned yet */
      if (dec_cont->pp_enabled && p_pic_buf[work_out].pp_data)
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_pic_buf[work_out].pp_data)));
      BqueuePictureRelease(&dec_cont->StrmStorage.bq, work_out); //TODO: whether need?
      return DEC_DISCARD_INTERNAL;
    }
  }

  if(coding_type != BFRAME) {  /* Buffer I or P picture */
    i = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    if(i >= 16)
      i -= 16;
  } else { /* Buffer B picture */
    j = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    i = j - 1;
    if(j >= 16) j -= 16;
    // if(i < 0)   i += 16;
    i %= 16;
    dec_cont->StrmStorage.out_buf[j] = dec_cont->StrmStorage.out_buf[i];
  }

  /* 2. assign the output info to p_pic_buf with work_out,
   * some info will be used to set regs, like the pic_type
   */
  dec_cont->StrmStorage.out_buf[i] = work_out;

  if(coding_type == IFRAME)
    pic_type = DEC_PIC_TYPE_I;
  else if(coding_type == PFRAME)
    pic_type = DEC_PIC_TYPE_P;
  else
    pic_type = DEC_PIC_TYPE_B;

  p_pic_buf[work_out].pic_code_type = pic_type;
  p_pic_buf[work_out].pic_type = (coding_type == IFRAME);
  AvsDecTimeCode(dec_cont, &p_pic_buf[work_out].time_code);

  /* 3. these info are determined only after step 2 */
  p_pic_buf[work_out].pic_id = pic_id;
  p_pic_buf[work_out].is_inter = (coding_type != IFRAME);
  p_pic_buf[work_out].tiled_mode = dec_cont->tiled_reference_enable;

  p_pic_buf[work_out].tf = dec_cont->Hdrs.top_field_first;
  p_pic_buf[work_out].rff = dec_cont->Hdrs.repeat_first_field;
  if (coding_type != BFRAME)
    p_pic_buf[work_out].picture_distance = dec_cont->Hdrs.picture_distance;

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

    x.x Function name:  AvsFreeBuffers

        Purpose:

        Input:          DecContainer *dec_cont

        Output:

------------------------------------------------------------------------------*/
void AvsFreeBuffers(DecContainer * dec_cont) {
  u32 i;

  BqueueRelease2( &dec_cont->StrmStorage.bq );

  /* Reference images */
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
  if (DWL_DEVMEM_VAILD(dec_cont->StrmStorage.direct_mvs))
    DWLFreeLinear(dec_cont->dwl, &dec_cont->StrmStorage.direct_mvs);

  dec_cont->StrmStorage.direct_mvs.virtual_address = NULL;
}

u32 AvsMarkOutputPicInfo(DecContainer *dec_cont, u32 pic_id, u32 *nbr_err_mbs)
{
  u32 discard_error_pic = 0, error_ratio = 0;
  struct ErrorPicInfo error_pic_info;
  u32 work_out = dec_cont->StrmStorage.work_out;
  u32 work0 = dec_cont->StrmStorage.work0;
  u32 work1 = dec_cont->StrmStorage.work1;
  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  picture_t *current_out = &p_pic_buf[work_out];

  /* 1. error_info of current frame . */
  error_pic_info.error_x = GetDecRegister(dec_cont->avs_regs, HWIF_MB_LOCATION_X);
  error_pic_info.error_y = GetDecRegister(dec_cont->avs_regs, HWIF_MB_LOCATION_Y);
  error_pic_info.pic_width_in_ctb  = dec_cont->StrmStorage.frame_width;
  error_pic_info.pic_height_in_ctb = dec_cont->StrmStorage.frame_height;
  error_pic_info.log2_ctb_size = 4;
  error_pic_info.tile_info.num_tile_rows    = 1; /* avs no tile */
  error_pic_info.tile_info.num_tile_columns = 1;
  *nbr_err_mbs = GetErrorCtbCount(&error_pic_info);

  /* the error can be ignored. */
  if (*nbr_err_mbs == 0)
    dec_cont->error_info = DEC_NO_ERROR;
  error_ratio = (*nbr_err_mbs) * EC_ROUND_COEFF /
                  (dec_cont->StrmStorage.frame_width * dec_cont->StrmStorage.frame_height);

  /* 2. error_info of reference frame */
  u32 max_error_ratio = 0;
  if (dec_cont->Hdrs.pic_coding_type == BFRAME) {
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR ||
        p_pic_buf[work1].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;

    max_error_ratio = MAX(p_pic_buf[work0].error_ratio, p_pic_buf[work1].error_ratio);
  }
  else if (dec_cont->Hdrs.pic_coding_type == PFRAME) {
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;

    max_error_ratio = MAX(p_pic_buf[work0].error_ratio, error_ratio);
  }
  error_ratio = MAX(max_error_ratio, error_ratio);

  current_out->error_info = dec_cont->error_info;
  current_out->error_ratio = error_ratio;

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
