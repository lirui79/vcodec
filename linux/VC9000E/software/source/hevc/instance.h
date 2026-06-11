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

#ifndef INSTANCE_H
#define INSTANCE_H

#include "container.h"
#include "enccommon.h"
#include "encpreprocess.h"
#include "encasiccontroller.h"

#include "hevcencapi.h" /* Callback type from API is reused */
#include "vcenc_aif.h"
#include <rate_control_picture.h>
#include "hash.h"
#include "sw_cu_tree.h"
#include "encdec400.h"
#include "enc_log.h"
#include "encufbc.h"

#ifdef SUPPORT_AV1
#include "av1instance.h"
#endif

#ifdef SUPPORT_VP9
#include "vp9enccommon.h"
#include "vp9instance.h"
#endif

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif


/** \brief Contains configurations of a picture. */
typedef struct {
  // GOP config related

  /** \brief The proposed picture coding type.
   *  \n Valid values include VCENC_INTRA_FRAME, VCENC_PREDICTED_FRAME, and VCENC_BIDIR_PREDICTED_FRAME. */
  VCEncPictureCodingType
      codingType;

  i32 poc;                  /**< \brief The picture order count (POC). */
  VCEncGopConfig gopConfig; /**< \brief The configurations of the current GOP. */
  i32 gopSize;              /**< \brief The size of the current GOP structure. */
  i32 gopPicIdx;            /**< \brief The order count of the current picture within the GOP, in range [0, gopSize-1]. */
  i32 picture_cnt;          /**< \brief The encoded picture count. */
  i32 picture_gopIdx;       /**< \brief The encoded picture GOP count. */
  i32 last_idr_picture_cnt; /**< \brief The encoded picture count of the last IDR frame. */

  /** \brief Whether to encode the current picture as an IDR frame, after which a new GOP is followed.
   *  \n <b>0</b>: do not encode as an IDR frame.
   *  \n <b>1</b>: encode as an IDR frame. */
  u32 bIsIDR;

  /** \brief Whether to encode the current picture as a non-IDR intra frame.
   *  \n <b>0</b>: do not encode as a non-IDR intra frame.
   *  \n <b>1</b>: encode as a non-IDR intra frame. */
  u32 bIsIntraOnly;

  // long-term reference relatived info, filled by GenNextPicConfig
  /** \brief Whether to periodically use long-term reference (LTR).
   *  \n <b>0</b>: do not periodically use
   *  \n <b>1</b>: periodically use */
  u32 bIsPeriodUsingLTR;

  /** \brief Whether to periodically update LTR frames.
   *  \n <b>0</b>: do not periodically update
   *  \n <b>1</b>: periodically update */
  u32 bIsPeriodUpdateLTR;

  VCEncGopPicConfig
      gopCurrPicConfig; /**< \brief The reference descriptions of the current picture. */

  /** \brief The POC of each LTR frame.
   *  \n A value greater than or equal to <b>0</b> indicates the POC of an LTR frame.
   *  \n A value of <b>-1</b> is indicates an invalid POC.*/
  i32 long_term_ref_pic
      [VCENC_MAX_LT_REF_FRAMES];

  /** \brief Whether the current picture uses each LTR frame.
   *  \n <b>0</b>: does not use
   *  \n <b>1</b>: uses */
  u32 bLTR_used_by_cur
      [VCENC_MAX_LT_REF_FRAMES];

  /** \brief Whether to update the POC of each LTR frame after encoding the current frame.
   *  \n <b>0</b>: do not update.
   *  \n <b>1</b>: update. */
  u32 bLTR_need_update
      [VCENC_MAX_LT_REF_FRAMES];

  /** \brief The special RPS index used by the current picture.
   *  \n The field value is greater than or equal to <b>-1</b>.
   *  \n If the field value is <b>-1</b>, it indicates that the current picture does not use special RPS index. */
  i8 i8SpecialRpsIdx;

  /** \brief The special RPS index used by the next picture.
   *  \n The field value is greater than or equal to <b>-1</b>.
   *  \n If the field value is <b>-1</b>, it indicates that the next picture does not use special RPS index.
   *  \n This field is valid only for H.264. */
  i8 i8SpecialRpsIdx_next;

  /** \brief current picture coded as LTR
   *  \n 0 - not an LTR
   *  \n 1..VCENC_MAX_LT_REF_FRAMES - idx of LTR */
  u8 u8IdxEncodedAsLTR;
} VCEncPicConfig;

/*Coding ctrl parameter*/
typedef struct {
  struct node *next;
  VCEncCodingCtrl encCodingCtrl; /*codingCtrl Parameter*/
  i32 startPicCnt; /*the picture_cnt for when parameters in encode order start to go into force*/
  u32 refCnt; /*number of this set of parameter in use*/
} EncCodingCtrlParam;

/*Coding ctrl*/
typedef struct {
  struct queue codingCtrlBufPool; /*codingCtrl parameter buffer pool*/
  struct queue codingCtrlQueue;   /*codingCtrl parameter queue*/
  /*corresponding poniter in codingCtrlQueue of current enforced coding ctrl parameters */
  EncCodingCtrlParam *pCodingCtrlParam;
} EncCodingCtrl;

/*Rate ctrl parameter*/
typedef struct {
  struct node *next;
  VCEncRateCtrl encRateCtrl; /*rateCtrl Parameter*/
  i32 startPicCnt; /*the picture_cnt for when parameters in encode order start to go into force*/
  u32 refCnt; /*number of this set of parameter in use*/
} EncRateCtrlParam;
/*FPS update request parameter*/
typedef struct {
  struct node *next;
  u32 frameRateNum;   /* frame rate numer */
  u32 frameRateDenom; /* frame rate denom */
  i32 frameRateUpdatePicCnt; /* frame rate update picture count */
} EncFpsRequest;

/*Rate ctrl*/
typedef struct {
  struct queue rateCtrlBufPool; /*rateCtrl parameter buffer pool*/
  struct queue rateCtrlQueue;   /*rateCtrl parameter queue*/
  struct queue fpsRequestBufPool; /*rateCtrl parameter buffer pool*/
  struct queue fpsRequestQueue;   /* FPS adjust request queue*/
  /*corresponding poniter in rateCtrlQueue of current enforced rate ctrl parameters */
  EncRateCtrlParam *pRateCtrlParam;
} EncRateCtrl;

typedef struct {
  struct node *next;
  VCEncIn encIn;
  /** pointer to coding ctrl parameters of this frame */
  EncCodingCtrlParam *pCodingCtrlParam;
  /** pointer to rate ctrl parameters of this frame */
  EncRateCtrlParam *pRateCtrlParam;
} VCEncJob;

/** information attached with each task */
typedef struct {
  /** counter to identify the input frame, same as display order counter */
  u32 picture_cnt;
  /** counter to record encode order */
  u32 encode_cnt;
  /** reference counter */
  u32 ref_cnt;

  /** aif information */
  VCEncAifInfo aif;
} VCEncInfo;

typedef struct {
  u32 tileLeft;   //based on ctb
  u32 tileRight;  //based on ctb
  u32 startTileIdx;

  /* Buffer addresses and offsets */
  u32 *pOutBuf;
  ptr_t busOutBuf;
  u32 outBufSize;
  int input_offset;

  /* HW job */
  int job_id;

  /* output */
  u32 sumOfQP;
  u32 sumOfQPNumber;
  u32 picComplexity;

  /* input YUV */
  u32 input_luma_stride;
  u32 input_chroma_stride;
  ptr_t inputLumBase;
  ptr_t inputCbBase;
  ptr_t inputCrBase;
  u32 inputChromaBaseOffset;
  u32 inputLumaBaseOffset;
  ptr_t stabNextLumaBase;

  /* cuinfo */
  ptr_t cuinfoTableBase;
  ptr_t cuinfoDataBase;

  u32 streamSize;
  u32 numNalu;

  u32 intraCu8Num;
  u32 skipCu8Num;
  u32 PBFrame4NRdCost;
  u32 SSEDivide256;
  u32 lumSSEDivide256;
  u32 cbSSEDivide64;
  u32 crSSEDivide64;
  //compress_coeff_scan_base

  i64 ssim_numerator_y;
  i64 ssim_numerator_u;
  i64 ssim_numerator_v;
  u32 ssim_denominator_y;
  u32 ssim_denominator_uv;

  // for CtbRc
  i32 ctbRcX0;
  i32 ctbRcX1;
  i32 ctbRcFrameMad;
  i32 ctbRcQpSum;
} TileCtrl;

typedef struct {
  /* cmd buffer ID, see waitJobid[];*/
  VcmdDes_t cmdbuf;
  /** control interrupt mode when generate JMP command
   * bit31 is mode_flag.
   *  - when mode_flag is 0, adapative interrupt mode is selected. in such
   *    mode, bit[30:0] is executing time estimated for current job;
   *  - when mode_flag is 1, manual interrupt mode is selected. in such mode,
   *    bit[0] is used to set IE flag in JMP command.
   *    bit[39:32] is batch count. */
  u64 interrupt_ctrl;
  /** priority of the job, normal=0, high/live=1 */
  u16 priority;
} VcmdJob;

struct vcenc_instance {
  void *ctx;
  u32 slice_idx;

  u32 encStatus;
  asicData_s asic;
  regValues_s *regs_bak;

  i32 vps_id; /* Video parameter set id */
  i32 sps_id; /* Sequence parameter set id */
  i32 pps_id; /* Picture parameter set id */
  i32 rps_id; /* Reference picture set id */

  struct buffer stream;
  struct buffer streams[MAX_CORE_NUM];

  EWLHwConfig_t featureToSupport;
  EWLHwConfig_t asic_core_cfg[MAX_SUPPORT_CORE_NUM];
  u32 asic_hw_id[MAX_SUPPORT_CORE_NUM];

  u32 reserve_core_info;

  u8 *temp_buffer; /* Data buffer, user set */
  u32 temp_size;   /* Size (bytes) of buffer */
  u32 temp_bufferBusAddress;

  // SPS&PPS parameters
  i32 max_cu_size;    /* Max coding unit size in pixels */
  i32 min_cu_size;    /* Min coding unit size in pixels */
  i32 max_tr_size;    /* Max transform size in pixels */
  i32 min_tr_size;    /* Min transform size in pixels */
  i32 tr_depth_intra; /* Max transform hierarchy depth */
  i32 tr_depth_inter; /* Max transform hierarchy depth */
  i32 width;
  i32 height;
  i32 ori_width;
  i32 ori_height;
  i32 enableScalingList;             /* */
  VCEncVideoCodecFormat codecFormat; /* Video Codec Format: HEVC/H264/AV1 */
  i32 pcm_enabled_flag;              /*enable pcm for HEVC*/
  i32 pcm_loop_filter_disabled_flag; /*pcm filter*/

  // intra setup
  u32 strong_intra_smoothing_enabled_flag;
  u32 constrained_intra_pred_flag;
  i32 enableDeblockOverride;
  i32 disableDeblocking;

  i32 tc_Offset;

  i32 beta_Offset;

  i32 ctbPerFrame;
  i32 ctbPerRow;
  i32 ctbPerCol;

  /* Minimum luma coding block size of coding units that convey
   * cu_qp_delta_abs and cu_qp_delta_sign and default quantization
   * parameter */
  i32 log2_qp_size;
  i32 qpHdr;

  i32 levelIdx;   /*level 5.1 =8*/
  i32 level;      /*level 5.1 =8*/
  i32 bAutoLevel; /* automatically calculate level */

  i32 profile; /**/
  i32 adaptive_profile; /* save adaptived profile if it exist, otherwise = -1 which use to */
                        /* update update adaptive profile before set_parameter() */
  i32 tier;

  preProcess_s preProcess;

  /* Rate control parameters */
  vcencRateControl_s rateControl;
  VCEncRateCtrl rateControl_ori;

  struct vps *vps;
  struct sps *sps;

  i32 poc;      /* encoded Picture order count */
  i32 frameNum; /* frame number in decoding order, 0 for IDR, +1 for each reference frame; used for H.264 */
  i32 frameNumExt; /* frame number in decoding order, 0 for IDR, +1 for each reference frame; used for H.264 */
  i32 idrPicId; /* idrPicId in H264, to distinguish subsequent idr pictures */
  u8 *lum;
  u8 *cb;
  u8 *cr;
  i32 chromaQpOffset;
  i32 enableSao;
  i32 output_buffer_over_flow;
  i32 encodeTimeout;
  u32 reEncodeCnt;
  u32 reEncodePreSize;
  const void *inst;

  i32 enableTS;
  i32 log2MaxTSBlockSizeMinus2;

  /* H.264 MMO */
  i32 h264_mmo_nops;
  i32 h264_mmo_unref[VCENC_MAX_REF_FRAMES];
  i32 h264_mmo_ltIdx[VCENC_MAX_REF_FRAMES];
  i32 h264_mmo_long_term_flag[VCENC_MAX_REF_FRAMES];
  i32 h264_mmo_unref_ext[VCENC_MAX_REF_FRAMES];
  i32 last_h264_mmo_nops;

  VCEncSliceReadyCallBackFunc sliceReadyCbFunc;
  VCEncTryNewParamsCallBackFunc cb_try_new_params;
  void *pAppData; /* User given application specific data */
  u32 frameCnt;
  u32 vuiVideoSignalTypePresentFlag;
  u32 vuiVideoFormat;
  u32 vuiVideoFullRange;
  u32 sarWidth;
  u32 sarHeight;
  i32 fieldOrder; /* 1=top first, 0=bottom first */
  u32 interlaced;
#ifdef VIDEOSTAB_ENABLED
  HWStabData vsHwData;
  SwStbData vsSwData;
#endif
  i32 gdrEnabled;
  i32 gdrStart;
  i32 gdrDuration;
  i32 gdrCount;
  i32 gdrAverageMBRows;
  i32 gdrMBLeft;
  i32 gdrFirstIntraFrame;
  u32 roi1Enable;
  u32 roi2Enable;
  u32 roi3Enable;
  u32 roi4Enable;
  u32 roi5Enable;
  u32 roi6Enable;
  u32 roi7Enable;
  u32 roi8Enable;
  u32 sse0Enable;
  u32 sse1Enable;
  u32 sse2Enable;
  u32 sse3Enable;
  u32 sse4Enable;
  u32 sse5Enable;
  u32 sse6Enable;
  u32 sse7Enable;
  u32 ctbRCEnable;
  i32 blockRCSize;
  u32 roiMapEnable;
  u32 RoimapCuCtrl_index_enable;
  u32 RoimapCuCtrl_enable; /* cu ctrl roi map enable (3 ~ 7 ) */
  u32 RoimapCuCtrl_ver;
  u32 RoiQpDelta_ver;
  u32 numNalus[MAX_CORE_NUM]; /* Amount of NAL units */
  u32 testId;
  i32 created_pic_num; /* number of pictures currently created */
  /* low latency */
  inputLineBuf_s inputLineBuf;
#if USE_TOP_CTRL_DENOISE
  unsigned int uiFrmNum;
  unsigned int uiNoiseReductionEnable;
  int FrmNoiseSigmaSmooth[5];
  int iFirstFrameSigma;
  int iNoiseL;
  int iSigmaCur;
  int iThreshSigmaCur;
  int iThreshSigmaPrev;
  int iSigmaCalcd;
  int iThreshSigmaCalcd;
  int iSliceQPPrev;
  // 3DNR
  int iSigmaYCur;
  int iSigmaUCur;
  int iSigmaVCur;

  int noiseSigmaEst;
  int noiseSigmaY;
  int noiseSigmaU;
  int noiseSigmaV;
  int noiseReductionStrength_IntraY;
  int noiseReductionStrength_IntraU;
  int noiseReductionStrength_IntraV;
  int noiseReductionStrength_InterY;
  int noiseReductionStrength_InterU;
  int noiseReductionStrength_InterV;
  int noiseReduction_ChromaMaxMV;
#endif
  i32 insertNewPPS;
  i32 insertNewPPSId;
  i32 maxPPSId;
  u32 maxTLayers; /*max temporal layers*/
  u32 rdoLevel;

  hashctx hashctx;

  /* for smart */
  i32 smartModeNoiseEn;
  i32 smartModeEnable;
  i32 smartH264Qp;
  i32 smartHevcLumQp;
  i32 smartHevcChrQp;
  i32 smartH264LumDcTh;
  i32 smartH264CbDcTh;
  i32 smartH264CrDcTh;
  /* threshold for hevc cu8x8/16x16/32x32 */
  i32 smartHevcLumDcTh[3];
  i32 smartHevcChrDcTh[3];
  i32 smartHevcLumAcNumTh[3];
  i32 smartHevcChrAcNumTh[3];
  /* back ground */
  i32 smartMeanTh[4];
  i32 smartMeanThPrev;
  /* foreground/background threashold: maximum foreground pixels in background block */
  i32 smartPixNumCntTh;
  /* number of frames from last recover frame */
  int smartRecoverGap;
  /** aif control */
  VCAifCtrl aif;

  /* for ctbRc Mode1 */
  u32 ctbRcSkinQPDelta;
  u32 ctbRcSkinMinQPDelta;
  u32 ctbRcSkinCbMin;
  u32 ctbRcSkinCbMax;
  u32 ctbRcSkinCrMin;
  u32 ctbRcSkinCrMax;
  u32 ctbRcSkinLumMin;
  u32 ctbRcSkinLumMax;
  u32 ctbRcSkinGradTh;
  u32 ctbRcTrailStrengthMax;
  u32 ctbRcTrailStrengthPrev;
  u32 ctbRcTrailStrength;
  i32 ctbRcTrailDeltaQp;
  u32 ctbRcEdgeTh;
  u32 ctbRcEdgeMaxQpDelta;
  u32 ctbRcDirection;
  u32 ctbRcThresholdI[16];
  u32 ctbRcThresholdP[16];
  u32 ctbRcThresholdB[16];
  u32 ctbRcThresholdCurFrame[16];

  /* visual quality prior mode decision */
  i32 visualBitRateTolerance;
  u32 IntraBiasChromaStrength;
  u32 IntraBiasStrengthMax;
  u32 IntraBiasStrength;
  u32 IntraBiasMvThreshold;

  /** has sideline job or not */
  u32 has_sideline;
  /** poc of sideline job */
  i32 sideline_poc;
  u32 bTrailAvoidIntraBias;

  u32 verbose;      /* Log printing mode */
  u32 dumpRegister; /**< enable dump register values for debug */
  u32 dumpCuInfo; /**< enable dump Cu Information after encoding one frame for debug */
  u32 dumpCtbBits; /**< enable dump Ctb encoded bits Information after encoding one frame for debug */

  /* for tile*/
  i32 tiles_enabled_flag;
  i32 num_tile_columns;
  i32 num_tile_rows;
  i32 loop_filter_across_tiles_enabled_flag;
  i32 tileMvConstraint;
  u16 *tileHeights;
  TileCtrl tileCtrl[HEVC_MAX_TILE_COLS];

  /* L2-cache */
  void *cache;
  u32 channel_idx;

  u32 input_alignment;
  u32 ref_alignment;
  u32 ref_ch_alignment;
  u32 aqInfoAlignment;
  u32 tileStrmSizeAlignmentExp;

  u32 compressor;
  u32 ctbRcMode;
  u32 outputCuInfo;
  u32 outputCtbBits;
  u32 numCtbBitsBuf;
  u32 frameInfo;
  u32 enablelumaInfo;

  /* AXIFE*/
  u32 axiFEEnable; /*0: axiFE disable   1:axiFE normal mode enable   2:axiFE bypass  3.security mode*/

  ITU_T_T35 itu_t_t35;
  u32 write_once_HDR10;
  Hdr10DisplaySei Hdr10Display;
  Hdr10LightLevelSei Hdr10LightLevel;
  VuiColorDescription vuiColorDescription;

  u32 RpsInSliceHeader;
  HWRPS_CONFIG sHwRps;

  bool rasterscan;

  /* cu tree look ahead queue */
  u32 pass;
  struct cuTreeCtr cuTreeCtl;
  bool bSkipCabacEnable;
  u32 lookaheadDepth;
  u32 idelay;
  u32 lookaheadDelay;
  VCEncLookahead lookahead;
  u32 numCuInfoBuf;
  u32 cuInfoBufIdx;
  /* 2 Type of motion score because of HW limit */
  /* Only if multiPass support, cuinfo version=2, mainly for 2pass */
  bool multiPassMotionScoreEnable;
  /* FUSE VIDEO_MotionScore: 2pass or single pass */
  bool motionScoreEnable;
  u32 extDSRatio;            /*0=1:1, 1=1:2*/
  void *cutreeJobBufferPool; /*cutree job buffer pool*/
  struct AGopInfo *gopInfoAddr[MAX_AGOPINFO_NUM];
  u32 nGopInfo;  //number of agopInfo

  /* Multi-core parallel ctr */
  struct sw_picture *pic[MAX_CORE_NUM];
  u32 parallelCoreNum;
  u32 readCnt;
  u32 jobCnt;

  /** enable multi-core encoding or not.
   * Note: in batch mode, cannot enable multi-core */
  u32 mcEnable;

#ifdef MULTI_FRAME_SUPPORT
  /** if enable batch mode encoding. in batch mode, software will aggragated some
   * jobs (e.g. encoding one frame is a job) and then drive the hardware in batch. */
  u32 batchEnable;
  /** the threshold to drive hardware when aggrated this count of jobs */
  u32 batchCount;
  /** jobs aggregated in software */
  VcmdJob vjobs[MAX_AGGREGATED_FRAMES];
  /** the number of job which has not send to hardware */
  u32 cacheCnt;
  /** the number of job has send to hardware */
  u32 sendCnt;
#endif

  u32 reservedCore;
  u32 maxReservedCoreNum;
  VCEncStrmBufs streamBufs[MAX_CORE_NUM]; /* output stream buffers */
  u32 waitJobid[MAX_CORE_NUM];
  VCEncOut EncOut[MAX_CORE_NUM];
  VCEncIn EncIn[MAX_CORE_NUM];
  VCEncAifInfo infos[MAX_CORE_NUM];
  VCEncLookaheadJob *lookaheadJob[MAX_CORE_NUM];
  encOutForCutree_s outForCutree;

  /* current information of processing job */
  VCEncAifInfo *info;

  /*stream Multi-segment ctrl*/
  streamMultiSeg_s streamMultiSegment;

  /* psy factor */
  float psyFactor;

#ifdef SUPPORT_AV1
  /* AV1 frame header */
  vcenc_instance_av1 av1_inst;
#endif

  u32 strmHeaderLen;

#ifdef SUPPORT_VP9
  /*VP9 inst*/
  vcenc_instance_vp9 vp9_inst;
#endif

  //for extra parameters.
  struct queue extraParaQ;

  u32 writeReconToDDR;

  u32 layerInRefIdc;
  u32 prefixNalSvcFlag;
  u32 svctEnable;
  u8 bRdoqLambdaAdjust;

  u32 bInputfileList;

  /*single pass manange encode order*/
  void *jobBufferPool;   /* buffer pool for job */
  struct queue jobQueue; /* job queue for reorder */
  VCEncIn encInFor1pass; /* maintain encode order information*/
  i32 gopSize;           /* gop size set by cmdline*/
  i32 gopMaxBSize;       /* the gop max number of B frames for agop set by cmdline*/
  i32 nextGopSize;       /* next gop size*/
  u32 pass2Agop4to1;
  u32 pass1CutGopto1;
  VCENCAdapGopCtr agop;
  VCEncPicConfig lastPicCfg;
  i32 enqueueJobNum; /* single pass enqueue job number */
  /* task has information of encoding frames which not done. */
  /** buffer pool to save task information */
  void *task_pool;
  /** queue to save task information */
  struct queue task_queue;

  /** encode order counter */
  u32 encode_cnt;

  /* */
  i8 bFlush;      /* flush flag, 1->flush*/
  i32 nextIdrCnt; /* next idr picture count*/
  i32 intraPeriod; /* non-IDR intra frame interval*/

  u8 enableTMVP; /* Enable tmvp */
  u32 numRefP; /* reference frame number of P frame, numRefP=1|2, set by user */

  //ncodeOrderMode_E encodeOrderMode; /* 0->init status; 1->encode in Frame input order; 2->adjust encode order inside API*/
  i32 bIOBufferBinding; /* if bind input buffer and output buffer 1->bind, 0 notbind */

  u8 flexRefsEnable;

  /*codingCtrl parameter*/
  EncCodingCtrl codingCtrl;

  /*rateCtrl parameter*/
  EncRateCtrl rateCtrl;

  //reference and recon common buffer
  u32 RefRingBufExtendHeight;
  u8 refRingBufEnable;

  i32 currentActualBitRate;

  double qpfactorSSE;

  u32 intraReconEnable;

  /* NLM */
  struct sw_picture *lastPicNLM; /* record last pic for I frame NLM */

  /* leading pictures */
  u32 enableLeadingPictures; /* enable leading pictures encoding */
  u32 outGopSize; /* gopSize for current encoded frame */
  /* poll input sliceinfo */
  u32 inputSliceInfoPollEn;

  ufbc_param ufbcParam;

  /* sw skip frame*/
  u32 sw_skip_flag;
  /** \brief send new SPS on FPS change or not
   * \n 0 = not send SPS
   * \n 1 = send SPS */
  u32 bSendSPSonFPSAdjust;
  /** \brief insert IDR on FPS change or not
   * \n 0 = not insert IDR
   * \n 1 = insert IDR */
  u32 bInsertIDRonFPSAdjust;
  /* for H264 DPB management on RC overflow */
  i32 skipGopPoc;
  u32 skipGopRpsId;
  u32 preSkipGop;
};

struct instance {
  struct vcenc_instance vcenc_instance; /* Visible to user */
  struct vcenc_instance *instance;      /* Instance sanity check */
  struct container container;           /* Encoder internal store */
};

/** \brief A linked list linear data structure which stores encoder buffer information. */
struct vcenc_buffer {
  struct vcenc_buffer *next;
  u8 *buffer;    /**< \brief Data store */
  u32 cnt;       /**< \brief Data byte cnt */
  ptr_t busaddr; /**< \brief Data store bus address */
};

struct container *get_container(struct vcenc_instance *instance);

#endif
