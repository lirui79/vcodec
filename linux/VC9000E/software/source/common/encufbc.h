/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2014 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#ifndef ENC_UFBC_H
#define ENC_UFBC_H

#include "base_type.h"
#include "osal.h"
#include "vcmdbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Function return values */
/*
enum UFBC_REG {
  sw_HW_ID,
  sw_HW_BuildDate,
  sw_ufbc_enable,
  sw_ufbc_hdr_base_addr_lsb,
  sw_ufbc_hdr_base_addr_msb,
  sw_ufbc_hdr_addr_lsb,
  sw_ufbc_hdr_addr_msb,
  sw_ufbc_hdr_stride,
  sw_ufbc_dec_err,
  sw_ufbc_dec_err_enable,
  UFBC_REG_MAX
};
*/
#define UFBC_CHANNEL_NUM 3
#define UFBC_TILE_SIZE 256
#define UFBC_AFBC_REG_MAX 10
#define UFBC_DEC400_REG_MAX 28
#define UFBC_PVRIC_REG_MAX 32

/*------------------------------------------------------------------------------
    UFBC_COMPRESSION_FORMAT specifies the compression color format.
------------------------------------------------------------------------------*/
#define UFBC_COMP_FMT_ARGB8     0
#define UFBC_COMP_FMT_XRGB8     1
#define UFBC_COMP_FMT_Y_ONLY    2
#define UFBC_COMP_FMT_UV_MIX    3
#define UFBC_COMP_FMT_A2RGB10   4

/*------------------------------------------------------------------------------
    UFBC_ALIGN_MODE specifies the compression result size alignment mode.
------------------------------------------------------------------------------*/
#define UFBC_COMP_ALIGN_32  0
#define UFBC_COMP_ALIGN_64  1

/*------------------------------------------------------------------------------
    UFBC_TILE_MODE specifies the width (scanline, x, length, stride) of the
    tile and the height (rows, y).
------------------------------------------------------------------------------*/
#define UFBC_TM_RASTER_256x1    0
#define UFBC_TM_RASTER_128x1    1
#define UFBC_TM_RASTER_64x1     2
#define UFBC_TM_TILE_16x4       3
#define UFBC_TM_TILE_64x4       4
#define UFBC_TM_TILE_32x4       5

/*------------------------------------------------------------------------------
    UFBC_BIT_DEPTH specifies the bit depth for Y/UV/Bayer format.
------------------------------------------------------------------------------*/
#define UFBC_BIT_DEPTH_8     0
#define UFBC_BIT_DEPTH_10    1
#define UFBC_BIT_DEPTH_12    2
#define UFBC_BIT_DEPTH_14    3
#define UFBC_BIT_DEPTH_16    4

/*------------------------------------------------------------------------------
    UFBC_COMPRESSION_FORMAT_PVRIC specifies the compression color format.
------------------------------------------------------------------------------*/
#define UFBC_PVRIC_COMP_FMT_ARGB8     0
#define UFBC_PVRIC_COMP_FMT_A2RGB10   1
#define UFBC_PVRIC_COMP_FMT_NV12      2
#define UFBC_PVRIC_COMP_FMT_P010      3

/*------------------------------------------------------------------------------
    UFBC_TILE_MODE_PVRIC specifies the width (scanline, x, length, stride) of the
    tile and the height (rows, y).
------------------------------------------------------------------------------*/
#define UFBC_PVRIC_TM_8x8      0
#define UFBC_PVRIC_TM_16x4     1
#define UFBC_PVRIC_TM_32x2     2

typedef struct {
  u32 compress_fmt;
  u32 tile_mode;
  ptr_t headerAddress;
  ptr_t blockAddress;
  u32 blockWidth;
  u32 blockHeight;
  u32 headerOffset;
  u32 headerStride;
} UFBCChannelCfg_dec400;

typedef struct {
  u32 compress_fmt;
  u32 tile_mode;
  ptr_t headerStartAddr;
  ptr_t blockStartAddr;
  ptr_t headerBaseAddr;
  ptr_t blockBaseAddr;
  u32 blockWidth;
  u32 blockHeight;
  u32 headerStride;
} UFBCChannelCfg_pvric;

typedef struct {
  u32 regs[UFBC_DEC400_REG_MAX];
  UFBCChannelCfg_dec400 chn_cfg[UFBC_CHANNEL_NUM];
  u32 compress_align_mode;
  u32 planes;
  u32 bit_depth;
} dec400RegParam;

typedef struct {
  ptr_t baseAddress;
  ptr_t startAddress;
  u32 regs[UFBC_AFBC_REG_MAX];
  u32 stride;
  u32 yuvTrans;
  u32 blockType;
  u32 blockSplit;
} afbcRegParam;

typedef struct {
  UFBCChannelCfg_pvric chn_cfg[UFBC_CHANNEL_NUM];
  u32 regs[UFBC_PVRIC_REG_MAX];
  u32 planes;
  u32 consColorVal[4];
} pvricRegParam;

typedef struct {
  u32 mode;
  const void *ewl_inst;
  VcmdDes_t *vcmd;
  u32 hwId;
  u32 has_ufbc;
  u32 format;
  u32 ioSize;
  union {
    afbcRegParam afbc;
    dec400RegParam dec400;
    pvricRegParam pvric;
  } regParam;
} EncUfbc;

typedef struct {
  u32 tileSize;
  u32 stride[UFBC_CHANNEL_NUM];
  ptr_t headerAddress[UFBC_CHANNEL_NUM];
} dec400Param;

typedef struct {
  u32 yuvTrans;
  u32 blockType;
  u32 blockSplit;
  u32 blockWidth;
  u32 blockHeight;
} afbcParam;

typedef struct {
  u32 stride[UFBC_CHANNEL_NUM];
  u32 blockType;
  u32 consColorVal[4];
} pvricParam;

typedef struct {
  u32 mode;
  u32 format;
  u32 xOffset;
  u32 yOffset;
  u32 width;
  u32 height;
  u32 alignment;
  VcmdDes_t *vcmd;
  ptr_t baseAddress[UFBC_CHANNEL_NUM];
  union {
    afbcParam afbc;
    dec400Param dec400;
    pvricParam pvric;
  } param;
} EncUfbcParam;

#ifdef SUPPORT_UFBC
void EncUfbcInit(EncUfbc *ufbc, const void *ewl, u32 ufbcSupport);
i32 EncUfbcSetParams(EncUfbc *ufbc, EncUfbcParam *ufbcParam);
void EncUfbcAsicStart(EncUfbc *ufbc);
void EncUfbcAsicStop(const void *ewl, void *inst, u32 ufbc_mode);
#else
#define EncUfbcInit(...)
#define EncUfbcSetParams(...)           (0)
#define EncUfbcAsicStart(...)
#define EncUfbcAsicStop(...)
#endif

#ifdef __cplusplus
}
#endif

#endif
