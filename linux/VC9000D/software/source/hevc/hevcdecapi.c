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
#include "version.h"
#include "hevc_container.h"
#include "hevcdecapi.h"
#include "hevc_decoder.h"
#include "hevc_util.h"
#include "hevc_dpb.h"
#include "hevc_asic.h"
#include "regdrv.h"
#include "hevc_byte_stream.h"
#include "deccfg.h"
#include "commonconfig.h"
#include "commonfunction.h"
#include "dwl.h"
#include "version.h"
#include "decapicommon.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include "hevcdecmc_internals.h"
#include "dec_log.h"
#include "errorhandling.h"
#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#define IS_REFERENCE(a) IsReference(&(a))
static u32 IsReference(const struct DpbPicture *a) {
  return (a->status && a->status != EMPTY);
}

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void HevcCaseInfoCollect(struct HevcDecContainer *dec_cont, CaseInfo *case_info);
#endif
static void HevcUpdateAfterPictureDecode(struct HevcDecContainer *dec_cont);
static u32 HevcSpsSupported(const struct HevcDecContainer *dec_cont);
static u32 HevcPpsSupported(const struct HevcDecContainer *dec_cont);

static u32 HevcAllocateResources(struct HevcDecContainer *dec_cont);
static void HevcGetSarInfo(const struct Storage *storage, u32 *sar_width,
                           u32 *sar_height);
extern void HevcPreparePpRun(struct HevcDecContainer *dec_cont);
static enum DecRet HevcDecNextPictureInternal(struct HevcDecContainer *dec_cont);
static enum DecRet HevcECDecisionOutput(struct HevcDecContainer *dec_cont, struct HevcDecPicture *output);
static void HevcEnterAbortState(struct HevcDecContainer *dec_cont);
static void HevcExistAbortState(struct HevcDecContainer *dec_cont);
static void HevcCycleCount(struct HevcDecContainer *dec_cont);
static void HevcSCMarkOutputPicInfo(struct HevcDecContainer *dec_cont, u32 picture_broken);
static void HevcDropCurrentPicutre(struct HevcDecContainer *dec_cont);
static void HevcMCMarkOutputPicInfo(struct HevcDecContainer *dec_cont,
                                    const struct HevcHwRdyCallbackArg *info,
                                    enum DecErrorInfo error_info,
                                    const u32* dec_regs);
static u32 HevcReplaceRefAvalible(struct HevcDecContainer *dec_cont);

#ifdef RANDOM_CORRUPT_RFC
u32 HevcCorruptRFC(struct HevcDecContainer *dec_cont);
#endif

#define DEC_DPB_NOT_INITIALIZED -1
#define DEC_MODE_HEVC 12
#define CHECK_TAIL_BYTES 16

/* Initializes decoder software. Function reserves memory for the
 * decoder instance and calls HevcInit to initialize the
 * instance data. */
enum DecRet HevcDecInit(HevcDecInst *dec_inst, const void *dwl, struct HevcDecConfig *dec_cfg) {
  struct HevcDecContainer *dec_cont;
  u32 core_mask = 0, core_mask_rfc;
  u32 low_latency_sim = 0, i;
  enum DecRet ret;

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if (dec_inst == NULL || dwl == NULL || dec_cfg == NULL)
    return (DEC_PARAM_ERROR);

  *dec_inst = NULL; /* return NULL instance for any error */
  /* check that hevc decoding supported in HW */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_HEVC_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","HevcDecInit# Hevc not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  if ((dec_cfg->decoder_mode & DEC_LOW_LATENCY) &&
      !SwGetCoreMaskByFeature(dwl, VSI_LOW_LATENCY)) {
    APITRACEERR("%s","HevcDecInit# Hevc low latency not supported in HW\n");
    return DEC_PARAM_ERROR;
  }

#ifdef ASIC_TRACE_SUPPORT
  if (dec_cfg->mcinit_cfg.mc_enable) {
    dec_cfg->mcinit_cfg.mc_enable = 0;
    dec_cfg->sim_mc = 1
    DWLSetSimMc(dwl);
  }
#endif

  dec_cont = (struct HevcDecContainer *)DWLmalloc(sizeof(struct HevcDecContainer));
  if (dec_cont == NULL) {
    APITRACEERR("%s","HevcDecInit# Memory allocation failed\n");
    return DEC_MEMFAIL;
  }
  DWLmemset(dec_cont, 0, sizeof(struct HevcDecContainer));

  dec_cont->heif_mode = (dec_cfg->decoder_mode & DEC_HEIF) ? 1 : 0;
  if (dec_cfg->decoder_mode & DEC_PARTIAL_DECODING)
    dec_cont->partial_decoding = 1;

  if (dec_cfg->decoder_mode & DEC_INTRA_ONLY)
    dec_cont->skip_non_intra = dec_cont->storage.skip_non_intra = 1;

  dec_cont->dwl = dwl;
  dec_cont->n_cores = DWLReadAsicCoreCount(dwl);
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_HEVC_DEC);
  SET_SECURE_MODE(dec_cont->core_mask , dec_cont->secure_mode);
  dec_cont->n_cores_available = SwGetCores(core_mask);
  if (dec_cfg->mcinit_cfg.mc_enable && dec_cont->n_cores_available > 1) {
    /* enable synchronization writing in multi-core HW */
    dec_cont->b_mc = 1;
    dec_cont->stream_consumed_callback.fn = dec_cfg->mcinit_cfg.stream_consumed_callback;
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_MULTICORE_E, 1);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_WRITESTAT_E, 1);
  }

  dec_cont->hevc_main10_support = SwGetCoreMaskByFeature(dwl, VSI_HEVC_MAIN10) ? 1 : 0;
  dec_cont->hevc_422_intra_support =  SwGetCoreMaskByFeature(dwl, VSI_HEVC_422_INTRA) ? 1 : 0;
  if ((dec_cfg->dump_auxinfo & DEC_AUX_QP) && SwGetCoreMaskByFeature(dwl, VSI_QP_DUMP))
    dec_cont->qp_output_enable = dec_cont->storage.qp_output_enable = 1;
  if ((dec_cfg->dump_auxinfo & DEC_AUX_MV))
    dec_cont->dmv_output_enable = dec_cont->storage.dmv_output_enable = 1;

  core_mask_rfc = SwGetCoreMaskByFeature(dec_cont->dwl, VSI_RFC);
  if (dec_cfg->use_video_compressor && core_mask_rfc)
    dec_cont->use_video_compressor = dec_cont->storage.use_video_compressor = 1;
  /* check whether core mask support rfc */
  if (dec_cont->use_video_compressor) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    if (dec_cont->core_mask & core_mask_rfc) {
      dec_cont->core_mask &= CORE_MASK(core_mask_rfc);
      dec_cont->core_mask |= bak_non_core_mask;
    }
    else {
      dec_cont->use_video_compressor = dec_cont->storage.use_video_compressor = 0;
      APITRACEDEBUG("HevcDecInit# not any core to support RFC from core_mask 0x%x, disable it\n",
                     dec_cont->core_mask);
    }
  }

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);
  if (dec_cont->vcmd_used && dec_cont->b_mc) {
    FifoInit(dec_cont->n_cores_available, &dec_cont->fifo_core);
    for (i = 0; i < dec_cont->n_cores_available; i++) {
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)i, FIFO_EXCEPTION_DISABLE);
    }
  }

  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL) {
    ret = DEC_MEMFAIL;
    goto err;
  }

  dec_cont->dec_state = HEVCDEC_INITIALIZED;
  dec_cont->hevc_regs[0] = DWLReadAsicID(dwl, DWL_CLIENT_TYPE_HEVC_DEC);
  SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_MODE, DEC_MODE_HEVC);
  SetCommonConfigRegs(dec_cont->hevc_regs);

  dec_cont->error_info = DEC_NO_ERROR;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  dec_cont->error_handling = dec_cfg->error_handling;
  dec_cont->error_policy = HANTRO_FALSE;
  dec_cont->num_buffers = dec_cfg->num_buffers;
  dec_cont->use_ext_dpb_size = dec_cont->num_buffers ? 1 : 0;
  dec_cont->multi_frame_input_flag = dec_cfg->multi_frame_input_flag;
  dec_cont->guard_size = dec_cfg->guard_size;
  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->checksum = dec_cont; /* save instance as a checksum */

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  pthread_mutex_init(&dec_cont->dmv_buffer_mutex, NULL);
  /* CV to be signaled when a dmv buffer is consumed by Application */
  pthread_cond_init(&dec_cont->dmv_buffer_cv, NULL);

  HevcInit(&dec_cont->storage, dec_cfg->no_output_reordering);
  dec_cont->storage.picture_broken = HANTRO_FALSE;
  /* Init frame buffer list */
  InitList(&dec_cont->fb_list);
  dec_cont->storage.dpb[0].fb_list = &dec_cont->fb_list;
  dec_cont->storage.dpb[1].fb_list = &dec_cont->fb_list;
  dec_cont->storage.dpb[0].dmv_buffer_mutex = &dec_cont->dmv_buffer_mutex;
  dec_cont->storage.dpb[1].dmv_buffer_mutex = &dec_cont->dmv_buffer_mutex;
  dec_cont->storage.dpb[0].dmv_buffer_cv = &dec_cont->dmv_buffer_cv;
  dec_cont->storage.dpb[1].dmv_buffer_cv = &dec_cont->dmv_buffer_cv;
  dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue;

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif
  if (dec_cfg->decoder_mode & DEC_LOW_LATENCY) {
#ifdef ASIC_TRACE_SUPPORT
    low_latency_sim = 1;
#else
    dec_cont->low_latency = 1;
    dec_cont->llstrminfo.updated_reg = 0;
#endif
#ifdef SUPPORT_DMA
    dec_cont->llstrminfo.dwl_inst = dwl;
#endif
    /* Set flag for using ddr_low_latency model and set pollmode and polltime swregs */
    SetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_STATUS_EXT_BUFFER_E, 1);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_MC_POLLMODE, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_MC_POLLTIME, 0);
    dec_cont->llstrminfo.strm_status_in_buffer = 1;
  }

  if(dec_cont->low_latency || low_latency_sim) {
    SetDecRegister(dec_cont->hevc_regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_BLOCK_BUFFER_MODE_E, 1);
    dec_cont->disable_slice_mode = 1;
  } else if (dec_cont->secure_mode) {

  } else {
    SetDecRegister(dec_cont->hevc_regs, HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->hevc_regs, HWIF_BLOCK_BUFFER_MODE_E, 0);
  }

#ifdef RANDOM_CORRUPT_RFC
  InitializeRandom(&dec_cont->error_params, "1 : 6", "1 : 100000", "1 : 6", 66);
#endif

  *dec_inst = (HevcDecInst)dec_cont;

  return (DEC_OK);

err:
  DWLfree(dec_cont);
  return ret;
}

/* This function provides read access to decoder information. This
 * function should not be called before HevcDecDecode function has
 * indicated that headers are ready. */
enum DecRet HevcDecGetInfo(HevcDecInst dec_inst, struct HevcDecInfo *dec_info) {
  u32 cropping_flag;
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  struct Storage *storage;

  if (dec_inst == NULL || dec_info == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  storage = &dec_cont->storage;

  if (storage->active_sps == NULL || storage->active_pps == NULL) {
    return (DEC_HDRS_NOT_RDY);
  }

  dec_info->interlaced_sequence = 0;
  dec_info->pic_width = HevcPicWidth(storage);
  dec_info->pic_height = HevcPicHeight(storage);
  dec_info->video_range = HevcVideoRange(storage);
  dec_info->matrix_coefficients = HevcMatrixCoefficients(storage);
  dec_info->colour_primaries = HevcColourPrimaries(storage);
  dec_info->timing_info_present_flag = HevcTimingInfoPresent(storage);
  dec_info->num_units_in_tick = HevcNumUnitsInTick(storage);
  dec_info->time_scale = HevcTimeScale(storage);
  dec_info->mono_chrome = HevcIsMonoChrome(storage);
  dec_info->chroma_format_idc = storage->active_sps->chroma_format_idc;
  if (!dec_cont->heif_mode) {
    if (dec_cont->pp_enabled)
      dec_info->pic_buff_size =
      (dec_cont->skip_non_intra ? 0 : storage->active_sps->max_dpb_size) + 1 + 1;
    else
      dec_info->pic_buff_size =
      (dec_cont->skip_non_intra ? 0 : storage->active_sps->max_dpb_size) + 1 + 2;
  } else {
    dec_info->pic_buff_size =
    dec_cont->skip_non_intra ? 1 : (storage->active_sps->max_dpb_size + 1 + 1);
  }
  dec_info->multi_buff_pp_size =
    storage->dpb->no_reordering ? 2 : dec_info->pic_buff_size;
  dec_info->dpb_mode = dec_cont->dpb_mode;

  HevcGetSarInfo(storage, &dec_info->sar_width, &dec_info->sar_height);

  HevcCroppingParams(storage, &cropping_flag,
                     &dec_info->crop_params.crop_left_offset,
                     &dec_info->crop_params.crop_out_width,
                     &dec_info->crop_params.crop_top_offset,
                     &dec_info->crop_params.crop_out_height);

  if (cropping_flag == 0) {
    dec_info->crop_params.crop_left_offset = 0;
    dec_info->crop_params.crop_top_offset = 0;
    dec_info->crop_params.crop_out_width = dec_info->pic_width;
    dec_info->crop_params.crop_out_height = dec_info->pic_height;
  }

  if (dec_cont->pp_enabled && dec_info->mono_chrome)
    dec_info->output_format = DEC_OUT_FRM_MONOCHROME;
  else
    dec_info->output_format = DEC_OUT_FRM_TILED_4X4;

  dec_info->bit_depth = ((HevcLumaBitDepth(storage) != 8) || (HevcChromaBitDepth(storage) != 8)) ? 10 : 8;


  if (dec_cont->pp_enabled)
    dec_info->pic_stride = NEXT_MULTIPLE(dec_info->pic_width * dec_info->bit_depth, 128) / 8;
  else
    /* Reference buffer. */
    dec_info->pic_stride = dec_info->pic_width * dec_info->bit_depth / 8;

  /* for HDR */
  u32 vui_trc, sei_trc;
  u32 tmp = HevcTransferCharacteristics(storage, &vui_trc, &sei_trc);
  if (tmp == 0) {
    dec_info->transfer_characteristics = vui_trc;
    dec_info->preferred_transfer_characteristics = sei_trc;
  }
  else if(tmp == 1)
    dec_info->transfer_characteristics = vui_trc;
  else if(tmp == 2)
    dec_info->preferred_transfer_characteristics = sei_trc;
  dec_info->trc_status = tmp;

  return (DEC_OK);
}

/* Releases the decoder instance. Function calls HevcShutDown to
 * release instance data and frees the memory allocated for the
 * instance. */
void HevcDecRelease(HevcDecInst dec_inst) {

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  const void *dwl;
  u32 i;

  if (dec_cont == NULL) {
    return;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return;
  }

  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (dec_cont->hw_rdy_callback_arg[i]) {
      DWLfree(dec_cont->hw_rdy_callback_arg[i]);
      dec_cont->hw_rdy_callback_arg[i] = NULL;
    }
  }
  pthread_mutex_destroy(&dec_cont->protect_mutex);
  pthread_mutex_destroy(&dec_cont->dmv_buffer_mutex);
  /* CV to be signaled when a dmv buffer is consumed by Application */
  pthread_cond_destroy(&dec_cont->dmv_buffer_cv);
  dwl = dec_cont->dwl;

  /* make sure all in sync in multicore mode, hw idle, output empty */
  if(dec_cont->b_mc) {
    HevcMCWaitPicReadyAll(dec_cont);
  } else {
    u32 i;
    const struct DpbStorage *dpb = dec_cont->storage.dpb;

    /* Empty the output list. This is just so that fb_list does not
     * complaint about still referenced pictures
     */
    for(i = 0; i < dpb->tot_buffers; i++) {
      if(dpb->pic_buff_id[i] != FB_NOT_VALID_ID &&
        IsBufferOutput(&dec_cont->fb_list, dpb->pic_buff_id[i])) {
        ClearOutput(&dec_cont->fb_list, dpb->pic_buff_id[i]);
      }
    }
  }

  if (dec_cont->asic_running) {
    /* stop HW */
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);
    SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 0);
    if(dec_cont->vcmd_used) {
      DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    }
    else {
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->hevc_regs[1]);
      DWLReleaseHw(dwl, dec_cont->core_id); /* release HW lock */
    }
    dec_cont->asic_running = 0;

    /* Decrement usage for DPB buffers */
    DecrementDPBRefCount(dec_cont->storage.dpb);
  }

#ifdef FPGA_PERF_AND_BW
  AveragePerfInfoPrint(&dec_cont->perf_info);
#endif

#ifndef USE_OMXIL_BUFFER
#ifndef EXT_BUF_SAFE_RELEASE
  if (!dec_cont->pp_enabled) {
    int i;
    pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
      dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
    }
    pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
  }
#endif

  /* block until all output is handled */
  WaitListNotInUse(&dec_cont->fb_list);
  if (dec_cont->storage.pp_enabled)
    InputQueueWaitNotUsed(dec_cont->storage.pp_buffer_queue);
#endif

  HevcShutdown(&dec_cont->storage);

  HevcFreeDpb(dec_cont, dec_cont->storage.dpb);
  HevcFreeDpbParastic(dec_cont, dec_cont->storage.dpb);

  if (dec_cont->storage.pp_enabled)
    InputQueueRelease(dec_cont->storage.pp_buffer_queue);

  ReleaseAsicBuffers(dec_cont, dec_cont->asic_buff);
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  for (i = 0; i <  dec_cont->n_cores * 2; i++)
    ReleaseAsicTileEdgeMems(dec_cont, i);

  if (dec_cont->vcmd_used && dec_cont->b_mc) {
    if (dec_cont->fifo_core)
      FifoRelease(dec_cont->fifo_core);
  }

  ReleaseList(&dec_cont->fb_list, dec_cont->heif_mode);


  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  return;
}


/* Decode stream data. Calls HevcDecode to do the actual decoding. */
enum DecRet HevcDecDecode(HevcDecInst dec_inst,
                          const struct HevcDecInput *input,
                          struct DecOutput *output) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 strm_len;
  u32 input_data_len; // used to generate error stream
  u32 total_read_bytes = 0; // used in low latency mode only
  const u8 *tmp_stream;
  enum DecRet return_value = DEC_STRM_PROCESSED;
  const struct DecHwFeatures *hw_feature = NULL;

  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_HEVC_DEC);
  /* Check that function input parameters are valid */
  if (input == NULL || output == NULL || dec_inst == NULL) {
    APITRACEERR("%s","HevcDecDecode# NULL arg(s)\n");
    return (DEC_PARAM_ERROR);
  }

  input_data_len = input->data_len;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    APITRACEERR("%s","HevcDecDecode# Decoder not initialized\n");
    return (DEC_NOT_INITIALIZED);
  }

  if(dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if (input->data_len == 0 || input->data_len > DEC_X170_MAX_STREAM_G2 ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->buffer) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(input->buffer_bus_address)) {
    APITRACEERR("%s","HevcDecDecode# Invalid arg value\n");
    return DEC_PARAM_ERROR;
  }

  dec_cont->stream_pos_updated = 0;
  output->strm_curr_pos = NULL;
  dec_cont->hw_stream_start_bus = input->stream_bus_address;
  dec_cont->hw_buffer_start_bus = input->buffer_bus_address;
  dec_cont->hw_stream_start = input->stream;
  dec_cont->hw_buffer = input->buffer;
  strm_len = dec_cont->hw_length = input_data_len;
  if(dec_cont->low_latency && !dec_cont->llstrminfo.stream_info.last_flag) {
    strm_len = dec_cont->hw_length = input_data_len = input->buff_len;
  } else if (dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag) {
    strm_len = dec_cont->hw_length = input_data_len = dec_cont->llstrminfo.stream_info.send_len;
  }
  /* For low latency mode, strmLen is set as a large value */
  dec_cont->hw_buffer_length = input->buff_len;
  tmp_stream = input->stream;
  dec_cont->stream_consumed_callback.p_strm_buff = input->stream;
  dec_cont->stream_consumed_callback.p_user_data = input->p_user_data;
  dec_cont->use_ringbuffer = ((input->stream + input_data_len) > (input->buffer + input->buff_len)) ? 1 : 0;
  dec_cont->skip_frame = input->skip_frame;

  /* If there are more buffers to be allocated or to be freed, waiting for buffers ready. */
  if (dec_cont->release_buffer ||
      (dec_cont->next_buf_size != 0 &&
       dec_cont->ext_buffer_num < dec_cont->min_buffer_num &&
       dec_cont->storage.active_sps != NULL &&
       dec_cont->storage.active_pps != NULL)) {
    return_value = DEC_WAITING_FOR_BUFFER;
    goto end;
  }

  do {
    enum HevcResult dec_result;
    u32 num_read_bytes = 0;
    struct Storage *storage = &dec_cont->storage;
    storage->sei_buffer = input->sei_buffer; /**< SEI buffer */

    if (dec_cont->dec_state == HEVCDEC_NEW_HEADERS) {
      dec_result = HEVC_HDRS_RDY;
      dec_cont->dec_state = HEVCDEC_INITIALIZED;
    } else if (dec_cont->dec_state == HEVCDEC_BUFFER_EMPTY) {
      APITRACEDEBUG("%s","HevcDecDecode# Skip HevcDecode\n");
      APITRACEDEBUG("%s","HevcDecDecode# Jump to HEVC_PIC_RDY\n");
      /* update stream pointers */
      struct StrmData *strm = dec_cont->storage.strm;
      strm->strm_buff_start = input->buffer;
      strm->strm_buff_size = input->buff_len;
      strm->strm_curr_pos = tmp_stream;
      strm->strm_data_size = strm_len;
      strm->stream_info = &dec_cont->llstrminfo.stream_info;

      dec_result = HEVC_PIC_RDY;
    }
    else if (dec_cont->dec_state == HEVCDEC_WAITING_FOR_BUFFER) {
      APITRACEDEBUG("%s","HevcDecDecode# Skip HevcDecode\n");
      APITRACEDEBUG("%s","HevcDecDecode# Jump to HEVC_PIC_RDY\n");

      dec_result = HEVC_BUFFER_NOT_READY;

    } else {
      dec_result = HevcDecode(dec_cont, tmp_stream, strm_len, input, &num_read_bytes);
      if (dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag) {
        input_data_len = dec_cont->llstrminfo.stream_info.send_len;
        strm_len = dec_cont->llstrminfo.stream_info.send_len;
        dec_cont->hw_length = dec_cont->llstrminfo.stream_info.send_len;
      }
      if (dec_cont->low_latency) {
        strm_len = input_data_len - total_read_bytes;
      }
      if (num_read_bytes > strm_len)
        num_read_bytes = strm_len;

      ASSERT(num_read_bytes <= strm_len);
    }

    strm_len -= num_read_bytes;
    tmp_stream += num_read_bytes;
    total_read_bytes += num_read_bytes;

    if (tmp_stream >= dec_cont->hw_buffer + dec_cont->hw_buffer_length && dec_cont->use_ringbuffer)
      tmp_stream -= dec_cont->hw_buffer_length;

    if (dec_cont->low_latency && !strm_len && tmp_stream == input->stream)
      tmp_stream = input->stream + input_data_len;

    switch (dec_result) {
    /* Split the old case HEVC_HDRS_RDY into case HEVC_HDRS_RDY & HEVC_BUFFER_NOT_READY. */
    /* In case HEVC_BUFFER_NOT_READY, we will allocate resources. */
    case HEVC_HDRS_RDY: {
      /* If both the the size and number of buffers allocated are enough,
       * decoding will continue as normal.
       */
      dec_cont->reset_dpb_done = 0;

#if 0
      /* hevc 422 intra only mode enable skip_non_intra feature */
      if (dec_cont->storage.active_sps &&
          dec_cont->storage.active_sps->chroma_format_idc == 2 &&
          dec_cont->hevc_422_intra_support) {
        dec_cont->skip_non_intra = dec_cont->storage.skip_non_intra = 1;
      }
#endif
      /* intra only mode disable RFC and Reorder */
      if (dec_cont->skip_non_intra) {
        dec_cont->use_video_compressor = 0;
        dec_cont->storage.use_video_compressor = 0;
        HevcDecSetNoReorder(dec_cont, 1);
      }

      if (storage->dpb->flushed && storage->dpb->num_out) {
        /* output first all DPB stored pictures */
        storage->dpb->flushed = 0;
        dec_cont->dec_state = HEVCDEC_NEW_HEADERS;
        return_value = DEC_PENDING_FLUSH;
        strm_len = 0;
        break;
      }

#ifdef USE_OMXIL_BUFFER
      if (!dec_cont->pp_enabled) {
        MarkListNotInUse(&dec_cont->fb_list);
      }
#endif

      /* Make sure that all frame buffers are not in use before
       * reseting DPB (i.e. all HW cores are idle and all output
       * processed) */
      WaitListNotInUse(&dec_cont->fb_list);

#ifndef USE_OMXIL_BUFFER
      WaitOutputEmpty(&dec_cont->fb_list);
      if (dec_cont->storage.pp_enabled) {
        u32 ret = InputQueueWaitNotUsed(dec_cont->storage.pp_buffer_queue);
        if (ret) {
          strm_len = 0;
          dec_cont->dec_state = HEVCDEC_NEW_HEADERS;
          return_value = DEC_PENDING_FLUSH;
          break;
        }
      }
#endif
      PushOutputPic(&dec_cont->fb_list, NULL, -2);

      if (storage->active_sps) {
        /* check for minimum and maximum dimensions */
        const struct SeqParamSet *sps = storage->active_sps;
        SwAdjustCoreMaskByWxH(dec_cont->dwl, sps->pic_width, sps->pic_height,
                              1 << sps->log_max_coding_block_size, DWL_CLIENT_TYPE_HEVC_DEC,
                              &dec_cont->core_mask);
        if (CORE_MASK(dec_cont->core_mask) == 0) {
          APITRACEERR("%s","HevcDecDecode# no any core mask support the SPS info\n");
          return DEC_STREAM_NOT_SUPPORTED;
        }
        hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                    DWL_CLIENT_TYPE_HEVC_DEC);
      }

      if (!HevcSpsSupported(dec_cont)) {
        storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
        storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;
        storage->pic_started = HANTRO_FALSE;
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
        storage->prev_buf_not_finished = HANTRO_FALSE;
        output->data_left = 0;
        if (dec_cont->low_latency) {
          output->strm_curr_pos = input->stream + input->data_len;
          output->strm_curr_bus_address = input->buffer_bus_address + input->data_len;
        }

        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                (void*)dec_cont->stream_consumed_callback.p_user_data);
        }

        return_value = DEC_STREAM_NOT_SUPPORTED;

        strm_len = 0;
        dec_cont->dpb_mode = DEC_DPB_DEFAULT;
        return return_value;
      } else {
        if (storage->active_sps) {
          dec_cont->asic_buff->bit_depth_luma = storage->active_sps->bit_depth_luma;
          dec_cont->asic_buff->bit_depth_chroma = storage->active_sps->bit_depth_chroma;
          dec_cont->asic_buff->pic_width = storage->active_sps->pic_width;
          dec_cont->asic_buff->pic_height = storage->active_sps->pic_height;
        }

        dec_result = HEVC_BUFFER_NOT_READY;
        dec_cont->dec_state = HEVCDEC_WAITING_FOR_BUFFER;
        strm_len = 0;
        return_value = DEC_HDRS_RDY;
      }

      strm_len = 0;
      dec_cont->dpb_mode = DEC_DPB_DEFAULT;
      break;
    }
    case HEVC_BUFFER_NOT_READY: {
      i32 ret;

      HevcCheckBufferRealloc(dec_cont, storage);

      if (storage->realloc_ext_buf) {
        HevcSetExternalBufferInfo(dec_cont, storage);
      }

      if (dec_cont->pp_enabled) {
        dec_cont->prev_pp_width = dec_cont->ppu_cfg[0].scale.width;
        dec_cont->prev_pp_height = dec_cont->ppu_cfg[0].scale.height;
      }

      struct HevcDecAsic *asic = dec_cont->asic_buff;

      /* Move ReleaseAsicBuffers from HevcAllocateResources() to here, for when
         enabling ASIC_TRACE_SUPPORT, misc_linear buffer is allocated by DWLMallocRefFrm,
         which needs to be freed before freeing reference buffer. Otherwise, DPB
         linear buffer may be not enough for some cases. */
      ReleaseAsicBuffers(dec_cont, asic);

      ret = HevcAllocateSwResources(dec_cont->dwl, storage, dec_cont, dec_cont->n_cores);
      if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

      ret = HevcAllocateResources(dec_cont);
      if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

      ret = AllocateAsicTileEdgeMems(dec_cont);
      if (ret != HANTRO_OK) return DEC_MEMFAIL;

      if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
        dec_cont->llstrminfo.strm_status_addr = dec_cont->llstrminfo.strm_status.virtual_address;
      }

#if 0
      u32 max_id = dec_cont->storage.dpb->no_reordering ? 1 :
                   dec_cont->storage.dpb->dpb_size;

      /* Reset multibuffer status */
      HevcPpMultiInit(dec_cont, max_id);
#endif

RESOURCE_NOT_READY:
      if (ret) {
        if (ret == DEC_WAITING_FOR_BUFFER)
          return_value = ret;
        else {
          /* TODO: miten viewit */
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
          storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;

          return_value = DEC_MEMFAIL;          /* signal that decoder failed to init parameter sets */
        }

        strm_len = 0;

        //dec_cont->dpb_mode = DEC_DPB_DEFAULT;

      } else {
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
        //return_value = DEC_HDRS_RDY;
      }

      /* Reset strm_len only for base view -> no HDRS_RDY to
       * application when param sets activated for stereo view */
      //strm_len = 0;

      //dec_cont->dpb_mode = DEC_DPB_DEFAULT;

      /* Initialize tiled mode */
#if 0
      if(DecCheckTiledMode(dec_cont->dpb_mode, 0) != HANTRO_OK ) {
        return_value = DEC_PARAM_ERROR;
      }
#endif

      break;
    }
    case HEVC_NO_FREE_BUFFER: { /* Sleep until buffer free. */
      tmp_stream = input->stream;
      strm_len = 0;
      return_value = DEC_NO_DECODING_BUFFER;
      break;
    }
    case HEVC_WAITING_BUFFER: { /* Allocate buffers externally. */
      tmp_stream = input->stream;
      strm_len = 0;
      return_value = DEC_WAITING_FOR_BUFFER;
      break;
    }
    case HEVC_PIC_RDY: {
      u32 tmp;
      u32 asic_status;
      u32 picture_broken;
      u32 prev_irq_buffer = dec_cont->dec_state == HEVCDEC_BUFFER_EMPTY; /* entry due to IRQ_BUFFER */
      struct HevcDecAsic *asic_buff = dec_cont->asic_buff;

      picture_broken = (storage->picture_broken &&
                        dec_cont->error_policy & DEC_EC_SEEK_NEXT_I &&
                        !IS_I_SLICE(storage->slice_header->slice_type));
      if (dec_cont->error_policy & DEC_EC_REF_REPLACE &&
          dec_cont->use_video_compressor &&
          !IS_RAP_NAL_UNIT(storage->prev_nal_unit) &&
          dec_cont->dec_state != HEVCDEC_BUFFER_EMPTY) {
        tmp = HevcReplaceRefAvalible(dec_cont);
        if (tmp == HANTRO_NOK) {
          /* not find avalible replace ref pic for error ref : SEEK NEXT I */
          picture_broken = HANTRO_TRUE;
          dec_cont->error_policy &= ~DEC_EC_NO_SKIP;
          dec_cont->error_policy |= DEC_EC_SEEK_NEXT_I;
        }
      }

      if (dec_cont->dec_state != HEVCDEC_BUFFER_EMPTY && !picture_broken) {
        /* setup the reference frame list; just at picture start */
        if (!HevcPpsSupported(dec_cont)) {
          storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
          storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
          storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;

          return_value = DEC_STREAM_NOT_SUPPORTED;
          goto end;
        }

        asic_buff->out_buffer = storage->curr_image->data;
        asic_buff->out_pp_buffer = storage->curr_image->pp_data;
        asic_buff->out_parasitic_buffer = storage->curr_image->parasitic_data;
        asic_buff->out_dmv_buffer = storage->curr_image->dmv_data;
        if (storage->active_pps) {
          asic_buff->chroma_qp_index_offset = storage->active_pps->cb_qp_offset;
          asic_buff->chroma_qp_index_offset2 =
            storage->active_pps->cr_qp_offset;
        }


        IncrementDPBRefCount(dec_cont->storage.dpb);

        /* Move below preocess after core_id reserved */
#if 0
        /* Allocate memory for asic filter or reallocate in case old
           one is too small. */
        if (AllocateAsicTileEdgeMems(dec_cont) != HANTRO_OK) {
          return_value = DEC_MEMFAIL;
          goto end;
        }
#endif

        HevcSetRegs(dec_cont, hw_feature);

        /* determine initial reference picture lists */
        HevcInitRefPicList(dec_cont);

#if 0
        /* we trust our memcpy; ignore return value */
        (void)DWLmemcpy(&storage->dpb[1], &storage->dpb[0],
                        sizeof(*storage->dpb));
#endif

        APITRACEDEBUG("%s","HevcDecDecode# Save POC status\n");
        (void)DWLmemcpy(&storage->poc[1], &storage->poc[0],
                        sizeof(*storage->poc));

        /* reset decode status */
        HevcUpdateAfterPictureDecode(dec_cont);

        /* enable output writing by default */
        if (dec_cont->skip_non_intra &&
            dec_cont->pp_enabled)
          dec_cont->asic_buff->disable_out_writing = 1;
        else
          dec_cont->asic_buff->disable_out_writing = 0;

        /* prepare PP if needed */
        /*HevcPreparePpRun(dec_cont);*/
      } else {
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
      }

      /* if there is a chroma sample changed,color-to-monochrome jump or monochrome-to-color jump */
      if(dec_cont->storage.active_sps != NULL &&
         dec_cont->storage.prev_is_monochrome != dec_cont->storage.active_sps->mono_chrome){
        u32 pic_width = dec_cont->storage.active_sps->pic_width;
        u32 pic_height = dec_cont->storage.active_sps->pic_height;
        u32 pixel_width = (dec_cont->storage.active_sps->bit_depth_luma == 8 &&
                  dec_cont->storage.active_sps->bit_depth_chroma == 8) ? 8 : 10;

        if(!dec_cont->storage.prev_is_monochrome){  /* color-to-monochrome jump, exit  to prevent HW IRQ TIMEOUT*/
          for(int i = 0;i<DEC_MAX_PPU_COUNT;i++){
            dec_cont->ppu_cfg[i].monochrome = dec_cont->storage.active_sps->mono_chrome;
          }
        }
        else if(dec_cont->storage.prev_is_monochrome){ /*monochrome-to-color jump,reset ppconfig*/
          PpUnitSetIntConfig(dec_cont->ppu_cfg, dec_cont->reserved_ppu_cfg, hw_feature, pixel_width, 1, 0);
        }

        if (CheckPpUnitConfig(hw_feature, pic_width, pic_height, 0, pixel_width,
                      dec_cont->storage.active_sps->chroma_format_idc, dec_cont->ppu_cfg))
          return DEC_INFOPARAM_ERROR;
      }

      /* run asic and react to the status */
      if (!picture_broken) {
        asic_status = HevcRunAsic(dec_cont, asic_buff);
        if (asic_status == DEC_MEMFAIL) {
          return_value = DEC_MEMFAIL;
          goto end;
        }
      } else {
        if (dec_cont->storage.pic_started)
          /* reset decode status */
          HevcUpdateAfterPictureDecode(dec_cont);
        asic_status = DEC_HW_IRQ_ERROR;
      }

#if 0
      printf("asic_status = %2d, slice type = %c, pic_order_cnt_lsb = %d\n",
             asic_status,
             IS_I_SLICE(storage->slice_header->slice_type) ? 'I' :
             IS_B_SLICE(storage->slice_header->slice_type) ? 'B' :
             IS_P_SLICE(storage->slice_header->slice_type) ? 'P' : '-',
             storage->slice_header->pic_order_cnt_lsb);
#endif

#ifdef RANDOM_CORRUPT_RFC
      /* Only corrupt "good" picture. */
      if (asic_status == DEC_HW_IRQ_RDY)
        HevcCorruptRFC(dec_cont);
#endif

      if (!dec_cont->asic_running && !picture_broken && !dec_cont->b_mc)
        DecrementDPBRefCount(dec_cont->storage.dpb);

      /* Handle system error situations */
      if (asic_status == X170_DEC_TIMEOUT) {
        /* This timeout is DWL(software/os) generated */
        return DEC_HW_TIMEOUT;
      } else if (asic_status == X170_DEC_SYSTEM_ERROR) {
        return DEC_SYSTEM_ERROR;
      } else if (asic_status == X170_DEC_FATAL_SYSTEM_ERROR) {
        return DEC_FATAL_SYSTEM_ERROR;
      } else if (asic_status == X170_DEC_HW_RESERVED) {
        return DEC_HW_RESERVED;
      }

      /* Handle possible common HW error situations */
      if (asic_status & DEC_HW_IRQ_BUS) {
        dec_cont->error_info = DEC_FRAME_ERROR;
        output->strm_curr_pos = (u8 *)input->stream;
        output->strm_curr_bus_address = input->stream_bus_address;
        if (dec_cont->low_latency) {
          while (!dec_cont->llstrminfo.stream_info.last_flag)
            sched_yield();
          input_data_len = dec_cont->llstrminfo.stream_info.send_len;
        }
        output->data_left = input_data_len;
        return DEC_HW_BUS_ERROR;
      } else if (asic_status &  DEC_HW_IRQ_EXT_TIMEOUT) {
        dec_cont->error_info = DEC_FRAME_ERROR;
        return DEC_HW_EXT_TIMEOUT;
      }
      /* Handle stream error dedected in HW */
      else if ((asic_status & DEC_HW_IRQ_TIMEOUT) ||
               (asic_status & DEC_HW_IRQ_ERROR)) {
        dec_cont->error_info = DEC_FRAME_ERROR;
        /* This timeout is HW generated */
        if (asic_status & DEC_HW_IRQ_TIMEOUT) {
          APITRACEERR("%s","HevcDecDecode# IRQ: HW TIMEOUT\n");
#ifdef TIMEOUT_ASSERT
          ASSERT(0);
#endif
        } else {
          APITRACEERR("%s","HevcDecDecode# IRQ: STREAM ERROR dedected\n");
        }

        if (dec_cont->packet_decoded != HANTRO_TRUE) {
          APITRACEDEBUG("%s","Reset pic_started\n");
          dec_cont->storage.pic_started = HANTRO_FALSE;
        }
        dec_cont->storage.picture_broken = HANTRO_TRUE;

        {
          struct StrmData *strm = dec_cont->storage.strm;

          strm->stream_info = &dec_cont->llstrminfo.stream_info;

          if (prev_irq_buffer) {
            /* Call HevcDecDecode() due to DEC_HW_IRQ_BUFFER,
               reset strm to input buffer. */
            strm->strm_buff_start = input->buffer;
            strm->strm_curr_pos = input->stream;
            strm->strm_buff_size = input->buff_len;
            if (dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag)
              input_data_len = dec_cont->llstrminfo.stream_info.send_len;
            strm->strm_data_size = input_data_len;
            strm->strm_buff_read_bits = (u32)(strm->strm_curr_pos - strm->strm_buff_start) * 8;
            strm->is_rb = dec_cont->use_ringbuffer;;
            strm->remove_emul3_byte = 0;
            strm->bit_pos_in_word = 0;
          }
          if (dec_cont->multi_frame_input_flag) {
            if (HevcNextStartCode(strm) == HANTRO_OK) {
              if (dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag)
                strm_len = input_data_len = dec_cont->llstrminfo.stream_info.send_len;
              if (dec_cont->low_latency)
                strm_len = input_data_len - total_read_bytes;
              if(strm->strm_curr_pos >= tmp_stream)
                strm_len -= (strm->strm_curr_pos - tmp_stream);
              else
                strm_len -= (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
              tmp_stream = strm->strm_curr_pos;
            }
          } else {
            if (dec_cont->low_latency) {
              while (!dec_cont->llstrminfo.stream_info.last_flag)
                sched_yield();
              input_data_len = dec_cont->llstrminfo.stream_info.send_len;
            }
            tmp_stream = input->stream + input_data_len;
          }
        }

        dec_cont->stream_pos_updated = 0;
      } else if (asic_status & DEC_HW_IRQ_BUFFER) {
        /* TODO: Need to check for CABAC zero words here? */
        APITRACEDEBUG("%s","HevcDecDecode# IRQ: BUFFER EMPTY\n");

#ifdef CASE_INFO_STAT
        if(case_info.frame_num < 10)
          case_info.slice_num[case_info.frame_num]++;
#endif
        /* a packet successfully decoded, don't Reset pic_started flag if
         * there is a need for rlc mode */
        dec_cont->dec_state = HEVCDEC_BUFFER_EMPTY;
        dec_cont->packet_decoded = HANTRO_TRUE;
        output->data_left = 0;
        if (dec_cont->use_ringbuffer)
          output->strm_curr_pos = input->stream + input->data_len - input->buff_len;
        else
          output->strm_curr_pos = input->stream + input->data_len;

        return DEC_BUF_EMPTY;
      } else { /* OK in here */
        if (dec_cont->hw_conceal)
          dec_cont->error_info = GetDecRegister(dec_cont->hevc_regs, HWIF_ERROR_INFO);
        else
          dec_cont->error_info = DEC_NO_ERROR;

#ifdef CASE_INFO_STAT
        HevcCaseInfoCollect(dec_cont, &case_info);
#endif
        if (IS_RAP_NAL_UNIT(storage->prev_nal_unit) || dec_cont->error_policy & DEC_EC_NO_SKIP) {
          dec_cont->storage.picture_broken = HANTRO_FALSE;
        }

        if (!dec_cont->b_mc && !dec_cont->secure_mode && !dec_cont->hw_conceal) {
          /* CHECK CABAC WORDS */
          struct StrmData strm_tmp = *dec_cont->storage.strm;
          strm_tmp.stream_info = &dec_cont->llstrminfo.stream_info;
          u32 consumed = 0;
          if (dec_cont->hw_stream_start >= (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8))
            consumed = dec_cont->hw_stream_start
                       - (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8);
          else
            consumed = dec_cont->hw_stream_start + dec_cont->hw_buffer_length
                       - (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8);

          strm_tmp.strm_curr_pos = dec_cont->hw_stream_start;
          strm_tmp.strm_buff_read_bits = 8*consumed;
          strm_tmp.bit_pos_in_word = 0;
          if(dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag)
            strm_tmp.strm_data_size = dec_cont->llstrminfo.stream_info.send_len;
          if (strm_tmp.strm_data_size - consumed > CHECK_TAIL_BYTES) {
            /* Do not check CABAC zero words if remaining bytes are too few. */
            u32 tmp = HevcCheckCabacZeroWords( &strm_tmp );
            if(tmp != HANTRO_OK) {
              if (dec_cont->packet_decoded != HANTRO_TRUE) {
                APITRACEDEBUG("%s","HevcDecDecode# Reset pic_started\n");
                dec_cont->storage.pic_started = HANTRO_FALSE;
              }

              dec_cont->storage.picture_broken = HANTRO_TRUE;
              {
                if (dec_cont->storage.dpb->current_out->to_be_displayed &&
                    dec_cont->storage.dpb->current_out->pic_output_flag)
                  dec_cont->storage.dpb->num_out_pics_buffered--;
                if(dec_cont->storage.dpb->fullness > 0)
                  dec_cont->storage.dpb->fullness--;
                dec_cont->storage.dpb->num_ref_frames--;
                dec_cont->storage.dpb->current_out->to_be_displayed = 0;
                dec_cont->storage.dpb->current_out->status = UNUSED;
                HevcUnBindDMVBuffer(dec_cont->storage.dpb,
                    dec_cont->storage.dpb->current_out->dmv_data);
                dec_cont->storage.dpb->current_out->pic_order_cnt = 0;
                dec_cont->storage.dpb->current_out->pic_order_cnt_lsb = 0;
                if (dec_cont->storage.pp_enabled) {
                  InputQueueReturnBuffer(storage->pp_buffer_queue,
                      DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.dpb->current_out->pp_data)));
                  HevcReturnDMVBuffer(dec_cont->storage.dpb,
                      dec_cont->storage.dpb->current_out->dmv_data->bus_address);
                }
                if (dec_cont->storage.dpb->current_out->sei_param)
                  dec_cont->storage.dpb->current_out->sei_param->sei_status = SEI_UNUSED;
              }
            }
          }
          if (dec_cont->multi_frame_input_flag) {
            struct StrmData *strm = dec_cont->storage.strm;
            strm->stream_info = &dec_cont->llstrminfo.stream_info;

            if (HevcNextStartCode(strm) == HANTRO_OK) {
              if (dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag)
                strm_len = input_data_len = dec_cont->llstrminfo.stream_info.send_len;
              if (dec_cont->low_latency)
                strm_len = input_data_len - total_read_bytes;
              if(strm->strm_curr_pos >= tmp_stream)
                strm_len -= (strm->strm_curr_pos - tmp_stream);
              else
                strm_len -= (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
              tmp_stream = strm->strm_curr_pos;
            }
          } else {
            if (dec_cont->low_latency) {
              while (!dec_cont->llstrminfo.stream_info.last_flag)
                sched_yield();
              input_data_len = dec_cont->llstrminfo.stream_info.send_len;
            }
            tmp_stream = input->stream + input_data_len;
          }
          dec_cont->stream_pos_updated = 0;
        } else if (!dec_cont->b_mc && dec_cont->hw_conceal) {
          /* do nothing */
        } else { /* dec_cont->b_mc = 1*/
          tmp_stream = (u8 *)input->stream + input_data_len;
          strm_len = 0;
          dec_cont->stream_pos_updated = 0;
        }
      }

#if 0
      CHECK CABAC WORDS
      struct StrmData strm_tmp = *dec_cont->storage.strm;
      tmp = dec_cont->hw_stream_start-input->stream;
      strm_tmp.strm_curr_pos = dec_cont->hw_stream_start;
      strm_tmp.strm_buff_read_bits = 8*tmp;
      strm_tmp.bit_pos_in_word = 0;
      strm_tmp.strm_buff_size = input->data_len;
      tmp = HevcCheckCabacZeroWords( &strm_tmp );
      if( tmp != HANTRO_OK ) {
        {
          struct StrmData *strm = dec_cont->storage.strm;
          const u8 *next =
            HevcFindNextStartCode(strm->strm_buff_start,
                                  strm->strm_buff_size);

          if(next != NULL) {
            u32 consumed;

            tmp_stream -= num_read_bytes;
            strm_len += num_read_bytes;

            consumed = (u32) (next - tmp_stream);
            tmp_stream += consumed;
            strm_len -= consumed;
          }
        }

      }
#endif

      /* For the switch between modes */
      dec_cont->packet_decoded = HANTRO_FALSE;
      dec_cont->pic_number++;
      storage->prev_idr_pic_ready = IS_IDR_NAL_UNIT(storage->prev_nal_unit);
      HevcSCMarkOutputPicInfo(dec_cont, picture_broken);
      HevcCycleCount(dec_cont);

      {
        /* C.5.2.3 Additional bumping */
        /* TODO:
         * sps_max_latency_increase_plus1[ HighestTid ] is not equal to 0 and
         * there is at least one picture in the DPB that is marked as "needed
         * for output" for which the associated variable PicLatencyCount that is
         * greater than or equal to SpsMaxLatencyPictures[ HighestTid ].
         */
        if (dec_cont->storage.active_sps != NULL) {
          u32 sublayer = dec_cont->storage.active_sps->max_sub_layers - 1;
          if (dec_cont->storage.active_sps->max_latency_increase[sublayer]) {
            u32 max_latency =
              dec_cont->storage.active_sps->max_num_reorder_pics[sublayer] +
              dec_cont->storage.active_sps->max_latency_increase[sublayer] - 1;
            HevcDpbCheckMaxLatency2(dec_cont->storage.dpb, max_latency);
          }
  #if 0
          u32 max_latency =
            dec_cont->storage.active_sps->max_num_reorder_pics[sublayer] +
            (dec_cont->storage.active_sps->max_latency_increase[sublayer]
             ? dec_cont->storage.active_sps
             ->max_latency_increase[sublayer] -
             1
             : 0);
  #else
          u32 max_latency =
            dec_cont->storage.active_sps->max_num_reorder_pics[sublayer];
  #endif
          HevcDpbCheckMaxLatency(dec_cont->storage.dpb, max_latency);
        }
      }

      return_value = DEC_PIC_DECODED;
      strm_len = 0;
      break;
    }
    case HEVC_PARAM_SET_ERROR: {
      if (HevcValidParamSets(&dec_cont->storage) == HANTRO_NOK && strm_len == 0) {
        return_value = DEC_PARAM_SET_ERROR;
      }

      /* update HW buffers if VLC mode */
      dec_cont->hw_length -= num_read_bytes;
      dec_cont->hw_stream_start = tmp_stream;
      if(tmp_stream >= input->stream)
        dec_cont->hw_stream_start_bus =
          input->stream_bus_address + (u32)(tmp_stream - input->stream);
      else
        dec_cont->hw_stream_start_bus =
          input->buffer_bus_address + (u32)(tmp_stream - input->buffer);

      /* check active sps is valid or not */
      if (dec_cont->storage.active_sps && !HevcSpsSupported(dec_cont)) {
        storage->active_sps_id = MAX_NUM_SEQ_PARAM_SETS;
        storage->active_pps_id = MAX_NUM_PIC_PARAM_SETS;
        storage->active_vps_id = MAX_NUM_VIDEO_PARAM_SETS;
        storage->pic_started = HANTRO_FALSE;
        dec_cont->dec_state = HEVCDEC_INITIALIZED;
        storage->prev_buf_not_finished = HANTRO_FALSE;

        if(dec_cont->b_mc) {
          /* release buffer fully processed by SW */
          if(dec_cont->stream_consumed_callback.fn)
            dec_cont->stream_consumed_callback.fn((u8*)input->stream,
                (void*)dec_cont->stream_consumed_callback.p_user_data);
        }

        return_value = DEC_STREAM_NOT_SUPPORTED;
        dec_cont->dpb_mode = DEC_DPB_DEFAULT;
        goto end;
      }

      return_value = DEC_PARAM_SET_ERROR;
      break;
    }
    case HEVC_NEW_ACCESS_UNIT: {
      dec_cont->stream_pos_updated = 0;

      dec_cont->storage.picture_broken = HANTRO_TRUE;
      HevcDropCurrentPicutre(dec_cont);

      /* reset decode status */
      HevcUpdateAfterPictureDecode(dec_cont);

      /* PP will run in HevcDecNextPicture() for this concealed picture */
      return_value = DEC_PIC_DECODED;

      dec_cont->pic_number++;
      strm_len = 0;

      break;
    }
    case HEVC_ABORTED: {
      dec_cont->dec_state = HEVCDEC_ABORTED;
      return DEC_ABORTED;
    }
    case HEVC_MEMFAIL: {
      /* make sure all in sync in multicore mode, hw idle, output empty */
      if(dec_cont->b_mc)
        HevcMCWaitPicReadyAll(dec_cont);
      return_value = DEC_MEMFAIL;
      goto end;
    }
    /* follow-up case do nothing */
    case HEVC_RDY: /* just skip consumed input data. */
    case HEVC_NONREF_PIC_SKIPPED:
    case HEVC_PB_PIC_SKIPPED:
    case HEVC_NO_REF:
    case HEVC_PARAM_SET_PARSED:
    case HEVC_SEI_PARSED:
    case HEVC_SEI_ERROR:
    case HEVC_SLICE_HDR_ERROR:
    case HEVC_REF_PICS_ERROR:
    case HEVC_NALUNIT_ERROR:
    case HEVC_AU_BOUNDARY_ERROR:
    /* fall through */
    default: {
      /* update HW buffers */
      dec_cont->hw_length -= num_read_bytes;
      dec_cont->hw_stream_start = tmp_stream;
      if(tmp_stream >= input->stream)
        dec_cont->hw_stream_start_bus =
          input->stream_bus_address + (u32)(tmp_stream - input->stream);
      else
        dec_cont->hw_stream_start_bus =
          input->buffer_bus_address + (u32)(tmp_stream - input->buffer);
      /* set return_value by dec_result */
      if (dec_result == HEVC_RDY) return_value = DEC_STRM_PROCESSED;
      if (dec_result == HEVC_NONREF_PIC_SKIPPED) return_value = DEC_NONREF_PIC_SKIPPED;
      if (dec_result == HEVC_PB_PIC_SKIPPED) return_value = DEC_PB_PIC_SKIPPED;
      if (dec_result == HEVC_NO_REF) return_value = DEC_NO_REFERENCE;
      if (dec_result == HEVC_PARAM_SET_PARSED) return_value = DEC_PARAM_SET_PARSED;
      if (dec_result == HEVC_SEI_PARSED) return_value = DEC_SEI_PARSED;
      if (dec_result == HEVC_SEI_ERROR) return_value = DEC_SEI_ERROR;
      if (dec_result == HEVC_SLICE_HDR_ERROR) return_value = DEC_SLICE_HDR_ERROR;
      if (dec_result == HEVC_REF_PICS_ERROR) return_value = DEC_REF_PICS_ERROR;
      if (dec_result == HEVC_NALUNIT_ERROR) return_value = DEC_NALUNIT_ERROR;
      if (dec_result == HEVC_AU_BOUNDARY_ERROR) return_value = DEC_AU_BOUNDARY_ERROR;
      /* SW error detected */
      if (dec_result < 0) {
#ifdef SUPPORT_GDR
      /* SW consumed data of one frame: recovery_poc_cnt-- */
      if (tmp_stream == (input->stream + input_data_len) &&
          dec_cont->storage.is_gdr_frame &&
          dec_cont->storage.sei_param_curr)
        dec_cont->storage.sei_param_curr->recovery_param.recovery_poc_cnt--;
#endif
        break;
      }
    }
    }
  } while (strm_len);

end:
  /*  If Hw decodes stream, update stream buffers from "storage" */
  if (dec_cont->stream_pos_updated || dec_cont->multi_frame_input_flag) {
    if (dec_cont->secure_mode)
      output->data_left = 0;
    else {
      output->strm_curr_pos = (u8 *)dec_cont->hw_stream_start;
      output->strm_curr_bus_address = dec_cont->hw_stream_start_bus;
      output->data_left = dec_cont->hw_length;
    }
  } else {
    /* else update based on SW stream decode stream values */
    u32 data_consumed = (u32)(tmp_stream - input->stream);
    if(tmp_stream >= input->stream)
      data_consumed = (u32)(tmp_stream - input->stream);
    else
      data_consumed = (u32)(tmp_stream + input->buff_len - input->stream);

    output->strm_curr_pos = (u8 *)tmp_stream;
    output->strm_curr_bus_address = input->stream_bus_address + data_consumed;
    if (output->strm_curr_bus_address >= (input->buffer_bus_address + input->buff_len))
      output->strm_curr_bus_address -= input->buff_len;

    output->data_left = input_data_len - data_consumed;
  }
  ASSERT(output->strm_curr_bus_address <= (input->buffer_bus_address + input->buff_len));

  if (dec_cont->storage.sei_param_curr &&
      dec_cont->storage.sei_param_curr->decode_id == input->pic_id &&
      dec_cont->storage.sei_param_curr->bufperiod_present_flag &&
      dec_cont->storage.sei_param_curr->pictiming_present_flag) {
    if (return_value == DEC_PIC_DECODED) {
      if(output->strm_curr_pos > input->stream)
        dec_cont->storage.stream_len = output->strm_curr_pos - input->stream;
      else
        dec_cont->storage.stream_len = output->strm_curr_pos + input->buff_len - input->stream;
      dec_cont->storage.bumping_flag = 1;
    }
  }

  FinalizeOutputAll(&dec_cont->fb_list);

  if (!dec_cont->b_mc) {
    while (HevcDecNextPictureInternal(dec_cont) == DEC_PIC_RDY);
  } else {
    if (return_value == DEC_PIC_DECODED ||
        return_value == DEC_PENDING_FLUSH) {
      HevcMCPushOutputAll(dec_cont);
    } else if(output->data_left == 0) {
      /* release buffer fully processed by SW */
      if(dec_cont->stream_consumed_callback.fn)
        dec_cont->stream_consumed_callback.fn((u8*)input->stream,
            (void*)dec_cont->stream_consumed_callback.p_user_data);
    }
  }

  if (dec_cont->abort)
    return (DEC_ABORTED);
  else
    return (return_value);
}

/* Updates decoder instance after decoding of current picture */
void HevcUpdateAfterPictureDecode(struct HevcDecContainer *dec_cont) {

  struct Storage *storage = &dec_cont->storage;

  HevcResetStorage(storage);

  storage->pic_started = HANTRO_FALSE;
  storage->valid_slice_in_access_unit = HANTRO_FALSE;
}

/* Checks if active SPS is valid, i.e. supported in current profile/level */
u32 HevcSpsSupported(const struct HevcDecContainer *dec_cont) {
  const struct SeqParamSet *sps = dec_cont->storage.active_sps;

  /* check hevc main 10 profile supported or not*/
  if (((sps->bit_depth_luma != 8) || (sps->bit_depth_chroma != 8)) &&
      !dec_cont->hevc_main10_support) {
    APITRACEERR("%s","HevcSpsSupported# Hevc main 10 profile not supported!\n");
    return 0;
  }

  if (!dec_cont->hevc_422_intra_support && sps->chroma_format_idc == 2) {
    APITRACEERR("%s","HevcSpsSupported# 422 profile!!! Not supported by HW\n");
    return 0;
  }

  return 1;
}

/* Checks if active PPS is valid, i.e. supported in current profile/level */
u32 HevcPpsSupported(const struct HevcDecContainer *dec_cont) {
  return dec_cont ? 1 : 0;
}

/* Allocates necessary memory buffers. */
u32 HevcAllocateResources(struct HevcDecContainer *dec_cont) {
  u32 ret;
  struct HevcDecAsic *asic = dec_cont->asic_buff;
  struct Storage *storage = &dec_cont->storage;
  const struct SeqParamSet *sps = storage->active_sps;

  SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_WIDTH_IN_CBS,
                 storage->pic_width_in_cbs);
  SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_HEIGHT_IN_CBS,
                 storage->pic_height_in_cbs);

  {
    u32 ctb_size = 1 << sps->log_max_coding_block_size;
    u32 pic_width_in_ctbs = storage->pic_width_in_ctbs * ctb_size;
    u32 pic_height_in_ctbs = storage->pic_height_in_ctbs * ctb_size;

    u32 partial_ctb_h = sps->pic_width != pic_width_in_ctbs ? 1 : 0;
    u32 partial_ctb_v = sps->pic_height != pic_height_in_ctbs ? 1 : 0;

    SetDecRegister(dec_cont->hevc_regs, HWIF_PARTIAL_CTB_X, partial_ctb_h);
    SetDecRegister(dec_cont->hevc_regs, HWIF_PARTIAL_CTB_Y, partial_ctb_v);

    u32 min_cb_size = 1 << sps->log_min_coding_block_size;
    SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_WIDTH_4X4,
                   (storage->pic_width_in_cbs * min_cb_size) >> 2);

    SetDecRegister(dec_cont->hevc_regs, HWIF_PIC_HEIGHT_4X4,
                   (storage->pic_height_in_cbs * min_cb_size) >> 2);
  }

  ret = AllocateAsicBuffers(dec_cont, asic);

  return ret;
}

/* Returns the sample aspect ratio info */
void HevcGetSarInfo(const struct Storage *storage, u32 *sar_width,
                    u32 *sar_height) {
  switch (HevcAspectRatioIdc(storage)) {
  case 0:
    *sar_width = 0;
    *sar_height = 0;
    break;
  case 1:
    *sar_width = 1;
    *sar_height = 1;
    break;
  case 2:
    *sar_width = 12;
    *sar_height = 11;
    break;
  case 3:
    *sar_width = 10;
    *sar_height = 11;
    break;
  case 4:
    *sar_width = 16;
    *sar_height = 11;
    break;
  case 5:
    *sar_width = 40;
    *sar_height = 33;
    break;
  case 6:
    *sar_width = 24;
    *sar_height = 1;
    break;
  case 7:
    *sar_width = 20;
    *sar_height = 11;
    break;
  case 8:
    *sar_width = 32;
    *sar_height = 11;
    break;
  case 9:
    *sar_width = 80;
    *sar_height = 33;
    break;
  case 10:
    *sar_width = 18;
    *sar_height = 11;
    break;
  case 11:
    *sar_width = 15;
    *sar_height = 11;
    break;
  case 12:
    *sar_width = 64;
    *sar_height = 33;
    break;
  case 13:
    *sar_width = 160;
    *sar_height = 99;
    break;
  case 255:
    HevcSarSize(storage, sar_width, sar_height);
    break;
  default:
    *sar_width = 0;
    *sar_height = 0;
  }
}

/* Get last decoded picture if any available. No pictures are removed
 * from output nor DPB buffers. */
enum DecRet HevcDecPeek(HevcDecInst dec_inst, struct HevcDecPicture *output) {
  struct HevcDecContainer *dec_cont;// = (struct HevcDecContainer *)dec_inst;
  struct DpbPicture *current_out;// = dec_cont->storage.dpb->current_out;

  if (dec_inst == NULL || output == NULL) {
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (struct HevcDecContainer *)dec_inst;
  current_out = dec_cont->storage.dpb->current_out;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state != HEVCDEC_NEW_HEADERS &&
      dec_cont->storage.dpb->fullness && current_out != NULL &&
      current_out->status != EMPTY) {

    u32 cropping_flag;

    output->output_dmv = current_out->dmv_data->virtual_address;
    output->output_dmv_bus_address = current_out->dmv_data->bus_address;
    output->dmv_size = dec_cont->storage.dpb->parasitic_buf_size;

    if (!dec_cont->pp_enabled) {
      output->output_qp = (u32 *)((addr_t)current_out->data->virtual_address +
          dec_cont->storage.dpb->out_qp_offset);
      output->output_qp_bus_address = current_out->data->bus_address +
          dec_cont->storage.dpb->out_qp_offset;
      output->qp_size = dec_cont->storage.qp_mem_size;
    } else {
      output->output_qp = (u32*)((addr_t)current_out->pp_data->virtual_address +
          dec_cont->auxinfo_offset);
      output->output_qp_bus_address = current_out->pp_data->bus_address +
          dec_cont->auxinfo_offset;
      output->qp_size = dec_cont->storage.qp_mem_size;
    }

    output->pictures[0].output_picture = current_out->data->virtual_address;
    output->pictures[0].output_picture_bus_address = current_out->data->bus_address;
    output->pic_id = current_out->pic_id;
    output->poc = current_out->pic_order_cnt;
    output->is_idr_picture = current_out->is_idr;
    output->pic_coding_type = current_out->pic_code_type;

    output->pictures[0].pic_width = HevcPicWidth(&dec_cont->storage);
    output->pictures[0].pic_height = HevcPicHeight(&dec_cont->storage);

    HevcCroppingParams(&dec_cont->storage, &cropping_flag,
                       &output->crop_params.crop_left_offset,
                       &output->crop_params.crop_out_width,
                       &output->crop_params.crop_top_offset,
                       &output->crop_params.crop_out_height);

    if (cropping_flag == 0) {
      output->crop_params.crop_left_offset = 0;
      output->crop_params.crop_top_offset = 0;
      output->crop_params.crop_out_width = output->pictures[0].pic_width;
      output->crop_params.crop_out_height = output->pictures[0].pic_height;
    }

    return (DEC_PIC_RDY);
  } else {
    return (DEC_OK);
  }
}

enum DecRet HevcDecNextPictureInternal(struct HevcDecContainer *dec_cont) {
  struct HevcDecPicture out_pic;
  const struct DpbOutPicture *dpb_out = NULL;
  u32 bit_depth;
  const struct DecHwFeatures *hw_feature = NULL;
  //u32 pic_width, pic_height;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  u32 i = 0;

  DWLmemset(&out_pic, 0, sizeof(struct HevcDecPicture));
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_HEVC_DEC);
  dpb_out = HevcNextOutputPicture(&dec_cont->storage);

  if (dpb_out == NULL) return DEC_OK;

  bit_depth = (dpb_out->bit_depth_luma == 8 && dpb_out->bit_depth_chroma == 8) ? 8 : 10;
  out_pic.bit_depth_luma = dpb_out->bit_depth_luma;
  out_pic.bit_depth_chroma = dpb_out->bit_depth_chroma;
  out_pic.pic_id = dpb_out->pic_id;
  out_pic.poc = dpb_out->poc;
  out_pic.pic_coding_type = dpb_out->pic_code_type;
  out_pic.is_idr_picture = dpb_out->is_idr;
  out_pic.sei_param = dpb_out->sei_param;
  out_pic.crop_params = dpb_out->crop_params;
  out_pic.cycles_per_mb = dpb_out->cycles_per_mb;
#ifdef FPGA_PERF_AND_BW
  out_pic.bitrate_4k60fps = dpb_out->bitrate_4k60fps;
  out_pic.bwrd_in_fs = dpb_out->bwrd_in_fs;
  out_pic.bwwr_in_fs = dpb_out->bwwr_in_fs;
#endif
  out_pic.pp_enabled = 0;
  out_pic.output_dmv = (u32*)((addr_t)dpb_out->dmv_data->virtual_address);
  out_pic.output_dmv_bus_address = dpb_out->dmv_data->bus_address;
  out_pic.dmv_size = dec_cont->storage.dpb->parasitic_buf_size;
  out_pic.error_ratio = dpb_out->error_ratio;
  out_pic.error_info = dpb_out->error_info;
  out_pic.is_gdr_frame = dpb_out->is_gdr_frame;

  if (dec_cont->pp_enabled) {
    out_pic.pp_enabled = 1;

    out_pic.output_qp = (u32*)((addr_t)dpb_out->pp_data->virtual_address + dec_cont->auxinfo_offset);
    out_pic.output_qp_bus_address = dpb_out->pp_data->bus_address + dec_cont->auxinfo_offset;
    out_pic.qp_size = dec_cont->storage.qp_mem_size;
    u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, dpb_out->pp_data);
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
      ppu_cfg->tile_coded_image = (dpb_out->num_tile_columns * dpb_out->num_tile_rows > 1);
      out_pic.pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      out_pic.pictures[i].pic_stride = ppu_cfg->ystride;
      out_pic.pictures[i].pic_stride_ch = ppu_cfg->cstride;
      //out_pic.pictures[i].chroma_format = ppu_cfg->pp_chroma_format;
      if (hw_feature->crop_step_rshift) {
        if (ppu_cfg->crop2.enabled) {
          out_pic.pictures[i].pic_width = (dec_cont->ppu_cfg[i].crop2.width / 2) << 1;
          out_pic.pictures[i].pic_height = (dec_cont->ppu_cfg[i].crop2.height / 2) << 1;
        } else {
          out_pic.pictures[i].pic_width = (dec_cont->ppu_cfg[i].scale.width / 2) << 1;
          out_pic.pictures[i].pic_height = (dec_cont->ppu_cfg[i].scale.height / 2) << 1;
        }
      } else {
        /*support odd crop*/
        if (ppu_cfg->crop2.enabled) {
          out_pic.pictures[i].pic_width = dec_cont->ppu_cfg[i].crop2.width;
          out_pic.pictures[i].pic_height = dec_cont->ppu_cfg[i].crop2.height;
        } else {
          out_pic.pictures[i].pic_width = dec_cont->ppu_cfg[i].scale.width;
          out_pic.pictures[i].pic_height = dec_cont->ppu_cfg[i].scale.height;
        }
      }
      out_pic.pictures[i].output_picture = (u32*)((addr_t)dpb_out->pp_data->virtual_address + ppu_cfg->luma_offset);
      out_pic.pictures[i].output_picture_bus_address = dpb_out->pp_data->bus_address + ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome) {
        out_pic.pictures[i].output_picture_chroma = (u32*)((addr_t)dpb_out->pp_data->virtual_address + ppu_cfg->chroma_offset);
        out_pic.pictures[i].output_picture_chroma_bus_address = dpb_out->pp_data->bus_address + ppu_cfg->chroma_offset;
      } else {
        out_pic.pictures[i].output_picture_chroma = NULL;
        out_pic.pictures[i].output_picture_chroma_bus_address = 0;
      }
      if (!output_picture) {
        output_picture = (DWLMemAddr)out_pic.pictures[i].output_picture_bus_address;
      }
      if(ppu_cfg->dec400_enabled)
        PpFillDec400TblInfo(ppu_cfg,
                            dpb_out->pp_data->virtual_address,
                            dpb_out->pp_data->bus_address,
                            &out_pic.pictures[i].luma_table,
                            &out_pic.pictures[i].chroma_table);
    }
  }
  else {
#ifdef TILE_8x8
    out_pic.pictures[0].output_format =
      dec_cont->use_video_compressor ? DEC_OUT_FRM_RFC :
        (bit_depth == 8 ? (dpb_out->mono_chrome ? DEC_OUT_FRM_YUV400TILE8x8 : DEC_OUT_FRM_YUV420TILE8x8) :
                          (dpb_out->mono_chrome ? DEC_OUT_FRM_YUV400TILE8x8_PACK10 : DEC_OUT_FRM_YUV420TILE8x8_PACK10));
#else
    out_pic.pictures[0].output_format =
      dec_cont->use_video_compressor ? DEC_OUT_FRM_RFC :
        (bit_depth == 8 ? (dpb_out->mono_chrome ? DEC_OUT_FRM_YUV400TILE : DEC_OUT_FRM_YUV420TILE) :
                          (dpb_out->mono_chrome ? DEC_OUT_FRM_YUV400TILE_PACK10 : DEC_OUT_FRM_YUV420TILE_PACK10));
#endif
    out_pic.pictures[0].pic_height = dpb_out->pic_height;
    out_pic.pictures[0].pic_width = dpb_out->pic_width;
    out_pic.pictures[0].chroma_format = dpb_out->chroma_format_idc;
    {
      out_pic.output_qp = (u32 *)((addr_t)dpb_out->data->virtual_address +
          dec_cont->storage.dpb->out_qp_offset);
      out_pic.output_qp_bus_address = dpb_out->data->bus_address +
          dec_cont->storage.dpb->out_qp_offset;
      out_pic.qp_size = dec_cont->storage.qp_mem_size;

      out_pic.pictures[0].output_picture = dpb_out->data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address = dpb_out->data->bus_address;
      if (dpb_out->mono_chrome) {
        out_pic.pictures[0].output_picture_chroma = NULL;
        out_pic.pictures[0].output_picture_chroma_bus_address = 0;
      }
      else {
        out_pic.pictures[0].output_picture_chroma = (u32*)((u8*)dpb_out->data->virtual_address + dec_cont->storage.pic_size);
        out_pic.pictures[0].output_picture_chroma_bus_address = dpb_out->data->bus_address + dec_cont->storage.pic_size;
      }
      if (dec_cont->use_video_compressor) {
        out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(8 * dpb_out->pic_width * bit_depth,
                                          ALIGN(dec_cont->align) * 8) / 8;
        out_pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dpb_out->pic_width, 16) * bit_depth,
                                          ALIGN(dec_cont->align) * 8) / 8;
      } else {
        out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(TILE_HEIGHT * dpb_out->pic_width * bit_depth,
                                         ALIGN(dec_cont->align) * 8) / 8;
        out_pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dpb_out->pic_width, 16) * bit_depth,
                                         ALIGN(dec_cont->align) * 8) / 8;
      }
    }
    out_pic.pictures[0].luma_table.virtual_address = dpb_out->data->virtual_address +
                                                     dec_cont->storage.dpb[0].cbs_tbl_offsety / 4;
    out_pic.pictures[0].luma_table.bus_address = dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsety;
    out_pic.pictures[0].luma_table.logical_size = dec_cont->storage.dpb_params.tbl_sizey;
    if (dpb_out->mono_chrome) {
      out_pic.output_rfc_chroma_base = NULL;
      out_pic.output_rfc_chroma_bus_address = 0;
    }
    else {
      out_pic.pictures[0].chroma_table.virtual_address = dpb_out->data->virtual_address +
                                                         dec_cont->storage.dpb[0].cbs_tbl_offsetc / 4;
      out_pic.pictures[0].chroma_table.bus_address = dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsetc;
      out_pic.pictures[0].chroma_table.logical_size = dec_cont->storage.dpb_params.tbl_sizec;
    }
    ASSERT(out_pic.pictures[0].output_picture_bus_address);
  }

  (void)HevcDecGetInfo((HevcDecInst)dec_cont, &out_pic.dec_info);
  out_pic.dec_info.pic_buff_size = dec_cont->storage.dpb->tot_buffers;
  /* workaround for error case: active_sps == NULL. */
  out_pic.dec_info.pic_width = out_pic.dec_info.pic_width ?
                               out_pic.dec_info.pic_width :
                               dpb_out->pic_width;
  out_pic.dec_info.pic_height = out_pic.dec_info.pic_height ?
                                out_pic.dec_info.pic_height :
                                dpb_out->pic_height;

  if (dec_cont->storage.pp_enabled)
    InputQueueSetBufAsUsed(dec_cont->storage.pp_buffer_queue, output_picture);
  PushOutputPic(&dec_cont->fb_list, &out_pic, dpb_out->mem_idx);

  /* If reference buffer is not external, consume it and return it to DPB list. */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    PopOutputPic(&dec_cont->fb_list, dpb_out->mem_idx);
    if (!dec_cont->dmv_output_enable)
      HevcReturnDMVBuffer(dec_cont->storage.dpb, out_pic.output_dmv_bus_address);
  }
  return (DEC_PIC_RDY);
}

enum DecRet HevcDecNextPicture(HevcDecInst dec_inst,
                               struct HevcDecPicture *picture) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 ret;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state == HEVCDEC_EOS && IsOutputEmpty(&dec_cont->fb_list)) {
    return (DEC_END_OF_STREAM);
  }

  if ((ret = PeekOutputPic(&dec_cont->fb_list, picture))) {
    /*Abort output fifo */
    if(ret == ABORT_MARKER)
      return (DEC_ABORTED);
    if(ret == FLUSH_MARKER)
      return (DEC_FLUSHED);
    /* For external buffer mode, following pointers are ready: */
    /*
         picture->output_picture
         picture->output_picture_bus_address
         picture->output_downscale_picture
         picture->output_downscale_picture_bus_address
         picture->output_downscale_picture_chroma
         picture->output_downscale_picture_chroma_bus_address
     */

    /* complete output pic EC policy */
    /* decision pic output */
    if (HevcECDecisionOutput(dec_cont, picture) != DEC_PIC_RDY)
      return DEC_DISCARD_INTERNAL;

    return (DEC_PIC_RDY);
  } else {
    return (DEC_OK);
  }
}

/*!\brief Mark last output picture consumed
 *
 * Application calls this after it has finished processing the picture
 * returned by HevcDecMCNextPicture.
 */
enum DecRet HevcDecPictureConsumed(HevcDecInst dec_inst,
                                   const struct HevcDecPicture *picture) {
  u32 id, i;
  const struct DpbStorage *dpb;
  struct HevcDecPicture pic;
  struct HevcDecContainer *dec_cont;
  struct Storage *storage;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (struct HevcDecContainer *)dec_inst;
  storage = &dec_cont->storage;
  ppu_cfg = dec_cont->ppu_cfg;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  dpb = dec_cont->storage.dpb;
  pic = *picture;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    /* If it's external reference buffer, consumed it as usual.*/
    /* find the mem descriptor for this specific buffer */
    for (id = 0; id < dpb->tot_buffers; id++) {
      if (pic.pictures[0].output_picture_bus_address == dpb->pic_buffers[id].bus_address) {
        break;
      }
    }

    /* check that we have a valid id */
    if (id >= dpb->tot_buffers) return DEC_PARAM_ERROR;

    PopOutputPic(&dec_cont->fb_list, dpb->pic_buff_id[id]);

    if (picture->sei_param)
      picture->sei_param->sei_status = SEI_UNUSED;
  } else {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (DWLMemAddr)picture->pictures[i].output_picture_bus_address;
        break;
      }
    }
    /* For raster/dscale buffer, return to input buffer queue. */
    if (storage->pp_buffer_queue) {
      if (InputQueueReturnBuffer(storage->pp_buffer_queue, output_picture) == NULL)
        return DEC_PARAM_ERROR;
      if (picture->sei_param)
        picture->sei_param->sei_status = SEI_UNUSED;
    }
    if (dec_cont->dmv_output_enable)
      HevcReturnDMVBuffer(dec_cont->storage.dpb, picture->output_dmv_bus_address);
  }

  return DEC_OK;
}

enum DecRet HevcDecEndOfStream(HevcDecInst dec_inst) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  i32 count;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  /* No need to call endofstream twice */
  if(dec_cont->dec_state == HEVCDEC_EOS) {
    return (DEC_OK);
  }

  if (dec_cont->n_cores > 1 && dec_cont->b_mc) {
    if (!dec_cont->vcmd_used) {
      /* Check all Core in idle state */
      for(count = 0; count < dec_cont->n_cores_available; count++) {
        while (dec_cont->dec_status[count] == DEC_RUNNING)
          sched_yield();
      }
    }
  } else {
    if (dec_cont->asic_running) {
      if(dec_cont->vcmd_used) {
        /* abort vcd for the last irq is buffer empty */
        DWLAbortCmdbuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      }
      else {
        /* stop HW for the last irq is buffer empty when single core */
        SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ_STAT, 0);
        SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_IRQ, 0);
        SetDecRegister(dec_cont->hevc_regs, HWIF_DEC_E, 0);
        DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                    dec_cont->hevc_regs[1]);
        DWLReleaseHw(dec_cont->dwl, dec_cont->core_id); /* release HW lock */
      }
      dec_cont->asic_running = 0;
      DecrementDPBRefCount(dec_cont->storage.dpb);
      dec_cont->error_info = DEC_FRAME_ERROR;
      dec_cont->hw_conceal = 0;
      HevcSCMarkOutputPicInfo(dec_cont, 0);
    }
  }

  if (dec_cont->vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl, dec_inst);
  }

  /* flush any remaining pictures form DPB */
  HevcFlushBuffer(&dec_cont->storage);

  FinalizeOutputAll(&dec_cont->fb_list);

  while (HevcDecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;

  /* After all output pictures were pushed, update decoder status to
   * reflect the end-of-stream situation. This way the HevcDecNextPicture
   * will not block anymore once all output was handled.
   */
  dec_cont->dec_state = HEVCDEC_EOS;

  /* wake-up output thread */
  PushOutputPic(&dec_cont->fb_list, NULL, -1);

  return (DEC_OK);
}

enum DecRet HevcDecUseExtraFrmBuffers(HevcDecInst dec_inst, u32 n) {

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;

  dec_cont->storage.n_extra_frm_buffers = n;

  return DEC_OK;
}

void HevcCycleCount(struct HevcDecContainer *dec_cont) {
  struct DpbStorage *dpb = dec_cont->storage.dpb;
  struct DpbPicture *current_out = dec_cont->storage.dpb->current_out;
#ifdef FPGA_PERF_AND_BW
  u64 bwrd_in_fs, bwwr_in_fs;
  u32 bit_depth = dec_cont->storage.active_sps->bit_depth_luma;
  u32 pic_size = HevcPicWidth(&dec_cont->storage) * HevcPicHeight(&dec_cont->storage);
  u32 bitrate = ((60 * GetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN) * 8/ HevcPicWidth(&dec_cont->storage))* 3840/ HevcPicHeight(&dec_cont->storage))* 2160 ;
#endif
  u32 cycles = 0;
  u32 mbs = (HevcPicWidth(&dec_cont->storage) *
             HevcPicHeight(&dec_cont->storage)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->hevc_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  current_out->cycles_per_mb = cycles;

#ifdef FPGA_PERF_AND_BW
  current_out->bitrate_4k60fps = bitrate;
  bwrd_in_fs = DWLReadBw(dec_cont->dwl, dec_cont->core_id, 0);// 0 is read bandwidth
  bwwr_in_fs = DWLReadBw(dec_cont->dwl, dec_cont->core_id, 1);// 1 is write bandwidth
  current_out->bwrd_in_fs = bwrd_in_fs * 16 * 1000/ pic_size;
  current_out->bwwr_in_fs = bwwr_in_fs * 16 * 1000/ pic_size;
#endif
  if (dpb->no_reordering) {
    u32 index = (dpb->out_index_w == 0) ? MAX_DPB_SIZE : (dpb->out_index_w - 1);
    /* struct DpbOutPicture *dpb_out = &dpb->out_buf[(dpb->out_index_w + dpb->dpb_size) % (dpb->dpb_size + 1)]; */
    struct DpbOutPicture *dpb_out = &dpb->out_buf[index];
    dpb_out->cycles_per_mb = cycles;
#ifdef FPGA_PERF_AND_BW
    dpb_out->bitrate_4k60fps = bitrate;
    dpb_out->bwrd_in_fs = bwrd_in_fs * 16 * 1000/ pic_size;
    dpb_out->bwwr_in_fs = bwwr_in_fs * 16 * 1000/ pic_size;
#endif
  }

#ifdef FPGA_PERF_AND_BW
  if(!dec_cont->b_mc) {
    DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info, pic_size, bit_depth);
  }
#endif
}

/* mark corrupt picture in output queue for MC */
void HevcMCMarkOutputPicInfo(struct HevcDecContainer *dec_cont,
                             const struct HevcHwRdyCallbackArg *info,
                             enum DecErrorInfo error_info,
                             const u32* dec_regs) {
  const struct DpbStorage *dpb = NULL;
  struct DWLLinearMem *p_out = NULL;
  struct ErrorPicInfo error_pic_info;
  OutputPicInfo pic_info;

  u32 i = 0;
  u32 j = 0;
  u32 tmp = 0;
  u32 num_err_ctbs = 0;
  u32 error_ratio = 0;
  u32 cycles = 0;
  u32 mbs = 0;
  u32 current_pic_id = 0;

  dpb = info->current_dpb;
  p_out = (struct DWLLinearMem *)GetDataById(dpb->fb_list, info->out_id);

  /* cycles count */
  mbs = (HevcPicWidth(&dec_cont->storage) * HevcPicHeight(&dec_cont->storage)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  /* error_info of ctbs_num. */
  if (error_info != DEC_NO_ERROR &&
      error_info != DEC_REF_ERROR) {
    if (dec_cont->hw_conceal)
      num_err_ctbs = GetDecRegister(dec_regs, HWIF_TOTAL_ERROR_CTBS);
    else {
      error_pic_info.error_x = GetDecRegister(dec_regs, HWIF_MB_LOCATION_X);
      error_pic_info.error_y = GetDecRegister(dec_regs, HWIF_MB_LOCATION_Y);
      if (dec_cont->storage.pps[info->pps_id]->tiles_enabled_flag) {
        error_pic_info.tile_info.num_tile_rows    = dec_cont->storage.pps[info->pps_id]->tile_info.num_tile_rows;
        error_pic_info.tile_info.num_tile_columns = dec_cont->storage.pps[info->pps_id]->tile_info.num_tile_columns;
        error_pic_info.tile_info.uniform_spacing  = dec_cont->storage.pps[info->pps_id]->tile_info.uniform_spacing;
        error_pic_info.tile_info.col_width  = dec_cont->storage.pps[info->pps_id]->tile_info.col_width;
        error_pic_info.tile_info.row_height = dec_cont->storage.pps[info->pps_id]->tile_info.row_height;
      } else {
        error_pic_info.tile_info.num_tile_rows    = 1;
        error_pic_info.tile_info.num_tile_columns = 1;
      }
      error_pic_info.pic_width_in_ctb = dec_cont->storage.pic_width_in_ctbs;
      error_pic_info.pic_height_in_ctb = dec_cont->storage.pic_height_in_ctbs;
      error_pic_info.log2_ctb_size = dec_cont->storage.sps[info->sps_id]->log_max_coding_block_size;
      num_err_ctbs = GetErrorCtbCount(&error_pic_info);
    }
    /* the error can be ignored. */
    if (num_err_ctbs == 0)
      error_info = DEC_NO_ERROR;
  }
  error_ratio = num_err_ctbs * EC_ROUND_COEFF /
                (dec_cont->storage.pic_width_in_ctbs * dec_cont->storage.pic_height_in_ctbs);

  /* check error_info of ref_list */
  for (i = 0; i <= dpb->dpb_size; i++) {
    if (dpb->buffer[i].error_info != DEC_NO_ERROR) {
      for (j = 0; j <= dpb->dpb_size; j++) {
        if (dpb->buffer[i].mem_idx == info->ref_mem_idx[j]) {
          error_info |= DEC_REF_ERROR;
          break;
        }
      }
    }
  }

  /* update out_buf: in NextPictureInternal */
  i = dpb->num_out;
  tmp = dpb->out_index_r;
  while((i--) > 0) {
    if (tmp == dpb->dpb_size + 1)
      tmp = 0;
    struct DpbOutPicture *dpb_pic = (struct DpbOutPicture *)dpb->out_buf + tmp;
    if(dpb_pic->data == p_out) {
      dpb_pic->error_info = error_info;
      dpb_pic->error_ratio = error_ratio;
      dpb_pic->cycles_per_mb = cycles;
      current_pic_id = dpb_pic->pic_id;
      break;
    }
    tmp++;
  }

  /* update in DPB */
  i = dpb->dpb_size + 1;
  while((i--) > 0) {
    struct DpbPicture *dpb_pic = (struct DpbPicture *)dpb->buffer + i;
    if(dpb_pic->data == p_out) {
      dpb_pic->error_info = error_info;
      dpb_pic->error_ratio = error_ratio;
      dpb_pic->cycles_per_mb = cycles;
      if (!current_pic_id)
        current_pic_id = dpb_pic->pic_id;
      else
        ASSERT(current_pic_id == dpb_pic->pic_id);
      break;
    }
  }

  /* update in fb_list */
  pic_info.id = info->out_id;
  pic_info.error_ratio = error_ratio;
  pic_info.cycles = cycles;
  pic_info.error_info = error_info;
  pic_info.pic_id = current_pic_id;
  MarkOutputPicInfo(dpb->fb_list, &pic_info);
}

/* mark corrupt picture in output queue for SC */
void HevcSCMarkOutputPicInfo(struct HevcDecContainer *dec_cont, u32 picture_broken) {
  struct Storage *storage = &dec_cont->storage;
  struct DpbStorage *dpb = dec_cont->storage.dpb;
  struct DpbPicture *current_out = dec_cont->storage.dpb->current_out;
  struct ErrorPicInfo error_pic_info;

  u32 i = 0;
  u32 num_err_ctbs = 0;
  u32 error_ratio = 0;

  /* error_info of ctbs_num. */
  if (dec_cont->error_info != DEC_NO_ERROR &&
      dec_cont->error_info != DEC_REF_ERROR) {
    if (dec_cont->hw_conceal)
      num_err_ctbs = GetDecRegister(dec_cont->hevc_regs, HWIF_TOTAL_ERROR_CTBS);
    else {
      error_pic_info.error_x = GetDecRegister(dec_cont->hevc_regs, HWIF_MB_LOCATION_X);
      error_pic_info.error_y = GetDecRegister(dec_cont->hevc_regs, HWIF_MB_LOCATION_Y);
      if (storage->active_pps->tiles_enabled_flag) {
        error_pic_info.tile_info.num_tile_rows    = storage->active_pps->tile_info.num_tile_rows;
        error_pic_info.tile_info.num_tile_columns = storage->active_pps->tile_info.num_tile_columns;
        error_pic_info.tile_info.uniform_spacing  = storage->active_pps->tile_info.uniform_spacing;
        error_pic_info.tile_info.col_width  = storage->active_pps->tile_info.col_width;
        error_pic_info.tile_info.row_height = storage->active_pps->tile_info.row_height;
      } else {
        error_pic_info.tile_info.num_tile_rows    = 1;
        error_pic_info.tile_info.num_tile_columns = 1;
      }
      error_pic_info.pic_width_in_ctb = storage->pic_width_in_ctbs;
      error_pic_info.pic_height_in_ctb = storage->pic_height_in_ctbs;
      error_pic_info.log2_ctb_size = storage->active_sps->log_max_coding_block_size;
      num_err_ctbs = GetErrorCtbCount(&error_pic_info);
    }
    /* the error can be ignored. */
    if (num_err_ctbs == 0)
      dec_cont->error_info = DEC_NO_ERROR;
  }
  if (picture_broken) /* skip HW run, 100% error. */
    error_ratio = 1 * EC_ROUND_COEFF;
  else
    error_ratio = num_err_ctbs * EC_ROUND_COEFF /
                  (storage->pic_width_in_ctbs * storage->pic_height_in_ctbs);

  /* check error_info of ref_list */
  for (i = 0; i <= dpb->dpb_size; i++) {
    if (IS_REFERENCE(dpb->buffer[i]) && dpb->buffer[i].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;
  }

  /* reordering */
  current_out->error_info = dec_cont->error_info;
  current_out->error_ratio = error_ratio;

  /* no-reordering */
  if (dpb->no_reordering) {
    u32 index = (dpb->out_index_w == 0) ? MAX_DPB_SIZE : (dpb->out_index_w - 1);
    /* struct DpbOutPicture *dpb_out = &dpb->out_buf[(dpb->out_index_w + dpb->dpb_size) % (dpb->dpb_size + 1)]; */
    struct DpbOutPicture *dpb_out = &dpb->out_buf[index];
    dpb_out->error_info = dec_cont->error_info;
    dpb_out->error_ratio = error_ratio;
  }
}

/* complete output pic EC policy : decision pic output */
enum DecRet HevcECDecisionOutput(struct HevcDecContainer *dec_cont, struct HevcDecPicture *output) {
  u8 discard_error_pic = 0;

  /* complete output pic EC policy */
  discard_error_pic = (((dec_cont->error_policy & DEC_EC_OUT_NO_ERROR) &&
                        (output->error_info != DEC_NO_ERROR)) ||
                       ((dec_cont->error_policy & DEC_EC_OUT_DECISION) &&
                        (output->error_info != DEC_NO_ERROR) &&
                        (output->error_ratio > dec_cont->error_ratio * 100)));
  if (output->error_info != DEC_NO_ERROR) {
    ECErrorInfoReturn(output->error_info, output->error_ratio, output->pic_id);
  }
  if (discard_error_pic) {
    /* picture consumed: call API function */
    return HevcDecPictureConsumed((void*)dec_cont, output);
  }

  return DEC_PIC_RDY;
}

void HevcDropCurrentPicutre(struct HevcDecContainer *dec_cont) {
  struct Storage *storage = &dec_cont->storage;
  if (dec_cont->storage.dpb->current_out->to_be_displayed &&
      dec_cont->storage.dpb->current_out->pic_output_flag)
    dec_cont->storage.dpb->num_out_pics_buffered--;
  if(dec_cont->storage.dpb->fullness > 0)
    dec_cont->storage.dpb->fullness--;
  dec_cont->storage.dpb->num_ref_frames--;
  dec_cont->storage.dpb->current_out->to_be_displayed = 0;
  dec_cont->storage.dpb->current_out->status = UNUSED;
  HevcUnBindDMVBuffer(dec_cont->storage.dpb,
      dec_cont->storage.dpb->current_out->dmv_data);
  if (storage->pp_enabled) {
    InputQueueReturnBuffer(storage->pp_buffer_queue,
        DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.dpb->current_out->pp_data)));
    HevcReturnDMVBuffer(dec_cont->storage.dpb,
        dec_cont->storage.dpb->current_out->dmv_data->bus_address);
  }
  if (dec_cont->storage.dpb->current_out->sei_param)
    dec_cont->storage.dpb->current_out->sei_param->sei_status = SEI_UNUSED;
  if (dec_cont->storage.no_reordering) {
    dec_cont->storage.dpb->num_out--;
    if (dec_cont->storage.dpb->out_index_w == 0)
      dec_cont->storage.dpb->out_index_w = MAX_DPB_SIZE;
    else
      dec_cont->storage.dpb->out_index_w--;
    ClearOutput(dec_cont->storage.dpb->fb_list, dec_cont->storage.dpb->current_out->mem_idx);
  }
  (void)storage;
}

static u32 HevcReplaceRefAvalible(struct HevcDecContainer *dec_cont) {
  u32 i = 0;
  struct DpbStorage *dpb = &dec_cont->storage.dpb[0];
  struct Storage *storage = &dec_cont->storage;

  u32 curr_poc = storage->dpb->current_out->pic_order_cnt;
  u32 frame_error_num = 0;

  /* ref_buffer error info count num */
  for (i = 0; i <= dpb->dpb_size; i++) {
    if (IS_REFERENCE(dpb->buffer[i])) {
      /* skip self. */
      if (dpb->buffer[i].pic_order_cnt == curr_poc)
        continue;

      if (dpb->buffer[i].error_info == DEC_NO_ERROR)
        return HANTRO_OK;
      else if (dpb->buffer[i].error_info == DEC_REF_ERROR)
        return HANTRO_OK;
      else {
        // /* if error_ratio less than 10%, don't replace it, use it as correct. */
        // if (dpb->buffer[i].error_ratio <= EC_RATIO_THRESHOLD)
        //   return HANTRO_OK;
        // else
          frame_error_num++;
      }
    }
  }

  if (frame_error_num != 0)
    return HANTRO_NOK; /* have error_ref can't be replaced */
  else
    return HANTRO_OK; /* ignore this error_ref */
}

enum DecRet HevcDecAddBuffer(HevcDecInst dec_inst,
                             struct DWLLinearMem *info) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;

  enum DecRet dec_ret = DEC_OK;
  struct Storage *storage = &dec_cont->storage;

  if (dec_inst == NULL || info == NULL ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->logical_size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->ext_buffer_num >= MAX_PIC_BUFFERS)
    return DEC_EXT_BUFFER_REJECTED; /* Too much buffers added. */

  dec_cont->n_ext_buf_size = dec_cont->ext_buffer_size = info->logical_size;
  if (dec_cont->ext_buffer_num == 0)
    dec_cont->buffer_index = 0;
  dec_cont->ext_buffers[dec_cont->ext_buffer_num] = *info;
  dec_cont->ext_buffer_num++;

  switch (dec_cont->buf_type) {
  case REFERENCE_BUFFER: {
    if (dec_cont->add_extra_ext_buf) {
      u32 i, j, ret;
      struct DpbStorage *dpb = dec_cont->storage.dpb;

      /* find first unused and not-to-be-displayed pic */
      for (i = 0; i <= dpb->dpb_size; i++) {
        if (!dpb->buffer[i].to_be_displayed &&
            !(dpb->buffer[i].status && dpb->buffer[i].status != EMPTY) &&
            !IsBufferReferenced(dpb->fb_list, dpb->pic_buff_id[i])) break;
      }

      if (i < MAX_DPB_SIZE + 1) {
        for (j = i; j < dpb->tot_buffers; j++) {
          if (IsFreeById(dpb->fb_list, dpb->pic_buff_id[j]) == HANTRO_OK ||
              dpb->buffer[j].data == NULL)
            break;
        }

        /* then add a new dpb */
        ret = HevcAddOneExtDpb(dec_cont, dpb, j, info);
        if (ret == MEMORY_ALLOCATION_ERROR) return MEMORY_ALLOCATION_ERROR;
      } else {
        if (dec_cont->buffer_index >= MAX_PIC_BUFFERS) {
          /* Too much buffers added. */
          return DEC_EXT_BUFFER_REJECTED;
        }
        j = i;
        ret = HevcAddOneExtDpb(dec_cont, dpb, j, info);
        if (ret == MEMORY_ALLOCATION_ERROR) return MEMORY_ALLOCATION_ERROR;
      }
      HevcAddParasiticBuf(dec_inst, storage->dpb, dec_cont->buffer_index);
      dec_cont->buffer_index++;
      dec_cont->buf_num--;
      dec_cont->add_extra_ext_buf = 0;
    } else {
      i32 i = dec_cont->buffer_index;
      u32 id;
      struct DpbStorage *dpb = dec_cont->storage.dpb;
      HevcAddParasiticBuf(dec_inst, storage->dpb, dec_cont->buffer_index);
      if (i < dpb->tot_buffers) {
        dpb->pic_buffers[i] = *info;
        if (i < dpb->dpb_size + 1) {
          u32 id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
          if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

          dpb->buffer[i].data = dpb->pic_buffers + i;
          dpb->buffer[i].parasitic_data = dpb->dpb_parasitic_buf + i;
          ASSERT(dpb->buffer[i].parasitic_data);
          dpb->buffer[i].mem_idx = id;
          dpb->pic_buff_id[i] = id;
        } else {
          id = AllocateIdFree(dpb->fb_list, dpb->pic_buffers + i);
          if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

          dpb->pic_buff_id[i] = id;
        }
        dec_cont->buffer_index++;
        dec_cont->buf_num--;
      } else {
        /* Adding extra buffers. */
        if (i >= MAX_PIC_BUFFERS) {
          /* Too much buffers added. */
          return DEC_EXT_BUFFER_REJECTED;
        }

        dpb->pic_buffers[i] = *info;
        /* Here we need the allocate a USED id, so that this buffer will be added as free buffer in SetFreePicBuffer. */
        id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
        if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

        dpb->pic_buff_id[i] = id;
        dec_cont->buffer_index++;
        dec_cont->buf_num = 0;
        /* TODO: protect this variable, which may be changed in two threads. */
        dpb->tot_buffers++;

        SetFreePicBuffer(dpb->fb_list, id);
      }

      if (dec_cont->buffer_index < dpb->tot_buffers)
        dec_ret = DEC_WAITING_FOR_BUFFER;
    }
    break;
  }
  case DOWNSCALE_OUT_BUFFER: {
    ASSERT(storage->pp_enabled);
    u32 i = dec_cont->ext_buffer_num;
    if (i >= MAX_PIC_BUFFERS)
      /* Too much buffers added. */
      return DEC_EXT_BUFFER_REJECTED;
    InputQueueAddBuffer(storage->pp_buffer_queue, info);
    dec_cont->buffer_index++;
    if (!dec_cont->add_extra_ext_buf) {
      if (dec_cont->buffer_index < dec_cont->min_buffer_num)
        dec_ret = DEC_WAITING_FOR_BUFFER;
      else
        dec_ret = DEC_OK;
    }
    else
      dec_cont->add_extra_ext_buf = 0;
    break;
  }
  default:
    break;
  }

  return dec_ret;
}

/* If this function is called after HevcDecDecode returns DEC_WAITING_FOR_BUFFER and
   externals buffers are not ready, this function will return the minimum number & size
   of external buffers required. Otherwise, it just returns the size of external buffer
   with buf_num set to 0. */
enum DecRet HevcDecGetBufferInfo(HevcDecInst dec_inst, struct DecBufferInfo *mem_info) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
//  enum DecRet dec_ret = DEC_OK;
  struct DWLLinearMem empty = {0};
  u32 i;
  struct DWLLinearMem *buffer = NULL;

  if (dec_inst == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    struct HevcDecAsic *asic_buff = dec_cont->asic_buff;
    u32 bit_depth = (asic_buff->bit_depth_luma == 8 && asic_buff->bit_depth_chroma == 8) ? 8 : 10;
    if (!dec_cont->use_video_compressor) {
      mem_info->cstride[0] = mem_info->ystride[0] =
                           NEXT_MULTIPLE(4 * asic_buff->pic_width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    }
    else {
      mem_info->ystride[0] = NEXT_MULTIPLE(8 * asic_buff->pic_width * bit_depth,
                                        ALIGN(dec_cont->align) * 8) / 8;
      mem_info->cstride[0] = NEXT_MULTIPLE(4 * asic_buff->pic_width * bit_depth,
                                        ALIGN(dec_cont->align) * 8) / 8;
    }
  } else {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!dec_cont->ppu_cfg[i].enabled) continue;
      mem_info->ystride[i] = dec_cont->ppu_cfg[i].ystride;
      mem_info->cstride[i] = dec_cont->ppu_cfg[i].cstride;
    }
  }

  if (dec_cont->release_buffer) {
    buffer = NULL;
    if (dec_cont->ext_buffer_num) {
      buffer = &dec_cont->ext_buffers[dec_cont->ext_buffer_num - 1];
      dec_cont->ext_buffer_num--;
    }
    if (buffer == NULL) {
        /* All buffers have been released. */
      dec_cont->release_buffer = 0;
      if (dec_cont->pp_enabled && dec_cont->pp_buffer_queue) {
        InputQueueRelease(dec_cont->pp_buffer_queue);
        dec_cont->pp_buffer_queue = InputQueueInit(0);
        if (dec_cont->pp_buffer_queue == NULL)
          return (DEC_MEMFAIL);
        dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue;
      }

      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      /* need to recycle sei */
#ifdef SUPPORT_SEI
      for (i = 0; i < MAX_PIC_BUFFERS; i++)
        HevcSetSeiUnused(dec_cont->storage.sei_param[i]);
#endif
      if (!dec_cont->pp_enabled)
        return (DEC_OK);
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return DEC_WAITING_FOR_BUFFER;
    }
  }

  /* Called after HevcDecDecode returns DEC_WAITING_FOR_BUFFER, set required buf num and size. */
  if(dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;
    return DEC_OK;
  }

  mem_info->buf_to_free = empty;
  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;
  mem_info->add_extra_ext_buf = dec_cont->add_extra_ext_buf;

  ASSERT((mem_info->buf_num && mem_info->next_buf_size) || (DWL_DEVMEM_VAILD(mem_info->buf_to_free)));

  return DEC_WAITING_FOR_BUFFER;
}

void HevcEnterAbortState(struct HevcDecContainer *dec_cont) {
  SetAbortStatusInList(&dec_cont->fb_list);
  InputQueueSetAbort(dec_cont->storage.pp_buffer_queue);
  dec_cont->abort = 1;
}

void HevcExistAbortState(struct HevcDecContainer *dec_cont) {
  ClearAbortStatusInList(&dec_cont->fb_list);
  InputQueueClearAbort(dec_cont->storage.pp_buffer_queue);
  dec_cont->abort = 0;
}

enum DecRet HevcDecAbort(HevcDecInst dec_inst) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting and rs/ds buffer waiting */
  HevcEnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet HevcDecAbortAfter(HevcDecInst dec_inst) {
  struct HevcDecContainer *dec_cont;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (struct HevcDecContainer *)dec_inst;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_state == HEVCDEC_EOS) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }
#endif

  if (dec_cont->asic_running && !dec_cont->b_mc) {
    /* stop HW */
    u32 core_id = (dec_cont->vcmd_used) ? dec_cont->cmdbuf_id : dec_cont->core_id;
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->hevc_regs, dec_cont->vcmd_used, core_id);

    DecrementDPBRefCount(dec_cont->storage.dpb);
    dec_cont->asic_running = 0;
  }

  /* In multi-Core senario, waithwready is executed through listener thread,
     here to check whether HW is finished */
  if(dec_cont->b_mc) {
    SwAbortMCDec(dec_cont->dwl, dec_inst, dec_cont->vcmd_used,
                dec_cont->n_cores_available, NULL);
  }


  /* Clear any remaining pictures and internal parameters in DPB */
  HevcEmptyDpb(dec_cont, dec_cont->storage.dpb);

  /* Clear any internal parameters in storage */
  HevcClearStorage(&(dec_cont->storage));

  /* Clear internal parameters in HevcDecContainer */
  dec_cont->dec_state = HEVCDEC_INITIALIZED;
  dec_cont->start_code_detected = 0;
  dec_cont->pic_number = 0;
  dec_cont->packet_decoded = 0;

#ifdef USE_OMXIL_BUFFER
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size + 2;   /* We need at least (dpb_size+2) output buffers before starting decoding. */
  else
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size + 1;
  dec_cont->buffer_index = 0;
  dec_cont->buf_num = dec_cont->min_buffer_num;
  dec_cont->ext_buffer_num = 0;
#endif

  HevcExistAbortState(dec_cont);

#ifdef USE_OMXIL_BUFFER
  if (!dec_cont->pp_enabled) {
    int i;
    pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
      dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
    }
    pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
  }
#endif

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

void HevcDecSetNoReorder(HevcDecInst dec_inst, u32 no_reorder) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  dec_cont->storage.no_reordering = no_reorder;
  dec_cont->storage.dpb->no_reordering = no_reorder;
}

enum DecRet HevcDecSetInfo(HevcDecInst dec_inst,
                          struct HevcDecConfig *dec_cfg) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  u32 pic_width = dec_cont->storage.active_sps->pic_width;
  u32 pic_height = dec_cont->storage.active_sps->pic_height;
  const struct DecHwFeatures *hw_feature = NULL;
  u32 i = 0;

  PpUnitConfig *ppu_cfg = &dec_cfg->ppu_cfg[0];
  u32 pixel_width = (dec_cont->storage.active_sps->bit_depth_luma == 8 &&
                     dec_cont->storage.active_sps->bit_depth_chroma == 8) ? 8 : 10;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont)
    return DEC_NOT_INITIALIZED;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                              DWL_CLIENT_TYPE_HEVC_DEC);
  if (!hw_feature) {
    APITRACEDEBUG("%s","HevcDecSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
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
  }

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif
   /* Range Mapping */
  if (dec_cont->storage.active_sps->vui_parameters_present_flag) { // some cases not have stuct vui_parameters->video_full_range_flag
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
      dec_cont->ppu_cfg[i].source_range = dec_cont->storage.active_sps->vui_parameters.video_full_range_flag;
    } // when input stream, rgb source range follows yuv source range, which get from stream sps info
  }
  DWLmemcpy(dec_cont->reserved_ppu_cfg, dec_cfg->ppu_cfg, sizeof(dec_cfg->ppu_cfg));
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, hw_feature,
                     pixel_width, 1, dec_cont->storage.active_sps->mono_chrome);

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;

  dec_cont->storage.pp_enabled = dec_cont->pp_enabled;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL) {
      u32 size = LANCZOS_COEFF_BUFFER_SIZE * sizeof(i16) * dec_cont->n_cores;
      dec_cont->ppu_cfg[i].lanczos_table.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(dec_cont->ppu_cfg[i].lanczos_table.mem_type,
                    DWL_MEM_USAGE_IN_PPULANCZOS_TABLE, dec_cont->secure_mode);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }

  if (CheckPpUnitConfig(hw_feature, pic_width, pic_height, 0, pixel_width,
                        dec_cont->storage.active_sps->chroma_format_idc, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  memcpy(dec_cont->delogo_params, dec_cfg->delogo_params, sizeof(dec_cont->delogo_params));
  if (CheckDelogo(dec_cont->delogo_params, dec_cont->storage.active_sps->bit_depth_luma, dec_cont->storage.active_sps->bit_depth_chroma))
    return DEC_PARAM_ERROR;

  /* config error process policy. */
  if (dec_cont->error_handling == DEC_EC_FRAME_TOLERANT_ERROR) {
    if (dec_cfg->hw_conceal)
      dec_cont->error_policy = DEC_EC_PIC_HWEC | DEC_EC_REF_ERROR_IGNORE | DEC_EC_NO_SKIP | DEC_EC_OUT_DECISION;
    else
      dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE | DEC_EC_NO_SKIP | DEC_EC_OUT_DECISION;
  } else if (dec_cont->error_handling == DEC_EC_FRAME_IGNORE_ERROR) {
    if (dec_cfg->hw_conceal)
      dec_cont->error_policy = DEC_EC_PIC_HWEC | DEC_EC_REF_ERROR_IGNORE | DEC_EC_NO_SKIP | DEC_EC_OUT_ALL;
    else
      dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE_ANYWAY | DEC_EC_NO_SKIP | DEC_EC_OUT_ALL;
  } else {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_RESET | DEC_EC_SEEK_NEXT_I | DEC_EC_OUT_NO_ERROR;
    dec_cfg->hw_conceal = 0;
    APITRACEDEBUG("%s","HevcDecSetInfo# DON'T SUPPORT HWEC IN NO ERROR POLICY.\n");
  }
  if (dec_cont->b_mc && dec_cont->error_handling) {
    if (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I ||
        dec_cont->error_policy & DEC_EC_REF_RESET ||
        dec_cont->error_policy & DEC_EC_REF_REPLACE ||
        dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY) {
      APITRACEERR("%s","DON'T SUPPORT THIS ERROR POLICY IN MC MODE.\n");
      return DEC_PARAM_ERROR;
    }
  }

  /* config HWEC. */
  dec_cont->hw_conceal = dec_cfg->hw_conceal ? dec_cfg->hw_conceal : 0;
  /* config slice mode. */
  if (!dec_cont->disable_slice_mode)
    dec_cont->disable_slice_mode = dec_cfg->disable_slice_mode;

  if (dec_cont->b_mc &&
      dec_cont->hw_conceal &&
      !dec_cont->disable_slice_mode) {
    APITRACEERR("%s","HevcDecSetInfo# Don't support slice_level_ec in MC mode.\n");
    return DEC_PARAM_ERROR;
  }

  /* config max temporal layer to decoding */
  dec_cont->max_temporal_layer = dec_cfg->max_temporal_layer;
  if (dec_cont->max_temporal_layer < -1 || dec_cont->max_temporal_layer > 6) {
    APITRACEERR("%s","HevcDecSetInfo# THE VALUE OF TEMPORAL LAYER ID SHALL BE IN THE RANGE OF -1 TO 6 IN HEVC.\n");
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config |= 1 << DOWNSCALE_OUT_BUFFER;
  else
    dec_cont->ext_buffer_config  = 1 << REFERENCE_BUFFER;

  /* update buffer size if there is no buffers in pp queue. */
  if (dec_cont->pp_enabled && dec_cont->storage.pp_buffer_queue &&
      !InputQueueGetBufferCnt(dec_cont->storage.pp_buffer_queue)) {
    HevcSetExternalBufferInfo(dec_cont, &dec_cont->storage);
  }

  return (DEC_OK);
}

void HevcDecUpdateStrmInfoCtrl(HevcDecInst dec_inst, struct strmInfo info) {
  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)dec_inst;
  static u32 len_update = 1;
  struct LLStrmInfo *llstrminfo = &dec_cont->llstrminfo;
  // info.is_back_to_buffer_begin = llstrminfo->stream_info.is_back_to_buffer_begin;
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

    /* check hw status */
    if (!llstrminfo->first_update && (dec_cont->asic_running == 0)) {
      llstrminfo->tmp_length = llstrminfo->ll_strm_len;
      llstrminfo->update_reg_flag = 0;
      llstrminfo->updated_reg = 1;
    }
  }
}

void HevcMCPushOutputAll(struct HevcDecContainer *dec_cont) {
  while (HevcDecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;
}

void HevcMCWaitOutFifoEmpty(struct HevcDecContainer *dec_cont) {
  WaitOutputEmpty(&dec_cont->fb_list);
}

void HevcMCWaitPicReadyAll(struct HevcDecContainer *dec_cont) {
  WaitListNotInUse(&dec_cont->fb_list);
}

void HevcMCSetRefPicStatus(const void *dwl_inst, struct DWLLinearMem *p_dmv_out) {
  /* frame status */
  DWLLinearMemset(dwl_inst, p_dmv_out,  -32, 0xFF, 32);
}

static u32 MCGetRefPicStatus(const u8 *p_sync_mem) {
  u32 ret;

  /* frame status */
  ret = ( p_sync_mem[1] << 8) + p_sync_mem[0];
  return ret;
}


static void MCValidateRefPicStatus(struct HevcDecContainer *dec_cont,
                                   const u32 *hevc_regs,
                                   struct HevcHwRdyCallbackArg *info) {
  const u8* p_ref_stat;
  struct DWLLinearMem *p_dmv_out;
  struct DpbStorage *dpb = info->current_dpb;
  u32 status, expected;
  av_unused addr_t device_bus_addr = 0;
  av_unused u8 sync_buf[32];

  p_dmv_out = &dpb->dmv_buffers[info->dmv_out_id];

  if (p_dmv_out->virtual_address == NULL)
    p_ref_stat = (u8 *)sync_buf;
  else
    p_ref_stat = (u8 *)p_dmv_out->virtual_address - 32;
  ASSERT(p_ref_stat);

  device_bus_addr = p_dmv_out->bus_address - 32;
  DWLDMATransData2(dec_cont->dwl, device_bus_addr, (void *)p_ref_stat, 32, DEVICE_TO_HOST);

  status = MCGetRefPicStatus(p_ref_stat);

  expected = GetDecRegister(hevc_regs, HWIF_PIC_HEIGHT_IN_CBS) <<
      GetDecRegister(hevc_regs, HWIF_MIN_CB_SIZE);

  if(status < expected) {
    ASSERT(status == expected);
    HevcMCSetRefPicStatus(dec_cont->dwl, p_dmv_out);
  }
}

/* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
void HevcMCHwRdyCallback(void *args, i32 core_id) {
  u32 dec_regs[DEC_X170_REGISTERS];

  struct HevcDecContainer *dec_cont = (struct HevcDecContainer *)args;
  struct HevcHwRdyCallbackArg *info;

  const void *dwl;
  const struct DpbStorage *dpb;

  u32 core_status, type;
  u32 i, cmdbuf_id = 0xFFFFFF;
  struct DWLLinearMem *p_out;
  struct DWLLinearMem  *p_dmv_out;


  enum DecErrorInfo error_info = DEC_NO_ERROR;

  ASSERT(dec_cont != NULL);
  ASSERT(core_id < MAX_ASIC_CORES || (dec_cont->vcmd_used && core_id < MAX_MC_CB_ENTRIES));

  dwl = dec_cont->dwl;

  if(dec_cont->vcmd_used) {
    cmdbuf_id = core_id;
    core_id = DWLGetVcmdMCVirtualCoreId(dwl, cmdbuf_id);
  }
  /* take a copy of the args as after we release the HW they
   * can be overwritten.
   */
  info = dec_cont->hw_rdy_callback_arg[core_id];

  dwl = dec_cont->dwl;
  dpb = info->current_dpb;

  /* read all hw regs */
  if(dec_cont->vcmd_used) {
    DWLVcmdMCRefreshStatusRegs(dwl, dec_regs, cmdbuf_id);
  }
  else {
    for (i = 0; i < DEC_X170_REGISTERS; i++) {
      dec_regs[i] = DWLReadReg(dwl, core_id, i * 4);
    }
  }

  /* React to the HW return value */
  core_status = GetDecRegister(dec_regs, HWIF_DEC_IRQ_STAT);

  if (dec_cont->pp_enabled)
    DecreaseInputQueueCnt(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(info->pp_data)));

  p_out = (struct DWLLinearMem *)GetDataById(dpb->fb_list,info->out_id);
  if (p_out == NULL) return;


  p_dmv_out = (struct DWLLinearMem *)&dpb->dmv_buffers[info->dmv_out_id];


  /* check if DEC_RDY, all other status are errors */
  if (core_status != DEC_HW_IRQ_RDY) {
    error_info = DEC_FRAME_ERROR;
#ifdef DEC_PRINT_BAD_IRQ
    APITRACEERR("\nCore %d \"bad\" IRQ = 0x%08x\n",
            core_id, core_status);
#endif
    /* reset HW if still enabled */
    if (core_status & DEC_HW_IRQ_BUFFER) {
      error_info = DEC_NO_ERROR;
      /* reset HW; we don't want an IRQ after reset so disable it */
      SetDecRegister(dec_regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_regs, HWIF_DEC_E, 0);
      if (!dec_cont->vcmd_used)
        DWLDisableHw(dwl, core_id, 0x04, dec_regs[1]);
    }

    /* reset DMV storage for erroneous pictures */
    u32 dmv_mem_size = dec_cont->storage.dmv_mem_size;
    DWLLinearMemset(dec_cont->dwl, p_dmv_out, 0, 0, dmv_mem_size);

    HevcMCSetRefPicStatus(dec_cont->dwl, p_dmv_out);
  } else {
    if (dec_cont->hw_conceal)
      error_info = GetDecRegister(dec_regs, HWIF_ERROR_INFO);
    else
      error_info = DEC_NO_ERROR;

    /* if there is no dpb date, do not call MCValidateRefPicStatus */
    if (GetDecRegister(dec_regs, HWIF_DEC_OUT_DIS) == 0)
      MCValidateRefPicStatus(dec_cont, dec_regs, info);
  }
  /* mark corrupt picture in output queue */
#ifdef FPGA_PERF_AND_BW
  DecPerfInfoCount(dec_cont->dwl, core_id, &dec_cont->perf_info,
                   dec_cont->storage.active_sps->pic_width * dec_cont->storage.active_sps->pic_height,
                   dec_cont->storage.active_sps->bit_depth_luma);
#endif
  HevcMCMarkOutputPicInfo(dec_cont, info, error_info, dec_regs);
  HevcDisableDMVBuffer((struct DpbStorage *)dpb, core_id);

  /* Unbind tile_edge buffer with core_id */
  if (!dec_cont->vcmd_used)
    UnbindAsicTileEdgeMemsFromCore(dec_cont, core_id);
  else
    UnbindAsicTileEdgeMemsFromCore(dec_cont, cmdbuf_id);

  /* release the stream buffer. Callback provided by app */
  if(dec_cont->stream_consumed_callback.fn)
    dec_cont->stream_consumed_callback.fn((u8*)info->stream,
                                          (void*)info->p_user_data);

  type = FB_HW_OUT_FRAME;
  ClearHWOutput(dpb->fb_list, info->out_id, type, dec_cont->pp_enabled);

  /* decrement buffer usage in our buffer handling */
  DecrementDPBRefCountExt((struct DpbStorage *)dpb, info->ref_id);
  /* clear IRQ status reg and release HW core */
  if (dec_cont->vcmd_used) {
    DWLReleaseCmdBuf(dwl, cmdbuf_id);
    if (dec_cont->b_mc)
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)core_id, FIFO_EXCEPTION_DISABLE);
  } else {
    dec_cont->dec_status[core_id] = DEC_IDLE;  /* set dec status to idle */
    DWLReleaseHw(dwl, core_id);
  }
}

void HevcMCSetHwRdyCallback(struct HevcDecContainer *dec_cont) {
  struct DpbStorage *dpb = dec_cont->storage.dpb;
  u32 type, i;

  u32 core_id = dec_cont->vcmd_used ? dec_cont->mc_buf_id : dec_cont->core_id;
  struct HevcHwRdyCallbackArg *arg = dec_cont->hw_rdy_callback_arg[core_id];

  if (arg == NULL) {
    dec_cont->hw_rdy_callback_arg[core_id] = (struct HevcHwRdyCallbackArg *)DWLmalloc(sizeof(struct HevcHwRdyCallbackArg));
    DWLmemset(dec_cont->hw_rdy_callback_arg[core_id], 0, sizeof(struct HevcHwRdyCallbackArg));
    arg = dec_cont->hw_rdy_callback_arg[core_id];
  }

  arg->stream = dec_cont->stream_consumed_callback.p_strm_buff;
  arg->p_user_data = dec_cont->stream_consumed_callback.p_user_data;
  arg->out_id = dpb->current_out->mem_idx;
  arg->dmv_out_id = dpb->current_out->dmv_mem_idx;
  if (dec_cont->pp_enabled)
    arg->pp_data = dpb->current_out->pp_data;
  arg->current_dpb = dpb;
  arg->pps_id = dec_cont->storage.active_pps_id;
  arg->sps_id = dec_cont->storage.active_sps_id;

  /* don't work */
  for (i = 0; i < dpb->dpb_size; i++) {
    arg->ref_id[i] = dpb->ref_id[i];
  }

  /* storage reference mem_idx */
  for (i = 0; i <= dpb->dpb_size; i++) {
    if (IS_REFERENCE(dpb->buffer[i]))
      arg->ref_mem_idx[i] = dpb->buffer[i].mem_idx;
    else
      arg->ref_mem_idx[i] = -1;
  }

  DWLSetIRQCallback(dec_cont->dwl, dec_cont->vcmd_used ? dec_cont->cmdbuf_id : core_id, HevcMCHwRdyCallback, dec_cont);

  type = FB_HW_OUT_FRAME;

  MarkHWOutput(&dec_cont->fb_list, dpb->current_out->mem_idx, type);
}

#ifdef RANDOM_CORRUPT_RFC
u32 HevcCorruptRFC(struct HevcDecContainer *dec_cont) {
  u32 luma_size, chroma_size, rfc_luma_size, rfc_chroma_size;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  struct HevcDecAsic *asic_buff = dec_cont->asic_buff;
  struct DpbStorage *dpb = dec_cont->storage.dpb;

  HevcGetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                    &rfc_luma_size, &rfc_chroma_size);

  /* output buffer is not ringbuffer, just send zero/null parameters*/
  if (RandomizeBitSwapInStream((u8 *)asic_buff->out_buffer->virtual_address,
                               NULL, 0,
                               NEXT_MULTIPLE(luma_size, ref_buffer_align) +
                                 NEXT_MULTIPLE(chroma_size, ref_buffer_align),
                               dec_cont->error_params.swap_bit_odds, 0)) {
    APITRACEERR("%s","Bitswap reference buffer corruption error (wrong config?)\n");
  }
  /* output buffer is not ringbuffer, just send zero/null parameters*/
  if (RandomizeBitSwapInStream((u8 *)asic_buff->out_buffer->virtual_address +
                                dpb->cbs_tbl_offsety, NULL, 0,
                               NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align) +
                                 NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align),
                               dec_cont->error_params.swap_bit_odds, 0)) {
    APITRACEERR("%s","Bitswap RFC table corruption error (wrong config?)\n");
  }
  return 0;
}
#endif

#ifdef CASE_INFO_STAT
void HevcCaseInfoCollect(struct HevcDecContainer *dec_cont, CaseInfo *case_info) {
  struct Storage *storage = &dec_cont->storage;
  u32 crop_width, crop_height, left_offset, top_offset, crop_flag;
  u32 pic_width, pic_height, bit_depth;
  u32 raster_stride;

  HevcCroppingParams(storage, &crop_flag, &left_offset, &crop_width, &top_offset, &crop_height);

  if(crop_flag == 0) {
    crop_width = HevcPicWidth(storage);
    crop_height = HevcPicHeight(storage);
  }

  pic_width = HevcPicWidth(storage);
  pic_height = HevcPicHeight(storage);

  if(crop_width != pic_width || crop_height != pic_height)
    case_info->crop_flag = 1;

  bit_depth = ((HevcLumaBitDepth(storage) != 8) || (HevcChromaBitDepth(storage) != 8)) ? 10 : 8;
  case_info->min_cb_size = storage->active_sps->log_min_coding_block_size;
  case_info->max_cb_size = storage->active_sps->log_max_coding_block_size;
  case_info->pic_width_in_cbs = storage->pic_width_in_cbs;
  case_info->pic_height_in_cbs = storage->pic_height_in_cbs;
  if(bit_depth == 8)
    raster_stride = NEXT_MULTIPLE(crop_width, 16);
  else
    raster_stride = NEXT_MULTIPLE(crop_width * 2, 16);

  if (dec_cont->pic_number == 0) {
    case_info->decode_width = pic_width;
    case_info->decode_height = pic_height;
    case_info->display_height = crop_height;
    case_info->display_width = crop_width;
    case_info->chroma_format_id = storage->active_sps->chroma_format_idc;;
    case_info->bit_depth = bit_depth;
    case_info->crop_x = left_offset;
    case_info->crop_y = top_offset;
  } else {
    if (case_info->decode_width != pic_width
	    || case_info->decode_height != pic_height) {
      case_info->decode_width = MAX (pic_width, case_info->decode_width);
      case_info->decode_height = MAX (pic_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (crop_height, case_info->display_height);
    case_info->display_width = MIN (crop_width, case_info->display_width);
    if (case_info->chroma_format_id != storage->active_sps->chroma_format_idc) {
      case_info->chroma_flag = 1;
    }
    if (case_info->bit_depth != bit_depth) {
      case_info->bit_depth = MAX (bit_depth, case_info->bit_depth);
      case_info->depth_flag = 1;
    }
  }

  if (dec_cont->pic_number < 10) {
    if (case_info->b_slice_flag)
      case_info->frame_type[dec_cont->pic_number] = B_FRAME;
    else if (case_info->p_slice_flag)
      case_info->frame_type[dec_cont->pic_number] = P_FRAME;
    else
      case_info->frame_type[dec_cont->pic_number] = I_FRAME;
    case_info->num_tile_cols_8k[dec_cont->pic_number] = storage->active_pps->tile_info.num_tile_columns;
    case_info->bit_depth_y_minus8[dec_cont->pic_number] = storage->active_sps->bit_depth_luma - 8;
    case_info->bit_depth_c_minus8[dec_cont->pic_number] = storage->active_sps->bit_depth_chroma - 8;
    case_info->blackwhite_e[dec_cont->pic_number] = storage->active_sps->chroma_format_idc == 0 ? 1 : 0;
    case_info->ppin_luma_size[dec_cont->pic_number] = crop_height * raster_stride;
    case_info->slice_num[dec_cont->pic_number] ++;
  }

  case_info->codec = DEC_MODE_HEVC;
  if ((dec_cont->storage.active_pps->tile_info.num_tile_columns
      * dec_cont->storage.active_pps->tile_info.num_tile_rows) > 1)
    case_info->tile_flag = 1;

  case_info->frame_num = dec_cont->pic_number + 1;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->hevc_regs, HWIF_STREAM_LEN) * 8 / HevcPicWidth(&dec_cont->storage))*
                        3840/ HevcPicHeight(&dec_cont->storage))* 2160 / 1024 / 1024;


}
#endif

