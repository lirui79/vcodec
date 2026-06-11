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

#include "command_line_parser.h"
#include "getopt.h"

#include <stdio.h>
#ifdef _HAVE_PTHREAD_H
#include <stdlib.h>
#endif
#include <string.h>
#include <ctype.h>
#include "dec_log.h"
#include "math.h"
extern int optind, opterr, optopt;
bool use_vcmd = 0;

#define VCDEC_HELP
#define PPDEC_HELP
#define ABS(a) (((a) < 0) ? -(a) : (a))

u32 FP32TOU32(float f)
{
    union
    {
        float f;
        unsigned int u;
    } usf;
    usf.f = f;
    return usf.u;
}

/*------------------------------------------------------------------------------
  _get_log_env
------------------------------------------------------------------------------*/
static void _get_log_env(log_env_setting *env_log) {
  char *env_log_tmp = NULL;
  unsigned int tmp_val_1;

  env_log_tmp = getenv("VCDEC_LOG_OUTPUT");
  if (env_log_tmp) {
    tmp_val_1 = atoi(env_log_tmp);
    if (tmp_val_1 < LOG_COUNT) {
      env_log->out_dir = (vcdec_log_output)tmp_val_1;
    }
  }

  env_log_tmp = getenv("VCDEC_LOG_LEVEL");
  if (env_log_tmp) {
    tmp_val_1 = atoi(env_log_tmp);
    if (tmp_val_1 < VCDEC_LOG_COUNT) {
      env_log->out_level = (vcdec_log_level)tmp_val_1;
    }
  }

  env_log_tmp = getenv("VCDEC_LOG_TRACE");
  if (env_log_tmp) {
    env_log->k_trace_map = atoi(env_log_tmp);
  }

}

int ParseLogTraceMapByName(char str[]) {
  int i;
  int n = 0;
  for (i = 0; str[i] != '\0'; i++) {
    if ((str[i] == 'A' || str[i] == 'a') && (str[i+1] == 'p' || str[i+1] == 'P') && (str[i+2] == 'i' || str[i+2] == 'I')){
      n |= 2;
      i+=2;
    }
    else if ((str[i] == 'S' || str[i] == 's') && (str[i+1] == 'T' || str[i+1] == 't')) {
      n |= 4;
      i+=3;
    }
    else if ((str[i] == 'C' || str[i] == 'c') && (str[i+1] == 'F' || str[i+1] == 'f')) {
      n |= 1;
      i+=2;
    }
    else if ((str[i] == 'D' || str[i] == 'd') && (str[i+1] == 'W' || str[i+1] == 'w')) {
      n |= 8;
      i+=2;
    }
    else if ((str[i] == 'M' || str[i] == 'm') && (str[i+1] == 'E' || str[i+1] == 'e' )) {
      n |= 16;
      i+=2;
    }

    else if ((str[i] == 'D' || str[i] == 'd') && (str[i+1] == 'P' || str[i+1] == 'p')) {
      n |= 32;
      i+=2;
    }

    else if ((str[i] == 'R' || str[i] == 'r') && (str[i+1] == 'E' || str[i+1] == 'e')) {
      n |= 64;
      i+=3;
    }

    else if ((str[i] == 'V' || str[i] == 'v') && (str[i+1] == 'C' || str[i+1] == 'c')) {
      n |= 128;
      i+=3;
    }

    else if ((str[i] == 'I' || str[i] == 'i') && (str[i+1] == 'R' || str[i+1] == 'r')){
      n |= 256;
      i+=2;
    }
    else if ((str[i] == 'P' || str[i] == 'p') && (str[i+1] == 'E' || str[i+1] == 'e'))
    {
      n |= 512;
      i+=3;
    }
    else if ((str[i] == 'A' || str[i] == 'a') && (str[i+1] == 'L' || str[i+1] == 'l'))
    {
      n |= 1023;
      i+=2;
    }
    else if (str[i] == ':')
      continue;
    else{
      APITRACEERR("%s","please enter a binary number or CFG/API/STRM/DWL/MEM/DPB/REGS/VCMD/IRQ/PERF/ALL\n");
      n = 1;
      break;

    }
  }
  return n;
}


int ParseLogTraceMap(char str[]) {
  int i;
  int n = 0;
  for (i = 0; str[i] != '\0'; i++){
    if (str[i] >= 'A'){
      n = ParseLogTraceMapByName(str);
      return n;
    }
    else if (str[i] == '1' || str[i] == '0'){
      n = n << 1;
      n += str[i] - '0';
    }
    else{
      APITRACEERR("%s","please enter a binary number --logtracemap\n");
      return 1;
    }
  }
  return n;
}

void PrintUsage(char* executable, enum TestBenchFormat fmt) {
  printf("Usage: %s [options] <file>\n", executable);

  if (fmt == PPDEC) {
#if defined(PPDEC_HELP) && !defined _WIN32
    char helptext[] = {
      #include "./ppdec_help.dat"
      , '\0' }; //cppcheck-suppress syntaxError
    fprintf(stdout, "%s", helptext);
    (void)helptext;
#endif
  } else {
#if defined(VCDEC_HELP) && !defined _WIN32
    char helptext[] = {
      #include "./vcdec_help.dat"
      , '\0'
    };
    fprintf(stdout, "%s", helptext);
    (void)helptext;
#endif
  }
  printf("\n");
}

void SetupDefaultParams(struct TestParams* params) {
  int i = 0;
  memset(params, 0, sizeof(struct TestParams));
  params->read_mode = STREAMREADMODE_FRAME;
  /* enabled in VCDecxxxInit and obsolete here */
  // params->tile_transpose = TRUE;
  params->oppoints = 1;
  params->auxinfo = 0;
  params->hw_conceal = 1;
  params->disable_slice = 0;
  params->is_ringbuffer = 1;
  params->mc_enable = 0;
  params->video_range = 1;
  params->rgb_stan = BT601;
  memset(params->ppu_cfg, 0, sizeof(params->ppu_cfg));
  memset(params->delogo_params, 0, sizeof(params->delogo_params));
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    params->ppu_cfg[i].shaper_enabled = 1;
    params->ppu_cfg[i].chroma_format = PP_YUV420;
    params->ppu_cfg[i].out_p010 = 1;
    params->ppu_cfg[i].source_range = 1;
    params->ppu_cfg[i].set_source_range_enable = 0;
    params->ppu_cfg[i].target_range = 0;
    params->ppu_cfg[i].set_target_range_enable = 0;
    params->ppu_cfg[i].dither_enable = 1;
    params->ppu_cfg[i].antialias = 1;
    params->ppu_cfg[i].enable_3dlut = 0;
    params->ppu_cfg[i].crop_id = 1;
    params->ppu_cfg[i].align_pixel_w = 1;
  }

  params->table_3dlut_name = NULL;
  params->error_handling = DEC_EC_FRAME_TOLERANT_ERROR;
  params->error_ratio = 10;
  params->align = DEC_ALIGN_16B;
  params->max_temporal_layer = -1;
  /* g1 formats only */
  params->convert_tiled_output = 0;
  params->save_index = 0;
  params->num_buffers = 0;
  params->skip_frame = DEC_SKIP_NON_REF_RECON;
  params->index_file_name = NULL;
  params->dpb_mode = DEC_DPB_FRAME;
  params->convert_to_frame_dpb = 0;
  params->use_extra_buffers = 0;
  params->allocate_extra_buffers_in_output = 0;
  params->stream_trace = NULL;
  params->use_ref_idct = 0;
  /* h264 only */
  params->support_asofmo_stream = 0;
  /* JPEG only */
  params->only_full_resolution = 0;
  params->ri_mc_enable = 0;
  params->instant_buffer = 0;
  /* vp8 only */
  params->snap_shot = 0;
  params->extra_strm_buffer_bits = 0;
  params->user_mem_alloc = 0;
  /* vp6 only */
  params->alpha = 0;
  /* mpeg4 only */
  params->strm_fmt_sorenson = DEC_INPUT_MPEG4;
  params->strm_fmt_custom = DEC_INPUT_MPEG4;
  params->custom.dimensions = 0;
  params->custom.widthxheight = NULL;
  /* vc1 only */
  params->long_stream = 0;
  params->enable_frame_picture = 0;
  /* rv only */
  params->rv_input = 0;
  /*for Multi-stream*/
  params->multimode = 0;
  params->nstream = 0;
  params->nstrmcfg = 0;
  for(i = 0; i < MAX_STREAMS; i++)
    params->streamcfg[i] = NULL;
  /* driver selection default */
  params->dec_dev = "/tmp/dev/hantrodec";
  params->mem_dev = "/tmp/dev/memalloc";
#ifdef SUPPORT_DMA
  params->dma_dev = "/tmp/dev/dma_drv";
#endif
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    params->pp_filter = params->ppu_cfg[i].pp_filter = LANCZOS;
    params->x_filter_param = params->ppu_cfg[i].x_filter_param = 2;
    params->y_filter_param = params->ppu_cfg[i].y_filter_param = 2;
  }
/*  params->pp_filter = params->ppu_cfg[0].pp_filter = LANCZOS;
  params->x_filter_param = params->ppu_cfg[0].x_filter_param = 2;
  params->y_filter_param = params->ppu_cfg[0].y_filter_param = 2;*/
#ifdef SUPPORT_RANDOM_LATENCY
  /* for axi latency test */
  params->axi_lg_r = 0;
  params->axi_lg_w = 0;
#endif

  /* default logmsg set */
  /* logmsg env default settting */
  log_env_setting env_log = {
      LOG_STDOUT, VCDEC_LOG_ERROR, 0x3};  //enable API ERROR
  _get_log_env(&env_log);
  params->logOutDir = env_log.out_dir;
  params->logOutLevel = env_log.out_level;
  params->logTraceMap = env_log.k_trace_map;
}

// Pp option:
//    --pp <a:b:c:d>    enable pp a/b/c/d
int ParsePpParams(char *optarg, u32 *enable) {
  char *p = optarg;
  char *q = p;
  int n;
  while (*q) {
    while (*p && isdigit(*p)) p++;
    if (*p && *p != ':') return 1;
    p++; n = atoi(q); q = p;
    if (n >= DEC_MAX_OUT_COUNT) return 1;
    enable[n]=1;
  }
  return 0;
}

// Parse cropping parameters from argument string: --cropN=wxh@(x,y)
// Return 1 for illegal string, otherwise return 0.
int ParseCropParams(char *optarg, u32 *x, u32 *y, u32 *w, u32 *h) {
  char *p = optarg;
  char *q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != 'x') return 1;
  //*p++ = '\0';
  p++; *w = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != '@') return 1;
  //*p++ = '\0';
  p++; *h = atoi(q); q = p;
  if (!*p || *p != '(') return 1;
  q = ++p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  //*p++ = '\0';
  p++; *x = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ')') return 1;
  //*p++ = '\0';
  p++; *y = atoi(q); q = p;
  if (*p) return 1;
  return 0;
}

// Parse scaling parameters from argument string:
//   --scaleN=-dx[:y]
//   --scaleN=wxh
// Return 1 for illegal string, otherwise return 0.
int ParseScaleParams(char *optarg, u32 *w, u32 *h, u32 *rx, u32 *ry) {
  char *p = optarg;
  char *q = p;
  if (*p && p[0] == '-' && p[1] == 'd') {
    p += 2; q = p;
    while (*p && isdigit(*p)) p++;
    if (!*p) {
      *rx = *ry = atoi(q);
      return 0;
    } else if (*p != ':') return 1;
    *p++ = '\0'; *rx = atoi(q); q = p;
    while (*p && isdigit(*p)) p++;
    if (*p) return 1;
    *ry = atoi(q);
    *w = *h = 0;
  } else {
    while (*p && isdigit(*p)) p++;
    if (!*p || *p != 'x') return 1;
    *p++ = '\0'; *w = atoi(q); q = p;
    while (*p && isdigit(*p)) p++;
    if (*p) return 1;
    *p++ = '\0'; *h = atoi(q); q = p;
    *rx = *ry = 0;
  }
  return 0;
}

static int ParsePosParams(char *optarg, u32 *x, u32 *y, u32 *w, u32 *h) {
  char *p = optarg;
  char *q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != 'x') return 1;
  p++; *w = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != '@') return 1;
  p++; *h = atoi(q); q = p;
  if (!*p || *p != '(') return 1;
  q = ++p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *x= atoi(q); q = p;
  if (*x <= 0) return 1;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ')') return 1;
  p++; *y= atoi(q); q = p;
  if (*y <= 0) return 1;
  if (*p) return 1;
  return 0;
}

static int ParsePixelParams(char *optarg, u32 *Y, u32 *U, u32 *V) {
  char *p = optarg;
  char *q = p;
  if (!*p || *p != '(') return 1;
  q = ++p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *Y = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *U = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ')') return 1;
  p++; *V = atoi(q); q = p;
  return 0;
}
#if 0
static int ParseConstPixelParams(char *optarg, u32 *lu0, u32 *lu1, u32 *ch0, u32 *ch1) {
  char *p = optarg;
  char *q = p;
  if (!*p || *p != '(') return 1;
  q = ++p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *lu0 = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *lu1 = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ',') return 1;
  p++; *ch0 = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ')') return 1;
  p++; *ch1 = atoi(q); q = p;
  return 0;
}
#endif
static int ParseConstPixelParams(char *optarg, u32 *lu0, u32 *lu1, u32 *ch0, u32 *ch1) {
  char *p = optarg;
  char *q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *lu0 = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *lu1 = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *ch0 = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  *ch1 = atoi(q);
  if (*p) return 1;
  return 0;
}

static int ParsePaddingOffset(char *optarg, u32 *l, u32 *t, u32 *r, u32 *b) {
  char *p = optarg;
  char *q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *l = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *t = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *r = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  *b = atoi(q);
  if (*p) return 1;
  return 0;
}

/* --padding-value=r:g:b:a
   --padding-value=y:u:v */
static int ParsePaddingValue(char *optarg, u32 *r_y, u32 *g_u, u32 *b_v, u32 *a) {
  char *p = optarg;
  char *q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *r_y = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (!*p || *p != ':') return 1;
  p++; *g_u = atoi(q); q = p;
  while (*p && isdigit(*p)) p++;
  if (*p && *p != ':') return 1;
  *b_v = atoi(q);
  if (!*p) return 0;
  p++;  q = p;
  while (*p && isdigit(*p)) p++;
  *a = atoi(q);
  if (*p) return 1;
  return 0;
}

int ParseParams(int argc, char* argv[], struct TestParams* params) {
  i32 c = 0;
  i32 option_index = 0;
  u8 flag_b = 0;
  u32 a = 0, pp = 0, a_h = 0, dec400_a;
  u32 delogo = 0;
  char *px = NULL;
#ifdef SUPPORT_RANDOM_LATENCY
  char *pr = NULL;
  char *pw = NULL;
#endif
  int i;
  char s[300];
  char *cml;
  cml = (char *)malloc(8000);
  memset(cml,0,8000);
  sprintf(s,"%s ","command line:");
  strcat(cml,s);
  for (i=0; i < argc; i++){
    sprintf(s,"%s ",argv[i]);
    strcat(cml,s);
  }
  CONFIGTRACE_I("%s\n",cml);
  free(cml);
  cml = NULL;
  static struct option long_options[] = {
    {"cp", required_argument, 0, 'C'},
    {"crop-win", required_argument, 0, 'C'},
    {"no-write", no_argument, 0, 'X'},
    {"low-latency", no_argument, 0, 'L'},
    {"secure", no_argument, 0, 'y'},
    {"partial", no_argument, 0, 'K'},
    {"cr-first", no_argument, 0, 'c'},
    {"pp-ycbcr", required_argument, 0, 'Y'},
    {"pp-packed-mode", required_argument, 0, 'P'},
    {"md5", no_argument, 0, 'M'},
    {"md5-per-pic", no_argument, 0, 'm'},
    {"mvc", no_argument, 0, 'J'},
    {"num-pictures", required_argument, 0, 'N'},
    {"output-file", required_argument, 0, 'O'},
    {"trace-files", no_argument, 0, 't'},
    {"rtl-trace", no_argument, 0, 'r'},
    {"separate-output-thread", no_argument, 0, 'Z'},
    {"single-frames-out", no_argument, 0, 'Q'},
    {"full-stream", no_argument, 0, 'F'},
    {"packet-by-packet", no_argument, 0, 'p'},
    {"nalu", no_argument, 0, 'u'},
    {"enable", required_argument, 0, 'E'},
    {"tiled", no_argument, 0, 'T'},
    {"disable-display-order", no_argument, 0, 'R'},
    {"sdl", no_argument, 0, 'j'},
    {"input-format", required_argument, 0, 'i'},
    {"down-scale", required_argument, 0, 'd'},
    {"flexible-scale", required_argument, 0, 'D'},
    {"stride", required_argument, 0, 'A'},
    {"align-height", required_argument, 0, 'l'},
    {"align-pixel-width", required_argument, 0, 'l'},
    {"dec400-align", required_argument, 0, 'l'},
    {"force-8bits", no_argument, 0, 'f'},
    {"compress-bypass", no_argument, 0, 'b'},
    {"non-ringbuffer", no_argument, 0, 'n'},
    {"prefetch-onepic", no_argument, 0, 'g'},
    {"pp-shaper-no-pad", no_argument, 0, 4},
    {"pp-shaper", no_argument, 0, 2},
    {"pp-shaper-dis", no_argument, 0, 21},
    {"pp",     required_argument, 0, 9},
    {"crop",   required_argument, 0, 9},
    {"scale",  required_argument, 0, 9},
    {"pp-tiled-out", no_argument, 0, 'U'},
    {"pp-planar", no_argument, 0, 'a'},
    {"pp-luma-only", no_argument, 0, 'W'},
    {"pp-rgb", no_argument, 0, 'G'},
    {"pp-rgb-planar", no_argument, 0, 'h'},
    {"pp-src-sel", required_argument, 0, 'B'},
    {"scaling-pad-yuv", required_argument, 0, 'B'},
    {"rgb-fmat", required_argument, 0, 'I'},
    {"rgb-std", required_argument, 0, 'o'},
    {"yuv-source-range", required_argument, 0, 12},
    {"rgb-source-range", required_argument, 0, 12},
    {"source-range", required_argument, 0, 12},
    {"yuv-target-range", required_argument, 0, 'z'},
    {"rgb-target-range", required_argument, 0, 'z'},
    {"target-range", required_argument, 0, 'z'},
    {"pp-dither-disable", no_argument, 0, 13},
    /* padding */
    {"pad-mode", required_argument, 0, 23},
    {"pad-offset", required_argument, 0, 24},
    {"pad-pix", required_argument, 0, 25},
    {"rgb-alpha", required_argument, 0, 3},
    {"out_stride", required_argument, 0, 's'},
    {"delogo", required_argument, 0, 0},
    {"pos", required_argument, 0, 0},
    {"show", required_argument, 0, 0},
    {"mode", required_argument, 0, 0},
    {"YUV", required_argument, 0, 0},
    {"second-crop", no_argument, 0, 1},
    {"intra-only", no_argument, 0, 6},
    {"pp-filter", required_argument, 0, 7},
    {"antialias", required_argument, 0, 7},
    {"filter-param", required_argument, 0, 8},
    {"pp-comp", no_argument, 0, 'v'},
    {"pp-pvfbc", no_argument, 0, 'v'},
    {"tiled-mode", required_argument, 0, 'q'},
    {"mc", no_argument, 0, 'e'},
    {"skip-frame", required_argument, 0, 0},
    {"index-file", required_argument, 0, 0},
    {"lc-int-enable", no_argument, 0},
    {"lc-stripe", required_argument, 0},
    {"input-buffer-size", required_argument, 0, 0},
    {"ec", required_argument, 0, 0},
    {"error-ratio", required_argument, 0, 0},
    {"tlayer", required_argument, 0, 0},
    {"3dlut", no_argument, 0, 20},
    {"table-3dlut", required_argument, 0, 15},
    {"blend", no_argument, 0, 15},
    {"blend-alpha", required_argument, 0, 15},
    {"blendx", required_argument, 0, 15},
    {"blendy", required_argument, 0, 15},
    {"libfile", required_argument, 0, 27},
    {"const-pix", required_argument, 0, 27},
    /* g1 formats only */
    {"tiled-out", no_argument, 0, 0},
    {"convert-tiled-out", no_argument, 0, 0},
    {"save-index", no_argument, 0, 0},
    {"num-buffers", required_argument, 0, 0},
    {"dpb-mode", no_argument, 0, 0},
    {"convert-frame-out", no_argument, 0, 0},
    {"use-extra-buffers", no_argument, 0, 0},
    {"alloc-extra-buffers", no_argument, 0, 0},
    {"stream-trace", required_argument, 0, 0},
    {"use-ref-idct", no_argument, 0, 0},
    /* h264 only */
    {"is-asofmo", no_argument, 0, 0},
    /* jpeg only */
    {"full-only", no_argument, 0, 0},
    {"ri-mc", no_argument, 0, 0},
    {"instant-buffer", no_argument, 0, 0},
    /* vp8 only */
    {"snap-shot", no_argument, 0, 0},
    {"extra-bits", required_argument, 0, 0},
    {"user-mem-alloc", no_argument, 0, 0},
    /* vp6 only */
    {"alpha", no_argument, 0, 0},
    /* mpeg4 only */
    {"strm-sorenson", no_argument, 0, 0},
    {"strm-custom", no_argument, 0, 0},
    {"custom", required_argument, 0, 0},
    /* vc1 only */
    {"long-stream", no_argument, 0, 0},
    {"frame-picture", no_argument, 0, 0},
    /* rv only */
    {"rv-input", no_argument, 0, 0},
    /* multi stream configs */
    {"multimode", required_argument, 0, 0}, // Multi-stream mode, 0--disable, 1--mult-thread, 2--multi-process(next to do)
    {"streamcfg", required_argument, 0, 0}, // extra stream config.
    // {"notranspose", no_argument, 0, 9}, // notranspose is obsolete.
    {"dis-ec", no_argument, 0, 9},
    {"auxinfo", required_argument, 0, 9},
    {"dis-slice", no_argument, 0, 9},
    {"oppoint", no_argument, 0, 'k'},
    {"tb-cfg", required_argument, 0, 10},
    {"help", no_argument, 0, 11},
   /* hantrodec memalloc select */
    {"mem-dev",required_argument, 0, 0},
    {"dec-dev",required_argument, 0, 0},
#ifdef SUPPORT_DMA
    /* dma select */
    {"dma-dev",required_argument, 0, 0},
#endif
    {"dis-interpred",no_argument, 0, 0},
    {"opl", no_argument, 0, 9},
#ifdef SUPPORT_RANDOM_LATENCY
    /* axi latency set */
    {"axi-lg",required_argument, 0, 0},
#endif
    /* for logmsg set */
    {"logoutdir", required_argument, 0, 0},
    {"logoutlevel", required_argument, 0, 0},
    {"logtracemap", required_argument, 0, 0},
    {"crc", no_argument, 0, 0},
    {"use-vcmd", no_argument, 0, 0},
    {0, 0, 0, 0}
  };

  /* read command line arguments */
  optind = 1; // to prevent segmentation fault happers after the second and later call getopt_long.
  while ((c = getopt_long(argc, argv,
                          "C:E:eFi:MmN:A:O:s:P:pY:TtrXlLB:cKZQq:Rjd:D:fJbngWwVvaYUGhuI:o:k:0:123:4567:8:9",
                          long_options,
                          &option_index)) != -1) {
    switch (c) {
    case 'C':
      if (optarg[0] == '1') {
        params->ppu_cfg[pp].crop_id = 1;
        break;
      } else if (optarg[0] == '2') {
        params->ppu_cfg[pp].crop_id = 2;
        break;
      } else if (optarg[0] == '3') {
        params->ppu_cfg[pp].crop_id = 3;
        break;
      } else if (optarg[0] == '4') {
        params->ppu_cfg[pp].crop_id = 4;
        break;
      } else {
        switch (optarg[0]) {
        case 'x':
          if (params->ppu_cfg[pp].crop_id == 1) {
            if (params->ppu_cfg[pp].crop2.enabled)
              params->ppu_cfg[pp].crop2.x = atoi(optarg + 1);
            else
              params->ppu_cfg[pp].crop.x = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 2){
            params->ppu_cfg[pp].crop11.x = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 3){
            params->ppu_cfg[pp].crop12.x = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 4){
            params->ppu_cfg[pp].crop13.x = atoi(optarg + 1);
          }
          break;
        case 'y':
          if (params->ppu_cfg[pp].crop_id == 1) {
            if (params->ppu_cfg[pp].crop2.enabled)
              params->ppu_cfg[pp].crop2.y = atoi(optarg + 1);
            else
              params->ppu_cfg[pp].crop.y = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 2){
            params->ppu_cfg[pp].crop11.y = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 3){
            params->ppu_cfg[pp].crop12.y = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 4){
            params->ppu_cfg[pp].crop13.y = atoi(optarg + 1);
          }
          break;
        case 'w':
          if (params->ppu_cfg[pp].crop_id == 1) {
            if (params->ppu_cfg[pp].crop2.enabled)
              params->ppu_cfg[pp].crop2.width = atoi(optarg + 1);
            else
              params->ppu_cfg[pp].crop.width = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 2){
            params->ppu_cfg[pp].crop11.width = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 3){
            params->ppu_cfg[pp].crop12.width = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 4){
            params->ppu_cfg[pp].crop13.width = atoi(optarg + 1);
          }
          break;
        case 'h':
          if (params->ppu_cfg[pp].crop_id == 1) {
            if (params->ppu_cfg[pp].crop2.enabled)
              params->ppu_cfg[pp].crop2.height = atoi(optarg + 1);
            else
              params->ppu_cfg[pp].crop.height = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 2){
            params->ppu_cfg[pp].crop11.height = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 3){
            params->ppu_cfg[pp].crop12.height = atoi(optarg + 1);
          } else if (params->ppu_cfg[pp].crop_id == 4){
            params->ppu_cfg[pp].crop13.height = atoi(optarg + 1);
          }
          break;
        default:
          CONFIGTRACE_E("%s\n","ERROR: Enable cropping parameter by using: -C[xywh]NNN. E.g.,");
          CONFIGTRACE_E("%s\n","\t -CxXXX -CyYYY     Crop from (XXX, YYY)");
          CONFIGTRACE_E("%s\n","\t -CwWWW -ChHHH     Crop size  WWWxHHH");
          return 1;
          }
          if (optarg[0]!='d' && !params->ppu_cfg[pp].crop2.enabled) {
            params->ppu_cfg[pp].enabled = 1;
            params->ppu_cfg[pp].crop.enabled = 1;
            params->ppu_cfg[pp].crop.set_by_user = 1;
            params->pp_enabled = 1;
          }
          break;
        }
    case 'X':
      params->sink_type = SINK_NULL;
      break;
    case 2:
      params->ppu_cfg[pp].shaper_enabled = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 21:
      params->ppu_cfg[pp].shaper_enabled = 0;
      break;
    case 4:
      params->ppu_cfg[pp].shaper_no_pad = 1;
      break;
    case 'L':
      params->decoder_mode |= DEC_LOW_LATENCY;
      break;
    case 'y':
      params->decoder_mode |= DEC_SECURITY;
      break;
    case 'K':
      params->decoder_mode |= DEC_PARTIAL_DECODING;
      break;
    case 'c':
      //params->cr_first = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->ppu_cfg[pp].cr_first = 1;
      params->pp_enabled = 1;
      break;
    case 't':
      params->hw_traces = 1;
      /* TODO(vmr): Check SW support for traces. */
      break;
    case 'r':
      params->trace_target = 1;
      break;
    case 'M':
      params->sink_type = SINK_MD5_SEQUENCE;
      break;
    case 'm':
      params->sink_type = SINK_MD5_PICTURE;
      break;
    case 'N':
      params->num_of_decoded_pics = atoi(optarg);
      break;
    case 'O':
      for (u32 i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        params->out_file_name[i] = (char *)malloc(NAME_MAX);
        if (!params->out_file_name[i]) {
          CONFIGTRACE_E("%s\n","ERROR: failed to create output file.");
          return 1;
        }
        strncpy(params->out_file_name[i], optarg, NAME_MAX);
      }
      break;
    case 'p':
      params->read_mode = STREAMREADMODE_PACKETIZE;
      break;
    case 'u':
      CONFIGTRACE_E("%s\n","Option -u is obsoleted and ignored. Please DON'T use it ANYMORE!");
      //params->read_mode = STREAMREADMODE_NALUNIT;
      break;
    case 'Z':
      params->extra_output_thread = 1;
      break;
    case 'Q':
      params->sink_type = SINK_FILE_PICTURE;
      break;
    case 'R':
      params->disable_display_order = 1;
      break;
    case 'F':
      /*CONFIGTRACE_E("%s\n",
        "ERROR: The FULLSTREAM feature will be removed, please don't use option \"-F\"!");
      return 1;*/
      params->read_mode = STREAMREADMODE_FULLSTREAM;
      break;
    case 'E':
      params->ppu_cfg[pp].out_p010 = 0;
      if (strcmp(optarg, "rs") == 0) {
        params->hw_format = DEC_OUT_FRM_RASTER_SCAN;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "pack10") == 0) {
        params->ppu_cfg[pp].out_p010 = 0;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "p010") == 0) {
        params->ppu_cfg[pp].out_p010 = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "I010") == 0) {
        params->ppu_cfg[pp].out_I010 = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "L010") == 0) {
        params->ppu_cfg[pp].out_L010 = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "1010") == 0) {
        params->ppu_cfg[pp].enabled = 1;
        params->ppu_cfg[pp].out_1010 = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "pbe") == 0) {
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "p012") == 0) {
        params->ppu_cfg[pp].out_p012 = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      if (strcmp(optarg, "I012") == 0) {
        params->ppu_cfg[pp].out_I012 = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      break;
#ifdef SDL_ENABLED
    case 'j':
      /* Make sure SDL sink supports only planar. */
      params->sink_type = SINK_SDL;
      break;
#endif /* SDL_ENABLED */
    case 'i':
      if (strcmp(optarg, "bs") == 0 || strcmp(optarg, "h265") == 0) {
        params->file_format = FILEFORMAT_BYTESTREAM_HEVC;
      } else if (strcmp(optarg, "ivf") == 0) {
        params->file_format = FILEFORMAT_IVF;
      } else if (strcmp(optarg, "webm") == 0) {
        params->file_format = FILEFORMAT_WEBM;
      } else if (strcmp(optarg, "h264") == 0) {
        params->file_format = FILEFORMAT_BYTESTREAM_H264;
      } else if (strcmp(optarg, "h266") == 0) {
        params->file_format = FILEFORMAT_BYTESTREAM_VVC;
      } else {
        CONFIGTRACE_E("%s\n","Unsupported file format");
        return 1;
      }
      break;
    case ':':
      CONFIGTRACE_E("Option -%c requires an argument.\n", optopt);
      return 1;
    case 'd':
      if (strlen(optarg) == 1 && (optarg[0] == '1' ||
                                  optarg[0] == '2' ||
                                  optarg[0] == '4' ||
                                  optarg[0] == '8'))
        params->ppu_cfg[pp].scale.ratio_x = params->ppu_cfg[pp].scale.ratio_y = optarg[0] - '0';
      else if (strlen(optarg) == 3 &&
               (optarg[0] == '1' || optarg[0] == '2' || optarg[0] == '4' || optarg[0] == '8') &&
               (optarg[2] == '1' || optarg[2] == '2' || optarg[2] == '4' || optarg[2] == '8') &&
               optarg[1] == ':') {
        params->ppu_cfg[pp].scale.ratio_x = optarg[0] - '0';
        params->ppu_cfg[pp].scale.ratio_y = optarg[2] - '0';

      } else {
        CONFIGTRACE_E("%s\n","ERROR: Enable down scaler parameter by using: -d[1248][:[1248]]");
        return 1;
      }
      CONFIGTRACE_E("Down scaler enabled: 1/%d, 1/%d\n",
              params->ppu_cfg[pp].scale.ratio_x, params->ppu_cfg[pp].scale.ratio_y);
      params->ppu_cfg[pp].scale.scale_by_ratio = 1;
      params->ppu_cfg[pp].scale.enabled = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'D':
      px = strchr(optarg, 'x');
      if (!px) {
        CONFIGTRACE_E("%s\n","Illegal parameter");
        CONFIGTRACE_E("%s\n","ERROR: Enable scaler parameter by using: -D[w]x[h]");
        return 1;
      }
      *px = '\0';
      params->ppu_cfg[pp].scale.width = atoi(optarg);
      params->ppu_cfg[pp].scale.height = atoi(px+1);
      if (params->ppu_cfg[pp].scale.width == 0 || params->ppu_cfg[pp].scale.height == 0) {
        CONFIGTRACE_E("Illegal scaled width/height: %d,%d\n",
                params->ppu_cfg[pp].scale.width,
                params->ppu_cfg[pp].scale.height);
        return 1;
      }
      params->ppu_cfg[pp].scale.scale_by_ratio = 0;
      params->ppu_cfg[pp].scale.enabled = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'A':
      a = atoi(optarg);
#define CASE_ALIGNMENT(NBYTE) case NBYTE: params->align = DEC_ALIGN_##NBYTE##B; break;
      switch (a) {
      CASE_ALIGNMENT(1);
      CASE_ALIGNMENT(8);
      CASE_ALIGNMENT(16);
      CASE_ALIGNMENT(32);
      CASE_ALIGNMENT(64);
      CASE_ALIGNMENT(128);
      CASE_ALIGNMENT(256);
      CASE_ALIGNMENT(512);
      CASE_ALIGNMENT(1024);
      CASE_ALIGNMENT(2048);
      default:
        CONFIGTRACE_E("Illegal parameter: A%s\n", optarg);
        CONFIGTRACE_E("%s\n","ERROR: valid alignment value: 1/8/16/32/64/128/256/512/1024/2048 bytes");
        return 1;
      }
      break;
    case 'l':
      if (strcmp(long_options[option_index].name,
             "align-height") == 0) {
        a_h = atoi(optarg);
        #define CASE_ALIGNMENT_h(NBYTE) case NBYTE: params->align_h = DEC_ALIGN_##NBYTE##B; break;
        switch (a_h) {
        CASE_ALIGNMENT_h(1);
        CASE_ALIGNMENT_h(8);
        CASE_ALIGNMENT_h(16);
        CASE_ALIGNMENT_h(32);
        CASE_ALIGNMENT_h(64);
        CASE_ALIGNMENT_h(128);
        default:
          CONFIGTRACE_E("Illegal parameter: A%s\n", optarg);
          CONFIGTRACE_E("%s\n","ERROR: valid alignment value: 1/8/16/32/64/128 bytes");
          return 1;
        }
      } else if (strcmp(long_options[option_index].name,
             "align-pixel-width") == 0) {
        params->ppu_cfg[pp].align_pixel_w = atoi(optarg);
      } else if (strcmp(long_options[option_index].name,
             "dec400-align") == 0) {
        dec400_a = atoi(optarg);
        #define CASE_ALIGNMENT_DEC400(NBYTE) case NBYTE: params->ppu_cfg[pp].dec400_align = DEC_ALIGN_##NBYTE##B; break;
        switch (dec400_a) {
        CASE_ALIGNMENT_DEC400(32);
        CASE_ALIGNMENT_DEC400(64);
        default:
          CONFIGTRACE_E("Illegal parameter: A%s\n", optarg);
          CONFIGTRACE_E("%s\n","ERROR: valid alignment value: 32/64 bytes");
          return 1;
        }
      }
      break;
    case 's':
      switch (optarg[0]) {
      case 'y':
        params->ppu_cfg[pp].ystride = atoi(optarg + 1);
        break;
      case 'c':
        params->ppu_cfg[pp].cstride = atoi(optarg + 1);
        break;
      default:
        CONFIGTRACE_E("%s\n","ERROR: Enable out stride parameter by using: -s[yc]NNN. E.g.");
        return 1;
      }
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'b':
      flag_b = 1;
      params->compress_bypass = 1;
      break;
    case 'n':
      params->is_ringbuffer = 0;
      break;
    case 'g':
      CONFIGTRACE_E("%s\n","Option -g is obsoleted and ignored. Please DON'T use it ANYMORE!");
      break;
#ifndef SDL_ENABLED
    case 'j':
#endif /* SDL_ENABLED */
    case 'f':
      params->ppu_cfg[pp].out_cut_8bits = 1;
      params->ppu_cfg[pp].out_p010 = 0;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'U':
    case 'T':
      params->ppu_cfg[pp].tiled_e = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'W':
      params->ppu_cfg[pp].monochrome = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'G':
      if(!params->ppu_cfg[pp].rgb) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGB888;  /* RGB888 as default */
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      break;
    case 'h':
      if(!params->ppu_cfg[pp].rgb_planar) {
        params->ppu_cfg[pp].rgb_planar = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGB888;  /* RGB888 as default */
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      }
      break;
    case 'P':
      if (strcmp(optarg, "YUYV") == 0) {
        params->ppu_cfg[pp].out_yuyv = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->ppu_cfg[pp].chroma_format = PP_YUV422;
        params->pp_enabled = 1;
      } else if (strcmp(optarg, "UYVY") == 0) {
        params->ppu_cfg[pp].out_uyvy = 1;
        params->ppu_cfg[pp].enabled = 1;
        params->ppu_cfg[pp].chroma_format = PP_YUV422;
        params->pp_enabled = 1;
      } else {
        CONFIGTRACE_E("Illegal format: %s\n", optarg);
      }
      break;
    case 'a':
      params->ppu_cfg[pp].planar = 1;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'Y':
      if (strcmp(optarg, "YUV420") == 0) {
        params->ppu_cfg[pp].chroma_format = PP_YUV420;
      } else if (strcmp(optarg, "YUV422") == 0) {
        params->ppu_cfg[pp].chroma_format = PP_YUV422;
      } else if (strcmp(optarg, "YUV444") == 0) {
        params->ppu_cfg[pp].chroma_format = PP_YUV444;
      } else {
        CONFIGTRACE_E("Illegal chroma format: %s\n", optarg);
        return 1;
      }
      params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGB888_P;
      params->ppu_cfg[pp].enabled = 1;
      params->pp_enabled = 1;
      break;
    case 'e':
      params->mc_enable = 1;
      break;
    case 'k':
      params->oppoints = 0;
      break;
    case 'q':
      if (strcmp(optarg, "TILED8x8") == 0 || strcmp(optarg, "TILED8X8") == 0) {
        params->ppu_cfg[pp].tiled_e = 1;
        params->ppu_cfg[pp].tile_mode = TILED8x8;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else if (strcmp(optarg, "TILED16x16") == 0 || strcmp(optarg, "TILED16X16") == 0) {
        params->ppu_cfg[pp].tiled_e = 1;
        params->ppu_cfg[pp].tile_mode = TILED16x16;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else if (strcmp(optarg, "TILED128x2") == 0 || strcmp(optarg, "TILED128X2") == 0) {
        params->ppu_cfg[pp].tiled_e = 1;
        params->ppu_cfg[pp].tile_mode = TILED128x2;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else if (strcmp(optarg, "TILED64x64") == 0 || strcmp(optarg, "TILED64X64") == 0) {
        params->ppu_cfg[pp].tiled_e = 1;
        params->ppu_cfg[pp].tile_mode = TILED64x64;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else if (strcmp(optarg, "TILED32x8") == 0 || strcmp(optarg, "TILED32X8") == 0) {
        params->ppu_cfg[pp].tiled_e = 1;
        params->ppu_cfg[pp].tile_mode = TILED32x8;
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else {
        CONFIGTRACE_E("Illegal tiled format: %s\n", optarg);
      }
      break;
    case 'v':
      params->ppu_cfg[pp].comp_enabled = 1;
      params->pp_enabled = 1;
      params->ppu_cfg[pp].enabled = 1;
      break;
    case 27:
      if (strcmp(long_options[option_index].name, "libfile") == 0) {
        params->in_lib_name = optarg;
      } else if (strcmp(long_options[option_index].name, "const-pix") == 0) {
        if (ParseConstPixelParams(optarg, &(params->ppu_cfg[pp].pp_pvfbc_lu_const0),
                                     &(params->ppu_cfg[pp].pp_pvfbc_lu_const1),
                                     &(params->ppu_cfg[pp].pp_pvfbc_ch_const0),
                                     &(params->ppu_cfg[pp].pp_pvfbc_ch_const1))) {
          CONFIGTRACE_E("ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        }
      }
      break;
    case 'I':
      params->ppu_cfg[pp].out_p010 = 0;
      if (strcmp(optarg, "RGB888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGB888;
      } else if (strcmp(optarg, "BGR888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_BGR888;
      } else if (strcmp(optarg, "R16G16B16") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_R16G16B16;
      } else if (strcmp(optarg, "B16G16R16") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_B16G16R16;
      } else if (strcmp(optarg, "ARGB888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_ARGB888;
      } else if (strcmp(optarg, "ABGR888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_ABGR888;
      } else if (strcmp(optarg, "RGBA888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGBA888;
      } else if (strcmp(optarg, "BGRA888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_BGRA888;
      } else if (strcmp(optarg, "A2R10G10B10") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_A2R10G10B10;
      } else if (strcmp(optarg, "A2B10G10R10") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_A2B10G10R10;
      } else if (strcmp(optarg, "X2R10G10B10") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_X2R10G10B10;
      } else if (strcmp(optarg, "X2B10G10R10") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_X2B10G10R10;
      } else if (strcmp(optarg, "R10G10B10A2") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_R10G10B10A2;
      } else if (strcmp(optarg, "B10G10R10A2") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_B10G10R10A2;
      } else if (strcmp(optarg, "XRGB888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_XRGB888;
      } else if (strcmp(optarg, "XBGR888") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_XBGR888;
      } else if (strcmp(optarg, "RGB888_P") == 0) {
        params->ppu_cfg[pp].rgb = 0;
        params->ppu_cfg[pp].rgb_planar = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGB888;
      } else if (strcmp(optarg, "BGR888_P") == 0) {
        params->ppu_cfg[pp].rgb = 0;
        params->ppu_cfg[pp].rgb_planar = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_BGR888;
      } else if (strcmp(optarg, "R16G16B16_P") == 0) {
        params->ppu_cfg[pp].rgb = 0;
        params->ppu_cfg[pp].rgb_planar = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_R16G16B16;
      } else if (strcmp(optarg, "B16G16R16_P") == 0) {
        params->ppu_cfg[pp].rgb = 0;
        params->ppu_cfg[pp].rgb_planar = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_B16G16R16;
      } else if (strcmp(optarg, "RGB565") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_RGB565;
      } else if (strcmp(optarg, "BGR565") == 0) {
        params->ppu_cfg[pp].rgb = 1;
        params->ppu_cfg[pp].rgb_format = DEC_OUT_FRM_BGR565;
      } else {
        CONFIGTRACE_E("Illegal RGB format: %s\n", optarg);
        return 1;
      }

      break;
    case 'o':
      if (strcmp(optarg, "BT601") == 0)
        params->rgb_stan = BT601;
      else if (strcmp(optarg, "BT601_L") == 0)
        params->rgb_stan = BT601_L;
      else if (strcmp(optarg, "BT709") == 0)
        params->rgb_stan = BT709;
      else if (strcmp(optarg, "BT709_L") == 0)
        params->rgb_stan = BT709_L;
      else if (strcmp(optarg, "BT2020") == 0)
        params->rgb_stan = BT2020;
      else if (strcmp(optarg, "BT2020_L") == 0)
        params->rgb_stan = BT2020_L;
      else
        CONFIGTRACE_E("%s\n","Illegal parameter");
      break;
    case 'z':
      if (strcmp(long_options[option_index].name, "target-range") == 0) {
        if (strcmp(optarg, "FULL") == 0) {
          params->ppu_cfg[pp].target_range = 1;
          params->ppu_cfg[pp].set_target_range_enable = 1;
        } else if (strcmp(optarg, "LIMITED") == 0) {
          params->ppu_cfg[pp].target_range = 0;
          params->ppu_cfg[pp].set_target_range_enable = 1;
        }
      } else
        CONFIGTRACE_E("%s\n","Illegal parameter");
      break;
    case 12:
      if (strcmp(long_options[option_index].name, "source-range") == 0) {
        if (strcmp(optarg, "FULL") == 0) {
          params->ppu_cfg[pp].source_range = 1;
          params->ppu_cfg[pp].set_source_range_enable = 1;
        } else if (strcmp(optarg, "LIMITED") == 0) {
          params->ppu_cfg[pp].source_range = 0;
          params->ppu_cfg[pp].set_source_range_enable = 1;
        }
      } else
        CONFIGTRACE_E("%s\n","Illegal parameter");
      break;
    case 13:
      params->ppu_cfg[pp].dither_enable = 0;
      break;
    case 23:
      params->ppu_cfg[pp].pad.mode = atoi(optarg);
      if (params->ppu_cfg[pp].pad.mode > 2) {
        CONFIGTRACE_E("ERROR: Illegal padding mode: %s\n",optarg);
        return 1;
      }
      break;
    case 24:
      if (ParsePaddingOffset(optarg, &params->ppu_cfg[pp].pad.l_off,
                                     &params->ppu_cfg[pp].pad.t_off,
                                     &params->ppu_cfg[pp].pad.r_off,
                                     &params->ppu_cfg[pp].pad.b_off)) {
          CONFIGTRACE_E("ERROR: Illegal padding offset argument: %s\n", optarg);
          return 1;
        }
       params->ppu_cfg[pp].enabled = 1;
      break;
    case 25:
      if (ParsePaddingValue(optarg, &params->ppu_cfg[pp].pad.r_y,
                                    &params->ppu_cfg[pp].pad.g_u,
                                    &params->ppu_cfg[pp].pad.b_v,
                                    &params->ppu_cfg[pp].pad.a)) {
          CONFIGTRACE_E("ERROR: Illegal padding value argument: %s\n", optarg);
          return 1;
        }
       params->ppu_cfg[pp].enabled = 1;
      break;
    case 3:
      params->ppu_cfg[pp].rgb_alpha = atoi(optarg);
      break;
    case 7:
      if (strcmp(long_options[option_index].name, "pp-filter") == 0) {
        if (strcmp(optarg, "VSI_LINEAR") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = VSI_LINEAR;
        else if (strcmp(optarg, "LANCZOS") == 0) {
          params->pp_filter = params->ppu_cfg[pp].pp_filter = LANCZOS;
          params->x_filter_param = params->ppu_cfg[pp].x_filter_param = 2;
          params->y_filter_param = params->ppu_cfg[pp].y_filter_param = 2;
        } else if (strcmp(optarg, "NEAREST") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = NEAREST;
        else if (strcmp(optarg, "BILINEAR") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = BI_LINEAR;
        else if (strcmp(optarg, "BICUBIC") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = BICUBIC;
        else if (strcmp(optarg, "SPLINE") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = SPLINE;
        else if (strcmp(optarg, "BOX") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = BOX;
        else if (strcmp(optarg, "FAST_LINEAR") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = FAST_LINEAR;
        else if (strcmp(optarg, "FAST_BICUBIC") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = FAST_BICUBIC;
        else if (strcmp(optarg, "AREA") == 0)
          params->pp_filter = params->ppu_cfg[pp].pp_filter = AREA;
        else {
          CONFIGTRACE_E("%s\n","Illegal parameter");
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "antialias") == 0) {
        if (atoi(optarg) == 0) {
          params->ppu_cfg[pp].antialias = 0;
        }
        else {
          params->ppu_cfg[pp].antialias = 1;
        }
      }
      break;
    case 8:
      px = strchr(optarg, 'x');
      if (!px) {
        CONFIGTRACE_E("%s\n","Illegal parameter");
        return 1;
      }
      *px = '\0';
      params->x_filter_param = params->ppu_cfg[pp].x_filter_param = atoi(optarg);
      params->y_filter_param = params->ppu_cfg[pp].y_filter_param = atoi(px+1);
      if (params->x_filter_param == 0 || params->y_filter_param == 0) {
        CONFIGTRACE_E("Illegal filter_param: %d,%d\n",
                params->x_filter_param,
                params->y_filter_param);
        return 1;
      }
      break;
    case 9:
      if (strcmp(long_options[option_index].name,
             "noperf") == 0) {
        //noperf = TRUE;
      }
      /* else if (strcmp(long_options[option_index].name,
                        "notranspose") == 0) {
        params->tile_transpose = FALSE;
      } */
      else if (strcmp(long_options[option_index].name,
                        "dis-ec") == 0) {
        params->hw_conceal = 0;
      } else if (strcmp(long_options[option_index].name,
                        "auxinfo") == 0) {
        params->auxinfo = atoi(optarg);
      } else if (strcmp(long_options[option_index].name,
                        "dis-slice") == 0) {
        params->disable_slice = 1;
      } else if (strcmp(long_options[option_index].name,
                        "pp") == 0) {
        pp = atoi(optarg);
        if (pp >= DEC_MAX_OUT_COUNT || (i32)pp < 0) {
          CONFIGTRACE_E("ERROR: Invalid index for option \"--pp\": %s\n", optarg);
          return 1;
        }
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else if (strcmp(long_options[option_index].name,
                        "crop") == 0) {
        if (params->ppu_cfg[pp].crop_id == 1) {
          if (ParseCropParams(optarg, &params->ppu_cfg[pp].crop.x,
                                    &params->ppu_cfg[pp].crop.y,
                                    &params->ppu_cfg[pp].crop.width,
                                    &params->ppu_cfg[pp].crop.height)) {
          CONFIGTRACE_E("ERROR: Illegal cropping argument: %s\n", optarg);
          return 1;
          }
        }
        params->ppu_cfg[pp].crop.enabled = 1;
        params->ppu_cfg[pp].crop.set_by_user = 1;
      } else if (strcmp(long_options[option_index].name,
                        "scale") == 0) {
        if (ParseScaleParams(optarg, &params->ppu_cfg[pp].scale.width,
                                     &params->ppu_cfg[pp].scale.height,
                                     &params->ppu_cfg[pp].scale.ratio_x,
                                     &params->ppu_cfg[pp].scale.ratio_y)) {
          CONFIGTRACE_E("ERROR: Illegal scaling argument: %s\n", optarg);
          return 1;
        }
        params->ppu_cfg[pp].scale.enabled = 1;
        if (params->ppu_cfg[pp].scale.ratio_x && params->ppu_cfg[pp].scale.ratio_y)
          params->ppu_cfg[pp].scale.scale_by_ratio = 1;
        else
          params->ppu_cfg[pp].scale.scale_by_ratio = 0;
      } else if (strcmp(long_options[option_index].name,
                        "cr-first") == 0) {
        params->ppu_cfg[pp].cr_first = 1;
      } else if (strcmp(long_options[option_index].name,
                        "pp-planar") == 0) {
        params->ppu_cfg[pp].planar = 1;
      } else if (strcmp(long_options[option_index].name,
                        "pp-tiled-out") == 0) {
        params->ppu_cfg[pp].tiled_e = 1;
        if (params->ppu_cfg[pp].tile_mode == TILED4x4)
          params->ppu_cfg[pp].tile_mode = TILED8x8;
      } else if (strcmp(long_options[option_index].name,
                        "p010") == 0) {
        params->ppu_cfg[pp].out_p010 = 1;
        params->ppu_cfg[pp].out_cut_8bits = 0;
      } else if (strcmp(long_options[option_index].name,
                        "I010") == 0) {
        params->ppu_cfg[pp].out_I010 = 1;
        params->ppu_cfg[pp].out_cut_8bits = 0;
      } else if (strcmp(long_options[option_index].name,
                        "p010lsb") == 0) {
        params->ppu_cfg[pp].out_L010 = 1;
        params->ppu_cfg[pp].out_cut_8bits = 0;
      } else if (strcmp(long_options[option_index].name,
                        "luma-only") == 0) {
        params->ppu_cfg[pp].monochrome = 1;
      } else if (strcmp(long_options[option_index].name,
                        "f8") == 0) {
        params->ppu_cfg[pp].out_cut_8bits = 1;
        params->ppu_cfg[pp].out_p010 = 0;
        params->ppu_cfg[pp].out_I010 = 0;
        params->ppu_cfg[pp].out_L010 = 0;
      }  else if (strcmp(long_options[option_index].name,
                        "opl") == 0) {
        params->sink_type = SINK_MD5_VTM;
      }
      break;
    case 10:
      params->in_tb_cfg_file_name = optarg;
      break;
    case 11:
      PrintUsage(argv[0], ALLFORMATS);
      /* if there is only printing HELP info argument, Yes, return special value */
      if (argc == 2)
        return 2;
      break;
    case 'J':
      params->mvc = 1;
      break;
    case 'B':
      if (strcmp(long_options[option_index].name, "pp-src-sel") == 0) {
        if (strcmp(optarg, "DOWN_ROUND") == 0)
          params->ppu_cfg[pp].src_sel_mode = DOWN_ROUND;
        else if (strcmp(optarg, "NO_ROUND") == 0)
          params->ppu_cfg[pp].src_sel_mode = NO_ROUND;
        else if (strcmp(optarg, "UP_ROUND") == 0)
          params->ppu_cfg[pp].src_sel_mode = UP_ROUND;
        else {
          CONFIGTRACE_E("ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "scaling-pad-yuv") == 0) {
        if (ParsePixelParams(optarg, &(params->ppu_cfg[pp].pad_Y),
                                     &(params->ppu_cfg[pp].pad_U),
                                     &(params->ppu_cfg[pp].pad_V))) {
          CONFIGTRACE_E("ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        } else {
          params->ppu_cfg[pp].pad_sel = 1;
        }
      } else {
        CONFIGTRACE_E("ERROR: Illegal parameters: %s\n",optarg);
        return 1;
      }
      break;
    case 0:
      if (strcmp(long_options[option_index].name, "delogo") == 0) {
        delogo = atoi(optarg);
        if (delogo >= 2/*|| delogo < 0 */) {
          CONFIGTRACE_E("ERROR, Invalid index for option \"--delogo\":%s\n", optarg);
          return 1;
        }
        params->delogo_params[delogo].enabled = 1;
      } else if (strcmp(long_options[option_index].name, "pos") == 0) {
        if (ParsePosParams(optarg, &(params->delogo_params[delogo].x),
                                   &(params->delogo_params[delogo].y),
                                   &(params->delogo_params[delogo].w),
                                   &(params->delogo_params[delogo].h))) {
          CONFIGTRACE_E("ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "show") == 0) {
        params->delogo_params[delogo].show = atoi(optarg);
      } else if (strcmp(long_options[option_index].name, "mode") == 0) {
        if (strcmp(optarg, "PIXEL_REPLACE") == 0) {
          params->delogo_params[delogo].mode = PIXEL_REPLACE;
        } else if (strcmp(optarg, "PIXEL_INTERPOLATION") == 0) {
          params->delogo_params[delogo].mode = PIXEL_INTERPOLATION;
          if (delogo == 1) {
            CONFIGTRACE_E("%s\n","ERROR, delogo params 1 not support PIXEL_INTERPOLATION");
            return 1;
          }
        } else {
          CONFIGTRACE_E("%s\n","ERROR, no supported mode");
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "YUV") == 0) {
        if (ParsePixelParams(optarg, &(params->delogo_params[delogo].Y),
                                     &(params->delogo_params[delogo].U),
                                     &(params->delogo_params[delogo].V))) {
          CONFIGTRACE_E("ERROR: Illegal parameters: %s\n",optarg);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "convert-tiled-out") == 0) {
        params->convert_tiled_output = 1;
      } else if (strcmp(long_options[option_index].name, "save-index") == 0) {
        params->save_index = 1;
      } else if (strcmp(long_options[option_index].name, "num-buffers") == 0) {
        params->num_buffers = atoi(optarg);
        if (params->num_buffers < 1 || params->num_buffers >= MAX_DPB_SIZE) {
          CONFIGTRACE_E("Illegal parameter: --buffers=%d\n", params->num_buffers);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "skip-frame") == 0) {
        if (strcmp(optarg, "non_ref_recon") == 0)
          params->skip_frame = DEC_SKIP_NON_REF_RECON;
        else if (strcmp(optarg, "non_ref") == 0)
          params->skip_frame = DEC_SKIP_NON_REF;
        else if (strcmp(optarg, "none") == 0) {
          params->skip_frame = DEC_SKIP_NONE;
        } else {
          CONFIGTRACE_E("Illegal parameter: --skip-frame=%s\n", optarg);
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "index-file") == 0) {
        params->index_file_name = (char *)malloc(255);
        if (!params->index_file_name) {
          CONFIGTRACE_E("%s\n","ERROR: failed to create output file.");
          return 1;
        }
        strncpy(params->index_file_name, optarg, 255);
      } else if (strcmp(long_options[option_index].name, "lc-int-enable") == 0) {
        /* Do nothing, it's not supported any more. */
      } else if (strcmp(long_options[option_index].name, "lc-stripe") == 0) {
        params->ppu_cfg[pp].lc_stripe = atoi(optarg);
        params->ppu_cfg[pp].enabled = 1;
        params->pp_enabled = 1;
      } else if(strcmp(long_options[option_index].name, "input-buffer-size") == 0) {
        params->input_buffer_size = atoi(optarg);
      } else if(strcmp(long_options[option_index].name, "ec") == 0) {
        if (strcmp(optarg, "no_error") == 0 || atoi(optarg) == 3)
          params->error_handling = DEC_EC_FRAME_NO_ERROR;
        else if (strcmp(optarg, "ignore_error") == 0 || atoi(optarg) == 2)
          params->error_handling = DEC_EC_FRAME_IGNORE_ERROR;
        else if (strcmp(optarg, "tolerant_error") == 0 || atoi(optarg) == 1) {
          params->error_handling = DEC_EC_FRAME_TOLERANT_ERROR;
        } else {
          CONFIGTRACE_E("Illegal parameter: --ec=%s\n", optarg);
          return 1;
        }
#ifdef SUPPORT_RANDOM_LATENCY
      } else if (strcmp(long_options[option_index].name, "axi-lg") == 0) {
        pr = strchr(optarg, 'R');
        pw = strchr(optarg, 'W');
        if ((!pr) || (!pw)){
          CONFIGTRACE_E("%s\n","Illegal paramter");
          CONFIGTRACE_E("%s\n","ERROR: Enable axi latency by using: --axi-lg=R[r]W[w]");
          return 1;
        }
        *pw = '\0';
        params->axi_lg_r = atoi(pr + 1);
        params->axi_lg_w = atoi(pw + 1);
#endif
      } else if (strcmp(long_options[option_index].name, "error-ratio") == 0) {
        params->error_ratio = atoi(optarg);
        if (params->error_ratio > 100) {
          CONFIGTRACE_E("Illegal parameter: --error-ratio=%d\n", atoi(optarg));
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "tlayer") == 0) {
        params->max_temporal_layer = atoi(optarg);
        if (params->max_temporal_layer < -1 || params->max_temporal_layer > 7) {
          CONFIGTRACE_E("Illegal parameter: --tlayer=%d\n", atoi(optarg));
          return 1;
        }
      } else if (strcmp(long_options[option_index].name, "dpb-mode") == 0) {
        params->dpb_mode = DEC_DPB_INTERLACED_FIELD;
      } else if (strcmp(long_options[option_index].name, "convert-frame-out") == 0) {
        params->convert_to_frame_dpb = 1;
      } else if (strcmp(long_options[option_index].name, "use-extra-buffers") == 0) {
        params->use_extra_buffers = 1;
      } else if (strcmp(long_options[option_index].name, "alloc-extra-buffers") == 0) {
        params->use_extra_buffers = 0;
        params->allocate_extra_buffers_in_output = 1;
      } else if (strcmp(long_options[option_index].name, "stream-trace") == 0) {
        params->stream_trace = optarg;
      } else if (strcmp(long_options[option_index].name, "use-ref-idct") == 0) {
        params->use_ref_idct = 1;
      } else if (strcmp(long_options[option_index].name, "full-only") == 0) {
        params->only_full_resolution = 1;
      } else if (strcmp(long_options[option_index].name, "ri-mc") == 0) {
        params->ri_mc_enable = 1;
      } else if (strcmp(long_options[option_index].name, "instant-buffer") == 0) {
        params->instant_buffer = 1;
      } else if(strcmp(long_options[option_index].name, "snap-shot") == 0) {
        params->snap_shot = 1;
      } else if(strcmp(long_options[option_index].name, "extra-bits") == 0) {
        params->extra_strm_buffer_bits = atoi(optarg);
      } else if(strcmp(long_options[option_index].name, "user-mem-alloc") == 0) {
        params->user_mem_alloc = 1;
      } else if(strcmp(long_options[option_index].name, "alpha") == 0) {
        params->alpha = 1;
      } else if(strcmp(long_options[option_index].name, "strm-sorenson") == 0) {
        params->strm_fmt_sorenson = DEC_INPUT_SORENSON;
      } else if(strcmp(long_options[option_index].name, "strm-custom") == 0) {
        params->strm_fmt_custom = DEC_INPUT_CUSTOM_1;
      } else if(strcmp(long_options[option_index].name, "custom") == 0) {
        params->custom.dimensions = 1;
        params->custom.widthxheight = optarg;
      } else if(strcmp(long_options[option_index].name, "long-stream") == 0) {
        params->long_stream = 1;
      } else if(strcmp(long_options[option_index].name, "frame-picture") == 0) {
        params->enable_frame_picture = 1;
      } else if(strcmp(long_options[option_index].name, "rv-input") == 0) {
        params->rv_input = 1;
      } else if(strcmp(long_options[option_index].name, "multimode") == 0) {
        params->multimode = atoi(optarg);
      } else if(strcmp(long_options[option_index].name, "streamcfg") == 0) {
        params->streamcfg[params->nstrmcfg++] = optarg;
      } else if(strcmp(long_options[option_index].name, "dec-dev") == 0){
        params->dec_dev = optarg;
      } else if(strcmp(long_options[option_index].name, "mem-dev") == 0){
        params->mem_dev = optarg;
#ifdef SUPPORT_DMA
      } else if(strcmp(long_options[option_index].name, "dma-dev") == 0){
        params->dma_dev = optarg;
#endif
      } else if(strcmp(long_options[option_index].name, "logoutdir") == 0){
        params->logOutDir = atoi(optarg);
      } else if(strcmp(long_options[option_index].name, "logoutlevel") == 0){
        params->logOutLevel = atoi(optarg);
      } else if(strcmp(long_options[option_index].name, "logtracemap") == 0){
        params->logTraceMap = ParseLogTraceMap(optarg);
      } else if(strcmp(long_options[option_index].name, "is-asofmo") == 0) {
        params->support_asofmo_stream = 1;
      } else if(strcmp(long_options[option_index].name, "use-vcmd") == 0) {
        use_vcmd = 1;
      } else {
       CONFIGTRACE_E("Unknown option character `\\x%x'.\n", optopt);
       return 1;
      }
      break;
    case 1:
      params->ppu_cfg[pp].crop2.enabled = 1;
      break;
    case 6:
      params->decoder_mode |= DEC_INTRA_ONLY;
      break;
    case '?':
      if (isprint(optopt))
        CONFIGTRACE_E("Unknown option `-%c'.\n", optopt);
      else
        CONFIGTRACE_E("Unknown option character %s.\n", argv[optind - 1]);
      return 1;
    case 20:
      params->ppu_cfg[pp].enabled = 1;
      params->ppu_cfg[pp].enable_3dlut = 1;
      break;
    case 15:
      params->ppu_cfg[pp].enabled = 1;
      if(strcmp(long_options[option_index].name, "table-3dlut") == 0) {
        params->ppu_cfg[pp].enable_3dlut = 1;
        params->table_3dlut_name = optarg;
      } else
        CONFIGTRACE_E("%s\n","Illegal parameter");
      break;
    default:
      break;
    }
  }

  /* config video range from rgb_std. */
  if (!IS_FULL_RANGE(params->rgb_stan)) {
    params->video_range = 0;
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      params->ppu_cfg[i].video_range = 0;
    }
  }

  if (optind >= argc && !params->multimode) {
    CONFIGTRACE_E("%s\n","Invalid or no input file specified");
    return 1;
  }

  if (argv[optind]) strcpy(params->in_file_name, argv[optind]);
#ifdef VCD_LOGMSG
  VCDecLogInit(params->logOutDir, params->logOutLevel, params->logTraceMap);
#endif

#ifdef PPU_V9_2_3
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params->ppu_cfg[i].enabled)
      continue;
    if (params->ppu_cfg[i].pp_filter == BI_LINEAR && params->ppu_cfg[i].antialias == 0) {
      params->pp_filter = params->ppu_cfg[i].pp_filter = FAST_LINEAR;
    }
    if (params->ppu_cfg[i].pp_filter == BICUBIC && params->ppu_cfg[i].antialias == 0) {
      params->pp_filter = params->ppu_cfg[i].pp_filter = FAST_BICUBIC;
    }
    if (!(params->ppu_cfg[i].pp_filter == FAST_LINEAR || params->ppu_cfg[i].pp_filter == FAST_BICUBIC) &&
        params->ppu_cfg[i].antialias == 0) {
      CONFIGTRACE_W("%s\n",
              "WARNING: option --antialias only works when --pp-filter=BILINEAR or --pp-filter=BICUBIC.");
    }
  }
#endif

  if (ResolveOverlap(params)) return 1;

  (void)flag_b;
  return 0;
}

int ResolveOverlap(struct TestParams* params) {
  if (params->mc_enable && params->read_mode != STREAMREADMODE_FRAME) {
    CONFIGTRACE_E("%s\n",
            "Overriding read_mode to FRAME mode "
            "when multicore decoding (--mc) is enabled.");
    params->read_mode = STREAMREADMODE_FRAME;
  }

  if ((params->decoder_mode & DEC_LOW_LATENCY) && params->is_ringbuffer) {
    CONFIGTRACE_E("%s\n",
            "Overriding input buffer to no-ringbuffer mode "
            "when low latency is enabled.");
    params->is_ringbuffer = 0;
  }

  if ((params->decoder_mode & DEC_LOW_LATENCY) && (params->decoder_mode & DEC_SECURITY)) {
    CONFIGTRACE_E("%s\n",
            "ERROR: options --low-latency and --secure are mutually "
            "exclusive!");
    return 1;
  }

  if (ResolvePpParamsOverlap(params, 0)) return 1;

  return 0;
}

int ResolvePpParamsOverlap(struct TestParams* params,
                           u32 standalone) {
  u32 i;

  //nomatter pp0 enable, rgb cfg should be set, because we only have one set of rgb cfg
  params->ppu_cfg[0].video_range = params->video_range;
  params->ppu_cfg[0].rgb_stan = params->rgb_stan;

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    /* Command line pp options have higher priority:
       if one pp channel has been enabled in command line options, tb.cfg will be
       ignored for this channel(goto CHECK_PPU_CONFIG). */
    if (!params->ppu_cfg[i].enabled) continue;

    /* Command line pp options in here. */
    if (params->ppu_cfg[i].rgb && params->ppu_cfg[i].rgb_planar)
      params->ppu_cfg[i].rgb = 0;
    params->ppu_cfg[i].video_range = params->video_range;
    params->ppu_cfg[i].rgb_stan = params->rgb_stan;
    // params->ppu_cfg[i].pp_filter = params->pp_filter;
    params->ppu_cfg[i].x_filter_param = params->x_filter_param;
    params->ppu_cfg[i].y_filter_param = params->y_filter_param;
    params->ppu_cfg[i].align = params->align;
    params->ppu_cfg[i].align_h = params->align_h;
    if (params->ppu_cfg[i].scale.enabled &&
        params->ppu_cfg[i].scale.width &&
        params->ppu_cfg[i].scale.scale_by_ratio) {
      /* Scaling option -D will override -d */
      params->ppu_cfg[i].scale.scale_by_ratio = 0;
      params->ppu_cfg[i].scale.ratio_x = 0;
      params->ppu_cfg[i].scale.ratio_y = 0;
    }
    /* Either fixed ratio scaling, or fixed size scaling. */
    if (params->ppu_cfg[i].enabled &&
        !params->ppu_cfg[i].scale.enabled) {
      params->ppu_cfg[i].scale.scale_by_ratio = 1;
      params->ppu_cfg[i].scale.ratio_x = 1;
      params->ppu_cfg[i].scale.ratio_y = 1;
    }
    if (params->ppu_cfg[i].crop2.width && params->ppu_cfg[i].crop2.height) {
      params->ppu_cfg[i].crop2.enabled = 1;
    }
    if (params->ppu_cfg[i].crop11.width && params->ppu_cfg[i].crop11.height) {
      params->ppu_cfg[i].crop11.enabled = 1;
    }
    if (params->ppu_cfg[i].crop12.width && params->ppu_cfg[i].crop12.height) {
      params->ppu_cfg[i].crop12.enabled = 1;
    }
    if (params->ppu_cfg[i].crop13.width && params->ppu_cfg[i].crop13.height) {
      params->ppu_cfg[i].crop13.enabled = 1;
    }
    if ((params->ppu_cfg[i].out_p010 &&
        (params->ppu_cfg[i].out_cut_8bits || params->ppu_cfg[i].out_be)) ||
        (params->ppu_cfg[i].out_cut_8bits && params->ppu_cfg[i].out_be)) {
      CONFIGTRACE_E("%s\n",
              "ERROR: options -f or -Epbe and -Ep010 are mutually "
              "exclusive!");
      return 1;
    }
    if (params->ppu_cfg[i].out_1010 && (params->ppu_cfg[i].out_p010 ||
        params->ppu_cfg[i].out_cut_8bits || params->ppu_cfg[i].out_be)) {
      CONFIGTRACE_E("%s\n",
              "ERROR: options -E1010 -f or -Epbe and -Ep010 are mutually "
              "exclusive!");
      return 1;
    }
    if (params->ppu_cfg[i].out_I010 && (params->ppu_cfg[i].out_1010 ||
        params->ppu_cfg[i].out_p010 || params->ppu_cfg[i].out_L010 ||
        params->ppu_cfg[i].out_cut_8bits || params->ppu_cfg[i].out_be)) {
      CONFIGTRACE_E("%s\n",
              "ERROR: options -EI010 -E1010 -EL010 -f or -Epbe and -Ep010 are mutually "
              "exclusive!");
      return 1;
    }
    if (params->ppu_cfg[i].out_L010 && (params->ppu_cfg[i].out_1010 ||
        params->ppu_cfg[i].out_p010 || params->ppu_cfg[i].out_I010 ||
        params->ppu_cfg[i].out_cut_8bits || params->ppu_cfg[i].out_be)) {
      CONFIGTRACE_E("%s\n",
              "ERROR: options -EL010 -EI010 -E1010 -f or -Epbe and -Ep010 are mutually "
              "exclusive!");
      return 1;
    }
    if ((params->ppu_cfg[i].rgb || params->ppu_cfg[i].rgb_planar) && (params->ppu_cfg[i].planar || (params->ppu_cfg[i].tiled_e && params->ppu_cfg[i].tile_mode != 7))) {
      CONFIGTRACE_E("%s\n",
              "ERROR: options --pp_rgb or --pp_rgb_planar and --pp-planar or --pp-tiled-out are mutually "
              "exclusive!");
      return 1;
    }

    params->pp_enabled = 1;

    if (params->ppu_cfg[i].enabled) {
      params->ppu_cfg[0].align = params->align;
      params->ppu_cfg[0].align_h = params->align_h;
    }
  }

  if (standalone) { /* pp standalone mode */
    params->pp_standalone = 1;

    if (!params->pp_enabled) {
      /* No pp enabled explicitly, then enable fixed ratio pp (1:1) */
      params->ppu_cfg[0].enabled = 1;
      params->ppu_cfg[0].scale.enabled = 1;
      params->ppu_cfg[0].scale.ratio_x = 1;
      params->ppu_cfg[0].scale.ratio_y = 1;
      params->ppu_cfg[0].scale.scale_by_ratio = 1;
      params->pp_enabled = 1;
    }
  }

  return (0);
}

/*------------------------------------------------------------------------------
-- The 3DLUT input table file should be a 17*17*17 lines *.txt file, where    --
-- each line has the form:                                                    --
--                        index0,index1,index2:R,G,B,                         --
--   * The three numbers separated by commas before the colon represent the   --
--       index of R/G/B.                                                      --
--   * After the colon, the three numbers separated by commas represent the   --
--       value of R/G/B.                                                      --
--   * The index here are for the convenience of showing the order of R/G/B   --
--       increment, and will not be downloaded to buffer, while the values of --
--       R/G/B will be downloaded to buffer for look up.                      --
--   * The index increase order is index2 (B dimension) increases firstly     --
--       from 0 to 16, then index 1 (G dimension) increases from 0 to 16, and --
--       index 0 (R dimension) increases from 0 to 16 at last.                --
------------------------------------------------------------------------------*/
void dump3DLutData(u16 threeDLutTableData[][3], char *fileName) {
  u16 threeDLutTableDataTmp[17*17*17][3] = {{0}};
  FILE* fp = fopen(fileName, "r");

  char c[100];
  char buf[20];
  char m[10], n[10], p[10];
  u16 a, b, d, i = 0;
  int ret = EOF + 1;

  printf("Dump3DLUT begin:\n");
  if (fp != NULL) {
    while(ret != EOF) {
      if(i == 4913)
        break;

      ret = fscanf(fp, "%99s", c);
      sscanf(c, "%*[^:]:%19[^/n]", buf);
      sscanf(buf, "%9[^,]", m);
      // a = str2int(m, 0);
      a = atoi(m);
      //printf("Value: %d\n", a);
      threeDLutTableDataTmp[i][2] = a;

      sscanf(buf, "%*[^,],%19s", buf);
      sscanf(buf, "%9[^,]", n);
      // b = str2int(n, 0);
      b = atoi(n);
      //printf("Value: %d\n", b);
      threeDLutTableDataTmp[i][1] = b;

      sscanf(buf, "%*[^,],%19s", buf);
      sscanf(buf, "%9[^,]", p);
      // d = str2int(p, 0);
      d = atoi(p);
      //printf("Value: %d\n", d);
      threeDLutTableDataTmp[i][0] = d;

      i++;
    }
    printf("Dump3DLUT end\n");

    for(u32 r_index = 0; r_index < 17; r_index++) {
      for(u32 g_index = 0; g_index < 17; g_index++) {
        for(u32 b_index = 0; b_index < 17; b_index++ ) {
          threeDLutTableData[r_index*18*18 + g_index*18 + b_index][0] = threeDLutTableDataTmp[r_index*17*17 + g_index*17 + b_index][2];
          threeDLutTableData[r_index*18*18 + g_index*18 + b_index][1] = threeDLutTableDataTmp[r_index*17*17 + g_index*17 + b_index][1];
          threeDLutTableData[r_index*18*18 + g_index*18 + b_index][2] = threeDLutTableDataTmp[r_index*17*17 + g_index*17 + b_index][0];
        }
      }
    }
    fclose(fp);
  }
}

void calculate3DLutTableFor2020To709(u16 Lut3DTableData[][3]) {
  float r, g, b;
  float r2, g2, b2;
  // fit 17*17*17 real data in 18*18*18 matrix
  for(u32 r_index = 0; r_index < 17; r_index++) {
    for(u32 g_index = 0; g_index < 17; g_index++) {
      for(u32 b_index = 0; b_index < 17; b_index++ ) {
        r = pow(r_index/16.f, 2.4); //Display linear
        g = pow(g_index/16.f, 2.4);
        b = pow(b_index/16.f, 2.4);

        //2020 to 709
        r2 = 1.66051f*r   - 0.58771f*g  - 0.0728006f*b;
        g2 =-0.124561f*r  + 1.13296f*g  - 0.0083991f*b;
        b2 =-0.0181677f*r - 0.100561f*g + 1.11873f*b;

        if(r2 < 0)
            r2 = 0;
        if(r2 > 1.f)
            r2 = 1.f;

        if(g2 < 0)
            g2 = 0;
        if(g2 > 1.f)
            g2 = 1.f;

        if(b2 < 0)
            b2 = 0;
        if(b2 > 1.f)
            b2 = 1.f;

        r2 = pow(r2, 1./2.4);
        g2 = pow(g2, 1./2.4);
        b2 = pow(b2, 1./2.4);
        Lut3DTableData[r_index*18*18 + g_index*18 + b_index][0] = (u8)(r2*254.99f+0.5);
        Lut3DTableData[r_index*18*18 + g_index*18 + b_index][1] = (u8)(g2*254.99f+0.5);
        Lut3DTableData[r_index*18*18 + g_index*18 + b_index][2] = (u8)(b2*254.99f+0.5);
      }
    }
  }
}
