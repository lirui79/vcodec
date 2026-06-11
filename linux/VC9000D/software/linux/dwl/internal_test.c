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

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "internal_test.h"

#define REG_DUMP_FILE "swreg.trc"
static FILE* reg_dump = NULL;

/*------------------------------------------------------------------------------
    Function name   : InternalTestInit
    Description     : Initialize InternalTest

    Return type     : -

    Argument        : -
------------------------------------------------------------------------------*/
void InternalTestInit() {
  if (reg_dump != NULL) {
    return;
  }

  reg_dump = fopen(REG_DUMP_FILE, "a+");
  if (NULL == reg_dump) {
    DTRACE_E("struct DWL: failed to open: %s\n", REG_DUMP_FILE);
  }
}

/*------------------------------------------------------------------------------
    Function name   : InternalTestFinalize
    Description     : Cleans up InternalTest

    Return type     : -

    Argument        : -
------------------------------------------------------------------------------*/
void InternalTestFinalize() {
  if (reg_dump != NULL) {
    fclose(reg_dump);
    reg_dump = NULL;
  }
}

/*------------------------------------------------------------------------------
    Function name   : InternalTestDumpReadSwReg
    Description     :

    Return type     : -

    Argument        : i32 core_id, u32 sw_reg, u32 value, u32* sw_regs
------------------------------------------------------------------------------*/
void InternalTestDumpReadSwReg(i32 core_id, u32 sw_reg, u32 value,
                               u32* sw_regs) {
  if (core_id == 0) {
    /* status register dump */
    if (sw_reg == 1) {
      /* write out the interrupt status bit */
      u32 tmp_val = (value >> 11) & 0xFF;
      if (tmp_val & 0x02) {
        fprintf(reg_dump, "R INTERRUPT STATUS: PICTURE DECODED\n");
      } else if (tmp_val & 0x08) {
        fprintf(reg_dump, "R INTERRUPT STATUS: BUFFER EMPTY\n");
      } else if (tmp_val & 0x10) {
        fprintf(reg_dump, "R INTERRUPT STATUS: ASO DETECTED\n");
      } else if (tmp_val & 0x20) {
        fprintf(reg_dump, "R INTERRUPT STATUS: ERROR DETECTED\n");
      } else if (tmp_val & 0x40) {
        fprintf(reg_dump, "R INTERRUPT STATUS: SLICE DECODED\n");
      } else {
        fprintf(reg_dump, "R SWREG%u:          %08X\n", sw_reg, value);
      }
    }
    fflush(reg_dump);
  }
}

/*------------------------------------------------------------------------------
    Function name   : InternalTestDumpWriteSwReg
    Description     :

    Return type     : -

    Argument        : i32 core_id, u32 sw_reg, u32 value, u32* sw_regs
------------------------------------------------------------------------------*/
void InternalTestDumpWriteSwReg(i32 core_id, u32 sw_reg, u32 value,
                                u32* sw_regs) {
  if (core_id == 0) {
    fflush(reg_dump);
  }
}
