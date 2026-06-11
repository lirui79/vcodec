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
--------------------------------------------------------------------------------
*/
#ifdef RATE_CONTROL_BUILD_SUPPORT

#include "vsi_string.h"
#include "encasiccontroller.h"
#include "rate_control_jpeg.h"
#include "tools.h"
#include <math.h>
#include "enccfg.h"
#include "enc_log.h"

/*
 * Using only one leaky bucket (Multible buckets is supported by std).
 * Constant bit rate (CBR) operation, ie. leaky bucket drain rate equals
 * average rate of the stream, is enabled if RC_CBR_HRD = 1. Frame skiping and
 * filler data are minimum requirements for the CBR conformance.
 *
 * Constant HRD parameters:
 *   low_delay_hrd_flag = 0, assumes constant delay mode.
 *       cpb_cnt_minus1 = 0, only one leaky bucket used.
 *         (cbr_flag[0] = RC_CBR_HRD, CBR mode.)
 */

/*------------------------------------------------------------------------------
  Module defines
------------------------------------------------------------------------------*/

/* Define this if strict bitrate control is needed, each window has a strict
 * bit budget. Otherwise a moving window is used for smoother quality.
*/

#define INITIAL_BUFFER_FULLNESS 80 /* Decoder Buffer in procents */
#define MIN_PIC_SIZE 50            /* Procents from picPerPic */

#define DIV(a, b) (((b) != 0) ? ((a) + (SIGN(a) * (b)) / 2) / (b) : (a))
#define DIV2(a, b) \
  (((a) + (SIGN(a) * (b)) / 2) / (b)) /* when b is non-zero value */
#define DSCY 32                       /* n * 32 */
#define DSCBITPERMB 128               //128 /* bitPerMb scaler  */
#define I32_MIN (i32)(1u << 31)
#define I64_MAX 9223372036854775807 /* 2 ^ 63 - 1 */
//#define QP_DELTA          2.0     /* Limit QP change between consecutive frames.*/
#define QP_DELTA 0x200 /* Limit QP change between consecutive frames.*/

//#define QP_DELTA_LIMIT    6.0     /* Threshold when above limit is abandoned. */
#define QP_DELTA_LIMIT 0x600 /* Threshold when above limit is abandoned. */

#define QP_FIXPOINT_0_POINT_1 0x1a

#define INTRA_QP_DELTA (0)
#define WORD_CNT_MAX 65535

#define MAX_QP 51

/*------------------------------------------------------------------------------
  Local structures
------------------------------------------------------------------------------*/

static const i32 q_step[511] = {

    0x279,   0x280,   0x288,   0x28f,   0x297,   0x29f,   0x2a7,   0x2af,
    0x2b6,   0x2bf,   0x2c7,   0x2cf,   0x2d7,   0x2e0,   0x2e8,   0x2f1,
    0x2fa,   0x303,   0x30c,   0x315,   0x31e,   0x327,   0x330,   0x33a,
    0x344,   0x34d,   0x357,   0x361,   0x36b,   0x375,   0x380,   0x38a,
    0x395,   0x39f,   0x3aa,   0x3b5,   0x3c0,   0x3cb,   0x3d6,   0x3e2,
    0x3ed,   0x3f9,   0x405,   0x411,   0x41d,   0x429,   0x436,   0x442,
    0x44f,   0x45c,   0x469,   0x476,   0x483,   0x490,   0x49e,   0x4ac,
    0x4ba,   0x4c8,   0x4d6,   0x4e4,   0x4f3,   0x501,   0x510,   0x51f,
    0x52f,   0x53e,   0x54e,   0x55e,   0x56d,   0x57e,   0x58e,   0x59e,
    0x5af,   0x5c0,   0x5d1,   0x5e3,   0x5f4,   0x606,   0x618,   0x62a,
    0x63c,   0x64f,   0x661,   0x674,   0x688,   0x69b,   0x6af,   0x6c3,
    0x6d7,   0x6eb,   0x700,   0x715,   0x72a,   0x73f,   0x754,   0x76a,
    0x780,   0x797,   0x7ad,   0x7c4,   0x7db,   0x7f3,   0x80a,   0x822,
    0x83a,   0x853,   0x86c,   0x885,   0x89e,   0x8b8,   0x8d2,   0x8ec,
    0x906,   0x921,   0x93c,   0x958,   0x974,   0x990,   0x9ac,   0x9c9,
    0x9e6,   0xa03,   0xa21,   0xa3f,   0xa5e,   0xa7d,   0xa9c,   0xabc,
    0xadb,   0xafc,   0xb1c,   0xb3d,   0xb5f,   0xb81,   0xba3,   0xbc6,
    0xbe9,   0xc0c,   0xc30,   0xc54,   0xc79,   0xc9e,   0xcc3,   0xce9,
    0xd10,   0xd37,   0xd5e,   0xd86,   0xdae,   0xdd7,   0xe00,   0xe2a,
    0xe54,   0xe7e,   0xea9,   0xed5,   0xf01,   0xf2e,   0xf5b,   0xf89,
    0xfb7,   0xfe6,   0x1015,  0x1045,  0x1075,  0x10a6,  0x10d8,  0x110a,
    0x113c,  0x1170,  0x11a4,  0x11d8,  0x120d,  0x1243,  0x1279,  0x12b0,
    0x12e8,  0x1320,  0x1359,  0x1392,  0x13cd,  0x1407,  0x1443,  0x147f,
    0x14bc,  0x14fa,  0x1538,  0x1578,  0x15b7,  0x15f8,  0x1639,  0x167b,
    0x16be,  0x1702,  0x1746,  0x178c,  0x17d2,  0x1819,  0x1860,  0x18a9,
    0x18f2,  0x193c,  0x1987,  0x19d3,  0x1a20,  0x1a6e,  0x1abd,  0x1b0c,
    0x1b5d,  0x1bae,  0x1c00,  0x1c54,  0x1ca8,  0x1cfd,  0x1d53,  0x1dab,
    0x1e03,  0x1e5c,  0x1eb6,  0x1f12,  0x1f6e,  0x1fcc,  0x202a,  0x208a,
    0x20eb,  0x214d,  0x21b0,  0x2214,  0x2279,  0x22e0,  0x2348,  0x23b1,
    0x241b,  0x2486,  0x24f3,  0x2561,  0x25d0,  0x2640,  0x26b2,  0x2725,
    0x279a,  0x280f,  0x2887,  0x28ff,  0x2979,  0x29f5,  0x2a71,  0x2af0,
    0x2b6f,  0x2bf0,  0x2c73,  0x2cf7,  0x2d7d,  0x2e05,  0x2e8d,  0x2f18,
    0x2fa4,  0x3032,  0x30c1,  0x3152,  0x31e5,  0x3279,  0x330f,  0x33a7,
    0x3441,  0x34dc,  0x357a,  0x3619,  0x36ba,  0x375c,  0x3801,  0x38a8,
    0x3950,  0x39fb,  0x3aa7,  0x3b56,  0x3c06,  0x3cb9,  0x3d6d,  0x3e24,
    0x3edd,  0x3f98,  0x4055,  0x4114,  0x41d6,  0x429a,  0x4360,  0x4428,
    0x44f3,  0x45c0,  0x4690,  0x4762,  0x4836,  0x490d,  0x49e6,  0x4ac2,
    0x4ba0,  0x4c81,  0x4d65,  0x4e4b,  0x4f34,  0x501f,  0x510e,  0x51ff,
    0x52f3,  0x53ea,  0x54e3,  0x55e0,  0x56df,  0x57e1,  0x58e7,  0x59ef,
    0x5afb,  0x5c0a,  0x5d1b,  0x5e30,  0x5f48,  0x6064,  0x6183,  0x62a5,
    0x63ca,  0x64f3,  0x661f,  0x674f,  0x6882,  0x69b9,  0x6af4,  0x6c32,
    0x6d74,  0x6eb9,  0x7003,  0x7150,  0x72a1,  0x73f6,  0x754f,  0x76ac,
    0x780d,  0x7972,  0x7adb,  0x7c48,  0x7dba,  0x7f30,  0x80aa,  0x8229,
    0x83ac,  0x8534,  0x86c0,  0x8851,  0x89e7,  0x8b81,  0x8d20,  0x8ec4,
    0x906c,  0x921a,  0x93cc,  0x9584,  0x9741,  0x9903,  0x9aca,  0x9c96,
    0x9e68,  0xa03f,  0xa21c,  0xa3fe,  0xa5e6,  0xa7d4,  0xa9c7,  0xabc0,
    0xadbf,  0xafc3,  0xb1ce,  0xb3df,  0xb5f6,  0xb814,  0xba37,  0xbc61,
    0xbe91,  0xc0c8,  0xc306,  0xc54a,  0xc795,  0xc9e6,  0xcc3f,  0xce9e,
    0xd105,  0xd373,  0xd5e8,  0xd864,  0xdae8,  0xdd73,  0xe006,  0xe2a0,
    0xe542,  0xe7ec,  0xea9e,  0xed58,  0xf01a,  0xf2e4,  0xf5b7,  0xf891,
    0xfb75,  0xfe61,  0x10155, 0x10453, 0x10759, 0x10a69, 0x10d81, 0x110a3,
    0x113ce, 0x11702, 0x11a40, 0x11d88, 0x120d9, 0x12434, 0x12799, 0x12b09,
    0x12e82, 0x13206, 0x13594, 0x1392d, 0x13cd1, 0x1407f, 0x14439, 0x147fd,
    0x14bcd, 0x14fa8, 0x1538e, 0x15780, 0x15b7e, 0x15f87, 0x1639d, 0x167bf,
    0x16bed, 0x17028, 0x1746f, 0x178c3, 0x17d23, 0x18191, 0x1860c, 0x18a94,
    0x18f2a, 0x193cd, 0x1987e, 0x19d3d, 0x1a20b, 0x1a6e6, 0x1abd0, 0x1b0c9,
    0x1b5d0, 0x1bae6, 0x1c00c, 0x1c541, 0x1ca85, 0x1cfd9, 0x1d53c, 0x1dab0,
    0x1e034, 0x1e5c9, 0x1eb6e, 0x1f123, 0x1f6ea, 0x1fcc2, 0x202ab, 0x208a6,
    0x20eb3, 0x214d2, 0x21b03, 0x22146, 0x2279c, 0x22e05, 0x23481, 0x23b10,
    0x241b3, 0x24869, 0x24f33, 0x25612, 0x25d05, 0x2640d, 0x26b29, 0x2725b,
    0x279a2, 0x280ff, 0x28872, 0x28ffb, 0x2979a, 0x29f50, 0x2a71d, 0x2af01,
    0x2b6fc, 0x2bf0f, 0x2c73b, 0x2cf7e, 0x2d7db, 0x2e050, 0x2e8de, 0x2f186,
    0x2fa47, 0x30322, 0x30c18, 0x31529, 0x31e54, 0x3279b, 0x330fd, 0x33a7b,
    0x34416, 0x34dcd, 0x357a1, 0x36192, 0x36ba0, 0x375cd, 0x38018};

/*------------------------------------------------------------------------------
  Local function prototypes
------------------------------------------------------------------------------*/

static i32 InitialQp(i32 bits, i32 pels);
static void PicQuantLimit(jpegEncRateControl_s *rc);
static i32 VirtualBuffer(jpegEncRateControl_s *rc, i32 timeInc, true_e hrd);
static void PicQuant(jpegEncRateControl_s *rc);
static i32 avg_rc_error_jpeg(jpegEncRateControl_s *rc, jpegLinReg_s *p);
static void update_rc_error(jpegLinReg_s *p, i32 bits, i32 windowLength);
static i32 new_pic_quant(jpegLinReg_s *p, jpegEncRateControl_s *rc, i32 bits,
                         true_e useQpDeltaLimit, u32 complexity,
                         i32 lastActualBits, i32 lastTargetBits,
                         i32 currentTargetBits);
static void update_tables(jpegLinReg_s *p, i32 qp, i32 bits);
static void update_model(jpegLinReg_s *p);
static i64 lin_sy(i32 *qp, i32 *r, i32 n);
static i64 lin_sx(i32 *qp, i32 n);
static i64 lin_sxy(i32 *qp, i32 *r, i32 n);
static i64 lin_nsxx(i32 *qp, i32 n);

static i32 RcGetTargetPicSize(jpegEncRateControl_s *rc, i32 accuDiff,
                              i32 intraBits, i32 rcWindow) {
  jpegRcVirtualBuffer_s *vb = &rc->virtualBuffer;

  if (rc->rcMode == JPEGENC_SINGLEFRAME) {
     rc->targetPicSize = vb->bitPerPic;
  } else {
    rc->targetPicSize = vb->bitPerPic - intraBits + DIV(accuDiff, rcWindow);
  }

  return accuDiff;
}

/*------------------------------------------------------------------------------

  VCEncInitRc() Initialize rate control.

------------------------------------------------------------------------------*/
bool_e JpegEncInitRc(jpegEncRateControl_s *rc, u32 newStream) {
  jpegRcVirtualBuffer_s *vb = &rc->virtualBuffer;
  i32 i;

#if defined(TRACE_RC) && (DBGOUTPUT == fpRcTrc)
  if (!fpRcTrc) fpRcTrc = fopen("rc.trc", "wt");
#endif

  if ((rc->qpMax > (51 << JPEG_QP_FRACTIONAL_BITS))) {
    return ENCHW_NOK;
  }
  //bitPerPic
  rc->bpp =
      (((i64)jpegRcCalculate(vb->bitRate, rc->outRateDenom, rc->outRateNum)) *
           100 +
       rc->picArea / 2) /
      rc->picArea;
  rc->vbrOn = 0;
  rc->picSkip = 1;

  /* QP -1: Initial QP estimation done by RC */
  if (rc->qpHdr == (-1 << JPEG_QP_FRACTIONAL_BITS)) {
    i32 tmp = jpegRcCalculate(vb->bitRate, rc->outRateDenom, rc->outRateNum);
    rc->qpHdr =
        rc->picRc
            ? InitialQp(tmp,
                        rc->picArea /*rc->ctbPerPic*rc->ctbSize*rc->ctbSize*/)
            : (JPEG_QP_DEFAULT << JPEG_QP_FRACTIONAL_BITS);
    PicQuantLimit(rc);
    rc->finiteQp = rc->qpHdr;
    if ((rc->finiteQp) >= (18 << JPEG_QP_FRACTIONAL_BITS)) {
      rc->finiteQp -= (18 << JPEG_QP_FRACTIONAL_BITS);
    } else {
      rc->finiteQp = 0;
    }
  } else {
    i32 tmp = jpegRcCalculate(vb->bitRate, rc->outRateDenom, rc->outRateNum);
    rc->finiteQp = InitialQp(tmp, rc->picArea);
    if ((rc->finiteQp) >= (18 << JPEG_QP_FRACTIONAL_BITS)) {
      rc->finiteQp -= (18 << JPEG_QP_FRACTIONAL_BITS);
    } else {
      rc->finiteQp = 0;
    }
  }

  if ((rc->qpHdr > rc->qpMax) || (rc->qpHdr < rc->qpMin)) {
    return ENCHW_NOK;
  }

  /* HRD needs frame RC and macroblock RC*/
  if (rc->hrd == ENCHW_YES) {
    rc->picRc = ENCHW_YES;
  }

  rc->frameCoded = ENCHW_YES;
  rc->sliceTypeCur = I_SLICE;
  rc->sliceTypePrev = P_SLICE;

  rc->qpHdrPrev = rc->qpHdr;
  rc->fixedQp = rc->qpHdr;
  rc->qpISlice = rc->qpHdr;

  vb->bitPerPic =
      jpegRcCalculate(vb->bitRate, rc->outRateDenom, rc->outRateNum);

  /* API parameter is named bitrateWindow but the actual usage is rate controlling
   * window in frames. RC tries to match the target bitrate inside the
   * window. Each window can contain multiple GOPs and the RC adapts to the
   * intra rate by calculating intraInterval. */

  RC_LOG_I("\nInitRc: picRc\t\t%i  hrd\t%i  picSkip\t%i\n", rc->picRc, rc->hrd,
           rc->picSkip);
  RC_LOG_I("  CPBsize\t%i\n BitRate\t%i\n BitPerPic\t%i\n", vb->bufferSize,
           vb->bitRate, vb->bitPerPic);

  /* If changing QP between frames don't reset GOP RC.
   * Changing bitrate resets RC the same way as new stream. */
  if (!newStream) return ENCHW_OK;

  //init error for I
  update_rc_error(&rc->intraError, 0x7fffffff, 0);
  //init model for I
  EWLmemset(&rc->intra, 0, sizeof(jpegLinReg_s));
  rc->intra.qs[0] = q_step[510];
  rc->intra.weight = 10;
  rc->intra.qp_prev = rc->qpHdr;
  rc->intra.frameBitCntLast = 0;
  rc->intra.targetPicSizeLast = 0;

  /* API parameter is named bitrateWindow but the actual usage is rate controlling
   * window in frames. RC tries to match the target bitrate inside the
   * window. Each window can contain multiple GOPs and the RC adapts to the
   * intra rate by calculating intraInterval. */
  rc->windowLen = rc->bitrateWindow;
  vb->windowRem = rc->bitrateWindow;
  rc->gopMulti = rc->bitrateWindow * rc->outRateDenom / rc->outRateNum;
  if (rc->gopMulti < 1) rc->gopMulti = 1;
  rc->targetPicSize = 0;
  rc->gopHeadTargetPicSize = 0;
  rc->targetGopSize = 0;
  rc->actualGopSize = 0;
  rc->encodedGopFrames = 0;
  rc->frameBitCnt = 0;
  vb->picTimeInc = 0;
  vb->realBitCnt = 0;
  vb->virtualBitCnt = 0;

  rc->frameCnt = 0;

  //rc->virtualBuffer.movingMinRate = jpegRcCalculate(rc->virtualBuffer.bitRate, 100, 100 + rc->f_tolMovingBitRate);
  if (vb->bufferSize) {
    vb->bucketFullness =
        jpegRcCalculate(vb->bufferSize, INITIAL_BUFFER_FULLNESS, 100);
    vb->bucketFullness = vb->bucketLevel = vb->bufferSize - vb->bucketFullness;
  }

  rc->inputSceneChange = 0;

  return ENCHW_OK;
}

/*------------------------------------------------------------------------------

  InitialQp()  Returns sequence initial quantization parameter.

------------------------------------------------------------------------------*/
#define OFFSET_QP 0
static i32 InitialQp(i32 bits, i32 pels) {
  const i32 qp_tbl[2][36] = {
      /*{27, 32, 36, 40, 44, 51, 58, 65, 72, 84, 96, 108, 119, 138, 156, 174, 192, 223, 253, 285, 314, 349, 384, 419, 453, 503, 553, 603, 653, 719, 784, 864, 0x7FFFFFFF},*/
      {16,  19,  23,  27,  32,  36,  40,  44,  51,  58,  65,  72,
       84,  96,  108, 119, 138, 156, 174, 192, 223, 253, 285, 314,
       349, 384, 419, 453, 503, 553, 603, 653, 719, 784, 864, 0x7FFFFFFF},
      /*{26, 38, 59, 96, 173, 305, 545, 0x7FFFFFFF},*/
      {51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34,
       33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16}};
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
  bits64 = jpegRcCalculate((i32)bits64, upscale, pels << 6);

  while ((qp_tbl[0][++i] - OFFSET_QP) < bits64) (void)1;

  RC_LOG_I("BPP\t\t%ld\n", bits64);

  return qp_tbl[1][i] << JPEG_QP_FRACTIONAL_BITS;
}
/*------------------------------------------------------------------------------
  VirtualBuffer()  Return difference of target and real buffer fullness.
  Virtual buffer and real bit count grow until one second.  After one second
  output bit rate per second is removed from virtualBitCnt and realBitCnt. Bit
  drifting has been taken care.

  If the leaky bucket in VBR mode becomes empty (e.g. underflow), those R * T_e
  bits are lost and must be decremented from virtualBitCnt. (NOTE: Drift
  calculation will mess virtualBitCnt up, so the loss is added to realBitCnt)
------------------------------------------------------------------------------*/
static i32 VirtualBuffer(jpegEncRateControl_s *rc, i32 timeInc, true_e hrd) {
  jpegRcVirtualBuffer_s *vb = &rc->virtualBuffer;
  i32 target;

  vb->picTimeInc += timeInc;

  /* picTimeInc must be in range of [0, timeScale) */
  while (vb->picTimeInc >= vb->timeScale) {
    vb->picTimeInc -= vb->timeScale;
    if (vb->realBitCnt < (I32_MIN + vb->bitRate))
      vb->realBitCnt = I32_MIN;
    else
      vb->realBitCnt -= vb->bitRate;

    if (vb->bucketLevel < (I32_MIN + vb->bitRate))
      vb->bucketLevel = I32_MIN;
    else
      vb->bucketLevel -= vb->bitRate;

    vb->seconds++;
    vb->averageBitRate = vb->bitRate + vb->realBitCnt / vb->seconds;
  }
  vb->virtualBitCnt = jpegRcCalculate(vb->bitRate, vb->picTimeInc,
                                      vb->timeScale);

  if (vb->bufferSize) {
    if (vb->bucketLevel >= vb->virtualBitCnt) {
      vb->bucketFullness = vb->bucketLevel - vb->virtualBitCnt;
    } else {
      vb->bucketFullness = 0;
      vb->realBitCnt += vb->virtualBitCnt - vb->bucketLevel;
      vb->bucketLevel = vb->virtualBitCnt;
    }
  }

  /* Saturate realBitCnt, this is to prevent overflows caused by much greater
     bitrate setting than is really possible to reach */
  if (vb->realBitCnt > 0x1FFFFFFF) vb->realBitCnt = 0x1FFFFFFF;
  if (vb->realBitCnt < -0x1FFFFFFF) vb->realBitCnt = -0x1FFFFFFF;

  target = vb->virtualBitCnt - vb->realBitCnt;
  // keep bits buffering space at least bitSpaceThrd
  i32 bitSpaceThrd = vb->bitRate * 3 / 4;
  rc->vbrOn = target >= bitSpaceThrd;

  //printf("target=%d,vb->realBitCnt=%d\n",target,vb->realBitCnt);
  /* Saturate target, prevents rc going totally out of control.
     This situation should never happen. */
  if (target > 0x1FFFFFFF) target = 0x1FFFFFFF;
  if (target < -0x1FFFFFFF) target = -0x1FFFFFFF;

  RC_LOG_I("virtualBitCnt:\t%d  realBitCnt:\t%d  bitPerPic:\t%d \n",
           vb->virtualBitCnt, vb->realBitCnt, vb->bitPerPic);
  RC_LOG_I("  diff bits:\t%d  avg bitrate:\t%d\n", target, vb->averageBitRate);
  return target;
}

static i32 rcModelErrorEst(jpegLinReg_s *p, jpegEncRateControl_s *rc) {
  i64 modelBits = rc->targetPicSize;
  i32 qp = rc->qpHdrPrev;
  i32 err = rc->frameBitCnt - (i32)modelBits;

  if (rc->codingType == ASIC_JPEG || rc->codingType == ASIC_MJPEG) return err;

  if (p->a1 == 0 && p->a2 == 0) return err;

  modelBits = DIV(p->a1, q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)]);
  modelBits += DIV(p->a2, (i64)q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)] *
                              q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)]);
  modelBits >>= JPEG_QP_FRACTIONAL_BITS;
  modelBits =
      MIN((modelBits * (rc->ctbPerPic * rc->ctbSize * rc->ctbSize / 16 / 16) /
           DSCBITPERMB),
          rc->maxPicSizeI);

  err = rc->frameBitCnt - (i32)modelBits;
  return err;
}

/*------------------------------------------------------------------------------
  VCEncAfterPicRc()  Update source model, bit rate target error and linear
  regression model for frame QP calculation. If HRD enabled, check leaky bucket
  status and return RC_OVERFLOW if coded frame must be skipped. Otherwise
  returns number of required filler payload bytes.
------------------------------------------------------------------------------*/
i32 JpegEncAfterPicRc(jpegEncRateControl_s *rc, u32 nonZeroCnt, u32 byteCnt,
                      u32 qpSum, u32 qpNum) {
  jpegRcVirtualBuffer_s *vb = &rc->virtualBuffer;
  i32 bitPerPic = rc->virtualBuffer.bitPerPic;
  i32 tmp, stat, bitCnt = (i32)byteCnt * 8, normBits;
  i32 imin, imax, i;
  i32 algNum = 0;
  jpegLinReg_s *rcModel = NULL, *rcError = NULL;
  i32 bitCntError;
  i32 prevFrameBitCnt = rc->frameBitCnt;
  rc->qpSum = (i32)qpSum;
  rc->qpNum = (i32)qpNum;
  rc->frameBitCnt = bitCnt;
  rc->frameCnt++;

  rc->actualGopSize += bitCnt;
  rc->targetGopSize += rc->targetPicSize;
  rc->encodedGopFrames++;

  normBits = jpegRcCalculate(
      bitCnt, DSCBITPERMB, rc->ctbPerPic * rc->ctbSize * rc->ctbSize / 16 / 16);
  algNum = 0;

  if (rc->targetPicSize) {
    tmp = ((bitCnt - rc->targetPicSize) * 100) / rc->targetPicSize;
  } else {
    tmp = 0;
  }
  RC_LOG_I("\nAFTER PIC RC:\n");
  RC_LOG_I(
      "RC QP(qpHdr %2.2f) BITS %d  BitErr/target\t%7i%%  "
      "BitErr/avg\t%7d%%\n",
      rc->qpHdr / 256.0, bitCnt, tmp,
      ((bitCnt - bitPerPic) * 100) / (bitPerPic + 1));

  if ((ABS(tmp) > ABS(rc->tolMovingBitRate - 100)) &&
        (rc->targetPicSize != 0)) {
    rcModel = &rc->intra;
    rcError = &rc->intraError;

    /* Store the error between target and actual frame size in percentage.
     * Saturate the error to avoid inter frames with mostly intra MBs
     * to affect too much. */
    bitCntError = rcModelErrorEst(rcModel, rc);
    update_rc_error(rcError, bitCntError, rc->windowLen);

    /* Update number of bits used for residual, inter or intra */
    update_tables(rcModel, rc->qpHdrPrev, normBits);
    update_model(rcModel);
  }

  /* clean scene change related flags */
  rc->inputSceneChange = 0;

  rc->sliceTypePrev = rc->sliceTypeCur;

  return 0;
}

/*------------------------------------------------------------------------------
  VCEncBeforePicRc()  Update virtual buffer, and calculate picInitQp for current
  picture , and coded status.
------------------------------------------------------------------------------*/
void JpegEncBeforePicRc(jpegEncRateControl_s *rc, u32 timeInc, u32 sliceType,
                        void *pic) {
  jpegRcVirtualBuffer_s *vb = &rc->virtualBuffer;
  i32 i, rcWindow = 1, intraBits = 0, tmp = 0;
  bool bTargetBitsExtremSmall = HANTRO_FALSE;

  rc->frameCoded = ENCHW_YES;
  rc->sliceTypeCur = sliceType;

  RC_LOG_I("\nBEFORE PIC RC: pic=%d\n", rc->frameCnt);
  RC_LOG_I("Frame type:\t%8i  timeInc:\t%8i\n", sliceType, timeInc);
  tmp = VirtualBuffer(rc, (i32)timeInc, rc->hrd);
  if (vb->windowRem == 0) {
    vb->windowRem = rc->windowLen - 1;
    /* New bitrate window, reset error counters. */
    //update_rc_error(&rc->rError_BFrame, 0x7fffffff, 18/*rc->windowLen*/);
    /* Don't reset intra error in case of intra-only, it would cause step. */
    if (rc->sliceTypeCur != rc->sliceTypePrev)
      update_rc_error(&rc->intraError, 0x7fffffff, rc->windowLen);

  } else {
    vb->windowRem--;
  }

#ifdef RC_WINDOW_STRICT
  /* In the end of window don't be too strict with matching the error
   * otherwise the end of window tends to twist QP. */
  rcWindow = MAX(MAX(3, rc->windowLen / 6), vb->windowRem);
  //printf("\n rcWindow=%d\n",rcWindow);
#else
  /* Actually we can be fairly easy with this one, let's make it
   * a moving window to smoothen the changes. */
  rcWindow = MAX(1, rc->windowLen);
#endif

  tmp = RcGetTargetPicSize(rc, tmp, intraBits, rcWindow);

  /* Limit the target to a realistic minimum that can be reached.
   * Setting target lower than this will confuse RC because it can never
   * be reached. Frame with only skipped mbs == 96 bits. */// carl 96 should be changed to what number
  bTargetBitsExtremSmall =
      ((96 + rc->ctbRows * rc->ctbSize) > rc->targetPicSize) ? HANTRO_TRUE
                                                             : HANTRO_FALSE;
  rc->targetPicSize = MAX(96 + rc->ctbRows * rc->ctbSize, rc->targetPicSize);

  RC_LOG_I("WndRem: %4i  \n", vb->windowRem);

  /* determine initial quantization parameter for current picture */
  PicQuant(rc);
  if (bTargetBitsExtremSmall && (rc->qpHdr < rc->qpHdrPrev) &&
      (rc->sliceTypeCur != I_SLICE) && 1) {
    rc->qpHdr = MAX(rc->qpHdrPrevGop, rc->qpHdr);
    RC_LOG_I("ExtremSmallReset QP %d %2.2f \n", rc->qpHdr, rc->qpHdr / 256.0);
  }

  /* quantization parameter user defined limitations */
  PicQuantLimit(rc);

  if (rc->sliceTypeCur == I_SLICE) {
    if (rc->fixedIntraQp) rc->qpHdr = rc->fixedIntraQp;

  }

  /* quantization parameter user defined limitations */
  PicQuantLimit(rc);

  /* Store the start QP, before ROI adjustment */
  rc->qpHdrPrev = rc->qpHdr;

  /* reset counters */
  rc->qpSum = 0;
  rc->qpLastCoded = rc->qpHdr;
  rc->qpTarget = rc->qpHdr;

  RC_LOG_I("Frame coded\t%8d  \n", rc->frameCoded);
  RC_LOG_I("Frame qpHdr\t%8d %2.2f  \n", rc->qpHdr, rc->qpHdr / 256.0);
  RC_LOG_I("GopRem:\t%8d  \n", vb->windowRem);
  RC_LOG_I("Target bits:\t%8d  \n", rc->targetPicSize);
  RC_LOG_I("\nintraBits:\t%8d  \n", intraBits);
  RC_LOG_I("bufferComp:\t%8d  \n", DIV(tmp, rcWindow));
}

/*------------------------------------------------------------------------------
  PicQuant()  Calculate quantization parameter for next frame. In the beginning
                of window use previous GOP average QP and otherwise find new QP
                using the target size and previous frames QPs and bit counts.
------------------------------------------------------------------------------*/
void PicQuant(jpegEncRateControl_s *rc) {
  i32 normBits, targetBits;
  true_e useQpDeltaLimit = ENCHW_YES;
  i32 algNum = 0;

  if (rc->picRc != ENCHW_YES) {
    rc->qpHdr = rc->fixedQp;
    RC_LOG_I("R/cx:  xxxx  QP: xx xx D:  xxxx newQP: xx\n");
    return;
  }

  /* determine initial quantization parameter for current picture */
  if (rc->sliceTypeCur == I_SLICE) {
    /* If all frames are intra we calculate new QP for intra the same way as for inter */
    if (rc->sliceTypePrev == I_SLICE) {
      useQpDeltaLimit = ENCHW_NO;
      if (rc->rcMode == JPEGENC_SINGLEFRAME) {
        targetBits = rc->targetPicSize;
      } else {
        rc->errBits = avg_rc_error_jpeg(rc, &rc->intraError);
        targetBits = rc->targetPicSize - rc->errBits;
      }
      targetBits = CLIP3(rc->minPicSizeI, 2 * rc->targetPicSize, targetBits);
      normBits =
          jpegRcCalculate(targetBits, DSCBITPERMB,
                          rc->ctbPerPic * rc->ctbSize * rc->ctbSize / 16 / 16);
      rc->qpISlice = rc->qpHdr =
          new_pic_quant(&rc->intra, rc, normBits, useQpDeltaLimit, 0, 0, 0, 0);
      RC_LOG_I("rc_qp (%d, %d, 0x%x) rc_er %d %d <-> %d %d %d  \n", targetBits,
               rc->qpHdr >> JPEG_QP_FRACTIONAL_BITS, rc->qpHdr,
               rc->targetPicSize, avg_rc_error_jpeg(rc, &rc->intraError),
               rc->intraError.bits[2], rc->intraError.bits[1],
               rc->intraError.bits[0]);
    }
  }
}

/*------------------------------------------------------------------------------

  PicQuantLimit()

------------------------------------------------------------------------------*/
void PicQuantLimit(jpegEncRateControl_s *rc) {
  rc->qpHdr = MIN(rc->qpMax, MAX(rc->qpMin, rc->qpHdr));
}

/*------------------------------------------------------------------------------

  Calculate()  I try to avoid overflow and calculate good enough result of a*b/c

------------------------------------------------------------------------------*/
i32 jpegRcCalculate(i32 a, i32 b, i32 c) {
  u32 left = 32;
  u32 right = 0;
  u32 shift;
  i32 sign = 1;
  i32 tmp;

  if (a == 0 || b == 0) {
    return 0;
  }
  //else if ((a * b / b) == a && c != 0)
  else if ((i64)a * (i64)b < (1LL << 31) && c != 0) {
    return (a * b / c);
  }
  if (a < 0) {
    sign = -1;
    a = -a;
  }
  if (b < 0) {
    sign *= -1;
    b = -b;
  }
  if (c < 0) {
    sign *= -1;
    c = -c;
  }

  if (c == 0) {
    return 0x7FFFFFFF * sign;
  }

  if (b > a) {
    tmp = b;
    b = a;
    a = tmp;
  }

  for (--left; (((u32)a << left) >> left) != (u32)a; --left)
    ;
  left--; /* unsigned values have one more bit on left,
               we want signed accuracy. shifting signed values gives
               lint warnings */

  while (((u32)b >> right) > (u32)c) {
    right++;
  }

  if (right > left) {
    return 0x7FFFFFFF * sign;
  } else {
    shift = left - right;
    return (i32)((((u32)a << shift) / (u32)c * (u32)b) >> shift) * sign;
  }
}
#if 1

/*------------------------------------------------------------------------------
  update_overhead()  Update PI(D)-control values
------------------------------------------------------------------------------*/
static void update_rc_error(jpegLinReg_s *p, i32 bits, i32 windowLen) {
  p->len = 3;

  if (bits == (i32)0x7fffffff) {
    /* RESET */
    p->bits[0] = 0;
    if (windowLen) /* Store the average error of previous GOP. */
      p->bits[1] = p->bits[1] / windowLen;
    else
      p->bits[1] = 0;
    p->bits[2] = 0;
    return;
  }
  p->bits[0] = bits - p->bits[2]; /* Derivative */
  if (bits > 0) //((bits > 0) && (bits + p->bits[1] > p->bits[1]))
    p->bits[1] = bits + p->bits[1]; /* Integral */
  if (bits < 0) //((bits < 0) && (bits + p->bits[1] < p->bits[1]))
    p->bits[1] = bits + p->bits[1]; /* Integral */
  p->bits[2] = bits;                /* Proportional */
  RC_LOG_I("P %6d I %7d D %7d\n", p->bits[2], p->bits[1], p->bits[0]);
}
#else
/*------------------------------------------------------------------------------
  avg_rc_error()  PI(D)-control for rate prediction error.
------------------------------------------------------------------------------*/
static i32 avg_rc_error(jpegLinReg_s *p) {
  /* Avoid overflow */
  i32 i;
  i32 biterror = 0;
  for (i = 0; i < p->len; i++) {
    biterror += p->bits[i];
  }
  return biterror / p->len;
}

/*------------------------------------------------------------------------------
  update_overhead()  Update PI(D)-control values
------------------------------------------------------------------------------*/
static void update_rc_error(jpegLinReg_s *p, i32 bits, i32 windowLen) {
  p->len = 4;

  if (bits == (i32)0x7fffffff) {
    /* RESET */
    p->bits[0] = 0;
    p->bits[1] = 0;
    p->bits[2] = 0;
    p->bits[3] = 0;
    p->bits[4] = 0;
    p->pos = 0;
    return;
  }
  p->bits[p->pos++] = bits;
  if (p->pos == p->len) p->pos = 0;
}

#endif

/*------------------------------------------------------------------------------
  avg_rc_error_jpeg()  PI(D)-control for rate prediction error.
------------------------------------------------------------------------------*/
static i32 avg_rc_error_jpeg(jpegEncRateControl_s *rc, jpegLinReg_s *p) {
  i32 error_bits;

  if (ABS(p->bits[2]) < 0xFFFFFFF && ABS(p->bits[1]) < 0xFFFFFFF) {
    error_bits = DIV(p->bits[2] * 8 + p->bits[1] * 4 + p->bits[0] * 0, 12);
    if ((rc->rcMode == JPEGENC_VBR) && (error_bits < 0)) {
      error_bits /= 2;
    }
    return error_bits;
  }

  /* Avoid overflow */
  return jpegRcCalculate(p->bits[2], 8, 10) +
         jpegRcCalculate(p->bits[1], 4, 10);
}

/*------------------------------------------------------------------------------
  new_pic_quant()  Calculate new quantization parameter from the 2nd degree R-Q
  equation. Further adjust Qp for "smoother" visual quality.
------------------------------------------------------------------------------*/
static i32 new_pic_quant(jpegLinReg_s *p, jpegEncRateControl_s *rc, i32 bits,
                         true_e useQpDeltaLimit, u32 complexity,
                         i32 lastActualBits, i32 lastTargetBits,
                         i32 currentTargetBits) {
  i64 diff_prev = 0, tmp = 0, diff, diff_best = 0x7FFFFFFFFF;

  i32 qp_prev = 0, qp_best = p->qp_prev, qp = p->qp_prev;
  i32 resetQp = 0;

  RC_LOG_I("R/cx:%6d \n", bits);

  if (p->a1 == 0 && p->a2 == 0) {
    //printf("p->a1 == 0 && p->a2 == 0\n");
    RC_LOG_I(" QP: xx xx D:  ==== newQP: %2d\n", qp);
    p->qp_prev = qp;
    return qp;
  }

  /* Find the qp that has the best match on fitted curve */
  do {
#ifdef OPT_PIC_QUANT_DIV
    i32 cur_q_step = q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)];
    tmp = DIV2((p->a1 * cur_q_step + p->a2), (i64)(cur_q_step * cur_q_step));
#else
    tmp = DIV(p->a1, q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)]);
    tmp += DIV(p->a2, (i64)q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)] *
                          q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)]);
#endif
    //diff = ABS(tmp*p->weight/10 - (bits << JPEG_QP_FRACTIONAL_BITS));
    diff = ABS(tmp - ((i64)bits << JPEG_QP_FRACTIONAL_BITS));

    if (diff < diff_best) {
      if (diff_best == 0x7FFFFFFFFF) {
        diff_prev = diff;
        qp_prev = qp;
      } else {
        diff_prev = diff_best;
        qp_prev = qp_best;
      }
      diff_best = diff;
      qp_best = qp;
      if ((tmp - (bits << JPEG_QP_FRACTIONAL_BITS)) <= 0) {
        qp -= QP_FIXPOINT_0_POINT_1;

        if (qp < 0) {
          qp_best = 0;
          break;
        }
      } else {
        qp += QP_FIXPOINT_0_POINT_1;

        if (qp > 0x3300) {
          qp_best = 0x3300;
          break;
        }
      }
    } else {
      break;
    }
  } while ((qp >= 0) && (qp <= 0x3300));

  qp = qp_best;

  RC_LOG_I("new QP %d, model a2 %ld(%f), a1 %ld(%f)\n", qp, p->a2,
           p->a2 / 256.0 / 256.0 / 256.0, p->a1, p->a1 / 256.0 / 256.0);

  if (lastActualBits && lastTargetBits) {
    /* This condition means that R(Q) history conflict with R(Q) of last frame.
           Just make Qp same as that of last frame. */
    if (((currentTargetBits >= lastActualBits) && (qp > p->qp_prev)) ||
        ((currentTargetBits <= lastActualBits) && (qp < p->qp_prev)))
      qp = p->qp_prev;
  }

new_pic_quant_end:

  qp = CLIP3(0, 0x3300, qp);
  RC_LOG_I("new adj QP %d %2.2f, qp_prev %d, D: %ld\n", qp, qp / 256.0, qp_prev,
           diff_prev - diff_best);

  true_e scaleQpEna = ENCHW_YES;
  if (rc->inputSceneChange)
    resetQp = InitialQp(rc->virtualBuffer.bitPerPic, rc->picArea);

  /* Limit Qp change for smoother visual quality */
  if (useQpDeltaLimit) {
    i32 minQp = p->qp_prev + (rc->picQpDeltaMin << JPEG_QP_FRACTIONAL_BITS);
    i32 maxQp = p->qp_prev + (rc->picQpDeltaMax << JPEG_QP_FRACTIONAL_BITS);
    i32 delta = qp - p->qp_prev;
    tmp = ABS(delta);

    //scale qpDelta
    if (scaleQpEna) {
      if (tmp > ((1 << JPEG_QP_FRACTIONAL_BITS) +
                 (1 << (JPEG_QP_FRACTIONAL_BITS - 1))))
        tmp >>= 1;
      else if (tmp <= (1 << (JPEG_QP_FRACTIONAL_BITS - 2)))
        tmp = tmp;		//cppcheck-suppress selfAssignment
      else
        tmp = tmp * 2 / 3;
    }

    qp = (delta >= 0) ? (p->qp_prev + tmp) : (p->qp_prev - tmp);
    qp = MAX(qp, resetQp);
    qp = CLIP3(minQp, maxQp, qp);
  } else {
    qp = MAX(qp, resetQp);
  }

  RC_LOG_I("new final QP %2.2f, resetQP %d, limit %d\n", qp / 256.0, resetQp,
           useQpDeltaLimit);
  return qp;
}

/*------------------------------------------------------------------------------
  update_tables()  only statistics of PSLICE, please.
------------------------------------------------------------------------------*/
static void update_tables(jpegLinReg_s *p, i32 qp, i32 bits) {
  const i32 clen = JPEG_RC_TABLE_LENGTH;
  i32 tmp = p->pos;

  p->qp_prev = qp;
  p->qs[tmp] = q_step[((qp * 10) >> JPEG_QP_FRACTIONAL_BITS)];
  p->bits[tmp] = bits;

  RC_LOG_D("upd table: qs %i  bits %i  qp %d %2.2f\n", p->qs[tmp], bits, qp,
           qp / 256.0);

  if (++p->pos >= clen) {
    p->pos = 0;
  }
  if (p->len < clen) {
    p->len++;
  }
}

/*------------------------------------------------------------------------------
            update_model()  Update model parameter by Linear Regression.
------------------------------------------------------------------------------*/
static void update_model(jpegLinReg_s *p) {
  i32 i, n = p->len, *r = p->bits;
  i32 *qs = p->qs;
  i64 a1, a2, sx = lin_sx(qs, n), sy = lin_sy(qs, r, n);

  for (i = 0; i < n; i++) {
    RC_LOG_D("model: qs %i  r %i\n", qs[i], r[i]);
  }

  a1 = lin_sxy(qs, r, n);
  if (n !=0)
    a1 = a1 < (I64_MAX / n) ? a1 * n : I64_MAX;

  if (sy == 0) {
    a1 = 0;
  } else {
    a1 -= sx < I64_MAX / sy ? sx * sy : I64_MAX;
  }

  a2 = (lin_nsxx(qs, n) - (sx * sx));
  if (a2 == 0) {
    if (p->a1 == 0) {
      /* If encountered in the beginning */
      a1 = 0;
    } else {
      a1 = (p->a1 * 2) / 3;
    }
  } else {
    a1 = ABS(a1) < (I64_MAX / DSCY) ? a1 * DSCY / a2
                                    : (a1 > 0 ? I64_MAX / a2 : -I64_MAX / a2);
  }

  /* Value of a1 shouldn't be excessive (small) */
  /* forbid negtive a1 and a2 to avoid breaking monotone RC curve*/
  a1 = MAX(a1, 0);
  a1 = MIN(a1, ((i64)262144 * 256 * 256 - 1));

  ASSERT(ABS(a1) * sx >= 0);
  ASSERT(sx * DSCY >= 0);
  a2 = DIV((sy * DSCY), n) - DIV((a1 * sx), n);
  if (a2 < 0) {
    a2 = 0;
    a1 = sx ? sy * DSCY / sx : 0;
  }

  RC_LOG_D("model: a2:%ld(%f)  a1:%ld(%f)\n", a2, a2 / 256.0 / 256.0 / 256.0,
           a1, a1 / 256.0 / 256.0);

  if (p->len > 0) {
    p->a1 = a1;
    p->a2 = a2;
  }
}

/*------------------------------------------------------------------------------
  lin_sy()  calculate value of Sy for n points.
------------------------------------------------------------------------------*/
static i64 lin_sy(i32 *qp, i32 *r, i32 n) {
  i64 sum = 0;

  while (n--) {
    sum += (i64)qp[n] * qp[n] * (r[n] << JPEG_QP_FRACTIONAL_BITS);
    if (sum < 0) {
      return I64_MAX / DSCY;
    }
  }
  return (DIV(sum, DSCY));
}

/*------------------------------------------------------------------------------
  lin_sx()  calculate value of Sx for n points.
------------------------------------------------------------------------------*/
static i64 lin_sx(i32 *qp, i32 n) {
  i64 tmp = 0;

  while (n--) {
    ASSERT(qp[n]);
    tmp += qp[n];
  }
  return tmp;
}

/*------------------------------------------------------------------------------
  lin_sxy()  calculate value of Sxy for n points.
------------------------------------------------------------------------------*/
static i64 lin_sxy(i32 *qp, i32 *r, i32 n) {
  i64 tmp, sum = 0;

  while (n--) {
    tmp = (i64)qp[n] * (i64)qp[n] * (i64)qp[n];
    //tmp =>> (JPEG_QP_FRACTIONAL_BITS*3);
    if (tmp > (r[n] << (3 * JPEG_QP_FRACTIONAL_BITS))) {
      sum += DIV(tmp, DSCY) * (r[n] << (JPEG_QP_FRACTIONAL_BITS));
    } else {
      sum += tmp * DIV((r[n] << (JPEG_QP_FRACTIONAL_BITS)), DSCY);
    }
    if (sum < 0) {
      return I64_MAX;
    }
  }
  return sum;
}

/*------------------------------------------------------------------------------
  lin_nsxx()  calculate value of n * Sxy for n points.
------------------------------------------------------------------------------*/
static i64 lin_nsxx(i32 *qp, i32 n) {
  i64 d = n;
  i64 tmp = 0, sum = 0;

  while (n--) {
    tmp = qp[n];
    tmp *= tmp;
    sum += d * tmp;
  }
  return sum;
}

#endif
