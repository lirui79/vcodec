/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2014 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#ifndef ENC_DEC400_H
#define ENC_DEC400_H

#include "base_type.h"
#include "osal.h"
#include "vcmdbuf.h"
#include "enccommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DEC400_CHANNEL_NUM 32
#define MAX_INPUT_CHANNEL_NUM (MAX_OVERLAY_NUM + 1)
#define DEC400_CONFIG_REG_NUM 9
/** Function return values */
typedef enum {
  DEC400_OK = 0,
  DEC400_ERROR = -1,
  DEC400_INVALID_ARGUMENT = -2
} DEC400Ret;

enum DEC400_REG {
  gcregAHBDECReadConfigEx,
  gcregAHBDECReadConfig,
  gcregAHBDECReadBufferBase,
  gcregAHBDECReadBufferBaseEx,
  gcregAHBDECReadBufferEnd,
  gcregAHBDECReadBufferEndEx,
  gcregAHBDECReadCacheBase,
  gcregAHBDECReadCacheBaseEx,
  gcregAHBDECWriteConfig,
  gcregAHBDECControl,
  gcregAHBDECControlEx,
  gcregAHBDECIntrEnblEx2,
  gcregAHBDECIntrAcknowledgeEx2,
  gcregAHBDECIntrEnbl,
  gcregAHBDECFastClearValue,
  DEC400_REG_MAX
};

/*------------------------------------------------------------------------------
    DEC400_FINISH_MODE specifies Dec400 completes compression in different way
    based on the hardware requirements.
------------------------------------------------------------------------------*/
enum DEC400_FINISH_MODE {
  DEC400_NONE,
  DEC400_FLUSH, /* Software flush all streams */
  DEC400_RESET, /* Soft reset DEC */
};

/*------------------------------------------------------------------------------
    DEC400_COMPRESSION_ENABLE
------------------------------------------------------------------------------*/
#define DEC400_COMP_ENABLE        (0x01 << 0)

/*------------------------------------------------------------------------------
    DEC400_COMPRESSION_FORMAT specifies the compression color format.
------------------------------------------------------------------------------*/
#define DEC400_COMP_FMT_ARGB8     (0x00 << 3)
#define DEC400_COMP_FMT_XRGB8     (0x01 << 3)
#define DEC400_COMP_FMT_AYUV      (0x02 << 3)
#define DEC400_COMP_FMT_UYUV      (0x03 << 3)
#define DEC400_COMP_FMT_YUV2      (0x04 << 3)
#define DEC400_COMP_FMT_YUV_ONLY  (0x05 << 3)
#define DEC400_COMP_FMT_UV_MIX    (0x06 << 3)
#define DEC400_COMP_FMT_ARGB4     (0x07 << 3)
#define DEC400_COMP_FMT_XRGB4     (0x08 << 3)
#define DEC400_COMP_FMT_A1RGB5    (0x09 << 3)
#define DEC400_COMP_FMT_X1RGB5    (0x0A << 3)
#define DEC400_COMP_FMT_R5G6B5    (0x0B << 3)
#define DEC400_COMP_FMT_A2RGB10   (0x0F << 3)
#define DEC400_COMP_FMT_ARGB16    (0x13 << 3)

/*------------------------------------------------------------------------------
    DEC400_ALIGN_MODE specifies the compression result size alignment mode.
------------------------------------------------------------------------------*/
#define DEC400_COMP_ALIGN_1   (0 << 16)
#define DEC400_COMP_ALIGN_16  (1 << 16)
#define DEC400_COMP_ALIGN_32  (2 << 16)
#define DEC400_COMP_ALIGN_64  (3 << 16)

/*------------------------------------------------------------------------------
    DEC400_TILE_MODE specifies the width (scanline, x, length, stride) of the
    tile and the height (rows, y).
------------------------------------------------------------------------------*/
#define DEC400_TM_TILE_8x8_x      (0x00 << 25)
#define DEC400_TM_TILE_8x8_y      (0x01 << 25)
#define DEC400_TM_TILE_16x4       (0x02 << 25)
#define DEC400_TM_TILE_8x4        (0x03 << 25)
#define DEC400_TM_TILE_4x8        (0x04 << 25)
#define DEC400_TM_TILE_4x4        (0x05 << 25)
#define DEC400_TM_RASTER_16x4     (0x06 << 25)
#define DEC400_TM_TILE_64x4       (0x07 << 25)
#define DEC400_TM_TILE_32x4       (0x08 << 25)
#define DEC400_TM_RASTER_256x1    (0x09 << 25)
#define DEC400_TM_RASTER_128x1    (0x0A << 25)
#define DEC400_TM_RASTER_64x4     (0x0B << 25)
#define DEC400_TM_RASTER_64x1     (0x0F << 25)
#define DEC400_TM_TILE_16x8       (0x10 << 25)
#define DEC400_TM_RASTER_32x4     (0x13 << 25)
#define DEC400_TM_RASTER_32x1     (0x16 << 25)
#define DEC400_TM_RASTER_16x1     (0x17 << 25)
#define DEC400_TM_TILE_8x4_S      (0x1F << 25)
#define DEC400_TM_TILE_16x4_S     (0x20 << 25)
#define DEC400_TM_TILE_32x4_S     (0x21 << 25)
#define DEC400_TM_TILE_32x8       (0x24 << 25)
#define DEC400_TM_TILE_16x8       (0x10 << 25)

/*------------------------------------------------------------------------------
    DEC400_BIT_DEPTH specifies the bit depth for Y/UV/Bayer format.
------------------------------------------------------------------------------*/
#define DEC400_BIT_DEPTH_8     (0 << 16)
#define DEC400_BIT_DEPTH_10    (1 << 16)
#define DEC400_BIT_DEPTH_12    (2 << 16)
#define DEC400_BIT_DEPTH_14    (3 << 16)
#define DEC400_BIT_DEPTH_16    (4 << 16)


/*------------------------------------------------------------------------------
    Dec400 maintains compression bookkeeping data per stream which it calls the
    tile status.
------------------------------------------------------------------------------*/
typedef struct {
  u16 tile_size;         /* Compression unit tile size in bytes */
  u8 bits_tile_in_table; /* Tile status bits needed for each tile */
  u8 tile_mode;          /* DEC400_TILE_MODE */
} Dec400TileInfo;


/*------------------------------------------------------------------------------
    Dec400 configuration feature parameters based on hardware build id.
------------------------------------------------------------------------------*/
typedef struct {
  u32 hw_build_id;
  u32 tile_mode_idx; /* different tile mode based on the hardware requirements */
  u32 dec400_data_align; /* the size of the compression data alignment bits width */
  u8 planar420_cbcr_table_style; /* 0-separated 1-continuous */
  u8 finish_mode;                /* DEC400_FINISH_MODE */
  u8 hw_work_around;
  u8 reg_version_index;
} Dec400Feature;

typedef struct {
  u32 compress_fmt;
  u32 tile_mode;
  u32 size;
  u32 fastclear_val;
} Dec400ChannelReadCfg;

typedef struct {
  EncPixelFormat pix_fmt;
  u32 planes;
  u32 bit_depth;
  u32 compress_align_mode;
  Dec400ChannelReadCfg chn_cfg[3];
}Dec400StreamReadCfg;

typedef struct {
  VcmdDes_t *vcmd;
  const void *ewl;
  u32 occupied_num;
  u32 compress_align_mode;
  u32 input_alignment;
  Dec400StreamReadCfg strm_cfg;
}Dec400ReadContext;

typedef struct {
  u32 dec400Enable;
  EncPixelFormat pix_fmt;
  u32 width;          // width of plane 0, in pixel
  u32 height;         // height of plane 0, in pixel
  u32 super_tile;
  ptr_t data_base[3];   // compress data base
  ptr_t table_base[3];  // Tile status table base
  u32 fastclearEnable[3];
  u32 fastclear_val[3]; // Fast clear value
  u32 is_osd;
}VCDec400StreamDsc;

typedef struct {
  u32 dec400Enable;
  u32 input_alignment;
  VcmdDes_t *vcmd;
  const void *ewl_inst;
  VCDec400StreamDsc streams[MAX_INPUT_CHANNEL_NUM];
} VCDec400data;

#define EncDec400SetAddrRegisterValue(dec400_data, name, value)    \
  do {                                                             \
    if (sizeof(ptr_t) == 8) {                                      \
      DEC400WriteReg(dec400_data, dec400_reg[name], value);                    \
      DEC400WriteReg(dec400_data, dec400_reg[name##Ex], (u32)((value) >> 32)); \
    } else {                                                       \
      DEC400WriteReg(dec400_data, dec400_reg[name], (u32)(value));             \
    }                                                              \
  } while (0)


/*------------------------------------------------------------------------------
    Function prototypes
------------------------------------------------------------------------------*/
#ifndef SUPPORT_DEC400
#define GetDec400Attribute(...)           (0)
#define VCEncEnableDec400(...)            (0)
#define VCEncDisableDec400(...)
#define VCEncSetDec400StreamBypass(...)
#define VCEncDec400RegisiterWL(...)       (0)
#define VCEncDec400UnregisiterWL(...)
#else
i32  GetDec400Attribute(u32 *tile_size, u32 *bits_tile_in_table,
	                      u32 *planar420_cbcr_table_style, u32 dec400Enable);
i32  VCEncEnableDec400(VCDec400data *dec400_data);
void VCEncDisableDec400(const void *ewl, void *vcmd);
void VCEncSetDec400StreamBypass(VCDec400data *dec400_data);
unsigned int ProbeDec400HwFeature(unsigned int  hw_id);
i32  VCEncDec400RegisiterWL(void *ewl_inst);
void VCEncDec400UnregisiterWL(void *ewl_inst);


#endif

#ifdef __cplusplus
}
#endif

#endif
