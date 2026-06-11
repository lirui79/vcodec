/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2014 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : H2 Encoder Wrapper Layer for OS services
--
------------------------------------------------------------------------------*/

#ifndef __VCMDBUF_H__
#define __VCMDBUF_H__

#ifdef __cplusplus
extern "C" {
#endif

  /** vcmd buffer description */
  typedef struct VcmdDes { 		//cppcheck-suppress syntaxError
    u32 *vcmdBuf; /**< pointer to cmd buffer */
    u32 vcmdBufSize; /**< bytes used in current buffer */
    u16 cmdbufid; /**< current cmd buffer id */

    ptr_t status_ba; /**< bus address to store stats */
    u32 core_mask;
    u32 priority;
  } VcmdDes_t;

  void VcmdbufCollectWriteRegData(const void *inst, VcmdDes_t *vcmd, u32 *src, u16 reg_start,
                              u32 reg_length);
  void VcmdbufCollectStallDataEncVideo(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectStallDataCuTree(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectStallDec400(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectWriteDec400FCRegData(const void *inst, VcmdDes_t *vcmd, u16 reg_start,
                                            ptr_t srcBufferAddr, u32 reg_length);
  void VcmdbufCollectReadRegData(const void *inst, VcmdDes_t *vcmd, u16 reg_start,
                             u32 reg_length);
  void VcmdbufCollectWriteMMURegData(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectWriteAxiFeRegData(const void *inst, VcmdDes_t *vcmd,
                                      u32 *src, u32 reg_start, u32 reg_num);
  void VcmdbufCollectWriteUFBCRegData(const void *inst, VcmdDes_t *vcmd, u32 *src,
                                  u16 reg_start, u32 reg_length);
  void VcmdbufCollectReadVcmdRegData(const void *inst, VcmdDes_t *vcmd, u16 reg_start,
                                  u32 reg_length);
  void VcmdbufCollectReadUFBCRegData(const void *inst, VcmdDes_t *vcmd,
                           u16 reg_start, u32 reg_length);
  void VcmdbufCollectWriteDec400RegData(const void *inst, VcmdDes_t *vcmd, u32 *src,
                                    u16 reg_start, u32 reg_length);
  void VcmdbufCollectClrIntReadClearDec400Data(const void *inst, VcmdDes_t *vcmd,
                                           u16 addr_offset);
  void VcmdbufCollectJmpData(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectEndData(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectClrIntData(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectClrIntUFBC(const void *inst, VcmdDes_t *vcmd, u32 irq_offset);
  void VcmdbufCollectStopHwData(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectNopData(const void *inst, VcmdDes_t *vcmd);
  void VcmdbufCollectStallUFBC(const void *inst, VcmdDes_t *vcmd);


#ifdef __cplusplus
}
#endif
#endif /*__VCMDBUF_H__*/

