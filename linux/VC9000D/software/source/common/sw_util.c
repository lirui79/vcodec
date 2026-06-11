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

#if defined(_WIN32) || defined(WIN32)
#include <malloc.h>
#else
#include <alloca.h>
#endif
#include "sw_util.h"
#include "sw_debug.h"
#include "sw_stream.h"
#include "vpufeature.h"
#include "dec_log.h"

u32 SwCountLeadingZeros(u32 value, u32 length) {

  u32 zeros = 0;
  u32 mask = (u32)1 << (length - 1);

  ASSERT(length <= 32);

  while (mask && !(value & mask)) {
    zeros++;
    mask >>= 1;
  }
  return (zeros);
}

u32 SwNumBits(u32 value) {
  return 32 - SwCountLeadingZeros(value, 32);
}

/*
* return a pointer which points to strm and guarantee there're at least 2 bytes space prior to the pointer for checking emulation prevention byte.
* like x x p x x x x x, p points to strm.
* there're 3 turn around types, respectively
* 0: no turn around.
* 1: turn around and strm is away from buf start more than (>=) 2 bytes, like x x x x s x x x, s is strm
* 2: turn around but strm is away from buf start less than (<) 2 bytes, like x s x x x x, s is strm.
* type 2 is used to handle corner case when emulation prevention byte (00 00 03) is splited by buf end
*/
u8* SwTurnAround(const u8 * strm, const u8* buf, u8* tmp_buf, u32 buf_size, u32 num_bits, struct strmInfo *stream_info) {
  u32 bytes = (num_bits+7)/8;
  u32 turn_around_type = 0;
  SwReadByteFn *fn_read_byte;

  if((strm + bytes) > (buf + buf_size))
    turn_around_type = 1;

  if((addr_t)strm - (addr_t)buf < 2)  {
    ASSERT(turn_around_type == 0);
    turn_around_type = 2;
  }

  if(turn_around_type == 0) {
    return NULL;
  } else if(turn_around_type == 1) {
    i32 i;
    u32 bytes_left = (u32)((addr_t)(buf + buf_size) - (addr_t)strm);

    if (stream_info && stream_info->low_latency)
      stream_info->is_back_to_buffer_begin = 1;
    fn_read_byte = SwGetReadByteFunc(stream_info);

    /* turn around */
    for(i = -3; i < (i32)0; i++) {
      tmp_buf[3 + i] = DWLPrivateAreaReadByte(strm + i);
    }
    for(i = 0; i < (i32)bytes_left; i++) {
      tmp_buf[3 + i] = fn_read_byte(strm + i, buf_size, stream_info);
    }
    /*turn around point*/
    for(i = 0; i < (i32)(bytes - bytes_left); i++) {
      tmp_buf[3 + bytes_left + i] = fn_read_byte(buf + i, buf_size, stream_info);
    }

    return tmp_buf+3;
  } else {
    i32 i;
    u32 left_byte = (u32)((addr_t) strm - (addr_t) buf);

    fn_read_byte = SwGetReadByteFunc(stream_info);

    /* turn around */
    for(i = 0; i < 2; i++) {
      tmp_buf[i] = DWLPrivateAreaReadByte(buf + buf_size - 2 + i);
    }

    /*turn around point*/
    for(i = 0; i < (i32)(bytes + left_byte); i++) {
      tmp_buf[i + 2] = fn_read_byte(buf + i, buf_size, stream_info);
    }

    return (tmp_buf + 2 + left_byte);
  }
}

#ifdef STACK_STAT
u32 *stack_top = NULL;
u32 stack_size = 0;

#define STACK_STAT_SIZE (100*1024)
#define MAGICWORD 0xDEADBEEF

void StackStatInit() {
  int i;
  u32 *p;

  if (stack_top == NULL)
  	stack_top = (u32 *)alloca(STACK_STAT_SIZE);  //cppcheck-suppress allocaCalled

  for (i = 0, p = stack_top; i < STACK_STAT_SIZE/sizeof(u32); i++) {
    *p++ = MAGICWORD;
  }
}

void StackConsumption(const char *func) {
  int i;
  u32 *p;
  u32 ss;

  for (i = 0, p = stack_top; i < STACK_STAT_SIZE/sizeof(u32); i++) {
    if (*p == MAGICWORD) {
      p++;
    } else {
      ss = STACK_STAT_SIZE - (p - stack_top) * sizeof(u32);
      if (ss > stack_size)
        stack_size = ss;
      break;
    }
  }

  printf("%s Stack Size: %u bytes\n", func, stack_size);
}
#endif

u32 SwGetCores(u32 core_mask) {
  u32 i, n_cores = 0;

  for (i=0; i<8*sizeof(u32); i++) {
    if ((core_mask >> i) & 0x1)
      n_cores++;
  }

  return n_cores;
}

u32 SwGetCoreMaskByFeature(const void *dwl_inst, enum FEATURE feature) {
  u32 i, core_mask = 0;
  const struct DecHwFeatures *hw_feature = NULL;
  u32 n_cores = DWLReadAsicCoreCount(dwl_inst);

#define SET_CORE_MASK(format)     if (hw_feature->format##_support) \
                                    core_mask |= 1 << i;
  for (i=0; i<n_cores; i++) {
    hw_feature = DWLGetHwFeaturesByID(dwl_inst, i);
    switch (feature) {
      case VSI_HEVC:           SET_CORE_MASK(hevc);             break;
      case VSI_HEVC_MAIN10:    SET_CORE_MASK(hevc_main10);      break;
      case VSI_HEVC_422_INTRA: SET_CORE_MASK(hevc_422_intra);   break;
      case VSI_VP9:            SET_CORE_MASK(vp9);              break;
      case VSI_VP9_PROFILE2:   SET_CORE_MASK(vp9_profile2);     break;
      case VSI_H264:           SET_CORE_MASK(h264);             break;
      case VSI_H264_ADV:       SET_CORE_MASK(h264_adv);         break;
      case VSI_H264_HIGH10:    SET_CORE_MASK(h264_high10);      break;
      case VSI_H264_422_INTRA: SET_CORE_MASK(h264_422_intra);   break;
      case VSI_CUSTOM_MPEG4:   SET_CORE_MASK(custom_mpeg4);     break;
      case VSI_SORENSON_SPARK: SET_CORE_MASK(sorenson_spark);   break;
      case VSI_VP8:            SET_CORE_MASK(vp8);              break;
      case VSI_WEBP:           SET_CORE_MASK(webp);             break;
      case VSI_VP7:            SET_CORE_MASK(vp7);              break;
      case VSI_AVS2:           SET_CORE_MASK(avs2);             break;
      case VSI_AVS2_MAIN10:    SET_CORE_MASK(avs2_main10);      break;
      case VSI_RFC:            SET_CORE_MASK(rfc);              break;
      case VSI_QP_DUMP:        SET_CORE_MASK(qp_dump);          break;
      case VSI_LOW_LATENCY:    SET_CORE_MASK(low_latency_mode); break;
      case VSI_VP9_HW_PROB:    SET_CORE_MASK(vp9_hw_prob);      break;
      default: break;
    }
  }

  return core_mask;
}

const struct DecHwFeatures *SwGetHwFeature(const void *dwl_inst, u32 core_mask, enum DWLClientType client_type) {
  const void *hw_feature = NULL;
  u32 i, n_cores;

  if (core_mask) {
    n_cores = DWLReadAsicCoreCount(dwl_inst);
    for (i=0; i<n_cores; i++) {
      if ((core_mask >> i) & 0x1)
        break;
    }
    if (i == n_cores) {
      APITRACEERR("%s","SwGetHwFeature# error misc_ctrl, please fix.\n");
      ASSERT(0);
    }
    else
      hw_feature = DWLGetHwFeaturesByID(dwl_inst, i);
  }

  if (!hw_feature)
    hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, client_type, &core_mask);

  ASSERT(hw_feature);

  return hw_feature;
}

static void SwGetMaxWxHByClientType(const struct DecHwFeatures *hw_feature,
                                    enum DWLClientType client_type, u32 align,
                                    u32 *max_width, u32 *max_height) {
#define SET_MAX_WXH(format)  *max_width = hw_feature->format##_max_dec_pic_width; \
                             *max_height = hw_feature->format##_max_dec_pic_height;
  switch (client_type) {
    case DWL_CLIENT_TYPE_H264_DEC: SET_MAX_WXH(h264);   break;
    case DWL_CLIENT_TYPE_MPEG4_DEC: SET_MAX_WXH(mpeg4); break;
    case DWL_CLIENT_TYPE_JPEG_DEC:
      *max_width = hw_feature->img_max_dec_width;
      *max_height = hw_feature->img_max_dec_height;     break;
    case DWL_CLIENT_TYPE_VC1_DEC:   SET_MAX_WXH(vc1);   break;
    case DWL_CLIENT_TYPE_MPEG2_DEC: SET_MAX_WXH(mpeg2); break;
    case DWL_CLIENT_TYPE_VP6_DEC:   SET_MAX_WXH(vp6);   break;
    case DWL_CLIENT_TYPE_AVS_DEC:   SET_MAX_WXH(avs);   break;
    case DWL_CLIENT_TYPE_RV_DEC:    SET_MAX_WXH(rv);    break;
    case DWL_CLIENT_TYPE_VP8_DEC:   SET_MAX_WXH(vp8);   break;
    case DWL_CLIENT_TYPE_VP9_DEC:   SET_MAX_WXH(vp9);   break;
    case DWL_CLIENT_TYPE_HEVC_DEC:  SET_MAX_WXH(hevc);  break;
    case DWL_CLIENT_TYPE_AVS2_DEC:  SET_MAX_WXH(hevc);  break;//FIXME, DecHwFeaturesno avs2 max WxH filed?
    case DWL_CLIENT_TYPE_AV1_DEC:   SET_MAX_WXH(av1);   break;
    case DWL_CLIENT_TYPE_VVC_DEC:   SET_MAX_WXH(vvc);   break;
    default: break;
  }
  *max_width = NEXT_MULTIPLE(*max_width, align);
  *max_height = NEXT_MULTIPLE(*max_height, align);
}

/* align 1 means intra for DWL_CLIENT_TYPE_VP8_DEC */
/* align 0 means thumb for DWL_CLIENT_TYPE_JPEG_DEC */
u32 SwAdjustCoreMaskByWxH(const void *dwl_inst, u32 width, u32 height, u32 align,
                          enum DWLClientType client_type, u32 *core_mask) {
  u32 i, tmp = 0, max_width, max_height, min_width, min_height;
  u32 bak_non_core_mask, is_check_WxH = 0, is_intra = 0;
  const struct DecHwFeatures *hw_feature = NULL;
  u32 n_cores = DWLReadAsicCoreCount(dwl_inst);

  if (LEGACY_CLEINT_TYPE(client_type)) {
    min_width= MIN_PIC_WIDTH_G1;
    min_height = MIN_PIC_HEIGHT_G1;
  }
  else if (client_type == DWL_CLIENT_TYPE_AVS2_DEC) {
    min_width = MIN_PIC_WIDTH_AVS2;
    min_height = MIN_PIC_HEIGHT_AVS2;
  }
  else if (client_type == DWL_CLIENT_TYPE_H264_DEC) {
    min_width = MIN_PIC_WIDTH_H264;
    min_height = MIN_PIC_HEIGHT_H264;
  }
  else {
    min_width = MIN_PIC_WIDTH;
    min_height = MIN_PIC_HEIGHT;
  }

  if (client_type == DWL_CLIENT_TYPE_JPEG_DEC ||
      client_type == DWL_CLIENT_TYPE_VP6_DEC) {
    is_check_WxH = 1;
  }

  if (client_type == DWL_CLIENT_TYPE_VP8_DEC) {
    is_intra = align;
    align = 1;
  }

  /* check picture size (minimum defined in decapicommon.h) */
  for (i=0; i<n_cores; i++) {
    hw_feature = DWLGetHwFeaturesByID(dwl_inst, i);
    if (client_type == DWL_CLIENT_TYPE_JPEG_DEC && align == 0) {
      max_width = JPEGDEC_MAX_WIDTH_TN;
      max_height = JPEGDEC_MAX_HEIGHT_TN;
      align = 1;
    }
    else
      SwGetMaxWxHByClientType(hw_feature, client_type, align, &max_width, &max_height);
    if (width <= max_width && height <= max_height &&
        width >= min_width && height >= min_height &&
        ((is_check_WxH | is_intra) ? width*height <= max_width*max_height : 1) &&
        (is_intra ? hw_feature->webp_support : 1))
      tmp |= 1 << i;
  }

  bak_non_core_mask = NON_CORE_MASK(*core_mask);
  if (CORE_MASK(*core_mask))
    *core_mask &= tmp;
  else
    *core_mask = tmp;

  *core_mask |= bak_non_core_mask;

  return tmp;
}