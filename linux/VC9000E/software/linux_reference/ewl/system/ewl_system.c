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
--  Abstract : Encoder Wrapper Layer system model adapter
--
------------------------------------------------------------------------------*/

#include "tools.h"

//#define V4L2
#ifdef V4L2
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#endif

/* Common EWL interface */
#include "ewl.h"
#include "ewl_common.h"
#include "encasiccontroller.h"
#include "enc_log.h"

/* HW register definitions */
#include "enc_core.h"
#include "osal.h"
#include "ewl_sys_local.h"
#include "ewl_memsync.h"
//#define CORE_NUM 4  /*move to enc_core.h*/
//#define MAX_CORE_NUM 4  /*move to enc_core.h*/

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))

static EWLSubSysOut_t subSysOut[MAX_CORE_NUM];
static u32 reserved_job_id[MAX_CORE_NUM];
static u32 instCounter;
static pthread_mutex_t instCounter_mutex = PTHREAD_MUTEX_INITIALIZER;

extern pthread_mutex_t ewl_mutex;

static u32 EWLGetCoreNumSys(const void *ctx);
static i32 EWLHwSysDone(const void *instance);
static void EWLReleaseHwSys(const void *inst);
static i32 EWLWaitHwRdySys(const void *instance, u32 *slicesReady,
                           void *waitOut, u32 *status_register);
static void EWLGetCoreOutRelSys(const void *inst, i32 ewl_ret,
                                EWLCoreWaitJob_t *job);
static void *EWLCoreWaitThreadSys(void *pCoreWait);
static void EwlCreateCoreWaitSys(void);
i32 EWLSyncMemDataSys(EWLLinearMem_t *mem, u32 offset, u32 length,
                      enum EWLMemSyncDirection dir);
i32 EWLMemSyncAllocHostBufferSys(const void *instance, u32 size, u32 alignment,
                                 EWLLinearMem_t *buff);
i32 EWLMemSyncFreeHostBufferSys(const void *instance, EWLLinearMem_t *buff);
#ifndef SUPPORT_MEM_SYNC
#define EWLSyncMemDataSys(mem, offset, length, dir) (EWL_OK)
#define EWLMemSyncAllocHostBufferSys(inst, size, alignment, buff) \
  (EWL_NOT_SUPPORT)
#define EWLMemSyncFreeHostBufferSys(inst, buff) (EWL_NOT_SUPPORT)
#endif

#ifdef VIRTUAL_PLATFORM_TEST
static void EWLVirtualPlatformTestIn(void *instance);
static void EWLVirtualPlatformTestOut(void *instance);
#endif

//#define DEBUG_WORKERS
#ifdef DEBUG_WORKERS
static void showWorkers(char *str, struct queue *workers, struct queue *frees)
{
  int i;
  EWLWorker *worker;
  printf("%s Workers\n",str);
  worker = (EWLWorker *)queue_tail(workers);
  i=0;
  while (worker) {
    printf(" %d, core id is %08x\n", i, worker->core_id);
    worker = (EWLWorker *)worker->next;
    i++;
  }

  printf("%s Frees\n",str);
  worker = (EWLWorker *)queue_tail(frees);
  i=0;
  while (worker) {
    printf(" %d, core id is %08x\n", i, worker->core_id);
    worker = (EWLWorker *)worker->next;
    i++;
  }
}
#else
#define showWorkers(a,b,c)
#endif

/*------------------------------------------------------------------------------

------------------------------------------------------------------------------*/

static u32 EWLReadAsicIDSys(u32 core_id, const void *ctx) {
  SysCore *core;
  u32 slice_idx = NODE(core_id);
  u32 core_idx = CORE(core_id);

  if (core_idx >= MAX_HWCORE_NUM) return 0;

  core = CoreEncSetup(slice_idx, core_idx);

  return CoreEncGetRegister(core, 0);
}

/*******************************************************************************
 Function name   : EWLReadAsicConfig
 Description     : Reads ASIC capability register, static implementation
 Return type     : EWLHwConfig_t
 Argument        : void
*******************************************************************************/
const static EWLHwConfig_t *EWLReadAsicConfigSys(u32 core_id, const void *ctx) {
  u32 slice_idx = NODE(core_id);
  u32 core_idx = CORE(core_id);
  SysCore *core = CoreEncSetup(slice_idx, core_idx);
  EWLCoreSignature_t signature;
  const EWLHwConfig_t *cfg_info = NULL;

  signature.hw_asic_id = EWLReadAsicIDSys(core_id, ctx);

  signature.hw_build_id = CoreEncGetRegister(core, HWIF_REG_BUILD_ID * 4);
  signature.fuse[0] = CoreEncGetRegister(core, HWIF_REG_BUILD_REV * 4);

  signature.fuse[1] = CoreEncGetRegister(core, HWIF_REG_CFG1 * 4);
  signature.fuse[2] = CoreEncGetRegister(core, HWIF_REG_CFG2 * 4);
  signature.fuse[3] = CoreEncGetRegister(core, HWIF_REG_CFG3 * 4);
  signature.fuse[4] = CoreEncGetRegister(core, HWIF_REG_CFG4 * 4);
  signature.fuse[5] = CoreEncGetRegister(core, HWIF_REG_CFG5 * 4);
  signature.fuse[6] = CoreEncGetRegister(core, HWIF_REG_CFGAXI * 4);

  signature.hw_asic_id = EWLReadAsicIDSys(core_id, ctx);
  signature.fuse[1] = CoreEncGetRegister(core, HWIF_REG_CFG1 * 4);

  if (EWL_OK != EWLGetCoreConfig(&signature, &cfg_info)){
  PTRACE("ERROR when get the core feature list.");
  ASSERT(cfg_info == NULL);
  }
  return cfg_info;
}

/*------------------------------------------------------------------------------

    System model adapter for EWL.

------------------------------------------------------------------------------*/
static u32 EWLReadRegSys(const void *inst, u32 offset) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  ASSERT(inst != NULL);
  ASSERT(offset < ASIC_SWREG_AMOUNT * 4);
  u32 core_id = FIRST_CORE(instance);
  SysCore *core = instance->core[CORE(core_id)];
  (void)inst;

  return CoreEncGetRegister(core, offset);
}

static void EWLWriteCoreRegSys(const void *inst, u32 offset, u32 val,
                               u32 core_id) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  ASSERT(inst != NULL);
  ASSERT(offset < ASIC_SWREG_AMOUNT * 4);
  if (core_id >= ARRAY_SIZE(instance->core))
  	return;
  SysCore *core = instance->core[CORE(core_id)];
  (void)inst;

  // EWLTRACE_E((void*)inst, "EWLWriteReg 0x%02x with value %08x\n", offset, val);

  CoreEncSetRegister(core, offset, val);
}
static void EWLWriteRegSys(const void *inst, u32 offset, u32 val) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  u32 core_id = LAST_CORE(instance);
  EWLWriteCoreRegSys(inst, offset, val, core_id);
}
static void EWLWriteBackRegSys(const void *inst, u32 offset, u32 val) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  u32 core_id = FIRST_CORE(instance);
  EWLWriteCoreRegSys(inst, offset, val, core_id);
}

static void EWLWriteRegbyClientTypeSys(const void *inst, u32 offset, u32 val,
                             u32 client_type) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  u32 core_id = FIRST_CORE(instance);
  SysCore *core = instance->core[CORE(core_id)];
  if(client_type == EWL_CLIENT_TYPE_UFBC)
  {
    CoreEncSetUfbcRegister(core, offset, val);
  }
}

u32 EWLGetClientTypeSys(const void *inst) {
  ewlSysInstance *enc = (ewlSysInstance *)inst;
  u32 client_type = enc->clientType;

  return client_type;
}

static u32 EWLGetCoreTypeByClientTypeSys(u32 client_type) { return 0; }

static u32 EWLChangeClientTypeSys(const void *inst, u32 client_type) {
  return 0;
}

static i32 EWLCheckCutreeValidSys(const void *inst) { return EWL_OK; }

/*------------------------------------------------------------------------------
    Function name   : EWLEnableHW
    Description     :
    Return type     : i32
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
static i32 EWLEnableHWSys(const void *instance, u32 offset, u32 val) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  (void)inst;
  (void)val;
  u32 core_id = LAST_CORE(inst);
  SysCore *core = inst->core[CORE(core_id)];
  u32 width = 0, height = 0;

#ifdef VIRTUAL_PLATFORM_TEST
  EWLVirtualPlatformTestIn((void *)inst);
#endif

  EWLTRACE_I((void *)instance,
           "EWLEnableHW core_id(%x) offset:0x%02x with value %08x\n", core_id,
           offset, val);

  /* setup callback if need */
  if (pollInputLineBufTestFunc)
    core->cfg.cb_get_input_rows = pollInputLineBufTestFunc;

  CoreEncSetRegister(core, offset, val);

  if (offset == (ASIC_REG_INDEX_STATUS * 4)) {
    if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_EN) &&
        CoreEncGetRegisterValue(core, HWIF_ENC_MODE) != ASIC_CUTREE) {
      if (CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_HEVC ||
          CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_H264 ||
          CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_AV1 ||
          CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_VP9) {
        width =
            (CoreEncGetRegisterValue(core, HWIF_ENC_PIC_WIDTH) |
             (CoreEncGetRegisterValue(core, HWIF_ENC_PIC_WIDTH_MSB) << 10) |
             (CoreEncGetRegisterValue(core, HWIF_ENC_PIC_WIDTH_MSB2) << 12)) *
            8;
        height = CoreEncGetRegisterValue(core, HWIF_ENC_PIC_HEIGHT) * 8;
      } else {
        width = (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_WIDTH) |
                 (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_WIDTH_MSB)
                  << 12)) *
                8;
        height = (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_HEIGHT) |
                  (CoreEncGetRegisterValue(core, HWIF_ENC_JPEG_PIC_HEIGHT_MSB)
                   << 12)) *
                 8;
      }
      if(!inst->streamTempBuffer){
        inst->streamTempBuffer = calloc(width * height * 2 * 4, sizeof(u8));
      }
      inst->streamLength = 0;
      inst->frameRdy = -1;
      inst->segmentAmount =
          CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT) /
          CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SIZE);
      inst->streamBase =
          (u8 *)CoreEncGetAddrRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BASE);
      CoreEncSetAddrRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BASE,
                                  (ptr_t)inst->streamTempBuffer);
      CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR, 0);
      inst->segmentRdCnt = 0;
    }
  }

  CoreEncRun(inst->core[CORE(core_id)], offset, val, &inst->winst);

  EWLHwSysDone(inst);

  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : EWLDisableHW
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
static void EWLDisableHWSys(const void *inst, u32 offset, u32 val) {
  ASSERT(inst != NULL);
  (void)inst;
  (void)val;
  if (offset != (4 * 4)) {
    ASSERT(0);
  }

  EWLTRACE_I((void *)inst, "EWLDisableHW 0x%02x with value %08x\n", offset * 4,
           val);
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetPerformance
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
static u32 EWLGetPerformanceSys(const void *instance) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  return inst->performance;
}

static i32 EWLGetDec400CoreidSys(const void *inst) { return 0; }

static void EWLGetDec400AttributeSys(u32 *tile_size,
                                     u32 *bits_tile_in_table,
                                     u32 *planar420_cbcr_table_style) {
  *tile_size = 256;
  *bits_tile_in_table = 4;
  *planar420_cbcr_table_style = 0;
}

struct queue *EWLGetWorkers(const void *inst) {
  ewlSysInstance *ewl = (ewlSysInstance *)inst;
  return &ewl->workers;
}

void ewlSetUncheckPidFlag(const void *inst) {
}


/*------------------------------------------------------------------------------
    Function name   : EWLInit
    Description     :
    Return type     : void
    Argument        : const void *inst
------------------------------------------------------------------------------*/
static const void *EWLInitSys(EWLInitParam_t *param) {
  int i;
  ewlSysInstance *inst;
  u32 core_num, slice_num, slice;

  if (param == NULL || param->clientType >= EWL_CLIENT_TYPE_MAX) return NULL;

  inst = (ewlSysInstance *)malloc(sizeof(ewlSysInstance));
  if (inst == NULL) return NULL;
  memset(inst, 0, sizeof(ewlSysInstance));

  slice_num = CoreEncGetSliceNum();
  (void)slice_num;
  slice = param->slice_idx;

  ASSERT(slice < slice_num);

  core_num = CoreEncGetCoreNum(slice);

  for (i = 0; i < core_num; i++) {
    inst->core[i] = CoreEncSetup(slice, i);
  }

  inst->clientType = param->clientType;
  inst->linMemChunks = 0;
  inst->refFrmChunks = 0;
  inst->totalChunks = 0;
  inst->streamTempBuffer = NULL;

#ifdef V4L2
  inst->fd_v4l2 = open(V4L2_DRV_PATH, O_RDWR);
  if (inst->fd_v4l2 == -1) {
    EWLTRACE_I(NULL, "EWLInit: failed to open: %s\n", V4L2_DRV_PATH);
	free(inst);
    return NULL;
  }
#endif

  inst->prof.frame_number = 0;
  inst->prof.total_bits = 0;
  inst->prof.total_Y_PSNR = 0.0;
  inst->prof.total_U_PSNR = 0.0;
  inst->prof.total_V_PSNR = 0.0;
  inst->prof.Y_PSNR_Max = -1;
  inst->prof.Y_PSNR_Min = 1000.0;
  inst->prof.U_PSNR_Max = -1;
  inst->prof.U_PSNR_Min = 1000.0;
  inst->prof.V_PSNR_Max = -1;
  inst->prof.V_PSNR_Min = 1000.0;
  inst->prof.total_ssim = 0.0;
  inst->prof.total_ssim_y = 0.0;
  inst->prof.total_ssim_u = 0.0;
  inst->prof.total_ssim_v = 0.0;
  inst->prof.QP_Max = 0;
  inst->prof.QP_Min = 52;

  queue_init(&inst->freelist);
  queue_init(&inst->workers);
  for (i = 0; i < (EWL_IS_JPEG_CLIENT(param->clientType)
                       ? 1
                       : (int)EWLGetCoreNumSys(param->context));
       i++) {
    EWLWorker *worker = EWLmalloc(sizeof(EWLWorker));
	if (worker == NULL)
		break;
    worker->core_id = COREID(slice, i);
    worker->next = NULL;
    queue_put(&inst->freelist, (struct node *)worker);
  }

  extern void EWLInitMulticore(u32 clientType);
  EWLInitMulticore(inst->clientType);
  VCEncDec400RegisiterWL(inst);
  pthread_mutex_lock(&instCounter_mutex);
  instCounter++;
  pthread_mutex_unlock(&instCounter_mutex);

  return (void *)inst;
}

/*------------------------------------------------------------------------------
    Function name   : EWLRelease
    Description     :
    Return type     : void
    Argument        : const void *inst
------------------------------------------------------------------------------*/
static i32 EWLReleaseSys(const void *instance) {
  u32 i, tothw, totswhw;
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  if (inst == NULL)	return EWL_ERROR;

  u32 core_id = LAST_WAIT_CORE(inst);
  SysCore *core = inst->core[CORE(core_id)];

  CoreEncRelease(core, &inst->winst, inst->clientType);

  EWLReleaseMulticore(inst->clientType);

  if (inst->streamTempBuffer) free(inst->streamTempBuffer);
#ifndef V4L2
  MEM_LOG_I("Memory Stats for Client %d\n", inst->clientType);
  for (i = 0, tothw = 0; i < inst->refFrmChunks; i++) {
    tothw += inst->refFrmAlloc[i];
    MEM_LOG_I("      HW Memory Chunk %2d: %u\n", i,
              inst->refFrmAlloc[i]);
  }
  MEM_LOG_I("Total HW Memory:   %u\n", tothw);
  for (i = 0, totswhw = 0; i < inst->linMemChunks; i++) {
    totswhw += inst->linMemAlloc[i];
    MEM_LOG_I("   SW-HW Memory Chunk %2d: %u\n", i,
              inst->linMemAlloc[i]);
  }
  MEM_LOG_I("Total SWHW Memory: %u\n", totswhw);
  printf("Total  HW  Memory: %u\n", tothw);
  printf("Total SWHW Memory: %u\n", totswhw);
#else
  if (inst->fd_v4l2 != -1) close(inst->fd_v4l2);
#endif  //end of #ifndef V4l2
  free_nodes(inst->workers.tail);
  free_nodes(inst->freelist.tail);
  free(inst);
  pthread_mutex_lock(&instCounter_mutex);
  if (instCounter > 0) instCounter--;
  pthread_mutex_unlock(&instCounter_mutex);
  if (instCounter == 0) CoreEncShutdown();
  return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : EWLLinearMem_t *info - place where the allocated
                        memory buffer parameters are returned
------------------------------------------------------------------------------*/
static i32 EWLMallocLinearSys(const void *instance, u32 size, u32 alignment,
                              EWLLinearMem_t *info) {
#ifndef V4L2
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(instance != NULL);
  ASSERT(inst->linMemChunks < MEM_CHUNKS);
  ASSERT(inst->totalChunks < MEM_CHUNKS);
  alignment = MAX(alignment, LINMEM_ALIGN);
  i32 ret;

  if (((info->mem_type & VPU_RD) || (info->mem_type & VPU_WR)) &&
      ((info->mem_type & CPU_RD) == 0 && (info->mem_type & CPU_WR) == 0))
    inst->refFrmAlloc[inst->refFrmChunks++] = size;
  else
    inst->linMemAlloc[inst->linMemChunks++] = size;

  EWLTRACE_I((void *)instance, "EWLMallocLinear: %8d bytes (aligned %8ld)\n",
           size, NEXT_ALIGNED_SYS(size, alignment));

  size = NEXT_ALIGNED_SYS(size, alignment);

  inst->chunks[inst->totalChunks] = (u32 *)calloc(1, size + alignment);
  if (inst->chunks[inst->totalChunks] == NULL) return EWL_ERROR;

  inst->alignedChunks[inst->totalChunks] =
      (u32 *)NEXT_ALIGNED_SYS(inst->chunks[inst->totalChunks], alignment);
  ;
  EWLTRACE_I((void *)instance, "EWLMallocLinear: %p, aligned %p\n",
           (void *)inst->chunks[inst->totalChunks],
           (void *)inst->alignedChunks[inst->totalChunks]);

  MEM_LOG_I("    Malloc Linear for Client %+4x(Type %d, Chunk %2d),"
            " %12u Bytes, Attribute: %d, Usage: (%2d)%s\n", ((ptr_t)inst&0xffff),
            inst->clientType, inst->totalChunks, size,
            EWLGetMemAttribute(info->mem_type), EWLGetMemHint(info->mem_type),
            EWLGetMemUsage(info->mem_type));

  info->busAddress = (ptr_t)inst->alignedChunks[inst->totalChunks++];

  info->virtualAddress = NULL;
  ret = EWLMemSyncAllocHostBufferSys(inst, size, alignment, info);
  if (ret == EWL_NOT_SUPPORT)
    info->virtualAddress = (u32 *)info->busAddress;
  else if (ret == EWL_ERROR)
    return ret;
  info->size = info->total_size = size;
  return EWL_OK;

#else
  ewlSysInstance *enc_ewl = (ewlSysInstance *)instance;
  EWLLinearMem_t *buff = (EWLLinearMem_t *)info;
  ASSERT(enc_ewl != NULL);
  ASSERT(buff != NULL);
  EWLTRACE_I((void *)instance, "EWLMallocLinear\t%8d bytes\n", size);
  if (alignment == 0) alignment = 1;

  u32 pgsize = getpagesize();
  int err;
  vsi_v4l2_mem_info params;
  memset(&params, 0, sizeof(params));

  buff->size = buff->total_size =
      (((size + (alignment - 1)) & (~(alignment - 1))) + (pgsize - 1)) &
      (~(pgsize - 1));
  params.size = (size + (alignment - 1)) & (~(alignment - 1));

  buff->virtualAddress = 0;
  buff->busAddress = 0;
  buff->allocVirtualAddr = 0;
  buff->allocBusAddr = 0;

  err = ioctl(enc_ewl->fd_v4l2, VSI_IOCTL_CMD_ALLOC, &params);
  if (err < 0) {
    EWLTRACE_E((void *)instance, "EWLMallocLiner: failed to alloc mem.\n");
    return EWL_ERROR;
  }
  buff->allocVirtualAddr =
      (u32 *)mmap((void *)params.busaddr, params.size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, enc_ewl->fd_v4l2, (unsigned long long)params.id);
  if (buff->allocVirtualAddr == MAP_FAILED) {
    EWLTRACE_I((void *)instance, "EWLInit: Failed to mmap busAddress: %p\n",
             (void *)params.busaddr);
    return EWL_ERROR;
  }

  /* ASIC might be in different address space */
  buff->allocBusAddr =
      params
          .busaddr;  // BUS_CPU_TO_ASIC(params.busAddress, params.translation_offset);
  buff->busAddress = (buff->allocBusAddr + (alignment - 1)) &
                     (~(((u64)alignment) - 1));  //left allign

  buff->virtualAddress =
      buff->allocVirtualAddr + ((alignment - 1) & (~(((u64)alignment) - 1)));
  buff->busAddress =
      (ptr_t)buff->virtualAddress;  //For c-model, busAddress is virtualAddress
  buff->id = params.id;

  return EWL_OK;
#endif
}

/*------------------------------------------------------------------------------
    Function name   : EWLFreeLinear
    Description     : Release a linera memory buffer, previously allocated with
                        EWLMallocLinear.

    Return type     : void

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - linear buffer memory information
------------------------------------------------------------------------------*/
static void EWLFreeLinearSys(const void *instance, EWLLinearMem_t *info) {
#ifndef V4L2
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  u32 i;
  ASSERT(instance != NULL);

  if (info->busAddress != 0) {
    for (i = 0; i < inst->totalChunks; i++) {
      if (inst->alignedChunks[i] == (u32 *)info->busAddress) {
        EWLTRACE_I((void *)instance, "EWLFreeLinear busAddress \t%p\n", \
                 (void *)info->busAddress);
        MEM_LOG_I("    Free Linear for Client %+4x(Type %d, Chunk %2d),"
                  " %12u Bytes, Attribute: %d, Usage: (%2d)%s\n",
                  ((ptr_t)inst&0xffff), inst->clientType, i, info->total_size,
                  EWLGetMemAttribute(info->mem_type), EWLGetMemHint(info->mem_type),
                  EWLGetMemUsage(info->mem_type));

        free(inst->chunks[i]);
        inst->chunks[i] = NULL;
        inst->alignedChunks[i] = NULL;
        info->busAddress = 0;
        info->total_size = 0;
        info->size = 0;
        break;
      }
    }
  }

  EWLMemSyncFreeHostBufferSys(inst, info);
#else
  ewlSysInstance *enc_ewl = (ewlSysInstance *)instance;
  EWLLinearMem_t *buff = (EWLLinearMem_t *)info;
  vsi_v4l2_mem_info meminfo;

  ASSERT(enc_ewl != NULL);
  ASSERT(buff != NULL);

  meminfo.id = buff->id;

  if (buff->allocBusAddr != 0)
    ioctl(enc_ewl->fd_v4l2, VSI_IOCTL_CMD_FREE, &meminfo);

  if (buff->allocVirtualAddr != MAP_FAILED)
    munmap(buff->allocVirtualAddr, buff->total_size);

  EWLTRACE_I((void *)instance, "EWLFreeLinear\t%p\n", buff->allocVirtualAddr);

  //reset buffer information
  buff->allocBusAddr = 0;
  buff->allocVirtualAddr = NULL;
  buff->busAddress = 0;
  buff->virtualAddress = NULL;
  buff->total_size = 0;
  buff->size = 0;

#endif
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocRefFrm
    Description     : Allocate a frame buffer (contiguous linear RAM memory)

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : EWLLinearMem_t *info - place where the allocated memory
                        buffer parameters are returned
------------------------------------------------------------------------------*/
static i32 EWLMallocRefFrmSys(const void *instance, u32 size, u32 alignment,
                              EWLLinearMem_t *info) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(instance != NULL);
  ASSERT(inst->refFrmChunks < MEM_CHUNKS);
  ASSERT(inst->totalChunks < MEM_CHUNKS);
  i32 ret;

  ret = EWLMallocLinearSys(instance, size, alignment, info);

  EWLTRACE_I((void *)instance, "EWLMallocRefFrm\t%8d bytes\n", size);
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : EWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        EWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - frame buffer memory information
------------------------------------------------------------------------------*/
static void EWLFreeRefFrmSys(const void *instance, EWLLinearMem_t *info) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  u32 i;
  ASSERT(instance != NULL);

  if (info->busAddress != 0) {
    for (i = 0; i < inst->totalChunks; i++) {
      if (inst->alignedChunks[i] == (u32 *)info->busAddress) {
        EWLTRACE_I((void *)instance, "EWLFreeRefFrm busAddress \t%p\n",
                 (void *)info->busAddress);

        MEM_LOG_I("    Free Linear for Client %+4x(Type %d, Chunk %2d),"
                  " %12u Bytes, Attribute: %d, Usage: (%2d)%s\n",
                  ((ptr_t)inst&0xffff), inst->clientType, i, info->total_size,
                  EWLGetMemAttribute(info->mem_type), EWLGetMemHint(info->mem_type),
                  EWLGetMemUsage(info->mem_type));

        free(inst->chunks[i]);
        inst->chunks[i] = NULL;
        inst->alignedChunks[i] = NULL;
        info->busAddress = 0;
        info->total_size = 0;
        info->size = 0;
        break;
      }
    }
  }
  EWLMemSyncFreeHostBufferSys(inst, info);
}

static void EWLDCacheRangeFlushSys(const void *instance, EWLLinearMem_t *info) {
  ASSERT(instance != NULL);
  (void)instance;
  ASSERT(info != NULL);
  (void)info;
}

static void EWLDCacheRangeRefreshSys(const void *instance,
                                     EWLLinearMem_t *info) {
  ASSERT(instance != NULL);
  (void)instance;
  ASSERT(info != NULL);
  (void)info;
}

static u32 EWLSegMemCopy(ewlSysInstance *inst, SysCore *core, u32 *pWrCnt) {
  u8 *streamBase;
  u8 *streamTmpBase;
  u32 segmentSize = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SIZE);
  u32 segmentOffset = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_OFFSET);
  u32 memLength;

  if(*pWrCnt == 0) { // first segment
    printf("[size][offset][amount][length] = [%u][%u][%u][%u]\n",
    segmentSize, segmentOffset, inst->segmentAmount, inst->streamLength);
    // inst->streamLength -= segmentOffset;
  }
  streamBase = inst->streamBase + segmentSize * (*pWrCnt % inst->segmentAmount);
  streamTmpBase = inst->streamTempBuffer + segmentSize * (*pWrCnt);
  memLength = (inst->streamLength < segmentSize)? inst->streamLength : segmentSize;

  memcpy(streamBase, streamTmpBase, memLength);
  printf("----> simulate [%u]: %10p -> %10p copy %u Bytes, [%u] left!\n", *pWrCnt, streamTmpBase, \
    (void*)streamBase, memLength, inst->streamLength - memLength);
  CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR, ++(*pWrCnt));

  if (inst->streamLength > segmentSize) {
    printf("---->trigger segment IRQ\n");
    CoreEncSetRegisterValue(core, HWIF_ENC_FRAME_RDY_STATUS, 0);
    CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RDY_INT, 1);
  } else {
    printf("---->trigger frame ready IRQ\n");
    CoreEncSetRegisterValue(core, HWIF_ENC_FRAME_RDY_STATUS, 1);
    CoreEncSetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RDY_INT, 0);
  }
  inst->streamLength = inst->streamLength - memLength;

  return memLength;
}


/*------------------------------------------------------------------------------
    Function name   : EWLSegIrqProc
    Description     : simulation for Segment mode > 0

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : SysCore *core - EWL instance->core[CORE(core_id)]
------------------------------------------------------------------------------*/
static i32 EWLSegIrqProc(const void *instance, SysCore *core) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  // u32 core_id = FIRST_CORE(inst);
  // SysCore *core = inst->core[CORE(core_id)];
  u32 memLength=0;

  if (inst->frameRdy == -1)
    inst->frameRdy = (CoreEncGetRegister(core, HSWREG(1)) & 0x04) == 0x04;

  if (inst->frameRdy == 0) return -1;

  // only for mode=2, using rd|wr in this mode only
  if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SW_SYNC_EN) == 1) {
    u32 rd = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RD_PTR);
    u32 wr = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR);
    if(wr > 0 && (inst->streamLength == 0))
      return 0;

    if (inst->streamLength == 0)
      inst->streamLength = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);

    if (wr >= rd && wr - rd < inst->segmentAmount) {
      EWLSegMemCopy(inst, core, &wr);
    }
  } else {  //mode=1 or mode=3
    if (inst->segmentRdCnt > 0 && (inst->streamLength == 0))
      return 0;

    u32 hwOutStreamLength = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
    // if (((hwOutStreamLength+segmentOffset)/segmentSize) < inst->segmentRdCnt)
    //   return 0;

    memLength = EWLSegMemCopy(inst, core, &inst->segmentRdCnt);
    CoreEncSetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT, (hwOutStreamLength + memLength));
  }

  return 0;
}

static i32 EWLWaitHwRdySys(const void *instance, u32 *slicesReady,
                           void *waitOut, u32 *status_register) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  u32 core_id = FIRST_CORE(inst);
  SysCore *core = inst->core[CORE(core_id)];
  u32 idx;

  // if (!(CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_HEVC ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_H264 ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_AV1 ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_VP9 ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_CUTREE)) {
  //u32 enc_mode = CoreEncGetRegisterValue(core, HWIF_ENC_MODE);
  if (inst->clientType == EWL_CLIENT_TYPE_CUTREE){
    CoreEncWaitHwRdy(inst->core[CORE(core_id)]);
    *status_register = EWLReadRegSys(inst, HSWREG(1));
#ifdef VIRTUAL_PLATFORM_TEST
      goto vdk_test;
#endif
    return EWL_OK;
  }

  pthread_mutex_lock(&ewl_mutex);

  for (int i = 0; i < MAX_CORE_NUM; i++) {
    if (!subSysOut[i].has_irq) continue;

    core = inst->core[i];

    /* when buffer full exception happens, will skip signal
     * simulation */
    if (1==CoreEncGetRegisterValue(core, HWIF_ENC_BUFFER_FULL))
      goto end;

    /*simulate stream segment interrupt when frame_rdy_irq is triggered*/
    if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_EN)) {
      if(EWLSegIrqProc(inst, core) < 0)
        goto end;
    }
    CoreEncWaitHwRdy(inst->core[CORE(core_id)]);
  end:
    if (inst->clientType == EWL_CLIENT_TYPE_JPEG_ENC) {
      *status_register = EWLReadRegSys(inst, HSWREG(1));
      pthread_mutex_unlock(&ewl_mutex);
      return EWL_OK;
  }

    idx = ((EWLCoreWaitOut_t *)waitOut)->irq_num;
    ((EWLCoreWaitOut_t *)waitOut)->irq_status[idx] =
        CoreEncGetRegister(core, HSWREG(1));
    ((EWLCoreWaitOut_t *)waitOut)->job_id[idx] = subSysOut[i].job_id;
    ((EWLCoreWaitOut_t *)waitOut)->irq_num++;
  }
  pthread_mutex_unlock(&ewl_mutex);

#ifdef VIRTUAL_PLATFORM_TEST
vdk_test:
  EWLVirtualPlatformTestOut((void *)inst);
#endif

  return EWL_OK;
}

static i32 EWLHwSysDone(const void *instance) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  u32 core_id = FIRST_CORE(inst);
  SysCore *core = inst->core[CORE(core_id)];
  u32 memLength = 0;

  if(CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_CUTREE) return EWL_OK;
  // if (!(CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_HEVC ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_H264 ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_AV1 ||
  //       CoreEncGetRegisterValue(core, HWIF_ENC_MODE) == ASIC_VP9))
  //   return EWL_OK;

  pthread_mutex_lock(&ewl_mutex);

  subSysOut[CORE(core_id)].has_irq = 1;

  subSysOut[CORE(core_id)].job_id = reserved_job_id[CORE(core_id)];
  subSysOut[CORE(core_id)].total_slice_num =
      CoreEncGetRegisterValue(core, HWIF_ENC_NUM_SLICES_READY);

  /*simulate stream segment interrupt when frame_rdy_irq is triggered*/
  if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_EN)) {
    if (inst->frameRdy == -1)
      inst->frameRdy = (CoreEncGetRegister(core, HSWREG(1)) & 0x04) == 0x04;

    if (inst->frameRdy == 0) goto end;

    // init get totally buffer size length
    if (inst->streamLength == 0)
      inst->streamLength = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);

    u32 segmentSize = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SIZE);
    u32 segmentOffset = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_OFFSET);
    // printf("[size][offset][amount][length] = [%d][%d][%d][%d]\n",
    //   segmentSize, segmentOffset, inst->segmentAmount, inst->streamLength);

    // for header size
    // segmentSize -= segmentOffset;
    // streamBase += segmentOffset;

      // only for mode=2, using rd|wr in this mode only
    if (CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_SW_SYNC_EN) == 1) {
      u32 rd = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_RD_PTR);
      u32 wr = CoreEncGetRegisterValue(core, HWIF_ENC_STRM_SEGMENT_WR_PTR);
      if(wr > 0 && (inst->streamLength == 0))
        return 0;

      if (inst->streamLength == 0)
        inst->streamLength = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);

      if (wr >= rd && wr - rd < inst->segmentAmount) {
        EWLSegMemCopy(inst, core, &wr);
      }
    } else {  //mode=1 or mode=3
      if (inst->segmentRdCnt > 0 && (inst->streamLength == 0))
        return 0;

      u32 hwOutStreamLength = CoreEncGetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
      //check buffer limit is not too big
      if (((hwOutStreamLength + segmentOffset)/segmentSize) < inst->segmentRdCnt)
        return 0;

      memLength = EWLSegMemCopy(inst, core, &inst->segmentRdCnt);
      CoreEncSetRegisterValue(core, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT, memLength);
    }

  }

end:

  pthread_mutex_unlock(&ewl_mutex);

  return EWL_OK;
}

static i32 EWLReserveHwSys(const void *inst, u32 *core_info, u32 *job_id) {
  ASSERT(inst != NULL);

  ewlSysInstance *instance = (ewlSysInstance *)inst;
  i32 core_num = EWLGetCoreNumSys(NULL);
  u32 core_bits = 0;
  i32 ret, core_idx;
  u32 client_type = instance->clientType;
  const EWLHwConfig_t *hw_cfg = NULL;
  static u32 jobId = 0;

  (void)inst;
  EWLTRACE_I((void *)instance, "EWLReserveHw\n");
  EWLWorker *worker;
  while (1) {
    pthread_mutex_lock(&ewl_mutex);
    worker = (EWLWorker *)queue_get(&instance->freelist);
    pthread_mutex_unlock(&ewl_mutex);
    if (worker == NULL)
      usleep(10);
    else
      break;
  }

  for (core_idx = 0; core_idx < core_num; core_idx++) {
    hw_cfg = EWLReadAsicConfigSys(core_idx, NULL);
    if (EWL_SUPPORT_CLIENT(hw_cfg, client_type) || \
        EWL_SUPPORT_CUTREE(hw_cfg, client_type)) {
      core_bits |= 1 << core_idx;
      continue;
    }
  }

  core_idx = 0;
  while (1) {
    if ((core_bits >> core_idx) & 0x1) {
      ret = CoreEncTryReserveHw(instance->core[core_idx]);
      if (ret == 0) {
        EWLTRACE_I((void *)instance, "Reserve: inst %p %p %p, core=%d\n",
                 instance->winst.hevc, instance->winst.jpeg,
                 instance->winst.cutree, core_idx);
        break;
      } else {
        usleep(10);
      }
    }
    core_idx = (core_idx + 1) % core_num;
  }
  worker->core_id = COREID(NODE(worker->core_id), core_idx);

  pthread_mutex_lock(&ewl_mutex);
  //queue_remove(&instance->freelist, (struct node *)worker);
  queue_put(&instance->workers, (struct node *)worker);
  showWorkers("Reserved:", &instance->workers, &instance->freelist);
  pthread_mutex_unlock(&ewl_mutex);

  if (EWL_IS_VIDEO_CLIENT(instance->clientType)) {
    pthread_mutex_lock(&ewl_mutex);
    reserved_job_id[core_idx] = jobId;
    *job_id = jobId++;
    pthread_mutex_unlock(&ewl_mutex);
  }

  return EWL_OK;
}

static void EWLReleaseHwSys(const void *inst) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  ASSERT(inst != NULL);
  (void)inst;
  EWLTRACE_I((void *)inst, "EWLReleaseHw\n");
  instance->performance =
      EWLReadRegSys(inst, 82 * 4);  //save the performance before release hw
  pthread_mutex_lock(&ewl_mutex);
  EWLWorker *worker = (EWLWorker *)queue_get(&instance->workers);
  pthread_mutex_unlock(&ewl_mutex);
  if (worker == NULL) return;
  if(worker->core_id >= ARRAY_SIZE(subSysOut)) return;

  if (EWL_IS_VIDEO_CLIENT(instance->clientType)) {
    pthread_mutex_lock(&ewl_mutex);
    subSysOut[CORE(worker->core_id)].has_irq = 0;
    subSysOut[CORE(worker->core_id)].slice_rdy_num = 0;
    subSysOut[CORE(worker->core_id)].total_slice_num = 0;
    subSysOut[CORE(worker->core_id)].irq_status = 0;
    pthread_mutex_unlock(&ewl_mutex);
  }

  CoreEncReleaseHw(instance->core[CORE(worker->core_id)]);

  pthread_mutex_lock(&ewl_mutex);
  //queue_remove(&instance->workers, (struct node *)worker);
  queue_put(&instance->freelist, (struct node *)worker);
  showWorkers("Released:", &instance->workers, &instance->freelist);
  pthread_mutex_unlock(&ewl_mutex);
  EWLTRACE_I((void *)inst, "Release: inst %p %p %p, core=%x\n",
           instance->winst.hevc, instance->winst.jpeg, instance->winst.cutree,
           worker->core_id);
}

/* use slice 0 because slice node is symitrical */
static u32 EWLGetCoreNumSys(const void *ctx) { return CoreEncGetCoreNum(0); }

static void EWLClearTraceProfileSys(const void *instance) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;

  inst->prof.frame_number = 0;
  inst->prof.total_bits = 0;
  inst->prof.total_Y_PSNR = 0.0;
  inst->prof.total_U_PSNR = 0.0;
  inst->prof.total_V_PSNR = 0.0;
  inst->prof.Y_PSNR_Max = -1;
  inst->prof.Y_PSNR_Min = 1000.0;
  inst->prof.U_PSNR_Max = -1;
  inst->prof.U_PSNR_Min = 1000.0;
  inst->prof.V_PSNR_Max = -1;
  inst->prof.V_PSNR_Min = 1000.0;
  inst->prof.total_ssim = 0.0;
  inst->prof.total_ssim_y = 0.0;
  inst->prof.total_ssim_u = 0.0;
  inst->prof.total_ssim_v = 0.0;
  inst->prof.QP_Max = 0;
  inst->prof.QP_Min = 52;
}

/*------------------------------------------------------------------------------
    Function name   : EWLTraceProfile
    Description     : print the PSNR and SSIM data, only valid for c-model.
    Return type     : void
    Argument        : none
------------------------------------------------------------------------------*/
static void EWLTraceProfileSys(const void *instance, void *prof_data, i32 qp,
                               i32 poc) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  float bit_rate_avg;
  float Y_PSNR_avg;
  float U_PSNR_avg;
  float V_PSNR_avg;
  //u32 core_id = LAST_CORE(inst);
  //SysCore *core = inst->core[CORE(core_id)];

  //i32 qp;

  // MUST be the same as the struct in "pictur".
  struct {
    i32 bitnum;
    float psnr_y, psnr_u, psnr_v;
    double ssim;
    double ssim_y, ssim_u, ssim_v;
  } prof;

  //void *prof_data;
  //i32 poc;

  //prof_data = (void*)CoreEncGetAddrRegisterValue(core, HWIF_ENC_COMPRESSEDCOEFF_BASE);
  //qp = CoreEncGetRegisterValue(core, HWIF_ENC_PIC_QP);
  //poc = CoreEncGetRegisterValue(core, HWIF_ENC_POC);

  memcpy(&prof, prof_data, sizeof(prof));

  if (inst->prof.QP_Max < qp) inst->prof.QP_Max = qp;
  if (inst->prof.QP_Min > qp) inst->prof.QP_Min = qp;
  if (inst->prof.Y_PSNR_Max < prof.psnr_y) inst->prof.Y_PSNR_Max = prof.psnr_y;
  if (inst->prof.Y_PSNR_Min > prof.psnr_y) inst->prof.Y_PSNR_Min = prof.psnr_y;
  if (inst->prof.U_PSNR_Max < prof.psnr_u) inst->prof.U_PSNR_Max = prof.psnr_u;
  if (inst->prof.U_PSNR_Min > prof.psnr_u) inst->prof.U_PSNR_Min = prof.psnr_u;
  if (inst->prof.V_PSNR_Max < prof.psnr_v) inst->prof.V_PSNR_Max = prof.psnr_v;
  if (inst->prof.V_PSNR_Min > prof.psnr_v) inst->prof.V_PSNR_Min = prof.psnr_v;
  inst->prof.frame_number++;
  inst->prof.total_bits += prof.bitnum;
  bit_rate_avg = (inst->prof.total_bits / inst->prof.frame_number) * 30;
  inst->prof.total_Y_PSNR += prof.psnr_y;
  Y_PSNR_avg = inst->prof.total_Y_PSNR / inst->prof.frame_number;
  inst->prof.total_U_PSNR += prof.psnr_u;
  U_PSNR_avg = inst->prof.total_U_PSNR / inst->prof.frame_number;
  inst->prof.total_V_PSNR += prof.psnr_v;
  V_PSNR_avg = inst->prof.total_V_PSNR / inst->prof.frame_number;

  inst->prof.total_ssim += prof.ssim;
  inst->prof.total_ssim_y += prof.ssim_y;
  inst->prof.total_ssim_u += prof.ssim_u;
  inst->prof.total_ssim_v += prof.ssim_v;

  printf(
      "    CModel::POC %3d QP %3d %9d bits [Y %.4f dB  U %.4f dB  V %.4f dB] "
      "[SSIM %.4f average_SSIM %.4f] [SSIM Y %.4f U %.4f V %.4f average_SSIM Y "
      "%.4f U %.4f V %.4f]\n",
      poc, qp, prof.bitnum, prof.psnr_y, prof.psnr_u, prof.psnr_v, prof.ssim,
      inst->prof.total_ssim / inst->prof.frame_number, prof.ssim_y, prof.ssim_u,
      prof.ssim_v, inst->prof.total_ssim_y / inst->prof.frame_number,
      inst->prof.total_ssim_u / inst->prof.frame_number,
      inst->prof.total_ssim_v / inst->prof.frame_number);
  printf(
      "    CModel::POC %3d QPMin/QPMax %d/%d Y_PSNR_Min/Max %.4f/%.4f dB "
      "U_PSNR_Min/Max %.4f/%.4f dB V_PSNR_Min/Max %.4f/%.4f \n",
      poc, inst->prof.QP_Min, inst->prof.QP_Max, inst->prof.Y_PSNR_Min,
      inst->prof.Y_PSNR_Max, inst->prof.U_PSNR_Min, inst->prof.U_PSNR_Max,
      inst->prof.V_PSNR_Min, inst->prof.V_PSNR_Max);
  printf(
      "    CModel::POC %3d frame %d Y_PSNR_avg %.4f dB  U_PSNR_avg %.4f dB "
      "V_PSNR_avg %.4f dB \n",
      poc, inst->prof.frame_number - 1, Y_PSNR_avg, U_PSNR_avg, V_PSNR_avg);
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetLineBufSram
    Description        : Get the base address of on-chip sram used for input MB line buffer.

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the sram parameters are returned
------------------------------------------------------------------------------*/
static i32 EWLGetLineBufSramSys(const void *instance, EWLLinearMem_t *info) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  ASSERT(info != NULL);

  info->virtualAddress = NULL;
  info->busAddress = 0;
  info->size = 0;

  return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocLoopbackLineBuf
    Description        : allocate loopback line buffer in memory, mainly used when there is no on-chip sram

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the mem parameters are returned
------------------------------------------------------------------------------*/
static i32 EWLMallocLoopbackLineBufSys(const void *instance, u32 size,
                                       EWLLinearMem_t *info) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  ASSERT(info != NULL);

  info->virtualAddress = NULL;
  info->busAddress = 0;
  info->size = 0;

  return EWL_OK;
}

#ifdef VIRTUAL_PLATFORM_TEST
static void EWLVirtualPlatformTestIn(void *instance) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  u32 core_id = LAST_CORE(inst);
  SysCore *core = inst->core[CORE(core_id)];
  SysBufferInfo BufInfo;
  int ret = 0;
  int i = 0, k = 0, VPBufNum = 0;
  u32 BufLength = 0;
  u32 *tmp_addr = NULL;
  u32 *src_addr = NULL;
  ptr_t addr_lsb, addr_msb;

  for (i = 0, VPBufNum = 0; i < ASIC_SWREG_AMOUNT; i++) {
    ret = CoreEncGetMemoryDescription(core, i, 0, &BufInfo);
    if (ret == 0) {
      addr_lsb = CoreEncGetRegister(core, i * 4);
      addr_msb = CoreEncGetRegister(core, BufInfo.reg_msb_idx * 4);
      if (sizeof(ptr_t) == 8) {
        src_addr = (u32 *)((addr_msb << 32) | addr_lsb);

      } else {
        src_addr = (u32 *)addr_lsb;
      }
      inst->CmodelUseOrigBuf[VPBufNum] = src_addr;
      if (src_addr == NULL) {
        //printf("Note: With current case in buffer, address register index %d is not be used\n", i);
        VPBufNum++;
        continue;
      }
      BufLength = BufInfo.stride * BufInfo.height;
      if (i != 60 && i != 186 && i != 449) {
        BufLength = NEXT_ALIGNED(BufLength);
        BufLength += LINMEM_ALIGN;
      }
      inst->VirtualPlatformBuf[VPBufNum] = (u32 *)calloc(1, BufLength);
      if (sizeof(ptr_t) == 8) {
        core->asicRegs[i] =
            (ptr_t)inst->VirtualPlatformBuf[VPBufNum] & 0xffffffff;
        core->asicRegs[BufInfo.reg_msb_idx] =
            ((ptr_t)inst->VirtualPlatformBuf[VPBufNum] >> 32) & 0xffffffff;
      } else {
        core->asicRegs[i] =
            (ptr_t)inst->VirtualPlatformBuf[VPBufNum] & 0xffffffff;
      }
      /* copy input buffer data */
      tmp_addr = inst->VirtualPlatformBuf[VPBufNum];
      if (BufInfo.flag & ENC_IN) {  //IN
        for (k = 0; k < BufInfo.height; k++) {
          EWLmemcpy(tmp_addr, src_addr, BufInfo.width);
          tmp_addr = (u32 *)((u8 *)tmp_addr + BufInfo.stride);
          src_addr = (u32 *)((u8 *)src_addr + BufInfo.stride);
        }
      }
      VPBufNum++;
    }
  }
}

static void EWLVirtualPlatformTestOut(void *instance) {
  ewlSysInstance *inst = (ewlSysInstance *)instance;
  ASSERT(inst != NULL);
  u32 core_id = FIRST_CORE(inst);
  SysCore *core = inst->core[CORE(core_id)];
  SysBufferInfo BufInfo;
  int ret = 0;
  int i = 0, k = 0, VPBufNum = 0;
  u32 BufLength = 0;
  u32 *tmp_addr = NULL;
  u32 *src_addr = NULL;
  ptr_t addr_lsb, addr_msb;

  for (i = 0, VPBufNum = 0; i < ASIC_SWREG_AMOUNT; i++) {
    ret = CoreEncGetMemoryDescription(core, i, 1, &BufInfo);
    if (ret == 0) {
      addr_lsb = CoreEncGetRegister(core, i * 4);
      addr_msb = CoreEncGetRegister(core, BufInfo.reg_msb_idx * 4);
      if (sizeof(ptr_t) == 8) {
        tmp_addr = (u32 *)((addr_msb << 32) | addr_lsb);
      } else {
        tmp_addr = (u32 *)addr_lsb;
      }
      src_addr = inst->CmodelUseOrigBuf[VPBufNum];
      if (src_addr == NULL) {
        VPBufNum++;
        //printf("Note: With current case out buffer, address register index %d is not be used\n", i);
        continue;
      }
      //copy input buffer data
      if (BufInfo.flag & ENC_OUT) {  //OUT
        for (k = 0; k < BufInfo.height; k++) {
          EWLmemcpy(src_addr, tmp_addr, BufInfo.width);
          tmp_addr = (u32 *)((u8 *)tmp_addr + BufInfo.stride);
          src_addr = (u32 *)((u8 *)src_addr + BufInfo.stride);
        }
      }
      if (sizeof(ptr_t) == 8) {
        core->asicRegs[i] =
            (ptr_t)inst->CmodelUseOrigBuf[VPBufNum] & 0xffffffff;
        core->asicRegs[BufInfo.reg_msb_idx] =
            ((ptr_t)inst->CmodelUseOrigBuf[VPBufNum] >> 32) & 0xffffffff;
      } else {
        core->asicRegs[i] =
            (ptr_t)inst->CmodelUseOrigBuf[VPBufNum] & 0xffffffff;
      }
      if (inst->VirtualPlatformBuf[VPBufNum]) {
        free(inst->VirtualPlatformBuf[VPBufNum]);
        inst->VirtualPlatformBuf[VPBufNum] = NULL;
      }
      VPBufNum++;
    }
  }
}
#endif

static void EWLSetReserveBaseDataSys(const void *inst, u64 interrupt_ctrl,
                                     u32 client_type) {}

static u32 EWLReadRegInitSys(const void *inst, u32 offset) { return EWL_OK; }

static i32 EWLReserveCmdbufSys(const void *inst, EWLResource_t *resource) {
  return EWL_OK;
}

static i32 EWLLinkRunCmdbufSys(const void *inst, u16 cmdbufid,
                               u16 cmdbuf_size) {
  return EWL_OK;
}

static i32 EWLWaitCmdbufSys(const void *inst, u16 cmdbufid, u32 *status) {
  return EWL_OK;
}

static void EWLGetRegsByCmdbufSys(const void *inst, u16 cmdbufid,
                                  u32 *regMirror) {
  (void)inst;
  (void)cmdbufid;
  (void)regMirror;
}

static i32 EWLReleaseCmdbufSys(const void *inst, u16 cmdbufid) {
  return EWL_OK;
}

void EWLSetVCMDModeSys(const void *inst, u32 mode) {
  (void)inst;
  (void)mode;
}

u32 EWLGetVCMDModeSys(const void *inst) {
  (void)inst;
  return 0;
}

i32 EWLGetVcmdCoreNumSys(const void *ctx, CLIENT_TYPE client_type) {
  (void)ctx;
  (void)client_type;
  return 0;
}

void EWLReadVcmdPrioritySys(const void *ctx, u32 *priority, u32 client_type) {
  (void)ctx;
  (void)priority;
  (void)client_type;
}

u32 EWLGetVCMDSupportSys() { return 0; }

static char *EWLGetDevNameSys(const void *inst) {
  (void)inst;
  return NULL;
}

static char *EWLGetMemDevNameSys(const void *inst) {
  (void)inst;
  return NULL;
}

u32 EWLReleaseEwlWorkerInstSys(const void *inst) {
  ewlSysInstance *instance = (ewlSysInstance *)inst;
  if (instance->winst.hevc != NULL) {
    free(instance->winst.hevc);
    instance->winst.hevc = NULL;
  }
  if (instance->winst.jpeg != NULL) {
    free(instance->winst.jpeg);
    instance->winst.jpeg = NULL;
  }
  if (instance->winst.cutree != NULL) {
    free(instance->winst.cutree);
    instance->winst.cutree = NULL;
  }
  return 0;
}

i32 EWLGetConfigRegisterSys(const void *inst, u32 core_id, u32 client_type, u32 offset) {
  if(client_type == EWL_CLIENT_TYPE_DEC400) {
    return 0x563;
  }
  return 0;
}

void EWLAttachSys(EWLFun *EWLFunP) {
  EWLFunP->EWLReadAsicIDP = EWLReadAsicIDSys;
  EWLFunP->EWLReadAsicConfigP = EWLReadAsicConfigSys;
  EWLFunP->EWLReadRegP = EWLReadRegSys;
  EWLFunP->EWLWriteCoreRegP = EWLWriteCoreRegSys;
  EWLFunP->EWLWriteRegP = EWLWriteRegSys;
  EWLFunP->EWLWriteBackRegP = EWLWriteBackRegSys;
  EWLFunP->EWLWriteRegbyClientTypeP = EWLWriteRegbyClientTypeSys;
  EWLFunP->EWLGetClientTypeP = EWLGetClientTypeSys;
  EWLFunP->EWLGetCoreTypeByClientTypeP = EWLGetCoreTypeByClientTypeSys;
  EWLFunP->EWLCheckCutreeValidP = EWLCheckCutreeValidSys;
  EWLFunP->EWLEnableHWP = EWLEnableHWSys;
  EWLFunP->EWLDisableHWP = EWLDisableHWSys;
  EWLFunP->EWLGetPerformanceP = EWLGetPerformanceSys;
  EWLFunP->EWLGetDec400CoreidP = EWLGetDec400CoreidSys;
  EWLFunP->EWLGetDec400AttributeP = EWLGetDec400AttributeSys;
  EWLFunP->EWLInitP = EWLInitSys;
  EWLFunP->EWLReleaseP = EWLReleaseSys;
  //EWLFunP->EwlReleaseCoreWaitP = EwlReleaseCoreWaitSys;
  //EWLFunP->EWLDequeueCoreOutJobP = EWLDequeueCoreOutJobSys;
  //EWLFunP->EWLEnqueueWaitjobP = EWLEnqueueWaitjobSys;
  //EWLFunP->EWLPutJobtoPoolP = EWLPutJobtoPoolSys;
  EWLFunP->EWLMallocRefFrmP = EWLMallocRefFrmSys;
  EWLFunP->EWLFreeRefFrmP = EWLFreeRefFrmSys;
  EWLFunP->EWLMallocLinearP = EWLMallocLinearSys;
  EWLFunP->EWLFreeLinearP = EWLFreeLinearSys;
  EWLFunP->EWLSyncMemDataP = EWLSyncMemDataSys;
  EWLFunP->EWLMemSyncAllocHostBufferP = EWLMemSyncAllocHostBufferSys;
  EWLFunP->EWLMemSyncFreeHostBufferP = EWLMemSyncFreeHostBufferSys;
  EWLFunP->EWLDCacheRangeFlushP = EWLDCacheRangeFlushSys;
  EWLFunP->EWLDCacheRangeRefreshP = EWLDCacheRangeRefreshSys;
  EWLFunP->EWLWaitHwRdyP = EWLWaitHwRdySys;
  EWLFunP->EWLReserveHwP = EWLReserveHwSys;
  EWLFunP->EWLReleaseHwP = EWLReleaseHwSys;
  EWLFunP->EWLGetCoreNumP = EWLGetCoreNumSys;
  EWLFunP->EWLTraceProfileP = EWLTraceProfileSys;
  EWLFunP->EWLGetLineBufSramP = EWLGetLineBufSramSys;
  EWLFunP->EWLMallocLoopbackLineBufP = EWLMallocLoopbackLineBufSys;

  //empty function
  EWLFunP->EWLSetReserveBaseDataP = EWLSetReserveBaseDataSys;
  EWLFunP->EWLReadRegInitP = EWLReadRegInitSys;
  EWLFunP->EWLReserveCmdbufP = EWLReserveCmdbufSys;
  EWLFunP->EWLLinkRunCmdbufP = EWLLinkRunCmdbufSys;
  EWLFunP->EWLWaitCmdbufP = EWLWaitCmdbufSys;
  EWLFunP->EWLGetRegsByCmdbufP = EWLGetRegsByCmdbufSys;
  EWLFunP->EWLReleaseCmdbufP = EWLReleaseCmdbufSys;
  EWLFunP->EWLGetVCMDSupportP = EWLGetVCMDSupportSys;
  EWLFunP->EWLSetVCMDModeP = EWLSetVCMDModeSys;
  EWLFunP->EWLGetVCMDModeP = EWLGetVCMDModeSys;
  EWLFunP->EWLReleaseEwlWorkerInstP = EWLReleaseEwlWorkerInstSys;
  EWLFunP->EWLClearTraceProfileP = EWLClearTraceProfileSys;
  EWLFunP->EWLGetVcmdCoreNumP = EWLGetVcmdCoreNumSys;
  EWLFunP->EWLReadVcmdPriorityP = EWLReadVcmdPrioritySys;
  EWLFunP->EWLGetDevNameP = EWLGetDevNameSys;
  EWLFunP->EWLGetMemDevNameP = EWLGetMemDevNameSys;
  EWLFunP->EWLGetConfigRegisterP = EWLGetConfigRegisterSys;
}
