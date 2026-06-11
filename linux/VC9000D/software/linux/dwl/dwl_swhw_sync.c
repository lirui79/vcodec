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
#include "dwl_hw_core_array.h"
#include "dwl_swhw_sync.h"
#include "decapicommon.h"
#include "deccfg.h"
#include "vwl_pc.h"
#include "dec_log.h"

#if defined(__GNUC__)
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#endif

#ifdef FPGA_PERF_AND_BW
#include "dwl_perf_info.h"
#endif
#ifdef VIRTUAL_PLATFORM_TEST
#include "buffer_info.h"
#endif

#ifdef ASIC_ONL_SIM
u8 l2_allocate_buf=1;
extern int MultiStreamId;
#ifdef SUPPORT_MULTI_CORE
#include <vpu_dwl_dpi.h>
extern struct TBCfg tb_cfg;
int dwl_g_frame_num =0; /*count decoded frame number*/
extern u32* core_reg_base_array[2];
extern int cur_core_id;
extern int start_mc_load;
extern int mc_load_end;
extern int cur_pic;
extern int finished_pic;
extern u32 max_pics_decode;
#endif
#endif

#define IS_DECMODE_VP78(swreg3) ((((swreg3)>>27) == 9) ||(((swreg3)>>27) == 10))

#ifdef VIRTUAL_PLATFORM_TEST
extern void DWLVirtualPlatformTestOut(i32 core_id, Core instance);
#endif
void PrintIrqType(u32 core_id, u32 status);

void *ThreadMCListener(void *args) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)args;
  struct MCListenerThreadParams *params = &dwl_inst->sync_params;
  u16 cmdbuf_id;
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
  int* hw_core_rdy_id;
  hw_core_rdy_id = malloc(sizeof(int));
  int sw_core_rdy[2];
#endif
#endif

  while (!params->b_stopped) {
    u32 ret;
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
    u32 j;
    if (cur_pic < params->n_dec_cores) {
      for (j = 0; j < params->n_dec_cores; j++) {
        while(!start_mc_load){
          usleep(100);
        }
        start_mc_load = 0;
        if(l2_allocate_buf == 1){
          dpi_l2_update_buf_base(core_reg_base_array[cur_core_id],cur_core_id);
          l2_allocate_buf=0;
        }
        dwl_g_frame_num++;
        dpi_mc_stimulus_load(core_reg_base_array[cur_core_id],dwl_g_frame_num,cur_core_id);
        mc_load_end = 1;
      }
    }
    else if ( cur_pic < max_pics_decode){
      while(!start_mc_load){
        usleep(100);
      }
      start_mc_load = 0;
      if(l2_allocate_buf == 1){
        dpi_l2_update_buf_base(core_reg_base_array[cur_core_id],cur_core_id);
        l2_allocate_buf=0;
      }
      dwl_g_frame_num++;
      dpi_mc_stimulus_load(core_reg_base_array[cur_core_id],dwl_g_frame_num,cur_core_id);
      mc_load_end = 1;
    }
#endif
#endif

#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
    if ( cur_pic > finished_pic || (params->n_dec_cores == 1 && cur_pic < max_pics_decode)){
      dpi_wait_mc_int(hw_core_rdy_id, MultiStreamId);
      finished_pic += 1;
      Core c = GetCoreById(dwl_inst->hw_core_array, *hw_core_rdy_id);
      while(!ONL_HwCoreIsDecRdy(c)) {
        usleep(100);
      }
    }
#endif
#endif
    if (!dwl_inst->vcmd_enabled)
      ret = WaitAnyCoreRdy(dwl_inst->hw_core_array);
    else {
      cmdbuf_id = ANY_CMDBUF_ID;
      ret = CmodelIoctlWaitCmdbuf(&cmdbuf_id);
    }
    UNUSED(ret);

    if (params->b_stopped) break;

    if (!dwl_inst->vcmd_enabled) {
      u32 i;
      /* check all decoder IRQ status register */
      for (i = 0; i < params->n_dec_cores; i++) {
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
        i = *hw_core_rdy_id;
#endif
#endif
        Core c = GetCoreById(dwl_inst->hw_core_array, i);

        u32* regs = HwCoreGetBaseAddress(c);

        /* check DEC IRQ status */
        if (HwCoreIsDecRdy(c)) {

          if (regs != NULL) PrintIrqType(i, regs[1]);
          DTRACE_D("DEC IRQ by Core %d\n", i);
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
          Core c = GetCoreById(dwl_inst->hw_core_array, i);
          u32 *core_reg_base = HwCoreGetBaseAddress(c);

          dpi_mc_output_check(core_reg_base,i);
#endif
#endif
#ifdef VIRTUAL_PLATFORM_TEST
          /*TODO:support mc */
          DWLVirtualPlatformTestOut(i, c);
#endif

          if (params->callback[i] != NULL) {
            u32 *core_reg_base = HwCoreGetBaseAddress(c);
            /* Refresh dwl_shadow_regs from hw core registers. */
		        if (core_reg_base != NULL) {
              for(int k = DEC_X170_REGISTERS - 1; k >= 0; --k) {
                dwl_inst->dwl_shadow_regs[i][k] = core_reg_base[k];
              }
            }
#ifdef FPGA_PERF_AND_BW
            *(params->bytes_consumed_perf + i) = DWLGetConsumedBytes(i, params->start_address_perf, &dwl_inst->dwl_shadow_regs[0][0]);
#endif
            if (params->callback[i] != NULL) {
              params->callback[i](params->callback_arg[i], i);
            }
          } else {
            /* single core instance => post dec ready */
            HwCorePostDecRdy(c);
          }
          break;
        }
      }
    } else {
      DTRACE_D("VCMD IRQ by cmd buf %d\n", cmdbuf_id);
      u32 *status = (u32 *)(dwl_inst->vcmd[cmdbuf_id].status_buf +
                            dwl_inst->vcmd_params.submodule_main_addr);
      struct VcmdBuf* vcmd = &dwl_inst->vcmd[cmdbuf_id];

      /* VCMD multicore decoding: updated registers to vcmd[cmdbuf_id].mc_fresh_reg_mirror. */

      vcmd->mc_fresh_reg_mirror[0] = *status++; // 0-0
      vcmd->mc_fresh_reg_mirror[1] = *status++; // 1-1
      vcmd->mc_fresh_reg_mirror[2] = *status++; // 2-261
      vcmd->mc_fresh_reg_mirror[3] = *status++; // 3-270
      vcmd->mc_fresh_reg_mirror[4] = *status++; // 4-168
      vcmd->mc_fresh_reg_mirror[5] = *status++; // 5-169
      vcmd->mc_fresh_reg_mirror[6] = *status++; // 6-62
      vcmd->mc_fresh_reg_mirror[7] = *status++; // 7-63

      if (params->callback[cmdbuf_id] != NULL) {
        params->callback[cmdbuf_id](params->callback_arg[cmdbuf_id], cmdbuf_id);
      }
      sem_post(params->sc_dec_rdy_sem + cmdbuf_id);
    }
  }
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
  free(hw_core_rdy_id);
#endif
#endif

  return NULL;
}
