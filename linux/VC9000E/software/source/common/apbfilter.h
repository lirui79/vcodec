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

#ifndef APBFILTER_H
#define APBFILTER_H
#include "base_type.h"
#include "vsi_string.h"
#include "osal.h"

struct ApbFilterHwCfg {
  /* the max pages supported by HW*/
  u8 support_page_max;
  /* separate r/w protest support*/
  u8 rw_protest;
  /* page_sel register offset to mask register*/
  u32 page_sel_offset;
};

struct ApbFilterMask {
  /* masked registers idx */
  u32 reg_idx;
  /* mask value of each register in security mode. 0:accessible 1:denial*/
  u32 value;
  /* page idx*/
  u8 page_idx;
};

/*the APB filter config*/
struct ApbFilterCfg {
  //u32 security_mode; /* 0: non-security mode 1:security mode*/
  u32 num_regs; /*number of registers to config for protection*/
  struct ApbFilterMask *reg_mask; /*pointer to an array of mask registers*/
};

typedef struct {
  u32 value;
  u32 offset;
  u32 flag;
} ApbFilterREG;

void ReadApbFilterHwCfg(struct ApbFilterHwCfg *apb_filter_hw_cfg);
i32 ConfigApbFilter(ApbFilterREG *regs, struct ApbFilterCfg *cfg);
i32 SelApbFilterPage(ApbFilterREG *reg, u32 page_sel, u32 page_sel_offset);

#endif
