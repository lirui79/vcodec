/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#include "av1_obu.h"
#include "av1decapi.h"
#include "av1hwd_container.h"
#include "sw_stream.h"
#include "av1_metadata.h"
#include "dec_log.h"

/**@brief Select an available METADATA parameters buffer(AV1).
 * @param[in]    metadata_param      the metadata_paramarray(metadata_param[MAX_PIC_BUFFERS])
 * @param[in]    metadata_param_curr the current metadata_param in metadata_param array
 * @param[in]    metadata_param_num  the effective number of metadata_param in metadata_param array
 * @param[in]    pic_id              the current picture decode id
 * @param[out]   u32                 return value :
 *                                   -HANTRO_OK : metadata_param allocate successful
 *                                   -HANTRO_NOK : metadata_param allocate unsuccessful
*/
static u32 Av1PrepareCurMetadataParams(struct MetadataParameters **metadata_param,
                                    struct MetadataParameters **metadata_param_curr,
                                    u32 metadata_param_num, u32 pic_id) {
  u32 i;

  if (metadata_param_num == 0 || (*metadata_param_curr) == NULL)
    return HANTRO_NOK;

  if ((*metadata_param_curr)->decode_id != pic_id) {
    for (i = 0; i < metadata_param_num; i++) {
      if (metadata_param[i]->metadata_status == METADATA_UNUSED) {
        (*metadata_param_curr) = metadata_param[i];
        (*metadata_param_curr)->metadata_status = METADATA_CURRENT;
        STREAMTRACE_I("%s", "METADATA : select metadata_param[%d]. \n", i);
        return HANTRO_OK;
      }
    }
    if (i == metadata_param_num) return HANTRO_NOK;
  }

  return HANTRO_OK;
}

/**@brief Allocate METADATA parameters buffer(AV1).
 * @param[in]    metadata_param      the metadata_param array(metadata_param[MAX_PIC_BUFFERS])
 * @param[in]    metadata_param_curr the current metadata_param in metadata_param array
 * @param[in]    metadata_param_num  the effective number of metadata_param in metadata_param array
 * @param[in]    ext_buffer_num      the pp buffer num
 * @param[out]   u32                 return value :
 *                                   -HANTRO_OK : metadata_param allocate successful
 *                                   -HANTRO_NOK : metadata_param allocate unsuccessful
*/
static u32 Av1AllocateMetadataParams(struct MetadataParameters **metadata_param,
                                     struct MetadataParameters **metadata_param_curr,
                                     u32 *metadata_param_num, u32 ext_buffer_num) {

  if (*metadata_param_num < ext_buffer_num || *metadata_param_num < MAX_DPB_SIZE + 1) {
    metadata_param[*metadata_param_num] =
      (struct MetadataParameters*)DWLmalloc(sizeof(struct MetadataParameters));
    if (metadata_param[*metadata_param_num] == NULL) {
      STREAMTRACE_I("%s", "METADATA : Memory allocation failed.\n ");
      return HANTRO_NOK;
    }
    DWLmemset(metadata_param[*metadata_param_num], 0, sizeof(struct MetadataParameters));

    /* this is a new metadata_param, used it */
    (*metadata_param_curr) = metadata_param[*metadata_param_num];
    (*metadata_param_curr)->metadata_status = METADATA_CURRENT;
    STREAMTRACE_I("%s", "METADATA : select metadata_param[%d], which is a new metadata_param. \n",
                  *metadata_param_num);
    (*metadata_param_num)++;
      return HANTRO_OK;
  }

  STREAMTRACE_I("%s", "METADATA : No metadata_param is available.\n ");
  return HANTRO_NOK;
}

/**@brief Reset METADATA parameters buffer(AV1). \n
 * Clear the original data in metadata_param.
 * @param[in]    metadata_param_curr the current metadata_param in metadata_param array
 * @param[in]    metadata_param_num  the effective number of metadata_param in metadata_param array
 * @param[in]    pic_id              the current picture decode id
 * @param[out]   u32                 return value :
 *                                   -HANTRO_OK : metadata_param reset successful
 *                                   -HANTRO_NOK : metadata_param reset unsuccessful
*/
static u32 Av1ResetMetadataParams(struct MetadataParameters *metadata_param_curr,
                           u32 metadata_param_num, u32 pic_id) {
  enum MetadataStatus metadata_status;
  DecMetadataBuffer payload_reg;

  if (metadata_param_num == 0 || metadata_param_curr == NULL)
    return HANTRO_NOK;

  if (metadata_param_curr->decode_id != pic_id) {
    payload_reg = metadata_param_curr->t35_param.payload_byte;
    metadata_status    = metadata_param_curr->metadata_status;
    DWLmemset(metadata_param_curr, 0, sizeof(struct MetadataParameters));
    if (payload_reg.buffer != NULL) {
      DWLmemset(payload_reg.buffer, 0, payload_reg.available_size);
      payload_reg.available_size = 0;
    }
    metadata_param_curr->t35_param.payload_byte = payload_reg;
    metadata_param_curr->metadata_status = metadata_status;
    metadata_param_curr->decode_id = pic_id;
  }

  return HANTRO_OK;
}

i32 Av1LoadMetadataBuffer(struct Av1DecContainer *dec_cont,
                          u32 pic_id) {
  i32 ret;

  STREAMTRACE_I("%s", "METADATA : Start Decoding.\n");
  ret = Av1PrepareCurMetadataParams(dec_cont->metadata_param,
                                        &dec_cont->metadata_param_curr,
                                        dec_cont->metadata_param_num,
                                        pic_id);
  if (ret != HANTRO_OK) {
    /* no available metadata_param : allocate a new.
      * note : there is a limit on the number of metadata_param array. */
    ret = Av1AllocateMetadataParams(dec_cont->metadata_param,
                                    &dec_cont->metadata_param_curr,
                                    &dec_cont->metadata_param_num,
                                    dec_cont->num_buffers);
    if (ret != HANTRO_OK) {
      /* will be waiting for metadata_param[i] to become available in here. */
      return DEC_NO_DECODING_BUFFER;
    }
  }

  /* reset current metadata_param memory : the metadata_param cannot be NULL. */
  ret = Av1ResetMetadataParams(dec_cont->metadata_param_curr,
                               dec_cont->metadata_param_num, pic_id);
  if (ret != HANTRO_OK) {
    return DEC_NO_DECODING_BUFFER;
  }

  return HANTRO_OK;
}

av_unused void av1_trailing_bits(u32 nbBits, struct StrmData *rb) {
  u32 trailing_one_bit = SwGetBits(rb, 1);
  (void)(trailing_one_bit);
  nbBits--;
  while (nbBits > 0) {
    u32 trailing_zero_bit = SwGetBits(rb, 1);
    (void)(trailing_zero_bit);
    nbBits--;
  }
}

static u32 Av1ParseHDRCLL(struct StrmData *strm, struct LightLevelParameters *cll_param, u32 payload_size)
{
  u32 tmp;
  if (strm== NULL || cll_param == NULL) return HANTRO_NOK;

  tmp = SwGetBits(strm, 16);
  if (tmp == END_OF_STREAM) return (END_OF_STREAM);
  cll_param->max_pic_average_light_level = tmp;

  tmp = SwGetBits(strm, 16);
  if (tmp == END_OF_STREAM) return (END_OF_STREAM);
  cll_param->max_content_light_level = tmp;

  /* consume the redundante data, like traling bits */
  /* 2 is max_pic_average_light_level, 2 is max_content_light_level*/
  if(payload_size > 4) {
    tmp = payload_size - 4;
    while(tmp--)
      SwFlushBits(strm, 8);
  }

  return HANTRO_OK;
}

static u32 Av1ParseHDRMDCV(struct StrmData *strm, struct MasterDisplayParameters *mdcv_param,
                           u32 payload_size)
{
  u32 tmp, i = 0;

  if (strm== NULL || mdcv_param == NULL) return HANTRO_NOK;

  for(i=0; i<3; i++) {
    tmp = SwGetBits(strm, 16);
    if (tmp == END_OF_STREAM) return (END_OF_STREAM);
    mdcv_param->display_primaries_x[i] = tmp;

    tmp = SwGetBits(strm, 16);
    if (tmp == END_OF_STREAM) return (END_OF_STREAM);
    mdcv_param->display_primaries_y[i] = tmp;
  }

  tmp = SwGetBits(strm, 16);
  if (tmp == END_OF_STREAM) return (END_OF_STREAM);
  mdcv_param->white_point_x = tmp;

  tmp = SwGetBits(strm, 16);
  if (tmp == END_OF_STREAM) return (END_OF_STREAM);
  mdcv_param->white_point_y = tmp;

  tmp = SwGetBits32(strm);
  if (tmp == END_OF_STREAM) return (END_OF_STREAM);
  mdcv_param->max_display_mastering_luminance = tmp;

  tmp = SwGetBits32(strm);
  if (tmp == END_OF_STREAM) return (END_OF_STREAM);
  mdcv_param->min_display_mastering_luminance = tmp;

  /* consume the redundante data, like traling bits */
  /* 12 is display_primaries_x|y, 4 is white_point_x|y, 8 is min|max_display_mastering_luminance*/
  if(payload_size > 24) {
    tmp = payload_size - 24;
    while(tmp--)
      SwFlushBits(strm, 8);
  }

  return HANTRO_OK;
}

static u32 Av1ParseT35(struct StrmData *strm, struct T35Parameters *t35_param, u32 payload_size)
{
  u32 tmp, conformed_payload_size;

  if(payload_size < 1) {
    STREAMTRACE_E("%s", "at lease one byte of the payload data\n");
    ASSERT(0);
    return HANTRO_NOK;
  }

  /* itu_t_t35_country_code */
  tmp = SwGetBits(strm, 8);
  t35_param->itu_t_t35_country_code[t35_param->counter] = tmp;
  conformed_payload_size = 1;

  /* itu_t_t35_country_code_extension_byte */
  if (tmp == 0xFF) {
    if((payload_size-conformed_payload_size) > 1) {
      tmp = SwGetBits(strm, 8);
      t35_param->itu_t_t35_country_code_extension_byte[t35_param->counter] = tmp;
      conformed_payload_size = 2;
    }
    else /* workaround */
      t35_param->itu_t_t35_country_code_extension_byte[t35_param->counter] = 0;
  }

  /* Any payload data is present for this OBU type, at lease one byte of the payload data
   (traling bit is) shall not be euqal to 0 */
  if(payload_size > conformed_payload_size) {
    DecMetadataBuffer *payload_byte = &t35_param->payload_byte;
    u8 *buffer;
    u32 real_payload_size, size;
    real_payload_size = payload_size - conformed_payload_size;
    /* check buffer */
    if (payload_byte->buffer == NULL) { /*first decode this Metadata in one frame */
      size = real_payload_size << 2;
      buffer = (u8*)DWLcalloc(1, size);
      if (buffer == NULL) return HANTRO_NOK;
      payload_byte->buffer = buffer;
      payload_byte->buffer_size = size;
      payload_byte->available_size = 0;
    }
    else { /* second decode this Metadata in one frame */
      if ((payload_byte->buffer_size - payload_byte->available_size) < real_payload_size) {
        /* don't have enough buffer, and reallocate buffer to quadruple capacity */
        size = payload_byte->buffer_size + (real_payload_size << 2);
        buffer = (u8*)DWLcalloc(1, size);
        if (buffer == NULL) return (HANTRO_NOK);
        DWLmemcpy(buffer, payload_byte->buffer, payload_byte->available_size);
        DWLfree(payload_byte->buffer); /* free the previous buffer */
        payload_byte->buffer = buffer;
        payload_byte->buffer_size = size;
      }
    }

    /* itu_t_t35_payload_bytes */
    while(real_payload_size--) /* include trailing_bits */{
      tmp = SwGetBits(strm, 8);
      payload_byte->buffer[payload_byte->available_size++] = tmp;
      t35_param->payload_byte_length[t35_param->counter]++;
    }
  } else {
    STREAMTRACE_I("%s", "no itu_t_t35_payload_bytes\n");
  }

  /* T35 counter : in one frame */
  t35_param->counter++;
  RESET_SEI_COUNTER(&t35_param->counter);

  return HANTRO_OK;
}

i32 Av1DecMetadataParse(struct MetadataParameters *metadata_param, struct StrmData *strm,
                        obuHeader_t *header) {
  av_unused i32 ret, tmp;
  u32 payload_bits, read_bits;
  int l = 0;
  OBU_METADATA_TYPE metadata_type;

  tmp = strm->strm_buff_read_bits;
  metadata_type = leb128(strm, &l);
  u32 residule_payload_size_bytes = header->payload_size - (strm->strm_buff_read_bits - tmp) / 8;
  switch(metadata_type) {
    case OBU_METADATA_TYPE_HDR_CLL: {
      ret = Av1ParseHDRCLL(strm, &metadata_param->cll_param, residule_payload_size_bytes);
      if(ret != HANTRO_OK) {
        metadata_param->lightlevel_present_flag = 0;
        STREAMTRACE_I("%s", "METADATA HDR CLL: Decoding Failed.\n");
        return ret;
      }
      else {
        metadata_param->lightlevel_present_flag = 1;
        STREAMTRACE_I("METADATA HDR CLL: Decoding successful, metadata_type %d\n", metadata_type);
      }
      break;
    }
    case OBU_METADATA_TYPE_HDR_MDCV: {
      ret = Av1ParseHDRMDCV(strm, &metadata_param->mdcv_param, residule_payload_size_bytes);
      if(ret != HANTRO_OK) {
        metadata_param->mastering_display_present_flag = 0;
        STREAMTRACE_E("%s", "METADATA HDR MDCV: Decoding Failed.\n");
        return ret;
      }
      else {
        metadata_param->mastering_display_present_flag = 1;
        STREAMTRACE_I("METADATA HDR MDCV: Decoding successful, metadata_type %d\n", metadata_type);
      }
      break;
    }
    case OBU_METADATA_TYPE_ITUT_T35: {
      ret = Av1ParseT35(strm, &metadata_param->t35_param, residule_payload_size_bytes);
      if(ret != HANTRO_OK) {
        metadata_param->t35_present_flag = 0;
        STREAMTRACE_E("%s", "METADATA ITUT T35: Decoding Failed.\n");
        return ret;
      }
      else {
        metadata_param->t35_present_flag = 1;
        STREAMTRACE_I("METADATA ITUT T35: Decoding successful, metadata_type %d\n", metadata_type);
      }
      break;
    }
    default: {
      u32 residule_payload_size_bytes = header->payload_size - (strm->strm_buff_read_bits - tmp)/8;
      while(residule_payload_size_bytes--)
        if (SwFlushBits(strm, 8) != HANTRO_OK) {
          STREAMTRACE_E("%s", "Av1DecMetadataParse: can't consume the bits completely correctly\n");
          return DEC_STRM_ERROR;
        }
      break;
    }
  }

  payload_bits = header->payload_size * 8;
  read_bits = strm->strm_buff_read_bits - tmp;
  if (payload_bits < read_bits) {
    STREAMTRACE_E("%s", "Av1DecMetadataParse: can't consume the bits completely correctly\n");
    return DEC_STRM_ERROR;
  }

  return HANTRO_OK;
}

void Av1UpdateMetadataInfo(struct SEI_buffer *sei_buffer, struct MetadataParameters *metadata_param){
  UNUSED(sei_buffer);
#ifdef SUPPORT_METADATA
  if (metadata_param) {
    u32 tmp_pic_id = metadata_param->decode_id;
    enum MetadataStatus  tmp_status = metadata_param->metadata_status;
    DWLmemset(metadata_param, 0, sizeof(struct MetadataParameters));
    metadata_param->decode_id = tmp_pic_id;
    metadata_param->metadata_status = tmp_status;
  }
#endif
}
