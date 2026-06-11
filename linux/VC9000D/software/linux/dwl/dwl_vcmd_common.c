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

#include "dwl_vcmd_common.h"
#include "dec_log.h"

/* VCMD */
/******************************************************************************/
#define OPCODE_WREG               (u32)(0x01)
#define OPCODE_END                (u32)(0x02)
#define OPCODE_NOP                (u32)(0x03)
#define OPCODE_RREG               (u32)(0x16)
#define OPCODE_INT                (u32)(0x18)
#define OPCODE_JMP                (u32)(0x19)
#define OPCODE_STALL              (u32)(0x09)
#define OPCODE_CLRINT             (u32)(0x1a)
#define OPCODE_M2M                (u32)(0x1b)

void *DWLmemcpy(void *d, const void *s, size_t n);

static void PrintInstr(u32 instr)
{
	if ((instr & 0xF8000000) == OPCODE_WREG << 27) {
		VCMDTRACE_I("current cmdbuf data = 0x%08x => [%s %s %d 0x%x]\n",
		instr, "WREG", ((instr >> 26) & 0x1) ? "FIX" : "",
		       ((instr >> 16) & 0x3FF) /*length*/, (instr & 0xFFFF));
	} else if ((instr & 0xF8000000) == OPCODE_END << 27) {
		VCMDTRACE_I("current cmdbuf data = 0x%08x => [%s]\n",
			instr, "END");

	} else if ((instr & 0xF8000000) == OPCODE_NOP << 27) {
		VCMDTRACE_I("current cmdbuf data = 0x%08x => [%s]\n",
			instr, "NOP");

	} else if ((instr & 0xF8000000) == OPCODE_RREG << 27) {

		VCMDTRACE_I("current cmdbuf data = 0x%08x => [%s %s %d 0x%x]\n",
			 instr, "RREG", ((instr >> 26) & 0x1) ? "FIX" : "",
		       ((instr >> 16) & 0x3FF) /*length*/, (instr & 0xFFFF));

	} else if ((instr & 0xF8000000) == OPCODE_JMP << 27) {
		VCMDTRACE_I("current cmdbuf data  = 0x%08x => [%s %s %s]\n",
			 instr, "JMP", ((instr >> 26) & 0x1) ? "RDY" : "",
		       ((instr >> 25) & 0x1) ? "IE" : "");

	} else if ((instr & 0xF8000000) == OPCODE_STALL << 27) {
		VCMDTRACE_I("current cmdbuf data = 0x%08x => [%s %s 0x%x]\n",
			instr, "STALL", ((instr >> 26) & 0x1) ? "IM" : "",
		       (instr & 0xFFFF));

	} else if ((instr & 0xF8000000) == OPCODE_CLRINT << 27) {
		VCMDTRACE_I("current cmdbuf data = 0x%08x => [%s %d 0x%x]\n",
			instr, "CLRINT", (instr >> 25) & 0x3,
		       (instr & 0xFFFF));

	} else if ((instr & 0xF8000000) == OPCODE_M2M << 27) {
                VCMDTRACE_I("current cmdbuf data  = 0x%08x => [%s]\n",
                         instr, "M2M");

	} else{
    VCMDTRACE_I("%s\n","ERROR OPCODE");
  }
}

void CWLCollectWriteRegData(struct VcmdBuf *vcmd, u32* src, u16 reg_start, u32 reg_length)
{
  u32 i;
  u32 *dst, *dst_base;
  {
    dst = dst_base = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++ = (OPCODE_WREG << 27) | (reg_length << 16) | (reg_start * 4);
    PrintInstr(OPCODE_WREG << 27 | (reg_length << 16)|(reg_start * 4));
    //data
#if 0
    DWLmemcpy(dst, src, reg_length*sizeof(u32));
    data_length += reg_length;
    dst += reg_length;
    //printf("CWLCollectWriteRegData: data_length= %d\n", data_length);
#else
    for(i = 0; i < reg_length; i++)
      *dst++ = *src++;
#endif
    //alignment
    if((dst - dst_base) % 2)
      *dst++ = 0;

    vcmd->cmd_buf_used += (dst - dst_base) * 4;
  }

}

void CWLCollectStallData(struct VcmdBuf *vcmd, u32 interruput_mask)
{
  u32 *dst;

  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++ = (OPCODE_STALL << 27) | (0 << 16) | (interruput_mask);
     PrintInstr((OPCODE_STALL << 27) | (0 << 16) | (interruput_mask));
    //alignment
    *dst = 0;
    vcmd->cmd_buf_used += 2 * 4;
  }
}

void CWLCollectReadRegData(struct VcmdBuf *vcmd, u16 reg_start, u32 reg_length, addr_t status_data_base_addr)
{
  u32 *dst;
  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++=(OPCODE_RREG << 27) | (reg_length << 16) | (reg_start * 4);
    //data
    *dst++=(u32)status_data_base_addr;
    PrintInstr((OPCODE_RREG << 27) | (reg_length << 16)| (reg_start * 4));
    if(sizeof(addr_t) == 8)
      *dst++ = (u32)((u64)status_data_base_addr >> 32);
    else
      *dst++ = 0;

    //alignment
    *dst = 0;
    vcmd->cmd_buf_used += 4 * 4;
  }
}


void CWLCollectNopData(struct VcmdBuf *vcmd)
{
  u32 *dst;

  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++=(OPCODE_NOP << 27) | (0);
    PrintInstr((OPCODE_NOP << 27) | (0));
    //alignment
    *dst = 0;
    vcmd->cmd_buf_used += 2 * 4;
  }
}

void CWLCollectJmpData(struct VcmdBuf *vcmd)
{
  u32 *dst;

  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++ = ((u32)OPCODE_JMP_RDY0 << 27) | (0);
    PrintInstr(((u32)OPCODE_JMP_RDY0 << 27) | (0));
    //addr
    *dst++ = (0);
    *dst++ = (0);
    *dst++ = (0);
    //alignment, do not do this step for driver will use this data to identify JMP and End opcode.
    vcmd->cmd_buf_used += 4 * 4;
  }
}

void CWLCollectEndData(struct VcmdBuf *vcmd)
{
  u32 *dst;

  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++=(OPCODE_END << 27) | (0);
    PrintInstr((OPCODE_END << 27) | (0));
    //alignment
    *dst++ = (0);
    vcmd->cmd_buf_used += 2 * 4;
  }
}

void CWLCollectClrIntData(struct VcmdBuf *vcmd, u32 clear_type,u16 interrupt_reg_addr,u32 bitmask)
{
  u32 *dst;

  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++ = (OPCODE_CLRINT << 27) | (clear_type << 25) | interrupt_reg_addr * 4;
    PrintInstr((OPCODE_CLRINT << 27) | (clear_type << 25) | interrupt_reg_addr * 4);
    //bitmask
    *dst = bitmask;
    vcmd->cmd_buf_used += 2 * 4;
  }
}


void CWLCollectM2MData(struct VcmdBuf *vcmd, addr_t dst_addr, addr_t src_addr, u32 length)
{
  u32 *dst;

  {
    dst = (u32 *)(vcmd->cmd_buf + vcmd->cmd_buf_used);
    //opcode
    *dst++ = (OPCODE_M2M << 27) | length;
    PrintInstr((OPCODE_M2M << 27) | length);
    //addr
    *dst++ = (u32)src_addr;
    if(sizeof(addr_t) == 8) {
      *dst++ = (u32)((u64)src_addr >> 32);
    } else {
      *dst++ = 0;
    }

    *dst++ = (u32)dst_addr;
    if(sizeof(addr_t) == 8)
      *dst++ = (u32)((u64)dst_addr >> 32);
    else
      *dst++ = 0;

    //alignment
    *dst = 0;

    vcmd->cmd_buf_used += 6 * 4;
  }
}
