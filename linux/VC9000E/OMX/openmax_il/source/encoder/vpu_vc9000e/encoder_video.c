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

#include <string.h>
#include <stdio.h>
#include <math.h>

#include "encoder.h"
#include "encoder_video.h"
#include "enc_video_priv.h"
#include "util.h"
#include "OSAL.h"
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX CODEC"

#if !defined (ENCVC9000E)
#error "SPECIFY AN ENCODER PRODUCT (ENCVC9000E) IN COMPILER DEFINES!"
#endif

vce_format_options vce_opts_h264 = {
  .profile = VCENC_H264_HIGH_PROFILE,
  .level = VCENC_H264_LEVEL_6_2,
  .max_cu_size = 16,
  .min_cu_size = 8,
  .max_tr_size = 16,
  .min_tr_size = 4,
  .tr_depth_intra = 1,
  .tr_depth_inter = 2,
  .layerInRefIdc = 0,
  .prefixNalSvcFlag = 0,
  .rdoLevel = 1,
  .svctEnable = 0,

  .extSramLumHeightBwd = 12,
  .extSramChrHeightBwd = 6,
  .extSramLumHeightFwd = 12,
  .extSramChrHeightFwd = 6,
};

vce_format_options vce_opts_hevc = {
  .profile = VCENC_HEVC_MAIN_PROFILE,
  .level = VCENC_HEVC_LEVEL_6_2,
  .max_cu_size = 64,
  .min_cu_size = 8,
  .max_tr_size = 16,
  .min_tr_size = 4,
  .tr_depth_intra = 2,
  .tr_depth_inter = 4,
  .layerInRefIdc = 0,
  .prefixNalSvcFlag = 0,
  .rdoLevel = 3,
  .svctEnable = 0,

  .extSramLumHeightBwd = 16,
  .extSramChrHeightBwd = 8,
  .extSramLumHeightFwd = 16,
  .extSramChrHeightFwd = 8,
};
#if 0
vce_format_options vce_opts_vp9 = {
  .profile = VCENC_VP9_MAIN_PROFILE,
  .level = 9,
  .max_cu_size = 64,
  .min_cu_size = 8,
  .max_tr_size = 16,
  .min_tr_size = 4,
  .tr_depth_intra = 2,
  .tr_depth_inter = 4,
  .layerInRefIdc = 0,
  .prefixNalSvcFlag = 0,
  .rdoLevel = 3,
  .svctEnable = 0,

  .extSramLumHeightBwd = 0,
  .extSramChrHeightBwd = 0,
  .extSramLumHeightFwd = 0,
  .extSramChrHeightFwd = 0,
};
#endif
vce_format_options vce_opts_av1 = {
  .profile = VCENC_AV1_MAIN_PROFILE,
  .level = 13,
  .max_cu_size = 64,
  .min_cu_size = 8,
  .max_tr_size = 16,
  .min_tr_size = 4,
  .tr_depth_intra = 2,
  .tr_depth_inter = 4,
  .layerInRefIdc = 0,
  .prefixNalSvcFlag = 0,
  .rdoLevel = 3,
  .svctEnable = 0,

  .extSramLumHeightBwd = 0,
  .extSramChrHeightBwd = 0,
  .extSramLumHeightFwd = 0,
  .extSramChrHeightFwd = 0,
};

VCE_VIDEO_OPTIONS vce_default_opts_video = {
  .inputRateNumer = 30,
  .inputRateDenom = 1,
  .outputRateNumer = 30,
  .outputRateDenom = 1,
  .firstPic = 0,
  .lastPic = 0x7fffffff,
  .width = DEFAULT,
  .height = DEFAULT,
  .lumWidthSrc = DEFAULT,
  .lumHeightSrc = DEFAULT,
  .inputFormat = VCENC_YUV420_PLANAR,
  .byteStream = 1,
//  .videoStab = 0,
  .enableCabac = 1,
  .bCabacInitFlag = 0,
  .strong_intra_smoothing_enabled_flag = 0,
  .pcm_loop_filter_disabled_flag = 0,
  .cirStart = 0,
  .cirInterval = 0,
  .intraArea = {0, -1, -1, -1, -1},
  .ipcmArea = {{0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1},
               {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}},
  .roiArea = {{0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1},
              {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}, {0, -1, -1, -1, -1}},
  .roiDeltaQp = {0, 0, 0, 0, 0, 0, 0, 0},
  .roiQp = {-255, -255, -255, -255, -255, -255, -255, -255},
  .rect = {{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
           {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}},

  .ipcmMapEnable = 0,
//  .skipMapEnable = 0,
//  .skipMapBlockUnit = 0,
  .rdoqMapEnable = 0,

  .fillerData = 0,
  .hrdConformance = 0,
  .cpbSize = -1,
  .intraPicRate = 0,
  .vbr = 0,
  .qpHdr = -1,
  .nQpMin = 0,
  .nQpMax = 51,
  .qpMinI = -1,
  .qpMaxI = -1,
  .bitPerSecond = 1000000,
  .cpbMaxRate = 0,
  .crf = -1,
  .rcMode = VCE_RC_CVBR,
  .bitVarRangeI = 10000,
  .bitVarRangeP = 10000,
  .bitVarRangeB = 10000,
  .u32StaticSceneIbitPercent = 80,
  .tolMovingBitRate = 100,
  .monitorFrames = DEFAULT,
  .picRc = 0,
  .ctbRcMode = 0,
  .blockRCSize = 0,
  .rcQpDeltaRange = 10,
  .rcBaseMBComplexity = 15,
  .picSkip = 0,
  .picQpDeltaMin = DEFAULT,
  .picQpDeltaMax = DEFAULT,
  .ctbRcRowQpStep = DEFAULT,
  .tolCtbRcInter = DEFAULT,
  .tolCtbRcIntra = DEFAULT,
  .bitrateWindow = DEFAULT,
  .intraQpDelta = DEFAULT,
  .fixedIntraQp = 0,
  .bFrameQpDelta = -1,
  .bDisableDeblocking = 0,
  .bEnableSAO = 1,
  .tc_Offset = 0,
  .beta_Offset = 0,
  .chromaQpOffset = 0,
  .tier = VCENC_HEVC_MAIN_TIER,
  .level = VCENC_AUTO_LEVEL,
  .smoothPsnrInGOP = 0,
  .sliceSize = 0,
  .rotation = 0,
  .mirror = 0,
  .horOffsetSrc = DEFAULT,
  .verOffsetSrc = DEFAULT,
  .colorConversion = VCENC_RGBTOYUV_BT601,
  .scaledWidth = 0,
  .scaledHeight = 0,
  .scaledOutputFormat = 0,
  .enableDeblockOverride = 0,
  .deblockOverride = 0,
  .enableScalingList = 0,
  .compressor = 0,
  .interlacedFrame = 0,
  .fieldOrder = 0,
  .videoRange = 0,
  .ssim = 1,
  .psnr = 1,
  .bSeiMessages = 0,
  .gopSize = 0,
  .gopCfg = NULL,
  .gopLowdelay = 0,
  .longTermGap = 0,
  .longTermGapOffset = 0,
  .ltrInterval = DEFAULT,
  .longTermQpDelta = 0,
  .gdrDuration = 0,
  .roiMapDeltaQpBlockUnit = 0,
  .roiMapDeltaQpEnable = 0,
  .RoiCuCtrlVer = 0,
  .RoiQpDeltaVer = 1,
  .layerInRefIdc = 0,
  .prefixNalSvcFlag = 0,
  .svctEnable = 0,
  .noiseReductionEnable = 0,
  .noiseLow = 10,
  .firstFrameSigma = 11,
  .bitDepthLuma = 8,
  .bitDepthChroma = 8,
  .enableOutputCuInfo = 0,
  .enableOutputCtbBits = 0,
  .dynamicRdoEnable = 0,
  .dynamicRdoCu16Bias = 3,
  .dynamicRdoCu16Factor = 80,
  .dynamicRdoCu32Bias = 2,
  .dynamicRdoCu32Factor = 32,
  .inputLineBufMode = 0,
  .inputLineBufDepth = 1,
  .amountPerLoopBack = 0,
  .segmentUnitHeight = 16,
  .hashtype = 0,
  .verbose = 0,
  .smartModeEnable = 0,
  .smartH264Qp = 30,
  .smartHevcLumQp = 30,
  .smartHevcChrQp = 30,
  .smartH264LumDcTh = 5,
  .smartH264CbDcTh = 1,
  .smartH264CrDcTh = 1,
  .smartHevcLumDcTh = {2, 2, 2},
  .smartHevcChrDcTh = {2, 2, 2},
  .smartHevcLumAcNumTh = {12, 51, 204},
  .smartHevcChrAcNumTh = {3, 12, 51},
  .smartMeanTh = {5 , 5, 5, 5},
  .smartPixNumCntTh = 0,
  .constChromaEn = 0,
  .constCb = DEFAULT,
  .constCr = DEFAULT,
  .tiles_enabled_flag = 0,
  .num_tile_columns = 1,
  .num_tile_rows = 1,
  .loop_filter_across_tiles_enabled_flag = 1,
  .tileMvConstraint = 0,
  .skip_frame_enabled_flag = 0,
  .skip_frame_poc = 0,
#ifdef RECON_REF_1KB_BURST_RW
  .exp_of_input_alignment = 10,
  .exp_of_ref_alignment = 10,
  .exp_of_ref_ch_alignment = 10,
#else
  .exp_of_input_alignment = 6,
  .exp_of_ref_alignment = 6,
  .exp_of_ref_ch_alignment = 6,
#endif
  .exp_of_aqinfo_alignment = 6,
  .exp_of_tile_stream_alignment = 0,
  .itu_t_t35_enable = 0,
  .itu_t_t35_country_code = 0,
  .itu_t_t35_country_code_extension_byte =0,
  .write_once_HDR10 = 0,
  .hdr10_display_enable = 0,
  .hdr10_dx0 = 0,
  .hdr10_dy0 = 0,
  .hdr10_dx1 = 0,
  .hdr10_dy1 = 0,
  .hdr10_dx2 = 0,
  .hdr10_dy2 = 0,
  .hdr10_wx = 0,
  .hdr10_wy = 0,
  .hdr10_maxluma = 0,
  .hdr10_minluma = 0,
  .hdr10_lightlevel_enable = 0,
  .hdr10_avglight = 0,
  .hdr10_maxlight = 0,
  .vuiColorDescripPresentFlag = 0,
  .vuiColorPrimaries = 2,
  .vuiTransferCharacteristics = 2,
  .vuiMatrixCoefficients = 2,
  .vuiVideoFormat = 5,
  .vuiVideoSignalTypePresentFlag = 0,
  .vuiAspectRatioEnable = 0,
  .vuiAspectRatioWidth = 0,
  .vuiAspectRatioHeight = 0,
  .RpsInSliceHeader = 0,
  .P010RefEnable = 0,
  .vui_timing_info_enable = 1,
  .picOrderCntType = 0,
  .log2MaxPicOrderCntLsb = 16,
  .log2MaxFrameNum = 12,
  .parallelCoreNum = 1,
  .dumpRegister = 0,
  .rasterscan = 0,
  .lookaheadDepth = 0,
  .streamMultiSegmentMode = 0,
  .streamMultiSegmentAmount = 4,
  .cuInfoVersion = -1,
  .enableRdoQuant = 0,
  .AXIAlignment = -1,
  .irqTypeMask = 0x01f0,
  .irqTypeCutreeMask = 0x01f0,
  .sliceNode = 0,
  .MEVertRange = 0,
  .mosaicEnables = 0,
  .mosXoffset = { 0 },
  .mosYoffset = { 0 },
  .mosWidth = { 0 },
  .mosHeight = { 0 },
  .codedChromaIdc = VCENC_CHROMA_IDC_420,
  .aq_mode = DEFAULT,
  .aq_strength = DEFAULT,
  .psyFactor = DEFAULT,
  .writeReconToDDR = 1,
  .TxTypeSearchEnable = 0,
  .av1InterFiltSwitch = 0,
  .tune = VCENC_TUNE_PSNR,
  .resendParamSet = 0,
  .sendAUD = 0,
  .sramPowerdownDisable = 0,
  .insertIDR = 0,
  .burstMaxLength = ENCH2_DEFAULT_BURST_LENGTH,
  .enableTMVP = DEFAULT,
  .refRingBufEnable = 0,
  .inLoopDSRatio = 0,
  .sbi_id_0 = 0,
  .sbi_id_1 = 1,
  .sbi_id_2 = 2,
  .maxIprop = 100,
  .minIprop = 1,
  .changePos = 90,

  .preset = DEFAULT,
  .coreMask = 0,
  .aifEnable = 0,
  .batchEnable = 0,
  .batchCount = 0,
  .gmv = {{0}},
};

u32 ctbRcThresholdI[CTBRC_THRESHOLD_NUM] = {0, 0, 0, 0, 3, 3, 5, 5, 6, 6, 6, 11, 11, 13, 17, 17};
u32 ctbRcThresholdP[CTBRC_THRESHOLD_NUM] = {0, 0, 0, 0, 3, 3, 5, 5, 6, 6, 6, 11, 11, 13, 17, 17};
u32 ctbRcThresholdB[CTBRC_THRESHOLD_NUM] = {0, 0, 0, 0, 3, 3, 5, 5, 6, 6, 6, 11, 11, 13, 17, 17};

//           Type POC QPoffset  QPfactor  num_ref_pics ref_pics  used_by_cur
char *RpsDefault_GOPSize_1[] = {
    "Frame1:  P    1   0        0.578     0      1        -1         1",
    NULL,
};

char *RpsDefault_V60_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8     0      1        -1         1",
    NULL,
};

char *RpsDefault_H264_GOPSize_1[] = {
    "Frame1:  P    1   0        0.4     0      1        -1         1",
    NULL,
};

char *RpsDefault_GOPSize_2[] = {
    "Frame1:  P        2   0        0.6     0      1        -2         1",
    "Frame2:  nrefB    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

char *RpsDefault_GOPSize_3[] = {
    "Frame1:  P        3   0        0.5     0      1        -3         1   ",
    "Frame2:  B        1   0        0.5     0      2        -1 2       1 1 ",
    "Frame3:  nrefB    2   0        0.68    0      2        -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_4[] = {
    "Frame1:  P        4   0        0.5      0     1       -4         1 ",
    "Frame2:  B        2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  nrefB    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  nrefB    3   0        0.5      0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_5[] = {
    "Frame1:  P        5   0        0.442    0     1       -5         1 ",
    "Frame2:  B        2   0        0.3536   0     2       -2 3       1 1",
    "Frame3:  nrefB    1   0        0.68     0     3       -1 1 4     1 1 0",
    "Frame4:  B        3   0        0.3536   0     2       -1 2       1 1 ",
    "Frame5:  nrefB    4   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_6[] = {
    "Frame1:  P        6   0        0.442    0     1       -6         1 ",
    "Frame2:  B        3   0        0.3536   0     2       -3 3       1 1",
    "Frame3:  B        1   0        0.3536   0     3       -1 2 5     1 1 0",
    "Frame4:  nrefB    2   0        0.68     0     3       -1 1 4     1 1 0",
    "Frame5:  B        4   0        0.3536   0     2       -1 2       1 1 ",
    "Frame6:  nrefB    5   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_7[] = {
    "Frame1:  P        7   0        0.442    0     1       -7         1 ",
    "Frame2:  B        3   0        0.3536   0     2       -3 4       1 1",
    "Frame3:  B        1   0        0.3536   0     3       -1 2 6     1 1 0",
    "Frame4:  nrefB    2   0        0.68     0     3       -1 1 5     1 1 0",
    "Frame5:  B        5   0        0.3536   0     2       -2 2       1 1 ",
    "Frame6:  nrefB    4   0        0.68     0     3       -1 1 3     1 1 0",
    "Frame7:  nrefB    6   0        0.68     0     2       -1 1       1 1 ",
    NULL,
};

char *RpsDefault_GOPSize_8[] = {
    "Frame1:  P        8   0        0.442    0  1           -8        1 ",
    "Frame2:  B        4   0        0.3536   0  2           -4 4      1 1 ",
    "Frame3:  B        2   0        0.3536   0  3           -2 2 6    1 1 0 ",
    "Frame4:  nrefB    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  nrefB    3   0        0.68     0  3           -1 1 5    1 1 0",
    "Frame6:  B        6   0        0.3536   0  2           -2 2      1 1",
    "Frame7:  nrefB    5   0        0.68     0  3           -1 1 3    1 1 0",
    "Frame8:  nrefB    7   0        0.68     0  2           -1 1      1 1",
    NULL,
};

char *RpsDefault_GOPSize_16[] = {
    "Frame1:  P       16   0        0.6      0  1           -16                   1",
    "Frame2:  B        8   0        0.2      0  2           -8   8                1   1",
    "Frame3:  B        4   0        0.33     0  3           -4   4  12            1   1   0",
    "Frame4:  B        2   0        0.33     0  4           -2   2   6  14        1   1   0   0",
    "Frame5:  nrefB    1   0        0.4      0  5           -1   1   3   7  15    1   1   0   0   0",
    "Frame6:  nrefB    3   0        0.4      0  4           -1   1   5  13        1   1   0   0",
    "Frame7:  B        6   0        0.33     0  3           -2   2  10            1   1   0",
    "Frame8:  nrefB    5   0        0.4      0  4           -1   1   3  11        1   1   0   0",
    "Frame9:  nrefB    7   0        0.4      0  3           -1   1   9            1   1   0",
    "Frame10: B       12   0        0.33     0  2           -4   4                1   1",
    "Frame11: B       10   0        0.33     0  3           -2   2   6            1   1   0",
    "Frame12: nrefB    9   0        0.4      0  4           -1   1   3   7        1   1   0   0",
    "Frame13: nrefB   11   0        0.4      0  3           -1   1   5            1   1   0",
    "Frame14: B       14   0        0.33     0  2           -2   2                1   1",
    "Frame15: nrefB   13   0        0.4      0  3           -1   1   3            1   1   0",
    "Frame16: nrefB   15   0        0.4      0  2           -1   1                1   1",
    NULL,
};

char *RpsDefault_Interlace_GOPSize_1[] = {
    "Frame1:  P    1   0        0.8       0   2           -1 -2     0 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_1[] = {
    "Frame1:  B    1   0        0.65      0     2       -1 -2         1 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_2[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -3         1 1",
    "Frame2:  B    2   0        0.578     0     2       -1 -2         1 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_3[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -4         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.578     0     2       -1 -3         1 1",
    NULL,
};

char *RpsLowdelayDefault_GOPSize_4[] = {
    "Frame1:  B    1   0        0.4624    0     2       -1 -5         1 1",
    "Frame2:  B    2   0        0.4624    0     2       -1 -2         1 1",
    "Frame3:  B    3   0        0.4624    0     2       -1 -3         1 1",
    "Frame4:  B    4   0        0.578     0     2       -1 -4         1 1",
    NULL,
};

char *RpsPass2_GOPSize_1[] = {
    "Frame1:  B        1   0        0.6     0      2        -1 -2       1 1",
    NULL,
};

char *RpsPass2_GOPSize_2[] = {
    "Frame1:  B        2   0        0.6     0      2        -2 -4      1 1",
    "Frame2:  nrefB    1   0        0.68    0      2        -1 1       1 1",
    NULL,
};

char *RpsPass2_GOPSize_3[] = {
    "Frame1:  B        3   0        0.5     0      2        -3 -6      1 1 ",
    "Frame2:  B        1   0        0.5     0      2        -1 2       1 1 ",
    "Frame3:  nrefB    2   0        0.68    0      3        -1 -2 1    1 0 1 ",
    NULL,
};

char *RpsPass2_GOPSize_4[] = {
    "Frame1:  B        4   0        0.5      0     2       -4 -8      1 1",
    "Frame2:  B        2   0        0.3536   0     2       -2 2       1 1",
    "Frame3:  nrefB    1   0        0.5      0     3       -1 1 3     1 1 0",
    "Frame4:  nrefB    3   0        0.5      0     3       -1 -3 1    1 0 1",
    NULL,
};

char *RpsPass2_GOPSize_5[] = {
    "Frame1:  B        5   0        0.442    0     2       -5 -10     1 1",
    "Frame2:  B        2   0        0.3536   0     2       -2 3       1 1",
    "Frame3:  nrefB    1   0        0.68     0     3       -1 1 4     1 1 0",
    "Frame4:  B        3   0        0.3536   0     3       -1 -3 2    1 0 1",
    "Frame5:  nrefB    4   0        0.68     0     3       -1 -4 1    1 0 1",
    NULL,
};

char *RpsPass2_GOPSize_6[] = {
    "Frame1:  B        6   0        0.442    0     2       -6 -12     1 1",
    "Frame2:  B        3   0        0.3536   0     2       -3 3       1 1",
    "Frame3:  B        1   0        0.3536   0     3       -1 2 5     1 1 0",
    "Frame4:  nrefB    2   0        0.68     0     4       -1 -2 1 4  1 0 1 0",
    "Frame5:  B        4   0        0.3536   0     3       -1 -4 2    1 0 1",
    "Frame6:  nrefB    5   0        0.68     0     3       -1 -5 1    1 0 1",
    NULL,
};

char *RpsPass2_GOPSize_7[] = {
    "Frame1:  B        7   0        0.442    0     2       -7 -14     1 1",
    "Frame2:  B        3   0        0.3536   0     2       -3 4       1 1",
    "Frame3:  B        1   0        0.3536   0     3       -1 2 6     1 1 0",
    "Frame4:  nrefB    2   0        0.68     0     4       -1 -2 1 5  1 0 1 0",
    "Frame5:  B        5   0        0.3536   0     3       -2 -5 2    1 0 1",
    "Frame6:  nrefB    4   0        0.68     0     4       -1 -4 1 3  1 0 1 0",
    "Frame7:  nrefB    6   0        0.68     0     3       -1 -6 1    1 0 1",
    NULL,
};

char *RpsPass2_GOPSize_8[] = {
    "Frame1:  B        8   0        0.442    0  2           -8 -16    1 1",
    "Frame2:  B        4   0        0.3536   0  2           -4 4      1 1",
    "Frame3:  B        2   0        0.3536   0  3           -2 2 6    1 1 0",
    "Frame4:  nrefB    1   0        0.68     0  4           -1 1 3 7  1 1 0 0",
    "Frame5:  nrefB    3   0        0.68     0  4           -1 -3 1 5 1 0 1 0",
    "Frame6:  B        6   0        0.3536   0  3           -2 -6 2   1 0 1",
    "Frame7:  nrefB    5   0        0.68     0  4           -1 -5 1 3 1 0 1 0",
    "Frame8:  nrefB    7   0        0.68     0  3           -1 -7 1   1 0 1",
    NULL,
};

i32 _CheckArea(VCEncPictureArea *area, const ENCODER_CODEC *h)
{
  i32 width = (h->options.width + h->options.max_cu_size - 1) / h->options.max_cu_size;
  i32 height = (h->options.height + h->options.max_cu_size - 1) / h->options.max_cu_size;

  if ((area->left < (u32)width) && (area->right < (u32)width) &&
      (area->top < (u32)height) && (area->bottom < (u32)height)) return 1;

  return 0;
}

static OMX_STRING nextToken (OMX_STRING str)
{
  OMX_STRING p = (OMX_STRING)strchr((const OMX_STRING)str, ' ');
  if (p)
  {
    while (*p == ' ') p ++;
    if (*p == '\0') p = NULL;
  }
  return p;
}

int ParseGopConfigString (OMX_STRING line, VCEncGopConfig *gopCfg, int frame_idx, int gopSize)
{
  if (!line)
    return -1;

  //format: FrameN Type POC QPoffset QPfactor  num_ref_pics ref_pics  used_by_cur
  int frameN, poc, num_ref_pics, i;
  char type[10];

  VCEncGopPicConfig *cfg = NULL;
  VCEncGopPicSpecialConfig *scfg = NULL;
  char *p_type = NULL;

  //frame idx
  sscanf ((const OMX_STRING)line, "Frame%d", &frameN);
  if ((frameN != (frame_idx + 1)) && (frameN != 0)) return -1;

  if (frameN > gopSize)
      return 0;

  if (0 == frameN)
  {
      //format: FrameN Type  QPoffset  QPfactor   TemporalId  num_ref_pics   ref_pics  used_by_cur  LTR    Offset   Interval
      scfg = &(gopCfg->pGopPicSpecialCfg[gopCfg->special_size++]);

      //frame type
      line = nextToken(line);
      if (!line) return -1;

      sscanf(line, "%s", type);
      scfg->nonReference = 0;
      if (strcmp(type, "I") == 0 || strcmp(type, "i") == 0)
          scfg->codingType = VCENC_INTRA_FRAME;
      else if (strcmp(type, "P") == 0 || strcmp(type, "p") == 0)
          scfg->codingType = VCENC_PREDICTED_FRAME;
      else if (strcmp(type, "B") == 0 || strcmp(type, "b") == 0)
          scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
      /* P frame not for reference */
      else if (strcmp(type, "nrefP") == 0)
      {
          scfg->codingType = VCENC_PREDICTED_FRAME;
          scfg->nonReference = 1;
      }
      /* B frame not for reference */
      else if (strcmp(type, "nrefB") == 0)
      {
          scfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
          scfg->nonReference = 1;
      }
      else
          scfg->codingType = scfg->nonReference = FRAME_TYPE_RESERVED;

      p_type = (char *)&type[0];

      //qp offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(scfg->QpOffset));

      //qp factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%lf", &(scfg->QpFactor));
      scfg->QpFactor = sqrt(scfg->QpFactor);

      //temporalId factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(scfg->temporalId));

      //num_ref_pics
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &num_ref_pics);
      if (num_ref_pics > VCENC_MAX_REF_FRAMES) /* NUMREFPICS_RESERVED -1 */
      {
          printf("GOP Config: Error, num_ref_pic can not be more than %d \n", VCENC_MAX_REF_FRAMES);
          return -1;
      }
      scfg->numRefPics = num_ref_pics;

      if ((scfg->codingType == VCENC_INTRA_FRAME) && (0 == num_ref_pics))
          num_ref_pics = 1;
      //ref_pics
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          if ((strncmp((const OMX_STRING)line, "L", 1) == 0) || (strncmp((const OMX_STRING)line, "l", 1) == 0))
          {
              sscanf((const OMX_STRING)line, "%c%d", p_type, &(scfg->refPics[i].ref_pic));
              scfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC( scfg->refPics[i].ref_pic - 1 );
          }
          else
          {
              sscanf((const OMX_STRING)line, "%d", &(scfg->refPics[i].ref_pic));
          }
      }
      if (i < num_ref_pics) return -1;

      //used_by_cur
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          sscanf((const OMX_STRING)line, "%u", &(scfg->refPics[i].used_by_cur));
      }
      if (i < num_ref_pics) return -1;

      // LTR
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &scfg->i32Ltr);
      if(VCENC_MAX_LT_REF_FRAMES < scfg->i32Ltr)
          return -1;

      // Offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &scfg ->i32Offset );

      // Interval
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &scfg ->i32Interval );

      if (0 != scfg->i32Ltr)
      {
          gopCfg->u32LTR_idx[ gopCfg->ltrcnt ] = LONG_TERM_REF_ID2DELTAPOC(scfg->i32Ltr-1);
          gopCfg->ltrcnt++;
          if (VCENC_MAX_LT_REF_FRAMES < gopCfg->ltrcnt)
              return -1;
      }

      // short_change
      scfg->i32short_change = 0;
      if (0 == scfg->i32Ltr)
      {
          /* not long-term ref */
          scfg->i32short_change = 1;
          for (i = 0; i < num_ref_pics; i++)
          {
              if (IS_LONG_TERM_REF_DELTAPOC(scfg->refPics[i].ref_pic)&& (0!=scfg->refPics[i].used_by_cur))
              {
                  scfg->i32short_change = 0;
                  break;
              }
          }
      }
  }
  else
  {
      //format: FrameN Type  POC  QPoffset    QPfactor   TemporalId  num_ref_pics  ref_pics  used_by_cur
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size++]);

      //frame type
      line = nextToken(line);
      if (!line) return -1;

      sscanf(line, "%s", type);
      p_type = (char *)&type[0];
      cfg->nonReference = 0;
      if (strcmp(type, "P") == 0 || strcmp(type, "p") == 0)
          cfg->codingType = VCENC_PREDICTED_FRAME;
      else if (strcmp(type, "B") == 0 || strcmp(type, "b") == 0)
          cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
      /* P frame not for reference */
      else if (strcmp(type, "nrefP") == 0)
      {
          cfg->codingType = VCENC_PREDICTED_FRAME;
          cfg->nonReference = 1;
      }
      /* B frame not for reference */
      else if (strcmp(type, "nrefB") == 0)
      {
          cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
          cfg->nonReference = 1;
      }
      else
          return -1;

      //poc
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &poc);
      if (poc < 1 || poc > gopSize) return -1;
      cfg->poc = poc;

      //qp offset
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(cfg->QpOffset));

      //qp factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%lf", &(cfg->QpFactor));
      // sqrt(QpFactor) is used in calculating lambda
      cfg->QpFactor = sqrt(cfg->QpFactor);

      //temporalId factor
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &(cfg->temporalId));

      //num_ref_pics
      line = nextToken(line);
      if (!line) return -1;
      sscanf((const OMX_STRING)line, "%d", &num_ref_pics);
      if (num_ref_pics < 0 || num_ref_pics > VCENC_MAX_REF_FRAMES)
      {
          printf("GOP Config: Error, num_ref_pic can not be more than %d \n", VCENC_MAX_REF_FRAMES);
          return -1;
      }

      //ref_pics
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          if ((strncmp((const OMX_STRING)line, "L", 1) == 0) || (strncmp((const OMX_STRING)line, "l", 1) == 0))
          {
              sscanf((const OMX_STRING)line, "%c%d", p_type, &(cfg->refPics[i].ref_pic));
              cfg->refPics[i].ref_pic = LONG_TERM_REF_ID2DELTAPOC(cfg->refPics[i].ref_pic - 1);
          }
          else
          {
              sscanf((const OMX_STRING)line, "%d", &(cfg->refPics[i].ref_pic));
          }
      }
      if (i < num_ref_pics) return -1;

      //used_by_cur
      for (i = 0; i < num_ref_pics; i++)
      {
          line = nextToken(line);
          if (!line) return -1;
          sscanf((const OMX_STRING)line, "%u", &(cfg->refPics[i].used_by_cur));
      }
      if (i < num_ref_pics) return -1;

      cfg->numRefPics = num_ref_pics;
  }

  return 0;
}

int ParseGopConfigFile (int gopSize, OMX_STRING fname, VCEncGopConfig *gopCfg)
{
#define MAX_LINE_LENGTH 1024
  int frame_idx = 0, line_idx = 0, addTmp;
  OMX_S8 achParserBuffer[MAX_LINE_LENGTH];
  FILE *fIn = fopen (fname, "r");
  if (fIn == NULL)
  {
    printf("GOP Config: Error, Can Not Open File %s\n", fname );
    return -1;
  }

  while ( 0 == feof(fIn))
  {
    if (feof (fIn)) break;
    line_idx ++;
    achParserBuffer[0] = '\0';
    // Read one line
    OMX_STRING line = (OMX_STRING)fgets ((OMX_STRING) achParserBuffer, MAX_LINE_LENGTH, fIn);
    if (!line) break;
    //handle line end
    OMX_STRING s = strpbrk((const OMX_STRING)line, "#\n");
    if(s) *s = '\0';

    addTmp = 1;
    line = strstr((const OMX_STRING)line, "Frame");
    if (line)
    {
      if( 0 == strncmp((const OMX_STRING)line, "Frame0", 6))
          addTmp = 0;

      if (ParseGopConfigString(line, gopCfg, frame_idx, gopSize) < 0)
      {
          printf("Invalid gop configure!\n");
          return -1;
      }

      frame_idx += addTmp;
    }
  }

  fclose(fIn);
  if (frame_idx != gopSize)
  {
    printf ("GOP Config: Error, Parsing File %s Failed at Line %d\n", fname, line_idx);
    return -1;
  }
  return 0;
}

int ReadGopConfig (OMX_STRING fname, OMX_STRING*config, VCEncGopConfig *gopCfg, int gopSize)
{
  int ret = -1;

  if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
    return -1;

  if(gopSize > MAX_GOP_SIZE)
    return -1;

  gopCfg->gopCfgOffset[gopSize] = gopCfg->size;
  if(fname)
  {
    ret = ParseGopConfigFile (gopSize, fname, gopCfg);
  }
  else if(config)
  {
    int id = 0;
    while (config[id])
    {
      ParseGopConfigString (config[id], gopCfg, id, gopSize);
      id ++;
    }
    ret = 0;
  }
  return ret;
}

static int _InitGopConfigs (OMX_U32 gopSize, ENCODER_CODEC *h, VCEncGopConfig *gopCfg, bool bPass2)
{
  OMX_U32 i, pre_load_num;
  OMX_STRING fname = h->options.gopCfg;
  char **rpsDefaultGop1 = RpsDefault_GOPSize_1;

  if (IS_H264(h->codecFormat))
    rpsDefaultGop1 = RpsDefault_H264_GOPSize_1;
/*
  else if (HW_ID_MAJOR_NUMBER(hwId) == 0x60)
    rpsDefaultGop1 = RpsDefault_V60_GOPSize_1;
*/

  char **default_configs[16] = {
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_1: rpsDefaultGop1,
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_2: RpsDefault_GOPSize_2,
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_3: RpsDefault_GOPSize_3,
              h->options.gopLowdelay ? RpsLowdelayDefault_GOPSize_4: RpsDefault_GOPSize_4,
              RpsDefault_GOPSize_5,
              RpsDefault_GOPSize_6,
              RpsDefault_GOPSize_7,
              RpsDefault_GOPSize_8,
              NULL,
              NULL,
              NULL,
              NULL,
              NULL,
              NULL,
              NULL,
              RpsDefault_GOPSize_16
  };

  if (gopSize > MAX_GOP_SIZE || (gopSize > 0 && default_configs[gopSize-1] == NULL && fname == NULL))
  {
    printf ("GOP Config: Error, Invalid GOP Size\n");
    return -1;
  }

  if (h->options.lowDelayB &&
      ((h->options.lookaheadDepth && bPass2) || h->options.lookaheadDepth == 0))
  {
    default_configs[0] = RpsPass2_GOPSize_1;
    default_configs[1] = RpsPass2_GOPSize_2;
    default_configs[2] = RpsPass2_GOPSize_3;
    default_configs[3] = RpsPass2_GOPSize_4;
    default_configs[4] = RpsPass2_GOPSize_5;
    default_configs[5] = RpsPass2_GOPSize_6;
    default_configs[6] = RpsPass2_GOPSize_7;
    default_configs[7] = RpsPass2_GOPSize_8;
  }

  //Handle Interlace
  if (h->options.interlacedFrame && gopSize==1)
  {
    default_configs[0] = RpsDefault_Interlace_GOPSize_1;
  }

  // GOP size in rps array for gopSize=N
  // N<=4:      GOP1, ..., GOPN
  // 4<N<=8:   GOP1, GOP2, GOP3, GOP4, GOPN
  // N > 8:       GOP1, GOPN
  // Adaptive:  GOP1, GOP2, GOP3, GOP4, GOP6, GOP8
  if (gopSize > 8)
    pre_load_num = 4;
  else if (gopSize>=4 || gopSize==0)
    pre_load_num = 4;
  else
    pre_load_num = gopSize;

  gopCfg->special_size = 0;
  gopCfg->ltrcnt       = 0;

  for (i = 1; i <= pre_load_num; i ++)
  {
    if (ReadGopConfig (gopSize==i ? fname : NULL, default_configs[i-1], gopCfg, i))
      return -1;
  }

  if (gopSize == 0)
  {
    //gop6
    if (ReadGopConfig (NULL, default_configs[5], gopCfg, 6))
      return -1;
    //gop8
    if (ReadGopConfig (NULL, default_configs[7], gopCfg, 8))
      return -1;
  }
  else if (gopSize > 4)
  {
    //gopSize
    if (ReadGopConfig (fname, default_configs[gopSize-1], gopCfg, gopSize))
      return -1;
  }

  if ((VSI_DEFAULT_VALUE != h->options.ltrInterval) && (gopCfg->special_size == 0))
  {
      if (gopSize != 1)
      {
          printf("GOP Config: Error, when using --LTR configure option, the gopsize alse should be set to 1!\n");
          return -1;
      }
      gopCfg->pGopPicSpecialCfg[0].poc = 0;
      gopCfg->pGopPicSpecialCfg[0].QpOffset = h->options.longTermQpDelta;
      gopCfg->pGopPicSpecialCfg[0].QpFactor = QPFACTOR_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].temporalId = TEMPORALID_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].codingType = FRAME_TYPE_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].numRefPics = NUMREFPICS_RESERVED;
      gopCfg->pGopPicSpecialCfg[0].i32Ltr = 1;
      gopCfg->pGopPicSpecialCfg[0].i32Offset = 0;
      gopCfg->pGopPicSpecialCfg[0].i32Interval = h->options.ltrInterval;
      gopCfg->pGopPicSpecialCfg[0].i32short_change = 0;
      gopCfg->u32LTR_idx[0]                    = LONG_TERM_REF_ID2DELTAPOC(0);

      gopCfg->pGopPicSpecialCfg[1].poc = 0;
      gopCfg->pGopPicSpecialCfg[1].QpOffset = QPOFFSET_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].QpFactor = QPFACTOR_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].temporalId = TEMPORALID_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].codingType = FRAME_TYPE_RESERVED;
      gopCfg->pGopPicSpecialCfg[1].numRefPics = 2;
      gopCfg->pGopPicSpecialCfg[1].refPics[0].ref_pic     = -1;
      gopCfg->pGopPicSpecialCfg[1].refPics[0].used_by_cur = 1;
      gopCfg->pGopPicSpecialCfg[1].refPics[1].ref_pic     = LONG_TERM_REF_ID2DELTAPOC(0);
      gopCfg->pGopPicSpecialCfg[1].refPics[1].used_by_cur = 1;
      gopCfg->pGopPicSpecialCfg[1].i32Ltr = 0;
      gopCfg->pGopPicSpecialCfg[1].i32Offset = h->options.longTermGapOffset;
      gopCfg->pGopPicSpecialCfg[1].i32Interval = h->options.longTermGap;
      gopCfg->pGopPicSpecialCfg[1].i32short_change = 0;

      gopCfg->special_size = 2;
      gopCfg->ltrcnt = 1;
  }

  if (0)
    for(i = 0; i < (gopSize == 0 ? gopCfg->size : gopCfg->gopCfgOffset[gopSize]); i++)
    {
      // when use long-term, change P to B in default configs (used for last gop)
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_PREDICTED_FRAME)
        cfg->codingType = VCENC_BIDIR_PREDICTED_FRAME;
    }

  //Compatible with old bFrameQpDelta setting
  if (h->options.bFrameQpDelta >= 0 && fname == NULL)
  {
    for (i = 0; i < gopCfg->size; i++)
    {
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[i]);
      if (cfg->codingType == VCENC_BIDIR_PREDICTED_FRAME)
        cfg->QpOffset = h->options.bFrameQpDelta;
    }
  }

  // lowDelay auto detection
  VCEncGopPicConfig *cfgStart = &(gopCfg->pGopPicCfg[gopCfg->gopCfgOffset[gopSize]]);
  if (gopSize == 1)
  {
    h->options.gopLowdelay = 1;
  }
  else if ((gopSize > 1) && (h->options.gopLowdelay == 0))
  {
     h->options.gopLowdelay = 1;
     for (i = 1; i < gopSize; i++)
     {
        if (cfgStart[i].poc < cfgStart[i-1].poc)
        {
          h->options.gopLowdelay = 0;
          break;
        }
     }
  }

  {
      i32 i32LtrPoc[VCENC_MAX_LT_REF_FRAMES];

      for (i = 0; i < VCENC_MAX_LT_REF_FRAMES; i++)
          i32LtrPoc[i] = -1;
      for (i = 0; i < gopCfg->special_size; i++)
      {
          if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > VCENC_MAX_LT_REF_FRAMES)
          {
              printf("GOP Config: Error, Invalid long-term index\n");
              return -1;
          }
          if (gopCfg->pGopPicSpecialCfg[i].i32Ltr > 0)
              i32LtrPoc[i] = gopCfg->pGopPicSpecialCfg[i].i32Ltr - 1;
      }

      for (i = 0; i < gopCfg->ltrcnt; i++)
      {
          if ((0 != i32LtrPoc[0]) || (-1 == i32LtrPoc[i]) || ((i>0) && i32LtrPoc[i] != (i32LtrPoc[i - 1] + 1)))
          {
              printf("GOP Config: Error, Invalid long-term index\n");
              return -1;
          }
      }
  }

  //For lowDelay, Handle the first few frames that miss reference frame
  if (1)
  {
    int nGop;
    int idx = 0;
    int maxErrFrame = 0;
    VCEncGopPicConfig *cfg;

    // Find the max frame number that will miss its reference frame defined in rps    
    while ((idx - maxErrFrame) < (int)gopSize)
    {
      nGop = (idx / gopSize) * gopSize;
      cfg = &(cfgStart[idx % gopSize]);

      for (i = 0; i < cfg->numRefPics; i ++)
      {
        //POC of this reference frame
        int refPoc = cfg->refPics[i].ref_pic + cfg->poc + nGop;
        if (refPoc < 0)
        {
          maxErrFrame = idx + 1;
        }
      }
      idx ++;
    }

    // Try to config a new rps for each "error" frame by modifying its original rps 
    for (idx = 0; idx < maxErrFrame; idx ++)
    {
      int j, iRef, nRefsUsedByCur, nPoc;
      VCEncGopPicConfig *cfgCopy;

      if (gopCfg->size >= MAX_GOP_PIC_CONFIG_NUM)
        break;

      // Add to array end
      cfg = &(gopCfg->pGopPicCfg[gopCfg->size]);
      cfgCopy = &(cfgStart[idx % gopSize]);
      memcpy(cfg, cfgCopy, sizeof (VCEncGopPicConfig));
      gopCfg->size ++;

      // Copy reference pictures
      nRefsUsedByCur = iRef = 0;
      nPoc = cfgCopy->poc + ((idx / gopSize) * gopSize);
      for (i = 0; i < cfgCopy->numRefPics; i ++)
      {
        int newRef = 1;
        int used_by_cur = cfgCopy->refPics[i].used_by_cur;
        int ref_pic = cfgCopy->refPics[i].ref_pic;
        // Clip the reference POC
        if ((cfgCopy->refPics[i].ref_pic + nPoc) < 0)
          ref_pic = 0 - (nPoc);

        // Check if already have this reference
        for (j = 0; j < iRef; j ++)
        {
          if (cfg->refPics[j].ref_pic == ref_pic)
          {
             newRef = 0;
             if (used_by_cur)
               cfg->refPics[j].used_by_cur = used_by_cur;
             break;
          }
        }

        // Copy this reference
        if (newRef)
        {
          cfg->refPics[iRef].ref_pic = ref_pic;
          cfg->refPics[iRef].used_by_cur = used_by_cur;
          iRef ++;
        }
      }
      cfg->numRefPics = iRef;
      // If only one reference frame, set P type.
      for (i = 0; i < cfg->numRefPics; i ++)
      {
        if (cfg->refPics[i].used_by_cur)
          nRefsUsedByCur ++;
      }
      if (nRefsUsedByCur == 1)
        cfg->codingType = VCENC_PREDICTED_FRAME;
    }
  }

#if 0
      //print for debug
      int idx;
      printf ("====== REF PICTURE SETS from %s ======\n",fname ? fname : "VSI_DEFAULT_VALUE");
      for (idx = 0; idx < gopCfg->size; idx ++)
      {
        int i;
        VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[idx]);
        OMX_S8 type = cfg->codingType==VCENC_PREDICTED_FRAME ? 'P' : cfg->codingType == VCENC_INTRA_FRAME ? 'I' : 'B';
        printf (" FRAME%2d:  %c %d %d %f %d", idx, type, cfg->poc, cfg->QpOffset, cfg->QpFactor, cfg->numRefPics);
        for (i = 0; i < cfg->numRefPics; i ++)
          printf (" %d", cfg->refPics[i].ref_pic);
        for (i = 0; i < cfg->numRefPics; i ++)
          printf (" %d", cfg->refPics[i].used_by_cur);
        printf("\n");
      }
      printf ("===========================================\n");
#endif
  return 0;
}

static void vce_init_pic(ENCODER_CODEC *h)
{
//    ma_s *ma = &h->ma;
    adapGopCtr *agop = &h->agop;
    h->validencodedframenumber = 0;

    //Adaptive Gop variables
    agop->last_gopsize = MAX_ADAPTIVE_GOP_SIZE;
    agop->gop_frm_num = 0;
    agop->sum_intra_vs_interskip = 0;
    agop->sum_skip_vs_interskip = 0;
    agop->sum_intra_vs_interskipP = 0;
    agop->sum_intra_vs_interskipB = 0;
    agop->sum_costP = 0;
    agop->sum_costB = 0;

#if 0
    ma->pos = ma->count = 0;
    ma->outputRateNumer = cml->outputRateNumer;
    ma->outputRateDenom = cml->outputRateDenom;
    if (cml->outputRateDenom)
        ma->length = MAX(LEAST_MONITOR_FRAME, MIN(cml->monitorFrames,
                MOVING_AVERAGE_FRAMES));
    else
        ma->length = MOVING_AVERAGE_FRAMES;
#endif
}

#if 0
i32 AdaptiveGopDecision(ENCODER_CODEC *h)
{
    i32 nextGopSize =-1;
    adapGopCtr * agop = &h->agop;
    VCEncIn *pEncIn = &h->encIn;

    double dIntraVsInterskip = (double)h->encOut.cuStatis.intraCu8Num/(double)((h->width/8) * (h->height/8));
    double dSkipVsInterskip = (double)h->encOut.cuStatis.skipCu8Num/(double)((h->width/8) * (h->height/8));

    agop->gop_frm_num++;
    agop->sum_intra_vs_interskip += dIntraVsInterskip;
    agop->sum_skip_vs_interskip += dSkipVsInterskip;
    agop->sum_costP += (pEncIn->codingType == VCENC_PREDICTED_FRAME)? h->encOut.cuStatis.PBFrame4NRdCost:0;
    agop->sum_costB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)? h->encOut.cuStatis.PBFrame4NRdCost:0;
    agop->sum_intra_vs_interskipP += (pEncIn->codingType == VCENC_PREDICTED_FRAME)? dIntraVsInterskip:0;
    agop->sum_intra_vs_interskipB += (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)? dIntraVsInterskip:0; 

    if(pEncIn->gopPicIdx == pEncIn->gopSize-1)//last frame of the current gop. decide the gopsize of next gop.
    {
        dIntraVsInterskip = agop->sum_intra_vs_interskip/agop->gop_frm_num;
        dSkipVsInterskip = agop->sum_skip_vs_interskip/agop->gop_frm_num;
        agop->sum_costB = (agop->gop_frm_num>1)?(agop->sum_costB/(agop->gop_frm_num-1)):0xFFFFFFF;
        agop->sum_intra_vs_interskipB = (agop->gop_frm_num>1)?(agop->sum_intra_vs_interskipB/(agop->gop_frm_num-1)):0xFFFFFFF;
        //Enabled adaptive GOP size for large resolution
        if (((h->width * h->height) >= (1280 * 720)) || ((MAX_ADAPTIVE_GOP_SIZE >3)&&((h->width * h->height) >= (416 * 240))))
        {
            if ((((double)agop->sum_costP/(double)agop->sum_costB)<1.1)&&(dSkipVsInterskip >= 0.95))
            {
                agop->last_gopsize = nextGopSize = 1;
            }
            else if (((double)agop->sum_costP/(double)agop->sum_costB)>5)
            {
             nextGopSize = agop->last_gopsize;
            }
            else
            {
                if( ((agop->sum_intra_vs_interskipP > 0.40) && (agop->sum_intra_vs_interskipP < 0.70)&& (agop->sum_intra_vs_interskipB < 0.10)) )
                {
                    agop->last_gopsize++;
                    if(agop->last_gopsize==5 || agop->last_gopsize==7)  
                    {
                        agop->last_gopsize++;
                    }
                    agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                    nextGopSize = agop->last_gopsize; //
                }
                else if (dIntraVsInterskip >= 0.30)
                {
                    agop->last_gopsize = nextGopSize = 1; //No B
                }
                else if (dIntraVsInterskip >= 0.20)
                {
                    agop->last_gopsize = nextGopSize = 2; //One B
                }
                else if (dIntraVsInterskip >= 0.10)
                {
                    agop->last_gopsize--;
                    if(agop->last_gopsize == 5 || agop->last_gopsize==7) 
                    {
                        agop->last_gopsize--;
                    }
                    agop->last_gopsize = MAX(agop->last_gopsize, 3);
                    nextGopSize = agop->last_gopsize; //
                }
                else
                {
                    agop->last_gopsize++;
                    if(agop->last_gopsize==5 || agop->last_gopsize==7)  
                    {
                        agop->last_gopsize++;
                    }
                    agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
                    nextGopSize = agop->last_gopsize; //
                }
            }
        }
        else
        {
            nextGopSize = 3;
        }
        agop->gop_frm_num = 0;
        agop->sum_intra_vs_interskip = 0;
        agop->sum_skip_vs_interskip = 0;
        agop->sum_costP = 0;
        agop->sum_costB = 0;
        agop->sum_intra_vs_interskipP = 0;
        agop->sum_intra_vs_interskipB = 0;

        nextGopSize = MIN(nextGopSize, MAX_ADAPTIVE_GOP_SIZE);
    }

    if(nextGopSize != -1)
        h->nextGopSize = nextGopSize;

    return nextGopSize;
}
#endif

/*------------------------------------------------------------------------------
Function name : InitPicConfig
Description   : initial pic reference configure
Return type   : void
Argument      : VCEncIn *pEncIn
------------------------------------------------------------------------------*/
void InitPicConfig(ENCODER_CODEC *h)
{
    i32 i, j, k, i32Poc;
    i32 i32MaxpicOrderCntLsb = 1 << 16;
    VCEncIn *pEncIn = &h->encIn;

    pEncIn->gopCurrPicConfig.codingType = FRAME_TYPE_RESERVED;
    pEncIn->gopCurrPicConfig.nonReference = FRAME_TYPE_RESERVED;
    pEncIn->gopCurrPicConfig.numRefPics = NUMREFPICS_RESERVED;
    pEncIn->gopCurrPicConfig.poc = -1;
    pEncIn->gopCurrPicConfig.QpFactor = QPFACTOR_RESERVED;
    pEncIn->gopCurrPicConfig.QpOffset = QPOFFSET_RESERVED;
    pEncIn->gopCurrPicConfig.temporalId = 0; //TEMPORALID_RESERVED;
    pEncIn->i8SpecialRpsIdx = -1;
    for (k = 0; k < VCENC_MAX_REF_FRAMES; k++)
    {
        pEncIn->gopCurrPicConfig.refPics[k].ref_pic     = INVALITED_POC;
        pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = 0;
    }

    for (k = 0; k < VCENC_MAX_LT_REF_FRAMES; k++)
        pEncIn->long_term_ref_pic[k] = INVALITED_POC;

    pEncIn->bIsPeriodUsingLTR = HANTRO_FALSE;
    pEncIn->bIsPeriodUpdateLTR = HANTRO_FALSE;

    for (i = 0; i < pEncIn->gopConfig.special_size; i++)
    {
        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Interval <= 0)
            continue;

        if (pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr == 0)
            pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
        else
        {
            pEncIn->bIsPeriodUpdateLTR = HANTRO_TRUE;

            for (k = 0; k < (i32)pEncIn->gopConfig.pGopPicSpecialCfg[i].numRefPics; k++)
            {
                i32 i32LTRIdx = pEncIn->gopConfig.pGopPicSpecialCfg[i].refPics[k].ref_pic;
                if ((IS_LONG_TERM_REF_DELTAPOC(i32LTRIdx)) && ((pEncIn->gopConfig.pGopPicSpecialCfg[i].i32Ltr-1) == LONG_TERM_REF_DELTAPOC2ID(i32LTRIdx)))
                {
                    pEncIn->bIsPeriodUsingLTR = HANTRO_TRUE;
                }
            }
        }
    }

    memset(pEncIn->bLTR_need_update, 0, sizeof(pEncIn->bLTR_need_update));
    pEncIn->bIsIDR       = HANTRO_TRUE;

    i32Poc = 0;
    /* check current picture encoded as LTR*/
    pEncIn->u8IdxEncodedAsLTR = 0;
    for (j = 0; j < pEncIn->gopConfig.special_size; j++)
    {
        if (pEncIn->bIsPeriodUsingLTR == HANTRO_FALSE)
            break;

        if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) || (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0))
            continue;

        i32Poc = i32Poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

        if (i32Poc < 0)
        {
            i32Poc += i32MaxpicOrderCntLsb;
            if (i32Poc >(i32MaxpicOrderCntLsb >> 1))
                i32Poc = -1;
        }

        if ((i32Poc >= 0) && (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0))
        {
            /* more than one LTR at the same frame position */
            if (0 != pEncIn->u8IdxEncodedAsLTR)
            {
                // reuse the same POC LTR
                pEncIn->bLTR_need_update[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr - 1] = HANTRO_TRUE;
                continue;
            }

            pEncIn->gopCurrPicConfig.codingType = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType == FRAME_TYPE_RESERVED) ? pEncIn->gopCurrPicConfig.codingType : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
            pEncIn->gopCurrPicConfig.numRefPics = ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == NUMREFPICS_RESERVED) ? pEncIn->gopCurrPicConfig.numRefPics : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
            pEncIn->gopCurrPicConfig.QpFactor = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED) ? pEncIn->gopCurrPicConfig.QpFactor : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
            pEncIn->gopCurrPicConfig.QpOffset = (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED) ? pEncIn->gopCurrPicConfig.QpOffset : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
            pEncIn->gopCurrPicConfig.temporalId = (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId == TEMPORALID_RESERVED) ? pEncIn->gopCurrPicConfig.temporalId : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

            if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics != NUMREFPICS_RESERVED))
            {
                for (k = 0; k < (i32)pEncIn->gopCurrPicConfig.numRefPics; k++)
                {
                    pEncIn->gopCurrPicConfig.refPics[k].ref_pic = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
                    pEncIn->gopCurrPicConfig.refPics[k].used_by_cur = pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
                }
            }

            pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
            pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = HANTRO_TRUE;
        }
    }
    u32 gopSize = h->options.gopSize;
    // bool adaptiveGop = (gopSize == 0);

    pEncIn->timeIncrement = 0;
    pEncIn->vui_timing_info_enable = h->options.vui_timing_info_enable;
    pEncIn->hashType = h->options.hashtype;
    pEncIn->poc = 0;
    //default gop size as IPPP
    pEncIn->gopSize =  gopSize;
    pEncIn->last_idr_picture_cnt = pEncIn->picture_cnt = 0;
    memcpy(pEncIn->gmv, h->options.gmv, sizeof(pEncIn->gmv));
}

//translate OMX profile to VCe's profile
static int vce_parse_hevc_profile(OMX_VIDEO_HEVCPROFILETYPE omx_profile, VCEncProfile *vc_profile)
{
    switch(omx_profile)
    {
        case OMX_VIDEO_HEVCProfileMain:
            *vc_profile = VCENC_HEVC_MAIN_PROFILE;
            break;
        case OMX_VIDEO_HEVCProfileMain10:
            *vc_profile = VCENC_HEVC_MAIN_10_PROFILE;
            break;
        case OMX_VIDEO_HEVCProfileMainStillPicture:
            *vc_profile = VCENC_HEVC_MAIN_STILL_PICTURE_PROFILE;
            break;
        default:
            DBGT_CRITICAL("Profile not supported (requested profile: %d)", omx_profile);
            DBGT_EPILOG("");
            return -1;
    }

    return 0;
}

//translate OMX level to VCE's level
static int vce_parse_hevc_level(OMX_VIDEO_HEVCLEVELTYPE omx_level, VCEncLevel *vc_level)
{
    switch (omx_level)
    {
        case OMX_VIDEO_HEVCLevel1:
            *vc_level = VCENC_HEVC_LEVEL_1;
            break;
        case OMX_VIDEO_HEVCLevel2:
            *vc_level = VCENC_HEVC_LEVEL_2;
            break;
        case OMX_VIDEO_HEVCLevel21:
            *vc_level = VCENC_HEVC_LEVEL_2_1;
            break;
        case OMX_VIDEO_HEVCLevel3:
            *vc_level = VCENC_HEVC_LEVEL_3;
            break;
        case OMX_VIDEO_HEVCLevel31:
            *vc_level = VCENC_HEVC_LEVEL_3_1;
            break;
        case OMX_VIDEO_HEVCLevel4:
            *vc_level = VCENC_HEVC_LEVEL_4;
            break;
        case OMX_VIDEO_HEVCLevel41:
            *vc_level = VCENC_HEVC_LEVEL_4_1;
            break;
        case OMX_VIDEO_HEVCLevel5:
            *vc_level = VCENC_HEVC_LEVEL_5;
            break;
        case OMX_VIDEO_HEVCLevel51:
            *vc_level = VCENC_HEVC_LEVEL_5_1;
            break;
        case OMX_VIDEO_HEVCLevel52:
            *vc_level = VCENC_HEVC_LEVEL_5_2;
            break;
        case OMX_VIDEO_HEVCLevel6:
            *vc_level = VCENC_HEVC_LEVEL_6;
            break;
        case OMX_VIDEO_HEVCLevel61:
            *vc_level = VCENC_HEVC_LEVEL_6_1;
            break;
        case OMX_VIDEO_HEVCLevel62:
        case OMX_VIDEO_HEVCLevelMax:
            *vc_level = VCENC_HEVC_LEVEL_6_2;
            break;
        default:
            *vc_level = VCENC_AUTO_LEVEL; /* automatic level */
            break;
    }

    return 0;
}

//translate OMX profile to VCE's profile
static int vce_parse_avc_profile(OMX_VIDEO_AVCPROFILETYPE omx_profile, VCEncProfile *vc_profile)
{
    switch(omx_profile)
    {
        case OMX_VIDEO_AVCProfileBaseline:
            *vc_profile = VCENC_H264_BASE_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileMain:
            *vc_profile = VCENC_H264_MAIN_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileHigh:
            *vc_profile = VCENC_H264_HIGH_PROFILE;
            break;
        case OMX_VIDEO_AVCProfileHigh10:
            *vc_profile = VCENC_H264_HIGH_10_PROFILE;
            break;
       default:
            DBGT_CRITICAL("Profile not supported (requested profile: %d)", omx_profile);
            DBGT_EPILOG("");
            return -1;
    }

    return 0;
}

//translate OMX level to VCE's level
static int vce_parse_avc_level(OMX_VIDEO_AVCLEVELTYPE omx_level, VCEncLevel *vc_level)
{
    if(omx_level <= OMX_VIDEO_AVCLevel51)
    {
        switch ((u32)omx_level)
        {
            case OMX_VIDEO_AVCLevel1:
                *vc_level = VCENC_H264_LEVEL_1;
                break;
            case OMX_VIDEO_AVCLevel1b:
                *vc_level = VCENC_H264_LEVEL_1_b;
                break;
            case OMX_VIDEO_AVCLevel11:
                *vc_level = VCENC_H264_LEVEL_1_1;
                break;
            case OMX_VIDEO_AVCLevel12:
                *vc_level = VCENC_H264_LEVEL_1_2;
                break;
            case OMX_VIDEO_AVCLevel13:
                *vc_level = VCENC_H264_LEVEL_1_3;
                break;
            case OMX_VIDEO_AVCLevel2:
                *vc_level = VCENC_H264_LEVEL_2;
                break;
            case OMX_VIDEO_AVCLevel21:
                *vc_level = VCENC_H264_LEVEL_2_1;
                break;
            case OMX_VIDEO_AVCLevel22:
                *vc_level = VCENC_H264_LEVEL_2_2;
                break;
            case OMX_VIDEO_AVCLevel3:
                *vc_level = VCENC_H264_LEVEL_3;
                break;
            case OMX_VIDEO_AVCLevel31:
                *vc_level = VCENC_H264_LEVEL_3_1;
                break;
            case OMX_VIDEO_AVCLevel32:
                *vc_level = VCENC_H264_LEVEL_3_2;
                break;
            case OMX_VIDEO_AVCLevel4:
                *vc_level = VCENC_H264_LEVEL_4;
                break;
            case OMX_VIDEO_AVCLevel41:
                *vc_level = VCENC_H264_LEVEL_4_1;
                break;
            case OMX_VIDEO_AVCLevel42:
                *vc_level = VCENC_H264_LEVEL_4_2;
                break;
            case OMX_VIDEO_AVCLevel5:
                *vc_level = VCENC_H264_LEVEL_5;
                break;
            case OMX_VIDEO_AVCLevel51:
                *vc_level = VCENC_H264_LEVEL_5_1;
                break;
            case OMX_VIDEO_AVCLevel52:
                *vc_level = VCENC_H264_LEVEL_5_2;
                break;
            case OMX_VIDEO_AVCLevel60:
                *vc_level = VCENC_H264_LEVEL_6;
                break;
            case OMX_VIDEO_AVCLevel61:
                *vc_level = VCENC_H264_LEVEL_6_1;
                break;
            case OMX_VIDEO_AVCLevel62:
                *vc_level = VCENC_H264_LEVEL_6_2;
                break;
            case OMX_VIDEO_AVCLevelAuto:
                *vc_level = VCENC_AUTO_LEVEL;
                break;
            default:
                *vc_level = VCENC_AUTO_LEVEL; /* automatic level */
                break;
        }
    }
    else
    {

        switch ((OMX_VIDEO_AVCEXTLEVELTYPE)omx_level)
        {

            case OMX_VIDEO_AVCLevel52:
                *vc_level = VCENC_H264_LEVEL_5_2;
                break;
            case OMX_VIDEO_AVCLevel60:
                *vc_level = VCENC_H264_LEVEL_6;
                break;
            case OMX_VIDEO_AVCLevel61:
                *vc_level = VCENC_H264_LEVEL_6_1;
                break;
            case OMX_VIDEO_AVCLevel62:
                *vc_level = VCENC_H264_LEVEL_6_2;
                break;
            case OMX_VIDEO_AVCLevelAuto:
                *vc_level = VCENC_AUTO_LEVEL;
                break;
            default:
                DBGT_CRITICAL("Unsupported encoding level %d", omx_level);
                DBGT_EPILOG("");
                return -1;
                break;
        }
    }

    return 0;
}

static int vce_parse_av1_profile(OMX_VIDEO_AV1PROFILETYPE omx_profile, VCEncProfile *vc_profile) {
  switch(omx_profile)
  {
    case OMX_VIDEO_AV1ProfileMain8:
    case OMX_VIDEO_AV1ProfileMain10:
    case OMX_VIDEO_AV1ProfileMain10HDR10:
    case OMX_VIDEO_AV1ProfileMain10HDR10Plus:
      *vc_profile = VCENC_AV1_MAIN_PROFILE;
      break;
    default:
      DBGT_CRITICAL("Profile not supported (requested profile: %d)", omx_profile);
      DBGT_EPILOG("");
      return -1;
  }

  return 0;
}

//translate OMX level to VCE's level
static int vce_parse_av1_level(OMX_VIDEO_AV1LEVELTYPE omx_level, VCEncLevel *vc_level)
{
    switch (omx_level)
    {
      case OMX_VIDEO_AV1Level2:
      case OMX_VIDEO_AV1Level21:
      case OMX_VIDEO_AV1Level22:
      case OMX_VIDEO_AV1Level23:
      case OMX_VIDEO_AV1Level3 :
      case OMX_VIDEO_AV1Level31:
      case OMX_VIDEO_AV1Level32:
      case OMX_VIDEO_AV1Level33:
      case OMX_VIDEO_AV1Level4 :
      case OMX_VIDEO_AV1Level41:
      case OMX_VIDEO_AV1Level42:
      case OMX_VIDEO_AV1Level43:
      case OMX_VIDEO_AV1Level5 :
      case OMX_VIDEO_AV1Level51:
      case OMX_VIDEO_AV1Level52:
      case OMX_VIDEO_AV1Level53:
      case OMX_VIDEO_AV1Level6 :
      case OMX_VIDEO_AV1Level61:
      case OMX_VIDEO_AV1Level62:
      case OMX_VIDEO_AV1Level63:
      case OMX_VIDEO_AV1Level7 :
      case OMX_VIDEO_AV1Level71:
      case OMX_VIDEO_AV1Level72:
      case OMX_VIDEO_AV1Level73:
      default:
            *vc_level = VCENC_AUTO_LEVEL; /* automatic level forever */
          break;
    }

    return 0;
}

void Parameter_Preset(VCE_VIDEO_OPTIONS *opts)
{
    /************************************************
      Preset: H264    Preset: HEVC      LA=40   rdoLevel    RDOQ
      N/A                 4                          Y           3                Y
      N/A                 3                          Y           3                N
      N/A                 2                          Y           2                N
      1                      1                          Y           1                N
      0                      0                          N           1                N
    *************************************************/
    if(opts->preset >=4)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 3;
      opts->enableRdoQuant = 1;
    }
    else if(opts->preset ==3)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 3;
      opts->enableRdoQuant = 0;
    }
    else if(opts->preset ==2)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 2;
      opts->enableRdoQuant = 0;
    }
    else if(opts->preset ==1)
    {
      opts->lookaheadDepth = 40;
      opts->rdoLevel = 1;
      opts->enableRdoQuant = 0;
    }
    else if(opts->preset ==0)
    {
      opts->lookaheadDepth = 0;
      opts->rdoLevel = 1;
      opts->enableRdoQuant = 0;
    }
}

void vce_parse_rcMode(ENCODER_CODEC *this, OMX_U32 rcMode)
{
    VCE_VIDEO_OPTIONS *opts = &this->options;

    if (rcMode == 0)
    {
        /* if rcMode uses default CVBR, below options can auto-change rcMode,
        * otherwise rcMode has higher priority
        */
        if (opts->vbr == 1 && opts->crf < 0)
            rcMode = 2;
        else if (opts->cpbSize > 0)
            rcMode = 1;
        else if (opts->crf >= 0)
            rcMode = 4;
        else if (this->nPictureRcEnabled <= 0)
            rcMode = 5;
    }

    /* check RC Mode with other parameters */
    switch (rcMode)
    {
        case 1:
            opts->rcMode = VCE_RC_CBR;
            if (opts->cpbSize <= 0)
            {
                /* set to 2 second size by default */
                opts->cpbSize = 2 * opts->bitPerSecond;
            }
            opts->vbr = 0;
            opts->crf = -1;
            this->nPictureRcEnabled = 1;
            break;
        case 2:
            opts->rcMode = VCE_RC_VBR;
            opts->vbr = 1;
            opts->crf = -1;
            opts->cpbSize = 0;
            opts->cpbMaxRate = 0;
            this->nPictureRcEnabled = 1;
            break;
        case 3:
            opts->rcMode = VCE_RC_ABR;
            opts->vbr = 0;
            opts->crf = -1;
            opts->cpbSize = 0;
            opts->cpbMaxRate = 0;
            this->nPictureRcEnabled = 1;
            break;
        case 4:
            opts->rcMode = VCE_RC_CRF;
            opts->vbr = 0;
            opts->cpbSize = 0;
            opts->cpbMaxRate = 0;
            if (opts->crf < 0)
                opts->crf = 0;
            this->nPictureRcEnabled = 0;
            break;
        case 5:
            opts->rcMode = VCE_RC_CQP;
            opts->vbr = 0;
            opts->crf = -1;
            opts->cpbSize = 0;
            opts->cpbMaxRate = 0;
            this->nPictureRcEnabled = 0;
            break;
        default:
            opts->rcMode = VCE_RC_CVBR;
            opts->vbr = 0;
            opts->crf = -1;
            opts->cpbSize = 0;
            opts->cpbMaxRate = 0;
            this->nPictureRcEnabled = 1;
            break;
    }
}

// init vce gop configs
static int vce_init_gopConfigs(ENCODER_CODEC *h)
{
    h->encIn.gopConfig.pGopPicCfg = h->gopPicCfg;

    h->encIn.gopConfig.pGopPicSpecialCfg = h->gopPicSpecialCfg;


    h->encIn.gopConfig.idr_interval = h->options.intraPicRate;
    h->encIn.gopConfig.firstPic = h->options.firstPic;
    h->encIn.gopConfig.lastPic = h->options.lastPic;
    h->encIn.gopConfig.outputRateNumer = h->options.outputRateNumer;  /* Output frame rate numerator */
    h->encIn.gopConfig.outputRateDenom = h->options.outputRateDenom; /* Output frame rate denominator */
    h->encIn.gopConfig.inputRateNumer = h->options.inputRateNumer;      /* Input frame rate numerator */
    h->encIn.gopConfig.inputRateDenom = h->options.inputRateDenom;      /* Input frame rate denominator */
    h->encIn.gopConfig.interlacedFrame = h->options.interlacedFrame;

    if (_InitGopConfigs(h->options.gopSize, h, &h->encIn.gopConfig, HANTRO_FALSE) != 0)
    {
        return -1;
    }

    if(h->options.lookaheadDepth) {
        memset (h->gopPicCfgPass2, 0, sizeof(h->gopPicCfgPass2));
        h->encIn.gopConfig.pGopPicCfg = h->gopPicCfgPass2;
        h->encIn.gopConfig.size = 0;
        memset(h->gopPicSpecialCfg, 0, sizeof(h->gopPicSpecialCfg));
        h->encIn.gopConfig.pGopPicSpecialCfg = h->gopPicSpecialCfg;
        if (_InitGopConfigs (h->options.gopSize, h, &h->encIn.gopConfig, HANTRO_TRUE) != 0)
        {
          return -1;
        }
        h->encIn.gopConfig.pGopPicCfgPass1 = h->gopPicCfg;
        h->encIn.gopConfig.pGopPicCfg = h->encIn.gopConfig.pGopPicCfgPass2 = h->gopPicCfgPass2;
    }

    h->encIn.gopConfig.gopLowdelay = h->options.gopLowdelay;

    if (h->options.intraPicRate != 1)
    {
        u32 maxRefPics = 0;
        i32 maxTemporalId = 0;
        int idx;
        for (idx = 0; idx < h->encIn.gopConfig.size; idx ++)
        {
            VCEncGopPicConfig *gop_cfg = &(h->encIn.gopConfig.pGopPicCfg[idx]);
            if (gop_cfg->codingType != VCENC_INTRA_FRAME)
            {
                if (maxRefPics < gop_cfg->numRefPics)
                    maxRefPics = gop_cfg->numRefPics;

                if (maxTemporalId < gop_cfg->temporalId)
                    maxTemporalId = gop_cfg->temporalId;
            }
            //TBD! Is it a potential bug for maxTemporalID???
        }

        h->maxRefPics = maxRefPics;
        h->maxTemporalId = maxTemporalId;
    }
    return 0;
}

static void vce_init_misc(ENCODER_CODEC *ctx) {
  EWLInitParam_t param;
  memset(&param, 0, sizeof(param));

  param.clientType = EWL_CLIENT_TYPE_MEM;  //buffer operation
  ctx->ewlInst = EWLInit(&param);
  ctx->hwCfg = EncGetAsicConfig(ctx->codecFormat, ctx->ewlInst);

  ctx->nIFrameCounter = 0;
  ctx->tile_width = (i32 *)OSAL_Malloc(ctx->options.num_tile_columns * sizeof(i32));
  OSAL_Memset(ctx->tile_width, 0, ctx->options.num_tile_columns * sizeof(i32));
  ctx->tile_height = (i32 *)OSAL_Malloc(ctx->options.num_tile_rows * sizeof(i32));
  OSAL_Memset(ctx->tile_height, 0, ctx->options.num_tile_rows * sizeof(i32));
}

static void vce_release_misc(ENCODER_CODEC *ctx) {
  if (ctx->ewlInst)
    EWLRelease(ctx->ewlInst);
  if (ctx->tile_width)
    OSAL_Free(ctx->tile_width);
  if (ctx->tile_height)
    OSAL_Free(ctx->tile_height);
}

// destroy codec instance
static void encoder_destroy_codec(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;

    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            VCEncRelease(this->instance);
            this->instance = 0;
        }
        vce_release_misc(this);
        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

static CODEC_STATE encoder_stream_start_codec(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    vce_init_pic(this);
    InitPicConfig(this);

    //TBD: only 1 stream output supported
    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;
    this->encIn.vui_timing_info_enable = 1;
    this->nTotalFrames = 0;

    //this->encIn.gopConfig.pGopPicCfg = this->gopPicCfg;

    VCEncRet ret = VCEncStrmStart(this->instance, &this->encIn, &this->encOut);

    switch (ret)
    {
        case VCENC_OK:
            //TBD alen: need to consider NAL UNIT stream type.
            stream->streamlen = this->encOut.streamSize;
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_ERROR:
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }

    //disable it as a temp workaround because
    //  current ctrlsw doesn't handle consumed buffer in VCEncStrmStart() as expected.
//    stream->bUseConsumedBuf = OMX_TRUE;
    stream->consumedBuf[CONSUMED_BUF_ID_PIXEL] = this->encOut.consumedAddr.inputbufBusAddr;
    stream->consumedBuf[CONSUMED_BUF_ID_STREAM] = this->encOut.consumedAddr.outbufBusAddr;

    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_stream_end_codec(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;

    VCEncRet ret = VCEncStrmEnd(this->instance, &this->encIn, &this->encOut);

    switch (ret)
    {
        case VCENC_OK:
            //TBD alen: need to consider NAL-UNIT stream type
            stream->streamlen = this->encOut.streamSize;
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    //disable it as a temp workaround because
    //  current ctrlsw doesn't handle consumed buffer in VCEncStrmEnd() as expected.
//    stream->bUseConsumedBuf = OMX_TRUE;
    stream->consumedBuf[CONSUMED_BUF_ID_PIXEL] = this->encOut.consumedAddr.inputbufBusAddr;
    stream->consumedBuf[CONSUMED_BUF_ID_STREAM] = this->encOut.consumedAddr.outbufBusAddr;

    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_flush_codec(ENCODER_PROTOTYPE* arg,
        STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    VCEncRet ret;

    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;

    ret = VCEncFlush(this->instance, &this->encIn, &this->encOut, NULL, NULL);

    switch (ret)
    {
        case VCENC_FRAME_READY:
            this->nTotalFrames++;
            stream->streamlen = this->encOut.streamSize;

            if (this->encOut.codingType == VCENC_INTRA_FRAME)
            {
                stat = CODEC_CODED_INTRA;
            }
            else if (this->encOut.codingType == VCENC_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else if (this->encOut.codingType == VCENC_BIDIR_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_BIDIR_PREDICTED;
            }
            else if (this->encOut.codingType == VCENC_NOTCODED_FRAME)
            {
                DBGT_PDEBUG("encOut: Not coded frame");
            }
            else
            {
                stat = CODEC_OK;
            }
            break;

        case VCENC_OK:
            stat = CODEC_FLUSH_DONE;
            break;

        case VCENC_FRAME_ENQUEUE:
            stat = CODEC_OK;
            break;

        case VCENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;

        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    stream->bUseConsumedBuf = OMX_TRUE;
    stream->consumedBuf[CONSUMED_BUF_ID_PIXEL] = this->encOut.consumedAddr.inputbufBusAddr;
    stream->consumedBuf[CONSUMED_BUF_ID_STREAM] = this->encOut.consumedAddr.outbufBusAddr;

    DBGT_EPILOG("");
    return stat;
}

#if 0
void _dbg_write_input(FRAME * frame)
{
    static FILE *_dbg_fh_in = NULL;

    if (_dbg_fh_in == NULL) {
        _dbg_fh_in = fopen("dbg_vce_input.yuv", "wb");
        if (!_dbg_fh_in)
        {
            printf("ERROR! Can't open dbg_vce_input.yuv\n");
        }
    }
    if (_dbg_fh_in)
    {
        fwrite(frame->fb_bus_data, frame->fb_frameSize, 1, _dbg_fh_in);
    }
}
#endif

static CODEC_STATE encoder_encode_codec(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    VIDEO_ENCODER_CONFIG* encConf = (VIDEO_ENCODER_CONFIG*) cfg;

    VCEncRet ret;
    VCEncRateCtrl rate_ctrl;
    VCEncCodingCtrl coding_ctrl;
    bool adaptiveGop = (this->options.gopSize == 0);

    //TBD: support 8-bit YUV 420 only, other formats??
    this->encIn.busLuma = frame->fb_bus_address;
    this->encIn.busChromaU = frame->fb_bus_address + (this->options.lumWidthSrc * this->options.lumHeightSrc);
    this->encIn.busChromaV = this->encIn.busChromaU + (this->options.lumWidthSrc * this->options.lumHeightSrc / 4);

    //_dbg_write_input(frame);

    this->encIn.timeIncrement = this->nEstTimeInc;

    this->encIn.pOutBuf[0] = (u32 *) stream->bus_data;
    this->encIn.outBufSize[0] = stream->buf_max_size;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.pOutBuf[1] = NULL;
    this->encIn.outBufSize[1] = 0;
    this->encIn.busOutBuf[1] = 0;

    /*this->encIn.gopSize = 1; // H2v1 supports only size 1 */
    this->encIn.gopConfig.pGopPicCfg = this->gopPicCfg;

    DBGT_PDEBUG("Input timeInc: %u outBufSize: %u, virtual address: %p, bus address: 0x%08lx",
        this->encIn.timeIncrement, this->encIn.outBufSize,
        this->encIn.pOutBuf, this->encIn.busOutBuf);

    this->encIn.codingType = (this->encIn.poc == 0) ? VCENC_INTRA_FRAME : this->nextCodingType;
    this->encIn.bIsIntraOnly = (0 == this->encIn.picture_cnt) ? HANTRO_TRUE : HANTRO_FALSE;

    DBGT_PDEBUG("frame->fb_bus_address 0x%08lx", frame->fb_bus_address);
    DBGT_PDEBUG("Frame type %s, POC %d, IFrame counter (%d/%d)", (this->encIn.codingType == VCENC_INTRA_FRAME) ? "I":"P",
        (int)this->encIn.poc, (int)this->nIFrameCounter, (int)this->nPFrames+1);

    if (this->nTotalFrames == 0)
    {
        this->encIn.timeIncrement = 0;
    }
    else
    {
        this->encIn.timeIncrement = this->options.outputRateDenom;
    }

    ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == VCENC_OK && (this->currBitrate != frame->bitrate))
    {
        rate_ctrl.bitPerSecond = frame->bitrate;
        ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
        this->currBitrate = frame->bitrate;
    }

    DBGT_PDEBUG("rate_ctrl.qpHdr %d", rate_ctrl.qpHdr);
    DBGT_PDEBUG("rate_ctrl.bitPerSecond %d", rate_ctrl.bitPerSecond);

    ret = VCEncGetCodingCtrl(this->instance, &coding_ctrl);

    if (ret == VCENC_OK)
    {
        int new_coding_ctrl = 0;
        if (coding_ctrl.intraArea.enable  != encConf->intraArea.bEnable ||
           ((coding_ctrl.intraArea.top     != encConf->intraArea.nTop ||
            coding_ctrl.intraArea.left    != encConf->intraArea.nLeft ||
            coding_ctrl.intraArea.bottom  != encConf->intraArea.nBottom ||
            coding_ctrl.intraArea.right   != encConf->intraArea.nRight) &&
            encConf->intraArea.bEnable))
        {
            coding_ctrl.intraArea.enable  = encConf->intraArea.bEnable;
            coding_ctrl.intraArea.top     = encConf->intraArea.nTop;
            coding_ctrl.intraArea.left    = encConf->intraArea.nLeft;
            coding_ctrl.intraArea.bottom  = encConf->intraArea.nBottom;
            coding_ctrl.intraArea.right   = encConf->intraArea.nRight;
            DBGT_PDEBUG("Intra area enable %d (%d, %d, %d, %d)",
                coding_ctrl.intraArea.enable, coding_ctrl.intraArea.top,
                coding_ctrl.intraArea.left, coding_ctrl.intraArea.bottom,
                coding_ctrl.intraArea.right);

            new_coding_ctrl = 1;
        }

        if (coding_ctrl.roi1Area.enable  != encConf->roiArea[0].bEnable ||
           ((coding_ctrl.roi1Area.top     != encConf->roiArea[0].nTop ||
            coding_ctrl.roi1Area.left    != encConf->roiArea[0].nLeft ||
            coding_ctrl.roi1Area.bottom  != encConf->roiArea[0].nBottom ||
            coding_ctrl.roi1Area.right   != encConf->roiArea[0].nRight ||
            coding_ctrl.roi1DeltaQp      != encConf->roiDeltaQP[0].nDeltaQP) &&
            encConf->roiArea[0].bEnable))
        {
            coding_ctrl.roi1Area.enable  = encConf->roiArea[0].bEnable;
            coding_ctrl.roi1Area.top     = encConf->roiArea[0].nTop;
            coding_ctrl.roi1Area.left    = encConf->roiArea[0].nLeft;
            coding_ctrl.roi1Area.bottom  = encConf->roiArea[0].nBottom;
            coding_ctrl.roi1Area.right   = encConf->roiArea[0].nRight;
            coding_ctrl.roi1DeltaQp      = encConf->roiDeltaQP[0].nDeltaQP;
            DBGT_PDEBUG("ROI area 1 enable %d (%d, %d, %d, %d)",
                coding_ctrl.roi1Area.enable, coding_ctrl.roi1Area.top,
                coding_ctrl.roi1Area.left, coding_ctrl.roi1Area.bottom,
                coding_ctrl.roi1Area.right);
            DBGT_PDEBUG("ROI 1 delta QP %d", coding_ctrl.roi1DeltaQp);

            new_coding_ctrl = 1;
        }

        if (coding_ctrl.roi2Area.enable  != encConf->roiArea[1].bEnable || 
           ((coding_ctrl.roi2Area.top     != encConf->roiArea[1].nTop ||
            coding_ctrl.roi2Area.left    != encConf->roiArea[1].nLeft ||
            coding_ctrl.roi2Area.bottom  != encConf->roiArea[1].nBottom ||
            coding_ctrl.roi2Area.right   != encConf->roiArea[1].nRight ||
            coding_ctrl.roi2DeltaQp      != encConf->roiDeltaQP[1].nDeltaQP) &&
            encConf->roiArea[1].bEnable))
        {
            coding_ctrl.roi2Area.enable  = encConf->roiArea[1].bEnable;
            coding_ctrl.roi2Area.top     = encConf->roiArea[1].nTop;
            coding_ctrl.roi2Area.left    = encConf->roiArea[1].nLeft;
            coding_ctrl.roi2Area.bottom  = encConf->roiArea[1].nBottom;
            coding_ctrl.roi2Area.right   = encConf->roiArea[1].nRight;
            coding_ctrl.roi2DeltaQp      = encConf->roiDeltaQP[1].nDeltaQP;
            DBGT_PDEBUG("ROI area 2 enable %d (%d, %d, %d, %d)",
                coding_ctrl.roi2Area.enable, coding_ctrl.roi2Area.top,
                coding_ctrl.roi2Area.left, coding_ctrl.roi2Area.bottom,
                coding_ctrl.roi2Area.right);
            DBGT_PDEBUG("ROI 2 delta QP %d", coding_ctrl.roi2DeltaQp);

            new_coding_ctrl = 1;
        }
        if(new_coding_ctrl)
            ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
    }

    ret = VCEncStrmEncode(this->instance, &this->encIn, &this->encOut, NULL, NULL);
    DBGT_PDEBUG("VCEncStrmEncode ret %d, stream size %d\n", ret, this->encOut.streamSize);
    switch (ret)
    {
        case VCENC_FRAME_READY:
            if (this->encOut.codingType != VCENC_NOTCODED_FRAME)
            {
                if (this->encOut.streamSize == 0)
                {
                    ;
                }
            }
            this->encIn.bIsIDR = HANTRO_FALSE;
            this->nTotalFrames++;
//            this->encIn.poc++;
            if (this->encOut.streamSize == 0)
            {
              this->encIn.picture_cnt++;
              stream->next_input_frame_id = this->encIn.picture_cnt;
              stat = CODEC_OK;
              break;
            }
            stream->streamlen = this->encOut.streamSize;

            (void)adaptiveGop;
            this->encIn.picture_cnt++;

            stream->next_input_frame_id = this->encIn.picture_cnt;
            if (this->encOut.codingType == VCENC_INTRA_FRAME)
            {
                stat = CODEC_CODED_INTRA;
            }
            else if (this->encOut.codingType == VCENC_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_PREDICTED;
            }
            else if (this->encOut.codingType == VCENC_BIDIR_PREDICTED_FRAME)
            {
                stat = CODEC_CODED_BIDIR_PREDICTED;
            }
            else if (this->encOut.codingType == VCENC_NOTCODED_FRAME)
            {
                DBGT_PDEBUG("encOut: Not coded frame");
            }
            else
            {
                stat = CODEC_OK;
            }
            break;
        case VCENC_FRAME_ENQUEUE:
            this->nTotalFrames++;
            this->encIn.picture_cnt++;
            this->encIn.bIsIDR = HANTRO_FALSE;
            stream->next_input_frame_id = this->encIn.picture_cnt;
            this->encIn.timeIncrement = this->options.outputRateDenom;
            stat = CODEC_ENQUEUE;
            break;

        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case VCENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            this->encIn.picture_cnt++;
            stream->next_input_frame_id = this->encIn.picture_cnt;
            break;
        case VCENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case VCENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case VCENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case VCENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case VCENC_HW_RESERVED:
            stat = CODEC_ERROR_RESERVED;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }

    stream->bUseConsumedBuf = OMX_TRUE;
    if (this->encOut.codingType == VCENC_NOTCODED_FRAME) {
      stream->consumedBuf[CONSUMED_BUF_ID_PIXEL] =  this->encIn.busLuma;
    } else {
      stream->consumedBuf[CONSUMED_BUF_ID_PIXEL] = this->encOut.consumedAddr.inputbufBusAddr;
    }
    stream->consumedBuf[CONSUMED_BUF_ID_STREAM] = this->encOut.consumedAddr.outbufBusAddr;

    DBGT_EPILOG("");
    return stat;
}

static VCEncVideoCodecFormat vce_init_opts(ENCODER_CODEC *ctx, const CODEC_CONFIG *params) {
  vce_format_options *fmt_opts;
  VCEncVideoCodecFormat vce_fmt;

  memcpy(&ctx->options, &vce_default_opts_video, sizeof(VCE_VIDEO_OPTIONS));
  switch ((u32)params->codecFormat.nCodecFormat) {
    case OMX_VIDEO_CodingHEVC:
      vce_fmt = VCENC_VIDEO_CODEC_HEVC;
      fmt_opts = &vce_opts_hevc;
      break;
    case OMX_VIDEO_CodingAVC:
      vce_fmt = VCENC_VIDEO_CODEC_H264;
      fmt_opts = &vce_opts_h264;
      break;
    case OMX_VIDEO_CodingAV1:
      vce_fmt = VCENC_VIDEO_CODEC_AV1;
      fmt_opts = &vce_opts_av1;
      break;
    default:
      DBGT_CRITICAL("codec format Error!");
      DBGT_EPILOG("");
      break;
  }

  ctx->options.profile = fmt_opts->profile;
  ctx->options.level = fmt_opts->level;
  ctx->options.max_cu_size = fmt_opts->max_cu_size;
  ctx->options.min_cu_size = fmt_opts->min_cu_size;
  ctx->options.max_tr_size = fmt_opts->max_tr_size;
  ctx->options.min_tr_size = fmt_opts->min_tr_size;
  ctx->options.tr_depth_intra = fmt_opts->tr_depth_intra;
  ctx->options.tr_depth_inter = fmt_opts->tr_depth_inter;
  ctx->options.layerInRefIdc = fmt_opts->layerInRefIdc;
  ctx->options.prefixNalSvcFlag = fmt_opts->prefixNalSvcFlag;
  ctx->options.rdoLevel = fmt_opts->rdoLevel;
  ctx->options.svctEnable = fmt_opts->svctEnable;

  ctx->options.extSramLumHeightBwd = fmt_opts->extSramLumHeightBwd;
  ctx->options.extSramChrHeightBwd = fmt_opts->extSramChrHeightBwd;
  ctx->options.extSramLumHeightFwd = fmt_opts->extSramLumHeightFwd;
  ctx->options.extSramChrHeightFwd = fmt_opts->extSramChrHeightFwd;

  return vce_fmt;
}

static void vce_update_opts_video(ENCODER_CODEC *h, const OMX_VIDEO_PARAM_VIDEOTYPE *video_cfg) {
  VCE_VIDEO_OPTIONS *opts = &h->options;

  //private options parsing
  opts->preset = video_cfg->preset;
  if((int)opts->preset != VSI_DEFAULT_VALUE) {
    Parameter_Preset(opts);
  }
  else {
    opts->rdoLevel = video_cfg->rdoLevel;
    opts->enableRdoQuant = video_cfg->enableRdoQuant;
  }
  opts->rdoLevel = CLIP3(1, 3, opts->rdoLevel) - 1;

  h->nPFrames = video_cfg->nPFrames;
  h->nBFrames = video_cfg->nBFrames;
  h->nRefFrames = video_cfg->nRefFrames;

  opts->byteStream = (video_cfg->byteStream) ? VCENC_BYTE_STREAM : VCENC_NAL_UNIT_STREAM;
  opts->ssim = video_cfg->ssim;
  opts->exp_of_input_alignment = video_cfg->exp_of_input_alignment;

  opts->ctbRcMode = video_cfg->hrdCpbSize > 0 ? 2 : 0;
#if 0
  opts->ctbRcMode = (video_cfg->ctbRc != VSI_DEFAULT_VALUE) ? video_cfg->ctbRc : 0;
#endif
  opts->cuInfoVersion = video_cfg->cuInfoVersion;
  opts->picRc = video_cfg->picRc;
  opts->gopSize = MIN(video_cfg->gopSize, MAX_GOP_SIZE);
  opts->gdrDuration = video_cfg->gdrDuration;

  opts->tc_Offset = video_cfg->nTcOffset;
  opts->beta_Offset = video_cfg->nBetaOffset;
  opts->enableDeblockOverride = video_cfg->bEnableDeblockOverride;
  opts->deblockOverride = video_cfg->bDeblockOverride;
  opts->bEnableSAO = video_cfg->bEnableSAO;
  opts->enableScalingList = video_cfg->bEnableScalingList;
  opts->RoiQpDeltaVer = video_cfg->RoiQpDelta_ver;
  opts->roiMapDeltaQpEnable = video_cfg->roiMapDeltaQpEnable;
  opts->roiMapDeltaQpBlockUnit = video_cfg->roiMapDeltaQpBlockUnit;
  opts->intraPicRate = video_cfg->intraPicRate;
  opts->ltrInterval = video_cfg->ltrInterval;
  opts->longTermQpDelta = video_cfg->longTermQpDelta;
  opts->longTermGap = video_cfg->longTermGap;
  opts->longTermGapOffset = video_cfg->longTermGapOffset;
  opts->sliceSize = video_cfg->sliceSize;
  opts->gopLowdelay = video_cfg->gopLowdelay;
  if (opts->gopSize != 1 && !opts->gopLowdelay)
    opts->lowDelayB = 1;
  opts->enableCabac = video_cfg->enableCabac;
  opts->bCabacInitFlag = video_cfg->bCabacInitFlag;
  opts->videoRange = video_cfg->videoRange;
  opts->fieldOrder = video_cfg->fieldOrder;
  opts->cirStart = video_cfg->cirStart;
  opts->cirInterval = video_cfg->cirInterval;
  opts->pcm_loop_filter_disabled_flag = video_cfg->pcm_loop_filter_disabled_flag;
  opts->ipcmMapEnable = video_cfg->ipcmMapEnable;
  memcpy(opts->roiQp, video_cfg->roiQp, sizeof(opts->roiQp));
  opts->chromaQpOffset = video_cfg->chromaQpOffset;
  opts->noiseReductionEnable = video_cfg->noiseReductionEnable;
  opts->noiseLow = video_cfg->noiseLow;
  opts->firstFrameSigma = video_cfg->noiseFirstFrameSigma;
  opts->num_tile_columns = video_cfg->num_tile_columns;
  opts->num_tile_rows = video_cfg->num_tile_rows;
  opts->loop_filter_across_tiles_enabled_flag = video_cfg->loop_filter_across_tiles_enabled_flag;
  opts->tiles_enabled_flag = (opts->num_tile_columns * opts->num_tile_rows) > 1;
  /* HDR10 */
  opts->hdr10_display_enable = video_cfg->hdr10_display_enable;
  opts->hdr10_dx0 = video_cfg->hdr10_dx0;
  opts->hdr10_dy0 = video_cfg->hdr10_dy0;
  opts->hdr10_dx1 = video_cfg->hdr10_dx1;
  opts->hdr10_dy1 = video_cfg->hdr10_dy1;
  opts->hdr10_dx2 = video_cfg->hdr10_dx2;
  opts->hdr10_dy2 = video_cfg->hdr10_dy2;
  opts->hdr10_wx = video_cfg->hdr10_wx;
  opts->hdr10_wy = video_cfg->hdr10_wy;
  opts->hdr10_maxluma = video_cfg->hdr10_maxluma;
  opts->hdr10_minluma = video_cfg->hdr10_minluma;
  opts->hdr10_lightlevel_enable = video_cfg->hdr10_lightlevel_enable;
  opts->hdr10_maxlight = video_cfg->hdr10_maxlight;
  opts->hdr10_avglight = video_cfg->hdr10_avglight;
  opts->vuiColorDescripPresentFlag = video_cfg->hdr10_color_enable;
  opts->vuiColorPrimaries = video_cfg->hdr10_primary;
  opts->vuiTransferCharacteristics = video_cfg->hdr10_transfer;
  opts->vuiMatrixCoefficients = video_cfg->hdr10_matrix;
  opts->RpsInSliceHeader = video_cfg->RpsInSliceHeader;
  opts->blockRCSize = video_cfg->blockRCSize;
  opts->rcQpDeltaRange = video_cfg->rcQpDeltaRange;
  opts->rcBaseMBComplexity = video_cfg->rcBaseMBComplexity;
  opts->picQpDeltaMin = video_cfg->picQpDeltaMin;
  opts->picQpDeltaMax = video_cfg->picQpDeltaMax;
  opts->bitVarRangeI = video_cfg->bitVarRangeI;
  opts->bitVarRangeP = video_cfg->bitVarRangeP;
  opts->bitVarRangeB = video_cfg->bitVarRangeB;
  opts->tolMovingBitRate = video_cfg->tolMovingBitRate;
  opts->tolCtbRcInter = video_cfg->tolCtbRcInter;
  opts->tolCtbRcIntra = video_cfg->tolCtbRcIntra;
  opts->ctbRcRowQpStep = video_cfg->ctbRowQpStep;
  if(video_cfg->monitorFrames != VSI_DEFAULT_VALUE)
    opts->monitorFrames = video_cfg->monitorFrames;
  else
    opts->monitorFrames = (h->options.outputRateNumer + h->options.outputRateDenom - 1) /
                          h->options.outputRateDenom;

  opts->bitrateWindow = video_cfg->bitrateWindow;
  opts->intraQpDelta = video_cfg->intraQpDelta;
  opts->fixedIntraQp = video_cfg->fixedIntraQp;
  opts->vbr = video_cfg->vbr;
  opts->smoothPsnrInGOP = video_cfg->smoothPsnrInGOP;
  opts->u32StaticSceneIbitPercent = video_cfg->staticSceneIbitPercent;
  opts->picSkip = video_cfg->picSkip;
  opts->vui_timing_info_enable = video_cfg->vui_timing_info_enable;
  opts->hashtype = video_cfg->hashtype;
  opts->firstPic = video_cfg->firstPic;
  opts->lastPic = video_cfg->lastPic;
  opts->extSramLumHeightBwd = video_cfg->extSramLumHeightBwd;
  opts->extSramChrHeightBwd = video_cfg->extSramChrHeightBwd;
  opts->extSramLumHeightFwd = video_cfg->extSramLumHeightFwd;
  opts->extSramChrHeightFwd = video_cfg->extSramChrHeightFwd;
  opts->AXIAlignment = video_cfg->AXIAlignment;
  if(video_cfg->codedChromaIdc == 0)
    opts->codedChromaIdc = VCENC_CHROMA_IDC_400;
  else if(video_cfg->codedChromaIdc == 2)
    opts->codedChromaIdc = VCENC_CHROMA_IDC_422;
  else
    opts->codedChromaIdc = VCENC_CHROMA_IDC_420;

  opts->aq_mode = video_cfg->aq_mode;
  opts->aq_strength = Q16_FLOAT(video_cfg->aq_strength);
  opts->writeReconToDDR = video_cfg->writeReconToDDR;
  opts->TxTypeSearchEnable = video_cfg->TxTypeSearchEnable;
  opts->psyFactor = video_cfg->PsyFactor;
  opts->MEVertRange = video_cfg->meVertSearchRange;
  opts->layerInRefIdc = video_cfg->layerInRefIdcEnable;
  opts->crf = video_cfg->crf;
  opts->cpbSize = video_cfg->hrdCpbSize;
  switch (h->eRateControl) {
    case OMX_Video_ControlRateDisable:
        opts->picSkip = 0;
        break;
    case OMX_Video_ControlRateVariable:
        opts->picSkip = 0;
        break;
    case OMX_Video_ControlRateConstant:
        opts->picSkip = 0;
        break;
    case OMX_Video_ControlRateVariableSkipFrames:
        opts->picSkip = 1;
        break;
    case OMX_Video_ControlRateConstantSkipFrames:
        opts->picSkip = 1;
        break;
    case OMX_Video_ControlRateMax:
        opts->picSkip = 0;
        break;
    default:
        opts->picSkip = 0;
        break;
  }
  vce_parse_rcMode(h, video_cfg->rcMode);
}

static int vce_update_opts_from_hevc_cfg(ENCODER_CODEC *h, const OMX_VIDEO_PARAM_HEVCTYPE *hevc_cfg) {
  VCE_VIDEO_OPTIONS *opts = &h->options;

  if(vce_parse_hevc_profile(hevc_cfg->eProfile, &opts->profile))
    return -1;

  if(vce_parse_hevc_level(hevc_cfg->eLevel, &opts->level))
    return -1;

  vce_update_opts_video(h, hevc_cfg);
  return 0;
}

static int vce_update_opts_from_avc_cfg(ENCODER_CODEC *h, const OMX_VIDEO_PARAM_AVCTYPE *avc_cfg,
                                          const OMX_VIDEO_PARAM_AVCEXTTYPE *avc_ext_cfg) {
  VCE_VIDEO_OPTIONS *opts = &h->options;

  if(vce_parse_avc_profile(avc_cfg->eProfile, &opts->profile))
    return -1;

  if(vce_parse_avc_level(avc_cfg->eLevel, &opts->level))
    return -1;

  //private options parsing
  opts->preset = avc_ext_cfg->preset;
  if((int)opts->preset != VSI_DEFAULT_VALUE)
      Parameter_Preset(opts);
  else
    opts->rdoLevel = avc_ext_cfg->rdoLevel;

  h->nPFrames = avc_cfg->nPFrames;
  h->nBFrames = avc_cfg->nBFrames;
  h->nRefFrames = avc_cfg->nRefFrames;

  opts->byteStream = VCENC_BYTE_STREAM;
  opts->rdoLevel = CLIP3(1, 3, opts->rdoLevel) - 1;
  opts->gopSize = MIN(avc_ext_cfg->gopSize, MAX_GOP_SIZE);
  opts->bEnableSAO = avc_cfg->bEnableASO;
  opts->intraPicRate = avc_cfg->nPFrames;
  opts->enableCabac = avc_cfg->bEntropyCodingCABAC;

  opts->monitorFrames = (h->options.outputRateNumer + h->options.outputRateDenom - 1) / h->options.outputRateDenom;

  switch (h->eRateControl) {
    case OMX_Video_ControlRateDisable:
        opts->picSkip = 0;
        break;
    case OMX_Video_ControlRateVariable:
        opts->picSkip = 0;
        break;
    case OMX_Video_ControlRateConstant:
        opts->picSkip = 0;
        break;
    case OMX_Video_ControlRateVariableSkipFrames:
        opts->picSkip = 1;
        break;
    case OMX_Video_ControlRateConstantSkipFrames:
        opts->picSkip = 1;
        break;
    case OMX_Video_ControlRateMax:
        opts->picSkip = 0;
        break;
    default:
        opts->picSkip = 0;
        break;
  }
  opts->firstPic = avc_ext_cfg->firstPic;
  opts->lastPic = avc_ext_cfg->lastPic;

  opts->extSramLumHeightBwd = avc_ext_cfg->extSramLumHeightBwd;
  opts->extSramChrHeightBwd = avc_ext_cfg->extSramChrHeightBwd;
  opts->extSramLumHeightFwd = avc_ext_cfg->extSramLumHeightFwd;
  opts->extSramChrHeightFwd = avc_ext_cfg->extSramChrHeightFwd;
  opts->AXIAlignment = avc_ext_cfg->AXIAlignment;
  opts->exp_of_input_alignment = avc_ext_cfg->exp_of_input_alignment;
  if(avc_ext_cfg->codedChromaIdc == 0)
      opts->codedChromaIdc = VCENC_CHROMA_IDC_400;
  else if(avc_ext_cfg->codedChromaIdc == 2)
      opts->codedChromaIdc = VCENC_CHROMA_IDC_422;
  else
      opts->codedChromaIdc = VCENC_CHROMA_IDC_420;
  opts->aq_mode = avc_ext_cfg->aq_mode;
  opts->aq_strength = Q16_FLOAT(avc_ext_cfg->aq_strength);
  opts->writeReconToDDR = avc_ext_cfg->writeReconToDDR;
  opts->TxTypeSearchEnable = avc_ext_cfg->TxTypeSearchEnable;
  opts->psyFactor = avc_ext_cfg->PsyFactor;
  opts->MEVertRange = avc_ext_cfg->meVertSearchRange;
  opts->layerInRefIdc = avc_ext_cfg->layerInRefIdcEnable;
  opts->crf = avc_ext_cfg->crf;
  opts->tolMovingBitRate = avc_ext_cfg->tolMovingBitRate;
  opts->cpbSize = avc_ext_cfg->hrdCpbSize;
  vce_parse_rcMode(h, avc_ext_cfg->rcMode);

  return 0;
}

static int vce_update_opts_from_av1_cfg(ENCODER_CODEC *h, const OMX_VIDEO_PARAM_AV1TYPE *av1_cfg) {
  VCE_VIDEO_OPTIONS *opts = &h->options;

  if(vce_parse_av1_profile(av1_cfg->eProfile, &opts->profile))
    return -1;

  if(vce_parse_av1_level(av1_cfg->eLevel, &opts->level))
    return -1;

  vce_update_opts_video(h, av1_cfg);
  return 0;
}
// update configurations to instance
static int vce_update_opts(ENCODER_CODEC *h, const CODEC_CONFIG* params)
{
    int ret = 0, i;
    VCE_VIDEO_OPTIONS *opts = &h->options;

    opts->width = params->common_config.nOutputWidth;
    opts->height = params->common_config.nOutputHeight;
    opts->lumWidthSrc = params->pp_config.origWidth;
    opts->lumHeightSrc = params->pp_config.origHeight;
    opts->outputRateNumer = Q16_FLOAT(params->common_config.nOutputFramerate);
    opts->inputRateNumer = Q16_FLOAT(params->common_config.nInputFramerate);
    opts->bitDepthLuma = params->common_config.nBitDepthLuma;
    opts->bitDepthChroma = params->common_config.nBitDepthChroma;
    opts->bitPerSecond = params->rate_config.nTargetBitrate;
    opts->nQpMin = params->rate_config.nQpMin;
    opts->nQpMax = params->rate_config.nQpMax;
    opts->hrdConformance = params->rate_config.nHrdEnabled;
    opts->bDisableDeblocking = params->bDisableDeblocking;
    opts->bSeiMessages = params->bSeiMessages;
    if (params->intraArea.bEnable) {
      opts->intraArea.enable = 1;
      opts->intraArea.top = params->intraArea.nTop;
      opts->intraArea.left = params->intraArea.nLeft;
      opts->intraArea.bottom = params->intraArea.nBottom;
      opts->intraArea.right = params->intraArea.nRight;
    }
    for (i=0; i<MAX_IPCM_AREA; i++) {
      if (params->ipcmArea[i].bEnable) {
        opts->ipcmArea[i].enable = 1;
        opts->ipcmArea[i].top = params->ipcmArea[i].nTop;
        opts->ipcmArea[i].left = params->ipcmArea[i].nLeft;
        opts->ipcmArea[i].bottom = params->ipcmArea[i].nBottom;
        opts->ipcmArea[i].right = params->ipcmArea[i].nRight;
      }
    }
    for (i=0; i<MAX_ROI_AREA; i++) {
      if (params->roiArea[i].bEnable) {
        opts->roiArea[i].enable = 1;
        opts->roiArea[i].top = params->roiArea[i].nTop;
        opts->roiArea[i].left = params->roiArea[i].nLeft;
        opts->roiArea[i].bottom = params->roiArea[i].nBottom;
        opts->roiArea[i].right = params->roiArea[i].nRight;
        opts->roiDeltaQp[i] = params->roiDeltaQP[i].nDeltaQP;
      }
    }

    h->nPictureRcEnabled = params->rate_config.nPictureRcEnabled;
    h->eRateControl = params->rate_config.eRateControl;
    h->nEstTimeInc = opts->outputRateDenom;
    h->nMaxTLayers = params->common_config.nMaxTLayers;

    if(h->codecFormat == VCENC_VIDEO_CODEC_HEVC)
      ret = vce_update_opts_from_hevc_cfg(h, &params->configs.hevc_config);
    else if(h->codecFormat == VCENC_VIDEO_CODEC_H264)
      ret = vce_update_opts_from_avc_cfg(h, &params->configs.avc_config, &params->avc_ext_config);
    else if(h->codecFormat == VCENC_VIDEO_CODEC_AV1)
      ret = vce_update_opts_from_av1_cfg(h, &params->configs.av1_config);

    return ret;
}

static void vce_prepare_enc_cfg(const ENCODER_CODEC* this, VCEncConfig *cfg) {
  i32 tmvpSupport;

  cfg->codecFormat = this->codecFormat;
  cfg->parallelCoreNum = this->options.parallelCoreNum;
  cfg->codedChromaIdc = this->options.codedChromaIdc;
  /* VP9 support TMVP; HEVC/AV1 support TMVP if hw Support,
    if TMVP is not set by user, it's default setting depends on HW capability */
  if (this->options.enableTMVP == DEFAULT) {
    tmvpSupport = IS_VP9(cfg->codecFormat) ||
                  (this->hwCfg->hevcTemporalMvpSupport && IS_HEVC(cfg->codecFormat) &&
                   /* Disable TMVP when encoding YUV444 */
                   cfg->codedChromaIdc != VCENC_CHROMA_IDC_444) ||
                  ((this->hwCfg->av1TemporalMvpSupport) && IS_AV1(cfg->codecFormat));
    cfg->enableTMVP = tmvpSupport;
  } else
    cfg->enableTMVP = this->options.enableTMVP;

  if (cfg->enableTMVP == 1 && cfg->parallelCoreNum > 1) {
    if(this->hwCfg->tmvpMcSupport == 0) {
      cfg->enableTMVP = 0;
      DBGT_WARNING("Warning: HW does not support tmvp and multi core at the same time!");
    }
  }

  cfg->streamType = this->options.byteStream;
  cfg->profile = this->options.profile;
  cfg->tier = this->options.tier;
  cfg->level = this->options.level;

  cfg->maxTLayers = this->nMaxTLayers;

  /* Find the max number of reference frame */
  if (this->options.intraPicRate == 1) {
    cfg->refFrameAmount = 0;
    if (this->options.noiseReductionEnable)
      cfg->refFrameAmount = 1;
  } else {
    u32 maxRefPics = 0;
    i32 maxTemporalId = 0;
    int idx;
    for (idx = 0; idx < this->encIn.gopConfig.size; idx++) {
      VCEncGopPicConfig *cfg = &(this->encIn.gopConfig.pGopPicCfg[idx]);
      if (cfg->codingType != VCENC_INTRA_FRAME) {
        if (maxRefPics < cfg->numRefPics)
          maxRefPics = cfg->numRefPics;
        if (maxTemporalId < cfg->temporalId) maxTemporalId = cfg->temporalId;
      }
    }
    for (idx = 0; idx < this->encIn.gopConfig.special_size; idx++) {
      VCEncGopPicSpecialConfig *cfg = &(this->encIn.gopConfig.pGopPicSpecialCfg[idx]);

      if ((cfg->temporalId != TEMPORALID_RESERVED) && (maxTemporalId < cfg->temporalId))
        maxTemporalId = cfg->temporalId;
    }
    cfg->refFrameAmount =
        maxRefPics + this->options.interlacedFrame + this->encIn.gopConfig.ltrcnt;
    cfg->maxTLayers = maxTemporalId + 1;
    if (maxTemporalId > 1) {
      cfg->refFrameAmount++;
    }
    // if (cml->flexRefs != NULL) {
    //   cfg->refFrameAmount = 4;
    //   cfg->maxTLayers = 4;
    // }
  }

  cfg->gopSize = this->options.gopSize;
  cfg->gopMaxBSize = DEFAULT;
  cfg->maxTLayers = (this->options.intraPicRate == 1) ? 1 : this->maxTemporalId + 1;
  cfg->strongIntraSmoothing = this->options.strong_intra_smoothing_enabled_flag;
  cfg->width = this->options.width;
  cfg->height = this->options.height;
  cfg->frameRateNum = this->options.outputRateNumer;
  cfg->frameRateDenom = this->options.outputRateDenom;
  cfg->bitDepthLuma = this->options.bitDepthLuma;
  cfg->bitDepthChroma = this->options.bitDepthChroma;
  cfg->enableSsim = this->options.ssim;
  cfg->rdoLevel = this->options.rdoLevel;
  cfg->ctbRcMode = this->options.ctbRcMode;
  cfg->bPass1AdaptiveGop = (this->options.gopSize == 0);
  cfg->cuInfoVersion = this->options.cuInfoVersion;
  cfg->numRefP = 1;
  cfg->interlacedFrame = this->options.interlacedFrame;
  cfg->enableOutputCuInfo = this->options.enableOutputCuInfo;
  cfg->exp_of_input_alignment = this->options.exp_of_input_alignment;
  cfg->exp_of_ref_alignment = this->options.exp_of_ref_alignment;
  cfg->exp_of_ref_ch_alignment = this->options.exp_of_ref_ch_alignment;
  cfg->exp_of_aqinfo_alignment = this->options.exp_of_aqinfo_alignment;
  cfg->P010RefEnable = this->options.P010RefEnable;
  cfg->picOrderCntType = this->options.picOrderCntType;
  cfg->log2MaxPicOrderCntLsb = this->options.log2MaxPicOrderCntLsb;
  cfg->log2MaxFrameNum = this->options.log2MaxFrameNum;
  cfg->dumpRegister = this->options.dumpRegister;
  cfg->rasterscan = this->options.rasterscan;
  cfg->pass = this->options.lookaheadDepth ? 2 : 0;
  cfg->extDSRatio = (this->options.lookaheadDepth && this->options.inLoopDSRatio) ? 1 : 0;
  cfg->lookaheadDepth = this->options.lookaheadDepth;
  cfg->irqTypeMask = this->options.irqTypeMask;
  cfg->irqTypeCutreeMask = this->options.irqTypeCutreeMask;
  cfg->compressor = this->options.compressor;
  //cfg->core_mask = this->options.coreMask;
  cfg->aifEnable = this->options.aifEnable;
  cfg->enc_mode.batch_flag = this->options.batchEnable;
  // extension config for new version
  cfg->extSramLumHeightBwd = this->options.extSramLumHeightBwd;
  cfg->extSramChrHeightBwd = this->options.extSramChrHeightBwd;
  cfg->extSramLumHeightFwd = this->options.extSramLumHeightFwd;
  cfg->extSramChrHeightFwd = this->options.extSramChrHeightFwd;
  cfg->AXIAlignment = this->options.AXIAlignment;

  cfg->enablePsnr = this->options.psnr;
  cfg->av1InterFiltSwitch = this->options.av1InterFiltSwitch;
  cfg->burstMaxLength = this->options.burstMaxLength;
  if (this->options.lookaheadDepth)
    cfg->inLoopDSRatio = 1;
  else
    cfg->inLoopDSRatio = 0;

  cfg->writeReconToDDR = this->options.writeReconToDDR;
  cfg->TxTypeSearchEnable = this->options.TxTypeSearchEnable;
  /* tile */
  cfg->tiles_enabled_flag = this->options.tiles_enabled_flag && !IS_H264(this->codecFormat);
  cfg->num_tile_columns = this->options.num_tile_columns;
  cfg->num_tile_rows = this->options.num_tile_rows;
  cfg->loop_filter_across_tiles_enabled_flag = this->options.loop_filter_across_tiles_enabled_flag;
  if (cfg->parallelCoreNum > 1 &&
     (cfg->width * cfg->height < 256 * 256 || cfg->num_tile_columns > 1)) {
    DBGT_WARNING("Disable multicore for small resolution or multi-tile-columns.");
    cfg->parallelCoreNum = 1;
  }
  cfg->tile_width = this->tile_width;
  cfg->tile_height = this->tile_height;

  i32 tileId;
  u32 log2_ctb_size = IS_H264(this->codecFormat) ? 4 : 6;
  u32 ctbPerRow = ((cfg->width + (1 << log2_ctb_size) - 1) >> log2_ctb_size);
  u32 ctbPerColumn = ((cfg->height + (1 << log2_ctb_size) - 1) >> log2_ctb_size);
  for (tileId = 0; tileId < cfg->num_tile_columns; tileId++)
    cfg->tile_width[tileId] = (tileId + 1) * ctbPerRow / cfg->num_tile_columns -
                              (tileId * ctbPerRow) / cfg->num_tile_columns;

  for (tileId = 0; tileId < cfg->num_tile_rows; tileId++)
    cfg->tile_height[tileId] = (tileId + 1) * ctbPerColumn / cfg->num_tile_rows -
                               (tileId * ctbPerColumn) / cfg->num_tile_rows;
}

static void vce_prepare_coding_ctrl(const ENCODER_CODEC* this, VCEncCodingCtrl *coding_ctrl) {
  u32 i;

  if (this->options.sliceSize != 0)
    coding_ctrl->sliceSize = this->options.sliceSize;
  if (this->options.enableCabac != 0)
    coding_ctrl->enableCabac = this->options.enableCabac;
  coding_ctrl->cabacInitFlag = this->options.bCabacInitFlag ? 1 : 0;

  coding_ctrl->vuiVideoFullRange = 0;
  if (this->options.videoRange != 0)
    coding_ctrl->vuiVideoFullRange = this->options.videoRange;

  if (this->options.enableRdoQuant != 0)
    coding_ctrl->enableRdoQuant = this->options.enableRdoQuant;

  coding_ctrl->enableScalingList = this->options.enableScalingList;
  //    coding_ctrl->cabacInitFlag = this->options.bCabacInitFlag;
  coding_ctrl->sramPowerdownDisable = this->options.sramPowerdownDisable;
  if(coding_ctrl->sramPowerdownDisable == 0) {
    coding_ctrl->sramPowerdownMode = 0;
    coding_ctrl->sramPowerdownTimerDiv32 = 96; //8'h60
  }
  coding_ctrl->disableDeblockingFilter = this->options.bDisableDeblocking;
  coding_ctrl->tc_Offset = this->options.tc_Offset;
  coding_ctrl->beta_Offset = this->options.beta_Offset;
  coding_ctrl->enableSao = this->options.bEnableSAO;
  coding_ctrl->enableDeblockOverride = this->options.enableDeblockOverride;
  coding_ctrl->deblockOverride = this->options.deblockOverride;
  coding_ctrl->enableDynamicRdo = this->options.dynamicRdoEnable;
  coding_ctrl->dynamicRdoCu16Bias = this->options.dynamicRdoCu16Bias;
  coding_ctrl->dynamicRdoCu16Factor = this->options.dynamicRdoCu16Factor;
  coding_ctrl->dynamicRdoCu32Bias = this->options.dynamicRdoCu32Bias;
  coding_ctrl->dynamicRdoCu32Factor = this->options.dynamicRdoCu32Factor;

  coding_ctrl->seiMessages = this->options.bSeiMessages;
  coding_ctrl->gdrDuration = this->options.gdrDuration;
  coding_ctrl->fieldOrder = this->options.fieldOrder;

  coding_ctrl->cirStart = this->options.cirStart;
  coding_ctrl->cirInterval = this->options.cirInterval;

  coding_ctrl->intraArea.top = this->options.intraArea.top;
  coding_ctrl->intraArea.left = this->options.intraArea.left;
  coding_ctrl->intraArea.bottom = this->options.intraArea.bottom;
  coding_ctrl->intraArea.right = this->options.intraArea.right;
  coding_ctrl->intraArea.enable = _CheckArea(&coding_ctrl->intraArea, this);

  coding_ctrl->pcm_loop_filter_disabled_flag = this->options.pcm_loop_filter_disabled_flag;

  VCEncPictureArea *ipcm_area_ptr[MAX_IPCM_AREA] = {&coding_ctrl->ipcm1Area,
                                                    &coding_ctrl->ipcm2Area,
                                                    &coding_ctrl->ipcm3Area,
                                                    &coding_ctrl->ipcm4Area,
                                                    &coding_ctrl->ipcm5Area,
                                                    &coding_ctrl->ipcm6Area,
                                                    &coding_ctrl->ipcm7Area,
                                                    &coding_ctrl->ipcm8Area
                                                  };
  for (i = 0; i < MAX_IPCM_AREA; i++) {
    *ipcm_area_ptr[i] = this->options.ipcmArea[i];
    ipcm_area_ptr[i]->enable = _CheckArea(ipcm_area_ptr[i], this);
    coding_ctrl->pcm_enabled_flag |= ipcm_area_ptr[i]->enable;

    DBGT_PDEBUG("IPCM area %d enabled (%d, %d, %d, %d)", i,
        coding_ctrl->ipcm1Area.top, coding_ctrl->ipcm1Area.left,
        coding_ctrl->ipcm1Area.bottom, coding_ctrl->ipcm1Area.right);
  }
  coding_ctrl->ipcmMapEnable = this->options.ipcmMapEnable;
  coding_ctrl->pcm_enabled_flag |= this->options.ipcmMapEnable;

  VCEncPictureArea *roi_ptr[MAX_ROI_AREA] = {&coding_ctrl->roi1Area,
                                              &coding_ctrl->roi2Area,
                                              &coding_ctrl->roi3Area,
                                              &coding_ctrl->roi4Area,
                                              &coding_ctrl->roi5Area,
                                              &coding_ctrl->roi6Area,
                                              &coding_ctrl->roi7Area,
                                              &coding_ctrl->roi8Area,
                                            };

  i32 *roi_deltaQP_ptr[MAX_ROI_AREA] = {&coding_ctrl->roi1DeltaQp,
                                        &coding_ctrl->roi2DeltaQp,
                                        &coding_ctrl->roi3DeltaQp,
                                        &coding_ctrl->roi4DeltaQp,
                                        &coding_ctrl->roi5DeltaQp,
                                        &coding_ctrl->roi6DeltaQp,
                                        &coding_ctrl->roi7DeltaQp,
                                        &coding_ctrl->roi8DeltaQp,
                                       };

  i32 *roi_QP_ptr[MAX_ROI_AREA] = {&coding_ctrl->roi1Qp,
                                    &coding_ctrl->roi2Qp,
                                    &coding_ctrl->roi3Qp,
                                    &coding_ctrl->roi4Qp,
                                    &coding_ctrl->roi5Qp,
                                    &coding_ctrl->roi6Qp,
                                    &coding_ctrl->roi7Qp,
                                    &coding_ctrl->roi8Qp,
                                  };
  for(i=0; i<MAX_ROI_AREA; i++) {
    *roi_ptr[i] = this->options.roiArea[i];
    *roi_deltaQP_ptr[i] = this->options.roiDeltaQp[i];
    *roi_QP_ptr[i] = this->options.roiQp[i];
    roi_ptr[i]->enable = _CheckArea(roi_ptr[i], this);
  }

  VCEncPictureArea *rect_ptr[MAX_ROI_AREA] = {&coding_ctrl->rect0,
                                              &coding_ctrl->rect1,
                                              &coding_ctrl->rect2,
                                              &coding_ctrl->rect3,
                                              &coding_ctrl->rect4,
                                              &coding_ctrl->rect5,
                                              &coding_ctrl->rect6,
                                              &coding_ctrl->rect7,
                                            };
  for (i = 0; i < MAX_ROI_AREA; i++) {
    *rect_ptr[i] = this->options.rect[i];
    rect_ptr[i]->enable = (_CheckArea(rect_ptr[i], this) && this->options.rect[i].enable);
  }

  //TBD alen: need to consider how to pass file handler.
  coding_ctrl->RoimapCuCtrl_index_enable = 0;
  coding_ctrl->RoimapCuCtrl_enable       = 0;
  coding_ctrl->roiMapDeltaQpEnable = this->options.roiMapDeltaQpEnable;
  coding_ctrl->roiMapDeltaQpBlockUnit = this->options.roiMapDeltaQpBlockUnit;

  if (this->options.lookaheadDepth) {
    coding_ctrl->roiMapDeltaQpEnable = 1;
    coding_ctrl->roiMapDeltaQpBlockUnit =
            IS_AV1(this->codecFormat) ? 0 : MAX(1, this->options.roiMapDeltaQpBlockUnit);
  }
  coding_ctrl->RoimapCuCtrl_ver = this->options.RoiCuCtrlVer;
  coding_ctrl->RoiQpDelta_ver   = this->options.RoiQpDeltaVer; // only valid when roiMapInfoBinFile is enabled;

  coding_ctrl->chroma_qp_offset = this->options.chromaQpOffset;

  /* denoise */
  coding_ctrl->noiseReductionEnable = this->options.noiseReductionEnable;
  coding_ctrl->noiseLow = CLIP3(1, 30, this->options.noiseLow);
  coding_ctrl->firstFrameSigma = CLIP3(1, 30, this->options.firstFrameSigma);

  // only support hardware handshaking.
  /* low latency */
  ASSERT(this->options.inputLineBufMode == 0 ||
         this->options.inputLineBufMode == 2 ||
         this->options.inputLineBufMode == 4);
  coding_ctrl->inputLineBufEn = this->options.inputLineBufMode;
  if (coding_ctrl->inputLineBufEn == 2)
    coding_ctrl->inputLineBufLoopBackEn = 1;
  else
    coding_ctrl->inputLineBufLoopBackEn = 0;
  coding_ctrl->inputLineBufDepth = this->options.inputLineBufDepth;
  coding_ctrl->amountPerLoopBack = this->options.amountPerLoopBack;
  if (coding_ctrl->inputLineBufEn == 2 || coding_ctrl->inputLineBufEn == 4)
    coding_ctrl->inputLineBufHwModeEn = 1;
  else
    coding_ctrl->inputLineBufHwModeEn = 0;
  coding_ctrl->inputLineBufCbFunc = NULL; // not support software handshaking
  coding_ctrl->inputLineBufCbData = NULL;

  /* SBI stream multi-segment */
  coding_ctrl->streamMultiSegmentMode = 0;
  coding_ctrl->streamMultiSegmentAmount = 0;
  coding_ctrl->streamMultiSegCbFunc = NULL;
  coding_ctrl->streamMultiSegCbData = NULL;
  //TODO lxj: parse from options
  {
    if (1)
      coding_ctrl->noiseSigmaY = coding_ctrl->noiseSigmaU = coding_ctrl->noiseSigmaV = 10;
    else
      coding_ctrl->noiseSigmaY = coding_ctrl->noiseSigmaU = coding_ctrl->noiseSigmaV = CLIP3(1, 30, 10);
    coding_ctrl->firstFrameSigma = coding_ctrl->noiseSigmaY; // save to firstFrameSigma also

    /* DENOISE 3DNR NLM */
    coding_ctrl->noiseReductionStrength_IntraY = 7;
    coding_ctrl->noiseReductionStrength_IntraU = 7;
    coding_ctrl->noiseReductionStrength_IntraV = 7;
    coding_ctrl->noiseReductionStrength_InterY = 7;
    coding_ctrl->noiseReductionStrength_InterU = 7;
    coding_ctrl->noiseReductionStrength_InterV = 7;
    coding_ctrl->noiseReduction_ChromaMaxMV = 4;

    coding_ctrl->intraReconEnable = 1;

    coding_ctrl->segmentUnitHeight = 16;
    coding_ctrl->lowlatGatingDisable = 1;
    coding_ctrl->lowlatGatingType = 0;
    coding_ctrl->lowlatGatingCyc = IS_H264(this->codecFormat) ? 7 : 31;

    /* ctbRc Mode1 config */
    coding_ctrl->ctbRcSkinQPDelta = 2;
    coding_ctrl->ctbRcSkinMinQPDelta = 3;
    coding_ctrl->ctbRcSkinCbMin = coding_ctrl->ctbRcSkinCbMax = 0;
    coding_ctrl->ctbRcSkinCrMin = coding_ctrl->ctbRcSkinCrMax = 0;
    coding_ctrl->ctbRcSkinLumMin = 0;
    coding_ctrl->ctbRcSkinLumMax = 255;
    coding_ctrl->ctbRcTrailStrengthMax = MIN(0, 3);
    coding_ctrl->ctbRcTrailDeltaQp = CLIP3(-7, 0, -4);
    coding_ctrl->ctbRcDirection = 8;
    for (i = 0; i < CTBRC_THRESHOLD_NUM; i++) {
      coding_ctrl->ctbRcThresholdI[i] = ctbRcThresholdI[i];
      coding_ctrl->ctbRcThresholdP[i] = ctbRcThresholdP[i];
      coding_ctrl->ctbRcThresholdB[i] = ctbRcThresholdB[i];
    }

    /* visual quality prior mode decision */
    coding_ctrl->visualBitRateTolerance = 50;
    coding_ctrl->IntraBiasChromaStrength = 0;
    coding_ctrl->IntraBiasStrength = 0;
    coding_ctrl->IntraBiasMvThreshold = 5;
    coding_ctrl->bTrailAvoidIntraBias = 1;
  }

  coding_ctrl->smartModeEnable = this->options.smartModeEnable;

  /* HDR 10 */
  coding_ctrl->Hdr10Display.hdr10_display_enable = this->options.hdr10_display_enable;
  if (this->options.hdr10_display_enable) {
    coding_ctrl->Hdr10Display.hdr10_dx0 = this->options.hdr10_dx0;
    coding_ctrl->Hdr10Display.hdr10_dy0 = this->options.hdr10_dy0;
    coding_ctrl->Hdr10Display.hdr10_dx1 = this->options.hdr10_dx1;
    coding_ctrl->Hdr10Display.hdr10_dy1 = this->options.hdr10_dy1;
    coding_ctrl->Hdr10Display.hdr10_dx2 = this->options.hdr10_dx2;
    coding_ctrl->Hdr10Display.hdr10_dy2 = this->options.hdr10_dy2;
    coding_ctrl->Hdr10Display.hdr10_wx  = this->options.hdr10_wx;
    coding_ctrl->Hdr10Display.hdr10_wy  = this->options.hdr10_wy;
    coding_ctrl->Hdr10Display.hdr10_maxluma = this->options.hdr10_maxluma;
    coding_ctrl->Hdr10Display.hdr10_minluma = this->options.hdr10_minluma;
  }

  coding_ctrl->Hdr10LightLevel.hdr10_lightlevel_enable = this->options.hdr10_lightlevel_enable;
  if (this->options.hdr10_lightlevel_enable) {
    coding_ctrl->Hdr10LightLevel.hdr10_maxlight = this->options.hdr10_maxlight;
    coding_ctrl->Hdr10LightLevel.hdr10_avglight = this->options.hdr10_avglight;
  }

  coding_ctrl->vuiColorDescription.vuiColorDescripPresentFlag  = this->options.vuiColorDescripPresentFlag;
  if (this->options.vuiColorDescripPresentFlag) {
    coding_ctrl->vuiColorDescription.vuiMatrixCoefficients     = this->options.vuiMatrixCoefficients;
    coding_ctrl->vuiColorDescription.vuiColorPrimaries         = this->options.vuiColorPrimaries;
      coding_ctrl->vuiColorDescription.vuiTransferCharacteristics = this->options.vuiTransferCharacteristics;
  }

  coding_ctrl->vuiVideoFormat = this->options.vuiVideoFormat;
  coding_ctrl->vuiVideoSignalTypePresentFlag = this->options.vuiVideoSignalTypePresentFlag;
  coding_ctrl->psyFactor = this->options.psyFactor;
  coding_ctrl->aq_mode = this->options.aq_mode;
  coding_ctrl->aq_strength = this->options.aq_strength;

  coding_ctrl->RpsInSliceHeader = this->options.RpsInSliceHeader;

  coding_ctrl->meVertSearchRange = this->options.MEVertRange;
  coding_ctrl->layerInRefIdcEnable = this->options.layerInRefIdc;

  coding_ctrl->sbi_id_0 = this->options.sbi_id_0;
  coding_ctrl->sbi_id_1 = this->options.sbi_id_1;
  coding_ctrl->sbi_id_2 = this->options.sbi_id_2;

  coding_ctrl->dynamicRdoCu16Bias = this->options.dynamicRdoCu16Bias;
  coding_ctrl->dynamicRdoCu16Factor = this->options.dynamicRdoCu16Factor;
  coding_ctrl->dynamicRdoCu32Bias = this->options.dynamicRdoCu32Bias;
  coding_ctrl->dynamicRdoCu32Factor = this->options.dynamicRdoCu32Factor;
  coding_ctrl->enableDynamicRdo = this->options.dynamicRdoEnable;
  coding_ctrl->batchCount = this->options.batchCount;
}

static void vce_prepare_rate_ctrl(const ENCODER_CODEC *this, const CODEC_CONFIG *params,
                            VCEncRateCtrl *rate_ctrl) {
  // optional. Set to -1 to use default
  if (params->rate_config.nQpDefault > 0)
      rate_ctrl->qpHdr = params->rate_config.nQpDefault;
  else
      rate_ctrl->qpHdr = -1;

  // optional. Set to -1 to use default
  if (this->options.nQpMin >= 0) {
      rate_ctrl->qpMinI = rate_ctrl->qpMinPB = this->options.nQpMin;

      if (rate_ctrl->qpHdr != -1 && rate_ctrl->qpHdr < (OMX_S32)rate_ctrl->qpMinI)
          rate_ctrl->qpHdr = rate_ctrl->qpMinI;
  }

  // optional. Set to -1 to use default
  if (this->options.nQpMax > 0) {
      rate_ctrl->qpMaxI = rate_ctrl->qpMaxPB = this->options.nQpMax;

      if (rate_ctrl->qpHdr > (OMX_S32)rate_ctrl->qpMaxI)
          rate_ctrl->qpHdr = rate_ctrl->qpMaxI;
  }

  // Set to -1 to use default
  if (this->nPictureRcEnabled >= 0)
      rate_ctrl->pictureRc = this->nPictureRcEnabled;

  rate_ctrl->changePos = this->options.changePos;
  rate_ctrl->ctbRc = this->options.ctbRcMode;
  rate_ctrl->blockRCSize = this->options.blockRCSize;
  rate_ctrl->rcQpDeltaRange = this->options.rcQpDeltaRange;
  rate_ctrl->rcBaseMBComplexity = this->options.rcBaseMBComplexity;
  if(this->options.picQpDeltaMin != VSI_DEFAULT_VALUE)
      rate_ctrl->picQpDeltaMin = this->options.picQpDeltaMin;
  if(this->options.picQpDeltaMax != VSI_DEFAULT_VALUE)
      rate_ctrl->picQpDeltaMax = this->options.picQpDeltaMax;
  if(this->options.bitVarRangeI != VSI_DEFAULT_VALUE)
      rate_ctrl->bitVarRangeI = this->options.bitVarRangeI;
  if(this->options.bitVarRangeP != VSI_DEFAULT_VALUE)
      rate_ctrl->bitVarRangeP = this->options.bitVarRangeP;
  if(this->options.bitVarRangeB != VSI_DEFAULT_VALUE)
      rate_ctrl->bitVarRangeB = this->options.bitVarRangeB;

  if(this->options.tolCtbRcInter != VSI_DEFAULT_VALUE)
      rate_ctrl->tolCtbRcInter = Q16_FLOAT(this->options.tolCtbRcInter);
  if(this->options.tolCtbRcIntra != VSI_DEFAULT_VALUE)
      rate_ctrl->tolCtbRcIntra = this->options.tolCtbRcIntra;
  if(this->options.ctbRcRowQpStep != VSI_DEFAULT_VALUE)
      rate_ctrl->ctbRcRowQpStep = this->options.ctbRcRowQpStep;

  rate_ctrl->longTermQpDelta = this->options.longTermQpDelta;
  rate_ctrl->tolMovingBitRate = this->options.tolMovingBitRate;
  rate_ctrl->hieQpDeltaEnable = (this->encIn.gopConfig.size > 1 && this->options.picRc == 1);
  rate_ctrl->monitorFrames = this->options.monitorFrames;
  if(rate_ctrl->monitorFrames>MOVING_AVERAGE_FRAMES)
      rate_ctrl->monitorFrames=MOVING_AVERAGE_FRAMES;
  else if(rate_ctrl->monitorFrames < 10)
      rate_ctrl->monitorFrames = (this->options.outputRateNumer > this->options.outputRateDenom) ? 10 : LEAST_MONITOR_FRAME;

  // optional. Set to -1 to use default
  if (this->options.bitPerSecond >= 0)
      rate_ctrl->bitPerSecond = this->options.bitPerSecond;

  // Set to -1 to use default
  if (this->options.hrdConformance >= 0)
      rate_ctrl->hrd = this->options.hrdConformance;

#if 0
  if (params->hevc_config.nPFrames)
    rate_ctrl->bitrateWindow = this->nPFrames + 1;
#endif
  if (this->options.intraPicRate != 0)
      rate_ctrl->bitrateWindow = MIN(this->options.intraPicRate, MAX_GOP_LEN);

  if(this->options.bitrateWindow != VSI_DEFAULT_VALUE)
      rate_ctrl->bitrateWindow = this->options.bitrateWindow;

  if(this->options.intraQpDelta != VSI_DEFAULT_VALUE)
      rate_ctrl->intraQpDelta = this->options.intraQpDelta;

  if(this->options.vbr != VSI_DEFAULT_VALUE)
      rate_ctrl->vbr = this->options.vbr;

  rate_ctrl->fixedIntraQp = this->options.fixedIntraQp;
  rate_ctrl->smoothPsnrInGOP = this->options.smoothPsnrInGOP;
  rate_ctrl->u32StaticSceneIbitPercent = this->options.u32StaticSceneIbitPercent;
  rate_ctrl->hrdCpbSize = this->options.cpbSize;

  rate_ctrl->pictureSkip = this->options.picSkip;

  rate_ctrl->crf = this->options.crf;
  rate_ctrl->frameRateNum = this->options.outputRateNumer;
  rate_ctrl->frameRateDenom = this->options.outputRateDenom;
  //TODO lxj: parse from options
  {
    rate_ctrl->tolRcUnderflow = 50;
  }

  rate_ctrl->cpbMaxRate = this->options.cpbMaxRate;
  rate_ctrl->rcMode = this->options.rcMode;
}

static VCEncRet vce_prepare_pre_pp_cfg(const ENCODER_CODEC *this, const CODEC_CONFIG *params,
                            VCEncPreProcessingCfg *pp_config) {

  VCEncRet ret = VCENC_OK;
  i32 tileId, horOff, verOff;

  pp_config->origWidth = params->pp_config.origWidth;
  pp_config->origHeight = params->pp_config.origHeight;
  pp_config->xOffset = params->pp_config.xOffset;
  pp_config->yOffset = params->pp_config.yOffset;
  pp_config->input_alignment = 1 << this->options.exp_of_input_alignment;
  pp_config->scaledOutput = 0;

  horOff = MAX(this->options.horOffsetSrc, 0);
  verOff = MAX(this->options.verOffsetSrc, 0);
  for (tileId = 0; tileId < this->options.num_tile_columns; tileId++) {
    if (tileId == 0) {
      pp_config->origWidth = this->options.lumWidthSrc;
      pp_config->origHeight = this->options.lumHeightSrc;
      pp_config->alignmentWidth = this->options.lumWidthSrc;
      pp_config->alignmentHeight = this->options.lumHeightSrc;
      if (this->options.horOffsetSrc != DEFAULT || this->options.tiles_enabled_flag)
        pp_config->xOffset = horOff;
      if (this->options.verOffsetSrc != DEFAULT || this->options.tiles_enabled_flag)
        pp_config->yOffset = verOff;
      horOff += this->tile_width[tileId] * 64;
    }
    /* else {
      pp_config->tileExtra[tileId - 1].origWidth = this->options.lumWidthSrc;
      pp_config->tileExtra[tileId - 1].origHeight = this->options.lumHeightSrc;
      if (this->options.horOffsetSrc != DEFAULT || this->options.tiles_enabled_flag)
        pp_config->tileExtra[tileId - 1].xOffset = horOff;
      if (this->options.verOffsetSrc != DEFAULT || this->options.tiles_enabled_flag)
        pp_config->tileExtra[tileId - 1].yOffset = verOff;
      horOff += this->options.tile_width[tileId] * 64;
    }
    */
  }

  switch (params->pp_config.formatType)
  {
    case OMX_COLOR_FormatYUV420PackedPlanar:
    case OMX_COLOR_FormatYUV420Planar:
      pp_config->inputType = VCENC_YUV420_PLANAR; //0
      break;
    case OMX_COLOR_FormatYUV420PackedSemiPlanar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
      pp_config->inputType = VCENC_YUV420_SEMIPLANAR; //1
      break;
    case OMX_COLOR_FormatYUV420SemiPlanarVU:
      pp_config->inputType = VCENC_YUV420_SEMIPLANAR_VU;
      break;
    case OMX_COLOR_FormatYCbYCr:
      pp_config->inputType = VCENC_YUV422_INTERLEAVED_YUYV; //3
      break;
    case OMX_COLOR_FormatCbYCrY:
      pp_config->inputType = VCENC_YUV422_INTERLEAVED_UYVY;
      break;
    case OMX_COLOR_Format16bitRGB565:
      pp_config->inputType = VCENC_RGB565; //5
      break;
    case OMX_COLOR_Format16bitBGR565:
      pp_config->inputType = VCENC_BGR565;
      break;
    case OMX_COLOR_Format16bitARGB1555:
      pp_config->inputType = VCENC_RGB555; //7
      break;
    case OMX_COLOR_Format16bitBGR555:
      pp_config->inputType = VCENC_BGR555;
      break;
    case OMX_COLOR_Format16bitARGB4444:
    case OMX_COLOR_Format12bitRGB444:
      pp_config->inputType = VCENC_RGB444; //9
      break;
    case OMX_COLOR_Format12bitBGR444:
      pp_config->inputType = VCENC_BGR444;
      break;
    case OMX_COLOR_Format25bitARGB1888:
    case OMX_COLOR_Format32bitARGB8888:
      pp_config->inputType = VCENC_RGB888; //11
      break;

    default:
      DBGT_CRITICAL("Unknown color format");
      ret = VCENC_INVALID_ARGUMENT;
      break;

    return ret;
  }

  switch (params->pp_config.angle)
  {
    case 0:
      pp_config->rotation = VCENC_ROTATE_0;
      break;
    case 90:
      pp_config->rotation = VCENC_ROTATE_90R;
      break;
    case 270:
      pp_config->rotation = VCENC_ROTATE_90L;
      break;
    default:
      DBGT_CRITICAL("Unsupported rotation angle");
      ret = VCENC_INVALID_ARGUMENT;
      break;
  }

  //pp_config->videoStabilization = params->pp_config.frameStabilization;
  return ret;
}

// create codec instance and initialize it
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_codec(const CODEC_CONFIG* params)
{
    DBGT_PROLOG("");
    VCEncConfig cfg;
    //VCEncApiVersion apiVer;
    //VCEncBuild encBuild;
    VCEncRet ret = VCENC_OK;

    ENCODER_CODEC* this = OSAL_Malloc(sizeof(ENCODER_CODEC));
    memset(this, 0, sizeof(ENCODER_CODEC));
    this->codecFormat = vce_init_opts(this, params);

    if(0 != vce_update_opts(this, params))
    {
        DBGT_CRITICAL("Parameters Error!");
        DBGT_EPILOG("");
        return NULL;
    }

    if(0 != vce_init_gopConfigs(this))
        return NULL;

#ifdef ENABLE_DBGT_TRACE
    VCEncApiVersion apiVer;
    VCEncBuild encBuild;

    DBGT_PDEBUG("Intra period %d", (int)this->nPFrames + 1);

    apiVer = VCEncGetApiVersion();

    DBGT_PDEBUG("VC Encoder API version %d.%d", apiVer.major, apiVer.minor);

    for (i = 0; i< EWLGetCoreNum(NULL); i++)
    {
        encBuild = VCEncGetBuild(i);
        DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
             encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
             encBuild.hwBuild, encBuild.swBuild / 1000000,
             (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);
    }
#endif

    vce_init_misc(this);

    memset(&cfg, 0, sizeof(cfg));
    vce_prepare_enc_cfg(this, &cfg);
    if (cfg.parallelCoreNum != this->options.parallelCoreNum)
      this->options.parallelCoreNum = cfg.parallelCoreNum;

    this->instance = NULL;
    ret = VCEncInit(&cfg, &this->instance, NULL);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncInit failed! (%d)", ret);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    this->base.stream_start = encoder_stream_start_codec;
    this->base.stream_end = encoder_stream_end_codec;
    this->base.encode = encoder_encode_codec;
    this->base.destroy = encoder_destroy_codec;
    this->base.flush = encoder_flush_codec;

    // Setup coding control
    VCEncCodingCtrl coding_ctrl;
    memset(&coding_ctrl, 0, sizeof(coding_ctrl));
    ret = VCEncGetCodingCtrl(this->instance, &coding_ctrl);
    if(ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncGetCodingCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    vce_prepare_coding_ctrl(this, &coding_ctrl);
    ret = VCEncSetCodingCtrl(this->instance, &coding_ctrl);
    if(ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetCodingCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup rate control
    VCEncRateCtrl rate_ctrl;
    memset(&rate_ctrl, 0, sizeof(rate_ctrl));
    ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }
    vce_prepare_rate_ctrl(this, params, &rate_ctrl);
    this->currBitrate = rate_ctrl.bitPerSecond;
    ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetRateCtrl failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

    // Setup preprocessing
    VCEncPreProcessingCfg pp_config;
    ret = VCEncGetPreProcessing(this->instance, &pp_config);
    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncGetPreProcessing failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }
    ret = vce_prepare_pre_pp_cfg(this, params, &pp_config);
    if (ret == VCENC_OK)
    {
        //DBGT_PDEBUG("Video stabilization: %d", (int)pp_config.videoStabilization);
        DBGT_PDEBUG("Rotation: %d", (int)params->pp_config.angle);
        ret = VCEncSetPreProcessing(this->instance, &pp_config);
    }

    if (ret != VCENC_OK)
    {
        DBGT_CRITICAL("VCEncSetPreProcessing failed! (%d)", ret);
        VCEncRelease(this->instance);
        OSAL_Free(this);
        DBGT_EPILOG("");
        return NULL;
    }

#ifdef TEST_DATA
{
    extern i32 Enc_test_data_init(i32 parallelCoreNum);
    Enc_test_data_init(cfg.parallelCoreNum);
}
#endif

    this->base.nInputBufferCountMin = (OMX_U32)VCEncGetEncodeMaxDelay(this->instance) + 1;

    DBGT_EPILOG("");
    return (ENCODER_PROTOTYPE*) this;
}

CODEC_STATE HantroHwEncOmx_encoder_intra_period_codec(ENCODER_PROTOTYPE* arg, OMX_U32 nPFrames, OMX_U32 nBFrames)
{
    DBGT_PROLOG("");

    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;

    this->nPFrames = nPFrames;
	this->nBFrames = nBFrames;
    DBGT_PDEBUG("New Intra period %d", (int)this->nPFrames+this->nBFrames+1);

    VCEncRateCtrl rate_ctrl;
    memset(&rate_ctrl, 0, sizeof(VCEncRateCtrl));

    VCEncRet ret = VCEncGetRateCtrl(this->instance, &rate_ctrl);

    if (ret == VCENC_OK)
    {
        rate_ctrl.bitrateWindow = nPFrames+nBFrames+1;

        ret = VCEncSetRateCtrl(this->instance, &rate_ctrl);
    }
    else
    {
        DBGT_CRITICAL("VCEncGetRateCtrl failed! (%d)", ret);
        DBGT_EPILOG("");
        return CODEC_ERROR_UNSPECIFIED;
    }

    switch (ret)
    {
        case VCENC_OK:
            stat = CODEC_OK;
            break;
        case VCENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case VCENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case VCENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        default:
            DBGT_CRITICAL("CODEC_ERROR_UNSPECIFIED");
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

CODEC_STATE HantroHwEncOmx_encoder_frame_rate_codec(ENCODER_PROTOTYPE* arg, OMX_U32 xFramerate)
{
    DBGT_PROLOG("");
    ENCODER_CODEC* this = (ENCODER_CODEC*)arg;

    this->nEstTimeInc = (OMX_U32) (TIME_RESOLUTION / Q16_FLOAT(xFramerate));

    DBGT_PDEBUG("New time increment %d", (int)this->nEstTimeInc);

    DBGT_EPILOG("");
    return CODEC_OK;
}
