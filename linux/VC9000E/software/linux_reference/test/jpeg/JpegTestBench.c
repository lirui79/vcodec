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

/* For parameter parsing */
#include "JpegTestBench.h"
#include "EncGetOption.h"

/* For SW/HW shared memory allocation */
#include "ewl.h"
#include "enc_log.h"

/* For accessing the EWL instance inside the encoder */
//#include "EncJpegInstance.h"

/* For compiler flags, test data, debug and tracing */
#include "enccommon.h"

/* For Hantro Jpeg encoder */
#include "jpegencapi.h"
#include "mjpegencapi.h"

#include "osal.h"
#include <stddef.h>

#include "encinputlinebuffer.h"
#include "EncJpegNonRoiFilterTables.h"
#include "enc_helper.h"
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
#include "sliceinfo.h"
#endif

/*--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/* User selectable testbench configuration */

/* Define this if you want to save each frame of motion jpeg
 * into frame%d.jpg */
/* #define SEPARATE_FRAME_OUTPUT */

/* Define this if yuv don't want to use debug printf */
/*#define ASIC_WAVE_TRACE_TRIGGER*/

/* Output stream is not written to file. This should be used
   when running performance simulations. */
/*#define NO_OUTPUT_WRITE */

/* Define these if you want to use testbench defined
 * comment header */
/*#define TB_DEFINED_COMMENT */

#define USER_DEFINED_QTABLE 10

#define MOVING_AVERAGE_FRAMES 30

#define MAX_BITRATE (2*1000*1000*1000) /* 2Gbps */

/* Global variables */

/* Command line options */

static option_s options[] = {
    {"help", 'H', 0, 0, 1, {0}},
    {"inputThumb", 'I', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(inputThumb)}},
    {"thumbnail", 'T', 1, I32_NUM_TYPE, 1, {OFFSETOF(thumbnail)}},
    {"widthThumb", 'K', 1, I32_NUM_TYPE, 1, {OFFSETOF(widthThumb)}},
    {"heightThumb", 'L', 1, I32_NUM_TYPE, 1, {OFFSETOF(heightThumb)}},
    {"output", 'o', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(output)}},
    {"firstPic", 'a', 1, I32_NUM_TYPE, 1, {OFFSETOF(firstPic)}},
    {"lastPic", 'b', 1, I32_NUM_TYPE, 1, {OFFSETOF(lastPic)}},
    {"lumWidthSrc", 'w', 1, I32_NUM_TYPE, 1, {OFFSETOF(lumWidthSrc)}},
    {"lumHeightSrc", 'h', 1, I32_NUM_TYPE, 1, {OFFSETOF(lumHeightSrc)}},
    {"width", 'x', 1, I32_NUM_TYPE, 1, {OFFSETOF(width)}},
    {"height", 'y', 1, I32_NUM_TYPE, 1, {OFFSETOF(height)}},
    {"horOffsetSrc", 'X', 1, I32_NUM_TYPE, 1, {OFFSETOF(horOffsetSrc)}},
    {"verOffsetSrc", 'Y', 1, I32_NUM_TYPE, 1, {OFFSETOF(verOffsetSrc)}},
    {"restartInterval", 'R', 1, I32_NUM_TYPE, 1, {OFFSETOF(restartInterval)}},
    {"qLevel", 'q', 1, I32_NUM_TYPE, 1, {OFFSETOF(qLevel)}},
    {"quality", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(quality)}},
    {"frameType", 'g', 1, I32_NUM_TYPE, 1, {OFFSETOF(frameType)}},
    {"colorConversion", 'v', 1, I32_NUM_TYPE, 1, {OFFSETOF(colorConversion)}},
    {"rotation", 'G', 1, I32_NUM_TYPE, 1, {OFFSETOF(rotation)}},
    {"codingType", 'p', 1, I32_NUM_TYPE, 1, {OFFSETOF(partialCoding)}},
    {"codingMode", 'm', 1, I32_NUM_TYPE, 1, {OFFSETOF(codingMode)}},
    {"markerType", 't', 1, I32_NUM_TYPE, 1, {OFFSETOF(markerType)}},
    {"units", 'u', 1, I32_NUM_TYPE, 1, {OFFSETOF(unitsType)}},
    {"xdensity", 'k', 1, I32_NUM_TYPE, 1, {OFFSETOF(xdensity)}},
    {"ydensity", 'l', 1, I32_NUM_TYPE, 1, {OFFSETOF(ydensity)}},
    {"write", 'W', 1, I32_NUM_TYPE, 1, {OFFSETOF(writeOut)}},
    {"comLength", 'c', 1, I32_NUM_TYPE, 1, {OFFSETOF(comLength)}},
    {"comFile", 'C', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(com)}},
    {"trigger", 'P', 1, 0, 1, {0}},
    {"inputLineBufferMode", 'S', 1, I32_NUM_TYPE, 1, {OFFSETOF(inputLineBufMode)}},
    {"inputLineBufferDepth", 'N', 1, I32_NUM_TYPE, 1, {OFFSETOF(inputLineBufDepth)}},
    {"inputLineBufferAmountPerLoopback", 's', 1, U32_NUM_TYPE, 1, {OFFSETOF(amountPerLoopBack)}},
    {"inputAlignmentExp", 'Q', 1, U32_NUM_TYPE, 1, {OFFSETOF(exp_of_input_alignment)}},
    {"inputSliceInfoEn", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(inputSliceInfoEn)}},
    {"input", 'i', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(input)}},
    /* hash frame data, 0--disable, 1--crc32, 2--checksum */
    {"hashtype", 'A', 1, U32_NUM_TYPE, 1, {OFFSETOF(hashtype)}},
    {"mirror", 'M', 1, I32_NUM_TYPE, 1, {OFFSETOF(mirror)}},
    {"XformCustomerPrivateFormat", 'D', 1, I32_NUM_TYPE, 1, {OFFSETOF(formatCustomizedType)}},
    {"enableConstChroma", 'd', 1, I32_NUM_TYPE, 1, {OFFSETOF(constChromaEn)}},
    {"constCb", 'e', 1, U32_NUM_TYPE, 1, {OFFSETOF(constCb)}},
    {"constCr", 'f', 1, U32_NUM_TYPE, 1, {OFFSETOF(constCr)}},
    {"lossless", '1', 1, I32_NUM_TYPE, 1, {OFFSETOF(losslessEnable)}},
    {"ptrans", '2', 1, I32_NUM_TYPE, 1, {OFFSETOF(ptransValue)}},
    {"bitPerSecond", 'B', 1, U32_NUM_TYPE, 1, {OFFSETOF(bitPerSecond)}},
    {"mjpeg", 'J', 1, U32_NUM_TYPE, 1, {OFFSETOF(mjpeg)}},
    {"frameRateNum", 'n', 1, U32_NUM_TYPE, 1, {OFFSETOF(frameRateNum)}},
    {"frameRateDenom", 'r', 1, U32_NUM_TYPE, 1, {OFFSETOF(frameRateDenom)}},
    {"qpHdr", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(qpHdr)}},
    {"qpMin", 'E', 1, U32_NUM_TYPE, 1, {OFFSETOF(qpmin)}},
    {"qpMax", 'F', 1, U32_NUM_TYPE, 1, {OFFSETOF(qpmax)}},
    {"rcMode", 'V', 1, I32_NUM_TYPE, 1, {OFFSETOF(rcMode)}},
    {"picQpDeltaRange", 'U', 1, I32_NUM_TYPE, 2, {OFFSETOF(picQpDeltaMin), OFFSETOF(picQpDeltaMax)}},
    {"segmentUnitHeight", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(segmentUnitHeight)}},
    {"roimapFile", '0', 1, STR_TYPE, 1, {OFFSETOF(roimapFile)}},
    {"nonRoiFilter", '0', 1, STR_TYPE, 1, {OFFSETOF(nonRoiFilter)}},
    {"nonRoiLevel", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(nonRoiLevel)}},
    {"fixedQP", 'O', 1, I32_NUM_TYPE, 1, {OFFSETOF(fixedQP)}},
    {"streamBufChain", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(streamBufChain)}},
    {"streamMultiSegmentMode", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(streamMultiSegmentMode)}},
    {"streamMultiSegmentSize", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(streamMultiSegmentSize)}},
    {"streamMultiSegmentAmount", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(streamMultiSegmentAmount)}},
    {"qTableFile", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(qTablePath[0])}},
    {"dec400TableInput", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(dec400CompTableinput[0])}},
    {"dec400TileSize", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(dec400TileSize)}},
    {"dec400TileMode", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(dec400TileMode)}},
    {"dec400DataAlignment", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(dec400DataAlignment)}},
    {"dec400RGBAX", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(dec400RGBAX)}},
    {"dec400RGBFormat", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(dec400RGBFormat)}},
    {"dec400TSHeaderEnable", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(dec400TSHeaderEnable)}},
    {"overlayEnables", '0', 1, I32_NUM_TYPE, 1, {OFFSETOF(overlayEnables)}},
    {"scanType", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(scanType)}},
    {"olInput01", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[0])}},
    {"olFormat01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[0])}},
    {"olAlpha01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[0])}},
    {"olWidth01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[0])}},
    {"olCropWidth01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[0])}},
    {"olHeight01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[0])}},
    {"olCropHeight01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[0])}},
    {"olXoffset01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[0])}},
    {"olCropXoffset01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[0])}},
    {"olYoffset01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[0])}},
    {"olCropYoffset01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[0])}},
    {"olYStride01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[0])}},
    {"olUVStride01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[0])}},
    {"olBitmapY01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[0])}},
    {"olBitmapU01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[0])}},
    {"olBitmapV01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[0])}},
    {"olSuperTile01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olSuperTile[0])}},
    {"olScaleWidth01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olScaleWidth[0])}},
    {"olScaleHeight01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olScaleHeight[0])}},
    {"osdDec400TableInput01", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[0])}},

    {"olInput02", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[1])}},
    {"olFormat02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[1])}},
    {"olAlpha02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[1])}},
    {"olWidth02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[1])}},
    {"olCropWidth02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[1])}},
    {"olHeight02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[1])}},
    {"olCropHeight02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[1])}},
    {"olXoffset02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[1])}},
    {"olCropXoffset02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[1])}},
    {"olYoffset02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[1])}},
    {"olCropYoffset02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[1])}},
    {"olYStride02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[1])}},
    {"olUVStride02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[1])}},
    {"olBitmapY02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[1])}},
    {"olBitmapU02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[1])}},
    {"olBitmapV02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[1])}},
    {"osdDec400TableInput02", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[1])}},

    {"olInput03", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[2])}},
    {"olFormat03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[2])}},
    {"olAlpha03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[2])}},
    {"olWidth03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[2])}},
    {"olCropWidth03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[2])}},
    {"olHeight03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[2])}},
    {"olCropHeight03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[2])}},
    {"olXoffset03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[2])}},
    {"olCropXoffset03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[2])}},
    {"olYoffset03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[2])}},
    {"olCropYoffset03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[2])}},
    {"olYStride03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[2])}},
    {"olUVStride03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[2])}},
    {"olBitmapY03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[2])}},
    {"olBitmapU03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[2])}},
    {"olBitmapV03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[2])}},
    {"osdDec400TableInput03", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[2])}},

    {"olInput04", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[3])}},
    {"olFormat04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[3])}},
    {"olAlpha04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[3])}},
    {"olWidth04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[3])}},
    {"olCropWidth04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[3])}},
    {"olHeight04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[3])}},
    {"olCropHeight04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[3])}},
    {"olXoffset04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[3])}},
    {"olCropXoffset04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[3])}},
    {"olYoffset04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[3])}},
    {"olCropYoffset04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[3])}},
    {"olYStride04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[3])}},
    {"olUVStride04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[3])}},
    {"olBitmapY04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[3])}},
    {"olBitmapU04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[3])}},
    {"olBitmapV04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[3])}},
    {"osdDec400TableInput04", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[3])}},

    {"olInput05", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[4])}},
    {"olFormat05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[4])}},
    {"olAlpha05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[4])}},
    {"olWidth05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[4])}},
    {"olCropWidth05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[4])}},
    {"olHeight05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[4])}},
    {"olCropHeight05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[4])}},
    {"olXoffset05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[4])}},
    {"olCropXoffset05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[4])}},
    {"olYoffset05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[4])}},
    {"olCropYoffset05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[4])}},
    {"olYStride05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[4])}},
    {"olUVStride05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[4])}},
    {"olBitmapY05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[4])}},
    {"olBitmapU05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[4])}},
    {"olBitmapV05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[4])}},
    {"osdDec400TableInput05", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[4])}},

    {"olInput06", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[5])}},
    {"olFormat06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[5])}},
    {"olAlpha06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[5])}},
    {"olWidth06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[5])}},
    {"olCropWidth06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[5])}},
    {"olHeight06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[5])}},
    {"olCropHeight06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[5])}},
    {"olXoffset06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[5])}},
    {"olCropXoffset06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[5])}},
    {"olYoffset06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[5])}},
    {"olCropYoffset06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[5])}},
    {"olYStride06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[5])}},
    {"olUVStride06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[5])}},
    {"olBitmapY06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[5])}},
    {"olBitmapU06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[5])}},
    {"olBitmapV06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[5])}},
    {"osdDec400TableInput06", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[5])}},

    {"olInput07", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[6])}},
    {"olFormat07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[6])}},
    {"olAlpha07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[6])}},
    {"olWidth07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[6])}},
    {"olCropWidth07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[6])}},
    {"olHeight07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[6])}},
    {"olCropHeight07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[6])}},
    {"olXoffset07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[6])}},
    {"olCropXoffset07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[6])}},
    {"olYoffset07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[6])}},
    {"olCropYoffset07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[6])}},
    {"olYStride07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[6])}},
    {"olUVStride07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[6])}},
    {"olBitmapY07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[6])}},
    {"olBitmapU07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[6])}},
    {"olBitmapV07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[6])}},
    {"osdDec400TableInput07", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[6])}},

    {"olInput08", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[7])}},
    {"olFormat08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[7])}},
    {"olAlpha08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[7])}},
    {"olWidth08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[7])}},
    {"olCropWidth08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[7])}},
    {"olHeight08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[7])}},
    {"olCropHeight08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[7])}},
    {"olXoffset08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[7])}},
    {"olCropXoffset08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[7])}},
    {"olYoffset08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[7])}},
    {"olCropYoffset08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[7])}},
    {"olYStride08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[7])}},
    {"olUVStride08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[7])}},
    {"olBitmapY08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[7])}},
    {"olBitmapU08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[7])}},
    {"olBitmapV08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[7])}},
    {"osdDec400TableInput08", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[7])}},

    {"olInput09", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[8])}},
    {"olFormat09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[8])}},
    {"olAlpha09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[8])}},
    {"olWidth09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[8])}},
    {"olCropWidth09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[8])}},
    {"olHeight09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[8])}},
    {"olCropHeight09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[8])}},
    {"olXoffset09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[8])}},
    {"olCropXoffset09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[8])}},
    {"olYoffset09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[8])}},
    {"olCropYoffset09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[8])}},
    {"olYStride09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[8])}},
    {"olUVStride09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[8])}},
    {"olBitmapY09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[8])}},
    {"olBitmapU09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[8])}},
    {"olBitmapV09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[8])}},
    {"osdDec400TableInput09", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[8])}},

    {"olInput10", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[9])}},
    {"olFormat10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[9])}},
    {"olAlpha10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[9])}},
    {"olWidth10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[9])}},
    {"olCropWidth10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[9])}},
    {"olHeight10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[9])}},
    {"olCropHeight10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[9])}},
    {"olXoffset10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[9])}},
    {"olCropXoffset10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[9])}},
    {"olYoffset10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[9])}},
    {"olCropYoffset10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[9])}},
    {"olYStride10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[9])}},
    {"olUVStride10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[9])}},
    {"olBitmapY10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[9])}},
    {"olBitmapU10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[9])}},
    {"olBitmapV10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[9])}},
    {"osdDec400TableInput10", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[9])}},

    {"olInput11", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[10])}},
    {"olFormat11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[10])}},
    {"olAlpha11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[10])}},
    {"olWidth11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[10])}},
    {"olCropWidth11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[10])}},
    {"olHeight11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[10])}},
    {"olCropHeight11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[10])}},
    {"olXoffset11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[10])}},
    {"olCropXoffset11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[10])}},
    {"olYoffset11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[10])}},
    {"olCropYoffset11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[10])}},
    {"olYStride11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[10])}},
    {"olUVStride11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[10])}},
    {"olBitmapY11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[10])}},
    {"olBitmapU11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[10])}},
    {"olBitmapV11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[10])}},
    {"osdDec400TableInput11", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[10])}},

    {"olInput12", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(olInput[11])}},
    {"olFormat12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olFormat[11])}},
    {"olAlpha12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olAlpha[11])}},
    {"olWidth12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olWidth[11])}},
    {"olCropWidth12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropWidth[11])}},
    {"olHeight12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olHeight[11])}},
    {"olCropHeight12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropHeight[11])}},
    {"olXoffset12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olXoffset[11])}},
    {"olCropXoffset12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropXoffset[11])}},
    {"olYoffset12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYoffset[11])}},
    {"olCropYoffset12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olCropYoffset[11])}},
    {"olYStride12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olYStride[11])}},
    {"olUVStride12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olUVStride[11])}},
    {"olBitmapY12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapY[11])}},
    {"olBitmapU12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapU[11])}},
    {"olBitmapV12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(olBitmapV[11])}},
    {"osdDec400TableInput12", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdDec400CompTableInput[11])}},
    /* Mosaic */
    {"mosaicEnables", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(mosaicEnables)}},
    {"mosArea01", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[0]), OFFSETOF(mosYoffset[0]),
                          OFFSETOF(mosWidth[0]), OFFSETOF(mosHeight[0])}},
    {"mosArea02", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[1]), OFFSETOF(mosYoffset[1]),
                          OFFSETOF(mosWidth[1]), OFFSETOF(mosHeight[1])}},
    {"mosArea03", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[2]), OFFSETOF(mosYoffset[2]),
                          OFFSETOF(mosWidth[2]), OFFSETOF(mosHeight[2])}},
    {"mosArea04", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[3]), OFFSETOF(mosYoffset[3]),
                          OFFSETOF(mosWidth[3]), OFFSETOF(mosHeight[3])}},
    {"mosArea05", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[4]), OFFSETOF(mosYoffset[4]),
                          OFFSETOF(mosWidth[4]), OFFSETOF(mosHeight[4])}},
    {"mosArea06", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[5]), OFFSETOF(mosYoffset[5]),
                          OFFSETOF(mosWidth[5]), OFFSETOF(mosHeight[5])}},
    {"mosArea07", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[6]), OFFSETOF(mosYoffset[6]),
                          OFFSETOF(mosWidth[6]), OFFSETOF(mosHeight[6])}},
    {"mosArea08", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[7]), OFFSETOF(mosYoffset[7]),
                          OFFSETOF(mosWidth[7]), OFFSETOF(mosHeight[7])}},
    {"mosArea09", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[8]), OFFSETOF(mosYoffset[8]),
                          OFFSETOF(mosWidth[8]), OFFSETOF(mosHeight[8])}},
    {"mosArea10", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[9]), OFFSETOF(mosYoffset[9]),
                          OFFSETOF(mosWidth[9]), OFFSETOF(mosHeight[9])}},
    {"mosArea11", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[10]), OFFSETOF(mosYoffset[10]),
                          OFFSETOF(mosWidth[10]), OFFSETOF(mosHeight[10])}},
    {"mosArea12", '0', 1, U32_NUM_TYPE, 4, {OFFSETOF(mosXoffset[11]), OFFSETOF(mosYoffset[11]),
                          OFFSETOF(mosWidth[11]), OFFSETOF(mosHeight[11])}},
    //OSD_MAP, mostly support 12 colors.
    {"osdMapEnable", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapEnable)}},
    {"osdMapInput", '0', 1, CHAR_ARRAY_TYPE, 1, {OFFSETOF(osdMapInput[0])}},
    {"osdMapStride", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapStride)}},
    {"osdMapBlockSize", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapBlockSize)}},

    {"osdMapAlpha01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[0])}},
    {"osdMapY01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[0])}},
    {"osdMapU01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[0])}},
    {"osdMapV01", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[0])}},

    {"osdMapAlpha02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[1])}},
    {"osdMapY02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[1])}},
    {"osdMapU02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[1])}},
    {"osdMapV02", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[1])}},

    {"osdMapAlpha03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[2])}},
    {"osdMapY03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[2])}},
    {"osdMapU03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[2])}},
    {"osdMapV03", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[2])}},

    {"osdMapAlpha04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[3])}},
    {"osdMapY04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[3])}},
    {"osdMapU04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[3])}},
    {"osdMapV04", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[3])}},

    {"osdMapAlpha05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[4])}},
    {"osdMapY05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[4])}},
    {"osdMapU05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[4])}},
    {"osdMapV05", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[4])}},

    {"osdMapAlpha06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[5])}},
    {"osdMapY06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[5])}},
    {"osdMapU06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[5])}},
    {"osdMapV06", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[5])}},

    {"osdMapAlpha07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[6])}},
    {"osdMapY07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[6])}},
    {"osdMapU07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[6])}},
    {"osdMapV07", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[6])}},

    {"osdMapAlpha08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[7])}},
    {"osdMapY08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[7])}},
    {"osdMapU08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[7])}},
    {"osdMapV08", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[7])}},

    {"osdMapAlpha09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[8])}},
    {"osdMapY09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[8])}},
    {"osdMapU09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[8])}},
    {"osdMapV09", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[8])}},

    {"osdMapAlpha10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[9])}},
    {"osdMapY10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[9])}},
    {"osdMapU10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[9])}},
    {"osdMapV10", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[9])}},

    {"osdMapAlpha11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[10])}},
    {"osdMapY11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[10])}},
    {"osdMapU11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[10])}},
    {"osdMapV11", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[10])}},

    {"osdMapAlpha12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapAlpha[11])}},
    {"osdMapY12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapY[11])}},
    {"osdMapU12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapU[11])}},
    {"osdMapV12", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(osdMapV[11])}},

    {"mosSizeIndex", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(mosSizeIndex)}},
    {"AXIAlignment", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(AXIAlignment)}},
    {"irqTypeMask", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(irqTypeMask)}},
    {"useVcmd", '3', 1, I32_NUM_TYPE, 1, {OFFSETOF(useVcmd)}},
    {"useDec400", '3', 1, I32_NUM_TYPE, 1, {OFFSETOF(useDec400)}},
    {"useL2Cache", '3', 1, I32_NUM_TYPE, 1, {OFFSETOF(useL2Cache)}},
    {"useAXIFE", '3', 1, I32_NUM_TYPE, 1, {OFFSETOF(useAXIFE)}},
    {"sramPowerdownDisable", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(sramPowerdownDisable)}},
    {"sramPowerdownMode", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(sramPowerdownMode)}},
    {"sramPowerdownTimerDiv32", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(sramPowerdownTimerDiv32)}},
    /*AXI max burst length */
    {"burstMaxLength", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(burstMaxLength)}},
    {"logOutDir", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(logOutDir)}},
    {"logOutLevel", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(logOutLevel)}},
    {"logTraceMap", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(logTraceMap)}},
    {"dumpRegister", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(dumpRegister)}},
    /* UFBC enable */
    {"ufbcMode", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(ufbcMode)}},
    {"ufbcYuvTrans", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(ufbcYuvTrans)}},
    {"ufbcBlockType", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(ufbcBlockType)}},
    {"ufbcBlockSplit", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(ufbcBlockSplit)}},
    {"ufbcConstantVal", '0', 1, STR_TYPE, 1, {OFFSETOF(ufbcConstantVal)}},
    {"secure_mode", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(secure_mode)}},
    {"priority", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(priority)}},
    {"core_mask", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(core_mask)}},
    {"encDevice", '0', 1, STR_TYPE, 1, {OFFSETOF(encDevice)}},
    {"memDevice", '0', 1, STR_TYPE, 1, {OFFSETOF(memDevice)}},
    {"lowlatGatingDisable", '0', 1, U32_NUM_TYPE, 1, {OFFSETOF(lowlatGatingDisable)}},
    {NULL, 0, 0, 0, 1, {0}}};

typedef struct {
  i32 frame[MOVING_AVERAGE_FRAMES];
  i32 length;
  i32 count;
  i32 pos;
  i32 frameRateNumer;
  i32 frameRateDenom;
} ma_s;
/*
static u32 osdMapDefaultYUV[4][12] = {
    {220, 200, 255, 255, 50, 100, 255, 100, 100, 100, 200, 200},
    {16, 235, 62, 172, 31, 219, 188, 78, 125, 39, 94, 23},
    {128, 15, 102, 41, 16, 16, 15, 10, 15, 115, 84, 71},
    {128, 127, 239, 26, 117, 138, 16, 229, 128, 184, 76, 122}};//default OSDMap{alpha,Y,U,V}
*/
/* SW/HW shared memories for input/output buffers */
EWLLinearMem_t pictureMem;
EWLLinearMem_t dec400CompTblMem;
EWLLinearMem_t outbufMem[MAX_STRM_BUF_NUM];
EWLLinearMem_t overlayMem[MAX_OVERLAY_NUM];
EWLLinearMem_t osdDec400CompTblMem[MAX_OVERLAY_NUM];
EWLLinearMem_t osdMapMem;
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
EWLLinearMem_t sliceinfoMem;
#endif

EWLLinearMem_t roimapMem;

typedef struct {
  int argc;
  char **argv;
} MainArgs;

#ifdef __FREERTOS__
typedef void *RET_TYPE;
u8 user_freertos_vcmd_en =
    1;  //used for use_freertos to switch normal_driver or vcmd_driver
static pthread_t tid_task;
#else
typedef int RET_TYPE;
#endif /* __FREERTOS__ */

/* Test bench definition of comment header */
#ifdef TB_DEFINED_COMMENT
/* COM data */
static u8 comment[] = "This is Hantro's test COM data header.";
static u32 comLen = sizeof(comment) / sizeof(u8) - 1;
#endif

static JpegEncCfg cfg;

static u32 writeOutput = 1;

/* Logic Analyzer trigger point */
i32 trigger_point = -1;

u32 thumbDataLength;
u8 *thumbData = NULL; /* thumbnail data buffer */

/* input mb line buffer struct */
static inputLineBufferCfg inputMbLineBuf;
static SegmentCtl_s streamSegCtl;

typedef struct  {
  u32 dec400Enable;
  /* point to hw config */
  const EWLHwConfig_t *HwCfg;
  /* overlay file handles */
  FILE *olFile[MAX_OVERLAY_NUM];
  /* dec400 table file for overlay 0 */
  FILE *osdDec400TableFile[MAX_OVERLAY_NUM];
} TestBench_s;

/* print cml log */
char *cmdl_all_log;

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
RET_TYPE MainTask(void *args);
static void FreeRes(JpegEncInst enc);
static int AllocRes(TestBench_s *tb, commandLine_s *cmdl, JpegEncInst encoder);
static int OpenEncoder(commandLine_s *cml, JpegEncInst *encoder);
static void CloseEncoder(JpegEncInst encoder);
static int ReadPic(u8 *image, i32 width, i32 height, i32 sliceNum,
                   i32 sliceRows, i32 frameNum, char *name, u32 inputMode,
                   u32 alignment, u32 ufbcMode, u32 ufbcBlockType, u32 scanType);
static int Parameter(i32 argc, char **argv, commandLine_s *ep);
static void Help(void);
static void WriteStrm(FILE *fout, u32 *outbuf, u32 size, u32 endian);
static void writeStrmBufs(FILE *fout, EWLLinearMem_t *bufs, u32 offset,
                          u32 size, u32 invalid_size, u32 endian);
static u32 GetResolution(char *filename, i32 *pWidth, i32 *pHeight);

#ifdef SEPARATE_FRAME_OUTPUT
static void WriteFrame(char *filename, u32 *strmbuf, u32 size);
static void writeFrameBufs(char *filename, EWLLinearMem_t *bufs, u32 offset,
                           u32 size);
#endif
static i32 InitInputLineBuffer(inputLineBufferCfg *lineBufCfg,
                               JpegEncCfg *encCfg, JpegEncIn *encIn,
                               JpegEncInst inst);
static void SetInputLineBuffer(inputLineBufferCfg *lineBufCfg,
                               JpegEncCfg *encCfg, JpegEncIn *encIn,
                               JpegEncInst inst, i32 sliceIdx);
static void EncStreamSegmentReady(void *cb_data);
static i32 InitStreamSegmentCrl(SegmentCtl_s *ctl, commandLine_s *cml,
                                FILE *out, JpegEncIn *encIn);
static void getAlignedPicSizebyFormat(JpegEncFrameType type, u32 width,
                                      u32 height, u32 alignment, u64 *luma_Size,
                                      u64 *chroma_Size, u64 *picture_Size, u32 scan_type);
static i32 JpegReadDEC400Data(JpegEncInst encoder, u8 *compDataBuf,
                              u8 *compTblBuf, u32 inputFormat, u32 src_width,
                              u32 src_height, char *inputDataFile,
                              FILE *dec400Table, i32 num,
                              i32 sliceNum, i32 sliceRows,
							  u32 alignment, u32 scanType,
							  u32 dec400Enable, u32 dec400TSHeaderEnable);
static int ReadFilter(commandLine_s *cmdl);
static int ReadRoimap(commandLine_s *cmdl, const void *ewl_inst);
static i8 CmlLog(char *p_argv, commandLine_s *cmdl);
static u32 SetDec400Enable(commandLine_s *cmdl);
static i32 file_read(FILE *file, u8 *data, u64 seek, size_t size) {
  if ((file == NULL) || (data == NULL)) return NOK;

  fseeko(file, seek, SEEK_SET);
  if (fread(data, sizeof(u8), size, file) < size) {
    if (!feof(file)) {
      return NOK;
    }
    return NOK;
  }

  return OK;
}

// Helper function to calculate time diffs.
unsigned int uTimeDiff(struct timeval end, struct timeval start) {
  return (end.tv_sec - start.tv_sec) * 1000000 + (end.tv_usec - start.tv_usec);
}

void JPEGtransYUVtoTile32format(commandLine_s *cml) {
  u8 *transform_buf = NULL;
  u8 *picture_buf = NULL;
  u32 i, j, k, col, row, p;
  u8 *start_add, *src_addr, *tile_addr, *cb_start_add, *cr_start_add,
      *cb_tile_add, *cr_tile_add, *cb_src_add, *cr_src_add;
  u32 transform_size, pictureSize;
  u64 seek;
  u8 *lum, *cb, *cr, *out;
  FILE *yuv_out = NULL;
  FILE *yuv_in = NULL;
  u32 mb_total = 0;

  /* Default resolution, try parsing input file name */
  if (cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT) {
    if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc)) {
      /* No dimensions found in filename, using default QCIF */
      cml->lumWidthSrc = 176;
      cml->lumHeightSrc = 144;
    }
  }

  if (cml->lumWidthSrc > 63535 || cml->lumHeightSrc > 63535) {
    /* using default QCIF */
    cml->lumWidthSrc = 176;
    cml->lumHeightSrc = 144;
  }

  if (cml->formatCustomizedType == 0)  //hevc
    transform_size = ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1))) *
                     ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1))) *
                     JpegEncGetBitsPerPixel(cml->frameType) / 8;
  else  //h264
  {
    mb_total = ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 16 *
               ((cml->lumHeightSrc + 16 - 1) & (~(16 - 1))) / 16;
    transform_size = mb_total / 5 * 2048 + mb_total % 5 * 400;
  }

  out = transform_buf = malloc(transform_size);
  if (transform_buf == NULL) return;

  pictureSize = (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc *
                JpegEncGetBitsPerPixel(cml->frameType) / 8;
  if (pictureSize <= 65535) picture_buf = malloc(pictureSize);
  if (picture_buf == NULL) {
    free(transform_buf);
    return;
  }
  lum = picture_buf;
  cb = lum + (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc;
  cr = cb + (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc / 4;

  yuv_out = fopen("trans_to_tile_format", "w");
  if (yuv_out == NULL) {
    free(transform_buf);
    free(picture_buf);
    return;
  }

  yuv_in = fopen(cml->input, "r");
  if (yuv_in == NULL) {
    free(transform_buf);
    free(picture_buf);
    fclose(yuv_out);
    return;
  }

  for (p = cml->firstPic; p <= cml->lastPic; p++) {
    seek = ((u64)p) * ((u64)pictureSize);
    if (file_read(yuv_in, lum, seek, pictureSize)) goto end;

    if (cml->formatCustomizedType == 0)  //hevc format
    {
      printf("transform YUV to DH HEVC\n");
      u32 row_32 = ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1))) / 32;
      u32 num32_per_row = ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1))) / 32;
      //luma
      for (i = 0; i < row_32; i++) {
        start_add = lum + i * 32 * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
        for (j = 0; j < num32_per_row * 2; j++) {
          tile_addr = start_add + j * 16;
          if (j < (((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 16)) {
            for (k = 0; k < 32; k++) {
              if ((i * 32 + k) >= cml->lumHeightSrc) {
                for (col = 0; col < 16; col++) {
                  *transform_buf++ = 0;
                }
              } else {
                src_addr = tile_addr;
                for (col = 0; col < 16; col++) {
                  *transform_buf++ = *(src_addr + col);
                }
                tile_addr =
                    tile_addr + ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
              }
            }
          } else {
            for (k = 0; k < 32; k++) {
              for (col = 0; col < 16; col++) {
                *transform_buf++ = 0;
              }
            }
          }
        }
      }
      //chroma
      for (i = 0; i < row_32; i++) {
        cb_start_add =
            cb + i * 16 * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
        cr_start_add =
            cr + i * 16 * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;

        for (j = 0; j < num32_per_row; j++) {
          cb_tile_add = cb_start_add + j * 16;
          cr_tile_add = cr_start_add + j * 16;

          for (k = 0; k < 16; k++) {
            if ((i * 16 + k) >= (cml->lumHeightSrc / 2)) {
              for (col = 0; col < 32; col++) {
                *transform_buf++ = 0;
              }
            } else {
              cb_src_add = cb_tile_add;
              cr_src_add = cr_tile_add;
              //cb
              for (col = 0; col < 16; col++) {
                if (16 * j + col >=
                    (((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2))
                  *transform_buf++ = 0;
                else
                  *transform_buf++ = *(cb_src_add + col);
              }
              //cr
              for (col = 0; col < 16; col++) {
                if (16 * j + col >=
                    (((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2))
                  *transform_buf++ = 0;
                else
                  *transform_buf++ = *(cr_src_add + col);
              }
              cb_tile_add =
                  cb_tile_add + ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
              cr_tile_add =
                  cr_tile_add + ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
            }
          }
        }
      }
    } else  //h264 format
    {
      printf("transform YUV to DH H264\n");
      u32 row_16 = ((cml->lumHeightSrc + 16 - 1) & (~(16 - 1))) / 16;
      u32 mb_per_row = ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 16;
      u32 mb_total = 0;
      for (i = 0; i < row_16; i++) {
        start_add = lum + i * 16 * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
        cb_start_add =
            cb + i * 8 * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
        cr_start_add =
            cr + i * 8 * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
        for (j = 0; j < mb_per_row; j++) {
          tile_addr = start_add + 16 * j;
          cb_tile_add = cb_start_add + 8 * j;
          cr_tile_add = cr_start_add + 8 * j;
          for (k = 0; k < 4; k++) {
            for (row = 0; row < 4; row++) {
              //luma
              if ((i * 16 + k * 4 + row) < cml->lumHeightSrc) {
                src_addr =
                    tile_addr +
                    (k * 4 + row) * ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1)));
                memcpy(transform_buf, src_addr, 16);
                transform_buf += 16;
              } else {
                memset(transform_buf, 0, 16);
                transform_buf += 16;
              }
            }

            //cb
            for (row = 0; row < 2; row++) {
              if ((i * 8 + k * 2 + row) < cml->lumHeightSrc / 2) {
                src_addr = cb_tile_add +
                           (k * 2 + row) *
                               ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
                memcpy(transform_buf, src_addr, 8);
                transform_buf += 8;
              } else {
                memset(transform_buf, 0, 8);
                transform_buf += 8;
              }
            }

            //cr
            for (row = 0; row < 2; row++) {
              if ((i * 8 + k * 2 + row) < cml->lumHeightSrc / 2) {
                src_addr = cr_tile_add +
                           (k * 2 + row) *
                               ((cml->lumWidthSrc + 16 - 1) & (~(16 - 1))) / 2;
                memcpy(transform_buf, src_addr, 8);
                transform_buf += 8;
              } else {
                memset(transform_buf, 0, 8);
                transform_buf += 8;
              }
            }
          }
          memset(transform_buf, 0, 16);
          transform_buf += 16;
          mb_total++;
          if (mb_total % 5 == 0) {
            memset(transform_buf, 0, 48);
            transform_buf += 48;
          }
        }
      }
    }

    u32 output_size = 0;

    if (cml->formatCustomizedType == 0)
      output_size =
          ((cml->lumWidthSrc + 32 - 1) & (~(32 - 1))) *
          ((cml->lumHeightSrc + 32 - 1) & (~(32 - 1))) *
          JpegEncGetBitsPerPixel(JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32) / 8;
    else
      output_size = mb_total / 5 * 2048 + mb_total % 5 * 400;

    for (i = 0; i < output_size; i++)
      fwrite((u8 *)(out) + i, sizeof(u8), 1, yuv_out);

    transform_buf = out;
  }
end:
  fclose(yuv_out);
  fclose(yuv_in);
  free(picture_buf);
  free(out);
  strcpy(cml->input, "trans_to_tile_format");
  cml->lastPic = cml->lastPic - cml->firstPic;
  cml->firstPic = 0;
  if (cml->formatCustomizedType == 0) {
    cml->frameType = JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32;
    cml->horOffsetSrc = cml->horOffsetSrc & (~(32 - 1));
    cml->verOffsetSrc = cml->verOffsetSrc & (~(32 - 1));
    cml->restartInterval = (cml->restartInterval + 1) / 2 * 2;
  } else {
    cml->frameType = JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4;
    cml->horOffsetSrc = cml->horOffsetSrc & (~(16 - 1));
    cml->verOffsetSrc = cml->verOffsetSrc & (~(16 - 1));
  }
}

void JPEGtransTo4228bitformat(commandLine_s *cml) {
  u8 *transform_buf = NULL;
  u8 *picture_buf = NULL;
  u32 i, j, k, p;
  u32 transform_size = 0;
  u64 pictureSize;
  u64 seek;
  u8 *lum, *cb, *cr, *out;
  FILE *yuv_out = NULL;
  FILE *yuv_in = NULL;

  /* Default resolution, try parsing input file name */
  if (cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT) {
    if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc)) {
      /* No dimensions found in filename, using default QCIF */
      cml->lumWidthSrc = 176;
      cml->lumHeightSrc = 144;
    }
  }
  if ((cml->lumWidthSrc > 65535) || (cml->lumHeightSrc > 65535)) return;

  if (cml->formatCustomizedType == 2)  //422_888
    transform_size = cml->lumWidthSrc * cml->lumHeightSrc * 2;

  out = transform_buf = malloc(transform_size);
  if (transform_buf == NULL) return;

  pictureSize = (u64)(cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc *
                JpegEncGetBitsPerPixel(cml->frameType) / 8;

  picture_buf = malloc(pictureSize);
  if (picture_buf == NULL) {
    free(transform_buf);
    return;
  }

  lum = picture_buf;
  cb = lum + (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc;
  cr = cb + (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc / 4;

  yuv_out = fopen("trans_to_422_888_format", "w");
  if (yuv_out == NULL) {
    free(transform_buf);
    free(picture_buf);
    return;
  }
  yuv_in = fopen(cml->input, "r");
  if (yuv_in == NULL) {
    free(transform_buf);
    free(picture_buf);
    fclose(yuv_out);
    return;
  }
  for (p = cml->firstPic; p <= cml->lastPic; p++) {
    seek = ((u64)p) * ((u64)pictureSize);
    if (file_read(yuv_in, lum, seek, pictureSize)) goto end;

    if (cml->formatCustomizedType == 2) {
      printf("transform YUV 420 to 422 888\n");

      //luma
      for (i = 0; i < cml->lumWidthSrc * cml->lumHeightSrc; i++) {
        memcpy(transform_buf++, lum + i, 1);
      }

      //chroma
      for (i = 0; i < cml->lumHeightSrc / 2; i++) {
        for (j = 0; j < 2; j++) {
          for (k = 0; k < cml->lumWidthSrc / 2; k++) {
            memcpy(transform_buf++, cb + cml->lumWidthSrc / 2 * i + k, 1);
            memcpy(transform_buf++, cr + cml->lumWidthSrc / 2 * i + k, 1);
          }
        }
      }
    }

    for (i = 0; i < transform_size; i++)
      fwrite((u8 *)(out) + i, sizeof(u8), 1, yuv_out);

    transform_buf = out;
  }
end:
  fclose(yuv_out);
  fclose(yuv_in);
  free(picture_buf);
  free(out);
  strcpy(cml->input, "trans_to_422_888_format");
  cml->lastPic = cml->lastPic - cml->firstPic;
  cml->firstPic = 0;
  cml->frameType = JPEGENC_YUV422SP_888;
}

void JPEGtransToTile4format(commandLine_s *cml) {
  u8 *transform_buf = NULL, *picture_buf = NULL;
  u8 *lum, *cb, *cr, *out;
  u64 picture_size, transform_size, seek;
  u32 x, y, i, p;
  u64 pic_lum_size, pic_chroma_size;
  u32 alignment = (cml->exp_of_input_alignment == 0)
                      ? 1
                      : (1 << cml->exp_of_input_alignment);
  u32 byte_per_compt = 0;

  FILE *yuv_out = NULL;
  FILE *yuv_in = NULL;

  getAlignedPicSizebyFormat(cml->frameType, cml->lumWidthSrc, cml->lumHeightSrc,
                            alignment, &pic_lum_size, &pic_chroma_size,
                            &picture_size, cml->scanType);
  switch (cml->frameType) {
    case JPEGENC_YUV420_SEMIPLANAR:
      byte_per_compt = 1;
      cml->frameType = JPEGENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4;
      break;
    case JPEGENC_YUV420_SEMIPLANAR_VU:
      byte_per_compt = 1;
      cml->frameType = JPEGENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4;
      break;
    case JPEGENC_YUV420_MS_P010:
      byte_per_compt = 2;
      cml->frameType = JPEGENC_YUV420_PLANAR_10BIT_P010_TILE_4_4;
  }

  getAlignedPicSizebyFormat(cml->frameType, cml->lumWidthSrc, cml->lumHeightSrc,
                            alignment, NULL, NULL, &transform_size, cml->scanType);

  out = transform_buf = malloc(transform_size);
  if (transform_buf == NULL) return;

  picture_buf = malloc(picture_size);
  if (picture_buf == NULL) {
    free(transform_buf);
    return;
  }

  lum = picture_buf;
  cb = lum + pic_lum_size;

  yuv_out = fopen("trans_to_fb_format", "w");
  if (yuv_out == NULL) {
    free(transform_buf);
    free(picture_buf);
    return;
  }

  yuv_in = fopen(cml->input, "r");
  if (yuv_in == NULL) {
    free(transform_buf);
    free(picture_buf);
    fclose(yuv_out);
    return;
  }

  printf("transform YUV to FB format\n");

  for (p = cml->firstPic; p <= cml->lastPic; p++) {
    seek = ((u64)p) * ((u64)picture_size);
    if (file_read(yuv_in, lum, seek, picture_size)) goto end;

    u32 stride = (cml->lumWidthSrc * 4 * byte_per_compt + alignment - 1) &
                 (~(alignment - 1));

    //luma
    for (x = 0; x < cml->lumWidthSrc / 4; x++) {
      for (y = 0; y < cml->lumHeightSrc; y++)
        memcpy(transform_buf + y % 4 * 4 * byte_per_compt + stride * (y / 4) +
                   x * 16 * byte_per_compt,
               lum + y * ((cml->lumWidthSrc + 15) & (~15)) * byte_per_compt +
                   x * 4 * byte_per_compt,
               4 * byte_per_compt);
    }

    transform_buf += stride * cml->lumHeightSrc / 4;

    //chroma
    for (x = 0; x < cml->lumWidthSrc / 4; x++) {
      for (y = 0; y < ((cml->lumHeightSrc / 2) + 3) / 4 * 4; y++)
        memcpy(transform_buf + y % 4 * 4 * byte_per_compt + stride * (y / 4) +
                   x * 16 * byte_per_compt,
               cb + y * ((cml->lumWidthSrc + 15) & (~15)) * byte_per_compt +
                   x * 4 * byte_per_compt,
               4 * byte_per_compt);
    }

    for (i = 0; i < transform_size; i++)
      fwrite((u8 *)(out) + i, sizeof(u8), 1, yuv_out);
    transform_buf = out;
  }

end:
  fclose(yuv_out);
  fclose(yuv_in);
  free(picture_buf);
  free(out);
  strcpy(cml->input, "trans_to_fb_format");
  cml->lastPic = cml->lastPic - cml->firstPic;
  cml->firstPic = 0;
}

void JPEGtransToCommdataformat(commandLine_s *cml) {
  u8 *transform_buf = NULL;
  u8 *picture_buf = NULL;
  u32 x, y, ix, iy, i, j, p;
  u64 transform_size = 0;
  u64 pictureSize;
  u64 seek;
  u8 *lum, *cb, *cr;
  FILE *yuv_out = NULL;
  FILE *yuv_in = NULL;
  u32 trans_format = 0;
  u32 byte_per_compt = 0;
  u8 *tile_start_addr, *dst_8_addr = NULL, *cb_start, *cr_start;
  u16 *dst_16_addr = NULL;

  /* Default resolution, try parsing input file name */
  if (cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT) {
    if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc)) {
      /* No dimensions found in filename, using default QCIF */
      cml->lumWidthSrc = 176;
      cml->lumHeightSrc = 144;
    }
  }
  if ((cml->lumWidthSrc > 65535) || (cml->lumHeightSrc > 65535)) return;

  if (cml->formatCustomizedType == 3)  //tile 4x4 8bit
    trans_format = JPEGENC_YUV420_8BIT_TILE_8_8;
  else if (cml->formatCustomizedType == 4)  //tile 4x4 10bit
    trans_format = JPEGENC_YUV420_10BIT_TILE_8_8;

  getAlignedPicSizebyFormat(trans_format, cml->lumWidthSrc, cml->lumHeightSrc,
                            0, NULL, NULL, &transform_size, cml->scanType);

  transform_buf = malloc(transform_size);
  if (transform_buf == NULL) return;

  pictureSize = (u64)(cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc *
                JpegEncGetBitsPerPixel(cml->frameType) / 8;
  picture_buf = malloc(pictureSize);
  if (picture_buf == NULL) {
    free(transform_buf);
    return;
  }

  lum = picture_buf;
  cb = lum + (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc;
  cr = cb + (cml->lumWidthSrc + 15) / 16 * 16 * cml->lumHeightSrc / 4;

  yuv_out = fopen("trans_to_tile8x8_commdata_format", "w");
  if (yuv_out == NULL) {
    free(transform_buf);
    free(picture_buf);
    return;
  }

  if (yuv_in == NULL) yuv_in = fopen(cml->input, "r");
  if (yuv_in == NULL) {
    free(transform_buf);
    free(picture_buf);
    fclose(yuv_out);
    return;
  }

  printf("transform YUV to CommData format\n");

  for (p = cml->firstPic; p <= cml->lastPic; p++) {
    seek = ((u64)p) * ((u64)pictureSize);
    if (file_read(yuv_in, lum, seek, pictureSize)) goto end;

    if (cml->formatCustomizedType == 3) {
      byte_per_compt = 1;
      dst_8_addr = transform_buf;
    } else {
      byte_per_compt = 2;
      dst_16_addr = (u16 *)transform_buf;
    }

    u32 orig_stride = cml->lumWidthSrc;

    //luma
    for (y = 0; y < ((cml->lumHeightSrc + 7) / 8); y++)
      for (x = 0; x < ((cml->lumWidthSrc + 7) / 8); x++) {
        tile_start_addr = lum + 8 * x + orig_stride * 8 * y;

        for (iy = 0; iy < 2; iy++)
          for (ix = 0; ix < 2; ix++)
            for (i = 0; i < 4; i++)
              if (cml->formatCustomizedType == 3) {
                memcpy(dst_8_addr,
                       tile_start_addr + orig_stride * i + 4 * ix +
                           orig_stride * 4 * iy,
                       4);
                dst_8_addr += 4;
              } else {
                u8 *tmp_addr = tile_start_addr + orig_stride * i + 4 * ix +
                               orig_stride * 4 * iy;
                u64 tmp = 0;
                for (j = 0; j < 4; j++)
                  tmp = tmp | (((u64)(*(tmp_addr + j)) << 8) << (16 * j));
                memcpy(dst_16_addr, &tmp, 4 * byte_per_compt);
                dst_16_addr += 4;
              }
      }

    //chroma
    for (y = 0; y < ((cml->lumHeightSrc / 2 + 3) / 4); y++) {
      for (x = 0; x < ((cml->lumWidthSrc + 15) / 16); x++) {
        cb_start = cb + 8 * x + orig_stride / 2 * 4 * y;
        cr_start = cr + 8 * x + orig_stride / 2 * 4 * y;

        for (i = 0; i < 4; i++) {
          for (j = 0; j < 16; j++) {
            if (j % 2 == 0) {
              if (cml->formatCustomizedType == 3)
                *dst_8_addr++ = *(cb_start + (j % 4) / 2 +
                                  orig_stride / 2 * (j / 4) + i * 2);
              else
                *dst_16_addr++ = *(cb_start + (j % 4) / 2 +
                                   orig_stride / 2 * (j / 4) + i * 2)
                                 << 8;
            } else {
              if (cml->formatCustomizedType == 3)
                *dst_8_addr++ = *(cr_start + (j % 4) / 2 +
                                  orig_stride / 2 * (j / 4) + i * 2);
              else
                *dst_16_addr++ = *(cr_start + (j % 4) / 2 +
                                   orig_stride / 2 * (j / 4) + i * 2)
                                 << 8;
            }
          }
        }
      }
    }

    if (cml->formatCustomizedType == 3)
      transform_size = dst_8_addr - transform_buf;
    else
      transform_size = (u8 *)dst_16_addr - (u8 *)transform_buf;

    for (i = 0; i < transform_size; i++)
      fwrite(transform_buf + i, sizeof(u8), 1, yuv_out);
  }

end:
  fclose(yuv_out);
  fclose(yuv_in);
  free(picture_buf);
  free(transform_buf);
  strcpy(cml->input, "trans_to_tile8x8_commdata_format");
  cml->lastPic = cml->lastPic - cml->firstPic;
  cml->firstPic = 0;
  if (cml->formatCustomizedType == 3)
    cml->frameType = JPEGENC_YUV420_8BIT_TILE_8_8;
  else
    cml->frameType = JPEGENC_YUV420_10BIT_TILE_8_8;
}

void getAlignedPicSizebyFormat(JpegEncFrameType type, u32 width, u32 height,
                               u32 alignment, u64 *luma_Size, u64 *chroma_Size,
                               u64 *picture_Size, u32 scan_type) {
  u32 luma_stride = 0, chroma_stride = 0;
  u64 lumaSize = 0, chromaSize = 0, pictureSize = 0;

  JpegEncGetAlignedStride(width, type, &luma_stride, &chroma_stride, alignment, scan_type);
  switch (type) {
    case JPEGENC_Y8b:
    case JPEGENC_Y10bWL:
    case JPEGENC_Y10bWH:
      lumaSize = (u64)luma_stride * height;
      chromaSize = 0;
      break;
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YVU420_PLANAR:
      lumaSize = (u64)luma_stride * height;
      chromaSize = (u64)chroma_stride * height / 2 * 2;
      break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
      lumaSize = (u64)luma_stride * height;
      chromaSize = (u64)chroma_stride * height / 2;
      break;
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
    case JPEGENC_RGB888_24BIT:
    case JPEGENC_BGR888_24BIT:
    case JPEGENC_RBG888_24BIT:
    case JPEGENC_GBR888_24BIT:
    case JPEGENC_BRG888_24BIT:
    case JPEGENC_GRB888_24BIT:
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    case JPEGENC_RGBX8888:
    case JPEGENC_BGRX8888:
    case JPEGENC_RGBX1010102:
    case JPEGENC_BGRX1010102:
      lumaSize = (u64)luma_stride * height;
      if (scan_type == JPEG_SUPERTILEX_SCAN)  lumaSize = (u64)luma_stride * ((height + 63) / 64); //height align to 64 when supertile.
      chromaSize = 0;
      break;
    case JPEGENC_YUV420_I010:
      lumaSize = (u64)luma_stride * height;
      chromaSize = (u64)chroma_stride * height / 2 * 2;
      break;
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010:
      lumaSize = (u64)luma_stride * height;
      chromaSize = (u64)chroma_stride * height / 2;
      break;
    case JPEGENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
      lumaSize = luma_stride * height * 10 / 8 ;
      chromaSize = chroma_stride * height / 2 * 2 * 10 / 8;
      break;
    case JPEGENC_YUV400_PLANAR_10BIT_PACKED:
      lumaSize = luma_stride * height * 10 / 8 ;
      chromaSize = 0;
      break;
    case JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32:
      lumaSize = (u64)luma_stride * height;
      chromaSize = (u64)lumaSize / 2;
      break;
    case JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4:
      lumaSize = (u64)luma_stride * height * 2 * 12 / 8;
      chromaSize = 0;
      break;
    case JPEGENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case JPEGENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
    case JPEGENC_YUV420_PLANAR_10BIT_P010_TILE_4_4:
      lumaSize = (u64)luma_stride * ((height + 3) / 4);
      chromaSize = (u64)chroma_stride * (((height / 2) + 3) / 4);
      break;
    case JPEGENC_YUV420_SEMIPLANAR_101010:
      lumaSize = luma_stride * height;
      chromaSize = chroma_stride * height / 2;
      break;
    case JPEGENC_YUV422SP_888:
    case JPEGENC_YVU422SP_888:
      lumaSize = (u64)luma_stride * height;
      chromaSize = (u64)chroma_stride * height;
      break;
    case JPEGENC_YUV420_8BIT_TILE_8_8:
    case JPEGENC_YVU420_8BIT_TILE_8_8:
      lumaSize = (u64)luma_stride * ((height + 7) / 8);
      chromaSize = (u64)chroma_stride * (((height / 2) + 3) / 4);
      break;
    case JPEGENC_YUV420_10BIT_TILE_8_8:
      lumaSize = (u64)luma_stride * ((height + 7) / 8);
      chromaSize = (u64)chroma_stride * (((height / 2) + 3) / 4);
      break;
    case JPEGENC_YUV420_FBC64:
    case JPEGENC_YUV420_UV_8BIT_TILE_128_2:
    case JPEGENC_YUV420_UV_10BIT_TILE_128_2:
      lumaSize = luma_stride * ((height + 1) / 2);
      chromaSize = chroma_stride * (((height / 2) + 1) / 2);
      break;
    case JPEGENC_YUV420SP10b:
      lumaSize = luma_stride * 10 / 8 * height;
      chromaSize = chroma_stride * height / 2 * 10 / 8;
      break;
    case JPENC_YUV420_10BIT_PACKED_Y0L2:
      lumaSize = luma_stride * 2 * 2 * height /2;
      chromaSize = 0;
      break;
    default:
      printf("not support this format\n");
      chromaSize = lumaSize = 0;
      break;
  }

  pictureSize = lumaSize + chromaSize;
  if (luma_Size != NULL) *luma_Size = lumaSize;
  if (chroma_Size != NULL) *chroma_Size = chromaSize;
  if (picture_Size != NULL) *picture_Size = pictureSize;
}

void GetOsdDec400Size(const void *wl_instance, commandLine_s *cmdl, u8 idx, u32 input_alignment,
                      u32 dec400TableSize[], u32 *dec400TotalTableSize) {
  u32 lumaSize = 0, chromaSize = 0, pictureSize = 0;
  u32 tileSize = 256;
  u32 bits_tile_in_table = 4;
  u32 planar420_cbcr_table_style = 0; /* 0-separated 1-continuous */
  u32 format = cmdl->olFormat[idx];

  //ASSERT(cmdl->olFormat[idx] == 0 && cmdl->olSuperTile[idx]);
  if (cmdl->olFormat[idx] == 0)
    format = JPEGENC_RGB888;
  EncGetDec400TsBufferSize(format, cmdl->olWidth[idx], cmdl->olHeight[idx],
  input_alignment, &lumaSize, &chromaSize, &pictureSize, cmdl->scanType, 0, 0);
  /* Since SuperTile format will always be 64x64 aligned, no need to take care tile align */
  if (cmdl->olSuperTile[idx]) {
    u32 luma_stride = cmdl->olYStride[idx];
    u32 height = (cmdl->olHeight[idx] + 63) / 64;
    pictureSize =
      STRIDE(luma_stride * height / (tileSize / 2) * bits_tile_in_table, 8) / 8;
  }
  *dec400TotalTableSize = pictureSize;
  dec400TableSize[0] = lumaSize;
  dec400TableSize[1] = chromaSize;
  dec400TableSize[2] = 0;
}

u32 GetOsdmapReadHeight(commandLine_s *cml) {
  u32 BlockSize = cml->osdMapBlockSize;
  u32 fillSize = 16;
  u32 filledHeight = ((cml->height  + fillSize - 1) / fillSize * fillSize);
  u32 filledWidth = ((cml->width  + fillSize - 1) / fillSize * fillSize);
  u32 read_height = (((cml->rotation == 1 || cml->rotation == 2) ? filledWidth : filledHeight) + BlockSize - 1) / BlockSize;

  return read_height;
}

/*------------------------------------------------------------------------------
    Add new frame bits for moving average bitrate calculation
------------------------------------------------------------------------------*/
void MaAddFrame(ma_s *ma, i32 frameSizeBits) {
  ma->frame[ma->pos++] = frameSizeBits;

  if (ma->pos == ma->length) ma->pos = 0;

  if (ma->count < ma->length) ma->count++;
}

/*------------------------------------------------------------------------------
    Calculate average bitrate of moving window
------------------------------------------------------------------------------*/
i32 Ma(ma_s *ma) {
  i32 i;
  unsigned long long sum = 0; /* Using 64-bits to avoid overflow */

  for (i = 0; i < ma->count; i++) sum += ma->frame[i];

  if (!ma->frameRateDenom) return 0;

  sum = sum / ma->count;

  return sum * (ma->frameRateNumer + ma->frameRateDenom - 1) /
         ma->frameRateDenom;
}

/*------------------------------------------------------------------------------
    Read overlay input file and set up overlay buffer
------------------------------------------------------------------------------*/
void SetupOverlayBuffer(TestBench_s *tb, JpegEncIn *pEncIn, i32 frameNum, commandLine_s *cml, u32 osdMapFrameNum) {
  int i, j;
  u8 *lum, *cb, *cr;
  u32 src_width, size_luma, size_chroma;
  u64 seek;
  u32 overlay_size = 0;
  u32 dec400TotalTableSize = 0;
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    pEncIn->busOlLum[i] = 0;
    pEncIn->busOlCb[i] = 0;
    pEncIn->busOlCr[i] = 0;
    pEncIn->overlayEnable[i] = cfg.olEnable[i];
    if (cfg.olEnable[i]) {
      int num = frameNum % cfg.olFrameNum[i];
      u32 y_stride = cfg.olYStride[i];
      u32 uv_stride = cfg.olUVStride[i];
      u32 read_height = cfg.olHeight[i];
      pEncIn->busOlLum[i] = overlayMem[i].busAddress;
      lum = (u8 *)overlayMem[i].virtualAddress;
      overlay_size = 0;

      switch (cfg.olFormat[i]) {
        case 0:  //ARGB
          src_width = cfg.olWidth[i] * 4;
          seek = num * (cfg.olHeight[i] * cfg.olWidth[i] * 4);
          if (cfg.olSuperTile[i]) {
            src_width = cfg.olYStride[i];
            read_height = (cfg.olHeight[i] + 63) / 64;
            seek = num * src_width * read_height;
          }
          if (cfg.osdDec400TableFile[i] && osdDec400CompTblMem[i].virtualAddress) {
            seek = num * (cfg.olHeight[i] * cfg.olYStride[i]);
            if (file_read(tb->olFile[i], lum, seek, cfg.olHeight[i] * cfg.olYStride[i])) {
              printf("Error: fail to read overlay\n");
              pEncIn->overlayEnable[i] = 0;
              break;
            }
          } else {
            for (j = 0; j < read_height; j++) {
              if (file_read(tb->olFile[i], lum, seek, src_width)) {
                printf("Error: fail to read overlay\n");
                pEncIn->overlayEnable[i] = 0;
                break;
              } else {
                seek += src_width;
                lum += y_stride;
              }
            }
          }
          overlay_size = y_stride * read_height;
          break;
        case 1:  //NV12
          size_luma = cfg.olYStride[i] * cfg.olHeight[i];
          seek = num * ((cfg.olWidth[i] * cfg.olHeight[i]) +
                        (cfg.olWidth[i]) * cfg.olHeight[i] / 2);
          src_width = cfg.olWidth[i];
          pEncIn->busOlCb[i] = pEncIn->busOlLum[i] + size_luma;  //Cb and Cr
          cb = lum + size_luma;
          if (cfg.osdDec400TableFile[i] && osdDec400CompTblMem[i].virtualAddress) {
            seek = num * (cfg.olHeight[i] * cfg.olYStride[i] * 3 / 2);
            if (file_read(tb->olFile[i], lum, seek, cfg.olHeight[i] * cfg.olYStride[i] * 3 / 2)) {
              printf("Error: fail to read overlay\n");
              pEncIn->overlayEnable[i] = 0;
              break;
            }
          } else {
            //Luma
            for (j = 0; j < read_height; j++) {
              if (file_read(tb->olFile[i], lum, seek, src_width)) {
                printf("Error: fail to read overlay\n");
                pEncIn->overlayEnable[i] = 0;
                break;
              } else {
                seek += src_width;
                lum += y_stride;
              }
            }
            //Cb and Cr
            for (j = 0; j < (read_height / 2); j++) {
              if (file_read(tb->olFile[i], cb, seek, src_width)) {
                //This situation means corrupted input, so do not rolling back
                pEncIn->overlayEnable[i] = 0;
                break;
              }
              seek += src_width;
              cb += uv_stride;
            }
          }
          size_chroma = uv_stride * (read_height / 2);
          overlay_size = size_luma + size_chroma;
          break;
        case 2:  //Bitmap
          /* Foreground pixels in bitmap need to be 2x2 aligned, otherwise there will be avoidless chroma
             artifact cased by YUV420 coding. This chroma artifact is affected by alpha value, background
             chroma, foreground bitmap chroma. */
          src_width = cfg.olWidth[i] / 8;
          seek = num * cfg.olHeight[i] * cfg.olWidth[i] / 8;
          for (j = 0; j < read_height; j++) {
            if (file_read(tb->olFile[i], lum, seek, src_width)) {
              printf("Error: fail to read overlay\n");
              pEncIn->overlayEnable[i] = 0;
              break;
            } else {
              seek += src_width;
              lum += y_stride;
            }
          }
          overlay_size = y_stride * read_height;
          break;
//        defualt:
//          break;
      }
    }
    EWLSyncMemData(&overlayMem[i], 0, overlay_size, HOST_TO_DEVICE);
  }
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    pEncIn->osdDec400Enable[i] = 0;
    if (cfg.osdDec400TableFile[i] && osdDec400CompTblMem[i].virtualAddress) {
      pEncIn->osdDec400Enable[i] = 2;
      int num = frameNum % cfg.olFrameNum[i];
      dec400TotalTableSize = cfg.osdDec400TableSize[i][0] + cfg.osdDec400TableSize[i][1] +
        cfg.osdDec400TableSize[i][2];
      seek = ((u64)num) * ((u64)dec400TotalTableSize);
      pEncIn->osdDec400TableBase[i][0] = osdDec400CompTblMem[i].busAddress;
      pEncIn->osdDec400TableBase[i][1] = osdDec400CompTblMem[i].busAddress + cfg.osdDec400TableSize[i][0];
      pEncIn->osdDec400TableBase[i][2] = pEncIn->osdDec400TableBase[i][1] + cfg.osdDec400TableSize[i][1];
      lum = (u8 *)osdDec400CompTblMem[i].virtualAddress;

      if (file_read(cfg.osdDec400TableFile[i], lum, seek, dec400TotalTableSize)) {
        printf("Error: fail to read osd dec400 table file\n");
        pEncIn->osdDec400Enable[i] = 0;
      }
    } else {
      pEncIn->osdDec400Enable[i] = 1;
    }
  }
  if (cfg.osdMapEnable) {
    pEncIn->osdMapEnable = cfg.osdMapEnable;
    u32 read_height = GetOsdmapReadHeight(cml);
    int num = frameNum % osdMapFrameNum;
    u32 osdMapSize = 0;

    pEncIn->osdMapInputAddr = osdMapMem.busAddress;
    lum = (u8 *)osdMapMem.virtualAddress;
    src_width = cfg.osdMapStride;//unit: byte; 4bits per 8x8 block.
    seek = num * src_width * read_height;
    FILE* osdMapFile = fopen(cml->osdMapInput, "rb");
    for (j = 0; j < read_height; j++) {
      if (file_read(osdMapFile, lum, seek, src_width)) {
        printf("Error: fail to read osdMap\n");
        pEncIn->osdMapEnable = 0;
        break;
      } else {
        seek += src_width;
        lum += src_width;
      }
    }
    if (osdMapFile) fclose(osdMapFile);
    osdMapSize = src_width * read_height;
    EWLSyncMemData(&osdMapMem, 0, osdMapSize, HOST_TO_DEVICE);
  }
}
/*------------------------------------------------------------------------------

    main

------------------------------------------------------------------------------*/
int main(int argc, char *argv[]) {
  osal_thread_init();
  MainArgs args = {argc, argv};
#ifdef __FREERTOS__
#ifndef FREERTOS_SIMULATOR
  Platform_init();
#endif
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid_task, &attr, &MainTask, &args);
  vTaskStartScheduler();

  return 0;  //should not arrive here if start scheduler successfully in real env
#else
  return MainTask(&args);
#endif
}

RET_TYPE MainTask(void *args) {
  MainArgs *args_ = (MainArgs *)args;
  int argc = args_->argc;
  char **argv = args_->argv;

  TestBench_s tb;

  u32 retMain = 0;
  JpegEncInst encoder;
  JpegEncRet ret;
  JpegEncIn encIn;
  JpegEncOut encOut;
  JpegEncApiVersion encVer;
  JpegEncBuild encBuild;
  int encodeFail = 0;

  FILE *fout = NULL;
  i32 picBytes = 0;
  u32 i = 0;
  u32 mjpeg_length = 0, mjpeg_movi_idx = 0;
  IDX_CHUNK *idx = NULL;
  ma_s ma;
  u32 total_bits = 0;
  long numbersquareoferror = 0;
  i32 maxerrorovertarget = 0;
  i32 maxerrorundertarget = 0;
  float sumsquareoferror = 0;
  float averagesquareoferror = 0;
  struct timeval timeFrameStart;
  struct timeval timeFrameEnd;
  int filter_ret = 0;
  int SliceRoimapByteNum = 0;
  u32 SliceWidthMB = 0;
  u32 SliceHeightMB = 0;
  FILE *osdMapFile = NULL;
  EWLInitParam_t param;
  const void *ctx = NULL;

  commandLine_s cmdl;

  fprintf(stdout,
          "\n* * * * * * * * * * * * * * * * * * * * *\n\n"
          "      HANTRO JPEG ENCODER TESTBENCH\n"
          "\n* * * * * * * * * * * * * * * * * * * * *\n\n");

  memset(&tb, 0, sizeof(tb));

  /* Print API and build version numbers */
  encVer = JpegEncGetApiVersion();
  /* Version */
  fprintf(stdout, "VCX Encoder API v%u.%u.%u\n", encVer.major, encVer.minor,
          encVer.micro);

  if (argc < 2) {
    Help();
    exit(0);
  }

  Default_Parameter(&cmdl);

  /* Parse command line parameters */
  if (Parameter(argc, argv, &cmdl) != 0) {
    fprintf(stderr, "Input parameter error\n");
    retMain = -1;
    goto return_;
  }
  if (cmdl.lumWidthSrc == DEFAULT || cmdl.lumHeightSrc == DEFAULT) {
    cmdl.lumWidthSrc = 176;
    cmdl.lumHeightSrc = 144;
  }
  if (cmdl.lumWidthSrc > 65535 || cmdl.lumHeightSrc > 65535) {
    fprintf(stderr, "Input width/height error\n");
    retMain = -1;
    goto return_;
  }

  if (CmlLog(argv[0], &cmdl) == NOK) {
    fprintf(stderr, "Output CML LOG error !\n");
    ret = NOK;
    goto return_;
  }

  memset(&param, 0, sizeof(EWLInitParam_t));
  /* Init EWL instance for test bench to malloc/free linear buffer*/
  param.context = NULL;
  param.clientType = EWL_CLIENT_TYPE_MEM;  //buffer operation
  param.enc_dev = cmdl.encDevice;
  param.mem_dev = cmdl.memDevice;
  param.useVcmd = cmdl.useVcmd;
  ctx = EWLInit(&param);

  if (!ctx) {
    fprintf(stderr, "InitEwlInst: init ewl instance failed.\n");
    ASSERT(0);
    exit(-1);
  }

  tb.HwCfg = EncGetAsicConfig(EWL_CLIENT_TYPE_JPEG_ENC, ctx);
  if (!tb.HwCfg) {
    fprintf(stderr, "GetHwCfg: get hw config failed.\n");
    ASSERT(0);
    exit(-1);
  }

  encBuild = JpegEncGetBuild(0, ctx);
  EWLRelease(ctx);
  ctx = NULL;

  fprintf(stdout, "HW ID:  0x%08x\t SW Build: %u\n\n", encBuild.hwBuild,
          encBuild.swBuild);

  if (cmdl.roimapFile == NULL) {
    cfg.enableRoimap = 0;
    filter_ret = -1;
  } else {
    cfg.enableRoimap = 1;
    filter_ret = ReadFilter(&cmdl);
    if (filter_ret == -1) {
      retMain = -1;
      goto return_;
    }
  }

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    cfg.osdDec400TableFile[i] = fopen(cmdl.osdDec400CompTableInput[i], "rb");
  }

  cfg.secure_mode = cmdl.secure_mode;

  if ((cmdl.formatCustomizedType == 0 || cmdl.formatCustomizedType == 1) &&
      (cmdl.frameType == JPEGENC_YUV420_PLANAR)) {
    JPEGtransYUVtoTile32format(&cmdl);
  }

  if ((cmdl.formatCustomizedType == 2) &&
      (cmdl.frameType == JPEGENC_YUV420_PLANAR)) {
    JPEGtransTo4228bitformat(&cmdl);
  }

  if (((cmdl.formatCustomizedType == 3) || (cmdl.formatCustomizedType == 4)) &&
      (cmdl.frameType == JPEGENC_YUV420_PLANAR)) {
    JPEGtransToCommdataformat(&cmdl);
  }

  if ((cmdl.formatCustomizedType == 5) &&
      ((cmdl.frameType == JPEGENC_YUV420_SEMIPLANAR) ||
       (cmdl.frameType == JPEGENC_YUV420_SEMIPLANAR_VU) ||
       (cmdl.frameType == JPEGENC_YUV420_MS_P010))) {
    JPEGtransToTile4format(&cmdl);
  }

  u32 inputLineBufLoopBackEn =
      (cmdl.inputLineBufMode == 1 || cmdl.inputLineBufMode == 2) ? 1 : 0;
  u32 inputLineBufHwModeEn =
      (cmdl.inputLineBufMode == 2 || cmdl.inputLineBufMode == 4) ? 1 : 0;
  if (tb.HwCfg->hw_build_id == 0x70002178 && inputLineBufLoopBackEn &&
       !inputLineBufHwModeEn && cmdl.inputLineBufDepth != DEFAULT &&
       cmdl.inputLineBufDepth > 1) {
    cmdl.inputLineBufDepth = 1;
  }
  /* Encoder initialization */
  if ((ret = OpenEncoder(&cmdl, &encoder)) != 0) {
    retMain = -ret;
    goto return_; /* Return positive value for test scripts */
  }

  if (cmdl.osdMapBlockSize != 8 && cmdl.osdMapBlockSize != 4 && cmdl.osdMapEnable) {
    fprintf(stderr, "OSDMap don't support block size %u\n", cmdl.osdMapBlockSize);
    retMain = -1;
    goto return_;
  }

  u32 read_width = (((cmdl.rotation == 1 || cmdl.rotation == 2) ? cmdl.height : cmdl.width) +
                    ((cmdl.osdMapBlockSize == 4) ? 7 : 15)) >> ((cmdl.osdMapBlockSize == 4) ? 3 : 4);
  if (cmdl.osdMapEnable &&
    (cmdl.osdMapStride < read_width)) {
    fprintf(stderr, "OSDMap invalid stride\n");
    retMain = -1;
    goto return_;
  }

  /* Allocate input and output buffers */
  if (AllocRes(&tb, &cmdl, encoder) != 0) {
    fprintf(stderr, "Failed to allocate the external resources!\n");
    FreeRes(encoder);
    CloseEncoder(encoder);
    retMain = 1;
    goto return_;
  }

  /* Setup encoder input */
  memset(&encIn, 0, sizeof(JpegEncIn));
  for (i = 0; i < (cmdl.streamBufChain ? 2 : 1); i++) {
    encIn.pOutBuf[i] = (u8 *)outbufMem[i].virtualAddress;
    encIn.busOutBuf[i] = outbufMem[i].busAddress;
    encIn.outBufSize[i] = outbufMem[i].size;
    printf("encIn.pOutBuf[%u] %p\n", i, encIn.pOutBuf[i]);
  }
  encIn.frameHeader = 1;

  ma.pos = ma.count = 0;
  ma.frameRateNumer = cmdl.frameRateNum;
  ma.frameRateDenom = cmdl.frameRateDenom;

  ma.length = MOVING_AVERAGE_FRAMES;

  {
    i32 slice = 0, sliceRows = 0;
    i32 next = 0, last = 0, picCnt = 0;
    i32 widthSrc, heightSrc;
    char *input, *dec400TblInput;
    u64 lumaSize = 0, chromaSize = 0, pictureSize = 0, dec400LumaTblSize = 0,
        dec400ChrTblSize = 0;
    u32 input_alignment = 1 << cmdl.exp_of_input_alignment;
    i32 mcuh = (cmdl.codingMode == 1 ? 8 : 16);
    FILE *dec400Table = NULL;

#ifdef SIMULATE_SLICEINFO_UPDATE
    inputSliceInfo_s inputSliceInfo_data;

    inputSliceInfo_data.pic_height = cmdl.height;
    inputSliceInfo_data.timeouttest_en = 0;
#endif

    /* If no slice mode, the slice equals whole frame */
    if (cmdl.partialCoding == 0)
      sliceRows = cmdl.lumHeightSrc;
    else
      sliceRows = cmdl.restartInterval * mcuh;
    if (sliceRows > cmdl.lumHeightSrc) sliceRows = cmdl.lumHeightSrc;

    widthSrc = (cmdl.lumWidthSrc + 15) / 16 * 16;
    heightSrc = cmdl.lumHeightSrc;

    if (cmdl.frameType == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32) {
      u32 w, h;

      w = (cmdl.lumWidthSrc + 31) & (~31);
      h = (cmdl.lumHeightSrc + 31) & (~31);

      if (cmdl.partialCoding == 0)
        sliceRows = h;
      else
        sliceRows = cmdl.restartInterval * 16;
      if (sliceRows > h) sliceRows = h;

      widthSrc = w;
      heightSrc = h;
    } else if (cmdl.frameType ==
               JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4) {
      u32 h;
      h = (cmdl.lumHeightSrc + 15) & (~15);
      if (cmdl.partialCoding == 0)
        sliceRows = h;
      else
        sliceRows = cmdl.restartInterval * 16;

      if (sliceRows > h) sliceRows = h;
    }

    input = cmdl.input;

    dec400TblInput = cmdl.dec400CompTableinput;
    last = cmdl.lastPic;

    JpegGetLumaSize(encoder, &lumaSize, &dec400LumaTblSize);
    JpegGetChromaSize(encoder, &chromaSize, &dec400ChrTblSize);
    encIn.busLum = pictureMem.busAddress;
    if (cmdl.frameType == JPEGENC_Y8b ||
        cmdl.frameType == JPEGENC_Y10bWL ||
        cmdl.frameType == JPEGENC_Y10bWH) {
      encIn.busCb = encIn.busCr = encIn.busLum;
    } else {
      encIn.busCb = encIn.busLum + lumaSize;
      encIn.busCr = encIn.busCb + chromaSize / 2;
    }

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
    encIn.sliceinfoBus = sliceinfoMem.busAddress;
#endif

    if (roimapMem.virtualAddress != NULL)
      encIn.busRoiMap = roimapMem.busAddress;
    else
      encIn.busRoiMap = 0;
    if (filter_ret == -1)
      encIn.filter = NULL;
    else
      encIn.filter = cfg.filter;

    encIn.dec400TableBusLum = dec400CompTblMem.busAddress;
    encIn.dec400TableBusCb = encIn.dec400TableBusLum + dec400LumaTblSize;
    encIn.dec400TableBusCr = encIn.dec400TableBusCb + dec400ChrTblSize / 2;
    /* Virtual addresses of input picture, used by software encoder */
    encIn.pLum = (u8 *)pictureMem.virtualAddress;
    encIn.pCb = encIn.pLum + lumaSize;
    encIn.pCr = encIn.pCb + chromaSize / 2;

    encIn.dec400TablepLum = (u8 *)dec400CompTblMem.virtualAddress;
    encIn.dec400TablepCb = encIn.dec400TablepLum + dec400LumaTblSize;
    encIn.dec400TablepCr = encIn.dec400TablepCb + dec400ChrTblSize / 2;

    //for VCEJ test bench, disable axiFE by default
    encIn.axiFEEnable = 0;
#ifndef SUPPORT_AXIFE
    encIn.axiFEEnable = 0;
#else
#ifdef CMODEL_AXIFE
    encIn.axiFEEnable = cmdl.useAXIFE;
#else
    encIn.axiFEEnable = 1;
#endif
#endif

    fout = fopen(cmdl.output, "wb");
    if (fout == NULL) {
      fprintf(stderr, "Failed to create the output file.\n");
      FreeRes(encoder);
      CloseEncoder(encoder);
      retMain = -1;
      goto return_;
    }

    if (cmdl.inputLineBufMode) {
      if (InitInputLineBuffer(&inputMbLineBuf, &cfg, &encIn, encoder) < 0) {
        fprintf(stderr,
                "Fail to Init Input Line Buffer: virt_addr=%p, bus_addr=%08x\n",
                inputMbLineBuf.sram, (u32)(inputMbLineBuf.sramBusAddr));
        goto end;
      }
    }

    if (cmdl.streamMultiSegmentMode != 0) {
      if (InitStreamSegmentCrl(&streamSegCtl, &cmdl, fout, &encIn) < 0) {
        fprintf(stderr,
                "Failed to Init multi segment, please check parament.\n");
        goto end;
      }
    }

    /* Set Full Resolution mode */
    ret = JpegEncSetPictureSize(encoder, &cfg);
    /* Handle error situation */
    if (ret != JPEGENC_OK) {
#ifndef ASIC_WAVE_TRACE_TRIGGER
      printf("FAILED. Error code: %i\n", ret);
#endif
      goto end;
    }

    /* Set up overlay input file pointers and frame numbers */
    for (i = 0; i < MAX_OVERLAY_NUM; i++) {
      if ((cmdl.overlayEnables >> i) & 1) {
        u32 frameByte = 1;
        tb.olFile[i] = fopen(cmdl.olInput[i], "rb");
        if (!tb.olFile[i]) {
          fprintf(stderr, "Fail to read OSD %u input file\n", i);
          goto end;
        }
        //Get number of input overlay frames
        fseeko(tb.olFile[i], 0, SEEK_END);
        if (cmdl.olFormat[i] == 0) {
          if (cmdl.olSuperTile[i] == 0)
            frameByte = (cmdl.olWidth[i] * cmdl.olHeight[i]) * 4;
          else
            frameByte = (((cmdl.olWidth[i] + 63) / 64) * 64) *
                        (((cmdl.olHeight[i] + 63) / 64) * 64) * 4;
        } else if (cmdl.olFormat[i] == 1) {
          frameByte = (cmdl.olWidth[i] * cmdl.olHeight[i]) / 2 * 3;
        } else if (cmdl.olFormat[i] == 2) {
          frameByte = (cmdl.olWidth[i] / 8) * (cmdl.olHeight[i]);
        }
        cfg.olFrameNum[i] = ftello(tb.olFile[i]) / frameByte;
      } else {
        cfg.olFrameNum[i] = 0;
      }
    }

    /*if mjpeg is enabled, assemble mjpeg container header*/
    if (cmdl.mjpeg == 1) {
      mjpeg_length = MjpegEncodeAVIHeader(
          outbufMem[0].virtualAddress, cfg.codingWidth, cfg.codingHeight,
          cmdl.frameRateNum, cmdl.frameRateDenom,
          cmdl.lastPic - cmdl.firstPic + 1);
      if (writeOutput)
        WriteStrm(fout, outbufMem[0].virtualAddress, mjpeg_length, 0);

      mjpeg_movi_idx = mjpeg_length + 4;
      u32 *output_buf = outbufMem[0].virtualAddress;
      MjpegAVIchunkheader((u8 **)&output_buf, "LIST", "movi", 0);
      if (writeOutput) WriteStrm(fout, outbufMem[0].virtualAddress, 12, 0);

      mjpeg_length += 12;
      idx = malloc((cmdl.lastPic - cmdl.firstPic + 1) * sizeof(IDX_CHUNK));
      if (idx == NULL) goto return_;
    }
    /* Check OSDMap input exist*/
    u32 osdMapFrameNum = 0;
    if (cmdl.osdMapEnable) {
      osdMapFile = fopen(cmdl.osdMapInput, "rb");
      if (!osdMapFile) {
        fprintf(stderr, "Fail to read OSDMap input file\n");
        goto end;
      }
      u32 frameByte = 1;
      u32 read_height = GetOsdmapReadHeight(&cmdl);
      fseeko(osdMapFile, 0, SEEK_END);
      frameByte = cmdl.osdMapStride * read_height;
      osdMapFrameNum = ftello(osdMapFile) / frameByte;
      if (osdMapFrameNum == 0) {
        printf(
            "OsdMap file: Error, file %s size is not suitable for region "
            "size\n", cmdl.osdMapInput);
        goto end;
      }
    }

    if (cfg.enableRoimap) {
      if (cmdl.partialCoding) {
        if (cmdl.codingMode == JPEGENC_420_MODE) {
          SliceWidthMB = (cmdl.width + 15) / 16;
          SliceHeightMB = sliceRows / 16;
        } else if (cmdl.codingMode == JPEGENC_422_MODE) {
          SliceWidthMB = (cmdl.width + 15) / 16;
          SliceHeightMB = sliceRows / 8;
        } else if (cmdl.codingMode == JPEGENC_MONO_MODE) {
          SliceWidthMB = (cmdl.width + 7) / 8;
          SliceHeightMB = sliceRows / 8;
        }
        SliceRoimapByteNum = (SliceWidthMB * SliceHeightMB + 7) / 8;
      }
    }
    /* Main encoding loop */
    ret = JPEGENC_FRAME_READY;
    next = cmdl.firstPic;
    while (next <= last && (ret == JPEGENC_FRAME_READY ||
                            ret == JPEGENC_OUTPUT_BUFFER_OVERFLOW)) {
#ifdef SEPARATE_FRAME_OUTPUT
      char framefile[50];
      sprintf(framefile, "frame%d%s.jpg", picCnt, mode == 1 ? "tn" : "");
      remove(framefile);
#endif
#ifndef ASIC_WAVE_TRACE_TRIGGER
      printf("Frame %3d started...\n", picCnt);
#endif
      fflush(stdout);

      if (cmdl.mjpeg == 1) {
        u32 *output_buf = outbufMem[0].virtualAddress;

        MjpegAVIchunkheader((u8 **)&output_buf, "00dc", NULL, 0);
        if (writeOutput) WriteStrm(fout, outbufMem[0].virtualAddress, 8, 0);

        mjpeg_length += 8;
        idx[picCnt].offset = mjpeg_length - 8;
      }

      /* Set up overlay input buffer */
      SetupOverlayBuffer(&tb, &encIn, next, &cmdl, osdMapFrameNum);
      if (cfg.enableRoimap) {
        encIn.busRoiMap = roimapMem.busAddress;
      }

      const void *ewl_inst = JpegEncGetEwl(encoder);
      /* Loop until one frame is encoded */
      do {
#ifndef NO_INPUT_YUV
        /* Read next slice */
        if (ReadPic((u8 *)pictureMem.virtualAddress, cmdl.lumWidthSrc,
                    heightSrc, slice, sliceRows, next, input, cmdl.frameType,
                    input_alignment, cmdl.ufbcMode, cmdl.ufbcBlockType, cmdl.scanType) !=
            0)  //Mark for klocwork: cmdl.lumWidthSrc was protented by openEncoder()
          break;
        pictureSize = lumaSize + chromaSize;

        if (dec400Table == NULL) dec400Table = fopen(dec400TblInput, "rb");
        if (dec400Table == NULL && cmdl.dec400TSHeaderEnable == 0) {
          encIn.dec400Enable = 0;
        } else if (JpegReadDEC400Data(encoder, (u8 *)pictureMem.virtualAddress,
                                      (u8 *)dec400CompTblMem.virtualAddress,
                                      cmdl.frameType, cmdl.lumWidthSrc,
                                      cmdl.lumHeightSrc, input, dec400Table,
                                      next, slice, sliceRows, input_alignment,
									  cmdl.scanType, tb.dec400Enable,
									  cmdl.dec400TSHeaderEnable) == NOK) {
          break;
        } else
          encIn.dec400Enable = 2 | SetDec400Enable(&cmdl);
        encIn.dec400TSHeaderEnable = cmdl.dec400TSHeaderEnable;
        EWLSyncMemData(&pictureMem, 0, pictureSize, HOST_TO_DEVICE);
        if (encIn.dec400Enable == 2)
          EWLSyncMemData(&dec400CompTblMem, 0, cmdl.dec400FrameTableSize,
                         HOST_TO_DEVICE);
#ifdef SUPPORT_DEC400
  if (encIn.dec400TSHeaderEnable && ((encIn.dec400Enable & 0xF) == 2)) {
    u64 SrcLumaSize, srcChrSize, dec400LumaTblSize, dec400ChrTblSize;
    JpegGetLumaSize(encoder, &SrcLumaSize, &dec400LumaTblSize);
    JpegGetChromaSize(encoder, &srcChrSize, &dec400ChrTblSize);
    EncParseDEC400HeaderData(&encIn.clearColorLow[0], &encIn.fcEnable[0],
      (u8 *)dec400CompTblMem.virtualAddress);
    if (dec400ChrTblSize) {
      EncParseDEC400HeaderData(&encIn.clearColorLow[1], &encIn.fcEnable[1],
        (u8 *)dec400CompTblMem.virtualAddress + dec400LumaTblSize + SrcLumaSize);
        if (cmdl.frameType == JPEGENC_YUV420_PLANAR || cmdl.frameType == JPEGENC_YUV420_I010) {
          EncParseDEC400HeaderData(&encIn.clearColorLow[2], &encIn.fcEnable[2],
            (u8 *)dec400CompTblMem.virtualAddress + dec400LumaTblSize + dec400ChrTblSize / 2
            + SrcLumaSize + srcChrSize / 2 );
        }
    }
#if 0
#ifdef SYSTEM_BUILD
    Fastclear((u8 *)pictureMem.virtualAddress, (u8 *)dec400CompTblMem.virtualAddress, cmdl.frameType,
      encIn.clearColorLow, encIn.dec400Enable, SrcLumaSize,
      srcChrSize, dec400LumaTblSize, dec400ChrTblSize);
#endif
#endif
  }
#endif

#endif
        if (cmdl.inputLineBufMode)
          SetInputLineBuffer(&inputMbLineBuf, &cfg, &encIn, encoder, slice);

        //Get overlay slice info
        JpegEncGetOverlaySlice(encoder, &encIn, cmdl.restartInterval,
                               cmdl.partialCoding, slice, sliceRows, cfg.yOffset);
        //Get osdmap slice info
        JpegEncGetOSDMapSlice(encoder, &encIn, cmdl.restartInterval,
                               cmdl.partialCoding, slice, cfg.yOffset);

#ifdef SIMULATE_SLICEINFO_UPDATE
        pthread_t tid_sliceinfo;

        inputSliceInfo_data.tid = &tid_sliceinfo;
        inputSliceInfo_data.poll_timeout = 0;
        inputSliceInfo_data.line_cnt = 1;
        inputSliceInfo_data.frm_rdy = 0;
        inputSliceInfo_data.sliceinfo_vir_base = sliceinfoMem.virtualAddress;
        InitSliceInfo(encoder, &inputSliceInfo_data);
#endif

        gettimeofday(&timeFrameStart, NULL);
        encIn.picCnt = picCnt;
        ret = JpegEncEncode(encoder, &encIn, &encOut);
        if ((cmdl.partialCoding == 1 && cmdl.verOffsetSrc > 0) &&
            slice < (cmdl.verOffsetSrc / sliceRows)) {
          encIn.busRoiMap = encIn.busRoiMap; //cppcheck-suppress selfAssignment
        } else {
          encIn.busRoiMap += ((SliceRoimapByteNum + 15) & (~15));
        }
        switch (ret) {
          case JPEGENC_RESTART_INTERVAL:

#ifndef ASIC_WAVE_TRACE_TRIGGER
            printf("Frame %3d restart interval! %6u bytes\n", picCnt,
                   encOut.jfifSize);
            fflush(stdout);
#endif

            if (writeOutput) {
              if (cmdl.streamMultiSegmentMode == 0) {
                EWLSyncMemData(&outbufMem[0], encOut.headerSize,
                               (encOut.jfifSize - encOut.headerSize),
                               DEVICE_TO_HOST);
                writeStrmBufs(fout, outbufMem, 0, encOut.jfifSize,
                              encOut.invalidBytesInBuf0Tail, 0);
              } else {
                u8 *streamBase =
                    streamSegCtl.streamBase + (streamSegCtl.streamRDCounter %
                                               streamSegCtl.segmentAmount) *
                                                  streamSegCtl.segmentSize;
                WriteStrm(streamSegCtl.outStreamFile, (u32 *)streamBase,
                          encOut.jfifSize - streamSegCtl.streamRDCounter *
                                                streamSegCtl.segmentSize,
                          0);
                streamSegCtl.streamRDCounter = 0;
              }
            }

#ifdef SEPARATE_FRAME_OUTPUT
            if (writeOutput) {
              EWLSyncMemData(&outbufMem[0], encOut.headerSize,
                             (encOut.jfifSize - encOut.headerSize),
                             DEVICE_TO_HOST);
              writeFrameBufs(framefile, outbufMem, 0, encOut.jfifSize);
            }

#endif
            picBytes += encOut.jfifSize;
            slice++; /* Encode next slice */
            break;

          case JPEGENC_FRAME_READY:
#ifndef ASIC_WAVE_TRACE_TRIGGER
            printf("Frame %3d ready! %6u bytes\n", picCnt, encOut.jfifSize);
            fflush(stdout);
#endif
            if (cmdl.inputLineBufMode) {
              VCEncUpdateInitSegNum(&inputMbLineBuf);
            }

            gettimeofday(&timeFrameEnd, NULL);

            if (writeOutput) {
              if (cmdl.streamMultiSegmentMode == 0) {
                EWLSyncMemData(&outbufMem[0], encOut.headerSize,
                               (encOut.jfifSize - encOut.headerSize),
                               DEVICE_TO_HOST);
                writeStrmBufs(fout, outbufMem, 0, encOut.jfifSize,
                              encOut.invalidBytesInBuf0Tail, 0);
              } else {
                u8 *streamBase =
                    streamSegCtl.streamBase + (streamSegCtl.streamRDCounter %
                                               streamSegCtl.segmentAmount) *
                                                  streamSegCtl.segmentSize;
                WriteStrm(streamSegCtl.outStreamFile, (u32 *)streamBase,
                          encOut.jfifSize - streamSegCtl.streamRDCounter *
                                                streamSegCtl.segmentSize,
                          0);
                streamSegCtl.streamRDCounter = 0;
              }
            }
#ifdef SEPARATE_FRAME_OUTPUT
            if (writeOutput) {
              EWLSyncMemData(&outbufMem[0], encOut.headerSize,
                             (encOut.jfifSize - encOut.headerSize),
                             DEVICE_TO_HOST);
              writeFrameBufs(framefile, outbufMem, 0, encOut.jfifSize);
            }

#endif
            if (cmdl.mjpeg == 1) {
              picBytes += encOut.jfifSize;
              mjpeg_length += picBytes;
              if (mjpeg_length % 4 != 0) {
                memset(outbufMem[0].virtualAddress, 0, 4 - (mjpeg_length % 4));
                if (writeOutput)
                  WriteStrm(fout, outbufMem[0].virtualAddress,
                            4 - (mjpeg_length % 4), 0);

                mjpeg_length = (mjpeg_length + 3) & (~3);
                picBytes = (picBytes + 3) & (~3);
              }
              idx[picCnt].length = picBytes;
              fseek(fout, -(picBytes + 4), SEEK_CUR);
              fwrite(&picBytes, sizeof(i32), 1, fout);
              fseek(fout, 0, SEEK_END);

              /*rate control statistic log*/
              total_bits += picBytes * 8;
              MaAddFrame(&ma, picBytes * 8);

              printf(
                  "=== Encoded %i Qp=%d bits=%d TotalBits=%u "
                  "averagebitrate=%llu HWCycles=%u Time(us %u HW +SW) \n",
                  picCnt, JpegGetQpHdr(encoder), picBytes * 8, total_bits,
                  ((unsigned long long)total_bits * cmdl.frameRateNum) /
                      ((picCnt + 1) * cmdl.frameRateDenom),
                  JpegEncGetPerformance(encoder),
                  uTimeDiff(timeFrameEnd, timeFrameStart));

              if (cmdl.bitPerSecond != 0 && (picCnt + 1) >= ma.length) {
                numbersquareoferror++;
                if (maxerrorovertarget < (Ma(&ma) - cmdl.bitPerSecond))
                  maxerrorovertarget = (Ma(&ma) - cmdl.bitPerSecond);
                if (maxerrorundertarget < (cmdl.bitPerSecond - Ma(&ma)))
                  maxerrorundertarget = (cmdl.bitPerSecond - Ma(&ma));
                sumsquareoferror +=
                    ((float)(ABS(Ma(&ma) - (i32)cmdl.bitPerSecond)) * 100 /
                     cmdl.bitPerSecond);
                averagesquareoferror = (sumsquareoferror / numbersquareoferror);
                printf(
                    "++++RateControl(movingBitrate=%d MaxOvertarget=%u%% "
                    "MaxUndertarget=%u%% AveDeviationPerframe=%f%%) \n",
                    Ma(&ma), maxerrorovertarget * 100 / cmdl.bitPerSecond,
                    maxerrorundertarget * 100 / cmdl.bitPerSecond,
                    averagesquareoferror);
              }
            } else
              printf("=== Encoded %i bits=%u HWCycles=%u Time(us %u HW +SW) \n",
                     picCnt, encOut.jfifSize, JpegEncGetPerformance(encoder),
                     uTimeDiff(timeFrameEnd, timeFrameStart));
            picBytes = 0;
            slice = 0;
#ifdef SIMULATE_SLICEINFO_UPDATE
            pthread_mutex_lock(&inputSliceInfo_data.frmrdy_mutex);
            inputSliceInfo_data.frm_rdy = JPEGENC_FRAME_READY;
            pthread_mutex_unlock(&inputSliceInfo_data.frmrdy_mutex);
#endif

            break;

          case JPEGENC_OUTPUT_BUFFER_OVERFLOW:

#ifndef ASIC_WAVE_TRACE_TRIGGER
            printf("Error: Frame %3d lost! Output buffer overflow.\n", picCnt);
#endif

            /* For debugging
                    if(writeOutput)
                        WriteStrm(fout, outbufMem.virtualAddress,
                                outbufMem.size, 0);*/
            /* Rewind the file back this picture's bytes
                    fseek(fout, -picBytes, SEEK_CUR);*/
            picBytes = 0;
            slice = 0;
            break;

          default:

#ifdef SIMULATE_SLICEINFO_UPDATE
          if (ret == JPEGENC_HW_SLICEINFO_TIMEOUT)
            inputSliceInfo_data.poll_timeout = POLL_TIMEOUT;
#endif

#ifndef ASIC_WAVE_TRACE_TRIGGER
            printf("FAILED. Error code: %i\n", ret);
#endif
            encodeFail = (int)ret;
            /* For debugging */
            if (writeOutput) {
              EWLSyncMemData(&outbufMem[0], encOut.headerSize,
                             (encOut.jfifSize - encOut.headerSize),
                             DEVICE_TO_HOST);
              writeStrmBufs(fout, outbufMem, 0, encOut.jfifSize,
                            encOut.invalidBytesInBuf0Tail, 0);
            }

            break;
        }
#ifdef SIMULATE_SLICEINFO_UPDATE
        pthread_join(tid_sliceinfo, NULL);
#endif
      } while (ret == JPEGENC_RESTART_INTERVAL);

      picCnt++;
      next = picCnt + cmdl.firstPic;
    } /* End of main encoding loop */

    if (dec400Table != NULL) fclose(dec400Table);

  } /* End of encoding modes */
end:

#ifndef ASIC_WAVE_TRACE_TRIGGER
  printf("Release encoder\n");
#endif
  if (cmdl.mjpeg == 1) {
    u32 idx_length;
    picBytes = mjpeg_length - mjpeg_movi_idx - 4;
    fseek(fout, mjpeg_movi_idx, SEEK_SET);
    fwrite(&picBytes, sizeof(i32), 1, fout);

    idx_length =
        MjpegEncodeAVIidx(outbufMem[0].virtualAddress, idx, mjpeg_movi_idx + 4,
                          (cmdl.lastPic - cmdl.firstPic + 1));
    mjpeg_length += idx_length;

    fseek(fout, 0, SEEK_END);
    if (writeOutput)
      WriteStrm(fout, outbufMem[0].virtualAddress, idx_length, 0);

    picBytes = mjpeg_length - 8;
    fseek(fout, 4, SEEK_SET);
    fwrite(&picBytes, sizeof(i32), 1, fout);
    if (idx != NULL) free(idx);
  }

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (tb.olFile[i]) fclose(tb.olFile[i]);
    if (cfg.osdDec400TableFile[i]) fclose(cfg.osdDec400TableFile[i]);
  }

  if (osdMapFile) fclose(osdMapFile);

  /* Free all resources */
  FreeRes(encoder);
  CloseEncoder(encoder);

  retMain = encodeFail;
return_:
  if (fout != NULL) fclose(fout);

#ifdef __FREERTOS__
#ifdef FREERTOS_SIMULATOR
  vTaskEndScheduler();  // need to open in simulator, and need to close on real env
#else
  Platform_deinit();
#endif
  (void)(retMain);

  return (void *)NULL;
#else
  return retMain;
#endif
}

/*------------------------------------------------------------------------------

    AllocRes

    Allocation of the physical memories used by both SW and HW:
    the input picture and the output stream buffer.

    NOTE! The implementation uses the EWL instance from the encoder
          for OS independence. This is not recommended in final environment
          because the encoder will release the EWL instance in case of error.
          Instead, the memories should be allocated from the OS the same way
          as inside EWLMallocLinear().

------------------------------------------------------------------------------*/
int AllocRes(TestBench_s *tb, commandLine_s *cmdl, JpegEncInst enc) {
  i32 sliceRows = 0;
  u64 pictureSize;
  u32 streamBufTotalSize;
  u32 headerSize = JPEGENC_STREAM_MIN_BUF0_SIZE;
  i32 i, ret;
  u64 lumaSize, chromaSize;
  u32 dec400LumaTblSize = 0, dec400ChrTblSize = 0;
  u32 input_alignment =
      (cmdl->exp_of_input_alignment == 0 ? 0
                                         : (1 << cmdl->exp_of_input_alignment));
  i32 mcuh = (cmdl->codingMode == 1 ? 8 : 16);
  i32 strmBufNum = cmdl->streamBufChain ? 2 : 1;
  i32 bufSizes[2] = {0, 0};
  i32 dec400VersionId = 0;
  FILE *dec400Table = fopen(cmdl->dec400CompTableinput, "rb");
  const void *ewl_inst = JpegEncGetEwl(enc);
  u32 ufbcHeaderSize = 0;
  if (ewl_inst == NULL) {
    fprintf(stderr, "Failed to get ewl instance!\n");
    goto error;
  }

  /* Set slice size and output buffer size
     * For output buffer size, 1 byte/pixel is enough for most images.
     * Some extra is needed for testing purposes (noise input) */

  if (cmdl->partialCoding == 0) {
    if (cmdl->frameType == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32)
      sliceRows = ((cmdl->lumHeightSrc + 32 - 1) & (~(32 - 1)));
    else if (cmdl->frameType == JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4)
      sliceRows = ((cmdl->lumHeightSrc + mcuh - 1) & (~(mcuh - 1)));
    else
      sliceRows = cmdl->lumHeightSrc;
  } else {
    sliceRows = cmdl->restartInterval * mcuh;
  }

  streamBufTotalSize = cmdl->width * sliceRows * 2;
  if (cmdl->thumbnail) headerSize += thumbDataLength;

  getAlignedPicSizebyFormat(cmdl->frameType, cmdl->lumWidthSrc, sliceRows,
                            input_alignment, &lumaSize, &chromaSize,
                            &pictureSize, cmdl->scanType);
  if (pictureSize == 0) {
    goto error;
  }
#ifdef SUPPORT_UFBC
#ifndef SYSTEM_BUILD
  u32 asic_format = 0;
  if (cmdl->ufbcMode)
    EncUfbcGetSize(cmdl->frameType, cmdl->ufbcBlockType, cmdl->lumWidthSrc, cmdl->lumHeightSrc,
      cmdl->ufbcMode, &asic_format, &ufbcHeaderSize, &pictureSize);
#endif
#endif
  if (dec400Table != NULL || cmdl->dec400TSHeaderEnable) {
    tb->dec400Enable = 2 | SetDec400Enable(cmdl);
    EncGetDec400TsBufferSize(
        cmdl->frameType, cmdl->lumWidthSrc, sliceRows, input_alignment,
        &dec400LumaTblSize, &dec400ChrTblSize, &cmdl->dec400FrameTableSize,
        cmdl->scanType, tb->dec400Enable, cmdl->dec400TSHeaderEnable);
    if (cmdl->dec400FrameTableSize == 0)
    {
        goto error;
    }
  }
  JpegSetLumaSize(
      enc, lumaSize,
      dec400LumaTblSize);  //((jpegInstance_s *)enc)->lumaSize = lumaSize;
  JpegSetChromaSize(
      enc, chromaSize,
      dec400ChrTblSize);  //((jpegInstance_s *)enc)->chromaSize = chromaSize;

  memset(&pictureMem, 0, sizeof(EWLLinearMem_t));
  pictureMem.virtualAddress = NULL;
  /* Here we use the EWL instance directly from the encoder
     * because it is the easiest way to allocate the linear memories */
  pictureMem.mem_type = EXT_WR | VPU_RD | EWL_MEM_TYPE_DPB;
  SET_MEM_USAGE(pictureMem.mem_type, EWL_MEM_USAGE_IN_SURFACE,
                cmdl->secure_mode);
  ret = EWLMallocLinear(ewl_inst, pictureSize + ufbcHeaderSize, 0, &pictureMem);
  if (ret != EWL_OK) {
    fprintf(stderr, "Failed to allocate input picture!\n");
    pictureMem.virtualAddress = NULL;
    goto error;
  }

  dec400CompTblMem.virtualAddress = NULL;
  if (dec400Table != NULL || cmdl->dec400TSHeaderEnable) {
    if (dec400LumaTblSize + dec400ChrTblSize > 0) {
      dec400CompTblMem.mem_type = EXT_WR | VPU_RD | EWL_MEM_TYPE_DPB;
      SET_MEM_USAGE(dec400CompTblMem.mem_type, EWL_MEM_USAGE_IN_SURFACE_DECTS,
                    cmdl->secure_mode);
      ret = EWLMallocLinear(ewl_inst, dec400LumaTblSize + dec400ChrTblSize, 16,
                            &dec400CompTblMem);
      if (ret != EWL_OK) {
        fprintf(stderr, "Failed to allocate dec400 compress table!\n");
        dec400CompTblMem.virtualAddress = NULL;
        goto error;
      }
    }
  }

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  sliceinfoMem.mem_type =
                    EXT_WR | VPU_RD | EWL_MEM_TYPE_VPU_WORKING;
   SET_MEM_USAGE(sliceinfoMem.mem_type,
	              EWL_MEM_USAGE_TMP_SLICEINFO, cmdl->secure_mode);
  ret = EWLMallocLinear(ewl_inst, SLICEINFO_SIZE, 16, &sliceinfoMem);
  if (ret != EWL_OK) {
      printf("ERROR: Allocate sliceinfo memory failed!\n");
      sliceinfoMem.virtualAddress = NULL;
      goto error;
  }
#endif

  if (strmBufNum == 1) {
    if (cmdl->streamMultiSegmentMode != 0) {
      if (cmdl->streamMultiSegmentMode == 3) {
        if (cmdl->streamMultiSegmentAmount == 0) {
          // bufSize should be integer multiple of streamMultiSegmentSize, larger than
          // streamBufTotalSize
          u32 tmp_segmentAmount =
              (streamBufTotalSize < cmdl->streamMultiSegmentSize)
                  ? 2
                  : streamBufTotalSize / cmdl->streamMultiSegmentSize + 1;
          bufSizes[0] = tmp_segmentAmount * cmdl->streamMultiSegmentSize;
        } else {
          bufSizes[0] =
              cmdl->streamMultiSegmentSize * cmdl->streamMultiSegmentAmount;
        }
      } else {  // mode 1,2
        bufSizes[0] =
            (cmdl->streamMultiSegmentAmount == 0)
                ? streamBufTotalSize / 128
                : cmdl->streamMultiSegmentSize * cmdl->streamMultiSegmentAmount;
      }
    } else {
      bufSizes[0] = streamBufTotalSize;
    }
  } else {
    /* set small stream buffer0 to test two stream buffers */
    bufSizes[0] = streamBufTotalSize / 100;
    bufSizes[1] = streamBufTotalSize - bufSizes[0];
  }
  if (cmdl->streamMultiSegmentMode == 0) {
    bufSizes[0] += headerSize;
  }
  memset(outbufMem, 0, sizeof(outbufMem));
  for (i = 0; i < strmBufNum; i++) {
    u32 size = bufSizes[i];

    /* For FPGA testing, smaller size maybe specified. */
    /* Max output buffer size is less than 256MB */
    //comment out outbufSize hard limitation for 16K*16K testing
    //size = size < (1024*1024*64) ? size : (1024*1024*64);

    outbufMem[i].mem_type = VPU_WR | CPU_WR | CPU_RD | EWL_MEM_TYPE_SLICE;
    SET_MEM_USAGE(outbufMem[i].mem_type, EWL_MEM_USAGE_OUT_STRM,
                  cmdl->secure_mode);
    ret = EWLMallocLinear(ewl_inst, size, 0, &outbufMem[i]);
    if (ret != EWL_OK) {
      fprintf(stderr, "Failed to allocate output buffer!\n");
      outbufMem[i].virtualAddress = NULL;
      goto error;
    }
  }
  //RoiMap assume cmdl->width/height % 16 == 0
  if (cfg.enableRoimap) {
    ret = ReadRoimap(cmdl, ewl_inst);
    if (ret == -1) {
      roimapMem.virtualAddress = NULL;
      goto error;
    }
  }

  /*Overlay input buffer*/
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    u32 block_size = 0;
    u32 dec400TableSize[3] = {0};
    u32 dec400TotalTableSize = 0;
    if ((cmdl->overlayEnables >> i) & 1) {
      switch (cmdl->olFormat[i]) {
        case 0:  //ARGB
          if (cmdl->olSuperTile[i] == 0)
            block_size = cmdl->olYStride[i] * cmdl->olHeight[i];
          else
            block_size = cmdl->olYStride[i] * ((cmdl->olHeight[i] + 63) / 64);
          break;
        case 1:  //NV12
          block_size = cmdl->olYStride[i] * cmdl->olHeight[i] +
                       cmdl->olUVStride[i] * cmdl->olHeight[i] / 2;
          break;
        case 2:  //Bitmap
          block_size = cmdl->olYStride[i] * cmdl->olHeight[i];
          break;
        default:  //3
          block_size = 0;
      }
      overlayMem[i].mem_type = EXT_WR | VPU_RD | EWL_MEM_TYPE_VPU_WORKING;
      SET_MEM_USAGE(overlayMem[i].mem_type, EWL_MEM_USAGE_IN_OVERLAY,
                    cmdl->secure_mode);
      if (EWLMallocLinear(ewl_inst, block_size, 0, &overlayMem[i]) != EWL_OK) {
        overlayMem[i].virtualAddress = NULL;
        goto error;
      }
      memset(overlayMem[i].virtualAddress, 0, block_size);

      /* Dec400 buffer */
      tb->osdDec400TableFile[i] = fopen(cmdl->osdDec400CompTableInput[i], "rb");
      if (tb->osdDec400TableFile[i])
        GetOsdDec400Size(ewl_inst, cmdl, i, input_alignment, dec400TableSize, &dec400TotalTableSize);

      if (dec400TotalTableSize) {
        SET_MEM_USAGE(osdDec400CompTblMem[i].mem_type, EWL_MEM_USAGE_IN_OVERLAY_DECTS,
                      cmdl->secure_mode);
        if (EWLMallocLinear(ewl_inst, dec400TotalTableSize, 0,
                            &osdDec400CompTblMem[i]) != EWL_OK) {
          osdDec400CompTblMem[i].virtualAddress = NULL;
          goto error;
        }
        memset(osdDec400CompTblMem[i].virtualAddress, 0, dec400TotalTableSize);
        for (u32 j = 0; j < 3; j++) {
          cfg.osdDec400TableSize[i][j] = dec400TableSize[j];
        }
      }
    } else {
      overlayMem[i].virtualAddress = NULL;
    }
  }
  //OSD map buffer
    if (cmdl->osdMapEnable) {
      u32 read_height = GetOsdmapReadHeight(cmdl);
      u32 block_size = cmdl->osdMapStride * read_height;
      osdMapMem.mem_type =
        EXT_WR | VPU_RD | EWL_MEM_TYPE_VPU_WORKING;
      SET_MEM_USAGE(osdMapMem.mem_type,
                    EWL_MEM_USAGE_IN_OSDMAP, cmdl->secure_mode);
      if (EWLMallocLinear(ewl_inst, block_size, 0, &osdMapMem) != EWL_OK) {
        osdMapMem.virtualAddress = NULL;
        goto error;
      }
      memset(osdMapMem.virtualAddress, 0, block_size);
    }

#ifndef ASIC_WAVE_TRACE_TRIGGER
  printf("Input %dx%d + %dx%d encoding at %dx%d + %dx%d ",
         cmdl->lumWidthSrcThumb, cmdl->lumHeightSrcThumb, cmdl->lumWidthSrc,
         cmdl->lumHeightSrc, cmdl->widthThumb, cmdl->heightThumb, cmdl->width,
         cmdl->height);

  if (cmdl->partialCoding != 0)
    printf("in slices of %dx%d", cmdl->width, sliceRows);
  printf("\n");
#endif

#ifndef ASIC_WAVE_TRACE_TRIGGER
  printf("Input buffer size:          %u bytes\n", pictureMem.size);
  printf("Input buffer bus address:   %p\n", (void *)pictureMem.busAddress);
  printf("Input buffer user address:  %10p\n", pictureMem.virtualAddress);
  for (i = 0; i < strmBufNum; i++) {
    printf("Output buffer%d size:         %u bytes\n", i, outbufMem[i].size);
    printf("Output buffer%d bus address:  %p\n", i,
           (void *)outbufMem[i].busAddress);
    printf("Output buffer%d user address: %10p\n", i,
           outbufMem[i].virtualAddress);
  }
#endif
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (tb->osdDec400TableFile[i] != NULL) fclose(tb->osdDec400TableFile[i]);
  }
  if (dec400Table != NULL) fclose(dec400Table);
  return 0;

error:
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (tb->osdDec400TableFile[i] != NULL) fclose(tb->osdDec400TableFile[i]);
  }
  if (dec400Table != NULL) fclose(dec400Table);
  return 1;
}

/*------------------------------------------------------------------------------

    FreeRes

------------------------------------------------------------------------------*/
void FreeRes(JpegEncInst enc) {
  i32 i;
  const void *ewl_inst = JpegEncGetEwl(enc);
  if (ewl_inst == NULL) {
    return;
  }

  EWLFreeLinear(ewl_inst, &pictureMem);
  for (i = 0; i < MAX_STRM_BUF_NUM; i++) {
    EWLFreeLinear(ewl_inst, &outbufMem[i]);
  }
  EWLFreeLinear(ewl_inst, &dec400CompTblMem);
  if (thumbData != NULL) free(thumbData);
#ifndef TB_DEFINED_COMMENT
  if (cfg.pCom != NULL) free((void *)cfg.pCom);
#endif

  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    EWLFreeLinear(ewl_inst, &overlayMem[i]);
    EWLFreeLinear(ewl_inst, &osdDec400CompTblMem[i]);
  }
  EWLFreeLinear(ewl_inst, &roimapMem);
  EWLFreeLinear(ewl_inst, &osdMapMem);
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  EWLFreeLinear(ewl_inst, &sliceinfoMem);
#endif
}

int ReadQTable(char *qTableFileName, u8 *qTableLuma, u8 *qTableChroma) {
  FILE *fp = NULL;
  u8 *qTable;
  int line[8];
  int ret = -1;
  int i, j, num;

  fp = fopen(qTableFileName, "rt");
  if (fp == NULL) return -1;

  qTable = qTableLuma;
  for (i = 0; i < 16; i++) {
    num = fscanf(fp, "%d %d %d %d %d %d %d %d\n", line, line + 1, line + 2,
                 line + 3, line + 4, line + 5, line + 6, line + 7);
    if (num != 8) {
      fclose(fp);
      return -1;
    }
    for (j = 0; j < 8; j++) {
      if (line[j] > 255) {
        fprintf(stderr, "Invalid Quant Table value %d at %d.\n", line[j],
                i * 8 + j);
        fclose(fp);
        return -1;
      }
      qTable[j] = (u8)line[j];
    }

    if (i == 7)
      qTable = qTableChroma;
    else
      qTable += 8;
  }

  fclose(fp);
  return 0;
}

int ReadFilter(commandLine_s *cmdl) {
  FILE *fpFilter = NULL;
  char buf[32];
  int val[8];
  int ret = -1;
  int num = 0;

  if (cmdl->nonRoiFilter != NULL) {
    fpFilter = fopen(cmdl->nonRoiFilter, "r");
    if (fpFilter == NULL) {
      printf("Error: Can Not Open nonRoiFilter File %s\n", cmdl->nonRoiFilter);
      return -1;
    }
    while (fgets(buf, 32, fpFilter) != NULL) {
      if (buf[0] == '#' || buf[0] == '\r' || buf[0] == '\n') {
        continue;
      }
      sscanf(buf, "%d %d %d %d %d %d %d %d", &val[0], &val[1], &val[2], &val[3],
             &val[4], &val[5], &val[6], &val[7]);
      for (int i = 0; i < 8; i++) {
        cfg.filter[num] = (u8)val[i];
        num++;
      }
    }
    fclose(fpFilter);
  } else {
    if (cmdl->nonRoiLevel >= 0 && cmdl->nonRoiLevel <= 9) {
      for (int j = 0; j < 64; j++) {
        cfg.filter[j] = NonRoiFilterLuminance[cmdl->nonRoiLevel][j];
        cfg.filter[j + 64] = NonRoiFilterChrominance[cmdl->nonRoiLevel][j];
      }
    } else {
      printf("Error: invalid nonRoiLevel value (%d).\n", cmdl->nonRoiLevel);
      return -1;
    }
  }
  return 0;
}

int ReadRoimap(commandLine_s *cmdl, const void *ewl_inst) {
  FILE *fpROI;
  char buf[30];
  i32 RoiRectNum = 0;
  u32 RoiRectLeft[30];
  u32 RoiRectTop[30];
  u32 RoiRectWidth[30];
  u32 RoiRectHeight[30];
  u32 RoiRegionCal = 0;
  u32 RoiRegionCalWidth = 0;
  u32 RoiSize = 0;
  u32 ImageWidthMB = 0;
  u32 ImageHeightMB = 0;
  u8 NumsBit = 0;
  u8 BitSet = 0;
  u32 byte_start[30];
  u32 byte_offest[30];
  u8 *roimap_virtualAddr = NULL;
  u8 RoiRegion_SetVal[8] = {0};
  u32 bitNum = 0;
  u8 roiValTmp = 0;
  u32 sliceRows = 0;
  u32 sliceRowsMb = 0;
  u32 mcuh = 0;
  u32 RoiSizePerSlice = 0;
  u32 MbNumPerSlice = 0;
  static const u8 set_val[9] = {0x00, 0x01, 0x03, 0x07, 0x0F,
                                0x1F, 0x3F, 0x7F, 0xFF};
  fpROI = fopen(cmdl->roimapFile, "r");
  if (fpROI == NULL) {
    printf("Error: Can Not Open ROI Map File %s\n", cmdl->roimapFile);
    return -1;
  }
  while (fgets(buf, 30, fpROI) != NULL) {
    if (buf[0] == 'r') {
      sscanf(buf, "roi=(%u,%u,%u,%u)", &RoiRectLeft[RoiRectNum],
             &RoiRectTop[RoiRectNum], &RoiRectWidth[RoiRectNum],
             &RoiRectHeight[RoiRectNum]);
      if (RoiRectLeft[RoiRectNum] + RoiRectWidth[RoiRectNum] > cmdl->width ||
          RoiRectTop[RoiRectNum] + RoiRectHeight[RoiRectNum] > cmdl->height) {
        printf(
            "jpeg_map.roi: Error, The Roi Region Coordinate Input Is Out Of "
            "Picture Range!\n");
        fclose(fpROI);
        return -1;
      }
      if (cmdl->codingMode == JPEGENC_420_MODE) {
        RoiRectLeft[RoiRectNum] = RoiRectLeft[RoiRectNum] / 16;
        RoiRectTop[RoiRectNum] = RoiRectTop[RoiRectNum] / 16;
        RoiRectWidth[RoiRectNum] = (RoiRectWidth[RoiRectNum] + 15) / 16;
        RoiRectHeight[RoiRectNum] = (RoiRectHeight[RoiRectNum] + 15) / 16;
      } else if (cmdl->codingMode == JPEGENC_422_MODE) {
        RoiRectLeft[RoiRectNum] = RoiRectLeft[RoiRectNum] / 16;
        RoiRectTop[RoiRectNum] = RoiRectTop[RoiRectNum] / 8;
        RoiRectWidth[RoiRectNum] = (RoiRectWidth[RoiRectNum] + 15) / 16;
        RoiRectHeight[RoiRectNum] = (RoiRectHeight[RoiRectNum] + 7) / 8;
      } else if (cmdl->codingMode == JPEGENC_MONO_MODE) {
        RoiRectLeft[RoiRectNum] = RoiRectLeft[RoiRectNum] / 8;
        RoiRectTop[RoiRectNum] = RoiRectTop[RoiRectNum] / 8;
        RoiRectWidth[RoiRectNum] = (RoiRectWidth[RoiRectNum] + 7) / 8;
        RoiRectHeight[RoiRectNum] = (RoiRectHeight[RoiRectNum] + 7) / 8;
      }
      RoiRectNum++;
    }
  }
  fclose(fpROI);

  if (cmdl->codingMode == JPEGENC_420_MODE) {
    ImageWidthMB = (cmdl->width + 15) / 16;
    ImageHeightMB = (cmdl->height + 15) / 16;
  } else if (cmdl->codingMode == JPEGENC_422_MODE) {
    ImageWidthMB = (cmdl->width + 15) / 16;
    ImageHeightMB = (cmdl->height + 7) / 8;
  } else if (cmdl->codingMode == JPEGENC_MONO_MODE) {
    ImageWidthMB = (cmdl->width + 7) / 8;
    ImageHeightMB = (cmdl->height + 7) / 8;
  }
  if (cmdl->partialCoding == 0) {
    RoiSize = (ImageWidthMB * ImageHeightMB + 7) / 8;
    sliceRowsMb = ImageHeightMB;
  } else {
    if (cmdl->codingMode == JPEGENC_420_MODE) {
      sliceRowsMb = cmdl->restartInterval;
    } else {
      sliceRowsMb = cmdl->restartInterval * 2;
    }
    RoiSizePerSlice = ((sliceRowsMb * ImageWidthMB + 7) / 8 + 15) & (~15);
    RoiSize =
        RoiSizePerSlice * ((ImageHeightMB + sliceRowsMb - 1) / sliceRowsMb);
  }
  roimapMem.mem_type = EXT_WR | VPU_RD | EWL_MEM_TYPE_VPU_WORKING;
  SET_MEM_USAGE(roimapMem.mem_type, EWL_MEM_USAGE_IN_QPMAP, cmdl->secure_mode);
  if (EWLMallocLinear(ewl_inst, RoiSize, 16, &roimapMem) != EWL_OK) {
    fprintf(stderr, "Failed to allocate RoiMap Memory!\n");
    roimapMem.virtualAddress = NULL;
    return 1;
  }
  memset(roimapMem.virtualAddress, 0, RoiSize);
  // ROI bitmap need 1
  roimap_virtualAddr = (u8 *)roimapMem.virtualAddress;

  for (int roinum = 0; roinum < RoiRectNum; roinum++) {
    roimap_virtualAddr = (u8 *)roimapMem.virtualAddress;
    for (int rows = 0; rows < ImageHeightMB; rows++) {
      for (int cals = 0; cals < ImageWidthMB; cals++) {
        if ((rows >= RoiRectTop[roinum] &&
             rows < (RoiRectTop[roinum] + RoiRectHeight[roinum])) &&
            cals >= RoiRectLeft[roinum] &&
            cals < (RoiRectLeft[roinum] + RoiRectWidth[roinum]))  //ROI
        {
          RoiRegion_SetVal[bitNum] = 1;
        } else {
          RoiRegion_SetVal[bitNum] = 0;
        }
        bitNum++;
        if (bitNum == 8 ||
            (rows == ImageHeightMB - 1 && cals == ImageWidthMB - 1) ||
            (MbNumPerSlice == sliceRowsMb * ImageWidthMB - 1)) {
          roiValTmp = RoiRegion_SetVal[0] * 128 + RoiRegion_SetVal[1] * 64 +
                      RoiRegion_SetVal[2] * 32 + RoiRegion_SetVal[3] * 16 +
                      RoiRegion_SetVal[4] * 8 + RoiRegion_SetVal[5] * 4 +
                      RoiRegion_SetVal[6] * 2 + RoiRegion_SetVal[7] * 1;
          *roimap_virtualAddr = *roimap_virtualAddr | roiValTmp;
          roimap_virtualAddr++;
          if ((MbNumPerSlice == sliceRowsMb * ImageWidthMB - 1) &&
              cmdl->partialCoding == 1) {
            roimap_virtualAddr =
                (u8 *)(((ptr_t)roimap_virtualAddr + 15) & (~15));
          }
          roiValTmp = 0;
          bitNum = 0;
          memset(RoiRegion_SetVal, 0, sizeof(RoiRegion_SetVal));
        }
        MbNumPerSlice++;
        if (MbNumPerSlice == sliceRowsMb * ImageWidthMB ||
            (rows == ImageHeightMB - 1 && cals == ImageWidthMB - 1)) {
          MbNumPerSlice = 0;
        }
      }
    }
  }
  EWLSyncMemData(&roimapMem, 0, RoiSize, HOST_TO_DEVICE);
  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : osd_overlap
    Description     : check osd input overlap
    Return type     : i32
    Argument        : cml
    Argument        : i - osd channel to check
------------------------------------------------------------------------------*/
i32 jpeg_osd_overlap(commandLine_s *cml, u8 id) {
  int i, tmpx, tmpy;
  int blockW = 64;
  int blockH = 16;
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if (!((cml->overlayEnables >> i) & 1) || i == id) continue;
    u32 xoffsetId = cml->olXoffset[id];
    u32 yoffsetId = cml->olYoffset[id];
    u32 cropWidthId = cml->olCropWidth[id];
    u32 cropHeightId = cml->olCropHeight[id];

    u32 xoffset = cml->olXoffset[i];
    u32 yoffset = cml->olYoffset[i];
    u32 cropWidth = cml->olCropWidth[i];
    u32 cropHeight = cml->olCropHeight[i];

    if (!((xoffsetId + cropWidthId) <= xoffset ||
          (yoffsetId + cropHeightId) <= yoffset ||
          xoffsetId >= (xoffset + cropWidth) ||
          yoffsetId >= (yoffset + cropHeight))) {
      return -1;
    }

    /* Check not share CTB: avoid loop all ctb */
    if ((xoffsetId + cropWidthId) <= xoffset &&
        (yoffsetId + cropHeightId) <= yoffset) {
      tmpx = ((xoffsetId + cropWidthId - 1) / blockW) * blockW;
      tmpy = ((yoffsetId + cropHeightId - 1) / blockH) * blockH;

      if (tmpx + blockW > xoffset && tmpy + blockH > yoffset) {
        return -1;
      }
    } else if ((xoffsetId + cropWidthId) <= xoffset &&
               yoffsetId >= (yoffset + cropHeight)) {
      tmpx = ((xoffsetId + cropWidthId - 1) / blockW) * blockW;
      tmpy = ((yoffset + cropHeight - 1) / blockH) * blockH;
      if (tmpx + blockW > xoffset && tmpy + blockH > yoffsetId) {
        return -1;
      }
    } else if (xoffsetId >= (xoffset + cropWidth) &&
               (yoffsetId + cropHeightId) <= yoffset) {
      tmpx = ((xoffset + cropWidth - 1) / blockW) * blockW;
      tmpy = ((yoffsetId + cropHeightId - 1) / blockH) * blockH;
      if (tmpx + blockW > xoffsetId && tmpy + blockH > yoffset) {
        return -1;
      }
    } else if (xoffsetId >= (xoffset + cropWidth) &&
               yoffsetId >= (yoffset + cropHeight)) {
      tmpx = ((xoffset + cropWidth - 1) / blockW) * blockW;
      tmpy = ((yoffset + cropHeight - 1) / blockH) * blockH;
      if (tmpx + blockW > xoffsetId && tmpy + blockH > yoffsetId) {
        return -1;
      }
    } else if ((xoffsetId + cropWidthId) <= xoffset) {
      tmpx = ((xoffsetId + cropWidthId - 1) / blockW) * blockW;
      if (tmpx + blockW > xoffset) return -1;
    } else if ((yoffsetId + cropHeightId) <= yoffset) {
      tmpy = ((yoffsetId + cropHeightId - 1) / blockH) * blockH;
      if (tmpy + blockH > yoffset) return -1;
    } else if (xoffsetId >= (xoffset + cropWidth)) {
      tmpx = ((xoffset + cropWidth - 1) / blockW) * blockW;
      if (tmpx + blockW > xoffsetId) return -1;
    } else if (yoffsetId >= (yoffset + cropHeight)) {
      tmpy = ((yoffset + cropHeight - 1) / blockH) * blockH;
      if (tmpy + blockH > yoffsetId) return -1;
    }
  }

  return 0;
}

/* An example of user defined quantization table */
u8 qTableLuma[64] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                     1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
u8 qTableChroma[64] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
                       1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

/*------------------------------------------------------------------------------

    OpenEncoder

------------------------------------------------------------------------------*/
int OpenEncoder(commandLine_s *cml, JpegEncInst *pEnc) {
  JpegEncRet ret;

  if ((cml->qLevel == USER_DEFINED_QTABLE) &&
      (0 != strcmp(cml->qTablePath, ""))) {
    if (0 != ReadQTable(cml->qTablePath, qTableLuma, qTableChroma)) {
      fprintf(stderr, "Failed to open qtable from file %s\n. ",
              cml->qTablePath);
      return -1;
    }
  }

#ifndef TB_DEFINED_COMMENT
  FILE *fileCom = NULL;
#endif

  /* Default resolution, try parsing input file name */
  if (cml->lumWidthSrc == DEFAULT || cml->lumHeightSrc == DEFAULT) {
    if (GetResolution(cml->input, &cml->lumWidthSrc, &cml->lumHeightSrc)) {
      /* No dimensions found in filename, using default QCIF */
      cml->lumWidthSrc = 176;
      cml->lumHeightSrc = 144;
    }
  }

  if ((cml->lumWidthSrc > 65535) || (cml->lumHeightSrc > 65535)) return -1;

  /* Encoder initialization */
  if (cml->width == DEFAULT) cml->width = cml->lumWidthSrc;

  if (cml->height == DEFAULT) cml->height = cml->lumHeightSrc;

  if (cml->osdMapStride == DEFAULT) cml->osdMapStride = cml->width / 16;

  /* Not sure the usage of following codes, comment out for now */
  /*if (cml->exp_of_input_alignment == 7)
    {
      if (cml->frameType==JPEGENC_YUV420_PLANAR || cml->frameType==JPEGENC_YVU420_PLANAR)
        cml->horOffsetSrc = ((cml->horOffsetSrc + 256 - 1) & (~(256 - 1)));
      else
        cml->horOffsetSrc = ((cml->horOffsetSrc + 128 - 1) & (~(128 - 1)));

      if ((cml->lumWidthSrc - cml->horOffsetSrc)< cml->width)
        cml->horOffsetSrc = 0;
    }*/

  /* overlay controls */
  u32 i;
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    if ((cml->overlayEnables >> i) & 1) {
      if (cml->olYStride[i] == 0) {
        switch (cml->olFormat[i]) {
          case 0:  //ARGB
            if (cml->olSuperTile[i] == 0)
              cml->olYStride[i] = cml->olWidth[i] * 4;
            else
              cml->olYStride[i] = ((cml->olWidth[i] + 63) / 64) * 64 * 64 * 4;
            break;
          case 1:  //NV12
            cml->olYStride[i] = cml->olWidth[i];
            break;
          case 2:  //Bitmap
            cml->olYStride[i] = cml->olWidth[i] / 8;
            break;
          default:
            break;
        }
      }

      if (cml->olUVStride[i] == 0) cml->olUVStride[i] = cml->olYStride[i];

      if (cml->olCropHeight[i] == 0) cml->olCropHeight[i] = cml->olHeight[i];
      if (cml->olCropWidth[i] == 0) cml->olCropWidth[i] = cml->olWidth[i];
      if (cml->olScaleWidth[i] == 0) cml->olScaleWidth[i] = cml->olCropWidth[i];
      if (cml->olScaleHeight[i] == 0)
        cml->olScaleHeight[i] = cml->olCropHeight[i];

      if (cml->olWidth[i] == 0 || cml->olHeight[i] == 0) {
        fprintf(stderr, "\nInvalid overlay region %u size\n", i + 1);
        return -1;
      }

      u64 widthMax =
          (cml->rotation == 1 || cml->rotation == 2) ? cml->height : cml->width;
      u64 heightMax =
          (cml->rotation == 1 || cml->rotation == 2) ? cml->width : cml->height;
      if (cml->olCropWidth[i] + cml->olXoffset[i] > widthMax ||
          cml->olCropHeight[i] + cml->olYoffset[i] > heightMax) {
        fprintf(stderr, "\nInvalid overlay region %u offset\n", i + 1);
        return -1;
      }

      if (cml->olCropXoffset[i] + cml->olCropWidth[i] > cml->olWidth[i] ||
          cml->olCropYoffset[i] + cml->olCropHeight[i] > cml->olHeight[i]) {
        fprintf(stderr, "\nInvalid overlay region %u cropping offset\n", i + 1);
        return -1;
      }

      if (cml->olFormat[i] == 2 && (cml->olCropWidth[i] % 8 != 0)) {
        fprintf(stderr,
                "\nInvalid overlay region %u cropping width for bitmap \n",
                i + 1);
        return -1;
      }

      if (jpeg_osd_overlap(cml, i)) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR overlay area overlapping \n");
        return -1;
      }

      if (cml->olSuperTile[i] != 0 && cml->olFormat[i] != 0) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR Super tile only support ARGB8888 "
                "format \n");
        return -1;
      }

      if (cml->olSuperTile[i] != 0 && i != 0) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR Super tile only support for channel "
                "1 \n");
        return -1;
      }

      if (cml->olSuperTile[i] != 0 && cml->partialCoding != 0) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR OSD Super tile does not support "
                "partialCoding  \n");
        return -1;
      }

      if ((cml->olSuperTile[i] == 0 &&
           cml->olScaleWidth[i] != cml->olCropWidth[i]) ||
          (cml->olSuperTile[i] == 0 &&
           cml->olScaleHeight[i] != cml->olCropHeight[i]) ||
          (cml->olScaleWidth[i] != cml->olCropWidth[i] && i != 0) ||
          (cml->olScaleHeight[i] != cml->olCropHeight[i] && i != 0)) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR Up scale only work for special "
                "usage \n");
        return -1;
      }

      if (cml->olSuperTile[i] != 0 && (cml->olCropXoffset[i] % 64 != 0 ||
                                       cml->olCropYoffset[i] % 64 != 0)) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR super tile cropping offset must be "
                "64 aligned \n");
        return -1;
      }

      if (cml->olSuperTile[i] != 0 &&
          (cml->olScaleWidth[i] < cml->olCropWidth[i] ||
           cml->olScaleHeight[i] < cml->olCropHeight[i])) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR osd only support upscaling but no "
                "downscaling \n");
        return -1;
      }

      if (cml->olSuperTile[i] != 0 &&
          ((cml->olScaleWidth[i] & 1) ||
           cml->olScaleWidth[i] > (cml->olCropWidth[i] * 2) ||
           (cml->olScaleHeight[i] & 1) ||
           cml->olScaleHeight[i] > (cml->olCropHeight[i] * 2))) {
        fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR osd upscaling width or height \n");
        return -1;
      }
    }
  }

  /* lossless mode */
  if (cml->predictMode != 0) {
    cfg.losslessEn = 1;
    cfg.predictMode = cml->predictMode;
    cfg.ptransValue = cml->ptransValue;
  } else {
    cfg.losslessEn = 0;
  }

  cfg.rotation = (JpegEncPictureRotation)cml->rotation;
  cfg.inputWidth = cml->lumWidthSrc;
  cfg.inputHeight = cml->lumHeightSrc;

  cfg.xOffset = cml->horOffsetSrc;
  cfg.yOffset = cml->verOffsetSrc;

  if (cfg.rotation && cfg.rotation != JPEGENC_ROTATE_180) {
    /* full */
    cfg.codingWidth = cml->height;
    cfg.codingHeight = cml->width;
    cfg.xDensity = cml->ydensity;
    cfg.yDensity = cml->xdensity;
  } else {
    /* full */
    cfg.codingWidth = cml->width;
    cfg.codingHeight = cml->height;
    cfg.xDensity = cml->xdensity;
    cfg.yDensity = cml->ydensity;
  }
  cfg.mirror = cml->mirror;

  if (cml->quality == -1) {
    if (cml->qLevel == USER_DEFINED_QTABLE) {
      cfg.qTableLuma = qTableLuma;
      cfg.qTableChroma = qTableChroma;
    } else {
      cfg.qLevel = cml->qLevel;
    }
    cfg.quality = -1;
  } else {
    cfg.quality = cml->quality;
  }

  cfg.restartInterval = cml->restartInterval;
  cfg.codingType = (JpegEncCodingType)cml->partialCoding;
  cfg.frameType = (JpegEncFrameType)cml->frameType;
  cfg.unitsType = (JpegEncAppUnitsType)cml->unitsType;
  cfg.markerType = (JpegEncTableMarkerType)cml->markerType;
  cfg.colorConversion.type = (JpegEncColorConversionType)cml->colorConversion;
  if (cfg.colorConversion.type == JPEGENC_RGBTOYUV_USER_DEFINED) {
    /* User defined RGB to YCbCr conversion coefficients, scaled by 16-bits */
    cfg.colorConversion.coeffA = 20000;
    cfg.colorConversion.coeffB = 44000;
    cfg.colorConversion.coeffC = 5000;
    cfg.colorConversion.coeffE = 35000;
    cfg.colorConversion.coeffF = 38000;
    cfg.colorConversion.coeffG = 35000;
    cfg.colorConversion.coeffH = 38000;
    cfg.colorConversion.LumaOffset = 0;
  }
  writeOutput = cml->writeOut;
  cfg.codingMode = (JpegEncCodingMode)cml->codingMode;

  /* low latency */
  cfg.inputLineBufEn = (cml->inputLineBufMode > 0) ? 1 : 0;
  cfg.inputLineBufLoopBackEn =
      (cml->inputLineBufMode == 1 || cml->inputLineBufMode == 2) ? 1 : 0;
  cfg.inputLineBufDepth = cml->inputLineBufDepth;
  cfg.amountPerLoopBack = cml->amountPerLoopBack;
  cfg.inputLineBufHwModeEn =
      (cml->inputLineBufMode == 2 || cml->inputLineBufMode == 4) ? 1 : 0;
  cfg.inputLineBufCbFunc = VCEncInputLineBufDone;
  cfg.inputLineBufCbData = &inputMbLineBuf;
  cfg.hashType = cml->hashtype;
  cfg.lowlatGatingDisable = cml->lowlatGatingDisable;
  if (cfg.lowlatGatingDisable > 1) {
    fprintf(stderr,
                "VCEncSetCodingCtrl: ERROR lowlatGatingDisable out of range.\n");
        return -1;
  }

  /* flexa sbi */
  cfg.sbi_id_0 = 0;
  cfg.sbi_id_1 = 1;
  cfg.sbi_id_2 = 2;
  cfg.segmentUnitHeight = cml->segmentUnitHeight;

  /*stream multi-segment*/
  cfg.streamMultiSegmentMode = cml->streamMultiSegmentMode;
  cfg.streamMultiSegmentAmount = cml->streamMultiSegmentAmount;
  cfg.streamMultiSegCbFunc = &EncStreamSegmentReady;
  cfg.streamMultiSegCbData = &streamSegCtl;
  cfg.streamMultiSegmentSize = cml->streamMultiSegmentSize;

  /* constant chroma control */
  cfg.constChromaEn = cml->constChromaEn;
  cfg.constCb = cml->constCb;
  cfg.constCr = cml->constCr;

  /* jpeg rc*/
  cfg.targetBitPerSecond = cml->bitPerSecond;
  cfg.frameRateNum = 1;
  cfg.frameRateDenom = 1;

  //framerate valid only when RC enabled
  if (cml->bitPerSecond) {
    cfg.frameRateNum = cml->frameRateNum;
    cfg.frameRateDenom = cml->frameRateDenom;
  }
  cfg.qpmin = cml->qpmin;
  cfg.qpmax = cml->qpmax;
  cfg.fixedQP = cml->fixedQP;
  cfg.rcMode = cml->rcMode;
  cfg.picQpDeltaMax = cml->picQpDeltaMax;
  cfg.picQpDeltaMin = cml->picQpDeltaMin;

  /*stride*/
  cfg.exp_of_input_alignment = cml->exp_of_input_alignment;

  /* overlay control */
  for (i = 0; i < MAX_OVERLAY_NUM; i++) {
    cfg.olEnable[i] = (cml->overlayEnables >> i) & 1;
    cfg.olFormat[i] = cml->olFormat[i];
    cfg.olAlpha[i] = cml->olAlpha[i];
    cfg.olWidth[i] = cml->olWidth[i];
    cfg.olCropWidth[i] = cml->olCropWidth[i];
    cfg.olHeight[i] = cml->olHeight[i];
    cfg.olCropHeight[i] = cml->olCropHeight[i];
    cfg.olXoffset[i] = cml->olXoffset[i];
    cfg.olCropXoffset[i] = cml->olCropXoffset[i];
    cfg.olYoffset[i] = cml->olYoffset[i];
    cfg.olCropYoffset[i] = cml->olCropYoffset[i];
    cfg.olYStride[i] = cml->olYStride[i];
    cfg.olUVStride[i] = cml->olUVStride[i];
    cfg.olBitmapY[i] = cml->olBitmapY[i];
    cfg.olBitmapU[i] = cml->olBitmapU[i];
    cfg.olBitmapV[i] = cml->olBitmapV[i];
    cfg.olSuperTile[i] = cml->olSuperTile[i];
    cfg.olScaleWidth[i] = cml->olScaleWidth[i];
    cfg.olScaleHeight[i] = cml->olScaleHeight[i];
  }

  /* mosaic controls */
  for (i = 0; i < MAX_MOSAIC_NUM; i++) {
    cfg.mosEnable[i] = (cml->mosaicEnables >> i) & 1;
    cfg.mosWidth[i] = cml->mosWidth[i];
    cfg.mosHeight[i] = cml->mosHeight[i];
    cfg.mosXoffset[i] = cml->mosXoffset[i];
    cfg.mosYoffset[i] = cml->mosYoffset[i];
  }
  cfg.mosSizeIndex = cml->mosSizeIndex;

  /* Set OSD map parameters */
  cfg.osdMapEnable = cml->osdMapEnable;
  //cfg.osdMapInput = cml->osdMapInput;
  cfg.osdMapStride = cml->osdMapStride;
  cfg.osdMapBlockSize = cml->osdMapBlockSize;
  for (i = 0; i < MAX_OSDMAP_COLOR_NUM; i++) {
    cfg.osdMapAlpha[i] = cml->osdMapAlpha[i];
    cfg.osdMapY[i] = cml->osdMapY[i];
    cfg.osdMapU[i] = cml->osdMapU[i];
    cfg.osdMapV[i] = cml->osdMapV[i];
  }

  /* SRAM power down mode disable  */
  cfg.sramPowerdownDisable = cml->sramPowerdownDisable;
  if(cfg.sramPowerdownDisable == 0) {
    cfg.sramPowerdownMode = cml->sramPowerdownMode;
    cfg.sramPowerdownTimerDiv32 = cml->sramPowerdownTimerDiv32;
  }

  /* dump Registers enable or not */
  cfg.dumpRegister = cml->dumpRegister;

  cfg.priority = cml->priority;
  cfg.core_mask = cml->core_mask;

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  cfg.sliceinfoEn = cml->inputSliceInfoEn;
#endif
#ifdef NO_OUTPUT_WRITE
  writeOutput = 0;
#endif

  if (cml->thumbnail < 0 || cml->thumbnail > 3) {
    fprintf(stderr, "\nNot valid thumbnail format!");
    return -1;
  }

  if (cml->thumbnail != 0) {
    FILE *fThumb;
    size_t size = 0;
    fThumb = fopen(cml->inputThumb, "rb");
    if (fThumb == NULL) {
      fprintf(stderr, "\nUnable to open Thumbnail file: %s\n", cml->inputThumb);
      return -1;
    }

    switch (cml->thumbnail) {
      case 1:
        fseek(fThumb, 0, SEEK_END);
        thumbDataLength = ftell(fThumb);
        fseek(fThumb, 0, SEEK_SET);
        break;
      case 2:
        thumbDataLength = 3 * 256 + cml->widthThumb * cml->heightThumb;
        break;
      case 3:
        thumbDataLength = cml->widthThumb * cml->heightThumb * 3;
        break;
      default:
        ASSERT(0);
    }

    thumbData = (u8 *)malloc(thumbDataLength);
    if (thumbData == NULL) {
      fprintf(stderr, "\nUnable to malloc buffer thumbData.\n");
      fclose(fThumb);
      return -1;
    }
    size = fread(thumbData, 1, thumbDataLength, fThumb);
    fclose(fThumb);
  }

  cfg.AXIAlignment = cml->AXIAlignment;
  cfg.irqTypeMask = cml->irqTypeMask;
  cfg.burstMaxLength = cml->burstMaxLength;

  cfg.ufbcParam.mode = cml->ufbcMode;
  if (cml->ufbcMode == JPEG_UFBC_MODE_AFBC_0 || cml->ufbcMode == JPEG_UFBC_MODE_AFBC_1) {
    cfg.ufbcParam.param.afbc.yuvTrans = cml->ufbcYuvTrans;
    cfg.ufbcParam.param.afbc.blockType = cml->ufbcBlockType;
    cfg.ufbcParam.param.afbc.blockSplit = cml->ufbcBlockSplit;
  } else if (cml->ufbcMode == JPEG_UFBC_MODE_PVRIC) {
    cfg.ufbcParam.param.pvric.blockType = cml->ufbcBlockType;
    GetConstantVal(cml->frameType, cml->ufbcConstantVal, cfg.ufbcParam.param.pvric.consColorVal);
  }

  cfg.enc_dev = cml->encDevice;
  cfg.mem_dev = cml->memDevice;
  cfg.useVcmd = cml->useVcmd;
  cfg.scanType = cml->scanType;

/* use either "hard-coded"/testbench COM data or user specific */
#ifdef TB_DEFINED_COMMENT
  cfg.comLength = comLen;
  cfg.pCom = comment;
#else
  cfg.comLength = cml->comLength;

  if (cfg.comLength) {
    /* allocate mem for & read comment data */
    cfg.pCom = (u8 *)malloc(cfg.comLength);

    fileCom = fopen(cml->com, "rb");
    if (fileCom == NULL) {
      fprintf(stderr, "\nUnable to open COMMENT file: %s\n", cml->com);
      return -1;
    }

    fread((void *)cfg.pCom, 1, cfg.comLength, fileCom);
    fclose(fileCom);
  }

#endif

#ifndef ASIC_WAVE_TRACE_TRIGGER
  fprintf(stdout, "Init config: %ux%u @ x%uy%u => %ux%u   \n", cfg.inputWidth,
          cfg.inputHeight, cfg.xOffset, cfg.yOffset, cfg.codingWidth,
          cfg.codingHeight);

  fprintf(stdout,
          "\n\t**********************************************************\n");
  fprintf(stdout, "\n\t-JPEG: ENCODER CONFIGURATION\n");
  if (cml->quality != -1) {
    fprintf(stdout, "\t-JPEG: Quality Factor \t:%d\n", cml->quality);
  } else {
    if (cml->qLevel == USER_DEFINED_QTABLE) {
      i32 i;
      fprintf(stdout, "JPEG: qTableLuma \t:");
      for (i = 0; i < 64; i++) fprintf(stdout, " %d", cfg.qTableLuma[i]);
      fprintf(stdout, "\n");
      fprintf(stdout, "JPEG: qTableChroma \t:");
      for (i = 0; i < 64; i++) fprintf(stdout, " %d", cfg.qTableChroma[i]);
      fprintf(stdout, "\n");
    } else {
      fprintf(stdout, "\t-JPEG: qp \t\t:%u\n", cfg.qLevel);
    }
  }
  fprintf(stdout, "\t-JPEG: inX \t\t:%u\n", cfg.inputWidth);
  fprintf(stdout, "\t-JPEG: inY \t\t:%u\n", cfg.inputHeight);
  fprintf(stdout, "\t-JPEG: outX \t\t:%u\n", cfg.codingWidth);
  fprintf(stdout, "\t-JPEG: outY \t\t:%u\n", cfg.codingHeight);
  fprintf(stdout, "\t-JPEG: rst \t\t:%u\n", cfg.restartInterval);
  fprintf(stdout, "\t-JPEG: xOff \t\t:%u\n", cfg.xOffset);
  fprintf(stdout, "\t-JPEG: yOff \t\t:%u\n", cfg.yOffset);
  fprintf(stdout, "\t-JPEG: frameType \t:%d\n", cfg.frameType);
  fprintf(stdout, "\t-JPEG: colorConversionType :%d\n",
          cfg.colorConversion.type);
  fprintf(stdout, "\t-JPEG: colorConversionA    :%d\n",
          cfg.colorConversion.coeffA);
  fprintf(stdout, "\t-JPEG: colorConversionB    :%d\n",
          cfg.colorConversion.coeffB);
  fprintf(stdout, "\t-JPEG: colorConversionC    :%d\n",
          cfg.colorConversion.coeffC);
  fprintf(stdout, "\t-JPEG: colorConversionE    :%d\n",
          cfg.colorConversion.coeffE);
  fprintf(stdout, "\t-JPEG: colorConversionF    :%d\n",
          cfg.colorConversion.coeffF);
  fprintf(stdout, "\t-JPEG: rotation \t:%d\n", cfg.rotation);
  fprintf(stdout, "\t-JPEG: codingType \t:%d\n", cfg.codingType);
  fprintf(stdout, "\t-JPEG: codingMode \t:%d\n", cfg.codingMode);
  fprintf(stdout, "\t-JPEG: markerType \t:%d\n", cfg.markerType);
  fprintf(stdout, "\t-JPEG: units \t\t:%d\n", cfg.unitsType);
  fprintf(stdout, "\t-JPEG: xDen \t\t:%u\n", cfg.xDensity);
  fprintf(stdout, "\t-JPEG: yDen \t\t:%u\n", cfg.yDensity);

  fprintf(stdout, "\t-JPEG: thumbnail format\t:%d\n", cml->thumbnail);
  fprintf(stdout, "\t-JPEG: Xthumbnail\t:%d\n", cml->widthThumb);
  fprintf(stdout, "\t-JPEG: Ythumbnail\t:%d\n", cml->heightThumb);

  fprintf(stdout, "\t-JPEG: First picture\t:%d\n", cml->firstPic);
  fprintf(stdout, "\t-JPEG: Last picture\t\t:%d\n", cml->lastPic);
  fprintf(stdout, "\t-JPEG: inputLineBufEn \t\t:%u\n", cfg.inputLineBufEn);
  fprintf(stdout, "\t-JPEG: inputLineBufLoopBackEn \t:%u\n",
          cfg.inputLineBufLoopBackEn);
  fprintf(stdout, "\t-JPEG: inputLineBufHwModeEn \t:%u\n",
          cfg.inputLineBufHwModeEn);
  fprintf(stdout, "\t-JPEG: inputLineBufDepth \t:%u\n", cfg.inputLineBufDepth);
  fprintf(stdout, "\t-JPEG: amountPerLoopBack \t:%u\n", cfg.amountPerLoopBack);

  fprintf(stdout, "\t-JPEG: streamMultiSegmentMode \t:%u\n",
          cfg.streamMultiSegmentMode);
  fprintf(stdout, "\t-JPEG: streamMultiSegmentAmount \t:%u\n",
          cfg.streamMultiSegmentAmount);

  fprintf(stdout, "\t-JPEG: constChromaEn \t:%d\n", cfg.constChromaEn);
  fprintf(stdout, "\t-JPEG: constCb \t:%u\n", cfg.constCb);
  fprintf(stdout, "\t-JPEG: constCr \t:%u\n", cfg.constCr);

#ifdef TB_DEFINED_COMMENT
  fprintf(stdout, "\n\tNOTE! Using comment values defined in testbench!\n");
#else
  fprintf(stdout, "\t-JPEG: comlen \t\t:%u\n", cfg.comLength);
  fprintf(stdout, "\t-JPEG: COM \t\t:%s\n", cfg.pCom);
#endif

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  fprintf(stdout, "\t-JPEG: sliceinfoEn \t\t:%u\n", cfg.sliceinfoEn);
#endif

  fprintf(stdout,
          "\n\t**********************************************************\n\n");
#endif

  if ((ret = JpegEncInit(&cfg, pEnc, NULL)) != JPEGENC_OK) {
    fprintf(stderr, "Failed to initialize the encoder. Error code: %8i\n", ret);
    return (int)ret;
  }

  if (thumbData != NULL) {
    JpegEncThumb jpegThumb;
    jpegThumb.format = cml->thumbnail == 1
                           ? JPEGENC_THUMB_JPEG
                           : cml->thumbnail == 3 ? JPEGENC_THUMB_RGB24
                                                 : JPEGENC_THUMB_PALETTE_RGB8;
    jpegThumb.width = cml->widthThumb;
    jpegThumb.height = cml->heightThumb;
    jpegThumb.data = thumbData;
    jpegThumb.dataLength = thumbDataLength;

    ret = JpegEncSetThumbnail(*pEnc, &jpegThumb);
    if (ret != JPEGENC_OK) {
      fprintf(stderr, "Failed to set thumbnail. Error code: %8i\n", ret);
      return -1;
    }
  }

  if (cml->qpHdr != -1) {
    JpegEncRateCtrl rc;
    JpegEncGetRateCtrl(*pEnc, &rc);
    rc.qpHdr = cml->qpHdr;
    JpegEncSetRateCtrl(*pEnc, &rc);
  }

  return 0;
}

/*------------------------------------------------------------------------------

    CloseEncoder

------------------------------------------------------------------------------*/
void CloseEncoder(JpegEncInst encoder) {
  JpegEncRet ret;

  if ((ret = JpegEncRelease(encoder)) != JPEGENC_OK) {
    fprintf(stderr, "Failed to release the encoder. Error code: %8i\n", ret);
  }
}

/*------------------------------------------------------------------------------

    Parameter

------------------------------------------------------------------------------*/
int ParseDelim(char *optArg, char delim) {
  i32 i;

  for (i = 0; i < (i32)strlen(optArg); i++)
    if (optArg[i] == delim) {
      optArg[i] = 0;
      return i;
    }

  return -1;
}

int Parameter(i32 argc, char **argv, commandLine_s *cml) {
  i32 ret, i, j;
  char *optarg;
  argument_s argument;
  int status = 0;
  argument.optCnt = 1;
  while ((ret = EncGetOption(argc, argv, options, &argument)) != -1) {
    if (ret == 2)
      continue;
    if (ret != 0) {
      status = -1;
      fprintf(stderr, "Error: Invalid Option %s\n", argv[argument.optCnt-1]);
    }
    optarg = argument.optArg;
    switch (argument.shortOpt) {
      case 'H':
        Help();
        exit(0);
      case 'i':
        if (strlen(optarg) < MAX_PATH) {
          strcpy(cml->input, optarg);
        } else {
          status = -1;
        }
        break;
      case 'I':
        if (strlen(optarg) < MAX_PATH) {
          strcpy(cml->inputThumb, optarg);
        } else {
          status = -1;
        }
        break;
      case 'o':
        if (strlen(optarg) < MAX_PATH) {
          strcpy(cml->output, optarg);
        } else {
          status = -1;
        }
        break;
      case 'C':
        if (strlen(optarg) < MAX_PATH) {
          strcpy(cml->com, optarg);
        } else {
          status = -1;
        }
        break;
      case 'a':
        cml->firstPic = atoi(optarg);
        break;
      case 'b':
        cml->lastPic = atoi(optarg);
        break;
      case 'x':
        cml->width = atoi(optarg);
        break;
      case 'y':
        cml->height = atoi(optarg);
        break;
      case 'w':
        cml->lumWidthSrc = atoi(optarg);
        break;
      case 'h':
        cml->lumHeightSrc = atoi(optarg);
        break;
      case 'X':
        cml->horOffsetSrc = atoi(optarg);
        break;
      case 'Y':
        cml->verOffsetSrc = atoi(optarg);
        break;
      case 'R':
        cml->restartInterval = atoi(optarg);
        break;
      case 'q':
        cml->qLevel = atoi(optarg);
        break;
      case 'g':
        cml->frameType = atoi(optarg);
        break;
      case 'v':
        cml->colorConversion = atoi(optarg);
        break;
      case 'G':
        cml->rotation = atoi(optarg);
        break;
      case 'p':
        cml->partialCoding = atoi(optarg);
        break;
      case 'm':
        cml->codingMode = atoi(optarg);
        break;
      case 't':
        cml->markerType = atoi(optarg);
        break;
      case 'u':
        cml->unitsType = atoi(optarg);
        break;
      case 'k':
        cml->xdensity = atoi(optarg);
        break;
      case 'l':
        cml->ydensity = atoi(optarg);
        break;
      case 'T':
        cml->thumbnail = atoi(optarg);
        break;
      case 'K':
        cml->widthThumb = atoi(optarg);
        break;
      case 'L':
        cml->heightThumb = atoi(optarg);
        break;
      case 'W':
        cml->writeOut = atoi(optarg);
        break;
      case 'c':
        cml->comLength = atoi(optarg);
        break;
      case 'P':
        trigger_point = atoi(optarg);
        break;
      case 'S':
        cml->inputLineBufMode = atoi(optarg);
        break;
      case 'N':
        cml->inputLineBufDepth = atoi(optarg);
        break;
      case 's':
        cml->amountPerLoopBack = atoi(optarg);
        break;
      case 'A':
        cml->hashtype = atoi(optarg);
        break;
      case 'M':
        cml->mirror = atoi(optarg);
        break;
      case 'D':
        cml->formatCustomizedType = atoi(optarg);
        break;
      case 'd':
        cml->constChromaEn = atoi(optarg);
        break;
      case 'e':
        cml->constCb = atoi(optarg);
        break;
      case 'f':
        cml->constCr = atoi(optarg);
        break;

      case '1': /* --lossless [n] */
        cml->predictMode = atoi(optarg);
        break;
      case '2': /* --ptrans [n] */
        cml->ptransValue = atoi(optarg);
        break;
      case 'B':
        cml->bitPerSecond = atoi(optarg);
        break;
      case 'J':
        cml->mjpeg = atoi(optarg);
        break;
      case 'n':
        cml->frameRateNum = atoi(optarg);
        break;
      case 'r':
        cml->frameRateDenom = atoi(optarg);
        break;
      case 'V':
        cml->rcMode = atoi(optarg);
        if ((cml->rcMode < 0) || (cml->rcMode > 2)) status = -1;
        break;
      case 'E':
        cml->qpmin = atoi(optarg);
        break;
      case 'F':
        cml->qpmax = atoi(optarg);
        break;
      case 'U':
        if ((i = ParseDelim(optarg, ':')) == -1) break;
        cml->picQpDeltaMin = atoi(optarg);
        optarg += i + 1;
        cml->picQpDeltaMax = atoi(optarg);

        if ((cml->picQpDeltaMin < -10) || (cml->picQpDeltaMin > -1) ||
            (cml->picQpDeltaMax < 1) || (cml->picQpDeltaMax > 10))
          status = -1;
        break;
      case 'O':
        cml->fixedQP = atoi(optarg);
        break;
      case 'Q':
        cml->exp_of_input_alignment = atoi(optarg);
        break;

      case '0':
        /* Check long option */
        if (strcmp(argument.longOpt, "encDevice") == 0) {
          cml->encDevice = optarg;
          break;
        }

        if (strcmp(argument.longOpt, "memDevice") == 0) {
          cml->memDevice = optarg;
          break;
        }

        if (strcmp(argument.longOpt, "scanType") == 0) {
          cml->scanType = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "segmentUnitHeight") == 0) {
          cml->segmentUnitHeight = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "streamBufChain") == 0) {
          cml->streamBufChain = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "streamMultiSegmentMode") == 0) {
          cml->streamMultiSegmentMode = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "streamMultiSegmentSize") == 0) {
          cml->streamMultiSegmentSize = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "streamMultiSegmentAmount") == 0) {
          cml->streamMultiSegmentAmount = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "qTableFile") == 0) {
          strcpy(cml->qTablePath, optarg);
          break;
        }

        if (strcmp(argument.longOpt, "qpHdr") == 0) {
          cml->qpHdr = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "quality") == 0) {
          cml->quality = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400TableInput") == 0) {
          strcpy(cml->dec400CompTableinput, optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400TileSize") == 0) {
          cml->dec400TileSize = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400TileMode") == 0) {
          cml->dec400TileMode = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400DataAlignment") == 0) {
          cml->dec400DataAlignment = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400RGBAX") == 0) {
          cml->dec400RGBAX = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400RGBFormat") == 0) {
          cml->dec400RGBFormat = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dec400TSHeaderEnable") == 0) {
          cml->dec400TSHeaderEnable = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "overlayEnables") == 0) {
          cml->overlayEnables = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "osdMapEnable") == 0) {
          cml->osdMapEnable = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "osdMapStride") == 0) {
          cml->osdMapStride = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "osdMapBlockSize") == 0) {
          cml->osdMapBlockSize = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "osdMapInput") == 0) {
          if (strlen(optarg) < MAX_PATH) {
          strcpy(cml->osdMapInput, optarg);
          } else {
            status = -1;
          }
          break;
        }

        if (strcmp(argument.longOpt, "olSuperTile01") == 0) {
          cml->olSuperTile[0] = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "olScaleWidth01") == 0) {
          cml->olScaleWidth[0] = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "olScaleHeight01") == 0) {
          cml->olScaleHeight[0] = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "logOutDir") == 0) {
          cml->logOutDir = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "logOutLevel") == 0) {
          cml->logOutLevel = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "logTraceMap") == 0) {
          cml->logTraceMap = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "dumpRegister") == 0) {
          cml->dumpRegister = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "secure_mode") == 0) {
          cml->secure_mode = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "inputSliceInfoEn") == 0) {
          cml->inputSliceInfoEn = atoi(optarg);
          break;
        }

        for (i = 0; i < MAX_OVERLAY_NUM; i++) {
          char optionTemp[22];
          /* length 7 */
          if (i >= 9) {
            optionTemp[7] = '1';
            optionTemp[8] = ((i + 1) % 10) + 48;
            optionTemp[9] = '\0';
          } else {
            optionTemp[7] = '0';
            optionTemp[8] = (i + 1) + 48;
            optionTemp[9] = '\0';
          }
          memcpy(optionTemp, "olInput", 7);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            if (strlen(optarg) < MAX_PATH) {
              strcpy(cml->olInput[i], optarg);
            } else {
              status = -1;
            }
            break;
          }

          memcpy(optionTemp, "olAlpha", 7);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olAlpha[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olWidth", 7);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olWidth[i] = atoi(optarg);
            break;
          }
          // length 8
          if (i >= 9) {
            optionTemp[8] = '1';
            optionTemp[9] = ((i + 1) % 10) + 48;
            optionTemp[10] = '\0';
          } else {
            optionTemp[8] = '0';
            optionTemp[9] = (i + 1) + 48;
            optionTemp[10] = '\0';
          }
          memcpy(optionTemp, "olFormat", 8);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olFormat[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olHeight", 8);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olHeight[i] = atoi(optarg);
            break;
          }

          //length 9
          if (i >= 9) {
            optionTemp[9] = '1';
            optionTemp[10] = ((i + 1) % 10) + 48;
            optionTemp[11] = '\0';
          } else {
            optionTemp[9] = '0';
            optionTemp[10] = (i + 1) + 48;
            optionTemp[11] = '\0';
          }
          memcpy(optionTemp, "olXoffset", 9);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olXoffset[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olYoffset", 9);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olYoffset[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olYStride", 9);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olYStride[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olBitmapY", 9);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olBitmapY[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olBitmapU", 9);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olBitmapU[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "olBitmapV", 9);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olBitmapV[i] = atoi(optarg);
            break;
          }

          //length 10
          if (i >= 9) {
            optionTemp[10] = '1';
            optionTemp[11] = ((i + 1) % 10) + 48;
            optionTemp[12] = '\0';
          } else {
            optionTemp[10] = '0';
            optionTemp[11] = (i + 1) + 48;
            optionTemp[12] = '\0';
          }
          memcpy(optionTemp, "olUVStride", 10);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olUVStride[i] = atoi(optarg);
            break;
          }

          //length 11
          if (i >= 9) {
            optionTemp[11] = '1';
            optionTemp[12] = ((i + 1) % 10) + 48;
            optionTemp[13] = '\0';
          } else {
            optionTemp[11] = '0';
            optionTemp[12] = (i + 1) + 48;
            optionTemp[13] = '\0';
          }
          memcpy(optionTemp, "olCropWidth", 11);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olCropWidth[i] = atoi(optarg);
            break;
          }

          //length 12
          if (i >= 9) {
            optionTemp[12] = '1';
            optionTemp[13] = ((i + 1) % 10) + 48;
            optionTemp[14] = '\0';
          } else {
            optionTemp[12] = '0';
            optionTemp[13] = (i + 1) + 48;
            optionTemp[14] = '\0';
          }
          memcpy(optionTemp, "olCropHeight", 12);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olCropHeight[i] = atoi(optarg);
            break;
          }

          //length 13
          if (i >= 9) {
            optionTemp[13] = '1';
            optionTemp[14] = ((i + 1) % 10) + 48;
            optionTemp[15] = '\0';
          } else {
            optionTemp[13] = '0';
            optionTemp[14] = (i + 1) + 48;
            optionTemp[15] = '\0';
          }
          memcpy(optionTemp, "olCropXoffset", 13);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olCropXoffset[i] = atoi(optarg);
            break;
          }
          memcpy(optionTemp, "olCropYoffset", 13);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->olCropYoffset[i] = atoi(optarg);
            break;
          }
          //length 19
          if (i >= 9) {
            optionTemp[19] = '1';
            optionTemp[20] = ((i + 1) % 10) + 48;
            optionTemp[21] = '\0';
          } else {
            optionTemp[19] = '0';
            optionTemp[20] = (i + 1) + 48;
            optionTemp[21] = '\0';
          }
          memcpy(optionTemp, "osdDec400TableInput", 19);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            if (strlen(optarg) < MAX_PATH) {
              strcpy(cml->osdDec400CompTableInput[i], optarg);
            } else {
              status = -1;
            }
            break;
          }
        }
        for (i = 0; i < MAX_OVERLAY_NUM; i++) {
          char optionTemp[20];
          /* length 7 */
          if (i >= 9) {
            optionTemp[11] = '1';
            optionTemp[12] = ((i + 1) % 10) + 48;
            optionTemp[13] = '\0';
          } else {
            optionTemp[11] = '0';
            optionTemp[12] = (i + 1) + 48;
            optionTemp[13] = '\0';
          }
          memcpy(optionTemp, "osdMapAlpha",11);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->osdMapAlpha[i] = atoi(optarg);
            break;
          }
           /* length 7 */
          if (i >= 9) {
            optionTemp[7] = '1';
            optionTemp[8] = ((i + 1) % 10) + 48;
            optionTemp[9] = '\0';
          } else {
            optionTemp[7] = '0';
            optionTemp[8] = (i + 1) + 48;
            optionTemp[9] = '\0';
          }
          memcpy(optionTemp, "osdMapY", 7);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->osdMapY[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "osdMapU", 7);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->osdMapU[i] = atoi(optarg);
            break;
          }

          memcpy(optionTemp, "osdMapV", 7);
          if (strcmp(argument.longOpt, optionTemp) == 0) {
            cml->osdMapV[i] = atoi(optarg);
            break;
          }
        }

        if (strcmp(argument.longOpt, "mosaicEnables") == 0)
          cml->mosaicEnables = atoi(optarg);

        if (strcmp(argument.longOpt, "mosSizeIndex") == 0)
          cml->mosSizeIndex = atoi(optarg);

        for (i = 0; i < MAX_MOSAIC_NUM; i++) {
          char optionTemp[20];
          memcpy(optionTemp, "mosArea", 7);
          if (i >= 9) {
            optionTemp[7] = '1';
            optionTemp[8] = ((i + 1) % 10) + 48;
            optionTemp[9] = '\0';
          } else {
            optionTemp[7] = '0';
            optionTemp[8] = (i + 1) + 48;
            optionTemp[9] = '\0';
          }

          if (strcmp(argument.longOpt, optionTemp) == 0) {
            /* Argument must be "xx:yy:XX:YY".
                 * xx is left coordinate, replace first ':' with 0 */
            if ((j = ParseDelim(optarg, ':')) == -1) break;
            cml->mosXoffset[i] = atoi(optarg);
            /* yy is top coordinate */
            optarg += j + 1;
            if ((j = ParseDelim(optarg, ':')) == -1) break;
            cml->mosYoffset[i] = atoi(optarg);
            /* XX is right coordinate */
            optarg += j + 1;
            if ((j = ParseDelim(optarg, ':')) == -1) break;
            cml->mosWidth[i] = atoi(optarg) - cml->mosXoffset[i];
            /* YY is bottom coordinate */
            optarg += j + 1;
            cml->mosHeight[i] = atoi(optarg) - cml->mosYoffset[i];
          }
        }

        if (strcmp(argument.longOpt, "AXIAlignment") == 0) {
          cml->AXIAlignment = strtoul(optarg, NULL, 16);
          break;
        }

        if (strcmp(argument.longOpt, "irqTypeMask") == 0) {
          cml->irqTypeMask = strtoul(optarg, NULL, 2);
          break;
        }

        if (strcmp(argument.longOpt, "roimapFile") == 0) {
          cml->roimapFile = optarg;
          break;
        }
        if (strcmp(argument.longOpt, "nonRoiFilter") == 0) {
          cml->nonRoiFilter = optarg;
          break;
        }
        if (strcmp(argument.longOpt, "nonRoiLevel") == 0) {
          cml->nonRoiLevel = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "sramPowerdownDisable") == 0) {
          cml->sramPowerdownDisable = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "sramPowerdownMode") == 0) {
          cml->sramPowerdownMode = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "sramPowerdownTimerDiv32") == 0) {
          cml->sramPowerdownTimerDiv32 = atoi(optarg);
          break;
        }
        /*AXI max burst length */
        if (strcmp(argument.longOpt, "burstMaxLength") == 0) {
          cml->burstMaxLength = atoi(optarg);
          break;
        }

        if (strcmp(argument.longOpt, "ufbcMode") == 0) {
          cml->ufbcMode = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "ufbcYuvTrans") == 0) {
          cml->ufbcYuvTrans = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "ufbcBlockType") == 0) {
          cml->ufbcBlockType = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "ufbcBlockSplit") == 0) {
          cml->ufbcBlockSplit = atoi(optarg);
          break;
        }
	  	if (strcmp(argument.longOpt, "ufbcConstantVal") == 0) {
          cml->ufbcConstantVal = optarg;
          break;
        }
        if (strcmp(argument.longOpt, "priority") == 0) {
          cml->priority = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "core_mask") == 0) {
          cml->core_mask = atoi(optarg);
          break;
        }
        if (strcmp(argument.longOpt, "lowlatGatingDisable") == 0) {
          cml->lowlatGatingDisable = atoi(optarg);
          break;
        }

      case '3':
        if (strcmp(argument.longOpt, "useVcmd") == 0)
          cml->useVcmd = atoi(optarg);
        if (strcmp(argument.longOpt, "useDec400") == 0)
          cml->useDec400 = atoi(optarg);
        if (strcmp(argument.longOpt, "useL2Cache") == 0)
          cml->useL2Cache = atoi(optarg);
        if (strcmp(argument.longOpt, "useAXIFE") == 0)
          cml->useAXIFE = atoi(optarg);
        break;

      default:
        break;
    }
  }

  //Add for preventing invalid value
  if (cml->widthThumb > 65535 || cml->heightThumb > 65535) {  //invalid
    cml->widthThumb = 32;
    cml->heightThumb = 32;
  }

  if (cml->bitPerSecond > MAX_BITRATE) cml->bitPerSecond = MAX_BITRATE;

  if (cml->frameRateNum > 1048575) cml->frameRateNum = 1048575;

  if (cml->frameRateDenom > cml->frameRateNum)
    cml->frameRateDenom = cml->frameRateNum;

  if (cml->qLevel > 10) cml->qLevel = 10;

  // if (cml->restartInterval > 65535)
  //  cml->restartInterval = 0;
  VCEncLogInit(cml->logOutDir, cml->logOutLevel, cml->logTraceMap, 0);
  return status;
}

/*------------------------------------------------------------------------------

    ReadPic

    Read raw YUV image data from file
    Image is divided into slices, each slice consists of equal amount of
    image rows except for the bottom slice which may be smaller than the
    others. sliceNum is the number of the slice to be read
    and sliceRows is the amount of rows in each slice (or 0 for all rows).

------------------------------------------------------------------------------*/
int ReadPic(u8 *image, i32 width, i32 height, i32 sliceNum, i32 sliceRows, i32 frameNum, char *name,
            u32 inputMode, u32 input_alignment, u32 ufbcMode, u32 ufbcBlockType, u32 scan_type) {
  FILE *file = NULL;
  u64 frameSize;
  i64 frameOffset;
  u32 luma_stride, chroma_stride;
  i64 sliceLumOffset = 0;
  i64 sliceCbOffset = 0;
  i64 sliceCrOffset = 0;
  u64 sliceLumSize;    /* The size of one slice in bytes */
  u64 sliceChromaSize; /* The size of one slice in bytes */
  u64 sliceCbSize;
  u64 sliceCrSize;
  i32 sliceLumWidth; /* Picture line length to be read */
  i32 sliceCbWidth;
  i32 sliceCrWidth;
  i32 i;
  i32 lumRowBpp = 0; /* Bits per pixel for lum*/
  i32 cbRowBpp = 0;  /* Bits per pixel for cb*/
  i32 crRowBpp = 0;  /* Bits per pixel for cr */
  i32 total_mb = 0;
  i32 sliceRowsOrg = sliceRows;
  size_t size;
  u32 lumaStrideSrc, chrStrideSrc;
  u32 ufbc_header_size = 0;

  if (sliceRows == 0) sliceRows = height;

  if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32)
    width = (width + 31) & (~31);

  if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32) {
    frameSize = (i64)width * height * JpegEncGetBitsPerPixel(inputMode) / 8;
  } else if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4) {
    total_mb = (width / 16) * (height / 16);
    frameSize = (i64)total_mb / 5 * 2048 + total_mb % 5 * 400;
  } else
    getAlignedPicSizebyFormat(inputMode, (u32)width, (u32)height, 0, NULL, NULL,
                              &frameSize, scan_type);

  JpegEncGetAlignedStride(width, inputMode, &luma_stride, &chroma_stride,
                          input_alignment, scan_type);

  JpegEncGetAlignedStride(width, inputMode, &lumaStrideSrc, &chrStrideSrc, 0, scan_type);

  switch (inputMode) {
    case JPEGENC_Y8b:
      lumRowBpp = 8;
      cbRowBpp = 0;
      crRowBpp = 0;
      break;
    case JPEGENC_Y10bWL:
    case JPEGENC_Y10bWH:
    case JPENC_YUV420_10BIT_PACKED_Y0L2:
      lumRowBpp = 16;
      cbRowBpp = 0;
      crRowBpp = 0;
      break;
    case JPEGENC_YUV420_PLANAR:
    case JPEGENC_YVU420_PLANAR:
      lumRowBpp = 8;
      cbRowBpp = 4;
      crRowBpp = 4;
      break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV422SP_888:
    case JPEGENC_YVU422SP_888:
    case JPEGENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case JPEGENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
    case JPEGENC_YUV420_FBC64:
    case JPEGENC_YUV420_UV_8BIT_TILE_128_2:
      lumRowBpp = 8;
      cbRowBpp = 8;
      crRowBpp = 0;
      break;
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
      lumRowBpp = 16;
      cbRowBpp = 0;
      crRowBpp = 0;
      break;
    case JPEGENC_YUV420_I010:
      lumRowBpp = 16;
      cbRowBpp = 8;
      crRowBpp = 8;
      break;
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010:
    case JPEGENC_YUV420_PLANAR_10BIT_P010_TILE_4_4:
      lumRowBpp = 16;
      cbRowBpp = 16;
      crRowBpp = 0;
      break;
    case JPEGENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
      lumRowBpp = 10;
      cbRowBpp = 5;
      crRowBpp = 5;
      break;
    case JPEGENC_YUV400_PLANAR_10BIT_PACKED:
      lumRowBpp = 10;
      break;
    case JPEGENC_YUV420SP10b:
      lumRowBpp = 10;
      cbRowBpp = 10;
      crRowBpp = 0;
      break;
    case JPEGENC_RGB888_24BIT:
    case JPEGENC_BGR888_24BIT:
    case JPEGENC_RBG888_24BIT:
    case JPEGENC_GBR888_24BIT:
    case JPEGENC_BRG888_24BIT:
    case JPEGENC_GRB888_24BIT:
      lumRowBpp = 24;
      cbRowBpp = 0;
      crRowBpp = 0;
      break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    case JPEGENC_RGBX8888:
    case JPEGENC_BGRX8888:
    case JPEGENC_RGBX1010102:
    case JPEGENC_BGRX1010102:
    default:
      lumRowBpp = 32;
      cbRowBpp = 0;
      crRowBpp = 0;
      break;
  }

  sliceLumWidth = width * lumRowBpp / 8; /* Luma bytes per input row */
  sliceCbWidth = width * cbRowBpp / 8;   /* Cb bytes per input row */
  sliceCrWidth = width * crRowBpp / 8;
  /* Size of complete slice in input file */
  sliceLumSize = (u64)sliceLumWidth * sliceRows;
  sliceCbSize = (u64)sliceCbWidth * sliceRows / 2;
  sliceCrSize = (u64)sliceCrWidth * sliceRows / 2;

  /* Offset for frame start from start of file */
  frameOffset = frameSize * frameNum;
  /* Offset for slice luma start from start of frame */
  sliceLumOffset = sliceLumSize * sliceNum;
  /* Offset for slice cb start from start of frame */
  if (sliceCbSize)
    sliceCbOffset =
        (u64)width * height * lumRowBpp / 8 + sliceCbSize * sliceNum;
  /* Offset for slice cr start from start of frame */
  if (sliceCrSize)
    sliceCrOffset = (u64)width * height * lumRowBpp / 8 +
                    (u64)width / 2 * height / 2 * lumRowBpp / 8 +
                    sliceCrSize * sliceNum;

  /* Size of completed slice*/
  getAlignedPicSizebyFormat(inputMode, (u32)width, (u32)sliceRows,
                            input_alignment, &sliceLumSize, &sliceChromaSize,
                            NULL, scan_type);

  if (inputMode == JPEGENC_YUV420_PLANAR || inputMode == JPEGENC_YUV420_I010 ||
      inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32 ||
      inputMode == JPEGENC_YVU420_PLANAR ||
      inputMode == JPEGENC_YUV420_PLANAR_10BIT_PACKED_PLANAR) {
    sliceCbSize = sliceChromaSize / 2;
    sliceCrSize = sliceChromaSize / 2;
  } else {
    sliceCbSize = sliceChromaSize;
    sliceCrSize = 0;
  }

  /* The bottom slice may be smaller than the others */
  if (sliceRows * (sliceNum + 1) > height)
    sliceRows = height - sliceRows * sliceNum;

  if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32) {
    sliceLumOffset = (u64)width * sliceRowsOrg * sliceNum;
    sliceCbOffset = (u64)width * height + width * sliceRowsOrg * sliceNum / 2;
  } else if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4) {
    total_mb = (width / 16) * sliceRowsOrg / 16 * sliceNum;
    sliceLumOffset = total_mb / 5 * 2048 + total_mb % 5 * 400;
  } else if ((inputMode == JPEGENC_YUV422SP_888) ||
             (inputMode == JPEGENC_YVU422SP_888)) {
    sliceLumOffset = (u64)width * sliceRowsOrg * sliceNum;
    sliceCbOffset = (u64)width * height + (u64)width * sliceRowsOrg * sliceNum;
  } else if ((inputMode == JPEGENC_YUV420_8BIT_TILE_8_8) ||
             (inputMode == JPEGENC_YVU420_8BIT_TILE_8_8) ||
             (inputMode == JPEGENC_YUV420_10BIT_TILE_8_8)) {
    sliceLumOffset = (u64)lumaStrideSrc * (sliceRowsOrg / 8) * sliceNum;
    sliceCbOffset = (u64)lumaStrideSrc * ((height + 7) / 8) +
                    chrStrideSrc * (((sliceRowsOrg / 2) + 3) / 4) * sliceNum;
  } else if (inputMode == JPEGENC_YUV420_FBC64 ||
             inputMode == JPEGENC_YUV420_UV_8BIT_TILE_128_2 ||
             inputMode == JPEGENC_YUV420_UV_10BIT_TILE_128_2) {
    sliceLumOffset = (u64)lumaStrideSrc * (sliceRowsOrg / 2) * sliceNum;
    sliceCbOffset = (u64)lumaStrideSrc * ((height + 1) / 2) +
                    chrStrideSrc * (((sliceRowsOrg / 2) + 1) / 2) * sliceNum;
  } else if (inputMode == JPEGENC_YUV420_SEMIPLANAR_101010) {
    sliceLumOffset = (u64)lumaStrideSrc * sliceRowsOrg * sliceNum;
    sliceCbOffset = (u64)lumaStrideSrc * height +
                    (u64)chrStrideSrc * sliceRowsOrg * sliceNum;
  } else if (inputMode == JPEGENC_RGB888 ||
             inputMode == JPEGENC_BGR888 ||
             inputMode == JPEGENC_RGBX8888 ||
             inputMode == JPEGENC_BGRX8888 ||
             inputMode == JPEGENC_RGB101010 ||
             inputMode == JPEGENC_BGR101010 ||
             inputMode == JPEGENC_RGBX1010102 ||
             inputMode == JPEGENC_BGRX1010102 ||
             inputMode == JPEGENC_RGB565 ||
             inputMode == JPEGENC_BGR565 ||
             inputMode == JPEGENC_RGB555 ||
             inputMode == JPEGENC_BGR555 ||
             inputMode == JPEGENC_RGB444 ||
             inputMode == JPEGENC_BGR444) {
    if (scan_type == 1) {
      sliceLumOffset = (u64)lumaStrideSrc * (sliceRowsOrg / 64) * sliceNum;
    }
  }

  if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_32_32) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    size = fread(image, 1, width * sliceRows, file);
    fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
    size = fread(image + width * sliceRows, 1, width * sliceRows / 2, file);
    goto error;
  } else if (inputMode == JPEGENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    size = fread(image, 1, width * sliceRows * 2 * 3 / 2, file);
    goto end;
  } else if (inputMode == JPEGENC_YUV420_8BIT_TILE_8_8 ||
             inputMode == JPEGENC_YVU420_8BIT_TILE_8_8 ||
             inputMode == JPEGENC_YUV420_10BIT_TILE_8_8) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }

    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    for (i = 0; i < ((sliceRows + 7) / 8); i++)
      size = fread(image + i * luma_stride, 1, lumaStrideSrc, file);

    sliceLumSize = luma_stride * ((sliceRows + 7) / 8);

    //chroma
    fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
    for (i = 0; i < (((sliceRows / 2) + 3) / 4); i++)
      size = fread(image + sliceLumSize + i * chroma_stride, 1, chrStrideSrc,
                   file);

    goto end;
  } else if (inputMode == JPEGENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4 ||
             inputMode == JPEGENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4 ||
             inputMode == JPEGENC_YUV420_PLANAR_10BIT_P010_TILE_4_4) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }

    /* Luma */
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    for (i = 0; i < ((sliceRows + 3) / 4); i++)
      size = fread(image + i * luma_stride, 1, lumaStrideSrc, file);

    sliceLumSize = (u64)luma_stride * ((sliceRows + 3) / 4);

    /* Chroma */
    fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
    for (i = 0; i < (((sliceRows / 2) + 3) / 4); i++)
      size = fread(image + sliceLumSize + i * chroma_stride, 1, chrStrideSrc,
                   file);

    goto end;
  } else if (inputMode == JPEGENC_YVU420_PLANAR) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);

    i64 scan = luma_stride;

    for (i = 0; i < sliceRows; i++)
      size = fread(image + scan * i, 1, sliceLumWidth, file);

    //cr
    if (sliceCrSize) {
      fseek(file, frameOffset + sliceCbOffset, SEEK_SET);

      scan = chroma_stride;

      for (i = 0; i < sliceRows / 2; i++)
        size = fread(image + sliceLumSize + sliceCrSize + scan * i, 1,
                     sliceCrWidth, file);
    }

    //cb
    if (sliceCbSize) {
      fseek(file, frameOffset + sliceCrOffset, SEEK_SET);

      scan = chroma_stride;

      for (i = 0; i < sliceRows / 2; i++)
        size = fread(image + sliceLumSize + scan * i, 1, sliceCbWidth, file);
    }
    goto end;
  } else if (inputMode == JPEGENC_YUV420_FBC64 ||
             inputMode == JPEGENC_YUV420_UV_8BIT_TILE_128_2 ||
             inputMode == JPEGENC_YUV420_UV_10BIT_TILE_128_2) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }

    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    for (i = 0; i < ((sliceRows + 1) / 2); i++)
      size = fread(image + i * luma_stride, 1, lumaStrideSrc, file);

    sliceLumSize = (u64)luma_stride * ((sliceRows + 1) / 2);

    /* Chroma */
    fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
    for (i = 0; i < (((sliceRows / 2) + 1) / 2); i++)
      size = fread(image + sliceLumSize + i * chroma_stride, 1, chrStrideSrc,
                   file);

    goto end;
  } else if (inputMode == JPEGENC_YUV420_SEMIPLANAR_101010) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }

    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    for (i = 0; i < sliceRows; i++)
      size = fread(image + i * luma_stride, 1, lumaStrideSrc, file);

    sliceLumSize = (u64)luma_stride * sliceRows;

    /* Chroma */
    fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
    for (i = 0; i < sliceRows / 2; i++)
      size = fread(image + sliceLumSize + i * chroma_stride, 1, chrStrideSrc,
                   file);

    goto end;
  } else if (inputMode == JPEGENC_Y8b ||
             inputMode == JPEGENC_Y10bWL ||
             inputMode == JPEGENC_Y10bWH ) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }

    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    for (i = 0; i < sliceRows; i++)
      size = fread(image + i * luma_stride, 1, lumaStrideSrc, file);

    sliceLumSize = (u64)luma_stride * sliceRows;
    goto end;
  } else if(inputMode == JPEGENC_YUV420_PLANAR_10BIT_PACKED_PLANAR) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    i64 scan = luma_stride * 10 / 8;
    for (i = 0; i < sliceRows; i++)
      size = fread(image + scan * i, 1, sliceLumWidth, file);

    //cb
    if (sliceCbSize) {
      fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
      scan = chroma_stride * 10 / 8;
      for (i = 0; i < sliceRows / 2; i++)
        size = fread(image + sliceLumSize + scan * i, 1, sliceCbWidth, file);
    }

    //cr
    if (sliceCrSize) {
      fseek(file, frameOffset + sliceCrOffset, SEEK_SET);
      scan = chroma_stride * 10 / 8;
      for (i = 0; i < sliceRows / 2; i++)
        size = fread(image + sliceLumSize + sliceCbSize + scan * i, 1,
                     sliceCrWidth, file);
    }
    goto end;
  } else if(inputMode == JPEGENC_YUV400_PLANAR_10BIT_PACKED) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    i64 scan = luma_stride * 10 / 8;
    for (i = 0; i < sliceRows; i++)
      size = fread(image + scan * i, 1, sliceLumWidth, file);

    goto end;
  } else if(inputMode == JPEGENC_YUV420SP10b) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    //luma
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    i64 scan = luma_stride * 10 / 8;
    for (i = 0; i < sliceRows; i++)
      size = fread(image + scan * i, 1, sliceLumWidth, file);

    //cb
    if (sliceCbSize) {
      fseek(file, frameOffset + sliceCbOffset, SEEK_SET);
      scan = chroma_stride * 10 / 8;
      for (i = 0; i < sliceRows / 2; i++)
        size = fread(image + sliceLumSize + scan * i, 1, sliceCbWidth, file);
    }
    goto end;
  } else if (inputMode == JPEGENC_RGB888 ||
             inputMode == JPEGENC_BGR888 ||
             inputMode == JPEGENC_RGBX8888 ||
             inputMode == JPEGENC_BGRX8888 ||
             inputMode == JPEGENC_RGB101010 ||
             inputMode == JPEGENC_BGR101010 ||
             inputMode == JPEGENC_RGBX1010102 ||
             inputMode == JPEGENC_BGRX1010102 ||
             inputMode == JPEGENC_RGB565 ||
             inputMode == JPEGENC_BGR565 ||
             inputMode == JPEGENC_RGB555 ||
             inputMode == JPEGENC_BGR555 ||
             inputMode == JPEGENC_RGB444 ||
             inputMode == JPEGENC_BGR444) {
    if (scan_type == 1) {
      file = fopen(name, "rb");
      if (file == NULL) {
        fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
        return -1;
      }
      //luma
      fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
      for (i = 0; i < ((sliceRows + 63) / 64); i++)
        size = fread(image + i * luma_stride, 1, lumaStrideSrc, file);
      sliceLumSize = luma_stride * ((sliceRows + 63) / 64);

      goto end;
    }
  } else if (inputMode == JPENC_YUV420_10BIT_PACKED_Y0L2) {
    file = fopen(name, "rb");
    if (file == NULL) {
      fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
      return -1;
    }
    fseek(file, frameOffset + sliceLumOffset, SEEK_SET);
    for (i = 0; i < (sliceRows / 2); i++)
      size = fread(image + i * luma_stride * 2 * 2, 1, lumaStrideSrc*4, file);
    sliceLumSize = (u64)luma_stride * sliceRows * 2;
    goto end;
  }

  /* Read input from file frame by frame */
#ifndef ASIC_WAVE_TRACE_TRIGGER
  u32 byteSize = (u32)(sliceLumWidth * sliceRows + sliceCbWidth * sliceRows / 2 + sliceCrWidth * sliceRows / 2);
  printf("Reading frame %d slice %d (%u bytes) from %s... ", frameNum, sliceNum, byteSize, name);
  fflush(stdout);
#endif

  file = fopen(name, "rb");
  if (file == NULL) {
    fprintf(stderr, "\nUnable to open VOP file: %s\n", name);
    return -1;
  }

#ifdef SUPPORT_UFBC
#ifndef SYSTEM_BUILD
  if(ufbcMode && sliceNum == 0) {
    u32 asic_format = 0;
    EncUfbcGetSize(inputMode, ufbcBlockType, (u32)width, (u32)height,
      ufbcMode, &asic_format, &ufbc_header_size, &frameSize);
    frameOffset = (frameSize + ufbc_header_size) * frameNum;
    fseek(file, frameOffset, SEEK_SET);
    fread(image, 1, ufbc_header_size + frameSize, file);
    goto end;
  }
#endif
#endif

  //luma
  fseek(file, frameOffset + sliceLumOffset, SEEK_SET);

  i64 scan = luma_stride;

  for (i = 0; i < sliceRows; i++)
    size = fread(image + scan * i, 1, sliceLumWidth, file);

  //cb
  if (sliceCbSize) {
    fseek(file, frameOffset + sliceCbOffset, SEEK_SET);

    scan = chroma_stride;

    if ((inputMode == JPEGENC_YUV422SP_888) ||
        (inputMode == JPEGENC_YVU422SP_888)) {
      for (i = 0; i < sliceRows; i++)
        size = fread(image + sliceLumSize + scan * i, 1, sliceCbWidth, file);
    } else {
      for (i = 0; i < sliceRows / 2; i++)
        size = fread(image + sliceLumSize + scan * i, 1, sliceCbWidth, file);
    }
  }

  //cr
  if (sliceCrSize) {
    fseek(file, frameOffset + sliceCrOffset, SEEK_SET);

    scan = chroma_stride;

    for (i = 0; i < sliceRows / 2; i++)
      size = fread(image + sliceLumSize + sliceCbSize + scan * i, 1,
                   sliceCrWidth, file);
  }
error:
  /* Stop if last VOP of the file */
  if (feof(file)) {
    fprintf(stderr, "\nI can't read VOP no: %d ", frameNum);
    fprintf(stderr, "from file: %s\n", name);
    fclose(file);
    return -1;
  }
end:

#ifndef ASIC_WAVE_TRACE_TRIGGER
  printf("OK\n");
  fflush(stdout);
#endif

  fclose(file);

  return 0;
}
i32 JpegReadDEC400Data(JpegEncInst encoder, u8 *compDataBuf, u8 *compTblBuf,
                       u32 inputFormat, u32 src_width, u32 src_height,
                       char *inputDataFile, FILE *dec400Table, i32 num,
                       i32 sliceNum, i32 sliceRows, u32 alignment,
					   u32 scanType, u32 dec400Enable, u32 dec400TSHeaderEnable) {
  u64 seek, seek_frame;
  u8 *lum;
  u8 *cb;
  u8 *cr;
  u64 lumaSize, chrSize;
  u64 src_img_size;
  u64 sliceLumOffset, sliceCbOffset, sliceCrOffset;
  u64 SrcLumaSize = 0, SrcChrSize = 0;
  u32 dec400LumaTblSize = 0, dec400ChrTblSize = 0, dec400FrameTableSize = 0;
  u64 pictureSize = 0;
  u64 SrcLumaSize_slice, dec400LumaTblSize_slice, SrcChrSize_slice,
      dec400ChrTblSize_slice;
  int ret = NOK;

  FILE *dec400Data = NULL;
  dec400Data = fopen(inputDataFile, "rb");
  if (dec400Data == NULL) return NOK;
  JpegGetLumaSize(encoder, &SrcLumaSize_slice, &dec400LumaTblSize_slice);
  JpegGetChromaSize(encoder, &SrcChrSize_slice, &dec400ChrTblSize_slice);
  getAlignedPicSizebyFormat(inputFormat, src_width, src_height,
                            alignment, &SrcLumaSize, &SrcChrSize,
                            &pictureSize, scanType);
  EncGetDec400TsBufferSize(inputFormat, src_width, src_height, alignment,
    &dec400LumaTblSize, &dec400ChrTblSize, &dec400FrameTableSize,
	scanType, dec400Enable, dec400TSHeaderEnable);

  /* Offset for slice luma start from start of frame */
  sliceLumOffset = dec400LumaTblSize_slice * sliceNum;
  if (inputFormat == JPEGENC_YUV420_PLANAR || inputFormat == JPEGENC_YUV420_I010) {
    /* Offset for slice cb start from start of frame */
    sliceCbOffset = dec400LumaTblSize + dec400ChrTblSize_slice / 2 * sliceNum;
    /* Offset for slice cr start from start of frame */
    sliceCrOffset = dec400LumaTblSize + dec400ChrTblSize / 2 +
                      dec400ChrTblSize_slice / 2 * sliceNum;
  } else {
    /* Offset for slice cb start from start of frame */
    sliceCbOffset = dec400LumaTblSize + dec400ChrTblSize_slice * sliceNum;
    /* Offset for slice cr start from start of frame */
    sliceCrOffset = dec400LumaTblSize + dec400ChrTblSize / 2 +
                      dec400ChrTblSize_slice * sliceNum;
  }
  lumaSize = dec400LumaTblSize_slice;
  chrSize = dec400ChrTblSize_slice;
  if (dec400TSHeaderEnable) {
    dec400Table = dec400Data;
    src_img_size = dec400FrameTableSize + SrcLumaSize + SrcChrSize;
  } else {
    src_img_size = dec400FrameTableSize;
  }
  seek_frame = ((u64)num) * ((u64)src_img_size);
  lum = compTblBuf;
  cb = lum + lumaSize;
  cr = cb + chrSize / 2;
  switch (inputFormat) {
    case JPEGENC_YUV420_PLANAR:
      seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Table, lum, seek, lumaSize)) goto return_;
      seek = seek_frame + sliceCbOffset;
      if (file_read(dec400Table, cb, seek, chrSize / 2)) goto return_;
      seek = seek_frame + sliceCrOffset;
      if (file_read(dec400Table, cr, seek, chrSize / 2)) goto return_;
      break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010:
    case JPEGENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case JPEGENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
    case JPEGENC_YUV420_PLANAR_10BIT_P010_TILE_4_4:
    case JPEGENC_YUV420_8BIT_TILE_8_8:
    case JPEGENC_YVU420_8BIT_TILE_8_8:
    case JPEGENC_YUV420_10BIT_TILE_8_8:
	  seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Table, lum, seek, lumaSize)) goto return_;
      seek = seek_frame + sliceCbOffset;
      if (file_read(dec400Table, cb, seek, chrSize)) goto return_;
      break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    case JPEGENC_RGBX8888:
    case JPEGENC_BGRX8888:
    case JPEGENC_RGBX1010102:
    case JPEGENC_BGRX1010102:
    case JPEGENC_YUV422_INTERLEAVED_YUYV:
    case JPEGENC_Y8b:
    case JPEGENC_RGB565:
    case JPEGENC_BGR565:
    case JPEGENC_RGB555:
    case JPEGENC_BGR555:
    case JPEGENC_RGB444:
    case JPEGENC_BGR444:
	seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Table, lum, seek, lumaSize)) goto return_;
      break;
    case JPEGENC_YVU420_PLANAR:
      seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Table, lum, seek, lumaSize)) goto return_;
      seek = seek_frame + sliceCbOffset;
      if (file_read(dec400Table, cr, seek, chrSize / 2)) goto return_;
      seek = seek_frame + sliceCrOffset;
      if (file_read(dec400Table, cb, seek, chrSize / 2)) goto return_;
      break;
    default:
      printf("DEC400 not support this format\n");
      goto return_;
  }

  lumaSize = SrcLumaSize_slice;
  chrSize = SrcChrSize_slice;
  if (dec400TSHeaderEnable) {
    src_img_size = SrcLumaSize + SrcChrSize + dec400FrameTableSize;
    seek = ((u64)num) * ((u64)src_img_size) + dec400FrameTableSize;
  } else {
  src_img_size = SrcLumaSize + SrcChrSize;
  seek_frame = ((u64)num) * ((u64)src_img_size);
  }
  lum = compDataBuf;
  cb = lum + lumaSize;
  cr = cb + chrSize / 2;
  /* Offset for slice luma start from start of frame */
  sliceLumOffset = SrcLumaSize_slice * sliceNum;
  if (inputFormat == JPEGENC_YUV420_PLANAR || inputFormat == JPEGENC_YUV420_I010) {
    /* Offset for slice cb start from start of frame */
    sliceCbOffset = SrcLumaSize + SrcChrSize_slice / 2 * sliceNum;
    /* Offset for slice cr start from start of frame */
    sliceCrOffset = SrcLumaSize + SrcChrSize / 2 +
                      SrcChrSize_slice / 2 * sliceNum;
  } else {
    /* Offset for slice cb start from start of frame */
    sliceCbOffset = SrcLumaSize + SrcChrSize_slice * sliceNum;
    /* Offset for slice cr start from start of frame */
    sliceCrOffset = SrcLumaSize + SrcChrSize / 2 +
                      SrcChrSize_slice * sliceNum;
  }

  switch (inputFormat) {
    case JPEGENC_YUV420_PLANAR:
      seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Data, lum, seek, lumaSize)) goto return_;
      seek = seek_frame + sliceCbOffset;
      if (file_read(dec400Data, cb, seek, chrSize / 2)) goto return_;
      seek = seek_frame + sliceCrOffset;
      if (file_read(dec400Data, cr, seek, chrSize / 2)) goto return_;
      break;
    case JPEGENC_YUV420_SEMIPLANAR:
    case JPEGENC_YUV420_SEMIPLANAR_VU:
    case JPEGENC_YUV420_MS_P010:
    case JPEGENC_YVU420_PLANAR_10BIT_P010:
    case JPEGENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case JPEGENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
    case JPEGENC_YUV420_PLANAR_10BIT_P010_TILE_4_4:
    case JPEGENC_YUV420_8BIT_TILE_8_8:
    case JPEGENC_YVU420_8BIT_TILE_8_8:
    case JPEGENC_YUV420_10BIT_TILE_8_8:
      seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Data, lum, seek, lumaSize)) goto return_;
      seek = seek_frame + sliceCbOffset;
      if (file_read(dec400Data, cb, seek, chrSize)) goto return_;
      break;
    case JPEGENC_RGB888:
    case JPEGENC_BGR888:
    case JPEGENC_RGB101010:
    case JPEGENC_BGR101010:
    case JPEGENC_RGBX8888:
    case JPEGENC_BGRX8888:
    case JPEGENC_RGBX1010102:
    case JPEGENC_BGRX1010102:
    case JPEGENC_YUV422_INTERLEAVED_YUYV:
    case JPEGENC_Y8b:
    case JPEGENC_Y10bWL:
    case JPEGENC_Y10bWH:
    case JPEGENC_RGB565:
    case JPEGENC_BGR565:
    case JPEGENC_RGB555:
    case JPEGENC_BGR555:
    case JPEGENC_RGB444:
    case JPEGENC_BGR444:
	  seek = seek_frame + sliceLumOffset;
      if (file_read(dec400Data, lum, seek, lumaSize)) goto return_;
      break;
    default:
      printf("DEC400 not support this format\n");
      goto return_;
  }
  ret = OK;
return_:
  fclose(dec400Data);
  return ret;
}

/*------------------------------------------------------------------------------

    Help

------------------------------------------------------------------------------*/
void Help(void) {
  char helptext[] = {
#include "../jpgenc_help.dat"
      , '\0'};

  fprintf(stdout, "Usage:  %s [options] -i inputfile\n", "jpeg_testenc");
  fprintf(stdout, "%s", helptext);
}

/*------------------------------------------------------------------------------

    Write encoded stream to file

------------------------------------------------------------------------------*/
void WriteStrm(FILE *fout, u32 *strmbuf, u32 size, u32 endian) {
  /* Swap the stream endianess before writing to file if needed */
  if (endian == 1) {
    u32 i = 0, words = (size + 3) / 4;

    while (words) {
      u32 val = strmbuf[i];
      u32 tmp = 0;

      tmp |= (val & 0xFF) << 24;
      tmp |= (val & 0xFF00) << 8;
      tmp |= (val & 0xFF0000) >> 8;
      tmp |= (val & 0xFF000000) >> 24;
      strmbuf[i] = tmp;
      words--;
      i++;
    }
  }

  /* Write the stream to file */

#ifndef ASIC_WAVE_TRACE_TRIGGER
  printf("Writing stream (%u bytes)... ", size);
  fflush(stdout);
#endif

  fwrite(strmbuf, 1, size, fout);

#ifndef ASIC_WAVE_TRACE_TRIGGER
  printf("OK\n");
  fflush(stdout);
#endif
}

/*------------------------------------------------------------------------------

    WriteStrmBufs
        Write encoded stream to file

    Params:
        fout - file to write
        bufs - stream buffers
        offset - stream buffer offset
        size - amount of data to write
        endian - data endianess, big or little

------------------------------------------------------------------------------*/
void writeStrmBufs(FILE *fout, EWLLinearMem_t *bufs, u32 offset, u32 size,
                   u32 invalid_size, u32 endian) {
  u8 *buf0 = (u8 *)bufs[0].virtualAddress;
  u8 *buf1 = (u8 *)bufs[1].virtualAddress;
  u32 buf0Len = bufs[0].size;

  if (!buf0) return;

  if (offset < buf0Len) {
    u32 size0 = MIN(size, buf0Len - offset - invalid_size);
    WriteStrm(fout, (u32 *)(buf0 + offset), size0, endian);
    if ((size0 < size) && buf1)
      WriteStrm(fout, (u32 *)buf1, size - size0, endian);
  } else if (buf1) {
    WriteStrm(fout, (u32 *)(buf1 + offset - buf0Len), size, endian);
  }
}

#ifdef SEPARATE_FRAME_OUTPUT
/*------------------------------------------------------------------------------

    Write encoded frame to file

------------------------------------------------------------------------------*/
void WriteFrame(char *filename, u32 *strmbuf, u32 size) {
  FILE *fp;

  fp = fopen(filename, "ab");

  if (fp) {
    fwrite(strmbuf, 1, size, fp);
    fclose(fp);
  }
}

/*------------------------------------------------------------------------------

    WriteFrameBufs
        Write encoded stream to file

    Params:
        bufs - stream buffers
        offset - stream buffer offset
        size - amount of data to write

------------------------------------------------------------------------------*/
void writeFrameBufs(char *filename, EWLLinearMem_t *bufs, u32 offset,
                    u32 size) {
  u8 *buf0 = (u8 *)bufs[0].virtualAddress;
  u8 *buf1 = (u8 *)bufs[1].virtualAddress;
  u32 buf0Len = bufs[0].size;

  if (!buf0) return;

  if (offset < buf0Len) {
    u32 size0 = MIN(size, buf0Len - offset);
    WriteFrame(filename, (u32 *)(buf0 + offset), size0);
    if ((size0 < size) && buf1) WriteFrame(filename, (u32 *)buf1, size - size0);
  } else if (buf1) {
    WriteFrame(filename, (u32 *)(buf1 + offset - buf0Len), size);
  }
}

#endif

/*------------------------------------------------------------------------------
    GetResolution
        Parse image resolution from file name
------------------------------------------------------------------------------*/
u32 GetResolution(char *filename, i32 *pWidth, i32 *pHeight) {
  i32 i;
  u32 w, h;
  u32 len = strlen(filename);
  i32 filenameBegin = 0;

  if (len < 4) return 1;

  /* Find last '/' in the file name, it marks the beginning of file name */
  for (i = len - 1; i > 0; --i)
    if (filename[i] == '/') {
      filenameBegin = i + 1;
      break;
    }

  /* If '/' found, it separates trailing path from file name */
  for (i = filenameBegin; i <= len - 3; ++i) {
    if ((strncmp(filename + i, "subqcif", 7) == 0) ||
        (strncmp(filename + i, "sqcif", 5) == 0)) {
      *pWidth = 128;
      *pHeight = 96;
      printf("Detected resolution SubQCIF (128x96) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "qcif", 4) == 0) {
      *pWidth = 176;
      *pHeight = 144;
      printf("Detected resolution QCIF (176x144) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "4cif", 4) == 0) {
      *pWidth = 704;
      *pHeight = 576;
      printf("Detected resolution 4CIF (704x576) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "cif", 3) == 0) {
      *pWidth = 352;
      *pHeight = 288;
      printf("Detected resolution CIF (352x288) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "qqvga", 5) == 0) {
      *pWidth = 160;
      *pHeight = 120;
      printf("Detected resolution QQVGA (160x120) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "qvga", 4) == 0) {
      *pWidth = 320;
      *pHeight = 240;
      printf("Detected resolution QVGA (320x240) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "vga", 3) == 0) {
      *pWidth = 640;
      *pHeight = 480;
      printf("Detected resolution VGA (640x480) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "720p", 4) == 0) {
      *pWidth = 1280;
      *pHeight = 720;
      printf("Detected resolution 720p (1280x720) from file name.\n");
      return 0;
    }
    if (strncmp(filename + i, "1080p", 5) == 0) {
      *pWidth = 1920;
      *pHeight = 1080;
      printf("Detected resolution 1080p (1920x1080) from file name.\n");
      return 0;
    }
    if (filename[i] == 'x') {
      if (sscanf(filename + i - 4, "%ux%u", &w, &h) == 2) {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %ux%u from file name.\n", w, h);
        return 0;
      } else if (sscanf(filename + i - 3, "%ux%u", &w, &h) == 2) {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %ux%u from file name.\n", w, h);
        return 0;
      } else if (sscanf(filename + i - 2, "%ux%u", &w, &h) == 2) {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %ux%u from file name.\n", w, h);
        return 0;
      }
    }
    if (filename[i] == 'w') {
      if (sscanf(filename + i, "w%uh%u", &w, &h) == 2) {
        *pWidth = w;
        *pHeight = h;
        printf("Detected resolution %ux%u from file name.\n", w, h);
        return 0;
      }
    }
  }

  return 1; /* Error - no resolution found */
}

/*------------------------------------------------------------------------------

    API tracing

------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

    InitInputLineBuffer
    -get address of input line buffer

------------------------------------------------------------------------------*/
i32 InitInputLineBuffer(inputLineBufferCfg *lineBufCfg, JpegEncCfg *encCfg,
                        JpegEncIn *encIn, JpegEncInst inst) {
  u32 luma_stride, chroma_stride;
  JpegEncGetAlignedStride(encCfg->inputWidth, encCfg->frameType, &luma_stride,
                          &chroma_stride, 1 << encCfg->exp_of_input_alignment, encCfg->scanType);
  memset(lineBufCfg, 0, sizeof(inputLineBufferCfg));
  lineBufCfg->inst = (void *)inst;
  //lineBufCfg->asic   = &(((jpegInstance_s *)inst)->asic);
  lineBufCfg->wrCnt = 0;
  lineBufCfg->depth = (JpegGetSbiSupport(inst) && encCfg->inputLineBufHwModeEn) ? 1 : encCfg->inputLineBufDepth;
  lineBufCfg->inputFormat = encCfg->frameType;
  lineBufCfg->lumaStride = luma_stride;
  lineBufCfg->chromaStride = chroma_stride;
  lineBufCfg->encWidth = encCfg->codingWidth;
  lineBufCfg->encHeight = encCfg->codingHeight;
  lineBufCfg->hwHandShake = encCfg->inputLineBufHwModeEn;
  lineBufCfg->loopBackEn = encCfg->inputLineBufLoopBackEn;
  lineBufCfg->amountPerLoopBack = encCfg->amountPerLoopBack;
  lineBufCfg->srcHeight =
    encCfg->codingType ? encCfg->restartInterval * 16 : encCfg->inputHeight;
  lineBufCfg->srcVerOffset = encCfg->codingType ? 0 : encCfg->yOffset;
  lineBufCfg->getMbLines = &JpegEncGetEncodedMbLines;
  lineBufCfg->setMbLines = &JpegEncSetInputMBLines;
  lineBufCfg->ctbSize =
    JpegGetSbiSupport(inst) ? encCfg->segmentUnitHeight
                            : (encCfg->losslessEn
                              ? (encCfg->codingMode == JPEGENC_MONO_MODE ? 1 : 2)
                              : (encCfg->codingMode == JPEGENC_420_MODE) ? 16 : 8);
    //Ljpeg lumaOnly: 1 lines per mcu;other: 2 lines per mcu
  lineBufCfg->lumSrc = (u8 *)encIn->pLum;
  lineBufCfg->cbSrc = (u8 *)encIn->pCb;
  lineBufCfg->crSrc = (u8 *)encIn->pCr;

  if (encIn->dec400TablepLum) {
    lineBufCfg->decTileSize = 256;
    lineBufCfg->alignment = 1 << encCfg->exp_of_input_alignment;
    lineBufCfg->lumDecTblSrc = (u8*)encIn->dec400TablepLum;
    lineBufCfg->cbDecTblSrc = (u8*)encIn->dec400TablepCb;
    lineBufCfg->crDecTblSrc = (u8*)encIn->dec400TablepCr;
  }
  lineBufCfg->initSegNum = 0;
  lineBufCfg->client_type = EWL_CLIENT_TYPE_JPEG_ENC;

  if (VCEncInitInputLineBuffer(lineBufCfg)) return -1;

  /* loopback mode */
  if (lineBufCfg->loopBackEn && lineBufCfg->lumBuf.buf) {
    encIn->busLum = lineBufCfg->lumBuf.busAddress;
    encIn->busCb = lineBufCfg->cbBuf.busAddress;
    encIn->busCr = lineBufCfg->crBuf.busAddress;
#ifdef SUPPORT_UFBC
    if (encIn->dec400TablepLum && encCfg->ufbcParam.mode) {
      encIn->dec400TableBusLum = lineBufCfg->decTblLumBuf.busAddress;
      encIn->dec400TableBusCb = lineBufCfg->decTblCbBuf.busAddress;
      encIn->dec400TableBusCr = lineBufCfg->decTblCrBuf.busAddress;
    }
#endif
    /* data in SRAM start from the line to be encoded*/
    if (encCfg->codingType == JPEGENC_WHOLE_FRAME) encCfg->yOffset = 0;
  }

  return 0;
}

/*------------------------------------------------------------------------------

    SetInputLineBuffer
    -setup inputLineBufferCfg
    -initialize line buffer

------------------------------------------------------------------------------*/
void SetInputLineBuffer(inputLineBufferCfg *lineBufCfg, JpegEncCfg *encCfg,
                        JpegEncIn *encIn, JpegEncInst inst, i32 sliceIdx) {
  if (encCfg->codingType == JPEGENC_SLICED_FRAME) {
    i32 h = encCfg->codingHeight + encCfg->yOffset;
    i32 sliceRows = encCfg->restartInterval * 16;
    i32 rows = sliceIdx * sliceRows;
    if ((rows + sliceRows) <= h)
      lineBufCfg->encHeight = sliceRows;
    else
      lineBufCfg->encHeight = h % sliceRows;
  }
  lineBufCfg->wrCnt = 0;
  encIn->lineBufWrCnt = VCEncStartInputLineBuffer(lineBufCfg, HANTRO_FALSE);
  encIn->initSegNum = lineBufCfg->initSegNum;
  return;
}

i32 InitStreamSegmentCrl(SegmentCtl_s *ctl, commandLine_s *cml, FILE *out,
                         JpegEncIn *encIn) {
  ctl->streamRDCounter = 0;
  ctl->streamMultiSegEn = cml->streamMultiSegmentMode != 0;
  ctl->streamBase = (u8 *)encIn->pOutBuf[0];
  if (cml->streamMultiSegmentMode == 3) {
    ctl->segmentSize = cml->streamMultiSegmentSize;
    ctl->segmentAmount = encIn->outBufSize[0] / cml->streamMultiSegmentSize;
  } else {
    ctl->segmentSize = encIn->outBufSize[0] / cml->streamMultiSegmentAmount;
    ctl->segmentSize = ((ctl->segmentSize + 16 - 1) &
                        (~(16 - 1)));  //segment size must be aligned to 16byte
    ctl->segmentAmount = cml->streamMultiSegmentAmount;
  }
  // if((ctl->segmentAmount < 2) || (ctl->segmentSize < 512)){
  //   return -1;
  // }
  ctl->outStreamFile = out;
  return 0;
}

/* Callback function called by the encoder SW after "segment ready"
    interrupt from HW. Note that this function is called after every segment is ready.
------------------------------------------------------------------------------*/
void EncStreamSegmentReady(void *cb_data) {
  u8 *streamBase;
  SegmentCtl_s *ctl = (SegmentCtl_s *)cb_data;

  if (ctl->streamMultiSegEn) {
    streamBase = ctl->streamBase +
                 (ctl->streamRDCounter % ctl->segmentAmount) * ctl->segmentSize;

    printf("<----receive segment irq:length:addr =  %u : %u : %10p\n",
           ctl->streamRDCounter, ctl->segmentSize, (void *)streamBase);
    if (writeOutput)
      WriteStrm(ctl->outStreamFile, (u32 *)streamBase, ctl->segmentSize, 0);

    ctl->streamRDCounter++;
  }
}

i8 CmlLog(char *p_argv, commandLine_s *cmdl) {
  int cmdl_all_log_size;
  cmdl_all_log_size = LOGBUFFER * sizeof(char);
  cmdl_all_log = malloc(cmdl_all_log_size);
  if (!cmdl_all_log) return NOK;
  memset(cmdl_all_log, 0, cmdl_all_log_size);
  if (CmlPrint(options, cmdl, cmdl_all_log, cmdl_all_log_size) == NOK)
    return NOK;
  else {
    CMLTRACE(" %s %s", p_argv, cmdl_all_log);
    free(cmdl_all_log);
    return OK;
  }
}

static u32 SetDec400Enable(commandLine_s *cmdl) {
  u32 dec400Enable = 0;
  dec400Enable = cmdl->dec400TileSize << 8 | cmdl->dec400TileMode << 16 |
    cmdl->dec400DataAlignment << 29 | cmdl->dec400RGBAX << 28 |
    cmdl->dec400RGBFormat << 25;
  return dec400Enable;
}
