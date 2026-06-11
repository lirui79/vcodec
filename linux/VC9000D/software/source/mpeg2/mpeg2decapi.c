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

#include <string.h>
#include "version.h"
#include "basetype.h"
#include "mpeg2hwd_cfg.h"
#include "mpeg2hwd_container.h"
#include "mpeg2hwd_utils.h"
#include "mpeg2hwd_strm.h"
#include "mpeg2decapi.h"
#include "mpeg2decapi_internal.h"
#include "dwl.h"
#include "regdrv.h"
#include "mpeg2hwd_headers.h"
#include "deccfg.h"
#include "workaround.h"
#include "bqueue.h"
#include "tiledref.h"
#include "errorhandling.h"
#include "commonconfig.h"
#include "vpufeature.h"
#include "sw_util.h"
#include "ppu.h"
#include "dec_log.h"
#include "mpeg2hwd_debug.h"
#include "commonfunction.h"

#ifdef MPEG2_ASIC_TRACE
#include "mpeg2asicdbgtrace.h"
#endif

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif

#define MPEG2_BUFFER_UNDEFINED    16

#define ID8170_DEC_TIMEOUT        0xFFU
#define ID8170_DEC_SYSTEM_ERROR   0xFEU
#define ID8170_DEC_HW_RESERVED    0xFDU

#define MPEG2DEC_UPDATE_POUTPUT \
    dec_cont->MbSetDesc.out_data.data_left = \
    DEC_STRM.p_strm_buff_start - dec_cont->MbSetDesc.out_data.strm_curr_pos; \
    (void) DWLmemcpy(output, &dec_cont->MbSetDesc.out_data, \
                             sizeof(struct DecOutput))
#define NON_B_BUT_B_ALLOWED \
   !dec_cont->Hdrs.low_delay && dec_cont->FrameDesc.pic_coding_type != BFRAME

#define MPEG2DEC_IS_FIELD_OUTPUT \
    dec_cont->Hdrs.interlaced && !dec_cont->pp_config_query.deinterlace

#define MPEG2DEC_NON_PIPELINE_AND_B_PICTURE \
    ((!dec_cont->pp_config_query.pipeline_accepted || dec_cont->Hdrs.interlaced) \
    && dec_cont->FrameDesc.pic_coding_type == BFRAME)
void mpeg2RefreshRegs(Mpeg2DecContainer * dec_cont);
void mpeg2FlushRegs(Mpeg2DecContainer * dec_cont);
static enum DecRet Mpeg2HandleVlcModeError(Mpeg2DecContainer * dec_cont, u32 pic_num);
static void mpeg2HandleFrameEnd(Mpeg2DecContainer * dec_cont);
static u32 RunDecoderAsic(Mpeg2DecContainer * dec_cont, addr_t strm_bus_address,
                          const struct DecHwFeatures *hw_feature);
static void Mpeg2FillPicStruct(Mpeg2DecPicture * picture,
                               Mpeg2DecContainer * dec_cont, u32 pic_index);
static u32 Mpeg2SetRegs(Mpeg2DecContainer * dec_cont, addr_t strm_bus_address,
                        const struct DecHwFeatures *hw_feature);
static enum DecRet Mpeg2DecNextPictureINTERNAL(Mpeg2DecInst dec_inst,
    Mpeg2DecPicture * picture, u32 end_of_stream);
static u32 Mpeg2CycleCount(Mpeg2DecContainer *dec_cont);
static void Mpeg2SetExternalBufferInfo(Mpeg2DecInst dec_inst);

static void Mpeg2EnterAbortState(Mpeg2DecContainer *dec_cont);
static void Mpeg2ExistAbortState(Mpeg2DecContainer *dec_cont);
static void Mpeg2EmptyBufferQueue(Mpeg2DecContainer *dec_cont);
static void Mpeg2CheckBufferRealloc(Mpeg2DecContainer *dec_cont);

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void Mpeg2CaseInfoCollect(Mpeg2DecContainer *dec_cont, CaseInfo *case_info);
#endif

#define DEC_DPB_NOT_INITIALIZED      -1

/*------------------------------------------------------------------------------

    Function: Mpeg2DecInit()

        Functional description:
            Initialize decoder software. Function reserves memory for the
            decoder instance.

        Inputs:

        Outputs:
            dec_inst        pointer to initialized instance is stored here

        Returns:
            DEC_OK       successfully initialized the instance
            MPEG2DEC_MEM_FAIL memory allocation failed

------------------------------------------------------------------------------*/
enum DecRet Mpeg2DecInit(Mpeg2DecInst * dec_inst, const void *dwl, struct Mpeg2DecConfig *dec_cfg) {
  /*@null@ */ Mpeg2DecContainer *dec_cont;
  u32 core_mask;
  enum DecRet ret;

  APITRACE("%s","Mpeg2DecInit#\n");
  APITRACE("%s","Mpeg2API_DecoderInit#\n");
  MPEG2FLUSH;

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if (((-1) >> 1) != (-1)) {
    APITRACEERR("%s","MPEG2DecInit# ERROR: Right shift is not signed\n");
    MPEG2FLUSH;
    return (DEC_INITFAIL);
  }
  /*lint -restore */

  if (dec_inst == NULL) {
    APITRACEERR("%s","MPEG2DecInit# ERROR: dec_inst == NULL");
    MPEG2FLUSH;
    return (DEC_PARAM_ERROR);
  }

  *dec_inst = NULL;
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_MPEG2_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","MPEG2DecInit# mpeg2 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }
  dec_cont = (Mpeg2DecContainer *) DWLmalloc(sizeof(Mpeg2DecContainer));
  MPEG2FLUSH;
  if(dec_cont == NULL) {
    APITRACEERR("%s","MPEG2DecInit# Memory allocation failed\n");
    return (DEC_MEMFAIL);
  }
  /* set everything initially zero */
  DWLmemset(dec_cont, 0, sizeof(Mpeg2DecContainer));

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->dwl = dwl;
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_MPEG2_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);

  mpeg2API_InitDataStructures(dec_cont);

  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->ApiStorage.first_field = 1;

  if( dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS )
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;
  dec_cont->StrmStorage.max_num_buffers = dec_cfg->num_frame_buffers;

  dec_cont->mpeg2_regs[0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_MPEG2_DEC);

  dec_cont->error_info = DEC_NO_ERROR;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  SetECPolicy(dec_cfg->error_handling, 0, &dec_cont->error_policy);
  dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;

  SetCommonConfigRegs(dec_cont->mpeg2_regs);

  dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VCD;

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }
  dec_cont->StrmStorage.release_buffer = 0;

  /* Custom DPB modes require tiled support >= 2 */
  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;

  InitWorkarounds(dwl, DEC_MODE_MPEG2, &dec_cont->workarounds);
  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;
  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);

  APITRACEDEBUG("Container %p\n", (void *)dec_cont);
  MPEG2FLUSH;
  APITRACE("%s","Mpeg2DecInit: OK\n");

  *dec_inst = (Mpeg2DecContainer *) dec_cont;

  return (DEC_OK);

err:
  pthread_mutex_destroy(&dec_cont->protect_mutex);
  DWLfree(dec_cont);
  return ret;
}

/*------------------------------------------------------------------------------

    Function: Mpeg2DecGetInfo()

        Functional description:
            This function provides read access to decoder information. This
            function should not be called before Mpeg2DecDecode function has
            indicated that headers are ready.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            DEC_OK            success
            DEC_PARAM_ERROR     invalid parameters

------------------------------------------------------------------------------*/
enum DecRet Mpeg2DecGetInfo(Mpeg2DecInst dec_inst, Mpeg2DecInfo * dec_info) {

#define API_STOR ((Mpeg2DecContainer *)dec_inst)->ApiStorage
#define DEC_FRAMED ((Mpeg2DecContainer *)dec_inst)->FrameDesc
#define DEC_STRM ((Mpeg2DecContainer *)dec_inst)->StrmDesc
#define DEC_STST ((Mpeg2DecContainer *)dec_inst)->StrmStorage
#define DEC_HDRS ((Mpeg2DecContainer *)dec_inst)->Hdrs
#define DEC_REGS ((Mpeg2DecContainer *)dec_inst)->mpeg2_regs

  APITRACE("%s","Mpeg2DecGetInfo#\n");

  if(dec_inst == NULL || dec_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  if(API_STOR.DecStat == UNINIT || API_STOR.DecStat == INITIALIZED) {
    return DEC_HDRS_NOT_RDY;
  }

  dec_info->frame_width = DEC_FRAMED.frame_width << 4;
  dec_info->frame_height = DEC_FRAMED.frame_height << 4;
  dec_info->coded_width = DEC_HDRS.horizontal_size;
  dec_info->coded_height = DEC_HDRS.vertical_size;

  dec_info->profile_and_level_indication = DEC_HDRS.profile_and_level_indication;
  dec_info->colour_description_present_flag = DEC_HDRS.color_description;
  dec_info->video_range = DEC_HDRS.video_range;
  dec_info->colour_primaries = DEC_HDRS.color_primaries;
  dec_info->transfer_characteristics = DEC_HDRS.transfer_characteristics;
  dec_info->matrix_coefficients = DEC_HDRS.matrix_coefficients;
  dec_info->video_format = DEC_HDRS.video_format;
  dec_info->stream_format = DEC_HDRS.mpeg2_stream;
  dec_info->interlaced_sequence = DEC_HDRS.interlaced;
  dec_info->pic_buff_size = 3;
  /*dec_info->multi_buff_pp_size = DEC_HDRS.interlaced ? 1 : 2;*/
  dec_info->multi_buff_pp_size = 2;

  mpeg2DecAspectRatio((Mpeg2DecContainer *) dec_inst, dec_info);

  dec_info->dpb_mode = ((Mpeg2DecContainer *)dec_inst)->dpb_mode;


  if(DEC_HDRS.interlaced &&
      (dec_info->dpb_mode != DEC_DPB_INTERLACED_FIELD)) {
    dec_info->output_format = MPEG2DEC_SEMIPLANAR_YUV420;
  } else {
    dec_info->output_format = MPEG2DEC_TILED_YUV420;
  }


  APITRACE("%s","Mpeg2DecGetInfo: OK\n");
  return (DEC_OK);

#undef API_STOR
#undef DEC_STRM
#undef DEC_FRAMED
#undef DEC_STST
#undef DEC_HDRS
}

/*------------------------------------------------------------------------------

    Function: Mpeg2DecDecode

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

enum DecRet Mpeg2DecDecode(Mpeg2DecInst dec_inst,
                           Mpeg2DecInput * input, struct DecOutput * output) {
#define API_STOR ((Mpeg2DecContainer *)dec_inst)->ApiStorage
#define DEC_STRM ((Mpeg2DecContainer *)dec_inst)->StrmDesc
#define DEC_FRAMED ((Mpeg2DecContainer *)dec_inst)->FrameDesc

  Mpeg2DecContainer *dec_cont;
  enum DecRet internal_ret;
  enum Mpeg2Result strm_dec_result;
  u32 asic_status;
  enum DecRet ret = 0;
  u32 field_rdy = 0;
  u32 error_concealment = 0;
  u32 input_data_len;
  const struct DecHwFeatures *hw_feature = NULL;

  APITRACE("%s","Mpeg2_dec_decode#\n");

  if(input == NULL || output == NULL || dec_inst == NULL) {
    APITRACEERR("%s","Mpeg2DecDecode# ERROR: PARAM_ERROR\n");
    return DEC_PARAM_ERROR;
  }

  dec_cont = ((Mpeg2DecContainer *) dec_inst);
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_MPEG2_DEC);

  /*
   *  Check if decoder is in an incorrect mode
   */
  if(API_STOR.DecStat == UNINIT) {

    APITRACEERR("%s","Mpeg2DecDecode: NOT_INITIALIZED\n");
    return DEC_NOT_INITIALIZED;
  }

  input_data_len = input->data_len;

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      input->stream == NULL || input->stream_bus_address == 0) {
    APITRACEERR("%s","Mpeg2DecDecode# ERROR: PARAM_ERROR\n");
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
    dec_cont->Hdrs.aspect_ratio_info = dec_cont->tmp_hdrs.aspect_ratio_info;
    dec_cont->Hdrs.frame_rate_code = dec_cont->tmp_hdrs.frame_rate_code;
    dec_cont->Hdrs.bit_rate_value = dec_cont->tmp_hdrs.bit_rate_value;
    dec_cont->Hdrs.vbv_buffer_size = dec_cont->tmp_hdrs.vbv_buffer_size;
    dec_cont->Hdrs.constr_parameters = dec_cont->tmp_hdrs.constr_parameters;
    dec_cont->Hdrs.frame_rate_code = dec_cont->tmp_hdrs.frame_rate_code;
    dec_cont->Hdrs.aspect_ratio_info = dec_cont->tmp_hdrs.aspect_ratio_info;
    dec_cont->FrameDesc.frame_width = (dec_cont->tmp_hdrs.horizontal_size + 15) >> 4;
    dec_cont->FrameDesc.frame_height = (dec_cont->tmp_hdrs.vertical_size + 15) >> 4;
    dec_cont->FrameDesc.total_mb_in_frame =
      dec_cont->FrameDesc.frame_width * dec_cont->FrameDesc.frame_height;
  }

  if (API_STOR.DecStat == HEADERSDECODED) {
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    Mpeg2CheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
#endif
        if (dec_cont->StrmStorage.ext_buffer_added) {
          dec_cont->StrmStorage.release_buffer = 1;
          ret = DEC_WAITING_FOR_BUFFER;
        }

        mpeg2FreeBuffers(dec_cont);
        if (!dec_cont->ApiStorage.p_qtable_base.virtual_address) {
          APITRACEDEBUG("%s","Allocate buffers\n");
          MPEG2FLUSH;
          internal_ret = mpeg2AllocateBuffers(dec_cont);
          /* Reset frame number to ensure PP doesn't run in non-pipeline
           * mode during 1st pic of new headers. */
          dec_cont->FrameDesc.frame_number = 0;
          if (internal_ret != DEC_OK) {
            APITRACEERR("%s","ALLOC BUFFER FAIL\n");
            APITRACEERR("%s","Mpeg2DecDecode# MEMFAIL\n");
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
        mpeg2FreeBuffers(dec_cont);
        if(!dec_cont->ApiStorage.p_qtable_base.virtual_address) {
          APITRACEDEBUG("%s","Allocate buffers\n");
          MPEG2FLUSH;
          internal_ret = mpeg2AllocateBuffers(dec_cont);
          /* Reset frame number to ensure PP doesn't run in non-pipeline
           * mode during 1st pic of new headers. */
          dec_cont->FrameDesc.frame_number = 0;
          if(internal_ret != DEC_OK) {
            APITRACEERR("%s","ALLOC BUFFER FAIL\n");
            APITRACEERR("%s","Mpeg2DecDecode# MEMFAIL\n");
            return (internal_ret);
          }
        }
      }
    }
  }

  /*
   *  Update stream structure
   */
  DEC_STRM.p_strm_buff_start = input->stream;
  DEC_STRM.strm_curr_pos = input->stream;
  DEC_STRM.bit_pos_in_word = 0;
  DEC_STRM.strm_buff_size = input_data_len;
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
      if (dec_cont->realloc_ext_buf) {
        dec_cont->buffer_index = 0;
        Mpeg2SetExternalBufferInfo(dec_cont);
        ret =  DEC_WAITING_FOR_BUFFER;
      }
    } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
      ret = DEC_WAITING_FOR_BUFFER;
    } else if (API_STOR.DecStat != HW_PIC_STARTED) {
      strm_dec_result = mpeg2StrmDec_Decode(dec_cont);
      if (dec_cont->unpaired_field ||
          (strm_dec_result != MPEG2_PIC_HDR_RDY &&
           strm_dec_result != MPEG2_END_OF_STREAM)) {
        dec_cont->unpaired_field = 0;
      }
      picture_t *p_pic_buf_cur_out = &dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out];
      switch (strm_dec_result) {
        case MPEG2_PIC_HDR_RDY: {
          /* if type inter predicted and no reference -> error */
          u32 p_ref_check = dec_cont->Hdrs.picture_coding_type==PFRAME &&
                            dec_cont->ApiStorage.first_field &&
                            dec_cont->StrmStorage.work0==INVALID_ANCHOR_PICTURE;

          u32 b_ref_check = dec_cont->Hdrs.picture_coding_type==BFRAME &&
                            (dec_cont->StrmStorage.work0==INVALID_ANCHOR_PICTURE ||
                            (dec_cont->StrmStorage.work1==INVALID_ANCHOR_PICTURE && dec_cont->Hdrs.closed_gop == 0) ||
                            dec_cont->StrmStorage.skip_b ||
                            input->skip_frame==DEC_SKIP_NON_REF); /* mpeg2 B frame will not be used to be reference frame, not support scalable */

          u32 picture_broken = dec_cont->StrmStorage.picture_broken &&
                               (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) &&
                               (dec_cont->Hdrs.picture_coding_type!=IFRAME);
          if(p_ref_check || b_ref_check || picture_broken) {
            if(dec_cont->StrmStorage.skip_b ||
                input->skip_frame == DEC_SKIP_NON_REF) {
              APITRACE("%s","Mpeg2DecDecode# DEC_NONREF_PIC_SKIPPED\n");
            }
            if (!dec_cont->ApiStorage.first_field && dec_cont->pp_enabled)
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_pic_buf_cur_out->pp_data)));
            error_concealment = HANTRO_TRUE;
            ret = Mpeg2HandleVlcModeError(dec_cont, input->pic_id);
          } else
            API_STOR.DecStat = HW_PIC_STARTED;

          if(dec_cont->field_rdy==1 && dec_cont->Hdrs.picture_structure==FRAMEPICTURE) {
            if (dec_cont->pp_enabled)
              InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_pic_buf_cur_out->pp_data)));
          }
          /* Initialize DPB mode */
          if (!dec_cont->Hdrs.progressive_sequence ||
              !dec_cont->Hdrs.frame_pred_frame_dct)
            dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
          else
            dec_cont->dpb_mode = DEC_DPB_FRAME;
          break;
        }
        case MPEG2_PIC_SUPRISE_B: {
          /* Handle suprise B */
          internal_ret = mpeg2DecAllocExtraBPic(dec_cont);
          if(internal_ret != DEC_OK) {
            APITRACEERR("%s","Mpeg2DecDecode# MEMFAIL Mpeg2DecAllocExtraBPic\n");
            return (internal_ret);
          }
          dec_cont->Hdrs.low_delay = 0;

          dec_cont->error_info = DEC_NO_ERROR;
          ret = Mpeg2DecBufferPicture(dec_cont, input->pic_id, HANTRO_FALSE, 0);

          // error_concealment = HANTRO_TRUE;
          // ret = Mpeg2HandleVlcModeError(dec_cont, input->pic_id);
          /* copy output parameters for this PIC */
          MPEG2DEC_UPDATE_POUTPUT;
          break;
      }
        case MPEG2_PIC_HDR_RDY_ERROR: {
          if (!dec_cont->ApiStorage.first_field && dec_cont->pp_enabled)
            InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_pic_buf_cur_out->pp_data)));
          ret = Mpeg2HandleVlcModeError(dec_cont, input->pic_id);
          error_concealment = HANTRO_TRUE;
          /* copy output parameters for this PIC */
          MPEG2DEC_UPDATE_POUTPUT;
          break;
        }
        case MPEG2_HDRS_RDY: {
          {
            /* check for minimum and maximum dimensions */
            SwAdjustCoreMaskByWxH(dec_cont->dwl, dec_cont->FrameDesc.frame_width << 4,
                                  dec_cont->FrameDesc.frame_height << 4, 1,
                                  DWL_CLIENT_TYPE_MPEG2_DEC, &dec_cont->core_mask);
            if (CORE_MASK(dec_cont->core_mask) == 0) {
              APITRACEERR("%s","Mpeg2DecDecode# ERROR: Unsupported size\n");
              return DEC_UNSUPPORTED;
            }
            hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_MPEG2_DEC);
          }
          internal_ret = mpeg2DecCheckSupport(dec_cont);
          if(internal_ret != DEC_OK) {
            dec_cont->StrmStorage.strm_dec_ready = FALSE;
            dec_cont->StrmStorage.valid_sequence = 0;
            API_STOR.DecStat = INITIALIZED;
            return internal_ret;
          }
          /* reset after MPEG2_HDRS_RDY */
          dec_cont->StrmStorage.error_in_hdr = 0;
          if(dec_cont->ApiStorage.first_headers) {
            dec_cont->ApiStorage.first_headers = 0;
            SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_WIDTH_IN_CBS,
                            dec_cont->FrameDesc.frame_width << 1);
            /* check the decoding mode */
            if(dec_cont->Hdrs.mpeg2_stream) {
              SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_MODE,
                            DEC_MODE_MPEG2);
            } else {
              SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_MODE,
                            DEC_MODE_MPEG1);
            }
          }

          /* Initialize DPB mode */
          if(!dec_cont->Hdrs.progressive_sequence ||
              !dec_cont->Hdrs.frame_pred_frame_dct)
            dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
          else
            dec_cont->dpb_mode = DEC_DPB_FRAME;
          /* Initialize tiled mode */
          /* Check mode validity */
          if(DecCheckTiledMode(
                                dec_cont->dpb_mode,
                                !dec_cont->Hdrs.progressive_sequence ||
                                  !dec_cont->Hdrs.frame_pred_frame_dct
                              ) != HANTRO_OK ) {
            APITRACEERR("%s","Mpeg2DecDecode# ERROR: DPB mode does not "\
                          "support tiled reference pictures\n");
            return DEC_PARAM_ERROR;
          }

          /* Handle MPEG-1 parameters */
          if(dec_cont->Hdrs.mpeg2_stream == MPEG1) {
            mpeg2HandleMpeg1Parameters(dec_cont);
          }

          API_STOR.DecStat = HEADERSDECODED;
          if (dec_cont->pp_enabled) {
            dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
            dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
          }

          APITRACEDEBUG("%s","HDRS_RDY\n");
          FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
          ret = DEC_HDRS_RDY;
          break;
        }
        default: {
          ASSERT(strm_dec_result == MPEG2_END_OF_STREAM);
          if (field_rdy) {
            ret = DEC_BUF_EMPTY;
          } else {
            ret = DEC_STRM_PROCESSED;
          }
          break;
        }
      }
    }

    /* picture header properly decoded etc -> start HW */
    if(API_STOR.DecStat == HW_PIC_STARTED) {
      if(dec_cont->ApiStorage.first_field &&
          !dec_cont->asic_running) {
        dec_cont->StrmStorage.work_out_prev = dec_cont->StrmStorage.work_out;
        dec_cont->StrmStorage.work_out = BqueueNext2(
                                           &dec_cont->StrmStorage.bq,
                                           dec_cont->StrmStorage.work0,
                                           dec_cont->Hdrs.low_delay ? BQUEUE_UNUSED : dec_cont->StrmStorage.work1,
                                           BQUEUE_UNUSED,
                                           dec_cont->FrameDesc.pic_coding_type == BFRAME);
        u32 cur_out_index = dec_cont->StrmStorage.work_out;
        picture_t *p_pic_buf_cur_out = &dec_cont->StrmStorage.p_pic_buf[cur_out_index];
        if(cur_out_index == INVALID_ANCHOR_PICTURE) {
          if (dec_cont->abort)
            return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          else {
            ret = DEC_NO_DECODING_BUFFER;
            break;
          }
#endif
        }
        p_pic_buf_cur_out->first_show = 1;
        if (dec_cont->pp_enabled) {
          u32 is_wait = 1;
#ifdef GET_FREE_BUFFER_NON_BLOCK
          is_wait = 0; //default value
#endif
          p_pic_buf_cur_out->pp_data = InputQueueGetBuffer(dec_cont->pp_buffer_queue, is_wait);
          if (p_pic_buf_cur_out->pp_data == NULL) {
            if (dec_cont->abort)
              return DEC_ABORTED;
            else {
              p_pic_buf_cur_out->first_show = 0;
              BqueuePictureRelease(&dec_cont->StrmStorage.bq, cur_out_index);
              ret = DEC_NO_DECODING_BUFFER;
              break;
            }
          }
#ifdef ENABLE_FPGA_VERIFICATION
          DWLLinearMemset(dec_cont->dwl, p_pic_buf_cur_out->pp_data, 0, 0,
                          p_pic_buf_cur_out->pp_data->size);
#endif
        }

        if (dec_cont->workarounds.mpeg.start_code) {
          PrepareStartCodeWorkaround(
            dec_cont->dwl,
            &p_pic_buf_cur_out->data,
            dec_cont->FrameDesc.frame_width,
            dec_cont->FrameDesc.frame_height,
            dec_cont->Hdrs.picture_structure == TOPFIELD,
            dec_cont->dpb_mode);
        }
      }

      DWLDMATransData2(dec_cont->dwl, (addr_t)input->stream_bus_address,
                      (void *)input->stream,
                      input->data_len, HOST_TO_DEVICE);

      asic_status = RunDecoderAsic(dec_cont, input->stream_bus_address, hw_feature);
      /* check start code workout if applicable: if timeout interrupt
       * from HW, but all macroblocks written to output -> assume
       * picture finished -> change to pic rdy. Stream end address
       * indicated by HW is not properly updated, but is handled in
       * mpeg2HandleFrameEnd() */
      if ( (asic_status & DEC_HW_IRQ_TIMEOUT) &&
           dec_cont->workarounds.mpeg.start_code ) {
        if ( ProcessStartCodeWorkaround(
               dec_cont->dwl,
               &dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].data,
               dec_cont->FrameDesc.frame_width,
               dec_cont->FrameDesc.frame_height,
               dec_cont->Hdrs.picture_structure == TOPFIELD,
               dec_cont->dpb_mode) ==
             HANTRO_TRUE ) {
          asic_status &= ~DEC_HW_IRQ_TIMEOUT;
          asic_status |= DEC_HW_IRQ_RDY;
        }
      }

      if(asic_status == ID8170_DEC_TIMEOUT) {
        return DEC_HW_TIMEOUT;
      } else if(asic_status == ID8170_DEC_SYSTEM_ERROR) {
        return DEC_SYSTEM_ERROR;
      } else if(asic_status == ID8170_DEC_HW_RESERVED) {
        return DEC_HW_RESERVED;
      } else if(asic_status & DEC_HW_IRQ_BUS) {
        return DEC_HW_BUS_ERROR;
      } else if( (asic_status & DEC_HW_IRQ_ERROR) ||
                 (asic_status & DEC_HW_IRQ_TIMEOUT) ||
                 (asic_status & DEC_HW_IRQ_ABORT)) {
        if (asic_status & DEC_HW_IRQ_ERROR) {
          APITRACEERR("%s","STREAM ERROR IN HW\n");
        } else if (asic_status & DEC_HW_IRQ_TIMEOUT) {
          APITRACEERR("%s","IRQ TIMEOUT IN HW\n");
        } else {
          APITRACEERR("%s","IRQ ABORT IN HW\n");
        }
        MPEG2FLUSH;

        dec_cont->error_info = DEC_FRAME_ERROR;
        ret = Mpeg2HandleVlcModeError(dec_cont, input->pic_id);
        error_concealment = HANTRO_TRUE;
        MPEG2DEC_UPDATE_POUTPUT;
      } else if(asic_status & DEC_HW_IRQ_BUFFER) {
        mpeg2DecPreparePicReturn(dec_cont);
        ret = DEC_BUF_EMPTY;
      }
      /* HW finished decoding a picture */
      else if(asic_status & DEC_HW_IRQ_RDY) {
        dec_cont->error_info = DEC_NO_ERROR;
        error_concealment = HANTRO_FALSE;
        if (dec_cont->Hdrs.picture_coding_type == IFRAME)
          dec_cont->StrmStorage.picture_broken = HANTRO_FALSE; /* reset error flag */
      } else {
        ASSERT(0);
      }

      u32 is_frame_rdy = 0;
      if ((asic_status & DEC_HW_IRQ_RDY) ||
         (asic_status & DEC_HW_IRQ_ERROR) ||
         (asic_status & DEC_HW_IRQ_TIMEOUT)) {
        if (dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
            !dec_cont->ApiStorage.first_field) {
          is_frame_rdy = asic_status & DEC_HW_IRQ_RDY;
          if (is_frame_rdy) {
            dec_cont->FrameDesc.frame_number++;
            dec_cont->field_rdy = 0;
            mpeg2HandleFrameEnd(dec_cont);
            dec_cont->ApiStorage.first_field = 1;
          }
          /* maybe also need to output when enable ec for stream error and timeout */
          ret = Mpeg2DecBufferPicture(dec_cont, input->pic_id, error_concealment,
                                      Mpeg2CycleCount(dec_cont));

#ifdef FPGA_PERF_AND_BW
          DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info,
                          (dec_cont->FrameDesc.frame_width << 4) *
                          (dec_cont->FrameDesc.frame_height << 4),
                          8);
#endif

          if (is_frame_rdy) {
            if(dec_cont->Hdrs.picture_coding_type != BFRAME) {
              /*if(dec_cont->Hdrs.low_delay == 0)*/
              {
                dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
              }
              dec_cont->StrmStorage.work0 = dec_cont->StrmStorage.work_out;
              if(dec_cont->StrmStorage.skip_b)
                dec_cont->StrmStorage.skip_b--;
            }
          }
        } else {
          field_rdy = 1;
          dec_cont->field_rdy = 1;

          /* Store the buffer index and pic header info for single field output */
          dec_cont->field_hdrs = dec_cont->Hdrs;
          dec_cont->StrmStorage.work_out_field = dec_cont->StrmStorage.work_out;

          mpeg2HandleFrameEnd(dec_cont);
          dec_cont->ApiStorage.first_field = 0;
          dec_cont->ApiStorage.field_pic_id = input->pic_id;
          if((u32)(dec_cont->StrmDesc.strm_curr_pos-dec_cont->StrmDesc.p_strm_buff_start) >= input_data_len) {
            ret = DEC_BUF_EMPTY;
          }
        }
#ifdef CASE_INFO_STAT
        Mpeg2CaseInfoCollect(dec_cont, &case_info);
#endif
        dec_cont->StrmStorage.valid_pic_header = FALSE;
        dec_cont->StrmStorage.valid_pic_ext_header = FALSE;

        /* handle first field indication */
        if(dec_cont->Hdrs.interlaced) {
          if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE)
            dec_cont->Hdrs.field_index++;
          else
            dec_cont->Hdrs.field_index = 1;

          dec_cont->Hdrs.first_field_in_frame++;
        }

        mpeg2DecPreparePicReturn(dec_cont);
        if(is_frame_rdy && dec_cont->Hdrs.picture_coding_type == IFRAME) {
          /* reset error flag */
          dec_cont->StrmStorage.picture_broken = HANTRO_FALSE;
        }
      }

      if(is_frame_rdy || (ret != DEC_STRM_PROCESSED && ret != DEC_BUF_EMPTY) || field_rdy) {
        API_STOR.DecStat = STREAMDECODING;
      }

      if(ret == DEC_PIC_DECODED ||
        (ret == DEC_STRM_PROCESSED && !is_frame_rdy) ||
        ret == DEC_BUF_EMPTY) {
        /* copy output parameters for this PIC (excluding stream pos) */
        dec_cont->MbSetDesc.out_data.strm_curr_pos = output->strm_curr_pos;
        MPEG2DEC_UPDATE_POUTPUT;
      }
    }
  } while(ret == 0);

  if(error_concealment && dec_cont->Hdrs.picture_coding_type != BFRAME) {
    dec_cont->StrmStorage.picture_broken = HANTRO_TRUE;
  }

  APITRACE("%s","Mpeg2DecDecode: Exit\n");
  output->strm_curr_pos = dec_cont->StrmDesc.strm_curr_pos;
  output->strm_curr_bus_address = input->stream_bus_address +
                                  (dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start);
  output->data_left = dec_cont->StrmDesc.strm_buff_size -
                      (output->strm_curr_pos - DEC_STRM.p_strm_buff_start);

  u32 tmpret;
  Mpeg2DecPicture tmp_output;
  do {
    tmpret = Mpeg2DecNextPictureINTERNAL(dec_cont, &tmp_output, 0);
    if(tmpret == DEC_ABORTED)
      return (DEC_ABORTED);
  } while( tmpret == DEC_PIC_RDY);

  if(dec_cont->abort)
    return(DEC_ABORTED);
  else
    return ((enum DecRet) ret);

#undef API_STOR
#undef DEC_STRM
#undef DEC_FRAMED
}

/*------------------------------------------------------------------------------

    Function: Mpeg2DecRelease()

        Functional description:
            Release the decoder instance.

        Inputs:
            dec_inst     Decoder instance

        Outputs:
            none

        Returns:
            none

------------------------------------------------------------------------------*/

void Mpeg2DecRelease(Mpeg2DecInst dec_inst) {
#define API_STOR ((Mpeg2DecContainer *)dec_inst)->ApiStorage
  Mpeg2DecContainer *dec_cont = NULL;
  const void *dwl;

  APITRACEDEBUG("%s","1\n");
  APITRACE("%s","Mpeg2DecRelease#\n");
  if(dec_inst == NULL) {
    APITRACEERR("%s","Mpeg2DecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  dec_cont = ((Mpeg2DecContainer *) dec_inst);
  dwl = dec_cont->dwl;

#ifndef USE_OMXIL_BUFFER
  /* Wait all buffers as unused */
  BqueueWaitNotInUse(&dec_cont->StrmStorage.bq);
#endif

  pthread_mutex_destroy(&dec_cont->protect_mutex);

  if (dec_cont->asic_running) {
    if (dec_cont->vcmd_used) {
      DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    }
    else {
      /* Release HW */
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4, 0);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    }
  }

  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  mpeg2FreeBuffers(dec_cont);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

#ifdef FPGA_PERF_AND_BW
  AveragePerfInfoPrint(&dec_cont->perf_info);
#endif

  DWLfree(dec_cont);

  APITRACE("%s","Mpeg2DecRelease: OK\n");
#undef API_STOR
  (void)dwl;
}


/*------------------------------------------------------------------------------
    Function name   : mpeg2RefreshRegs
    Description     :
    Return type     : void
    Argument        : Mpeg2DecContainer *dec_cont
------------------------------------------------------------------------------*/
void mpeg2RefreshRegs(Mpeg2DecContainer * dec_cont) {
  addr_t i;
  u32 *dec_regs = dec_cont->mpeg2_regs;

  if(dec_cont->vcmd_used) {
    DWLRefreshRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->mpeg2_regs);
  } else {
    for(i = 0; i < DEC_X170_REGISTERS; i++) {
      dec_regs[i] = DWLReadReg(dec_cont->dwl, dec_cont->core_id, 4 * i);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : mpeg2FlushRegs
    Description     :
    Return type     : void
    Argument        : Mpeg2DecContainer *dec_cont
------------------------------------------------------------------------------*/
void mpeg2FlushRegs(Mpeg2DecContainer * dec_cont) {
  i32 i;
  u32 *dec_regs = dec_cont->mpeg2_regs;

  if (dec_cont->vcmd_used) {
    DWLFlushRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->mpeg2_regs,
                     dec_cont->mc_refresh_regs[dec_cont->core_id], dec_cont->core_id);
  }else {
    for(i = 2; i < DEC_X170_REGISTERS; i++) {
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * i, dec_regs[i]);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : Mpeg2HandleVlcModeError
    Description     :
    Return type     : enum DecRet
    Argument        : Mpeg2DecContainer *dec_cont
------------------------------------------------------------------------------*/
enum DecRet Mpeg2HandleVlcModeError(Mpeg2DecContainer * dec_cont, u32 pic_num) {
  enum DecRet ret = DEC_OK, tmp;
  ASSERT(dec_cont->StrmStorage.strm_dec_ready);

  u32 work_out = dec_cont->StrmStorage.work_out;
  tmp = mpeg2StrmDec_NextStartCode(dec_cont);
  if(tmp != END_OF_STREAM) {
    dec_cont->StrmDesc.strm_curr_pos -= 4;
    dec_cont->StrmDesc.strm_buff_read_bits -= 32;
  }

  /* picture freezed due to error in first field -> skip/ignore 2nd field */
  if(dec_cont->Hdrs.picture_structure != FRAMEPICTURE &&
      dec_cont->Hdrs.picture_coding_type != BFRAME &&
      dec_cont->ApiStorage.first_field) {
    dec_cont->ApiStorage.ignore_field = 1;
    dec_cont->ApiStorage.first_field = 0;
  } else
    dec_cont->ApiStorage.first_field = 1;

  dec_cont->ApiStorage.DecStat = STREAMDECODING;
  dec_cont->StrmStorage.valid_pic_header = FALSE;
  dec_cont->StrmStorage.valid_pic_ext_header = FALSE;
  dec_cont->Hdrs.picture_structure = FRAMEPICTURE;

  /* return directly when DEC_EC_OUT_NO_ERROR */
  if(dec_cont->error_policy & DEC_EC_OUT_NO_ERROR) {
    ret = DEC_STRM_PROCESSED;
    return ret;
  }

  /* error in first picture -> set reference to grey */
  if(!dec_cont->FrameDesc.frame_number) {
    /* 1st picture, take care of false header data */
    dec_cont->StrmStorage.error_in_hdr = 1;
    /* Don't do it if first field has been decoded successfully */
    u32 init_value = 128, size = 0;
    if (!dec_cont->field_rdy) {
      u32 out_w, out_h;
      out_w = NEXT_MULTIPLE(4 * dec_cont->FrameDesc.frame_width * 16, ALIGN(dec_cont->align));
      out_h = dec_cont->FrameDesc.frame_height * 4;
      size = out_w * out_h * 3 / 2;
    }
    DWLLinearMemset(dec_cont->dwl, &dec_cont->StrmStorage.p_pic_buf[work_out].data,
                    0, init_value, size);
    mpeg2DecPreparePicReturn(dec_cont);
    /* no pictures finished -> return STRM_PROCESSED */
    if(tmp == END_OF_STREAM)
      ret = DEC_STRM_PROCESSED;

    dec_cont->StrmStorage.work0 = work_out;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work0].error_info = DEC_FRAME_ERROR;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work0].error_ratio = 1 * EC_ROUND_COEFF;
    dec_cont->StrmStorage.skip_b = 2;
  } else {
    if(dec_cont->Hdrs.picture_coding_type != BFRAME) {
      dec_cont->StrmStorage.skip_b = 2;
      dec_cont->FrameDesc.frame_number++;

      // dec_cont->error_info = DEC_FRAME_ERROR;
      // ret = Mpeg2DecBufferPicture(dec_cont, pic_num, HANTRO_TRUE, 0);

      // BqueueDiscard(&dec_cont->StrmStorage.bq, work_out); //TODO whether need?
      dec_cont->StrmStorage.work_out_prev = work_out;
      // dec_cont->StrmStorage.work_out = dec_cont->StrmStorage.work0;

      if(dec_cont->StrmStorage.work1 != INVALID_ANCHOR_PICTURE)
      {
        dec_cont->StrmStorage.work1 = dec_cont->StrmStorage.work0;
        dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work1].error_info = DEC_FRAME_ERROR;
        dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work1].error_ratio = 1 * EC_ROUND_COEFF;
      }
    } else {
      ret = DEC_NONREF_PIC_SKIPPED;
      dec_cont->StrmStorage.work_out_prev = dec_cont->StrmStorage.work0;
    }
  }

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : mpeg2HandleFrameEnd
    Description     :
    Return type     : u32
    Argument        : Mpeg2DecContainer *dec_cont
------------------------------------------------------------------------------*/
void mpeg2HandleFrameEnd(Mpeg2DecContainer * dec_cont) {

  u32 tmp;

  dec_cont->StrmDesc.strm_buff_read_bits =
    8 * (dec_cont->StrmDesc.strm_curr_pos -
         dec_cont->StrmDesc.p_strm_buff_start);
  dec_cont->StrmDesc.bit_pos_in_word = 0;

  do {
    tmp = mpeg2StrmDec_ShowBits(dec_cont, 32);
    if((tmp >> 8) == 0x1)
      break;
  } while(mpeg2StrmDec_FlushBits(dec_cont, 8) == HANTRO_OK);

}

/*------------------------------------------------------------------------------

         Function name: RunDecoderAsic

         Purpose:       Set Asic run lenght and run Asic

         Input:         Mpeg2DecContainer *dec_cont

         Output:        void

------------------------------------------------------------------------------*/
u32 RunDecoderAsic(Mpeg2DecContainer * dec_cont, addr_t strm_bus_address,
                   const struct DecHwFeatures *hw_feature) {
  i32 ret;
  addr_t tmp = 0;
  u32 asic_status = 0;
  addr_t mask;
  u32 irq = 0;
  struct DWLReqInfo info = {0};

  u32 work_out = dec_cont->StrmStorage.work_out;

  MPEG2FLUSH;

  ASSERT(dec_cont->StrmStorage.p_pic_buf[work_out].data.bus_address != 0);
  ASSERT(strm_bus_address != 0);

  mask = 15;

  /* q-tables to buffer */
  mpeg2HandleQTables(dec_cont);

  /* Save frameDesc/Hdr/dpb_mode info for current picture. */
  dec_cont->StrmStorage.p_pic_buf[work_out].FrameDesc = dec_cont->FrameDesc;
  dec_cont->StrmStorage.p_pic_buf[work_out].Hdrs = dec_cont->Hdrs;
  dec_cont->StrmStorage.p_pic_buf[work_out].dpb_mode = dec_cont->dpb_mode;

  if(!dec_cont->asic_running) {
    u32 reserve_ret = 0;
    tmp = Mpeg2SetRegs(dec_cont, strm_bus_address, hw_feature);
    if(tmp == HANTRO_NOK)
      return 0;

    info.core_mask = dec_cont->core_mask;
    info.width = dec_cont->FrameDesc.frame_width*16;
    info.height = dec_cont->FrameDesc.frame_height*16;
    info.owner = (void *)dec_cont;
    if (dec_cont->vcmd_used) {
      dec_cont->core_id = 0;
      reserve_ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_cont->cmdbuf_id);
      UNUSED(reserve_ret);
    } else {
      (void) DWLReserveHw(dec_cont->dwl, &info, &dec_cont->core_id);
    }

    SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_OUT_DIS, 0);
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_FILTERING_DIS, 1);

    dec_cont->asic_running = 1;

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x4, 0);

    mpeg2FlushRegs(dec_cont);

    if (dec_cont->vcmd_used)
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->ppu_cfg, 0);
    else
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->core_id, dec_cont->ppu_cfg, 0);

    /* Enable HW */
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_E, 1);
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->mpeg2_regs[1]);
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

    SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));
    /* amount of stream (as seen by the HW), obtained as amount of stream
     * given by the application subtracted by number of bytes decoded by
     * SW (if strm_bus_address is not 64/128-bit aligned -> adds number of bytes
     * from previous 64/128-bit aligned boundary) */
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_STREAM_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));

    SetDecRegister(dec_cont->mpeg2_regs, HWIF_STRM_BUFFER_LEN,
                   dec_cont->StrmDesc.strm_buff_size -
                   ((tmp & ~(mask)) - strm_bus_address));
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_STRM_START_OFFSET, 0);

    SetDecRegister(dec_cont->mpeg2_regs, HWIF_STRM_START_BIT,
                   dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

    /* This depends on actual register allocation */
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->mpeg2_regs[5]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->mpeg2_regs[6]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->mpeg2_regs[258]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->mpeg2_regs[259]);
    if (IS_LEGACY(dec_cont->mpeg2_regs[0]))
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 12, dec_cont->mpeg2_regs[12]);
    else
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169, dec_cont->mpeg2_regs[169]);
    if (sizeof(addr_t) == 8) {
      if(hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->mpeg2_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, dec_cont->mpeg2_regs[122]);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, dec_cont->mpeg2_regs[168]);
      } else {
        ASSERT(dec_cont->mpeg2_regs[122] == 0);
        ASSERT(dec_cont->mpeg2_regs[168] == 0);
      }
    } else {
      if(hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->mpeg2_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, 0);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, 0);
      }
    }
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->mpeg2_regs[1]);
  }

  /* Wait for HW ready */
  APITRACEDEBUG("%s","Wait for Decoder\n");
  if (dec_cont->vcmd_used)
    ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
  else
    ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id, (u32) DEC_X170_TIMEOUT_LENGTH);

  mpeg2RefreshRegs(dec_cont);

  if(ret == DWL_HW_WAIT_OK) {
    asic_status = GetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_IRQ_STAT);
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
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_IRQ, 0);

    dec_cont->asic_running = 0;
    if (dec_cont->vcmd_used) {
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      UNUSED(irq);
    } else {
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                    dec_cont->mpeg2_regs[1]);
      (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    }
  }

  /* if HW interrupt indicated either BUFFER_EMPTY or
   * MPEG2_RDY -> read stream end pointer and update StrmDesc structure */
  if((asic_status &
      (DEC_HW_IRQ_BUFFER | DEC_HW_IRQ_RDY |
       DEC_HW_IRQ_ERROR | DEC_HW_IRQ_TIMEOUT))) {
    tmp = GET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_RLC_VLC_BASE);

    if ( asic_status &
         (DEC_HW_IRQ_ERROR | DEC_HW_IRQ_TIMEOUT) ) {
      if (tmp > strm_bus_address + 8)
        tmp -= 8;
      else
        tmp = strm_bus_address;
    }

    if((tmp - strm_bus_address) <= dec_cont->StrmDesc.strm_buff_size) {
      dec_cont->StrmDesc.strm_curr_pos =
        dec_cont->StrmDesc.p_strm_buff_start + (tmp - strm_bus_address);
    } else {
      dec_cont->StrmDesc.strm_curr_pos =
        dec_cont->StrmDesc.p_strm_buff_start + dec_cont->StrmDesc.strm_buff_size;
    }
    /* if timeout interrupt and no bytes consumed by HW -> advance one
     * byte to prevent processing current slice again (may only happen
     * in slice-by-slice mode) */
    if ( (asic_status & DEC_HW_IRQ_TIMEOUT) &&
         tmp == strm_bus_address &&
         dec_cont->StrmDesc.strm_curr_pos <
         (dec_cont->StrmDesc.p_strm_buff_start + dec_cont->StrmDesc.strm_buff_size) )
      dec_cont->StrmDesc.strm_curr_pos++;
  }

  dec_cont->StrmDesc.strm_buff_read_bits =
    8 * (dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start);

  dec_cont->StrmDesc.bit_pos_in_word = 0;

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_IRQ_STAT, 0);

  return asic_status;

}

/*------------------------------------------------------------------------------

    Function name: Mpeg2DecNextPicture

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
enum DecRet Mpeg2DecNextPicture(Mpeg2DecInst dec_inst, Mpeg2DecPicture * picture) {
  /* Variables */
  Mpeg2DecContainer *dec_cont;
  i32 ret;

  /* Code */
  //APITRACE("Mpeg2_dec_next_picture#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","Mpeg2DecNextPicture# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (Mpeg2DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecNextPicture# ERROR: Decoder not initialized\n");
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
      APITRACE("%s","Mpeg2DecNextPicture# DEC_END_OF_STREAM\n");
      return DEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      APITRACE("%s","Mpeg2DecNextPicture# DEC_FLUSHED\n");
      return DEC_FLUSHED;
    }

    *picture = dec_cont->StrmStorage.picture_info[i];

    APITRACE("%s","Mpeg2DecNextPicture# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else
    return DEC_ABORTED;
}

/*------------------------------------------------------------------------------

    Function name: Mpeg2DecNextPicture_INTERNAL

    Functional description:
        Push next picture in display order into output fifo if any available.

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK         No picture available.
        DEC_PIC_RDY    Picture ready.
        DEC_PARAM_ERROR     invalid parameters
        DEC_NOT_INITIALIZED   decoder instance not initialized yet

------------------------------------------------------------------------------*/
enum DecRet Mpeg2DecNextPictureINTERNAL(Mpeg2DecInst dec_inst,
    Mpeg2DecPicture * picture, u32 end_of_stream) {
  /* Variables */
  enum DecRet return_value = DEC_PIC_RDY;
  Mpeg2DecContainer *dec_cont;
  u32 pic_index = MPEG2_BUFFER_UNDEFINED;
  u32 min_count;
  /* Code */
  APITRACE("%s","Mpeg2_dec_next_picture_INTERNAL#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","Mpeg2DecNextPictureINTERNAL# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (Mpeg2DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecNextPictureINTERNAL# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  min_count = 0;
  DWLmemset(picture, 0, sizeof(Mpeg2DecPicture));
  if(dec_cont->Hdrs.low_delay == 0 && !end_of_stream &&
      !dec_cont->StrmStorage.new_headers_change_resolution)
    min_count = 1;

  /* this is to prevent post-processing of non-finished pictures in the
   * end of the stream */
  if(end_of_stream && dec_cont->FrameDesc.pic_coding_type == BFRAME) {
    dec_cont->FrameDesc.pic_coding_type = PFRAME;
  }

  /* Nothing to send out */
  if(dec_cont->StrmStorage.out_count <= min_count) {
    (void) DWLmemset(picture, 0, sizeof(Mpeg2DecPicture));
    picture->pictures[0].output_picture = NULL;
    picture->interlaced = dec_cont->Hdrs.interlaced;
    /* print nothing to send out */
    return_value = DEC_OK;
  } else {
    pic_index = dec_cont->StrmStorage.out_index;
    pic_index = dec_cont->StrmStorage.out_buf[pic_index];

    Mpeg2FillPicStruct(picture, dec_cont, pic_index);

    /* field output */
    //if(MPEG2DEC_IS_FIELD_OUTPUT)
    if (!dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.progressive_sequence &&
        !dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.progressive_frame) {
      picture->interlaced = 1;
      picture->field_picture = 1;

      if(!dec_cont->ApiStorage.output_other_field) {
        picture->top_field =
          dec_cont->StrmStorage.p_pic_buf[pic_index].tf ? 1 : 0;
        dec_cont->ApiStorage.output_other_field = 1;
      } else {
        picture->top_field =
          dec_cont->StrmStorage.p_pic_buf[pic_index].tf ? 0 : 1;
        dec_cont->ApiStorage.output_other_field = 0;
        dec_cont->StrmStorage.out_count--;
        dec_cont->StrmStorage.out_index++;
        dec_cont->StrmStorage.out_index &= 15;
      }
    } else {
      /* progressive or deinterlaced frame output */
      picture->interlaced = 0; //dec_cont->StrmStorage.p_pic_buf[pic_index].Hdrs.interlaced;
      picture->top_field = 0;
      picture->field_picture = 0;
      dec_cont->StrmStorage.out_count--;
      dec_cont->StrmStorage.out_index++;
      dec_cont->StrmStorage.out_index &= 15;
    }

    picture->output_other_field = dec_cont->ApiStorage.output_other_field;

    picture->single_field =
      dec_cont->StrmStorage.p_pic_buf[pic_index].sf ? 1 : 0;

    if (picture->single_field)
      picture->top_field = dec_cont->StrmStorage.p_pic_buf[pic_index].ps;


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

    Function name: Mpeg2DecPictureConsumed

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
enum DecRet Mpeg2DecPictureConsumed(Mpeg2DecInst dec_inst, Mpeg2DecPicture * picture) {
  /* Variables */
  Mpeg2DecContainer *dec_cont;
  u32 i;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */
  APITRACE("%s","Mpeg2_dec_picture_consumed#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","Mpeg2DecPictureConsumed# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (Mpeg2DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->StrmStorage.num_buffers; i++) {
      if(picture->pictures[0].output_picture_bus_address == dec_cont->StrmStorage.p_pic_buf[i].data.bus_address) {
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

enum DecRet Mpeg2DecEndOfStream(Mpeg2DecInst dec_inst) {
  Mpeg2DecContainer *dec_cont = (Mpeg2DecContainer *) dec_inst;
  Mpeg2DecPicture output;
  enum DecRet ret;

  APITRACE("%s","Mpeg2DecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  /* Do not do it twice */
  if(dec_cont->dec_stat == DEC_END_OF_STREAM) {
    return (DEC_OK);
  }

  if (dec_cont->vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl, dec_inst);
  } else {
    if(dec_cont->asic_running) {
      /* stop HW */
      SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_E, 0);
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->mpeg2_regs[1] | DEC_IRQ_DISABLE);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
      dec_cont->asic_running = 0;
    }
  }

#if 1
  if (dec_cont->field_rdy) {
    // Single field in current buffer
    dec_cont->StrmStorage.work_out = dec_cont->StrmStorage.work_out_field;
    dec_cont->Hdrs = dec_cont->field_hdrs;
    dec_cont->StrmStorage.p_pic_buf[dec_cont->StrmStorage.work_out].sf = 1;
    dec_cont->error_info = DEC_NO_ERROR;
    ret = Mpeg2DecBufferPicture(dec_cont, dec_cont->ApiStorage.field_pic_id, HANTRO_FALSE,
                                Mpeg2CycleCount(dec_cont));
  }
#endif

  while((ret = Mpeg2DecNextPictureINTERNAL(dec_inst, &output, 1)) == DEC_PIC_RDY);
  if(ret == DEC_ABORTED) {
    return (DEC_ABORTED);
  }

  dec_cont->dec_stat = DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);

  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;

  APITRACE("%s","Mpeg2DecEndOfStream# DEC_OK\n");
  return (DEC_OK);
}

/*----------------------=-------------------------------------------------------

    Function name: Mpeg2FillPicStruct

    Functional description:
        Fill data to output pic description

    Input:
        dec_cont    Decoder container
        picture    Pointer to return value struct

    Return values:
        void

------------------------------------------------------------------------------*/
static void Mpeg2FillPicStruct(Mpeg2DecPicture * picture,
                               Mpeg2DecContainer * dec_cont, u32 pic_index) {
  picture_t *p_pic;

  p_pic = (picture_t *) dec_cont->StrmStorage.p_pic_buf;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;

  if (!dec_cont->pp_enabled) {
    picture->pictures[0].frame_width = p_pic[pic_index].FrameDesc.frame_width << 4;
    picture->pictures[0].frame_height = p_pic[pic_index].FrameDesc.frame_height << 4;
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
      if (ppu_cfg->dec400_enabled)
        PpFillDec400TblInfo(ppu_cfg,
                            p_pic[pic_index].pp_data->virtual_address,
                            p_pic[pic_index].pp_data->bus_address,
                            &picture->pictures[i].dec400_luma_table,
                            &picture->pictures[i].dec400_chroma_table);
    }

  }
  picture->interlaced = p_pic[pic_index].Hdrs.interlaced;

  picture->key_picture = p_pic[pic_index].pic_type;
  picture->pic_id = p_pic[pic_index].pic_id;
  picture->decode_id = p_pic[pic_index].pic_id;
  picture->pic_coding_type[0] = p_pic[pic_index].pic_code_type[0];
  picture->pic_coding_type[1] = p_pic[pic_index].pic_code_type[1];

  /* handle first field indication */
  if(p_pic[pic_index].Hdrs.interlaced) {
    if(p_pic[pic_index].Hdrs.field_out_index)
      p_pic[pic_index].Hdrs.field_out_index = 0;
    else
      p_pic[pic_index].Hdrs.field_out_index = 1;
  }

  picture->first_field = p_pic[pic_index].ff[p_pic[pic_index].Hdrs.field_out_index];
  picture->repeat_first_field = p_pic[pic_index].rff;
  picture->repeat_frame_count = p_pic[pic_index].rfc;
  // picture->error_info = p_pic[pic_index].error_info;
  // picture->error_ratio = p_pic[pic_index].error_ratio;
  picture->number_of_err_mbs = p_pic[pic_index].nbr_err_mbs;
  picture->cycles_per_mb = p_pic[pic_index].cycles_per_mb;
  picture->dpb_mode = p_pic[pic_index].dpb_mode;
  (void) DWLmemcpy(&picture->time_code,
                   &p_pic[pic_index].time_code, sizeof(struct DecTime));
}

/*------------------------------------------------------------------------------

    Function name: Mpeg2SetRegs

    Functional description:
        Set registers

    Input:
        container

    Return values:
        void

------------------------------------------------------------------------------*/
static u32 Mpeg2SetRegs(Mpeg2DecContainer *dec_cont, addr_t strm_bus_address,
                        const struct DecHwFeatures *hw_feature) {
  addr_t tmp = 0;
  addr_t mask;

  u32 work_out = dec_cont->StrmStorage.work_out;

#ifdef _DEC_PP_USAGE
  Mpeg2DecPpUsagePrint(dec_cont, DECPP_UNSPECIFIED, work_out, 1,
                       dec_cont->StrmStorage.latest_id);
#endif

  /*
  if(dec_cont->Hdrs.interlaced)
      SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_OUT_TILED_E, 0);
  */
  mask = 15;

  APITRACEDEBUG("Decoding to index %d \n", work_out);

  /* swReg3 */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_INTERLACE_E,
                 dec_cont->Hdrs.interlaced);

  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_FIELDMODE_E, 0);
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_TOPFIELDFIRST_E,
                   dec_cont->Hdrs.top_field_first);
  } else {
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_FIELDMODE_E, 1);
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_TOPFIELD_E,
                   dec_cont->Hdrs.picture_structure == 1);
#ifdef ASIC_TRACE_SUPPORT
    /* TopFieldFirst will be used for system model to clear PP output buffer at
       the first field in top simulation. */
    if (dec_cont->ApiStorage.first_field)
      SetDecRegister(dec_cont->mpeg2_regs, HWIF_TOPFIELDFIRST_E,
                     dec_cont->Hdrs.picture_structure == 1);
    else
      SetDecRegister(dec_cont->mpeg2_regs, HWIF_TOPFIELDFIRST_E,
                     dec_cont->Hdrs.picture_structure != 1);
#else
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_TOPFIELDFIRST_E,
                   dec_cont->Hdrs.top_field_first);
#endif
  }

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_HEIGHT_IN_CBS,
                  dec_cont->FrameDesc.frame_height << 1);

  if(dec_cont->Hdrs.picture_coding_type == BFRAME || dec_cont->Hdrs.picture_coding_type == DFRAME)  /* ? */
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_B_E, 1);
  else
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_B_E, 0);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_PIC_INTER_E,
                 dec_cont->Hdrs.picture_coding_type == PFRAME ||
                 dec_cont->Hdrs.picture_coding_type == BFRAME ? 1 : 0);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FWD_INTERLACE_E, 0);  /* ??? */

  /* Never write out mvs, as SW doesn't allocate any memory for them */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_WRITE_MVS_E, 0 );

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_ALT_SCAN_E,
                  dec_cont->Hdrs.alternate_scan);

  /* swReg5 */

  /* tmp is strm_bus_address + number of bytes decoded by SW */
  tmp = dec_cont->StrmDesc.strm_curr_pos - dec_cont->StrmDesc.p_strm_buff_start;
  tmp = strm_bus_address + tmp;

  /* bus address must not be zero */
  if(!(tmp & ~(mask))) {
    return 0;
  }

  /* pointer to start of the stream, mask to get the pointer to
   * previous 64/128-bit aligned position */
  SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

  /* amount of stream (as seen by the HW), obtained as amount of
   * stream given by the application subtracted by number of bytes
   * decoded by SW (if strm_bus_address is not 64/128-bit aligned -> adds
   * number of bytes from previous 64/128-bit aligned boundary) */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_STREAM_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_STRM_BUFFER_LEN,
                 dec_cont->StrmDesc.strm_buff_size -
                 ((tmp & ~(mask)) - strm_bus_address));
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_STRM_START_OFFSET, 0);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_STRM_START_BIT,
                 dec_cont->StrmDesc.bit_pos_in_word + 8 * (tmp & (mask)));

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_CH_QP_OFFSET, 0); /* ? */

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_QSCALE_TYPE,
                 dec_cont->Hdrs.quant_type);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_CON_MV_E, dec_cont->Hdrs.concealment_motion_vectors);  /* ? */

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_INTRA_DC_PREC,
                 dec_cont->Hdrs.intra_dc_precision);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_INTRA_VLC_TAB,
                 dec_cont->Hdrs.intra_vlc_format);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FRAME_PRED_DCT,
                 dec_cont->Hdrs.frame_pred_frame_dct);

  /* swReg6 */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_INIT_QP, 1);

  /* swReg12 */
  /*SetDecRegister(dec_cont->mpeg2_regs, HWIF_RLC_VLC_BASE, 0);     */

  picture_t *p_pic_buf = dec_cont->StrmStorage.p_pic_buf;
  /* swReg13 */
  if(dec_cont->Hdrs.picture_structure == TOPFIELD ||
      dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_DEC_OUT_BASE,
                 p_pic_buf[work_out].data.bus_address);
  } else {
    /* start of bottom field line */
    if(dec_cont->dpb_mode == DEC_DPB_FRAME ) {
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_DEC_OUT_BASE,
                   (p_pic_buf[work_out].data.bus_address +
                    ((dec_cont->FrameDesc.frame_width << 4))));
    } else if( dec_cont->dpb_mode == DEC_DPB_INTERLACED_FIELD ) {
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_DEC_OUT_BASE,
                   p_pic_buf[work_out].data.bus_address);
    }
  }
  SetDecRegister( dec_cont->mpeg2_regs, HWIF_DPB_ILACE_MODE,
                  dec_cont->dpb_mode );

  SetDecRegister( dec_cont->mpeg2_regs, HWIF_PP_OUT_E_U, dec_cont->pp_enabled);
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_UNIQUE_ID, UNIQUE_ID(0));

  if (dec_cont->pp_enabled &&
      hw_feature->max_ppu_count) {
    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    u32 bottom_flag = !(dec_cont->Hdrs.picture_structure == FRAMEPICTURE ||
                        dec_cont->Hdrs.picture_structure == 1);
    u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, p_pic_buf[work_out].pp_data);
    struct PpParams pp_args = {hw_feature,
                               dec_cont->ppu_cfg,
                               p_pic_buf[work_out].pp_data,
                               0,
                               bottom_flag,
                               pp_out_ctrl
                              };
    InitPpUnitBoundCoeff(hw_feature, (dec_cont->Hdrs.picture_structure != FRAMEPICTURE), ppu_cfg);
    PPSetRegs(dec_cont->mpeg2_regs, &pp_args);
    /* Warning: only single core are currently supported (core_id = 0) */
    PPSetLancozsScaleRegs(dec_cont->mpeg2_regs, hw_feature, ppu_cfg, 0);
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_PP_IN_FORMAT_U, 1);
  }

  if (dec_cont->enable_3dlut)
    DWLDMATransData(dec_cont->dwl, &dec_cont->ppu_cfg[0].table_3dlut_buffer,
                    0, dec_cont->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);
  /* Stride registers only available since g1v8_2 */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_OUT_Y_STRIDE,
                  NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width * 4 * 16, ALIGN(dec_cont->align)));
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_OUT_C_STRIDE,
                  NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width * 4 * 16, ALIGN(dec_cont->align)));

  u32 work0 = dec_cont->StrmStorage.work0;
  u32 work1 = dec_cont->StrmStorage.work1;
  u32 tmp_fwd = work0;
  /* past anchor set to future anchor if past is invalid (second
   * picture in sequence is B)
   */
  if(dec_cont->Hdrs.picture_coding_type == BFRAME) {
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
    tmp_fwd = (work1 != INVALID_ANCHOR_PICTURE) ? work1 : work0;
  }

  if(dec_cont->Hdrs.picture_structure == FRAMEPICTURE) {
    if(dec_cont->Hdrs.picture_coding_type == BFRAME) { /* ? */
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER0_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER1_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER2_BASE, p_pic_buf[work0].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER3_BASE, p_pic_buf[work0].data.bus_address);
    } else {
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER0_BASE, p_pic_buf[work0].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER1_BASE, p_pic_buf[work0].data.bus_address);
    }
  } else {
    /* past anchor not available -> use current (this results in using the
     * same top or bottom field as reference and output picture base,
     * output is probably corrupted) */
    if(tmp_fwd == INVALID_ANCHOR_PICTURE)
      tmp_fwd = work_out;

    if(dec_cont->ApiStorage.first_field || dec_cont->Hdrs.picture_coding_type == BFRAME) {
      /*
       * if ((dec_cont->Hdrs.picture_structure == 1 &&
       * dec_cont->Hdrs.top_field_first) ||
       * (dec_cont->Hdrs.picture_structure == 2 &&
       * !dec_cont->Hdrs.top_field_first))
       */
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER0_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER1_BASE, p_pic_buf[tmp_fwd].data.bus_address);
    } else if(dec_cont->Hdrs.picture_structure == 1) {
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER0_BASE, p_pic_buf[tmp_fwd].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER1_BASE, p_pic_buf[work_out].data.bus_address);
    } else if(dec_cont->Hdrs.picture_structure == 2) {
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER0_BASE, p_pic_buf[work_out].data.bus_address);
      SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER1_BASE, p_pic_buf[tmp_fwd].data.bus_address);
    }
    SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER2_BASE, p_pic_buf[work0].data.bus_address);
    SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_REFER3_BASE, p_pic_buf[work0].data.bus_address);
  }

  /* swReg18 */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_ALT_SCAN_FLAG_E, dec_cont->Hdrs.alternate_scan);

  /* ? */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FCODE_FWD_HOR, dec_cont->Hdrs.f_code_fwd_hor);
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FCODE_FWD_VER, dec_cont->Hdrs.f_code_fwd_ver);
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FCODE_BWD_HOR, dec_cont->Hdrs.f_code_bwd_hor);
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FCODE_BWD_VER, dec_cont->Hdrs.f_code_bwd_ver);

  if(!dec_cont->Hdrs.mpeg2_stream && dec_cont->Hdrs.f_code[0][0]) {
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_MV_ACCURACY_FWD, 0);
  } else
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_MV_ACCURACY_FWD, 1);

  if(!dec_cont->Hdrs.mpeg2_stream && dec_cont->Hdrs.f_code[1][0]) {
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_MV_ACCURACY_BWD, 0);
  } else
    SetDecRegister(dec_cont->mpeg2_regs, HWIF_MV_ACCURACY_BWD, 1);

  /* swReg40 */
  SET_ADDR_REG(dec_cont->mpeg2_regs, HWIF_QTABLE_BASE,
               dec_cont->ApiStorage.p_qtable_base.bus_address);

  /* swReg48 */
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_STARTMB_X, 0);
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_STARTMB_Y, 0);

  SetDecRegister(dec_cont->mpeg2_regs, HWIF_DEC_OUT_DIS, 0);
  SetDecRegister(dec_cont->mpeg2_regs, HWIF_FILTERING_DIS, 1);

  dec_cont->tiled_reference_enable =
    DecSetupTiledReference( dec_cont->mpeg2_regs,
                            dec_cont->dpb_mode,
                            !dec_cont->Hdrs.progressive_sequence ||
                            !dec_cont->Hdrs.frame_pred_frame_dct
                          );

  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function name: Mpeg2DecPeek

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
enum DecRet Mpeg2DecPeek(Mpeg2DecInst dec_inst, Mpeg2DecPicture * picture) {
  /* Variables */
  enum DecRet return_value = DEC_PIC_RDY;
  Mpeg2DecContainer *dec_cont;
  u32 pic_index = MPEG2_BUFFER_UNDEFINED;

  /* Code */
  APITRACE("%s","Mpeg2_dec_peek#\n");

  /* Check that function input parameters are valid */
  if(picture == NULL) {
    APITRACEERR("%s","Mpeg2DecPeek# ERROR: picture is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (Mpeg2DecContainer *) dec_inst;

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecPeek# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  /* no output pictures available */
  /* when output release thread enabled, Mpeg2DecNextPictureINTERNAL() called in
     Mpeg2DecDecode(), and "dec_cont->StrmStorage.out_count--" may called in
     Mpeg2DecNextPicture() before Mpeg2DecPeek() called, so dec_cont->fullness
     used to sample the real out_count in case of Mpeg2DecNextPicture() called
     before than Mpeg2DecPeek() */
  u32 tmp = dec_cont->fullness;
  if(!tmp || dec_cont->StrmStorage.new_headers_change_resolution) {
    (void) DWLmemset(picture, 0, sizeof(Mpeg2DecPicture));
    picture->pictures[0].output_picture = NULL;
    picture->interlaced = dec_cont->Hdrs.interlaced;
    /* print nothing to send out */
    return_value = DEC_OK;
  } else {
    /* output current (last decoded) picture */
    pic_index = dec_cont->StrmStorage.work_out;
    Mpeg2FillPicStruct(picture, dec_cont, pic_index);
  }

  return return_value;
}

static u32 Mpeg2CycleCount(Mpeg2DecContainer *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->FrameDesc.frame_height << 4, 16) *
             NEXT_MULTIPLE(dec_cont->FrameDesc.frame_width << 4, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->mpeg2_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  return cycles;
}

void Mpeg2SetExternalBufferInfo(Mpeg2DecInst dec_inst) {
  Mpeg2DecContainer *dec_cont = (Mpeg2DecContainer *)dec_inst;
  u32 ext_buffer_size;
  u32 buffers = 3;

  u32 ref_buff_size = mpeg2GetRefFrmSize(dec_cont);
  ext_buffer_size = ref_buff_size;

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

enum DecRet Mpeg2DecGetBufferInfo(Mpeg2DecInst dec_inst, struct DecBufferInfo *mem_info) {
  Mpeg2DecContainer  * dec_cont = (Mpeg2DecContainer *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;
  u32 i;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    u32 frame_width = dec_cont->FrameDesc.frame_width << 4;
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

enum DecRet Mpeg2DecAddBuffer(Mpeg2DecInst dec_inst, struct DWLLinearMem *info) {
  Mpeg2DecContainer *dec_cont;
  enum DecRet dec_ret = DEC_OK;
  u32 i;

  if (dec_inst == NULL || info == NULL)
    return DEC_PARAM_ERROR;
  dec_cont = (Mpeg2DecContainer *)dec_inst;
  i = dec_cont->buffer_index;

  if(X170_CHECK_BUS_ADDRESS(info->bus_address) ||
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

  /* buffer is not enoughm, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    dec_cont->StrmStorage.p_pic_buf[i].data = *info;
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
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

void Mpeg2EnterAbortState(Mpeg2DecContainer *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);

}

void Mpeg2ExistAbortState(Mpeg2DecContainer *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->StrmStorage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void Mpeg2EmptyBufferQueue(Mpeg2DecContainer *dec_cont) {
  BqueueEmpty(&dec_cont->StrmStorage.bq);
  dec_cont->StrmStorage.work_out_prev = 0;
  dec_cont->StrmStorage.work_out = 0;
  dec_cont->StrmStorage.work0 =
    dec_cont->StrmStorage.work1 = INVALID_ANCHOR_PICTURE;
}

void Mpeg2StateReset(Mpeg2DecContainer *dec_cont) {
  u32 buffers = 3;

  buffers = dec_cont->StrmStorage.max_num_buffers;
  if( buffers < 3 )
    buffers = 3;

  /* Clear parameters in decContainer */
  dec_cont->unpaired_field = 0;

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
  dec_cont->dec_stat = DEC_OK;
  dec_cont->field_rdy = 0;

  /* Clear parameters in DecStrmStorage */
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->StrmStorage.strm_dec_ready = 0;
  dec_cont->StrmStorage.valid_pic_header = 0;
  dec_cont->StrmStorage.valid_pic_ext_header = 0;
  dec_cont->StrmStorage.valid_sequence = 0;
#endif
  dec_cont->StrmStorage.error_in_hdr = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->StrmStorage.bq.queue_size = buffers;
  dec_cont->StrmStorage.num_buffers = buffers;
#endif
  dec_cont->StrmStorage.out_index = 0;
  dec_cont->StrmStorage.out_count = 0;
  dec_cont->StrmStorage.skip_b = 0;
  dec_cont->StrmStorage.prev_pic_coding_type = 0;
  dec_cont->StrmStorage.prev_pic_structure = 0;
  dec_cont->StrmStorage.picture_broken = 0;
  dec_cont->StrmStorage.new_headers_change_resolution = 0;
  dec_cont->StrmStorage.release_buffer = 0;
  dec_cont->StrmStorage.ext_buffer_added = 0;

  /* Clear parameters in DecApiStorage */
  dec_cont->ApiStorage.DecStat = INITIALIZED;
  dec_cont->ApiStorage.first_field = 1;
  dec_cont->ApiStorage.output_other_field = 0;
  dec_cont->ApiStorage.ignore_field = 0;
  dec_cont->ApiStorage.parity = 0;

  /* Clear parameters in DecFrameDesc */
  dec_cont->FrameDesc.frame_number = 0;
  dec_cont->FrameDesc.pic_coding_type = 0;
  dec_cont->FrameDesc.field_coding_type[0] = 0;
  dec_cont->FrameDesc.field_coding_type[1] = 0;
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->FrameDesc.frame_time_pictures = 0;
  dec_cont->FrameDesc.time_code_hours = 0;
  dec_cont->FrameDesc.time_code_minutes = 0;
  dec_cont->FrameDesc.time_code_minutes = 0;
#endif

  (void) DWLmemset(&dec_cont->MbSetDesc, 0, sizeof(DecMbSetDesc));
  (void) DWLmemset(&dec_cont->StrmDesc, 0, sizeof(DecStrmDesc));
  (void) DWLmemset(dec_cont->StrmStorage.out_buf, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
  if (!dec_cont->pp_enabled)
    (void) DWLmemset(dec_cont->StrmStorage.p_pic_buf, 0, 16 * sizeof(picture_t));
  (void) DWLmemset(dec_cont->StrmStorage.picture_info, 0, 32 * sizeof(Mpeg2DecPicture));
#endif
#ifdef CLEAR_HDRINFO_IN_SEEK
  (void) DWLmemset(&(dec_cont->Hdrs), 0, sizeof(DecHdrs));
  (void) DWLmemset(&(dec_cont->tmp_hdrs), 0, sizeof(DecHdrs));
  (void) DWLmemset(&(dec_cont->field_hdrs), 0, sizeof(DecHdrs));
  mpeg2API_InitDataStructures(dec_cont);
#endif

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

enum DecRet Mpeg2DecAbort(Mpeg2DecInst dec_inst) {
  Mpeg2DecContainer *dec_cont = (Mpeg2DecContainer *) dec_inst;

  APITRACE("%s","Mpeg2DecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecAbort# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  Mpeg2EnterAbortState(dec_cont);
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","Mpeg2DecAbort# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet Mpeg2DecAbortAfter(Mpeg2DecInst dec_inst) {
  Mpeg2DecContainer *dec_cont = (Mpeg2DecContainer *) dec_inst;
  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL || dec_cont->ApiStorage.DecStat == UNINIT) {
    APITRACEERR("%s","Mpeg2DecAbortAfter# ERROR: Decoder not initialized\n");
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
    u32 core_id = (dec_cont->vcmd_used) ? dec_cont->cmdbuf_id : dec_cont->core_id;
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->mpeg2_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  Mpeg2EmptyBufferQueue(dec_cont);

  Mpeg2StateReset(dec_cont);

  Mpeg2ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","Mpeg2DecAbortAfter# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet Mpeg2DecSetInfo(Mpeg2DecInst dec_inst,
                            struct Mpeg2DecConfig *dec_cfg) {
#define DEC_FRAMED ((Mpeg2DecContainer *)dec_inst)->FrameDesc
  /*@null@ */ Mpeg2DecContainer *dec_cont = (Mpeg2DecContainer *)dec_inst;
  u32 pic_width = DEC_FRAMED.frame_width << 4;
  u32 pic_height = DEC_FRAMED.frame_height << 4;
  u32 i = 0;
  const struct DecHwFeatures *hw_feature = NULL;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_MPEG2_DEC);
  if (!hw_feature) {
    APITRACEDEBUG("%s","Mpeg2DecSetInfo# not found any hw_feature.\n");
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

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, hw_feature, 8, !dec_cont->Hdrs.interlaced, 0);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL) {
      u32 size = LANCZOS_COEFF_BUFFER_SIZE * sizeof(i16);
      dec_cont->ppu_cfg[i].lanczos_table.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(dec_cont->ppu_cfg[i].lanczos_table.mem_type,
                    DWL_MEM_USAGE_IN_PPULANCZOS_TABLE,
                    dec_cont->secure_mode);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(hw_feature, pic_width, pic_height, dec_cont->Hdrs.interlaced,
                        8, dec_cont->Hdrs.chroma_format, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    dec_cont->ppu_cfg[i].pixel_width = 8;
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;
  }

  return (DEC_OK);
}



void Mpeg2CheckBufferRealloc(Mpeg2DecContainer *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (mpeg2GetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
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
      if (mpeg2GetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (mpeg2GetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}


#ifdef CASE_INFO_STAT
void Mpeg2CaseInfoCollect(Mpeg2DecContainer *dec_cont, CaseInfo *case_info) {
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
      case IFRAME: case_info->frame_type[case_info->frame_num] = I_FRAME; break;
      case BFRAME: case_info->frame_type[case_info->frame_num] = B_FRAME; break;
      case PFRAME: case_info->frame_type[case_info->frame_num] = P_FRAME; break;
    }
    case_info->fieldmode[case_info->frame_num] = dec_cont->Hdrs.picture_structure == FRAMEPICTURE? 0 : 1;
    case_info->slice_num[case_info->frame_num]++;
  }

  if(dec_cont->Hdrs.mpeg2_stream)
    case_info->codec = DEC_MODE_MPEG2;
  else
    case_info->codec = DEC_MODE_MPEG1;

  case_info->interlace_flag = dec_cont->Hdrs.interlaced;
  case_info->bit_depth = 8;
  case_info->chroma_format_id = 1;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->mpeg2_regs, HWIF_STREAM_LEN) * 8/ pic_width)
                         * 3840/ pic_height) * 2160/ 1024/ 1024;
  case_info->frame_num++;
}
#endif
