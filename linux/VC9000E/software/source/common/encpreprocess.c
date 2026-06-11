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
--  Description : Preprocessor setup
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "encpreprocess.h"
#include "enccommon.h"

#define HSWREG(n) ((n)*4)

/*------------------------------------------------------------------------------
  EncPreProcessAlloc
------------------------------------------------------------------------------*/
i32 EncPreProcessAlloc(preProcess_s *preProcess, i32 mbPerPicture) {
  i32 status = ENCHW_OK;
  i32 i;

  for (i = 0; i < 3; i++) {
    preProcess->roiSegmentMap[i] = (u8 *)EWLcalloc(mbPerPicture, sizeof(u8));
    if (preProcess->roiSegmentMap[i] == NULL) status = ENCHW_NOK;
  }

  for (i = 0; i < 2; i++) {
    preProcess->skinMap[i] = (u8 *)EWLcalloc(mbPerPicture, sizeof(u8));
    if (preProcess->skinMap[i] == NULL) status = ENCHW_NOK;
  }

  preProcess->mvMap = (i32 *)EWLcalloc(mbPerPicture, sizeof(i32));
  if (preProcess->mvMap == NULL) status = ENCHW_NOK;

  preProcess->scoreMap = (u8 *)EWLcalloc(mbPerPicture, sizeof(u8));
  if (preProcess->scoreMap == NULL) status = ENCHW_NOK;

  if (status != ENCHW_OK) {
    EncPreProcessFree(preProcess);
    return ENCHW_NOK;
  }

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------
  EncPreProcessFree
------------------------------------------------------------------------------*/
void EncPreProcessFree(preProcess_s *preProcess) {
  i32 i;

  for (i = 0; i < 3; i++) {
    if (preProcess->roiSegmentMap[i]) EWLfree(preProcess->roiSegmentMap[i]);
    preProcess->roiSegmentMap[i] = NULL;
  }

  for (i = 0; i < 2; i++) {
    if (preProcess->skinMap[i]) EWLfree(preProcess->skinMap[i]);
    preProcess->skinMap[i] = NULL;
  }

  if (preProcess->mvMap) EWLfree(preProcess->mvMap);
  preProcess->mvMap = NULL;

  if (preProcess->scoreMap) EWLfree(preProcess->scoreMap);
  preProcess->scoreMap = NULL;
}

/*------------------------------------------------------------------------------
    EncPreGetHwFormat
------------------------------------------------------------------------------*/
u32 EncPreGetHwFormat(u32 inputFormat) {
  /* Input format mapping API values to SW/HW register values. */
  static const u32 inputFormatMapping[66] = {
      ASIC_INPUT_YUV420PLANAR,
      ASIC_INPUT_YUV420SEMIPLANAR,
      ASIC_INPUT_YUV420SEMIPLANAR,
      ASIC_INPUT_YUYV422INTERLEAVED,
      ASIC_INPUT_UYVY422INTERLEAVED,
      ASIC_INPUT_RGB565,
      ASIC_INPUT_RGB565,
      ASIC_INPUT_RGB555,
      ASIC_INPUT_RGB555,
      ASIC_INPUT_RGB444,
      ASIC_INPUT_RGB444,
      ASIC_INPUT_RGB888,
      ASIC_INPUT_RGB888,
      ASIC_INPUT_RGB101010,
      ASIC_INPUT_RGB101010,
      ASIC_INPUT_I010,
      ASIC_INPUT_P010,
      ASIC_INPUT_PACKED_10BIT_PLANAR,
      ASIC_INPUT_PACKED_10BIT_Y0L2,
      ASIC_INPUT_YUV420_TILE32,
      ASIC_INPUT_YUV420_TILE16_PCK4,
      ASIC_INPUT_YUV420_TILE4,
      ASIC_INPUT_YUV420_TILE4,
      ASIC_INPUT_P010_TILE4,
      ASIC_INPUT_SP_101010,
      ASIC_INPUT_YUV422_888,
      ASIC_INPUT_YUV420_TILE64,
      ASIC_INPUT_YUV420_TILE64,
      ASIC_INPUT_YUV420PCK16_TILE,
      ASIC_INPUT_YUV420PCK10_TILE,
      ASIC_INPUT_YUV420PCK10_TILE,
      ASIC_INPUT_YUV420_TILE128,
      ASIC_INPUT_YUV420_TILE128,
      ASIC_INPUT_YUV420PCK10_TILE96,
      ASIC_INPUT_YUV420PCK10_TILE96,
      ASIC_INPUT_YUV420SP_TILE8,
      ASIC_INPUT_P010_TILE8,
      ASIC_INPUT_YUV420PLANAR,
      ASIC_INPUT_YUV420_UV_TILE64x2,
      ASIC_INPUT_YUV420_10BIT_UV_TILE128x2,
      ASIC_INPUT_RGB888_24BIT,
      ASIC_INPUT_RGB888_24BIT,
      ASIC_INPUT_RGB888_24BIT,
      ASIC_INPUT_RGB888_24BIT,
      ASIC_INPUT_RGB888_24BIT,
      ASIC_INPUT_RGB888_24BIT,
      ASIC_INPUT_YUV422_888,
      ASIC_INPUT_YUV444PLANAR,
      ASIC_INPUT_YUV444_XYUV8888,
      ASIC_INPUT_YUV444_XYUV2101010,
      ASIC_INPUT_RGB888,
      ASIC_INPUT_RGB888,
      ASIC_INPUT_RGB101010,
      ASIC_INPUT_RGB101010,
      ASIC_INPUT_P010,
      ASIC_INPUT_YUV420PLANAR,
      ASIC_INPUT_I010,
      ASIC_INPUT_P010,
      ASIC_INPUT_YUV420SP10b,
      ASIC_INPUT_YUV422P10bWL,
      ASIC_INPUT_YUV420SP_TILE8,
      ASIC_INPUT_P010_TILE8,
      ASIC_INPUT_YUYV422INTERLEAVED,
      ASIC_INPUT_UYVY422INTERLEAVED,
      ASIC_INPUT_YUV420SP_TILE8,
      ASIC_INPUT_PACKED_10BIT_PLANAR,};

  return inputFormatMapping[inputFormat];
}

static const u32 rgbMaskBits[64][3] = {
    {0, 0, 0},   {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {15, 10, 4}, /* RGB565 */
    {4, 10, 15}, /* BGR565 */
    {14, 9, 4},  /* RGB565 */
    {4, 9, 14},  /* BGR565 */
    {11, 7, 3},  /* RGB444 */
    {3, 7, 11},  /* BGR444 */
    {23, 15, 7}, /* RGB888 */
    {7, 15, 23}, /* BGR888 */
    {29, 19, 9}, /* RGB101010 */
    {9, 19, 29}, /* BGR101010 */
    {0, 0, 0},   {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0},   {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0},   {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0},   {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {0, 0, 0},   {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
    {7, 15, 23}, /* RGB888 24bit,  VCENC_RGB888_24BIT */
    {23, 15, 7}, /* BGR888 24bit,  VCENC_BGR888_24BIT */
    {7, 23, 15}, /* RBG888 24bit,  VCENC_RBG888_24BIT */
    {23, 7, 15}, /* GBR888 24bit,  VCENC_GBR888_24BIT */
    {15, 23, 7}, /* BRG888 24bit,  VCENC_BRG888_24BIT */
    {15, 7, 23}, /* GRB888 24bit,  VCENC_GRB888_24BIT */
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {31, 23, 15}, /* RGBX8888 */
    {15, 23, 31}, /* BGRX8888 */
    {31, 21, 11}, /* RGBX1010102 */
    {11, 21, 31}, /* BGRX1010102 */
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0},
    {0, 0, 0}
};

/*------------------------------------------------------------------------------

  EncPreProcessCheck

  Check image size: Cropped frame _must_ fit inside of source image

  Input preProcess Pointer to preProcess_s structure.

  Return  ENCHW_OK  No errors.
    ENCHW_NOK Error condition.

------------------------------------------------------------------------------*/
i32 EncPreProcessCheck(const preProcess_s *preProcess, i32 tileColumnNum) {
  u32 tmp;
  u32 width, height;
  u32 tileId;

#if 0 /* These limits apply for input stride but camstab needs the
  actual pixels without padding. */
  u32 w_mask;

  /* Check width limits: lum&ch strides must be full 64-bit addresses */
  if (preProcess->inputFormat == 0)       /* YUV 420 planar */
    w_mask = 0x0F;                      /* 16 multiple */
  else if (preProcess->inputFormat <= 1)  /* YUV 420 semiplanar */
    w_mask = 0x07;                      /* 8 multiple  */
  else if (preProcess->inputFormat <= 9)  /* YUYV 422 or 16-bit RGB */
    w_mask = 0x03;                      /* 4 multiple  */
  else                                    /* 32-bit RGB */
    w_mask = 0x01;                      /* 2 multiple  */

  if (preProcess->lumWidthSrc & w_mask)
  {
    status = ENCHW_NOK;
  }
#endif
  for (tileId = 0; tileId < tileColumnNum; tileId++) {
    if (preProcess->lumHeightSrc[tileId] & 0x01) {
      return ENCHW_NOK;
    }

    if (preProcess->lumWidthSrc[tileId] > MAX_INPUT_IMAGE_WIDTH) {
      return ENCHW_NOK;
    }
  }

  width = preProcess->lumWidth;
  height = preProcess->lumHeight;
  if (preProcess->rotation && preProcess->rotation != 3) {
    u32 tmp;

    tmp = width;
    width = height;
    height = tmp;
  }

  if (tileColumnNum) {
    if (tileColumnNum > 1 && preProcess->rotation)
        return ENCHW_NOK;

  tmp = MAX(preProcess->horOffsetSrc[0] + width, preProcess->horOffsetSrc[0]);
  if (tmp > MAX(preProcess->lumWidthSrc[0], preProcess->alignLumWidth)) {
      return ENCHW_NOK;
  }

  tmp = MAX(preProcess->verOffsetSrc[0] + height, preProcess->verOffsetSrc[0]);
  if (tmp > MAX(preProcess->lumHeightSrc[0], preProcess->alignLumHeight)) {
      return ENCHW_NOK;
    }
  }

  return ENCHW_OK;
}

u32 EncGetAlignedByteStride(int width, i32 input_format, u32 *luma_stride,
                            u32 *chroma_stride, u32 input_alignment, u32 scan_type) {
  u32 alignment = input_alignment == 0 ? 1 : input_alignment;
  u32 pixelByte = 1;

  if (luma_stride == NULL || chroma_stride == NULL) return 1;

  switch (input_format) {
    case ENC_PIXFMT_YUV420_PLANAR:   //VCENC_YUV420_PLANAR
    case ENC_PIXFMT_YVU420_PLANAR:  //VCENC_YVU420_PLANAR
      *luma_stride = STRIDE(width, alignment);
      *chroma_stride = STRIDE(width / 2, alignment);
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR:  //VCENC_YUV420_SEMIPLANAR
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU:  //VCENC_YUV420_SEMIPLANAR_VU
    case ENC_PIXFMT_YUV444_PLANAR:  //VCENC_YUV444_PLANAR
      *luma_stride = STRIDE(width, alignment);
      *chroma_stride = STRIDE(width, alignment);
      break;
    case ENC_PIXFMT_YUV422_INTERLEAVED_YUYV:  //VCENC_YUV422_INTERLEAVED_YUYV
    case ENC_PIXFMT_YUV422_INTERLEAVED_UYVY:  //VCENC_YUV422_INTERLEAVED_UYVY
    case ENC_PIXFMT_YUV422_INTERLEAVED_YVYU:  //VCENC_YUV422_INTERLEAVED_YVYU
    case ENC_PIXFMT_YUV422_INTERLEAVED_VYUY:  //VCENC_YUV422_INTERLEAVED_VYUY
    case ENC_PIXFMT_RGB565:   //VCENC_RGB565
    case ENC_PIXFMT_BGR565:   //VCENC_BGR565
    case ENC_PIXFMT_RGB555:   //VCENC_RGB555
    case ENC_PIXFMT_BGR555:   //VCENC_BGR555
    case ENC_PIXFMT_RGB444:   //VCENC_RGB444
    case ENC_PIXFMT_BGR444:  //VCENC_BGR444
      *luma_stride = STRIDE(width * 2, alignment);
      if (scan_type == VCENC_SUPERTILEX_SCAN) *luma_stride = STRIDE(STRIDE(width, 64) * 64 * 2, alignment);
      *chroma_stride = 0;
      pixelByte = 2;
      break;
    case ENC_PIXFMT_RGB888:  //VCENC_RGB888
    case ENC_PIXFMT_BGR888:  //VCENC_BGR888
    case ENC_PIXFMT_RGB101010:  //VCENC_RGB101010
    case ENC_PIXFMT_BGR101010:  //VCENC_BGR101010
    case ENC_PIXFMT_YUV444_XYUV8888:  //VCENC_YUV444_XYUV8888
    case ENC_PIXFMT_YUV444_XYUV2101010:  //VCENC_YUV444_XYUV2101010
    case ENC_PIXFMT_RGBX8888:  //VCENC_RGBX8888
    case ENC_PIXFMT_BGRX8888:  //VCENC_BGRX8888
    case ENC_PIXFMT_RGBX1010102:  //VCENC_RGBX1010102
    case ENC_PIXFMT_BGRX1010102:  //VCENC_BGRX1010102
      *luma_stride = STRIDE(width * 4, alignment);
      if (scan_type == VCENC_SUPERTILEX_SCAN) *luma_stride = STRIDE(STRIDE(width, 64) * 64  * 4, alignment);
      *chroma_stride = 0;
      pixelByte = 4;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_I010:  //VCENC_YUV420_PLANAR_10BIT_I010
      *luma_stride = STRIDE(width * 2, alignment);
      *chroma_stride = STRIDE(width / 2 * 2, alignment);
      pixelByte = 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010:  //VCENC_YUV420_PLANAR_10BIT_P010
    case ENC_PIXFMT_YVU420_PLANAR_10BIT_P010:  //VCENC_YVU420_PLANAR_10BIT_P010
      *luma_stride = STRIDE(width * 2, alignment);
      *chroma_stride = STRIDE(width * 2, alignment);
      pixelByte = 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_PACKED_PLANAR:  //VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR
      *luma_stride = STRIDE(width, 64);
      *chroma_stride = STRIDE(width, 64) / 2;
      break;
    case ENC_PIXFMT_YUV420_10BIT_PACKED_Y0L2:  //VCENC_YUV420_10BIT_PACKED_Y0L2
      *luma_stride = STRIDE(width, 4);
      *chroma_stride = 0;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_8BIT_TILE_32_32:  //VCENC_YUV420_PLANAR_8BIT_TILE_32_32
      *luma_stride = STRIDE(width, 32);
      *chroma_stride = STRIDE(width, 32) / 2;
      break;
    case ENC_PIXFMT_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4:  //VCENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4
      *luma_stride = STRIDE(width, 16);
      *chroma_stride = 0;
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR_8BIT_TILE_4_4:  //VCENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4
    case ENC_PIXFMT_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:  //VCENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4
      *luma_stride = STRIDE(width * 4, alignment);
      *chroma_stride = STRIDE(width * 4, alignment);
      break;
    case ENC_PIXFMT_YUV420_PLANAR_10BIT_P010_TILE_4_4:  //VCENC_YUV420_PLANAR_10BIT_P010_TILE_4_4
      *luma_stride = STRIDE(width * 4 * 2, alignment);
      *chroma_stride = STRIDE(width * 4 * 2, alignment);
      pixelByte = 2;
      break;
    case ENC_PIXFMT_YUV420_SEMIPLANAR_101010:  //VCENC_YUV420_SEMIPLANAR_101010
      *luma_stride = STRIDE((width + 2) / 3 * 4, alignment);
      *chroma_stride = STRIDE((width + 2) / 3 * 4, alignment);
      break;
    case ENC_PIXFMT_YUV422_888:  //JPEGENC_YUV422SP_888
    case ENC_PIXFMT_YVU422SP_888:  //JPEGENC_YVU422SP_888
      *luma_stride = STRIDE(width, alignment);
      *chroma_stride = STRIDE(width, alignment);
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_64_4:  //VCENC_YUV420_8BIT_TILE_64_4
    case ENC_PIXFMT_YUV420_UV_8BIT_TILE_64_4:  //VCENC_YUV420_UV_8BIT_TILE_64_4
      *luma_stride = STRIDE(STRIDE(width, 64) * 4, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 64) * 4, alignment);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_32_4:  //VCENC_YUV420_10BIT_TILE_32_4
      *luma_stride = STRIDE(STRIDE(width, 32) * 2 * 4, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 32) * 2 * 4, alignment);
      pixelByte = 2;
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_48_4:  //VCENC_YUV420_10BIT_TILE_48_4
    case ENC_PIXFMT_YUV420_VU_10BIT_TILE_48_4:  //VCENC_YUV420_VU_10BIT_TILE_48_4
      *luma_stride = STRIDE((width + 47) / 48 * 48 / 3 * 4 * 4, alignment);
      *chroma_stride = STRIDE((width + 47) / 48 * 48 / 3 * 4 * 4, alignment);
      break;
    case ENC_PIXFMT_YUV420_8BIT_TILE_128_2:  //VCENC_YUV420_8BIT_TILE_128_2
    case ENC_PIXFMT_YUV420_UV_8BIT_TILE_128_2:  //VCENC_YUV420_UV_8BIT_TILE_128_2
      *luma_stride = STRIDE(STRIDE(width, 128) * 2, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 128) * 2, alignment);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_96_2:  //VCENC_YUV420_10BIT_TILE_96_2
    case ENC_PIXFMT_YUV420_VU_10BIT_TILE_96_2:  //VCENC_YUV420_VU_10BIT_TILE_96_2
      *luma_stride = STRIDE((width + 95) / 96 * 96 / 3 * 4 * 2, alignment);
      *chroma_stride = STRIDE((width + 95) / 96 * 96 / 3 * 4 * 2, alignment);
      break;
    case ENC_PIXFMT_YVU420_8BIT_TILE_8_8:  //VCENC_YUV420_8BIT_TILE_8_8
    case ENC_PIXFMT_YUV420_8BIT_TILE_8_8:  //VCENC_YUV420_8BIT_TILE_8_8
      *luma_stride = STRIDE(STRIDE(width, 8) * 8, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 16) * 4, alignment);
      break;
    case ENC_PIXFMT_YUV420_10BIT_TILE_8_8:  //VCENC_YUV420_10BIT_TILE_8_8
      *luma_stride = STRIDE(STRIDE(width, 8) * 8 * 2, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 16) * 4 * 2, alignment);
      break;
    case ENC_PIXFMT_YUV420_UV_8BIT_TILE_64_2:  //VCENC_YUV420_UV_8BIT_TILE_64_2
      *luma_stride = STRIDE(STRIDE(width, 64) * 2, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 64) * 2, alignment);
      break;
    case ENC_PIXFMT_YUV420_UV_10BIT_TILE_128_2:  //VCENC_YUV420_UV_10BIT_TILE_128_2
      *luma_stride = STRIDE(STRIDE(width, 128) * 2 * 2, alignment);
      *chroma_stride = STRIDE(STRIDE(width, 128) * 2 * 2, alignment);
      pixelByte = 2;
      break;
    case ENC_PIXFMT_RGB888_24BIT:  //VCENC_RGB888_24BIT
    case ENC_PIXFMT_BGR888_24BIT:  //VCENC_BGR888_24BIT
    case ENC_PIXFMT_RBG888_24BIT:  //VCENC_RBG888_24BIT
    case ENC_PIXFMT_GBR888_24BIT:  //VCENC_GBR888_24BIT
    case ENC_PIXFMT_BRG888_24BIT:  //VCENC_BRG888_24BIT
    case ENC_PIXFMT_GRB888_24BIT:  //VCENC_GRB888_24BIT
      *luma_stride = STRIDE(width * 3, alignment);
      *chroma_stride = 0;
      break;
    case ENC_PIXFMT_Y8b:  //VCENC_Y8b
      *luma_stride = STRIDE(width, alignment);
      *chroma_stride = 0;
      break;
    case ENC_PIXFMT_Y10bWL:  //VCENC_Y10bWL
    case ENC_PIXFMT_Y10bWH:  //VCENC_Y10bWH
      *luma_stride = STRIDE(width * 2, alignment);
      *chroma_stride = 0;
      pixelByte = 2;
      break;
    case ENC_PIXFMT_YUV420SP10b:  //VCENC_YUV420SP10b
      *luma_stride = STRIDE(width, 64);
      *chroma_stride = STRIDE(width, 64);
      break;
    case ENC_PIXFMT_YUV422P10bWL:  //VCENC_YUV422P10bWL_Raster_A16N
      *luma_stride = STRIDE(width * 2, alignment);
      *chroma_stride = STRIDE(width / 2 * 2, alignment);
      pixelByte = 2;
      break;
    case ENC_PIXFMT_Y8b_TILE_8_8:  //VCENC_Y8b_TILE_8_8
     *luma_stride = STRIDE(STRIDE(width, 8) * 8, alignment);
     *chroma_stride = 0;
     break;
    case ENC_PIXFMT_Y10bWH_TILE_8_8:  //VCENC_Y10bWH_TILE_8_8
      *luma_stride = STRIDE(STRIDE(width, 8) * 8 * 2, alignment);
      *chroma_stride = 0;
      break;
    case ENC_PIXFMT_YUV400_PLANAR_10BIT_PACKED:  //VCENC_YUV400_PLANAR_10BIT_PACKED
      *luma_stride = STRIDE(width, 64);
      *chroma_stride = 0;
      break;
    default:
      *luma_stride = 0;
      *chroma_stride = 0;
      break;
  }

  return pixelByte;
}

/*------------------------------------------------------------------------------

  EncPreProcess

  Preform cropping

  Input asic  Pointer to asicData_s structure
    preProcess Pointer to preProcess_s structure.

------------------------------------------------------------------------------*/
void EncPreProcess(asicData_s *asic, preProcess_s *preProcess, void *ctx,
                   u32 tileId) {
  u32 tmp, i;
  u32 width, height;
  regValues_s *regs;
  u32 luma_stride, chroma_stride, pixelByte;
  u32 horOffsetSrc;
  u32 alignment;
  u32 height64;
  u32 client_type;

  ASSERT(asic != NULL && preProcess != NULL);

  horOffsetSrc = preProcess->horOffsetSrc[tileId];
  alignment = preProcess->input_alignment;

  regs = &asic->regs;

  pixelByte = EncGetAlignedByteStride(preProcess->lumWidthSrc[tileId],
                                      preProcess->inputFormat, &luma_stride,
                                      &chroma_stride, alignment, preProcess->scanType);

  if (preProcess->interlacedFrame) {
    luma_stride *= 2;
    chroma_stride *= 2;
  };

  luma_stride /= pixelByte;
  chroma_stride /= pixelByte;

  regs->input_luma_stride = luma_stride;
  regs->input_chroma_stride = chroma_stride;

  regs->pixelsOnRow = luma_stride;
  height64 = ((preProcess->lumHeight + 63) & ~63);
  regs->dummyReadEnable = 0;

  regs->scanType = preProcess->scanType;

  if (preProcess->scanType == VCENC_SUPERTILEX_SCAN)
  {
    if ((preProcess->inputFormat >= VCENC_YUV422_INTERLEAVED_YUYV && preProcess->inputFormat <= VCENC_BGR101010) ||
       (preProcess->inputFormat >= VCENC_YUV444_XYUV8888 &&
       preProcess->inputFormat <= VCENC_BGRX1010102) ||
       preProcess->inputFormat == VCENC_YUV422_INTERLEAVED_YVYU ||
       preProcess->inputFormat == VCENC_YUV422_INTERLEAVED_VYUY) /* YUYV422 / RGB / XYUV8888 / XYUV2101010 */
    {
        /* Input image position after crop and stabilization */
      tmp = (preProcess->verOffsetSrc[tileId] / 64) * luma_stride; // vertical tile offset //here luma_stride = pixel * 64
      tmp += (horOffsetSrc / 64) * 64 * 64; // hor tile offset
      tmp *= pixelByte; // 2/4 bytes per pixel.

      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;
      regs->supertileXOffsetX = horOffsetSrc % 64;
      regs->supertileXOffsetY = preProcess->verOffsetSrc[tileId] % 64;
      regs->input_luma_stride = luma_stride / 64; // input_luma_stride uses pixel unit.
    }
  } else {
    /* cropping */
    if (preProcess->inputFormat <= 2 ||
        preProcess->inputFormat ==
            VCENC_YVU420_PLANAR ||
        preProcess->inputFormat ==
            VCENC_YUV444_PLANAR) /* YUV 420 planar/semiplanar, YUV 444 planar */
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      if (preProcess->videoStab) regs->stabNextLumaBase += (tmp & (~15));

      /* Chroma */
      if (preProcess->inputFormat == VCENC_YUV444_PLANAR) {
        tmp = preProcess->verOffsetSrc[tileId];
        tmp *= chroma_stride;
        tmp += horOffsetSrc;
        regs->inputCbBase += (tmp & (~15));
        regs->inputCrBase += (tmp & (~15));
        regs->inputChromaBaseOffset = tmp & 15;
      }
      else if (preProcess->inputFormat == VCENC_YUV420_PLANAR ||
          preProcess->inputFormat == VCENC_YVU420_PLANAR) {
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= chroma_stride;
        tmp += horOffsetSrc / 2;
        if (((chroma_stride) % 16) == 0) {
          regs->inputCbBase += (tmp & (~15));
          regs->inputCrBase += (tmp & (~15));
          regs->inputChromaBaseOffset = tmp & 15;
        } else {
          regs->inputCbBase += (tmp & (~7));
          regs->inputCrBase += (tmp & (~7));
          regs->inputChromaBaseOffset = tmp & 7;
        }
        regs->dummyReadAddr = regs->inputCrBase + regs->inputChromaBaseOffset +
                              height64 / 2 * chroma_stride +
                              preProcess->lumWidth / 2 - 1;
      } else {
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= chroma_stride / 2;
        tmp += horOffsetSrc / 2;
        tmp *= 2;

        regs->inputCbBase += (tmp & (~15));
        regs->inputChromaBaseOffset = tmp & 15;
        regs->dummyReadAddr = regs->inputCbBase + regs->inputChromaBaseOffset +
                              height64 / 2 * chroma_stride +
                              preProcess->lumWidth - 1;
      }
    } else if (preProcess->inputFormat <= 10 ||
               preProcess->inputFormat == VCENC_YUV422_INTERLEAVED_YVYU ||
               preProcess->inputFormat == VCENC_YUV422_INTERLEAVED_VYUY) /* YUV 422 / RGB 16bpp */
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      regs->inputImageFormat = EncPreGetHwFormat(preProcess->inputFormat);
      if (regs->inputImageFormat == ASIC_INPUT_YUYV422INTERLEAVED ||
          regs->inputImageFormat == ASIC_INPUT_UYVY422INTERLEAVED)
        horOffsetSrc = (horOffsetSrc / 2) * 2;
      tmp += horOffsetSrc;
      tmp *= 2;

      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;
      regs->inputChromaBaseOffset = (regs->inputLumaBaseOffset / 4) * 4;
      regs->dummyReadAddr = regs->inputLumBase + regs->inputLumaBaseOffset +
                            height64 * luma_stride * 2 +
                            preProcess->lumWidth * 2 - 1;

      if (preProcess->videoStab) regs->stabNextLumaBase += (tmp & (~15));
    } else if (preProcess->inputFormat <= 14 ||
               (preProcess->inputFormat >= VCENC_YUV444_XYUV8888 &&
               preProcess->inputFormat <= VCENC_BGRX1010102)) /* RGB 32bpp / XYUV8888 / XYUV2101010 */
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;

      regs->inputImageFormat = EncPreGetHwFormat(preProcess->inputFormat);

      if (regs->inputImageFormat == ASIC_INPUT_YUYV422INTERLEAVED ||
          regs->inputImageFormat == ASIC_INPUT_UYVY422INTERLEAVED)
        horOffsetSrc = (horOffsetSrc / 2) * 2;
      tmp += horOffsetSrc;
      tmp *= 4;

      regs->inputLumBase += (tmp & (~15));
      /* Note: HW does the cropping AFTER RGB to YUYV conversion
       * so the offset is calculated using 16bpp */
      regs->inputLumaBaseOffset = (tmp & 15);
      regs->inputChromaBaseOffset = (regs->inputLumaBaseOffset / 4) * 4;
      regs->dummyReadAddr = regs->inputLumBase + regs->inputLumaBaseOffset +
                            height64 * luma_stride + preProcess->lumWidth * 4 - 1;

      if (preProcess->videoStab) regs->stabNextLumaBase += (tmp & (~15));
    } else if (preProcess->inputFormat == 15)  //I010
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc;
      tmp *= 2;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      {
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= chroma_stride;
        tmp += horOffsetSrc / 2;
        tmp *= 2;
        if (((chroma_stride * 2) % 16) == 0) {
          regs->inputCbBase += (tmp & (~15));
          regs->inputCrBase += (tmp & (~15));
          regs->inputChromaBaseOffset = tmp & 15;
        } else {
          regs->inputCbBase += (tmp & (~7));
          regs->inputCrBase += (tmp & (~7));
          regs->inputChromaBaseOffset = tmp & 7;
        }
      }
      regs->dummyReadAddr = regs->inputCrBase + regs->inputChromaBaseOffset +
                            height64 / 2 * chroma_stride * 2 +
                            preProcess->lumWidth - 1;
    } else if (preProcess->inputFormat == 59) // VCENC_YUV422P10bWL
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc;
      tmp *= 2;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      {
        tmp = preProcess->verOffsetSrc[tileId];
        tmp *= chroma_stride;
        tmp += horOffsetSrc / 2;
        tmp *= 2;
        if (((chroma_stride * 2) % 16) == 0) {
          regs->inputCbBase += (tmp & (~15));
          regs->inputCrBase += (tmp & (~15));
          regs->inputChromaBaseOffset = tmp & 15;
        } else {
          regs->inputCbBase += (tmp & (~7));
          regs->inputCrBase += (tmp & (~7));
          regs->inputChromaBaseOffset = tmp & 7;
        }
      }
    } else if (preProcess->inputFormat == 55 || //Y8b
               preProcess->inputFormat == 56 ||  //Y10bWL
               preProcess->inputFormat == 57)  //Y10bWH
    {
        /* Input image position after crop and stabilization */
        tmp = preProcess->verOffsetSrc[tileId];
        tmp *= luma_stride;
        tmp += horOffsetSrc;
        if (preProcess->inputFormat == 56 || preProcess->inputFormat == 57)
          tmp *= 2;
        regs->inputLumBase += (tmp & (~15));
        regs->inputLumaBaseOffset = tmp & 15;

        /* Chroma */
        regs->dummyReadAddr = regs->inputCbBase;
    } else if (preProcess->inputFormat == 16 || preProcess->inputFormat == 54)  //P010
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc;
      tmp *= 2;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      {
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= chroma_stride / 2;
        tmp += horOffsetSrc / 2;
        tmp *= 2 * 2;

        if (((luma_stride) % 16) == 0) {
          regs->inputCbBase += (tmp & (~15));
          regs->inputChromaBaseOffset = tmp & 15;
        } else {
          regs->inputCbBase += (tmp & (~7));
          regs->inputChromaBaseOffset = tmp & 7;
        }
      }
      regs->dummyReadAddr =
          regs->inputCbBase + regs->inputChromaBaseOffset +
          (height64 / 2 * luma_stride / 2 + preProcess->lumWidth / 2) * 4 - 1;
    } else if (preProcess->inputFormat == 17) {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride * 10 / 8;
      tmp += horOffsetSrc * 10 / 8;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      {
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= luma_stride * 5 / 8;
        tmp += horOffsetSrc * 5 / 8;
        if (((luma_stride * 5 / 8) % 16) == 0) {
          regs->inputCbBase += (tmp & (~15));
          regs->inputCrBase += (tmp & (~15));
          regs->inputChromaBaseOffset = tmp & 15;
        } else {
          regs->inputCbBase += (tmp & (~7));
          regs->inputCrBase += (tmp & (~7));
          regs->inputChromaBaseOffset = tmp & 7;
        }
      }
      regs->dummyReadAddr = regs->inputCrBase + regs->inputChromaBaseOffset +
                            height64 / 2 * luma_stride * 5 / 8 +
                            (preProcess->lumWidth) * 5 / 8 - 1;
      } else if (preProcess->inputFormat == 58) { //YUV420SP10b
        /* Input image position after crop and stabilization */
        tmp = preProcess->verOffsetSrc[tileId];
        tmp *= luma_stride * 10 / 8;
        tmp += horOffsetSrc * 10 / 8;
        regs->inputLumBase += (tmp & (~15));
        regs->inputLumaBaseOffset = tmp & 15;

        /* Chroma */
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= chroma_stride * 10 / 8;
        tmp += horOffsetSrc * 10 / 8;
        regs->inputCbBase += (tmp & (~15));
        regs->inputChromaBaseOffset = tmp & 15;
    } else if (preProcess->inputFormat == 18) {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId] / 2;
      tmp *= luma_stride / 2;
      tmp *= 8;
      tmp += horOffsetSrc / 2 * 8;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;
    } else if (preProcess->inputFormat ==
               19) /* VCENC_YUV420_PLANAR_8BIT_TILE_32_32 */
    {
      /* Input image position after crop and stabilization */
      /*crop alignment for 32x32*/
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc * 32;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      tmp = preProcess->verOffsetSrc[tileId] / 2;
      tmp *= luma_stride / 2;
      tmp += (horOffsetSrc / 2) * 16;
      tmp *= 2;

      regs->inputCbBase += (tmp & (~15));
      regs->inputChromaBaseOffset = tmp & 15;
    } else if (preProcess->inputFormat ==
               20) /* VCENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4 */
    {
      /* Input image position after crop and stabilization */
      /*crop alignment for 32x32*/
      u32 mb_per_row = preProcess->lumWidthSrc[tileId] / 16;
      u32 mb_cropped =
          mb_per_row * preProcess->verOffsetSrc[tileId] / 16 + horOffsetSrc / 16;

      tmp = mb_cropped / 5 * 2048 + mb_cropped % 5 * 400;

      if (preProcess->sliced_frame)  //jpeg
      {
        u32 slice_cropped = mb_per_row * preProcess->verOffsetSrc[tileId] / 16;
        u32 slice_cropped_size =
            slice_cropped / 5 * 2048 + slice_cropped % 5 * 400;
        tmp = tmp - slice_cropped_size;
      }

      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;
      regs->inputCbBase = horOffsetSrc / 16;
      regs->inputCrBase = preProcess->verOffsetSrc[tileId] / 16;

      if (preProcess->sliced_frame)  //jpeg
      {
        preProcess->verOffsetSrc[tileId] = 0;
      }
    } else if (preProcess->inputFormat == 21 || preProcess->inputFormat == 22 ||
               preProcess->inputFormat == 23) {
      regs->dummyReadAddr =
          regs->inputCbBase +
          (preProcess->verOffsetSrc[tileId] + height64) / 8 * luma_stride +
          ((preProcess->horOffsetSrc[tileId] + preProcess->lumWidth) * 4 - 1) *
              (1 + (preProcess->inputFormat == 23));
    } else if (preProcess->inputFormat == 24) /* P101010*/
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc / 3 * 4;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      tmp = preProcess->verOffsetSrc[tileId] / 2;
      tmp *= chroma_stride;
      tmp += horOffsetSrc / 3 * 4;
      regs->inputCbBase += (tmp & (~15));
      regs->inputChromaBaseOffset = tmp & 15;
    } else if (preProcess->inputFormat == 25) /* 422_888*/
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      tmp += horOffsetSrc;
      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = tmp & 15;

      /* Chroma */
      regs->inputCbBase += (tmp & (~15));
      regs->inputChromaBaseOffset = tmp & 15;
    } else if (preProcess->inputFormat >= 26 &&
               preProcess->inputFormat <= 34) /* 420_tile*/
    {
      regs->input_luma_stride = luma_stride;
      regs->input_chroma_stride = chroma_stride;
    } else if (preProcess->inputFormat == VCENC_YUV420_8BIT_TILE_8_8 ||
               preProcess->inputFormat == VCENC_YVU420_8BIT_TILE_8_8 ||
               preProcess->inputFormat == VCENC_YUV420_10BIT_TILE_8_8) {
      if (preProcess->verOffsetSrc[tileId] % 8 != 0 ||
          preProcess->horOffsetSrc[tileId] % 8 != 0) {
        printf("cropping need to be aligned to 8x8 tile\n");
        ASSERT (0);
      }
      else {
        bool is2bpp = preProcess->inputFormat == VCENC_YUV420_10BIT_TILE_8_8;
        tmp = preProcess->verOffsetSrc[tileId] / 8;
        tmp *= luma_stride;
        tmp += horOffsetSrc / 8 * 64 * (is2bpp ? 2 : 1);

        regs->inputLumBase += (tmp & (~15));
        regs->inputLumaBaseOffset = tmp & 15;

        /* Chroma */
        tmp = preProcess->verOffsetSrc[tileId] / 2 / 4;
        tmp *= chroma_stride;
        tmp += horOffsetSrc / 2 / 2 * 16;
        regs->inputCbBase += (tmp & (~15));
        regs->inputChromaBaseOffset = tmp & 15;
      }
    } else if (preProcess->inputFormat == 38) /* VCENC_YUV420_UV_8BIT_TILE_64_2 */
    {
      if (preProcess->verOffsetSrc[tileId] % 4 != 0 ||
          preProcess->horOffsetSrc[tileId] % 64 != 0) {
        printf("FBC cropping need to be aligned to 64x4 tile \n");
        ASSERT (0);
      }
      else {
        tmp = preProcess->verOffsetSrc[tileId] / 2;
        tmp *= luma_stride;
        tmp += horOffsetSrc * 2;

        regs->inputLumBase += (tmp & (~15));
        regs->inputLumaBaseOffset = tmp & 15;

        /* Chroma */
        tmp = preProcess->verOffsetSrc[tileId] / 4;
        tmp *= chroma_stride;
        tmp += horOffsetSrc * 2;
        regs->inputCbBase += (tmp & (~15));
        regs->inputChromaBaseOffset = tmp & 15;
      }
    } else if ((preProcess->inputFormat >= 40) &&
               (preProcess->inputFormat <=
                45)) /* RGB 24bpp , VCENC_RGB888_24BIT ~ VCENC_GRB888_24BIT*/
    {
      /* Input image position after crop and stabilization */
      tmp = preProcess->verOffsetSrc[tileId];
      tmp *= luma_stride;
      regs->inputImageFormat = EncPreGetHwFormat(preProcess->inputFormat);
      tmp += horOffsetSrc * 3;

      regs->inputLumBase += (tmp & (~15));
      regs->inputLumaBaseOffset = (tmp & 15);
      regs->inputChromaBaseOffset = (regs->inputLumaBaseOffset / 4) * 4;
      regs->dummyReadAddr = regs->inputLumBase + regs->inputLumaBaseOffset +
                            height64 * luma_stride + preProcess->lumWidth * 3 - 1;

      if (preProcess->videoStab) regs->stabNextLumaBase += (tmp & (~15));
    } else if (preProcess->inputFormat == VCENC_Y8b_TILE_8_8 ||
               preProcess->inputFormat == VCENC_Y10bWH_TILE_8_8) /* 400_tile*/
    {
      if (preProcess->verOffsetSrc[tileId] % 8 != 0 ||
        preProcess->horOffsetSrc[tileId] % 8 != 0) {
        printf("cropping need to be aligned to 8x8 tile \n");
        ASSERT (0);
      }
      else {
        bool is2bpp = preProcess->inputFormat == VCENC_Y10bWH_TILE_8_8;
        tmp = preProcess->verOffsetSrc[tileId] / 8;
        tmp *= luma_stride;
        tmp += horOffsetSrc / 8 * 64 * (is2bpp ? 2 : 1);

        regs->inputLumBase += (tmp & (~15));
        regs->inputLumaBaseOffset = tmp & 15;
      }
    }
  }
  if (!regs->dummyReadEnable) regs->dummyReadAddr = 0;

  /* YUV subsampling, map API values into HW reg values */
  regs->inputImageFormat = EncPreGetHwFormat(preProcess->inputFormat);

  if (preProcess->inputFormat == 2 || preProcess->inputFormat == 22 ||
      preProcess->inputFormat == 26 || preProcess->inputFormat == 30 ||
      preProcess->inputFormat == 31 || preProcess->inputFormat == 34 ||
      preProcess->inputFormat == 46 || preProcess->inputFormat == 54 ||
      preProcess->inputFormat == 62 || preProcess->inputFormat == 63 ||
      preProcess->inputFormat == 64)
    regs->chromaSwap = 1;

  regs->inputImageRotation = preProcess->rotation;
  regs->inputImageMirror = preProcess->mirror;

  /* source image setup, size and fill */
  width = preProcess->lumWidth;
  height = preProcess->lumHeight;
  regs->scaledVertivalWeightEn = 1;
  regs->scaledHorizontalCopy = 0;
  regs->scaledVerticalCopy = 0;
  client_type = EncAsicGetClientType(preProcess->codecFormat);
  const EWLHwConfig_t *config = EncAsicGetAsicConfig(client_type, ctx);
  if (!config) {
    printf("Cannot Get Valid Configure!\n");
    return;
  }
  /* Scaling ratio for down-scaling, fixed point 1.16, calculate from
      rotated dimensions. */
  if (preProcess->scaledWidth * preProcess->scaledHeight > 0 &&
      preProcess->scaledOutput) {
    i32 width8 = 0;
    i32 height8 = 0;

    u32 width = preProcess->lumWidth * (preProcess->inLoopDSRatio + 1);
    u32 height = preProcess->lumHeight * (preProcess->inLoopDSRatio + 1);

    if ((regs->codingType == ASIC_JPEG) ||
        (regs->codingType == ASIC_H264))  //aligned to 16
    {
      width8 = (width + 15) / 16 * 16;
      height8 = (height + 15) / 16 * 16;
    } else {
      width8 = (width + 7) / 8 * 8;
      height8 = (height + 7) / 8 * 8;
    }

    regs->scaledOutputFormat = preProcess->scaledOutputFormat;
    regs->scaledWidth = preProcess->scaledWidth;
    regs->scaledHeight = preProcess->scaledHeight;
    regs->scaledSkipLeftPixelColumn = 0;
    regs->scaledSkipTopPixelRow = 0;
    regs->scaledWidthRatio = (u32)(preProcess->scaledWidth << 16) / width8 + 1;
    regs->scaledHeightRatio =
        (u32)(preProcess->scaledHeight << 16) / height8 + 1;
    regs->scaledHeightRatio = MIN(65535, regs->scaledHeightRatio);
    regs->scaledWidthRatio = MIN(65535, regs->scaledWidthRatio);

    if (config->scaled420Support == 1) {
      if (width8 == preProcess->scaledWidth) {
        regs->scaledHorizontalCopy = 1;
      }
      if (height8 == preProcess->scaledHeight) {
        regs->scaledVerticalCopy = 1;
      }
    } else {
      if (width == preProcess->scaledWidth) {
        regs->scaledHorizontalCopy = 1;
      }
      if (height == preProcess->scaledHeight) {
        regs->scaledVerticalCopy = 1;
      }

      if (preProcess->rotation == 2) {
        regs->scaledSkipTopPixelRow =
            (((height8 * regs->scaledHeightRatio) >> 16) -
             preProcess->scaledHeight) /
            2;

        if (preProcess->scaledHeight == height) {
          regs->scaledSkipTopPixelRow = (8 - (height & 0x07)) / 2;
          if (!(height & 0x07)) regs->scaledSkipTopPixelRow = 0;
        }
      }
      if (preProcess->rotation == 1) {
        regs->scaledSkipLeftPixelColumn =
            (((width8 * regs->scaledWidthRatio) >> 16) -
             preProcess->scaledWidth) /
            2;
        if (preProcess->scaledWidth == width) {
          regs->scaledSkipLeftPixelColumn = (8 - (width & 0x07)) / 2;
          if (!(width & 0x07)) regs->scaledSkipLeftPixelColumn = 0;
        }
      }

      if (preProcess->rotation == 3) {
        regs->scaledSkipLeftPixelColumn =
            (((width8 * regs->scaledWidthRatio) >> 16) -
             preProcess->scaledWidth) /
            2;
        if (preProcess->scaledWidth == width) {
          regs->scaledSkipLeftPixelColumn = (8 - (width & 0x07)) / 2;
          if (!(width & 0x07)) regs->scaledSkipLeftPixelColumn = 0;
        }
        regs->scaledSkipTopPixelRow =
            (((height8 * regs->scaledHeightRatio) >> 16) -
             preProcess->scaledHeight) /
            2;

        if (preProcess->scaledHeight == height) {
          regs->scaledSkipTopPixelRow = (8 - (height & 0x07)) / 2;
          if (!(height & 0x07)) regs->scaledSkipTopPixelRow = 0;
        }
      }
    }
  } else {
    regs->scaledWidth = regs->scaledHeight = 0;
    regs->scaledWidthRatio = regs->scaledHeightRatio = 0;
  }

  /* For rotated image, swap dimensions back to normal. */
  if (preProcess->rotation && preProcess->rotation != 3) {
    u32 tmp;

    tmp = width;
    width = height;
    height = tmp;
  }

  /* Set mandatory input parameters in asic structure */
  //regs->picWidth = (width + 7) / 8;
  //regs->picHeight = (height + 7) / 8;
  //asic->regs.picWidth = pic->sps->width_min_cbs;
  //asic->regs.picHeight = pic->sps->height_min_cbs;
  u32 mcuh = (regs->codingType == ASIC_JPEG && regs->jpegMode == 1 &&
                      regs->ljpegFmt == 1
                  ? 8
                  : 16);
  regs->mbsInRow = (width + 15) / 16;
  regs->mbsInCol = (height + mcuh - 1) / mcuh;

  /* Set the overfill values */
  if (width & 0x07)
    regs->xFill = (8 - (width & 0x07)) / 2;
  else
    regs->xFill = 0;

  if (height & 0x07)
    regs->yFill = 8 - (height & 0x07);
  else
    regs->yFill = 0;

  if ((regs->codingType == ASIC_JPEG) || (regs->codingType == ASIC_H264)) {
    if (width & 0x0f) {
      //TODO: H1 prp is doing /4 crop for both JPEG/H.264
      if (regs->codingType == ASIC_JPEG)
        regs->xFill = (16 - (width & 0x0f)) / 2;
      else if (regs->codingType == ASIC_H264)
        regs->xFill = (16 - (width & 0x0f)) / 2;
    } else
      regs->xFill = 0;
  }

  if ((regs->codingType == ASIC_JPEG &&
       !(regs->jpegMode == 1 && regs->ljpegFmt == 1)) ||
      (regs->codingType == ASIC_H264)) {
    if (height & 0x0f)
      regs->yFill = (16 - (height & 0x0f));
    else
      regs->yFill = 0;
  }

  if (preProcess->inputFormat == 20 || preProcess->inputFormat == 19) {
    regs->xFill = 0;
    regs->yFill = 0;
  }

  if (preProcess->inputFormat == VCENC_Y8b ||
      preProcess->inputFormat == VCENC_Y10bWL ||
      preProcess->inputFormat == VCENC_Y10bWH ||
      preProcess->inputFormat == VCENC_Y8b_TILE_8_8 ||
      preProcess->inputFormat == VCENC_Y10bWH_TILE_8_8 ) {
    u32 chroma_value = (regs->outputBitWidthLuma == 2)? (512) : (128);
    regs->constChromaEn = 1;
    regs->constCb = chroma_value;
    regs->constCr = chroma_value;
  } else {
    regs->constChromaEn = preProcess->constChromaEn;
    regs->constCb = preProcess->constCb;
    regs->constCr = preProcess->constCr;
  }
  /* video stabilization */
  if (regs->codingType != ASIC_JPEG && preProcess->videoStab != 0)
    regs->stabMode = 2; /* stab + encode */
  else
    regs->stabMode = 0;

  //Perform OSD cropping
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (preProcess->overlayEnable[i]) {
      switch (preProcess->overlayFormat[i]) {
        case 0:  //ARGB
          if (preProcess->overlaySuperTile[0]) {
            /* For super tile, offset has to be 64 aligned */
            tmp = STRIDE(preProcess->overlayVerOffset[i], 64) / 64;
            tmp *= preProcess->overlayYStride[i];
            tmp += ((preProcess->overlayCropXoffset[i] / 64) * (64 * 64) +
                    preProcess->overlayCropXoffset[i] % 64) *
                   4;
          } else {
            tmp = preProcess->overlayVerOffset[i];
            tmp *= preProcess->overlayYStride[i];
            tmp += preProcess->overlayCropXoffset[i] * 4;
          }
          preProcess->overlayInputYAddr[i] += tmp;
          preProcess->overlayWidth[i] = preProcess->overlayCropWidth[i];
          preProcess->overlayHeight[i] = preProcess->overlaySliceHeight[i];
          break;
        case 1:  //NV12
          //Luma
          tmp = preProcess->overlayVerOffset[i];
          tmp *= preProcess->overlayYStride[i];
          tmp += preProcess->overlayCropXoffset[i];
          preProcess->overlayInputYAddr[i] += tmp;
          preProcess->overlayWidth[i] = preProcess->overlayCropWidth[i];
          preProcess->overlayHeight[i] = preProcess->overlaySliceHeight[i];

          //Chroma
          tmp = preProcess->overlayVerOffset[i] / 2;
          tmp *= preProcess->overlayUVStride[i];
          tmp += preProcess->overlayCropXoffset[i];
          preProcess->overlayInputUAddr[i] += tmp;
          break;
        case 2:  //Bitmap
          tmp = preProcess->overlayVerOffset[i];
          tmp *= preProcess->overlayYStride[i];
          tmp += preProcess->overlayCropXoffset[i] / 8;
          preProcess->overlayInputYAddr[i] += tmp;
          preProcess->overlayWidth[i] = preProcess->overlayCropWidth[i];
          preProcess->overlayHeight[i] = preProcess->overlaySliceHeight[i];
          break;
        default:
          break;
      }
    }
  }

#ifdef TRACE_PREPROCESS
  EncTracePreProcess(preProcess);
#endif

  return;
}

/*------------------------------------------------------------------------------

  EncSetColorConversion

  Set color conversion coefficients and RGB input mask

  Input asic  Pointer to asicData_s structure
    preProcess Pointer to preProcess_s structure.

------------------------------------------------------------------------------*/
void EncSetColorConversion(preProcess_s *preProcess, asicData_s *asic) {
  regValues_s *regs;

  ASSERT(asic != NULL && preProcess != NULL);

  regs = &asic->regs;
  regs->colorConversionLumaOffset = 0;

  /* Y  = A R + B G + C B + offset
       * Cb = E B - G Y + 128
       * Cr = F R - H Y + 128
       */
  switch (preProcess->colorConversionType) {
    case 0: /* BT.601 */
    default:
      /* Y  = 0.2989 R + 0.5866 G + 0.1145 B
       * Cb = 0.5647 B - 0.5647 Y + 128
       * Cr = 0.7132 R - 0.7132 Y + 128
       */
      preProcess->colorConversionType = 0;
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA = 19589;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB = 38443;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC = 7504;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE = 37008;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffF = 46740;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffG = 37008;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH = 46740;
      break;

    case 1: /* BT.709 */
      /* Y  = 0.2126 R + 0.7152 G + 0.0722 B
       * Cb = 0.5389 B - 0.5389 Y + 128
       * Cr = 0.6350 R - 0.6350 Y + 128
       */
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA = 13933;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB = 46871;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC = 4732;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE = 35317;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffF = 41615;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffG = 35317;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH = 41615;
      break;

    case 2: /* User defined */
      /* Limitations for coefficients: A+B+C <= 65536 */
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffF;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffG;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH;
      regs->colorConversionLumaOffset = preProcess->colorConversionLumaOffset;
      break;

    case 3:  // BT.2020
        // 10bits
      /* Y  = 0.2627 R + 0.678 G + 0.0593 B
         * Cb = 0.531519 B - 0.531519 Y  + 512
         * Cr = 0.67815 R - 0.67815 Y   + 512
        */
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA = 17216;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB = 44433;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC = 3886;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE = 34834;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffF = 44443;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffG = 34834;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH = 44443;
      break;

    case 4: /* BT.601 of full range[0,255]*/
      /* Y' = 0.257 R + 0.504 G + 0.098 B
             * Y  = Y' + 16
             * Cb = 0.495 B - 0.576 Y' + 128
             * Cr = 0.627 R - 0.73Y' + 128
             */
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA = 16843;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB = 33030;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC = 6423;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE = 32440;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffG = 41091;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffF = 37749;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH = 47841;
      regs->colorConversionLumaOffset = 16;
      break;

    case 5: /* BT.601 of limited range[0,219]*/
      /* Y' = 0.299 R + 0.587 G + 0.114 B
             * Y  = Y' + 16
             * Cb = 0.579 B - 0.579 Y' + 128
             * Cr = 0.728 R - 0.728 Y' + 128
             */
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA = 19595;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB = 38470;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC = 7471;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE = 37945;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffF = 47710;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffG = 37945;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH = 47710;
      regs->colorConversionLumaOffset = 16;
      break;

    case 6: /* BT.709 of full range[0,255]*/
      /* Y' = 0.1826 R + 0.6142 G + 0.062 B
             * Y  = Y' + 16
             * Cb = 0.4732 B - 0.5509 Y' + 128
             * Cr = 0.5579 R - 0.6495 Y' + 128
             */
      regs->colorConversionCoeffA = preProcess->colorConversionCoeffA = 11967;
      regs->colorConversionCoeffB = preProcess->colorConversionCoeffB = 40252;
      regs->colorConversionCoeffC = preProcess->colorConversionCoeffC = 4063;
      regs->colorConversionCoeffE = preProcess->colorConversionCoeffE = 31012;
      regs->colorConversionCoeffF = preProcess->colorConversionCoeffG = 36563;
      regs->colorConversionCoeffG = preProcess->colorConversionCoeffF = 36104;
      regs->colorConversionCoeffH = preProcess->colorConversionCoeffH = 42566;
      regs->colorConversionLumaOffset = 16;
      break;
  }

  /* Setup masks to separate R, G and B from RGB */
  if(preProcess->inputFormat < 55) {
    regs->rMaskMsb = rgbMaskBits[preProcess->inputFormat][0];
    regs->gMaskMsb = rgbMaskBits[preProcess->inputFormat][1];
    regs->bMaskMsb = rgbMaskBits[preProcess->inputFormat][2];
  }
}

