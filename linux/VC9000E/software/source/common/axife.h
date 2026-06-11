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
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/
#ifndef AXIFE_H
#define AXIFE_H
#include "base_type.h"
#include "vsi_string.h"
#include "osal.h"
#include "ewl.h"
#include "vcmdbuf.h"

#define AXIFE_MODE_ADDRESS 0
#define AXIFE_MODE_ID 1
#define AXIFE_MAX_CHAN_NUM 64

#ifdef AXIFE_V_3_0
  #define AXIFE_CHN_REG_ID_BASE (0x100 / 4)
  #define NSAID_CFG_REG_ID_BASE 16
  #define NSAID_SEL_REG_ID      19
#elif defined(AXIFE_V_3_2)
  #define AXIFE_CHN_REG_ID_BASE (0x400 / 4)
  #define NSAID_CFG_REG_ID_BASE 29
  #define NSAID_SEL_REG_ID      32
  #define MID_SEL_REG_ID        29
  #define IDLE_DELAY_CONFIG_REG_ID 34
  #define IDLE_OUTPUT_DELAY_CYCLES 0x0F
#else
  #define AXIFE_CHN_REG_ID_BASE  (0x40 / 4)
#endif

#define AXIFE_REG_ID_FeCtrl 10
#define AXIFE_REG_CFG_START 11
#define AXIFE_REG_CFG_END 15

#define AXIFE_REG_NUM     (AXIFE_CHN_REG_ID_BASE + AXIFE_MAX_CHAN_NUM*8)

enum AxiRegName {
#include "axife_enum.h"
  AXI_LAST_REG
};

typedef struct {
  u32 value;
  u32 offset;
  u32 flag;
} REG;

typedef struct {
  REG registers[AXIFE_REG_NUM];
} AXIFE_REG_OUT;

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

/*Description of a read/write channel */
struct ChnDesc {
  /*Address mode:high 32-bit part of the start and end address
    ID mode: the AXI ID of the address segment*/
  u32 sw_axi_base_addr_id;
  u32 sw_axi_start_addr;
  u32 sw_axi_end_addr;
  u32 sw_axi_user; /*software programed AWUSER axim*/
  u32 sw_axi_ns;   /*software programmed AWPROT_axim*/
  u32 sw_axi_qos;  /*software programmed AWQOS_axim*/
};

/* All the channels configured by sw, adding a read/write configuration
   for channels that are not specified explicitly*/
struct AxiFeChns {
  u32 nbr_rd_chns; /*nbr of configured read channels */
  u32 nbr_wr_chns; /*nbr of configured write channels*/
  struct ChnDesc rd_chns[AXIFE_MAX_CHAN_NUM];
  struct ChnDesc wr_chns[AXIFE_MAX_CHAN_NUM];
  /* The remained address segments that are not specified by the channels
     address seqment groups above, for read and write respectively*/
  u8 sw_axi_wuser; /*software programmed AWUSER_axim*/
  u8 sw_axi_wns;   /*software programmed AWPROT_axim[1]*/
  u8 sw_axi_wqos;  /*software programmed AWQOS_axim*/
  /*read*/
  u8 sw_axi_ruser;
  u8 sw_axi_rns;
  u8 sw_axi_rqos;
};

/* All the AXIFE configured by sw */
struct VCAxiFeData {
  u32 mode; /*0: disable; 1: normal mode; 2: bypass; 3.security mode*/
  VcmdDes_t *vcmd;
  const void *ewl;
  struct AxiFeChns channelCfg;
  struct AxiFeCommonCfg commonCfg;
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

void VCEncAxiFeDisable(const void *ewl, void *vcmd);
void VCEncAxiFeEnable(struct VCAxiFeData * axife_data);

#endif
