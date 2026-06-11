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

#include <OMX_Component.h>
#include <OMX_Core.h>
#include <OMX_Types.h>

/* std includes */
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* test client */
#include "omxtestcommon.h"
#include "util.h"
#include "version.h"

/*
  struct for transferring parameter definitions
  between functions
 */
typedef struct OMXDECODER_PARAMETERS
{
  OMX_STRING infile;
  OMX_STRING outfile;
  OMX_STRING varfile;

  OMX_U32 buffer_count;
  OMX_U32 buffer_size;

  OMX_BOOL image_input;
  OMX_BOOL splitted_output;

  int rotation;
  OMX_MIRRORTYPE mirror;

  OMX_U32 num_to_decode;
  OMX_U32 input_width;
  OMX_U32 input_height;
  char suffix[10];
  OMX_BOOL rcv_input;

  OMX_U32 custom_width;
  OMX_U32 custom_height;

  OMX_BOOL cropping;
  OMX_S32 crop_x;
  OMX_S32 crop_y;
  OMX_U32 crop_width;
  OMX_U32 crop_height;
  OMX_BOOL is_scale_Q16;
  OMX_S32 scale_x;
  OMX_S32 scale_y;
  OMX_BOOL mc_enable;
} OMXDECODER_PARAMETERS;

/* parameters instance */
OMXDECODER_PARAMETERS parameters;

int arg_count;

char **arguments;

/*
  print usage
 */
void print_usage(OMX_STRING swname)
{
    OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                   "usage: %s <input-compression-format> <input-file> <output-file> <input-width> <input-height > [options]\n"
                   "\n"
                   "  Available options:\n"
                   "    -i, --input-file                        Input file\n"
                   "    -o, --output-file                       Output file\n"
                   "    -I, --input-compression-format          Compression format: 'avc/h264/264'\n"
                   "                                                                'hevc/h265/265'\n"
                   "                                                                'vp9'\n"
                   "                                                                'avs2'\n"
                   "                                                                'av1'\n"
                   "                                                                'vvc'\n"
                   "                                                                'mpeg4'\n"
                   "                                                                'sorenson'\n"
                   "                                                                'h263'\n"
                   "                                                                'divx3'\n"
                   "                                                                'divx' (or divx4/divx5/divx6)\n"
                   "                                                                'jpeg'\n"
                   "                                                                'mjpeg'\n"
                   "                                                                'avs',\n"
                   "                                                                'wmv/vc1/rcv'\n"
                   "                                                                'vp8'\n"
                   "                                                                'vp7'\n"
                   "                                                                'vp6'\n"
                   "                                                                'rv'\n"
                   "                                                                'webp'\n"
                   "    -iw, --input-width                      Width of input picture\n"
                   "    -ih, --input-height                     Height of input picture\n"
                   "    -cx, --crop-x-coordinate                Coordinate of crop x\n"
                   "    -cy, --crop-y-coordinate                Coordinate of crop y\n"
                   "    -cw, --crop-width                       Width of crop\n"
                   "    -ch, --crop-height                      Height of crop\n"
                   "    -sw, --scale-width                      Width of scale\n"
                   "    -sh, --scale-height                     Height of scale\n"
                   "    -sQ16, --scale-Q16-enabled              Enable Q16 for option '-sw/-sh'\n"
                   "    note: if use '-sQ16', the scale ratio(Q16) corresponding to sw/sh value is:\n "
                   "                                            0x4000  --> 0.25\n"
                   "                                            0x8000  --> 0.5\n"
                   "                                            0xC000  --> 0.75\n"
                   "                                            0x10000 --> 1\n"
                   "                                            0x20000 --> 2\n"
                   "                                            0x40000 --> 4\n"
                   "                                            0x80000 --> 8\n"
                   "    -num, --num-to-decode                   The frame num need to be decoded\n"
                   "    -cusw, --divx3-custom-width             DIVX3 width\n"
                   "    -cush, --divx3-custom-heigh             DIVX3 height\n"
                   "\n"
                   "  Following options are not supported for vc9000d:\n"
                   "    -O, --output-color-format               Color format for output; 'YUV420Planar', 'YUV420SemiPlanar',\n"
                   "    -e, --error-concealment                 Use error concealment\n"
                   "    -x, --output-width                      Width of output image\n"
                   "    -y, --output-height                     Height of output image\n"
                   "    -s, --buffer-size                       Size of allocated buffers (640*480*3/2) \n"
                   "    -c, --buffer-count                      count of buffers allocated for each port (30)\n"
                   "    -v, --buffer-variance-file              Name of variance file\n"
                   "    -r, --rotation                          Rotation value, angle in degrees\n"
                   "    -m, --mirror                            Mirroring, 1=horizontal, 2=vertical, 3=both\n"
                   "    -D, --write-buffers                     Write each buffer additionally to a separate file\n"
                   "\n"
                   "  Return value:\n"
                   "    0  OK\n"
                   "    -  Failures indicated as OMX error codes\n"
                   "\n"
                   "  For example:\n"
                   "    ./omxdectest -I hevc -i ./stream.hevc -o output.yuv -iw 1920 -ih 1080 -num 10\n"
                   "\n"
                   , swname);
}

/*
  Macro that is used specifically to check parameter values
  in parameter checking loop.
 */
#define OMXDECODER_CHECK_NEXT_VALUE(i, args, argc, msg) \
  if(++(i) == argc || (*((args)[i])) == '-')  { \
    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, msg); \
    return OMX_ErrorBadParameter; \
  }

OMX_ERRORTYPE process_parameters(int argc, char **args,
                                 OMXDECODER_PARAMETERS * params)
{
  OMX_S32 i;
  OMX_STRING files[3] = { 0 };  // input, output, input-variance

  params->infile = 0;
  params->outfile = 0;

  params->rotation = 0;
  params->mirror = OMX_MirrorMax;
  params->num_to_decode = 0;

  i = 1;
  while(i < argc)
  {

    if(strcmp(args[i], "-v") == 0 ||
       strcmp(args[i], "--input-variance-file") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for input variance file missing.\n");
      files[2] = args[i];
    }
    else if(strcmp(args[i], "-i") == 0 ||
            strcmp(args[i], "--input-file") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for input file is missing.\n");
      files[0] = args[i];
    }
    else if(strcmp(args[i], "-o") == 0 ||
            strcmp(args[i], "--output-file") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for output file is missing.\n");
      files[1] = args[i];
    }
    else if(strcmp(args[i], "-I") == 0 ||
            strcmp(args[i], "--input-compression-format") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for input compression format missing.\n");
      strcpy(parameters.suffix, args[i]);
      if(strcmp(args[i], "jpeg") == 0)
      {
        parameters.image_input = OMX_TRUE;
      } else if (strcmp(args[i], "webp") == 0) {
        strcpy(parameters.suffix, "vp8");
        parameters.image_input = OMX_TRUE;
      } else if (strcmp(args[i], "rm") == 0) {
        strcpy(parameters.suffix, "rv");
      }
    }
    else if(strcmp(args[i], "-s") == 0 ||
            strcmp(args[i], "--buffer-size") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for buffer size missing.\n");
      params->buffer_size = atoi(args[i]);
    }
    else if(strcmp(args[i], "-c") == 0 ||
            strcmp(args[i], "--buffer-count") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for buffer count is missing.\n");
      params->buffer_count = atoi(args[i]);
    }
    else if(strcmp(args[i], "-r") == 0 ||
            strcmp(args[i], "--rotation") == 0)
    {
      if(++i == argc)
      {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                       "Parameter for rotation is missing.\n");
        return OMX_ErrorBadParameter;
      }
      params->rotation = atoi(args[i]);
    }
    else if(strcmp(args[i], "-m") == 0 || strcmp(args[i], "--mirror") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for mirroring is missing.\n");
      switch (atoi(args[i]))
      {
      case 1:
        params->mirror = OMX_MirrorHorizontal;
        break;
      case 2:
        params->mirror = OMX_MirrorVertical;
        break;
      case 3:
        params->mirror = OMX_MirrorBoth;
        break;
      default:
        params->mirror = OMX_MirrorNone;
        break;
      }
    }
    else if(strcmp(args[i], "-D") == 0 ||
            strcmp(args[i], "--write-buffers") == 0)
    {
      params->splitted_output = OMX_TRUE;
    }
    else if (strcmp(args[i], "-num") == 0 ||
             strcmp(args[i], "--num-to-decode") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for decode output number is missing.\n");
      params->num_to_decode = atoi(args[i]);
    }
    else if (strcmp(args[i], "-iw") == 0 ||
             strcmp(args[i], "--input-width") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                    "Parameter for input width is missing.\n");
      params->input_width = atoi(args[i]);
    }
    else if (strcmp(args[i], "-ih") == 0 ||
             strcmp(args[i], "--input-height") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for input height is missing.\n");
      params->input_height = atoi(args[i]);
    }
    else if (strcmp(args[i], "-cusw") == 0 ||
             strcmp(args[i], "--divx3-custom-width") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for custom width is missing.\n");
      params->custom_width = atoi(args[i]);
    }
    else if (strcmp(args[i], "-cush") == 0 ||
             strcmp(args[i], "--divx3-custom-height") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for custom height is missing.\n");
      params->custom_height = atoi(args[i]);
    }
    else if (strcmp(args[i], "-cx") == 0 ||
             strcmp(args[i], "--crop-X-coordinate") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for crop X coordinate is missing.\n");
      params->crop_x = atoi(args[i]);
    }
    else if (strcmp(args[i], "-cy") == 0 ||
             strcmp(args[i], "--crop-Y-coordinate") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for crop Y coordinate is missing.\n");
      params->crop_y = atoi(args[i]);
    }
    else if (strcmp(args[i], "-cw") == 0 ||
             strcmp(args[i], "--crop-width") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for crop width is missing.\n");
      params->crop_width = atoi(args[i]);
    }
    else if (strcmp(args[i], "-ch") == 0 ||
             strcmp(args[i], "--crop-height") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for crop height is missing.\n");
      params->crop_height = atoi(args[i]);
    }
    else if (strcmp(args[i], "-sQ16") == 0 ||
             strcmp(args[i], "--scale-Q16-enabled") == 0)
    {
      params->is_scale_Q16 = OMX_TRUE;
    }
    else if (strcmp(args[i], "-sw") == 0 ||
             strcmp(args[i], "--scale-width") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for scale width is missing.\n");
      char *endptr;
      int base = 10;
      if (args[i][0] == '0' && (args[i][1] == 'x' || args[i][1] == 'X')) {
        base = 16;
      }
      params->scale_x = strtol(args[i], &endptr, base);
    }
    else if (strcmp(args[i], "-sh") == 0 ||
             strcmp(args[i], "--scale-height") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for scale height is missing.\n");
      char *endptr;
      int base = 10;
      if (args[i][0] == '0' && (args[i][1] == 'x' || args[i][1] == 'X')) {
        base = 16;
      }
      params->scale_y = strtol(args[i], &endptr, base);
    }
    else if (strcmp(args[i], "-mc") == 0 ||
             strcmp(args[i], "--multicore") == 0)
    {
      params->mc_enable = OMX_TRUE;
    }
    if (params->crop_width > 0 && params->crop_height > 0) {
      params->cropping = OMX_TRUE;
    }
    ++i;
  }

  if(files[0] == 0)
  {
    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, "No input file.\n");
    return OMX_ErrorBadParameter;
  }

  if(files[1] == 0)
  {
    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR, "No output file.\n");
    return OMX_ErrorBadParameter;
  }

  params->infile = files[0];
  params->outfile = files[1];
  params->varfile = files[2];

  return OMX_ErrorNone;
}

OMX_ERRORTYPE process_output_parameters(int argc, char **args,
                                        OMX_PARAM_PORTDEFINITIONTYPE * params)
{
  OMX_S32 i;
  i = 1;
  OMX_COLOR_FORMATTYPE eColorFormat;
  while(i < argc)
  {
    /* output color format */
    if(strcmp(args[i], "-O") == 0 ||
       strcmp(args[i], "--output-color-format") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for output color format missing.\n");
      if(strcasecmp(args[i], "YUV420Planar") == 0)
        eColorFormat = OMX_COLOR_FormatYUV420PackedPlanar;
      else if(strcasecmp(args[i], "YUV420SemiPlanar") == 0)
        eColorFormat = OMX_COLOR_FormatYUV420PackedSemiPlanar;
      else if(strcasecmp(args[i], "YCbCr") == 0)
        eColorFormat = OMX_COLOR_FormatYCbYCr;
      else if(strcasecmp(args[i], "16bitARGB4444") == 0)
        eColorFormat = OMX_COLOR_Format16bitARGB4444;
      else if(strcasecmp(args[i], "16bitARGB1555") == 0)
        eColorFormat = OMX_COLOR_Format16bitARGB1555;
      else if(strcasecmp(args[i], "16bitRGB565") == 0)
        eColorFormat = OMX_COLOR_Format16bitRGB565;
      else if(strcasecmp(args[i], "16bitBGR565") == 0)
        eColorFormat = OMX_COLOR_Format16bitBGR565;
      else if(strcasecmp(args[i], "32bitBGRA8888") == 0)
        eColorFormat = OMX_COLOR_Format32bitBGRA8888;
      else if(strcasecmp(args[i], "32bitARGB8888") == 0)
        eColorFormat = OMX_COLOR_Format32bitARGB8888;
      else
      {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                      "Unknown output color format.\n");
        return OMX_ErrorBadParameter;
      }
      if (params->eDomain == OMX_PortDomainVideo) {
        params->format.video.eColorFormat = eColorFormat;
      } else if (params->eDomain == OMX_PortDomainImage) {
        params->format.image.eColorFormat = eColorFormat;
      }
    }
    else if(strcmp(args[i], "-x") == 0 ||
            strcmp(args[i], "--output-width") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for output width is missing.\n");
      if (params->eDomain == OMX_PortDomainVideo) {
        params->format.video.nFrameWidth = atoi(args[i]);
      } else if (params->eDomain == OMX_PortDomainImage) {
        params->format.image.nFrameWidth = atoi(args[i]);
      }
    }
    else if(strcmp(args[i], "-y") == 0 ||
            strcmp(args[i], "--output-height") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for output height is missing.\n");
      if (params->eDomain == OMX_PortDomainVideo) {
        params->format.video.nFrameHeight = atoi(args[i]);
      } else if (params->eDomain == OMX_PortDomainImage) {
        params->format.image.nFrameHeight = atoi(args[i]);
      }
    }
    else
    {
      /* do nothing */
    }
    ++i;
  }


  return OMX_ErrorNone;
}

/*
*/
OMX_ERRORTYPE process_input_parameters(int argc, char **args,
                                       OMX_PARAM_PORTDEFINITIONTYPE * params)
{
  OMX_S32 i;         // input, output, input-variance

  i = 1;
  while(i < argc)
  {

    /* input compression format */
    if (strcmp(args[i], "-I") == 0 ||
        strcmp(args[i], "--input-compression-format") == 0)
    {
      OMXDECODER_CHECK_NEXT_VALUE(i, args, argc,
                                  "Parameter for input compression format missing.\n");

      if (strcasecmp(args[i], "avc") == 0 ||
          strcasecmp(args[i], "h264") == 0 ||
          strcasecmp(args[i], "264") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingAVC;
      else if (strcasecmp(args[i], "hevc") == 0 ||
               strcasecmp(args[i], "h265") == 0 ||
               strcasecmp(args[i], "265") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingHEVC;
      else if (strcasecmp(args[i], "vp9") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingVP9;
      else if (strcasecmp(args[i], "avs2") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingAVS2;
      else if (strcasecmp(args[i], "av1") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingAV1;
      else if (strcasecmp(args[i], "vvc") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingVVC;
      else if (strcasecmp(args[i], "mpeg4") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG4;
      else if (strcasecmp(args[i], "sorenson") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingSORENSON;
      else if (strcasecmp(args[i], "h263") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingH263;
      else if (strcasecmp(args[i], "divx") == 0 ||
               strcasecmp(args[i], "divx4") == 0 ||
               strcasecmp(args[i], "divx5") == 0 ||
               strcasecmp(args[i], "divx6") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingDIVX;
      else if (strcasecmp(args[i], "divx3") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingDIVX3;
      else if (strcasecmp(args[i], "mjpeg") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingMJPEG;
      else if (strcasecmp(args[i], "avs") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingAVS;
      else if (strcasecmp(args[i], "wmv") == 0 ||
           strcasecmp(args[i], "vc1") == 0 ||
           strcasecmp(args[i], "rcv") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingWMV;
      else if (strcasecmp(args[i], "mpeg2") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingMPEG2;
      else if (strcasecmp(args[i], "vp8") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingVP8;
      else if (strcasecmp(args[i], "vp7") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingVP7;
      else if (strcasecmp(args[i], "vp6") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingVP6;
      else if (strcasecmp(args[i], "rv") == 0)
        params->format.video.eCompressionFormat = OMX_VIDEO_CodingRV;
      else if (strcmp(args[i], "jpeg") == 0)
        params->format.image.eCompressionFormat = OMX_IMAGE_CodingJPEG;
      else if (strcmp(args[i], "webp") == 0) {
        params->format.image.eCompressionFormat = OMX_IMAGE_CodingWEBP;
      }
      else
      {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                       "Unknown input compression format.\n");
        return OMX_ErrorBadParameter;
      }
    }
    else if (strcmp(args[i], "-e") == 0 ||
             strcmp(args[i], "--error-concealment") == 0)
    {
      if (!parameters.image_input) {
        params->format.video.bFlagErrorConcealment = OMX_TRUE;
      } else {
        params->format.image.bFlagErrorConcealment = OMX_TRUE;
      }
    }
    else
    {
      /* do nothing */
    }
    ++i;
  }

  return OMX_ErrorNone;
}

/*
  omx_decoder_port_initialize
 */
OMX_ERRORTYPE omx_decoder_port_initialize(OMXCLIENT * appdata,
                                          OMX_PARAM_PORTDEFINITIONTYPE * p)
{
  UNUSED_PARAMETER(appdata);
  OMX_ERRORTYPE omxError;

  omxError = OMX_ErrorNone;
  switch (p->eDir)
  {
    /* CASE IN: initialize input port */
  case OMX_DirInput:

    OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                   "Using port at index %i as input port\n", p->nPortIndex);

    OMXCLIENT_RETURN_ON_ERROR(process_input_parameters
                              (arg_count, arguments, p), omxError);

    p->nBufferCountActual = parameters.buffer_count;

    /*if(parameters.buffer_size) {
     * p->nBufferSize = parameters.buffer_size;
     * } */

    break;

    /* CASE OUT: initialize output port */
  case OMX_DirOutput:

    OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                   "Using port at index %i as output port\n",
                   p->nPortIndex);
    OMXCLIENT_RETURN_ON_ERROR(process_output_parameters
                              (arg_count, arguments, p), omxError);

    p->nBufferCountActual = parameters.buffer_count;

    /*if(parameters.buffer_size) {
     * p->nBufferSize = parameters.buffer_size;
     * } */
    break;

  case OMX_DirMax:
  default:
    break;
  }

  return omxError;
}
#define Q16 65536
/*
  main
 */
int main(int argc, char **args)
{
  OMXCLIENT client;

  OMX_ERRORTYPE omxError;

  OMX_U32 *variance_array;

  OMX_U32 variance_array_len;

  memset(&parameters, 0, sizeof(OMXDECODER_PARAMETERS));

  arg_count = argc;
  arguments = args;

  parameters.buffer_size = 0;
  parameters.buffer_count = 6;

  omxError = process_parameters(argc, args, &parameters);
  if(omxError != OMX_ErrorNone)
  {
    print_usage(args[0]);
    return omxError;
  }

  variance_array_len = 0;
  variance_array = 0;

  if(parameters.varfile &&
     (omxError = omxclient_read_variance_file(&variance_array, &variance_array_len,
                                              parameters.varfile)) != OMX_ErrorNone)
  {
    return omxError;
  }

  omxError = OMX_Init();
  if(omxError == OMX_ErrorNone)
  {
    char name[20] = "video_decoder.";
    strcat(name, parameters.suffix);
    if(parameters.image_input)
    {
      omxError =
        omxclient_component_create(&client,
                                   COMPONENT_NAME_IMAGE,
                                   name,
                                   parameters.buffer_count);
    }
    else
    {
      omxError =
        omxclient_component_create(&client,
                                   COMPONENT_NAME_VIDEO,
                                   name,
                                   parameters.buffer_count);
    }

    client.store_buffers = parameters.splitted_output;
    client.output_name = parameters.outfile;
    client.num_to_decode = parameters.num_to_decode;
    client.input_width = parameters.input_width;
    client.input_height = parameters.input_height;
    client.custom_width = parameters.custom_width;
    client.custom_height = parameters.custom_height;
    client.mc_enable = parameters.mc_enable;
    if (!parameters.is_scale_Q16) {
      client.scale_width = parameters.scale_x;
      client.scale_height = parameters.scale_y;
    }
    if(omxError == OMX_ErrorNone)
    {
      omxError = omxclient_check_component_version(client.component);
    }
    else
    {
      OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                     "Component creation failed: '%s'\n",
                     OMX_OSAL_TraceErrorStr(omxError));
    }

    if(omxError == OMX_ErrorNone)
    {
      if(parameters.image_input)
        omxError =
          omxclient_component_initialize_image(&client,
                                               &omx_decoder_port_initialize);
      else
        omxError =
          omxclient_component_initialize(&client,
                                         &omx_decoder_port_initialize,
                                         parameters.infile);

      if(omxError == OMX_ErrorNone)
      {

         /* omxError =
          omxlclient_initialize_buffers_fixed(&client,
                            parameters.buffer_size,
                            parameters.
                            buffer_count); */

        /* re-allocation of buffers after this is not properly defined, can't rely on port
         * configuration */

        omxError = omxclient_initialize_buffers(&client);
      }
      else if (omxError == OMX_ErrorBadParameter) {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                     "Failed: don't support this format! '%s'\n",
                     OMX_OSAL_TraceErrorStr(omxError));
        return omxError;
      }
      else
      {
        OMX_ERRORTYPE error = omxclient_component_free_buffers(&client);
        if (error != OMX_ErrorNone)
        {
          OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                         "FreeBuffer failed: %s\n",
                         OMX_OSAL_TraceErrorStr(omxError));
        }
      }

      /* set output crop */
      if(parameters.crop_width > 0 && parameters.crop_height > 0) {

        OMX_CONFIG_RECTTYPE rect;

        omxclient_struct_init(&rect, OMX_CONFIG_RECTTYPE);

        if (parameters.crop_x < 0) {
          parameters.crop_x = 0;
          OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                         "rect x could not be less than 0, forcing to 0!\n");
        }
        if (parameters.crop_y < 0) {
          parameters.crop_y = 0;
          OMX_OSAL_Trace(OMX_OSAL_TRACE_INFO,
                         "rect y could not be less than 0, forcing to 0!\n");
        }
        rect.nPortIndex = 1;
        rect.nLeft      = parameters.crop_x;
        rect.nTop       = parameters.crop_y;
        rect.nWidth     = parameters.crop_width;
        rect.nHeight    = parameters.crop_height;

        if((omxError =
          OMX_SetConfig(client.component, OMX_IndexConfigCommonOutputCrop,
                        &rect)) != OMX_ErrorNone)
        {

          OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                         "rect could not be set: %s\n",
                         OMX_OSAL_TraceErrorStr(omxError));
          return omxError;
        }
       }

      /* set output scale */
      if(parameters.scale_x > 0 && parameters.scale_y > 0 && parameters.is_scale_Q16 > 0) {

        OMX_CONFIG_SCALEFACTORTYPE scale;

        omxclient_struct_init(&scale, OMX_CONFIG_SCALEFACTORTYPE);

        scale.nPortIndex = 1;
        scale.xWidth = parameters.scale_x;
        scale.xHeight = parameters.scale_y;

        if((omxError =
          OMX_SetConfig(client.component, OMX_IndexConfigCommonScale,
                        &scale)) != OMX_ErrorNone)
        {

          OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                         "scale could not be set: %s\n",
                         OMX_OSAL_TraceErrorStr(omxError));
          return omxError;
        }
      }

      /* set rotation */
      if(parameters.rotation != 0)
      {

        OMX_CONFIG_ROTATIONTYPE rotation;

        omxclient_struct_init(&rotation, OMX_CONFIG_ROTATIONTYPE);

        rotation.nPortIndex = 1;
        rotation.nRotation = parameters.rotation;

        if((omxError =
          OMX_SetConfig(client.component, OMX_IndexConfigCommonRotate,
                        &rotation)) != OMX_ErrorNone)
        {

          OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                         "Rotation could not be set: %s\n",
                         OMX_OSAL_TraceErrorStr(omxError));
          return omxError;
        }
      }

      /* set mirroring */
      if(parameters.mirror != OMX_MirrorMax)
      {

        OMX_CONFIG_MIRRORTYPE mirror;

        omxclient_struct_init(&mirror, OMX_CONFIG_MIRRORTYPE);

        mirror.nPortIndex = 1;
        mirror.eMirror = parameters.mirror;

        if((omxError =
          OMX_SetConfig(client.component, OMX_IndexConfigCommonMirror,
                        &mirror)) != OMX_ErrorNone)
        {

          OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                         "Mirroring could not be set: %s\n",
                         OMX_OSAL_TraceErrorStr(omxError));
          return omxError;
        }
      }

      if(omxError == OMX_ErrorNone)
      {
        // /* disable post-processor port */
        // omxError =
        //   OMX_SendCommand(client.component, OMX_CommandPortDisable, 2,
        //           NULL);
        // if(omxError != OMX_ErrorNone)
        // {
        //   return omxError;
        // }

        /* execute conversion */

          omxError =
            omxclient_execute(&client, parameters.infile,
                              variance_array, variance_array_len,
                              parameters.outfile);

        if(omxError != OMX_ErrorNone)
        {
          OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                         "Video processing failed: '%s'\n",
                         OMX_OSAL_TraceErrorStr(omxError));
        }
      }
      else
      {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                       "Component video initialization failed: '%s'\n",
                       OMX_OSAL_TraceErrorStr(omxError));
      }

      /* destroy the component since it was succesfully created */
      omxError = omxclient_component_destroy(&client);
      if(omxError != OMX_ErrorNone)
      {
        OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                       "Component destroy failed: '%s'\n",
                       OMX_OSAL_TraceErrorStr(omxError));
      }
    }

    OMX_Deinit();
  }
  else
  {
    OMX_OSAL_Trace(OMX_OSAL_TRACE_ERROR,
                   "OMX initialization failed: '%s'\n",
                   OMX_OSAL_TraceErrorStr(omxError));

    /* FAIL: OMX initialization failed, reason */
  }

  return omxError;
}