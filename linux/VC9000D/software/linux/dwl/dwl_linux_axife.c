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
#include "dwlthread.h"
#include "dwl_vcmd_common.h"
#include "dwl_linux_axife.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dec_log.h"
#include "hantrodec_defs.h"


#ifdef __FREERTOS__
#include "hantrodec_freertos.h"
#include "memalloc_freertos.h"
#elif defined(__linux__)
#include "hantrodec.h"
#include "memalloc.h"
#else //For other os
//TODO...
#endif

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

#define AXIFE_RD 0
#define AXIFE_WR 1

#define AXIFE_MODE_ADDRESS 0
#define AXIFE_MODE_ID      1

static const u32 reg_mask[33] = {
  0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F,
  0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
  0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF, 0x0001FFFF,
  0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
  0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF,
  0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF
};

static const u32 axife_reg_spec[AXIFE_LAST_REG][4]= {
  #include "dwl_linux_axife_register.h"
};

static void AxiFeSetRegister(u32* reg_base, u32 id, u32 value) {
  u32 tmp;
  if (axife_reg_spec[id][0] >= AXIFE_CHAN_REG_NUM) {
    printf("chanel registers not use this function\n");
    return;
  }
  tmp = reg_base[axife_reg_spec[id][0]];
  tmp &= ~(reg_mask[axife_reg_spec[id][1]] << (axife_reg_spec[id][2]));
  tmp |= (value & reg_mask[axife_reg_spec[id][1]]) << (axife_reg_spec[id][2]);
  reg_base[axife_reg_spec[id][0]] = tmp;
}

static u32 AxiFeGetRegister(const u32* reg_base, u32 id) {
  u32 tmp;
  if (axife_reg_spec[id][0] >= AXIFE_CHAN_REG_NUM) {
    printf("chanel registers not use this function\n");
    return 0;
  }
  tmp = reg_base[axife_reg_spec[id][0]];
  tmp = tmp >> (axife_reg_spec[id][2]);
  tmp &= reg_mask[axife_reg_spec[id][1]];
  return tmp;
}

#if defined(AXIFE_V_3_0) || defined(AXIFE_V_3_2)
void DWLSelectAxiFeNsaid(void *dwl, u32 subsys_id, u32 secure_mode);
#endif

static void AxiFeSetChns(u32* reg_base, u32 id, u32 dir, u32 mode, struct AxiFeHwCfg* fe_hw_cfg, struct ChnDesc* chan_dec) {
  u32 offset = 0;
  /* as the spec, missing read/write channels when AXI_CHN_NUMW is different AXI_CHN_NUMR, space is saved.*/
  if (dir == AXIFE_RD) {
    if (id < fe_hw_cfg->axi_wr_chn_num)
      offset = id * 8;
    else
      offset = fe_hw_cfg->axi_wr_chn_num * 8 + (id - fe_hw_cfg->axi_wr_chn_num) * 4;
  } else {
    if (id < fe_hw_cfg->axi_rd_chn_num)
      offset = id * 8 + 4;
    else
      offset = fe_hw_cfg->axi_rd_chn_num * 8 + (id - fe_hw_cfg->axi_rd_chn_num) * 4;
  }
  /* configure the channel parameters*/
  reg_base[offset] = chan_dec->sw_axi_base_addr_id;
  /*only address mode need to configure start/end address*/
  if (mode == AXIFE_MODE_ADDRESS) {
    reg_base[offset + 1] = chan_dec->sw_axi_start_addr << 4;
    reg_base[offset + 2] = chan_dec->sw_axi_end_addr << 4;
  }
  reg_base[offset + 3] |= chan_dec->sw_axi_user;
  reg_base[offset + 3] |= (chan_dec->sw_axi_ns << 8);
  reg_base[offset + 3] |= (chan_dec->sw_axi_qos << 9);
  return;
}

void DWLReadAxiFeHwCfg(void *dwl, u32 subsys_id, struct AxiFeHwCfg* fe_hw_cfg) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct axife_cfg axife_hw_cfg;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  axife_hw_cfg.id = subsys_id;

  if (ioctl(dev->base.fd, HANTRODEC_IOC_AXIFE_CONFIG, &axife_hw_cfg)) {
      DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
      ASSERT(0);
  }

  fe_hw_cfg->axi_rd_chn_num = axife_hw_cfg.axi_rd_chn_num;
  fe_hw_cfg->axi_wr_chn_num = axife_hw_cfg.axi_wr_chn_num;
  fe_hw_cfg->axi_rd_burst_length = axife_hw_cfg.axi_rd_burst_length;
  fe_hw_cfg->axi_wr_burst_length = axife_hw_cfg.axi_wr_burst_length;
  fe_hw_cfg->fe_mode = axife_hw_cfg.fe_mode;

  return;
}

void DWLConfigAxiFe(void *dwl, u32 subsys_id, struct AxiFeCommonCfg* fe_common_cfg) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

  AxiFeSetRegister(reg_base, AXI_SW_SECURE_MODE, fe_common_cfg->sw_secure_mode);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_USER_MODE, fe_common_cfg->sw_axi_user_mode);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_ADDR_MODE, fe_common_cfg->sw_axi_addr_mode);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_PROT_MODE, fe_common_cfg->sw_axi_prot_mode);
  AxiFeSetRegister(reg_base, AXI_SW_WORK_MODE, fe_common_cfg->sw_work_mode);
  AxiFeSetRegister(reg_base, AXI_SW_SINGLE_ID_EN, fe_common_cfg->sw_single_is_enable);
  AxiFeSetRegister(reg_base, AXI_SW_RESET_REG_EN, fe_common_cfg->sw_reset_reg_enable);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_RD_ID, fe_common_cfg->sw_axi_rd_id);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_WR_ID, fe_common_cfg->sw_axi_wr_id);

  core.id = subsys_id;
  core.reg_id = 11;
  core.regs = &reg_base[core.reg_id];
  core.size = 4 * 2;
  core.type = HW_AXIFE;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

void DWLConfigAxiFeChns(void *dwl, u32 subsys_id, struct AxiFeChns *fe_chns) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;
  int ioctl_req;
  u32 i;
  u32 mode = 0;
  struct AxiFeHwCfg fe_hw_cfg;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

  DWLReadAxiFeHwCfg(dwl, subsys_id, &fe_hw_cfg);
  if (fe_chns->nbr_rd_chns > fe_hw_cfg.axi_rd_chn_num ||
      fe_chns->nbr_wr_chns > fe_hw_cfg.axi_wr_chn_num) {
    printf("the configured channel number is not enough\n");
    return;
  }
  mode = fe_hw_cfg.fe_mode;
  for (i = 0; i < fe_chns->nbr_rd_chns; i++)
    AxiFeSetChns(&reg_base[AXIFE_CHAN_REG_NUM], i, AXIFE_RD, (mode ? AXIFE_MODE_ID : AXIFE_MODE_ADDRESS), &fe_hw_cfg, &(fe_chns->rd_channels[i]));

  for (i = 0; i < fe_chns->nbr_wr_chns; i++)
    AxiFeSetChns(&reg_base[AXIFE_CHAN_REG_NUM], i, AXIFE_WR, (mode ? AXIFE_MODE_ID : AXIFE_MODE_ADDRESS), &fe_hw_cfg, &(fe_chns->wr_channels[i]));

  AxiFeSetRegister(reg_base, AXI_SW_AXI_REM_RUSER, fe_chns->sw_axi_ruser);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_REM_RNS, fe_chns->sw_axi_rns);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_REM_RQOS, fe_chns->sw_axi_rqos);

  AxiFeSetRegister(reg_base, AXI_SW_AXI_REM_WUSER, fe_chns->sw_axi_wuser);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_REM_WNS, fe_chns->sw_axi_wns);
  AxiFeSetRegister(reg_base, AXI_SW_AXI_REM_WQOS, fe_chns->sw_axi_wqos);

  ioctl_req = (int)HANTRODEC_IOCS_DEC_WRITE_REG;

  core.id = subsys_id;
  core.reg_id = 13;
  core.regs = &reg_base[core.reg_id];
  core.size = 4 * 2;
  core.type = HW_AXIFE;

  if (ioctl(dev->base.fd, ioctl_req, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }

  core.id = subsys_id;
  core.reg_id = 16;
  core.regs = &reg_base[core.reg_id];
  core.size = 4 * 4 * (fe_chns->nbr_rd_chns + fe_chns->nbr_wr_chns);
  core.type = HW_AXIFE;

  if(ioctl(dev->base.fd, ioctl_req, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

void DWLEnableAxiFe(void *dwl, u32 subsys_id, u32 mode) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

#if defined(AXIFE_V_3_0) || defined(AXIFE_V_3_2)
  DWLSelectAxiFeNsaid(dwl, subsys_id, mode);
#endif

#ifdef AXIFE_V_3_2
  /* Config delay cycles for idle output */
  AxiFeSetRegister(reg_base, AXI_SW_IDLE_LAT,
                   IDLE_OUTPUT_DELAY_CYCLES);
#endif

  if (mode) {
    AxiFeSetRegister(reg_base, AXI_SW_FRONTEND_EN, 1);
  } else {
    //AxiFeSetRegister(reg_base, AXI_SW_WORK_MODE, 1);
    AxiFeSetRegister(reg_base, AXI_SW_SECURE_MODE, 0);
    AxiFeSetRegister(reg_base, AXI_SW_FRONTEND_EN, 1);
  }

  /* for pass through mode, first wirte the mode cfg to HW*/
  if (!mode) {
    core.id = subsys_id;
    core.reg_id = 11;
    core.regs = &reg_base[core.reg_id];
    core.size = 4;
    core.type = HW_AXIFE;
    if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
      DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
      ASSERT(0);
    }
  }

  core.id = subsys_id;
  core.reg_id = 10;
  core.regs = &reg_base[core.reg_id];
  core.size = 4;
  core.type = HW_AXIFE;
  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

void DWLReadAxiFeStat(void *dwl, u32 subsys_id, struct AxiFeStat *fe_stat) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

  core.id = subsys_id;
  core.reg_id = 1;
  core.regs = &reg_base[core.reg_id];
  core.size = 4 * 9;
  core.type = HW_AXIFE;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }

  fe_stat->sw_axi_r_len_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_R_LEN_CNT);
  fe_stat->sw_axi_r_dat_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_R_DAT_CNT);
  fe_stat->sw_axi_r_req_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_R_REQ_CNT);
  fe_stat->sw_axi_rlast_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_RLAST_CNT);
  fe_stat->sw_axi_w_len_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_W_LEN_CNT);
  fe_stat->sw_axi_w_dat_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_W_DAT_CNT);
  fe_stat->sw_axi_w_req_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_W_REG_CNT);
  fe_stat->sw_axi_wlast_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_WLAST_CNT);
  fe_stat->sw_axi_w_ack_cnt = AxiFeGetRegister(reg_base, AXI_SW_AXI_W_ACK_CNT);
  return;
}

void DWLDisableAxiFe(void *dwl, u32 subsys_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

  AxiFeSetRegister(reg_base, AXI_SW_FRONTEND_EN, 0);
  AxiFeSetRegister(reg_base, AXI_SW_SOFT_RESET, 1);

  core.id = subsys_id;
  core.reg_id = 10;
  core.regs = &reg_base[core.reg_id];
  core.size = 4;
  core.type = HW_AXIFE;
  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

#ifdef FPGA_PERF_AND_BW
u64 DWLReadAxiFeBw(void *dwl, u32 subsys_id, u32 num){
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct AxiFeStat fes;
  u32 val;
  ASSERT(dec_dwl);

  DWLReadAxiFeStat(dec_dwl, subsys_id, &fes);
  if(num == 0)
    val = fes.sw_axi_r_len_cnt;
  else
    val = fes.sw_axi_w_len_cnt;
  return val;
}
#endif

void DWLResetAxiFe(void *dwl, u32 subsys_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

  AxiFeSetRegister(reg_base, AXI_SW_SOFT_RESET, 1);

  core.id = subsys_id;
  core.reg_id = 10;
  core.regs = &reg_base[core.reg_id];
  core.size = 4;
  core.type = HW_AXIFE;
  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

void DWLConfigureCmdBufForAxiFe(const void *instance, u32 cmd_buf_id, u32 mode) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];

  u32 reg_ctrl = 0x2;
  u32 reg_fecfg = 0x0;

#if defined(AXIFE_V_3_0) || defined(AXIFE_V_3_2)
  u32 nsaid_sel = (mode != 0);
#ifndef SUPPORT_AXIFE_MID
  u32 reg_nsaid_sel = (nsaid_sel << axife_reg_spec[NSAID_SEL_CORE][2]) |
                      (nsaid_sel << axife_reg_spec[NSAID_SEL_META][2]) |
                      (nsaid_sel << axife_reg_spec[NSAID_SEL_DECTS][2]);

  CWLCollectWriteRegData(vcmd, &reg_nsaid_sel,
                          dev->vcmd_params.submodule_axife_addr/4 + axife_reg_spec[NSAID_SEL_CORE][0],
                          1);
#else
  CWLCollectWriteRegData(vcmd, &nsaid_sel,
                          dev->vcmd_params.submodule_axife_addr/4 + axife_reg_spec[AXI_SW_MID_SEL][0],
                          1);
#endif
#endif

#ifdef AXIFE_V_3_2
  u32 reg_idle_cyc = IDLE_OUTPUT_DELAY_CYCLES;

  CWLCollectWriteRegData(vcmd, &reg_idle_cyc,
                         dev->vcmd_params.submodule_axife_addr/4 + axife_reg_spec[AXI_SW_IDLE_LAT][0],
                          1);
#endif

  if (mode) {
    CWLCollectWriteRegData(vcmd, &reg_ctrl,
                            dev->vcmd_params.submodule_axife_addr/4 + axife_reg_spec[AXI_SW_FRONTEND_EN][0],
                            1);
  } else {
    CWLCollectWriteRegData(vcmd, &reg_fecfg,
                            dev->vcmd_params.submodule_axife_addr/4 + axife_reg_spec[AXI_SW_SECURE_MODE][0],
                            1);
    CWLCollectWriteRegData(vcmd, &reg_ctrl,
                            dev->vcmd_params.submodule_axife_addr/4 + axife_reg_spec[AXI_SW_FRONTEND_EN][0],
                            1);
  }

}

#if defined(AXIFE_V_3_0) || defined(AXIFE_V_3_2)
void DWLSelectAxiFeNsaid(void *dwl, u32 subsys_id, u32 secure_mode) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;
  u32 reg_spec_id = 0;
  u32 nsaid_sel = (secure_mode != 0);

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

#ifdef SUPPORT_AXIFE_MID
  reg_spec_id = AXI_SW_MID_SEL;
    /* Select MID: 0 - normal mid; 1 - protect mid */
  AxiFeSetRegister(reg_base, AXI_SW_MID_SEL, nsaid_sel);
#else
  reg_spec_id = NSAID_SEL_CORE;
  AxiFeSetRegister(reg_base, NSAID_SEL_CORE, nsaid_sel);
  AxiFeSetRegister(reg_base, NSAID_SEL_META, nsaid_sel);
  AxiFeSetRegister(reg_base, NSAID_SEL_DECTS, nsaid_sel);
#endif

  core.id = subsys_id;
  core.reg_id = axife_reg_spec[reg_spec_id][0];
  core.regs = &reg_base[core.reg_id];
  core.size = 4;
  core.type = HW_AXIFE;
  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

/* Just a pseudo code to demo how to set NSAID registers, as it should be done in TrustZone */
void DWLConfigAxiFeNsaid(void *dwl, u32 subsys_id, struct AxiFeNsaidCfg* cfg) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->axife_shadow_regs[subsys_id];

  AxiFeSetRegister(reg_base, NSAID_CORE_PROTECT_VAL, cfg->core_protect_id);
  AxiFeSetRegister(reg_base, NSAID_CORE_PUBLIC_VAL, cfg->core_public_id);
  AxiFeSetRegister(reg_base, NSAID_META_PROTECT_VAL, cfg->meta_protect_id);
  AxiFeSetRegister(reg_base, NSAID_META_PUBLIC_VAL, cfg->meta_public_id);
  AxiFeSetRegister(reg_base, NSAID_DECTS_PROTECT_VAL, cfg->ts_protect_id);
  AxiFeSetRegister(reg_base, NSAID_DECTS_PUBLIC_VAL, cfg->ts_public_id);

  /*Select the public NSAID by default*/
  AxiFeSetRegister(reg_base, NSAID_SEL_CORE, 0);
  AxiFeSetRegister(reg_base, NSAID_SEL_META, 0);
  AxiFeSetRegister(reg_base, NSAID_SEL_DECTS, 0);

  core.id = subsys_id;
  core.reg_id = axife_reg_spec[NSAID_CORE_PROTECT_VAL][0];
  core.regs = &reg_base[core.reg_id];
  core.size = 4 * 4;
  core.type = HW_AXIFE;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
      DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
      ASSERT(0);
  }
}
#endif
