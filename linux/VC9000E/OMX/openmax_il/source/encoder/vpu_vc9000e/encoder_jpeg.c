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

#include "encoder_jpeg.h"
#include "enc_image_priv.h"
#include "util.h"
#include "jpegencapi.h"
#include "OSAL.h"
#include <string.h>
#include "dbgtrace.h"

#undef DBGT_PREFIX
#define DBGT_PREFIX "OMX JPEG"

#if !defined (ENCVC9000E)
#error "SPECIFY AN ENCODER PRODUCT (ENCVC9000E) IN COMPILER DEFINES!"
#endif

VCE_JPEG_OPTIONS vce_default_opts_jpeg = {
  .width = DEFAULT,
  .height = DEFAULT,
  .lumWidthSrc = DEFAULT,
  .lumHeightSrc = DEFAULT,
  .horOffsetSrc = 0,
  .verOffsetSrc = 0,
  .restartInterval = 0,
  .frameType = JPEGENC_YUV420_PLANAR,
  .colorConversion = JPEGENC_RGBTOYUV_BT601,
  .rotation = 0,
  .partialCoding = 0,
  .codingMode = 0,
  .markerType = 0,
  .qLevel = 1,
  .quality = -1,
  .nonRoiLevel = 10,
  .unitsType = 0,
  .xdensity = 1,
  .ydensity = 1,
  .thumbnail = 0,
  .widthThumb = 32,
  .heightThumb = 32,
  .comLength = 0,
  .pCom = NULL,
  .inputLineBufMode = 0,
  .inputLineBufDepth = 1,
  .amountPerLoopBack = 0,
  .segmentUnitHeight = 16,
  .hashtype = 0,
  .mirror = 0,
  .constChromaEn = 0,
  .constCb = 0x80,
  .constCr = 0x80,
  .predictMode = 0,
  .ptransValue = 0,
  .bitPerSecond = 0,
  .frameRateNum = 30,
  .frameRateDenom = 1,
  .rcMode = 1,
  .picQpDeltaMin = -2,
  .picQpDeltaMax = 3,
  .qpmin = 0,
  .qpmax = 51,
  .fixedQP = -1,
  .exp_of_input_alignment = 0,//7,
  .streamMultiSegmentMode = 0,
  .streamMultiSegmentAmount = 4,
  .streamMultiSegmentSize = 1024,
  .AXIAlignment = 0,
  .irqTypeMask = 0x1f0,
  .overlayEnables = 0,
  .mosaicEnables = 0,
  .mosXoffset = { 0 },
  .mosYoffset = { 0 },
  .mosWidth = { 0 },
  .mosHeight = { 0 },
  .burstMaxLength = 0,
  .sbi_id_0 = 0,
  .sbi_id_1 = 1,
  .sbi_id_2 = 2,
  .sramPowerdownDisable = 0,
  .sramPowerdownMode = 0,
  .sramPowerdownTimerDiv32 = 96,
  .coreMask = 0,
};

/*------------------------------------------------------------------------------
   get picture size
------------------------------------------------------------------------------*/
static void vce_getAlignedPicSizebyFormat(JpegEncFrameType type, u32 width, u32 height, u32 alignment,
                                       u64 *luma_Size,u64 *chroma_Size,u64 *picture_Size)
{
    u32 luma_stride=0, chroma_stride = 0;
    u64 lumaSize = 0, chromaSize = 0, pictureSize = 0;

#ifdef VCE_2_3
    JpegEncGetAlignedStride(width,type, &luma_stride, &chroma_stride, alignment);
#endif

#ifdef VCE_2_4
    u32 scan_type = 0; //TODO 0:raster scan [Default],1:superTileX scan
    JpegEncGetAlignedStride(width,type, &luma_stride, &chroma_stride, alignment, scan_type);
#endif

    switch(type)
    {
        case JPEGENC_YUV420_PLANAR:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2*2;
            break;

        case JPEGENC_YUV420_SEMIPLANAR:
        case JPEGENC_YUV420_SEMIPLANAR_VU:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2;
            break;

        case JPEGENC_YUV422_INTERLEAVED_YUYV:
        case JPEGENC_YUV422_INTERLEAVED_UYVY:
        case JPEGENC_RGB565:
        case JPEGENC_BGR565:
        case JPEGENC_RGB555:
        case JPEGENC_BGR555:
        case JPEGENC_RGB444:
        case JPEGENC_BGR444:
        case JPEGENC_RGB888:
        case JPEGENC_BGR888:
        case JPEGENC_RGB101010:
        case JPEGENC_BGR101010:
            lumaSize = luma_stride * height;
            chromaSize = 0;
            break;

        case JPEGENC_YUV420_I010:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2*2;
            break;

        case JPEGENC_YUV420_MS_P010:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height/2;
            break;

#if 0
        case JPEGENC_YUV420_8BIT_DAHUA_HEVC:
            lumaSize = luma_stride * height;
            chromaSize = lumaSize/2;
            break;

        case JPEGENC_YUV420_8BIT_DAHUA_H264:
            lumaSize = luma_stride * height * 2* 12/ 8;
            chromaSize = 0;
            break; 
#endif

        case JPEGENC_YUV422SP_888:
            lumaSize = luma_stride * height;
            chromaSize = chroma_stride * height;
            break;

        default:
            chromaSize = lumaSize = 0;
            break;
    }

    pictureSize = lumaSize + chromaSize;
    if (luma_Size != NULL)    *luma_Size = lumaSize;
    if (chroma_Size != NULL)  *chroma_Size = chromaSize;
    if (picture_Size != NULL) *picture_Size = pictureSize;
}

//! Destroy codec instance.
static void encoder_destroy_jpeg(ENCODER_PROTOTYPE* arg)
{
    DBGT_PROLOG("");

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;
    if (this)
    {
        this->base.stream_start = 0;
        this->base.stream_end = 0;
        this->base.encode = 0;
        this->base.destroy = 0;

        if (this->instance)
        {
            JpegEncRelease(this->instance);
            this->instance = 0;
        }

        OSAL_Free(this);
    }
    DBGT_EPILOG("");
}

// JPEG does not support streaming.
static CODEC_STATE encoder_stream_start_jpeg(ENCODER_PROTOTYPE* arg, STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;

    CODEC_STATE stat = CODEC_OK;

    this->encIn.pOutBuf[0] = (u8 *) stream->bus_data;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.outBufSize[0] = (stream->buf_max_size < 16*1024*1024) ? stream->buf_max_size : 16*1024*1024;

    DBGT_EPILOG("");
    return stat;
}

static CODEC_STATE encoder_stream_end_jpeg(ENCODER_PROTOTYPE* arg, STREAM_BUFFER* stream)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(stream);

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;

    CODEC_STATE stat = CODEC_OK;

    this->encIn.pOutBuf[0] = (u8 *) stream->bus_data;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.outBufSize[0] = (stream->buf_max_size < 16*1024*1024) ? stream->buf_max_size : 16*1024*1024;

    DBGT_EPILOG("");
    return stat;
}


// Encode a simple raw frame using underlying hantro codec.
// Converts OpenMAX structures to corresponding Hantro codec structures,
// and calls API methods to encode frame.
// Constraints:
// 1. only full frame encoding is supported, i.e., no thumbnails
// /param arg Codec instance
// /param frame Frame to encode
// /param stream Output stream
static CODEC_STATE encoder_encode_jpeg(ENCODER_PROTOTYPE* arg, FRAME* frame,
                                        STREAM_BUFFER* stream, void* cfg)
{
    DBGT_PROLOG("");

    DBGT_ASSERT(arg);
    DBGT_ASSERT(frame);
    DBGT_ASSERT(stream);
    UNUSED_PARAMETER(cfg);

    ENCODER_JPEG* this = (ENCODER_JPEG*)arg;
    CODEC_STATE stat = CODEC_ERROR_UNSPECIFIED;
    u64 height, luma_size = 0, chroma_size = 0;
    VCE_JPEG_OPTIONS *opts = &this->options;

    this->encIn.frameHeader = this->frameHeader ? 1 : 0;
    height = opts->lumHeightSrc;

    if (this->sliceMode)
    {
        OMX_U32 tmpSliceHeight = this->sliceHeight;

        // Last slice may be smaller so calculate slice height.
        if ((this->sliceHeight * this->sliceNumber) > (OMX_U32)this->options.lumHeightSrc)
        {
            tmpSliceHeight = this->options.lumHeightSrc - ((this->sliceNumber - 1) * this->sliceHeight);
        }


        if (this->leftoverCrop == OMX_TRUE)
        {
            /*  If picture full (Enough slices) start encoding again */
            if ((this->sliceHeight * this->sliceNumber) >= (OMX_U32)this->options.lumHeightSrc)
            {
                this->leftoverCrop = OMX_FALSE;
                this->sliceNumber = 1;
            }
            else
            {
                stream->streamlen = 0;
                DBGT_EPILOG("");
                return CODEC_OK;
            }
        }

        if (this->omxFrameType == OMX_COLOR_FormatYUV422Planar)
        {
            if ((tmpSliceHeight % 8) != 0)
            {
                DBGT_CRITICAL("Slice height is not divisible by 8");
                DBGT_EPILOG("");
                return CODEC_ERROR_INVALID_ARGUMENT;
            }
        }
        else
        {
            if ((tmpSliceHeight % 16) != 0)
            {
                DBGT_CRITICAL("Slice height is not divisible by 16");
                DBGT_EPILOG("");
                return CODEC_ERROR_INVALID_ARGUMENT;
            }
        }

        height = tmpSliceHeight;
    }

    vce_getAlignedPicSizebyFormat(opts->frameType, opts->lumWidthSrc, height, 1<<opts->exp_of_input_alignment, &luma_size, &chroma_size, NULL);
    this->encIn.busLum = frame->fb_bus_address;
    this->encIn.busCb = this->encIn.busLum + luma_size; // Cb or U
    this->encIn.busCr = this->encIn.busCb + chroma_size/2; // Cr or V
    this->encIn.pLum = (u8 *)frame->fb_bus_data;
    this->encIn.pCb = this->encIn.pLum + luma_size;
    this->encIn.pCr = this->encIn.pCb + chroma_size/2;

    this->encIn.pOutBuf[0] = (u8 *) stream->bus_data;
    this->encIn.busOutBuf[0] = stream->bus_address;
    this->encIn.outBufSize[0] = (stream->buf_max_size < 16*1024*1024) ? stream->buf_max_size : 16*1024*1024;

    JpegEncOut encOut;

    JpegEncRet ret = JpegEncEncode(this->instance, &this->encIn, &encOut);

    switch (ret)
    {
        case JPEGENC_RESTART_INTERVAL:
            // return encoded slice
            stream->streamlen = encOut.jfifSize;
            stat = CODEC_CODED_SLICE;
            this->sliceNumber++;
            break;
        case JPEGENC_FRAME_READY:
            stream->streamlen = encOut.jfifSize;
            stat = CODEC_OK;

            if ((this->sliceMode == OMX_TRUE) &&
                (this->sliceHeight * this->sliceNumber) >= (OMX_U32)opts->height)
            {
                this->leftoverCrop  = OMX_TRUE;
            }
            break;
        case JPEGENC_NULL_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case JPEGENC_INSTANCE_ERROR:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
        case JPEGENC_INVALID_ARGUMENT:
            stat = CODEC_ERROR_INVALID_ARGUMENT;
            break;
        case JPEGENC_INVALID_STATUS:
            stat = CODEC_ERROR_INVALID_STATE;
            break;
        case JPEGENC_OUTPUT_BUFFER_OVERFLOW:
            stat = CODEC_ERROR_BUFFER_OVERFLOW;
            break;
        case JPEGENC_HW_TIMEOUT:
            stat = CODEC_ERROR_HW_TIMEOUT;
            break;
        case JPEGENC_HW_BUS_ERROR:
            stat = CODEC_ERROR_HW_BUS_ERROR;
            break;
        case JPEGENC_HW_RESET:
            stat = CODEC_ERROR_HW_RESET;
            break;
        case JPEGENC_SYSTEM_ERROR:
            stat = CODEC_ERROR_SYSTEM;
            break;
        case JPEGENC_HW_RESERVED:
            stat = CODEC_ERROR_RESERVED;
            break;
        default:
            stat = CODEC_ERROR_UNSPECIFIED;
            break;
    }
    DBGT_EPILOG("");
    return stat;
}

static int vce_update_frame_type(OMX_COLOR_FORMATTYPE omx_format, ENCODER_JPEG *ctx) {
  VCE_JPEG_OPTIONS *opts = &ctx->options;

  switch (omx_format) {
    case OMX_COLOR_FormatYUV420PackedPlanar:
    case OMX_COLOR_FormatYUV420Planar:
      opts->frameType= JPEGENC_YUV420_PLANAR;
      break;
    case OMX_COLOR_FormatYUV420PackedSemiPlanar:
    case OMX_COLOR_FormatYUV420SemiPlanar:
      opts->frameType= JPEGENC_YUV420_SEMIPLANAR;
      break;
    case OMX_COLOR_FormatYCbYCr:
      opts->frameType= JPEGENC_YUV422_INTERLEAVED_YUYV;
      break;
    case OMX_COLOR_FormatCbYCrY:
      opts->frameType= JPEGENC_YUV422_INTERLEAVED_UYVY;
      break;
    case OMX_COLOR_Format16bitRGB565:
      opts->frameType= JPEGENC_RGB565;
      break;
    case OMX_COLOR_Format16bitBGR565:
      opts->frameType= JPEGENC_BGR565;
      break;
    case OMX_COLOR_Format16bitARGB4444:
      opts->frameType= JPEGENC_RGB444;
      break;
    case OMX_COLOR_Format16bitARGB1555:
      opts->frameType= JPEGENC_RGB555;
      break;
    case OMX_COLOR_Format12bitRGB444:
      opts->frameType= JPEGENC_RGB444;
      break;
    case OMX_COLOR_Format25bitARGB1888:
    case OMX_COLOR_Format32bitARGB8888:
      opts->frameType= JPEGENC_RGB888;
      break;
    default:
      DBGT_CRITICAL("Unknown color format");
      return -1;
  }

  return 0;
}

static int vce_update_opts_jpeg(ENCODER_JPEG *ctx, const JPEG_CONFIG* params) {
  VCE_JPEG_OPTIONS *opts = &ctx->options;

  opts->width = params->codingWidth;
  opts->height = params->codingHeight;
  opts->lumWidthSrc = (params->pp_config.origWidth + 15) & (~15);
  opts->lumHeightSrc = params->pp_config.origHeight;
  opts->horOffsetSrc = params->pp_config.xOffset;
  opts->verOffsetSrc = params->pp_config.yOffset;
  opts->partialCoding = params->codingType;
  // slice mode configuration
  if (params->codingType != JPEGENC_WHOLE_FRAME) {
    if (params->sliceHeight > 0) {
      ctx->sliceMode = OMX_TRUE;
      if (ctx->omxFrameType == OMX_COLOR_FormatYUV422Planar)
          opts->restartInterval = params->sliceHeight / 8;
      else
          opts->restartInterval = params->sliceHeight / 16;
      ctx->sliceHeight = params->sliceHeight;
    }
    else {
      DBGT_CRITICAL("Invalid slice height");
      return JPEGENC_INVALID_ARGUMENT;
    }
  }
  else {
    ctx->sliceMode = OMX_FALSE;
    ctx->sliceHeight = 0;
  }

  if(vce_update_frame_type(params->pp_config.formatType, ctx))
    return -1;

  switch (params->pp_config.angle) {
    case 0:
      opts->rotation = JPEGENC_ROTATE_0;
      break;
    case 90:
      opts->rotation = JPEGENC_ROTATE_90R;
      break;
    case 180:
      opts->rotation = JPEGENC_ROTATE_180;
      break;
    case 270:
      opts->rotation = JPEGENC_ROTATE_90L;
      break;
    default:
      DBGT_CRITICAL("Unsupported rotation angle");
      return -1;
  }
  opts->qLevel = params->qLevel;

  if (params->bAddHeaders) {
    opts->unitsType = params->unitsType;
    opts->xdensity = params->xDensity;
    opts->ydensity = params->yDensity;
    opts->markerType = params->markerType;
    opts->comLength = strlen(params->stringCommentMarker);
    opts->pCom = (const u8 *)params->stringCommentMarker;
  }

  return 0;
}

void vce_init_opts_jpeg(ENCODER_JPEG *ctx) {
  memcpy(&ctx->options, &vce_default_opts_jpeg, sizeof(VCE_JPEG_OPTIONS));
}

void vce_prepare_cfg_jpeg(const ENCODER_JPEG *ctx, JpegEncCfg *cfg) {
  const VCE_JPEG_OPTIONS *opts = &ctx->options;

  cfg->losslessEn = 0;
  cfg->predictMode = opts->predictMode;
  cfg->ptransValue = opts->ptransValue;
  cfg->mirror = opts->mirror;
  cfg->colorConversion.type = opts->colorConversion;

  // only support hardware handshaking.
  /* low latency */
  ASSERT(opts->inputLineBufMode == 0 ||
         opts->inputLineBufMode == 2 ||
         opts->inputLineBufMode == 4);
  cfg->inputLineBufEn = opts->inputLineBufMode;
  if (cfg->inputLineBufEn == 2)
    cfg->inputLineBufLoopBackEn = 1;
  else
    cfg->inputLineBufLoopBackEn = 0;
  cfg->inputLineBufDepth = opts->inputLineBufDepth;
  cfg->amountPerLoopBack = opts->amountPerLoopBack;
  if (cfg->inputLineBufEn == 2 || cfg->inputLineBufEn == 4)
    cfg->inputLineBufHwModeEn = 1;
  else
    cfg->inputLineBufHwModeEn = 0;
  cfg->inputLineBufCbFunc = NULL; // not support software handshaking
  cfg->inputLineBufCbData = NULL;

  /* SBI stream multi-segment */
  cfg->streamMultiSegmentMode = opts->streamMultiSegmentMode;
  cfg->streamMultiSegmentAmount = opts->streamMultiSegmentAmount;
  cfg->streamMultiSegCbFunc = NULL;
  cfg->streamMultiSegCbData = NULL;
  cfg->streamMultiSegmentSize = opts->streamMultiSegmentSize;
  /* constant chroma control */
  cfg->constChromaEn = opts->constChromaEn;
  cfg->constCb = opts->constCb;
  cfg->constCr = opts->constCr;
  /* jpeg rc*/
  cfg->targetBitPerSecond = opts->bitPerSecond;
  cfg->frameRateNum = 1;
  cfg->frameRateDenom = 1;
  if (opts->bitPerSecond) {
    cfg->frameRateNum = opts->frameRateNum;
    cfg->frameRateDenom = opts->frameRateDenom;
  }
  cfg->qpmin = opts->qpmin;
  cfg->qpmax = opts->qpmax;
  cfg->fixedQP = opts->fixedQP;
  cfg->rcMode   = opts->rcMode;
  cfg->picQpDeltaMax = opts->picQpDeltaMax;
  cfg->picQpDeltaMin = opts->picQpDeltaMin;

  cfg->exp_of_input_alignment = opts->exp_of_input_alignment;
  cfg->qTableLuma = NULL;
  cfg->qTableChroma = NULL;
  if (opts->quality == -1) {
    cfg->qLevel = opts->qLevel;
    cfg->quality = -1;
  }
  else
    cfg->quality = opts->quality;

  cfg->frameType = opts->frameType;
  cfg->rotation = opts->rotation;
  cfg->codingMode = opts->codingMode;

  if (ctx->frameHeader) {
    cfg->unitsType = (JpegEncAppUnitsType)opts->unitsType;
    cfg->markerType = (JpegEncTableMarkerType)opts->markerType;
    cfg->xDensity = opts->xdensity;
    cfg->yDensity = opts->ydensity;
    cfg->comLength = opts->comLength;
    cfg->pCom = opts->pCom;
  }

  DBGT_PDEBUG("unitsType %d", (int)cfg->unitsType);
  DBGT_PDEBUG("markerType %d", (int)cfg->markerType);
  DBGT_PDEBUG("xDensity %d", (int)cfg->xDensity);
  DBGT_PDEBUG("yDensity %d", (int)cfg->yDensity);
  DBGT_PDEBUG("comLength %d", (int)cfg->comLength);
  DBGT_PDEBUG("pCom %p", cfg->pCom);

  // set encoder mode parameters
  cfg->inputWidth = opts->lumWidthSrc;
  cfg->inputHeight = opts->lumHeightSrc;
  cfg->xOffset = opts->horOffsetSrc;
  cfg->yOffset = opts->verOffsetSrc;
  cfg->codingType = opts->partialCoding;
  cfg->codingWidth = opts->width;
  cfg->codingHeight = opts->height;

  cfg->restartInterval = opts->restartInterval;

  /* flexa sbi */
  cfg->sbi_id_0 = opts->sbi_id_0;
  cfg->sbi_id_1 = opts->sbi_id_1;
  cfg->sbi_id_2 = opts->sbi_id_2;
  cfg->segmentUnitHeight = opts->segmentUnitHeight;

  /* SRAM power down mode disable  */
  cfg->sramPowerdownDisable = opts->sramPowerdownDisable;
  if(cfg->sramPowerdownDisable == 0) {
    cfg->sramPowerdownMode = opts->sramPowerdownMode;
    cfg->sramPowerdownTimerDiv32 = opts->sramPowerdownTimerDiv32;
  }
  /* vcmd core mask to specify core */
  cfg->core_mask = opts->coreMask;
}

// Create JPEG codec instance and initialize it.
ENCODER_PROTOTYPE* HantroHwEncOmx_encoder_create_jpeg(const JPEG_CONFIG* params)
{
  DBGT_PROLOG("");

  DBGT_ASSERT(params);

  JpegEncCfg cfg;
  JpegEncRet ret;
  ENCODER_JPEG* this = OSAL_Malloc(sizeof(ENCODER_JPEG));
  memset(this, 0, sizeof(ENCODER_JPEG));

  //set default values -- for configs which no params passed  from OMX
  vce_init_opts_jpeg(this);

  this->sliceNumber = 1;
  this->omxFrameType = params->pp_config.formatType;

  if(vce_update_opts_jpeg(this, params)) {
    DBGT_CRITICAL("Create jpeg encoder failed -- parameters not supported!");
    OSAL_Free((OSAL_PTR *)this);
    DBGT_EPILOG("");
    return NULL;
  }

  // encIn struct init
  this->instance = 0;
  this->leftoverCrop = OMX_FALSE;
  this->frameHeader = params->bAddHeaders;

  memset(&cfg, 0, sizeof(JpegEncCfg));
  vce_prepare_cfg_jpeg(this, &cfg);

  // initialize static methods
  this->base.stream_start = encoder_stream_start_jpeg;
  this->base.stream_end = encoder_stream_end_jpeg;
  this->base.encode = encoder_encode_jpeg;
  this->base.destroy = encoder_destroy_jpeg;

#ifdef ENABLE_DBGT_TRACE
  JpegEncApiVersion apiVer;
  JpegEncBuild encBuild;
  apiVer = JpegEncGetApiVersion();
  DBGT_PDEBUG("Jpeg encoder API version %d.%d", apiVer.major, apiVer.minor);

  for (i = 0; i< EWLGetCoreNum(NULL); i++)
  {
      encBuild = JpegEncGetBuild(i, NULL);
      DBGT_PDEBUG("HW ID: %c%c 0x%08x\t SW Build: %u.%u.%u",
            encBuild.hwBuild>>24, (encBuild.hwBuild>>16)&0xff,
            encBuild.hwBuild, encBuild.swBuild / 1000000,
            (encBuild.swBuild / 1000) % 1000, encBuild.swBuild % 1000);
  }
#endif

  ret = JpegEncInit(&cfg, &this->instance, NULL);

  if (ret != JPEGENC_OK)
  {
      DBGT_CRITICAL("JpegEncInit failed! (%d)", ret);
      OSAL_Free(this);
      DBGT_EPILOG("");
      return NULL;
  }

  ret = JpegEncSetPictureSize(this->instance, &cfg);

  if (ret != JPEGENC_OK)
  {
      DBGT_CRITICAL("JpegEncSetPictureSize failed! (%d)", ret);
      OSAL_Free(this);
      DBGT_EPILOG("");
      return NULL;
  }
  DBGT_EPILOG("");
  return (ENCODER_PROTOTYPE*) this;
}

