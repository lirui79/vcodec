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
#include "av1hwd_asic.h"
#include "av1hwd_container.h"
#include "av1hwd_output.h"
#include "basetype.h"
#include "decapicommon.h"
#include "errorhandling.h"
#include "dwl.h"
#include "fifo.h"
#include "regdrv.h"
#include "dec_log.h"

#define EOS_MARKER (-1)

static u32 CycleCount(struct Av1DecContainer *dec_cont);
static u32 GetErrorRatio(struct Av1DecContainer *dec_cont, u32 asic_running);
#ifdef FPGA_PERF_AND_BW
static u32 BitCount(struct Av1DecContainer *dec_cont);
static u32 BwCount(struct Av1DecContainer *dec_cont, u32 num);
#endif
static i32 FindIndex(struct Av1DecContainer *dec_cont, DWLMemAddr address,
                     u32 buffer_type);
static i32 NextOutput(struct Av1DecContainer *dec_cont);
static i32 Av1ProcessAsicStatus(struct Av1DecContainer *dec_cont, u32 asic_status);
// static void Av1ConstantConcealment(struct Av1DecContainer *dec_cont, u8 value);
static enum DecRet Av1ECDecisionOutput(struct Av1DecContainer *dec_cont, struct Av1DecPicture *output);

#ifdef FPGA_PERF_AND_BW
u32 av1_pic_num = 0;
#endif

u32 CycleCount(struct Av1DecContainer *dec_cont) {
  u32 cycles = 0;
  u32 mbs = (NEXT_MULTIPLE(dec_cont->height, 16) *
             NEXT_MULTIPLE(dec_cont->width, 16)) >>
            8;
  if (mbs) {
    if (dec_cont->use_multicore)
      cycles = dec_cont->cycle_sum / mbs;
    else
      cycles = GetDecRegister(dec_cont->av1_regs[0], HWIF_PERF_CYCLE_COUNT) / mbs;
  }

  return cycles;
}
u32 GetErrorRatio(struct Av1DecContainer *dec_cont, u32 asic_running) {
  u32 num_err_sbs = 0;
  u32 error_ratio = 0;
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct ErrorPicInfo error_pic_info;
  u32 sb_size = dec->sb_size ? 128 : 64;
  if (dec_cont->error_info == DEC_NO_ERROR ||
      dec_cont->error_info == DEC_REF_ERROR) {
    return 0;
  } else {
    if (!asic_running) {
      return 1 * EC_ROUND_COEFF;
    }
    error_pic_info.error_x = GetDecRegister(dec_cont->av1_regs[0], HWIF_MB_LOCATION_X);
    error_pic_info.error_y = GetDecRegister(dec_cont->av1_regs[0], HWIF_MB_LOCATION_Y);
    if (dec->log2_tile_columns || dec->log2_tile_rows) {
      error_pic_info.tile_info.num_tile_rows    = dec->av1_tile_rows;
      error_pic_info.tile_info.num_tile_columns = dec->av1_tile_cols;
      error_pic_info.tile_info.col_width  = dec->tile_col_width_sb;
      error_pic_info.tile_info.row_height = dec->tile_row_height_sb;
    } else {
      error_pic_info.tile_info.num_tile_rows    = 1;
      error_pic_info.tile_info.num_tile_columns = 1;
    }
    error_pic_info.pic_width_in_ctb = (dec->width + sb_size - 1) / sb_size;
    error_pic_info.pic_height_in_ctb = (dec->height + sb_size - 1) / sb_size;
    error_pic_info.log2_ctb_size = dec->sb_size ? 7 : 6;
    num_err_sbs = GetErrorCtbCount(&error_pic_info);
    /* the error can be ignored. */
    if (num_err_sbs == 0)
      dec_cont->error_info = DEC_NO_ERROR;
  }
  error_ratio = num_err_sbs * EC_ROUND_COEFF /
                (((dec->width + sb_size - 1) / sb_size) * ((dec->height + sb_size - 1) / sb_size));
  /* (JZQ)EC Unify: for test. */
  // if (error_ratio != 0) error_ratio = 5000;

  return error_ratio;
}
#ifdef FPGA_PERF_AND_BW
u32 BitCount(struct Av1DecContainer *dec_cont){
  u32 bitrate = ((60 * GetDecRegister(dec_cont->av1_regs[0], HWIF_STREAM_LEN) * 8/ NEXT_MULTIPLE(dec_cont->height, 16)) * 3840/ NEXT_MULTIPLE(dec_cont->width, 16)) * 2160;
  return bitrate;
}

u32 BwCount(struct Av1DecContainer *dec_cont, u32 num){
  u64 bw_rd_wr = DWLReadBw(dec_cont->dwl, dec_cont->core_id, num);
  if (dec_cont->decoder.bit_depth == 10)
    bw_rd_wr *= 0.8;
  u32 out_val = bw_rd_wr * 16 * 1000/ (NEXT_MULTIPLE(dec_cont->height, 16) * NEXT_MULTIPLE(dec_cont->width, 16));
  return out_val;
}
#endif
i32 FindIndex(struct Av1DecContainer *dec_cont, DWLMemAddr address,
              u32 buffer_type) {
  i32 i = 0;
  struct DWLLinearMem *pictures = NULL;
  i32 num_buffers = 0;

  if (buffer_type == REFERENCE_BUFFER) {
    pictures = dec_cont->asic_buff->pictures;
    num_buffers = MIN(MAX_PIC_BUFFERS, dec_cont->num_buffers);
  } else if (buffer_type == DOWNSCALE_OUT_BUFFER) {
    pictures = dec_cont->asic_buff->pp_pictures;
    num_buffers = MIN(MAX_PIC_BUFFERS, dec_cont->num_pp_buffers);
  }

  for (i = 0; i < (i32)num_buffers; i++)
    if (DWL_DEVMEM_IN_RANGE(*(pictures + i), address)) break;
  ASSERT((u32)i < num_buffers);
  return i;
}

i32 NextOutput(struct Av1DecContainer *dec_cont) {
  i32 disp;
  size_t i;
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

      disp = dec_cont->t_fifo_out_disp_num[dec_cont->t_fifo_out_disp_rd];
      dec_cont->t_fifo_out_disp_rd =
          (dec_cont->t_fifo_out_disp_rd + 1) % MAX_PIC_BUFFERS;
      if (disp == (int)dec_cont->pic_number) {
        /*  fifo_out had the right output. */
        output_i = i;
      } else {
        /* Until we get the next picture in display order, push the outputs
         * to the display reordering fifo */
        FifoPush(dec_cont->fifo_display, (void *)i, FIFO_EXCEPTION_DISABLE);
      }
    } else
      return ABORT_MARKER;
  }

  return output_i;
}

enum DecRet Av1DecPictureConsumed(Av1DecInst dec_inst,
                                  const struct Av1DecPicture *picture) {
  if (dec_inst == NULL || picture == NULL) {
    return DEC_PARAM_ERROR;
  }
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
  struct Av1DecPicture pic = *picture;
  u32 buffer, i;
  DWLMemAddr output_picture = (DWLMemAddr)0;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;

  /* Get the buffer pointer to be consumed */
  if (!dec_cont->pp_enabled) {
    output_picture = (DWLMemAddr)pic.pictures[0].output_luma_bus_address;
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

  /* For dscale output buffer, return it to input buffer queue. */
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
    buffer = FindIndex(dec_cont, output_picture, DOWNSCALE_OUT_BUFFER);

    if (buffer >= MAX_PIC_BUFFERS)
		return DEC_PARAM_ERROR;

    Av1BufferQueueRemoveRef(dec_cont->pp_bq, buffer);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A
    // buffer can be in the output queue once at a time.
    dec_cont->asic_buff->display_index[buffer] = 0;

    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    u32 buffer = FindIndex(dec_cont, output_picture, REFERENCE_BUFFER);

    if (buffer >= MAX_PIC_BUFFERS)
		return DEC_PARAM_ERROR;

    /* Remove the reference to the buffer. */
    Av1BufferQueueRemoveRef(dec_cont->bq, buffer);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A
    // buffer can be in the output queue once at a time.
    dec_cont->asic_buff->display_index[buffer] = 0;

    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }

#ifdef SUPPORT_METADATA
  if (picture->metadata_param)
    picture->metadata_param->metadata_status = METADATA_UNUSED;
#endif

  return DEC_OK;
}

enum DecRet Av1DecNextPicture(Av1DecInst dec_inst,
                              struct Av1DecPicture *output) {
  i32 i;
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;
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


  if (!dec_cont->pp_enabled)
    ASSERT(i >= 0 && (u32)i < dec_cont->num_buffers);
  else
    ASSERT(i >= 0 && (u32)i < dec_cont->num_pp_buffers);

  *output = dec_cont->asic_buff->picture_info[i];
  dec_cont->pic_number++;
  /* complete output pic EC policy */
  /* decision pic output */
  if (Av1ECDecisionOutput(dec_cont, output) != DEC_PIC_RDY)
    return DEC_DISCARD_INTERNAL;

  return DEC_PIC_RDY;
}

enum DecRet Av1DecEndOfStream(Av1DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;

  /* Don't do end of stream twice. This is not thread-safe, so it must be
   * called from the single input thread that is also used to call
   * Av1DecDecode. */
  if (dec_cont->dec_stat == AV1DEC_END_OF_STREAM) {
    ASSERT(0); /* Let the assert kill the stuff in debug mode */
    return DEC_END_OF_STREAM;
  }

  if (dec_cont->asic_running)
    Av1SyncAndOutput(dec_cont);

  /* If buffer queue has been already initialized, we can use it to track
   * pending cores and outputs safely. */
  if (dec_cont->bq) {
    /* if the references and queue were already flushed, cannot
     * do it again. */
    if (dec_cont->asic_buff->out_buffer_i != AV1_UNDEFINED_BUFFER &&
        dec_cont->asic_buff->out_buffer_i != EMPTY_MARKER) {
      u32 i = 0;
      /* Workaround for ref counting since this buffer is never used. */
      Av1BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
      dec_cont->asic_buff->out_buffer_i = AV1_UNDEFINED_BUFFER;

      for (i = 0; i < dec_cont->num_buffers; i++) {
        i32 ref = Av1BufferQueueGetRef(dec_cont->bq, i);
        if (ref != AV1_UNDEFINED_BUFFER)
          Av1BufferQueueRemoveRef(dec_cont->bq, ref);
      }
    }
  }
  dec_cont->t_fifo_out_disp_wr =
      (dec_cont->t_fifo_out_disp_wr + 1) % MAX_PIC_BUFFERS;
  FifoPush(dec_cont->fifo_out, (void *)(size_t)EOS_MARKER,
           FIFO_EXCEPTION_DISABLE);
  return DEC_OK;
}

void Av1PicToOutput(struct Av1DecContainer *dec_cont, u32 asic_running) {
  struct PicCallbackArg info = dec_cont->pic_callback_arg;

#ifdef FPGA_PERF_AND_BW
  char* pic_types[] = {"        IDR", "Non-IDR (P)"};
#endif
  u32 ref_index = info.index;

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
    info.index = dec_cont->asic_buff->pp_buffer_map[info.index];
#ifdef USE_PICTURE_DISCARD
  if (!dec_cont->asic_buff->first_show[ref_index])
#endif
  {
    pthread_mutex_lock(&dec_cont->sync_out);
    while (dec_cont->asic_buff->display_index[info.index])
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
    info.pic.cycles_per_mb = CycleCount(dec_cont);
    info.pic.error_ratio = GetErrorRatio(dec_cont, asic_running);
    info.pic.error_info = dec_cont->error_info;
#ifdef FPGA_PERF_AND_BW
    info.pic.bitrate_4k60fps = BitCount(dec_cont);
    info.pic.bwrd_in_fs = BwCount(dec_cont, 0);
    info.pic.bwwr_in_fs = BwCount(dec_cont, 1);
    if (dec_cont->decoder.show_existing_frame == 0) {
      printf("PERF_PIC %2u, type %s, %4u cycles / mb, %4u Mbps (4k@60fps), BW R/W: %0.3f/%0.3f (x FRAME 4k@60fps)\n",
	     av1_pic_num++,
	     pic_types[info.pic.is_intra_frame ? DEC_PIC_TYPE_I :DEC_PIC_TYPE_P],
	     info.pic.cycles_per_mb,
	     info.pic.bitrate_4k60fps/ 1024/ 1024,
	     (double)info.pic.bwrd_in_fs/ 1000/ 2.157,
	     (double)info.pic.bwwr_in_fs/ 1000/ 2.157);
    }
    info.pic.bitrate_4k60fps = 0;
    info.pic.bwrd_in_fs = 0;
    info.pic.bwwr_in_fs = 0;
#endif
  }
  dec_cont->asic_buff->picture_info[info.index] = info.pic;

  if (info.show_frame) {
#ifdef USE_PICTURE_DISCARD
    if (dec_cont->asic_buff->first_show[ref_index] == 0)
#endif
    {
      dec_cont->asic_buff->display_index[info.index] = dec_cont->display_number++;
      dec_cont->t_fifo_out_disp_num[dec_cont->t_fifo_out_disp_wr] =
          dec_cont->display_number - 1;
      dec_cont->t_fifo_out_disp_wr =
          (dec_cont->t_fifo_out_disp_wr + 1) % MAX_PIC_BUFFERS;
      FifoPush(dec_cont->fifo_out, (void *)(size_t)info.index,
              FIFO_EXCEPTION_DISABLE);
      dec_cont->asic_buff->first_show[ref_index] = 1;
    }
#ifdef USE_PICTURE_DISCARD
    else {
      /* Remove the reference to the buffer. */
      if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
        Av1BufferQueueRemoveRef(dec_cont->pp_bq, info.index);
    }
#endif
    if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
      Av1BufferQueueRemoveRef(dec_cont->bq, ref_index);
  }
}

void Av1SetupPicToOutput(struct Av1DecContainer *dec_cont, u32 pic_id) {
  struct PicCallbackArg *args = &dec_cont->pic_callback_arg;
  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  u32 pp_index;
  u32 bit_depth = dec_cont->decoder.bit_depth;
  u32 i;

  DWLmemset(args, 0, sizeof(struct PicCallbackArg));
  args->index = dec_cont->asic_buff->out_buffer_i;
  args->fifo_out = dec_cont->fifo_out;
  args->show_existing_frame = dec_cont->decoder.show_existing_frame;

  if (dec_cont->decoder.show_existing_frame) {
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
      args->pic = dec_cont->asic_buff->picture_info[args->index];
    else if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER))
      args->pic = dec_cont->asic_buff->picture_info[dec_cont->asic_buff->pp_buffer_map[args->index]];
    args->pic.decode_id = pic_id;
    args->pic.is_intra_frame = 0;
    args->show_frame = 1;
    if (dec_cont->metadata_param_curr &&
        dec_cont->metadata_param_curr->decode_id == pic_id)
      args->pic.metadata_param = dec_cont->metadata_param_curr;
    return;
  }
  args->show_frame = dec_cont->decoder.show_frame;
  /* Fill in the picture information for everything we know. */
  args->pic.is_intra_frame = dec_cont->decoder.key_frame;
  args->pic.intra_only = dec_cont->decoder.intra_only;
  args->pic.is_golden_frame = 0;
  args->pic.metadata_param = dec_cont->metadata_param_curr;
  /* Frame size and format information. */
  args->pic.frame_width = NEXT_MULTIPLE(dec_cont->decoder.superres_width, 8);
  args->pic.frame_height = NEXT_MULTIPLE(dec_cont->height, 8);
  args->pic.coded_width = dec_cont->width;
  args->pic.coded_height = dec_cont->height;
  args->pic.bits_per_sample = dec_cont->decoder.bit_depth;
  args->pic.color_space = dec_cont->decoder.color_space;
  args->pic.color_range = dec_cont->decoder.color_range;
  args->pic.usr_ptr = dec_cont->usr_ptr;
  args->pic.superres_width = dec_cont->decoder.superres_width;
  pp_index = dec_cont->asic_buff->pp_buffer_map[args->index];

  args->pic.fill_chroma =
      dec_cont->decoder.monochrome && dec_cont->decoder.apply_grain;
  args->pic.apply_grain = dec_cont->decoder.apply_grain && dec_cont->pp_enabled;

  u32 pic_width = dec_cont->decoder.superres_is_scaled
                      ? dec_cont->decoder.superres_width
                      : dec_cont->decoder.width;
  u32 pic_height = dec_cont->decoder.height;
  if (dec_cont->pp_enabled) {
    u32 pp_out_ctrl = dec_cont->asic_buff->pp_out_ctrl[pp_index];

    for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled || !PP_CHANNEL_OUT_ENABLE(pp_out_ctrl, i)) continue;

#if 0
      if (!dec_cont->ppu_cfg[i].crop.set_by_user &&
          !dec_cont->ppu_cfg[i].scale.set_by_user) {
        args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].vir_left + pic_width + dec_cont->ppu_cfg[i].vir_right;
        args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].vir_top + pic_height + dec_cont->ppu_cfg[i].vir_bottom;
      } else
#endif
      {
        if (ppu_cfg->crop2.enabled) {
        args->pic.pictures[i].frame_width = (dec_cont->ppu_cfg[i].crop2.width / 2) << 1;
        args->pic.pictures[i].frame_height = (dec_cont->ppu_cfg[i].crop2.height / 2) << 1;
        } else {
          args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].vir_left + dec_cont->ppu_cfg[i].scale.width + dec_cont->ppu_cfg[i].vir_right;
          args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].vir_top + dec_cont->ppu_cfg[i].scale.height + dec_cont->ppu_cfg[i].vir_bottom;
          if (!dec_cont->ppu_cfg[i].crop.set_by_user &&
              (dec_cont->ppu_cfg[i].scale.ratio_x == 1 && dec_cont->ppu_cfg[i].scale.ratio_y == 1)) {
            if (((pic_width + 1) & ~0x1) == dec_cont->ppu_cfg[i].scale.width)
              args->pic.pictures[i].frame_width = dec_cont->ppu_cfg[i].vir_left + pic_width + dec_cont->ppu_cfg[i].vir_right;
            if (((pic_height + 1) & ~0x1) == dec_cont->ppu_cfg[i].scale.height)
              args->pic.pictures[i].frame_height = dec_cont->ppu_cfg[i].vir_top + pic_height + dec_cont->ppu_cfg[i].vir_bottom;
          }
        }
      }

      args->pic.pictures[i].pic_stride = dec_cont->ppu_cfg[i].ystride;
      args->pic.pictures[i].pic_stride_ch = dec_cont->ppu_cfg[i].cstride;
      args->pic.pictures[i].output_luma_base =
          (u32 *)((addr_t)dec_cont->asic_buff->pp_pictures[pp_index]
                      .virtual_address +
                  ppu_cfg->luma_offset);
      args->pic.pictures[i].output_luma_bus_address =
          dec_cont->asic_buff->pp_pictures[pp_index].bus_address +
          ppu_cfg->luma_offset;
      if (!ppu_cfg->monochrome && !dec_cont->decoder.monochrome) {
        args->pic.pictures[i].output_chroma_base =
            (u32 *)((addr_t)dec_cont->asic_buff->pp_pictures[pp_index]
                        .virtual_address +
                    ppu_cfg->chroma_offset);
        args->pic.pictures[i].output_chroma_bus_address =
            dec_cont->asic_buff->pp_pictures[pp_index].bus_address +
            ppu_cfg->chroma_offset;
      } else {
        args->pic.pictures[i].output_chroma_base = NULL;
        args->pic.pictures[i].output_chroma_bus_address = 0;
      }
      args->pic.pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
      args->pic.pictures[i].out_bit_depth = ppu_cfg->pixel_width;
      if (ppu_cfg->dec400_enabled) {
        PpFillDec400TblInfo(ppu_cfg,
                            dec_cont->asic_buff->pp_pictures[pp_index].virtual_address,
                            dec_cont->asic_buff->pp_pictures[pp_index].bus_address,
                            &args->pic.pictures[i].dec400_luma_table,
                            &args->pic.pictures[i].dec400_chroma_table);
      }
    }
  } else {
    args->pic.pictures[0].frame_width = args->pic.frame_width;
    args->pic.pictures[0].frame_height = args->pic.frame_height;
    if (dec_cont->use_video_compressor) {
      args->pic.pictures[0].output_format = DEC_OUT_FRM_RFC;
      args->pic.pictures[0].pic_stride =
          NEXT_MULTIPLE(
              8 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
              ALIGN(dec_cont->align) * 8) /
          8;
      args->pic.pictures[0].pic_stride_ch =
          NEXT_MULTIPLE(
              4 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
              ALIGN(dec_cont->align) * 8) /
          8;
    } else {
      #ifdef TILE_8x8
      args->pic.pictures[0].output_format =
        bit_depth == 8 ? (dec_cont->decoder.monochrome ? DEC_OUT_FRM_YUV400TILE8x8 : DEC_OUT_FRM_YUV420TILE8x8) :
                         (dec_cont->decoder.monochrome ? DEC_OUT_FRM_YUV400TILE8x8_PACK10 : DEC_OUT_FRM_YUV420TILE8x8_PACK10);
      args->pic.pictures[0].pic_stride =
        NEXT_MULTIPLE(
              8 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
              ALIGN(dec_cont->align) * 8) / 8;
      args->pic.pictures[0].pic_stride_ch =
        NEXT_MULTIPLE(
              4 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
              ALIGN(dec_cont->align) * 8) / 8;
      #else
      args->pic.pictures[0].output_format =
        bit_depth == 8 ? (dec_cont->decoder.monochrome ? DEC_OUT_FRM_YUV400TILE : DEC_OUT_FRM_YUV420TILE) :
                         (dec_cont->decoder.monochrome ? DEC_OUT_FRM_YUV400TILE_PACK10 : DEC_OUT_FRM_YUV420TILE_PACK10);
      args->pic.pictures[0].pic_stride = args->pic.pictures[0].pic_stride_ch =
        NEXT_MULTIPLE(
              4 * NEXT_MULTIPLE(dec_cont->superres_width, 8) * bit_depth,
              ALIGN(dec_cont->align) * 8) /
          8;
      #endif
    }

    args->pic.pictures[0].output_luma_base =
        dec_cont->asic_buff->pictures[args->index].virtual_address;
    args->pic.pictures[0].output_luma_bus_address =
        dec_cont->asic_buff->pictures[args->index].bus_address;
    if (dec_cont->decoder.monochrome) {
      args->pic.pictures[0].output_chroma_base = NULL;
      args->pic.pictures[0].output_chroma_bus_address = 0;
    } else {
      args->pic.pictures[0].output_chroma_base =
          dec_cont->asic_buff->pictures[args->index].virtual_address +
          dec_cont->asic_buff->pictures_c_offset[args->index] / 4;
      args->pic.pictures[0].output_chroma_bus_address =
          dec_cont->asic_buff->pictures[args->index].bus_address +
          dec_cont->asic_buff->pictures_c_offset[args->index];
    }

    if (dec_cont->use_video_compressor) {
      /* Compression table info. */
      args->pic.output_rfc_luma_base =
          dec_cont->asic_buff->pictures[args->index].virtual_address +
          dec_cont->asic_buff->cbs_y_tbl_offset[args->index] / 4;
      args->pic.output_rfc_luma_bus_address =
          dec_cont->asic_buff->pictures[args->index].bus_address +
          dec_cont->asic_buff->cbs_y_tbl_offset[args->index];

      if (dec_cont->decoder.monochrome) {
        args->pic.output_rfc_chroma_base = NULL;
        args->pic.output_rfc_chroma_bus_address = 0;
      } else {
        args->pic.output_rfc_chroma_base =
            dec_cont->asic_buff->pictures[args->index].virtual_address +
            dec_cont->asic_buff->cbs_c_tbl_offset[args->index] / 4;
        args->pic.output_rfc_chroma_bus_address =
            dec_cont->asic_buff->pictures[args->index].bus_address +
            dec_cont->asic_buff->cbs_c_tbl_offset[args->index];
      }
    }
  }

  /* Finally, set the information we don't know yet to 0. */
  /* To be set after decoding. */
  args->pic.error_ratio = 0;
  args->pic.error_info = DEC_NO_ERROR;
  args->pic.pic_id = pic_id;
  args->pic.decode_id = pic_id;
  //  args->pic.fc_num_tiles_col = 1 <<
  //  dec_cont->asic_buff->log2_tile_columns[args->index];

  int lst2_buf_idx;
  int lst3_buf_idx;
  int gld_buf_idx;
  int alt_buf_idx;
  int lst_buf_idx;
  int bwd_buf_idx;
  int alt2_buf_idx;

  if (!dec_cont->pp_enabled) {
    lst2_buf_idx = Av1BufferQueueGetRef(
      dec_cont->bq,
      dec_cont->decoder.active_ref_idx[LAST2_FRAME_EX - LAST_FRAME]);
    lst3_buf_idx = Av1BufferQueueGetRef(
      dec_cont->bq,
      dec_cont->decoder.active_ref_idx[LAST3_FRAME_EX - LAST_FRAME]);
    gld_buf_idx = Av1BufferQueueGetRef(
      dec_cont->bq,
      dec_cont->decoder.active_ref_idx[GOLDEN_FRAME_EX - LAST_FRAME]);
    alt_buf_idx = Av1BufferQueueGetRef(
      dec_cont->bq,
      dec_cont->decoder.active_ref_idx[ALTREF_FRAME_EX - LAST_FRAME]);
    lst_buf_idx = Av1BufferQueueGetRef(
     dec_cont->bq, dec_cont->decoder.active_ref_idx[LAST_FRAME - LAST_FRAME]);
    bwd_buf_idx = Av1BufferQueueGetRef(
      dec_cont->bq,
      dec_cont->decoder.active_ref_idx[BWDREF_FRAME_EX - LAST_FRAME]);
    alt2_buf_idx = Av1BufferQueueGetRef(
      dec_cont->bq,
      dec_cont->decoder.active_ref_idx[ALTREF2_FRAME_EX - LAST_FRAME]);
  } else {
    lst2_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq,
      dec_cont->decoder.active_ref_idx[LAST2_FRAME_EX - LAST_FRAME]);
    lst3_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq,
      dec_cont->decoder.active_ref_idx[LAST3_FRAME_EX - LAST_FRAME]);
    gld_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq,
      dec_cont->decoder.active_ref_idx[GOLDEN_FRAME_EX - LAST_FRAME]);
    alt_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq,
      dec_cont->decoder.active_ref_idx[ALTREF_FRAME_EX - LAST_FRAME]);
    lst_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq, dec_cont->decoder.active_ref_idx[LAST_FRAME - LAST_FRAME]);
    bwd_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq,
      dec_cont->decoder.active_ref_idx[BWDREF_FRAME_EX - LAST_FRAME]);
    alt2_buf_idx = Av1BufferQueueGetRef(
      dec_cont->pp_bq,
      dec_cont->decoder.active_ref_idx[ALTREF2_FRAME_EX - LAST_FRAME]);
  }

  args->pic.frame_offset = dec_cont->decoder.frame_offset;

  args->pic.lst2_frame_offset =
      dec_cont->asic_buff->picture_info[lst2_buf_idx].frame_offset;
  args->pic.lst3_frame_offset =
      dec_cont->asic_buff->picture_info[lst3_buf_idx].frame_offset;
  args->pic.gld_frame_offset =
      dec_cont->asic_buff->picture_info[gld_buf_idx].frame_offset;
  args->pic.alt_frame_offset =
      dec_cont->asic_buff->picture_info[alt_buf_idx].frame_offset;
  args->pic.lst_frame_offset =
      dec_cont->asic_buff->picture_info[lst_buf_idx].frame_offset;
  args->pic.bwd_frame_offset =
      dec_cont->asic_buff->picture_info[bwd_buf_idx].frame_offset;
  args->pic.alt2_frame_offset =
      dec_cont->asic_buff->picture_info[alt2_buf_idx].frame_offset;
}

i32 Av1ProcessAsicStatus(struct Av1DecContainer *dec_cont, u32 asic_status) {
  // TODO: when does this happen, different from timeout irq?
  if (asic_status == AV1HWDEC_SYSTEM_TIMEOUT) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    /* This timeout is DWL(software/os) generated */
    return DEC_HW_TIMEOUT;
    // TODO:
  } else if (asic_status == AV1HWDEC_SYSTEM_ERROR) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_SYSTEM_ERROR;
  } else if (asic_status == AV1HWDEC_STREAM_ERROR) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_STRM_ERROR;
  }

  /* Handle possible common HW error situations */
  if (asic_status & DEC_HW_IRQ_BUS) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_HW_BUS_ERROR;
  } else if (asic_status & DEC_HW_IRQ_EXT_TIMEOUT) {
    dec_cont->error_info = DEC_FRAME_ERROR;
    return DEC_HW_EXT_TIMEOUT;
  }

  /* for all the rest we will output a picture (concealed or not) */
  if ((asic_status & DEC_HW_IRQ_TIMEOUT) || (asic_status & DEC_HW_IRQ_ERROR)) {
    dec_cont->error_info = DEC_FRAME_ERROR;

    /* This timeout is HW generated */
    if (asic_status & DEC_HW_IRQ_TIMEOUT) {
#ifdef AV1HWTIMEOUT_ASSERT
      ASSERT(0);
#endif
      APITRACEERR("%s","IRQ: HW TIMEOUT\n");
    } else {
      APITRACEERR("%s","IRQ: STREAM ERROR\n");
    }

    // // show previous pic again, remove ref to allocated
    // if (dec_cont->asic_buff->out_buffer_i !=
    //     dec_cont->asic_buff->prev_out_buffer_i) {
    //   Av1BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
    //   Av1BufferQueueRemoveRef(dec_cont->pp_bq,
    //                           dec_cont->asic_buff->pp_buffer_map[dec_cont->asic_buff->out_buffer_i]);
    // }
    // if (dec_cont->asic_buff->prev_out_buffer_i >= 0)
    //   dec_cont->asic_buff->out_buffer_i = dec_cont->asic_buff->prev_out_buffer_i;
    // Av1SetupPicToOutput(dec_cont);
  } else if (asic_status & DEC_HW_IRQ_RDY) {
    dec_cont->error_info = DEC_NO_ERROR;
    APITRACE("%s","IRQ: PICTURE RDY\n");

    if (dec_cont->decoder.key_frame || dec_cont->error_policy & DEC_EC_NO_SKIP) {
      dec_cont->picture_broken = HANTRO_FALSE;
    }
  } else {
    dec_cont->error_info = DEC_FRAME_ERROR;
    ASSERT(0);
  }

  return DEC_OK;
}
static void assign_seq_norm_data(struct Av1Decoder *decoder) {
  int i;

  decoder->obu_seq_hdr_checked.sequencer_ok = 1;

  decoder->obu_seq_hdr_checked.vp_profile = decoder->vp_profile;

  decoder->obu_seq_hdr_checked.still_picture = decoder->still_picture;

  decoder->obu_seq_hdr_checked.reduced_still_picture_hdr =
      decoder->reduced_still_picture_hdr;

  decoder->obu_seq_hdr_checked.timing_info_present_flag =
      decoder->timing_info_present_flag;

  decoder->obu_seq_hdr_checked.num_units_in_tick = decoder->num_units_in_tick;

  decoder->obu_seq_hdr_checked.time_scale = decoder->time_scale;

  decoder->obu_seq_hdr_checked.equal_picture_interval =
      decoder->equal_picture_interval;

  decoder->obu_seq_hdr_checked.num_ticks_per_picture =
      decoder->num_ticks_per_picture;

  decoder->obu_seq_hdr_checked.decoder_model_info_present_flag =
      decoder->decoder_model_info_present_flag;

  decoder->obu_seq_hdr_checked.buffer_delay_length =
      decoder->buffer_delay_length;

  decoder->obu_seq_hdr_checked.num_units_in_decoding_tick =
      decoder->num_units_in_decoding_tick;

  decoder->obu_seq_hdr_checked.buffer_removal_time_length =
      decoder->buffer_removal_time_length;

  decoder->obu_seq_hdr_checked.frame_presentation_time_length =
      decoder->frame_presentation_time_length;

  decoder->obu_seq_hdr_checked.initial_display_delay_present_flag =
      decoder->initial_display_delay_present_flag;

  decoder->obu_seq_hdr_checked.operating_points_cnt =
      decoder->operating_points_cnt;
  for (i = 0; i < 32; i++) {
    decoder->obu_seq_hdr_checked.operating_point_idc[i] =
        decoder->operating_point_idc[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->obu_seq_hdr_checked.level[i] = decoder->level[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->obu_seq_hdr_checked.seq_tier[i] = decoder->seq_tier[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->obu_seq_hdr_checked.initial_display_delay_present[i] =
        decoder->initial_display_delay_present[i];
  }
  for (i = 0; i < 32; i++) {
    decoder->obu_seq_hdr_checked.initial_display_delay[i] =
        decoder->initial_display_delay[i];
  }

  decoder->obu_seq_hdr_checked.num_bits_w = decoder->num_bits_w;

  decoder->obu_seq_hdr_checked.num_bits_h = decoder->num_bits_h;

  decoder->obu_seq_hdr_checked.max_width = decoder->max_width;

  decoder->obu_seq_hdr_checked.max_height = decoder->max_height;

  decoder->obu_seq_hdr_checked.frame_id_numbers_present_flag =
      decoder->frame_id_numbers_present_flag;

  decoder->obu_seq_hdr_checked.delta_frame_id_length =
      decoder->delta_frame_id_length;

  decoder->obu_seq_hdr_checked.frame_id_length = decoder->frame_id_length;

  decoder->obu_seq_hdr_checked.sb_size = decoder->sb_size;

  decoder->obu_seq_hdr_checked.enable_filter_intra =
      decoder->enable_filter_intra;

  decoder->obu_seq_hdr_checked.enable_intra_edge_filter =
      decoder->enable_intra_edge_filter;

  decoder->obu_seq_hdr_checked.enable_interintra_compound =
      decoder->enable_interintra_compound;

  decoder->obu_seq_hdr_checked.enable_masked_compound =
      decoder->enable_masked_compound;

  decoder->obu_seq_hdr_checked.enable_warped_motion =
      decoder->enable_warped_motion;

  decoder->obu_seq_hdr_checked.enable_dual_filter = decoder->enable_dual_filter;

  decoder->obu_seq_hdr_checked.enable_order_hint = decoder->enable_order_hint;

  decoder->obu_seq_hdr_checked.enable_jnt_comp = decoder->enable_jnt_comp;

  decoder->obu_seq_hdr_checked.enable_ref_frame_mvs =
      decoder->enable_ref_frame_mvs;

  decoder->obu_seq_hdr_checked.force_screen_content_tools =
      decoder->force_screen_content_tools;

  decoder->obu_seq_hdr_checked.force_integer_mv = decoder->force_integer_mv;

  decoder->obu_seq_hdr_checked.order_hint_bits_minus1 =
      decoder->order_hint_bits_minus1;

  decoder->obu_seq_hdr_checked.enable_superres = decoder->enable_superres;

  decoder->obu_seq_hdr_checked.enable_cdef = decoder->enable_cdef;

  decoder->obu_seq_hdr_checked.enable_restoration = decoder->enable_restoration;

  decoder->obu_seq_hdr_checked.bit_depth = decoder->bit_depth;

  decoder->obu_seq_hdr_checked.monochrome = decoder->monochrome;

  decoder->obu_seq_hdr_checked.color_primaries = decoder->color_primaries;

  decoder->obu_seq_hdr_checked.transfer_characteristics =
      decoder->transfer_characteristics;

  decoder->obu_seq_hdr_checked.matrix_coefficients =
      decoder->matrix_coefficients;

  decoder->obu_seq_hdr_checked.color_range = decoder->color_range;

  decoder->obu_seq_hdr_checked.subsampling_x = decoder->subsampling_x;

  decoder->obu_seq_hdr_checked.subsampling_y = decoder->subsampling_y;

  decoder->obu_seq_hdr_checked.chroma_sample_position =
      decoder->chroma_sample_position;

  decoder->obu_seq_hdr_checked.separate_uv_delta_q =
      decoder->separate_uv_delta_q;

  decoder->obu_seq_hdr_checked.film_grain_params_present =
      decoder->film_grain_params_present;
}

i32 Av1SyncAndOutput(struct Av1DecContainer *dec_cont) {
  i32 ret = 0;
  u32 asic_status;
  i32 i;
  u32 asic_running = 0;
  /* aliases */
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  /* If hw was running, sync with hw and output picture */
  asic_running = dec_cont->asic_running;
  if (dec_cont->asic_running) {
    asic_status = Av1AsicSync(dec_cont); // will set dec_cont->asic_running=0

    /* (JZQ)EC Unify: for test. */
    // if (dec_cont->pic_number == 2) asic_status = DEC_HW_IRQ_ERROR;
    /* Handle asic return status */
    ret = Av1ProcessAsicStatus(dec_cont, asic_status);
    if (ret) return ret;
  }

  /* output pic. */
  {
    /* check error_info of ref_list */
    if (!dec_cont->decoder.key_frame && !dec_cont->decoder.intra_only && !dec_cont->decoder.allow_intrabc) {
      u32 i = 0;
      u32 ref_frame_idx = 0;
      for (i = 0; i < AV1_ACTIVE_REFS_EX; ++i) {
        if (!dec_cont->pp_enabled) {
          ref_frame_idx =
            Av1BufferQueueGetRef(dec_cont->bq, dec_cont->decoder.active_ref_idx[i]);
        } else {
          ref_frame_idx =
            Av1BufferQueueGetRef(dec_cont->pp_bq, dec_cont->decoder.active_ref_idx[i]);
        }
        if(dec_cont->asic_buff->picture_info[ref_frame_idx].error_info != DEC_NO_ERROR) {
          dec_cont->error_info |= DEC_REF_ERROR;
        }
      }
    }

    /* Adapt probabilities */
    /* TODO should this be done after error handling? */
    if(!dec_cont->vcmd_m2m)
      Av1UpdateProbabilities(dec_cont);

    /* Update reference frame flags */
    if (dec_cont->heif_mode) {
      if (!dec_cont->skip_no_intra)
        Av1UpdateRefs(dec_cont);
    } else {
      Av1UpdateRefs(dec_cont);
    }
    /* Update output info */
    Av1PicToOutput(dec_cont, asic_running);

    // update frame comp modes for next
    //    dec_cont->fc = FcUpdateModes(dec_cont->multi_swregs);
    if (dec_cont->decoder.key_frame && dec_cont->decoder.show_frame &&
        dec_cont->decoder.show_existing_frame == 0 &&
        dec_cont->decoder.obu_hdr.temporal_layer_id == 0) {
      // // assign variables if decoder is no problem.
      // if (!error_concealment)
      //   assign_seq_norm_data(&dec_cont->decoder);
      // else if (dec_cont->decoder.input_same_seqheadr >= 2)
      //   assign_seq_norm_data(&dec_cont->decoder);
      assign_seq_norm_data(&dec_cont->decoder);
    }
    for (i = 0; i < dec_cont->decoder.input_sequence_num; i++) {
      if (dec_cont->decoder.seq_hdr[i] != NULL)
        DWLfree(dec_cont->decoder.seq_hdr[i]);
    }
    dec_cont->decoder.input_sequence_num = 0;
    dec_cont->decoder.input_same_seqheadr = 0;

    /* Store prev out info */
    if (dec_cont->error_info == DEC_NO_ERROR ||
        dec_cont->error_info == DEC_REF_ERROR) {
      // if (error_concealment) Av1ConstantConcealment(dec_cont, 128);
      // if (dec_cont->decoder.refresh_frame_flags)
      //   asic_buff->prev_out_buffer_i = asic_buff->out_buffer_i;
    } else {
      dec_cont->picture_broken = HANTRO_TRUE;
    }
  }

  asic_buff->out_buffer_i = AV1_UNDEFINED_BUFFER;
  return 0;
}

#if 0
void Av1ConstantConcealment(struct Av1DecContainer *dec_cont, u8 pixel_value) {
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  i32 index = asic_buff->out_buffer_i;

  dec_cont->picture_broken = 1;
  // Size of picture (luma & chroma) is just the offset of dir mv,
  // which is stored next to picture buffer.
  DWLPrivateAreaMemset(asic_buff->dpb_parasitic_buf[index].virtual_address, pixel_value,
                       asic_buff->dir_mvs_offset[index]);
  if (dec_cont->pp_enabled) {
    DWLPrivateAreaMemset(asic_buff->pp_pictures[index].virtual_address,
                         pixel_value, asic_buff->pp_pictures[index].size);
  }
}
#endif

void Av1EnterAbortState(struct Av1DecContainer *dec_cont) {
  Av1BufferQueueSetAbort(dec_cont->pp_bq);
  Av1BufferQueueSetAbort(dec_cont->bq);
  FifoSetAbort(dec_cont->fifo_out);
  FifoSetAbort(dec_cont->fifo_display);
  dec_cont->abort = 1;
}

void Av1ExistAbortState(struct Av1DecContainer *dec_cont) {
  Av1BufferQueueClearAbort(dec_cont->pp_bq);
  Av1BufferQueueClearAbort(dec_cont->bq);
  FifoClearAbort(dec_cont->fifo_out);
  FifoClearAbort(dec_cont->fifo_display);
  dec_cont->abort = 0;
}

void Av1EmptyBufferQueue(struct Av1DecContainer *dec_cont) {
#ifdef USE_OMXIL_BUFFER
  u32 i;
  for (i = 0; i < dec_cont->num_buffers; i++) {
    Av1BufferQueueEmptyRef(dec_cont->bq, i);
    dec_cont->asic_buff->display_index[i] = 0;
  }

  if (dec_cont->pp_enabled) {
    for (i = 0; i < dec_cont->num_pp_buffers; i++) {
      Av1BufferQueueEmptyRef(dec_cont->pp_bq, i);
      dec_cont->asic_buff->display_index[i] = 0;
    }
  }
#endif
}

void Av1ResetDecState(struct Av1DecContainer *dec_cont) {
  dec_cont->dec_stat = AV1DEC_INITIALIZED;
  dec_cont->add_buffer = 0;
  dec_cont->out_count = 0;
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
  dec_cont->prev_is_key = 0;

  u32 oppoints_bak = dec_cont->decoder.oppoints;
  u32 tile_transpose_bak = dec_cont->decoder.tile_transpose;
  DWLmemset(&dec_cont->decoder, 0, sizeof(struct Av1Decoder));
  dec_cont->decoder.tile_transpose = tile_transpose_bak;
  dec_cont->decoder.oppoints = oppoints_bak;
  dec_cont->decoder.default_cdfs = (struct AV1CDFs *)dec_cont->asic_buff->default_cdfs_mem.virtual_address;
  dec_cont->decoder.default_cdfs_addr = dec_cont->asic_buff->default_cdfs_mem.bus_address;
  dec_cont->decoder.default_cdfs_ndvc = (struct MvCDFs *)dec_cont->asic_buff->default_cdfs_ndvc_mem.virtual_address;
  dec_cont->decoder.default_cdfs_ndvc_addr = dec_cont->asic_buff->default_cdfs_ndvc_mem.bus_address;
  dec_cont->decoder.cdfs_last = (struct AV1CDFs *)dec_cont->asic_buff->cdfs_last_mem.virtual_address;
  dec_cont->decoder.cdfs_last_addr = dec_cont->asic_buff->cdfs_last_mem.bus_address;
  AV1SetDefaultCDFs(&dec_cont->decoder);

  Av1AsicReset(dec_cont);
  DWLmemset(&dec_cont->pic_callback_arg, 0, sizeof(struct PicCallbackArg));
  if (dec_cont->fifo_out) FifoRelease(dec_cont->fifo_out);
  if (dec_cont->fifo_display) FifoRelease(dec_cont->fifo_display);
  FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_out);
  FifoInit(MAX_PIC_BUFFERS, &dec_cont->fifo_display);
#ifdef USE_OMXIL_BUFFER
  if (dec_cont->bq && !dec_cont->pp_enabled) {
     dec_cont->num_buffers = dec_cont->num_buffers_reserved;
     Av1BufferQueueRelease(dec_cont->bq, 0);
     dec_cont->bq = Av1BufferQueueInitialize(dec_cont->num_buffers);
  }
#endif

  if (dec_cont->pp_enabled) {
#ifdef USE_OMXIL_BUFFER
    dec_cont->num_pp_buffers = 0;
#endif
    if (dec_cont->pp_bq) {
      Av1BufferQueueReset(dec_cont->pp_bq);
    }
  }

  dec_cont->asic_buff->out_buffer_i = EMPTY_MARKER;
  dec_cont->asic_buff->out_pp_buffer_i = EMPTY_MARKER;
  dec_cont->no_decoding_buffer = 0;
  /* clear metadata_param */
#ifdef SUPPORT_METADATA
  u32 i = 0;
  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    if (dec_cont->metadata_param[i] && dec_cont->metadata_param[i]->metadata_status != METADATA_UNUSED) {
      dec_cont->metadata_param[i]->metadata_status = METADATA_UNUSED;
      dec_cont->metadata_param[i]->decode_id = -1;
      if (dec_cont->metadata_param[i]->t35_param.payload_byte.buffer) {
        dec_cont->metadata_param[i]->t35_param.payload_byte.available_size = 0;
        dec_cont->metadata_param[i]->t35_param.counter = 0;
      }
    }
  }
#endif
}


enum DecRet Av1DecAbort(Av1DecInst dec_inst) {
  struct Av1DecContainer *dec_cont;// = (struct Av1DecContainer *)dec_inst;
  enum FifoRet ret;
  FifoObject tmp;
  BufferQueue queue;
  FifoInst fifo;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  dec_cont = (struct Av1DecContainer *)dec_inst;
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

    Av1BufferQueueRemoveRef(queue, i);

    pthread_mutex_lock(&dec_cont->sync_out);
    // Release buffer for use as an output (i.e. "show existing frame"). A buffer can
    // be in the output queue once at a time.
    dec_cont->asic_buff->display_index[i] = 0;
    pthread_cond_signal(&dec_cont->sync_out_cv);
    pthread_mutex_unlock(&dec_cont->sync_out);
  }
#endif

  /* Abort frame buffer waiting and rs/ds buffer waiting */
  Av1EnterAbortState(dec_cont);

  if (dec_cont->no_decoding_buffer) {
    /* Release the buffer that have been got from buffer queue, but not ready for decoding. */
    if (dec_cont->bq && dec_cont->asic_buff->out_buffer_i >= 0) {
      Av1BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
    }
    if (dec_cont->pp_bq && dec_cont->asic_buff->out_pp_buffer_i >= 0) {
      Av1BufferQueueRemoveRef(dec_cont->pp_bq, dec_cont->asic_buff->out_pp_buffer_i);
    }
  }

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return DEC_OK;
}

enum DecRet Av1DecAbortAfter(Av1DecInst dec_inst) {
  struct Av1DecContainer *dec_cont = (struct Av1DecContainer *)dec_inst;

  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }

  pthread_mutex_lock(&dec_cont->protect_mutex);

#if 0
  /* If a normal EOS is waited, return directly */
  if (dec_cont->dec_stat == AV1DEC_END_OF_STREAM) {
    pthread_mutex_unlock(&dec_cont->protect_mutex);
    return DEC_OK;
  }
#endif

  /* If hw was running, stop and release hw */
  if (dec_cont->asic_running) {
    Av1AsicSync(dec_cont);
    /* Remove the last picture that is being decoded when aborting. */
    u32 ref_index = dec_cont->pic_callback_arg.index;
    u32 pp_index;
    Av1BufferQueueRemoveRef(dec_cont->bq, ref_index);
    pp_index = dec_cont->asic_buff->pp_buffer_map[ref_index];
    if (dec_cont->pp_bq)
      Av1BufferQueueRemoveRef(dec_cont->pp_bq, pp_index);
  }
#if 0
  /* Stop and release HW */
  (void)AV1SyncAndOutput(dec_cont);

  /* If buffer queue has been already initialized, we can use it to track
   * pending cores and outputs safely. */
  if (dec_cont->bq) {
    /* if the references and queue were already flushed, cannot
     * do it again. */
    if (dec_cont->asic_buff->out_buffer_i != AV1_UNDEFINED_BUFFER &&
        dec_cont->asic_buff->out_buffer_i != ABORT_MARKER) {
      u32 i = 0;
      /* Workaround for ref counting since this buffer is never used. */
      Av1BufferQueueRemoveRef(dec_cont->bq, dec_cont->asic_buff->out_buffer_i);
      dec_cont->asic_buff->out_buffer_i = AV1_UNDEFINED_BUFFER;

      for (i = 0; i < dec_cont->num_buffers; i++) {
        Av1BufferQueueRemoveRef(dec_cont->bq,
                                Av1BufferQueueGetRef(dec_cont->bq, i));
      }
    }
  }
#endif
  /* Clear reference count in buffer queue */
  Av1EmptyBufferQueue(dec_cont);

  Av1ResetDecState(dec_cont);

  /* Exist abort state */
  Av1ExistAbortState(dec_cont);

  pthread_mutex_unlock(&dec_cont->protect_mutex);
  return DEC_OK;
}

/* complete output pic EC policy : decision pic output */
enum DecRet Av1ECDecisionOutput(struct Av1DecContainer *dec_cont, struct Av1DecPicture *output) {
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
    return Av1DecPictureConsumed((void*)dec_cont, output);
  }

  return DEC_PIC_RDY;
}
