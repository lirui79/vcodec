/*------------------------------------------------------------------------------
--       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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

#ifndef _ENC_IMAGE_PRIV_H_
#define _ENC_IMAGE_PRIV_H_

#include <stdint.h>
#include "hevcencapi.h"
#include "enccfg.h"
#include "encoder.h"

#ifdef __cplusplus
extern  "C" {
#endif

#define USER_DEFINED_QTABLE 10

typedef struct {
  i32 width;
  i32 height;
  i32 lumWidthSrc;
  i32 lumHeightSrc;
  i32 horOffsetSrc;
  i32 verOffsetSrc;
  i32 restartInterval;
  JpegEncFrameType frameType;
  JpegEncColorConversionType colorConversion;
  JpegEncPictureRotation rotation;
  i32 partialCoding;
  JpegEncCodingMode codingMode;
  i32 markerType;
  i32 qLevel;
  i32 quality;
  i32 nonRoiLevel;
  i32 unitsType;
  i32 xdensity;
  i32 ydensity;
  i32 thumbnail;
  i32 widthThumb;
  i32 heightThumb;
  i32 comLength;
  const u8 *pCom;
  i32 inputLineBufMode;
  i32 inputLineBufDepth;
  u32 amountPerLoopBack;
  i32 segmentUnitHeight;
  u32 hashtype;
  i32 mirror;
  i32 constChromaEn;
  u32 constCb;
  u32 constCr;
  i32 predictMode;
  i32 ptransValue;
  u32 bitPerSecond;
  u32 frameRateNum;
  u32 frameRateDenom;
  i32 rcMode;
  i32 picQpDeltaMin;
  i32 picQpDeltaMax;
  u32 qpmin;
  u32 qpmax;
  i32 fixedQP;
  u32 exp_of_input_alignment;
  u32 streamMultiSegmentMode;
  u32 streamMultiSegmentAmount;
  u32 streamMultiSegmentSize;
  u32 AXIAlignment;
  u32 irqTypeMask;
  u32 overlayEnables;
  u32 mosaicEnables;
  u32 mosXoffset[MAX_MOSAIC_NUM];
  u32 mosYoffset[MAX_MOSAIC_NUM];
  u32 mosWidth[MAX_MOSAIC_NUM];
  u32 mosHeight[MAX_MOSAIC_NUM];
  u32 burstMaxLength;
  u32 sbi_id_0;
  u32 sbi_id_1;
  u32 sbi_id_2;
  /* SRAM power down mode disable */
  u32 sramPowerdownDisable;
  u32 sramPowerdownMode;
  u32 sramPowerdownTimerDiv32;
  u32 coreMask;
} VCE_JPEG_OPTIONS;

typedef struct ENCODER_JPEG
{
  ENCODER_PROTOTYPE base;

  JpegEncInst instance;
  JpegEncIn encIn;
  OMX_BOOL frameHeader;
  OMX_BOOL leftoverCrop;
  OMX_BOOL sliceMode;
  OMX_U32 sliceHeight;
  OMX_U32 sliceNumber;
  OMX_COLOR_FORMATTYPE omxFrameType;

  VCE_JPEG_OPTIONS options;
  const void *ewlInst;
  const EWLHwConfig_t *hwCfg; /* HW fuse */
} ENCODER_JPEG;

#ifdef __cplusplus
}
#endif
#endif /* _ENC_IMAGE_PRIV_H_ */


