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
------------------------------------------------------------------------------*/

#include "dwl.h"
#include "deccfg.h"
#include "sw_debug.h"

#include "avs2_container.h"
#include "avs2_params.h"
#include "avs2_storage.h"
#include "avs2_dpb.h"
#include "avs2_decoder.h"
#include "dec_log.h"

#define IS_REFERENCE(a) IsReference(&(a))
static u32 IsReference(const struct Avs2DpbPicture *a) {
  return ((a->status) && (a->status != EMPTY));
}

/* Initializes storage structure. Sets contents of the storage to '0' except
 * for the active parameter set ids, which are initialized to invalid values.
 * */
void Avs2InitStorage(struct Avs2Storage *storage) {

  ASSERT(storage);

  storage->sps.cnt = 0;
  storage->pps.cnt = 0;
  // storage->aub->first_call_flag = HANTRO_TRUE;
  storage->poc[0] = storage->poc[1] = INIT_POC;
}

/**
 * Compare the two SPS.
 * @return 0 same;
 *         1 differenct;
 */
static int Avs2CompareSeqParamSets(struct Avs2SeqParam *sps1,
                                   struct Avs2SeqParam *sps2) {
  int length = (u8 *)(&sps1->cnt) - (u8 *)(sps1);
  u8 *data1 = (u8 *)sps1;
  u8 *data2 = (u8 *)sps2;
  int i;

  /* compare syntax part only */
  for (i = 0; i < length; i++) {
    if (data1[i] != data2[i]) {
      return 1;
    }
  }

  return 0;
}

/**
 * Store sequence parameter set into the storage. If SPS is overwritten,
 *  need to check if contents changes and if it does, check if sps is same.
 * @return 0 fail for different sps is found;
 *         1 success to save the sps;
 *         2 duplicate sps is found.
 */
u32 Avs2StoreSeqParamSet(struct Avs2Storage *storage,
                         struct Avs2SeqParam *seq_param_set) {

  ASSERT(storage);
  ASSERT(seq_param_set);

  if ((storage->sps.cnt != 0) &&
      (Avs2CompareSeqParamSets(seq_param_set, &storage->sps) != 0)) {
    return (HANTRO_NOK);
  }

  DWLmemcpy(&storage->sps, seq_param_set, sizeof(struct Avs2SeqParam));
  return (HANTRO_OK);
}

/**
 * Store picture parameter set into the storage.
 */
u32 Avs2StorePicParamSet(struct Avs2Storage *storage,
                         struct Avs2PicParam *pic_param_set) {

  ASSERT(storage);
  ASSERT(pic_param_set);

  DWLmemcpy(&storage->pps, pic_param_set, sizeof(struct Avs2PicParam));

  return (HANTRO_OK);
}

/* Reset contents of the storage. This should be called before processing of
 * new image is started. */
void Avs2ResetStorage(struct Avs2Storage *storage) {

  ASSERT(storage);

  storage->prev_idr_pic_ready = HANTRO_FALSE;
}

/**
 * Check if any valid SPS/PPS combination exists in the storage.
 *
 * @return HANTRO_OK valid;
 *         HANTRO_NOK invalid.
 */
u32 Avs2ValidParamSets(struct Avs2Storage *storage) {

  ASSERT(storage);

  if ((storage->sps.cnt > 0) && (storage->pps.cnt > 0)) {
    return (HANTRO_OK);
  }

  return (HANTRO_NOK);
}

void Avs2ClearStorage(struct Avs2Storage *storage) {

  /* Variables */

  /* Code */
  u32 i;
  ASSERT(storage);

  Avs2ResetStorage(storage);

  storage->frame_rate = 0;
  storage->pic_started = HANTRO_FALSE;
  storage->valid_slice_in_access_unit = HANTRO_FALSE;
  storage->checked_aub = 0;
  storage->prev_buf_not_finished = HANTRO_FALSE;
  storage->prev_buf_pointer = NULL;
  storage->prev_bytes_consumed = 0;
  storage->picture_broken = HANTRO_FALSE;
  storage->poc_last_display = INIT_POC;

  (void)DWLmemset(&storage->poc, 0, 2 * sizeof(i32));
  (void)DWLmemset(&storage->curr_image, 0, sizeof(struct Image));
  //(void)DWLmemset(&storage->prev_nal_unit, 0, sizeof(struct NalUnit));
  //(void)DWLmemset(&storage->slice_header, 0, 2 * sizeof(struct SliceHeader));
  (void)DWLmemset(&storage->strm, 0, sizeof(struct StrmData));

  (void)i;
}
#if 0
/**
 * Calculate the buffer resources after seq parameter set is set. Check if
 *current setup
 *  can fit the requirement.
 *
 * @return 0 current buffer setup is OK;
 *         1 current buffer setup need to relloc.
 */
u32 Avs2IsExternalBuffersRealloc(void *dec_inst, struct Avs2Storage *storage) {

  const struct Avs2SeqParam *sps = &storage->sps;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 buff_size, pp_size = 0;
  // u32 is_frame_buffer;

  u32 n_extra_frm_buffers = storage->n_extra_frm_buffers;
  u32 dpb_size = sps->max_dpb_size + 1;
  u32 min_buffer_num;
  enum DecBufferType buf_type;

  pp_size = CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0U);

  u32 tot_buffers = dpb_size + 2 + n_extra_frm_buffers;
  if (tot_buffers > MAX_PIC_BUFFERS)
    tot_buffers = MAX_PIC_BUFFERS;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    min_buffer_num = dpb_size + 2; /* We need at least (dpb_size+2) output
                                      buffers before starting decoding. */
    buff_size = storage->buff_spec.ref.buff_size;
    buf_type = REFERENCE_BUFFER;
    // is_frame_buffer = 1;
  } else {
    min_buffer_num = dpb_size + 1;
    // is_frame_buffer = 0;
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
      buff_size = pp_size;
      buf_type = DOWNSCALE_OUT_BUFFER;
    }
  }

  if (buff_size <= dec_cont->ext_buffer_size &&
      min_buffer_num + dec_cont->guard_size <= dec_cont->buffer_num_added)
    dec_cont->reset_ext_buffer = 0;
  else
    dec_cont->reset_ext_buffer = 1;

  if (!dec_cont->use_adaptive_buffers) dec_cont->reset_ext_buffer = 1;

  (void)buf_type;

  return dec_cont->reset_ext_buffer;
}
#endif
/**
 * Derived Buffer Information from SPS.
 *
 * @pre sps is valid;
 * @return HANTRO_OK success;
 *         HANTRO_NOK fail;
 */
int Avs2DeriveBufferSpec(struct Avs2Storage *storage,
                         struct Avs2BufferSpec *info,
                         DecPicAlignment alignment) {

  const struct Avs2SeqParam *sps = &storage->sps;
  u32 pixel_width = sps->sample_bit_depth;

  u32 out_w, out_w_c, out_h;
  u32 pic_size, pic_csize;
  u32 ref_buffer_align = MAX(16, ALIGN(alignment));

  if (sps->cnt == 0) {
    STREAMTRACE_E("%s","[avs2dec] Cannot Derive Buffer Spec when sps is invalid.\n");
    return HANTRO_NOK;
  };

  /* pixels in tiles */
  out_w = NEXT_MULTIPLE(4 * sps->pic_width_in_cbs * 8 * pixel_width,
                        ALIGN(alignment) * 8) / 8;
  out_w_c = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(sps->pic_width_in_cbs * 8, 16) * pixel_width,
                        ALIGN(alignment) * 8) / 8;
  out_h = sps->pic_height_in_cbs * 8 / 4;
  pic_size = NEXT_MULTIPLE(out_w * out_h, ref_buffer_align);
  pic_csize = NEXT_MULTIPLE(out_w_c * out_h / 2, ref_buffer_align);
  info->ref.picy_size = pic_size;
  info->ref.pic_size = pic_size + pic_csize;

  /* dmv : ctbs_num * 16x16_block_per_ctu * num_bytes_per_16x16  */
#if 0
  info->ref.dmv_size = NEXT_MULTIPLE((sps->pic_width_in_ctbs * sps->pic_height_in_ctbs *
                        (1 << (2 * (sps->lcu_size_in_bit - 4))) * 16), ref_buffer_align);
#else
  u32 pic_width_in_ctb64 = (sps->pic_width + (1 << 6) - 1) >> 6;
  u32 pic_height_in_ctb64 = (sps->pic_height + (1 << 6) - 1) >> 6;

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
  info->ref.dmv_size =
    /* num ctbs */
     NEXT_MULTIPLE((pic_width_in_ctb64 * pic_height_in_ctb64 *
     /* num 16x16 blocks in ctbs */
     (1 << (2 * (6 - 4))) *
     /* num bytes per 16x16 */
     16), ref_buffer_align);
#endif
  //(1 << (2 * (sps->lcu_size_in_bit - 4))) * 16 *12);
  //*
  // only for avs2, each 4x4 has 12 bytes
  // 12);

  /* tables for compressor */
  info->ref.tbl_size = info->ref.tbly_size = info->ref.tblc_size = 0;
  if (storage->use_video_compressor) {
    u32 pic_width_in_cbsy, pic_height_in_cbsy;
    u32 pic_width_in_cbsc, pic_height_in_cbsc;
    pic_width_in_cbsy = (sps->pic_width_in_cbs);
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = ((sps->pic_width_in_cbs * 8 + 16 - 1) / 16);
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (sps->pic_height_in_cbs);
    pic_height_in_cbsc = (sps->pic_height_in_cbs * 4 + 4 - 1) / 4;

    /* luma table size */
    info->ref.tbly_size =
        NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, ref_buffer_align);
    /* chroma table size */
    info->ref.tblc_size =
        NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, ref_buffer_align);
    /* total */
    info->ref.tbl_size = info->ref.tbly_size + info->ref.tblc_size;
  }

  /* reference */
  info->ref.buff_size =
      info->ref.pic_size;


  return HANTRO_OK;
}

void Avs2SetPp(struct Avs2Storage *storage, struct Avs2PpoutParam *pp,
               PpUnitIntConfig *ppu_cfg, const struct DecHwFeatures *hw_feature) {
  pp->pp_buffer = storage->curr_image->pp_data;
  pp->ppu_cfg = ppu_cfg;
  pp->hw_feature = hw_feature;
  pp->top_field_first = storage->top_field_first;
  pp->bottom_flag = (storage->sps.is_field_sequence && !storage->pps.is_top_field);
}

#define SYNC_WORD_MC    32
void Avs2SetRecon(struct Avs2Storage *storage, struct Reference *recon,
                  struct DWLLinearMem *data) {
  struct Avs2BufferSpec *info = &storage->buff_spec;
//  int i;
//  int *mv_pt;

  recon->y.bus_address = data->bus_address;
  recon->y.logical_size = recon->y.size = info->ref.picy_size;

  recon->c.bus_address = data->bus_address + info->ref.picy_size;
  recon->c.logical_size = recon->c.size =
      info->ref.pic_size - info->ref.picy_size;

  if (data->virtual_address != NULL) {
    recon->y.virtual_address = data->virtual_address;
    recon->c.virtual_address = data->virtual_address + info->ref.picy_size / 4;
  } else {
    recon->y.virtual_address = NULL;
    recon->c.virtual_address = NULL;
  }
  if (storage->curr_image->parasitic_data->virtual_address != NULL) {
    recon->mv.virtual_address = storage->curr_image->parasitic_data->virtual_address
                                + NEXT_MULTIPLE(SYNC_WORD_MC, MAX(16, ALIGN(storage->align)))/4;
    recon->sync.virtual_address = recon->mv.virtual_address - SYNC_WORD_MC / 4;
  } else {
    recon->mv.virtual_address = NULL;
    recon->sync.virtual_address = NULL;
  }
  recon->mv.bus_address = storage->curr_image->parasitic_data->bus_address
                          + NEXT_MULTIPLE(SYNC_WORD_MC, MAX(16, ALIGN(storage->align)));
  recon->mv.logical_size = recon->mv.size = info->ref.dmv_size;
  recon->sync.bus_address = recon->mv.bus_address - SYNC_WORD_MC;
  recon->sync.logical_size = recon->sync.size = SYNC_WORD_MC;
#if 0
  // clear mv buffer
  mv_pt = (int *)recon->mv.virtual_address;

  for (i = 0; i < info->ref.dmv_size; i += 12) {  // 12 bytes for each 4x4
    *mv_pt++ = 0;
    *mv_pt++ = 0;
    *mv_pt++ = -1;
  }
#endif

  if (storage->use_video_compressor) {
    if (data->virtual_address != NULL) {
      recon->y_tbl.virtual_address =
          recon->c.virtual_address + (info->ref.pic_size - info->ref.picy_size) / 4;
      recon->c_tbl.virtual_address =
          recon->y_tbl.virtual_address + recon->y_tbl.size / 4;
    } else {
      recon->y_tbl.virtual_address = NULL;
      recon->c_tbl.virtual_address = NULL;
    }
    recon->y_tbl.bus_address = recon->c.bus_address + info->ref.pic_size - info->ref.picy_size;
    recon->y_tbl.logical_size = recon->y_tbl.size = info->ref.tbly_size;
    recon->c_tbl.bus_address = recon->y_tbl.bus_address + recon->y_tbl.size;
    recon->c_tbl.logical_size = recon->c_tbl.size = info->ref.tblc_size;
  }
}

void Avs2SetRef(struct Avs2Storage *storage, struct Avs2RefsParam *refs,
                struct Avs2DpbStorage *dpb) {

  struct Avs2BufferSpec *info = &storage->buff_spec;
  int i, j, idx;

  // info->ref.picy_size: y size
  // info->ref.pic_size: yuv size
  // info->ref.buff_size = info->ref.pic_size + info->ref.dmv_size +
  // info->ref.tbl_size;
  // dpb->pic_size = para->pic_size;
  // dpb->dir_mv_offset = NEXT_MULTIPLE(para->pic_size * 3 / 2, 16);
  // dpb->cbs_tbl_offsety = para->buff_size - para->tbl_sizey - para->tbl_sizec;
  // dpb->cbs_tbl_offsetc = para->buff_size - para->tbl_sizec;
  // dpb->cbs_tbl_size = para->tbl_sizey + para->tbl_sizec;

  for (i = 0; i < MAXREF; i++) {
    idx = dpb->ref_pic_set_st[i];

    /* skip non reference */
    if (!IS_REFERENCE(dpb->buffer[idx])) {
      refs->ref[i].is_invalid = 1;
      continue;
    }

    /* reference_frame == current_frame : error bitstream encounted. */
    if (dpb->buffer[idx].img_poi == dpb->poi) {
      refs->ref[i].is_invalid = 1;
      /* use others ref */
      int idx_tmp = 0;
      for (j = 0; j < MAXREF; j++) {
        idx_tmp = dpb->ref_pic_set_st[j];
        if (idx_tmp != idx &&
            dpb->buffer[idx_tmp].img_poi != dpb->poi &&
            IS_REFERENCE(dpb->buffer[idx_tmp])) {
          idx = idx_tmp;
          break;
        }
      }
      /* can't find others ref: don't replace. */
    }

    refs->ref[i].y.bus_address = dpb->buffer[idx].data->bus_address;
    refs->ref[i].y.virtual_address = dpb->buffer[idx].data->virtual_address;
    refs->ref[i].y.logical_size = info->ref.picy_size;
    refs->ref[i].y.size = info->ref.picy_size;

    refs->ref[i].c.bus_address =
        dpb->buffer[idx].data->bus_address + info->ref.picy_size;
    refs->ref[i].c.virtual_address =
        dpb->buffer[idx].data->virtual_address + info->ref.picy_size / 4;
    refs->ref[i].c.logical_size = info->ref.pic_size - info->ref.picy_size;
    refs->ref[i].c.size = info->ref.pic_size - info->ref.picy_size;

    refs->ref[i].img_poi = dpb->buffer[idx].img_poi;

    refs->ref[i].mv.bus_address =
        dpb->buffer[idx].parasitic_data->bus_address
        + NEXT_MULTIPLE(SYNC_WORD_MC,MAX(16, ALIGN(storage->align)));
    refs->ref[i].mv.virtual_address =
        dpb->buffer[idx].parasitic_data->virtual_address
        + NEXT_MULTIPLE(SYNC_WORD_MC,MAX(16, ALIGN(storage->align)))/4;
    refs->ref[i].mv.logical_size = refs->ref[i].mv.size = info->ref.dmv_size;

    if (storage->use_video_compressor) {
      refs->ref[i].y_tbl.bus_address =
          refs->ref[i].c.bus_address + info->ref.pic_size - info->ref.picy_size;
      refs->ref[i].y_tbl.virtual_address =
          refs->ref[i].c.virtual_address + (info->ref.pic_size - info->ref.picy_size) / 4;
      refs->ref[i].y_tbl.logical_size = info->ref.tbly_size;
      refs->ref[i].y_tbl.size = info->ref.tbly_size;

      refs->ref[i].c_tbl.bus_address =
          refs->ref[i].y_tbl.bus_address + info->ref.tbly_size;
      refs->ref[i].c_tbl.virtual_address =
          refs->ref[i].y_tbl.virtual_address + info->ref.tbly_size / 4;
      refs->ref[i].c_tbl.logical_size = info->ref.tblc_size;
      refs->ref[i].c_tbl.size = info->ref.tblc_size;
    }

    for (j = 0; j < MAXREF; j++) {
      refs->ref[i].ref_poc[j] = dpb->buffer[idx].ref_poc[j];
    }

    /* Unify EC */
    refs->ref[i].is_invalid = 0;
    refs->ref[i].error_info = dpb->buffer[idx].error_info;
    refs->ref[i].error_ratio = dpb->buffer[idx].error_ratio;
  }

  idx = dpb->ref_pic_set_st[MAXREF];

  refs->background.y.bus_address = dpb->buffer[idx].data->bus_address;
  refs->background.y.virtual_address = dpb->buffer[idx].data->virtual_address;
  refs->background.y.logical_size = info->ref.picy_size;
  refs->background.y.size = info->ref.picy_size;

  refs->background.c.bus_address =
      dpb->buffer[idx].data->bus_address + info->ref.picy_size;
  refs->background.c.virtual_address =
      dpb->buffer[idx].data->virtual_address + info->ref.picy_size / 4;
  refs->background.c.logical_size = info->ref.pic_size - info->ref.picy_size;
  refs->background.c.size = info->ref.pic_size - info->ref.picy_size;

  refs->background.img_poi = dpb->buffer[idx].img_poi;

  refs->background.mv.bus_address =
      dpb->buffer[idx].parasitic_data->bus_address
      + NEXT_MULTIPLE(SYNC_WORD_MC, MAX(16, ALIGN(storage->align)));
  refs->background.mv.virtual_address =
      dpb->buffer[idx].parasitic_data->virtual_address
      + NEXT_MULTIPLE(SYNC_WORD_MC, MAX(16, ALIGN(storage->align)))/4;
  refs->background.mv.logical_size = refs->background.mv.size =
      info->ref.dmv_size;

  if (storage->use_video_compressor) {
    refs->background.y_tbl.bus_address =
        refs->background.c.bus_address + info->ref.pic_size - info->ref.picy_size;
    refs->background.y_tbl.virtual_address =
        refs->background.c.virtual_address + (info->ref.pic_size - info->ref.picy_size) / 4;
    refs->background.y_tbl.logical_size = info->ref.tbly_size;
    refs->background.y_tbl.size = info->ref.tbly_size;

    refs->background.c_tbl.bus_address =
        refs->background.y_tbl.bus_address + info->ref.tbly_size;
    refs->background.c_tbl.virtual_address =
        refs->background.y_tbl.virtual_address + info->ref.tbly_size / 4;
    refs->background.c_tbl.logical_size = info->ref.tblc_size;
    refs->background.c_tbl.size = info->ref.tblc_size;
  }
}
#if 0
void Avs2SetExternalBufferInfo(void *dec_inst, struct Avs2Storage *storage) {
  const struct Avs2SeqParam *sps = &storage->sps;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 buff_size, pp_size = 0;
#ifdef ASIC_TRACE_SUPPORT
  u32 is_frame_buffer;
#endif

  u32 n_extra_frm_buffers = storage->n_extra_frm_buffers;

  u32 dpb_size = sps->max_dpb_size + 1;
  u32 min_buffer_num;
  enum DecBufferType buf_type;

  pp_size = CalcPpUnitBufferSize(dec_cont->ppu_cfg, 0U);

  u32 tot_buffers = dpb_size + 2 + n_extra_frm_buffers;
  if (tot_buffers > MAX_PIC_BUFFERS)
    tot_buffers = MAX_PIC_BUFFERS;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    min_buffer_num = dpb_size + 2; /* We need at least (dpb_size+2) output
                                      buffers before starting decoding. */
    buff_size = storage->buff_spec.ref.buff_size;
    buf_type = REFERENCE_BUFFER;
#ifdef ASIC_TRACE_SUPPORT
    is_frame_buffer = 1;
#endif
  } else {
    min_buffer_num = dpb_size;
#ifdef ASIC_TRACE_SUPPORT
    is_frame_buffer = 0;
#endif
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER)) {
      buff_size = pp_size;
      buf_type = DOWNSCALE_OUT_BUFFER;
    }
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = buff_size;
  dec_cont->buf_type = buf_type;
#ifdef ASIC_TRACE_SUPPORT
  dec_cont->is_frame_buffer = is_frame_buffer;
#endif
}
#endif

/* Allocates SW resources after parameter set activation. */
void Avs2GetRefFrmSize(struct Avs2DecContainer *dec_cont,
                       u32 *luma_size, u32 *chroma_size,
                       u32 *rfc_luma_size, u32 *rfc_chroma_size) {
  struct Avs2Storage *storage = &dec_cont->storage;
  struct Avs2SeqParam *sps = &storage->sps;
  u32 tbl_sizey, tbl_sizec;

  u32 out_w, out_h;
  u32 ref_size;
  u32 pixel_width = sps->sample_bit_depth;


  out_w = NEXT_MULTIPLE(4 * sps->pic_width * pixel_width, ALIGN(dec_cont->align) * 8) / 8;
  out_h = sps->pic_height / 4;
  ref_size = out_w * out_h;
  if (luma_size) *luma_size = ref_size;
  if (chroma_size) {
    out_w = NEXT_MULTIPLE(4 * NEXT_MULTIPLE(sps->pic_width, 16) * pixel_width, ALIGN(dec_cont->align) * 8) / 8;
    out_h = sps->pic_height / 4;
    ref_size = out_w * out_h;
    *chroma_size = ref_size / 2;
  }

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
#if 0
  u32 dmv_mem_size =
    /* num ctbs */
    (pic_width_in_ctbs * pic_height_in_ctbs *
     /* num 16x16 blocks in ctbs */
     (1 << (2 * (sps->log_max_coding_block_size - 4))) *
     /* num bytes per 16x16 */
     16);
  ref_size = NEXT_MULTIPLE(ref_size * 3 / 2, 16) + dmv_mem_size;
#endif

  if (storage->use_video_compressor) {
    u32 pic_width_in_cbsy, pic_height_in_cbsy;
    u32 pic_width_in_cbsc, pic_height_in_cbsc;
    pic_width_in_cbsy = ((sps->pic_width + 8 - 1)/8);
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = ((sps->pic_width + 16 - 1)/16);
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (sps->pic_height + 8 - 1)/8;
    pic_height_in_cbsc = (sps->pic_height/2 + 4 - 1)/4;

    /* luma table size */
    tbl_sizey = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, 16);
    tbl_sizec = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, 16);
  } else
    tbl_sizey = tbl_sizec = 0;

  if (rfc_luma_size) *rfc_luma_size = tbl_sizey;
  if (rfc_chroma_size) *rfc_chroma_size = tbl_sizec;
}

u32 Avs2IsExternalBuffersRealloc(void *dec_inst, struct Avs2Storage *storage) {
  const struct Avs2SeqParam *sps = &storage->sps;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  u32 luma_size = 0, chroma_size = 0, rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 buff_size, pp_size = 0, pic_size;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  Avs2GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                      &rfc_luma_size, &rfc_chroma_size);

#if 0
  /* TODO: next cb or ctb multiple? */
  u32 pic_width_in_ctbs =
    (sps->pic_width + (1 << sps->log_max_coding_block_size) - 1) >>
    sps->log_max_coding_block_size;
  u32 pic_height_in_ctbs =
    (sps->pic_height + (1 << sps->log_max_coding_block_size) - 1) >>
    sps->log_max_coding_block_size;

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
  u32 dmv_mem_size =
    /* num ctbs */
    (pic_width_in_ctbs * pic_height_in_ctbs *
     /* num 16x16 blocks in ctbs */
     (1 << (2 * (sps->log_max_coding_block_size - 4))) *
     /* num bytes per 16x16 */
     16);
#else
  /* Calculate dir_mv_size based on the max resolution, so that the changing in
     CTB size of given resolution won't result in the external buffer
     re-allocation. */
#if 0
  u32 pic_width_in_ctb64 = (sps->pic_width + (1 << 6) - 1) >> 6;
  u32 pic_height_in_ctb64 = (sps->pic_height + (1 <<6) - 1) >> 6;

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
  u32 dmv_mem_size =
    /* num ctbs */
    NEXT_MULTIPLE((pic_width_in_ctb64 * pic_height_in_ctb64 *
     /* num 16x16 blocks in ctbs */
     (1 << (2 * (6 - 4))) *
     /* num bytes per 16x16 */
     16), ref_buffer_align);
#endif
#endif

  pic_size =  NEXT_MULTIPLE(luma_size, ref_buffer_align);

  u32 n_extra_frm_buffers = storage->n_extra_frm_buffers;

  /* allocate 32 bytes for multicore status fields */
  /* locate it after picture and before direct MV */
  u32 ref_buff_size = pic_size
                      + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
  u32 dpb_size = sps->max_dpb_size + 1;
  u32 min_buffer_num;

  if (storage->use_video_compressor) {
    /* luma table size */
    ref_buff_size += NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
    /* chroma table size */
    ref_buff_size += NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);
  }

  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);

  u32 tot_buffers = dpb_size + 2 + n_extra_frm_buffers;
#ifdef ASIC_TRACE_SUPPORT
  //tot_buffers += 9;
#endif
  if (tot_buffers > MAX_PIC_BUFFERS)
    tot_buffers = MAX_PIC_BUFFERS;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    min_buffer_num = dpb_size + 2;   /* We need at least (dpb_size+2) output buffers before starting decoding. */
    buff_size = ref_buff_size;
  } else {
    min_buffer_num = dpb_size + 1;
    ASSERT(IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER));
    buff_size = pp_size;
  }

  if (buff_size <= dec_cont->ext_buffer_size &&
      min_buffer_num + dec_cont->guard_size <= dec_cont->buffer_num_added)
    dec_cont->reset_ext_buffer = 0;
  else
    dec_cont->reset_ext_buffer = 1;

  if (!dec_cont->use_adaptive_buffers)
    dec_cont->reset_ext_buffer = 1;

  return dec_cont->reset_ext_buffer;
}

void Avs2SetExternalBufferInfo(void *dec_inst, struct Avs2Storage *storage) {
  const struct Avs2SeqParam *sps = &storage->sps;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  u32 buff_size, pp_size = 0, pic_size;
  u32 luma_size = 0, chroma_size = 0, rfc_luma_size = 0, rfc_chroma_size = 0;
	u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));


  Avs2GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                      &rfc_luma_size, &rfc_chroma_size);

  /* TODO: next cb or ctb multiple? */
#if 0
  u32 pic_width_in_ctbs =
    (sps->pic_width + (1 << sps->log_max_coding_block_size) - 1) >>
    sps->log_max_coding_block_size;
  u32 pic_height_in_ctbs =
    (sps->pic_height + (1 << sps->log_max_coding_block_size) - 1) >>
    sps->log_max_coding_block_size;
#else
  /* Calculate dir_mv_size based on the max resolution, so that the changing in
     CTB size of given resolution won't result in the external buffer
     re-allocation. */
  u32 pic_width_in_ctb64 = (sps->pic_width + (1 << 6) - 1) >> 6;
  u32 pic_height_in_ctb64 = (sps->pic_height + (1 <<6) - 1) >> 6;
#endif

  pic_size = NEXT_MULTIPLE(luma_size, ref_buffer_align);

  u32 n_extra_frm_buffers = storage->n_extra_frm_buffers;

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
  u32 dmv_mem_size =
    /* num ctbs */
    NEXT_MULTIPLE((pic_width_in_ctb64 * pic_height_in_ctb64 *
     /* num 16x16 blocks in ctbs */
     (1 << (2 * (6 - 4))) *
     /* num bytes per 16x16 */
     16), ref_buffer_align);

  /* allocate 32 bytes for multicore status fields */
  /* locate it after picture and before direct MV */
  u32 ref_buff_size = pic_size
                      + NEXT_MULTIPLE(chroma_size, ref_buffer_align)
                      + dmv_mem_size
                      + NEXT_MULTIPLE(32, ref_buffer_align);
  u32 dpb_size = sps->max_dpb_size + 1;
  u32 min_buffer_num;
  enum DecBufferType buf_type;

  if (storage->use_video_compressor) {
    /* luma table size */
    ref_buff_size += NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
    /* chroma table size */
    ref_buff_size += NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);
  }

  PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
  pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);

  u32 tot_buffers = dpb_size + 2 + n_extra_frm_buffers;
#ifdef ASIC_TRACE_SUPPORT
  //tot_buffers += 9;
#endif
  if (tot_buffers > MAX_PIC_BUFFERS)
    tot_buffers = MAX_PIC_BUFFERS;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    min_buffer_num = dpb_size + 2;   /* We need at least (dpb_size+2) output buffers before starting decoding. */
    buff_size = ref_buff_size;
    buf_type = REFERENCE_BUFFER;
  } else {
    min_buffer_num = dpb_size + 1;
    ASSERT(IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, DOWNSCALE_OUT_BUFFER));
    buff_size = pp_size;
    buf_type = DOWNSCALE_OUT_BUFFER;
  }

  dec_cont->buf_num = min_buffer_num;
  dec_cont->next_buf_size = buff_size;
  dec_cont->buf_type = buf_type;
}

u32 Avs2AllocateSwResources(const void *dwl, struct Avs2Storage *storage, void *dec_inst) {
  u32 tmp;
  const struct Avs2SeqParam *sps = &storage->sps;
  struct Avs2DpbInitParams dpb_params;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 luma_size = 0, chroma_size = 0, rfc_luma_size = 0, rfc_chroma_size = 0;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  Avs2GetRefFrmSize(dec_cont, &luma_size, &chroma_size,
                      &rfc_luma_size, &rfc_chroma_size);

  /* TODO: next cb or ctb multiple? */

  storage->pic_size = NEXT_MULTIPLE(luma_size, ref_buffer_align);
  storage->curr_image->width = sps->pic_width;
  storage->curr_image->height = sps->pic_height;
  /* dpb output reordering disabled if
   * 1) application set no_reordering flag
   * 2) num_reorder_frames equal to 0 */
  if (storage->no_reordering)
    dpb_params.no_reordering = HANTRO_TRUE;
  else
    dpb_params.no_reordering = HANTRO_FALSE;

  dpb_params.n_cores = dec_cont->hwdec.n_cores;
  dpb_params.dpb_size = sps->max_dpb_size;
  dpb_params.pic_size = storage->pic_size;
  dpb_params.n_extra_frm_buffers = storage->n_extra_frm_buffers;

  /* Calculate dir_mv_size based on the max resolution, so that the changing in
     CTB size of given resolution won't result in the external buffer
     re-allocation. */

  u32 pic_width_in_ctb64 = (sps->pic_width + (1 << 6) - 1) >> 6;
  u32 pic_height_in_ctb64 = (sps->pic_height + (1 << 6) - 1) >> 6;

  /* buffer size of dpb pic = pic_size + dir_mv_size + tbl_size */
  storage->dmv_mem_size =
    /* num ctbs */
     NEXT_MULTIPLE((pic_width_in_ctb64 * pic_height_in_ctb64 *
     /* num 16x16 blocks in ctbs */
     (1 << (2 * (6 - 4))) *
     /* num bytes per 16x16 */
     16), ref_buffer_align);

  /* allocate 32 bytes for multicore status fields */
  /* locate it after picture and before direct MV */
  dpb_params.buff_size = storage->pic_size
                         + NEXT_MULTIPLE(chroma_size, ref_buffer_align);
  dpb_params.dpb_parasitic_buf_size = storage->dmv_mem_size + NEXT_MULTIPLE(32, ref_buffer_align);
  if (storage->use_video_compressor) {
    /* luma table size */
    dpb_params.buff_size += NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
    /* chroma table size */
    dpb_params.buff_size += NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);

    dpb_params.tbl_sizey = NEXT_MULTIPLE(rfc_luma_size, ref_buffer_align);
    dpb_params.tbl_sizec = NEXT_MULTIPLE(rfc_chroma_size, ref_buffer_align);
  } else
    dpb_params.tbl_sizey = dpb_params.tbl_sizec = 0;

  /* note that calling ResetDpb here results in losing all
   * pictures currently in DPB -> nothing will be output from
   * the buffer even if no_output_of_prior_pics_flag is HANTRO_FALSE */
  tmp = Avs2ResetDpb(dec_inst, storage->dpb, &dpb_params);


  storage->dpb->pic_width = sps->pic_width;
  storage->dpb->pic_height = sps->pic_height;
  storage->dpb->sample_bit_depth = sps->sample_bit_depth;

  {
    struct DecCropParams *crop = &storage->dpb->crop_params;
//    const struct Avs2SeqParam *sps = &storage->sps;
    const struct Avs2SeqDisaplay *disp = &storage->ext.display;

    if (disp->cnt) {
      crop->crop_left_offset = 0;
      crop->crop_top_offset = 0;
      crop->crop_out_width = disp->display_horizontal_size;
      crop->crop_out_height = disp->display_vertical_size;
    } else {
      u32 cropping_flag;
      Avs2CroppingParams(storage, &cropping_flag, &crop->crop_left_offset,
                         &crop->crop_out_width, &crop->crop_top_offset,
                         &crop->crop_out_height);
    }
  }

  if (tmp != HANTRO_OK) return (tmp);


   /* Issue the external buffer request here when necessary. */
  if (storage->pp_enabled &&storage->dpb->dpb_reset
      && dec_cont->reset_ext_buffer) {
    PpUnitIntConfig *ppu_cfg = dec_cont->ppu_cfg;
    u32 pp_size = 0;
    dec_cont->release_buffer = 1;

    pp_size = CalcPpUnitBufferSize(ppu_cfg, 0);
    dec_cont->min_buffer_num = sps->max_dpb_size + 2;

    dec_cont->buf_type = DOWNSCALE_OUT_BUFFER;
    dec_cont->next_buf_size = pp_size;
    dec_cont->buf_num = dec_cont->min_buffer_num;

      return DEC_WAITING_FOR_BUFFER;
   }
  /*
    {
      struct DecCropParams *crop = &storage->dpb->crop_params;
      const struct SeqParamSet *sps = storage->active_sps;

      if (sps->pic_cropping_flag) {
        u32 tmp1 = 2, tmp2 = 1;

        crop->crop_left_offset = tmp1 * sps->pic_crop_left_offset;
        crop->crop_out_width =
            sps->horizontal_size -
            tmp1 * (sps->pic_crop_left_offset + sps->pic_crop_right_offset);

        crop->crop_top_offset = tmp1 * tmp2 * sps->pic_crop_top_offset;
        crop->crop_out_height =
            sps->vertical_size - tmp1 * tmp2 * (sps->pic_crop_top_offset +
                                             sps->pic_crop_bottom_offset);
      } else {
        crop->crop_left_offset = 0;
        crop->crop_top_offset = 0;
        crop->crop_out_width = sps->horizontal_size;
        crop->crop_out_height = sps->vertical_size;
      }
    }
  */

  return HANTRO_OK;
}

