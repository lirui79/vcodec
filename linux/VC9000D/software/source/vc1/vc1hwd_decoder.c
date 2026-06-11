/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#include "vc1decapi.h"
#include "vc1hwd_decoder.h"
#include "vc1hwd_stream.h"
#include "vc1hwd_picture_layer.h"
#include "regdrv.h"
#include "bqueue.h"
#include "vc1hwd_asic.h"
#include "input_queue.h"
#include "vc1hwd_storage.h"
#include "sw_util.h"
#include <string.h>
#include "dec_log.h"

/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#define SHOW1(p) (p[0]); p+=1;
#define SHOW4(p) (p[0]) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); p+=4;

#define    BIT0(tmp)  ((tmp & 1)   >>0);
#define    BIT1(tmp)  ((tmp & 2)   >>1);
#define    BIT2(tmp)  ((tmp & 4)   >>2);
#define    BIT3(tmp)  ((tmp & 8)   >>3);
#define    BIT4(tmp)  ((tmp & 16)  >>4);
#define    BIT5(tmp)  ((tmp & 32)  >>5);
#define    BIT6(tmp)  ((tmp & 64)  >>6);
#define    BIT7(tmp)  ((tmp & 128) >>7);

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

static u16x ValidateMetadata(swStrmStorage_t * storage, const struct DecMetaData *p_meta_data,
                             const struct DecHwFeatures *hw_feature);
u16x AllocateMemories( decContainer_t *dec_cont,
                       swStrmStorage_t *storage,
                       const void *dwl );

/*------------------------------------------------------------------------------
    Functions
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Function name: vc1hwdInit

        Functional description:
                    Initializes decoder and allocates memories.

        Inputs:
            dwl             Instance of decoder wrapper layer
            storage        Stream storage descriptor
            p_meta_data       Pointer to metadata information

        Outputs:
            None

        Returns:
            VC1HWD_METADATA_ERROR / VC1HWD_OK

------------------------------------------------------------------------------*/
u16x vc1hwdInit(const void *dwl, swStrmStorage_t *storage,
                const struct DecMetaData *p_meta_data,
                u32 num_frame_buffers, const struct DecHwFeatures *hw_feature) {
  u16x rv;

  ASSERT(storage);
  ASSERT(p_meta_data);
  UNUSED(dwl);

  /* check initialization data if not advanced profile stream */
  if (p_meta_data->profile != VC1_ADVANCED_PROFILE) {
    rv = ValidateMetadata(storage, p_meta_data, hw_feature);
    if (rv != VC1HWD_OK)
      return (VC1HWD_METADATA_ERROR);
  }
  /* Advanced profile */
  else {
    storage->max_bframes = 7;
    storage->profile = VC1_ADVANCED;
  }

  if( storage->max_bframes > 0 )
    storage->min_count = 1;
  else
    storage->min_count = 0;

  if( num_frame_buffers > MAX_PIC_BUFFERS )
    num_frame_buffers = MAX_PIC_BUFFERS;

  storage->max_num_buffers = num_frame_buffers;

  storage->work0 = storage->work1 = INVALID_ANCHOR_PICTURE;
  storage->work_out_prev =
    storage->work_out = BqueueNext(
                          &storage->bq,
                          storage->work0,
                          storage->work1,
                          BQUEUE_UNUSED,
                          0 );
  storage->first_frame = HANTRO_TRUE;
  storage->picture_broken = HANTRO_FALSE;

  return (VC1HWD_OK);

}
/*------------------------------------------------------------------------------

    Function name: AllocateMemories

        Functional description:
            Allocates decoder internal memories

        Inputs:
            dec_cont    Decoder container
            storage    Stream storage descriptor
            dwl         Instance of decoder wrapper layer

        Outputs:
            None

        Returns:
            VC1HWD_MEMORY_FAIL / VC1HWD_OK

------------------------------------------------------------------------------*/
u16x AllocateMemories( decContainer_t *dec_cont,
                       swStrmStorage_t *storage,
                       const void *dwl ) {
  u16x i;
  u16x rv;
  u16x size;
  picture_t *p_pic = NULL;
  u32 buffers;

  if( storage->max_bframes > 0 ) {
    buffers = 3;
  } else {
    buffers = 2;
  }

  /* Calculate minimum amount of buffers */
  storage->work_buf_amount = storage->max_num_buffers;
  if( storage->work_buf_amount < buffers )
    storage->work_buf_amount = buffers;
  rv = BqueueInit2(&storage->bq,
                   storage->work_buf_amount );
  if(rv != HANTRO_OK) {
    (void)vc1hwdRelease(dwl,
                        storage);
    return (VC1HWD_MEMORY_FAIL);
  }

  /* Memory for all picture_t structures */
  p_pic = (picture_t*)DWLmalloc(sizeof(picture_t) * (16+1));
  if(p_pic == NULL) {
    (void)vc1hwdRelease(dwl,
                        storage);
    return (VC1HWD_MEMORY_FAIL);
  }
  (void)DWLmemset(p_pic, 0, sizeof(picture_t)*(16+1));

  /* set pointer to picture_t buffer */
  storage->p_pic_buf = (struct picture*)p_pic;

  size = NEXT_MULTIPLE(4 * dec_cont->storage.pic_width_in_mbs * 16,
                        ALIGN(dec_cont->align)) *
                        dec_cont->storage.pic_height_in_mbs * 4 * 3 / 2;

  if (dec_cont->pp_enabled) {
    dec_cont->n_int_buf_size = size;
    for (i = 0; i < storage->work_buf_amount; i++) {
      p_pic[i].data.mem_type = DWL_MEM_TYPE_DPB | DWL_MEM_TYPE_DMA_DEVICE_ONLY;
      SET_MEM_USAGE(p_pic[i].data.mem_type, DWL_MEM_USAGE_OUT_REFERENCE,
                    dec_cont->secure_mode);
      if (DWLMallocRefFrm(dwl, size, &p_pic[i].data) != 0) {
        (void)vc1hwdRelease(dwl, storage);
        return (VC1HWD_MEMORY_FAIL);
      }
      /* init coded image size to max coded image size */
      p_pic[i].coded_width = storage->max_coded_width;
      p_pic[i].coded_height = storage->max_coded_height;
    }
  }

  storage->p_mb_flags = (u8*)DWLmalloc(((storage->num_of_mbs+9)/10)*10);
  if (storage->p_mb_flags == NULL) {
    (void)vc1hwdRelease(dwl,
                        storage);
    return (VC1HWD_MEMORY_FAIL);
  }
  (void)DWLmemset(storage->p_mb_flags, 0, ((storage->num_of_mbs+9)/10)*10);

  /* bit plane coded data, 3 bits per macroblock, have to allocate integer
   * number of 4-byte words */
  dec_cont->bit_plane_ctrl.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
  SET_MEM_USAGE(dec_cont->bit_plane_ctrl.mem_type, DWL_MEM_USAGE_OUT_BITPLANE_CTRL,
                dec_cont->secure_mode);
  rv = DWLMallocLinear(dwl,
                       (storage->num_of_mbs + 9)/10 * sizeof(u32),
                       &dec_cont->bit_plane_ctrl);
  if ((rv != 0) ||
      X170_CHECK_VIRTUAL_ADDRESS( dec_cont->bit_plane_ctrl.virtual_address ) ||
      X170_CHECK_BUS_ADDRESS( dec_cont->bit_plane_ctrl.bus_address )) {
    (void)vc1hwdRelease(dwl,
                        &dec_cont->storage);
    return (VC1HWD_MEMORY_FAIL);
  }

  /* If B pictures in the stream allocate space for direct mode mvs */
  if(dec_cont->storage.max_bframes) {
    /* allocate for even number of macroblock rows to accommodate direct
     * mvs of field pictures */
    dec_cont->direct_mvs.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
    if (dec_cont->storage.pic_height_in_mbs & 0x1) {
      SET_MEM_USAGE(dec_cont->direct_mvs.mem_type, DWL_MEM_USAGE_TMP_DIRMV,
                  dec_cont->secure_mode);
      rv = DWLMallocLinear(dec_cont->dwl,
                           ((dec_cont->storage.num_of_mbs +
                             dec_cont->storage.pic_width_in_mbs+7) & ~0x7) * 2 * sizeof(u32),
                           &dec_cont->direct_mvs);
    } else {
      rv = DWLMallocLinear(dec_cont->dwl,
                           ((dec_cont->storage.num_of_mbs+7) & ~0x7) * 2 * sizeof(u32),
                           &dec_cont->direct_mvs);
    }
#ifdef ASIC_TRACE_SUPPORT
    if (!rv) {
      if (dec_cont->direct_mvs.virtual_address != NULL) {
        DWLmemset(dec_cont->direct_mvs.virtual_address,
                  0, dec_cont->direct_mvs.logical_size);
      } else {
        DWLLinearMemset(dec_cont->dwl, &dec_cont->direct_mvs, 0, 0, dec_cont->direct_mvs.logical_size);
      }
    }
#endif
    if (rv != 0 || X170_CHECK_BUS_ADDRESS( dec_cont->direct_mvs.bus_address )) {
      DWLFreeLinear(dwl, &dec_cont->bit_plane_ctrl);
      (void)vc1hwdRelease(dwl,
                          &dec_cont->storage);
      return (VC1HWD_MEMORY_FAIL);
    }
  } else
    dec_cont->direct_mvs.virtual_address = 0;

  return (VC1HWD_OK);
}

/*------------------------------------------------------------------------------

    Function name: ValidateMetadata

        Functional description:
                    Function checks that all necessary initialization metadata
                    are present in metadata structure and values are sane.

        Inputs:
            storage        Stream storage descriptor
            p_meta_data       Pointer to metadata information

        Outputs:

        Returns:
            VC1HWD_OK / VC1HWD_METADATA_ERROR

------------------------------------------------------------------------------*/
u16x ValidateMetadata(swStrmStorage_t *storage,const struct DecMetaData *p_meta_data,
                     const struct DecHwFeatures *hw_feature) {
  /* check metadata information */
  if ( p_meta_data->max_coded_width < MIN_PIC_WIDTH_G1 ||
       p_meta_data->max_coded_width > hw_feature->vc1_max_dec_pic_width ||
       p_meta_data->max_coded_height < MIN_PIC_HEIGHT_G1 ||
       p_meta_data->max_coded_height > hw_feature->vc1_max_dec_pic_height ||
       (p_meta_data->max_coded_width & 0x1) ||
       (p_meta_data->max_coded_height & 0x1) ||
       p_meta_data->quantizer > 3 )
    return (VC1HWD_METADATA_ERROR);

  storage->cur_coded_width     = storage->max_coded_width
                                 = p_meta_data->max_coded_width;
  storage->cur_coded_height    = storage->max_coded_height
                                 = p_meta_data->max_coded_height;
  storage->pic_width_in_mbs  = (p_meta_data->max_coded_width+15) >> 4;
  storage->pic_height_in_mbs = (p_meta_data->max_coded_height+15) >> 4;
  storage->num_of_mbs       = (storage->pic_width_in_mbs *
                               storage->pic_height_in_mbs);

  if (storage->num_of_mbs > MAX_NUM_MBS)
    return(VC1HWD_METADATA_ERROR);

  storage->vs_transform       = p_meta_data->vs_transform ? 1 : 0;
  storage->overlap           = p_meta_data->overlap ? 1 : 0;
  storage->sync_marker        = p_meta_data->sync_marker ? 1 : 0;
  storage->frame_interp_flag   = p_meta_data->frame_interp ? 1 : 0;
  storage->quantizer         = p_meta_data->quantizer;

  storage->max_bframes        = p_meta_data->max_bframes;
  storage->fast_uv_mc          = p_meta_data->fast_uv_mc ? 1 : 0;
  storage->extended_mv        = p_meta_data->extended_mv ? 1 : 0;
  storage->multi_res          = p_meta_data->multi_res ? 1 : 0;
  storage->range_red          = p_meta_data->range_red ? 1 : 0;
  storage->dquant            = p_meta_data->dquant;
  storage->loop_filter        = p_meta_data->loop_filter ? 1 : 0;
  storage->profile           =
    (p_meta_data->profile == 0) ? VC1_SIMPLE : VC1_MAIN;

  /* is dquant valid */
  if (storage->dquant > 2)
    return (VC1HWD_METADATA_ERROR);


  /* Quantizer specification. > 3 is SMPTE reserved */
  if (p_meta_data->quantizer > 3)
    return (VC1HWD_METADATA_ERROR);

  /* maximum number of consecutive B frames. > 7 SMPTE reserved */
  if (p_meta_data->max_bframes > 7)
    return (VC1HWD_METADATA_ERROR);

  return (VC1HWD_OK);
}

/*------------------------------------------------------------------------------

    Function name: vc1hwdDecode

        Functional description:
            Control for sequence and picture layer decoding

        Inputs:
            dec_cont    Decoder container
            storage    Stream storage descriptor
            stream_data  Descriptor for stream data

        Outputs:
            None

        Returns:
            VC1HWD_ERROR
            VC1HWD_END_OF_SEQ
            VC1HWD_USER_DATA_RDY
            VC1HWD_METADATA_ERROR
            VC1HWD_SEQ_HDRS_RDY
            VC1HWD_PIC_HDRS_RDY
            VC1HWD_FIELD_HDRS_RDY
            VC1HWD_ENTRY_POINT_HDRS_RDY
            VC1HWD_USER_DATA_RDY
            VC1HWD_NOT_CODED_PIC
            VC1HWD_MEMORY_FAIL
            VC1HWD_HDRS_ERROR

------------------------------------------------------------------------------*/
enum VC1HWDStatus vc1hwdDecode( decContainer_t *dec_cont,
                                swStrmStorage_t *storage,
                                strmData_t *stream_data ) {
  /* Variables */

  startCode_e start_code;
  u32 tmp;
  u32 read_bits_start, read_bits_end;
  enum VC1HWDStatus ret_val = VC1HWD_OK;
  swStrmStorage_t tmp_storage;

  /* Code */

  ASSERT(storage);
  ASSERT(stream_data);

  /* ADVANCED PROFILE */
  if (storage->profile == VC1_ADVANCED) {
    storage->slice = HANTRO_FALSE;
    storage->missing_field = HANTRO_FALSE;
    do {
      /* Get Start Code */
      start_code = vc1hwdGetStartCode(stream_data);
      switch (start_code) {
      case SC_SEQ:
        STREAMTRACE_I("%s","Sc_SEQ found\n");
        /* Don't skip seq layer decoding after the first
         * pic is successfully decoded to avoid seq dynamic change */
        tmp_storage = dec_cont->storage;
        ret_val = vc1hwdDecodeSequenceLayer(&tmp_storage, stream_data);
        if (ret_val == VC1HWD_SEQ_HDRS_RDY) {
          dec_cont->storage = tmp_storage;
        } else {
          if (tmp_storage.hrd_rate){
            DWLfree(tmp_storage.hrd_rate);
            tmp_storage.hrd_rate = NULL;
          }
          if (tmp_storage.hrd_buffer){
            DWLfree(tmp_storage.hrd_buffer);
            tmp_storage.hrd_buffer = NULL;
          }
        }
        break;
      case SC_SEQ_UD:
        STREAMTRACE_I("%s","Sc_SEQ_UD found\n");
        ret_val = vc1hwdGetUserData(storage, stream_data);
        ret_val = VC1HWD_USER_DATA_RDY;
        break;
      case SC_ENTRY_POINT:
        STREAMTRACE_I("%s","Sc_ENTRY_POINT found\n");

        /* Check that sequence headers are decoded succesfully */
        if (!(storage->hdrs_decoded & HDR_SEQ)) {
          ret_val = vc1hwdGetUserData(storage, stream_data);
          ret_val = VC1HWD_USER_DATA_RDY;
          break;
        }
        //    return VC1HWD_HDRS_ERROR;

        ret_val = vc1hwdDecodeEntryPointLayer(storage, stream_data);
        break;
      case SC_ENTRY_POINT_UD:
        STREAMTRACE_I("%s","Sc_ENTRY_POINT_UD found\n");
        ret_val = vc1hwdGetUserData(storage, stream_data);
        ret_val = VC1HWD_USER_DATA_RDY;
        break;
      case SC_FRAME:
        STREAMTRACE_I("%s","Sc_FRAME found\n");
        /* Check that headers are decoded succesfully */
        if (storage->hdrs_decoded != HDR_BOTH) {
          ret_val = vc1hwdGetUserData(storage, stream_data);
          ret_val = VC1HWD_USER_DATA_RDY;
          break;
        }
        //    return VC1HWD_HDRS_ERROR;

        stream_data->slice_piclayer_emulation_bits = 0;
        read_bits_start = stream_data->strm_buff_read_bits;
        ret_val = vc1hwdDecodePictureLayerAP(storage, stream_data);

        read_bits_end = stream_data->strm_buff_read_bits;
        storage->pic_layer.pic_header_bits =
          read_bits_end - read_bits_start;

        storage->pic_layer.pic_header_bits -=
          stream_data->slice_piclayer_emulation_bits ;

        /* Set anchor frame/field inter/intra status */
        if( storage->pic_layer.fcm == FIELD_INTERLACE )
          tmp = storage->pic_layer.tff ^ 1;
        else
          tmp = 0;
        if( storage->pic_layer.pic_type == PTYPE_I )
          storage->anchor_inter[ tmp ] = HANTRO_FALSE;
        else if ( (storage->pic_layer.pic_type == PTYPE_P) ||
                  (storage->pic_layer.pic_type == PTYPE_Skip) )
          storage->anchor_inter[ tmp ] = HANTRO_TRUE;

        /* flag used to detect missing field */
        if( storage->pic_layer.fcm == FIELD_INTERLACE )
          storage->ff_start = HANTRO_TRUE;
        else
          storage->ff_start = HANTRO_FALSE;

        /* previous frame was field coded and
         * second field missing */
        if ( (storage->p_pic_buf[storage->work_out].fcm ==
              FIELD_INTERLACE) &&
             (storage->p_pic_buf[storage->work_out].is_first_field ==
              HANTRO_TRUE) ) {
          /* replace orphan 1.st field of previous frame in
           * frame buffer */
          if (storage->field_count)
            storage->field_count--;
          //if (storage->outp_count)
          //  storage->outp_count--;
          BqueueDiscard( &storage->bq, storage->work_out );
          if (dec_cont->pp_enabled)
            InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
          storage->work_out_prev =
            storage->work_out = storage->work0;
          STREAMTRACE_E("%s","MISSING SECOND FIELD\n");
          storage->missing_field = HANTRO_TRUE;
        }
        break;
      case SC_FRAME_UD:
        STREAMTRACE_I("%s","\nSc_FRAME_UD found\n");
        ret_val = vc1hwdGetUserData(storage, stream_data);
        ret_val = VC1HWD_USER_DATA_RDY;
        break;
      case SC_FIELD:
        STREAMTRACE_I("%s","\nSc_FIELD found\n");
        /* If FCM at this point is other than field interlace,
         * then we have an extra field in the stream. Let's skip
         * it altogether and move on to the next SC. */
        if( storage->pic_layer.fcm != FIELD_INTERLACE )
          continue;

        /* Check that headers are decoded succesfully */
        if (storage->hdrs_decoded != HDR_BOTH) {
          ret_val = vc1hwdGetUserData(storage, stream_data);
          ret_val = VC1HWD_USER_DATA_RDY;
          break;
        }
        //    return VC1HWD_HDRS_ERROR;

        /* previous frame finished and field SC found
         * -> first field missing -> discard */
        if( (storage->pic_layer.fcm == FIELD_INTERLACE) &&
            (storage->ff_start == HANTRO_FALSE)) {
          STREAMTRACE_E("%s","MISSING FIRST FIELD\n");
          storage->missing_field = HANTRO_TRUE;
          return VC1HWD_ERROR;
        }
        storage->ff_start = HANTRO_FALSE;
        stream_data->slice_piclayer_emulation_bits = 0;

        /* Substract field header bits from total length */
        storage->pic_layer.pic_header_bits -=
          storage->pic_layer.field_header_bits;

        ret_val = vc1hwdDecodeFieldLayer( storage,
                                          stream_data,
                                          HANTRO_FALSE );
        /* And add length of new header */
        storage->pic_layer.pic_header_bits +=
          storage->pic_layer.field_header_bits;

        storage->pic_layer.pic_header_bits -=
          stream_data->slice_piclayer_emulation_bits ;

        /* Set anchor field inter/intra status */
        if( storage->pic_layer.pic_type == PTYPE_I ) {
          storage->anchor_inter[ storage->pic_layer.tff ] =
            HANTRO_FALSE;
        } else if ( (storage->pic_layer.pic_type == PTYPE_P) ||
                    (storage->pic_layer.pic_type == PTYPE_Skip) ) {
          storage->anchor_inter[ storage->pic_layer.tff ] =
            HANTRO_TRUE;
        }
        break;
      case SC_FIELD_UD:
        STREAMTRACE_I("%s","Sc_FIELD_UD found\n");
        ret_val = vc1hwdGetUserData(storage, stream_data);
        ret_val = VC1HWD_USER_DATA_RDY;
        break;
      case SC_SLICE:
        STREAMTRACE_I("%s","Sc_SLICE found\n");

        /* Check that headers are decoded succesfully */
        if (storage->hdrs_decoded != HDR_BOTH) {
          ret_val = vc1hwdGetUserData(storage, stream_data);
          ret_val = VC1HWD_USER_DATA_RDY;
          break;
        }
        //    return VC1HWD_HDRS_ERROR;

        /* If we found slice start code, and picture type is skipped,
         * it must be an error. */
        if( storage->pic_layer.pic_type == PTYPE_Skip )
          return VC1HWD_ERROR;

        ret_val = VC1HWD_PIC_HDRS_RDY;
        storage->slice = HANTRO_TRUE;

        /* unflush startcode */
        stream_data->strm_buff_read_bits-=32;
        stream_data->strm_curr_pos -= 4;
        break;
      case SC_SLICE_UD:
        STREAMTRACE_I("%s","Sc_SLICE_UD found\n");
        ret_val = VC1HWD_USER_DATA_RDY;
        break;
      case SC_END_OF_SEQ:
        storage->first_frame = 0;
        STREAMTRACE_I("%s","Sc_END_OF_SEQ found\n");
        return (VC1HWD_END_OF_SEQ);
      case SC_NOT_FOUND:
        ret_val = vc1hwdFlushBits(stream_data, 8);
        break;
      default:
        STREAMTRACE_E("%s","Sc_ERROR found\n");
        ret_val = HANTRO_NOK;
        if (storage->hdrs_decoded != HDR_BOTH) {
          ret_val = vc1hwdGetUserData(storage, stream_data);
          ret_val = VC1HWD_USER_DATA_RDY;
          break;
        }
        //    return (VC1HWD_HDRS_ERROR);
        break;
      }
      /* check stream exhaustion */
      if(vc1hwdIsExhausted(stream_data)) {
        /* Notice!! VC1HWD_USER_DATA_RDY is used to return
         * DEC_STRM_PROCESSED from the API in these cases */
        if ( (start_code == SC_NOT_FOUND) ||
             (ret_val == VC1HWD_USER_DATA_RDY) ) {
          return VC1HWD_USER_DATA_RDY;
        }

        STREAMTRACE_E("%s","Stream exhausted!");
        ret_val = HANTRO_NOK;
      }
    } while ( (ret_val != HANTRO_NOK) &&
              (ret_val != VC1HWD_METADATA_ERROR ) &&
              (ret_val != VC1HWD_SEQ_HDRS_RDY) &&
              (ret_val != VC1HWD_PIC_HDRS_RDY) &&
              (ret_val != VC1HWD_FIELD_HDRS_RDY) &&
              (ret_val != VC1HWD_ENTRY_POINT_HDRS_RDY) &&
              (ret_val != VC1HWD_USER_DATA_RDY) );
  }
  /* SIMPLE AND MAIN PROFILE */
  else {
    /* if size of the stream is either 1 or 0 bytes,
     * then picture is skipped and it shall be reconstructed
     * as a P frame which is identical to its reference frame */
    if (stream_data->strm_buff_size <= 1) {
      if(stream_data->strm_buff_size == 1) {
        ret_val = vc1hwdFlushBits(stream_data, 8);
      }
      /* just adjust picture buffer indexes
       * to correct positions */
      if (storage->max_bframes > 0) {
        /* Increase references to reference picture */
        if( storage->work1 != storage->work0) {
          storage->work1 = storage->work0;
        }
        /* Skipped frame shall be reconstructed as a
         * P-frame which is identical to its reference */
        storage->pic_layer.pic_type = PTYPE_P;

        BqueueDiscard( &storage->bq, storage->work_out );
        if (dec_cont->pp_enabled)
          InputQueueReturnBuffer(dec_cont->pp_buffer_queue, DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
        storage->work_out_prev = storage->work_out;
        storage->work_out = storage->work0;
        BqueueWaitBufNotInUse( &dec_cont->storage.bq, dec_cont->storage.work_out);
        if(dec_cont->pp_enabled) {
          InputQueueWaitBufNotUsed(dec_cont->pp_buffer_queue,DWL_GET_DEVMEM_ADDR(*(dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data)));
        }
        STREAMTRACE_E("%s","Skipped picture with MAXBFRAMES>0!\n");
        return(VC1HWD_ERROR);
      } else {
        return VC1HWD_NOT_CODED_PIC;
      }
    }
    /* Allocate memories */
    if (dec_cont->dec_stat == VC1DEC_INITIALIZED) {
      BqueueWaitNotInUse(&storage->bq);
      if (dec_cont->pp_enabled)
        InputQueueWaitNotUsed(dec_cont->pp_buffer_queue);
    }
    /* Decode picture layer headers */
    ret_val = vc1hwdDecodePictureLayer(storage, stream_data);

    /* check stream exhaustion */
    if(vc1hwdIsExhausted(stream_data)) {
      STREAMTRACE_E("%s","Stream exhausted!\n");
      ret_val = HANTRO_NOK;
    }

    if (!dec_cont->same_pic_header) {
      if (storage->pic_layer.pic_type == PTYPE_I ||
          storage->pic_layer.pic_type == PTYPE_BI )
        storage->rnd = 0;
      else if (storage->pic_layer.pic_type == PTYPE_P)
        storage->rnd = 1 - storage->rnd;
    }

    if( storage->pic_layer.pic_type == PTYPE_I )
      storage->anchor_inter[ 0 ] = HANTRO_FALSE;
    else if ( storage->pic_layer.pic_type == PTYPE_P )
      storage->anchor_inter[ 0 ] = HANTRO_TRUE;

  }

  if( (ret_val == VC1HWD_PIC_HDRS_RDY) ||
      (ret_val == VC1HWD_FIELD_HDRS_RDY) ) {
    if( storage->pic_layer.pic_type == PTYPE_Skip ) {
      /* Increase references to reference picture */
      if( storage->work1 != storage->work0) {
        storage->work1 = storage->work0;
      }
      storage->work_out_prev = storage->work_out;
      storage->work_out = storage->work0;
      return VC1HWD_NOT_CODED_PIC;
    }

    /* field interlace */
    if (storage->pic_layer.fcm == FIELD_INTERLACE) {
      /* no reference picture for P field */
      if ( (storage->pic_layer.field_pic_type == FP_I_P) &&
           (storage->pic_layer.is_ff == HANTRO_FALSE) &&
           (storage->first_frame) &&
           (storage->pic_layer.num_ref == 0) &&
           (storage->pic_layer.ref_field != 0) ) {
        STREAMTRACE_E("%s","No anchor picture for P field\n");
        ret_val = HANTRO_NOK;
      }

      /* invalid field parity */
      if ( (storage->pic_layer.is_ff == HANTRO_FALSE) &&
           (storage->p_pic_buf[storage->work_out].is_top_field_first ==
            storage->pic_layer.top_field) ) {
        STREAMTRACE_E("%s","Same parity for successive fields in the frame\n");
        ret_val = HANTRO_NOK;
      }
    }
    /* if P frame, check that anchor available */
    if( (storage->pic_layer.is_ff == HANTRO_TRUE) &&
        (storage->pic_layer.pic_type == PTYPE_P) &&
        (storage->work0 == INVALID_ANCHOR_PICTURE) ) {
      STREAMTRACE_E("%s","No anchor picture for P picture\n");
      ret_val = HANTRO_NOK;
    } else if( dec_cont->storage.pic_layer.pic_type == PTYPE_B &&
               (dec_cont->storage.work1 == INVALID_ANCHOR_PICTURE ||
                dec_cont->storage.work0 == INVALID_ANCHOR_PICTURE)) {
      STREAMTRACE_E("%s","No anchor pictures for B picture!\n");
      ret_val = HANTRO_NOK;
    }
  }
#ifdef VCD_LOGMSG
  char* profile = "not support";
  char* level = "not support";
  switch (storage->profile)
  {
  case VC1_SIMPLE: {
    profile = "Simple";
    if (storage->level == 0) level = "Low";
    if (storage->level == 2) level = "Medium";
    break;
  }
  case VC1_MAIN: {
    profile = "Main";
    if (storage->level == 0) level = "Low";
    if (storage->level == 2) level = "Medium";
    if (storage->level == 4) level = "High";
    break;
  }
  case VC1_ADVANCED: {
    profile = "Advanced";
    if (storage->level == 0) level = "L0";
    if (storage->level == 1) level = "L1";
    if (storage->level == 2) level = "L2";
    if (storage->level == 3) level = "L3";
    break;
  }
  default:
    break;
  }
  STREAMTRACE_I("Profile:%s | Level:%s\n", profile, level);
#endif

  if (ret_val == HANTRO_NOK) {
    storage->picture_broken = HANTRO_TRUE;
//	if(storage->hrd_rate != NULL) DWLfree(storage->hrd_rate);
//	if(storage->hrd_buffer != NULL) DWLfree(storage->hrd_buffer);
    return(VC1HWD_ERROR);
  } else
    return(ret_val);

}

/*------------------------------------------------------------------------------

    Function name: vc1hwdRelease

        Functional description:
            Release allocated resources

        Inputs:
            dwl             DWL instance
            storage        Stream storage descriptor.

        Outputs:
            None

        Returns:
            HANTRO_OK

------------------------------------------------------------------------------*/
u16x vc1hwdRelease(const void *dwl,
                   swStrmStorage_t *storage) {

  /* Variables */
  u16x i;
  picture_t *p_pic = NULL;

  /* Code */
  ASSERT(storage);
  BqueueRelease2(&storage->bq);

  p_pic = (picture_t*)storage->p_pic_buf;

  /* free all allocated picture buffers */
  if (p_pic != NULL) {
    if (((decContainer_t *)(storage->dec_cont))->pp_enabled) {
      for (i = 0; i < storage->work_buf_amount; i++) {
        DWLFreeRefFrm(dwl, &p_pic[i].data);
      }
    }
    DWLfree((picture_t*)storage->p_pic_buf);

    storage->p_pic_buf = NULL;
  }
  if(storage->p_mb_flags) {
    DWLfree(storage->p_mb_flags);
    storage->p_mb_flags = NULL;
  }

  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function: vc1hwdUnpackMetaData

        Functional description:
            Unpacks metadata elements from buffer, when metadata is packed
            according to SMPTE VC-1 Standard Annexes J and L.

        Inputs:
            p_buffer         Buffer containing packed metadata

        Outputs:
            p_meta_data       Unpacked metadata

        Returns:
            HANTRO_OK       Metadata unpacked OK
            HANTRO_NOK      Metadata is in wrong format or indicates
                            unsupported tools

------------------------------------------------------------------------------*/
u16x vc1hwdUnpackMetaData(const u8 * p_buffer, struct DecMetaData *p_meta_data) {

  /* Variables */

  u32 tmp1, tmp2;

  /* Code */

  STREAMTRACE_I("%s","Unpacking META DATA\n");
  STREAMTRACE_I("%s","Struct_SEQUENCE_HEADER_C\n");

  /* Initialize sequence header C elements */
  p_meta_data->loop_filter =
    p_meta_data->multi_res =
      p_meta_data->fast_uv_mc =
        p_meta_data->extended_mv =
          p_meta_data->dquant =
            p_meta_data->vs_transform =
              p_meta_data->overlap =
                p_meta_data->sync_marker =
                  p_meta_data->range_red =
                    p_meta_data->max_bframes =
                      p_meta_data->quantizer =
                        p_meta_data->frame_interp = 0;

  tmp1 = SHOW1(p_buffer);
  tmp2  = BIT7(tmp1);
  tmp2 <<=1;
  tmp2 |= BIT6(tmp1);
  tmp2 <<=1;
  tmp2 |= BIT5(tmp1);
  tmp2 <<=1;
  tmp2 |= BIT4(tmp1);

  STREAMTRACE_I("PROFILE: \t %d\n",tmp2);

  p_meta_data->profile = tmp2;
  if (tmp2 != VC1_ADVANCED_PROFILE) { /* Simple and Main, see the spec table 263*/
    tmp1 = SHOW1(p_buffer);

    tmp2 = BIT3(tmp1);
    STREAMTRACE_I("LOOPFILTER: \t %d\n",tmp2);
    p_meta_data->loop_filter = tmp2;
    tmp2 = BIT2(tmp1);
    STREAMTRACE_I("Reserved3: \t %d\n",tmp2);
#ifdef HANTRO_PEDANTIC_MODE
    if( tmp2 != 0 ) return HANTRO_NOK;
#endif /* HANTRO_PEDANTIC_MODE */
    tmp2 = BIT1(tmp1);
    STREAMTRACE_I("MULTIRES: \t %d\n",tmp2);
    p_meta_data->multi_res = tmp2;
    tmp2 = BIT0(tmp1);
    STREAMTRACE_I("Reserved4: \t %d\n",tmp2);
#ifdef HANTRO_PEDANTIC_MODE
    if( tmp2 != 1 ) return HANTRO_NOK;
#endif /* HANTRO_PEDANTIC_MODE */
    tmp1 = SHOW1(p_buffer);
    tmp2 = BIT7(tmp1);
    STREAMTRACE_I("FASTUVMC: \t %d\n",tmp2);
    p_meta_data->fast_uv_mc = tmp2;
    tmp2 = BIT6(tmp1);
    STREAMTRACE_I("EXTENDED_MV: \t %d\n",tmp2);
    p_meta_data->extended_mv = tmp2;
    tmp2 = BIT5(tmp1);
    tmp2 <<=1;
    tmp2 |= BIT4(tmp1);
    STREAMTRACE_I("DQUANT: \t %d\n",tmp2);
    p_meta_data->dquant = tmp2;
    if(p_meta_data->dquant > 2)
      return HANTRO_NOK;

    tmp2 = BIT3(tmp1);
    p_meta_data->vs_transform = tmp2;
    STREAMTRACE_I("VTRANSFORM: \t %d\n",tmp2);
    tmp2 = BIT2(tmp1);
    STREAMTRACE_I("Reserved5: \t %d\n",tmp2);
    /* Reserved5 needs to be checked, it affects stream syntax. */
    if( tmp2 != 0 ) return HANTRO_NOK;
    tmp2 = BIT1(tmp1);
    p_meta_data->overlap = tmp2;
    STREAMTRACE_I("OVERLAP: \t %d\n",tmp2);
    tmp2 = BIT0(tmp1);
    p_meta_data->sync_marker = tmp2;
    STREAMTRACE_I("SYNCMARKER: \t %d\n",tmp2);

    tmp1 = SHOW1(p_buffer);	//cppcheck-suppress uselessAssignmentPtrArg
    tmp2 = BIT7(tmp1);
    STREAMTRACE_I("RANGERED: \t %d\n",tmp2);
    p_meta_data->range_red = tmp2;
    tmp2 =  BIT6(tmp1);
    tmp2 <<=1;
    tmp2 |= BIT5(tmp1);
    tmp2 <<=1;
    tmp2 |= BIT4(tmp1);
    STREAMTRACE_I("MAXBFRAMES: \t %d\n",tmp2);
    p_meta_data->max_bframes = tmp2;
    tmp2 = BIT3(tmp1);
    tmp2 <<=1;
    tmp2 |= BIT2(tmp1);
    p_meta_data->quantizer = tmp2;
    STREAMTRACE_I("QUANTIZER: \t %d\n",tmp2);
    tmp2 = BIT1(tmp1);
    p_meta_data->frame_interp = tmp2;
    STREAMTRACE_I("FINTERPFLAG: \t %d\n",tmp2);
    tmp2 = BIT0(tmp1);
    STREAMTRACE_I("Reserved6: \t %d\n", tmp2);
    /* Reserved6 needs to be checked, it affects stream syntax. */
    if( tmp2 != 1 ) return HANTRO_NOK;
  }

  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function name: vc1hwdErrorConcealment

        Functional description:
            Perform error concealment.

        Inputs:
            flush           HANTRO_TRUE    - Initialize picture to 128
                            HANTRO_FALSE   - Freeze to previous picture
            storage        Stream storage descriptor.

        Outputs:

        Returns:
            None

------------------------------------------------------------------------------*/
void vc1hwdErrorConcealment(decContainer_t *dec_cont, const u16x flush,
                             swStrmStorage_t * storage ) {

  /* Variables */
  u32 tmp;
  u32 tmp_out;
  u32 i;
  /* Code */

  tmp_out = storage->work_out;
  if(flush){
    DWLLinearMemset(dec_cont->dwl, &storage->p_pic_buf[ storage->work_out ].data, 0, 128,
                    storage->num_of_mbs * 384);
    /* if other buffer contains non-paired field -> throw away */
    for (i = 0; i < storage->bq.queue_size; i++) {
      if (storage->work_out != i) {
        if (storage->p_pic_buf[i].fcm == FIELD_INTERLACE &&
            storage->p_pic_buf[i].is_first_field == HANTRO_TRUE) {
          /* replace orphan 1.st field of previous frame in
           * frame buffer */
          if (storage->field_count)
            storage->field_count--;
          /* now outp_count is added when the second field is ready, so when the second field missing or corrupted, no need to decrease.*/
          //if (storage->outp_count)
          //  storage->outp_count--;
        }
      }
    }
  } else {
    /* we don't change indexes for broken B frames
     * because those are not buffered */
    if ( (storage->pic_layer.pic_type == PTYPE_I) ||
         (storage->pic_layer.pic_type == PTYPE_P)) {
      BqueueDiscard( &storage->bq, storage->work_out );
      storage->work_out = storage->work0;
      storage->work_out_prev = storage->work0;
      if (!storage->p_pic_buf[tmp_out].is_first_field)
        tmp_out = storage->work_out;
    }
  }

  /* unbuffer first field if second field is corrupted */
  if ((!storage->pic_layer.is_ff && !storage->missing_field)) {
    /* replace orphan 1.st field of previous frame in
     * frame buffer */
    if (storage->field_count)
      storage->field_count--;
    //if (storage->outp_count)
    //  storage->outp_count--;
  }
  /* mark fcm as frame interlaced because concealment is frame based */
  if (storage->pic_layer.fcm == FIELD_INTERLACE) {
    /* conceal whole frame */
    storage->pic_layer.fcm = FRAME_INTERLACE;
  }
  /* skip B frames after corrupted anchor frame */
  if ( (storage->pic_layer.pic_type == PTYPE_I) ||
       (storage->pic_layer.pic_type == PTYPE_P) ) {
    storage->skip_b = 2;
    tmp = storage->work_out;
  } else
    tmp = storage->prev_bidx; /* concealing B frame */

  /* both fields are concealed */
  ((picture_t*)storage->p_pic_buf)[tmp].is_first_field = HANTRO_FALSE;
}

/*------------------------------------------------------------------------------

    Function name: vc1hwdSeekFrameStart

        Functional description:

        Inputs:
            storage    Stream storage descriptor.
            p_strm_data   Input stream data.

        Outputs:
            None

        Returns:
            OK/NOK/END_OF_STREAM

------------------------------------------------------------------------------*/
u32 vc1hwdSeekFrameStart(swStrmStorage_t * storage,
                         strmData_t *p_strm_data ) {
  u32 rv;
  u32 sc;
  u32 tmp;
  u8 *p_strm;

  if (storage->profile == VC1_ADVANCED) {
    /* Seek next frame start */
    tmp = p_strm_data->bit_pos_in_word;
    if (tmp)
      rv = vc1hwdFlushBits(p_strm_data, 8-tmp);

    do {
      p_strm = p_strm_data->strm_curr_pos;

      /* Check that we have enough stream data */
      if (((p_strm_data->strm_buff_read_bits>>3)+4) <= p_strm_data->strm_buff_size) {
        sc = ( ((u32)p_strm[0]) << 24 ) +
             ( ((u32)p_strm[1]) << 16 ) +
             ( ((u32)p_strm[2]) << 8  ) +
             ( ((u32)p_strm[3]) );
      } else
        sc = 0;

      if (sc == SC_FRAME ||
          sc == SC_SEQ ||
          sc == SC_ENTRY_POINT ||
          sc == SC_END_OF_SEQ ||
          sc == END_OF_STREAM ) {
        break;
      }
    } while (vc1hwdFlushBits(p_strm_data, 8) == HANTRO_OK);
  } else {
    /* mark all stream processed for simple/main profiles */
    p_strm_data->strm_curr_pos = p_strm_data->p_strm_buff_start +
                                 p_strm_data->strm_buff_size;
  }

  if (vc1hwdIsExhausted(p_strm_data))
    rv = END_OF_STREAM;
  else
    rv = HANTRO_OK;

  return rv;
}

/*------------------------------------------------------------------------------

    Function name: BufferPicture

        Functional description:
            Perform error concealment.

        Inputs:
            dec_cont        Decoder container
            pic_to_buffer     Picture buffer index of pic to buffer.

        Outputs:

        Returns:
            HANTRO_OK       Picture buffered ok
            HANTRO_NOK      Buffer full

------------------------------------------------------------------------------*/
u16x vc1hwdBufferPicture( decContainer_t *dec_cont, u16x pic_to_buffer,
                          u16x buffer_b, u16x pic_id) {

  /* Variables */

  i32 i, j;
  u32 ff_index;
  swStrmStorage_t * storage = &dec_cont->storage;
  /* Code */
  ASSERT(storage);

  /* Index 0 for first field, 1 for second field */
  ff_index = (storage->pic_layer.is_ff) ? 0 : 1;

  if (storage->pic_layer.fcm == FIELD_INTERLACE) {
    storage->field_count++;
    /* for error concealment purposes */
    if (storage->field_count >= 2)
      storage->first_frame = HANTRO_FALSE;

    /* Just return if second field of the frame */
    if (ff_index) {
      /* store pic_id and number of err MBs for second field also */
      storage->out_pic_id[ff_index][storage->prev_outp_idx] = pic_id;
      /* for interlace field, added when second field ready*/
      storage->p_pic_buf[pic_to_buffer].buffered++;
      storage->outp_count++;
      return HANTRO_OK;
    } else {
      if ( (storage->field_count >=3) && (storage->outp_count >= 2) ) {
        /* force output count to be 1 if first field
         * and outp_count == 2. This prevents buffer overflow
         * in some rare error cases */
        STREAMTRACE_E("%s","Picture buffer output count exceeded. Overwriting picture!!!\n");
        storage->outp_count = 1;
      }
    }
  } else {
    storage->field_count += 2;
    /* for error concealment purposes */
    if (storage->field_count >= 2)
      storage->first_frame = HANTRO_FALSE;
  }

  if ( storage->outp_count >= MAX_OUTPUT_PICS ) {
    return HANTRO_NOK;
  }

  if( buffer_b == 0 ) { /* Buffer I or P picture */
    i = storage->outp_idx + storage->outp_count;
    if( i >= MAX_OUTPUT_PICS ) i -= MAX_OUTPUT_PICS;
  } else { /* Buffer B picture */
    j = storage->outp_idx + storage->outp_count;
    i = j - 1;
    if( j >= MAX_OUTPUT_PICS ) j -= MAX_OUTPUT_PICS;
    if( i < 0 ) i += MAX_OUTPUT_PICS;
    /* Added check due to max 2 pic latency */
    else if( i >= MAX_OUTPUT_PICS ) i -= MAX_OUTPUT_PICS;

    storage->outp_buf[j] = storage->outp_buf[i];
    storage->out_pic_id[0][j] = storage->out_pic_id[0][i];
    storage->out_pic_id[1][j] = storage->out_pic_id[1][i];
  }
  storage->prev_outp_idx = i;
  storage->outp_buf[i] = pic_to_buffer;

  if (storage->pic_layer.fcm != FIELD_INTERLACE)
    storage->p_pic_buf[pic_to_buffer].buffered++;

  if (storage->pic_layer.fcm == FIELD_INTERLACE) {
    storage->out_pic_id[ff_index][i] = pic_id;
  } else {
    /* set same pic_id for both fields of frame */
    storage->out_pic_id[0][i] = pic_id;
    storage->out_pic_id[1][i] = pic_id;
  }

  if (storage->pic_layer.fcm != FIELD_INTERLACE)
    storage->outp_count++;

  dec_cont->fullness = storage->outp_count;

  return HANTRO_OK;
}

/*------------------------------------------------------------------------------

    Function name: vc1hwdNextPicture

        Functional description:
            Perform error concealment.

        Inputs:
            storage        Stream storage descriptor.
            end_of_stream     Indicates whether end of stream has been reached.

        Outputs:
            p_next_picture    Next picture buffer index

        Returns:
            HANTRO_OK       Picture ok
            HANTRO_NOK      No more pictures

------------------------------------------------------------------------------*/
u16x vc1hwdNextPicture( swStrmStorage_t * storage, u16x * p_next_picture,
                        u32 *p_field_to_ret, u16x end_of_stream, u32 deinterlace,
                        u32* p_pic_id, u32* decode_id, u32* p_err_mbs) {

  /* Variables */

  u32 i;
  u32 min_count;

  /* Code */

  ASSERT(storage);
  ASSERT(p_next_picture);

  min_count = 0;
  /* Determine how many pictures we are willing to give out... */
  if(storage->min_count > 0 && !end_of_stream)
    min_count = 1;

  /* when deinterlacing is ON, we don't give first field out
   * before second field is also decoded */
  if ((storage->field_count % 2) && deinterlace) {
    return HANTRO_NOK;
  }

  if (storage->outp_count <= min_count) {
    return HANTRO_NOK;
  }

  if (storage->interlace && !deinterlace) {
    if ((storage->field_count < 3) && !end_of_stream)
      return HANTRO_NOK;

    i = storage->outp_idx;
    *p_field_to_ret = storage->field_to_return;
    *p_next_picture = storage->outp_buf[i];
    *p_pic_id = storage->out_pic_id[storage->field_to_return][i];
    decode_id[0] = storage->out_pic_id[0][i];
    decode_id[1] = storage->out_pic_id[1][i];

    if (storage->field_to_return == 1) { /* return second field */
      storage->outp_count--;
      i++;
      if( i == MAX_OUTPUT_PICS ) i = 0;
      storage->outp_idx = i;
      if (storage->p_pic_buf[*p_next_picture].fcm == PROGRESSIVE) {
        storage->field_to_return = 0;
        storage->field_count--;
        return HANTRO_NOK;
      }
    }

    storage->field_to_return = 1-storage->field_to_return;
    storage->field_count--;
  } else {
    i = storage->outp_idx;
    storage->outp_count--;
    *p_pic_id = storage->out_pic_id[0][i];
    decode_id[0] = decode_id[1] = storage->out_pic_id[0][i];
    *p_next_picture = storage->outp_buf[i++];
    if( i == MAX_OUTPUT_PICS ) i = 0;
    storage->outp_idx = i;

    storage->field_count -= 2;
  }

  return HANTRO_OK;

}

/*------------------------------------------------------------------------------

    Function name: vc1hwdSetPictureInfo

        Functional description:
            Fill picture layer information to picture_t structure.

        Inputs:
            dec_cont        Container for the decoder
            pic_id           Current picture id

        Outputs:

        Returns:
            None

------------------------------------------------------------------------------*/
void vc1hwdSetPictureInfo( decContainer_t *dec_cont, u32 pic_id ) {

  /* Variables */
  u32 work_index;
  picture_t *p_pic;
  u32 ff_index;
  u32 pic_type;

  /* Code */
  ASSERT(dec_cont);

  p_pic = (picture_t*)dec_cont->storage.p_pic_buf;
  work_index = dec_cont->storage.work_out;

  p_pic[ work_index ].coded_width  = dec_cont->storage.cur_coded_width;
  p_pic[ work_index ].coded_height = dec_cont->storage.cur_coded_height;
  p_pic[ work_index ].range_red_frm = dec_cont->storage.pic_layer.range_red_frm;
  p_pic[ work_index ].fcm = dec_cont->storage.pic_layer.fcm;
  memcpy(p_pic[ work_index ].ppu_cfg, dec_cont->ppu_cfg, sizeof(dec_cont->ppu_cfg));

  if (dec_cont->storage.pic_layer.fcm == PROGRESSIVE ||
      dec_cont->storage.pic_layer.fcm == FRAME_INTERLACE ) {
    p_pic[ work_index ].key_frame =
      ( dec_cont->storage.pic_layer.pic_type == PTYPE_I ) ?
      HANTRO_TRUE : HANTRO_FALSE;

    if(dec_cont->storage.pic_layer.pic_type == PTYPE_I)
      pic_type = DEC_PIC_TYPE_I;
    else if(dec_cont->storage.pic_layer.pic_type == PTYPE_P)
      pic_type = DEC_PIC_TYPE_P;
    else if(dec_cont->storage.pic_layer.pic_type == PTYPE_B)
      pic_type = DEC_PIC_TYPE_B;
    else
      pic_type = DEC_PIC_TYPE_BI;

    p_pic[ work_index ].pic_code_type[0] = pic_type;
    p_pic[ work_index ].pic_code_type[1] = pic_type;
  } else {
    p_pic[ work_index ].key_frame =
      ( dec_cont->storage.pic_layer.field_pic_type == FP_I_I ||
        dec_cont->storage.pic_layer.field_pic_type == FP_I_P ||
        dec_cont->storage.pic_layer.field_pic_type == FP_P_I) ?
      HANTRO_TRUE : HANTRO_FALSE;

    switch( dec_cont->storage.pic_layer.field_pic_type ) {
    case FP_I_I:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_I;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_I;
      break;
    case FP_I_P:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_I;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_P;
      break;
    case FP_P_I:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_P;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_I;
      break;
    case FP_P_P:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_P;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_P;
      break;
    case FP_B_B:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_B;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_B;
      break;
    case FP_B_BI:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_B;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_BI;
      break;
    case FP_BI_B:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_BI;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_B;
      break;
    case FP_BI_BI:
      p_pic[ work_index ].pic_code_type[0] = DEC_PIC_TYPE_BI;
      p_pic[ work_index ].pic_code_type[1] = DEC_PIC_TYPE_BI;
      break;
    default:
      STREAMTRACE_E("%s","Unknown field_pic_type!\n");
      break;
    }
  }
  p_pic[ work_index ].tiled_mode      = dec_cont->tiled_reference_enable;
  p_pic[ work_index ].range_map_yflag  = dec_cont->storage.range_map_yflag;
  p_pic[ work_index ].range_map_y      = dec_cont->storage.range_map_y;
  p_pic[ work_index ].range_map_uv_flag = dec_cont->storage.range_map_uv_flag;
  p_pic[ work_index ].range_map_uv     = dec_cont->storage.range_map_uv;

  p_pic[ work_index ].is_first_field = dec_cont->storage.pic_layer.is_ff;
  p_pic[ work_index ].is_top_field_first = dec_cont->storage.pic_layer.tff;
  p_pic[ work_index ].rff = dec_cont->storage.pic_layer.rff;
  p_pic[ work_index ].rptfrm = dec_cont->storage.pic_layer.rptfrm;

  /* fill correct field structure */
  ff_index = (dec_cont->storage.pic_layer.is_ff) ? 0 : 1;
  p_pic[ work_index ].field[ff_index].int_comp_f =
    dec_cont->storage.pic_layer.int_comp_field;
  p_pic[ work_index ].field[ff_index].i_scale_a =
    dec_cont->storage.pic_layer.i_scale;
  p_pic[ work_index ].field[ff_index].i_shift_a =
    dec_cont->storage.pic_layer.i_shift;
  p_pic[ work_index ].field[ff_index].i_scale_b =
    dec_cont->storage.pic_layer.i_scale2;
  p_pic[ work_index ].field[ff_index].i_shift_b =
    dec_cont->storage.pic_layer.i_shift2;
  p_pic[ work_index ].field[ff_index].type =
    dec_cont->storage.pic_layer.pic_type;
  p_pic[ work_index ].field[ff_index].pic_id = pic_id;
  /* reset post-processing status */
  p_pic[ work_index ].field[ff_index].dec_pp_stat = NONE;

}

/*------------------------------------------------------------------------------

    Function name: vc1hwdUpdateWorkBufferIndexes

        Functional description:
            Check anchor availability and update picture buffer work indexes.

        Inputs:
            dec_cont        Container for the decoder.
            is_bpic          Is current picture B or BI picture
            dec_ctrl

        Outputs:

        Returns:
            None

------------------------------------------------------------------------------*/
void vc1hwdUpdateWorkBufferIndexes( decContainer_t *dec_cont, u32 is_bpic, u32 dec_ctrl) {
  swStrmStorage_t * storage = &dec_cont->storage;

  if (storage->pic_layer.is_ff == HANTRO_TRUE) {
    u32 work0, work1;

    /* Determine work buffers (updating is done only after the frame
     * so here we assume situation after decoding frame */
    if( is_bpic ) {
      work0 = storage->work0;
      work1 = storage->work1;
    } else {
      work0 = storage->work_out;
      work1 = storage->work0;
    }
    /* Get free buffer */
    storage->work_out_prev = storage->work_out;
    storage->work_out = BqueueNext2(
                          &storage->bq,
                          work0,
                          work1,
                          BQUEUE_UNUSED,
                          is_bpic );
    if(storage->work_out == INVALID_ANCHOR_PICTURE)
      return;
    storage->p_pic_buf[storage->work_out].first_show = 1;
    if (dec_cont->pp_enabled) {
      struct DWLLinearMem *pp_buffer = NULL;
#ifdef GET_FREE_BUFFER_NON_BLOCK
      pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 0);
#else
      pp_buffer = InputQueueGetBuffer(dec_cont->pp_buffer_queue, 1);
#endif
      dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].pp_data = pp_buffer;
      if (pp_buffer == NULL) {
        storage->p_pic_buf[storage->work_out].first_show = 0;
        BqueuePictureRelease(&storage->bq, storage->work_out);
        return;
      }
#ifdef ENABLE_FPGA_VERIFICATION
      if (pp_buffer) {
        /* update device buffer by host buffer */
        DWLLinearMemset(dec_cont->dwl, pp_buffer, 0, 0, pp_buffer->size);
      }
#endif
      InputQueueSetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer, PP_OUT_CTRL(dec_ctrl));
    }

    if( is_bpic ) {
      storage->prev_bidx = storage->work_out;

    }
  }
}

enum ECDataState vc1hwdECHandleInData( decContainer_t *dec_cont,
                                      const VC1DecInput* input,
                                      strmData_t *stream_data,
                                      struct DecOutput* output ) {
  enum ECDataState re = EC_STATE_NONE;
  if (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) {
    re = EC_STATE_DISCARD;
    /* update stream position */
    (void)vc1hwdSeekFrameStart(&dec_cont->storage, stream_data);
    output->strm_curr_pos = stream_data->strm_curr_pos;
    output->strm_curr_bus_address = input->stream_bus_address +
                                    (stream_data->strm_curr_pos - stream_data->p_strm_buff_start);
    output->data_left = stream_data->strm_buff_size -
                        (stream_data->strm_curr_pos - stream_data->p_strm_buff_start);

    if(dec_cont->storage.profile != VC1_ADVANCED)
      output->data_left = 0;
  } else if (dec_cont->error_policy & DEC_EC_NO_SKIP) {
    re = EC_STATE_NONE;
  } else {
    //stub
  }
  return re;
}

enum ECDataState vc1hwdECHandleReconData(decContainer_t *dec_cont, u32 is_update_reon) {
  enum ECDataState re = EC_STATE_NONE;
  swStrmStorage_t * storage = &dec_cont->storage;
  u32 first_frame = (dec_cont->storage.first_frame) ? 1 : 0;
  u32 is_bframe = (storage->pic_layer.pic_type != PTYPE_I) &&
                  (storage->pic_layer.pic_type != PTYPE_P);
  if (dec_cont->error_policy == DEC_EC_FRAME_NO_ERROR)
    return EC_STATE_NONE;

  if (!first_frame) {
    if (!is_bframe) {
      storage->work_out_prev = storage->work0;
      if (!is_update_reon) {
        BqueueDiscard(&storage->bq, storage->work_out);
        storage->work_out = storage->work0;
      }
      re = EC_STATE_REPLACE;
    } else
      re = EC_STATE_NONE;
  } else {
    DWLLinearMemset(dec_cont->dwl, &storage->p_pic_buf[ storage->work_out ].data, 0, 128,
                    storage->num_of_mbs * 384);
    if (dec_cont->error_policy & DEC_EC_SEEK_NEXT_I) {
      storage->work_out_prev = INVALID_ANCHOR_PICTURE;
      storage->work0 = INVALID_ANCHOR_PICTURE;
      storage->work1 = INVALID_ANCHOR_PICTURE;
      re = EC_STATE_DISCARD;
    } else if (dec_cont->error_policy & DEC_EC_NO_SKIP){
      for (u32 i = 0; i < storage->bq.queue_size; i++) {
        if (storage->work_out != i) {
          if (storage->p_pic_buf[i].fcm == FIELD_INTERLACE &&
              storage->p_pic_buf[i].is_first_field == HANTRO_TRUE) {
            /* replace orphan 1.st field of previous frame in
            * frame buffer */
            if (storage->field_count)
              storage->field_count--;
          }
        }
      }
      re = EC_STATE_REPLACE;
    }
  }
    /* unbuffer first field if second field is corrupted */
  if ((!storage->pic_layer.is_ff && !storage->missing_field)) {
    /* replace orphan 1.st field of previous frame in
     * frame buffer */
    if (storage->field_count)
      storage->field_count--;
  }
    /* mark fcm as frame interlaced because concealment is frame based */
  if (storage->pic_layer.fcm == FIELD_INTERLACE) {
    /* conceal whole frame */
    storage->pic_layer.fcm = FRAME_INTERLACE;
  }

  return re;
}

void vc1ECMarkOutDataErrInfo(decContainer_t *dec_cont, u32 asic_status) {
  u32 error_info = DEC_NO_ERROR;
  u32 is_i_frame = (dec_cont->storage.pic_layer.pic_type == PTYPE_I);
  u32 is_ref_has_error = (is_i_frame) ? (0) :
                           ((dec_cont->storage.p_pic_buf[dec_cont->storage.work0].error_info != DEC_NO_ERROR) ||
                            (dec_cont->storage.p_pic_buf[dec_cont->storage.work1].error_info != DEC_NO_ERROR));
  if (asic_status &
        (DEC_HW_IRQ_ERROR | DEC_HW_IRQ_ASO |
         DEC_HW_IRQ_TIMEOUT | DEC_HW_IRQ_ABORT) ) {
    error_info = DEC_FRAME_ERROR;
  } else if (asic_status & DEC_HW_IRQ_RDY) {
    error_info = DEC_NO_ERROR;
  }

  if (is_ref_has_error)
    error_info |= DEC_REF_ERROR;
  dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_info = error_info;

}

enum ECDataState vc1hwdECGetOutDataAction(decContainer_t *dec_cont) {
  enum DecErrorInfo error_info = dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_info;
  u32 error_ratio = dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].error_ratio;
  u32 discard_error_pic = IsECDropOutput(dec_cont->error_policy,
                                         dec_cont->error_ratio,
                                         error_info, error_ratio);

  return discard_error_pic ? EC_STATE_DISCARD : EC_STATE_NONE;
}
