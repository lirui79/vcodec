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

#ifndef _ENC_VIDEO_PRIV_H_
#define _ENC_VIDEO_PRIV_H_

#include <stdint.h>
#include "hevcencapi.h"
#include "enccfg.h"
#include "encoder.h"

#ifdef __cplusplus
extern  "C" {
#endif

#define MOVING_AVERAGE_FRAMES 120
#define LEAST_MONITOR_FRAME 3
#define MAX_GOP_LEN 300

#define MAX_LINE_LENGTH_BLOCK (512*8)
#define CTBRC_THRESHOLD_NUM (16)

// #define CONV_TABLE_SIZE(a) sizeof(a) / sizeof(vsi_omx_def_conv)
// typedef struct vsi_omx_def_conv{
//   uint32_t omx_val;
//   uint32_t vsi_val;
// } vsi_omx_def_conv;

/* Structure for command line options */
typedef struct VCE_VIDEO_OPTIONS
{
  i32 outputRateNumer;
  i32 outputRateDenom;
  i32 inputRateNumer;
  i32 inputRateDenom;
  i32 firstPic;
  i32 lastPic;
  i32 width;
  i32 height;
  i32 lumWidthSrc;
  i32 lumHeightSrc;
  VCEncPictureType inputFormat;
  VCEncStreamType byteStream;
//  i32 videoStab;
//  VCEncVideoCodecFormat codecFormat;
  i32 enableCabac;
  i32 bCabacInitFlag;
  u32 strong_intra_smoothing_enabled_flag;
  u32 pcm_loop_filter_disabled_flag;
  u32 cirStart;
  u32 cirInterval;

  VCEncPictureArea intraArea;
  VCEncPictureArea ipcmArea[MAX_IPCM_AREA];
  VCEncPictureArea roiArea[MAX_ROI_AREA];
  i32 roiDeltaQp[MAX_ROI_AREA];
  i32 roiQp[MAX_ROI_AREA];
  VCEncPictureArea rect[MAX_ROI_AREA];

  i32 ipcmMapEnable;
  // i32 skipMapEnable;
  // i32 skipMapBlockUnit;
  i32 rdoqMapEnable;
  true_e fillerData;
  i32 hrdConformance;
  i32 cpbSize;
  i32 intraPicRate;
  i32 vbr;
  i32 qpHdr;
  i32 nQpMin;
  i32 nQpMax;
  i32 qpMinI;
  i32 qpMaxI;

  i32 qpHdrI;
  i32 qpHdrP;
  i32 qpHdrB;

  i32 bitPerSecond;
  i32 cpbMaxRate;
  i32 crf;
  rcMode_e rcMode;
  i32 bitVarRangeI;
  i32 bitVarRangeP;
  i32 bitVarRangeB;
  u32 u32StaticSceneIbitPercent;
  i32 tolMovingBitRate;
  i32 monitorFrames;
  i32 picRc;
  i32 ctbRcMode;
  i32 blockRCSize;
  u32 rcQpDeltaRange;
  u32 rcBaseMBComplexity;
  i32 picSkip;
  i32 picQpDeltaMin;
  i32 picQpDeltaMax;
  i32 ctbRcRowQpStep;
  float tolCtbRcInter;
  float tolCtbRcIntra;
  i32 bitrateWindow;
  i32 intraQpDelta;
  i32 fixedIntraQp;
  i32 bFrameQpDelta;
  i32 bDisableDeblocking;
  i32 bEnableSAO;
  i32 tc_Offset;
  i32 beta_Offset;
  i32 chromaQpOffset;
  VCEncProfile profile;
  VCEncTier tier;
  VCEncLevel level;
  i32 smoothPsnrInGOP;
  i32 sliceSize;
  i32 rotation;
  i32 mirror;
  i32 horOffsetSrc;
  i32 verOffsetSrc;
  VCEncColorConversionType colorConversion;
  i32 scaledWidth;
  i32 scaledHeight;
  i32 scaledOutputFormat;
  i32 enableDeblockOverride;
  i32 deblockOverride;
  i32 enableScalingList;
  u32 compressor;
  i32 interlacedFrame;
  i32 fieldOrder;
  i32 videoRange;
  i32 ssim;
  i32 psnr;
  i32 bSeiMessages;
  u32 gopSize;
  char *gopCfg;
  u32 gopLowdelay;
  u32 lowDelayB;
  u32 longTermGap;
  u32 longTermGapOffset;
  i32 ltrInterval;
  i32 longTermQpDelta;
  i32 gdrDuration;
  u32 roiMapDeltaQpBlockUnit;
  u32 roiMapDeltaQpEnable;
  u32 RoiCuCtrlVer;
  u32 RoiQpDeltaVer;
  i32 noiseReductionEnable;
  i32 noiseLow;
  i32 firstFrameSigma;
  i32 bitDepthLuma;
  i32 bitDepthChroma;
  u32 enableOutputCuInfo;
  u32 enableOutputCtbBits;
  u32 dynamicRdoEnable;
  u32 dynamicRdoCu16Bias;
  u32 dynamicRdoCu16Factor;
  u32 dynamicRdoCu32Bias;
  u32 dynamicRdoCu32Factor;
  i32 inputLineBufMode;
  i32 inputLineBufDepth;
  i32 amountPerLoopBack;
  i32 segmentUnitHeight;
  u32 hashtype;
  u32 verbose;
  i32 smartModeEnable;
  i32 smartH264Qp;
  i32 smartHevcLumQp;
  i32 smartHevcChrQp;
  i32 smartH264LumDcTh;
  i32 smartH264CbDcTh;
  i32 smartH264CrDcTh;
  i32 smartHevcLumDcTh[3];
  i32 smartHevcChrDcTh[3];
  i32 smartHevcLumAcNumTh[3];
  i32 smartHevcChrAcNumTh[3];
  i32 smartMeanTh[4];
  i32 smartPixNumCntTh;
  i32 constChromaEn;
  u32 constCb;
  u32 constCr;
  i32 tiles_enabled_flag;
  i32 num_tile_columns;
  i32 num_tile_rows;
  i32 loop_filter_across_tiles_enabled_flag;
  i32 tileMvConstraint;
  i32 skip_frame_enabled_flag;
  i32 skip_frame_poc;
  u32 exp_of_input_alignment;
  u32 exp_of_ref_alignment;
  u32 exp_of_ref_ch_alignment;
  u32 exp_of_aqinfo_alignment;
  u32 exp_of_tile_stream_alignment;
  u32 itu_t_t35_enable;
  u32 itu_t_t35_country_code;
  u32 itu_t_t35_country_code_extension_byte;
  u32 write_once_HDR10;
  u32 hdr10_display_enable;
  u32 hdr10_dx0;
  u32 hdr10_dy0;
  u32 hdr10_dx1;
  u32 hdr10_dy1;
  u32 hdr10_dx2;
  u32 hdr10_dy2;
  u32 hdr10_wx;
  u32 hdr10_wy;
  u32 hdr10_maxluma;
  u32 hdr10_minluma;
  u32 hdr10_lightlevel_enable;
  u32 hdr10_avglight;
  u32 hdr10_maxlight;
  u32 vuiColorDescripPresentFlag;
  u32 vuiColorPrimaries;
  u32 vuiTransferCharacteristics;
  u32 vuiMatrixCoefficients;
  u32 vuiVideoFormat;
  u32 vuiVideoSignalTypePresentFlag;
  u32 vuiAspectRatioEnable;
  u32 vuiAspectRatioWidth;
  u32 vuiAspectRatioHeight;
  u32 RpsInSliceHeader;
  u32 P010RefEnable;
  u32 vui_timing_info_enable;
  u32 picOrderCntType;
  u32 log2MaxPicOrderCntLsb;
  u32 log2MaxFrameNum;
  u32 parallelCoreNum;
  u32 dumpRegister;
  u32 rasterscan;
  u32 lookaheadDepth;
  u32 streamMultiSegmentMode;
  u32 streamMultiSegmentAmount;
  i32 cuInfoVersion;
  u32 enableRdoQuant;
  u64 AXIAlignment;
  u32 irqTypeMask;
  u32 irqTypeCutreeMask;
  u32 sliceNode;
  u32 MEVertRange;
  u32 mosaicEnables;
  u32 mosXoffset[MAX_MOSAIC_NUM];
  u32 mosYoffset[MAX_MOSAIC_NUM];
  u32 mosWidth[MAX_MOSAIC_NUM];
  u32 mosHeight[MAX_MOSAIC_NUM];
  VCEncChromaIdcType codedChromaIdc; // 0: 400, 1: 420, 2: 422
  i32 aq_mode;
  float aq_strength;
  float psyFactor;
  u32 writeReconToDDR;
  u32 TxTypeSearchEnable;
  u32 av1InterFiltSwitch;
  u32 tune;
  u32 resendParamSet;
  u32 sendAUD;
  u32 sramPowerdownDisable;
  u32 insertIDR;
  u32 burstMaxLength;
  i32 enableTMVP;
  u32 refRingBufEnable;

  i32 max_cu_size;                   /* Max coding unit size in pixels */
  i32 min_cu_size;                   /* Min coding unit size in pixels */
  i32 max_tr_size;                   /* Max transform size in pixels */
  i32 min_tr_size;                   /* Min transform size in pixels */
  i32 tr_depth_intra;                /* Max transform hierarchy depth */
  i32 tr_depth_inter;                /* Max transform hierarchy depth */
  u32 layerInRefIdc;
  u32 prefixNalSvcFlag;
  u32 rdoLevel;
  u32 svctEnable;

  u32 extSramLumHeightBwd;
  u32 extSramChrHeightBwd;
  u32 extSramLumHeightFwd;
  u32 extSramChrHeightFwd;

  u32 refFrameAmount;
  u32 force_idr;

  u32 inLoopDSRatio;
  u32 sbi_id_0;
  u32 sbi_id_1;
  u32 sbi_id_2;

  u32 maxIprop;
  u32 minIprop;
  i32 changePos;

  u32 preset;         /* 0..4 for HEVC. 0..1 for H264. Trade off performance and compression efficiency */

  u32 coreMask;
  u32 aifEnable;
  u32 batchEnable;
  u32 batchCount;
  i16 gmv[2][2];
} VCE_VIDEO_OPTIONS;

typedef struct {
  VCEncProfile profile;
  VCEncLevel level;
  i32 max_cu_size;                   /* Max coding unit size in pixels */
  i32 min_cu_size;                   /* Min coding unit size in pixels */
  i32 max_tr_size;                   /* Max transform size in pixels */
  i32 min_tr_size;                   /* Min transform size in pixels */
  i32 tr_depth_intra;                /* Max transform hierarchy depth */
  i32 tr_depth_inter;                /* Max transform hierarchy depth */
  u32 layerInRefIdc;
  u32 prefixNalSvcFlag;
  u32 rdoLevel;
  u32 svctEnable;

  u32 extSramLumHeightBwd;
  u32 extSramChrHeightBwd;
  u32 extSramLumHeightFwd;
  u32 extSramChrHeightFwd;
} vce_format_options;

typedef struct {
  i32 frame[MOVING_AVERAGE_FRAMES];
  i32 length;
  i32 count;
  i32 pos;
  i32 outputRateNumer;
  i32 outputRateDenom;
} ma_s;

typedef struct {
  i32 gop_frm_num;
  double sum_intra_vs_interskip;
  double sum_skip_vs_interskip;
  double sum_intra_vs_interskipP;
  double sum_intra_vs_interskipB;
  i32 sum_costP;
  i32 sum_costB;
  i32 last_gopsize;
} adapGopCtr;

typedef struct ENCODER_CODEC
{
  ENCODER_PROTOTYPE base;

  VCEncVideoCodecFormat codecFormat;
  VCEncInst instance;
  VCEncIn encIn;
  VCEncOut encOut;

  i32 nPictureRcEnabled;
  u32 eRateControl;

  u32 nEstTimeInc;
  u32 nIFrameCounter;
  u32 nRefFrames;
  u32 nPFrames;
  u32 nBFrames;
  u32 nTotalFrames;
  u32 nMaxTLayers;

  VCEncGopPicConfig gopPicCfg[MAX_GOP_PIC_CONFIG_NUM];
  VCEncGopPicConfig gopPicCfgPass2[MAX_GOP_PIC_CONFIG_NUM];
  VCEncGopPicSpecialConfig gopPicSpecialCfg[MAX_GOP_SPIC_CONFIG_NUM];
  u32 maxRefPics;
  u32 maxTemporalId;

  i32 nextGopSize;
  VCEncPictureCodingType nextCodingType;

  VCE_VIDEO_OPTIONS options;

  u32 validencodedframenumber;
  ma_s ma;
  adapGopCtr agop;

  u32 currBitrate;
  const void *ewlInst;
  const EWLHwConfig_t *hwCfg; /* HW fuse */
  i32 *tile_width;
  i32 *tile_height;
} ENCODER_CODEC;

#ifdef __cplusplus
}
#endif
#endif /* _ENC_VIDEO_PRIV_H_ */


