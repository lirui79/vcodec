/*------------------------------------------------------------------------------
--       Copyright (c) 2018-2019, VeriSilicon Inc. All rights reserved        --
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
#include "version.h"
#include "avs2_container.h"
#include "avs2decapi.h"
#include "avs2_decoder.h"
#include "avs2_dpb.h"
#include "avs2hwd_asic.h"
#include "avs2decmc_internals.h"
#include "regdrv.h"
#include "deccfg.h"
#include "commonconfig.h"
#include "dwl.h"
#include "vpufeature.h"
#include "ppu.h"
#include "dec_log.h"
#include "errorhandling.h"
#include "commonfunction.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void Avs2CaseInfoCollect(struct Avs2DecContainer *dec_cont, CaseInfo *case_info);
#endif

static void Avs2ResetSeqParam(struct Avs2DecContainer *dec_cont);
static void Avs2UpdateAfterPictureDecode(struct Avs2DecContainer *dec_cont);
static u32 Avs2SpsSupported(const struct Avs2DecContainer *dec_cont);
static u32 Avs2PpsSupported(const struct Avs2DecContainer *dec_cont);

static u32 Avs2AllocateResources(struct Avs2DecContainer *dec_cont);
static void Avs2GetSarInfo(const struct Avs2Storage *storage, u32 *sar_width,
                           u32 *sar_height);
extern void Avs2PreparePpRun(struct Avs2DecContainer *dec_cont);
extern u32 Avs2NextStartCode(struct StrmData *stream);
extern u32 Avs2DpbMarkAllUnused(struct Avs2DpbStorage *dpb);

static enum DecRet Avs2DecNextPictureInternal(struct Avs2DecContainer *dec_cont);
static enum DecRet Avs2ECDecisionOutput(struct Avs2DecContainer *dec_cont, struct Avs2DecPicture *output);
static void Avs2CycleCount(struct Avs2DecContainer *dec_cont);
static void Avs2SCMarkOutputPicInfo(struct Avs2DecContainer *dec_cont, u32 picture_broken);
static void Avs2DropCurrentPicutre(struct Avs2DecContainer *dec_cont);
static void Avs2MCMarkOutputPicInfo(struct Avs2DecContainer *dec_cont,
                                    const struct Avs2HwRdyCallbackArg *info,
                                    enum DecErrorInfo error_info,
                                    const u32* dec_regs);
static void Avs2EnterAbortState(struct Avs2DecContainer *dec_cont);
static void Avs2ExistAbortState(struct Avs2DecContainer *dec_cont);
static u32 Avs2ReplaceRefAvalible(struct Avs2DecContainer *dec_cont);

#ifdef RANDOM_CORRUPT_RFC
u32 Avs2CorruptRFC(struct Avs2DecContainer *dec_cont);
#endif

#define DEC_DPB_NOT_INITIALIZED -1
#define CHECK_TAIL_BYTES 16

#define IS_REFERENCE(a) IsReference(&(a))
static u32 IsReference(const struct Avs2DpbPicture *a) {
  return ((a->status) && (a->status != EMPTY));
}

static void updateHwStream(struct Avs2DecContainer *dec_cont) {
  struct Avs2StreamParam *stream = dec_cont->hwdec.stream;

  /* consider all buffer processed */
  dec_cont->hw_stream_start = stream->stream;
  dec_cont->hw_stream_start_bus = stream->stream_bus_addr;
  /* if turnaround */
  if(dec_cont->hw_stream_start > (dec_cont->hw_buffer + dec_cont->hw_buffer_length)) {
    dec_cont->hw_stream_start -= dec_cont->hw_buffer_length;
    dec_cont->hw_stream_start_bus -= dec_cont->hw_buffer_length;
  }
  dec_cont->hw_length = stream->stream_length;
  dec_cont->hw_bit_pos = stream->stream_offset;
  dec_cont->stream_pos_updated = 1;
}

//#define DUMP_AVS2_OUTPUT
#ifdef DUMP_AVS2_OUTPUT
/*
 * Dump pp planar output to hantro_decoding.yuv, to compare with reference
 * decoder output in decoding order.
 */
 static void DumpHwOutput(Avs2DecInst dec_inst) {

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  struct Avs2Storage *storage = &dec_cont->storage;
  static FILE* hantro_avs2_decoding_file=NULL;

  //static int write_frame_number=0;
  if(hantro_avs2_decoding_file==NULL) {
      hantro_avs2_decoding_file = fopen("hantro_decoding.yuv","wb");
  }
  if(hantro_avs2_decoding_file!=NULL) {
    if(storage->pps.progressive_frame) {
      u8 *p = (u8 *)dec_cont->storage.curr_image->pp_data->virtual_address;
      int i;
      for (i = 0; i < dec_cont->ppu_cfg[0].scale.height; i++) {
        fwrite(p, 1, dec_cont->ppu_cfg[0].scale.width, hantro_avs2_decoding_file);
        p += dec_cont->ppu_cfg[0].ystride;
      }
      p = (u8 *)dec_cont->storage.curr_image->pp_data->virtual_address + dec_cont->ppu_cfg[0].chroma_offset;
      for (i = 0; i < dec_cont->ppu_cfg[0].scale.height; i++) {
        /* chroma in planar */
        fwrite(p, 1, dec_cont->ppu_cfg[0].scale.width / 2, hantro_avs2_decoding_file);
        p += dec_cont->ppu_cfg[0].cstride;
      }
    } else
    {
      if(storage->pps.is_top_field)
      {
        //luma
        for(int line=0;line<dec_cont->storage.curr_image->height;line++)
        {
            fwrite(dec_cont->storage.curr_image->pp_data->virtual_address+dec_cont->storage.curr_image->width*line*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2,
                  dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1,
                  dec_cont->storage.curr_image->width,hantro_avs2_decoding_file);
        }
        for(int line=0;line<dec_cont->storage.curr_image->height/2;line++)
        {
            fwrite(dec_cont->storage.curr_image->pp_data->virtual_address+
                  dec_cont->storage.curr_image->width*dec_cont->storage.curr_image->height*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2 +
                  dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2*line/2,
                  dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1,
                  dec_cont->storage.curr_image->width/2,hantro_avs2_decoding_file);
        }
        for(int line=0;line<dec_cont->storage.curr_image->height/2;line++)
        {
            fwrite(dec_cont->storage.curr_image->pp_data->virtual_address+
                  dec_cont->storage.curr_image->width*dec_cont->storage.curr_image->height*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2
                  +dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2*dec_cont->storage.curr_image->height/2/2
                  +dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2*line/2 ,dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1,
                  dec_cont->storage.curr_image->width/2,hantro_avs2_decoding_file);
        }
      }
      else
      {
        //luma
        for(int line=0;line<dec_cont->storage.curr_image->height;line++)
        {
            fwrite(dec_cont->storage.curr_image->pp_data->virtual_address+dec_cont->storage.curr_image->width*line*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2 +
                  dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/4,
                  dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1,
                  dec_cont->storage.curr_image->width,hantro_avs2_decoding_file);
        }
        for(int line=0;line<dec_cont->storage.curr_image->height/2;line++)
        {
            fwrite(dec_cont->storage.curr_image->pp_data->virtual_address+
                  dec_cont->storage.curr_image->width*dec_cont->storage.curr_image->height*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2 +
                  dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2*line/2 +
                  dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2/4,
                  dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1,
                  dec_cont->storage.curr_image->width/2,hantro_avs2_decoding_file);
        }
        for(int line=0;line<dec_cont->storage.curr_image->height/2;line++)
        {
            fwrite(dec_cont->storage.curr_image->pp_data->virtual_address+
                  dec_cont->storage.curr_image->width*dec_cont->storage.curr_image->height*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2+
                  dec_cont->storage.curr_image->width/2*dec_cont->storage.curr_image->height*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2/2+
                  dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2*line/2+
                  dec_cont->storage.curr_image->width*(dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1)/2/4,
                  dec_cont->ppu_cfg[0].stream_pixel_width == 10 ? 2 : 1,
                  dec_cont->storage.curr_image->width/2,hantro_avs2_decoding_file);
        }
      }
    }
    //printf("write frame number %d, pic ID %d, PP buff size %d\n", ++write_frame_number,dec_cont->storage.current_pic_id,tot_buf_size);
  }
}
#endif

/**
 * Initializes decoder software. Function reserves memory for the
 * decoder instance and calls Avs2Init to initialize the
 * instance data.
 */
enum DecRet Avs2DecInit(Avs2DecInst *dec_inst, const void *dwl,
                        struct Avs2DecConfig *dec_cfg) {
  struct Avs2DecContainer *dec_cont;
  HwdRet hwd_ret;
  u32 core_mask = 0, core_mask_rfc, i;

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  if (dec_inst == NULL || dwl == NULL || dec_cfg == NULL)
    return DEC_PARAM_ERROR;

  *dec_inst = NULL; /* return NULL instance for any error */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_AVS2_DEC, &core_mask);
  /* check that hevc decoding supported in HW */
  if (core_mask == 0) {
    APITRACEERR("%s","Avs2DecInit# Avs2 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  if ((dec_cfg->decoder_mode & DEC_LOW_LATENCY) &&
      !SwGetCoreMaskByFeature(dwl, VSI_LOW_LATENCY)) {
    APITRACEERR("%s","Avs2DecInit# Avs2 low latency not supported in HW\n");
    return DEC_PARAM_ERROR;
  }

#ifdef ASIC_TRACE_SUPPORT
  if (dec_cfg->mcinit_cfg.mc_enable) {
    dec_cfg->mcinit_cfg.mc_enable = 0;
    dec_cfg->sim_mc = 1;
    DWLSetSimMc(dwl);
  }
#endif

  dec_cont = (struct Avs2DecContainer *)DWLmalloc(sizeof(struct Avs2DecContainer));
  if (dec_cont == NULL)
    return DEC_MEMFAIL;

  DWLmemset(dec_cont, 0, sizeof(struct Avs2DecContainer));
  dec_cont->dwl = dwl;
  dec_cont->hwdec.n_cores = DWLReadAsicCoreCount(dec_cont->dwl);
  dec_cont->hwdec.core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->hwdec.core_mask, DWL_CLIENT_TYPE_AVS2_DEC);
  SET_SECURE_MODE(dec_cont->hwdec.core_mask, dec_cont->secure_mode);
  dec_cont->hwdec.n_cores_available = SwGetCores(core_mask);
  core_mask_rfc = SwGetCoreMaskByFeature(dec_cont->dwl, VSI_RFC);
  if (dec_cfg->use_video_compressor && core_mask_rfc)
    dec_cont->use_video_compressor = dec_cont->storage.use_video_compressor = 1;
  /* check whether core mask support rfc */
  if (dec_cont->use_video_compressor) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->hwdec.core_mask);
    if (dec_cont->hwdec.core_mask & core_mask_rfc) {
      dec_cont->hwdec.core_mask &= CORE_MASK(core_mask_rfc);
      dec_cont->hwdec.core_mask |= bak_non_core_mask;
    }
    else {
      dec_cont->use_video_compressor = dec_cont->storage.use_video_compressor = 0;
      APITRACEDEBUG("Avs2DecInit# not any core to support RFC from core_mask 0x%x, disable it\n",
                     dec_cont->hwdec.core_mask);
    }
  }
  if (dec_cfg->mcinit_cfg.mc_enable && dec_cont->hwdec.n_cores_available > 1) {
    dec_cont->hwdec.b_mc = 1;
    dec_cont->hwdec.stream_consumed_callback.fn = dec_cfg->mcinit_cfg.stream_consumed_callback;
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_MULTICORE_E, 1);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_WRITESTAT_E, 1);
  }

  dec_cont->hwdec.regs[0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_AVS2_DEC);
  Avs2Init(dec_cont, dec_cfg->no_output_reordering);
  dec_cont->dec_state = AVS2DEC_INITIALIZED;

  dec_cont->error_info = dec_cont->hwdec.error_info = DEC_NO_ERROR;
  dec_cont->error_ratio = dec_cont->hwdec.error_ratio = dec_cfg->error_ratio;
  dec_cont->error_handling = dec_cont->hwdec.error_handling = dec_cfg->error_handling;
  dec_cont->error_policy = dec_cont->hwdec.error_policy = HANTRO_FALSE;

  pthread_mutex_init(&dec_cont->protect_mutex, NULL);
  dec_cont->checksum = dec_cont; /* save instance as a checksum */

  /* Init frame buffer list */
  InitList(&dec_cont->fb_list);
  dec_cont->storage.picture_broken = HANTRO_FALSE;
  dec_cont->storage.dpb[0].fb_list = &dec_cont->fb_list;
  dec_cont->storage.dpb[1].fb_list = &dec_cont->fb_list;
  dec_cont->pp_buffer_queue = InputQueueInit(0);
  if (dec_cont->pp_buffer_queue == NULL)
    return DEC_MEMFAIL;
  dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue;

  dec_cont->multi_frame_input_flag = dec_cfg->multi_frame_input_flag;
  dec_cont->guard_size = dec_cfg->guard_size;
  dec_cont->use_adaptive_buffers = dec_cfg->use_adaptive_buffers;
  dec_cont->main10_support = SwGetCoreMaskByFeature(dec_cont->dwl, VSI_AVS2_MAIN10);
  dec_cont->align = dec_cont->hwdec.align = dec_cont->storage.align = dec_cfg->align;

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  /* set some hw config */
  dec_cont->hwcfg.use_video_compressor = dec_cont->use_video_compressor;
  dec_cont->hwcfg.disable_out_writing = 0;
  Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_CFG, &dec_cont->hwcfg);

  if (dec_cfg->decoder_mode & DEC_LOW_LATENCY) {
#ifndef ASIC_TRACE_SUPPORT
    dec_cont->low_latency = 1;
    dec_cont->llstrminfo.updated_reg = 0;
#endif
#ifdef SUPPORT_DMA
    dec_cont->llstrminfo.dwl_inst = dwl;
#endif
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BLOCK_BUFFER_MODE_E, 1);
    /* Set flag for using ddr_low_latency model and set pollmode and polltime swregs */
    SetDecRegister(dec_cont->hwdec.regs, HWIF_STREAM_STATUS_EXT_BUFFER_E, 1);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_MC_POLLMODE, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_MC_POLLTIME, 0);
    dec_cont->llstrminfo.strm_status_in_buffer = 1;
  } else {
    SetDecRegister(dec_cont->hwdec.regs, HWIF_STREAM_STATUS_EXT_BUFFER_E, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->hwdec.regs, HWIF_BLOCK_BUFFER_MODE_E, 0);
  }
  dec_cont->hwdec.llstrminfo = &dec_cont->llstrminfo;

#ifdef RANDOM_CORRUPT_RFC
  InitializeRandom(&dec_cont->error_params, "1 : 6", "1 : 100000", "1 : 6", 66);
#endif

  dec_cont->hwdec.vcmd_used = DWLVcmdIsUsed(dec_cont->dwl);
  if (dec_cont->hwdec.vcmd_used && dec_cont->hwdec.b_mc) {
    FifoInit(dec_cont->hwdec.n_cores_available, &dec_cont->hwdec.fifo_core);
    for (i = 0; i < dec_cont->hwdec.n_cores_available; i++) {
      FifoPush(dec_cont->hwdec.fifo_core, (FifoObject)(addr_t)i, FIFO_EXCEPTION_DISABLE);
    }
  }

  /* allocate internal buffers */
  dec_cont->hwdec.secure_mode = dec_cont->secure_mode;
  dec_cont->hwdec.cmems = &dec_cont->cmems;
  hwd_ret = Avs2HwdAllocInternals(&dec_cont->hwdec);
  if (hwd_ret != HWD_OK) {
    APITRACEERR("%s","[avs2dec] Cannot Get Intenal Buffers.\n");
    if (dec_cont->pp_buffer_queue != NULL) {
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue = NULL;
    }
    return DEC_MEMFAIL;
  }

  /* Allocate buffer for ddr_low_latency model */
  if (dec_cont->llstrminfo.strm_status_in_buffer == 1 &&
      dec_cont->llstrminfo.strm_status.virtual_address == NULL){
    dec_cont->llstrminfo.strm_status.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE | DWL_MEM_TYPE_VPU_WORKING;
    if (DWLMallocLinear(dec_cont->dwl, 16 * 4, &dec_cont->llstrminfo.strm_status)) {
      if (dec_cont->pp_buffer_queue != NULL) {
        InputQueueRelease(dec_cont->pp_buffer_queue);
        dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue = NULL;
      }
      return DEC_MEMFAIL;
    }
    dec_cont->llstrminfo.strm_status_addr = dec_cont->llstrminfo.strm_status.virtual_address;
  }

  *dec_inst = (Avs2DecInst)dec_cont;

  return (DEC_OK);
}

/* This function provides read access to decoder information. This
 * function should not be called before Avs2DecDecode function has
 * indicated that headers are ready. */
enum DecRet Avs2DecGetInfo(Avs2DecInst dec_inst, struct Avs2DecInfo *dec_info) {
  u32 cropping_flag;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  struct Avs2Storage *storage;

  if (dec_inst == NULL || dec_info == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  storage = &dec_cont->storage;

  if (storage->sps.cnt == 0 || storage->pps.cnt == 0) {
    return (DEC_HDRS_NOT_RDY);
  }

  dec_info->pic_width = Avs2PicWidth(storage);
  /* height of the frame for interlace stream */
  dec_info->pic_height = Avs2PicHeight(storage) << dec_cont->storage.sps.is_field_sequence;

  // FIXME: dec_info->matrix_coefficients = Avs2MatrixCoefficients(storage);
  // dec_info->video_range = Avs2VideoRange(storage);
  dec_info->mono_chrome = Avs2IsMonoChrome(storage);
  if (dec_cont->pp_enabled)
    dec_info->pic_buff_size = Avs2GetMinDpbSize(storage) + 1 + 1;
  else
    dec_info->pic_buff_size = Avs2GetMinDpbSize(storage) + 1 + 2;
  dec_info->multi_buff_pp_size =
      storage->dpb->no_reordering ? 2 : dec_info->pic_buff_size;
  dec_info->dpb_mode = dec_cont->dpb_mode;

  Avs2GetSarInfo(storage, &dec_info->sar_width, &dec_info->sar_height);

  Avs2CroppingParams(storage, &cropping_flag,
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

  if (dec_cont->pp_enabled)
    if (dec_info->mono_chrome)
      dec_info->output_format = DEC_OUT_FRM_MONOCHROME;
    else
      dec_info->output_format = DEC_OUT_FRM_RASTER_SCAN;
  else
    dec_info->output_format = DEC_OUT_FRM_TILED_4X4;

  dec_info->bit_depth = Avs2SampleBitDepth(storage);
  dec_info->out_bit_depth = Avs2OutputBitDepth(storage);
  dec_info->interlaced_sequence = dec_cont->storage.sps.is_field_sequence;

  if (dec_cont->pp_enabled)
    dec_info->pic_stride =
        NEXT_MULTIPLE(dec_info->pic_width * dec_info->bit_depth, 128) / 8;
  else
    /* Reference buffer. */
    dec_info->pic_stride = dec_info->pic_width * dec_info->bit_depth / 8;

  /* for HDR */
  // dec_info->transfer_characteristics =
  // storage->sps[storage->active_sps_id]->vui_parameters.transfer_characteristics;

  return (DEC_OK);
}

/* Releases the decoder instance. Function calls Avs2ShutDown to
 * release instance data and frees the memory allocated for the
 * instance. */
void Avs2DecRelease(Avs2DecInst dec_inst) {

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 i = 0;

  if (dec_cont == NULL) {
    return;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return;
  }
  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (dec_cont->hwdec.hw_rdy_callback_arg[i]) {
      DWLfree(dec_cont->hwdec.hw_rdy_callback_arg[i]);
      dec_cont->hwdec.hw_rdy_callback_arg[i] = NULL;
    }
  }
  pthread_mutex_destroy(&dec_cont->protect_mutex);

  /* make sure all in sync in multicore mode, hw idle, output empty */
  if(dec_cont->hwdec.b_mc) {
    Avs2MCWaitPicReadyAll(dec_cont);
  } else {
    u32 i;
    const struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;

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

  if (dec_cont->hwdec.asic_running) {
    /* stop HW */
    Avs2HwdStopHw(&dec_cont->hwdec, dec_cont->hwdec.core_id);
    dec_cont->hwdec.asic_running = 0;

    /* Decrement usage for DPB buffers */
    DecrementDPBRefCount(dec_cont->storage.dpb);
  }

  /* block until all output is handled */
  WaitListNotInUse(&dec_cont->fb_list);

  Avs2Shutdown(&dec_cont->storage);

  Avs2FreeDpb(dec_cont, dec_cont->storage.dpb);
  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }
  /* Free buffer for ddr_low_latency model */
  if (dec_cont->llstrminfo.strm_status.virtual_address){
    dec_cont->llstrminfo.strm_status.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE | DWL_MEM_TYPE_VPU_WORKING;
    DWLFreeLinear(dec_cont->dwl, &dec_cont->llstrminfo.strm_status);
    dec_cont->llstrminfo.strm_status.virtual_address = NULL;
  }
  if (dec_cont->storage.pp_enabled)
    InputQueueRelease(dec_cont->storage.pp_buffer_queue);

  // release internal buffers
  Avs2HwdRelease(&dec_cont->hwdec);

#if !defined(EXT_BUF_SAFE_RELEASE)
  if (!dec_cont->pp_enabled) {
    int i;
    pthread_mutex_lock(&dec_cont->fb_list.ref_count_mutex);
    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
      dec_cont->fb_list.fb_stat[i].n_ref_count = 0;
    }
    pthread_mutex_unlock(&dec_cont->fb_list.ref_count_mutex);
  }
#endif

  Avs2ReleaseList(&dec_cont->fb_list);

#ifdef FPGA_PERF_AND_BW
  AveragePerfInfoPrint(&dec_cont->perf_info);
#endif


  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  return;
}

/* Decode stream data. Calls Avs2Decode to do the actual decoding. */
enum DecRet Avs2DecDecode(Avs2DecInst dec_inst,
                          const struct Avs2DecInput *input,
                          struct DecOutput *output) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 strm_len;
  u32 input_data_len;  // used to generate error stream
  const u8 *tmp_stream;
  enum DecRet return_value = DEC_STRM_PROCESSED;
  const struct DecHwFeatures *hw_feature = NULL;
  /* Check that function input parameters are valid */
  if (input == NULL || output == NULL || dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  input_data_len = input->data_len;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if (input->data_len == 0 || input->data_len > DEC_X170_MAX_STREAM_G2 ||
      X170_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      X170_CHECK_BUS_ADDRESS(input->stream_bus_address) ||
      X170_CHECK_VIRTUAL_ADDRESS(input->buffer) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(input->buffer_bus_address)) {
    return DEC_PARAM_ERROR;
  }

  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->hwdec.core_mask),
                              DWL_CLIENT_TYPE_AVS2_DEC);
  dec_cont->stream_pos_updated = 0;
  output->strm_curr_pos = NULL;
  dec_cont->hw_stream_start_bus = input->stream_bus_address;
  dec_cont->hw_buffer_start_bus = input->buffer_bus_address;
  dec_cont->hw_stream_start = input->stream;
  dec_cont->hw_buffer = input->buffer;
  strm_len = dec_cont->hw_length = input_data_len;
  if(dec_cont->low_latency && !dec_cont->llstrminfo.stream_info.last_flag)
    strm_len = dec_cont->hw_length = input->buff_len;
  /* For low latency mode, strmLen is set as a large value */
  dec_cont->hw_buffer_length = input->buff_len;
  tmp_stream = input->stream;
  dec_cont->hwdec.stream_consumed_callback.p_strm_buff = input->stream;
  dec_cont->hwdec.stream_consumed_callback.p_user_data = input->p_user_data;

  /* If there are more buffers to be allocated or to be freed, waiting for
   * buffers ready. */
  if (dec_cont->release_buffer ||
      (dec_cont->next_buf_size != 0 &&
       dec_cont->buffer_num_added < dec_cont->min_buffer_num)) {
    return_value = DEC_WAITING_FOR_BUFFER;
    goto end;
  }

  do {
    enum Avs2Result dec_result;
    u32 num_read_bytes = 0;
    struct Avs2Storage *storage = &dec_cont->storage;
    dec_cont->storage.dpb->storage = &dec_cont->storage;
    if (dec_cont->dec_state == AVS2DEC_NEW_HEADERS) {
    //MEANS new headers(sps) come, need to check if buffers need to be reallocated.
      dec_result = AVS2_HDRS_RDY;
      dec_cont->dec_state = AVS2DEC_INITIALIZED;

    }
     else if (dec_cont->dec_state == AVS2DEC_BUFFER_EMPTY) {
     //to release dpb buffer?
      APITRACEDEBUG("%s","Avs2DecDecode: Skip Avs2DecDecode\n");
      APITRACEDEBUG("%s","Avs2DecDecode: Jump to AVS2_PIC_RDY\n");

      dec_result = AVS2_PIC_RDY;
    }
    else if (dec_cont->dec_state == AVS2DEC_WAITING_FOR_BUFFER) {
      APITRACEDEBUG("%s","Avs2DecDecode: Skip Avs2DecDecode\n");
      APITRACEDEBUG("%s","Avs2DecDecode: Jump to AVS2_PIC_RDY\n");

      dec_result = AVS2_BUFFER_NOT_READY;

    }
    else {
#if 0
    assert((dec_cont->dec_state == AVS2DEC_INITIALIZED) ||
           (dec_cont->dec_state == AVS2DEC_READY));
#endif
    dec_result = Avs2Decode(dec_cont, tmp_stream, strm_len, input,
                            &num_read_bytes);
    if (dec_cont->llstrminfo.stream_info.low_latency && dec_cont->llstrminfo.stream_info.last_flag) {
      input_data_len = dec_cont->llstrminfo.stream_info.send_len;
      strm_len = dec_cont->llstrminfo.stream_info.send_len;
      dec_cont->hw_length = dec_cont->llstrminfo.stream_info.send_len;
      if (tmp_stream < input->stream) {
        strm_len -= tmp_stream + dec_cont->hw_buffer_length - input->stream;
      } else {
        strm_len -= tmp_stream - input->stream;
      }
    }
    if (num_read_bytes > strm_len) num_read_bytes = strm_len;

    ASSERT(num_read_bytes <= strm_len);
  }

    tmp_stream += num_read_bytes;
    if (tmp_stream >= dec_cont->hw_buffer + dec_cont->hw_buffer_length &&
        ((dec_cont->hw_stream_start + dec_cont->hw_length) > (dec_cont->hw_buffer + dec_cont->hw_buffer_length)))
      tmp_stream -= dec_cont->hw_buffer_length;
    strm_len -= num_read_bytes;

    switch (dec_result) {
      /* Split the old case AVS2_HDRS_RDY into case AVS2_HDRS_RDY &
       * AVS2_BUFFER_NOT_READY. */
      /* In case AVS2_BUFFER_NOT_READY, we will allocate resources. */
      case AVS2_HDRS_RDY: {
        /* If both the the size and number of buffers allocated are enough,
         * decoding will continue as normal.
         */
        dec_cont->reset_dpb_done = 0;

        /* flush and reallocate buffers */
        if (storage->dpb->flushed && storage->dpb->num_out) {
          /* output first all DPB stored pictures */
          storage->dpb->flushed = 0;
          dec_cont->dec_state = AVS2DEC_NEW_HEADERS;
          return_value = DEC_PENDING_FLUSH;
          strm_len = 0;
          break;
        }

        WaitOutputEmpty(&dec_cont->fb_list);
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
        if (dec_cont->storage.pp_enabled) {
          u32 ret = InputQueueWaitNotUsed(dec_cont->storage.pp_buffer_queue);
          if (ret) {
            strm_len = 0;
            dec_cont->dec_state = AVS2DEC_NEW_HEADERS;
            return_value = DEC_PENDING_FLUSH;
            break;
          }
        }
#endif
        PushOutputPic(&dec_cont->fb_list, NULL, -2);
        {
          /* check for minimum and maximum dimensions */
          SwAdjustCoreMaskByWxH(dec_cont->dwl, storage->sps.pic_width_in_cbs * 8,
                                storage->sps.pic_height_in_cbs * 8, 1,
                                DWL_CLIENT_TYPE_AVS2_DEC, &dec_cont->hwdec.core_mask);
          if (CORE_MASK(dec_cont->hwdec.core_mask) == 0) {
            APITRACEERR("%s","Avs2DecDecode# no any core mask support the sps info\n");
            return DEC_STREAM_NOT_SUPPORTED;
          }
          hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->hwdec.core_mask),
                                      DWL_CLIENT_TYPE_AVS2_DEC);
        }
        if (!Avs2SpsSupported(dec_cont)) {
          Avs2ResetSeqParam(dec_cont);
          storage->pic_started = HANTRO_FALSE;
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
          storage->prev_buf_not_finished = HANTRO_FALSE;
          output->data_left = 0;

          if(dec_cont->hwdec.b_mc) {
            /* release buffer fully processed by SW */
            if(dec_cont->hwdec.stream_consumed_callback.fn)
              dec_cont->hwdec.stream_consumed_callback.fn((u8*)input->stream,
                  (void*)dec_cont->hwdec.stream_consumed_callback.p_user_data);
          }

          return_value = DEC_STREAM_NOT_SUPPORTED;
          strm_len = 0;
          if (dec_cont->low_latency) {
            output->strm_curr_pos = input->stream + input->data_len;
            output->strm_curr_bus_address = input->buffer_bus_address + input->data_len;
          }
          dec_cont->dpb_mode = DEC_DPB_DEFAULT;
          return return_value;
        } else {
          /* FIXME: Remove it... If raster out, raster manager hasn't been
           * initialized yet. */
          dec_result = AVS2_BUFFER_NOT_READY;
          dec_cont->dec_state = AVS2DEC_WAITING_FOR_BUFFER;
          strm_len = 0;
          return_value = DEC_HDRS_RDY;
        }

        strm_len = 0;
        dec_cont->dpb_mode = DEC_DPB_DEFAULT;
        break;
      }
      case AVS2_BUFFER_NOT_READY: {
        i32 ret;

        if (Avs2IsExternalBuffersRealloc(dec_cont, storage)) {
          Avs2SetExternalBufferInfo(dec_cont, storage);
        }

        Avs2DeriveBufferSpec(storage, &storage->buff_spec, dec_cont->align);

        ret = Avs2AllocateSwResources(dec_cont->dwl, storage, dec_cont);
        if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

        ret = Avs2AllocateResources(dec_cont);
        if (ret != HANTRO_OK) goto RESOURCE_NOT_READY;

      RESOURCE_NOT_READY:
        if (ret) {
          if (ret == DEC_WAITING_FOR_BUFFER)
            return_value = ret;
          else {
            /* TODO: miten viewit */
            Avs2ResetSeqParam(dec_cont);
            return_value = DEC_MEMFAIL; /* signal that decoder failed to init
                                           parameter sets */
          }

          strm_len = 0;

          // dec_cont->dpb_mode = DEC_DPB_DEFAULT;

        } else {
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
          // return_value = DEC_HDRS_RDY;
        }

/* Reset strm_len only for base view -> no HDRS_RDY to
 * application when param sets activated for stereo view */
// strm_len = 0;

// dec_cont->dpb_mode = DEC_DPB_DEFAULT;

/* Initialize tiled mode */
#if 0
      if(
          DecCheckTiledMode(
                             dec_cont->dpb_mode, 0) != HANTRO_OK ) {
        return_value = DEC_PARAM_ERROR;
      }
#endif

        break;
      }

#ifdef GET_FREE_BUFFER_NON_BLOCK
      case AVS2_NO_FREE_BUFFER:
        tmp_stream = input->stream;
        strm_len = 0;
        return_value = DEC_NO_DECODING_BUFFER;
        break;
#endif

      case AVS2_PIC_RDY: {
        HwdRet hwd_ret;
        u32 tmp = 0;
        u32 asic_status;
        u32 picture_broken;
        u32 prev_irq_buffer = dec_cont->dec_state == AVS2DEC_BUFFER_EMPTY; /* entry due to IRQ_BUFFER */
       // struct Avs2AsicBuffers *asic_buff = &dec_cont->storage.cmems;
        struct Avs2StreamParam *stream = &dec_cont->storage.input;

        picture_broken = (storage->picture_broken &&
                          dec_cont->error_policy & DEC_EC_SEEK_NEXT_I &&
                          storage->pps.type != I_IMG);
        if (dec_cont->error_policy & DEC_EC_REF_REPLACE &&
            dec_cont->use_video_compressor &&
            storage->pps.type != I_IMG &&
            dec_cont->dec_state != AVS2DEC_BUFFER_EMPTY) {
          tmp = Avs2ReplaceRefAvalible(dec_cont);
          if (tmp == HANTRO_NOK) {
            /* not find avalible replace ref pic for error ref : SEEK NEXT I */
            picture_broken = HANTRO_TRUE;
            dec_cont->error_policy &= ~DEC_EC_NO_SKIP;
            dec_cont->error_policy |= DEC_EC_SEEK_NEXT_I;
          }
        }

        if (dec_cont->dec_state != AVS2DEC_BUFFER_EMPTY && !picture_broken) {
          /* setup the reference frame list; just at picture start */
          if (!Avs2PpsSupported(dec_cont)) {
            Avs2ResetSeqParam(dec_cont);

            return_value = DEC_STREAM_NOT_SUPPORTED;
            goto end;
          }

          stream->is_rb = ((dec_cont->hw_stream_start + dec_cont->hw_length) > (dec_cont->hw_buffer + dec_cont->hw_buffer_length)) ? 1 : 0;
          stream->stream = (u8 *)dec_cont->hw_stream_start;
          stream->stream_bus_addr = dec_cont->hw_stream_start_bus;
          stream->stream_length = dec_cont->hw_length;
          // stream->stream_offset = 3; //dec_cont->hw_bit_pos/8;
          stream->stream_offset = storage->strm[0].strm_buff_read_bits/8;
          stream->ring_buffer.bus_address = dec_cont->hw_buffer_start_bus;
          stream->ring_buffer.virtual_address = (u32 *)dec_cont->hw_buffer;
          stream->ring_buffer.size = stream->ring_buffer.logical_size =
              dec_cont->hw_buffer_length;
          Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_STREAM, stream);
#ifdef ENABLE_FPGA_VERIFICATION
          if (!dec_cont->pp_enabled) {
            DWLLinearMemset(dec_cont->dwl, storage->curr_image->data, 0, 0,
                            storage->curr_image->data->size);
          }
#endif
          // asic_buff->out_buffer = storage->curr_image->data;
          Avs2SetRecon(storage, &storage->recon, storage->curr_image->data);
          Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_RECON, &storage->recon);

// asic_buff->out_pp_buffer = storage->curr_image->pp_data;
// storage->ppout.pic.nv12.y = *storage->curr_image->pp_data;
// Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_PP, &storage->ppout);
          IncrementDPBRefCount(dec_cont->storage.dpb);

#if 1  // for test, don't delete
          {
            Avs2SetRef(storage, &storage->refs, dec_cont->storage.dpb);

          }
#endif
          /* determine initial reference picture lists */
          Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_REFS,
                           &dec_cont->storage.refs);

          APITRACEDEBUG("%s","Save DPB status\n");
          /* we trust our memcpy; ignore return value */
          (void)DWLmemcpy(&storage->dpb[1], &storage->dpb[0],
                          sizeof(*storage->dpb));

          APITRACEDEBUG("%s","Save POC status\n");
          (void)DWLmemcpy(&storage->poc[1], &storage->poc[0],
                          sizeof(*storage->poc));

          /* create output picture list */
          Avs2UpdateAfterPictureDecode(dec_cont);

          /* remove unused buffer */
          Avs2DpbRemoveUnused(dec_cont->storage.dpb);

          /* set ppu cfg*/
          if (dec_cont->pp_enabled) {
            Avs2SetPp(storage, &storage->ppout, &dec_cont->ppu_cfg[0], hw_feature);
            Avs2HwdSetParams(&dec_cont->hwdec, ATTRIB_PP, &storage->ppout);
          }
        } else {
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
        }

        /* run asic and react to the status */
        if (!picture_broken) {
          APITRACEDEBUG("DECODING DOI=%d, POI=%d %s\n", dec_cont->storage.pps.coding_order,
                   dec_cont->storage.pps.poc,
                   dec_cont->storage.pps.progressive_frame?"FRAME":
                   (dec_cont->storage.pps.is_top_field?"TOP":"BOTTOM"));
          //APITRACEDEBUG(" !!!!!!RUN HARDWARE !!!!!!\n");
          dec_cont->hwdec.cfg->start_code_detected = dec_cont->start_code_detected;
          dec_cont->hwdec.error_info = dec_cont->error_info;
          dec_cont->hwdec.error_policy = dec_cont->error_policy;
          dec_cont->hwdec.curr_img_poi = storage->dpb->current_out->img_poi;
          dec_cont->hwdec.unique_id = UNIQUE_ID(dec_cont->pic_number);
          hwd_ret = Avs2HwdRun(&dec_cont->hwdec, (void*)dec_cont);
          /* low latency: use to sync stream update thread. */
          dec_cont->asic_running = 1;
          if (dec_cont->pp_enabled)
            IncreaseInputQueueCnt(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.dpb->current_out->pp_data)));
          hwd_ret = Avs2HwdSync(&dec_cont->hwdec, 0);
          if (dec_cont->pp_enabled && !dec_cont->hwdec.asic_running && !dec_cont->hwdec.b_mc)
            DecreaseInputQueueCnt(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.dpb->current_out->pp_data)));

          dec_cont->asic_running = 0;
          /* low latency: update updated_reg that update thread know current frame dec done. */
          while(dec_cont->low_latency && (!dec_cont->llstrminfo.updated_reg)){
            sched_yield();
          }
          if(dec_cont->low_latency)
            dec_cont->llstrminfo.updated_reg = 0;

          (void)hwd_ret;
          updateHwStream(dec_cont);
          asic_status = dec_cont->hwdec.status;
          // DecrementDPBRefCount(dec_cont->storage.dpb);
#ifdef DUMP_AVS2_OUTPUT
          DumpHwOutput(dec_cont);
#endif
        } else {
          if (dec_cont->storage.pic_started)
            /* reset decode status */
            Avs2UpdateAfterPictureDecode(dec_cont);
          asic_status = DEC_HW_IRQ_ERROR;
        }
        // TODO(JZQ): used for sw ec debug.
        // if (input->pic_id == 4) asic_status = DEC_HW_IRQ_ERROR;

#ifdef RANDOM_CORRUPT_RFC
        /* Only corrupt "good" picture. */
        if (asic_status == DEC_HW_IRQ_RDY)
          Avs2CorruptRFC(dec_cont);
#endif

        if (!dec_cont->hwdec.asic_running && !picture_broken && !dec_cont->hwdec.b_mc)
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
          if (dec_cont->llstrminfo.stream_info.low_latency) {
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
            APITRACEERR("%s","IRQ: HW TIMEOUT\n");
#ifdef TIMEOUT_ASSERT
            ASSERT(0);
#endif
          } else {
            APITRACEERR("%s","IRQ: STREAM ERROR dedected\n");
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
              /* Call Avs2DecDecode() due to DEC_HW_IRQ_BUFFER,
                 reset strm to input buffer. */
              strm->strm_buff_start = input->buffer;
              strm->strm_curr_pos = input->stream;
              strm->strm_buff_size = input->buff_len;
              if (dec_cont->llstrminfo.stream_info.low_latency &&
                  dec_cont->llstrminfo.stream_info.last_flag)
                input_data_len = dec_cont->llstrminfo.stream_info.send_len;
              strm->strm_data_size = input_data_len;
              strm->strm_buff_read_bits =
                  (u32)(strm->strm_curr_pos - strm->strm_buff_start) * 8;
              strm->is_rb = ((input->stream + input_data_len) > (input->buffer + input->buff_len)) ? 1 : 0;
              ;
              strm->remove_emul3_byte = 0;
              strm->bit_pos_in_word = 0;
            }
            if (dec_cont->multi_frame_input_flag) {
              if (Avs2NextStartCode(strm) == HANTRO_OK) {
                if (dec_cont->llstrminfo.stream_info.low_latency && dec_cont->llstrminfo.stream_info.last_flag)
                  strm_len = dec_cont->llstrminfo.stream_info.send_len;
                if (strm->strm_curr_pos >= tmp_stream)
                  strm_len -= (strm->strm_curr_pos - tmp_stream);
                else
                  strm_len -=
                      (strm->strm_curr_pos + strm->strm_buff_size - tmp_stream);
                tmp_stream = strm->strm_curr_pos;
              }
            } else {
              if (dec_cont->llstrminfo.stream_info.low_latency) {
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
          APITRACEDEBUG("%s","IRQ: BUFFER EMPTY\n");
#ifdef CASE_INFO_STAT
          if(case_info.frame_num < 10)
            case_info.slice_num[case_info.frame_num]++;
#endif
          /* a packet successfully decoded, don't Reset pic_started flag if
           * there is a need for rlc mode */
          dec_cont->dec_state = AVS2DEC_BUFFER_EMPTY;
          dec_cont->packet_decoded = HANTRO_TRUE;
          output->data_left = 0;

          return DEC_BUF_EMPTY;
        } else {/* OK in here */
          dec_cont->error_info = DEC_NO_ERROR;
#ifdef CASE_INFO_STAT
          Avs2CaseInfoCollect(dec_cont, &case_info);
#endif
          if (storage->pps.type == I_IMG || dec_cont->error_policy & DEC_EC_NO_SKIP) {
            dec_cont->storage.picture_broken = HANTRO_FALSE;
          }

          if (!dec_cont->hwdec.b_mc/* && !dec_cont->hw_coneal*/){
            /* CHECK CABAC WORDS */
            struct StrmData strm_tmp = *dec_cont->storage.strm;
            u32 consumed = 0;
            if ((u8 *)dec_cont->hwdec.stream->stream_bus_addr >= (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8))
              consumed = (u8 *)dec_cont->hwdec.stream->stream_bus_addr
                         - (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8);
            else
              consumed = (u8 *)dec_cont->hwdec.stream->stream_bus_addr + dec_cont->hw_buffer_length
                         - (strm_tmp.strm_curr_pos-strm_tmp.strm_buff_read_bits / 8);

            strm_tmp.strm_curr_pos = (u8 *)dec_cont->hwdec.stream->stream_bus_addr;
            strm_tmp.strm_buff_read_bits = 8 * consumed;
            strm_tmp.bit_pos_in_word = 0;
            if(dec_cont->low_latency && dec_cont->llstrminfo.stream_info.last_flag)
              strm_tmp.strm_data_size = dec_cont->llstrminfo.stream_info.send_len;
            if (0) {  // if (strm_tmp.strm_data_size - consumed >
                      // CHECK_TAIL_BYTES) {
              /* Do not check CABAC zero words if remaining bytes are too few.
               */
              u32 tmp = HANTRO_OK;
              if (tmp != HANTRO_OK) {
                if (dec_cont->packet_decoded != HANTRO_TRUE) {
                  APITRACEDEBUG("%s","Reset pic_started\n");
                  dec_cont->storage.pic_started = HANTRO_FALSE;
                }

                dec_cont->storage.picture_broken = HANTRO_TRUE;
                {
                  if (dec_cont->storage.dpb->current_out->to_be_displayed)
                    dec_cont->storage.dpb->num_out_pics_buffered--;
                  if (dec_cont->storage.dpb->fullness > 0)
                    dec_cont->storage.dpb->fullness--;
                  dec_cont->storage.dpb->num_ref_frames--;
                  dec_cont->storage.dpb->current_out->to_be_displayed = 0;
                  dec_cont->storage.dpb->current_out->status = UNUSED;
                  dec_cont->storage.dpb->current_out->img_poi = 0;
                  dec_cont->storage.dpb->current_out->img_coi = 0;
                  if (dec_cont->storage.pp_enabled) {
                    if (!dec_cont->storage.dpb->current_out->first_field)
                      InputQueueReturnBuffer(storage->pp_buffer_queue,
                                        DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.dpb->current_out->pp_data)));
                  }
                }
                if (dec_cont->multi_frame_input_flag) {
                  struct StrmData *strm = dec_cont->storage.strm;

                  if (Avs2NextStartCode(strm) == HANTRO_OK) {
                    if (dec_cont->llstrminfo.stream_info.low_latency && dec_cont->llstrminfo.stream_info.last_flag)
                      strm_len = dec_cont->llstrminfo.stream_info.send_len;
                    if (strm->strm_curr_pos >= tmp_stream)
                      strm_len -= (strm->strm_curr_pos - tmp_stream);
                    else
                      strm_len -= (strm->strm_curr_pos + strm->strm_buff_size -
                                   tmp_stream);
                    tmp_stream = strm->strm_curr_pos;
                  }
                } else {
                  if (dec_cont->llstrminfo.stream_info.low_latency) {
                    while (!dec_cont->llstrminfo.stream_info.last_flag)
                      sched_yield();
                    input_data_len = dec_cont->llstrminfo.stream_info.send_len;
                  }
                  tmp_stream = input->stream + input_data_len;
                }
                dec_cont->stream_pos_updated = 0;
              }
            }
          } else { /* dec_cont->b_mc = 1 */
            tmp_stream = (u8 *)input->stream + input_data_len;
            strm_len = 0;
            dec_cont->stream_pos_updated = 0;
          }
        }

        /* For the switch between modes */
        dec_cont->packet_decoded = HANTRO_FALSE;
        dec_cont->pic_number++;
        Avs2CycleCount(dec_cont);
        Avs2SCMarkOutputPicInfo(dec_cont, picture_broken);

        return_value = DEC_PIC_DECODED;
        strm_len = 0;
        break;
      }
      case AVS2_PARAM_SET_ERROR: {
        if (Avs2ValidParamSets(&dec_cont->storage) == HANTRO_NOK && strm_len == 0) {
          return_value = DEC_PARAM_SET_ERROR;
        }

        /* update HW buffers if VLC mode */
        dec_cont->hw_length -= num_read_bytes;
        dec_cont->hw_stream_start = tmp_stream;
        if (tmp_stream >= input->stream)
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address + (u32)(tmp_stream - input->stream);
        else
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address + (u32)(tmp_stream + input->buff_len - input->stream);

        /* check active sps is valid or not */
        if (!Avs2SpsSupported(dec_cont)) {
          Avs2ResetSeqParam(dec_cont);
          storage->pic_started = HANTRO_FALSE;
          dec_cont->dec_state = AVS2DEC_INITIALIZED;
          storage->prev_buf_not_finished = HANTRO_FALSE;

          if(dec_cont->hwdec.b_mc) {
            /* release buffer fully processed by SW */
            if (dec_cont->hwdec.stream_consumed_callback.fn)
              dec_cont->hwdec.stream_consumed_callback.fn((u8*)input->stream,
                  (void*)dec_cont->hwdec.stream_consumed_callback.p_user_data);
          }

          return_value = DEC_STREAM_NOT_SUPPORTED;
          dec_cont->dpb_mode = DEC_DPB_DEFAULT;
          goto end;
        }

        return_value = DEC_PARAM_SET_ERROR;
        break;
      }
      case AVS2_NEW_ACCESS_UNIT: {
        dec_cont->stream_pos_updated = 0;

        dec_cont->storage.picture_broken = HANTRO_TRUE;
        Avs2DropCurrentPicutre(dec_cont);

        /* reset decode status */
        Avs2UpdateAfterPictureDecode(dec_cont);

        /* PP will run in Avs2DecNextPicture() for this concealed picture */
        return_value = DEC_PIC_DECODED;

        dec_cont->pic_number++;
        strm_len = 0;

        break;
      }
      case AVS2_ABORTED: {
        dec_cont->dec_state = AVS2DEC_ABORTED;
        return DEC_ABORTED;
      }
      // case AVS3_MEMFAIL: {
      //   /* make sure all in sync in multicore mode, hw idle, output empty */
      //   if(dec_cont->hwdec.b_mc)
      //     Avs3MCWaitPicReadyAll(dec_cont);
      //   return_value = DEC_MEMFAIL;
      //   goto end;
      // }
      /* follow-up case do nothing */
      case AVS2_RDY: /* just skip consumed input data. */
      case AVS2_NONREF_PIC_SKIPPED:
      case AVS2_NO_REF:
      case AVS2_NALUNIT_ERROR:
      /* fall through */
      default: {/* AVS2_RDY */
        dec_cont->hw_length -= num_read_bytes;
        dec_cont->hw_stream_start = tmp_stream;
        if (tmp_stream >= input->stream)
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address + (u32)(tmp_stream - input->stream);
        else
          dec_cont->hw_stream_start_bus =
              input->stream_bus_address + (u32)(tmp_stream + input->buff_len - input->stream);

        if(dec_cont->storage.sps.new_sps_flag == 1) {
          if(storage->video_edit_code || storage->sequence_end_flag) {
            if (dec_cont->storage.video_edit_code) {
              dec_cont->storage.dpb->edit_code_flush = 1;
            }

            Avs2DpbMarkAllUnused(dec_cont->storage.dpb);
            FinalizeOutputAll(&dec_cont->fb_list);
            while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY);
            // WaitOutputEmpty(&dec_cont->fb_list);
#ifdef USE_OMXIL_BUFFER
            if (!dec_cont->pp_enabled /*dec_cont->output_format == DEC_OUT_FRM_TILED_4X4*/) {
              MarkListNotInUse(&dec_cont->fb_list);
            }
#endif
            WaitListNotInUse(&dec_cont->fb_list);

            storage->sequence_end_flag =0;
            storage->video_edit_code =0;
            dec_cont->storage.dpb->edit_code_flush = 0;
          }
        }

        /* set return_value by dec_result */
        if (dec_result == AVS2_RDY) return_value = DEC_STRM_PROCESSED;
        if (dec_result == AVS2_NONREF_PIC_SKIPPED) return_value = DEC_NONREF_PIC_SKIPPED;
        if (dec_result == AVS2_NO_REF) return_value = DEC_NO_REFERENCE;
        if (dec_result == AVS2_NALUNIT_ERROR) return_value = DEC_NALUNIT_ERROR;
        /* SW error detected */
        if (dec_result < 0) {
          break;
        }
      }
    }
  } while (strm_len);

end:

  /*  If Hw decodes stream, update stream buffers from "storage" */
  if (dec_cont->stream_pos_updated || dec_cont->multi_frame_input_flag) {
    output->strm_curr_pos = (u8 *)dec_cont->hw_stream_start;
    output->strm_curr_bus_address = dec_cont->hw_stream_start_bus;
    output->data_left = dec_cont->hw_length;
  } else {
    /* else update based on SW stream decode stream values */
    u32 data_consumed = (u32)(tmp_stream - input->stream);
    if (tmp_stream >= input->stream)
      data_consumed = (u32)(tmp_stream - input->stream);
    else
      data_consumed = (u32)(tmp_stream + input->buff_len - input->stream);

    output->strm_curr_pos = (u8 *)tmp_stream;
    output->strm_curr_bus_address = input->stream_bus_address + data_consumed;
    if (output->strm_curr_bus_address >= (input->buffer_bus_address + input->buff_len))
      output->strm_curr_bus_address -= input->buff_len;

    output->data_left = input_data_len - data_consumed;
  }
  ASSERT(output->strm_curr_bus_address <=
         (input->buffer_bus_address + input->buff_len));

  Avs2DpbUpdateOutputList(dec_cont->storage.dpb, &dec_cont->storage.pps);

  FinalizeOutputAll(&dec_cont->fb_list);
  if (!dec_cont->hwdec.b_mc) {
    while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY);
  } else {
    if (return_value == DEC_PIC_DECODED ||
        return_value == DEC_PENDING_FLUSH) {
      Avs2MCPushOutputAll(dec_cont);
    } else if(output->data_left == 0) {
      /* release buffer fully processed by SW */
      if(dec_cont->hwdec.stream_consumed_callback.fn)
        dec_cont->hwdec.stream_consumed_callback.fn((u8*)input->stream,
            (void*)dec_cont->hwdec.stream_consumed_callback.p_user_data);
    }
  }

  if (dec_cont->abort)
    return (DEC_ABORTED);
  else
    return (return_value);
}

/* Updates decoder instance after decoding of current picture */
void Avs2UpdateAfterPictureDecode(struct Avs2DecContainer *dec_cont) {

  struct Avs2Storage *storage = &dec_cont->storage;

  Avs2ResetStorage(storage);

  storage->pic_started = HANTRO_FALSE;
  storage->valid_slice_in_access_unit = HANTRO_FALSE;
}

/* Checks if active SPS is valid, i.e. supported in current profile/level */
u32 Avs2SpsSupported(const struct Avs2DecContainer *dec_cont) {
  const struct Avs2SeqParam *sps = &dec_cont->storage.sps;

  if (sps->cnt == 0) return 0;

  /* check hevc main 10 profile supported or not*/
  if (((sps->sample_bit_depth != 8)) && !dec_cont->main10_support) {
    APITRACEERR("%s","AVS2 main 10 profile not supported!\n");
    return 0;
  }

  return 1;
}

/* Checks if active PPS is valid, i.e. supported in current profile/level */
u32 Avs2PpsSupported(const struct Avs2DecContainer *dec_cont) {
  return dec_cont ? 1 : 0;
}

void Avs2ResetSeqParam(struct Avs2DecContainer *dec_cont) {
  dec_cont->storage.sps.cnt = 0;
  dec_cont->storage.pps.cnt = 0;
}

/* Allocates necessary memory buffers. */
u32 Avs2AllocateResources(struct Avs2DecContainer *dec_cont) {
  u32 dwl_ret = HANTRO_OK;

#ifdef USE_FAKE_RFC_TABLE
  /* release old */
  if (dec_cont->cmems.fake_rfc_tbl.virtual_address != NULL) {
#ifdef ASIC_TRACE_SUPPORT
    DWLFreeRefFrm(dec_cont->dwl, &dec_cont->cmems.fake_rfc_tbl);
#else
    DWLFreeLinear(dec_cont->dwl, &dec_cont->cmems.fake_rfc_tbl);
#endif
    dec_cont->cmems.fake_rfc_tbl.virtual_address = NULL;
  }
  /* allocate new */
  dwl_ret = Avs2HwdAllocFakeTableMem(&dec_cont->hwdec, &dec_cont->cmems);
#endif

  return dwl_ret;
}

/* Returns the sample aspect ratio info */
void Avs2GetSarInfo(const struct Avs2Storage *storage, u32 *sar_width,
                    u32 *sar_height) {

  const struct Avs2SeqParam *sps;

  ASSERT(storage);
  sps = &storage->sps;

  if (storage->sps.cnt) {
    *sar_width = 0;
    *sar_height = 0;
  } else {
    i32 width = (storage->ext.display.cnt)
                    ? (storage->ext.display.display_horizontal_size)
                    : (sps->horizontal_size);
    i32 height = (storage->ext.display.cnt)
                     ? (storage->ext.display.display_vertical_size)
                     : (sps->vertical_size);
    switch (sps->aspect_ratio_information) {
      case 1:
        *sar_width = 1;
        *sar_height = 1;
        break;
      case 2:
        *sar_width = 4 * width;
        *sar_height = 3 * height;
        break;
      case 3:
        *sar_width = 16 * width;
        *sar_height = 9 * height;
        break;
      case 4:
        *sar_width = 221 * width / 100;
        *sar_height = height;
        break;
      case 0: /* forbiden */
      default:
        *sar_width = 0;
        *sar_height = 0;
        break;
    }
  }
}

/* Get last decoded picture if any available. No pictures are removed
 * from output nor DPB buffers. */
enum DecRet Avs2DecPeek(Avs2DecInst dec_inst, struct Avs2DecPicture *output) {
  struct Avs2DecContainer *dec_cont;// = (struct Avs2DecContainer *)dec_inst;
  struct Avs2DpbPicture *current_out;// = dec_cont->storage.dpb->current_out;

  if (dec_inst == NULL || output == NULL) {
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (struct Avs2DecContainer *)dec_inst;
  current_out = dec_cont->storage.dpb->current_out;


  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state != AVS2DEC_NEW_HEADERS &&
      dec_cont->storage.dpb->fullness && current_out != NULL &&
      current_out->status != EMPTY) {

    u32 cropping_flag;

    output->pictures[0].output_picture = current_out->data->virtual_address;
    output->pictures[0].output_picture_bus_address =
        current_out->data->bus_address;
    output->pic_id = current_out->pic_id;
    output->decode_id = current_out->decode_id;
    output->type = current_out->type;

    output->pictures[0].pic_width = Avs2PicWidth(&dec_cont->storage);
    output->pictures[0].pic_height = Avs2PicHeight(&dec_cont->storage);

    Avs2CroppingParams(&dec_cont->storage, &cropping_flag,
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
  }

  return (DEC_OK);
}

enum DecRet Avs2DecNextPictureInternal(struct Avs2DecContainer *dec_cont) {
  struct Avs2DecPicture out_pic;
  struct Avs2DecInfo dec_info;
  const struct Avs2DpbOutPicture *dpb_out = NULL;
  u32 bit_depth, out_bit_depth;
  //u32 pic_width, pic_height;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  u32 i = 0;
  u32 pic_ready;
  const struct DecHwFeatures *hw_feature = NULL;
  DWLmemset(&out_pic, 0, sizeof(struct Avs2DecPicture));
  dpb_out = Avs2NextOutputPicture(&dec_cont->storage);

  if (dpb_out == NULL) return DEC_OK;

  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->hwdec.core_mask),
                              DWL_CLIENT_TYPE_AVS2_DEC);
//  pic_width = dpb_out->pic_width;
//  pic_height = dpb_out->pic_height;
  bit_depth = dpb_out->sample_bit_depth;
  out_bit_depth = bit_depth == 8 ? 8 : 10;
  out_pic.sample_bit_depth = dpb_out->sample_bit_depth;
  out_pic.output_bit_depth = dpb_out->output_bit_depth;
  out_pic.pic_id = dpb_out->pic_id;
  out_pic.decode_id = dpb_out->decode_id;
  out_pic.type = dpb_out->type;
  out_pic.crop_params = dpb_out->crop_params;
  out_pic.cycles_per_mb = dpb_out->cycles_per_mb;
  out_pic.error_ratio = dpb_out->error_ratio;
  out_pic.error_info = dpb_out->error_info;
  out_pic.pp_enabled = 0;
  if (dec_cont->storage.pp_enabled) {
    out_pic.pp_enabled = 1;
    u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, dpb_out->pp_data);
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;
      out_pic.pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      out_pic.pictures[i].pic_stride = ppu_cfg->ystride;
      out_pic.pictures[i].pic_stride_ch = ppu_cfg->cstride;
      /*support odd crop*/
      if (hw_feature->crop_step_rshift) {
        out_pic.pictures[i].pic_width = (dec_cont->ppu_cfg[i].scale.width / 2)
                                        << 1;
        out_pic.pictures[i].pic_height = (dec_cont->ppu_cfg[i].scale.height / 2)
                                         << 1;
      } else {
        out_pic.pictures[i].pic_width = dec_cont->ppu_cfg[i].scale.width;
        out_pic.pictures[i].pic_height = dec_cont->ppu_cfg[i].scale.height;
      }
      out_pic.pictures[i].output_picture =
          (u32 *)((addr_t)dpb_out->pp_data->virtual_address +
                  ppu_cfg->luma_offset);
      out_pic.pictures[i].output_picture_bus_address =
          dpb_out->pp_data->bus_address + ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome) {
        out_pic.pictures[i].output_picture_chroma =
            (u32 *)((addr_t)dpb_out->pp_data->virtual_address +
                    ppu_cfg->chroma_offset);
        out_pic.pictures[i].output_picture_chroma_bus_address =
            dpb_out->pp_data->bus_address + ppu_cfg->chroma_offset;
      } else {
        out_pic.pictures[i].output_picture_chroma = NULL;
        out_pic.pictures[i].output_picture_chroma_bus_address = 0;
      }
      out_pic.pictures[i].top_field_first = dpb_out->top_field_first;
      out_pic.pictures[i].fields_in_picture = dpb_out->is_field_sequence==0? 2:
                                              (dpb_out->top_field_first&& dpb_out->is_top_field)? 1:
                                              (dpb_out->top_field_first==0 && dpb_out->is_top_field==0)? 1:2;
      if (!output_picture) {
        output_picture = (DWLMemAddr)out_pic.pictures[i].output_picture_bus_address;
      }
      if(ppu_cfg->dec400_enabled)
        PpFillDec400TblInfo(ppu_cfg,
                            dpb_out->pp_data->virtual_address,
                            dpb_out->pp_data->bus_address,
                            &out_pic.pictures[i].dec400_luma_table,
                            &out_pic.pictures[i].dec400_chroma_table);
    }
  } else {
    out_pic.pictures[0].output_format = dec_cont->pp_enabled;
    out_pic.pictures[0].pic_height = dpb_out->pic_height;
    out_pic.pictures[0].pic_width = dpb_out->pic_width;
    if (dec_cont->pp_enabled) {
      out_pic.pictures[0].pic_stride =
          NEXT_MULTIPLE(dpb_out->pic_width * out_bit_depth,
                        ALIGN(dec_cont->align) * 8) /
          8;
      out_pic.pictures[0].output_picture = dpb_out->pp_data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address =
          dpb_out->pp_data->bus_address;
      out_pic.pictures[0].output_picture_chroma =
          dpb_out->pp_data->virtual_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
      out_pic.pictures[0].output_picture_chroma_bus_address =
          dpb_out->pp_data->bus_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height;
    } else {
      out_pic.pictures[0].pic_stride =
          NEXT_MULTIPLE(4 * dpb_out->pic_width * bit_depth,
                        ALIGN(dec_cont->align) * 8) /
          8;
      out_pic.pictures[0].pic_stride_ch =
          NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dpb_out->pic_width, 16) * bit_depth,
                        ALIGN(dec_cont->align) * 8) /
          8;
      out_pic.pictures[0].output_picture = dpb_out->data->virtual_address;
      out_pic.pictures[0].output_picture_bus_address =
          dpb_out->data->bus_address;
      out_pic.pictures[0].output_picture_chroma =
          dpb_out->data->virtual_address + out_pic.pictures[0].pic_stride *
                                               out_pic.pictures[0].pic_height /
                                               4 / 4;
      out_pic.pictures[0].output_picture_chroma_bus_address =
          dpb_out->data->bus_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
#ifdef TILE_8x8
      out_pic.pictures[0].output_format =
          bit_depth == 8 ? (dec_cont->hwdec.sps->chroma_format != 1 ? DEC_OUT_FRM_YUV400TILE8x8 : DEC_OUT_FRM_YUV420TILE8x8) :
                           (dec_cont->hwdec.sps->chroma_format != 1 ? DEC_OUT_FRM_YUV400TILE8x8_PACK10 : DEC_OUT_FRM_YUV420TILE8x8_PACK10);

      out_pic.pictures[0].pic_stride_ch =
          NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dpb_out->pic_width, 16) * bit_depth,
                        ALIGN(dec_cont->align) * 8) / 8;

      if (dec_cont->use_video_compressor) {
        out_pic.pictures[0].pic_stride =
          NEXT_MULTIPLE(4 * dpb_out->pic_width * bit_depth,
                        ALIGN(dec_cont->align) * 8) / 8;
        out_pic.pictures[0].output_picture_chroma =
          dpb_out->data->virtual_address + out_pic.pictures[0].pic_stride *
                                               out_pic.pictures[0].pic_height /
                                               4 / 4;
        out_pic.pictures[0].output_picture_chroma_bus_address =
          dpb_out->data->bus_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / 4;
      } else {
        out_pic.pictures[0].pic_stride =
          NEXT_MULTIPLE(TILE_HEIGHT * dpb_out->pic_width * bit_depth,
                        ALIGN(dec_cont->align) * 8) /
          8;
        out_pic.pictures[0].output_picture_chroma =
          dpb_out->data->virtual_address + out_pic.pictures[0].pic_stride *
                                               out_pic.pictures[0].pic_height /
                                               4 / TILE_HEIGHT;
        out_pic.pictures[0].output_picture_chroma_bus_address =
          dpb_out->data->bus_address +
          out_pic.pictures[0].pic_stride * out_pic.pictures[0].pic_height / TILE_HEIGHT;
      }
#else
      out_pic.pictures[0].output_format =
          bit_depth == 8 ? (dec_cont->hwdec.sps->chroma_format != 1 ? DEC_OUT_FRM_YUV400TILE : DEC_OUT_FRM_YUV420TILE) :
                           (dec_cont->hwdec.sps->chroma_format != 1 ? DEC_OUT_FRM_YUV400TILE_PACK10 : DEC_OUT_FRM_YUV420TILE_PACK10);
#endif
      if (dec_cont->use_video_compressor) {
        /* No alignment when compressor is enabled. */
        out_pic.pictures[0].pic_stride = NEXT_MULTIPLE(8 * dpb_out->pic_width * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
        out_pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dpb_out->pic_width, 16) * bit_depth,
                                             ALIGN(dec_cont->align) * 8) / 8;
        out_pic.pictures[0].output_format = DEC_OUT_FRM_RFC;

        out_pic.output_rfc_luma_base = dpb_out->data->virtual_address +
                                        dec_cont->storage.dpb[0].cbs_tbl_offsety/4;
        out_pic.output_rfc_luma_bus_address = dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsety;
        out_pic.output_rfc_chroma_base = dpb_out->data->virtual_address + dec_cont->storage.dpb[0].cbs_tbl_offsetc/4;
        out_pic.output_rfc_chroma_bus_address = dpb_out->data->bus_address + dec_cont->storage.dpb[0].cbs_tbl_offsetc;
      }
    }
    ASSERT(out_pic.pictures[0].output_picture_bus_address);
  }

  (void)Avs2DecGetInfo((Avs2DecInst)dec_cont, &dec_info);  // Mark for klocwork: dec_info.video_range and dec_info.transfer_characteristics have no use here.
  (void)DWLmemcpy(&out_pic.dec_info, &dec_info, sizeof(struct Avs2DecInfo));
  out_pic.dec_info.pic_buff_size = dec_cont->storage.dpb->tot_buffers;
  pic_ready = ((!dec_cont->pp_enabled) || (dec_cont->pp_enabled &&
               (!dpb_out->is_field_sequence ||
               (dpb_out->is_field_sequence &&
               ((dpb_out->top_field_first && !dpb_out->is_top_field) ||
               (!dpb_out->top_field_first && dpb_out->is_top_field)||
               (dec_cont->storage.dpb->edit_code_flush)||
               (dec_cont->storage.dpb->flushed)||
               dpb_out->output_field==1)))));

  if (dec_cont->storage.pp_enabled && pic_ready)
    InputQueueSetBufAsUsed(dec_cont->storage.pp_buffer_queue, output_picture);
  if (pic_ready) {
    PushOutputPic(&dec_cont->fb_list, &out_pic, dpb_out->mem_idx);

  /* If reference buffer is not external, consume it and return it to DPB list.
   */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    PopOutputPic(&dec_cont->fb_list, dpb_out->mem_idx);
  } else if(!pic_ready && dec_cont->pp_enabled && dec_cont->storage.no_reordering) {
    ClearOutput(&dec_cont->fb_list, dpb_out->mem_idx);
  }
  return (DEC_PIC_RDY);
}

enum DecRet Avs2DecNextPicture(Avs2DecInst dec_inst,
                               struct Avs2DecPicture *picture) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 ret;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  if (dec_cont->dec_state == AVS2DEC_EOS && IsOutputEmpty(&dec_cont->fb_list)) {
    return (DEC_END_OF_STREAM);
  }

  if ((ret = PeekOutputPic(&dec_cont->fb_list, picture))) {
    /*Abort output fifo */
    if (ret == ABORT_MARKER) return (DEC_ABORTED);
    if (ret == FLUSH_MARKER) return (DEC_FLUSHED);
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
    if (Avs2ECDecisionOutput(dec_cont, picture) != DEC_PIC_RDY)
      return DEC_DISCARD_INTERNAL;

    return (DEC_PIC_RDY);
  } else {
    return (DEC_OK);
  }
}
/*!\brief Mark last output picture consumed
 *
 * Application calls this after it has finished processing the picture
 * returned by Avs2DecNextPicture.
 */

enum DecRet Avs2DecPictureConsumed(Avs2DecInst dec_inst,
                                   const struct Avs2DecPicture *picture) {
  u32 id, i;
  const struct Avs2DpbStorage *dpb;
  struct Avs2DecPicture pic;
  struct Avs2DecContainer *dec_cont;// = (struct Avs2DecContainer *)dec_inst;
  struct Avs2Storage *storage;// = &dec_cont->storage;
  DWLMemAddr output_picture = (DWLMemAddr)NULL;
  PpUnitIntConfig *ppu_cfg;// = dec_cont->ppu_cfg;

  if (dec_inst == NULL || picture == NULL) {
    return (DEC_PARAM_ERROR);
  }
  dec_cont = (struct Avs2DecContainer *)dec_inst;
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
      if (pic.pictures[0].output_picture_bus_address ==
              dpb->pic_buffers[id].bus_address &&
          pic.pictures[0].output_picture ==
              dpb->pic_buffers[id].virtual_address) {
        break;
      }
    }

    /* check that we have a valid id */
    if (id >= dpb->tot_buffers) return DEC_PARAM_ERROR;

    PopOutputPic(&dec_cont->fb_list, dpb->pic_buff_id[id]);

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
    if (storage->pp_enabled) {
      if (InputQueueReturnBuffer(storage->pp_buffer_queue, output_picture) == NULL)
        return DEC_PARAM_ERROR;
    }
  }

  return DEC_OK;
}


enum DecRet Avs2DecEndOfStream(Avs2DecInst dec_inst) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  i32 count;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  /* No need to call endofstream twice */
  if (dec_cont->dec_state == AVS2DEC_EOS) {
    return (DEC_OK);
  }

  if (!dec_cont->hwdec.vcmd_used && dec_cont->hwdec.n_cores > 1) {
    /* Check all Core in idle state */
    for (count = 0; count < dec_cont->hwdec.n_cores_available; count++) {
      while (dec_cont->hwdec.dec_status[count] == DEC_RUNNING) {
        sched_yield();
      }
    }
  } else {
    if (dec_cont->hwdec.asic_running) {
      /* stop HW */
      SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_IRQ_STAT, 0);
      SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_IRQ, 0);
      SetDecRegister(dec_cont->hwdec.regs, HWIF_DEC_E, 0);
      if (dec_cont->hwdec.vcmd_used) {
        DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->hwdec.cmdbuf_id);
      }
      else {
        DWLDisableHw(dec_cont->dwl, dec_cont->hwdec.core_id, 4 * 1,
                     dec_cont->hwdec.regs[1]);
        DWLReleaseHw(dec_cont->dwl, dec_cont->hwdec.core_id); /* release HW lock */
      }
      dec_cont->hwdec.asic_running = 0;
      DecrementDPBRefCount(dec_cont->storage.dpb);
    }
  }

  if (dec_cont->hwdec.vcmd_used) {
    DWLWaitCmdbufsDone(dec_cont->dwl, dec_inst);
  }

  /* flush any remaining pictures form DPB */
  Avs2FlushBuffer(&dec_cont->storage);

  FinalizeOutputAll(&dec_cont->fb_list);

  while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;

  /* After all output pictures were pushed, update decoder status to
   * reflect the end-of-stream situation. This way the Avs2DecNextPicture
   * will not block anymore once all output was handled.
   */
  dec_cont->dec_state = AVS2DEC_EOS;

  /* wake-up output thread */
  PushOutputPic(&dec_cont->fb_list, NULL, -1);

  /* block until all output is handled */
  WaitListNotInUse(&dec_cont->fb_list);

  return (DEC_OK);
}

enum DecRet Avs2DecUseExtraFrmBuffers(Avs2DecInst dec_inst, u32 n) {

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  dec_cont->storage.n_extra_frm_buffers = n;

  return DEC_OK;
}

void Avs2CycleCount(struct Avs2DecContainer *dec_cont) {
  struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;
  struct Avs2DpbPicture *current_out = dec_cont->storage.dpb->current_out;
  u32 cycles = 0;
  u32 mbs = (Avs2PicWidth(&dec_cont->storage) *
             Avs2PicHeight(&dec_cont->storage)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_cont->hwdec.regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  current_out->cycles_per_mb = cycles;

  if (dpb->no_reordering) {
    u32 index = (dpb->out_index_w == 0) ? MAX_DPB_SIZE : (dpb->out_index_w - 1);
    /* struct DpbOutPicture *dpb_out = &dpb->out_buf[(dpb->out_index_w + dpb->dpb_size) % (dpb->dpb_size + 1)]; */
    struct Avs2DpbOutPicture *dpb_out = &dpb->out_buf[index];
    dpb_out->cycles_per_mb = cycles;
  }

#ifdef FPGA_PERF_AND_BW
  u32 pic_size = Avs2PicWidth(&dec_cont->storage) * Avs2PicHeight(&dec_cont->storage);
  u32 bit_depth = Avs2SampleBitDepth(&dec_cont->storage);
  DecPerfInfoCount(dec_cont->dwl, dec_cont->hwdec.core_id, &dec_cont->perf_info, pic_size, bit_depth);
#endif
}

/* mark corrupt picture in output queue for MC */
void Avs2MCMarkOutputPicInfo(struct Avs2DecContainer *dec_cont,
                             const struct Avs2HwRdyCallbackArg *info,
                             enum DecErrorInfo error_info,
                             const u32* dec_regs) {
  const struct Avs2DpbStorage *dpb = NULL;
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
  mbs = (Avs2PicWidth(&dec_cont->storage) * Avs2PicHeight(&dec_cont->storage)) >> 8;
  if (mbs)
    cycles = GetDecRegister(dec_regs, HWIF_PERF_CYCLE_COUNT) / mbs;

  /* error_info of ctbs_num. */
  if (error_info != DEC_NO_ERROR &&
      error_info != DEC_REF_ERROR) {
    error_pic_info.error_x = GetDecRegister(dec_regs, HWIF_MB_LOCATION_X);
    error_pic_info.error_y = GetDecRegister(dec_regs, HWIF_MB_LOCATION_Y);
    error_pic_info.tile_info.num_tile_rows = 1;
    error_pic_info.tile_info.num_tile_columns = 1;
    error_pic_info.pic_width_in_ctb = info->pic_width_in_ctbs;
    error_pic_info.pic_height_in_ctb = info->pic_height_in_ctbs;
    error_pic_info.log2_ctb_size = info->lcu_size_in_bit;
    num_err_ctbs = GetErrorCtbCount(&error_pic_info);
    /* the error can be ignored. */
    if (num_err_ctbs == 0)
      error_info = DEC_NO_ERROR;
  }
  error_ratio = num_err_ctbs * EC_ROUND_COEFF /
                (info->pic_width_in_ctbs * info->pic_height_in_ctbs);

  /* check error_info of ref_list */
  for (i = 0; i < dpb->dpb_size; i++) {
    if (dpb->buffer[i].error_info != DEC_NO_ERROR) {
      for (j = 0; j < dpb->dpb_size; j++) {
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
    struct Avs2DpbOutPicture *dpb_pic = (struct Avs2DpbOutPicture *)dpb->out_buf + tmp;
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
    struct Avs2DpbPicture *dpb_pic = (struct Avs2DpbPicture *)dpb->buffer + i;
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
void Avs2SCMarkOutputPicInfo(struct Avs2DecContainer *dec_cont, u32 picture_broken) {
  struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;
  struct Avs2DpbPicture *current_out = dec_cont->storage.dpb->current_out;
  struct ErrorPicInfo error_pic_info;

  u32 i = 0;
  u32 idx = 0;
  u32 num_err_ctbs = 0; /* LCU */
  u32 error_ratio = 0;

  /* error_info of ctbs_num. */
  if (dec_cont->error_info != DEC_NO_ERROR &&
      dec_cont->error_info != DEC_REF_ERROR) {
    error_pic_info.error_x = GetDecRegister(dec_cont->hwdec.regs, HWIF_MB_LOCATION_X);
    error_pic_info.error_y = GetDecRegister(dec_cont->hwdec.regs, HWIF_MB_LOCATION_Y);
    error_pic_info.tile_info.num_tile_rows    = 1;
    error_pic_info.tile_info.num_tile_columns = 1;
    error_pic_info.pic_width_in_ctb = dec_cont->storage.sps.pic_width_in_ctbs;
    error_pic_info.pic_height_in_ctb = dec_cont->storage.sps.pic_height_in_ctbs;
    error_pic_info.log2_ctb_size = dec_cont->storage.sps.lcu_size_in_bit;
    num_err_ctbs = GetErrorCtbCount(&error_pic_info);
    /* the error can be ignored. */
    if (num_err_ctbs == 0)
      dec_cont->error_info = DEC_NO_ERROR;
  }
  if (picture_broken) /* skip HW run, 100% error. */
    error_ratio = 1 * EC_ROUND_COEFF;
  else
    error_ratio = num_err_ctbs * EC_ROUND_COEFF /
                  (dec_cont->storage.sps.pic_width_in_ctbs * dec_cont->storage.sps.pic_height_in_ctbs);

  /* (JZQ)EC Unify: for test. */
  // if (error_ratio != 0 && error_ratio != EC_ROUND_COEFF) error_ratio = 5000;

  /* check error_info of ref_list */
  for (i = 0; i < MAXREF; i++) {
    idx = dpb->ref_pic_set_st[i];
    if (IS_REFERENCE(dpb->buffer[idx]) && dpb->buffer[idx].error_info != DEC_NO_ERROR)
      dec_cont->error_info |= DEC_REF_ERROR;
  }

  /* reordering */
  current_out->error_info = dec_cont->error_info;
  current_out->error_ratio = error_ratio;

  /* no-reordering */
  if (dpb->no_reordering) {
    idx = (dpb->out_index_w == 0) ? (dpb->dpb_size - 1) : (dpb->out_index_w - 1);
    struct Avs2DpbOutPicture *dpb_out = &dpb->out_buf[idx];
    dpb_out->error_info = dec_cont->error_info;
    dpb_out->error_ratio = error_ratio;
  }
}

/* complete output pic EC policy : decision pic output */
enum DecRet Avs2ECDecisionOutput(struct Avs2DecContainer *dec_cont, struct Avs2DecPicture *output) {
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
    return Avs2DecPictureConsumed((void*)dec_cont, output);
  }

  return DEC_PIC_RDY;
}

void Avs2DropCurrentPicutre(struct Avs2DecContainer *dec_cont) {
  struct Avs2Storage *storage = &dec_cont->storage;
  if (dec_cont->storage.dpb->current_out->to_be_displayed)
    dec_cont->storage.dpb->num_out_pics_buffered--;
  if (dec_cont->storage.dpb->fullness > 0) dec_cont->storage.dpb->fullness--;
  dec_cont->storage.dpb->num_ref_frames--;
  dec_cont->storage.dpb->current_out->to_be_displayed = 0;
  dec_cont->storage.dpb->current_out->status = UNUSED;
  if (storage->pp_enabled)
    InputQueueReturnBuffer(
        storage->pp_buffer_queue,
        DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.dpb->current_out->pp_data)));

  if (dec_cont->storage.no_reordering) {
    dec_cont->storage.dpb->num_out--;
    if (dec_cont->storage.dpb->out_index_w == 0)
      dec_cont->storage.dpb->out_index_w =  dec_cont->storage.dpb->dpb_size-1;
    else
      dec_cont->storage.dpb->out_index_w--;
    ClearOutput(dec_cont->storage.dpb->fb_list,
                dec_cont->storage.dpb->current_out->mem_idx);
  }
  (void)storage;
}

enum DecRet Avs2DecAddBuffer(Avs2DecInst dec_inst, struct DWLLinearMem *info) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
//  struct Avs2AsicBuffers *asic_buff = &dec_cont->storage.cmems;
  enum DecRet dec_ret = DEC_OK;

  struct Avs2Storage *storage = &dec_cont->storage;

  if (dec_inst == NULL || info == NULL ||
      X170_CHECK_BUS_ADDRESS(info->bus_address) ||
      X170_CHECK_BUS_ADDRESS_AGLINED(info->bus_address) ||
      info->logical_size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }

  // if (dec_cont->buf_num == 0)
  //  return DEC_EXT_BUFFER_REJECTED;

  dec_cont->n_ext_buf_size = dec_cont->ext_buffer_size = info->logical_size;
  dec_cont->ext_buffers[dec_cont->buffer_num_added] = *info;
  dec_cont->buffer_num_added++;

  switch (dec_cont->buf_type) {
    case REFERENCE_BUFFER: {
      i32 i = dec_cont->buffer_index;
      u32 id;
      struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;
      if (i < MAX_PIC_BUFFERS) {
        if (DWL_OK != Avs2AllocParasiticBuf(dec_cont->dwl, dpb->dpb_parasitic_buf_size,
                          dpb->dpb_parasitic_buf + i, dec_cont->secure_mode))
          return (MEMORY_ALLOCATION_ERROR);
      }
#if 1
      if (i < dpb->tot_buffers) {
        dpb->pic_buffers[i] = *info;
        if (i < dpb->dpb_size + 1) {
          u32 id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
          if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;

          dpb->buffer[i].data = dpb->pic_buffers + i;
          dpb->buffer[i].parasitic_data = dpb->dpb_parasitic_buf + i;
          dpb->buffer[i].mem_idx = id;
          dpb->pic_buff_id[i] = id;
          dpb->buffer[i].to_be_displayed = HANTRO_FALSE;
          dpb->buffer[i].img_coi = -257;
          dpb->buffer[i].img_poi = -256;
          dpb->buffer[i].refered_by_others = 0;
          dpb->buffer[i].next_poc = 0x7FFFFFFF;
          dpb->buffer[i].first_field = 0;

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
        /* Here we need the allocate a USED id, so that this buffer will be
         * added as free buffer in SetFreePicBuffer. */
        id = AllocateIdUsed(dpb->fb_list, dpb->pic_buffers + i);
        if (id == FB_NOT_VALID_ID) return MEMORY_ALLOCATION_ERROR;
        dpb->pic_buff_id[i] = id;

        dec_cont->buffer_index++;
        dec_cont->buf_num = 0;
        /* TODO: protect this variable, which may be changed in two threads. */
        dpb->tot_buffers++;

        SetFreePicBuffer(dpb->fb_list, id);
      }
#else
      InputQueueAddBuffer(dec_cont->in_buffers, info);
#endif

      if (dec_cont->buffer_index < dpb->tot_buffers)
        dec_ret = DEC_WAITING_FOR_BUFFER;

      /* Since only one type of buffer will be set as external, we don't need to
       * switch to adding next buffer type. */
      /*
      else {
        dec_cont->next_buf_size = 0;
        dec_cont->buffer_index = 0;
        dec_cont->buf_num = 0;
      #ifdef ASIC_TRACE_SUPPORT
        dec_cont->is_frame_buffer = 0;
      #endif
      }
      */
      break;
    }
    case DOWNSCALE_OUT_BUFFER: {

      ASSERT(storage->pp_enabled);
      ASSERT(dec_cont->buffer_index < dec_cont->min_buffer_num);

      InputQueueAddBuffer(storage->pp_buffer_queue, info);

      dec_cont->buffer_index++;

      if (dec_cont->buffer_index == dec_cont->min_buffer_num) {
        dec_cont->next_buf_size = 0;
        dec_cont->buffer_index = 0;
      } else {
        dec_ret = DEC_WAITING_FOR_BUFFER;
      }
      break;
    }
    default:
      break;
  }

  return dec_ret;
}

enum DecRet Avs2DecGetBufferInfo(Avs2DecInst dec_inst,
                                 struct DecBufferInfo *mem_info) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  //  enum DecRet dec_ret = DEC_OK;
  struct DWLLinearMem empty = {0};
  u32 i;
  struct DWLLinearMem *buffer = NULL;

  if (dec_inst == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    struct Avs2Storage *storage = &dec_cont->storage;
    u32 bit_depth = storage->sps.sample_bit_depth;
    if (dec_cont->use_video_compressor) {
      mem_info->ystride[0] = NEXT_MULTIPLE(8 * storage->sps.pic_width * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
      mem_info->cstride[0] = NEXT_MULTIPLE(4 * storage->sps.pic_width * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
    } else {
      mem_info->cstride[0] = mem_info->ystride[0] =
                             NEXT_MULTIPLE(4 * storage->sps.pic_width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
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
    if (dec_cont->buffer_num_added) {
      buffer = &dec_cont->ext_buffers[dec_cont->buffer_num_added - 1];
      dec_cont->buffer_num_added--;
    }
    if (buffer == NULL) {
        /* All buffers have been released. */
      dec_cont->release_buffer = 0;
      InputQueueRelease(dec_cont->pp_buffer_queue);
      dec_cont->pp_buffer_queue = InputQueueInit(0);
      if (dec_cont->pp_buffer_queue == NULL)
        return (DEC_MEMFAIL);
      dec_cont->storage.pp_buffer_queue = dec_cont->pp_buffer_queue;

      mem_info->buf_to_free = empty;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      if (!dec_cont->pp_enabled)
        return (DEC_OK);
    } else {
      mem_info->buf_to_free = *buffer;
      mem_info->next_buf_size = 0;
      mem_info->buf_num = 0;
      return DEC_WAITING_FOR_BUFFER;
    }
  }

  /* Called after Avs2DecDecode returns DEC_WAITING_FOR_BUFFER, set required buf num and size. */
  if(dec_cont->next_buf_size == 0) {
    /* External reference buffer: release done. */
    mem_info->buf_to_free = empty;
    mem_info->next_buf_size = dec_cont->next_buf_size;
    mem_info->buf_num = dec_cont->buf_num + dec_cont->guard_size;
    return DEC_OK;
  }

  mem_info->buf_to_free = empty;
  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num;
  //if (mem_info->buf_num == 1) return DEC_OK;
  ASSERT((mem_info->buf_num && mem_info->next_buf_size) ||
         (mem_info->buf_to_free.virtual_address != NULL));

  return DEC_WAITING_FOR_BUFFER;
}

void Avs2EnterAbortState(struct Avs2DecContainer *dec_cont) {
  SetAbortStatusInList(&dec_cont->fb_list);
  InputQueueSetAbort(dec_cont->storage.pp_buffer_queue);
  dec_cont->abort = 1;
}

void Avs2ExistAbortState(struct Avs2DecContainer *dec_cont) {
  ClearAbortStatusInList(&dec_cont->fb_list);
  InputQueueClearAbort(dec_cont->storage.pp_buffer_queue);
  dec_cont->abort = 0;
}

u32 Avs2ReplaceRefAvalible(struct Avs2DecContainer *dec_cont) {
  u32 i = 0;
  u32 idx = 0;
  i32 old_idx = EC_INVALID_IDX;
  struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;

  u32 curr_pos = dec_cont->storage.dpb->current_out_pos;
  u32 frame_error_num = 0;

  /* ref_buffer error info count num */
  for (i = 0; i < MAXREF; i++) {
    idx = dpb->ref_pic_set_st[i];

    /* skip repeat index. */
    if (idx == old_idx) continue;
    old_idx = idx;

    if (IS_REFERENCE(dpb->buffer[idx])) {
      /* skip self. */
      if (idx == curr_pos)
        continue;

      if (dpb->buffer[idx].error_info == DEC_NO_ERROR)
        return HANTRO_OK;
      else if (dpb->buffer[idx].error_info == DEC_REF_ERROR)
        return HANTRO_OK;
      else {
        // /* if error_ratio less than 10%, don't replace it, use it as correct. */
        // if (dpb->buffer[idx].error_ratio <= EC_RATIO_THRESHOLD)
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

enum DecRet Avs2DecAbort(Avs2DecInst dec_inst) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);
  /* Abort frame buffer waiting and rs/ds buffer waiting */
  Avs2EnterAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return (DEC_OK);
}

enum DecRet Avs2DecAbortAfter(Avs2DecInst dec_inst) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return (DEC_PARAM_ERROR);
  }
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If normal EOS is waited, return directly */
  if(dec_cont->dec_state == AVS2DEC_EOS) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return (DEC_OK);
  }
#endif

  if (dec_cont->hwdec.asic_running && !dec_cont->hwdec.b_mc) {
    /* stop HW */
    u32 core_id = (dec_cont->hwdec.vcmd_used) ? dec_cont->hwdec.cmdbuf_id : dec_cont->hwdec.core_id;
    SwAbortSliceDec(dec_cont->dwl, dec_inst, dec_cont->hwdec.regs, dec_cont->hwdec.vcmd_used, core_id);

    DecrementDPBRefCount(dec_cont->storage.dpb);
    dec_cont->hwdec.asic_running = 0;
  }

  /* In multi-Core senario, wait hw ready is executed through listener thread,
     here to check whether HW is finished */
  if(dec_cont->hwdec.b_mc) {
    SwAbortMCDec(dec_cont->dwl, dec_inst, dec_cont->hwdec.vcmd_used,
                dec_cont->hwdec.n_cores_available, NULL);
  }

  /* Clear any remaining pictures and internal parameters in DPB */
  Avs2EmptyDpb(dec_cont, dec_cont->storage.dpb);

  /* Clear any internal parameters in storage */
  Avs2ClearStorage(&(dec_cont->storage));

  /* Clear internal parameters in Avs2DecContainer */
  dec_cont->dec_state = AVS2DEC_INITIALIZED;
  dec_cont->start_code_detected = 0;
  dec_cont->pic_number = 0;
  dec_cont->packet_decoded = 0;

#ifdef USE_OMXIL_BUFFER
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size +
                               2; /* We need at least (dpb_size+2) output
                                     buffers before starting decoding. */
  else
    dec_cont->min_buffer_num = dec_cont->storage.dpb->dpb_size + 1;
  dec_cont->buffer_index = 0;
  dec_cont->buf_num = dec_cont->min_buffer_num;
  dec_cont->buffer_num_added = 0;
#endif

  Avs2ExistAbortState(dec_cont);

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

void Avs2DecSetNoReorder(Avs2DecInst dec_inst, u32 no_reorder) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  dec_cont->storage.no_reordering = no_reorder;
  dec_cont->storage.dpb->no_reordering = no_reorder;
}

enum DecRet Avs2DecSetInfo(Avs2DecInst dec_inst,
                           struct Avs2DecConfig *dec_cfg) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 pic_width = Avs2PicWidth(&dec_cont->storage);
  u32 pic_height = Avs2PicHeight(&dec_cont->storage);
  const struct DecHwFeatures *hw_feature = NULL;
  u32 i;
  PpUnitConfig *ppu_cfg = &dec_cfg->ppu_cfg[0];
  u32 pixel_width = Avs2SampleBitDepth(&dec_cont->storage);

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->hwdec.core_mask);
    dec_cont->hwdec.core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->hwdec.core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->hwdec.core_mask),
                              DWL_CLIENT_TYPE_AVS2_DEC);
  if (!hw_feature) {
    APITRACEDEBUG("%s","Avs2DecSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
  }

  if (dec_cont->storage.sps.is_field_sequence) pic_height *= 2;

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return (DEC_NOT_INITIALIZED);
  }

  /* ref aligment */
  dec_cont->align = dec_cont->hwdec.align = dec_cont->storage.align = dec_cfg->align;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    /* ppu alignment */
    dec_cfg->ppu_cfg[i].align = dec_cfg->align;
    /* 3dlut */
    if (dec_cfg->ppu_cfg[i].enable_3dlut && (dec_cfg->ppu_cfg[i].rgb || dec_cfg->ppu_cfg[i].rgb_planar)) {
      dec_cont->enable_3dlut = 1;
    }
    dec_cont->ppu_cfg[i].table_3dlut_buffer = dec_cfg->table_3dlut_buffer;
  }

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg, hw_feature, pixel_width,
                     !dec_cont->storage.sps.is_field_sequence, 0);

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;

  dec_cont->storage.pp_enabled = dec_cont->hwdec.pp_enabled = dec_cont->pp_enabled;

  for (u32 i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL) {
      u32 size = LANCZOS_COEFF_BUFFER_SIZE * sizeof(i16) * dec_cont->hwdec.n_cores;
      dec_cont->ppu_cfg[i].lanczos_table.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(dec_cont->ppu_cfg[i].lanczos_table.mem_type,
                    DWL_MEM_USAGE_IN_PPULANCZOS_TABLE, dec_cont->secure_mode);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(hw_feature, pic_width, pic_height,
                        dec_cont->storage.sps.is_field_sequence, pixel_width,
                        dec_cont->storage.sps.chroma_format, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  /* config error process policy. */
  if (dec_cont->error_handling == DEC_EC_FRAME_TOLERANT_ERROR) {
    if (dec_cont->hwdec.b_mc)
      dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_ERROR_IGNORE | DEC_EC_NO_SKIP | DEC_EC_OUT_DECISION;
    else
      dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE | DEC_EC_NO_SKIP | DEC_EC_OUT_DECISION;
  } else if (dec_cont->error_handling == DEC_EC_FRAME_IGNORE_ERROR) {
    if (dec_cont->hwdec.b_mc)
      dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_ERROR_IGNORE | DEC_EC_NO_SKIP | DEC_EC_OUT_ALL;
    else
      dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE_ANYWAY | DEC_EC_NO_SKIP | DEC_EC_OUT_ALL;
  } else {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_RESET | DEC_EC_SEEK_NEXT_I | DEC_EC_OUT_NO_ERROR;
  }
  dec_cont->hwdec.error_policy = dec_cont->error_policy;
  if (dec_cont->hwdec.b_mc && dec_cont->error_handling) {
    if (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I ||
        dec_cont->error_policy & DEC_EC_REF_RESET ||
        dec_cont->error_policy & DEC_EC_REF_REPLACE ||
        dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY) {
      APITRACEERR("%s","DON'T SUPPORT THIS ERROR POLICY IN MC MODE.\n");
      return DEC_PARAM_ERROR;
    }
  }

  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config |= 1 << DOWNSCALE_OUT_BUFFER;
  else
    dec_cont->ext_buffer_config = 1 << REFERENCE_BUFFER;
  Avs2SetExternalBufferInfo(dec_cont, &(dec_cont->storage));

  return (DEC_OK);
}


void Avs2MCPushOutputAll(struct Avs2DecContainer *dec_cont) {
  while (Avs2DecNextPictureInternal(dec_cont) == DEC_PIC_RDY)
    ;
}

void Avs2MCWaitOutFifoEmpty(struct Avs2DecContainer *dec_cont) {
  WaitOutputEmpty(&dec_cont->fb_list);
}

void Avs2MCWaitPicReadyAll(struct Avs2DecContainer *dec_cont) {
  WaitListNotInUse(&dec_cont->fb_list);
}

void Avs2MCSetRefPicStatus(const void *dwl_inst, struct DWLLinearMem *p_dmv_out) {
  /* frame status */
  DWLLinearMemset(dwl_inst, p_dmv_out,  -32, 0xFF, 32);
}

static u32 MCGetRefPicStatus(const u8 *p_sync_mem) {
  u32 ret;

  /* frame status */
  ret = ( p_sync_mem[1] << 8) + p_sync_mem[0];
  return ret;
}


static void MCValidateRefPicStatus(struct Avs2DecContainer *dec_cont,
                                   const u32 *hevc_regs,
                                   struct Avs2HwRdyCallbackArg *info) {
  u8* p_ref_stat;
  struct DWLLinearMem *p_dmv_out;
  // struct Avs2DpbStorage *dpb = info->current_dpb;
  u32 status, expected;
  av_unused addr_t device_bus_addr = 0;
  av_unused u8 sync_buf[32];

  p_dmv_out = &info->dmv;

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
    Avs2MCSetRefPicStatus(dec_cont->dwl, p_dmv_out);
  }
}

/* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
void Avs2MCHwRdyCallback(void *args, i32 core_id) {
  u32 dec_regs[DEC_X170_REGISTERS];

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)args;
  struct Avs2HwRdyCallbackArg *info;

  const void *dwl;
  const struct Avs2DpbStorage *dpb;

  u32 core_status, type;
  u32 i, cmdbuf_id = 0xFFFFFF;
  struct DWLLinearMem *p_out, *p_dmv_out;
  enum DecErrorInfo error_info = DEC_NO_ERROR;

#ifdef FPGA_PERF_AND_BW
  u32 pic_size = Avs2PicWidth(&dec_cont->storage) * Avs2PicHeight(&dec_cont->storage);
#endif
  ASSERT(dec_cont != NULL);
  ASSERT(core_id < MAX_ASIC_CORES || (dec_cont->hwdec.vcmd_used && core_id < MAX_MC_CB_ENTRIES));

  dwl = dec_cont->dwl;
  if(dec_cont->hwdec.vcmd_used) {
    cmdbuf_id = core_id;
    core_id = DWLGetVcmdMCVirtualCoreId(dwl, cmdbuf_id);
  }

  /* take a copy of the args as after we release the HW they
   * can be overwritten.
   */
  info = dec_cont->hwdec.hw_rdy_callback_arg[core_id];
  dpb = info->current_dpb;

  /* read all hw regs */
  if(dec_cont->hwdec.vcmd_used) {
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
  p_dmv_out = (struct DWLLinearMem *)&info->dmv;

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
      if (!dec_cont->hwdec.vcmd_used)
        DWLDisableHw(dwl, core_id, 0x04, dec_regs[1]);
    }
    /* reset DMV storage for erroneous pictures */
    {
      u32 dmv_mem_size = p_dmv_out->size;
      DWLLinearMemset(dec_cont->dwl, p_dmv_out, 0, 0, dmv_mem_size);
    }

    Avs2MCSetRefPicStatus(dec_cont->dwl, p_dmv_out);
  } else {
    // if (dec_cont->hw_conceal)
    //   error_info = GetDecRegister(dec_regs, HWIF_ERROR_INFO);
    // else
    error_info = DEC_NO_ERROR;
    /* if there is no dpb date, do not call MCValidateRefPicStatus */
    if (GetDecRegister(dec_regs, HWIF_DEC_OUT_DIS) == 0)
      MCValidateRefPicStatus(dec_cont, dec_regs, info);
  }
  /* mark corrupt picture in output queue */
#ifdef FPGA_PERF_AND_BW
  DecPerfInfoCount(dec_cont->dwl, core_id, &dec_cont->perf_info, pic_size,
                   dec_cont->storage.sps.sample_bit_depth);
#endif
  Avs2MCMarkOutputPicInfo(dec_cont, info, error_info, dec_regs);
  // Avs2DisableDMVBuffer((struct Avs2DpbStorage *)dpb, core_id);

  /* release the stream buffer. Callback provided by app */
  if(dec_cont->hwdec.stream_consumed_callback.fn)
    dec_cont->hwdec.stream_consumed_callback.fn((u8*)info->stream,
                                                (void*)info->p_user_data);

  type = FB_HW_OUT_FRAME;
  ClearHWOutput(dpb->fb_list, info->out_id, type, dec_cont->pp_enabled);

  /* decrement buffer usage in our buffer handling */
  DecrementDPBRefCountExt((struct Avs2DpbStorage *)dpb, info->ref_id);
  /* clear IRQ status reg and release HW core */
  if (dec_cont->hwdec.vcmd_used) {
    DWLReleaseCmdBuf(dwl, cmdbuf_id);
    // if (dec_cont->hwdec.b_mc)
      FifoPush(dec_cont->hwdec.fifo_core, (FifoObject)(addr_t)core_id, FIFO_EXCEPTION_DISABLE);
  } else {
    dec_cont->hwdec.dec_status[core_id] = DEC_IDLE;  /* set dec status to idle */
    DWLReleaseHw(dwl, core_id);
  }
}

void Avs2MCSetHwRdyCallback(struct Avs2DecContainer *dec_cont) {
  struct Avs2DpbStorage *dpb = dec_cont->storage.dpb;
  u32 type, i;

  u32 core_id = dec_cont->hwdec.vcmd_used ? dec_cont->hwdec.mc_buf_id : dec_cont->hwdec.core_id;
  struct Avs2HwRdyCallbackArg *arg = dec_cont->hwdec.hw_rdy_callback_arg[core_id];

  if (arg == NULL) {
    dec_cont->hwdec.hw_rdy_callback_arg[core_id] = DWLmalloc(sizeof(struct Avs2HwRdyCallbackArg));
    arg = dec_cont->hwdec.hw_rdy_callback_arg[core_id];
  }
  arg->stream = dec_cont->hwdec.stream_consumed_callback.p_strm_buff;
  arg->p_user_data = dec_cont->hwdec.stream_consumed_callback.p_user_data;
  arg->out_id = dpb->current_out->mem_idx;
  if (dec_cont->pp_enabled)
    arg->pp_data = dpb->current_out->pp_data;
  arg->current_dpb = dpb;
  arg->dmv = dec_cont->hwdec.recon->mv;
  arg->pic_height_in_ctbs = dec_cont->hwdec.sps->pic_height_in_ctbs;
  arg->pic_width_in_ctbs = dec_cont->hwdec.sps->pic_width_in_ctbs;
  arg->lcu_size_in_bit = dec_cont->hwdec.sps->lcu_size_in_bit;

  /* don't work */
  for (i = 0; i < dpb->dpb_size; i++) {
    arg->ref_id[i] = dpb->ref_id[i];
  }

  /* storage reference mem_idx */
  for (i = 0; i < dpb->dpb_size; i++) {
    if (IS_REFERENCE(dpb->buffer[i]))
      arg->ref_mem_idx[i] = dpb->buffer[i].mem_idx;
    else
      arg->ref_mem_idx[i] = -1;
  }

  core_id = dec_cont->hwdec.vcmd_used ? dec_cont->hwdec.cmdbuf_id : core_id;
  DWLSetIRQCallback(dec_cont->dwl, core_id, Avs2MCHwRdyCallback, dec_cont);

  type = FB_HW_OUT_FRAME;

  MarkHWOutput(&dec_cont->fb_list, dpb->current_out->mem_idx, type);
}

#ifdef RANDOM_CORRUPT_RFC
/* Allocates SW resources after parameter set activation. */
extern void Avs2GetRefFrmSize(struct Avs2DecContainer *dec_cont,
                              u32 *luma_size, u32 *chroma_size,
                              u32 *rfc_luma_size, u32 *rfc_chroma_size);

u32 Avs2CorruptRFC(struct Avs2DecContainer *dec_cont) {
  u32 luma_size, chroma_size, rfc_luma_size, rfc_chroma_size;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  struct Avs2Storage storage = dec_cont->storage;
  struct Avs2DpbStorage *dpb = storage.dpb;

  Avs2GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                      &rfc_luma_size, &rfc_chroma_size);

  /* output buffer is not ringbuffer, just send zero/null parameters*/
  if (RandomizeBitSwapInStream((u8 *)storage.curr_image->data->virtual_address,
                               NULL, 0,
                               NEXT_MULTIPLE(luma_size, ref_buffer_align) +
                                 NEXT_MULTIPLE(chroma_size, ref_buffer_align),
                               dec_cont->error_params.swap_bit_odds, 0)) {
    APITRACEERR("%s","Bitswap reference buffer corruption error (wrong config?)\n");
  }
  /* output buffer is not ringbuffer, just send zero/null parameters*/
  if (RandomizeBitSwapInStream((u8 *)storage.curr_image->data->virtual_address +
                                dpb->cbs_tbl_offsety, NULL, 0,
                               NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align) +
                                 NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align),
                               dec_cont->error_params.swap_bit_odds, 0)) {
    APITRACEERR("%s","Bitswap RFC table corruption error (wrong config?)\n");
  }
  return 0;
}
#endif

void Avs2DecUpdateStrmInfoCtrl(Avs2DecInst dec_inst, struct strmInfo info) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  static u32 len_update = 1;
  struct LLStrmInfo *llstrminfo = &dec_cont->llstrminfo;
  dec_cont->llstrminfo.stream_info = info;

  if (dec_cont->llstrminfo.update_reg_flag) {
    /* wait for hw ready if it's the first time to update length register */
    if (dec_cont->llstrminfo.first_update) {
      while (!dec_cont->asic_running)
        sched_yield();
      dec_cont->llstrminfo.first_update = 0;
      dec_cont->llstrminfo.ll_strm_len = 0;
      len_update = 1;
    }

    SwUpdateStrmInfoCtrl(llstrminfo, &len_update);

    /* check hw status */
    if (!dec_cont->llstrminfo.first_update && !dec_cont->asic_running) {
      dec_cont->llstrminfo.tmp_length = dec_cont->llstrminfo.ll_strm_len;
      dec_cont->llstrminfo.update_reg_flag = 0;
      dec_cont->llstrminfo.updated_reg = 1;
    }
  }
}

#ifdef CASE_INFO_STAT
void Avs2CaseInfoCollect(struct Avs2DecContainer *dec_cont, CaseInfo *case_info) {
  struct Avs2Storage *storage = &dec_cont->storage;
  u32 crop_width, crop_height;
  u32 pic_width, pic_height;
  u32 bit_depth;
  u32 raster_stride;

  u32 monochrome = Avs2IsMonoChrome(storage);
  pic_width = Avs2PicWidth(storage);
  pic_height = Avs2PicHeight(storage);
  bit_depth = Avs2SampleBitDepth(storage);

  case_info->min_cb_size = 3;
  case_info->max_cb_size = dec_cont->storage.sps.lcu_size_in_bit;
  case_info->pic_width_in_cbs =  storage->sps.pic_width_in_cbs;
  case_info->pic_height_in_cbs = storage->sps.pic_height_in_cbs;

  if ((storage->sps.horizontal_size != storage->sps.pic_width_in_cbs * 8) ||
      (storage->sps.vertical_size != storage->sps.pic_height_in_cbs * 8)) {
    case_info->crop_flag = 1;
    crop_width = storage->sps.horizontal_size;
    crop_height = storage->sps.vertical_size;
  } else {
    crop_width = pic_width;
    crop_height = pic_height;
  }

  case_info->interlace_flag = case_info->interlace_flag == 1 ? 1 : dec_cont->storage.sps.is_field_sequence;
  pic_height = pic_height << dec_cont->storage.sps.is_field_sequence;
  crop_height = crop_height << dec_cont->storage.sps.is_field_sequence;

  if(bit_depth == 8)
    raster_stride = NEXT_MULTIPLE(crop_width, 16);
  else
    raster_stride = NEXT_MULTIPLE(crop_width * 2, 16);

  if (dec_cont->pic_number == 0) {
    case_info->decode_width = pic_width;
    case_info->decode_height = pic_height;
    case_info->display_height = crop_height;
    case_info->display_width = crop_width;
    case_info->bit_depth = bit_depth;
    if(monochrome)
      case_info->chroma_format_id = 0;
    else
      case_info->chroma_format_id = 1;
    case_info->crop_x = case_info->crop_y = 0;
  } else {
    if (case_info->decode_width !=  pic_width
    || case_info->decode_height != pic_height) {
      case_info->decode_width = MAX (pic_width, case_info->decode_width);
      case_info->decode_height = MAX (pic_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    case_info->display_height = MIN (crop_height, case_info->display_height);
    case_info->display_width = MIN (crop_width, case_info->display_width);
    if (case_info->bit_depth != bit_depth) {
      case_info->bit_depth = MAX (bit_depth, case_info->bit_depth);
      case_info->depth_flag = 1;
    }
    if ((case_info->chroma_format_id && monochrome)||( !case_info->chroma_format_id && !monochrome))
      case_info->chroma_flag = 1;
  }
  if(case_info->frame_num < 10) {
    switch (storage->pps.type) {
      case F_IMG: case_info->frame_type[dec_cont->pic_number] = F_FRAME; break;
      case B_IMG: case_info->frame_type[dec_cont->pic_number] = B_FRAME; break;
      case I_IMG:
        if (storage->pps.typeb == BACKGROUND_IMG && storage->sps.background_picture_enable)
          if (storage->pps.background_picture_output_flag)
            case_info->frame_type[dec_cont->pic_number] = G_FRAME;
          else
            case_info->frame_type[dec_cont->pic_number] = GB_FRAME;
        else
          case_info->frame_type[dec_cont->pic_number]  = I_FRAME;
        break;
      case P_IMG:
        if (storage->pps.typeb == BP_IMG && storage->sps.background_picture_enable)
          case_info->frame_type[dec_cont->pic_number] = S_FRAME;
        else
          case_info->frame_type[dec_cont->pic_number]  = P_FRAME;
      break;
    }
    case_info->bit_depth_y_minus8[dec_cont->pic_number] = storage->sps.sample_bit_depth - 8;
    case_info->bit_depth_c_minus8[dec_cont->pic_number] = storage->sps.sample_bit_depth - 8;
    case_info->blackwhite_e[dec_cont->pic_number] = monochrome == 1 ? 1 : 0;
    case_info->ppin_luma_size[dec_cont->pic_number] = crop_height * raster_stride;
    case_info->slice_num[case_info->frame_num]++;
  }
  case_info->codec = DEC_MODE_AVS2;
  case_info->frame_num++;
  case_info->bitrate += ((60 * GetDecRegister(dec_cont->hwdec.regs, HWIF_STREAM_LEN) * 8/ NEXT_MULTIPLE(Avs2PicHeight(storage), 16))
                          * 3840/ NEXT_MULTIPLE(Avs2PicWidth(storage), 16)) * 2160/ 1024/ 1024;

}
#endif
