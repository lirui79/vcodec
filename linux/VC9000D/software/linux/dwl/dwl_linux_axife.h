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
#ifndef DWL_AXIFE
#define DWL_AXIFE
#include "basetype.h"
#include "decapicommon.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dwlthread.h"

#ifdef __FREERTOS__
//nothing
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#else //For other os
//TODO...
#endif

enum AxiRegName {
#include "dwl_linux_axife_enum.h"
AXIFE_LAST_REG
};

#define AXIFE_MAX_CHAN_NUM 64

#ifdef AXIFE_V_3_0
  #define AXIFE_CHN_REG_BASE    0x100
  #define NSAID_CFG_REG_ID_BASE 16
  #define NSAID_SEL_REG_ID      19
#elif defined(AXIFE_V_3_2)
  #define AXIFE_CHN_REG_BASE     0x400
  #define NSAID_CFG_REG_ID_BASE  29
  #define NSAID_SEL_REG_ID       32
  #define IDLE_OUTPUT_DELAY_CYCLES 0x0F
#else
  #define AXIFE_CHN_REG_BASE  0x40
#endif

#define AXIFE_CHAN_REG_NUM (AXIFE_CHN_REG_BASE / 4)

#define AXIFE_REG_NUM     (AXIFE_CHAN_REG_NUM + AXIFE_MAX_CHAN_NUM*8)

/* AXI FE configure */
struct AxiFeHwCfg {
  /* Read channel number by hardware configured named AXI_CHN_NUMR */
  u8 axi_rd_chn_num;
  /* Write channel number by hardware configured named AXI_CHN_NUMW */
  u8 axi_wr_chn_num;
  /* Read BurstLength by hardware configured MASTER_BLR(unit:SYS_DATA_WIDTH,default:16bytes)*/
  u8 axi_rd_burst_length;
  /* Write BurstLength by hardware configured MASTER_BLW(unit:SYS_DATA_WIDTH,default:16bytes)*/
  u8 axi_wr_burst_length;
  /* work_mode(ID or addr) configured by hardware*/
  u8 fe_mode;
};

struct AxiFeCommonCfg {
  /* secure mode flag to set APORT_axim[1]*/
  u32 sw_secure_mode;
  u32 sw_axi_user_mode;
  u32 sw_axi_addr_mode;
  u8 sw_axi_prot_mode;
  u8 sw_work_mode;
  /* this control is only valid when workMode == "pass through" */
  u8 sw_single_is_enable;
  /* when hot reset, whether reset software registers start from reg11*/
  u8 sw_reset_reg_enable;
  /* base ID for master read bursts transaction*/
  u8 sw_axi_rd_id;
  /* base ID for master write bursts transcation */
  u8 sw_axi_wr_id;
};


struct AxiFeNsaidCfg {
  /* NSAID config */
  u8 core_public_id;
  u8 core_protect_id;
  u8 meta_public_id;
  u8 meta_protect_id;
  u8 ts_public_id;
  u8 ts_protect_id;
};

/*Description of a read/write channel */
struct ChnDesc {
  /*Address mode:high 32-bit part of the start and end address
    ID mode: the AXI ID of the address segment*/
  u32 sw_axi_base_addr_id;
  u32 sw_axi_start_addr;
  u32 sw_axi_end_addr;
  u32 sw_axi_user; /*software programed AWUSER axim*/
  u32 sw_axi_ns; /*software programmed AWPROT_axim*/
  u32 sw_axi_qos; /*software programmed AWQOS_axim*/
};

/* All the channels configured by sw, adding a read/write configuration
   for channels that are not specified explicitly*/
struct AxiFeChns {
  u32 nbr_rd_chns; /*nbr of configured read channels */
  u32 nbr_wr_chns; /*nbr of configured write channels*/
  struct ChnDesc *rd_channels;
  struct ChnDesc *wr_channels;
  /* The remained address segments that are not specified by the channels
     address seqment groups above, for read and write respectively*/
  u8 sw_axi_wuser; /*software programmed AWUSER_axim*/
  u8 sw_axi_wns; /*software programmed AWPROT_axim[1]*/
  u8 sw_axi_wqos; /*software programmed AWQOS_axim*/
  /*read*/
  u8 sw_axi_ruser;
  u8 sw_axi_rns;
  u8 sw_axi_rqos;
};

/*Statistics from AXI FE*/
struct AxiFeStat {
  /*AXI read burst length accumulator for current frame (ARLEN+1)*/
  u32 sw_axi_r_len_cnt;
  /*AXI data read cycle accumulator for current frame*/
  u32 sw_axi_r_dat_cnt;
  /*AXI read requests accumulator for current frame*/
  u32 sw_axi_r_req_cnt;
  /* Accumulator for AXI read last data of each burst for current frame*/
  u32 sw_axi_rlast_cnt;
  /*AXI write burst length accumulator for current frame*/
  u32 sw_axi_w_len_cnt;
  /*AXI data write cycle accumulator for current frame*/
  u32 sw_axi_w_dat_cnt;
  /*AXI read requests accumulator for current frame*/
  u32 sw_axi_w_req_cnt;
  /*Accumulator for AXI write last data of each burst for current frame*/
  u32 sw_axi_wlast_cnt;
  /*AXI write responses accumulators for current frame*/
  u32 sw_axi_w_ack_cnt;
};

void DWLReadAxiFeHwCfg(void *dwl, u32 subsys_id, struct AxiFeHwCfg *fe_hw_cfg);
void DWLConfigAxiFe(void *dwl, u32 subsys_id, struct AxiFeCommonCfg *fe_common_cfg);
void DWLConfigAxiFeChns(void *dwl, u32 subsys_id, struct AxiFeChns *fe_chns);
void DWLEnableAxiFe(void *dwl, u32 subsys_id, u32 mode);
void DWLReadAxiFeStat(void *dwl, u32 subsys_id, struct AxiFeStat *fe_stat);
void DWLDisableAxiFe(void *dwl, u32 subsys_id);
void DWLResetAxiFe(void *dwl, u32 subsys_id);
void DWLConfigureCmdBufForAxiFe(const void *instance, u32 cmd_buf_id, u32 mode) ;
#ifdef FPGA_PERF_AND_BW
u64 DWLReadAxiFeBw(void *dwl, u32 subsys_id, u32 num);
#endif

#endif
