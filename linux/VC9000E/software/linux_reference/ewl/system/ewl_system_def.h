/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2015 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : header file of for System Model EWL Common Part
--
------------------------------------------------------------------------------*/

#ifndef __EWL_SYSTEM_H__
#define __EWL_SYSTEM_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include "ewl.h"

#define ASIC_STATUS_IRQ_INTERVAL 0x100

/* Mask fields */
#define mask_1b (u32)0x00000001
#define mask_2b (u32)0x00000003
#define mask_3b (u32)0x00000007
#define mask_4b (u32)0x0000000F
#define mask_5b (u32)0x0000001F
#define mask_6b (u32)0x0000003F
#define mask_8b (u32)0x000000FF
#define mask_9b (u32)0x000001FF
#define mask_10b (u32)0x000003FF
#define mask_18b (u32)0x0003FFFF

typedef struct {
  i32 (*EWLGetLineBufSramP)(const void *, EWLLinearMem_t *);
  i32 (*EWLMallocLoopbackLineBufP)(const void *, u32, EWLLinearMem_t *);
  u32 (*EWLGetClientTypeP)(const void *);
  u32 (*EWLGetCoreTypeByClientTypeP)(u32);
  i32 (*EWLCheckCutreeValidP)(const void *);
  u32 (*EWLReadAsicIDP)(u32, const void *);
  const EWLHwConfig_t *(*EWLReadAsicConfigP)(u32, const void *);
  const void *(*EWLInitP)(EWLInitParam_t *);
  i32 (*EWLReleaseP)(const void *);
  void (*EwlReleaseCoreWaitP)(void *);
  EWLCoreWaitJob_t *(*EWLDequeueCoreOutJobP)(const void *, u32);
  void (*EWLEnqueueOutToWaitP)(const void *, EWLCoreWaitJob_t *);
  void (*EWLEnqueueWaitjobP)(const void *, u32);
  void (*EWLPutJobtoPoolP)(const void *, struct node *);
  void (*EWLPutJobtoPool)(const void *, struct node *);
  void (*EWLWriteCoreRegP)(const void *, u32, u32, u32);
  void (*EWLWriteRegP)(const void *, u32, u32);
  void (*EWLSetReserveBaseDataP)(const void *, u64, u32);
  void (*EWLWriteBackRegP)(const void *, u32, u32);
  i32 (*EWLEnableHWP)(const void *, u32, u32);
  u32 (*EWLGetPerformanceP)(const void *);
  void (*EWLDisableHWP)(const void *, u32, u32);
  u32 (*EWLReadRegP)(const void *, u32);
  u32 (*EWLReadRegInitP)(const void *, u32);
  i32 (*EWLMallocRefFrmP)(const void *, u32, u32, EWLLinearMem_t *);
  void (*EWLFreeRefFrmP)(const void *, EWLLinearMem_t *);
  i32 (*EWLMallocLinearP)(const void *, u32, u32, EWLLinearMem_t *);
  void (*EWLFreeLinearP)(const void *, EWLLinearMem_t *);
  i32 (*EWLSyncMemDataP)(EWLLinearMem_t *, u32, u32, enum EWLMemSyncDirection);
  i32 (*EWLMemSyncAllocHostBufferP)(const void *, u32, u32, EWLLinearMem_t *);
  i32 (*EWLMemSyncFreeHostBufferP)(const void *, EWLLinearMem_t *);
  void (*EWLDCacheRangeFlushP)(const void *, EWLLinearMem_t *);
  i32 (*EWLWaitHwRdyP)(const void *, u32 *, void *, u32 *);
  void (*EWLDCacheRangeRefreshP)(const void *, EWLLinearMem_t *);
  void (*EWLReleaseHwP)(const void *);
  i32 (*EWLReserveHwP)(const void *, u32 *, u32 *);
  u32 (*EWLGetCoreNumP)(const void *);
  i32 (*EWLGetDec400CoreidP)(const void *);
  void (*EWLGetDec400AttributeP)(u32 *, u32 *, u32 *);
  i32 (*EWLReserveCmdbufP)(const void *, EWLResource_t *);
  i32 (*EWLLinkRunCmdbufP)(const void *, u16, u16);
  i32 (*EWLWaitCmdbufP)(const void *, u16, u32 *);
  void (*EWLGetRegsByCmdbufP)(const void *, u16, u32 *);
  i32 (*EWLReleaseCmdbufP)(const void *, u16);
  void (*EWLTraceProfileP)(const void *, void *, i32, i32);
  u32 (*EWLGetVCMDSupportP)();
  void (*EWLSetVCMDModeP)(const void *inst, u32 mode);
  u32 (*EWLGetVCMDModeP)(const void *inst);

  u32 (*EWLReleaseEwlWorkerInstP)(const void *inst);
  void (*EWLClearTraceProfileP)(const void *);
  void (*EWLWriteRegbyClientTypeP)(const void *inst, u32 , u32 ,
                             u32 );
  i32 (*EWLGetVcmdCoreNumP)(const void *, CLIENT_TYPE);
  void (*EWLReadVcmdPriorityP)(const void *, u32 *, u32);

  char *(*EWLGetDevNameP)(const void *);
  char *(*EWLGetMemDevNameP)(const void *);
  i32 (*EWLGetConfigRegisterP)(const void *, u32, u32, u32);
} EWLFun;

#ifdef __cplusplus
}
#endif

#endif /* __EWL_SYSTEM_H__ */

