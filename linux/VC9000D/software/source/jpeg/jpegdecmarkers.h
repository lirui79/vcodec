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

#ifndef JPEGDECMARKERS_H
#define JPEGDECMARKERS_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/

/* JPEG markers, table B.1 page 32 */

enum {
  SOF0 = 0xC0,
  SOF1 = 0xC1,
  SOF2 = 0xC2,
  SOF3 = 0xC3,
  SOF5 = 0xC5,
  SOF6 = 0xC6,
  SOF7 = 0xC7,
  SOF9 = 0xC8,
  SOF10 = 0xCA,
  SOF11 = 0xCB,
  SOF13 = 0xCD,
  SOF14 = 0xCE,
  SOF15 = 0xCF,
  JPG = 0xC8,
  DHT = 0xC4,
  DAC = 0xCC,
  SOI = 0xD8,
  EOI = 0xD9,
  SOS = 0xDA,
  DQT = 0xDB,
  DNL = 0xDC,
  DRI = 0xDD,
  DHP = 0xDE,
  EXP = 0xDF,
  APP0 = 0xE0,
  APP1 = 0xE1,
  APP2 = 0xE2,
  APP3 = 0xE3,
  APP4 = 0xE4,
  APP5 = 0xE5,
  APP6 = 0xE6,
  APP7 = 0xE7,
  APP8 = 0xE8,
  APP9 = 0xE9,
  APP10 = 0xEA,
  APP11 = 0xEB,
  APP12 = 0xEC,
  APP13 = 0xED,
  APP14 = 0xEE,
  APP15 = 0xEF,
  JPG0 = 0xF0,
  JPG1 = 0xF1,
  JPG2 = 0xF2,
  JPG3 = 0xF3,
  JPG4 = 0xF4,
  JPG5 = 0xF5,
  JPG6 = 0xF6,
  JPG7 = 0xF7,
  JPG8 = 0xF8,
  JPG9 = 0xF9,
  JPG10 = 0xFA,
  JPG11 = 0xFB,
  JPG12 = 0xFC,
  JPG13 = 0xFD,
  COM = 0xFE,
  TEM = 0x01,
  RST0 = 0xD0,
  RST1 = 0xD1,
  RST2 = 0xD2,
  RST3 = 0xD3,
  RST4 = 0xD4,
  RST5 = 0xD5,
  RST6 = 0xD6,
  RST7 = 0xD7
};

enum {
  TAG_IMAGE_DISCRIPTION = 0x10e,
  TAG_ARTIST = 0x13b,
  TAG_CAMERA_MAKE = 0x10f,
  TAG_CAMERA_MODEL = 0x110,
  TAG_ORIENTATION = 0x112,
  TAG_XRESOLUTION = 0x11a,
  TAG_YRESOLUTION = 0x11b,
  TAG_RESOLUTION_UNIT = 0x128,
  TAG_SOFTWARE = 0x131,
  TAG_DATETIME = 0x132,
  TAG_YCBCR_POSITIONING = 0x213,
  TAG_EXIFOFFSET = 0x8769,

  TAG_EXPOSURE_TIME = 0x829a,
  TAG_FNUMBER = 0x829d,
  TAG_EXPOSURE_PROGRAM = 0x8822,
  TAG_ISOSPEEDRATINGS = 0x8827,
  TAG_EXIF_VERSION = 0x9000,
  TAG_DATETIME_ORIGINAL = 0x9003,
  TAG_DATETIME_DIGITIZED = 0x9004,
  TAG_EXPOSURE_BIAS_VALUE = 0x9204,
  TAG_MAXAPERTURE_VALUE = 0x9205,
  TAG_METERING_MODE = 0x9207,
  TAG_LIGHTSOURCE = 0x9208,
  TAG_FLASH = 0x9209,
  TAG_FOCALLENGTH = 0x920a,
  TAG_COLORSPACE = 0xa001,
  TAG_EXIF_IMAGEWIDTH = 0xa002,
  TAG_EXIF_IMAGEHEIGHT = 0xa003,

  TAG_IMAGEWIDTH = 0x100,
  TAG_IMAGEHEIGHT = 0x101,
  TAG_COMPRESSION = 0x103,
  TAG_PHOTOMETRIC_INTERPRETATION = 0x106,
  TAG_JPEGIF_OFFSET = 0x201,
  TAG_JPEGIF_BYTECOUNT = 0x202,
  TAG_YCBCR_COEFFICIENTS = 0x211,
};
/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

#endif /* #ifdef JPEGDECMARKERS_H */
