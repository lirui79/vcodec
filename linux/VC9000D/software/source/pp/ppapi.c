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
#include "version.h"
#include "basetype.h"
#include "ppapi.h"
#include "ppinternal.h"
#include "regdrv.h"
#include "dwl.h"
#include "decppif.h"
#include "ppcfg.h"
#include "ppu.h"
#include "deccfg.h"
#include "commonconfig.h"
#include "sw_util.h"
#include "sw_debug.h"
#include <string.h>
#include "dec_log.h"

static const u32 coeff[6][9] = {{4899,9617,1868,2765,5427,8192,8192,6860,1332},
                                {4207,8260,1604,2428,4768,7196,7196,6026,1170},
                                {3483,11718,1183,1877,6315,8192,8192,7441,751},
                                {2991,10064,1016,1649,5547,7196,7196,6536,660},
                                {4304,11108,972,2288,5904,8192,8192,7533,659},
                                {3696,9540,834,2010,5187,7196,7196,6617,579}};
static u32 PPCycleCount(PPContainer *pp_c, PPDecPicture *output);

/*------------------------------------------------------------------------------
    Function name   : PPInit
    Description     : initialize pp
    Return type     : enum DecRet
    Argument        : PPInst * post_pinst
------------------------------------------------------------------------------*/
enum DecRet PPInit(PPInst * p_post_pinst, const void *dwl) {
  PPContainer *p_pp_cont;
  u32 core_mask;

  if(p_post_pinst == NULL || dwl == NULL) {
    return (DEC_PARAM_ERROR);
  }

  *p_post_pinst = NULL; /* return NULL instance for any error */
  /* check that VP6 decoding supported in HW */
  DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_ST_PP, &core_mask);
  if (core_mask == 0) {
    APITRACEERR("%s","PPInit# PP standalone not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }
  p_pp_cont = (PPContainer *) DWLmalloc(sizeof(PPContainer));

  if(p_pp_cont == NULL) {
    return (DEC_MEMFAIL);
  }

  DWLmemset(p_pp_cont, 0, sizeof(PPContainer));
  p_pp_cont->dwl = dwl;
  p_pp_cont->core_mask = core_mask;
  p_pp_cont->secure_mode = 0;
  SET_CLIENT_TYPE(p_pp_cont->core_mask, DWL_CLIENT_TYPE_ST_PP);
  SET_SECURE_MODE(p_pp_cont->core_mask , p_pp_cont->secure_mode);

  p_pp_cont->pp_regs[0] = DWLReadAsicID(p_pp_cont->dwl, DWL_CLIENT_TYPE_ST_PP);
  SetDecRegister(p_pp_cont->pp_regs, HWIF_DEC_MODE, 14);
  SetCommonConfigRegs(p_pp_cont->pp_regs);
  *p_post_pinst = p_pp_cont;
  p_pp_cont->vcmd_used = DWLVcmdIsUsed(dwl);
  p_pp_cont->enable_3dlut = 0;
  return (DEC_OK);
}

/*------------------------------------------------------------------------------
    Function name   : PPSetInfo
    Description     :
    Return type     : enum DecRet
    Argument        : PPInst post_pinst
    Argument        : PPConfig * p_pp_conf
------------------------------------------------------------------------------*/
enum DecRet PPSetInfo(PPInst post_pinst, PPConfig * p_pp_conf) {
  PPContainer *pp_c = (PPContainer *) post_pinst;;
  u32 i;
  u32 bit_depth;
  const struct DecHwFeatures *hw_feature = NULL;
  PpUnitIntConfig *ppu_cfg;

  if(post_pinst == NULL || p_pp_conf == NULL)
    return (DEC_PARAM_ERROR);

  pp_c = (PPContainer *) post_pinst;

  if (CORE_MASK(p_pp_conf->misc_ctrl) != 0) {
    u32 bak_non_core_mask = NON_CORE_MASK(pp_c->core_mask);
    pp_c->core_mask = CORE_MASK(p_pp_conf->misc_ctrl);
    pp_c->core_mask |= bak_non_core_mask;
  }
  hw_feature = SwGetHwFeature(pp_c->dwl, CORE_MASK(pp_c->core_mask), DWL_CLIENT_TYPE_ST_PP);
  if (!hw_feature) {
    APITRACEDEBUG("%s","PPSetInfo# not found any hw_feature.\n");
    return DEC_PARAM_ERROR;
  }

  if (((p_pp_conf->in_format == PP_IN_NV12_8BIT || p_pp_conf->in_format == PP_IN_NV21_8BIT || p_pp_conf->in_format == PP_IN_420P_8BIT
      || p_pp_conf->in_format == PP_IN_422_NV16_8BIT || p_pp_conf->in_format == PP_IN_422_NV61_8BIT || p_pp_conf->in_format == PP_IN_444P_8BIT
      || p_pp_conf->in_format == PP_IN_RGB888_P || p_pp_conf->in_format == PP_IN_BGR888_P) && p_pp_conf->in_stride < p_pp_conf->in_width)) {
    APITRACEERR("%s","Input stride illegal: the stride of NV12/NV21/420P/NV16/NV61/444P/RGB_Planar 8bit format must be not less than input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_NV12_P010 || p_pp_conf->in_format == PP_IN_NV21_P010 || p_pp_conf->in_format == PP_IN_420P_P010
      || p_pp_conf->in_format == PP_IN_422_NV16_P010 || p_pp_conf->in_format == PP_IN_422_NV61_P010 || p_pp_conf->in_format == PP_IN_444P_P010
      || p_pp_conf->in_format == PP_IN_R16G16B16_P || p_pp_conf->in_format == PP_IN_B16G16R16_P) && p_pp_conf->in_stride < 2 * p_pp_conf->in_width) {
    APITRACEERR("%s","Input stride illegal: the stride of NV12/NV21/420P/NV16/NV61/444P p010 and R16G16B16/B16G16R16 planar format must be not less than twice input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_TILED4x4) && p_pp_conf->in_stride < 4 * p_pp_conf->in_width) {
    APITRACEERR("%s","Input stride illegal: the stride of tile4x4 format must be not less than four times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_TILED4x4_P010) && p_pp_conf->in_stride < 8 * p_pp_conf->in_width) {
    APITRACEERR("%s","Input stride illegal: the stride of tile4x4 p010 format must be not less than eight times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if((p_pp_conf->in_format == PP_IN_RGB888 || p_pp_conf->in_format == PP_IN_BGR888) && p_pp_conf->in_stride < 3 * p_pp_conf->in_width) {
    APITRACEERR("%s","Input stride illegal: the stride of RGB888 /BGR888 format must be not less than three times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_ARGB888 ||
    p_pp_conf->in_format == PP_IN_ABGR888 ||
    p_pp_conf->in_format == PP_IN_RGBA888 ||
    p_pp_conf->in_format == PP_IN_BGRA888 ||
    /*p_pp_conf->in_format == PP_IN_XRGB888 ||
    p_pp_conf->in_format == PP_IN_XBGR888 ||*/
    p_pp_conf->in_format == PP_IN_A2R10G10B10 ||
    p_pp_conf->in_format == PP_IN_A2B10G10R10 ||
    p_pp_conf->in_format == PP_IN_X2R10G10B10 ||
    p_pp_conf->in_format == PP_IN_X2B10G10R10 ||
    p_pp_conf->in_format == PP_IN_R10G10B10A2 ||
    p_pp_conf->in_format == PP_IN_B10G10R10A2) && p_pp_conf->in_stride < 4 * p_pp_conf->in_width) {
    APITRACEERR("%s","Input stride illegal: the stride of ARGB888/ABRG888/RGBA888/BGRA888/XRGB888/XBRG888/A2R10G10B10/A2B10G10R10/X2R10G10B10/X2B10G10R10/R10G10B10A2/B10G10R10A2 format must be not less than four times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if (p_pp_conf->in_tiled_mode == IN_TILED64x64 && p_pp_conf->in_stride < p_pp_conf->in_width * 4 * 64) {
    APITRACEERR("%s","Input stride illegal: the stride of ARGB888/XRGB888/A2R10G10B10/X2R10G10B10 tile64x64 format must be not less than 4 * 64 times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_NV12_8BIT && p_pp_conf->in_tiled_mode == IN_TILED8x8) && p_pp_conf->in_stride < p_pp_conf->in_width * 8) {
    APITRACEERR("%s","Input stride illegal: the stride of YUV tile8x8 8bit format must be not less than 8 times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_NV12_P010 && p_pp_conf->in_tiled_mode == IN_TILED8x8) && p_pp_conf->in_stride < p_pp_conf->in_width * 2 * 8) {
    APITRACEERR("%s","Input stride illegal: the stride of YUV tile8x8 p010 format must be not less than 2 * 8 times input width.\n");
    return (DEC_PARAM_ERROR);
  } else if ((p_pp_conf->in_format == PP_IN_422_YUYV_8BIT || p_pp_conf->in_format == PP_IN_422_YVYU_8BIT || p_pp_conf->in_format == PP_IN_422_UYVY_8BIT || p_pp_conf->in_format == PP_IN_422_VYUY_8BIT) && p_pp_conf->in_stride < p_pp_conf->in_width * 2){
    APITRACEERR("%s","Input stride illegal: the stride of 422packed(YUYV/YVYU/UYVY/VYUY) format must be not less than 2 times input width.\n");
    return (DEC_PARAM_ERROR);
  }

  if (p_pp_conf->in_rfc)
    bit_depth = (p_pp_conf->in_luma_bitdepth == 8 && p_pp_conf->in_chroma_bitdepth == 8) ? 8 : 10;
  else
    bit_depth = IS_PP_IN_8BIT(p_pp_conf->in_format) ? 8 : 10;

  ppu_cfg = pp_c->ppu_cfg;
  pp_c->in_format = p_pp_conf->in_format;
  pp_c->in_stride = p_pp_conf->in_stride;
  pp_c->in_stride_ch = p_pp_conf->in_stride_ch;
  pp_c->in_height = p_pp_conf->in_height;
  pp_c->in_width = p_pp_conf->in_width;
  pp_c->in_luma_stride = p_pp_conf->in_lu_stride;
  pp_c->in_chroma_stride = p_pp_conf->in_ch_stride;
  pp_c->in_luma_table_stride = p_pp_conf->in_lut_stride;
  pp_c->in_chroma_table_stride = p_pp_conf->in_cht_stride;
  pp_c->in_luma_bitdepth = p_pp_conf->in_luma_bitdepth;
  pp_c->in_chroma_bitdepth = p_pp_conf->in_chroma_bitdepth;
  pp_c->in_chroma_format_idc = p_pp_conf->in_chroma_format_idc;
  pp_c->pp_in_buffer = p_pp_conf->pp_in_buffer;
  pp_c->pp_out_buffer = p_pp_conf->pp_out_buffer;
  pp_c->in_tiled_mode = p_pp_conf->in_tiled_mode;
  pp_c->in_dec400 = p_pp_conf->in_dec400;
  pp_c->in_dec400_a = p_pp_conf->in_dec400_a;

  /* ref aligment */
  // dec_cont->align = dec_cfg->align;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    /* ppu alignment */
    p_pp_conf->ppu_config[i].align = p_pp_conf->align;

    /* Range Mapping */
    // ppstandalone only can get yuv/rgb source_range from cmodel option
    ppu_cfg[i].source_range = p_pp_conf->ppu_config[i].source_range;

    ppu_cfg[i].pp_in_org_width = p_pp_conf->in_width;
    ppu_cfg[i].pp_in_org_height = p_pp_conf->in_height;

    /* ppu dwl instance */
    ppu_cfg[i].dwl = pp_c->dwl;

    /* 3dlut */
    if (p_pp_conf->ppu_config[i].enable_3dlut && (p_pp_conf->ppu_config[i].rgb || p_pp_conf->ppu_config[i].rgb_planar)) {
      pp_c->enable_3dlut = 1;
    }
    ppu_cfg[i].table_3dlut_buffer = p_pp_conf->table_3dlut_buffer;
  }

  PpUnitSetIntConfig(ppu_cfg, p_pp_conf->ppu_config, hw_feature, bit_depth, 1, !(p_pp_conf->in_chroma_format_idc || IS_PP_IN_RGB(p_pp_conf->in_format)));
  u32 in_width, in_height;
  in_width = p_pp_conf->in_width;
  in_height = p_pp_conf->in_height;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    pp_c->ppu_cfg[i].lanczos_table.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    if (pp_c->ppu_cfg[i].lanczos_table.virtual_address == NULL) {
      u32 size = LANCZOS_COEFF_BUFFER_SIZE * sizeof(i16);
      i32 ret = DWLMallocLinear(pp_c->dwl, size, &pp_c->ppu_cfg[i].lanczos_table);
      if (ret != 0)
        return(DEC_MEMFAIL);
    }
  }
  if (CheckPpUnitConfig(hw_feature, in_width, in_height, 0, bit_depth, (p_pp_conf->in_format >= 16 && p_pp_conf->in_format <= 27) ? PP_CHROMA_444 : PP_CHROMA_420, ppu_cfg))
    return DEC_PARAM_ERROR;

  /* rgb2yuv param config */
  if (IS_PP_IN_RGB(p_pp_conf->in_format)) {
    ppu_cfg->rgb_stan = p_pp_conf->ppu_config->rgb_stan;
    ppu_cfg->video_range = p_pp_conf->ppu_config->video_range;
#ifndef PPU_V9_2_3
    if (p_pp_conf->ppu_config->video_range) {
      /* full range */
      if (bit_depth == 8) {
        ppu_cfg->range_max = 255;
        ppu_cfg->range_min = 0;
      } else {
        ppu_cfg->range_max = 1023;
        ppu_cfg->range_min = 0;
      }
    } else {
      /* limited range */
      if (bit_depth == 8) {
        ppu_cfg->range_max = 235;
        ppu_cfg->range_min = 16;
      } else {
        ppu_cfg->range_max = 940;
        ppu_cfg->range_min = 64;
      }
    }
#else
    if (bit_depth == 8) {
      ppu_cfg->range_max = 255;
      ppu_cfg->range_min = 0;
    } else {
      ppu_cfg->range_max = 1023;
      ppu_cfg->range_min = 0;
    }
#endif
  }

  pp_c->pp_enabled = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++)
  pp_c->pp_enabled |= ppu_cfg[i].enabled;
  CalcPpUnitBufferSize(ppu_cfg, 0);
  return (DEC_OK);
}

/*------------------------------------------------------------------------------
    Function name   : PPRelease
    Description     :
    Return type     : void
    Argument        : PPInst post_pinst
------------------------------------------------------------------------------*/
void PPRelease(PPInst post_pinst) {
  PPContainer *pp_c;
  u32 i;
  if(post_pinst == NULL) {
    return;
  }

  pp_c = (PPContainer *) post_pinst;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if (pp_c->ppu_cfg[i].lanczos_table.virtual_address) {
      DWLFreeLinear(pp_c->dwl, &pp_c->ppu_cfg[i].lanczos_table);
      pp_c->ppu_cfg[i].lanczos_table.virtual_address = NULL;
    }
  }

  DWLfree(pp_c);
}

/*------------------------------------------------------------------------------
    Function name   : PPDecode
    Description     : set up and run pp
    Return type     : void
    Argument        : pp instance
------------------------------------------------------------------------------*/
enum DecRet PPDecode(PPInst post_pinst) {
  PPContainer *pp_c;
  i32 ret = 0;
  u32 reserve_ret;
  struct DWLReqInfo info = {0};

  av_unused u32 lu_ts_size = 0, ch_ts_size = 0;

  const struct DecHwFeatures *hw_feature = NULL;

  pp_c = (PPContainer *) post_pinst;
  if(pp_c == NULL)
    return(DEC_PARAM_ERROR);

  hw_feature = SwGetHwFeature(pp_c->dwl, CORE_MASK(pp_c->core_mask), DWL_CLIENT_TYPE_ST_PP);
  /* Set PP-standalone mode related register */
  if (IS_PP_IN_TILED4x4(pp_c->in_format)) {
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_stride >> 3);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride >> 3);
#ifdef PPU_V9_2_3
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_CHROMA_FORMAT_U, pp_c->in_chroma_format_idc);
  } else if (pp_c->in_tiled_mode == IN_TILED8x8) {
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_stride >> 3);
    /* for tile8x8 ouput format, chroma format is tile4x4 */
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride_ch >> 3);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_CHROMA_FORMAT_U, pp_c->in_chroma_format_idc);

    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_TILED_MODE_U, pp_c->in_tiled_mode);
#endif
  } else if (IS_PP_IN_RGB(pp_c->in_format)) {
    /* rgb2yuv regs config */
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFA1_U, coeff[pp_c->ppu_cfg->rgb_stan][0]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFA2_U, coeff[pp_c->ppu_cfg->rgb_stan][1]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFB_U, coeff[pp_c->ppu_cfg->rgb_stan][2]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFC_U, coeff[pp_c->ppu_cfg->rgb_stan][3]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFD_U, coeff[pp_c->ppu_cfg->rgb_stan][4]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFE_U, coeff[pp_c->ppu_cfg->rgb_stan][5]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFG_U, coeff[pp_c->ppu_cfg->rgb_stan][6]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFH_U, coeff[pp_c->ppu_cfg->rgb_stan][7]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_COLOR_COEFFI_U, coeff[pp_c->ppu_cfg->rgb_stan][8]);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_VIDEO_RANGE_U, pp_c->ppu_cfg->video_range);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_RGB_RANGE_MAX_U, pp_c->ppu_cfg->range_max);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_RGB_RANGE_MIN_U, pp_c->ppu_cfg->range_min);
    /* stride regs config */
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_stride);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride);
#ifdef PPU_V9_2_3
    if (pp_c->in_tiled_mode == IN_TILED64x64) {
      SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_stride >> 6);
      SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride >> 6);
      SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_TILED_MODE_U, pp_c->in_tiled_mode);
    }
    if (pp_c->in_dec400) {
      /* for dec400d rgb input need set chroma format */
      SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_CHROMA_FORMAT_U, 3);
    }
#endif
  } else if (IS_PP_IN_RFC(pp_c->in_format)) {
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_luma_stride >> 3);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_chroma_stride >> 3);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_PIC_WIDTH_U, pp_c->in_width);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_LUBITDEPTH_MINUS_8_U, pp_c->in_luma_bitdepth - 8);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_CHBITDEPTH_MINUS_8_U, pp_c->in_chroma_bitdepth - 8);
    SetDecRegister(pp_c->pp_regs,HWIF_PP_IN_CHROMA_FORMAT_U, pp_c->in_chroma_format_idc);
  } else {
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_Y_STRIDE, pp_c->in_stride);
    if (IS_PP_IN_YUV_PLANAR(pp_c->in_format)) /*TODO: modify in_stride*/
      SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride_ch);
    else
      SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_C_STRIDE, pp_c->in_stride_ch);
    // SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_CHROMA_FORMAT, pp_c->chroma_format);
    SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_CHROMA_FORMAT_U, pp_c->in_chroma_format_idc);
  }

#ifdef PPU_v9_2_3
  u8 sub_y;
  if (IS_PP_IN_YUV420(pp_c->in_format)) {
    //sub_x = 2;
    sub_y = 2;
  } else if (IS_PP_IN_YUV422(pp_c->in_format)) {
    //sub_x = 2;
    sub_y = 1;
  } else {
    //sub_x = 1;
    sub_y = 1;
  }
  SetDecRegister(pp_c->pp_regs, HWIF_PP_DEC400D_EN_U, pp_c->in_dec400);
  if (pp_c->in_dec400) {
    u32 luma_size = 0, chroma_size = 0;
    if (pp_c->in_dec400_a == DEC_ALIGN_64B)
      SetDecRegister(pp_c->pp_regs, HWIF_PP_DEC400D_ALIGN_U, 1);
    else
      SetDecRegister(pp_c->pp_regs, HWIF_PP_DEC400D_ALIGN_U, 0);
    if (pp_c->in_tiled_mode == IN_TILED64x64) {
      luma_size = pp_c->in_stride * NEXT_MULTIPLE(pp_c->in_height, 64) / 64;
      chroma_size = 0;
    } else if (pp_c->in_tiled_mode == IN_TILED8x8) {
      luma_size = pp_c->in_stride * NEXT_MULTIPLE(pp_c->in_height, 8) / 8;
      chroma_size = pp_c->in_stride_ch * NEXT_MULTIPLE((pp_c->in_height + 1)/ 2, 4) / 4;
    }
    /* dec400 luma in */
    lu_ts_size = NEXT_MULTIPLE(NEXT_MULTIPLE((NEXT_MULTIPLE(luma_size, 256) / 256 * 4 + 7) / 8, 16) + DEC400_IN_HEADER_SIZE, 256);
    if(chroma_size)
      ch_ts_size = NEXT_MULTIPLE(NEXT_MULTIPLE((NEXT_MULTIPLE(chroma_size, 256) / 256 * 4 + 7) / 8, 16) + DEC400_IN_HEADER_SIZE, 256);
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + lu_ts_size);
    /* dec400 chroma in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CH_BASE_U_LSB,
                HWIF_PP_IN_CH_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + lu_ts_size + ch_ts_size + luma_size);
    /* dec400 luma ts table in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LUT_BASE_U_LSB,
                HWIF_PP_IN_LUT_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + luma_size + chroma_size);
    /* dec400 chroma ts table in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CHT_BASE_U_LSB,
                HWIF_PP_IN_CHT_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + lu_ts_size + luma_size);
  } else if (IS_PP_IN_RGB_PLANAR(pp_c->in_format)) {
    if (pp_c->in_format == PP_IN_RGB888_P || pp_c->in_format == PP_IN_R16G16B16_P) {
      SET_ADDR_REG2(pp_c->pp_regs,
                  HWIF_PP_IN_R_BASE_U_LSB,
                  HWIF_PP_IN_R_BASE_U_MSB,
                  pp_c->pp_in_buffer.bus_address);
      SET_ADDR_REG2(pp_c->pp_regs,
                  HWIF_PP_IN_G_BASE_U_LSB,
                  HWIF_PP_IN_G_BASE_U_MSB,
                  pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height);
      SET_ADDR_REG2(pp_c->pp_regs,
                  HWIF_PP_IN_B_BASE_U_LSB,
                  HWIF_PP_IN_B_BASE_U_MSB,
                  pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height * 2);
    } else if (pp_c->in_format == PP_IN_BGR888_P || pp_c->in_format == PP_IN_B16G16R16_P) {
      SET_ADDR_REG2(pp_c->pp_regs,
                  HWIF_PP_IN_B_BASE_U_LSB,
                  HWIF_PP_IN_B_BASE_U_MSB,
                  pp_c->pp_in_buffer.bus_address);
      SET_ADDR_REG2(pp_c->pp_regs,
                  HWIF_PP_IN_G_BASE_U_LSB,
                  HWIF_PP_IN_G_BASE_U_MSB,
                  pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height);
      SET_ADDR_REG2(pp_c->pp_regs,
                  HWIF_PP_IN_R_BASE_U_LSB,
                  HWIF_PP_IN_R_BASE_U_MSB,
                  pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height * 2);
    }
  } else if (IS_PP_IN_RGB(pp_c->in_format)) {
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address);
  } else if (IS_PP_IN_422PACKED(pp_c->in_format)){
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address);
  } else if (IS_PP_IN_YUV_PLANAR(pp_c->in_format)) {/*support 420P, TODO: support 422P*/
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address);
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CH_BASE_U_LSB,
                HWIF_PP_IN_CH_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height);
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_B_BASE_U_LSB,
                HWIF_PP_IN_B_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height + pp_c->in_stride_ch * (sub_y == 2 ? (pp_c->in_height + 1) / 2 : pp_c->in_height));
  } else if (IS_PP_IN_RFC(pp_c->in_format)) {
#else
  if (IS_PP_IN_RFC(pp_c->in_format)) {
#endif
    /* rfc luma in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address);
    /* rfc chroma in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CH_BASE_U_LSB,
                HWIF_PP_IN_CH_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_luma_stride * pp_c->in_height / 8);
    /* rfc luma table in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LUT_BASE_U_LSB,
                HWIF_PP_IN_LUT_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_luma_stride * pp_c->in_height / 8+ pp_c->in_chroma_stride * pp_c->in_height / 2 / 4);
    /* rfc chroma table in */
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CHT_BASE_U_LSB,
                HWIF_PP_IN_CHT_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_luma_stride * pp_c->in_height / 8 + pp_c->in_chroma_stride * pp_c->in_height / 2 / 4
                + pp_c->in_luma_table_stride * pp_c->in_height / 8);
  } else { /*TODO support 400 input*/
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_LU_BASE_U_LSB,
                HWIF_PP_IN_LU_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address);
    SET_ADDR_REG2(pp_c->pp_regs,
                HWIF_PP_IN_CH_BASE_U_LSB,
                HWIF_PP_IN_CH_BASE_U_MSB,
                pp_c->pp_in_buffer.bus_address + pp_c->in_stride * pp_c->in_height / ((pp_c->in_format == PP_IN_TILED4x4 || pp_c->in_format == PP_IN_TILED4x4_P010) ? 4 : 1));
  }


  SetDecRegister(pp_c->pp_regs, HWIF_UNIQUE_ID, UNIQUE_ID(0));

  for (int i = 0; i < DEC_MAX_PPU_COUNT; i++)
    pp_c->ppu_cfg[i].pp_in_format = pp_c->in_format;

  struct PpParams pp_args = {hw_feature,
                             pp_c->ppu_cfg,
                             &pp_c->pp_out_buffer,
                             0,
                             0,
                             0
                            };
  PPSetRegs(pp_c->pp_regs, &pp_args);
  SetDecRegister(pp_c->pp_regs, HWIF_PP_IN_FORMAT_U, pp_c->in_format); /* override default value */

  // DWLDMATransData(pp_c->dwl, &pp_c->pp_in_buffer, 0, pp_c->pp_in_buffer.size, HOST_TO_DEVICE);
  DWLDMATransData2(pp_c->dwl, (addr_t)pp_c->pp_in_buffer.bus_address,
                  (void *)pp_c->pp_in_buffer.virtual_address,
                  pp_c->pp_in_buffer.size, HOST_TO_DEVICE);

  if (pp_c->enable_3dlut)
    DWLDMATransData(pp_c->dwl, &pp_c->ppu_cfg[0].table_3dlut_buffer,0,
                    pp_c->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);

  info.core_mask = pp_c->core_mask;
  info.width = pp_c->in_stride;
  info.height = pp_c->in_height;
  info.owner = (void *)pp_c;
  if (pp_c->vcmd_used)
    reserve_ret = DWLReserveCmdBuf(pp_c->dwl, &info, &pp_c->cmdbuf_id);
  else
    reserve_ret = DWLReserveHw(pp_c->dwl, &info, &pp_c->core_id);

  if (reserve_ret != DWL_OK)
    return DEC_HW_RESERVED;

  PPFlushRegs(pp_c);

  if (pp_c->vcmd_used)
    DWLReadPpConfigure(pp_c->dwl, pp_c->cmdbuf_id, pp_c->ppu_cfg, 0);
  else
    DWLReadPpConfigure(pp_c->dwl, pp_c->core_id, pp_c->ppu_cfg, 0);
  SetDecRegister(pp_c->pp_regs, HWIF_DEC_E, 1);
  if (pp_c->vcmd_used) {
    DWLWriteReg(pp_c->dwl, pp_c->core_id, 4 * 1, pp_c->pp_regs[1]);
    DWLEnableCmdBuf(pp_c->dwl, pp_c->cmdbuf_id);
  } else
    DWLEnableHw(pp_c->dwl, pp_c->core_id, 4 * 1,
                pp_c->pp_regs[1]);

  if (pp_c->vcmd_used)
    ret = DWLWaitCmdBufReady(pp_c->dwl, pp_c->cmdbuf_id);
  else
    ret = DWLWaitHwReady(pp_c->dwl, pp_c->core_id, (u32) (-1));

  if(ret != DWL_HW_WAIT_OK) {
    /* Reset HW */
    SetDecRegister(pp_c->pp_regs, HWIF_DEC_E, 0);
    SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ, 0);

    if (pp_c->vcmd_used)
      DWLReleaseCmdBuf(pp_c->dwl, pp_c->cmdbuf_id);
    else {
      DWLDisableHw(pp_c->dwl, pp_c->core_id, 4 * 1,
                   pp_c->pp_regs[1]);
      DWLReleaseHw(pp_c->dwl, pp_c->core_id);
    }
    return DEC_HW_TIMEOUT;
  }

  PPRefreshRegs(pp_c);

  SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(pp_c->pp_regs, HWIF_DEC_IRQ, 0); /* just in case */
  if (pp_c->vcmd_used)
    DWLReleaseCmdBuf(pp_c->dwl, pp_c->cmdbuf_id);
  else {
    DWLDisableHw(pp_c->dwl, pp_c->core_id, 4 * 1,
                 pp_c->pp_regs[1]);
    DWLReleaseHw(pp_c->dwl, pp_c->core_id);
  }
  return DEC_OK;
}

enum DecRet PPNextPicture(PPInst post_pinst, PPDecPicture *output) {
  PPContainer *pp_c;
  u32 i, is_10bit;
  PpUnitIntConfig *ppu_cfg;
  const u32 *tile_status_virtual_address = NULL;
  addr_t tile_status_bus_address = 0;

  pp_c = (PPContainer *) post_pinst;
  (void) DWLmemset(output, 0, sizeof(PPDecPicture));
  ppu_cfg = pp_c->ppu_cfg;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled) continue;
    output->pictures[i].output_picture = (u32*)((addr_t)pp_c->pp_out_buffer.virtual_address + ppu_cfg->luma_offset);
    output->pictures[i].output_picture_bus_address = pp_c->pp_out_buffer.bus_address + ppu_cfg->luma_offset;
    if (ppu_cfg->monochrome) {
      output->pictures[i].output_picture_chroma = NULL;
      output->pictures[i].output_picture_chroma_bus_address = 0;
    } else {
      output->pictures[i].output_picture_chroma = (u32*)((addr_t)pp_c->pp_out_buffer.virtual_address + ppu_cfg->chroma_offset);
      output->pictures[i].output_picture_chroma_bus_address = pp_c->pp_out_buffer.bus_address + ppu_cfg->chroma_offset;
    }
    output->pictures[i].output_format = TransUnitConfig2Format(ppu_cfg);
    if (ppu_cfg->crop2.enabled) {
      output->pictures[i].pic_width = ppu_cfg->crop2.width;
      output->pictures[i].pic_height = ppu_cfg->crop2.height;
    } else {
      output->pictures[i].pic_width = ppu_cfg->frm_width;
      output->pictures[i].pic_height = ppu_cfg->frm_height;
    }
    output->pictures[i].pic_stride = ppu_cfg->ystride;
    output->pictures[i].pic_stride_ch = ppu_cfg->cstride;

    is_10bit = IS_PP_IN_10BIT(pp_c->in_format) && (ppu_cfg->out_cut_8bits != 1);
    output->pictures[i].bit_depth_luma = is_10bit ? 10 : 8;
    output->pictures[i].bit_depth_chroma = is_10bit ? 10 : 8;
  }
  output->cycles_per_mb = PPCycleCount(pp_c, output);
  /* dec400_table_buffer is put into the end region of all pp buffer */
  ppu_cfg = pp_c->ppu_cfg;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled) continue;
    if (ppu_cfg->luma_size) {
      if(tile_status_bus_address == 0) {
        tile_status_virtual_address = (u32 *)pp_c->pp_out_buffer.virtual_address;
        tile_status_bus_address = pp_c->pp_out_buffer.bus_address;
      }
    }
  }
  ppu_cfg = pp_c->ppu_cfg;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled || !ppu_cfg->dec400_enabled) continue;
    PpFillDec400TblInfo(ppu_cfg,
                          tile_status_virtual_address,
                          tile_status_bus_address,
                          &output->pictures[i].dec400_luma_table,
                          &output->pictures[i].dec400_chroma_table);
  }
  return DEC_OK;
}

u32 PPCycleCount(PPContainer *pp_c, PPDecPicture *output) {
  u32 cycles = 0;
  u32 mbs = PPPicWidth(pp_c) * PPPicHeight(pp_c) >> 8;
  if (mbs)
    cycles = GetDecRegister(pp_c->pp_regs, HWIF_PERF_CYCLE_COUNT) / mbs;
#ifdef FPGA_PERF_AND_BW
  u64 bwrd_per_mb, bwwr_per_mb;
  u32 pic_size = PPPicWidth(pp_c) * PPPicHeight(pp_c);
  bwrd_per_mb = DWLReadBw(pp_c->dwl, pp_c->core_id, 0);// 0 is read bandwidth
  bwwr_per_mb = DWLReadBw(pp_c->dwl, pp_c->core_id, 1);// 1 is write bandwidth
  if (output->pictures[0].bit_depth_luma == 10){
    bwrd_per_mb *= 0.8;
    bwwr_per_mb *= 0.8;
  }
  output->bwrd_per_mb = bwrd_per_mb * 16 * 1000/ pic_size;
  output->bwwr_per_mb = bwwr_per_mb * 16 * 1000/ pic_size;
#endif
  return cycles;
}
