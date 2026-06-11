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

#include "vc1decapi.h"
#include "version.h"
#include "vc1hwd_container.h"
#include "vc1hwd_decoder.h"
#include "vc1hwd_util.h"
#include "vc1hwd_pp_pipeline.h"
#include "regdrv.h"
#include "vc1hwd_asic.h"
#include "dwl.h"
#include "deccfg.h"
#include "tiledref.h"
#include "bqueue.h"
#include "errorhandling.h"
#include "commonconfig.h"
#include "vpufeature.h"
#include "ppu.h"
#include "sw_util.h"
#include "dec_log.h"

/*------------------------------------------------------------------------------
    External compiler flags
--------------------------------------------------------------------------------

_VC1DEC_TRACE         Trace VC1 Decoder API function calls.

--------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/
#ifndef HANTRO_OK
#define HANTRO_OK 0
#endif

#ifndef HANTRO_NOK
#define HANTRO_NOK 1
#endif

static void WriteBitPlaneCtrl(u32 *bit_plane_ctrl, u8 *mb_flags, u32 num_mbs);
extern u16x AllocateMemories( decContainer_t *dec_cont,
                              swStrmStorage_t *storage,
                              const void *dwl );
static void VC1SetExternalBufferInfo(VC1DecInst dec_inst);

static enum DecRet VC1DecNextPicture_INTERNAL(VC1DecInst dec_inst, VC1DecPicture *picture,
    u32 end_of_stream);
static void VC1FillPicStruct(decContainer_t *dec_cont, VC1DecPicture *picture, u32 pic_index);
static u32 VC1CycleCount(decContainer_t *dec_cont);
static void VC1EnterAbortState(decContainer_t *dec_cont);
static void VC1ExistAbortState(decContainer_t *dec_cont);
static void VC1EmptyBufferQueue(decContainer_t *dec_cont);
static void VC1CheckBufferRealloc(decContainer_t *dec_cont);
static enum DecRet vc1GetRetValByIrq(u32 asic_status);
static void vc1ECGetErrorRatio(decContainer_t *dec_cont);

#define DEC_DPB_NOT_INITIALIZED      -1

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void VC1CaseInfoCollect(decContainer_t *dec_cont, CaseInfo *case_info);
#endif
/*------------------------------------------------------------------------------

    Function name: VC1DecInit

        Functional description:
            Initializes decoder software. Function reserves memory for the
            instance and calls vc1hwdInit to initialize the instance data.

        Inputs:
            dec_inst    Pointer to decoder instance.
            dwl         Pointer to stream metadata container.
            dec_cfg     Pointer to decoder config info.

        Return values:
            DEC_OK           Initialization is successful.
            DEC_PARAM_ERROR  Error with function parameters.
            DEC_MEMFAIL      Memory allocation failed.
            DEC_INITFAIL     Initialization failed.
            DEC_DWL_ERROR    Error initializing the system interface

------------------------------------------------------------------------------*/
enum DecRet VC1DecInit(VC1DecInst* dec_inst, const void *dwl, struct VC1DecConfig *dec_cfg) {
  u32 rv, core_mask;
  decContainer_t *dec_cont;
  enum DecRet ret;
  const struct DecHwFeatures *hw_feature = NULL;

  APITRACE("%s","VC1DecInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
  if ( ((-1)>>1) != (-1) ) {
    APITRACEERR("%s","VC1DecInit# ERROR: Right shift is not signed\n");
    return(DEC_INITFAIL);
  }
  /*lint -restore */

  if (dec_inst == NULL || dec_cfg->p_meta_data == NULL) {
    APITRACEERR("%s","VC1DecInit# ERROR: NULL argument\n");
    return(DEC_PARAM_ERROR);
  }

  *dec_inst = NULL; /* return NULL instance for any error */
  hw_feature = DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_VC1_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","VC1DecInit# vc1 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }
  /* create decoder container */
  dec_cont = (decContainer_t *)DWLmalloc(sizeof(decContainer_t));
  if (dec_cont == NULL) {
    APITRACEERR("%s","VC1DecInit# ERROR: Memory allocation failed\n");
    return(DEC_MEMFAIL);
  }
  DWLmemset(dec_cont, 0, sizeof(decContainer_t));

  /* init stream storage structure */
  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->dwl = dwl;
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_VC1_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);
  dec_cont->storage.dec_cont = dec_cont;
  /* Initialize decoder */
  rv = vc1hwdInit(dwl, &dec_cont->storage, dec_cfg->p_meta_data,
                  dec_cfg->num_frame_buffers, hw_feature);
  if ( rv != VC1HWD_OK) {
    APITRACEERR("%s","VC1DecInit# ERROR: Invalid initialization metadata\n");
    ret = DEC_PARAM_ERROR;
    goto err;
  }

  dec_cont->dec_stat  = VC1DEC_INITIALIZED;
  dec_cont->pic_number = 0;
  dec_cont->asic_running = 0;
  dec_cont->seq_state = SEQ_CLEAN;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  SetECPolicy(dec_cfg->error_handling, 0, &dec_cont->error_policy);
  dec_cont->vc1_regs[0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_VC1_DEC);

  SetCommonConfigRegs(dec_cont->vc1_regs);

  SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_MODE, DEC_MODE_VC1);

  /* Set prediction filter taps */
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_0, (u32)-4);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_1, 53);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_2, 18);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_0_3, (u32)-3);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_0, (u32)-1);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_1,  9);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_2,  9);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_1_3, (u32)-1);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_0, (u32)-3);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_1, 18);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_2, 53);
  SetDecRegister(dec_cont->vc1_regs, HWIF_PRED_BC_TAP_2_3, (u32)-4);

  dec_cont->max_strm_len = DEC_X170_MAX_STREAM_VCD;

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }
  dec_cont->storage.release_buffer = 0;

  /* Custom DPB modes require tiled support >= 2 */
  dec_cont->dpb_mode = DEC_DPB_NOT_INITIALIZED;

  /* take top/botom fields into consideration */
  if (FifoInit(32, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->n_guard_size = dec_cfg->guard_size;

  APITRACE("%s","VC1DecInit# OK\n");

  *dec_inst = (VC1DecInst)dec_cont;

  return(DEC_OK);

err:
  pthread_mutex_destroy(&dec_cont->protect_mutex);
  DWLfree(dec_cont);
  return ret;
}

/*------------------------------------------------------------------------------

    Function name: VC1DecDecode

    Functional description:
        Decodes VC-1 stream.

    Inputs:
        dec_inst Reference to decoder instance.
        input  Decoder input structure.

    Output:
        output Decoder output structure.

    Return values:
        DEC_PIC_DECODED        Picture is decoded.
        DEC_STRM_PROCESSED     Stream handled, no picture ready for display.
        DEC_PARAM_ERROR        Error with function parameters.
        DEC_NOT_INITIALIZED    Attempt to decode with uninitalized decoder.
        DEC_MEMFAIL            Memory allocation failed.
        DEC_HW_BUS_ERROR       A bus error
        DEC_HW_TIMEOUT         Timeout occurred while waiting for HW
        DEC_SYSTEM_ERROR       Wait for hardware failed
        DEC_HW_RESERVED        HW reservation attempt failed
        DEC_HDRS_RDY           Stream headeres decoded.
        DEC_STRM_ERROR         Stream decoding failed.
        DEC_RESOLUTION_CHANGED Picture dimensions has been changed.

------------------------------------------------------------------------------*/
enum DecRet VC1DecDecode( VC1DecInst dec_inst,
                        const VC1DecInput* input,
                        struct DecOutput* output ) {
  decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  picture_t *p_pic;
  strmData_t stream_data;
  u32 asic_status;
  u32 first_frame;
  enum DecRet return_value = DEC_PIC_DECODED;
  enum VC1HWDStatus dec_result = VC1HWD_OK;
  u16x tmp_ret_val;
#ifdef NDEBUG
  UNUSED(tmp_ret_val);
#endif
  u32 work_index = INVALID_ANCHOR_PICTURE;
  u32 ff_index;
  u32 error_detect = HANTRO_FALSE;
  u16x is_bpic = HANTRO_FALSE;
  u32 is_intra;
  const struct DecHwFeatures *hw_feature = NULL;

  APITRACE("%s","VC1DecDecode#\n");

  /* Check that function input parameters are valid */
  if (input == NULL || dec_inst == NULL) {
    APITRACEERR("%s","VC1DecDecode# ERROR: NULL argument\n");
    return(DEC_PARAM_ERROR);
  }

  if ((X170_CHECK_VIRTUAL_ADDRESS( input->stream ) ||
       X170_CHECK_BUS_ADDRESS( input->stream_bus_address ) ||
       (input->stream_size == 0) ||
       input->stream_size > dec_cont->max_strm_len)) {
    APITRACEERR("%s","VC1DecDecode# ERROR: Invalid input parameters\n");
    return(DEC_PARAM_ERROR);
  }

  /* Check if decoder is in an incorrect mode */
  if (dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecDecode# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
  }
  if(dec_cont->abort)
    return DEC_ABORTED;

  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_VC1_DEC);
  /* init strmData_t structure */
  stream_data.p_strm_buff_start = (u8*)input->stream;
  stream_data.strm_curr_pos = stream_data.p_strm_buff_start;
  stream_data.bit_pos_in_word = 0;
  stream_data.strm_buff_size = input->stream_size;
  stream_data.strm_buff_read_bits = 0;
  stream_data.strm_exhausted = HANTRO_FALSE;

  if (dec_cont->storage.profile == VC1_ADVANCED)
    stream_data.remove_emul_prev_bytes = 1;
  else
    stream_data.remove_emul_prev_bytes = 0;

  first_frame = (dec_cont->storage.first_frame) ? 1 : 0;

  if((dec_cont->dec_stat == VC1DEC_INITIALIZED) &&
      (dec_cont->storage.profile != VC1_ADVANCED)) {
    u32 tmp = AllocateMemories(dec_cont, &dec_cont->storage, dec_cont->dwl);
    enum DecRet rv;
    if(tmp != HANTRO_OK) {
      rv = DEC_MEMFAIL;
    } else {
      dec_cont->dec_stat = VC1DEC_STREAMDECODING;
      VC1SetExternalBufferInfo(dec_cont);
      dec_cont->buffer_index = 0;
      rv = DEC_WAITING_FOR_BUFFER;
    }

    /* update stream position */
    output->strm_curr_pos = stream_data.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    output->data_left = stream_data.strm_buff_size -
                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    if(dec_cont->abort)
      return(DEC_ABORTED);
    else
      return (rv);
  }

  if (dec_cont->dec_stat == VC1DEC_HEADERSDECODED &&
      (dec_cont->storage.profile == VC1_ADVANCED)) {
    /* check if buffer need to be realloced, both external buffer and internal buffer */
    VC1CheckBufferRealloc(dec_cont);
    if (!dec_cont->pp_enabled) {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        BqueueWaitNotInUse(&dec_cont->storage.bq);
#endif
        if (dec_cont->storage.ext_buffer_added) {
          dec_cont->storage.release_buffer = 1;
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          return_value = DEC_WAITING_FOR_BUFFER;
        }

        /* Allocate memories */
        (void)vc1hwdRelease(dec_cont->dwl, &dec_cont->storage);
        if(dec_cont->bit_plane_ctrl.virtual_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->bit_plane_ctrl);
        if(dec_cont->direct_mvs.bus_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->direct_mvs);

        u32 tmp = AllocateMemories(dec_cont, &dec_cont->storage, dec_cont->dwl);
        if (tmp != HANTRO_OK)
          return (DEC_MEMFAIL);
      }
    } else {
      if (dec_cont->realloc_ext_buf) {
#ifndef USE_OMXIL_BUFFER
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
#endif
        if (dec_cont->storage.ext_buffer_added) {
          dec_cont->storage.release_buffer = 1;
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          return_value = DEC_WAITING_FOR_BUFFER;
        }
      }

      if (dec_cont->realloc_int_buf) {
        /* Allocate memories */
        (void)vc1hwdRelease(dec_cont->dwl, &dec_cont->storage);
        if(dec_cont->bit_plane_ctrl.virtual_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->bit_plane_ctrl);
        if(dec_cont->direct_mvs.bus_address)
          DWLFreeLinear(dec_cont->dwl, &dec_cont->direct_mvs);

        u32 tmp = AllocateMemories(dec_cont, &dec_cont->storage, dec_cont->dwl);
        if (tmp != HANTRO_OK)
          return (DEC_MEMFAIL);
      }
    }
  }

  if (dec_cont->dec_stat == VC1DEC_HEADERSDECODED) {
    dec_cont->dec_stat = VC1DEC_STREAMDECODING;
    if (dec_cont->realloc_ext_buf) {
      dec_cont->buffer_index = 0;
      VC1SetExternalBufferInfo(dec_cont);
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      output->data_left = input->stream_size;
      return (DEC_WAITING_FOR_BUFFER);
    }
  } else if (dec_cont->buffer_index < dec_cont->ext_min_buffer_num) {
    output->strm_curr_pos = (u8 *)input->stream;
    output->strm_curr_bus_address = input->stream_bus_address;
    output->data_left = input->stream_size;
    return (DEC_WAITING_FOR_BUFFER);
  }
  /* decode SW part (picture layer) */
  if (dec_cont->storage.resolution_changed == HANTRO_FALSE) {
    dec_result = vc1hwdDecode(dec_cont, &dec_cont->storage, &stream_data);
    hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                DWL_CLIENT_TYPE_VC1_DEC);
    if (dec_cont->storage.resolution_changed) {
      /* save stream position and dec_result */
      dec_cont->storage.tmp_strm_data = stream_data;
      dec_cont->storage.prev_dec_result = dec_result;
    }
#if 0
    /* Enable PP to handle range map. */
    if (dec_cont->storage.range_map_yflag || dec_cont->storage.range_map_uv_flag)
      dec_cont->pp_enabled = 1;
#endif
  } else {
    /* continue and handle errors in pic layer decoding */
    dec_result = dec_cont->storage.prev_dec_result;
    /* restore stream position */
    stream_data = dec_cont->storage.tmp_strm_data;
    dec_cont->storage.resolution_changed = HANTRO_FALSE;
    if (dec_cont->pp_enabled) {
      PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
      (void)CalcPpUnitBufferSize(ppu_cfg, 0);
    }
  }

  output->strm_curr_pos = stream_data.strm_curr_pos;
  output->strm_curr_bus_address = input->stream_bus_address +
                                  (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
  output->data_left = stream_data.strm_buff_size -
                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

  /* return if resolution changed so that user can check PP
   * maximum scaling ratio */
  if (dec_cont->storage.resolution_changed) {
    APITRACEDEBUG("%s","VC1DecDecode# DEC_RESOLUTION_CHANGED\n");

    u32 tmpret;
    VC1DecPicture tmp_output;
    do {
      tmpret = VC1DecNextPicture_INTERNAL(dec_cont, &tmp_output, 1);
      if(tmpret == DEC_ABORTED)
        return (DEC_ABORTED);
    } while( tmpret == DEC_PIC_RDY);

    if(dec_cont->abort) {
      return(DEC_ABORTED);
    } else {
      output->strm_curr_pos = (u8 *)input->stream;
      output->strm_curr_bus_address = input->stream_bus_address;
      output->data_left = input->stream_size;
      return DEC_RESOLUTION_CHANGED;
    }
  }

  is_intra = HANTRO_FALSE;
  if(dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) {
    if( dec_cont->storage.pic_layer.field_pic_type == FP_I_I &&
        !dec_cont->storage.pic_layer.is_ff ) {
      is_intra = HANTRO_TRUE;
      dec_cont->seq_state = SEQ_CLEAN;
    }
  } else if ( dec_cont->storage.pic_layer.pic_type == PTYPE_I ) {
    is_intra = HANTRO_TRUE;
    dec_cont->seq_state = SEQ_CLEAN;
  }
  if (dec_result == VC1HWD_ERROR &&
      dec_cont->storage.pic_layer.pic_type != PTYPE_B &&
      dec_cont->storage.pic_layer.pic_type != PTYPE_BI) {
    dec_cont->seq_state = SEQ_DIRTY;
  }

  if (dec_cont->seq_state != SEQ_CLEAN) {
    if (vc1hwdECHandleInData(dec_cont, input, &stream_data, output) == EC_STATE_DISCARD)
      return DEC_STRM_PROCESSED;
  }

  if((dec_cont->storage.pic_layer.pic_type == PTYPE_B ||
      dec_cont->storage.pic_layer.pic_type == PTYPE_BI) &&
      (dec_result != VC1HWD_SEQ_HDRS_RDY &&
       dec_result != VC1HWD_ENTRY_POINT_HDRS_RDY))  {
    is_bpic = HANTRO_TRUE;

    if (input->skip_frame == DEC_SKIP_NON_REF) {

      /* skip B frame and seek next frame start */
      (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
      output->strm_curr_pos = stream_data.strm_curr_pos;
      output->strm_curr_bus_address = input->stream_bus_address +
                                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
      output->data_left = stream_data.strm_buff_size -
                          (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

      if(dec_cont->storage.profile != VC1_ADVANCED)
        output->data_left = 0;

      APITRACEERR("%s","VC1DecDecode# DEC_NONREF_PIC_SKIPPED\n");

      return DEC_NONREF_PIC_SKIPPED; /*DEC_STRM_PROCESSED;*/
    }
  }

  /* Initialize DPB mode */
  if( dec_cont->storage.interlace)
    dec_cont->dpb_mode = DEC_DPB_INTERLACED_FIELD;
  else
    dec_cont->dpb_mode = DEC_DPB_FRAME;

  /* Initialize tiled mode */

  /* Check mode validity */
  if(DecCheckTiledMode(
                        dec_cont->dpb_mode,
                        dec_cont->storage.interlace ) !=
      HANTRO_OK ) {
    APITRACEERR("%s","VC1DecDecode# ERROR: DPB mode does not support tiled "\
                "reference pictures\n");
    return DEC_PARAM_ERROR;
  }

    dec_cont->tiled_reference_enable =
      DecSetupTiledReference( dec_cont->vc1_regs,
                              dec_cont->dpb_mode,
                              dec_cont->storage.interlace );


  /* Check dec_result */
  if (dec_result == VC1HWD_SEQ_HDRS_RDY) {
    /* Force raster scan output for interlaced sequences */
    /*
    if (dec_cont->storage.interlace)
        SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_OUT_TILED_E, 0);
        */

    APITRACEDEBUG("%s","VC1DecDecode# DEC_HDRS_RDY (Sequence layer)\n");
    dec_cont->dec_stat = VC1DEC_HEADERSDECODED;
    if (dec_cont->pp_enabled) {
      dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
      dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
    }

    /* for a case, it should be go here again due to the first allocation is calulated by max_coded width,
       this is only for error concealment*/
    dec_cont->storage.first_frame = 1;
    //dec_cont->storage.outp_count = 0;
    FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
    if(dec_cont->abort)
      return(DEC_ABORTED);
    else
      return (DEC_HDRS_RDY);
  } else if (dec_result == VC1HWD_ENTRY_POINT_HDRS_RDY) {
    APITRACEDEBUG("%s","VC1DecDecode# DEC_HDRS_RDY (Entry-Point layer\n");
    dec_cont->dec_stat = VC1DEC_HEADERSDECODED;
    if (dec_cont->pp_enabled) {
      dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
      dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
    }

    dec_cont->storage.first_frame = 1;
    //dec_cont->storage.outp_count = 0;
    FifoPush(dec_cont->fifo_display, (FifoObject)-2, FIFO_EXCEPTION_DISABLE);
    if(dec_cont->abort)
      return(DEC_ABORTED);
    else
      return (DEC_HDRS_RDY);
  } else if (dec_result == VC1HWD_USER_DATA_RDY) {
    APITRACEDEBUG("%s","VC1DecDecode# DEC_STRM_PROCESSED (User data)\n");
    return (DEC_STRM_PROCESSED);
  } else if (dec_result == VC1HWD_METADATA_ERROR ) {
    APITRACEERR("%s","VC1DecDecode# DEC_STRM_ERROR\n");
    return (DEC_STRM_ERROR);
  } else if (dec_result == VC1HWD_END_OF_SEQ) {
    APITRACEDEBUG("%s","VC1DecDecode# DEC_END_OF_SEQ\n");
    return (DEC_END_OF_SEQ);
  } else if (dec_result == VC1HWD_MEMORY_FAIL) {
    APITRACEERR("%s","VC1DecDecode# DEC_MEMFAIL\n");
    return (DEC_MEMFAIL);
  } else if (dec_result == VC1HWD_HDRS_ERROR) {
    /* Handle case where sequence or entry-point headers are not
     * decoded before the first frame (all resources not allocated) */
    APITRACEERR("%s","VC1DecDecode# DEC_STRM_ERROR (header error)\n");
    return (DEC_STRM_ERROR);
  } else if (dec_result == VC1HWD_ERROR ||
             (first_frame && dec_result == VC1HWD_NOT_CODED_PIC)) {
    /* Perform error concealment */
    APITRACEERR("%s","Picture layer decoding failed!\n");
    error_detect = HANTRO_TRUE;

    /* force output index to zero if it is first frame
     * and picture is skipped */
    if (first_frame) {
      dec_cont->storage.work_out_prev =
        dec_cont->storage.work_out = BqueueNext2(
                                       &dec_cont->storage.bq,
                                       dec_cont->storage.work0,
                                       dec_cont->storage.work1,
                                       BQUEUE_UNUSED,
                                       0);
      if(dec_cont->storage.work_out == INVALID_ANCHOR_PICTURE) {
        if (dec_cont->abort)
          return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->same_pic_header = 1;
          return DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;
      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].first_show = 1;

      if (dec_cont->pp_enabled) {
        struct DWLLinearMem * pp_buffer = NULL;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
#else
        pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
#endif
        dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data = pp_buffer;

        if (pp_buffer == NULL) {
          if (dec_cont->abort)
            return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
         else {
            output->strm_curr_pos = (u8 *)input->stream;
            output->strm_curr_bus_address = input->stream_bus_address;
            output->data_left = input->stream_size;
            dec_cont->same_pic_header = 1;
            BqueuePictureRelease(&dec_cont->storage.bq, dec_cont->storage.work_out);
            return DEC_NO_DECODING_BUFFER;
          }
#endif
        }
#ifdef ENABLE_FPGA_VERIFICATION
      if (pp_buffer) {
        /* update device buffer by host buffer */
        DWLLinearMemset(dec_cont->dwl, pp_buffer, 0, 0, pp_buffer->size);
      }
#endif
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
      }
    }

    return_value = DEC_STRM_PROCESSED;
    /* discard syntax error B frame or orphan field */
    if (first_frame || (is_bpic) || dec_cont->storage.missing_field) {
      (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
      if (dec_cont->storage.missing_field &&
          !dec_cont->storage.pic_layer.is_ff) {
        /*discard first field*/
        return_value = DEC_DISCARD_INTERNAL;
      }
    }
  } else if (dec_result == VC1HWD_NOT_CODED_PIC) {
    if( dec_cont->storage.work0 == INVALID_ANCHOR_PICTURE ) {
      u16x work_out_prev_tmp, work_out_tmp;
      work_out_prev_tmp = dec_cont->storage.work_out_prev;
      work_out_tmp = dec_cont->storage.work_out;
      dec_cont->storage.work_out_prev = dec_cont->storage.work_out;
      dec_cont->storage.work0 = dec_cont->storage.work_out;
      dec_cont->storage.work_out = BqueueNext2(
                                     &dec_cont->storage.bq,
                                     dec_cont->storage.work0,
                                     dec_cont->storage.work1,
                                     BQUEUE_UNUSED, 0 );
      if(dec_cont->storage.work_out == INVALID_ANCHOR_PICTURE) {
        if (dec_cont->abort)
          return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->storage.work0 = INVALID_ANCHOR_PICTURE;
          dec_cont->storage.work_out_prev = work_out_prev_tmp;
          dec_cont->storage.work_out = work_out_tmp;
          dec_cont->same_pic_header = 1;
          return DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;
      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].first_show = 1;
    }
    /* Set picture specific information to picture_t struct */
    vc1hwdSetPictureInfo( dec_cont, input->pic_id );

    /* Clear key-frame flag from output picture, skipped pictures
     * are P pictures and therefore not key-frames... */
    p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
    p_pic[ dec_cont->storage.work_out ].key_frame = HANTRO_FALSE;
    p_pic[ dec_cont->storage.work_out ].fcm =
      dec_cont->storage.pic_layer.fcm;

    /* set non coded picture for stand alone post-processing */
    p_pic[ dec_cont->storage.work_out ].field[0].dec_pp_stat = STAND_ALONE;
    p_pic[ dec_cont->storage.work_out ].field[1].dec_pp_stat = STAND_ALONE;

    return_value = DEC_PIC_DECODED;
    tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                       dec_cont->storage.work_out,
                                       is_bpic, input->pic_id);
    ASSERT( tmp_ret_val == HANTRO_OK );
#ifndef USE_PICTURE_DISCARD
    if (dec_cont->pp_enabled)
      IncreaseInputQueueCnt(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
#endif

    /* update stream position */
    output->strm_curr_pos = stream_data.strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
    output->data_left = stream_data.strm_buff_size -
                        (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

    if(dec_cont->storage.profile != VC1_ADVANCED)
      output->data_left = 0;

#ifdef _DEC_PP_USAGE
    PrintDecPpUsage(dec_cont,
                    dec_cont->storage.pic_layer.is_ff,
                    dec_cont->storage.work_out,
                    HANTRO_TRUE,
                    input->pic_id);
#endif
  } else { /* dec_result == VC1HWD_OK */
    if(!dec_cont->storage.slice) {
      /* update picture buffer work indexes and
       * check anchor availability */
      u16x work_out_prev_tmp, work_out_tmp;
      work_out_prev_tmp = dec_cont->storage.work_out_prev;
      work_out_tmp = dec_cont->storage.work_out;
      vc1hwdUpdateWorkBufferIndexes( dec_cont, is_bpic, input->dec_ctrl);
      if(dec_cont->storage.work_out == INVALID_ANCHOR_PICTURE || (dec_cont->pp_enabled &&
         dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data == NULL)) {
        if (dec_cont->abort)
          return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else {
          output->strm_curr_pos = (u8 *)input->stream;
          output->strm_curr_bus_address = input->stream_bus_address;
          output->data_left = input->stream_size;
          dec_cont->storage.work_out_prev = work_out_prev_tmp;
          dec_cont->storage.work_out = work_out_tmp;
          dec_cont->same_pic_header = 1;
          return DEC_NO_DECODING_BUFFER;
        }
#endif
      }
      else if (dec_cont->same_pic_header)
        dec_cont->same_pic_header = 0;

#ifdef _DEC_PP_USAGE
      PrintDecPpUsage(dec_cont,
                      dec_cont->storage.pic_layer.is_ff,
                      dec_cont->storage.work_out,
                      HANTRO_TRUE,
                      input->pic_id);
#endif
      /* Set picture specific information to picture_t struct */
      vc1hwdSetPictureInfo( dec_cont, input->pic_id );

      /* write bit plane control for HW */
      WriteBitPlaneCtrl(dec_cont->bit_plane_ctrl.virtual_address,
                        dec_cont->storage.p_mb_flags, dec_cont->storage.num_of_mbs);
      DWLDMATransData(dec_cont->dwl, &dec_cont->bit_plane_ctrl, 0,
                      dec_cont->bit_plane_ctrl.size, HOST_TO_DEVICE);
    }

    /* Start HW */
    asic_status = VC1RunAsic(dec_cont, &stream_data, input->stream_bus_address, hw_feature);

    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);

    vc1ECMarkOutDataErrInfo(dec_cont,asic_status);

    if (asic_status == X170_DEC_TIMEOUT ||
        asic_status == X170_DEC_SYSTEM_ERROR ||
        asic_status == X170_DEC_HW_RESERVED ||
        asic_status & DEC_HW_IRQ_BUS) {
      error_detect = HANTRO_TRUE;
      if (dec_cont->pp_enabled) {
        InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
      }
      if (error_detect && !is_bpic)
        dec_cont->seq_state = SEQ_DIRTY;
      return vc1GetRetValByIrq(asic_status);
    } else if (asic_status & DEC_HW_IRQ_BUFFER) {
      /* update stream position */
#ifdef CASE_INFO_STAT
      if(case_info.frame_num < 10)
        case_info.slice_num[case_info.frame_num]++;
#endif
      output->strm_curr_pos = stream_data.strm_curr_pos;
      output->strm_curr_bus_address = input->stream_bus_address +
                                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
      output->data_left = stream_data.strm_buff_size -
                          (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

      if(dec_cont->storage.profile != VC1_ADVANCED)
        output->data_left = 0;

      APITRACEDEBUG("%s","VC1DecDecode# DEC_BUF_EMPTY (Buffer empty)\n");
      return (DEC_BUF_EMPTY);
    } else if (asic_status &
               (DEC_HW_IRQ_ERROR |
                DEC_HW_IRQ_ASO |
                DEC_HW_IRQ_TIMEOUT |
                DEC_HW_IRQ_ABORT) ) {
      error_detect = HANTRO_TRUE;
      vc1ECGetErrorRatio(dec_cont);

      if (first_frame || is_bpic || dec_cont->storage.slice) {
        (void)vc1hwdSeekFrameStart(&dec_cont->storage, &stream_data);
      }
      u32 is_update_recon = 0;
      if (EC_STATE_DISCARD == vc1hwdECGetOutDataAction(dec_cont)) {
        is_update_recon = 0;
        if (dec_cont->pp_enabled) {
          InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
        }
        return_value = DEC_DISCARD_INTERNAL;
        work_index = (is_bpic) ? dec_cont->storage.prev_bidx : dec_cont->storage.work_out;
      } else {
        is_update_recon = 1;
        return_value = DEC_PIC_DECODED;
      }
      vc1hwdECHandleReconData(dec_cont, is_update_recon);

      if(return_value == DEC_PIC_DECODED ) {
        /* buffer concealed frame
         * (individual fields are not concealed) */
        tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                           dec_cont->storage.work_out,
                                           is_bpic, input->pic_id);
#ifdef CASE_INFO_STAT
       VC1CaseInfoCollect(dec_cont, &case_info);
#endif
        ASSERT( tmp_ret_val == HANTRO_OK );
      }
    } else { /*(DEC_X170_IRQ_RDY)*/
      tmp_ret_val = vc1hwdBufferPicture( dec_cont,
                                         dec_cont->storage.work_out,
                                         is_bpic, input->pic_id);
      ASSERT( tmp_ret_val == HANTRO_OK );

#ifdef CASE_INFO_STAT
       VC1CaseInfoCollect(dec_cont, &case_info);
#endif

      return_value = DEC_PIC_DECODED;
      if( is_intra ) {
        dec_cont->storage.picture_broken = HANTRO_FALSE;
      }
    }

    if(error_detect && dec_cont->storage.pic_layer.pic_type != PTYPE_B ) {
      dec_cont->storage.picture_broken = HANTRO_TRUE;
    }
  }

  if (error_detect && !is_bpic)
    dec_cont->seq_state = SEQ_DIRTY;

  /* update stream position */
  output->strm_curr_pos = stream_data.strm_curr_pos;
  output->strm_curr_bus_address = input->stream_bus_address +
                                  (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);
  output->data_left = stream_data.strm_buff_size -
                      (stream_data.strm_curr_pos - stream_data.p_strm_buff_start);

  if(dec_cont->storage.profile != VC1_ADVANCED)
    output->data_left = 0;

  /* Update picture buffer work indexes */
  if (!is_bpic) {
    u32 is_second_ff = (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) &&
                       (dec_cont->storage.pic_layer.is_ff == HANTRO_FALSE);
    u32 not_field_ilace = dec_cont->storage.pic_layer.fcm != FIELD_INTERLACE;
    if (not_field_ilace || is_second_ff) {
      if( dec_cont->storage.max_bframes > 0 ) {
        dec_cont->storage.work1 = dec_cont->storage.work0;
      }
      dec_cont->storage.work0 = dec_cont->storage.work_out;
    }
  }

  /* force index to 2 for B frames
   * (prevents overwriting return value of successfully decoded pic) */
  if (INVALID_ANCHOR_PICTURE == work_index)
    work_index = (is_bpic) ? dec_cont->storage.prev_bidx : dec_cont->storage.work_out;

  p_pic = (picture_t*)dec_cont->storage.p_pic_buf;

  /* anchor picture succesfully decoded */
  if ( !is_bpic && !error_detect) {
    /* reset B picture skip after succesfully decoded anchor picture */
    if(dec_cont->storage.skip_b)
      dec_cont->storage.skip_b--;
  }

  ff_index = (dec_cont->storage.pic_layer.is_ff) ? 0 : 1;
  /* store return_value to field structure */
  if (return_value == DEC_PIC_DECODED) {
    p_pic[ work_index ].field[ff_index].return_value = DEC_PIC_RDY;
    dec_cont->pic_number++;
  } else {
    p_pic[ work_index ].field[ff_index].return_value = return_value;
  }

  /* Copy return value etc. to second field structure too... */
  if (dec_cont->storage.pic_layer.fcm != FIELD_INTERLACE)
    p_pic[ work_index ].field[1] = p_pic[ work_index ].field[0];

  u32 tmpret;
  VC1DecPicture tmp_output;
  do {
    tmpret = VC1DecNextPicture_INTERNAL(dec_cont, &tmp_output, 0);
    if(tmpret == DEC_ABORTED)
      return (DEC_ABORTED);
  } while( tmpret == DEC_PIC_RDY);

  APITRACE("%s","VC1DecDecode# OK\n");
  if(dec_cont->abort)
    return(DEC_ABORTED);
  else
    return(return_value);

  /*lint --e(550) Symbol 'tmp_ret_val' not accessed */
}

/*------------------------------------------------------------------------------

    Function name: VC1DecRelease

        Functional description:
            releases decoder resources.

        Input:
            dec_inst Reference to decoder instance.

        Return values:
            none

------------------------------------------------------------------------------*/
void VC1DecRelease(VC1DecInst dec_inst) {
  decContainer_t *dec_cont;
  const void *dwl;

  APITRACE("%s","VC1DecRelease#\n");

  if (dec_inst == NULL) {
    APITRACEERR("%s","VC1DecRelease# ERROR: dec_inst == NULL\n");
    return;
  }

  dec_cont = (decContainer_t*)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecRelease# ERROR: Decoder not initialized\n");
    return;
  }

  pthread_mutex_destroy(&dec_cont->protect_mutex);
  dwl = dec_cont->dwl;

  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);

  if(dec_cont->asic_running) {
    DWLWriteReg(dwl, dec_cont->core_id, 1 * 4, 0); /* stop HW */
    DWLReleaseHw(dwl, dec_cont->core_id);  /* release HW lock */
    dec_cont->asic_running = 0;
  }

  (void)vc1hwdRelease(dwl,
                      &dec_cont->storage);

  if(dec_cont->bit_plane_ctrl.virtual_address)
    DWLFreeLinear(dwl, &dec_cont->bit_plane_ctrl);
  if(dec_cont->direct_mvs.bus_address)
    DWLFreeLinear(dwl, &dec_cont->direct_mvs);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  if (dec_cont->storage.hrd_rate)
    DWLfree(dec_cont->storage.hrd_rate);
  if (dec_cont->storage.hrd_buffer)
    DWLfree(dec_cont->storage.hrd_buffer);
  if (dec_cont->storage.hrd_fullness)
    DWLfree(dec_cont->storage.hrd_fullness);

  if (dec_cont->pp_buffer_queue) InputQueueRelease(dec_cont->pp_buffer_queue);

  dec_cont->storage.hrd_rate = NULL;
  dec_cont->storage.hrd_buffer = NULL;
  dec_cont->storage.hrd_fullness = NULL;
  DWLfree(dec_cont);

  APITRACE("%s","VC1DecRelease# OK\n");
}

/*------------------------------------------------------------------------------

    Function: VC1DecGetInfo()

        Functional description:
            This function provides read access to decoder information.

        Inputs:
            dec_inst     decoder instance

        Outputs:
            dec_info    pointer to info struct where data is written

        Returns:
            DEC_OK             success
            DEC_PARAM_ERROR    invalid parameters

------------------------------------------------------------------------------*/
enum DecRet VC1DecGetInfo(VC1DecInst dec_inst, VC1DecInfo * dec_info) {
  decContainer_t *dec_cont;

  APITRACE("%s","VC1DecGetInfo#\n");

  if(dec_inst == NULL || dec_info == NULL) {
    APITRACEERR("%s","VC1DecGetInfo# ERROR: dec_inst or dec_info is NULL\n");
    return (DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t*)dec_inst;

  if(dec_cont->tiled_reference_enable) {
    dec_info->output_format = VC1DEC_TILED_YUV420;
  } else {
    dec_info->output_format = VC1DEC_SEMIPLANAR_YUV420;
  }

  dec_info->max_coded_width  = dec_cont->storage.max_coded_width;
  dec_info->max_coded_height = dec_cont->storage.max_coded_height;
  dec_info->coded_width     = dec_cont->storage.cur_coded_width;
  dec_info->coded_height    = dec_cont->storage.cur_coded_height;
  dec_info->par_width       = dec_cont->storage.aspect_horiz_size;
  dec_info->par_height      = dec_cont->storage.aspect_vert_size;
  dec_info->frame_rate_numerator    = dec_cont->storage.frame_rate_nr;
  dec_info->frame_rate_denominator  = dec_cont->storage.frame_rate_dr;
  dec_info->interlaced_sequence = dec_cont->storage.interlace;
  dec_info->dpb_mode        = dec_cont->dpb_mode;
  dec_info->multi_buff_pp_size = 2; /*dec_cont->storage.interlace ? 1 : 2;*/

  if(dec_info->interlaced_sequence &&
      (dec_info->dpb_mode != DEC_DPB_INTERLACED_FIELD)) {
    dec_info->output_format = VC1DEC_SEMIPLANAR_YUV420;
  } else {
    dec_info->output_format = VC1DEC_TILED_YUV420;
  }


  APITRACE("%s","VC1DecGetInfo# OK\n");

  return (DEC_OK);

}

/*------------------------------------------------------------------------------

    Function: VC1DecUnpackMetadata

        Functional description:
            Unpacks metadata elements for sequence header C from buffer,
            when metadata is packed according to SMPTE VC-1 Standard Annex J.

        Inputs:
            p_buffer     Pointer to buffer containing packed metadata. Buffer
                        must contain at least 4 bytes.
            buffer_size  Buffer size in bytes.
            p_meta_data   Pointer to stream metadata container.

        Return values:
            DEC_OK             Metadata successfully unpacked.
            DEC_PARAM_ERROR    Error with function parameters.
            DEC_METADATA_FAIL  Meta data is in wrong format or indicates
                                  unsupported tools

------------------------------------------------------------------------------*/
enum DecRet VC1DecUnpackMetaData(const u8* p_buffer, u32 buffer_size,
                                 struct DecMetaData* p_meta_data ) {
  APITRACE("%s","VC1DecUnpackMetadata#\n");

  if (buffer_size < 4) {
    APITRACEERR("%s","VC1DecUnpackMetadata# ERROR: buffer_size < 4\n");
    return(DEC_PARAM_ERROR);
  }
  if (p_buffer == NULL) {
    APITRACEERR("%s","VC1DecUnpackMetadata# ERROR: p_buffer == NULL\n");
    return(DEC_PARAM_ERROR);
  }
  if (p_meta_data == NULL) {
    APITRACEERR("%s","VC1DecUnpackMetadata# ERROR: p_meta_data == NULL\n");
    return(DEC_PARAM_ERROR);
  }

  if ( vc1hwdUnpackMetaData( p_buffer, p_meta_data ) != HANTRO_OK ) {
    APITRACEERR("%s","VC1DecUnpackMetadata# ERROR: Metadata failure\n");
    return(DEC_METADATA_FAIL);
  }

  APITRACE("%s","VC1DecUnpackMetadata# OK\n");

  return(DEC_OK);
}


/*------------------------------------------------------------------------------

    Function name: WriteBitPlaneCtrl

        Functional description:
            Write bit plane control information to SW-HW shared memory.

        Inputs:
            mb_flags     pointer to data decoded from bitplane
            num_mbs      number of macroblocks in picture

        Outputs:
            bit_plane_ctrl    pointer to SW-HW shared memory where bit plane
                            control data shall be written

        Return values:

------------------------------------------------------------------------------*/
void WriteBitPlaneCtrl(u32 *bit_plane_ctrl, u8 *mb_flags, u32 num_mbs) {

  u32 i, j;
  u32 tmp;
  u8 tmp1;
  u32 *p_tmp = bit_plane_ctrl;

  for (i = (num_mbs+9)/10; i--;) {
    tmp = 0;
    for (j = 0; j < 10; j++) {
      tmp1 = *mb_flags++;
      tmp = (tmp<<3) | (tmp1 & 0x7);
    }
    tmp <<= 2;
    *p_tmp++ = tmp;
  }

}

/*------------------------------------------------------------------------------

    Function name: VC1DecNextPicture

    Functional description:
        Retrieve next decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct
        end_of_stream Indicates whether end of stream has been reached

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK               No picture available.
        DEC_PIC_RDY          Picture ready.
        DEC_PARAM_ERROR      Error with function parameters.
        DEC_NOT_INITIALIZED  Attempt to call function with uninitalized
                                decoder.

------------------------------------------------------------------------------*/
enum DecRet VC1DecNextPicture( VC1DecInst dec_inst, VC1DecPicture *picture) {

  /* Variables */
  decContainer_t *dec_cont;
  i32 ret;

  /* Code */

  //APITRACE("%s","VC1DecNextPicture#");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    APITRACEERR("%s","VC1DecNextPicture# ERROR: picture is NULL\n");
    return(DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1SwDecNextPicture# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
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
      APITRACEDEBUG("%s","VC1DecNextPicture# DEC_END_OF_SEQ\n");
      return DEC_END_OF_STREAM;
    }
    if ((i32)i == -2) {
      APITRACEDEBUG("%s","VC1DecNextPicture# DEC_FLUSHED\n");
      return DEC_FLUSHED;
    }

    *picture = dec_cont->storage.picture_info[i];
    //for (j = 0; j < 4; j++)
    //dec_cont->storage.picture_info[i].pictures[j].output_picture = NULL;
    if (!picture->field_picture ||
        (picture->field_picture & !picture->first_field))
      ECErrorInfoReturn(picture->error_info, picture->error_ratio, picture->pic_id);

    APITRACE("%s","VC1DecNextPicture# DEC_PIC_RDY\n");
    return (DEC_PIC_RDY);
  } else
    return DEC_ABORTED;
}

/*------------------------------------------------------------------------------

    Function name: VC1DecNextPicture_INTERNAL

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
        DEC_PARAM_ERROR      Error with function parameters.
        DEC_NOT_INITIALIZED  Attempt to call function with uninitalized
                                decoder.

------------------------------------------------------------------------------*/
enum DecRet VC1DecNextPicture_INTERNAL( VC1DecInst     dec_inst,
                                      VC1DecPicture  *picture,
                                      u32            end_of_stream) {

  /* Variables */

  enum DecRet return_value = DEC_PIC_RDY;
  decContainer_t *dec_cont;
  picture_t *p_pic;
  u32 pic_index;
  u32 field_to_return = 0;
  u32 err_mbs, pic_id;
  u32 decode_id[2];
  /* Code */

  APITRACE("%s","VC1DecNextPicture_INTERNAL#\n");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    APITRACEERR("%s","VC1DecNextPicture_INTERNAL# ERROR: picture is NULL\n");
    return(DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1SwDecNextPicture_INTERNAL# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
  }

  (void) DWLmemset(picture, 0, sizeof(VC1DecPicture));
  /* Check that asic is ready */
  if(dec_cont->asic_running) {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 1 * 4, 0);   /* stop HW */

    DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);    /* release HW lock */

    dec_cont->asic_running = 0;
  }

  /* Query next picture */
  if( vc1hwdNextPicture( &dec_cont->storage, &pic_index, &field_to_return,
                         end_of_stream, 0,
                         &pic_id, decode_id, &err_mbs ) == HANTRO_NOK ) {
    picture->pictures[0].output_picture = NULL;
    picture->pictures[0].output_picture_bus_address = 0;
    picture->pictures[0].coded_width = 0;
    picture->pictures[0].coded_height = 0;
    picture->pictures[0].frame_width = 0;
    picture->pictures[0].frame_height = 0;
    picture->key_picture = 0;
    picture->range_red_frm = 0;
    picture->range_map_yflag = 0;
    picture->range_map_y = 0;
    picture->range_map_uv_flag = 0;
    picture->range_map_uv = 0;
    picture->pic_id = 0;
    picture->decode_id[0] = picture->decode_id[1] = -1;
    picture->pic_coding_type[0] = 0;
    picture->pic_coding_type[1] = 0;
    picture->interlaced = 0;
    picture->field_picture = 0;
    picture->top_field = 0;
    picture->first_field = 0;
    picture->anchor_picture = 0;
    picture->repeat_first_field = 0;
    picture->repeat_frame_count = 0;
    picture->number_of_err_mbs = 0;
    picture->cycles_per_mb = 0;
    picture->pictures[0].output_format = DEC_OUT_FRM_RASTER_SCAN;
    return_value = DEC_OK;
    picture->pictures[0].crop_params.crop_left_offset = 0;
    picture->pictures[0].crop_params.crop_out_width = 0;
    picture->pictures[0].crop_params.crop_top_offset = 0;
    picture->pictures[0].crop_params.crop_out_height = 0;
  } else {
    p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
    VC1FillPicStruct(dec_cont, picture, pic_index);
    picture->interlaced        =
      (p_pic[pic_index].fcm == PROGRESSIVE) ? 0 : 1;
    picture->field_picture      =
      (p_pic[pic_index].fcm == FIELD_INTERLACE) ? 1 : 0;
    picture->top_field          = ((1-field_to_return) ==
                                   p_pic[pic_index].is_top_field_first) ? 1 : 0;
    picture->first_field        = (p_pic[pic_index].fcm == PROGRESSIVE) ? 0 :
                                   ((field_to_return == 0) ? 1 : 0);
    picture->anchor_picture     =
      ((p_pic[pic_index].field[field_to_return].type == PTYPE_I) ||
       (p_pic[pic_index].field[field_to_return].type == PTYPE_P)) ? 1 : 0;
    picture->repeat_first_field  = p_pic[pic_index].rff;
    picture->repeat_frame_count  = p_pic[pic_index].rptfrm;
    picture->pic_id              = pic_id;
    picture->pic_coding_type[0]  = p_pic[pic_index].pic_code_type[0];
    picture->pic_coding_type[1]  = p_pic[pic_index].pic_code_type[1];
    picture->decode_id[0]        = decode_id[0];
    picture->decode_id[1]        = decode_id[1];
    picture->number_of_err_mbs   = p_pic[pic_index].number_of_err_mbs;
    picture->cycles_per_mb       = VC1CycleCount(dec_cont);
    picture->error_info          = p_pic[pic_index].error_info;
    picture->error_ratio         = p_pic[pic_index].error_ratio;
    return_value                 =
      p_pic[pic_index].field[field_to_return].return_value;

    if(return_value == DEC_PIC_RDY) {
#ifdef USE_PICTURE_DISCARD
      if (dec_cont->storage.p_pic_buf[pic_index].first_show)
#endif
      {
        dec_cont->storage.picture_info[dec_cont->fifo_index] = *picture;
#ifndef USE_PICTURE_DISCARD
        if (BqueueWaitBufNotInUse( &dec_cont->storage.bq, pic_index) != HANTRO_OK)
          return DEC_ABORTED;
        if(dec_cont->pp_enabled) {
          InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[pic_index].pp_data)));
        }
#endif
        if((field_to_return && picture->interlaced) || !picture->interlaced) {
          BqueueSetBufferAsUsed(&dec_cont->storage.bq, pic_index);
          if (dec_cont->storage.p_pic_buf[pic_index].buffered &&
               ((dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE &&
               !dec_cont->storage.pic_layer.is_ff) ||
               dec_cont->storage.pic_layer.fcm != FIELD_INTERLACE) &&
               dec_cont->storage.p_pic_buf[pic_index].first_show) {
            dec_cont->storage.p_pic_buf[pic_index].buffered--;

          }
          dec_cont->storage.p_pic_buf[pic_index].first_show = 0;
          if(dec_cont->pp_enabled) {
            InputQueueSetBufAsUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[pic_index].pp_data)));
            BqueuePictureRelease(&dec_cont->storage.bq, pic_index);
          }
        }
        FifoPush(dec_cont->fifo_display, (FifoObject)(addr_t)dec_cont->fifo_index, FIFO_EXCEPTION_DISABLE);
        dec_cont->fifo_index++;
        if(dec_cont->fifo_index == 32)
          dec_cont->fifo_index = 0;
      }
    }
  }

  return return_value;
}

/*------------------------------------------------------------------------------

    Function name: VC1DecPictureConsumed

    Functional description:
        Release specific decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to  picture struct


    Return values:
        DEC_PARAM_ERROR         Decoder instance or picture is null
        DEC_NOT_INITIALIZED     Decoder instance isn't initialized
        DEC_OK                          picture release success
------------------------------------------------------------------------------*/
enum DecRet VC1DecPictureConsumed(VC1DecInst dec_inst, VC1DecPicture * picture) {
  /* Variables */
  decContainer_t *dec_cont;
  u32 i;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;

  /* Code */

  APITRACE("%s","VC1DecPictureConsumed#\n");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    APITRACEERR("%s","VC1DecPictureConsumed# ERROR: picture is NULL\n");
    return(DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecPictureConsumed# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
  }

  if (!dec_cont->pp_enabled) {
    for(i = 0; i < dec_cont->storage.work_buf_amount; i++) {
      if(picture->pictures[0].output_picture_bus_address == dec_cont->storage.p_pic_buf[i].data.bus_address) {
        BqueuePictureRelease(&dec_cont->storage.bq, i);
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

enum DecRet VC1DecEndOfStream(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *) dec_inst;
  enum DecRet ret;
  VC1DecPicture output;

  APITRACE("%s","VC1DecEndOfStream#\n");

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecEndOfStream# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
  }

  /* Do not do it twice */
  if(dec_cont->dec_stat == VC1DEC_STREAM_END) {
    return (DEC_OK);
  }

  if (dec_cont->vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl, dec_inst);
  } else {
    if(dec_cont->asic_running) {
      /* stop HW */
      SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_E, 0);
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->vc1_regs[1] | DEC_IRQ_DISABLE);
      DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);  /* release HW lock */
      dec_cont->asic_running = 0;
    }
  }

  while((ret = VC1DecNextPicture_INTERNAL(dec_inst, &output, 1)) == DEC_PIC_RDY);
  if(ret == DEC_ABORTED) {
    return (DEC_ABORTED);
  }

  dec_cont->dec_stat = VC1DEC_STREAM_END;
  FifoPush(dec_cont->fifo_display, (FifoObject)-1, FIFO_EXCEPTION_DISABLE);

  APITRACE("%s","VC1DecEndOfStream# DEC_OK\n");
  return (DEC_OK);
}


/*------------------------------------------------------------------------------

    Function name: VC1DecPeek

    Functional description:
        Retrieve last decoded picture

    Input:
        dec_inst     Reference to decoder instance.
        picture    Pointer to return value struct

    Output:
        picture Decoder output picture.

    Return values:
        DEC_OK               No picture available.
        DEC_PIC_RDY          Picture ready.
        DEC_PARAM_ERROR      Error with function parameters.
        DEC_NOT_INITIALIZED  Attempt to call function with uninitalized
                                decoder.

------------------------------------------------------------------------------*/
enum DecRet VC1DecPeek( VC1DecInst     dec_inst,
                      VC1DecPicture  *picture ) {

  /* Variables */

  decContainer_t *dec_cont;
  picture_t *p_pic;

  /* Code */

  APITRACE("%s","VC1DecPeek#\n");

  /* Check that function input parameters are valid */
  if (picture == NULL) {
    APITRACEERR("%s","VC1DecPeek# ERROR: picture is NULL\n");
    return(DEC_PARAM_ERROR);
  }

  dec_cont = (decContainer_t *)dec_inst;

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecPeek# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
  }

  /* Query next picture */
  /* when output release thread enabled, VC1DecNextPicture_INTERNAL() called in
     VC1DecDecode(), and so dec_cont->fullness used to sample the real outCount
     in case of VC1DecNextPicture_INTERNAL() called before than VC1DecPeek() */
  u32 tmp = dec_cont->fullness;

  if( tmp == 0 ||
      ( dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE &&
        dec_cont->storage.pic_layer.is_ff ) ) {
    DWLmemset(picture, 0, sizeof(VC1DecPicture));
    return DEC_OK;
  }

  DWLmemset(picture, 0, sizeof(VC1DecPicture));
  p_pic = (picture_t*)dec_cont->storage.p_pic_buf +
          dec_cont->storage.work_out;
  VC1FillPicStruct(dec_cont, picture, dec_cont->storage.work_out);
  picture->interlaced        = (p_pic->fcm == PROGRESSIVE) ? 0 : 1;
  picture->field_picture      = 0;
  picture->top_field          = 0;
  picture->first_field        = 0;
  picture->anchor_picture     =
    ((p_pic->field[0].type == PTYPE_I) ||
     (p_pic->field[0].type == PTYPE_P)) ? 1 : 0;
  picture->repeat_first_field  = p_pic->rff;
  picture->repeat_frame_count  = p_pic->rptfrm;
  picture->pic_id             =
    dec_cont->storage.out_pic_id[0][dec_cont->storage.prev_outp_idx];
  picture->pic_coding_type[0]  = p_pic->pic_code_type[0];
  picture->pic_coding_type[1]  = p_pic->pic_code_type[1];
  picture->number_of_err_mbs   = p_pic->number_of_err_mbs;
  picture->cycles_per_mb = VC1CycleCount(dec_cont);
  return DEC_PIC_RDY;

}

void VC1SetExternalBufferInfo(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  u32 buffers = 0;
  u32 ext_buffer_size;

  ext_buffer_size = VC1GetRefFrmSize(dec_cont);

  if( dec_cont->storage.max_bframes > 0 ) {
    buffers = 3;
  } else {
    buffers = 2;
  }

  u32 newbuffers = dec_cont->storage.max_num_buffers;
  if(newbuffers > buffers)
    buffers = newbuffers;

 if (dec_cont->pp_enabled) {
    ext_buffer_size = CalcPpUnitBufferSize(&dec_cont->ppu_cfg[0], 0);
  }

  dec_cont->storage.prev_num_mbs = dec_cont->storage.num_of_mbs;

  dec_cont->ext_min_buffer_num = dec_cont->buf_num =  buffers;
  dec_cont->next_buf_size = ext_buffer_size;
}

enum DecRet VC1DecGetBufferInfo(VC1DecInst dec_inst, struct DecBufferInfo *mem_info) {
  decContainer_t  * dec_cont = (decContainer_t *)dec_inst;

  struct DWLLinearMem empty = {0, 0, 0};

  struct DWLLinearMem *buffer = NULL;
  u32 i;

  if(dec_cont == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    mem_info->cstride[0] = mem_info->ystride[0] =
                           NEXT_MULTIPLE(4 * dec_cont->storage.pic_width_in_mbs * 16, ALIGN(dec_cont->align));
    u32 frame_width = (dec_cont->storage.cur_coded_width + 15 ) & ~15;
    mem_info->cstride[0] = mem_info->ystride[0] =
                            NEXT_MULTIPLE(frame_width * 4, ALIGN(dec_cont->align));
  } else {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!dec_cont->ppu_cfg[i].enabled) continue;
      mem_info->ystride[i] = dec_cont->ppu_cfg[i].ystride;
      mem_info->cstride[i] = dec_cont->ppu_cfg[i].cstride;
    }
  }

  if (dec_cont->storage.release_buffer) {
    /* Release old buffers from input queue. */
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
      /* All buffers have been released. */
      dec_cont->storage.release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL) {
        return (DEC_MEMFAIL);
      }
      dec_cont->storage.ext_buffer_added = 0;
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

enum DecRet VC1DecAddBuffer(VC1DecInst dec_inst, struct DWLLinearMem *info) {
  decContainer_t *dec_cont = (decContainer_t *)dec_inst;
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
    dec_cont->storage.p_pic_buf[i].data = *info;
    dec_cont->storage.p_pic_buf[i].coded_width = dec_cont->storage.max_coded_width;
    dec_cont->storage.p_pic_buf[i].coded_height = dec_cont->storage.max_coded_height;
    if(dec_cont->buffer_index > dec_cont->ext_min_buffer_num) {
      /* Adding extra buffers. */
      dec_cont->storage.bq.queue_size++;
      dec_cont->storage.work_buf_amount++;
    }
  } else {
    /* Add down scale buffer. */
    InputQueueAddBuffer(dec_cont->pp_buffer_queue, info);
  }
  dec_cont->storage.ext_buffer_added = 1;
  return dec_ret;
}

static void VC1FillPicStruct(decContainer_t *dec_cont, VC1DecPicture *picture, u32 pic_index) {
  picture_t *p_pic;

  p_pic = (picture_t *) dec_cont->storage.p_pic_buf;
  PpUnitIntConfig *ppu_cfg = NULL;
  u32 i;

  if (!dec_cont->pp_enabled) {
    picture->pictures[0].output_picture    = (u8*)p_pic[pic_index].data.virtual_address;
    picture->pictures[0].output_picture_bus_address = p_pic[pic_index].data.bus_address;
    picture->pictures[0].coded_width        = p_pic[pic_index].coded_width;
    picture->pictures[0].coded_height       = p_pic[pic_index].coded_height;
    picture->pictures[0].frame_width        = ( picture->pictures[0].coded_width + 15 ) & ~15;
    picture->pictures[0].frame_height       = ( picture->pictures[0].coded_height + 15 ) & ~15;
    picture->pictures[0].pic_stride         = NEXT_MULTIPLE(picture->pictures[0].frame_width * 4,
                                                            ALIGN(dec_cont->align));
    picture->pictures[0].pic_stride_ch         = picture->pictures[0].pic_stride;
    picture->pictures[0].output_picture_chroma    = (u8*)p_pic[pic_index].data.virtual_address +
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
    ppu_cfg = p_pic[pic_index].ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
      picture->pictures[i].output_picture    = (u8*)((addr_t)p_pic[pic_index].pp_data->virtual_address + ppu_cfg->luma_offset);
      picture->pictures[i].output_picture_bus_address = p_pic[pic_index].pp_data->bus_address + ppu_cfg->luma_offset;
      picture->pictures[i].output_picture_chroma    = (u8*)((addr_t)p_pic[pic_index].pp_data->virtual_address + ppu_cfg->chroma_offset);
      picture->pictures[i].output_picture_chroma_bus_address    = p_pic[pic_index].pp_data->bus_address + ppu_cfg->chroma_offset;
      picture->pictures[i].coded_width        = ppu_cfg->scale.width;
      picture->pictures[i].coded_height       = ppu_cfg->scale.height;
      picture->pictures[i].frame_width        = NEXT_MULTIPLE(ppu_cfg->scale.width, ALIGN(ppu_cfg->align));
      picture->pictures[i].frame_height       = ppu_cfg->scale.height;
      picture->pictures[i].pic_stride         = ppu_cfg->ystride;
      picture->pictures[i].pic_stride_ch      = ppu_cfg->cstride;
      picture->pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      picture->pictures[i].crop_params.crop_left_offset = ppu_cfg[i].crop.x;
      picture->pictures[i].crop_params.crop_top_offset = ppu_cfg[i].crop.y;
      if (ppu_cfg[i].crop.width == 0)
        picture->pictures[i].crop_params.crop_out_width = ppu_cfg[i].scale.width;
      else
        picture->pictures[i].crop_params.crop_out_width = ppu_cfg[i].crop.width;
      if (ppu_cfg[i].crop.height == 0)
        picture->pictures[i].crop_params.crop_out_height = ppu_cfg[i].scale.height;
      else
        picture->pictures[i].crop_params.crop_out_height = ppu_cfg[i].crop.height;
      if (ppu_cfg->dec400_enabled)
        PpFillDec400TblInfo(ppu_cfg,
                           p_pic[pic_index].pp_data->virtual_address,
                           p_pic[pic_index].pp_data->bus_address ,
                           &picture->pictures[i].dec400_luma_table,
                           &picture->pictures[i].dec400_chroma_table);
    }

  }
  picture->key_picture        = p_pic[pic_index].key_frame;
  picture->range_red_frm       = p_pic[pic_index].range_red_frm;
  picture->range_map_yflag     = p_pic[pic_index].range_map_yflag;
  picture->range_map_y         = p_pic[pic_index].range_map_y;
  picture->range_map_uv_flag    = p_pic[pic_index].range_map_uv_flag;
  picture->range_map_uv        = p_pic[pic_index].range_map_uv;
}

static u32 VC1CycleCount(decContainer_t *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->storage.pic_height_in_mbs << 4, 16) *
             NEXT_MULTIPLE(dec_cont->storage.pic_width_in_mbs << 4, 16)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->vc1_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  return cycles;
}

void VC1EnterAbortState(decContainer_t *dec_cont) {
  dec_cont->abort = 1;
  BqueueSetAbort(&dec_cont->storage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoSetAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueSetAbort(dec_cont->pp_buffer_queue);
}


void VC1ExistAbortState(decContainer_t *dec_cont) {
  dec_cont->abort = 0;
  BqueueClearAbort(&dec_cont->storage.bq);
#ifdef USE_OMXIL_BUFFER
  FifoClearAbort(dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueClearAbort(dec_cont->pp_buffer_queue);
}

void VC1EmptyBufferQueue(decContainer_t *dec_cont) {
  BqueueEmpty(&dec_cont->storage.bq);
  dec_cont->storage.work_out = 0;
  dec_cont->storage.work_out_prev = 0;
  dec_cont->storage.work0 =
    dec_cont->storage.work1 = INVALID_ANCHOR_PICTURE;
}

void VC1StateReset(decContainer_t *dec_cont) {
  u32 buffers = 0;

  if( dec_cont->storage.max_bframes > 0 ) {
    buffers = 3;
  } else {
    buffers = 2;
  }
  u32 newbuffers = dec_cont->storage.max_num_buffers;
  if(newbuffers > buffers)
    buffers = newbuffers;

  /* Clear parameters in decContainer_t */
  dec_cont->dec_stat = VC1DEC_STREAMDECODING;
  dec_cont->pic_number = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->ext_min_buffer_num = buffers;
  dec_cont->buffer_index = 0;
#endif
  dec_cont->realloc_ext_buf = 0;
  dec_cont->realloc_int_buf = 0;
  dec_cont->fullness = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->fifo_index = 0;
  dec_cont->ext_buffer_num = 0;
#endif
  dec_cont->same_pic_header = 0;

  /* Clear parameters in swStrmStorage_t */
  dec_cont->storage.first_frame = HANTRO_TRUE;
  dec_cont->storage.picture_broken = HANTRO_FALSE;
  dec_cont->storage.prev_bidx = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->storage.work_buf_amount = buffers;
  dec_cont->storage.bq.queue_size = buffers;
#endif
  dec_cont->storage.field_to_return = 0;
  dec_cont->storage.outp_idx = 0;
  dec_cont->storage.prev_outp_idx = 0;
  dec_cont->storage.outp_count = 0;
  dec_cont->storage.field_count = 0;
  dec_cont->storage.rnd = 0;
  dec_cont->storage.skip_b = 0;
  dec_cont->storage.resolution_changed = 0;
  dec_cont->storage.prev_dec_result = 0;
  dec_cont->storage.slice = HANTRO_FALSE;
  dec_cont->storage.missing_field = HANTRO_FALSE;
  dec_cont->storage.release_buffer = 0;
  dec_cont->storage.ext_buffer_added = 0;
#ifdef CLEAR_HDRINFO_IN_SEEK
  dec_cont->storage.hdrs_decoded = 0;
#endif

#ifdef USE_OMXIL_BUFFER
  if (dec_cont->storage.p_pic_buf && !dec_cont->pp_enabled)
    (void) DWLmemset(dec_cont->storage.p_pic_buf, 0, 16 * sizeof(picture_t));
  (void) DWLmemset(dec_cont->storage.picture_info, 0, 32 * sizeof(VC1DecPicture));
#endif
  (void) DWLmemset(&dec_cont->storage.pic_layer, 0, sizeof(pictureLayer_t));
  (void) DWLmemset(&dec_cont->storage.tmp_strm_data, 0, sizeof(strmData_t));
  (void) DWLmemset(dec_cont->storage.outp_buf, 0, 16 * sizeof(u16x));
  (void) DWLmemset(dec_cont->storage.out_pic_id, 0, 32 * sizeof(u16x));
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->fifo_display)
    FifoRelease(dec_cont->fifo_display);
  FifoInit(32, &dec_cont->fifo_display);
#endif
  if (dec_cont->pp_enabled)
    InputQueueReset(dec_cont->pp_buffer_queue);
}

enum DecRet VC1DecAbort(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *) dec_inst;

  APITRACE("%s","VC1DecAbort#\n");

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecAbort# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting */
  VC1EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","VC1DecAbort# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet VC1DecAbortAfter(VC1DecInst dec_inst) {
  decContainer_t *dec_cont = (decContainer_t *) dec_inst;

  APITRACE("%s","VC1DecAbortAfter#\n");

  /* Check if decoder is in an incorrect mode */
  if (dec_inst == NULL || dec_cont->dec_stat == VC1DEC_UNINITIALIZED) {
    APITRACEERR("%s","VC1DecAbortAfter# ERROR: Decoder not initialized\n");
    return(DEC_NOT_INITIALIZED);
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
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->vc1_regs, dec_cont->vcmd_used, core_id);
    dec_cont->asic_running = 0;
  }

  /* Clear any remaining pictures from DPB */
  VC1EmptyBufferQueue(dec_cont);

  VC1StateReset(dec_cont);

  VC1ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);

  APITRACE("%s","VC1DecAbortAfter# DEC_OK\n");
  return (DEC_OK);
}

enum DecRet VC1DecSetInfo(VC1DecInst dec_inst,
                        struct VC1DecConfig *dec_cfg) {
  /*@null@ */ decContainer_t *dec_cont = (decContainer_t *)dec_inst;
  u32 pic_width = dec_cont->storage.cur_coded_width;
  u32 pic_height = dec_cont->storage.cur_coded_height;
  u32 i;
  const struct DecHwFeatures *hw_feature = NULL;
  PpUnitConfig *ppu_cfg = dec_cfg->ppu_config;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_VC1_DEC);
  if (!hw_feature) {
    APITRACEDEBUG("%s","VC1DecSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
  }
  /* ref aligment */
  dec_cont->align = dec_cfg->align;
  /* ppu alignment */
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

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, hw_feature, 8, !dec_cont->storage.interlace, 0);
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
  if (CheckPpUnitConfig(hw_feature, pic_width, pic_height, dec_cont->storage.interlace,
                        8, PP_CHROMA_420, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;
  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;
  }

  return (DEC_OK);
}


void VC1CheckBufferRealloc(decContainer_t *dec_cont) {
  dec_cont->realloc_int_buf = 0;
  dec_cont->realloc_ext_buf = 0;
  /* tile output */
  if (!dec_cont->pp_enabled) {
    if (dec_cont->use_adaptive_buffers) {
      /* Check if external buffer size is enouth */
      if (VC1GetRefFrmSize(dec_cont) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->storage.prev_coded_width != dec_cont->storage.cur_coded_width ||
          dec_cont->storage.prev_coded_height != dec_cont->storage.cur_coded_height)
        dec_cont->realloc_ext_buf = 1;
    }

    dec_cont->realloc_int_buf = 0;

  } else { /* PP output*/
    if (dec_cont->use_adaptive_buffers) {
      if (CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0) > dec_cont->n_ext_buf_size)
        dec_cont->realloc_ext_buf = 1;
      if (VC1GetRefFrmSize(dec_cont) > dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }

    if (!dec_cont->use_adaptive_buffers) {
      if (dec_cont->ppu_cfg[0].scale.width != dec_cont->prev_pp_width ||
          dec_cont->ppu_cfg[0].scale.height != dec_cont->prev_pp_height)
        dec_cont->realloc_ext_buf = 1;
      if (VC1GetRefFrmSize(dec_cont) != dec_cont->n_int_buf_size)
        dec_cont->realloc_int_buf = 1;
    }
  }
}

//only get return value by a part of asic_status values
static enum DecRet vc1GetRetValByIrq(u32 asic_status)
{
  switch(asic_status) {
  case X170_DEC_TIMEOUT:
    APITRACEERR("%s","VC1DecDecode# DEC_HW_TIMEOUT");
    return DEC_HW_TIMEOUT;
  case X170_DEC_SYSTEM_ERROR:
    APITRACEERR("%s","VC1DecDecode# DEC_SYSTEM_ERROR");
    return DEC_SYSTEM_ERROR;
  case X170_DEC_HW_RESERVED:
    APITRACEERR("%s","VC1DecDecode# DEC_HW_RESERVED");
    return DEC_HW_RESERVED;
  }

  if (asic_status & DEC_HW_IRQ_BUS) {
    APITRACEERR("%s","VC1DecDecode# DEC_HW_BUS_ERROR");
    return DEC_HW_BUS_ERROR;
  } else if (asic_status & DEC_HW_IRQ_BUFFER) {
    APITRACEERR("%s","VC1DecDecode# DEC_BUF_EMPTY (Buffer empty)");
    return (DEC_BUF_EMPTY);
  }

  return DEC_OK;
}

#ifdef CASE_INFO_STAT
void VC1CaseInfoCollect(decContainer_t *dec_cont, CaseInfo *case_info) {
  u32 pic_width,pic_height,display_width,display_height;

  pic_width = dec_cont->storage.max_coded_width;
  pic_height = dec_cont->storage.max_coded_height;
  display_width = dec_cont->storage.cur_coded_width;
  display_height = dec_cont->storage.cur_coded_height;

 if (pic_width != display_width || pic_height != display_height)
    case_info->crop_flag = 1;

  if (!case_info->frame_num) {
    case_info->decode_width = pic_width;
    case_info->decode_height = pic_height;
    case_info->display_width = display_width;
    case_info->display_height = display_height;
  } else {
    if (case_info->decode_width != pic_width
    || case_info->decode_height != pic_height) {
      case_info->decode_width = MAX (pic_width , case_info->decode_width);
      case_info->decode_height = MAX (pic_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (display_height, case_info->display_height);
    case_info->display_width = MIN (display_width, case_info->display_width);
  }
  if (case_info->frame_num < 10) {
    if (dec_cont->storage.pic_layer.fcm == PROGRESSIVE || dec_cont->storage.pic_layer.fcm == FRAME_INTERLACE) {
      switch(dec_cont->storage.pic_layer.pic_type) {
        case PTYPE_I: case_info->frame_type[case_info->frame_num] = I_FRAME; break;
        case PTYPE_B: case_info->frame_type[case_info->frame_num] = B_FRAME; break;
        case PTYPE_P: case_info->frame_type[case_info->frame_num] = P_FRAME; break;
        default: case_info->frame_type[case_info->frame_num] = BI_FRAME; break;
      }
      case_info->slice_num[case_info->frame_num]++;
      case_info->frame_num++;
    } else {
      switch(dec_cont->storage.pic_layer.field_pic_type){
        case FP_I_I: case_info->frame_type[case_info->frame_num] = FIELD_I_I; break;
        case FP_I_P: case_info->frame_type[case_info->frame_num] = FIELD_I_P; break;
        case FP_P_I: case_info->frame_type[case_info->frame_num] = FIELD_P_I; break;
        case FP_P_P: case_info->frame_type[case_info->frame_num] = FIELD_P_P; break;
        case FP_B_B: case_info->frame_type[case_info->frame_num] = FIELD_B_B; break;
        case FP_B_BI: case_info->frame_type[case_info->frame_num] = FIELD_B_BI; break;
        case FP_BI_B: case_info->frame_type[case_info->frame_num] = FIELD_BI_B; break;
        case FP_BI_BI: case_info->frame_type[case_info->frame_num] = FIELD_BI_BI; break;
      }
      case_info->slice_num[case_info->frame_num]++;
      if(dec_cont->storage.field_count % 2 == 0)
        case_info->frame_num++;
    }
    case_info->fieldmode[case_info->frame_num] = dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE? 1 : 0;
  }
  case_info->codec = DEC_MODE_VC1;
  case_info->interlace_flag = case_info->interlace_flag == 1? 1 : dec_cont->storage.interlace;
  case_info->bit_depth = 8;
  case_info->chroma_format_id = 1;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->vc1_regs, HWIF_STREAM_LEN) * 8/ pic_width)
                         * 3840/ pic_height) * 2160/ 1024/ 1024;
}
#endif

static void vc1ECGetErrorRatio(decContainer_t *dec_cont) {

  u32 error_ratio = getG1OutDataErrorRatio(dec_cont->storage.max_coded_width, dec_cont->storage.max_coded_height,
                                        GetDecRegister(dec_cont->vc1_regs, HWIF_MB_LOCATION_X),
                                        GetDecRegister(dec_cont->vc1_regs, HWIF_MB_LOCATION_Y));

  dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_ratio += error_ratio;
  if (dec_cont->storage.pic_layer.fcm != PROGRESSIVE) {
    if (!dec_cont->storage.pic_layer.is_ff) {
      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_ratio =
          dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_ratio / 2;
    }
  } else {
    dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_ratio = error_ratio;
  }
}
