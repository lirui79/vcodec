/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
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

#ifndef __JPEGDECAPI_H__
#define __JPEGDECAPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "basetype.h"
#include "commonfunction.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "dwl.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/
/*
#define JPEG_BASELINE 0x0
#define JPEG_PROGRESSIVE 0x1
#define JPEG_EXTENDED 0x2
#define JPEG_NONINTERLEAVED 0x3
*/

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/
typedef void *JpegDecInst;

enum {
  TYPE_IFD0 = 1,
  TYPE_EXIFSUBIFD,
  TYPE_IFD1,
};

typedef struct {
  u32 value;
  const char* name;
} ExifMetaDataDescriptor;

static const ExifMetaDataDescriptor FlashWay[] = {
  {0, "Flash did not fire"},
  {0x1, "Flash fired"},
  {0x5, "Strobe return light not detected"},
  {0x7, "Strobe return light detected"},
  {0x9, "Flash fired, compulsory flash mode"},
  {0xd, "Flash fired, compulsory flash mode, return light not detect"},
  {0xf, "Flash fired, compulsory flash mode, return light detected"},
  {0x10, "Flash did not fire, compulsory flash mode"},
  {0x18, "Flash did not fire, auto mode"},
  {0x19, "Flash fired, auto mode"},
  {0x1d, "Flash fired, auto mode, return light not detected"},
  {0x1f, "Flash fired, auto mode, return light detected"},
  {0x20, "No flash function"},
  {0x41, "Flash fired, red-eye reduction mode"},
  {0x45, "Flash fired, red-eye reduction mode, return light not detected"},
  {0x47, "Flash fired, red-eye reduction mode, return light detected"},
  {0x49, "Flash fired, compulsory flash mode, red-eye reduction mode"},
  {0x4d, "Flash fired, compulsory flash mode, red-eye reduction mode, return light not detected"},
  {0x4f, "Flash fired, compulsory flash mode, red-eye reduction mode, return light detected"},
  {0x59, "Flash fired, auto mode, red-eye reduction mode"},
  {0x5d, "Flash fired, auto mode, return light not detected, red-eye reduction mode"},
  {0x5f, "Flash fired, auto mode, return light detected, red-eye reduction mode"}
};

static const ExifMetaDataDescriptor LightSoure[] = {
  {0, "Unknown"},
  {1, "Daylight"},
  {2, "Fluorescent"},
  {3, "Tungsten (incandescent light)"},
  {4, "Flash"},
  {9, "Fine weather"},
  {10, "Cloudy weather"},
  {11, "Shade"},
  {12, "Daylight fluorescent (D 5700 7100K)"},
  {13, "Day white fluorescent (N 4600  5400K)"},
  {14, "Cool white fluorescent (W 3900 4500K)"},
  {15, "White fluorescent (WW 3200 3700K)"},
  {17, "Standard light A"},
  {18, "Standard light B"},
  {19, "Standard light C"},
  {20, "D55"},
  {21, "D65"},
  {22, "D75"},
  {23, "D50"},
  {24, "ISO studio tungsten"},
  {255, "other light source"},
};

static const ExifMetaDataDescriptor Compression[] = {
  {1, "uncompressed"},
  {6, "JPEG"},
};

static const ExifMetaDataDescriptor Resolution_unit[] = {
  {2, "inch"},
  {3, "cm"},
};

static const ExifMetaDataDescriptor ExposureProgram[] = {
  {0, "Undefined"},
  {1, "Manual"},
  {2, "Normal program"},
  {3, "Aperture priority"},
  {4, "Shutter priority"},
  {5, "Creative program"},
  {6, "Action program"},
  {7, "Portrait mode"},
  {8, "Landscape program"}
};

static const ExifMetaDataDescriptor MeteringMode[] = {
  {0, "Unknown"},
  {1, "Average"},
  {2, "CenterWeightedAverage"},
  {3, "Spot"},
  {4, "MultiSpot"},
  {5, "Pattern"},
  {6, "Partial"},
  {255, "other"},
};

static const ExifMetaDataDescriptor ColorSpace[] = {
  {1, "sRGB"},
  {0xffff, "Uncalibrated"}
};

/* Image information */
typedef struct {
  u32 display_width;
  u32 display_height;
  u32 output_width;    /* Number of pixels/line in the image  */
  u32 output_height;   /* Number of lines in in the image     */
  u32 bit_depth; /* luma & chroma */
  u32 version;
  u32 units;
  u32 x_density;
  u32 y_density;
  u32 output_format;   /* DEC_OUT_FRM_YUV400
                             * JPEGDEC_YCbCr420
                             * JPEGDEC_YCbCr422
                             */
  enum JpegCodingMode coding_mode; /* JPEG_BASELINE
                         * JPEG_PROGRESSIVE
                         * JPEG_NONINTERLEAVED
                         */

  u32 thumbnail_type;  /* Thumbnail exist or not or not supported */
  u32 display_width_thumb;
  u32 display_height_thumb;
  u32 output_width_thumb;   /* Number of pixels/line in the image  */
  u32 output_height_thumb;  /* Number of lines in in the image     */
  u32 output_format_thumb;  /* DEC_OUT_FRM_YUV400
                                 * JPEGDEC_YCbCr420
                                 * JPEGDEC_YCbCr422
                                 */
  u32 coding_mode_thumb;    /* JPEG_BASELINE
                                 * JPEG_PROGRESSIVE
                                 * JPEG_NONINTERLEAVED
                                 */
  DecPicAlignment align; /* alignment information, it is maybe default value defined inside ctrlSW,
                                               application uses it for output buffer allocation */

  u32 img_max_dec_width;
  u32 img_max_dec_height;
  struct ExifInfo exif_info;
#ifdef EXTRACT_THUMBNAIL_JPG
  FILE *out_thumbnail_file;
  u8* thumbnail_base;
  u32 thumbnail_len;
#endif
} JpegDecImageInfo;

/* Decoder input JFIF information */
typedef struct {
  struct DWLLinearMem stream_buffer;  /* input stream buffer */
  u32 stream_length;   /* input stream length */
  u32 buffer_size; /* input stream buffer size in input stream buffering mode */
  u32 dec_image_type;   /* Full image or Thumbnail to be decoded */
  u32 slice_mb_set; /* slice mode: mcu rows to decode */
  u32 ri_count;     /* count of restart intervals in input stream */
  u32 *ri_array;    /* offsets of beginnings of restart intervals */
  struct DWLLinearMem picture_buffer_y;    /* luma output address ==> if user allocated */
  struct DWLLinearMem picture_buffer_cb_cr; /* chroma output address ==> if user allocated */
  struct DWLLinearMem picture_buffer_cr; /* chroma output address ==> if user allocated */
  void *p_user_data;
  u8 *stream;  /* input stream bus address */
  u32 thumbnail_done;
  u32 dec_ctrl;
} JpegDecInput;

/* Decoder output */
typedef struct {
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
  u32 bit_depth;
  struct DecOutput output;
  struct JpegOutputInfo {
    struct DWLLinearMem output_picture_y;    /* Pointer to the Luma output image */
    struct DWLLinearMem output_picture_cb_cr; /* Pointer to the Chroma output image */
    struct DWLLinearMem output_picture_cr; /* Pointer to the Chroma output image */
    u32 output_width;           /* bytes in a pixel line (with padded bytes) */
    u32 output_height;
    u32 display_width;          /* valid pixels */
    u32 display_height;
    u32 output_width_thumb;
    u32 output_height_thumb;
    u32 display_width_thumb;
    u32 display_height_thumb;
    u32 pic_stride;
    u32 pic_stride_ch;
    enum DecPictureFormat output_format;
    struct DWLLinearMem dec400_luma_table;           /**< Buffer properties *//*sunny add for tile status address*/
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
  enum DecErrorInfo error_info;
  u32 error_ratio;
  u32 pic_id;
} JpegDecOutput;

struct JpegDecConfig {
  enum DecErrorHandling error_handling;
  u32 error_ratio;
  DecPicAlignment align;
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  DelogoConfig delogo_params[2];
  u32 dec_image_type;
  enum DecDecoderMode decoder_mode;
  u32 strm_status_in_buffer; /**< With low-latency decoding mode,
                             stream status is updated in buffer instead of registers. */
  struct DecMCConfig mcinit_cfg;
  enum DecPictureFormat chroma_format;
  u32 use_adaptive_buffers;
  u32 misc_ctrl;
};
/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

/* Initialization */
enum DecRet JpegDecInit(JpegDecInst *dec_inst, const void *dwl, struct JpegDecConfig *dec_cfg);

/* Release */
void JpegDecRelease(JpegDecInst dec_inst);

/* Get image information of the JFIF */
enum DecRet JpegDecGetImageInfo(JpegDecInst dec_inst,
                               JpegDecInput *p_dec_in,
                               JpegDecImageInfo *p_image_info);

/* Decode JFIF */
enum DecRet JpegDecDecode(JpegDecInst dec_inst,
                         JpegDecInput * p_dec_in, JpegDecOutput * p_dec_out);

/* Update HW in low latency mode */
void JpegDecUpdateStrmInfoCtrl(JpegDecInst dec_inst,
                                struct strmInfo info);


enum DecRet JpegDecNextPicture(JpegDecInst dec_inst,
                              JpegDecOutput * output,
                              JpegDecImageInfo *info);

enum DecRet JpegDecPictureConsumed(JpegDecInst dec_inst, JpegDecOutput * output);

enum DecRet JpegDecEndOfStream(JpegDecInst dec_inst);

enum DecRet JpegDecSetInfo(JpegDecInst dec_inst, struct JpegDecConfig *dec_cfg);

enum DecRet JpegDecGetBufferInfo(JpegDecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet JpegDecAddBuffer(JpegDecInst dec_inst, struct DWLLinearMem *info);
enum DecRet JpegDecAbort(JpegDecInst dec_inst);

enum DecRet JpegDecAbortAfter(JpegDecInst dec_inst);

/*------------------------------------------------------------------------------
    Prototype of the API trace funtion. Traces all API entries and returns.
    This must be implemented by the application using the decoder API!
    Argument:
        string - trace message, a null terminated string
------------------------------------------------------------------------------*/
void JpegDecTrace(const char *string);

#ifdef EXTRACT_THUMBNAIL_JPG
void JpegDecGetThumbnailJpeg(JpegDecInput * p_dec_in, JpegDecImageInfo * p_image_info);
#endif

#ifdef __cplusplus
}
#endif

#endif
