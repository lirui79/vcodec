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
#ifndef DWL_APBFILTER_H
#define DWL_APBFILTER_H
#include "basetype.h"
#include "decapicommon.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dwlthread.h"
#include "hantrodec.h"

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

struct ApbFilterHwCfg {
  /* masked registers size */
  u32 nbr_mask_regs;
  /* number of modes, a.k.a,pages*/
  u8 num_mode;
  /* apb filter byte address to access ctrl mask bits*/
  u32 mask_reg_offset;
  /* mask bit number for each registers*/
  u8 mask_bits_per_reg;
  /* apb filter byte address to access page_sel register in this module*/
  u32 page_sel_addr;
  /*if the queried IP(core type) has apb filter*/
  u32 has_apb_filter;
};

/* A register seqment is a continuous array of register.*/
struct RegSegments {
  u32 start_index; /*starting index of the register segment*/
  u32 size; /*size in bytes*/
  u32 attr; /*R|W; 0-unaccessible, 1-WO, 2-RO, 3-RW*/
};

/*the APB filter description of a HW core*/
struct ApbFilterDesc {
  enum CoreType type; /* type of HW core in a subsystem*/
  u32 enable; /* enable or disable apb filter for this core*/
  u32 nbr_reg_segments; /*number of registers segment*/
  struct RegSegments *reg_segs; /*pointer to an array of register seqment*/
};

void DWLReadApbFilterHwCfg(void *dwl, u32 subsys_id, enum CoreType type, struct ApbFilterHwCfg *apb_filter_hw_cfg);
void DWLSetupApbFilter(void *dwl, u32 subsys_id, struct ApbFilterDesc *apb_filter, u32 page_sel);
#endif
