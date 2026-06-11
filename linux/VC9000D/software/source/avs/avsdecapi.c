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
#include "avsdecapi.h"
#include "basetype.h"
#include "avs_cfg.h"
#include "avs_container.h"
#include "avs_utils.h"
#include "avs_strm.h"
#include "avsdecapi_internal.h"
#include "dwl.h"
#include "regdrv.h"
#include "avs_headers.h"
#include "deccfg.h"
#include "tiledref.h"
#include "errorhandling.h"
#include "commonconfig.h"
#include "vpufeature.h"
#include "ppu.h"
#include "string.h"
#include "sw_util.h"
#include "sw_performance.h"
#include "dec_log.h"
#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void AvsCaseInfoCollect(DecContainer * dec_cont, CaseInfo *case_info);
#endif

#define AVS_BUFFER_UNDEFINED    16
#define AVS_NOT_SUPPORTED       0
#define ID8170_DEC_TIMEOUT        0xFFU
#define ID8170_DEC_SYSTEM_ERROR   0xFEU
#define ID8170_DEC_HW_RESERVED    0xFDU

#define AVSDEC_IS_FIELD_OUTPUT \
    !dec_cont->Hdrs.progressive_sequence && !dec_cont->pp_config_query.deinterlace

#define AVSDEC_NON_PIPELINE_AND_B_PICTURE \
    ((!dec_cont->pp_config_query.pipeline_accepted || !dec_cont->Hdrs.progressive_sequence) \
    && dec_cont->Hdrs.pic_coding_type == BFRAME)
void AvsRefreshRegs(DecContainer * dec_cont);
void AvsFlushRegs(DecContainer * dec_cont);
static enum DecRet AvsHandleVlcModeError(DecContainer * dec_cont, u32 pic_num);
static void AvsHandleFrameEnd(DecContainer * dec_cont);
static u32 RunDecoderAsic(DecContainer * dec_cont, addr_t strm_bus_address,
                          const struct DecHwFeatures *hw_feature);
static void AvsFillPicStruct(AvsDecPicture * picture,
                             DecContainer * dec_cont, u32 pic_index);
static u32 AvsSetRegs(DecContainer * dec_cont, addr_t strm_bus_address,
                      const struct DecHwFeatures *hw_feature);
static enum DecRet AvsDecNextPictureINTERNAL(AvsDecInst dec_inst,
    AvsDecPicture * picture, u32 end_of_stream);
static u32 AvsCycleCount(DecContainer *dec_cont);
static void AvsSetExternalBufferInfo(AvsDecInst dec_inst);

static void AvsEnterAbortState(DecContainer *dec_cont);
static void AvsExistAbortState(DecContainer *dec_cont);
static void AvsEmptyBufferQueue(DecContainer *dec_cont);
static void AvsCheckBufferRealloc(DecContainer *dec_cont);

#define DEC_DPB_NOT_INITIALIZED      -1

/*------------------------------------------------------------------------------

    Function: AvsDecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance.

        Inputs:
            enum DecErrorHandling error_handling
                            Flag to determine which error concealment method to use.

        Outputs:
            dec_inst         pointer to initialized instance is stored here

        Returns:
            DEC_OK       successfully initialized the instance
            AVSDEC_MEM_FAIL memory allocation failed

------------------------------------------------------------------------------*/
enum DecRet AvsDecInit(AvsDecInst * dec_inst, const void *dwl, struct AvsDecConfig *dec_cfg) {
  /*@null@ */
  DecContainer *dec_cont;
  u32 core_mask;
  const struct DecHwFeatures *hw_feature = NULL;
  enum DecRet ret;

  APITRACE("%s","AvsDecInit#");
  APITRACE("%s","AvsAPI_DecoderInit#");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if (((-1) >> 1) != (-1)) {
    APITRACEERR("%s","AVSDecInit# ERROR: Right shift is not signed");
    return DEC_INITFAIL;
  }
  /*lint -restore */

  if(dec_inst == NULL) {
    APITRACEERR("%s","AVSDecInit# ERROR: dec_inst == NULL");
    return DEC_PARAM_ERROR;
  }
  *dec_inst = NULL;
  hw_feature = DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_AVS_DEC, &core_mask);
  /* check that AVS decoding supported in HW */
  if(core_mask == 0) {
    APITRACEERR("%s","AVSDecInit# ERROR: AVS not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  APITRACEDEBUG("size of DecContainer %d \n", sizeof(DecContainer));
  dec_cont = (DecContainer *) DWLmalloc(sizeof(DecContainer));
  if(dec_cont == NULL) {
    APITRACEERR("%s","AVSDecInit# Memory allocation failed\n");
    return DEC_MEMFAIL;
  }
  /* set everything initially zero */
  DWLmemset(dec_cont, 0, sizeof(DecContainer));

  dec_cont->dwl = dwl;
  dec_cont->avs_plus_support = hw_feature->avs_plus_support;
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_AVS_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  if (dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS)
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;

  dec_cont->StrmStorage.max_num_buffers = dec_cfg->num_frame_buffers;

  AvsAPI_InitDataStructures(dec_cont);

  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->ApiStorage.first_field = 1;
  dec_cont->StrmStorage.unsupported_features_present = 0;

  dec_cont->avs_regs[0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_AVS_DEC);

  dec_cont->error_info = DEC_NO_ERROR;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  SetECPolicy(dec_cfg->error_handling, 0, &dec_cont->error_policy);
  dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;

  SetCommonConfigRegs(dec_cont->avs_regs);

  /* Set prediction filter taps */
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_0,-1);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_1, 5);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_2, 5);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_0_3,-1);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_0, 1);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_1, 7);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_2, 7);
  SetDecRegister(dec_cont->avs_regs, HWIF_PRED_BC_TAP_1_3, 1);

  APITRACEDEBUG("AVS Plus supported: %s\n", hw_feature->avs_plus_support? "YES" : "NO");

  dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VCD;

  /* Custom DPB modes require tiled support >= 2 */
  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;

  APITRACEDEBUG("Container %p\n", (void*) dec_cont);
  APITRACE("%s","AvsDecInit: OK\n");
  *dec_inst = (DecContainer *) dec_cont;

  return (DEC_OK);

err:
  DWLfree(dec_cont);
  return ret;
}

/*------------------------------------------------------------------------------

    Function: AvsDecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before AvsDecDecode function has
            indicated that headers are ready.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            DEC_OK            success
            DEC_PARAM_ERROR     invalid parameters

------------------------------------------------------------------------------*/
enum DecRet AvsDecGetInfo(AvsDecInst dec_inst, AvsDecInfo * dec_info) {

#define API_STOR ((DecContainer *)dec_inst)->ApiStorage
#define DEC_STST ((DecContainer *)dec_inst)->StrmStorage
#define DEC_HDRS ((DecContainer *)dec_inst)->Hdrs
#define DEC_REGS ((DecContainer *)dec_inst)->avs_regs
  APITRACE("%s","AvsDecGetInfo#");

  if(dec_inst == NULL || dec_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  dec_info->multi_buff_pp_size = 2;

  if(API_STOR.DecStat == UNINIT || API_STOR.DecStat == INITIALIZED) {
    return DEC_HDRS_NOT_RDY;
  }

  dec_info->frame_width = DEC_STST.frame_width << 4;
  dec_info->frame_height = DEC_STST.frame_height << 4;

  dec_info->coded_width = DEC_HDRS.horizontal_size;
  dec_info->coded_height = DEC_HDRS.vertical_size;

  dec_info->profile_id = DEC_HDRS.profile_id;
  dec_info->level_id = DEC_HDRS.level_id;
  dec_info->video_range = DEC_HDRS.sample_range;
  dec_info->video_format = DEC_HDRS.video_format;
  dec_info->interlaced_sequence = !DEC_HDRS.progressive_sequence;
  dec_info->dpb_mode = ((DecContainer *)dec_inst)->dpb_mode;
  dec_info->pic_buff_size = ((DecContainer *)dec_inst)->buf_num;

  AvsDecAspectRatio((DecContainer *) dec_inst, dec_info);


  if(!DEC_HDRS.progressive_sequence &&
      (dec_info->dpb_mode != DEC_DPB_INTERLACED_FIELD)) {
    dec_info->output_format = AVSDEC_SEMIPLANAR_YUV420;
  } else {
    dec_info->output_format = AVSDEC_TILED_YUV420;
  }


  APITRACE("%s","AvsDecGetInfo: OK");
  return (DEC_OK);

#undef API_STOR
#undef DEC_STST
#undef DEC_HDRS
#undef DEC_REGS

}


/*------------------------------------------------------------------------------

    Function: AvsDecDecode

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

enum DecRet AvsDecDecode(AvsDecInst dec_inst,
                       AvsDecInput * input, struct DecOutput * output) {
#define API_STOR ((DecContainer *)dec_inst)->ApiStorage
#define DEC_STRM ((DecContainer *)dec_inst)->StrmDesc

  DecContainer *dec_cont;
  enum DecRet internal_ret;
  enum AVSResult strm_dec_result;
  u32 asic_status;
  i32 ret = 0;
  u32 field_rdy = 0;
  u32 error_concealment = 0;
  const struct DecHwFeatures *hw_feature = NULL;

  APITRACE("%s","Avs_dec_decode#\n");

  if(input == NULL || output == NULL || dec_inst == NULL) {
    APITRACEERR("%s","AvsDecDecode# ERROR: PARAM_ERROR\n");
    return DEC_PARAM_ERROR;
  }

  dec_cont = ((DecContainer *) dec_inst);
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_AVS_DEC);

  if(dec_cont->StrmStorage.unsupported_features_present) {
    return (DEC_FORMAT_NOT_SUPPORTED);
  }

  /*
   *  Check if decoder is in an incorrect mode
   */
  if(API_STOR.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecDecode: ERROR: NOT_INITIALIZED\n");
    return DEC_NOT_INITIALIZED;
  }
  if(dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      input->stream == NULL || input->stream_bus_address == 0) {
    APITRACEERR("%s","AvsDecDecode# ERROR: PARAM_ERROR\n");
    return DEC_PARAM_ERROR;
  }

  /* If we have set up for delayed resolution change, do it here */
  if(dec_cont->StrmStorage.new_headers_change_resolution) {
#ifndef USE_OMXIL_BUFFER
    BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
    if (dec_cont->pp_enabled)
      InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
    dec_cont->StrmStorage.new_headers_change_resolution = 0;
    dec_cont->Hdrs.horizontal_size = dec_cont->tmp_hdrs.horizontal_size;
    dec_cont->Hdrs.vertical_size = dec_cont->tmp_hdrs.vertical_size;
    /* Set rest of parameters just in case */
    dec_cont->Hdrs.aspect_ratio = dec_cont->tmp_hdrs.aspect_ratio;
    dec_cont->Hdrs.frame_rate_code = dec_cont->tmp_hdrs.frame_rate_code;
    dec_cont->Hdrs.bit_rate_value = dec_cont->tmp_hdrs.bit_rate_value;

    dec_cont->StrmStorage.frame_width =
      (dec_cont->Hdrs.horizontal_size + 15) >> 4;
    if(dec_cont->Hdrs.progressive_sequence)
      dec_cont->StrmStorage.frame_height =
        (dec_cont->Hdrs.vertical_size + 15) >> 4;
    else
      dec_cont->StrmStorage.frame_height =
        2 * ((dec_cont->Hdrs.vertical_size + 31) >> 5);
    dec_cont->StrmStorage.total_mbs_in_frame =
      (dec_cont->StrmStorage.frame_width *
       dec_cont->StrmStorage.frame_height);
  }

  if(API_STOR.DecStat == HEADERSDECODED) {
    /* check if buffer need to be realloced,
       both external buffer and internal buffer */
    AvsCheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = DEC_WAITING_FOR_BUFFER;
        }

        AvsFreeBuffers(dec_cont);
        if(!dec_cont->StrmStorage.direct_mvs.virtual_address) {
          APITRACEDEBUG("%s","Allocate buffers\n");
          internal_ret = AvsAllocateBuffers(dec_cont);
          if(internal_ret != DEC_OK) {
            APITRACEDEBUG("%s","ALLOC BUFFER FAIL\n");
            APITRACEERR("%s","AvsDecDecode# MEMFAIL\n");
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
        AvsFreeBuffers(dec_cont);
        if(!dec_cont->StrmStorage.direct_mvs.virtual_address) {
          APITRACEDEBUG("%s","Allocate buffers\n");
          internal_ret = AvsAllocateBuffers(dec_cont);
          if(internal_ret != DEC_OK) {
            APITRACEERR("%s","ALLOC BUFFER FAIL\n");
            APITRACEERR("%s","AvsDecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    }
    dec_cont->StrmStorage.work_out = INVALID_ANCHOR_PICTURE;
    dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work1 =
                                  INVALID_ANCHOR_PICTURE;
  }

  /*
   *  Update stream structure
   */
  DEC_STRM.p_strm_buff_start = input->stream;
  DEC_STRM.strm_curr_pos = input->stream;
  DEC_STRM.bit_pos_in_word = 0;
  DEC_STRM.strm_buff_size = input->data_len;
  DEC_STRM.strm_buff_read_bits = 0;

#ifdef _DEC_PP_USAGE
  dec_cont->StrmStorage.latest_id = input->pic_id;
#endif
  do {
    APITRACEDEBUG("%s","Start Decode\n");
    /* run SW if HW is not in the middle of processing a picture
     * (indicated as HW_PIC_STARTED decoder status) */
    if(API_STOR.DecStat == HEADERSDECODED) {
      API_STOR.DecStat = STREAMDECODING;
      if(dec_cont->realloc_ext_buf) {
        dec_cont->buffer_index = 0;
        AvsSetExternalBufferInfo(dec_cont);
        ret = DEC_WAITING_FOR_BUFFER;
      }
    } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
      ret = DEC_WAITING_FOR_BUFFER;
    } else if(API_STOR.DecStat != HW_PIC_STARTED) {
      strm_dec_result = AvsStrmDec_Decode(dec_cont);
      /* TODO: Could it be odd field? If, so release PP and Hw. */
      switch (strm_dec_result) {
        case AVS_PIC_HDR_RDY: {
          /* if type inter predicted and no reference -> error */
          u32 p_ref_check = dec_cont->Hdrs.pic_coding_type==PFRAME &&
                            dec_cont->StrmStorage.work0==INVALID_ANCHOR_PICTURE;
          u32 b_ref_check = dec_cont->Hdrs.pic_coding_type==BFRAME &&
                            (dec_cont->StrmStorage.work0==INVALID_ANCHOR_PICTURE ||
                            dec_cont->StrmStorage.skip_b ||
                            input->skip_frame == DEC_SKIP_NON_REF);
          u32 b_ref_check_work1 = dec_cont->Hdrs.pic_coding_type==BFRAME &&
                                  (dec_cont->StrmStorage.work1==INVALID_ANCHOR_PICTURE &&
                                   !(dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY));
          b_ref_check |= b_ref_check_work1;
          u32 picture_broken = dec_cont->StrmStorage.picture_broken &&
                               (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) &&
                               (dec_cont->Hdrs.pic_coding_type!=IFRAME);
          if(p_ref_check || b_ref_check || picture_broken) {
            if(dec_cont->StrmStorage.skip_b ||
                input->skip_frame == DEC_SKIP_NON_REF) {
              APITRACE("%s","AvsDecDecode# DEC_PENDING_FLUSH\n");
            }
            if (!dec_cont->ApiStorage.first_field && dec_cont->pp_enabled)
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue,
                  DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data)));
            ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
            error_concealment = HANTRO_TRUE;
          } else
            API_STOR.DecStat = HW_PIC_STARTED;
          break;
        }
        case AVS_PIC_SUPRISE_B: {
          /* Handle suprise B */
          dec_cont->Hdrs.low_delay = 0;

          u32 work_out = dec_cont->StrmStorage.work_out;
          ret = DEC_STRM_PROCESSED;
          if (work_out != INVALID_ANCHOR_PICTURE) {
            dec_cont->error_info = DEC_NO_ERROR;
            ret = AvsDecBufferPicture(dec_cont, input->pic_id, HANTRO_FALSE, 0);
          }
          // ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
          // error_concealment = 1;
          break;
        }
        case AVS_PIC_HDR_RDY_ERROR: {
          if(dec_cont->StrmStorage.unsupported_features_present) {
            dec_cont->StrmStorage.unsupported_features_present = 0;
            return DEC_STREAM_NOT_SUPPORTED;
          }
          if (!dec_cont->ApiStorage.first_field && dec_cont->pp_enabled)
            InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data)));
          ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
          error_concealment = 1;
          break;
        }
        case AVS_HDRS_RDY: {
          {
            /* check for minimum and maximum dimensions */
            SwAdjustCoreMaskByWxH(dec_cont->dwl, dec_cont->StrmStorage.frame_height << 4,
                                  dec_cont->StrmStorage.frame_height << 4, 1, DWL_CLIENT_TYPE_AVS_DEC, &dec_cont->core_mask);
            if (CORE_MASK(dec_cont->core_mask) == 0) {
              APITRACEERR("%s","AvsDecDecode# no any core mask support the SPS info\n");
              return DEC_STREAM_NOT_SUPPORTED;
            }
            hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_AVS_DEC);
          }
          internal_ret = AvsDecCheckSupport(dec_cont);
          if(internal_ret != DEC_OK) {
            dec_cont->StrmStorage.strm_dec_ready = FALSE;
            dec_cont->StrmStorage.valid_sequence = 0;
            API_STOR.DecStat = INITIALIZED;
            return internal_ret;
          }

          if(dec_cont->ApiStorage.first_headers) {
            dec_cont->ApiStorage.first_headers = 0;

            SetDecRegister(dec_cont->avs_regs, HWIF_PIC_WIDTH_IN_CBS,
                            dec_cont->StrmStorage.frame_width << 1);
            SetDecRegister(dec_cont->avs_regs, HWIF_DEC_MODE,
                          DEC_MODE_AVS);
          }

          /* Initialize DPB mode */
          if( !dec_cont->Hdrs.progressive_sequence)
            dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
          else
            dec_cont->dpb_mode = DEC_DPB_FRAME;

          /* Initialize tiled mode */
            /* Check mode validity */
          ret = DecCheckTiledMode(dec_cont->dpb_mode,!dec_cont->Hdrs.progressive_sequence);
          if(ret != HANTRO_OK ) {
            APITRACEERR("%s","AvsDecDecode# ERROR: DPB mode does not "\
                        "support tiled reference pictures\n");
            return DEC_PARAM_ERROR;
          }

          API_STOR.DecStat = HEADERSDECODED;

          if (dec_cont->pp_enabled) {
            dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
            dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
          }

          FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
          APITRACE("%s","HDRS_RDY\n");
          ret = DEC_HDRS_RDY;
          break;
        }
        default: {
          ASSERT(strm_dec_result == AVS_END_OF_STREAM);
          if (dec_cont->StrmStorage.new_headers_change_resolution)
            ret = DEC_PIC_DECODED;
          else
            ret = DEC_STRM_PROCESSED;
          break;
        }
      }
    }

    /* picture header properly decoded etc -> start HW */
    if(API_STOR.DecStat == HW_PIC_STARTED) {
      if(dec_cont->ApiStorage.first_field &&
          !dec_cont->asic_running) {
        dec_cont->StrmStorage.work_out = BqueueNext2(
                                           &dec_cont->StrmStorage.bq,
                                           dec_cont->StrmStorage.work0,
                                           dec_cont->StrmStorage.work1,
                                           BQUEUE_UNUSED,
                                           dec_cont->Hdrs.pic_coding_type == BFRAME );
        if(dec_cont->StrmStorage.work_out == INVALID_ANCHOR_PICTURE) {
          if (dec_cont->abort)
            return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          else {
            ret = DEC_NO_DECODING_BUFFER;
            break;
          }
#endif
        }
        dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].first_show = 1;

        if (dec_cont->pp_enabled) {
          struct DWLLinearMem *pp_buffer = NULL;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
          dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data = pp_buffer;
          if (pp_buffer== NULL) {
            dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].first_show = 0;
            BqueuePictureRelease(&dec_cont->StrmStorage.bq, dec_cont->StrmStorage.work_out);
            ret = DEC_NO_DECODING_BUFFER;
            break;
          }
#else
          pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
          dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].pp_data = pp_buffer;
          if (pp_buffer == NULL)
            return DEC_ABORTED;
#endif
          InputQueueSetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer,
            PP_OUT_CTRL(input->dec_ctrl));

#ifdef ENABLE_FPGA_VERIFICATION
          /* update device buffer by host buffer */
          DWLLinearMemset(dec_cont->dwl, pp_buffer, 0, 0, pp_buffer->size);
#endif
        }
      }
      asic_status = RunDecoderAsic(dec_cont, input->stream_bus_address, hw_feature);

      if(asic_status == ID8170_DEC_TIMEOUT) {
        return DEC_HW_TIMEOUT;
      } else if(asic_status == ID8170_DEC_SYSTEM_ERROR) {
        return DEC_SYSTEM_ERROR;
      } else if(asic_status == ID8170_DEC_HW_RESERVED) {
        return DEC_HW_RESERVED;
      } else if(asic_status & DEC_HW_IRQ_ABORT) {
        APITRACEDEBUG("%s","IRQ ABORT IN HW\n");
        dec_cont->error_info = DEC_FRAME_ERROR;
        ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
        error_concealment = HANTRO_TRUE;
      } else if( (asic_status & DEC_HW_IRQ_ERROR) ||
                 (asic_status & DEC_HW_IRQ_TIMEOUT) ) {
        if (asic_status & DEC_HW_IRQ_ERROR) {
          dec_cont->StrmStorage.prev_pic_coding_type =
            dec_cont->Hdrs.pic_coding_type;
          APITRACEERR("%s","STREAM ERROR IN HW\n");
        } else {
          APITRACEERR("%s","IRQ TIMEOUT IN HW");
        }
        dec_cont->error_info = DEC_FRAME_ERROR;
        ret = AvsHandleVlcModeError(dec_cont, input->pic_id);
        error_concealment = HANTRO_TRUE;
      } else if(asic_status & DEC_HW_IRQ_BUFFER) {
        AvsDecPreparePicReturn(dec_cont);
        ret = DEC_BUF_EMPTY;

      }
      /* HW finished decoding a picture */
      else if(asic_status & DEC_HW_IRQ_RDY) {
        dec_cont->error_info = DEC_NO_ERROR;
        error_concealment = HANTRO_FALSE;
        if (dec_cont->Hdrs.pic_coding_type == IFRAME)
          dec_cont->StrmStorage.picture_broken = HANTRO_FALSE; /* reset error flag */
      } else {
        ASSERT(0);
      }

      u32 is_frame_rdy = 0;
      if((asic_status & DEC_HW_IRQ_RDY) ||
         (asic_status & DEC_HW_IRQ_ERROR) ||
         (asic_status & DEC_HW_IRQ_TIMEOUT)) {
        if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
            !dec_cont->ApiStorage.first_field) {
          is_frame_rdy = asic_status & DEC_HW_IRQ_RDY;
          if(is_frame_rdy) {
            field_rdy = 0;
            dec_cont->StrmStorage.frame_number++;
            AvsHandleFrameEnd(dec_cont);
            dec_cont->ApiStorage.first_field = 1;
          }
          /* maybe also need to output when enable ec for stream error and timeout */
          ret = AvsDecBufferPicture(dec_cont, input->pic_id, error_concealment,
                                    AvsCycleCount(dec_cont));
          if(is_frame_rdy) {
            if(dec_cont->Hdrs.pic_coding_type != BFRAME) {
              dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
              dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work_out;
              if(dec_cont->StrmStorage.skip_b)
                dec_cont->StrmStorage.skip_b--;
            }
            dec_cont->StrmStorage.prev_pic_coding_type = dec_cont->Hdrs.pic_coding_type;
            if( dec_cont->Hdrs.pic_coding_type != BFRAME )
              dec_cont->StrmStorage.prev_pic_structure = dec_cont->Hdrs.picture_structure;
          }

        } else {
          field_rdy = 1;
          AvsHandleFrameEnd(dec_cont);
          dec_cont->ApiStorage.first_field = 0;
          if((u32)(dec_cont->StrmDesc.strm_curr_pos-dec_cont->StrmDesc.p_strm_buff_start) >= input->data_len)
            ret = DEC_BUF_EMPTY;
        }
        dec_cont->StrmStorage.valid_pic_header = HANTRO_FALSE;

        /* handle first field indication */
        if(!dec_cont->Hdrs.progressive_sequence) {
          if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE)
            dec_cont->StrmStorage.field_index++;
          else
            dec_cont->StrmStorage.field_index = 1;
        }
#ifdef CASE_INFO_STAT
        AvsCaseInfoCollect(dec_cont, &case_info);
#endif
        AvsDecPreparePicReturn(dec_cont);

        if(is_frame_rdy || (ret != DEC_STRM_PROCESSED && ret != DEC_BUF_EMPTY && !field_rdy))
          API_STOR.DecStat = STREAMDECODING;
      }
    }
  } while(ret == 0);

  if( error_concealment && dec_cont->Hdrs.pic_coding_type != BFRAME ) {
    dec_cont->StrmStorage.picture_broken = 1;
  }

  APITRACE("%s","AvsDecDecode: Exit\n");
  if (ret == DEC_DISCARD_INTERNAL) {
    output->strm_curr_pos = input->stream + input->data_len;
    output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
    output->data_left = 0;
  } else {
    output->strm_curr_pos = dec_cont->StrmDesc.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start);
    output->data_left = dec_cont->StrmDesc.strm_buff_size -
                        (output->strm_curr_pos - DEC_STRM.p_strm_buff_start);
  }

  u32 tmpret;
  AvsDecPicture tmp_output;
  do {
    tmpret = AvsDecNextPictureINTERNAL(dec_cont, &tmp_output, 0);
    if(tmpret == DEC_ABORTED)
      return (DEC_ABORTED);
  } while( tmpret == DEC_PIC_RDY);

  if(dec_cont->abort)
    return(DEC_ABORTED);
  else
    return ((enum DecRet) ret);

#undef API_STOR
#undef DEC_STRM

}

/*------------------------------------------------------------------------------

    Function: AvsDecRelease()

        Functional description:
            Release the decoder instance.

        Inputs:
            dec_inst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void AvsDecRelease(AvsDecInst dec_inst) {
  DecContainer *dec_cont = NULL;
  u32 i;

  APITRACE("%s","AvsDecRelease#\n");
  if(dec_inst == NULL) {
    APITRACEERR("%s","AvsDecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  dec_cont = ((DecContainer *) dec_inst);
  /* Wait all buffers as unused */
  BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);

  pthread_mutex_destroy(&dec_cont->protect_mutex);
  if(dec_cont->asic_running) {
    if (dec_cont->vcmd_used) {
      DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    }
    else {
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, 0);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    }

    dec_cont->asic_running = 0;
  }
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  AvsFreeBuffers(dec_cont);

  DWLfree(dec_cont);

  APITRACE("%s","AvsDecRelease: OK\n");
}

/*------------------------------------------------------------------------------
    Function name   : AvsRefreshRegs
    Description     :
    Return type     : void
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
void AvsRefreshRegs(DecContainer * dec_cont) {
  i32 i;
  u32 *dec_regs = dec_cont->avs_regs;

  if(dec_cont->vcmd_used) {
      DWLRefreshRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->avs_regs);
  } else {
    for(i = 0; i < DEC_X170_REGISTERS; i++) {
      dec_regs[i] = DWLReadReg(dec_cont->dwl, dec_cont->core_id, 4 * i);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : AvsFlushRegs
    Description     :
    Return type     : void
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
void AvsFlushRegs(DecContainer * dec_cont) {
  i32 i;
  u32 *dec_regs = dec_cont->avs_regs;

  if (dec_cont->vcmd_used) {
      DWLFlushRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->avs_regs,
                       dec_cont->mc_refresh_regs[dec_cont->core_id], dec_cont->core_id);
  }else {
    for(i = 2; i < DEC_X170_REGISTERS; i++) {
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * i, dec_regs[i]);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : AvsHandleVlcModeError
    Description     :
    Return type     : enum DecRet
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
enum DecRet AvsHandleVlcModeError(DecContainer * dec_cont, u32 pic_num) {
  enum DecRet ret = DEC_OK;
  u32 tmp;
  ASSERT(dec_cont->StrmStorage.strm_dec_ready);

  u32 work_out = dec_cont->StrmStorage.work_out;
  tmp = AvsStrmDec_NextStartCode(dec_cont);
  if(tmp != END_OF_STREAM) {
    dec_cont->StrmDesc.strm_curr_pos -= 4;
    dec_cont->StrmDesc.strm_buff_read_bits -= 32;
  }

  dec_cont->ApiStorage.first_field = 1;
  dec_cont->ApiStorage.DecStat = STREAMDECODING;
  dec_cont->StrmStorage.valid_pic_header = HANTRO_FALSE;
  dec_cont->Hdrs.picture_structure = FRAMEPICTURE;

  /* return directly when DEC_EC_OUT_NO_ERROR */
  if(dec_cont->error_policy & DEC_EC_OUT_NO_ERROR) {
    ret = DEC_STRM_PROCESSED;
    return ret;
  }

  /* error in first picture -> set reference to grey */
  if(!dec_cont->StrmStorage.frame_number) {
    AvsDecPreparePicReturn(dec_cont);

    /* no pictures finished -> return STRM_PROCESSED */
    if(tmp == END_OF_STREAM)
      ret = DEC_STRM_PROCESSED;

    dec_cont->StrmStorage.work0 = work_out;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work0].error_info = DEC_FRAME_ERROR;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work0].error_ratio = 1 * EC_ROUND_COEFF;
    dec_cont->StrmStorage.skip_b = 2;
  } else {
    if(dec_cont->Hdrs.pic_coding_type != BFRAME) {
      dec_cont->StrmStorage.skip_b = 2;
      dec_cont->StrmStorage.frame_number++;

      // dec_cont->error_info = DEC_FRAME_ERROR;
      // ret = AvsDecBufferPicture(dec_cont, pic_num, HANTRO_TRUE, 0);
      // BqueueDiscard( &dec_cont->StrmStorage.bq, work_out ); //TODO whether need?
      // dec_cont->StrmStorage.work_out = dec_cont->StrmStorage.work0;

      // dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
      if(dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE)
      {
        dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
        dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work1].error_info = DEC_FRAME_ERROR;
        dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work1].error_ratio = 1 * EC_ROUND_COEFF;
      }
    }
  }

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : AvsHandleFrameEnd
    Description     :
    Return type     : u32
    Argument        : DecContainer *dec_cont
------------------------------------------------------------------------------*/
void AvsHandleFrameEnd(DecContainer * dec_cont) {

  u32 tmp;

  dec_cont->StrmDesc.strm_buff_read_bits =
    8 * (dec_cont->StrmDesc.strm_curr_pos -
         dec_cont->StrmDesc.p_strm_buff_start);
  dec_cont->StrmDesc.bit_pos_in_word = 0;

  do {
    tmp = AvsStrmDec_ShowBits(dec_cont, 32);
    if((tmp >> 8) == 0x1)
      break;
  } while(AvsStrmDec_FlushBits(dec_cont, 8) == HANTRO_OK);

}

/*------------------------------------------------------------------------------

         Function name: RunDecoderAsic

         Purpose:       Set Asic run lenght and run Asic

         Input:         DecContainer *dec_cont

         Output:        void

------------------------------------------------------------------------------*/
u32 RunDecoderAsic(DecContainer * dec_cont, addr_t strm_bus_address,
                   const struct DecHwFeatures *hw_feature) {
  i32 ret;
  addr_t tmp = 0;
  u32 asic_status = 0;
  addr_t mask;
  u32 irq = 0;
  struct DWLReqInfo info = {0};

  ASSERT(dec_cont->StrmStorage.
         p_pic_buf[(i32)dec_cont->StrmStorage.work_out].data.bus_address != 0);
  ASSERT(strm_bus_address != 0);

  mask = 15; //.g1_strm_128bit_align always = 1


  /* Save frame/Hdr info for current picture. */
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].frame_width
    = dec_cont->StrmStorage.frame_width;
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].frame_height
    = dec_cont->StrmStorage.frame_height;
  dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].Hdrs = dec_cont->Hdrs;

  if(!dec_cont->asic_running) {
    u32 reserve_ret = 0;
    tmp = AvsSetRegs(dec_cont, strm_bus_address, hw_feature);
    if(tmp == HANTRO_NOK)
      return 0;

    info.core_mask = dec_cont->core_mask;
    info.width = dec_cont->StrmStorage.frame_width*16;
    info.height = dec_cont->StrmStorage.frame_height*16;
    info.owner = (void *)dec_cont;
    if (dec_cont->vcmd_used) {
      dec_cont->core_id = 0;
      reserve_ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_cont->cmdbuf_id);
      UNUSED(reserve_ret);
    } else {
      (void) DWLReserveHw(dec_cont->dwl, &info, &dec_cont->core_id);
    }

    /* Warning: only single core are currently supported (core_id = 0) */
    PPSetLancozsScaleRegs(dec_cont->avs_regs, hw_feature, dec_cont->ppu_cfg, dec_cont->core_id);

    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_DIS, 0);

    dec_cont->asic_running = 1;

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x4, 0);
    AvsFlushRegs(dec_cont);
    /* Enable HW */
    if (dec_cont->vcmd_used)
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->ppu_cfg, 0);
    else
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->core_id, dec_cont->ppu_cfg, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_E, 1);
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id,
                4 * 1, dec_cont->avs_regs[1]);
  } else { /* in the middle of decoding, continue decoding */
    /* tmp is strm_bus_address + number of bytes decoded by SW */
    tmp = dec_cont->StrmDesc.strm_curr_pos -
          dec_cont->StrmDesc.p_strm_buff_start;
    tmp = strm_bus_address + tmp;

    /* pointer to start of the stream, mask to get the pointer to
     * previous 64/128-bit aligned position */
    if(!(tmp & ~(mask))) {
      return 0;
    }

    SET_ADDR_REG(dec_cont->avs_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));
    /* amount of stream (as seen by the HW), obtained as amount of stream
     * given by the application subtracted by number of bytes decoded by
     * SW (if strm_bus_address is not 64/128-bit aligned -> adds number of bytes
     * from previous 64/128-bit aligned boundary) */
    SetDecRegister(dec_cont->avs_regs, HWIF_STREAM_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));

    SetDecRegister(dec_cont->avs_regs, HWIF_STRM_BUFFER_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));
    SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_OFFSET, 0);

    SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

    /* This depends on actual register allocation */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->avs_regs[5]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->avs_regs[6]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->avs_regs[258]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->avs_regs[259]);
    if (IS_LEGACY(dec_cont->avs_regs[0]))
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 12, dec_cont->avs_regs[12]);
    else
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169, dec_cont->avs_regs[169]);
    if(sizeof(addr_t) == 8) {
      if (hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->avs_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, dec_cont->avs_regs[122]);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, dec_cont->avs_regs[168]);
      } else {
        ASSERT(dec_cont->avs_regs[122] == 0);
        ASSERT(dec_cont->avs_regs[168] == 0);
      }
    } else {
      if (hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->avs_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, 0);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, 0);
      }
    }
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id,
                4 * 1, dec_cont->avs_regs[1]);
  }

  /* Wait for HW ready */
  if (dec_cont->vcmd_used)
    ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
  else
    ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id, (u32) DEC_X170_TIMEOUT_LENGTH);

  AvsRefreshRegs(dec_cont);
  if(ret == DWL_HW_WAIT_OK) {
    asic_status =
      GetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT);
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
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_E, 0);
    dec_cont->asic_running = 0;

    if (dec_cont->vcmd_used) {
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      UNUSED(irq);
    } else {
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,  dec_cont->avs_regs[1]);
      (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    }
  }

  /* if HW interrupt indicated either BUFFER_EMPTY or
   * AVS_RDY -> read stream end pointer and update StrmDesc structure */
  if((asic_status &
      (DEC_HW_IRQ_BUFFER | DEC_HW_IRQ_RDY))) {
    tmp = GET_ADDR_REG(dec_cont->avs_regs, HWIF_RLC_VLC_BASE);

    if(((tmp - strm_bus_address) <= dec_cont->max_strm_len) &&
        ((tmp - strm_bus_address) <= dec_cont->StrmDesc.strm_buff_size)) {
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

  SetDecRegister(dec_cont->avs_regs, HWIF_DEC_IRQ_STAT, 0);

  return asic_status;

}

/*------------------------------------------------------------------------------

    Function name: AvsDecNextPicture

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
enum DecRet AvsDecNextPicture(AvsDecInst dec_inst, AvsDecPicture * picture) {
  /* Variables */
  DecContainer *dec_cont;
  i32 ret;

  /* Code */
  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","AvsDecNextPicture# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecNextPicture# ERROR: Decoder not initialized\n");
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
      APITRACE("%s","AvsDecNextPicture# DEC_END_OF_STREAM\n");
      return DEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      APITRACE("%s","AvsDecNextPicture# DEC_FLUSHED\n");
      return DEC_FLUSHED;
    }

    *picture = dec_cont->StrmStorage.picture_info[i];

    APITRACE("%s","AvsDecNextPicture# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else
    return DEC_ABORTED;
}

/*------------------------------------------------------------------------------

    Function name: AvsDecNextPictureINTERNAL

    Functional description:
        Push next picture in display order into output fifo if any available.

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK               No picture available.
        DEC_PIC_RDY          Picture ready.
        DEC_PARAM_ERROR      invalid parameters
        DEC_NOT_INITIALIZED  decoder instance not initialized yet

------------------------------------------------------------------------------*/
enum DecRet AvsDecNextPictureINTERNAL(AvsDecInst dec_inst,
                                     AvsDecPicture * picture, u32 end_of_stream) {
  /* Variables */
  enum DecRet return_value = DEC_PIC_RDY;
  DecContainer *dec_cont;
  u32 pic_index = AVS_BUFFER_UNDEFINED;
  u32 min_count;

  /* Code */
  APITRACE("%s","Avs_dec_next_picture_INTERNAL#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","AvsDecNextPictureINTERNAL# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecNextPictureINTERNAL# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  DWLmemset(picture, 0, sizeof(AvsDecPicture));
  min_count = 0;
  if(dec_cont->StrmStorage.sequence_low_delay == 0 && !end_of_stream &&
      !dec_cont->StrmStorage.new_headers_change_resolution)
    min_count = 1;

  /* this is to prevent post-processing of non-finished pictures in the
   * end of the stream */
  if(end_of_stream && dec_cont->Hdrs.pic_coding_type == BFRAME) {
    dec_cont->Hdrs.pic_coding_type = PFRAME;
  }

  /* Nothing to send out */
  if(dec_cont->StrmStorage.out_count <= min_count) {
    (void) DWLmemset(picture, 0, sizeof(AvsDecPicture));
    picture->pictures[0].output_picture = NULL;
    picture->interlaced = !dec_cont->Hdrs.progressive_sequence;
    return_value = DEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.out_index;
    pic_index = dec_cont->StrmStorage.out_buf[pic_index];

    AvsFillPicStruct(picture, dec_cont, pic_index);

    /* field output */
    //if(AVSDEC_IS_FIELD_OUTPUT)
    if(!dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.progressive_sequence) {
      picture->interlaced = 1;
      picture->field_picture = 1;

      if(!dec_cont->ApiStorage.output_other_field) {
        picture->top_field =
          dec_cont->StrmStorage.p_pic_buf[(i32)pic_index].tf ? 1 : 0;
        dec_cont->ApiStorage.output_other_field = 1;
      } else {
        picture->top_field =
          dec_cont->StrmStorage.p_pic_buf[(i32)pic_index].tf ? 0 : 1;
        dec_cont->ApiStorage.output_other_field = 0;
        dec_cont->StrmStorage.out_count--;
        dec_cont->StrmStorage.out_index++;
        dec_cont->StrmStorage.out_index &= 15;
      }
    } else {
      /* progressive or deinterlaced frame output */
      picture->interlaced = !dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.progressive_sequence;
      picture->top_field = 0;
      picture->field_picture = 0;
      dec_cont->StrmStorage.out_count--;
      dec_cont->StrmStorage.out_index++;
      dec_cont->StrmStorage.out_index &= 15;
    }

#ifdef USE_PICTURE_DISCARD
    if (dec_cont->StrmStorage.p_pic_buf[pic_index].first_show)
#endif
    {
      /* wait this buffer as unused */
      if(BqueueWaitBufNotInUse(&dec_cont->StrmStorage.bq, pic_index) != HANTRO_OK)
        return DEC_ABORTED;
      if(dec_cont->pp_enabled) {
        InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data)));
      }

      //dec_cont->StrmStorage.p_pic_buf[pic_index].notDisplayed = 1;

      /* set this buffer as used */
      if((!dec_cont->ApiStorage.output_other_field &&
          picture->interlaced) || !picture->interlaced) {
        BqueueSetBufferAsUsed(&dec_cont->StrmStorage.bq, pic_index);
        dec_cont->StrmStorage.p_pic_buf[pic_index].first_show = 0;
        if(dec_cont->pp_enabled)
          InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->StrmStorage.p_pic_buf[pic_index].pp_data)));
      }

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

    Function name: AvsDecPictureConsumed

    Functional description:
        release specific decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to  picture struct


    Return values:
        DEC_PARAM_ERROR         Decoder instance or picture is null
        DEC_NOT_INITIALIZED     Decoder instance isn't initialized
        DEC_OK                  picture release success
------------------------------------------------------------------------------*/
enum DecRet AvsDecPictureConsumed(AvsDecInst dec_inst, AvsDecPicture * picture) {
  /* Variables */
  DecContainer *dec_cont;
  u32 i;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */
  APITRACE("%s","Avs_dec_picture_consumed#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","AvsDecPictureConsumed# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers; i++) {
      if(picture->pictures[0].output_picture_bus_address == dec_cont->StrmStorage.p_pic_buf[i].data.bus_address ) {
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

enum DecRet AvsDecEndOfStream(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *) dec_inst;
  AvsDecPicture output;
  enum DecRet ret;

  APITRACE("%s","AvsDecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }
  if(dec_cont->dec_stat == DEC_END_OF_STREAM) {
    return (DEC_OK);
  }

  if(dec_cont->asic_running) {
    /* stop HW */
    u32 core_id = (dec_cont->vcmd_used) ? dec_cont->cmdbuf_id : dec_cont->core_id;
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->avs_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  while((ret = AvsDecNextPictureINTERNAL(dec_inst, &output, 1)) == DEC_PIC_RDY);
  if(ret == DEC_ABORTED) {
    return (DEC_ABORTED);
  }

  dec_cont->dec_stat = DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);

  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;

  APITRACE("%s","AvsDecEndOfStream# DEC_OK\n");
  return (DEC_OK);
}


/*----------------------=-------------------------------------------------------

    Function name: AvsFillPicStruct

    Functional description:
        Fill data to output pic description

    Input:
        dec_cont    Decoder container
        picture    Pointer to return value struct

    Return values:
        void

------------------------------------------------------------------------------*/
static void AvsFillPicStruct(AvsDecPicture * picture,
                             DecContainer * dec_cont, u32 pic_index) {
  picture_t *p_pic;

  p_pic = (picture_t *) dec_cont->StrmStorage.p_pic_buf;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;

  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = p_pic[pic_index].frame_width << 4;
    picture->pictures[0].frame_height = p_pic[pic_index].frame_height << 4;
    picture->pictures[0].coded_width = p_pic[pic_index].Hdrs.horizontal_size;
    picture->pictures[0].coded_height = p_pic[pic_index].Hdrs.vertical_size;
    picture->pictures[0].pic_stride = NEXT_MULTIPLE(picture->pictures[0].frame_width * 4,
                                                      ALIGN(dec_cont->align));
    picture->pictures[0].pic_stride_ch = picture->pictures[0].pic_stride;
    picture->pictures[0].output_picture = (u8 *)p_pic[pic_index].data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic[pic_index].data.bus_address;
    picture->pictures[0].output_picture_chroma = (u8 *)p_pic[pic_index].data.virtual_address +
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
      picture->pictures[i].output_format = TransUnitConfig2Format(&dec_cont->ppu_cfg[i]);
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
      if(ppu_cfg->dec400_enabled)
        PpFillDec400TblInfo(ppu_cfg,
                            p_pic[pic_index].pp_data->virtual_address,
                            p_pic[pic_index].pp_data->bus_address,
                            &picture->pictures[i].dec400_luma_table,
                            &picture->pictures[i].dec400_chroma_table);
    }
  }
  picture->interlaced = !p_pic[pic_index].Hdrs.progressive_sequence;

  picture->key_picture = p_pic[pic_index].pic_type;
  picture->pic_id = p_pic[pic_index].pic_id;
  picture->decode_id = p_pic[pic_index].pic_id;
  picture->pic_coding_type = p_pic[pic_index].pic_code_type;

  /* handle first field indication */
  if(!p_pic[pic_index].Hdrs.progressive_sequence) {
    if(dec_cont->StrmStorage.field_out_index)
      dec_cont->StrmStorage.field_out_index = 0;
    else
      dec_cont->StrmStorage.field_out_index = 1;
  }

  picture->first_field = p_pic[pic_index].ff[dec_cont->StrmStorage.field_out_index];
  picture->repeat_first_field = p_pic[pic_index].rff;
  picture->repeat_frame_count = p_pic[pic_index].rfc;
  picture->number_of_err_mbs = p_pic[pic_index].nbr_err_mbs;
  picture->cycles_per_mb = p_pic[pic_index].cycles_per_mb;
  (void) DWLmemcpy(&picture->time_code,
                   &p_pic[pic_index].time_code, sizeof(struct DecTime));
}

/*------------------------------------------------------------------------------

    Function name: AvsSetRegs

    Functional description:
        Set registers

    Input:
        container

    Return values:
        void

------------------------------------------------------------------------------*/
static u32 AvsSetRegs(DecContainer * dec_cont, addr_t strm_bus_address,
                      const struct DecHwFeatures *hw_feature) {
  addr_t tmp = 0;
  u32 tmp_fwd, tmp_curr;
  addr_t mask;
  u32 work_out = dec_cont->StrmStorage.work_out;

#ifdef _DEC_PP_USAGE
  AvsDecPpUsagePrint(dec_cont, DECPP_UNSPECIFIED, work_out, 1,
                     dec_cont->StrmStorage.latest_id);
#endif

  mask = 15; //g1_strm_128bit_align = 1
  /*
  if(!dec_cont->Hdrs.progressive_sequence)
      SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_TILED_E, 0);
      */

  APITRACEDEBUG("Decoding to index %d \n", work_out);

  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_INTERLACE_E, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_FIELDMODE_E, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_TOPFIELD_E, 0);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_INTERLACE_E, 1);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_FIELDMODE_E, 1);
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_TOPFIELD_E,
                   dec_cont->ApiStorage.first_field);
  }


  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_HEIGHT_IN_CBS,
                   dec_cont->StrmStorage.frame_height << 1);


  if(dec_cont->Hdrs.pic_coding_type == BFRAME)
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_B_E, 1);
  else
    SetDecRegister(dec_cont->avs_regs, HWIF_PIC_B_E, 0);

  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_INTER_E,
                 dec_cont->Hdrs.pic_coding_type != IFRAME);

  /* tmp is strm_bus_address + number of bytes decoded by SW */
  tmp = dec_cont->StrmDesc.strm_curr_pos -
        dec_cont->StrmDesc.p_strm_buff_start;
  tmp = strm_bus_address + tmp;

  /* bus address must not be zero */
  if(!(tmp & ~0x7)) {
    return 0;
  }

  /* pointer to start of the stream, mask to get the pointer to
   * previous 64/128-bit aligned position */
  SET_ADDR_REG(dec_cont->avs_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

  /* amount of stream (as seen by the HW), obtained as amount of
   * stream given by the application subtracted by number of bytes
   * decoded by SW (if strm_bus_address is not 64/128-bit aligned -> adds
   * number of bytes from previous 64/128-bit aligned boundary) */
  SetDecRegister(dec_cont->avs_regs, HWIF_STREAM_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));

  SetDecRegister(dec_cont->avs_regs, HWIF_STRM_BUFFER_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));
  SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_OFFSET, 0);

  SetDecRegister(dec_cont->avs_regs, HWIF_STRM_START_BIT,
                 dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));
  DWLDMATransData2(dec_cont->dwl, (addr_t)strm_bus_address,
                  (void *)dec_cont->StrmDesc.p_strm_buff_start,
                  dec_cont->StrmDesc.strm_buff_size, HOST_TO_DEVICE);
  if (dec_cont->enable_3dlut)
    DWLDMATransData(dec_cont->dwl, &dec_cont->ppu_cfg[0].table_3dlut_buffer, 0,
                    dec_cont->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);

  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_FIXED_QUANT,
                 dec_cont->Hdrs.fixed_picture_qp);
  SetDecRegister(dec_cont->avs_regs, HWIF_INIT_QP,
                 dec_cont->Hdrs.picture_qp);

  /* AVS Plus stuff */
  SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_E,
                 dec_cont->Hdrs.weighting_quant_flag);
  SetDecRegister(dec_cont->avs_regs, HWIF_AVS_AEC_E,
                 dec_cont->Hdrs.aec_enable);
  SetDecRegister(dec_cont->avs_regs, HWIF_NO_FWD_REF_E,
                 dec_cont->Hdrs.no_forward_reference_flag);
  SetDecRegister(dec_cont->avs_regs, HWIF_PB_FIELD_ENHANCED_E,
                 dec_cont->Hdrs.pb_field_enhanced_flag);

  if (dec_cont->Hdrs.profile_id == 0x48) {
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_AVSP_ENA, 1);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_DEC_AVSP_ENA, 0);
  }

  if(dec_cont->Hdrs.weighting_quant_flag == 1 &&
      dec_cont->Hdrs.chroma_quant_param_disable == 0x0) {
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CB,
                   dec_cont->Hdrs.chroma_quant_param_delta_cb);
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CR,
                   dec_cont->Hdrs.chroma_quant_param_delta_cr);
  } else {
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CB, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_QP_DELTA_CR, 0);
  }

  if(dec_cont->Hdrs.weighting_quant_flag == 1) {
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_MODEL,
                   dec_cont->Hdrs.weighting_quant_model);

    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_0,
                   dec_cont->Hdrs.weighting_quant_param[0]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_1,
                   dec_cont->Hdrs.weighting_quant_param[1]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_2,
                   dec_cont->Hdrs.weighting_quant_param[2]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_3,
                   dec_cont->Hdrs.weighting_quant_param[3]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_4,
                   dec_cont->Hdrs.weighting_quant_param[4]);
    SetDecRegister(dec_cont->avs_regs, HWIF_WEIGHT_QP_5,
                   dec_cont->Hdrs.weighting_quant_param[5]);
  }
  /* AVS Plus end */

  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  if (dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
      dec_cont->ApiStorage.first_field) {
    SET_ADDR_REG(dec_cont->avs_regs, HWIF_DEC_OUT_BASE,
                 p_pic_buf[work_out].data.bus_address);
  } else {

    /* start of bottom field line */
    if(dec_cont->dpb_mode == DEC_DPB_FRAME ) {
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_DEC_OUT_BASE,
                   (dec_cont->StrmStorage.p_pic_buf[work_out].
                    data.bus_address +
                    ((dec_cont->StrmStorage.frame_width << 4))));
    } else if( dec_cont->dpb_mode == DEC_DPB_INTERLACED_FIELD ) {
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_DEC_OUT_BASE,
                   p_pic_buf[work_out].data.bus_address);
    }
  }
  SetDecRegister(dec_cont->avs_regs, HWIF_PP_OUT_E_U, dec_cont->pp_enabled);
  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    u32 bottom_flag = !(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
                        dec_cont->ApiStorage.first_field);
    u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, p_pic_buf[work_out].pp_data);
    struct PpParams pp_args = {hw_feature,
                               ppu_cfg,
                               p_pic_buf[work_out].pp_data,
                               0,
                               bottom_flag,
                               pp_out_ctrl
                              };
    InitPpUnitBoundCoeff(hw_feature, (dec_cont->Hdrs.picture_structure != FRAMEPICTURE), ppu_cfg);

    PPSetRegs(dec_cont->avs_regs, &pp_args);
    SetDecRegister(dec_cont->avs_regs, HWIF_PP_IN_FORMAT_U, 1);
  }
  SetDecRegister(dec_cont->avs_regs, HWIF_UNIQUE_ID, UNIQUE_ID(0));

  /* Stride registers only available since g1v8_2 */
  SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_Y_STRIDE,
                    NEXT_MULTIPLE(dec_cont->StrmStorage.frame_width * 4 * 16, ALIGN(dec_cont->align)));
  SetDecRegister(dec_cont->avs_regs, HWIF_DEC_OUT_C_STRIDE,
                    NEXT_MULTIPLE(dec_cont->StrmStorage.frame_width * 4 * 16, ALIGN(dec_cont->align)));

  SetDecRegister( dec_cont->avs_regs, HWIF_DPB_ILACE_MODE,
                  dec_cont->dpb_mode );

  u32 work0 = dec_cont->StrmStorage.work0;
  u32 work1 = dec_cont->StrmStorage.work1;
  if(dec_cont->Hdrs.pic_coding_type == BFRAME) {
    /* try to ref replacement */
    u32 error_ratio_work0 = p_pic_buf[work0].error_ratio;
    u32 error_ratio_work1 = p_pic_buf[work1].error_ratio;
    if (p_pic_buf[work0].error_info != DEC_NO_ERROR) {
      if (error_ratio_work0 > EC_RATIO_THRESHOLD && error_ratio_work1 <= EC_RATIO_THRESHOLD)
        dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work1;
    }
    if (p_pic_buf[work1].error_info != DEC_NO_ERROR) {
      if (error_ratio_work1 > EC_RATIO_THRESHOLD && error_ratio_work0 <= EC_RATIO_THRESHOLD)
        dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
    }
    work0 = dec_cont->StrmStorage.work0;
    work1 = dec_cont->StrmStorage.work1;
  }
  /* past anchor set to future anchor if past is invalid (second
   * picture in sequence is B)
   */
  tmp_fwd = (work1 != INVALID_ANCHOR_PICTURE) ? work1 : work0;

  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    if(dec_cont->Hdrs.pic_coding_type == BFRAME) {
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE, p_pic_buf[work0].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE, p_pic_buf[work0].data.bus_address);

      /* block distances */
      /* current to future anchor */
      tmp = (2*p_pic_buf[work0].picture_distance -
             2*dec_cont->Hdrs.picture_distance + 512) & 0x1FF;
      /* prevent division by zero */
      if (!tmp) tmp = 2;
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2, 512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3, 512/tmp);

      /* current to past anchor */
      if (dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*p_pic_buf[work1].picture_distance - 512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0, 512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1, 512/tmp);

      /* future anchor to past anchor */
      if (work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*p_pic_buf[work0].picture_distance -
               2*p_pic_buf[work1].picture_distance - +512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      tmp = 16384/tmp;
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, tmp);

      /* future anchor to previous past anchor */
      tmp = dec_cont->StrmStorage.future2prev_past_dist;
      tmp = 16384/tmp;
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, tmp);
    } else {
      tmp_fwd =
        dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE ?
        dec_cont->StrmStorage.work1 :
        dec_cont->StrmStorage.work0;

      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE, p_pic_buf[work0].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE, p_pic_buf[work0].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE, p_pic_buf[tmp_fwd].data.bus_address);

      /* current to past anchor */
      tmp = (2*dec_cont->Hdrs.picture_distance -
             2*p_pic_buf[work0].picture_distance - 512) & 0x1FF;
      if (!tmp) tmp = 2;

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0, 512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1, 512/tmp);
      /* current to previous past anchor */
      if (work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*p_pic_buf[work1].picture_distance - 512) & 0x1FF;
        if (!tmp) tmp = 2;
      }

      /* this will become "future to previous past" for next B */
      dec_cont->StrmStorage.future2prev_past_dist = tmp;

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3,
                     512/tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 0);
    }
    /* AVS Plus stuff */
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2, 0);
    SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3, 0);
    /* AVS Plus end */
  } else { /* field interlaced */
    if(dec_cont->Hdrs.pic_coding_type == BFRAME) {
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE, p_pic_buf[work0].data.bus_address);
      SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE, p_pic_buf[work0].data.bus_address);

      /* block distances */
      tmp = (2*dec_cont->StrmStorage.
             p_pic_buf[work0].picture_distance -
             2*dec_cont->Hdrs.picture_distance + 512) & 0x1FF;
      /* prevent division by zero */
      if (!tmp) tmp = 2;
      if (dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp+1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2, 512/tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3, 512/(tmp+1));
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2, 512/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3, 512/tmp);
      }

      if (work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*p_pic_buf[work1].picture_distance - 512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      if (dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0, 512/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1, 512/tmp);
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp+1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0, 512/tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1, 512/(tmp+1));
      }

      if (work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*p_pic_buf[work0].picture_distance -
               2*p_pic_buf[work1].picture_distance - +512) & 0x1FF;
        if (!tmp) tmp = 2;
      }
      /* AVS Plus stuff */
      if (dec_cont->Hdrs.pb_field_enhanced_flag &&
          !dec_cont->ApiStorage.first_field) {
        /* in this case, BlockDistanceRef is different with before, the mvRef points to top field */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 16384/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 16384/tmp);

        /* future anchor to previous past anchor */
        tmp = dec_cont->StrmStorage.future2prev_past_dist;

        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 16384/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 16384/tmp);
      } else {
        if (dec_cont->ApiStorage.first_field) {
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 16384/(tmp-1));
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 16384/tmp);
        } else {
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 16384/1);
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 16384/tmp);
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 16384/(tmp+1));
        }

        /* future anchor to previous past anchor */
        tmp = dec_cont->StrmStorage.future2prev_past_dist;

        if( dec_cont->ApiStorage.first_field) {
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 16384/(tmp-1));
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 16384/tmp);
        } else
          SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 16384/tmp);
      }

      if(dec_cont->ApiStorage.first_field) {
        /* 1 means delta=2, 3 means delta=-2, 0 means delta=0 */
        /* delta1 */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

        /* deltaFw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
        /* deltaBw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3, 0);
      } else {
        /* delta1 */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

        /* deltaFw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 0);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
        /* deltaBw */
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2,
                       (u32)-2);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3,
                       (u32)-2);
      }
      /* AVS Plus end */
    } else {
      tmp_curr = work_out;
      /* past anchor not available -> use current (this results in using
       * the same top or bottom field as reference and output picture
       * base, output is probably corrupted) */
      if(tmp_fwd == INVALID_ANCHOR_PICTURE)
        tmp_fwd = tmp_curr;

      if(dec_cont->ApiStorage.first_field) {
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE, p_pic_buf[work0].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE, p_pic_buf[work0].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE, p_pic_buf[tmp_fwd].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE, p_pic_buf[tmp_fwd].data.bus_address);

      } else {
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER0_BASE, p_pic_buf[tmp_curr].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER1_BASE, p_pic_buf[work0].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER2_BASE, p_pic_buf[work0].data.bus_address);
        SET_ADDR_REG(dec_cont->avs_regs, HWIF_REFER3_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      }

      tmp = (2*dec_cont->Hdrs.picture_distance -
             2*dec_cont->StrmStorage.
             p_pic_buf[(i32)dec_cont->StrmStorage.work0].picture_distance -
             512) & 0x1FF;
      if (!tmp) tmp = 2;

      if(!dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, 1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2,
                       tmp+1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0, 512/1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2, 512/(tmp+1));
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_0, tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_0, 512/(tmp-1));
      }
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_1, tmp);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_1, 512/tmp);

      if (work1 != INVALID_ANCHOR_PICTURE) {
        tmp = (2*dec_cont->Hdrs.picture_distance -
               2*p_pic_buf[work1].picture_distance - 512) & 0x1FF;
        if (!tmp) tmp = 2;
      }

      /* this will become "future to previous past" for next B */
      dec_cont->StrmStorage.future2prev_past_dist = tmp;

      if(dec_cont->ApiStorage.first_field) {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_2, tmp-1);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_2, 512/(tmp-1));
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3, 512/tmp);
      } else {
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_DIST_CUR_3, tmp);
        SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_CUR_3, 512/tmp);
      }

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_INVD_COL_3, 0);

      /* AVS Plus stuff */
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_COL_3, 0);

      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_0, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_1, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_2, 0);
      SetDecRegister(dec_cont->avs_regs, HWIF_REF_DELTA_CUR_3, 0);
      /* AVS Plus end */
    }
  }

  SetDecRegister(dec_cont->avs_regs, HWIF_STARTMB_X, 0);
  SetDecRegister(dec_cont->avs_regs, HWIF_STARTMB_Y, 0);

  SetDecRegister(dec_cont->avs_regs, HWIF_FILTERING_DIS,
                 dec_cont->Hdrs.loop_filter_disable);
  SetDecRegister(dec_cont->avs_regs, HWIF_ALPHA_OFFSET,
                 dec_cont->Hdrs.alpha_offset);
  SetDecRegister(dec_cont->avs_regs, HWIF_BETA_OFFSET,
                 dec_cont->Hdrs.beta_offset);
  SetDecRegister(dec_cont->avs_regs, HWIF_SKIP_MODE,
                 dec_cont->Hdrs.skip_mode_flag);

  SetDecRegister(dec_cont->avs_regs, HWIF_PIC_REFER_FLAG,
                 dec_cont->Hdrs.picture_reference_flag);

  if (dec_cont->Hdrs.pic_coding_type == PFRAME ||
      (dec_cont->Hdrs.pic_coding_type == IFRAME /*&&
         !dec_cont->ApiStorage.first_field*/)) { /* AVS Plus change */
    SetDecRegister(dec_cont->avs_regs, HWIF_WRITE_MVS_E, 1);
  } else
    SetDecRegister(dec_cont->avs_regs, HWIF_WRITE_MVS_E, 0);

  if (dec_cont->ApiStorage.first_field ||
      ( dec_cont->Hdrs.pic_coding_type == BFRAME &&
        dec_cont->StrmStorage.prev_pic_structure ))
    SET_ADDR_REG(dec_cont->avs_regs, HWIF_DIR_MV_BASE,
                 dec_cont->StrmStorage.direct_mvs.bus_address);
  else
    SET_ADDR_REG(dec_cont->avs_regs, HWIF_DIR_MV_BASE,
                 dec_cont->StrmStorage.direct_mvs.bus_address +
                     NEXT_MULTIPLE(((( dec_cont->StrmStorage.frame_width *
                         dec_cont->StrmStorage.frame_height/2 + 1) & ~0x1) *
                         4 * sizeof(u32)), MAX(16, ALIGN(dec_cont->align))));
  /* AVS Plus stuff */
  SET_ADDR_REG(dec_cont->avs_regs, HWIF_DIR_MV_BASE2,
               dec_cont->StrmStorage.direct_mvs.bus_address);

  /* AVS Plus end */
  SetDecRegister(dec_cont->avs_regs, HWIF_PREV_ANC_TYPE,
                 !p_pic_buf[work0].pic_type ||
                 (!dec_cont->ApiStorage.first_field &&
                  dec_cont->StrmStorage.prev_pic_structure == 0));

  /* b-picture needs to know if future reference is field or frame coded */
  SetDecRegister(dec_cont->avs_regs, HWIF_REFER2_FIELD_E,
                 dec_cont->StrmStorage.prev_pic_structure == 0);
  SetDecRegister(dec_cont->avs_regs, HWIF_REFER3_FIELD_E,
                 dec_cont->StrmStorage.prev_pic_structure == 0);

  dec_cont->tiled_reference_enable =
    DecSetupTiledReference( dec_cont->avs_regs,
                            dec_cont->dpb_mode,
                            !dec_cont->Hdrs.progressive_sequence );


  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function name: AvsDecPeek

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
enum DecRet AvsDecPeek(AvsDecInst dec_inst, AvsDecPicture * picture) {
  /* Variables */

  DecContainer *dec_cont;
  u32 pic_index;
  picture_t *p_pic;

  /* Code */

  APITRACE("%s","Avs_dec_peek#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","AvsDecPeek# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecPeek# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  /* when output release thread enabled, AvsDecNextPictureINTERNAL() called in
     AvsDecDecode(), and "dec_cont->StrmStorage.out_count--" may called in
     AvsDecNextPicture() before AvsDecPeek() called, so dec_cont->fullness
     used to sample the real out_count in case of AvsDecNextPictureINTERNAL() called
     before than AvsDecPeek() */
  u32 tmp = dec_cont->fullness;

  if(!tmp ||
      dec_cont->StrmStorage.new_headers_change_resolution) {
    (void) DWLmemset(picture, 0, sizeof(AvsDecPicture));
    return DEC_OK;
  }

  pic_index = dec_cont->StrmStorage.work_out;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = dec_cont->StrmStorage.frame_width << 4;
    picture->pictures[0].frame_height = dec_cont->StrmStorage.frame_height << 4;
    picture->pictures[0].coded_width = dec_cont->Hdrs.horizontal_size;
    picture->pictures[0].coded_height = dec_cont->Hdrs.vertical_size;
  } else {
    picture->pictures[0].frame_width = (dec_cont->StrmStorage.frame_width << 4) >> dec_cont->dscale_shift_x;
    picture->pictures[0].frame_height = (dec_cont->StrmStorage.frame_height << 4) >> dec_cont->dscale_shift_y;
    picture->pictures[0].coded_width = dec_cont->Hdrs.horizontal_size >> dec_cont->dscale_shift_x;
    picture->pictures[0].coded_height = dec_cont->Hdrs.vertical_size >> dec_cont->dscale_shift_y;
  }
  picture->interlaced = !dec_cont->Hdrs.progressive_sequence;

  p_pic = dec_cont->StrmStorage.p_pic_buf + pic_index;
  if (!dec_cont->pp_enabled) {
    picture->pictures[0].output_picture = (u8 *) p_pic->data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic->data.bus_address;
  } else {
    picture->pictures[0].output_picture = (u8 *) p_pic->pp_data->virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic->pp_data->bus_address;
  }
  picture->key_picture = p_pic->pic_type;
  picture->pic_id = p_pic->pic_id;
  picture->decode_id = p_pic->pic_id;
  picture->pic_coding_type = p_pic->pic_code_type;

  picture->repeat_first_field = p_pic->rff;
  picture->repeat_frame_count = p_pic->rfc;
  picture->number_of_err_mbs = p_pic->nbr_err_mbs;
  picture->cycles_per_mb = p_pic->cycles_per_mb;
  picture->pictures[0].output_format = p_pic->tiled_mode ?
                           DEC_OUT_FRM_TILED_4X4 : DEC_OUT_FRM_RASTER_SCAN;

  (void) DWLmemcpy(&picture->time_code,
                   &p_pic->time_code, sizeof(struct DecTime));

  /* frame output */
  picture->field_picture = 0;
  picture->top_field = 0;
  picture->first_field = 0;

  return DEC_PIC_RDY;
}

u32 AvsCycleCount(DecContainer *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->StrmStorage.frame_height << 4, 16) *
             NEXT_MULTIPLE(dec_cont->StrmStorage.frame_width << 4, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->avs_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  return cycles;
}

void AvsSetExternalBufferInfo(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *)dec_inst;
  u32 ext_buffer_size;

  ext_buffer_size = AvsGetRefFrmSize(dec_cont);

  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }

  dec_cont->ext_min_buffer_num = dec_cont->buf_num =  buffers;
  dec_cont->next_buf_size = ext_buffer_size;
}

enum DecRet AvsDecGetBufferInfo(AvsDecInst dec_inst, struct DecBufferInfo *mem_info) {
  DecContainer  * dec_cont = (DecContainer *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;
  u32 i;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    u32 frame_width = dec_cont->StrmStorage.frame_width << 4;
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
         (DWL_DEVMEM_VAILD(mem_info->buf_to_free)));

  return DEC_WAITING_FOR_BUFFER;
}

enum DecRet AvsDecAddBuffer(AvsDecInst dec_inst, struct DWLLinearMem *info) {
  DecContainer *dec_cont;// = (DecContainer *)dec_inst;
  enum DecRet dec_ret = DEC_OK;
  u32 i;// = dec_cont->buffer_index;

  if (dec_inst == NULL)
	  return DEC_PARAM_ERROR;
  dec_cont = (DecContainer *)dec_inst;
  i = dec_cont->buffer_index;

  if(info == NULL ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->buffer_index >= MAX_PIC_BUFFERS)
    /* Too much buffers added. */
    return DEC_EXT_BUFFER_REJECTED;

  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;
  dec_cont->buffer_index++;
  dec_cont->n_ext_buf_size = info->size;


  /* buffer is not enough, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    dec_cont->StrmStorage.p_pic_buf[i].data = *info;
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
      dec_cont->StrmStorage.p_pic_buf[i].data = *info;
      dec_cont->buffer_index++;
    }
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->StrmStorage.ext_buffer_added = 1;
  return dec_ret;
}


void AvsEnterAbortState(DecContainer *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void AvsExistAbortState(DecContainer *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void AvsEmptyBufferQueue(DecContainer *dec_cont) {
  BqueueEmpty(&dec_cont->StrmStorage.bq);
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
}

void AvsStateReset(DecContainer *dec_cont) {
  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  /* Clear internal parameters in DecContainer */
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
  dec_cont->fullness = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->fifo_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->dec_stat = DEC_OK;

  /* Clear internal parameters in DecStrmStorage */
  dec_cont->StrmStorage.valid_pic_header = 0;
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->StrmStorage.strm_dec_ready = 0;
  dec_cont->StrmStorage.valid_sequence = 0;
#endif
  dec_cont->StrmStorage.out_index = 0;
  dec_cont->StrmStorage.out_count = 0;
  dec_cont->StrmStorage.skip_b = 0;
  dec_cont->StrmStorage.prev_pic_coding_type = 0;
  dec_cont->StrmStorage.prev_pic_structure = 0;
  dec_cont->StrmStorage.field_out_index = 1;
  dec_cont->StrmStorage.frame_number = 0;
  dec_cont->StrmStorage.picture_broken = 0;
  dec_cont->StrmStorage.unsupported_features_present = 0;
  dec_cont->StrmStorage.release_buffer = 0;
  dec_cont->StrmStorage.ext_buffer_added = 0;
  dec_cont->StrmStorage.sequence_low_delay = 0;
  dec_cont->StrmStorage.new_headers_change_resolution = 0;
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->StrmStorage.bq.queue_size)
  {
    dec_cont->StrmStorage.bq.queue_size = buffers;
    dec_cont->StrmStorage.num_buffers = buffers;
    dec_cont->ext_min_buffer_num = buffers;
  }
  dec_cont->buffer_index = 0;
#endif
  dec_cont->StrmStorage.future2prev_past_dist = 0;

  /* Clear internal parameters in DecApiStorage */
  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->ApiStorage.first_field = 1;
  dec_cont->ApiStorage.output_other_field = 0;

  (void) DWLmemset(&(dec_cont->StrmDesc), 0, sizeof(DecStrmDesc));
  (void) DWLmemset(&(dec_cont->out_data), 0, sizeof(struct DecOutput));
  (void) DWLmemset(dec_cont->StrmStorage.out_buf, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
   if (!dec_cont->pp_enabled)
    (void) DWLmemset(dec_cont->StrmStorage.p_pic_buf, 0, 16 * sizeof(picture_t));
  (void) DWLmemset(dec_cont->StrmStorage.picture_info, 0, 32 * sizeof(AvsDecPicture));
#endif
#ifdef CLEAR_HDRINFO_IN_SEEK
  (void) DWLmemset(&(dec_cont->Hdrs), 0, sizeof(DecHdrs));
  (void) DWLmemset(&(dec_cont->tmp_hdrs), 0, sizeof(DecHdrs));
  AvsAPI_InitDataStructures(dec_cont);
#endif

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

enum DecRet AvsDecAbort(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *) dec_inst;

  APITRACE("%s","AvsDecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecAbort# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  AvsEnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet AvsDecAbortAfter(AvsDecInst dec_inst) {
  DecContainer *dec_cont = (DecContainer *) dec_inst;

  APITRACE("%s","AvsDecAbortAfter#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","AvsDecAbortAfter# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_stat == DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }
#endif

  /* Stop and release HW */
  if(dec_cont->asic_running) {
    /* stop HW */
    u32 core_id = (dec_cont->vcmd_used) ? dec_cont->cmdbuf_id : dec_cont->core_id;
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->avs_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  AvsEmptyBufferQueue(dec_cont);

  AvsStateReset(dec_cont);

  AvsExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","AvsDecAbortAfter# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet AvsDecSetInfo(AvsDecInst dec_inst,
                        struct AvsDecConfig *dec_cfg) {
  /*@null@ */ DecContainer *dec_cont = (DecContainer *)dec_inst;
  u32 pic_width = dec_cont->Hdrs.horizontal_size;
  u32 pic_height = dec_cont->Hdrs.vertical_size;
  const struct DecHwFeatures *hw_feature = NULL;
  u32 i;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_AVS_DEC);
  if (!hw_feature) {
    APITRACEDEBUG("%s","AvsDecSetInfo# not found any hw_feature.\n");
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

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, hw_feature, 8, dec_cont->Hdrs.progressive_sequence, 0);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
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
  if (CheckPpUnitConfig(hw_feature, pic_width, pic_height, !dec_cont->Hdrs.progressive_sequence,
                        8, dec_cont->Hdrs.chroma_format, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    dec_cont->ppu_cfg[i].pixel_width = 8;
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;
  }

  return (DEC_OK);
}

void AvsCheckBufferRealloc(DecContainer *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (AvsGetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
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
      if (AvsGetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (AvsGetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}


#ifdef CASE_INFO_STAT
void AvsCaseInfoCollect(DecContainer * dec_cont, CaseInfo *case_info) {
  u32 display_width, display_height, frame_width, frame_height;

  frame_width = dec_cont->StrmStorage.frame_width << 4;
  frame_height = dec_cont->StrmStorage.frame_height << 4;
  display_width = dec_cont->Hdrs.horizontal_size;
  display_height = dec_cont->Hdrs.vertical_size;

  if(display_width != frame_width || display_height != frame_height) {
      case_info->crop_flag = 1;
  }

  if(case_info->frame_num == 0) {
    case_info->decode_width = frame_width;
    case_info->decode_height = frame_height;
    case_info->display_width = display_width;
    case_info->display_height = display_height;
    case_info->chroma_format_id = dec_cont->Hdrs.chroma_format;
  } else {
    if(case_info->decode_width != frame_width
    || case_info->decode_height != frame_height) {
      case_info->decode_width = MAX (frame_width , case_info->decode_width);
      case_info->decode_height = MAX (frame_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (display_height, case_info->display_height);
    case_info->display_width = MIN (display_width, case_info->display_width);
    if (case_info->chroma_format_id != dec_cont->Hdrs.chroma_format) {
      case_info->chroma_format_id = dec_cont->Hdrs.chroma_format;
      case_info->chroma_flag = 1;
    }
  }

  case_info->bit_depth = 8;
  case_info->codec = DEC_MODE_AVS;
  case_info->interlace_flag = !dec_cont->Hdrs.progressive_sequence;

  if(dec_cont->StrmStorage.frame_number < 10) {
      switch(dec_cont->Hdrs.pic_coding_type) {
        case IFRAME: case_info->frame_type[dec_cont->StrmStorage.frame_number] = I_FRAME; break;
        case BFRAME: case_info->frame_type[dec_cont->StrmStorage.frame_number] = B_FRAME; break;
        case PFRAME: case_info->frame_type[dec_cont->StrmStorage.frame_number] = P_FRAME; break;
      }
      case_info->fieldmode[dec_cont->StrmStorage.frame_number] = dec_cont->Hdrs.picture_structure == FRAMEPICTURE? 0:1;
  }
  case_info->bitrate += (60 * GetDecRegister(dec_cont->avs_regs, HWIF_STREAM_LEN) * 8/ (case_info->decode_width)
                            * 3840/ (case_info->decode_height)) * 2160/ 1024/ 1024;
  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
    !dec_cont->ApiStorage.first_field)
    case_info->frame_num++;

}
#endif
