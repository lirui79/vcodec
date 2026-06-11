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

#include "dwlthread.h"
#include "basetype.h"
#include "decapicommon.h"
#include "commonfunction.h"
#include "errorhandling.h"
#include "dwl.h"
#include "fifo.h"
#include "regdrv.h"
#include "vp9decapi.h"
#include "vp9hwd_asic.h"
#include "vp9hwd_container.h"
#include "vp9hwd_output.h"
#include "vpufeature.h"
#include "dec_log.h"

#define EOS_MARKER   (-1)
#define ABORT_MARKER (-2)
#define FLUSH_MARKER (-3)
#define NO_OUTPUT_MARKER (-4)


static u32 CycleCount(struct Vp9DecContainer *dec_cont);
static u32 GetErrorRatio(struct Vp9DecContainer *dec_cont, u32 asic_running);
#ifdef FPGA_PERF_AND_BW
static u32 BitCount(struct Vp9DecContainer *dec_cont);
static u32 BwCount(struct Vp9DecContainer *dec_cont, u32 num);
#endif
static i32 FindIndex(struct Vp9DecContainer *dec_cont, DWLMemAddr address, u32 buffer_type);
static i32 NextOutput(struct Vp9DecContainer *dec_cont);
static i32 Vp9ProcessAsicStatus(struct Vp9DecContainer *dec_cont, u32 asic_status);
// static void Vp9ConstantConcealment(struct Vp9DecContainer *dec_cont, u8 value);
static enum DecRet Vp9ECDecisionOutput(struct Vp9DecContainer *dec_cont, struct Vp9DecPicture *output);

#ifdef FPGA_PERF_AND_BW
u32 vp9_pic_num = 0;
#endif

u32 CycleCount(struct Vp9DecContainer *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->height, 16) *
             NEXT_MULTIPLE(dec_cont->width, 16)) >> 8;
  if (mbs) {
    if (dec_cont->b_mc)
      cycles = dec_cont->cycle_sum / mbs;
    else
      cycles = GetDecRegister(dec_cont->vp9_regs[0], HWIF_PERF_CYCLE_COUNT) / mbs;
  }

  return cycles;
}
u32 GetErrorRatio(struct Vp9DecContainer *dec_cont, u32 asic_running) {
  u32 i = 0;
  u32 num_tiles = 0;
  u32 num_err_sbs = 0;
  u32 error_ratio = 0;
  u32 tile_col_width_sb[129] = {0};
  u32 tile_row_height_sb[129] = {0};
  struct Vp9Decoder *dec = &dec_cont->decoder;
  struct ErrorPicInfo error_pic_info;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  if (dec_cont->error_info == DEC_NO_ERROR ||
      dec_cont->error_info == DEC_REF_ERROR) {
    return 0;
  } else {
    if (!asic_running) {
      return 1 * EC_ROUND_COEFF;
    }
    error_pic_info.error_x = GetDecRegister(dec_cont->vp9_regs[0], HWIF_MB_LOCATION_X);
    error_pic_info.error_y = GetDecRegister(dec_cont->vp9_regs[0], HWIF_MB_LOCATION_Y);
    if (dec->log2_tile_columns || dec->log2_tile_rows) {
      error_pic_info.tile_info.num_tile_rows    = (1 << dec->log2_tile_rows);
      error_pic_info.tile_info.num_tile_columns = (1 << dec->log2_tile_columns);
      num_tiles = error_pic_info.tile_info.num_tile_rows * error_pic_info.tile_info.num_tile_columns;
      for (i = 0; i < num_tiles; i++) {
        tile_col_width_sb[i] = dec->tile_col_width_sb[i];
        tile_row_height_sb[i] = dec->tile_row_height_sb[i];
      }
      error_pic_info.tile_info.col_width  = tile_col_width_sb;
      error_pic_info.tile_info.row_height = tile_row_height_sb;
    } else {
      error_pic_info.tile_info.num_tile_rows    = 1;
      error_pic_info.tile_info.num_tile_columns = 1;
    }
    error_pic_info.pic_width_in_ctb = (asic_buff->width + 63) / 64;
    error_pic_info.pic_height_in_ctb = (asic_buff->height + 63) / 64;
    error_pic_info.log2_ctb_size = 6;
    num_err_sbs = GetErrorCtbCount(&error_pic_info);
    /* the error can be ignored. */
    if (num_err_sbs == 0)
      dec_cont->error_info = DEC_NO_ERROR;
  }
  error_ratio = num_err_sbs * EC_ROUND_COEFF /
                (((asic_buff->width + 63) / 64) * ((asic_buff->height + 63) / 64));
  /* (JZQ)EC Unify: for test. */
  // if (error_ratio != 0) error_ratio = 5000;

  return error_ratio;
}
#ifdef FPGA_PERF_AND_BW
u32 BitCount(struct Vp9DecContainer *dec_cont){
  u32 bitrate = ((60 * GetDecRegister(dec_cont->vp9_regs[0], HWIF_STREAM_LEN) * 8/ NEXT_MULTIPLE(dec_cont->height, 8)) * 3840/ NEXT_MULTIPLE(dec_cont->width, 8)) * 2160;
  return bitrate;
}

u32 BwCount(struct Vp9DecContainer *dec_cont, u32 num){
  u64 bw_rd_wr = DWLReadBw(dec_cont->dwl, dec_cont->core_id, num);
  if (dec_cont->decoder.bit_depth == 10)
    bw_rd_wr *= 0.8;
  u32 out_val = bw_rd_wr * 16 * 1000/ (NEXT_MULTIPLE(dec_cont->height, 16) * NEXT_MULTIPLE(dec_cont->width, 16));
  return out_val;
}
#endif
i32 FindIndex(struct Vp9DecContainer *dec_cont, DWLMemAddr address, u32 buffer_type) {
  i32 i;
  struct DWLLinearMem *pictures = NULL;
  i32 num_buffers = 0;

  if (buffer_type == REFERENCE_BUFFER) {
    pictures = dec_cont->asic_buff->pictures;
    num_buffers = dec_cont->num_buffers;
  } else if (buffer_type ==  DOWNSCALE_OUT_BUFFER) {
    pictures = dec_cont->asic_buff->pp_pictures;
    num_buffers = dec_cont->num_pp_buffers;
  }

  for (i = 0; i < (i32)num_buffers; i++)
    if (DWL_DEVMEM_IN_RANGE(*(pictures + i), address)) break;
  ASSERT((u32)i < num_buffers);
  return i;
}

i32 NextOutput(struct Vp9DecContainer *dec_cont) {
  i32 i;
  u32 j;
  i32 output_i = -1;
  u32 size;
  FifoObject tmp;
  i32 ret;

  if (dec_cont->abort)
    return ABORT_MARKER;

  size = FifoCount(dec_cont->fifo_display);

  /* If there are pictures in the display reordering buffer, check them
   * first to see if our next output is there. */
  for (j = 0; j < size; j++) {
    if ((ret = FifoPop(dec_cont->fifo_display, &tmp,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
                       FIFO_EXCEPTION_ENABLE
#else
                       FIFO_EXCEPTION_DISABLE
#endif
                      )) != FIFO_ABORT) {
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
      if (ret == FIFO_EMPTY) break;
#endif
      i = (i32)((addr_t)tmp);
      if (dec_cont->asic_buff->display_index[i] == dec_cont->pic_number) {
        /*  fifo_display had the right output. */
        output_i = i;
        break;
      } else {
        tmp = (FifoObject)(addr_t)i;
        FifoPush(dec_cont->fifo_display, tmp, FIFO_EXCEPTION_DISABLE);
      }
    } else
      return ABORT_MARKER;
  }

  /* Look for output in decode ordered out_fifo. */
  while (output_i < 0) {
    /* Blocks until next output is available */
    if ((ret = FifoPop(dec_cont->fifo_out, &tmp,
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
                       FIFO_EXCEPTION_ENABLE
#else
                       FIFO_EXCEPTION_DISABLE
#endif
                      )) != FIFO_ABORT) {
#ifdef GET_OUTPUT_BUFFER_NON_BLOCK
      if (ret == FIFO_EMPTY) return NO_OUTPUT_MARKER;
#endif

      i = (i32)((addr_t)tmp);
      if (i == EOS_MARKER || i == FLUSH_MARKER) return i;

      if (dec_cont->asic_buff->display_index[i] == dec_cont->pic_number) {
        /*  fifo_out had the right output. */
        output_i = i;
      } else {
        /* Until we get the next picture in display order, push the outputs
        * to the display reordering fifo */
        tmp = (FifoObject)(addr_t)i;
        FifoPush(dec_cont->fifo_display, tmp, FIFO_EXCEPTION_DISABLE);
      }
    } else
      return ABORT_MARKER;
  }

  return output_i;
}


enum DecRet Vp9DecPictureConsumed(Vp9DecInst dec_inst,
                                  const struct Vp9DecPicture *picture) {
  if (dec_inst == NULL || picture == NULL) {
    return DEC_PARAM_ERROR;
  }
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  u32 buffer, i = 0;
  DWLMemAddr output_picture = (DWLMemAddr)0;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  if (!dec_cont->pp_enabled) {
    output_picture = (DWLMemAddr)picture->pictures[0].output_luma_bus_address;
  } else {
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
        continue;
      else {
        output_picture = (DWLMemAddr)picture->pictures[i].output_luma_bus_address;
        break;
      }
    }
  }
  /* For raster/dscale output buffer, return it to input buffer queue. */
  if ( IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    buffer = FindIndex(dec_cont, output_picture, DOWNSCALE_OUT_BUFFER);

    Vp9BufferQueueRemoveRef(dec_cont->pp_bq, buffer);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[buffer] = 0;

    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

  /* FIXME: here only external buffer will be consumed, since only external buffer
   * bases addresses will be set when Vp9DecPictureConsumed() is called. */
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    buffer = FindIndex(dec_cont, output_picture, REFERENCE_BUFFER);
    /* Remove the reference to the buffer. */
    Vp9BufferQueueRemoveRef(dec_cont->bq, buffer);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[buffer] = 0;

    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

  return DEC_OK;
}

enum DecRet Vp9DecNextPicture(Vp9DecInst dec_inst,
                              struct Vp9DecPicture *output) {
  i32 i;
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;
  if (dec_inst == NULL || output == NULL) {
    return DEC_PARAM_ERROR;
  }

  /* Check for valid decoder instance */
  if (dec_cont->checksum != dec_cont) {
    return DEC_NOT_INITIALIZED;
  }

  /*  NextOutput will block until there is an output. */
  i = NextOutput(dec_cont);
  if (i == EOS_MARKER) {
    return DEC_END_OF_STREAM;
  }
  if (i == ABORT_MARKER) {
    return DEC_ABORTED;
  }

  if (i == FLUSH_MARKER) {
    return DEC_FLUSHED;
  }
  if (i == NO_OUTPUT_MARKER)
    return DEC_OK;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    ASSERT(i >= 0 && (u32)i < dec_cont->num_buffers);
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    ASSERT(i >= 0 && (u32)i < dec_cont->num_pp_buffers);
  }

  *output = dec_cont->asic_buff->picture_info[i];
  dec_cont->pic_number++;
  /* complete output pic EC policy */
  /* decision pic output */
  if (Vp9ECDecisionOutput(dec_cont, output) != DEC_PIC_RDY)
    return DEC_DISCARD_INTERNAL;

  /* FIXME: if tiled buffers are output while not to be used (not external buffers) ,
   * we need remove the reference to it here, since in ZTE's framework, this channel
   * won't be used and won't be consumed neither. That will cause these buffers
   * unavailable when tile+ds are configured. */
#if 0
  if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
      !dec_cont->pp_enabled) {
    u32 buffer = FindIndex(dec_cont, output->output_luma_base, REFERENCE_BUFFER);

    /* Remove the reference to the buffer. */
    Vp9BufferQueueRemoveRef(dec_cont->bq, buffer);

    //pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    //dec_cont->asic_buff->display_index[buffer] = 0;

    //pthread_cond_signal(&dec_cont->sync_out_cv);
    //pthread_mutex_unlock(&dec_cont->sync_out);
  }
#endif

  return DEC_PIC_RDY;
}

enum DecRet Vp9DecEndOfStream(Vp9DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;

  /* Don't do end of stream twice. This is not thread-safe, so it must be
   * called from the single input thread that is also used to call
   * Vp9DecDecode. */
  if (dec_cont->dec_stat == VP9DEC_END_OF_STREAM) {
    return DEC_END_OF_STREAM;
  }

  if (dec_cont->asic_running)
    VP9SyncAndOutput(dec_cont);

  /* If buffer queue has been already initialized, we can use it to track
   * pending cores and outputs safely. */
  if (dec_cont->bq) {
    /* if the references and queue were already flushed, cannot
     * do it again. */
    if (dec_cont->asic_buff->out_buffer_i != VP9_UNDEFINED_BUFFER &&
        dec_cont->asic_buff->out_buffer_i != EMPTY_MARKER) {
      u32 i = 0;
      /* Workaround for ref counting since this buffer is never used. */
      Vp9BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
      dec_cont->asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;

      for (i = 0; i < VP9_REF_LIST_SIZE; i++) {
        i32 ref_buffer_i = Vp9BufferQueueGetRef(dec_cont->bq, i);
        if (ref_buffer_i != VP9_UNDEFINED_BUFFER) {
          Vp9BufferQueueRemoveRef(dec_cont->bq, ref_buffer_i);
        }
      }
    }
  }
  dec_cont->dec_stat = VP9DEC_END_OF_STREAM;
  FifoPush(dec_cont->fifo_out, (void *)EOS_MARKER, FIFO_EXCEPTION_DISABLE);

  return DEC_OK;
}

void Vp9PicToOutput(struct Vp9DecContainer *dec_cont, u32 asic_running) {
  struct PicCallbackArg *info = &dec_cont->pic_callback_arg;
#ifdef FPGA_PERF_AND_BW
  char* pic_types[] = {"        IDR", "Non-IDR (P)"};
#endif
  FifoObject tmp;
  u32 ref_index = info->index;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
    info->index = dec_cont->asic_buff->pp_buffer_map[info->index];

#ifdef USE_PICTURE_DISCARD
  if(!dec_cont->asic_buff->first_show[ref_index])
#endif
  {
    pthread_mutex_lock(&dec_cont->sync_out);
    while (dec_cont->asic_buff->display_index[info->index])
      pthread_cond_wait(&dec_cont->sync_out_cv, &dec_cont->sync_out);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

  /* Update picture_info:
   * 1. HW start;
   * 2. picture broken;
   */
  if (asic_running == 1 ||
      (asic_running == 0 && dec_cont->decoder.show_existing_frame == 0)) {
      /* update info: only when asic_running!=0 */
    info->pic.cycles_per_mb = CycleCount(dec_cont);
    info->pic.error_ratio = GetErrorRatio(dec_cont, asic_running);
    info->pic.error_info = dec_cont->error_info;
#ifdef FPGA_PERF_AND_BW
    info->pic.bitrate_4k60fps = BitCount(dec_cont);
    info->pic.bwrd_in_fs = BwCount(dec_cont, 0);
    info->pic.bwwr_in_fs = BwCount(dec_cont, 1);
    if (dec_cont->decoder.show_existing_frame == 0) {
      printf("PERF_PIC %2u, type %s, %4u cycles / mb, %4u Mbps (4k@60fps), BW R/W: %0.3f/%0.3f (x FRAME 4k@60fps)\n",
	      vp9_pic_num++,
	      pic_types[info->pic.is_intra_frame ? DEC_PIC_TYPE_I :DEC_PIC_TYPE_P],
	      info->pic.cycles_per_mb,
	      info->pic.bitrate_4k60fps/ 1024/ 1024,
	      (double)info->pic.bwrd_in_fs/ 1000/ 2.157,
	      (double)info->pic.bwwr_in_fs/ 1000/ 2.157);
    }
    info->pic.bitrate_4k60fps = 0;
    info->pic.bwrd_in_fs = 0;
    info->pic.bwwr_in_fs = 0;
#endif
  }
  dec_cont->asic_buff->picture_info[info->index] = info->pic;

  if (info->show_frame) {
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[ref_index] == 0)
#endif
    {
      dec_cont->asic_buff->display_index[info->index] = dec_cont->display_number++;
      tmp = (FifoObject)(addr_t)info->index;
      FifoPush(dec_cont->fifo_out, tmp, FIFO_EXCEPTION_DISABLE);
      dec_cont->asic_buff->first_show[ref_index] = 1;
    }
#ifdef USE_PICTURE_DISCARD
    else {
      /* Remove the reference to the buffer. */

      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
        Vp9BufferQueueRemoveRef(dec_cont->pp_bq, info->index);

      if (!dec_cont->pp_enabled)
        Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);

      //dec_cont->asic_buff->display_index[info->index] = 0;
    }
#endif

    if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);

#if 0
      pthread_mutex_lock(&dec_cont->sync_out);
      // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
      // be in the output queue once at a time.
      //dec_cont->asic_buff->display_index[info->index] = 0;

      pthread_cond_signal(&dec_cont->sync_out_cv);
      pthread_mutex_unlock(&dec_cont->sync_out);
#endif
    }
  }

}

void Vp9SetupPicToOutput(struct Vp9DecContainer *dec_cont, u32 pic_id) {
  struct PicCallbackArg *args = &dec_cont->pic_callback_arg;

  u32 bit_depth = dec_cont->decoder.bit_depth;
  u32 pp_index;
  const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 i;

  DWLmemset(args, 0, sizeof(struct PicCallbackArg));
  args->index = dec_cont->asic_buff->out_buffer_i;
  args->prev_index = dec_cont->asic_buff->prev_out_buffer_i;
  args->fifo_out = dec_cont->fifo_out;
  args->show_existing_frame = dec_cont->decoder.show_existing_frame;
  args->pic.num_tile_columns = (1 << dec_cont->decoder.log2_tile_columns);

  if (dec_cont->decoder.show_existing_frame) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
      args->pic = dec_cont->asic_buff->picture_info[args->index];
    else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
      args->pic = dec_cont->asic_buff->picture_info[dec_cont->asic_buff->pp_buffer_map[args->index]];
    args->pic.decode_id = pic_id;
    args->pic.is_intra_frame = 0;
    args->show_frame = 1;
    //args->display_number = dec_cont->display_number++;
    args->pic.display_id = args->display_number;
    return;
  }
  args->show_frame = dec_cont->decoder.show_frame;
  if (args->show_frame) {
    //args->display_number = dec_cont->display_number++;
  }
  /* Fill in the picture information for everything we know. */
  args->pic.display_id = args->display_number;
  args->pic.is_intra_frame = dec_cont->decoder.key_frame || dec_cont->decoder.intra_only;
  args->pic.is_golden_frame = 0;
  args->refresh_frame_flags = dec_cont->decoder.refresh_frame_flags;
  args->reset_frame_flags = dec_cont->decoder.reset_frame_flags;
  /* Frame size and format information. */
  args->pic.frame_width = NEXT_MULTIPLE(dec_cont->width, 8);
  args->pic.frame_height = NEXT_MULTIPLE(dec_cont->height, 8);
  args->pic.coded_width = dec_cont->width;
  args->pic.coded_height = dec_cont->height;
  args->pic.bit_depth_luma = args->pic.bit_depth_chroma = bit_depth;
  //args->pic.pic_stride = args->pic.frame_width * bit_depth / 8;
  args->pic.ref_pic_stride = dec_cont->asic_buff->out_y_stride[args->index];
  args->pic.ref_pic_ch_stride = dec_cont->asic_buff->out_c_stride[args->index];
  pp_index = dec_cont->asic_buff->pp_buffer_map[args->index];

  if (dec_cont->pp_enabled) {
    u32 pp_out_ctrl = dec_cont->asic_buff->pp_out_ctrl[pp_index];
    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;

      if (dec_cont->ppu_cfg[i].crop2.enabled) {
        args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].crop2.width;
        args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].crop2.height;
      } else {
        args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].scale.width;
        args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].scale.height;
        if (!dec_cont->ppu_cfg[i].crop.set_by_user) {
          if (((dec_cont->width + 1) & ~0x1) == dec_cont->ppu_cfg[i].scale.width)
            args->pic.pictures[i].frame_width = dec_cont->width;
          if (((dec_cont->height + 1) & ~0x1) == dec_cont->ppu_cfg[i].scale.height)
            args->pic.pictures[i].frame_height = dec_cont->height;
        }
      }

      //args->pic.pic_stride = NEXT_MULTIPLE((decoded_width >> dec_cont->down_scale_x_shift) * bit_depth, 16 * 8) / 8;
      args->pic.pictures[i].pic_stride = dec_cont->asic_buff->ds_stride[args->index][i];
      args->pic.pictures[i].pic_stride_ch = dec_cont->asic_buff->ds_stride_ch[args->index][i];
      args->pic.pictures[i].output_luma_base =
        (u32*)((addr_t)dec_cont->asic_buff->pp_pictures[pp_index].virtual_address + ppu_cfg->luma_offset);
      args->pic.pictures[i].output_luma_bus_address =
        dec_cont->asic_buff->pp_pictures[pp_index].bus_address + ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome) {
        args->pic.pictures[i].output_chroma_base =
          (u32*)((addr_t)dec_cont->asic_buff->pp_pictures[pp_index].virtual_address + ppu_cfg->chroma_offset);
        args->pic.pictures[i].output_chroma_bus_address =
          dec_cont->asic_buff->pp_pictures[pp_index].bus_address + ppu_cfg->chroma_offset;
      } else {
        args->pic.pictures[i].output_chroma_base = NULL;
        args->pic.pictures[i].output_chroma_bus_address = 0;
      }
      args->pic.pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      args->pic.pictures[i].out_bit_depth = ppu_cfg->pixel_width;
      if(ppu_cfg->dec400_enabled)
        PpFillDec400TblInfo(ppu_cfg,
                            dec_cont->asic_buff->pp_pictures[pp_index].virtual_address,
                            dec_cont->asic_buff->pp_pictures[pp_index].bus_address,
                            &args->pic.pictures[i].dec400_luma_table,
                            &args->pic.pictures[i].dec400_chroma_table);
    }
  } else {
    if (dec_cont->use_video_compressor) {
      args->pic.pictures[0].output_format = DEC_OUT_FRM_RFC;
      if (hw_feature->rfc_support) {
        args->pic.pictures[0].pic_stride = NEXT_MULTIPLE(8 * NEXT_MULTIPLE(dec_cont->width, 8) * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
        args->pic.pictures[0].pic_stride_ch = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(dec_cont->width, 16) * bit_depth,
                                            ALIGN(dec_cont->align) * 8) / 8;
      } else {
        args->pic.pictures[0].pic_stride = dec_cont->asic_buff->out_y_stride[args->index];
        args->pic.pictures[0].pic_stride_ch = dec_cont->asic_buff->out_c_stride[args->index];
      }
    } else {
      #ifdef TILE_8x8
        args->pic.pictures[0].output_format = bit_depth == 8 ? DEC_OUT_FRM_YUV420TILE8x8 : DEC_OUT_FRM_YUV420TILE8x8_PACK10;
      #else
        args->pic.pictures[0].output_format = bit_depth == 8 ? DEC_OUT_FRM_YUV420TILE : DEC_OUT_FRM_YUV420TILE_PACK10;
      #endif
      args->pic.pictures[0].pic_stride = dec_cont->asic_buff->out_y_stride[args->index];
      args->pic.pictures[0].pic_stride_ch = dec_cont->asic_buff->out_c_stride[args->index];
    }
    args->pic.pictures[0].frame_width = args->pic.frame_width;
    args->pic.pictures[0].frame_height = args->pic.frame_height;
  }
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    args->pic.pictures[0].output_luma_base =
      dec_cont->asic_buff->pictures[args->index].virtual_address;
    args->pic.pictures[0].output_luma_bus_address =
      dec_cont->asic_buff->pictures[args->index].bus_address;
    args->pic.pictures[0].output_chroma_base =
      dec_cont->asic_buff->pictures[args->index].virtual_address + dec_cont->asic_buff->pictures_c_offset[args->index] / 4;
    args->pic.pictures[0].output_chroma_bus_address =
      dec_cont->asic_buff->pictures[args->index].bus_address + dec_cont->asic_buff->pictures_c_offset[args->index];

    if (dec_cont->use_video_compressor) {
      /* Compression table info. */
      args->pic.output_rfc_luma_base =
        dec_cont->asic_buff->pictures[args->index].virtual_address + dec_cont->asic_buff->cbs_y_tbl_offset [args->index] / 4;
      args->pic.output_rfc_luma_bus_address =
        dec_cont->asic_buff->pictures[args->index].bus_address + dec_cont->asic_buff->cbs_y_tbl_offset [args->index];
      args->pic.output_rfc_chroma_base =
        dec_cont->asic_buff->pictures[args->index].virtual_address + dec_cont->asic_buff->cbs_c_tbl_offset [args->index] / 4;
      args->pic.output_rfc_chroma_bus_address =
        dec_cont->asic_buff->pictures[args->index].bus_address + dec_cont->asic_buff->cbs_c_tbl_offset [args->index];
    }
  }

  if (!dec_cont->pp_enabled)
    args->pic.pictures[0].out_bit_depth  = bit_depth;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
    args->pic.pictures[0].out_bit_depth  = bit_depth;
  args->pic.use_video_compressor = dec_cont->use_video_compressor;

  /* Finally, set the information we don't know yet to 0. */
  /* To be set after decoding. */
  args->pic.error_ratio = 0;
  args->pic.error_info = DEC_NO_ERROR;
  args->pic.pic_id = pic_id;
  args->pic.decode_id = pic_id;

  if (dec_cont->b_mc) {
    if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only) {
      for (i = 0; i < VP9_ACTIVE_REFS; i++) {
        args->index_ref[i] = Vp9BufferQueueGetRef(dec_cont->bq, dec_cont->decoder.active_ref_idx[i]);
      }
    }
    args->p_ref_status = (u8 *)dec_cont->asic_buff->dpb_parasitic_buf[args->index].virtual_address +
                               dec_cont->asic_buff->sync_mc_offset[args->index];
  }
}

i32 Vp9ProcessAsicStatus(struct Vp9DecContainer *dec_cont, u32 asic_status) {
  /* Handle system error situations */
  if (asic_status == VP9HWDEC_SYSTEM_TIMEOUT) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    /* This timeout is DWL(software/os) generated */
    return DEC_HW_TIMEOUT;
  } else if (asic_status == VP9HWDEC_SYSTEM_ERROR) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_SYSTEM_ERROR;
  } else if (asic_status == VP9HWDEC_FATAL_SYSTEM_ERROR) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_FATAL_SYSTEM_ERROR;
  } else if (asic_status == VP9HWDEC_HW_RESERVED) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_HW_RESERVED;
  }

  /* Handle possible common HW error situations */
  if (asic_status & DEC_HW_IRQ_BUS) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_HW_BUS_ERROR;
  } else if (asic_status &  DEC_HW_IRQ_EXT_TIMEOUT) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_HW_EXT_TIMEOUT;
  }

  /* for all the rest we will output a picture (concealed or not) */
  if ((asic_status & DEC_HW_IRQ_TIMEOUT) || (asic_status & DEC_HW_IRQ_ERROR) ||
      (asic_status & DEC_HW_IRQ_ASO) /* to signal lost residual */) {
    dec_cont->error_info = DEC_FRAME_ERROR;

    /* This timeout is HW generated */
    if (asic_status & DEC_HW_IRQ_TIMEOUT) {
#ifdef VP9HWTIMEOUT_ASSERT
      ASSERT(0);
#endif
      APITRACEDEBUG("%s","IRQ: HW TIMEOUT\n");
    } else {
      APITRACEERR("%s","IRQ: STREAM ERROR\n");
    }
  } else if (asic_status & DEC_HW_IRQ_RDY) {
    dec_cont->error_info = DEC_NO_ERROR;
    APITRACEDEBUG("%s","IRQ: PICTURE RDY\n");

    if (dec_cont->decoder.key_frame || dec_cont->error_policy & DEC_EC_NO_SKIP) {
      dec_cont->picture_broken = HANTRO_FALSE;
    }
  } else if (asic_status & DEC_HW_IRQ_EXT_TIMEOUT) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    APITRACEDEBUG("%s","IRQ: EXT_TIMEOUT\n");
  } else {
    dec_cont->error_info = DEC_FRAME_ERROR;
    ASSERT(0);
  }

  return DEC_OK;
}

i32 VP9SyncAndOutput(struct Vp9DecContainer *dec_cont) {
  i32 ret = 0;
  u32 asic_status = 0;
  u32 asic_running = 0, tmp = 0;
  /* aliases */
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  const struct DecHwFeatures *hw_feature = dec_cont->hw_feature;

  /* If hw was running, sync with hw and output picture */
  asic_running = dec_cont->asic_running;
  if (dec_cont->asic_running) {
    asic_status = Vp9AsicSync(dec_cont);

    /* (JZQ)EC Unify: for test. */
    // if (dec_cont->pic_number == 2) asic_status = DEC_HW_IRQ_ERROR;
    /* Handle asic return status */
    ret = Vp9ProcessAsicStatus(dec_cont, asic_status);
    if (ret) return ret;
  }

  /* output pic. */
  {
    /* check error_info of ref_list */
    if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only){
      u32 i = 0;
      u32 ref_frame_idx = 0;
      struct Vp9Decoder *dec = &dec_cont->decoder;
      for (i = 0; i < VP9_ACTIVE_REFS; i++) {
        if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
          ref_frame_idx = Vp9BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
        } else {
          ref_frame_idx = Vp9BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
        }
        if(dec_cont->asic_buff->picture_info[ref_frame_idx].error_info != DEC_NO_ERROR) {
          dec_cont->error_info |= DEC_REF_ERROR;
        }
      }
    }

    /* Adapt probabilities */
    /* TODO should this be done after error handling? */
    if (!hw_feature->vp9_hw_prob_support || (hw_feature->vp9_hw_prob_support && dec_cont->b_mc)) {
      Vp9UpdateProbabilities(dec_cont);
    } else if (hw_feature->vp9_hw_prob_support && !dec_cont->b_mc) {
      if (asic_status == 2 && dec_cont->decoder.refresh_entropy_probs){
        tmp = dec_cont->decoder.probs[dec_cont->decoder.frame_context_idx];
        dec_cont->decoder.probs[dec_cont->decoder.frame_context_idx] = dec_cont->decoder.probs[4];
        dec_cont->decoder.probs[4] = tmp;
      }
    }
    /* Update reference frame flags */
    Vp9UpdateRefs(dec_cont);
    /* Update output pic info. */
    Vp9PicToOutput(dec_cont, asic_running);

    /* Store prev out info */
    if (dec_cont->error_info == DEC_NO_ERROR ||
        dec_cont->error_info == DEC_REF_ERROR) {
      // if (dec_cont->error_info) Vp9ConstantConcealment(dec_cont, 128);
      // asic_buff->prev_out_buffer_i = asic_buff->out_buffer_i;
    } else {
      dec_cont->picture_broken = HANTRO_TRUE;
      /*now dec_cont->display_number++ is put in the function Vp9PicToOutput(), and this function will be called allways if one frame done,
        but when return error interrupt, it will not call Vp9PicToOutput to push into the queue, so no need to decrease 1 to keep same with pic_number
      if (dec_cont->decoder.show_frame)
        dec_cont->display_number--;*/
      // if ((dec_cont->decoder.error_resilient == 0 && dec_cont->decoder.frame_parallel_decoding == 0) ||
      //     (dec_cont->decoder.refresh_entropy_probs))
      //   /* current probabilities can't been used by next frame, so need discard. */
      //   dec_cont->entropy_broken = 1;
    }

    asic_buff->prev_out_buffer_i = asic_buff->out_buffer_i;
    asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;
    return ret;
  }
}

// void Vp9ConstantConcealment(struct Vp9DecContainer *dec_cont, u8 pixel_value) {
//   struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
//   i32 index = asic_buff->out_buffer_i;

//   dec_cont->picture_broken = HANTRO_TRUE;
//   // Size of picture (luma & chroma) is just the offset of dir mv,
//   // which is stored next to picture buffer.
//   DWLPrivateAreaMemset(asic_buff->pictures[index].virtual_address, pixel_value,
//                        asic_buff->dir_mvs_offset[index]);

//   if (dec_cont->pp_enabled) {
//     DWLPrivateAreaMemset(asic_buff->pp_pictures[index].virtual_address, pixel_value,
//                          asic_buff->pp_pictures[index].size);
//   }
// }

void Vp9EnterAbortState(struct Vp9DecContainer *dec_cont) {
  Vp9BufferQueueSetAbort(dec_cont->pp_bq);
  Vp9BufferQueueSetAbort(dec_cont->bq);
  FifoSetAbort(dec_cont->fifo_out);
  FifoSetAbort(dec_cont->fifo_display);
  dec_cont->abort = 1;
}

void Vp9ExistAbortState(struct Vp9DecContainer *dec_cont) {
  Vp9BufferQueueClearAbort(dec_cont->pp_bq);
  Vp9BufferQueueClearAbort(dec_cont->bq);
  FifoClearAbort(dec_cont->fifo_out);
  FifoClearAbort(dec_cont->fifo_display);
  dec_cont->abort = 0;
}


void Vp9EmptyBufferQueue(struct Vp9DecContainer *dec_cont) {
#ifdef USE_OMXIL_BUFFER
  u32 i;

  for (i = 0; i < dec_cont->num_buffers; i++) {
    Vp9BufferQueueEmptyRef(dec_cont->bq, i);
    dec_cont->asic_buff->display_index[i] = 0;
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    for (i = 0; i < dec_cont->num_pp_buffers; i++) {
      Vp9BufferQueueEmptyRef(dec_cont->pp_bq, i);
      dec_cont->asic_buff->display_index[i] = 0;
    }
  }
#endif
}

void Vp9ResetDecState(struct Vp9DecContainer *dec_cont) {
  dec_cont->dec_stat = VP9DEC_INITIALIZED;
  dec_cont->add_buffer = 0;
  dec_cont->out_count = 0;
  dec_cont->active_segment_map = 0;
#ifdef USE_OMXIL_BUFFER
  dec_cont->buffer_index = 0;
  dec_cont->buf_num = dec_cont->min_buffer_num;
  dec_cont->buffer_num_added = 0;
#endif
  dec_cont->error_info = DEC_NO_ERROR;
  dec_cont->picture_broken = HANTRO_FALSE;
  dec_cont->display_number = 1;
  dec_cont->pic_number = 1;
  dec_cont->intra_only = 0;
  dec_cont->conceal = 0;
  dec_cont->prev_is_key = 0;
  dec_cont->prob_refresh_detected = 0;
  // dec_cont->entropy_broken = 0;
  struct DWLLinearMem entropy_last_bak = dec_cont->decoder.entropy_last;
  DWLmemset(&dec_cont->decoder, 0, sizeof(struct Vp9Decoder));
  DWLmemset(&dec_cont->bc, 0, sizeof(struct VpBoolCoder));
  dec_cont->decoder.entropy_last = entropy_last_bak;
  Vp9AsicReset(dec_cont);
  DWLmemset(&dec_cont->pic_callback_arg, 0, sizeof(struct PicCallbackArg));
  if (dec_cont->fifo_out) FifoRelease(dec_cont->fifo_out);
  if (dec_cont->fifo_display) FifoRelease(dec_cont->fifo_display);
  FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_out);
  FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_display);
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->bq && IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
     dec_cont->num_buffers = dec_cont->num_buffers_reserved;
     Vp9BufferQueueRelease(dec_cont->bq, 0);
     dec_cont->bq = Vp9BufferQueueInitialize(dec_cont->num_buffers);
  }
#endif

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
#ifdef USE_OMXIL_BUFFER
    dec_cont->num_pp_buffers = 0;
#endif
    if (dec_cont->pp_bq) {
      Vp9BufferQueueReset(dec_cont->pp_bq);
    }
  }

  dec_cont->asic_buff->out_buffer_i = EMPTY_MARKER;
  dec_cont->asic_buff->out_pp_buffer_i = EMPTY_MARKER;
  dec_cont->no_decoding_buffer = 0;
}

enum DecRet Vp9DecAbort(Vp9DecInst dec_inst) {
  struct Vp9DecContainer *dec_cont;
  enum FifoRet ret;
  FifoObject tmp;
  BufferQueue queue;
  FifoInst fifo;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  dec_cont = (struct Vp9DecContainer *)dec_inst;
  fifo = dec_cont->fifo_display;

  pthread_mutex_lock(&dec_cont->protect_mutex);

#ifndef MULTI_THREAD_OUTPUT
  /* Before entering abort, remove all the pending output buffer from output/display fifo,
     since after Abort, FifoPop always returns FIFO_ABORT. */
  queue = IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) ? dec_cont->bq : dec_cont->pp_bq;

  while (1) {
    i32 i;
    ret = FifoPop(fifo, &tmp, FIFO_EXCEPTION_ENABLE);
    if (ret != FIFO_OK) {
      if (fifo == dec_cont->fifo_display) {
        fifo = dec_cont->fifo_out;
        continue;
      } else break;
    }
    i = (i32)((addr_t)tmp);

    Vp9BufferQueueRemoveRef(queue, i);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[i] = 0;
    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }
#endif

  /* Abort frame buffer waiting and rs/ds buffer waiting */
  Vp9EnterAbortState(dec_cont);

  if (dec_cont->no_decoding_buffer) {
    /* Release the buffer that have been got from buffer queue, but not ready for decoding. */
    if (dec_cont->bq && dec_cont->asic_buff->out_buffer_i >= 0) {
      Vp9BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
    }
    if (dec_cont->pp_bq && dec_cont->asic_buff->out_pp_buffer_i >= 0) {
      Vp9BufferQueueRemoveRef(dec_cont->pp_bq, dec_cont->asic_buff->out_pp_buffer_i);
    }
  }

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return DEC_OK;
}

enum DecRet Vp9DecAbortAfter(Vp9DecInst dec_inst) {
  struct Vp9DecContainer *dec_cont = (struct Vp9DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If a normal EOS is waited, return directly */
  if (dec_cont->dec_stat == VP9DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return DEC_OK;
  }
#endif

  /* If hw was running, stop and release hw */
  if (dec_cont->asic_running) {
    Vp9AsicSync(dec_cont);
    /* Remove the last picture that is being decoded when aborting. */
    struct PicCallbackArg *info = &dec_cont->pic_callback_arg;
    u32 ref_index = info->index;
    u32 pp_index;
    Vp9BufferQueueRemoveRef(dec_cont->bq, ref_index);
    pp_index = dec_cont->asic_buff->pp_buffer_map[ref_index];
    if (dec_cont->pp_bq)
      Vp9BufferQueueRemoveRef(dec_cont->pp_bq, pp_index);
  }
#if 0
  /* Stop and release HW */
  (void)VP9SyncAndOutput(dec_cont);

  /* If buffer queue has been already initialized, we can use it to track
   * pending cores and outputs safely. */
  if (dec_cont->bq) {
    /* if the references and queue were already flushed, cannot
     * do it again. */
    if (dec_cont->asic_buff->out_buffer_i != VP9_UNDEFINED_BUFFER &&
        dec_cont->asic_buff->out_buffer_i != ABORT_MARKER) {
      u32 i = 0;
      /* Workaround for ref counting since this buffer is never used. */
      Vp9BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
      dec_cont->asic_buff->out_buffer_i = VP9_UNDEFINED_BUFFER;

      for (i = 0; i < dec_cont->num_buffers; i++) {
        Vp9BufferQueueRemoveRef(dec_cont->bq,
                                Vp9BufferQueueGetRef(dec_cont->bq, i));
      }
    }
  }
#endif
  /* Clear reference count in buffer queue */
  Vp9EmptyBufferQueue(dec_cont);

  Vp9ResetDecState(dec_cont);

  /* Exist abort state */
  Vp9ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return DEC_OK;
}

/* complete output pic EC policy : decision pic output */
enum DecRet Vp9ECDecisionOutput(struct Vp9DecContainer *dec_cont, struct Vp9DecPicture *output) {
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
    return Vp9DecPictureConsumed((void*)dec_cont, output);
  }

  return DEC_PIC_RDY;
}
