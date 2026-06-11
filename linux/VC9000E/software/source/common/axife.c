/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#ifdef SUPPORT_AXIFE

#include "axife.h"

#define AXIFE_RD 0
#define AXIFE_WR 1

static const u32 reg_mask[33] = {
    0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F,
    0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
    0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF, 0x0001FFFF,
    0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
    0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF,
    0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF};

static const u32 axife_reg_spec[AXI_LAST_REG][4] = {
#include "axife_register.h"
};

#define AXI_CHAN_REG_NUM 16
static u32 axife_client[2] = {EWL_CLIENT_TYPE_AXIFE, EWL_CLIENT_TYPE_AXIFE_1};

static void AxiFeWriteRegister(const void *inst, REG *reg, VcmdDes_t *vcmd) {
  u32 i;

  if (reg->flag == 0) {
    return;
  }

  if (vcmd) {
#ifdef VCMD_BUILD_SUPPORT
    VcmdbufCollectWriteAxiFeRegData(inst, vcmd, &reg->value, reg->offset, 1);
#endif
  } else {
    for (i=0; i<2; i++) {
        EWLWriteRegbyClientType(inst, reg->offset, reg->value, axife_client[i]);
    }
  }
}

static void AxiFeWriteChnRegs(const void *inst,
                              REG *chn_regs, u32 reg_num, VcmdDes_t *vcmd) {
  u32 i;

  if (vcmd == NULL) {
    for (i = 0; i < reg_num; i++) {
        AxiFeWriteRegister(inst, &chn_regs[i], NULL);
    }
    return;
  }

  //vcmd mode
#ifdef VCMD_BUILD_SUPPORT
  if (chn_regs[0].flag) {
    u32 _val[AXIFE_MAX_CHAN_NUM * 8];

    for (i=0; i<reg_num; i++) {
      _val[i] = chn_regs[i].value;
    }

    VcmdbufCollectWriteAxiFeRegData(inst, vcmd,
                                    _val,
                                    AXIFE_CHN_REG_ID_BASE * 4,
                                    reg_num);

  }
#endif
}

static void AxiFeSetRegister(REG *reg_base, u32 id, u32 value) {
  u32 tmp;
  if (axife_reg_spec[id][0] >= AXI_CHAN_REG_NUM) {
    printf("chanel registers not use this function\n");
    return;
  }
  tmp = reg_base[axife_reg_spec[id][0]].value;
  tmp &= ~(reg_mask[axife_reg_spec[id][1]] << (axife_reg_spec[id][2]));
  tmp |= (value & reg_mask[axife_reg_spec[id][1]]) << (axife_reg_spec[id][2]);
  reg_base[axife_reg_spec[id][0]].value = tmp;
  reg_base[axife_reg_spec[id][0]].offset = axife_reg_spec[id][0] * 4;
  reg_base[axife_reg_spec[id][0]].flag = 1;
}

static u32 AxiFeGetRegister(REG *reg_base, u32 id) {
  u32 tmp;
  if (axife_reg_spec[id][0] >= AXI_CHAN_REG_NUM) {
    printf("chanel registers not use this function\n");
    return 0;
  }
  tmp = reg_base[axife_reg_spec[id][0]].value;
  tmp = tmp >> (axife_reg_spec[id][2]);
  tmp &= reg_mask[axife_reg_spec[id][1]];
  return tmp;
}

static void AxiSetChns(REG *reg_base, u32 id, u32 dir, u32 mode,
                       struct AxiFeHwCfg *fe_hw_cfg, struct ChnDesc *chan_dec) {
  u32 offset = 0;
  /* as the spec, missing read/write channels when AXI_CHN_NUMW is different AXI_CHN_NUMR, space is saved.*/
  if (dir == AXIFE_RD) {
    if (id < fe_hw_cfg->axi_wr_chn_num)
      offset = id * 8;
    else
      offset =
          fe_hw_cfg->axi_wr_chn_num * 8 + (id - fe_hw_cfg->axi_wr_chn_num) * 4;
  } else {
    if (id < fe_hw_cfg->axi_rd_chn_num)
      offset = id * 8 + 4;
    else
      offset =
          fe_hw_cfg->axi_rd_chn_num * 8 + (id - fe_hw_cfg->axi_rd_chn_num) * 4;
  }
  /* configure the channel parameters*/
  reg_base[offset].value = chan_dec->sw_axi_base_addr_id;
  reg_base[offset].offset = offset * 4;
  reg_base[offset].flag = 1;
  /* Always set start/end address to be compatible with VCMD mode.
     Although they are valid only when mode euqals to AXIFE_MODE_ADDRES*/
  reg_base[offset + 1].value = chan_dec->sw_axi_start_addr << 4;
  reg_base[offset + 1].offset = (offset + 1) * 4;
  reg_base[offset + 1].flag = 1;
  reg_base[offset + 2].value = chan_dec->sw_axi_end_addr << 4;
  reg_base[offset + 2].offset = (offset + 2) * 4;
  reg_base[offset + 2].flag = 1;

  reg_base[offset + 3].value |= chan_dec->sw_axi_user;
  reg_base[offset + 3].value |= (chan_dec->sw_axi_ns << 8);
  reg_base[offset + 3].value |= (chan_dec->sw_axi_qos << 9);
  reg_base[offset + 3].offset = (offset + 3) * 4;
  reg_base[offset + 3].flag = 1;
  return;
}

void ParseAxiFeHwCfg(u32 axiFeFuse, struct AxiFeHwCfg *fe_hw_cfg) {
  fe_hw_cfg->axi_rd_chn_num = axiFeFuse & 0x7F;
  ASSERT(fe_hw_cfg->axi_rd_chn_num <= AXIFE_MAX_CHAN_NUM);
  fe_hw_cfg->axi_wr_chn_num = (axiFeFuse >> 7) & 0x7F;
  ASSERT(fe_hw_cfg->axi_wr_chn_num <= AXIFE_MAX_CHAN_NUM);
  fe_hw_cfg->axi_rd_burst_length = (axiFeFuse >> 14) & 0x1F;
  fe_hw_cfg->axi_wr_burst_length = (axiFeFuse >> 22) & 0x1F;
  fe_hw_cfg->fe_mode = 0;  //addr mode

  return;
}

void ConfigAxiFe(REG *axife_shadow_regs, struct AxiFeCommonCfg *fe_common_cfg) {
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_SECURE_MODE,
                   fe_common_cfg->sw_secure_mode);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_USER_MODE,
                   fe_common_cfg->sw_axi_user_mode);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_ADDR_MODE,
                   fe_common_cfg->sw_axi_addr_mode);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_PROT_MODE,
                   fe_common_cfg->sw_axi_prot_mode);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_WORK_MODE,
                   fe_common_cfg->sw_work_mode);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_SINGLE_ID_EN,
                   fe_common_cfg->sw_single_is_enable);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_RESET_REG_EN,
                   fe_common_cfg->sw_reset_reg_enable);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_RD_ID,
                   fe_common_cfg->sw_axi_rd_id);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_WR_ID,
                   fe_common_cfg->sw_axi_wr_id);


#if defined(AXIFE_V_3_0) || defined(AXIFE_V_3_2)
  u32 nsaid_sel = (fe_common_cfg->sw_secure_mode != 0);

#ifndef SUPPORT_AXIFE_MID
  AxiFeSetRegister(axife_shadow_regs, NSAID_SEL_CORE, nsaid_sel);
  AxiFeSetRegister(axife_shadow_regs, NSAID_SEL_META, nsaid_sel);
  AxiFeSetRegister(axife_shadow_regs, NSAID_SEL_DECTS, nsaid_sel);
#else
  /* Select MID: 0 - normal mid; 1 - protect mid */
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_MID_SEL, nsaid_sel);
#endif /* SUPPORT_AXIFE_MID */
#endif

#ifdef AXIFE_V_3_2
  /* Config delay cycles for idle output */
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_IDLE_LAT,
                   IDLE_OUTPUT_DELAY_CYCLES);
#endif
}

void ConfigAxiFeChns(REG *axife_shadow_regs, struct AxiFeHwCfg *fe_hw_cfg,
                     struct AxiFeChns *fe_chns) {
  u32 i;
  u32 mode = 0;

  if (fe_chns->nbr_rd_chns > fe_hw_cfg->axi_rd_chn_num ||
      fe_chns->nbr_wr_chns > fe_hw_cfg->axi_wr_chn_num) {
    printf("the configured channel number is not enough\n");
    return;
  }
  mode = fe_hw_cfg->fe_mode;
  for (i = 0; i < fe_chns->nbr_rd_chns; i++)
    AxiSetChns(axife_shadow_regs, i, AXIFE_RD,
               (mode ? AXIFE_MODE_ID : AXIFE_MODE_ADDRESS), fe_hw_cfg,
               &(fe_chns->rd_chns[i]));

  for (i = 0; i < fe_chns->nbr_wr_chns; i++)
    AxiSetChns(axife_shadow_regs, i, AXIFE_WR,
               (mode ? AXIFE_MODE_ID : AXIFE_MODE_ADDRESS), fe_hw_cfg,
               &(fe_chns->wr_chns[i]));

  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_REM_RUSER,
                   fe_chns->sw_axi_ruser);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_REM_RNS, fe_chns->sw_axi_rns);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_REM_RQOS,
                   fe_chns->sw_axi_rqos);

  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_REM_WUSER,
                   fe_chns->sw_axi_wuser);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_REM_WNS, fe_chns->sw_axi_wns);
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_AXI_REM_WQOS,
                   fe_chns->sw_axi_wqos);
}

void EnableAxiFe(const void *ewl, struct AxiFeHwCfg *fe_hw_cfg,
                  REG *axife_shadow_regs, VcmdDes_t *vcmd, u32 byPass) {
  u32 i;

  if (byPass) {
    AxiFeSetRegister(axife_shadow_regs, AXI_SW_WORK_MODE, 1);
    AxiFeSetRegister(axife_shadow_regs, AXI_SW_FRONTEND_EN, 1);
  } else {
    AxiFeSetRegister(axife_shadow_regs, AXI_SW_WORK_MODE, 0);
    AxiFeSetRegister(axife_shadow_regs, AXI_SW_FRONTEND_EN, 1);
  }

  //write basic cfg registers to AXIFE
  for (i = AXIFE_REG_CFG_START; i <= AXIFE_REG_CFG_END; i++) {
    AxiFeWriteRegister(ewl, &axife_shadow_regs[i], vcmd);
  }

  //write channel cfg registers to AXIFE
  AxiFeWriteChnRegs(ewl,
                    &axife_shadow_regs[AXIFE_CHN_REG_ID_BASE],
                    (fe_hw_cfg->axi_rd_chn_num + fe_hw_cfg->axi_wr_chn_num) * 4,
                    vcmd);

#if defined(AXIFE_V_3_0) || defined(AXIFE_V_3_2)
#ifndef SUPPORT_AXIFE_MID
  //write NSAID select registers to AXIFE
  AxiFeWriteRegister(ewl, &axife_shadow_regs[NSAID_SEL_REG_ID], vcmd);
#else
  //write MID select registers to AXIFE
  AxiFeWriteRegister(ewl, &axife_shadow_regs[MID_SEL_REG_ID], vcmd);
#endif /* SUPPORT_AXIFE_MID */
#endif

#ifdef AXIFE_V_3_2
  //write IDEL Delay Configure registers to AXIFE
  AxiFeWriteRegister(ewl, &axife_shadow_regs[IDLE_DELAY_CONFIG_REG_ID], vcmd);
#endif

  //finally set enable register
  AxiFeWriteRegister(ewl, &axife_shadow_regs[AXIFE_REG_ID_FeCtrl], vcmd);
}

void ReadAxiFeStat(REG *axife_shadow_regs, struct AxiFeStat *fe_stat) {
  fe_stat->sw_axi_r_len_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_R_LEN_CNT);
  fe_stat->sw_axi_r_dat_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_R_DAT_CNT);
  fe_stat->sw_axi_r_req_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_R_REQ_CNT);
  fe_stat->sw_axi_rlast_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_RLAST_CNT);
  fe_stat->sw_axi_w_len_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_W_LEN_CNT);
  fe_stat->sw_axi_w_dat_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_W_DAT_CNT);
  fe_stat->sw_axi_w_req_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_W_REG_CNT);
  fe_stat->sw_axi_wlast_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_WLAST_CNT);
  fe_stat->sw_axi_w_ack_cnt =
      AxiFeGetRegister(axife_shadow_regs, AXI_SW_AXI_W_ACK_CNT);
  return;
}

void ResetAxiFe(REG *axife_shadow_regs) {
  AxiFeSetRegister(axife_shadow_regs, AXI_SW_SOFT_RESET, 1);
}

/* Enable AxiFe */
void VCEncAxiFeEnable(struct VCAxiFeData * axife_data) {
#ifdef SUPPORT_AXIFE
  u32 axiFeFuse;
  struct AxiFeHwCfg fe_hw_cfg = {0, };
  AXIFE_REG_OUT regs;
  u32 i;
  REG *reg = &(regs.registers[0]);

  if (axife_data->mode == 0) return;

  axiFeFuse = EWLGetConfigRegister(axife_data->ewl, 0, EWL_CLIENT_TYPE_AXIFE, 0);

  ParseAxiFeHwCfg(axiFeFuse, &fe_hw_cfg);

  memset(reg, 0, sizeof(AXIFE_REG_OUT));
  if (axife_data->mode == 3) {
    ConfigAxiFeChns(reg, &fe_hw_cfg, &axife_data->channelCfg);

    ConfigAxiFe(reg, &axife_data->commonCfg);
  }

  EnableAxiFe(axife_data->ewl, &fe_hw_cfg, reg,
              axife_data->vcmd, axife_data->mode == 2);

#endif
}

/* Disable AxiFe */
void VCEncAxiFeDisable(const void *ewl, void *vcmd) {
#ifdef SUPPORT_AXIFE
#if 0
  AXIFE_REG_OUT regs = {0, };

  if (vcmd) {
    //AXIFE can't be disabled as VCMD needs to access memory.
    AxiFeSetRegister(regs.registers, AXI_SW_WORK_MODE, 0);
    AxiFeWriteRegister(ewl, &regs.registers[AXIFE_REG_CFG_START], vcmd);
  } else {
    AxiFeSetRegister(regs.registers, AXI_SW_FRONTEND_EN, 0);
    AxiFeWriteRegister(ewl, &regs.registers[AXIFE_REG_ID_FeCtrl], vcmd);
  }
#endif
#endif
}

#endif
