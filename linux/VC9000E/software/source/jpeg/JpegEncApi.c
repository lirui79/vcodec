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
--  Abstract  :  JPEG Encoder API
--
------------------------------------------------------------------------------*/

#define JPEGENC_MAX_SIZE 4194304 /* 2048x2048 = 4194304 macroblocks */
#define JPEGENC_IMAGE_MAX_HEIGHT 32768

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "jpegencapi.h"
#include "vce_version.h"

#include "EncJpegInit.h"
#include "EncJpegInstance.h"
#include "EncJpegCodeFrame.h"
#include "EncJpegQuantTables.h"
#include "EncJpegMarkers.h"

#include "EncJpegPutBits.h"
#include "encasiccontroller.h"
#include "ewl.h"
#include "rate_control_jpeg.h"
#include "enccommon.h"
#include "encdec400.h"
#include "axife.h"
#include "enc_log.h"
#include "encufbc.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

static bool_e CheckJpegCfg(const JpegEncCfg *pEncCfg, void *ctx);
static i32 CheckThumbnailCfg(const JpegEncThumb *pCfgThumb);
static i32 CheckFullSize(const JpegEncCfg *pCfgFull);
static void JpegEncCfgAxiFe(jpegInstance_s *instance);
/*******************************************************************************
 Function name : JpegEncGetApiVersion
 Description   :
 Return type   : JpegEncApiVersion
*******************************************************************************/
JpegEncApiVersion JpegEncGetApiVersion(void) {
  JpegEncApiVersion ver;

  ver.major = VCESW_VERSION_MAJOR;
  ver.minor = VCESW_VERSION_MINOR;
  ver.micro = VCESW_VERSION_MICRO;
  APITRACE_INFO(NULL, "JpegEncGetVersion# OK\n");
  return ver;
}

/*******************************************************************************
 Function name : JpegEncGetBuild
 Description   :
 Return type   : JpegEncBuild
*******************************************************************************/
JpegEncBuild JpegEncGetBuild(u32 core_id, const void *ctx) {
  JpegEncBuild ver;

  ver.swBuild = VCENC_BUILD_CLNUM;
  ver.hwBuild = EWLReadAsicID(core_id, ctx);
  APITRACE_INFO(ctx, "JpegEncGetBuild# OK\n");
  return (ver);
}

static i32 InitialJpegQp(i32 bits, i32 pels) {
  const i32 qp_tbl[2][139] = {
      {508,  514,  524,  535,   546,   557,   569,   580,   594,       606,
       616,  628,  641,  655,   668,   681,   695,   710,   723,       737,
       751,  767,  780,  796,   812,   828,   842,   858,   874,       892,
       911,  927,  944,  963,   982,   1000,  1021,  1040,  1061,      1081,
       1103, 1124, 1146, 1171,  1194,  1219,  1244,  1261,  1283,      1303,
       1325, 1351, 1376, 1408,  1435,  1461,  1493,  1526,  1545,      1575,
       1602, 1632, 1658, 1689,  1724,  1761,  1800,  1838,  1873,      1911,
       1951, 1980, 2020, 2063,  2106,  2149,  2198,  2234,  2284,      2336,
       2380, 2420, 2475, 2524,  2574,  2601,  2658,  2705,  2746,      2803,
       2848, 2915, 2980, 3046,  3129,  3206,  3282,  3353,  3421,      3454,
       3519, 3592, 3655, 3750,  3816,  3876,  3939,  4010,  4119,      4233,
       4320, 4428, 4532, 4636,  4739,  4972,  5021,  5145,  5247,      5435,
       5544, 5737, 5945, 6074,  6297,  6509,  6862,  7131,  7515,      7963,
       8448, 9085, 9446, 10189, 10605, 11883, 12621, 15170, 0x7fffffff},
      {138, 137, 136, 135, 134, 133, 132, 131, 130, 129, 128, 127, 126, 125,
       124, 123, 122, 121, 120, 119, 118, 117, 116, 115, 114, 113, 112, 111,
       110, 109, 108, 107, 106, 105, 104, 103, 102, 101, 100, 99,  98,  97,
       96,  95,  94,  93,  92,  91,  90,  89,  88,  87,  86,  85,  84,  83,
       82,  81,  80,  79,  78,  77,  76,  75,  74,  73,  72,  71,  70,  69,
       68,  67,  66,  65,  64,  63,  62,  61,  60,  59,  58,  57,  56,  55,
       54,  53,  52,  51,  50,  49,  48,  47,  46,  45,  44,  43,  42,  41,
       40,  39,  38,  37,  36,  35,  34,  33,  32,  31,  30,  29,  28,  27,
       26,  25,  24,  23,  22,  21,  20,  19,  18,  17,  16,  15,  14,  13,
       12,  11,  10,  9,   8,   7,   6,   5,   4,   3,   2,   1,   0}};
  const i32 upscale = 20000;
  i32 i = -1;

  //avoid overflow
  i64 bits64 = (i64)bits;

  /* prevents overflow, QP would anyway be 17 with this high bitrate
     for all resolutions under and including 1920x1088 */
  //    if (bits > 1000000)
  //      return 17;

  /* Make room for multiplication */
  pels >>= 8;
  bits64 >>= 5;

  /* Use maximum QP if bitrate way too low. */
  if (!bits64) return 51 << JPEG_QP_FRACTIONAL_BITS;

  /* Adjust the bits value for the current resolution */
  bits64 *= pels + 250;
  ASSERT(pels > 0);
  ASSERT(bits64 > 0);
  bits64 /= 350 + (3 * pels) / 4;
  bits64 = jpegRcCalculate(bits64, upscale, pels << 6);

  while ((qp_tbl[0][++i]) < bits64)
    ;

  return ((qp_tbl[1][i] << JPEG_QP_FRACTIONAL_BITS) * 51 + 69) / 138;
}

JpegEncRet JpegEncInitRC(jpegInstance_s *pEncInst, const JpegEncCfg *pEncCfg) {
  JpegEncRet ret;

  pEncInst->rateControl.picRc =
      pEncCfg->targetBitPerSecond ? ENCHW_YES : ENCHW_NO;

  if (pEncInst->rateControl.picRc) {
    //avoid VBV buffer
    pEncInst->timeIncrement = 0;

    pEncInst->rateControl.outRateDenom = pEncCfg->frameRateDenom;
    pEncInst->rateControl.outRateNum = pEncCfg->frameRateNum;
    pEncInst->rateControl.rcMode = pEncCfg->rcMode;
    pEncInst->rateControl.codingType =
        (pEncCfg->rcMode == JPEGENC_SINGLEFRAME) ? ASIC_JPEG : ASIC_MJPEG;
    pEncInst->rateControl.monitorFrames =
        MAX(JPEG_LEAST_MONITOR_FRAME, pEncInst->rateControl.outRateNum /
                                          pEncInst->rateControl.outRateDenom);
    pEncInst->rateControl.picSkip = ENCHW_NO;
    pEncInst->rateControl.hrd = ENCHW_NO;

    //control bit accurate, default 105% accurate
    pEncInst->rateControl.tolMovingBitRate = 103;

    pEncInst->rateControl.picArea = ((pEncCfg->codingWidth + 7) & (~7)) *
                                    ((pEncCfg->codingHeight + 7) & (~7));

    pEncInst->rateControl.ctbSize = 16;
    pEncInst->rateControl.ctbPerPic = pEncInst->rateControl.picArea / 16 / 16;
    pEncInst->rateControl.ctbRows = ((pEncCfg->codingHeight + 7) / 16);

    pEncInst->rateControl.qpHdr = -1
                                  << JPEG_QP_FRACTIONAL_BITS;  //default init qp
    pEncInst->rateControl.qpMin = pEncCfg->qpmin << JPEG_QP_FRACTIONAL_BITS;
    pEncInst->rateControl.qpMax = pEncCfg->qpmax << JPEG_QP_FRACTIONAL_BITS;
    pEncInst->rateControl.virtualBuffer.bitRate = pEncCfg->targetBitPerSecond;
    pEncInst->rateControl.virtualBuffer.bufferSize = 0xffffffff;
    pEncInst->rateControl.bitrateWindow = 1;

    pEncInst->rateControl.fixedIntraQp = 0 << JPEG_QP_FRACTIONAL_BITS;

    jpegRcVirtualBuffer_s *vb = &pEncInst->rateControl.virtualBuffer;

    pEncInst->rateControl.picQpDeltaMax = pEncCfg->picQpDeltaMax;
    pEncInst->rateControl.picQpDeltaMin = pEncCfg->picQpDeltaMin;
    vb->unitsInTic = pEncInst->rateControl.outRateDenom;
    vb->timeScale = pEncInst->rateControl.outRateNum;
    vb->bitPerPic =
        jpegRcCalculate(vb->bitRate, pEncInst->rateControl.outRateDenom,
                        pEncInst->rateControl.outRateNum);

    pEncInst->rateControl.maxPicSizeI =
        jpegRcCalculate(pEncInst->rateControl.virtualBuffer.bitRate,
                        pEncInst->rateControl.outRateDenom,
                        pEncInst->rateControl.outRateNum) * (8);

    pEncInst->rateControl.minPicSizeI =
        jpegRcCalculate(pEncInst->rateControl.virtualBuffer.bitRate,
                        pEncInst->rateControl.outRateDenom,
                        pEncInst->rateControl.outRateNum) / (8);

#if 0
    //init qp
    pEncInst->rateControl.qpHdr =
        InitialJpegQp(vb->bitPerPic, pEncInst->rateControl.picArea);

    pEncInst->rateControl.qpHdr =
        MIN(pEncInst->rateControl.qpMax,
            MAX(pEncInst->rateControl.qpMin, pEncInst->rateControl.qpHdr));
#endif

    if (pEncCfg->rcMode != JPEGENC_SINGLEFRAME) {
      pEncInst->rateControl.vbr =
          (pEncCfg->rcMode == JPEGENC_VBR) ? HANTRO_TRUE : HANTRO_FALSE;
    }

    if (JpegEncInitRc(&pEncInst->rateControl, 1) != ENCHW_OK) {
      ret = JPEGENC_ERROR;
      return ret;
    }

    pEncInst->rateControl.sliceTypePrev = I_SLICE;
  }
  else {
    pEncInst->rateControl.bitrateWindow = 1;
    pEncInst->rateControl.outRateNum = 1;
    pEncInst->rateControl.outRateDenom = 1;
    pEncInst->rateControl.monitorFrames = 3;
    pEncInst->rateControl.picQpDeltaMin = -1;
    pEncInst->rateControl.picQpDeltaMax = 1;
  }

  return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncInit
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncCfg * pEncCfg
 Argument      : JpegEncInst * instAddr
*******************************************************************************/
JpegEncRet JpegEncInit(const JpegEncCfg *pEncCfg, JpegEncInst *instAddr,
                       void *ctx) {
  JpegEncRet ret;
  jpegInstance_s *pEncInst = NULL;
  u32 vcmd_en;

  APITRACE_INFO(ctx, "JpegEncInit#\n");

  /* check that right shift on negative numbers is performed signed */
  /*lint -save -e* following check causes multiple lint messages */
#if (((-1) >> 1) != (-1))
#error Right bit-shifting (>>) does not preserve the sign
#endif
  /*lint -restore */

  /* Check for illegal inputs */
  if (pEncCfg == NULL || instAddr == NULL) {
    APITRACE_ERR(ctx, "JpegEncInit: ERROR null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  /* Initialize encoder instance and allocate memories */
  ret = JpegInit(pEncCfg, &pEncInst, &ctx);
  if ((ret != JPEGENC_OK) || (pEncInst == NULL)) {
    APITRACE_ERR(ctx, "JpegEncInit: ERROR Initialization failed\n");
    return ret;
  }
  pEncInst->ctx = ctx;

  if (pEncCfg->frameType > JPEGENC_YUV422_INTERLEAVED_UYVY &&
      pEncCfg->frameType < JPEGENC_YUV420_I010)
    pEncInst->featureToSupport.rgbEnabled = 1;
  else
    pEncInst->featureToSupport.rgbEnabled = 0;

  pEncInst->featureToSupport.jpegEnabled = 1;

  vcmd_en = EWLGetVCMDSupport(ctx);

  if (!(pEncInst->asic.axife_data =
            (struct VCAxiFeData *)EWLcalloc(1, sizeof(struct VCAxiFeData)))) {
    JpegShutdown(pEncInst);
    return JPEGENC_MEMORY_ERROR;
  }
  pEncInst->asic.dumpRegister = pEncCfg->dumpRegister;
  pEncInst->asic.regs.bVCMDAvailable = EWLGetVCMDSupport(ctx) ? true : false;
  pEncInst->asic.regs.bVCMDEnable =
      EWLGetVCMDMode(pEncInst->asic.ewl) ? true : false;

  pEncInst->asic.secure_mode = pEncCfg->secure_mode;
  /* Check that configuration is valid */
  if (CheckJpegCfg(pEncCfg, ctx) == ENCHW_NOK) {
    EWLRelease(pEncInst->asic.ewl);
    if (pEncInst->asic.axife_data != NULL) EWLfree(pEncInst->asic.axife_data);
    if (pEncInst != NULL) EWLfree(pEncInst);
    APITRACE_ERR(ctx, "JpegEncInit: ERROR invalid argument\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  /*Get the supported features of all cores*/
  //EWLHwConfig_t asic_cfg;
  const EWLHwConfig_t *asic_cfg = NULL;
  u32 core_info =
      0; /*mode[1bit](1:all 0:specified)+amount[3bit](the needing amount -1)+reserved+core_mapping[8bit]*/
  u32 valid_num = 0;
  u32 i = 0;
  if (vcmd_en == 0) {
    for (i = 0; i < EWLGetCoreNum(ctx); i++) {
      asic_cfg = EWLReadAsicConfig(i, ctx);
      if (asic_cfg &&
          JpegEncCoreHasFeatures(asic_cfg, &pEncInst->featureToSupport)) {
        valid_num++;
      }
      else {
        continue;
      }
    }

    if (valid_num == 0) /*none of cores is supported*/
    {
      EWLRelease(pEncInst->asic.ewl);
      if (pEncInst->asic.axife_data != NULL) EWLfree(pEncInst->asic.axife_data);
      if (pEncInst != NULL) EWLfree(pEncInst);
      APITRACE_ERR(ctx,
                   "JpegEncInit: ERROR none of cores supports JPEG format!\n");
      return JPEGENC_INVALID_ARGUMENT;
    }

    core_info |=
        1u << CORE_INFO_MODE_OFFSET;  //now just support 1 core,so mode is all.

    core_info |= 0 << CORE_INFO_AMOUNT_OFFSET;  //now just support 1 core

    core_info |= pEncCfg->slice_idx << 16;

    pEncInst->reserve_core_info = core_info;
    APITRACE_INFO(ctx, "VCEJ reserve core info:%x\n",
                  pEncInst->reserve_core_info);
  }
  else {
    pEncInst->asic.regs.vcmd.core_mask =
        EncAsicGetCoreMask(EWL_CLIENT_TYPE_JPEG_ENC, ctx, pEncCfg->priority);
    if (pEncCfg->core_mask) {
      pEncInst->asic.regs.vcmd.core_mask &= pEncCfg->core_mask;
    }
    pEncInst->asic.regs.vcmd.priority = pEncCfg->priority;
  }
  /* low latency */
  asic_cfg = EncAsicGetAsicConfig(EWL_CLIENT_TYPE_JPEG_ENC, ctx);
  if (!asic_cfg) {
    APITRACE_ERR(ctx, "JpegEncInit: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }
  pEncInst->asic.regs.lowlatGatingDisable = pEncCfg->lowlatGatingDisable;
  pEncInst->asic.regs.lowlatGatingType = 1;//check emc and recon idle in Jpeg.

  EncUfbcInit(&pEncInst->ufbc, pEncInst->asic.ewl, asic_cfg->ufbcSupport);

  pEncInst->inputLineBuf.inputLineBufEn = pEncCfg->inputLineBufEn;
  pEncInst->inputLineBuf.inputLineBufLoopBackEn =
      pEncCfg->inputLineBufLoopBackEn;
  pEncInst->inputLineBuf.inputLineBufDepth = pEncCfg->inputLineBufDepth;
  pEncInst->inputLineBuf.amountPerLoopBack = pEncCfg->amountPerLoopBack;
  pEncInst->inputLineBuf.inputLineBufHwModeEn = pEncCfg->inputLineBufHwModeEn;
  pEncInst->inputLineBuf.cbFunc = pEncCfg->inputLineBufCbFunc;
  pEncInst->inputLineBuf.cbData = pEncCfg->inputLineBufCbData;

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  pEncInst->sliceinfoEn = pEncCfg->sliceinfoEn;
#endif

  /* flexa sbi */
  pEncInst->inputLineBuf.sbi_id_0 = pEncCfg->sbi_id_0;
  pEncInst->inputLineBuf.sbi_id_1 = pEncCfg->sbi_id_1;
  pEncInst->inputLineBuf.sbi_id_2 = pEncCfg->sbi_id_2;
  pEncInst->inputLineBuf.segmentUnitHeight = pEncCfg->segmentUnitHeight;

  /*stream multi-segment*/
  pEncInst->streamMultiSegment.streamMultiSegmentMode =
      pEncCfg->streamMultiSegmentMode;
  pEncInst->streamMultiSegment.streamMultiSegmentSize =
      pEncCfg->streamMultiSegmentSize;
  pEncInst->streamMultiSegment.streamMultiSegmentAmount =
      pEncCfg->streamMultiSegmentAmount;
  pEncInst->streamMultiSegment.cbFunc = pEncCfg->streamMultiSegCbFunc;
  pEncInst->streamMultiSegment.cbData = pEncCfg->streamMultiSegCbData;

  /* hardware config */
  pEncInst->asic.regs.qp = 0;
  pEncInst->asic.regs.constrainedIntraPrediction = 0;
  pEncInst->asic.regs.frameCodingType = ASIC_INTRA;
  pEncInst->asic.regs.roundingCtrl = 0;
  pEncInst->asic.regs.codingType = ASIC_JPEG;

  /* SRAM power down mode */
  pEncInst->asic.regs.sramPowerdownDisable = pEncCfg->sramPowerdownDisable;
  pEncInst->asic.regs.sramPowerdownMode = pEncCfg->sramPowerdownMode;
  pEncInst->asic.regs.sramPowerdownTimerDiv32 = pEncCfg->sramPowerdownTimerDiv32;

  /* stride*/
  pEncInst->input_alignment = 1 << pEncCfg->exp_of_input_alignment;

  /* UFBC enable */
  pEncInst->ufbcParam.mode = pEncCfg->ufbcParam.mode;
  memcpy(&pEncInst->ufbcParam.param, &pEncCfg->ufbcParam.param, sizeof(pEncCfg->ufbcParam.param));

  /* Pre processing */
  pEncInst->preProcess.lumWidthSrc[0] = 0;
  pEncInst->preProcess.lumHeightSrc[0] = 0;
  pEncInst->preProcess.lumWidth = 0;
  pEncInst->preProcess.lumHeight = 0;
  pEncInst->preProcess.horOffsetSrc[0] = 0;
  pEncInst->preProcess.verOffsetSrc[0] = 0;
  pEncInst->preProcess.rotation = 0;
  pEncInst->preProcess.videoStab = 0;
  pEncInst->preProcess.input_alignment = pEncInst->input_alignment;
  pEncInst->preProcess.scanType = pEncCfg->scanType;

  pEncInst->preProcess.inputFormat = pEncCfg->frameType;
  pEncInst->preProcess.colorConversionType = pEncCfg->colorConversion.type;
  pEncInst->preProcess.colorConversionCoeffA = pEncCfg->colorConversion.coeffA;
  pEncInst->preProcess.colorConversionCoeffB = pEncCfg->colorConversion.coeffB;
  pEncInst->preProcess.colorConversionCoeffC = pEncCfg->colorConversion.coeffC;
  pEncInst->preProcess.colorConversionCoeffE = pEncCfg->colorConversion.coeffE;
  pEncInst->preProcess.colorConversionCoeffF = pEncCfg->colorConversion.coeffF;
  pEncInst->preProcess.colorConversionCoeffG = pEncCfg->colorConversion.coeffG;
  pEncInst->preProcess.colorConversionCoeffH = pEncCfg->colorConversion.coeffH;
  pEncInst->preProcess.colorConversionLumaOffset =
      pEncCfg->colorConversion.LumaOffset;

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    pEncInst->preProcess.overlayEnable[i] = pEncCfg->olEnable[i];
    pEncInst->preProcess.overlayFormat[i] = pEncCfg->olFormat[i];
    pEncInst->preProcess.overlayAlpha[i] = pEncCfg->olAlpha[i];
    pEncInst->preProcess.overlayWidth[i] = pEncCfg->olWidth[i];
    pEncInst->preProcess.overlayCropWidth[i] = pEncCfg->olCropWidth[i];
    pEncInst->preProcess.overlayHeight[i] = pEncCfg->olHeight[i];
    pEncInst->preProcess.overlayCropHeight[i] = pEncCfg->olCropHeight[i];
    pEncInst->preProcess.overlayXoffset[i] = pEncCfg->olXoffset[i];
    pEncInst->preProcess.overlayCropXoffset[i] = pEncCfg->olCropXoffset[i];
    pEncInst->preProcess.overlayYoffset[i] = pEncCfg->olYoffset[i];
    pEncInst->preProcess.overlayCropYoffset[i] = pEncCfg->olCropYoffset[i];
    pEncInst->preProcess.overlayYStride[i] = pEncCfg->olYStride[i];
    pEncInst->preProcess.overlayUVStride[i] = pEncCfg->olUVStride[i];
    pEncInst->preProcess.overlayBitmapY[i] = pEncCfg->olBitmapY[i];
    pEncInst->preProcess.overlayBitmapU[i] = pEncCfg->olBitmapU[i];
    pEncInst->preProcess.overlayBitmapV[i] = pEncCfg->olBitmapV[i];
    pEncInst->preProcess.overlaySuperTile[i] = pEncCfg->olSuperTile[i];
    pEncInst->preProcess.overlayScaleWidth[i] = pEncCfg->olScaleWidth[i];
    pEncInst->preProcess.overlayScaleHeight[i] = pEncCfg->olScaleHeight[i];
  }

  for (i = 0; i < MAX_MOSAIC_NUM; i++) {
    pEncInst->preProcess.mosEnable[i] = pEncCfg->mosEnable[i];
    pEncInst->preProcess.mosWidth[i] = pEncCfg->mosWidth[i];
    pEncInst->preProcess.mosHeight[i] = pEncCfg->mosHeight[i];
    pEncInst->preProcess.mosXoffset[i] = pEncCfg->mosXoffset[i];
    pEncInst->preProcess.mosYoffset[i] = pEncCfg->mosYoffset[i];
  }
  pEncInst->preProcess.mosSizeIndex = pEncCfg->mosSizeIndex;

  /* Get OSD map parameters */
  pEncInst->preProcess.osdMapEnable = pEncCfg->osdMapEnable;
  pEncInst->preProcess.osdMapStride = pEncCfg->osdMapStride;
  pEncInst->preProcess.osdMapBlockSize = pEncCfg->osdMapBlockSize;
  for (i = 0; i < MAX_OSDMAP_COLOR_NUM; i++) {
    pEncInst->preProcess.osdMapAlpha[i] = pEncCfg->osdMapAlpha[i];
    pEncInst->preProcess.osdMapY[i] = pEncCfg->osdMapY[i];
    pEncInst->preProcess.osdMapU[i] = pEncCfg->osdMapU[i];
    pEncInst->preProcess.osdMapV[i] = pEncCfg->osdMapV[i];
  }

  pEncInst->asic.regs.ufbcMode = pEncInst->ufbcParam.mode;

  EncSetColorConversion(&pEncInst->preProcess, &pEncInst->asic);

  /* constant chroma control */
  pEncInst->preProcess.constChromaEn = pEncCfg->constChromaEn;
  pEncInst->preProcess.constCb = pEncCfg->constCb;
  pEncInst->preProcess.constCr = pEncCfg->constCr;

  //default registers to avoid assert
  pEncInst->asic.regs.minCbSize = 3;
  pEncInst->asic.regs.maxCbSize = 6;
  pEncInst->asic.regs.minTrbSize = 2;
  pEncInst->asic.regs.maxTrbSize = 4;

  // lossless jpeg.
  pEncInst->asic.regs.ljpegEn = pEncInst->jpeg.losslessEn;
  pEncInst->asic.regs.ljpegFmt = pEncInst->jpeg.codingMode;
  pEncInst->asic.regs.ljpegPsv = pEncInst->jpeg.predictMode;
  pEncInst->asic.regs.ljpegPt = pEncInst->jpeg.ptransValue;

  pEncInst->asic.regs.vcmd.vcmdBufSize = 0;

  if (pEncInst->jpeg.codingMode == JPEGENC_MONO_MODE) {
    pEncInst->jpeg.frame.Nf = 1;
  }

  /*AXI max burst length */
  if (0 == pEncCfg->burstMaxLength)
    pEncInst->asic.regs.AXI_burst_max_length =
        ENCH2_DEFAULT_BURST_LENGTH;  //default
  else
    pEncInst->asic.regs.AXI_burst_max_length = pEncCfg->burstMaxLength;

  /* AXI alignment */
  if (pEncCfg->AXIAlignment == 0) {
    pEncInst->asic.regs.AXI_burst_align_wr_common =
        asic_cfg->axi_burst_align_wr_common;
    pEncInst->asic.regs.AXI_burst_align_wr_stream =
        asic_cfg->axi_burst_align_wr_stream;
    pEncInst->asic.regs.AXI_burst_align_wr_chroma_ref =
        asic_cfg->axi_burst_align_wr_chroma_ref;
    pEncInst->asic.regs.AXI_burst_align_wr_luma_ref =
        asic_cfg->axi_burst_align_wr_luma_ref;
    pEncInst->asic.regs.AXI_burst_align_rd_common =
        asic_cfg->axi_burst_align_rd_common;
    pEncInst->asic.regs.AXI_burst_align_rd_prp =
        asic_cfg->axi_burst_align_rd_prp;
    pEncInst->asic.regs.AXI_burst_align_rd_ch_ref_prefetch =
        asic_cfg->axi_burst_align_rd_ch_ref_prefetch;
    pEncInst->asic.regs.AXI_burst_align_rd_lu_ref_prefetch =
        asic_cfg->axi_burst_align_rd_lu_ref_prefetch;
  } else {
    pEncInst->asic.regs.AXI_burst_align_wr_common =
        (pEncCfg->AXIAlignment >> 28) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_wr_stream =
        (pEncCfg->AXIAlignment >> 24) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_wr_chroma_ref =
        (pEncCfg->AXIAlignment >> 20) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_wr_luma_ref =
        (pEncCfg->AXIAlignment >> 16) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_rd_common =
        (pEncCfg->AXIAlignment >> 12) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_rd_prp =
        (pEncCfg->AXIAlignment >> 8) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_rd_ch_ref_prefetch =
        (pEncCfg->AXIAlignment >> 4) & 0xf;
    pEncInst->asic.regs.AXI_burst_align_rd_lu_ref_prefetch =
        (pEncCfg->AXIAlignment) & 0xf;
    /* Check AXI alignment */
    u32 axi_align_fuse = asic_cfg->axi_burst_align_wr_common +
                         asic_cfg->axi_burst_align_wr_stream +
                         asic_cfg->axi_burst_align_wr_chroma_ref +
                         asic_cfg->axi_burst_align_wr_luma_ref +
                         asic_cfg->axi_burst_align_rd_common +
                         asic_cfg->axi_burst_align_rd_prp +
                         asic_cfg->axi_burst_align_rd_ch_ref_prefetch +
                         asic_cfg->axi_burst_align_rd_lu_ref_prefetch;

    if (axi_align_fuse == 0) {
      APITRACEERR(
          "JpegEncInit: Error, configured AXI is not supported by hardware\n");
      return JPEGENC_INVALID_ARGUMENT;
    } else {
      if ((asic_cfg->axi_burst_align_wr_common <
           pEncInst->asic.regs.AXI_burst_align_wr_common) ||
          (asic_cfg->axi_burst_align_wr_stream <
           pEncInst->asic.regs.AXI_burst_align_wr_stream) ||
          (asic_cfg->axi_burst_align_wr_chroma_ref <
           pEncInst->asic.regs.AXI_burst_align_wr_chroma_ref) ||
          (asic_cfg->axi_burst_align_wr_luma_ref <
           pEncInst->asic.regs.AXI_burst_align_wr_luma_ref) ||
          (asic_cfg->axi_burst_align_rd_common <
           pEncInst->asic.regs.AXI_burst_align_rd_common) ||
          (asic_cfg->axi_burst_align_rd_prp <
           pEncInst->asic.regs.AXI_burst_align_rd_prp) ||
          (asic_cfg->axi_burst_align_rd_ch_ref_prefetch <
           pEncInst->asic.regs.AXI_burst_align_rd_ch_ref_prefetch) ||
          (asic_cfg->axi_burst_align_rd_lu_ref_prefetch <
           pEncInst->asic.regs.AXI_burst_align_rd_lu_ref_prefetch)) {
        APITRACEERR(
            "JpegEncInit: Error, configured AXI is not matched with "
            "hardware\n");
        return JPEGENC_INVALID_ARGUMENT;
      }
    }
  }

  /* Irq type mask */
  pEncInst->asic.regs.irq_type_sw_reset_mask =
      (pEncCfg->irqTypeMask >> 8) & 0x01;
  pEncInst->asic.regs.irq_type_fuse_error_mask =
      (pEncCfg->irqTypeMask >> 7) & 0x01;
  pEncInst->asic.regs.irq_type_buffer_full_mask =
      (pEncCfg->irqTypeMask >> 6) & 0x01;
  pEncInst->asic.regs.irq_type_bus_error_mask =
      (pEncCfg->irqTypeMask >> 5) & 0x01;
  pEncInst->asic.regs.irq_type_timeout_mask =
      (pEncCfg->irqTypeMask >> 4) & 0x01;
  pEncInst->asic.regs.irq_type_strm_segment_mask =
      (pEncCfg->irqTypeMask >> 3) & 0x01;
  pEncInst->asic.regs.irq_type_line_buffer_mask =
      (pEncCfg->irqTypeMask >> 2) & 0x01;
  pEncInst->asic.regs.irq_type_slice_rdy_mask =
      (pEncCfg->irqTypeMask >> 1) & 0x01;
  pEncInst->asic.regs.irq_type_frame_rdy_mask = pEncCfg->irqTypeMask & 0x01;

  hash_init(&pEncInst->jpeg.hashctx, pEncCfg->hashType);

  /* Status == INIT   Initialization succesful */
  pEncInst->encStatus = ENCSTAT_INIT;
  pEncInst->fixedQP = pEncCfg->fixedQP;

  JpegEncInitRC(pEncInst, pEncCfg);

  pEncInst->inst = pEncInst; /* used as checksum */

  *instAddr = (JpegEncInst)pEncInst;

  pEncInst->asic.regs.bInitUpdate = 1;
  APITRACE_INFO(ctx, "JpegEncInit: OK\n");
  return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncRelease
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
*******************************************************************************/
JpegEncRet JpegEncRelease(JpegEncInst inst) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;

  APITRACE_INFO((void *)inst, "JpegEncRelease#\n");

  /* Check for illegal inputs */
  if (pEncInst == NULL) {
    APITRACE_ERR((void *)inst, "JpegEncRelease: ERROR null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    APITRACE_ERR((void *)inst, "JpegEncRelease: ERROR Invalid instance\n");
    return JPEGENC_INSTANCE_ERROR;
  }

  APITRACE_INFO((void *)inst, "JpegEncRelease: OK\n");

  JpegShutdown(pEncInst);

#if defined SYSTEM_BUILD && defined TEST_DATA && defined SUPPORT_UFBC
  extern void trace_sw_release();
  trace_sw_release();
#endif

  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : JpegEncSetRateCtrl
    Description   : Sets rate control parameters

    Return type   : JpegEncRet
    Argument      : inst - the instance in use
                    pRateCtrl - user provided parameters
------------------------------------------------------------------------------*/
JpegEncRet JpegEncSetRateCtrl(JpegEncInst inst,
                              const JpegEncRateCtrl *pRateCtrl) {
  jpegInstance_s *jpegenc_instance = (jpegInstance_s *)inst;
  jpegEncRateControl_s *rc;
  u32 i, tmp;
  i32 prevBitrate;
  i32 bitrateWindow;
  i32 frame_rate_change = 0;

  /* Check for illegal inputs */
  if ((inst == NULL) || (pRateCtrl == NULL)) {
    APITRACE_ERR((void *)inst, "JpegEncSetRateCtrl: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  APITRACE_INFO((void *)inst, "JpegEncSetRateCtrl#\n");
  APITRACE_PARAM((void *)inst, " %s : %d\n", "pictureRc", pRateCtrl->pictureRc);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "qpHdr", pRateCtrl->qpHdr);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "qpMinPB", pRateCtrl->qpMin);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "qpMaxPB", pRateCtrl->qpMax);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "bitPerSecond",
                 pRateCtrl->bitPerSecond);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "hrd", pRateCtrl->hrd);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "bitrateWindow",
                 pRateCtrl->bitrateWindow);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "picQpDeltaMin",
                 pRateCtrl->picQpDeltaMin);
  APITRACE_PARAM((void *)inst, " %s : %d\n", "picQpDeltaMax",
                 pRateCtrl->picQpDeltaMax);

  /* Check for existing instance */
  if (jpegenc_instance->inst != jpegenc_instance) {
    APITRACE_ERR((void *)inst, "JpegEncSetRateCtrl: ERROR Invalid instance\n");
    return JPEGENC_INSTANCE_ERROR;
  }

  rc = &jpegenc_instance->rateControl;

  /* Check for invalid input values */
  if (pRateCtrl->pictureRc > 1) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetRateCtrl: ERROR Invalid enable/disable value\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->qpHdr > 51 || pRateCtrl->qpMin > 51 || pRateCtrl->qpMax > 51) {
    APITRACE_ERR((void *)inst, "JpegEncSetRateCtrl: ERROR Invalid QP\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if (pRateCtrl->bitrateWindow < 1 || pRateCtrl->bitrateWindow > 300) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetRateCtrl: ERROR Invalid GOP length\n");
    return JPEGENC_INVALID_ARGUMENT;
  }
  if (pRateCtrl->monitorFrames < JPEG_LEAST_MONITOR_FRAME ||
      pRateCtrl->monitorFrames > 120) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetRateCtrl: ERROR Invalid monitorFrames\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  /* Frame rate may change. */
  if (pRateCtrl->frameRateDenom == 0 || pRateCtrl->frameRateNum == 0) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncSetRateCtrl: ERROR Invalid frameRateDenom, frameRateNum\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if (rc->outRateNum != (i32)pRateCtrl->frameRateNum ||
      rc->outRateDenom != (i32)pRateCtrl->frameRateDenom) {
    /*Though only when ((rc->outRateNum/rc->outRateDenom) != (i32)(pRateCtrl->frameRateNum/pRateCtrl->frameRateDenom)) the frame rate is changed,
    we consider all the situation as frame rate is changed, and sps will change.*/
    frame_rate_change = 1;
    rc->outRateNum = pRateCtrl->frameRateNum;
    rc->outRateDenom = pRateCtrl->frameRateDenom;
  }

  /* Bitrate affects only when rate control is enabled */
  if (pRateCtrl->pictureRc &&
      (((pRateCtrl->bitPerSecond < 10000) &&
        (rc->outRateNum > rc->outRateDenom)) ||
       ((((pRateCtrl->bitPerSecond * rc->outRateDenom) / rc->outRateNum) <
         10000) &&
        (rc->outRateNum < rc->outRateDenom)) ||
       pRateCtrl->bitPerSecond >
           JPEGENC_MAX_SIZE * rc->outRateNum / rc->outRateDenom)) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetRateCtrl: ERROR Invalid bitPerSecond\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if ((pRateCtrl->picQpDeltaMin > -1) || (pRateCtrl->picQpDeltaMin < -10) ||
      (pRateCtrl->picQpDeltaMax < 1) || (pRateCtrl->picQpDeltaMax > 10)) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetRateCtrl: ERROR picQpRange out of range. Min:Max "
                 "should be in [-1,-10]:[1,10]\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  {
    u32 cpbSize = pRateCtrl->bitPerSecond;
    u32 bps = pRateCtrl->bitPerSecond;

    /* Set the parameters to rate control */
    if (pRateCtrl->pictureRc != 0)
      rc->picRc = ENCHW_YES;
    else
      rc->picRc = ENCHW_NO;

    rc->picSkip = ENCHW_NO;
    rc->hrd = ENCHW_NO;
    rc->qpHdr = pRateCtrl->qpHdr << JPEG_QP_FRACTIONAL_BITS;
    rc->qpMin = pRateCtrl->qpMin << JPEG_QP_FRACTIONAL_BITS;
    rc->qpMax = pRateCtrl->qpMax << JPEG_QP_FRACTIONAL_BITS;
    prevBitrate = rc->virtualBuffer.bitRate;
    rc->virtualBuffer.bitRate = bps;
    bitrateWindow = rc->bitrateWindow;
    rc->bitrateWindow = pRateCtrl->bitrateWindow;
    rc->monitorFrames = pRateCtrl->monitorFrames;
//    rc->maxPicSizeI = rc->maxPicSizeI;
//    rc->minPicSizeI = rc->minPicSizeI;
  }

  rc->fixedIntraQp = pRateCtrl->fixedIntraQp << JPEG_QP_FRACTIONAL_BITS;

  /* New parameters checked already so ignore return value.
  * Reset RC bit counters when changing bitrate. */
  if (rc->picRc == ENCHW_YES) {
    (void)JpegEncInitRc(rc, (rc->virtualBuffer.bitRate != prevBitrate) ||
                              (bitrateWindow != rc->bitrateWindow) ||
                              frame_rate_change);
  }

  APITRACE_INFO((void *)inst, "JPEGEncSetRateCtrl: OK\n");
  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------

    Function name : JpegEncGetRateCtrl
    Description   : Return current rate control parameters

    Return type   : JpegEncRet
    Argument      : inst - the instance in use
                    pRateCtrl - place where parameters are returned
------------------------------------------------------------------------------*/
JpegEncRet JpegEncGetRateCtrl(JpegEncInst inst, JpegEncRateCtrl *pRateCtrl) {
  jpegInstance_s *jpegenc_instance = (jpegInstance_s *)inst;
  jpegEncRateControl_s *rc;

  APITRACE_INFO((void *)inst, "JpegEncGetRateCtrl#\n");

  /* Check for illegal inputs */
  if ((jpegenc_instance == NULL) || (pRateCtrl == NULL)) {
    APITRACE_ERR((void *)inst, "JpegEncGetRateCtrl: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (jpegenc_instance->inst != jpegenc_instance) {
    APITRACE_ERR((void *)inst, "JpegEncGetRateCtrl: ERROR Invalid instance\n");
    return JPEGENC_INSTANCE_ERROR;
  }

  /* Get the values */
  rc = &jpegenc_instance->rateControl;

  pRateCtrl->pictureRc = rc->picRc == ENCHW_NO ? 0 : 1;
  /* CTB RC */
  pRateCtrl->qpHdr = rc->qpHdr >> JPEG_QP_FRACTIONAL_BITS;
  pRateCtrl->qpMin = rc->qpMin >> JPEG_QP_FRACTIONAL_BITS;
  pRateCtrl->qpMax = rc->qpMax >> JPEG_QP_FRACTIONAL_BITS;
  pRateCtrl->bitPerSecond = rc->virtualBuffer.bitRate;

  /* get the outputRateNum */
  pRateCtrl->frameRateNum = rc->outRateNum;
  pRateCtrl->frameRateDenom = rc->outRateDenom;
  pRateCtrl->hrd = rc->hrd == ENCHW_NO ? 0 : 1;
  pRateCtrl->bitrateWindow = rc->bitrateWindow;

  pRateCtrl->fixedIntraQp = rc->fixedIntraQp >> JPEG_QP_FRACTIONAL_BITS;
  pRateCtrl->monitorFrames = rc->monitorFrames;

  pRateCtrl->picQpDeltaMin = jpegenc_instance->rateControl.picQpDeltaMin;
  pRateCtrl->picQpDeltaMax = jpegenc_instance->rateControl.picQpDeltaMax;

  APITRACE_INFO((void *)inst, "JpegEncGetRateCtrl: OK\n");
  return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncSetThumbnail
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
 Argument      : JpegEncCfg * pEncCfg
*******************************************************************************/
JpegEncRet JpegEncSetThumbnail(JpegEncInst inst,
                               const JpegEncThumb *pJpegThumb) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;

  APITRACE_INFO((void *)inst, "JpegEncSetThumbnail#\n");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pJpegThumb == NULL)) {
    APITRACE_ERR((void *)inst, "JpegEncSetThumbnail: ERROR null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    APITRACE_ERR((void *)inst, "JpegEncSetThumbnail: ERROR Invalid instance\n");
    return JPEGENC_INSTANCE_ERROR;
  }

  if (CheckThumbnailCfg(pJpegThumb) != JPEGENC_OK) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetThumbnail: ERROR Invalid thumbnail\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  pEncInst->jpeg.appn.thumbEnable = 1;

  /* save the thumbnail config */
  (void)EWLmemcpy(&pEncInst->jpeg.thumbnail, pJpegThumb, sizeof(JpegEncThumb));

  APITRACE_INFO((void *)inst, "JpegEncSetThumbnail: OK\n");

  return JPEGENC_OK;
}

int JpegEncCoreHasFeatures(const EWLHwConfig_t *feature, EWLHwConfig_t *cfg) {
  if (cfg->jpegEnabled == 1 && cfg->jpegEnabled == feature->jpegEnabled)
    return 1;
  else
    return 0;
}

/*******************************************************************************
 Function name : JpegEncSetPictureSize
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
 Argument      : JpegEncCfg * pEncCfg
*******************************************************************************/
JpegEncRet JpegEncSetPictureSize(JpegEncInst inst, const JpegEncCfg *pEncCfg) {
  u32 mbTotal = 0;
  u32 height = 0;
  u32 heightMcu, widthMcus;
  asicMemAlloc_s allocCfg;
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;

  APITRACE_INFO((void *)inst, "JpegEncSetPictureSize#\n");

  /* Check for illegal inputs */
  if ((pEncInst == NULL) || (pEncCfg == NULL)) {
    APITRACE_ERR((void *)inst, "JpegEncSetPictureSize: ERROR null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetPictureSize: ERROR Invalid instance\n");
    return JPEGENC_INSTANCE_ERROR;
  }

  if (CheckFullSize(pEncCfg) != JPEGENC_OK) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncSetPictureSize: ERROR Out of range image dimension(s)\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if (pEncCfg->losslessEn) {
    if (pEncCfg->rotation) {
      APITRACE_ERR(
          (void *)inst,
          "JpegEncSetPictureSize: ERROR Not allow rotation for lossless\n");
      return JPEGENC_INVALID_ARGUMENT;
    }
    if (pEncCfg->frameType >= JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32) {
      APITRACE_ERR(
          (void *)inst,
          "JpegEncSetPictureSize: ERROR Not allow such format for lossless\n");
      return JPEGENC_INVALID_ARGUMENT;
    }
  }

  if (pEncCfg->losslessEn) {
    widthMcus = (pEncCfg->codingWidth + 1) / 2;
    heightMcu = 2;
  } else {
    if (pEncCfg->codingMode == JPEGENC_MONO_MODE) {
      widthMcus = (pEncCfg->codingWidth + 7) / 8;
      heightMcu = 8;
    } else {
      widthMcus = (pEncCfg->codingWidth + 15) / 16;
      heightMcu = (pEncCfg->codingMode == JPEGENC_422_MODE ? 8 : 16);
    }
  }

  if (((pEncCfg->restartInterval * heightMcu) > pEncCfg->codingHeight) ||
      ((pEncCfg->restartInterval * widthMcus) > 0xFFFF)) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetPictureSize: ERROR restart interval too big\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if ((((pEncCfg->xOffset & 1) != 0) || ((pEncCfg->yOffset & 1) != 0)) &&
      (pEncCfg->scanType == 0)) {
    APITRACE_ERR((void *)inst, "JpegEncSetPictureSize: ERROR Invalid offset\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  /* Restart interval must be enabled for sliced encoding */
  if (pEncCfg->codingType == JPEGENC_SLICED_FRAME) {
    if (pEncCfg->rotation != JPEGENC_ROTATE_0) {
      APITRACE_ERR(
          (void *)inst,
          "JpegEncSetPictureSize: ERROR rotation not allowed in sliced mode\n");
      return JPEGENC_INVALID_ARGUMENT;
    }

    if (pEncCfg->restartInterval == 0) {
      APITRACE_ERR((void *)inst,
                   "JpegEncSetPictureSize: ERROR restart interval not set\n");
      return JPEGENC_INVALID_ARGUMENT;
    }

    /* Extra limitation for partial encoding: yOffset must be
         * multiple of slice height in rows */
    if ((pEncCfg->yOffset % (pEncCfg->restartInterval * heightMcu)) != 0) {
      APITRACE_ERR((void *)inst,
                   "JpegEncSetPictureSize: ERROR yOffset not valid\n");
      return JPEGENC_INVALID_ARGUMENT;
    }
  }

  mbTotal =
      (u32)(((pEncCfg->codingWidth + 15) / 16) *
            ((pEncCfg->codingHeight +
              (!pEncCfg->losslessEn && pEncCfg->codingMode == JPEGENC_422_MODE
                   ? heightMcu
                   : 16) -
              1) /
             (!pEncCfg->losslessEn && pEncCfg->codingMode == JPEGENC_422_MODE
                  ? heightMcu
                  : 16)));

  pEncInst->jpeg.header = ENCHW_YES;
  pEncInst->jpeg.width = pEncCfg->codingWidth;
  pEncInst->jpeg.height = pEncCfg->codingHeight;
  /*pEncInst->jpeg.lastColumn = ((pEncInst->jpeg.width + 15) / 16);*/
  pEncInst->jpeg.mbPerFrame = mbTotal;

  /* Pre processing */
  pEncInst->preProcess.lumWidthSrc[0] = pEncCfg->inputWidth;
  pEncInst->preProcess.lumHeightSrc[0] = pEncCfg->inputHeight;
  pEncInst->preProcess.lumWidth = pEncCfg->codingWidth;
  pEncInst->preProcess.lumHeight = pEncCfg->codingHeight;
  pEncInst->preProcess.horOffsetSrc[0] = pEncCfg->xOffset;
  pEncInst->preProcess.verOffsetSrc[0] = pEncCfg->yOffset;
  pEncInst->preProcess.rotation = pEncCfg->rotation;
  pEncInst->preProcess.mirror = pEncCfg->mirror;
  pEncInst->preProcess.input_alignment = (1 << pEncCfg->exp_of_input_alignment);

  /* Restart interval (MCU rows converted to macroblocks) */
  pEncInst->jpeg.rstMbRows = pEncCfg->restartInterval;
  pEncInst->jpeg.restart.Ri = (u32)(pEncCfg->restartInterval * widthMcus);

  /* Coding type */
  if (pEncCfg->codingType == JPEGENC_WHOLE_FRAME) {
    pEncInst->jpeg.codingType = ENC_WHOLE_FRAME;
    height = pEncInst->jpeg.height;
  } else {
    /* Sliced mode */
    pEncInst->jpeg.codingType = ENC_PARTIAL_FRAME;
    pEncInst->jpeg.sliceRows = (u32)pEncCfg->restartInterval;
    height =
        (pEncCfg->restartInterval * (pEncCfg->losslessEn ? 16 : heightMcu));
  }

#ifdef JPEGENC_422_MODE_SUPPORTED
  if (pEncCfg->codingMode == JPEGENC_420_MODE)
    pEncInst->jpeg.codingMode = JPEGENC_420_MODE;
  else {
    if ((pEncInst->preProcess.inputFormat != JPEGENC_YUV422_INTERLEAVED_YUYV) &&
        (pEncInst->preProcess.inputFormat != JPEGENC_YUV422_INTERLEAVED_UYVY) &&
        (pEncInst->preProcess.inputFormat != JPEGENC_YUV422_INTERLEAVED_YVYU) &&
        (pEncInst->preProcess.inputFormat != JPEGENC_YUV422_INTERLEAVED_VYUY) &&
        (pEncInst->preProcess.inputFormat != JPEGENC_YUV422SP_888) &&
        (pEncInst->preProcess.inputFormat != JPEGENC_YVU422SP_888)) {
      APITRACE_ERR((void *)inst,
                   "JpegEncSetPictureSize: ERROR 4:2:0 input in 4:2:2 mode\n");
      return JPEGENC_INVALID_ARGUMENT;
    }
    if (pEncInst->preProcess.rotation != JPEGENC_ROTATE_0) {
      APITRACE_ERR((void *)inst,
                   "JpegEncSetPictureSize: ERROR rotation in 4:2:2 mode\n");
      return JPEGENC_INVALID_ARGUMENT;
    }
    pEncInst->jpeg.codingMode = JPEGENC_422_MODE;
  }
#else
  //FIXME: check fuse if support other format
  pEncInst->jpeg.codingMode = pEncCfg->codingMode;
#endif

  /* Check that configuration is valid */
  if (EncPreProcessCheck(&pEncInst->preProcess, 0) == ENCHW_NOK) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncSetPictureSize: ERROR invalid pre-processing argument\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  /* Allocate internal SW/HW shared memories */
  memset(&allocCfg, 0, sizeof(asicMemAlloc_s));
  allocCfg.width = (u32)pEncInst->jpeg.width;
  allocCfg.height = height;
  allocCfg.encodingType = ASIC_JPEG;
  allocCfg.is_malloc = 1;
  pEncInst->asic.secure_mode = pEncCfg->secure_mode;
  if (EncAsicMemAlloc_V2(&pEncInst->asic, &allocCfg) != ENCHW_OK) {
    APITRACE_ERR((void *)inst,
                 "JpegEncSetPictureSize: ERROR ewl memory allocation\n");
    return JPEGENC_EWL_MEMORY_ERROR;
  }

  APITRACE_INFO((void *)inst, "JpegEncSetPictureSize: OK\n");

  return JPEGENC_OK;
}

/*******************************************************************************
 Function name : JpegEncGetOverlaySlice
 Description   :
 Return type   : void
*******************************************************************************/
void JpegEncGetOverlaySlice(JpegEncInst inst, JpegEncIn *pEncIn,
                            i32 restartInterval, i32 partialCoding, i32 slice,
                            i32 sliceRows, u32 yOffset) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;
  preProcess_s *preProcess = &pEncInst->preProcess;
  u32 i;
  u32 heightMcu = (pEncInst->jpeg.codingMode == JPEGENC_422_MODE ? 8 : 16);
  if ((pEncInst->jpeg.codingType == ENC_PARTIAL_FRAME) && (preProcess->verOffsetSrc[0] > (u32)(slice * pEncInst->jpeg.sliceRows * heightMcu)))
  {
     for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    preProcess->overlayVerOffset[i] = preProcess->overlayCropYoffset[i];
    preProcess->overlaySliceHeight[i] = preProcess->overlayCropHeight[i];
    preProcess->overlaySliceYoffset[i] = preProcess->overlayYoffset[i];
    }
    return;
  }
  if (pEncInst->jpeg.codingType == ENC_PARTIAL_FRAME)
    slice = slice - (yOffset / (u32)(pEncInst->jpeg.sliceRows * heightMcu));
  u32 minY = slice * 16 * restartInterval;
  u32 maxY = minY + sliceRows;

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    preProcess->overlayVerOffset[i] = preProcess->overlayCropYoffset[i];
    preProcess->overlaySliceHeight[i] = preProcess->overlayCropHeight[i];
    preProcess->overlaySliceYoffset[i] = preProcess->overlayYoffset[i];

    //Get slice info
    if (pEncIn->overlayEnable[i] && partialCoding) {
      //We do not do any overlay if region is outside slice
      if (preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] - 1 <
              minY ||
          preProcess->overlayYoffset[i] >= maxY) {
        preProcess->overlayEnable[i] = 0;
        continue;
      }
      preProcess->overlayEnable[i] = 1;

      //Only two case for slice y offset
      preProcess->overlaySliceYoffset[i] = 0;

      /*Only need to chop if part of overlay region is within slice*/
      //First case: Top part; Only need to modify height and slice y offset
      if (preProcess->overlayYoffset[i] >= minY &&
          preProcess->overlayYoffset[i] < maxY &&
          preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] >=
              maxY) {
        preProcess->overlaySliceHeight[i] =
            maxY - preProcess->overlayYoffset[i];
        preProcess->overlaySliceYoffset[i] =
            preProcess->overlayYoffset[i] - minY;
      }
      else if (preProcess->overlayYoffset[i] >= minY &&
          preProcess->overlayYoffset[i] < maxY &&
          preProcess->overlayYoffset[i] + preProcess->overlayCropHeight[i] <
              maxY) {
        preProcess->overlaySliceYoffset[i] =
            preProcess->overlayYoffset[i] - minY;
        preProcess->overlaySliceHeight[i] = preProcess->overlayCropHeight[i];
      }
      //Second case: Middle part; Need to take care of cropping y offset
      else if (preProcess->overlayYoffset[i] <= minY &&
               preProcess->overlayYoffset[i] +
                       preProcess->overlayCropHeight[i] >=
                   maxY) {
        preProcess->overlayVerOffset[i] += minY - preProcess->overlayYoffset[i];
        preProcess->overlaySliceHeight[i] = sliceRows;
      }
      //Third case: bottom part;
      else if (preProcess->overlayYoffset[i] +
                       preProcess->overlayCropHeight[i] <=
                   maxY &&
               preProcess->overlayYoffset[i] <= minY) {
        preProcess->overlayVerOffset[i] += minY - preProcess->overlayYoffset[i];
        preProcess->overlaySliceHeight[i] = preProcess->overlayYoffset[i] +
                                            preProcess->overlayCropHeight[i] -
                                            minY;
      }
    }
  }
}

void JpegEncGetOSDMapSlice(JpegEncInst inst, JpegEncIn *pEncIn,
                           i32 restartInterval, i32 partialCoding,
                           i32 slice, u32 yOffset) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;
  preProcess_s *preProcess = &pEncInst->preProcess;
  if (preProcess->osdMapEnable) {
    u32 heightMcu = (pEncInst->jpeg.codingMode == JPEGENC_422_MODE ? 8 : 16);
    u32 BlockSize = preProcess->osdMapBlockSize;
    if ((pEncInst->jpeg.codingType == ENC_PARTIAL_FRAME) && (preProcess->verOffsetSrc[0] > (u32)(slice * pEncInst->jpeg.sliceRows * heightMcu)))
      return;
    if (pEncInst->jpeg.codingType == ENC_PARTIAL_FRAME)
      slice = slice - (yOffset / (u32)(pEncInst->jpeg.sliceRows * heightMcu));
    if(restartInterval != 0 && partialCoding)
    {
      u32 addressOffset = preProcess->osdMapStride * (heightMcu / BlockSize) * restartInterval;
      if(slice != 0)
      {
        pEncIn->osdMapInputAddr = pEncIn->osdMapInputAddr + addressOffset;
      }
    }
  }
}
#define DCTSIZE2 64

static const unsigned int std_luminance_quant_tbl[DCTSIZE2] = {
    16, 11, 10, 16, 24,  40,  51,  61,  12, 12, 14, 19, 26,  58,  60,  55,
    14, 13, 16, 24, 40,  57,  69,  56,  14, 17, 22, 29, 51,  87,  80,  62,
    18, 22, 37, 56, 68,  109, 103, 77,  24, 35, 55, 64, 81,  104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99};
static const unsigned int std_chrominance_quant_tbl[DCTSIZE2] = {
    17, 18, 24, 47, 99, 99, 99, 99, 18, 21, 26, 66, 99, 99, 99, 99,
    24, 26, 56, 99, 99, 99, 99, 99, 47, 66, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
    99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

/*
 * Quantization table setup routines
 */
static void JpegCalcQuantTable(uint8_t quant_tbl[64],
                                 const unsigned int *basic_table,
                                 int scale_factor, int force_baseline)
/* Define a quantization table equal to the basic_table times
 * a scale factor (given as a percentage).
 * If force_baseline is TRUE, the computed quantization table entries
 * are limited to 1..255 for JPEG baseline compatibility.
 */
{
  int i;
  long temp;

  for (i = 0; i < 64; i++) {
    temp = ((long)basic_table[i] * scale_factor + 50L) / 100L;
    /* limit the values to the valid range */
    if (temp <= 0L) temp = 1L;
    if (temp > 32767L) temp = 32767L; /* max quantizer needed for 12 bits */
    if (force_baseline && temp > 255L)
      temp = 255L; /* limit to baseline range if requested */

    //we support only baseline
    //quant_tbl[i] = (uint16_t) temp;
    quant_tbl[i] = (uint8_t)temp;
  }
  JpegSetQTable(quant_tbl, quant_tbl);
}

static int JpegGetQualityScalingFactor(int quality)
/* Convert a user-specified quality rating to a percentage scaling factor
 * for an underlying quantization table, using our recommended scaling curve.
 * The input 'quality' factor should be 0 (terrible) to 100 (very good).
 */
{
  /* Safety limit on quality factor.  Convert 0 to 1 to avoid zero divide. */
  if (quality <= 0) quality = 1;
  if (quality > 100) quality = 100;

  /* The basic table is used as-is (scaling 100) for a quality of 50.
   * Qualities 50..100 are converted to scaling percentage 200 - 2*Q;
   * note that at Q=100 the scaling is 0, which will cause JpegCalcQuantTable
   * to make all the table entries 1 (hence, minimum quantization loss).
   * Qualities 1..50 are converted to scaling percentage 5000/Q.
   */
  if (quality < 50)
    quality = 5000 / quality;
  else
    quality = 200 - quality * 2;

  return quality;
}

void JpegEncQuantTab(uint8_t quant_div_tbl[64], int quality, int force_baseline,
                     bool bLuma) {
  /* Convert user 0-100 rating to percentage scaling */
  //int scale_factor = JpegGetQualityScalingFactor(quality);

  /* Convert user 0-138 scaling level*/
  const int scale_table[] = {
      1,    2,    3,    4,    5,    6,    7,    8,    9,   10,  11,  12,  13,
      14,   15,   16,   17,   18,   19,   20,   21,   22,  23,  24,  25,  27,
      28,   29,   30,   32,   33,   35,   36,   37,   38,  39,  40,  41,  44,
      45,   46,   47,   48,   50,   53,   55,   57,   60,  62,  64,  66,  68,
      70,   72,   74,   77,   79,   82,   84,   86,   90,  94,  96,  98,  103,
      105,  110,  114,  117,  119,  124,  129,  133,  138, 140, 146, 153, 156,
      160,  164,  168,  174,  179,  185,  189,  196,  204, 208, 212, 219, 223,
      234,  239,  240,  248,  261,  269,  280,  284,  295, 311, 320, 325, 335,
      353,  362,  375,  392,  408,  415,  425,  443,  471, 482, 496, 520, 540,
      563,  572,  609,  635,  654,  687,  727,  755,  797, 842, 897, 955, 986,
      1043, 1130, 1227, 1297, 1412, 1515, 1663, 1930, 2395};
  int scale_factor = scale_table[quality];

  /* Set up standard quality tables */
  JpegCalcQuantTable(
      (uint8_t *)quant_div_tbl,
      bLuma ? std_luminance_quant_tbl : std_chrominance_quant_tbl, scale_factor,
      force_baseline);
}


/* Run HW without block */
JpegEncRet JpegEncEncodeRun(JpegEncInst inst, const JpegEncIn *pEncIn,
                            JpegEncOut *pEncOut) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;
  jpegData_s *jpeg;
  asicData_s *asic;
  preProcess_s *preProcess;
  JpegEncRet ret;
  u32 qp = 0;
  i32 heightMcu;
  u32 buf0MinSize = JPEGENC_STREAM_MIN_BUF0_SIZE;
  i32 i;
  u32 vcmd_en;

  /* Check for illegal inputs */
  if (pEncInst == NULL) {
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR null instance\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  if ((pEncIn == NULL) || (pEncOut == NULL)) {
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR null arguments\n");
    ret = JPEGENC_NULL_ARGUMENT;
    goto enc_err;
  }

  /* Check for existing instance */
  if (pEncInst->inst != pEncInst) {
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR Invalid instance\n");
    ret = JPEGENC_INSTANCE_ERROR;
    goto enc_err;
  }

  /* Check for features not compiled.*/
#ifndef VCMD_BUILD_SUPPORT
  if (pEncInst->asic.regs.bVCMDAvailable != 0) {
    APITRACE_ERR((void *)inst,
                 "JpegEncEncode: VCMD not supported at compile time.\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }
#endif
#if !defined (SUPPORT_DEC400) && !defined (SUPPORT_UFBC)
  if (pEncIn->dec400Enable != 0) /**< \brief 1: bypass 2: enable */
  {
    APITRACE_ERR((void *)inst,
                 "JpegEncEncode: DEC400 not supported at compile time.\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }
#endif
#ifndef LOW_LATENCY_BUILD_SUPPORT
  if (pEncInst->inputLineBuf.inputLineBufEn != 0) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncEncode: inputLineBuffer not supported at compile time.\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }
#endif
#ifndef RATE_CONTROL_BUILD_SUPPORT
  if (pEncInst->rateControl.picRc != 0) {
    APITRACE_ERR((void *)inst,
                 "JpegEncEncode: rateControl not supported at compile time.\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }
#endif

  asic = &pEncInst->asic;
  jpeg = &pEncInst->jpeg;
  preProcess = &pEncInst->preProcess;
  if (jpeg->appn.thumbEnable) buf0MinSize += jpeg->thumbnail.dataLength;

#ifndef LOW_LATENCY_SLICEINFO_SUPPORT
  if (pEncInst->sliceinfoEn != 0) {
      APITRACEERR(
          "JpegApi: inputSliceInfo not supported at compile time.\n");
      return ENCHW_NOK;
  }
#endif
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  if (!asic->regs.asicCfg->prpLowlatencySignalbyDDR) {
    APITRACEERR(
         "VCEncStrmEncode: HW not support ddr low latency.\n");
    return VCENC_ERROR;
  }
  if (pEncInst->inputLineBuf.inputLineBufEn) {
    APITRACEERR(
         "If enable linebuffer mode, low latency not use polling sliceinfo");
    return ENCHW_NOK;
  }
  if (pEncInst->sliceinfoEn && preProcess->rotation != 0) {
      APITRACEERR(
          "JpegApi: inputSliceInfo NOT support PRP rotation currently.\n");
      return ENCHW_NOK;
  }
#endif

  /* Check for invalid input values */
  if (pEncIn->pOutBuf[0] == NULL) {
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR Invalid output buffer0\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }

  if (pEncIn->outBufSize[0] < buf0MinSize &&
      pEncInst->streamMultiSegment.streamMultiSegmentMode == 0) {
    APITRACE_ERR((void *)inst,
                 "JpegEncEncode: ERROR Too small output buffer0\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }

  if ((pEncIn->outBufSize[0] <
       pEncInst->streamMultiSegment.streamMultiSegmentSize *
           pEncInst->streamMultiSegment.streamMultiSegmentAmount) &&
      pEncInst->streamMultiSegment.streamMultiSegmentMode == 3) {
    APITRACE_ERR((void *)inst,
                 "JpegEncEncode: ERROR Too small output buffer0\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }

  if (pEncIn->outBufSize[1] &&
      ((asic->regs.asicCfg->streamBufferChain == 0) ||
       pEncInst->streamMultiSegment.streamMultiSegmentMode != 0)) {
    APITRACE_ERR((void *)inst,
                 "JpegEncEncode: ERROR Two stream buffer not supported\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }

  if (pEncIn->outBufSize[1] && (pEncIn->pOutBuf[1] == NULL)) {
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR Invalid output buffer1\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }

  /* Clear the output structure */
  pEncOut->jfifSize = 0;

  /* todo: check that thumbnail fits also */
  /* Set stream buffer, the size has been checked */
  if (EncJpegSetBuffer(&pEncInst->stream, (u8 *)pEncIn->pOutBuf[0],
                       (u32)pEncIn->outBufSize[0]) == ENCHW_NOK) {
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR Invalid output buffer\n");
    ret = JPEGENC_INVALID_ARGUMENT;
    goto enc_err;
  }

  /* Setup input line buffer */
  pEncInst->inputLineBuf.wrCnt = pEncIn->lineBufWrCnt;
  pEncInst->inputLineBuf.initSegNum = pEncIn->initSegNum;

  /* Set ASIC input image */
  asic->regs.inputLumBase = pEncIn->busLum;
  asic->regs.inputCbBase = pEncIn->busCb;
  asic->regs.inputCrBase = pEncIn->busCr;

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  /* Set sliceinfo buffer */
  asic->regs.SliceInfoBase = pEncIn->sliceinfoBus;
#endif

  //RoiMap
  if (pEncIn->busRoiMap == 0 || pEncIn->filter == NULL)
    asic->regs.roiEnable = 0;
  else {
    asic->regs.busRoiMap = pEncIn->busRoiMap;
    asic->regs.roiEnable = 1;
  }
  asic->regs.pixelsOnRow = (u32)preProcess->lumWidthSrc[0];
  asic->regs.jpegMode = (jpeg->codingMode == JPEGENC_420_MODE) ? 0 : 1;
  asic->regs.ljpegFmt = jpeg->codingMode;
  for (i = 0; i < MAX_STRM_BUF_NUM; i++) {
    asic->regs.outputStrmSize[i] = pEncIn->outBufSize[i];
    asic->regs.outputStrmBase[i] = pEncIn->busOutBuf[i];
    // for segment mode != 0, identify mode=3 and mode1,2 using strmBase != 0
    if (pEncInst->streamMultiSegment.streamMultiSegmentMode == 1 ||
        pEncInst->streamMultiSegment.streamMultiSegmentMode == 2) {
      asic->regs.outputStrmBase[i] = pEncIn->busOutBuf[0];
    }
  }

  /*stream multi-segment*/
  asic->regs.streamMultiSegEn =
      pEncInst->streamMultiSegment.streamMultiSegmentMode != 0;
  asic->regs.streamMultiSegSWSyncEn =
      pEncInst->streamMultiSegment.streamMultiSegmentMode == 2;
  asic->regs.streamMultiSegRD = 0;
  asic->regs.streamMultiSegIRQEn = 0;
  pEncInst->streamMultiSegment.rdCnt = 0;
  if (asic->regs.streamMultiSegEn) {
    if (pEncInst->streamMultiSegment.streamMultiSegmentMode == 3) {
      asic->regs.streamMultiSegSize =
          pEncInst->streamMultiSegment.streamMultiSegmentSize;
      asic->regs.streamMultiSegIRQEn = 1;
    } else {
      //make sure the stream size is equal to N*segment size
      asic->regs.streamMultiSegSize =
          asic->regs.outputStrmSize[0] /
          pEncInst->streamMultiSegment.streamMultiSegmentAmount;
      asic->regs.streamMultiSegSize =
          ((asic->regs.streamMultiSegSize + 16 - 1) &
           (~(16 - 1)));  //segment size must be aligned to 16byte
      asic->regs.outputStrmSize[0] =
          asic->regs.streamMultiSegSize *
          pEncInst->streamMultiSegment.streamMultiSegmentAmount;
      asic->regs.streamMultiSegIRQEn = 1;
    }
    APITRACE_INFO((void *)inst, "segment size = %d\n",
                  asic->regs.streamMultiSegSize);
  }

  heightMcu = (jpeg->codingMode == JPEGENC_422_MODE ? 8 : 16);

  /* slice/restart information */
  if (jpeg->codingType == ENC_WHOLE_FRAME) {
    asic->regs.jpegSliceEnable = 0;
    asic->regs.jpegRestartInterval = jpeg->rstMbRows;
    asic->regs.jpegRestartMarker = 0;
  } else {
    asic->regs.jpegSliceEnable = 1;
    //asic->regs.jpegRestartInterval = 0;
    asic->regs.jpegRestartInterval = jpeg->rstMbRows;
  }

  /* Check if this is start of a new frame */
  if (jpeg->sliceNum == 0) {
    jpeg->mbNum = 0;
    /*jpeg->column = 0;*/
    jpeg->row = 0;
  }

  /* For sliced frame, check if this slice should be encoded.
     * Vertical offset is multiple of slice height */
  if ((jpeg->codingType == ENC_PARTIAL_FRAME) &&
      (preProcess->verOffsetSrc[0] >
       (u32)(jpeg->sliceNum * jpeg->sliceRows * heightMcu))) {
    jpeg->sliceNum++;
    APITRACE_INFO((void *)inst, "JpegEncEncode: OK  restart interval\n");
    return JPEGENC_RESTART_INTERVAL;
  }

  if (pEncInst->rateControl.picRc || pEncInst->fixedQP != -1) {
    if (pEncInst->rateControl.picRc) {
      JpegEncBeforePicRc(&pEncInst->rateControl, pEncInst->timeIncrement,
                         I_SLICE, NULL);
      jpeg->qp = CLIP3(0, 138,
                       (((pEncInst->rateControl.qpHdr * 138 + 25) / 51) >>
                        JPEG_QP_FRACTIONAL_BITS));
    } else if (pEncInst->fixedQP != -1) {
      pEncInst->rateControl.qpHdr = pEncInst->fixedQP
                                    << JPEG_QP_FRACTIONAL_BITS;
      jpeg->qp = CLIP3(
          0, 138,
          ((((pEncInst->fixedQP << JPEG_QP_FRACTIONAL_BITS) * 138 + 25) / 51) >>
           JPEG_QP_FRACTIONAL_BITS));
    }

    /* Set parameters depending on user config */
    /* Choose quantization tables */
    JpegEncQuantTab(pEncInst->jpeg.qTableLuma, jpeg->qp, HANTRO_TRUE,
                    HANTRO_TRUE);
    JpegEncQuantTab(pEncInst->jpeg.qTableChroma, jpeg->qp, HANTRO_TRUE,
                    HANTRO_FALSE);
#if 1
    APITRACE_INFO((void *)inst, "qTableLuma\n");
    for(int i=0; i<64; i+=8)
      APITRACE_INFO((void *)inst, " %3d %3d %3d %3d %3d %3d %3d %3d\n", pEncInst->jpeg.qTableLuma[i],
        pEncInst->jpeg.qTableLuma[i+1], pEncInst->jpeg.qTableLuma[i+2],
        pEncInst->jpeg.qTableLuma[i+3], pEncInst->jpeg.qTableLuma[i+4],
        pEncInst->jpeg.qTableLuma[i+5], pEncInst->jpeg.qTableLuma[i+6],
        pEncInst->jpeg.qTableLuma[i+7]);
    APITRACE_INFO((void *)inst, "qTableChroma\n");
    for(int i=0; i<64; i+=8)
      APITRACE_INFO((void *)inst, " %3d %3d %3d %3d %3d %3d %3d %3d\n", pEncInst->jpeg.qTableChroma[i],
        pEncInst->jpeg.qTableChroma[i+1], pEncInst->jpeg.qTableChroma[i+2],
        pEncInst->jpeg.qTableChroma[i+3], pEncInst->jpeg.qTableChroma[i+4],
        pEncInst->jpeg.qTableChroma[i+5], pEncInst->jpeg.qTableChroma[i+6],
        pEncInst->jpeg.qTableChroma[i+7]);
#endif

    pEncInst->jpeg.qTable.pQlumi = pEncInst->jpeg.qTableLuma;
    pEncInst->jpeg.qTable.pQchromi = pEncInst->jpeg.qTableChroma;

    /* Copy quantization tables to ASIC internal memories */
    EncAsicSetQuantTable(&pEncInst->asic, pEncInst->jpeg.qTable.pQlumi,
                         pEncInst->jpeg.qTable.pQchromi);
    if (asic->regs.roiEnable)
      JpegEncSetNonRoi(inst, pEncIn->filter);
  }

  /* set the rst value for HW if RST wanted */
  if (jpeg->restart.Ri) {
    i32 rstCount = jpeg->rstCount;
    if (jpeg->losslessEn) {
      //when heighMcu = 2 => mcuInMb = 16/2=8
      if (jpeg->codingMode == JPEGENC_MONO_MODE) {
        rstCount = (jpeg->rstCount * 16) % 8;
      } else {
        rstCount = (jpeg->rstCount * 8) % 8;
      }
    } else {
      if (jpeg->codingMode == JPEGENC_MONO_MODE) {
        rstCount = (jpeg->rstCount * 2) % 8;
      }
    }

    switch (rstCount) {
      case 0:
        asic->regs.jpegRestartMarker = RST0;
        break;
      case 1:
        asic->regs.jpegRestartMarker = RST1;
        break;
      case 2:
        asic->regs.jpegRestartMarker = RST2;
        break;
      case 3:
        asic->regs.jpegRestartMarker = RST3;
        break;
      case 4:
        asic->regs.jpegRestartMarker = RST4;
        break;
      case 5:
        asic->regs.jpegRestartMarker = RST5;
        break;
      case 6:
        asic->regs.jpegRestartMarker = RST6;
        break;
      case 7:
        asic->regs.jpegRestartMarker = RST7;
        break;
      default:
        ASSERT(0);
    }

    APITRACE_INFO((void *)inst, "RST=%x\n", asic->regs.jpegRestartMarker);
    jpeg->rstCount++;

    if (jpeg->rstCount > 7) jpeg->rstCount = 0;
  }

  if (preProcess->overlayEnable[0] != 0) {
    asic->regs.overlayScaleStepW =
        (u16)((double)(preProcess->overlayCropWidth[0] << 16) /
              preProcess->overlayScaleWidth[0]);
    asic->regs.overlayScaleStepH =
        (u16)((double)(preProcess->overlayCropHeight[0] << 16) /
              preProcess->overlayScaleHeight[0]);
  } else {
    asic->regs.overlayScaleStepW = 0;
    asic->regs.overlayScaleStepH = 0;
  }

  for (int i = 0; i < MAX_OVERLAY_NUM; i++) {
    preProcess->overlayInputYAddr[i] = pEncIn->busOlLum[i];
    preProcess->overlayInputUAddr[i] = pEncIn->busOlCb[i];
    preProcess->overlayInputVAddr[i] = pEncIn->busOlCr[i];
  }
  asic->regs.mosSizeIndex = preProcess->mosSizeIndex;

  if (jpeg->codingType == ENC_WHOLE_FRAME) {
    /* Adjust ASIC input image with pre-processing */
    preProcess->sliced_frame = 0;
    EncPreProcess(asic, preProcess, pEncInst->ctx, 0);
  } else {
    /* Set frame dimensions in slice mode for pre-processing */
    if ((jpeg->row + jpeg->sliceRows) <= (jpeg->height / heightMcu)) {
      preProcess->lumHeight = (heightMcu * jpeg->sliceRows);
    } else {
      preProcess->lumHeight = (jpeg->height % (jpeg->sliceRows * heightMcu));
    }

    if (preProcess->inputFormat !=
        JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4)
      preProcess->verOffsetSrc[0] = 0;
    else {
      preProcess->verOffsetSrc[0] =
          jpeg->sliceNum * jpeg->sliceRows * heightMcu;
      preProcess->sliced_frame = 1;
    }

    /* Check if we need to update height for last slice (for ASIC) */
    if (((jpeg->height / heightMcu) - jpeg->row) < jpeg->sliceRows) {
      asic->regs.mbsInCol =
          (((jpeg->height + heightMcu - 1) / heightMcu) - jpeg->row);
    }

    /* enable EOI writing in last slice */
    if ((jpeg->row + jpeg->sliceRows) >=
        ((jpeg->height + heightMcu - 1) / heightMcu)) {
      asic->regs.jpegSliceEnable = 0;
    }

    /* Adjust ASIC input image with pre-processing */
    EncPreProcess(asic, preProcess, pEncInst->ctx, 0);
  }

  /* OSD_MAP region(reuse OSD registers)  to  do*/
    if (pEncIn->osdMapEnable) {
    i= MAX_OVERLAY_NUM - 1;
    int j = MAX_OVERLAY_NUM - 1;
    for (j = MAX_OVERLAY_NUM - 1; j >= 0; j--) {
      if (preProcess->overlayEnable[j] ||
        preProcess->mosEnable[j]) {
        if (preProcess->mosEnable[j]) {
          asic->regs.overlayEnable[i] = preProcess->mosEnable[j];
          asic->regs.overlayFormat[i] = 3;
          asic->regs.overlayXoffset[i] = preProcess->mosXoffset[j];
          asic->regs.overlayYoffset[i] = preProcess->mosYoffset[j];
          asic->regs.overlayWidth[i] = preProcess->mosWidth[j];
          asic->regs.overlayHeight[i] = preProcess->mosHeight[j];
        } else {
          //Offsets and cropping are handled in software prp, so we don't need to adjust them here
          asic->regs.overlayYAddr[i] = preProcess->overlayInputYAddr[j];
        asic->regs.overlayUAddr[i] = preProcess->overlayInputUAddr[j];
        asic->regs.overlayVAddr[i] = preProcess->overlayInputVAddr[j];
          asic->regs.overlayEnable[i] =
              preProcess->overlayEnable[j] && pEncIn->overlayEnable[j];
          asic->regs.overlayFormat[i] = preProcess->overlayFormat[j];
          asic->regs.overlayAlpha[i] = preProcess->overlayAlpha[j];
          asic->regs.overlayXoffset[i] = preProcess->overlayXoffset[j];
          asic->regs.overlayYoffset[i] = preProcess->overlaySliceYoffset[j];
          asic->regs.overlayWidth[i] = preProcess->overlayWidth[j];
          asic->regs.overlayHeight[i] = preProcess->overlayHeight[j];
          asic->regs.overlayYStride[i] = preProcess->overlayYStride[j];
          asic->regs.overlayUVStride[i] =
              preProcess->overlayUVStride[j];
          asic->regs.overlayBitmapY[i] = preProcess->overlayBitmapY[j];
          asic->regs.overlayBitmapU[i] = preProcess->overlayBitmapU[j];
          asic->regs.overlayBitmapV[i] = preProcess->overlayBitmapV[j];
        }
        i--;
      }
    }
  } else {
      for (i = 0; i < MAX_OVERLAY_NUM; i++) {
      /* Reuse OSD registers */
      if (preProcess->mosEnable[i]) {
        asic->regs.overlayEnable[i] = preProcess->mosEnable[i];
        asic->regs.overlayFormat[i] = 3;
        asic->regs.overlayXoffset[i] = preProcess->mosXoffset[i];
        asic->regs.overlayYoffset[i] = preProcess->mosYoffset[i];
        asic->regs.overlayWidth[i] = preProcess->mosWidth[i];
        asic->regs.overlayHeight[i] = preProcess->mosHeight[i];
      } else {
        //Offsets and cropping are handled in software prp, so we don't need to adjust them here
        asic->regs.overlayYAddr[i] = preProcess->overlayInputYAddr[i];
        asic->regs.overlayUAddr[i] = preProcess->overlayInputUAddr[i];
        asic->regs.overlayVAddr[i] = preProcess->overlayInputVAddr[i];
        asic->regs.overlayEnable[i] =
            preProcess->overlayEnable[i] && pEncIn->overlayEnable[i];
        asic->regs.overlayFormat[i] = preProcess->overlayFormat[i];
        asic->regs.overlayAlpha[i] = preProcess->overlayAlpha[i];
        asic->regs.overlayXoffset[i] = preProcess->overlayXoffset[i];
        asic->regs.overlayYoffset[i] = preProcess->overlaySliceYoffset[i];
        asic->regs.overlayWidth[i] = preProcess->overlayWidth[i];
        asic->regs.overlayHeight[i] = preProcess->overlayHeight[i];
        asic->regs.overlayYStride[i] = preProcess->overlayYStride[i];
        asic->regs.overlayUVStride[i] =
            preProcess->overlayUVStride[i];
        asic->regs.overlayBitmapY[i] = preProcess->overlayBitmapY[i];
        asic->regs.overlayBitmapU[i] = preProcess->overlayBitmapU[i];
        asic->regs.overlayBitmapV[i] = preProcess->overlayBitmapV[i];
      }
    }
  }

  if (preProcess->overlaySuperTile[0]) {
    //To use 20 bit reg support 4k, times 64 in Cmodel
    asic->regs.overlayYStride[0] = preProcess->overlayYStride[0] / 64;
    asic->regs.overlayUVStride[0] = preProcess->overlayUVStride[0] / 64;
  }
  asic->regs.overlaySuperTile = preProcess->overlaySuperTile[0];
  asic->regs.overlayScaleWidth = preProcess->overlayScaleWidth[0];
  asic->regs.overlayScaleHeight = preProcess->overlayScaleHeight[0];

  if (pEncIn->osdMapEnable) {
    if (!asic->regs.overlayEnable[0]) {
      asic->regs.osdMapEnable = pEncIn->osdMapEnable; //osdMap disable if osd region full.
      asic->regs.overlayYAddr[0] = pEncIn->osdMapInputAddr;
      asic->regs.overlayYStride[0] = preProcess->osdMapStride; //pixel stride
      asic->regs.osdMapBlockType = preProcess->osdMapBlockSize == 4 ? 1 : 0; //pixel stride
      for (i = 0; i < MAX_OSDMAP_COLOR_NUM; i++) {
        if (asic->regs.overlayEnable[i])
          break;
        /* Reuse OSD small index region registers */
        asic->regs.overlayAlpha[i] = preProcess->osdMapAlpha[i];
        asic->regs.overlayBitmapY[i] = preProcess->osdMapY[i];
        asic->regs.overlayBitmapU[i] = preProcess->osdMapU[i];
        asic->regs.overlayBitmapV[i] = preProcess->osdMapV[i];
      }
    }
  }

  asic->regs.picWidth = asic->regs.mbsInRow * 16;
  asic->regs.picHeight = asic->regs.mbsInCol * heightMcu;
#ifdef SUPPORT_UFBC
  EncUfbcParam ufbc_param;
  ufbc_param.format = preProcess->inputFormat;
  ufbc_param.width = preProcess->lumWidthSrc[0];
  ufbc_param.height = preProcess->lumHeightSrc[0];
  ufbc_param.xOffset = preProcess->horOffsetSrc[0];
  ufbc_param.yOffset = preProcess->verOffsetSrc[0];
  ufbc_param.alignment = preProcess->input_alignment;
  ufbc_param.mode = pEncInst->ufbcParam.mode;
  ufbc_param.vcmd = &asic->regs.vcmd;
  ufbc_param.baseAddress[0] = pEncIn->busLum;
  ufbc_param.baseAddress[1] = pEncIn->busCb;
  ufbc_param.baseAddress[2] = pEncIn->busCr;
  if (pEncInst->ufbcParam.mode == UFBC_MODE_DEC400) {
    ufbc_param.param.dec400.headerAddress[0] = pEncIn->dec400TableBusLum;
    ufbc_param.param.dec400.headerAddress[1] = pEncIn->dec400TableBusCb;
    ufbc_param.param.dec400.tileSize = UFBC_TILE_SIZE;
  }  else if (pEncInst->ufbcParam.mode == UFBC_MODE_AFBC_0 || pEncInst->ufbcParam.mode == UFBC_MODE_AFBC_1) {
    ufbc_param.param.afbc.yuvTrans = pEncInst->ufbcParam.param.afbc.yuvTrans;
    ufbc_param.param.afbc.blockType = pEncInst->ufbcParam.param.afbc.blockType;
    ufbc_param.param.afbc.blockSplit = pEncInst->ufbcParam.param.afbc.blockSplit;
  } else if (pEncInst->ufbcParam.mode == UFBC_MODE_PVRIC) {
    ufbc_param.param.pvric.blockType = pEncInst->ufbcParam.param.pvric.blockType;
    for (u32 i = 0; i<4; i++) {
      ufbc_param.param.pvric.consColorVal[i] = pEncInst->ufbcParam.param.pvric.consColorVal[i];
    }
  }

  asic->ufbc = &pEncInst->ufbc;
  if (EncUfbcSetParams(asic->ufbc, &ufbc_param)) {
    return VCENC_INVALID_ARGUMENT;
  }

#ifdef SYSTEM_BUILD
#ifdef TEST_DATA
  if (pEncInst->ufbcParam.mode) {
    extern i32 trace_cam_data_ufbc(ptr_t busLuma, ptr_t busChromaU,
                                   ptr_t busChromaV, u32 format,
                                   u32 luma_stride, u32 chroma_stride,
                                   u32 pic_width, u32 pic_height,
                                   u32 height_stride, u32 picNum);
    int asic_format = EncPreGetHwFormat(preProcess->inputFormat);
    u32 picCnt = 0, height = 0;
    if (jpeg->codingType == ENC_PARTIAL_FRAME)
    {
      picCnt = pEncInst->jpeg.sliceNum;
      height = asic->regs.jpegRestartInterval * heightMcu;
    } else {
      picCnt = pEncIn->picCnt;
      height = preProcess->lumHeightSrc[0];
    }
    trace_cam_data_ufbc(pEncIn->busLum, pEncIn->busCb, pEncIn->busCr,
                        asic_format, asic->regs.input_luma_stride,
                        asic->regs.input_chroma_stride,
                        preProcess->lumWidthSrc[0], height,
                        ufbc_param.height, picCnt);

  }
#endif
#endif /* TEST_DATA */
#endif

  /* Enable/disable jfif header generation */
  if (pEncIn->frameHeader)
    jpeg->frame.header = ENCHW_YES;
  else
    jpeg->frame.header = ENCHW_NO;

  u32 is_dec400_in_work_mode = 0;
  u32 height_dec400 = 0;
  is_dec400_in_work_mode = ((pEncIn->dec400Enable & 0xF) == 2) ? 1 : 0;
  pEncInst->dec400_data.streams[0].pix_fmt = preProcess->inputFormat;
  pEncInst->dec400_data.streams[0].width = preProcess->lumWidthSrc[0];
  if (jpeg->codingType == ENC_PARTIAL_FRAME)
  {
    height_dec400 = asic->regs.jpegRestartInterval * heightMcu;
  } else {
    height_dec400 = preProcess->lumHeightSrc[0];
  }
  pEncInst->dec400_data.streams[0].height = height_dec400;
  pEncInst->dec400_data.input_alignment = preProcess->input_alignment;
  pEncInst->dec400_data.streams[0].table_base[0] = pEncIn->dec400TableBusLum;
  pEncInst->dec400_data.streams[0].table_base[1] = pEncIn->dec400TableBusCb;
  pEncInst->dec400_data.streams[0].table_base[2] = pEncIn->dec400TableBusCr;
  pEncInst->dec400_data.streams[0].dec400Enable = pEncIn->dec400Enable;
  pEncInst->dec400_data.dec400Enable = pEncIn->dec400Enable;
  pEncInst->dec400_data.vcmd = &asic->regs.vcmd;

  pEncInst->dec400_data.ewl_inst = asic->ewl;
  pEncInst->dec400_data.streams[0].data_base[2] = pEncIn->busCr;
  pEncInst->dec400_data.streams[0].data_base[1] = pEncIn->busCb;
  pEncInst->dec400_data.streams[0].data_base[0] = pEncIn->busLum;
  pEncInst->dec400_data.streams[0].super_tile = preProcess->scanType;
  pEncInst->dec400_data.streams[0].fastclearEnable[0] = pEncIn->fcEnable[0];
  pEncInst->dec400_data.streams[0].fastclearEnable[1] = pEncIn->fcEnable[1];
  pEncInst->dec400_data.streams[0].fastclearEnable[2] = pEncIn->fcEnable[2];
  pEncInst->dec400_data.streams[0].fastclear_val[0] = pEncIn->clearColorLow[0];
  pEncInst->dec400_data.streams[0].fastclear_val[1] = pEncIn->clearColorLow[1];
  pEncInst->dec400_data.streams[0].fastclear_val[2] = pEncIn->clearColorLow[2];

  if (pEncIn->dec400TSHeaderEnable) {
    pEncInst->dec400_data.streams[0].table_base[0] =
      (u32)pEncInst->dec400_data.streams[0].table_base[0] + DEC400_HEADER_BUF_SIZE;
    pEncInst->dec400_data.streams[0].table_base[1] =
      (u32)pEncInst->dec400_data.streams[0].table_base[1] + DEC400_HEADER_BUF_SIZE;
    pEncInst->dec400_data.streams[0].table_base[2] =
      (u32)pEncInst->dec400_data.streams[0].table_base[2] + DEC400_HEADER_BUF_SIZE;
  }

  /* OSD dec400 setting */
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    pEncInst->dec400_data.streams[i + 1].pix_fmt = preProcess->overlayFormat[i];
    pEncInst->dec400_data.streams[i + 1].width = preProcess->overlayWidth[i];
    pEncInst->dec400_data.streams[i + 1].height = preProcess->overlayHeight[i];
    pEncInst->dec400_data.streams[i + 1].table_base[0] = pEncIn->osdDec400TableBase[i][0];
    pEncInst->dec400_data.streams[i + 1].table_base[1] = pEncIn->osdDec400TableBase[i][1];
    pEncInst->dec400_data.streams[i + 1].table_base[2] = pEncIn->osdDec400TableBase[i][2];
    pEncInst->dec400_data.streams[i + 1].dec400Enable = pEncIn->osdDec400Enable[i];
    pEncInst->dec400_data.streams[i + 1].is_osd = 1;
    pEncInst->dec400_data.streams[i + 1].data_base[2] = asic->regs.overlayVAddr[i];
    pEncInst->dec400_data.streams[i + 1].data_base[1] = asic->regs.overlayUAddr[i];
    pEncInst->dec400_data.streams[i + 1].data_base[0] = asic->regs.overlayYAddr[i];
    pEncInst->dec400_data.streams[i + 1].super_tile = preProcess->overlaySuperTile[i];

    if (!is_dec400_in_work_mode && (pEncIn->osdDec400Enable[i] & 0xF) == 2)
      is_dec400_in_work_mode = 1;
  }
  if (is_dec400_in_work_mode)
    pEncInst->dec400_data.dec400Enable = pEncInst->dec400_data.dec400Enable | 0x2;;

  asic->dec400_data = &pEncInst->dec400_data;

#ifdef SUPPORT_AXIFE
  pEncInst->axiFEEnable = pEncIn->axiFEEnable;
  JpegEncCfgAxiFe(pEncInst);
#endif
  vcmd_en = EWLGetVCMDSupport(pEncInst->asic.ewl);
  if (vcmd_en == 0) {
    /* Check if HW resource is available */
    if (EWLReserveHw(pEncInst->asic.ewl, &pEncInst->reserve_core_info, NULL) ==
        EWL_ERROR) {
      APITRACE_ERR((void *)inst,
                   "JpegEncEncode: ERROR hw resource unavailable\n");
      ret = JPEGENC_HW_RESERVED;
      goto enc_err;
    }

    //dec400
    if (VCEncSetDec400(asic) != VCENC_OK) {
      EWLReleaseHw(asic->ewl);
      return JPEGENC_INVALID_ARGUMENT;
    }

    //AXIFE
#ifdef SUPPORT_AXIFE
    if (asic->axife_data->mode) {
      VCEncAxiFeEnable(asic->axife_data);
    }
#endif

  //ufbc
    if(asic->ufbc->has_ufbc) {
      EncUfbcAsicStart(asic->ufbc);
    }
  }

  /* Encode one image or one slice */
  if (EncJpegCodeFrameRun(pEncInst) == JPEGENCODE_INVALID_ARGUMENT) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncEncodeRun: DEC400 doesn't exist or format not supported\n");
    if (vcmd_en == 0) {
      EWLReleaseHw(pEncInst->asic.ewl);

    } else {
#ifdef VCMD_BUILD_SUPPORT
      EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
#endif
    }

    return JPEGENC_INVALID_ARGUMENT;
  }
  pEncOut->invalidBytesInBuf0Tail = pEncInst->invalidBytesInBuf0Tail;

  return JPEGENC_OK;

enc_err:
  hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
  return ret;
}

/* Wait HW status */
JpegEncRet JpegEncEncodeWait(JpegEncInst inst, JpegEncOut *pEncOut) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;
  jpegData_s *jpeg;
  asicData_s *asic;
  jpegEncodeFrame_e ret;

  /* Encode one image or one slice */
  ret = EncJpegCodeFrameWait(pEncInst);

  if (ret != JPEGENCODE_OK) {
    /* Error has occured and the frame is invalid */
    JpegEncRet to_user;

    /* Error has occured and the image is invalid.
         * The image size is passed to the user and can be used for debugging */
    pEncOut->jfifSize = (i32)pEncInst->stream.byteCnt;

    switch (ret) {
      case JPEGENCODE_TIMEOUT:
        APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR HW timeout\n");
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
        if (pEncInst->sliceinfo_status & ASIC_STATUS_POLL_SLICEINFO_TIMEOUT){
          APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR HW polling sliceinfo timeout\n");
          to_user = JPEGENC_HW_SLICEINFO_TIMEOUT;
          break;
        }
#endif
#ifdef SUPPORT_UFBC
        if (pEncInst->ufbc_status & ASIC_STATUS_UFBC_DEC_ERR){
          APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR HW ufbc timeout\n");
          to_user = JPEGENC_HW_UFBC_ERROR;
          break;
        }
        if (pEncInst->ufbc_status & ASIC_STATUS_UFBC_CFG_ERR){
          APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR ufbc config error\n");
          to_user = JPEGENC_HW_UFBC_ERROR;
          break;
        }
#endif
        to_user = JPEGENC_HW_TIMEOUT;
        break;
      case JPEGENCODE_HW_RESET:
        APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR HW reset detected\n");
        to_user = JPEGENC_HW_RESET;
        break;
      case JPEGENCODE_HW_ERROR:
        APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR HW failure\n");
        to_user = JPEGENC_HW_BUS_ERROR;
        break;
      case JPEGENCODE_SYSTEM_ERROR:
      default:
        /* System error has occured, encoding can't continue */
        pEncInst->encStatus = ENCSTAT_ERROR;
        APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR Fatal system error\n");
        to_user = JPEGENC_SYSTEM_ERROR;
    }

    hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
    return to_user;
  }

  asic = &pEncInst->asic;
  jpeg = &pEncInst->jpeg;

  /* Store the stream size in output structure */
  pEncOut->jfifSize = (i32)pEncInst->stream.byteCnt;
  pEncOut->headerSize = asic->regs.jpegHeaderLength;

  /* Check for stream buffer overflow */
  if (pEncInst->stream.overflow == ENCHW_YES) {
    /* The rest of the frame is lost */
    jpeg->sliceNum = 0;
    APITRACE_ERR((void *)inst, "JpegEncEncode: ERROR stream buffer overflow\n");
    hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
    return JPEGENC_OUTPUT_BUFFER_OVERFLOW;
  }

  if (pEncInst->rateControl.picRc) {
    JpegEncAfterPicRc(&pEncInst->rateControl, 0, pEncInst->stream.byteCnt,
                      jpeg->qp, 1);
    if (pEncInst->rateControl.codingType == ASIC_MJPEG)
      pEncInst->timeIncrement = pEncInst->rateControl.outRateDenom;
  }

  hash_reset(&pEncInst->jpeg.hashctx, asic->regs.hashval,
             asic->regs.hashoffset);

  /* Check if this is end of slice or end of frame */
  if (jpeg->mbNum < jpeg->mbPerFrame) {
    jpeg->sliceNum++;
    APITRACE_INFO((void *)inst, "JpegEncEncode: OK  restart interval\n");
    return JPEGENC_RESTART_INTERVAL;
  }
  asic->regs.hashval = hash_finalize(&pEncInst->jpeg.hashctx);
  hash_init(&pEncInst->jpeg.hashctx, pEncInst->jpeg.hashctx.hash_type);
  jpeg->sliceNum = 0;
  jpeg->rstCount = 0;

  APITRACE_INFO((void *)inst, "JpegEncEncode: OK  frame ready\n");
  if (asic->regs.hashtype == 1) {
    APITRACE_INFO((void *)inst, "crc32 %08x\n", asic->regs.hashval);
  } else if (asic->regs.hashtype == 2) {
    APITRACE_INFO((void *)inst, "checksum %08x\n", asic->regs.hashval);
  }
  return JPEGENC_FRAME_READY;
}

/*******************************************************************************
 Function name : JpegEncEncode
 Description   :
 Return type   : JpegEncRet
 Argument      : JpegEncInst inst
 Argument      : JpegEncIn * pEncIn
 Argument      : JpegEncOut *pEncOut
*******************************************************************************/
JpegEncRet JpegEncEncode(JpegEncInst inst, const JpegEncIn *pEncIn,
                         JpegEncOut *pEncOut) {
  JpegEncRet ret;

  APITRACE_INFO((void *)inst, "JpegEncEncode#\n");
  ret = JpegEncEncodeRun(inst, pEncIn, pEncOut);

  if (ret != JPEGENC_OK) {
    return ret;
  }

  return JpegEncEncodeWait(inst, pEncOut);
}

/*******************************************************************************
 Function name : JpegEncGetBitsPerPixel
 Description   : Returns the amount of bits per pixel for given format.
 Return type   : u32 bitsPerPixel
 Argument      : JpegEncFrameType
*******************************************************************************/
u32 JpegEncGetBitsPerPixel(JpegEncFrameType type) {
  switch (type) {
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YVU420_PLANAR:
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32:
    case JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4:
      return 12;
    case JPEGENC_YUV420_I010:
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010:
      return 24;
    case JPEGENC_YUV422_INTERLEAVED_YUYV:
    case JPEGENC_YUV422_INTERLEAVED_UYVY:
    case JPEGENC_YUV422_INTERLEAVED_YVYU:
    case JPEGENC_YUV422_INTERLEAVED_VYUY:
    case JPEGENC_RGB565:
    case JPEGENC_BGR565:
    case JPEGENC_RGB555:
    case JPEGENC_BGR555:
    case JPEGENC_RGB444:
    case JPEGENC_BGR444:
    case JPEGENC_YUV422SP_888:
    case JPEGENC_YVU422SP_888:
    case JPEGENC_Y10bWL:
    case JPEGENC_Y10bWH:
    case JPENC_YUV420_10BIT_PACKED_Y0L2:
      return 16;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    case JPEGENC_RGBX8888:
    case JPEGENC_BGRX8888:
    case JPEGENC_RGBX1010102:
    case JPEGENC_BGRX1010102:
      return 32;
    case JPEGENC_Y8b:
      return 8;
    default:
      return 0;
  }
}

/*******************************************************************************
 Function name : CheckFullSize
 Description   : Check that full image size is valid
 Return type   : JPEGENC_OK for success
 Argument      : JpegEncCfg
*******************************************************************************/
i32 CheckFullSize(const JpegEncCfg *pCfgFull) {
  if ((pCfgFull->inputWidth > 32768) || (pCfgFull->inputHeight > 32768)) {
    return JPEGENC_ERROR;
  }

  if ((pCfgFull->codingWidth < 32) || (pCfgFull->codingWidth > (2048 * 16))) {
    return JPEGENC_ERROR;
  }

  if ((pCfgFull->codingHeight < 32) || (pCfgFull->codingHeight > (2048 * 16))) {
    return JPEGENC_ERROR;
  }

  if (((pCfgFull->codingWidth + 15) >> 4) *
          ((pCfgFull->codingHeight + 15) >> 4) >
      JPEGENC_MAX_SIZE) {
    return JPEGENC_ERROR;
  }

  if ((pCfgFull->codingWidth & (1)) != 0) {
    return JPEGENC_ERROR;
  }

  if ((pCfgFull->codingHeight & (1)) != 0) {
    return JPEGENC_ERROR;
  }

  /*if((pCfgFull->inputWidth & (15)) != 0)
    {
        return JPEGENC_ERROR;
    }

    if(pCfgFull->inputWidth < ((pCfgFull->codingWidth + 15) & (~15)))
    {
        return JPEGENC_ERROR;
    }*/

  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : VCEncGetAlignedStride
    Description   : Returns the stride in byte by given aligment and format.
    Return type   : u32
------------------------------------------------------------------------------*/
u32 JpegEncGetAlignedStride(int width, i32 input_format, u32 *luma_stride,
                            u32 *chroma_stride, u32 input_alignment, u32 scan_type) {
  return EncGetAlignedByteStride(width, input_format, luma_stride,
                                 chroma_stride, input_alignment, scan_type);
}

u32 JpegEncPreGetHwFormat(u32 input_format){
  return EncPreGetHwFormat(input_format);
}

/*******************************************************************************
 Function name : CheckThumbnailCfg
 Description   : Check that thumbnail data is valid
 Return type   : JPEGENC_OK for success
 Argument      : JpegEncCfg
*******************************************************************************/
i32 CheckThumbnailCfg(const JpegEncThumb *pCfgThumb) {
  u32 dataLength;
  if (pCfgThumb->width < 16 ||
      (pCfgThumb->width > 255 &&
       (pCfgThumb->format == JPEGENC_THUMB_PALETTE_RGB8 ||
        pCfgThumb->format ==
            JPEGENC_THUMB_RGB24))) /* max size limit by data range, 8-bits */
  {
    APITRACE_ERR(
        (void *)pCfgThumb,
        "CheckThumbnailCfg: Width in pixels of thumbnail is not in the "
        "range of 16~255.\n");
    return JPEGENC_ERROR;
  }
  if (pCfgThumb->height < 16 ||
      (pCfgThumb->height > 255 &&
       (pCfgThumb->format == JPEGENC_THUMB_PALETTE_RGB8 ||
        pCfgThumb->format ==
            JPEGENC_THUMB_RGB24))) /* max size limit by data range, 8-bits */
  {
    APITRACE_ERR((void *)pCfgThumb,
                 "CheckThumbnailCfg: Height in pixels of thumbnail is not in "
                 "the range of 16~255.\n");
    return JPEGENC_ERROR;
  }
  if (pCfgThumb->data == NULL) {
    return JPEGENC_ERROR;
  }

  switch (pCfgThumb->format) {
    case JPEGENC_THUMB_JPEG: {
      dataLength =
          ((1 << 16) - 1) - 8; /* 16 bits minus the APP0 ext field count */
      if (pCfgThumb->dataLength > dataLength) {
        APITRACE_ERR(
            (void *)pCfgThumb,
            "CheckThumbnailCfg: Total thumbnail data is larger than 16 bit.\n");
        return JPEGENC_ERROR;
      }
    } break;
    case JPEGENC_THUMB_PALETTE_RGB8: {
      dataLength = 3 * 256 + (pCfgThumb->width * pCfgThumb->height);
      if ((dataLength > (((1 << 16) - 1) -
                         10)) || /* 16 bits minus the APP0 ext field count */
          (pCfgThumb->dataLength != dataLength)) {
        APITRACE_ERR(
            (void *)pCfgThumb,
            "CheckThumbnailCfg: Total thumbnail data is larger than 16 bit.\n");
        return JPEGENC_ERROR;
      }
    } break;
    case JPEGENC_THUMB_RGB24: {
      dataLength = (3 * pCfgThumb->width * pCfgThumb->height);
      if ((dataLength > (((1 << 16) - 1) -
                         10)) || /* 16 bits minus the APP0 ext field count */
          (pCfgThumb->dataLength != dataLength)) {
        APITRACE_ERR(
            (void *)pCfgThumb,
            "CheckThumbnailCfg: Total thumbnail data is larger than 16 bit.\n");
        return JPEGENC_ERROR;
      }
    } break;
    default:
      return JPEGENC_ERROR;
  }

  return JPEGENC_OK;
}

/*******************************************************************************
 Function name : CheckJpegCfg
 Description   : Check that all values in the input structure are valid
 Return type   : bool_e
 Argument      : JpegEncCfg
*******************************************************************************/
bool_e CheckJpegCfg(const JpegEncCfg *pEncCfg, void *ctx) {
  /* check HW limitations */

  u32 i = 0;

  i32 core_id = -1;

  const EWLHwConfig_t *cfg =
      EncAsicGetAsicConfig(EWL_CLIENT_TYPE_JPEG_ENC, ctx);
  if (!cfg) {
    APITRACE_ERR((void *)ctx, "JpegApi: ERROR Null argument\n");
    return ENCHW_NOK;
  }
  /* is JPEG encoding supported */
  if (cfg->jpegEnabled == EWL_HW_CONFIG_NOT_SUPPORTED) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: ERROR Jpeg encoding is not supported\n");
    return ENCHW_NOK;
  }
  /* is color conversion supported */
  if (cfg->rgbEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
      ((pEncCfg->frameType >= JPEGENC_RGB565 &&
        pEncCfg->frameType <= JPEGENC_BGR101010) ||
       (pEncCfg->frameType >= JPEGENC_RGBX8888 &&
        pEncCfg->frameType <= JPEGENC_BGRX1010102))) {
    APITRACE_ERR((void *)ctx, "JpegApi: rgb format is not supported\n");
    return ENCHW_NOK;
  }
  if (cfg->jpeg422Support == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->codingMode == JPEGENC_422_MODE && pEncCfg->losslessEn == 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: 422 coding mode is not supported\n");
    return ENCHW_NOK;
  }

  if (cfg->jpeg400Support == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->codingMode == JPEGENC_MONO_MODE && pEncCfg->losslessEn == 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: 400 coding mode is not supported\n");
    return ENCHW_NOK;
  }

  if (cfg->streamMultiSegment == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->streamMultiSegmentMode != 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: multi segment is not supported\n");
    return ENCHW_NOK;
  }
  if (cfg->cscExtendSupport == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->colorConversion.type >= JPEGENC_RGBFULL_TO_YUVLIMIT_BT601) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: color conversion type is not supported BT601\n");
    return ENCHW_NOK;
  }
  if ((pEncCfg->colorConversion.type == JPEGENC_RGBTOYUV_USER_DEFINED) &&
      (cfg->cscExtendSupport == EWL_HW_CONFIG_NOT_SUPPORTED) &&
      ((pEncCfg->colorConversion.coeffG != pEncCfg->colorConversion.coeffE) ||
       (pEncCfg->colorConversion.coeffH != pEncCfg->colorConversion.coeffF) ||
       (pEncCfg->colorConversion.LumaOffset != 0))) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: color conversion type is not supported\n");
    return ENCHW_NOK;
  }

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (pEncCfg->olEnable[i] &&
        cfg->OSDSupport == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSD is not supported\n");
      return ENCHW_NOK;
    }
    if (!pEncCfg->olEnable[i]) continue;
    if (i >= cfg->maxOsdNum) {
      APITRACE_ERR((void *)ctx,
                   "JpegApi: OSD region exceed supported number\n");
      return ENCHW_NOK;
    }
  }

  for (i = 0; i < MAX_MOSAIC_NUM; i++) {
    if (pEncCfg->mosEnable[i] &&
        cfg->MosaicSupport == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACE_ERR((void *)ctx, "JpegApi: Mosaic is not supported\n");
      return ENCHW_NOK;
    }
    if (!pEncCfg->mosEnable[i]) continue;
    if (i >= cfg->maxMosaicNum) {
      APITRACE_ERR((void *)ctx,
                   "JpegApi: Mosaic region exceed supported number\n");
      return ENCHW_NOK;
    }
  }

  if (cfg->JpegRoiMapSupport == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->enableRoimap != 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: ROI map is not supported\n");
    return ENCHW_NOK;
  }

  if (cfg->prpMirrorSupport == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->mirror != 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: mirror is not supported\n");
    return ENCHW_NOK;
  }

  if (cfg->ljpegSupport == EWL_HW_CONFIG_NOT_SUPPORTED &&
      pEncCfg->losslessEn != 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: lossless is not supported\n");
    return ENCHW_NOK;
  }

  if ((cfg->NVFormatOnlySupport == EWL_HW_CONFIG_ENABLED) &&
      (pEncCfg->frameType != JPEGENC_YUV420_SEMIPLANAR) &&
      (pEncCfg->frameType != JPEGENC_YUV420_SEMIPLANAR_VU)) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: input format only support NV12 or NV21 by HW\n");
    return ENCHW_NOK;
  }

  if ((cfg->NonRotationSupport == EWL_HW_CONFIG_ENABLED) &&
      (pEncCfg->rotation != 0)) {
    APITRACE_ERR((void *)ctx,
                "JpegApi: ERROR rotation not supported by HW\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->qLevel > 10) {
    APITRACE_ERR((void *)ctx, "JpegApi: qlevel should be less than 11\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->frameType >= JPEGENC_FORMAT_MAX) {
    APITRACE_ERR((void *)ctx, "JpegApi: frame type is not supported\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->mirror > 2) {
    APITRACE_ERR((void *)ctx, "JpegApi: mirror type is not supported\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->codingType != JPEGENC_WHOLE_FRAME &&
      pEncCfg->codingType != JPEGENC_SLICED_FRAME) {
    APITRACE_ERR(
        (void *)ctx,
        "JpegApi: coding type can not be both whole frame and sliced frame\n");
    return ENCHW_NOK;
  }

  /* only support internally YUV420 or MONO */
  if (pEncCfg->codingMode != JPEGENC_420_MODE &&
      pEncCfg->codingMode != JPEGENC_422_MODE &&
      pEncCfg->codingMode != JPEGENC_MONO_MODE) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: coding mod only support 420,422 and mono\n");
    return ENCHW_NOK;
  }

  /* Units type must be valid */
  if (pEncCfg->unitsType != JPEGENC_NO_UNITS &&
      pEncCfg->unitsType != JPEGENC_DOTS_PER_INCH &&
      pEncCfg->unitsType != JPEGENC_DOTS_PER_CM) {
    APITRACE_ERR((void *)ctx, "JpegApi: unit type is not valid\n");
    return ENCHW_NOK;
  }

  /* Xdensity and Ydensity must valid */
  if ((pEncCfg->xDensity > 0xFFFFU) || (pEncCfg->yDensity > 0xFFFFU)) {
    APITRACE_ERR((void *)ctx, "JpegApi: x and y density is not valid\n");
    return ENCHW_NOK;
  }

  /* COM header length */
  if ((pEncCfg->comLength > 0xFFFDU) ||
      (pEncCfg->comLength != 0 && pEncCfg->pCom == NULL)) {
    APITRACE_ERR((void *)ctx, "JpegApi: com header length must be valid\n");
    return ENCHW_NOK;
  }

  /* Marker type must be valid */
  if (pEncCfg->markerType != JPEGENC_SINGLE_MARKER &&
      pEncCfg->markerType != JPEGENC_MULTI_MARKER) {
    APITRACE_ERR((void *)ctx, "JpegApi: marker type is not valid\n");
    return ENCHW_NOK;
  }

  /* Input Line buffer check */
  if (pEncCfg->inputLineBufEn) {
    /* check zero depth */
    if ((pEncCfg->inputLineBufDepth == 0 || pEncCfg->amountPerLoopBack == 0) &&
        (pEncCfg->inputLineBufLoopBackEn || pEncCfg->inputLineBufHwModeEn)) {
      APITRACE_ERR(
          (void *)ctx,
          "JpegApi: line buffer parameters is not valid while enabled\n");
      return ENCHW_NOK;
    }

    /* not support ratation */
    if (pEncCfg->rotation) {
      APITRACE_ERR((void *)ctx,
                   "JpegApi: line buffer mode rotation is not supported\n");
      return ENCHW_NOK;
    }
  }

  if (pEncCfg->streamMultiSegmentMode > 3) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: multi segment mode or amount is not valid\n");
    return ENCHW_NOK;
  }

  // if (pEncCfg->streamMultiSegmentMode != 0 &&
  //     pEncCfg->streamMultiSegmentSize > pEncCfg->codingHeight * pEncCfg->codingWidth * 4) {
  //   APITRACE_ERR((void *)ctx,
  //               "JpegApi: multi segment SegmentSize is too large than need\n");
  //   return ENCHW_NOK;
  // }

  if (pEncCfg->constChromaEn) {
    u32 maxCh = 255;
    if ((pEncCfg->constCb > maxCh) || (pEncCfg->constCr > maxCh)) {
      APITRACE_ERR((void *)ctx, "JpegApi: constant cb/cr value is not valid\n");
      return ENCHW_NOK;
    }
  }
  /*stride*/
  if (pEncCfg->exp_of_input_alignment < 4 &&
      pEncCfg->exp_of_input_alignment > 0) {
    APITRACE_ERR((void *)ctx, "JpegApi: input alignment is not valid\n");
    return ENCHW_NOK;
  }

  /*Motion JPEG and RC check. Not support slice mode*/
  if ((pEncCfg->frameRateNum != pEncCfg->frameRateDenom) &&
      (pEncCfg->codingType == JPEGENC_SLICED_FRAME)) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: sliced frame coding type not support motion jpeg\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->qpmin > 51 || pEncCfg->qpmax > 51 || pEncCfg->fixedQP > 51 ||
      pEncCfg->fixedQP < -1) {
    APITRACE_ERR((void *)ctx, "JpegApi: qp related value is not valid\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->targetBitPerSecond && pEncCfg->fixedQP != -1) {
    APITRACE_ERR((void *)ctx, "JpegApi: fixedQP mode not support targetBit\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->quality != -1 && pEncCfg->fixedQP != -1) {
    APITRACE_ERR((void *)ctx, "JpegApi: fixedQP mode will ignore quality factor setting\n");
  }

  /* Overlay check */
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (pEncCfg->olEnable[i]) {
      if (pEncCfg->olWidth[i] == 0 || pEncCfg->olHeight[i] == 0) {
        APITRACE_ERR((void *)ctx,
                     "JpegApi: OSD source width or height is not valid\n");
        return ENCHW_NOK;
      }

      if (pEncCfg->olWidth[i] & 1 || pEncCfg->olHeight[i] & 1 ||
          pEncCfg->olXoffset[i] & 1 || pEncCfg->olYoffset[i] & 1) {
        APITRACE_ERR((void *)ctx,
                     "JpegApi: OSD source width or height is not 2 aligned\n");
        return ENCHW_NOK;
      }

      if (pEncCfg->olXoffset[i] + pEncCfg->olCropWidth[i] >
              pEncCfg->xOffset + pEncCfg->codingWidth ||
          pEncCfg->olYoffset[i] + pEncCfg->olCropHeight[i] >
              pEncCfg->yOffset + pEncCfg->codingHeight) {
        APITRACE_ERR((void *)ctx,
                     "JpegApi: OSD cropping size exceed input boundary\n");
        return ENCHW_NOK;
      }

      if (pEncCfg->olCropXoffset[i] + pEncCfg->olCropWidth[i] >
              pEncCfg->olWidth[i] ||
          pEncCfg->olCropYoffset[i] + pEncCfg->olCropHeight[i] >
              pEncCfg->olHeight[i]) {
        APITRACE_ERR((void *)ctx,
                     "JpegApi: OSD cropping size exceed OSD source size\n");
        return ENCHW_NOK;
      }
    }
  }

  /* OSDMap checker */
  if (pEncCfg->osdMapEnable) {
    u32 osd_chan = 0, osdmap_chan = 0;
    if (cfg->OSDMapSupport == 0) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSDMap not supported\n");
      return ENCHW_NOK;
    }

    if (pEncCfg->codingMode == JPEGENC_MONO_MODE) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSDMap not supported for mono\n");
      return ENCHW_NOK;
    }

    if (!cfg->OSDMapVersion && pEncCfg->osdMapBlockSize == 4) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSDMap version[%d] not support block size %d\n", cfg->OSDMapVersion, pEncCfg->osdMapBlockSize);
      return ENCHW_NOK;
    }

    if (pEncCfg->osdMapBlockSize != 4 && pEncCfg->osdMapBlockSize != 8) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSDMap do not support block size %d\n", pEncCfg->osdMapBlockSize);
      return ENCHW_NOK;
    }

    if (pEncCfg->olSuperTile[0]) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSDMap conflict with overlay superTile\n");
      return ENCHW_NOK;
    }

    if (pEncCfg->osdMapStride < ((pEncCfg->codingWidth + pEncCfg->osdMapBlockSize * 2 - 1) / pEncCfg->osdMapBlockSize / 2)) {
      APITRACE_ERR((void *)ctx, "JpegApi: OSDMap invalid stride\n");
      return ENCHW_NOK;
    }

    for (i = 0; i < MAX_OVERLAY_NUM; i++) {
      if (pEncCfg->olEnable[i] | pEncCfg->mosEnable[i]){
        osd_chan++;
      }
      if (pEncCfg->osdMapAlpha[i]){
        osdmap_chan++;
      }
    }

    if (osd_chan + osdmap_chan > MAX_OVERLAY_NUM) {
      APITRACEERR("JpegApi: The number of colors set for osdmap exceeds the available channels. Overlay and"
       "mosaic occupy %d channels, please set the number of colors to less than %d\n", osd_chan, MAX_OVERLAY_NUM - osd_chan);
      return ENCHW_NOK;
    }
  }

  for (i = 0; i < MAX_MOSAIC_NUM; i++) {
    if (pEncCfg->mosEnable[i] &&
        cfg->mosaicVersion == 0 && pEncCfg->mosSizeIndex != 1) {
      APITRACE_ERR((void *)ctx, "JpegApi: Mosaic size index is not supported\n");
      return ENCHW_NOK;
    }
  }

  if (pEncCfg->inputWidth % 2 != 0) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: input source width is not aligned to 2\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->codingMode == JPEGENC_MONO_MODE) {
  	JpegEncFrameType support_formats[] = {
		JPEGENC_YUV420_PLANAR,
		JPEGENC_YUV420_SEMIPLANAR,
		JPEGENC_YUV420_SEMIPLANAR_VU,		
		JPEGENC_YUV420_I010,
		JPEGENC_YUV420_MS_P010,
		JPEGENC_YVU420_PLANAR,
		JPEGENC_Y8b,
		JPEGENC_Y10bWL,
		JPEGENC_Y10bWH };
	int length = sizeof(support_formats)/sizeof(JpegEncFrameType);
	int idx=0;
	for(idx=0; idx<length; idx++) {
		if (pEncCfg->frameType == support_formats[idx])
			break;
	}
    if (idx==length) {
      APITRACE_ERR(
          (void *)ctx,
          "JpegApi: input format is not supported by monochrome encoding\n");
      return ENCHW_NOK;
    }

    if (pEncCfg->rotation != JPEGENC_ROTATE_0) {
      APITRACE_ERR(
          (void *)ctx,
          "JpegApi: rotation is not supported by monochrome encoding\n");
      return ENCHW_NOK;
    }

    for (i = 0; i < MAX_MOSAIC_NUM; i++) {
      if (pEncCfg->mosEnable[i]) {
        APITRACE_ERR((void *)ctx,
                 "JpegApi: mosaic is not supported by monochrome encoding\n");
        return ENCHW_NOK;
      }
    }
  }

  if ((pEncCfg->codingMode == JPEGENC_422_MODE) ^
      ((pEncCfg->frameType == JPEGENC_YUV422SP_888)
      || (pEncCfg->frameType == JPEGENC_YVU422SP_888))) {
    APITRACE_ERR((void *)ctx,
               "JpegApi: not support encoding input format %d as %s\n",
               (int)(pEncCfg->frameType),
               ((pEncCfg->codingMode == JPEGENC_422_MODE)?"YUV422":"YUV420"));
    return ENCHW_NOK;
  }

  /* encode size check */
  if (pEncCfg->codingWidth > cfg->maxEncodedWidthJPEG) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: Invalid width, not supported by HW coding core\n");
    return ENCHW_NOK;
  }
  if (pEncCfg->codingHeight > JPEGENC_IMAGE_MAX_HEIGHT) {
    APITRACE_ERR((void *)ctx,
                 "JpegApi: Invalid heigth, not supported by HW coding core\n");
    return ENCHW_NOK;
  }

  //scan type
  if ((cfg->superTileXSupport == EWL_HW_CONFIG_NOT_SUPPORTED) &&
      (pEncCfg->scanType == JPEG_SUPERTILEX_SCAN)) {
    APITRACEERR("JpegApi: ERROR superTileX scanType not supported by HW\n");
    return ENCHW_NOK;
  }

  if (pEncCfg->scanType == JPEG_SUPERTILEX_SCAN) {
    u32 rotate = pEncCfg->rotation == JPEGENC_ROTATE_90R ||
                 pEncCfg->rotation == JPEGENC_ROTATE_90L;
    if ((rotate && (pEncCfg->codingWidth + pEncCfg->xOffset > pEncCfg->inputHeight ||
         pEncCfg->codingHeight + pEncCfg->yOffset > pEncCfg->inputWidth)) ||
        (!rotate && (pEncCfg->codingWidth + pEncCfg->xOffset > pEncCfg->inputWidth ||
         pEncCfg->codingHeight + pEncCfg->yOffset > pEncCfg->inputHeight))) {
      APITRACE_ERR((void *)ctx,
                 "JpegApi: encoded image size larger than original size\n");
      return ENCHW_NOK;
    }
  }

  if ((pEncCfg->rotation != JPEGENC_ROTATE_0) &&
      (pEncCfg->codingWidth < 64 || pEncCfg->codingHeight < 64)) {
    APITRACE_ERR((void *)ctx,
                "JpegApi: rotation support the minimal resolution is 64x64\n");
    return ENCHW_NOK;

  }

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncGetEncodedMbLines
    Description   : Get how many MB lines has been encoded by encoder.
    Return type   : u32
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
u32 JpegEncGetEncodedMbLines(JpegEncInst inst) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;
  u32 lines;

  APITRACE_INFO((void *)inst, "JpegEncGetEncodedMbLines#\n");

  /* Check for illegal inputs */
  if (!pEncInst) {
    APITRACE_ERR((void *)inst,
                 "JpegEncGetEncodedMbLines: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  if (!pEncInst->inputLineBuf.inputLineBufEn) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncGetEncodedMbLines: ERROR Invalid mode for input control\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  lines = EncAsicGetRegisterValue(
      pEncInst->asic.ewl, pEncInst->asic.regs.regMirror, HWIF_CTB_ROW_RD_PTR);
  lines +=
      EncAsicGetRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror,
                              HWIF_CTB_ROW_RD_PTR_JPEG_MSB)
      << 10;
  return lines;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncSetInputMBLines
    Description   : Set the input buffer lines available of current picture.
    Return type   : JpegEncRet
    Argument      : inst - encoder instance
    Argument      : lines - number of macroblock lines
------------------------------------------------------------------------------*/
JpegEncRet JpegEncSetInputMBLines(JpegEncInst inst, u32 lines) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;

  APITRACE_INFO((void *)inst, "JpegEncSetInputMBLines#\n");

  /* Check for illegal inputs */
  if (!pEncInst) {
    APITRACE_ERR((void *)inst, "JpegEncSetInputMBLines: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  if (!pEncInst->inputLineBuf.inputLineBufEn) {
    APITRACE_ERR(
        (void *)inst,
        "JpegEncSetInputMBLines: ERROR Invalid mode for input control\n");
    return JPEGENC_INVALID_ARGUMENT;
  }

  if (!pEncInst->asic.regs.bVCMDEnable) {
    EncAsicWriteRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror,
                            HWIF_CTB_ROW_WR_PTR, lines & 0x3ff);
    EncAsicWriteRegisterValue(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror,
                            HWIF_CTB_ROW_WR_PTR_JPEG_MSB, lines >> 10);
  } else {
    EncAsicWriteRegisterValueByVcmd(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror,
                            HWIF_CTB_ROW_WR_PTR, lines & 0x3ff);
    EncAsicWriteRegisterValueByVcmd(pEncInst->asic.ewl, pEncInst->asic.regs.regMirror,
                            HWIF_CTB_ROW_WR_PTR_JPEG_MSB, lines >> 10);
  }
  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegGetLumaSize
    Description   : Get luma size.
    Return type   : luma size
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
JpegEncRet JpegGetLumaSize(JpegEncInst inst, u64 *lumaSize,
                           u64 *dec400LumaTableSize) {
  /* Check for illegal inputs */
  if (!inst) {
    APITRACE_ERR((void *)inst, "JpegGetLumaSize: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }
  if (lumaSize != NULL) *lumaSize = ((jpegInstance_s *)inst)->lumaSize;
  if (dec400LumaTableSize != NULL)
    *dec400LumaTableSize = ((jpegInstance_s *)inst)->dec400LumaTableSize;
  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegSetLumaSize
    Description   : Set luma size.
    Return type   : JpegEncRet
    Argument      : inst - encoder instance, lumaSize - luma size to set
------------------------------------------------------------------------------*/
JpegEncRet JpegSetLumaSize(JpegEncInst inst, u64 lumaSize,
                           u64 dec400LumaTableSize) {
  /* Check for illegal inputs */
  if (!inst) {
    APITRACE_ERR((void *)inst, "JpegSetLumaSize: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }
  ((jpegInstance_s *)inst)->lumaSize = lumaSize;
  ((jpegInstance_s *)inst)->dec400LumaTableSize = dec400LumaTableSize;
  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegGetChromaSize
    Description   : Get chroma size.
    Return type   : chroma size
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
JpegEncRet JpegGetChromaSize(JpegEncInst inst, u64 *chromaSize,
                             u64 *dec400ChrTableSize) {
  /* Check for illegal inputs */
  if (!inst) {
    APITRACE_ERR((void *)inst, "JpegGetChromaSize: ERROR Null argument\n");
    return -1;
  }
  if (chromaSize != NULL) *chromaSize = ((jpegInstance_s *)inst)->chromaSize;
  if (dec400ChrTableSize != NULL)
    *dec400ChrTableSize = ((jpegInstance_s *)inst)->dec400ChrTableSize;
  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegSetChromaSize
    Description   : Set chroma size.
    Return type   : JpegEncRet
    Argument      : inst - encoder instance, chromaSize - chroma size to set
------------------------------------------------------------------------------*/
JpegEncRet JpegSetChromaSize(JpegEncInst inst, u64 chromaSize,
                             u64 dec400ChrTableSize) {
  /* Check for illegal inputs */
  if (!inst) {
    APITRACE_ERR((void *)inst, "JpegSetChromaSize: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }
  ((jpegInstance_s *)inst)->chromaSize = chromaSize;
  ((jpegInstance_s *)inst)->dec400ChrTableSize = dec400ChrTableSize;
  return JPEGENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : JpegGetQpHdr
    Description   : Get qp hdr.
    Return type   : qp hdr
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
int JpegGetQpHdr(JpegEncInst inst) {
  /* Check for illegal inputs */
  if (!inst) {
    APITRACE_ERR((void *)inst, "JpegGetQpHdr: ERROR Null argument\n");
    return -1;
  }
  return ((jpegInstance_s *)inst)->rateControl.qpHdr >> JPEG_QP_FRACTIONAL_BITS;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncGetEwl
    Description   : Get the ewl instance.
    Return type   : ewl instance pointer
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
const void *JpegEncGetEwl(JpegEncInst inst) {
  const void *ewl;
  if (inst == NULL) {
    APITRACE_ERR((void *)inst, "JpegEncGetEwl: ERROR Null argument\n");
    ASSERT(0);
    return NULL;
  }
  ewl = ((jpegInstance_s *)inst)->asic.ewl;
  if (ewl == NULL) {
    APITRACE_ERR((void *)inst, "JpegEncGetEwl: EWL instance get failed.\n");
    ASSERT(0);
    return NULL;
  }
  return ewl;
}

/*------------------------------------------------------------------------------
    Function name : JpegEncGetPerformance
    Description   : Get JPEG encoding HW performance.
    Return type   : u32
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
u32 JpegEncGetPerformance(JpegEncInst inst) {
  const void *ewl;
  u32 performanceData;
  ASSERT(inst);
  APITRACE_INFO((void *)inst, "JpegEncGetPerformance#\n");
  /* Check for illegal inputs */
  if (inst == NULL) {
    APITRACE_ERR((void *)inst, "JpegEncGetPerformance: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }

  ewl = ((jpegInstance_s *)inst)->asic.ewl;
  performanceData = EncAsicGetPerformance(ewl);
  PERFTRACE((void *)inst, "HWCycles=%d\n", performanceData);
  APITRACE_INFO((void *)inst, "JpegEncGetPerformance:OK\n");
  return performanceData;
}

/* Config AxiFe */
static void JpegEncCfgAxiFe(jpegInstance_s *instance) {
#ifdef SUPPORT_AXIFE
  asicData_s *asic = &instance->asic;
  struct VCAxiFeData *af_cfg;
  struct ChnDesc *rd_ch;
  struct ChnDesc *wr_ch;

  af_cfg = asic->axife_data;
  af_cfg->mode = instance->axiFEEnable;
  af_cfg->ewl = (const void *)asic->ewl;
  af_cfg->vcmd = NULL;

  if (af_cfg->mode == 0) {
    return;
  }

  if (asic->regs.bVCMDEnable) af_cfg->vcmd = &asic->regs.vcmd;

  if (af_cfg->mode == 3) {
    memset(&af_cfg->channelCfg, 0, sizeof(struct AxiFeChns));
    memset(&af_cfg->commonCfg, 0, sizeof(struct AxiFeCommonCfg));

    //example to config AXIFE
    af_cfg->channelCfg.nbr_rd_chns = 1;
    af_cfg->channelCfg.nbr_wr_chns = 1;
    rd_ch = &af_cfg->channelCfg.rd_chns[0];
    wr_ch = &af_cfg->channelCfg.wr_chns[0];

    rd_ch->sw_axi_base_addr_id = asic->regs.inputLumBase >> 32;
    rd_ch->sw_axi_start_addr = asic->regs.inputLumBase & 0xFFFFFFFF;
    rd_ch->sw_axi_end_addr =
        (asic->regs.inputLumBase +
         asic->regs.input_luma_stride * instance->preProcess.lumHeightSrc[0]) &
        0xFFFFFFFF;
    rd_ch->sw_axi_user = 0;
    rd_ch->sw_axi_ns = 0;

    wr_ch->sw_axi_base_addr_id = asic->regs.outputStrmBase[0] >> 32;
    wr_ch->sw_axi_start_addr = asic->regs.outputStrmBase[0] & 0xFFFFFFFF;
    wr_ch->sw_axi_end_addr =
        (asic->regs.outputStrmBase[0] + asic->regs.outputStrmSize[0]) &
        0xFFFFFFFF;
    wr_ch->sw_axi_user = 0;
    wr_ch->sw_axi_ns = 0;

    af_cfg->commonCfg.sw_secure_mode = 1;
    af_cfg->commonCfg.sw_axi_user_mode = 3;
    af_cfg->commonCfg.sw_axi_prot_mode = 1;
  }
#endif
}

/*------------------------------------------------------------------------------
    Function name : JpegGetSbiSupport
    Description   : Get Sbi Support.
    Return type   : u32
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
u32 JpegGetSbiSupport(JpegEncInst inst) {
  /* Check for illegal inputs */
  if (!inst) {
    APITRACE_ERR((void *)inst, "JpegGetSbiSupport: ERROR Null argument\n");
    return JPEGENC_NULL_ARGUMENT;
  }
  return ((jpegInstance_s *)inst)->asic.regs.asicCfg->prpSbiSupport;
}

/**
 * Set quantization matrix according to the quality factor
 *
 * \brief Set quantization matrix according to the quality factor.
 *
 * \param [in] inst Encoder instance
 * \param [in] quality factor
 * \returns  <tt> \ref JpegEncRet </tt>
 */
JpegEncRet JpegEncSetQuailty(JpegEncInst inst, JpegEncQuality *quality) {
  jpegInstance_s *pEncInst = (jpegInstance_s *)inst;
  i32 need_change_quality = HANTRO_FALSE;
  JpegEncRet ret = VCENC_OK;

  /* Convert user 0-100 rating to percentage scaling */
  if (pEncInst->fixedQP!=-1) {
    need_change_quality = HANTRO_TRUE;
  } else if (pEncInst->quality.factor!=quality->factor) {
    need_change_quality = HANTRO_TRUE;
  }

  if (need_change_quality) {
    int scale_factor = JpegGetQualityScalingFactor(quality->factor);

    /* Set up standard quality tables */
    JpegCalcQuantTable(pEncInst->jpeg.qTableLuma, std_luminance_quant_tbl,
                         scale_factor, HANTRO_TRUE);
    JpegCalcQuantTable(pEncInst->jpeg.qTableChroma, std_chrominance_quant_tbl,
                         scale_factor, HANTRO_TRUE);

    /* debug quant table */
    APITRACE_INFO((void *)inst, "qTableLuma\n");
    for(int i=0; i<64; i+=8) {
      APITRACE_INFO((void *)inst, " %3d %3d %3d %3d %3d %3d %3d %3d\n", pEncInst->jpeg.qTableLuma[i],
        pEncInst->jpeg.qTableLuma[i+1], pEncInst->jpeg.qTableLuma[i+2],
        pEncInst->jpeg.qTableLuma[i+3], pEncInst->jpeg.qTableLuma[i+4],
        pEncInst->jpeg.qTableLuma[i+5], pEncInst->jpeg.qTableLuma[i+6],
        pEncInst->jpeg.qTableLuma[i+7]);
    }
    APITRACE_INFO((void *)inst, "qTableChroma\n");
    for(int i=0; i<64; i+=8) {
      APITRACE_INFO((void *)inst, " %3d %3d %3d %3d %3d %3d %3d %3d\n", pEncInst->jpeg.qTableChroma[i],
        pEncInst->jpeg.qTableChroma[i+1], pEncInst->jpeg.qTableChroma[i+2],
        pEncInst->jpeg.qTableChroma[i+3], pEncInst->jpeg.qTableChroma[i+4],
        pEncInst->jpeg.qTableChroma[i+5], pEncInst->jpeg.qTableChroma[i+6],
        pEncInst->jpeg.qTableChroma[i+7]);
    }
    pEncInst->jpeg.qTable.pQlumi = pEncInst->jpeg.qTableLuma;
    pEncInst->jpeg.qTable.pQchromi = pEncInst->jpeg.qTableChroma;

    /* update state */
    pEncInst->fixedQP = -1;
    pEncInst->quality.factor = quality->factor;
  }

  return ret;
}

/**
 * Set parameters for encoding the non-ROI region. The non-ROI's parameters
 * are depends on default quantization table. So if default quantization table
 * changed, the non-ROI parameter need to update too.
 *
 * \brief Set quantization matrix of non-ROI region.
 *
 * \param [in] inst Encoder instance
 * \param [in] filter used for calculating non-ROI region quantization matrix
 * \returns  <tt> \ref JpegEncRet </tt>
 */
JpegEncRet JpegEncSetNonRoi(JpegEncInst inst, const u8 *filter) {
  jpegInstance_s *enc = (jpegInstance_s *)inst;
  JpegEncQuantTables *tbl = &enc->jpeg.qTable;

  for (int i = 0; i < 64; i++) {
    enc->jpeg.qTableLumaNonRoi[i] =
        (filter[i] == 0) ? 255 : (MIN(255, tbl->pQlumi[i] * 255 / filter[i]));
    enc->jpeg.qTableChromaNonRoi[i] =
        (filter[i + 64] == 0) ? 255 : (MIN(255, tbl->pQchromi[i] * 255 /
                                       filter[i + 64]));
  }
  EncAsicSetNonRoiQuantTable(&enc->asic, enc->jpeg.qTableLumaNonRoi,
                             enc->jpeg.qTableChromaNonRoi, filter);

  return VCENC_OK;
}


/**
 * Set parameters used for encoding.
 *
 * \brief Set parameters used for encoding.
 *
 * \param [in] inst Encoder instance
 * \param [in] type of parameters to set
 * \param [in] parameters for corresponding type.
 * \returns  <tt> \ref JpegEncRet </tt>
 */
JpegEncRet JpegEncSetParams(JpegEncInst inst, JpegEncParamType type,
                                  void *param)
{
  JpegEncRet ret = JPEGENC_OK;

  switch(type)
  {
    case JPEGENC_PARAM_QUALITY:
      ret = JpegEncSetQuailty(inst, (JpegEncQuality*)param);
      break;
    default:
      ret = JPEGENC_INVALID_ARGUMENT;
      break;
  }

  return ret;
}

/**
 * Get parameters used for encoding.
 *
 * \brief Get parameters used for encoding.
 *
 * \param [in] inst Encoder instance
 * \param [in] type of parameters to set
 * \param [out] parameters for corresponding type.
 * \returns  <tt> \ref JpegEncRet </tt>
 */
JpegEncRet JpegEncGetParams(JpegEncInst inst, JpegEncParamType type,
                                  void *param)
{
  jpegInstance_s *enc = (jpegInstance_s *)inst;
  JpegEncRet ret = JPEGENC_OK;

  switch(type)
  {
    case JPEGENC_PARAM_QUALITY:
      memcpy(param, &enc->quality, sizeof(enc->quality));
      break;
    default:
      ret = JPEGENC_INVALID_ARGUMENT;
      break;
  }

  return ret;
}

/*------------------------------------------------------------------------------
    JpegEncSetQTable
------------------------------------------------------------------------------*/
void JpegSetQTable(u8 *dst, const u8 *src) {
  i32 i;

  for (i = 0; i < 64; i++) {
    u8 qp = src[i];

    /* Round qp to value that can be handled by ASIC */
    if (qp > 128)
      qp = qp / 8 * 8;
    else if (qp > 64)
      qp = qp / 4 * 4;
    else if (qp > 32)
      qp = qp / 2 * 2;

    dst[i] = qp;
  }
}

