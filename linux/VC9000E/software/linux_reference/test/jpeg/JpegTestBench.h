/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Jpeg Encoder testbench
--
------------------------------------------------------------------------------*/
#ifndef _JPEGTESTBENCH_H_
#define _JPEGTESTBENCH_H_

#include "base_type.h"
#include "enccommon.h"

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* Maximum lenght of the file path */
#ifndef MAX_PATH
#define MAX_PATH 512
#endif

#define DEFAULT -255

/* Structure for command line options */
typedef struct {
  char input[MAX_PATH];
  char output[MAX_PATH];
  char inputThumb[MAX_PATH];
  char *roimapFile;
  char *nonRoiFilter;
  i32 firstPic;
  i32 lastPic;
  i32 width;
  i32 height;
  i32 lumWidthSrc;
  i32 lumHeightSrc;
  i32 horOffsetSrc;
  i32 verOffsetSrc;
  i32 restartInterval;
  i32 frameType;
  i32 colorConversion;
  i32 rotation;
  i32 partialCoding;
  i32 codingMode;
  i32 markerType;
  i32 qLevel;
  i32 quality;
  i32 nonRoiLevel;
  char qTablePath[MAX_PATH];
  i32 unitsType;
  i32 xdensity;
  i32 ydensity;
  i32 thumbnail;
  i32 widthThumb;
  i32 heightThumb;
  i32 lumWidthSrcThumb;
  i32 lumHeightSrcThumb;
  i32 horOffsetSrcThumb;
  i32 verOffsetSrcThumb;
  i32 writeOut;
  i32 comLength;
  char com[MAX_PATH];
  i32 inputLineBufMode;
  i32 inputLineBufDepth;
  u32 amountPerLoopBack;
  i32 segmentUnitHeight;
  u32 hashtype;
  i32 mirror;
  i32 formatCustomizedType;
  i32 constChromaEn;
  u32 constCb;
  u32 constCr;
  i32 losslessEnable;
  i32 predictMode;
  i32 ptransValue;
  u32 bitPerSecond;
  u32 mjpeg;
  u32 frameRateNum;
  u32 frameRateDenom;
  i32 rcMode;
  i32 picQpDeltaMin;
  i32 picQpDeltaMax;
  u32 qpmin;
  u32 qpmax;
  i32 qpHdr;
  i32 fixedQP;
  u32 exp_of_input_alignment;
  u32 streamBufChain;
  u32 streamMultiSegmentMode;
  u32 streamMultiSegmentSize;
  u32 streamMultiSegmentAmount;
  char dec400CompTableinput[MAX_PATH];
  u32 dec400FrameTableSize;
  u32 scanType;

  /* AXI alignment */
  u32 AXIAlignment;

  /* irq Type mask */
  u32 irqTypeMask;

  char olInput[MAX_OVERLAY_NUM][MAX_PATH];
  u32 overlayEnables;
  u32 olFormat[MAX_OVERLAY_NUM];
  u32 olAlpha[MAX_OVERLAY_NUM];
  u32 olWidth[MAX_OVERLAY_NUM];
  u32 olCropWidth[MAX_OVERLAY_NUM];
  u32 olHeight[MAX_OVERLAY_NUM];
  u32 olCropHeight[MAX_OVERLAY_NUM];
  u32 olXoffset[MAX_OVERLAY_NUM];
  u32 olCropXoffset[MAX_OVERLAY_NUM];
  u32 olYoffset[MAX_OVERLAY_NUM];
  u32 olCropYoffset[MAX_OVERLAY_NUM];
  u32 olYStride[MAX_OVERLAY_NUM];
  u32 olUVStride[MAX_OVERLAY_NUM];
  u32 olBitmapY[MAX_OVERLAY_NUM];
  u32 olBitmapU[MAX_OVERLAY_NUM];
  u32 olBitmapV[MAX_OVERLAY_NUM];
  u32 olSuperTile[MAX_OVERLAY_NUM];
  u32 olScaleWidth[MAX_OVERLAY_NUM];
  u32 olScaleHeight[MAX_OVERLAY_NUM];
  char osdDec400CompTableInput[MAX_OVERLAY_NUM][MAX_PATH];

  //Mosaic area
  u32 mosaicEnables;
  u32 mosXoffset[MAX_MOSAIC_NUM];
  u32 mosYoffset[MAX_MOSAIC_NUM];
  u32 mosWidth[MAX_MOSAIC_NUM];
  u32 mosHeight[MAX_MOSAIC_NUM];
  u32 mosSizeIndex;

  /* OSD_MAP */
  u32 osdMapEnable;
  char osdMapInput[MAX_PATH];
  u32 osdMapStride;
  u32 osdMapBlockSize;
  u32 osdMapAlpha[MAX_OSDMAP_COLOR_NUM];
  u32 osdMapY[MAX_OSDMAP_COLOR_NUM];
  u32 osdMapU[MAX_OSDMAP_COLOR_NUM];
  u32 osdMapV[MAX_OSDMAP_COLOR_NUM];

  /* SRAM power down mode disable */
  u32 sramPowerdownDisable;
  u32 sramPowerdownMode;
  u32 sramPowerdownTimerDiv32;
  /* Whether to enable VCMD, only valid for cmodel */
  i32 useVcmd;
  i32 useDec400;
  i32 useL2Cache;
  i32 useAXIFE;

  /*AXI max burst length */
  u32 burstMaxLength;

  /* logmsg env parameter */
  u32 logOutDir;
  u32 logOutLevel;
  u32 logTraceMap;
  u32 dumpRegister;
  /* UFBC enable */
  u32 ufbcMode;
  u32 ufbcYuvTrans;
  u32 ufbcBlockType;
  u32 ufbcBlockSplit;
  char *ufbcConstantVal;
  u32 secure_mode;
  /* enable poll sliceinfo */
  u32 inputSliceInfoEn;
  u32 priority;
  u32 core_mask;
  char *encDevice;
  char *memDevice;
#ifdef USE_LIBVA
  const void *vaDriverName;
#endif
  /* low latency gating */
  u32 lowlatGatingDisable;
  u32 dec400TileSize;
  u32 dec400TileMode;
  u32 dec400DataAlignment;
  u32 dec400RGBFormat;
  u32 dec400RGBAX;
  u32 dec400TSHeaderEnable;
} commandLine_s;

typedef struct {
  u32 streamRDCounter;
  u32 streamMultiSegEn;
  u32 streamMultiSegOffset;
  u8 *streamBase;
  u32 segmentSize;
  u32 segmentAmount;
  FILE *outStreamFile;
} SegmentCtl_s;

#endif
