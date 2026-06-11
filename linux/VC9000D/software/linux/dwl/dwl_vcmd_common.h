/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2019 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : VC8000 CODEC Wrapper Layer
--
------------------------------------------------------------------------------*/
#ifndef __CWL_VC8000_VCMD_COMMON_H__
#define __CWL_VC8000_VCMD_COMMON_H__

//#include <stdio.h>
//#include <signal.h>
//#include <semaphore.h>
//#include <queue.h>
#include "basetype.h"
#ifdef MODEL_SIMULATION
#include "dwl_swhw_sync.h"
#else
#include "dwl_linux.h"
#endif

enum {
  OPCODE_WREG = 0x1,
  OPCODE_END = 0x2,
  OPCODE_NOP = 0x3,
  OPCODE_STALL = 0x9,
  OPCODE_RREG = 0x16,
  OPCODE_INT = 0x18,
  OPCODE_JMP_RDY0 = 0x19,
  OPCODE_CLRINT = 0x1A,
  OPCODE_M2M = 0x1B
};

#define CLRINT_OPTYPE_READ_WRITE_1_CLEAR         0
#define CLRINT_OPTYPE_READ_WRITE_0_CLEAR         1
#define CLRINT_OPTYPE_READ_CLEAR                 2

#define VC8000E_FRAME_RDY_INT_MASK                        0x0001
#define VC8000E_CUTREE_RDY_INT_MASK                       0x0002
#define VC8000E_DEC400_INT_MASK                           0x0004
#define VC8000E_L2CACHE_INT_MASK                          0x0008
#define VC8000E_MMU_INT_MASK                              0x0010

#define VCD_FRAME_RDY_INT_MASK               0x0100
#define VCD_DEC400_INT_MASK_1_0_C            0x0400
#define VCD_L2CACHE_INT_MASK_1_0_C           0x0800
#define VCD_MMU_INT_MASK_1_0_C               0x1000

#define VCD_DEC400_INT_MASK                  0x0200
#define VCD_L2CACHE_INT_MASK                 0x0400
#define VCD_MMU_INT_MASK                     0x0800

void CWLCollectWriteRegData(struct VcmdBuf *vcmd, u32* src, u16 reg_start, u32 reg_length);
void CWLCollectStallData(struct VcmdBuf *vcmd, u32 interruput_mask);
void CWLCollectReadRegData(struct VcmdBuf *vcmd, u16 reg_start, u32 reg_length, addr_t status_data_base_addr);
void CWLCollectNopData(struct VcmdBuf *vcmd);
void CWLCollectJmpData(struct VcmdBuf *vcmd);
void CWLCollectClrIntData(struct VcmdBuf *vcmd, u32 clear_type,u16 interrupt_reg_addr,u32 bitmask);
void CWLCollectM2MData(struct VcmdBuf *vcmd, addr_t dst_addr, addr_t src_addr, u32 length);
void CWLCollectEndData(struct VcmdBuf *vcmd);

#endif


