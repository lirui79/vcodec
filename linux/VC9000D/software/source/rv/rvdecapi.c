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

#include "version.h"
#include "basetype.h"
#include "rv_container.h"
#include "rv_utils.h"
#include "rv_strm.h"
#include "rvdecapi.h"
#include "rvdecapi_internal.h"
#include "dwl.h"
#include "regdrv.h"
#include "rv_headers.h"
#include "deccfg.h"
#include "rv_rpr.h"
#include "ppu.h"
#include "rv_debug.h"

#include "tiledref.h"
#include "commonconfig.h"
#include "errorhandling.h"
#include "vpufeature.h"
#include "sw_util.h"
#include "dec_log.h"
#include "commonfunction.h"

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif

#define RV_BUFFER_UNDEFINED    16

#define RV_DEC_X170_MODE 8 /* TODO: What's the right mode for Hukka */

#define ID8170_DEC_TIMEOUT        0xFFU
#define ID8170_DEC_SYSTEM_ERROR   0xFEU
#define ID8170_DEC_HW_RESERVED    0xFDU

#define RVDEC_UPDATE_POUTPUT

#define RVDEC_NON_PIPELINE_AND_B_PICTURE \
        ((!dec_cont->pp_config_query.pipeline_accepted) \
             && dec_cont->FrameDesc.pic_coding_type == RV_B_PIC)

static u32 rvHandleVlcModeError(RvDecContainer * dec_cont, u32 pic_num);
static void rvHandleFrameEnd(RvDecContainer * dec_cont);
static u32 RunDecoderAsic(RvDecContainer * dec_cont, addr_t strm_bus_address,
                   const struct DecHwFeatures *hw_feature);
static u32 RvSetRegs(RvDecContainer * dec_cont, addr_t strm_bus_address,
                     const struct DecHwFeatures *hw_feature);
static void RvFillPicStruct(RvDecPicture * picture,
                          RvDecContainer * dec_cont, u32 pic_index);
static void RvSetExternalBufferInfo(RvDecContainer * dec_cont);

static enum DecRet RvDecNextPicture_INTERNAL(RvDecInst dec_inst,
                                   RvDecPicture * picture, u32 end_of_stream);
static u32 RvCycleCount(RvDecContainer *dec_cont);
static void RvEnterAbortState(RvDecContainer *dec_cont);
static void RvExistAbortState(RvDecContainer *dec_cont);
static void RvEmptyBufferQueue(RvDecContainer *dec_cont);
static void RvCheckBufferRealloc(RvDecContainer *dec_cont);

static enum ECDataState rvECGetInDataAction(u32 error_policy);
static void rvECHandleReconData(RvDecContainer *dec_cont, u32 is_update_recon);
static void rvECMarkOutDataErrInfo(RvDecContainer *dec_cont, u32 asic_status, u32 is_i_frame);
static enum ECDataState rvECGetOutDataAction(RvDecContainer *dec_cont, u32 error_ratio);

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void RvCaseInfoCollect(RvDecContainer *dec_cont, CaseInfo *case_info);
#endif

/*------------------------------------------------------------------------------

    Function: RvDecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance.

        Inputs:
            error_handling
                            Flag to determine which error concealment method to use.

        Outputs:
            dec_inst        pointer to initialized instance is stored here

        Returns:
            DEC_OK       successfully initialized the instance
            RVDEC_MEM_FAIL memory allocation failed

------------------------------------------------------------------------------*/
enum DecRet RvDecInit(RvDecInst * dec_inst, const void *dwl, struct RvDecConfig *dec_cfg) {
  /*@null@ */ RvDecContainer *dec_cont;
  u32 core_mask;
  enum DecRet ret;
  enum DWLRet tmp;

  APITRACE("%s","RvDecInit#\n");
  APITRACEDEBUG("%s","RvAPI_DecoderInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if(((-1) >> 1) != (-1)) {
    APITRACEERR("%s","RVDecInit# ERROR: Right shift is not signed\n");
    return (DEC_INITFAIL);
  }
  /*lint -restore */

  if(dec_inst == NULL) {
    APITRACEERR("%s","RVDecInit# ERROR: dec_inst == NULL\n");
    return (DEC_PARAM_ERROR);
  }

  *dec_inst = NULL;
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_RV_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","RVDecInit# rv not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  APITRACEDEBUG("size of RvDecContainer %d \n", sizeof(RvDecContainer));
  dec_cont = (RvDecContainer *) DWLmalloc(sizeof(RvDecContainer));
  if(dec_cont == NULL) {
    APITRACEERR("%s","RvDecInit# Memory allocation failed\n");
    return (DEC_MEMFAIL);
  }
  /* set everything initially zero */
  DWLmemset(dec_cont, 0, sizeof(RvDecContainer));

  dec_cont->StrmStorage.slices.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(dec_cont->StrmStorage.slices.mem_type, DWL_MEM_USAGE_IN_STRM_SLICE,
                dec_cont->secure_mode);
  tmp = DWLMallocLinear(dwl, RV_DEC_X170_MAX_NUM_SLICES*sizeof(u32),
                        &dec_cont->StrmStorage.slices);
  if( tmp == DWL_ERROR) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->dwl = dwl;
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_RV_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->rv_regs[0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_RV_DEC);
  SetCommonConfigRegs(dec_cont->rv_regs);

  dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VCD;

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }
  dec_cont->StrmStorage.release_buffer = 0;

  dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;
  if(dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS)
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;
  dec_cont->StrmStorage.max_num_buffers = dec_cfg->num_frame_buffers;

  dec_cont->StrmStorage.is_rv8 = dec_cfg->rv_version == 0;
  if (dec_cfg->rv_version == 0 && dec_cfg->frame_sizes != NULL) {
    dec_cont->StrmStorage.frame_code_length = dec_cfg->frame_code_length;
    DWLmemcpy(dec_cont->StrmStorage.frame_sizes, dec_cfg->frame_sizes,
              18*sizeof(u32));
    SetDecRegister(dec_cont->rv_regs, HWIF_FRAMENUM_LEN,
                   dec_cfg->frame_code_length);
  }

  /* prediction filter taps */
  if (dec_cfg->rv_version == 0) {
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_0, -1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_1, 12);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_2,  6);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_0,  6);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_1,  9);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_2,  1);
  } else {
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_0,  1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_1, -5);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_0_2, 20);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_0,  1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_1, -5);
    SetDecRegister(dec_cont->rv_regs, HWIF_PRED_BC_TAP_1_2, 52);
  }

  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_MODE, RV_DEC_X170_MODE);
  SetDecRegister(dec_cont->rv_regs, HWIF_RV_PROFILE, dec_cfg->rv_version != 0);

  dec_cont->StrmStorage.max_frame_width = dec_cfg->max_frame_width;
  dec_cont->StrmStorage.max_frame_height = dec_cfg->max_frame_height;
  dec_cont->StrmStorage.max_mbs_per_frame =
    ((dec_cont->StrmStorage.max_frame_width +15)>>4)*
    ((dec_cont->StrmStorage.max_frame_height+15)>>4);

  InitWorkarounds(dwl, RV_DEC_X170_MODE, &dec_cont->workarounds);

  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;

  dec_cont->seq_state = SEQ_CLEAN;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  SetECPolicy(dec_cfg->error_handling, 0, &dec_cont->error_policy);

  rvAPI_InitDataStructures(dec_cont);

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);

  APITRACEDEBUG("Container %p\n", (void*) dec_cont);
  APITRACE("%s","RvDecInit: OK\n");

  *dec_inst = (RvDecContainer *) dec_cont;

  return (DEC_OK);

err:
  pthread_mutex_destroy(&dec_cont->protect_mutex);
  DWLfree(dec_cont);
  return ret;
}

/*------------------------------------------------------------------------------

    Function: RvDecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before RvDecDecode function has
            indicated that headers are ready.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            DEC_OK            success
            DEC_PARAM_ERROR     invalid parameters

------------------------------------------------------------------------------*/
enum DecRet RvDecGetInfo(RvDecInst dec_inst, RvDecInfo * dec_info) {

  DecFrameDesc *p_frame_d;
  DecApiStorage *p_api_stor;
  DecHdrs *p_hdrs;
  RvDecPicture *p_out_pic;
  u32 out_pic_stat;

  APITRACE("%s","RvDecGetInfo#\n");

  if(dec_inst == NULL || dec_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  p_api_stor = &((RvDecContainer*)dec_inst)->ApiStorage;
  p_frame_d = &((RvDecContainer*)dec_inst)->FrameDesc;
  p_hdrs = &((RvDecContainer*)dec_inst)->Hdrs;
  p_out_pic = &((RvDecContainer*) dec_inst)->out_pic;
  out_pic_stat = ((RvDecContainer*) dec_inst)->output_stat;

  dec_info->multi_buff_pp_size = 2;
  dec_info->pic_buff_size = ((RvDecContainer *)dec_inst)->buf_num;

  if(p_api_stor->DecStat == UNINIT || p_api_stor->DecStat == INITIALIZED) {
    return DEC_HDRS_NOT_RDY;
  }

  dec_info->frame_width = p_frame_d->frame_width << 4;
  dec_info->frame_height = p_frame_d->frame_height << 4;

  dec_info->coded_width = p_hdrs->horizontal_size;
  dec_info->coded_height = p_hdrs->vertical_size;

  dec_info->dpb_mode = DEC_DPB_FRAME;


  dec_info->output_format = RVDEC_TILED_YUV420;


  dec_info->out_pic_coded_width = p_out_pic->pictures[0].coded_width;
  dec_info->out_pic_coded_height = p_out_pic->pictures[0].coded_height;
  dec_info->out_pic_stat = out_pic_stat;

  APITRACE("%s","RvDecGetInfo: OK\n");
  return (DEC_OK);

}

/*------------------------------------------------------------------------------

    Function: RvDecDecode

        Functional description:
            Decode stream data. Calls StrmDec_Decode to do the actual decoding.

        Input:
            dec_inst     decoder instance
            input      pointer to input struct

        Outputs:
            output     pointer to output struct

        Returns:
            DEC_NOT_INITIALIZED   decoder instance not initialized yet
            DEC_PARAM_ERROR       invalid parameters

            DEC_STRM_PROCESSED    stream buffer decoded
            DEC_HDRS_RDY          headers decoded
            DEC_PIC_DECODED       decoding of a picture finished
            DEC_STRM_ERROR        serious error in decoding, no
                                       valid parameter sets available
                                       to decode picture data

------------------------------------------------------------------------------*/

enum DecRet RvDecDecode(RvDecInst dec_inst, RvDecInput * input, struct DecOutput * output) {
  RvDecContainer *dec_cont;
  enum DecRet internal_ret;
  DecApiStorage *p_api_stor;
  DecStrmDesc *p_strm_desc;
  u32 strm_dec_result;
  u32 asic_status;
  i32 ret = 0;
  u32 error_detected = 0;
  u32 i;
  u32 align;
  u32 *slice_info;
  u32 contains_invalid_slice = HANTRO_FALSE;
  const struct DecHwFeatures *hw_feature = NULL;

  APITRACE("%s","Rv_dec_decode#\n");

  if(input == NULL || output == NULL || dec_inst == NULL) {
    APITRACEERR("%s","RvDecDecode# ERROR: PARAM_ERROR\n");
    return DEC_PARAM_ERROR;
  }

  dec_cont = ((RvDecContainer *) dec_inst);
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_RV_DEC);
  p_api_stor = &dec_cont->ApiStorage;
  p_strm_desc = &dec_cont->StrmDesc;

  /*
   *  Check if decoder is in an incorrect mode
   */
  if(p_api_stor->DecStat == UNINIT) {

    APITRACEERR("%s","RvDecDecode: ERROR: NOT_INITIALIZED\n");
    return DEC_NOT_INITIALIZED;
  }

  if(dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      input->stream == NULL || input->stream_bus_address == 0) {
    APITRACEERR("%s","RvDecDecode# ERROR: PARAM_ERROR\n");
    return DEC_PARAM_ERROR;
  }

  /* If we have set up for delayed resolution change, do it here */
  if(dec_cont->StrmStorage.rpr_detected) {
    u32 new_width, new_height = 0;
    u32 num_pics_resampled = 0;
    u32 resample_pics[2] = {0};
    struct DWLLinearMem tmp_data;
#ifndef USE_OMXIL_BUFFER
    BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
    if (dec_cont->pp_enabled) {
      InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
    }
#endif

    dec_cont->StrmStorage.rpr_detected = 0;

    num_pics_resampled = 0;
    if( dec_cont->StrmStorage.rpr_next_pic_type == RV_P_PIC) {
      resample_pics[0] = dec_cont->StrmStorage.work0;
      num_pics_resampled = 1;
    } else if ( dec_cont->StrmStorage.rpr_next_pic_type == RV_B_PIC ) {
      /* B picture resampling not supported (would affect picture output
       * order and co-located MB data). So let's skip B frames until
       * next reference picture. */
      dec_cont->StrmStorage.skip_b = 1;
    }

    /* Allocate extra picture buffer for resampling */
    if( num_pics_resampled ) {
      internal_ret = rvAllocateRprBuffer( dec_cont );
      if( internal_ret != DEC_OK ) {
        APITRACEERR("%s","ALLOC RPR BUFFER FAIL\n");
        APITRACEERR("%s","RvDecDecode# MEMFAIL\n");
        return (internal_ret);
      }
    }

    new_width = dec_cont->tmp_hdrs.horizontal_size;
    new_height = dec_cont->tmp_hdrs.vertical_size;

    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    for(i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;

      /* if user does not set crop but set scale, should modify
       * crop.width and crop.height when dynamic resolution*/
      if(!ppu_cfg->crop.set_by_user) {
        ppu_cfg->crop.width = new_width;
        ppu_cfg->crop.height = new_height;
        if(ppu_cfg->scale.scale_by_ratio && ppu_cfg->scale.ratio_x) {
          /* behavior consistent with other formats in fixed scale mode,
           * chroma_offset also need reset when resolution change.*/
          ppu_cfg->scale.width = ppu_cfg->crop.width / ppu_cfg->scale.ratio_x;
          ppu_cfg->scale.height = ppu_cfg->crop.height / ppu_cfg->scale.ratio_y;
        }
      }
    }
    if (CheckPpUnitConfig(hw_feature, new_width, new_height, 0, 8, PP_CHROMA_420, dec_cont->ppu_cfg))
      return DEC_PARAM_ERROR;

    if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->next_buf_size ) {
      p_api_stor->DecStat = HEADERSDECODED;
    } else if (dec_cont->pp_enabled) {
      InputQueueBufferClear(dec_cont->dwl, dec_cont->pp_buffer_queue);
    }

    /* Resample ref picture(s). Should be safe to do at this point; all
     * original size pictures are output before this point. */
    for( i = 0 ; i < num_pics_resampled ; ++i ) {
      u32 j = resample_pics[i];
      picture_t * p_ref_pic;

      p_ref_pic = &dec_cont->StrmStorage.p_pic_buf[j];
      if (!p_ref_pic->coded_width || !p_ref_pic->coded_height ||
          !p_ref_pic->frame_width || !p_ref_pic->frame_height) {
          p_ref_pic->coded_width =  dec_cont->Hdrs.horizontal_size;
          p_ref_pic->frame_width = ( 15 + dec_cont->Hdrs.horizontal_size ) & ~15;
          p_ref_pic->coded_height = dec_cont->Hdrs.vertical_size;
          p_ref_pic->frame_height = ( 15 + dec_cont->Hdrs.vertical_size ) & ~15;
      }
      if( p_ref_pic->coded_width == new_width &&
          p_ref_pic->coded_height == new_height )
        continue;
#if 0
      PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
      for(i = 0; i < 4; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;

        /* if user does not set crop but set scale, should modify
         * crop.width and crop.height when dynamic resolution*/
        if(!ppu_cfg->crop.set_by_user) {
          ppu_cfg->crop.width = new_width;
          ppu_cfg->crop.height = new_height;
          if(ppu_cfg->scale.set_by_user && ppu_cfg->scale.ratio_x) {
            /* behavior consistent with other formats in fixed scale mode,
             * chroma_offset also need reset when resolution change.*/
            ppu_cfg->scale.width = new_width / ppu_cfg->scale.ratio_x;
            ppu_cfg->scale.height = new_height / ppu_cfg->scale.ratio_y;
            if(ppu_cfg->tiled_e)
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height / 4;
            else
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height;
          } else if (ppu_cfg->scale.set_by_user && !ppu_cfg->scale.ratio_x) {
            /* not reset when flexible scale mode */
          } else {
            ppu_cfg->scale.width = new_width;
            ppu_cfg->scale.height = new_height;
            if(ppu_cfg->tiled_e)
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height / 4;
            else
              ppu_cfg->chroma_offset = ppu_cfg->ystride * ppu_cfg->scale.height;
          }
        }
      }
#endif
      align = 1 << dec_cont->align;
      rvRpr( p_ref_pic,
             &dec_cont->StrmStorage.p_rpr_buf,
             &dec_cont->StrmStorage.rpr_work_buffer,
             0 /*round*/,
             new_width,
             new_height,
             dec_cont->tiled_reference_enable, align,
             dec_cont->dwl);

      p_ref_pic->coded_width = new_width;
      p_ref_pic->frame_width = ( 15 + new_width ) & ~15;
      p_ref_pic->coded_height = new_height;
      p_ref_pic->frame_height = ( 15 + new_height ) & ~15;

      tmp_data = dec_cont->StrmStorage.p_rpr_buf.data;
      dec_cont->StrmStorage.p_rpr_buf.data = p_ref_pic->data;
      p_ref_pic->data = tmp_data;
    }

    dec_cont->Hdrs.horizontal_size = new_width;
    dec_cont->Hdrs.vertical_size = new_height;

    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_WIDTH_IN_CBS,
                    dec_cont->FrameDesc.frame_width << 1);
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_HEIGHT_IN_CBS,
                    dec_cont->FrameDesc.frame_height << 1);


    dec_cont->StrmStorage.strm_dec_ready = HANTRO_TRUE;
  }

  if (p_api_stor->DecStat == HEADERSDECODED) {
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    RvCheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = DEC_WAITING_FOR_BUFFER;
        }
        rvFreeBuffers(dec_cont);

        if( dec_cont->StrmStorage.max_frame_width == 0 ) {
          dec_cont->StrmStorage.max_frame_width =
            dec_cont->Hdrs.horizontal_size;
          dec_cont->StrmStorage.max_frame_height =
            dec_cont->Hdrs.vertical_size;
          dec_cont->StrmStorage.max_mbs_per_frame =
            ((dec_cont->StrmStorage.max_frame_width +15)>>4)*
            ((dec_cont->StrmStorage.max_frame_height+15)>>4);
        }

        if(!dec_cont->StrmStorage.direct_mvs.bus_address)
        {
          APITRACEDEBUG("%s","Allocate buffers\n");
          internal_ret = rvAllocateBuffers(dec_cont);
          if(internal_ret != DEC_OK) {
            APITRACEERR("%s","ALLOC BUFFER FAIL\n");
            APITRACEERR("%s","RvDecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = DEC_WAITING_FOR_BUFFER;
        }
      }

      if (dec_cont->realloc_int_buf) {

        rvFreeBuffers(dec_cont);

        if( dec_cont->StrmStorage.max_frame_width == 0 ) {
          dec_cont->StrmStorage.max_frame_width =
            dec_cont->Hdrs.horizontal_size;
          dec_cont->StrmStorage.max_frame_height =
            dec_cont->Hdrs.vertical_size;
          dec_cont->StrmStorage.max_mbs_per_frame =
            ((dec_cont->StrmStorage.max_frame_width +15)>>4)*
            ((dec_cont->StrmStorage.max_frame_height+15)>>4);
        }

        if(!dec_cont->StrmStorage.direct_mvs.bus_address)
        {
          APITRACEDEBUG("%s","Allocate buffers\n");
          internal_ret = rvAllocateBuffers(dec_cont);
          if(internal_ret != DEC_OK) {
            APITRACEERR("%s","ALLOC BUFFER FAIL\n");
            APITRACEERR("%s","RvDecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    }
  }

  /*
   *  Update stream structure
   */
  p_strm_desc->p_strm_buff_start = input->stream;
  p_strm_desc->strm_curr_pos = input->stream;
  p_strm_desc->bit_pos_in_word = 0;
  p_strm_desc->strm_buff_size = input->data_len;
  p_strm_desc->strm_buff_read_bits = 0;

  dec_cont->StrmStorage.num_slices = input->slice_info_num + 1;
  /* Limit maximum n:o of slices
   * (TODO, should we report an error?) */
  if(dec_cont->StrmStorage.num_slices > RV_DEC_X170_MAX_NUM_SLICES)
    dec_cont->StrmStorage.num_slices = RV_DEC_X170_MAX_NUM_SLICES;
  slice_info = dec_cont->StrmStorage.slices.virtual_address;

#ifdef RV_RAW_STREAM_SUPPORT
  dec_cont->StrmStorage.raw_mode = input->slice_info_num == 0;
#endif

  /* convert slice offsets into slice sizes, TODO: check if memory given by application is writable */
  if (p_api_stor->DecStat == STREAMDECODING
#ifdef RV_RAW_STREAM_SUPPORT
      && !dec_cont->StrmStorage.raw_mode
#endif
     ) {
    /* Copy offsets to HW external memory */
    for( i = 0 ; i < input->slice_info_num ; ++i ) {
      i32 tmp;
      if( i == input->slice_info_num-1 )
        tmp = input->data_len;
      else
        tmp = input->slice_info[i+1].offset;
      slice_info[i] = tmp - input->slice_info[i].offset;
      if(!input->slice_info[i].is_valid) {
        contains_invalid_slice = HANTRO_TRUE;
      }
    }
    DWLDMATransData(dec_cont->dwl, &dec_cont->StrmStorage.slices, 0,
                    dec_cont->StrmStorage.slices.size, HOST_TO_DEVICE);
  }

#ifdef _DEC_PP_USAGE
  dec_cont->StrmStorage.latest_id = input->pic_id;
#endif

  if(contains_invalid_slice) {
    /* If stream contains even one invalid slice, discard */
    APITRACEERR("%s","STREAM ERROR; LEAST ONE SLICE BROKEN\n");
    RVFLUSH;
    dec_cont->FrameDesc.pic_coding_type = RV_P_PIC;
    error_detected = HANTRO_TRUE;
    RVDEC_UPDATE_POUTPUT;
    ret = DEC_STRM_PROCESSED;
  } else { /* All slices OK */
    /* TODO: do we need loop? (maybe if many slices?) */
    do {
      APITRACEDEBUG("%s","Start Decode\n");
      /* run SW if HW is not in the middle of processing a picture
       * (indicated as HW_PIC_STARTED decoder status) */
      if(p_api_stor->DecStat == HEADERSDECODED) {
        p_api_stor->DecStat = STREAMDECODING;
        if (dec_cont->realloc_ext_buf) {
          RvSetExternalBufferInfo(dec_cont);
          FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
          dec_cont->buffer_index = 0;
          ret = DEC_WAITING_FOR_BUFFER;
        }
      } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
        ret =  DEC_WAITING_FOR_BUFFER;
      } else if(p_api_stor->DecStat != HW_PIC_STARTED) {
        strm_dec_result = rv_StrmDecode(dec_cont);
        switch (strm_dec_result) {
        case RV_PIC_HDR_RDY:
          /* if type inter predicted and no reference -> error */
          if((dec_cont->FrameDesc.pic_coding_type == RV_P_PIC &&
              dec_cont->StrmStorage.work0 == INVALID_ANCHOR_PICTURE) ||
              (dec_cont->FrameDesc.pic_coding_type == RV_B_PIC &&
               (dec_cont->StrmStorage.work1 == INVALID_ANCHOR_PICTURE ||
                dec_cont->StrmStorage.work0 == INVALID_ANCHOR_PICTURE ||
                dec_cont->StrmStorage.skip_b ||
                input->skip_frame == DEC_SKIP_NON_REF)) ||
              (dec_cont->FrameDesc.pic_coding_type == RV_P_PIC &&
               dec_cont->StrmStorage.picture_broken &&
               dec_cont->StrmStorage.intra_freeze)) {
            error_detected = HANTRO_TRUE;
          } else {
            p_api_stor->DecStat = HW_PIC_STARTED;
          }
          break;

        case RV_PIC_HDR_RDY_ERROR:
          error_detected = HANTRO_TRUE;
          /* copy output parameters for this PIC */
          RVDEC_UPDATE_POUTPUT;
          break;

        case RV_PIC_HDR_RDY_RPR:
          dec_cont->StrmStorage.strm_dec_ready = FALSE;
          p_api_stor->DecStat = STREAMDECODING;

          ret = DEC_STRM_PROCESSED;
          break;

        case RV_HDRS_RDY:
          {
            /* check for minimum and maximum dimensions */
            SwAdjustCoreMaskByWxH(dec_cont->dwl, dec_cont->FrameDesc.frame_width << 4,
                                  dec_cont->FrameDesc.frame_height<< 4, 1,
                                  DWL_CLIENT_TYPE_RV_DEC, &dec_cont->core_mask);
            if (CORE_MASK(dec_cont->core_mask) == 0) {
              APITRACEERR("%s","Mpeg2DecDecode# ERROR: Unsupported size\n");
              return DEC_UNSUPPORTED;
            }
            hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_RV_DEC);
          }
          internal_ret = rvDecCheckSupport(dec_cont);
          if(internal_ret != DEC_OK) {
            dec_cont->StrmStorage.strm_dec_ready = FALSE;
            p_api_stor->DecStat = INITIALIZED;
            return internal_ret;
          }

          dec_cont->ApiStorage.first_headers = 0;

          SetDecRegister(dec_cont->rv_regs, HWIF_PIC_WIDTH_IN_CBS,
                          dec_cont->FrameDesc.frame_width << 1);
          SetDecRegister(dec_cont->rv_regs, HWIF_PIC_HEIGHT_IN_CBS,
                          dec_cont->FrameDesc.frame_height << 1);


          p_api_stor->DecStat = HEADERSDECODED;
          if (dec_cont->pp_enabled) {
            dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
            dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
          }
          FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
          BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
          APITRACEDEBUG("%s","HDRS_RDY\n");
          ret = DEC_HDRS_RDY;
          output->data_left = input->data_len;
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          if(dec_cont->StrmStorage.rpr_detected)
            ret = DEC_STRM_PROCESSED;
          return ret;

        default:
          output->data_left = 0;
          output->strm_curr_pos = (u8 *)input->stream + input->data_len;
          output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
          //ASSERT(strm_dec_result == RV_END_OF_STREAM);
          if(dec_cont->StrmStorage.rpr_detected) {
            ret = DEC_PIC_DECODED;
          } else {
            ret = DEC_STRM_PROCESSED;
          }
          return ret;
        }
      }

      /* picture header properly decoded etc -> start HW */
      if(p_api_stor->DecStat == HW_PIC_STARTED) {

        if (dec_cont->seq_state != SEQ_CLEAN &&
            dec_cont->FrameDesc.pic_coding_type != RV_I_PIC) {
          if (EC_STATE_DISCARD == rvECGetInDataAction(dec_cont->error_policy)) {
            output->data_left = 0;
            output->strm_curr_pos = (u8 *)input->stream + input->data_len;
            output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
            return DEC_STRM_PROCESSED;
          }
        }

        if (!dec_cont->asic_running) {
          dec_cont->StrmStorage.work_out =
            BqueueNext2( &dec_cont->StrmStorage.bq,
                         dec_cont->StrmStorage.work0,
                         dec_cont->StrmStorage.work1,
                         BQUEUE_UNUSED,
                         dec_cont->FrameDesc.pic_coding_type == RV_B_PIC );
          if(dec_cont->StrmStorage.work_out == INVALID_ANCHOR_PICTURE) {
            if (dec_cont->abort)
              return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
            else {
              output->strm_curr_pos = input->stream;
              output->strm_curr_bus_address = input->stream_bus_address;
              output->data_left = input->data_len;
              p_api_stor->DecStat = STREAMDECODING;
              dec_cont->same_slice_header = 1;
              return DEC_NO_DECODING_BUFFER;
            }
#endif
          }
          else if (dec_cont->same_slice_header)
            dec_cont->same_slice_header = 0;

          dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].first_show = 1;

          if (dec_cont->pp_enabled) {
            struct DWLLinearMem *pp_buffer = NULL;
#ifdef GET_FREE_BUFFER_NON_BLOCK
            pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
#else
            pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
#endif
            dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data = pp_buffer;
            if (pp_buffer == NULL) {
              if (dec_cont->abort) {
                return DEC_ABORTED;
              } else {
                dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].first_show = 0;
                BqueuePictureRelease(&dec_cont->StrmStorage.bq, dec_cont->StrmStorage.work_out);
                output->strm_curr_pos = input->stream;
                output->strm_curr_bus_address = input->stream_bus_address;
                output->data_left = input->data_len;
                p_api_stor->DecStat = STREAMDECODING;
                dec_cont->same_slice_header = 1;
                return DEC_NO_DECODING_BUFFER;
              }
            }
#ifdef ENABLE_FPGA_VERIFICATION
            /* update device buffer by host buffer */
            DWLLinearMemset(dec_cont->dwl, pp_buffer, 0, 0, pp_buffer->size);
#endif
            InputQueueSetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer, PP_OUT_CTRL(input->dec_ctrl));
          }

          if (dec_cont->StrmStorage.partial_freeze) {
            PreparePartialFreeze(dec_cont->dwl,
                                 &dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].data,
                                 dec_cont->FrameDesc.frame_width,
                                 dec_cont->FrameDesc.frame_height);
          }
        }

        DWLDMATransData2(dec_cont->dwl, (addr_t)input->stream_bus_address,
                        (void *)input->stream,
                        input->data_len, HOST_TO_DEVICE);

        asic_status = RunDecoderAsic(dec_cont, input->stream_bus_address, hw_feature);

        rvECMarkOutDataErrInfo(dec_cont, asic_status,
                               dec_cont->FrameDesc.pic_coding_type == RV_I_PIC);

        if(asic_status == ID8170_DEC_TIMEOUT) {
          ret = DEC_HW_TIMEOUT;
        } else if(asic_status == ID8170_DEC_SYSTEM_ERROR) {
          ret = DEC_SYSTEM_ERROR;
        } else if(asic_status == ID8170_DEC_HW_RESERVED) {
          ret = DEC_HW_RESERVED;
        } else if(asic_status & DEC_HW_IRQ_BUS) {
          ret = DEC_HW_BUS_ERROR;
        } else if( (asic_status & DEC_HW_IRQ_ERROR) ||
                   (asic_status & DEC_HW_IRQ_TIMEOUT) ||
                   (asic_status & DEC_HW_IRQ_ABORT)) {
          if(asic_status & DEC_HW_IRQ_TIMEOUT ||
              asic_status & DEC_HW_IRQ_ABORT) {
            APITRACEERR("%s","IRQ TIMEOUT IN HW\n");
          } else {
            APITRACEERR("%s","STREAM ERROR IN HW\n");
            RVFLUSH;
          }

          u32 is_update_recon = 0;
          u32 error_ratio = getG1OutDataErrorRatio(dec_cont->Hdrs.horizontal_size, dec_cont->Hdrs.vertical_size,
                                  GetDecRegister(dec_cont->rv_regs, HWIF_MB_LOCATION_X),
                                  GetDecRegister(dec_cont->rv_regs, HWIF_MB_LOCATION_Y));

          dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].error_ratio = error_ratio;
          if (EC_STATE_DISCARD == rvECGetOutDataAction(dec_cont, error_ratio)) {
            if (dec_cont->pp_enabled) {
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue,
                  DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data)));
            }
            ret = DEC_DISCARD_INTERNAL;
            is_update_recon = 0;
          } else {
            dec_cont->FrameDesc.frame_number++;
            rvHandleFrameEnd(dec_cont);
            rvDecBufferPicture(dec_cont,
                              input->pic_id,
                              dec_cont->FrameDesc.
                              pic_coding_type == RV_B_PIC,
                              dec_cont->FrameDesc.
                              pic_coding_type == RV_P_PIC,
                              DEC_PIC_DECODED, 0);

            ret = DEC_PIC_DECODED;
            rvDecPreparePicReturn(dec_cont);
            is_update_recon = 1;
          }
          error_detected = HANTRO_TRUE;
          rvECHandleReconData(dec_cont, is_update_recon);
          RVDEC_UPDATE_POUTPUT;
        } else if(asic_status & DEC_HW_IRQ_BUFFER) {
          rvDecPreparePicReturn(dec_cont);
#ifdef CASE_INFO_STAT
          if(case_info.frame_num < 10)
            case_info.slice_num[case_info.frame_num]++;
#endif
          ret = DEC_BUF_EMPTY;
        } else if(asic_status & DEC_HW_IRQ_RDY) {
        } else {
          ASSERT(0);
        }

        /* HW finished decoding a picture */
        if(asic_status & DEC_HW_IRQ_RDY) {
          dec_cont->FrameDesc.frame_number++;

          rvHandleFrameEnd(dec_cont);

          rvDecBufferPicture(dec_cont,
                             input->pic_id,
                             dec_cont->FrameDesc.
                             pic_coding_type == RV_B_PIC,
                             dec_cont->FrameDesc.
                             pic_coding_type == RV_P_PIC,
                             DEC_PIC_DECODED, 0);

          ret = DEC_PIC_DECODED;

#ifdef CASE_INFO_STAT
          RvCaseInfoCollect(dec_cont, &case_info);
#endif

#ifdef FPGA_PERF_AND_BW
          DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info,
                          (dec_cont->FrameDesc.frame_width << 4) *
                          (dec_cont->FrameDesc.frame_height << 4),
                          8);
#endif
          if(dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
            dec_cont->StrmStorage.work1 =
              dec_cont->StrmStorage.work0;
            dec_cont->StrmStorage.work0 =
              dec_cont->StrmStorage.work_out;
            if(dec_cont->StrmStorage.skip_b)
              dec_cont->StrmStorage.skip_b--;
          }

          if(dec_cont->FrameDesc.pic_coding_type == RV_I_PIC) {
            dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;
            dec_cont->seq_state = SEQ_CLEAN;
          }

          rvDecPreparePicReturn(dec_cont);
        }

        if(ret != DEC_STRM_PROCESSED && ret != DEC_BUF_EMPTY) {
          p_api_stor->DecStat = STREAMDECODING;
        }

        if(ret == DEC_PIC_DECODED || ret == DEC_STRM_PROCESSED || ret == DEC_BUF_EMPTY) {
          /* copy output parameters for this PIC (excluding stream pos) */
          dec_cont->MbSetDesc.out_data.strm_curr_pos =
            output->strm_curr_pos;
          RVDEC_UPDATE_POUTPUT;
        }
      }
    } while(ret == 0);
  }

  if(error_detected && dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
    dec_cont->StrmStorage.picture_broken = HANTRO_TRUE;
    dec_cont->seq_state = SEQ_DIRTY;
  }

  APITRACE("%s","RvDecDecode: Exit\n");
  if(!dec_cont->StrmStorage.rpr_detected) {
    output->strm_curr_pos = dec_cont->StrmDesc.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start);
    output->data_left = dec_cont->StrmDesc.strm_buff_size -
                        (output->strm_curr_pos - p_strm_desc->p_strm_buff_start);
  } else {
    output->strm_curr_pos = input->stream;
    output->strm_curr_bus_address = input->stream_bus_address;
    output->data_left = input->data_len;
    ret = DEC_STRM_PROCESSED;
  }

  dec_cont->output_stat = RvDecNextPicture_INTERNAL(dec_cont, &dec_cont->out_pic, 0);
  if(dec_cont->output_stat == DEC_ABORTED)
    return (DEC_ABORTED);

  if(dec_cont->abort)
    return(DEC_ABORTED);
  else
    return ((enum DecRet) ret);
}

/*------------------------------------------------------------------------------

    Function: RvDecRelease()

        Functional description:
            Release the decoder instance.

        Inputs:
            dec_inst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void RvDecRelease(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = NULL;

  APITRACEDEBUG("%s","1\n");
  APITRACE("%s","RvDecRelease#\n");
  if(dec_inst == NULL) {
    APITRACEERR("%s","RvDecRelease# ERROR: dec_inst == NUL\nL");
    return;
  }

  dec_cont = ((RvDecContainer *) dec_inst);

  /* Wait all buffers as unused */
  BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);

  pthread_mutex_destroy(&dec_cont->protect_mutex);

  if (dec_cont->asic_running)
    (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);

  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  rvFreeBuffers(dec_cont);

  if (dec_cont->StrmStorage.slices.virtual_address != NULL)
    DWLFreeLinear(dec_cont->dwl, &dec_cont->StrmStorage.slices);

  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

#ifdef FPGA_PERF_AND_BW
  AveragePerfInfoPrint(&dec_cont->perf_info);
#endif
  DWLfree(dec_cont);

  APITRACE("%s","RvDecRelease: OK\n");
}


/*------------------------------------------------------------------------------
    Function name   : rvRefreshRegs
    Description     :
    Return type     : void
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
void rvRefreshRegs(RvDecContainer * dec_cont) {
  i32 i;
  u32 *dec_regs = dec_cont->rv_regs;

  if(dec_cont->vcmd_used) {
       DWLRefreshRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->rv_regs);
  } else {
    for(i = 0; i < DEC_X170_REGISTERS; i++) {
      dec_regs[i] = DWLReadReg(dec_cont->dwl, dec_cont->core_id, 4 * i);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : rvFlushRegs
    Description     :
    Return type     : void
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
void rvFlushRegs(RvDecContainer * dec_cont) {
  i32 i;
  u32 *dec_regs = dec_cont->rv_regs;

  if (dec_cont->vcmd_used) {
    DWLFlushRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->rv_regs,
                     dec_cont->mc_refresh_regs[dec_cont->core_id], dec_cont->core_id);
  }else {
    for(i = 2; i < DEC_X170_REGISTERS; i++) {
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * i, dec_regs[i]);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : rvHandleVlcModeError
    Description     :
    Return type     : u32
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
#if !(defined(_MSC_VER) || defined(_WIN32))
__attribute__ ((unused))
#endif
u32 rvHandleVlcModeError(RvDecContainer * dec_cont, u32 pic_num) {
  u32 ret = DEC_STRM_PROCESSED;
  ASSERT(dec_cont->StrmStorage.strm_dec_ready);

  /*
  tmp = rvStrmDec_NextStartCode(dec_cont);
  if(tmp != END_OF_STREAM)
  {
      dec_cont->StrmDesc.strm_curr_pos -= 4;
      dec_cont->StrmDesc.strm_buff_read_bits -= 32;
  }
  */

  /* error in first picture -> set reference to grey */
  if(!dec_cont->FrameDesc.frame_number) {
    u32 out_w, out_h, size;
    out_w = NEXT_MULTIPLE(4 * dec_cont->FrameDesc.frame_width * 16, ALIGN(dec_cont->align));
    out_h = dec_cont->FrameDesc.frame_height * 4;
    size = out_w * out_h * 3 / 2;
    (void) DWLmemset(dec_cont->StrmStorage.
                    p_pic_buf[dec_cont->StrmStorage.work_out].data.
                    virtual_address, 128, size);
    rvDecPreparePicReturn(dec_cont);

    /* no pictures finished -> return STRM_PROCESSED */
    ret = DEC_STRM_PROCESSED;
    dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work_out;
    dec_cont->StrmStorage.skip_b = 2;
  } else {
    if(dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
      dec_cont->FrameDesc.frame_number++;

      BqueueDiscard(&dec_cont->StrmStorage.bq,
                    dec_cont->StrmStorage.work_out );
      dec_cont->StrmStorage.work_out = dec_cont->StrmStorage.work0;

      rvDecBufferPicture(dec_cont,
                         pic_num,
                         dec_cont->FrameDesc.pic_coding_type == RV_B_PIC,
                         1, (enum DecRet) FREEZED_PIC_RDY,
                         dec_cont->FrameDesc.total_mb_in_frame);

      ret = DEC_PIC_DECODED;

      dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
      dec_cont->StrmStorage.skip_b = 2;
    } else {
      if(dec_cont->StrmStorage.intra_freeze) {
        dec_cont->FrameDesc.frame_number++;
        rvDecBufferPicture(dec_cont,
                           pic_num,
                           dec_cont->FrameDesc.pic_coding_type ==
                           RV_B_PIC, 1, (enum DecRet) FREEZED_PIC_RDY,
                           dec_cont->FrameDesc.total_mb_in_frame);

        ret = DEC_PIC_DECODED;

      } else {
        ret = DEC_NONREF_PIC_SKIPPED;
      }
    }
  }

  dec_cont->ApiStorage.DecStat = STREAMDECODING;

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : rvHandleFrameEnd
    Description     :
    Return type     : u32
    Argument        : RvDecContainer *dec_cont
------------------------------------------------------------------------------*/
void rvHandleFrameEnd(RvDecContainer * dec_cont) {

  dec_cont->StrmDesc.strm_buff_read_bits =
    8 * (dec_cont->StrmDesc.strm_curr_pos -
         dec_cont->StrmDesc.p_strm_buff_start);
  dec_cont->StrmDesc.bit_pos_in_word = 0;

}

/*------------------------------------------------------------------------------

         Function name: RunDecoderAsic

         Purpose:       Set Asic run lenght and run Asic

         Input:         RvDecContainer *dec_cont

         Output:        void

------------------------------------------------------------------------------*/
u32 RunDecoderAsic(RvDecContainer * dec_cont, addr_t strm_bus_address,
                   const struct DecHwFeatures *hw_feature) {
  i32 ret;
  addr_t tmp = 0;
  u32 asic_status = 0;
  addr_t mask;
  u32 irq = 0;
  struct DWLReqInfo info = {0};

  ASSERT(dec_cont->StrmStorage.
         p_pic_buf[dec_cont->StrmStorage.work_out].data.bus_address != 0);
  ASSERT(strm_bus_address != 0);

  mask = 15;

  if(!dec_cont->asic_running) {
    u32 reserve_ret = 0;
    tmp = RvSetRegs(dec_cont, strm_bus_address, hw_feature);
    if(tmp == HANTRO_NOK)
      return 0;
    info.core_mask = dec_cont->core_mask;
    info.width = dec_cont->FrameDesc.frame_width * 16;
    info.height = dec_cont->FrameDesc.frame_height * 16;
    info.owner = (void *)dec_cont;
    if (dec_cont->vcmd_used) {
      dec_cont->core_id = 0;
      reserve_ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_cont->cmdbuf_id);
      UNUSED(reserve_ret);
    } else {
      (void) DWLReserveHw(dec_cont->dwl, &info, &dec_cont->core_id);
    }

    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_DIS, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_FILTERING_DIS, 1);

    dec_cont->asic_running = 1;

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x4, 0);

    rvFlushRegs(dec_cont);

    if (dec_cont->vcmd_used)
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->ppu_cfg, 0);
    else
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->core_id, dec_cont->ppu_cfg, 0);

    /* Enable HW */
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 1);
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->rv_regs[1]);
  } else { /* in the middle of decoding, continue decoding */
    /* tmp is strm_bus_address + number of bytes decoded by SW */
    /* TODO: alotetaanko aina bufferin alusta? */
    tmp = dec_cont->StrmDesc.strm_curr_pos -
          dec_cont->StrmDesc.p_strm_buff_start;
    tmp = strm_bus_address + tmp;

    /* pointer to start of the stream, mask to get the pointer to
     * previous 64/128-bit aligned position */
    if(!(tmp & ~(mask))) {
      return 0;
    }

    SET_ADDR_REG(dec_cont->rv_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));
    /* amount of stream (as seen by the HW), obtained as amount of stream
     * given by the application subtracted by number of bytes decoded by
     * SW (if strm_bus_address is not 64/128-bit aligned -> adds number of bytes
     * from previous 64/128-bit aligned boundary) */
    SetDecRegister(dec_cont->rv_regs, HWIF_STREAM_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));

    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_BUFFER_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));
    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_OFFSET, 0);

    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

    /* This depends on actual register allocation */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->rv_regs[5]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->rv_regs[6]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->rv_regs[258]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->rv_regs[259]);
    if (IS_LEGACY(dec_cont->rv_regs[0]))
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 12, dec_cont->rv_regs[12]);
    else
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169, dec_cont->rv_regs[169]);
    if (sizeof(addr_t) == 8) {
      if(hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->rv_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, dec_cont->rv_regs[122]);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, dec_cont->rv_regs[168]);
      } else {
        ASSERT(dec_cont->rv_regs[122] == 0);
        ASSERT(dec_cont->rv_regs[168] == 0);
      }
    } else {
      if(hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->rv_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, 0);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, 0);
      }
    }
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->rv_regs[1]);
  }

  /* Wait for HW ready */
  APITRACEDEBUG("%s","Wait for Decoder\n");
  if (dec_cont->vcmd_used)
    ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
  else
    ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id, (u32) DEC_X170_TIMEOUT_LENGTH);

  rvRefreshRegs(dec_cont);

  if(ret == DWL_HW_WAIT_OK) {
    asic_status =
      GetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT);
  } else if(ret == DWL_HW_WAIT_TIMEOUT) {
    asic_status = ID8170_DEC_TIMEOUT;
  } else {
    asic_status = ID8170_DEC_SYSTEM_ERROR;
  }

  if(!(asic_status & DEC_HW_IRQ_BUFFER) ||
      (asic_status & DEC_HW_IRQ_ERROR) ||
      (asic_status & DEC_HW_IRQ_BUS) ||
      (asic_status == ID8170_DEC_TIMEOUT) ||
      (asic_status == ID8170_DEC_SYSTEM_ERROR)) {
    /* reset HW */
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 0);

    dec_cont->asic_running = 0;
    if (dec_cont->vcmd_used) {
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      UNUSED(irq);
    } else {
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->rv_regs[1]);
      (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    }
  }

  /* if HW interrupt indicated either BUFFER_EMPTY or
   * RV_RDY -> read stream end pointer and update StrmDesc structure */
  if((asic_status &
      (DEC_HW_IRQ_BUFFER | DEC_HW_IRQ_RDY))) {
    tmp = GET_ADDR_REG(dec_cont->rv_regs, HWIF_RLC_VLC_BASE);

    if((tmp - strm_bus_address) <= dec_cont->max_strm_len) {
      dec_cont->StrmDesc.strm_curr_pos =
        dec_cont->StrmDesc.p_strm_buff_start + (tmp - strm_bus_address);
    } else {
      dec_cont->StrmDesc.strm_curr_pos =
        dec_cont->StrmDesc.p_strm_buff_start +
        dec_cont->StrmDesc.strm_buff_size;
    }

    dec_cont->StrmDesc.strm_buff_read_bits =
      8 * (dec_cont->StrmDesc.strm_curr_pos -
           dec_cont->StrmDesc.p_strm_buff_start);
    dec_cont->StrmDesc.bit_pos_in_word = 0;
  }

  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);

  return asic_status;

}

/*------------------------------------------------------------------------------

    Function name: RvDecNextPicture

    Functional description:
        Retrieve next decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK         No picture available.
        DEC_PIC_RDY    Picture ready.

------------------------------------------------------------------------------*/
enum DecRet RvDecNextPicture(RvDecInst dec_inst, RvDecPicture *picture) {
  /* Variables */
  enum DecRet return_value = DEC_PIC_RDY;
  RvDecContainer *dec_cont;
  i32 ret;

  /* Code */
  //APITRACE("%s","Rv_dec_next_picture#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","RvDecNextPicture# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecNextPicture# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  addr_t i;
  if ((ret = FifoPop(dec_cont->fifo_display, (FifoObject *)&i,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
          FIFO_EXCEPTION_ENABLE
#else
          FIFO_EXCEPTION_DISABLE
#endif
          )) != FIFO_ABORT) {

#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
    if (ret == FIFO_EMPTY) return DEC_OK;
#endif

    if ((i32)i == -1) {
      APITRACE("%s","RvDecNextPicture# DEC_END_OF_STREAM\n");
      return DEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      APITRACE("%s","RvDecNextPicture# DEC_FLUSHED\n");
      return DEC_FLUSHED;
    }

    *picture = dec_cont->StrmStorage.picture_info[i];
    ECErrorInfoReturn(picture->error_info, picture->error_ratio, picture->pic_id);
    APITRACE("%s","RvDecNextPicture# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else
    return DEC_ABORTED;

  return return_value;
}

/*------------------------------------------------------------------------------

    Function name: RvDecNextPicture_INTERNAL

    Functional description:
        Push next picture in display order into output fifo if any available.

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK                No picture available.
        DEC_PIC_RDY           Picture ready.
        rvDEC_PARAM_ERROR       invalid parameters.
        DEC_NOT_INITIALIZED   decoder instance not initialized yet.

------------------------------------------------------------------------------*/
enum DecRet RvDecNextPicture_INTERNAL(RvDecInst dec_inst,
                                   RvDecPicture * picture, u32 end_of_stream) {
  /* Variables */
  enum DecRet return_value = DEC_PIC_RDY;
  RvDecContainer *dec_cont;
  u32 pic_index = RV_BUFFER_UNDEFINED;
  u32 min_count;
  static u32 pic_count = 0;

  /* Code */
  APITRACE("%s","Rv_dec_next_picture_INTERNAL#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","RvDecNextPicture_INTERNAL# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecNextPicture_INTERNAL# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  min_count = 0;
  (void) DWLmemset(picture, 0, sizeof(RvDecPicture));
  if(!end_of_stream && !dec_cont->StrmStorage.rpr_detected)
    min_count = 1;

  /* this is to prevent post-processing of non-finished pictures in the
   * end of the stream */
  if(end_of_stream && dec_cont->FrameDesc.pic_coding_type == RV_B_PIC) {
    dec_cont->FrameDesc.pic_coding_type = RV_P_PIC;
  }

  /* Nothing to send out */
  if(dec_cont->StrmStorage.out_count <= min_count) {
    (void) DWLmemset(picture, 0, sizeof(RvDecPicture));
    picture->pictures[0].output_picture = NULL;
    return_value = DEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.out_index;
    pic_index = dec_cont->StrmStorage.out_buf[pic_index];

    RvFillPicStruct(picture, dec_cont, pic_index);

    pic_count++;

    dec_cont->StrmStorage.out_count--;
    dec_cont->StrmStorage.out_index++;
    dec_cont->StrmStorage.out_index &= 0xF;

#ifdef USE_PICTURE_DISCARD
    if (dec_cont->StrmStorage.p_pic_buf[pic_index].first_show)
#endif
    {
      /* wait this buffer as unused */
      if (BqueueWaitBufNotInUse(&dec_cont->StrmStorage.bq, pic_index) != HANTRO_OK)
        return DEC_ABORTED;

      if(dec_cont->pp_enabled) {
        InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data)));
      }

      dec_cont->StrmStorage.p_pic_buf[pic_index].first_show = 0;

      /* set this buffer as used */
      BqueueSetBufferAsUsed(&dec_cont->StrmStorage.bq, pic_index);

      if(dec_cont->pp_enabled)
        InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data)));

      dec_cont->StrmStorage.picture_info[dec_cont->fifo_index] = *picture;
      FifoPush(dec_cont->fifo_display, (FifoObject)(addr_t)dec_cont->fifo_index, FIFO_EXCEPTION_DISABLE);
      dec_cont->fifo_index++;
      if(dec_cont->fifo_index == 32)
        dec_cont->fifo_index = 0;
      if (dec_cont->pp_enabled) {
        BqueuePictureRelease(&dec_cont->StrmStorage.bq, pic_index);
      }
    }
  }

  return return_value;
}

/*------------------------------------------------------------------------------

    Function name: RvDecPictureConsumed

    Functional description:
        release specific decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to  picture struct


    Return values:
        DEC_PARAM_ERROR         Decoder instance or picture is null
        DEC_NOT_INITIALIZED     Decoder instance isn't initialized
        DEC_OK                          picture release success
------------------------------------------------------------------------------*/
enum DecRet RvDecPictureConsumed(RvDecInst dec_inst, RvDecPicture * picture) {
  /* Variables */
  RvDecContainer *dec_cont;
  u32 i;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */
  APITRACE("%s","Rv_dec_picture_consumed#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","RvDecPictureConsumed# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers; i++) {
      if(picture->pictures[0].output_picture_bus_address
          == dec_cont->StrmStorage.p_pic_buf[i].data.bus_address) {
        BqueuePictureRelease(&dec_cont->StrmStorage.bq, i);
        return (DEC_OK);
      }
    }
  } else {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (DWLMemAddr)picture->pictures[i].output_picture_bus_address;
        break;
      }
    }
    InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
    return (DEC_OK);
  }
  return (DEC_PARAM_ERROR);
}


enum DecRet RvDecEndOfStream(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = (RvDecContainer *) dec_inst;

  APITRACE("%s","RvDecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecEndOfStream# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }
  if(dec_cont->dec_stat == DEC_END_OF_STREAM) {
    return (DEC_OK);
  }

  if (dec_cont->vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl, dec_inst);
  } else {
    if(dec_cont->asic_running) {
      /* stop HW */
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->rv_regs, HWIF_DEC_E, 0);
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->rv_regs[1] | DEC_IRQ_DISABLE);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
      dec_cont->asic_running = 0;
    }
  }

  dec_cont->output_stat = RvDecNextPicture_INTERNAL(dec_cont, &dec_cont->out_pic, 1);
  if(dec_cont->output_stat == DEC_ABORTED) {
    return (DEC_ABORTED);
  }

  dec_cont->dec_stat = DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);

  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;

  APITRACE("%s","RvDecEndOfStream# DEC_OK\n");
  return DEC_OK;
}

/*----------------------=-------------------------------------------------------

    Function name: RvFillPicStruct

    Functional description:
        Fill data to output pic description

    Input:
        dec_cont    Decoder container
        picture    Pointer to return value struct

    Return values:
        void

------------------------------------------------------------------------------*/
void RvFillPicStruct(RvDecPicture * picture,
                     RvDecContainer * dec_cont, u32 pic_index) {
  picture_t *p_pic;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;

  p_pic = (picture_t *) dec_cont->StrmStorage.p_pic_buf;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = p_pic[pic_index].frame_width;
    picture->pictures[0].frame_height = p_pic[pic_index].frame_height;
    picture->pictures[0].coded_width = p_pic[pic_index].coded_width;
    picture->pictures[0].coded_height = p_pic[pic_index].coded_height;
    picture->pictures[0].pic_stride = NEXT_MULTIPLE(picture->pictures[0].frame_width * 4,
                                                    ALIGN(dec_cont->align));

    picture->pictures[0].pic_stride_ch = picture->pictures[0].pic_stride;
    picture->pictures[0].output_picture = (u8 *) p_pic[pic_index].data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic[pic_index].data.bus_address;
    picture->pictures[0].output_picture_chroma = (u8 *) p_pic[pic_index].data.virtual_address +
        picture->pictures[0].pic_stride * picture->pictures[0].coded_height / 4;
    picture->pictures[0].output_picture_chroma_bus_address = p_pic[pic_index].data.bus_address +
        picture->pictures[0].pic_stride * picture->pictures[0].coded_height / 4;
    picture->pictures[0].output_format = DEC_OUT_FRM_YUV420TILE;
    picture->pictures[0].crop_params.crop_left_offset = dec_cont->ppu_cfg[0].crop.x;
    picture->pictures[0].crop_params.crop_top_offset = dec_cont->ppu_cfg[0].crop.y;
    if (dec_cont->ppu_cfg[0].crop.width == 0)
      picture->pictures[0].crop_params.crop_out_width = picture->pictures[0].coded_width;
    else
      picture->pictures[0].crop_params.crop_out_width = dec_cont->ppu_cfg[0].crop.width;
    if (dec_cont->ppu_cfg[0].crop.height == 0)
      picture->pictures[0].crop_params.crop_out_height = picture->pictures[0].coded_height;
    else
      picture->pictures[0].crop_params.crop_out_height = dec_cont->ppu_cfg[0].crop.height;
  } else {
    u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, p_pic[pic_index].pp_data);
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
      picture->pictures[i].frame_width = dec_cont->ppu_cfg[i].scale.width;
      picture->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
      picture->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
      picture->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
      picture->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
      picture->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
      picture->pictures[i].output_picture = (u8 *) ((addr_t)p_pic[pic_index].pp_data->virtual_address + ppu_cfg->luma_offset);
      picture->pictures[i].output_picture_bus_address = p_pic[pic_index].pp_data->bus_address + ppu_cfg->luma_offset;
      picture->pictures[i].output_picture_chroma = (u8 *) ((addr_t)p_pic[pic_index].pp_data->virtual_address + ppu_cfg->chroma_offset);
      picture->pictures[i].output_picture_chroma_bus_address = p_pic[pic_index].pp_data->bus_address + ppu_cfg->chroma_offset;
      picture->pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      picture->pictures[i].crop_params.crop_left_offset = dec_cont->ppu_cfg[i].crop.x;
      picture->pictures[i].crop_params.crop_top_offset = dec_cont->ppu_cfg[i].crop.y;
      if (dec_cont->ppu_cfg[i].crop.width == 0)
        picture->pictures[i].crop_params.crop_out_width = dec_cont->ppu_cfg[i].scale.width;
      else
        picture->pictures[i].crop_params.crop_out_width = dec_cont->ppu_cfg[i].crop.width;
      if (dec_cont->ppu_cfg[i].crop.height == 0)
        picture->pictures[i].crop_params.crop_out_height = dec_cont->ppu_cfg[i].scale.height;
      else
        picture->pictures[i].crop_params.crop_out_height = dec_cont->ppu_cfg[i].crop.height;
    }

    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !ppu_cfg->dec400_enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
      PpFillDec400TblInfo(ppu_cfg,
                          p_pic[pic_index].pp_data->virtual_address,
                          p_pic[pic_index].pp_data->bus_address,
                          &picture->pictures[i].dec400_luma_table,
                          &picture->pictures[i].dec400_chroma_table);
    }
  }
  picture->key_picture = p_pic[pic_index].pic_type;
  picture->pic_id = p_pic[pic_index].decode_id;
  picture->decode_id = p_pic[pic_index].decode_id;
  picture->pic_coding_type = p_pic[pic_index].pic_code_type;
  picture->number_of_err_mbs = p_pic[pic_index].nbr_err_mbs;
  picture->cycles_per_mb = RvCycleCount(dec_cont);
  picture->error_ratio = p_pic[pic_index].error_ratio;
  picture->error_info = p_pic[pic_index].error_info;
}

/*------------------------------------------------------------------------------

    Function name: RvSetRegs

    Functional description:
        Set registers

    Input:
        container

    Return values:
        void

------------------------------------------------------------------------------*/
u32 RvSetRegs(RvDecContainer * dec_cont, addr_t strm_bus_address,
              const struct DecHwFeatures *hw_feature) {
  addr_t tmp = 0;
  u32 tmp_fwd;
  addr_t mask;

#ifdef _DEC_PP_USAGE
  RvDecPpUsagePrint(dec_cont, DECPP_UNSPECIFIED,
                    dec_cont->StrmStorage.work_out, 1,
                    dec_cont->StrmStorage.latest_id);
#endif

  APITRACEDEBUG("Decoding to index %d \n",
                   dec_cont->StrmStorage.work_out);


  mask = 15;


  /* swReg3 */
  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_INTERLACE_E, 0);
  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_FIELDMODE_E, 0);

  if(dec_cont->FrameDesc.pic_coding_type == RV_B_PIC)
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_B_E, 1);
  else
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_B_E, 0);

  SetDecRegister(dec_cont->rv_regs, HWIF_PIC_INTER_E,
                 dec_cont->FrameDesc.pic_coding_type == RV_P_PIC ||
                 dec_cont->FrameDesc.pic_coding_type == RV_B_PIC ? 1 : 0);

  SetDecRegister(dec_cont->rv_regs, HWIF_WRITE_MVS_E,
                 dec_cont->FrameDesc.pic_coding_type == RV_P_PIC);


  SetDecRegister(dec_cont->rv_regs, HWIF_INIT_QP,
                 dec_cont->FrameDesc.qp);

  SetDecRegister(dec_cont->rv_regs, HWIF_RV_FWD_SCALE,
                 dec_cont->StrmStorage.fwd_scale);
  SetDecRegister(dec_cont->rv_regs, HWIF_RV_BWD_SCALE,
                 dec_cont->StrmStorage.bwd_scale);

  /* swReg5 */

  /* tmp is strm_bus_address + number of bytes decoded by SW */
#ifdef RV_RAW_STREAM_SUPPORT
  if (dec_cont->StrmStorage.raw_mode)
    tmp = dec_cont->StrmDesc.strm_curr_pos -
          dec_cont->StrmDesc.p_strm_buff_start;
  else
#endif
    tmp = 0;

  tmp = strm_bus_address + tmp;

  /* bus address must not be zero */
  if(!(tmp & ~(mask))) {
    return 0;
  }

  /* pointer to start of the stream, mask to get the pointer to
   * previous 64-bit aligned position */
  SET_ADDR_REG(dec_cont->rv_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

  /* amount of stream (as seen by the HW), obtained as amount of
   * stream given by the application subtracted by number of bytes
   * decoded by SW (if strm_bus_address is not 64-bit aligned -> adds
   * number of bytes from previous 64-bit aligned boundary) */
  SetDecRegister(dec_cont->rv_regs, HWIF_STREAM_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));

  SetDecRegister(dec_cont->rv_regs, HWIF_STRM_BUFFER_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));
  SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_OFFSET, 0);

#ifdef RV_RAW_STREAM_SUPPORT
  if (dec_cont->StrmStorage.raw_mode)
    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));
  else
#endif
    SetDecRegister(dec_cont->rv_regs, HWIF_STRM_START_BIT, 0);

  /* swReg13 */
  SET_ADDR_REG(dec_cont->rv_regs, HWIF_DEC_OUT_BASE,
               dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].
               data.bus_address);

  SetDecRegister(dec_cont->rv_regs, HWIF_PP_OUT_E_U, dec_cont->pp_enabled);
  SetDecRegister(dec_cont->rv_regs, HWIF_UNIQUE_ID, UNIQUE_ID(0));

  if (dec_cont->pp_enabled &&
      hw_feature->max_ppu_count) {
    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    struct DWLLinearMem *pp_buffer = dec_cont->StrmStorage.p_pic_buf[dec_cont->
                              StrmStorage.work_out].pp_data;
    u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer);
    struct PpParams pp_args = {hw_feature,
                               ppu_cfg,
                               pp_buffer,
                               0,
                               0,
                               pp_out_ctrl
                              };
    PPSetRegs(dec_cont->rv_regs, &pp_args);
    /* Warning: only single core are currently supported (core_id = 0) */
    PPSetLancozsScaleRegs(dec_cont->rv_regs, hw_feature, ppu_cfg, 0);
    SetDecRegister(dec_cont->rv_regs, HWIF_PP_IN_FORMAT_U, 1);
  }

  if (dec_cont->enable_3dlut)
    DWLDMATransData(dec_cont->dwl, &dec_cont->ppu_cfg[0].table_3dlut_buffer, 0,
                    dec_cont->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);
  /* Stride registers only available since g1v8_2 */
  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_Y_STRIDE,
                  NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width * 4 * 16, ALIGN(dec_cont->align)));
  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_C_STRIDE,
                  NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width * 4 * 16, ALIGN(dec_cont->align)));


  if(dec_cont->FrameDesc.pic_coding_type == RV_B_PIC) { /* ? */
    /* past anchor set to future anchor if past is invalid (second
     * picture in sequence is B) */
    tmp_fwd =
      dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
      dec_cont->StrmStorage.work1 :
      dec_cont->StrmStorage.work0;

    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER0_BASE,
                 dec_cont->StrmStorage.p_pic_buf[tmp_fwd].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER1_BASE,
                 dec_cont->StrmStorage.p_pic_buf[tmp_fwd].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER2_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER3_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
  } else {
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER0_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
    SET_ADDR_REG(dec_cont->rv_regs, HWIF_REFER1_BASE,
                 dec_cont->StrmStorage.
                 p_pic_buf[dec_cont->StrmStorage.work0].data.
                 bus_address);
  }

  SetDecRegister(dec_cont->rv_regs, HWIF_STARTMB_X, 0);
  SetDecRegister(dec_cont->rv_regs, HWIF_STARTMB_Y, 0);

  SetDecRegister(dec_cont->rv_regs, HWIF_DEC_OUT_DIS, 0);
  SetDecRegister(dec_cont->rv_regs, HWIF_FILTERING_DIS, 1);

  SET_ADDR_REG(dec_cont->rv_regs, HWIF_DIR_MV_BASE,
               dec_cont->StrmStorage.direct_mvs.bus_address);
  SetDecRegister(dec_cont->rv_regs, HWIF_PREV_ANC_TYPE,
                 dec_cont->StrmStorage.p_pic_buf[
                   dec_cont->StrmStorage.work0].is_inter);

#ifdef RV_RAW_STREAM_SUPPORT
  if (dec_cont->StrmStorage.raw_mode)
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_SLICE_AM, 0);
  else
#endif
    SetDecRegister(dec_cont->rv_regs, HWIF_PIC_SLICE_AM,
                   dec_cont->StrmStorage.num_slices-1);
  SET_ADDR_REG(dec_cont->rv_regs, HWIF_QTABLE_BASE,
               dec_cont->StrmStorage.slices.bus_address);

  if (!dec_cont->StrmStorage.is_rv8)
    SetDecRegister(dec_cont->rv_regs, HWIF_FRAMENUM_LEN,
                   dec_cont->StrmStorage.frame_size_bits);


  dec_cont->tiled_reference_enable =
    DecSetupTiledReference( dec_cont->rv_regs,
                            DEC_DPB_FRAME,
                            0 /* interlaced content not present */ );



  if (dec_cont->StrmStorage.raw_mode) {
    SetDecRegister(dec_cont->rv_regs, HWIF_RV_OSV_QUANT,
                   dec_cont->FrameDesc.vlc_set );
  }
  return HANTRO_OK;
}


/*------------------------------------------------------------------------------

    Function name: RvDecPeek

    Functional description:
        Retrieve last decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK         No picture available.
        DEC_PIC_RDY    Picture ready.

------------------------------------------------------------------------------*/
enum DecRet RvDecPeek(RvDecInst dec_inst, RvDecPicture * picture) {
  /* Variables */
  enum DecRet return_value = DEC_PIC_RDY;
  RvDecContainer *dec_cont;
  u32 pic_index = RV_BUFFER_UNDEFINED;

  /* Code */
  APITRACE("%s","Rv_dec_peek#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","RvDecPeek# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (RvDecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecPeek# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  /* when output release thread enabled, RvDecNextPicture_INTERNAL() called in
     RvDecDecode(), and "dec_cont->StrmStorage.out_count--" may called in
     RvDecNextPicture_INTERNAL() before RvDecPeek() called, so dec_cont->fullness
     used to sample the real out_count in case of RvDecNextPicture_INTERNAL() called
     before than RvDecPeek() */
  u32 tmp = dec_cont->fullness;

  /* Nothing to send out */
  if(!tmp) {
    (void) DWLmemset(picture, 0, sizeof(RvDecPicture));
    return_value = DEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.work_out;
    RvFillPicStruct(picture, dec_cont, pic_index);
    if (dec_cont->StrmStorage.out_count > 0)
      dec_cont->StrmStorage.out_count--;
  }

  return return_value;
}

void RvSetExternalBufferInfo(RvDecContainer * dec_cont) {
  u32 pic_size;
  u32 ext_buffer_size;
  u32 out_w, out_h;
  u32 buffers = 3;
  u32 ref_buff_size;


  if(dec_cont->StrmStorage.max_frame_width) {
    out_w = NEXT_MULTIPLE(4 * ((dec_cont->StrmStorage.max_frame_width + 15)>>4) * 16, ALIGN(dec_cont->align));
    out_h = ((dec_cont->StrmStorage.max_frame_height + 15)>>4) * 4;
    pic_size = out_w * out_h * 3 / 2;
  } else {
    out_w = NEXT_MULTIPLE(4 * ((dec_cont->Hdrs.horizontal_size + 15)>>4) * 16, ALIGN(dec_cont->align));
    out_h = ((dec_cont->Hdrs.vertical_size + 15)>>4) * 4;
    pic_size = out_w * out_h * 3 / 2;
  }

  ref_buff_size = pic_size;
  ext_buffer_size = ref_buff_size;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }

  if (dec_cont->pp_enabled)
    dec_cont->buf_num = buffers;
  else
    dec_cont->buf_num =  buffers + 1;
  dec_cont->ext_min_buffer_num = dec_cont->buf_num;
  dec_cont->next_buf_size = ext_buffer_size;
}

enum DecRet RvDecGetBufferInfo(RvDecInst dec_inst, struct DecBufferInfo *mem_info) {
  RvDecContainer  * dec_cont = (RvDecContainer *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;
  u32 i;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    u32 frame_width = dec_cont->FrameDesc.frame_width * 16;
    mem_info->cstride[0] = mem_info->ystride[0] =
                           NEXT_MULTIPLE(frame_width * 4, ALIGN(dec_cont->align));
  } else {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!dec_cont->ppu_cfg[i].enabled) continue;
      mem_info->ystride[i] = dec_cont->ppu_cfg[i].ystride;
      mem_info->cstride[i] = dec_cont->ppu_cfg[i].cstride;
    }
  }

  if (dec_cont->StrmStorage.release_buffer) {
    /* Release old buffers from input queue. */
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
      /* All buffers have been released. */
      dec_cont->StrmStorage.release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL) {
        return (DEC_MEMFAIL);
      }
      dec_cont->StrmStorage.ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return DEC_OK;
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return DEC_WAITING_FOR_BUFFER;
    }
  }

  if(dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;
    return DEC_OK;
  }

  mem_info->buf_to_free = empty;
  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num + dec_cont->n_guard_size;

  ASSERT((mem_info->buf_num && mem_info->next_buf_size) ||
         (DWL_GET_DEVMEM_ADDR(mem_info->buf_to_free)));

  return DEC_WAITING_FOR_BUFFER;
}

enum DecRet RvDecAddBuffer(RvDecInst dec_inst, struct DWLLinearMem *info) {
  RvDecContainer *dec_cont = (RvDecContainer *)dec_inst;
  enum DecRet dec_ret = DEC_OK;

  if(dec_inst == NULL || info == NULL ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  u32 i = dec_cont->buffer_index;

  if (dec_cont->buffer_index >= MAX_PIC_BUFFERS)
    /* Too much buffers added. */
    return DEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->buffer_index++;
  dec_cont->n_ext_buf_size = info->size;

  /* buffer is not enoughm, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    if(dec_cont->buffer_index <= dec_cont->ext_min_buffer_num) {
      if(dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
        dec_cont->StrmStorage.p_pic_buf[i].data = *info;
      } else {
        dec_cont->StrmStorage.p_rpr_buf.data = *info;
      }
    } else {
      dec_cont->StrmStorage.p_pic_buf[i - 1].data = *info;
      dec_cont->StrmStorage.bq.queue_size++;
      dec_cont->StrmStorage.num_buffers++;
    }
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->StrmStorage.ext_buffer_added = 1;
  return dec_ret;
}

static u32 RvCycleCount(RvDecContainer *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->FrameDesc.frame_height << 4, 16) *
             NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width << 4, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->rv_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  return cycles;
}

void RvEnterAbortState(RvDecContainer *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void RvExistAbortState(RvDecContainer *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void RvEmptyBufferQueue(RvDecContainer *dec_cont) {
  BqueueEmpty(&dec_cont->StrmStorage.bq);
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
}

void RvStateReset(RvDecContainer *dec_cont) {
  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  /* Clear internal parameters in RvDecContainer */
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
  dec_cont->fullness = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->fifo_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->same_slice_header = 0;
  dec_cont->mb_error_conceal = 0;
  dec_cont->dec_stat = DEC_OK;
  dec_cont->output_stat = DEC_OK;

  /* Clear internal parameters in DecStrmStorage */
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->StrmStorage.strm_dec_ready = 0;
#endif
  dec_cont->StrmStorage.out_index = 0;
  dec_cont->StrmStorage.out_count = 0;
  dec_cont->StrmStorage.skip_b = 0;
  dec_cont->StrmStorage.prev_pic_coding_type = 0;
  dec_cont->StrmStorage.picture_broken = 0;
  dec_cont->StrmStorage.rpr_detected = 0;
  dec_cont->StrmStorage.rpr_next_pic_type = 0;
  dec_cont->StrmStorage.previous_b = 0;
  dec_cont->StrmStorage.previous_mode_full = 0;
  dec_cont->StrmStorage.fwd_scale = 0;
  dec_cont->StrmStorage.bwd_scale = 0;
  dec_cont->StrmStorage.tr = 0;
  dec_cont->StrmStorage.prev_tr = 0;
  dec_cont->StrmStorage.trb = 0;
  dec_cont->StrmStorage.frame_size_bits = 0;
  dec_cont->StrmStorage.pic_id = 0;
  dec_cont->StrmStorage.prev_pic_id = 0;
  dec_cont->StrmStorage.release_buffer = 0;
  dec_cont->StrmStorage.ext_buffer_added = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->StrmStorage.bq.queue_size = buffers;
  dec_cont->StrmStorage.num_buffers = buffers;
#endif

  /* Clear internal parameters in DecApiStorage */
  dec_cont->ApiStorage.DecStat = STREAMDECODING;

  /* Clear internal parameters in DecFrameDesc */
  dec_cont->FrameDesc.frame_number = 0;
  dec_cont->FrameDesc.pic_coding_type = 0;
  dec_cont->FrameDesc.vlc_set = 0;
  dec_cont->FrameDesc.qp = 0;

  (void) DWLmemset(&dec_cont->MbSetDesc, 0, sizeof(DecMbSetDesc));
  (void) DWLmemset(&dec_cont->StrmDesc, 0, sizeof(DecStrmDesc));
  (void) DWLmemset(&dec_cont->out_pic, 0, sizeof(RvDecPicture));
  (void) DWLmemset(dec_cont->StrmStorage.out_buf, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
  if (!dec_cont->pp_enabled) {
    (void) DWLmemset(dec_cont->StrmStorage.p_pic_buf, 0, 16 * sizeof(picture_t));
    (void) DWLmemset(&dec_cont->StrmStorage.p_rpr_buf, 0, sizeof(picture_t));
  }
  (void) DWLmemset(dec_cont->StrmStorage.picture_info, 0, 32 * sizeof(RvDecPicture));
#endif
#ifdef CLEAR_HDRINFO_IN_SEEK
  (void) DWLmemset(&dec_cont->Hdrs, 0, sizeof(DecHdrs));
  (void) DWLmemset(&dec_cont->tmp_hdrs, 0, sizeof(DecHdrs));
#endif

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

enum DecRet RvDecAbort(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = (RvDecContainer *) dec_inst;

  APITRACE("%s","RvDecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecAbort# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  RvEnterAbortState(dec_cont);
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","RvDecAbort# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet RvDecAbortAfter(RvDecInst dec_inst) {
  RvDecContainer *dec_cont = (RvDecContainer *) dec_inst;

  APITRACE("%s","RvDecAbortAfter#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","RvDecAbortAfter# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

  /* Stop and release HW */
  if(dec_cont->asic_running) {
    u32 core_id = (dec_cont->vcmd_used) ? dec_cont->cmdbuf_id : dec_cont->core_id;
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->rv_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  RvEmptyBufferQueue(dec_cont);

  RvStateReset(dec_cont);

  RvExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","RvDecAbortAfter# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet RvDecSetInfo(RvDecInst dec_inst, struct RvDecConfig *dec_cfg) {
  /*@null@ */ RvDecContainer *dec_cont = (RvDecContainer *)dec_inst;
  u32 pic_width = dec_cont->Hdrs.horizontal_size;
  u32 pic_height = dec_cont->Hdrs.vertical_size;
  u32 i;
  const struct DecHwFeatures *hw_feature = NULL;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_RV_DEC);
  if (!hw_feature) {
    APITRACEDEBUG("%s","RvDecSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
  }

  /* ref aligment */
  dec_cont->align = dec_cfg->align;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    /* ppu alignment */
    dec_cfg->ppu_config[i].align = dec_cfg->align;
    /* ppu dwl instance */
    dec_cont->ppu_cfg[i].dwl = dec_cont->dwl;
    /* 3dlut */
    if (dec_cfg->ppu_config[i].enable_3dlut && (dec_cfg->ppu_config[i].rgb || dec_cfg->ppu_config[i].rgb_planar)) {
      dec_cont->enable_3dlut = 1;
    }
    dec_cont->ppu_cfg[i].table_3dlut_buffer = dec_cfg->table_3dlut_buffer;
  }

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, hw_feature, 8, 1, 0);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL) {
      u32 size = LANCZOS_COEFF_BUFFER_SIZE * sizeof(i16);
      dec_cont->ppu_cfg[i].lanczos_table.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(dec_cont->ppu_cfg[i].lanczos_table.mem_type,
                    DWL_MEM_USAGE_IN_PPULANCZOS_TABLE, dec_cont->secure_mode);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(hw_feature, pic_width, pic_height, 0, 8, PP_CHROMA_420, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    dec_cont->ppu_cfg[i].pixel_width = 8;
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;
  }
  return (DEC_OK);
}

void RvCheckBufferRealloc(RvDecContainer *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (rvGetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->Hdrs.prev_horizontal_size != dec_cont->Hdrs.horizontal_size ||
          dec_cont->Hdrs.prev_vertical_size != dec_cont->Hdrs.vertical_size)
        dec_cont->realloc_ext_buf = 1;
    }

    dec_cont->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
      if (rvGetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (rvGetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}


#ifdef CASE_INFO_STAT
void RvCaseInfoCollect(RvDecContainer *dec_cont, CaseInfo *case_info) {
  u32 pic_width,pic_height,display_width,display_height;

  pic_width = dec_cont->FrameDesc.frame_width << 4;
  pic_height = dec_cont->FrameDesc.frame_height << 4;
  display_width = dec_cont->Hdrs.horizontal_size;
  display_height = dec_cont->Hdrs.vertical_size;

 if(pic_width != display_width || pic_height != display_height)
    case_info->crop_flag = 1;

  if(!case_info->frame_num) {
    case_info->decode_width = pic_width;
    case_info->decode_height = pic_height;
    case_info->display_width = display_width;
    case_info->display_height = display_height;
  } else {
    if(case_info->decode_width != pic_width
    || case_info->decode_height != pic_height) {
      case_info->decode_width = MAX (pic_width , case_info->decode_width);
      case_info->decode_height = MAX (pic_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (display_height, case_info->display_height);
    case_info->display_width = MIN (display_width, case_info->display_width);
  }
  if(case_info->frame_num < 10) {
    switch (dec_cont->FrameDesc.pic_coding_type) {
      case RV_I_PIC: case_info->frame_type[case_info->frame_num] = I_FRAME; break;
      case RV_B_PIC: case_info->frame_type[case_info->frame_num] = B_FRAME; break;
      case RV_P_PIC: case_info->frame_type[case_info->frame_num] = P_FRAME; break;
      case RV_FI_PIC: case_info->frame_type[case_info->frame_num] = FI_FRAME; break;
    }
  }
  case_info->codec = DEC_MODE_RV;

  case_info->bit_depth = 8;
  case_info->chroma_format_id = 1;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->rv_regs, HWIF_STREAM_LEN) * 8/ pic_width)
                         * 3840/ pic_height) * 2160/ 1024/ 1024;
  case_info->frame_num++;
}
#endif

static enum ECDataState rvECGetInDataAction(u32 error_policy) {
  enum ECDataState re = EC_STATE_NONE;
  if (error_policy & DEC_EC_SEEK_NEXT_I) {
    re = EC_STATE_DISCARD;
  } else if (error_policy & DEC_EC_NO_SKIP) {
    re = EC_STATE_NONE;
  } else {
    //stub
  }
  return re;
}

static void rvECMarkOutDataErrInfo(RvDecContainer *dec_cont, u32 asic_status, u32 is_i_frame) {
  u32 error_info = DEC_NO_ERROR;
  if((asic_status & DEC_HW_IRQ_TIMEOUT) ||
     (asic_status & DEC_HW_IRQ_ERROR) ||
     (asic_status & DEC_HW_IRQ_ABORT)) {
    error_info = DEC_FRAME_ERROR;
  }

  u32 is_ref_has_error = is_i_frame ? 0 :
                         ((dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work0].error_info != DEC_NO_ERROR) ||
                          (dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work1].error_info != DEC_NO_ERROR));

  if (asic_status & DEC_HW_IRQ_RDY) {
    if (is_ref_has_error)
      error_info |= DEC_REF_ERROR;
    else
      error_info = DEC_NO_ERROR;
  }
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].error_info = error_info;
}

static void rvECHandleReconData(RvDecContainer *dec_cont, u32 is_update_recon) {
  if(dec_cont->FrameDesc.pic_coding_type != RV_B_PIC) {
    dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
    // if (is_update_recon) {
      dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work_out;
    // }
    if(dec_cont->StrmStorage.skip_b)
      dec_cont->StrmStorage.skip_b--;
  }
}


static enum ECDataState rvECGetOutDataAction(RvDecContainer *dec_cont, u32 error_ratio) {
  enum DecErrorInfo error_info =
                    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].error_info;

  u32 discard_error_pic = IsECDropOutput(dec_cont->error_policy,
                                          dec_cont->error_ratio,
                                          error_info, error_ratio);

  return discard_error_pic ? EC_STATE_DISCARD : EC_STATE_NONE;
}
