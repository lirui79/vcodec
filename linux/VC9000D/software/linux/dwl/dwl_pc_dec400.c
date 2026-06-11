/*------------------------------------------------------------------------------

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
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "dwl_swhw_sync.h"
#include "sw_performance.h"
#include "dec_log.h"
#include "regdrv.h"
#include "vpufeature.h"
#include "tb_cfg.h"
#include "dwl_memsync.h"
#ifdef VIRTUAL_PLATFORM_TEST
#include "buffer_info.h"
#endif

#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#include "dwl_vcmd_common.h"
#include "vwl_pc.h"

#ifdef FPGA_PERF_AND_BW
#include "dwl_perf_info.h"
#endif

#define DEC_MODE_H264 0
#define DEC_MODE_MPEG4 1
#define DEC_MODE_H263 2
#define DEC_MODE_JPEG 3
#define DEC_MODE_VC1 4
#define DEC_MODE_MPEG2 5
#define DEC_MODE_MPEG1 6
#define DEC_MODE_VP6 7
#define DEC_MODE_RV 8
#define DEC_MODE_VP7 9
#define DEC_MODE_VP8 10
#define DEC_MODE_AVS 11
#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9 13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2 16

#define AHBDEC_CONTROL  0
#define AHBDEC_CONTROLEX  1
#define AHBDEC_CONTROLEX2  2
#define AHBDEC_INTR_ENBL  3
#define AHBDEC_INTR_ENBLEX  4
#define AHBDEC_INTR_ENBLEX2  5
#define AHBDEC_INTR_ACKNOWLEDGE  6
#define AHBDEC_INTR_ACKNOWLEDGEEX  7
#define AHBDEC_INTR_ACKNOWLEDGEEX2  8
#define AHBDEC_TILE_STATUS_DEBUG  9
#define AHBDEC_ENCODER_DEBUG  10
#define AHBDEC_DECODER_DEBUG  11
#define AHBDEC_TOTAL_READSIN  12
#define AHBDEC_TOTAL_WRITESIN  13
#define AHBDEC_TOTAL_READ_BURSTS_IN  14
#define AHBDEC_TOTAL_WRITE_BURSTS_IN  15
#define AHBDEC_TOTAL_READS_REQ_IN  16
#define AHBDEC_TOTAL_WRITES_REQ_IN  17
#define AHBDEC_TOTAL_READ_LASTS_IN  18
#define AHBDEC_TOTAL_WRITE_LASTS_IN  19
#define AHBDEC_TOTAL_WRITE_RESPONSE_IN  20
#define AHBDEC_TOTAL_READS_OUT  21
#define AHBDEC_TOTAL_WRITES_OUT  22
#define AHBDEC_TOTAL_READ_BURSTS_OUT  23
#define AHBDEC_TOTAL_WRITE_BURSTS_OUT  24
#define AHBDEC_TOTAL_READS_REQ_OUT  25
#define AHBDEC_TOTAL_WRITES_REQ_OUT  26
#define AHBDEC_TOTAL_READ_LASTS_OUT  27
#define AHBDEC_TOTAL_WRITE_LASTS_OUT  28
#define AHBDEC_TOTAL_WRITE_RESPONSE_OUT  29
#define AHBDEC_STATUS  30
#define AHBDEC_WRITE_CONFIG  31
#define AHBDEC_WRITE_EXCONFIG  32
#define AHBDEC_WRITE_BUFFER_BASE  33
#define AHBDEC_WRITE_BUFFER_BASEEX  34
#define AHBDEC_WRITE_BUFFER_END  35
#define AHBDEC_WRITE_BUFFER_ENDEX  36
#define AHBDEC_WRITE_CACHE_BASE  37
#define AHBDEC_WRITE_CACHE_BASEEX  38
#define AHBDEBUG_INFO_OUT  39
#define AHBDEC_DEBUG0  40
#define AHBDEC_DEBUG1  41
#define AHBDEC_DEBUG2  42
#define AHBDEC_DEBUG3  43
#define AHBDEC_DEBUG4  44
#define AHBDEC_DEBUG5  45
//count compression register
#define AHBDEC_CRDEBUG_TILE128_TYPE0  46
#define AHBDEC_CRDEBUG_TILE128_TYPE2  47
#define AHBDEC_CRDEBUG_TILE256_TYPE0  48
#define AHBDEC_CRDEBUG_TILE256_TYPE2  49
#define AHBDEC_CRDEBUG_TILE256_TYPE4  50
#define AHBDEC_CRDEBUG_TILE256_TYPE6  51

const u32 dec400_reg_offset[][52] = {
  {0x800,  //gcregAHBDECControl  0
  0x804,  //gcregAHBDECControlEx  1
  0x808,  //gcregAHBDECControlEx2  2
  0x80C,  //gcregAHBDECIntrEnbl  3
  0x810,  //gcregAHBDECIntrEnblEx  4
  0x814,  //gcregAHBDECIntrEnblEx2  5
  0x818,  //gcregAHBDECIntrAcknowledge  6
  0x81C,  //gcregAHBDECIntrAcknowledgeEx  7
  0x820,  //gcregAHBDECIntrAcknowledgeEx2  8
  0x824,  //gcregAHBDECTileStatusDebug  9
  0x828,  //gcregAHBDECEncoderDebug  10
  0x82C,  //gcregAHBDECDecoderDebug  11
  0x830,  //gcregAHBDECTotalReadsIn  12
  0x834,  //gcregAHBDECTotalWritesIn  13
  0x838,  //gcregAHBDECTotalReadBurstsIn  14
  0x83C,  //gcregAHBDECTotalWriteBurstsIn  15
  0x840,  //gcregAHBDECTotalReadsReqIn  16
  0x844,  //gcregAHBDECTotalWritesReqIn  17
  0x848,  //gcregAHBDECTotalReadLastsIn  18
  0x84C,  //gcregAHBDECTotalWriteLastsIn  19
  0x850,  //gcregAHBDECTotalWriteResponseIn  20
  0x854,  //gcregAHBDECTotalReadsOut  21
  0x858,  //gcregAHBDECTotalWritesOut  22
  0x85C,  //gcregAHBDECTotalReadBurstsOut  23
  0x860,  //gcregAHBDECTotalWriteBurstsOut  24
  0x864,  //gcregAHBDECTotalReadsReqOut  25
  0x868,  //gcregAHBDECTotalWritesReqOut  26
  0x86C,  //gcregAHBDECTotalReadLastsOut  27
  0x870,  //gcregAHBDECTotalWriteLastsOut  28
  0x874,  //gcregAHBDECTotalWriteResponseOut  29
  0x878,  //gcregAHBDECStatus  30
  0x980,  //gcregAHBDECWriteConfig  31
  0xA00,  //gcregAHBDECWriteExConfig  32
  0xD80,  //gcregAHBDECWriteBufferBase  33
  0xE00,  //gcregAHBDECWriteBufferBaseEx  34
  0xE80,  //gcregAHBDECWriteBufferEnd  35
  0xF00,  //gcregAHBDECWriteBufferEndEx  36
  0x1180,  //gcregAHBDECWriteCacheBase  37
  0x1200,  //gcregAHBDECWriteCacheBaseEx  38
  0x1280,  //gcregAHBDebugInfoOut  39
  0x1284,  //gcregAHBDECDebug0  40
  0x1288,  //gcregAHBDECDebug1  41
  0x128C,  //gcregAHBDECDebug2  42
  0x1290,  //gcregAHBDECDebug3  43
  0x1294,  //gcregAHBDECDebug4  44
  0x1298,  //gcregAHBDECDebug5  45
  //count compression register
  0x12A8,  //gcregAHBDECCrDebugTile128Type0  46
  0x12B0,  //gcregAHBDECCrDebugTile128Type2  47
  0x12B8,  //gcregAHBDECCrDebugTile256Type0  48
  0x12C0,  //gcregAHBDECCrDebugTile256Type2  49
  0x12C8,  //gcregAHBDECCrDebugTile256Type4  50
  0x12D0}  //gcregAHBDECCrDebugTile256Type6  51
};

void DWLDec400WriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value)
{
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  HwCoreDec400SetRegister(dwl_inst->current_core, offset, value);
}

u32 DWLDec400ReadRegFromHw(const void *instance, i32 core_id, u32 offset)
{
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  return HwCoreDec400GetRegister(dwl_inst->current_core, offset);
}

void DWLDecF1Configure(const void *instance, i32 core_id) {
  /*
   * when input stream is  yuv/Rgb, Resize pic(PP output) and origin pic should enable dec compression.
   * when input stream is hevc/vp9, Resize pic(PP output) should enable dec compression.
   * when input stream is h264,  ref pic and resize pic(PP output) should enable dec compression.
  */
  int i = 0;
  u64 y_addr = 0;
  u64 uv_addr = 0;
  unsigned int y_size = 0;
  unsigned int uv_size = 0;
  unsigned int mode = 0;
  unsigned int mono_chroma = 0;
  unsigned int w_offset = 0;
  unsigned int pic_interlace=0;
  unsigned int frame_mbs_only_flag = 0;
  u32 reg_control[3]={0, 0, 0};
  u32 reg_config[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_config_ex[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base_ex[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base_end[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base_end_ex[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  HwCoreGetDec400Core(dec_dwl->current_core);
  u32 dec400_index = 0;

  mode = (dec_dwl->dwl_shadow_regs[core_id][3]>>27)&0x1F;
  DTRACE_D("start the dec f1 cfg ,mode = %d!!\n", mode);

  reg_control[0] = (0x20 << 18) | //[23:18]  sw_flush_id
                   (0x1 << 16); //[16]  disable_hw_flush
  reg_control[1] = (0x1 << 19) | //[19]
                   (0x1 << 17); //[17]
  reg_control[2] = 0x003FD021;

  /* new project use 64 alignment from 20210305 */
  reg_control[0] |= (0x1 << 8); //[8]  align mode : 64


  if (mode != DEC_MODE_VC1 && mode != DEC_MODE_VP8)
    mono_chroma = (dec_dwl->dwl_shadow_regs[core_id][7]>>30) & 0x01;
  if (mode != DEC_MODE_JPEG)
    pic_interlace = (dec_dwl->dwl_shadow_regs[core_id][3]>>23) & 0x1;
  if(mode == DEC_MODE_H264 || mode == DEC_MODE_H264_H10P)
    frame_mbs_only_flag = (dec_dwl->dwl_shadow_regs[core_id][5]) & 0x01;

  if((pic_interlace == 1) || (frame_mbs_only_flag == 1)) {
    DTRACE_D("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
    reg_control[0] = 0x00810002;
    DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROL], reg_control[0]);
    return;
  }

  /* bypass dec400 if all PPs' dec400 disabled */
  if (!dec_dwl->ppu_cfg[core_id][0].dec400_enabled &&
      !dec_dwl->ppu_cfg[core_id][1].dec400_enabled &&
      !dec_dwl->ppu_cfg[core_id][2].dec400_enabled &&
      !dec_dwl->ppu_cfg[core_id][3].dec400_enabled &&
      !dec_dwl->ppu_cfg[core_id][4].dec400_enabled &&
      !dec_dwl->ppu_cfg[core_id][5].dec400_enabled) {
    reg_control[0] = 0x00810002;
    DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROL], reg_control[0]);
    return;
  }

  /*confiure the gloabal control registers, enable write tile status(table)*/
  DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROL], reg_control[0]);
  DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROLEX], reg_control[1]);//case806 setting
  DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROLEX2], reg_control[2]);

  reg_control[0] = 0xFFFFFFFF; //[31:0]  intr_enbl_vec
  /*this is a interrupt enable register. Each bit enables a corresponding event. the value enable AXI bus error interrupt*/
  DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBL], reg_control[0]);
  DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBLEX2], reg_control[0]);

  u32 pp_y_addr_reg = 386; /* HWIF_PPX_OUT_LU_BASE_U_LSB */
  u32 pp_c_addr_reg = 388; /* HWIF_PPX_OUT_CH_BASE_U_LSB */
  u32 pp_total_buffer_size=0;
  u8 pp_enable_channel=0;
  u32 reg_addr_y=0;
  u32 reg_addr_c=0;
  addr_t pp_bus_address_start=0;
  addr_t luma_table_offset = 0;
  addr_t chroma_table_offset = 0;
  u32 dec400_header_size = DEC400_IN_HEADER_SIZE;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dec_dwl->ppu_cfg[core_id][i].enabled || !dec_dwl->ppu_cfg[core_id][i].dec400_enabled)
      continue;

    if (dec_dwl->ppu_cfg[core_id][i].enabled == 1) {
      reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
      reg_addr_c = pp_c_addr_reg + i * PPU_REG_RANGE;
      y_addr = (dec_dwl->dwl_shadow_regs[core_id][reg_addr_y] | ((u64)dec_dwl->dwl_shadow_regs[core_id][reg_addr_y - 1] << 32));
      uv_addr = (dec_dwl->dwl_shadow_regs[core_id][reg_addr_c] | ((u64)dec_dwl->dwl_shadow_regs[core_id][reg_addr_c - 1] << 32));

      if (dec_dwl->ppu_cfg[core_id][i].tiled_e == 1) {
        if (dec_dwl->ppu_cfg[core_id][i].rgb) {
          y_size = dec_dwl->ppu_cfg[core_id][i].ystride * NEXT_MULTIPLE(dec_dwl->ppu_cfg[core_id][i].scale.height, 64)/64;
          uv_size = 0;
          if (dec_dwl->ppu_cfg[core_id][i].tile_mode == TILED64x64) {
            if (dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_ABGR888 ||
                dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_ARGB888) {
              /* (30:25)| tile 8x8 mode (0) */
              reg_config[w_offset / 4] = 0x30001;
              /* 8bit input */
              reg_config_ex[w_offset / 4] = 0<<16;
            } else if (dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_XBGR888 ||
                       dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_XRGB888) {
              /* (30:25)| tile 8x8 mode (0) */
              /* (7:3)  | XRGB8 (1) */
              reg_config[w_offset / 4] = 0x30009;
              /* 8bit input */
              reg_config_ex[w_offset / 4] = 0<<16;
            } else if (dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2B10G10R10 ||
                       dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2R10G10B10) {
              reg_config[w_offset / 4] = 0x30079;
              reg_config_ex[w_offset / 4] = 0<<16;
            } else {
              continue;
            }
          } else {
            continue;
          }
        } else {
          y_size = dec_dwl->ppu_cfg[core_id][i].ystride * NEXT_MULTIPLE(dec_dwl->ppu_cfg[core_id][i].scale.height,4)/4;
          uv_size = dec_dwl->ppu_cfg[core_id][i].cstride * NEXT_MULTIPLE((dec_dwl->ppu_cfg[core_id][i].scale.height/2),4)/4;

          if (dec_dwl->ppu_cfg[core_id][i].pixel_width == 8) {
            /*(30:25)|   how many pixels in the tile and the walking direction in the tile, 7 means TILE64X4
              (17:16)|   Compression result size alignment mode Only support Align32Byte in SUNPLUS
              (7:3)  |   Compression color format,5 indicates Y, 6 indicates UV
              (0:0)  |   Compression enable */
            reg_config[w_offset / 4] = 0x0E030029;
            /*8bit input*/
            reg_config_ex[w_offset / 4] = 0<<16;
            if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
              //(30:25)|   8 means TILE32X4(chroma*2 == luma)
              reg_config[w_offset / 4 + 1] = 0x10030031;
              reg_config_ex[w_offset / 4 + 1] =  0<<16;
            }
          } else {
            //(30:25)|   8 means TILE32X4(32*4*2byte/pixel=256 align)
            reg_config[w_offset / 4] = 0x10030029;
            /* (18:16)|   1 means BIT10*/
            reg_config_ex[w_offset / 4] = 1<<16;
            if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
              reg_config[w_offset / 4 + 1] = 0x04030031;
              reg_config_ex[w_offset / 4 + 1] = 1<<16;
            }
          }
        }
      } else if (dec_dwl->ppu_cfg[core_id][i].rgb) {
        y_size = dec_dwl->ppu_cfg[core_id][i].ystride * dec_dwl->ppu_cfg[core_id][i].scale.height;
        uv_size = 0;
        if (dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_ABGR888 ||
            dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_ARGB888) {
          // (30:25)| F means RASTER64x1
          reg_config[w_offset / 4] = 0x1E030001;
          /*8bit input*/
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_XBGR888 ||
                   dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_XRGB888) {
          /* (30:25)| tile 8x8 mode (0) */
          /* (7:3)  | XRGB8 (1) */
          reg_config[w_offset / 4] = 0x1E030009;
          /* 8bit input */
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2B10G10R10 ||
                   dec_dwl->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2R10G10B10) {
          // (30:25)| F means RASTER64x1
          reg_config[w_offset / 4] = 0x1E030079;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else {
          continue;
        }
      } else if (dec_dwl->ppu_cfg[core_id][i].planar) {
        y_size = dec_dwl->ppu_cfg[core_id][i].ystride * dec_dwl->ppu_cfg[core_id][i].scale.height;
        uv_size = dec_dwl->ppu_cfg[core_id][i].cstride * dec_dwl->ppu_cfg[core_id][i].scale.height;

        if(dec_dwl->ppu_cfg[core_id][i].pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12030029;
          /*8bit input*/
          reg_config_ex[w_offset / 4] = 0<<16;

          if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x12030029;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14030029;
          /*10bit input*/
          reg_config_ex[w_offset / 4] = 1<<16;

          if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14030029;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if (dec_dwl->ppu_cfg[core_id][i].chroma_format < PP_CHROMA_422) {
        y_size = dec_dwl->ppu_cfg[core_id][i].ystride * dec_dwl->ppu_cfg[core_id][i].scale.height;
        uv_size = dec_dwl->ppu_cfg[core_id][i].cstride * dec_dwl->ppu_cfg[core_id][i].scale.height/2;

        if(dec_dwl->ppu_cfg[core_id][i].pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12030029;
          /*8bit input*/
          reg_config_ex[w_offset / 4] = 0<<16;

          if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14030031;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14030029;
          /*10bit input*/
          reg_config_ex[w_offset / 4] = 1<<16;

          if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x1E030031;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if (dec_dwl->ppu_cfg[core_id][i].chroma_format == PP_CHROMA_422) {
        y_size = dec_dwl->ppu_cfg[core_id][i].ystride * dec_dwl->ppu_cfg[core_id][i].scale.height;
        uv_size = 0;

        if(dec_dwl->ppu_cfg[core_id][i].out_uyvy) {
          reg_config[w_offset / 4] = 0x14030019;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (dec_dwl->ppu_cfg[core_id][i].out_yuyv) {
          reg_config[w_offset / 4] = 0x14030021;
          reg_config_ex[w_offset / 4] = 0<<16;
        }
      }

      /*configure the LSB of luma start address*/
      reg_base[w_offset / 4] = (y_addr & 0xFFFFFFFF);
      /*configure the MSB of luma start addree*/
      reg_base_ex[w_offset / 4] = (y_addr >> 32);
      /*configure the LSB of luma end address*/
      reg_base_end[w_offset / 4] = ((y_addr + y_size - 1) & 0xFFFFFFFF);
      /*configure the LSB of luma end address*/
      reg_base_end_ex[w_offset / 4] = ((y_addr + y_size - 1) >> 32);
      if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)
         && dec_dwl->ppu_cfg[core_id][i].chroma_format != PP_CHROMA_422
         && !dec_dwl->ppu_cfg[core_id][i].rgb) {
        /*configure the LSB of chroma start address*/
        reg_base[w_offset / 4 + 1] = (uv_addr & 0xFFFFFFFF);
        /*configure the MSB of chroma start addree*/
        reg_base_ex[w_offset / 4 + 1] = (uv_addr  >> 32);
        /*configure the LSB of chroma end address*/
        reg_base_end[w_offset / 4 + 1] = ((uv_addr + uv_size  - 1) & 0xFFFFFFFF);
        /*configure the MSB of chroma end address*/
        reg_base_end_ex[w_offset / 4 + 1] = ((uv_addr + uv_size  - 1) >> 32);
      }

      w_offset += 0x8;

      pp_enable_channel++;
    }
  }

  w_offset = 0;
  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dec_dwl->ppu_cfg[core_id][i].enabled || !dec_dwl->ppu_cfg[core_id][i].dec400_enabled)
      continue;

    if (dec_dwl->ppu_cfg[core_id][i].enabled == 1) {
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG] + w_offset, reg_config[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_EXCONFIG] + w_offset, reg_config_ex[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG] + w_offset + 0x4, reg_config[w_offset / 4 + 1]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_EXCONFIG] + w_offset + 0x4, reg_config_ex[w_offset / 4 + 1]);

      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASE] + w_offset, reg_base[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASEEX] + w_offset, reg_base_ex[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASE] + w_offset + 0x4, reg_base[w_offset / 4 + 1]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASEEX] + w_offset + 0x4, reg_base_ex[w_offset / 4 + 1]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_END] + w_offset, reg_base_end[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_ENDEX] + w_offset, reg_base_end_ex[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_END] + w_offset + 0x4, reg_base_end[w_offset / 4 + 1]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_ENDEX] + w_offset + 0x4, reg_base_end_ex[w_offset / 4 + 1]);
    }
    w_offset += 0x8;
  }

  w_offset = 0;

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dec_dwl->ppu_cfg[core_id][i].enabled) {
      if (mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome || dec_dwl->ppu_cfg[core_id][i].rgb
         || dec_dwl->ppu_cfg[core_id][i].chroma_format == PP_CHROMA_422)
        dec_dwl->ppu_cfg[core_id][i].chroma_size = 0;
      pp_total_buffer_size += dec_dwl->ppu_cfg[core_id][i].luma_size + dec_dwl->ppu_cfg[core_id][i].chroma_size;
      if (pp_bus_address_start == 0) {
        reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
        y_addr = (dec_dwl->dwl_shadow_regs[core_id][reg_addr_y] | ((u64)dec_dwl->dwl_shadow_regs[core_id][reg_addr_y - 1] << 32));
        pp_bus_address_start = y_addr;
      }
    }
  }


  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dec_dwl->ppu_cfg[core_id][i].enabled || !dec_dwl->ppu_cfg[core_id][i].dec400_enabled)
      continue;
    if(dec_dwl->ppu_cfg[core_id][i].enabled == 1) {
      luma_table_offset = dec_dwl->ppu_cfg[core_id][i].luma_offset + dec_dwl->ppu_cfg[core_id][i].luma_size + dec400_header_size;
      chroma_table_offset = dec_dwl->ppu_cfg[core_id][i].chroma_offset + dec_dwl->ppu_cfg[core_id][i].chroma_size + dec400_header_size;
      /*configure the LSB of the luma table address*/
      reg_base[w_offset / 4] = ((pp_bus_address_start + luma_table_offset)&0xFFFFFFFF);
       /*configure the MSB of the luma table address*/
      reg_base_ex[w_offset / 4] = ((u64)(pp_bus_address_start + luma_table_offset) >> 32);
      if ((!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) && (!dec_dwl->ppu_cfg[core_id][i].rgb)
          && dec_dwl->ppu_cfg[core_id][i].chroma_format != PP_CHROMA_422) {
        reg_base[w_offset / 4 + 1] = ((pp_bus_address_start + chroma_table_offset)&0xFFFFFFFF);
        /*configure the MSB of the chroma table address*/
        reg_base_ex[w_offset / 4 + 1] = ((u64)(pp_bus_address_start + chroma_table_offset) >> 32);
      }
      w_offset += 0x8;
    }
  }

  w_offset = 0;
  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dec_dwl->ppu_cfg[core_id][i].enabled || !dec_dwl->ppu_cfg[core_id][i].dec400_enabled)
      continue;
    if(dec_dwl->ppu_cfg[core_id][i].enabled == 1) {
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE] + w_offset, reg_base[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASEEX] + w_offset, reg_base_ex[w_offset / 4]);
      if (!(mono_chroma || dec_dwl->ppu_cfg[core_id][i].monochrome)) {
        DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE] + w_offset + 0x4, reg_base[w_offset / 4 + 1]);
        DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASEEX] + w_offset+ 0x4, reg_base_ex[w_offset / 4 + 1]);
      }
      w_offset += 0x8;
    }
  }

  //DWLStatisticsDec400CompressRatio(dec_dwl, core_id);
#ifdef _DWL_DEBUG
  DTRACE_D("gcregAHBDECControl=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROL]));
  DTRACE_D("gcregAHBDECControlEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROLEX]));
  DTRACE_D("gcregAHBDECControlEx2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROLEX2]));
  DTRACE_D("gcregAHBDECIntrEnbl=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBL]));
  DTRACE_D("gcregAHBDECIntrEnblEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBLEX]));
  DTRACE_D("gcregAHBDECIntrEnblEx2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBLEX2]));
  DTRACE_D("%s","luma configure\n");
  DTRACE_D("gcregAHBDECWriteConfig=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG]));
  DTRACE_D("gcregAHBDECWriteExConfig=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_EXCONFIG]));
  DTRACE_D("gcregAHBDECWriteBufferBase=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASE]));
  DTRACE_D("gcregAHBDECWriteBufferBaseEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASEEX]));
  DTRACE_D("gcregAHBDECWriteBufferEnd=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_END]));
  DTRACE_D("gcregAHBDECWriteBufferEndEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_ENDEX]));
  DTRACE_D("gcregAHBDECWriteCacheBase=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE]));
  DTRACE_D("gcregAHBDECWriteCacheBaseEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASEEX]));
  DTRACE_D("%s","chroma configure\n");
  DTRACE_D("gcregAHBDECWriteConfig+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG]+0x4));
  DTRACE_D("gcregAHBDECWriteExConfig+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_EXCONFIG]+0x4));
  DTRACE_D("gcregAHBDECWriteBufferBase+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASE]+0x4));
  DTRACE_D("gcregAHBDECWriteBufferBaseEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASEEX]+0x4));
  DTRACE_D("gcregAHBDECWriteBufferEnd+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_END]+0x4));
  DTRACE_D("gcregAHBDECWriteBufferEndEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_ENDEX]+0x4));
  DTRACE_D("gcregAHBDECWriteCacheBase+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE]+0x4));
  DTRACE_D("gcregAHBDECWriteCacheBaseEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASEEX]+0x4));
#endif
}

#ifdef ASIC_ONL_SIM
/*------------------------------------------------------------------------------
    Function name   : DWLDecF1Configure
    Description     : configure the dec400 F1 before enable the decoding.
    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/
void dpi_DWLDecF1Configure(u32 *reg_base, const void *instance,i32 core_id) {
  /*
   * when input stream is  yuv/Rgb, Resize pic(PP output) and origin pic should enable dec compression.
   * when input stream is hevc/vp9, Resize pic(PP output) should enable dec compression.
   * when input stream is h264,  ref pic and resize pic(PP output) should enable dec compression.
  */
  int i = 0;
  unsigned int y_addr = 0;
  unsigned int uv_addr = 0;
  unsigned int y_size = 0;
  unsigned int uv_size = 0;
  unsigned int mode = 0;
  unsigned int mono_chroma = 0;
  unsigned int w_offset = 0;
  unsigned int pic_interlace=0;
  unsigned int frame_mbs_only_flag = 0;
  unsigned int num_tile_column = 0;
  //unsigned int uv_sel = 0;

  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  mode = (reg_base[3]>>27)&0x1F;
  DTRACE_I("start the dec f1 cfg ,mode = %d!!\n", mode);

  mono_chroma = (reg_base[7]>>30)&0x01;
  if(mode == DWL_CLIENT_TYPE_H264_DEC) {
    frame_mbs_only_flag = ((reg_base[5] & 0x01) == 0);
    pic_interlace = (reg_base[3]>>23)&0x1;
    if((pic_interlace == 1) || (frame_mbs_only_flag == 0)) {
      DTRACE_I("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
      return;
    }
  }
  if((mode == DEC_MODE_HEVC) || (mode == DEC_MODE_VP9)) {
    num_tile_column = (reg_base[10]>>17)&0x7F;
    if(num_tile_column > 1)
      return;
  }
  dpi_dec400_apb_op(gcregAHBDECControl, 0x00810000, 1, MultiStreamId);
  dpi_dec400_apb_op(gcregAHBDECIntrEnbl, 0xFFFFFFFF, 1, MultiStreamId);
  dpi_dec400_apb_op(gcregAHBDECControlEx, 0x000A0000, 1, MultiStreamId);//case806 setting
  //dpi_dec400_apb_op(gcregAHBDECControlEx, 0x00020000, 1, MultiStreamId);
  //dpi_dec400_apb_op(gcregAHBDECIntrEnblEx, 0xFFFFFFFF, 1, MultiStreamId);
  //dpi_dec400_apb_op(gcregAHBDECControlEx2, 0x0000043f, 1, MultiStreamId);
  dpi_dec400_apb_op(gcregAHBDECIntrEnblEx2, 0xFFFFFFFF, 1, MultiStreamId);

  u32 pp_y_addr_reg = 386; /* HWIF_PPX_OUT_LU_BASE_U_LSB */
  u32 pp_c_addr_reg = 388; /* HWIF_PPX_OUT_CH_BASE_U_LSB */
  u32 pp_total_buffer_size=0;
  u8 pp_enable_channel=0;
  u32 reg_addr_y=0;
  u32 reg_addr_c=0;
  addr_t pp_bus_address_start=0;
  u32 dpi_ppu_base = (u32)(HW_TB_PP_LUMA_BASE);

  u32 y_tbl_size[DEC_MAX_PPU_COUNT];
  u32 y_tbl_base[DEC_MAX_PPU_COUNT];
  u32 c_tbl_size[DEC_MAX_PPU_COUNT];
  u32 c_tbl_base[DEC_MAX_PPU_COUNT];

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dwl_inst->ppu_cfg[core_id][i].enabled == 1){
      reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
      reg_addr_c = pp_c_addr_reg + i * PPU_REG_RANGE;
      y_addr = reg_base[reg_addr_y];
      uv_addr = reg_base[reg_addr_c];

      if(dwl_inst->ppu_cfg[core_id][i].tiled_e == 1){
        y_size = dwl_inst->ppu_cfg[core_id][i].ystride * NEXT_MULTIPLE(dwl_inst->ppu_cfg[core_id][i].scale.height,4)/4;
        uv_size = dwl_inst->ppu_cfg[core_id][i].cstride * NEXT_MULTIPLE(((dwl_inst->ppu_cfg[core_id][i].scale.height + 1)/ 2),4)/4;
      }else{
        y_size = dwl_inst->ppu_cfg[core_id][i].ystride * dwl_inst->ppu_cfg[core_id][i].scale.height;
        uv_size = dwl_inst->ppu_cfg[core_id][i].cstride * ((dwl_inst->ppu_cfg[core_id][i].scale.height + 1)/ 2);
      }

      if(pp_bus_address_start == 0)
        pp_bus_address_start = reg_base[reg_addr_y];
      if(dwl_inst->ppu_cfg[core_id][i].pixel_width == 8) {
        dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset, 0x0E020029, 1, MultiStreamId);
        dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset, 0<<16, 1, MultiStreamId);
        if (!(mono_chroma || dwl_inst->ppu_cfg[core_id][i].monochrome)) {
          dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset + 0x4, 0x10020031, 1, MultiStreamId);
          dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset + 0x4, 0<<16, 1, MultiStreamId);
        }
      } else {
          dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset, 0x10020029, 1, MultiStreamId);
          dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset, 1<<16, 1, MultiStreamId);
          if (!(mono_chroma || dwl_inst->ppu_cfg[core_id][i].monochrome)) {
            dpi_dec400_apb_op(gcregAHBDECWriteConfig + w_offset + 0x4, 0x04020031, 1, MultiStreamId);
            dpi_dec400_apb_op(gcregAHBDECWriteExConfig + w_offset + 0x4, 1<<16, 1, MultiStreamId);
          }
      }
      dpi_dec400_apb_op(gcregAHBDECWriteBufferBase + w_offset, dpi_ppu_base, 1, MultiStreamId);
      dpi_dec400_apb_op(gcregAHBDECWriteBufferBaseEx + w_offset, 0x00000000, 1, MultiStreamId);
      dpi_dec400_apb_op(gcregAHBDECWriteBufferEnd + w_offset, dpi_ppu_base + y_size - 1, 1, MultiStreamId);
      dpi_dec400_apb_op(gcregAHBDECWriteBufferEndEx + w_offset, 0x00000000, 1, MultiStreamId);
      if (!(mono_chroma || dwl_inst->ppu_cfg[core_id][i].monochrome)) {
        dpi_dec400_apb_op(gcregAHBDECWriteBufferBase + w_offset + 0x4,dpi_ppu_base + y_size , 1, MultiStreamId);
        dpi_dec400_apb_op(gcregAHBDECWriteBufferBaseEx + w_offset + 0x4, 0x00000000, 1, MultiStreamId);
        dpi_dec400_apb_op(gcregAHBDECWriteBufferEnd + w_offset + 0x4, dpi_ppu_base + y_size + uv_size  - 1, 1, MultiStreamId);
        dpi_dec400_apb_op(gcregAHBDECWriteBufferEndEx + w_offset + 0x4, 0x00000000, 1, MultiStreamId);
      }
      w_offset += 0x8;

      pp_total_buffer_size += y_size;
      if (!(mono_chroma || dwl_inst->ppu_cfg[core_id][i].monochrome))
        pp_total_buffer_size += uv_size;

      dpi_ppu_base = dpi_ppu_base + y_size + uv_size;

      pp_enable_channel++;
    }
  }
  w_offset = 0;

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++ ){
    if(dwl_inst->ppu_cfg[core_id][i].enabled == 1) {
      y_tbl_size[i] = (((NEXT_MULTIPLE(dwl_inst->ppu_cfg[core_id][i].ystride * dwl_inst->ppu_cfg[core_id][i].scale.height,ALIGN(dwl_inst->ppu_cfg[core_id][i].align))/ALIGN(dwl_inst->ppu_cfg[core_id][i].align))*4+7)/8+15)/16*16;
      c_tbl_size[i] = (((NEXT_MULTIPLE(dwl_inst->ppu_cfg[core_id][i].ystride * (dwl_inst->ppu_cfg[core_id][i].scale.height+1)/2,ALIGN(dwl_inst->ppu_cfg[core_id][i].align))/ALIGN(dwl_inst->ppu_cfg[core_id][i].align))*4+7)/8+15)/16*16;

      if ( i == 0 ){
        y_tbl_base[i] = dpi_ppu_base;
      } else {
        y_tbl_base[i] = c_tbl_base[i-1]+c_tbl_size[i-1];
      }
      c_tbl_base[i] = y_tbl_base[i] + y_tbl_size[i];
    }
  }

  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(dwl_inst->ppu_cfg[core_id][i].enabled == 1) {
      dpi_dec400_apb_op(gcregAHBDECWriteCacheBase + w_offset,y_tbl_base[i], 1, MultiStreamId);
      dpi_dec400_apb_op(gcregAHBDECWriteCacheBaseEx + w_offset, 0x00000000, 1, MultiStreamId);
      if (!(mono_chroma || dwl_inst->ppu_cfg[core_id][i].monochrome)) {
        dpi_dec400_apb_op(gcregAHBDECWriteCacheBase + w_offset + 0x4, c_tbl_base[i], 1, MultiStreamId);
        dpi_dec400_apb_op(gcregAHBDECWriteCacheBaseEx + w_offset+ 0x4, 0x00000000, 1, MultiStreamId);
      }
      w_offset += 0x8;
    }
  }
#if 0
  DTRACE_D("gcregAHBDECControl=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECControl));
  DTRACE_D("gcregAHBDECControlEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECControlEx));
  DTRACE_D("gcregAHBDECControlEx2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECControlEx2));
  DTRACE_D("gcregAHBDECIntrEnbl=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECIntrEnbl));
  DTRACE_D("gcregAHBDECIntrEnblEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECIntrEnblEx));
  DTRACE_D("gcregAHBDECIntrEnblEx2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECIntrEnblEx2));
  DTRACE_D("%s","luma configure\n");
  DTRACE_D("gcregAHBDECWriteConfig=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteConfig));
  DTRACE_D("gcregAHBDECWriteExConfig=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteExConfig));
  DTRACE_D("gcregAHBDECWriteBufferBase=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBase));
  DTRACE_D("gcregAHBDECWriteBufferBaseEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBaseEx));
  DTRACE_D("gcregAHBDECWriteBufferEnd=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEnd));
  DTRACE_D("gcregAHBDECWriteBufferEndEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEndEx));
  DTRACE_D("gcregAHBDECWriteCacheBase=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBase));
  DTRACE_D("gcregAHBDECWriteCacheBaseEx=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBaseEx));
  DTRACE_D("%s","chroma configure\n");
  DTRACE_D("gcregAHBDECWriteConfig+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteConfig+0x4));
  DTRACE_D("gcregAHBDECWriteExConfig+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteExConfig+0x4));
  DTRACE_D("gcregAHBDECWriteBufferBase+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBase+0x4));
  DTRACE_D("gcregAHBDECWriteBufferBaseEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferBaseEx+0x4));
  DTRACE_D("gcregAHBDECWriteBufferEnd+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEnd+0x4));
  DTRACE_D("gcregAHBDECWriteBufferEndEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteBufferEndEx+0x4));
  DTRACE_D("gcregAHBDECWriteCacheBase+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBase+0x4));
  DTRACE_D("gcregAHBDECWriteCacheBaseEx+4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, gcregAHBDECWriteCacheBaseEx+0x4));
#endif
}
#endif // ASIC_ONL_SIM
