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

//#include "hevc_image.h"
//#include "hevc_util.h"
#include "basetype.h"
#include "dwl.h"
#include "sw_debug.h"

#include "avs2_container.h"
#include "avs2_cfg.h"
#include "avs2_dpb.h"
#include "dec_log.h"

//COI = DOI, POI = POC
/* Function style implementation for IS_REFERENCE() macro to fix compiler
 * warnings */
static u32 IsReference(const struct Avs2DpbPicture *a) {
  return ((a->status) && (a->status != EMPTY));
}

static u32 IsExisting(const struct Avs2DpbPicture *a) {
  return (a->status > NON_EXISTING && a->status != EMPTY);
}

static u32 IsShortTerm(const struct Avs2DpbPicture *a) {
  return (a->status == SHORT_TERM);
}

static u32 IsLongTerm(const struct Avs2DpbPicture *a) {
  return (a->status == LONG_TERM);
}

static u32 IsShortLongTerm(const struct Avs2DpbPicture *a) {
  return (a->status == SHORT_LONG_TERM);
}

static void SetStatus(struct Avs2DpbPicture *pic,
                      const enum DpbPictureStatus s) {
  pic->status = s;
}


static i32 GetPoc(struct Avs2DpbPicture *pic) {
  return (pic->status == EMPTY ? 0x7FFFFFFF : pic->img_poi); // = poc
}

#define IS_REFERENCE(a) IsReference(&(a))
#define IS_EXISTING(a) IsExisting(&(a))
#define IS_SHORT_TERM(a) IsShortTerm(&(a))
#define IS_LONG_TERM(a) IsLongTerm(&(a))
#define IS_SHORT_LONG_TERM(a) IsShortLongTerm(&(a))

#define SET_STATUS(pic, s) SetStatus(&(pic), s)

#define GET_POC(pic) GetPoc(&(pic))

#define MAX_NUM_REF_IDX_L0_ACTIVE 16

#define MEM_STAT_DPB 0x1
#define MEM_STAT_OUT 0x2
#define INVALID_MEM_IDX 0xFF

static void Avs2DpbBufFree(struct Avs2DpbStorage *dpb, u32 i);

static i32 Avs2FindDpbPic(struct Avs2DpbStorage *dpb, i32 poc);
//static i32 Avs2FindDpbRefPic(struct Avs2DpbStorage *dpb, i32 poc, u32 type);

// static /*@null@ */ struct Avs2DpbPicture *Avs2FindSmallestPicOrderCnt(
//  struct Avs2DpbStorage *dpb);
static struct Avs2DpbPicture *Avs2FindSmallestPicOrderCnt(
    struct Avs2DpbStorage *dpb);

static u32 Avs2OutputPicture(struct Avs2DpbStorage *dpb);
u32 Avs2DpbMarkAllUnused(struct Avs2DpbStorage *dpb);

static u32 Avs2InitDpb(const void *dwl, struct Avs2DpbStorage *dpb,
                       struct Avs2DpbInitParams *para);

static void Avs2DpbRemoveBackground(struct Avs2DpbStorage *dpb);

/* Output pictures if the are more outputs than reorder delay set in sps */
void Avs2DpbCheckMaxLatency(struct Avs2DpbStorage *dpb, u32 max_latency) {
  u32 i;

  while (dpb->num_out_pics_buffered > max_latency) {
    i = Avs2OutputPicture(
        dpb);  // copy dpb buf to dpb output buffer, num_out_pics_buffered--
    ASSERT(i == HANTRO_OK);
    (void)i;
  }
}

/* Update list of output pictures. To be called after slice header is decoded
 * and reference pictures determined based on that. */
void Avs2DpbUpdateOutputList(struct Avs2DpbStorage *dpb,
                             struct Avs2PicParam *pps) {
  u32 i;
  //struct Avs2DpbPicture *tmp;
  struct Avs2Storage *storage = dpb->storage;


  /* dpb was initialized not to reorder the pictures -> output current
   * picture immediately */
  if (dpb->no_reordering) {
/* we dont know the output buffer at this point as
 * HevcAllocateDpbImage() has not been called, yet
 */
#if 0
  } else if (dpb->low_delay) {    //low delay
      /* output this picture next */
      struct Avs2DpbOutPicture *dpb_out = &dpb->out_buf[dpb->out_index_w];
      const struct Avs2DpbPicture *current_out = dpb->current_out;

#ifdef USE_EXTERNAL_BUFFER
      dpb_out->pp_data = current_out->pp_data;
#endif
      dpb_out->data = current_out->data;
      //dpb_out->is_idr = current_out->is_idr;
      dpb_out->pic_id = current_out->pic_id;
      dpb_out->decode_id = current_out->decode_id;
      dpb_out->num_err_mbs = current_out->num_err_mbs;
      dpb_out->mem_idx = current_out->mem_idx;
      dpb_out->tiled_mode = current_out->tiled_mode;

      dpb_out->pic_width = dpb->pic_width;
      dpb_out->pic_height = dpb->pic_height;
      dpb_out->crop_params = dpb->crop_params;
      dpb_out->sample_bit_depth = dpb->sample_bit_depth;

      dpb->last_poi = current_out->img_poi;
      dpb->num_out++;
      dpb->out_index_w++;
      if (dpb->out_index_w == MAX_DPB_SIZE + 1) dpb->out_index_w = 0;

      MarkTempOutput(dpb->fb_list, current_out->mem_idx);
#endif
  } else {
    //this variable is not used.
    dpb->output_cur_dec_pic = 0;

#if 1
#if 1
    /* C.5.2.2
     * The number of pictures in the DPB that are marked as "needed for output"
     * is greater than sps_max_num_reorder_pics[ HighestTid ].
     */
    if(dpb->num_out_pics_buffered >
           ((storage->sps.picture_reorder_delay))) {
      i = Avs2OutputPicture(dpb);
      //ASSERT(i == HANTRO_OK);
      (void)i;
    }
#else
    i = Avs2OutputPicture(dpb);
#endif
#endif
#if 0
    /* output pictures if buffer full */
    while (dpb->fullness > dpb->real_size) {
      i = Avs2OutputPicture(dpb);
      if (i != HANTRO_OK) break;
    }
#endif
  }
}

void Avs2DpbShow(struct Avs2DpbStorage *dpb) {
  int i;
  struct Avs2DpbPicture *pic;
  // delete the frame that will never be used, line 459 in Image.c
  for (i = 0; i <= dpb->dpb_size; i++) {
    pic = &dpb->buffer[i];
    if ((pic->status != UNUSED) || (pic->refered_by_others) ||
        (pic->to_be_displayed))
      DPB_TRACE(
          "[DPB]index=%d   doi=%d poi=%d status=%d display=%d refer=%d "
          "out=0x%8x at mem_idx=%d\n",
          i, pic->img_coi, pic->img_poi, pic->status, pic->to_be_displayed,
          pic->refered_by_others,
          (u32)(pic->pp_data == NULL) ? (0) : (pic->pp_data->virtual_address),
          pic->mem_idx);
  }
}

void Avs2DpbRemoveUnused(struct Avs2DpbStorage *dpb) {
  int i, j;
  // delete the frame that will never be used, line 459 in Image.c
  for (i = 0; i < dpb->num_to_remove; i++) {
    for (j = 0; j <= dpb->dpb_size; j++) {
      if (dpb->buffer[j].img_coi >= -256 &&
          dpb->buffer[j].img_coi == dpb->coi - dpb->remove_pic[i]) {
        break;
      }
    }
    if(j>dpb->dpb_size)
      continue;
    if (IS_SHORT_TERM(dpb->buffer[j])) {
      // short term frame, as reference pictures in spec.
      DPB_TRACE("[DPB] remove image doi=%d poi=%d at index=%d mem_idx=%d\n",
                dpb->buffer[j].img_coi, dpb->buffer[j].img_poi, j,
                dpb->buffer[j].mem_idx);
      dpb->buffer[j].refered_by_others = 0;
      SET_STATUS(dpb->buffer[j], UNUSED);
      /* remove frames which have been outputted */
      if (dpb->buffer[j].to_be_displayed == HANTRO_FALSE) {
        dpb->buffer[j].img_poi = -256;
        dpb->buffer[j].img_coi = -257;
        Avs2DpbBufFree(dpb, j);
      }
    } else if (IS_SHORT_LONG_TERM(dpb->buffer[j])) {
      // short long term frame, as scene picture also stored in reference
      // picture buffer in spec 9.2.4.
      DPB_TRACE("[DPB] remove scene image doi=%d poi=%d at index=%d mem_idx=%d\n",
                dpb->buffer[j].img_coi, dpb->buffer[j].img_poi, j,
                dpb->buffer[j].mem_idx);
      dpb->buffer[j].refered_by_others = 1;
      SET_STATUS(dpb->buffer[j], LONG_TERM);
    }
  }

  struct Avs2DpbPicture *current_out = dpb->current_out;
  // see spec. 9.2.4  buffer of decoded image.
  if (current_out->type == AVS2_PIC_GB || current_out->type == AVS2_PIC_G) {
    // GB and G frame
    Avs2DpbRemoveBackground(dpb);
    if(current_out->type == AVS2_PIC_GB)
      SET_STATUS(*current_out, LONG_TERM);
    else if(current_out->type == AVS2_PIC_G)
      SET_STATUS(*current_out, SHORT_LONG_TERM);
  } else {
    if (dpb->refered_by_others)
      SET_STATUS(*current_out, SHORT_TERM);
    else
      SET_STATUS(*current_out, UNUSED);
  }

  Avs2DpbShow(dpb);
}

/* Returns pointer to reference picture at location 'index' in the DPB. */
u8 *Avs2GetRefPicData(const struct Avs2DpbStorage *dpb, u32 index) {

  if (index >= dpb->dpb_size)  ////dpb_size:  sps->max_dpb_size + 1
    return (NULL);
  else if (!IS_EXISTING(dpb->buffer[index]))  // not short term and long term
    return (NULL);
  else
    return (u8 *)(dpb->buffer[index]
                      .data->virtual_address);  // data: reference frame address
}

/* Get picture type */
Avs2PicType Avs2GetType(struct Avs2DpbStorage *dpb) {
  switch (dpb->type) {
    case I_IMG:
      if (dpb->typeb == BACKGROUND_IMG && dpb->background_picture_enable)
        if (dpb->background_picture_output_flag)
          return AVS2_PIC_G;
        else
          return AVS2_PIC_GB;
      else
        return AVS2_PIC_I;
    case P_IMG:
      if (dpb->typeb == BP_IMG && dpb->background_picture_enable)
        return AVS2_PIC_S;
      else
        return AVS2_PIC_P;
    case F_IMG:
      return AVS2_PIC_F;
    case B_IMG:
      return AVS2_PIC_B;
    default:
      DPB_TRACE("%s","[avs2dec] Invalid picture type.\n");
      break;
  }
  return AVS2_PIC_P;
}

// Remove the old scene picture, which may only exist in scene picture buffer,
// or also in reference picture buffer.
static void Avs2DpbRemoveBackground(struct Avs2DpbStorage *dpb) {
  int i;
  struct Avs2Storage *storage = dpb->storage;

  for (i = 0; i <= dpb->dpb_size; i++) {
    if (i == dpb->current_out_pos) continue;
    if (IS_LONG_TERM(dpb->buffer[i])) {
      DPB_TRACE(
          "[DPB] remove background doi=%d poi=%d at index=%d mem_idx=%d\n",
          dpb->buffer[i].img_coi, dpb->buffer[i].img_poi, i,
          dpb->buffer[i].mem_idx);

      if(dpb->buffer[i].type == AVS2_PIC_GB) {
        // GB frame is removed directly
        dpb->buffer[i].refered_by_others = 0;
        SET_STATUS(dpb->buffer[i], UNUSED);
        /* remove frames which have been outputted */
        if (dpb->buffer[i].to_be_displayed == HANTRO_FALSE) {
          dpb->buffer[i].img_poi = -256;
          dpb->buffer[i].img_coi = -257;
          Avs2DpbBufFree(dpb, i);
        }
        if (storage->pp_enabled && dpb->buffer[i].pp_data) {
          if (!dpb->buffer[i].first_field)
            InputQueueReturnBuffer(storage->pp_buffer_queue,
                              DWL_GET_DEVMEM_ADDR(*(dpb->buffer[i].pp_data)));
        }
      } else {
        /* G picture which has been removed from reference picture buffers will
           be a long term picture too (only exists in scene picture buffer).
           And now it's removed from scene picture buffer. */
        dpb->buffer[i].refered_by_others = 0;
        SET_STATUS(dpb->buffer[i], UNUSED);
        if (dpb->buffer[i].to_be_displayed == HANTRO_FALSE) {
          dpb->buffer[i].img_poi = -256;
          dpb->buffer[i].img_coi = -257;
          Avs2DpbBufFree(dpb, i);
        }
      }

      break;
    } else if(IS_SHORT_LONG_TERM(dpb->buffer[i])) {
      ASSERT(dpb->buffer[i].type == AVS2_PIC_G);
      /* G frame has been removed from scene picture buffer and now it only exist
         in reference picture buffers and is changed to SHORT frame. */
      DPB_TRACE(
          "[DPB] remove background doi=%d poi=%d at index=%d mem_idx=%d\n",
          dpb->buffer[i].img_coi, dpb->buffer[i].img_poi, i,
          dpb->buffer[i].mem_idx);
      SET_STATUS(dpb->buffer[i], SHORT_TERM);
      break;
    }
  }
}

#define TO_BE_DISPLAYED(dpb) ((dpb)->typeb != BACKGROUND_IMG || (dpb)->background_picture_output_flag)

static struct DWLLinearMem *FindPpBuffer(struct Avs2DpbStorage *dpb, int poc, u32* pos ) {
  u32 i;
  for (i = 0; i <= dpb->dpb_size; i++) {
    if (dpb->buffer[i].next_poc == poc &&
        !(dpb->buffer[i].type == AVS2_PIC_GB && TO_BE_DISPLAYED(dpb))) {
      *pos = i;
      return dpb->buffer[i].pp_data;
    }
  }
  return NULL;
}

void print_dpb_buffer_status(struct Avs2DpbStorage *dpb)
{
  DPB_TRACE("%s","DPB BUFFER STATUS!\n");
  for(int i=0;i<= dpb->dpb_size;i++)
    DPB_TRACE("img_coi=%d,img_poi=%d,to_be_display=%d,status=%d\n",dpb->buffer[i].img_coi,dpb->buffer[i].img_poi,dpb->buffer[i].to_be_displayed,dpb->buffer[i].status);
}

/* Returns pointer to location where next picture shall be stored. */
void *Avs2AllocateDpbImage(const void *dwl, struct Avs2DpbStorage *dpb, i32 pic_order_cnt,
                           u32 current_pic_id) {

  i32 i;
  u32 to_be_displayed;
  u32 *p;
  struct Avs2Storage *storage = dpb->storage;

  /* find first unused and not-to-be-displayed pic */
  for (i = 0; i <= dpb->dpb_size; i++) {
    // check fref buffer, if has been output or invalid, break
    //(reference output order index - cur frame output order index) >=128,
    //9.2.2.(E)
    if (/*(dpb->buffer[i].img_coi < -256 ||
         abs(dpb->buffer[i].img_poi - dpb->poi) >= 128) && */
        (!dpb->buffer[i].to_be_displayed) &&
        (!dpb->buffer[i].refered_by_others) &&
        (!IsReference(&(dpb->buffer[i]))) &&
        !(dpb->buffer[i].first_field && dpb->buffer[i].field_coupled == 0)) {
      break;
    }
  }

  if (i > dpb->dpb_size) {
    i32 temp_img_poi=0xfff;
    i32 temp_i=0xfff;

    for (i = dpb->dpb_size; i >=0;  i--) {
      //use G frame
      if(dpb->buffer[i].to_be_displayed == 0 && dpb->buffer[i].refered_by_others==0) {
        if(temp_img_poi>=dpb->buffer[i].img_poi) {
          temp_img_poi = dpb->buffer[i].img_poi;
          temp_i = i;
        }
      }
    }
    i = temp_i;
    if(i == 0xfff)
    {
      int temp_img_coi=0xfff;
      int temp_i=0xfff;
      for (i = dpb->dpb_size; i >=0;  i--) {
        //use the first decoded frame
        if(dpb->buffer[i].to_be_displayed == 0  && dpb->buffer[i].status == SHORT_TERM) {
          if(temp_img_coi>=dpb->buffer[i].img_coi) {
            temp_img_coi = dpb->buffer[i].img_coi;
            temp_i = i;
          }
        }
      }
      i = temp_i;
      if(i == 0xfff)
        return NULL;
    }

    if (storage->pp_enabled &&
        storage->sps.is_field_sequence &&
        dpb->buffer[i].pp_data) {
      InputQueueReturnBuffer(storage->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dpb->buffer[i].pp_data)));
    }
  }
  dpb->dpb_overflow_cnt = 0;

  ASSERT(i <= dpb->dpb_size);  // 0-15
  dpb->current_out = &dpb->buffer[i];  // current frame point to buffer i
  dpb->current_out_pos = i;  // set current pos to buffer index

  if (storage->pp_enabled) {
    if (!storage->sps.is_field_sequence) {
      dpb->current_out->pp_data =
          InputQueueGetBuffer(storage->pp_buffer_queue, 0);
      if (dpb->current_out->pp_data == NULL) {
        return NULL;
      }
#ifdef ENABLE_FPGA_VERIFICATION
    DWLLinearMemset(dwl, dpb->current_out->pp_data, 0, 0,
                    dpb->current_out->pp_data->size);
#endif
      dpb->current_out->first_field = 0;
    } else {
      u32 position_in_dpb;
      if(Avs2GetType(dpb)==AVS2_PIC_GB)
        dpb->current_out->pp_data = FindPpBuffer(dpb, 0xfffffff,&position_in_dpb);
      else
        dpb->current_out->pp_data = FindPpBuffer(dpb, storage->pps.poc,&position_in_dpb);
      if (dpb->current_out->pp_data == NULL) {
        dpb->current_out->pp_data =
            InputQueueGetBuffer(storage->pp_buffer_queue, 0);
        if (dpb->current_out->pp_data == NULL) {
          return NULL;
        }
#ifdef ENABLE_FPGA_VERIFICATION
        DWLLinearMemset(dwl, dpb->current_out->pp_data, 0, 0,
                        dpb->current_out->pp_data->size);
#endif
        dpb->current_out->first_field = 1;
        dpb->current_out->field_coupled = HANTRO_FALSE;

        if (storage->pps.is_top_field)
          dpb->current_out->next_poc = storage->pps.poc + 1;
        else
          dpb->current_out->next_poc = storage->pps.poc - 1;
        if (storage->pps.is_top_field)
          storage->top_field_first = 1;
        else
          storage->top_field_first = 0;
      } else {
        dpb->buffer[position_in_dpb].field_coupled=HANTRO_TRUE;
        dpb->current_out->first_field = 0;
        dpb->current_out->next_poc = 0x7FFFFFFF;
        if (storage->pps.is_top_field)
          storage->top_field_first = 0;
        else
          storage->top_field_first = 1;
      }
    }
  }

  dpb->current_out->status = EMPTY;  // mark current frame status empty
  // mem_idx: current frame 's id in fb_list,  if fb_list[mem_idx] is
  // referenced(n_ref_count !=0),
  // find new free buffer with new_id in fb_list
#if !defined(ASIC_TRACE_SUPPORT) || !defined(SUPPORT_MULTI_CORE)
  /* To avoid multicore top simultion buffer confliction, we always find a new
     free buffer. */
  if (IsBufferReferenced(dpb->fb_list, dpb->current_out->mem_idx) ||
      IsBufferOutput(dpb->fb_list, dpb->current_out->mem_idx))
#endif
  {
    u32 new_id = GetFreePicBuffer(dpb->fb_list, dpb->current_out->mem_idx);
    if (new_id == FB_NOT_VALID_ID) {
      if (dpb->no_free_buffer_cnt == 0) {
        DPB_TRACE("%s","[DPB] no free buffer.\n");
      } else {
        DPB_TRACE("%s",",");
      }
      dpb->no_free_buffer_cnt++;
      if (storage->pp_enabled) {
        if (dpb->current_out->first_field ||
            !storage->sps.is_field_sequence)
          InputQueueReturnBuffer(storage->pp_buffer_queue,
                            DWL_GET_DEVMEM_ADDR(*(dpb->current_out->pp_data)));
      }
      if (dpb->current_out->first_field & (dpb->current_out->field_coupled == 0)) {
        dpb->current_out->first_field = 0;
        dpb->current_out->next_poc = 0x7FFFFFFF;
        dpb->current_out->field_coupled = HANTRO_TRUE;
      }
      return NULL;
    }
    dpb->no_free_buffer_cnt = 0;
    // wait fb_list[mem_idx] free, or get new free buffer with n_ref_count ==0
    if (new_id !=
        dpb->current_out->mem_idx) {  // compare new id and old_id in fb_list
      SetFreePicBuffer(dpb->fb_list,
                       dpb->current_out->mem_idx);  // set old_id FREE
      dpb->current_out->mem_idx = new_id;  // mem_idx = new id
      dpb->current_out->data =
          GetDataById(dpb->fb_list, new_id);  // fb_stat[new_id].data address
      dpb->current_out->parasitic_data = &dpb->dpb_parasitic_buf[new_id];
    }
  }

  ASSERT(dpb->current_out->data);
  ASSERT(dpb->current_out->parasitic_data);

 // if (Avs2GetType(dpb) == AVS2_PIC_GB)
  if (dpb->no_reordering ||
      (dpb->typeb == BACKGROUND_IMG && dpb->background_picture_output_flag == 0))
    to_be_displayed = HANTRO_FALSE; //GB
  else
    to_be_displayed = HANTRO_TRUE;//G,.....
  // no_reordering ==0, low_delay, output immediately
  {
    struct Avs2DpbPicture *current_out =
        dpb->current_out;  // store current pic info to dpb->current_out

    // current_out->is_idr = is_idr;
    // current_out->is_tsa_ref = 0;
    current_out->error_ratio = 0;
    current_out->error_info = DEC_NO_ERROR;
    current_out->pic_num = (i32)current_pic_id;
    current_out->pic_id = (i32)current_pic_id;
    current_out->decode_id = (i32)current_pic_id;
    current_out->type = Avs2GetType(dpb);

    // SET_POC(*current_out, cur_poc);
    current_out->img_coi = dpb->coi;  // store COI ==doi
    current_out->img_poi = dpb->poi;  // store POI == poc
    current_out->to_be_displayed = to_be_displayed;
    current_out->refered_by_others = dpb->refered_by_others;
    current_out->is_field_sequence = storage->sps.is_field_sequence;
    current_out->is_top_field = storage->pps.is_top_field;
    current_out->top_field_first = storage->top_field_first;

    DPB_TRACE("[DPB] current picture type: %d (doi=%d poi=%d)\n",
              current_out->type, current_out->img_coi, current_out->img_poi);

    if (current_out->to_be_displayed) {
      dpb->num_out_pics_buffered++;
    }
    dpb->fullness++;
    dpb->num_ref_frames++;
  }

  /* store POCs of ref pic buffer for current pic */
  p = dpb->current_out->ref_poc;  // poc of each buffer for current frame

  if (dpb->type != B_IMG) {
    for (i = 0; i < dpb->num_of_ref; i++)
      *p++ = dpb->buffer[dpb->ref_pic_set_st[i]].img_poi;
  } else {
    *p++ = dpb->buffer[dpb->ref_pic_set_st[0]].img_poi;
    *p++ = dpb->buffer[dpb->ref_pic_set_st[1]].img_poi;
  }

  for (i = dpb->num_of_ref; i < MAXREF; i++) {
    *p++ = 0;  // clear unused reference frame POI
  }

  return dpb->current_out->data;
}

/* This function is only called when DPB are external buffer, and the DPB is sufficient
 * for new sequence and no re-allocating is needed. Just setting new offsets are needed. */
u32 Avs2ReInitDpb(const void *dec_inst, struct Avs2DpbStorage *dpb,
                  struct Avs2DpbInitParams *dpb_params) {
  u32 i;
  struct FrameBufferList *fb_list = dpb->fb_list;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 old_dpb_size = dpb->dpb_size;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  ASSERT(dpb_params->pic_size);
  ASSERT(dpb_params->dpb_size);

  dpb->pic_size = dpb_params->pic_size;

  dpb->max_long_term_frame_idx = NO_LONG_TERM_FRAME_INDICES;

  dpb->real_size = dpb_params->dpb_size;
  dpb->dpb_size = dpb_params->dpb_size + 1;

  dpb->max_ref_frames = dpb_params->dpb_size;
  dpb->no_reordering = dpb_params->no_reordering;
  dpb->fullness = 0;
  (void)Avs2DpbMarkAllUnused(dpb);
  RemoveTempOutputAll(dpb->fb_list, dpb);
  dpb->num_out = 0;
  dpb->out_index_w = dpb->out_index_r = 0;

  dpb->num_out_pics_buffered = 0;
  dpb->num_ref_frames = 0;
  dpb->prev_ref_frame_num = 0;
  dpb->prev_out_idx = INVALID_MEM_IDX;
  dpb->field_first_output=1;

  /* allocate 32 bytes for multicore status fields */
  /* locate it before direct MV */
  /* align sync_mc buffer, storage sync bytes adjoining to dir mv*/
  dpb->dir_mv_offset = NEXT_MULTIPLE(32, ref_buffer_align);
  dpb->sync_mc_offset = dpb->dir_mv_offset - 32;

  if (dpb_params->tbl_sizey) {
    dpb->cbs_tbl_offsety = dpb_params->buff_size - dpb_params->tbl_sizey - dpb_params->tbl_sizec;
    dpb->cbs_tbl_offsetc = dpb_params->buff_size - dpb_params->tbl_sizec;
    dpb->cbs_tbl_size = dpb_params->tbl_sizey + dpb_params->tbl_sizec;
  }

  if (old_dpb_size < dpb->dpb_size) {
    //ReInitList(fb_list);

    for (i = old_dpb_size + 1; i < dpb->dpb_size + 1; i++) {
      /* Find a unused buffer j. */
      u32 j, id;
      for (j = 0; j < MAX_PIC_BUFFERS; j++) {
        u32 found = 0;
        for (u32 k = 0; k < i; k++) {
          if (dpb->pic_buffers[j].bus_address == dpb->buffer[k].data->bus_address) {
            found = 1;
            break;
          }
        }
        if (!found)
          break;
      }
      ASSERT(j < MAX_PIC_BUFFERS);

      dpb->buffer[i].data = dpb->pic_buffers + j;
      dpb->buffer[i].parasitic_data = dpb->dpb_parasitic_buf + j;
      id = GetIdByData(fb_list, (void *)dpb->buffer[i].data);
      MarkIdAllocated(fb_list, id);
      dpb->buffer[i].mem_idx = id;
      dpb->pic_buff_id[j] = id;
    }
  } else if (old_dpb_size > dpb->dpb_size) {
    for (i = dpb->dpb_size + 1; i < old_dpb_size + 1; i++) {
      /* Remove extra dpb buffers from DPB. */
      MarkIdFree(fb_list, dpb->buffer[i].mem_idx);
    }
  }
  return (HANTRO_OK);
}

u32 Avs2InitDpb(const void *dec_inst, struct Avs2DpbStorage *dpb,
                struct Avs2DpbInitParams *para) {
  u32 i;
  struct FrameBufferList *fb_list = dpb->fb_list;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  u32 ref_buffer_align = MAX(16, ALIGN(dec_cont->align));

  ASSERT(para->pic_size);
  ASSERT(para->dpb_size);

  /* we trust our memset; ignore return value */
  (void)DWLmemset(dpb, 0, sizeof(*dpb)); /* make sure all is clean */
  (void)DWLmemset(dpb->pic_buff_id, FB_NOT_VALID_ID, sizeof(dpb->pic_buff_id));

  dpb->storage = &dec_cont->storage;

  /* restore value */
  dpb->fb_list = fb_list;

  dpb->pic_size = para->pic_size;
  dpb->max_long_term_frame_idx = NO_LONG_TERM_FRAME_INDICES;

  dpb->real_size = para->dpb_size;
  dpb->dpb_size = para->dpb_size + 1;

  dpb->max_ref_frames = para->dpb_size;
  dpb->no_reordering = para->no_reordering;
  dpb->fullness = 0;
  dpb->num_out_pics_buffered = 0;
  dpb->num_ref_frames = 0;
  dpb->prev_ref_frame_num = 0;
  dpb->num_out = 0;
  dpb->out_index_w = 0;
  dpb->out_index_r = 0;
  dpb->prev_out_idx = INVALID_MEM_IDX;
  dpb->field_first_output=1;

  dpb->tot_buffers = dpb->dpb_size + 2 + para->n_extra_frm_buffers;  // tot_buffers > dpb->dpb_size + 2
  /* figure out extra buffers for multicore */
  if (para->n_cores > 1) {
    /* one extra buffer for each core */
    /* do not allocate twice for multiview */
    dpb->tot_buffers += para->n_cores;
  }
#if defined(ASIC_TRACE_SUPPORT) && defined(SUPPORT_MULTI_CORE)
  dpb->tot_buffers += 10;
#endif

  if (dpb->tot_buffers > MAX_PIC_BUFFERS)
    dpb->tot_buffers = MAX_PIC_BUFFERS;
  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) && (!dec_cont->reset_ext_buffer)) {
    dec_cont->min_buffer_num = dpb->dpb_size + 2;   /* We need at least (dpb_size+2) output buffers before starting decoding. */
    dec_cont->buffer_num_added = 0;
  } else
    dec_cont->min_buffer_num = dpb->dpb_size + 1;

  dpb->dir_mv_offset = NEXT_MULTIPLE(32, ref_buffer_align);
  dpb->sync_mc_offset = dpb->dir_mv_offset - 32;
  dpb->dpb_parasitic_buf_size = para->dpb_parasitic_buf_size;

  if (para->tbl_sizey) {
    dpb->cbs_tbl_offsety = para->buff_size - para->tbl_sizey - para->tbl_sizec;
    dpb->cbs_tbl_offsetc = para->buff_size - para->tbl_sizec;
    dpb->cbs_tbl_size = para->tbl_sizey + para->tbl_sizec;
  }
  dpb->dpb_reset = 1;
  if (!dpb->out_buf) {
    dpb->out_buf = DWLmalloc((dpb->dpb_size) * sizeof(struct Avs2DpbOutPicture));

    if (dpb->out_buf == NULL) {
      return (MEMORY_ALLOCATION_ERROR);
    }
  }

   if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    for (i = 0; i < dpb->tot_buffers; i++) {
      /* yuv picture + direct mode motion vectors */
      /* TODO(min): request external buffers. */
      if (!DWL_DEVMEM_VAILD(dpb->pic_buffers[i])) {
        dec_cont->next_buf_size = para->buff_size;
        dec_cont->buf_type = REFERENCE_BUFFER;
        dec_cont->buf_num = dpb->tot_buffers;
        dec_cont->buffer_index = i;
        dec_cont->release_buffer = 1;
        return DEC_WAITING_FOR_BUFFER;
      }
    }
  } else {
    int max_size = MIN(MAX_DPB_SIZE, dpb->dpb_size);
    for (i = 0; i < dpb->tot_buffers; i++) {
      /* pic_buffers[MAX_PIC_BUFFERS]: each buffer size is yuv picture +
       * direct mode motion vectors */
      dpb->pic_buffers[i].mem_type = DWL_MEM_TYPE_DPB | DWL_MEM_TYPE_DMA_DEVICE_ONLY;
      SET_MEM_USAGE(dpb->pic_buffers[i].mem_type, DWL_MEM_USAGE_OUT_REFERENCE,
                    dec_cont->secure_mode);
      if (DWL_OK != DWLMallocRefFrm(dec_cont->dwl, para->buff_size, dpb->pic_buffers + i))
        return (MEMORY_ALLOCATION_ERROR);
      if (DWL_OK != Avs2AllocParasiticBuf(dec_cont->dwl, para->dpb_parasitic_buf_size,
                                          dpb->dpb_parasitic_buf + i, dec_cont->secure_mode))
        return (MEMORY_ALLOCATION_ERROR);

      if (i < max_size + 1 ) {
        u32 id = AllocateIdUsed(
            dpb->fb_list,
            dpb->pic_buffers + i);  // search for 1 UNALLOCATED id in fb_list
        if (id == FB_NOT_VALID_ID)
          return MEMORY_ALLOCATION_ERROR;  // id in fb_list is set to FB_ALLOCATED

        dpb->buffer[i].data = dpb->pic_buffers + i;
        dpb->buffer[i].parasitic_data = dpb->dpb_parasitic_buf + i;
        dpb->buffer[i].mem_idx = id;  // set id to buffer mem_idx
        dpb->pic_buff_id[i] = id;

        dpb->buffer[i].to_be_displayed = HANTRO_FALSE;
        dpb->buffer[i].img_coi = -257;
        dpb->buffer[i].img_poi = -256;
        dpb->buffer[i].refered_by_others = 0;
        dpb->buffer[i].next_poc = 0x7FFFFFFF;
        dpb->buffer[i].first_field = 0;
        dpb->buffer[i].field_coupled = HANTRO_TRUE;

      } else {  // if i >= dpb->dpb_size + 1
        u32 id = AllocateIdFree(
            dpb->fb_list, dpb->pic_buffers + i);  // search for 1 UNALLOCATED id
        if (id == FB_NOT_VALID_ID)
          return MEMORY_ALLOCATION_ERROR;  // id in fb_list is set to FB_FREE

        dpb->pic_buff_id[i] = id;  // set id to pic_buff_id
      }
    }
  }
  return (HANTRO_OK);
}


/* Reset the DPB. This function should be called when an IDR slice (other than
 * the first) activates new sequence parameter set.  Function calls
 * Avs2FreeDpb to free old allocated memories and Avs2InitDpb to
 * re-initialize the DPB. Same inputs, outputs and returns as for
 * Avs2InitDpb. */
u32 Avs2ResetDpb(const void *dec_inst, struct Avs2DpbStorage *dpb,
                 struct Avs2DpbInitParams *dpb_params) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  dpb->dpb_reset = 0;

  // Only reset DPB once for a sequence.
  if (dec_cont->reset_dpb_done)
    return HANTRO_OK;

  if ((!dec_cont->use_adaptive_buffers && dpb->pic_size == dpb_params->pic_size) ||
      (dec_cont->use_adaptive_buffers &&
       ((IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
         dec_cont->ext_buffer_size >= dpb_params->buff_size) ||
        (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
         dpb->pic_size >= dpb_params->pic_size)))) {
    dpb->max_long_term_frame_idx = NO_LONG_TERM_FRAME_INDICES;

    dpb->no_reordering = dpb_params->no_reordering;
    dpb->flushed = 0;

    /*
     * 1. DPB are external buffers,
     *    a) no adaptive method: dpb size are same as old ones;
     *    b) use new adaptive method, there are enough "guard buffers" added;
     * 2. DPB are internal buffers, and number of dpb buffer are same.
     */
    if ((IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
         ((!dec_cont->use_adaptive_buffers && dpb->real_size == dpb_params->dpb_size) ||
          (dec_cont->use_adaptive_buffers && !dec_cont->reset_ext_buffer))) ||
        (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER) &&
         dpb->real_size == dpb_params->dpb_size)) {
      /* number of pictures and DPB size are not changing */
      /* no need to reallocate DPB */
      dec_cont->reset_dpb_done = 1;
      Avs2ReInitDpb(dec_inst, dpb, dpb_params);
      return (HANTRO_OK);
    }
  }

  dec_cont->reset_dpb_done = 1;
  Avs2FreeDpbExt(dec_inst, dpb);

  return Avs2InitDpb(dec_inst, dpb, dpb_params);
}

/* Determines picture order counts of the reference pictures (of current
 * and subsequenct pictures) based on delta values in ref pic sets. */
void Avs2SetRefPicPocList(struct Avs2DpbStorage *dpb, struct Avs2PicParam *pps,
                          struct Avs2SeqParam *sps, i32 pic_order_cnt) {

  u32 i;
#if 0
  if (pps->type == I_IMG && pps->typeb == BACKGROUND_IMG) { // G/GB frame
        dpb->num_of_ref = 0;
  } else if (pps->type == P_IMG && pps->typeb == BP_IMG) { // only one reference frame(G\GB) for S frame
        dpb->num_of_ref = 1;
  }
#endif
  if ((dpb->dpb_overflow_cnt == 0) && (dpb->no_free_buffer_cnt == 0)) {
    DPB_TRACE("%s\n","[DPB] RPS reference:(");
    for (i = 0; i < pps->rps.num_of_ref; i++) {
      DPB_TRACE_NP("%d ", pps->coding_order - pps->rps.ref_pic[i]);
    }
    if (pps->background_reference_enable) DPB_TRACE("%s\n","+BACKBROUND");
    DPB_TRACE_NP("%s",")\n");
    DPB_TRACE("%s\n","[DPB] RPS    remove:(");
    for (i = 0; i < pps->rps.num_to_remove; i++) {
      DPB_TRACE_NP("%d ", pps->coding_order - pps->rps.remove_pic[i]);
    }
    DPB_TRACE_NP("%s\n",")");
  }

  for (i = 0; i < pps->rps.num_of_ref;
       i++) {  // DOI  = DOI -DeltaDOI[i], 9.2.3.b
    dpb->poc_st_curr[i] =
        pps->coding_order - pps->rps.ref_pic[i];  // store DOI in poc_st_curr
  }
  dpb->num_poc_st_curr = i;

  if (pps->background_reference_enable) {
    /* use background frame as long term. */
    dpb->num_poc_lt_curr = 1;
    dpb->poc_lt_curr[0] = 0;
    dpb->num_poc_lt_foll = 0;
  } else {
    /* not use background frame as long term. */
    dpb->num_poc_lt_curr = 0;
    dpb->num_poc_lt_foll = 0;
  }
}


void Avs2SetPOC(struct Avs2DpbStorage *dpb, struct Avs2PicParam *pps,
                struct Avs2SeqParam *sps) {
  int i, j;

  dpb->poi = pps->poc;  // current poi
  dpb->coi = pps->coding_order;  // current coi
  dpb->typeb = pps->typeb;
  dpb->type = pps->type;
  dpb->picture_reorder_delay = sps->picture_reorder_delay;
  dpb->displaydelay = pps->displaydelay;
  dpb->refered_by_others = pps->rps.referd_by_others;
  dpb->background_picture_enable = sps->background_picture_enable;
  dpb->background_picture_output_flag = pps->background_picture_output_flag;
  dpb->low_delay = sps->low_delay;
  dpb->num_of_ref = pps->rps.num_of_ref;

  dpb->num_to_remove = pps->rps.num_to_remove;
  for (i = 0; i < MAXREF; i++) dpb->remove_pic[i] = pps->rps.remove_pic[i];

  dpb->pic_width = sps->pic_width_in_cbs * 8;
  dpb->pic_height = sps->pic_height_in_cbs * 8;
  dpb->sample_bit_depth = sps->sample_bit_depth;
  dpb->output_bit_depth = sps->output_bit_depth;

  // update doi and poi, calc_picture_distance
  if (dpb->coi < dpb->prev_coi) {
    for (i = 0; i <= dpb->dpb_size; i++) {
      if (dpb->buffer[i].img_poi >=
          0) {  // all reference POI = POI - 256,  DOI = DOI -256
        dpb->buffer[i].img_poi -= 256;
        dpb->buffer[i].img_coi -= 256;
        dpb->buffer[i].next_poc -= 256;
      }
      for (j = 0; j < MAXREF; j++) {
        dpb->buffer[i].ref_poc[j] -=
            256;  // all reference frame's POI = POI - 256
      }
    }

    dpb->last_poi -= 256;
    dpb->curr_idr_poi -= 256;  // IDR DOI
    dpb->curr_idr_coi -= 256;  // IDR POI
  }

  if (sps->new_sequence_flag) {
    dpb->curr_idr_poi = pps->poc;
    dpb->curr_idr_coi = pps->coding_order;
    sps->new_sequence_flag = 0;
  }
  dpb->prev_coi = (pps->coding_order % 256);
}

/* Determines reference pictures for current pictures. */
u32 Avs2SetRefPics(struct Avs2DpbStorage *dpb, struct Avs2SeqParam *sps,
                   struct Avs2PicParam *pps) {

  u32 i,j;
  i32 idx = 0;
  u32 st_count[MAX_DPB_SIZE + 1] = {0};
  u32 ret = DEC_OK;
  struct Avs2DpbPicture temp_buff;

  Avs2SetPOC(dpb, pps, sps);  // prepare_RefInfo

  //re-order
  if (dpb->refered_by_others) {
    if (dpb->typeb == BACKGROUND_IMG && dpb->background_picture_enable==0) {
      ;
    }
    else
    {
      for(i=0;i<pps->rps.num_of_ref;i++)
      {
        temp_buff = dpb->buffer[i];
        for(j=i;j<dpb->dpb_size;j++)
        {
          if(dpb->buffer[j].img_coi == dpb->coi - pps->rps.ref_pic[i])
            break;
        }
        if(j!=dpb->dpb_size) //(j!=dpb->dpb_size + 1)
        {
          dpb->buffer[i] = dpb->buffer[j];
          dpb->buffer[j] = temp_buff;
        }
      }
    }
  }
  Avs2SetRefPicPocList(dpb, pps, sps,
                       pps->poc);  // caculate reference doi from header

  for (i = 0; i < dpb->num_poc_st_curr; i++) {
    idx = Avs2FindDpbPic(dpb, dpb->poc_st_curr[i]);  // compare DOI
//    if (idx == -1)
    {
      //idx = Avs2FindDpbPicLong(dpb, dpb->poc_st_curr[i]);  // compare DOI
      if (idx == -1) {
        ;//ret = DEC_PARAM_ERROR;  // not found
        //printf("refer frame dpb idx=%d,ref coi=%d,dpb->buffer[i].img_poi == pps->poc err\n",idx,dpb->poc_st_curr[i]);
        continue;
      }
    }
    if(dpb->buffer[i].img_poi == pps->poc)
    {
       //printf("refer frame dpb idx=%d,ref coi=%d\n",idx,dpb->poc_st_curr[i]);
      ;//ret = DEC_PARAM_ERROR;
    }
    dpb->ref_pic_set_st[i] = idx;  // buffer idx
    st_count[idx]++;
  }

  /* mark pics in dpb */
  for (i = 0; i <= dpb->dpb_size; i++) {
    if ((st_count[i]) && ((!IsLongTerm(&dpb->buffer[i])) && (!IsShortLongTerm(&dpb->buffer[i]))))
      SET_STATUS(dpb->buffer[i], SHORT_TERM);  // mark dpb->buffer[i] short term
  }

  // find background ref with long term
  for (i = 0; i <= dpb->dpb_size; i++) {
    if (IsLongTerm(&dpb->buffer[i]) || IsShortLongTerm(&dpb->buffer[i]))
    {
      dpb->ref_pic_set_st[MAXREF] = i;  // long term at pos MAXREF
    }
  }

  return ret;
}

/* Find a picture from the buffer. The picture to be found is
 * identified by poc */
static i32 Avs2FindDpbPic(struct Avs2DpbStorage *dpb, i32 doi) {
  u32 i = 0;
  //? should have "=" ?
  while (i <= dpb->dpb_size) {
    if ((dpb->buffer[i].img_coi == doi)&&
        IS_REFERENCE(dpb->buffer[i])&&dpb->buffer[i].type!=AVS2_PIC_GB) {
      return i;
    }
    i++;
  }
#if 1
  //error concealment
  i = 0;
  while (i <= dpb->dpb_size) {
    if (IS_EXISTING(dpb->buffer[i])) {
      return i;
    }
    i++;
  }
#endif
  if(dpb->type==I_IMG)
    return 0;
  else
    return (-1);

}
#if 0
static i32 Avs2FindDpbPicLong(struct Avs2DpbStorage *dpb, i32 doi) {
  u32 i = 0;
//? should have "=" ?
  while (i < dpb->dpb_size) {
    if ((dpb->buffer[i].img_coi == doi)&&
        IS_LONG_TERM(dpb->buffer[i])) {
      return i;
    }
    i++;
  }
#if 0
  //error concealment
  i = 0;
  while (i < dpb->dpb_size) {
    if (IS_EXISTING(dpb->buffer[i])) {
      return i;
    }
    i++;
  }
#endif
  if(dpb->type==I_IMG)
    return 0;
  else
    return (-1);

}
#endif

/* Finds picture with smallest picture order count. This will be the next
 * picture in display order. line 2227 in image.c */
struct Avs2DpbPicture *Avs2FindSmallestPicOrderCnt(struct Avs2DpbStorage *dpb) {
  u32 i;
  i32 poi_min;
  struct Avs2DpbPicture *tmp;
  i32 poi_tmp;
//  i32 poi_cur;
  i32 pos = -1;

  ASSERT(dpb);

  poi_min = 0x7FFFFFFF;  // set to max
  tmp = NULL;

  for (i = 0; i <= dpb->dpb_size; i++) {
    poi_tmp = GET_POC(dpb->buffer[i]);
    /* TODO: currently only outputting frames, asssumes that fields of a
     * frame are output consecutively */
    if ((dpb->buffer[i].to_be_displayed &&  // to_be_displayed == 1
        (poi_tmp < poi_min))&&
        (((poi_tmp + dpb->picture_reorder_delay) <= dpb->coi) ||
         (dpb->flushed == 1)||(dpb->edit_code_flush == 1))) {
      tmp = dpb->buffer + i;
      poi_min = poi_tmp;
      pos = i;
    }
  }

  if (pos != -1) {
    dpb->last_poi = poi_min;

    /* remove it from DPB, write_frame */
    tmp->to_be_displayed = HANTRO_FALSE;
    if ((IsShortTerm(tmp)) &&
        ((tmp->refered_by_others == 0 || tmp->img_coi == -257))&&
        !(tmp->is_field_sequence&&tmp->first_field&& tmp->field_coupled==0)) {
      tmp->img_poi = -256;  // set poc to -256, output order index
      tmp->img_coi = -257;  // set decode order index to invalid,
    }
    DPB_TRACE("[DPB] output %d frame (doi=%d poi=%d)\n", pos, tmp->img_coi,
              tmp->img_poi);

    if (dpb->flushed == 1||(dpb->edit_code_flush == 1)) {
      tmp->refered_by_others = 0;
      SetStatus(tmp, UNUSED);
      /* remove frames which have been outputted */
      if (tmp->to_be_displayed == HANTRO_FALSE) {
        tmp->img_poi = -256;
        tmp->img_coi = -257;
        Avs2DpbBufFree(dpb, pos);
      }
      DPB_TRACE("[DPB] flush as unused %d frame (doi=%d poi=%d)\n", pos,
                tmp->img_coi, tmp->img_poi);
    }
  }
  return (tmp);
}

/* Function to put next display order picture into the output buffer. */
u32 Avs2OutputPicture(struct Avs2DpbStorage *dpb) {
  struct Avs2DpbPicture *tmp;
  struct Avs2DpbOutPicture *dpb_out;
  u32 output_field = 0;
  ASSERT(dpb);

  if (dpb->no_reordering) return (HANTRO_NOK);
  tmp = Avs2FindSmallestPicOrderCnt(dpb);

  /* no pictures to be displayed */
  if (tmp == NULL) return (HANTRO_NOK);
  if(tmp->is_field_sequence)
  {
    if(dpb->field_first_output==1)
    {

      if(tmp->top_field_first==0)
      {
        output_field=1;
      }
      else
      {
        if(tmp->first_field&&tmp->field_coupled==HANTRO_TRUE)
        {
          tmp->next_poc = 0x7FFFFFFF;
          //tmp->first_field = 0;
          dpb->num_out_pics_buffered--;
          if (!IS_REFERENCE(*tmp)) {
            if (dpb->fullness > 0) dpb->fullness--;
          }
          return (HANTRO_OK);
        }
        if(tmp->first_field&&(!((dpb->flushed == 1)||(dpb->edit_code_flush == 1))))
        {
          dpb->num_out_pics_buffered--;
          if (!IS_REFERENCE(*tmp)) {
            if (dpb->fullness > 0) dpb->fullness--;
          }
          return (HANTRO_OK);
        }
      }
      dpb->field_first_output=0;
    }
    else
    {
      if(tmp->first_field&&tmp->field_coupled==HANTRO_TRUE)
      {
        tmp->next_poc = 0x7FFFFFFF;
        //tmp->first_field = 0;
        dpb->num_out_pics_buffered--;
        if (!IS_REFERENCE(*tmp)) {
          if (dpb->fullness > 0) dpb->fullness--;
        }
        return (HANTRO_OK);
      }
      if(tmp->first_field&&(!((dpb->flushed == 1)||(dpb->edit_code_flush == 1))))
      {
        dpb->num_out_pics_buffered--;
        if (!IS_REFERENCE(*tmp)) {
          if (dpb->fullness > 0) dpb->fullness--;
        }
        return (HANTRO_OK);
      }
    }
  }

  /* if output buffer full -> ignore oldest */
  if (dpb->num_out == dpb->dpb_size + 1) {
    // TODO(atna) figure out how to safely handle output overflow

    /* it seems that output overflow can occur with corrupted streams
     * and display smoothing mode
     */
    ClearOutput(dpb->fb_list,
                dpb->out_buf[dpb->out_index_r]
                    .mem_idx);  // clear out_buf[dpb->out_index_r], oldest

    dpb->out_index_r++;  // next oldest
    if (dpb->out_index_r == dpb->dpb_size) dpb->out_index_r = 0;
    dpb->num_out--;
  }
  /* remove it from DPB */
 // tmp->to_be_displayed = HANTRO_FALSE;
//  if (tmp->pic_output_flag)
  dpb->num_out_pics_buffered--;


  /* updated output list */
  dpb_out = &dpb->out_buf[dpb->out_index_w]; /* next output */
  dpb_out->pp_data = tmp->pp_data;
  dpb_out->data = tmp->data;
  dpb_out->parasitic_data = tmp->parasitic_data;
  dpb_out->type = tmp->type;
  dpb_out->is_tsa_ref = tmp->is_tsa_ref;
  dpb_out->pic_id = tmp->pic_id;
  dpb_out->decode_id = tmp->decode_id;
  dpb_out->mem_idx = tmp->mem_idx;
  dpb_out->tiled_mode = tmp->tiled_mode;
  dpb_out->cycles_per_mb = tmp->cycles_per_mb;
  dpb_out->error_ratio = tmp->error_ratio;
  dpb_out->error_info = tmp->error_info;

  dpb_out->pic_width = dpb->pic_width;
  dpb_out->pic_height = dpb->pic_height;
  dpb_out->crop_params = dpb->crop_params;
  dpb_out->sample_bit_depth = dpb->sample_bit_depth;
  dpb_out->output_bit_depth = dpb->output_bit_depth;
  dpb_out->is_field_sequence = tmp->is_field_sequence;
  dpb_out->is_top_field = tmp->is_top_field;
  dpb_out->top_field_first = tmp->top_field_first;
  dpb_out->output_field=output_field;
  tmp->next_poc = 0x7FFFFFFF;
  tmp->first_field = 0;
  tmp->field_coupled = HANTRO_TRUE;
  dpb->num_out++;
  dpb->out_index_w++;
  if (dpb->out_index_w == dpb->dpb_size) dpb->out_index_w = 0;

  if (!IS_REFERENCE(*tmp)) {
    if (dpb->fullness > 0) dpb->fullness--;
  }

  MarkTempOutput(dpb->fb_list, tmp->mem_idx);

  return (HANTRO_OK);
}

/* Get next display order picture from the output buffer. */
struct Avs2DpbOutPicture *Avs2DpbOutputPicture(struct Avs2DpbStorage *dpb) {
  u32 tmp_idx;
  DEBUG_PRINT(("Avs2DpbOutputPicture: index %u outnum %u\n",
               (dpb->num_out -
                    ((dpb->out_index_w - dpb->out_index_r + dpb->dpb_size) %
                     (dpb->dpb_size))),
                dpb->num_out));
  if (dpb->num_out) {
    tmp_idx = dpb->out_index_r++;
    if (dpb->out_index_r == dpb->dpb_size) dpb->out_index_r = 0;
    dpb->num_out--;
    dpb->prev_out_idx = dpb->out_buf[tmp_idx].mem_idx;

    return (dpb->out_buf + tmp_idx);
  } else
    return (NULL);
}

/* Flush the DPB. Function puts all pictures needed for display into the
 * output buffer. This function shall be called in the end of the stream to
 * obtain pictures buffered for display re-ordering purposes. */
void Avs2FlushDpb(struct Avs2DpbStorage *dpb) {
  dpb->flushed = 1;
  /* output all pictures */
  while (Avs2OutputPicture(dpb) == HANTRO_OK)
    ;
}

i32 Avs2FreeDpbExt(const void *dec_inst, struct Avs2DpbStorage *dpb) {

  u32 i;
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  ASSERT(dpb);

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    /* Client will make sure external memory to be freed.*/
    for (i = 0; i < dpb->tot_buffers; i++) {
      if (DWL_DEVMEM_VAILD(dpb->pic_buffers[i])) {
        if (dpb->pic_buff_id[i] != FB_NOT_VALID_ID)
          ReleaseId(dpb->fb_list, dpb->pic_buff_id[i]);
        /*
                dec_cont->next_buf_size = 0;
                return DEC_WAITING_FOR_BUFFER;
        */
      }
    }
  } else {
    for (i = 0; i < dpb->tot_buffers; i++) {
      if (DWL_DEVMEM_VAILD(dpb->pic_buffers[i])) {
        DWLFreeRefFrm(dec_cont->dwl, dpb->pic_buffers + i);
        if (dpb->pic_buff_id[i] != FB_NOT_VALID_ID)
          ReleaseId(dpb->fb_list, dpb->pic_buff_id[i]);
      }
      if (DWL_DEVMEM_VAILD(dpb->dpb_parasitic_buf[i]))
         Avs2ReleaseParasiticBuf(dec_cont->dwl, dpb->dpb_parasitic_buf + i);
    }
  }

  return 0;
}

i32 Avs2FreeDpb(const void *dec_inst, struct Avs2DpbStorage *dpb){

  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;

  u32 i;

  ASSERT(dpb);

  for (i = 0; i < dpb->tot_buffers; i++) {
    if (DWL_DEVMEM_VAILD(dpb->pic_buffers[i])) {
      if (!IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER))
        DWLFreeRefFrm(dec_cont->dwl, dpb->pic_buffers + i);
      if (dpb->pic_buff_id[i] != FB_NOT_VALID_ID)
        ReleaseId(dpb->fb_list, dpb->pic_buff_id[i]);
    }
    if (DWL_DEVMEM_VAILD(dpb->dpb_parasitic_buf[i]))
        Avs2ReleaseParasiticBuf(dec_cont->dwl, dpb->dpb_parasitic_buf + i);
  }

  if (dpb->out_buf != NULL) {
    DWLfree(dpb->out_buf);
    dpb->out_buf = NULL;
  }

  return 0;
}

/* Updates DPB ref frame count (and fullness) after marking a ref pic as
 * unused */
void Avs2DpbBufFree(struct Avs2DpbStorage *dpb, u32 i) {
  dpb->num_ref_frames--;

  if (!dpb->buffer[i].to_be_displayed) {
    if (dpb->fullness > 0) dpb->fullness--;
  }
}

/* Marks all reference pictures as unused and outputs all the pictures. */
u32 Avs2DpbMarkAllUnused(struct Avs2DpbStorage *dpb) {
  u32 i;

  if (dpb->edit_code_flush) {
    for (i = 0; i <= dpb->dpb_size; i++) {
      if (IS_REFERENCE(dpb->buffer[i])) {
        SET_STATUS(dpb->buffer[i], UNUSED);
        dpb->buffer[i].refered_by_others=0;
        Avs2DpbBufFree(dpb, i);
      }
    }
  }

  /* output all pictures */
  while (Avs2OutputPicture(dpb) == HANTRO_OK)
    ;

  if (dpb->edit_code_flush) {
    dpb->num_ref_frames = 0;
    dpb->max_long_term_frame_idx = NO_LONG_TERM_FRAME_INDICES;
    dpb->prev_ref_frame_num = 0;
  }
  return (HANTRO_OK);
}

void Avs2EmptyDpb(const void *dec_inst, struct Avs2DpbStorage *dpb) {
  struct Avs2DecContainer *dec_cont = (struct Avs2DecContainer *)dec_inst;
  i32 i;

  for (i = 0; i <= dpb->dpb_size; i++) {
    if (dpb->buffer[i].to_be_displayed == HANTRO_TRUE) {
      /* If the allocated buffer is still in DPB and not output to
       * dpb->out_buf by OutputPicture yet, return the coupled
       * raster/downscal buffer. */
      if (dpb->storage->pp_enabled) {
        InputQueueReturnBuffer(dpb->storage->pp_buffer_queue,
                          DWL_GET_DEVMEM_ADDR(*(dpb->buffer[i].pp_data)));
      }
    }
    SET_STATUS(dpb->buffer[i], UNUSED);
    dpb->buffer[i].to_be_displayed = 0;
    dpb->buffer[i].error_ratio = 0;
    dpb->buffer[i].error_info = DEC_NO_ERROR;
    dpb->buffer[i].pic_num = 0;
    dpb->buffer[i].img_poi = -256;
    dpb->buffer[i].img_coi = -257;
    dpb->buffer[i].pic_order_cnt_lsb = 0;
    dpb->buffer[i].type = 0;
    dpb->buffer[i].is_tsa_ref = 0;
    dpb->buffer[i].cycles_per_mb = 0;
    dpb->buffer[i].error_ratio = 0;
    dpb->buffer[i].error_info = DEC_NO_ERROR;
    dpb->buffer[i].pic_struct = 0;
    dpb->buffer[i].dpb_output_time = 0;
    dpb->buffer[i].next_poc = 0x7fffffff;
    dpb->buffer[i].first_field = 0;
    dpb->buffer[i].field_coupled = 1;


#ifdef USE_OMXIL_BUFFER
    dpb->buffer[i].pp_data = NULL;
    if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
      dpb->buffer[i].mem_idx = INVALID_MEM_IDX;
      dpb->buffer[i].data = NULL;
      dpb->buffer[i].parasitic_data = NULL;
    }
#endif
  }

  RemoveTempOutputAll(dpb->fb_list, dpb);
  RemoveOutputAll(dpb->fb_list, dpb);

#ifdef USE_OMXIL_BUFFER
  if (dpb->storage && dpb->storage->pp_enabled)
    InputQueueReturnAllBuffer(dpb->storage->pp_buffer_queue);

  if (IS_EXTERNAL_BUFFER(dec_cont->ext_buffer_config, REFERENCE_BUFFER)) {
    for (i = 0; i < dpb->tot_buffers; i++) {
      if (DWL_DEVMEM_VAILD(dpb->pic_buffers[i])) {
        if (dpb->pic_buff_id[i] != FB_NOT_VALID_ID) {
          ReleaseId(dpb->fb_list, dpb->pic_buff_id[i]);
        }
      }
    }
    dpb->fb_list->free_buffers = 0;
  }
#endif

  ResetOutFifoInList(dpb->fb_list);

  dpb->current_out = NULL;
  dpb->current_out_pos = 0;
  dpb->cpb_removal_time = 0;
  dpb->bumping_flag = 0;
  dpb->num_out = 0;
  dpb->out_index_w = 0;
  dpb->out_index_r = 0;
  dpb->dpb_reset = 0;
  dpb->max_long_term_frame_idx = NO_LONG_TERM_FRAME_INDICES;
  dpb->num_ref_frames = 0;
  dpb->fullness = 0;
  dpb->num_out_pics_buffered = 0;
  dpb->prev_ref_frame_num = 0;
  dpb->flushed = 0;
  dpb->edit_code_flush = 0;
  dpb->field_first_output=1;
  dpb->prev_out_idx = INVALID_MEM_IDX;
#ifdef USE_OMXIL_BUFFER
  dpb->tot_buffers = dpb->dpb_size + 2 + dec_cont->storage.n_extra_frm_buffers;
  /* figure out extra buffers for multicore */
  if (dec_cont->hwdec.n_cores > 1) {
    /* one extra buffer for each core */
    /* do not allocate twice for multiview */
    dpb->tot_buffers += dec_cont->hwdec.n_cores;
  }

  if (dpb->tot_buffers > MAX_PIC_BUFFERS)
    dpb->tot_buffers = MAX_PIC_BUFFERS;
#endif

  dpb->num_poc_st_curr = 0;
  dpb->num_poc_st_curr_before = 0;
  dpb->num_poc_st_foll = 0;
  dpb->num_poc_lt_curr = 0;
  dpb->num_poc_lt_foll = 0;

  (void)DWLmemset(dpb->poc_lt_curr, 0, sizeof(dpb->poc_lt_curr));
  (void)DWLmemset(dpb->poc_lt_foll, 0, sizeof(dpb->poc_lt_foll));
  (void)DWLmemset(dpb->poc_st_curr, 0, sizeof(dpb->poc_st_curr));
  (void)DWLmemset(dpb->poc_st_foll, 0, sizeof(dpb->poc_st_foll));
  (void)DWLmemset(dpb->ref_pic_set_lt, 0, sizeof(dpb->ref_pic_set_lt));
  (void)DWLmemset(dpb->ref_pic_set_st, 0, sizeof(dpb->ref_pic_set_st));
#ifdef USE_OMXIL_BUFFER
  if (dpb->storage && dpb->storage->pp_enabled)
    InputQueueWaitNotUsed(dpb->storage->pp_buffer_queue);
#endif

  if (dpb->storage && dpb->storage->pp_enabled)
    InputQueueReset(dpb->storage->pp_buffer_queue);
  (void)dec_cont;
}

void Avs2ReleaseParasiticBuf(const void *dwl, struct DWLLinearMem *info) {

  DWLFREE_LINEAR_MEM2(dwl, info);
  DWLmemset(info, 0, sizeof(*info));
  return;
}

i32 Avs2AllocParasiticBuf(const void *dwl, u32 size, struct DWLLinearMem *info, u32 secure_mode) {
  if (size == 0)
    return DEC_PARAM_ERROR;
  info->mem_type = DWL_MEM_TYPE_DMA_DEVICE_ONLY;
  SET_MEM_USAGE(info->mem_type, DWL_MEM_USAGE_TMP_DIRMV, secure_mode);
  if (DWLMALLOC_LINEAR_MEM2(dwl, size, info) != 0)
    return DEC_MEMFAIL;
  return DWL_OK;
}