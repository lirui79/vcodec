/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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
#include "basetype.h"
#include "decapicommon.h"
#include "fifo.h"
#include "vp9decapi.h"
#include "version.h"

#include "dwl.h"
#include "regdrv.h"

#include "vp9hwd_container.h"
#include "vp9hwd_asic.h"
#include "vp9hwd_headers.h"
#include "vp9hwd_output.h"
#include "sw_util.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include "dec_log.h"
#include "errorhandling.h"
#include "commonfunction.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#define VP9_MIN_EXT_BUFFERS 8
#define FLUSH_MARKER        (-3)
#define DOWN_SCALE_SIZE(w, ds) (((w)/(ds)) & ~0x1)

static u32 Vp9CheckSupport(struct Vp9DecContainer *dec_cont);
static void Vp9Freeze(struct Vp9DecContainer *dec_cont);
static i32 Vp9DecodeHeaders(struct Vp9DecContainer *dec_cont,
                            const struct Vp9DecInput *input);
static u32 Vp9ReplaceRefAvalible(struct Vp9DecContainer *dec_cont);

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void Vp9CaseInfoCollect(struct Vp9DecContainer *dec_cont, CaseInfo *case_info);
#endif

enum DecRet Vp9DecInit(Vp9DecInst *dec_inst, const void *dwl, struct Vp9DecConfig *dec_cfg) {
  struct Vp9DecContainer *dec_cont;
  u32 is_legacy = 0;
  u32 core_mask = 0, core_mask_rfc, i;
  enum DecRet ret;

  /* check that right shift on negative numbers is performed signed */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif

  if (dec_inst == NULL || dwl == NULL || dec_cfg == NULL)
    return DEC_PARAM_ERROR;

  *dec_inst = NULL; /* return NULL instance for any error */
  /* check that hevc decoding supported in HW */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_VP9_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","Vp9DecInit# Vp9 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  if ((dec_cfg->decoder_mode & DEC_LOW_LATENCY) &&
      !SwGetCoreMaskByFeature(dwl, VSI_LOW_LATENCY)) {
    APITRACEERR("%s","Vp9DecInit# Vp9 low latency not supported in HW\n");
    return DEC_PARAM_ERROR;
  }

  /* allocate instance */
  dec_cont = (struct Vp9DecContainer *)DWLmalloc(sizeof(struct Vp9DecContainer));
  if (dec_cont == NULL) {
    APITRACEERR("%s","Vp9DecInit# Memory allocation failed\n");
  	return DEC_MEMFAIL;
  }

  DWLmemset(dec_cont, 0, sizeof(struct Vp9DecContainer));
  dec_cont->dwl = dwl;
  dec_cont->n_cores = DWLReadAsicCoreCount(dwl);
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_VP9_DEC);
  SET_SECURE_MODE(dec_cont->core_mask, dec_cont->secure_mode);
  dec_cont->n_cores_available = SwGetCores(core_mask);
  core_mask_rfc = SwGetCoreMaskByFeature(dec_cont->dwl, VSI_RFC);
  if (dec_cfg->use_video_compressor && core_mask_rfc)
    dec_cont->use_video_compressor = 1;
  /* check whether core mask support rfc */
  if (dec_cont->use_video_compressor) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    if (dec_cont->core_mask & core_mask_rfc) {
      dec_cont->core_mask &= CORE_MASK(core_mask_rfc);
      dec_cont->core_mask |= bak_non_core_mask;
    }
    else {
      dec_cont->use_video_compressor = 0;
      APITRACEDEBUG("Vp9DecInit# not any core to support RFC from core_mask 0x%x, disable it\n",
                     dec_cont->core_mask);
    }
  }

  if (dec_cfg->mcinit_cfg.mc_enable && dec_cont->n_cores_available > 1) {
    dec_cont->b_mc = 1;
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_MULTICORE_E, 1);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_WRITESTAT_E, 1);
  }
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_VP9_DEC);

  for (i = 0; i < 5; i++)
    dec_cont->decoder.probs[i] = i;
  pthread_mutex_init(&dec_cont->protect_mutex, NULL);

  dec_cont->guard_size = dec_cfg->guard_size;
  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->vp9_10bit_support = SwGetCoreMaskByFeature(dwl, VSI_VP9_PROFILE2) ? 1 : 0;
  if ((DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_VP9_DEC) & 0x0000ffff) < 0x1030 )
    dec_cont->decoder.tile_transpose = 0; /* not support before 0x1030. */
  else
    dec_cont->decoder.tile_transpose = dec_cfg->tile_transpose;

  /* low latency: set register for low latency mode */
  if (dec_cfg->decoder_mode & DEC_LOW_LATENCY) {
#ifndef ASIC_TRACE_SUPPORT
    dec_cont->low_latency = 1;
    dec_cont->llstrminfo.updated_reg = 0;
#endif
#ifdef SUPPORT_DMA
    dec_cont->llstrminfo.dwl_inst = dwl;
#endif
    /* Set flag for using ddr_low_latency model and set pollmode and polltime swregs */
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_MC_POLLMODE, 0);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_MC_POLLTIME, 0);
    /* Set flag for using ddr_low_latency model and set pollmode and polltime swregs */
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_STATUS_EXT_BUFFER_E, 1);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_BLOCK_BUFFER_MODE_E, 1);
    dec_cont->llstrminfo.strm_status_in_buffer = 1;
    dec_cont->decoder.tile_transpose = 0;
  } else {
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_BLOCK_BUFFER_MODE_E, 0);
  }
  dec_cont->align = dec_cfg->align;
#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  /* initial setup of instance */

  dec_cont->dec_stat = VP9DEC_INITIALIZED;
  dec_cont->checksum = dec_cont; /* save instance as a checksum */
  if (dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS)
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;

  Vp9AsicInit(dec_cont, dec_cfg->multicore_poll_period); /* Init ASIC */
  dec_cont->pic_number = dec_cont->display_number = 1;

  dec_cont->error_info = DEC_NO_ERROR;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  dec_cont->error_handling = dec_cfg->error_handling;
  dec_cont->error_policy = HANTRO_FALSE;
  dec_cont->picture_broken = HANTRO_FALSE;
  // dec_cont->entropy_broken = HANTRO_FALSE;
  dec_cont->decoder.refbu_pred_hits = 0;


  if (FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_out) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  if (FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  if (pthread_mutex_init(&dec_cont->sync_out, NULL) ||
      pthread_cond_init(&dec_cont->sync_out_cv, NULL)) {
    ret = DEC_SYSTEM_ERROR;
    goto err;
  }

  dec_cont->num_buffers = dec_cfg->num_frame_buffers;
  /* For external reference buffer, initially there will be num_frame_buffers reference buffer,
      and dynamic_buffer_limit can be increased to VP9DEC_DYNAMIC_PIC_LIMIT. */
  dec_cont->dynamic_buffer_limit = VP9DEC_DYNAMIC_PIC_LIMIT;

  dec_cont->min_buffer_num = dec_cfg->num_frame_buffers;  /* TODO(min): what's minimum output buffers num? */
  if (dec_cont->min_buffer_num < VP9_MIN_EXT_BUFFERS)
    dec_cont->min_buffer_num = VP9_MIN_EXT_BUFFERS;

  dec_cont->num_buffers_reserved = dec_cont->num_buffers;
  dec_cont->bq = Vp9BufferQueueInitialize(dec_cont->num_buffers);
  if (dec_cont->bq == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  if (SwGetCoreMaskByFeature(dec_cont->dwl, VSI_VP9_HW_PROB) && !dec_cont->b_mc) {
    dec_cont->decoder.entropy_last.mem_type = DWL_MEM_TYPE_DMA_HOST_AND_DEVICE |
                                              DWL_MEM_TYPE_VPU_WORKING_SPECIAL;
    if ( DWL_OK != DWLMallocLinear(dec_cont->dwl, NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 256) * 5,
                                   &dec_cont->decoder.entropy_last)) {
      ret = DEC_MEMFAIL;
      goto err;
    }
  }

  dec_cont->vcmd_used = DWLVcmdIsUsed(dec_cont->dwl);
  if (dec_cont->vcmd_used && dec_cont->b_mc) {
    /* Allows the maximum cmd buffers as real cores number. */
    FifoInit(dec_cont->n_cores_available, &dec_cont->fifo_core);
    for (i = 0; i < dec_cont->n_cores_available; i++) {
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)i, FIFO_EXCEPTION_DISABLE);
    }
  }

  /* return new instance to application */
  *dec_inst = (Vp9DecInst)dec_cont;

  (void)is_legacy;
  return DEC_OK;

err:
  pthread_mutex_destroy(&dec_cont->protect_mutex);
  pthread_mutex_destroy(&dec_cont->sync_out);
  pthread_cond_destroy(&dec_cont->sync_out_cv);
  DWLfree(dec_cont);
  return ret;
}

void Vp9DecRelease(Vp9DecInst dec_inst) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  u32 i;

  /* Check for valid decoder instance */
  if (dec_cont == NULL || dec_cont->checksum != dec_cont)
    return;

  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (dec_cont->hw_rdy_callback_arg[i]) {
      DWLfree(dec_cont->hw_rdy_callback_arg[i]);
      dec_cont->hw_rdy_callback_arg[i] = NULL;
    }
  }
  pthread_mutex_destroy(&dec_cont->protect_mutex);

  ASSERT(dec_cont->asic_running == 0);

  Vp9AsicReleaseMem(dec_cont);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  if (SwGetCoreMaskByFeature(dec_cont->dwl, VSI_VP9_HW_PROB) && !dec_cont->b_mc) {
    if (dec_cont->decoder.entropy_last.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->decoder.entropy_last);
      dec_cont->decoder.entropy_last.virtual_address = NULL;
    }
  }

  Vp9AsicReleaseFilterBlockMem(dec_cont);
  Vp9AsicReleasePictures(dec_cont);

  if (dec_cont->fifo_out) FifoRelease(dec_cont->fifo_out);
  if (dec_cont->fifo_display) FifoRelease(dec_cont->fifo_display);
  if (dec_cont->vcmd_used && dec_cont->b_mc) {
    if (dec_cont->fifo_core)
      FifoRelease(dec_cont->fifo_core);
  }

  pthread_cond_destroy(&dec_cont->sync_out_cv);
  pthread_mutex_destroy(&dec_cont->sync_out);

#ifdef FPGA_PERF_AND_BW
  AveragePerfInfoPrint(&dec_cont->perf_info);
#endif

  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  return;
}

enum DecRet Vp9DecGetInfo(Vp9DecInst dec_inst, struct Vp9DecInfo *dec_info) {
  const struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;

  if (dec_inst == NULL || dec_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return DEC_NOT_INITIALIZED;
  }

  if (dec_cont->dec_stat == VP9DEC_INITIALIZED) {
    return DEC_HDRS_NOT_RDY;
  }

  dec_info->vp_version = dec_cont->decoder.vp_version;
  dec_info->vp_profile = dec_cont->decoder.vp_profile;
  dec_info->bit_depth = dec_cont->decoder.bit_depth;

  /* Fragments have 8 pixels */
  dec_info->coded_width = dec_cont->decoder.width;
  dec_info->coded_height = dec_cont->decoder.height;
  dec_info->frame_height = NEXT_MULTIPLE(dec_cont->decoder.height, 8);
  dec_info->frame_width = NEXT_MULTIPLE(dec_cont->decoder.width, 8);

  dec_info->scaled_width = dec_cont->decoder.scaled_width;
  dec_info->scaled_height = dec_cont->decoder.scaled_height;
  dec_info->dpb_mode = DEC_DPB_FRAME;

  dec_info->output_format = DEC_OUT_FRM_TILED_4X4;

  /* Reference buffer. */
  dec_info->pic_stride = NEXT_MULTIPLE(dec_cont->decoder.width, 8) * dec_info->bit_depth / 8;

  dec_info->pic_buff_size = dec_cont->min_buffer_num;
  return DEC_OK;
}

extern void Vp9AsicStrmTileInfoCreate(struct Vp9DecContainer *dec_cont,
                          u8* strm_vir_address, u32 data_len,
                          u8* buf_vir_address, u32 buf_len);

enum DecRet Vp9DecDecode(Vp9DecInst dec_inst, const struct Vp9DecInput *input,
                         struct DecOutput *output) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  u32 picture_broken = HANTRO_FALSE;
  i32 ret;

  /* Check that function input parameters are valid */
  if (input == NULL || output == NULL || dec_inst == NULL)
    return DEC_PARAM_ERROR;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont)
    return DEC_NOT_INITIALIZED;

  if (dec_cont->abort)
    return DEC_ABORTED;

  dec_cont->input_data_len = input->data_len; // used to generate error stream
  if (dec_cont->no_decoding_buffer) {
    // If no decoding buffer, go to request new buffer directly.
    goto request_free_buffer;
  }

  if ((input->data_len > DEC_X170_MAX_STREAM_G2) ||
      (input->data_len == 0) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->buffer) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(input->buffer_bus_address)) {
    APITRACEERR("input params error, data_len %d, stream %p, stream_bus_address %p, buffer %p, buffer_bus_address %p\n",
                 input->data_len, (void *)input->stream, (void *)input->stream_bus_address,
                 (void *)input->buffer, (void *)input->buffer_bus_address);
    return DEC_PARAM_ERROR;
  }

  /* If there are more buffers to be allocated or to be freed, waiting for buffers ready. */
  if (dec_cont->buf_to_free != NULL || dec_cont->buf_not_added == 1 ||
      (dec_cont->next_buf_size != 0 && dec_cont->buffer_num_added < dec_cont->min_buffer_num)) {
    if (dec_cont->abort)
      return(DEC_ABORTED);
    else {
      ret = DEC_WAITING_FOR_BUFFER;
      goto RETURN_FOR_BUFFER;
    }
  }

  // ret = Vp9AsicAllocateMem(dec_cont);
  // if (ret != DEC_OK) {
  //   if (dec_cont->abort)
  //     return(DEC_ABORTED);
  //   else
  //     goto RETURN_FOR_BUFFER;
  // }

  if (dec_cont->dec_stat == VP9DEC_NEW_HEADERS) {
    /* TODO: Allocate picture buffers here. */
    ret = Vp9AsicAllocatePictures(dec_cont);
    if (ret != 0) {
      if (dec_cont->abort)
        return(DEC_ABORTED);
      else if (ret == DEC_MEMFAIL) {
        return(ret);
      } else {
        ret = DEC_WAITING_FOR_BUFFER;
        goto RETURN_FOR_BUFFER;
      }
    }
    dec_cont->dec_stat = VP9DEC_DECODING;
  }
  else if (dec_cont->dec_stat == VP9DEC_WAITING_FOR_BUFFER) {
  }
  else if (dec_cont->input_data_len) {
    /* Decode SW part of the frame */
    ret = Vp9DecodeHeaders(dec_cont, input);
    if (ret) {
      if (dec_cont->abort)
        return(DEC_ABORTED);
      else {
        if (ret == DEC_INFOPARAM_ERROR) {
          return DEC_INFOPARAM_ERROR;
        } else if (ret == DEC_STRM_ERROR) {
          dec_cont->picture_broken = HANTRO_TRUE;
          goto RETURN_FOR_BUFFER;
        } else if (ret == DEC_HDRS_RDY) {
          FifoPush(dec_cont->fifo_out, (void *)FLUSH_MARKER, FIFO_EXCEPTION_DISABLE);
          goto RETURN_FOR_BUFFER;
        } else {
          goto RETURN_FOR_BUFFER;
        }
      }
    }
  }
  /* missing picture, conceal */
  else {
    if (/*dec_cont->force_intra_freeze || */dec_cont->prev_is_key) {
      dec_cont->decoder.probs_decoded = 0;
      Vp9Freeze(dec_cont);
      ret = DEC_PIC_DECODED;
      goto RETURN_FOR_BUFFER;
    }
  }

request_free_buffer:
  ret = Vp9AsicAllocateMem(dec_cont);
  if (ret != DEC_OK) {
    if (dec_cont->abort)
      return(DEC_ABORTED);
    else
      goto RETURN_FOR_BUFFER;
  }
  /* low latency: strm_status_addr point to strm_status buffer. */
  if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
    dec_cont->llstrminfo.strm_status_addr = dec_cont->llstrminfo.strm_status.virtual_address;
    dec_cont->lltileinfo.tile_status_addr = ((u32 *)dec_cont->llstrminfo.strm_status.virtual_address + 2);
  }

  /* Get free picture buffer */
  ret = Vp9GetRefFrm(dec_cont, input);
  if(ret == DEC_ABORTED) {
    dec_cont->no_decoding_buffer = 0;
    return ret;
  } else if (ret == DEC_NO_DECODING_BUFFER) {
    dec_cont->no_decoding_buffer = 1;
    goto RETURN_FOR_BUFFER;
  } else if (ret) {
    dec_cont->no_decoding_buffer = 0;
    dec_cont->dec_stat = VP9DEC_WAITING_FOR_BUFFER;
    if (dec_cont->abort)
      return(DEC_ABORTED);
    else
    {
      goto RETURN_FOR_BUFFER;
    }
  } else
    dec_cont->no_decoding_buffer = 0;

#if 0
  /* Verify we have enough auxilary buffer for filter tile edge data. */
  ret = Vp9AsicAllocateFilterBlockMem(dec_cont);
  if (ret) {
    dec_cont->dec_stat = VP9DEC_WAITING_FOR_BUFFER;
    if (dec_cont->abort)
      return(DEC_ABORTED);
    else
      goto RETURN_FOR_BUFFER;
  }
#endif

  dec_cont->dec_stat = VP9DEC_DECODING;

  /* Although there are multi cores, they may not be run in parallel
     due to limitations VP9 coding tools, in this case, SW swith from
     MC to SC, or SC to MC internally which is invisible to application,
     In this function, SW determine which mode(SC/MC) should be used */
  //TODO：should we remove this function in new tile-mc mode?
  //if (dec_cont->b_mc)
  //  Vp9DetermineCoreMode(dec_cont);

  /* prepare asic */
  //Vp9AsicProbUpdate(dec_cont);

  Vp9AsicStrmPosUpdate(dec_cont, input->stream_bus_address, dec_cont->input_data_len,
                       input->buffer_bus_address, input->buff_len);

  Vp9AsicInitPicture(dec_cont);

  if (dec_cont->low_latency)
    Vp9AsicStrmTileInfoCreate(dec_cont, input->stream, input->frame_len,
                           input->buffer, input->buff_len);
  else
    Vp9AsicStrmTileInfoCreate(dec_cont, input->stream, dec_cont->input_data_len,
                           input->buffer, input->buff_len);


  DWLDMATransData2(dec_cont->dwl, (addr_t)input->buffer_bus_address,
                  (void *)input->buffer,
                  input->buff_len, HOST_TO_DEVICE);

  /* rollback DEC_EC_SEEK_NEXT_I to DEC_EC_NO_SKIP */
  if (dec_cont->decoder.key_frame || dec_cont->decoder.intra_only) {
    dec_cont->error_info = DEC_NO_ERROR; /* the ERROR flag needs to be cleared. */
    if (dec_cont->picture_broken &&
        dec_cont->error_policy & DEC_EC_SEEK_NEXT_I &&
        dec_cont->error_policy & DEC_EC_REF_REPLACE &&
        dec_cont->use_video_compressor) {
      dec_cont->error_policy &= ~DEC_EC_SEEK_NEXT_I;
      dec_cont->error_policy |= DEC_EC_NO_SKIP;
    }
  }

  /* need skip run asic ?? */
  if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only) {
    if (dec_cont->picture_broken && dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) {
      picture_broken = HANTRO_TRUE;
    }
    if (dec_cont->error_policy & DEC_EC_REF_REPLACE && dec_cont->use_video_compressor) {
      u32 tmp = Vp9ReplaceRefAvalible(dec_cont);
      if (tmp == HANTRO_NOK) {
        /* not find avalible replace ref pic for error ref : SEEK NEXT I */
        picture_broken = HANTRO_TRUE;
        dec_cont->error_policy &= ~DEC_EC_NO_SKIP;
        dec_cont->error_policy |= DEC_EC_SEEK_NEXT_I;
      }
    }
  }

  /* run the hardware */
  if (!picture_broken) {
    ret = Vp9AsicRun(dec_cont, input->pic_id);
    if (ret == DEC_MEMFAIL) goto RETURN_FOR_BUFFER;
  } else {
    dec_cont->error_info = DEC_FRAME_ERROR;
    Vp9SetupPicToOutput(dec_cont, input->pic_id);
    // Vp9UpdateRefs(dec_cont);
  }
#ifdef CASE_INFO_STAT
  Vp9CaseInfoCollect(dec_cont, &case_info);
#endif

  /* process IRQ & set output pic */
  ret = VP9SyncAndOutput(dec_cont);
#ifdef FPGA_PERF_AND_BW
  if(!dec_cont->b_mc)
    DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info,
                     NEXT_MULTIPLE(dec_cont->decoder.height, 8) *
                     NEXT_MULTIPLE(dec_cont->decoder.width, 8),
                     dec_cont->decoder.bit_depth);
#endif

  if (ret) {
    if (dec_cont->abort)
      return(DEC_ABORTED);
    else
      goto RETURN_FOR_BUFFER;
  }

  ret = DEC_PIC_DECODED;

RETURN_FOR_BUFFER:
  if (ret == DEC_WAITING_FOR_BUFFER ||
      ret == DEC_HDRS_RDY ||
      ret == DEC_NO_DECODING_BUFFER) {
    output->data_left = dec_cont->input_data_len;
    if (dec_cont->low_latency)
      output->data_left = dec_cont->input_data_len = dec_cont->llstrminfo.stream_info.send_len;
    output->strm_curr_pos = input->stream;
    output->strm_curr_bus_address = input->stream_bus_address;
  } else {
    output->data_left = 0;
    output->strm_curr_pos = input->stream + input->data_len;
    output->strm_curr_bus_address =
        input->stream_bus_address + input->data_len;
    if (dec_cont->low_latency) {
      dec_cont->input_data_len = dec_cont->llstrminfo.stream_info.send_len;
      output->strm_curr_pos = input->stream + dec_cont->llstrminfo.stream_info.send_len;
      output->strm_curr_bus_address =
          input->stream_bus_address + dec_cont->llstrminfo.stream_info.send_len;
      output->llstrm_curr_address = dec_cont->llstrm_curr_address;
    }

    if (output->strm_curr_bus_address >= (input->buffer_bus_address + input->buff_len)) {
      output->strm_curr_bus_address -= input->buff_len;
      output->strm_curr_pos -= input->buff_len;
    }
  }

  return ret;
}

u32 Vp9CheckSupport(struct Vp9DecContainer *dec_cont) {
  if ((dec_cont->decoder.bit_depth == 12) ||
      (dec_cont->decoder.bit_depth == 10 && !dec_cont->vp9_10bit_support)) {
    return HANTRO_NOK;
  }

  return HANTRO_OK;
}

void Vp9Freeze(struct Vp9DecContainer *dec_cont) {
  /* Rollback entropy probabilities if refresh is not set */
  if (dec_cont->decoder.probs_decoded &&
      dec_cont->decoder.refresh_entropy_probs == 0) {
    if (dec_cont->hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
      DWLmemcpy(&dec_cont->decoder.entropy, dec_cont->decoder.entropy_last.virtual_address,
                sizeof(struct Vp9EntropyProbs));
    } else {
      DWLmemcpy(&dec_cont->decoder.entropy, &dec_cont->decoder.entropys,
                sizeof(struct Vp9EntropyProbs));
    }
  }
  // /* lost accumulated coeff prob updates -> force video freeze until next
  //  * keyframe received */
  // else if (dec_cont->prob_refresh_detected == 1) {
  //   dec_cont->force_intra_freeze = 1;
  // }

  dec_cont->picture_broken = HANTRO_TRUE;

  /* TODO Reset mv memory if needed? */
}

i32 Vp9DecodeHeaders(struct Vp9DecContainer *dec_cont,
                     const struct Vp9DecInput *input) {
  i32 ret;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  struct Vp9Decoder *dec = &dec_cont->decoder;

  dec_cont->prev_is_key = dec->key_frame;
  dec->prev_is_key_frame = dec->key_frame;
  dec->prev_show_frame = dec->show_frame;
  dec->probs_decoded = 0;
  dec->last_width = dec->width;
  dec->last_height = dec->height;

  DWLmemset(&dec->delta_probs, 0 , sizeof(struct Vp9DeltaProbs));
  /* decode frame tag */
  ret = Vp9DecodeFrameTag(input->stream, dec_cont->input_data_len, input->buffer, input->buff_len, dec_cont);
  if (ret != HANTRO_OK) {
    if (dec->bit_depth == 12)
      return DEC_STREAM_NOT_SUPPORTED;
    else if ((dec->bit_depth == 10) && !dec_cont->vp9_10bit_support)
      return DEC_STREAM_NOT_SUPPORTED;
    else if (dec_cont->dec_stat == VP9DEC_INITIALIZED && dec->intra_only == 0)
      return DEC_STRM_PROCESSED;
    else if ((dec_cont->pic_number == 1 && !dec->prev_is_key_frame) ||
             dec_cont->dec_stat != VP9DEC_DECODING)
      return DEC_STRM_ERROR;
    else {
      Vp9Freeze(dec_cont);
      return DEC_PIC_DECODED;
    }
  } else if (!dec->key_frame && !dec->intra_only && dec_cont->dec_stat == VP9DEC_INITIALIZED) {
    return DEC_STRM_PROCESSED;
  } else if (ret == HANTRO_OK && dec->show_existing_frame) {
    asic_buff->out_buffer_i =
      Vp9BufferQueueGetRef(dec_cont->bq, dec->show_existing_frame_index);
#ifdef CASE_INFO_STAT
    case_info.show_existing_frame = 1;
#endif
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[asic_buff->out_buffer_i]) {
        return DEC_STRM_PROCESSED;
    }
#endif
    if(Vp9BufferQueueAddRef(dec_cont->bq, asic_buff->out_buffer_i)==HANTRO_NOK)
      return DEC_STRM_PROCESSED;
    if(Vp9BufferQueueAddRef(dec_cont->pp_bq, asic_buff->pp_buffer_map[asic_buff->out_buffer_i])==HANTRO_NOK)
      return DEC_STRM_PROCESSED;
    Vp9SetupPicToOutput(dec_cont, input->pic_id);
    asic_buff->out_buffer_i = -1;
    Vp9PicToOutput(dec_cont, 0);

    return DEC_PIC_DECODED;
  }
  /* low latency: need stream_info read byte in low latency mode. */
  dec_cont->bc.stream_info = &dec_cont->llstrminfo.stream_info;
  /* Decode frame header (now starts bool coder as well) */
  if (dec_cont->hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
    ret = Vp9FrameHeaderParser(input->stream + dec->frame_tag_size,
                               dec_cont->input_data_len - dec->frame_tag_size,
                               &dec_cont->bc, input->buffer,
                               input->buff_len, &dec_cont->decoder,
                               dec_cont->secure_mode);
  } else {
    ret = Vp9DecodeFrameHeader(input->stream + dec->frame_tag_size,
                               dec_cont->input_data_len - dec->frame_tag_size,
                               &dec_cont->bc, input->buffer,
                               input->buff_len, &dec_cont->decoder,
                               dec_cont->secure_mode);
  }

  if (ret != HANTRO_OK) {
    if ((dec_cont->pic_number == 1 && !dec->prev_is_key_frame) ||
        dec_cont->dec_stat != VP9DEC_DECODING)
      return DEC_STRM_ERROR;
    else {
      Vp9Freeze(dec_cont);
      return DEC_PIC_DECODED;
    }
  }
  /* flag the stream as non "error-resilient" */
  else if (dec->refresh_entropy_probs)
    dec_cont->prob_refresh_detected = 1;

  ret = Vp9SetPartitionOffsets(input->stream, dec_cont->input_data_len,
                               &dec_cont->decoder, dec_cont->secure_mode,
			       &dec_cont->llstrminfo.stream_info);
  /* ignore errors in partition offsets if HW error concealment used
   * (assuming parts of stream missing -> partition start offsets may
   * be larger than amount of stream in the buffer) */
  if (ret != HANTRO_OK) {
    if ((dec_cont->pic_number == 1 && !dec->prev_is_key_frame) ||
        dec_cont->dec_stat != VP9DEC_DECODING)
      return DEC_STRM_ERROR;
    else {
      Vp9Freeze(dec_cont);
      return DEC_PIC_DECODED;
    }
  }

  asic_buff->width = NEXT_MULTIPLE(dec->width, 8);
  asic_buff->height = NEXT_MULTIPLE(dec->height, 8);

  /* If the frame dimensions are not supported by HW,
     release allocated picture buffers and return error */
  if (((dec_cont->width != dec->width) || (dec_cont->height != dec->height))) {
    /* check for minimum and maximum dimensions */
    SwAdjustCoreMaskByWxH(dec_cont->dwl, asic_buff->width, asic_buff->height,
                          1, DWL_CLIENT_TYPE_VP9_DEC, &dec_cont->core_mask);
    if (CORE_MASK(dec_cont->core_mask) == 0) {
      APITRACEERR("%s","Vp9DecodeHeaders# no any core mask support the header info\n");
      return DEC_STREAM_NOT_SUPPORTED;
    }
    if (Vp9CheckSupport(dec_cont) != HANTRO_OK) {
      //Vp9AsicReleaseFilterBlockMem(dec_cont);
      //Vp9AsicReleasePictures(dec_cont);
      //dec_cont->dec_stat = VP9DEC_INITIALIZED;
      return DEC_STREAM_NOT_SUPPORTED;
    }
    dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                          DWL_CLIENT_TYPE_VP9_DEC);
  }

  if (((dec_cont->width != dec->width) ||
      (dec_cont->height != dec->height)) &&
      dec_cont->pp_enabled) {
    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    u32 i = 0;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      /* flexible scale ratio */
      if(!ppu_cfg->crop.set_by_user) {
        /*support odd crop*/
        if(dec_cont->hw_feature->crop_step_rshift) {
          ppu_cfg->crop.width = (dec->width+1) & ~0x1;
          ppu_cfg->crop.height = (dec->height+1) & ~0x1;
        } else {
          ppu_cfg->crop.width = dec->width;
          ppu_cfg->crop.height = dec->height;
        }
      }
      if(ppu_cfg->scale.scale_by_ratio && ppu_cfg->scale.ratio_x) {
        /*support odd crop*/
        if (dec_cont->hw_feature->crop_step_rshift) {
          ppu_cfg->scale.width = DOWN_SCALE_SIZE(ppu_cfg->crop.width,
                                                 ppu_cfg->scale.ratio_x);
          ppu_cfg->scale.height = DOWN_SCALE_SIZE(ppu_cfg->crop.height,
                                                 ppu_cfg->scale.ratio_y);
        } else {
          ppu_cfg->scale.width = ppu_cfg->crop.width / ppu_cfg->scale.ratio_x;
          ppu_cfg->scale.height = ppu_cfg->crop.height / ppu_cfg->scale.ratio_y;
        }
      } else if(!ppu_cfg->scale.enabled) {
        ppu_cfg->scale.width = ppu_cfg->crop.width;
        ppu_cfg->scale.height = ppu_cfg->crop.height;
      }
    }
    /*support odd crop*/
    if (dec_cont->hw_feature->crop_step_rshift) {
      if (CheckPpUnitConfig(dec_cont->hw_feature, ((dec->width+1)&~0x1), ((dec->height+1)&~0x1), 0, dec->bit_depth,
                            PP_CHROMA_420,  dec_cont->ppu_cfg))
        return DEC_INFOPARAM_ERROR;
    } else {
      if (CheckPpUnitConfig(dec_cont->hw_feature, dec->width, dec->height, 0, dec->bit_depth,
                            PP_CHROMA_420,  dec_cont->ppu_cfg))
        return DEC_INFOPARAM_ERROR;
    }
    if ((dec->last_width != dec->width) || (dec->last_height != dec->height))
      CalcPpUnitBufferSize(&dec_cont->ppu_cfg[0], 0);
  }

  dec_cont->width = dec->width;
  dec_cont->height = dec->height;

  if (dec_cont->dec_stat == VP9DEC_INITIALIZED) {
    dec_cont->dec_stat = VP9DEC_NEW_HEADERS;
    return DEC_HDRS_RDY;
  }

  /* If we are here and dimensions are still 0, it means that we have
   * yet to decode a valid keyframe, in which case we must give up. */
  if (dec_cont->width == 0 || dec_cont->height == 0) {
    return DEC_STRM_PROCESSED;
  }

  return DEC_OK;
}

enum DecRet Vp9DecAddBuffer(Vp9DecInst dec_inst,
                            struct DWLLinearMem *info) {
  struct Vp9DecContainer *dec_cont;// = (struct Vp9DecContainer *)dec_inst;
  struct DecAsicBuffers *asic_buff;// = dec_cont->asic_buff;
  enum DecRet dec_ret = DEC_OK;

  if (dec_inst == NULL)
  	return DEC_PARAM_ERROR;
  dec_cont = (struct Vp9DecContainer *)dec_inst;
  asic_buff = dec_cont->asic_buff;

  if (info == NULL ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->logical_size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }
  if (dec_cont->buffer_index >= MAX_PIC_BUFFERS)
    return DEC_EXT_BUFFER_REJECTED;

  APITRACEDEBUG("%s dds external buffer with size %d\n","Vp9DecAddBuffer#", info->size);

  switch (dec_cont->buf_type) {
  case REFERENCE_BUFFER: {
    dec_cont->add_buffer = 0;

    if (dec_cont->buffer_index == dec_cont->num_buffers) {
      /* Need to allocate a new buffer during decoding... */
      dec_cont->num_buffers++;
      dec_cont->add_buffer = 1;
    }

    ASSERT(dec_cont->buffer_index < dec_cont->num_buffers);
    asic_buff->pictures[dec_cont->buffer_index] = *info;
    if (!DWL_DEVMEM_VAILD(asic_buff->dpb_parasitic_buf[dec_cont->buffer_index])) {
      Vp9AllocParasiticBuf(dec_cont->dwl, dec_cont->next_dpb_parasitic_buf_size,
          &asic_buff->dpb_parasitic_buf[dec_cont->buffer_index], dec_cont->secure_mode);
    }
    dec_cont->buffer_num_added++;
    if (dec_cont->buf_num)
      dec_cont->buf_num--;
    //if (dec_cont->dec_stat == VP9DEC_NEW_HEADERS)
    dec_cont->buffer_index++;

    if (dec_cont->buffer_index >= dec_cont->num_buffers) {
      //if (dec_cont->buffer_index >= dec_cont->num_buffers)
      //  dec_cont->buffer_index = 0;
      if (dec_cont->add_buffer) {
        /* Need to allocate a new buffer during decoding... */
        Vp9BufferQueueAddBuffer(dec_cont->bq);
        dec_cont->add_buffer = 0;
      }
      dec_cont->next_buf_size = 0;
      // dec_cont->buf_num = 0;
      dec_cont->buf_to_free = NULL;
      dec_cont->buf_not_added = 0;

      if (dec_cont->add_buffer)
        Vp9BufferQueueAddBuffer(dec_cont->bq);

    } else {
      if (dec_cont->buffer_num_added >= dec_cont->num_buffers) {
        /* It's just reallocating old smaller picture. */
        dec_cont->buf_not_added = 0;
        dec_ret = DEC_OK;
      } else
        dec_ret = DEC_WAITING_FOR_BUFFER;
    }
    break;
  }
  case DOWNSCALE_OUT_BUFFER:
    //ASSERT(dec_cont->buffer_index < dec_cont->num_buffers);

    asic_buff->pp_pictures[dec_cont->buffer_index] = *info;
    if (!asic_buff->realloc_out_buffer) {
      dec_cont->buffer_index++;
      dec_cont->buffer_num_added++;
      Vp9BufferQueueAddBuffer(dec_cont->pp_bq);
      dec_cont->num_pp_buffers++;
    }

    if (dec_cont->dec_stat != VP9DEC_NEW_HEADERS ||
        dec_cont->buffer_index >= dec_cont->num_buffers) {
      dec_cont->buf_not_added = 0;
      dec_cont->next_buf_size = 0;
      dec_cont->buf_to_free = NULL;
    } else
      dec_ret = DEC_WAITING_FOR_BUFFER;
    break;
  default:
    break;
  }

  return dec_ret;
}


enum DecRet Vp9DecGetBufferInfo(Vp9DecInst dec_inst, struct DecBufferInfo *mem_info) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
//  enum DecRet dec_ret = DEC_OK;
  struct DWLLinearMem empty = {0};
  u32 i;

  if (dec_inst == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    u32 bit_depth = dec_cont->decoder.bit_depth;
    if (!dec_cont->use_video_compressor) {
      mem_info->cstride[0] = mem_info->ystride[0] =
                             NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    }
    else {
      mem_info->ystride[0] = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(dec_cont->width, 8) * bit_depth,
                                          ALIGN(dec_cont->align) * 8) / 8;
      mem_info->cstride[0] = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dec_cont->width, 8) * bit_depth,
                                          ALIGN(dec_cont->align) * 8) / 8;
    }
  } else {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!dec_cont->ppu_cfg[i].enabled) continue;
      mem_info->ystride[i] = dec_cont->ppu_cfg[i].ystride;
      mem_info->cstride[i] = dec_cont->ppu_cfg[i].cstride;
    }
  }

  if (dec_cont->buf_to_free == NULL && dec_cont->next_buf_size == 0)
    return DEC_OK;

  if (dec_cont->buf_to_free) {
    mem_info->buf_to_free = *dec_cont->buf_to_free;

    // TODO(min): here we assume that the buffer should be freed externally.
    dec_cont->buf_to_free->virtual_address = NULL;
    dec_cont->buf_to_free->bus_address = 0;
    dec_cont->buf_to_free = NULL;
    dec_cont->buf_not_added = 1;
  } else
    mem_info->buf_to_free = empty;

  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;

  APITRACEDEBUG("Vp9DecGetBufferInfo# requests %d external buffers with size of %d\n", mem_info->buf_num, mem_info->next_buf_size);

  return DEC_WAITING_FOR_BUFFER;
}

enum DecRet Vp9DecUseExtraFrmBuffers(Vp9DecInst dec_inst, u32 n) {

  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;

  dec_cont->n_extra_frm_buffers = n;

  return DEC_OK;
}


enum DecRet Vp9DecSetInfo(Vp9DecInst dec_inst, struct Vp9DecConfig *dec_cfg) {
  struct Vp9DecContainer *dec_cont;
  u32 pic_width;
  u32 pic_height;
  u32 pixel_width;
  PpUnitConfig *ppu_cfg;
  u32 i = 0;

  if (dec_inst == NULL || dec_cfg == NULL)
    return (DEC_PARAM_ERROR);

  dec_cont = (struct Vp9DecContainer *)dec_inst;
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont)
    return DEC_NOT_INITIALIZED;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_VP9_DEC);
  if (!dec_cont->hw_feature) {
    APITRACEDEBUG("%s","HevcDecSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
  }
  pic_width = (dec_cont->decoder.width + 1) & (~0x1);
  pic_height = (dec_cont->decoder.height + 1) & (~0x1);
  pixel_width = dec_cont->decoder.bit_depth;
  ppu_cfg = dec_cfg->ppu_cfg;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  /* ref aligment */
  dec_cont->align = dec_cfg->align;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    /* ppu alignment */
    dec_cfg->ppu_cfg[i].align = dec_cfg->align;
    /* ppu dwl instance */
    dec_cont->ppu_cfg[i].dwl = dec_cont->dwl;
    /* 3dlut */
    if (dec_cfg->ppu_cfg[i].enable_3dlut && (dec_cfg->ppu_cfg[i].rgb || dec_cfg->ppu_cfg[i].rgb_planar)) {
      dec_cont->enable_3dlut = 1;
    }
    dec_cont->ppu_cfg[i].table_3dlut_buffer = dec_cfg->table_3dlut_buffer;
    /* Range Mapping */
    dec_cont->ppu_cfg[i].source_range = dec_cont->decoder.yuv_range;
    // when input stream, rgb source range follows yuv source range, which get from stream sps info
  }

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, dec_cont->hw_feature, pixel_width, 1, 0);

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;
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
  if (CheckPpUnitConfig(dec_cont->hw_feature, pic_width, pic_height, 0, pixel_width,
                        PP_CHROMA_420, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  memcpy(dec_cont->delogo_params, dec_cfg->delogo_params, sizeof(dec_cont->delogo_params));
  if (CheckDelogo(dec_cont->delogo_params, dec_cont->decoder.bit_depth, dec_cont->decoder.bit_depth))
    return DEC_PARAM_ERROR;

  /* config error process policy. */
  if (dec_cont->error_handling == DEC_EC_FRAME_TOLERANT_ERROR) {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE | DEC_EC_NO_SKIP | DEC_EC_OUT_DECISION;
  } else if (dec_cont->error_handling == DEC_EC_FRAME_IGNORE_ERROR) {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE_ANYWAY | DEC_EC_NO_SKIP | DEC_EC_OUT_ALL;
  } else {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_RESET | DEC_EC_SEEK_NEXT_I | DEC_EC_OUT_NO_ERROR;
  }

  dec_cont->ext_buffer_config = 0;
  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config |= 1 << DOWNSCALE_OUT_BUFFER;
  else
    dec_cont->ext_buffer_config  = 1 << REFERENCE_BUFFER;

  if (dec_cont->pp_bq == NULL) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
      dec_cont->pp_bq = Vp9BufferQueueInitialize(0);
      if (dec_cont->pp_bq == NULL) {
        Vp9BufferQueueRelease(dec_cont->pp_bq, 1);
        return DEC_MEMFAIL;
      }
    }
  }

  Vp9SetExternalBufferInfo(dec_cont);
  return (DEC_OK);
}

void Vp9DecUpdateStrmInfoCtrl(Vp9DecInst dec_inst, struct strmInfo info) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  static u32 len_update = 1;
  struct LLStrmInfo *llstrminfo = &dec_cont->llstrminfo;
  llstrminfo->stream_info = info;

  if (llstrminfo->update_reg_flag) {
    /* wait for hw ready if it's the first time to update length register */
    if (llstrminfo->first_update) {
      while (!dec_cont->asic_running)
        sched_yield();
      llstrminfo->first_update = 0;
      llstrminfo->ll_strm_len = 0;
      len_update = 1;
    }

    SwUpdateStrmInfoCtrl(llstrminfo, &len_update);

    Vp9DecUpdateTileInfo(dec_inst);

    /* check hw status */
    if (!llstrminfo->first_update && !dec_cont->asic_running) {
      llstrminfo->tmp_length = llstrminfo->ll_strm_len;
      llstrminfo->update_reg_flag = 0;
      llstrminfo->updated_reg = 1;
    }
  }
}

void Vp9DecUpdateTileInfo(Vp9DecInst dec_inst) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  u32 tmp;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct LLTileInfo *lltileinfo = &dec_cont->lltileinfo;
  struct LLStrmInfo *llstrminfo = &dec_cont->llstrminfo;
  u32 start = 0, end = 0;

  if (lltileinfo->update_done)
    return;

  if (!lltileinfo->update_rdy)
    return;
  /* add 4 bytes: the next tile hrd */
  while ((llstrminfo->stream_info.send_len - dec->frame_tag_size - dec->offset_to_dct_parts) >= (lltileinfo->tile_size + 4)
    || llstrminfo->stream_info.last_flag) {
    if (lltileinfo->cur_rows < lltileinfo->tile_rows) {
      if (!lltileinfo->cur_cols) {
        tmp = (lltileinfo->cur_rows + 1) * dec_cont->lltileinfo.h_sbs / lltileinfo->tile_rows;
        lltileinfo->tmp_h = tmp - lltileinfo->prev_h;
        lltileinfo->prev_h = tmp;
        lltileinfo->prev_w = 0;
      }

      if (lltileinfo->h_sbs >=3 && !lltileinfo->cur_rows && !lltileinfo->tmp_h)
        dec_cont->first_tile_empty = 1;

      if (lltileinfo->cur_cols < lltileinfo->tile_cols) {
        tmp = (lltileinfo->cur_cols + 1) * lltileinfo->w_sbs / lltileinfo->tile_cols;
        dec->tile_col_width_sb[lltileinfo->tile_id] = tmp - lltileinfo->prev_w;
        dec->tile_row_height_sb[lltileinfo->tile_id] = lltileinfo->tmp_h;

        if ((lltileinfo->cur_rows == (lltileinfo->tile_rows - 1)) &&
            (lltileinfo->cur_cols == (lltileinfo->tile_cols - 1))) {
          dec->tile_offset_start[lltileinfo->tile_id] = lltileinfo->tile_size;
          dec->tile_offset_end[lltileinfo->tile_id] = lltileinfo->frame_len - dec->frame_tag_size - dec->offset_to_dct_parts;
        } else {
          dec->tile_offset_start[lltileinfo->tile_id] = lltileinfo->tile_size + 4;
         /* strm_next_address is the tile hrd address, buf_address and buf_len is the address and len for input buf */
          if (lltileinfo->strm_next_address >= (lltileinfo->buf_address + lltileinfo->buf_len))
             lltileinfo->strm_next_address -= lltileinfo->buf_len;
          lltileinfo->tile_size += *lltileinfo->strm_next_address << 24;
          lltileinfo->strm_next_address++;
          if (lltileinfo->strm_next_address >= (lltileinfo->buf_address + lltileinfo->buf_len))
            lltileinfo->strm_next_address -= lltileinfo->buf_len;
          lltileinfo->tile_size += *lltileinfo->strm_next_address << 16;
          lltileinfo->strm_next_address++;
          if (lltileinfo->strm_next_address >= (lltileinfo->buf_address + lltileinfo->buf_len))
            lltileinfo->strm_next_address -= lltileinfo->buf_len;
          lltileinfo->tile_size += *lltileinfo->strm_next_address << 8;
          lltileinfo->strm_next_address++;
          if (lltileinfo->strm_next_address >= (lltileinfo->buf_address + lltileinfo->buf_len))
            lltileinfo->strm_next_address -= lltileinfo->buf_len;
          lltileinfo->tile_size += *lltileinfo->strm_next_address;
          lltileinfo->strm_next_address++;
          dec->tile_offset_end[lltileinfo->tile_id] = lltileinfo->tile_size + 4;
          lltileinfo->tile_size += 4;
          lltileinfo->strm_next_address += dec->tile_offset_end[lltileinfo->tile_id] - dec->tile_offset_start[lltileinfo->tile_id];
        }

        /* update tile info in tile info buffer */
        *lltileinfo->tileinfo++ = dec->tile_col_width_sb[lltileinfo->tile_id];
        *lltileinfo->tileinfo++ = 0;
        *lltileinfo->tileinfo++ = 0;
        *lltileinfo->tileinfo++ = 0;
        *lltileinfo->tileinfo++ = dec->tile_row_height_sb[lltileinfo->tile_id];
        *lltileinfo->tileinfo++ = 0;
        *lltileinfo->tileinfo++ = 0;
        *lltileinfo->tileinfo++ = 0;
        if ((lltileinfo->cur_rows == (lltileinfo->tile_rows - 1)) &&
            (lltileinfo->cur_cols == (lltileinfo->tile_cols - 1))) {
          start = dec->tile_offset_start[lltileinfo->tile_id];
        } else {
          start = dec->tile_offset_start[lltileinfo->tile_id] - 4;
        }
        *lltileinfo->tileinfo++ = start & 255;
        *lltileinfo->tileinfo++ = (start >> 8) & 255;
        *lltileinfo->tileinfo++ = (start >> 16) & 255;
        *lltileinfo->tileinfo++ = (start >> 24) & 255;
        end = dec->tile_offset_end[lltileinfo->tile_id];
        *lltileinfo->tileinfo++ = end & 255;
        *lltileinfo->tileinfo++ = (end >> 8) & 255;
        *lltileinfo->tileinfo++ = (end >> 16) & 255;
        *lltileinfo->tileinfo++ = (end >> 24) & 255;

        lltileinfo->tile_id++;
        if (lltileinfo->strm_next_address > lltileinfo->buf_address) {
          if ((lltileinfo->strm_next_address - lltileinfo->buf_address) > lltileinfo->frame_len) {
            lltileinfo->update_done = 1;
            break;
          }
        } else {
          if ((lltileinfo->strm_next_address + lltileinfo->buf_len - lltileinfo->buf_address) > lltileinfo->frame_len) {
            lltileinfo->update_done = 1;
            break;
          }
        }
        lltileinfo->cur_cols++;
        if (lltileinfo->cur_cols == lltileinfo->tile_cols) {
          lltileinfo->cur_rows++;
          lltileinfo->cur_cols = 0;
        }
        lltileinfo->prev_w = tmp;
      }
    } else {
      lltileinfo->update_done = 1;
      break;
    }
    /* no more new data for current frame */
    if (llstrminfo->stream_info.last_flag)
      lltileinfo->update_done = 1;
  }

  SwUpdateTileInfoCtrl(lltileinfo);
}

u32 Vp9ReplaceRefAvalible(struct Vp9DecContainer *dec_cont) {
  u32 i = 0;
  i32 old_index = -1;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 index_ref[VP9_ACTIVE_REFS];
  u32 index_info[VP9_ACTIVE_REFS] = {0};
  u32 tmp1_index = 0;
  u32 tmp2_index = 0;
  u32 tmp1_pic_size = 0;
  u32 tmp2_pic_size = 0;
  u32 curr_pic_size = 0;

  /* get ref info. */
  for (i = 0; i < VP9_ACTIVE_REFS; i++) {
    index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
  }
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    for (i = 0; i < VP9_ACTIVE_REFS; i++) {
      index_info[i] = index_ref[i];
    }
  } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    for (i = 0; i < VP9_ACTIVE_REFS; i++)
      index_info[i] = Vp9BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
  }

  for (i = 0; i < VP9_ACTIVE_REFS; i++) {
    if (asic_buff->picture_info[index_info[i]].error_info != DEC_NO_ERROR &&
        asic_buff->picture_info[index_info[i]].error_info != DEC_REF_ERROR) {
      /* this ref need replace. */

      /* same ref index need skip. */
      if (old_index == index_info[i])
        continue;

      old_index = index_info[i];

      /* NO_RFC: if error_ratio less than 10%, don't replace it, use it as correct. */
      if (!dec_cont->use_video_compressor &&
          asic_buff->picture_info[index_info[i]].error_ratio <= EC_RATIO_THRESHOLD)
        continue;

      curr_pic_size = asic_buff->picture_info[index_info[i]].coded_width *
                      asic_buff->picture_info[index_info[i]].coded_height;
      /* check 1th ref pic. */
      tmp1_index = (i + 1) % 3;
      tmp1_pic_size = asic_buff->picture_info[index_info[tmp1_index]].coded_width *
                      asic_buff->picture_info[index_info[tmp1_index]].coded_height;
      if (tmp1_pic_size == curr_pic_size) {
        if (dec_cont->use_video_compressor) {
          if (asic_buff->picture_info[index_info[tmp1_index]].error_info == DEC_NO_ERROR ||
              asic_buff->picture_info[index_info[tmp1_index]].error_info == DEC_REF_ERROR)
            continue;
        } else {
          /* NO_RFC: if error_ratio less than 10%, use it as correct. */
          if (asic_buff->picture_info[index_info[tmp1_index]].error_ratio <= EC_RATIO_THRESHOLD)
            continue;
        }
      }
      /* check 2th ref pic. */
      tmp2_index = (i + 2) % 3;
      tmp2_pic_size = asic_buff->picture_info[index_info[tmp2_index]].coded_width *
                      asic_buff->picture_info[index_info[tmp2_index]].coded_height;
      if (tmp2_pic_size == curr_pic_size) {
        if (dec_cont->use_video_compressor) {
          if (asic_buff->picture_info[index_info[tmp2_index]].error_info == DEC_NO_ERROR ||
              asic_buff->picture_info[index_info[tmp2_index]].error_info == DEC_REF_ERROR)
            continue;
        } else {
          /* NO_RFC: if error_ratio less than 10%, use it as correct. */
          if (asic_buff->picture_info[index_info[tmp2_index]].error_ratio <= EC_RATIO_THRESHOLD)
            continue;
        }
      }

      /* current error ref can't find replaced ref. */
      return HANTRO_NOK;
    }
  }
  /* all error ref can find replaced ref. */
  return HANTRO_OK;
}

#ifdef CASE_INFO_STAT
void Vp9CaseInfoCollect(struct Vp9DecContainer *dec_cont, CaseInfo *case_info) {
  u32 raster_stride;

  case_info->min_cb_size = 3;
  case_info->max_cb_size = 6;
  case_info->pic_width_in_cbs = (dec_cont->width + 7) / 8;
  case_info->pic_height_in_cbs = (dec_cont->height + 7) / 8;
  if(dec_cont->decoder.bit_depth == 8)
    raster_stride = NEXT_MULTIPLE(dec_cont->decoder.width, 16);
  else
    raster_stride = NEXT_MULTIPLE(dec_cont->decoder.width * 2, 16);

  if (case_info->frame_num == 0) {
    case_info->decode_width = NEXT_MULTIPLE(dec_cont->decoder.width, 8);
    case_info->decode_height = NEXT_MULTIPLE(dec_cont->decoder.height, 8);
    case_info->display_height = dec_cont->decoder.height;
    case_info->display_width = dec_cont->decoder.width;
    case_info->bit_depth = dec_cont->decoder.bit_depth;
    case_info->crop_x = case_info->crop_y = 0;
  } else {
    if (case_info->decode_width != NEXT_MULTIPLE(dec_cont->decoder.width, 8)
    || case_info->decode_height != NEXT_MULTIPLE(dec_cont->decoder.height, 8)) {
      case_info->decode_width = MAX (NEXT_MULTIPLE(dec_cont->decoder.width, 8), case_info->decode_width);
      case_info->decode_height = MAX (NEXT_MULTIPLE(dec_cont->decoder.height, 8), case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (dec_cont->decoder.height, case_info->display_height);
    case_info->display_width = MIN (dec_cont->decoder.width, case_info->display_width);
    if(case_info->bit_depth != dec_cont->decoder.bit_depth) {
      case_info->bit_depth = MAX (dec_cont->decoder.bit_depth, case_info->bit_depth);
      case_info->depth_flag = 1;
    }
  }

  if (case_info->frame_num < 10){
    if (dec_cont->decoder.key_frame)
      case_info->frame_type[case_info->frame_num] = I_FRAME;
    else
      case_info->frame_type[case_info->frame_num] = P_FRAME;
    case_info->num_tile_cols_8k[dec_cont->pic_number] = 1 << dec_cont->decoder.log2_tile_columns;
    case_info->bit_depth_y_minus8[dec_cont->pic_number] = dec_cont->decoder.bit_depth - 8;
    case_info->bit_depth_c_minus8[dec_cont->pic_number] = dec_cont->decoder.bit_depth - 8;
    case_info->ppin_luma_size[dec_cont->pic_number] = dec_cont->decoder.height * raster_stride;
    case_info->slice_num[case_info->frame_num]++;
  }
  case_info->codec = DEC_MODE_VP9;
  case_info->chroma_format_id = 1;
  case_info->frame_num ++;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_LEN) * 8/ NEXT_MULTIPLE(dec_cont->height, 16))
                        * 3840/ NEXT_MULTIPLE(dec_cont->width, 16)) * 2160/ 1024/ 1024;
  case_info->tile_flag = GetDecRegister(dec_cont->vp9_regs[0], HWIF_TILE_ENABLE);
}
#endif
