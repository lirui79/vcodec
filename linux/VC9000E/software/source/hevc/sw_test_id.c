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
--------------------------------------------------------------------------------*/
#ifdef INTERNAL_TEST

//  Abstract : Encoder setup according to a test vector

// header
#include "sw_test_id.h"
#include "vsi_string.h"
#include "ewl.h"
#include "buffer_info.h"

//#define ASIC_FRAMESTART_OPT

// static function
static void HevcFrameQuantizationTest(struct vcenc_instance *inst);
static void HevcSliceTest(struct vcenc_instance *inst);
static void HevcMbQuantizationTest(struct vcenc_instance *inst);
static void HevcFilterTest(struct vcenc_instance *inst);
static void HevcUserDataTest(struct vcenc_instance *inst);
static void HevcIntra32FavorTest(struct vcenc_instance *inst);
static void HevcIntra16FavorTest(struct vcenc_instance *inst);
static void HevcInterFavorTest(struct vcenc_instance *inst);
static void HevcCroppingTest(struct vcenc_instance *inst);
static void HevcRgbInputMaskTest(struct vcenc_instance *inst);
static void HevcMadTest(struct vcenc_instance *inst);
static void HevcMvTest(struct vcenc_instance *inst);
static void HevcDMVPenaltyTest(struct vcenc_instance *inst);
static void HevcMaxOverfillMv(struct vcenc_instance *inst);
static void HevcRoiTest(struct vcenc_instance *inst);
static void HevcIntraAreaTest(struct vcenc_instance *inst);
static void HevcCirTest(struct vcenc_instance *inst);
static void HevcIntraSliceMapTest(struct vcenc_instance *inst);
static void HevcSegmentTest(struct vcenc_instance *inst);
static void HevcRefFrameTest(struct vcenc_instance *inst);
static void HevcTemporalLayerTest(struct vcenc_instance *inst);
static void HevcSegmentMapTest(struct vcenc_instance *inst);
static void HevcPenaltyTest(struct vcenc_instance *inst);
static void HevcDownscalingTest(struct vcenc_instance *inst);
static void HevcLtrIntervalTest(struct vcenc_instance *inst);
static void HevcIPCMTest(struct vcenc_instance *inst);
static void VCEncSmartModeTest(struct vcenc_instance *inst);
static void VCEncConstChromaTest(struct vcenc_instance *inst);
static void VCEncCtbRcTest(struct vcenc_instance *inst);
static void VCEncCtbRc1TestV2(struct vcenc_instance *inst);
static void VCEncGMVTest(struct vcenc_instance *inst);
static void VCEncExtLineBufferTest(struct vcenc_instance *inst);
static void VCEncMEVertRangeTest(struct vcenc_instance *inst);
static void VCEncRefFrameUsingInputFrameTest(struct vcenc_instance *inst);
static void VCEncH264LookaheadIntraModeNon4x4Test(struct vcenc_instance *inst);
static void VCEncH264LookaheadIntraModeNon8x8Test(struct vcenc_instance *inst);
static void VCEncRdoqLambdaAdjustTest(struct vcenc_instance *inst);
static void VCEncBilinearDownsampleTest(struct vcenc_instance *inst);
static void VCEncSimpleRdoAssignTest(struct vcenc_instance *inst);
static void VCEncLookaheadIntrapredBySatdTest(struct vcenc_instance *inst);
static void VCEncLookaheadBypassRDOTest(struct vcenc_instance *inst);
static void VCEncMotionScoreTest(struct vcenc_instance *inst);
static void HevcIMBlockUnitTest(struct vcenc_instance *inst, int dsRatio,
                                int outRoiMapDeltaQpBlockUnit);
static void VCEncCtbRcRowQpConstraint(struct vcenc_instance *inst);
static void VCEncCtbRc3ExtremeTestV2(struct vcenc_instance *inst);
static void VCEncTrailDetectTest(struct vcenc_instance *inst, struct sw_picture *pic);
static void VCEncModSubjPreferTest(struct vcenc_instance *inst);
static void VCEncIntraBiasTest(struct vcenc_instance *inst);
static void VCEncChromaCheckTest(struct vcenc_instance *inst);
static void HevcUFBCDECTest(struct vcenc_instance *inst);
static void HevcUFBCCFGTest(struct vcenc_instance *inst);

/*------------------------------------------------------------------------------

    TestID defines a test configuration for the encoder. If the encoder control
    software is compiled with INTERNAL_TEST flag the test ID will force the
    encoder operation according to the test vector.

    TestID  Description
    0       No action, normal encoder operation
    1       Frame quantization test, adjust qp for every frame, qp = 0..51
    2       Slice test, adjust slice amount for each frame
    4       Stream buffer limit test, limit=500 (4kB) for first frame
    6       Quantization test, min and max QP values.
    7       Filter test, set disableDeblocking and filterOffsets A and B
    8       Segment test, set segment map and segment qps
    9       Reference frame test, all combinations of reference and refresh.
    10      Segment map test
    11      Temporal layer test, reference and refresh as with 3 layers
    12      User data test
    15      Intra16Favor test, set to maximum value
    16      Cropping test, set cropping values for every frame
    19      RGB input mask test, set all values
    20      MAD test, test all MAD QP change values
    21      InterFavor test, set to maximum value
    22      MV test, set cropping offsets so that max MVs are tested
    23      DMV penalty test, set to minimum/maximum values
    24      Max overfill MV
    26      ROI test
    27      Intra area test
    28      CIR test
    29      Intra slice map test
    31      Non-zero penalty test, don't use zero penalties
    34      Downscaling test
    56      test IM original/output block unit combination. dsRatio 1/1, outRoiMapDeltaQpBlockUnit 64x64
    57      test IM original/output block unit combination. dsRatio 1/1, outRoiMapDeltaQpBlockUnit 32x32
    58      test IM original/output block unit combination. dsRatio 1/1, outRoiMapDeltaQpBlockUnit 16x16
    59      test IM original/output block unit combination. dsRatio 1/2, outRoiMapDeltaQpBlockUnit 64x64
    60      test IM original/output block unit combination. dsRatio 1/2, outRoiMapDeltaQpBlockUnit 32x32
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    HevcConfigureTestBeforeFrame

    Function configures the encoder instance before starting frame encoding

------------------------------------------------------------------------------*/
void HevcConfigureTestBeforeFrame(struct vcenc_instance *inst) {
  ASSERT(inst);
  switch (inst->testId) {
    case (u32)TID_QP:
      HevcFrameQuantizationTest(inst);
      break;
    case (u32)TID_SLC:
      HevcSliceTest(inst);
      break;
      //NotSupport case 6: HevcMbQuantizationTest(inst);           break;
    case (u32)TID_DBF:
      HevcFilterTest(inst);
      break;
    case 8:
      HevcSegmentTest(inst);
      break;
    case 9:
      HevcRefFrameTest(inst);
      break;
      //NotSupport case 10: HevcSegmentMapTest(inst);              break;
    case 11:
      HevcTemporalLayerTest(inst);
      break;
    case 12:
      HevcUserDataTest(inst);
      break;
    case 16:
      /* do not change vertical offset here in input line buffer mode, because test bench don't know this change */
      if (!inst->inputLineBuf.inputLineBufEn) {
        HevcCroppingTest(inst);
      }
      break;
    case 19:
      HevcRgbInputMaskTest(inst);
      break;
    case 20:
      HevcMadTest(inst);
      break;
    case 22:
      HevcMvTest(inst);
      break;
    case 24:
      HevcMaxOverfillMv(inst);
      break;
    case TID_IA:
      HevcIntraAreaTest(inst);
      break;
    case TID_CIR:
      HevcCirTest(inst);
      break;
      //NotSupport case 29: HevcIntraSliceMapTest(inst);           break;
    case 34:
      HevcDownscalingTest(inst);
      break;
    case TID_LTR:
      HevcLtrIntervalTest(inst);
      break;
    case TID_SMT:
      VCEncSmartModeTest(inst);
      break;
    case TID_CCH:
      VCEncConstChromaTest(inst);
      break;
    case TID_RC:
      VCEncCtbRcTest(inst);
      break;
    case TID_MOTION_SCORE:
      VCEncMotionScoreTest(inst);
      break;
    case TID_CTBRC1_V2:
      VCEncCtbRc1TestV2(inst);
      break;
    case TID_UFBC_DEC_ERROR:
      HevcUFBCDECTest(inst);
      break;
    case TID_UFBC_CFG_ERROR:
      HevcUFBCCFGTest(inst);
      break;
    default:
      break;
  }
}

/*------------------------------------------------------------------------------

    HevcConfigureTestBeforeStart

    Function configures the encoder instance before starting frame encoding

------------------------------------------------------------------------------*/
void HevcConfigureTestBeforeStart(struct vcenc_instance *inst, struct sw_picture *pic) {
  ASSERT(inst);
  /* update all regs for internal verification */
#ifndef ASIC_FRAMESTART_OPT
  if ((TID_NONE < inst->testId) && (inst->testId < TID_MAX))
    inst->asic.regs.bInitUpdate = 1;
#endif

  switch (inst->testId) {
    case (u32)TID_ROI:
      HevcRoiTest(inst);
      break;
    case (u32)TID_F32:
      HevcIntra32FavorTest(inst);
      break;
    case (u32)TID_F16:
      HevcIntra16FavorTest(inst);
      break;
    case (u32)TID_INT:
      HevcStreamBufferLimitTest(inst, NULL);
      break;
    case 21:
      HevcInterFavorTest(inst);
      break;
    case 23:
      HevcDMVPenaltyTest(inst);
      break;
    case 31:
      HevcPenaltyTest(inst);
      break;
    case (u32)TID_IPCM:
      HevcIPCMTest(inst);
      break;
    case (u32)TID_GMV:
      VCEncGMVTest(inst);
      break;
    case (u32)TID_EXTLINEBUF:
      VCEncExtLineBufferTest(inst);
      break;
    case (u32)TID_MEVR:
      VCEncMEVertRangeTest(inst);
      break;
    case (u32)TID_INPUTASREF:
      VCEncRefFrameUsingInputFrameTest(inst);
      break;
    case (u32)TID_NON_I4X4:
      VCEncH264LookaheadIntraModeNon4x4Test(inst);
      break;
    case (u32)TID_NON_I8X8:
      VCEncH264LookaheadIntraModeNon8x8Test(inst);
      break;
    case (u32)TID_RDOQ_LAMBDA_ADJ:
      VCEncRdoqLambdaAdjustTest(inst);
      break;
    case (u32)TID_BILINEAR_DS:
      VCEncBilinearDownsampleTest(inst);
      break;
    case (u32)TID_SIMPLE_RDO:
      VCEncSimpleRdoAssignTest(inst);
      break;
    case (u32)TID_INTRA_BY_SATD:
      VCEncLookaheadIntrapredBySatdTest(inst);
      break;
    case (u32)TID_BYPASS_RDO:
      VCEncLookaheadBypassRDOTest(inst);
      break;
    case TID_TRAIL_DETECT:
      VCEncTrailDetectTest(inst, pic);
      break;
    case TID_MODSUBJ_PREFER:
      VCEncModSubjPreferTest(inst);
      break;
    case TID_IBIAS:
      VCEncIntraBiasTest(inst);
      break;
    case TID_CHROMA_CHECK:
      VCEncChromaCheckTest(inst);
      break;
    case TID_CTBRCRowQPCONSTRAINT:
      VCEncCtbRcRowQpConstraint(inst);
      break;
    case TID_CTBRC3_EXTREME_V2:
      VCEncCtbRc3ExtremeTestV2(inst);
    default:
      break;
  }
}

/*------------------------------------------------------------------------------

    VCEncCuSizeAssignTest

    Function configures the encoder instance at once when cusize is assigned.

------------------------------------------------------------------------------*/
void VCEncCuSizeAssignTest(VCEncInst encoder, u32 cuSizeAssign) {
  struct vcenc_instance *inst = (struct vcenc_instance *)encoder;
  ASSERT(inst);
   if (inst->asic.regs.asicCfg->tuneToolsSet2Support &&
      !IS_H264(inst->codecFormat)) {
    inst->asic.regs.HevcSimpleRdoAssign = cuSizeAssign;
    if (cuSizeAssign) inst->rdoLevel = 0;
  }
  if (cuSizeAssign) printf("VCEncCuSizeAssignTest:%u\n", inst->asic.regs.HevcSimpleRdoAssign);
}

/*------------------------------------------------------------------------------

    EncConfigureTestId

    Function configures the encoder instance at once when VCEncSetTestId is called

------------------------------------------------------------------------------*/
void EncConfigureTestId(struct vcenc_instance *inst) {
  ASSERT(inst);
  switch (inst->testId) {
    case TID_IMOUTBLOCKUNIT1:
      HevcIMBlockUnitTest(inst, 0, 0);
      break;
    case TID_IMOUTBLOCKUNIT2:
      HevcIMBlockUnitTest(inst, 0, 1);
      break;
    case TID_IMOUTBLOCKUNIT3:
      HevcIMBlockUnitTest(inst, 0, 2);
      break;
    case TID_IMOUTBLOCKUNIT4:
      HevcIMBlockUnitTest(inst, 1, 0);
      break;
    case TID_IMOUTBLOCKUNIT5:
      HevcIMBlockUnitTest(inst, 1, 1);
      break;
    default:
      break;
  }
}

static u32 getRandU(struct vcenc_instance *inst, u32 min, u32 max) {
  static u32 next = 0;

  //init seed
  if (inst->pass)
    next = inst->frameCnt;
  else if (next == 0)
    next = (inst->width * inst->height) >> 8;

  next = next * 1103515245 + 12345;
  return (next % (max - min + 1)) + min;
}

static i32 getRandS(struct vcenc_instance *inst, i32 min, i32 max) {
  i32 off = (min >= 0) ? 0 : -min;
  i32 val = getRandU(inst, min + off, max + off);
  val -= off;
  return val;
}

/*------------------------------------------------------------------------------
  HevcQuantizationTest
------------------------------------------------------------------------------*/
void HevcFrameQuantizationTest(struct vcenc_instance *inst) {
  i32 vopNum = inst->frameCnt;

  /* Inter frame qp start zero */
  inst->rateControl.qpHdr =
      MIN(inst->rateControl.qpMaxPB,
          MAX(inst->rateControl.qpMinPB,
              (((vopNum - 1) % 52) << QP_FRACTIONAL_BITS)));
  inst->rateControl.fixedQp = inst->rateControl.qpHdr;

  inst->asic.regs.bRateCtrlUpdate = 1;
  printf("HevcFrameQuantTest# qpHdr %d\n",
         inst->rateControl.qpHdr >> QP_FRACTIONAL_BITS);
}

/*------------------------------------------------------------------------------
  VCEncRefFrameUsingInputFrameTest
  ------------------------------------------------------------------------------*/
void VCEncRefFrameUsingInputFrameTest(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->encVisualTuneSupport ||
      inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->asic.regs.RefFrameUsingInputFrameEnable = 1;
  }
  printf("RefFrameUsingInputFrameEnableTest\n");
}

/*------------------------------------------------------------------------------
  VCEncH264LookaheadIntraModeNon4x4Test
  ------------------------------------------------------------------------------*/
void VCEncH264LookaheadIntraModeNon4x4Test(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->encVisualTuneSupport ||
      inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->asic.regs.H264Intramode4x4Disable = 1;
  }
  printf("VCEncH264LookaheadIntraModeNon4x4Test\n");
}

/*------------------------------------------------------------------------------
  VCEncH264LookaheadIntraModeNon8x8Test
  ------------------------------------------------------------------------------*/
void VCEncH264LookaheadIntraModeNon8x8Test(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->encVisualTuneSupport ||
      inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->asic.regs.H264Intramode8x8Disable = 1;
  }
  printf("VCEncH264LookaheadIntraModeNon8x8Test\n");
}

/*------------------------------------------------------------------------------
  VCEncRdoqLambdaAdjustTest
  ------------------------------------------------------------------------------*/
void VCEncRdoqLambdaAdjustTest(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->bRdoqLambdaAdjust = 1;
  }
  printf("VCEncRdoqLambdaAdjustTest\n");
}

/*------------------------------------------------------------------------------
  VCEncBilinearDownsampleTest
  ------------------------------------------------------------------------------*/
void VCEncBilinearDownsampleTest(struct vcenc_instance *inst) {
  if (inst->pass == 1 && inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->asic.regs.InLoopDSBilinearEnable = 1;
  }
  printf("VCEncBilinearDownsampleTest\n");
}

/*------------------------------------------------------------------------------
  VCEncSimpleRdoAssignTest
  ------------------------------------------------------------------------------*/
void VCEncSimpleRdoAssignTest(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->tuneToolsSet2Support &&
      !IS_H264(inst->codecFormat)) {
    inst->asic.regs.HevcSimpleRdoAssign = 1;
    inst->asic.regs.rdoLevel = inst->rdoLevel = 0;
  }
  printf("VCEncSimpleRdoAssignTest\n");
}

/*------------------------------------------------------------------------------
  VCEncLookaheadIntrapredBySatdTest(
  ------------------------------------------------------------------------------*/
void VCEncLookaheadIntrapredBySatdTest(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->asic.regs.PredModeBySatdEnable = 1;
  }
  printf("VCEncLookaheadIntrapredBySatdTest\n");
}

/*------------------------------------------------------------------------------
  VCEncLookaheadBypassRDOTest
  ------------------------------------------------------------------------------*/
void VCEncLookaheadBypassRDOTest(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->tuneToolsSet2Support) {
    inst->asic.regs.PredModeBySatdEnable = 1;
    inst->asic.regs.RefFrameUsingInputFrameEnable = 1;
    inst->asic.regs.sw_av1_allow_update_cdf = 0;
    //HW logic depends on skip cabac
    inst->asic.regs.bSkipCabacEnable = 1;
  }
  printf("VCEncLookaheadBypassRDOTest\n");
}

/*------------------------------------------------------------------------------
  VCEncMotionScoreTest
  ------------------------------------------------------------------------------*/
void VCEncMotionScoreTest(struct vcenc_instance *inst) {
  if (inst->asic.regs.asicCfg->bMultiPassSupport) {
    inst->multiPassMotionScoreEnable = 1;
  }
  printf("VCEncMotionScoreTest\n");
}

/*------------------------------------------------------------------------------
  HevcLtrTest
------------------------------------------------------------------------------*/
void HevcLtrIntervalTest(struct vcenc_instance *inst) {
  i32 poc = inst->poc;
  /*
  if(poc == 0) {
    inst->rateControl.ltrInterval = 4;
  } else if(poc > 1 && poc % inst->rateControl.ltrInterval == 1) {
    inst->rateControl.ltrInterval += (inst->rateControl.ltrInterval < 12 ? 4 : 12);
  }
  */
}
/*------------------------------------------------------------------------------
  HevcSliceTest
------------------------------------------------------------------------------*/
void HevcSliceTest(struct vcenc_instance *inst) {
  if ((inst->codecFormat == VCENC_VIDEO_CODEC_AV1) ||
      (inst->codecFormat == VCENC_VIDEO_CODEC_VP9))
    return;

  i32 vopNum = inst->frameCnt;

  int sliceSize = (vopNum) % inst->ctbPerCol;

  inst->asic.regs.sliceSize = sliceSize;
  inst->asic.regs.sliceNum =
      (sliceSize == 0) ? 1 : ((inst->ctbPerCol + (sliceSize - 1)) / sliceSize);

  inst->asic.regs.bCodingCtrlUpdate = 1;
  printf("HevcSliceTest# sliceSize %d\n", sliceSize);
}

/*------------------------------------------------------------------------------
  HevcStreamBufferLimitTest
------------------------------------------------------------------------------*/
void HevcStreamBufferLimitTest(struct vcenc_instance *inst,
                               VCEncStrmBufs *bufs) {
  static VCEncStrmBufs strmBufs;
  if (!inst) {
    if (bufs && strmBufs.buf[0] && strmBufs.bufLen[0]) *bufs = strmBufs;
    return;
  }

  regValues_s *swctrl = &(inst->asic.regs);
  if (swctrl->asicCfg->streamBufferChain && swctrl->outputStrmSize[1] &&
      (inst->parallelCoreNum <= 1)) {
    u32 size, size0, size1, offset0, offset1;

    size = inst->width * inst->height / (inst->frameCnt ? 16 : 4);
    if (inst->output_buffer_over_flow)
      size = 2 * (strmBufs.bufLen[0] + strmBufs.bufLen[1]);
    size0 = size * ((inst->frameCnt % 50) + 1) / 100;
    size1 = size - size0;
    size += inst->stream.byteCnt;
    size0 += inst->stream.byteCnt;
    offset1 = inst->frameCnt % 16;
    offset0 = inst->stream.byteCnt ? 0 :
              (inst->asic.regs.sliceNum != 1) ? 0 : (15 - offset1);

    size0 = MIN(size0, swctrl->outputStrmSize[0] - offset0);
    size1 = MIN(size1, swctrl->outputStrmSize[1] - offset1);
    swctrl->outputStrmSize[0] = size0;
    swctrl->outputStrmSize[1] = size1;
    swctrl->outputStrmBase[0] += offset0;
    swctrl->outputStrmBase[1] += offset1;

    printf(
        "HevcStreamBufferLimitTest# streamBuffer Addr %p %p Size %u + %u = %u "
        "bytes\n",
        (u8 *)swctrl->outputStrmBase[0], (u8 *)swctrl->outputStrmBase[1], size0,
        size1, size0 + size1);

    strmBufs.buf[0] = inst->streamBufs[0].buf[0] + offset0;
    strmBufs.buf[1] = inst->streamBufs[0].buf[1] + offset1;
    strmBufs.bufLen[0] = swctrl->outputStrmSize[0];
    strmBufs.bufLen[1] = swctrl->outputStrmSize[1];
    strmBufs.bufOffset[0] = offset0;
    strmBufs.bufOffset[1] = offset1;

    if (swctrl->sliceNum > 1) inst->streamBufs[0] = strmBufs;

    return;
  }

  static u32 firstFrame = 1;
  if (!firstFrame) return;

  firstFrame = 0;
  inst->asic.regs.outputStrmSize[0] = 4000;
  printf("HevcStreamBufferLimitTest# streamBufferLimit %u bytes\n",
         inst->asic.regs.outputStrmSize[0]);
}

/*------------------------------------------------------------------------------
  HevcQuantizationTest
  NOTE: ASIC resolution for wordCntTarget and wordError is value/4
------------------------------------------------------------------------------*/
void HevcMbQuantizationTest(struct vcenc_instance *inst) {
  i32 vopNum = inst->frameCnt;
  vcencRateControl_s *rc = &inst->rateControl;

  rc->qpMin = (MIN(51, vopNum / 4)) << QP_FRACTIONAL_BITS;
  rc->qpMax = (MAX(0, 51 - vopNum / 4)) << QP_FRACTIONAL_BITS;
  rc->qpMax = (MAX(rc->qpMax, rc->qpMin)) << QP_FRACTIONAL_BITS;

  rc->qpLastCoded = rc->qpTarget = rc->qpHdr =
      MIN(rc->qpMax, MAX(rc->qpMin, 26 << QP_FRACTIONAL_BITS));

  inst->asic.regs.bRateCtrlUpdate = 1;
  printf("HevcMbQuantTest# min %d  max %d  QP %d\n",
         rc->qpMin >> QP_FRACTIONAL_BITS, rc->qpMax >> QP_FRACTIONAL_BITS,
         rc->qpHdr >> QP_FRACTIONAL_BITS);
}

/*------------------------------------------------------------------------------
  HevcFilterTest
------------------------------------------------------------------------------*/
void HevcFilterTest(struct vcenc_instance *inst) {
  const i32 intra_period = 2;    // need pattern has intraPicRate=2.
  const i32 disable_period = 2;  // <=3 if corss slice deblock flag
  i32 vopNum = inst->frameCnt % (intra_period * disable_period * 13 * 2);
  regValues_s *regs = &inst->asic.regs;
  i32 enableDeblockOverride = inst->enableDeblockOverride;

  if (!enableDeblockOverride) {
    printf("HevcFilterTest# invalid deblock_filter_override_enable_flag.\n");
    return;
  }
  regs->slice_deblocking_filter_override_flag = 1;

  //inst->disableDeblocking = (vopNum/2)%3;
  //FIXME: not support disable across slice deblock now.

  inst->disableDeblocking = (vopNum / intra_period) % disable_period;

  if (vopNum == 0) {
    inst->tc_Offset = -6;
    inst->beta_Offset = 6;
  } else if (vopNum < (intra_period * disable_period * 13)) {
    if (vopNum % (intra_period * disable_period) == 0) {
      inst->tc_Offset += 1;
      inst->beta_Offset -= 1;
    }
  } else if (vopNum == (intra_period * disable_period * 13)) {
    inst->tc_Offset = -6;
    inst->beta_Offset = -6;
  } else if (vopNum < (intra_period * disable_period * 13 * 2)) {
    if (vopNum % (intra_period * disable_period) == 0) {
      inst->tc_Offset += 1;
      inst->beta_Offset += 1;
    }
  }

  inst->asic.regs.bCodingCtrlUpdate = 1;
  printf(
      "HevcFilterTest# disableDeblock = %d, filterOffA = %i filterOffB = %i\n",
      inst->disableDeblocking, inst->tc_Offset, inst->beta_Offset);
}

/*------------------------------------------------------------------------------
  HevcSegmentTest
------------------------------------------------------------------------------*/
void HevcSegmentTest(struct vcenc_instance *inst) {
#if 0
  u32 frame = (u32)inst->frameCnt;
  u32 *map = inst->asic.segmentMap.virtualAddress;
  u32 mbPerFrame = (inst->mbPerFrame + 7) / 8 * 8; /* Rounded upwards to 8 */
  u32 i, j;
  u32 mask;
  i32 qpSgm[4];

  inst->asic.regs.segmentEnable = 1;
  inst->asic.regs.segmentMapUpdate = 1;
  if (frame < 2)
  {
    for (i = 0; i < mbPerFrame / 8 / 4; i++)
    {
      map[i * 4 + 0] = 0x00000000;
      map[i * 4 + 1] = 0x11111111;
      map[i * 4 + 2] = 0x22222222;
      map[i * 4 + 3] = 0x33333333;
    }
  }
  else
  {
    for (i = 0; i < mbPerFrame / 8; i++)
    {
      mask = 0;
      for (j = 0; j < 8; j++)
        mask |= ((j + (frame - 2) / 2 + j * frame / 3) % 4) << (28 - j * 4);
      map[i] = mask;
    }
  }

  for (i = 0; i < 4; i++)
  {
    qpSgm[i] = (32 + i + frame / 2 + frame * i) % 52;
    qpSgm[i] = MAX(inst->rateControl.qpMin,
                   MIN(inst->rateControl.qpMax, qpSgm[i]));
  }
  inst->rateControl.qpHdr = qpSgm[0];
  inst->rateControl.mbQpAdjustment[0] = MAX(-8, MIN(7, qpSgm[1] - qpSgm[0]));
  inst->rateControl.mbQpAdjustment[1] = qpSgm[2] - qpSgm[0];
  inst->rateControl.mbQpAdjustment[2] = qpSgm[3] - qpSgm[0];

  printf("HevcSegmentTest# enable=%d update=%d map=0x%08x%08x%08x%08x\n",
         inst->asic.regs.segmentEnable, inst->asic.regs.segmentMapUpdate,
         map[0], map[1], map[2], map[3]);
  printf("HevcSegmentTest# qp=%d,%d,%d,%d\n",
         qpSgm[0], qpSgm[1], qpSgm[2], qpSgm[3]);
#endif
}

/*------------------------------------------------------------------------------
  HevcRefFrameTest
------------------------------------------------------------------------------*/
void HevcRefFrameTest(struct vcenc_instance *inst) {
#if 0
  i32 pic = inst->frameCnt;
  picBuffer *picBuffer = &inst->picBuffer;

  /* Only adjust for p-frames */
  if (picBuffer->cur_pic->p_frame)
  {
    picBuffer->cur_pic->ipf = pic & 0x1 ? 1 : 0;
    if (inst->seqParameterSet.numRefFrames > 1)
      picBuffer->cur_pic->grf = pic & 0x2 ? 1 : 0;
    picBuffer->refPicList[0].search = pic & 0x8 ? 1 : 0;
    if (inst->seqParameterSet.numRefFrames > 1)
      picBuffer->refPicList[1].search = pic & 0x10 ? 1 : 0;
  }

  printf("HevcRefFrameTest#\n");
#endif
}

/*------------------------------------------------------------------------------
  HevcTemporalLayerTest
------------------------------------------------------------------------------*/
void HevcTemporalLayerTest(struct vcenc_instance *inst) {
#if 0
  i32 pic = inst->frameCnt;
  picBuffer *picBuffer = &inst->picBuffer;

  /* Four temporal layers, base layer (LTR) every 8th frame. */
  if (inst->seqParameterSet.numRefFrames > 1)
    picBuffer->cur_pic->grf = pic & 0x7 ? 0 : 1;
  if (picBuffer->cur_pic->p_frame)
  {
    /* Odd frames don't update ipf */
    picBuffer->cur_pic->ipf = pic & 0x1 ? 0 : 1;
    /* Frames 1,2,3 & 4,5,6 reference prev */
    picBuffer->refPicList[0].search = pic & 0x3 ? 1 : 0;
    /* Every fourth frame (layers 0&1) reference LTR */
    if (inst->seqParameterSet.numRefFrames > 1)
      picBuffer->refPicList[1].search = pic & 0x3 ? 0 : 1;
  }

  printf("HevcTemporalLayer#\n");
#endif
}

/*------------------------------------------------------------------------------
  HevcSegmentMapTest
------------------------------------------------------------------------------*/
void HevcSegmentMapTest(struct vcenc_instance *inst) {
#if 0
  u32 frame = (u32)inst->frameCnt;
  u32 *map = inst->asic.segmentMap.virtualAddress;
  u32 mbPerFrame = (inst->mbPerFrame + 7) / 8 * 8; /* Rounded upwards to 8 */
  u32 i, j;
  u32 mask;
  i32 qpSgm[4];

  inst->asic.regs.segmentEnable = 1;
  inst->asic.regs.segmentMapUpdate = 1;
  if (frame < 2)
  {
    for (i = 0; i < mbPerFrame / 8; i++)
      map[i] = 0x01020120;
  }
  else
  {
    for (i = 0; i < mbPerFrame / 8; i++)
    {
      mask = 0;
      for (j = 0; j < 8; j++)
        mask |= ((j + frame) % 4) << (28 - j * 4);
      map[i] = mask;
    }
  }

  if (frame < 2)
  {
    qpSgm[0] = inst->rateControl.qpMax;
    qpSgm[1] = qpSgm[2] = qpSgm[3] = inst->rateControl.qpMin;
  }
  else
  {
    for (i = 0; i < 4; i++)
    {
      qpSgm[i] = (32 + i * frame) % 52;
      qpSgm[i] = MAX(inst->rateControl.qpMin,
                     MIN(inst->rateControl.qpMax, qpSgm[i]));
    }
  }
  inst->rateControl.qpHdr = qpSgm[0];
  inst->rateControl.mbQpAdjustment[0] = MAX(-8, MIN(7, qpSgm[1] - qpSgm[0]));
  inst->rateControl.mbQpAdjustment[1] = qpSgm[2] - qpSgm[0];
  inst->rateControl.mbQpAdjustment[2] = qpSgm[3] - qpSgm[0];

  printf("HevcSegmentMapTest# enable=%d update=%d map=0x%08x%08x%08x%08x\n",
         inst->asic.regs.segmentEnable, inst->asic.regs.segmentMapUpdate,
         map[0], map[1], map[2], map[3]);
  printf("HevcSegmentMapTest# qp=%d,%d,%d,%d\n",
         qpSgm[0], qpSgm[1], qpSgm[2], qpSgm[3]);
#endif
}

/*------------------------------------------------------------------------------
  HevcUserDataTest
------------------------------------------------------------------------------*/
void HevcUserDataTest(struct vcenc_instance *inst) {
#if 0
  static u8 *userDataBuf = NULL;
  i32 userDataLength = 16 + ((inst->frameCnt * 11) % 2000);
  i32 i;

  /* Allocate a buffer for user data, encoder reads data from this buffer
   * and writes it to the stream. TODO: This is never freed. */
  if (!userDataBuf)
    userDataBuf = (u8 *)EWLmalloc(2048);

  if (!userDataBuf)
    return;

  for (i = 0; i < userDataLength; i++)
  {
    /* Fill user data buffer with ASCII symbols from 48 to 125 */
    userDataBuf[i] = 48 + i % 78;
  }

  /* Enable user data insertion */
  inst->rateControl.sei.userDataEnabled = ENCHW_YES;
  inst->rateControl.sei.pUserData = userDataBuf;
  inst->rateControl.sei.userDataSize = userDataLength;

  printf("HevcUserDataTest# userDataSize %d\n", userDataLength);
#endif
}

/*------------------------------------------------------------------------------
  HevcIntra16FavorTest
------------------------------------------------------------------------------*/
void HevcIntra16FavorTest(struct vcenc_instance *inst) {
  asicData_s *asic = &inst->asic;

  asic->regs.intraPenaltyPic4x4 = 0x3ff;
  asic->regs.intraPenaltyPic8x8 = 0x1fff;
  //asic->regs.intraPenaltyPic16x16 = HEVCIntraPenaltyTu16[asic->regs.qp];
  //asic->regs.intraPenaltyPic32x32 = HEVCIntraPenaltyTu32[asic->regs.qp];

  asic->regs.intraPenaltyRoi14x4 = 0x3ff;
  asic->regs.intraPenaltyRoi18x8 = 0x1fff;
  //asic->regs.intraPenaltyRoi116x16 = HEVCIntraPenaltyTu16[asic->regs.qp-asic->regs.roi1DeltaQp];
  //asic->regs.intraPenaltyRoi132x32 = HEVCIntraPenaltyTu32[asic->regs.qp-asic->regs.roi1DeltaQp];

  asic->regs.intraPenaltyRoi24x4 = 0x3ff;
  asic->regs.intraPenaltyRoi28x8 = 0x1fff;
  //asic->regs.intraPenaltyRoi216x16 = HEVCIntraPenaltyTu16[asic->regs.qp-asic->regs.roi2DeltaQp];
  //asic->regs.intraPenaltyRoi232x32 = HEVCIntraPenaltyTu32[asic->regs.qp-asic->regs.roi2DeltaQp];

  printf("HevcIntra16FavorTest# intra16Favor. \n");
}

/*------------------------------------------------------------------------------
  HevcIntra32FavorTest
------------------------------------------------------------------------------*/
void HevcIntra32FavorTest(struct vcenc_instance *inst) {
  asicData_s *asic = &inst->asic;

  asic->regs.intraPenaltyPic4x4 = 0x3ff;
  asic->regs.intraPenaltyPic8x8 = 0x1fff;
  asic->regs.intraPenaltyPic16x16 = 0x3fff;
  //asic->regs.intraPenaltyPic32x32 = HEVCIntraPenaltyTu32[asic->regs.qp];

  asic->regs.intraPenaltyRoi14x4 = 0x3ff;
  asic->regs.intraPenaltyRoi18x8 = 0x1fff;
  asic->regs.intraPenaltyRoi116x16 = 0x3fff;
  //asic->regs.intraPenaltyRoi132x32 = HEVCIntraPenaltyTu32[asic->regs.qp-asic->regs.roi1DeltaQp];

  asic->regs.intraPenaltyRoi24x4 = 0x3ff;
  asic->regs.intraPenaltyRoi28x8 = 0x1fff;
  asic->regs.intraPenaltyRoi216x16 = 0x3fff;
  //asic->regs.intraPenaltyRoi232x32 = HEVCIntraPenaltyTu32[asic->regs.qp-asic->regs.roi2DeltaQp];

  printf("HevcIntra32FavorTest# intra32Favor. \n");
}

/*------------------------------------------------------------------------------
  HevcInterFavorTest
------------------------------------------------------------------------------*/
void HevcInterFavorTest(struct vcenc_instance *inst) {
#if 0
  i32 s;

  /* Force combinations of inter favor and skip penalty values */

  for (s = 0; s < 4; s++)
  {
    if ((inst->frameCnt % 3) == 0)
    {
      inst->asic.regs.pen[s][ASIC_PENALTY_INTER_FAVOR] = 0x7FFF;
    }
    else if ((inst->frameCnt % 3) == 1)
    {
      inst->asic.regs.pen[s][ASIC_PENALTY_SKIP] = 0;
      inst->asic.regs.pen[s][ASIC_PENALTY_INTER_FAVOR] = 0x7FFF;
    }
    else
    {
      inst->asic.regs.pen[s][ASIC_PENALTY_SKIP] = 0;
    }
  }

  printf("HevcInterFavorTest# interFavor %d skipPenalty %d\n",
         inst->asic.regs.pen[0][ASIC_PENALTY_INTER_FAVOR],
         inst->asic.regs.pen[0][ASIC_PENALTY_SKIP]);
#endif
}

/*------------------------------------------------------------------------------
  HevcCroppingTest
------------------------------------------------------------------------------*/
void HevcCroppingTest(struct vcenc_instance *inst) {
  u32 tileId;
  inst->preProcess.horOffsetSrc[0] = (inst->frameCnt % 8) * 2;
  if (EncPreProcessCheck(&inst->preProcess, inst->num_tile_columns) ==
      ENCHW_NOK)
    inst->preProcess.horOffsetSrc[0] = 0;
  for (tileId = 0; tileId < inst->num_tile_columns; tileId++) {
    inst->preProcess.verOffsetSrc[tileId] = (inst->frameCnt / 4) * 2;
    if (EncPreProcessCheck(&inst->preProcess, inst->num_tile_columns) ==
        ENCHW_NOK)
      inst->preProcess.verOffsetSrc[tileId] = 0;
  }

  inst->asic.regs.bPreprocessUpdate = 1;
  printf("HevcCroppingTest# horOffsetSrc %u  verOffsetSrc %u\n",
         inst->preProcess.horOffsetSrc[0], inst->preProcess.verOffsetSrc[0]);
}

/*------------------------------------------------------------------------------
  HevcRgbInputMaskTest
------------------------------------------------------------------------------*/
void HevcRgbInputMaskTest(struct vcenc_instance *inst) {
  u32 frameNum = (u32)inst->frameCnt;
  static u32 rMsb = 0;
  static u32 gMsb = 0;
  static u32 bMsb = 0;
  static u32 lsMask = 0; /* Lowest possible mask position */
  static u32 msMask = 0; /* Highest possible mask position */

  /* First frame normal
   * 1..29 step rMaskMsb values
   * 30..58 step gMaskMsb values
   * 59..87 step bMaskMsb values */
  if (frameNum == 0) {
    rMsb = inst->asic.regs.rMaskMsb;
    gMsb = inst->asic.regs.gMaskMsb;
    bMsb = inst->asic.regs.bMaskMsb;
    lsMask = MIN(rMsb, gMsb);
    lsMask = MIN(bMsb, lsMask);
    msMask = MAX(rMsb, gMsb);
    msMask = MAX(bMsb, msMask);
    if (msMask < 16)
      msMask = 15 - 2; /* 16bit RGB, 13 mask positions: 3..15  */
    else
      msMask = 31 - 2; /* 32bit RGB, 29 mask positions: 3..31 */
  } else if (frameNum <= msMask) {
    inst->asic.regs.rMaskMsb = MAX(frameNum + 2, lsMask);
    inst->asic.regs.gMaskMsb = gMsb;
    inst->asic.regs.bMaskMsb = bMsb;
  } else if (frameNum <= msMask * 2) {
    inst->asic.regs.rMaskMsb = rMsb;
    inst->asic.regs.gMaskMsb = MAX(frameNum - msMask + 2, lsMask);
    if (inst->asic.regs.inputImageFormat == 4) /* RGB 565 special case */
      inst->asic.regs.gMaskMsb = MAX(frameNum - msMask + 2, lsMask + 1);
    inst->asic.regs.bMaskMsb = bMsb;
  } else if (frameNum <= msMask * 3) {
    inst->asic.regs.rMaskMsb = rMsb;
    inst->asic.regs.gMaskMsb = gMsb;
    inst->asic.regs.bMaskMsb = MAX(frameNum - msMask * 2 + 2, lsMask);
  } else {
    inst->asic.regs.rMaskMsb = rMsb;
    inst->asic.regs.gMaskMsb = gMsb;
    inst->asic.regs.bMaskMsb = bMsb;
  }

  inst->asic.regs.bPreprocessUpdate = 1;
  printf("HevcRgbInputMaskTest#  %u %u %u\n", inst->asic.regs.rMaskMsb,
         inst->asic.regs.gMaskMsb, inst->asic.regs.bMaskMsb);
}

/*------------------------------------------------------------------------------
  HevcMadTest
------------------------------------------------------------------------------*/
void HevcMadTest(struct vcenc_instance *inst) {
#if 0
  u32 frameNum = (u32)inst->frameCnt;

  /* All values in range [-8,7] */
  inst->rateControl.mbQpAdjustment[0] = -8 + (frameNum % 16);
  inst->rateControl.mbQpAdjustment[1] = -127 + (frameNum % 254);
  inst->rateControl.mbQpAdjustment[2] = 127 - (frameNum % 254);
  /* Step 256, range [0,63*256] */
  inst->mad.threshold[0] = 256 * ((frameNum + 1) % 64);
  inst->mad.threshold[1] = 256 * ((frameNum / 2) % 64);
  inst->mad.threshold[2] = 256 * ((frameNum / 3) % 64);

  printf("HevcMadTest#  Thresholds: %d,%d,%d  QpDeltas: %d,%d,%d\n",
         inst->asic.regs.madThreshold[0],
         inst->asic.regs.madThreshold[1],
         inst->asic.regs.madThreshold[2],
         inst->rateControl.mbQpAdjustment[0],
         inst->rateControl.mbQpAdjustment[1],
         inst->rateControl.mbQpAdjustment[2]);
#endif
}

/*------------------------------------------------------------------------------
  HevcMvTest
------------------------------------------------------------------------------*/
void HevcMvTest(struct vcenc_instance *inst) {
  u32 tileId;
  u32 frame = (u32)inst->frameCnt;

  /* Set cropping offsets according to max MV length, decrement by frame
   * x = 32, 160, 32, 159, 32, 158, ..
   * y = 48, 80, 48, 79, 48, 78, .. */
  inst->preProcess.horOffsetSrc[0] =
      32 + (frame % 2) * 128 - (frame % 2) * (frame / 2);
  if (EncPreProcessCheck(&inst->preProcess, inst->num_tile_columns) ==
      ENCHW_NOK)
    inst->preProcess.horOffsetSrc[0] = 0;
  for (tileId = 0; tileId < inst->num_tile_columns; tileId++) {
    inst->preProcess.verOffsetSrc[tileId] =
        48 + (frame % 2) * 32 - (frame % 2) * (frame / 2);
    if (EncPreProcessCheck(&inst->preProcess, inst->num_tile_columns) ==
        ENCHW_NOK)
      inst->preProcess.verOffsetSrc[tileId] = 0;
  }

  inst->asic.regs.bPreprocessUpdate = 1;
  printf("HevcMvTest# horOffsetSrc %u  verOffsetSrc %u\n",
         inst->preProcess.horOffsetSrc[0], inst->preProcess.verOffsetSrc[0]);
}

/*------------------------------------------------------------------------------
  HevcDMVPenaltyTest
------------------------------------------------------------------------------*/
void HevcDMVPenaltyTest(struct vcenc_instance *inst) {
#if 0
  u32 frame = (u32)inst->frameCnt;
  i32 s;

  /* Set DMV penalty values to maximum and minimum */
  for (s = 0; s < 4; s++)
  {
    inst->asic.regs.pen[s][ASIC_PENALTY_DMV_4P] = frame % 2 ? 127 - frame / 2 : frame / 2;
    inst->asic.regs.pen[s][ASIC_PENALTY_DMV_1P] = frame % 2 ? 127 - frame / 2 : frame / 2;
    inst->asic.regs.pen[s][ASIC_PENALTY_DMV_QP] = frame % 2 ? 127 - frame / 2 : frame / 2;
  }

  printf("HevcDMVPenaltyTest# penalty4p %d  penalty1p %d  penaltyQp %d\n",
         inst->asic.regs.pen[0][ASIC_PENALTY_DMV_4P],
         inst->asic.regs.pen[0][ASIC_PENALTY_DMV_1P],
         inst->asic.regs.pen[0][ASIC_PENALTY_DMV_QP]);
#endif
}

/*------------------------------------------------------------------------------
  HevcMaxOverfillMv
------------------------------------------------------------------------------*/
void HevcMaxOverfillMv(struct vcenc_instance *inst) {
  u32 frame = (u32)inst->frameCnt;
  u32 tileId;

  /* Set cropping offsets according to max MV length.
   * In test cases the picture is selected so that this will
   * cause maximum horizontal MV to point into overfilled area. */
  inst->preProcess.horOffsetSrc[0] = 32 + (frame % 2) * 128;
  if (EncPreProcessCheck(&inst->preProcess, inst->num_tile_columns) ==
      ENCHW_NOK)
    inst->preProcess.horOffsetSrc[0] = 0;

  for (tileId = 0; tileId < inst->num_tile_columns; tileId++) {
    inst->preProcess.verOffsetSrc[tileId] = 176;
    if (EncPreProcessCheck(&inst->preProcess, inst->num_tile_columns) ==
        ENCHW_NOK)
      inst->preProcess.verOffsetSrc[tileId] = 0;
  }

  inst->asic.regs.bPreprocessUpdate = 1;
  printf("HevcMaxOverfillMv# horOffsetSrc %u  verOffsetSrc %u\n",
         inst->preProcess.horOffsetSrc[0], inst->preProcess.verOffsetSrc[0]);
}

/*------------------------------------------------------------------------------
  HevcRoiTest
------------------------------------------------------------------------------*/
void HevcRoiTest(struct vcenc_instance *inst) {
  regValues_s *regs = &inst->asic.regs;
  u32 frame = (u32)inst->frameCnt;
  u32 ctbPerRow = inst->ctbPerRow;
  u32 ctbPerCol = inst->ctbPerCol;
  u32 frames = MIN(ctbPerRow, ctbPerCol);
  u32 loop = frames * 3;

  if (regs->asicCfg->roiAbsQpSupport) {
    static i32 roi1DeltaQpS;
    static i32 roi2DeltaQpS;
    static i32 roi1QpS;
    static i32 roi2QpS;
    static i32 roi3DeltaQpS;
    static i32 roi3QpS;
    static i32 roi4DeltaQpS;
    static i32 roi4QpS;
    static i32 roi5DeltaQpS;
    static i32 roi5QpS;
    static i32 roi6DeltaQpS;
    static i32 roi6QpS;
    static i32 roi7DeltaQpS;
    static i32 roi7QpS;
    static i32 roi8DeltaQpS;
    static i32 roi8QpS;
    if (inst->frameCnt == 0) {
      roi1DeltaQpS = regs->roi1DeltaQp;
      roi2DeltaQpS = regs->roi2DeltaQp;
      roi1QpS = regs->roi1Qp;
      roi2QpS = regs->roi2Qp;
      if (regs->asicCfg->ROI8Support) {
        roi3DeltaQpS = regs->roi3DeltaQp;
        roi3QpS = regs->roi3Qp;
        roi4DeltaQpS = regs->roi4DeltaQp;
        roi4QpS = regs->roi4Qp;
        roi5DeltaQpS = regs->roi5DeltaQp;
        roi5QpS = regs->roi5Qp;
        roi6DeltaQpS = regs->roi6DeltaQp;
        roi6QpS = regs->roi6Qp;
        roi7DeltaQpS = regs->roi7DeltaQp;
        roi7QpS = regs->roi7Qp;
        roi8DeltaQpS = regs->roi8DeltaQp;
        roi8QpS = regs->roi8Qp;
      }
    }

    if (regs->roi1Qp >= 0)
      regs->roi1Qp = (roi1QpS + inst->frameCnt) % 52;
    else {
      i32 mod = roi1DeltaQpS >= 0 ? 32 : 33;
      i32 tmp = (ABS(roi1DeltaQpS) + inst->frameCnt) % mod;
      regs->roi1DeltaQp = roi1DeltaQpS >= 0 ? tmp : -tmp;
    }

    if (regs->roi2Qp >= 0)
      regs->roi2Qp = (roi2QpS + inst->frameCnt) % 52;
    else {
      i32 mod = roi2DeltaQpS >= 0 ? 32 : 33;
      i32 tmp = (ABS(roi2DeltaQpS) + inst->frameCnt) % mod;
      regs->roi2DeltaQp = roi2DeltaQpS >= 0 ? tmp : -tmp;
    }

    if (regs->asicCfg->ROI8Support) {
      if (regs->roi3Qp >= 0)
        regs->roi3Qp = (roi3QpS + inst->frameCnt) % 52;
      else {
        i32 mod = roi3DeltaQpS >= 0 ? 32 : 33;
        i32 tmp = (ABS(roi3DeltaQpS) + inst->frameCnt) % mod;
        regs->roi3DeltaQp = roi3DeltaQpS >= 0 ? tmp : -tmp;
      }

      if (regs->roi4Qp >= 0)
        regs->roi4Qp = (roi4QpS + inst->frameCnt) % 52;
      else {
        i32 mod = roi4DeltaQpS >= 0 ? 32 : 33;
        i32 tmp = (ABS(roi4DeltaQpS) + inst->frameCnt) % mod;
        regs->roi4DeltaQp = roi4DeltaQpS >= 0 ? tmp : -tmp;
      }

      if (regs->roi5Qp >= 0)
        regs->roi5Qp = (roi5QpS + inst->frameCnt) % 52;
      else {
        i32 mod = roi5DeltaQpS >= 0 ? 32 : 33;
        i32 tmp = (ABS(roi5DeltaQpS) + inst->frameCnt) % mod;
        regs->roi5DeltaQp = roi5DeltaQpS >= 0 ? tmp : -tmp;
      }

      if (regs->roi6Qp >= 0)
        regs->roi6Qp = (roi6QpS + inst->frameCnt) % 52;
      else {
        i32 mod = roi6DeltaQpS >= 0 ? 32 : 33;
        i32 tmp = (ABS(roi6DeltaQpS) + inst->frameCnt) % mod;
        regs->roi6DeltaQp = roi6DeltaQpS >= 0 ? tmp : -tmp;
      }

      if (regs->roi7Qp >= 0)
        regs->roi7Qp = (roi7QpS + inst->frameCnt) % 52;
      else {
        i32 mod = roi7DeltaQpS >= 0 ? 32 : 33;
        i32 tmp = (ABS(roi7DeltaQpS) + inst->frameCnt) % mod;
        regs->roi7DeltaQp = roi7DeltaQpS >= 0 ? tmp : -tmp;
      }

      if (regs->roi8Qp >= 0)
        regs->roi8Qp = (roi8QpS + inst->frameCnt) % 52;
      else {
        i32 mod = roi8DeltaQpS >= 0 ? 32 : 33;
        i32 tmp = (ABS(roi8DeltaQpS) + inst->frameCnt) % mod;
        regs->roi8DeltaQp = roi8DeltaQpS >= 0 ? tmp : -tmp;
      }
    }

    if (inst->frameCnt) {
      regs->roi1Left = getRandU(inst, 0, ctbPerRow - 1);
      regs->roi1Top = getRandU(inst, 0, ctbPerCol - 1);
      regs->roi1Right = getRandU(inst, regs->roi1Left, ctbPerRow - 1);
      regs->roi1Bottom = getRandU(inst, regs->roi1Top, ctbPerCol - 1);
      regs->roi2Left = getRandU(inst, 0, ctbPerRow - 1);
      regs->roi2Top = getRandU(inst, 0, ctbPerCol - 1);
      regs->roi2Right = getRandU(inst, regs->roi2Left, ctbPerRow - 1);
      regs->roi2Bottom = getRandU(inst, regs->roi2Top, ctbPerCol - 1);

      if (regs->asicCfg->ROI8Support) {
        regs->roi3Left = getRandU(inst, 0, ctbPerRow - 1);
        regs->roi3Top = getRandU(inst, 0, ctbPerCol - 1);
        regs->roi3Right = getRandU(inst, regs->roi3Left, ctbPerRow - 1);
        regs->roi3Bottom = getRandU(inst, regs->roi3Top, ctbPerCol - 1);

        regs->roi4Left = getRandU(inst, 0, ctbPerRow - 1);
        regs->roi4Top = getRandU(inst, 0, ctbPerCol - 1);
        regs->roi4Right = getRandU(inst, regs->roi4Left, ctbPerRow - 1);
        regs->roi4Bottom = getRandU(inst, regs->roi4Top, ctbPerCol - 1);

        regs->roi5Left = getRandU(inst, 0, ctbPerRow - 1);
        regs->roi5Top = getRandU(inst, 0, ctbPerCol - 1);
        regs->roi5Right = getRandU(inst, regs->roi5Left, ctbPerRow - 1);
        regs->roi5Bottom = getRandU(inst, regs->roi5Top, ctbPerCol - 1);

        regs->roi6Left = getRandU(inst, 0, ctbPerRow - 1);
        regs->roi6Top = getRandU(inst, 0, ctbPerCol - 1);
        regs->roi6Right = getRandU(inst, regs->roi6Left, ctbPerRow - 1);
        regs->roi6Bottom = getRandU(inst, regs->roi6Top, ctbPerCol - 1);

        regs->roi7Left = getRandU(inst, 0, ctbPerRow - 1);
        regs->roi7Top = getRandU(inst, 0, ctbPerCol - 1);
        regs->roi7Right = getRandU(inst, regs->roi7Left, ctbPerRow - 1);
        regs->roi7Bottom = getRandU(inst, regs->roi7Top, ctbPerCol - 1);

        regs->roi8Left = getRandU(inst, 0, ctbPerRow - 1);
        regs->roi8Top = getRandU(inst, 0, ctbPerCol - 1);
        regs->roi8Right = getRandU(inst, regs->roi8Left, ctbPerRow - 1);
        regs->roi8Bottom = getRandU(inst, regs->roi8Top, ctbPerCol - 1);
      }
    }
  } else {
    /* Loop after this many encoded frames */
    frame = frame % loop;
    regs->roi1DeltaQp = (frame % 15) + 1;
    regs->roi2DeltaQp = 15 - (frame % 15);

    /* Set two ROI areas according to frame dimensions. */
    if (frame < frames) {
      /* ROI1 in top-left corner, ROI2 in bottom-right corner */
      regs->roi1Left = regs->roi1Top = 0;
      regs->roi1Right = regs->roi1Bottom = frame;
      regs->roi2Left = ctbPerRow - 1 - frame;
      regs->roi2Top = ctbPerCol - 1 - frame;
      regs->roi2Right = ctbPerRow - 1;
      regs->roi2Bottom = ctbPerCol - 1;
    } else if (frame < frames * 2) {
      /* ROI1 gets smaller towards top-right corner,
       * ROI2 towards bottom-left corner */
      frame -= frames;
      regs->roi1Left = frame;
      regs->roi1Top = 0;
      regs->roi1Right = ctbPerRow - 1;
      regs->roi1Bottom = ctbPerCol - 1 - frame;
      regs->roi2Left = 0;
      regs->roi2Top = frame;
      regs->roi2Right = ctbPerRow - 1 - frame;
      regs->roi2Bottom = ctbPerCol - 1;
    } else if (frame < frames * 3) {
      /* 1x1/2x2 ROIs moving diagonal across frame */
      frame -= frames * 2;
      regs->roi1Left = frame - frame % 2;
      regs->roi1Right = frame;
      regs->roi1Top = frame - frame % 2;
      regs->roi1Bottom = frame;
      regs->roi2Left = frame - frame % 2;
      regs->roi2Right = frame;
      regs->roi2Top = ctbPerCol - 1 - frame;
      regs->roi2Bottom = ctbPerCol - 1 - frame + frame % 2;
    }
  }

  regs->roiUpdate = 1;
  printf("HevcRoiTest# ROI1:%d x%uy%u-x%uy%u  ROI2:%d x%uy%u-x%uy%u\n",
         regs->roi1DeltaQp, regs->roi1Left, regs->roi1Top, regs->roi1Right,
         regs->roi1Bottom, regs->roi2DeltaQp, regs->roi2Left, regs->roi2Top,
         regs->roi2Right, regs->roi2Bottom);

  if (regs->asicCfg->ROI8Support) {
    printf("HevcRoiTest# ROI3:%d x%uy%u-x%uy%u  ROI4:%d x%uy%u-x%uy%u\n",
           regs->roi3DeltaQp, regs->roi3Left, regs->roi3Top, regs->roi3Right,
           regs->roi3Bottom, regs->roi4DeltaQp, regs->roi4Left, regs->roi4Top,
           regs->roi4Right, regs->roi4Bottom);

    printf("HevcRoiTest# ROI5:%d x%uy%u-x%uy%u  ROI6:%d x%uy%u-x%uy%u\n",
           regs->roi5DeltaQp, regs->roi5Left, regs->roi5Top, regs->roi5Right,
           regs->roi5Bottom, regs->roi6DeltaQp, regs->roi6Left, regs->roi6Top,
           regs->roi6Right, regs->roi6Bottom);

    printf("HevcRoiTest# ROI7:%d x%uy%u-x%uy%u  ROI8:%d x%uy%u-x%uy%u\n",
           regs->roi7DeltaQp, regs->roi7Left, regs->roi7Top, regs->roi7Right,
           regs->roi7Bottom, regs->roi8DeltaQp, regs->roi8Left, regs->roi8Top,
           regs->roi8Right, regs->roi8Bottom);
  }
}

/*------------------------------------------------------------------------------
  HevcIPCMTest
------------------------------------------------------------------------------*/
void HevcIPCMTest(struct vcenc_instance *inst) {
  regValues_s *regs = &inst->asic.regs;
  u32 frame = (u32)inst->frameCnt;
  u32 ctbPerRow = inst->ctbPerRow;
  u32 ctbPerCol = inst->ctbPerCol;
  u32 frames = MIN(ctbPerRow, ctbPerCol);
  u32 loop = frames * 3;
  //VP9 and AV1 does not support IPCM
  if (inst->codecFormat == VCENC_VIDEO_CODEC_AV1 || inst->codecFormat == VCENC_VIDEO_CODEC_VP9)
    return;
  /* Loop after this many encoded frames */
  frame = frame % loop;

  /* Set two IPCM areas according to frame dimensions. */
  if (frame < frames) {
    /* IPCM1 in top-left corner, IPCM2 in bottom-right corner */
    regs->ipcm1AreaLeft = regs->ipcm1AreaTop = 0;
    regs->ipcm1AreaRight = regs->ipcm1AreaBottom = frame;
    regs->ipcm2AreaLeft = ctbPerRow - 1 - frame;
    regs->ipcm2AreaTop = ctbPerCol - 1 - frame;
    regs->ipcm2AreaRight = ctbPerRow - 1;
    regs->ipcm2AreaBottom = ctbPerCol - 1;
  } else if (frame < frames * 2) {
    /* IPCM1 gets smaller towards top-right corner,
     * IPCM2 towards bottom-left corner */
    frame -= frames;
    regs->ipcm1AreaLeft = frame;
    regs->ipcm1AreaTop = 0;
    regs->ipcm1AreaRight = ctbPerRow - 1;
    regs->ipcm1AreaBottom = ctbPerCol - 1 - frame;
    regs->ipcm2AreaLeft = 0;
    regs->ipcm2AreaTop = frame;
    regs->ipcm2AreaRight = ctbPerRow - 1 - frame;
    regs->ipcm2AreaBottom = ctbPerCol - 1;
  } else if (frame < frames * 3) {
    /* 1x1/2x2 IPCMs moving diagonal across frame */
    frame -= frames * 2;
    regs->ipcm1AreaLeft = frame - frame % 2;
    regs->ipcm1AreaRight = frame;
    regs->ipcm1AreaTop = frame - frame % 2;
    regs->ipcm1AreaBottom = frame;
    regs->ipcm2AreaLeft = frame - frame % 2;
    regs->ipcm2AreaRight = frame;
    regs->ipcm2AreaTop = ctbPerCol - 1 - frame;
    regs->ipcm2AreaBottom = ctbPerCol - 1 - frame + frame % 2;
  }

  inst->asic.regs.bCodingCtrlUpdate = 1;

  printf("HevcIPCMTest# IPCM1: x%uy%u-x%uy%u  IPCM2: x%uy%u-x%uy%u\n",
         regs->ipcm1AreaLeft, regs->ipcm1AreaTop, regs->ipcm1AreaRight,
         regs->ipcm1AreaBottom, regs->ipcm2AreaLeft, regs->ipcm2AreaTop,
         regs->ipcm2AreaRight, regs->ipcm2AreaBottom);
}

/*------------------------------------------------------------------------------
  HevcIntraAreaTest
------------------------------------------------------------------------------*/
void HevcIntraAreaTest(struct vcenc_instance *inst) {
  regValues_s *regs = &inst->asic.regs;
  u32 frame = (u32)inst->frameCnt;
  u32 ctbPerRow = inst->ctbPerRow;
  u32 ctbPerCol = inst->ctbPerCol;
  u32 frames = MIN(ctbPerRow, ctbPerCol);
  u32 loop = frames * 3;

  /* Loop after this many encoded frames */
  frame = frame % loop;

  if (frame < frames) {
    /* Intra area in top-left corner, gets bigger every frame */
    regs->intraAreaLeft = regs->intraAreaTop = 0;
    regs->intraAreaRight = regs->intraAreaBottom = frame;
  } else if (frame < frames * 2) {
    /* Intra area gets smaller towards top-right corner */
    frame -= frames;
    regs->intraAreaLeft = frame;
    regs->intraAreaTop = 0;
    regs->intraAreaRight = ctbPerRow - 1;
    regs->intraAreaBottom = ctbPerCol - 1 - frame;
  } else if (frame < frames * 3) {
    /* 1x1/2x2 Intra area moving diagonal across frame */
    frame -= frames * 2;
    regs->intraAreaLeft = frame - frame % 2;
    regs->intraAreaRight = frame;
    regs->intraAreaTop = frame - frame % 2;
    regs->intraAreaBottom = frame;
  }

  regs->roiUpdate = 1;
  printf("HevcIntraAreaTest# x%uy%u-x%uy%u\n", regs->intraAreaLeft,
         regs->intraAreaTop, regs->intraAreaRight, regs->intraAreaBottom);
}

/*------------------------------------------------------------------------------
  HevcCirTest
------------------------------------------------------------------------------*/
void HevcCirTest(struct vcenc_instance *inst) {
  regValues_s *regs = &inst->asic.regs;
  u32 frame = (u32)inst->frameCnt;
  u32 ctbPerRow = inst->ctbPerRow;
  u32 ctbPerFrame = inst->ctbPerFrame;
  u32 loop = inst->ctbPerFrame + 6;

  /* Loop after this many encoded frames */
  frame = frame % loop;

  switch (frame) {
    case 0:
    case 1:
      regs->cirStart = 0;
      regs->cirInterval = 1;
      break;
    case 2:
      regs->cirStart = 0;
      regs->cirInterval = 2;
      break;
    case 3:
      regs->cirStart = 0;
      regs->cirInterval = 3;
      break;
    case 4:
      regs->cirStart = 0;
      regs->cirInterval = ctbPerRow;
      break;
    case 5:
      regs->cirStart = 0;
      regs->cirInterval = ctbPerRow + 1;
      break;
    case 6:
      regs->cirStart = 0;
      regs->cirInterval = ctbPerFrame - 1;
      break;
    case 7:
      regs->cirStart = ctbPerFrame - 1;
      regs->cirInterval = 1;
      break;
    default:
      regs->cirStart = frame - 7;
      regs->cirInterval = (ctbPerFrame - frame) % (ctbPerRow * 2);
      break;
  }

  regs->bCodingCtrlUpdate = 1;
  printf("HevcCirTest# start:%u interval:%u\n", regs->cirStart,
         regs->cirInterval);
}

/*------------------------------------------------------------------------------
  HevcIntraSliceMapTest
------------------------------------------------------------------------------*/
void HevcIntraSliceMapTest(struct vcenc_instance *inst) {
#if 0
  u32 frame = (u32)inst->frameCnt;
  u32 mbPerCol = inst->mbPerCol;

  frame = frame % (mbPerCol * 10);

  if (frame <= 1)
  {
    inst->slice.sliceSize = inst->mbPerRow;
    inst->intraSliceMap[0] = 0x55555555;
    inst->intraSliceMap[1] = 0x55555555;
    inst->intraSliceMap[2] = 0x55555555;
  }
  else if (frame < mbPerCol + 1)
  {
    inst->slice.sliceSize = inst->mbPerRow * (frame - 1);
    inst->intraSliceMap[0] = 0xAAAAAAAA;
    inst->intraSliceMap[1] = 0xAAAAAAAA;
    inst->intraSliceMap[2] = 0xAAAAAAAA;
  }
  else
  {
    inst->slice.sliceSize = inst->mbPerRow * (frame % mbPerCol);
    inst->intraSliceMap[0] += frame;
    inst->intraSliceMap[1] += frame;
    inst->intraSliceMap[2] += frame;
  }

  printf("HevcIntraSliceMapTest# "
         "sliceSize: %d  map1: 0x%x  map2: 0x%x  map3: 0x%x \n",
         inst->slice.sliceSize, inst->intraSliceMap[0],
         inst->intraSliceMap[1], inst->intraSliceMap[2]);
#endif
}

/*------------------------------------------------------------------------------
  HevcRFDTest
------------------------------------------------------------------------------*/
void HevcRFDTest(struct vcenc_instance *inst, struct sw_picture *pic) {
  // test RFD with corrupted table
  // expected to fail, but not hang
  u32 i;
  asicData_s *asic = &inst->asic;
  u32 lumaTblSize, chromaTblSize = 0;
  EncGetCompressTableSize(asic->regs.codedChromaIdc, inst->compressor,
        pic->sps->width, pic->sps->height, &lumaTblSize, &chromaTblSize);
  u32 MemSyncSize = 0;
  MemSyncSize += lumaTblSize;
  if (pic->recon_compress.lumaCompressed) {
    u8 *lum_table =
        (u8 *)(asic->compressTbl[pic->picture_memeory_id].virtualAddress);
    u8 *lum =
        (u8 *)(asic->internalreconLuma[pic->picture_memeory_id].virtualAddress);
    if (pic->poc & 1) {
      // disturb compress table
      for (i = 0; i < lumaTblSize; i++) lum_table[i] = (i >> 2);
    } else {
      // disturb compressed data, one value per AU
      for (i = 0; (int)i < pic->recon.lum_height * pic->recon.lum_width; i++)
        lum[i] = (i >> 9);
    }
  }
  if (pic->recon_compress.chromaCompressed) {
    u8 *chroma_table =
        ((u8 *)(asic->compressTbl[pic->picture_memeory_id].virtualAddress)) +
        lumaTblSize;
    for (i = 0; i < chromaTblSize; i += 16)
      chroma_table[i] = chroma_table[i + 1] = i;

    MemSyncSize += chromaTblSize;
  }
  EWLSyncMemData(&asic->compressTbl[pic->picture_memeory_id], 0, MemSyncSize,
                 HOST_TO_DEVICE);
}

/*------------------------------------------------------------------------------
  HevcPenaltyTest
------------------------------------------------------------------------------*/
void HevcPenaltyTest(struct vcenc_instance *inst) {
#if 0
  i32 s;

  inst->asic.regs.inputReadChunk = 1;

  /* Set non-zero values */
  for (s = 0; s < 4; s++)
  {
    inst->asic.regs.pen[s][ASIC_PENALTY_SPLIT16x8] = 1;
    inst->asic.regs.pen[s][ASIC_PENALTY_SPLIT8x8] = 2;
    inst->asic.regs.pen[s][ASIC_PENALTY_SPLIT8x4] = 3;
    inst->asic.regs.pen[s][ASIC_PENALTY_SPLIT4x4] = 4;
    inst->asic.regs.pen[s][ASIC_PENALTY_SPLIT_ZERO] = 5;
  }

  printf("HevcPenaltyTest# splitPenalty %d %d %d %d %d\n",
         inst->asic.regs.pen[0][ASIC_PENALTY_SPLIT16x8],
         inst->asic.regs.pen[0][ASIC_PENALTY_SPLIT8x8],
         inst->asic.regs.pen[0][ASIC_PENALTY_SPLIT8x4],
         inst->asic.regs.pen[0][ASIC_PENALTY_SPLIT4x4],
         inst->asic.regs.pen[0][ASIC_PENALTY_SPLIT_ZERO]);
#endif
}

/*------------------------------------------------------------------------------
  HevcDownscaling
------------------------------------------------------------------------------*/
void HevcDownscalingTest(struct vcenc_instance *inst) {
  u32 frame = (u32)inst->frameCnt;
  u32 width = MIN(DOWN_SCALING_MAX_WIDTH, inst->preProcess.lumWidth);
  u32 xy = MIN(width, inst->preProcess.lumHeight);

  if (!frame) return;

  if (frame <= xy / 2) {
    inst->preProcess.scaledWidth = width - (frame / 2) * 2;
    inst->preProcess.scaledHeight = inst->preProcess.lumHeight - frame * 2;
  } else {
    u32 i, x, y;
    i = frame - xy / 2;
    x = i % (width / 8);
    y = i / (width / 8);
    inst->preProcess.scaledWidth = width - x * 2;
    inst->preProcess.scaledHeight = inst->preProcess.lumHeight - y * 2;
  }

  if (!inst->preProcess.scaledWidth) inst->preProcess.scaledWidth = width - 8;

  if (!inst->preProcess.scaledHeight)
    inst->preProcess.scaledHeight = inst->preProcess.lumHeight - 8;

  inst->asic.regs.bPreprocessUpdate = 1;
  printf("HevcDownscalingTest# %ux%u => %ux%u\n", inst->preProcess.lumWidth,
         inst->preProcess.lumHeight, inst->preProcess.scaledWidth,
         inst->preProcess.scaledHeight);
}

void VCEncSmartModeTest(struct vcenc_instance *inst) {
  i32 i;
  if (inst->frameCnt == 0) {
    inst->smartHevcLumQp = inst->smartH264Qp = getRandU(inst, 25, 35);
    inst->smartHevcChrQp = getRandU(inst, 25, 35);
  } else {
    inst->smartHevcLumQp = (inst->smartHevcLumQp + 1) % 52;
    inst->smartHevcChrQp = (inst->smartHevcChrQp + 1) % 52;
    inst->smartH264Qp = (inst->smartH264Qp + 1) % 52;
  }

  inst->smartH264LumDcTh = getRandU(inst, 1, 20);
  inst->smartH264CbDcTh = getRandU(inst, 1, 10);
  inst->smartH264CrDcTh = getRandU(inst, 1, 10);
  for (i = 0; i < 3; i++) {
    i32 size = 1 << ((i + 3) * 2);
    inst->smartHevcLumDcTh[i] = getRandU(inst, 1, 20);
    inst->smartHevcChrDcTh[i] = getRandU(inst, 1, 10);
    inst->smartHevcLumAcNumTh[i] = getRandU(inst, 1, size / 2);
    inst->smartHevcChrAcNumTh[i] = getRandU(inst, 1, size / 2);
  }
  for (i = 0; i < 4; i++) inst->smartMeanTh[i] = getRandU(inst, 0, 20);

  inst->smartPixNumCntTh = getRandU(inst, 0, 20);

  inst->asic.regs.bCodingCtrlUpdate = 1;
  printf("BgTest# Rand Bg_QP = (%d, %d)\n", inst->smartHevcLumQp,
         inst->smartHevcChrQp);
}

void VCEncConstChromaTest(struct vcenc_instance *inst) {
  // the first two frames just use input const chroma
  if (inst->frameCnt > 1) {
    u32 maxCh = (1 << (inst->sps->bit_depth_chroma_minus8 + 8)) - 1;
    inst->preProcess.constCb = getRandU(inst, 0, maxCh);
    inst->preProcess.constCr = getRandU(inst, 0, maxCh);
  }
  inst->asic.regs.bPreprocessUpdate = 1;
  printf("ConstChromaTest# ConstCb = %u, ConstCr = %u\n",
         inst->preProcess.constCb, inst->preProcess.constCr);
}

void VCEncCtbRcTest(struct vcenc_instance *inst) {
  regValues_s *regs = &(inst->asic.regs);
  u32 maxCtbRcQpDelta = (regs->asicCfg->ctbRcVersion > 1) ? 51 : 15;
  i32 n = inst->frameCnt % 51;
  u32 delta = inst->rateControl.rcQpDeltaRange;

  inst->rateControl.tolCtbRcInter = (float)n / 10;
  inst->rateControl.tolCtbRcIntra = inst->rateControl.tolCtbRcInter * 2;

  if (inst->frameCnt > 1)
    inst->rateControl.rcQpDeltaRange = delta ? (delta - 1) : maxCtbRcQpDelta;

  inst->asic.regs.bRateCtrlUpdate = 1;
  printf("CtbRcTest# tolctbrcinter = %f, tolctbrcintra = %f, deltaRange = %u\n",
         (float)inst->rateControl.tolCtbRcInter, (float)inst->rateControl.tolCtbRcIntra,
         inst->rateControl.rcQpDeltaRange);
}

void VCEncCtbRc1TestV2(struct vcenc_instance *inst) {
  asicData_s *asic = &inst->asic;
  if (asic->regs.asicCfg->ctbRcVersion != 2) return;
  u32 i = 0;
  /* ctbRc complexity direction and threshold */
  u32 compBias = RAND(0, 20) <= 10;
  u32 delta = RAND(0, 64) / RAND(4, 16);
  inst->ctbRcDirection = RAND(0, 15);
  inst->ctbRcThresholdI[i] = compBias ? 0 : RAND(0, delta);
  inst->ctbRcThresholdP[i] = compBias ? 0 : RAND(0, delta);
  inst->ctbRcThresholdB[i] = compBias ? 0 : RAND(0, delta);
  for (i = 1; i < 16; i++) {
    delta = RAND(0, 64) / RAND(4, 16);
    inst->ctbRcThresholdI[i] = RAND(inst->ctbRcThresholdI[i - 1], MIN(inst->ctbRcThresholdI[i - 1] + delta, 255));
    inst->ctbRcThresholdP[i] = RAND(inst->ctbRcThresholdP[i - 1], MIN(inst->ctbRcThresholdP[i - 1] + delta, 255));
    inst->ctbRcThresholdB[i] = RAND(inst->ctbRcThresholdB[i - 1], MIN(inst->ctbRcThresholdB[i - 1] + delta, 255));
  }
  /* skin detection*/
  u32 nSkinCostomize = RAND(0, 20) <= 5;
  inst->ctbRcSkinQPDelta = RAND(0, 7);
  inst->ctbRcSkinMinQPDelta = RAND(0, inst->ctbRcDirection);
  inst->ctbRcSkinLumMin = nSkinCostomize ? 0 : RAND(0, 255);
  inst->ctbRcSkinLumMax = nSkinCostomize ? 0 : RAND(inst->ctbRcSkinLumMin, 255);
  inst->ctbRcSkinCbMin = nSkinCostomize ? 0 : RAND(0, 255);
  inst->ctbRcSkinCbMax = nSkinCostomize ? 0 : RAND(inst->ctbRcSkinCbMin, 255);
  inst->ctbRcSkinCrMin = nSkinCostomize ? 0 : RAND(0, 255);
  inst->ctbRcSkinCrMax = nSkinCostomize ? 0 : RAND(inst->ctbRcSkinCrMin, 255);
  /* edge detection */
  inst->ctbRcEdgeTh = RAND(0, 127);
  inst->ctbRcEdgeMaxQpDelta = RAND(inst->ctbRcDirection, 15);
  asic->regs.bCodingCtrlUpdate = 1;
  u32 *thresholdI = inst->ctbRcThresholdI;
  u32 *thresholdP = inst->ctbRcThresholdP;
  u32 *thresholdB = inst->ctbRcThresholdB;

  printf("CtbRc1TestV2# Comp: Direction %u\n", inst->ctbRcDirection);
  printf("CtbRc1TestV2# Comp: ThresholdI[%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u]\n",
          thresholdI[0], thresholdI[1], thresholdI[2], thresholdI[3],
          thresholdI[4], thresholdI[5], thresholdI[6], thresholdI[7],
          thresholdI[8], thresholdI[9], thresholdI[10], thresholdI[11],
          thresholdI[12], thresholdI[13], thresholdI[14], thresholdI[15]);
  printf("CtbRc1TestV2# Comp: thresholdP[%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u]\n",
          thresholdP[0], thresholdP[1], thresholdP[2], thresholdP[3],
          thresholdP[4], thresholdP[5], thresholdP[6], thresholdP[7],
          thresholdP[8], thresholdP[9], thresholdP[10], thresholdP[11],
          thresholdP[12], thresholdP[13], thresholdP[14], thresholdP[15]);
  printf("CtbRc1TestV2# Comp: thresholdB[%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u]\n",
          thresholdB[0], thresholdB[1], thresholdB[2], thresholdB[3],
          thresholdB[4], thresholdB[5], thresholdB[6], thresholdB[7],
          thresholdB[8], thresholdB[9], thresholdB[10], thresholdB[11],
          thresholdB[12], thresholdB[13], thresholdB[14], thresholdB[15]);
  printf("CtbRc1TestV2# Skin: QPDelta = %u, MinQPDelta = %u, Lum(%u-%u) Cb(%u-%u) Cr(%u-%u)\n",
         inst->ctbRcSkinQPDelta, inst->ctbRcSkinMinQPDelta, inst->ctbRcSkinLumMin, inst->ctbRcSkinLumMax,
         inst->ctbRcSkinCbMin, inst->ctbRcSkinCbMax, inst->ctbRcSkinCrMin, inst->ctbRcSkinCbMax);
  printf("CtbRc1TestV2# Edge: EdgeTh = %u, MaxQpDelta = %u\n", inst->ctbRcEdgeTh, inst->ctbRcEdgeMaxQpDelta);
}

void VCEncCtbRcRowQpConstraint(struct vcenc_instance *inst)
{
  asicData_s *asic = &inst->asic;
  if (asic->regs.asicCfg->ctbRcVersion != 2) return;
  inst->rateControl.ctbRateCtrl.qpStep = asic->regs.ctbRcQpStep = RAND(0, 7);
  inst->rateControl.ctbRateCtrl.ctbRcRowQpDeltaRange = asic->regs.ctbRcRowQpDeltaRange = RAND(0, 31);
}

/* ctbRc Version2 extreme parameters test */
void VCEncCtbRc3ExtremeTestV2(struct vcenc_instance *inst)
{
  asicData_s *asic = &inst->asic;
  if (asic->regs.asicCfg->ctbRcVersion != 2) return;
  u32 extremeBias = RAND(0, 20) <= 15;
  u32 extremeQpStep = RAND(0, 4) ? 7 : 0;
  u32 extremeRowQpDeltaRange = RAND(0, 4) ? 31 : 0;
  /* ctbRc qpStep and RowQpDeltaRange */
  inst->rateControl.ctbRateCtrl.qpStep = asic->regs.ctbRcQpStep =
        extremeBias ? extremeQpStep : RAND(0, 7);
  inst->rateControl.ctbRateCtrl.ctbRcRowQpDeltaRange = asic->regs.ctbRcRowQpDeltaRange =
        extremeBias ? extremeRowQpDeltaRange : RAND(0, 31);

  u32 i = 0;
  /* ctbRc complexity direction and threshold */
  u32 compBias = RAND(0, 20) <= 10;
  u32 delta = RAND(0, 64) / RAND(4, 16);
  u32 extremeDirection = RAND(0, 1) ? 0 : 15;
  u32 extremeThreshold0 = extremeDirection ? 100 : 0;//big enough

  asic->regs.ctbRcDirection = extremeBias ? extremeDirection : RAND(0, 15);
  asic->regs.ctbRcThreshold[i] = extremeBias ? extremeThreshold0 : (compBias ? 0 : RAND(0, delta));
  for (i = 1; i < 16; i++) {
    delta = extremeBias ? RAND(0, 2) : (RAND(0, 64) / RAND(4, 16));
    asic->regs.ctbRcThreshold[i] = RAND(asic->regs.ctbRcThreshold[i - 1], MIN(asic->regs.ctbRcThreshold[i - 1] + delta, 255));
  }
  /* skin detection*/
  u32 nSkinCostomize = RAND(0, 20) <= 10;
  u32 extremeSkinQPDelta = RAND(0, 4) ? 7 : 0;
  u32 extremeSkinMinQPDelta = RAND(0, 4) ? 0 : asic->regs.ctbRcDirection;

  asic->regs.ctbRcSkinQPDelta = extremeBias ? extremeSkinQPDelta : RAND(0, 7);
  asic->regs.ctbRcSkinMinQPDelta = extremeBias ? extremeSkinMinQPDelta : RAND(0, asic->regs.ctbRcDirection);
  asic->regs.ctbRcSkinLumMin = nSkinCostomize ? 0 : RAND(1, 5);
  asic->regs.ctbRcSkinLumMax = nSkinCostomize ? 0 : RAND(MAX(asic->regs.ctbRcSkinLumMin, 200), 255);
  asic->regs.ctbRcSkinCbMin = nSkinCostomize ? 0 : RAND(1, 5);
  asic->regs.ctbRcSkinCbMax = nSkinCostomize ? 0 : RAND(MAX(asic->regs.ctbRcSkinCbMin, 200), 255);
  asic->regs.ctbRcSkinCrMin = nSkinCostomize ? 0 : RAND(1, 5);
  asic->regs.ctbRcSkinCrMax = nSkinCostomize ? 0 : RAND(MAX(asic->regs.ctbRcSkinCrMin, 200), 255);

  /* edge detection */
  asic->regs.ctbRcEdgeTh = RAND(0, 127);
  u32 extremeEdgeMaxQpDelta = RAND(0, 1) ? asic->regs.ctbRcDirection : 15;
  asic->regs.ctbRcEdgeMaxQpDelta = extremeBias ? extremeEdgeMaxQpDelta : RAND(asic->regs.ctbRcDirection, 15);
  asic->regs.bCodingCtrlUpdate = 1;
  u32 *threshold = asic->regs.ctbRcThreshold;

  printf("CtbRc1TestV2# Comp: Direction %u\n", asic->regs.ctbRcDirection);
  printf("CtbRc1TestV2# Comp: Threshold[%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u:%u]\n",
          threshold[0], threshold[1], threshold[2], threshold[3],
          threshold[4], threshold[5], threshold[6], threshold[7],
          threshold[8], threshold[9], threshold[10], threshold[11],
          threshold[12], threshold[13], threshold[14], threshold[15]);
  printf("CtbRc1TestV2# Skin: QPDelta = %u, MinQPDelta = %u, Lum(%u-%u) Cb(%u-%u) Cr(%u-%u)\n",
         asic->regs.ctbRcSkinQPDelta, asic->regs.ctbRcSkinMinQPDelta, asic->regs.ctbRcSkinLumMin, asic->regs.ctbRcSkinLumMax,
         asic->regs.ctbRcSkinCbMin, asic->regs.ctbRcSkinCbMax, asic->regs.ctbRcSkinCrMin, asic->regs.ctbRcSkinCbMax);
  printf("CtbRc1TestV2# Edge: EdgeTh = %u, MaxQpDelta = %u\n", asic->regs.ctbRcEdgeTh, asic->regs.ctbRcEdgeMaxQpDelta);

  printf("CtbRc2TestV2# Row: QpStep = %u, RowQpDeltaRange = %u\n", asic->regs.ctbRcQpStep, asic->regs.ctbRcRowQpDeltaRange);
}



void VCEncTrailDetectTest(struct vcenc_instance *inst, struct sw_picture *pic)
{
  asicData_s *asic = &inst->asic;
  if (inst->ctbRcTrailStrengthMax == 0) return;

  if (pic->sliceInst->type != I_SLICE && pic->rpl[0][0]->sliceInst->type != I_SLICE) {
    asic->regs.trailDetectEnable = 1;
    asic->regs.tmvpRefMvInfoBaseL0 = pic->rpl[0][0]->mvInfoBase;
  } else {
    asic->regs.trailDetectEnable = 0;
  }
  asic->regs.writeTMVinfoDDR = pic->sliceInst->type != I_SLICE;
  asic->regs.tmvpMvInfoBase = (ptr_t)pic->mvInfoBase;
  asic->regs.trailDeltaQp = -(i32)(inst->frameCnt % 8);
  asic->regs.trailDetectPrevMvTh = (inst->frameCnt % 32);
  asic->regs.trailDetectCurMvTh = (inst->frameCnt % 8);
  //Only detect CU size >= trailDetectMinCuLogSize, = 3 + trailDetectMinCuLogSize
  asic->regs.trailDetectMinCuLogSize = (inst->frameCnt % 1);
  asic->regs.trailSplitThCU16 = (0.1 * (inst->frameCnt % 10)) * (1 << SUBJECT_THRESH_FIX_POINT);
  asic->regs.trailSplitThCU32 = (0.1 * (inst->frameCnt % 10)) * (1 << SUBJECT_THRESH_FIX_POINT);
  //Currently only apply small lambda in trail area for H264
  asic->regs.trailUseLambdaMinQp = (inst->frameCnt % 32);
  double newQpFactor = (0.1 * (inst->frameCnt % 11));
  asic->regs.trailQpFactorSAD = (u32)((newQpFactor * (1 << QPFACTOR_FIX_POINT)) + 0.5);
  asic->regs.trailQpFactorSSE = (u32)((newQpFactor * newQpFactor * (1 << QPFACTOR_FIX_POINT)) + 0.5);
  asic->regs.trailSaoChrMaxQp = (inst->frameCnt % 52);
  asic->regs.trailSaoNumCu16Ctb = (inst->frameCnt % 16);
}

void VCEncModSubjPreferTest(struct vcenc_instance *inst)
{
  asicData_s *asic = &inst->asic;
  if (!asic->regs.asicCfg->modSubjPrefer) return;

  if (IS_H264(inst->codecFormat)) {
    asic->regs.skipBinCostMaxDistTh =
                 (0.1 * (inst->frameCnt % 10)) * (1 << SUBJECT_THRESH_FIX_POINT);
  }

  if (IS_H264(inst->codecFormat) || IS_HEVC(inst->codecFormat))
  {
    asic->regs.intraChrBincostRatio = inst->frameCnt % ((1 << SUBJECT_THRESH_FIX_POINT) + 1); /*0~64*/
    asic->regs.disableIntraDistRatioThr = inst->frameCnt %((1 << SUBJECT_THRESH_FIX_POINT)) + (1 << SUBJECT_THRESH_FIX_POINT); /*64~127*/
  }

  printf ("ModSubjPreferTest# skipBinCostMaxDistTh[%u] intraChrBincostRatio[%u] disableIntraDistRatioThr[%u]\n",
    asic->regs.skipBinCostMaxDistTh, asic->regs.intraChrBincostRatio, asic->regs.disableIntraDistRatioThr);
  return;
}

void VCEncIntraBiasTest(struct vcenc_instance *inst)
{
  asicData_s *asic = &inst->asic;
  if (!asic->regs.asicCfg->modSubjPrefer) return;

  if (IS_H264(inst->codecFormat) || IS_HEVC(inst->codecFormat))
  {
    asic->regs.iBiasCostRatioThr = inst->frameCnt % (1 << (SUBJECT_THRESH_FIX_POINT-1)) + (1 << SUBJECT_THRESH_FIX_POINT); /*64~95*/
    asic->regs.iBiasDistRatioThr1 = inst->frameCnt % ((1 << SUBJECT_THRESH_FIX_POINT)+1); /*0~64*/
    asic->regs.iBiasDistRatioThr2 = inst->frameCnt % ((1 << SUBJECT_THRESH_FIX_POINT)+1); /*0~64*/
    asic->regs.IntraBiasMvThreshold = inst->frameCnt % (64); /*0~63*/
    asic->regs.trailAvoidIntraBias = inst->frameCnt % 2; /*0,1*/
  }
  printf ("IntraBiasTest# iBiasCostRatioThr[%u] iBiasDistRatioThr1[%u] iBiasDistRatioThr2[%u] IntraBiasMvThreshold[%u], trailAvoidIntraBias[%u]\n",
    asic->regs.iBiasCostRatioThr, asic->regs.iBiasDistRatioThr1, asic->regs.iBiasDistRatioThr2, asic->regs.IntraBiasMvThreshold, asic->regs.trailAvoidIntraBias);

  return;
}

void VCEncChromaCheckTest(struct vcenc_instance *inst)
{
  asicData_s *asic = &inst->asic;
  if (!asic->regs.asicCfg->modSubjPrefer) return;

  if (IS_H264(inst->codecFormat) || IS_HEVC(inst->codecFormat))
  {
    asic->regs.iBiasChromaMseThr = inst->frameCnt % 91 + 10; /*10~100*/
  }

  printf ("ChromaCheckTest# iBiasChromaMseThr[%u]\n", asic->regs.iBiasChromaMseThr);

  return;
}

/*------------------------------------------------------------------------------
  VCEncGMVTest
------------------------------------------------------------------------------*/
void VCEncGMVTest(struct vcenc_instance *inst) {
  regValues_s *regs = &inst->asic.regs;
  i16 maxX, maxY, minX, minY;
  static i16 gmv[2][2] = {{0, 0}, {0, 0}};

  if (inst->frameCnt == 1) {
    gmv[0][0] = regs->gmv[0][0];
    gmv[0][1] = regs->gmv[0][1];
    gmv[1][0] = regs->gmv[1][0];
    gmv[1][1] = regs->gmv[1][1];
  }

  getGMVRange(&maxX, &maxY, 0, IS_H264(inst->codecFormat),
              regs->frameCodingType == 2);
  minY = -maxY;
  minX = -maxX;

  // 1: I; 0: P; 2:B
  if ((regs->frameCodingType != 1) && (inst->frameCnt > 1)) {
    gmv[0][1] += GMV_STEP_Y;
    if (gmv[0][1] > maxY) {
      gmv[0][1] = minY;
      gmv[0][0] += GMV_STEP_X;
      if (gmv[0][0] > maxX) gmv[0][0] = minX;
    }
    regs->gmv[0][0] = gmv[0][0];
    regs->gmv[0][1] = gmv[0][1];
  }

  if ((regs->frameCodingType == 2) && (inst->frameCnt > 2)) {
    gmv[1][1] += GMV_STEP_Y;
    if (gmv[1][1] > maxY) {
      gmv[1][1] = minY;
      gmv[1][0] += GMV_STEP_X;
      if (gmv[1][0] > maxX) gmv[1][0] = minX;
    }
    regs->gmv[1][0] = gmv[1][0];
    regs->gmv[1][1] = gmv[1][1];
  }

  if (regs->frameCodingType != 1)
    printf("GmvRcTest# List0 GMV = (%d %d)\n", regs->gmv[0][0],
           regs->gmv[0][1]);
  if (regs->frameCodingType == 2)
    printf("GmvRcTest# List1 GMV = (%d %d)\n", regs->gmv[1][0],
           regs->gmv[1][1]);
}

/*------------------------------------------------------------------------------
  VCEncMEVertRangeTest
------------------------------------------------------------------------------*/
void VCEncMEVertRangeTest(struct vcenc_instance *inst) {
  regValues_s *regs = &inst->asic.regs;
  if ((!regs->asicCfg->meVertRangeProgramable) || (inst->frameCnt == 0) ||
      (regs->frameCodingType == 1))
    return;

  i32 maxVertRange =
      (IS_H264(inst->codecFormat) ? regs->asicCfg->meVertSearchRangeH264
                                  : regs->asicCfg->meVertSearchRangeHEVC) *
      8;
  if (maxVertRange == 0) maxVertRange = IS_H264(inst->codecFormat) ? 24 : 40;

  i32 rangeH264[4] = {24, 48, 64, 0};
  i32 rangeHevc[4] = {40, 64, 40, 0};
  i32 rangeId = (inst->frameCnt - 1) & 3;
  i32 vRange =
      IS_H264(inst->codecFormat) ? rangeH264[rangeId] : rangeHevc[rangeId];
  if (vRange > maxVertRange) vRange = 0;

  regs->meAssignedVertRange = vRange >> 3;

  regs->bCodingCtrlUpdate = 1;
  printf("MEVertSearchRangeTest# VertRange = %u\n",
         regs->meAssignedVertRange << 3);
}

#if 0
/*------------------------------------------------------------------------------
  MbPerInterruptTest
------------------------------------------------------------------------------*/
void MbPerInterruptTest(trace_s *trace)
{
  if (trace->testId != 3)
  {
    return;
  }

  trace->control.mbPerInterrupt = trace->vopNum;
}


/*------------------------------------------------------------------------------
HevcSliceQuantTest()  Change sliceQP from min->max->min.
------------------------------------------------------------------------------*/
void HevcSliceQuantTest(trace_s *trace, slice_s *slice, mb_s *mb,
                        rateControl_s *rc)
{
  if (trace->testId != 8)
  {
    return;
  }

  rc->vopRc   = NO;
  rc->mbRc    = NO;
  rc->picSkip = NO;

  if (mb->qpPrev == rc->qpMin)
  {
    mb->qpLum = rc->qpMax;
  }
  else if (mb->qpPrev == rc->qpMax)
  {
    mb->qpLum = rc->qpMin;
  }
  else
  {
    mb->qpLum = rc->qpMax;
  }

  mb->qpCh  = qpCh[MIN(51, MAX(0, mb->qpLum + mb->chromaQpOffset))];
  rc->qp = rc->qpHdr = mb->qpLum;
  slice->sliceQpDelta = mb->qpLum - slice->picInitQpMinus26 - 26;
}

#endif

/*------------------------------------------------------------------------------
  VCEncExtLineBufferTest
------------------------------------------------------------------------------*/
void VCEncExtLineBufferTest(struct vcenc_instance *inst) {
  u32 frame = (u32)inst->frameCnt % 81;
  u32 delta = frame / 9;
  u32 i = frame % 9;
  u32 j = (i + delta) % 9;

  inst->asic.regs.sram_linecnt_lum_fwd = 2 * i;
  inst->asic.regs.sram_linecnt_lum_bwd = 2 * j;
  inst->asic.regs.sram_linecnt_chr_fwd = i;
  inst->asic.regs.sram_linecnt_chr_bwd = j;

  inst->asic.regs.bInitUpdate = 1;
  printf(
      "HevcExtLineBufferTest# "
      "lineCnt  lumFwd: %u  lumBwd: %u  chrFwd: %u chrBwd %u\n",
      inst->asic.regs.sram_linecnt_lum_fwd,
      inst->asic.regs.sram_linecnt_lum_bwd,
      inst->asic.regs.sram_linecnt_chr_fwd,
      inst->asic.regs.sram_linecnt_chr_bwd);
}

u32 VCEncReEncodeTest(struct vcenc_instance *inst, u32 testId, u32 status) {
  u32 ret = status;

  if (testId == TID_RE_ENCODE_FOR_BUFFER_FULL) {
    static u32 onlyOnce[5];
    if (((inst->frameCnt == 0) || (inst->frameCnt == 3)) && inst->pass != 1) {
      if (!onlyOnce[inst->frameCnt]) {
        onlyOnce[inst->frameCnt] = 1;
        ret = ASIC_STATUS_BUFF_FULL;
        printf(
            "VCEncReEncodeTest# output_buffer_over_flow: re-encode input frame "
            "%u\n",
            inst->frameCnt);
      }
    }
  }
  else if (testId == TID_RE_ENCODE_FOR_TIMEOUT) {
    static u32 onlyOncePass1[7];
    static u32 onlyOncePass2[6];
    if (((inst->frameCnt == 3) || (inst->frameCnt == 6)) && inst->pass == 1) {
      if (!onlyOncePass1[inst->frameCnt]) {
        onlyOncePass1[inst->frameCnt] = 1;
        ret = ASIC_STATUS_HW_TIMEOUT;
        printf(
            "VCEncReEncodeTest# timeout: re-encode this frame "
            "%u\n",
            inst->frameCnt);
      }
    }
    else if (((inst->frameCnt == 2) || (inst->frameCnt == 5)) && inst->pass == 2) {
      if (!onlyOncePass2[inst->frameCnt]) {
        onlyOncePass2[inst->frameCnt] = 1;
        ret = ASIC_STATUS_HW_TIMEOUT;
        printf(
            "VCEncReEncodeTest# timeout: re-encode this frame "
            "%u\n",
            inst->frameCnt);
      }
    }
  }
  return ret;
}

void VCEncSetReEncodeTestParameter(struct vcenc_instance *inst) {
  asicData_s *asic = &inst->asic;
  asic->regs.regValueAXIWritePending = 0x25000;
  asic->regs.regValueAXIReadPending = 0x23000;
  asic->regs.regValueAXITotalPending = 0x50000;
}

void VCEncAdjustFPSTest(VCEncIn *encIn, struct sps *s_br, bool send)
{
  if (send == true) {
    printf("Updated SPS is sent, vui_time_scale = %u, vui_num_units_in_tick = %u.\n",
          s_br->vui.timeScale, s_br->vui.numUnitsInTick);
  }
  else {
    printf("Update FPS at gopIdx = %d!\n", encIn->gopPicIdx);
  }
}

/*------------------------------------------------------------------------------
  HevcIMBlockUnitTest
------------------------------------------------------------------------------*/
void HevcIMBlockUnitTest(struct vcenc_instance *inst, int dsRatio,
                         int outRoiMapDeltaQpBlockUnit) {
  if (inst->pass == 2) {
    struct vcenc_instance *instLA =
        (struct vcenc_instance *)inst->lookahead.priv_inst;
    instLA->preProcess.inLoopDSRatio = dsRatio;
    instLA->cuTreeCtl.outRoiMapDeltaQpBlockUnit = outRoiMapDeltaQpBlockUnit;
    inst->log2_qp_size = MIN(inst->log2_qp_size, 6 - outRoiMapDeltaQpBlockUnit);

    printf("HevcIMBlockUnitTest# dsRatio %d outRoiMapDeltaQpBlockUnit %d\n",
           dsRatio, outRoiMapDeltaQpBlockUnit);
  }
}

/*------------------------------------------------------------------------------
  HevcSliceInfoPollTimeoutTest
------------------------------------------------------------------------------*/
void HevcSliceInfoPollTimeoutTest(struct vcenc_instance *inst,
                                  u32 *timeout_test_en) {
  if (inst->testId != TID_SLICEINFO_TIMEOUT) {
    *timeout_test_en = 0;
    return;
  }
  *timeout_test_en = 1;
}

/*------------------------------------------------------------------------------
  HevcUFBCDECTest
------------------------------------------------------------------------------*/
void HevcUFBCDECTest(struct vcenc_instance *inst) {
  inst->preProcess.lumWidthSrc[0] = inst->preProcess.lumWidthSrc[0] + 320;
}

/*------------------------------------------------------------------------------
  HevcUFBCCFGTest
------------------------------------------------------------------------------*/
void HevcUFBCCFGTest(struct vcenc_instance *inst) {
#ifdef SUPPORT_UFBC
  inst->asic.ufbc->regParam.afbc.yuvTrans = 1;
#endif
}

#endif
