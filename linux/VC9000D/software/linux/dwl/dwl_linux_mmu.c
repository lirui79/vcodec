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
#include "dwl_linux_mmu.h"
#include "dwl_linux.h"
#include "dwl.h"
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

void DWLFlushCmdBufForMMU(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];
  u32 enable = 0x10;
  u32 disable = 0x00;
#ifdef SUPPORT_48PA_MMU
  u32 flushcmd_data = 0x80000000; // bit31 is mmuReg_flushCmd, bit[3:0] is mmuReg_flushId

  CWLCollectWriteRegData(vcmd, &flushcmd_data, (dev->vcmd_params.submodule_MMU_addr + MMU_REG_PTFLUSH) / 4, 1);
#else

  CWLCollectWriteRegData(vcmd, &enable, (dev->vcmd_params.submodule_MMU_addr + MMU_REG_FLUSH) / 4, 1);
  CWLCollectWriteRegData(vcmd, &disable, (dev->vcmd_params.submodule_MMU_addr + MMU_REG_FLUSH) / 4, 1);
#endif


  if (dev->vcmd_params.submodule_MMUWrite_addr != 0xffff) {
    CWLCollectWriteRegData(vcmd, &enable,
                            (dev->vcmd_params.submodule_MMUWrite_addr + MMU_REG_FLUSH) / 4,
                            1);
    CWLCollectWriteRegData(vcmd, &disable,
                            (dev->vcmd_params.submodule_MMUWrite_addr + MMU_REG_FLUSH) / 4,
                            1);
  }
}

#ifdef SUPPORT_48PA_MMU
void DWLSwitchMMUPageTableByCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];
  struct page_table_switch params;
  u32 i, page_table_id;

  if (ioctl(dev->base.fd, (int)HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF, &params) != 0)
    return;
  if (params.id < 0)
    return;
  if (params.pt_flush.flush_cnt > 0) {
    for (i = 0; i < params.pt_flush.flush_cnt; i++)
       CWLCollectWriteRegData(vcmd, &params.pt_flush.flush_vmid[i], (dev->vcmd_params.submodule_MMU_addr + MMU_REG_PTFLUSH) / 4, 1);
  }
  page_table_id = params.id;
  CWLCollectWriteRegData(vcmd, &page_table_id, (dev->vcmd_params.submodule_MMU_addr + MMU_REG_PAGE_TABLE_ID) / 4, 1);

  if (dev->vcmd_params.submodule_MMUWrite_addr != 0xffff) {
    if (params.pt_flush.flush_cnt > 0) {
      for (i = 0; i < params.pt_flush.flush_cnt; i++)
         CWLCollectWriteRegData(vcmd, &params.pt_flush.flush_vmid[i], (dev->vcmd_params.submodule_MMUWrite_addr + MMU_REG_PTFLUSH) / 4, 1);
    }
    CWLCollectWriteRegData(vcmd, &page_table_id, (dev->vcmd_params.submodule_MMUWrite_addr + MMU_REG_PAGE_TABLE_ID) / 4, 1);
  }
}
#endif

