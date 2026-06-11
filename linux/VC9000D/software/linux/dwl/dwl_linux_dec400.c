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
#include "basetype.h"
#include "decapicommon.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dwlthread.h"
#include "dwl_vcmd_common.h"
#include "dwl_linux_dec400.h"
#include "dec_log.h"

#ifdef __linux__
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
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

/*below registers will be used to configure DEC400, and the value is the offset*/
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

/* 8KB for each command buffer */
/* FIXME(min): find a more flexible approach. */
#define VCMD_BUF_SIZE (8*1024)
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

i32 DWLConfigureCmdBufForDec400(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];
  u32 i = 0;
  u64 y_addr = 0;
  u64 uv_addr = 0;
  u32 y_size = 0;
  u32 uv_size = 0;
  u32 mode = 0;
  u32 mono_chroma = 0;
  u32 w_offset = 0;
  u32 pic_interlace=0;
  u32 frame_mbs_only_flag = 0;
  u32 reg_control[3]={0,0,0};
  u32 reg_config[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_config_ex[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base_ex[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base_end[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 reg_base_end_ex[DEC_MAX_PPU_COUNT*2]={0,0,0,0};
  u32 *regs = vcmd->reg_mirror;
  mode = (regs[3]>>27)&0x1F;
  u32 dec400_index = 0;
  PpUnitIntConfig *ppu_cfg  = vcmd->ppu_cfg;

  if (mode != DEC_MODE_VC1 && mode != DEC_MODE_VP8)
    mono_chroma = (regs[7]>>30)&0x01;
  if (mode != DEC_MODE_JPEG)
    pic_interlace = (regs[3]>>23)&0x1;
  if(mode == DEC_MODE_H264 || mode == DEC_MODE_H264_H10P)
    frame_mbs_only_flag = (regs[5])&0x01;

  if((pic_interlace == 1) || (frame_mbs_only_flag == 1)) {
    printf("mode=%u ,pic_interlace=%u frame_mbs_only_flag=%u,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
    reg_control[0] = 0x00810002;
    CWLCollectWriteRegData(vcmd,
                           &reg_control[0],
                           dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_CONTROL]/4,
                           1);
    return DWL_ERROR;
  }

  /* bypass dec400 if all PPs' dec400 disabled */
  if (!ppu_cfg || (!ppu_cfg->dec400_enabled && !(ppu_cfg + 1)->dec400_enabled &&
      !(ppu_cfg + 2)->dec400_enabled && !(ppu_cfg + 4)->dec400_enabled &&
      !(ppu_cfg + 5)->dec400_enabled)) {
    reg_control[0] = 0x00810002;
    CWLCollectWriteRegData(vcmd,
                       &reg_control[0],
                       dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_CONTROL]/4,
                       1);
    return DWL_ERROR;
  }

  /* new project use 64 alignment from 20210305 */
  reg_control[0] = 0x00810000 | (0x1 << 8); //[8]  align mode : 64
  reg_control[1] = 0x000A0000;
  reg_control[2] = 0x003FD021;
  CWLCollectWriteRegData(vcmd,
                         &reg_control[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_CONTROL]/4, /* dec400 */
                         3);
  reg_control[0] = 0xFFFFFFFF;
  CWLCollectWriteRegData(vcmd,
                         &reg_control[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBL]/4, /* dec400 */
                         1);
  CWLCollectWriteRegData(vcmd,
                         &reg_control[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_INTR_ENBLEX2]/4, /* dec400 */
                         1);
  u32 pp_y_addr_reg = 386; /* HWIF_PPX_OUT_LU_BASE_U_LSB */
  u32 pp_c_addr_reg = 388; /* HWIF_PPX_OUT_CH_BASE_U_LSB */
  u8 pp_enable_channel=0;
  u32 reg_addr_y=0;
  u32 reg_addr_c=0;
  addr_t pp_bus_address_start=0;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled || !ppu_cfg->dec400_enabled) continue;

    if (ppu_cfg->enabled == 1) {
      reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
      reg_addr_c = pp_c_addr_reg + i * PPU_REG_RANGE;
      y_addr = (regs[reg_addr_y] | ((u64)regs[reg_addr_y - 1] << 32));
      uv_addr = (regs[reg_addr_c] | ((u64)regs[reg_addr_c - 1] << 32));

      if (ppu_cfg->tiled_e == 1) {
        if (ppu_cfg->rgb) {
          y_size = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height, 64)/64;
          uv_size = 0;
          if (ppu_cfg->tile_mode == TILED64x64) {
            if (ppu_cfg->rgb_format == PP_OUT_ABGR888 || ppu_cfg->rgb_format == PP_OUT_ARGB888) {
              reg_config[w_offset / 4] = 0x30001;
              reg_config_ex[w_offset / 4] = 0<<16;
            } else if (ppu_cfg->rgb_format == PP_OUT_XBGR888 || ppu_cfg->rgb_format == PP_OUT_XRGB888) {
              reg_config[w_offset / 4] = 0x30009;
              reg_config_ex[w_offset / 4] = 0<<16;
            } else if (ppu_cfg->rgb_format == PP_OUT_A2B10G10R10 ||
                       ppu_cfg->rgb_format == PP_OUT_A2R10G10B10) {
              reg_config[w_offset / 4] = 0x30079;
              reg_config_ex[w_offset / 4] = 0<<16;
            } else {
              continue;
            }
          } else {
            continue;
          }
        } else {
          y_size = ppu_cfg->ystride * NEXT_MULTIPLE(ppu_cfg->scale.height,4)/4;
          uv_size = ppu_cfg->cstride * NEXT_MULTIPLE((ppu_cfg->scale.height/ 2),4)/4;

          if(ppu_cfg->pixel_width == 8) {
            reg_config[w_offset / 4] = 0x0E030029;
            reg_config_ex[w_offset / 4] = 0<<16;
            if (!(mono_chroma || ppu_cfg->monochrome)) {
              reg_config[w_offset / 4 + 1] = 0x10030031;
              reg_config_ex[w_offset / 4 + 1] =  0<<16;
            }
          } else {
            reg_config[w_offset / 4] = 0x10030029;
            reg_config_ex[w_offset / 4] = 1<<16;
            if (!(mono_chroma || ppu_cfg->monochrome)) {
              reg_config[w_offset / 4 + 1] = 0x04030031;
              reg_config_ex[w_offset / 4 + 1] = 1<<16;
            }
          }
        }
      } else if (ppu_cfg->rgb) {
        y_size = ppu_cfg->ystride * ppu_cfg->scale.height;
        uv_size = 0;
        if (ppu_cfg->rgb_format == PP_OUT_ABGR888 || ppu_cfg->rgb_format == PP_OUT_ARGB888) {
          reg_config[w_offset / 4] = 0x1E030001;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (ppu_cfg->rgb_format == PP_OUT_XBGR888 || ppu_cfg->rgb_format == PP_OUT_XRGB888) {
          reg_config[w_offset / 4] = 0x1E030009;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (ppu_cfg->rgb_format == PP_OUT_A2B10G10R10 ||
                   ppu_cfg->rgb_format == PP_OUT_A2R10G10B10) {
          reg_config[w_offset / 4] = 0x1E030079;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else {
          continue;
        }
      } else if (ppu_cfg->planar) {
        y_size = ppu_cfg->ystride * ppu_cfg->scale.height;
        uv_size = ppu_cfg->cstride * ppu_cfg->scale.height;

        if(ppu_cfg->pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12030029;
          reg_config_ex[w_offset / 4] = 0<<16;
          if (!(mono_chroma || ppu_cfg->monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x12030029;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14030029;
          reg_config_ex[w_offset / 4] = 1<<16;
          if (!(mono_chroma || ppu_cfg->monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14030029;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if(ppu_cfg->chroma_format < PP_CHROMA_422) {
        y_size = ppu_cfg->ystride * ppu_cfg->scale.height;
        uv_size = ppu_cfg->cstride * ppu_cfg->scale.height / 2;

        if(ppu_cfg->pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12030029;
          reg_config_ex[w_offset / 4] = 0<<16;
          if (!(mono_chroma || ppu_cfg->monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14030031;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14030029;
          reg_config_ex[w_offset / 4] = 1<<16;
          if (!(mono_chroma || ppu_cfg->monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x1E030031;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if(ppu_cfg->chroma_format == PP_CHROMA_422) {
        y_size = ppu_cfg->ystride * ppu_cfg->scale.height;
        uv_size = 0;

        if(ppu_cfg->out_uyvy) {
          reg_config[w_offset / 4] = 0x14030019;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (ppu_cfg->out_yuyv) {
          reg_config[w_offset / 4] = 0x14030021;
          reg_config_ex[w_offset / 4] = 0<<16;
        }
      }

      reg_base[w_offset / 4] = (y_addr & 0xFFFFFFFF);
      reg_base_ex[w_offset / 4] = (y_addr >> 32);
      reg_base_end[w_offset / 4] = ((y_addr + y_size - 1) & 0xFFFFFFFF);
      reg_base_end_ex[w_offset / 4] = ((y_addr + y_size - 1) >> 32);
      if (!(mono_chroma || ppu_cfg->monochrome) && !ppu_cfg->rgb && ppu_cfg->chroma_format != PP_CHROMA_422) {
        reg_base[w_offset / 4 + 1] = (uv_addr & 0xFFFFFFFF);
        reg_base_ex[w_offset / 4 + 1] = (uv_addr  >> 32);
        reg_base_end[w_offset / 4 + 1] = ((uv_addr + uv_size  - 1) & 0xFFFFFFFF);
        reg_base_end_ex[w_offset / 4 + 1] = ((uv_addr + uv_size  - 1) >> 32);
      }
      w_offset += 0x8;

      pp_enable_channel++;
    }
  }

  CWLCollectWriteRegData(vcmd,
                         &reg_config[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  CWLCollectWriteRegData(vcmd,
                         &reg_config_ex[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_EXCONFIG]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  CWLCollectWriteRegData(vcmd,
                         &reg_base[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASE]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  CWLCollectWriteRegData(vcmd,
                         &reg_base_ex[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASEEX]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  CWLCollectWriteRegData(vcmd,
                         &reg_base_end[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_END]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  CWLCollectWriteRegData(vcmd,
                         &reg_base_end_ex[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_ENDEX]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  w_offset = 0;
  ppu_cfg  = vcmd->ppu_cfg;
  for(i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if(ppu_cfg->enabled) {
      if (mono_chroma || ppu_cfg->monochrome || ppu_cfg->chroma_format == PP_CHROMA_422 || ppu_cfg->rgb)
        ppu_cfg->chroma_size = 0;
      if (pp_bus_address_start == 0) {
        reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
        y_addr = (regs[reg_addr_y] | ((u64)regs[reg_addr_y - 1] << 32));
        pp_bus_address_start = y_addr - ppu_cfg->luma_offset;
      }
    }
  }

  ppu_cfg  = vcmd->ppu_cfg;
  for(i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
    if(!ppu_cfg->enabled || !ppu_cfg->dec400_enabled) continue;

    if(ppu_cfg->enabled == 1) {
      /* luma data is put into the end region of dec400_luma_table,
        chroma data is put into the end region of dec400_chroma_table */
      /* 207D need to plus DEC400_IN_HEADER_SIZE */
      addr_t dec400_table_luma_offset = ppu_cfg->dec400_pln0_tile_status_offset + DEC400_IN_HEADER_SIZE;
      addr_t dec400_table_chroma_offset = ppu_cfg->dec400_pln1_tile_status_offset + DEC400_IN_HEADER_SIZE;
      addr_t ts_pln0_addr = pp_bus_address_start + dec400_table_luma_offset;
      addr_t ts_pln1_addr = pp_bus_address_start + dec400_table_chroma_offset;

      reg_base[w_offset / 4] = ts_pln0_addr & 0xFFFFFFFF;
      reg_base_ex[w_offset / 4] = (u64)ts_pln0_addr >> 32;
      if (!(mono_chroma || ppu_cfg->monochrome) && !ppu_cfg->rgb && ppu_cfg->chroma_format != PP_CHROMA_422) {
        reg_base[w_offset / 4 + 1] = ts_pln1_addr & 0xFFFFFFFF;
        reg_base_ex[w_offset / 4 + 1] = (u64)ts_pln1_addr >> 32;
      }
      w_offset += 0x8;
    }
  }
  CWLCollectWriteRegData(vcmd,
                         &reg_base[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  CWLCollectWriteRegData(vcmd,
                         &reg_base_ex[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASEEX]/4, /* dec400 */
                         DEC_MAX_PPU_COUNT*2);
  return DWL_OK;
}

void DWLFuseCmdBufForDec400(const void *instance, u32 cmd_buf_id, u32* index) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];
  u32 reg_control[3]={0,0,0};
  u32 *regs = vcmd->reg_mirror;
  u32 mode = (regs[3]>>27)&0x1F;
  u32 pic_interlace = 0;
  u32 dec400_index = 0;
  addr_t status_buf_addr = 0;
  PpUnitIntConfig *ppu_cfg  = vcmd->ppu_cfg;

  if(mode == DEC_MODE_H264 || mode == DEC_MODE_H264_H10P) {
    u32 frame_mbs_only_flag = ((regs[5] & 0x01) == 0);
    pic_interlace = (regs[3]>>23)&0x1;
    if((pic_interlace == 1) || (frame_mbs_only_flag == 0)) {
      DTRACE_D("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
      return;
    }
  } else if (mode == DEC_MODE_MPEG4 || mode == DEC_MODE_VC1 ||
             mode == DEC_MODE_MPEG2 || mode == DEC_MODE_RV ||
             mode == DEC_MODE_AVS || mode == DEC_MODE_AVS2) {
    pic_interlace =  (regs[3]>>23)&0x1;
    if (pic_interlace == 1) {
      DTRACE_D("mode=%d ,pic_interlace=%d ,BYPASS DEC400!!!\n",mode,pic_interlace);
      return;
    }
  }


  /* bypass dec400 if all PPs' dec400 disabled */
  if (!ppu_cfg || (!ppu_cfg->dec400_enabled && !(ppu_cfg + 1)->dec400_enabled &&
      !(ppu_cfg + 2)->dec400_enabled && !(ppu_cfg + 4)->dec400_enabled &&
      !(ppu_cfg + 5)->dec400_enabled)) {
    DTRACE_D("mode=%d ,DEC400 not enabled ,BYPASS DEC400!!!\n", mode);
    return;
  }

  reg_control[0] = 0x00810101;
  CWLCollectWriteRegData(vcmd,
                         &reg_control[0],
                         dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_CONTROL]/4, /* flush dec400 */
                         1);
  /* Wait interrupt from DEC400 */
  CWLCollectStallData(vcmd,
                      (dev->vcmd_params.vcmd_hw_version_id == HW_ID_1_0_C ?
                       VCD_DEC400_INT_MASK_1_0_C : VCD_DEC400_INT_MASK));

#ifdef SUPPORT_MMU
  status_buf_addr = vcmd->mmu_status_bus_addr;
#else
  status_buf_addr = vcmd->status_bus_addr;
#endif

  status_buf_addr += dev->vcmd_params.submodule_main_addr/2;
  ASSERT((dev->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
  CWLCollectReadRegData(vcmd,
                        dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_INTR_ACKNOWLEDGE]/4, 1, /* Acknowledge */
                        status_buf_addr + 4 * *index);
  *index += 1;
  ASSERT((dev->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
  CWLCollectReadRegData(vcmd,
                        dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_INTR_ACKNOWLEDGEEX2]/4, 1, /* AcknowledgeEx2 */
                        status_buf_addr + 4 * *index);
  *index += 1;

  /* disable all channels */
  u32 reg_config[32] = {0};
  CWLCollectWriteRegData(vcmd,
                        &reg_config[0],
                        dev->vcmd_params.submodule_dec400_addr/4 + dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG]/4,
                        32);
}

/*------------------------------------------------------------------------------
    Function name   : DWLDec400WriteReg
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/

void DWLDec400WriteReg(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev= (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec400_shadow_regs[core_id];

  DTRACE_D("DECF1 Core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset/4,
            offset, value);

  ASSERT(dec_dwl != NULL);
  ASSERT(core_id < dev->num_cores);
  reg_base[offset/4] = value;
}

/*------------------------------------------------------------------------------
    Function name   : DWLDec400WriteRegToHw
    Description     : Write a value to a hardware IO register(directly write to HW)

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLDec400WriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec400_shadow_regs[core_id];
  DTRACE_D("DECF1 Core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset/4,
            offset, value);

  ASSERT(dec_dwl != NULL);
  ASSERT(core_id < (i32)dev->num_cores);
  DWLDec400WriteReg(dec_dwl, core_id, offset, value);

  core.id = core_id;
  core.regs = &reg_base[offset/4];
  core.reg_id = offset/4;
  core.size = 4;
  core.type = HW_DEC400;

  if(ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRO_DEC400_IOCS_DEC_WRITE_REG failed\n");
    ASSERT(0);
  }
}
/*------------------------------------------------------------------------------
    Function name   : DWLDec400ReadReg
    Description     : Read the value of a hardware IO register

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLDec400ReadReg(const void *instance, i32 core_id, u32 offset) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec400_shadow_regs[core_id];
  u32 val = 0;

  ASSERT(dec_dwl != NULL);
  ASSERT(core_id < dev->num_cores);

  DTRACE_D("DECF1 Core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset/4,
            offset, val);

  val = reg_base[offset/4];
  return val;
}
/*------------------------------------------------------------------------------
    Function name   : DWLDec400ReadRegFromHw
    Description     : Read the value of a hardware IO register(directly read from HW)

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLDec400ReadRegFromHw(const void *instance, i32 core_id, u32 offset) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec400_shadow_regs[core_id];
  struct core_desc core;
  u32 val = 0;

  ASSERT(dec_dwl != NULL);
  ASSERT(core_id < (i32)dev->num_cores);

  DTRACE_D("DECF1 Core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, offset/4,
            offset, val);
  core.id = core_id;
  core.regs = &reg_base[offset/4];
  core.reg_id = offset/4;
  core.size = 4;
  core.type = HW_DEC400;

  if(ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRO_DEC400_IOCS_DEC_READ_REG failed\n");
    ASSERT(0);
  }

  val = DWLDec400ReadReg(instance, core_id, offset);
  return val;
}

void DWLDec400DisableAll(const void *instance, i32 core_id) {
  int i;
  u32 dec400_index = 0;

  /*clear the condigure and address for each channel*/
  for(i = 0; i < 32; i++) {
    DWLDec400WriteRegToHw(instance, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG] + i*4, 0x0|(0x5<<3)|(0x3<<16)|(0x9<<25));
    DWLDec400WriteRegToHw(instance, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_EXCONFIG] + i*4, 0x0);
    DWLDec400WriteRegToHw(instance, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_BASE] + i*4, 0xffffffff);
    DWLDec400WriteRegToHw(instance, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_BUFFER_END] + i*4, 0xffffffff);
    DWLDec400WriteRegToHw(instance, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE] + i*4, 0xffffffff);
  }
  UNUSED(dec400_index);
}

void DWLStatisticsDec400CompressRatio(const void *instance, i32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  u32 sum_tile128 = 0;
  u32 num_tile_64B_tile128 = 0;

  u32 sum_tile256 = 0;
  u32 num_tile_64B_tile256 = 0;
  u32 num_tile_128B_tile256 = 0;
  u32 num_tile_192B_tile256 = 0;
  u32 dec400_index = 0;

  num_tile_64B_tile128 = DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CRDEBUG_TILE128_TYPE2]);
  sum_tile128 = num_tile_64B_tile128 + DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CRDEBUG_TILE128_TYPE0]);

  num_tile_64B_tile256 = DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CRDEBUG_TILE256_TYPE2]);
  num_tile_128B_tile256 = DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CRDEBUG_TILE256_TYPE4]);
  num_tile_192B_tile256 = DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CRDEBUG_TILE256_TYPE6]);

  sum_tile256 = num_tile_64B_tile256 + num_tile_128B_tile256 + num_tile_192B_tile256 +
                 DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CRDEBUG_TILE256_TYPE0]);

  double compress_ration = 0;

  if(sum_tile128 != 0 && sum_tile256 != 0 ) {
    printf("DWLStaticDec400CompressRatio WARNING tile128 and tile256 should not have values simultaneously\n");
  } else if(sum_tile128 != 0){
    compress_ration = (double)(num_tile_64B_tile128 )/(sum_tile128 * 2);
  } else if (sum_tile256 != 0) {
    compress_ration =(double) ((num_tile_64B_tile256 + num_tile_128B_tile256 *2 + num_tile_192B_tile256 * 3)/(sum_tile256 * 4));
  }
  printf("DWLStaticDec400CompressRatio core_id %d, ration %0.2f \n", core_id, compress_ration);
  UNUSED(dec400_index);
}

/*------------------------------------------------------------------------------
    Function name   : DWLDecF1Configure
    Description     : configure the dec400 F1 before enable the decoding.

    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/
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
  u32 dec400_index = 0;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *dec_regs = &dev->dec_shadow_regs[core_id];

  mode = (dec_regs[3] >> 27) & 0x1F;
  DTRACE_D("start the dec f1 cfg ,mode = %d!!\n", mode);

  reg_control[0] = (0x20 << 18) | //[23:18]  sw_flush_id
                   (0x1 << 16); //[16]  disable_hw_flush
  reg_control[1] = (0x1 << 19) | //[19]
                   (0x1 << 17); //[17]
  reg_control[2] = 0x003FD021;

  /* new project use 64 alignment from 20210305 */
  reg_control[0] |= (0x1 << 8); //[8]  align mode : 64

  if (mode != DEC_MODE_VC1 && mode != DEC_MODE_VP8)
    mono_chroma = (dec_regs[7] >> 30) & 0x01;
  if (mode != DEC_MODE_JPEG)
    pic_interlace = (dec_regs[3] >> 23) & 0x1;
  if(mode == DEC_MODE_H264 || mode == DEC_MODE_H264_H10P)
    frame_mbs_only_flag = (dec_regs[5]) & 0x01;

  if((pic_interlace == 1) || (frame_mbs_only_flag == 1)) {
    DTRACE_D("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
    reg_control[0] = 0x00810002;
    DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_CONTROL], reg_control[0]);
    return;
  }

  /* bypass dec400 if all PPs' dec400 disabled */
  if (!dev->ppu_cfg[core_id][0].dec400_enabled &&
      !dev->ppu_cfg[core_id][1].dec400_enabled &&
      !dev->ppu_cfg[core_id][2].dec400_enabled &&
      !dev->ppu_cfg[core_id][3].dec400_enabled &&
      !dev->ppu_cfg[core_id][4].dec400_enabled &&
      !dev->ppu_cfg[core_id][5].dec400_enabled) {
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
  u8 pp_enable_channel=0;
  u32 reg_addr_y=0;
  u32 reg_addr_c=0;
  addr_t pp_bus_address_start=0;

  for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dev->ppu_cfg[core_id][i].enabled || !dev->ppu_cfg[core_id][i].dec400_enabled)
      continue;

    if (dev->ppu_cfg[core_id][i].enabled == 1) {
      reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
      reg_addr_c = pp_c_addr_reg + i * PPU_REG_RANGE;
      y_addr = (reg_base[reg_addr_y] | ((u64)reg_base[reg_addr_y - 1] << 32));
      uv_addr = (reg_base[reg_addr_c] | ((u64)reg_base[reg_addr_c - 1] << 32));

      if (dev->ppu_cfg[core_id][i].tiled_e == 1) {
        if (dev->ppu_cfg[core_id][i].rgb) {
          y_size = dev->ppu_cfg[core_id][i].ystride * NEXT_MULTIPLE(dev->ppu_cfg[core_id][i].scale.height,64)/64;
          uv_size = 0;
          if (dev->ppu_cfg[core_id][i].tile_mode == TILED64x64) {
            if (dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_ABGR888 ||
                dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_ARGB888) {
              /* (30:25)| tile 8x8 mode (0) */
              reg_config[w_offset / 4] = 0x30001;
              //reg_config[w_offset / 4] = 0x20001;
              /* 8bit input */
              reg_config_ex[w_offset / 4] = 0<<16;
            } else if (dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_XBGR888 ||
                       dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_XRGB888) {
              /* (30:25)| tile 8x8 mode (0) */
              /* (7:3)  | XRGB8 (1) */
              reg_config[w_offset / 4] = 0x30009;
              /* 8bit input */
              reg_config_ex[w_offset / 4] = 0<<16;
            } else if (dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2B10G10R10 ||
                       dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2R10G10B10) {
              reg_config[w_offset / 4] = 0x30079;
              /* (18:16)|   1 means BIT10*/
              reg_config_ex[w_offset / 4] = 0<<16;
            } else {
              continue;
            }
          } else {
            continue;
          }
        } else {
          y_size = dev->ppu_cfg[core_id][i].ystride * NEXT_MULTIPLE(dev->ppu_cfg[core_id][i].scale.height,4)/4;
          uv_size = dev->ppu_cfg[core_id][i].cstride * NEXT_MULTIPLE((dev->ppu_cfg[core_id][i].scale.height/2),4)/4;

          if (dev->ppu_cfg[core_id][i].pixel_width == 8) {
            /*(30:25)|   how many pixels in the tile and the walking direction in the tile, 7 means TILE64X4
              (17:16)|   Compression result size alignment mode Only support Align32Byte in SUNPLUS
              (7:3)  |   Compression color format,5 indicates Y, 6 indicates UV
              (0:0)  |   Compression enable */
            reg_config[w_offset / 4] = 0x0E030029;
            /*8bit input*/
            reg_config_ex[w_offset / 4] = 0<<16;
            if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
              //(30:25)|   8 means TILE32X4(chroma*2 == luma)
              reg_config[w_offset / 4 + 1] = 0x10030031;
              reg_config_ex[w_offset / 4 + 1] =  0<<16;
            }
          } else {
            //(30:25)|   8 means TILE32X4(32*4*2byte/pixel=256 align)
            reg_config[w_offset / 4] = 0x10030029;
            /* (18:16)|   1 means BIT10*/
            reg_config_ex[w_offset / 4] = 1<<16;
            if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
              reg_config[w_offset / 4 + 1] = 0x04030031;
              reg_config_ex[w_offset / 4 + 1] = 1<<16;
            }
          }
        }
      } else if (dev->ppu_cfg[core_id][i].rgb) {
        y_size = dev->ppu_cfg[core_id][i].ystride * dev->ppu_cfg[core_id][i].scale.height;
        uv_size = 0;
        if (dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_ABGR888 ||
            dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_ARGB888) {
          // (30:25)| F means RASTER64x1
          reg_config[w_offset / 4] = 0x1E030001;
          /*8bit input*/
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_XBGR888 ||
                   dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_XRGB888) {
          /* (30:25)| tile 8x8 mode (0) */
          /* (7:3)  | XRGB8 (1) */
          reg_config[w_offset / 4] = 0x1E030009;
          /* 8bit input */
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2B10G10R10 ||
                   dev->ppu_cfg[core_id][i].rgb_format == PP_OUT_A2R10G10B10) {
          // (30:25)| F means RASTER64x1
          reg_config[w_offset / 4] = 0x1E030079;
          /* (18:16)|   1 means BIT10*/
          reg_config_ex[w_offset / 4] = 0<<16;
        } else {
          continue;
        }
      } else if (dev->ppu_cfg[core_id][i].planar) {
        y_size = dev->ppu_cfg[core_id][i].ystride * dev->ppu_cfg[core_id][i].scale.height;
        uv_size = dev->ppu_cfg[core_id][i].cstride * dev->ppu_cfg[core_id][i].scale.height;

        if(dev->ppu_cfg[core_id][i].pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12030029;
          /*8bit input*/
          reg_config_ex[w_offset / 4] = 0<<16;

          if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x12030029;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14030029;
          /*10bit input*/
          reg_config_ex[w_offset / 4] = 1<<16;

          if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14030029;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if (dev->ppu_cfg[core_id][i].chroma_format < PP_CHROMA_422){
        y_size = dev->ppu_cfg[core_id][i].ystride * dev->ppu_cfg[core_id][i].scale.height;
        uv_size = dev->ppu_cfg[core_id][i].cstride * dev->ppu_cfg[core_id][i].scale.height/2;

        if(dev->ppu_cfg[core_id][i].pixel_width == 8) {
          reg_config[w_offset / 4] = 0x12030029;
          /*8bit input*/
          reg_config_ex[w_offset / 4] = 0<<16;

          if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x14030031;
            reg_config_ex[w_offset / 4 + 1] =  0<<16;
          }
        } else {
          reg_config[w_offset / 4] = 0x14030029;
          /*10bit input*/
          reg_config_ex[w_offset / 4] = 1<<16;

          if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
            reg_config[w_offset / 4 + 1] = 0x1E030031;
            reg_config_ex[w_offset / 4 + 1] = 1<<16;
          }
        }
      } else if (dev->ppu_cfg[core_id][i].chroma_format == PP_CHROMA_422) {
        y_size = dev->ppu_cfg[core_id][i].ystride * dev->ppu_cfg[core_id][i].scale.height;
        uv_size = 0;

        if(dev->ppu_cfg[core_id][i].out_uyvy) {
          reg_config[w_offset / 4] = 0x14030019;
          reg_config_ex[w_offset / 4] = 0<<16;
        } else if (dev->ppu_cfg[core_id][i].out_yuyv) {
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
      if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome) && (!dev->ppu_cfg[core_id][i].rgb)
         && dev->ppu_cfg[core_id][i].chroma_format != PP_CHROMA_422) {
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
    if(!dev->ppu_cfg[core_id][i].enabled || !dev->ppu_cfg[core_id][i].dec400_enabled)
      continue;

    if (dev->ppu_cfg[core_id][i].enabled == 1) {
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
    if(dev->ppu_cfg[core_id][i].enabled) {
      if ((mono_chroma || dev->ppu_cfg[core_id][i].monochrome) || dev->ppu_cfg[core_id][i].rgb
          || dev->ppu_cfg[core_id][i].chroma_format == PP_CHROMA_422)
        dev->ppu_cfg[core_id][i].chroma_size = 0;
      if (pp_bus_address_start == 0) {
        reg_addr_y = pp_y_addr_reg + i * PPU_REG_RANGE;
        y_addr = (reg_base[reg_addr_y] | ((u64)reg_base[reg_addr_y - 1] << 32));
        pp_bus_address_start = y_addr - dev->ppu_cfg[core_id][i].luma_offset;
      }
    }
  }


  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dev->ppu_cfg[core_id][i].enabled || !dev->ppu_cfg[core_id][i].dec400_enabled)
      continue;
    if(dev->ppu_cfg[core_id][i].enabled == 1) {
      /* luma data is put into the end region of dec400_luma_table,
        chroma data is put into the end region of dec400_chroma_table */
      /* 207D need to plus DEC400_IN_HEADER_SIZE */
      addr_t dec400_table_luma_offset = dev->ppu_cfg[core_id][i].dec400_pln0_tile_status_offset + DEC400_IN_HEADER_SIZE;
      addr_t dec400_table_chroma_offset = dev->ppu_cfg[core_id][i].dec400_pln1_tile_status_offset + DEC400_IN_HEADER_SIZE;
      addr_t ts_pln0_addr = pp_bus_address_start + dec400_table_luma_offset;
      addr_t ts_pln1_addr = pp_bus_address_start + dec400_table_chroma_offset;

      /*configure the LSB of the luma table address*/
      reg_base[w_offset / 4] = (ts_pln0_addr & 0xFFFFFFFF);
       /*configure the MSB of the luma table address*/
      reg_base_ex[w_offset / 4] = ((u64)ts_pln0_addr >> 32);
      if ((!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) && (!dev->ppu_cfg[core_id][i].rgb)
          && dev->ppu_cfg[core_id][i].chroma_format != PP_CHROMA_422) {
        reg_base[w_offset / 4 + 1] = (ts_pln1_addr & 0xFFFFFFFF);
        /*configure the MSB of the chroma table address*/
        reg_base_ex[w_offset / 4 + 1] = ((u64)ts_pln1_addr >> 32);
      }
      w_offset += 0x8;
    }
  }

  w_offset = 0;
  for(i = 0; i < DEC_MAX_PPU_COUNT; i++) {
    if(!dev->ppu_cfg[core_id][i].enabled || !dev->ppu_cfg[core_id][i].dec400_enabled)
      continue;
    if(dev->ppu_cfg[core_id][i].enabled == 1) {
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASE] + w_offset, reg_base[w_offset / 4]);
      DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CACHE_BASEEX] + w_offset, reg_base_ex[w_offset / 4]);
      if (!(mono_chroma || dev->ppu_cfg[core_id][i].monochrome)) {
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

int  DWLDecF1Fuse(const void *instance, i32 core_id) {
  const struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  unsigned int ctl_val = 0;
  unsigned int loop_time = 500000;
  unsigned int mode = 0;
  unsigned int pic_interlace=0;
  unsigned int frame_mbs_only_flag = 0;
  u32 dec400_index = 0;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

  mode = (reg_base[3]>>27)&0x1F;
  if(mode == DEC_MODE_H264 || mode == DEC_MODE_H264_H10P) {
  /*frame_mbs_only_flag = reg_base[5] & 0x01;*/
    frame_mbs_only_flag = ((DWLReadReg(instance, core_id, 4 * 5) & 0x01) == 0);
    pic_interlace = (reg_base[3]>>23)&0x1;
    if((pic_interlace == 1) || (frame_mbs_only_flag == 0)) {
      DTRACE_D("mode=%d ,pic_interlace=%d frame_mbs_only_flag=%d,BYPASS DEC400!!!\n",mode,pic_interlace,frame_mbs_only_flag);
      return 0;
    }
  } else if (mode == DEC_MODE_MPEG4 || mode == DEC_MODE_VC1 ||
             mode == DEC_MODE_MPEG2 || mode == DEC_MODE_RV ||
             mode == DEC_MODE_AVS || mode == DEC_MODE_AVS2) {
    pic_interlace = (reg_base[3]>>23)&0x1;
    if (pic_interlace == 1) {
      DTRACE_D("mode=%d ,pic_interlace=%d ,BYPASS DEC400!!!\n",mode,pic_interlace);
      return 0;
    }
  }

  /* bypass dec400 if all PPs' dec400 disabled */
  if (!dev->ppu_cfg[core_id][0].dec400_enabled &&
      !dev->ppu_cfg[core_id][1].dec400_enabled &&
      !dev->ppu_cfg[core_id][2].dec400_enabled &&
      !dev->ppu_cfg[core_id][3].dec400_enabled &&
      !dev->ppu_cfg[core_id][4].dec400_enabled &&
      !dev->ppu_cfg[core_id][5].dec400_enabled) {
    DTRACE_D("mode=%d ,DEC400 not enabled ,BYPASS DEC400!!!\n", mode);
    return 0;
  }

#ifdef _DWL_DEBUG
  DTRACE_D("gcregAHBDECTileStatusDebug=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TILE_STATUS_DEBUG]));
  DTRACE_D("gcregAHBDECEncoderDebug=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_ENCODER_DEBUG]));
  DTRACE_D("gcregAHBDECDecoderDebug=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DECODER_DEBUG]));
  DTRACE_D("gcregAHBDECTotalReadsIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READSIN]));
  DTRACE_D("gcregAHBDECTotalWritesIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITESIN]));
  DTRACE_D("gcregAHBDECTotalReadBurstsIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READ_BURSTS_IN]));
  DTRACE_D("gcregAHBDECTotalWriteBurstsIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITE_BURSTS_IN]));
  DTRACE_D("gcregAHBDECTotalReadsReqIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READS_REQ_IN]));
  DTRACE_D("gcregAHBDECTotalWritesReqIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITES_REQ_IN]));
  DTRACE_D("gcregAHBDECTotalReadLastsIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READ_LASTS_IN]));
  DTRACE_D("gcregAHBDECTotalWriteLastsIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITE_LASTS_IN]));
  DTRACE_D("gcregAHBDECTotalWriteResponseIn=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITE_RESPONSE_IN]));
  DTRACE_D("gcregAHBDECTotalReadsOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READS_OUT]));
  DTRACE_D("gcregAHBDECTotalWritesOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITES_OUT]));
  DTRACE_D("gcregAHBDECTotalReadBurstsOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READ_BURSTS_OUT]));
  DTRACE_D("gcregAHBDECTotalWriteBurstsOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITE_BURSTS_OUT]));
  DTRACE_D("gcregAHBDECTotalReadsReqOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READS_REQ_OUT]));
  DTRACE_D("gcregAHBDECTotalWritesReqOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITES_REQ_OUT]));
  DTRACE_D("gcregAHBDECTotalReadLastsOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_READ_LASTS_OUT]));
  DTRACE_D("gcregAHBDECTotalWriteLastsOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITE_LASTS_OUT]));
  DTRACE_D("gcregAHBDECTotalWriteResponseOUT=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_TOTAL_WRITE_RESPONSE_OUT]));
  DTRACE_D("gcregAHBDECDebug0=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DEBUG0]));
  DTRACE_D("gcregAHBDECDebug1=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DEBUG1]));
  DTRACE_D("gcregAHBDECDebug2=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DEBUG2]));
  DTRACE_D("gcregAHBDECDebug3=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DEBUG3]));
  DTRACE_D("gcregAHBDECDebug4=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DEBUG4]));
  DTRACE_D("gcregAHBDECDebug5=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_DEBUG5]));
  DTRACE_D("gcregAHBDECStatus=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_STATUS]));
  DTRACE_D("gcregAHBDECDebugInfoOut=%08x\n",DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEBUG_INFO_OUT]));
#endif
  /*assure the DEC400 is idle*/
  while( ((ctl_val = DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_STATUS])) & 0x1) != 0x00000001) {
    usleep(10);
    loop_time --;
    if(loop_time == 0) return -1;
  }
  /*excute the SW flush, bit18-22 includes the 32 channels*/
  ctl_val = 0x00810101;
  DWLDec400WriteRegToHw(instance, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG], ctl_val);

#ifdef ENABLE_FPGA_VERIFICATION
  /* Fixme: sleep 1s to guarantee to get right dec400 table temporarily, it  be fixed*/
  //comments this sleep() to try to reproduce this error and fix it.
  //usleep(1000000);
#endif

  /* read and clean the DEC400 status, only need to read the register,
   * the status will be cleaned after get acknowledgeEx2 bit 0 equal 1*/
  DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ACKNOWLEDGE]);
  loop_time = 500000;
  while( (DWLDec400ReadRegFromHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_INTR_ACKNOWLEDGEEX2]) & 0x1) != 0x00000001) {
    usleep(10);
    loop_time --;
    if(loop_time == 0) return -1;
  }
  /* disable all channels */
  for (u32 i = 0; i < 32; i++) {
    DWLDec400WriteRegToHw(dec_dwl, core_id, dec400_reg_offset[dec400_index][AHBDEC_WRITE_CONFIG] + i*4, 0);
  }
  UNUSED(dec400_index);
  return 0;
}

