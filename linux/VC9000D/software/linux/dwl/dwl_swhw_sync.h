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

#ifndef SOFTWARE_LINUX_DWL_DWL_SWHW_SYNC_H_
#define SOFTWARE_LINUX_DWL_DWL_SWHW_SYNC_H_

#include "sw_util.h"
#include "dwlthread.h"
#include "dwl_hw_core_array.h"
#include "dwl_activity_trace.h"
#include "dwl_hw_core.h"
#include "deccfg.h"
#include "vwl_pc.h"
#include "ppu.h"

struct VcmdBuf {
  u32 core_id;  /* core allocated for this vcmd. */
  u32 client_type;
  u8 *cmd_buf;  /* pointer to cmd buffer */
  u8 *status_buf;
  addr_t status_bus_addr;  /* status offset to VCMD base */
  u32 cmd_buf_size;   /* cmd buffer bytes allocated */
  u32 cmd_buf_used;   /* bytes used in current buffer */
  u32 status;         /* DEC HW status (swreg1) after decoding */
  u32 *reg_mirror; /* point to decoder core registers-mirror array, which value are set by ctrlsw */
  u32 *mc_fresh_reg_mirror; /* Used to point mc fresh regs based the current core*/
  u32 mc_buf_id;

  PpUnitIntConfig *ppu_cfg;
  u16 *tiles;
  const void *owner; /* belong to which decoder instance */
  u32 secure_mode;
#ifdef SUPPORT_VCMD_M2M
  struct VcmdDataMvInfo vcmd_data_mv;
#endif
};

struct MCListenerThreadParams {
  int fd;
  int b_stopped;
  unsigned int n_dec_cores;
  sem_t sc_dec_rdy_sem[MAX_MC_CB_ENTRIES];
  volatile u32 *reg_base[MAX_ASIC_CORES];
  DWLIRQCallbackFn *callback[MAX_MC_CB_ENTRIES];
  void *callback_arg[MAX_MC_CB_ENTRIES];
#ifdef FPGA_PERF_AND_BW
  u32 *bytes_consumed_perf;
  addr_t *start_address_perf;
#endif
};

struct HANTRODWL {
  u32 client_type;
  u8 *free_ref_frm_mem;
  u8 *frm_base;

  u32 b_reserved_pipe;

  /* Keep track of allocated memories */
  u32 reference_total;
  u32 reference_alloc_count;
  u64 reference_maximum;
  u32 linear_total;
  i32 linear_alloc_count;

  struct MCListenerThreadParams sync_params;
  pthread_t mc_listener_thread;
  HwCoreArray hw_core_array;
  /* TODO(vmr): Get rid of temporary core "memory" mechanism. */
  Core current_core;
  struct ActivityTrace activity;
  PpUnitIntConfig *ppu_cfg[MAX_ASIC_CORES];
  u16* tiles[MAX_ASIC_CORES];
  /* counters for core usage statistics */
  u32 core_usage_counts[MAX_ASIC_CORES];
  u32 frame_count;
  u32 vcmd_enabled;
  u32 vcmd_m2m; /* vcmd support m2m */
  /* vcmd mset and m2mp*/
  u32 vcmd_m2mp;
  /* Submodule address/command buffer parameters when VCMD is used. */
  struct config_parameter vcmd_params;
  struct cmdbuf_mem_parameter vcmd_mem_params;
  struct VcmdBuf vcmd[MAX_VCMD_ENTRIES];
  u32 secure_mode[MAX_ASIC_CORES];
  u32 sim_mc; //flag only to frame level mc trace file.
  Core last_dec_core;
  pthread_mutex_t mem_mutex;
#ifdef SUPPORT_DEC400
  u32 dec400_enable[MAX_ASIC_CORES];
#endif
/* shadow HW registers */
/* Now cmodel uses similiar registers transferring path as fpga: */
/*   xxxdec_container->xxx_regs[] -> dwl_shadow_regs -> cmodel core regs */
  u32 dwl_shadow_regs[MAX_ASIC_CORES][MAX_REG_COUNT];

#ifdef FPGA_PERF_AND_BW
  u32 bytes_consumed_perf[MAX_ASIC_CORES];
  addr_t start_address_perf[MAX_ASIC_CORES];
#endif
};

void *ThreadMCListener(void *args);

#endif /* SOFTWARE_LINUX_DWL_DWL_SWHW_SYNC_H_ */
