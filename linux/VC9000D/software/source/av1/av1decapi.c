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

#include "av1decapi.h"
#include "basetype.h"
#include "commonconfig.h"
#include "commonfunction.h"
#include "decapicommon.h"
#include "dwl.h"
#include "fifo.h"
#include "version.h"

#include "av1_obu.h"
#include "av1_metadata.h"
#include "av1hwd_asic.h"
#include "av1hwd_container.h"
#include "av1hwd_headers.h"
#include "av1hwd_output.h"
#include "dectypes.h"
#include "sw_stream.h"
#include "sw_util.h"
#include "dec_log.h"
#include "aom_defines.h"
#include "errorhandling.h"
#include "regdrv.h"

#ifdef MODEL_SIMULATION
#include "asic.h"
#endif
#define AV1_MIN_EXT_BUFFERS 8

#ifndef UINT32_MAX
#define UINT32_MAX 0xFFFFFFFF
#endif

#define MAX_ACTIVE_REFS AV1_ACTIVE_REFS_EX

/* Max amount of stream */
#define DEC_MAX_STREAM ((1 << 30) - 1)
#define DEC_CHECK_BUS_ADDRESS(d) ((d) < 64 ? 1 : 0)
#define DEC_CHECK_VIRTUAL_ADDRESS(d) (((void *)(d) < (void *)64) ? 1 : 0)
#define DOWN_SCALE_SIZE(w, ds) (((w) / (ds)) & ~0x1)

static u32 Av1CheckSupport(struct Av1DecContainer *dec_cont);
static void Av1Freeze(struct Av1DecContainer *dec_cont);
static i32 Av1DecodeHeaders(struct Av1DecContainer *dec_cont,
                            const struct Av1DecInput *input);
static i32 Av1AsicAllocateFakeTableMem(struct Av1DecContainer *dec_cont);
static u32 Av1ReplaceRefAvalible(struct Av1DecContainer *dec_cont);
void AV1SetDefaultCDFs(struct Av1Decoder *x);

#ifdef CASE_INFO_STAT
#include "case_info.h"
static void Av1CaseInfoCollect(struct Av1DecContainer *dec_cont, CaseInfo *case_info);
#endif

enum DecRet Av1DecInit(Av1DecInst *dec_inst, const void *dwl,
                       struct Av1DecConfig *dec_cfg) {
  struct Av1DecContainer *dec_cont;
  struct DWLCodecConfig codec_cfg;
  u32 core_mask = 0, core_mask_rfc, i;
  enum DecRet ret;

/* check that right shift on negative numbers is performed signed */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif

  if (dec_inst == NULL || dwl == NULL || dec_cfg == NULL)
    return DEC_PARAM_ERROR;

  *dec_inst = NULL; /* return NULL instance for any error */
  /* check that decoding supported in HW */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_AV1_DEC, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","Av1DecInit# Av1 not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  if ((dec_cfg->decoder_mode & DEC_LOW_LATENCY)) {
    if (!SwGetCoreMaskByFeature(dwl, VSI_LOW_LATENCY)) {
      APITRACEERR("%s","Av1DecInit# Av1 low latency not supported in HW\n");
      return DEC_PARAM_ERROR;
    }
  }

  /* allocate instance */
  dec_cont = (struct Av1DecContainer *)DWLcalloc(1, sizeof(struct Av1DecContainer));
  if (dec_cont == NULL) {
    APITRACEERR("%s","Av1DecInit# Memory allocation failed\n");
    return (DEC_MEMFAIL);
  }
  dec_cont->dwl = dwl;
  dec_cont->n_cores = DWLReadAsicCoreCount(dwl);
  dec_cont->core_mask = core_mask;
  dec_cont->secure_mode = (dec_cfg->decoder_mode & DEC_SECURITY) ? 1 : 0;
  SET_CLIENT_TYPE(dec_cont->core_mask, DWL_CLIENT_TYPE_AV1_DEC);
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
      APITRACEDEBUG("Av1DecInit# not any core to support RFC from core_mask 0x%x, disable it\n",
                     dec_cont->core_mask);
    }
  }
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                        DWL_CLIENT_TYPE_AV1_DEC);

  if (dec_cfg->mc_enable && dec_cont->n_cores_available > 1) {
    dec_cont->use_multicore = 1;
    dec_cont->use_multicore_backup = 1;
    /* should config when multi */
    // SetDecRegister(dec_cont->av1_regs[0], HWIF_DEC_MULTICORE_E, 1);
    // SetDecRegister(dec_cont->av1_regs[0], HWIF_DEC_WRITESTAT_E, 1);
  }

#ifdef SUPPORT_VCMD_M2M
  /* Init array used to save CDFs info  */
  dec_cont->vcmd_m2m = DWLCheckVcmdM2M(dec_cont->dwl);/* check if hw support m2m */
  if (dec_cont->vcmd_m2m)
    DWLmemset(&dec_cont->decoder.cdfs_info, 0, sizeof(struct CDFsInfo));
#endif

  /* initial setup of instance */
  dec_cont->av1_regs[0][0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_AV1_DEC);
  dec_cont->dec_stat = AV1DEC_INITIALIZED;
  dec_cont->checksum = dec_cont; /* save instance as a checksum */
  // dec_cont->pp = pp_cfg;
  dec_cont->host_accessible_frames = TRUE;

  if (dec_cfg->num_frame_buffers > MAX_PIC_BUFFERS)
    dec_cfg->num_frame_buffers = MAX_PIC_BUFFERS;

  dec_cont->num_buffers = dec_cfg->num_frame_buffers;

  if ((dec_cfg->decoder_mode & DEC_HEIF) &&
      (dec_cfg->decoder_mode & DEC_INTRA_ONLY)) {
    dec_cont->skip_no_intra = 1;
    dec_cont->num_buffers = dec_cfg->num_frame_buffers = 1;
  }
  Av1AsicInit(&codec_cfg, dec_cont, dec_cfg->multicore_poll_period);


  dec_cont->pic_number = dec_cont->display_number = 1;
  dec_cont->error_info = DEC_NO_ERROR;
  dec_cont->error_ratio = dec_cfg->error_ratio;
  dec_cont->error_handling = dec_cfg->error_handling;
  dec_cont->error_policy = HANTRO_FALSE;
  dec_cont->picture_broken = HANTRO_FALSE;
  dec_cont->decoder.refbu_pred_hits = 0;


  /* Low latency: tile_transpose need set as 0 in low latency mode */
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
    SetDecRegister(dec_cont->av1_regs[0], HWIF_STREAM_STATUS_EXT_BUFFER_E, 1);
    SetDecRegister(dec_cont->av1_regs[0], HWIF_BUFFER_EMPTY_INT_E, 0);
    SetDecRegister(dec_cont->av1_regs[0], HWIF_BLOCK_BUFFER_MODE_E, 1);
    dec_cont->decoder.tile_transpose = 0;
    SetDecRegister(dec_cont->av1_regs[0], HWIF_DEC_MC_POLLMODE, 0);
    SetDecRegister(dec_cont->av1_regs[0], HWIF_DEC_MC_POLLTIME, 0);
    dec_cont->llstrminfo.strm_status_in_buffer = 1;
  } else {
    SetDecRegister(dec_cont->av1_regs[0], HWIF_BUFFER_EMPTY_INT_E, 1);
    SetDecRegister(dec_cont->av1_regs[0], HWIF_BLOCK_BUFFER_MODE_E, 0);
  }

  if ((Av1AsicAllocateMem(dec_cont->dwl, dec_cont->asic_buff, dec_cont->secure_mode) != 0) ||
      (Av1AllocatLowLatencyMem(dec_cont) != 0)) {
    return DEC_MEMFAIL;
  }
  /* low latency: strm_status_addr point to status buffer */
  if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
    dec_cont->llstrminfo.strm_status_addr = dec_cont->llstrminfo.strm_status.virtual_address;
    dec_cont->lltileinfo.tile_status_addr = ((u32 *)dec_cont->llstrminfo.strm_status.virtual_address + 2);
  }

  dec_cont->decoder.default_cdfs = (struct AV1CDFs *)dec_cont->asic_buff->default_cdfs_mem.virtual_address;
  dec_cont->decoder.default_cdfs_addr = dec_cont->asic_buff->default_cdfs_mem.bus_address;
  dec_cont->decoder.default_cdfs_ndvc = (struct MvCDFs *)dec_cont->asic_buff->default_cdfs_ndvc_mem.virtual_address;
  dec_cont->decoder.default_cdfs_ndvc_addr = dec_cont->asic_buff->default_cdfs_ndvc_mem.bus_address;
  dec_cont->decoder.cdfs_last = (struct AV1CDFs *)dec_cont->asic_buff->cdfs_last_mem.virtual_address;
  dec_cont->decoder.cdfs_last_addr = dec_cont->asic_buff->cdfs_last_mem.bus_address;

  AV1SetDefaultCDFs(&dec_cont->decoder);

  if (FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_out) != FIFO_OK)
    return DEC_MEMFAIL;

  if (FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_display) != FIFO_OK) {
    ret = DEC_MEMFAIL;
    goto err0;
  }
  if (pthread_mutex_init(&dec_cont->sync_out, NULL)) {
    ret = DEC_SYSTEM_ERROR;
    goto err1;
  }
  if(pthread_cond_init(&dec_cont->sync_out_cv, NULL)){
    ret = DEC_SYSTEM_ERROR;
    goto err2;
  }

  /* TODO this limit could later be given through the API. Dynamic reference
     frame allocation can allocate buffers dynamically up to this limit.
     Otherwise sw stops and waits for the free buffer to emerge to the fifo */
  dec_cont->dynamic_buffer_limit = AV1DEC_DYNAMIC_PIC_LIMIT;

  dec_cont->heif_mode = (dec_cfg->decoder_mode & DEC_HEIF) ? 1 : 0;//dec avif stream
  dec_cont->annexb = dec_cfg->annexb;
  dec_cont->plainobu = dec_cfg->plainobu;
  dec_cont->decoder.oppoints = dec_cfg->oppoints;

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif

  /* return new instance to application */
  *dec_inst = (Av1DecInst)dec_cont;
  dec_cont->num_buffers = dec_cfg->num_frame_buffers;
  dec_cont->dynamic_buffer_limit = AV1DEC_DYNAMIC_PIC_LIMIT;
  dec_cont->min_buffer_num = dec_cfg->num_frame_buffers;
  if (dec_cont->min_buffer_num < AV1_MIN_EXT_BUFFERS)
    dec_cont->min_buffer_num = AV1_MIN_EXT_BUFFERS;

  if ((dec_cfg->decoder_mode & DEC_HEIF) && (dec_cfg->decoder_mode & DEC_INTRA_ONLY))
    dec_cont->min_buffer_num = 1;
  dec_cont->num_buffers_reserved = dec_cont->num_buffers;
  dec_cont->bq = Av1BufferQueueInitialize(dec_cont->num_buffers);
  if (dec_cont->bq == NULL) {
    ret = DEC_MEMFAIL;
    goto err3;
  }

  dec_cont->vcmd_used = DWLVcmdIsUsed(dwl);
  if (dec_cont->vcmd_used && dec_cont->use_multicore) {
    /* Allows the maximum cmd buffers as real cores number. */
    ret = DEC_MEMFAIL;
    if(dec_cont->fifo_core) // Mark for klocwork issue, why, LXJ?
      goto err4; // shouldn't hanppen
    if (FifoInit(dec_cont->n_cores_available, &dec_cont->fifo_core) != FIFO_OK)
      goto err4;

    for (i = 0; i < dec_cont->n_cores_available; i++)
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)i, FIFO_EXCEPTION_DISABLE);
  }

  //Mark for klocwork: dec_cont->bq and dec_cont->pp_bq cannot be released here accorrding to Chen Min, ignore this issue
  return DEC_OK;
  //Mark for klocwork: cs_semaphore will be released in FifoRelease, it's not a problem
err4:
  if (dec_cont->fifo_core) FifoRelease(dec_cont->fifo_core);
  if (dec_cont->bq != NULL) Av1BufferQueueRelease(dec_cont->bq, 1);
err3:
  pthread_cond_destroy(&dec_cont->sync_out_cv);
err2:
  pthread_mutex_destroy(&dec_cont->sync_out);
err1:
  if (dec_cont->fifo_display) FifoRelease(dec_cont->fifo_display);
err0:
  if (dec_cont->fifo_out) FifoRelease(dec_cont->fifo_out);

  return ret;
}

void Av1DecRelease(Av1DecInst dec_inst) {
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
  u32 i = 0;
  /* Check for valid decoder instance */
  if (dec_cont == NULL || dec_cont->checksum != dec_cont) {
    return;
  }

  ASSERT(dec_cont->asic_running == 0);
  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (dec_cont->hw_rdy_callback_arg[i]) {
      DWLfree(dec_cont->hw_rdy_callback_arg[i]);
      dec_cont->hw_rdy_callback_arg[i] = NULL;
    }
  }
  Av1AsicReleaseMem(dec_cont->dwl, dec_cont->asic_buff);
  Av1ReleaseLowLatencyMem(dec_cont);
  Av1AsicReleaseFilterBlockMem(dec_cont->dwl, dec_cont->asic_buff);
  Av1AsicReleaseFbcMem(dec_cont);
  Av1AsicReleasePictures(dec_cont);

  if (dec_cont->fifo_out) FifoRelease(dec_cont->fifo_out);
  if (dec_cont->fifo_display) FifoRelease(dec_cont->fifo_display);
  if (dec_cont->vcmd_used && dec_cont->use_multicore) {
    if (dec_cont->fifo_core)
      FifoRelease(dec_cont->fifo_core);
  }

  pthread_cond_destroy(&dec_cont->sync_out_cv);
  pthread_mutex_destroy(&dec_cont->sync_out);

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(dec_cont->dwl, &dec_cont->ppu_cfg[i].lanczos_table);
      dec_cont->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  /* free metadata_param */
#ifdef SUPPORT_METADATA
  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    if (dec_cont->metadata_param[i]) {
      if (dec_cont->metadata_param[i]->t35_param.payload_byte.buffer) {
        FREE(dec_cont->metadata_param[i]->t35_param.payload_byte.buffer);
      }
      FREE(dec_cont->metadata_param[i]);
    }
  }
#endif

  dec_cont->checksum = NULL;
  DWLfree(dec_cont);

  return;
}

enum DecRet Av1DecGetInfo(Av1DecInst dec_inst, struct Av1DecInfo *dec_info) {
  const struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;

  if (dec_inst == NULL || dec_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return DEC_NOT_INITIALIZED;
  }

  if (dec_cont->dec_stat == AV1DEC_INITIALIZED) {
    return DEC_HDRS_NOT_RDY;
  }

  dec_info->vp_version = dec_cont->decoder.vp_version;
  dec_info->vp_profile = dec_cont->decoder.vp_profile;
  dec_info->bit_depth = dec_cont->decoder.bit_depth;

  /* Fragments have 8 pixels */
  dec_info->coded_width = dec_cont->decoder.width;
  dec_info->coded_height = dec_cont->decoder.height;
  dec_info->frame_height = NEXT_MULTIPLE(dec_cont->decoder.height, 8);
  dec_info->superres_width = dec_cont->decoder.superres_width;
  dec_info->frame_width = NEXT_MULTIPLE(dec_cont->decoder.superres_width, 8);
  dec_info->monochrome = dec_cont->decoder.monochrome;
  dec_info->scaled_width = dec_cont->decoder.scaled_width;
  dec_info->scaled_height = dec_cont->decoder.scaled_height;
  dec_info->pic_buff_size = dec_cont->min_buffer_num;
  dec_info->timing_info_present_flag = dec_cont->decoder.timing_info_present_flag;
  dec_info->num_units_in_tick = dec_cont->decoder.num_units_in_tick;
  dec_info->time_scale = dec_cont->decoder.time_scale;
  dec_info->max_width = dec_cont->decoder.max_width;
  dec_info->max_height = dec_cont->decoder.max_height;
  if (dec_cont->decoder.matrix_coefficients != AOM_CICP_MC_UNSPECIFIED ||
      dec_cont->decoder.color_primaries != AOM_CICP_CP_UNSPECIFIED ||
      dec_cont->decoder.transfer_characteristics != AOM_CICP_TC_UNSPECIFIED) {
    dec_info->colour_description_present_flag = 1;
  }
  dec_info->matrix_coefficients = dec_cont->decoder.matrix_coefficients;
  dec_info->colour_primaries = dec_cont->decoder.color_primaries;
  dec_info->transfer_characteristics = dec_cont->decoder.transfer_characteristics;

  return DEC_OK;
}
#if 1
void Av1ModifySequenceHeader(struct Av1Decoder *decoder) {
  int i;
  if (decoder->obu_seq_hdr_checked.sequencer_ok == 0) return;
  if (decoder->vp_profile != decoder->obu_seq_hdr_checked.vp_profile)
    decoder->vp_profile = decoder->obu_seq_hdr_checked.vp_profile;

  if (decoder->still_picture != decoder->obu_seq_hdr_checked.still_picture)
    decoder->still_picture = decoder->obu_seq_hdr_checked.still_picture;

  if (decoder->reduced_still_picture_hdr !=
      decoder->obu_seq_hdr_checked.reduced_still_picture_hdr)
    decoder->reduced_still_picture_hdr =
        decoder->obu_seq_hdr_checked.reduced_still_picture_hdr;

  if (decoder->timing_info_present_flag !=
      decoder->obu_seq_hdr_checked.timing_info_present_flag)
    decoder->timing_info_present_flag =
        decoder->obu_seq_hdr_checked.timing_info_present_flag;

  if (decoder->num_units_in_tick !=
      decoder->obu_seq_hdr_checked.num_units_in_tick)
    decoder->num_units_in_tick = decoder->obu_seq_hdr_checked.num_units_in_tick;

  if (decoder->time_scale != decoder->obu_seq_hdr_checked.time_scale)
    decoder->time_scale = decoder->obu_seq_hdr_checked.time_scale;

  if (decoder->equal_picture_interval !=
      decoder->obu_seq_hdr_checked.equal_picture_interval)
    decoder->equal_picture_interval =
        decoder->obu_seq_hdr_checked.equal_picture_interval;

  if (decoder->num_ticks_per_picture !=
      decoder->obu_seq_hdr_checked.num_ticks_per_picture)
    decoder->num_ticks_per_picture =
        decoder->obu_seq_hdr_checked.num_ticks_per_picture;

  if (decoder->decoder_model_info_present_flag !=
      decoder->obu_seq_hdr_checked.decoder_model_info_present_flag)
    decoder->decoder_model_info_present_flag =
        decoder->obu_seq_hdr_checked.decoder_model_info_present_flag;

  if (decoder->buffer_delay_length !=
      decoder->obu_seq_hdr_checked.buffer_delay_length)
    decoder->buffer_delay_length =
        decoder->obu_seq_hdr_checked.buffer_delay_length;

  if (decoder->num_units_in_decoding_tick !=
      decoder->obu_seq_hdr_checked.num_units_in_decoding_tick)
    decoder->num_units_in_decoding_tick =
        decoder->obu_seq_hdr_checked.num_units_in_decoding_tick;

  if (decoder->buffer_removal_time_length !=
      decoder->obu_seq_hdr_checked.buffer_removal_time_length)
    decoder->buffer_removal_time_length =
        decoder->obu_seq_hdr_checked.buffer_removal_time_length;

  if (decoder->frame_presentation_time_length !=
      decoder->obu_seq_hdr_checked.frame_presentation_time_length)
    decoder->frame_presentation_time_length =
        decoder->obu_seq_hdr_checked.frame_presentation_time_length;

  if (decoder->initial_display_delay_present_flag !=
      decoder->obu_seq_hdr_checked.initial_display_delay_present_flag)
    decoder->initial_display_delay_present_flag =
        decoder->obu_seq_hdr_checked.initial_display_delay_present_flag;

  if (decoder->operating_points_cnt !=
      decoder->obu_seq_hdr_checked.operating_points_cnt)
    decoder->operating_points_cnt =
        decoder->obu_seq_hdr_checked.operating_points_cnt;
  for (i = 0; i < 32; i++) {
    if (decoder->operating_point_idc[i] !=
        decoder->obu_seq_hdr_checked.operating_point_idc[i])
      decoder->operating_point_idc[i] =
          decoder->obu_seq_hdr_checked.operating_point_idc[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->level[i] != decoder->obu_seq_hdr_checked.level[i])
      decoder->level[i] = decoder->obu_seq_hdr_checked.level[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->seq_tier[i] != decoder->obu_seq_hdr_checked.seq_tier[i])
      decoder->seq_tier[i] = decoder->obu_seq_hdr_checked.seq_tier[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->initial_display_delay_present[i] !=
        decoder->obu_seq_hdr_checked.initial_display_delay_present[i])
      decoder->initial_display_delay_present[i] =
          decoder->obu_seq_hdr_checked.initial_display_delay_present[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->initial_display_delay[i] !=
        decoder->obu_seq_hdr_checked.initial_display_delay[i])
      decoder->initial_display_delay[i] =
          decoder->obu_seq_hdr_checked.initial_display_delay[i];
  }

  if (decoder->num_bits_w != decoder->obu_seq_hdr_checked.num_bits_w)
    decoder->num_bits_w = decoder->obu_seq_hdr_checked.num_bits_w;

  if (decoder->num_bits_h != decoder->obu_seq_hdr_checked.num_bits_h)
    decoder->num_bits_h = decoder->obu_seq_hdr_checked.num_bits_h;

  if (decoder->max_width != decoder->obu_seq_hdr_checked.max_width)
    decoder->max_width = decoder->obu_seq_hdr_checked.max_width;

  if (decoder->max_height != decoder->obu_seq_hdr_checked.max_height)
    decoder->max_height = decoder->obu_seq_hdr_checked.max_height;

  if (decoder->frame_id_numbers_present_flag !=
      decoder->obu_seq_hdr_checked.frame_id_numbers_present_flag)
    decoder->frame_id_numbers_present_flag =
        decoder->obu_seq_hdr_checked.frame_id_numbers_present_flag;

  if (decoder->delta_frame_id_length !=
      decoder->obu_seq_hdr_checked.delta_frame_id_length)
    decoder->delta_frame_id_length =
        decoder->obu_seq_hdr_checked.delta_frame_id_length;

  if (decoder->frame_id_length != decoder->obu_seq_hdr_checked.frame_id_length)
    decoder->frame_id_length = decoder->obu_seq_hdr_checked.frame_id_length;

  if (decoder->sb_size != decoder->obu_seq_hdr_checked.sb_size)
    decoder->sb_size = decoder->obu_seq_hdr_checked.sb_size;

  if (decoder->enable_filter_intra !=
      decoder->obu_seq_hdr_checked.enable_filter_intra)
    decoder->enable_filter_intra =
        decoder->obu_seq_hdr_checked.enable_filter_intra;

  if (decoder->enable_intra_edge_filter !=
      decoder->obu_seq_hdr_checked.enable_intra_edge_filter)
    decoder->enable_intra_edge_filter =
        decoder->obu_seq_hdr_checked.enable_intra_edge_filter;

  if (decoder->enable_interintra_compound !=
      decoder->obu_seq_hdr_checked.enable_interintra_compound)
    decoder->enable_interintra_compound =
        decoder->obu_seq_hdr_checked.enable_interintra_compound;

  if (decoder->enable_masked_compound !=
      decoder->obu_seq_hdr_checked.enable_masked_compound)
    decoder->enable_masked_compound =
        decoder->obu_seq_hdr_checked.enable_masked_compound;

  if (decoder->enable_warped_motion !=
      decoder->obu_seq_hdr_checked.enable_warped_motion)
    decoder->enable_warped_motion =
        decoder->obu_seq_hdr_checked.enable_warped_motion;

  if (decoder->enable_dual_filter !=
      decoder->obu_seq_hdr_checked.enable_dual_filter)
    decoder->enable_dual_filter =
        decoder->obu_seq_hdr_checked.enable_dual_filter;

  if (decoder->enable_order_hint !=
      decoder->obu_seq_hdr_checked.enable_order_hint)
    decoder->enable_order_hint = decoder->obu_seq_hdr_checked.enable_order_hint;

  if (decoder->enable_jnt_comp != decoder->obu_seq_hdr_checked.enable_jnt_comp)
    decoder->enable_jnt_comp = decoder->obu_seq_hdr_checked.enable_jnt_comp;

  if (decoder->enable_ref_frame_mvs !=
      decoder->obu_seq_hdr_checked.enable_ref_frame_mvs)
    decoder->enable_ref_frame_mvs =
        decoder->obu_seq_hdr_checked.enable_ref_frame_mvs;

  if (decoder->force_screen_content_tools !=
      decoder->obu_seq_hdr_checked.force_screen_content_tools)
    decoder->force_screen_content_tools =
        decoder->obu_seq_hdr_checked.force_screen_content_tools;

  if (decoder->force_integer_mv !=
      decoder->obu_seq_hdr_checked.force_integer_mv)
    decoder->force_integer_mv = decoder->obu_seq_hdr_checked.force_integer_mv;

  if (decoder->order_hint_bits_minus1 !=
      decoder->obu_seq_hdr_checked.order_hint_bits_minus1)
    decoder->order_hint_bits_minus1 =
        decoder->obu_seq_hdr_checked.order_hint_bits_minus1;

  if (decoder->enable_superres != decoder->obu_seq_hdr_checked.enable_superres)
    decoder->enable_superres = decoder->obu_seq_hdr_checked.enable_superres;

  if (decoder->enable_cdef != decoder->obu_seq_hdr_checked.enable_cdef)
    decoder->enable_cdef = decoder->obu_seq_hdr_checked.enable_cdef;

  if (decoder->enable_restoration !=
      decoder->obu_seq_hdr_checked.enable_restoration)
    decoder->enable_restoration =
        decoder->obu_seq_hdr_checked.enable_restoration;

  if (decoder->bit_depth != decoder->obu_seq_hdr_checked.bit_depth)
    decoder->bit_depth = decoder->obu_seq_hdr_checked.bit_depth;

  if (decoder->monochrome != decoder->obu_seq_hdr_checked.monochrome)
    decoder->monochrome = decoder->obu_seq_hdr_checked.monochrome;

  if (decoder->color_primaries != decoder->obu_seq_hdr_checked.color_primaries)
    decoder->color_primaries = decoder->obu_seq_hdr_checked.color_primaries;

  if (decoder->transfer_characteristics !=
      decoder->obu_seq_hdr_checked.transfer_characteristics)
    decoder->transfer_characteristics =
        decoder->obu_seq_hdr_checked.transfer_characteristics;

  if (decoder->matrix_coefficients !=
      decoder->obu_seq_hdr_checked.matrix_coefficients)
    decoder->matrix_coefficients =
        decoder->obu_seq_hdr_checked.matrix_coefficients;

  if (decoder->color_range != decoder->obu_seq_hdr_checked.color_range)
    decoder->color_range = decoder->obu_seq_hdr_checked.color_range;

  if (decoder->subsampling_x != decoder->obu_seq_hdr_checked.subsampling_x)
    decoder->subsampling_x = decoder->obu_seq_hdr_checked.subsampling_x;

  if (decoder->subsampling_y != decoder->obu_seq_hdr_checked.subsampling_y)
    decoder->subsampling_y = decoder->obu_seq_hdr_checked.subsampling_y;

  if (decoder->chroma_sample_position !=
      decoder->obu_seq_hdr_checked.chroma_sample_position)
    decoder->chroma_sample_position =
        decoder->obu_seq_hdr_checked.chroma_sample_position;

  if (decoder->separate_uv_delta_q !=
      decoder->obu_seq_hdr_checked.separate_uv_delta_q)
    decoder->separate_uv_delta_q =
        decoder->obu_seq_hdr_checked.separate_uv_delta_q;

  if (decoder->film_grain_params_present !=
      decoder->obu_seq_hdr_checked.film_grain_params_present)
    decoder->film_grain_params_present =
        decoder->obu_seq_hdr_checked.film_grain_params_present;
}

void Av1CheckSequenceHeader(struct Av1Decoder *decoder,
                            obuSequenceHeaderInput_t *seq_hdr) {
  int i;

  if (decoder->vp_profile != seq_hdr->vp_profile)
    decoder->vp_profile = seq_hdr->vp_profile;

  if (decoder->still_picture != seq_hdr->still_picture)
    decoder->still_picture = seq_hdr->still_picture;

  if (decoder->reduced_still_picture_hdr != seq_hdr->reduced_still_picture_hdr)
    decoder->reduced_still_picture_hdr = seq_hdr->reduced_still_picture_hdr;

  if (decoder->timing_info_present_flag != seq_hdr->timing_info_present_flag)
    decoder->timing_info_present_flag = seq_hdr->timing_info_present_flag;

  if (decoder->num_units_in_tick != seq_hdr->num_units_in_tick)
    decoder->num_units_in_tick = seq_hdr->num_units_in_tick;

  if (decoder->time_scale != seq_hdr->time_scale)
    decoder->time_scale = seq_hdr->time_scale;

  if (decoder->equal_picture_interval != seq_hdr->equal_picture_interval)
    decoder->equal_picture_interval = seq_hdr->equal_picture_interval;

  if (decoder->num_ticks_per_picture != seq_hdr->num_ticks_per_picture)
    decoder->num_ticks_per_picture = seq_hdr->num_ticks_per_picture;

  if (decoder->decoder_model_info_present_flag !=
      seq_hdr->decoder_model_info_present_flag)
    decoder->decoder_model_info_present_flag =
        seq_hdr->decoder_model_info_present_flag;

  if (decoder->buffer_delay_length != seq_hdr->buffer_delay_length)
    decoder->buffer_delay_length = seq_hdr->buffer_delay_length;

  if (decoder->num_units_in_decoding_tick !=
      seq_hdr->num_units_in_decoding_tick)
    decoder->num_units_in_decoding_tick = seq_hdr->num_units_in_decoding_tick;

  if (decoder->buffer_removal_time_length !=
      seq_hdr->buffer_removal_time_length)
    decoder->buffer_removal_time_length = seq_hdr->buffer_removal_time_length;

  if (decoder->frame_presentation_time_length !=
      seq_hdr->frame_presentation_time_length)
    decoder->frame_presentation_time_length =
        seq_hdr->frame_presentation_time_length;

  if (decoder->initial_display_delay_present_flag !=
      seq_hdr->initial_display_delay_present_flag)
    decoder->initial_display_delay_present_flag =
        seq_hdr->initial_display_delay_present_flag;

  if (decoder->operating_points_cnt != seq_hdr->operating_points_cnt)
    decoder->operating_points_cnt = seq_hdr->operating_points_cnt;
  for (i = 0; i < 32; i++) {
    if (decoder->operating_point_idc[i] != seq_hdr->operating_point_idc[i])
      decoder->operating_point_idc[i] = seq_hdr->operating_point_idc[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->level[i] != seq_hdr->level[i])
      decoder->level[i] = seq_hdr->level[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->seq_tier[i] != seq_hdr->seq_tier[i])
      decoder->seq_tier[i] = seq_hdr->seq_tier[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->initial_display_delay_present[i] !=
        seq_hdr->initial_display_delay_present[i])
      decoder->initial_display_delay_present[i] =
          seq_hdr->initial_display_delay_present[i];
  }
  for (i = 0; i < 32; i++) {
    if (decoder->initial_display_delay[i] != seq_hdr->initial_display_delay[i])
      decoder->initial_display_delay[i] = seq_hdr->initial_display_delay[i];
  }

  if (decoder->num_bits_w != seq_hdr->num_bits_w)
    decoder->num_bits_w = seq_hdr->num_bits_w;

  if (decoder->num_bits_h != seq_hdr->num_bits_h)
    decoder->num_bits_h = seq_hdr->num_bits_h;

  if (decoder->max_width != seq_hdr->max_width)
    decoder->max_width = seq_hdr->max_width;

  if (decoder->max_height != seq_hdr->max_height)
    decoder->max_height = seq_hdr->max_height;

  if (decoder->frame_id_numbers_present_flag !=
      seq_hdr->frame_id_numbers_present_flag)
    decoder->frame_id_numbers_present_flag =
        seq_hdr->frame_id_numbers_present_flag;

  if (decoder->delta_frame_id_length != seq_hdr->delta_frame_id_length)
    decoder->delta_frame_id_length = seq_hdr->delta_frame_id_length;

  if (decoder->frame_id_length != seq_hdr->frame_id_length)
    decoder->frame_id_length = seq_hdr->frame_id_length;

  if (decoder->sb_size != seq_hdr->sb_size) decoder->sb_size = seq_hdr->sb_size;

  if (decoder->enable_filter_intra != seq_hdr->enable_filter_intra)
    decoder->enable_filter_intra = seq_hdr->enable_filter_intra;

  if (decoder->enable_intra_edge_filter != seq_hdr->enable_intra_edge_filter)
    decoder->enable_intra_edge_filter = seq_hdr->enable_intra_edge_filter;

  if (decoder->enable_interintra_compound !=
      seq_hdr->enable_interintra_compound)
    decoder->enable_interintra_compound = seq_hdr->enable_interintra_compound;

  if (decoder->enable_masked_compound != seq_hdr->enable_masked_compound)
    decoder->enable_masked_compound = seq_hdr->enable_masked_compound;

  if (decoder->enable_warped_motion != seq_hdr->enable_warped_motion)
    decoder->enable_warped_motion = seq_hdr->enable_warped_motion;

  if (decoder->enable_dual_filter != seq_hdr->enable_dual_filter)
    decoder->enable_dual_filter = seq_hdr->enable_dual_filter;

  if (decoder->enable_order_hint != seq_hdr->enable_order_hint)
    decoder->enable_order_hint = seq_hdr->enable_order_hint;

  if (decoder->enable_jnt_comp != seq_hdr->enable_jnt_comp)
    decoder->enable_jnt_comp = seq_hdr->enable_jnt_comp;

  if (decoder->enable_ref_frame_mvs != seq_hdr->enable_ref_frame_mvs)
    decoder->enable_ref_frame_mvs = seq_hdr->enable_ref_frame_mvs;

  if (decoder->force_screen_content_tools !=
      seq_hdr->force_screen_content_tools)
    decoder->force_screen_content_tools = seq_hdr->force_screen_content_tools;

  if (decoder->force_integer_mv != seq_hdr->force_integer_mv)
    decoder->force_integer_mv = seq_hdr->force_integer_mv;

  if (decoder->order_hint_bits_minus1 != seq_hdr->order_hint_bits_minus1)
    decoder->order_hint_bits_minus1 = seq_hdr->order_hint_bits_minus1;

  if (decoder->enable_superres != seq_hdr->enable_superres)
    decoder->enable_superres = seq_hdr->enable_superres;

  if (decoder->enable_cdef != seq_hdr->enable_cdef)
    decoder->enable_cdef = seq_hdr->enable_cdef;

  if (decoder->enable_restoration != seq_hdr->enable_restoration)
    decoder->enable_restoration = seq_hdr->enable_restoration;

  if (decoder->bit_depth != seq_hdr->bit_depth)
    decoder->bit_depth = seq_hdr->bit_depth;

  if (decoder->monochrome != seq_hdr->monochrome)
    decoder->monochrome = seq_hdr->monochrome;

  if (decoder->color_primaries != seq_hdr->color_primaries)
    decoder->color_primaries = seq_hdr->color_primaries;

  if (decoder->transfer_characteristics != seq_hdr->transfer_characteristics)
    decoder->transfer_characteristics = seq_hdr->transfer_characteristics;

  if (decoder->matrix_coefficients != seq_hdr->matrix_coefficients)
    decoder->matrix_coefficients = seq_hdr->matrix_coefficients;

  if (decoder->color_range != seq_hdr->color_range)
    decoder->color_range = seq_hdr->color_range;

  if (decoder->subsampling_x != seq_hdr->subsampling_x)
    decoder->subsampling_x = seq_hdr->subsampling_x;

  if (decoder->subsampling_y != seq_hdr->subsampling_y)
    decoder->subsampling_y = seq_hdr->subsampling_y;

  if (decoder->chroma_sample_position != seq_hdr->chroma_sample_position)
    decoder->chroma_sample_position = seq_hdr->chroma_sample_position;

  if (decoder->separate_uv_delta_q != seq_hdr->separate_uv_delta_q)
    decoder->separate_uv_delta_q = seq_hdr->separate_uv_delta_q;

  if (decoder->film_grain_params_present != seq_hdr->film_grain_params_present)
    decoder->film_grain_params_present = seq_hdr->film_grain_params_present;
}

void Av1SaveSequenceHeader(struct Av1Decoder *decoder,
                           obuSequenceHeaderInput_t *seq_hdr) {
  int i;
  seq_hdr->vp_profile = decoder->vp_profile;

  seq_hdr->still_picture = decoder->still_picture;

  seq_hdr->reduced_still_picture_hdr = decoder->reduced_still_picture_hdr;

  seq_hdr->timing_info_present_flag = decoder->timing_info_present_flag;

  seq_hdr->num_units_in_tick = decoder->num_units_in_tick;

  seq_hdr->time_scale = decoder->time_scale;

  seq_hdr->equal_picture_interval = decoder->equal_picture_interval;

  seq_hdr->num_ticks_per_picture = decoder->num_ticks_per_picture;

  seq_hdr->decoder_model_info_present_flag =
      decoder->decoder_model_info_present_flag;

  seq_hdr->buffer_delay_length = decoder->buffer_delay_length;

  seq_hdr->num_units_in_decoding_tick = decoder->num_units_in_decoding_tick;

  seq_hdr->buffer_removal_time_length = decoder->buffer_removal_time_length;

  seq_hdr->frame_presentation_time_length =
      decoder->frame_presentation_time_length;

  seq_hdr->initial_display_delay_present_flag =
      decoder->initial_display_delay_present_flag;

  seq_hdr->operating_points_cnt = decoder->operating_points_cnt;
  for (i = 0; i < 32; i++) {
    seq_hdr->operating_point_idc[i] = decoder->operating_point_idc[i];
  }
  for (i = 0; i < 32; i++) {
    seq_hdr->level[i] = decoder->level[i];
  }
  for (i = 0; i < 32; i++) {
    seq_hdr->seq_tier[i] = decoder->seq_tier[i];
  }
  for (i = 0; i < 32; i++) {
    seq_hdr->initial_display_delay_present[i] =
        decoder->initial_display_delay_present[i];
  }
  for (i = 0; i < 32; i++) {
    seq_hdr->initial_display_delay[i] = decoder->initial_display_delay[i];
  }

  seq_hdr->num_bits_w = decoder->num_bits_w;

  seq_hdr->num_bits_h = decoder->num_bits_h;

  seq_hdr->max_width = decoder->max_width;

  seq_hdr->max_height = decoder->max_height;

  seq_hdr->frame_id_numbers_present_flag =
      decoder->frame_id_numbers_present_flag;

  seq_hdr->delta_frame_id_length = decoder->delta_frame_id_length;

  seq_hdr->frame_id_length = decoder->frame_id_length;

  seq_hdr->sb_size = decoder->sb_size;

  seq_hdr->enable_filter_intra = decoder->enable_filter_intra;

  seq_hdr->enable_intra_edge_filter = decoder->enable_intra_edge_filter;

  seq_hdr->enable_interintra_compound = decoder->enable_interintra_compound;

  seq_hdr->enable_masked_compound = decoder->enable_masked_compound;

  seq_hdr->enable_warped_motion = decoder->enable_warped_motion;

  seq_hdr->enable_dual_filter = decoder->enable_dual_filter;

  seq_hdr->enable_order_hint = decoder->enable_order_hint;

  seq_hdr->enable_jnt_comp = decoder->enable_jnt_comp;

  seq_hdr->enable_ref_frame_mvs = decoder->enable_ref_frame_mvs;

  seq_hdr->force_screen_content_tools = decoder->force_screen_content_tools;

  seq_hdr->force_integer_mv = decoder->force_integer_mv;

  seq_hdr->order_hint_bits_minus1 = decoder->order_hint_bits_minus1;

  seq_hdr->enable_superres = decoder->enable_superres;

  seq_hdr->enable_cdef = decoder->enable_cdef;

  seq_hdr->enable_restoration = decoder->enable_restoration;

  seq_hdr->bit_depth = decoder->bit_depth;

  seq_hdr->monochrome = decoder->monochrome;

  seq_hdr->color_primaries = decoder->color_primaries;

  seq_hdr->transfer_characteristics = decoder->transfer_characteristics;

  seq_hdr->matrix_coefficients = decoder->matrix_coefficients;

  seq_hdr->color_range = decoder->color_range;

  seq_hdr->subsampling_x = decoder->subsampling_x;

  seq_hdr->subsampling_y = decoder->subsampling_y;

  seq_hdr->chroma_sample_position = decoder->chroma_sample_position;

  seq_hdr->separate_uv_delta_q = decoder->separate_uv_delta_q;

  seq_hdr->film_grain_params_present = decoder->film_grain_params_present;
}

void Av1RestoreSequenceHeader(struct Av1Decoder *decoder,
                              obuSequenceHeaderInput_t *seq_hdr) {
  int i;
  decoder->vp_profile = seq_hdr->vp_profile;

  decoder->still_picture = seq_hdr->still_picture;

  decoder->reduced_still_picture_hdr = seq_hdr->reduced_still_picture_hdr;

  decoder->timing_info_present_flag = seq_hdr->timing_info_present_flag;

  decoder->num_units_in_tick = seq_hdr->num_units_in_tick;

  decoder->time_scale = seq_hdr->time_scale;

  decoder->equal_picture_interval = seq_hdr->equal_picture_interval;

  decoder->num_ticks_per_picture = seq_hdr->num_ticks_per_picture;

  decoder->decoder_model_info_present_flag =
      seq_hdr->decoder_model_info_present_flag;

  decoder->buffer_delay_length = seq_hdr->buffer_delay_length;

  decoder->num_units_in_decoding_tick = seq_hdr->num_units_in_decoding_tick;

  decoder->buffer_removal_time_length = seq_hdr->buffer_removal_time_length;

  decoder->frame_presentation_time_length =
      seq_hdr->frame_presentation_time_length;

  decoder->initial_display_delay_present_flag =
      seq_hdr->initial_display_delay_present_flag;

  decoder->operating_points_cnt = seq_hdr->operating_points_cnt;
  for (i = 0; i < 32; i++) {
    decoder->operating_point_idc[i] = seq_hdr->operating_point_idc[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->level[i] = seq_hdr->level[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->seq_tier[i] = seq_hdr->seq_tier[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->initial_display_delay_present[i] =
        seq_hdr->initial_display_delay_present[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->initial_display_delay[i] = seq_hdr->initial_display_delay[i];
  }

  decoder->num_bits_w = seq_hdr->num_bits_w;

  decoder->num_bits_h = seq_hdr->num_bits_h;

  decoder->max_width = seq_hdr->max_width;

  decoder->max_height = seq_hdr->max_height;

  decoder->frame_id_numbers_present_flag =
      seq_hdr->frame_id_numbers_present_flag;

  decoder->delta_frame_id_length = seq_hdr->delta_frame_id_length;

  decoder->frame_id_length = seq_hdr->frame_id_length;

  decoder->sb_size = seq_hdr->sb_size;

  decoder->enable_filter_intra = seq_hdr->enable_filter_intra;

  decoder->enable_intra_edge_filter = seq_hdr->enable_intra_edge_filter;

  decoder->enable_interintra_compound = seq_hdr->enable_interintra_compound;

  decoder->enable_masked_compound = seq_hdr->enable_masked_compound;

  decoder->enable_warped_motion = seq_hdr->enable_warped_motion;

  decoder->enable_dual_filter = seq_hdr->enable_dual_filter;

  decoder->enable_order_hint = seq_hdr->enable_order_hint;

  decoder->enable_jnt_comp = seq_hdr->enable_jnt_comp;

  decoder->enable_ref_frame_mvs = seq_hdr->enable_ref_frame_mvs;

  decoder->force_screen_content_tools = seq_hdr->force_screen_content_tools;

  decoder->force_integer_mv = seq_hdr->force_integer_mv;

  decoder->order_hint_bits_minus1 = seq_hdr->order_hint_bits_minus1;

  decoder->enable_superres = seq_hdr->enable_superres;

  decoder->enable_cdef = seq_hdr->enable_cdef;

  decoder->enable_restoration = seq_hdr->enable_restoration;

  decoder->bit_depth = seq_hdr->bit_depth;

  decoder->monochrome = seq_hdr->monochrome;

  decoder->color_primaries = seq_hdr->color_primaries;

  decoder->transfer_characteristics = seq_hdr->transfer_characteristics;

  decoder->matrix_coefficients = seq_hdr->matrix_coefficients;

  decoder->color_range = seq_hdr->color_range;

  decoder->subsampling_x = seq_hdr->subsampling_x;

  decoder->subsampling_y = seq_hdr->subsampling_y;

  decoder->chroma_sample_position = seq_hdr->chroma_sample_position;

  decoder->separate_uv_delta_q = seq_hdr->separate_uv_delta_q;

  decoder->film_grain_params_present = seq_hdr->film_grain_params_present;
}

int Av1CompareSequenceHeader(obuSequenceHeaderInput_t *seq_hdr_a,
                             obuSequenceHeaderInput_t *seq_hdr_b) {
  int i;
  if (seq_hdr_a->vp_profile != seq_hdr_b->vp_profile) return 1;

  if (seq_hdr_a->still_picture != seq_hdr_b->still_picture) return 1;

  if (seq_hdr_a->reduced_still_picture_hdr !=
      seq_hdr_b->reduced_still_picture_hdr)
    return 1;

  if (seq_hdr_a->timing_info_present_flag !=
      seq_hdr_b->timing_info_present_flag)
    return 1;

  if (seq_hdr_a->num_units_in_tick != seq_hdr_b->num_units_in_tick) return 1;

  if (seq_hdr_a->time_scale != seq_hdr_b->time_scale) return 1;

  if (seq_hdr_a->equal_picture_interval != seq_hdr_b->equal_picture_interval)
    return 1;

  if (seq_hdr_a->num_ticks_per_picture != seq_hdr_b->num_ticks_per_picture)
    return 1;

  if (seq_hdr_a->decoder_model_info_present_flag !=
      seq_hdr_b->decoder_model_info_present_flag)
    return 1;

  if (seq_hdr_a->buffer_delay_length != seq_hdr_b->buffer_delay_length)
    return 1;

  if (seq_hdr_a->num_units_in_decoding_tick !=
      seq_hdr_b->num_units_in_decoding_tick)
    return 1;

  if (seq_hdr_a->buffer_removal_time_length !=
      seq_hdr_b->buffer_removal_time_length)
    return 1;

  if (seq_hdr_a->frame_presentation_time_length !=
      seq_hdr_b->frame_presentation_time_length)
    return 1;

  if (seq_hdr_a->initial_display_delay_present_flag !=
      seq_hdr_b->initial_display_delay_present_flag)
    return 1;

  if (seq_hdr_a->operating_points_cnt != seq_hdr_b->operating_points_cnt)
    return 1;

  for (i = 0; i < 32; i++) {
    if (seq_hdr_a->operating_point_idc[i] != seq_hdr_b->operating_point_idc[i])
      return 1;
  }
  for (i = 0; i < 32; i++) {
    if (seq_hdr_a->level[i] != seq_hdr_b->level[i]) return 1;
  }
  for (i = 0; i < 32; i++) {
    if (seq_hdr_a->seq_tier[i] != seq_hdr_b->seq_tier[i]) return 1;
  }
  for (i = 0; i < 32; i++) {
    if (seq_hdr_a->initial_display_delay_present[i] !=
        seq_hdr_b->initial_display_delay_present[i])
      return 1;
  }
  for (i = 0; i < 32; i++) {
    if (seq_hdr_a->initial_display_delay[i] !=
        seq_hdr_b->initial_display_delay[i])
      return 1;
  }

  if (seq_hdr_a->num_bits_w != seq_hdr_b->num_bits_w) return 1;

  if (seq_hdr_a->num_bits_h != seq_hdr_b->num_bits_h) return 1;

  if (seq_hdr_a->max_width != seq_hdr_b->max_width) return 1;

  if (seq_hdr_a->max_height != seq_hdr_b->max_height) return 1;

  if (seq_hdr_a->frame_id_numbers_present_flag !=
      seq_hdr_b->frame_id_numbers_present_flag)
    return 1;

  if (seq_hdr_a->delta_frame_id_length != seq_hdr_b->delta_frame_id_length)
    return 1;

  if (seq_hdr_a->frame_id_length != seq_hdr_b->frame_id_length) return 1;

  if (seq_hdr_a->sb_size != seq_hdr_b->sb_size) return 1;

  if (seq_hdr_a->enable_filter_intra != seq_hdr_b->enable_filter_intra)
    return 1;

  if (seq_hdr_a->enable_intra_edge_filter !=
      seq_hdr_b->enable_intra_edge_filter)
    return 1;

  if (seq_hdr_a->enable_interintra_compound !=
      seq_hdr_b->enable_interintra_compound)
    return 1;

  if (seq_hdr_a->enable_masked_compound != seq_hdr_b->enable_masked_compound)
    return 1;

  if (seq_hdr_a->enable_warped_motion != seq_hdr_b->enable_warped_motion)
    return 1;

  if (seq_hdr_a->enable_dual_filter != seq_hdr_b->enable_dual_filter) return 1;

  if (seq_hdr_a->enable_order_hint != seq_hdr_b->enable_order_hint) return 1;

  if (seq_hdr_a->enable_jnt_comp != seq_hdr_b->enable_jnt_comp) return 1;

  if (seq_hdr_a->enable_ref_frame_mvs != seq_hdr_b->enable_ref_frame_mvs)
    return 1;

  if (seq_hdr_a->force_screen_content_tools !=
      seq_hdr_b->force_screen_content_tools)
    return 1;

  if (seq_hdr_a->force_integer_mv != seq_hdr_b->force_integer_mv) return 1;

  if (seq_hdr_a->order_hint_bits_minus1 != seq_hdr_b->order_hint_bits_minus1)
    return 1;

  if (seq_hdr_a->enable_superres != seq_hdr_b->enable_superres) return 1;

  if (seq_hdr_a->enable_cdef != seq_hdr_b->enable_cdef) return 1;

  if (seq_hdr_a->enable_restoration != seq_hdr_b->enable_restoration) return 1;

  if (seq_hdr_a->bit_depth != seq_hdr_b->bit_depth) return 1;

  if (seq_hdr_a->monochrome != seq_hdr_b->monochrome) return 1;

  if (seq_hdr_a->color_primaries != seq_hdr_b->color_primaries) return 1;

  if (seq_hdr_a->transfer_characteristics !=
      seq_hdr_b->transfer_characteristics)
    return 1;

  if (seq_hdr_a->matrix_coefficients != seq_hdr_b->matrix_coefficients)
    return 1;

  if (seq_hdr_a->color_range != seq_hdr_b->color_range) return 1;

  if (seq_hdr_a->subsampling_x != seq_hdr_b->subsampling_x) return 1;

  if (seq_hdr_a->subsampling_y != seq_hdr_b->subsampling_y) return 1;

  if (seq_hdr_a->chroma_sample_position != seq_hdr_b->chroma_sample_position)
    return 1;

  if (seq_hdr_a->separate_uv_delta_q != seq_hdr_b->separate_uv_delta_q)
    return 1;

  if (seq_hdr_a->film_grain_params_present !=
      seq_hdr_b->film_grain_params_present)
    return 1;
  return 0;
}

#endif
enum DecRet Av1DecDecode(Av1DecInst dec_inst, const struct Av1DecInput *input,
                         struct DecOutput *output) {
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
  u32 picture_broken = HANTRO_FALSE;
  i32 ret;
  u32 data_len;
  u32 vcmd_m2m;
  // struct DWLLinearMem occupied_bitstream;
  /* Check that function input parameters are valid */
  if (input == NULL || output == NULL || dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return DEC_NOT_INITIALIZED;
  }

  if (dec_cont->abort) {
    return (DEC_ABORTED);
  }

  if ((input->data_len > DEC_MAX_STREAM) ||
      DEC_CHECK_VIRTUAL_ADDRESS(input->stream) ||
      DEC_CHECK_BUS_ADDRESS(input->stream_bus_address)) {
    return DEC_PARAM_ERROR;
  }
  dec_cont->input_data_len = input->data_len;  // used to generate error stream
  dec_cont->usr_ptr = input->usr_ptr;

  if (dec_cont->no_decoding_buffer) {
    // If no decoding buffer, go to request new buffer directly.
    goto request_free_buffer;
  }


  /* If there are more buffers to be allocated or to be freed, waiting for
   * buffers ready. */
  if (dec_cont->buf_to_free != NULL || dec_cont->buf_not_added == 1 ||
      (dec_cont->next_buf_size != 0 &&
       dec_cont->buffer_num_added < dec_cont->min_buffer_num)) {
    ret = DEC_WAITING_FOR_BUFFER;
    goto RETURN_FOR_BUFFER;
  }

  if (dec_cont->dec_stat == AV1DEC_NEW_HEADERS) {
    /* Check whether pp parameters are legal. */
    // ret = Av1DecSetInfo(dec_inst);
    // if (ret) return (enum DecRet)ret;
    ret = Av1AsicAllocatePictures(dec_cont);
    if (ret == DEC_WAITING_FOR_BUFFER || ret == DEC_MEMFAIL) {
      goto RETURN_FOR_BUFFER;
    }
    dec_cont->dec_stat = AV1DEC_DECODING;
  } else if (dec_cont->dec_stat == AV1DEC_WAITING_FOR_BUFFER) {
  } else if (input->data_len) {
    /* Decode SW part of the frame */
    ret = Av1DecodeHeaders(dec_cont, input);
    if (ret == DEC_INFOPARAM_ERROR) {
      return DEC_INFOPARAM_ERROR;
    } else if (ret == DEC_STRM_ERROR) {
      dec_cont->picture_broken = HANTRO_TRUE;
      goto RETURN_FOR_BUFFER;
    } else if (ret) {
      if (ret == DEC_HDRS_RDY)
        FifoPush(dec_cont->fifo_out, (void *)FLUSH_MARKER, FIFO_EXCEPTION_DISABLE);
      goto RETURN_FOR_BUFFER;
    }
  }
  /* missing picture, conceal */
  else {
    if (/*dec_cont->force_intra_freeze || */dec_cont->prev_is_key) {
      dec_cont->decoder.probs_decoded = 0;
      Av1Freeze(dec_cont);
      return DEC_PIC_DECODED;
    }
  }

  if (dec_cont->pp.bit_depth > dec_cont->decoder.bit_depth) {
    return DEC_PARAM_ERROR; /* Decoder is not wired to upsample in PP. */
  }
  dec_cont->pack_pixel_to_msbs = dec_cont->pp.bit_packing == DEC_BITPACKING_MSB;

request_free_buffer:
  ret = Av1AsicAllocateFakeTableMem(dec_cont);
  if (ret) return (DEC_MEMFAIL);
  /* Get free picture buffer */
  ret = Av1GetRefFrm(dec_cont, input);
  if (ret == DEC_ABORTED) {
    dec_cont->no_decoding_buffer = 0;
    goto RETURN_FOR_BUFFER;
  } else if (ret == DEC_NO_DECODING_BUFFER) {
    // Av1UpdateMetadataInfo(NULL, dec_cont->metadata_param_curr);
    dec_cont->no_decoding_buffer = 1;
    goto RETURN_FOR_BUFFER;
  } else if (ret == DEC_WAITING_FOR_BUFFER) {
    dec_cont->no_decoding_buffer = 0;
    dec_cont->dec_stat = AV1DEC_WAITING_FOR_BUFFER;
    goto RETURN_FOR_BUFFER;
  } else if (ret == DEC_MEMFAIL) {
    dec_cont->no_decoding_buffer = 0;
    goto RETURN_FOR_BUFFER;
  } else
    dec_cont->no_decoding_buffer = 0;

  // ret = Av1AsicAllocateFilterBlockMem(dec_cont);
  /* Verify we have enough auxiliary buffer for filter tile edge data. */
  ret = Av1AsicAllocateFilterBlockMem(
    dec_cont->dwl, &dec_cont->decoder,
    dec_cont->asic_buff,
    dec_cont->pp_enabled ? dec_cont->ppu_cfg : NULL,
    dec_cont->use_multicore_backup,
    dec_cont->secure_mode);
  if (ret) return (DEC_MEMFAIL);

  ret = Av1AsicAllocateFbcMem(dec_cont);
  if (ret) return (DEC_MEMFAIL);
  dec_cont->dec_stat = AV1DEC_DECODING;

  /* prepare asic */

#ifdef SUPPORT_VCMD_M2M
  vcmd_m2m = dec_cont->vcmd_m2m;
#else
  vcmd_m2m = 0;
#endif

  Av1AsicProbUpdate(&dec_cont->decoder, dec_cont->asic_buff, &dec_cont->sw_ctrl, vcmd_m2m);

  DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->prob_tbl, 0,
                  dec_cont->asic_buff->prob_tbl.size, HOST_TO_DEVICE);

  //Av1AsicInitPicture(dec_cont,input);

#if 0
  DWLMarkMemory(
      dec_cont->dwl,
      &dec_cont->asic_buff->pictures[dec_cont->asic_buff->out_buffer_i],
      DWL_HW_MARK_DEVICE_TO_HOST | DWL_HW_MARK_ONE_SHOT);
  DWLMarkMemory(
      dec_cont->dwl,
      &dec_cont->asic_buff->pictures_c[dec_cont->asic_buff->out_buffer_i],
      DWL_HW_MARK_DEVICE_TO_HOST | DWL_HW_MARK_ONE_SHOT);
#endif
#if 0
  if (dec_cont->secondary_out_cfg.secondary_output_e) {
    DWLMarkMemory(dec_cont->dwl,
                  &dec_cont->asic_buff
                       ->pp_pictures[dec_cont->asic_buff->out_buffer_i],
                  DWL_HW_MARK_DEVICE_TO_HOST | DWL_HW_MARK_ONE_SHOT);
  }
#endif

  if (dec_cont->decoder.not_valid_tile_dimension) {
    return DEC_STREAM_NOT_SUPPORTED;
  }

  data_len = dec_cont->decoder.frame_size;
  Av1AsicStrmPosUpdate(dec_cont, input->stream_bus_address, data_len,
                       input->buffer_bus_address, input->buff_len);

  DWLDMATransData2(dec_cont->dwl, (addr_t)input->buffer_bus_address,
                  (void *)input->buffer,
                  input->buff_len, HOST_TO_DEVICE);

  Av1AsicInitPicture(dec_cont,input);

  //if(dec_cont->sw_ctrl.sw_overlap_flag && (dec_cont->height > 4352)){
  //  APITRACEDEBUG("film grain stream height %d larger than 4352\n",dec_cont->height);
  //  return DEC_STREAM_NOT_SUPPORTED;
  //}
  /* rollback DEC_EC_SEEK_NEXT_I to DEC_EC_NO_SKIP */
  if (dec_cont->decoder.key_frame || dec_cont->decoder.intra_only || dec_cont->decoder.allow_intrabc) {
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
  if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only && !dec_cont->decoder.allow_intrabc) {
    if (dec_cont->picture_broken && dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) {
      picture_broken = HANTRO_TRUE;
    }
    if (dec_cont->error_policy & DEC_EC_REF_REPLACE && dec_cont->use_video_compressor) {
      u32 tmp = Av1ReplaceRefAvalible(dec_cont);
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
    Av1AsicRun(dec_cont, input->pic_id);
  } else {
    dec_cont->error_info = DEC_FRAME_ERROR;
    Av1SetupPicToOutput(dec_cont, input->pic_id);
  }

#ifdef CASE_INFO_STAT
  Av1CaseInfoCollect(dec_cont, &case_info);
#endif

  dec_cont->usr_ptr = input->usr_ptr;
  /* process IRQ & set output picc */
  ret = Av1SyncAndOutput(dec_cont);
  if (ret) goto RETURN_FOR_BUFFER;

  if (dec_cont->decoder.show_frame || dec_cont->decoder.show_existing_frame) {
    ret = DEC_PIC_DECODED;
  } else {
    ret = DEC_STRM_PROCESSED;
  }

RETURN_FOR_BUFFER:
  if (dec_cont->abort)
    ret = DEC_ABORTED;

  if (ret == DEC_WAITING_FOR_BUFFER || ret == DEC_HDRS_RDY ||
      ret == DEC_NO_DECODING_BUFFER) {
    output->data_left = input->data_len;
    output->strm_curr_pos = input->stream;
    output->strm_curr_bus_address = input->stream_bus_address;
    return (enum DecRet)ret;
  } else {
    output->data_left = input->data_len - dec_cont->decoder.frame_size;
    output->strm_curr_pos = input->stream + dec_cont->decoder.frame_size;
    output->strm_curr_bus_address =
        input->stream_bus_address + dec_cont->decoder.frame_size;
    if (dec_cont->low_latency) {
      output->data_left = 0;
      output->strm_curr_pos = input->stream + dec_cont->llstrminfo.stream_info.send_len;
      output->strm_curr_bus_address = input->stream_bus_address + dec_cont->llstrminfo.stream_info.send_len;
    }

    if(output->strm_curr_bus_address >= (input->buffer_bus_address + input->buff_len)) {
      output->strm_curr_bus_address -= input->buff_len;
      output->strm_curr_pos -= input->buff_len;
    }
  }

  return (enum DecRet)ret;
}

u32 Av1CheckSupport(struct Av1DecContainer *dec_cont) {
  // AV1 Level 5.3 limits
  if (// Filter line buffer can handle up to 4k width. With AV1 this is not a
      // Filter line buffer can handle up to 4k width. With AV1 this is not a
      // problem if there are no tile rows as maximum tile width is 4k, but if
      // there are tile rows the line buffer must extend to the full width of
      // the decoded picture.
      // AV1 decodes row tiles in transposed order
      ((dec_cont->decoder.bit_depth == 12 ||
        dec_cont->decoder.subsampling_x != 1 ||
        dec_cont->decoder.subsampling_y != 1))) {
    return HANTRO_NOK;
  }

  return HANTRO_OK;
}

void Av1Freeze(struct Av1DecContainer *dec_cont) {
  /* Rollback entropy probabilities if refresh is not set */
  if (dec_cont->decoder.probs_decoded &&
      dec_cont->decoder.refresh_entropy_probs == HANTRO_FALSE) {
    DWLmemcpy(&dec_cont->decoder.entropy, &dec_cont->decoder.entropy_last,
              sizeof(struct Av1EntropyProbs));
  }
  // /* lost accumulated coeff prob updates -> force video freeze until next
  //  * keyframe received */
  // else if (dec_cont->prob_refresh_detected) {
  //   dec_cont->force_intra_freeze = 1;
  // }

  dec_cont->picture_broken = HANTRO_FALSE;

  /* TODO Reset mv memory if needed? */
}

u32 ReadUvlc(struct StrmData *rb) {
  int lz = 0;
  while (!SwGetBits(rb, 1)) lz++;
  // Maximum 32 bits.
  if (lz >= 32) return UINT32_MAX;
  u32 v = SwGetBits(rb, lz);
  v += ((u32)1 << lz) - 1;
  return v;
}

#define LEVEL_BITS 5

static i32 ObuSequenceHeader(struct StrmData *rb,
                             struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  // profile
  dec->vp_profile = SwGetBits(rb, 3);
  // level
  dec->still_picture = SwGetBits(rb, 1);
  dec->reduced_still_picture_hdr = SwGetBits(rb, 1);

  if (dec->reduced_still_picture_hdr) {
    dec->timing_info_present_flag = 0;
    dec->decoder_model_info_present_flag = 0;
    dec->initial_display_delay_present_flag = 0;
    dec->operating_points_cnt = 1;
    dec->operating_point_idc[0] = 0;
    dec->level[0] = SwGetBits(rb, LEVEL_BITS);
    dec->seq_tier[0] = 0;
    dec->decoder_model_present[0] = 0;
    dec->initial_display_delay_present[0] = 0;
  } else {
    dec->timing_info_present_flag = SwGetBits(rb, 1);
    if (dec->timing_info_present_flag) {
      dec->num_units_in_tick = SwGetBits32(rb);
      dec->time_scale = SwGetBits32(rb);
      dec->equal_picture_interval = SwGetBits(rb, 1);
      if (dec->equal_picture_interval)
        dec->num_ticks_per_picture = ReadUvlc(rb) + 1;
      dec->decoder_model_info_present_flag = SwGetBits(rb, 1);
      if (dec->decoder_model_info_present_flag) {
        dec->buffer_delay_length = SwGetBits(rb, 5) + 1;
        dec->num_units_in_decoding_tick = SwGetBits32(rb);
        dec->buffer_removal_time_length = SwGetBits(rb, 5) + 1;
        dec->frame_presentation_time_length = SwGetBits(rb, 5) + 1;
      }
    } else {
      dec->decoder_model_info_present_flag = 0;
    }

    dec->initial_display_delay_present_flag = SwGetBits(rb, 1);

    dec->operating_points_cnt = SwGetBits(rb, 5) + 1;
    for (int i = 0; i < (int)dec->operating_points_cnt; i++) {
      dec->operating_point_idc[i] = SwGetBits(rb, 12);
      dec->level[i] = SwGetBits(rb, LEVEL_BITS);
      // TODO: check invalid (24-30)
      if (dec->level[i] > 7)
        dec->seq_tier[i] = SwGetBits(rb, 1);
      else
        dec->seq_tier[i] = 0;
      if (dec->decoder_model_info_present_flag) {
        dec->decoder_model_present[i] = SwGetBits(rb, 1);
        if (dec->decoder_model_present[i]) {
          int n = dec->buffer_delay_length;
          dec->decoder_buffer_delay[i] = SwGetBits(rb, n);
          dec->encoder_buffer_delay[i] = SwGetBits(rb, n);
          dec->low_delay_mode_flag[i] = SwGetBits(rb, 1);
        }
      } else {
        dec->decoder_model_present[i] = 0;
      }

      if (dec->initial_display_delay_present_flag) {
        dec->initial_display_delay_present[i] = SwGetBits(rb, 1);
        if (dec->initial_display_delay_present[i]) {
          dec->initial_display_delay[i] = SwGetBits(rb, 4);
        }
      }
    }
  }

#ifdef VCD_LOGMSG
  char* profile = "not support";
  switch (dec->vp_profile)
  {
  case 0:
    profile = "Main";
    break;
  case 1:
    profile = "High";
    break;
  case 2:
    profile = "Professional";
    break;
  default:
    break;
  }
  char* level = "not support";
  switch (dec->level[0])
  {
  case 0:
    level = "2.0";
    break;
  case 1:
    level = "2.1";
    break;
  case 2:
    level = "2.2";
    break;
  case 3:
    level = "2.3";
    break;
  case 4:
    level = "3.0";
    break;
  case 5:
    level = "3.1";
    break;
  case 6:
    level = "3.2";
    break;
  case 7:
    level = "3.3";
    break;
  case 8:
    level = "4.0";
    break;
  case 9:
    level = "4.1";
    break;
  case 10:
    level = "4.2";
    break;
  case 11:
    level = "4.3";
    break;
  case 12:
    level = "5.0";
    break;
  case 13:
    level = "5.1";
    break;
  case 14:
    level = "5.2";
    break;
  case 15:
    level = "5.3";
    break;
  case 16:
    level = "6.0";
    break;
  case 17:
    level = "6.1";
    break;
  case 18:
    level = "6.2";
    break;
  case 19:
    level = "6.3";
    break;
  case 20:
    level = "7.0";
    break;
  case 21:
    level = "7.1";
    break;
  case 22:
    level = "7.2";
    break;
  case 23:
    level = "7.3";
    break;
  default:
    break;
  }
  STREAMTRACE_I("Profile:%s | Level:%s\n", profile, level);
#endif

  dec->current_operating_point = dec->operating_point_idc[0];

  dec->num_bits_w = SwGetBits(rb, 4) + 1;
  dec->num_bits_h = SwGetBits(rb, 4) + 1;
  dec->max_width = SwGetBits(rb, dec->num_bits_w) + 1;
  dec->max_height = SwGetBits(rb, dec->num_bits_h) + 1;

  if (dec->reduced_still_picture_hdr)
    dec->frame_id_numbers_present_flag = 0;
  else
    dec->frame_id_numbers_present_flag = SwGetBits(rb, 1);
  if (dec->frame_id_numbers_present_flag) {
    dec->delta_frame_id_length = SwGetBits(rb, 4) + 2;
    dec->frame_id_length = SwGetBits(rb, 3) + dec->delta_frame_id_length + 1;
  }
  // sb_size: use_128x128_superblock
  dec->sb_size = SwGetBits(rb, 1);

  dec->enable_filter_intra = SwGetBits(rb, 1);
  dec->enable_intra_edge_filter = SwGetBits(rb, 1);

  if (!dec->reduced_still_picture_hdr) {
    dec->enable_interintra_compound = SwGetBits(rb, 1);
    dec->enable_masked_compound = SwGetBits(rb, 1);
    dec->enable_warped_motion = SwGetBits(rb, 1);
    dec->enable_dual_filter = SwGetBits(rb, 1);
    dec->enable_order_hint = SwGetBits(rb, 1);
    dec->enable_jnt_comp = dec->enable_order_hint ? SwGetBits(rb, 1) : 0;
    dec->enable_ref_frame_mvs = dec->enable_order_hint ? SwGetBits(rb, 1) : 0;

    if (SwGetBits(rb, 1)) {
      dec->force_screen_content_tools = 2;
    } else {
      dec->force_screen_content_tools = SwGetBits(rb, 1);
    }

    if (dec->force_screen_content_tools) {
      if (SwGetBits(rb, 1))
        dec->force_integer_mv = 2;
      else
        dec->force_integer_mv = SwGetBits(rb, 1);
    }

    if (dec->enable_order_hint)
      dec->order_hint_bits_minus1 = SwGetBits(rb, 3);
    else
      dec->order_hint_bits_minus1 = -1;
  } else {
    dec->enable_interintra_compound = 0;
    dec->enable_masked_compound = 0;
    dec->enable_warped_motion = 0;
    dec->enable_dual_filter = 0;
    dec->enable_order_hint = 0;
    dec->enable_jnt_comp = 0;
    dec->enable_ref_frame_mvs = 0;
    dec->force_screen_content_tools = 2;
    dec->force_integer_mv = 2;
    dec->order_hint_bits_minus1 = -1;
  }

  dec->enable_superres = SwGetBits(rb, 1);
  dec->enable_cdef = SwGetBits(rb, 1);
  dec->enable_restoration = SwGetBits(rb, 1);

  u32 high_bitdepth = SwGetBits(rb, 1);
  if (dec->vp_profile == 2 && high_bitdepth) {
    u32 twelve_bit = SwGetBits(rb, 1);
    dec->bit_depth = twelve_bit ? 12 : 10;
  } else if (dec->vp_profile <= 2) {
    dec->bit_depth = high_bitdepth ? 10 : 8;
  }

  if (dec->vp_profile == 1) {
    dec->monochrome = 0;
  } else {
    dec->monochrome = SwGetBits(rb, 1);
  }

  // color_description_present_flag
  if (SwGetBits(rb, 1)) {
    dec->color_primaries = SwGetBits(rb, 8);
    dec->transfer_characteristics = SwGetBits(rb, 8);
    dec->matrix_coefficients = SwGetBits(rb, 8);
  } else {
    dec->color_primaries = AOM_CICP_CP_UNSPECIFIED;
    dec->transfer_characteristics = AOM_CICP_TC_UNSPECIFIED;
    dec->matrix_coefficients = AOM_CICP_MC_UNSPECIFIED;
  }

  if (dec->monochrome) {
    dec->color_range = (enum DecVideoRange)SwGetBits(rb, 1);
    dec->subsampling_x = 1;
    dec->subsampling_y = 1;
    dec->chroma_sample_position = 0;
    dec->separate_uv_delta_q = 0;
  } else {
    if (dec->color_primaries == AOM_CICP_CP_BT_709 &&
        dec->transfer_characteristics == AOM_CICP_TC_SRGB &&
        dec->matrix_coefficients == AOM_CICP_MC_IDENTITY) {
      dec->color_range = DEC_VIDEO_FULL_SWING;
      dec->subsampling_x = 0;
      dec->subsampling_y = 0;
    } else {
      dec->color_range = (enum DecVideoRange)(SwGetBits(rb, 1));
      if (dec->vp_profile == 0) {
        dec->subsampling_x = 1;
        dec->subsampling_y = 1;
      } else if (dec->vp_profile == 1) {
        dec->subsampling_x = 0;
        dec->subsampling_y = 0;
      } else {
        if (dec->bit_depth == 12) {
          dec->subsampling_x = SwGetBits(rb, 1);
          if (dec->subsampling_x)
            dec->subsampling_y = SwGetBits(rb, 1);
          else
            dec->subsampling_y = 0;
        } else {
          dec->subsampling_x = 1;
          dec->subsampling_y = 0;
        }
      }

      if (dec->subsampling_x && dec->subsampling_y)
        dec->chroma_sample_position = SwGetBits(rb, 2);
    }
    dec->separate_uv_delta_q = SwGetBits(rb, 1);
  }

  dec->film_grain_params_present = SwGetBits(rb, 1);

  return HANTRO_OK;
}

bool IsObuInCurrentOperatingPoint(struct Av1Decoder *dec, obuHeader_t *hdr) {
  if (dec->current_operating_point == 0) return TRUE;
  if (((dec->current_operating_point >> hdr->temporal_layer_id) & 0x1) &&
      ((dec->current_operating_point >> (hdr->spatial_layer_id + 8)) & 0x1))
    return TRUE;
  if (dec->oppoints)
    return TRUE;
  else
    return FALSE;
}

i32 Av1CalcTileOffsets(struct Av1Decoder *dec, const u8 *base, int offset,
                       int tile_start, int tile_end, int len, const u8 *buf,
                       int buf_len) {
  for (int tile_id = tile_start; tile_id <= tile_end; tile_id++) {
    if (tile_id == dec->tile_end) {
      dec->tile_offset_start[tile_id] = offset;
      dec->tile_offset_end[tile_id] = len;
    } else {
      int size = 1;
      for (u32 i = 0; i <= dec->tile_sz_mag && offset < len; i++) {
        if ((base + offset) >= (buf + buf_len))
          size += (u32)(*(base + offset - buf_len)) << (i * 8);
        else
          size += (u32)(*(base + offset)) << (i * 8);
        offset++;
      }
      if (offset >= len || offset + size >= len || size >= len || size < 1) {
        return HANTRO_NOK;
      }
      dec->tile_offset_start[tile_id] = offset;
      dec->tile_offset_end[tile_id] = offset + size;
      offset += size;
    }
    if (dec->tile_offset_end[tile_id] <= dec->tile_offset_start[tile_id])
      return HANTRO_NOK;
  }
  return HANTRO_OK;
}

i32 padding_check(struct StrmData *rb, const u8 *strm, int decoded_payload_size,
                  int payload_size) {
  while (decoded_payload_size < payload_size) {
    u8 padding_byte;
    padding_byte = SwGetBits(rb, 8);
    decoded_payload_size++;
    if (padding_byte != 0) {
      return HANTRO_NOK;
    }
  }
  return HANTRO_OK;
}

i32 av1dec_check_trailing_bits(struct StrmData *rb) {
  int bits_before_alignment = 8 - rb->strm_buff_read_bits % 8;
  int trailing = SwGetBits(rb, bits_before_alignment);
  if (trailing != (1 << (bits_before_alignment - 1))) {
    return HANTRO_NOK;
  }
  return HANTRO_OK;
}

i32 Av1DecodeObuHeaders(struct Av1DecContainer *dec_cont, const u8 *strm,
                        u32 data_len, const struct Av1DecInput *input) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct StrmData rb;
  int i;
  u32 tmp;
  const u8 *buf = input->buffer;
  u32 buf_len = input->buff_len;

  dec->frame_tag_size = 0;
  dec->show_existing_frame = 0;

  obuHeader_t *hdr = &dec->obu_hdr;
  DWLmemset(&dec->obu_hdr, 0, sizeof(obuHeader_t));

  rb.strm_buff_start = buf;
  rb.strm_curr_pos = strm;
  rb.bit_pos_in_word = 0;
  rb.strm_buff_size = buf_len;
  rb.strm_data_size = data_len;
  rb.strm_buff_read_bits = 0;
  rb.remove_emul3_byte = 1;
  rb.emul_byte_count = 0;
  rb.is_rb = ((strm + data_len) > (buf + buf_len)) ? 1 : 0;
  rb.remove_avs_fake_2bits = 0;
  rb.stream_info = &dec_cont->llstrminfo.stream_info;

  // const u8 *data_end = strm + data_len;
  bool got_frame_header = 0;
  int obu_len = 0;
  int n_tile_groups = 0;
  bool last_tile_group = 0;
  bool hdr_done = 0;
  i32 ret = DEC_OK;

  dec->tile_end = -1;
  int size = 0;
  int frame_size = 0;

  while (1) {
    if (data_len <= 0) break;
    if (dec_cont->annexb) {
      int l = 0;
      size = leb128(&rb, &l);
      obu_len += l;
      // strm += l;
      data_len -= l;
    }
    ret = ReadObuHeader(&rb, hdr, dec_cont->annexb, size, TRUE, dec_cont->heif_mode);
    if (ret != HANTRO_OK) return DEC_STRM_ERROR;
    // strm += hdr->header_size;
    data_len -= hdr->header_size;
    obu_len += hdr->header_size;

    if (hdr->type != OBU_TEMPORAL_DELIMITER &&
        hdr->type != OBU_SEQUENCE_HEADER && hdr->type != OBU_PADDING) {
      if (!IsObuInCurrentOperatingPoint(dec, hdr)) {
        int l = hdr->payload_size;
        while (l > 0) {
          ret = SwFlushBits(&rb, 8);
          if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          l--;
        }
        // strm += hdr->payload_size;
        data_len -= hdr->payload_size;
        obu_len += hdr->payload_size;
        continue;
      }
    }

    // rb = {strm, strm, 0, data_len, 0, 1, 0};
    // rb.strm_buff_start = strm;
    // rb.strm_curr_pos = strm;
    // rb.bit_pos_in_word = 0;
    // rb.strm_buff_size = data_len;
    // rb.strm_data_size = data_len;
    // rb.strm_buff_read_bits = 0;
    // rb.remove_emul3_byte = 1;
    // rb.emul_byte_count = 0;
    // rb.is_rb = 0;
    // rb.remove_avs_fake_2bits = 0;
    tmp = rb.strm_buff_read_bits;
    strm = rb.strm_curr_pos;
    switch (hdr->type) {
      case OBU_TEMPORAL_DELIMITER: {
        for (i = 0; i < dec->input_sequence_num; i++) {
          if (dec->seq_hdr[i] != NULL) DWLfree(dec->seq_hdr[i]);
        }
        dec->input_sequence_num = 0;
        got_frame_header = 0;
        break;
      }
      case OBU_SEQUENCE_HEADER: {
        // rb.strm_buff_size = hdr->payload_size;
        ret = ObuSequenceHeader(&rb, dec_cont);
        if (ret != HANTRO_OK) return DEC_STRM_ERROR;
        if (av1dec_check_trailing_bits(&rb) != HANTRO_OK) return DEC_STRM_ERROR;
        u32 payload_bits = hdr->payload_size * 8;
        u32 read_bits = rb.strm_buff_read_bits - tmp;
        if (payload_bits < read_bits) return DEC_STRM_ERROR;
        if (padding_check(&rb, strm, (read_bits + 7) / 8, hdr->payload_size) !=
            HANTRO_OK)
          return DEC_STRM_ERROR;
        dec->seq_hdr[dec->input_sequence_num] =
            DWLcalloc(1, sizeof(obuSequenceHeaderInput_t));
        if (dec->input_sequence_num < 15 && got_frame_header == FALSE) {
          Av1SaveSequenceHeader(dec, dec->seq_hdr[dec->input_sequence_num]);
          dec->input_sequence_num++;
        }
        if (got_frame_header) {
          // restore sequence header.
          Av1RestoreSequenceHeader(dec, &dec->seq_frame_hdr);
        }
        break;
      }
      case OBU_FRAME_HEADER:
      case OBU_FRAME: {
        if (hdr->type != OBU_FRAME &&
            got_frame_header)  // Skip frame header if we already have one
          break;

        ret = Av1DecodeFrameTag(&rb, dec_cont);
        if (ret != HANTRO_OK) return DEC_STRM_ERROR;
        got_frame_header = TRUE;
        Av1SaveSequenceHeader(dec, &dec->seq_frame_hdr);
        if (hdr->type == OBU_FRAME) {
          if (rb.bit_pos_in_word > 0) {
            ret = SwFlushBits(&rb, 8 - rb.bit_pos_in_word);
            if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          }
          // strm = rb.strm_curr_pos;
          // rb = {strm + dec->frame_tag_size, strm + dec->frame_tag_size, 0,
          // data_len - dec->frame_tag_size, 0, 1, 0};
          // rb.strm_buff_start = strm + dec->frame_tag_size;
          // rb.strm_curr_pos = strm + dec->frame_tag_size;
          // rb.bit_pos_in_word = 0;
          // rb.strm_buff_size = data_len - dec->frame_tag_size;
          // rb.strm_data_size = data_len - dec->frame_tag_size;
          // rb.strm_buff_read_bits = 0;
          // rb.remove_emul3_byte = 1;
          // rb.emul_byte_count = 0;
          // rb.is_rb = 0;
          // rb.remove_avs_fake_2bits = 0;
          ret = Av1DecodeTileGroupHeader(&rb, dec_cont, &last_tile_group, 1);
          if (ret != HANTRO_OK && !last_tile_group) return DEC_STRM_ERROR;
          dec->frame_tag_size += dec->tile_group_hdr_size;
          const u8 *base = rb.strm_curr_pos + dec->tile_group_hdr_size;
          u32 read_bits = rb.strm_buff_read_bits + dec->tile_group_hdr_size * 8;
          if (base >= (buf + buf_len)) base -= buf_len;

          int len = (data_len < hdr->payload_size ? data_len : hdr->payload_size) - dec->frame_tag_size;
          if (len < 0 || (len + (read_bits >> 3)) > rb.strm_data_size)
            return DEC_STRM_ERROR;
          if (!dec_cont->low_latency) {
            ret = Av1CalcTileOffsets(dec, base, 0, dec->tile_start, dec->tile_end,
                                     len, buf, buf_len);
            if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          } else {
            dec_cont->lltileinfo.tile_cols = dec->av1_tile_cols;
            dec_cont->lltileinfo.tile_rows = dec->av1_tile_rows;
            dec_cont->lltileinfo.cur_rows = 0;
            dec_cont->lltileinfo.cur_cols = 0;
            dec_cont->llhrddata = rb;
            dec_cont->llhrddata.strm_buff_read_bits += dec->tile_group_hdr_size * 8;
            dec_cont->lltileinfo.tile_id = 0;
            dec_cont->lltileinfo.tile_size = 0;
            dec_cont->lltileinfo.last_tile_group = last_tile_group;
            dec_cont->lltileinfo.strm_next_address = (u8 *)rb.strm_curr_pos + dec->tile_group_hdr_size;
            dec_cont->lltileinfo.payload_len = hdr->payload_size;
            dec_cont->lltileinfo.payload_len_lost = hdr->payload_size - dec->tile_group_hdr_size;
            dec_cont->lltileinfo.read_bits = read_bits;
            /* note: payload_size include hdr_size */
            dec_cont->llhrddata.strm_buff_read_bits += dec->tile_group_hdr_size * 8;
            /* set update_rdy first */
            dec_cont->lltileinfo.update_rdy = 0;
            dec_cont->lltileinfo.update_done = 0;
            /* data_len == frame_len */
            dec_cont->lltileinfo.frame_len = data_len;
            dec_cont->lltileinfo.buf_address = input->buffer;
            dec_cont->lltileinfo.buf_len = buf_len;
            /* next data is tile group hrd */
            dec_cont->lltileinfo.tile_group_hrd = 0;
            dec_cont->lltileinfo.obu_hrd = 0;
          }
          n_tile_groups = 1;
        } else {
          if (av1dec_check_trailing_bits(&rb) != HANTRO_OK)
            return DEC_STRM_ERROR;
          u32 read_bits = rb.strm_buff_read_bits - tmp;
          if (padding_check(&rb, strm, (read_bits + 7) / 8,
                            hdr->payload_size) != HANTRO_OK)
            return DEC_STRM_ERROR;
        }
        if (hdr->type == OBU_FRAME || dec->show_existing_frame) {
          hdr_done = TRUE;
#if 0
          if (rb.bit_pos_in_word > 0)
            SwFlushBits(&rb, 8 - rb.bit_pos_in_word);
          u32 read_bits = rb.strm_buff_read_bits - tmp;
          //strm += hdr->payload_size;
          int l = hdr->payload_size * 8 - read_bits/8;
          while (l > 0) {
            SwFlushBits(&rb, 8);
            l--;
          }
#else
          rb.strm_curr_pos = strm + hdr->payload_size;
          if (rb.strm_curr_pos >= (buf + buf_len)) rb.strm_curr_pos -= buf_len;
          rb.bit_pos_in_word = 0;
          rb.strm_buff_read_bits += hdr->payload_size * 8;
          ;
#endif
          if (data_len < hdr->payload_size)
            hdr->payload_size = data_len;
          data_len -= hdr->payload_size;
        }
        frame_size += hdr->payload_size;
        break;
      }
      case OBU_TILE_GROUP: {
        ret = Av1DecodeTileGroupHeader(&rb, dec_cont, &last_tile_group, 0);
        if (ret != HANTRO_OK) return DEC_STRM_ERROR;
        dec->frame_tag_size = dec->tile_group_hdr_size;
        const u8 *base = rb.strm_curr_pos + dec->tile_group_hdr_size;
        u32 read_bits = rb.strm_buff_read_bits + dec->tile_group_hdr_size * 8;
        if (base >= (buf + buf_len)) base -= buf_len;

        int len = hdr->payload_size - dec->frame_tag_size;

        if (len < 0 || (len + (read_bits >> 3)) > rb.strm_data_size)
          return DEC_STRM_ERROR;
        if (!dec_cont->low_latency) {
          ret = Av1CalcTileOffsets(dec, base, 0, dec->tile_start, dec->tile_end,
                                   len, buf, buf_len);
          if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          int l = hdr->payload_size;
          while (l > 0) {
            ret = SwFlushBits(&rb, 8);
            if (ret != HANTRO_OK) return DEC_STRM_ERROR;
            l--;
          }
        } else {
          dec_cont->lltileinfo.tile_cols = dec->av1_tile_cols;
          dec_cont->lltileinfo.tile_rows = dec->av1_tile_rows;
          dec_cont->lltileinfo.cur_rows = 0;
          dec_cont->lltileinfo.cur_cols = 0;
          dec_cont->llhrddata = rb;
          dec_cont->llhrddata.strm_buff_read_bits += dec->tile_group_hdr_size * 8;
          dec_cont->lltileinfo.tile_id = 0;
          dec_cont->lltileinfo.tile_size = 0;
          dec_cont->lltileinfo.last_tile_group = last_tile_group;
          dec_cont->lltileinfo.strm_next_address = (u8 *)rb.strm_curr_pos + dec->tile_group_hdr_size;
          dec_cont->lltileinfo.payload_len = hdr->payload_size;
          dec_cont->lltileinfo.payload_len_lost = hdr->payload_size - dec->tile_group_hdr_size;
          dec_cont->lltileinfo.read_bits = read_bits;
          /* note: payload_size include hdr_size */
          dec_cont->llhrddata.strm_buff_read_bits += dec->tile_group_hdr_size * 8;
          /* set update_rdy first */
          dec_cont->lltileinfo.update_rdy = 0;
          dec_cont->lltileinfo.update_done = 0;
          /* data_len == frame_len */
          dec_cont->lltileinfo.frame_len = data_len;
          dec_cont->lltileinfo.buf_address = input->buffer;
          dec_cont->lltileinfo.buf_len = buf_len;
          /* next data is tile group hrd */
          dec_cont->lltileinfo.tile_group_hrd = 0;
          dec_cont->lltileinfo.obu_hrd = 0;
        }

        // strm += hdr->payload_size;
        data_len -= hdr->payload_size;
        n_tile_groups = 1;
        if (ret != HANTRO_OK) return DEC_STRM_ERROR;
        frame_size += hdr->payload_size;
        while (!last_tile_group && !dec_cont->low_latency) {
          // Keep reading until we reach last tile group header
          if (dec_cont->annexb) {
            int l = 0;
            size = leb128(&rb, &l);
            // strm += l;
            data_len -= l;
          }
          ret = ReadObuHeader(
              &rb, hdr, dec_cont->annexb,
              size - (dec_cont->annexb ? 0 : dec->tile_group_hdr_size), TRUE, dec_cont->heif_mode);
          // strm += hdr->header_size;
          data_len -= hdr->header_size;
          frame_size += hdr->total_size;
          if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          if (hdr->type == OBU_TILE_GROUP) {
            // rb = {strm, strm, 0, data_len, 0, 1, 0};
            // rb.strm_buff_start = strm;
            // rb.strm_curr_pos = strm;
            // rb.bit_pos_in_word = 0;
            // rb.strm_buff_size = data_len;
            // rb.strm_data_size = data_len;
            // rb.strm_buff_read_bits = 0;
            // rb.remove_emul3_byte = 1;
            // rb.emul_byte_count = 0;
            // rb.is_rb = 0;
            // rb.remove_avs_fake_2bits = 0;
            ret = Av1DecodeTileGroupHeader(&rb, dec_cont, &last_tile_group, 0);
            if (ret != HANTRO_OK) return DEC_STRM_ERROR;
            // int offset = (strm - base) + dec->tile_group_hdr_size;
            int offset = (rb.strm_buff_read_bits - read_bits +
                          dec->tile_group_hdr_size * 8) >>
                         3;
            // int end = (strm - base) + hdr->payload_size;
            int end =
                ((rb.strm_buff_read_bits - read_bits) >> 3) + hdr->payload_size;
            if (end + ((read_bits >> 3)) > rb.strm_data_size)
              return DEC_STRM_ERROR;

            ret = Av1CalcTileOffsets(dec, base, offset, dec->tile_start,
                                     dec->tile_end, end, buf, buf_len);
            n_tile_groups++;
            if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          }

          int l = hdr->payload_size;
          while (l > 0) {
            ret = SwFlushBits(&rb, 8);
            if (ret != HANTRO_OK) return DEC_STRM_ERROR;
            l--;
          }

          // strm += hdr->payload_size;
          data_len -= hdr->payload_size;
        }
        hdr_done = TRUE;
        break;
      }
      case OBU_METADATA: {
#ifdef SUPPORT_METADATA
        ret = Av1LoadMetadataBuffer(dec_cont, input->pic_id);
        if(ret != HANTRO_OK) return ret;
        ret = Av1DecMetadataParse(dec_cont->metadata_param_curr, &rb, hdr);
        if(ret != HANTRO_OK) return DEC_STRM_ERROR;
        break;
#endif
      }
      case OBU_PADDING:
      case OBU_REDUNDANT_FRAME_HEADER:
      default: {
        int l = hdr->payload_size;
        while (l > 0) {
          ret = SwFlushBits(&rb, 8);
          if (ret != HANTRO_OK) return DEC_STRM_ERROR;
          l--;
        }
        break;
      }
    }

    if (hdr_done) {
      // frame decoded, allow extra zero bytes after
      while (data_len && ((int)(SwShowBits(&rb, 8)) == 0) && !dec_cont->low_latency) {
        ret = SwFlushBits(&rb, 8);
        if (ret) {
          ret = HANTRO_OK;
          break;
        }
        data_len--;
        frame_size++;
      }
      if (!dec_cont->low_latency)
        dec->frame_size = MIN(input->data_len, obu_len + frame_size);
      break;
    }
    // strm += hdr->payload_size;
    if (data_len < hdr->payload_size)
      hdr->payload_size = data_len;
    data_len -= hdr->payload_size;
    obu_len += hdr->payload_size;
  }
  if (dec->frame_size == 0)
    dec->frame_size = MIN(input->data_len, obu_len + frame_size);
  // Add offset to tile data
  dec->frame_tag_size += obu_len;

  return ret;
}

i32 Av1DecodeHeaders(struct Av1DecContainer *dec_cont,
                     const struct Av1DecInput *input) {
  i32 ret;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  struct Av1Decoder *dec = &dec_cont->decoder;
  dec->tile_start_concealment = -1;
  dec_cont->prev_is_monochrome = dec->monochrome;
  dec_cont->prev_is_key = dec->key_frame;
  dec->prev_is_key_frame = dec->key_frame;
  dec->prev_show_frame = dec->show_frame;
  dec->probs_decoded = 0;
  dec->last_width = dec->width;
  dec->last_superres_width = dec->superres_width;
  dec->last_height = dec->height;
  dec->frame_tag_decoded = 0;

  if (dec->show_frame)
    dec->current_video_frame++;

  const u8 *strm = input->stream;
  u32 data_len = input->data_len;
  if (dec_cont->low_latency)
    data_len = input->frame_len;

  ret = Av1DecodeObuHeaders(dec_cont, strm, data_len, input);

  if (ret == DEC_STRM_ERROR) return ret;
  if (ret != HANTRO_OK) {
    if ((dec_cont->pic_number == 1 && !dec->prev_is_key_frame) ||
        dec_cont->dec_stat != AV1DEC_DECODING) {
      return DEC_STRM_ERROR;
    }
    else if (ret == DEC_NO_DECODING_BUFFER)
      return ret;
    else {
      Av1Freeze(dec_cont);
      return DEC_PIC_DECODED;
    }
  } else if (dec->show_existing_frame) {
    asic_buff->out_buffer_i =
        Av1BufferQueueGetRef(dec_cont->bq, dec->show_existing_frame_index);
#ifdef CASE_INFO_STAT
    case_info.show_existing_frame = 1;
#endif
    if (asic_buff->out_buffer_i == AV1_UNDEFINED_BUFFER)
      return DEC_STRM_ERROR;
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[asic_buff->out_buffer_i]) {
        return DEC_STRM_PROCESSED;
    }
#endif
    Av1BufferQueueAddRef(dec_cont->bq, asic_buff->out_buffer_i);
    Av1BufferQueueAddRef(dec_cont->pp_bq,
                         asic_buff->pp_buffer_map[asic_buff->out_buffer_i]);
    Av1SetupPicToOutput(dec_cont, input->pic_id);
    asic_buff->out_buffer_i = -1;
    Av1PicToOutput(dec_cont, 0);
    return DEC_PIC_DECODED;
  }

  /* Decode frame header (now starts bool coder as well) */
  //  u32 header_size = input->data_len - dec->frame_tag_size;

  ret = Av1SetPartitionOffsets(input->stream, input->data_len,
                               &dec_cont->decoder);

  /* ignore errors in partition offsets if HW error concealment used
   * (assuming parts of stream missing -> partition start offsets may
   * be larger than amount of stream in the buffer) */
  if (ret != HANTRO_OK && !dec_cont->low_latency) {
    if ((dec_cont->pic_number == 1 && !dec->prev_is_key_frame) ||
        dec_cont->dec_stat != AV1DEC_DECODING) {
      return DEC_STRM_ERROR;
    } else {
      Av1Freeze(dec_cont);
      return DEC_PIC_DECODED;
    }
  }

  asic_buff->height = NEXT_MULTIPLE(dec->height, 8);
  asic_buff->width = NEXT_MULTIPLE(dec_cont->decoder.superres_width, 8);
  /* If the frame dimensions are not supported by HW,
     release allocated picture buffers and return error */
  if (((dec_cont->width != dec->width) || (dec_cont->height != dec->height) ||
       (dec_cont->superres_width != dec->superres_width) ||
       (dec_cont->bit_depth != dec->bit_depth))) {
    SwAdjustCoreMaskByWxH(dec_cont->dwl, asic_buff->width, asic_buff->height,
                          1, DWL_CLIENT_TYPE_AV1_DEC, &dec_cont->core_mask);
    if (CORE_MASK(dec_cont->core_mask) == 0) {
      APITRACEERR("%s","Av1DecodeHeaders# no any core mask support the header info\n");
      return DEC_STREAM_NOT_SUPPORTED;
    }
    dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask),
                                          DWL_CLIENT_TYPE_AV1_DEC);
    if (Av1CheckSupport(dec_cont) != HANTRO_OK) {
      // Av1AsicReleaseFilterBlockMem(dec_cont->dwl, dec_cont->asic_buff);
      // Av1AsicReleasePictures(dec_cont);
      dec_cont->dec_stat = AV1DEC_INITIALIZED;
      return DEC_STREAM_NOT_SUPPORTED;
    }
  }

  /* If the frame dimensions chnaged or bit depth is changed, update PP
   * parameters */
  if (((dec_cont->superres_width != dec->superres_width) ||
       (dec_cont->height != dec->height) ||
       (dec_cont->bit_depth != dec->bit_depth)) &&
      dec_cont->pp_enabled) {
    PpUnitSetIntConfig(dec_cont->ppu_cfg, dec_cont->reserved_ppu_cfg, dec_cont->hw_feature,
      dec->bit_depth, 1, dec_cont->decoder.monochrome);
    u32 pic_width = dec_cont->decoder.superres_is_scaled
                        ? dec_cont->decoder.superres_width
                        : dec_cont->decoder.width;

    PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
    u32 i = 0;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled) continue;

      /* flexible scale ratio */
      if (!ppu_cfg->crop.set_by_user) {
        /*support odd crop*/
        if(dec_cont->hw_feature->crop_step_rshift) {
          ppu_cfg->crop.width = (pic_width + 1) & ~0x1;
          ppu_cfg->crop.height = (dec->height + 1) & ~0x1;
        } else {
          ppu_cfg->crop.width = pic_width;
          ppu_cfg->crop.height = dec->height;
        }
      }
      if (ppu_cfg->scale.scale_by_ratio && ppu_cfg->scale.ratio_x) {
        /*support odd crop*/
        if(dec_cont->hw_feature->crop_step_rshift) {
          ppu_cfg->scale.width =
            DOWN_SCALE_SIZE(ppu_cfg->crop.width, ppu_cfg->scale.ratio_x);
          ppu_cfg->scale.height =
            DOWN_SCALE_SIZE(ppu_cfg->crop.height, ppu_cfg->scale.ratio_y);
        } else {
          ppu_cfg->scale.width = ppu_cfg->crop.width / ppu_cfg->scale.ratio_x;
          ppu_cfg->scale.height = ppu_cfg->crop.height / ppu_cfg->scale.ratio_y;
        }
      } else if (!ppu_cfg->scale.enabled) {
        ppu_cfg->scale.width = ppu_cfg->crop.width;
        ppu_cfg->scale.height = ppu_cfg->crop.height;
      }
    }
    /*support odd crop*/
    if (dec_cont->hw_feature->crop_step_rshift) {
      if (CheckPpUnitConfig(dec_cont->hw_feature, ((pic_width + 1) & ~0x1), ((dec->height + 1) & ~0x1),
                            0, dec->bit_depth, dec_cont->decoder.monochrome ? PP_CHROMA_400: PP_CHROMA_420, dec_cont->ppu_cfg))
        return DEC_INFOPARAM_ERROR;
    } else {
      if (CheckPpUnitConfig(dec_cont->hw_feature, pic_width, dec->height,
                            0, dec->bit_depth, dec_cont->decoder.monochrome ? PP_CHROMA_400: PP_CHROMA_420, dec_cont->ppu_cfg))
        return DEC_INFOPARAM_ERROR;
    }

    //if ((dec->last_superres_width != dec->superres_width) ||
    //    (dec->last_height != dec->height))
      CalcPpUnitBufferSize(&dec_cont->ppu_cfg[0], 0);
  }

  /* if there is a chroma sample changed,color-to-monochrome jump or monochrome-to-color jump */
  if(dec_cont->prev_is_monochrome != dec_cont->decoder.monochrome){
    if(!dec_cont->prev_is_monochrome){  /*color-to-monochrome jump,exit  to avoid HW IRQ TIMEOUT*/
      for(int i = 0;i<DEC_MAX_PPU_COUNT;i++){
        dec_cont->ppu_cfg[i].monochrome = dec_cont->decoder.monochrome;
      }
    }
    else if(dec_cont->prev_is_monochrome){  /*monochrome-to-color jump,reset ppconfig*/
      PpUnitSetIntConfig(dec_cont->ppu_cfg, dec_cont->reserved_ppu_cfg,
        dec_cont->hw_feature, dec_cont->decoder.bit_depth, 1, 0);
    }
    u32 pic_width = dec_cont->decoder.superres_is_scaled
                    ? dec_cont->decoder.superres_width
                    : dec_cont->decoder.width;
    if (CheckPpUnitConfig(dec_cont->hw_feature,  ((pic_width + 1) & ~0x1),  ((dec_cont->decoder.height + 1) & ~0x1),
                      0, dec_cont->decoder.bit_depth,
                      dec_cont->decoder.monochrome ? PP_CHROMA_400: PP_CHROMA_420, dec_cont->ppu_cfg))
      return DEC_INFOPARAM_ERROR;
  }


  dec_cont->width = dec->width;
  dec_cont->superres_width = dec->superres_width;
  dec_cont->height = dec->height;
  dec_cont->bit_depth = dec->bit_depth;

  /* If we are here and dimensions are still 0, it means that we have
   * yet to decode a valid keyframe, in which case we must give up. */
  if (dec_cont->width == 0 || dec_cont->height == 0 ||
      !dec->frame_tag_decoded) {
    return DEC_STRM_PROCESSED;
  }

  if (dec_cont->dec_stat == AV1DEC_INITIALIZED) {
    dec_cont->dec_stat = AV1DEC_NEW_HEADERS;
    return DEC_HDRS_RDY;
  }

  // /* If output picture is broken and we are not decoding a base frame,
  //  * don't even start HW, just output same picture again. */
  // if (!dec->key_frame && dec_cont->picture_broken &&
  //     (dec_cont->intra_freeze || dec_cont->force_intra_freeze)) {
  //   if (dec->show_frame) {
  //     Av1PicToOutput(dec_cont);
  //   }
  //   Av1Freeze(dec_cont);
  //   return DEC_PIC_DECODED;
  // }

  return DEC_OK;
}

i32 Av1AsicAllocateFakeTableMem(struct Av1DecContainer *dec_cont) {
  i32 dwl_ret;
  u32 size;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

#ifdef USE_FAKE_RFC_TABLE
  if (dec_cont->use_video_compressor) {
    /* Allocate and initialize fake RFC table for robustness. */
    u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
    u32 pic_width_in_cbsy, pic_height_in_cbsy;
    u32 pic_width_in_cbsc, pic_height_in_cbsc;
    u32 luma_table_size, chroma_table_size;
    u32 bit_depth = dec_cont->decoder.bit_depth;

    pic_width_in_cbsy = (asic_buff->width + 8 - 1)/8;
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = (asic_buff->width + 16 - 1)/16;
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (asic_buff->height + 8 - 1)/8;
    pic_height_in_cbsc = (asic_buff->height/2 + 4 - 1)/4;

    /* luma table size */
    luma_table_size = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, ref_buffer_align);
    /* chroma table size */
    chroma_table_size = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, ref_buffer_align);
    size = luma_table_size + chroma_table_size;

    /* frame resolution changed. */
    if (size != asic_buff->tbl_sizey + asic_buff->tbl_sizec) {
      asic_buff->tbl_sizey = luma_table_size;
      asic_buff->tbl_sizec = chroma_table_size;
      if (size > asic_buff->fake_rfc_tbl.size) {
        /* need reallocate */
        if (asic_buff->fake_rfc_tbl.virtual_address != NULL) {
#ifdef ASIC_TRACE_SUPPORT
          DWLFreeRefFrm(dec_cont->dwl, &asic_buff->fake_rfc_tbl);
#else
          DWLFreeLinear(dec_cont->dwl, &asic_buff->fake_rfc_tbl);
#endif
          asic_buff->fake_rfc_tbl.virtual_address = NULL;
        }
        asic_buff->fake_rfc_tbl.mem_type = DWL_MEM_TYPE_VPU_WORKING | DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
        SET_MEM_USAGE(asic_buff->fake_rfc_tbl.mem_type, DWL_MEM_USAGE_TMP_FAKERFC_TABLE, dec_cont->secure_mode);
#ifdef ASIC_TRACE_SUPPORT
        dwl_ret = DWLMallocRefFrm(dec_cont->dwl, size, &asic_buff->fake_rfc_tbl);
#else
        dwl_ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->fake_rfc_tbl);
#endif
        if (dwl_ret) return -1;
      }
      GenerateFakeRFCTable((u8 *)asic_buff->fake_rfc_tbl.virtual_address,
                            pic_width_in_cbsy, pic_height_in_cbsy,
                            pic_width_in_cbsc, pic_height_in_cbsc,
                            bit_depth, ref_buffer_align);
      DWLDMATransData(dec_cont->dwl, &asic_buff->fake_rfc_tbl, 0,
                      asic_buff->fake_rfc_tbl.size, HOST_TO_DEVICE);
    }
  }
#else
  (void)dwl_ret;
  (void)size;
  (void)asic_buff;
#endif

  return 0;
}

u32 Av1ReplaceRefAvalible(struct Av1DecContainer *dec_cont) {
  u32 i = 0, j = 0;
  i32 old_index = -1;
  u32 find_flags = 0;
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  i32 index_ref[MAX_ACTIVE_REFS];
  i32 index_info[MAX_ACTIVE_REFS];

  u32 tmp_pic_size = 0;
  u32 curr_pic_size = 0;

  /* get ref info. */
  for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    index_ref[i] = Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
  }
  if (!dec_cont->pp_enabled) {
    for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
      index_info[i] = index_ref[i];
    }
  } else {
    for (i = 0; i < AV1_ACTIVE_REFS_EX; i++)
      index_info[i] = Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
  }

  for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    if (asic_buff->picture_info[index_info[i]].error_info != DEC_NO_ERROR &&
        asic_buff->picture_info[index_info[i]].error_info != DEC_REF_ERROR) {
      /* this ref need replace. */
      find_flags = 0;

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

      /* check ref pic */
      for (j = 0; j < AV1_ACTIVE_REFS_EX; j++) {
        /* skip itself */
        if (index_info[i] == index_info[j]) continue;

        tmp_pic_size = asic_buff->picture_info[index_info[j]].coded_width *
                       asic_buff->picture_info[index_info[j]].coded_height;
        if (tmp_pic_size == curr_pic_size) {
          if (dec_cont->use_video_compressor) {
            if (asic_buff->picture_info[index_info[j]].error_info == DEC_NO_ERROR ||
                asic_buff->picture_info[index_info[j]].error_info == DEC_REF_ERROR) {
              find_flags = 1;
              break;
            }
          } else {
            /* NO_RFC: if error_ratio less than 10%, use it as correct. */
            if (asic_buff->picture_info[index_info[j]].error_ratio <= EC_RATIO_THRESHOLD) {
              find_flags = 1;
              break;
            }
          }
        }
      }
      if (find_flags) continue;

      /* current error ref can't find replaced ref. */
      return HANTRO_NOK;
    }
  }
  /* all error ref can find replaced ref. */
  return HANTRO_OK;
}

enum DecRet Av1DecSetInfo(Av1DecInst dec_inst, struct Av1DecConfig *dec_cfg) {
  struct Av1DecContainer *dec_cont;// = (struct Av1DecContainer *)dec_inst;
  u32 pic_width, pic_height, pixel_width;
  u32 i, size;
  PpUnitConfig *ppu_cfg;

  if (dec_inst == NULL || dec_cfg == NULL)
    return (DEC_PARAM_ERROR);

  dec_cont = (struct Av1DecContainer *)dec_inst;
  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont)
    return DEC_NOT_INITIALIZED;

  if (CORE_MASK(dec_cfg->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(dec_cont->core_mask);
    dec_cont->core_mask = CORE_MASK(dec_cfg->misc_ctrl);
    dec_cont->core_mask |= bak_non_core_mask;
  }
  dec_cont->hw_feature = SwGetHwFeature(dec_cont->dwl, CORE_MASK(dec_cont->core_mask), DWL_CLIENT_TYPE_AV1_DEC);
  if (!dec_cont->hw_feature) {
    APITRACEDEBUG("%s","Av1DecSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
  }

  pic_width = dec_cont->decoder.superres_is_scaled
                      ? dec_cont->decoder.superres_width
                      : dec_cont->decoder.width;
  pic_height = dec_cont->decoder.height;
  pixel_width = dec_cont->decoder.bit_depth;
  ppu_cfg = dec_cfg->ppu_cfg;

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
    dec_cont->ppu_cfg[i].source_range = dec_cont->decoder.color_range;
    // when input stream, rgb source range follows yuv source range, which get from stream sps info
  }

#ifdef MODEL_SIMULATION
  cmodel_ref_buf_alignment = MAX(16, ALIGN(dec_cont->align));
#endif
  /* Considering the case that the resolution is odd. */
  if (dec_cont->hw_feature->crop_step_rshift) {
    pic_width = (pic_width + 1) & (~0x1);
    pic_height = (pic_height + 1) & (~0x1);
  }

  DWLmemcpy(dec_cont->reserved_ppu_cfg, dec_cfg->ppu_cfg, sizeof(dec_cfg->ppu_cfg));
  PpUnitSetIntConfig(dec_cont->ppu_cfg, ppu_cfg,
    dec_cont->hw_feature, pixel_width, 1, dec_cont->decoder.monochrome);

  dec_cont->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
    dec_cont->pp_enabled |= dec_cont->ppu_cfg[i].enabled;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].lanczos_table.virtual_address == NULL) {
      size = LANCZOS_COEFF_BUFFER_SIZE * sizeof(i16);
      dec_cont->ppu_cfg[i].lanczos_table.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(dec_cont->ppu_cfg[i].lanczos_table.mem_type, DWL_MEM_USAGE_IN_PPULANCZOS_TABLE,
                    dec_cont->secure_mode);
      i32 ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(dec_cont->hw_feature, pic_width, pic_height,
                        0, pixel_width,
                        dec_cont->decoder.monochrome ? PP_CHROMA_400: PP_CHROMA_420, dec_cont->ppu_cfg))
    return DEC_PARAM_ERROR;

  /* config error process policy. */
  if (dec_cont->error_handling == DEC_EC_FRAME_TOLERANT_ERROR) {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE | DEC_EC_NO_SKIP | DEC_EC_OUT_DECISION;
  } else if (dec_cont->error_handling == DEC_EC_FRAME_IGNORE_ERROR) {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_REPLACE_ANYWAY | DEC_EC_NO_SKIP | DEC_EC_OUT_ALL;
  } else {
    dec_cont->error_policy = DEC_EC_PIC_NO_RECOVERY | DEC_EC_REF_RESET | DEC_EC_SEEK_NEXT_I | DEC_EC_OUT_NO_ERROR;
  }

  /* config HWEC: AV1 always enable. */
  dec_cont->hw_conceal = HANTRO_TRUE;

  dec_cont->ext_buffer_config = 0;
  if (dec_cont->pp_enabled)
    dec_cont->ext_buffer_config = 1 << DOWNSCALE_OUT_BUFFER;
  else
    dec_cont->ext_buffer_config = 1 << REFERENCE_BUFFER;

  if (dec_cont->pp_bq == NULL) {
    if (dec_cont->pp_enabled) {
      dec_cont->pp_bq = Av1BufferQueueInitialize(0);
      if (dec_cont->pp_bq == NULL) {
        Av1BufferQueueRelease(dec_cont->pp_bq, 1);
        return DEC_MEMFAIL;
      }
    }
  }
  Av1SetExternalBufferInfo(dec_cont);
  return (DEC_OK);
}

void Av1DecUpdateStrmInfoCtrl(Av1DecInst dec_inst, struct strmInfo info) {
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
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

    Av1DecUpdateTileInfo(dec_inst);

    /* check hw status */
    if (!llstrminfo->first_update && !dec_cont->asic_running) {
      llstrminfo->tmp_length = llstrminfo->ll_strm_len;
      llstrminfo->update_reg_flag = 0;
      llstrminfo->updated_reg = 1;
    }
  }
}

void Av1DecUpdateTileInfo(Av1DecInst dec_inst) {
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct LLTileInfo *lltileinfo = &dec_cont->lltileinfo;
  struct LLStrmInfo *llstrminfo = &dec_cont->llstrminfo;
  int size = 0;
  i32 ret = DEC_OK;
  obuHeader_t *hdr = &dec->obu_hdr;

  if (lltileinfo->update_done)
    return;

  if (!lltileinfo->update_rdy)
    return;

  if (lltileinfo->obu_hrd) {
    int need_bits = 8;
    if (dec_cont->annexb) {
      /* leb128 */
      need_bits += 64;
    }
    /* has extension, has size field */
    need_bits += 8 + 64;
    if (((llstrminfo->stream_info.send_len * 8) < (dec_cont->llhrddata.strm_buff_read_bits + need_bits)) &&
        (!llstrminfo->stream_info.last_flag)) {
        goto update;
    }
    if (dec_cont->annexb) {
      int l = 0;
      size = leb128(&dec_cont->llhrddata, &l);
    }
    ret = ReadObuHeader(&dec_cont->llhrddata, hdr, dec_cont->annexb,
                        size, TRUE, dec_cont->heif_mode);
    if (ret != HANTRO_OK) {
      lltileinfo->update_done = 1;
      goto update;
    }
    if (hdr->type != OBU_TILE_GROUP) {
      dec_cont->llhrddata.strm_buff_read_bits += hdr->payload_size * 8;
      dec_cont->llhrddata.strm_curr_pos += hdr->payload_size;
      goto update;
    }
    /* payload size not include header size */
    lltileinfo->payload_len = hdr->payload_size;
    lltileinfo->payload_len_lost = hdr->payload_size;
    lltileinfo->tile_group_hrd = 1;
    lltileinfo->obu_hrd = 0;
  }

  if (lltileinfo->tile_group_hrd) {
    if (((llstrminfo->stream_info.send_len * 8) < (dec_cont->llhrddata.strm_buff_read_bits + 32 * 3)) &&
        (!llstrminfo->stream_info.last_flag)) {
        goto update;
    }
    ret = Av1DecodeTileGroupHeader(&dec_cont->llhrddata, dec_cont, &lltileinfo->last_tile_group, 0);
    if (ret != HANTRO_OK) lltileinfo->update_done = 1;
    dec->frame_tag_size += dec->tile_group_hdr_size;
    lltileinfo->tile_size += dec_cont->llhrddata.strm_curr_pos + dec->tile_group_hdr_size - lltileinfo->strm_next_address;
    lltileinfo->strm_next_address = (u8 *)dec_cont->llhrddata.strm_curr_pos + dec->tile_group_hdr_size;
    /* note: payload_size include hdr_size */
    dec_cont->llhrddata.strm_buff_read_bits += dec->tile_group_hdr_size * 8;
    lltileinfo->tile_group_hrd = 0;
  }

  while (((llstrminfo->stream_info.send_len) >= ((dec_cont->llhrddata.strm_buff_read_bits / 8) + dec->tile_sz_mag)) ||
         lltileinfo->tile_id == dec->tile_end) {
    if (lltileinfo->update_done)
      break;
    if (lltileinfo->tile_id == dec->tile_end) {
      dec->tile_offset_start[lltileinfo->tile_id] = lltileinfo->tile_size;
      dec->tile_offset_end[lltileinfo->tile_id] = dec->tile_offset_start[lltileinfo->tile_id] + lltileinfo->payload_len_lost;
      dec_cont->llhrddata.strm_buff_read_bits += lltileinfo->payload_len_lost * 8;
      dec_cont->llhrddata.strm_curr_pos += lltileinfo->payload_len;
      lltileinfo->payload_len_lost = 0;
      if (dec_cont->llhrddata.strm_curr_pos >= (lltileinfo->buf_address + lltileinfo->buf_len))
        dec_cont->llhrddata.strm_curr_pos -= lltileinfo->buf_len;
    } else if (lltileinfo->tile_id < dec->tile_end &&
               ((llstrminfo->stream_info.send_len) >= ((dec_cont->llhrddata.strm_buff_read_bits / 8) + dec->tile_sz_mag))) {
      int size = 1;
      for (u32 i = 0; i <= dec->tile_sz_mag; i++) {
        if (lltileinfo->strm_next_address >= (lltileinfo->buf_address + lltileinfo->buf_len))
          lltileinfo->strm_next_address -= lltileinfo->buf_len;
        size += (u32)(*lltileinfo->strm_next_address++) << (i * 8);
        lltileinfo->tile_size++;
        lltileinfo->payload_len_lost--;
        dec_cont->llhrddata.strm_buff_read_bits += 8;
      }
      dec->tile_offset_start[lltileinfo->tile_id] = lltileinfo->tile_size;
      dec->tile_offset_end[lltileinfo->tile_id] = lltileinfo->tile_size + size;
      lltileinfo->tile_size += size;
      lltileinfo->payload_len_lost -= size;
      dec_cont->llhrddata.strm_buff_read_bits += size * 8;
      lltileinfo->strm_next_address += size;
    }
    lltileinfo->cur_cols = lltileinfo->tile_id % dec->av1_tile_cols;
    lltileinfo->cur_rows = lltileinfo->tile_id / dec->av1_tile_cols;
    *lltileinfo->tileinfo++ = dec->tile_col_start_sb[lltileinfo->cur_cols + 1] - dec->tile_col_start_sb[lltileinfo->cur_cols];
    *lltileinfo->tileinfo++ = 0;
    *lltileinfo->tileinfo++ = 0;
    *lltileinfo->tileinfo++ = 0;
    *lltileinfo->tileinfo++ = dec->tile_row_start_sb[lltileinfo->cur_rows + 1] - dec->tile_row_start_sb[lltileinfo->cur_rows];
    *lltileinfo->tileinfo++ = 0;
    *lltileinfo->tileinfo++ = 0;
    *lltileinfo->tileinfo++ = 0;
    *lltileinfo->tileinfo++ = dec->tile_offset_start[lltileinfo->tile_id] & 255;
    *lltileinfo->tileinfo++ = (dec->tile_offset_start[lltileinfo->tile_id] >> 8) & 255;
    *lltileinfo->tileinfo++ = (dec->tile_offset_start[lltileinfo->tile_id] >> 16) & 255;
    *lltileinfo->tileinfo++ = (dec->tile_offset_start[lltileinfo->tile_id] >> 24) & 255;
    *lltileinfo->tileinfo++ = dec->tile_offset_end[lltileinfo->tile_id] & 255;
    *lltileinfo->tileinfo++ = (dec->tile_offset_end[lltileinfo->tile_id] >> 8) & 255;
    *lltileinfo->tileinfo++ = (dec->tile_offset_end[lltileinfo->tile_id] >> 16) & 255;
    *lltileinfo->tileinfo++ = (dec->tile_offset_end[lltileinfo->tile_id] >> 24) & 255;
    lltileinfo->tile_id++;
    if (lltileinfo->tile_id == (dec->tile_end + 1)) {
      if (lltileinfo->last_tile_group) {
        lltileinfo->update_done = 1;
      } else {
        lltileinfo->obu_hrd = 1;
      }
      break;
    }
  }

  /* no more new data for current frame */
  if (llstrminfo->stream_info.last_flag && (!lltileinfo->obu_hrd)) {
    lltileinfo->update_done = 1;
  }

update:

  SwUpdateTileInfoCtrl(lltileinfo);
}

enum DecRet Av1DecAddBuffer(Av1DecInst dec_inst, struct DWLLinearMem *info) {
  struct Av1DecContainer *dec_cont;// = (struct Av1DecContainer *)dec_inst;
  struct DecAsicBuffers *asic_buff;// = dec_cont->asic_buff;
  enum DecRet dec_ret = DEC_OK;

  if (dec_inst == NULL )
  	return DEC_PARAM_ERROR;
  dec_cont = (struct Av1DecContainer *)dec_inst;
  asic_buff = dec_cont->asic_buff;

  if (info == NULL ||
      DEC_CHECK_BUS_ADDRESS(info->bus_address) ||
      info->size < dec_cont->next_buf_size) {
    return DEC_PARAM_ERROR;
  }
  if (dec_cont->buffer_index >= MAX_PIC_BUFFERS)
    return DEC_EXT_BUFFER_REJECTED;

  APITRACEDEBUG("%s# adds external buffer with size %d\n","Av1DecAddBuffer",info->size);

  if (!dec_cont->pp_enabled) {
    dec_cont->add_buffer = 0;

    if (dec_cont->buffer_index == dec_cont->num_buffers) {
      /* Need to allocate a new buffer during decoding... */
      dec_cont->num_buffers++;
      dec_cont->add_buffer = 1;
    }
    ASSERT(dec_cont->buffer_index < dec_cont->num_buffers);

    asic_buff->pictures[dec_cont->buffer_index] = *info;

    if (dec_cont->next_dpb_parasitic_buf_size >
        asic_buff->dpb_parasitic_buf[dec_cont->buffer_index].size) {
      if (DWL_DEVMEM_VAILD(asic_buff->dpb_parasitic_buf[dec_cont->buffer_index]))
        Av1ReleaseParasiticBuf(dec_cont->dwl,
                              &asic_buff->dpb_parasitic_buf[dec_cont->buffer_index]);
      Av1AllocParasiticBuf(dec_cont->dwl, dec_cont->next_dpb_parasitic_buf_size,
                            &asic_buff->dpb_parasitic_buf[dec_cont->buffer_index],
                            dec_cont->secure_mode);
    }

    if (!dec_cont->asic_buff->realloc_out_buffer) {
      dec_cont->buffer_num_added++;
      dec_cont->buffer_index++;
    }
    if (dec_cont->buf_num) dec_cont->buf_num--;

    if (dec_cont->buffer_index >= dec_cont->num_buffers) {
      /* Need to add all the picture buffers in state AV1DEC_NEW_HEADERS. */
      if (dec_cont->add_buffer) {
        /* Need to allocate a new buffer during decoding... */
        Av1BufferQueueAddBuffer(dec_cont->bq);
        dec_cont->add_buffer = 0;
      }

      {
        dec_cont->next_buf_size = 0;
        dec_cont->buf_to_free = NULL;
        dec_cont->buf_not_added = 0;
      }
    } else {
      if (dec_cont->buffer_num_added >= dec_cont->num_buffers) {
        /* It's just reallocating old smaller picture. */
        dec_cont->buf_not_added = 0;
        dec_ret = DEC_OK;
      } else
        dec_ret = DEC_WAITING_FOR_BUFFER;
    }
  } else {
    asic_buff->pp_pictures[dec_cont->buffer_index] = *info;

    if (!dec_cont->asic_buff->realloc_out_buffer) {
      dec_cont->buffer_index++;
      dec_cont->buffer_num_added++;
      Av1BufferQueueAddBuffer(dec_cont->pp_bq);
      dec_cont->num_pp_buffers++;
    }

    if (dec_cont->dec_stat != AV1DEC_NEW_HEADERS ||
        dec_cont->buffer_index >= dec_cont->num_buffers) {
      dec_cont->buf_not_added = 0;
      dec_cont->next_buf_size = 0;
      dec_cont->buf_to_free = NULL;
    } else
      dec_ret = DEC_WAITING_FOR_BUFFER;
  }
  return dec_ret;
}

enum DecRet Av1DecGetBufferInfo(Av1DecInst dec_inst,
                                struct DecBufferInfo *mem_info) {
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
  struct DWLLinearMem empty = {NULL, 0, 0};
  u32 i;

  if (dec_inst == NULL || mem_info == NULL) {
    return DEC_PARAM_ERROR;
  }

  (void) DWLmemset(mem_info, 0, sizeof(struct DecBufferInfo));

  if (!dec_cont->pp_enabled) {
    u32 bit_depth = dec_cont->decoder.bit_depth;
    if (dec_cont->use_video_compressor) {
      mem_info->ystride[0] = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
      mem_info->cstride[0] = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
    } else {
      mem_info->cstride[0] = mem_info->ystride[0] =
                             NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
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
    dec_cont->buf_to_free = NULL;
    dec_cont->buf_not_added = 1;
  } else
    mem_info->buf_to_free = empty;

  mem_info->next_buf_size = dec_cont->next_buf_size;
  mem_info->buf_num = dec_cont->buf_num;

  APITRACEDEBUG("%s# requests %d external buffers with size of %d\n","Av1DecGetBufferInfo",mem_info->buf_num,
         mem_info->next_buf_size);

  return DEC_WAITING_FOR_BUFFER;
}

#ifdef CASE_INFO_STAT
void Av1CaseInfoCollect(struct Av1DecContainer *dec_cont, CaseInfo *case_info) {
  u32 pic_width, pic_height;
  u32 display_height, display_width;
  u32 raster_stride;

  pic_width = dec_cont->decoder.superres_is_scaled
                      ? dec_cont->decoder.superres_width
                      : dec_cont->decoder.width;
  display_width = pic_width;
  pic_width =  NEXT_MULTIPLE(pic_width, 8);
  pic_height = NEXT_MULTIPLE(dec_cont->decoder.height, 8);
  display_height = dec_cont->decoder.height;

  if (pic_width != display_width || pic_height != display_height)
    case_info->crop_flag = 1;

  case_info->min_cb_size = 3;
  case_info->max_cb_size = dec_cont->decoder.sb_size ? 7 : 6;
  case_info->pic_width_in_cbs =  ROUNDUPX(dec_cont->width, 8) >> 3;
  case_info->pic_height_in_cbs = ROUNDUPX(dec_cont->height, 8) >> 3;

  if(dec_cont->decoder.bit_depth == 8)
    raster_stride = NEXT_MULTIPLE(display_width, 16);
  else
    raster_stride = NEXT_MULTIPLE(display_width * 2, 16);

  if(case_info->frame_num == 0) {
    case_info->decode_width = pic_width;
    case_info->decode_height = pic_height;
    case_info->display_width = display_width;
    case_info->display_height = display_height;
    case_info->bit_depth = dec_cont->decoder.bit_depth;
    if(dec_cont->decoder.monochrome)
      case_info->chroma_format_id = 0;
    else
      case_info->chroma_format_id = 1;
    case_info->crop_x = case_info->crop_y = 0;
  } else {
    if(case_info->decode_width != pic_width
    || case_info->decode_height!= pic_height) {
      case_info->decode_width = MAX (pic_width, case_info->decode_width);
      case_info->decode_height = MAX (pic_height, case_info->decode_height);
      case_info->resolution_flag = 1;
    }
    if(case_info->display_width != display_width
    || case_info->display_height!= display_height) {
      case_info->display_height = MIN (display_height, case_info->display_height);
      case_info->display_width = MIN (display_width, case_info->display_width);
      case_info->resolution_flag = 1;
    }
    if(case_info->bit_depth != dec_cont->decoder.bit_depth) {
      case_info->bit_depth = MAX(dec_cont->decoder.bit_depth, case_info->bit_depth);
      case_info->depth_flag = 1;
    }
    if((case_info->chroma_format_id && dec_cont->decoder.monochrome) || ( !case_info->chroma_format_id && !dec_cont->decoder.monochrome))
      case_info->chroma_flag = 1;
  }

  if (case_info->frame_num < 10) {
    if (dec_cont->decoder.key_frame)
      case_info->frame_type[case_info->frame_num] = I_FRAME;
    else
      case_info->frame_type[case_info->frame_num] = P_FRAME;
    case_info->bit_depth_y_minus8[case_info->frame_num] = dec_cont->decoder.bit_depth - 8;
    case_info->bit_depth_c_minus8[case_info->frame_num] = dec_cont->decoder.bit_depth - 8;
    case_info->blackwhite_e[case_info->frame_num] = dec_cont->decoder.monochrome == 0 ? 1 : 0;
    case_info->num_tile_cols_8k[case_info->frame_num] = dec_cont->decoder.av1_tile_cols;
    case_info->ppin_luma_size[case_info->frame_num] = display_height * raster_stride;
    case_info->slice_num[case_info->frame_num]++;
  }

  case_info->frame_num++;
  case_info->codec = DEC_MODE_AV1;

  if(dec_cont->decoder.allow_intrabc)
    case_info->intrabc_flag = 1;
  case_info->interintra = dec_cont->decoder.enable_interintra_compound;
  case_info->palette_mode = dec_cont->decoder.obu_seq_hdr_checked.force_screen_content_tools;
  case_info->filter_intra_pred = dec_cont->decoder.obu_seq_hdr_checked.enable_filter_intra;
  case_info->intra_edge_filter = dec_cont->decoder.obu_seq_hdr_checked.enable_intra_edge_filter;

  case_info->bitrate += ((60 * GetDecRegister(dec_cont->av1_regs[0], HWIF_STREAM_LEN) * 8/ NEXT_MULTIPLE(dec_cont->height, 16))
                        * 3840/ NEXT_MULTIPLE(dec_cont->width, 16)) * 2160/ 1024/ 1024;
}
#endif
