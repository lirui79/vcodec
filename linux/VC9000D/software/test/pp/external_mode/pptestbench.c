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
#define _SW_DEBUG_PRINT

#include "vcdecapi.h"
#include "dwl.h"
#include "dwlthread.h"
#include "ppapi.h"
#include <time.h>
#include <signal.h>
#include <ctype.h>
#include "basetype.h"
#include "tb_cfg.h"
#include "regdrv.h"
#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif
#include "common_sink.h"
#include "trace_hooks.h"
#include "command_line_parser.h"
#include "dec_log.h"
#include "vcd_tools.h"

#define MAX4(a, b, c, d) MAX(MAX(a, b), MAX(c, d))

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/
DecPicAlignment align = DEC_ALIGN_16B;  /* default: 16 bytes alignment */
DecPicAlignment align_h = DEC_ALIGN_16B;  /* default: 16 bytes alignment */
struct TBCfg tb_cfg;
FILE *finput = NULL;

/* These global values are found from commonconfig.c.
 * partial tb_cfg.dec_params parameters. */
extern struct DecParams dec_params;
/* secure mode */
u32 secure_mode;

u32 md5sum = 0;
u32 pixel_width = 8;
u32 pixel_width_pp = 8;
u32 luma_bit_depth = 8;
u32 chroma_bit_depth = 8;
u32 pp_enabled = 0;
u32 rgb_enabled = 0;
/* user input arguments */
u32 scale_enabled = 0;
u32 scaled_w, scaled_h;
u32 crop_enabled = 0;
u32 crop_x = 0;
u32 crop_y = 0;
u32 crop_w = 0;
u32 crop_h = 0;
const char* in_file_name = NULL;
/* for RFC Decompress */
const char* in_luma_data = NULL;
const char* in_chroma_data = NULL;
const char* in_luma_table = NULL;
const char* in_chroma_table = NULL;
const char* in_table = NULL;
FILE *finput_luma = NULL;
FILE *finput_chroma = NULL;
FILE *finput_luma_table = NULL;
FILE *finput_chroma_table = NULL;
FILE *finput_table = NULL;

PPInst pp_inst = NULL;
YuvSink* yuvsink; /* Yuvsink instance. */

#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define ALIGN(a) (1 << (a))
#define DOWN_SCALE_SIZE(w, ds) (((w)/(ds)) & ~0x1)
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define DEC400_IN_HEADER_SIZE (128)

static u64 CalcOnePpUnitPVFBCPlane0Size(PpUnitConfig *ppu_cfg) {
  u64 pp_height = 0, pp_width = 0;
  u64 payload_size, header_size, plane0_size;
  if (ppu_cfg->crop2.enabled){
    pp_width = ppu_cfg->crop2.width;
    pp_height = ppu_cfg->crop2.height;
  } else {
    pp_width = ppu_cfg->scale.width;
    pp_height = ppu_cfg->scale.height;
  }
  pp_width = ppu_cfg->out_p010 ? pp_width * 2 : pp_width;
  payload_size = NEXT_MULTIPLE(pp_width, 64) * NEXT_MULTIPLE(pp_height, 4);
  header_size = NEXT_MULTIPLE(payload_size / 256, 256);
  plane0_size = payload_size + header_size;
  return plane0_size;
}
static u64 CalcOnePpUnitPVFBCPlane1Size(PpUnitConfig *ppu_cfg, u32 mono_chrome) {
  u64 pp_height = 0, pp_width = 0;
  u64 payload_size = 0, header_size = 0, plane1_size = 0;
  u8 sub_x, sub_y;
  if (!ppu_cfg->monochrome && !mono_chrome && !ppu_cfg->rgb &&
    !ppu_cfg->rgb_planar && !ppu_cfg->out_yuyv && !ppu_cfg->out_uyvy &&
    (ppu_cfg->tile_mode != TILED16x16)) {
    if (ppu_cfg->chroma_format == PP_YUV420) {
      sub_x = sub_y = 2;
    } else if (ppu_cfg->chroma_format == PP_YUV422) {
      sub_x = 2;
      sub_y = 1;
    } else {
      sub_x = sub_y = 1;
    }
    if (ppu_cfg->crop2.enabled) {/*semiplanar: only support 420 semiplanar*/
      pp_width = sub_x == 2 ? NEXT_MULTIPLE(ppu_cfg->crop2.width, 2) : ppu_cfg->crop2.width * 2;
      pp_height = sub_y == 2 ? (ppu_cfg->crop2.height + 1) / 2 : ppu_cfg->crop2.height;
    } else {
      pp_width = sub_x == 2 ? NEXT_MULTIPLE(ppu_cfg->scale.width , 2) : ppu_cfg->scale.width * 2;
      pp_height = sub_y == 2 ? (ppu_cfg->scale.height + 1) / 2 : ppu_cfg->scale.height;
    }
    pp_width = ppu_cfg->out_p010 ? pp_width * 2 : pp_width;
    payload_size = NEXT_MULTIPLE(pp_width, 64) * NEXT_MULTIPLE(pp_height, 4);
    header_size = NEXT_MULTIPLE(payload_size / 256, 256);
    plane1_size = payload_size + header_size;
  }
  return plane1_size;
}
/*------------------------------------------------------------------------------

    Function name:  main

    Purpose:
        main function of decoder testbench. Provides command line interface
        with file I/O for H.264 decoder. Prints out the usage information
        when executed without arguments.

------------------------------------------------------------------------------*/

int main(int argc, char **argv) {

  u32 i, j, m, n;
  u32 pp_width, pp_height, pp_stride, pp_stride_ch = 0;
  long int strm_len;
  int ra;
  enum DecRet ret;
  struct DecPictures dec_picture;
  DWLmemset(&dec_picture, 0, sizeof(struct DecPictures));
  const void *dwl = NULL;
  struct DWLInitParam dwl_init = { DWL_CLIENT_TYPE_ST_PP };
  const struct DecHwFeatures *hw_feature = NULL;
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig dec_cfg;
  DWLmemset(&dec_cfg, 0, sizeof(struct DecConfig));
  u32 pic_num = 0;
  u32 decode_pic_num = 0;
  u32 max_num_pics = 0;
  u32 in_stride = 0;
  u32 in_stride_ch = 0;

  /* for RFC Decompress */
  u32 luma_in_stride = 0;
  u32 chroma_in_stride = 0;

  u32 pixel_width = 8;
  u32 pixel_width_pp = 8;
  u32 yuv_stride = 0;
  u32 yuv_stride_ch = 0;
  u32 in_height = 0;
  u32 in_width = 0;
  u32 in_p010 = 0;
  u32 in_I010 = 0;
  u32 in_rgb = 0;
  // u32 rgb_format = 0;
  // u32 input_pixel_planar = 0;
  u32 in_format = PP_IN_NV12_8BIT;
  u32 in_tiled4x4 = 0;
  u8 in_tiled_mode = 0;
  u32 pp_buff_size = 0, buff_size = 0, new_buff_size = 0;
  u32 in_rfc = 0;
  u32 in_dec400 = 0;
  u32 input_lu_size = 0;
  u32 input_ch_size = 0;
  u32 dec400_lu_ts_size = 0;
  u32 dec400_ch_ts_size = 0;
  u32 in_dec400_a = 0;
  u32 if_table_merge = 0;
  u32 dec400_enabled[DEC_MAX_OUT_COUNT] = {0};
  u32 pp_pvfbc[DEC_MAX_OUT_COUNT] = {0};

  FILE *f_tbcfg = NULL;
  FILE *in_ctrl = NULL;
  FILE *osd_file = NULL;
#ifndef ENABLE_FPGA_VERIFICATION
  FILE *fp_dump_data = NULL;
#endif
  u32 ctrl_flag = 0;
  u32 len;
  u32 * pic_ctrl = NULL;
  char pic_name[256] = {0};
  struct DWLLinearMem pp_in_buffer = {0};
  struct DWLLinearMem pp_out_buffer = {0};
  struct TestParams params;
  struct OutFileInfo outfile_info = {0};
  u32 tmp;
  (void)tmp;
  u8 enable_3dlut = 0;
  u8 sub_x, sub_y;
  u8 *input_va;

  int tmp_argc = 0;
  char *tmp_argv[64] = {argv[0], NULL};

  DEBUG_PRINT(("\n[TB] * * * * * * * * * * * * * * * * \n\n\n[TB] "
          "      "
          "PP TESTBENCH\n" "\n\n[TB] * * * * * * * * * * * * * * * * \n"));

  /* YUV420 */
  // u32 sub_x = 2;
  // u32 sub_y = 2;
  u8 in_chroma_format_idc = 1; // yuv420 default
  SetupDefaultParams(&params);
  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  f_tbcfg = fopen("tb.cfg", "r");
  if(f_tbcfg == NULL) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT FILE: \"tb.cfg\"\n"));
    DEBUG_PRINT(("[TB] USING DEFAULT CONFIGURATION\n"));
  } else {
    fclose(f_tbcfg);
    if(TBParseConfig("tb.cfg", TBReadParam, &tb_cfg) == TB_FALSE)
      return -1;
    if(TBCheckCfg(&tb_cfg) != 0)
      return -1;
  }

  if (f_tbcfg != NULL) {
    /* this dec_params used in commonconfig.c */
    /* set parameters with dec_params */
    dec_params.bus_burst_length = tb_cfg.dec_params.bus_burst_length;
    dec_params.clk_gate_decoder = tb_cfg.dec_params.clk_gate_decoder;
    dec_params.non_seq_clk = tb_cfg.dec_params.non_seq_clk;
    dec_params.seq_clk = tb_cfg.dec_params.seq_clk;
    dec_params.apf_threshold_value = tb_cfg.dec_params.apf_threshold_value;
    dec_params.axi_wr_outstand = tb_cfg.dec_params.axi_wr_outstand;
    dec_params.axi_rd_outstand = tb_cfg.dec_params.axi_rd_outstand;
  }

  if(argc < 2) {
    PrintUsage(argv[0], PPDEC);
    return 0;
  }

  if (argc == 2) {
    if ((strncmp(argv[1], "-h", 2) == 0) ||
        (strcmp(argv[1], "--help") == 0)){
      PrintUsage(argv[0], PPDEC);
      return 0;
    }
  }

  /* read command line arguments: ppdec only */
  char temp[64];
  for (i = 1; i < (u32)argc; i++) {
    if(strncmp(argv[i], "-S", 2) == 0) {
      in_stride = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "--in-stride=", 12) == 0) {
      in_stride = (u32) atoi(argv[i] + 12);
    } else if(strncmp(argv[i], "-H", 2) == 0) {
      in_height = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "--height=", 9) == 0) {
      in_height = (u32) atoi(argv[i] + 9);
    } else if(strncmp(argv[i], "-W", 2) == 0) {
      in_width = (u32) atoi(argv[i] + 2);
    } else if(strncmp(argv[i], "--width=", 8) == 0) {
      in_width = (u32) atoi(argv[i] + 8);
    } else if(strncmp(argv[i], "--IL=", 5) == 0) {    // input luma RFC data file
      strcpy(params.in_luma_file, argv[i] + 5);
      in_rfc = 1;
      // input_pixel_planar = 0;
    } else if (strncmp(argv[i], "--IC=", 5) == 0) {    // input chroma RFC data file
      strcpy(params.in_chroma_file, argv[i] + 5);
    } else if (strncmp(argv[i], "--LT=", 5) == 0) {    //input luma table file
      strcpy(params.in_luma_table_file, argv[i] + 5);
    } else if (strncmp(argv[i], "--CT=", 5) == 0) {    //input chroma table file
      strcpy(params.in_chroma_table_file, argv[i] + 5);
    } else if (strncmp(argv[i], "--AT=", 5) == 0) {    //input luma table + chroma table file
      strcpy(params.in_table_file, argv[i] + 5);
      if_table_merge = 1;
    } else if (strncmp(argv[i], "--LB=", 5) == 0) {    //input luma bitdepth
      strcpy(temp, argv[i] + 5);
      luma_bit_depth = atoi(temp);
    } else if (strncmp(argv[i], "--CB=", 5) == 0) {//input chroma bitdepth
      strcpy(temp, argv[i] + 5);
      chroma_bit_depth = atoi(temp);
    } else if (strncmp(argv[i], "--SL=", 5) == 0) {    //input luma stride
      strcpy(temp, argv[i] + 5);
      luma_in_stride = atoi(temp);
    } else if (strncmp(argv[i], "--SC=", 5) == 0) {    //input chroma stride
      strcpy(temp, argv[i] + 5);
      chroma_in_stride = atoi(temp);
    } else if (strncmp(argv[i], "--CHFMT", 7) == 0) {    //input chroma_format_idc
      strcpy(temp, argv[i] + 8);
      in_chroma_format_idc = atoi(temp);
    } else if (strcmp(argv[i], "--indec400") == 0) {
      in_dec400 = 1;
    } else if (strncmp(argv[i], "--TS=", 5) == 0) {    //input dec400 TS table
      strcpy(params.in_table_file, argv[i] + 5);
      if_table_merge = 1;
    } else if (strncmp(argv[i], "--TSL=", 6) == 0) {    //input dec400 luma table
      strcpy(params.in_luma_table_file, argv[i] + 6);
    } else if (strncmp(argv[i], "--TSC=", 6) == 0) {    //input dec400 chroma table
      strcpy(params.in_chroma_table_file, argv[i] + 6);
    } else if (strncmp(argv[i], "--indec400-align", 16) == 0) {
      strcpy(temp, argv[i] + 17);
      if (atoi(temp) == 64)
        in_dec400_a = DEC_ALIGN_64B;
      else
        in_dec400_a = DEC_ALIGN_32B;
      in_dec400 = 1;
    } else if(strcmp(argv[i], "--inI010") == 0) {
      in_p010 = 1;
      in_I010 = 1;
    } else if ((strncmp(argv[i], "--in-fmt", 8) == 0)) {
      if (strcmp(argv[i]+9, "NV12") == 0) {
        in_format = PP_IN_NV12_8BIT;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "420P") == 0) {
        in_format = PP_IN_420P_8BIT;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "NV12_P010") == 0) {
        in_p010 = 1;
        in_format = PP_IN_NV12_P010;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "420P_P010") == 0) {
        in_p010 = 1;
        in_format = PP_IN_420P_P010;
        in_chroma_format_idc = 1;
      }
#ifdef PPU_V9_2_3
      else if (strcmp(argv[i]+9, "NV21") == 0) {
        in_format = PP_IN_NV21_8BIT;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "422NV16") == 0) {
        in_format = PP_IN_422_NV16_8BIT;
        in_chroma_format_idc = 2;
      } else if (strcmp(argv[i]+9, "422YUYV") == 0) {
        in_format = PP_IN_422_YUYV_8BIT;
        in_chroma_format_idc = 2;
      } else if (strcmp(argv[i]+9, "422YVYU") == 0) {
        in_format = PP_IN_422_YVYU_8BIT;
        in_chroma_format_idc = 2;
      } else if (strcmp(argv[i]+9, "422UYVY") == 0) {
        in_format = PP_IN_422_UYVY_8BIT;
        in_chroma_format_idc = 2;
      } else if (strcmp(argv[i]+9, "422VYUY") == 0) {
        in_format = PP_IN_422_VYUY_8BIT;
        in_chroma_format_idc = 2;
      } /*else if (strcmp(argv[i]+9, "422P") == 0) {
        //input_pixel_planar = 1;
        in_format = PP_IN_422P_8BIT;
      } else if (strcmp(argv[i]+9, "444NV24") == 0) {
        //input_pixel_planar = 0;
        in_format = PP_IN_444_NV24_8BIT;
      } else if (strcmp(argv[i]+9, "444NV42") == 0) {
        //input_pixel_planar = 0;
        in_format = PP_IN_444_NV42_8BIT;
      } else if (strcmp(argv[i]+9, "444P") == 0) {
        //input_pixel_planar = 1;
        in_format = PP_IN_444P_8BIT;
      } */
#endif
      else if (strcmp(argv[i]+9, "TILED4x4") == 0) {
        in_tiled4x4 = 1;
        in_format = PP_IN_TILED4x4;
        in_chroma_format_idc = 1;
      }
#ifdef PPU_V9_2_3
      else if (strcmp(argv[i]+9, "TILED8x8") == 0) {
        in_tiled_mode = IN_TILED8x8;
        in_format = PP_IN_NV12_8BIT;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "TILED8x8_YUV400") == 0) {
        in_tiled_mode = IN_TILED8x8;
        in_format = PP_IN_NV12_8BIT;
        in_chroma_format_idc = 0;
      } else if (strcmp(argv[i]+9, "TILED8x8_P010") == 0) {
        in_tiled_mode = IN_TILED8x8;
        in_format = PP_IN_NV12_P010;
        in_p010 = 1;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "TILED8x8_P010_YUV400") == 0) {
        in_tiled_mode = IN_TILED8x8;
        in_format = PP_IN_NV12_P010;
        in_p010 = 1;
        in_chroma_format_idc = 0;
      } else if (strcmp(argv[i]+9, "TILED64x64_ARGB888") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_format = PP_IN_ARGB888;
      } else if (strcmp(argv[i]+9, "TILED64x64_ABGR888") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_format = PP_IN_ABGR888;
      } else if (strcmp(argv[i]+9, "TILED64x64_XRGB888") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_format = PP_IN_XRGB888;
      } else if (strcmp(argv[i]+9, "TILED64x64_XBGR888") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_format = PP_IN_XBGR888;
      } else if (strcmp(argv[i]+9, "TILED64x64_A2R10G10B10") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_p010 = 1;
        in_format = PP_IN_A2R10G10B10;
      } else if (strcmp(argv[i]+9, "TILED64x64_A2B10G10R10") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_p010 = 1;
        in_format = PP_IN_A2B10G10R10;
      } else if (strcmp(argv[i]+9, "TILED64x64_X2R10G10B10") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_p010 = 1;
        in_format = PP_IN_X2R10G10B10;
      } else if (strcmp(argv[i]+9, "TILED64x64_X2B10G10R10") == 0) {
        in_tiled_mode = IN_TILED64x64;
        in_rgb = 1;
        in_p010 = 1;
        in_format = PP_IN_X2B10G10R10;

      } else if (strcmp(argv[i]+9, "NV21_P010") == 0) {
        in_p010 = 1;
        in_format = PP_IN_NV21_P010;
        in_chroma_format_idc = 1;
      } else if (strcmp(argv[i]+9, "422NV16_P010") == 0) {
        in_p010 = 1;
        in_format = PP_IN_422_NV16_P010;
        in_chroma_format_idc = 2;
      } /*else if (strcmp(argv[i]+9, "422P_P010") == 0) {
        in_p010 = 1;
        //input_pixel_planar = 1;
        in_format = PP_IN_422P_P010;
      } else if (strcmp(argv[i]+9, "444NV24_P010") == 0) {
        in_p010 = 1;
        //input_pixel_planar = 0;
        in_format = PP_IN_444_NV24_P010;
      } else if (strcmp(argv[i]+9, "444NV42_P010") == 0) {
        in_p010 = 1;
        //input_pixel_planar = 0;
        in_format = PP_IN_444_NV42_P010;
      } else if (strcmp(argv[i]+9, "444P_P010") == 0) {
        in_p010 = 1;
        //input_pixel_planar = 1;
        in_format = PP_IN_444P_P010;
      }*/
#endif
      else if (strcmp(argv[i]+9, "TILED4x4_P010") == 0) {
        in_p010 = 1;
        in_tiled4x4 = 1;
        in_format = PP_IN_TILED4x4_P010;
      } else if (strcmp(argv[i]+9, "RGB888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_RGB888;
      } else if (strcmp(argv[i]+9, "BGR888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_BGR888;
      } else if (strcmp(argv[i]+9, "ARGB888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_ARGB888;
      } else if (strcmp(argv[i]+9, "ABGR888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_ABGR888;
      } else if (strcmp(argv[i]+9, "XRGB888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_XRGB888;
      } else if (strcmp(argv[i]+9, "XBGR888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_XBGR888;
      } else if (strcmp(argv[i]+9, "RGBA888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_RGBA888;
      } else if (strcmp(argv[i]+9, "BGRA888") == 0) {
        in_rgb = 1;
        in_format = PP_IN_BGRA888;
      } else if (strcmp(argv[i]+9, "A2R10G10B10") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_A2R10G10B10;
      } else if (strcmp(argv[i]+9, "A2B10G10R10") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_A2B10G10R10;
      } else if (strcmp(argv[i]+9, "X2R10G10B10") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_X2R10G10B10;
      } else if (strcmp(argv[i]+9, "X2B10G10R10") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_X2B10G10R10;
      } else if (strcmp(argv[i]+9, "R10G10B10A2") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_R10G10B10A2;
      } else if (strcmp(argv[i]+9, "B10G10R10A2") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_B10G10R10A2;
      }
#ifdef PPU_V9_2_3
      else if (strcmp(argv[i]+9, "RGB888_P") == 0) {
        in_rgb = 1;
        in_format = PP_IN_RGB888_P;
      } else if (strcmp(argv[i]+9, "BGR888_P") == 0) {
        in_rgb = 1;
        in_format = PP_IN_BGR888_P;
      } else if (strcmp(argv[i]+9, "R16G16B16_P") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_R16G16B16_P;
      } else if (strcmp(argv[i]+9, "B16G16R16_P") == 0) {
        in_p010 = 1;
        in_rgb = 1;
        in_format = PP_IN_B16G16R16_P;
      }
#endif
      else {
        fprintf(stdout, "[TB] Illegal input format: %s\n", argv[i]);
        return 1;
      }
    } else if(strncmp(argv[i], "--sizes", 7) == 0) {
      char *px = strchr(argv[i]+7, '=');
      strcpy(pic_name, px+1);
    } else {
      tmp_argv[++tmp_argc] = argv[i];
    }
  }

  if (in_rfc) {
    in_luma_data = params.in_luma_file;
    in_chroma_data = params.in_chroma_file;
    if (!if_table_merge) {
      in_luma_table = params.in_luma_table_file;
      in_chroma_table = params.in_chroma_table_file;
    } else
      in_table = params.in_table_file;

    pixel_width = (chroma_bit_depth == 8 && luma_bit_depth == 8) ? 8 : 10;
    if (pixel_width == 10) {
      in_p010 = 1;
    }
    dec_cfg.in_chroma_bitdepth = chroma_bit_depth;
    dec_cfg.in_luma_bitdepth = luma_bit_depth;
    dec_cfg.in_format = PP_IN_RFC;
    tmp_argv[++tmp_argc] = (char *)in_luma_data;
  } else if (in_dec400) {
    in_table = params.in_table_file;
  }

  /* Use common command line parser to parse options. */
  i32 ret_tmp;
  if ((ret_tmp = ParseParams(tmp_argc + 1, tmp_argv, &params))) {
    if (ret_tmp == 1) {
      printf("Failed to parse params.\n\n");
      PrintUsage(argv[0], PPDEC);
      ret_tmp = 1;
      return 1;
    }
    else {
      ret_tmp = 0;
      return 0;
    }
  }

  /* get the value form params */
  max_num_pics = params.num_of_decoded_pics;
  pp_enabled = params.pp_enabled;

  if (in_rfc && in_luma_data && (in_luma_table || in_table)) {
    /* open RFC data and table for reading */
    finput_luma = fopen(in_luma_data, "rb");
    // finput = finput_luma;
    if(finput_luma == NULL) {
      DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT LUMA RFC FILE\n"));
      goto end;
    }

    if (in_luma_table != NULL) {

      finput_luma_table = fopen(in_luma_table, "rb");

      if(finput_luma_table == NULL) {
        DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT LUMA RFC TABLE FILE\n"));
        goto end;
      }
    } else if (in_table) {
      finput_table = fopen(in_table, "rb");
      if(finput_table == NULL) {
        DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT RFC TABLE FILE\n"));
        goto end;
      }
    }

    if (in_chroma_format_idc && !in_rgb) {
      if (in_chroma_data && (in_chroma_table || in_table)) {
        finput_chroma = fopen(in_chroma_data, "rb");
        if(finput_chroma == NULL) {
          DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT CHROMA RFC FILE\n"));
          goto end;
        }

        if(in_chroma_table != NULL) {
          finput_chroma_table = fopen(in_chroma_table, "rb");
          if(finput_chroma_table == NULL) {
            DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT CHROMA RFC TABLE FILE\n"));
            goto end;
          }
        }
      }
    }
  } else if (in_dec400) {
    if (in_table == NULL && in_luma_table == NULL) {
      DEBUG_PRINT(("[TB] NEED INPUT TS TABLE WHEN INPUT IS DEC400 DATA\n"));
      goto end;
    }
    if(in_table != NULL) {
      finput_table = fopen(in_table, "rb");
      if(finput_table == NULL) {
        DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT TS TABLE FILE\n"));
        goto end;
      }
    } else {
      if (in_luma_table != NULL) {
        finput_luma_table = fopen(in_luma_table, "rb");
        if(finput_luma_table == NULL) {
          DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT DEC400 LUMA TABLE FILE\n"));
          goto end;
        }
      }

      if(in_chroma_format_idc && in_chroma_table != NULL) {
        finput_chroma_table = fopen(in_chroma_table, "rb");
        if(finput_chroma_table == NULL) {
          DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT DEC400 CHROMA TABLE FILE\n"));
          goto end;
        }
      }
    }
    finput = fopen(params.in_file_name, "rb");
    if(finput == NULL) {
      DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT FILE\n"));
      goto end;;
    }

  } else {

    /******** PHASE ********/
    DEBUG_PRINT(("\n[TB] PHASE 0: OPEN/READ FILE \n"));

    /* open input file for reading, file name given by user. If file open
   * fails -> exit */
    finput = fopen(params.in_file_name, "rb");
    if(finput == NULL) {
      DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT FILE\n"));
      return -1;
    }
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
  tb_cfg.pp_params.pipeline_e = 0; /* Disable pipeline mode for pp standalone tb. */
  alignwidth = 1 << params.align;
  alignheight = 1 << params.align_h;
  in_lib_name = params.in_lib_name;
#endif
#ifdef ASIC_TRACE_SUPPORT
  /* open tracefiles */
  if(!OpenTraceFiles()) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN TRACE FILE(S)\n"));
  }
#endif

  in_ctrl = fopen(pic_name, "r");
  if (in_ctrl == NULL) {
    ctrl_flag = 0;
    if (!in_width || !in_height) {
      fprintf(stdout, "[TB] ERROR: Input width/height must be specified by options "
                      "-W/-H.\n");
      return 1;
    }
    if(IS_PP_IN_YUV444(in_format)){
      sub_x = sub_y = 1;
    } else if (IS_PP_IN_YUV422(in_format) && !IS_PP_IN_422PACKED(in_format)){
      sub_x = 2;
      sub_y = 1;
    } else if (IS_PP_IN_YUV420(in_format))
      sub_x = sub_y = 2;

    if (in_stride) {
      if (IS_PP_IN_YUV_PLANAR(in_format)){
        in_stride_ch = sub_x == 2 ? (in_stride + 1) / 2 : in_stride;
      } else if(IS_PP_IN_YUV420(in_format) || IS_PP_IN_YUV422(in_format) || IS_PP_IN_YUV444(in_format)) {
        in_stride_ch = sub_x == 2 ? NEXT_MULTIPLE(in_stride, 2) : in_stride * 2;
      }
      if (in_dec400) {
        in_stride = NEXT_MULTIPLE(in_stride, 256); //dec400 always align to 256bytes
        if (in_stride_ch)
          in_stride_ch = NEXT_MULTIPLE(in_stride_ch, 256);
      }
    } else {
      if (in_rgb) {
        if (IS_PP_IN_RGB_PLANAR(in_format)) {
          in_stride = (in_width << in_p010);
        } else if (IS_PP_IN_ALPHA_X_FROMAT_RGB(in_format)) {
          in_stride = in_width * 4;
          if (in_tiled_mode == IN_TILED64x64) {
            in_stride = NEXT_MULTIPLE(in_width, 64) * 4 * 64;
            //in_height = NEXT_MULTIPLE(in_height, 64);
          }
        } else {
          in_stride = (in_width << in_p010) * 3; //packed
          // in_stride = in_width * 3;
        }
      } else if (in_tiled4x4) {
        in_stride = (in_width << in_p010) * 4;
      } else if (in_tiled_mode == IN_TILED8x8) {
        in_stride = (NEXT_MULTIPLE(in_width, 8) << in_p010) * 8;
        in_stride_ch = (NEXT_MULTIPLE((in_width + 1) / 2, 4) << in_p010) * 4 * 2;
      } else if (IS_PP_IN_422PACKED(in_format)) {
        in_stride = ((in_width * 2)<< in_p010);
      } else if (IS_PP_IN_YUV_PLANAR(in_format)){
        in_stride = (in_width << in_p010);
        in_stride_ch = (sub_x == 2 ? (in_width + 1) / 2 : in_width) << in_p010;
      } else {
        in_stride = (in_width << in_p010);
        in_stride_ch = ((sub_x == 2 ? NEXT_MULTIPLE(in_width, 2) : 2 * in_width) << in_p010);
      }
      if (in_dec400) {
        in_stride = NEXT_MULTIPLE(in_stride, 256); //dec400 always align to 256bytes
        if (in_stride_ch)
          in_stride_ch = NEXT_MULTIPLE(in_stride_ch, 256);
      }
    }
    if (!in_stride_ch) {
      if (IS_PP_IN_YUV_PLANAR(in_format)) {
        in_stride_ch = (sub_x == 2 ? (in_width + 1) / 2 : in_width) << in_p010;
      } else if (in_tiled_mode == IN_TILED8x8) {
        in_stride_ch = (NEXT_MULTIPLE((in_width + 1) / 2, 4) << in_p010) * 4 * 2;
      } else if (IS_PP_IN_YUV420(in_format) || (IS_PP_IN_YUV422(in_format) && !IS_PP_IN_422PACKED(in_format)) || IS_PP_IN_YUV444(in_format)) {
        in_stride_ch = (sub_x == 2 ? NEXT_MULTIPLE(in_width, 2) : in_width * 2) << in_p010;
      }
      if (in_dec400 && in_stride_ch) {
        in_stride_ch = NEXT_MULTIPLE(in_stride_ch, 256); //dec400 always align to 256bytes
      }
    }
  } else {
    ctrl_flag = 1;
    fseek(in_ctrl, 0L, SEEK_END);
    len = ftell(in_ctrl);
    rewind(in_ctrl);
    if (in_rfc)
      pic_num = len / 28;
    else
      pic_num = len / 20;
    pic_ctrl = (u32*)malloc(len);
    if (pic_ctrl == NULL || len == 0)
      return 1;
    ra = fread((u8*)(pic_ctrl), 1, len, in_ctrl);
    (void) ra;
  }

  /* check size of the input file -> length of the stream in bytes */
  if (!ctrl_flag) {
    if (in_rfc) {
        fseek(finput_luma, 0L, SEEK_END);
        pic_num = ftell(finput_luma) / (luma_in_stride * in_height / 8);
        rewind(finput_luma);
    } else {
      fseek(finput, 0L, SEEK_END);
      strm_len = ftell(finput);
      if (in_rgb) {
        if (IS_PP_IN_RGB_PLANAR(in_format)) {
          pic_num = (strm_len / (in_stride * in_height * 3));
        } else if (in_tiled_mode == IN_TILED64x64) {
          input_lu_size = in_stride * (NEXT_MULTIPLE(in_height, 64) / 64);
          input_ch_size = 0;
          pic_num = strm_len / (in_stride * (NEXT_MULTIPLE(in_height, 64) / 64));
        } else {
          pic_num = (strm_len / (in_stride * in_height)); //packed
        }
      } else {
        if (in_tiled4x4) {
          pic_num = strm_len / (in_stride * (NEXT_MULTIPLE(in_height, 4) / 4 + NEXT_MULTIPLE(in_height / 2, 4) / 4));
        } else if (in_tiled_mode == IN_TILED8x8) {
          /* for 420 tile8x8, cstride is stride/2 */
          input_lu_size = in_stride * NEXT_MULTIPLE(in_height, 8) / 8;
          input_ch_size = in_stride / 2 * NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4;
          pic_num = strm_len / (in_stride * (NEXT_MULTIPLE(in_height, 8) / 8 + NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4 / 2));
        } else {
          if(IS_PP_IN_YUV420(in_format)) {
            if(in_format == PP_IN_420P_8BIT || in_format == PP_IN_420P_P010)
              pic_num = (strm_len / (in_stride * in_height + in_stride_ch * NEXT_MULTIPLE(in_height, 2)));
            else
              pic_num = (strm_len / (in_stride * in_height + in_stride_ch * ((in_height + 1) / 2)));
          } else if(IS_PP_IN_422PACKED(in_format)){
            pic_num = (strm_len / (in_stride * in_height));
          } else if(IS_PP_IN_YUV422(in_format)) {
            pic_num = (strm_len / (in_stride * in_height + in_stride_ch * in_height));
          } else if(in_format == PP_IN_444P_8BIT || in_format == PP_IN_444P_P010) {
            pic_num = (strm_len / (in_stride * in_height + in_stride_ch * in_height * 2));
          }
        }
      }
      rewind(finput);
    }
  } else {
    if (in_rfc) {
      in_width = pic_ctrl[0];
      in_height = pic_ctrl[1];
      luma_bit_depth = dec_cfg.in_luma_bitdepth = pic_ctrl[2];
      chroma_bit_depth = dec_cfg.in_chroma_bitdepth = pic_ctrl[3];
      luma_in_stride = dec_cfg.in_lu_stride = pic_ctrl[4];
      chroma_in_stride = dec_cfg.in_ch_stride = pic_ctrl[5];
      in_chroma_format_idc = pic_ctrl[6];
      dec_cfg.in_lut_stride = (in_width / 8 + 16 - 1) / 16 * 16;
      dec_cfg.in_cht_stride = ((in_width + 16 - 1)  / 16 + 16 - 1) / 16 * 16;
      dec_cfg.in_rfc = in_rfc;
      dec_cfg.in_format = PP_IN_RFC;
      dec_cfg.in_chroma_format_idc = in_chroma_format_idc;
      if (in_chroma_format_idc) {
        fseek(finput_chroma, 0L, SEEK_END);
        if(ftell(finput_chroma) == 0) {
          goto end;
        }
        rewind(finput_chroma);
      }
    } else {
      in_rgb = IS_PP_IN_RGB(pic_ctrl[0]);
      in_format = pic_ctrl[0];
      in_p010 = !IS_PP_IN_8BIT(pic_ctrl[0]);
      in_tiled4x4 = IS_PP_IN_TILED4x4(pic_ctrl[0]);
      in_width = pic_ctrl[1];
      in_stride = pic_ctrl[2];
      in_height = pic_ctrl[3];
      in_stride_ch = pic_ctrl[4];
      dec_cfg.in_tiled_mode = in_tiled_mode;
      if (!in_rgb) {
        if (IS_PP_IN_YUV420(in_format)) {
          in_chroma_format_idc = PP_YUV420;
          dec_cfg.in_chroma_format_idc = PP_YUV420;
        } else if (IS_PP_IN_YUV422(in_format)) {
          in_chroma_format_idc = PP_YUV422;
          dec_cfg.in_chroma_format_idc = PP_YUV422;
        }
      }
    }
  }
  if (in_tiled4x4 && ((in_width % 4) || (in_height % 8))) {
    fprintf(stdout, "[TB] ERROR: Input width/height must be contained whole tiled4x4 "
                    "-W/-H.\n");
    return 1;
  }
  else if (in_tiled_mode == IN_TILED8x8 && ((in_width % 8) || (in_height % 8))) {
    fprintf(stdout, "[TB] ERROR: Input width/height must be contained whole tiled8x8 "
                    "-W/-H.\n");
    goto end;
  }
  if (in_dec400 && !(in_tiled_mode == IN_TILED8x8 || in_tiled_mode == IN_TILED64x64)) {
    fprintf(stdout, "[TB] ERROR: Dec400 input only support tiled8x8 or supertile "
                    "--indec400\n");
    goto end;
  }
  if (!in_dec400 && (in_tiled_mode == IN_TILED8x8 || in_tiled_mode == IN_TILED64x64)) {
    fprintf(stdout, "[TB] ERROR: Only support dec400 compressed tiled8x8 or supertile input,"
                    "please add --indec400 \n");
    goto end;
  }
  if (in_tiled_mode == IN_TILED64x64 && !IS_PP_IN_ALPHA_X_FROMAT_RGB(in_format)) {
    fprintf(stdout, "[TB] ERROR: tiled64x64 only support for ARGB "
                    "--pix-fmt=TILED64x64\n");
    goto end;
  }

  DEBUG_PRINT(("\n[TB] INPUT FILE INFORMATION: \n"));
  if (!in_rgb) {
    if(in_chroma_format_idc == 0) {
      DEBUG_PRINT(("[TB] \t-yuv_format: yuv400\n"));
    } else if(in_chroma_format_idc == 1) {
      DEBUG_PRINT(("[TB] \t-yuv_format: yuv420\n"));
    } else if(in_chroma_format_idc == 2) {
      DEBUG_PRINT(("[TB] \t-yuv_format: yuv422\n"));
    } else if(in_chroma_format_idc == 3) {
      DEBUG_PRINT(("[TB] \t-yuv_format: yuv444\n"));
    }
  }
  DEBUG_PRINT(("[TB] \t-is_in_rfc: %u\n", in_rfc));
  DEBUG_PRINT(("[TB] \t-is_in_dec400: %u\n", in_dec400));
  DEBUG_PRINT(("[TB] \t-is_in_rgb: %u\n", in_rgb));
  DEBUG_PRINT(("[TB] \t-is_in_p010: %u\n", in_p010));
  DEBUG_PRINT(("[TB] \t-is_in_I010: %u\n", in_I010));
  DEBUG_PRINT(("[TB] \t-is_in_tiled4x4: %u\n", in_tiled4x4));
  DEBUG_PRINT(("[TB] \t-is_in_tiled8x8: %d\n", in_tiled_mode == IN_TILED8x8));
  DEBUG_PRINT(("[TB] \t-is_in_tiled64x64: %d\n", in_tiled_mode == IN_TILED64x64));
  DEBUG_PRINT(("[TB] \t-pic_width: %u\n", in_width));
  DEBUG_PRINT(("[TB] \t-pic_height: %u\n", in_height));
  DEBUG_PRINT(("[TB] \t-luma_bit_depth: %u\n", luma_bit_depth));
  DEBUG_PRINT(("[TB] \t-chroma_bit_depth: %u\n", chroma_bit_depth));

  if(in_rfc) {
    pixel_width = (chroma_bit_depth == 8 && luma_bit_depth == 8) ? 8 : 10;
  } else if (in_p010) pixel_width = 10;

  DEBUG_PRINT(("[TB] \tThe total number of pictures to decode: %u\n", pic_num));
  DEBUG_PRINT(("[TB] \tThe length of the stream in bytes: %ld\n", strm_len));

#ifdef ASIC_TRACE_SUPPORT
  tb_cfg.pp_params.in_width = in_width;
  tb_cfg.pp_params.in_height = in_height;
#endif

  /* config video range from input_rgb_fmt. */
  if (!IS_FULL_RANGE(params.rgb_stan)) {
    params.video_range = 0;
    params.ppu_cfg[0].video_range = 0;
  }

  ResolvePpParamsOverlap(&params, !tb_cfg.pp_params.pipeline_e);

  pp_enabled = params.pp_enabled;
  memcpy(dec_cfg.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (pixel_width == 8) {
      dec_cfg.ppu_cfg[i].out_cut_8bits = 0;
      dec_cfg.ppu_cfg[i].out_p010 = 0;
      dec_cfg.ppu_cfg[i].out_1010 = 0;
      dec_cfg.ppu_cfg[i].out_I010 = 0;
      dec_cfg.ppu_cfg[i].out_L010 = 0;
    }
    #if 0
    // range_max/min will be rewritten at PpUnitSetIntConfig() in ppu.c
    // so here is useless
    if (dec_cfg.ppu_cfg[i].video_range) {
      if (pixel_width == 8) {
        dec_cfg.ppu_cfg[i].range_max = 255;
        dec_cfg.ppu_cfg[i].range_min = 0;
      } else {
        dec_cfg.ppu_cfg[i].range_max = 1023;
        dec_cfg.ppu_cfg[i].range_min = 0;
      }
    } else {
      if (pixel_width == 8) {
        dec_cfg.ppu_cfg[i].range_max = 235;
        dec_cfg.ppu_cfg[i].range_min = 16;
      } else {
        dec_cfg.ppu_cfg[i].range_max = 940;
        dec_cfg.ppu_cfg[i].range_min = 64;
      }
    }
    #endif
    if (dec_cfg.ppu_cfg[i].monochrome) {
      dec_cfg.ppu_cfg[i].rgb = 0;
      dec_cfg.ppu_cfg[i].rgb_planar = 0;
    }
    if (!in_chroma_format_idc) {
      dec_cfg.ppu_cfg[i].chroma_format = PP_YUV400;
    }
    if (dec_cfg.ppu_cfg[i].chroma_format == PP_YUV444) {
      dec_cfg.ppu_cfg[i].planar = 1;
      params.ppu_cfg[i].planar = 1;
    }

    if (!dec_cfg.ppu_cfg[i].crop.width || !dec_cfg.ppu_cfg[i].crop.height) {
      dec_cfg.ppu_cfg[i].crop.x = dec_cfg.ppu_cfg[i].crop.y = 0;
      dec_cfg.ppu_cfg[i].crop.width = in_width;
      dec_cfg.ppu_cfg[i].crop.height = in_height;
    }
    if (!dec_cfg.ppu_cfg[i].scale.width || !dec_cfg.ppu_cfg[i].scale.height) {
      if (dec_cfg.ppu_cfg[i].scale.ratio_x && dec_cfg.ppu_cfg[i].scale.ratio_y) {
        if (params.crop_align) {
          dec_cfg.ppu_cfg[i].scale.width = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[i].crop.width, dec_cfg.ppu_cfg[i].scale.ratio_x);
          dec_cfg.ppu_cfg[i].scale.height = DOWN_SCALE_SIZE(dec_cfg.ppu_cfg[i].crop.height, dec_cfg.ppu_cfg[i].scale.ratio_y);
          dec_cfg.ppu_cfg[i].scale.enabled = 1;
        } else {
          dec_cfg.ppu_cfg[i].scale.width = dec_cfg.ppu_cfg[i].crop.width / dec_cfg.ppu_cfg[i].scale.ratio_x;
          dec_cfg.ppu_cfg[i].scale.height = dec_cfg.ppu_cfg[i].crop.height / dec_cfg.ppu_cfg[i].scale.ratio_y;
          dec_cfg.ppu_cfg[i].scale.enabled = 1;
        }
      } else {
        dec_cfg.ppu_cfg[i].scale.width = dec_cfg.ppu_cfg[i].crop.width;
        dec_cfg.ppu_cfg[i].scale.height = dec_cfg.ppu_cfg[i].crop.height;
      }
    }
    if (params.ppu_cfg[i].enable_3dlut) {
      enable_3dlut = 1;
    }
  }
  if (!pp_enabled) {
    DEBUG_PRINT(("[TB] PP is not enabled. Finish.\n"));
    goto end;
  }


  /* Initialize decoder. If unsuccessful -> exit */
  dwl_init.dec_dev =  params.dec_dev;
  dwl_init.mem_dev =  params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
  dwl = DWLInit(&dwl_init);

  init_config.codec = DEC_PPDEC;
  init_config.dwl_inst = dwl;
  ret = VCDecInit((const void **)&pp_inst, &init_config);
  if(ret != DEC_OK) {
    DEBUG_PRINT(("[TB] PP INITIALIZATION FAILED\n"));
    goto end;
  }

  DEBUG_PRINT(("\n[TB] PHASE 1: PP INITIALIZATION SUCCESSFUL\n"));

  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl, DWL_CLIENT_TYPE_ST_PP, &core_mask);
  for (u32 i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!dec_cfg.ppu_cfg[i].enabled) continue;
    if (dec_cfg.ppu_cfg[i].comp_enabled) {
      switch (hw_feature->pp_comp_support[i]) {
        case 2: dec400_enabled[i] = 1; break;
        case 3: pp_pvfbc[i] = 1; break;
        default:
          DEBUG_PRINT(("\n[TB]The HW ppu[%u] don't support --pp-comp.\n", i));
      }
    }
  }
  if (in_rfc) {
    dec_cfg.in_lu_stride = luma_in_stride;
    dec_cfg.in_ch_stride = chroma_in_stride;
    dec_cfg.in_lut_stride = (in_width / 8 + 16 - 1) / 16 * 16;
    dec_cfg.in_cht_stride = ((in_width + 16 - 1)  / 16 + 16 - 1) / 16 * 16;
    dec_cfg.in_rfc = in_rfc;
    dec_cfg.in_format = PP_IN_RFC;
    dec_cfg.in_chroma_format_idc = in_chroma_format_idc;
  } else {
    dec_cfg.in_format = in_format;
    if (IS_PP_IN_YUV420(in_format)) {
      in_chroma_format_idc = PP_YUV420;
      dec_cfg.in_chroma_format_idc = PP_YUV420;
    } else if (IS_PP_IN_YUV422(in_format)) {
      in_chroma_format_idc = PP_YUV422;
      dec_cfg.in_chroma_format_idc = PP_YUV422;
    }
  }
  if (in_dec400) {
    dec_cfg.in_dec400 = in_dec400;
    if (in_dec400_a == 32) {
      dec_cfg.in_dec400_a = DEC_ALIGN_32B;
    } else {
      /* default set dec400 align as 64B */
      dec_cfg.in_dec400_a = DEC_ALIGN_64B;
    }
  }
  if (!IS_PP_IN_RGB(dec_cfg.in_format)){
    if (dec_cfg.in_chroma_format_idc == PP_YUV420) {
      sub_x = 2;
      sub_y = 2;
    } else if (dec_cfg.in_chroma_format_idc == PP_YUV422){
      sub_x = 2;
      sub_y = 1;
    } else {
      sub_x = sub_y = 1;
    }
  }
  /*input buffer size*/
  dec_cfg.in_stride = yuv_stride = NEXT_MULTIPLE(in_stride, 16);
  dec_cfg.in_stride_ch = yuv_stride_ch = NEXT_MULTIPLE(in_stride_ch, 16);
  dec_cfg.in_height = in_height;
  dec_cfg.in_width = in_width;

  if (in_rgb) {
    if (IS_PP_IN_RGB_PLANAR(in_format)) {
      buff_size = yuv_stride * in_height * 3;
    } else {
      buff_size = yuv_stride * in_height;
    }
    if (in_tiled_mode == IN_TILED64x64) {
      buff_size = yuv_stride * (NEXT_MULTIPLE(in_height, 64) / 64);
      input_lu_size = buff_size;
      input_ch_size = 0;
    }
  } else if (in_tiled4x4) {
    buff_size = yuv_stride * (NEXT_MULTIPLE(in_height, 4) / 4 + NEXT_MULTIPLE(in_height / 2, 4) / 4);
  } else if (in_rfc) {
    /* luma rfc data + chroma rfc data + luma rfc table + chroma rfc table */
    buff_size = luma_in_stride * in_height / 8 + chroma_in_stride * in_height / sub_y / 4 + dec_cfg.in_lut_stride * in_height / 8
                + dec_cfg.in_cht_stride * in_height / sub_y / 4;
  } else if (in_tiled_mode == IN_TILED8x8) {
    /* TODO(Yixiao):always allocate as 420, need update? */
    buff_size = yuv_stride * (NEXT_MULTIPLE(in_height, 8) / 8 + NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4 / 2);
    input_lu_size = yuv_stride * NEXT_MULTIPLE(in_height, 8) / 8;
    input_ch_size = yuv_stride / 2 * NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4;
  } else if (IS_PP_IN_422PACKED(in_format)) {
    buff_size = yuv_stride * in_height;
  } else if (IS_PP_IN_YUV_PLANAR(in_format)) {
    buff_size = yuv_stride * in_height;
    buff_size += yuv_stride_ch * (sub_y == 2 ? NEXT_MULTIPLE(in_height, 2) : in_height * 2);
  } else {
    //buff_size = yuv_stride * in_height * 3 / 2;
    buff_size = yuv_stride * in_height;
    buff_size += yuv_stride_ch * (sub_y == 2 ?  (in_height + 1) / 2 : in_height);
  }
  /* add ts table size */
  if (in_dec400) {
    dec400_lu_ts_size = NEXT_MULTIPLE(NEXT_MULTIPLE(NEXT_MULTIPLE(input_lu_size, 256) / 256 * 4, 8) / 8, 16) + DEC400_IN_HEADER_SIZE;
    dec400_ch_ts_size = NEXT_MULTIPLE(NEXT_MULTIPLE(NEXT_MULTIPLE(input_ch_size, 256) / 256 * 4, 8) / 8, 16) + DEC400_IN_HEADER_SIZE;
    buff_size = buff_size + NEXT_MULTIPLE(dec400_lu_ts_size, 256) + NEXT_MULTIPLE(dec400_ch_ts_size, 256);
  }
#ifdef SUPPORT_DMA
  pp_in_buffer.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(pp_in_buffer.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if (DWLMallocLinear(dwl, buff_size, &pp_in_buffer) != DWL_OK) {
    DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end;
  } else {
    DEBUG_PRINT(("\n[TB] PHASE 2-1: ALLOCATE INPUT STREAM BUFFER MEMORY\n"));
    DEBUG_PRINT(("[TB] \t-Input: Allocated buffer: virt: %p bus: 0x%llx\n",
            (void *)pp_in_buffer.virtual_address, pp_in_buffer.bus_address));
    DEBUG_PRINT(("[TB] \t-Input: Buffer size: %u\n", buff_size));
  }


  u32 ext_buffer_size = 0;
  u32 luma_size = 0;
  u32 chroma_size = 0;
  u32 dec400_table_size = 0;
  u32 dec400_luma_table_size = 0;
  u32 dec400_chroma_table_size = 0;

  if (align != params.align) {
    align = params.align;
  }
  /*output buffer size*/
  u8 output_chroma_format;
  u8 pp_out_sub_x, pp_out_sub_y;
  u32 pp_width_ch, pp_height_ch;
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!dec_cfg.ppu_cfg[i].enabled) continue;
    output_chroma_format = dec_cfg.ppu_cfg[i].chroma_format;
    if (output_chroma_format == PP_YUV420) {
      pp_out_sub_x = pp_out_sub_y = 2;
    } else if (output_chroma_format == PP_YUV422) {
      pp_out_sub_x = 2;
      pp_out_sub_y = 1;
    } else {
      pp_out_sub_x = pp_out_sub_y = 1;
    }

    pixel_width_pp = (dec_cfg.ppu_cfg[i].out_cut_8bits || pixel_width == 8) ? 8 :
                      ((dec_cfg.ppu_cfg[i].out_p010 || dec_cfg.ppu_cfg[i].out_I010 || dec_cfg.ppu_cfg[i].out_L010 ||
                      (dec_cfg.ppu_cfg[i].tiled_e && pixel_width > 8)) ? 16 : pixel_width);
    if (dec_cfg.ppu_cfg[i].rgb || dec_cfg.ppu_cfg[i].rgb_planar) {
      if (!dec_cfg.ppu_cfg[i].rgb_planar){
        if (IS_PIC_32BIT(dec_cfg.ppu_cfg[i].rgb_format))
          pixel_width_pp = 32;
        else if (IS_PIC_24BIT(dec_cfg.ppu_cfg[i].rgb_format))
          pixel_width_pp = 24;
        else if (IS_PIC_16BIT(dec_cfg.ppu_cfg[i].rgb_format))
          pixel_width_pp = 16;
        else if (IS_PIC_8BIT(dec_cfg.ppu_cfg[i].rgb_format))
          pixel_width_pp = 8;
        else if (IS_PIC_48BIT(dec_cfg.ppu_cfg[i].rgb_format))
          pixel_width_pp = 48;
      } else {
        if (IS_PIC_48BIT(dec_cfg.ppu_cfg[i].rgb_format))
          pixel_width_pp = 16;
        else
          pixel_width_pp = 8;
      }
    }
    luma_size = chroma_size = 0;
    if (dec_cfg.ppu_cfg[i].tiled_e && !dec_cfg.ppu_cfg[i].tile_mode) {
      pp_width = NEXT_MULTIPLE(dec_cfg.ppu_cfg[i].scale.width, 4);
      pp_height = NEXT_MULTIPLE(dec_cfg.ppu_cfg[i].scale.height, 4) / 4;
      pp_stride = NEXT_MULTIPLE(4 * pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
      luma_size = pp_stride * pp_height;
      pp_buff_size = luma_size;
      /* chroma */
      if (!dec_cfg.ppu_cfg[i].monochrome) {
        pp_height = NEXT_MULTIPLE(dec_cfg.ppu_cfg[i].scale.height/2, 4) / 4;
        chroma_size = pp_stride * pp_height;
        pp_buff_size += chroma_size;
      }
    } else if (dec_cfg.ppu_cfg[i].out_1010) {
      pp_width = ((dec_cfg.ppu_cfg[i].scale.width + 2) / 3) * 3;
      pp_stride = NEXT_MULTIPLE(4 * pp_width / 3, ALIGN(align));
      pp_stride_ch = NEXT_MULTIPLE(4 * (pp_out_sub_x == 2 ? 1 : 2) * pp_width / 3, ALIGN(align));
      if (dec400_enabled[i])
        pp_stride_ch = NEXT_MULTIPLE(pp_stride_ch, 256);
      pp_height = dec_cfg.ppu_cfg[i].scale.height;
      pp_height_ch = pp_out_sub_y == 2 ? (dec_cfg.ppu_cfg[i].scale.height + 1) / 2 : dec_cfg.ppu_cfg[i].scale.height;
      luma_size = pp_stride * pp_height;
      pp_buff_size = luma_size;
      if (!dec_cfg.ppu_cfg[i].monochrome) {
        if (dec_cfg.ppu_cfg[i].planar) {
          chroma_size = pp_stride_ch * pp_height_ch * 2;
        } else
          chroma_size = pp_stride_ch * pp_height_ch;
        pp_buff_size += chroma_size;
      }
    } else if (dec_cfg.ppu_cfg[i].tiled_e && dec_cfg.ppu_cfg[i].tile_mode) {
      if (dec_cfg.ppu_cfg[i].tile_mode == TILED64x64) {
        /* TILED64x64 only support ARGB888 or XRGB888 or A2R10G10B10 */
        pp_width = dec_cfg.ppu_cfg[i].scale.width;
        pp_height = dec_cfg.ppu_cfg[i].scale.height;
        pp_stride = NEXT_MULTIPLE(NEXT_MULTIPLE(pp_width, 64) * 64 * 4, ALIGN(align));
        if (dec400_enabled[i])
          pp_stride = NEXT_MULTIPLE(pp_stride, 256);
        luma_size = pp_stride * (NEXT_MULTIPLE(pp_height, 64) / 64);
        pp_buff_size = luma_size;
      } else if (dec_cfg.ppu_cfg[i].tile_mode == TILED8x8) {
        pp_width = dec_cfg.ppu_cfg[i].scale.width;
        pp_height = dec_cfg.ppu_cfg[i].scale.height;
        pp_stride = NEXT_MULTIPLE(8 * pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        if (dec400_enabled[i])
          pp_stride = NEXT_MULTIPLE(pp_stride, 256);
        luma_size = pp_stride * (NEXT_MULTIPLE(pp_height, 8) / 8);
        pp_buff_size = luma_size;
        /* chroma */
        if (!dec_cfg.ppu_cfg[i].monochrome) {
          pp_stride = NEXT_MULTIPLE(4 * pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
          if (dec400_enabled[i])
            pp_stride = NEXT_MULTIPLE(pp_stride, 256);
          chroma_size = pp_stride * NEXT_MULTIPLE((pp_height + 1) / 2, 4) / 4;
          pp_buff_size += chroma_size;
        }
      } else {
        pp_stride = NEXT_MULTIPLE(2 * NEXT_MULTIPLE(dec_cfg.ppu_cfg[i].scale.width, 256) * pixel_width_pp, ALIGN(align) * 8) / 8;
        if (dec400_enabled[i])
          pp_stride = NEXT_MULTIPLE(pp_stride, 256);
        pp_height = dec_cfg.ppu_cfg[i].scale.height;
        luma_size = pp_stride * pp_height;
        pp_buff_size = luma_size;
        /* chroma */
        if (!dec_cfg.ppu_cfg[i].monochrome) {
          pp_height = dec_cfg.ppu_cfg[i].scale.height / 2;
          chroma_size = pp_stride * pp_height;
          pp_buff_size += chroma_size;
        }
      }
    } else {
      pp_width = dec_cfg.ppu_cfg[i].scale.width;
      pp_height = dec_cfg.ppu_cfg[i].scale.height;
      if (dec_cfg.ppu_cfg[i].pad.mode) {
        pp_width += dec_cfg.ppu_cfg[i].pad.l_off + dec_cfg.ppu_cfg[i].pad.r_off;
        pp_height += dec_cfg.ppu_cfg[i].pad.t_off + dec_cfg.ppu_cfg[i].pad.b_off;
      }
      if (dec_cfg.ppu_cfg[i].rgb) {
        pp_stride = NEXT_MULTIPLE(pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        if (dec400_enabled[i])
          pp_stride = NEXT_MULTIPLE(pp_stride, 256);
        luma_size = NEXT_MULTIPLE(pp_stride * pp_height, PLANE_ALIGNMENT);
      } else if (dec_cfg.ppu_cfg[i].rgb_planar) {
        pp_stride = NEXT_MULTIPLE(pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        if (dec400_enabled[i])
          pp_stride = NEXT_MULTIPLE(pp_stride, 256);
        luma_size = 3 * NEXT_MULTIPLE(pp_stride * pp_height, PLANE_ALIGNMENT);
      } else {
        pp_width_ch = pp_out_sub_x == 2 ? (pp_width + 1) / 2 : pp_width;
        pp_height_ch = pp_out_sub_y == 2 ? (pp_height + 1) / 2 : pp_height;
        pp_stride = NEXT_MULTIPLE(pp_width * pixel_width_pp, ALIGN(align) * 8) / 8;
        if (dec400_enabled[i])
          pp_stride = NEXT_MULTIPLE(pp_stride, 256);

        if (dec_cfg.ppu_cfg[i].planar) {
          pp_stride_ch = NEXT_MULTIPLE(pp_width_ch * pixel_width_pp, ALIGN(align) * 8) / 8;
          if (dec400_enabled[i])
            pp_stride_ch = NEXT_MULTIPLE(pp_stride_ch, 256);
        } else {
          pp_stride_ch = NEXT_MULTIPLE(pp_width_ch * pp_out_sub_x * pixel_width_pp, ALIGN(align) * 8) / 8;
          if (dec400_enabled[i])
            pp_stride_ch = NEXT_MULTIPLE(pp_stride_ch, 256);
        }
        luma_size = pp_stride * pp_height;
      }
      pp_buff_size = luma_size;
      if (!dec_cfg.ppu_cfg[i].monochrome && !dec_cfg.ppu_cfg[i].rgb && !dec_cfg.ppu_cfg[i].rgb_planar) {
        if (dec_cfg.ppu_cfg[i].planar){
          chroma_size = pp_stride_ch * pp_height_ch * 2;
        } else {
          chroma_size = pp_stride_ch * pp_height_ch;
        }
        pp_buff_size += chroma_size;
      }
    }

    /*support PVFBC*/
    if (pp_pvfbc[i]) {/*only support YUV420SP*/
      u64 plane0_size, plane1_size;
      plane0_size = CalcOnePpUnitPVFBCPlane0Size(&dec_cfg.ppu_cfg[i]);
      plane1_size = CalcOnePpUnitPVFBCPlane1Size(&dec_cfg.ppu_cfg[i], dec_cfg.ppu_cfg[i].chroma_format == PP_YUV400);
      // #ifdef MODEL_SIMULATION
      // //pp_buff_size = MAX(plane0_size + plane1_size, luma_size + chroma_size);
      // pp_buff_size = MAX(plane0_size, luma_size) + MAX(plane1_size, chroma_size);
      // #else
      pp_buff_size = plane0_size + plane1_size;
      //#endif
    }

    ext_buffer_size += NEXT_MULTIPLE(pp_buff_size, 16);
    if (dec400_enabled[i]) {
      dec400_luma_table_size = NEXT_MULTIPLE(NEXT_MULTIPLE(luma_size / 256, 128) + DEC400_IN_HEADER_SIZE, 256);
      if (chroma_size)
        dec400_chroma_table_size = NEXT_MULTIPLE(NEXT_MULTIPLE(chroma_size / 256, 128) + DEC400_IN_HEADER_SIZE, 256);
      dec400_table_size += dec400_luma_table_size + dec400_chroma_table_size;
      if (dec_cfg.ppu_cfg[i].tile_mode && dec_cfg.ppu_cfg[i].tile_mode == TILED64x64)
        dec400_table_size += NEXT_MULTIPLE(NEXT_MULTIPLE(dec_cfg.ppu_cfg[i].scale.width, 8) / 8, 64) * NEXT_MULTIPLE(dec_cfg.ppu_cfg[i].scale.height, 8) / 8;
    }
  }

  pp_buff_size = ext_buffer_size + dec400_table_size;
#ifdef SUPPORT_DMA
  pp_out_buffer.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
  SET_MEM_USAGE(pp_out_buffer.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
  if(DWLMallocLinear(dwl, pp_buff_size, &pp_out_buffer) != DWL_OK) {
    DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end;
  } else {
    DEBUG_PRINT(("\n[TB] PHASE 3: ALLOCATE OUTPUT STREAM BUFFER MEMORY\n"));
    DEBUG_PRINT(("[TB] \t-Output: Allocated buffer: virt: %p bus: 0x%llx\n",
            (void *)pp_out_buffer.virtual_address, pp_out_buffer.bus_address));
    DEBUG_PRINT(("[TB] \t-Output: Buffer size: %u\n", pp_buff_size));
  }

  /* for color remapping (3dlut) */
  if (enable_3dlut) {
    u32 size = 18 * 18 * 18 * 3 * sizeof(u16);
    // ppu_int_cfg.table_3dlut.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    dec_cfg.table_3dlut_buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                      DWL_MEM_TYPE_CPU;
    if (DWLMallocLinear(dwl, size, &dec_cfg.table_3dlut_buffer)) {
      return -1;
    }
    DWLmemset(dec_cfg.table_3dlut_buffer.virtual_address, 0, size);
    u16 lut3DTableData[18 * 18 * 18][3];
    memset(lut3DTableData, 0, sizeof(lut3DTableData));
    u16 tmpDataNew[18 * 18 * 18][3];
    if (params.table_3dlut_name != NULL) {
      dump3DLutData(lut3DTableData, params.table_3dlut_name);
    } else {
      calculate3DLutTableFor2020To709(lut3DTableData);
    }

    u16 cnt=0;
    for (u16 row_shift1 = 0; row_shift1 < 18 * 18 * 18 / 8; row_shift1 += 18 * 18 / 4) {
      for (u16 col_shift1 = 0; col_shift1 < 8; col_shift1 += 4) { // 4
        for (u16 row_shift2 = 0; row_shift2 < 18 * 18 / 4; row_shift2 += 18 / 2) {
          for (u16 col_shift2 = 0; col_shift2 < 4; col_shift2 += 2) {
            for (u16 row_shift3 = 0; row_shift3 < 18 / 2; row_shift3++) {
              u16 i = 8 * (row_shift1 + row_shift2 + row_shift3) + col_shift1 + col_shift2;
              tmpDataNew[i][0]=lut3DTableData[cnt][0];
              tmpDataNew[i][1]=lut3DTableData[cnt][1];
              tmpDataNew[i][2]=lut3DTableData[cnt][2];
              cnt++;
              tmpDataNew[i+1][0]=lut3DTableData[cnt][0];
              tmpDataNew[i+1][1]=lut3DTableData[cnt][1];
              tmpDataNew[i+1][2]=lut3DTableData[cnt][2];
              cnt++;
            }
          }
        }
      }
    }
    for(u16 i = 0; i < 18 * 18 * 18; i++) {
      lut3DTableData[i][2] = tmpDataNew[i][0]; // R
      lut3DTableData[i][1] = tmpDataNew[i][1]; // G
      lut3DTableData[i][0] = tmpDataNew[i][2]; // B
    }
    memcpy((u32 *)dec_cfg.table_3dlut_buffer.virtual_address, lut3DTableData, 18 * 18 * 18 * 3 * sizeof(u16));
  }

  dec_cfg.align = params.align;
  dec_cfg.in_dec400 = in_dec400;
  dec_cfg.in_dec400_a = in_dec400_a;
  dec_cfg.in_tiled_mode = in_tiled_mode;
  dec_cfg.pp_in_buffer = pp_in_buffer;
  dec_cfg.pp_out_buffer = pp_out_buffer;
  dec_cfg.chroma_format = in_chroma_format_idc;
  dec_cfg.in_chroma_format_idc = in_chroma_format_idc;
  ret = VCDecSetInfo(pp_inst, &dec_cfg);
  if (ret != DEC_OK) {
    DEBUG_PRINT(("[TB] Invalid pp parameters\n"));
    goto end;
  }

  DEBUG_PRINT(("\n[TB] PHASE 4-1: SET PP PARAMETERS INFORMATION SUCCESSFUL\n"));

  /* main decoding loop */
  i = 0;
  do {
    if ((i != 0) && (ctrl_flag)) {
      if (in_rfc) {
          in_width = pic_ctrl[7 * i];
          in_height = pic_ctrl[7 * i + 1];
          dec_cfg.in_luma_bitdepth = pic_ctrl[7 * i + 2];
          dec_cfg.in_chroma_bitdepth = pic_ctrl[7 * i + 3];
          luma_in_stride = dec_cfg.in_lu_stride = pic_ctrl[7 * i + 4];
          chroma_in_stride = dec_cfg.in_ch_stride = pic_ctrl[7 * i + 5];
          dec_cfg.in_lut_stride = (in_width / 8 + 16 - 1) / 16 * 16;
          dec_cfg.in_cht_stride = ((in_width + 16 - 1)  / 16 + 16 - 1) / 16 * 16;
          dec_cfg.in_rfc = in_rfc;
          dec_cfg.in_format = PP_IN_RFC;
          in_chroma_format_idc = pic_ctrl[7 * i + 6];
          dec_cfg.in_chroma_format_idc = in_chroma_format_idc;
      } else {
        in_rgb = IS_PP_IN_RGB(pic_ctrl[5 * i]);
        in_format = pic_ctrl[5 * i];
        in_p010 = !IS_PP_IN_8BIT(pic_ctrl[5 * i]);
        in_tiled4x4 = IS_PP_IN_TILED4x4(pic_ctrl[5 * i]);
        in_width = pic_ctrl[5 * i + 1];
        in_height = pic_ctrl[5 * i + 3];
        in_stride = pic_ctrl[5 * i + 2];
        in_stride_ch = pic_ctrl[5 * i + 4];
        yuv_stride = NEXT_MULTIPLE(in_stride, 16);
        yuv_stride_ch = NEXT_MULTIPLE(in_stride_ch, 16);
        if (!in_rgb) {
          if (IS_PP_IN_YUV420(in_format)){
            in_chroma_format_idc = PP_YUV420;
            dec_cfg.in_chroma_format_idc = PP_YUV420;
          } else if (IS_PP_IN_YUV422(in_format)) {
            in_chroma_format_idc = PP_YUV422;
            dec_cfg.in_chroma_format_idc = PP_YUV422;
          }
        }
  #ifdef ASIC_TRACE_SUPPORT
        tb_cfg.pp_params.in_width = in_width;
        tb_cfg.pp_params.in_height = in_height;
  #endif
      }

      /*recalculate input buffer size*/
      if (!IS_PP_IN_RGB(dec_cfg.in_format)){
        if (dec_cfg.in_chroma_format_idc == PP_YUV420) {
          sub_x = 2;
          sub_y = 2;
        } else if (dec_cfg.in_chroma_format_idc == PP_YUV422){
          sub_x = 2;
          sub_y = 1;
        } else {
          sub_x = sub_y = 1;
        }
      }

      if (in_rgb) {
        if (IS_PP_IN_RGB_PLANAR(in_format)) {
          new_buff_size = yuv_stride * in_height * 3;
        } else {
          new_buff_size = yuv_stride * in_height;
        }
        if (in_tiled_mode == IN_TILED64x64) {
         new_buff_size = yuv_stride * (NEXT_MULTIPLE(in_height, 64) / 64);
         input_lu_size = new_buff_size;
         input_ch_size = 0;
       }
      } else if (in_tiled4x4) {
        new_buff_size = yuv_stride * (NEXT_MULTIPLE(in_height, 4) / 4 + NEXT_MULTIPLE(in_height / 2, 4) / 4);
      } else if (in_tiled_mode == IN_TILED8x8) {
        /* TODO(Yixiao):always allocate as 420, need update? */
        new_buff_size = yuv_stride * (NEXT_MULTIPLE(in_height, 8) / 8 + NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4 / 2);
        input_lu_size = yuv_stride * NEXT_MULTIPLE(in_height, 8) / 8;
        input_ch_size = yuv_stride / 2 * NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4;
      } else if (in_rfc) {
        /* luma rfc data + chroma rfc data + luma rfc table + chroma rfc table */
        new_buff_size = luma_in_stride * in_height / 8 + chroma_in_stride * in_height / sub_y / 4 + dec_cfg.in_lut_stride * in_height / 8
                + dec_cfg.in_cht_stride * in_height / sub_y / 4;
      } else if (IS_PP_IN_422PACKED(in_format)) {
        new_buff_size = yuv_stride * in_height;
      } else if (IS_PP_IN_YUV_PLANAR(in_format)) {
        new_buff_size = yuv_stride * in_height;
        new_buff_size += yuv_stride_ch * (sub_y == 2 ? NEXT_MULTIPLE(in_height, 2) : in_height * 2);
      } else {
        //new_buff_size = yuv_stride * in_height * 3 / 2;
        new_buff_size = yuv_stride * in_height;
        new_buff_size += yuv_stride_ch * (sub_y == 2 ?  (in_height + 1) / 2 : in_height);
      }
      /* add ts table size */
      if (in_dec400) {
        dec400_lu_ts_size = NEXT_MULTIPLE(NEXT_MULTIPLE(input_lu_size, 256) / 256 * 4, 8) / 8 + DEC400_IN_HEADER_SIZE;
        dec400_ch_ts_size = NEXT_MULTIPLE(NEXT_MULTIPLE(input_ch_size, 256) / 256 * 4, 8) / 8 + DEC400_IN_HEADER_SIZE;
        buff_size = buff_size + NEXT_MULTIPLE(dec400_lu_ts_size, 256) + NEXT_MULTIPLE(dec400_ch_ts_size, 256);
      }

      /*reset input buffer size*/
      if (new_buff_size != buff_size) {
        buff_size = new_buff_size;
        DWLFreeLinear(dwl, &pp_in_buffer);
        SET_MEM_USAGE(pp_in_buffer.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
        if(DWLMallocLinear(dwl, buff_size, &pp_in_buffer) != DWL_OK) {
          DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
          goto end;
        } else {
         DEBUG_PRINT(("\n[TB] PHASE 2-2: ALLOCATE NEW INPUT STREAM BUFFER MEMORY\n"));
         DEBUG_PRINT(("[TB] \t-Input: Allocated buffer: virt: %p bus: 0x%llx\n",
            (void *)pp_in_buffer.virtual_address, pp_in_buffer.bus_address));
         DEBUG_PRINT(("[TB] \tBuffer new size: %u\n", buff_size));
        }
        dec_cfg.in_format = in_format;
        dec_cfg.in_stride = yuv_stride;
        dec_cfg.in_stride_ch = yuv_stride_ch;
        dec_cfg.in_height = in_height;
        dec_cfg.in_width = in_width;
        dec_cfg.pp_in_buffer = pp_in_buffer;
        ret = VCDecSetInfo(pp_inst, &dec_cfg);
        if (ret != DEC_OK) {
          DEBUG_PRINT(("[TB] Invalid pp parameters\n"));
          goto end;
        }
        DEBUG_PRINT(("\n[TB] PHASE 4-2: RESET PP PARAMETERS INFORMATION SUCCESSFUL\n"));
      }
    }
    /* Picture ID is the picture number in decoding order */
    decode_pic_num++;

    if (!IS_PP_IN_RGB(dec_cfg.in_format)){
      if (dec_cfg.in_chroma_format_idc == PP_YUV420) {
        sub_x = 2;
        sub_y = 2;
      } else if (dec_cfg.in_chroma_format_idc == PP_YUV422){
        sub_x = 2;
        sub_y = 1;
      } else {
        sub_x = sub_y = 1;
      }
    }

    if (in_dec400 && dec400_lu_ts_size) {
#ifndef SUPPORT_DEC400_HEADER
        if (finput_table)
          ra = fread(((u8*)(pp_in_buffer.virtual_address) + DEC400_IN_HEADER_SIZE), 1, dec400_lu_ts_size - DEC400_IN_HEADER_SIZE , finput_table);
        else
          ra = fread(((u8*)(pp_in_buffer.virtual_address) + DEC400_IN_HEADER_SIZE), 1, dec400_lu_ts_size - DEC400_IN_HEADER_SIZE , finput_luma_table);
#else
        if (finput_table)
          ra = fread((u8*)(gc_in_buffer.virtual_address), 1, dec400_lu_ts_size, finput_table);
        else
          ra = fread((u8*)(gc_in_buffer.virtual_address), 1, dec400_lu_ts_size, finput_luma_table);
#endif
      input_va = (u8*)(pp_in_buffer.virtual_address) + NEXT_MULTIPLE(dec400_lu_ts_size, 256);
    } else {
      input_va = (u8*)(pp_in_buffer.virtual_address);
    }

    if (in_rgb) {
      if (IS_PP_IN_RGB_PLANAR(in_format)) {
        for (m = 0; m < in_height * 3; m++)
          ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
      } else if(in_tiled_mode == IN_TILED64x64) {
        for (m = 0; m < NEXT_MULTIPLE(in_height, 64) / 64; m++)
          ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
      } else {
        for (m = 0; m < in_height; m++)
          ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
      }
    } else if (in_tiled4x4) {
      for (m = 0; m < (NEXT_MULTIPLE(in_height, 4) / 4 + NEXT_MULTIPLE(in_height / 2, 4) / 4); m++)
        ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
    } else if (in_tiled_mode == IN_TILED8x8) {
      /* for luma */
      for (m = 0; m < (NEXT_MULTIPLE(in_height, 8) / 8); m++)
        ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);

      if (in_chroma_format_idc > 0) {
      /* for chroma */
        if(in_dec400 && dec400_ch_ts_size) {
#ifndef SUPPORT_DEC400_HEADER
            if (finput_table)
              ra = fread(((u8*)(input_va) + yuv_stride * m + DEC400_IN_HEADER_SIZE), 1, dec400_ch_ts_size - DEC400_IN_HEADER_SIZE , finput_table);
            else
              ra = fread(((u8*)(input_va) + yuv_stride * m + DEC400_IN_HEADER_SIZE), 1, dec400_ch_ts_size - DEC400_IN_HEADER_SIZE , finput_chroma_table);
#else
            if (finput_table)
              ra = fread(((u8*)(input_va) + yuv_stride * m ), 1, dec400_ch_ts_size , finput_table);
            else
              ra = fread(((u8*)(input_va) + yuv_stride * m ), 1, dec400_ch_ts_size , finput_chroma_table);
#endif
          input_va = (u8*)(input_va) + yuv_stride * m + NEXT_MULTIPLE(dec400_ch_ts_size, 256);
        }
        for (m = 0; m < (NEXT_MULTIPLE((in_height + 1) / 2, 4) / 4); m++)
          ra = fread(((u8*)(input_va) + yuv_stride_ch * m), 1, in_stride_ch, finput);
      }
    } else if (in_rfc) {
      /* luma rfc data */
      for (m = 0; m < in_height / 8; m++)
        ra = fread(((u8*)(input_va) + dec_cfg.in_lu_stride * m), 1, dec_cfg.in_lu_stride, finput_luma);

      /* chroma rfc data */
      for (m = 0; m < in_height / sub_y / 4; m++)
        ra = fread(((u8*)(input_va) + dec_cfg.in_lu_stride * in_height / 8 + dec_cfg.in_ch_stride * m), 1, dec_cfg.in_ch_stride, finput_chroma);

      if (in_table) {
        ra = fread(((u8*)(input_va) + dec_cfg.in_lu_stride * in_height / 8 + dec_cfg.in_ch_stride * in_height / sub_y / 4), 1,
            dec_cfg.in_lut_stride * in_height / 8 + dec_cfg.in_cht_stride * in_height / sub_y / 4, finput_table);
      } else {
        /* luma rfc table */
        ra = fread(((u8*)(input_va) + dec_cfg.in_lu_stride * in_height / 8 + dec_cfg.in_ch_stride * in_height / sub_y / 4), 1,
            dec_cfg.in_lut_stride * in_height / 8, finput_luma_table);
        /* chroma rfc table */
        ra = fread(((u8*)(input_va) + dec_cfg.in_lu_stride * in_height / 8 + dec_cfg.in_ch_stride * in_height / sub_y / 4 +
                          dec_cfg.in_lut_stride * in_height / 8), 1, dec_cfg.in_cht_stride * in_height / sub_y / 4, finput_chroma_table);
      }
    } else if (IS_PP_IN_422PACKED(in_format)) {
      for (m = 0; m < in_height; m++)
        ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
    } else if (!IS_PP_IN_YUV_PLANAR(dec_cfg.in_format)) {/*support 420SP, 422SP, 444SP*/
      #if 0
      for (m = 0; m < (sub_y == 2 ? in_height * 3 / 2 : in_height * 2); m++)
        ra = fread(((u8*)(gc_in_buffer.virtual_address) + yuv_stride * m), 1, in_stride, finput);
      #else
      for (m = 0; m < in_height; m++) {
        ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
      }
      for (m = 0; m < (sub_y == 2 ? (in_height + 1) / 2 : in_height); m++){
        ra = fread(((u8*)(input_va) + yuv_stride * in_height + yuv_stride_ch * m), 1, in_stride_ch, finput);
      }
      #endif
    } else {
      /*last: SW converts planar format from input YUV file to NV12 format as HW input.
        now:  suppor YUV planar input*/
      for (m = 0; m < in_height; m++)/*support 420P, 422P, 444P*/
        ra = fread(((u8*)(input_va) + yuv_stride * m), 1, in_stride, finput);
      for (m = 0; m < (sub_y == 2 ? NEXT_MULTIPLE(in_height, 2) : in_height * 2); m++) {
        ra = fread(((u8*)(input_va) + yuv_stride * in_height + yuv_stride_ch * m), 1, in_stride_ch, finput);
      }
    }

    (void) ra;
    if (in_I010) {
      u16* base = (u16*)pp_in_buffer.virtual_address;
      for (m = 0; m < in_height * 3 / 2; m++) {
        for (n = 0; n < in_width; n++) {
          *(base + n) = *(base + n) << 6;
        }
        base += yuv_stride / 2;
      }
    }
    /*support odd crop*/
    params.crop_align = hw_feature->crop_step_rshift;
    /* Create output sink after output file names are determined. */
    if (yuvsink == NULL) {
      outfile_info.bitstream_format = BITSTREAM_PP_INPUT;
      outfile_info.pic_width = in_width;
      outfile_info.pic_height = in_height;
      outfile_info.bit_depth = pixel_width == 8 ? 8 : 10;
      outfile_info.is_interlaced = FALSE;
      outfile_info.is_thumbnail = FALSE;
      params.compress_bypass = TRUE; /* PPDEC don't support rfc */
      GenerateOutputFileName(&params, &outfile_info);
      if ((yuvsink = CreateYuvSink(&params)) == NULL) {
        fprintf(stderr, "[TB] Failed to create YUV sink\n");
        goto end;
      } else{
        DEBUG_PRINT(("\n[TB] PHASE 5: CREATE OUTPUT FILE SUCCESSFUL\n"));
      }
    }

    /* call API function to perform decoding */
    ret = VCDecDecode(pp_inst, NULL, NULL);
    DEBUG_PRINT(("[TB] PIC %u\n", decode_pic_num));
    ret = VCDecNextPicture(pp_inst, &dec_picture);
    u32 only_once = 0;
    u32* host_base = NULL;
    struct DecPicture* in = NULL;
    for (j = 0; j < DEC_MAX_OUT_COUNT; j++) {
      in = &dec_picture.pictures[j];
      if (!in->pic_width || !in->pic_height)
        continue;

      if (only_once == 0) {
        if (pp_out_buffer.virtual_address == NULL) {
          host_base = DWLmalloc(pp_out_buffer.size);
          pp_out_buffer.virtual_address = host_base;
        }
        DWLDMATransData(dwl, &pp_out_buffer, 0, pp_out_buffer.size, DEVICE_TO_HOST);
        only_once = 1;
      }
      SwSetHostOutbaseAfterDma(host_base, in, &pp_out_buffer);
      /* Write output picture to file */
      yuvsink->WritePicture(yuvsink->inst, in, j);
      SwClearHostOutbaseAfterWriteFile(host_base, in);
    }
    if (host_base != NULL) {
      DWLfree(host_base);
      pp_out_buffer.virtual_address = NULL;
    }
    if (decode_pic_num == max_num_pics)
      decode_pic_num = pic_num;
    i++;
  } while(decode_pic_num < pic_num);

  DEBUG_PRINT(("\n[TB] PHASE 6: DECODE SUCCESSFUL\n"));
  DEBUG_PRINT(("\n[TB] INPUT FILE INFORMATION: \n"));
  DEBUG_PRINT(("[TB] \t-bitstream_format: BITSTREAM_PP_INPUT\n"));
  DEBUG_PRINT(("[TB] \t-pic_width: %u\n", outfile_info.pic_width));
  DEBUG_PRINT(("[TB] \t-pic_height: %u\n", outfile_info.pic_height));
  DEBUG_PRINT(("[TB] \t-bit_depth: %u\n", outfile_info.bit_depth));
  DEBUG_PRINT(("[TB] \t-is_interlaced: %d\n", outfile_info.is_interlaced));
  DEBUG_PRINT(("[TB] \t-params.compress_bypass: %u\n",params.compress_bypass));

end:
#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  if (enable_3dlut) {
    if (dec_cfg.table_3dlut_buffer.virtual_address) {
      DWLFreeLinear(dwl, &dec_cfg.table_3dlut_buffer);
      dec_cfg.table_3dlut_buffer.virtual_address = NULL;
    }
  }

  /* release decoder instance */
  if (pp_in_buffer.bus_address)
    DWLFreeLinear(dwl, &pp_in_buffer);
  if (pp_out_buffer.bus_address)
    DWLFreeLinear(dwl, &pp_out_buffer);
  if (pp_inst)
    VCDecRelease(pp_inst);
  if (dwl)
    DWLRelease(dwl);
  VCDecLogDestory();
  if (pic_ctrl)
    free(pic_ctrl);

  DEBUG_PRINT(("[TB] PHASE 7: RELEASE SUCCESSFUL\n"));

  PrintOutputFileName(&params);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  if (finput_luma) fclose(finput_luma);
  if (finput_luma_table) fclose(finput_luma_table);
  if (finput_chroma) fclose(finput_chroma);
  if (finput_chroma_table) fclose(finput_chroma_table);
  if (finput) fclose(finput);
  if (osd_file) fclose (osd_file);

  return 0;
}
