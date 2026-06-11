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
#include "mp4dechwd_container.h"
#include "mp4deccfg.h"
#include "mp4decapi.h"
#include "regdrv.h"
#include "mp4debug.h"
#include "mp4dechwd_utils.h"
#include "mp4decapi_internal.h"
#include "input_queue.h"
#include "dec_log.h"
#ifdef MP4_ASIC_TRACE
#include "mpeg4asicdbgtrace.h"
#endif
#include "sw_util.h"

static u32 MP4DecCheckProfileSupport(DecContainer * dec_cont);
static u32 MP4MarkOutputPicInfo(DecContainer *dec_cont, u32 pic_id, u32 *nbr_err_mbs);

/*------------------------------------------------------------------------------

        Function name:  ClearDataStructures()

        Purpose:        Initialize Data Structures in DecContainer.

        Input:          DecContainer *dec_cont

        Output:         u32

------------------------------------------------------------------------------*/
void MP4API_InitDataStructures(DecContainer * dec_cont) {

  /*
   *  have to be initialized into 1 to enable
   *  decoding stream without VO-headers
   */
  dec_cont->Hdrs.visual_object_verid = 1;
  dec_cont->Hdrs.video_format = 5;
  dec_cont->Hdrs.colour_primaries = 1;
  dec_cont->Hdrs.transfer_characteristics = 1;
  dec_cont->Hdrs.matrix_coefficients = 1;
  dec_cont->Hdrs.low_delay = 0;

  dec_cont->StrmStorage.vp_qp = 1;
  dec_cont->ApiStorage.first_headers = 1;

  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
}

/*------------------------------------------------------------------------------

        Function name:  MP4DecTimeCode

        Purpose:        Write time data to output

        Input:          DecContainer *dec_cont, struct DecTime *time_code

        Output:         void

------------------------------------------------------------------------------*/

void MP4DecTimeCode(DecContainer * dec_cont, struct DecTime * time_code) {

#define DEC_VOPD dec_cont->VopDesc
#define DEC_HDRS dec_cont->Hdrs
#define DEC_STST dec_cont->StrmStorage
#define DEC_SVDS dec_cont->SvDesc

  ASSERT(dec_cont);
  ASSERT(time_code);

  if(DEC_STST.short_video) {

    u32 time_step;

    if (DEC_SVDS.cpcf) {
      DEC_HDRS.vop_time_increment_resolution = 1800000;
      if (DEC_SVDS.cpcfc >> 7) /* factor 1001 (5.1.7) */
        time_step = (DEC_SVDS.cpcfc & 0x7F) * 1001;
      else                     /* factor 1000 (5.1.7) */
        time_step = (DEC_SVDS.cpcfc & 0x7F) * 1000;
    } else {
      DEC_HDRS.vop_time_increment_resolution = 30000;
      time_step = 1001;
    }

    DEC_VOPD.vop_time_increment +=
      (dec_cont->VopDesc.tics_from_prev * time_step);
    while(DEC_VOPD.vop_time_increment >= DEC_HDRS.vop_time_increment_resolution) {
      DEC_VOPD.vop_time_increment -= DEC_HDRS.vop_time_increment_resolution;
      DEC_VOPD.time_code_seconds++;
      if(DEC_VOPD.time_code_seconds > 59) {
        DEC_VOPD.time_code_minutes++;
        DEC_VOPD.time_code_seconds = 0;
        if(DEC_VOPD.time_code_minutes > 59) {
          DEC_VOPD.time_code_hours++;
          DEC_VOPD.time_code_minutes = 0;
          if(DEC_VOPD.time_code_hours > 23) {
            DEC_VOPD.time_code_hours = 0;
          }
        }
      }
    }
  }
  time_code->hours = DEC_VOPD.time_code_hours;
  time_code->minutes = DEC_VOPD.time_code_minutes;
  time_code->seconds = DEC_VOPD.time_code_seconds;
  time_code->time_incr = DEC_VOPD.vop_time_increment;
  time_code->time_res = DEC_HDRS.vop_time_increment_resolution;

#undef DEC_VOPD
#undef DEC_HDRS
#undef DEC_STST
#undef DEC_SVDS

}

/*------------------------------------------------------------------------------

       Function name:  MP4NotCodedVop

       Purpose:        prepare HW for not coded VOP, rlc mode

       Input:          DecContainer *dec_cont, TimeCode *time_code

       Output:         void

------------------------------------------------------------------------------*/

void MP4NotCodedVop(DecContainer * dec_cont) {

  extern const u8 asic_pos_no_rlc[6];
  u32 asic_tmp = 0;
  u32 i = 0;

  asic_tmp |= (1U << ASICPOS_VPBI);
  asic_tmp |= (1U << ASICPOS_MBTYPE);
  asic_tmp |= (1U << ASICPOS_MBNOTCODED);

  asic_tmp |= (dec_cont->StrmStorage.q_p << ASICPOS_QP);
  for(i = 0; i < 6; i++) {
    asic_tmp |= (1 << asic_pos_no_rlc[i]);
  }

  *dec_cont->MbSetDesc.p_ctrl_data_addr = asic_tmp;

  /* only first has VP boundary */

  asic_tmp &= ~(1U << ASICPOS_VPBI);

  for(i = 1; i < dec_cont->VopDesc.total_mb_in_vop; i++) {
    *(dec_cont->MbSetDesc.p_ctrl_data_addr + i) = asic_tmp;
    dec_cont->MbSetDesc.p_mv_data_addr[i*NBR_MV_WORDS_MB] = 0;
  }
  dec_cont->MbSetDesc.p_mv_data_addr[0*NBR_MV_WORDS_MB] = 0;

}

/*------------------------------------------------------------------------------

       Function name:  MP4AllocateBuffers

       Purpose:        Allocate memory

       Input:          DecContainer *dec_cont

       Output:         DEC_MEMFAIL/DEC_OK

------------------------------------------------------------------------------*/

enum DecRet MP4AllocateBuffers(DecContainer * dec_cont) {
#define DEC_VOPD dec_cont->VopDesc

  u32 i;
  i32 ret = 0;
  u32 size_tmp = 0;
  u32 buffers = 0;
  /* Allocate mb control buffer */

  ASSERT(DEC_VOPD.total_mb_in_vop != 0);

  if (dec_cont->rlc_mode && !dec_cont->MbSetDesc.ctrl_data_mem.virtual_address) {
    if (MP4AllocateRlcBuffers(dec_cont) != DEC_OK)
      return (DEC_MEMFAIL);
  }

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
    dec_cont->n_int_buf_size = size_tmp = mpeg4GetRefFrmSize(dec_cont);
    for (i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      dec_cont->StrmStorage.data[i].mem_type = DWL_MEM_TYPE_DPB |
                                               DWL_MEM_TYPE_DMA_DEVICE_ONLY;
      SET_MEM_USAGE(dec_cont->StrmStorage.data[i].mem_type,
                    DWL_MEM_USAGE_OUT_REFERENCE,
                    dec_cont->secure_mode);
      ret |= DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                             &dec_cont->StrmStorage.data[i]);
      dec_cont->StrmStorage.p_pic_buf[i].data_index = i;
      if(dec_cont->StrmStorage.data[i].bus_address == 0) {
        return (DEC_MEMFAIL);
      }
    }
  }

  dec_cont->StrmStorage.direct_mvs.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(dec_cont->StrmStorage.direct_mvs.mem_type, DWL_MEM_USAGE_TMP_DIRMV,
                dec_cont->secure_mode);
  ret = DWLMallocLinear(dec_cont->dwl,
                        ((DEC_VOPD.total_mb_in_vop+3)&~0x3)*4*sizeof(u32),
                        &dec_cont->StrmStorage.direct_mvs);

  dec_cont->StrmStorage.quant_mat_linear.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(dec_cont->StrmStorage.quant_mat_linear.mem_type, DWL_MEM_USAGE_IN_QUANT_MAT,
                dec_cont->secure_mode);
  ret |= DWLMallocLinear(dec_cont->dwl, 2*64,
                         &dec_cont->StrmStorage.quant_mat_linear);
  if(ret) {
    return (DEC_MEMFAIL);
  }

  /* initialize quantization tables */
  if(dec_cont->Hdrs.quant_type)
    MP4SetQuantMatrix(dec_cont);
  /*
   *  dec_cont->MbSetDesc
   */

  dec_cont->MbSetDesc.odd_rlc = 0;

#if 0
  /* initialize first picture buffer grey, may be used as reference
   * in certain error cases */
    (void) DWLmemset(dec_cont->StrmStorage.data[0].virtual_address,
#ifdef ASIC_TRACE_SUPPORT
                     0,
#else
                     128,
#endif
                     dec_cont->StrmStorage.data[0].logical_size);
#endif
  return DEC_OK;

#undef DEC_VOPD
}

/*------------------------------------------------------------------------------

        Function name:  MP4FreeBuffers

        Purpose:        Free memory

        Input:          DecContainer *dec_cont

        Output:

------------------------------------------------------------------------------*/

void MP4FreeBuffers(DecContainer * dec_cont) {
  u32 i=0;

  /* Allocate mb control buffer */
  BqueueRelease2(&dec_cont->StrmStorage.bq);

  if(dec_cont->MbSetDesc.ctrl_data_mem.bus_address)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->MbSetDesc.ctrl_data_mem);

  if(dec_cont->MbSetDesc.mv_data_mem.bus_address)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->MbSetDesc.mv_data_mem);

  if(dec_cont->MbSetDesc.rlc_data_mem.bus_address)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->MbSetDesc.rlc_data_mem);

  if(dec_cont->MbSetDesc.DcCoeffMem.bus_address)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->MbSetDesc.DcCoeffMem);

  if(dec_cont->StrmStorage.direct_mvs.bus_address)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->StrmStorage.direct_mvs);

  if(dec_cont->StrmStorage.quant_mat_linear.bus_address)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->StrmStorage.quant_mat_linear);

  if (dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers ; i++) {
      if(DWL_DEVMEM_VAILD(dec_cont->StrmStorage.data[i]))
        DWLFreeRefFrm(dec_cont->dwl, &dec_cont->StrmStorage.data[i]);
    }
  }

}

/*------------------------------------------------------------------------------

        Function name:  MP4AllocateRlcBuffers

        Purpose:        Allocate memory for rlc mode

        Input:          DecContainer *dec_cont

        Output:         DEC_MEMFAIL/DEC_OK

------------------------------------------------------------------------------*/

enum DecRet MP4AllocateRlcBuffers(DecContainer * dec_cont) {
#define DEC_VOPD dec_cont->VopDesc

  i32 ret = 0;
  u32 size_rlc = 0, size_mv = 0, size_control = 0, size_dc = 0;

  ASSERT(DEC_VOPD.total_mb_in_vop != 0);

  if (dec_cont->rlc_mode) {
    size_control = NBR_OF_WORDS_MB * DEC_VOPD.total_mb_in_vop * 4;
    dec_cont->MbSetDesc.ctrl_data_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(dec_cont->MbSetDesc.ctrl_data_mem.mem_type, DWL_MEM_USAGE_IN_MBCTRL,
                  dec_cont->secure_mode);
    ret |= DWLMallocLinear(dec_cont->dwl, size_control,
                           &dec_cont->MbSetDesc.ctrl_data_mem);

    dec_cont->MbSetDesc.p_ctrl_data_addr =
      dec_cont->MbSetDesc.ctrl_data_mem.virtual_address;

    /* Allocate motion vector data buffer */
    size_mv = NBR_MV_WORDS_MB * DEC_VOPD.total_mb_in_vop * 4;
    dec_cont->MbSetDesc.mv_data_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(dec_cont->MbSetDesc.mv_data_mem.mem_type, DWL_MEM_USAGE_IN_MV,
                  dec_cont->secure_mode);
    ret |= DWLMallocLinear(dec_cont->dwl, size_mv,
                           &dec_cont->MbSetDesc.mv_data_mem);
    dec_cont->MbSetDesc.p_mv_data_addr =
      dec_cont->MbSetDesc.mv_data_mem.virtual_address;

    /* RLC data buffer */

    size_rlc = (_MP4_RLC_BUFFER_SIZE * DEC_VOPD.total_mb_in_vop * 4);
    dec_cont->MbSetDesc.rlc_data_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(dec_cont->MbSetDesc.rlc_data_mem.mem_type, DWL_MEM_USAGE_IN_MBRLC,
                  dec_cont->secure_mode);
    ret |= DWLMallocLinear(dec_cont->dwl, size_rlc,
                           &dec_cont->MbSetDesc.rlc_data_mem);
    dec_cont->MbSetDesc.rlc_data_buffer_size = size_rlc;
    dec_cont->MbSetDesc.p_rlc_data_addr =
      dec_cont->MbSetDesc.rlc_data_mem.virtual_address;
    dec_cont->MbSetDesc.p_rlc_data_curr_addr = dec_cont->MbSetDesc.p_rlc_data_addr;
    dec_cont->MbSetDesc.p_rlc_data_vp_addr = dec_cont->MbSetDesc.p_rlc_data_addr;

    /* Separate DC component data buffer */

    size_dc = (NBR_DC_WORDS_MB * DEC_VOPD.total_mb_in_vop * 4);
    dec_cont->MbSetDesc.DcCoeffMem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(dec_cont->MbSetDesc.DcCoeffMem.mem_type, DWL_MEM_USAGE_IN_MBDC_COEFF,
                  dec_cont->secure_mode);
    ret |= DWLMallocLinear(dec_cont->dwl, size_dc,
                           &dec_cont->MbSetDesc.DcCoeffMem);
    dec_cont->MbSetDesc.p_dc_coeff_data_addr =
      dec_cont->MbSetDesc.DcCoeffMem.virtual_address;

    if(ret)
      return (DEC_MEMFAIL);
  }

  /* reset memories */

  (void)DWLmemset(dec_cont->MbSetDesc.ctrl_data_mem.virtual_address,
                  0x0, size_control);
  (void)DWLmemset(dec_cont->MbSetDesc.mv_data_mem.virtual_address,
                  0x0, size_mv);
  (void)DWLmemset(dec_cont->MbSetDesc.rlc_data_mem.virtual_address,
                  0x0, size_rlc);
  (void)DWLmemset(dec_cont->MbSetDesc.DcCoeffMem.virtual_address,
                  0x0, size_dc);

  return DEC_OK;

#undef DEC_VOPD
}

/*------------------------------------------------------------------------------

        Function name:  MP4DecAllocExtraBPic

        Purpose:        allocate b picture after normal allocation

        Input:          DecContainer *dec_cont

        Output:         MP4DEC_STRMERROR/DEC_OK

------------------------------------------------------------------------------*/

enum DecRet MP4DecAllocExtraBPic(DecContainer * dec_cont) {
  i32 ret = 0;
  u32 size_tmp = 0;
  u32 extra_buffer = 0;

  /* If we already have enough buffers, do nothing. */
  if( dec_cont->StrmStorage.num_buffers >= 3)
    return DEC_OK;

  dec_cont->StrmStorage.num_buffers = 3;

  size_tmp = mpeg4GetRefFrmSize(dec_cont);

  BqueueRelease2(&dec_cont->StrmStorage.bq);
  ret = BqueueInit2(&dec_cont->StrmStorage.bq,
                    dec_cont->StrmStorage.num_buffers );
  if(ret != HANTRO_OK)
    return (DEC_MEMFAIL);
  dec_cont->StrmStorage.data[2].mem_type = DWL_MEM_TYPE_DPB |
                                           DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(dec_cont->StrmStorage.data[2].mem_type,
                DWL_MEM_USAGE_OUT_REFERENCE,
                dec_cont->secure_mode);
  ret = DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                        &dec_cont->StrmStorage.data[2]);
  dec_cont->StrmStorage.p_pic_buf[2].data_index = 2;
  if(dec_cont->StrmStorage.data[2].bus_address == 0 || ret) {
    return (DEC_MEMFAIL);
  }
  if (dec_cont->pp_enabled) {
    /* Add PP output buffers. */
    struct DWLLinearMem pp_buffer = {0};
    u32 pp_width, pp_height, pp_stride, pp_buff_size;

    pp_width = (dec_cont->VopDesc.vop_width * 16) >> dec_cont->dscale_shift_x;
    pp_height = (dec_cont->VopDesc.vop_height * 16) >> dec_cont->dscale_shift_y;
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
    dec_cont->StrmStorage.data[3].mem_type = DWL_MEM_TYPE_DPB |
                                             DWL_MEM_TYPE_DMA_DEVICE_ONLY;
    SET_MEM_USAGE(dec_cont->StrmStorage.data[3].mem_type,
                  DWL_MEM_USAGE_OUT_REFERENCE,
                  dec_cont->secure_mode);
    ret = DWLMallocRefFrm(dec_cont->dwl, size_tmp,
                          &dec_cont->StrmStorage.data[3]);
    if(dec_cont->StrmStorage.data[3].bus_address == 0 || ret) {
      return (DEC_MEMFAIL);
    }
    if (dec_cont->pp_enabled) {
      /* Add PP output buffers. */
      struct DWLLinearMem pp_buffer;
      u32 pp_width, pp_height, pp_stride, pp_buff_size;

      pp_width = (dec_cont->VopDesc.vop_width * 16) >> dec_cont->dscale_shift_x;
      pp_height = (dec_cont->VopDesc.vop_height * 16) >> dec_cont->dscale_shift_y;
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

        Function name:  MP4DecCheckSupport

        Purpose:        Check picture sizes etc

        Input:          DecContainer *dec_cont

        Output:         DEC_STREAM_NOT_SUPPORTED/DEC_OK

------------------------------------------------------------------------------*/

enum DecRet MP4DecCheckSupport(DecContainer * dec_cont) {
#define DEC_VOPD dec_cont->VopDesc

  /* Check height of interlaced pic */

  if(dec_cont->VopDesc.vop_height < (MIN_PIC_HEIGHT_G1 >> 3)
      && dec_cont->Hdrs.interlaced ) {
    APITRACEERR("%s","Interlaced height not supported\n");
    return DEC_STREAM_NOT_SUPPORTED;
  }

  if(DEC_VOPD.total_mb_in_vop > MP4API_DEC_MBS) {
    APITRACEERR("Maximum number of macroblocks exceeded %d \n",
                  DEC_VOPD.total_mb_in_vop);
    return DEC_STREAM_NOT_SUPPORTED;
  }

  if(MP4DecCheckProfileSupport(dec_cont)) {
    APITRACEERR("%s","Profile not supported\n");
    return DEC_STREAM_NOT_SUPPORTED;
  }
  return DEC_OK;

#undef DEC_VOPD
}


/*------------------------------------------------------------------------------

   x.x Function name:  MP4DecPixelAspectRatio

       Purpose:        Set pixel aspext ratio values for GetInfo


       Input:          DecContainer *dec_cont
                       MP4DecInfo * dec_info    pointer to DecInfo

       Output:         void

------------------------------------------------------------------------------*/
void MP4DecPixelAspectRatio(DecContainer * dec_cont, MP4DecInfo * dec_info) {


  APITRACEDEBUG("PAR %d\n", dec_cont->Hdrs.aspect_ratio_info);


  switch(dec_cont->Hdrs.aspect_ratio_info) {

  case 0x2: /* 0010 12:11 */
    dec_info->par_width = 12;
    dec_info->par_height = 11;
    break;

  case 0x3: /* 0011 10:11 */
    dec_info->par_width = 10;
    dec_info->par_height = 11;
    break;

  case 0x4: /* 0100 16:11 */
    dec_info->par_width = 16;
    dec_info->par_height = 11;
    break;

  case 0x5: /* 0101 40:11 */
    dec_info->par_width = 40;
    dec_info->par_height = 33;
    break;

  case 0xF: /* 1111 Extended PAR */
    dec_info->par_width = dec_cont->Hdrs.par_width;
    dec_info->par_height = dec_cont->Hdrs.par_height;
    break;

  default: /* Square */
    dec_info->par_width = dec_info->par_height = 1;
    break;
  }
  return;
}

/*------------------------------------------------------------------------------

        Function name:  MP4DecBufferPicture

        Purpose:        Rotate buffers and store information about picture, and
                        decide whether output the current frame with index if work_out

        Input:          DecContainer *dec_cont
                        pic_id, error_concealment, return value and time information

        Output:         DEC_STRM_PROCESSED / DEC_PIC_DECODED

------------------------------------------------------------------------------*/
enum DecRet MP4DecBufferPicture(DecContainer *dec_cont, u32 pic_id, u32 error_concealment, u32 cycles_per_mb) {
  u32 i, j, work_out, vop_type;
  u32 pic_type;

  ASSERT(dec_cont);
  ASSERT(dec_cont->StrmStorage.out_count <=
         dec_cont->StrmStorage.num_buffers - 1);

  vop_type = dec_cont->VopDesc.vop_coding_type;
  if( vop_type != BVOP ) { /* Buffer I or P picture */
    i = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    if( i >= 16 ) {
      i -= 16;
    }
  } else { /* Buffer B picture */
    j = dec_cont->StrmStorage.out_index + dec_cont->StrmStorage.out_count;
    i = j - 1;
    if( j >= 16 ) j -= 16;
    if( i >= 16 ) i -= 16;
    // if( i < 0 ) i += 16;
    i %= 16;
    dec_cont->StrmStorage.out_buf[j] = dec_cont->StrmStorage.out_buf[i];
  }

  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  work_out = dec_cont->StrmStorage.work_out;
  dec_cont->send_out = 1;

#ifdef USE_PICTURE_DISCARD
  if (!p_pic_buf[work_out].first_show) {
    dec_cont->send_out = 0;
    return DEC_DISCARD_INTERNAL;
  } else {
    u32 t = 0;
    for (t=0; t<(dec_cont->StrmStorage.out_count+1); t++) {
        if ((dec_cont->StrmStorage.out_buf[(dec_cont->StrmStorage.out_index+t)%16] ==
            dec_cont->StrmStorage.work_out) &&
            ((dec_cont->StrmStorage.out_index+t)%16 != i)) {
          dec_cont->send_out = 0;
          return DEC_DISCARD_INTERNAL;
        }
    }
  }
#endif

  /* 1. assign the output info to p_pic_buf with work_out,
   * some info will be used to set regs, like the pic_type
   */
  dec_cont->StrmStorage.out_buf[i] = work_out;
  if(vop_type == IVOP)
    pic_type = DEC_PIC_TYPE_I;
  else if(vop_type == PVOP)
    pic_type = DEC_PIC_TYPE_P;
  else
    pic_type = DEC_PIC_TYPE_B;
  p_pic_buf[work_out].pic_type = pic_type;
  p_pic_buf[work_out].tiled_mode = dec_cont->tiled_reference_enable;

  MP4DecTimeCode(dec_cont, &p_pic_buf[work_out].time_code);

  /* 2. mark the output info, decide if output*/
  u32 discard_error_pic = 0, nbr_err_mbs = 0;
  enum DecErrorInfo work0_error_info = p_pic_buf[dec_cont->StrmStorage.work0].error_info;
  enum DecErrorInfo work1_error_info = p_pic_buf[dec_cont->StrmStorage.work0].error_info ||
                                       p_pic_buf[dec_cont->StrmStorage.work1].error_info;
  if ((error_concealment==HANTRO_TRUE) ||
      (vop_type==PVOP && work0_error_info != DEC_NO_ERROR) ||
      (vop_type==BVOP && work1_error_info != DEC_NO_ERROR)) {
    discard_error_pic = MP4MarkOutputPicInfo(dec_cont, pic_id, &nbr_err_mbs);
    if (discard_error_pic) {
      /* maybe the pp_data hasn't been assigned yet */
      if (dec_cont->pp_enabled && p_pic_buf[work_out].pp_data)
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_pic_buf[work_out].pp_data)));
      BqueuePictureRelease(&dec_cont->StrmStorage.bq, work_out); //TODO: whether need?
      return DEC_STRM_PROCESSED;
    }
  }

  /* 3. these info are determined only after step 2 */
  p_pic_buf[work_out].pic_id = pic_id;
  p_pic_buf[work_out].is_inter = vop_type;
  p_pic_buf[work_out].nbr_err_mbs = nbr_err_mbs;
  p_pic_buf[work_out].cycles_per_mb = cycles_per_mb;

  dec_cont->StrmStorage.out_count++;
  dec_cont->fullness = dec_cont->StrmStorage.out_count;

  return DEC_PIC_DECODED;
}
/*------------------------------------------------------------------------------

       Function name:  MP4SetQuantMatrix

       Purpose:        Set hw to use stream defined or default matrises



       Input:          DecContainer *dec_cont

       Output:         void

------------------------------------------------------------------------------*/
void MP4SetQuantMatrix(DecContainer * dec_cont) {

  u32 i, tmp;
  u8 *p;
  u32 *p_lin;

  const u8 default_intra_mat[64] = {
    8, 17, 18, 19, 21, 23, 25, 27, 17, 18, 19, 21, 23, 25, 27, 28,
    20, 21, 22, 23, 24, 26, 28, 30, 21, 22, 23, 24, 26, 28, 30, 32,
    22, 23, 24, 26, 28, 30, 32, 35, 23, 24, 26, 28, 30, 32, 35, 38,
    25, 26, 28, 30, 32, 35, 38, 41, 27, 28, 30, 32, 35, 38, 41, 45
  };

  const u8 default_non_intra_mat[64] = {
    16, 17, 18, 19, 20, 21, 22, 23, 17, 18, 19, 20, 21, 22, 23, 24,
    18, 19, 20, 21, 22, 23, 24, 25, 19, 20, 21, 22, 23, 24, 26, 27,
    20, 21, 22, 23, 25, 26, 27, 28, 21, 22, 23, 24, 26, 27, 28, 30,
    22, 23, 24, 26, 27, 28, 30, 31, 23, 24, 25, 27, 28, 30, 31, 33
  };


  p = (u8 *)dec_cont->StrmStorage.quant_mat;
  p_lin = (u32 *)dec_cont->StrmStorage.quant_mat_linear.virtual_address;

  if(p[0]) {
    for (i = 0; i < 16; i++) {
      tmp = (p[4*i+0]<<24) | (p[4*i+1]<<16) |
            (p[4*i+2]<<8)  | (p[4*i+3]<<0);
      p_lin[i] = tmp;
    }
  } else { /* default */
    for (i = 0; i < 16; i++) {
      tmp = (default_intra_mat[4*i+0]<<24) |
            (default_intra_mat[4*i+1]<<16) |
            (default_intra_mat[4*i+2]<<8) |
            (default_intra_mat[4*i+3]<<0);
      p_lin[i] = tmp;
    }
  }

  if(p[64]) {
    for (i = 16; i < 32; i++) {
      tmp = (p[4*i+0]<<24) | (p[4*i+1]<<16) |
            (p[4*i+2]<<8)  | (p[4*i+3]<<0);
      p_lin[i] = tmp;
    }
  } else {
    for (i = 0; i < 16; i++) {
      tmp = (default_non_intra_mat[4*i+0]<<24) |
            (default_non_intra_mat[4*i+1]<<16) |
            (default_non_intra_mat[4*i+2]<<8) |
            (default_non_intra_mat[4*i+3]<<0);
      p_lin[i+16] = tmp;
    }
  }

  DWLDMATransData(dec_cont->dwl, &dec_cont->StrmStorage.quant_mat_linear, 0,
                          dec_cont->StrmStorage.quant_mat_linear.size, HOST_TO_DEVICE);

}
/*------------------------------------------------------------------------------

       Function name:  MP4DecCheckProfileSupport

       Purpose:        Check support for ASP tools

       Input:          DecContainer *dec_cont

       Output:         void

------------------------------------------------------------------------------*/
static u32 MP4DecCheckProfileSupport(DecContainer *dec_cont) {
  u32 ret = 0;
  const struct DecHwFeatures *hw_feature = NULL;

  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_MPEG4_DEC);

  if(hw_feature->mpeg4_support == MPEG4_SIMPLE_PROFILE &&
      !dec_cont->StrmStorage.sorenson_spark) {

    if(dec_cont->Hdrs.quant_type)
      ret++;

    if(!dec_cont->Hdrs.low_delay)
      ret++;

    if(dec_cont->Hdrs.interlaced)
      ret++;

    if(dec_cont->Hdrs.quarterpel)
      ret++;
  }

  return ret;
}


/*------------------------------------------------------------------------------

       Function name:  MP4DecBFrameSupport

       Purpose:        Check support for B frames

       Input:          DecContainer *dec_cont

       Output:         void

------------------------------------------------------------------------------*/
u32 MP4DecBFrameSupport(DecContainer *dec_cont) {
  const struct DecHwFeatures *hw_feature = NULL;
  UNUSED(dec_cont);
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_MPEG4_DEC);

  return hw_feature->mpeg4_support == MPEG4_ADVANCED_SIMPLE_PROFILE;
}
/*------------------------------------------------------------------------------

        Function name:  MP4DecResolveData

        Purpose:        Get virtual address for this picture

        Input:          DecContainer *dec_cont

        Output:         void

------------------------------------------------------------------------------*/
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

u32 * MP4DecResolveData(DecContainer * dec_cont, u32 index ) {
  if( index > INVALID_ANCHOR_PICTURE ||
      index >= ARRAY_SIZE(dec_cont->StrmStorage.p_pic_buf))
    return NULL;
  return (u32*)&dec_cont->StrmStorage.data[dec_cont->StrmStorage.
                                    p_pic_buf[index].data_index];
}

/*------------------------------------------------------------------------------

        Function name:  MP4DecResolveBus

        Purpose:        Get bus address for this picture

        Input:          DecContainer *dec_cont

        Output:         void

------------------------------------------------------------------------------*/
addr_t MP4DecResolveBus(DecContainer * dec_cont, u32 index ) {
  if( index > INVALID_ANCHOR_PICTURE )
    return 0;
  if (index >= ARRAY_SIZE(dec_cont->StrmStorage.p_pic_buf))
  	return 0;
  return dec_cont->StrmStorage.data[dec_cont->StrmStorage.
                                    p_pic_buf[index].data_index].bus_address;
}

/*------------------------------------------------------------------------------

        Function name:  MP4DecChangeDataIndex

        Purpose:        Move picture storage to point to a different physical
                        picture

        Input:          DecContainer *dec_cont

        Output:         void

------------------------------------------------------------------------------*/
void MP4DecChangeDataIndex( DecContainer * dec_cont, u32 to, u32 from) {
  dec_cont->StrmStorage.p_pic_buf[to].data_index =
    dec_cont->StrmStorage.p_pic_buf[from].data_index;
}

u32 MP4MarkOutputPicInfo(DecContainer *dec_cont, u32 pic_id, u32 *nbr_err_mbs) {
  u32 discard_error_pic = 0, error_ratio = 0;
  struct ErrorPicInfo error_pic_info;
  u32 work_out = dec_cont->StrmStorage.work_out;
  u32 work0 = dec_cont->StrmStorage.work0;
  u32 work1 = dec_cont->StrmStorage.work1;
  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  picture_t *current_out = &p_pic_buf[work_out];

  /* 1. error_info of current frame . */
  error_pic_info.error_x = GetDecRegister(dec_cont->mp4_regs, HWIF_MB_LOCATION_X);
  error_pic_info.error_y = GetDecRegister(dec_cont->mp4_regs, HWIF_MB_LOCATION_Y);
  error_pic_info.pic_width_in_ctb  = dec_cont->VopDesc.vop_width;
  error_pic_info.pic_height_in_ctb = dec_cont->VopDesc.vop_height;
  error_pic_info.log2_ctb_size = 4;
  error_pic_info.tile_info.num_tile_rows    = 1; /* mpeg4 no tile */
  error_pic_info.tile_info.num_tile_columns = 1;
  *nbr_err_mbs = GetErrorCtbCount(&error_pic_info);

  /* the error can be ignored. */
  if (*nbr_err_mbs == 0)
    dec_cont->error_info = DEC_NO_ERROR;
  error_ratio = (*nbr_err_mbs) * EC_ROUND_COEFF /
                  (dec_cont->VopDesc.vop_width * dec_cont->VopDesc.vop_height);

  /* 2. error_info of reference frame */
  u32 max_error_ratio = 0;
  if (dec_cont->VopDesc.vop_coding_type == BVOP) {
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR ||
        p_pic_buf[work1].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;

    max_error_ratio = MAX(p_pic_buf[work0].error_ratio, p_pic_buf[work1].error_ratio);
  }
  else if (dec_cont->VopDesc.vop_coding_type == PVOP) {
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;

    max_error_ratio = MAX(p_pic_buf[work0].error_ratio, error_ratio);
  }
  error_ratio = MAX(max_error_ratio, error_ratio);

  current_out->error_info = dec_cont->error_info;
  current_out->error_ratio = error_ratio;
  current_out->pic_id = pic_id;
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
