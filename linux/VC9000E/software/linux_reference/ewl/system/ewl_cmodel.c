/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2015 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
--  Abstract : Encoder Wrapper Layer system model adapter
--
------------------------------------------------------------------------------*/

#include "ewl.h"

/* HW register definitions */
#include "enc_core.h"
#include "osal.h"
#include "ewl_system_def.h"

EWLFun *EWLFunCmodelP = NULL;
static u32 EWLFunCmodelRefCnt = 0;
static pthread_mutex_t ewlfuncmodel_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Global variables */
#ifdef EWL_PERFORMANCE
static u32 vopNum;
static u32 memAlloc[MEM_CHUNKS];
static u32 memAllocTotal;
static u32 memChunks;
#endif

/* Function to test input line buffer with hardware handshake, should be set by app. (invalid for system) */
u32 (*pollInputLineBufTestFunc)(void) = NULL;

void EWLAttachVCDM(EWLFun *EWLFunP);
void EWLAttachSys(EWLFun *EWLFunP);

#ifdef SUPPORT_MEM_STATISTIC
void EwlShowMemoryStats() {}
#endif

void *EWLmalloc(u32 n) {
#ifdef EWL_PERFORMANCE
  ASSERT(memChunks < MEM_CHUNKS);
  memAlloc[memChunks++] = n;
  memAllocTotal += n;
#endif
  return malloc((size_t)n);
}

void EWLfree(void *p) { free(p); }

void *EWLcalloc(u32 n, u32 s) {
#ifdef EWL_PERFORMANCE
  ASSERT(memChunks < MEM_CHUNKS);
  memAlloc[memChunks++] = n * s;
  memAllocTotal += n * s;
#endif
  return calloc((size_t)n, (size_t)s);
}

mem_ret EWLmemcpy(void *d, const void *s, u32 n) {
  return memcpy(d, s, (size_t)n);
}

mem_ret EWLmemset(void *d, i32 c, u32 n) {
  return memset(d, (int)c, (size_t)n);
}

u32 EWLGetVcmdVersionId(const void *inst) {
  return 0;
}

int EWLmemcmp(const void *s1, const void *s2, u32 n) {
#ifdef SAFESTRING
  int ind = 0;
  int rc = 0;
  if ((rc = memcmp_s(s1, (size_t)n, s2, (size_t)n, &ind)) != EOK) {
    printf("%s %d  Ind=%d  Error rc=%d \n", __FUNCTION__, __LINE__, ind, rc);
    ASSERT(0);
  }

  return ind;
#else
  return memcmp(s1, s2, (size_t)n);
#endif
}

i32 EWLGetLineBufSram(const void *instance, EWLLinearMem_t *info) {
  return EWLFunCmodelP->EWLGetLineBufSramP(instance, info);
}
i32 EWLMallocLoopbackLineBuf(const void *instance, u32 size,
                             EWLLinearMem_t *info) {
  return EWLFunCmodelP->EWLMallocLoopbackLineBufP(instance, size, info);
}

u32 EWLGetClientType(const void *inst) {
  return EWLFunCmodelP->EWLGetClientTypeP(inst);
}

u32 EWLGetCoreTypeByClientType(u32 client_type) {
  return EWLFunCmodelP->EWLGetCoreTypeByClientTypeP(client_type);
}

i32 EWLCheckCutreeValid(const void *inst) {
  return EWLFunCmodelP->EWLCheckCutreeValidP(inst);
}

u32 EWLReadAsicID(u32 client_type, const void *ctx) {
  return EWLFunCmodelP->EWLReadAsicIDP(client_type, ctx);
}

const EWLHwConfig_t *EWLReadAsicConfig(u32 client_type, const  void *ctx) {
  return EWLFunCmodelP->EWLReadAsicConfigP(client_type, ctx);
}

const void *EWLInit(EWLInitParam_t *param) {
  EWLFun *p;
  SysCoreInfo CoreInfo;

  pthread_mutex_lock(&ewlfuncmodel_mutex);
  if (EWLFunCmodelP) {
    EWLFunCmodelRefCnt++;
    pthread_mutex_unlock(&ewlfuncmodel_mutex);
    return EWLFunCmodelP->EWLInitP(param);
  }

  p = (EWLFun *)EWLcalloc(1, sizeof(EWLFun));
  if (p == NULL) {
    pthread_mutex_unlock(&ewlfuncmodel_mutex);
    return NULL;
  }

  EWLFunCmodelP = p;
  CoreInfo = CoreEncGetHwInfo(0, 0);
  if (param->useVcmd == 0) {
    EWLAttachSys(EWLFunCmodelP);
  } else {
    if (CoreInfo.Cfg[0].has_vcmd == HasExtHw) {
      EWLAttachVCDM(EWLFunCmodelP);
    } else {
      EWLAttachSys(EWLFunCmodelP);
      param->useVcmd = 0;
    }
  }
  EWLFunCmodelRefCnt = 1;
  pthread_mutex_unlock(&ewlfuncmodel_mutex);
  return EWLFunCmodelP->EWLInitP(param);
}

i32 EWLRelease(const void *instance) {
  i32 ret;

  pthread_mutex_lock(&ewlfuncmodel_mutex);
  if (EWLFunCmodelRefCnt == 0) {
    pthread_mutex_unlock(&ewlfuncmodel_mutex);
    return 0;
  }
  pthread_mutex_unlock(&ewlfuncmodel_mutex);

  ret = EWLFunCmodelP->EWLReleaseP(instance);
  pthread_mutex_lock(&ewlfuncmodel_mutex);
  if (EWLFunCmodelRefCnt == 1) {
    EWLfree(EWLFunCmodelP);
    EWLFunCmodelP = NULL;
  }
  EWLFunCmodelRefCnt--;
  pthread_mutex_unlock(&ewlfuncmodel_mutex);

  return ret;
}

void EWLWriteRegbyClientType(const void *inst, u32 offset, u32 val,
                             u32 client_type) {
  EWLFunCmodelP->EWLWriteRegbyClientTypeP(inst, offset, val, client_type);
}

void EWLWriteCoreReg(const void *instance, u32 offset, u32 val, u32 core_id) {
  EWLFunCmodelP->EWLWriteCoreRegP(instance, offset, val, core_id);
}

void EWLWriteCoreRegByVcmd(const void *inst, u32 offset, u32 num, u32 *val) {
  (void)inst;
  (void)offset;
  (void)val;
  (void)num;
}

void EWLWriteReg(const void *instance, u32 offset, u32 val) {
  EWLFunCmodelP->EWLWriteRegP(instance, offset, val);
}

void EWLWriteBackRegbyClientType(const void *inst, u32 offset, u32 val,
                                 u32 client_type) {
  (void)inst;
  (void)offset;
  (void)val;
  (void)client_type;
}

void EWLSetReserveBaseData(const void *inst, u64 interrupt_ctrl,
                           u32 client_type) {
  EWLFunCmodelP->EWLSetReserveBaseDataP(inst, interrupt_ctrl, client_type);
}

void EWLWriteBackReg(const void *inst, u32 offset, u32 val) {
  EWLFunCmodelP->EWLWriteBackRegP(inst, offset, val);
}

i32 EWLEnableHW(const void *inst, u32 offset, u32 val) {
  return EWLFunCmodelP->EWLEnableHWP(inst, offset, val);
}

void EWLDisableHW(const void *inst, u32 offset, u32 val) {
  EWLFunCmodelP->EWLDisableHWP(inst, offset, val);
}

u32 EWLGetPerformance(const void *inst) {
  return EWLFunCmodelP->EWLGetPerformanceP(inst);
}

u32 EWLReadRegbyClientType(const void *inst, u32 offset, u32 client_type) {
  (void)inst;
  (void)offset;
  (void)client_type;

  return 0;
}

u32 EWLReadReg(const void *inst, u32 offset) {
  return EWLFunCmodelP->EWLReadRegP(inst, offset);
}

u32 EWLReadRegInit(const void *inst, u32 offset) {
  return EWLFunCmodelP->EWLReadRegInitP(inst, offset);
}

i32 EWLMallocRefFrm(const void *instance, u32 size, u32 alignment,
                    EWLLinearMem_t *info) {
  return EWLFunCmodelP->EWLMallocRefFrmP(instance, size, alignment, info);
}

void EWLFreeRefFrm(const void *instance, EWLLinearMem_t *info) {
  EWLFunCmodelP->EWLFreeRefFrmP(instance, info);
}

i32 EWLMallocLinear(const void *instance, u32 size, u32 alignment,
                    EWLLinearMem_t *info) {
  return EWLFunCmodelP->EWLMallocLinearP(instance, size, alignment, info);
}

void EWLFreeLinear(const void *instance, EWLLinearMem_t *info) {
  EWLFunCmodelP->EWLFreeLinearP(instance, info);
}

#ifdef SUPPORT_MEM_SYNC
i32 EWLSyncMemData(EWLLinearMem_t *mem, u32 offset, u32 length,
                   enum EWLMemSyncDirection dir) {
  return EWLFunCmodelP->EWLSyncMemDataP(mem, offset, length, dir);
}

i32 EWLMemSyncAllocHostBuffer(const void *inst, u32 size, u32 alignment,
                              EWLLinearMem_t *buff) {
  return EWLFunCmodelP->EWLMemSyncAllocHostBufferP(inst, size, alignment, buff);
}

i32 EWLMemSyncFreeHostBuffer(const void *inst, EWLLinearMem_t *buff) {
  return EWLFunCmodelP->EWLMemSyncFreeHostBufferP(inst, buff);
}
#endif

i32 EWLGetDec400Coreid(const void *inst) {
  return EWLFunCmodelP->EWLGetDec400CoreidP(inst);
}


i32 EWLReserveCmdbuf(const void *inst, EWLResource_t *resource) {
  return EWLFunCmodelP->EWLReserveCmdbufP(inst, resource);
}

i32 EWLLinkRunCmdbuf(const void *inst, u16 cmdbufid, u16 cmdbuf_size) {
  return EWLFunCmodelP->EWLLinkRunCmdbufP(inst, cmdbufid, cmdbuf_size);
}

i32 EWLWaitCmdbuf(const void *inst, u16 cmdbufid, u32 *status) {
  return EWLFunCmodelP->EWLWaitCmdbufP(inst, cmdbufid, status);
}

void EWLGetRegsByCmdbuf(const void *inst, u16 cmdbufid, u32 *regMirror) {
  EWLFunCmodelP->EWLGetRegsByCmdbufP(inst, cmdbufid, regMirror);
}

i32 EWLReleaseCmdbuf(const void *inst, u16 cmdbufid) {
  return EWLFunCmodelP->EWLReleaseCmdbufP(inst, cmdbufid);
}

void EWLTraceProfile(const void *instance, void *prof_data, i32 qp, i32 poc) {
  EWLFunCmodelP->EWLTraceProfileP(instance, prof_data, qp, poc);
}

void EWLDCacheRangeFlush(const void *instance, EWLLinearMem_t *info) {
  EWLFunCmodelP->EWLDCacheRangeFlushP(instance, info);
}

void EWLDCacheRangeRefresh(const void *instance, EWLLinearMem_t *info) {
  EWLFunCmodelP->EWLDCacheRangeRefreshP(instance, info);
}

i32 EWLWaitHwRdy(const void *instance, u32 *slicesReady, void *waitOut,
                 u32 *status_register) {
  return EWLFunCmodelP->EWLWaitHwRdyP(instance, slicesReady, waitOut,
                                      status_register);
}

i32 EWLReserveHw(const void *inst, u32 *core_info, u32 *job_id) {
  return EWLFunCmodelP->EWLReserveHwP(inst, core_info, job_id);
}

void EWLReleaseHw(const void *inst) { EWLFunCmodelP->EWLReleaseHwP(inst); }

u32 EWLGetCoreNum(const void *ctx) { return EWLFunCmodelP->EWLGetCoreNumP(ctx); }

void EWLSetVCMDMode(const void *inst, u32 mode) {
  EWLFunCmodelP->EWLSetVCMDModeP(inst, mode);
}

u32 EWLGetVCMDMode(const void *inst) {
  return EWLFunCmodelP->EWLGetVCMDModeP(inst);
}

u32 EWLGetVCMDSupport(const void *inst) {
  return EWLFunCmodelP->EWLGetVCMDSupportP();
}

u32 EWLReleaseEwlWorkerInst(const void *inst) {
  return EWLFunCmodelP->EWLReleaseEwlWorkerInstP(inst);
}

void EWLClearTraceProfile(const void *inst) {
  EWLFunCmodelP->EWLClearTraceProfileP(inst);
}

i32 EWLGetVcmdCoreNum(const void *ctx, CLIENT_TYPE client_type) {
  return EWLFunCmodelP->EWLGetVcmdCoreNumP(ctx, client_type);
}

void EWLReadVcmdPriority(const void *ctx, u32 *priority, u32 client_type) {
  return EWLFunCmodelP->EWLReadVcmdPriorityP(ctx, priority, client_type);
}

char *EWLGetDevName(const void *inst) {
  return EWLFunCmodelP->EWLGetDevNameP(inst);
}

char *EWLGetMemDevName(const void *inst) {
  return EWLFunCmodelP->EWLGetMemDevNameP(inst);
}

void EWLAttach(const void *ctx, int slice_idx, i32 vcmd_support) {
  (void)ctx;
  (void)slice_idx;
  (void)vcmd_support;

  return;
}

void EWLDetach() { return; }

i32 EWLGetConfigRegister(const void *inst, u32 core_id, u32 client_type, u32 offset) {
  return EWLFunCmodelP->EWLGetConfigRegisterP(inst, core_id, client_type, offset);
}

u32 EWLIsVCMDSupportM2REG(const void *inst) {
  return 0;
}