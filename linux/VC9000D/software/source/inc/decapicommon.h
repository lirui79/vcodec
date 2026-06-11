/*--
--       Copyright (c) 2015-2023, VeriSilicon Inc. All rights reserved        --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#ifndef DECAPICOMMON_H
#define DECAPICOMMON_H

#include "basetype.h"

/** \brief The maximum number of cores supported in multi-core configuration.
 *  \n For G2, multi-core configuration is not supported.
 *  \ingroup common_group */
#define MAX_ASIC_CORES 4

/**
 * \addtogroup common_group
 *
 * @{
 */
/**@}*/

/** \brief The maximum output channels from \c VCDecNexPicture().
 *  \ingroup common_group */
#define DEC_MAX_PPU_COUNT 2
#define DEC_MAX_OUT_COUNT DEC_MAX_PPU_COUNT
/** \brief The pp0 start register.
 *  \ingroup common_group */
#define PP0_START_REG 384
/** \brief The pp unit register range.
 *  \ingroup common_group */
#ifdef PPU_V9_2_3
#define PPU_REG_RANGE 128
#else //PPU_V9_2_1_2
#define PPU_REG_RANGE 64
#endif

/** \brief The maximum number of registers.
 *  \ingroup common_group */
#define MAX_REG_COUNT        (PP0_START_REG + PPU_REG_RANGE * DEC_MAX_PPU_COUNT)
#define DEC_X170_REGISTERS   MAX_REG_COUNT
#define TOTAL_X170_REGISTERS MAX_REG_COUNT
/** \brief The YUV-to RGB conversion standard BT.601 for full range YUV.
 *  \ingroup common_group */
#define BT601 0
/** \brief The YUV-to RGB conversion standard BT.601 for limited range YUV.
 *  \ingroup common_group */
#define BT601_L 1
/** \brief The YUV-to RGB conversion standard BT.709 for full range YUV.
 *  \ingroup common_group */
#define BT709 2
/** \brief The YUV-to RGB conversion standard BT.709 for limited range YUV.
 *  \ingroup common_group */
#define BT709_L 3
/** \brief The YUV-to RGB conversion standard BT.2020 for full range YUV.
 *  \ingroup common_group */
#define BT2020 4
/** \brief The YUV-to RGB conversion standard BT.2020 for limited range YUV.
 *  \ingroup common_group */
#define BT2020_L 5
/** \brief A function that checks whether full range is used.
 *  \ingroup common_group */
#define IS_FULL_RANGE(fmt) \
        ((fmt == BT601) || (fmt == BT709) || (fmt == BT2020))

/** \brief The linear interpolation algorithm.
 *  \ingroup common_group */
#define VSI_LINEAR 0
/** \brief The Lanczos interpolation algorithm.
 *  \ingroup common_group */
#define LANCZOS 1
/** \brief The nearest interpolation algorithm.
 *  \ingroup common_group */
#define NEAREST  2
/** \brief The bilinear interpolation algorithm.
 *  \ingroup common_group */
#define BI_LINEAR 3
/** \brief The bicubic interpolation algorithm.
 *  \ingroup common_group */
#define BICUBIC 4
/** \brief The Spline interpolation algorithm.
 *  \ingroup common_group */
#define SPLINE 5
/** \brief The box interpolation algorithm.
 *  \ingroup common_group */
#define BOX 6
/** \brief The fast linear interpolation algorithm.
 *  \ingroup common_group */
#define FAST_LINEAR 7
/** \brief The fast bicubic interpolation algorithm.
 *  \ingroup common_group */
#define FAST_BICUBIC 8
/** \brief The area interpolation algorithm.
 *  \ingroup common_group */
#define AREA 9

/** \brief The 4x4 tiled output mode.
 * \ingroup common_group */
#define TILED4x4 0
/** \brief The 8x8 tiled output mode.
 * \ingroup common_group */
#define TILED8x8 1
/** \brief The 16x16 tiled output mode.
 * \ingroup common_group */
#define TILED16x16 2
/** \brief The 32x8 tiled output mode.
 * \ingroup common_group */
#define TILED32x8 3
/** \brief The 128x2 tiled output mode.
 * \ingroup common_group */
#define TILED128x2 4
/** \brief The 64x64 tiled output mode.
 * \ingroup common_group */
#define TILED64x64 7

/** \brief The round-down algorithm for center-point selection.
 *  \ingroup common_group */
#define DOWN_ROUND 0
/** \brief The no-round algorithm for center-point selection.
 *  \ingroup common_group */
#define NO_ROUND   1
/** \brief The round-up algorithm for center-point selection.
 *  \ingroup common_group */
#define UP_ROUND   2

/** \brief The packed mode YUYV for YUV422 input.
 *  \ingroup common_group */
#define YUYV 0
/** \brief The packed mode UYVY for YUV422 input.
 *  \ingroup common_group */
#define UYVY 1

/** \brief The output format YUV400.
 *  \ingroup common_group */
#define PP_YUV400 0
/** \brief The output format YUV420.
 *  \ingroup common_group */
#define PP_YUV420 1
/** \brief The output format YUV422.
 *  \ingroup common_group */
#define PP_YUV422 2
/** \brief The output format YUV444.
 *  \ingroup common_group */
#define PP_YUV444 3

/** \brief Defines VC1 profiles.
 *  \ingroup common_group */
enum VC1Profile {
  /** The Simple profile. */
  VC1_SIMPLE_PROFILE = 0,
  /** The Main profile. */
  VC1_MAIN_PROFILE = 4,
  /** The Advanced profile. */
  VC1_ADVANCED_PROFILE = 12
};

/**
 * \brief Extra alignment between different planes of PP buffers.
 * \ingroup common_group
 */
#define PLANE_ALIGNMENT 1

/** \brief The 4x4 tiled input mode for standalone post-processing without decoder pipeline.
 *  \ingroup common_group */
#define IN_TILED4x4 0
/** \brief The 8x8 tiled input mode for standalone post-processing without decoder pipeline.
 *  \ingroup common_group */
#define IN_TILED8x8 1
/** \brief The 64x64 tiled input mode for standalone post-processing without decoder pipeline.
 *  \ingroup common_group */
#define IN_TILED64x64 2

/** \brief The input data format Y8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \n For details about data formats, see <em>Hantro VC9000D Series Memory Buffer and Format
 *  Organization</em>.
 *  \ingroup common_group */
#define PP_IN_400_8BIT  0
/** \brief The input data format YUV420SP8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_NV12_8BIT 1
/** \brief The input data format YVU420SP8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_NV21_8BIT 2
/** \brief The input data format YUV420P8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_420P_8BIT 3
/** \brief The input data format YUV422SP8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_NV16_8BIT 4
/** \brief The third-party compressed 4x4 tiled input data format for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_TILED4x4  5
/** \brief The input data format YUV422YUYV8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_YUYV_8BIT 6
/** \brief The input data format YUV422YVYU8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_YVYU_8BIT 7
/** \brief The input data format YUV422UYVY8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_UYVY_8BIT 8
/** \brief The input data format YUV422VYUY8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_VYUY_8BIT 9
/** \brief The input data format YUV420SP10bWH_Raster_A<em>X</em>. for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_NV12_P010 10
/** \brief The input data format YVU420SP10bWH_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_NV21_P010 11
/** \brief The input data format YUV420P10bWH_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_420P_P010 12
/** \brief The input data format YUV422SP10bWH_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_NV16_P010 13
/** \brief The third-party compressed 4x4 tiled P010 input data format for standalone
 *  post-processing without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_TILED4x4_P010 14
/** \brief The input data format Y10bWH_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_400_P010  15
/** \brief The input data format RGB888P_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_RGB888_P 16
/** \brief The input data format BGR888P_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_BGR888_P 17
/** \brief The input data format RGB16l16l16lP_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_R16G16B16_P 18
/** \brief The input data format BGR16l16l16lP_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_B16G16R16_P 19
/** \brief The input data format RGB888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_RGB888 20
/** \brief The input data format BGR888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_BGR888 21
/** \brief The input data format ARGB8888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_ARGB888 22
/** \brief The input data format ABGR8888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_ABGR888 23
/** \brief The input data format ARGB2101010_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_A2R10G10B10 24
/** \brief The input data format ABGR2101010_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_A2B10G10R10 25
/** \brief The input data format XRGB2101010_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_X2R10G10B10 26
/** \brief The input data format XBGR2101010_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_X2B10G10R10 27
/** \brief The RFC compressed input data format for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_RFC         28
/** \brief The input data format YVU422SP8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_NV61_8BIT 29
/** \brief The input data format YVU422SP10bWH_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_422_NV61_P010 30
/** \brief The input data format YUV444P8b_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_444P_8BIT 31
/** \brief The input data format YUV444P10bWH_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_444P_P010 32
/** \brief The input data format RGBA8888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_RGBA888 33 /* R8G8B8A8 */
/** \brief The input data format BGRA8888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_BGRA888 34 /* B8G8R8A8 */
/** \brief The input data format RGBA1010102_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_R10G10B10A2 35
/** \brief The input data format BGRA1010102_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_B10G10R10A2 36
/** \brief The input data format XRGB888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_XRGB888 37
/** \brief The input data format XBGR888_Raster_A<em>X</em> for standalone post-processing
 *  without decoder pipeline.
 *  \ingroup common_group */
#define PP_IN_XBGR888 38

#if 0
#define PP_IN_422P_8BIT     40   /*< planar YUV 4:2:2*/
#define PP_IN_444_NV24_8BIT 41 /*< semi planar YUV 4:4:4, 24bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)*/
#define PP_IN_444_NV42_8BIT 42 /*as above, but U and V bytes are swapped*/

#define PP_IN_422P_P010     50   /*< planar YUV 4:2:2*/
#define PP_IN_444_NV24_P010 51
#define PP_IN_444_NV42_P010 52
#endif

#if !defined(MODEL_SIMULATION) || defined(HW_PIC_DIMENSIONS)
/** \brief The minimum picture width supported by advanced decoder hardware for codec standards except AVS2.
 *  \ingroup common_group */
#define MIN_PIC_WIDTH 72
/** \brief The minimum picture height supported by advanced decoder hardware for codec standards except AVS2.
 *  \ingroup common_group */
#define MIN_PIC_HEIGHT 72
/** \brief The minimum picture width supported by advanced decoder hardware for AVS2.
 *  \ingroup common_group */
#define MIN_PIC_WIDTH_AVS2 128
/** \brief The minimum picture width supported by advanced decoder hardware for AVS2.
 *  \ingroup common_group */
#define MIN_PIC_HEIGHT_AVS2 128
#define MIN_PIC_WIDTH_H264          48
#define MIN_PIC_HEIGHT_H264          48
#else /* MODEL_SIMULATION */
#define MIN_PIC_WIDTH               8
#define MIN_PIC_HEIGHT              8

#define MIN_PIC_WIDTH_AVS2          8
#define MIN_PIC_HEIGHT_AVS2         8

#define MIN_PIC_WIDTH_H264          8
#define MIN_PIC_HEIGHT_H264         8
#endif /* MODEL_SIMULATION */

#define MIN_PIC_WIDTH_G1            48
#define MIN_PIC_HEIGHT_G1           48
#define PNGDEC_MIN_WIDTH            32
#define PNGDEC_MIN_HEIGHT           32
#define JPEGDEC_MAX_WIDTH_TN        256
#define JPEGDEC_MAX_HEIGHT_TN       256

/** JPEG specific.
 *  \ingroup jpeg_group */
enum {
  /** No units, X and Y specify the pixel aspect ratio. */
  JPEGDEC_NO_UNITS = 0,
  /** \brief X and Y are dots per inch. */
  JPEGDEC_DOTS_PER_INCH = 1,
  /** \brief X and Y are dots per cm. */
  JPEGDEC_DOTS_PER_CM = 2
};

/** Defines JPEG thumbnail types.
 *  \ingroup jpeg_group */
enum {
  /** Thumbnail in JPEG format. */
  JPEGDEC_THUMBNAIL_JPEG = 0x10,
  /** Unsupported thumbnail format. */
  JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT = 0x11,
  /** No thumbnail. */
  JPEGDEC_NO_THUMBNAIL = 0x12
};

/** Defines to-be-decoded JPEG image types.
 *  \ingroup jpeg_group */
enum {
  /** Full resolution JPEG. */
  JPEGDEC_IMAGE = 0,
  /** Thumbnail JPEG. */
  JPEGDEC_THUMBNAIL = 1
};

/** Defines JPEG coding modes.
 *  \ingroup jpeg_group */
enum JpegCodingMode {
  /** Unsupported process. */
  JPEG_NOT_SUPPORTED = 0x0,
  /** Baseline DCT process. */
  JPEG_BASELINE = 0x01,
  /** Progressive DCT process, Huffman coding. */
  JPEG_PROGRESSIVE = 0x02,
  /** Extended sequential DCT process, Huffman coding. */
  JPEG_EXTENDED = 0x03,
  /** Non-interleaved encoding. */
  JPEG_NONINTERLEAVED = 0x04,
};

#ifdef OPEN_HWCFG_TO_CLIENT
#define MAX_CLIENT_TYPE 20
/** \brief A structure to store hardware configuration.
 *  \ingroup common_group */
struct DecHwConfig {
  u32 mpeg4_support;        /**< \brief one of the MPEG4 values defined above */
  u32 custom_mpeg4_support; /**< \brief one of the MPEG4 custom values defined above */
  u32 h264_support;         /**< \brief one of the H264 values defined above */
  u32 vc1_support;          /**< \brief one of the VC1 values defined above */
  u32 mpeg2_support;        /**< \brief one of the MPEG2 values defined above */
  u32 jpeg_support;         /**< \brief one of the JPEG values defined above */
  u32 max_dec_pic_width[MAX_CLIENT_TYPE];    /**< \brief maximum picture width in decoder */
  u32 max_dec_pic_height[MAX_CLIENT_TYPE];   /**< \brief maximum picture height in decoder */
  u32 pp_standalone;        /**< \brief PP_SUPPORTED or PP_NOT_SUPPORTED */
  u32 pp_config;            /**< \brief Bitwise list of PP function */
  u32 max_pp_out_pic_width;   /**< \brief maximum post-processor output picture width */
  u32 max_pp_out_pic_height;   /**< \brief maximum post-processor output picture height */
  u32 sorenson_spark_support; /**< \brief one of the SORENSON_SPARK values defined above */
  u32 vp6_support;           /**< \brief one of the VP6 values defined above */
  u32 vp7_support;           /**< \brief one of the VP7 values defined above */
  u32 vp8_support;           /**< \brief one of the VP8 values defined above */
  u32 vp9_support;           /**< \brief HW supports VP9 */
  u32 avs_support;           /**< \brief one of the AVS values defined above */
  u32 rv_support;            /**< \brief one of the HUKKA values defined above */
  u32 webp_support;          /**< \brief one of the WEBP values defined above */
  u32 stride_support;        /**< \brief HW supports separate Y and C strides */
  u32 avs_plus_support;      /**< \brief one of the AVS PLUS values defined above */
  u32 addr64_support;         /**< \brief HW supports 64bit addressing */
  u32 avs2_support;           /**< \brief HW supports AVS2 */
  u32 hevc_support;          /**< \brief HW supports HEVC */
  u32 av1_support;           /**< \brief HW supports AV1 */
  u32 vvc_support;           /**< \brief HW supports VVC */

  u32 hevc_main10_support;  /**< \brief HW supports HEVC Main10 profile*/
  u32 vp9_10bit_support;     /**< \briefHW supports VP9 10 bits profile */
  u32 ds_support;            /**< \brief HW supports down scaling. */
  u32 rfc_support;           /**< \brief HW supports reference frame compression. */
  u32 fmt_p010_support;      /**< \brief HW supports P010 format. */
};
#endif

/** \brief Contains the information of the decoder API version.
 *  \ingroup common_group */
struct DecApiVersion {
  /** \brief The major version with architecture update or product promotion. */
  u32 major;
  /** \brief The minor version with milestone features added. */
  u32 minor;
  /** \brief The micro version with feature improvement or interface changes. */
  u32 micro;
};

/** \brief Contains the build information.
 *  \ingroup common_group */
struct DecSwHwBuild {
  /** \brief The software build ID. */
  u32 sw_build;
  /** \brief The hardware ASIC ID. */
  u32 asic_id;
  /** \brief The hardware build ID of each core. */
  u32 hw_build_id[MAX_ASIC_CORES];
#ifdef OPEN_HWCFG_TO_CLIENT
  /** \brief The hardware configurations of each core. */
  struct DecHwConfig hw_config[MAX_ASIC_CORES];
#endif
};

#define LOW_LATENCY_PACKET_SIZE 256

/** Defines content storage modes in DPB.
 *  \ingroup common_group */
enum DecDpbMode {
  /** (Default) Data in DPB is stored by frame. */
  DEC_DPB_FRAME = 0,
  /** Data in DPB is stored by interfaced field. */
  DEC_DPB_INTERLACED_FIELD = 1
};

/** Defines decoder working modes.
 *  \ingroup common_group */
enum DecDecoderMode {
  /** The normal decoding mode, in which the hardware is not enabled until the complete data of a
   *  frame is ready.
   *  \n The minimum hardware decoding unit is a frame. */
  DEC_NORMAL =           0x00000000,
  /** The low-latency decoding mode, in which the hardware can be enabled and then wait for input
   *  stream data.
   *  \n The minimum hardware decoding unit is 256 bytes. */
  DEC_LOW_LATENCY =      0x00000001,
  /** (Deprecated) The security playback mode. */
  DEC_SECURITY =         0x00000002,
  /** A special decoding mode which does not write reconstructed data, reference data,
   *  MV, or post-processing output for non-reference pictures.
   *  \n This mode is not supported currently. */
  DEC_PARTIAL_DECODING = 0x00000004,
  /** The intra-only decoding mode, which decodes intra frames only. */
  DEC_INTRA_ONLY =       0x00000008,
  /** The HEIF mode, in which the decoder is fed with HEIF files as its input. */
  DEC_HEIF =             0x00000010
};


/** Defines flags to indicate whether auxiliary infomation is present.
 *  \ingroup common_group */
enum DecAuxInfo {
  /** No auxiliary infomation is present. */
  DEC_AUX_NONE =          0x00000000,
  /** QP information is present. */
  DEC_AUX_QP =            0x00000001,
  /** MV information is present. */
  DEC_AUX_MV =            0x00000002,
};

/** Defines delogo filter modes.
 *  \ingroup common_group */
enum DelogoMode {
  /** \brief Disables the delogo filter. */
  PIXEL_NO_DELOGO = 0,
  /** \brief Enables the delogo filter in replacement mode. */
  PIXEL_REPLACE = 1,
  /** \brief Enables the delogo filter in interpolation mode. */
  PIXEL_INTERPOLATION = 2
};
/* DEPRECATED!!! do not use in new applications! */
#define DEC_DPB_DEFAULT DEC_DPB_FRAME

/** Defines output picture formats.
 *
 *  For details about color formats, see <em>Hantro VC9000D Series Memory Buffer and Format
 *  Organization</em>.
 *  \ingroup common_group */
enum DecPictureFormat {
  /** Tiled 4x4 format. */
  DEC_OUT_FRM_TILED_4X4 = 0,
  /** (Deprecated). */
  DEC_OUT_FRM_TILED_8X4 = 1,
  /** YUV420SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_RASTER_SCAN = 2,
  /** YUV420P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_PLANAR_420 = 3,
  /** Tiled 8x8 format. */
  DEC_OUT_FRM_TILED_8X8 = 4,
  /** Tiled 8x8 P010 format, 10&12 bit. */
  DEC_OUT_FRM_TILED_8X8_P010 = 5,
  /** Tiled 16x16 format. */
  DEC_OUT_FRM_TILED_16X16 = 6,
  /** Tiled 16x16 P010 format, 10&12 bit. */
  DEC_OUT_FRM_TILED_16X16_P010 = 7,
  /** Tiled 64x64 format. */
  DEC_OUT_FRM_TILED_64X64,
  /** Tiled 64x64 format. */
  DEC_OUT_FRM_TILED_64X64_PACK10,
  /** Y8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_MONOCHROME,
  /** Reference frame compression (RFC) compressed format. */
  DEC_OUT_FRM_RFC,

  /* FBC */
  /** Third-party compressed 16x16 tiled format. */
  DEC_OUT_FRM_TILED_32X8_COMP,
  /** Third-party compressed 16x16 tiled P010 format, 10&12 bit. */
  DEC_OUT_FRM_TILED_32X8_P010_COMP,
  /** Third-party compressed 16x16 tiled format. */
  DEC_OUT_FRM_TILED_16X16_COMP,
  /** Third-party compressed 16x16 tiled P010 format, 10&12 bit. */
  DEC_OUT_FRM_TILED_16X16_P010_COMP,

  /* DEC400 */
  /** DEC400 compressed YUV400 semi-planar. */
  DEC_OUT_FRM_DEC400_400SP,
  /** DEC400 compressed YUV420 semi-planar. */
  DEC_OUT_FRM_DEC400_420SP,
  /** DEC400 compressed YUV400 tile4x4. */
  DEC_OUT_FRM_DEC400_400TILE,
  /** DEC400 compressed YUV420 tile4x4. */
  DEC_OUT_FRM_DEC400_420TILE,
  /** DEC400 compressed YUV400 tile8x8. */
  DEC_OUT_FRM_DEC400_400TILED8x8,
  /** DEC400 compressed YUV420 tile4x4. */
  DEC_OUT_FRM_DEC400_420TILED8x8,
  /** DEC400 compressed YUV400 planar. */
  DEC_OUT_FRM_DEC400_400P,
  /** DEC400 compressed YUV420 planar. */
  DEC_OUT_FRM_DEC400_420P,
  /** DEC400 compressed YUV422 8bit packed. */
  DEC_OUT_FRM_DEC400_422PACKED,
  /** DEC400 output BGR888. */
  DEC_OUT_FRM_DEC400_BGR888,
  /** DEC400 output RGB888. */
  DEC_OUT_FRM_DEC400_RGB888,
  /** DEC400 output ABGR8889. */
  DEC_OUT_FRM_DEC400_ABGR888,
  /** DEC400 output ARGB8888. */
  DEC_OUT_FRM_DEC400_ARGB888,
  /** DEC400 output BGRA8888. */
  DEC_OUT_FRM_DEC400_BGRA888,
  /** DEC400 output RGBA8888. */
  DEC_OUT_FRM_DEC400_RGBA888,
  /** DEC400 output A2B10G10R10. */
  DEC_OUT_FRM_DEC400_A2B10G10R10,
  /** DEC400 output A2R10G10B10. */
  DEC_OUT_FRM_DEC400_A2R10G10B10,
  /** DEC400 compressed X2B10G10R10. */
  DEC_OUT_FRM_DEC400_X2B10G10R10,
  /** DEC400 compressed X2R10G10B10. */
  DEC_OUT_FRM_DEC400_X2R10G10B10,
  /** DEC400 output A2B10G10R10. */
  DEC_OUT_FRM_DEC400_B10G10R10A2,
  /** DEC400 output A2R10G10B10. */
  DEC_OUT_FRM_DEC400_R10G10B10A2,
  /** DEC400 output XBGR8888. */
  DEC_OUT_FRM_DEC400_XBGR888,
  /** DEC400 output XRGB8888. */
  DEC_OUT_FRM_DEC400_XRGB888,
  /** DEC400 output ABGR8888_TILED64X64.*/
  DEC_OUT_FRM_DEC400_ABGR888_TILED64X64,
  /** DEC400 output ARGB8888_TILED64X64. */
  DEC_OUT_FRM_DEC400_ARGB888_TILED64X64,
  /** DEC400 output A2B10G10R10_TILED64X64. */
  DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64,
  /** DEC400 output A2R10G10B10_TILED64X64. */
  DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64,
  /** DEC400 compressed X2B10G10R10_TILED64X64. */
  DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64,
  /** DEC400 compressed X2R10G10B10_TILED64X64. */
  DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64,
  /** DEC400 output XBGR8888_TILED64X64. */
  DEC_OUT_FRM_DEC400_XBGR888_TILED64X64,
  /** DEC400 output XRGB8888_TILED64X64. */
  DEC_OUT_FRM_DEC400_XRGB888_TILED64X64,
  /** DEC400 output BGRA8888_TILED64X64. */
  DEC_OUT_FRM_DEC400_BGRA888_TILED64X64,
  /** DEC400 output RGBA8888_TILED64X64. */
  DEC_OUT_FRM_DEC400_RGBA888_TILED64X64,
  /** DEC400 output B10G10R10A2_TILED64X64. */
  DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64,
  /** DEC400 output R10G10B10A2_TILED64X64. */
  DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64,

  /*PVFBC*/
  /** PVFBC compressed YUV420 semi-planar */
  DEC_OUT_FRM_PVFBC_420SP,
  /** PVFBC compressed YUV420 P010 semi-planar */
  DEC_OUT_FRM_PVFBC_420SP_P010,

  /* YUV420 */
  /** Uncompressed YUV420SP8b_YuvSp4x4_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV420TILE,
  /** Uncompressed YUV420SP10b_YuvSp4x4_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV420TILE_PACK10,
  /** Uncompressed YUV420SP12b_YuvSp4x4_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV420TILE_PACK12,
  /** Uncompressed YUV420SP8b_YuvSp8x8_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV420TILE8x8,
  /** Uncompressed YUV420SP10b_YuvSp8x8_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV420TILE8x8_PACK10,
  /** Uncompressed YUV420SP12b_YuvSp8x8_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV420TILE8x8_PACK12,
  /** YUV420SP10bWH_YuvSp4x4_A<em>X</em>. */
  DEC_OUT_FRM_YUV420TILE_P010,
  /** YUV420SP10bWL_YuvSp4x4_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV420TILE_I010,
  /** YUV420SP10bWL_YuvSp4x4_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV420TILE_L010,
  /** YUV420SP12bWH_YuvSp4x4_A<em>X</em>. */
  DEC_OUT_FRM_YUV420TILE_P012,
  /** YUV420SP12bWL_YuvSp4x4_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV420TILE_I012,
  /** YUV420SP8b_YuvSp128x2_A<em>X</em>. */
  DEC_OUT_FRM_YUV420TILE_128X2,
  /** YUV420SP10bWH_YuvSp128x2_A<em>X</em>. */
  DEC_OUT_FRM_YUV420TILE_128X2_P010,
  /** YUV420SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420SP,
  /** YUV420SP10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420SP_PACK10,
  /** (Deprecated) YUV420SP12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420SP_PACK12,
  /** YUV420SP10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420SP_P010,
  /** YUV420SP10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV420SP_I010,
  /** YUV420SP10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV420SP_L010,
  /** YUV420SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420SP_P012,
  /** YUV420SP12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV420SP_I012,
  /** YUV420SP10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420SP_1010,
  /** YUV420P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420P,
  /** YUV420P10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420P_PACK10,
  /** YUV420P12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420P_PACK12,
  /** YUV420P10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420P_P010,
  /** YUV420P10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV420P_I010,
  /** YUV420P10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV420P_L010,
  /** YUV420P12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420P_P012,
  /** YUV420P12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV420P_I012,
  /** YUV420P10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV420P_1010,

  /* YUV400 */
  /** Uncompressed Y8b_Tile4x4_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV400TILE,
  /** Uncompressed Y10b_Tile4x4_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV400TILE_PACK10,
  /** Uncompressed Y8b_Tile8x8_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV400TILE8x8,
  /** Uncompressed Y10b_Tile8x8_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV400TILE8x8_PACK10,
  /** Uncompressed Y10bWH_Tile8x8_A<em>X</em> for output to DPB. */
  DEC_OUT_FRM_YUV400TILE8x8_P010,
  /** Y10bWH_Tile4x4_A<em>X</em>. */
  DEC_OUT_FRM_YUV400TILE_P010,
  /** Y10bWL_Tile4x4_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV400TILE_I010,
  /** Y10bWL_Tile4x4_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV400TILE_L010,
  /** (Deprecated) Y12bWH_Tile4x4_A<em>X</em>. */
  DEC_OUT_FRM_YUV400TILE_P012,
  /** (Deprecated) Y12bWL_Tile4x4_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV400TILE_I012,
  /** Y8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV400,
  /** Y10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV400_PACK10,
  /** (Deprecated) Y12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV400_PACK12,
  /** Y10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV400_P010,
  /** Y10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV400_I010,
  /** Y10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV400_L010,
  /** Y12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV400_P012,
  /** Y12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV400_I012,
  /** Y8b_Tile128x2_A<em>X</em>. */
  DEC_OUT_FRM_YUV400TILE_128X2,
  /** Y10bWH_Tile128x2_A<em>X</em>. */
  DEC_OUT_FRM_YUV400TILE_128X2_P010,

  /* YUV422 */
  /** YUV422SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422SP,
  /** YUV422SP10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422SP_PACK10,
  /** (Deprecated) YUV422SP12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422SP_PACK12,
  /** YUV422SP10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422SP_P010,
  /** YUV422SP10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV422SP_I010,
  /** YUV422SP10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV422SP_L010,
  /** (Deprecated) YUV422SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422SP_P012,
  /** (Deprecated) YUV422SP12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV422SP_I012,
  /** YUV422SP10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422SP_1010,
  /** (Deprecated) YUV422P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422P,
  /** (Deprecated) YUV422P10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422P_PACK10,
  /** (Deprecated) YUV422P12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422P_PACK12,
  /** YUV422P10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422P_P010,
  /** (Deprecated) YUV422P10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV422P_I010,
  /** (Deprecated) YUV422P10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV422P_L010,
  /** (Deprecated) YUV422P12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422P_P012,
  /** (Deprecated) YUV422P12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV422P_I012,
  /** YUV422YUYV8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422_YUYV,
  /** YUV422UYVY8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422_UYVY,
  /** YUV422YVYU8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422_YVYU,
  /** YUV422VYUY8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV422_VYUY,

  /* YUV440 */
  /** (Deprecated) YUV440 8-bit raster-scan. */
  DEC_OUT_FRM_YUV440,

  /* YUV411 */
  /** (Deprecated) YUV411SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV411SP,

  /* YUV444 */
  /** (Deprecated) YUV444SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444SP,
  /** (Deprecated) YUV444SP10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444SP_PACK10,
  /** (Deprecated) YUV444SP12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444SP_PACK12,
  /** (Deprecated) YUV444SP10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444SP_P010,
  /** (Deprecated) YUV444SP10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV444SP_I010,
  /** (Deprecated) YUV444SP10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV444SP_L010,
  /** (Deprecated) YUV444SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444SP_P012,
  /** (Deprecated) YUV444SP12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV444SP_I012,
  /** (Deprecated) YUV444SP10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444SP_1010,
  /** YUV444P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444P,
  /** (Deprecated) YUV444P10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444P_PACK10,
  /** (Deprecated) YUV444P12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444P_PACK12,
  /** YUV444P10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444P_P010,
  /** (Deprecated) YUV444P10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_YUV444P_I010,
  /** (Deprecated) YUV444P10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_YUV444P_L010,
  /** (Deprecated) YUV444P12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_YUV444P_P012,
  /** (Deprecated) YUV444P12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_YUV444P_I012,

  /* NV21 */
  /** YVU420SP8b_YuvSp4x4_A<em>X</em>. */
  DEC_OUT_FRM_NV21TILE,
  /** YVU420SP10b_YuvSp4x4_A<em>X</em>. */
  DEC_OUT_FRM_NV21TILE_PACK10,
  /** YVU420SP10bWH_YuvSp4x4_A<em>X</em>. */
  DEC_OUT_FRM_NV21TILE_P010,
  /** YVU420SP10bWL_YuvSp4x4_A<em>X</em> I010. */
  DEC_OUT_FRM_NV21TILE_I010,
  /** YVU420SP10bWL_YuvSp4x4_A<em>X</em> L010. */
  DEC_OUT_FRM_NV21TILE_L010,
  /** YVU420SP12bWH_YuvSp4x4_A<em>X</em>. */
  DEC_OUT_FRM_NV21TILE_P012,
  /** YVU420SP12bWL_YuvSp4x4_A<em>X</em> I012. */
  DEC_OUT_FRM_NV21TILE_I012,
  /** YVU420SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21SP,
  /** YVU420SP10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21SP_PACK10,
  /** YVU420SP12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21SP_PACK12,
  /** YVU420SP10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21SP_P010,
  /** YVU420SP10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_NV21SP_I010,
  /** YVU420SP10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_NV21SP_L010,
  /** (Deprecated) YVU420SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21SP_P012,
  /** (Deprecated) YVU420SP12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_NV21SP_I012,
  /** YVU420SP10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21SP_1010,
  /** YVU420P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21P,
  /** YVU420P10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21P_PACK10,
  /** (Deprecated) YVU420P12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21P_PACK12,
  /** YVU420P10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21P_P010,
  /** YVU420P10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_NV21P_I010,
  /** YVU420P10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_NV21P_L010,
  /** YVU420SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21P_P012,
  /** YVU420P12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_NV21P_I012,
  /** YVU420P10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_NV21P_1010,

  /* 422 NV21 */
  /** YVU422SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21SP,
  /** YVU422SP10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21SP_PACK10,
  /** YVU422SP12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21SP_PACK12,
  /** YVU422SP10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21SP_P010,
  /** YVU422SP10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_422_NV21SP_I010,
  /** YVU422SP10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_422_NV21SP_L010,
  /** (Deprecated) YVU422SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21SP_P012,
  /** (Deprecated) YVU422SP12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_422_NV21SP_I012,
  /** YVU422SP10bDWL_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21SP_1010,
  /** YVU422P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21P,
  /** YVU422P10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21P_PACK10,
  /** (Deprecated) YVU422P12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21P_PACK12,
  /** YVU422P10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21P_P010,
  /** (Deprecated) YVU422P10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_422_NV21P_I010,
  /** (Deprecated) YVU422P10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_422_NV21P_L010,
  /** (Deprecated) YVU422P12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_422_NV21P_P012,
  /** (Deprecated) YVU422P12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_422_NV21P_I012,

  /* 444 NV21 */
  /** YVU444SP8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21SP,
  /** YVU444SP10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21SP_PACK10,
  /** (Deprecated) YVU444SP12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21SP_PACK12,
  /** YVU444SP10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21SP_P010,
  /** (Deprecated) YVU422SP10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_444_NV21SP_I010,
  /** (Deprecated) YVU422SP10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_444_NV21SP_L010,
  /** (Deprecated) YVU444SP12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21SP_P012,
  /** (Deprecated) YVU444SP12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_444_NV21SP_I012,
  /** YVU444P8b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21P,
  /** YVU444P10b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21P_PACK10,
  /** YVU444P12b_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21P_PACK12,
  /** YVU444P10bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21P_P010,
  /** YVU444P10bWL_Raster_A<em>X</em> I010. */
  DEC_OUT_FRM_444_NV21P_I010,
  /** YVU444P10bWL_Raster_A<em>X</em> L010. */
  DEC_OUT_FRM_444_NV21P_L010,
  /** YVU444P12bWH_Raster_A<em>X</em>. */
  DEC_OUT_FRM_444_NV21P_P012,
  /** YVU444P12bWL_Raster_A<em>X</em> I012. */
  DEC_OUT_FRM_444_NV21P_I012,

  /* RGB */
  /* sw_ppx_out_rgb_fmt == 0 */
  /** RGB888P_Raster_A<em>X</em>. */
  DEC_OUT_FRM_RGB888_P,
  /** BGR888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_BGR888,
  /* sw_ppx_out_rgb_fmt == 1 */
  /** BGR888P_Raster_A<em>X</em>. */
  DEC_OUT_FRM_BGR888_P,
  /** RGB888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_RGB888,
  /* sw_ppx_out_rgb_fmt == 2 */
  /** RGB16l16l16lP_Raster_A<em>X</em>. */
  DEC_OUT_FRM_R16G16B16_P,
  /** BGR16l16l16l_Raster_A<em>X</em>. */
  DEC_OUT_FRM_B16G16R16,
  /* sw_ppx_out_rgb_fmt == 3 */
  /** BGR16l16l16lP_Raster_A<em>X</em>. */
  DEC_OUT_FRM_B16G16R16_P,
  /** RGB161616_Raster_A<em>X</em>. */
  DEC_OUT_FRM_R16G16B16,
  /* sw_ppx_out_rgb_fmt == 4 */
  /** ABGR8888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_ABGR888,
  /* sw_ppx_out_rgb_fmt == 5 */
  /** ARGB8888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_ARGB888,
  /* sw_ppx_out_rgb_fmt == 6 */
  /** ABGR2101010_Raster_A<em>X</em>. */
  DEC_OUT_FRM_A2B10G10R10,
  /* sw_ppx_out_rgb_fmt == 7 */
  /** ARGB2101010_Raster_A<em>X</em>. */
  DEC_OUT_FRM_A2R10G10B10,
  /* sw_ppx_out_rgb_fmt == 8 */
  /** XBGR8888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_XBGR888,
  /* sw_ppx_out_rgb_fmt == 9 */
  /** XRGB8888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_XRGB888,
  /** BGRA8888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_BGRA888,
  /** RGBA8888_Raster_A<em>X</em>. */
  DEC_OUT_FRM_RGBA888,
  /** BGRA1010102_Raster_A<em>X</em>. */
  DEC_OUT_FRM_B10G10R10A2,
  /** RGBA1010102_Raster_A<em>X</em>. */
  DEC_OUT_FRM_R10G10B10A2,
  /** XRGB2101010_Raster_A<em>X</em>. */
  DEC_OUT_FRM_X2R10G10B10,
  /** XBGR2101010_Raster_A<em>X</em>. */
  DEC_OUT_FRM_X2B10G10R10,
  /** RGB565_Raster_A<em>X</em>. */
  DEC_OUT_FRM_RGB565,
  /** BGR565_Raster_A<em>X</em>. */
  DEC_OUT_FRM_BGR565,
  /* super_tile */
  /** ABGR8888_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_ABGR888_TILED64X64,
  /** ARGB8888_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_ARGB888_TILED64X64,
  /** ABGR2101010_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_A2B10G10R10_TILED64X64,
  /** ARGB2101010_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_A2R10G10B10_TILED64X64,
  /** XBGR8888_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_XBGR888_TILED64X64,
  /** XRGB8888_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_XRGB888_TILED64X64,
  /** XBGR2101010_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_X2B10G10R10_TILED64X64,
  /** XRGB2101010_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_X2R10G10B10_TILED64X64,
  /** BGRA8888_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_BGRA888_TILED64X64,
  /** RGBA8888_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_RGBA888_TILED64X64,
  /** BGRA1010102_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_B10G10R10A2_TILED64X64,
  /** RGBA1010102_Tile64x64_A<em>X</em>. */
  DEC_OUT_FRM_R10G10B10A2_TILED64X64,
  /* Note: Please update the function of ParseDecPictureFormat in parselogmsg.c synchornously if changing this enum */
};

/** \brief A function that checks whether the given format is a packed RGB format, in which all
 *  components are packed together in the same plane.
 *  \ingroup common_group */
static inline u32 IS_PIC_PACKED_RGB(enum DecPictureFormat fmt)
{
  return (
    (fmt) == DEC_OUT_FRM_RGB888 || \
    (fmt) == DEC_OUT_FRM_BGR888 || \
    (fmt) == DEC_OUT_FRM_R16G16B16 || \
    (fmt) == DEC_OUT_FRM_B16G16R16 || \
    (fmt) == DEC_OUT_FRM_ARGB888 || \
    (fmt) == DEC_OUT_FRM_ABGR888 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_XRGB888 || \
    (fmt) == DEC_OUT_FRM_XBGR888 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_RGBA888 || \
    (fmt) == DEC_OUT_FRM_BGRA888 || \
	  (fmt) == DEC_OUT_FRM_RGB565 || \
    (fmt) == DEC_OUT_FRM_BGR565 || \
    (fmt) == DEC_OUT_FRM_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64);
}

/** \brief A function that checks whether the given format is a planar RGB format, in which R, G,
 *  and B components are stored in three separate planes.
 *  \ingroup common_group */
static inline u32 IS_PIC_PLANAR_RGB(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_RGB888_P || \
    (fmt) == DEC_OUT_FRM_BGR888_P || \
    (fmt) == DEC_OUT_FRM_R16G16B16_P || \
    (fmt) == DEC_OUT_FRM_B16G16R16_P);
}

/** \brief A function that checks whether the given format is an RGB format, either packed or
 *  planar.
 *  \ingroup common_group */
static inline u32 IS_PIC_RGB(enum DecPictureFormat fmt)
{
  return (
    (fmt) == DEC_OUT_FRM_RGB888 || \
    (fmt) == DEC_OUT_FRM_BGR888 || \
    (fmt) == DEC_OUT_FRM_R16G16B16 || \
    (fmt) == DEC_OUT_FRM_B16G16R16 || \
    (fmt) == DEC_OUT_FRM_ARGB888 || \
    (fmt) == DEC_OUT_FRM_ABGR888 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_XRGB888 || \
    (fmt) == DEC_OUT_FRM_RGB888_P || \
    (fmt) == DEC_OUT_FRM_BGR888_P || \
    (fmt) == DEC_OUT_FRM_R16G16B16_P || \
    (fmt) == DEC_OUT_FRM_B16G16R16_P || \
    (fmt) == DEC_OUT_FRM_XBGR888 || \
    (fmt) == DEC_OUT_FRM_RGBA888 || \
    (fmt) == DEC_OUT_FRM_BGRA888 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_RGB565 || \
    (fmt) == DEC_OUT_FRM_BGR565 || \
    (fmt) == DEC_OUT_FRM_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64);
}

/** \brief A function that checks whether the given format includes an alpha component.
 *  \ingroup common_group */
static inline u32 IS_PIC_ALPHA_FORMAT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_ARGB888 || \
    (fmt) == DEC_OUT_FRM_ABGR888 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_RGBA888 || \
    (fmt) == DEC_OUT_FRM_BGRA888 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10_TILED64X64 ||\
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64);
}

/** \brief A function that checks whether the given format is an RGB format with an X component.
 *  \ingroup common_group */
static inline u32 IS_PIC_XRGB_FORMAT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_XRGB888 || \
    (fmt) == DEC_OUT_FRM_XBGR888 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64);
}

/** \brief A function that checks whether the given format is a monochroma format, which includes
 *  only a Y component.
 *  \ingroup common_group */
static inline u32 IS_PIC_MONOCHROME(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_MONOCHROME || \
    (fmt) == DEC_OUT_FRM_YUV400TILE || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_I010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_L010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_P012 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_I012 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8 || \
    (fmt) == DEC_OUT_FRM_YUV400 || \
    (fmt) == DEC_OUT_FRM_YUV400_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400_I010 || \
    (fmt) == DEC_OUT_FRM_YUV400_L010 || \
    (fmt) == DEC_OUT_FRM_YUV400_P012 || \
    (fmt) == DEC_OUT_FRM_YUV400_I012 || \
    (fmt) == DEC_OUT_FRM_YUV400_PACK10 || \
    (fmt) == DEC_OUT_FRM_DEC400_400SP || \
    (fmt) == DEC_OUT_FRM_DEC400_400P || \
    (fmt) == DEC_OUT_FRM_DEC400_400TILE || \
    (fmt) == DEC_OUT_FRM_DEC400_400TILED8x8 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_128X2 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_128X2_P010);
}

/** \brief A function that checks whether the given format is a YUV420 format, in which the
 *  U and V components have half the height and half the width of the Y component.
 *  \ingroup common_group */
static inline u32 IS_PIC_YCbCr420(enum DecPictureFormat fmt)
{
  return (((fmt) >= DEC_OUT_FRM_NV21SP && \
   (fmt) <= DEC_OUT_FRM_NV21P_1010) || \
   ((fmt) >= DEC_OUT_FRM_YUV420SP && \
   (fmt) <= DEC_OUT_FRM_YUV420P_1010) || \
   (fmt) == DEC_OUT_FRM_PVFBC_420SP || \
   (fmt) == DEC_OUT_FRM_PVFBC_420SP_P010);
}

/** \brief A function that checks whether the given format is a YUV422 format, in which the
 *  U and V components have the same height and half the width of the Y component.
 *  \ingroup common_group */
static inline u32 IS_PIC_YCbCr422(enum DecPictureFormat fmt)
{
  return (((fmt) >= DEC_OUT_FRM_YUV422SP && \
     (fmt) <= DEC_OUT_FRM_YUV422_UYVY) || \
     ((fmt) >= DEC_OUT_FRM_422_NV21SP && \
     (fmt) <= DEC_OUT_FRM_422_NV21P_I012));
}

/** \brief A funciton that checks whether the given format is a YUV444 format, in which the
 *  U and V components have the same height and width of the Y component.
 *  \ingroup common_group */
static inline u32 IS_PIC_YCbCr444(enum DecPictureFormat fmt)
{
  return (((fmt) >= DEC_OUT_FRM_YUV444SP && \
     (fmt) <= DEC_OUT_FRM_YUV444P_I012) || \
     ((fmt) >= DEC_OUT_FRM_444_NV21SP && \
     (fmt) <= DEC_OUT_FRM_444_NV21P_I012));
 }

/** \brief A function that checks whether the given format is a planar YUV format, in which each
 *  component is stored in a separate buffer.
 *  \ingroup common_group */
static inline u32 IS_PIC_PLANAR(enum DecPictureFormat fmt)
{
  //cppcheck-suppress duplicateExpression
  return ((fmt) == DEC_OUT_FRM_PLANAR_420 || \
    (fmt) == DEC_OUT_FRM_YUV420P || \
    (fmt) == DEC_OUT_FRM_YUV420P_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV422P_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV420P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_I010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_L010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_P012 || \
    (fmt) == DEC_OUT_FRM_YUV420P_I012 || \
    (fmt) == DEC_OUT_FRM_YUV420P_1010 || \
    (fmt) == DEC_OUT_FRM_NV21P || \
    (fmt) == DEC_OUT_FRM_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21P_PACK12 || \
    (fmt) == DEC_OUT_FRM_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_NV21P_L010 || \
    (fmt) == DEC_OUT_FRM_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_NV21P_1010 || \
    (fmt) == DEC_OUT_FRM_YUV422P || \
    (fmt) == DEC_OUT_FRM_YUV422P_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV422P_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV422P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV422P_I010 || \
    (fmt) == DEC_OUT_FRM_YUV422P_L010 || \
    (fmt) == DEC_OUT_FRM_YUV422P_P012 || \
    (fmt) == DEC_OUT_FRM_YUV422P_I012 || \
    (fmt) == DEC_OUT_FRM_422_NV21P || \
    (fmt) == DEC_OUT_FRM_422_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_PACK12 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_L010 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_YUV444P || \
    (fmt) == DEC_OUT_FRM_YUV444P_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV444P_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV444P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV444P_I010 || \
    (fmt) == DEC_OUT_FRM_YUV444P_L010 || \
    (fmt) == DEC_OUT_FRM_YUV444P_P012 || \
    (fmt) == DEC_OUT_FRM_YUV444P_I012 || \
    (fmt) == DEC_OUT_FRM_444_NV21P || \
    (fmt) == DEC_OUT_FRM_444_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_PACK12 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_L010 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_DEC400_400P || \
    (fmt) == DEC_OUT_FRM_DEC400_420P || \
    IS_PIC_PLANAR_RGB(fmt));
}

/** \brief A function that checks whether the given field format is a planar YUV format, in which
 *  each component is stored in a separate buffer.
 *  \ingroup common_group */
static inline u32 IS_PIC_FIELD_PLANAR(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_PLANAR_420 || \
    (fmt) == DEC_OUT_FRM_YUV420P || \
    (fmt) == DEC_OUT_FRM_YUV420P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_I010 );
}

/** \brief A function that checks whether the given format is a 1010 format, in which the three
 *  10-bit components of a pixel are stored in 32 bits with 2 bits pending.
 *  \ingroup common_group */
static inline u32 IS_PIC_1010(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_NV21P_1010 || \
   (fmt) == DEC_OUT_FRM_NV21SP_1010 || \
   (fmt) == DEC_OUT_FRM_YUV420P_1010 || \
   (fmt) == DEC_OUT_FRM_YUV420SP_1010 || \
   (fmt) == DEC_OUT_FRM_YUV422SP_1010 || \
   (fmt) == DEC_OUT_FRM_422_NV21SP_1010);
}

/** \brief A function that checks whether the given format is an NV21 format, in which the U and V
 *  components of a pixel are stored in VU interleaved mode, like V0U0V1U1V2U2...
 *  \ingroup common_group */
static inline u32 IS_PIC_NV21(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_NV21TILE || \
    (fmt) == DEC_OUT_FRM_NV21TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_P010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_I010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_L010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_P012 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_I012 || \
    (fmt) == DEC_OUT_FRM_NV21SP || \
    (fmt) == DEC_OUT_FRM_NV21SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21SP_P010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_I010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_L010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_P012 || \
    (fmt) == DEC_OUT_FRM_NV21SP_I012 || \
    (fmt) == DEC_OUT_FRM_NV21P || \
    (fmt) == DEC_OUT_FRM_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_NV21P_L010 );
}

/** \brief A function that checks whether the given format is a semi-planar format, in which the U
 *  and V components of a pixel are stored in interleaved mode, like V0U0V1U1V2U2... or U0V0U1V1U2V2...
 *  \ingroup common_group */
static inline u32 IS_PIC_SEMIPLANAR(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_YUV420SP || \
    (fmt) == DEC_OUT_FRM_YUV420SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_I010 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_L010 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_P012 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_I012 || \
    (fmt) == DEC_OUT_FRM_NV21SP || \
    (fmt) == DEC_OUT_FRM_NV21SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21SP_P010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_I010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_L010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_P012 || \
    (fmt) == DEC_OUT_FRM_NV21SP_I012 || \
    (fmt) == DEC_OUT_FRM_RASTER_SCAN || \
    (fmt) == DEC_OUT_FRM_PVFBC_420SP || \
    (fmt) == DEC_OUT_FRM_PVFBC_420SP_P010);
}

/** \brief A function that checks whether the component of each pixel is of 8 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_8BIT(enum DecPictureFormat fmt)
{
  //cppcheck-suppress duplicateExpression
  return ((fmt) == DEC_OUT_FRM_YUV420TILE || \
    (fmt) == DEC_OUT_FRM_YUV420SP || \
    (fmt) == DEC_OUT_FRM_YUV420P || \
    (fmt) == DEC_OUT_FRM_YUV422SP || \
    (fmt) == DEC_OUT_FRM_YUV422P || \
    (fmt) == DEC_OUT_FRM_YUV444SP || \
    (fmt) == DEC_OUT_FRM_YUV444P || \
    (fmt) == DEC_OUT_FRM_YUV400TILE || \
    (fmt) == DEC_OUT_FRM_YUV400 || \
    (fmt) == DEC_OUT_FRM_NV21TILE || \
    (fmt) == DEC_OUT_FRM_NV21SP || \
    (fmt) == DEC_OUT_FRM_NV21P || \
    (fmt) == DEC_OUT_FRM_422_NV21SP || \
    (fmt) == DEC_OUT_FRM_422_NV21P || \
    (fmt) == DEC_OUT_FRM_444_NV21SP || \
    (fmt) == DEC_OUT_FRM_444_NV21P || \
    (fmt) == DEC_OUT_FRM_RGB888_P || \
    (fmt) == DEC_OUT_FRM_BGR888_P || \
    (fmt) == DEC_OUT_FRM_RGB888 || \
    (fmt) == DEC_OUT_FRM_BGR888 || \
    (fmt) == DEC_OUT_FRM_ARGB888 || \
    (fmt) == DEC_OUT_FRM_ABGR888 || \
    (fmt) == DEC_OUT_FRM_XRGB888 || \
    (fmt) == DEC_OUT_FRM_XBGR888 || \
    (fmt) == DEC_OUT_FRM_RGBA888 || \
    (fmt) == DEC_OUT_FRM_BGRA888 || \
    (fmt) == DEC_OUT_FRM_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_TILED_8X8 || \
    (fmt) == DEC_OUT_FRM_TILED_16X16 || \
    (fmt) == DEC_OUT_FRM_TILED_16X16_COMP || \
    (fmt) == DEC_OUT_FRM_TILED_32X8_COMP || \
    (fmt) == DEC_OUT_FRM_TILED_64X64 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_128X2 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8 || \
    (fmt) == DEC_OUT_FRM_YUV422_YUYV || \
    (fmt) == DEC_OUT_FRM_YUV422_UYVY || \
    (fmt) == DEC_OUT_FRM_YUV422_YVYU || \
    (fmt) == DEC_OUT_FRM_YUV422_VYUY || \
    (fmt) == DEC_OUT_FRM_PVFBC_420SP);
}

/** \brief A function that checks whether the component of each pixel is of 10 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_10BIT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_YUV420SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV420P_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV422SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV422P_PACK10 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV444SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV444P_PACK10 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_PACK10 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_PACK10 || \
    (fmt) == DEC_OUT_FRM_TILED_64X64_PACK10);
}

/** \brief A function that checks whether the component of each pixel is of 12 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_12BIT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_YUV420SP_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV400_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV420P_PACK12 || \
    (fmt) == DEC_OUT_FRM_NV21SP_PACK12 || \
    (fmt) == DEC_OUT_FRM_NV21P_PACK12  || \
    (fmt) == DEC_OUT_FRM_YUV422SP_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV422P_PACK12 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_PACK12 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_PACK12  || \
    (fmt) == DEC_OUT_FRM_YUV444SP_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV444P_PACK12 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_PACK12 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8_PACK12);
}

/** \brief A function that checks whether the component of each pixel is of 16 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_16BIT(enum DecPictureFormat fmt)
{
  return (
    (fmt) == DEC_OUT_FRM_YUV420TILE_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_P012 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_P012 || \
    (fmt) == DEC_OUT_FRM_YUV420P_P012 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_I010 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_I010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_I010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_I012 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_I012 || \
    (fmt) == DEC_OUT_FRM_YUV420P_I012 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_L010 || \
    (fmt) == DEC_OUT_FRM_YUV420SP_L010 || \
    (fmt) == DEC_OUT_FRM_YUV420P_L010 || \
    (fmt) == DEC_OUT_FRM_YUV422SP_P010 || \
    (fmt) == DEC_OUT_FRM_YUV422P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV422SP_P012 || \
    (fmt) == DEC_OUT_FRM_YUV422P_P012 || \
    (fmt) == DEC_OUT_FRM_YUV422SP_I010 || \
    (fmt) == DEC_OUT_FRM_YUV422P_I010 || \
    (fmt) == DEC_OUT_FRM_YUV422SP_I012 || \
    (fmt) == DEC_OUT_FRM_YUV422P_I012 || \
    (fmt) == DEC_OUT_FRM_YUV422SP_L010 || \
    (fmt) == DEC_OUT_FRM_YUV422P_L010 || \
    (fmt) == DEC_OUT_FRM_YUV444SP_P010 || \
    (fmt) == DEC_OUT_FRM_YUV444P_P010 || \
    (fmt) == DEC_OUT_FRM_YUV444SP_P012 || \
    (fmt) == DEC_OUT_FRM_YUV444P_P012 || \
    (fmt) == DEC_OUT_FRM_YUV444SP_I010 || \
    (fmt) == DEC_OUT_FRM_YUV444P_I010 || \
    (fmt) == DEC_OUT_FRM_YUV444SP_I012 || \
    (fmt) == DEC_OUT_FRM_YUV444P_I012 || \
    (fmt) == DEC_OUT_FRM_YUV444SP_L010 || \
    (fmt) == DEC_OUT_FRM_YUV444P_L010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_P012 || \
    (fmt) == DEC_OUT_FRM_YUV400_P012 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_I010 || \
    (fmt) == DEC_OUT_FRM_YUV400_I010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_I012 || \
    (fmt) == DEC_OUT_FRM_YUV400_I012 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_L010 || \
    (fmt) == DEC_OUT_FRM_YUV400_L010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_P010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_P010 || \
    (fmt) == DEC_OUT_FRM_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_P012 || \
    (fmt) == DEC_OUT_FRM_NV21SP_P012 || \
    (fmt) == DEC_OUT_FRM_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_I010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_I010 || \
    (fmt) == DEC_OUT_FRM_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_I012 || \
    (fmt) == DEC_OUT_FRM_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_L010 || \
    (fmt) == DEC_OUT_FRM_NV21SP_L010 || \
    (fmt) == DEC_OUT_FRM_NV21P_L010 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_P010 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_P012 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_I010 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_I012 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_422_NV21SP_L010 || \
    (fmt) == DEC_OUT_FRM_422_NV21P_L010 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_P010 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_P010 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_P012 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_P012 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_I010 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_I010 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_I012 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_I012 || \
    (fmt) == DEC_OUT_FRM_444_NV21SP_L010 || \
    (fmt) == DEC_OUT_FRM_444_NV21P_L010 || \
    (fmt) == DEC_OUT_FRM_R16G16B16_P || \
    (fmt) == DEC_OUT_FRM_B16G16R16_P || \
    (fmt) == DEC_OUT_FRM_R16G16B16 || \
    (fmt) == DEC_OUT_FRM_B16G16R16 || \
    (fmt) == DEC_OUT_FRM_RGB565 || \
    (fmt) == DEC_OUT_FRM_BGR565 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_128X2_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_128X2_P010 || \
    (fmt) == DEC_OUT_FRM_PVFBC_420SP_P010);
}

/** \brief A function that checks whether the three RGB components of each pixel are stored in
 *  packed 24 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_24BIT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_RGB888 || \
    (fmt) == DEC_OUT_FRM_BGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGB888);
}

/** \brief A function that checks whether the three RGB components with an extra alpha or X
 *  component of each pixel are stored in packed 32 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_32BIT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_ARGB888 || \
    (fmt) == DEC_OUT_FRM_ABGR888 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_XRGB888 || \
    (fmt) == DEC_OUT_FRM_XBGR888 || \
    (fmt) == DEC_OUT_FRM_RGBA888 || \
    (fmt) == DEC_OUT_FRM_BGRA888 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64);
}

/** \brief A function that checks whether the three RGB components of each pixel are stored in
 *  packed 48 bits.
 *  \ingroup common_group */
static inline u32 IS_PIC_48BIT(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_R16G16B16 || \
    (fmt) == DEC_OUT_FRM_B16G16R16);
}

/** \brief A function that checks whether the component of each plane is stored in tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILE(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_4X4 || \
  (fmt) == DEC_OUT_FRM_TILED_8X4 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE || \
  (fmt) == DEC_OUT_FRM_YUV420TILE_P010 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE_I010 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE_L010 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE_P012 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE_I012 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE_PACK10 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE || \
  (fmt) == DEC_OUT_FRM_YUV400TILE_P010 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE_I010 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE_L010 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE_P012 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE_I012 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE_PACK10 || \
  (fmt) == DEC_OUT_FRM_NV21TILE || \
  (fmt) == DEC_OUT_FRM_NV21TILE_PACK10 || \
  (fmt) == DEC_OUT_FRM_NV21TILE_P010 || \
  (fmt) == DEC_OUT_FRM_NV21TILE_I010 || \
  (fmt) == DEC_OUT_FRM_NV21TILE_L010 || \
  (fmt) == DEC_OUT_FRM_NV21TILE_P012 || \
  (fmt) == DEC_OUT_FRM_NV21TILE_I012 || \
  (fmt) == DEC_OUT_FRM_DEC400_400TILE || \
  (fmt) == DEC_OUT_FRM_DEC400_420TILE || \
  (fmt) == DEC_OUT_FRM_YUV400TILE8x8_PACK10 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE8x8_P010 || \
  (fmt) == DEC_OUT_FRM_YUV400TILE8x8 || \
  (fmt) == DEC_OUT_FRM_YUV420TILE8x8 || \
  (fmt) == DEC_OUT_FRM_DEC400_400TILED8x8 || \
  (fmt) == DEC_OUT_FRM_DEC400_420TILED8x8 || \
  (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
  (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64);
}

/** \brief A function that checks whether the component of each plane is stored in an RFC
 *  compressed format.
 *  \ingroup common_group */
static inline u32 IS_PIC_RFC(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_RFC);
}

/** \brief A function that checks whether the component of each plane is stored in a DEC400
 *  compressed format.
 *  \ingroup common_group */
static inline u32 IS_PIC_DEC400(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_DEC400_400SP || \
    (fmt) == DEC_OUT_FRM_DEC400_420SP || \
    (fmt) == DEC_OUT_FRM_DEC400_400P || \
    (fmt) == DEC_OUT_FRM_DEC400_420P || \
    (fmt) == DEC_OUT_FRM_DEC400_400TILE || \
    (fmt) == DEC_OUT_FRM_DEC400_420TILE || \
    (fmt) == DEC_OUT_FRM_DEC400_400TILED8x8 || \
    (fmt) == DEC_OUT_FRM_DEC400_420TILED8x8 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_422PACKED);
}

/** \brief A function that checks whether the component of each plane is stored in a PVFBC
 *  compressed format.
 *  \ingroup common_group */
static inline u32 IS_PIC_PVFBC(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_PVFBC_420SP || \
    (fmt) == DEC_OUT_FRM_PVFBC_420SP_P010);
}

/** \brief A function that checks whether the component of each plane is stored in 4x4 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILED4x4(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_4X4 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_I010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_L010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_P012 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE_I012 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_I010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_L010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_P012 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE_I012 || \
    (fmt) == DEC_OUT_FRM_NV21TILE || \
    (fmt) == DEC_OUT_FRM_NV21TILE_PACK10 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_P012 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_I010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_L010 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_I012 || \
    (fmt) == DEC_OUT_FRM_NV21TILE_P010);
}

/** \brief A function that checks whether the component of each plane in the refernce buffer is
 *  stored in 8x8 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_REF_TILED8x8(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_YUV420TILE8x8 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8_PACK10 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8_PACK12 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_P010 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_PACK10);
}

/** \brief A function that checks whether the component of each plane is stored in 8x8 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILED8x8(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_8X8 || \
    (fmt) == DEC_OUT_FRM_TILED_8X8_P010 ||	\
    (fmt) == DEC_OUT_FRM_DEC400_400TILED8x8 || \
    (fmt) == DEC_OUT_FRM_DEC400_420TILED8x8 || \
    (fmt) == DEC_OUT_FRM_YUV400TILE8x8_P010 || \
    (fmt) == DEC_OUT_FRM_YUV420TILE8x8);
}

/** \brief A function that checks whether the component of each plane is stored in 16x16 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILED16x16(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_16X16 || (fmt) == DEC_OUT_FRM_TILED_16X16_P010);
}

/** \brief A function that checks whether the component of each plane is stored in third-party
 *  compressed 16x16 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILED16x16_COMP(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_16X16_COMP || (fmt) == DEC_OUT_FRM_TILED_16X16_P010_COMP);
}

/** \brief A function that checks whether the component of each plane is stored in third-party
 *  compressed 32x8 tiles.
 * \ingroup common_group */
static inline u32 IS_PIC_TILED32x8_COMP(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_32X8_COMP || (fmt) == DEC_OUT_FRM_TILED_32X8_P010_COMP);
}

/** \brief A function that checks whether the three components are stored in a packed YUV422
 *  format, either in YUYV or UYVY mode, or in a DEC400 compressed format.
 *  \ingroup common_group */
static inline u32 IS_PIC_422PACKED(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_YUV422_YUYV || \
    (fmt) == DEC_OUT_FRM_YUV422_UYVY || \
    (fmt) == DEC_OUT_FRM_YUV422_YVYU || \
    (fmt) == DEC_OUT_FRM_YUV422_VYUY || \
    (fmt) == DEC_OUT_FRM_DEC400_422PACKED);
}

/** \brief A function that checks whether the components of each plane are stored in 128x2 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILED128x2(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_YUV400TILE_128X2 || \
          (fmt) == DEC_OUT_FRM_YUV420TILE_128X2 || \
          (fmt) == DEC_OUT_FRM_YUV400TILE_128X2_P010 || \
          (fmt) == DEC_OUT_FRM_YUV420TILE_128X2_P010);
}

/** \brief A function that checks whether the components of each plane are stored in 64x64 tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_TILED64x64(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_TILED_64X64 || \
    (fmt) == DEC_OUT_FRM_TILED_64X64_PACK10);
}

/** \brief A function that checks whether the three RGB components are packed and stored in 64x64
 *  tiles.
 *  \ingroup common_group */
static inline u32 IS_PIC_RGB_TILED64x64(enum DecPictureFormat fmt)
{
  return ((fmt) == DEC_OUT_FRM_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_R10G10B10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ABGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_ARGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_A2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2B10G10R10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_X2R10G10B10_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XBGR888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_XRGB888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_BGRA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_RGBA888_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_B10G10R10A2_TILED64X64 || \
    (fmt) == DEC_OUT_FRM_DEC400_R10G10B10A2_TILED64X64);
}

/** \brief A function that checks whether each component of a standalone input buffer is stored in
 *  16-bit P010 format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_P010(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_400_P010) || \
  	(fmt == PP_IN_NV12_P010) || \
  	(fmt == PP_IN_NV21_P010) || \
  	(fmt == PP_IN_420P_P010) || \
  	(fmt == PP_IN_422_NV16_P010) || \
  	(fmt == PP_IN_422_NV61_P010) || \
  	(fmt == PP_IN_444P_P010) || \
  	(fmt == PP_IN_TILED4x4_P010));
}

/** \brief A function that checks whether each component of a standalone input buffer is stored in
 *  a planar YUV format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_YUV_PLANAR(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_420P_8BIT) || \
  	(fmt == PP_IN_420P_P010) || \
  	(fmt == PP_IN_444P_8BIT) || \
  	(fmt == PP_IN_444P_P010));
}

/** \brief A function that checks whether each component of a standalone input buffer is stored in
 *  4x4 tiles.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_TILED4x4(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_TILED4x4) || (fmt == PP_IN_TILED4x4_P010));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in 4x4 tiled
 *  packed RGB format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_RGB(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_RGB888) || \
	   (fmt == PP_IN_BGR888) || \
	   (fmt == PP_IN_ARGB888) || \
	   (fmt == PP_IN_ABGR888) || \
	   (fmt == PP_IN_RGBA888) || \
	   (fmt == PP_IN_BGRA888) || \
	   (fmt == PP_IN_XRGB888) || \
	   (fmt == PP_IN_XBGR888) || \
	   (fmt == PP_IN_A2R10G10B10) || \
	   (fmt == PP_IN_A2B10G10R10) || \
	   (fmt == PP_IN_X2R10G10B10) || \
	   (fmt == PP_IN_X2B10G10R10) || \
	   (fmt == PP_IN_R10G10B10A2) || \
	   (fmt == PP_IN_B10G10R10A2) || \
	   (fmt == PP_IN_RGB888_P) || \
	   (fmt == PP_IN_BGR888_P) || \
	   (fmt == PP_IN_R16G16B16_P) || \
	   (fmt == PP_IN_B16G16R16_P));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a packed
 *  ARGB or XRGB format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_ALPHA_X_FROMAT_RGB(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_ARGB888) || \
    (fmt == PP_IN_ABGR888) || \
    (fmt == PP_IN_XRGB888) || \
    (fmt == PP_IN_XBGR888) || \
    (fmt == PP_IN_RGBA888) || \
    (fmt == PP_IN_BGRA888) || \
    (fmt == PP_IN_A2R10G10B10) || \
    (fmt == PP_IN_A2B10G10R10) || \
    (fmt == PP_IN_X2R10G10B10) || \
    (fmt == PP_IN_X2B10G10R10) || \
    (fmt == PP_IN_R10G10B10A2) || \
    (fmt == PP_IN_B10G10R10A2));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a planar
 *  RGB format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_RGB_PLANAR(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_RGB888_P) || \
	   (fmt == PP_IN_BGR888_P) || \
	   (fmt == PP_IN_R16G16B16_P) || \
	   (fmt == PP_IN_B16G16R16_P));
}

/** \brief A function that checks whether each component of a standalone input buffer is of 8
 *  valid bits.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_8BIT(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_400_8BIT) || \
     (fmt == PP_IN_NV12_8BIT) || \
     (fmt == PP_IN_NV21_8BIT) || \
     (fmt == PP_IN_420P_8BIT) || \
     (fmt == PP_IN_422_NV16_8BIT) || \
     (fmt == PP_IN_422_NV61_8BIT) || \
     (fmt == PP_IN_422_YUYV_8BIT) || \
     (fmt == PP_IN_422_YVYU_8BIT) || \
     (fmt == PP_IN_422_UYVY_8BIT) || \
     (fmt == PP_IN_422_VYUY_8BIT) || \
     (fmt == PP_IN_444P_8BIT) || \
     (fmt == PP_IN_TILED4x4) || \
     (fmt == PP_IN_RGB888) || \
     (fmt == PP_IN_BGR888) || \
     (fmt == PP_IN_ARGB888) || \
     (fmt == PP_IN_ABGR888) || \
     (fmt == PP_IN_XRGB888) || \
     (fmt == PP_IN_XBGR888) || \
     (fmt == PP_IN_RGBA888) || \
     (fmt == PP_IN_BGRA888) || \
     (fmt == PP_IN_RGB888_P) || \
     (fmt == PP_IN_BGR888_P));
}

/** \brief A function that checks whether each component of a standalone input buffer is of 10
 *  valid bits.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_10BIT(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_400_P010) || \
	   (fmt == PP_IN_NV12_P010) || \
	   (fmt == PP_IN_NV21_P010) || \
	   (fmt == PP_IN_420P_P010) || \
	   (fmt == PP_IN_422_NV16_P010) || \
	   (fmt == PP_IN_422_NV61_P010) || \
	   (fmt == PP_IN_444P_P010) || \
	   (fmt == PP_IN_TILED4x4_P010) || \
	   (fmt == PP_IN_A2R10G10B10) || \
	   (fmt == PP_IN_A2B10G10R10) || \
	   (fmt == PP_IN_X2R10G10B10) || \
	   (fmt == PP_IN_X2B10G10R10) || \
	   (fmt == PP_IN_R10G10B10A2) || \
	   (fmt == PP_IN_B10G10R10A2) || \
	   (fmt == PP_IN_R16G16B16_P) || \
	   (fmt == PP_IN_B16G16R16_P));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a packed
 *  YUV422 format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_422PACKED(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_422_YUYV_8BIT) || \
	   (fmt == PP_IN_422_YVYU_8BIT) || \
	   (fmt == PP_IN_422_UYVY_8BIT) || \
	   (fmt == PP_IN_422_VYUY_8BIT));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a
 *  semi-planar YUV format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_YUV_SEMIPLANAR(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_NV12_8BIT) || \
	   (fmt == PP_IN_NV21_8BIT) || \
	   (fmt == PP_IN_NV12_P010) || \
	   (fmt == PP_IN_NV21_P010) || \
	   (fmt == PP_IN_422_NV16_8BIT) || \
	   (fmt == PP_IN_422_NV16_P010) || \
	   (fmt == PP_IN_422_NV61_8BIT) || \
	   (fmt == PP_IN_422_NV61_P010));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a YUV420
 *  format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_YUV420(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_NV12_8BIT) || \
	   (fmt == PP_IN_NV21_8BIT) || \
	   (fmt == PP_IN_NV12_P010) || \
	   (fmt == PP_IN_NV21_P010) || \
	   (fmt == PP_IN_420P_8BIT) || \
	   (fmt == PP_IN_420P_P010));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a YUV422
 *  format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_YUV422(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_422_NV16_8BIT) || \
	   (fmt == PP_IN_422_NV16_P010) || \
	   (fmt == PP_IN_422_NV61_8BIT) || \
	   (fmt == PP_IN_422_NV61_P010) || \
	   (fmt == PP_IN_422_YUYV_8BIT) || \
	   (fmt == PP_IN_422_YVYU_8BIT) || \
	   (fmt == PP_IN_422_UYVY_8BIT) || \
	   (fmt == PP_IN_422_VYUY_8BIT));
}

/** \brief A function that checks whether data in a standalone input buffer is stored in a YUV444
 *  format.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_YUV444(enum DecPictureFormat fmt)
{
  return ((fmt == PP_IN_444P_8BIT) || (fmt == PP_IN_444P_P010));
}

/** \brief A function that checks whether data in a standalone input buffer is RFC compressed.
 *  \ingroup common_group */
static inline u32 IS_PP_IN_RFC(enum DecPictureFormat fmt)
{
  return (fmt == PP_IN_RFC);
}
/**@}*/

/** Defines picture coding types.
 *  \ingroup common_group */
enum DecPicCodingType {
  /** Intra coded picture. */
  DEC_PIC_TYPE_I           = 0,
  /** P-picture, which is a picture coded using intra or inter prediction with at most one motion
   *  vector and reference index to predict the sample values of each block. */
  DEC_PIC_TYPE_P           = 1,
  /** B picture, which is a picture coded using intra or inter prediction with at most two motion
   *  vectors and reference indices to predict the sample values of each block. */
  DEC_PIC_TYPE_B           = 2,
  /** D-Picture in MPEG-2. */
  DEC_PIC_TYPE_D           = 3,
  /** Forced intra coded picture in RealVideo. */
  DEC_PIC_TYPE_FI          = 4,
  /** BI picture in VC-1. A BI picture is a B-picture where all the macroblocks are intra-coded. */
  DEC_PIC_TYPE_BI          = 5
};

/** Defines error handling modes.
 *  \ingroup common_group */
enum DecErrorHandling {
  /** Clarity-first mode, which outputs only correct pictures.
   *  \n Correct pictures refer to pictures with no stream errors in decoding. All referenced
   *  pictures are correct pictures. */
  DEC_EC_FRAME_NO_ERROR = 1,
  /** Fluency-first mode, which decodes as many pictures as possible and outputs all pictures
   *  including erroneous pictures. */
  DEC_EC_FRAME_IGNORE_ERROR = 2,
  /** Fluency-clarity balanced mode, which outputs only good pictures.
   *  \n Good pictures refer to both correct pictures and erroneous pictures with the error ratio
   *  less than <tt>DecInitConfig.error_ratio</tt>.
   *  \n All good frames are used as reference and output. Erroneous frames with the error ratio
   *  exceeding \c DecInitConfig.error_ratio are replaced by the nearest good frame in decoding. */
  DEC_EC_FRAME_TOLERANT_ERROR = 3
};

/** Defines stride alignments.
 *  \ingroup common_group */
typedef enum {
  /** 1-byte alignment. */
  DEC_ALIGN_1B = 0,
  /** 8-byte alignment. */
  DEC_ALIGN_8B = 3,
  /** 16-byte alignment. */
  DEC_ALIGN_16B,
  /** 32-byte alignment. */
  DEC_ALIGN_32B,
  /** 64-byte alignment. */
  DEC_ALIGN_64B,
  /** 128-byte alignment. */
  DEC_ALIGN_128B,
  /** 256-byte alignment. */
  DEC_ALIGN_256B,
  /** 512-byte alignment. */
  DEC_ALIGN_512B,
  /** 1024-byte alignment. */
  DEC_ALIGN_1024B,
  /** 2048-byte alignment. */
  DEC_ALIGN_2048B,
} DecPicAlignment;

/** \brief Contains delogo filter configurations.
 *  \ingroup common_group */
typedef struct _DelogoConfig {
  /** \brief Whether to enable the delogo filter.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 enabled;
  /** \brief The X coordinate. */
  u32 x;
  /** \brief The Y coordinate. */
  u32 y;
  /** \brief The width. */
  u32 w;
  /** \brief The height. */
  u32 h;
  /** \brief The delogo filter level. */
  u32 show;
  /** \brief The delogo filter mode. */
  enum DelogoMode mode;
  /** \brief The Y value. */
  u32 Y;
  /** \brief The U value. */
  u32 U;
  /** \brief The V value. */
  u32 V;
} DelogoConfig;

/** \brief Contains configurations of a cropping region for post-processing.
 *  \ingroup common_group */
struct CropParams {
  /** \brief Whether to enable cropping of the region.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 enabled;
  /** \brief Whether the configurations of the region are user specified.
   *  \n Valid values:
   *  \n - <tt>0</tt>: the configurations are not user specified.
   *  \n - <tt>1</tt>: the configurations are user specified. */
  u32 set_by_user;
  /** \brief The start X coordinate of the region. */
  u32 x;
  /** \brief The start Y coordinate of the region. */
  u32 y;
  /** \brief The width of the region. */
  u32 width;
  /** \brief The height of the region. */
  u32 height;
};

/** \brief Contains alpha blending configurations of a pixel processing instance.
 *  \ingroup common_group */
struct BlendParams {
  /** \brief Whether to enable alpha blending.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u8  enable;
  /** \brief The alpha factor for blending. */
  u32 alpha;
  /** \brief The start X coordinate for blending. */
  u32 x;
  /** \brief The start Y coordinate for blending. */
  u32 y;
};

/** \brief Contains configurations of a post-processing channel.
 *  \ingroup common_group */
typedef struct _PpUnitConfig {
  /** \brief Whether to enable the post-processing channel.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 enabled;
  /** \brief Whether to enable 4x4 tiled output.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 tiled_e;
  /** \brief Whether to enable RGB output.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 rgb;
  /** \brief Whether to enable planar RGB output.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 rgb_planar;
  /** \brief Whether to output the chroma components in VU order instead of UV.
   *  \n Valid values:
   *  \n - <tt>0</tt>: UV order.
   *  \n - <tt>1</tt>: VU order. */
  u32 cr_first;
  /** \brief Whether to enable the hardware shaper feature.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 shaper_enabled;
  /** \brief Whether to disallow the shaper to pad at the frame right edge.
   *  \n Valid values:
   *  \n - <tt>0</tt>: allow.
   *  \n - <tt>1</tt>: disallow. */
  u32 shaper_no_pad;
  /** \brief The alignment for DEC400 compression.
   *  \n Valid values: <tt> \ref DEC_ALIGN_32B</tt> and <tt> \ref DEC_ALIGN_64B</tt>. */
  DecPicAlignment dec400_align;
  /** \brief Whether to enable planar output.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 planar;
  /** \brief The alignment of the post-processing output. */
  DecPicAlignment align;
  /** \brief The height alignment of the post-processing output. */
  DecPicAlignment align_h;
  /** \brief The width alignment of the post-processing output. */
  u32 align_pixel_w;
  /** \brief The stride for the Y plane.
   *  \n If this field is set to a non-zero value, the software checks the validity of the value.
   *  If the value is valid, the software uses the value as the stride.
   *  \n If this field is set to <tt>0</tt>, the software uses the stride that is automatically
   *  calculated. */
  u32 ystride;
  /** \brief The stride for the UV plane.
   *  \n If this field is set to a non-zero value, the software checks the validity of the value.
   *  If the value is valid, the software uses the value as the stride.
   *  \n If this field is set to <tt>0</tt>, the software uses the stride that is automatically
   *  calculated. */
  u32 cstride;

  /** \brief The configurations of region 0 for cropping before scaling. */
  struct CropParams crop;
  /** \brief The configurations of region 1 for cropping before scaling. */
  struct CropParams crop11;
  /** \brief The configurations of region 2 for cropping before scaling. */
  struct CropParams crop12;
  /** \brief The configurations of region 3 for cropping before scaling. */
  struct CropParams crop13;
  /** \brief The cropping region ID. */
  u32 crop_id;

  struct {
    /** \brief Whether to enable cropping after scaling. */
    u32 enabled;
    /** \brief The start X coordinate of the cropping region. */
    u32 x;
    /** \brief The start Y coordinate of the cropping region. */
    u32 y;
    /** \brief The width of the cropping region. */
    u32 width;
    /** \brief The height of the cropping region. */
    u32 height;
  } crop2;

  struct {
    /** \brief Whether to enable scaling. */
    u32 enabled;
    /** \brief Whether to determine the scaled output size is calulcated based on ratio
     *  configurations. */
    u32 scale_by_ratio;
    /** \brief Value <tt>1</tt>/<tt>2</tt>/<tt>4</tt>/<tt>8</tt>: down-scaling ratio of 1/ratio_x.
     *  Value <tt>0</tt>: scaled output size set by width/height. */
    u32 ratio_x;
    /** \brief Value <tt>1</tt>/<tt>2</tt>/<tt>4</tt>/<tt>8</tt>: down-scaling ratio of 1/ratio_y.
     *  Value <tt>0</tt>: scaled output size set by width/height. */
    u32 ratio_y;
    /** \brief The width of the scaled output. */
    u32 width;
    /** \brief The height of the scaled output. */
    u32 height;
  } scale;

  struct {
    /** \brief Value <tt>0</tt>: no padding.
     *  Value <tt>1</tt>: padding with edge pixel data.
     *  Value <tt>2</tt>: padding with a fixed pixel. */
    u32 mode;
    /** \brief The Y or R component of the fixed YUV or RGB pixel value for padding. */
    u32 r_y;
    /** \brief The U or G component of the fixed YUV or RGB pixel value for padding. */
    u32 g_u;
    /** \brief The V or B component of the fixed YUV or RGB pixel value for padding. */
    u32 b_v;
    /** \brief The alpha component of the fixed RGB pixel value for padding. */
    u32 a;
    /** \brief The left padding width. */
    u32 l_off;
    /** \brief The right padding width. */
    u32 r_off;
    /** \brief The top padding height. */
    u32 t_off;
    /** \brief The bottom padding height. */
    u32 b_off;
  } pad;

  /** \brief Whether to output data in a monochrome (Y-only) format.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a monochrome format.
   *  \n - <tt>1</tt>: output data in a monochrome format. */
  u32 monochrome;
  /** \brief Whether to output data in a YUV P010 format.
   *  \n For details about P010, see <em>Vivante GC820T Series Memory Buffer and Format
   *  Organization</em>.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a YUV P010 format.
   *  \n - <tt>1</tt>: output data in a YUV P010 format. */
  u32 out_p010;
  /** \brief Whether to output data in a YUV 1010 format, where each component uses 10 bits.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a YUV 1010 format.
   *  \n - <tt>1</tt>: output data in a YUV 1010 format. */
  u32 out_1010;
  /** \brief Whether to output data in a YUV I010 format.
   *  \n For details about I010, see <em>Vivante GC820T Series Memory Buffer and Format
   *  Organization</em>.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a YUV I010 format.
   *  \n - <tt>1</tt>: output data in a YUV I010 format. */
  u32 out_I010;
  /** \brief Whether to output data in a YUV L010 format.
   *  \n For details about L010, see <em>Vivante GC820T Series Memory Buffer and Format
   *  Organization</em>.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a YUV L010 format.
   *  \n - <tt>1</tt>: output data in a YUV L010 format. */
  u32 out_L010;
  /** \brief Whether to output data in a YUV P012 format.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a YUV P012 format.
   *  \n - <tt>1</tt>: output data in a YUV P012 format. */
  u32 out_p012;
  /** \brief Whether to output data in a YUV I012 format.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in a YUV I012 format.
   *  \n - <tt>1</tt>: output data in a YUV I012 format. */
  u32 out_I012;
  /** \brief (Deprecated). */
  u32 out_be;
  /** \brief Whether to force 8-bit output.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not force 8-bit output.
   *  \n - <tt>1</tt>: force 8-bit output. */
  u32 out_cut_8bits;
  /** \brief Whether to output data in the YUYV format, which is a packed YUV422 format.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in the YUYV format.
   *  \n - <tt>1</tt>: output data in the YUYV format. */
  u32 out_yuyv;
  /** \brief Whether to output data in the UYVY format, which is a packed YUV422 format.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not output data in the UYVY format.
   *  \n - <tt>1</tt>: output data in the UYVY format. */
  u32 out_uyvy;
  /** \brief Whether to use the full or limited pixel value range.
   *  \n Valid values:
   *  \n - <tt>0</tt>: limited range.
   *  \n - <tt>1</tt>: full range. */
  u32 video_range;
  /** \brief The maximum pixel value corresponding to the video range. */
  u32 range_max;
  /** \brief The minimum pixel value corresponding to the video range. */
  u32 range_min;
  /** \brief The RGB output format. */
  u32 rgb_format;
  /** \brief The color space conversion standard.
   *  \n The standard can be <tt> \ref BT601</tt>, <tt> \ref BT601_L</tt>, <tt> \ref BT709</tt>,
   *  <tt> \ref BT709_L</tt>, <tt> \ref BT2020</tt>, and <tt> \ref BT2020_L</tt>. */
  u32 rgb_stan;

  /** \brief Set the source yuv/rgb range for range mapping.
   *  \n Valid values:
   *  \n - <tt>0</tt>: limited range.
   *  \n - <tt>1</tt>: full range. */
  u32 source_range;
  /** \brief Whether \c PpUnitConfig.source_range is valid.
   *  \n Valid values:
   *  \n - <tt>0</tt>: invalid
   *  \n - <tt>1</tt>: valid. */
  u32 set_source_range_enable;
  /** \brief Set the target yuv/rgb range for range mapping.
   *  \n Valid values:
   *  \n - <tt>0</tt>: limited range.
   *  \n - <tt>1</tt>: full range. */
  u32 target_range;
  /** \brief Whether \c PpUnitConfig.target_range is valid.
   *  \n Valid values:
   *  \n - <tt>0</tt>: invalid
   *  \n - <tt>1</tt>: valid. */
  u32 set_target_range_enable;
  /** \brief Whether to enable dithering.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 dither_enable;
  /** \brief Whether to enable 3D LUT for color remapping.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u8 enable_3dlut;

  /** \brief The alpha value for output ARGB pixels in YUV-to-RGB conversion. */
  u32 rgb_alpha;
  /** \brief The scaling algorithm.
   *  \n Valid values: <tt> \ref VSI_LINEAR</tt>, <tt> \ref LANCZOS</tt>, <tt> \ref NEAREST</tt>,
   *  <tt> \ref BI_LINEAR</tt>, <tt> \ref BICUBIC</tt>, <tt> \ref SPLINE</tt>, <tt> \ref BOX</tt>,
   *  <tt> \ref FAST_LINEAR</tt>, <tt> \ref FAST_BICUBIC</tt>, and <tt> \ref AREA</tt>. */
  u32 pp_filter;
  /** \brief The horizontal window for the Lanczos algorithm. */
  u32 x_filter_param;
  /** \brief The vertical window for the Lanczos algorithm. */
  u32 y_filter_param;
  /** \brief The tiled mode when tiled output is enabled. */
  u32 tile_mode;
  /** \brief Whether to enable compression.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 comp_enabled;
  /** \brief The constant luma value 0 for VFBC. */
  u32 pp_pvfbc_lu_const0;
  /** \brief The constant luma value 1 for VFBC. */
  u32 pp_pvfbc_lu_const1;
  /** \brief The constant chroma value 0 for VFBC. */
  u32 pp_pvfbc_ch_const0;
  /** \brief The constant chroma value 1 for VFBC. */
  u32 pp_pvfbc_ch_const1;
  /** \brief The algorithm used to select the center point for scaling.
   *  \n Valid values: <tt> \ref DOWN_ROUND</tt>, <tt> \ref NO_ROUND</tt>, and
   *  <tt> \ref UP_ROUND</tt>.
   *  \n Default value: <tt> \ref DOWN_ROUND</tt>. */
  u32 src_sel_mode;
  /** \brief Whether to pad a fixed YUV value at picture edges in scaling.
   *  \n Valid values:
   *  \n - <tt>0</tt>: do not pad a fixed YUV value.
   *  \n - <tt>1</tt>: pad a fixed YUV value. */
  u32 pad_sel;
  /** \brief The Y component of the fixed YUV value for padding in scaling. */
  u32 pad_Y;
  /** \brief The U component of the fixed YUV value for padding in scaling. */
  u32 pad_U;
  /** \brief The V component of the fixed YUV value for padding in scaling. */
  u32 pad_V;
  /** \brief The YUV output format.
   *  \n Valid values: <tt> \ref PP_YUV400</tt>, <tt> \ref PP_YUV420</tt>,
   *  <tt> \ref PP_YUV422</tt>, and <tt> \ref PP_YUV444</tt>. */
  u32 chroma_format;
  /** \brief The number of lines to trigger the line counter interrupt. */
  u32 lc_stripe;
  /** \brief The beginning offset for luma data write in the post-processing output buffer for
   *  HEIF. */
  u32 luma_offset;
  /** \brief The beginning offset for chroma data write in the post-processing output buffer for
   *  HEIF. */
  u32 chroma_offset;
  /** \brief The offset of setting luma and chroma data write offset in the post-processing output
   *  buffer by TB for HEIF. */
  u32 buf_off_set_by_user;
  /** \brief Whether the filter size changes with the scaling ratio.
   *  \n Valid values:
   *  \n - <tt>0</tt>: does not change.
   *  \n - <tt>1</tt>: changes. */
  u32 antialias;
} PpUnitConfig;
#endif /* DECAPICOMMON_H */
