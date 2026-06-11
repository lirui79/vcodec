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
#include "dwl_perf_info.h"



#ifdef FPGA_PERF_AND_BW
addr_t DWLReadStartAddress(i32 core_id, u32 *dwl_shadow_regs) {
  u32 start_offset;
  addr_t start_addr;

  start_addr = *(dwl_shadow_regs + core_id * MAX_REG_COUNT + 169);
  if (sizeof(addr_t) == 8)
    start_addr |= ((u64)*(dwl_shadow_regs + core_id * MAX_REG_COUNT + 168)) << 32;
  start_offset = *(dwl_shadow_regs + core_id * MAX_REG_COUNT + 259);
  start_addr += start_offset;

  return start_addr;
}

u32 DWLGetConsumedBytes(i32 core_id, addr_t *start_address, u32 *dwl_shadow_regs) {
  addr_t last_read_address;
  u32 hw_buffer_len, bytes_consumed;

  hw_buffer_len = *(dwl_shadow_regs + core_id * MAX_REG_COUNT + 258);
  last_read_address = *(dwl_shadow_regs + core_id * MAX_REG_COUNT + 169);
  if (sizeof(addr_t) == 8)
    last_read_address |= ((u64)*(dwl_shadow_regs + core_id * MAX_REG_COUNT + 168)) << 32;

  if( *(start_address + core_id) > last_read_address)
    bytes_consumed = hw_buffer_len - (u32)(*(start_address + core_id) - last_read_address);
  else
    bytes_consumed = (u32)(last_read_address - *(start_address + core_id));
  return bytes_consumed;
}
#endif
