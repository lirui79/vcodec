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

#include "basetype.h"
#include "dwl.h"
#include "regdrv.h"
#include "vp9hwd_asic.h"
#include "vp9hwd_container.h"
#include "vp9hwd_output.h"
#include "vp9hwd_probs.h"
#include "commonconfig.h"
#include "commonfunction.h"
#include "commonvp9.h"
#include "vp9_entropymv.h"
#include "vpufeature.h"
#include "ppu.h"
#include "delogo.h"
#include <string.h>
#include "dec_log.h"
#include "errorhandling.h"
#ifdef ASIC_TRACE_SUPPORT
#include "asic.h"
#endif

#define DEC_MODE_VP9 13
#define MAX3(A,B,C) ((A)>(B)&&(A)>(C)?(A):((B)>(C)?(B):(C)))

static const int kMaxTiles =
    128;  // same as av1, TODO

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
static void Vp9SetEmptyPictureData(struct Vp9DecContainer *dec_cont);
#endif
static i32 Vp9MallocRefFrm(struct Vp9DecContainer *dec_cont, u32 index);
static i32 Vp9AllocateSegmentMap(struct Vp9DecContainer *dec_cont);
static i32 Vp9FreeSegmentMap(struct Vp9DecContainer *dec_cont);
static i32 Vp9ReallocateSegmentMap(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetTileInfo(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetReferenceFrames(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetSegmentation(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetLoopFilter(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetPictureDimensions(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSetMulticoreParams(struct Vp9DecContainer *dec_cont,
                                      size_t tile);
static void Vp9AsicSetOutput(struct Vp9DecContainer *dec_cont);
static u32 RequiredBufferCount(struct Vp9DecContainer *dec_cont);
static void Vp9AsicSyncMC(struct Vp9DecContainer *dec_cont);
static void Vp9MCHwRdyCallback(void* arg, i32 core_id);
static i32 Vp9ReplaceRefPic(struct Vp9DecContainer *dec_cont, u32 *index_info,u32 curr_index);

u32 RequiredBufferCount(struct Vp9DecContainer *dec_cont) {
  return (Vp9BufferQueueCountReferencedBuffers(dec_cont->bq) + 2);
}

void Vp9AsicInit(struct Vp9DecContainer *dec_cont, const int multicore_poll_period) {
  dec_cont->vp9_regs[0][0] = DWLReadAsicID(dec_cont->dwl, DWL_CLIENT_TYPE_VP9_DEC);
  dec_cont->multicore_poll_period = multicore_poll_period;

  SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_MODE, DEC_MODE_VP9);

  SetCommonConfigRegs(dec_cont->vp9_regs[0]);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_OUT_EC_BYPASS, dec_cont->use_video_compressor ? 0 : 1);

}


i32 Vp9AsicAllocateMem(struct Vp9DecContainer *dec_cont) {

//  const void *dwl = dec_cont->dwl;
//  i32 dwl_ret;
  u32 size;
  i32 ret = 0;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

#if 0
  if (asic_buff->prob_tbl.virtual_address == NULL) {
    dec_cont->next_buf_size = sizeof(struct Vp9EntropyProbs);
    return DEC_WAITING_FOR_BUFFER;
  }

  if (asic_buff->ctx_counters.virtual_address == NULL) {
    dec_cont->next_buf_size = sizeof(struct Vp9EntropyCounts);
    return DEC_WAITING_FOR_BUFFER;
  }

  /* max number of tiles times width and height (2 bytes each),
   * rounding up to next 16 bytes boundary + one extra 16 byte
   * chunk (HW guys wanted to have this) */
  if (asic_buff->tile_info.virtual_address == NULL) {
    dec_cont->next_buf_size = (VP9_MAX_TILE_COLS * VP9_MAX_TILE_ROWS * 2 * sizeof(u16) + 15 + 16) & ~0xF;
    return DEC_WAITING_FOR_BUFFER;
  }
#else
  // vp9:
  //       row_sb:      4 bytes
  //       col_sb:      4 bytes
  //       start_pos:   4 bytes   (tile start byte offset from base)
  //       end_pos:     4 bytes   (tile end byte offset)
  //       x AV1_MAX_TILES (20*22) = 7040 bytes
  if (dec_cont->hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
    asic_buff->tile_info_offset = 0;
    size = NEXT_MULTIPLE((VP9_MAX_TILE_COLS * VP9_MAX_TILE_ROWS * 4 * sizeof(u32) + 15 + 16) & ~0xF, 16);
  } else {
    asic_buff->prob_tbl_offset = 0;
    asic_buff->tile_info_offset = asic_buff->prob_tbl_offset
                                  + NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 16);
    size = NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 16)
           + NEXT_MULTIPLE((VP9_MAX_TILE_COLS * VP9_MAX_TILE_ROWS * 4 * sizeof(u32) + 15 + 16) & ~0xF, 16);
  }

  if (asic_buff->misc_linear.virtual_address == NULL) {
    asic_buff->misc_linear.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                          DWL_MEM_TYPE_VPU_WORKING;
    SET_MEM_USAGE(asic_buff->misc_linear.mem_type, DWL_MEM_USAGE_TMP_MISC_LINEAR,
                      dec_cont->secure_mode);
    ret = DWLMallocLinear(dec_cont->dwl, size, &asic_buff->misc_linear);
    if (ret) return DEC_MEMFAIL;
  }

  if (asic_buff->ctx_counters.virtual_address == NULL) {
    asic_buff->ctx_counters.mem_type = DWL_MEM_TYPE_DMA_DEVICE_TO_HOST |
                                       DWL_MEM_TYPE_VPU_WORKING_SPECIAL;
    SET_MEM_USAGE(asic_buff->ctx_counters.mem_type, DWL_MEM_USAGE_OUT_CTXCOUNT,
                  dec_cont->secure_mode);
     /* +1 for accumulation all tiles ctx count */
    ret = DWLMallocLinear(dec_cont->dwl,
                          NEXT_MULTIPLE(sizeof(struct Vp9EntropyCounts), 16) *
                            (dec_cont->b_mc ? (VP9_MAX_TILE_COLS + 1) : 1),
                          &asic_buff->ctx_counters);
    if (ret) return DEC_MEMFAIL;
  }

#endif

  if (asic_buff->multicore_sync_buffers.virtual_address == NULL) {
    size = NEXT_MULTIPLE(64 * kMaxTiles, 4096);
    asic_buff->multicore_sync_buffers.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(asic_buff->multicore_sync_buffers.mem_type, DWL_MEM_USAGE_TMP_VP9MULTICORE_SYNC,
                  dec_cont->secure_mode);
    ret |= DWLMallocLinear(dec_cont->dwl, size, &asic_buff->multicore_sync_buffers);
    if (ret) return DEC_MEMFAIL;
  }

  /* low latency: Allocate buffer for ddr_low_latency model */
  if((dec_cont->llstrminfo.strm_status_in_buffer == 1) && (dec_cont->llstrminfo.strm_status.virtual_address == NULL)){
    dec_cont->llstrminfo.strm_status.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE | DWL_MEM_TYPE_VPU_WORKING;
    if(DWLMallocLinear(dec_cont->dwl, 16 * 4, &dec_cont->llstrminfo.strm_status))
      return 1;
   }

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
        asic_buff->fake_rfc_tbl.mem_type = DWL_MEM_TYPE_VPU_WORKING |
                                           DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#ifdef ASIC_TRACE_SUPPORT
        ret |= DWLMallocRefFrm(dec_cont->dwl, size, &asic_buff->fake_rfc_tbl);
#else
        ret |= DWLMallocLinear(dec_cont->dwl, size, &asic_buff->fake_rfc_tbl);
#endif
        if (ret) return DEC_MEMFAIL;
      }
      GenerateFakeRFCTable((u8 *)asic_buff->fake_rfc_tbl.virtual_address,
                            pic_width_in_cbsy, pic_height_in_cbsy,
                            pic_width_in_cbsc, pic_height_in_cbsc,
                            bit_depth, ref_buffer_align);
      DWLDMATransData(dec_cont->dwl, &asic_buff->fake_rfc_tbl, 0,
                      asic_buff->fake_rfc_tbl.size, HOST_TO_DEVICE);
    }
  }
#endif

  return DEC_OK;
}

i32 Vp9AsicReleaseMem(struct Vp9DecContainer *dec_cont) {

  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (asic_buff->misc_linear.virtual_address != NULL) {
    DWLFreeLinear(dec_cont->dwl, &asic_buff->misc_linear);
    asic_buff->misc_linear.virtual_address = NULL;
    asic_buff->misc_linear.bus_address = 0;
    asic_buff->misc_linear.size = 0;
  }

  if (asic_buff->ctx_counters.virtual_address != NULL) {
    DWLFreeLinear(dec_cont->dwl, &asic_buff->ctx_counters);
    asic_buff->ctx_counters.virtual_address = NULL;
    asic_buff->ctx_counters.bus_address = 0;
    asic_buff->ctx_counters.size = 0;
  }

  if (asic_buff->multicore_sync_buffers.virtual_address) {
    DWLFreeLinear(dec_cont->dwl, &asic_buff->multicore_sync_buffers);
    asic_buff->multicore_sync_buffers.virtual_address = NULL;
  }

  /* low latency: stream status in ddr buffer */
  if(dec_cont->llstrminfo.strm_status.virtual_address != NULL){
    DWLFreeLinear(dec_cont->dwl, &dec_cont->llstrminfo.strm_status);
    dec_cont->llstrminfo.strm_status.virtual_address = NULL;
  }

#ifdef USE_FAKE_RFC_TABLE
  if (asic_buff->fake_rfc_tbl.virtual_address != NULL) {
#ifdef ASIC_TRACE_SUPPORT
    DWLFreeRefFrm(dec_cont->dwl, &asic_buff->fake_rfc_tbl);
#else
    DWLFreeLinear(dec_cont->dwl, &asic_buff->fake_rfc_tbl);
#endif
    asic_buff->fake_rfc_tbl.virtual_address = NULL;
  }
#endif

  return DWL_OK;
}

/* Allocate filter memories that are dependent on tile structure and height.
 * tile_id: current tile id to decode; default value 0 for single core.*/
i32 Vp9AsicAllocateFilterBlockMem(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_tile_cols = 1 << dec_cont->decoder.log2_tile_columns;
  const u32 tile_cols = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;
  i32 dwl_ret;
  u32 i = 0, count = 0;
  u32 pp_reorder_offset = 0;
  u32 scale_offset = 0;
  u32 pp_scale_size = 0;
  u32 scale_out_offset = 0;
  u32 pp_scale_out_size = 0;
  if (num_tile_cols < 2) return HANTRO_OK;

  u32 height32 = NEXT_MULTIPLE(asic_buff->height, 64);
  /* filter_mem.*/
  u32 tile_edge_size = NEXT_MULTIPLE(height32 * ASIC_VERT_FILTER_RAM_SIZE_VP9, 128) * (num_tile_cols - 1);
  /* bsd_control_mem.*/
  u32 filter_control_size = NEXT_MULTIPLE(height32 * ASIC_BSD_CTRL_RAM_SIZE_VP9, 128) * (num_tile_cols - 1);
  /* rfc_mem.*/
  /* after tile8x8 optimize, buffer HWIF_RFC_COLBUF_BASE_LSB is always used no matter if RFC is enable or not */
  tile_edge_size += ASIC_RFC_RAM_SIZE * (height32 / 32) * (num_tile_cols - 1);

  if (dec_cont->pp_enabled) {
    pp_reorder_offset = tile_edge_size;
    /* only one tile pp tile_edge buffer need for sc mode. */
    num_tile_cols = (tile_cols > 1) ? num_tile_cols : 1;
    tile_edge_size += PPGetLancozsColumnBufferSize(dec_cont->ppu_cfg, asic_buff->height, dec_cont->decoder.bit_depth, num_tile_cols);
  }
  if (asic_buff->tile_edge.size < tile_edge_size) {
    /* If already allocated, release the old, too small buffers. */
    Vp9AsicReleaseFilterBlockMem(dec_cont);

    asic_buff->filter_control.mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY | DWL_MEM_TYPE_VPU_ONLY;
    SET_MEM_USAGE(asic_buff->filter_control.mem_type, DWL_MEM_USAGE_TMP_FILTER_CONTROL,
                  dec_cont->secure_mode);
    dwl_ret = DWLMallocLinear(dec_cont->dwl, filter_control_size, &asic_buff->filter_control);
    if (dwl_ret != DWL_OK) {
      Vp9AsicReleaseFilterBlockMem(dec_cont);
      return HANTRO_NOK;
    }

    asic_buff->tile_edge.mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY | DWL_MEM_TYPE_VPU_ONLY;
    SET_MEM_USAGE(asic_buff->tile_edge.mem_type, DWL_MEM_USAGE_TMP_PPUTILE_EDGE,
                  dec_cont->secure_mode);
    dwl_ret = DWLMallocLinear(dec_cont->dwl, tile_edge_size, &asic_buff->tile_edge);
    if (dwl_ret != DWL_OK) {
      Vp9AsicReleaseFilterBlockMem(dec_cont);
      return HANTRO_NOK;
    }
  }

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].enabled)
      count++;
  }
  pp_scale_size = 0;
  pp_scale_out_size = 0;
  scale_offset = pp_reorder_offset + dec_cont->ppu_cfg[0].reorder_size * num_tile_cols;
  scale_out_offset = scale_offset + count * dec_cont->ppu_cfg[0].scale_size * num_tile_cols;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (dec_cont->ppu_cfg[i].enabled) {
      dec_cont->ppu_cfg[i].reorder_buf_bus[0] = asic_buff->tile_edge.bus_address + pp_reorder_offset;
      dec_cont->ppu_cfg[i].scale_buf_bus[0] = asic_buff->tile_edge.bus_address + scale_offset + pp_scale_size;
      dec_cont->ppu_cfg[i].scale_out_buf_bus[0] = asic_buff->tile_edge.bus_address + scale_out_offset + pp_scale_out_size;
      pp_scale_size += dec_cont->ppu_cfg[i].scale_size * num_tile_cols;
      pp_scale_out_size += dec_cont->ppu_cfg[i].scale_out_size * num_tile_cols;
    }
  }

  return HANTRO_OK;
}

/* Set filter memories info for each tile that are allocated by Vp9AsicAllocateFilterBlockMem().
 * only set once for single core frame mode.
 * tile_id: current tile id to decode; default value 0 for single core.*/
i32 Vp9AsicSetFilterBlockMemIInfo(struct Vp9DecContainer *dec_cont, u32 tile_id) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_tile_cols = 1 << dec_cont->decoder.log2_tile_columns;
  u32 filter_mem_size = 0;

  u32 height32 = NEXT_MULTIPLE(asic_buff->height, 64);
  asic_buff->filter_mem_offset[tile_id] = tile_id * NEXT_MULTIPLE(height32 * ASIC_VERT_FILTER_RAM_SIZE_VP9, 128);
  asic_buff->filter_control_offset[tile_id] = tile_id * NEXT_MULTIPLE(height32 * ASIC_BSD_CTRL_RAM_SIZE_VP9, 128);
  /* filter_mem size.*/
  filter_mem_size = NEXT_MULTIPLE(height32 * ASIC_VERT_FILTER_RAM_SIZE_VP9, 128) * (num_tile_cols - 1);

  /* after tile8x8 optimize, buffer HWIF_RFC_COLBUF_BASE_LSB is always used no matter if RFC is enable or not */
  asic_buff->rfc_offset[tile_id] = filter_mem_size + tile_id * ASIC_RFC_RAM_SIZE * (height32 / 32);

  return HANTRO_OK;
}

i32 Vp9AsicReleaseFilterBlockMem(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (asic_buff->tile_edge.bus_address != 0) {
    DWLFreeLinear(dec_cont->dwl, &asic_buff->tile_edge);
    asic_buff->tile_edge.virtual_address = NULL;
    asic_buff->tile_edge.bus_address = 0;
    asic_buff->tile_edge.size = 0;
  }
  if (asic_buff->filter_control.bus_address != 0) {
      DWLFreeLinear(dec_cont->dwl, &asic_buff->filter_control);
      asic_buff->filter_control.virtual_address = NULL;
      asic_buff->filter_control.bus_address = 0;
      asic_buff->filter_control.size = 0;
  }

  return DWL_OK;
}

i32 Vp9AsicAllocatePictures(struct Vp9DecContainer *dec_cont) {
  u32 i;
  i32 ret;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  dec_cont->active_segment_map = 0;
  if (Vp9AllocateSegmentMap(dec_cont) != HANTRO_OK)
    return DEC_MEMFAIL;

  /* Require external picture buffers. */
  for (i = 0; i < dec_cont->num_buffers; i++) {
    ret = Vp9MallocRefFrm(dec_cont, i);
    if (ret == DEC_WAITING_FOR_BUFFER)
      return DEC_WAITING_FOR_BUFFER;
    else if (ret != HANTRO_OK)
      return DEC_MEMFAIL;
  }

  ASSERT(asic_buff->width / 4 < 0x1FFF);
  ASSERT(asic_buff->height / 4 < 0x1FFF);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_MAX_CB_SIZE, 6); /* 64x64 */
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_MIN_CB_SIZE, 3); /* 8x8 */

  asic_buff->out_buffer_i = -1;

  return HANTRO_OK;
}

void Vp9AsicReleasePictures(struct Vp9DecContainer *dec_cont) {
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  //for (i = 0; i < dec_cont->num_buffers; i++) {
  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
        DWL_DEVMEM_VAILD(asic_buff->pictures[i]))
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[i]);
  }

  Vp9ReleaseParasiticBufs(dec_cont);

  if (dec_cont->bq) {
    Vp9BufferQueueRelease(dec_cont->bq, !IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER));
    dec_cont->bq = NULL;
  }

  if (dec_cont->pp_bq) {
    Vp9BufferQueueRelease(dec_cont->pp_bq,
                         (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)));
    dec_cont->pp_bq = NULL;
  }

  DWLmemset(asic_buff->pictures, 0, sizeof(asic_buff->pictures));

  Vp9FreeSegmentMap(dec_cont);
}

i32 Vp9AllocateFrame(struct Vp9DecContainer *dec_cont, u32 index) {
  i32 ret = HANTRO_OK;

  if (Vp9MallocRefFrm(dec_cont, index)) ret = HANTRO_NOK;

  /* Following code will be excuted in Vp9DecAddBuffer(...) when the frame buffer is allocated externally. (bottom half) */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    dec_cont->num_buffers++;
    Vp9BufferQueueAddBuffer(dec_cont->bq);
  }

  return ret;
}

i32 Vp9ReallocateFrame(struct Vp9DecContainer *dec_cont, u32 index) {
  i32 ret = HANTRO_OK;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 out_index = 0;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    out_index = index;
  else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
    out_index = dec_cont->asic_buff->pp_buffer_map[index];

  pthread_mutex_lock(&dec_cont->sync_out);
  while (dec_cont->asic_buff->display_index[out_index])
    pthread_cond_wait(&dec_cont->sync_out_cv, &dec_cont->sync_out);

  /* Reallocate larger picture buffer into current index */
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
      asic_buff->pictures[asic_buff->out_buffer_i].logical_size < asic_buff->picture_size) {
    if (DWL_DEVMEM_VAILD(asic_buff->pictures[index]))
      DWLFreeRefFrm(dec_cont->dwl, &asic_buff->pictures[index]);
    asic_buff->pictures[index].mem_type = DWL_MEM_TYPE_DPB | DWL_MEM_TYPE_DMA_DEVICE_ONLY;
    SET_MEM_USAGE(asic_buff->pictures[index].mem_type, DWL_MEM_USAGE_OUT_REFERENCE,
                  dec_cont->secure_mode);
    ret = DWLMallocRefFrm(dec_cont->dwl, asic_buff->picture_size, &asic_buff->pictures[index]);
    if (ret != DWL_OK) {
       pthread_mutex_unlock(&dec_cont->sync_out);
       return DEC_MEMFAIL;
    }
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) ||
      IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    // All the external buffer free/malloc will be done in bottom half in AddBuffer()...
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      dec_cont->buf_to_free = &asic_buff->pictures[index];
      dec_cont->next_buf_size = asic_buff->picture_size;
      dec_cont->buf_type = REFERENCE_BUFFER;
      dec_cont->buffer_index = asic_buff->out_buffer_i;
      asic_buff->realloc_out_buffer = 1;
      dec_cont->buf_num = 1;
      ret = DEC_WAITING_FOR_BUFFER;
    } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) &&
             asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size < asic_buff->pp_size) {
      dec_cont->buf_to_free = &asic_buff->pp_pictures[asic_buff->pp_buffer_map[index]];
      dec_cont->next_buf_size = asic_buff->pp_size;
      dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
      dec_cont->buffer_index = asic_buff->out_pp_buffer_i;
      asic_buff->realloc_out_buffer = 1;
      dec_cont->buf_num = 1;
      ret = DEC_WAITING_FOR_BUFFER;
    }
  }

  pthread_mutex_unlock(&dec_cont->sync_out);

  return ret;
}

i32 Vp9MallocRefFrm(struct Vp9DecContainer *dec_cont, u32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_ctbs, luma_size, chroma_size, dir_mvs_size, delta_probs_size;
  u64 pp_size = 0;
  u32 pic_width_in_cbsy, pic_height_in_cbsy;
  u32 pic_width_in_cbsc, pic_height_in_cbsc;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth;
  i32 dwl_ret = DWL_OK;
  u32 out_w, out_w_luma, out_w_chroma, out_h, i;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  PpUnitIntConfig *ppu_cfg;

  bit_depth = dec_cont->decoder.bit_depth;

  /* No stride when compression is used. */
  if (dec_cont->use_video_compressor) {
    out_w_luma = out_w = 4 * asic_buff->width * bit_depth / 8;
    out_w_chroma = 4 * NEXT_MULTIPLE(asic_buff->width, 16) * bit_depth / 8;
  } else {
    out_w_luma = NEXT_MULTIPLE(TILE_HEIGHT * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_w_chroma = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(asic_buff->width, 16) * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  }
  out_h = asic_buff->height / 4;
  luma_size = out_w * out_h;
  chroma_size = (out_w_chroma * out_h) / 2;
  asic_buff->out_y_stride[index] = out_w_luma;
  asic_buff->out_c_stride[index] = out_w_chroma;
  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  dir_mvs_size = NEXT_MULTIPLE(num_ctbs * 64 * 16, ref_buffer_align); /* MVs (16 MBs / CTB * 16 bytes / MB) */
  delta_probs_size = NEXT_MULTIPLE(sizeof(struct Vp9DeltaProbs), ref_buffer_align);

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) && dec_cont->pp_enabled) {
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
  } else {
    luma_table_size = chroma_table_size = 0;
  }

  /* luma */
  asic_buff->pictures_c_offset[index] = NEXT_MULTIPLE(luma_size, ref_buffer_align);
  if (dec_cont->use_video_compressor) {
    asic_buff->cbs_y_tbl_offset[index] = asic_buff->pictures_c_offset[index]
                                         + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
    asic_buff->cbs_c_tbl_offset[index] = asic_buff->cbs_y_tbl_offset[index]
                                         + luma_table_size;
    asic_buff->delta_probs_offset[index] = asic_buff->cbs_c_tbl_offset[index]
                                         + chroma_table_size;
  } else {
    asic_buff->cbs_y_tbl_offset[index] = 0;
    asic_buff->cbs_c_tbl_offset[index] = 0;
    asic_buff->delta_probs_offset[index] = asic_buff->pictures_c_offset[index]
                                         + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
  }
  /* 32 bytes for MC sync mem */
  if (dec_cont->hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
    asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                              + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                              + luma_table_size + chroma_table_size
                              + delta_probs_size;
  } else {
    asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                              + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                              + luma_table_size + chroma_table_size;
  }
  asic_buff->dir_mvs_offset[index] = NEXT_MULTIPLE(32, ref_buffer_align);
  asic_buff->sync_mc_offset[index] = asic_buff->dir_mvs_offset[index] - 32;
  asic_buff->dpb_parasitic_buf_size = NEXT_MULTIPLE(32, ref_buffer_align)
                                      + dir_mvs_size;
  asic_buff->pp_size = pp_size;

  if (DWL_DEVMEM_COMPARE(asic_buff->pictures[index], DWL_DEVMEM_INIT)) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      dec_cont->next_buf_size = asic_buff->picture_size;
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
      dwl_ret |= DWLMallocRefFrm(dec_cont->dwl, asic_buff->picture_size, &asic_buff->pictures[index]);
      dwl_ret |= Vp9AllocParasiticBuf(dec_cont->dwl, asic_buff->dpb_parasitic_buf_size, &asic_buff->dpb_parasitic_buf[index],
                                      dec_cont->secure_mode);
    }
  }

  if (index < dec_cont->min_buffer_num) {
    if (DWL_DEVMEM_COMPARE(asic_buff->pp_pictures[index], DWL_DEVMEM_INIT) && dec_cont->pp_enabled) {
      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
        dec_cont->next_buf_size = asic_buff->pp_size;
        dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
        if (index == 0)
          dec_cont->buf_num = dec_cont->min_buffer_num;
        else dec_cont->buf_num = 1;
        return DEC_WAITING_FOR_BUFFER;
      }
    }
  }

  if (dwl_ret != DWL_OK) {
    // Vp9AsicReleasePictures(dec_cont);
    return HANTRO_NOK;
  }

  return HANTRO_OK;
}

#if 0
i32 Vp9FreeRefFrm(struct Vp9DecContainer *dec_cont, u32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (asic_buff->pictures[index].virtual_address != NULL) {
    /* Reallocate bigger picture buffer into current index */
    dec_cont->buffer_index = index;
    dec_cont->buf_to_free = &asic_buff->pictures[index];
    dec_cont->next_buf_size = 0;
    dec_cont->buf_num = 1;
    dec_cont->buf_type = REFERENCE_BUFFER;
    return DEC_WAITING_FOR_BUFFER;
  }
  return HANTRO_OK;
}
#endif

void Vp9SetExternalBufferInfo(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_ctbs, luma_size, chroma_size, dir_mvs_size;
  u32 pp_size = 0;
  u32 luma_table_size, chroma_table_size;
  u32 picture_size, dscale_size, buff_size;
  u32 min_buffer_num;
  enum DecBufferType buf_type;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;

  Vp9GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                     &rfc_luma_size, &rfc_chroma_size);

  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  dir_mvs_size = NEXT_MULTIPLE(num_ctbs * 64 * 16, ref_buffer_align); /* MVs (16 MBs / CTB * 16 bytes / MB) */
  if (dec_cont->pp_enabled)
    pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);

  /* luma table size */
  luma_table_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  /* chroma table size */
  chroma_table_size = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  // Add 32bytes for MCsync mem
  picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                 + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                 + luma_table_size + chroma_table_size;
  if (dec_cont->pp_enabled)
    dscale_size = pp_size;
  else
    dscale_size = 0;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    min_buffer_num = dec_cont->min_buffer_num;
    buff_size = picture_size;
    dec_cont->next_dpb_parasitic_buf_size = NEXT_MULTIPLE(32, ref_buffer_align)
                                            + dir_mvs_size;
    buf_type = REFERENCE_BUFFER;
  } else {
    min_buffer_num = dec_cont->min_buffer_num;
    ASSERT(IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER));
    buff_size = dscale_size;
    buf_type = DOWNSCALE_OUT_BUFFER;
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = buff_size;
  dec_cont->buf_type = buf_type;
}

void Vp9CalculateBufSize(struct Vp9DecContainer *dec_cont, i32 index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 num_ctbs, luma_size, chroma_size, dir_mvs_size, delta_probs_size;
  u64 pp_size = 0;
  u32 luma_table_size, chroma_table_size;
  u32 bit_depth;
  u32 out_w, out_w_luma, out_w_chroma, i;
  u32 rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));
  PpUnitIntConfig *ppu_cfg;

  bit_depth = dec_cont->decoder.bit_depth;

  Vp9GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                     &rfc_luma_size, &rfc_chroma_size);

  if (dec_cont->use_video_compressor) {
    out_w_luma = out_w = 4 * asic_buff->width * bit_depth / 8;
    out_w_chroma = 4 * NEXT_MULTIPLE(asic_buff->width, 16) * bit_depth / 8;
  } else {
    out_w_luma = NEXT_MULTIPLE(TILE_HEIGHT * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_w_chroma = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(asic_buff->width, 16) * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  }
  asic_buff->out_y_stride[index] = out_w_luma;
  asic_buff->out_c_stride[index] = out_w_chroma;
  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  dir_mvs_size = NEXT_MULTIPLE(num_ctbs * 64 * 16, ref_buffer_align); /* MVs (16 MBs / CTB * 16 bytes / MB) */
  delta_probs_size = NEXT_MULTIPLE(sizeof(struct Vp9DeltaProbs), ref_buffer_align);

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) && dec_cont->pp_enabled) {

    ppu_cfg = dec_cont->ppu_cfg;
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      asic_buff->ds_stride[index][i] = ppu_cfg->ystride;
      asic_buff->ds_stride_ch[index][i] = ppu_cfg->cstride;
    }
    pp_size = CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0);
  }

  /* luma table size */
  luma_table_size = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
  /* chroma table size */
  chroma_table_size = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

  if (dec_cont->hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
    asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                              + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                              + luma_table_size + chroma_table_size
                              + delta_probs_size;
  } else {
    asic_buff->picture_size = NEXT_MULTIPLE(luma_size, ref_buffer_align)
                              + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                              + luma_table_size + chroma_table_size;
  }
  asic_buff->pp_size = pp_size;

  asic_buff->pictures_c_offset[index] = NEXT_MULTIPLE(luma_size, ref_buffer_align);
  /* align sync_mc buffer, storage sync bytes adjoining to dir mv*/
  asic_buff->dir_mvs_offset[index] = NEXT_MULTIPLE(32, ref_buffer_align); //32 byts for MC sync mem
  asic_buff->sync_mc_offset[index] = asic_buff->dir_mvs_offset[index] - 32;

  if (dec_cont->use_video_compressor) {
    asic_buff->cbs_y_tbl_offset[index] = asic_buff->pictures_c_offset[index]
                                         + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
    asic_buff->cbs_c_tbl_offset[index] = asic_buff->cbs_y_tbl_offset[index]
                                         + luma_table_size;
    asic_buff->delta_probs_offset[index] = asic_buff->cbs_c_tbl_offset[index]
                                           + chroma_table_size;
  } else {
    asic_buff->cbs_y_tbl_offset[index] = 0;
    asic_buff->cbs_c_tbl_offset[index] = 0;
    asic_buff->delta_probs_offset[index] = asic_buff->pictures_c_offset[index]
                                           + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
  }
  asic_buff->dpb_parasitic_buf_size = NEXT_MULTIPLE(32, ref_buffer_align) +
                                      dir_mvs_size;
}

i32 Vp9GetRefFrm(struct Vp9DecContainer *dec_cont, const struct Vp9DecInput *input) {
  u32 limit = dec_cont->dynamic_buffer_limit;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  if (RequiredBufferCount(dec_cont) < limit)
    limit = RequiredBufferCount(dec_cont);

  if (asic_buff->realloc_tile_edge_mem) return HANTRO_OK;

  if (!asic_buff->realloc_out_buffer && !asic_buff->realloc_seg_map_buffer) {
    if (!dec_cont->no_decoding_buffer || asic_buff->out_buffer_i == EMPTY_MARKER) {
      asic_buff->out_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->bq, limit);
      if (asic_buff->out_buffer_i >= 0 && asic_buff->out_buffer_i < MAX_PIC_BUFFERS)
        asic_buff->first_show[asic_buff->out_buffer_i] = 0;
      if (asic_buff->out_buffer_i == ABORT_MARKER) {
        return DEC_ABORTED;
      }
#ifdef GET_FREE_BUFFER_NON_BLOCK
      else if (asic_buff->out_buffer_i == EMPTY_MARKER) {
        asic_buff->out_pp_buffer_i = EMPTY_MARKER;
        return DEC_NO_DECODING_BUFFER;
      }
#endif
      else if (asic_buff->out_buffer_i < 0) {
        if (Vp9AllocateFrame(dec_cont, dec_cont->num_buffers)) {
          /* Request for a new buffer. */
          asic_buff->realloc_out_buffer = 0;
          return DEC_WAITING_FOR_BUFFER;
        }
        asic_buff->out_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->bq, limit);
      }

      /* add "asic_buff->out_buffer_i >= MAX_PIC_BUFFERS" to */
      /* just fix coverity warning */
      if (asic_buff->out_buffer_i < 0 || asic_buff->out_buffer_i >= MAX_PIC_BUFFERS)
        return HANTRO_OK;

      /* Caculate the buffer size required for current picture. */
      Vp9CalculateBufSize(dec_cont, asic_buff->out_buffer_i);
    }

    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
      if (!dec_cont->no_decoding_buffer || asic_buff->out_pp_buffer_i == EMPTY_MARKER) {
        asic_buff->out_pp_buffer_i = Vp9BufferQueueGetBuffer(dec_cont->pp_bq, 0);
        if(asic_buff->out_pp_buffer_i == ABORT_MARKER) {
          return DEC_ABORTED;
        }
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
  }
  /* Reallocate larger dpb parasitic buffer into current index if needed */
  if ((asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i].logical_size < asic_buff->dpb_parasitic_buf_size)) {
      if (DWL_DEVMEM_VAILD(asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i]))
    Vp9ReleaseParasiticBuf(dec_cont->dwl, &asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i]);
    i32 dwl_ret = Vp9AllocParasiticBuf(dec_cont->dwl, asic_buff->dpb_parasitic_buf_size, &asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i],
                                      dec_cont->secure_mode);
    if (dwl_ret) return dwl_ret;
  }
  /* Reallocate picture memories if needed */
  if (asic_buff->pictures[asic_buff->out_buffer_i].logical_size < asic_buff->picture_size ||
      (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER) &&
       asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].logical_size < asic_buff->pp_size)
     ) {
    i32 dwl_ret = Vp9ReallocateFrame(dec_cont, asic_buff->out_buffer_i);
    if (dwl_ret) return dwl_ret;
  }

  asic_buff->realloc_out_buffer = 0;

#ifdef ENABLE_FPGA_VERIFICATION
  /* Please donot change memset logic for FPGA verification */
  if (dec_cont->pp_enabled) {
    /* update device buffer by host buffer */
    DWLLinearMemset(dec_cont->dwl, &asic_buff->pp_pictures[asic_buff->out_pp_buffer_i], 0, 0,
                    asic_buff->pp_pictures[asic_buff->out_pp_buffer_i].size);
  }
#endif

  if (Vp9ReallocateSegmentMap(dec_cont) != HANTRO_OK) {
    asic_buff->realloc_seg_map_buffer = 1;
    return DEC_WAITING_FOR_BUFFER;
  }
  asic_buff->realloc_seg_map_buffer = 0;
  return HANTRO_OK;
}

/* Reallocates segment maps if resolution changes bigger than initial
   resolution. Needs synchronization if SW is running parallel with HW */
i32 Vp9ReallocateSegmentMap(struct Vp9DecContainer *dec_cont) {
  i32 dwl_ret;
  u32 num_ctbs, memory_size;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  memory_size = num_ctbs * 32; /* Segment map uses 32 bytes / CTB */

  /* Do nothing if we have big enough buffers for segment maps,
     Actually we allocate largest segment maps at the beginning,
     so the reallocating should never be happen here  */
  if (memory_size <= asic_buff->segment_map_size) return HANTRO_OK;

  /* Allocate new segment maps for larger resolution */
  {
    /* Allocate new segment maps for larger resolution */
    struct DWLLinearMem new_segment_map = {0};
    i32 new_segment_size = memory_size;

    new_segment_map.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                               DWL_MEM_TYPE_VPU_ONLY;
    SET_MEM_USAGE(new_segment_map.mem_type, DWL_MEM_USAGE_IN_SEGMENT_MAP,
                  dec_cont->secure_mode);
    dwl_ret = DWLMallocLinear(dec_cont->dwl, memory_size * 2, &new_segment_map);

    if (dwl_ret != DWL_OK) {
      //Vp9AsicReleasePictures(dec_cont);
      return HANTRO_NOK;
    }
    /* Copy existing segment maps into new buffers */
    DWLmemcpy(new_segment_map.virtual_address,
              asic_buff->segment_map.virtual_address,
              asic_buff->segment_map_size);
    DWLmemcpy((u8 *)new_segment_map.virtual_address + new_segment_size,
              (u8 *)asic_buff->segment_map.virtual_address + asic_buff->segment_map_size,
              asic_buff->segment_map_size);
    /* Free old segment maps */
    Vp9FreeSegmentMap(dec_cont);

    asic_buff->segment_map_size = new_segment_size;
    asic_buff->segment_map = new_segment_map;
  }

  return HANTRO_OK;
}


i32 Vp9AllocateSegmentMap(struct Vp9DecContainer *dec_cont) {
  i32 dwl_ret;
  u32 num_ctbs, memory_size;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  /* Set segment map with max width/height */
  num_ctbs = ((dec_cont->hw_feature->vp9_max_dec_pic_width + 63) / 64) *
             ((dec_cont->hw_feature->vp9_max_dec_pic_height + 63) / 64);
  //num_ctbs = ((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64);
  memory_size = num_ctbs * 32; /* Segment map uses 32 bytes / CTB */

  /* Do nothing if we have big enough buffers for segment maps */
  if (memory_size <= asic_buff->segment_map_size) return HANTRO_OK;

  /* Free old segment maps */
  if (asic_buff->segment_map.bus_address)
    Vp9FreeSegmentMap(dec_cont);

  if (asic_buff->segment_map.bus_address == 0) {
    asic_buff->segment_map.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                          DWL_MEM_TYPE_VPU_ONLY;
    SET_MEM_USAGE(asic_buff->segment_map.mem_type, DWL_MEM_USAGE_IN_SEGMENT_MAP,
                      dec_cont->secure_mode);
    dwl_ret = DWLMallocLinear(dec_cont->dwl, memory_size * 2, &asic_buff->segment_map);
    if (dwl_ret) return DEC_MEMFAIL;
    asic_buff->segment_map_size = memory_size;
  }

  DWLDMATransData(dec_cont->dwl, &asic_buff->segment_map, 0, memory_size * 2, HOST_TO_DEVICE);

  return HANTRO_OK;
}

i32 Vp9FreeSegmentMap(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (asic_buff->segment_map.bus_address != 0) {
    DWLFreeLinear(dec_cont->dwl, &asic_buff->segment_map);
    asic_buff->segment_map.virtual_address = NULL;
    asic_buff->segment_map.bus_address = 0;
    asic_buff->segment_map.size = 0;
  }

  return HANTRO_OK;
}

void Vp9UpdateProbabilities(struct Vp9DecContainer *dec_cont) {
  u32 i = 0, j = 0, tmp = 0;
  const u32 tile_cols = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;
  u32 *p[VP9_MAX_TILE_COLS + 1] = { NULL };

  /* Backward adaptation of probs based on context counters. */
  if (dec_cont->decoder.error_resilient == 0 &&
      dec_cont->decoder.frame_parallel_decoding == 0) {
    /* transfer data from device buffer to host buffer */
    DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->ctx_counters, 0,
                    dec_cont->asic_buff->ctx_counters.size, DEVICE_TO_HOST);

    /* Read context counters from HW output memory. */
    if (!dec_cont->b_mc || tile_cols == 1) {
      DWLmemcpy(&dec_cont->decoder.ctx_ctr,
                dec_cont->asic_buff->ctx_counters.virtual_address,
                sizeof(struct Vp9EntropyCounts));
    } else {
      for (i = 0; i < VP9_MAX_TILE_COLS + 1; i++) {
        p[i] = (u32*)(((u8*)dec_cont->asic_buff->ctx_counters.virtual_address) +
               NEXT_MULTIPLE(sizeof(struct Vp9EntropyCounts), 16) * i);
      }
      for (i = 0; i < sizeof(struct Vp9EntropyCounts)/4; i++) {
        tmp = 0;
        for (j = 0; j < tile_cols; j++) {
          tmp += *p[j]++;
        }
        *p[VP9_MAX_TILE_COLS]++ = tmp;
      }
      DWLmemcpy(&dec_cont->decoder.ctx_ctr,
                ((u8*)dec_cont->asic_buff->ctx_counters.virtual_address) +
               NEXT_MULTIPLE(sizeof(struct Vp9EntropyCounts), 16) * VP9_MAX_TILE_COLS,
                sizeof(struct Vp9EntropyCounts));
    }

    Vp9AdaptCoefProbs(&dec_cont->decoder);
    if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only) {
      Vp9AdaptModeProbs(&dec_cont->decoder);
      Vp9AdaptModeContext(&dec_cont->decoder);
      Vp9AdaptNmvProbs(&dec_cont->decoder);
    }
  }

  /* Store the adapted probs as base for following frames. */
  Vp9StoreProbs(&dec_cont->decoder);
}


#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
void Vp9SetEmptyPictureData(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 index = asic_buff->out_buffer_i;

  DWLPrivateAreaMemset(asic_buff->pictures[index].virtual_address, SET_EMPTY_PICTURE_DATA,
                       asic_buff->pictures[index].size);
  DWLPrivateAreaMemset(asic_buff->pictures_c[index].virtual_address,
                       SET_EMPTY_PICTURE_DATA, asic_buff->pictures_c[index].size);
  DWLmemset(asic_buff->dir_mvs[index].virtual_address, SET_EMPTY_PICTURE_DATA,
            asic_buff->dir_mvs[index].size);

  if (!dec_cont->compress_bypass) {
    DWLmemset(asic_buff->cbs_luma_table[index].virtual_address,
              SET_EMPTY_PICTURE_DATA, asic_buff->cbs_luma_table[index].size);
    DWLmemset(asic_buff->cbs_chroma_table[index].virtual_address,
              SET_EMPTY_PICTURE_DATA, asic_buff->cbs_chroma_table[index].size);
  }

  if (dec_cont->pp_enabled) {
    DWLPrivateAreaMemset(asic_buff->raster_luma[index].virtual_address,
                         SET_EMPTY_PICTURE_DATA, asic_buff->raster_luma[index].size);
    DWLPrivateAreaMemset(asic_buff->raster_chroma[index].virtual_address,
                         SET_EMPTY_PICTURE_DATA, asic_buff->raster_chroma[index].size);
  }
}
#endif

void Vp9AsicSetTileInfo(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  u32 buildid = dec_cont->vp9_regs[0][0]&0x0000ffff;
  u8* p8 = NULL;
  u16* p16 = NULL;

  SET_ADDR_REG(dec_cont->tile_reg, HWIF_TILE_BASE,
               asic_buff->misc_linear.bus_address + asic_buff->tile_info_offset);
  SetDecRegister(dec_cont->tile_reg, HWIF_TILE_ENABLE,
                 dec->log2_tile_columns || dec->log2_tile_rows);
  SetDecRegister(dec_cont->tile_reg, HWIF_TILE_TRANSPOSE, dec->tile_transpose);

  if (dec_cont->low_latency) {
    if (dec->log2_tile_columns || dec->log2_tile_rows) {
      u32 tile_rows = (1 << dec->log2_tile_rows);
      u32 tile_cols = (1 << dec->log2_tile_columns);
      u32 h_sbs = (asic_buff->height + 63) / 64;
      u32 w_sbs = (asic_buff->width + 63) / 64;
      SetDecRegister(dec_cont->tile_reg, HWIF_NUM_TILE_COLS_8K, MIN(tile_cols,w_sbs));
      SetDecRegister(dec_cont->tile_reg, HWIF_NUM_TILE_ROWS_8K, MIN(tile_rows, h_sbs));
    } else {
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_NUM_TILE_COLS_8K, 1);
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_NUM_TILE_ROWS_8K, 1);
    }
    dec_cont->lltileinfo.tileinfo = ((u8 *)asic_buff->misc_linear.virtual_address + asic_buff->tile_info_offset);
    /* flag for update thread, means all variables are ready. */
    dec_cont->lltileinfo.update_rdy = 1;
  } else {
    if (dec->log2_tile_columns || dec->log2_tile_rows) {
      u32 tile_rows = (1 << dec->log2_tile_rows);
      u32 tile_cols = (1 << dec->log2_tile_columns);
      u32 i, j;
      if (buildid < 0x1030) {
        p16 = (u16 *)((u8 *)asic_buff->misc_linear.virtual_address + asic_buff->tile_info_offset);
      } else {
        p8 = ((u8 *)asic_buff->misc_linear.virtual_address + asic_buff->tile_info_offset);
      }
      u32 w_sbs = (asic_buff->width + 63) / 64;
      u32 h_sbs = (asic_buff->height + 63) / 64;
      u32 tile_id=0;
      const int transpose = dec->tile_transpose;

      /* write width + height for each tile in pic */
      if (tile_rows - h_sbs == 1) {
        if(transpose == 0)
        {
          for (i = 1; i < tile_rows; i++) {

            for (j = 0; j < tile_cols; j++) {
              tile_id = (i-1)*tile_cols + j;
              if (buildid < 0x1030) {
                *p16++ = dec->tile_col_width_sb[tile_id];
                *p16++ = dec->tile_row_height_sb[tile_id];
              } else {
                *p8++ = dec->tile_col_width_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = dec->tile_row_height_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                //for stream start/end
                u32 start;
                if (i == tile_rows - 1 && j == tile_cols - 1)
                  start = dec->tile_offset_start[tile_id];
                else
                  start = dec->tile_offset_start[tile_id] - 4;
                *p8++ = start & 255;
                *p8++ = (start >> 8) & 255;
                *p8++ = (start >> 16) & 255;
                *p8++ = (start >> 24) & 255;
                u32 end = dec->tile_offset_end[tile_id];
                *p8++ = end & 255;
                *p8++ = (end >> 8) & 255;
                *p8++ = (end >> 16) & 255;
                *p8++ = (end >> 24) & 255;
              }
            }
          }
        }
        else
        {
          for (j = 0; j < tile_cols; j++) {
            for (i = 1; i < tile_rows; i++) {
              tile_id = (i-1)*tile_cols + j;
              if (buildid < 0x1030) {
                *p16++ = dec->tile_col_width_sb[tile_id];
                *p16++ = dec->tile_row_height_sb[tile_id];
              } else {
                *p8++ = dec->tile_col_width_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = dec->tile_row_height_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                //for stream start/end
                u32 start;
                if (i == tile_rows - 1 && j == tile_cols - 1)
                  start = dec->tile_offset_start[tile_id];
                else
                  start = dec->tile_offset_start[tile_id] - 4;
                *p8++ = start & 255;
                *p8++ = (start >> 8) & 255;
                *p8++ = (start >> 16) & 255;
                *p8++ = (start >> 24) & 255;
                u32 end = dec->tile_offset_end[tile_id];
                *p8++ = end & 255;
                *p8++ = (end >> 8) & 255;
                *p8++ = (end >> 16) & 255;
                *p8++ = (end >> 24) & 255;
              }
            }
          }
        }
      } else if (tile_rows - h_sbs == 2) {
        /* When pic height (rounded up) is less than 3 SB rows, more than one
         * tile row may be skipped */
        if(transpose == 0)
        {
          for (i = 2; i < tile_rows; i++) {

            for (j = 0; j < tile_cols; j++) {
              tile_id = (i-2)*tile_cols + j;
              if (buildid < 0x1030) {
                *p16++ = dec->tile_col_width_sb[tile_id];
                *p16++ = dec->tile_row_height_sb[tile_id];
              } else {
                *p8++ = dec->tile_col_width_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = dec->tile_row_height_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                //for stream start/end
                u32 start;
                if (i == tile_rows - 1 && j == tile_cols - 1)
                  start = dec->tile_offset_start[tile_id];
                else
                  start = dec->tile_offset_start[tile_id] - 4;
                *p8++ = start & 255;
                *p8++ = (start >> 8) & 255;
                *p8++ = (start >> 16) & 255;
                *p8++ = (start >> 24) & 255;
                u32 end = dec->tile_offset_end[tile_id];
                *p8++ = end & 255;
                *p8++ = (end >> 8) & 255;
                *p8++ = (end >> 16) & 255;
                *p8++ = (end >> 24) & 255;
              }
            }
          }
        }
        else
        {
          for (j = 0; j < tile_cols; j++) {
            for (i = 2; i < tile_rows; i++) {

              tile_id = (i-2)*tile_cols + j;
              if (buildid < 0x1030) {
                *p16++ = dec->tile_col_width_sb[tile_id];
                *p16++ = dec->tile_row_height_sb[tile_id];
              } else {
                *p8++ = dec->tile_col_width_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = dec->tile_row_height_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                //for stream start/end
                u32 start;
                if (i == tile_rows - 1 && j == tile_cols - 1)
                  start = dec->tile_offset_start[tile_id];
                else
                  start = dec->tile_offset_start[tile_id] - 4;
                *p8++ = start & 255;
                *p8++ = (start >> 8) & 255;
                *p8++ = (start >> 16) & 255;
                *p8++ = (start >> 24) & 255;
                u32 end = dec->tile_offset_end[tile_id];
                *p8++ = end & 255;
                *p8++ = (end >> 8) & 255;
                *p8++ = (end >> 16) & 255;
                *p8++ = (end >> 24) & 255;
              }
            }
          }
        }
      } else {
        if(transpose == 0)
        {
          for (i = 0; i < tile_rows; i++) {

            for (j = 0; j < tile_cols; j++) {
              tile_id = i*tile_cols + j;
              if (buildid < 0x1030) {
                *p16++ = dec->tile_col_width_sb[tile_id];
                *p16++ = dec->tile_row_height_sb[tile_id];
              } else {
                *p8++ = dec->tile_col_width_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = dec->tile_row_height_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                //for stream start/end
                u32 start;
                if (i == tile_rows - 1 && j == tile_cols - 1)
                  start = dec->tile_offset_start[tile_id];
                else
                  start = dec->tile_offset_start[tile_id] - 4;
                *p8++ = start & 255;
                *p8++ = (start >> 8) & 255;
                *p8++ = (start >> 16) & 255;
                *p8++ = (start >> 24) & 255;
                u32 end = dec->tile_offset_end[tile_id];
                *p8++ = end & 255;
                *p8++ = (end >> 8) & 255;
                *p8++ = (end >> 16) & 255;
                *p8++ = (end >> 24) & 255;
              }
            }
          }
        }
        else
        {
          for (j = 0; j < tile_cols; j++) {
            for (i = 0; i < tile_rows; i++) {

              tile_id = i*tile_cols + j;
              if (buildid < 0x1030) {
                *p16++ = dec->tile_col_width_sb[tile_id];
                *p16++ = dec->tile_row_height_sb[tile_id];
              } else {
                *p8++ = dec->tile_col_width_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = dec->tile_row_height_sb[tile_id];
                *p8++ = 0;
                *p8++ = 0;
                *p8++ = 0;
                //for stream start/end
                u32 start;
                if (i == tile_rows - 1 && j == tile_cols - 1)
                  start = dec->tile_offset_start[tile_id];
                else
                  start = dec->tile_offset_start[tile_id] - 4;
                *p8++ = start & 255;
                *p8++ = (start >> 8) & 255;
                *p8++ = (start >> 16) & 255;
                *p8++ = (start >> 24) & 255;
                u32 end = dec->tile_offset_end[tile_id];
                *p8++ = end & 255;
                *p8++ = (end >> 8) & 255;
                *p8++ = (end >> 16) & 255;
                *p8++ = (end >> 24) & 255;
              }
            }
          }
        }
      }
      SetDecRegister(dec_cont->tile_reg, HWIF_NUM_TILE_COLS_8K, MIN(tile_cols,w_sbs));
      SetDecRegister(dec_cont->tile_reg, HWIF_NUM_TILE_ROWS_8K, MIN(tile_rows, h_sbs));

    } else {
      /* just one "tile", dimensions equal to pic size in SBs */
        if (buildid < 0x1030) {
          p16 = (u16 *)((u8 *)asic_buff->misc_linear.virtual_address + asic_buff->tile_info_offset);
        } else {
          p8 = ((u8 *)asic_buff->misc_linear.virtual_address + asic_buff->tile_info_offset);
        }
        if (buildid < 0x1030) {
          *p16++ = (asic_buff->width + 63) / 64;
          *p16++ = (asic_buff->height + 63) / 64;
        } else {
          *p8++ = (asic_buff->width + 63) / 64;
          *p8++ = 0;
          *p8++ = 0;
          *p8++ = 0;
          *p8++ =(asic_buff->height + 63) / 64;
          *p8++ = 0;
          *p8++ = 0;
          *p8++ = 0;
          //for stream start/end
          u32 start = dec->tile_offset_start[0];
          *p8++ = start & 255;
          *p8++ = (start >> 8) & 255;
          *p8++ = (start >> 16) & 255;
          *p8++ = (start >> 24) & 255;
          u32 end = dec->tile_offset_end[0];
          *p8++ = end & 255;
          *p8++ = (end >> 8) & 255;
          *p8++ = (end >> 16) & 255;
          *p8++ = (end >> 24) & 255;
        }

      SetDecRegister(dec_cont->vp9_regs[0], HWIF_NUM_TILE_COLS_8K, 1);
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_NUM_TILE_ROWS_8K, 1);
    }
  }
}

void Vp9AsicSetReferenceFrames(struct Vp9DecContainer *dec_cont) {
  u32 tmp1, tmp2, tmp3, tmp4, i;
  u32 cur_height, cur_width;
  u32 index;
  i32 replace_index = EC_INVALID_IDX;
  u8 replace_flags = 0;
  u8 fake_table_flags = 0;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 index_ref[VP9_ACTIVE_REFS];
  u32 index_info[VP9_ACTIVE_REFS] = {0};
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  //i32 ref_count[MAX_PIC_BUFFERS] = {0};
  u32 num_ref = 0;

  for (i = 0; i < VP9_ACTIVE_REFS; i++) {
    index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
    //if (index_ref[i] >= 0)
      //ref_count[index_ref[i]]++;
  }

  //for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    //if (ref_count[i])
      //num_ref++;
 // }
  num_ref = 3;
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_REF_FRAMES, num_ref);

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    for (i = 0; i < VP9_ACTIVE_REFS; i++) {
      index_info[i] = index_ref[i];
    }
  } else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    for (i = 0; i < VP9_ACTIVE_REFS; i++)
      index_info[i] = Vp9BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
  }

  /* unrounded picture dimensions */
  cur_width = dec_cont->decoder.width;
  cur_height = dec_cont->decoder.height;

  /* last reference */
  tmp1 = asic_buff->picture_info[index_info[0]].coded_width;
  tmp2 = asic_buff->picture_info[index_info[0]].coded_height;
  tmp3 = asic_buff->picture_info[index_info[0]].ref_pic_stride;
  tmp4 = asic_buff->picture_info[index_info[0]].ref_pic_ch_stride;
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_WIDTH, tmp1);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_HEIGHT, tmp2);
  if (!dec_cont->use_video_compressor) {
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_Y_STRIDE, tmp3 >> 3);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_C_STRIDE, tmp4 >> 3);
  } else {
    u32 ystride, cstride;
    if (hw_feature->rfc_support) {
      ystride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
    } else {
      ystride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    }
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_Y_STRIDE, ystride);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_C_STRIDE, cstride);
  }
  tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
  if(tmp1>(2<<VP9_REF_SCALE_SHIFT))
    tmp1 = (2<<VP9_REF_SCALE_SHIFT);
  tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
  if(tmp2>(2<<VP9_REF_SCALE_SHIFT))
    tmp2 = (2<<VP9_REF_SCALE_SHIFT);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_HOR_SCALE, tmp1);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_LREF_VER_SCALE, tmp2);
  /* EC: DEC_EC_REF_REPLACE or DEC_EC_REF_REPLACE_ANYWAY*/
  /* VP9: don't support fake table */
  index = 0;
  if (dec_cont->error_policy & DEC_EC_REF_REPLACE ||
      dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY) {
    if (asic_buff->picture_info[index_info[index]].error_info != DEC_NO_ERROR) {
      replace_index = Vp9ReplaceRefPic(dec_cont, index_info, index);
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
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_YBASE,
               asic_buff->pictures[index_ref[index]].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_CBASE,
               asic_buff->pictures[index_ref[index]].bus_address +
               asic_buff->pictures_c_offset[index_ref[index]]);
  if (!dec_cont->use_video_compressor) {
    ASSERT(asic_buff->cbs_y_tbl_offset[index_ref[index]] == 0);
    ASSERT(asic_buff->cbs_c_tbl_offset[index_ref[index]] == 0);
  } else {
#ifndef USE_FAKE_RFC_TABLE
    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_TYBASE,
                 asic_buff->pictures[index_ref[index]].bus_address +
                 asic_buff->cbs_y_tbl_offset[index_ref[index]]);
    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_TCBASE,
                 asic_buff->pictures[index_ref[index]].bus_address +
                 asic_buff->cbs_c_tbl_offset[index_ref[index]]);
#else
    if (dec_cont->decoder.key_frame || fake_table_flags == 1) {
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_TYBASE,
                   asic_buff->fake_rfc_tbl.bus_address);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_TCBASE,
                   asic_buff->fake_rfc_tbl.bus_address +
                   asic_buff->tbl_sizey);
#ifdef MEMSET_FAKE_REF_BUFFER
      if (fake_table_flags == 1) {
        /* try clean this ref buffer, not use the garbage */
        DWLLinearMemset(dec_cont->dwl, &asic_buff->pictures[index_ref[index]],
                        0, 0x80, asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      }
#endif
    } else {
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_TYBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_TCBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_c_tbl_offset[index_ref[index]]);
    }
#endif
  }
  /* Colocated MVs are always from previous decoded frame */
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER0_DBASE,
               asic_buff->dpb_parasitic_buf[asic_buff->prev_out_buffer_i].bus_address +
               asic_buff->dir_mvs_offset[asic_buff->prev_out_buffer_i]);
  /* refer0_dbase is used for reference status base address calc for last reference. */
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER1_DBASE,
               asic_buff->dpb_parasitic_buf[index_ref[index]].bus_address +
                 asic_buff->dir_mvs_offset[index_ref[index]]);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_LAST_SIGN_BIAS,
                 dec->ref_frame_sign_bias[LAST_FRAME]);

  /* golden reference */
  tmp1 = asic_buff->picture_info[index_info[1]].coded_width;
  tmp2 = asic_buff->picture_info[index_info[1]].coded_height;
  tmp3 = asic_buff->picture_info[index_info[1]].ref_pic_stride;
  tmp4 = asic_buff->picture_info[index_info[1]].ref_pic_ch_stride;
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_WIDTH, tmp1);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_HEIGHT, tmp2);
  if (!dec_cont->use_video_compressor) {
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_Y_STRIDE, tmp3 >> 3);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_C_STRIDE, tmp4 >> 3);
  } else {
    u32 ystride, cstride;
    if (hw_feature->rfc_support) {
      ystride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
    } else {
      ystride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    }
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_Y_STRIDE, ystride);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_C_STRIDE, cstride);
  }
  tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
  if(tmp1>(2<<VP9_REF_SCALE_SHIFT))
    tmp1 = (2<<VP9_REF_SCALE_SHIFT);
  tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
  if(tmp2>(2<<VP9_REF_SCALE_SHIFT))
    tmp2 = (2<<VP9_REF_SCALE_SHIFT);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_HOR_SCALE, tmp1);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_VER_SCALE, tmp2);
  /* EC: DEC_EC_REF_REPLACE or DEC_EC_REF_REPLACE_ANYWAY*/
  /* VP9: don't support fake table */
  index = 1;
  if (dec_cont->error_policy & DEC_EC_REF_REPLACE ||
      dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY) {
    if (asic_buff->picture_info[index_info[index]].error_info != DEC_NO_ERROR) {
      replace_index = Vp9ReplaceRefPic(dec_cont, index_info, index);
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
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_YBASE,
               asic_buff->pictures[index_ref[index]].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_CBASE,
               asic_buff->pictures[index_ref[index]].bus_address +
               asic_buff->pictures_c_offset[index_ref[index]]);
  if (!dec_cont->use_video_compressor) {
    ASSERT(asic_buff->cbs_y_tbl_offset[index_ref[index]] == 0);
    ASSERT(asic_buff->cbs_c_tbl_offset[index_ref[index]] == 0);
  } else {
#ifndef USE_FAKE_RFC_TABLE
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_TYBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_TCBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_c_tbl_offset[index_ref[index]]);
#else
    if (dec_cont->decoder.key_frame || fake_table_flags == 1) {
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_TYBASE,
                   asic_buff->fake_rfc_tbl.bus_address);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_TCBASE,
                   asic_buff->fake_rfc_tbl.bus_address +
                   asic_buff->tbl_sizey);
#ifdef MEMSET_FAKE_REF_BUFFER
      if (fake_table_flags == 1) {
        /* try clean this ref buffer, not use the garbage */
        DWLLinearMemset(dec_cont->dwl, &asic_buff->pictures[index_ref[index]],
                        0, 0x80, asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      }
#endif
    } else {
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_TYBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_TCBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_c_tbl_offset[index_ref[index]]);
    }
#endif
  }
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER4_DBASE,
               asic_buff->dpb_parasitic_buf[index_ref[index]].bus_address +
                 asic_buff->dir_mvs_offset[index_ref[index]]);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_GREF_SIGN_BIAS,
                 dec->ref_frame_sign_bias[GOLDEN_FRAME]);

  /* alternate reference */
  tmp1 = asic_buff->picture_info[index_info[2]].coded_width;
  tmp2 = asic_buff->picture_info[index_info[2]].coded_height;
  tmp3 = asic_buff->picture_info[index_info[2]].ref_pic_stride;
  tmp4 = asic_buff->picture_info[index_info[2]].ref_pic_ch_stride;
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_WIDTH, tmp1);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_HEIGHT, tmp2);
  if (!dec_cont->use_video_compressor) {
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_Y_STRIDE, tmp3 >> 3);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_C_STRIDE, tmp4 >> 3);
  } else {
    u32 ystride, cstride;
    if (hw_feature->rfc_support) {
      ystride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
      cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 6;
    } else {
      ystride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 8) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
      cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(tmp1, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    }
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_Y_STRIDE, ystride);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_C_STRIDE, cstride);
  }
  tmp1 = (tmp1 << VP9_REF_SCALE_SHIFT) / cur_width;
  if(tmp1>(2<<VP9_REF_SCALE_SHIFT))
    tmp1 = (2<<VP9_REF_SCALE_SHIFT);
  tmp2 = (tmp2 << VP9_REF_SCALE_SHIFT) / cur_height;
  if(tmp2>(2<<VP9_REF_SCALE_SHIFT))
    tmp2 = (2<<VP9_REF_SCALE_SHIFT);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_HOR_SCALE, tmp1);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_VER_SCALE, tmp2);
  /* EC: DEC_EC_REF_REPLACE or DEC_EC_REF_REPLACE_ANYWAY*/
  /* VP9: don't support fake table */
  index = 2;
  if (dec_cont->error_policy & DEC_EC_REF_REPLACE ||
      dec_cont->error_policy & DEC_EC_REF_REPLACE_ANYWAY) {
    if (asic_buff->picture_info[index_info[index]].error_info != DEC_NO_ERROR) {
      replace_index = Vp9ReplaceRefPic(dec_cont, index_info, index);
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
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_YBASE,
               asic_buff->pictures[index_ref[index]].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_CBASE,
               asic_buff->pictures[index_ref[index]].bus_address +
               asic_buff->pictures_c_offset[index_ref[index]]);

  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_DBASE,
               asic_buff->dpb_parasitic_buf[index_ref[index]].bus_address +
               asic_buff->dir_mvs_offset[index_ref[index]]);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_AREF_SIGN_BIAS,
                 dec->ref_frame_sign_bias[ALTREF_FRAME]);

  if (!dec_cont->use_video_compressor) {
    ASSERT(asic_buff->cbs_y_tbl_offset[index_ref[index]] == 0);
    ASSERT(asic_buff->cbs_c_tbl_offset[index_ref[index]] == 0);
  } else {
#ifndef USE_FAKE_RFC_TABLE
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_TYBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_TCBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_c_tbl_offset[index_ref[index]]);
#else
    if (dec_cont->decoder.key_frame || fake_table_flags == 1) {
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_TYBASE,
                   asic_buff->fake_rfc_tbl.bus_address);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_TCBASE,
                   asic_buff->fake_rfc_tbl.bus_address +
                   asic_buff->tbl_sizey);
#ifdef MEMSET_FAKE_REF_BUFFER
      if (fake_table_flags == 1) {
        /* try clean this ref buffer, not use the garbage */
        DWLLinearMemset(dec_cont->dwl, &asic_buff->pictures[index_ref[index]],
                        0, 0x80, asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      }
#endif
    } else {
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_TYBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_y_tbl_offset[index_ref[index]]);
      SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_REFER5_TCBASE,
                   asic_buff->pictures[index_ref[index]].bus_address +
                   asic_buff->cbs_c_tbl_offset[index_ref[index]]);
    }
#endif
  }
}

void Vp9AsicSetSegmentation(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  //struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 *vp9_regs = dec_cont->vp9_regs[0];
  u32 s;
  u32 segval[MAX_MB_SEGMENTS][SEG_LVL_MAX];

  /* Segmentation */
  SetDecRegister(vp9_regs, HWIF_SEGMENT_E, dec->segment_enabled);
  SetDecRegister(vp9_regs, HWIF_SEGMENT_UPD_E, dec->segment_map_update);
  SetDecRegister(vp9_regs, HWIF_SEGMENT_TEMP_UPD_E,
                 dec->segment_map_temporal_update);
  /* Set filter level and QP for every segment ID. Initialize all
   * segments with default QP and filter level. */
  for (s = 0; s < MAX_MB_SEGMENTS; s++) {
    segval[s][0] = dec->qp_yac;
    segval[s][1] = dec->loop_filter_level;
    segval[s][2] = 0; /* segment ref_frame disabled */
    segval[s][3] = 0; /* segment skip disabled */
  }
  /* If a feature is enabled for a segment, overwrite the default. */
  if (dec->segment_enabled) {
    i32(*segdata)[SEG_LVL_MAX] = dec->segment_feature_data;

    if (dec->segment_feature_mode == VP9_SEG_FEATURE_ABS) {
      for (s = 0; s < MAX_MB_SEGMENTS; s++) {
        if (dec->segment_feature_enable[s][0])
          segval[s][0] = CLIP3(0, 255, segdata[s][0]);
        if (dec->segment_feature_enable[s][1])
          segval[s][1] = CLIP3(0, 63, segdata[s][1]);
        if (!dec->key_frame && dec->segment_feature_enable[s][2])
          segval[s][2] = segdata[s][2] + 1;
        if (dec->segment_feature_enable[s][3]) segval[s][3] = 1;
      }
    } else { /* delta mode */
      for (s = 0; s < MAX_MB_SEGMENTS; s++) {
        if (dec->segment_feature_enable[s][0])
          segval[s][0] = CLIP3(0, 255, dec->qp_yac + segdata[s][0]);
        if (dec->segment_feature_enable[s][1])
          segval[s][1] = CLIP3(0, 63, dec->loop_filter_level + segdata[s][1]);
        if (!dec->key_frame && dec->segment_feature_enable[s][2])
          segval[s][2] = segdata[s][2] + 1;
        if (dec->segment_feature_enable[s][3]) segval[s][3] = 1;
      }
    }
  }
  /* Write QP, filter level, ref frame and skip for every segment */
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG0, segval[0][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG0, segval[0][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG0, segval[0][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG0, segval[0][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG1, segval[1][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG1, segval[1][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG1, segval[1][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG1, segval[1][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG2, segval[2][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG2, segval[2][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG2, segval[2][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG2, segval[2][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG3, segval[3][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG3, segval[3][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG3, segval[3][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG3, segval[3][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG4, segval[4][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG4, segval[4][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG4, segval[4][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG4, segval[4][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG5, segval[5][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG5, segval[5][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG5, segval[5][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG5, segval[5][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG6, segval[6][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG6, segval[6][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG6, segval[6][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG6, segval[6][3]);
  SetDecRegister(vp9_regs, HWIF_QUANT_SEG7, segval[7][0]);
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL_SEG7, segval[7][1]);
  SetDecRegister(vp9_regs, HWIF_REFPIC_SEG7, segval[7][2]);
  SetDecRegister(vp9_regs, HWIF_SKIP_SEG7, segval[7][3]);
}

void Vp9AsicSetLoopFilter(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 *vp9_regs = dec_cont->vp9_regs[0];
  //struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  /* loop filter */
  SetDecRegister(vp9_regs, HWIF_FILT_LEVEL, dec->loop_filter_level);
  SetDecRegister(vp9_regs, HWIF_FILTERING_DIS, dec->loop_filter_level == 0);
  SetDecRegister(vp9_regs, HWIF_FILT_SHARPNESS, dec->loop_filter_sharpness);

  if (dec->mode_ref_lf_enabled) {
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_0, dec->mb_ref_lf_delta[0]);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_1, dec->mb_ref_lf_delta[1]);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_2, dec->mb_ref_lf_delta[2]);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_3, dec->mb_ref_lf_delta[3]);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_0, dec->mb_mode_lf_delta[0]);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_1, dec->mb_mode_lf_delta[1]);
  } else {
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_0, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_1, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_2, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_REF_ADJ_3, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_0, 0);
    SetDecRegister(vp9_regs, HWIF_FILT_MB_ADJ_1, 0);
  }
  //SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_VERT_FILT_BASE,
  //             asic_buff->tile_edge.bus_address + asic_buff->filter_mem_offset);
  //SET_ADDR_REG(dec_cont->vp9_regs, HWIF_DEC_BSD_CTRL_BASE,
  //             asic_buff->tile_edge.bus_address + asic_buff->filter_control_offset);
}

void Vp9AsicSetPictureDimensions(struct Vp9DecContainer *dec_cont) {
  /* Write dimensions for the current picture
     (This is needed when scaling is used) */
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_PIC_WIDTH_IN_CBS,
                 (dec_cont->width + 7) / 8);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_PIC_HEIGHT_IN_CBS,
                 (dec_cont->height + 7) / 8);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_PIC_WIDTH_4X4,
                 NEXT_MULTIPLE(dec_cont->width, 8) >> 2);
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_PIC_HEIGHT_4X4,
                 NEXT_MULTIPLE(dec_cont->height, 8) >> 2);
}

void Vp9AsicSetMulticoreParams(struct Vp9DecContainer *dec_cont, size_t tile) {
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  const u32 tile_cols = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;

  if (dec_cont->b_mc) {
    struct Vp9Decoder *dec = &dec_cont->decoder;
    if (tile_cols <= 1) {
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_E, 0);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_MODE,
                     0);
    } else if (tile == 0) {
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_E, 1);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_MODE,
                     MULTICORE_LEFT_TILE);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_WRITESTAT_E, 1);
    } else if (tile < (dec->vp9_tile_cols - 1)) {
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_E, 1);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_MODE,
                     MULTICORE_INNER_TILE);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_WRITESTAT_E, 1);
    } else {
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_E, 1);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MULTICORE_MODE,
                     MULTICORE_RIGHT_TILE);
      SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_WRITESTAT_E, 1);
    }
    SetDecRegister(dec_cont->vp9_regs[tile], HWIF_TILE_MC_TILE_COL, tile);

    SetDecRegister(dec_cont->vp9_regs[tile], HWIF_TILE_MC_TILE_START_X,
                   dec->tile_col_start_sb[tile]);

    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_TILE_MC_SYNC_CURR_BASE,
                 dec_cont->asic_buff->multicore_sync_buffers.bus_address + tile * 64);
    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_TILE_MC_SYNC_LEFT_BASE,
                 dec_cont->asic_buff->multicore_sync_buffers.bus_address +
                 (tile ? (tile - 1) : 0) * 64);

    SetDecRegister(dec_cont->vp9_regs[tile], HWIF_DEC_MC_POLLTIME,
                   dec_cont->multicore_poll_period);

    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_DEC_VERT_FILT_BASE,
                 asic_buff->tile_edge.bus_address + asic_buff->filter_mem_offset[tile]);
    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_DEC_BSD_CTRL_BASE,
             asic_buff->filter_control.bus_address + asic_buff->filter_control_offset[tile]);
    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_RFC_COLBUF_BASE,
                 asic_buff->tile_edge.bus_address + asic_buff->rfc_offset[tile]);
    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_RFC_LEFT_COLBUF_BASE,
                 asic_buff->tile_edge.bus_address + (tile ? asic_buff->rfc_offset[tile - 1] : asic_buff->rfc_offset[tile]));
    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_DEC_LEFT_VERT_FILT_BASE,
                 asic_buff->tile_edge.bus_address + (tile ? asic_buff->filter_mem_offset[tile - 1] : asic_buff->filter_mem_offset[tile]));
    SET_ADDR_REG(dec_cont->vp9_regs[tile], HWIF_DEC_LEFT_BSD_CTRL_BASE,
                 asic_buff->filter_control.bus_address + (tile ? asic_buff->filter_control_offset[tile - 1] : asic_buff->filter_control_offset[tile]));

    PPSetLancozsMutiCoreScaleRegs(dec_cont->vp9_regs[tile], hw_feature,
                                  dec_cont->ppu_cfg, tile);
  }
}

void Vp9AsicSetOutput(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;

  SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_OUT_DIS, 0);

#ifdef ENABLE_FPGA_VERIFICATION
  /* only clear recon */
  if (!dec_cont->pp_enabled) {
    /* update device buffer by host buffer */
    DWLLinearMemset(dec_cont->dwl, &asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i],
                    asic_buff->dir_mvs_offset[asic_buff->out_buffer_i] - 32, 0, 32);
  }
#endif

  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_DEC_OUT_YBASE,
               asic_buff->pictures[asic_buff->out_buffer_i].bus_address);
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_DEC_OUT_CBASE,
               asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->pictures_c_offset[asic_buff->out_buffer_i]);

  u32 ystride, cstride;
  if (!dec_cont->use_video_compressor) {
    ystride = asic_buff->out_y_stride[asic_buff->out_buffer_i];
    cstride = asic_buff->out_c_stride[asic_buff->out_buffer_i];
  } else {
    if (hw_feature->rfc_support) {
      ystride = NEXT_MULTIPLE(8 * asic_buff->width * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 3;
    } else {
      ystride = NEXT_MULTIPLE(4 * asic_buff->width * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    }
    cstride = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(asic_buff->width, 16) * dec_cont->decoder.bit_depth, ALIGN(dec_cont->align) * 8) >> 3;
  }
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_OUT_Y_STRIDE, ystride / 8); //in unit of 8 bytes
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_OUT_C_STRIDE, cstride / 8); //in unit of 8 bytes

  if (dec_cont->use_video_compressor) {
    if (RFC_MAY_OVERFLOW(dec_cont->decoder.width, dec_cont->decoder.bit_depth))
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_OUT_EC_BYTE_WORD, 1);
    else
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_OUT_EC_BYTE_WORD, dec_cont->decoder.bit_depth > 8);
    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_DEC_OUT_TYBASE,
                 asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->cbs_y_tbl_offset[asic_buff->out_buffer_i]);
    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_DEC_OUT_TCBASE,
                 asic_buff->pictures[asic_buff->out_buffer_i].bus_address + asic_buff->cbs_c_tbl_offset[asic_buff->out_buffer_i]);
  }
  SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_DEC_OUT_DBASE,
               asic_buff->dpb_parasitic_buf[asic_buff->out_buffer_i].bus_address + asic_buff->dir_mvs_offset[asic_buff->out_buffer_i]);

  struct DWLLinearMem *pp_buffer = &asic_buff->pp_pictures[asic_buff->pp_buffer_map[asic_buff->out_buffer_i]];

  SetDecRegister(dec_cont->vp9_regs[0], HWIF_UNIQUE_ID, UNIQUE_ID(dec_cont->pic_number));

  /* Raster/PP output configuration. */
  if (dec_cont->pp_enabled) {
    u32 pp_out_ctrl = dec_cont->asic_buff->pp_out_ctrl[asic_buff->out_pp_buffer_i];
    struct PpParams pp_args = {hw_feature,
                               dec_cont->ppu_cfg,
                               pp_buffer,
                               0,
                               0,
                               pp_out_ctrl
                              };

    SetDecRegister(dec_cont->vp9_regs[0], HWIF_PP_IN_FORMAT_U, 1);
    PPSetRegs(dec_cont->vp9_regs[0], &pp_args);
    DelogoSetRegs(dec_cont->vp9_regs[0], hw_feature, dec_cont->delogo_params);
  }
  if (dec_cont->enable_3dlut)
    DWLDMATransData(dec_cont->dwl, &dec_cont->ppu_cfg[0].table_3dlut_buffer, 0,
                    dec_cont->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);
}

void Vp9AsicInitPicture(struct Vp9DecContainer *dec_cont) {
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 *vp9_regs = dec_cont->vp9_regs[0];
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;

#ifdef SET_EMPTY_PICTURE_DATA /* USE THIS ONLY FOR DEBUGGING PURPOSES */
  Vp9SetEmptyPictureData(dec_cont);
#endif

  /* make sure that output pic sync memory is cleared */
  DWLLinearMemset(dec_cont->dwl, &asic_buff->pictures[asic_buff->out_buffer_i], asic_buff->sync_mc_offset[asic_buff->out_buffer_i], 0, 32);

  Vp9AsicSetOutput(dec_cont);

  if (!dec->key_frame && !dec->intra_only) {
    Vp9AsicSetReferenceFrames(dec_cont);
  }

  //Vp9AsicSetTileInfo(dec_cont);

  Vp9AsicSetSegmentation(dec_cont);

  Vp9AsicSetLoopFilter(dec_cont);

  Vp9AsicSetPictureDimensions(dec_cont);

  SetDecRegister(vp9_regs, HWIF_BIT_DEPTH_Y_MINUS8, dec->bit_depth - 8);
  SetDecRegister(vp9_regs, HWIF_BIT_DEPTH_C_MINUS8, dec->bit_depth - 8);

  /* QP deltas applied after choosing base QP based on segment ID. */
  SetDecRegister(vp9_regs, HWIF_QP_DELTA_Y_DC, dec->qp_ydc);
  SetDecRegister(vp9_regs, HWIF_QP_DELTA_CH_DC, dec->qp_ch_dc);
  SetDecRegister(vp9_regs, HWIF_QP_DELTA_CH_AC, dec->qp_ch_ac);
  SetDecRegister(vp9_regs, HWIF_LOSSLESS_E, dec->lossless);

  /* Mark intra_only frame also a keyframe but copy inter probabilities to
     partition probs for the stream decoding. */
  SetDecRegister(vp9_regs, HWIF_IDR_PIC_E, (dec->key_frame || dec->intra_only));

  SetDecRegister(vp9_regs, HWIF_TRANSFORM_MODE, dec->transform_mode);
  SetDecRegister(vp9_regs, HWIF_MCOMP_FILT_TYPE, dec->mcomp_filter_type);
  SetDecRegister(vp9_regs, HWIF_HIGH_PREC_MV_E,
                 !dec->key_frame && dec->allow_high_precision_mv);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_MODE, dec->comp_pred_mode);
  SetDecRegister(vp9_regs, HWIF_TEMPOR_MVP_E,
                 !dec->error_resilient && !dec->key_frame &&
                 !dec->prev_is_key_frame && !dec->intra_only &&
                 !dec->resolution_change && dec->prev_show_frame);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_FIXED_REF, dec->comp_fixed_ref);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_VAR_REF0, dec->comp_var_ref[0]);
  SetDecRegister(vp9_regs, HWIF_COMP_PRED_VAR_REF1, dec->comp_var_ref[1]);

  if (!dec_cont->conceal) {
    if (dec->key_frame)
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_WRITE_MVS_E, 0);
    else
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_WRITE_MVS_E, 1);
  }
  SetDecRegister(dec_cont->vp9_regs[0], HWIF_DEC_TILE_INT_E, 0);

  const size_t tile_cols = dec_cont->b_mc ? dec->vp9_tile_cols : 1;
  if (tile_cols <= 1) {
    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_TILE_MC_SYNC_CURR_BASE,
          asic_buff->multicore_sync_buffers.bus_address);
    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_TILE_MC_SYNC_LEFT_BASE,
          asic_buff->multicore_sync_buffers.bus_address);
  }

  DWLmemset(asic_buff->multicore_sync_buffers.virtual_address, 0,
            asic_buff->multicore_sync_buffers.logical_size);
  DWLDMATransData(dec_cont->dwl, &asic_buff->multicore_sync_buffers, 0,
                  asic_buff->multicore_sync_buffers.logical_size, HOST_TO_DEVICE);

  for (size_t t = 0; t < tile_cols; ++t) {
    memcpy(dec_cont->vp9_regs[t], dec_cont->vp9_regs[0],  //TODO optimize?
           sizeof(dec_cont->vp9_regs[0]));
  }

  if (hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
    if (asic_buff->pictures[asic_buff->out_buffer_i].virtual_address != NULL) {
      DWLmemcpy((char *) (asic_buff->pictures[asic_buff->out_buffer_i].virtual_address) +
              asic_buff->delta_probs_offset[asic_buff->out_buffer_i], &dec->delta_probs,
              sizeof(struct Vp9DeltaProbs));
    } else {
      av_unused u32 size = sizeof(struct Vp9DeltaProbs);
      av_unused u8* host_vir_addr = (u8 *)&dec->delta_probs;
      DWLDMATransData2(dec_cont->dwl, asic_buff->pictures[asic_buff->out_buffer_i].bus_address +
                       asic_buff->delta_probs_offset[asic_buff->out_buffer_i], host_vir_addr,
                       size, HOST_TO_DEVICE);
    }
  }
}


void Vp9AsicStrmPosUpdate(struct Vp9DecContainer *dec_cont,
                          addr_t strm_bus_address, u32 data_len,
                          addr_t buf_bus_address, u32 buf_len) {
  u32 tmp, hw_bit_pos;
  addr_t tmp_addr;
  u32 is_rb = ((strm_bus_address + data_len) > (buf_bus_address + buf_len)) ? 1 : 0;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;

  APITRACEDEBUG("%s","Vp9AsicStrmPosUpdate:\n");
  /* Bit position where SW has decoded frame headers.
  tmp = (dec_cont->bc.pos) * 8 + (8 - dec_cont->bc.count);*/

  /* Residual partition after frame header partition. */
  tmp = dec->frame_tag_size + dec->offset_to_dct_parts;

  if(is_rb) {
    u32 turn_around = 0;
    tmp_addr = strm_bus_address + tmp;
    if(tmp_addr >= (buf_bus_address + buf_len)) {
      tmp_addr -= buf_len;
      turn_around = 1;
    }

    hw_bit_pos = (tmp_addr & DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (addr_t)(~DEC_HW_ALIGN_MASK); /* align the base */

    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STRM_START_BIT, hw_bit_pos);

    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_STREAM_BASE, buf_bus_address);

    /* Total stream length passed to HW. */
    if (turn_around)
      tmp = data_len + strm_bus_address - (tmp_addr + buf_len);
    else
      tmp = data_len + strm_bus_address - tmp_addr;
    /* low latency: init parameter for low latency */
    if(dec_cont->low_latency) {
      dec_cont->llstrminfo.ll_strm_bus_address = tmp_addr;
      dec_cont->llstrminfo.ll_strm_len = tmp;
      dec_cont->llstrminfo.first_update = 1;
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_LEN, 0);
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_LAST_BUFFER_E, 0);
      if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
        *dec_cont->llstrminfo.strm_status_addr = 0;
      }
      dec_cont->llstrminfo.update_reg_flag = 1;
    } else
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_LEN, tmp);

    /* stream data start offset */
    tmp_addr = tmp_addr - buf_bus_address;
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STRM_START_OFFSET, tmp_addr);

    /* stream buffer size */
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STRM_BUFFER_LEN, buf_len);
  } else {
    tmp_addr = strm_bus_address + tmp;

    hw_bit_pos = (tmp_addr & DEC_HW_ALIGN_MASK) * 8;
    tmp_addr &= (addr_t)(~DEC_HW_ALIGN_MASK); /* align the base */

    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STRM_START_BIT, hw_bit_pos);

    SET_ADDR_REG(dec_cont->vp9_regs[0], HWIF_STREAM_BASE, tmp_addr);

    /* Total stream length passed to HW. */
    tmp = data_len - (tmp_addr - strm_bus_address);

    /* low latency: init parameter for low latency */
    if(dec_cont->low_latency) {
      dec_cont->llstrminfo.ll_strm_bus_address = tmp_addr;
      dec_cont->llstrminfo.ll_strm_len = tmp;
      dec_cont->llstrminfo.first_update = 1;
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_LEN, 0);
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_LAST_BUFFER_E, 0);
      if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
        *dec_cont->llstrminfo.strm_status_addr = 0;
      }
      dec_cont->llstrminfo.update_reg_flag = 1;
    } else
      SetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_LEN, tmp);

    /* stream data start offset */
    tmp = (u32)(tmp_addr - buf_bus_address);
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STRM_START_OFFSET, 0);

    /* stream buffer size */
    SetDecRegister(dec_cont->vp9_regs[0], HWIF_STRM_BUFFER_LEN, buf_len - tmp);
  }
  if (dec_cont->low_latency && dec_cont->llstrminfo.strm_status_in_buffer) {
    DWLDMATransData(dec_cont->dwl, &dec_cont->llstrminfo.strm_status, 0,
                    16 * 4, HOST_TO_DEVICE);
  }

  APITRACEDEBUG("STREAM BUS ADDR: 0x%08x\n", strm_bus_address);
  APITRACEDEBUG("HW STREAM BASE: 0x%08x\n", tmp_addr);
  APITRACEDEBUG("HW START BIT: %d\n", hw_bit_pos);
  APITRACEDEBUG("HW STREAM LEN: %d\n", tmp);
}
static int tile_log2(int blk_size, int target) {
  int k;
  for (k = 0; (blk_size << k) < target; k++) {
  }
  return k;
}
void Vp9AsicStrmTileInfoCreate(struct Vp9DecContainer *dec_cont,
                          u8* strm_vir_address, u32 data_len,
                          u8* buf_vir_address, u32 buf_len) {
  u32 tmp;
  u8* tmp_addr;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  //u32 core_id = dec_cont->b_mc ? dec_cont->core_id : 0;
  u32 tile_id=0;
  u32 tile_size=0;
  int start_sb = 0;
  APITRACEDEBUG("%s","Vp9AsicStrmPosUpdate:\n");

  /* Bit position where SW has decoded frame headers.
  tmp = (dec_cont->bc.pos) * 8 + (8 - dec_cont->bc.count);*/

  /* Residual partition after frame header partition. */
  tmp = dec->frame_tag_size + dec->offset_to_dct_parts;

  tmp_addr = strm_vir_address + tmp;

  if (dec_cont->low_latency) {
    dec_cont->lltileinfo.tile_cols = dec->log2_tile_columns ? (1 << dec->log2_tile_columns) : 1;
    dec_cont->lltileinfo.tile_rows = dec->log2_tile_rows ? (1 << dec->log2_tile_rows) : 1;
    dec_cont->lltileinfo.cur_cols = 0;
    dec_cont->lltileinfo.cur_rows = 0;
    /* next tile unit start bus address */
    dec_cont->lltileinfo.strm_next_address = strm_vir_address + tmp;
    dec_cont->lltileinfo.tile_id = 0;
    dec_cont->lltileinfo.tile_size = 0;
    /* set update_rdy first */
    dec_cont->lltileinfo.update_rdy = 0;
    dec_cont->lltileinfo.update_done = 0;
    dec_cont->lltileinfo.w_sbs = (asic_buff->width + 63) / 64;
    dec_cont->lltileinfo.h_sbs = (asic_buff->height + 63) / 64;
    dec_cont->lltileinfo.prev_h = 0;
    dec_cont->lltileinfo.prev_w = 0;
    dec_cont->lltileinfo.frame_len = data_len;
    dec_cont->lltileinfo.buf_address = buf_vir_address;
    dec_cont->lltileinfo.buf_len = buf_len;
    if (dec_cont->lltileinfo.tile_rows - dec_cont->lltileinfo.h_sbs == 1)
      dec_cont->lltileinfo.cur_rows = 1;
    else if (dec_cont->lltileinfo.tile_rows - dec_cont->lltileinfo.h_sbs == 2)
      dec_cont->lltileinfo.cur_rows = 2;
  } else {
    if (dec->log2_tile_columns || dec->log2_tile_rows) {
      u32 tile_rows = (1 << dec->log2_tile_rows);
      u32 tile_cols = (1 << dec->log2_tile_columns);
      u32 i, j, h, tmp, prev_h, prev_w;
      u32 w_sbs = (asic_buff->width + 63) / 64;
      u32 h_sbs = (asic_buff->height + 63) / 64;
      {
        u32 actual_tile_rows=tile_rows;
        u32 actual_tile_cols=tile_cols;
        for (i = 0, prev_h = 0; i < tile_rows; i++) {
          tmp = (i + 1) * h_sbs / tile_rows;
          h = tmp - prev_h;
          prev_h = tmp;

          if (h_sbs >= 3 && !i && !h)
            dec_cont->first_tile_empty = 1;

          for (j = 0, prev_w = 0; j < tile_cols; j++) {
            tmp = (j + 1) * w_sbs / tile_cols;
#if 1
            dec->tile_col_width_sb[tile_id]=tmp - prev_w;
            dec->tile_row_height_sb[tile_id]=h;

            //for stream start/end
            if((i==(tile_rows-1))&&(j==(tile_cols-1)))
            {
              dec->tile_offset_start[tile_id]=tile_size;
              dec->tile_offset_end[tile_id]=data_len - dec->frame_tag_size - dec->offset_to_dct_parts;
              if(data_len > buf_len)
                dec->tile_offset_end[tile_id]= dec->tile_offset_start[tile_id]+buf_len-tile_size;
              if(dec->tile_offset_end[tile_id]<dec->tile_offset_start[tile_id])
              {
                dec->tile_offset_end[tile_id]=dec->tile_offset_start[tile_id];
              }
            }
            else
            {
              dec->tile_offset_start[tile_id]=tile_size+4;
              if(dec->tile_offset_start[tile_id]>buf_len)
                dec->tile_offset_start[tile_id]=buf_len;

              if (tmp_addr >= (buf_vir_address + buf_len))
                tmp_addr -= buf_len;
              tile_size+=*tmp_addr<<24;
              tmp_addr++;
              if (tmp_addr >= (buf_vir_address + buf_len))
                tmp_addr -= buf_len;
              tile_size+=*tmp_addr<<16;
              tmp_addr++;
              if (tmp_addr >= (buf_vir_address + buf_len))
                tmp_addr -= buf_len;
              tile_size+=*tmp_addr<<8;
              tmp_addr++;
              if (tmp_addr >= (buf_vir_address + buf_len))
                tmp_addr -= buf_len;
              tile_size+=*tmp_addr;
              tmp_addr++;
              dec->tile_offset_end[tile_id]=tile_size+4;
              tile_size+=4;
              if(tile_size > buf_len)
              {
                tile_size = buf_len;
                dec->tile_offset_end[tile_id]= buf_len ;
              }
              if(dec->tile_offset_end[tile_id]<dec->tile_offset_start[tile_id])
              {
                dec->tile_offset_end[tile_id]=dec->tile_offset_start[tile_id];
              }
              tmp_addr += dec->tile_offset_end[tile_id]-dec->tile_offset_start[tile_id];
            }
#else
            *p++ = tmp - prev_w;
            *p++ = h;
#endif
            prev_w = tmp;
            if(dec->tile_col_width_sb[tile_id] && dec->tile_row_height_sb[tile_id])
              tile_id ++;
            else
            {
              if(dec->tile_col_width_sb[tile_id]==0 && i==0)
                actual_tile_cols--;
              if(dec->tile_row_height_sb[tile_id]==0 && j==0)
                actual_tile_rows--;
            }
          }
          start_sb = 0;
          for (j = 0; j < tile_cols; j++) {
            dec->tile_col_start_sb[j] = start_sb;
            start_sb += dec->tile_col_width_sb[j];
          }
        }
        dec->log2_tile_rows = tile_log2(1, actual_tile_rows);
        dec->log2_tile_columns = tile_log2(1, actual_tile_cols);
      }

    } else {
      /* just one "tile", dimensions equal to pic size in SBs */
      dec->tile_col_width_sb[tile_id]=(asic_buff->width + 63) / 64;
      dec->tile_row_height_sb[tile_id]=(asic_buff->height + 63) / 64;
      dec->tile_col_start_sb[tile_id] = 0;

      //for stream start/end
      dec->tile_offset_start[tile_id]=tile_size;
      dec->tile_offset_end[tile_id]=data_len - dec->frame_tag_size - dec->offset_to_dct_parts;
      if(dec->tile_offset_end[tile_id] - dec->tile_offset_start[tile_id] > buf_len)
      {
        dec->tile_offset_end[tile_id] = dec->tile_offset_start[tile_id] + buf_len;
      }
    }
  }
}

u32 Vp9AsicRun(struct Vp9DecContainer *dec_cont, u32 pic_id) {
  i32 ret = 0;
  i32 core_id = 0;
  u32 i;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  const u32 tile_cols = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;
  u32 n_jobs = tile_cols;
  struct JobData1* dec_jobs;
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  struct DWLReqInfo info = {0};

  dec_jobs = (struct JobData1*)DWLmalloc(n_jobs * sizeof(struct JobData1));
  if (dec_jobs == NULL)
  	return DEC_MEMFAIL;

  dec_cont->job = dec_jobs;

  if (!dec_cont->asic_running) {
    dec_cont->asic_running = 1;

    /* allocate memory before reserve hw, for multi cores only need allocate buffer once. */
    ret = Vp9AsicAllocateFilterBlockMem(dec_cont);
    if (ret == HANTRO_NOK) {
      dec_cont->asic_running = 0;
      return DEC_MEMFAIL;
    }
    for (i = 0; i < n_jobs; i++) {
      Vp9AsicSetFilterBlockMemIInfo(dec_cont, i);

      if (dec_cont->b_mc) {
        Vp9AsicSetMulticoreParams(dec_cont, i);
      } else {
        SET_ADDR_REG(dec_cont->vp9_regs[i], HWIF_DEC_VERT_FILT_BASE,
                     asic_buff->tile_edge.bus_address + asic_buff->filter_mem_offset[i]);
        SET_ADDR_REG(dec_cont->vp9_regs[i], HWIF_DEC_BSD_CTRL_BASE,
                 asic_buff->filter_control.bus_address);
        SET_ADDR_REG(dec_cont->vp9_regs[i], HWIF_RFC_COLBUF_BASE,
                     asic_buff->tile_edge.bus_address + asic_buff->rfc_offset[i]);
        SET_ADDR_REG(dec_cont->vp9_regs[i], HWIF_RFC_LEFT_COLBUF_BASE,
                     asic_buff->tile_edge.bus_address + asic_buff->rfc_offset[i]);
      }

      /* Set the buffer bus address for using ddr_low_latency */
      if(dec_cont->llstrminfo.strm_status_in_buffer == 1){
        SET_ADDR_REG(dec_cont->vp9_regs[i], HWIF_LG_STREAM_STATUS_BASE,
                     dec_cont->llstrminfo.strm_status.bus_address);
      }

      dec_cont->tile_reg = dec_cont->vp9_regs[i];
      info.core_mask = dec_cont->core_mask;
      info.width = dec_cont->width;
      info.height = dec_cont->height;
      info.owner = (void *)dec_cont;
      if (dec_cont->vcmd_used) {
        dec_cont->mc_buf_id = 0;
        if (dec_cont->b_mc) {
          FifoObject obj;
          FifoPop(dec_cont->fifo_core, &obj, FIFO_EXCEPTION_DISABLE);
          dec_cont->mc_buf_id = (i32)(addr_t)obj;
        }
        ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_jobs[i].cmdbuf_id);
        dec_cont->cmdbuf_id = dec_jobs[i].cmdbuf_id;
      } else {
        ret = DWLReserveHw(dec_cont->dwl, &info, &core_id);
      }

      if (ret != DWL_OK) {
        return VP9HWDEC_HW_RESERVED;
      }

      dec_cont->core_id = dec_cont->vcmd_used ? dec_cont->mc_buf_id :
          core_id;

      /* core_id relative asic process */
      Vp9AsicSetFilterBlockMemIInfo(dec_cont, i);

      Vp9AsicProbUpdate(dec_cont, i);
      Vp9AsicSetTileInfo(dec_cont);

      /* Warning: only single core are currently supported (core_id = 0) */
      PPSetLancozsScaleRegs(dec_cont->vp9_regs[i], hw_feature, dec_cont->ppu_cfg, 0);
      /* the [prob_tbl] and [tile map] are all in misc_linear buffer.
      * [prob_tbl] : always needs DMA.
      * [tile map] : always needs DMA.
      */
      DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->misc_linear, 0,
                      dec_cont->asic_buff->misc_linear.size, HOST_TO_DEVICE);

      /* Resolution change will clear any previous segment map. */
      if (dec->resolution_change || (dec->key_frame && i == 0) ||
          dec->error_resilient || dec->intra_only) {
        if (dec_cont->b_mc && i == 0) {
          DWLmemset(asic_buff->segment_map.virtual_address, 0,
                    asic_buff->segment_map.logical_size);
        } else if (!dec_cont->b_mc) {
          DWLmemset(asic_buff->segment_map.virtual_address, 0,
                    asic_buff->segment_map.logical_size);
        }
      }

      if (dec_cont->b_mc && i == 0) {
        DWLDMATransData(dec_cont->dwl, &asic_buff->segment_map, 0,
                        asic_buff->segment_map.size, HOST_TO_DEVICE);
      } else if (!dec_cont->b_mc) {
        DWLDMATransData(dec_cont->dwl, &asic_buff->segment_map, 0,
                        asic_buff->segment_map.size, HOST_TO_DEVICE);
      }

      if (dec_cont->vcmd_used)
        DWLReadPpConfigure(dec_cont->dwl, dec_jobs[i].cmdbuf_id, dec_cont->ppu_cfg, 0);
      else
        DWLReadPpConfigure(dec_cont->dwl, dec_cont->core_id, dec_cont->ppu_cfg, 0);

      SetDecRegister(dec_cont->vp9_regs[i], HWIF_DEC_E, 1);
      if (dec_cont->vcmd_used) {
        if(dec_cont->mc_buf_id >= dec_cont->n_cores_available) {
          dec_cont->mc_buf_id = 0; /* in theory, shouldn't reach here */
        }
        DWLFlushRegister(dec_cont->dwl, dec_jobs[i].cmdbuf_id, dec_cont->vp9_regs[i],
                         dec_cont->mc_refresh_regs[dec_cont->mc_buf_id], dec_cont->mc_buf_id);
      }
      else {
        FlushDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs[i],
                          hw_feature->max_ppu_count);
      }

      dec_jobs[i].core_id = core_id;
      dec_jobs[i].dwl = (void*)(dec_cont->dwl);

      dec_cont->tile_status[i] = TILE_TODO;

      /* when b_mc is 0, the callback callback_arg has been clear up in DWLReserveHw/DWLReserveCmdBuf */
      if (dec_cont->b_mc) {
         u32 id;
        if (dec_cont->vcmd_used) {
          id = dec_cont->mc_buf_id;
          DWLSetIRQCallback(dec_cont->dwl, dec_jobs[i].cmdbuf_id, Vp9MCHwRdyCallback, dec_cont);
        } else {
          id = dec_jobs[i].core_id;
          DWLSetIRQCallback(dec_cont->dwl, dec_cont->core_id, Vp9MCHwRdyCallback, dec_cont);
        }
        if (dec_cont->hw_rdy_callback_arg[id] == NULL) {
          dec_cont->hw_rdy_callback_arg[id] = DWLmalloc(sizeof(struct Vp9HwRdyCallbackArg));
        }
        dec_cont->hw_rdy_callback_arg[id]->tile_index = i;
      }

      if (dec_cont->vcmd_used) {
        DWLEnableCmdBuf(dec_cont->dwl, dec_jobs[i].cmdbuf_id);
      } else {
        DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[i][1]);
      }
#ifdef ASIC_TRACE_SUPPORT
      if (dec_cont->b_mc)
        while (dec_cont->tile_status[i] == TILE_TODO) {
          sched_yield();
        };
#endif
    }
  } else {
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 13,
                dec_cont->vp9_regs[0][13]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 14,
                dec_cont->vp9_regs[0][14]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 15,
                dec_cont->vp9_regs[0][15]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[0][1]);
  }

  Vp9SetupPicToOutput(dec_cont, pic_id);

  return ret;
}

void Vp9AsicSyncMC(struct Vp9DecContainer *dec_cont) {
  const u32 n_jobs = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;
  int i;
  for(i = 0; i < n_jobs; i++) {
    while (dec_cont->tile_status[i] == TILE_TODO)
      sched_yield();

    dec_cont->tile_status[i] = TILE_TODO;
  }
}

/* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
static void Vp9MCHwRdyCallback(void* arg, i32 core_id)
{
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)arg;
  struct Vp9HwRdyCallbackArg *info = NULL;
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  u32 combuf_id = 0, index, core_status;

  if(dec_cont->vcmd_used) {
    combuf_id = core_id;
    core_id = DWLGetVcmdMCVirtualCoreId(dec_cont->dwl, combuf_id);
    info = dec_cont->hw_rdy_callback_arg[core_id];
    index = info->tile_index;
    DWLVcmdMCRefreshStatusRegs(dec_cont->dwl, dec_cont->vp9_regs[index], combuf_id);
  }
  else {
    info = dec_cont->hw_rdy_callback_arg[core_id];
    index = info->tile_index;
    RefreshDecRegisters(dec_cont->dwl, core_id, dec_cont->vp9_regs[index], hw_feature->max_ppu_count);
  }

  /* React to the HW return value */
  core_status = GetDecRegister(dec_cont->vp9_regs[index], HWIF_DEC_IRQ_STAT);

  /* check if DEC_RDY, all other status are errors */
  if (core_status != DEC_HW_IRQ_RDY) {
    DWLmemset((void*)((u8 *)dec_cont->asic_buff->multicore_sync_buffers.virtual_address + index * 64), 0xFF, 64);
    DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->multicore_sync_buffers, index * 64, 64, HOST_TO_DEVICE);
  }

  if(index == 0)
    dec_cont->cycle_sum = 0;
  dec_cont->cycle_sum += GetDecRegister(dec_cont->vp9_regs[index], HWIF_PERF_CYCLE_COUNT);

#ifdef FPGA_PERF_AND_BW
  if(!dec_cont->vcmd_used)
    DecPerfInfoCount(dec_cont->dwl, dec_cont->core_id, &dec_cont->perf_info,
                     NEXT_MULTIPLE(dec_cont->decoder.height, 8) *
                     NEXT_MULTIPLE(dec_cont->decoder.width, 8),
                     dec_cont->decoder.bit_depth);
#endif

  dec_cont->tile_status[index] = TILE_DONE;
  if (dec_cont->vcmd_used) {
    DWLReleaseCmdBuf(dec_cont->dwl, combuf_id);
    if (dec_cont->b_mc)
      FifoPush(dec_cont->fifo_core, (FifoObject)(addr_t)core_id, FIFO_EXCEPTION_DISABLE);
  }
  else
    DWLReleaseHw(dec_cont->dwl, core_id);
}

u32 Vp9AsicSync(struct Vp9DecContainer *dec_cont) {
  enum DWLRet ret[VP9_MAX_TILE_COLS] = {0};

  const u32 n_jobs = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;
  i32 i;
  u32 asic_status = 0;
  struct Vp9Decoder *dec = &dec_cont->decoder;
  u32 tile_rows = (1 << dec->log2_tile_rows);
  struct JobData1* dec_jobs = (struct JobData1*)dec_cont->job;

  if (dec_cont->first_tile_empty)
    tile_rows -= 1;

  if (dec_cont->b_mc == 0) {
    for (i = 0; i < n_jobs; i++) {
      if (dec_cont->vcmd_used)
        ret[i] = DWLWaitCmdBufReady(dec_cont->dwl, dec_jobs[i].cmdbuf_id);
      else
        ret[i] = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id, (u32)DEC_X170_TIMEOUT_LENGTH);

      dec_cont->tile_status[i] = TILE_DONE;

      if(dec_cont->vcmd_used) {
        DWLRefreshRegister(dec_cont->dwl, dec_jobs[i].cmdbuf_id, dec_cont->vp9_regs[i]);
      }
      else {
        RefreshDecRegisters(dec_cont->dwl, dec_cont->core_id, dec_cont->vp9_regs[i],
                            dec_cont->hw_feature->max_ppu_count);
      }

      if (dec_cont->vcmd_used) {
        (void) DWLReleaseCmdBuf(dec_cont->dwl, dec_jobs[i].cmdbuf_id);
      }
      else {
        /* HW done, release it! */
        DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vp9_regs[i][1]);
        (void) DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
      }
    }
  } else {
    Vp9AsicSyncMC(dec_cont);
  }

  if (dec_cont->asic_buff->realloc_seg_map_buffer == 0) {/* realloc */
    DWLDMATransData(dec_cont->dwl, &dec_cont->asic_buff->segment_map, 0,
                    dec_cont->asic_buff->segment_map.size, DEVICE_TO_HOST);
  }
  if (SwGetCoreMaskByFeature(dec_cont->dwl, VSI_VP9_HW_PROB) && !dec_cont->b_mc) {
    DWLDMATransData(dec_cont->dwl, &dec_cont->decoder.entropy_last, 0,
                    dec_cont->decoder.entropy_last.size, DEVICE_TO_HOST);
  }

  DWLfree(dec_jobs);

  for (size_t t = 0; t < n_jobs; ++t) {

    if (ret[t] != DWL_HW_WAIT_OK) {
      APITRACEERR("%s","DWLWaitHwReady\n");
      APITRACEDEBUG("DWLWaitHwReady returned: %d\n", ret[t]);
      ClearSwIRQ(dec_cont->vp9_regs[t]);
      dec_cont->asic_running = 0;
      asic_status = (ret[t] == DWL_HW_WAIT_ERROR) ? VP9HWDEC_SYSTEM_ERROR
             : VP9HWDEC_SYSTEM_TIMEOUT;
      break;
    }

    u32 core_asic_status = 0;

    /* React to the HW return value */
    core_asic_status = GetDecRegister(dec_cont->vp9_regs[t], HWIF_DEC_IRQ_STAT);
    asic_status |= core_asic_status;
    ClearSwIRQ(dec_cont->vp9_regs[t]);
  }

  dec_cont->first_tile_empty = 0;

  dec_cont->asic_running = 0;

  /* low latency: update updated_reg that update thread know current frame dec done. */
  while(dec_cont->low_latency && (!dec_cont->llstrminfo.updated_reg)){
    sched_yield();
  }
  if(dec_cont->low_latency) {
    dec_cont->llstrminfo.updated_reg = 0;
    dec_cont->llstrm_curr_address = GetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_BASE_LSB);
    if (sizeof(addr_t) == 8)
      dec_cont->llstrm_curr_address |= ((u64)GetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_BASE_MSB)) << 32;
  }

  return asic_status;
}

void Vp9AsicProbUpdate(struct Vp9DecContainer *dec_cont, size_t tile) {
  av_unused const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  const u32 tile_cols = dec_cont->b_mc ? dec_cont->decoder.vp9_tile_cols : 1;

  struct DWLLinearMem *segment_map = &dec_cont->asic_buff->segment_map;
  u8 *asic_prob_base = (u8 *)dec_cont->asic_buff->misc_linear.virtual_address + dec_cont->asic_buff->prob_tbl_offset;
  av_unused u32 buff_index = dec_cont->asic_buff->out_buffer_i;

  if (hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
    SET_ADDR_REG(dec_cont->tile_reg, HWIF_PROB_TAB_BASE,
                 dec_cont->decoder.entropy_last.bus_address +
                 NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 256) * dec_cont->decoder.probs[dec_cont->decoder.frame_context_idx]);
    SET_ADDR_REG(dec_cont->tile_reg, HWIF_CTX_COUNTER_BASE,
                 dec_cont->decoder.entropy_last.bus_address +
                 NEXT_MULTIPLE(sizeof(struct Vp9EntropyProbs), 256) * dec_cont->decoder.probs[4]);

    SET_ADDR_REG(dec_cont->tile_reg, HWIF_DELTA_PROBS_BASE,
                 dec_cont->asic_buff->pictures[dec_cont->asic_buff->out_buffer_i].bus_address +
                 dec_cont->asic_buff->delta_probs_offset[dec_cont->asic_buff->out_buffer_i]);
    DWLDMATransData(dec_cont->dwl, &dec_cont->decoder.entropy_last, 0,
                    dec_cont->decoder.entropy_last.size, HOST_TO_DEVICE);

    SetDecRegister(dec_cont->tile_reg, HWIF_REFRESH_ENTROPY_PROBS, dec_cont->decoder.refresh_entropy_probs);
    SetDecRegister(dec_cont->tile_reg, HWIF_ERROR_RESILIENT_MODE, dec_cont->decoder.error_resilient);
    SetDecRegister(dec_cont->tile_reg, HWIF_FRAME_PARALLEL_DECODING, dec_cont->decoder.frame_parallel_decoding);
    SetDecRegister(dec_cont->tile_reg, HWIF_KEY_FRAME, dec_cont->decoder.key_frame);
    SetDecRegister(dec_cont->tile_reg, HWIF_INTRA_ONLY, dec_cont->decoder.intra_only);
    SetDecRegister(dec_cont->tile_reg, HWIF_PREV_IS_KEY_FRAME, dec_cont->decoder.prev_is_key_frame);
    SetDecRegister(dec_cont->tile_reg, HWIF_TRANSFORM_MODE_TX_MODE_SELECT,
                   dec_cont->decoder.transform_mode == TX_MODE_SELECT);
    SetDecRegister(dec_cont->tile_reg, HWIF_MCOMP_FILTER_TYPE_SWITCHABLE,
                   dec_cont->decoder.mcomp_filter_type == SWITCHABLE);
    SetDecRegister(dec_cont->tile_reg, HWIF_HIGH_PREC_MV_E,
                   !dec_cont->decoder.key_frame && dec_cont->decoder.allow_high_precision_mv);
    SetDecRegister(dec_cont->tile_reg, HWIF_HW_UPDATE_PROB_E, 1);
#ifdef ASIC_TRACE_SUPPORT
    cmodel_frame_context_buffer_idx = dec_cont->decoder.probs[dec_cont->decoder.frame_context_idx];
    cmodel_temp_context_buffer_idx = dec_cont->decoder.probs[4];
    cmodel_reset_frame_context_buffer = dec_cont->decoder.reset_frame_context >= 2 ? 1: 0;
#endif
  } else {
    SET_ADDR_REG(dec_cont->tile_reg, HWIF_DELTA_PROBS_BASE, (addr_t)0);
    if (tile == 0) {
      /* Write probability tables to HW memory */
      DWLmemcpy(asic_prob_base, &dec_cont->decoder.entropy,
                sizeof(struct Vp9EntropyProbs));
    }

    SET_ADDR_REG(dec_cont->tile_reg, HWIF_PROB_TAB_BASE,
                 dec_cont->asic_buff->misc_linear.bus_address + dec_cont->asic_buff->prob_tbl_offset);
    SetDecRegister(dec_cont->tile_reg, HWIF_HW_UPDATE_PROB_E, 0);

    SET_ADDR_REG(dec_cont->tile_reg, HWIF_CTX_COUNTER_BASE,
                 dec_cont->asic_buff->ctx_counters.bus_address +
                 NEXT_MULTIPLE(sizeof(struct Vp9EntropyCounts), 16) * tile);
  }

#if 0
  SetDecRegister(dec_cont->vp9_regs, HWIF_SEGMENT_READ_BASE_LSB,
                 segment_map[dec_cont->active_segment_map].bus_address);
  SetDecRegister(dec_cont->vp9_regs, HWIF_SEGMENT_WRITE_BASE_LSB,
                 segment_map[1 - dec_cont->active_segment_map].bus_address);
#else
  SET_ADDR_REG(dec_cont->tile_reg, HWIF_SEGMENT_READ_BASE,
               segment_map->bus_address + dec_cont->active_segment_map * dec_cont->asic_buff->segment_map_size);
  SET_ADDR_REG(dec_cont->tile_reg, HWIF_SEGMENT_WRITE_BASE,
               segment_map->bus_address + (1 - dec_cont->active_segment_map) * dec_cont->asic_buff->segment_map_size);
#endif

  /* Update active segment map for next frame */
  if (dec_cont->decoder.segment_map_update && tile == tile_cols - 1)
    dec_cont->active_segment_map = 1 - dec_cont->active_segment_map;
}

void Vp9UpdateRefs(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  {
    if (dec_cont->decoder.reset_frame_flags) {
      Vp9BufferQueueUpdateRef(dec_cont->bq, (1 << NUM_REF_FRAMES) - 1,
                              REFERENCE_NOT_SET);
      Vp9BufferQueueUpdateRef(dec_cont->pp_bq, (1 << NUM_REF_FRAMES) - 1,
                              REFERENCE_NOT_SET);
      dec_cont->decoder.reset_frame_flags = 0;
    }
    Vp9BufferQueueUpdateRef(dec_cont->bq, dec_cont->decoder.refresh_frame_flags,
                            asic_buff->out_buffer_i);
    Vp9BufferQueueUpdateRef(dec_cont->pp_bq, dec_cont->decoder.refresh_frame_flags,
                            asic_buff->out_pp_buffer_i);
  }
  if (!dec_cont->decoder.show_frame/* || dec_cont->error_info != DEC_NO_ERROR*/) {
    /* If the picture will not be outputted, we need to remove ref used to
     * protect the output. */
    Vp9BufferQueueRemoveRef(dec_cont->bq, asic_buff->out_buffer_i);
    /* For raster/dscale output buffer, return it to input buffer queue. */
    Vp9BufferQueueRemoveRef(dec_cont->pp_bq, asic_buff->out_pp_buffer_i);
  }
}

void Vp9AsicReset(struct Vp9DecContainer *dec_cont) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  asic_buff->realloc_out_buffer = 0;
  asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;
  asic_buff->prev_out_buffer_i = VP9_UNDEFINED_BUFFER;
  asic_buff->out_pp_buffer_i = VP9_UNDEFINED_BUFFER;
  asic_buff->realloc_seg_map_buffer = 0;
  asic_buff->realloc_tile_edge_mem = 0;
#ifdef USE_OMXIL_BUFFER
  //DWLmemset(asic_buff->pictures, 0, MAX_PIC_BUFFERS * sizeof(struct DWLLinearMem));
#endif
  DWLmemset(asic_buff->first_show, 0, MAX_PIC_BUFFERS * sizeof(i32));
  DWLmemset(asic_buff->picture_info, 0, MAX_PIC_BUFFERS * sizeof(struct Vp9DecPicture));
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    asic_buff->out_pp_buffer_i = VP9_UNDEFINED_BUFFER;
    DWLmemset(asic_buff->pp_buffer_map, 0, MAX_PIC_BUFFERS * sizeof(i32));
#ifdef USE_OMXIL_BUFFER
    DWLmemset(asic_buff->pp_pictures, 0, MAX_PIC_BUFFERS * sizeof(struct DWLLinearMem));
#endif
  }
}

/* Fix chroma RFC table when the width/height is (48, 64] */
void Vp9FixChromaRFCTable(struct Vp9DecContainer *dec_cont) {
  u32 frame_width = NEXT_MULTIPLE(dec_cont->width, 8);
  u32 frame_height = NEXT_MULTIPLE(dec_cont->height, 8);
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 i, j, cbs_size = 0;
  u8 *pch_rfc_tbl, *ptbl = NULL;
  u8 cbs_sizes_8bit[14] = {129, 2, 4, 8, 16, 32, 64, 129, 2, 4, 8, 16, 32, 64};
  u8 cbs_sizes_10bit[14] = {0xa1, 0x42, 0x85, 10, 20, 40, 80, 0xa1, 0x42, 0x85, 10, 20, 40, 80};
  u32 pic_width_in_cbsc  = NEXT_MULTIPLE(frame_width, 256)/16;
  u32 pic_height_in_cbsc = NEXT_MULTIPLE(frame_height/2, 4)/4;
  u32 offset;
  u32 bit_depth = dec_cont->decoder.bit_depth;

  if (!dec_cont->use_video_compressor ||
      ((frame_width <= 48 || frame_width > 64) &&
       (frame_height <= 48 || frame_height > 64)))
    return;

  pch_rfc_tbl = (u8 *)asic_buff->pictures[asic_buff->out_buffer_i].virtual_address +
                asic_buff->cbs_c_tbl_offset[asic_buff->out_buffer_i];

  /* Fill missing 1 CBS in the right edge. */
  if (frame_width > 48 && frame_width <= 64) {
    cbs_size = (frame_width - 48) * 4;
    for (i = 0; i < frame_height / 8; i++) {
#if 1
      pch_rfc_tbl[4] = (pch_rfc_tbl[4] & 0x1f) | ((cbs_size & 0x07) << 5);
      pch_rfc_tbl[5] = cbs_size >> 3;
#else
      pch_rfc_tbl[11] = (pch_rfc_tbl[11] & 0x1f) | ((cbs_size & 0x07) << 5);
      pch_rfc_tbl[10] = cbs_size >> 3;
#endif
      pch_rfc_tbl += 16;
    }
  } else {
    pch_rfc_tbl += pic_width_in_cbsc * 6;
  }

  if (bit_depth == 8) {
    cbs_size = 64;
    ptbl = cbs_sizes_8bit;
  } else if (bit_depth == 10) {
    cbs_size = 80;
    ptbl = cbs_sizes_10bit;
  }

  // Compression table for C
  if (frame_height > 48 && frame_height <= 64) {
    for (i = 0; i < pic_height_in_cbsc - 6; i++) {
      offset = 0;
      for (j = 0; j < pic_width_in_cbsc/16; j++) {
#if 0
        *(u16 *)pcbs = offset;
        memcpy(pcbs+2, ptbl, 14);
#else
        if (ptbl != NULL)  memcpy(pch_rfc_tbl, ptbl, 14);
        *(pch_rfc_tbl+14) = offset >> 8;
        *(pch_rfc_tbl+15) = offset & 0xff;
#endif
        pch_rfc_tbl += 16;
        offset += 16 * cbs_size;
      }
    }
  }
}

void Vp9GetRefFrmSize(struct Vp9DecContainer *dec_cont,
                       u32 *luma_size, u32 *chroma_size,
                       u32 *rfc_luma_size, u32 *rfc_chroma_size) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 pic_width_in_cbsy, pic_height_in_cbsy;
  u32 pic_width_in_cbsc, pic_height_in_cbsc;
  u32 tbl_sizey, tbl_sizec;

  u32 bit_depth;
  u32 out_w, out_h;
  u32 ref_size;

  bit_depth = dec_cont->decoder.bit_depth;

  out_w = NEXT_MULTIPLE(4 * asic_buff->width * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
  out_h = asic_buff->height / 4;
  ref_size = out_w * out_h;
  if (luma_size) *luma_size = ref_size;
  if (chroma_size) {
    out_w = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(asic_buff->width, 16) * bit_depth, ALIGN(dec_cont->align) * 8) / 8;
    out_h = asic_buff->height / 4;
    ref_size = out_w * out_h;
    *chroma_size = ref_size / 2;
  }

  if (dec_cont->use_video_compressor) {
    pic_width_in_cbsy = (asic_buff->width + 8 - 1)/8;
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = (asic_buff->width + 16 - 1)/16;
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (asic_buff->height + 8 - 1)/8;
    pic_height_in_cbsc = (asic_buff->height/2 + 4 - 1)/4;

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

static i32 Vp9ReplaceRefPic(struct Vp9DecContainer *dec_cont, u32 *index_info, u32 curr_index) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  u32 tmp1_index = 0;
  u32 tmp2_index = 0;

  u32 tmp1_pic_size = 0;
  u32 tmp2_pic_size = 0;
  u32 curr_pic_size = 0;

  curr_pic_size = asic_buff->picture_info[index_info[curr_index]].coded_width *
                  asic_buff->picture_info[index_info[curr_index]].coded_height;
  /* NO_RFC: if error_ratio less than 10%, don't replace it, use it as correct. */
  if (!dec_cont->use_video_compressor &&
      asic_buff->picture_info[index_info[curr_index]].error_ratio <= EC_RATIO_THRESHOLD)
    return curr_index;

  /* Ref search priority: golden > altref > last */
  switch (curr_index)
  {
  case 0:
    tmp1_index = 1;
    tmp2_index = 2;
    break;
  case 1:
    tmp1_index = 2;
    tmp2_index = 0;
    break;
  case 2:
    tmp1_index = 1;
    tmp2_index = 0;
    break;
  default:
    return EC_INVALID_IDX;
    break;
  }
  tmp1_pic_size = asic_buff->picture_info[index_info[tmp1_index]].coded_width *
                  asic_buff->picture_info[index_info[tmp1_index]].coded_height;
  tmp2_pic_size = asic_buff->picture_info[index_info[tmp2_index]].coded_width *
                  asic_buff->picture_info[index_info[tmp2_index]].coded_height;
  /* firstly: find from correct ref */
  if (asic_buff->picture_info[index_info[tmp1_index]].error_info == DEC_NO_ERROR)
    if (tmp1_pic_size == curr_pic_size) return tmp1_index;
  if (asic_buff->picture_info[index_info[tmp2_index]].error_info == DEC_NO_ERROR)
    if (tmp2_pic_size == curr_pic_size) return tmp2_index;
  /* secondly: find from ref_error ref */
  if (asic_buff->picture_info[index_info[tmp1_index]].error_info == DEC_REF_ERROR)
    if (tmp1_pic_size == curr_pic_size) return tmp1_index;
  if (asic_buff->picture_info[index_info[tmp2_index]].error_info == DEC_REF_ERROR)
    if (tmp2_pic_size == curr_pic_size) return tmp2_index;
  /* thirdly: find from tolerable_error_count */
  if (!dec_cont->use_video_compressor &&
      asic_buff->picture_info[index_info[tmp1_index]].error_ratio <= EC_RATIO_THRESHOLD)
    if (tmp1_pic_size == curr_pic_size) return tmp1_index;
  if (!dec_cont->use_video_compressor &&
      asic_buff->picture_info[index_info[tmp2_index]].error_ratio <= EC_RATIO_THRESHOLD)
    if (tmp2_pic_size == curr_pic_size) return tmp2_index;

  /* not find */
  return EC_INVALID_IDX;
}

 /* Allocate buffer for sync mc and dmv buffer */
i32 Vp9AllocParasiticBuf(const void *dwl, u32 size, struct DWLLinearMem *info, u32 secure_mode) {

  if(size == 0)
    return DEC_PARAM_ERROR;
  info->mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(info->mem_type, DWL_MEM_USAGE_TMP_DIRMV, secure_mode);
  if (DWLMALLOC_LINEAR_MEM2(dwl, size, info) !=0)
    return (DEC_MEMFAIL);

  return DWL_OK;
}

void Vp9ReleaseParasiticBuf(const void *dwl, struct DWLLinearMem *info) {
  DWLFREE_LINEAR_MEM2(dwl, info);
  DWLmemset(info, 0, sizeof(*info));
  return;
}

i32 Vp9ReleaseParasiticBufs(struct Vp9DecContainer *dec_cont) {
  u32 i = 0;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    if (DWL_DEVMEM_VAILD(asic_buff->dpb_parasitic_buf[i]))
      Vp9ReleaseParasiticBuf(dec_cont->dwl, &asic_buff->dpb_parasitic_buf[i]);
  }
  DWLmemset(asic_buff->dpb_parasitic_buf, 0, sizeof(asic_buff->dpb_parasitic_buf));
  return DWL_OK;
}