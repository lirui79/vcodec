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
--  Abstract : VCMD Driver for Encoder
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "osal.h"
#include "ewl.h"
#include "vcmdbuf.h"
//#include "vcx_driver.h"
#include "encswhwregisters.h"

/** VCMD COMMAND OPCODE */
#define OPCODE_WREG               ((u32)0x01 << 27)
#define OPCODE_END                ((u32)0x02 << 27)
#define OPCODE_NOP                ((u32)0x03 << 27)
#define OPCODE_RREG               ((u32)0x16 << 27)
#define OPCODE_INT                ((u32)0x18 << 27)
#define OPCODE_JMP                ((u32)0x19 << 27)
#define OPCODE_STALL              ((u32)0x09 << 27)
#define OPCODE_CLRINT             ((u32)0x1a << 27)
#define OPCODE_JMP_RDY0           ((u32)0x19 << 27)
#define OPCODE_JMP_RDY1           (((u32)0x19 << 27) | ((u32)1 << 26))
#define OPCODE_M2REG               ((u32)0x1E << 27)

#define JMP_IE_1                  ((u32)1<<25)
#define JMP_RDY_1                 ((u32)1<<26)

#define CLRINT_OPTYPE_READ_WRITE_1_CLEAR         0
#define CLRINT_OPTYPE_READ_WRITE_0_CLEAR         1
#define CLRINT_OPTYPE_READ_CLEAR                 2

#define VCE_FRAME_RDY_INT_MASK               0x0001
#define VCE_CUTREE_RDY_INT_MASK              0x0002
#define VCE_DEC400_INT_MASK                  0x0004
#define VCE_L2CACHE_INT_MASK                 0x0008
#define VCE_MMU_INT_MASK                     0x0010
#define CUTREE_MMU_INT_MASK                      0x0020

#define VCD_FRAME_RDY_INT_MASK               0x0100
#define VCD_DEC400_INT_MASK                  0x0400
#define VCD_L2CACHE_INT_MASK                 0x0800
#define VCD_MMU_INT_MASK                     0x1000

#define VCD_DEC400_INT_MASK_1_1_1            0x0200
#define VCD_L2CACHE_INT_MASK_1_1_1           0x0400
#define VCD_MMU_INT_MASK_1_1_1               0x0800
#define VCE_UFBC_INT_MASK                    0x1000

static void CWLCollectWriteRegData(VcmdDes_t *vcmd, u32* src,u16 reg_start,
                            u32 reg_length) {
  u32 *dst, *dst_base;

  {
    dst = dst_base = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_WREG | (reg_length << 16) | (reg_start * 4);
    //data
    EWLmemcpy(dst, src, reg_length * sizeof(u32));
    dst += reg_length;

    //alignment
    if ((dst - dst_base) % 2)
      *dst++ = 0;

    vcmd->vcmdBufSize += (dst - dst_base);
  }
}

static void CWLCollectStallData(VcmdDes_t *vcmd, u32 interruput_mask) {
  u32 *dst;

  {
    dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_STALL | (0 << 16) | (interruput_mask);
    //alignment
    *dst = 0;

    vcmd->vcmdBufSize += 2;
  }
}

static void CWLCollectReadRegData(VcmdDes_t *vcmd, u16 reg_start, u32 reg_length,
                            ptr_t status_data_base_addr) {
  u32 *dst;

  //status_data_base_addr += reg_start*4;
  {
    dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_RREG | (reg_length << 16) | (reg_start * 4);
    //data
    *dst++ = (u32)status_data_base_addr;
    *dst++ = (u32)((u64)status_data_base_addr >> 32);

    //alignment
    *dst = 0;

    vcmd->vcmdBufSize += 4;
  }
}

static void CWLCollectNopData(VcmdDes_t *vcmd) {
  u32 *dst;

  {
    dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_NOP | (0);
    //alignment
    *dst = 0;

    vcmd->vcmdBufSize += 2;
  }
}

static void CWLCollectJmpData(VcmdDes_t *vcmd) {
  u32 *dst;

  {
    dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_JMP_RDY0 | (0);
    //addr
    *dst++ = (0);
    *dst++ = (0);
    *dst++ = (0);
    //alignment, do not do this step for driver will use this data to identify JMP and End opcode.
    vcmd->vcmdBufSize += 4;
  }
}

static void CWLCollectEndData(VcmdDes_t *vcmd) {
  u32 *dst;

  {
    dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_END | (0);
    //alignment
    *dst++ = (0);
    //do not do this step for driver will use this data to identify JMP and End opcode.
    vcmd->vcmdBufSize += 4;
  }
}

static void CWLCollectClrIntData(VcmdDes_t *vcmd, u32 clear_type, u16 interrupt_reg_addr,
                          u32 bitmask) {
  u32 *dst;

  {
    dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
    //opcode
    *dst++ = OPCODE_CLRINT | (clear_type << 25) | interrupt_reg_addr * 4;
    //bitmask
    *dst = bitmask;

    vcmd->vcmdBufSize += 2;
  }
}

static void CWLCollectM2RegData(VcmdDes_t *vcmd, u16 reg_start, u32 reg_length,
                            ptr_t mem_ba) {
  u32 *dst;

  dst = vcmd->vcmdBuf + vcmd->vcmdBufSize;
  //opcode
  *dst++ = OPCODE_M2REG | (reg_length << 16) | (reg_start * 4);
  //srcBufferAddr
  *dst++ = (0);
  *dst++ = (u32)mem_ba;
  *dst = (u32)((u64)mem_ba >> 32);

  vcmd->vcmdBufSize += 4;
}


static ptr_t VcmdBufGetStatusBufferBusAddr(const void *inst, VcmdDes_t *vcmd, u16 comp_type, u16 cmdbufid)
{
  ptr_t status_base;

  ASSERT(vcmd->cmdbufid == cmdbufid);

  status_base = vcmd->status_ba;
  if (comp_type==EWL_CLIENT_TYPE_MAIN) {
    status_base += EWLGetClientVcmdStatusBufOffset(inst, EWL_CLIENT_TYPE_MAIN);
  } else if (comp_type==EWL_CLIENT_TYPE_UFBC) {
    status_base += EWLGetClientVcmdStatusBufOffset(inst, EWL_CLIENT_TYPE_UFBC);
  }
  return status_base;
}

void VcmdbufCollectWriteRegData(const void *inst, VcmdDes_t *vcmd, u32 *src, u16 reg_start,
                            u32 reg_length) {
  u16 reg_base;

  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_MAIN)/4;

  CWLCollectWriteRegData(vcmd, src, reg_base + reg_start, reg_length);
  return;
}

void VcmdbufCollectNopData(const void *inst, VcmdDes_t *vcmd) {
  CWLCollectNopData(vcmd);
  return;
}

void VcmdbufCollectStallDataEncVideo(const void *inst, VcmdDes_t *vcmd) {
  u32 interruput_mask;

  interruput_mask = VCE_FRAME_RDY_INT_MASK;
  CWLCollectStallData(vcmd, interruput_mask);
  return;
}

void VcmdbufCollectStallUFBC(const void *inst, VcmdDes_t *vcmd) {
  u32 interruput_mask;
  interruput_mask = VCE_UFBC_INT_MASK | VCE_FRAME_RDY_INT_MASK;
  CWLCollectStallData(vcmd, interruput_mask);
  return;
}

void VcmdbufCollectStallDataCuTree(const void *inst, VcmdDes_t *vcmd) {
  u32 interruput_mask;
  u32 hw_id =  EWLGetVcmdVersionId(inst);
  if (hw_id >= 0x4342150A) {
    interruput_mask = VCE_FRAME_RDY_INT_MASK;
  }
  else {
    interruput_mask = VCE_CUTREE_RDY_INT_MASK;
  }
  CWLCollectStallData(vcmd, interruput_mask);
  return;
}

void VcmdbufCollectReadRegData(const void *inst, VcmdDes_t *vcmd,
                           u16 reg_start, u32 reg_length) {
  u16 reg_base;
  ptr_t status_base;

  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_MAIN)/4;
  status_base = VcmdBufGetStatusBufferBusAddr(inst, vcmd, EWL_CLIENT_TYPE_MAIN,
                        vcmd->cmdbufid);

  CWLCollectReadRegData(vcmd, reg_base + reg_start, reg_length,
                        status_base + reg_start * 4);
  return;
}

void VcmdbufCollectJmpData(const void *inst, VcmdDes_t *vcmd) {
  CWLCollectJmpData(vcmd);
  return;
}

void VcmdbufCollectEndData(const void *inst, VcmdDes_t *vcmd) {
  CWLCollectEndData(vcmd);
  return;
}

void VcmdbufCollectClrIntData(const void *inst, VcmdDes_t *vcmd) {
  u16 reg_base;
  u16 clear_type;
  u32 bitmask;

  clear_type = CLRINT_OPTYPE_READ_WRITE_1_CLEAR;
  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_MAIN) / 4;
  bitmask = 0xffffffff;
  CWLCollectClrIntData(vcmd, clear_type, reg_base + 1, bitmask);

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  CWLCollectClrIntData(vcmd, clear_type, reg_base + 118, bitmask);
#endif
  return;
}

void VcmdbufCollectClrIntUFBC(const void *inst, VcmdDes_t *vcmd, u32 irq_offset) {
  u16 reg_base;
  u16 clear_type;
  u32 bitmask;

  clear_type = CLRINT_OPTYPE_READ_WRITE_1_CLEAR;
  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_UFBC) / 4;
  bitmask = 0xffffffff;
  CWLCollectClrIntData(vcmd, clear_type, reg_base + irq_offset, bitmask);
  return;
}

void VcmdbufCollectClrIntReadClearDec400Data(const void *inst, VcmdDes_t *vcmd,
                                         u16 addr_offset) {
  u16 reg_base;
  u16 clear_type;
  u32 bitmask;

  clear_type = CLRINT_OPTYPE_READ_CLEAR;
  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_DEC400) / 4;
  bitmask = 0xffffffff;
  CWLCollectClrIntData(vcmd, clear_type, reg_base + addr_offset, bitmask);
  return;
}

void VcmdbufCollectStopHwData(const void *inst, VcmdDes_t *vcmd) {
  u16 reg_base;
  u16 clear_type;
  u32 bitmask;

  clear_type = CLRINT_OPTYPE_READ_WRITE_1_CLEAR;
  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_MAIN) / 4;
  bitmask = 0xfffffffe;
  CWLCollectClrIntData(vcmd, clear_type, reg_base + ASIC_REG_INDEX_STATUS,
                       bitmask);
  return;
}

// this function is for vcmd.
void VcmdbufCollectReadVcmdRegData(const void *inst, VcmdDes_t *vcmd, u16 reg_start,
                               u32 reg_length) {
  u16 reg_base;
  ptr_t status_data_base_addr = 0;

  reg_base = 0;
  if (1) //(enc->vcmd_enc_core_info.vcmd_hw_version_id > HW_ID_1_0_C)
    CWLCollectReadRegData(vcmd, reg_base + reg_start, reg_length,
                          status_data_base_addr + reg_start * 4);
  return;
}

//this function is for dec400
void VcmdbufCollectWriteDec400RegData(const void *inst, VcmdDes_t *vcmd, u32 *src,
                                  u16 reg_start, u32 reg_length) {
  u16 reg_base;

  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_DEC400) / 4;
  CWLCollectWriteRegData(vcmd, src, reg_base + reg_start, reg_length);
  return;
}

//this function is for dec400
void VcmdbufCollectStallDec400(const void *inst, VcmdDes_t *vcmd) {
  u32 interruput_mask;

  interruput_mask = VCE_DEC400_INT_MASK;
  CWLCollectStallData(vcmd, interruput_mask);
  return;
}

void VcmdbufCollectWriteDec400FCRegData(const void *inst, VcmdDes_t *vcmd, u16 reg_start,
                            ptr_t srcBufferAddr, u32 reg_length) {
  u32 *dst, *dst_base;
  u16 reg_addr, reg_base;

  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_DEC400) / 4;
  reg_addr = reg_base + reg_start;

  CWLCollectM2RegData(vcmd, reg_addr, reg_length, srcBufferAddr);
}

//this function is for MMU0/1
void VcmdbufCollectWriteMMURegData(const void *inst, VcmdDes_t *vcmd) {
  u16 reg_base0,reg_base1;
#ifdef SUPPORT_48PA_MMU
  int ret = -1, i;
  u16 reg_pagetable_id = 107;
  u16 reg_flushcmd = 109;
  //The maximum supported vmid is 16
  u32 mmu_ctrl[20] = {0x80000000};
#else
  u16 reg_flush = 97;
  u32 mmu_ctrl[2] = {0x10, 0x00};
#endif
  u32 *src;

#ifdef SUPPORT_SHARED_MMU
  return;
#endif

#ifdef SUPPORT_MMU
  src = mmu_ctrl;

  reg_base0 = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_MMU0);

  if (reg_base0 != 0xffff) {
#ifdef SUPPORT_48PA_MMU
    ret = EWLMMUSwitchPageTableByCmdbuf(inst, &mmu_ctrl[1]);
    if (ret == 0) {
      // flush pt by vmid
      for (i = 0; i < mmu_ctrl[2]; i++)
        CWLCollectWriteRegData(vcmd, src + 3 + i, reg_base0 / 4 + reg_flushcmd, 1);
      // switch mmu page table
      CWLCollectWriteRegData(vcmd, src + 1, reg_base0 / 4 + reg_pagetable_id, 1);
    }
    // flush all
    CWLCollectWriteRegData(vcmd, src , reg_base0 / 4 + reg_flushcmd, 1);
#else
    CWLCollectWriteRegData(vcmd, src, reg_base0 / 4 + reg_flush, 1);
    //CWLCollectNopData(vcmd);
    CWLCollectWriteRegData(vcmd, src + 1, reg_base0 / 4 + reg_flush, 1);
#endif
 }

  reg_base1 =EWLGetClientOffset(inst,EWL_CLIENT_TYPE_MMU1);
  if (reg_base1 != 0xffff) {
#ifdef SUPPORT_48PA_MMU
    if (ret == 0) {
      // flush pt by vmid
      for (i = 0; i < mmu_ctrl[1]; i++)
        CWLCollectWriteRegData(vcmd, src + 3 + i, reg_base1 / 4 + reg_flushcmd, 1);
      // switch mmu page table
      CWLCollectWriteRegData(vcmd, src + 1, reg_base1 / 4 + reg_pagetable_id, 1);
    }
    // flush all
    CWLCollectWriteRegData(vcmd, src , reg_base1 / 4 + reg_flushcmd, 1);
#else
    CWLCollectWriteRegData(vcmd, src, reg_base1 / 4 + reg_flush, 1);
    //CWLCollectNopData(vcmd);
    CWLCollectWriteRegData(vcmd, src + 1, reg_base1 / 4 + reg_flush, 1);
#endif
  }
#endif
}

//this function is for AXIFE0/1
void VcmdbufCollectWriteAxiFeRegData(const void *inst, VcmdDes_t *vcmd,
                                     u32 *src, u32 reg_start, u32 reg_num) {
  u32 reg_base[2] = {0xFFFF, 0xFFFF};
  u32 i;

  reg_base[0] = EWLGetClientOffset(inst, EWL_CLIENT_TYPE_AXIFE);
  reg_base[1] = EWLGetClientOffset(inst, EWL_CLIENT_TYPE_AXIFE_1);

#ifdef SUPPORT_AXIFE
  for (i=0; i<2; i++) {
      if (reg_base[i] != 0xFFFF) {
        CWLCollectWriteRegData(vcmd, src, (reg_base[i] + reg_start)/4, reg_num);
      }
    }
#endif
}

//this function is for ufbc

void VcmdbufCollectReadUFBCRegData(const void *inst, VcmdDes_t *vcmd,
                           u16 reg_start, u32 reg_length) {
  u16 reg_base;
  ptr_t status_base;

  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_UFBC) / 4;
  status_base = VcmdBufGetStatusBufferBusAddr(inst, vcmd, EWL_CLIENT_TYPE_UFBC,
                        vcmd->cmdbufid);

  CWLCollectReadRegData(vcmd, reg_base + reg_start, reg_length,
                        status_base + reg_start * 4);
  return;
}

void VcmdbufCollectWriteUFBCRegData(const void *inst, VcmdDes_t *vcmd, u32 *src,
                                  u16 reg_start, u32 reg_length) {
  u16 reg_base;

  reg_base = EWLGetClientOffset(inst,EWL_CLIENT_TYPE_UFBC) / 4;
  CWLCollectWriteRegData(vcmd, src, reg_base + reg_start, reg_length);
  return;
}

