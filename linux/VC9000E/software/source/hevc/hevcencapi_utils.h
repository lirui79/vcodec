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
-- Abstract : Hevc API utils Messages.
--
------------------------------------------------------------------------------*/
#ifndef API_UTILS_H
#define API_UTILS_H

#include "instance.h"
#include "hevcencapi.h"


#define PRM_SET_BUFF_SIZE 1024            /* Parameter sets max buffer size */
#define RPS_SET_BUFF_SIZE 140             /* rps sets max buffer size */
#define VCENC_MAX_BITRATE (800000 * 1000) /* Level 6.2 high tier limit */

/* HW ID check. VCEncInit() will fail if HW doesn't match. */

#define VCENC_MIN_ENC_WIDTH 128
#define VCENC_MAX_ENC_WIDTH 8192
#define VCENC_MIN_ENC_HEIGHT 128
#define VCENC_MAX_ENC_HEIGHT 8192
#define VCENC_MAX_ENC_HEIGHT_EXT 8640
#define VCENC_HEVC_MAX_LEVEL VCENC_HEVC_LEVEL_6_2
#define VCENC_H264_MAX_LEVEL VCENC_H264_LEVEL_6_2

#define VCENC_DEFAULT_QP 26

#define VCENC_MAX_PP_INPUT_WIDTH 32768

#define VCENC_MAX_USER_DATA_SIZE 2048

#define VCENC_AV1_MAX_ENC_WIDTH 4096
#define VCENC_AV1_MAX_ENC_AREA (4096 * 2304)

#define VCENC_VP9_MAX_ENC_WIDTH 4096
#define VCENC_VP9_MAX_ENC_HEIGHT 8384
#define VCENC_VP9_MAX_ENC_AREA (4096 * 2176)

#define VCENC_BUS_ADDRESS_VALID(bus_address) \
  (((bus_address) != 0) /*&& \
                                              ((bus_address & 0x0F) == 0)*/)

#define VCENC_BUS_CH_ADDRESS_VALID(bus_address) \
  (((bus_address) != 0) /*&& \
                                              ((bus_address & 0x07) == 0)*/)

/** integer QP to High precision QP */
#define TO_HQP(x) ((x)<<QP_FRACTIONAL_BITS)

#define FIX_POINT_LAMBDA

#define LARGE_QPFACTOR_FOR_BIGQP

#define APIPRINT(v, ...)        \
  {                             \
    if (v) printf(__VA_ARGS__); \
  }

#define HEVC_LEVEL_NUM 13
#define H264_LEVEL_NUM 20
#define AV1_LEVEL_NUM 24
#define VP9_LEVEL_NUM 14
#define AV1_VALID_MAX_LEVEL 15
#define VP9_VALID_MAX_LEVEL 10

#define HDR10_NOCFGED 0
#define HDR10_CFGED 1
#define HDR10_CODED 2

#define EXCEPT_VP9_GOP4_TO_GOP1_1PASS(format, intra_ratio_average, skip_ratio_average, frame5, frame7, pass) \
       ((pass == 0) && (format != VCENC_VIDEO_CODEC_VP9) && ((intra_ratio_average >= 0.6) || ((intra_ratio_average >= 0.25) && frame5.intra_ratio >= 0.15 && (((skip_ratio_average + intra_ratio_average) >= 0.7)) && frame7.skip_ratio >= 0.8)))

#define EXCEPT_VP9_GOP4_TO_GOP1_PASS2(format, intra_ratio_average, skip_ratio_average, frame7, pass) \
       ((pass == 2) && (format != VCENC_VIDEO_CODEC_VP9) && ((intra_ratio_average >= 0.7) || ((intra_ratio_average >= 0.25) && ((skip_ratio_average + intra_ratio_average) >= 0.8) && frame7.skip_ratio >= 0.91)))

#define VP9_GOP4_TO_GOP1(format, intra_ratio_average, frame5, pass) \
       ((format == VCENC_VIDEO_CODEC_VP9) && ((((intra_ratio_average >= 0.6) || ((intra_ratio_average >= 0.25) && frame5.intra_ratio >= 0.15)) && (pass == 0)) || ((intra_ratio_average >= 0.7) && (pass == 2))))

#define GOP4_TO_GOP8(format, frame5) \
       (!(((frame5.intra_ratio >= (format == VCENC_VIDEO_CODEC_H264 ? 0.7 : 0.5) || (frame5.intra_ratio >= 0.25 && frame5.skip_ratio >= 0.5)) && format != VCENC_VIDEO_CODEC_VP9) || \
            (frame5.intra_ratio >= 0.4 && format == VCENC_VIDEO_CODEC_VP9)))

#define EXCEPT_VP9_GOP8_TO_GOP4(format, frame0, frame1) \
       ((format != VCENC_VIDEO_CODEC_VP9) && (frame0.intra_ratio >= ((format == VCENC_VIDEO_CODEC_H264) ? 0.5 : 0.7)) && \
        (frame1.skip_ratio >= (format == VCENC_VIDEO_CODEC_H264 ? 0.3 : 0.15)))

#define VP9_GOP8_TO_GOP4(format, frame0, frame1) \
       ((format == VCENC_VIDEO_CODEC_VP9) && (frame0.intra_ratio >= 0.5) && (frame1.intra_ratio >= 0.2 && frame1.intra_ratio <= 0.4))

/*------------------------------------------------------------------------------
       Encoder API utils function prototypes
  ------------------------------------------------------------------------------*/
void VCEncShutdown(VCEncInst inst);
VCEncRet VCEncClear(VCEncInst inst);
VCEncRet VCEncStop(VCEncInst inst);
void GenNextPicConfig(VCEncIn *pEncIn, const u8 *gopCfgOffset,
                      i32 i32LastPicPoc, struct vcenc_instance *vcenc_instance);
u64 CalNextPic(VCEncGopConfig *cfg, int picture_cnt);
void StrmEncodeTraceEncInPara(VCEncIn *pEncIn,
                              struct vcenc_instance *vcenc_instance);
VCEncRet StrmEncodeCheckPara(struct vcenc_instance *vcenc_instance,
                             VCEncIn *pEncIn, VCEncOut *pEncOut,
                             asicData_s *asic, u32 client_type);
void StrmEncodeMeqnSimpleBinCostConfig(struct vcenc_instance *vcenc_instance,
                                       asicData_s *asic);
void StrmEncodeSmartskipConfig(i32 prevQP, asicData_s *asic,
                              VCEncIn *pEncIn,
                              struct vcenc_instance *vcenc_instance);
void StrmEncodeGlobalmvConfig(asicData_s *asic, struct sw_picture *pic,
                              VCEncIn *pEncIn,
                              struct vcenc_instance *vcenc_instance);
void StrmEncodeOverlayConfig(asicData_s *asic, VCEncIn *pEncIn,
                             struct vcenc_instance *vcenc_instance);
void StrmEncodeOSDMapConfig(asicData_s *asic, VCEncIn *pEncIn,
                             struct vcenc_instance *vcenc_instance);
void StrmEncodePrefixSei(struct vcenc_instance *vcenc_instance, struct sps *s,
                         VCEncOut *pEncOut, struct sw_picture *pic,
                         VCEncIn *pEncIn);
void StrmEncodeSuffixSei(struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                         VCEncOut *pEncOut);
void StrmEncodeGradualDecoderRefresh(struct vcenc_instance *vcenc_instance,
                                     asicData_s *asic, VCEncIn *pEncIn,
                                     VCEncPictureCodingType *codingType,
                                     const EWLHwConfig_t *cfg);
void StrmEncodeRegionOfInterest(struct vcenc_instance *vcenc_instance,
                                asicData_s *asic);
VCEncRet EncGetSSIM(struct vcenc_instance *inst, VCEncOut *pEncOut);
void CalculateSSIM(struct vcenc_instance *inst, VCEncOut *pEncOut,
                   i64 ssim_numerator_y, i64 ssim_numerator_u,
                   i64 ssim_numerator_v, u32 ssim_denominator_y,
                   u32 ssim_denominator_uv);
VCEncRet EncGetPSNR(struct vcenc_instance *inst, VCEncOut *pEncOut);
void CalculatePSNR(struct vcenc_instance *inst, VCEncOut *pEncOut, u32 width);
VCEncRet TemporalMvpGenConfig(struct vcenc_instance *vcenc_instance,
                              VCEncIn *pEncIn, struct container *c,
                              struct sw_picture *pic,
                              VCEncPictureCodingType codingType);
VCEncRet VCEncVisualGenConfig(struct vcenc_instance *vcenc_instance, struct sw_picture *pic, VCEncIn *pEncIn);
void VCEncVisualStrengthUpdate(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn);
VCEncRet GenralRefPicMarking(struct vcenc_instance *vcenc_instance,
                             struct container *c, struct rps *r,
                             VCEncPictureCodingType codingType, VCEncIn *pEncIn);
VCEncRet IdrRefPicFlush(struct vcenc_instance *vcenc_instance,
                        struct container *c, i32 useful_poc, VCEncIn *pEncIn);

i32 EncInitLookAheadBufCnt(const VCEncConfig *config, i32 lookaheadDepth);

i32 FindNextForceIdr(struct queue *jobQueue);

i32 AGopDecision(const struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                 const VCEncOut *pEncOut, i32 *pNextGopSize,
                 VCENCAdapGopCtr *agop);

i32 AGopDecisionRefine(const struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                 const VCEncOut *pEncOut, i32 *pNextGopSize,
                 VCENCAdapGopCtr *agop);

void VCEncPass2CheckAgop(VCEncInst inst, VCEncIn *pEncIn, VCEncOut *pEncOut,
                 VCEncRet ret);

void VCEncAddNaluSize(VCEncOut *pEncOut, u32 naluSizeBytes, u32 tileId);

VCEncRet VCEncCodecPrepareEncode(struct vcenc_instance *vcenc_instance,
                                 const VCEncIn *pEncIn, VCEncOut *pEncOut,
                                 VCEncPictureCodingType codingType,
                                 struct sw_picture *pic, struct container *c,
                                 u32 *segcounts);

VCEncRet VCEncCodecPostEncodeUpdate(struct vcenc_instance *vcenc_instance,
                                    const VCEncIn *pEncIn, VCEncOut *pEncOut,
                                    VCEncPictureCodingType codingType,
                                    struct sw_picture *pic);

VCEncRet VCEncFlushDisplay(struct vcenc_instance *vcenc_instance,
                                    const VCEncIn *pEncIn, VCEncOut *pEncOut,
                                    VCEncPictureCodingType codingType);

void EndOfSequence(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn,
                   VCEncOut *pEncOut);
void SavePicCfg(const VCEncIn *pEncIn, VCEncPicConfig *pPicCfg);

VCEncRet SinglePassEnqueueJob(struct vcenc_instance *vcenc_instance,
                              const VCEncIn *pEncIn);

VCEncJob *SinglePassGetNextJob(struct vcenc_instance *vcenc_instance,
                               const i32 picCnt);

void InitAgop(VCENCAdapGopCtr *agop);

void SetPicCfgToEncIn(const VCEncPicConfig *pPicCfg, VCEncIn *pEncIn);

VCEncPictureCodingType FindNextPic(VCEncInst inst, VCEncIn *encIn,
                                   i32 nextGopSize, const u8 *gopCfgOffset,
                                   i32 nextIdrCnt, bool fpsChange);

/* manage task information */
VCEncRet VceTaskCreate(struct vcenc_instance *inst, VCEncIn *in);
VCEncRet VceTaskRelease(struct vcenc_instance *inst, VCEncIn *in);


/* multi-tile */
void TileInfoConfig(struct vcenc_instance *vcenc_instance,
                    struct sw_picture *pic, u32 tileId,
                    VCEncPictureCodingType codingType, VCEncOut *pEncOut);
u32 TileTotalStreamSize(struct vcenc_instance *vcenc_instance);
void TileInfoCollect(struct vcenc_instance *vcenc_instance, u32 tileId,
                     u32 numNalu);
u32 FindIndexBywaitCoreJobid(struct vcenc_instance *vcenc_instance,
                             u32 waitCoreJobid);
void FillVCEncout(struct vcenc_instance *vcenc_instance, VCEncOut *pEncOut);

//[pass2/single pass]update coding ctrl parameters matched with current frame in instance
void EncUpdateCodingCtrlParam(struct vcenc_instance *pEncInst,
                              EncCodingCtrlParam *pCodingCtrlParam,
                              const i32 picCnt);
//[pass1]update coding ctrl parameters matched with current frame in instance
void EncUpdateCodingCtrlForPass1(VCEncInst pEncInst,
                                 EncCodingCtrlParam *pCodingCtrlParam);
//[pass1/pass2/single pass]update rate ctrl parameters matched with current frame in instance
void EncUpdateRateCtrlParam(struct vcenc_instance *pEncInst,
                              EncRateCtrlParam *pRateCtrlParam,
                              const VCEncIn *pEncIn);
bool EnqueueFpsRequest(struct queue *fpsRequestQueue, EncFpsRequest *req);
bool checkFpsUpdate(struct vcenc_instance *vcenc_instance, int picture_cnt);
VCEncRet EncFpsUpdate(struct vcenc_instance *vcenc_instance,
                  struct queue *jobQueue,
                  EncRateCtrlParam *pRateCtrlParam,
                  const VCEncIn *pEncIn);
VCEncJob *findJob(struct queue *jobQueue, int picture_cnt);

u32 EncGetCodingMode(VCEncVideoCodecFormat codecFormat);
/* check inputed coding ctrl parameters*/
VCEncRet EncCheckCodingCtrlParam(struct vcenc_instance *pEncInst,
                                 VCEncCodingCtrl *pCodeParams);
/* check input rate ctrl parameters */
VCEncRet EncCheckRateCtrlParam(struct vcenc_instance *pEncInst,
                                 VCEncRateCtrl *pRateCtrl);

void VCEncEncodeSeiHdr10(struct vcenc_instance *vcenc_instance,
                         VCEncOut *pEncOut);
void VideoTuneConfig(const VCEncConfig *config,
                     struct vcenc_instance *vcenc_inst);
/* set picture configs to job */
void SetPictureCfgToJob(const VCEncIn *pEncIn, VCEncIn *pJobEncIn,
                        u8 gdrDuration);

#ifndef RATE_CONTROL_BUILD_SUPPORT
/* set qpHdr and fixedQp. */
bool_e VCEncInitQp(vcencRateControl_s *rc, u32 newStream);
/* calculate qpHdr for current picture. */
void VCEncBeforeCQP(vcencRateControl_s *rc, u32 timeInc, u32 sliceType,
                    bool use_ltr_cur, struct sw_picture *pic);
#endif

/*init coding ctrl parameter queue*/
void EncInitCodingCtrlQueue(VCEncInst inst);

/*init rate ctrl parameter queue*/
void EncInitRateCtrlQueue(VCEncInst inst);

void VCEncGetFrameInfo(struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn, asicData_s *asic, VCEncOut *pEncOut);

void VCEncCfgAxiFe(struct vcenc_instance *inst, asicData_s *asic, u32 cutree, u32 tileId);

void VCEncSetApbFilter(void *inst);

void VCEncSetwaitJobCfg(VCEncIn *pEncIn, struct vcenc_instance *vcenc_instance,
                        asicData_s *asic, u32 waitCoreJobid);

void VCEncCfgDec400(VCEncIn *pEncIn,
                    struct vcenc_instance *vcenc_instance, asicData_s *asic,
                    u32 tileId);

i32 VCEncCfgUfbc(VCEncIn *pEncIn,
                    struct vcenc_instance *vcenc_instance, asicData_s *asic,
                    EncUfbcParam *ufbc_param);

VCEncRet VCEncSetSubsystem(struct vcenc_instance *vcenc_instance,
                           VCEncIn *pEncIn, asicData_s *asic,
                           struct sw_picture *pic, u32 tileId);

void VCEncResetCallback(VCEncSliceReady *slice_callback,
                        struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                        VCEncOut *pEncOut, u32 next_core_index,
                        u32 multicore_flag);
void VCEncSetRingBuffer(struct vcenc_instance *vcenc_instance, asicData_s *asic,struct sw_picture *pic);
u32 VCEncGetChromaIdc(u32 InputChromaIdc, struct vcenc_instance *vcenc_inst);
void VCEncSetCropOffset(struct vcenc_instance *vcenc_instance, preProcess_s *pp_tmp);
void VCEncExcessBitrateConfigTuning(struct vcenc_instance *vcenc_instance, double * qpfactor, VCEncPictureCodingType codingType);

VCEncRet VCEncStrmGetOutput(VCEncInst inst, VCEncIn *pEncIn, VCEncOut *pEncOut,
                    VCEncSliceReadyCallBackFunc sliceReadyCbFunc,
                    void *pAppData);
VCEncJob *handleSkipFrame(struct vcenc_instance *vcenc_instance, i32 *pNextGopSize, VCEncOut *pEncOut, VCEncIn *pEncInFor1pass, VCEncJob *job);

#endif
