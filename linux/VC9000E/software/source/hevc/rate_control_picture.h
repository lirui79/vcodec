
#ifndef RATE_CONTROL_PICTURE_H
#define RATE_CONTROL_PICTURE_H
#include "base_type.h"
#include "sw_picture.h"
#include "enccommon.h"
#include "hevcSei.h"
#ifdef VSB_TEMP_TEST
#include "video_statistic.h"
#endif
#ifdef __cplusplus
extern"C"{
#endif
#ifdef RCP_FIXED_POINT_OPT
#define zc612041ce8 (0xaa0+5898-0x2192)
#define LL_FIXED	((0x1443+1265-0x1933)<<zc612041ce8)
#define LL_HALF 	((0x505+6262-0x1d7a)<<(zc612041ce8-(0x92d+5313-0x1ded)))
#define RCP_DATA_TYPE(x) ((i64)(x * (float)LL_FIXED + 0.5))
#define zaf6a9cbf8f	(0x774+525-0x962)
#define Q31_FIXED_ONE	((i64)(0x212+5832-0x18d9)<<zaf6a9cbf8f)
#define Q31_FIXED_HALF  ((i64)(0x651+2505-0x1019)<<(zaf6a9cbf8f-\
(0x1694+2495-0x2052)))
#else
#define LL_FIXED	1.0
#define LL_HALF 	0.5
#define RCP_DATA_TYPE(x) x
#define Q31_FIXED_ONE	1.0
#define Q31_FIXED_HALF  0.5
#endif
#define zec761a150a
#define zea077a043d
enum{VCENCRC_OVERFLOW=-(0x1cdf+91-0x1d39)};
#define RC_CBR_HRD \
  (0x8ec+1441-0xe8c) 
#define zb4f7b36dcd (0x1039+4050-0x2004)       
#define z9ae471aa41 (0xc67+5705-0x22a6) 
#define zdb70b37658 (0xb42+1034-0xf42)  
#define INTRA_QPDELTA_DEFAULT (-(0x1343+4263-0x23e7))
#ifndef CTBRC_STRENGTH
#define QP_FRACTIONAL_BITS (0x19d8+2315-0x22e3)
#else
#define QP_FRACTIONAL_BITS (0x772+6546-0x20fc)
#endif
#define QP_DEFAULT (0x9a+9164-0x244c)
#define TOL_CTB_RC_FIX_POINT (0xbc3+4770-0x1e5e)
#define z2caa41c7fe (0x823+860-0xb43)
#define LEAST_MONITOR_FRAME (0x50c+4251-0x15a4)
#define I32_MAX ((i32)2147483647)
#define CLIP_I32_ADD(a, b) ((a) > (I32_MAX - (b)) ? I32_MAX : (a) + (b))
#define IS_CTBRC(x) ((x)&(0x66c+2766-0x1137))
#define IS_CTBRC_FOR_QUALITY(x) ((x)&(0x1b2+2790-0xc97))
#define IS_CTBRC_FOR_BITRATE(x) ((x)&(0xa1d+3504-0x17cb))
#define CLR_CTBRC_FOR_BITRATE(x) \
  { (x) &= (~(0x15fb+3753-0x24a2)); }
#define CTB_RC_QP_STEP_FIXP (0x17e1+3022-0x239f)
#define CTB_RC_ROW_FACTOR_FIXP (0x198a+2949-0x24ff)
enum{SIMPLE_SCENE,NORMAL_SCENE,COMPLEX_SCENE,};typedef enum{zb5bbf0457d,
z6ac475593e,z0715dbdfce,zb590ff1927,}zb1b0798615;typedef enum{z16f1b4d9d7=
(0x1252+4062-0x2230),z358e31d775,z5a9ecf279c,z7887fd7171,z044ddf4bbe,z2e6fd2c284
,}zdf33e88a23;typedef struct{i32 frame[(0x13b+9682-0x2695)];i32 length;i32 count
;i32 zff13b54c4f;i32 z57f8526067;i32 zb2975ef616;}zcf4bacd786;typedef struct{u32
 intraCu8Num[(0x33f+1624-0x91f)];u32 skipCu8Num[(0xfc9+5345-0x2432)];u32 
PBFrame4NRdCost[(0x1d44+1205-0x2181)];i32 length;i32 count;i32 zff13b54c4f;u32 
zc09ddd71e6;u32 z59fac279de;u32 zf374f6a90b;}z5a5950b51a;typedef struct{i64 a1;
i64 zca076e6f67;i32 z8ad09e4259;i32 zcb89df56bf[zdb70b37658+(0x15b9+2771-0x208b)
];i32 zdfcc1a3d2b[zdb70b37658+(0xa53+5075-0x1e25)];i32 zff13b54c4f;i32 len;i32 
z8bdaa35e64;i32 z9f6f1ccdd6;i32 z8043ecc287;i32 prevFrameBits;i32 prevTargetBits
;}za506561fab;typedef struct{i32 ze60c2c5fdb[zb4f7b36dcd];i32 z0b244d785d[
zb4f7b36dcd];i32 zf596d61938[z9ae471aa41];i32 z6b3312b96b[z9ae471aa41];i32 
z31c3ec84f4;i32 zf8a3f36c10;}z6783136e61;typedef struct{i32 bufferSize;i32 
z8bbd1e30ab;i32 ze8bd0d9c56;i32 maxBitRate;i32 bufferRate;i32 bitRate;i32 
bitPerPic;i32 z1f9e750b2c;i32 timeScale;i32 unitsInTic;i32 zbe9fd58c6a;i32 
realBitCnt;i32 z87a5b08757;i32 z9b20c7aeb4;i32 zd5c2c0ded9;i32 z2bdef80297;i32 
bucketFullness;i32 bucketLevel;i32 zc0a193821d;i32 z11b549eb01;i32 z0d777460a4;}
rcVirtualBuffer_s;typedef struct{i32 x0;i32 x1;i32 xMin;i32 started;i32 
preFrameMad;ptr_t ctbMemPreAddr;u32*ctbMemPreVirtualAddr;i32 frameCnt;i32 
idxType;i32 refCount;i32 valid;}ctbRcModel_s;typedef struct{ptr_t ctbMemCurAddr;
u32*ctbMemCurVirtualAddr;u32 ctbRcModelsIdx;}zc85d51b72c;typedef struct{
ctbRcModel_s models[(0x1402+1130-0x186a)+MAX_CORE_NUM];i32 qpSumForRc;i32 qpStep
;i32 ctbRcRowQpDeltaRange;i32 rowFactor;zc85d51b72c Cur[MAX_CORE_NUM];i32 
updateModelIdx;}ctbRateControl_s;typedef struct{RCP_64bit coeffMin;RCP_64bit 
coeff;RCP_64bit count;RCP_64bit decay;RCP_64bit offset;RCP_64bit shortTermCost;
RCP_64bit shortTermCostDecay;RCP_64bit shortTermCostCount;RCP_64bit cost;
RCP_64bit longTermCost;RCP_64bit longTermCostDecay;RCP_64bit longTermCostCount;
i32 qp;i32 qpPrev;i32 prevFrameBits;i32 prevTargetBits;i8 z537039b124;i8 
z33e751d204;i8 z88cdc37774;}rcPredictor;typedef struct{true_e picRc;u32 ctbRc;
true_e picSkip;true_e fillerData;i32 tolUnderflow;true_e hrd;true_e vbr;i32 
zeb1320bacc;i32 rcMode;u32 zab659326f4;i32 picArea;i32 ctbPerPic;i32 ctbRows;i32
 ctbCols;i32 ctbSize;i32 zea3862b025;i32 nonZeroCnt;i32 z199dc1d4e8;i32 qpSum;
i32 z1217f7c48b;RCP_32bit zecc8b26b90;u32 sliceTypeCur;u32 sliceTypePrev;true_e 
frameCoded;true_e skipGop;i32 fixedQp;i32 qpHdr;i32 qpMin;i32 qpMax;i32 qpMinI;
i32 qpMaxI;i32 qpMinPB;i32 qpMaxPB;i32 qpHdrPrev;i32 qpLastCoded;i32 qpTarget;
i32 z8fd7bcaec1;u32 zc97e82b7f1;i32 outRateNum;i32 outRateDenom;i32 zcbf5f92f04;
i32 zdeb37127ce;i32 z29350a2390;z6783136e61 z1e137903f2;rcVirtualBuffer_s 
virtualBuffer;sei_s sei;i32 z792101456d,z6e3dcebb58;za506561fab za8b95563b2;i32 
targetPicSize;i32 z3eab678f73;i32 zc7d8469149;i32 z040c884dc1;i32 zc7008a3cc6;
i32 z8e4b0bbb41;
#if VBR_RC
i32 zf066713d8c;i32 zd99b97e997;i32 zbd6d50ffdd;i32 zb4bd254b9e;i32 z5f310a70ab;
#endif
i32 bitVarRangeI;i32 bitVarRangeP;i32 bitVarRangeB;i32 minPicSizeI;i32 
maxPicSizeI;i32 minPicSizeP;i32 maxPicSizeP;i32 minPicSizeB;i32 maxPicSizeB;i32 
ze5fe0d2d6b;i32 tolMovingBitRate;i32 monitorFrames;float tolCtbRcInter;float 
tolCtbRcIntra;u32 maxIprop;u32 minIprop;i32 changePos;RCP_64bit qpFactor;i32 
ze173381e96;i32 ze41cb9116e;i32 za25e347240;i32 zf0e7d7a43a;i32 ze84c721aa9;i32 
zabbbd97b5e;u32 z3c9e80fceb;u32 frameCnt;i32 bitrateWindow;i32 zf9d3566790;i32 
z3c5c469fd0;i32 z29e41b4870;i32 intraQpDelta;i32 longTermQpDelta;i32 
frameQpDelta;u32 fixedIntraQp;i32 zdfb346795b;i32 hierarchial_sse[
(0xba3+2555-0x1596)][(0x13a7+3527-0x2166)];i32 smooth_psnr_in_gop;i32 
hierarchial_bit_allocation_GOP_size;i32 z45813bc4d2;i32 encoded_frame_number;u32
 gopPoc;u32 picSkipAllowed;i32 minPicSize;i32 maxPicSize;i32 encodedFramesBits;
u32 ctbRcQpDeltaReverse;u32 ctbRcBitsMin;u32 ctbRcBitsMax;u32 ctbRctotalLcuBit;
u32 bitsRatio;u32 ctbRcThrdMin;u32 ctbRcThrdMax;i32 z2aab5d849e;u32 
rcQpDeltaRange;u32 rcBaseMBComplexity;i32 picQpDeltaMin;i32 picQpDeltaMax;i32 
z9942fdcdc6;i32 inputSceneChange;zcf4bacd786 zafb762023b;z5a5950b51a z5bd36220d1
;u32 rcPicComplexity;RCP_64bit complexity;i32 z257688f63c;i32 z44de60b69c;i32 
zc3e0c52022;i32 z6f73eb8ac3;i32 zf062627859;u32 intraCu8Num;u32 skipCu8Num;u32 
PBFrame4NRdCost;RCP_32bit reciprocalOfNumBlk8;u32 codingType;i32 i32MaxPicSize;
u32 z52fad3c87c;i32 z75b9ee3750;i32 z2b59435d98;u32 z93e1c9ca47;u32 
u32StaticSceneIbitPercent;ctbRateControl_s ctbRateCtrl;i32 crf;RCP_32bit 
z247948ead7;
#ifndef RCP_FIXED_POINT_OPT
double pbOffset;double ipOffset;
#endif
u64 z2b0accbf8b;
#ifndef RCP_FIXED_POINT_OPT
double z37c52f3b35;
#endif
int z02f70a8bf0;i32 pass;RCP_32bit pass1CurCost;i32 agopPass1GopPicIdx;RCP_32bit
 pass1GopCost[(0x11dc+2995-0x1d8b)];RCP_32bit pass1AvgCost[(0xa59+4419-0x1b98)];
i32 pass1GopFrameNum[(0x1d3+1074-0x601)];i32 pass1FrameNum[(0xc6d+759-0xf60)];
i32 zc6884ee9fd[(0x3d4+736-0x6ae)];rcPredictor rcPred[(0x86d+47-0x896)];i32 
predLayerId;u32 hieQpDeltaEnable;true_e cbr_flag;u8 IFrameQpVisualTuning;u8 
pass1EstCostAll;u8 visualLmdTuning;u8 z11fe80aba6;u8 initialQpTuning;zb1b0798615
 z0c75bf73dc;u32 curMotionScores[(0x4d3+2422-0xe47)][(0xa0d+6973-0x2548)];u32 
za734b322bf[(0xd82+4263-0x1e25)];true_e z64b59ebb96;u32 coreNum;}
vcencRateControl_s;
#ifndef RATE_CONTROL_BUILD_SUPPORT
#define VCEncInitRc za96ee99723
#define VCEncBeforePicRc za87ad661eb
#define VCEncAfterPicRc(zca7520bb04, nonZeroCnt, ze3947b4c8e, qpSum, z1217f7c48b\
) ((0x17c5+503-0x19bc))
#define z580c5b15ed(zca7520bb04, frameCnt) ((0xe46+2103-0x167d))
#define rcCalculate(a, b, c) ((c) == (0xcdf+321-0xe20) ? (0x5a9+661-0x83e) : ((a\
) * (b) / (c)))
#else
bool_e VCEncInitRc(vcencRateControl_s*zca7520bb04,u32 zcb3d5e8dbc);void 
VCEncBeforePicRc(vcencRateControl_s*zca7520bb04,u32 zeabf8072f6,u32 z9acc17595d,
bool z2f7fa6af59,struct sw_picture*);i32 VCEncAfterPicRc(vcencRateControl_s*
zca7520bb04,u32 nonZeroCnt,u32 ze3947b4c8e,u32 qpSum,u32 z1217f7c48b);u32 
z580c5b15ed(vcencRateControl_s*zca7520bb04,u32 frameCnt);i32 rcCalculate(i32 a,
i32 b,i32 c);
#endif
#ifdef __cplusplus
}
#endif
#endif

