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
--------------------------------------------------------------------------------
--
--  Abstract : VC Encoder DEC400 Interface Implementation
--
------------------------------------------------------------------------------*/

#include "vsi_string.h"
#include "encdec400.h"
#include "osal.h"
#include "enc_log.h"
#include "enccommon.h"
#include "encpreprocess.h"

#ifdef SUPPORT_DEC400

/** \brief [0,7] Whether to enable DEC400. */
  /** \brief [8,15] DEC400 tileSize. */
  /** \brief [16,23] DEC400 tileMode. */
  /** \brief [25,27] DEC400 RGB format. */
  /** \brief [28,28] DEC400 RGB format X or A.(ARGB: 0 or XRGB: 1). */
  /** \brief [29,31] DEC400 data alignment. */
#define BIT_DEC400_MODE         (0)
#define BIT_DEC400_TILE_SZ      (8)
#define BIT_DEC400_TILE_MODE    (16)
#define BIT_DEC400_RGB_FORMAT   (25)
#define BIT_DEC400_RGB_XA       (28)
#define BIT_DEC400_ALIGN        (29)

#define MASK_DEC400_MODE        (0x000000FF)
#define MASK_DEC400_TILE_SZ     (0x0000FF00)
#define MASK_DEC400_TILE_MODE   (0x00FF0000)
#define MASK_DEC400_RGB_FORMAT  (0x0E000000)
#define MASK_DEC400_RGB_XA      (0x10000000)
#define MASK_DEC400_ALIGN       (0xE0000000)

#define DEC400_PARSE(val, NAME) (((val) & MASK_DEC400_##NAME) >> BIT_DEC400_##NAME)

#define PTRACE_E(fmt, ...) \
  VCEncTraceMsg(NULL, VCENC_LOG_ERROR, VCENC_LOG_TRACE_EWL, fmt, ##__VA_ARGS__)
#define FULL_ZERO (0x0)
#define FULL_FF (0xFFFFFFFF)
#define BYPASS_VALUE FULL_ZERO

static Dec400Feature dec400_feature_list[] = {
    {
        .hw_build_id = 0x00000525,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_32,
        .planar420_cbcr_table_style = 1,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000518,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_64,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_RESET,
        .hw_work_around = 0,
        .reg_version_index = 0,
    },
    {
        .hw_build_id = 0x00000520,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_64,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_RESET,
        .hw_work_around = 0,
        .reg_version_index = 0,
    },
    {
        .hw_build_id = 0x00000528,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_64,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 1,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000529,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_64,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000538,
        .tile_mode_idx = 1,
        .dec400_data_align = DEC400_COMP_ALIGN_32,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000534,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_32,
        .planar420_cbcr_table_style = 1,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000550,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_32,
        .planar420_cbcr_table_style = 1,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000555,
        .tile_mode_idx = 2,
        .dec400_data_align = DEC400_COMP_ALIGN_32,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000556,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_64,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000557,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_64,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    },
    {
        .hw_build_id = 0x00000563,
        .tile_mode_idx = 0,
        .dec400_data_align = DEC400_COMP_ALIGN_32,
        .planar420_cbcr_table_style = 0,
        .finish_mode = DEC400_FLUSH,
        .hw_work_around = 0,
        .reg_version_index = 1,
    }};
static Dec400Feature *hw_feature = NULL;

static u32 reg_offset[2][DEC400_REG_MAX] = {

    {0xC80, 0x800, 0x900, 0xF80, 0xE80, 0x1180, 0x980,
     0x1000, 0x880, 0xB00, 0xB70, 0x0, 0x0, 0x0, 0x1300},

    {0x900, 0x880, 0xA80, 0xB00, 0xB80, 0xC00, 0x1080,
     0x1100, 0x980, 0x800, 0x804, 0x814, 0x820, 0x80C, 0x1300}};
static u32 *hw_reg_offset = NULL;
pthread_mutex_t dec400_mutex = PTHREAD_MUTEX_INITIALIZER;
void VCEncDec400GetTileSize(Dec400TileInfo *info, u32 dec400Enable);

static void EncDec400GetPlaneSize(Dec400ReadContext *ctx,
                                  VCDec400StreamDsc *stream) {

  Dec400StreamReadCfg *cfg = &ctx->strm_cfg;
  u32 luma_stride = 0, chroma_stride = 0, height = 0;
  u32 lumaSize = 0, chromaSize = 0, pictureSize = 0;

  EncGetAlignedByteStride(stream->width, stream->pix_fmt,
                          &luma_stride, &chroma_stride, ctx->input_alignment, stream->super_tile);
  height = stream->height;
  cfg->chn_cfg[0].size = 0;
  cfg->chn_cfg[1].size = 0;
  cfg->chn_cfg[2].size = 0;
  switch (stream->pix_fmt) {
    case ENC_PIXFMT_YUV420_PLANAR:
      cfg->chn_cfg[0].size = luma_stride * height;
      cfg->chn_cfg[1].size = chroma_stride * height / 2;
      cfg->chn_cfg[2].size = chroma_stride * height / 2;
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR:
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU:
      cfg->chn_cfg[0].size = luma_stride * height;
      cfg->chn_cfg[1].size = chroma_stride * height / 2;
      break;
    case ENC_PIXFMT_YUV422_INTERLEAVED_YUYV:
    case ENC_PIXFMT_YUV422_INTERLEAVED_UYVY:
    case ENC_PIXFMT_RGB565:
    case ENC_PIXFMT_BGR565:
    case ENC_PIXFMT_RGB555:
    case ENC_PIXFMT_BGR555:
    case ENC_PIXFMT_RGB444:
    case ENC_PIXFMT_BGR444:
    case ENC_PIXFMT_RGB888:
    case ENC_PIXFMT_BGR888:
    case ENC_PIXFMT_RGB101010:
    case ENC_PIXFMT_BGR101010:
    case ENC_PIXFMT_RGBX8888:
    case ENC_PIXFMT_BGRX8888:
    case ENC_PIXFMT_RGBX1010102:
    case ENC_PIXFMT_BGRX1010102:
      cfg->chn_cfg[0].size = luma_stride * height;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_I010:
      cfg->chn_cfg[0].size = luma_stride * height;
      cfg->chn_cfg[1].size = chroma_stride * height / 2 * 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010:
      cfg->chn_cfg[0].size = luma_stride * height;
      cfg->chn_cfg[1].size = chroma_stride * height / 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_PACKED_PLANAR:
      cfg->chn_cfg[0].size = luma_stride * 10 / 8 * height;
      cfg->chn_cfg[1].size = chroma_stride * 10 / 8 * height / 2 * 2;
      break;
    case ENC_PIXFMT_YUV420_10BIT_PACKED_Y0L2:
      cfg->chn_cfg[0].size = luma_stride * 2 * 2 * height / 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_8BIT_TILE_32_32:
      cfg->chn_cfg[0].size = luma_stride * ((height + 32 - 1) & (~(32 - 1)));
      cfg->chn_cfg[1].size = cfg->chn_cfg[0].size / 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4:
      cfg->chn_cfg[0].size = luma_stride * height * 2 * 12 / 8;
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
      cfg->chn_cfg[0].size = luma_stride * ((height + 3) / 4);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010_TILE_4_4:
      cfg->chn_cfg[0].size = luma_stride * ((height + 3) / 4);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR_101010:
      cfg->chn_cfg[0].size = luma_stride * height;
      cfg->chn_cfg[1].size = chroma_stride * height / 2;
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_64_4:
    case ENC_PIXFMT_YUV420_UV_8BIT_TILE_64_4:
      cfg->chn_cfg[0].size = luma_stride * ((height + 3) / 4);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_32_4:
      cfg->chn_cfg[0].size = luma_stride * ((height + 3) / 4);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_48_4:
    case ENC_PIXFMT_YUV420_VU_10BIT_TILE_48_4:
      cfg->chn_cfg[0].size = luma_stride * ((height + 3) / 4);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_128_2:
    case ENC_PIXFMT_YUV420_UV_8BIT_TILE_128_2:
      cfg->chn_cfg[0].size = luma_stride * ((height + 1) / 2);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 1) / 2);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_96_2:
    case ENC_PIXFMT_YUV420_VU_10BIT_TILE_96_2:
      cfg->chn_cfg[0].size = luma_stride * ((height + 1) / 2);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 1) / 2);
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_8_8:
      cfg->chn_cfg[0].size = luma_stride * ((height + 7) / 8);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_8_8:
      cfg->chn_cfg[0].size = luma_stride * ((height + 7) / 8);
      cfg->chn_cfg[1].size = chroma_stride * (((height / 2) + 3) / 4);
      break;
    case ENC_PIXFMT_Y8b_TILE_8_8:
    case ENC_PIXFMT_Y10bWH_TILE_8_8:
      cfg->chn_cfg[0].size = luma_stride * ((height + 7) / 8);
	  break;
    default:
      PTRACE_E("DEC400 not support this format\n");
      break;
  }
}
#if 0
void ChangeYUVbyFC(u8* tile_status, u8 *data, u32 value, u32 size, u32 tileSize) {
  u32 ts_val = 0;
  for (int i=0; i < size; i++) {
    ts_val= *(tile_status + i) & 0xf;
    if (ts_val == 1) {
      for (int j=i*tileSize / 2; j < i*tileSize / 2 + tileSize / 4; j++) {
        *((u32 *)data + j) = value;
      }
    }
    ts_val= (*(tile_status + i) & 0xf0) >> 4;
    if (ts_val == 1) {
      for (int j=i*tileSize / 2 + tileSize / 4; j < (i + 1) *(tileSize / 2); j++) {
        *((u32 *)data + j) = value;
      }
    }
  }
}

void Fastclear(u8 *dataBuf, u8 *compTblBuf, u32 inputFormat, u32 clearColorLow[3], u32 dec400Enable, u32 lumSize, u32 chrSize, u32 lumTblSize, u32 chrTblSize) {
  Dec400TileInfo tile_info;
  u8 *tile_status_lum = compTblBuf + DEC400_HEADER_BUF_SIZE;
  u8 *tile_status_cb = compTblBuf + lumTblSize;
  u8 *tile_status_cr = tile_status_cb + chrTblSize / 2;
  u8 *lum = dataBuf;
  u8 *cb = dataBuf + lumSize;
  u8 *cr = cb + chrSize / 2;


  VCEncDec400GetTileSize(&tile_info, dec400Enable);

  ChangeYUVbyFC(tile_status_lum, lum, clearColorLow[0], lumTblSize - DEC400_HEADER_BUF_SIZE, tile_info.tile_size);

  if (chrTblSize != 0) {
    if (inputFormat == VCENC_YUV420_PLANAR) {
      ChangeYUVbyFC(tile_status_cb, cb, clearColorLow[1], chrTblSize / 2 - DEC400_HEADER_BUF_SIZE, tile_info.tile_size);
      ChangeYUVbyFC(tile_status_cr, cr, clearColorLow[2], chrTblSize / 2 - DEC400_HEADER_BUF_SIZE, tile_info.tile_size);
    } else {
      ChangeYUVbyFC(tile_status_cb, cb, clearColorLow[1], chrTblSize - DEC400_HEADER_BUF_SIZE, tile_info.tile_size);
    }
  }
}
#endif
void EncParseDEC400HeaderData(u32 *clearColorVal, u32 *fcEnable, u8 *hdrData)
{
  *clearColorVal =  (u32)(*(hdrData + 0)) |
                    (u32)(*(hdrData + 1)) << 8 |
                    (u32)(*(hdrData + 2)) << 16 |
                    (u32)(*(hdrData + 3)) << 24;
  *fcEnable = (u32)(*(hdrData + 8));
}

static void DEC400WriteReg(Dec400ReadContext *ctx, u32 reg_off, u32 val) {
  if (ctx->vcmd) {
    VcmdbufCollectWriteDec400RegData(ctx->ewl, ctx->vcmd, &val, reg_off/4, 1);
  } else {
    EWLWriteRegbyClientType(ctx->ewl, reg_off, val, EWL_CLIENT_TYPE_DEC400);
  }
}

static void DEC400WriteRegBack(Dec400ReadContext *ctx, u32 reg_off, u32 val) {
  if (ctx->vcmd) {
    VcmdbufCollectWriteDec400RegData(ctx->ewl, ctx->vcmd, &val, reg_off/4, 1);
  } else {
    EWLWriteRegbyClientType(ctx->ewl, reg_off, val, EWL_CLIENT_TYPE_DEC400);
  }
}

static u32 DEC400ReadReg(const void *ewl, u32 offset) {
  return EWLReadRegbyClientType(ewl, offset, EWL_CLIENT_TYPE_DEC400);
}

void VCEncDec400GetTileSize(Dec400TileInfo *info, u32 dec400Enable) {
  u32 tile_size = 0;
  u32 id = DEC400_PARSE(dec400Enable, TILE_SZ);

  info->bits_tile_in_table = 4;

  ASSERT(hw_feature);

  switch (id) {
    case 1: info->tile_size = 128; break;
    case 2: info->tile_size = 512; break;
    case 3: info->tile_size = 256; break;
    default:
      if (hw_feature->tile_mode_idx == 1)
        info->tile_size = 128;
      else
        info->tile_size = 256;
      break;
  }
}

u32 _getCompressionAlignMode(u32 dec400Enable) {
  u32 align_mode = DEC400_COMP_ALIGN_32;
  u32 id = DEC400_PARSE(dec400Enable, ALIGN);

  switch (id) {
    case 1: align_mode = DEC400_COMP_ALIGN_32; break;
    case 2: align_mode = DEC400_COMP_ALIGN_64; break;
    case 3: align_mode = DEC400_COMP_ALIGN_1; break;
    case 4: align_mode = DEC400_COMP_ALIGN_16; break;
    default: align_mode = hw_feature->dec400_data_align; break;
  }
  return align_mode;
}

static u32 _convert_rgb_format(u32 format, u32 dec400Enable) {
  u32 b_XRGB, rgb_format_idx, rgb_format = 0;
  b_XRGB = DEC400_PARSE(dec400Enable, RGB_XA);
  rgb_format_idx = DEC400_PARSE(dec400Enable, RGB_FORMAT);
  if (b_XRGB) {
    switch (rgb_format_idx) {
      case  1: rgb_format = DEC400_COMP_FMT_XRGB8; break;
      case  2: rgb_format = DEC400_COMP_FMT_A2RGB10; break;
      case  3: rgb_format = DEC400_COMP_FMT_XRGB4; break;
      case  4: rgb_format = DEC400_COMP_FMT_X1RGB5; break;
      case  5: rgb_format = DEC400_COMP_FMT_R5G6B5; break;
      default: rgb_format = format; break;
    }
  } else {
    switch (rgb_format_idx) {
      case  1: rgb_format = DEC400_COMP_FMT_ARGB8; break;
      case  2: rgb_format = DEC400_COMP_FMT_A2RGB10; break;
      case  3: rgb_format = DEC400_COMP_FMT_ARGB4; break;
      case  4: rgb_format = DEC400_COMP_FMT_A1RGB5; break;
      case  5: rgb_format = DEC400_COMP_FMT_R5G6B5; break;
      default:
        if (format == DEC400_COMP_FMT_XRGB8) {
          rgb_format = DEC400_COMP_FMT_ARGB8;
        } else if (format == DEC400_COMP_FMT_XRGB4) {
          rgb_format = DEC400_COMP_FMT_ARGB4;
        } else if (format == DEC400_COMP_FMT_X1RGB5) {
          rgb_format = DEC400_COMP_FMT_A1RGB5;
        } else {
          rgb_format = format;
        }
        break;
    }
  }
  return rgb_format;
}

static u32 _convert_raster_tm(u32 mode, u32 superTile, u32 dec400Enable) {
  u32 user_mode_idx, tile_mode = mode;
  user_mode_idx = DEC400_PARSE(dec400Enable, TILE_MODE);

  switch (user_mode_idx) {
    case  1: tile_mode = DEC400_TM_TILE_8x8_x; break;
    case  2: tile_mode = DEC400_TM_TILE_8x8_y; break;
    case  3: tile_mode = DEC400_TM_TILE_16x4; break;
    case  4: tile_mode = DEC400_TM_TILE_8x4; break;
    case  5: tile_mode = DEC400_TM_TILE_4x8; break;
    case  6: tile_mode = DEC400_TM_RASTER_16x4; break;
    case  7: tile_mode = DEC400_TM_TILE_64x4; break;
    case  8: tile_mode = DEC400_TM_TILE_32x4; break;
    case  9: tile_mode = DEC400_TM_RASTER_256x1; break;
    case 10: tile_mode = DEC400_TM_RASTER_128x1; break;
    case 11: tile_mode = DEC400_TM_RASTER_64x1; break;
    case 12: tile_mode = DEC400_TM_TILE_16x8; break;
    case 13: tile_mode = DEC400_TM_RASTER_32x4; break;
    case 14: tile_mode = DEC400_TM_RASTER_32x1; break;
    case 15: tile_mode = DEC400_TM_RASTER_16x1; break;
    case 16: tile_mode = DEC400_TM_TILE_8x4_S; break;
    case 17: tile_mode = DEC400_TM_TILE_16x4_S; break;
    case 18: tile_mode = DEC400_TM_TILE_32x4_S; break;
    case 19: tile_mode = DEC400_TM_TILE_32x8; break;
    default:
      if (mode == DEC400_TM_RASTER_256x1) {
        if (hw_feature->tile_mode_idx == 1)
          tile_mode = DEC400_TM_RASTER_128x1;
        if (hw_feature->tile_mode_idx == 2)
          tile_mode = DEC400_TM_TILE_64x4;
      } else if (mode == DEC400_TM_RASTER_128x1) {
        if (superTile == 1) {
          tile_mode = DEC400_TM_TILE_16x8;
        }
        if (hw_feature->tile_mode_idx == 1) {
          tile_mode = DEC400_TM_RASTER_64x1;
          if (superTile == 1)
            tile_mode = DEC400_TM_TILE_8x8_x;
        }
      } else if (mode == DEC400_TM_RASTER_64x1) {
        if (superTile == 1) {
          tile_mode = DEC400_TM_TILE_8x8_x;
        }
        if (hw_feature->tile_mode_idx == 1) {
          tile_mode = DEC400_TM_RASTER_32x1;
          if (superTile == 1)
            tile_mode = DEC400_TM_TILE_8x4;
          if (superTile == 2)
            tile_mode = DEC400_TM_TILE_4x8;
        }
      }
      break;
  }
  return tile_mode;
}

static void Dec400ParseStreamCfg(Dec400ReadContext *ctx,
                                VCDec400StreamDsc *stream) {
  Dec400StreamReadCfg *cfg = &ctx->strm_cfg;
  cfg->compress_align_mode = ctx->compress_align_mode;
  cfg->planes = 0;
  u32 superTile = stream->super_tile;
  //if the stream is osd, need to convert the format.
  if (stream->is_osd) {
    if (stream->pix_fmt == 0) {
      stream->pix_fmt = ENC_PIXFMT_RGB888;
    }
  }
  cfg->pix_fmt = stream->pix_fmt;

  //set the fast clear value.
  if (stream->fastclearEnable[0])
    cfg->chn_cfg[0].fastclear_val = stream->fastclear_val[0];
  if (stream->fastclearEnable[1])
    cfg->chn_cfg[1].fastclear_val = stream->fastclear_val[1];
  if (stream->fastclearEnable[2])
    cfg->chn_cfg[2].fastclear_val = stream->fastclear_val[2];

  EncDec400GetPlaneSize(ctx, stream);
  //if stream is dec400 bypass, need to get the plane's num and size to config the buffer base registers.
  if (DEC400_PARSE(stream->dec400Enable, MODE) == 1) {
    for (u32 i = 0; i < 3; i++) {
      if (stream->data_base[i] != 0){
        cfg->planes++;
      }
    }
    return;
  }

  switch (cfg->pix_fmt) {
    case ENC_PIXFMT_YUV420_PLANAR:
        cfg->planes = 3;
        cfg->bit_depth = DEC400_BIT_DEPTH_8;
        cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
        cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_256x1, superTile, stream->dec400Enable);
        cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
        cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_256x1, superTile, stream->dec400Enable);
        cfg->chn_cfg[2].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
        cfg->chn_cfg[2].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_256x1, superTile, stream->dec400Enable);
        break;

    case ENC_PIXFMT_YUV420_SEMIPLANAR:
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU:
        cfg->planes = 2;
        cfg->bit_depth = DEC400_BIT_DEPTH_8;
        cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
        cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_256x1, superTile, stream->dec400Enable);
        cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
        cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_128x1, superTile, stream->dec400Enable);
        break;

    case ENC_PIXFMT_YUV420_PLANAR_10BIT_I010:
        cfg->planes = 2;
        cfg->bit_depth = DEC400_BIT_DEPTH_10;
        cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
        cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_128x1, superTile, stream->dec400Enable);
        cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
        cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_64x1, superTile, stream->dec400Enable);
        break;

    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010:
        cfg->planes = 2;
        cfg->bit_depth = DEC400_BIT_DEPTH_10;
        cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
        cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_128x1, superTile, stream->dec400Enable);
        cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
        cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_64x1, superTile, stream->dec400Enable);
        break;

    case ENC_PIXFMT_RGB888:
    case ENC_PIXFMT_BGR888:
    case ENC_PIXFMT_RGBX8888:
    case ENC_PIXFMT_BGRX8888:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = _convert_rgb_format(DEC400_COMP_FMT_XRGB8, stream->dec400Enable);
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_64x1, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_RGB101010:
    case ENC_PIXFMT_BGR101010:
    case ENC_PIXFMT_RGBX1010102:
    case ENC_PIXFMT_BGRX1010102:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = _convert_rgb_format(DEC400_COMP_FMT_A2RGB10, stream->dec400Enable);
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_64x1, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
      cfg->planes = 2;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_TILE_64x4, superTile, stream->dec400Enable);
      cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
      cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_TILE_32x4, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010_TILE_4_4:
      cfg->planes = 2;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_TILE_32x4, superTile, stream->dec400Enable);
      cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
      cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_TILE_16x4, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_RGB444:
    case ENC_PIXFMT_BGR444:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = _convert_rgb_format(DEC400_COMP_FMT_XRGB4, stream->dec400Enable);
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_128x1, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_RGB555:
    case ENC_PIXFMT_BGR555:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = _convert_rgb_format(DEC400_COMP_FMT_X1RGB5, stream->dec400Enable);
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_128x1, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_RGB565:
    case ENC_PIXFMT_BGR565:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = _convert_rgb_format(DEC400_COMP_FMT_R5G6B5, stream->dec400Enable);
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_RASTER_128x1, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_Y8b_TILE_8_8:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_TILE_32x4, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_Y10bWH_TILE_8_8:
      cfg->planes = 1;
      cfg->bit_depth = DEC400_BIT_DEPTH_10;
      cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_TILE_16x4, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_8_8:
      cfg->planes = 2;
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_TILE_32x8, superTile, stream->dec400Enable);
      cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
      cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_TILE_32x4, superTile, stream->dec400Enable);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_8_8:
      cfg->planes = 2;
      cfg->bit_depth = DEC400_BIT_DEPTH_10;
      cfg->chn_cfg[0].compress_fmt = DEC400_COMP_FMT_YUV_ONLY;
      cfg->chn_cfg[0].tile_mode = _convert_raster_tm(DEC400_TM_TILE_16x8, superTile, stream->dec400Enable);
      cfg->chn_cfg[1].compress_fmt = DEC400_COMP_FMT_UV_MIX;
      cfg->chn_cfg[1].tile_mode = _convert_raster_tm(DEC400_TM_TILE_16x4, superTile, stream->dec400Enable);
      break;
    default:
      cfg->bit_depth = DEC400_BIT_DEPTH_8;
      APITRACEERR("Dec400ParseStreamCfg: DEC400 not support this format\n");
      break;
  }
}

static void _dec400_set_read_config(Dec400ReadContext *ctx, u32 chn, u32 val) {
  u32 reg_off = hw_reg_offset[gcregAHBDECReadConfig];

  if (hw_feature->hw_work_around == 1) {
    reg_off = hw_reg_offset[gcregAHBDECWriteConfig];
  }

  reg_off += chn * 4;
  DEC400WriteReg(ctx, reg_off, val);
}

static void Dec400SetStreamCfg(Dec400ReadContext *ctx,
                                VCDec400StreamDsc *stream) {
  Dec400StreamReadCfg *cfg = &ctx->strm_cfg;
  u32 i, ch;

  for (i = 0; i< cfg->planes; i++) {
    ch = ctx->occupied_num + i;
    //set read config
    if (DEC400_PARSE(stream->dec400Enable, MODE) == 1) {
      _dec400_set_read_config(ctx, ch, 0x0);
    } else {
      _dec400_set_read_config(ctx, ch,
                            cfg->compress_align_mode |
                            cfg->chn_cfg[i].compress_fmt |
                            cfg->chn_cfg[i].tile_mode |
                            DEC400_COMP_ENABLE);
    }

    //set fast clear value
    if (stream->fastclearEnable[i]) {
#ifdef DEC400_FAST_CLEAR_ANALYSIS_BY_HW
      if (ctx->vcmd)
        VcmdbufCollectWriteDec400FCRegData(ctx->ewl, ctx->vcmd,
                                          (hw_reg_offset[gcregAHBDECFastClearValue] + ch * 4) / 4,
                                          stream->table_base[i] - DEC400_HEADER_BUF_SIZE, 1);
#else
      DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECFastClearValue] + ch * 4, cfg->chn_cfg[i].fastclear_val);
#endif
    }

    //set read config extra
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadConfigEx] + ch * 4, cfg->bit_depth);

    //set read buffer base
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadBufferBase] + ch * 4, (u32)stream->data_base[i]);
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadBufferBaseEx] + ch * 4, stream->data_base[i] >> 32);

    //set read buffer end
    ptr_t addr_end = stream->data_base[i] + cfg->chn_cfg[i].size - 1;
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadBufferEnd] + ch * 4, (u32)addr_end);
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadBufferEndEx] + ch * 4, addr_end >> 32);

    //set read cache (table) base
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadCacheBase] + ch * 4, (u32)stream->table_base[i]);
    DEC400WriteReg(ctx, hw_reg_offset[gcregAHBDECReadCacheBaseEx] + ch * 4, stream->table_base[i] >> 32);
  }
}

#if 0
/*******************************************************************************
 Function name   : GetDec400Attribute
 Description     : Get dec400's attribute
 Return type     : i32
*******************************************************************************/
i32 GetDec400Attribute(u32 *tile_size, u32 *bits_tile_in_table,
                       u32 *planar420_cbcr_table_style, u32 dec400Enable) {
#ifdef SUPPORT_DEC400
  Dec400TileInfo tile = {0, 0, 0};
  VCEncDec400GetTileSize(&tile, dec400Enable);
  *tile_size = tile.tile_size;
  *bits_tile_in_table = tile.bits_tile_in_table;
  *planar420_cbcr_table_style = hw_feature->planar420_cbcr_table_style;
  return DEC400_OK;
#else
  return DEC400_ERROR;
#endif
}
#endif

/*******************************************************************************
 Function name   : VCEncEnableDec400
 Description     :
 Return type     : i32 - error code
 Argument        : VCDec400data *dec400_data - pointer to struct VCDec400data.
*******************************************************************************/
i32 VCEncEnableDec400(VCDec400data *dec400_data) {
#ifdef SUPPORT_DEC400
  Dec400ReadContext context;
  memset(&context, 0, sizeof(context));
  VCDec400StreamDsc *stream = NULL;
  Dec400StreamReadCfg *cfg = &context.strm_cfg;
  if (!DEC400_PARSE(dec400_data->dec400Enable, MODE))
    return DEC400_OK;

  u32 i = 0;
  context.occupied_num = 0;
  context.compress_align_mode = _getCompressionAlignMode(dec400_data->dec400Enable);
  context.input_alignment = dec400_data->input_alignment;
  context.ewl = dec400_data->ewl_inst;
  context.vcmd = dec400_data->vcmd;
  if (EWLGetVCMDSupport(dec400_data->ewl_inst) == 0) {
    context.vcmd = NULL;
  }

  DEC400WriteReg(&context, hw_reg_offset[gcregAHBDECControl], 0x00000000);
  DEC400WriteReg(&context, hw_reg_offset[gcregAHBDECControlEx], 0x00000000);

  if (hw_feature->finish_mode == DEC400_FLUSH) {
    DEC400WriteReg(&context, hw_reg_offset[gcregAHBDECIntrEnblEx2], FULL_FF);
    DEC400WriteReg(&context, hw_reg_offset[gcregAHBDECIntrEnbl], FULL_FF);
  } else {
    if (DEC400_PARSE(stream->dec400Enable, MODE) == 1) {
      VCEncSetDec400StreamBypass(dec400_data);
      return DEC400_OK;
    }
  }

  for (i=0; i<MAX_INPUT_CHANNEL_NUM; i++) {
    stream = &dec400_data->streams[i];
    if (stream->data_base[0] == 0) {
      continue;
    }

    Dec400ParseStreamCfg(&context, stream);
    if ((context.occupied_num + cfg->planes) >= MAX_DEC400_CHANNEL_NUM) {
      //DEC400 read channel are all occupied
      break;
    }

    Dec400SetStreamCfg(&context, stream);
    context.occupied_num += cfg->planes;

  }

#endif
  return DEC400_OK;
}

/*******************************************************************************
 Function name   : VCEncDisableDec400
 Description     :
 Return type     : void
 Argument        : VCDec400data *dec400_data - pointer to struct VCDec400data.
*******************************************************************************/
void VCEncDisableDec400(const void *ewl, void *vcmd) {
#ifdef SUPPORT_DEC400
  Dec400ReadContext context;
  u32 loop = 1000;
  context.ewl = ewl;
  context.vcmd = vcmd;
if (EWLGetVCMDSupport(ewl) == 0) {
    context.vcmd = NULL;
  }

  if (hw_feature->finish_mode == DEC400_RESET) {
    //do SW reset after one frame and wait it done by HW if needed
    DEC400WriteRegBack(&context, hw_reg_offset[gcregAHBDECControl], 0x00000010);
#ifdef PC_PCI_FPGA_DEMO
    usleep(80000);
#endif
  } else if (hw_feature->finish_mode == DEC400_FLUSH) {
    //flush tile status and wait for IRQ
    DEC400WriteRegBack(&context, hw_reg_offset[gcregAHBDECControl], 0x00000001);
    if (context.vcmd == 0) {
      do {
        if (DEC400ReadReg(ewl,
                          hw_reg_offset[gcregAHBDECIntrAcknowledgeEx2]) &
            0x00000001)
          break;
        usleep(80);
      } while (loop--);
    } else {
      //insert stall cmd.
      {
        u32 current_length = 0;
        VcmdbufCollectStallDec400(
            ewl,
            vcmd);
      }
      //clear int status register
      {
        u32 current_length = 0;
        VcmdbufCollectClrIntReadClearDec400Data(
            ewl,
            vcmd,
            hw_reg_offset[gcregAHBDECIntrAcknowledgeEx2] / 4);
      }
    }
  }
  for (u32 i = 0; i < MAX_DEC400_CHANNEL_NUM; i++) {
    _dec400_set_read_config(&context, i, 0x0);
  }

#endif
}

/*******************************************************************************
 Function name   : VCEncSetDec400StreamBypass
 Description     : Make dec400 bypass and nothing to do by configing the start
                   and end address as 0 for dec400 table and data that will make
                   dec400 cann't hold the frame buffer data.
 Return type     : void
 Argument        : VCDec400data *dec400_data - pointer to struct VCDec400data.
*******************************************************************************/
void VCEncSetDec400StreamBypass(VCDec400data *dec400_data) {
#ifdef SUPPORT_DEC400
  Dec400ReadContext context;
  u32 loop = 1000;
  context.ewl = dec400_data->ewl_inst;
  context.vcmd = dec400_data->vcmd;
  i32 core_id = EWLGetDec400Coreid(dec400_data->ewl_inst);
  if (core_id == -1) return;
  if (hw_feature == NULL) return;
    //do SW reset after one frame and wait it done by HW if needed
    DEC400WriteReg(&context, hw_reg_offset[gcregAHBDECControl], 0x00000010);
#ifdef PC_PCI_FPGA_DEMO
    usleep(80000);
#endif
    //disable global bypass to enable stream bypass
    DEC400WriteReg(&context, hw_reg_offset[gcregAHBDECControl], 0x02010088);
#endif
}

/*******************************************************************************
 Function name   : ProbeDec400HwFeature
 Description     : Query and set Dec400 hw feature to global variable hw_feature
 Return type     : u32. 1: failed ; 0: successfully
 Argument        : unsigned int  hw_id - hardware id.
*******************************************************************************/
unsigned int ProbeDec400HwFeature(unsigned int  hw_id) {
  u32 i = 0;
#ifdef SUPPORT_DEC400
  u32 list_num = sizeof(dec400_feature_list) / sizeof(Dec400Feature);

  pthread_mutex_lock(&dec400_mutex);
  if (!hw_feature) {
    for (i = 0; i < list_num; i++) {
      if (dec400_feature_list[i].hw_build_id == hw_id) {
        hw_feature = &dec400_feature_list[i];
        hw_reg_offset = reg_offset[hw_feature->reg_version_index];
        break;
      }
    }
  }
  pthread_mutex_unlock(&dec400_mutex);
  if (hw_feature == NULL) {
    PTRACE_E("DEC400: failed to get dec400 build id\n");
    return 1;
  }
#endif
  return 0;
}

/*******************************************************************************
 Function name   : GetDec400HwID
 Description     : Configure the Dec400 version ID
 Return type     : i32
 Argument        : void *ewl_inst - void pointer to ewl instance.
*******************************************************************************/
static i32 GetDec400HwID(void *ewl_inst) {
#ifdef SUPPORT_DEC400
  i32 core_id = 0;
  u32 dec400_hw_id = 0;
  int list_num = sizeof(dec400_feature_list) / sizeof(Dec400Feature);

  //get version id
  if ((core_id = EWLGetDec400Coreid(ewl_inst)) == -1)
    return DEC400_INVALID_ARGUMENT;

  if ((dec400_hw_id = EWLGetConfigRegister(ewl_inst, core_id, EWL_CLIENT_TYPE_DEC400, 0x30)) == 0)
    return DEC400_INVALID_ARGUMENT;

  return dec400_hw_id;
#endif
  return DEC400_OK;
}

/*******************************************************************************
 Function name   : VCEncDec400RegisiterWL
 Description     : Configure the Dec400 features based on the build id.
 Return type     : i32
 Argument        : void *ewl_inst - void pointer to ewl instance.
*******************************************************************************/
i32 VCEncDec400RegisiterWL(void *ewl_inst) {
#ifdef SUPPORT_DEC400
#ifdef DEC400_FAST_CLEAR_ANALYSIS_BY_HW
  if (EWLGetVCMDSupport(ewl_inst) != 0) {
    if (EWLIsVCMDSupportM2REG(ewl_inst) != 0) {
      PTRACE_E("The version of VCMD does not support M2REG.\n");
      return DEC400_ERROR;
    }
  } else {
    PTRACE_E("The macro DEC400_FAST_CLEAR_ANALYSIS_BY_HW needs to be used in VCMD mode.\n");
    return DEC400_ERROR;
  }
#endif
  i32 id = GetDec400HwID(ewl_inst);
  if (id < 0) {
    return DEC400_ERROR;
  }

  if (ProbeDec400HwFeature(id)) {
    return DEC400_ERROR;
  }
#endif
  return DEC400_OK;
}
#endif

/*------------------------------------------------------------------------------
    Function name : EncGetDec400TsBufferSize
    Description   : Returns the Dec400 tile status buffer size in byte by given
                    format, width, height and alignment.
    Return type   : void
------------------------------------------------------------------------------*/
void EncGetDec400TsBufferSize(u32 type, u32 width, u32 height, u32 alignment,
                                u32 *luma_size, u32*chroma_size,
                                u32 *picture_size, u32 scanType, u32 dec400Enable,
                                u32 dec400TSHeaderEnable) {
  u32 luma_stride = 0, chroma_stride = 0;
  u32 luma_sz = 0, chroma_sz = 0;
  u32 luma_tab_sz = 0, chroma_tab_sz = 0, chroma_tab_sz2 = 0;
  u32 chroma_tab_padding = 0, pic_tab_sz = 0;
  Dec400TileInfo tile_info;
  u32 planar420_cbcr_table_style = 0;

  u32 tile_sz, ts_bits;

#ifdef SUPPORT_DEC400
  VCEncDec400GetTileSize(&tile_info, dec400Enable);
  tile_sz = tile_info.tile_size;
  ts_bits = tile_info.bits_tile_in_table;
  planar420_cbcr_table_style = hw_feature->planar420_cbcr_table_style;
#else
  tile_sz = 256;
  ts_bits = 4;
#endif

  if (alignment == 0) alignment = 256;

  EncGetAlignedByteStride(width, type, &luma_stride, &chroma_stride, alignment, scanType);

  luma_sz = luma_stride * height;
  chroma_sz = chroma_stride * height;

  luma_tab_sz = STRIDE(STRIDE(luma_sz / tile_sz * ts_bits, 8) / 8, 16);
  chroma_tab_sz = STRIDE(chroma_sz / tile_sz * ts_bits, 8) / 8;
  chroma_tab_sz2 = STRIDE(chroma_sz / 2 / tile_sz * ts_bits, 8) / 8;

  switch (type) {
    case ENC_PIXFMT_YUV420_PLANAR:
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_I010:
      if (planar420_cbcr_table_style == 1) {
        chroma_tab_padding = STRIDE(chroma_tab_sz, 16) - chroma_tab_sz;
      } else {
        //padding cb cr respectively
        chroma_tab_sz = 2 * STRIDE(chroma_tab_sz2, 16);
      }
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR:
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU:
      chroma_tab_sz = STRIDE(chroma_tab_sz2, 16);
      break;
    case ENC_PIXFMT_RGB444:
    case ENC_PIXFMT_BGR444:
    case ENC_PIXFMT_RGB555:
    case ENC_PIXFMT_BGR555:
    case ENC_PIXFMT_RGB565:
    case ENC_PIXFMT_BGR565:
    case ENC_PIXFMT_RGB888:
    case ENC_PIXFMT_BGR888:
    case ENC_PIXFMT_RGB101010:
    case ENC_PIXFMT_BGR101010:
    case ENC_PIXFMT_RGBX8888:
    case ENC_PIXFMT_BGRX8888:
    case ENC_PIXFMT_RGBX1010102:
    case ENC_PIXFMT_BGRX1010102:
    case ENC_PIXFMT_YUV422_INTERLEAVED_YUYV:
      if (scanType == VCENC_SUPERTILEX_SCAN) {
        luma_tab_sz = STRIDE(
                    STRIDE((luma_stride * ((height + 63) / 64)) / tile_sz * ts_bits, 8) / 8,
                    16);
      }
      chroma_tab_sz = 0;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010:
      chroma_tab_sz = STRIDE(chroma_tab_sz2, 16);
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010_TILE_4_4:
      luma_tab_sz = STRIDE(
                      STRIDE((luma_sz / 4) / tile_sz * ts_bits, 8) / 8,
                      16);
      chroma_tab_sz = STRIDE(
                        STRIDE((chroma_stride * STRIDE(height / 2, 4) / 4) / tile_sz * ts_bits, 8) / 8,
                        16);
      break;
    case ENC_PIXFMT_Y8b:
    case ENC_PIXFMT_Y10bWL:
    case ENC_PIXFMT_Y10bWH:
      chroma_tab_sz = 0;
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_8_8:
    case ENC_PIXFMT_YUV420_10BIT_TILE_8_8:
      luma_tab_sz = STRIDE(
                      STRIDE((luma_sz / 8) / tile_sz * ts_bits, 8) / 8,
                      16);
      chroma_tab_sz = STRIDE(STRIDE((chroma_stride * STRIDE(height / 2, 4) / 4) /
                                     tile_sz * ts_bits,
                                 8) /
                              8,
                          16);
      break;
    case ENC_PIXFMT_Y8b_TILE_8_8:
    case ENC_PIXFMT_Y10bWH_TILE_8_8:
      luma_tab_sz = STRIDE(
                      STRIDE((luma_sz / 8) / tile_sz * ts_bits, 8) / 8,
                      16);
      chroma_tab_sz = 0;
      break;

    default:
      PTRACE_E("DEC400 do not support input picture format\n");
      chroma_tab_sz = luma_tab_sz = 0;
      break;
  }

  if (dec400TSHeaderEnable) {
    luma_tab_sz += DEC400_HEADER_BUF_SIZE;
    luma_tab_sz = STRIDE(luma_tab_sz, 256);
    if (chroma_tab_sz)
      chroma_tab_sz += DEC400_HEADER_BUF_SIZE;
    if (type == ENC_PIXFMT_YUV420_PLANAR || type == ENC_PIXFMT_YUV420_PLANAR_10BIT_I010)
      chroma_tab_sz += DEC400_HEADER_BUF_SIZE;
  }

  pic_tab_sz = luma_tab_sz + chroma_tab_sz;
  pic_tab_sz += chroma_tab_padding;

  if (luma_size != NULL) *luma_size = luma_tab_sz;
  if (chroma_size != NULL) *chroma_size = chroma_tab_sz;
  if (picture_size != NULL) *picture_size = pic_tab_sz;
}


/*******************************************************************************
 Function name   : VCEncDec400UnregisiterWl
 Description     : unRegisiter Wrapper Layer
 Return type     : void
*******************************************************************************/
void VCEncDec400UnregisiterWl(void *ewl_inst) {
#ifdef SUPPORT_DEC400
  hw_feature = NULL;
  return;
#endif
}
