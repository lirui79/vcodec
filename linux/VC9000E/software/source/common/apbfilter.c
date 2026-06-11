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

#ifdef SUPPORT_APBFT
#include "apbfilter.h"

void ReadApbFilterHwCfg(struct ApbFilterHwCfg *apb_filter_hw_cfg) {
  //should get from apb filter fuse in future
  apb_filter_hw_cfg->support_page_max = 16;
  apb_filter_hw_cfg->rw_protest = 0;
  apb_filter_hw_cfg->page_sel_offset = 4;

  return;
}

i32 ConfigApbFilter(ApbFilterREG *regs, struct ApbFilterCfg *cfg) {
  u32 i;
  struct ApbFilterHwCfg apb_hw_cfg;
  struct ApbFilterMask *reg_mask;
  ApbFilterREG *reg;

  ReadApbFilterHwCfg(&apb_hw_cfg);

  for (i = 0; i < cfg->num_regs; i++) {
    reg_mask = &(cfg->reg_mask[i]);
    if (reg_mask->page_idx > apb_hw_cfg.support_page_max) return -1;

    reg = &(regs[reg_mask->reg_idx]);

    reg->value &= ~(1 << reg_mask->page_idx);
    reg->value |= reg_mask->value << reg_mask->page_idx;
    reg->offset = reg_mask->reg_idx * 4;
    reg->flag = 1;
  }

  return 0;
}

i32 SelApbFilterPage(ApbFilterREG *reg, u32 page_sel, u32 page_sel_offset) {
  u32 i;
  struct ApbFilterHwCfg apb_hw_cfg;

  ReadApbFilterHwCfg(&apb_hw_cfg);

  if (page_sel > apb_hw_cfg.support_page_max) return -1;

  reg->value = page_sel;
  reg->offset = page_sel_offset;
  reg->flag = 1;

  return 0;
}
#endif
