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
#include "dwl_linux_apbfilter.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dec_log.h"
#include "hantrodec_defs.h"
#ifdef __linux__
#include "hantrodec.h"
#include "memalloc.h"
#elif defined(__FREERTOS__)
#include "hantrodec_freertos.h"
#include "memalloc_freertos.h"
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

static u32 apb_filter_reg[MAX_ASIC_CORES][2048];

void DWLReadApbFilterHwCfg(void *dwl, u32 subsys_id, enum CoreType type, struct ApbFilterHwCfg *apb_filter_hw_cfg) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct apbfilter_cfg apb_filter_cfg;
  int ioctl_req;

  assert(dec_dwl);

  ioctl_req = (int)HANTRODEC_IOC_APBFILTER_CONFIG;

  pthread_mutex_lock(&dec_dwl->shadow_mutex[subsys_id]);

  apb_filter_cfg.id = subsys_id;
  apb_filter_cfg.type = type;

  if(ioctl(dec_dwl->fd, ioctl_req, &apb_filter_cfg))
  {
      DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
      assert(0);
  }
  pthread_mutex_unlock(&dec_dwl->shadow_mutex[subsys_id]);

  apb_filter_hw_cfg->nbr_mask_regs = apb_filter_cfg.nbr_mask_regs;
  apb_filter_hw_cfg->num_mode = apb_filter_cfg.num_mode;
  apb_filter_hw_cfg->mask_reg_offset = apb_filter_cfg.mask_reg_offset;
  apb_filter_hw_cfg->mask_bits_per_reg = apb_filter_cfg.mask_bits_per_reg;
  apb_filter_hw_cfg->page_sel_addr = apb_filter_cfg.page_sel_addr;
  apb_filter_hw_cfg->has_apb_filter = apb_filter_cfg.has_apbfilter;

  return;
}

void DWLSetupApbFilter(void *dwl, u32 subsys_id, struct ApbFilterDesc *apb_filter, u32 page_sel) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) dwl;
  struct core_desc core;
  struct ApbFilterHwCfg apb_filter_hw_cfg;
  int ioctl_req;
  u32 i, j;

  assert(dec_dwl);

  if (!apb_filter->enable)
    return;

  DWLReadApbFilterHwCfg(dec_dwl, subsys_id, apb_filter->type, &apb_filter_hw_cfg);

  if (!apb_filter_hw_cfg.has_apb_filter)
    return;

  if (page_sel >= apb_filter_hw_cfg.num_mode)
    return; /*can only support apb_filter_hw_cfg.num_mode pase_sel setting*/

  /*configure the reg RW mode*/
  for (i = 0; i < apb_filter->nbr_reg_segments; i++) {
    for (j = 0; j < apb_filter->reg_segs[i].size / 4; j++) {
      if (apb_filter->reg_segs[i].start_index + j >= apb_filter_hw_cfg.nbr_mask_regs) {
        return;/*reg number overflow*/
      }
      apb_filter_reg[subsys_id][apb_filter->reg_segs[i].start_index + j] = apb_filter->reg_segs[i].attr;
    }
  }
  /*configure page_sel*/
  apb_filter_reg[subsys_id][apb_filter_hw_cfg.nbr_mask_regs + 1] = page_sel;

  ioctl_req = (int)HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG;
  pthread_mutex_lock(&dec_dwl->shadow_mutex[subsys_id]);

  core.id = subsys_id;
  core.reg_id = 0;
  core.regs = &apb_filter_reg[core.id][core.reg_id];
  core.size = 4 * (apb_filter_hw_cfg.nbr_mask_regs + 1);
  core.type = apb_filter->type;
  if(ioctl(dec_dwl->fd, ioctl_req, &core))
  {
    DTRACE_D("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    assert(0);
  }
  pthread_mutex_unlock(&dec_dwl->shadow_mutex[subsys_id]);
}
