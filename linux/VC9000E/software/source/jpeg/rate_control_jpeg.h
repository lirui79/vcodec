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

#ifndef RATE_CONTROL_JPEG_H
#define RATE_CONTROL_JPEG_H

#include "base_type.h"
#include "enccommon.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "EncJpeg.h"

#ifndef CTBRC_STRENGTH
#define JPEG_QP_FRACTIONAL_BITS 0
#else
#define JPEG_QP_FRACTIONAL_BITS 8
#endif

#define JPEG_LEAST_MONITOR_FRAME 3
#define JPEG_QP_DEFAULT 26

#define JPEG_RC_TABLE_LENGTH 10 /* DO NOT CHANGE THIS */

typedef struct {
  i64 a1;                           /* model parameter */
  i64 a2;                           /* model parameter */
  i32 qp_prev;                      /* previous QP */
  i32 qs[JPEG_RC_TABLE_LENGTH + 1]; /* quantization step size */
  i32 bits[JPEG_RC_TABLE_LENGTH +
           1];  /* Number of bits needed to code residual */
  i32 pos;      /* current position */
  i32 len;      /* current lenght */
  i32 zero_div; /* a1 divisor is 0 */
  i32 cbr;
  i32 weight;
  i32 frameBitCntLast;
  i32 targetPicSizeLast;
} jpegLinReg_s;

/* Virtual buffer */
typedef struct {
  i32 bufferSize;      /* size of the virtual buffer */
  i32 movingMinRate;   /* moving min bitrate */
  i32 movingMaxRate;   /* moving max bitrate */
  i32 maxBitRate;      /* cpb max bitrate */
  i32 bufferRate;      /* bitPerPic with maxBitRate */
  i32 bitRate;         /* input bit rate per second */
  i32 bitPerPic;       /* average number of bits per picture */
  i32 picTimeInc;      /* timeInc since last coded picture */
  i32 timeScale;       /* input frame rate numerator */
  i32 unitsInTic;      /* input frame rate denominator */
  i32 virtualBitCnt;   /* virtual (channel) bit count */
  i32 realBitCnt;      /* real bit count */
  i32 bufferOccupancy; /* number of bits in the buffer */
  i32 skipFrameTarget; /* how many frames should be skipped in a row */
  i32 skippedFrames;   /* how many frames have been skipped in a row */
  i32 nonZeroTarget;
  i32 bucketFullness; /* Leaky Bucket fullness */
  i32 bucketLevel;    /* Leaky Bucket fullness + virtualBitCnt */
  /* new rate control */
  i32 windowRem;
  i32 seconds;        /* Full seconds elapsed */
  i32 averageBitRate; /* This buffer average bitrate for full seconds */
} jpegRcVirtualBuffer_s;

typedef struct {
  true_e picRc;
  JpegEncRcMode rcMode;
  true_e picSkip; /* Frame Skip enable */
  true_e hrd;     /* HRD restrictions followed or not */
  true_e vbr;     /* Variable Bit Rate Control */
  i32 vbrOn;      /* vbr mode turn on or off by condition */
  u32 fillerIdx;
  i32 picArea;
  i32 ctbPerPic; /* Number of macroblock per picture */
  i32 ctbRows;   /* ctb rows in picture */
  i32 ctbCols;   /* ctb columns in picture */
  i32 ctbSize;   /* ctb size */
  i32 qpSum;     /* Qp sum counter */
  i32 qpNum;
  u32 sliceTypeCur;
  u32 sliceTypePrev;
  true_e frameCoded; /* Pic coded information */
  i32 fixedQp;       /* Pic header qp when fixed */
  i32 qpHdr;         /* Pic header qp of current voded picture */
  i32 qpMin;         /* Pic header minimum qp for current picture */
  i32 qpMax;         /* Pic header maximum qp for current picture */
  i32 qpMinI;        /* Pic header minimum qp for I frame, user set */
  i32 qpMaxI;        /* Pic header maximum qp for I frame, user set */
  i32 qpHdrPrev;     /* Pic header qp of previous coded picture */
  i32 qpLastCoded;   /* Quantization parameter of last coded mb */
  i32 qpTarget;      /* Target quantrization parameter */
  i32 qpISlice; /* qp of I slice, not changed until I qp is calculated by I slice model */
  i32 qpHdrPrevGop;
  u32 estTimeInc;
  i32 outRateNum;
  i32 outRateDenom;
  i32 gDelaySum;
  i32 gInitialDelay;
  i32 gInitialDoffs;
  jpegRcVirtualBuffer_s virtualBuffer;
  i32 gBufferMin, gBufferMax;
  /* new rate control */
  jpegLinReg_s intra;      /* Data for intra frames */
  jpegLinReg_s intraError; /* Prediction error for intra frames */
  i32 targetPicSize;
  i32 gopHeadTargetPicSize; /* record targetPicSize of GOP headFrame (I/P) for pass-2 */
  i32 targetGopSize;    /* target gop bit size */
  i32 actualGopSize;    /* current gop bit size so far */
  i32 encodedGopFrames; /* encoded frames number of gop */
  i32 minPicSizeI;
  i32 maxPicSizeI;
  i32 frameBitCnt;
  i32 tolMovingBitRate;
  i32 monitorFrames;
  float tolCtbRcInter;
  float tolCtbRcIntra;
  /* for gop rate control */
  u32 frameCnt;
  i32 bitrateWindow;
  i32 windowLen;     /* Bitrate window which tries to match target */
  u32 fixedIntraQp;
  i32 bpp;
  i32 encoded_frame_number;
  i32 errBits;
  i32 picQpDeltaMin;
  i32 picQpDeltaMax;
  i32 inputSceneChange;

  i32 minIQp;
  i32 finiteQp;
  i32 gopMulti;
  i32 gopLastBitCnt;
  i32 intraframeBitCnt;
  u32 codingType;
} jpegEncRateControl_s;

/*------------------------------------------------------------------------------
      Function prototypes
  ------------------------------------------------------------------------------*/
#ifndef RATE_CONTROL_BUILD_SUPPORT
#define JpegEncInitRc(rc, newStream) (0)
#define JpegEncBeforePicRc(rc, timeInc, sliceType, sw_picture)
#define JpegEncAfterPicRc(rc, nonZeroCnt, byteCnt, qpSum, qpNum) (0)
#define jpegRcCalculate(a, b, c) ((c) == 0 ? 0 : ((a) * (b) / (c)))
#else
bool_e JpegEncInitRc(jpegEncRateControl_s *rc, u32 newStream);
void JpegEncBeforePicRc(jpegEncRateControl_s *rc, u32 timeInc, u32 sliceType,
                        void *);
i32 JpegEncAfterPicRc(jpegEncRateControl_s *rc, u32 nonZeroCnt, u32 byteCnt,
                      u32 qpSum, u32 qpNum);
i32 jpegRcCalculate(i32 a, i32 b, i32 c);
#endif

#ifdef __cplusplus
}
#endif

#endif
