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
#include "decapicommon.h"
#include "vp8decapi.h"
#include "vp8decmc_internals.h"
#include "version.h"
#include "dwl.h"
#include "vp8hwd_buffer_queue.h"
#include "vp8hwd_container.h"
#include "vp8hwd_debug.h"
#include "tiledref.h"
#include "vp8hwd_asic.h"
#include "regdrv.h"
#include "vp8hwd_asic.h"
#include "vp8hwd_headers.h"
#include "errorhandling.h"
#include "vpufeature.h"
#include "ppu.h"
#include <stdio.h>
#include "sw_util.h"
#include "commonconfig.h"
#include "dec_log.h"
#include "commonfunction.h"

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#include <stdio.h>
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif


#define MB_MULTIPLE(x)  (((x)+15)&~15)

#define VP8_MIN_WIDTH  48
#define VP8_MIN_HEIGHT 48
#define VP8_MIN_WIDTH_EN_DTRC  48
#define VP8_MIN_HEIGHT_EN_DTRC 48
#define MAX_PIC_SIZE   4096*4096

#define EOS_MARKER (-1)
#define FLUSH_MARKER (-2)

static void vp8hwdFreeze(VP8DecContainer_t *dec_cont);
static u32 CheckBitstreamWorkaround(vp8_decoder_t* dec);
#if 0
static void DoBitstreamWorkaround(vp8_decoder_t* dec, DecAsicBuffers_t *p_asic_buff, vpBoolCoder_t*bc);
#endif
void vp8hwdHwErrorConceal(VP8DecContainer_t *dec_cont, addr_t bus_address,
                        u32 conceal_everything);
static struct DWLLinearMem* GetPrevRef(VP8DecContainer_t *dec_cont);
void ConcealRefAvailability(u32 * output, u32 height, u32 width, u32 align);

i32 FindIndex(VP8DecContainer_t* dec_cont, DWLMemAddr address);
i32 FindPpIndex(VP8DecContainer_t* dec_cont, DWLMemAddr address);
static enum DecRet VP8DecNextPicture_INTERNAL(VP8DecInst dec_inst,
    VP8DecPicture * output, u32 end_of_stream);
static u32 VP8CycleCount(VP8DecContainer_t *dec_cont);
static enum DecRet VP8PushOutput(VP8DecContainer_t* dec_cont);

static void VP8SetExternalBufferInfo(VP8DecInst dec_inst);

static void VP8EnterAbortState(VP8DecContainer_t* dec_cont);
static void VP8ExistAbortState(VP8DecContainer_t* dec_cont);
static void VP8EmptyBufferQueue(VP8DecContainer_t* dec_cont);
static void VP8CheckBufferRealloc(VP8DecContainer_t* dec_cont);
extern void VP8HwdBufferQueueSetAbort(BufferQueue queue);
extern void VP8HwdBufferQueueClearAbort(BufferQueue queue);
extern void VP8HwdBufferQueueEmptyRef(BufferQueue queue, i32 buffer);

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void VP8CaseInfoCollect(VP8DecContainer_t* dec_cont, CaseInfo* case_info);
#endif

/*------------------------------------------------------------------------------
    Function name   : vp8decinit
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst * dec_inst
                      enum DecErrorHandling error_handling
------------------------------------------------------------------------------*/
enum DecRet VP8DecInit(VP8DecInst * dec_inst, const void *dwl,  struct VP8DecConfig *dec_cfg) {
  VP8DecContainer_t *dec_cont;
  u32 core_mask;
  enum DecRet ret;

  APITRACE("%s","VP8DecInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if(dec_inst == NULL) {
    APITRACEERR("%s","VP8DecInit# ERROR: dec_inst == NULL");
    return (DEC_PARAM_ERROR);
  }

  *dec_inst = NULL;   /* return NULL instance for any error */
  /* check that decoding supported in HW */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_VP8_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","VP6DecInit# VP6 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }
  if (dec_cfg->dec_format == DEC_INPUT_VP7) {
    core_mask = SwGetCoreMaskByFeature(dwl, VSI_VP7);
    if (core_mask == 0) {
      APITRACEERR("%s","VP8DecInit# ERROR: VP7 not supported in HW\n");
      return DEC_FORMAT_NOT_SUPPORTED;
    }
  }

  if(dec_cfg->dec_format == DEC_INPUT_VP8 || dec_cfg->dec_format == DEC_INPUT_WEBP) {
    core_mask = SwGetCoreMaskByFeature(dwl, VSI_VP8);
    if (core_mask == 0) {
      APITRACEERR("%s","VP8DecInit# ERROR: VP8 not supported in HW\n");
      return DEC_FORMAT_NOT_SUPPORTED;
    }
  }

  /* allocate instance */
  dec_cont = (VP8DecContainer_t *) DWLmalloc(sizeof(VP8DecContainer_t));
  if(dec_cont == NULL) {
    APITRACEERR("%s","VP8DecInit# ERROR: Memory allocation failed\n");
    return DEC_MEMFAIL;
  }
  DWLmemset(dec_cont, 0, sizeof(VP8DecContainer_t));
  dec_cont->dwl = dwl;
  dec_cont->num_cores = DWLReadAsicCoreCount(dec_cont->dwl);
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_VP8_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, core_mask, DWL_CLIENT_TYPE_VP8_DEC);

  /* initial setup of instance */
  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->dec_stat = VP8DEC_INITIALIZED;
  dec_cont->checksum = dec_cont;  /* save instance as a checksum */

  if( dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS)
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;
  switch(dec_cfg->dec_format) {
    case DEC_INPUT_VP7:
      dec_cont->dec_mode = dec_cont->decoder.dec_mode = VP8HWD_VP7;
      if(dec_cfg->num_frame_buffers < 3)
        dec_cfg->num_frame_buffers = 3;
      break;
    case DEC_INPUT_VP8:
      dec_cont->dec_mode = dec_cont->decoder.dec_mode = VP8HWD_VP8;
      if(dec_cfg->num_frame_buffers < 4)
        dec_cfg->num_frame_buffers = 4;
      break;
    case DEC_INPUT_WEBP:
      dec_cont->dec_mode = dec_cont->decoder.dec_mode = VP8HWD_VP8;
      dec_cont->intra_only = HANTRO_TRUE;
      dec_cfg->num_frame_buffers = 1;
      break;
    default:
      APITRACEERR("%s","VP8DecInit# ERROR: dec_cfg->dec_format not support.\n");
      return DEC_FORMAT_NOT_SUPPORTED;
      break;
  }

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);
  dec_cont->num_buffers = dec_cfg->num_frame_buffers;
  dec_cont->num_buffers_reserved = dec_cfg->num_frame_buffers;
  VP8HwdAsicInit(dec_cont);   /* Init ASIC */

  dec_cont->num_cores = DWLReadAsicCoreCount(dec_cont->dwl);
  if(!dec_cont->num_cores) {
    APITRACEERR("%s","VP8DecInit# ERROR: no any core to support vp8.\n");
    ret = DEC_DWL_ERROR;
    goto err;
  }

  dec_cont->seq_state = SEQ_CLEAN;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  SetECPolicy(dec_cfg->error_handling, 0, &dec_cont->error_policy);

  dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VCD;

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }
  dec_cont->asic_buff->release_buffer = 0;

  dec_cont->picture_broken = 0;
  dec_cont->decoder.refbu_pred_hits = 0;

  if (!dec_cfg->error_handling && dec_cfg->dec_format == DEC_INPUT_VP8)
    dec_cont->hw_ec_support = 0;

  if ((dec_cfg->dec_format == DEC_INPUT_VP8) || (dec_cfg->dec_format == DEC_INPUT_WEBP))
    dec_cont->stride_support = 1;
  if (FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_out) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;

  /* If tile mode is enabled, should take DTRC minimum size(96x48) into consideration */

  dec_cont->min_dec_pic_width = VP8_MIN_WIDTH_EN_DTRC;
  dec_cont->min_dec_pic_height = VP8_MIN_HEIGHT_EN_DTRC;

  /* return new instance to application */
  *dec_inst = (VP8DecInst) dec_cont;

  APITRACE("%s","VP8DecInit# OK\n");
  return (DEC_OK);

err:
  DWLfree(dec_cont);
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecRelease
    Description     :
    Return type     : void
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
void VP8DecRelease(VP8DecInst dec_inst) {

  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  const void *dwl;
  u32 i;

  APITRACE("%s","VP8DecRelease#\n");

  if(dec_cont == NULL) {
    APITRACEERR("%s","VP8DecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP8DecRelease# ERROR: Decoder not initialized\n");
    return;
  }

  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (dec_cont->hw_rdy_callback_arg[i]) {
      DWLfree(dec_cont->hw_rdy_callback_arg[i]);
      dec_cont->hw_rdy_callback_arg[i] = NULL;
    }
  }
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

  VP8HwdAsicReleaseMem(dec_cont);
  VP8HwdAsicReleasePictures(dec_cont);
  if (dec_cont->hw_ec_support)
    vp8hwdReleaseEc(&dec_cont->ec);

  if (dec_cont->fifo_out)
    FifoRelease(dec_cont->fifo_out);

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

  APITRACE("%s","VP8DecRelease# OK\n");

  return;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecGetInfo
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : VP8DecInfo * dec_info
------------------------------------------------------------------------------*/
enum DecRet VP8DecGetInfo(VP8DecInst dec_inst, VP8DecInfo * dec_info) {
  const VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;

  APITRACE("%s","VP8DecGetInfo#\n");

  if(dec_inst == NULL || dec_info == NULL) {
    APITRACEERR("%s","VP8DecGetInfo# ERROR: dec_inst or dec_info is NULL\n");
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP8DecGetInfo# ERROR: Decoder not initialized\n");
    return DEC_NOT_INITIALIZED;
  }

  if (dec_cont->dec_stat == VP8DEC_INITIALIZED) {
    return DEC_HDRS_NOT_RDY;
  }

  dec_info->vp_version = dec_cont->decoder.vp_version;
  dec_info->vp_profile = dec_cont->decoder.vp_profile;
  dec_info->pic_buff_size = dec_cont->buf_num;

  dec_info->output_format = VP8DEC_TILED_YUV420;

  /* Fragments have 8 pixels */
  dec_info->coded_width = dec_cont->decoder.width;
  dec_info->coded_height = dec_cont->decoder.height;
  dec_info->frame_width = (dec_cont->decoder.width + 15) & ~15;
  dec_info->frame_height = (dec_cont->decoder.height + 15) & ~15;
  dec_info->scaled_width = dec_cont->decoder.scaled_width;
  dec_info->scaled_height = dec_cont->decoder.scaled_height;
  dec_info->dpb_mode = DEC_DPB_FRAME;

  return DEC_OK;
}

static enum DecRet FetchPPBuffer(VP8DecContainer_t *dec_cont, const VP8DecInput *input,
                            DecAsicBuffers_t *p_asic_buff, struct DecOutput *output, u32 data_left) {
  struct DWLLinearMem *pp_buffer = NULL;

#ifdef GET_FREE_BUFFER_NON_BLOCK
  pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
#else
  pp_buffer= InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
#endif
  p_asic_buff->pp_buffer = pp_buffer;
  if (pp_buffer == NULL) {
    if (dec_cont->abort)
      return DEC_ABORTED;
    output->data_left = data_left;
    output->strm_curr_pos = (u8 *)input->stream + input->data_len-data_left;
    output->strm_curr_bus_address = input->stream_bus_address + input->data_len-data_left;
    dec_cont->no_decoding_buffer = 1;
    p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 0;
    VP8HwdBufferQueueRemoveRef(dec_cont->bq, p_asic_buff->out_buffer_i);
    return DEC_NO_DECODING_BUFFER;
  }

#ifdef ENABLE_FPGA_VERIFICATION
    /* update device buffer by host buffer */
    DWLLinearMemset(dec_cont->dwl, pp_buffer, 0, 0, pp_buffer->size);
#endif
  InputQueueSetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer, PP_OUT_CTRL(input->dec_ctrl));
  p_asic_buff->out_pp_buffer_i = FindPpIndex(dec_cont, DWL_GET_DEVMEM_ADDR(*(p_asic_buff->pp_buffer)));
  p_asic_buff->pp_buffer_map[p_asic_buff->out_buffer_i] = p_asic_buff->out_pp_buffer_i;
  return DEC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecDecode
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : const VP8DecInput * input
    Argument        : VP8DecFrame * output
------------------------------------------------------------------------------*/
enum DecRet VP8DecDecode(VP8DecInst dec_inst,
                       const VP8DecInput * input, struct DecOutput * output) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff;
  i32 ret;
  u32 asic_status;
  u32 error_detected = 0;
  struct DWLReqInfo info = {0};
  APITRACE("%s","VP8DecDecode#\n");

  /* Check that function input parameters are valid */
  if(input == NULL || output == NULL || dec_inst == NULL) {
    APITRACEERR("%s","VP8DecDecode# ERROR: NULL arg(s)\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP8DecDecode# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  DWLmemset(output, 0, sizeof(struct DecOutput));

  if (dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if(((input->data_len > dec_cont->max_strm_len) && !dec_cont->intra_only) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address)) {
    APITRACEERR("%s","VP8DecDecode# ERROR: Invalid arg value\n");
    return DEC_PARAM_ERROR;
  }

  if ((input->p_pic_buffer_y != NULL && input->pic_buffer_bus_address_y == 0) ||
      (input->p_pic_buffer_y == NULL && input->pic_buffer_bus_address_y != 0) ||
      (input->p_pic_buffer_c != NULL && input->pic_buffer_bus_address_c == 0) ||
      (input->p_pic_buffer_c == NULL && input->pic_buffer_bus_address_c != 0) ||
      (input->p_pic_buffer_y == NULL && input->p_pic_buffer_c != 0) ||
      (input->p_pic_buffer_y != NULL && input->p_pic_buffer_c == 0)) {
    APITRACEERR("%s","VP8DecDecode# ERROR: Invalid arg value\n");
    return DEC_PARAM_ERROR;
  }

#ifdef VP8DEC_EVALUATION
  if(dec_cont->pic_number > VP8DEC_EVALUATION) {
    APITRACEERR("%s","VP8DecDecode# DEC_EVALUATION_LIMIT_EXCEEDED\n");
    return DEC_EVALUATION_LIMIT_EXCEEDED;
  }
#endif

  if(!input->data_len && dec_cont->stream_consumed_callback ) {
    dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
    return DEC_OK;
  }
  /* aliases */
  p_asic_buff = dec_cont->asic_buff;
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_VP8_DEC);

  if (dec_cont->no_decoding_buffer || dec_cont->get_buffer_after_abort) {
    p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
    if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
      if (dec_cont->abort)
        return DEC_ABORTED;
      else {
        output->data_left = input->data_len;
        output->strm_curr_pos = (u8 *)input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        dec_cont->no_decoding_buffer = 1;
        return DEC_NO_DECODING_BUFFER;
      }
    }
    p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
    p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];

#ifdef ENABLE_FPGA_VERIFICATION
    if(!dec_cont->pp_enabled) {
      /* update device buffer by host buffer */
      DWLLinearMemset(dec_cont->dwl, p_asic_buff->out_buffer, 0, 0,
                      p_asic_buff->out_buffer->size);
    }
#endif
    if (dec_cont->pp_enabled && !dec_cont->intra_only &&
       !p_asic_buff->strides_used) {
      ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, input->data_len);
      if (ret != DEC_OK) return ret;
    }

    p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
    dec_cont->display_number = input->pic_id;
    dec_cont->no_decoding_buffer = 0;

    if (dec_cont->get_buffer_after_abort) {
      dec_cont->get_buffer_after_abort = 0;
      VP8HwdBufferQueueUpdateRef(dec_cont->bq,
          BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
          p_asic_buff->out_buffer_i);

      if(dec_cont->intra_only != HANTRO_TRUE) {
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
      }
    }

    if (VP8PushOutput(dec_cont) == DEC_ABORTED)
      return DEC_ABORTED;
  }

  if((!dec_cont->intra_only) && input->slice_height ) {
    APITRACEERR("%s","VP8DecDecode# ERROR: Invalid arg value\n");
    return DEC_PARAM_ERROR;
  }
  if(dec_cont->intra_only) {
    u32 index = p_asic_buff->out_buffer_i;
    if (dec_cont->pp_enabled)
      index = p_asic_buff->pp_buffer_map[index];
    while(dec_cont->asic_buff->not_displayed[index])
      sched_yield();
  }

  /* application indicates that slice mode decoding should be used ->
   * disabled unless WebP not used */
  if (dec_cont->dec_stat != VP8DEC_MIDDLE_OF_PIC &&
      dec_cont->intra_only && input->slice_height) {
    u32 tmp;
    /* Slice mode can only be enabled if image width is larger
     * than supported video decoder maximum width. */
    if( input->data_len >= 5 ) {
      /* Peek frame width. We make shortcuts and assumptions:
       *  -always keyframe
       *  -always VP8 (i.e. not VP7)
       *  -keyframe start code and frame tag skipped
       *  -if keyframe start code invalid, handle it later */
      tmp = (input->stream[7] << 8)|
            (input->stream[6]); /* Read 16-bit chunk */
      tmp = tmp & 0x3fff;
      if( tmp > dec_cont->hw_feature->vp8_max_dec_pic_width) {
        if(input->slice_height > 255) {
          APITRACEERR("%s","VP8DecDecode# ERROR: Slice height > max\n");
          return DEC_PARAM_ERROR;
        }

        dec_cont->slice_height = input->slice_height;
      } else {
        dec_cont->slice_height = 0;
      }
      output->slice_height = dec_cont->slice_height; /* transfer it to testbench */
    } else {
      /* Too little data in buffer, let later error management
       * handle it. Disallow slice mode. */
    }
  }

  if (dec_cont->intra_only && input->p_pic_buffer_y) {
    dec_cont->user_mem = 1;
    dec_cont->asic_buff->user_mem.p_pic_buffer_y[0] = input->p_pic_buffer_y;
    dec_cont->asic_buff->user_mem.pic_buffer_bus_addr_y[0] =
      input->pic_buffer_bus_address_y;
    dec_cont->asic_buff->user_mem.p_pic_buffer_c[0] = input->p_pic_buffer_c;
    dec_cont->asic_buff->user_mem.pic_buffer_bus_addr_c[0] =
      input->pic_buffer_bus_address_c;
  }

  if (dec_cont->dec_stat == VP8DEC_NEW_HEADERS) {
#ifdef SLICE_MODE_LARGE_PIC
    if((dec_cont->asic_buff->width*
        dec_cont->asic_buff->height > WEBP_MAX_PIXEL_AMOUNT_NONSLICE) &&
        dec_cont->intra_only &&
        (dec_cont->slice_height == 0)) {
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      return DEC_STREAM_NOT_SUPPORTED;
    }
#endif

    dec_cont->dec_stat = VP8DEC_DECODING;
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    VP8CheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {

        VP8HwdAsicReleaseMem(dec_cont);
        if (dec_cont->hw_ec_support)
          vp8hwdReleaseEc(&dec_cont->ec);

      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        VP8HwdBufferQueueWaitNotInUse(dec_cont->bq);
#endif
        if (dec_cont->asic_buff->ext_buffer_added) {
          dec_cont->asic_buff->release_buffer = 1;
          //return DEC_WAITING_FOR_BUFFER;
        }
        VP8HwdAsicReleasePictures(dec_cont);
      } else {
        if(dec_cont->stream_consumed_callback == NULL &&
            dec_cont->bq && dec_cont->intra_only != HANTRO_TRUE) {
          i32 index;
          /* Legacy single core: remove references made for the next decode. */
          if (( index = VP8HwdBufferQueueGetPrevRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          if (( index = VP8HwdBufferQueueGetAltRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          if (( index = VP8HwdBufferQueueGetGoldenRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                     dec_cont->asic_buff->out_buffer_i);
        }

        if(p_asic_buff->mvs[0].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[0]);

        if(p_asic_buff->mvs[1].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[1]);
      }

      if((ret = VP8HwdAsicAllocatePictures(dec_cont)) != 0 ||
        (dec_cont->hw_ec_support &&
        vp8hwdInitEc(&dec_cont->ec, dec_cont->width, dec_cont->height, 16))) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        APITRACEERR
        ("%s","VP8DecDecode# ERROR: Picture memory allocation failed\n");
        if (ret == -2)
          return DEC_ABORTED;
        else
          return DEC_MEMFAIL;
      }

      if(VP8HwdAsicAllocateMem(dec_cont) != 0) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        APITRACEERR("%s","VP8DecInit# ERROR: ASIC Memory allocation failed\n");
        return DEC_MEMFAIL;
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->asic_buff->ext_buffer_added) {
          dec_cont->asic_buff->release_buffer = 1;
          //return DEC_WAITING_FOR_BUFFER;
        }
      }

      VP8HwdAsicReleaseMem(dec_cont);
      if (dec_cont->hw_ec_support)
        vp8hwdReleaseEc(&dec_cont->ec);

      if (dec_cont->realloc_int_buf) {
        VP8HwdAsicReleasePictures(dec_cont);
      } else {
        if(dec_cont->stream_consumed_callback == NULL &&
            dec_cont->bq && dec_cont->intra_only != HANTRO_TRUE) {
          i32 index;
          /* Legacy single core: remove references made for the next decode. */
          if (( index = VP8HwdBufferQueueGetPrevRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          if (( index = VP8HwdBufferQueueGetAltRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
          if (( index = VP8HwdBufferQueueGetGoldenRef(dec_cont->bq)) != REFERENCE_NOT_SET)
            VP8HwdBufferQueueRemoveRef(dec_cont->bq, index);
        }

        if(p_asic_buff->mvs[0].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[0]);

        if(p_asic_buff->mvs[1].virtual_address != NULL)
          DWLFreeLinear(dec_cont->dwl, &p_asic_buff->mvs[1]);
      }

      if((ret = VP8HwdAsicAllocatePictures(dec_cont)) != 0 ||
        (dec_cont->hw_ec_support &&
        vp8hwdInitEc(&dec_cont->ec, dec_cont->width, dec_cont->height, 16))) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        APITRACEERR
        ("%s","VP8DecDecode# ERROR: Picture memory allocation failed\n");
        if (ret == -2)
          return DEC_ABORTED;
        else
          return DEC_MEMFAIL;
      }

      if(VP8HwdAsicAllocateMem(dec_cont) != 0) {
        if(dec_cont->stream_consumed_callback != NULL) {
          dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
        }
        APITRACEERR("%s","VP8DecInit# ERROR: ASIC Memory allocation failed\n");
        return DEC_MEMFAIL;
      }

      if (dec_cont->realloc_int_buf && !dec_cont->realloc_ext_buf) {
        if (dec_cont->pp_enabled && (!p_asic_buff->strides_used)) {
          /* Id for first frame. */
          p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
          if(p_asic_buff->out_buffer_i == (i32)0xFFFFFFFF) {
            if ( dec_cont->abort)
                return DEC_ABORTED;
            else {
              output->data_left = input->data_len;
              output->strm_curr_pos = (u8 *)input->stream;
              output->strm_curr_bus_address = input->stream_bus_address;
              dec_cont->no_decoding_buffer = 1;
              return DEC_NO_DECODING_BUFFER;
            }
          }

          p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
          p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];

          ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, input->data_len);
          if (ret != DEC_OK) return ret;
          /* These need to point at something so use the output buffer */
          VP8HwdBufferQueueUpdateRef(dec_cont->bq,
                                     BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
                                     p_asic_buff->out_buffer_i);
          if(dec_cont->intra_only != HANTRO_TRUE) {
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
          }
        }
      }
    }

    if (dec_cont->realloc_ext_buf) {
      VP8SetExternalBufferInfo(dec_cont);
      dec_cont->buffer_index = 0;
      dec_cont->dec_stat = VP8DEC_WAITING_BUFFER;
      output->data_left = input->data_len;
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      return DEC_WAITING_FOR_BUFFER;
    }
  }
  else if (dec_cont->dec_stat == VP8DEC_WAITING_BUFFER) {
    if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
      output->data_left = input->data_len;
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      return DEC_WAITING_FOR_BUFFER;
    }

    if (dec_cont->pp_enabled && (!p_asic_buff->strides_used)) {
      /* Id for first frame. */
      p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
      if(p_asic_buff->out_buffer_i == (i32)0xFFFFFFFF) {
        if ( dec_cont->abort)
            return DEC_ABORTED;
        else {
          output->data_left = input->data_len;
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          dec_cont->no_decoding_buffer = 1;
          return DEC_NO_DECODING_BUFFER;
        }
      }

      p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
      ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, input->data_len);
      if (ret != DEC_OK) return ret;
      /* These need to point at something so use the output buffer */
      VP8HwdBufferQueueUpdateRef(dec_cont->bq,
                                 BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
                                 p_asic_buff->out_buffer_i);
      if(dec_cont->intra_only != HANTRO_TRUE) {
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
        VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
      }
    }
    dec_cont->dec_stat = VP8DEC_DECODING;
  }
  else if (dec_cont->dec_stat != VP8DEC_MIDDLE_OF_PIC && input->data_len) {
    dec_cont->prev_is_key = dec_cont->decoder.key_frame;
    dec_cont->decoder.probs_decoded = 0;

    /* decode frame tag */
    vp8hwdDecodeFrameTag( input->stream, &dec_cont->decoder );

    /* When on key-frame, reset probabilities and such */
    if( dec_cont->decoder.key_frame ) {
      vp8hwdResetDecoder( &dec_cont->decoder);
      dec_cont->seq_state = SEQ_CLEAN;
    }
    /* intra only and non key-frame */
    else if (dec_cont->intra_only) {
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      return DEC_STRM_ERROR;
    }

    if (dec_cont->decoder.key_frame || dec_cont->decoder.vp_version > 0) {
      p_asic_buff->dc_pred[0] = p_asic_buff->dc_pred[1] =
                                  p_asic_buff->dc_match[0] = p_asic_buff->dc_match[1] = 0;
    }

    /* Decode frame header (now starts bool coder as well) */
    ret = vp8hwdDecodeFrameHeader(
            input->stream + dec_cont->decoder.frame_tag_size,
            input->data_len - dec_cont->decoder.frame_tag_size,
            &dec_cont->bc, &dec_cont->decoder );
    if( ret != HANTRO_OK ) {
      dec_cont->seq_state = SEQ_DIRTY;
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      APITRACEERR("%s","VP8DecDecode# ERROR: Frame header decoding failed\n");
      if (!dec_cont->pic_number || dec_cont->dec_stat != VP8DEC_DECODING) {
        return DEC_STRM_ERROR;
      } else {
        APITRACE("%s","VP8DecDecode# DEC_STRM_PROCESSED\n");
        return DEC_STRM_PROCESSED;
      }
    }
    /* flag the stream as non "error-resilient" */
    else if (dec_cont->decoder.refresh_entropy_probs)
      dec_cont->prob_refresh_detected = 1;

    if(CheckBitstreamWorkaround(&dec_cont->decoder)) {
      /* do bitstream workaround */
      /*DoBitstreamWorkaround(&dec_cont->decoder, p_asic_buff, &dec_cont->bc);*/
    }

    ret = vp8hwdSetPartitionOffsets(input->stream, input->data_len,
                                    &dec_cont->decoder);
    /* ignore errors in partition offsets if HW error concealment used
     * (assuming parts of stream missing -> partition start offsets may
     * be larger than amount of stream in the buffer) */
    if (ret != HANTRO_OK && !dec_cont->hw_ec_support) {
      dec_cont->seq_state = SEQ_DIRTY;
      if(dec_cont->stream_consumed_callback != NULL) {
        dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
      }
      if (!dec_cont->pic_number || dec_cont->dec_stat != VP8DEC_DECODING) {
        return DEC_STRM_ERROR;
      } else {
        APITRACE("%s","VP8DecDecode# DEC_STRM_PROCESSED\n");
        return DEC_STRM_PROCESSED;
      }
    }

    if (dec_cont->seq_state != SEQ_CLEAN) {
      if (vp8ECGetInDataAction(dec_cont->error_policy) == EC_STATE_DISCARD) {
        return DEC_STRM_PROCESSED;
      }
    }

    /* check for picture size change */
    if((dec_cont->width != (dec_cont->decoder.width)) ||
        (dec_cont->height != (dec_cont->decoder.height))) {

      if (dec_cont->stream_consumed_callback != NULL && dec_cont->bq) {
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   dec_cont->asic_buff->out_buffer_i);
        dec_cont->asic_buff->out_buffer_i = VP8_UNDEFINED_BUFFER;
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
        VP8HwdBufferQueueRemoveRef(dec_cont->bq,
                                   VP8HwdBufferQueueGetAltRef(dec_cont->bq));
        /* Wait for output processing to finish before releasing. */
        VP8HwdBufferQueueWaitPending(dec_cont->bq);
        if (dec_cont->pp_enabled && (!p_asic_buff->strides_used)) {
          /* Id for first frame. */
          p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
          if(p_asic_buff->out_buffer_i == (i32)0xFFFFFFFF) {
            if ( dec_cont->abort)
              return DEC_ABORTED;
            else {
              output->data_left = input->data_len;
              output->strm_curr_pos = (u8 *)input->stream;
              output->strm_curr_bus_address = input->stream_bus_address;
              dec_cont->no_decoding_buffer = 1;
              return DEC_NO_DECODING_BUFFER;
            }
          }
          p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
          p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
          ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, input->data_len);
          if (ret != DEC_OK) return ret;
          /* These need to point at something so use the output buffer */
          VP8HwdBufferQueueUpdateRef(dec_cont->bq,
                                     BQUEUE_FLAG_PREV | BQUEUE_FLAG_GOLDEN | BQUEUE_FLAG_ALT,
                                     p_asic_buff->out_buffer_i);
          if(dec_cont->intra_only != HANTRO_TRUE) {
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetPrevRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetAltRef(dec_cont->bq));
            VP8HwdBufferQueueAddRef(dec_cont->bq, VP8HwdBufferQueueGetGoldenRef(dec_cont->bq));
          }
        }
      }

      /* reallocate picture buffers */
      p_asic_buff->width = ( dec_cont->decoder.width + 15 ) & ~15;
      p_asic_buff->height = ( dec_cont->decoder.height + 15 ) & ~15;

      {
        /* check for minimum and maximum dimensions */
        u32 is_intra = dec_cont->intra_only;
        SwAdjustCoreMaskByWxH(dec_cont->dwl, dec_cont->asic_buff->width ,
                              dec_cont->asic_buff->height, is_intra,
                              DWL_CLIENT_TYPE_VP8_DEC, &dec_cont->core_mask);
        if (CORE_MASK(dec_cont->core_mask) == 0) {
          APITRACEERR("%s","VP8DecDecode# ERROR: Unsupported size\n");
          if (dec_cont->stream_consumed_callback)
            dec_cont->stream_consumed_callback((u8*)input->stream, input->p_user_data);
          dec_cont->dec_stat = VP8DEC_INITIALIZED;
          return DEC_STREAM_NOT_SUPPORTED;
        }
        dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                              DWL_CLIENT_TYPE_VP8_DEC);
      }

      if (dec_cont->pp_enabled) {
        dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
        dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
      }

      dec_cont->prev_width = dec_cont->width;
      dec_cont->prev_height = dec_cont->height;
      dec_cont->width = dec_cont->decoder.width;
      dec_cont->height = dec_cont->decoder.height;

      dec_cont->dec_stat = VP8DEC_NEW_HEADERS;

      APITRACE("%s","VP8DecDecode# DEC_HDRS_RDY\n");

      FifoPush(dec_cont->fifo_out, (FifoObject)FLUSH_MARKER, FIFO_EXCEPTION_DISABLE);
      if(dec_cont->abort)
        return(DEC_ABORTED);
      else {
        output->data_left = input->data_len;
        output->strm_curr_pos = (u8 *)input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        return DEC_HDRS_RDY;
      }
    }

    /* If we are here and dimensions are still 0, it means that we have
     * yet to decode a valid keyframe, in which case we must give up. */
    if( dec_cont->width == 0 || dec_cont->height == 0 ) {
      return DEC_STRM_PROCESSED;
    }

    if (dec_cont->decoder.key_frame && dec_cont->dec_stat == VP8DEC_INITIALIZED) {
      dec_cont->dec_stat = VP8DEC_NEW_HEADERS;
      output->data_left = input->data_len;
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      return DEC_HDRS_RDY;
    }
  }
  /* missing picture, conceal */
  else if (!input->data_len) {
    if (!dec_cont->hw_ec_support || dec_cont->prev_is_key) {
      dec_cont->decoder.probs_decoded = 0;
      APITRACE("%s","VP8DecDecode# DEC_STRM_PROCESSED\n");
      return DEC_STRM_PROCESSED;
    } else {
      dec_cont->conceal_start_mb_x = dec_cont->conceal_start_mb_y = 0;
      vp8hwdHwErrorConceal(dec_cont, input->stream_bus_address,
                         /*conceal_everything*/ 1);
      /* Assume that broken picture updated the last reference only and
       * also addref it since it will be outputted one more time. */
      VP8HwdBufferQueueAddRef(dec_cont->bq, p_asic_buff->out_buffer_i);
      VP8HwdBufferQueueUpdateRef(dec_cont->bq, BQUEUE_FLAG_PREV,
                                 p_asic_buff->out_buffer_i);
      p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
      p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
      p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
      p_asic_buff->out_buffer = NULL;
      dec_cont->pic_number++;
      dec_cont->out_count++;
      if (dec_cont->prob_refresh_detected) {
        dec_cont->picture_broken = 1;
        dec_cont->force_intra_freeze = 1;
      }

      if (VP8PushOutput(dec_cont) == DEC_ABORTED)
        return DEC_ABORTED;
      p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
      if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
        if (dec_cont->abort)
          return DEC_ABORTED;
        else {
          output->data_left = 0;
          output->strm_curr_pos = (u8*)input->stream + input->data_len;
          output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
          dec_cont->no_decoding_buffer = 1;
          return DEC_PIC_DECODED;
          //return DEC_NO_DECODING_BUFFER;
        }
      }
      p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
      ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, 0);
      if (ret == DEC_NO_DECODING_BUFFER) ret = DEC_PIC_DECODED;
      if (ret != DEC_OK) return ret;
#if 0
      dec_cont->pic_number++;
      dec_cont->out_count++;
      ASSERT(p_asic_buff->out_buffer != NULL);
      if (dec_cont->prob_refresh_detected) {
        dec_cont->picture_broken = 1;
        dec_cont->force_intra_freeze = 1;
      }

      if (VP8PushOutput(dec_cont) == DEC_ABORTED)
        return DEC_ABORTED;
#endif
      return DEC_PIC_DECODED;
    }
  }

  if (dec_cont->dec_stat != VP8DEC_MIDDLE_OF_PIC) {
    info.core_mask = dec_cont->core_mask;
    info.width = dec_cont->asic_buff->width;
    info.height = dec_cont->asic_buff->height;
    info.owner = (void *)dec_cont;
    if (dec_cont->vcmd_used) {
      dec_cont->core_id = 0;
      ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_cont->cmdbuf_id);
    } else {
      ret = DWLReserveHw(dec_cont->dwl, &info, &dec_cont->core_id);
    }

    if(ret != DWL_OK) {
      APITRACEERR("%s","ERROR: DWLReserveHw Failed");
      return VP8HWDEC_HW_RESERVED;
    }

    /* prepare asic */

    VP8HwdAsicProbUpdate(dec_cont);

    VP8HwdSegmentMapUpdate(dec_cont);

    VP8HwdAsicInitPicture(dec_cont);

    VP8HwdAsicStrmPosUpdate(dec_cont, input->stream_bus_address);

    /* Store the needed data for callback setup. */
    /* TODO(vmr): Consider parametrizing this. */
    dec_cont->stream = input->stream;
    dec_cont->p_user_data = input->p_user_data;

    /* input stream buffer */
    DWLDMATransData2(dec_cont->dwl, (addr_t)input->stream_bus_address,
                    (void *)input->stream,
                    input->data_len, HOST_TO_DEVICE);
    /* the [segment map] and [pro_tbl] are all in pro_tbl buffer.
    * [segment map] : always needs DMA.
    * [pro_tbl] : always needs DMA.
    */
    av_unused u32 index = ((dec_cont->num_cores>1) && dec_cont->stream_consumed_callback) ? dec_cont->core_id : 0;
    DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->prob_tbl[index], 0,
                    dec_cont->asic_buff->prob_tbl[index].size, HOST_TO_DEVICE);
  } else
    VP8HwdAsicContPicture(dec_cont);

  /* run the hardware */
  asic_status = VP8HwdAsicRun(dec_cont);

  /* Rollback entropy probabilities if refresh is not set */
  if(dec_cont->decoder.refresh_entropy_probs == HANTRO_FALSE) {
    DWLmemcpy( &dec_cont->decoder.entropy, &dec_cont->decoder.entropy_last,
               sizeof(vp8EntropyProbs_t));
    DWLmemcpy( dec_cont->decoder.vp7_scan_order, dec_cont->decoder.vp7_prev_scan_order,
               sizeof(dec_cont->decoder.vp7_scan_order));
  }
  /* If in asynchronous mode, just return OK  */
  if (asic_status == VP8HWDEC_ASYNC_MODE) {
    /* find first free buffer and use it as next output */
    if (!error_detected || dec_cont->intra_only) {
      p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
      p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
      p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
      p_asic_buff->out_buffer = NULL;

      /* If we are never going to output the current buffer, we can release
      * the ref for output on the buffer. */
      if (!dec_cont->decoder.show_frame) {
        VP8HwdBufferQueueRemoveRef(dec_cont->bq, p_asic_buff->out_buffer_i);
        if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
          InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_asic_buff->pp_pictures[p_asic_buff->out_pp_buffer_i])));
        }
      }
      /* If WebP, we will be recycling only one buffer and no update is
       * necessary. */
      if (!dec_cont->intra_only) {
        p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
        if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
          if ( dec_cont->abort)
            return DEC_ABORTED;
          else {
            output->data_left = 0;
            output->strm_curr_pos = (u8*)input->stream + input->data_len;
            output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
            dec_cont->no_decoding_buffer = 1;
            return DEC_OK;
          }
        }
        p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
      }
      p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
      ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, 0);
      if (ret == DEC_NO_DECODING_BUFFER) ret = DEC_OK;
      if (ret != DEC_OK) return ret;
      ASSERT(p_asic_buff->out_buffer != NULL);
    }
    return DEC_OK;
  }

  /* Handle system error situations */
  if(asic_status == VP8HWDEC_SYSTEM_TIMEOUT) {
    /* This timeout is DWL(software/os) generated */
    APITRACE("%s","VP8DecDecode# DEC_HW_TIMEOUT, SW generated\n");
    return DEC_HW_TIMEOUT;
  } else if(asic_status == VP8HWDEC_SYSTEM_ERROR) {
    APITRACEERR("%s","VP8DecDecode# VP8HWDEC_SYSTEM_ERROR\n");
    return DEC_SYSTEM_ERROR;
  } else if(asic_status == VP8HWDEC_HW_RESERVED) {
    APITRACE("%s","VP8DecDecode# VP8HWDEC_HW_RESERVED\n");
    return DEC_HW_RESERVED;
  }

  /* Handle possible common HW error situations */
  if(asic_status & DEC_HW_IRQ_BUS) {
    APITRACEERR("%s","VP8DecDecode# DEC_HW_BUS_ERROR\n");
    return DEC_HW_BUS_ERROR;
  }

  vp8ECMarkOutDataErrInfo(dec_cont, asic_status);
  /* for all the rest we will output a picture (concealed or not) */
  if((asic_status & DEC_HW_IRQ_TIMEOUT) ||
      (asic_status & DEC_HW_IRQ_ERROR) ||
      (asic_status & DEC_HW_IRQ_ASO) || /* to signal lost residual */
      (asic_status & DEC_HW_IRQ_ABORT)) {
#ifdef CASE_INFO_STAT
     VP8CaseInfoCollect(dec_cont, &case_info);
#endif
    /* This timeout is HW generated */
    if(asic_status & DEC_HW_IRQ_TIMEOUT) {
#ifdef VP8HWTIMEOUT_ASSERT
        ASSERT(0);
#endif
      APITRACEDEBUG("%s","IRQ: HW TIMEOUT\n");
    } else {
      APITRACEDEBUG("%s","IRQ: STREAM ERROR\n");
    }
    if (!dec_cont->hw_ec_support) {
      dec_cont->seq_state = SEQ_DIRTY;
      p_asic_buff->error_ratio[p_asic_buff->out_buffer_i] =
                              getG1OutDataErrorRatio(dec_cont->width, dec_cont->height,
                                  GetDecRegister(dec_cont->vp8_regs, HWIF_MB_LOCATION_X),
                                  GetDecRegister(dec_cont->vp8_regs, HWIF_MB_LOCATION_Y));

      if (EC_STATE_DISCARD == vp8ECGetOutDataAction(dec_cont)) {
        error_detected = 1;
        dec_cont->decoder.show_frame = 0;
      } else {
        error_detected = 0;
      }
    } else {
      u32 conceal_everything = 0;
      /* keyframe -> all mbs concealed */
      if (dec_cont->decoder.key_frame ||
          (asic_status & DEC_HW_IRQ_TIMEOUT) ||
          (asic_status & DEC_HW_IRQ_ABORT)) {
        dec_cont->conceal_start_mb_x = 0;
        dec_cont->conceal_start_mb_y = 0;
        conceal_everything = 1;
      } else {
        /* concealment start point read from sw/hw registers */
        dec_cont->conceal_start_mb_x =
          GetDecRegister(dec_cont->vp8_regs, HWIF_STARTMB_X);
        dec_cont->conceal_start_mb_y =
          GetDecRegister(dec_cont->vp8_regs, HWIF_STARTMB_Y);
        /* error in control partition -> conceal all mbs from start point
         * onwards, otherwise only intra mbs get concealed */
        conceal_everything = !(asic_status & DEC_HW_IRQ_ASO);
      }

      /* HW error concealment not used if
      * 1) previous frame was key frame (no ref mvs available) AND
      * 2) whole control partition corrupted (no current mvs available) */
      if ((!dec_cont->prev_is_key || !conceal_everything ||
          dec_cont->conceal_start_mb_y || dec_cont->conceal_start_mb_x))
        vp8hwdHwErrorConceal(dec_cont, input->stream_bus_address,
                          conceal_everything);
    }
    if (dec_cont->slice_height && dec_cont->intra_only) {
      dec_cont->slice_concealment = 1;
    }
  } else if(asic_status & DEC_HW_IRQ_RDY) {
  } else if (asic_status & DEC_HW_IRQ_SLICE) {
  } else {
    ASSERT(0);
  }

  if(asic_status & DEC_HW_IRQ_RDY) {
    APITRACEDEBUG("%s","IRQ: PICTURE RDY\n");

    if (dec_cont->decoder.key_frame) {
      dec_cont->picture_broken = 0;
      dec_cont->force_intra_freeze = 0;
    }
#ifdef CASE_INFO_STAT
    VP8CaseInfoCollect(dec_cont, &case_info);
#endif

#ifdef FPGA_PERF_AND_BW
    DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info,
                     NEXT_MULTIPLE(dec_cont->decoder.height, 16) *
                     NEXT_MULTIPLE(dec_cont->decoder.width, 16),
                     8);
#endif

    if (dec_cont->slice_height) {
      dec_cont->output_rows = p_asic_buff->height -  dec_cont->tot_decoded_rows*16;
      /* Below code not needed; slice mode always disables loop-filter -->
       * output 16 rows multiple */
      /*
      if (dec_cont->tot_decoded_rows)
          dec_cont->output_rows += 8;
          */
      dec_cont->last_slice = 1;
      if (VP8PushOutput(dec_cont) == DEC_ABORTED)
        return DEC_ABORTED;
      return DEC_PIC_DECODED;
    }

  } else if (asic_status & DEC_HW_IRQ_SLICE) {
    dec_cont->dec_stat = VP8DEC_MIDDLE_OF_PIC;
#ifdef CASE_INFO_STAT
    if(case_info.frame_num < 10)
      case_info.slice_num[case_info.frame_num]++;
#endif
    dec_cont->output_rows = dec_cont->slice_height * 16;
    /* Below code not needed; slice mode always disables loop-filter -->
     * output 16 rows multiple */
    /*if (!dec_cont->tot_decoded_rows)
        dec_cont->output_rows -= 8;*/

    dec_cont->tot_decoded_rows += dec_cont->slice_height;

    if (VP8PushOutput(dec_cont) == DEC_ABORTED)
      return DEC_ABORTED;
    return DEC_SLICE_RDY;
  }

  if(dec_cont->intra_only != HANTRO_TRUE)
    VP8HwdUpdateRefs(dec_cont, error_detected);

  /* updata output_data info */
  if (output->strm_curr_pos == NULL) {
    output->data_left = 0;
    output->strm_curr_pos = (u8 *)input->stream + input->data_len;
    output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
  }

  if (dec_cont->decoder.show_frame)
    dec_cont->out_count++;
  /* find first free buffer and use it as next output */
  if (!error_detected || dec_cont->intra_only) {
    p_asic_buff->prev_out_buffer = p_asic_buff->out_buffer;
    p_asic_buff->prev_out_buffer_i = p_asic_buff->out_buffer_i;
    p_asic_buff->decode_id[p_asic_buff->prev_out_buffer_i] = input->pic_id;
    p_asic_buff->out_buffer = NULL;
    if(dec_cont->decoder.show_frame) {
      if (VP8PushOutput(dec_cont) == DEC_ABORTED)
        return DEC_ABORTED;
    }

    /* If WebP, we will be recycling only one buffer and no update is
     * necessary. */
    if (!dec_cont->intra_only) {
      if(dec_cont->decoder.show_frame) {
        if (VP8PushOutput(dec_cont) == DEC_ABORTED)
          return DEC_ABORTED;
      }
      VP8HwdBufferQueueRemoveRef(dec_cont->bq, p_asic_buff->out_buffer_i);
      if (!dec_cont->decoder.show_frame && dec_cont->pp_enabled && !p_asic_buff->strides_used) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(p_asic_buff->pp_pictures[p_asic_buff->out_pp_buffer_i])));
      }
      p_asic_buff->out_buffer_i = VP8HwdBufferQueueGetBuffer(dec_cont->bq);
      if(p_asic_buff->out_buffer_i == 0xFFFFFFFF) {
        if (dec_cont->abort)
          return DEC_ABORTED;
        else {
          output->data_left = 0;
          output->strm_curr_pos = (u8*)input->stream + input->data_len;
          output->strm_curr_bus_address = input->stream_bus_address + input->data_len;
          dec_cont->no_decoding_buffer = 1;
          return DEC_PIC_DECODED;
          //return DEC_NO_DECODING_BUFFER;
        }
      }
      p_asic_buff->first_show[p_asic_buff->out_buffer_i] = 1;
    }

    p_asic_buff->out_buffer = &p_asic_buff->pictures[p_asic_buff->out_buffer_i];
    ret = FetchPPBuffer(dec_cont, input, p_asic_buff, output, 0);
      if (ret == DEC_NO_DECODING_BUFFER) ret = DEC_PIC_DECODED;
    if (ret != DEC_OK) return ret;
    ASSERT(p_asic_buff->out_buffer != NULL);
  } else {
    dec_cont->picture_broken = 1;
    if (!dec_cont->pic_number) {
      u32 *prev_ref = GetPrevRef(dec_cont)->virtual_address;
      if (prev_ref) {
        (void) DWLmemset( prev_ref, 128,
                          p_asic_buff->width * p_asic_buff->height * 3 / 2);
      }
    }
    if (error_detected)
      return DEC_DISCARD_INTERNAL;
    else if (!dec_cont->decoder.show_frame)
      return DEC_STRM_PROCESSED;
  }
  if (!error_detected)
    dec_cont->pic_number++;

  if (VP8PushOutput(dec_cont) == DEC_ABORTED)
    return DEC_ABORTED;

  if (!error_detected) {
    APITRACE("%s","VP8DecDecode# DEC_PIC_DECODED\n");
    return DEC_PIC_DECODED;
  } else {
    return DEC_DISCARD_INTERNAL;
  }
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecNextPicture
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : VP8DecPicture * output
    Argument        : u32 end_of_stream
------------------------------------------------------------------------------*/
enum DecRet VP8DecNextPicture(VP8DecInst dec_inst, VP8DecPicture * output) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  i32 ret;

  //APITRACE("%s","VP8DecNextPicture#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP8DecNextPicture# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP8DecNextPicture# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }
  /*  NextOutput will block until there is an output. */
  addr_t i;
  if ((ret = FifoPop(dec_cont->fifo_out, (FifoObject *)&i,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
        FIFO_EXCEPTION_ENABLE
#else
        FIFO_EXCEPTION_DISABLE
#endif
        )) != FIFO_ABORT) {

#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
    if (ret == FIFO_EMPTY) return DEC_OK;
#endif

    if ((i32)i == EOS_MARKER) {
      APITRACE("%s","VP8DecNextPicture# DEC_END_OF_STREAM\n");
      return DEC_END_OF_STREAM;
    }
    if ((i32)i == FLUSH_MARKER) {
      APITRACE("%s","VP8DecNextPicture# DEC_FLUSHED\n");
      return DEC_FLUSHED;
    }

    *output = dec_cont->asic_buff->picture_info[i];
    ECErrorInfoReturn(output->error_info, output->error_ratio, output->pic_id);

    APITRACE("%s","VP8DecNextPicture# DEC_PIC_RDY\n");
    return DEC_PIC_RDY;
  } else
    return DEC_ABORTED;

  APITRACE("%s","VP8DecNextPicture# DEC_OK\n");
  return (DEC_OK);
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecNextPicture_INTERNAL
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
    Argument        : VP8DecPicture * output
    Argument        : u32 end_of_stream
------------------------------------------------------------------------------*/
enum DecRet VP8DecNextPicture_INTERNAL(VP8DecInst dec_inst,
                                     VP8DecPicture * output, u32 end_of_stream) {
  VP8DecContainer_t *dec_cont;// = (VP8DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff;// = dec_cont->asic_buff;
  u32 pic_for_output = 0;
  u32 ref_index, index;
  PpUnitIntConfig *ppu_cfg;
  u32 i;

  APITRACE("%s","VP8DecNextPicture_INTERNAL#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP8DecNextPicture_INTERNAL# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (VP8DecContainer_t *) dec_inst;
  p_asic_buff = dec_cont->asic_buff;

  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP8DecNextPicture_INTERNAL# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->out_count && !dec_cont->output_rows)
    return DEC_OK;

  if(dec_cont->slice_concealment)
    return (DEC_OK);

  (void) DWLmemset(output, 0, sizeof(VP8DecPicture));
  /* slice for output */
  if (dec_cont->output_rows) {
    output->num_slice_rows = dec_cont->output_rows;
    output->last_slice = dec_cont->last_slice;

    if (dec_cont->user_mem) {
      output->pictures[0].p_output_frame = p_asic_buff->user_mem.p_pic_buffer_y[0];
      output->pictures[0].p_output_frame_c = p_asic_buff->user_mem.p_pic_buffer_c[0];
      output->pictures[0].output_frame_bus_address =
        p_asic_buff->user_mem.pic_buffer_bus_addr_y[0];
      output->pictures[0].output_frame_bus_address_c =
        p_asic_buff->user_mem.pic_buffer_bus_addr_c[0];
    } else {
      if (!dec_cont->pp_enabled) {
        u32 stride, offset;
        stride = NEXT_MULTIPLE(p_asic_buff->width * 4, ALIGN(dec_cont->align));
        offset = 16 * (dec_cont->slice_height + 1) * stride / 4;
        output->pictures[0].p_output_frame = p_asic_buff->pictures[0].virtual_address;
        output->pictures[0].output_frame_bus_address =
          p_asic_buff->pictures[0].bus_address;

        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame_c = p_asic_buff->pictures_c[0].virtual_address;
          output->pictures[0].output_frame_bus_address_c =
            p_asic_buff->pictures_c[0].bus_address;
        } else {
          output->pictures[0].p_output_frame_c =
            (u32*)((addr_t)output->pictures[0].p_output_frame + offset);
          output->pictures[0].output_frame_bus_address_c =
            output->pictures[0].output_frame_bus_address + offset;
        }
      } else {
        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame = p_asic_buff->pictures[0].virtual_address;
          output->pictures[0].output_frame_bus_address =
            p_asic_buff->pictures[0].bus_address;
          output->pictures[0].p_output_frame_c = p_asic_buff->pictures_c[0].virtual_address;
          output->pictures[0].output_frame_bus_address_c =
            p_asic_buff->pictures_c[0].bus_address;
        } else {
          u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, p_asic_buff->pp_pictures[0]);
          ppu_cfg = dec_cont->ppu_cfg;
          for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
            if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
            output->pictures[i].p_output_frame = (u32*)((addr_t)p_asic_buff->pp_pictures[0]->virtual_address + ppu_cfg->luma_offset);
            output->pictures[i].output_frame_bus_address =
              p_asic_buff->pp_pictures[0]->bus_address + ppu_cfg->luma_offset;
            output->pictures[i].p_output_frame_c =
              (u32*)((addr_t)p_asic_buff->pp_pictures[0]->virtual_address + ppu_cfg->chroma_offset);
            output->pictures[i].output_frame_bus_address_c =
              p_asic_buff->pp_pictures[0]->bus_address + ppu_cfg->chroma_offset;
          }
        }
      }
    }

    output->pic_id = p_asic_buff->picture_info[0].decode_id;
    output->decode_id = p_asic_buff->picture_info[0].decode_id;
    output->is_intra_frame = dec_cont->decoder.key_frame;
    output->cycles_per_mb = VP8CycleCount(dec_cont);
    output->is_golden_frame = 0;
    output->nbr_of_err_mbs = 0;
    if (!dec_cont->pp_enabled || p_asic_buff->strides_used) {
      output->pictures[0].frame_width = (dec_cont->width + 15) & ~15;
      output->pictures[0].frame_height = (dec_cont->height + 15) & ~15;
      output->pictures[0].coded_width = dec_cont->width;
      output->pictures[0].coded_height = dec_cont->height;
      output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                            p_asic_buff->luma_stride : p_asic_buff->width;
      output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                              p_asic_buff->chroma_stride : p_asic_buff->width;
      output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].frame_width * 4,
                                                     ALIGN(dec_cont->align));
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
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
      ppu_cfg = dec_cont->ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
        if (!ppu_cfg->enabled) continue;
        output->pictures[i].frame_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].luma_stride = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
        output->pictures[i].chroma_stride = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
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
                          p_asic_buff->pp_pictures[0]->virtual_address,
                          p_asic_buff->pp_pictures[0]->bus_address,
                          &output->pictures[i].dec400_luma_table,
                          &output->pictures[i].dec400_chroma_table);
      }

    }

    dec_cont->output_rows = 0;

    while(dec_cont->asic_buff->not_displayed[0])
      sched_yield();

    VP8HwdBufferQueueSetBufferAsUsed(dec_cont->bq, 0);
    if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
      InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->asic_buff->pp_pictures[0])));
    }
    dec_cont->asic_buff->not_displayed[0] = 1;
    dec_cont->asic_buff->picture_info[0] = *output;
    if (dec_cont->pp_enabled)
      VP8HwdBufferQueueReleaseBuffer(dec_cont->bq, 0);
    FifoPush(dec_cont->fifo_out, 0, FIFO_EXCEPTION_DISABLE);
    return (DEC_PIC_RDY);
  }

  output->num_slice_rows = 0;

  pic_for_output = 1;

  if (pic_for_output) {
    const struct DWLLinearMem *out_pic = NULL;
    const struct DWLLinearMem *out_pic_c = NULL;

    out_pic = p_asic_buff->prev_out_buffer;
    out_pic_c = &p_asic_buff->pictures_c[ p_asic_buff->prev_out_buffer_i ];

    dec_cont->out_count--;

    output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                          p_asic_buff->luma_stride : p_asic_buff->width;
    output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                            p_asic_buff->chroma_stride : p_asic_buff->width;

    if (dec_cont->user_mem) {
      output->pictures[0].p_output_frame = p_asic_buff->user_mem.p_pic_buffer_y[0];
      output->pictures[0].output_frame_bus_address =
        p_asic_buff->user_mem.pic_buffer_bus_addr_y[0];
      output->pictures[0].p_output_frame_c = p_asic_buff->user_mem.p_pic_buffer_c[0];
      output->pictures[0].output_frame_bus_address_c =
        p_asic_buff->user_mem.pic_buffer_bus_addr_c[0];

    } else {
      if (!dec_cont->pp_enabled) {
        output->pictures[0].p_output_frame = out_pic->virtual_address;
        output->pictures[0].output_frame_bus_address = out_pic->bus_address;
        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
          output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
        } else {
          u32 stride, chroma_buf_offset;
          stride = p_asic_buff->chroma_stride ?
                    p_asic_buff->chroma_stride : p_asic_buff->width;
          stride = NEXT_MULTIPLE(stride * 4, ALIGN(dec_cont->align));
          chroma_buf_offset = stride * p_asic_buff->height / 4;
          output->pictures[0].p_output_frame_c = output->pictures[0].p_output_frame +
                                     chroma_buf_offset / 4;
          output->pictures[0].output_frame_bus_address_c = output->pictures[0].output_frame_bus_address +
                                               chroma_buf_offset;
        }
      } else {
        if(p_asic_buff->strides_used) {
          output->pictures[0].p_output_frame = out_pic->virtual_address;
          output->pictures[0].output_frame_bus_address = out_pic->bus_address;
          output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
          output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
        } else {
          index = p_asic_buff->pp_buffer_map[p_asic_buff->prev_out_buffer_i];
          ppu_cfg = dec_cont->ppu_cfg;
          u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, p_asic_buff->pp_pictures[index]);
          for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
            if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
            output->pictures[i].p_output_frame = (u32*)((addr_t)p_asic_buff->pp_pictures[index]->virtual_address + ppu_cfg->luma_offset);
            output->pictures[i].output_frame_bus_address =
              p_asic_buff->pp_pictures[index]->bus_address + ppu_cfg->luma_offset;
            output->pictures[i].p_output_frame_c =
              (u32*)((addr_t)p_asic_buff->pp_pictures[index]->virtual_address + ppu_cfg->chroma_offset);
            output->pictures[i].output_frame_bus_address_c =
              p_asic_buff->pp_pictures[index]->bus_address + ppu_cfg->chroma_offset;
          }
        }
      }
    }
    output->is_intra_frame = dec_cont->decoder.key_frame;
    output->cycles_per_mb = VP8CycleCount(dec_cont);
    output->is_golden_frame = 0;
    output->nbr_of_err_mbs = 0;
    output->error_info = p_asic_buff->error_info[p_asic_buff->prev_out_buffer_i];
    output->error_ratio = p_asic_buff->error_ratio[p_asic_buff->prev_out_buffer_i];;
#if 0
    output->frame_width = (dec_cont->width + 15) & ~15;
    output->frame_height = (dec_cont->height + 15) & ~15;
    output->coded_width = dec_cont->width;
    output->coded_height = dec_cont->height;
#endif
    ref_index = index = FindIndex(dec_cont, DWL_GET_DEVMEM_ADDR(*out_pic));
    if (dec_cont->pp_enabled && !p_asic_buff->strides_used)
      index = p_asic_buff->pp_buffer_map[index];
    if (!dec_cont->pp_enabled || p_asic_buff->strides_used) {
      output->pictures[0].frame_width = dec_cont->asic_buff->frame_width[ref_index];
      output->pictures[0].frame_height = dec_cont->asic_buff->frame_height[ref_index];
      output->pictures[0].coded_width = dec_cont->asic_buff->coded_width[ref_index];
      output->pictures[0].coded_height = dec_cont->asic_buff->coded_height[ref_index];
      output->pictures[0].pic_stride = NEXT_MULTIPLE(output->pictures[0].frame_width * 4,
                                                     ALIGN(dec_cont->align));
      output->pictures[0].pic_stride_ch = output->pictures[0].pic_stride;
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
      ppu_cfg = dec_cont->ppu_cfg;
      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;
        output->pictures[i].frame_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].coded_width = dec_cont->ppu_cfg[i].scale.width;
        output->pictures[i].coded_height = dec_cont->ppu_cfg[i].scale.height;
        output->pictures[i].luma_stride = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
        output->pictures[i].chroma_stride = NEXT_MULTIPLE(dec_cont->ppu_cfg[i].scale.width, ALIGN(ppu_cfg->align));
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
        if (!ppu_cfg->enabled || !ppu_cfg->dec400_enabled) continue;
          PpFillDec400TblInfo(ppu_cfg,
                              p_asic_buff->pp_pictures[index]->virtual_address,
                              p_asic_buff->pp_pictures[index]->bus_address,
                              &output->pictures[i].dec400_luma_table,
                              &output->pictures[i].dec400_chroma_table);
      }

    }
    APITRACE("%s","VP8DecNextPicture_INTERNAL# DEC_PIC_RDY\n");
    output->pic_id = p_asic_buff->decode_id[ref_index];
    output->decode_id = p_asic_buff->decode_id[ref_index];
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[ref_index])
#endif
    {
    if (VP8HwdBufferQueueWaitBufNotInUse(dec_cont->bq, ref_index) == HANTRO_NOK)
      return DEC_ABORTED;
    VP8HwdBufferQueueSetBufferAsUsed(dec_cont->bq, ref_index);
    if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
      InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->asic_buff->pp_pictures[index])));
      InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->asic_buff->pp_pictures[index])));
    }
    dec_cont->asic_buff->not_displayed[index] = 1;
    dec_cont->asic_buff->picture_info[index] = *output;
    if (dec_cont->pp_enabled)
      VP8HwdBufferQueueReleaseBuffer(dec_cont->bq, ref_index);
    FifoPush(dec_cont->fifo_out, (FifoObject)(addr_t)index, FIFO_EXCEPTION_DISABLE);
      dec_cont->asic_buff->first_show[ref_index] = 0;
    }
    return (DEC_PIC_RDY);
  }

  APITRACE("%s","VP8DecNextPicture_INTERNAL# DEC_OK\n");
  return (DEC_OK);

}


/*------------------------------------------------------------------------------
    Function name   : VP8DecPictureConsumed
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP8DecPictureConsumed(VP8DecInst dec_inst,
                                const VP8DecPicture * picture) {
  u32 buffer_id;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;
  u32 i = 0;
  if (dec_inst == NULL || picture == NULL) {
    return DEC_PARAM_ERROR;
  }

  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  if (dec_cont->pp_enabled && !dec_cont->asic_buff->strides_used) {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (DWLMemAddr)picture->pictures[i].output_frame_bus_address;
        break;
      }
    }
    buffer_id = FindPpIndex(dec_cont, output_picture);
    if (buffer_id >= dec_cont->num_pp_buffers)
      return DEC_PARAM_ERROR;
  } else {
    buffer_id = FindIndex(dec_cont, picture->pictures[0].output_frame_bus_address);
    if (buffer_id >= dec_cont->num_buffers)
      return DEC_PARAM_ERROR;

  }

  /* Remove the reference to the buffer. */
  if(dec_cont->asic_buff->not_displayed[buffer_id]) {
    dec_cont->asic_buff->not_displayed[buffer_id] = 0;
    if(picture->num_slice_rows == 0 || picture->last_slice) {
      if (dec_cont->pp_enabled && !dec_cont->asic_buff->strides_used) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, output_picture);
      }
      else
        VP8HwdBufferQueueReleaseBuffer(dec_cont->bq, buffer_id);
    }
  }
  APITRACE("%s","VP8DecPictureConsumed# DEC_OK\n");
  return DEC_OK;
}


/*------------------------------------------------------------------------------
    Function name   : VP8DecEndOfStream
    Description     : Used for signalling stream end from the input thread
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP8DecEndOfStream(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  VP8DecPicture output;
  enum DecRet ret;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  /* Don't do end of stream twice. This is not thread-safe, so it must be
   * called from the single input thread that is also used to call
   * VP8DecDecode. */
  if (dec_cont->dec_stat == DEC_END_OF_STREAM) {
    return DEC_END_OF_STREAM;
  }

  while((ret = VP8DecNextPicture_INTERNAL(dec_inst, &output, 1)) == DEC_PIC_RDY);
  if(ret == DEC_ABORTED) {
    return (DEC_ABORTED);
  }

  dec_cont->dec_stat = DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_out, (FifoObject)EOS_MARKER, FIFO_EXCEPTION_DISABLE);

  return DEC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecPeek
    Description     :
    Return type     : enum DecRet
    Argument        : VP8DecInst dec_inst
------------------------------------------------------------------------------*/
enum DecRet VP8DecPeek(VP8DecInst dec_inst, VP8DecPicture * output) {
  VP8DecContainer_t *dec_cont;// = (VP8DecContainer_t *) dec_inst;
  DecAsicBuffers_t *p_asic_buff;// = dec_cont->asic_buff;
  u32 buff_id;
  const struct DWLLinearMem *out_pic = NULL;
  const struct DWLLinearMem *out_pic_c = NULL;

  APITRACE("%s","VP8DecPeek#\n");

  if(dec_inst == NULL || output == NULL) {
    APITRACEERR("%s","VP8DecPeek# ERROR: dec_inst or output is NULL\n");
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (VP8DecContainer_t *) dec_inst;
  p_asic_buff = dec_cont->asic_buff;


  /* Check for valid decoder instance */
  if(dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","VP8DecPeek# ERROR: Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  /* Don't allow peek for webp */
  if(dec_cont->intra_only) {
    DWLmemset(output, 0, sizeof(VP8DecPicture));
    return DEC_OK;
  }

  /* when output release thread enabled, VP8DecNextPicture_INTERNAL() called in
     VP8DecDecode(), and so dec_cont->fullness used to sample the real out_count
     in case of VP8DecNextPicture_INTERNAL() called before than VP8DecPeek() */
  u32 tmp = dec_cont->fullness;

  if (!tmp) {
    DWLmemset(output, 0, sizeof(VP8DecPicture));
    return DEC_OK;
  }

  out_pic = p_asic_buff->prev_out_buffer;
  out_pic_c = &p_asic_buff->pictures_c[ p_asic_buff->prev_out_buffer_i ];

  if (!dec_cont->pp_enabled) {
    output->pictures[0].p_output_frame = out_pic->virtual_address;
    output->pictures[0].output_frame_bus_address = out_pic->bus_address;
    if(p_asic_buff->strides_used) {
      output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
      output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
    } else {
      u32 chroma_buf_offset = p_asic_buff->width * p_asic_buff->height;
      output->pictures[0].p_output_frame_c = output->pictures[0].p_output_frame +
                                 chroma_buf_offset / 4;
      output->pictures[0].output_frame_bus_address_c = output->pictures[0].output_frame_bus_address +
                                           chroma_buf_offset;
    }
  } else {
    if(p_asic_buff->strides_used) {
      output->pictures[0].p_output_frame = out_pic->virtual_address;
      output->pictures[0].output_frame_bus_address = out_pic->bus_address;
      output->pictures[0].p_output_frame_c = out_pic_c->virtual_address;
      output->pictures[0].output_frame_bus_address_c = out_pic_c->bus_address;
    } else {
      u32 offset = (16 * (dec_cont->slice_height + 1) >> dec_cont->dscale_shift_y) * p_asic_buff->width >> dec_cont->dscale_shift_x;
      output->pictures[0].p_output_frame = p_asic_buff->pp_pictures[p_asic_buff->pp_buffer_map[p_asic_buff->prev_out_buffer_i]]->virtual_address;
      output->pictures[0].output_frame_bus_address =
        p_asic_buff->pp_pictures[p_asic_buff->pp_buffer_map[p_asic_buff->prev_out_buffer_i]]->bus_address;
      output->pictures[0].p_output_frame_c =
        (u32*)((addr_t)output->pictures[0].p_output_frame + offset);
      output->pictures[0].output_frame_bus_address_c =
        output->pictures[0].output_frame_bus_address + offset;
    }
  }

  buff_id = FindIndex(dec_cont, output->pictures[0].output_frame_bus_address);
  output->pic_id = p_asic_buff->decode_id[buff_id];
  output->decode_id = p_asic_buff->decode_id[buff_id];
  output->is_intra_frame = dec_cont->decoder.key_frame;
  output->cycles_per_mb = VP8CycleCount(dec_cont);
  output->is_golden_frame = 0;
  output->nbr_of_err_mbs = 0;

  if (!dec_cont->pp_enabled || p_asic_buff->strides_used) {
    output->pictures[0].frame_width = (dec_cont->width + 15) & ~15;
    output->pictures[0].frame_height = (dec_cont->height + 15) & ~15;
    output->pictures[0].coded_width = dec_cont->width;
    output->pictures[0].coded_height = dec_cont->height;
    output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                          p_asic_buff->luma_stride : p_asic_buff->width;
    output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                            p_asic_buff->chroma_stride : p_asic_buff->width;
  } else {
    output->pictures[0].frame_width = ((dec_cont->width + 15) & ~15) >> dec_cont->dscale_shift_x;
    output->pictures[0].frame_height = ((dec_cont->height + 15) & ~15) >> dec_cont->dscale_shift_y;
    output->pictures[0].coded_width = dec_cont->width >> dec_cont->dscale_shift_x;
    output->pictures[0].coded_height = dec_cont->height >> dec_cont->dscale_shift_y;
    output->pictures[0].luma_stride = p_asic_buff->luma_stride ?
                          p_asic_buff->luma_stride : p_asic_buff->width;
    output->pictures[0].chroma_stride = p_asic_buff->chroma_stride ?
                            p_asic_buff->chroma_stride : p_asic_buff->width;
    output->pictures[0].luma_stride = output->pictures[0].luma_stride >> dec_cont->dscale_shift_x;
    output->pictures[0].chroma_stride = output->pictures[0].chroma_stride >> dec_cont->dscale_shift_x;
  }
  return (DEC_PIC_RDY);


}

void vp8hwdMCFreeze(VP8DecContainer_t *dec_cont) {
  /*TODO (mheikkinen) Error handling/concealment is still under construction.*/
  /* TODO Output reference handling */
  VP8HwdBufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
  dec_cont->stream_consumed_callback((u8*)dec_cont->stream, dec_cont->p_user_data);
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdFreeze
    Description     :
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/

#if !(defined(_MSC_VER) || defined(_WIN32))
__attribute__((unused))
#endif
static void vp8hwdFreeze(VP8DecContainer_t *dec_cont) {
  /* for multicore */
  if(dec_cont->stream_consumed_callback) {
    vp8hwdMCFreeze(dec_cont);
    return;
  }
  /* Skip */
  dec_cont->pic_number++;
  dec_cont->ref_to_out = 1;
  if (dec_cont->decoder.show_frame)
    dec_cont->out_count++;

  /* Rollback entropy probabilities if refresh is not set */
  if (dec_cont->decoder.probs_decoded &&
      dec_cont->decoder.refresh_entropy_probs == HANTRO_FALSE) {
    DWLmemcpy( &dec_cont->decoder.entropy, &dec_cont->decoder.entropy_last,
               sizeof(vp8EntropyProbs_t));
    DWLmemcpy( dec_cont->decoder.vp7_scan_order, dec_cont->decoder.vp7_prev_scan_order,
               sizeof(dec_cont->decoder.vp7_scan_order));
  }
  /* lost accumulated coeff prob updates -> force video freeze until next
   * keyframe received */
  else if (dec_cont->hw_ec_support && dec_cont->prob_refresh_detected) {
    dec_cont->force_intra_freeze = 1;
  }

  dec_cont->picture_broken = 1;

  /* reset mv memory to not to use too old mvs in extrapolation */
  if (dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_ref].virtual_address)
    DWLmemset(
      dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_ref].virtual_address,
      0, dec_cont->width * dec_cont->height / 256 * 16 * sizeof(u32));
}

/*------------------------------------------------------------------------------
    Function name   : CheckBitstreamWorkaround
    Description     : Check if we need a workaround for a rare bug.
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
u32 CheckBitstreamWorkaround(vp8_decoder_t* dec) {
  /* TODO: HW ID check, P pic stuff */
  if( dec->segmentation_map_update &&
      dec->coeff_skip_mode == 0 &&
      dec->key_frame) {
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : DoBitstreamWorkaround
    Description     : Perform workaround for bug.
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
#if 0
void DoBitstreamWorkaround(vp8_decoder_t* dec, DecAsicBuffers_t *p_asic_buff, vpBoolCoder_t*bc) {
  /* TODO in entirety */
}
#endif

/* TODO(mheikkinen) Work in progress */
void ConcealRefAvailability(u32 * output, u32 height, u32 width, u32 align) {
  u32 offset = 0;
  u32 out_w_luma, out_w_chroma, out_h;

  out_w_luma = NEXT_MULTIPLE(4 * width, ALIGN(align));
  out_w_chroma = NEXT_MULTIPLE(4 * width, ALIGN(align));
  out_h = height / 4;
  offset = out_w_luma * out_h + out_w_chroma * out_h / 2;

  u8 * p_ref_status = (u8 *)(output + offset / 4);

  p_ref_status[1] = height & 0xFFU;
  p_ref_status[0] = (height >> 8) & 0xFFU;
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdHwErrorConceal
    Description     :
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
void vp8hwdHwErrorConceal(VP8DecContainer_t *dec_cont, addr_t bus_address,
                        u32 conceal_everything) {
  /* force keyframes processed like normal frames (mvs extrapolated for
   * all mbs) */
  if (dec_cont->decoder.key_frame) {
    dec_cont->decoder.key_frame = 0;
  }

  vp8hwdEc(&dec_cont->ec,
           dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_ref].virtual_address,
           dec_cont->asic_buff->mvs[dec_cont->asic_buff->mvs_curr].virtual_address,
           dec_cont->conceal_start_mb_y * dec_cont->width/16 + dec_cont->conceal_start_mb_x,
           conceal_everything);

  dec_cont->conceal = 1;
  if (conceal_everything)
    dec_cont->conceal_start_mb_x = dec_cont->conceal_start_mb_y = 0;
  VP8HwdAsicInitPicture(dec_cont);
  VP8HwdAsicStrmPosUpdate(dec_cont, bus_address);

  ConcealRefAvailability(dec_cont->asic_buff->out_buffer->virtual_address,
                         MB_MULTIPLE(dec_cont->asic_buff->height),  MB_MULTIPLE(dec_cont->asic_buff->width),
                         dec_cont->align);

  dec_cont->conceal = 0;
}

/*------------------------------------------------------------------------------
    Function name   : VP8DecSetPictureBuffers
    Description     :
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
enum DecRet VP8DecSetPictureBuffers( VP8DecInst dec_inst,
                                   VP8DecPictureBufferProperties * p_pbp ) {

  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *) dec_inst;
  u32 i;
  u32 ok;
#if DEC_X170_REFBU_WIDTH == 64
  const u32 max_stride = 1<<18;
#else
  const u32 max_stride = 1<<17;
#endif /* DEC_X170_REFBU_WIDTH */
  u32 luma_stride_pow2 = 0;
  u32 chroma_stride_pow2 = 0;

  if(!dec_inst || !p_pbp) {
    APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: NULL parameter\n");
    return DEC_PARAM_ERROR;
  }

  /* Allow this only at stream start! */
  if ( ((dec_cont->dec_stat != VP8DEC_NEW_HEADERS) &&
        (dec_cont->dec_stat != VP8DEC_INITIALIZED)) ||
       (dec_cont->pic_number > 0)) {
    APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: Setup allowed at stream"\
                " start only!\n");
    return DEC_PARAM_ERROR;
  }

  if( !dec_cont->stride_support ) {
    APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: Not supported\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  /* Tiled mode and custom strides not supported yet */
  if(
      p_pbp->luma_stride || p_pbp->chroma_stride) {
    APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: tiled mode and picture "\
                "buffer properties conflict\n");
    return DEC_PARAM_ERROR;
  }

  /* Strides must be 2^N for some N>=4 */
  if(p_pbp->luma_stride || p_pbp->chroma_stride) {
    ok = 0;
    for ( i = 10 ; i < 32 ; ++i ) {
      if(p_pbp->luma_stride == ((u32)1<<i)) {
        luma_stride_pow2 = i;
        ok = 1;
        break;
      }
    }
    if(!ok) {
      APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: luma stride must be a "\
                  "power of 2\n");
      return DEC_PARAM_ERROR;
    }

    ok = 0;
    for ( i = 10 ; i < 32 ; ++i ) {
      if(p_pbp->chroma_stride == ((u32)1<<i)) {
        chroma_stride_pow2 = i;
        ok = 1;
        break;
      }
    }
    if(!ok) {
      APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: luma stride must be a "\
                  "power of 2\n");
      return DEC_PARAM_ERROR;
    }
  }

  /* Max luma stride check */
  if(p_pbp->luma_stride > max_stride) {
    APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: luma stride exceeds "\
                "maximum\n");
    return DEC_PARAM_ERROR;
  }

  /* Max chroma stride check */
  if(p_pbp->chroma_stride > max_stride) {
    APITRACEERR("%s","VP8DecSetPictureBuffers# ERROR: chroma stride exceeds "\
                "maximum\n");
    return DEC_PARAM_ERROR;
  }

  dec_cont->asic_buff->luma_stride = p_pbp->luma_stride;
  dec_cont->asic_buff->chroma_stride = p_pbp->chroma_stride;
  dec_cont->asic_buff->luma_stride_pow2 = luma_stride_pow2;
  dec_cont->asic_buff->chroma_stride_pow2 = chroma_stride_pow2;
  dec_cont->asic_buff->strides_used = 0;
  if( dec_cont->asic_buff->luma_stride ||
      dec_cont->asic_buff->chroma_stride ) {
    dec_cont->asic_buff->strides_used = 1;
  }
  VP8SetExternalBufferInfo(dec_cont);

  APITRACE("%s","VP8DecSetPictureBuffers# OK\n");
  return (DEC_OK);

}

static struct DWLLinearMem* GetPrevRef(VP8DecContainer_t *dec_cont) {
  return dec_cont->asic_buff->pictures +
         VP8HwdBufferQueueGetPrevRef(dec_cont->bq);
}

i32 FindIndex(VP8DecContainer_t* dec_cont, DWLMemAddr address) {
  i32 i;

  if(dec_cont->user_mem) {
    for (i = 0; i < (i32)dec_cont->num_buffers; i++) {
      if(dec_cont->asic_buff->user_mem.pic_buffer_bus_addr_y[i] == address)
        break;
    }
  } else {
    for (i = 0; i < (i32)dec_cont->num_buffers; i++) {
      if (DWL_DEVMEM_COMPARE(dec_cont->asic_buff->pictures[i], address))
        break;
    }
  }
  return i;
}

i32 FindPpIndex(VP8DecContainer_t* dec_cont, DWLMemAddr address) {
  i32 i;

  for (i = 0; i < (i32)dec_cont->num_pp_buffers; i++) {
    if (DWL_DEVMEM_IN_RANGE(*(dec_cont->asic_buff->pp_pictures[i]), address))
      break;
  }
  return i;
}

static u32 VP8CycleCount(VP8DecContainer_t *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->height, 16) *
             NEXT_MULTIPLE(dec_cont->width, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->vp8_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  return cycles;
}

static enum DecRet VP8PushOutput(VP8DecContainer_t* dec_cont) {
  u32 ret=DEC_OK;
  VP8DecPicture output;

  /* Sample dec_cont->out_count for Peek */
  dec_cont->fullness = dec_cont->out_count;
  if((dec_cont->num_cores==1) || !dec_cont->stream_consumed_callback) { // single core decode
    do {
      ret = VP8DecNextPicture_INTERNAL(dec_cont, &output, 0);
      if(ret == DEC_ABORTED)
        return (DEC_ABORTED);
    } while( ret == DEC_PIC_RDY);
  }
  return ret;
}

void VP8SetExternalBufferInfo(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 buffer;
  u32 ext_buffer_size;

  buffer = dec_cont->num_buffers_reserved;
  switch(dec_cont->dec_mode) {
  case VP8HWD_VP7:
    if(buffer < 3)
      buffer = 3;
    break;
  case VP8HWD_VP8:
    if(dec_cont->intra_only == HANTRO_TRUE)
      buffer = 1;
    else {
      if(buffer < 4)
        buffer = 4;
    }
    break;
  }

  ext_buffer_size = VP8GetRefFrmSize(dec_cont);
  if (dec_cont->pp_enabled && !p_asic_buff->strides_used) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    ext_buffer_size = CalcPpUnitBufferSize(ppu_cfg, 0) + 16;
  }

  dec_cont->ext_min_buffer_num = dec_cont->buf_num = buffer;
  dec_cont->next_buf_size = ext_buffer_size;
}

enum DecRet VP8DecGetBufferInfo(VP8DecInst dec_inst, struct DecBufferInfo *mem_info) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;
  u32 i;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    u32 frame_width = (dec_cont->width + 15) & ~15;
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

enum DecRet VP8DecAddBuffer(VP8DecInst dec_inst, struct DWLLinearMem *info) {
  VP8DecContainer_t *dec_cont;// = (VP8DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff;// = dec_cont->asic_buff;
  enum DecRet dec_ret = DEC_OK;

  if (dec_inst == NULL)
    return DEC_PARAM_ERROR;

  dec_cont = (VP8DecContainer_t *)dec_inst;
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

  /* buffer is not enoughm, return WAITING_FOR_BUFFER */
  if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num)
    dec_ret = DEC_WAITING_FOR_BUFFER;

  if (dec_cont->pp_enabled == 0) {
    p_asic_buff->pictures[i] = *info;
    p_asic_buff->pictures_c[i].virtual_address =
      p_asic_buff->pictures[i].virtual_address +
      (p_asic_buff->chroma_buf_offset/4);
    p_asic_buff->pictures_c[i].bus_address =
      p_asic_buff->pictures[i].bus_address +
      p_asic_buff->chroma_buf_offset;

    if ((dec_cont->num_cores>1) && dec_cont->stream_consumed_callback)
    {
      (void)DWLLinearMemset(dec_cont->dwl, &p_asic_buff->pictures[i],
                            p_asic_buff->sync_mc_offset, ~0, 16);
    }
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
      dec_cont->num_buffers++;
      VP8HwdBufferQueueAddBuffer(dec_cont->bq, i);
    }
  } else {
    /* Add down scale buffer. */
    dec_cont->num_pp_buffers++;
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
    p_asic_buff->pp_pictures[i] = &dec_cont->ext_buffers[i];
  }
  dec_cont->asic_buff->ext_buffer_added = 1;
  return dec_ret;
}

void VP8EnterAbortState(VP8DecContainer_t* dec_cont) {
  dec_cont->abort = 1;
  VP8HwdBufferQueueSetAbort(dec_cont->bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_out);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}

void VP8ExistAbortState(VP8DecContainer_t* dec_cont) {
  dec_cont->abort = 0;
  VP8HwdBufferQueueClearAbort(dec_cont->bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_out);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}


void VP8EmptyBufferQueue(VP8DecContainer_t* dec_cont) {
  u32 i;

  for (i = 0; i < dec_cont->num_buffers; i++) {
    VP8HwdBufferQueueEmptyRef(dec_cont->bq, i);
  }
}

void VP8StateReset(VP8DecContainer_t* dec_cont) {
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 buffers = dec_cont->num_buffers_reserved;

  /* Clear internal parameters in VP8DecContainer_t */
  dec_cont->dec_stat = VP8DEC_INITIALIZED;
  dec_cont->pic_number = 0;
  dec_cont->display_number = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->num_buffers = buffers;
  if (dec_cont->bq)
    VP8HwdBufferQueueRelease(dec_cont->bq);
  dec_cont->bq = VP8HwdBufferQueueInitialize(dec_cont->num_buffers);
  dec_cont->num_pp_buffers = 0;
#endif
  dec_cont->last_slice = 0;
  dec_cont->fullness = 0;
  dec_cont->picture_broken = 0;
  dec_cont->out_count = 0;
  dec_cont->ref_to_out = 0;
  dec_cont->slice_concealment = 0;
  dec_cont->tot_decoded_rows = 0;
  dec_cont->output_rows = 0;
  dec_cont->conceal = 0;
  dec_cont->conceal_start_mb_x = 0;
  dec_cont->conceal_start_mb_y = 0;
  dec_cont->prev_is_key = 0;
  dec_cont->force_intra_freeze = 0;
  dec_cont->prob_refresh_detected = 0;
  dec_cont->get_buffer_after_abort = 1;
  (void) DWLmemset(&dec_cont->bc, 0, sizeof(vpBoolCoder_t));

  /* Clear internal parameters in DecAsicBuffers */
  p_asic_buff->release_buffer = 0;
  p_asic_buff->ext_buffer_added = 0;
  p_asic_buff->prev_out_buffer = NULL;
  p_asic_buff->mvs_curr = p_asic_buff->mvs_ref = 0;
  p_asic_buff->whole_pic_concealed = 0;
  p_asic_buff->dc_pred[0] = p_asic_buff->dc_pred[1] =
  p_asic_buff->dc_match[0] = p_asic_buff->dc_match[1] = 0;
  (void) DWLmemset(p_asic_buff->decode_id, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->first_show, 0, 16 * sizeof(u32));
#ifdef USE_OMXIL_BUFFER
  (void) DWLmemset(p_asic_buff->not_displayed, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->display_index, 0, 16 * sizeof(u32));
  (void) DWLmemset(p_asic_buff->picture_info, 0, 16 * sizeof(VP8DecPicture));
  if (dec_cont->pp_enabled) {
    u32 i = 0;
    for (i=0; i<MAX_PIC_BUFFERS; i++)
      p_asic_buff->pp_pictures[i] = NULL;
  }
#endif
  p_asic_buff->out_buffer_i = REFERENCE_NOT_SET;
  p_asic_buff->out_buffer = NULL;
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_out);
  FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_out);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
  (void)buffers;
}

enum DecRet VP8DecAbort(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;

  APITRACE("%s","VP8DecAbort#\n");

  if (dec_inst == NULL) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  VP8EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","VP8DecAbort# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet VP8DecAbortAfter(VP8DecInst dec_inst) {
  VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;

  APITRACE("%s","VP8DecAbortAfter#\n");

  if (dec_inst == NULL) {
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
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->vp8_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  VP8EmptyBufferQueue(dec_cont);

  VP8StateReset(dec_cont);

  VP8ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","VP8DecAbortAfter# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet VP8DecSetInfo(VP8DecInst dec_inst,
                        struct VP8DecConfig *dec_cfg) {
  /*@null@ */ VP8DecContainer_t *dec_cont = (VP8DecContainer_t *)dec_inst;
  DecAsicBuffers_t *p_asic_buff = dec_cont->asic_buff;
  u32 pic_width;
  u32 pic_height;
  u32 i;
  pic_width = p_asic_buff->luma_stride ?
              p_asic_buff->luma_stride : p_asic_buff->width;
  if (!dec_cont->slice_height)
    pic_height = p_asic_buff->height;
  else
    pic_height = (dec_cont->slice_height + 1) * 16;
  const struct DecHwFeatures *hw_feature = NULL;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_VP8_DEC);
  dec_cont->hw_feature = hw_feature;
  if (!hw_feature) {
    APITRACEDEBUG("%s","VP8DecSetInfo# not found any hw_feature.\n");
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
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;

  return (DEC_OK);
}



void VP8CheckBufferRealloc(VP8DecContainer_t *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (VP8GetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
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
      if (VP8GetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (VP8GetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}

#ifdef CASE_INFO_STAT
void VP8CaseInfoCollect(VP8DecContainer_t *dec_cont, CaseInfo* case_info)
{
  u32 display_width, display_height, frame_width, frame_height;
  display_width = dec_cont->ppu_cfg[0].scale.width;
  display_height = dec_cont->ppu_cfg[0].scale.height;
  frame_width = (dec_cont->decoder.width + 15) & ~15;
  frame_height = (dec_cont->decoder.height + 15) & ~15;
  if (dec_cont->ppu_cfg[0].crop.width == 0)
    display_width = dec_cont->ppu_cfg[0].scale.width;
  else {
    display_width = dec_cont->ppu_cfg[0].crop.width;
    if(display_width != frame_width)
      case_info->crop_flag = 1;
  }
  if (dec_cont->ppu_cfg[0].crop.height == 0)
    display_width = dec_cont->ppu_cfg[0].scale.height;
  else {
    display_height = dec_cont->ppu_cfg[0].crop.height;
    if(display_height != frame_height)
      case_info->crop_flag = 1;
  }

  if(dec_cont->pic_number == 0) {
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
  }
  case_info->bit_depth = 8;
  case_info->chroma_format_id = 1;

  if (dec_cont->pic_number < 10){
    if (dec_cont->decoder.key_frame)
      case_info->frame_type[dec_cont->pic_number] = I_FRAME;
    else
      case_info->frame_type[dec_cont->pic_number] = P_FRAME;
    case_info->slice_num[case_info->frame_num]++;
  }
  case_info->codec = dec_cont->dec_mode == 1 ? DEC_MODE_VP7 : DEC_MODE_VP8;
  case_info->frame_num = dec_cont->pic_number + 1;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->vp8_regs, HWIF_STREAM_LEN) * 8/ NEXT_MULTIPLE(dec_cont->height, 16))
                         * 3840/ NEXT_MULTIPLE(dec_cont->width, 16)) * 2160/ 1024/ 1024;
}
#endif
