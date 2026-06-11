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

#include "av1hwd_asic.h"
#include "av1_commondec.h"
#include "av1_entropymv.h"
#include "av1hwd_container.h"
#include "av1hwd_output.h"
#include "av1hwd_probs.h"
#include "commonconfig.h"
#include "basetype.h"
#include "dwl.h"
#include "ppu.h"
#include "regdrv.h"
#include "sw_util.h"

#include <string.h>

#include "aom_qm.h"
#include "av1_film_grain_noise_table.h"
#include "av1_global_motion.h"
#include "dec_log.h"
#include "errorhandling.h"

#if INTPTR_MAX == INT64_MAX
#ifdef VDK_STRICT_TEST
#define REG_WRITE_ADDR_AR(reg, ind, addr) \
  if ((addr) != 0) { \
    reg##_msb(sw_ctrl, ind, (u32)((u64)(addr) >> 32) ^ ENCRYP_CODE);   \
  } else { \
    reg##_msb(sw_ctrl, ind, 0);   \
  } \
  reg(sw_ctrl, ind, addr);
#else
#define REG_WRITE_ADDR_AR(reg, ind, addr) \
  reg##_msb(sw_ctrl, ind, (u32)((u64)(addr) >> 32));   \
  reg(sw_ctrl, ind, addr);
#endif
#else
#define REG_WRITE_ADDR_AR(reg, ind, addr) reg(sw_ctrl, ind, addr);
#endif

#define MAX_TILE_COLS (8192 / 256)
#define MAX_TILE_ROWS 4

#define DEC_MODE_AV1 17

static const int kMaxTiles =
    128;  // MaxTiles level 6.3 -
          // https://aomediacodec.github.io/av1-spec/#levels

extern u32 dec_apf_disable;

#ifdef ASIC_TRACE_SUPPORT
extern u32 ref_frame_mark;
#define TRACE_REF_FRAME_ADDR(v) ref_frame_mark = v;
#else
#define TRACE_REF_FRAME_ADDR(v)
#endif

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
static void Av1SetEmptyPictureData(struct Av1DecContainer *dec_cont);
#endif
static i32 Av1MallocRefFrm(struct Av1DecContainer *dec_cont, u32 index);
// static i32 Av1FreeRefFrm(struct Av1DecContainer *dec_cont, u32 index);
// static void Av1AsicSetTileInfo(struct Av1DecContainer *dec_cont);

void Av1AsicSetTileInfoRegs(struct Av1Decoder *dec,
                            struct DecAsicBuffers *asic_buff,
                            struct SwRegisters *sw_ctrl);

static void Av1AsicSetTileInfoMem(struct Av1DecContainer *dec_cont,const struct Av1DecInput *input);
static void Av1AsicSetReferenceFrames(struct Av1DecContainer *dec_cont);
void Av1AsicSetSegmentation(struct Av1Decoder *dec, struct SwRegisters *sw_ctrl);
void Av1AsicSetLoopFilter(struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, struct SwRegisters *sw_ctrl);
void Av1AsicSetPictureDimensions(struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, struct SwRegisters *sw_ctrl);
static void Av1AsicSetMulticoreParams(struct Av1DecContainer *dec_cont,
                                      size_t tile);
static void Av1AsicSetOutput(struct Av1DecContainer *dec_cont);
static u32 RequiredBufferCount(struct Av1DecContainer *dec_cont);

void Av1CalculateBufSize(struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff,
                              PpUnitIntConfig *ppu_cfg, u32 use_video_compressor,
                              u32 align, u32 index);
static void Av1AsicSyncMC(struct Av1DecContainer *dec_cont);
static void Av1MCHwRdyCallback(void* arg, i32 core_id);
static i32 Av1ReplaceRefPic(struct Av1DecContainer *dec_cont, i32 *index_info,u32 curr_index);
#ifdef SUPPORT_VCMD_M2M
static void Av1ProbUpdateInfo(struct Av1DecContainer *dec_cont);
static void Av1UpdateCDFsInfo(struct Av1DecContainer *dec_cont);
#endif
static void Av1GetRefFrmSize(
    struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    u32 use_video_compressor,
    u32 align,
    u32 *luma_size, u32 *chroma_size,
    u32 *rfc_luma_size, u32 *rfc_chroma_size);
#ifdef USE_LIBVA
static void av1_vaapi_get_golden_yuv(struct Av1DecContainer *dec_cont);
#endif

u32 RequiredBufferCount(struct Av1DecContainer *dec_cont) {
  return (Av1BufferQueueCountReferencedBuffers(dec_cont->bq) + 2);
}

void Av1AsicInit(const struct DWLCodecConfig *config,
                 struct Av1DecContainer *dec_cont,
                 const int multicore_poll_period) {
  struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;

  // Set swaps etc. based on common config
  sw_ctrl->sw_dec_mode = DEC_MODE_AV1;
  SetDecRegister(dec_cont->av1_regs[0], HWIF_DEC_MODE, sw_ctrl->sw_dec_mode);
  SetCommonConfigRegs(dec_cont->av1_regs[0]);

  dec_cont->multicore_poll_period = multicore_poll_period;
  sw_ctrl->sw_apf_disable = dec_apf_disable;
}

i32 Av1AsicAllocateMem(const void *dwl, struct DecAsicBuffers *asic_buff, u32 secure_mode) {
  i32 dwl_ret;
  u32 size;
  size = (sizeof(struct AV1CDFs) + 31) & (~31);
  asic_buff->prob_tbl.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(asic_buff->prob_tbl.mem_type, DWL_MEM_USAGE_IN_PROBTABLE,
                secure_mode);
  dwl_ret = DWLMallocLinear(dwl, size, /*DWL_HW_MARK_HOST_TO_DEVICE,*/
                            &asic_buff->prob_tbl);
  if (dwl_ret != DWL_OK) goto err;
  asic_buff->prob_tbl_out.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_TO_HOST;
  SET_MEM_USAGE(asic_buff->prob_tbl_out.mem_type, DWL_MEM_USAGE_TMP_PROBTABLE_OUT,
                secure_mode);
  dwl_ret = DWLMallocLinear(dwl, size, /*DWL_HW_MARK_DEVICE_TO_HOST,*/
                            &asic_buff->prob_tbl_out);
  if (dwl_ret != DWL_OK) goto err;

  size = (sizeof(struct AV1CDFs) + 31) & (~31);
  asic_buff->default_cdfs_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(asic_buff->default_cdfs_mem.mem_type, DWL_MEM_USAGE_IN_AV1CDFS, secure_mode);
  dwl_ret = DWLMallocLinear(dwl, size, /*DWL_HW_MARK_HOST_TO_DEVICE,*/
                            &asic_buff->default_cdfs_mem);
  if (dwl_ret != DWL_OK) goto err;
  size = sizeof(struct MvCDFs);
  asic_buff->default_cdfs_ndvc_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(asic_buff->default_cdfs_ndvc_mem.mem_type, DWL_MEM_USAGE_IN_AV1CDFS_NDVC, secure_mode);
  dwl_ret = DWLMallocLinear(dwl, size, /*DWL_HW_MARK_HOST_TO_DEVICE,*/
                            &asic_buff->default_cdfs_ndvc_mem);
  if (dwl_ret != DWL_OK) goto err;
  size = (((sizeof(struct AV1CDFs)  + 31) & (~31)) * NUM_REF_FRAMES);
  asic_buff->cdfs_last_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
				       DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(asic_buff->cdfs_last_mem.mem_type, DWL_MEM_USAGE_IN_AV1CDFS_LAST, secure_mode);
  dwl_ret = DWLMallocLinear(dwl, size, /*DWL_HW_MARK_HOST_TO_DEVICE,*/
                            &asic_buff->cdfs_last_mem);
  if (dwl_ret != DWL_OK) goto err;
  // AV1: max number of tiles times width and height (1 byte each)
  //      rounding up to next 64 bytes boundary = 256 bytes
  // AV1:
  //       row_sb:      4 bytes
  //       col_sb:      4 bytes
  //       start_pos:   4 bytes   (tile start byte offset from base)
  //       end_pos:     4 bytes   (tile end byte offset)
  //       x AV1_MAX_TILES (128) = 1024 bytes
  size = AV1_MAX_TILES * 16;

  asic_buff->tile_info.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(asic_buff->tile_info.mem_type, DWL_MEM_USAGE_IN_AV1TILEINFO,
                secure_mode);
  dwl_ret = DWLMallocLinear(dwl, size, /*DWL_HW_MARK_HOST_TO_DEVICE,*/
                            &asic_buff->tile_info);
  if (dwl_ret != DWL_OK) goto err;

  {
    asic_buff->film_grain_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(asic_buff->film_grain_mem.mem_type, DWL_MEM_USAGE_IN_AV1FILM_GRAIN,
                  secure_mode);
    dwl_ret |= DWLMallocLinear(
        dwl, sizeof(struct AV1FilmGrainMemory),
        /*DWL_HW_MARK_HOST_TO_DEVICE,*/ &asic_buff->film_grain_mem);
    asic_buff->global_model.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(asic_buff->global_model.mem_type, DWL_MEM_USAGE_IN_AV1GLOBAL_MODEL,
                  secure_mode);
    dwl_ret |= DWLMallocLinear(
        dwl, GM_GLOBAL_MODELS_PER_FRAME * GLOBAL_MODEL_TOTAL_SIZE,
        /*DWL_HW_MARK_HOST_TO_DEVICE,*/ &asic_buff->global_model);
  }
  DWLDMATransData(dwl, &asic_buff->prob_tbl_out, 0,
                  asic_buff->prob_tbl_out.size, HOST_TO_DEVICE);
  DWLDMATransData(dwl, &asic_buff->prob_tbl, 0,
                  asic_buff->prob_tbl.size, HOST_TO_DEVICE);
  DWLDMATransData(dwl, &asic_buff->tile_info, 0,
                  asic_buff->tile_info.size, HOST_TO_DEVICE);
  DWLDMATransData(dwl, &asic_buff->film_grain_mem, 0,
                  asic_buff->film_grain_mem.size, HOST_TO_DEVICE);
  DWLDMATransData(dwl, &asic_buff->global_model, 0,
                  asic_buff->global_model.size, HOST_TO_DEVICE);

#if 0
  if (dec_cont->pp_enabled/* &&
      dec_cont->secondary_out_cfg.secondary_output_tiled == 0*/) {
    size = NEXT_MULTIPLE(4352 * 32 * 2, 4096);
    dwl_ret |= DWLMallocLinear(dwl, size, /*DWL_HW_MARK_DEVICE_ONLY,*/
                               &asic_buff->secondary_column_buffer);
  }
#endif

  // Always allocate multicore buffers, since it can be changed per-frame
  size = NEXT_MULTIPLE(64 * kMaxTiles, 4096);
  asic_buff->multicore_sync_buffers.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(asic_buff->multicore_sync_buffers.mem_type,
                DWL_MEM_USAGE_TMP_AV1MULTICORE_SYNC,
                secure_mode);
  dwl_ret |= DWLMallocLinear(dwl, size, &asic_buff->multicore_sync_buffers);

  return 0;
err:
  Av1AsicReleaseMem(dwl, asic_buff);
  return -1;
}

void Av1AsicReleaseMem(const void *dwl, struct DecAsicBuffers *asic_buff) {
  if (asic_buff->prob_tbl.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->prob_tbl);
    asic_buff->prob_tbl.virtual_address = NULL;
  }
  if (asic_buff->prob_tbl_out.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->prob_tbl_out);
    asic_buff->prob_tbl_out.virtual_address = NULL;
  }
  if (asic_buff->tile_info.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->tile_info);
    asic_buff->tile_info.virtual_address = NULL;
  }

  if (asic_buff->film_grain_mem.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->film_grain_mem);
    asic_buff->film_grain_mem.virtual_address = NULL;
  }
  if (asic_buff->global_model.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->global_model);
    asic_buff->global_model.virtual_address = NULL;
  }

  if (asic_buff->default_cdfs_mem.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->default_cdfs_mem);
    asic_buff->default_cdfs_mem.virtual_address = NULL;
  }
  if (asic_buff->default_cdfs_ndvc_mem.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->default_cdfs_ndvc_mem);
    asic_buff->default_cdfs_ndvc_mem.virtual_address = NULL;
  }
  if (asic_buff->cdfs_last_mem.virtual_address != NULL) {
    DWLFreeLinear(dwl, &asic_buff->cdfs_last_mem);
    asic_buff->cdfs_last_mem.virtual_address = NULL;
  }

#if 0
  if (asic_buff->secondary_column_buffer.virtual_address) {
    DWLFreeLinear(dwl, &asic_buff->secondary_column_buffer);
    asic_buff->secondary_column_buffer.virtual_address = NULL;
  }
#endif

  if (asic_buff->multicore_sync_buffers.virtual_address) {
    DWLFreeLinear(dwl, &asic_buff->multicore_sync_buffers);
    asic_buff->multicore_sync_buffers.virtual_address = NULL;
  }

#ifdef USE_FAKE_RFC_TABLE
  if (asic_buff->fake_rfc_tbl.virtual_address != NULL) {
#ifdef ASIC_TRACE_SUPPORT
    DWLFreeRefFrm(dwl, &asic_buff->fake_rfc_tbl);
#else
    DWLFreeLinear(dwl, &asic_buff->fake_rfc_tbl);
#endif
    asic_buff->fake_rfc_tbl.virtual_address = NULL;
  }
#endif
}

void Av1ReleaseLowLatencyMem(struct Av1DecContainer *dec_cont) {
  /* low latency: stream status in ddr buffer */
  if(dec_cont->llstrminfo.strm_status.virtual_address != NULL){
    DWLFreeLinear(dec_cont->dwl, &dec_cont->llstrminfo.strm_status);
    dec_cont->llstrminfo.strm_status.virtual_address = NULL;
  }
}

i32 Av1AllocatLowLatencyMem(struct Av1DecContainer *dec_cont) {
  /* low latency: Allocate buffer for ddr_low_latency model */
  if(dec_cont->llstrminfo.strm_status_in_buffer == 1 && dec_cont->llstrminfo.strm_status.virtual_address == NULL){
    dec_cont->llstrminfo.strm_status.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE | DWL_MEM_TYPE_VPU_WORKING;
    if(DWLMallocLinear(dec_cont->dwl, 16 * 4, &dec_cont->llstrminfo.strm_status))
      return HANTRO_NOK;
   }
   return HANTRO_OK;
}

i32 Av1AsicAllocateFbcMem(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_tile_cols = 1 << dec_cont->decoder.log2_tile_columns;
  if (num_tile_cols < 2) return HANTRO_OK;
  u32 size = 0, i = 0;
  u32 pixel_width = dec_cont->decoder.bit_depth;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].tiled_e && dec_cont->ppu_cfg[i].tile_mode) {
      dec_cont->ppu_cfg[i].fbc_tile_offset = size;
      size += 16 * pixel_width / 8 * 3 / 2 * NEXT_MULTIPLE((dec_cont->ppu_cfg[i].vir_top +
              dec_cont->ppu_cfg[i].scale.height + dec_cont->ppu_cfg[i].vir_bottom), 32) * num_tile_cols;
    }
  }
  if (asic_buff->fbc_size >= size) return HANTRO_OK;

  /* If already allocated, release the old, too small buffers. */
  Av1AsicReleaseFbcMem(dec_cont);

  /*DWL_HW_MARK_DEVICE_ONLY*/
  asic_buff->fbc_tile.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(asic_buff->fbc_tile.mem_type, DWL_MEM_USAGE_TMP_AV1AFBC_TILE,
                dec_cont->secure_mode);
  i32 dwl_ret = DWLMallocLinear(dec_cont->dwl, size, &dec_cont->asic_buff->fbc_tile);

  if (dwl_ret != DWL_OK) {
    Av1AsicReleaseFbcMem(dec_cont);
    return HANTRO_NOK;
  }
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    dec_cont->ppu_cfg[i].fbc_tile = dec_cont->asic_buff->fbc_tile;
  }
  asic_buff->fbc_size = size;
  return HANTRO_OK;
}

/* Allocate filter memories that are dependent on tile structure and height. */
i32 Av1AsicAllocateFilterBlockMem(const void *dwl, struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, PpUnitIntConfig *ppu_cfg, u32 use_multicore_backup, u32 secure_mode) {
  u32 num_tile_cols = 1 << dec->log2_tile_columns;
  const u32 tile_cols = use_multicore_backup ? dec->av1_tile_cols : 1;
  u32 i = 0, count = 0;
  u32 pp_reorder_offset = 0;
  u32 scale_offset = 0;
  u32 pp_scale_size = 0;
  u32 scale_out_offset = 0;
  u32 pp_scale_out_size = 0;
  if (num_tile_cols < 2) return HANTRO_OK;

  u32 size;
  u32 filter_control_mem_size;
  u32 pic_height = NEXT_MULTIPLE(asic_buff->height, 64);
  u32 height_in_sb = pic_height / 64;
  u32 stripe_num = ((pic_height + 8) +63) / 64;
  //u32 max_bit_depth = 10;

  /* db tile col ctrl buffer */
  asic_buff->db_ctrl_col_tsize = NEXT_MULTIPLE(height_in_sb * 2 * 16 * 16, 128);
  filter_control_mem_size = asic_buff->db_ctrl_col_tsize * num_tile_cols;

  /* db tile col data buffer */
  asic_buff->db_data_col_offset = 0;
  asic_buff->db_data_col_tsize = NEXT_MULTIPLE(height_in_sb * 3 * 20 * 16, 128);
  size = asic_buff->db_data_col_tsize * num_tile_cols;
  asic_buff->cdef_col_offset = size;

  /* cdef tile col buffer */
  asic_buff->cdef_col_tsize = NEXT_MULTIPLE(height_in_sb * 60 * 16, 128);
  size += asic_buff->cdef_col_tsize * num_tile_cols;
  asic_buff->sr_col_offset = size;

  /* sr tile col buffer */
  asic_buff->sr_col_tsize = NEXT_MULTIPLE(height_in_sb * 60 * 4 * 16, 128);
  size += asic_buff->sr_col_tsize * num_tile_cols;
  asic_buff->lr_col_offset = size;

  /* lr tile col buffer */
  asic_buff->lr_col_tsize = NEXT_MULTIPLE(stripe_num * 120 * 16, 128);
  size += asic_buff->lr_col_tsize * num_tile_cols;

  /* after tile8x8 optimize, buffer HWIF_RFC_COLBUF_BASE_LSB is always used no matter if RFC is enable or not */
  asic_buff->rfc_col_offset = size;
  asic_buff->rfc_col_size = NEXT_MULTIPLE(asic_buff->height, 64) / 64 * 128;
  size += asic_buff->rfc_col_size * num_tile_cols;

  if (ppu_cfg) {
    pp_reorder_offset = size;
    /* only one tile pp tile_edge buffer need for sc mode. */
    num_tile_cols = (tile_cols > 1) ? num_tile_cols : 1;
    size += PPGetLancozsColumnBufferSize(ppu_cfg, asic_buff->height, dec->bit_depth, num_tile_cols);
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
      if (ppu_cfg[i].enabled)
        count++;
    }
  }
  if (asic_buff->filter_mem.size < size) {
    /* If already allocated, release the old, too small buffers. */
    Av1AsicReleaseFilterBlockMem(dwl, asic_buff);

    /*DWL_HW_MARK_DEVICE_ONLY*/
    asic_buff->filter_control.mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY | DWL_MEM_TYPE_VPU_ONLY;
    SET_MEM_USAGE(asic_buff->filter_control.mem_type, DWL_MEM_USAGE_TMP_FILTER_CONTROL,
                  secure_mode);
    i32 dwl_ret = DWLMallocLinear(dwl, filter_control_mem_size, &asic_buff->filter_control);

    if (dwl_ret != DWL_OK) {
      Av1AsicReleaseFilterBlockMem(dwl, asic_buff);
      return HANTRO_NOK;
    }
    asic_buff->filter_mem.mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY | DWL_MEM_TYPE_VPU_ONLY;
    SET_MEM_USAGE(asic_buff->filter_mem.mem_type, DWL_MEM_USAGE_TMP_AV1FILTER,
                secure_mode);
    dwl_ret = DWLMallocLinear(dwl, size, &asic_buff->filter_mem);

    if (dwl_ret != DWL_OK) {
      Av1AsicReleaseFilterBlockMem(dwl, asic_buff);
      return HANTRO_NOK;
    }
  }
  if (ppu_cfg) {
    pp_scale_size = 0;
    pp_scale_out_size = 0;
    scale_offset = pp_reorder_offset + ppu_cfg[0].reorder_size * num_tile_cols;
    scale_out_offset = scale_offset + count * ppu_cfg[0].scale_size * num_tile_cols;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
      if (ppu_cfg[i].enabled) {
        ppu_cfg[i].reorder_buf_bus[0] = asic_buff->filter_mem.bus_address + pp_reorder_offset;
        ppu_cfg[i].scale_buf_bus[0] = asic_buff->filter_mem.bus_address + scale_offset + pp_scale_size;
        ppu_cfg[i].scale_out_buf_bus[0] = asic_buff->filter_mem.bus_address + scale_out_offset + pp_scale_out_size;
        pp_scale_size += ppu_cfg[i].scale_size * num_tile_cols;
        pp_scale_out_size += ppu_cfg[i].scale_out_size * num_tile_cols;
      }
    }
  }
  return HANTRO_OK;
}

void Av1AsicReleaseFbcMem(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  if (dec_cont->asic_buff->fbc_tile.virtual_address != NULL) {
    DWLFreeLinear(dec_cont->dwl, &dec_cont->asic_buff->fbc_tile);
    dec_cont->asic_buff->fbc_tile.virtual_address = NULL;
    asic_buff->fbc_size = 0;
  }
}

void Av1AsicReleaseFilterBlockMem(const void *dwl, struct DecAsicBuffers *asic_buff) {
  // struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  if (DWL_DEVMEM_VAILD(asic_buff->filter_mem)) {
    DWLFreeLinear(dwl, &asic_buff->filter_mem);
    asic_buff->filter_mem.virtual_address = NULL;
    asic_buff->filter_mem.size = 0;
  }
  if (DWL_DEVMEM_VAILD(asic_buff->filter_control)) {
    DWLFreeLinear(dwl, &asic_buff->filter_control);
    asic_buff->filter_control.virtual_address = NULL;
    asic_buff->filter_control.size = 0;
  }
}

i32 Av1AsicAllocatePictures(struct Av1DecContainer *dec_cont) {
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  i32 ret = 0;

  for (i = 0; i < dec_cont->num_buffers; i++) {
    ret = Av1MallocRefFrm(dec_cont, i);
    if (ret == DEC_WAITING_FOR_BUFFER) {
      return DEC_WAITING_FOR_BUFFER;
    } else if (ret == DEC_MEMFAIL) {
      return DEC_MEMFAIL;
    }
  }

  ASSERT(asic_buff->width / 4 < 0x1FFF);
  ASSERT(asic_buff->height / 4 < 0x1FFF);

  asic_buff->out_buffer_i = -1;

  return 0;
}

void Av1AsicReleasePictures(struct Av1DecContainer *dec_cont) {
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
#if 0
  for (i = 0; i < dec_cont->num_buffers; i++) {
    Av1FreeRefFrm(dec_cont, i);
  }
#else
  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    if (dec_cont->pp_enabled && DWL_DEVMEM_VAILD(asic_buff->pictures[i]))
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[i]);
  }
#endif
  Av1ReleaseParasiticBufs(dec_cont);
  if (dec_cont->bq) {
    Av1BufferQueueRelease(dec_cont->bq, 1);
    dec_cont->bq = NULL;
  }

  if (dec_cont->pp_bq) {
    Av1BufferQueueRelease(dec_cont->pp_bq, 1);
    dec_cont->pp_bq = NULL;
  }

  DWLmemset(asic_buff->pictures, 0, sizeof(asic_buff->pictures));
  DWLmemset(asic_buff->dpb_parasitic_buf, 0, sizeof(asic_buff->dpb_parasitic_buf));
}

i32 Av1AllocateFrame(struct Av1DecContainer *dec_cont, u32 index) {
  i32 ret = HANTRO_OK;

  if (Av1MallocRefFrm(dec_cont, index)) ret = HANTRO_NOK;

  if (dec_cont->pp_enabled) {
    dec_cont->num_buffers++;
    Av1BufferQueueAddBuffer(dec_cont->bq);
  }

  return ret;
}

i32 Av1ReallocateFrame(struct Av1DecContainer *dec_cont, u32 index) {
  i32 ret = HANTRO_OK;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 out_index = 0;

  if (!dec_cont->pp_enabled)
    out_index = index;
  else
    out_index = dec_cont->asic_buff->pp_buffer_map[index];

  pthread_mutex_lock(&dec_cont->sync_out);
  while (dec_cont->asic_buff->display_index[out_index])
    pthread_cond_wait(&dec_cont->sync_out_cv, &dec_cont->sync_out);

  Av1SetExternalBufferInfo(dec_cont);

  if (dec_cont->pp_enabled &&
      asic_buff->pictures[asic_buff->out_buffer_i].logical_size <
          asic_buff->picture_size) {
    if (DWL_DEVMEM_VAILD(asic_buff->pictures[index]))
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[index]);
    asic_buff->pictures[index].mem_type = DWL_MEM_TYPE_DPB | DWL_MEM_TYPE_DMA_DEVICE_ONLY;
    SET_MEM_USAGE(asic_buff->pictures[index].mem_type, DWL_MEM_USAGE_OUT_REFERENCE,
                  dec_cont->secure_mode);
    ret = DWLMallocRefFrm(dec_cont->dwl, asic_buff->picture_size,
                          &asic_buff->pictures[index]);
    if (ret != DWL_OK) {
       pthread_mutex_unlock(&dec_cont->sync_out);
       return DEC_MEMFAIL;
    }

    // Global models
    for (int i = 0; i < GM_GLOBAL_MODELS_PER_FRAME; ++i) {
      asic_buff->global_models[index][i] = sDefaultParams;
    }
  }
  /* Reallocate larger dpb parasitic buffer into current index if needed */
  if ((asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i].logical_size <
       asic_buff->dpb_parasitic_buf_size)) {

    if (DWL_DEVMEM_VAILD(asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i]))
      Av1ReleaseParasiticBuf(dec_cont->dwl, &asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i]);

    i32 dwl_ret = Av1AllocParasiticBuf(dec_cont->dwl, asic_buff->dpb_parasitic_buf_size,
                                       &asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i],
                                       dec_cont->secure_mode);
    if (dwl_ret != DWL_OK) {
      pthread_mutex_unlock(&dec_cont->sync_out);
      return DEC_MEMFAIL;
    }
  }

  if (!dec_cont->pp_enabled) {
    dec_cont->buf_to_free = &asic_buff->pictures[index];
    dec_cont->next_buf_size = asic_buff->picture_size;
    dec_cont->next_dpb_parasitic_buf_size = asic_buff->dpb_parasitic_buf_size;
    dec_cont->buffer_index = asic_buff->out_buffer_i;
    asic_buff->realloc_out_buffer = 1;
    dec_cont->buf_type = REFERENCE_BUFFER;
    dec_cont->buf_num = 1;
    ret = DEC_WAITING_FOR_BUFFER;
  } else if (dec_cont->pp_enabled &&
             asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size <
                 asic_buff->pp_size) {
    dec_cont->buf_to_free =
        &asic_buff->pp_pictures[asic_buff->pp_buffer_map[index]];
    dec_cont->next_buf_size = asic_buff->pp_size;
    dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
    dec_cont->buffer_index = asic_buff->out_pp_buffer_i;
    asic_buff->realloc_out_buffer = 1;
    dec_cont->buf_num = 1;
    ret = DEC_WAITING_FOR_BUFFER;
  }

  pthread_mutex_unlock(&dec_cont->sync_out);

  return ret;
}

i32 Av1MallocRefFrm(struct Av1DecContainer *dec_cont, u32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_sbs, luma_size, chroma_size, dir_mvs_size;
  u64 pp_size = 0;
  u32 pic_width_in_cbsy, pic_height_in_cbsy;
  u32 pic_width_in_cbsc, pic_height_in_cbsc;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth;
  i32 dwl_ret = DWL_OK;
  u32 out_w, out_h, i;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  PpUnitIntConfig *ppu_cfg;

  bit_depth = dec_cont->decoder.bit_depth;
  /* No stride when compression is used. */
  if (dec_cont->use_video_compressor)
    out_w = 4 * asic_buff->width * bit_depth / 8;
  else
    out_w = NEXT_MULTIPLE(TILE_HEIGHT * asic_buff->width * bit_depth,
                          ALIGN(dec_cont->align) * 8) /
            8;
  out_h = asic_buff->height / TILE_HEIGHT;
  luma_size = out_w * out_h;
  chroma_size = luma_size / 2;
  asic_buff->out_stride[index] = out_w;
  num_sbs = ((asic_buff->width + 63) / 64 + 1) * ((asic_buff->height + 63) / 64  + 1);
  dir_mvs_size =
      NEXT_MULTIPLE(num_sbs *24*128/8,
                    ref_buffer_align); /* MVs (16 MBs / CTB * 12 bytes / MB) */

  if (dec_cont->pp_enabled) {
    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      asic_buff->ds_stride[index][i] = ppu_cfg->ystride;
      asic_buff->ds_stride_ch[index][i] = ppu_cfg->cstride;
    }
    pp_size = CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0);
  }
  if (dec_cont->use_video_compressor) {
    pic_width_in_cbsy = (asic_buff->width + 8 - 1) / 8;
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = (asic_buff->width + 16 - 1) / 16;
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (asic_buff->height + 8 - 1) / 8;
    pic_height_in_cbsc = (asic_buff->height / 2 + 4 - 1) / 4;

    /* luma table size */
    luma_table_size =
        NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, ref_buffer_align);
    /* chroma table size */
    chroma_table_size =
        NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, ref_buffer_align);
  } else {
    luma_table_size = chroma_table_size = 0;
  }

  /* luma */
  asic_buff->pictures_c_offset[index] =
      NEXT_MULTIPLE(luma_size, ref_buffer_align);
  /* align sync_mc buffer, storage sync bytes adjoining to dir mv*/
  asic_buff->dir_mvs_offset[index] =
      NEXT_MULTIPLE(64, ref_buffer_align);

  if (dec_cont->use_video_compressor) {
    asic_buff->cbs_y_tbl_offset[index] =
        asic_buff->pictures_c_offset[index] + NEXT_MULTIPLE(chroma_size,ref_buffer_align);
    asic_buff->cbs_c_tbl_offset[index] =
        asic_buff->cbs_y_tbl_offset[index] + luma_table_size;
  } else {
    asic_buff->cbs_y_tbl_offset[index] = 0;
    asic_buff->cbs_c_tbl_offset[index] = 0;
  }
  asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align) +
                            NEXT_MULTIPLE(chroma_size, ref_buffer_align) +
                            luma_table_size + chroma_table_size;
  asic_buff->dpb_parasitic_buf_size = NEXT_MULTIPLE(64, ref_buffer_align) +
                                      dir_mvs_size;
  asic_buff->pp_size = pp_size;

  if (DWL_DEVMEM_COMPARE(asic_buff->pictures[index], DWL_DEVMEM_INIT)) {
    if (!dec_cont->pp_enabled) {
      dec_cont->next_buf_size = asic_buff->picture_size;
      dec_cont->next_dpb_parasitic_buf_size = asic_buff->dpb_parasitic_buf_size;
      dec_cont->buf_type = REFERENCE_BUFFER;
      if (index == 0)
        dec_cont->buf_num = dec_cont->num_buffers;
      else
        dec_cont->buf_num = 1;
      dec_cont->buffer_index = index;
      return DEC_WAITING_FOR_BUFFER;
    } else {
      asic_buff->pictures[index].mem_type = DWL_MEM_TYPE_DPB | DWL_MEM_TYPE_DMA_DEVICE_ONLY;
      SET_MEM_USAGE(asic_buff->pictures[index].mem_type, DWL_MEM_USAGE_OUT_REFERENCE,
                    dec_cont->secure_mode);
      dwl_ret = DWLMallocRefFrm(dec_cont->dwl, asic_buff->picture_size,
                                &asic_buff->pictures[index]);
      if (dwl_ret != DWL_OK) return DEC_MEMFAIL;
      dwl_ret = Av1AllocParasiticBuf(dec_cont->dwl, asic_buff->dpb_parasitic_buf_size,
                          &asic_buff->dpb_parasitic_buf[index],dec_cont->secure_mode);
      if (dwl_ret != DWL_OK) return DEC_MEMFAIL;
    }
  }

  if (index < dec_cont->min_buffer_num) {
    if (DWL_DEVMEM_COMPARE(asic_buff->pp_pictures[index], DWL_DEVMEM_INIT) &&
        dec_cont->pp_enabled) {
      dec_cont->next_buf_size = asic_buff->pp_size;
      dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
      if (index == 0)
        dec_cont->buf_num = dec_cont->min_buffer_num;
      else
        dec_cont->buf_num = 1;
      return DEC_WAITING_FOR_BUFFER;
    }
  }

  if (dwl_ret != DWL_OK) {
    Av1AsicReleasePictures(dec_cont);
    return DEC_MEMFAIL;
  }

  // Global models
  for (int i = 0; i < GM_GLOBAL_MODELS_PER_FRAME; ++i) {
    asic_buff->global_models[index][i] = sDefaultParams;
  }

  return DEC_OK;
}

void Av1SetExternalBufferInfo(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_sbs, luma_size, chroma_size, dir_mvs_size;
  u32 pp_size = 0;
  u32 luma_table_size, chroma_table_size;
  u32 picture_size, dscale_size, buff_size;
  u32 min_buffer_num;
  enum DecBufferType buf_type;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;


  Av1GetRefFrmSize(&dec_cont->decoder, asic_buff, dec_cont->use_video_compressor,
                   dec_cont->align, &luma_size, &chroma_size, &rfc_luma_size,
                   &rfc_chroma_size);

  num_sbs = ((asic_buff->width + 63) / 64 + 1) * ((asic_buff->height + 63) / 64 +1);
  dir_mvs_size =
      NEXT_MULTIPLE(num_sbs * 24*128/8,
                    ref_buffer_align); /* MVs (16 MBs / CTB * 12 bytes / MB) */
  if (dec_cont->pp_enabled) {
    pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }

  /* luma table size */
  luma_table_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  /* chroma table size */
  chroma_table_size = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align) +
                 NEXT_MULTIPLE(chroma_size, ref_buffer_align) +
                 luma_table_size + chroma_table_size;
  asic_buff->pp_size = pp_size;
  if (dec_cont->pp_enabled)
    dscale_size = pp_size;
  else
    dscale_size = 0;

  if (!dec_cont->pp_enabled) {
    min_buffer_num = dec_cont->min_buffer_num;
    buff_size = picture_size;
    dec_cont->next_dpb_parasitic_buf_size = NEXT_MULTIPLE(64, ref_buffer_align) +
                                            dir_mvs_size;
    buf_type = REFERENCE_BUFFER;
  } else {
    min_buffer_num = dec_cont->min_buffer_num;
    buff_size = dscale_size;
    buf_type = DOWNSCALE_OUT_BUFFER;
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = buff_size;
  dec_cont->buf_type = buf_type;
}
#if 0
i32 Av1FreeRefFrm(struct Av1DecContainer *dec_cont, u32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (dec_cont->pp_enabled) {
    if (asic_buff->pictures[index].virtual_address != NULL) {
      struct DWLLinearMem buff = asic_buff->pictures[index];
      buff.virtual_address = (u32 *)((char *)buff.virtual_address);
      DWLFreeRefFrm(dec_cont->dwl, &buff);
      asic_buff->pictures[index].virtual_address = NULL;
    }
    if (asic_buff->pictures_c[index].virtual_address != NULL) {
      struct DWLLinearMem buff = asic_buff->pictures_c[index];
      buff.virtual_address = (u32 *)((char *)buff.virtual_address);
      DWLFreeRefFrm(dec_cont->dwl, &buff);
      asic_buff->pictures_c[index].virtual_address = NULL;
    }
    if (asic_buff->dir_mvs[index].virtual_address != NULL) {
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->dir_mvs[index]);
      asic_buff->dir_mvs[index].virtual_address = NULL;
    }
    if (asic_buff->rfc_table[index].virtual_address != NULL) {
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->rfc_table[index]);
      asic_buff->rfc_table[index].virtual_address = NULL;
    }
  }
  if (asic_buff->raster_luma[index].virtual_address != NULL)
    DWLFreeRefFrm(dec_cont->dwl, &asic_buff->raster_luma[index]);
  if (asic_buff->raster_chroma[index].virtual_address != NULL)
    DWLFreeRefFrm(dec_cont->dwl, &asic_buff->raster_chroma[index]);
  if (asic_buff->bodp_cfg[index].virtual_address != NULL)
    DWLFreeLinear(dec_cont->dwl, &asic_buff->bodp_cfg[index]);
  return HANTRO_OK;
}
#endif
i32 Av1GetRefFrm(struct Av1DecContainer *dec_cont, const struct Av1DecInput *input) {
  u32 limit = dec_cont->dynamic_buffer_limit;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  if (RequiredBufferCount(dec_cont) < limit)
    limit = RequiredBufferCount(dec_cont);

  if (!asic_buff->realloc_out_buffer) {
    if (!dec_cont->no_decoding_buffer || asic_buff->out_buffer_i == EMPTY_MARKER) {
      asic_buff->out_buffer_i = Av1BufferQueueGetBuffer(dec_cont->bq, limit);
      if (asic_buff->out_buffer_i >= 0 && asic_buff->out_buffer_i < MAX_PIC_BUFFERS)
        asic_buff->first_show[asic_buff->out_buffer_i] = 0;
      if (asic_buff->out_buffer_i == ABORT_MARKER)
        return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else if (asic_buff->out_buffer_i == EMPTY_MARKER) {
        asic_buff->out_pp_buffer_i = EMPTY_MARKER;
        return DEC_NO_DECODING_BUFFER;
      }
#endif
      else if (asic_buff->out_buffer_i < 0) {
        if (Av1AllocateFrame(dec_cont, dec_cont->num_buffers)) {
          /* Request for a new buffer. */
          asic_buff->realloc_out_buffer = 0;
          return DEC_WAITING_FOR_BUFFER;
        }
        asic_buff->out_buffer_i = Av1BufferQueueGetBuffer(dec_cont->bq, limit);
      }

      Av1CalculateBufSize(&dec_cont->decoder, asic_buff,
                      dec_cont->pp_enabled ? dec_cont->ppu_cfg : NULL,
                      dec_cont->use_video_compressor,
                      dec_cont->align, asic_buff->out_buffer_i);
    }
    if (dec_cont->pp_enabled) {
      if (!dec_cont->no_decoding_buffer || asic_buff->out_pp_buffer_i == EMPTY_MARKER) {
        asic_buff->out_pp_buffer_i = Av1BufferQueueGetBuffer(dec_cont->pp_bq, 0);
        if (asic_buff->out_pp_buffer_i == ABORT_MARKER)
          return DEC_ABORTED;
#ifdef GET_FREE_BUFFER_NON_BLOCK
        else if (asic_buff->out_pp_buffer_i == EMPTY_MARKER) {
          return DEC_NO_DECODING_BUFFER;
        }
#endif
        else if (asic_buff->out_pp_buffer_i < 0) {
          /* pp buffer should be allocate when reference buffer return DEC_WAITING_FOR_BUFFER,*/
          /* this code just for removing negative value warning. */
          return DEC_WAITING_FOR_BUFFER;
        }
      }
      asic_buff->pp_buffer_map[asic_buff->out_buffer_i] = asic_buff->out_pp_buffer_i;
      asic_buff->pp_out_ctrl[asic_buff->out_pp_buffer_i] = input->dec_ctrl;
    }

    DWLmemcpy(dec_cont->asic_buff->segment_info[asic_buff->out_buffer_i]
                  .segment_feature_enable,
              dec_cont->decoder.segment_feature_enable,
              sizeof(dec_cont->decoder.segment_feature_enable));
    DWLmemcpy(dec_cont->asic_buff->segment_info[asic_buff->out_buffer_i]
                  .segment_feature_data,
              dec_cont->decoder.segment_feature_data,
              sizeof(dec_cont->decoder.segment_feature_data));
  }

  /* Reallocate picture memories if needed */
  if (asic_buff->pictures[asic_buff->out_buffer_i].logical_size <
          asic_buff->picture_size ||
      (dec_cont->pp_enabled &&
       asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size <
           asic_buff->pp_size)) {
    /* Reallocate bigger picture buffer into current index */
    i32 dwl_ret = Av1ReallocateFrame(dec_cont, asic_buff->out_buffer_i);
    if (dwl_ret) return dwl_ret;
  }

  memcpy(dec_cont->asic_buff->global_models[asic_buff->out_buffer_i],
         dec_cont->decoder.models,
         sizeof(struct WarpedMotionParams) * GM_GLOBAL_MODELS_PER_FRAME);

  if (!dec_cont->pp_enabled) {
    asic_buff->picture_info[asic_buff->out_buffer_i].frame_offset =
        dec_cont->decoder.frame_offset;
  } else {
    asic_buff->picture_info[asic_buff->out_pp_buffer_i].frame_offset =
        dec_cont->decoder.frame_offset;
  }

  asic_buff->realloc_out_buffer = 0;

#ifdef ENABLE_FPGA_VERIFICATION
/* for multilayer stream can't clean the pp buffer.*/
  if (!dec_cont->heif_mode && dec_cont->pp_enabled) {
    DWLLinearMemset(dec_cont->dwl, &asic_buff->pp_pictures[asic_buff->out_pp_buffer_i], 0,
                    0, asic_buff->pp_pictures[asic_buff->out_buffer_i].size);
  }
#endif

  return HANTRO_OK;
}
#ifdef SUPPORT_VCMD_M2M
static void Av1ProbUpdateInfo(struct Av1DecContainer *dec_cont){
  struct Av1Decoder *dec = &dec_cont->decoder;
  const int mv_cdf_offset = offsetof(struct AV1CDFs, mv_cdf);
  const int mv_cdf_size = sizeof(struct MvCDFs);
  const int cdf_size = sizeof(struct AV1CDFs);
  u32 *info_num = &dec_cont->decoder.cdfs_info.load_num;
  struct CDF_INFO *cdfs_load_info = &dec->cdfs_info.cdfs_load_info[*info_num];

  cdfs_load_info->cdf_size = cdf_size;
  cdfs_load_info->src_cdf_addr = dec->cdfs_addr;
  cdfs_load_info->dst_prob_addr = dec_cont->asic_buff->prob_tbl.bus_address;

  (*info_num)++;
  cdfs_load_info = &dec->cdfs_info.cdfs_load_info[*info_num];
  if(dec->key_frame || dec->intra_only) { // reset all cdf buff to default value
    cdfs_load_info->cdf_size = mv_cdf_size;
    cdfs_load_info->src_cdf_addr = dec->cdfs_ndvc_addr; // Intrabc cdfs
    cdfs_load_info->dst_prob_addr = dec_cont->asic_buff->prob_tbl.bus_address + mv_cdf_offset;
    (*info_num)++;
  }
}

void Av1UpdateCDFsInfo(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  const int mv_cdf_offset = offsetof(struct AV1CDFs, mv_cdf);
  const int mv_cdf_size = sizeof(struct MvCDFs);
  const int mv_cdf_end_offset = mv_cdf_offset + mv_cdf_size;
  const int cdf_size = sizeof(struct AV1CDFs);
  u32 i, j;
  addr_t asic_prob_base_addr = dec_cont->asic_buff->prob_tbl_out.bus_address;
  u32 *info_num = &dec->cdfs_info.save_num;
  struct CDF_INFO *cdfs_save_info = NULL;

  if (dec->refresh_entropy_probs == AV1_REFRESH_FRAME_CONTEXT_BACKWARD) {
    for (i = 0; i < NUM_REF_FRAMES; i++) {
      if (dec->refresh_frame_flags & (1 << i)) {
        cdfs_save_info = &dec->cdfs_info.cdfs_save_info[*info_num];
        if(dec->key_frame || dec->intra_only) { // reset all cdf buff to default value
          cdfs_save_info = &dec->cdfs_info.cdfs_save_info[*info_num];
          cdfs_save_info->cdf_size = mv_cdf_offset;
          cdfs_save_info->src_cdf_addr = asic_prob_base_addr;
          cdfs_save_info->dst_prob_addr = dec->cdfs_last_addr +
            ((addr_t)(&dec->cdfs_last[i]) - (addr_t)&dec->cdfs_last[0]);

          (*info_num)++;
          cdfs_save_info = &dec->cdfs_info.cdfs_save_info[*info_num];
          cdfs_save_info->cdf_size = cdf_size - mv_cdf_end_offset;
          cdfs_save_info->src_cdf_addr = asic_prob_base_addr + mv_cdf_end_offset;
          cdfs_save_info->dst_prob_addr  = (dec->cdfs_last_addr +
            ((addr_t)(&dec->cdfs_last[i]) - (addr_t)&dec->cdfs_last[0])) + mv_cdf_end_offset;
          (*info_num)++;
        } else {
          cdfs_save_info = &dec->cdfs_info.cdfs_save_info[*info_num];
          cdfs_save_info->cdf_size = cdf_size;
          cdfs_save_info->src_cdf_addr = asic_prob_base_addr;
          cdfs_save_info->dst_prob_addr = dec->cdfs_last_addr +
            ((addr_t)(&dec->cdfs_last[i]) - (addr_t)&dec->cdfs_last[0]);
          (*info_num)++;
        }
        for (j = 0; j < NUM_REF_FRAMES; j++) {
          if (dec->refresh_frame_flags & (1 << j)) {
            if (i != j) {
              cdfs_save_info = &dec->cdfs_info.cdfs_save_info[*info_num];
              cdfs_save_info->cdf_size = cdf_size;
              cdfs_save_info->src_cdf_addr = dec->cdfs_last_addr +
                ((addr_t)(&dec->cdfs_last[i]) - (addr_t)&dec->cdfs_last[0]);
              cdfs_save_info->dst_prob_addr  = dec->cdfs_last_addr +
                ((addr_t)(&dec->cdfs_last[j]) - (addr_t)&dec->cdfs_last[0]);
              (*info_num)++;
            }
          }
        }
        break;
      }
    }
  }
}
#endif

void Av1WriteCDFToMemory(u8 *asic_prob_base, struct Av1Decoder *dec) {
  const int mv_cdf_offset = offsetof(struct AV1CDFs, mv_cdf);

  DWLmemcpy(asic_prob_base, dec->cdfs, sizeof(struct AV1CDFs));
  if (dec->key_frame || dec->intra_only) {
    // Overwrite MV context area with intrabc MV context
    DWLmemcpy(asic_prob_base + mv_cdf_offset, dec->cdfs_ndvc,
              sizeof(struct MvCDFs));
  }
}

void Av1ReadCDFFromMemory(u8 *asic_prob_base, struct Av1Decoder *dec) {
  const int mv_cdf_offset = offsetof(struct AV1CDFs, mv_cdf);
  const int mv_cdf_size = sizeof(struct MvCDFs);
  const int mv_cdf_end_offset = mv_cdf_offset + mv_cdf_size;
  const int cdf_size = sizeof(struct AV1CDFs);
  u8 *cdf_base = (u8 *)dec->cdfs;
  //u8 *cdf_ndvc_base = (u8 *)dec->cdfs_ndvc;
#if 1
  if (dec->key_frame || dec->intra_only) {
    DWLmemcpy(cdf_base, asic_prob_base, mv_cdf_offset);
    // Read intrabc MV context
    //DWLmemcpy(cdf_ndvc_base, asic_prob_base + mv_cdf_offset, mv_cdf_size);
    DWLmemcpy(cdf_base + mv_cdf_end_offset, asic_prob_base + mv_cdf_end_offset,
              cdf_size - mv_cdf_end_offset);
  } else
#endif
  {
    DWLmemcpy(cdf_base, asic_prob_base, cdf_size);
  }
}

void Av1UpdateProbabilities(struct Av1DecContainer *dec_cont) {
  /* Read context counters from HW output memory. */
  u8 *asic_prob_base = (u8 *)dec_cont->asic_buff->prob_tbl_out.virtual_address;

  /* After decoding store the CDFs in a frame context when backward adapting */
  if (dec_cont->decoder.refresh_entropy_probs ==
      AV1_REFRESH_FRAME_CONTEXT_BACKWARD) {
    /* transfer data from device buffer to host buffer */
    DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->prob_tbl_out, 0,
                    dec_cont->asic_buff->prob_tbl_out.size, DEVICE_TO_HOST);

    for (int i = 0; i < NUM_REF_FRAMES; i++) {
      if (dec_cont->decoder.refresh_frame_flags & (1 << i)) {
        Av1GetCDFs(&dec_cont->decoder, i);
        // Read CDFs back from HW memory
        Av1ReadCDFFromMemory(asic_prob_base, &dec_cont->decoder);
        Av1StoreCDFs(&dec_cont->decoder);
        break;
      }
    }
  }
}

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
void Av1SetEmptyPictureData(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 index = asic_buff->out_buffer_i;

  DWLmemset(asic_buff->pictures[index].virtual_address, SET_EMPTY_PICTURE_DATA,
            asic_buff->pictures[index].size);
  DWLmemset(asic_buff->pictures_c[index].virtual_address,
            SET_EMPTY_PICTURE_DATA, asic_buff->pictures_c[index].size);
  DWLmemset(asic_buff->dir_mvs[index].virtual_address, SET_EMPTY_PICTURE_DATA,
            asic_buff->dir_mvs[index].size);

  if (dec_cont->pp_enabled) {
    DWLmemset(asic_buff->raster_luma[index].virtual_address,
              SET_EMPTY_PICTURE_DATA, asic_buff->raster_luma[index].size);
    DWLmemset(asic_buff->raster_chroma[index].virtual_address,
              SET_EMPTY_PICTURE_DATA, asic_buff->raster_chroma[index].size);
  }
}
#endif

static bool CheckTileWidth(struct Av1Decoder *dec, int width, bool leftmost) {
  bool valid = TRUE;
  if (!leftmost && dec->sb_size == 0 && dec->superres_is_scaled && width == 1) {
    APITRACE("%s","WARNING: Superres used and tile width == 64\n");
    valid = FALSE;
  }

  const int sb_size_log2 = dec->sb_size ? 7 : 6;
  int tile_width_pixels = (width << sb_size_log2);
  if (dec->superres_is_scaled) {
    tile_width_pixels =
        (tile_width_pixels * (9 + dec->scale_denom_minus9) + 4) / 8;
  }
  if (tile_width_pixels > 4096) {
    if (dec->superres_is_scaled)
      APITRACE("%s","WARNING: Tile width after superres > 4096\n");
    else
      APITRACE("%s","WARNING: Tile width > 4096\n");
    valid = FALSE;
  }
  return valid;
}

void Av1AsicSetTileInfoRegs(struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, struct SwRegisters *sw_ctrl) {
  int transpose = dec->tile_transpose;

  size_t context_update_tile_id = dec->context_update_tile_id;
  size_t context_update_y = context_update_tile_id / dec->av1_tile_cols;
  size_t context_update_x = context_update_tile_id % dec->av1_tile_cols;
  if (transpose) {
    context_update_tile_id =
        context_update_x * dec->av1_tile_rows + context_update_y;
  }

  sw_ctrl->sw_tile_enable = dec->log2_tile_columns || dec->log2_tile_rows;
  // sw_ctrl->sw_log2_tile_cols = dec->log2_tile_columns;
  sw_ctrl->sw_num_tile_rows_8k_av1 = dec->av1_tile_rows;
  sw_ctrl->sw_num_tile_cols_8k = dec->av1_tile_cols;
  sw_ctrl->sw_context_update_tile_id = context_update_tile_id;
  sw_ctrl->sw_tile_transpose = transpose;

  if (sw_ctrl->sw_tile_enable) APITRACE("%s","NOTICE: tile enabled.\n");

  REG_WRITE_ADDR(sw_ctrl->sw_tile_base, asic_buff->tile_info.bus_address);

}

void Av1AsicSetTileInfoMem(struct Av1DecContainer *dec_cont,const struct Av1DecInput *input) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  const int transpose = dec->tile_transpose;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (dec_cont->low_latency) {
    dec_cont->lltileinfo.tileinfo = (u8 *)asic_buff->tile_info.virtual_address;
    /* flag for update thread, means all variables are ready. */
    dec_cont->lltileinfo.update_rdy = 1;
    return;
  }
  int tmp = dec->frame_tag_size + dec->offset_to_dct_parts;
  int stream_len=input->buff_len-tmp;

#if 0
  DWLMarkMemory(dec_cont->dwl, &asic_buff->tile_info,
                DWL_HW_MARK_HOST_TO_DEVICE | DWL_HW_MARK_ONE_SHOT);
#endif
  u8 *p1 = (u8 *)asic_buff->tile_info.virtual_address;
  // u8 *p2 = p1 + 128*2;

  int size0 = transpose ? dec->av1_tile_cols : dec->av1_tile_rows;
  int size1 = transpose ? dec->av1_tile_rows : dec->av1_tile_cols;

  dec->not_valid_tile_dimension = FALSE;
  // Write tile dimensions
  for (int tile0 = 0; tile0 < size0; tile0++) {
    for (int tile1 = 0; tile1 < size1; tile1++) {
      int tile_y = transpose ? tile1 : tile0;
      int tile_x = transpose ? tile0 : tile1;
      int tile_id = transpose ? tile1 * size0 + tile0 : tile0 * size1 + tile1;

      u32 y0 = dec->tile_row_start_sb[tile_y];
      u32 y1 = dec->tile_row_start_sb[tile_y + 1];
      u32 x0 = dec->tile_col_start_sb[tile_x];
      u32 x1 = dec->tile_col_start_sb[tile_x + 1];

      bool leftmost = (tile_x == dec->av1_tile_cols - 1);
      if (!dec->not_valid_tile_dimension)
        dec->not_valid_tile_dimension = !CheckTileWidth(dec, x1 - x0, leftmost);
      if ((x0 << (dec->sb_size ? 7 : 6)) >= dec->width ||
          (y0 << (dec->sb_size ? 7 : 6)) >= dec->height)
        dec->not_valid_tile_dimension = 1;

      // tile size in SB units (width,height)
      *p1++ = x1 - x0;
      *p1++ = 0;
      *p1++ = 0;
      *p1++ = 0;
      *p1++ = y1 - y0;
      *p1++ = 0;
      *p1++ = 0;
      *p1++ = 0;

      // tile start position (offset from sw_stream0_base)
      u32 start = dec->tile_offset_start[tile_id];
      *p1++ = start & 255;
      *p1++ = (start >> 8) & 255;
      *p1++ = (start >> 16) & 255;
      *p1++ = (start >> 24) & 255;
      if (!dec->not_valid_tile_dimension)
      {
        if((start+1)>stream_len)
          dec->not_valid_tile_dimension = 1;
      }

      // # of bytes in tile data
      u32 end = dec->tile_offset_end[tile_id];
      *p1++ = end & 255;
      *p1++ = (end >> 8) & 255;
      *p1++ = (end >> 16) & 255;
      *p1++ = (end >> 24) & 255;
      if (!dec->not_valid_tile_dimension)
      {
        if(end>stream_len)
          dec->not_valid_tile_dimension = 1;
      }
    }
  }

  DWLDMATransData(dec_cont->dwl, &asic_buff->tile_info, 0, asic_buff->tile_info.size, HOST_TO_DEVICE);
}

#define MAX_ACTIVE_REFS AV1_ACTIVE_REFS_EX

int GetRelativeDist(struct Av1Decoder *dec, int a, int b) {
  if (!dec->enable_order_hint) return 0;
  const int bits = dec->order_hint_bits_minus1;

  int diff = a - b;
  int m = 1 << bits;
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

#define POPULATE_REF_OFFSET(index)                                             \
  {                                                                            \
    int ref_offset[MAX_REF_FRAMES_EX - 1];                                     \
    Av1PopulateRefOffset(dec,                                                  \
                         &(asic_buff->picture_info[refs_selected[(index)-1]]), \
                         ref_offset);                                          \
    sw_ctrl->sw_mf##index##_last_offset = ref_offset[0];                       \
    sw_ctrl->sw_mf##index##_last2_offset = ref_offset[1];                      \
    sw_ctrl->sw_mf##index##_last3_offset = ref_offset[2];                      \
    sw_ctrl->sw_mf##index##_golden_offset = ref_offset[3];                     \
    sw_ctrl->sw_mf##index##_bwdref_offset = ref_offset[4];                     \
    sw_ctrl->sw_mf##index##_altref2_offset = ref_offset[5];                    \
    sw_ctrl->sw_mf##index##_altref_offset = ref_offset[6];                     \
  }

void Av1PopulateRefOffset(struct Av1Decoder *dec, struct Av1DecPicture *pi,
                          int *ref_offset) {
  ref_offset[0] = GetRelativeDist(dec, pi->frame_offset, pi->lst_frame_offset);
  ref_offset[1] = GetRelativeDist(dec, pi->frame_offset, pi->lst2_frame_offset);
  ref_offset[2] = GetRelativeDist(dec, pi->frame_offset, pi->lst3_frame_offset);
  ref_offset[3] = GetRelativeDist(dec, pi->frame_offset, pi->gld_frame_offset);
  ref_offset[4] = GetRelativeDist(dec, pi->frame_offset, pi->bwd_frame_offset);
  ref_offset[5] = GetRelativeDist(dec, pi->frame_offset, pi->alt2_frame_offset);
  ref_offset[6] = GetRelativeDist(dec, pi->frame_offset, pi->alt_frame_offset);
}

static void set_ref_width(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_ref0_width = val;
  } else if (i == 1) {
    sw_ctrl->sw_ref1_width = val;
  } else if (i == 2) {
    sw_ctrl->sw_ref2_width = val;
  } else if (i == 3) {
    sw_ctrl->sw_ref3_width = val;
  } else if (i == 4) {
    sw_ctrl->sw_ref4_width = val;
  } else if (i == 5) {
    sw_ctrl->sw_ref5_width = val;
  } else if (i == 6) {
    sw_ctrl->sw_ref6_width = val;
  } else {
    HLS_CHECK("%s","Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_height(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_ref0_height = val;
  } else if (i == 1) {
    sw_ctrl->sw_ref1_height = val;
  } else if (i == 2) {
    sw_ctrl->sw_ref2_height = val;
  } else if (i == 3) {
    sw_ctrl->sw_ref3_height = val;
  } else if (i == 4) {
    sw_ctrl->sw_ref4_height = val;
  } else if (i == 5) {
    sw_ctrl->sw_ref5_height = val;
  } else if (i == 6) {
    sw_ctrl->sw_ref6_height = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_hor_scale(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_ref0_hor_scale = val;
  } else if (i == 1) {
    sw_ctrl->sw_ref1_hor_scale = val;
  } else if (i == 2) {
    sw_ctrl->sw_ref2_hor_scale = val;
  } else if (i == 3) {
    sw_ctrl->sw_ref3_hor_scale = val;
  } else if (i == 4) {
    sw_ctrl->sw_ref4_hor_scale = val;
  } else if (i == 5) {
    sw_ctrl->sw_ref5_hor_scale = val;
  } else if (i == 6) {
    sw_ctrl->sw_ref6_hor_scale = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_ver_scale(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_ref0_ver_scale = val;
  } else if (i == 1) {
    sw_ctrl->sw_ref1_ver_scale = val;
  } else if (i == 2) {
    sw_ctrl->sw_ref2_ver_scale = val;
  } else if (i == 3) {
    sw_ctrl->sw_ref3_ver_scale = val;
  } else if (i == 4) {
    sw_ctrl->sw_ref4_ver_scale = val;
  } else if (i == 5) {
    sw_ctrl->sw_ref5_ver_scale = val;
  } else if (i == 6) {
    sw_ctrl->sw_ref6_ver_scale = val;
  } else {
    HLS_CHECK("%s","Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_lum_base(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_ybase = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_ybase = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_ybase = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_ybase = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_ybase = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_ybase = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_ybase = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_lum_base_msb(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_ybase_msb = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_ybase_msb = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_ybase_msb = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_ybase_msb = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_ybase_msb = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_ybase_msb = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_ybase_msb = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_cb_base(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_cbase = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_cbase = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_cbase = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_cbase = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_cbase = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_cbase = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_cbase = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_cb_base_msb(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_cbase_msb = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_cbase_msb = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_cbase_msb = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_cbase_msb = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_cbase_msb = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_cbase_msb = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_cbase_msb = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_ty_base(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_tybase = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_tybase = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_tybase = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_tybase = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_tybase = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_tybase = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_tybase = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_dbase(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_dbase = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_dbase = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_dbase = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_dbase = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_dbase = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_dbase = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_dbase = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_dbase_msb(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_dbase_msb = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_dbase_msb = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_dbase_msb = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_dbase_msb = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_dbase_msb = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_dbase_msb = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_dbase_msb = val;
  } else {
    HLS_CHECK("%s","Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_ty_base_msb(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_tybase_msb = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_tybase_msb = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_tybase_msb = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_tybase_msb = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_tybase_msb = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_tybase_msb = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_tybase_msb = val;
  } else {
    HLS_CHECK("%s", "Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_tc_base(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_tcbase = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_tcbase = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_tcbase = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_tcbase = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_tcbase = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_tcbase = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_tcbase = val;
  } else {
    HLS_CHECK("%s","Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_tc_base_msb(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_refer0_tcbase_msb = val;
  } else if (i == 1) {
    sw_ctrl->sw_refer1_tcbase_msb = val;
  } else if (i == 2) {
    sw_ctrl->sw_refer2_tcbase_msb = val;
  } else if (i == 3) {
    sw_ctrl->sw_refer3_tcbase_msb = val;
  } else if (i == 4) {
    sw_ctrl->sw_refer4_tcbase_msb = val;
  } else if (i == 5) {
    sw_ctrl->sw_refer5_tcbase_msb = val;
  } else if (i == 6) {
    sw_ctrl->sw_refer6_tcbase_msb = val;
  } else {
    HLS_CHECK("%s","Error: trying to set invalid reference index.\n");
  }
}

static void set_ref_sign_bias(struct SwRegisters *sw_ctrl, int i, int val) {
  if (i == 0) {
    sw_ctrl->sw_ref0_sign_bias = val;
  } else if (i == 1) {
    sw_ctrl->sw_ref1_sign_bias = val;
  } else if (i == 2) {
    sw_ctrl->sw_ref2_sign_bias = val;
  } else if (i == 3) {
    sw_ctrl->sw_ref3_sign_bias = val;
  } else if (i == 4) {
    sw_ctrl->sw_ref4_sign_bias = val;
  } else if (i == 5) {
    sw_ctrl->sw_ref5_sign_bias = val;
  } else if (i == 6) {
    sw_ctrl->sw_ref6_sign_bias = val;
  } else {
    HLS_CHECK("%s","Error: trying to set invalid reference index.\n");
  }
}

#define MAX_FRAME_DISTANCE 31
void Av1AsicSetReferenceFrames(struct Av1DecContainer *dec_cont) {
  u32 tmp1, tmp2, i;
  u32 cur_height, cur_width;
  i32 replace_index = EC_INVALID_IDX;
  u8 replace_flags = 0;
  u8 fake_table_flags = 0;
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  i32 index_ref[MAX_ACTIVE_REFS];
  i32 index_info[MAX_ACTIVE_REFS];
  i32 ref_count[MAX_PIC_BUFFERS] = {0};
  struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;

  bool ref_scale_e = TRUE;

  for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    index_ref[i] = Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
    if (index_ref[i] >= 0)
      ref_count[index_ref[i]]++;
  }

  if (!dec->allow_intrabc) {
    for (i = 0; i < MAX_PIC_BUFFERS; i++) {
      if (ref_count[i])
        sw_ctrl->sw_ref_frames++;
    }
  } else
    sw_ctrl->sw_ref_frames = 1;

  if (!dec_cont->pp_enabled) {
    for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
      index_info[i] = index_ref[i];
    }
  } else {
    for (i = 0; i < AV1_ACTIVE_REFS_EX; i++)
      index_info[i] = Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
  }

  /* unrounded picture dimensions */
  cur_width = dec_cont->decoder.width;
  cur_height = dec_cont->decoder.height;

  // u8 prev_valid = 0;
  for (i = LAST_FRAME; i < MAX_REF_FRAMES_EX; i++) {
    u32 ref = i - 1;
    u32 index;
    if (dec->allow_intrabc) {
      /* intrabc frame only ref itself. */
      index = asic_buff->out_buffer_i;
      tmp1 = cur_width;
      tmp2 = cur_height;
    } else {
      index = index_info[ref];
      tmp1 = asic_buff->picture_info[index].superres_width;
      tmp2 = asic_buff->picture_info[index].coded_height;
      index = index_ref[ref];
    }

    set_ref_width(sw_ctrl, ref, tmp1);
    set_ref_height(sw_ctrl, ref, tmp2);
    tmp1 = ((tmp1 << AV1_REF_SCALE_SHIFT) + cur_width / 2) / cur_width;
    tmp2 = ((tmp2 << AV1_REF_SCALE_SHIFT) + cur_height / 2) / cur_height;

    set_ref_hor_scale(sw_ctrl, ref, tmp1);
    set_ref_ver_scale(sw_ctrl, ref, tmp2);
    // if (tmp1 != (1 << AV1_REF_SCALE_SHIFT) ||
    //     tmp2 != (1 << AV1_REF_SCALE_SHIFT)) {
    //   ref_scale_e = TRUE;
    // }

    // if (asic_buff->pictures[index].bus_address ==
    //     asic_buff->pictures[asic_buff->prev_out_buffer_i].bus_address) {
    //   prev_valid = 1;
    // }

    /* EC: DEC_EC_REF_REPLACE or DEC_EC_REF_REPLACE_ANYWAY*/
    if (!dec->allow_intrabc) { /* intrabc frame: don.t need do this. */
      if (dec_cont->error_policy & DEC_EC_REF_REPLACE ||
          dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY) {
        if (asic_buff->picture_info[index].error_info != DEC_NO_ERROR) {
          replace_index = Av1ReplaceRefPic(dec_cont, index_info, index);
          index = replace_index != EC_INVALID_IDX ? replace_index : index;
          replace_flags = 1;
        }
      }
      /* not find avalible replace ref */
      if (replace_flags == 1 && replace_index == EC_INVALID_IDX) {
        if (dec_cont->use_video_compressor) {
          /* have rfc:
           * 1. DEC_EC_REF_REPLACE: will seek next I, can't run asic, so do nothing in here.
           * 2. DEC_EC_REF_REPLACE_ANYWAY: use FALE_TABLE.
           */
          fake_table_flags = 1;
        } else {
          /* no rfc: just use error ref, so do nothing in here. */
        }
      }
    }
    /* set ref frame register: luma & chroma. */
    REG_WRITE_ADDR_AR(set_ref_lum_base, ref,
                       asic_buff->pictures[index].bus_address);
    REG_WRITE_ADDR_AR(set_ref_cb_base, ref,
                       asic_buff->pictures[index].bus_address +
                           asic_buff->pictures_c_offset[index]);

    /* set dmv register. */
    REG_WRITE_ADDR_AR(set_ref_dbase, ref,
                       asic_buff->dpb_parasitic_buf[index].bus_address +
                              asic_buff->dir_mvs_offset[index]);

    /* set rfc table. */
    if (dec_cont->use_video_compressor) {
#ifndef USE_FAKE_RFC_TABLE
      REG_WRITE_ADDR_AR(set_ref_ty_base, ref,
                         asic_buff->pictures[index].bus_address +
                             asic_buff->cbs_y_tbl_offset[index]);
      REG_WRITE_ADDR_AR(set_ref_tc_base, ref,
                         asic_buff->pictures[index].bus_address +
                             asic_buff->cbs_c_tbl_offset[index]);
#else
      if (dec_cont->decoder.key_frame || fake_table_flags == 1) {
        REG_WRITE_ADDR_AR(set_ref_ty_base, ref,
                           asic_buff->fake_rfc_tbl.bus_address);
        REG_WRITE_ADDR_AR(set_ref_tc_base, ref,
                           asic_buff->fake_rfc_tbl.bus_address +
                                           asic_buff->tbl_sizey);
#ifdef MEMSET_FAKE_REF_BUFFER
        if (fake_table_flags == 1) {
          /* try clean this ref buffer, not use the garbage */
          DWLLinearMemset(dec_cont->dwl, &asic_buff->pictures[index], 0, 0x80, asic_buff->cbs_y_tbl_offset[index]);
        }
#endif
      } else {
        REG_WRITE_ADDR_AR(set_ref_ty_base, ref,
                           asic_buff->pictures[index].bus_address +
                               asic_buff->cbs_y_tbl_offset[index]);
        REG_WRITE_ADDR_AR(set_ref_tc_base, ref,
                           asic_buff->pictures[index].bus_address +
                               asic_buff->cbs_c_tbl_offset[index]);
      }
#endif
    }

    set_ref_sign_bias(sw_ctrl, ref, dec->ref_frame_sign_bias[i]);
  }

  // if (dec->allow_intrabc) {
  //   asic_buff->prev_out_buffer_i = asic_buff->out_buffer_i;
  // } else if (!prev_valid) {
  //   asic_buff->prev_out_buffer_i = index_ref[0];  // LAST
  // }

  {
    int gld_buf_idx = index_info[GOLDEN_FRAME_EX - LAST_FRAME];
    int alt_buf_idx = index_info[ALTREF_FRAME_EX - LAST_FRAME];
    int lst_buf_idx = index_info[LAST_FRAME - LAST_FRAME];
    int bwd_buf_idx = index_info[BWDREF_FRAME_EX - LAST_FRAME];
    int alt2_buf_idx = index_info[ALTREF2_FRAME_EX - LAST_FRAME];
    int lst2_buf_idx = index_info[LAST2_FRAME_EX - LAST_FRAME];

    int cur_frame_offset = dec_cont->decoder.frame_offset;
    int alt_frame_offset = 0;
    int gld_frame_offset = 0;
    int bwd_frame_offset = 0;
    int alt2_frame_offset = 0;
    int refs_selected[3] = {0, 0, 0};
    int cur_mi_cols = (dec_cont->decoder.width + 7) >> 3;
    int cur_mi_rows = (dec_cont->decoder.height + 7) >> 3;
    u8 mf_types[3] = {0, 0, 0};

    if (alt_buf_idx != kReferenceNotSet)
      alt_frame_offset = asic_buff->picture_info[alt_buf_idx].frame_offset;
    if (gld_buf_idx != kReferenceNotSet)
      gld_frame_offset = asic_buff->picture_info[gld_buf_idx].frame_offset;
    if (bwd_buf_idx != kReferenceNotSet)
      bwd_frame_offset = asic_buff->picture_info[bwd_buf_idx].frame_offset;
    if (alt2_buf_idx != kReferenceNotSet)
      alt2_frame_offset = asic_buff->picture_info[alt2_buf_idx].frame_offset;

    int ref_stamp = 2;
    int ref_ind = 0;

    if (lst_buf_idx != kReferenceNotSet) {
      const int alt_frame_offset_in_lst =
          asic_buff->picture_info[lst_buf_idx].alt_frame_offset;

      const int is_lst_overlay = (alt_frame_offset_in_lst == gld_frame_offset);
      if (!is_lst_overlay) {
        int lst_mi_cols =
            (asic_buff->picture_info[lst_buf_idx].coded_width + 7) >> 3;
        int lst_mi_rows =
            (asic_buff->picture_info[lst_buf_idx].coded_height + 7) >> 3;
        // TODO(stan): what's the difference btw key_frame and intra_only?
        int lst_intra_only =
            asic_buff->picture_info[lst_buf_idx].intra_only ||
            asic_buff->picture_info[lst_buf_idx].is_intra_frame;
        if (lst_mi_cols == cur_mi_cols && lst_mi_rows == cur_mi_rows &&
            !lst_intra_only) {
          mf_types[ref_ind] = LAST_FRAME;
          refs_selected[ref_ind++] = lst_buf_idx;
        }
      }
      ref_stamp--;
    }

    if (GetRelativeDist(dec, bwd_frame_offset, cur_frame_offset) > 0) {
      int bwd_mi_cols =
          (asic_buff->picture_info[bwd_buf_idx].coded_width + 7) >> 3;
      int bwd_mi_rows =
          (asic_buff->picture_info[bwd_buf_idx].coded_height + 7) >> 3;
      int bwd_intra_only = asic_buff->picture_info[bwd_buf_idx].intra_only ||
                           asic_buff->picture_info[bwd_buf_idx].is_intra_frame;
      if (bwd_mi_cols == cur_mi_cols && bwd_mi_rows == cur_mi_rows &&
          !bwd_intra_only) {
        mf_types[ref_ind] = BWDREF_FRAME_EX;
        refs_selected[ref_ind++] = bwd_buf_idx;
        ref_stamp--;
      }
    }

    if (GetRelativeDist(dec, alt2_frame_offset, cur_frame_offset) > 0) {
      int alt2_mi_cols =
          (asic_buff->picture_info[alt2_buf_idx].coded_width + 7) >> 3;
      int alt2_mi_rows =
          (asic_buff->picture_info[alt2_buf_idx].coded_height + 7) >> 3;
      int alt2_intra_only =
          asic_buff->picture_info[alt2_buf_idx].intra_only ||
          asic_buff->picture_info[alt2_buf_idx].is_intra_frame;
      if (alt2_mi_cols == cur_mi_cols && alt2_mi_rows == cur_mi_rows &&
          !alt2_intra_only) {
        mf_types[ref_ind] = ALTREF2_FRAME_EX;
        refs_selected[ref_ind++] = alt2_buf_idx;
        ref_stamp--;
      }
    }

    if (GetRelativeDist(dec, alt_frame_offset, cur_frame_offset) > 0 &&
        ref_stamp >= 0) {
      int alt_mi_cols =
          (asic_buff->picture_info[alt_buf_idx].coded_width + 7) >> 3;
      int alt_mi_rows =
          (asic_buff->picture_info[alt_buf_idx].coded_height + 7) >> 3;
      int alt_intra_only = asic_buff->picture_info[alt_buf_idx].intra_only ||
                           asic_buff->picture_info[alt_buf_idx].is_intra_frame;
      if (alt_mi_cols == cur_mi_cols && alt_mi_rows == cur_mi_rows &&
          !alt_intra_only) {
        mf_types[ref_ind] = ALTREF_FRAME_EX;
        refs_selected[ref_ind++] = alt_buf_idx;
        ref_stamp--;
      }
    }

    if (ref_stamp >= 0 && lst2_buf_idx != kReferenceNotSet) {
      int lst2_mi_cols =
          (asic_buff->picture_info[lst2_buf_idx].coded_width + 7) >> 3;
      int lst2_mi_rows =
          (asic_buff->picture_info[lst2_buf_idx].coded_height + 7) >> 3;
      int lst2_intra_only =
          asic_buff->picture_info[lst2_buf_idx].intra_only ||
          asic_buff->picture_info[lst2_buf_idx].is_intra_frame;
      if (lst2_mi_cols == cur_mi_cols && lst2_mi_rows == cur_mi_rows &&
          !lst2_intra_only) {
        mf_types[ref_ind] = LAST2_FRAME_EX;
        refs_selected[ref_ind++] = lst2_buf_idx;
        ref_stamp--;
      }
    }

    int cur_offset[MAX_REF_FRAMES_EX - 1];
    int cur_roffset[MAX_REF_FRAMES_EX - 1];
    for (int rf = 0; rf < MAX_REF_FRAMES_EX - 1; ++rf) {
      int buf_idx = index_info[rf];
      if (buf_idx != kReferenceNotSet) {
        cur_offset[rf] =
            GetRelativeDist(dec, cur_frame_offset,
                            asic_buff->picture_info[buf_idx].frame_offset);
        cur_roffset[rf] =
            GetRelativeDist(dec, asic_buff->picture_info[buf_idx].frame_offset,
                            cur_frame_offset);
      } else {
        cur_offset[rf] = 0;
        cur_roffset[rf] = 0;
      }
    }

    sw_ctrl->sw_use_temporal0_mvs = 0;
    sw_ctrl->sw_use_temporal1_mvs = 0;
    sw_ctrl->sw_use_temporal2_mvs = 0;
    sw_ctrl->sw_use_temporal3_mvs = 0;

    if (dec->use_ref_frame_mvs && ref_ind > 0 &&
        cur_offset[mf_types[0] - LAST_FRAME] <= MAX_FRAME_DISTANCE &&
        cur_offset[mf_types[0] - LAST_FRAME] >= -MAX_FRAME_DISTANCE) {
      sw_ctrl->sw_use_temporal0_mvs = 1;
      POPULATE_REF_OFFSET(1)
    }

    if (dec->use_ref_frame_mvs && ref_ind > 1 &&
        cur_offset[mf_types[1] - LAST_FRAME] <= MAX_FRAME_DISTANCE &&
        cur_offset[mf_types[1] - LAST_FRAME] >= -MAX_FRAME_DISTANCE) {
      sw_ctrl->sw_use_temporal1_mvs = 1;
      POPULATE_REF_OFFSET(2)
    }

    if (dec->use_ref_frame_mvs && ref_ind > 2 &&
        cur_offset[mf_types[2] - LAST_FRAME] <= MAX_FRAME_DISTANCE &&
        cur_offset[mf_types[2] - LAST_FRAME] >= -MAX_FRAME_DISTANCE) {
      sw_ctrl->sw_use_temporal2_mvs = 1;
      POPULATE_REF_OFFSET(3)
    }

    // Pass one additional frame that will contain the segment information
    if (dec->segment_enabled &&
        dec->primary_ref_frame < ALLOWED_REFS_PER_FRAME_EX) {
      // Primary ref frame is zero based
      int prim_buf_idx = index_ref[dec->primary_ref_frame];
      if (prim_buf_idx != kReferenceNotSet) {
        REG_WRITE_ADDR(sw_ctrl->sw_segment_read_base,
                       asic_buff->dpb_parasitic_buf[prim_buf_idx].bus_address +
                           asic_buff->dir_mvs_offset[prim_buf_idx]);
        sw_ctrl->sw_use_temporal3_mvs = 1;
      }
    }
    if (dec->primary_ref_frame < ALLOWED_REFS_PER_FRAME_EX) {
      int prim_buf_idx = index_info[dec->primary_ref_frame];
      dec->resolution_change =
          cur_mi_cols !=
              (int)((asic_buff->picture_info[prim_buf_idx].coded_width + 7) >>
                    3) ||
          cur_mi_rows !=
              (int)((asic_buff->picture_info[prim_buf_idx].coded_height + 7) >>
                    3);
    }

    // TODO(vitvitskyy): these offsets can not be negative? change the type
    // from sai9 to uai9
    sw_ctrl->sw_cur_last_offset = cur_offset[0];
    sw_ctrl->sw_cur_last2_offset = cur_offset[1];
    sw_ctrl->sw_cur_last3_offset = cur_offset[2];
    sw_ctrl->sw_cur_golden_offset = cur_offset[3];
    sw_ctrl->sw_cur_bwdref_offset = cur_offset[4];
    sw_ctrl->sw_cur_altref2_offset = cur_offset[5];
    sw_ctrl->sw_cur_altref_offset = cur_offset[6];

    sw_ctrl->sw_cur_last_roffset = cur_roffset[0];
    sw_ctrl->sw_cur_last2_roffset = cur_roffset[1];
    sw_ctrl->sw_cur_last3_roffset = cur_roffset[2];
    sw_ctrl->sw_cur_golden_roffset = cur_roffset[3];
    sw_ctrl->sw_cur_bwdref_roffset = cur_roffset[4];
    sw_ctrl->sw_cur_altref2_roffset = cur_roffset[5];
    sw_ctrl->sw_cur_altref_roffset = cur_roffset[6];

    /* Index start from 0 */
    sw_ctrl->sw_mf1_type = mf_types[0] - LAST_FRAME;
    sw_ctrl->sw_mf2_type = mf_types[1] - LAST_FRAME;
    sw_ctrl->sw_mf3_type = mf_types[2] - LAST_FRAME;
  }

  sw_ctrl->sw_ref_scaling_enable = ref_scale_e;
}
#undef MAX_FRAME_DISTANCE

void Av1AsicSetSecondaryOutput(struct Av1DecContainer *dec_cont, const u32 bypass_filter) {
  // struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
   struct DWLLinearMem *pp_buffer = &dec_cont->asic_buff->pp_pictures[asic_buff->out_pp_buffer_i];
  // AV1 seems to allow non-showable frames to be displayed with
  // show_existing_frame (but AV1 doesn't), so enable secondary output
  // regardless in AV1 mode
  // FIXME(yc): PP buffer is also needed here
  if (dec_cont->pp_enabled) {
    SetDecRegister(dec_cont->av1_regs[0], HWIF_PP_IN_FORMAT_U, 1);
    u32 pp_out_ctrl = dec_cont->asic_buff->pp_out_ctrl[asic_buff->out_pp_buffer_i];
    struct PpParams pp_args = {dec_cont->hw_feature,
                               dec_cont->ppu_cfg,
                               pp_buffer,
                               0,
                               0,
                               pp_out_ctrl
                              };
    PPSetRegs(dec_cont->av1_regs[0], &pp_args);
  }
}

void Av1AsicSetSegmentation(struct Av1Decoder *dec, struct SwRegisters *sw_ctrl) {
  u32 s;
  u32 segval[MAX_MB_SEGMENTS][SEG_AV1_LVL_MAX];

  /* Segmentation */
  sw_ctrl->sw_segment_e = dec->segment_enabled;
  sw_ctrl->sw_segment_upd_e = dec->segment_map_update;
  sw_ctrl->sw_segment_temp_upd_e = dec->segment_map_temporal_update;
  sw_ctrl->sw_error_resilient = dec->error_resilient || dec->resolution_change;

  if ((dec->key_frame || dec->intra_only) || sw_ctrl->sw_error_resilient) {
    sw_ctrl->sw_use_temporal3_mvs = 0;
  }

  sw_ctrl->sw_filt_level0 = dec->loop_filter_level;
  sw_ctrl->sw_filt_level1 = dec->loop_filter_level_r;
  sw_ctrl->sw_filt_level2 = dec->loop_filter_level_u;
  sw_ctrl->sw_filt_level3 = dec->loop_filter_level_v;
  u8 segsign = 0;
  /* Set filter level and QP for every segment ID. Initialize all
   * segments with default QP and filter level. */
  for (s = 0; s < MAX_MB_SEGMENTS; s++) {
    segval[s][SEG_AV1_LVL_ALT_Q] = 0;
    segval[s][SEG_AV1_LVL_ALT_LF_Y_V] = 0;
    segval[s][SEG_AV1_LVL_ALT_LF_Y_H] = 0;
    segval[s][SEG_AV1_LVL_ALT_LF_U] = 0;
    segval[s][SEG_AV1_LVL_ALT_LF_V] = 0;
    segval[s][SEG_AV1_LVL_REF_FRAME] = 0; /* segment ref_frame disabled */
    segval[s][SEG_AV1_LVL_SKIP] = 0;      /* segment skip disabled */
    segval[s][SEG_AV1_LVL_GLOBALMV] = 0;  /* global motion */
  }
  /* If a feature is enabled for a segment, overwrite the default. */
  if (dec->segment_enabled) {
    i32(*segdata)[SEG_AV1_LVL_MAX] = dec->segment_feature_data;

    for (s = 0; s < MAX_MB_SEGMENTS; s++) {
      if (dec->segment_feature_enable[s][SEG_AV1_LVL_ALT_Q]) {
        segval[s][SEG_AV1_LVL_ALT_Q] =
            CLIP3(0, 255, ABS(segdata[s][SEG_AV1_LVL_ALT_Q]));
        segsign |= (segdata[s][SEG_AV1_LVL_ALT_Q] < 0) << s;
      }

      if (dec->segment_feature_enable[s][SEG_AV1_LVL_ALT_LF_Y_V])
        segval[s][SEG_AV1_LVL_ALT_LF_Y_V] =
            CLIP3(-63, 63, segdata[s][SEG_AV1_LVL_ALT_LF_Y_V]);

      if (dec->segment_feature_enable[s][SEG_AV1_LVL_ALT_LF_Y_H])
        segval[s][SEG_AV1_LVL_ALT_LF_Y_H] =
            CLIP3(-63, 63, segdata[s][SEG_AV1_LVL_ALT_LF_Y_H]);

      if (dec->segment_feature_enable[s][SEG_AV1_LVL_ALT_LF_U])
        segval[s][SEG_AV1_LVL_ALT_LF_U] =
            CLIP3(-63, 63, segdata[s][SEG_AV1_LVL_ALT_LF_U]);

      if (dec->segment_feature_enable[s][SEG_AV1_LVL_ALT_LF_V])
        segval[s][SEG_AV1_LVL_ALT_LF_V] =
            CLIP3(-63, 63, segdata[s][SEG_AV1_LVL_ALT_LF_V]);

      if (!dec->key_frame &&
          dec->segment_feature_enable[s][SEG_AV1_LVL_REF_FRAME])
        segval[s][SEG_AV1_LVL_REF_FRAME] =
            segdata[s][SEG_AV1_LVL_REF_FRAME] + 1;

      if (dec->segment_feature_enable[s][SEG_AV1_LVL_SKIP])
        segval[s][SEG_AV1_LVL_SKIP] = 1;
      if (dec->segment_feature_enable[s][SEG_AV1_LVL_GLOBALMV])
        segval[s][SEG_AV1_LVL_GLOBALMV] = 1;
    }
  }

  sw_ctrl->sw_seg_quant_sign = segsign;
  /* Write QP, filter level, ref frame and skip for every segment */
  sw_ctrl->sw_quant_seg0 = segval[0][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg0 = segval[0][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg0 = segval[0][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg0 = segval[0][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg0 = segval[0][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg0 = segval[0][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg0 = segval[0][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg0 = segval[0][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg1 = segval[1][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg1 = segval[1][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg1 = segval[1][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg1 = segval[1][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg1 = segval[1][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg1 = segval[1][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg1 = segval[1][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg1 = segval[1][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg2 = segval[2][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg2 = segval[2][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg2 = segval[2][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg2 = segval[2][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg2 = segval[2][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg2 = segval[2][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg2 = segval[2][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg2 = segval[2][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg3 = segval[3][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg3 = segval[3][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg3 = segval[3][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg3 = segval[3][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg3 = segval[3][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg3 = segval[3][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg3 = segval[3][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg3 = segval[3][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg4 = segval[4][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg4 = segval[4][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg4 = segval[4][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg4 = segval[4][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg4 = segval[4][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg4 = segval[4][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg4 = segval[4][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg4 = segval[4][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg5 = segval[5][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg5 = segval[5][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg5 = segval[5][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg5 = segval[5][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg5 = segval[5][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg5 = segval[5][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg5 = segval[5][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg5 = segval[5][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg6 = segval[6][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg6 = segval[6][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg6 = segval[6][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg6 = segval[6][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg6 = segval[6][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg6 = segval[6][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg6 = segval[6][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg6 = segval[6][SEG_AV1_LVL_GLOBALMV];

  sw_ctrl->sw_quant_seg7 = segval[7][SEG_AV1_LVL_ALT_Q];
  sw_ctrl->sw_filt_level_delta0_seg7 = segval[7][SEG_AV1_LVL_ALT_LF_Y_V];
  sw_ctrl->sw_filt_level_delta1_seg7 = segval[7][SEG_AV1_LVL_ALT_LF_Y_H];
  sw_ctrl->sw_filt_level_delta2_seg7 = segval[7][SEG_AV1_LVL_ALT_LF_U];
  sw_ctrl->sw_filt_level_delta3_seg7 = segval[7][SEG_AV1_LVL_ALT_LF_V];
  sw_ctrl->sw_refpic_seg7 = segval[7][SEG_AV1_LVL_REF_FRAME];
  sw_ctrl->sw_skip_seg7 = segval[7][SEG_AV1_LVL_SKIP];
  sw_ctrl->sw_global_mv_seg7 = segval[7][SEG_AV1_LVL_GLOBALMV];
}

void Av1AsicSetLoopFilter(struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    struct SwRegisters *sw_ctrl) {
  /* loop filter */
  sw_ctrl->sw_filtering_dis =
      (dec->loop_filter_level == 0) && (dec->loop_filter_level_r == 0);

  sw_ctrl->sw_filt_sharpness = dec->loop_filter_sharpness;
  sw_ctrl->sw_filt_level_base_gt32 = dec->loop_filter_level >= 32;

  if (dec->mode_ref_lf_enabled) {
    sw_ctrl->sw_filt_ref_adj_0 = dec->mb_ref_lf_delta[0];
    sw_ctrl->sw_filt_ref_adj_1 = dec->mb_ref_lf_delta[1];
    sw_ctrl->sw_filt_ref_adj_2 = dec->mb_ref_lf_delta[2];
    sw_ctrl->sw_filt_ref_adj_3 = dec->mb_ref_lf_delta[3];
    sw_ctrl->sw_filt_ref_adj_4 = dec->mb_ref_lf_delta[4];
    sw_ctrl->sw_filt_ref_adj_5 = dec->mb_ref_lf_delta[5];
    sw_ctrl->sw_filt_ref_adj_6 = dec->mb_ref_lf_delta[6];
    sw_ctrl->sw_filt_ref_adj_7 = dec->mb_ref_lf_delta[7];
    sw_ctrl->sw_filt_mb_adj_0 = dec->mb_mode_lf_delta[0];
    sw_ctrl->sw_filt_mb_adj_1 = dec->mb_mode_lf_delta[1];
  } else {
    sw_ctrl->sw_filt_mb_adj_0 = 0;
    sw_ctrl->sw_filt_mb_adj_1 = 0;
    sw_ctrl->sw_filt_ref_adj_0 = 0;
    sw_ctrl->sw_filt_ref_adj_1 = 0;
    sw_ctrl->sw_filt_ref_adj_2 = 0;
    sw_ctrl->sw_filt_ref_adj_3 = 0;
    sw_ctrl->sw_filt_ref_adj_4 = 0;
    sw_ctrl->sw_filt_ref_adj_5 = 0;
    sw_ctrl->sw_filt_ref_adj_6 = 0;
    sw_ctrl->sw_filt_ref_adj_7 = 0;
  }
  REG_WRITE_ADDR(sw_ctrl->sw_dec_vert_filt_base,
                 asic_buff->filter_mem.bus_address);
  REG_WRITE_ADDR(sw_ctrl->sw_dec_bsd_ctrl_base,
                 asic_buff->filter_control.bus_address);
}

void Av1AsicSetCDEF(struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    struct SwRegisters *sw_ctrl) {
  /* CDEF */
  sw_ctrl->sw_cdef_damping = dec->cdef_damping;
  sw_ctrl->sw_cdef_bits = dec->cdef_bits;

  u32 luma_pri_strength = 0;
  u16 luma_sec_strength = 0;
  u32 chroma_pri_strength = 0;
  u16 chroma_sec_strength = 0;
  // TODO(min)? fix it later...
#if 0
  for (int i = 0; i < 8; i++) {
    if (i == (1 << (dec->cdef_bits))) break;
    luma_pri_strength.set_slc(i * 4,
      uai4(dec->cdef_luma_primary_strength[i]));
    luma_sec_strength.set_slc(i * 2,
      uai2(dec->cdef_luma_secondary_strength[i]));
    chroma_pri_strength.set_slc(i * 4,
        uai4(dec->cdef_chroma_primary_strength[i]));
    chroma_sec_strength.set_slc(i * 2,
        uai2(dec->cdef_chroma_secondary_strength[i]));
  }
#else
  for (int i = 0; i < 8; i++) {
    if (i == (1 << (dec->cdef_bits))) break;
    luma_pri_strength |= dec->cdef_luma_primary_strength[i] << (i * 4);
    luma_sec_strength |= dec->cdef_luma_secondary_strength[i] << (i * 2);
    chroma_pri_strength |= dec->cdef_chroma_primary_strength[i] << (i * 4);
    chroma_sec_strength |= dec->cdef_chroma_secondary_strength[i] << (i * 2);
  }
#endif

  sw_ctrl->sw_cdef_luma_primary_strength = luma_pri_strength;
  sw_ctrl->sw_cdef_luma_secondary_strength = luma_sec_strength;
  sw_ctrl->sw_cdef_chroma_primary_strength = chroma_pri_strength;
  sw_ctrl->sw_cdef_chroma_secondary_strength = chroma_sec_strength;

  // tile column buffer; repurpose some encoder specific base
  REG_WRITE_ADDR(sw_ctrl->sw_cdef_colbuf_base,
                 asic_buff->filter_mem.bus_address +
                 asic_buff->cdef_col_offset);
}

void Av1AsicSetLR(struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    struct SwRegisters *sw_ctrl) {
  u16 lr_type = 0;
  u16 lr_unit_size = 0;
  // TODO(min): fix it later...
#if 0
  for (int i = 0; i < 3; i++) {
    lr_type.set_slc(i * 2, uai2(dec->lr_type[i]));
    lr_unit_size.set_slc(i * 2, uai2(dec->lr_unit_size[i]));
  }
#else
  for (int i = 0; i < 3; i++) {
    lr_type |= dec->lr_type[i] << (i * 2);
    lr_unit_size |= dec->lr_unit_size[i] << (i * 2);
  }
#endif
  sw_ctrl->sw_lr_type = lr_type;
  sw_ctrl->sw_lr_unit_size = lr_unit_size;
  REG_WRITE_ADDR(sw_ctrl->sw_lr_colbuf_base,
                 asic_buff->filter_mem.bus_address +
                 asic_buff->lr_col_offset);
}

void init_scaling_function(u8 scaling_points[][2], u8 num_points,
                           u8 scaling_lut[]) {
  if (num_points == 0) {
    DWLmemset(scaling_lut, 0, 256);
    return;
  }

  for (int i = 0; i < scaling_points[0][0]; i++)
    scaling_lut[i] = scaling_points[0][1];

  for (int point = 0; point < num_points - 1; point++) {
    int delta_y = scaling_points[point + 1][1] - scaling_points[point][1];
    int delta_x = scaling_points[point + 1][0] - scaling_points[point][0];

    int64 delta =
        delta_x ? delta_y * ((65536 + (delta_x >> 1)) / delta_x) : 0;

    for (int x = 0; x < delta_x; x++) {
      scaling_lut[scaling_points[point][0] + x] =
          scaling_points[point][1] + (int)((x * delta + 32768) >> 16);
    }
  }

  for (int i = scaling_points[num_points - 1][0]; i < 256; i++)
    scaling_lut[i] = scaling_points[num_points - 1][1];
}

void Av1AsicSetFGS(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;
  u32 pp_enabled = dec_cont->pp_enabled;
  if (!dec->apply_grain) {
    sw_ctrl->sw_apply_grain = 0;
    // store reset params
    asic_buff->fg_params[asic_buff->out_buffer_i] = dec->fg_params;
    return;
  }
  struct Av1FilmGrainParams *fg_params = &dec->fg_params;
  if (!dec->update_parameters) {
    int active_ref = dec->film_grain_params_ref_idx;
    int index_ref = Av1BufferQueueGetRef(dec_cont->bq, active_ref);
    u16 random_seed = fg_params->random_seed;
    *fg_params = asic_buff->fg_params[index_ref];
    fg_params->random_seed = random_seed;
  }
  asic_buff->fg_params[asic_buff->out_buffer_i] = *fg_params;

  // film grain applied on secondary output
  sw_ctrl->sw_apply_grain = pp_enabled ? 1 : 0;
  sw_ctrl->sw_num_y_points_b = fg_params->num_y_points > 0;
  sw_ctrl->sw_num_cb_points_b = fg_params->num_cb_points > 0;
  sw_ctrl->sw_num_cr_points_b = fg_params->num_cr_points > 0;
  sw_ctrl->sw_scaling_shift = fg_params->scaling_shift + 8;
  if (!fg_params->chroma_scaling_from_luma) {
    sw_ctrl->sw_cb_mult = fg_params->cb_mult - 128;
    sw_ctrl->sw_cb_luma_mult = fg_params->cb_luma_mult - 128;
    sw_ctrl->sw_cb_offset = fg_params->cb_offset - 256;
    sw_ctrl->sw_cr_mult = fg_params->cr_mult - 128;
    sw_ctrl->sw_cr_luma_mult = fg_params->cr_luma_mult - 128;
    sw_ctrl->sw_cr_offset = fg_params->cr_offset - 256;
  } else {
    sw_ctrl->sw_cb_mult = 0;
    sw_ctrl->sw_cb_luma_mult = 64;
    sw_ctrl->sw_cb_offset = 0;
    sw_ctrl->sw_cr_mult = 0;
    sw_ctrl->sw_cr_luma_mult = 64;
    sw_ctrl->sw_cr_offset = 0;
  }
  sw_ctrl->sw_overlap_flag = fg_params->overlap_flag;
  sw_ctrl->sw_clip_to_restricted_range = fg_params->clip_to_restricted_range;
  sw_ctrl->sw_chroma_scaling_from_luma = fg_params->chroma_scaling_from_luma;
  sw_ctrl->sw_random_seed = fg_params->random_seed;

  init_scaling_function(fg_params->scaling_points_y, fg_params->num_y_points,
                        dec->fgsmem.scaling_lut_y);

  if (fg_params->chroma_scaling_from_luma) {
    memcpy(dec->fgsmem.scaling_lut_cb, dec->fgsmem.scaling_lut_y,
           sizeof(*dec->fgsmem.scaling_lut_y) * 256);
    memcpy(dec->fgsmem.scaling_lut_cr, dec->fgsmem.scaling_lut_y,
           sizeof(*dec->fgsmem.scaling_lut_y) * 256);
  } else {
    init_scaling_function(fg_params->scaling_points_cb,
                          fg_params->num_cb_points, dec->fgsmem.scaling_lut_cb);
    init_scaling_function(fg_params->scaling_points_cr,
                          fg_params->num_cr_points, dec->fgsmem.scaling_lut_cr);
  }

#ifdef SCALING_LUT_DEBUG_PRINT
  printf("scaling_lut_y\n");
  for (int i = 0; i < 256; i++) {
    printf("%d ", dec->fgsmem.scaling_lut_y[i]);
    if ((i + 1) % 10 == 0) {
      printf("\n");
    }
  }
  printf("\n\n");

  printf("scaling_lut_cb\n");
  for (int i = 0; i < 256; i++) {
    printf("%d ", dec->fgsmem.scaling_lut_cb[i]);
    if ((i + 1) % 10 == 0) {
      printf("\n");
    }
  }
  printf("\n\n");

  printf("scaling_lut_cr\n");
  for (int i = 0; i < 256; i++) {
    printf("%d ", dec->fgsmem.scaling_lut_cr[i]);
    if ((i + 1) % 10 == 0) {
      printf("\n");
    }
  }
  printf("\n\n");
#endif

  int ar_coeffs_y[24];
  int ar_coeffs_cb[25];
  int ar_coeffs_cr[25];
  int luma_grain_block[73][82];
  int cb_grain_block[38][44];
  int cr_grain_block[38][44];

  for (int i = 0; i < 25; i++) {
    if (i < 24) {
      ar_coeffs_y[i] = fg_params->ar_coeffs_y[i] - 128;
    }
    ar_coeffs_cb[i] = fg_params->ar_coeffs_cb[i] - 128;
    ar_coeffs_cr[i] = fg_params->ar_coeffs_cr[i] - 128;
  }

  int ar_coeff_lag = fg_params->ar_coeff_lag;
  int ar_coeff_shift = fg_params->ar_coeff_shift + 6;
  int grain_scale_shift = fg_params->grain_scale_shift;
  int bitdepth = dec->bit_depth;
  int grain_center = 128 << (bitdepth - 8);
  int grain_min = 0 - grain_center;
  int grain_max = (256 << (bitdepth - 8)) - 1 - grain_center;

  GenerateLumaGrainBlock(luma_grain_block, bitdepth, fg_params->num_y_points,
                         grain_scale_shift, ar_coeff_lag, ar_coeffs_y,
                         ar_coeff_shift, grain_min, grain_max,
                         fg_params->random_seed);

  GenerateChromaGrainBlock(
      luma_grain_block, cb_grain_block, cr_grain_block, bitdepth,
      fg_params->num_y_points, fg_params->num_cb_points,
      fg_params->num_cr_points, grain_scale_shift, ar_coeff_lag, ar_coeffs_cb,
      ar_coeffs_cr, ar_coeff_shift, grain_min, grain_max,
      fg_params->chroma_scaling_from_luma, fg_params->random_seed);

  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 64; j++) {
      dec->fgsmem.cropped_luma_grain_block[i * 64 + j] =
          luma_grain_block[i + 9][j + 9];
    }
  }

  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 32; j++) {
      dec->fgsmem.cropped_chroma_grain_block[i * 64 + 2 * j] =
          cb_grain_block[i + 6][j + 6];
      dec->fgsmem.cropped_chroma_grain_block[i * 64 + 2 * j + 1] =
          cr_grain_block[i + 6][j + 6];
    }
  }

  DWLmemcpy(asic_buff->film_grain_mem.virtual_address, &dec->fgsmem,
            sizeof(struct AV1FilmGrainMemory));
#if 0
  DWLMarkMemory(dwl, &asic_buff->film_grain_mem,
                DWL_HW_MARK_HOST_TO_DEVICE | DWL_HW_MARK_ONE_SHOT);
#endif
  REG_WRITE_ADDR(sw_ctrl->sw_filmgrain_base,
                 asic_buff->film_grain_mem.bus_address);

  if (sw_ctrl->sw_apply_grain) APITRACE("%s","NOTICE: filmgrain enabled.\n");
  DWLDMATransData(dec_cont->dwl, &asic_buff->film_grain_mem, 0,
                  asic_buff->film_grain_mem.size, HOST_TO_DEVICE);
}

void Av1SetSuperresParams(struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, struct SwRegisters *sw_ctrl) {
  // Compute and store scaling paramers needed for superres
  sw_ctrl->sw_superres_luma_step = dec->superres_luma_step;
  sw_ctrl->sw_superres_chroma_step = dec->superres_chroma_step;
  sw_ctrl->sw_superres_luma_step_invra =
      dec->superres_luma_step_invra;
  sw_ctrl->sw_superres_chroma_step_invra =
      dec->superres_chroma_step_invra;
  sw_ctrl->sw_superres_init_luma_subpel_x =
      dec->superres_init_luma_subpel_x;
  sw_ctrl->sw_superres_init_chroma_subpel_x =
      dec->superres_init_chroma_subpel_x;
  REG_WRITE_ADDR(sw_ctrl->sw_superres_colbuf_base,
                 asic_buff->filter_mem.bus_address +
                     asic_buff->sr_col_offset);
}

void Av1AsicSetPictureDimensions(struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, struct SwRegisters *sw_ctrl) {
  /* Write dimensions for the current picture
     (This is needed when scaling is used) */
  sw_ctrl->sw_pic_width_in_cbs = ROUNDUPX(dec->width, 8) >> 3;
  sw_ctrl->sw_pic_height_in_cbs = ROUNDUPX(dec->height, 8) >> 3;
  sw_ctrl->sw_pic_width_pad = ROUNDUPX(dec->width, 8) - dec->width;
  sw_ctrl->sw_pic_height_pad = ROUNDUPX(dec->height, 8) - dec->height;

  // struct Av1Decoder *dec = &dec_cont->decoder;
  sw_ctrl->sw_superres_pic_width = dec->superres_width;
  sw_ctrl->sw_superres_is_scaled = dec->superres_is_scaled;
  sw_ctrl->sw_scale_denom_minus9 = dec->scale_denom_minus9;

  Av1SetSuperresParams(dec, asic_buff, sw_ctrl);
}

void Av1AsicSetMulticoreParams(struct Av1DecContainer *dec_cont, size_t tile) {
  if (dec_cont->use_multicore) {
    struct Av1Decoder *dec = &dec_cont->decoder;
    struct SwRegisters *sw_ctrl = &dec_cont->multi_sw_ctrl[tile];

    sw_ctrl->sw_dec_multicore_e = dec_cont->use_multicore;
    sw_ctrl->sw_dec_writestat_e = 1;
    if (tile == 0) {
      sw_ctrl->sw_dec_multicore_mode = MULTICORE_LEFT_TILE;
    } else if (tile < (dec->av1_tile_cols - 1)) {
      sw_ctrl->sw_dec_multicore_mode = MULTICORE_INNER_TILE;
    } else {
      sw_ctrl->sw_dec_multicore_mode = MULTICORE_RIGHT_TILE;
    }
    sw_ctrl->sw_tile_mc_tile_col = tile;
    // Note: sbx_offset isn't needed in decoder mode, since the offsets are
    // propagated
    sw_ctrl->sw_tile_mc_tile_start_x = dec->tile_col_start_sb[tile];
    REG_WRITE_ADDR(
        sw_ctrl->sw_tile_mc_sync_curr_base,
        dec_cont->asic_buff->multicore_sync_buffers.bus_address + tile * 64);
    REG_WRITE_ADDR(sw_ctrl->sw_tile_mc_sync_left_base,
                   dec_cont->asic_buff->multicore_sync_buffers.bus_address +
                   (tile ? (tile - 1) : 0) * 64 );
    sw_ctrl->sw_dec_mc_polltime = dec_cont->multicore_poll_period;

    REG_WRITE_ADDR(sw_ctrl->sw_dec_vert_filt_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   tile * dec_cont->asic_buff->db_data_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_dec_bsd_ctrl_base,
                   dec_cont->asic_buff->filter_control.bus_address +
                   tile * dec_cont->asic_buff->db_ctrl_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_cdef_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->cdef_col_offset +
                   tile * dec_cont->asic_buff->cdef_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_superres_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->sr_col_offset +
                   tile * dec_cont->asic_buff->sr_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_lr_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->lr_col_offset +
                   tile * dec_cont->asic_buff->lr_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_rfc_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->rfc_col_offset +
                   tile * dec_cont->asic_buff->rfc_col_size);

    REG_WRITE_ADDR(sw_ctrl->sw_dec_left_vert_filt_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   (tile ? (tile - 1) : 0) * dec_cont->asic_buff->db_data_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_dec_left_bsd_ctrl_base,
                   dec_cont->asic_buff->filter_control.bus_address +
                   (tile ? (tile - 1) : 0) * dec_cont->asic_buff->db_ctrl_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_cdef_left_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->cdef_col_offset +
                   (tile ? (tile - 1) : 0) * dec_cont->asic_buff->cdef_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_superres_left_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->sr_col_offset +
                   (tile ? (tile - 1) : 0) * dec_cont->asic_buff->sr_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_lr_left_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->lr_col_offset +
                   (tile ? (tile - 1) : 0) * dec_cont->asic_buff->lr_col_tsize);

    REG_WRITE_ADDR(sw_ctrl->sw_rfc_left_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->rfc_col_offset +
                   (tile ? (tile - 1) : 0) * dec_cont->asic_buff->rfc_col_size);
    PPSetLancozsMutiCoreScaleRegs(dec_cont->av1_regs[tile], dec_cont->hw_feature,
                                  dec_cont->ppu_cfg, tile);
  }
}

void Av1AsicSetOutput(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;

  /* intrabc sync memory memset */
  DWLLinearMemset(dec_cont->dwl, &asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i],
                  asic_buff->dir_mvs_offset[asic_buff->out_buffer_i] - 64, 0, 64);

#ifdef ENABLE_FPGA_VERIFICATION
  /* only clear recon */
  if (!dec_cont->pp_enabled) {
    DWLLinearMemset(dec_cont->dwl, &asic_buff->pictures[asic_buff->out_buffer_i], 0, 0,
                    asic_buff->pictures[asic_buff->out_buffer_i].size);
  }
#endif
  sw_ctrl->sw_dec_alignment = ALIGN(dec_cont->align);
  REG_WRITE_ADDR(sw_ctrl->sw_dec_out_ybase,
                 asic_buff->pictures[asic_buff->out_buffer_i].bus_address);
  REG_WRITE_ADDR(sw_ctrl->sw_dec_out_cbase,
                 asic_buff->pictures[asic_buff->out_buffer_i].bus_address +
                     asic_buff->pictures_c_offset[asic_buff->out_buffer_i]);
  REG_WRITE_ADDR(sw_ctrl->sw_dec_out_dbase,
                 asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i].bus_address +
                     asic_buff->dir_mvs_offset[asic_buff->out_buffer_i]);
  if (dec_cont->use_video_compressor) {
    REG_WRITE_ADDR(sw_ctrl->sw_dec_out_tybase,
                   asic_buff->pictures[asic_buff->out_buffer_i].bus_address +
                       asic_buff->cbs_y_tbl_offset[asic_buff->out_buffer_i]);
    REG_WRITE_ADDR(sw_ctrl->sw_dec_out_tcbase,
                   asic_buff->pictures[asic_buff->out_buffer_i].bus_address +
                       asic_buff->cbs_c_tbl_offset[asic_buff->out_buffer_i]);
  }
  if (dec_cont->use_video_compressor) {
    sw_ctrl->sw_dec_out_ec_bypass = 0;
  } else {
    sw_ctrl->sw_dec_out_ec_bypass = 1;
  }
  if (dec_cont->use_video_compressor) {
#if 0
    /* If the size of CBS row >= 64KB, which means it's possible that the offset
       may overflow in EC table, set the EC output in word alignment. */
    if (RFC_MAY_OVERFLOW(ROUNDUPX(asic_buff->width, 8), dec_cont->decoder.bit_depth))
      base_swregs->ec_word_align = 1;
    else
      base_swregs->ec_word_align = 0;
#endif
    /* The word_align is default disabled.*/
    sw_ctrl->sw_ec_word_align = 1;
  }
  // save column tile info
  asic_buff->log2_tile_columns[asic_buff->out_buffer_i] =
      dec_cont->decoder.log2_tile_columns;
}

void Av1AsicInitPicture(struct Av1DecContainer *dec_cont,const struct Av1DecInput *input) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  u8 *dst = (u8 *)(asic_buff->global_model.virtual_address);

  dec_cont->use_multicore = dec_cont->use_multicore_backup;
  for (int ref_frame = 0; ref_frame < GM_GLOBAL_MODELS_PER_FRAME; ++ref_frame) {
    ASSERT(dec->models[ref_frame].wmtype <= AFFINE);

    /* In DDR wmmat order is 0, 1, 3, 2, 4, 5 */
    for (int i = 0; i < 6; ++i) {
      if (i == 2)
        *(i32 *)(dst) = dec->models[ref_frame].wmmat[3];
      else if (i == 3)
        *(i32 *)(dst) = dec->models[ref_frame].wmmat[2];
      else
        *(i32 *)(dst) = dec->models[ref_frame].wmmat[i];
      dst += 4;
    }
    *(i16 *)(dst) = dec->models[ref_frame].alpha;
    dst += 2;
    *(i16 *)(dst) = dec->models[ref_frame].beta;
    dst += 2;
    *(i16 *)(dst) = dec->models[ref_frame].gamma;
    dst += 2;
    *(i16 *)(dst) = dec->models[ref_frame].delta;
    dst += 2;
  }
  DWLDMATransData(dec_cont->dwl, &asic_buff->global_model, 0,
                  asic_buff->global_model.size, HOST_TO_DEVICE);

  struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
  Av1SetEmptyPictureData(dec_cont);
#endif

  Av1AsicSetOutput(dec_cont);

  sw_ctrl->sw_ref_frames = 0;
  sw_ctrl->sw_use_temporal0_mvs = 0;
  sw_ctrl->sw_use_temporal1_mvs = 0;
  sw_ctrl->sw_use_temporal2_mvs = 0;
  sw_ctrl->sw_use_temporal3_mvs = 0;
  if ((!dec->key_frame && !dec->intra_only) || dec->allow_intrabc) {
    Av1AsicSetReferenceFrames(dec_cont);
  } else {
    // DWLMarkClear(dec_cont->dwl, &asic_buff->dir_mvs[asic_buff->out_buffer_i],
    //             0);
  }

  Av1AsicSetSegmentation(dec, sw_ctrl);

  Av1AsicSetLoopFilter(dec, asic_buff, sw_ctrl);

  Av1AsicSetPictureDimensions(dec, asic_buff, sw_ctrl);

  Av1AsicSetCDEF(dec, asic_buff, sw_ctrl);

  Av1AsicSetLR(dec, asic_buff, sw_ctrl);

  Av1AsicSetFGS(dec_cont);

  sw_ctrl->sw_enable_cdef =  // dec->enable_cdef;
      !(dec->cdef_damping == 0 && dec->cdef_bits == 0 &&
        dec->cdef_luma_primary_strength[0] == 0 &&
        dec->cdef_luma_secondary_strength[0] == 0 &&
        dec->cdef_chroma_primary_strength[0] == 0 &&
        dec->cdef_chroma_secondary_strength[0] == 0);

  const u32 bypass_filter = !sw_ctrl->sw_superres_is_scaled &&
                            !sw_ctrl->sw_enable_cdef &&
                            !sw_ctrl->sw_filt_level0 &&
                            !sw_ctrl->sw_filt_level1 &&
                            !sw_ctrl->sw_lr_type;
  Av1AsicSetSecondaryOutput(dec_cont, bypass_filter);

  const int using_qm = dec_cont->decoder.using_qmatrix;

  const int qmlevel_y = (using_qm == 0) ? NUM_QM_LEVELS - 1 : dec->qm_y;
  const int qmlevel_u = (using_qm == 0) ? NUM_QM_LEVELS - 1 : dec->qm_u;
  const int qmlevel_v = (using_qm == 0) ? NUM_QM_LEVELS - 1 : dec->qm_v;

  sw_ctrl->sw_qmlevel_y = qmlevel_y;
  sw_ctrl->sw_qmlevel_u = qmlevel_u;
  sw_ctrl->sw_qmlevel_v = qmlevel_v;

#if DEBUG_AOM_QM
  fprintf(
      stderr,
      "setup_segmentation_dequant y %i u %i v %i dec->separate_uv_delta_q %u\n",
      qmlevel_y, qmlevel_u, qmlevel_v, dec->separate_uv_delta_q);
#endif

  REG_WRITE_ADDR(sw_ctrl->sw_global_model_base,
                 dec_cont->asic_buff->global_model.bus_address);
  REG_WRITE_ADDR(dec_cont->sw_ctrl.sw_global_model_base,
                 dec_cont->asic_buff->global_model.bus_address);
  sw_ctrl->sw_blackwhite_e = dec->monochrome;

  sw_ctrl->sw_bit_depth_y_minus8 = dec->bit_depth - 8;
  sw_ctrl->sw_bit_depth_c_minus8 = dec->bit_depth - 8;
  sw_ctrl->sw_quant_base_qindex = dec->qp_yac;

  /* In case of 16-bit content and big-endian architecture we need to prevent
   * hardware from swapping the bytes within 16-bit pixel values written out. */
  /*
  int x = 1;
  u8 big_endian = *(char*)&x != 1;
  if (big_endian && dec->bit_depth > 8) {
    SetDecRegister(av1_regs, HWIF_DEC_PIC_SWAP,
                   GetDecRegister(av1_regs, HWIF_DEC_PIC_SWAP) ^ 0x1);
    if (rs_out_bit_depth > 8) {
      SetDecRegister(av1_regs, HWIF_DEC_RSCAN_SWAP,
                     GetDecRegister(av1_regs, HWIF_DEC_RSCAN_SWAP) ^ 0x1);
    }
  }
  */

  /* QP deltas applied after choosing base QP based on segment ID. */
  sw_ctrl->sw_qp_delta_y_dc_av1 = dec->qp_ydc;
  sw_ctrl->sw_qp_delta_ch_dc_av1 = dec->qp_ch_dc;
  sw_ctrl->sw_qp_delta_ch_ac_av1 = dec->qp_ch_ac;
  sw_ctrl->sw_quant_delta_v_dc = dec->qp_cv_dc;
  sw_ctrl->sw_quant_delta_v_ac = dec->qp_cv_ac;
  sw_ctrl->sw_lossless_e = dec->lossless[0];

  /* Mark intra_only frame also a keyframe but copy inter probabilities to
     partition probs for the stream decoding. */
  sw_ctrl->sw_idr_pic_e = (dec->key_frame || dec->intra_only);

  sw_ctrl->sw_transform_mode = dec->transform_mode;
  sw_ctrl->sw_mcomp_filt_type = dec->mcomp_filter_type;
  sw_ctrl->sw_high_prec_mv_e = !dec->key_frame && dec->allow_high_precision_mv;
  sw_ctrl->sw_comp_pred_mode = dec->comp_pred_mode;

  sw_ctrl->sw_tempor_mvp_e = dec->use_ref_frame_mvs;
  sw_ctrl->sw_show_frame = dec->show_frame;

  sw_ctrl->sw_av1_comp_pred_fixed_ref = dec->comp_fixed_ref;
  sw_ctrl->sw_comp_pred_var_ref0_av1 = dec->comp_var_ref[0];
  sw_ctrl->sw_comp_pred_var_ref1_av1 = dec->comp_var_ref[1];

  sw_ctrl->sw_dec_tile_size_mag = dec->tile_sz_mag;

  sw_ctrl->sw_allow_screen_content_tools = dec->allow_screen_content_tools;
  sw_ctrl->sw_allow_intrabc = dec->allow_intrabc;
  sw_ctrl->sw_force_interger_mv = dec->frm_force_integer_mv;
  sw_ctrl->sw_reduced_tx_set_used = dec->reduced_tx_set_used;
  sw_ctrl->sw_delta_q_present = dec->av1_delta_q_present;
  sw_ctrl->sw_delta_q_res_log = dec->av1_delta_q_res_log;
  sw_ctrl->sw_allow_interintra = dec->enable_interintra_compound;
  sw_ctrl->sw_allow_masked_compound = dec->enable_masked_compound;
  sw_ctrl->sw_ref0_gm_mode = dec->models[0].wmtype;
  sw_ctrl->sw_ref1_gm_mode = dec->models[1].wmtype;
  sw_ctrl->sw_ref2_gm_mode = dec->models[2].wmtype;
  sw_ctrl->sw_ref3_gm_mode = dec->models[3].wmtype;
  sw_ctrl->sw_ref4_gm_mode = dec->models[4].wmtype;
  sw_ctrl->sw_ref5_gm_mode = dec->models[5].wmtype;
  sw_ctrl->sw_ref6_gm_mode = dec->models[6].wmtype;

  sw_ctrl->sw_enable_dual_filter = dec->enable_dual_filter;
  sw_ctrl->sw_enable_jnt_comp = dec->enable_jnt_comp;
  sw_ctrl->sw_enable_intra_edge_filter = dec->enable_intra_edge_filter;
  sw_ctrl->sw_allow_filter_intra = dec->enable_filter_intra;
  sw_ctrl->sw_delta_lf_present = dec->delta_lf_present_flag;
  sw_ctrl->sw_delta_lf_multi = dec->av1_delta_lf_multi;
  sw_ctrl->sw_delta_lf_res_log = dec->av1_delta_lf_res_log;
  sw_ctrl->sw_switchable_motion_mode = dec->switchable_motion_mode;

  sw_ctrl->sw_skip_mode = dec->skip_mode_flag;
  sw_ctrl->sw_skip_ref0 = dec->skip_ref0;
  if (sw_ctrl->sw_skip_ref0 == 0) sw_ctrl->sw_skip_ref0 = 1;
  sw_ctrl->sw_skip_ref1 = dec->skip_ref1;
  if (sw_ctrl->sw_skip_ref1 == 0) sw_ctrl->sw_skip_ref1 = 1;

  sw_ctrl->sw_disable_cdf_update = dec->disable_cdf_update;
  sw_ctrl->sw_preskip_segid = dec->preskip_segid;
  sw_ctrl->sw_last_active_seg = dec->last_active_seg;
  sw_ctrl->sw_max_cb_size = dec->sb_size ? 7 : 6;
  sw_ctrl->sw_min_cb_size = 3;
  sw_ctrl->sw_allow_warp = dec->allow_warped_motion;
  sw_ctrl->sw_write_mvs_e = 1;

  sw_ctrl->sw_error_conceal_e = dec_cont->hw_conceal;
  sw_ctrl->sw_unique_id = UNIQUE_ID(dec_cont->pic_number);

  Av1AsicSetTileInfoMem(dec_cont,input);
  for(int i = 0; i < DEC_MAX_PPU_COUNT; i++){
    if ((dec->av1_tile_cols > 1) &&
       (!PPCheckMutiCoreSupport(&dec_cont->ppu_cfg[i], bypass_filter, dec->sb_size ? 7 : 6, dec->av1_tile_cols, dec->tile_col_start_sb))) {
      dec_cont->use_multicore = 0;
    }
  }
#if 0
  DWLMarkMemory(dec_cont->dwl, &asic_buff->global_model,
                DWL_HW_MARK_HOST_TO_DEVICE | DWL_HW_MARK_ONE_SHOT);
#endif

  const size_t tile_cols = dec_cont->use_multicore ? dec->av1_tile_cols : 1;
  const size_t tile_count =
      dec_cont->use_multicore ? (dec->av1_tile_cols * dec->av1_tile_rows) : 1;
  if (tile_count > kMaxTiles) {
    // How to handle this error? Should we change the interface to
    // Av1AsicInitPicture()?
    ASSERT(!"Too many tiles");
    // exit(1);
  }

  if (tile_cols <= 1) {
    REG_WRITE_ADDR(
          sw_ctrl->sw_tile_mc_sync_curr_base,
          dec_cont->asic_buff->multicore_sync_buffers.bus_address);
    REG_WRITE_ADDR(
          sw_ctrl->sw_tile_mc_sync_left_base,
          dec_cont->asic_buff->multicore_sync_buffers.bus_address);
  }

  DWLmemset(dec_cont->asic_buff->multicore_sync_buffers.virtual_address, 0,
            dec_cont->asic_buff->multicore_sync_buffers.logical_size);

  DWLDMATransData(dec_cont->dwl, &asic_buff->multicore_sync_buffers, 0,
                  asic_buff->multicore_sync_buffers.logical_size, HOST_TO_DEVICE);
  if (dec_cont->enable_3dlut)
    DWLDMATransData(dec_cont->dwl, &dec_cont->ppu_cfg[0].table_3dlut_buffer, 0,
                    dec_cont->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);

  if (!dec_cont->use_multicore)
    REG_WRITE_ADDR(sw_ctrl->sw_rfc_colbuf_base,
                   dec_cont->asic_buff->filter_mem.bus_address +
                   dec_cont->asic_buff->rfc_col_offset);


  for (size_t t = 0; t < tile_cols; ++t) {
    memcpy(&dec_cont->multi_sw_ctrl[t], &dec_cont->sw_ctrl, //TODO optimize?
           sizeof(struct SwRegisters));

    memcpy(dec_cont->av1_regs[t], dec_cont->av1_regs[0], //TODO optimize?
           sizeof(dec_cont->av1_regs[0]));

    Av1AsicSetTileInfoRegs(dec, asic_buff, &dec_cont->multi_sw_ctrl[t]);
    if (tile_cols > 1) Av1AsicSetMulticoreParams(dec_cont, t);
  }
}

void Av1AsicStrmPosUpdate(struct Av1DecContainer *dec_cont,
                          addr_t strm_bus_address, u32 data_len,
                          addr_t buf_bus_address, u32 buf_len) {
  u32 tmp, hw_bit_pos;
  addr_t tmp_addr;
  u32 is_rb = ((strm_bus_address + data_len) > (buf_bus_address + buf_len)) ? 1 : 0;
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct SwRegisters *sw_ctrl = &dec_cont->sw_ctrl;

  dec_cont->sw_ctrl.sw_stream_len = data_len;

  APITRACE("%s","Av1AsicStrmPosUpdate:\n");

  /* Bit position where SW has decoded frame headers.
  tmp = (dec_cont->bc.pos) * 8 + (8 - dec_cont->bc.count);*/

  /* Residual partition after frame header partition. */
  tmp = dec->frame_tag_size + dec->offset_to_dct_parts;

  if (is_rb) {
    u32 turn_around = 0;
    tmp_addr = strm_bus_address + tmp;
    if (tmp_addr >= (buf_bus_address + buf_len)) {
      tmp_addr -= buf_len;
      turn_around = 1;
    }

    hw_bit_pos = (tmp_addr & (addr_t)DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (~((addr_t)DEC_HW_ALIGN_MASK)); /* align the base */

    sw_ctrl->sw_strm_start_bit = hw_bit_pos;
    REG_WRITE_ADDR(sw_ctrl->sw_stream_base, buf_bus_address);

    /* Total stream length passed to HW. */
    if (turn_around)
      tmp = data_len + strm_bus_address - (tmp_addr + buf_len);
    else
      tmp = data_len + strm_bus_address - tmp_addr;
    /* low latency: init parameter for low latency. */
    if(dec_cont->low_latency) {
      dec_cont->llstrminfo.ll_strm_bus_address = tmp_addr;
      dec_cont->llstrminfo.ll_strm_len = tmp;
      dec_cont->llstrminfo.first_update = 1;
      sw_ctrl->sw_stream_len = 0;
      if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
        *dec_cont->llstrminfo.strm_status_addr = 0;
      }
      dec_cont->llstrminfo.update_reg_flag = 1;
    } else
      sw_ctrl->sw_stream_len = tmp;

    /* stream data start offset */
    tmp_addr = tmp_addr - buf_bus_address;
    sw_ctrl->sw_strm_start_offset = tmp_addr;

    /* stream buffer size */
    sw_ctrl->sw_strm_buffer_len = buf_len;
  } else {
    tmp_addr = strm_bus_address + tmp;
    hw_bit_pos = (tmp_addr &(addr_t) DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (~((addr_t)DEC_HW_ALIGN_MASK)); /* align the base */

    sw_ctrl->sw_strm_start_bit = hw_bit_pos;
    REG_WRITE_ADDR(sw_ctrl->sw_stream_base, tmp_addr);

    /* Total stream length passed to HW. */
    tmp = data_len - (tmp_addr - strm_bus_address);
    /* low latency: init parameter for low latency. */
    if(dec_cont->low_latency) {
      dec_cont->llstrminfo.ll_strm_bus_address = tmp_addr;
      dec_cont->llstrminfo.ll_strm_len = tmp;
      dec_cont->llstrminfo.first_update = 1;
      sw_ctrl->sw_stream_len = 0;
      if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
        *dec_cont->llstrminfo.strm_status_addr = 0;
      }
      dec_cont->llstrminfo.update_reg_flag = 1;
    } else
      sw_ctrl->sw_stream_len = tmp;

    /* stream data start offset */
    sw_ctrl->sw_strm_start_offset = 0;

    /* stream buffer size */
    tmp = (u32)(tmp_addr - buf_bus_address);
    sw_ctrl->sw_strm_buffer_len = buf_len - tmp;
    //sw_ctrl->sw_strm_buffer_len = tmp;
  }
  if (dec_cont->low_latency && dec_cont->llstrminfo.strm_status_in_buffer)
    DWLDMATransData(dec_cont->dwl, &dec_cont->llstrminfo.strm_status, 0,
                    16 * 4, HOST_TO_DEVICE);

  APITRACE("STREAM BUS ADDR: 0x%08lx\n", strm_bus_address);
  APITRACE("HW STREAM BASE: 0x%08lx\n", tmp_addr);
  APITRACE("HW START BIT: %d\n", hw_bit_pos);
  APITRACE("HW STREAM LEN: %d\n", tmp);
}

static void DecAsicResetStatusRegs(struct SwRegisters *sw_ctrl) {
  // Reset IRQ and other flags
  sw_ctrl->sw_dec_e = 0;
  sw_ctrl->sw_dec_timeout = 0;
  sw_ctrl->sw_dec_bus_int = 0;
  sw_ctrl->sw_dec_rdy_int = 0;
  sw_ctrl->sw_dec_irq = 0;
  // sw_ctrl->sw_axi_read_data_overflow = 0;
  // sw_ctrl->sw_axi_write_data_underflow = 0;
  // sw_ctrl->sw_idct_overflow = 0;
  // sw_ctrl->sw_out_stream_overflow = 0;
  sw_ctrl->sw_dec_error_int = 0;
}

u32 Av1AsicRun(struct Av1DecContainer *dec_cont, u32 pic_id) {
  struct SwRegisters *sw_ctrl = dec_cont->multi_sw_ctrl;
  const u32 tile_cols = dec_cont->use_multicore ? dec_cont->decoder.av1_tile_cols : 1;
  int i;
  i32 ret = 0;
  i32 core_id = 0;
  u32 n_jobs = tile_cols;
  struct JobData1* dec_jobs;
  struct DWLReqInfo info = {0};
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;

  dec_jobs = (struct JobData1*)DWLmalloc(n_jobs * sizeof(struct JobData1));
  if (dec_jobs == NULL) return AV1HWDEC_SYSTEM_ERROR;
  dec_cont->job = dec_jobs;

  dec_cont->asic_running = 1;
#ifdef SUPPORT_VCMD_M2M
  if (dec_cont->vcmd_m2m)
    Av1ProbUpdateInfo(dec_cont);
#endif
  for (i = 0; i < n_jobs; i++, sw_ctrl++) {
    DecAsicResetStatusRegs(sw_ctrl);

    SetSwCtrl(dec_cont->av1_regs[i],  &dec_cont->multi_sw_ctrl[i]);

    /* Set the buffer bus address for using ddr_low_latency */
    if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
      SET_ADDR_REG(dec_cont->av1_regs[i], HWIF_LG_STREAM_STATUS_BASE,
                   dec_cont->llstrminfo.strm_status.bus_address);
    }

    info.core_mask = dec_cont->core_mask;
    info.width = dec_cont->width;
    info.height = dec_cont->height;
    info.owner = (void *)dec_cont;
    if (dec_cont->vcmd_used) {
      dec_cont->mc_buf_id = 0;
      if (dec_cont->use_multicore) {
        FifoObject obj;
        FifoPop(dec_cont->fifo_core, &obj, FIFO_EXCEPTION_DISABLE);
        dec_cont->mc_buf_id = (i32)(addr_t)obj;
      }
      ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_jobs[i].cmdbuf_id);
    } else {
      ret = DWLReserveHw(dec_cont->dwl, &info, &core_id);
    }
    if (ret != DWL_OK)
      return AV1HWDEC_HW_RESERVED;

#ifdef SUPPORT_VCMD_M2M
    if (dec_cont->vcmd_m2m){
      if(i == n_jobs - 1)
        Av1UpdateCDFsInfo(dec_cont);
      /* Transfer data mv info to dwl */
      DWLCmdM2MSendData(dec_cont->dwl, dec_cont->decoder.cdfs_info.cdfs_load_info,
              dec_cont->decoder.cdfs_info.cdfs_save_info, dec_jobs[i].cmdbuf_id);
    }
#endif
    dec_cont->core_id = dec_cont->vcmd_used ? dec_cont->mc_buf_id :
                        (dec_cont->use_multicore ? core_id : 0);
    /* Warning: only single core are currently supported (core_id = 0) */
    if (!dec_cont->use_multicore)
      PPSetLancozsScaleRegs(dec_cont->av1_regs[i], dec_cont->hw_feature, dec_cont->ppu_cfg, 0);
    if (dec_cont->vcmd_used)
      DWLReadPpConfigure(dec_cont->dwl, dec_jobs[i].cmdbuf_id, dec_cont->ppu_cfg, 0);
    else
      DWLReadPpConfigure(dec_cont->dwl, core_id, dec_cont->ppu_cfg, 0);

    if (dec_cont->vcmd_used) {
      if(dec_cont->mc_buf_id >= dec_cont->n_cores_available) {
        dec_cont->mc_buf_id = 0; /* in theory, shouldn't reach here */
      }
      DWLFlushRegister(dec_cont->dwl, dec_jobs[i].cmdbuf_id, dec_cont->av1_regs[i],
              dec_cont->mc_refresh_regs[dec_cont->mc_buf_id], dec_cont->mc_buf_id);
    } else {
      FlushDecRegisters(dec_cont->dwl, core_id, dec_cont->av1_regs[i],
                        dec_cont->hw_feature->max_ppu_count);
    }

    dec_jobs[i].core_id = core_id;
    dec_jobs[i].dwl = (void*)(dec_cont->dwl);

    dec_cont->tile_status[i] = TILE_TODO;

    if (dec_cont->use_multicore) {
      u32 id;
      if (dec_cont->vcmd_used) {
        id = dec_cont->mc_buf_id;
        DWLSetIRQCallback(dec_cont->dwl, dec_jobs[i].cmdbuf_id, Av1MCHwRdyCallback, dec_cont);
      } else {
        id = dec_jobs[i].core_id;
        DWLSetIRQCallback(dec_cont->dwl, dec_cont->core_id, Av1MCHwRdyCallback, dec_cont);
      }
      if (dec_cont->hw_rdy_callback_arg[id] == NULL) {
        dec_cont->hw_rdy_callback_arg[id] = DWLmalloc(sizeof(struct Av1HwRdyCallbackArg));
      }
      dec_cont->hw_rdy_callback_arg[id]->tile_index = i;
    }

    if (dec_cont->vcmd_used) {
      DWLEnableCmdBuf(dec_cont->dwl, dec_jobs[i].cmdbuf_id);
    } else {
      DWLEnableHw(dec_cont->dwl, core_id, 4 * 1, dec_cont->av1_regs[i][1]);
    }
#ifdef ASIC_TRACE_SUPPORT
    if (dec_cont->use_multicore)
      while (dec_cont->tile_status[i] == TILE_TODO) {
        sched_yield();
      };
#endif
  }
#ifdef SUPPORT_VCMD_M2M
  if (dec_cont->vcmd_m2m)
    DWLmemset(&dec_cont->decoder.cdfs_info, 0, sizeof(struct CDFsInfo));
#endif

  Av1SetupPicToOutput(dec_cont, pic_id);
  return DEC_OK;
}

void Av1AsicSyncMC(struct Av1DecContainer *dec_cont) {
  const u32 n_jobs = dec_cont->use_multicore ? dec_cont->decoder.av1_tile_cols : 1;
  int i;
  for(i = 0; i < n_jobs; i++) {
    while (dec_cont->tile_status[i] == TILE_TODO)
      sched_yield();

    dec_cont->tile_status[i] = TILE_TODO;
  }
}



/* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
static void Av1MCHwRdyCallback(void* arg, i32 core_id)
{
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)arg;
  struct Av1HwRdyCallbackArg *info = NULL;
  u32 combuf_id = 0, index, core_status = DEC_OK;
#ifdef PERFORMANCE_TEST
  u32 tile0_cycles = 0;
#endif

  if(dec_cont->vcmd_used) {
    combuf_id = core_id;
    core_id = DWLGetVcmdMCVirtualCoreId(dec_cont->dwl, combuf_id);
    info = dec_cont->hw_rdy_callback_arg[core_id];
    index = info->tile_index;
    DWLVcmdMCRefreshStatusRegs(dec_cont->dwl, dec_cont->av1_regs[index], combuf_id);
  }
  else {
    info = dec_cont->hw_rdy_callback_arg[core_id];
    index = info->tile_index;
    RefreshDecRegisters(dec_cont->dwl, core_id, dec_cont->av1_regs[index], dec_cont->hw_feature->max_ppu_count);
  }

  /* React to the HW return value */
  core_status = GetDecRegister(dec_cont->av1_regs[index], HWIF_DEC_IRQ_STAT);

  /* check if DEC_RDY, all other status are errors */
  if (core_status != DEC_HW_IRQ_RDY) {
    DWLmemset((void*)((u8 *)dec_cont->asic_buff->multicore_sync_buffers.virtual_address + index * 64), 0xFF, 64);
    DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->multicore_sync_buffers, index * 64,
                    64, HOST_TO_DEVICE);
  }

  if (dec_cont->vcmd_used) {
    DWLReleaseCmdBuf(dec_cont->dwl, combuf_id);
    if (dec_cont->use_multicore)
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)core_id, FIFO_EXCEPTION_DISABLE);
  }
  else
    DWLReleaseHw(dec_cont->dwl, core_id);

#ifdef PERFORMANCE_TEST
  DWLGetActiveTime(dec_cont->dwl, &dec_cont->tile_active_time, &dec_cont->active_time);
  if(index == 0){
    tile0_cycles = GetDecRegister(dec_cont->av1_regs[0], HWIF_PERF_CYCLE_COUNT);
    if(dec_cont->pic_number==1){
      dec_cont->cycle_time_ratio = tile0_cycles  / dec_cont->tile_active_time;
    }
  }
  if(index == dec_cont->decoder.av1_tile_cols - 1){
    dec_cont->cycle_sum = (dec_cont->active_time - dec_cont->last_active_time) * dec_cont->cycle_time_ratio;
    dec_cont->last_active_time = dec_cont->active_time;
  }
#else
  if(index == 0)
    dec_cont->cycle_sum = 0;
  dec_cont->cycle_sum += GetDecRegister(dec_cont->av1_regs[index], HWIF_PERF_CYCLE_COUNT);
#endif

  dec_cont->tile_status[index] = TILE_DONE;
}


u32 Av1AsicSync(struct Av1DecContainer *dec_cont) {
  enum DWLRet ret[MAX_TILE_COLS] = {0};

  const u32 n_jobs = dec_cont->use_multicore ? dec_cont->decoder.av1_tile_cols : 1;
  i32 i;
  u32 asic_status = 0;
  struct JobData1* dec_jobs = (struct JobData1*)dec_cont->job;
  //struct JobData1* load_job = (struct JobData1*)dec_cont->load_cdfs_job;
  //struct JobData1* save_job = (struct JobData1*)dec_cont->save_cdfs_job;

  if (dec_cont->use_multicore == 0) {
    for (i = 0; i < n_jobs; i++) {
      if (dec_cont->vcmd_used)
        ret[i] = DWLWaitCmdBufReady(dec_cont->dwl, dec_jobs[i].cmdbuf_id);
      else
        ret[i] = DWLWaitHwReady(dec_cont->dwl, dec_jobs[i].core_id, 0);

      dec_cont->tile_status[i] = TILE_DONE;

      if(dec_cont->vcmd_used)
        DWLRefreshRegister(dec_cont->dwl, dec_jobs[i].cmdbuf_id, dec_cont->av1_regs[i]);
      else
        RefreshDecRegisters(dec_cont->dwl, dec_jobs[i].core_id, dec_cont->av1_regs[i],
                            dec_cont->hw_feature->max_ppu_count);
      if (dec_cont->vcmd_used)
        DWLReleaseCmdBuf(dec_cont->dwl, dec_jobs[i].cmdbuf_id);
      else
        DWLReleaseHw(dec_cont->dwl, dec_jobs[i].core_id);
    }
  } else {
    Av1AsicSyncMC(dec_cont);
  }

  DWLfree(dec_jobs);

  for (size_t t = 0; t < n_jobs; ++t) {
    struct SwRegisters *sw_ctrl = &dec_cont->multi_sw_ctrl[t];
    sw_ctrl->sw_dec_error_int =
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_ERROR_INT);
    sw_ctrl->sw_dec_error_int |=
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_ABORT_INT);
    sw_ctrl->sw_dec_rdy_int =
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_RDY_INT);
    sw_ctrl->sw_dec_bus_int =
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_BUS_INT);
    sw_ctrl->sw_dec_timeout =
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_TIMEOUT);
    sw_ctrl->sw_dec_ext_timeout_int =
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_EXT_TIMEOUT_INT);
    sw_ctrl->sw_dec_error_code =
        GetDecRegister(dec_cont->av1_regs[t], HWIF_DEC_ERROR_CODE);
    sw_ctrl->sw_tile_left = GetDecRegister(dec_cont->av1_regs[t], HWIF_TILE_LEFT);
    if (sw_ctrl->sw_dec_error_int)
      APITRACEERR("error no = %d\n", sw_ctrl->sw_dec_error_code);
    if (ret[t] == DWL_HW_WAIT_TIMEOUT) {
      asic_status = AV1HWDEC_SYSTEM_TIMEOUT;
      break;
    }

    if ((sw_ctrl->sw_dec_error_int && sw_ctrl->sw_dec_error_code == 0) ||
        sw_ctrl->sw_tile_left) {
      asic_status = AV1HWDEC_STREAM_ERROR;
      break;
    }

    u32 core_asic_status = 0;

    if (sw_ctrl->sw_dec_rdy_int)
      core_asic_status = DEC_HW_IRQ_RDY;
    else if (sw_ctrl->sw_dec_timeout)
      core_asic_status = DEC_HW_IRQ_TIMEOUT;
    else if (sw_ctrl->sw_dec_ext_timeout_int)
      core_asic_status = DEC_HW_IRQ_EXT_TIMEOUT;
    else if (sw_ctrl->sw_dec_bus_int)
      core_asic_status = DEC_HW_IRQ_BUS;
    else if (sw_ctrl->sw_dec_error_int || sw_ctrl->sw_tile_left)
      core_asic_status = DEC_HW_IRQ_ERROR;

    asic_status |= core_asic_status;
  }

  dec_cont->asic_running = 0;

  /* low latency: update updated_reg that update thread know current frame dec done. */
  while(dec_cont->low_latency && (!dec_cont->llstrminfo.updated_reg)){
    sched_yield();
  }
  if(dec_cont->low_latency)
    dec_cont->llstrminfo.updated_reg = 0;

  return asic_status;
}

void Av1AsicProbUpdate(struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    struct SwRegisters *sw_ctrl,
    u32 vcmd_m2m) {
  u8 *asic_prob_base = (u8 *)asic_buff->prob_tbl.virtual_address;

#ifdef TRACE_EXT_TX_INTER_TABLE
  printf(
      "EXT_TX_TABLES probs: av1_inter_ext_tx_prob[0] = {%3d, %3d, %3d}\n"
      "EXT_TX_TABLES probs: av1_inter_ext_tx_prob[1] = {%3d, %3d, %3d}\n"
      "EXT_TX_TABLES probs: av1_inter_ext_tx_prob16  = {%3d, %3d, %3d}\n",
      dec->entropy.a.av1_inter_ext_tx_prob[0][0],
      dec->entropy.a.av1_inter_ext_tx_prob[0][1],
      dec->entropy.a.av1_inter_ext_tx_prob[0][2],
      dec->entropy.a.av1_inter_ext_tx_prob[1][0],
      dec->entropy.a.av1_inter_ext_tx_prob[1][1],
      dec->entropy.a.av1_inter_ext_tx_prob[1][2],
      dec->entropy.a.av1_inter_ext_tx_prob16[0],
      dec->entropy.a.av1_inter_ext_tx_prob16[1],
      dec->entropy.a.av1_inter_ext_tx_prob16[2]);
#endif  // TRACE_EXT_TX_INTER_TABLE

  /* Write probability tables to HW memory */
  if (!vcmd_m2m)
    Av1WriteCDFToMemory(asic_prob_base, dec);

  REG_WRITE_ADDR(sw_ctrl->sw_prob_tab_base,
                 asic_buff->prob_tbl.bus_address);
  REG_WRITE_ADDR(sw_ctrl->sw_prob_tab_out_base,
                 asic_buff->prob_tbl_out.bus_address);

  // We don't use counters in multisymbol
  // REG_WRITE_ADDR(base_swregs->sw_ctx_counter_base, 0L);
}

void Av1UpdateRefs(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  {
    if (dec_cont->decoder.reset_frame_flags) {
      Av1BufferQueueUpdateRef(dec_cont->bq, (1 << NUM_REF_FRAMES) - 1,
                              kReferenceNotSet);
      Av1BufferQueueUpdateRef(dec_cont->pp_bq, (1 << NUM_REF_FRAMES) - 1,
                              kReferenceNotSet);
      dec_cont->decoder.reset_frame_flags = 0;
    }
    Av1BufferQueueUpdateRef(dec_cont->bq, dec_cont->decoder.refresh_frame_flags,
                            asic_buff->out_buffer_i);
    Av1BufferQueueUpdateRef(dec_cont->pp_bq,
                            dec_cont->decoder.refresh_frame_flags,
                            asic_buff->out_pp_buffer_i);
  }
  if (!dec_cont->decoder.show_frame) {
    /* If the picture will not be outputted, we need to remove ref used to
        protect the output. */
    Av1BufferQueueRemoveRef(dec_cont->bq, asic_buff->out_buffer_i);
    Av1BufferQueueRemoveRef(dec_cont->pp_bq, asic_buff->out_pp_buffer_i);
  }
}

void Av1AsicReset(struct Av1DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  asic_buff->realloc_out_buffer = 0;
  asic_buff->out_buffer_i = AV1_UNDEFINED_BUFFER;
  // asic_buff->prev_out_buffer_i = AV1_UNDEFINED_BUFFER;
  asic_buff->out_pp_buffer_i = AV1_UNDEFINED_BUFFER;
  DWLmemset(asic_buff->first_show, 0, MAX_PIC_BUFFERS * sizeof(i32));
  DWLmemset(asic_buff->picture_info, 0, MAX_PIC_BUFFERS * sizeof(struct Av1DecPicture));
  if (dec_cont->pp_enabled) {
    asic_buff->out_pp_buffer_i = AV1_UNDEFINED_BUFFER;
    DWLmemset(asic_buff->pp_buffer_map, 0, MAX_PIC_BUFFERS * sizeof(i32));
#ifdef USE_OMXIL_BUFFER
    DWLmemset(asic_buff->pp_pictures, 0, MAX_PIC_BUFFERS * sizeof(struct DWLLinearMem));
#endif
  }
}

void Av1CalculateBufSize(
    struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    PpUnitIntConfig *ppu_cfg,
    u32 use_video_compressor,
    u32 align, u32 index) {
  u32 num_sbs, luma_size, chroma_size, dir_mvs_size;
  u64 pp_size = 0;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth;
  u32 out_w, i;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 ref_buffer_align = MAX(16, ALIGN(align));

  bit_depth = dec->bit_depth;

  Av1GetRefFrmSize(dec, asic_buff, use_video_compressor,
                   align, &luma_size, &chroma_size, &rfc_luma_size, &rfc_chroma_size);

  if (use_video_compressor)
    out_w = 4 * asic_buff->width * bit_depth / 8;
  else
    out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth,
                          ALIGN(align) * 8) /
            8;
  asic_buff->out_stride[index] = out_w;
  num_sbs = ((asic_buff->width + 63) / 64+1) * ((asic_buff->height + 63) / 64+1);
  dir_mvs_size =
      NEXT_MULTIPLE(num_sbs * 24*128/8,
                    ref_buffer_align); /* MVs (16 MBs / CTB * 12 bytes / MB) */
  if (ppu_cfg) {
    PpUnitIntConfig *tmp = ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, tmp++) {
      if (!tmp->enabled)
        continue;
      asic_buff->ds_stride[index][i] = tmp->ystride;
      asic_buff->ds_stride_ch[index][i] = tmp->cstride;
    }
    pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);
  }
  /* luma table size */
  luma_table_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  /* chroma table size */
  chroma_table_size = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align) +
                            NEXT_MULTIPLE(chroma_size, ref_buffer_align) +
                            luma_table_size + chroma_table_size;
  asic_buff->dpb_parasitic_buf_size = NEXT_MULTIPLE(64, ref_buffer_align) +
                                      dir_mvs_size;
  asic_buff->pp_size = pp_size;

  asic_buff->pictures_c_offset[index] = NEXT_MULTIPLE(luma_size, ref_buffer_align);
  /* align sync_mc buffer, storage sync bytes adjoining to dir mv*/
  asic_buff->dir_mvs_offset[index] = NEXT_MULTIPLE(64, ref_buffer_align);

  if (use_video_compressor) {
    asic_buff->cbs_y_tbl_offset[index] =
        asic_buff->pictures_c_offset[index] + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
    asic_buff->cbs_c_tbl_offset[index] =
        asic_buff->cbs_y_tbl_offset[index] + luma_table_size;
  } else {
    asic_buff->cbs_y_tbl_offset[index] = 0;
    asic_buff->cbs_c_tbl_offset[index] = 0;
  }
}

static void Av1GetRefFrmSize(
    struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    u32 use_video_compressor,
    u32 align,
    u32 *luma_size, u32 *chroma_size,
    u32 *rfc_luma_size, u32 *rfc_chroma_size) {
  u32 pic_width_in_cbsy, pic_height_in_cbsy;
  u32 pic_width_in_cbsc, pic_height_in_cbsc;
  u32 tbl_sizey, tbl_sizec;

  u32 bit_depth;
  u32 out_w, out_h;
  u32 ref_size;

  bit_depth = dec->bit_depth;

  out_w = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(asic_buff->width, 8) * bit_depth,
                        ALIGN(align) * 8) /
          8;
  out_h = asic_buff->height / 4;
  ref_size = out_w * out_h;

  if (luma_size) *luma_size = ref_size;
  if (chroma_size) *chroma_size = ref_size / 2;

  if (use_video_compressor) {
    pic_width_in_cbsy = (asic_buff->width + 8 - 1) / 8;
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = (asic_buff->width + 16 - 1) / 16;
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (asic_buff->height + 8 - 1) / 8;
    pic_height_in_cbsc = (asic_buff->height / 2 + 4 - 1) / 4;

    /* luma table size */
    tbl_sizey = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, 16);
    /* chroma table size */
    tbl_sizec = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, 16);
  } else {
    tbl_sizey = tbl_sizec = 0;
  }

  if (rfc_luma_size) *rfc_luma_size = tbl_sizey;
  if (rfc_chroma_size) *rfc_chroma_size = tbl_sizec;
}

static i32 Av1ReplaceRefPic(struct Av1DecContainer *dec_cont, i32 *index_info, u32 curr_index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  u32 i = 0;
  u32 index = 0;
  u32 tmp_pic_size = 0;
  u32 curr_pic_size = 0;

  curr_pic_size = asic_buff->picture_info[curr_index].coded_width *
                  asic_buff->picture_info[curr_index].coded_height;
  /* NO_RFC: if error_ratio less than 10%, don't replace it, use it as correct. */
  if (!dec_cont->use_video_compressor &&
      asic_buff->picture_info[curr_index].error_ratio <= EC_RATIO_THRESHOLD)
    return curr_index;

  /* firstly: find from correct ref */
  for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    index = index_info[i];
    /* skip itself. */
    if (index == curr_index) continue;

    tmp_pic_size = asic_buff->picture_info[index].coded_width *
                   asic_buff->picture_info[index].coded_height;
    if (asic_buff->picture_info[index].error_info == DEC_NO_ERROR)
      if (tmp_pic_size == curr_pic_size) return index;
  }
  /* secondly: find from ref_error ref */
  for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    index = index_info[i];
    /* skip itself. */
    if (index == curr_index) continue;

    tmp_pic_size = asic_buff->picture_info[index].coded_width *
                   asic_buff->picture_info[index].coded_height;
    if (asic_buff->picture_info[index].error_info == DEC_REF_ERROR)
      if (tmp_pic_size == curr_pic_size) return index;
  }
  /* thirdly: find from tolerable_error_count */
  for (i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    index = index_info[i];
    /* skip itself. */
    if (index == curr_index) continue;

    tmp_pic_size = asic_buff->picture_info[index].coded_width *
                   asic_buff->picture_info[index].coded_height;
    if (!dec_cont->use_video_compressor &&
        asic_buff->picture_info[index].error_ratio <= EC_RATIO_THRESHOLD)
      if (tmp_pic_size == curr_pic_size) return index;
  }

  /* not find */
  return EC_INVALID_IDX;
}

void Av1ReleaseParasiticBuf(const void *dwl, struct DWLLinearMem *info) {
  DWLFREE_LINEAR_MEM2(dwl, info);
  DWLmemset(info, 0, sizeof(*info));
  return;
}

 /* Allocate buffer for sync mc, dmv, rfct buffer */
i32 Av1AllocParasiticBuf(const void *dwl, u32 size, struct DWLLinearMem *info, u32 secure_mode) {

  if(size == 0)
    return DWL_OK;
  info->mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(info->mem_type, DWL_MEM_USAGE_TMP_DIRMV,
                secure_mode);
  if (DWLMALLOC_LINEAR_MEM2(dwl, size, info) !=0)
    return (DWL_ERROR);

  return DWL_OK;
}

i32 Av1ReleaseParasiticBufs(struct Av1DecContainer *dec_cont) {
  u32 i = 0;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    if (DWL_DEVMEM_VAILD(asic_buff->dpb_parasitic_buf[i]))
      Av1ReleaseParasiticBuf(dec_cont->dwl, &asic_buff->dpb_parasitic_buf[i]);
  }
  DWLmemset(asic_buff->dpb_parasitic_buf, 0, sizeof(asic_buff->dpb_parasitic_buf));
  return DWL_OK;
}
