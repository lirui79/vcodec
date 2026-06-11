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

#include "commonconfig.h"
#include "basetype.h"
#include "decapicommon.h"
#include "vp6decapi.h"
#include "version.h"
#include "dwl.h"
#include "vp6hwd_container.h"
#include "vp6hwd_debug.h"
#include "vp6hwd_asic.h"
#include "bqueue.h"
#include "errorhandling.h"
#include "tiledref.h"
#include "vpufeature.h"
#include "sw_util.h"
#include "ppu.h"
#include "dec_log.h"
#include "commonfunction.h"

#ifndef HANTRO_OK
#define HANTRO_OK 0
#endif

#ifndef HANTRO_NOK
#define HANTRO_NOK 1
#endif

#define VP6_MIN_WIDTH  48
#define VP6_MIN_HEIGHT 48
#define VP6_MIN_WIDTH_EN_DTRC  48
#define VP6_MIN_HEIGHT_EN_DTRC 48
#define MAX_PIC_SIZE   4096*4096
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static void VP6SetExternalBufferInfo(VP6DecInst dec_inst);
static i32 FindIndex(VP6DecContainer_t *dec_cont, DWLMemAddr address);
static i32 FindPpIndex(VP6DecContainer_t* dec_cont, DWLMemAddr address);
enum DecRet VP6DecNextPicture_INTERNAL(VP6DecInst dec_inst,
                                     VP6DecPicture * output, u32 end_of_stream);
static u32 VP6CycleCount(VP6DecContainer_t *dec_cont);
static enum DecRet VP6PushOutput(VP6DecContainer_t* dec_cont);

static void VP6EnterAbortState(VP6DecContainer_t *dec_cont);
static void VP6ExistAbortState(VP6DecContainer_t *dec_cont);
static void VP6EmptyBufferQueue(VP6DecContainer_t *dec_cont);
static void VP6CheckBufferRealloc(VP6DecContainer_t* dec_cont);

#ifdef CASE_INFO_STAT
#define DEC_8190_MODE_VP6           0x07U
#include "case_info.h"
static void VP6CaseInfoCollect(VP6DecContainer_t* dec_cont, CaseInfo* case_info);
#endif

/*Function used for EC*/
static enum ECDataState vp6ECGetOutDataAction(VP6DecContainer_t *dec_cont, DecAsicBuffers_t *p_asic_buff);
static void vp6ECMarkOutDataErrInfo(DecAsicBuffers_t *p_asic_buff, u32 asic_status, u32 is_i_frame);
static enum ECDataState vp6ECHandleReconData(VP6DecContainer_t *dec_cont, DecAsicBuffers_t *p_asic_buff, u32 is_update_recon);
static enum ECDataState vp6ECGetInDataAction( VP6DecContainer_t *dec_cont);

/*------------------------------------------------------------------------------
    Function name   : vp6decinit
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst * dec_inst
                     enum DecErrorHandling error_handling
------------------------------------------------------------------------------*/
enum DecRet VP6DecInit(VP6DecInst * dec_inst, const void *dwl, struct VP6DecConfig *dec_cfg) {
  enum DecRet ret;
  VP6DecContainer_t *dec_cont;
  u32 core_mask;

  APITRACE("%s","VP6DecInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if(dec_inst == NULL) {
    APITRACEERR("%s","VP6DecInit# ERROR: dec_inst == NULL\n");
    return (DEC_PARAM_ERROR);
  }

  *dec_inst = NULL;   /* return NULL instance for any error */
  /* check that VP6 decoding supported in HW */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_VP6_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","VP6DecInit# VP6 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  /* allocate instance */
  dec_cont = (VP6DecContainer_t *) DWLmalloc(sizeof(VP6DecContainer_t));

  if(dec_cont == NULL) {
    APITRACEERR("%s","VP6DecInit# ERROR: Memory allocation failed\n");
    ret = DEC_MEMFAIL;
    goto err;
  }

  DWLmemset(dec_cont, 0, sizeof(VP6DecContainer_t));
  dec_cont->dwl = dwl;
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_VP6_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, core_mask, DWL_CLIENT_TYPE_VP6_DEC);

  /* initial setup of instance */
  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->dec_stat = VP6DEC_INITIALIZED;
  dec_cont->checksum = dec_cont;  /* save instance as a checksum */
  if(dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS)
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;
  if(dec_cfg->num_frame_buffers < 3)
    dec_cfg->num_frame_buffers = 3;
  dec_cont->num_buffers = dec_cfg->num_frame_buffers;
  dec_cont->num_buffers_reserved = dec_cfg->num_frame_buffers;

  VP6HwdAsicInit(dec_cont);   /* Init ASIC */

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);

  if(VP6HwdAsicAllocateMem(dec_cont) != 0) {
    APITRACEERR("%s","VP6DecInit# ERROR: ASIC Memory allocation failed\n");
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VCD;

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }
  dec_cont->asic_buff->release_buffer = 0;
  dec_cont->seq_state = SEQ_CLEAN;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  SetECPolicy(dec_cfg->error_handling, 0, &dec_cont->error_policy);

  dec_cont->picture_broken = 0;

  if (FifoInit(16, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;

  dec_cont->dec_stat = VP6DEC_INITIALIZED;

  /* return new instance to application */
  *dec_inst = (VP6DecInst) dec_cont;

  APITRACE("%s","VP6DecInit# OK\n");
  return (DEC_OK);

err:
  if(dec_cont != NULL)
    DWLfree(dec_cont);

  *dec_inst = NULL;
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : VP6DecRelease
    Description     :
    Return type     : void
    Argument        : VP6DecInst dec_inst
------------------------------------------------------------------------------*/
void VP6DecRelease(VP6DecInst dec_inst) {

  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *) dec_inst;
  const void *dwl;

  APITRACE("%s","VP6DecRelease#\n");

  if(dec_cont == NULL) {
    APITRACEERR("%s","VP6DecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecRelease# ERROR: Decoder not initialized\n");
    return;
  }

  /* Wait all buffers as unused */
  BqueueWaitNotInUse(&dec_cont->bq);

  dwl = dec_cont->dwl;

  pthread_mutex_destroy(&dec_cont->protect_mutex);

  if(dec_cont->asic_running) {
    if(dec_cont->vcmd_used) {
      DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    }
    else {
      DWLDisableHw(dwl, dec_cont->core_id, 1 * 4, 0);    /* stop HW */
      DWLReleaseHw(dwl, dec_cont->core_id);  /* release HW lock */
    }
    dec_cont->asic_running = 0;
  }

  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  VP6HwdAsicReleaseMem(dec_cont);
  VP6HwdAsicReleasePictures(dec_cont);
  VP6HWDeleteHuffman(&dec_cont->pb);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

#ifdef FPGA_PERF_AND_BW
  AveragePerfInfoPrint(&dec_cont->perf_info);
#endif

  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  APITRACE("%s","VP6DecRelease# OK\n");

  return;
}

/*------------------------------------------------------------------------------
    Function name   : VP6DecGetInfo
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst dec_inst
    Argument        : VP6DecInfo * dec_info
------------------------------------------------------------------------------*/
enum DecRet VP6DecGetInfo(VP6DecInst dec_inst, VP6DecInfo * dec_info) {
  const VP6DecContainer_t *dec_cont = (VP6DecContainer_t *) dec_inst;

  APITRACE("%s","VP6DecGetInfo#\n");

  if(dec_inst == NULL || dec_info == NULL) {
    APITRACEERR("%s","VP6DecGetInfo# ERROR: dec_inst or dec_info is NULL\n");
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecGetInfo# ERROR: Decoder not initialized\n");
    return DEC_NOT_INITIALIZED;
  }

  dec_info->vp6_version = dec_cont->pb.Vp3VersionNo;
  dec_info->vp6_profile = dec_cont->pb.VpProfile;
  dec_info->pic_buff_size = dec_cont->buf_num;


  dec_info->output_format = VP6DEC_TILED_YUV420;


  /* Fragments have 8 pixels */
  dec_info->frame_width = dec_cont->pb.HFragments * 8;
  dec_info->frame_height = dec_cont->pb.VFragments * 8;
  dec_info->scaled_width = dec_cont->pb.OutputWidth * 8;
  dec_info->scaled_height = dec_cont->pb.OutputHeight * 8;
  dec_info->dpb_mode = DEC_DPB_FRAME;

  dec_info->scaling_mode = dec_cont->pb.ScalingMode;

  return DEC_OK;
}

static enum DecRet FetchPPBuffer(VP6DecContainer_t *dec_cont, const VP6DecInput *input,
                              DecAsicBuffers_t *p_asic_buff, struct DecOutput *output) {
  struct DWLLinearMem *pp_buffer = NULL;
  u32 index;

  index = p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->pp_out_buffer);
#ifdef GET_FREE_BUFFER_NON_BLOCK
  pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
  p_asic_buff->pp_out_buffer[index] = pp_buffer;
  if (pp_buffer == NULL) {
    BqueuePictureRelease(&dec_cont->bq, p_asic_buff->out_buffer_i);
    output->data_left = input->data_len;
    output->strm_curr_pos = (u8 *)input->stream;
    output->strm_curr_bus_address = input->stream_bus_address;
    return DEC_NO_DECODING_BUFFER;
  }
#else
  pp_buffer= InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
  p_asic_buff->pp_out_buffer[index] = pp_buffer;
  if (pp_buffer== NULL)
    return DEC_ABORTED;
#endif

#ifdef ENABLE_FPGA_VERIFICATION
  /* update device buffer by host buffer */
  DWLLinearMemset(dec_cont->dwl, pp_buffer, 0, 0, pp_buffer->size);
#endif
  InputQueueSetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer, PP_OUT_CTRL(input->dec_ctrl));

  return DEC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VP6DecDecode
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst dec_inst
    Argument        : const VP6DecInput * input
    Argument        : VP6DecFrame * output
------------------------------------------------------------------------------*/
enum DecRet VP6DecDecode(VP6DecInst dec_inst,
                       const VP6DecInput * input, struct DecOutput * output) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff = NULL;
  i32 ret;
  u32 asic_status;

  APITRACE("%s","VP6DecDecode#\n");

  /* Check that function input parameters are valid */
  if(input == NULL || output == NULL || dec_inst == NULL) {
    APITRACEERR("%s","VP6DecDecode# ERROR: NULL arg(s)\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecDecode# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  DWLmemset(output, 0, sizeof(struct DecOutput));

  if (dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if(input->data_len == 0 ||
      input->data_len > dec_cont->max_strm_len ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address)) {
    APITRACEERR("%s","VP6DecDecode# ERROR: Invalid arg value\n");
    return DEC_PARAM_ERROR;
  }

#ifdef VP6DEC_EVALUATION
  if(dec_cont->pic_number > VP6DEC_EVALUATION) {
    APITRACEDEBUG("VP6DecDecode# DEC_EVALUATION_LIMIT_EXCEEDED\n");
    return DEC_EVALUATION_LIMIT_EXCEEDED;
  }
#endif
  if(dec_cont->abort)
    return (DEC_ABORTED);

  /* aliases */
  p_asic_buff = dec_cont->asic_buff;
  if (dec_cont->get_buffer_after_abort) {
    p_asic_buff->out_buffer_i = BqueueNext2( &dec_cont->bq,
                                        BQUEUE_UNUSED, BQUEUE_UNUSED,
                                        BQUEUE_UNUSED, 0 );
    if (p_asic_buff->out_buffer_i == INVALID_ANCHOR_PICTURE) {
      if (dec_cont->abort)
        return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else {
        output->data_left = input->data_len;
        output->strm_curr_pos = (u8 *)input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        return DEC_NO_DECODING_BUFFER;
      }
#endif
    }

    p_asic_buff->first_show[p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->first_show)] = 1;

    p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->pictures)];
    ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output);
    if (ret != DEC_OK)
      return ret;
    /* These need to point at something so use the output buffer */
    p_asic_buff->refBuffer        = p_asic_buff->out_buffer;
    p_asic_buff->golden_buffer     = p_asic_buff->out_buffer;
    p_asic_buff->ref_buffer_i      = INVALID_ANCHOR_PICTURE;
    p_asic_buff->golden_buffer_i   = INVALID_ANCHOR_PICTURE;
    dec_cont->get_buffer_after_abort = 0;
  }

  if (dec_cont->no_decoding_buffer) {
    dec_cont->no_decoding_buffer = 0;
    goto request_decoding_buffer;
  }

  if(dec_cont->dec_stat == VP6DEC_NEW_HEADERS) {
    /* we stopped the decoding after noticing new picture size */
    /* continue from where we left */

    /* check if buffer need to be realloced, both external buffer and internal buffer */
    VP6CheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->bq);
#endif
        if (dec_cont->asic_buff->ext_buffer_added) {
          dec_cont->asic_buff->release_buffer = 1;
        }
        VP6HwdAsicReleasePictures(dec_cont);

        if((ret = VP6HwdAsicAllocatePictures(dec_cont)) != 0) {
          APITRACEERR
          ("%s","VP6DecDecode# ERROR: Picture memory allocation failed\n");
          if (ret == -2)
            return DEC_ABORTED;
          else
            return DEC_MEMFAIL;
        }
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->asic_buff->ext_buffer_added) {
          dec_cont->asic_buff->release_buffer = 1;
        }
      }
      if (dec_cont->realloc_int_buf) {
        VP6HwdAsicReleasePictures(dec_cont);

        if((ret = VP6HwdAsicAllocatePictures(dec_cont)) != 0) {
          APITRACEERR
          ("%s","VP6DecDecode# ERROR: Picture memory allocation failed\n");
          if (ret == -2)
            return DEC_ABORTED;
          else
            return DEC_MEMFAIL;
        }
      }
    }
    if (dec_cont->realloc_ext_buf) {
      VP6SetExternalBufferInfo(dec_cont);
      dec_cont->buffer_index = 0;
      dec_cont->dec_stat = VP6DEC_WAITING_BUFFER;
      output->data_left = input->data_len;
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      return DEC_WAITING_FOR_BUFFER;
    }
  }
  else if (dec_cont->dec_stat == VP6DEC_WAITING_BUFFER) {
    if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
      output->data_left = input->data_len;
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      return DEC_WAITING_FOR_BUFFER;
    }

    if (dec_cont->pp_enabled) {
      p_asic_buff->out_buffer_i = BqueueNext2( &dec_cont->bq,
                                  BQUEUE_UNUSED, BQUEUE_UNUSED,
                                  BQUEUE_UNUSED, 0 );

      if (p_asic_buff->out_buffer_i == INVALID_ANCHOR_PICTURE) {
        if (dec_cont->abort)
          return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->data_left = input->data_len;
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          return DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      p_asic_buff->first_show[p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->first_show)] = 1;

      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->pictures)];
      if (dec_cont->pp_enabled) {
        ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output);
        if (ret != DEC_OK)
          return ret;
      }
      /* These need to point at something so use the output buffer */
      p_asic_buff->refBuffer        = p_asic_buff->out_buffer;
      p_asic_buff->golden_buffer     = p_asic_buff->out_buffer;
      p_asic_buff->ref_buffer_i       = INVALID_ANCHOR_PICTURE;
      p_asic_buff->golden_buffer_i    = INVALID_ANCHOR_PICTURE;
    }
    dec_cont->dec_stat = VP6DEC_INITIALIZED;
    goto continue_pic_decode;
  }

  Vp6StrmInit(&dec_cont->pb.strm, input->stream, input->data_len);

  /* update strm base addresses to ASIC */
  p_asic_buff->partition1_base = input->stream_bus_address;
  p_asic_buff->partition2_base = input->stream_bus_address;

  /* decode frame headers */
  ret = VP6HWLoadFrameHeader(&dec_cont->pb);

  if(ret || dec_cont->pb.br.strm_error) {
    APITRACEERR("%s","VP6DecDecode# ERROR: Frame header decoding failed\n");
    dec_cont->seq_state = SEQ_DIRTY;
    return DEC_STRM_ERROR;
  }

  if (dec_cont->seq_state != SEQ_CLEAN) {
    if (dec_cont->pb.FrameType == BASE_FRAME) {
      dec_cont->seq_state = SEQ_CLEAN;
      dec_cont->picture_broken = 0;
    }
    if (vp6ECGetInDataAction(dec_cont) == EC_STATE_DISCARD) {
      output->data_left = 0;
      return DEC_STRM_PROCESSED;
    }
  }

  /* check for picture size change */
  if((dec_cont->width != (dec_cont->pb.HFragments * 8)) ||
      (dec_cont->height != (dec_cont->pb.VFragments * 8))) {

    /* reallocate picture buffers */
    p_asic_buff->width = dec_cont->pb.HFragments * 8;
    p_asic_buff->height = dec_cont->pb.VFragments * 8;
    {
      /* check for minimum and maximum dimensions */
      SwAdjustCoreMaskByWxH(dec_cont->dwl, p_asic_buff->width, p_asic_buff->height, 1,
                            DWL_CLIENT_TYPE_VP6_DEC, &dec_cont->core_mask);
      if (CORE_MASK(dec_cont->core_mask) == 0) {
        APITRACEERR("%s","VP6DecDecode# ERROR: Unsupported size\n");
        dec_cont->dec_stat = VP6DEC_INITIALIZED;
        return DEC_STREAM_NOT_SUPPORTED;
      }
      dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                            DWL_CLIENT_TYPE_VP6_DEC);
    }

    if (dec_cont->pp_enabled) {
      dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
      dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
    }

    dec_cont->prev_width = dec_cont->width;
    dec_cont->prev_height = dec_cont->height;

    dec_cont->width = dec_cont->pb.HFragments * 8;
    dec_cont->height = dec_cont->pb.VFragments * 8;

    dec_cont->dec_stat = VP6DEC_NEW_HEADERS;

    APITRACE("%s","VP6DecDecode# DEC_HDRS_RDY\n");
    FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
    if(dec_cont->abort)
      return DEC_ABORTED;
    else {
      output->data_left = input->data_len;
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      return DEC_HDRS_RDY;
    }
  }

continue_pic_decode:

  /* decode probability updates */
  ret = VP6HWDecodeProbUpdates(&dec_cont->pb);
  if(ret) {
    APITRACEERR
    ("%s","VP6DecDecode# ERROR: Priobability updates decoding failed\n");
    return DEC_STRM_ERROR;
  }
  if (dec_cont->pb.br.strm_error) {
    dec_cont->seq_state = SEQ_DIRTY;
    return DEC_STRM_ERROR;
  }

  /* prepare asic */
  VP6HwdAsicProbUpdate(dec_cont);
  DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->prob_tbl, 0,
                  dec_cont->asic_buff->prob_tbl.size, HOST_TO_DEVICE);

  VP6HwdAsicInitPicture(dec_cont);

  VP6HwdAsicStrmPosUpdate(dec_cont);

  DWLDMATransData2(dec_cont->dwl, (addr_t)input->stream_bus_address,
                  (void *)input->stream, input->data_len, HOST_TO_DEVICE);

  /* run the hardware */
  asic_status = VP6HwdAsicRun(dec_cont);

  /* updata output_data info */
  if (output->strm_curr_pos == NULL) {
    output->data_left = 0;
    output->strm_curr_pos = (u8 *)input->stream + input->data_len;
    output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
  }

  /* Handle system error situations */
  if(asic_status == VP6HWDEC_SYSTEM_TIMEOUT) {
    /* This timeout is DWL(software/os) generated */
    APITRACEERR("%s","VP6DecDecode# DEC_HW_TIMEOUT, SW generated\n");
    return DEC_HW_TIMEOUT;
  } else if(asic_status == VP6HWDEC_SYSTEM_ERROR) {
    APITRACEERR("%s","VP6DecDecode# VP6HWDEC_SYSTEM_ERROR\n");
    return DEC_SYSTEM_ERROR;
  } else if(asic_status == VP6HWDEC_HW_RESERVED) {
    APITRACE("%s","VP6DecDecode# VP6HWDEC_HW_RESERVED\n");
    return DEC_HW_RESERVED;
  }

  /* Handle possible common HW error situations */
  if(asic_status & DEC_HW_IRQ_BUS) {
    APITRACEERR("%s","VP6DecDecode# DEC_HW_BUS_ERROR\n");
    return DEC_HW_BUS_ERROR;
  }

  vp6ECMarkOutDataErrInfo(p_asic_buff, asic_status,
                          dec_cont->pb.FrameType == BASE_FRAME);

  if((asic_status & DEC_HW_IRQ_TIMEOUT) ||
      (asic_status & DEC_HW_IRQ_ERROR) ||
      (asic_status & DEC_HW_IRQ_ABORT)) {

    dec_cont->seq_state = SEQ_DIRTY;
    dec_cont->picture_broken = 1;
    p_asic_buff->error_ratio[p_asic_buff->out_buffer_i] =
                              getG1OutDataErrorRatio(dec_cont->width, dec_cont->height,
                                      GetDecRegister(dec_cont->vp6_regs, HWIF_MB_LOCATION_X),
                                      GetDecRegister(dec_cont->vp6_regs, HWIF_MB_LOCATION_Y));

    if (EC_STATE_DISCARD == vp6ECGetOutDataAction(dec_cont, p_asic_buff)) {
      vp6ECHandleReconData(dec_cont, p_asic_buff, 0);
      BqueueDiscard(&dec_cont->bq, p_asic_buff->out_buffer_i);
      if (dec_cont->pp_enabled)
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_asic_buff->pp_out_buffer[p_asic_buff->out_buffer_i])));
      return DEC_DISCARD_INTERNAL;
    } else {
      vp6ECHandleReconData(dec_cont, p_asic_buff, 1);
      dec_cont->out_count++;
    }
  } else if(asic_status & DEC_HW_IRQ_RDY) {
  } else {
    ASSERT(0);
  }

  if(asic_status & DEC_HW_IRQ_RDY) {
    APITRACEDEBUG("%s","IRQ: PICTURE RDY\n");

#ifdef CASE_INFO_STAT
    VP6CaseInfoCollect(dec_cont, &case_info);
#endif

#ifdef FPGA_PERF_AND_BW
    DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info,
                     dec_cont->pb.HFragments * dec_cont->pb.VFragments * 64, 8);
#endif
    if(dec_cont->pb.FrameType == BASE_FRAME) {
      p_asic_buff->refBuffer = p_asic_buff->out_buffer;
      p_asic_buff->golden_buffer = p_asic_buff->out_buffer;

      p_asic_buff->ref_buffer_i   = p_asic_buff->out_buffer_i;
      p_asic_buff->golden_buffer_i = p_asic_buff->out_buffer_i;

      dec_cont->picture_broken = 0;

    } else if(dec_cont->pb.RefreshGoldenFrame) {
      p_asic_buff->refBuffer = p_asic_buff->out_buffer;
      p_asic_buff->ref_buffer_i   = p_asic_buff->out_buffer_i;
      p_asic_buff->golden_buffer = p_asic_buff->out_buffer;
      p_asic_buff->golden_buffer_i = p_asic_buff->out_buffer_i;
    } else {
      p_asic_buff->refBuffer = p_asic_buff->out_buffer;
      p_asic_buff->ref_buffer_i   = p_asic_buff->out_buffer_i;
    }
    dec_cont->out_count++;
  }

  /* find first free buffer and use it as next output */
  {
    p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
    p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
    p_asic_buff->out_buffer = NULL;

request_decoding_buffer:

    p_asic_buff->out_buffer_i = BqueueNext2( &dec_cont->bq,
                                p_asic_buff->ref_buffer_i,
                                p_asic_buff->golden_buffer_i,
                                BQUEUE_UNUSED, 0);
    if(p_asic_buff->out_buffer_i == INVALID_ANCHOR_PICTURE) {
      if (dec_cont->abort)
        return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else {
        output->data_left = input->data_len;
        output->strm_curr_pos = (u8 *)input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        dec_cont->no_decoding_buffer = 1;
        return DEC_NO_DECODING_BUFFER;
      }
#endif
    }
    p_asic_buff->first_show[p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->first_show)] = 1;
    p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
    p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i % ARRAY_SIZE(p_asic_buff->pictures)];
    if (dec_cont->pp_enabled && (p_asic_buff->out_buffer_i != p_asic_buff->prev_out_buffer_i)) {
      ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output);
      if (ret != DEC_OK)
        return ret;
    }
  }

  dec_cont->pic_number++;

  if (VP6PushOutput(dec_cont) == DEC_ABORTED)
    return DEC_ABORTED;

  APITRACE("%s","VP6DecDecode# DEC_PIC_DECODED\n");
  return DEC_PIC_DECODED;
}

/*------------------------------------------------------------------------------
    Function name   : VP6DecNextPicture
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP6DecNextPicture(VP6DecInst dec_inst, VP6DecPicture * output) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *) dec_inst;
  i32 ret;

 // APITRACEERR("%s","VP6DecNextPicture#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP6DecNextPicture# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecNextPicture# ERROR: Decoder not initialized\n");
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
      APITRACE("%s","VP6DecNextPicture# DEC_END_OF_STREAM\n");
      return DEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      APITRACE("%s","VP6DecNextPicture# DEC_FLUSHED\n");
      return DEC_FLUSHED;
    }

    *output = dec_cont->asic_buff->picture_info[i];
    ECErrorInfoReturn(output->error_info, output->error_ratio, output->pic_id);

    APITRACE("%s","VP6DecNextPicture# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else
    return DEC_ABORTED;

  APITRACE("%s","VP6DecNextPicture# DEC_OK\n");
  return (DEC_OK);

}

/*------------------------------------------------------------------------------
    Function name   : VP6DecNextPicture_INTERNAL
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP6DecNextPicture_INTERNAL(VP6DecInst dec_inst,
                                     VP6DecPicture * output, u32 end_of_stream) {
  VP6DecContainer_t *dec_cont;// = (VP6DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff;// = dec_cont->asic_buff;
  u32 pic_for_output = 0, i;
  u32 buffer_id;
  PpUnitIntConfig *ppu_cfg;// = dec_cont->ppu_cfg;

  APITRACE("%s","VP6DecNextPicture_INTERNAL#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP6DecNextPicture_INTERNAL# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (VP6DecContainer_t *) dec_inst;
  p_asic_buff = dec_cont->asic_buff;
  ppu_cfg = dec_cont->ppu_cfg;

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecNextPicture_INTERNAL# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  pic_for_output = dec_cont->out_count != 0;

  DWLmemset(output, 0, sizeof(VP6DecPicture));
  if (pic_for_output) {
    const struct DWLLinearMem *out_pic = NULL;

    out_pic = p_asic_buff->prev_out_buffer;

    dec_cont->out_count--;

    buffer_id = FindIndex(dec_cont, DWL_GET_DEVMEM_ADDR(*out_pic));
    output->pic_id = dec_cont->asic_buff->decode_id[p_asic_buff->prev_out_buffer_i];

    if(dec_cont->pb.FrameType == BASE_FRAME)
      output->pic_coding_type = DEC_PIC_TYPE_I;
    else
      output->pic_coding_type = DEC_PIC_TYPE_P;

    output->is_intra_frame = 0;
    output->is_golden_frame = 0;
    output->nbr_of_err_mbs = 0;
    output->cycles_per_mb = VP6CycleCount(dec_cont);
#if 0
    output->frame_width = dec_cont->width;
    output->frame_height = dec_cont->height;
#endif
    if (!dec_cont->pp_enabled) {
      output->pictures[0].frame_width = dec_cont->asic_buff->frame_width[buffer_id];
      output->pictures[0].frame_height = dec_cont->asic_buff->frame_height[buffer_id];
      output->pictures[0].coded_width = dec_cont->asic_buff->frame_width[buffer_id];
      output->pictures[0].coded_height = dec_cont->asic_buff->frame_height[buffer_id];
      output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].frame_width * 4,
                                        ALIGN(dec_cont->align));
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
      output->pictures[0].p_output_frame = out_pic->virtual_address;
      output->pictures[0].output_frame_bus_address = out_pic->bus_address;
      output->pictures[0].output_picture_chroma = (u32*)((u8*)out_pic->virtual_address +
          output->pictures[0].pic_stride * output->pictures[0].coded_height / 4);
      output->pictures[0].output_picture_chroma_bus_address = out_pic->bus_address +
          output->pictures[0].pic_stride * output->pictures[0].coded_height / 4;
      output->pictures[0].output_format = DEC_OUT_FRM_YUV420TILE;
      output->pictures[0].crop_params.crop_left_offset = dec_cont->ppu_cfg[0].crop.x;
      output->pictures[0].crop_params.crop_top_offset = dec_cont->ppu_cfg[0].crop.y;
      if (dec_cont->ppu_cfg[0].crop.width == 0)
        output->pictures[0].crop_params.crop_out_width = output->pictures[0].coded_width;
      else
        output->pictures[0].crop_params.crop_out_width = dec_cont->ppu_cfg[0].crop.width;
      if (dec_cont->ppu_cfg[0].crop.height == 0)
        output->pictures[0].crop_params.crop_out_height = output->pictures[0].coded_height;
      else
        output->pictures[0].crop_params.crop_out_height = dec_cont->ppu_cfg[0].crop.height;
    } else {
      u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, p_asic_buff->pp_out_buffer[buffer_id]);
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
        output->pictures[i].p_output_frame = (u32*)((addr_t)p_asic_buff->pp_out_buffer[buffer_id]->virtual_address + ppu_cfg->luma_offset);
        output->pictures[i].output_frame_bus_address = p_asic_buff->pp_out_buffer[buffer_id]->bus_address + ppu_cfg->luma_offset;
        output->pictures[i].output_picture_chroma = (u32*)((addr_t)p_asic_buff->pp_out_buffer[buffer_id]->virtual_address + ppu_cfg->chroma_offset);
        output->pictures[i].output_picture_chroma_bus_address = p_asic_buff->pp_out_buffer[buffer_id]->bus_address + ppu_cfg->chroma_offset;
        output->pictures[i].frame_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
        output->pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
        output->pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
        output->pictures[i].crop_params.crop_left_offset = dec_cont->ppu_cfg[i].crop.x;
        output->pictures[i].crop_params.crop_top_offset = dec_cont->ppu_cfg[i].crop.y;
        if (dec_cont->ppu_cfg[i].crop.width == 0)
          output->pictures[i].crop_params.crop_out_width = dec_cont->ppu_cfg[i].scale.width;
        else
          output->pictures[i].crop_params.crop_out_width = dec_cont->ppu_cfg[i].crop.width;
        if (dec_cont->ppu_cfg[i].crop.height == 0)
          output->pictures[i].crop_params.crop_out_height = dec_cont->ppu_cfg[i].scale.height;
        else
          output->pictures[i].crop_params.crop_out_height = dec_cont->ppu_cfg[i].crop.height;
        if (ppu_cfg->dec400_enabled)
          PpFillDec400TblInfo(ppu_cfg,
                            p_asic_buff->pp_out_buffer[buffer_id]->virtual_address,
                            p_asic_buff->pp_out_buffer[buffer_id]->bus_address,
                            &output->pictures[i].dec400_luma_table,
                            &output->pictures[i].dec400_chroma_table);
      }

    }
    output->decode_id = dec_cont->asic_buff->decode_id[buffer_id];
    output->error_info = dec_cont->asic_buff->error_info[buffer_id];
    output->error_ratio = dec_cont->asic_buff->error_ratio[buffer_id];

#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[buffer_id])
#endif
    {
      /* wait this buffer as unused */
      if (BqueueWaitBufNotInUse(&dec_cont->bq, buffer_id) != HANTRO_OK)
        return DEC_ABORTED;
      dec_cont->asic_buff->not_displayed[buffer_id] = 1;
      dec_cont->asic_buff->first_show[buffer_id] = 0;

      /* set this buffer as used */
      BqueueSetBufferAsUsed(&dec_cont->bq, buffer_id);

      if (dec_cont->pp_enabled) {
        InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,
          DWL_GET_DEVMEM_ADDR(*(dec_cont->asic_buff->pp_out_buffer[buffer_id])));
        InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,
          DWL_GET_DEVMEM_ADDR(*(dec_cont->asic_buff->pp_out_buffer[buffer_id])));
      }
      dec_cont->asic_buff->picture_info[buffer_id] = *output;
      FifoPush(dec_cont->fifo_display, (FifoObject)(addr_t)buffer_id, FIFO_EXCEPTION_DISABLE);
    }

    APITRACE("%s","VP6DecNextPicture_INTERNAL# DEC_PIC_RDY\n");

    return (DEC_PIC_RDY);
  }

  APITRACE("%s","VP6DecNextPicture_INTERNAL# DEC_OK\n");
  return (DEC_OK);

}


/*------------------------------------------------------------------------------
    Function name   : VP6DecPictureConsumed
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP6DecPictureConsumed(VP6DecInst dec_inst, VP6DecPicture * output) {
  VP6DecContainer_t *dec_cont;// = (VP6DecContainer_t *) dec_inst;
  u32 buffer_id;
  u32 i = 0;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;// = dec_cont->ppu_cfg;
  APITRACE("%s","VP6DecPictureConsumed#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP6DecPictureConsumed# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (VP6DecContainer_t *) dec_inst;
  ppu_cfg = dec_cont->ppu_cfg;

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecPictureConsumed# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }
  if (!dec_cont->pp_enabled)
    buffer_id = FindIndex(dec_cont, output->pictures[0].output_frame_bus_address);
  else {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (DWLMemAddr)output->pictures[i].output_frame_bus_address;
        break;
      }
    }
    buffer_id = FindPpIndex(dec_cont, output_picture);
  }

  if (buffer_id >= dec_cont->num_buffers)
    return (DEC_PARAM_ERROR);

  /* Remove the reference to the buffer. */
  if(dec_cont->asic_buff->not_displayed[buffer_id]) {
    dec_cont->asic_buff->not_displayed[buffer_id] = 0;
    BqueuePictureRelease(&dec_cont->bq, buffer_id);
    if (dec_cont->pp_enabled)
      InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
  }
  APITRACE("%s","VP6DecPictureConsumed# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet VP6DecEndOfStream(VP6DecInst dec_inst) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *)dec_inst;
  enum DecRet ret;
  VP6DecPicture output;

  APITRACE("%s","VP6DecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL) {
    APITRACEERR("%s","VP6DecEndOfStream# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecEndOfStream# ERROR: Decoder not initialized\n");
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
      SetDecRegister(dec_cont->vp6_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->vp6_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->vp6_regs, HWIF_DEC_E, 0);
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->vp6_regs[1] | DEC_IRQ_DISABLE);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
      dec_cont->asic_running = 0;
    }
  }

  ret = VP6DecNextPicture_INTERNAL(dec_inst, &output, 1);
  if(ret == DEC_ABORTED) {
    return (DEC_ABORTED);
  }

  dec_cont->dec_stat = DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);

  APITRACE("%s","VP6DecEndOfStream# DEC_OK\n");
  return (DEC_OK);
}

static u32 VP6CycleCount(VP6DecContainer_t *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->height, 16) *
             NEXT_MULTIPLE(dec_cont->width, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->vp6_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  return cycles;
}

static enum DecRet VP6PushOutput(VP6DecContainer_t* dec_cont) {
  enum DecRet ret;
  VP6DecPicture output;

  /* Sample dec_cont->out_count for Peek */
  dec_cont->fullness = dec_cont->out_count;
  ret = VP6DecNextPicture_INTERNAL(dec_cont, &output, 0);
  if(ret == DEC_ABORTED)
    return (DEC_ABORTED);
  else
    return ret;
}

static i32 FindIndex(VP6DecContainer_t* dec_cont, DWLMemAddr address) {
  i32 i;

  for (i = 0; i < (i32)dec_cont->num_buffers; i++) {
    if (DWL_DEVMEM_COMPARE(dec_cont->asic_buff->pictures[i], address))
      break;
  }
  return i;
}

static i32 FindPpIndex(VP6DecContainer_t* dec_cont, DWLMemAddr address) {
  i32 i;

  for (i = 0; i < (i32)dec_cont->num_buffers; i++) {
    if (DWL_DEVMEM_IN_RANGE(*(dec_cont->asic_buff->pp_out_buffer[i]), address) &&
        (dec_cont->asic_buff->not_displayed[i]))
      break;
  }
  return i;
}
/*------------------------------------------------------------------------------
    Function name   : VP6DecPeek
    Description     :
    Return type     : enum DecRet
    Argument        : VP6DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP6DecPeek(VP6DecInst dec_inst, VP6DecPicture * output) {
  VP6DecContainer_t *dec_cont;// = (VP6DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff;// = dec_cont->asic_buff;
  u32 buffer_id;
  const struct DWLLinearMem *out_pic = NULL;

  APITRACE("%s","VP6DecPeek#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP6DecPeek# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (VP6DecContainer_t *) dec_inst;
  p_asic_buff = dec_cont->asic_buff;

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecPeek# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  /* when output release thread enabled, VP6DecNextPicture_INTERNAL() called in
     VP6DecDecode(), and "dec_cont->out_count--" may called in VP6DecNextPicture()
     before VP6DecPeek() called, so dec_cont->fullness used to sample the real
     out_count in case of VP6DecNextPicture_INTERNAL() called before than VP6DecPeek() */
  u32 tmp = dec_cont->fullness;

  if (tmp == 0) {
    (void)DWLmemset(output, 0, sizeof(VP6DecPicture));
    return DEC_OK;
  }

  out_pic = p_asic_buff->prev_out_buffer;

  buffer_id = FindIndex(dec_cont, DWL_GET_DEVMEM_ADDR(*out_pic));
  if (!dec_cont->pp_enabled) {
    output->pictures[0].p_output_frame = out_pic->virtual_address;
    output->pictures[0].output_frame_bus_address = out_pic->bus_address;
  } else {
    output->pictures[0].p_output_frame = p_asic_buff->pp_out_buffer[buffer_id]->virtual_address;
    output->pictures[0].output_frame_bus_address = p_asic_buff->pp_out_buffer[buffer_id]->bus_address;
  }
  output->pic_id = dec_cont->asic_buff->decode_id[p_asic_buff->prev_out_buffer_i];
  output->decode_id = dec_cont->asic_buff->decode_id[p_asic_buff->prev_out_buffer_i];
  if(dec_cont->pb.FrameType == BASE_FRAME)
    output->pic_coding_type = DEC_PIC_TYPE_I;
  else
    output->pic_coding_type = DEC_PIC_TYPE_P;
  output->is_intra_frame = 0;
  output->is_golden_frame = 0;
  output->nbr_of_err_mbs = 0;
  output->cycles_per_mb = VP6CycleCount(dec_cont);

  if (!dec_cont->pp_enabled) {
    output->pictures[0].frame_width = dec_cont->width;
    output->pictures[0].frame_height = dec_cont->height;
  } else {
    output->pictures[0].frame_width = dec_cont->width >> dec_cont->dscale_shift_x;
    output->pictures[0].frame_height = dec_cont->height >> dec_cont->dscale_shift_y;
  }

  APITRACE("%s","VP6DecPeek# DEC_PIC_RDY\n");

  return (DEC_PIC_RDY);

}

void VP6SetExternalBufferInfo(VP6DecInst dec_inst) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *)dec_inst;
  u32 ext_buffer_size;

  ext_buffer_size = VP6GetRefFrmSize(dec_cont);
  if (dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }
  dec_cont->ext_min_buffer_num = dec_cont->buf_num = dec_cont->num_buffers;
  dec_cont->next_buf_size = ext_buffer_size;
}

enum DecRet VP6DecGetBufferInfo(VP6DecInst dec_inst, struct DecBufferInfo *mem_info) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;
  u32 i;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));
  if (!dec_cont->pp_enabled) {
    u32 frame_width = dec_cont->width;
    mem_info->cstride[0] = mem_info->ystride[0] =
                           NEXT_MULTIPLE(frame_width * 4, ALIGN(dec_cont->align));
  } else {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!dec_cont->ppu_cfg[i].enabled) continue;
      mem_info->ystride[i] = dec_cont->ppu_cfg[i].ystride;
      mem_info->cstride[i] = dec_cont->ppu_cfg[i].cstride;
    }
  }

  if (dec_cont->asic_buff->release_buffer) {
    /* Release old buffers from input queue. */
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
      /* All buffers have been released. */
      dec_cont->asic_buff->release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL) {
        return (DEC_MEMFAIL);
      }
      dec_cont->asic_buff->ext_buffer_added = 0;
      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      //return DEC_OK;
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

  return DEC_OK;
}

enum DecRet VP6DecAddBuffer(VP6DecInst dec_inst, struct DWLLinearMem *info) {
  VP6DecContainer_t *dec_cont;// = (VP6DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff;// = dec_cont->asic_buff;
  enum DecRet dec_ret = DEC_OK;

  if (dec_inst == NULL)
  	return DEC_PARAM_ERROR;
  dec_cont = (VP6DecContainer_t *)dec_inst;
  p_asic_buff = dec_cont->asic_buff;

  if(info == NULL ||
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

  /* buffer is not enough, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    p_asic_buff->pictures[i] = *info;
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->asic_buff->ext_buffer_added = 1;
  return dec_ret;
}

void VP6EnterAbortState(VP6DecContainer_t *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void VP6ExistAbortState(VP6DecContainer_t *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void VP6EmptyBufferQueue(VP6DecContainer_t *dec_cont) {
  BqueueEmpty(&dec_cont->bq);
}

void VP6StateReset(VP6DecContainer_t *dec_cont) {
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 buffers = dec_cont->num_buffers_reserved;

/* Clear internal parameters in VP6DecContainer */
  dec_cont->dec_stat = VP6DEC_INITIALIZED;
  dec_cont->pic_number = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->bq.queue_size = buffers;
  dec_cont->num_buffers = buffers;
#endif
  dec_cont->fullness = 0;
  dec_cont->out_count = 0;
  dec_cont->get_buffer_after_abort = 1;

  /* Clear internal parameters in DecAsicBuffers */
  (void) DWLmemset(p_asic_buff->decode_id, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->first_show, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
  (void) DWLmemset(p_asic_buff->picture_info, 0, 16 * sizeof(VP6DecPicture));
#endif

  p_asic_buff->whole_pic_concealed = 0;
  p_asic_buff->release_buffer = 0;
  p_asic_buff->ext_buffer_added = 0;
  p_asic_buff->prev_out_buffer = NULL;
  p_asic_buff->ref_buffer_i       = INVALID_ANCHOR_PICTURE;
  p_asic_buff->golden_buffer_i    = INVALID_ANCHOR_PICTURE;
  p_asic_buff->prev_out_buffer_i  = INVALID_ANCHOR_PICTURE;
  p_asic_buff->out_buffer_i = 0;
  p_asic_buff->out_buffer = NULL;
  p_asic_buff->prev_out_buffer = NULL;

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(16, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
  (void)buffers;
}

enum DecRet VP6DecAbort(VP6DecInst dec_inst) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *) dec_inst;

  APITRACE("%s","VP6DecAbort#\n");
  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL) {
    APITRACEERR("%s","VP6DecAbort# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecAbort# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }
  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  VP6EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet VP6DecAbortAfter(VP6DecInst dec_inst) {
  VP6DecContainer_t *dec_cont = (VP6DecContainer_t *) dec_inst;

  APITRACE("%s","VP6DecAbortAfter#\n");
  /* Check if decoder is in an incorrect mode */
  if(dec_inst == NULL) {
    APITRACEERR("%s","VP6DecAbortAfter# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP6DecAbortAfter# ERROR: Decoder not initialized\n");
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
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->vp6_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  VP6EmptyBufferQueue(dec_cont);

  VP6StateReset(dec_cont);

  VP6ExistAbortState(dec_cont);
  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","VP6DecAbortAfter# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet VP6DecSetInfo(VP6DecInst dec_inst,
                        struct VP6DecConfig *dec_cfg) {
  /*@null@ */ VP6DecContainer_t *dec_cont = (VP6DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 pic_width = p_asic_buff->width;
  u32 pic_height = p_asic_buff->height;
  u32 i;
  const struct DecHwFeatures *hw_feature = NULL;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_VP6_DEC);
  dec_cont->hw_feature = hw_feature;
  if (!hw_feature) {
    APITRACEDEBUG("%s","VP6DecSetInfo# not found any hw_feature.\n");
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

void VP6CheckBufferRealloc(VP6DecContainer_t *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (VP6GetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->prev_width != dec_cont->width ||
          dec_cont->prev_height != dec_cont->height)
        dec_cont->realloc_ext_buf = 1;
    }

    dec_cont->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
      if (VP6GetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (VP6GetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}

#ifdef CASE_INFO_STAT
void VP6CaseInfoCollect(VP6DecContainer_t *dec_cont, CaseInfo* case_info)
{
  u32 display_width, display_height, frame_width, frame_height;
  display_width = dec_cont->pb.HFragments * 8;
  display_height = dec_cont->pb.VFragments * 8;
  frame_width = dec_cont->pb.HFragments * 8;
  frame_height = dec_cont->pb.VFragments * 8;

  if(frame_width!=display_width || frame_height!=display_height)
    case_info->crop_flag = 1;

  if (case_info->frame_num < 10){
    if (dec_cont->pb.FrameType == BASE_FRAME)
      case_info->frame_type[case_info->frame_num] = I_FRAME;
    else
      case_info->frame_type[case_info->frame_num] = P_FRAME;
  }
  if(case_info->frame_num == 0) {
    case_info->decode_width = frame_width;
    case_info->decode_height = frame_height;
    case_info->display_width = display_width;
    case_info->display_height = display_height;
  } else {
    if(case_info->decode_width != frame_width
    || case_info->decode_height != frame_height) {
      case_info->decode_width = MAX (frame_width , case_info->decode_width);
      case_info->decode_height = MAX (frame_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (display_height, case_info->display_height);
    case_info->display_width = MIN (display_width, case_info->display_width);
    case_info->slice_num[case_info->frame_num] ++;
  }
  case_info->codec = DEC_MODE_VP6;
  case_info->bit_depth = 8;
  case_info->chroma_format_id = 1;
  case_info->frame_num ++;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->vp6_regs, HWIF_STREAM_LEN) * 8/ frame_width)
                         * 3840/ frame_height) * 2160/ 1024/ 1024;
}
#endif

static enum ECDataState vp6ECGetInDataAction( VP6DecContainer_t *dec_cont) {
  enum ECDataState re = EC_STATE_NONE;
  if (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) {
    re = EC_STATE_DISCARD;
  } else if (dec_cont->error_policy & DEC_EC_NO_SKIP) {
    re = EC_STATE_NONE;
  } else {
    //stub
  }
  return re;
}

static enum ECDataState vp6ECHandleReconData(VP6DecContainer_t *dec_cont, DecAsicBuffers_t *p_asic_buff,
    u32 is_update_recon) {
  if (!dec_cont->pic_number) {
    DWLLinearMemset(dec_cont->dwl, p_asic_buff->refBuffer, 0, 128, p_asic_buff->refBuffer->size);
    if (dec_cont->pp_enabled) {
      DWLLinearMemset(dec_cont->dwl, p_asic_buff->pp_out_buffer[p_asic_buff->out_buffer_i], 0, 128,
                      p_asic_buff->pp_out_buffer[p_asic_buff->out_buffer_i]->size);
    }
  }
  if (is_update_recon) {
    if(dec_cont->pb.FrameType == BASE_FRAME) {
      p_asic_buff->refBuffer = p_asic_buff->out_buffer;
      p_asic_buff->golden_buffer = p_asic_buff->out_buffer;
      p_asic_buff->ref_buffer_i   = p_asic_buff->out_buffer_i;
      p_asic_buff->golden_buffer_i = p_asic_buff->out_buffer_i;

    } else if(dec_cont->pb.RefreshGoldenFrame) {
      p_asic_buff->refBuffer = p_asic_buff->out_buffer;
      p_asic_buff->ref_buffer_i   = p_asic_buff->out_buffer_i;
      p_asic_buff->golden_buffer = p_asic_buff->out_buffer;
      p_asic_buff->golden_buffer_i = p_asic_buff->out_buffer_i;
    } else {
      p_asic_buff->refBuffer = p_asic_buff->out_buffer;
      p_asic_buff->ref_buffer_i   = p_asic_buff->out_buffer_i;
    }
  } else {
  //roughly replace reference buffer by golden buffer
    p_asic_buff->refBuffer    = p_asic_buff->golden_buffer;
    p_asic_buff->ref_buffer_i = p_asic_buff->golden_buffer_i;
  }

  return EC_STATE_REPLACE;
}

static void vp6ECMarkOutDataErrInfo(DecAsicBuffers_t *p_asic_buff, u32 asic_status, u32 is_i_frame) {
  u32 error_info = DEC_NO_ERROR;
  u32 is_ref_has_error = is_i_frame ? 0 :
                            ((p_asic_buff->error_info[p_asic_buff->ref_buffer_i] != DEC_NO_ERROR) ||
                             (p_asic_buff->error_info[p_asic_buff->golden_buffer_i] != DEC_NO_ERROR));

  if((asic_status & DEC_HW_IRQ_TIMEOUT) ||
     (asic_status & DEC_HW_IRQ_ERROR) ||
     (asic_status & DEC_HW_IRQ_ABORT)) {
    error_info = DEC_FRAME_ERROR;
  }

  if (asic_status & DEC_HW_IRQ_RDY) {
      error_info = DEC_NO_ERROR;
  }

  if (is_ref_has_error)
    error_info |= DEC_REF_ERROR;

  p_asic_buff->error_info[p_asic_buff->out_buffer_i] = error_info;
}

static enum ECDataState vp6ECGetOutDataAction(VP6DecContainer_t *dec_cont, DecAsicBuffers_t *p_asic_buff) {
  enum DecErrorInfo error_info = p_asic_buff->error_info[p_asic_buff->out_buffer_i];
  u32 error_ratio = p_asic_buff->error_ratio[p_asic_buff->out_buffer_i];

  u32 discard_error_pic = IsECDropOutput(dec_cont->error_policy,
                                         dec_cont->error_ratio,
                                         error_info, error_ratio);

  return discard_error_pic ? EC_STATE_DISCARD : EC_STATE_NONE;
}
