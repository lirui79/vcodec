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
--  Abstract : Encoder Wrapper Layer, common parts
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "ewl.h"
#include "ewl_common.h"
#include "ewl_local.h"
#include "encswhwregisters.h"
#include "encdec400.h"
#include "enc_log.h"
#include "ewl_memsync.h"

#ifdef __FREERTOS__
#include "user_freertos.h"
#include "dev_common_freertos.h"
#include "memalloc_freertos.h"
#elif defined(__linux__)
#include "memalloc.h"
#else
#endif

#ifdef __FREERTOS__
//nothing
#elif defined(__linux__)
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#ifdef USE_EFENCE
#include "efence.h"
#endif

/* macro to convert CPU bus address to ASIC bus address */
#ifdef PC_PCI_FPGA_DEMO
//#define BUS_CPU_TO_ASIC(address)    (((address) & (~0xff000000)) | SDRAM_LM_BASE)
//#define BUS_CPU_TO_ASIC(address, offset) (((address) - (offset)) | SDRAM_LM_BASE)
#define BUS_CPU_TO_ASIC(address, offset) ((address) - (offset))
#else
#define BUS_CPU_TO_ASIC(address, offset) ((address) | SDRAM_LM_BASE)
#endif

volatile u32 asic_status;
static const char *synthLangName[3] = {"UNKNOWN", "VHDL", "VERILOG"};
/* Function to test input line buffer with hardware handshake, should be set by app. (invalid for system) */
u32 (*pollInputLineBufTestFunc)(void) = NULL;

extern pthread_mutex_t ewl_mutex;

#ifdef SUPPORT_MEM_STATISTIC
/*Count heap statistics */
EWLMemoryStatistics_t memoryInfo = {0};
static pthread_mutex_t linearBufferCountMutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t memoryCountMutex = PTHREAD_MUTEX_INITIALIZER;

void EwlShowMemoryStats() {
  MEM_LOG_I("Max Heap Memory:           %8d bytes\n",
            memoryInfo.maxHeapMemory);
  MEM_LOG_I("Max Heap Malloc Times:     %8d times\n",
            memoryInfo.maxHeapMallocTimes);
  MEM_LOG_I("Average Heap Malloc Times: %8d times\n",
            memoryInfo.averageHeapMallocTimes);

  MEM_LOG_I("Heap Max Linear Memory:    %8d bytes\n",
            memoryInfo.maxLinearMemory);
  MEM_LOG_I("Heap Mallc Linear Times:   %8d times\n",
            memoryInfo.maxLinearMallocTimes);
  MEM_LOG_I("Average Heap Malloc Times: %8d times\n",
            memoryInfo.averageLinearMallocTimes);
}
#endif /* SUPPORT_MEM_STATISTIC */

/* SW/SW shared memory */
/*------------------------------------------------------------------------------
    Function name   : EWLmalloc
    Description     : Allocate a memory block. Same functionality as
                      the ANSI C malloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

    Argument        : u32 n - Bytes to allocate
------------------------------------------------------------------------------*/
void *EWLmalloc(u32 n) {
  void *p = malloc((size_t)n);
  PTRACE_I("EWLmalloc\t%8d bytes --> %p\n", n, p);

#ifdef SUPPORT_MEM_STATISTIC
  int memorySize = malloc_usable_size(p);
  pthread_mutex_lock(&memoryCountMutex);
  memoryInfo.maxHeapMallocTimes++;
  memoryInfo.heapMallocTimes++;
  if (memoryInfo.averageHeapMallocTimes < memoryInfo.heapMallocTimes)
    memoryInfo.averageHeapMallocTimes = memoryInfo.heapMallocTimes;
  memoryInfo.memoryValue += memorySize;
  if (memoryInfo.maxHeapMemory < memoryInfo.memoryValue)
    memoryInfo.maxHeapMemory = memoryInfo.memoryValue;
  pthread_mutex_unlock(&memoryCountMutex);
#endif

  return p;
}

/*------------------------------------------------------------------------------
    Function name   : EWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()

    Return type     : void

    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void EWLfree(void *p) {
  PTRACE_I("EWLfree\t%p\n", p);

  if (p != NULL) {
#ifdef SUPPORT_MEM_STATISTIC
    int memorySize = malloc_usable_size(p);
    pthread_mutex_lock(&memoryCountMutex);
    memoryInfo.heapMallocTimes--;
    memoryInfo.memoryValue -= memorySize;
    pthread_mutex_unlock(&memoryCountMutex);
#endif
    free(p);
  }
}

/*------------------------------------------------------------------------------
    Function name   : EWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *EWLcalloc(u32 n, u32 s) {
  void *p = NULL;
#ifdef __FREERTOS__
  p = malloc((n) * (s));
  if (p) memset(p, 0, (n) * (s));
#else
  p = calloc((size_t)n, (size_t)s);
#endif

  PTRACE_I("EWLcalloc\t%8d bytes --> %p\n", n * s, p);

#ifdef SUPPORT_MEM_STATISTIC
  int memorySize = malloc_usable_size(p);
  pthread_mutex_lock(&memoryCountMutex);
  memoryInfo.maxHeapMallocTimes++;
  memoryInfo.heapMallocTimes++;
  if (memoryInfo.averageHeapMallocTimes < memoryInfo.heapMallocTimes)
    memoryInfo.averageHeapMallocTimes = memoryInfo.heapMallocTimes;
  memoryInfo.memoryValue += memorySize;
  if (memoryInfo.maxHeapMemory < memoryInfo.memoryValue)
    memoryInfo.maxHeapMemory = memoryInfo.memoryValue;
  pthread_mutex_unlock(&memoryCountMutex);
#endif

  return p;
}

/*------------------------------------------------------------------------------
    Function name   : EWLmemcpy
    Description     : Copies characters between buffers. Same functionality as
                      the ANSI C memcpy()

    Return type     : The value of destination d

    Argument        : void *d - Destination buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
mem_ret EWLmemcpy(void *d, const void *s, u32 n) {
  return memcpy(d, s, (size_t)n);
}

/*------------------------------------------------------------------------------
    Function name   : EWLmemset
    Description     : Sets buffers to a specified character. Same functionality
                      as the ANSI C memset()

    Return type     : The value of destination d

    Argument        : void *d - Pointer to destination
    Argument        : i32 c - Character to set
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
mem_ret EWLmemset(void *d, i32 c, u32 n) {
  return memset(d, (int)c, (size_t)n);
}

/*------------------------------------------------------------------------------
    Function name   : EWLmemcmp
    Description     : Compares two buffers. Same functionality
                      as the ANSI C memcmp()

    Return type     : Zero if the first n bytes of s1 match s2

    Argument        : const void *s1 - Buffer to compare
    Argument        : const void *s2 - Buffer to compare
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
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

#ifdef POLLING_ISR
static void polling_vcmd_isr(void *inst) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;
  long retVal;
  u16 core_id = 0xffff;  //0xffff means polling all cores

  while (1) {
    retVal = ioctl(enc->fd_enc, HANTRO_IOCH_POLLING_CMDBUF, &core_id);
    usleep(10000);  //10ms
    if (enc->wait_polling_break) break;
  }
}
#endif

int MapAsicRegisters(void *dev) {
  unsigned long base;
  unsigned int size;
  u32 *pRegs;
  vcx_cwl_t *enc = (vcx_cwl_t *)dev;
  u32 i;
  subsysReg *reg;
  SUBSYS_CORE_INFO info;
  u32 core_id;

  for (i = 0; i < EWLGetCoreNum(dev); i++) {
    reg = &enc->reg_all_cores[i];
    base = size = i;
    ioctl(enc->fd_enc, HANTRO_IOCG_HWOFFSET, &base);
    ioctl(enc->fd_enc, HANTRO_IOCG_HWIOSIZE, &size);

    /* map hw registers to user space */
    pRegs = (u32 *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                        enc->fd_enc, base);
    if (pRegs == MAP_FAILED) {
      PTRACE_E("EWLInit: Failed to mmap regs\n");
      return -1;
    }
    reg->pRegBase = pRegs;
    reg->regSize = size;
    reg->subsys_id = i;

    info.type_info = i;
    ioctl(enc->fd_enc, HANTRO_IOCG_CORE_INFO, &info);
    for (core_id = 0; core_id < CORE_MAX; core_id++) {
      if (info.type_info & (1 << core_id)) {
        u32 idx = ((core_id == CORE_VCEJ) ? CORE_VCE : core_id);
        reg->core[idx].core_id = idx;
        reg->core[idx].regSize = info.regSize[idx];
        reg->core[idx].regBase = base + info.offset[idx];
        reg->core[idx].pRegBase = (u32 *)((u8 *)pRegs + info.offset[idx]);
      } else
        reg->core[core_id].core_id = -1;
    }
    PTRACE_I("EWLInit: mmap regs %d bytes --> %p\n", size, pRegs);
  }
  return 0;
}

/* get the address and size of on-chip SRAM used for loopback linebuffer */
static i32 EWLInitLineBufSram(vcx_cwl_t *enc) {
  if (!enc) return EWL_ERROR;

  /* By default, VCE doesn't contain such on-chip SRAM.
    In the special case of FPGA verification for line buffer,
    there is a SRAM with some registers,
    used for loopback line buffer and emulation of HW handshake*/
  enc->lineBufSramBase = 0;
  enc->lineBufSramSize = 0;
  enc->pLineBufSram = MAP_FAILED;

#ifdef PCIE_FPGA_VERI_LINEBUF
  if (ioctl(enc->fd_enc, HANTRO_IOCG_SRAMOFFSET, &enc->lineBufSramBase) == -1) {
    PTRACE_E("ioctl HANTRO_IOCG_SRAMOFFSET failed\n");
    return EWL_ERROR;
  }
  if (ioctl(enc->fd_enc, HANTRO_IOCG_SRAMEIOSIZE, &enc->lineBufSramSize) ==
      -1) {
    PTRACE_E("ioctl HANTRO_IOCG_SRAMEIOSIZE failed\n");
    return EWL_ERROR;
  }

  /* map srame address to user space */
  enc->pLineBufSram =
      (u32 *)mmap(0, enc->lineBufSramSize, PROT_READ | PROT_WRITE, MAP_SHARED,
                  enc->fd_enc, enc->lineBufSramBase);
  if (enc->pLineBufSram == MAP_FAILED) {
    PTRACE_E("EWLInit: Failed to mmap SRAM Address!\n");
    return EWL_ERROR;
  }
  enc->lineBufSramBase = 0x3FE00000;
#endif

  return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetLineBufSram
    Description        : Get the base address of on-chip sram used for input line buffer.

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the sram parameters are returned
------------------------------------------------------------------------------*/
i32 EWLGetLineBufSram(const void *inst, EWLLinearMem_t *info) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;

  ASSERT(enc != NULL);
  ASSERT(info != NULL);

  if (enc->pLineBufSram != MAP_FAILED) {
    info->virtualAddress = (u32 *)enc->pLineBufSram;
    info->busAddress = enc->lineBufSramBase;
    info->size = enc->lineBufSramSize;
  } else {
    info->virtualAddress = NULL;
    info->busAddress = 0;
    info->size = 0;
  }

  PTRACE_I("EWLMallocLinear %p (ASIC) --> %p\n", (void *)info->busAddress,
           info->virtualAddress);
  return EWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : EWLMallocLoopbackLineBuf
    Description        : allocate loopback line buffer in memory, mainly used when there is no on-chip sram

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - EWL instance
    Argument        : EWLLinearMem_t *info - place where the mem parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocLoopbackLineBuf(const void *inst, u32 size, EWLLinearMem_t *info) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;
  EWLLinearMem_t *buff = (EWLLinearMem_t *)info;
  i32 ret;

  ASSERT(enc != NULL);
  ASSERT(buff != NULL);

  PTRACE_I("EWLMallocLoopbackLineBuf\t%8d bytes\n", size);

  ret = EWLMallocLinear(enc, size, 0, buff);

  PTRACE_I("EWLMallocLoopbackLineBuf %p --> %p\n", (void *)buff->busAddress,
           buff->virtualAddress);

  return ret;
}

/*******************************************************************************
 Function name   : EWLSetDevName
 Description     : Specify device name
 Argument        : -
*******************************************************************************/
static void EWLSetDevName(vcx_cwl_t *enc, char *dev_name) {
  char *name = ENC_MODULE_PATH;

  if (dev_name) {
    name = dev_name;
  }

  enc->enc_dev_n = EWLmalloc(strlen(name) + 1);
  if (enc->enc_dev_n) {
    strcpy(enc->enc_dev_n, name);
  }
}

static void EWLSetMemDevName(vcx_cwl_t *enc, char *dev_name) {
  char *name = MEMALLOC_MODULE_PATH;

  if (dev_name) {
    name = dev_name;
  }

  enc->mem_dev_n = EWLmalloc(strlen(name) + 1);
  if (enc->mem_dev_n) {
    strcpy(enc->mem_dev_n, name);
  }
}

/*******************************************************************************
 Function name   : EWLGetDevName
 Description     : Specify device name
 Argument        : -
*******************************************************************************/
char *EWLGetDevName(const void *inst) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;

  return enc->enc_dev_n;
}

char *EWLGetMemDevName(const void *inst) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;

  return enc->mem_dev_n;
}

u32 EWLGetClientType(const void *inst) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;

  return enc->clientType;
}

u32 EWLGetCoreTypeByClientType(u32 client_type) {
  u32 core_type;
  switch (client_type) {
    case EWL_CLIENT_TYPE_JPEG_ENC:
      core_type = CORE_VCE;
      break;
    case EWL_CLIENT_TYPE_CUTREE:
      core_type = CORE_CUTREE;
      break;
    case EWL_CLIENT_TYPE_DEC400:
      core_type = CORE_DEC400;
      break;
    case EWL_CLIENT_TYPE_L2CACHE:
      core_type = CORE_L2CACHE;
      break;
    case EWL_CLIENT_TYPE_AXIFE:
      core_type = CORE_AXIFE;
      break;
    case EWL_CLIENT_TYPE_AXIFE_1:
      core_type = CORE_AXIFE_1;
      break;
    case EWL_CLIENT_TYPE_APBFT:
      core_type = CORE_APBFT;
      break;
    case EWL_CLIENT_TYPE_UFBC:
      core_type = CORE_UFBC;
      break;
    default:
      core_type = CORE_VCE;
      break;
  }
  return core_type;
}

/*******************************************************************************
 Function name   : EWLCheckCutreeValid
 Description     : check it's cutree or not
*******************************************************************************/
i32 EWLCheckCutreeValid(const void *inst) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;
  u32 i = 0, val = 0;

  /* Check invalid parameters */
  if (enc == NULL) return EWL_ERROR;
  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    u32 core_id = LAST_CORE(enc);
    u32 core_type = EWLGetCoreTypeByClientType(enc->clientType);
    regMapping *reg = &(enc->reg_all_cores[core_id].core[core_type]);
    u32 hw_id = *(reg->pRegBase);

    if (hw_id == 0x90004200) {
      return EWL_OK;
    }
    else {
      val = *(reg->pRegBase + 287);
      if (val & 0x10000000)
        return EWL_OK;
      else
        return EWL_ERROR;
    }
  } else {
    return EWL_OK;
  }
}

u32 EWLGetVcmdModuleType(CLIENT_TYPE client) {
  u32 module_type = VCMD_TYPE_ENCODER;
  switch (client) {
  case EWL_CLIENT_TYPE_CUTREE:
    module_type = VCMD_TYPE_CUTREE;
    break;
  case EWL_CLIENT_TYPE_JPEG_ENC:
    module_type = VCMD_TYPE_ENCODER;
    break;
  default:
    module_type = VCMD_TYPE_ENCODER;
    break;
  }
  return module_type;
}

#ifdef VCMD_BUILD_SUPPORT
static i32 EWLGetVcmdParameters(int fd, u32 read_only,
                                CLIENT_TYPE client,
                                u32 **regs,
                                struct config_parameter *core_info,
                                struct cmdbuf_mem_parameter *cmdbuf_info) {
  u32 module_type = EWLGetVcmdModuleType(client);
  int flags;

  /* Get vcmd parameters */
  core_info->module_type = module_type;
  if (ioctl(fd, HANTRO_IOCH_GET_VCMD_PARAMETER, core_info)) {
    PTRACE_E("ioctl HANTRO_IOCH_GET_VCMD_PARAMETER failed\n");
    ASSERT(0);
    return EWL_ERROR;
  }

  if (core_info->vcmd_core_num == 0) {
    PTRACE_I("There is no proper vcmd  for encoder \n");
    return EWL_ERROR;
  }

  /* Get cmd-buf parameters */
  if (ioctl(fd, HANTRO_IOCH_GET_CMDBUF_PARAMETER, cmdbuf_info)) {
    PTRACE_E("ioctl HANTRO_IOCH_GET_CMDBUF_PARAMETER failed \n");
    return EWL_ERROR;
  }

  /* Mapping the cmd-bufs */
  flags = PROT_READ;
  cmdbuf_info->cmd_virt_addr = MAP_FAILED;
  if (!read_only) {
    flags = PROT_READ | PROT_WRITE;
    cmdbuf_info->cmd_virt_addr = (u32 *)mmap(
        0, cmdbuf_info->cmd_total_size, flags,
        MAP_SHARED, fd, cmdbuf_info->cmd_phy_addr);
    if (cmdbuf_info->cmd_virt_addr == MAP_FAILED) {
      PTRACE_E("EWLGetVcmdParameters: Failed to mmap vcmd buf\n");
      goto err;
    }
  }

  cmdbuf_info->status_virt_addr = (u32 *)mmap(
      0, cmdbuf_info->status_total_size, flags,
      MAP_SHARED, fd, cmdbuf_info->status_phy_addr);

  if (cmdbuf_info->status_virt_addr == MAP_FAILED) {
    PTRACE_E("EWLGetVcmdParameters: Failed to mmap vcmd status buf\n");
    goto err;
  }

  cmdbuf_info->reg_virt_addr = (u32 *)mmap(
      0, cmdbuf_info->reg_total_size, flags,
      MAP_SHARED, fd, cmdbuf_info->reg_phy_addr);

  if (cmdbuf_info->reg_virt_addr == MAP_FAILED) {
    PTRACE_E("EWLGetVcmdParameters: Failed to mmap vcmd reg buf\n");
    goto err;
  }

  *regs = cmdbuf_info->reg_virt_addr +
          cmdbuf_info->reg_unit_size / 4 * 0 +
          core_info->submodule_main_addr / 4;

  return EWL_OK;

err:
  if (cmdbuf_info->cmd_virt_addr != MAP_FAILED) {
    munmap(cmdbuf_info->cmd_virt_addr, cmdbuf_info->cmd_total_size);
    cmdbuf_info->cmd_virt_addr = MAP_FAILED;
  }
  if (cmdbuf_info->status_virt_addr != MAP_FAILED) {
    munmap(cmdbuf_info->status_virt_addr, cmdbuf_info->status_total_size);
    cmdbuf_info->status_virt_addr = MAP_FAILED;
  }
  if (cmdbuf_info->reg_virt_addr != MAP_FAILED) {
    munmap(cmdbuf_info->reg_virt_addr, cmdbuf_info->reg_total_size);
    cmdbuf_info->reg_virt_addr = MAP_FAILED;
  }
  return EWL_ERROR;
}
#endif

u32 EWLReadAsicID(u32 id, const void *ctx) {
  u32 hw_id = ~0;
  unsigned long base = ~0;
  unsigned int size;
  u32 *pRegs = MAP_FAILED;
  u32 core_num = 0;
  u32 *mapped_addr = MAP_FAILED;
  u32 mapped_size = 0;
  vcx_cwl_t *ewl_ctx = (vcx_cwl_t *)ctx;

  /* Check invalid parameters */
  ASSERT(ewl_ctx != NULL);

  if (ewl_ctx->vcmdEnable == 0) {
    SUBSYS_CORE_INFO info;
    u32 core_id = id;

    core_num = EWLGetCoreNum(ctx);
    if (core_id > core_num - 1) goto end;
    /* ask module for base */
    base = core_id;
    if (ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_HWOFFSET, &base) == -1) {
      PTRACE_E("ioctl failed\n");
      goto end;
    }
    size = core_id;
    if (ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_HWIOSIZE, &size) == -1) {
      PTRACE_E("ioctl failed\n");
      goto end;
    }
    /* map hw registers to user space */
    pRegs = (u32 *)mmap(0, size, PROT_READ, MAP_SHARED, ewl_ctx->fd_enc, base);
    if (pRegs == MAP_FAILED) {
      PTRACE_E("EWLReadAsicID: Failed to mmap regs\n");
      goto end;
    }
    mapped_addr = pRegs;
    mapped_size = size;
    info.type_info = core_id;
    if (ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_CORE_INFO, &info) == -1) {
      PTRACE_E("ioctl failed\n");
      goto end;
    }
    core_id = GET_ENCODER_IDX(info.type_info);
    hw_id = *((u32 *)((u8 *)pRegs + info.offset[core_id]));
  }
  #ifdef VCMD_BUILD_SUPPORT
  else {
    struct config_parameter vcmd_core_info;
    struct cmdbuf_mem_parameter vcmd_cmdbuf_info;
    u32 client_type = id;

    i32 ret;
    memset(&vcmd_core_info, 0, sizeof(vcmd_core_info));
    memset(&vcmd_cmdbuf_info, 0, sizeof(vcmd_cmdbuf_info));
    ret = EWLGetVcmdParameters(ewl_ctx->fd_enc, 1,
                               (CLIENT_TYPE)client_type,
                               &pRegs,
                               &vcmd_core_info,
                               &vcmd_cmdbuf_info);
    if (ret == EWL_ERROR) goto end;

    hw_id = pRegs[0];

    munmap(vcmd_cmdbuf_info.status_virt_addr, vcmd_cmdbuf_info.status_total_size);
    munmap(vcmd_cmdbuf_info.reg_virt_addr, vcmd_cmdbuf_info.reg_total_size);
  }
  #endif

  PTRACE_I("EWLReadAsicID: 0x%08x at 0x%08lx\n", hw_id, base);
end:
  if (mapped_addr != MAP_FAILED) {
    munmap(mapped_addr, mapped_size);
  }

  return hw_id;
}

const EWLHwConfig_t *EWLReadAsicConfig(u32 id, const void *ctx) {
  unsigned long base;
  unsigned int size;
  u32 *pRegs = MAP_FAILED, cfgval;
  u32 hw_id = ~0;
  const EWLHwConfig_t *cfg_info = NULL;
  SUBSYS_CORE_INFO info;
  u32 *mapped_addr = MAP_FAILED;
  u32 mapped_size;
  vcx_cwl_t *ewl_ctx = (vcx_cwl_t *)ctx;

  /* Check invalid parameters */
  ASSERT(ewl_ctx != NULL);

  if (ewl_ctx->vcmdEnable == 0) {
    u32 core_id = id;
    if (core_id > EWLGetCoreNum(ctx) - 1) goto end;

    base = core_id;
    size = core_id;
    ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_HWOFFSET, &base);
    ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_HWIOSIZE, &size);

    /* map hw registers to user space */
    pRegs = (u32 *)mmap(0, size, PROT_READ, MAP_SHARED, ewl_ctx->fd_enc, base);

    if (pRegs == MAP_FAILED) {
      PTRACE_E("EWLReadAsicConfig: Failed to mmap regs\n");
      goto end;
    }

    mapped_addr = pRegs;
    mapped_size = size;

    info.type_info = core_id;
    if (ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_CORE_INFO, &info) == -1) {
      PTRACE_E("ioctl failed\n");
      goto end;
    }

    core_id = GET_ENCODER_IDX(info.type_info);
    pRegs = (u32 *)((u8 *)pRegs + info.offset[core_id]);
    hw_id = pRegs[0];
    cfgval = pRegs[80];
  }
  #ifdef VCMD_BUILD_SUPPORT
  else {
    struct config_parameter vcmd_core_info;
    struct cmdbuf_mem_parameter vcmd_cmdbuf_info;
    memset(&vcmd_core_info, 0, sizeof(vcmd_core_info));
    memset(&vcmd_cmdbuf_info, 0, sizeof(vcmd_cmdbuf_info));
    vcmd_cmdbuf_info.status_virt_addr = MAP_FAILED;
    vcmd_cmdbuf_info.reg_virt_addr = MAP_FAILED;
    u32 client_type = id;

    i32 ret;
    ret = EWLGetVcmdParameters(ewl_ctx->fd_enc, 1,
                               (CLIENT_TYPE)client_type,
                               &pRegs,
                               &vcmd_core_info,
                               &vcmd_cmdbuf_info);
    if (ret == EWL_ERROR) goto end;

    hw_id = pRegs[0];
    cfgval = pRegs[80];

    mapped_addr = vcmd_cmdbuf_info.reg_virt_addr;
    mapped_size = vcmd_cmdbuf_info.reg_total_size;

    if(vcmd_cmdbuf_info.status_virt_addr != MAP_FAILED) {
      munmap(vcmd_cmdbuf_info.status_virt_addr,
             vcmd_cmdbuf_info.status_total_size);
    }
  }
  #endif

  EWLCoreSignature_t signature;
  if (EWL_OK != EWLGetCoreSignature(pRegs, &signature)) {
    PTRACE_E("ERROR when get the core signature.");
  }

  ASSERT(signature.hw_asic_id == hw_id);
  if (EWL_OK != EWLGetCoreConfig(&signature, &cfg_info)) {
    PTRACE_E("ERROR when get the core feature list.");
  }

end:
  if (mapped_addr != MAP_FAILED) {
    munmap(mapped_addr, mapped_size);
  }

  return cfg_info;
}

struct queue *EWLGetWorkers(const void *inst) {
  vcx_cwl_t *ewl = (vcx_cwl_t *)inst;
  return &ewl->workers;
}

void ewlSetUncheckPidFlag(const void *inst) {
  vcx_cwl_t *ewl = (vcx_cwl_t *)inst;
  ewl->unCheckPid=1;
}

const void *EWLInit(EWLInitParam_t *param) {
  vcx_cwl_t *enc = NULL;
  int i;

  PTRACE_I("EWLInit: Start\n");

  /* Check for NULL pointer */
  if (param == NULL || param->clientType >= EWL_CLIENT_TYPE_MAX) {
    PTRACE_E(("EWLInit: Bad calling parameters!\n"));
    return NULL;
  }

  /* Allocate instance */
  if ((enc = (vcx_cwl_t *)EWLmalloc(sizeof(vcx_cwl_t))) == NULL) {
    PTRACE_E("EWLInit: failed to alloc vcx_cwl_t struct\n");
    return NULL;
  }
  memset(enc, 0, sizeof(vcx_cwl_t));

  enc->clientType = param->clientType;
  enc->fd_enc = enc->fd_memalloc = -1;
  enc->reg_all_cores = NULL;
  enc->vcmd_cmdbuf_info.cmd_virt_addr = MAP_FAILED;
  enc->vcmd_cmdbuf_info.status_virt_addr = MAP_FAILED;

  //set dev names
  EWLSetDevName(enc, param->enc_dev);
  EWLSetMemDevName(enc, param->mem_dev);
  if (enc->enc_dev_n == NULL || enc->mem_dev_n == NULL) {
    PTRACE_E("EWLInit: failed to alloc device name\n");
    EWLfree(enc);
    return NULL;
  }


  enc->fd_enc = open(enc->enc_dev_n, O_RDWR);
  if (enc->fd_enc == -1) {
    PTRACE_E("EWLInit: failed to open: %s\n", enc->enc_dev_n);
    goto err;
  }

  enc->fd_memalloc = open(enc->mem_dev_n, O_RDWR);
  if (enc->fd_memalloc == -1) {
    PTRACE_E("EWLInit: failed to open: %s\n", enc->mem_dev_n);
    goto err;
  }

  if (ioctl(enc->fd_enc, HANTRO_IOCH_GET_VCMD_ENABLE, &enc->vcmdEnable) == -1) {
    PTRACE_E("EWLInit: ioctl HANTRO_IOCH_GET_VCMD_ENABLE failed\n");
  }

  if (ioctl(enc->fd_enc, HANTRO_IOCH_GET_MMU_ENABLE, &enc->mmuEnable)) {
    PTRACE_E("ioctl HANTRO_IOCH_GET_MMU_ENABLE failed \n");
    goto err;
  }

  if (enc->vcmdEnable == 0) {
    enc->vcmd_mode = VCMD_MODE_DISABLED;
    enc->reg_all_cores = EWLmalloc(EWLGetCoreNum((void *)enc) * sizeof(subsysReg));
    enc->coreAmout = EWLGetCoreNum((void *)enc);

    if (ioctl(enc->fd_enc, HANTRO_IOCH_GET_PM_SUPPORT, &enc->pm_support)) {
      PTRACE_E("ioctl HANTRO_IOCH_GET_PM_SUPPORT failed \n");
      goto err;
    }

    /* map hw registers to user space.*/
    if (MapAsicRegisters((void *)enc) != 0) {
      PTRACE_E("EWLReserveHw map register failed\n");
      goto err;
    }

    if (EWLInitLineBufSram(enc) != EWL_OK) {
      PTRACE_E("EWLInit: PCIE FPGA Verification Fail!\n");
      goto err;
    }
#ifdef MULTICORE_SUPPORT
    queue_init(&enc->freelist);
    queue_init(&enc->workers);
    for (i = 0; i < (int)EWLGetCoreNum((void *)enc); i++) {
      EWLWorker *worker = EWLmalloc(sizeof(EWLWorker));
	  if (worker == NULL) goto err;
      worker->core_id = i;
      worker->next = NULL;
      queue_put(&enc->freelist, (struct node *)worker);
    }
    EWLInitMulticore(enc->clientType);
#endif
  }
  #ifdef VCMD_BUILD_SUPPORT
  else {
    ASSERT(enc->reg_all_cores == NULL);
    enc->vcmd_mode = VCMD_MODE_ENABLED;

    i32 ret;
    ret = EWLGetVcmdParameters(enc->fd_enc, 0,
                               (CLIENT_TYPE)param->clientType,
                               &enc->main_module_init_reg_addr,
                               &enc->vcmd_enc_core_info,
                               &enc->vcmd_cmdbuf_info);
    if (ret == EWL_ERROR) goto err;

    queue_init(&enc->workers);
  }
  #endif

  VCEncDec400RegisiterWL(enc);
  PTRACE_I("EWLInit: Return %p\n", enc);
  return enc;

err:
  EWLRelease(enc);
  PTRACE_I("EWLInit: Return NULL\n");
  return NULL;
}

i32 EWLRelease(const void *inst) {
  vcx_cwl_t *enc = (vcx_cwl_t *)inst;
  u32 i;

  ASSERT(enc != NULL);

  if (enc == NULL) return EWL_OK;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
#ifdef MULTICORE_SUPPORT
    EWLReleaseMulticore(enc->clientType);
#endif
    /* Release the register mapping info of each core */
    for (i = 0; i < EWLGetCoreNum((void *)enc); i++) {
      if (enc->reg_all_cores != NULL &&
          enc->reg_all_cores[i].pRegBase != MAP_FAILED) {
        munmap((void *)enc->reg_all_cores[i].pRegBase,
               enc->reg_all_cores[i].regSize);
      }
    }

    EWLfree(enc->reg_all_cores);
    enc->reg_all_cores = NULL;
    /* Release the sram */
    if (enc->pLineBufSram != MAP_FAILED) {
      munmap((void *)enc->pLineBufSram, enc->lineBufSramSize);
    }

    free_nodes(enc->freelist.tail);
  } else {
    if (enc->vcmd_cmdbuf_info.cmd_virt_addr != MAP_FAILED) {
      munmap(enc->vcmd_cmdbuf_info.cmd_virt_addr,
             enc->vcmd_cmdbuf_info.cmd_total_size);
    }
    if (enc->vcmd_cmdbuf_info.status_virt_addr != MAP_FAILED) {
      munmap(enc->vcmd_cmdbuf_info.status_virt_addr,
             enc->vcmd_cmdbuf_info.status_total_size);
    }
    if (enc->vcmd_cmdbuf_info.reg_virt_addr != MAP_FAILED) {
      munmap(enc->vcmd_cmdbuf_info.reg_virt_addr, enc->vcmd_cmdbuf_info.reg_total_size);
      enc->vcmd_cmdbuf_info.reg_virt_addr = MAP_FAILED;
    }
  }

  free_nodes(enc->workers.tail);
  if (enc->fd_enc != -1) close(enc->fd_enc);
  if (enc->fd_memalloc != -1) close(enc->fd_memalloc);

  EWLfree(enc->enc_dev_n);
  EWLfree(enc->mem_dev_n);
  EWLfree(enc);

  PTRACE_I("EWLRelease: instance freed\n");

  return EWL_OK;
}

void EWLWriteRegbyClientType(const void *inst, u32 offset, u32 val,
                             u32 client_type) {
  vcx_cwl_t *enc;
  regMapping *reg;
  u32 core_id, core_type;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return;
  }

  core_id = LAST_CORE(enc);

  core_type = EWLGetCoreTypeByClientType(client_type);
  reg = &(enc->reg_all_cores[core_id].core[core_type]);

  ASSERT(reg != NULL && offset < reg->regSize);

  if (reg->core_id == -1) return;

  if (offset == 0x04) {
    //asic_status = val;
  }

  offset = offset / 4;
  *(reg->pRegBase + offset) = val;

  PTRACE_I("EWLWriteReg 0x%02x with value %08x\n", offset * 4, val);
}

/*******************************************************************************
 Function name   : EWLWriteReg
 Description     : Set the content of a hadware register
 Return type     : void
 Argument        : u32 offset
 Argument        : u32 val
*******************************************************************************/
void EWLWriteCoreReg(const void *inst, u32 offset, u32 val, u32 core_id) {
  vcx_cwl_t *enc;
  u32 core_type;
  regMapping *reg;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return;
  }

  core_type = EWLGetCoreTypeByClientType(enc->clientType);
  reg = &(enc->reg_all_cores[core_id].core[core_type]);

  ASSERT(reg != NULL && offset < reg->regSize);

  if (offset == 0x04) {
    //asic_status = val;
  }

  offset = offset / 4;
  *(reg->pRegBase + offset) = val;

  PTRACE_I("EWLWriteReg 0x%02x with value %08x\n", offset * 4, val);
}

/*******************************************************************************
 Function name   : EWLWriteRegByVcmd
 Description     : Set the content of a hadware register
 Return type     : void
 Argument        : u32 offset
 Argument        : u32 *val
*******************************************************************************/
void EWLWriteCoreRegByVcmd(const void *inst, u32 offset, u32 num, u32 *val) {
  vcx_cwl_t *enc;
  u32 core_type;
  struct core_regs_wr core;
  int i;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return;
  }

  ASSERT(num <= CORE_REGS_WR_NUM);

  core_type = EWLGetCoreTypeByClientType(enc->clientType);

  core.type = core_type;
  core.id = enc->reserve_cmdbuf_info.core_id;
  core.reg_num = num;
  core.reg_id = offset / 4;
  for (i = 0; i < num; i++)
    core.reg_val[i] = val[i];

  ioctl(enc->fd_enc, HANTRO_IOCH_WRITE_CORE_REGS, &core);

  PTRACE_I("EWLWriteRegByVcmd 0x%02x with value %u\n", offset, num);
}

void EWLWriteReg(const void *inst, u32 offset, u32 val) {
  u32 core_id = 0;
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    core_id = LAST_CORE(enc);
  }
  EWLWriteCoreReg(inst, offset, val, core_id);
}

void EWLWriteBackRegbyClientType(const void *inst, u32 offset, u32 val,
                                 u32 client_type) {
  vcx_cwl_t *enc;
  regMapping *reg;
  u32 core_id, core_type;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return;
  }

  core_id = FIRST_CORE(enc);

  core_type = EWLGetCoreTypeByClientType(client_type);
  reg = &(enc->reg_all_cores[core_id].core[core_type]);

  ASSERT(reg != NULL && offset < reg->regSize);

  if (reg->core_id == -1) return;

  if (offset == 0x04) {
    //asic_status = val;
  }

  offset = offset / 4;
  *(reg->pRegBase + offset) = val;

  PTRACE_I("EWLWriteReg 0x%02x with value %08x\n", offset * 4, val);
}

void EWLWriteBackReg(const void *inst, u32 offset, u32 val) {
  u32 core_id = 0;
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    core_id = FIRST_CORE(enc);
  }
  EWLWriteCoreReg(inst, offset, val, core_id);
}

/*------------------------------------------------------------------------------
    Function name   : EWLEnableHW
    Description     :
    Return type     : int
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
i32 EWLEnableHW(const void *inst, u32 offset, u32 val) {
  u32 core_id = 0;
  vcx_cwl_t *enc;
  u32 core_type;
  regMapping *reg;
  i32 ret = 0;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return 0;
  }

  core_id = LAST_CORE(enc);
  core_type = EWLGetCoreTypeByClientType(enc->clientType);

#if (!defined SUPPORT_SHARED_MMU) && (defined SUPPORT_MMU)
  if (enc->mmuEnable == 1) {
#ifdef SUPPORT_48PA_MMU
    ioctl(enc->fd_enc, HANTRO_IOCS_MMU_SWITCH_PAGETABLE, &core_id);
#endif
    ioctl(enc->fd_enc, HANTRO_IOCS_MMU_FLUSH, &core_id);
  }
#endif

  if (enc->pm_support) {
    u32 core_info = 0;
    if (EWL_IS_JPEG_CLIENT(enc->clientType))
      core_type = CORE_VCEJ;

    core_info |= core_id << 4;
    core_info |= core_type;
    ret = ioctl(enc->fd_enc, HANTRO_IOCG_ENABLE_CORE, &core_info);
  } else {
    reg = &(enc->reg_all_cores[core_id].core[core_type]);

    ASSERT(reg != NULL && offset < reg->regSize);

    offset = offset / 4;
    *(reg->pRegBase + offset) = val;
  }

  PTRACE_I("EWLEnableHW 0x%02x with value %08x\n", offset * 4, val);

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : EWLDisableHW
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
void EWLDisableHW(const void *inst, u32 offset, u32 val) {
  u32 core_id = 0;
  vcx_cwl_t *enc;
  u32 core_type;
  regMapping *reg;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return;
  }

  core_id = FIRST_CORE(enc);
  core_type = EWLGetCoreTypeByClientType(enc->clientType);
  reg = &(enc->reg_all_cores[core_id].core[core_type]);

  ASSERT(reg != NULL && offset < reg->regSize);

  offset = offset / 4;
  *(reg->pRegBase + offset) = val;

  PTRACE_I("EWLDisableHW 0x%02x with value %08x\n", offset * 4, val);
}

/*------------------------------------------------------------------------------
    Function name   : EWLGetPerformance
    Description     :
    Return type     : void
    Argument        : const void *inst
    Argument        : u32 offset
    Argument        : u32 val
------------------------------------------------------------------------------*/
u32 EWLGetPerformance(const void *inst) {
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  return enc->performance;
}

u32 EWLReadRegbyClientType(const void *inst, u32 offset, u32 client_type) {
  vcx_cwl_t *enc;
  u32 val;
  u32 core_id;
  u32 core_type;
  regMapping *reg;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return -1;
  }

  core_id = FIRST_CORE(enc);
  core_type = EWLGetCoreTypeByClientType(client_type);
  reg = &(enc->reg_all_cores[core_id].core[core_type]);
  ASSERT(offset < reg->regSize);

  offset = offset / 4;
  val = *(reg->pRegBase + offset);
  PTRACE_I("EWLReadReg 0x%02x --> %08x\n", offset * 4, val);
  return val;
}

/*******************************************************************************
 Function name   : EWLReadReg
 Description     : Retrive the content of a hadware register
                    Note: The status register will be read after every MB
                    so it may be needed to buffer it's content if reading
                    the HW register is slow.
 Return type     : u32
 Argument        : u32 offset
*******************************************************************************/
u32 EWLReadReg(const void *inst, u32 offset) {
  vcx_cwl_t *enc;
  u32 val;
  volatile u32 *status_addr;

  enc = (vcx_cwl_t *)inst;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    u32 core_id;
    u32 core_type;
    regMapping *reg;
    core_id = FIRST_CORE(enc);
    core_type = EWLGetCoreTypeByClientType(enc->clientType);
    reg = &(enc->reg_all_cores[core_id].core[core_type]);
    ASSERT(offset < reg->regSize);
    status_addr = reg->pRegBase;

  } else {
    u16 cmdbufid;
    cmdbufid = FIRST_CMDBUF_ID(enc);
    status_addr = enc->vcmd_cmdbuf_info.status_virt_addr +
                  enc->vcmd_cmdbuf_info.status_unit_size / 4 * cmdbufid;
    status_addr += enc->vcmd_enc_core_info.status_main_addr / 4;
  }
  offset = offset / 4;
  val = *(status_addr + offset);
  PTRACE_I("EWLReadReg 0x%02x --> %08x\n", offset * 4, val);
  return val;
}

u32 EWLGetVcmdVersionId(const void *inst) {
  vcx_cwl_t *enc;
  enc = (vcx_cwl_t *)inst;
  return enc->vcmd_enc_core_info.vcmd_hw_version_id;
}

u32 EWLReadRegInit(const void *inst, u32 offset) {
  vcx_cwl_t *enc;
  u32 val;
  u32 *reg_addr;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return EWL_OK;
  }
  reg_addr = enc->main_module_init_reg_addr;
  offset = offset / 4;
  val = *(reg_addr + offset);

  PTRACE_I("EWLReadReg 0x%02x --> %08x\n", offset * 4, val);

  return val;
}

/*------------------------------------------------------------------------------
Function name   : EWLmmapBuffer
Description     : mmap the bus address of VPU buffer to virtual address

Return type     : u32* - virtual address after mmap

Argument        : const void * instance - EWL instance
Argument        : MemallocParams params - parameters of memory alloc
Argument        : EWLLinearMem_t *buff - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
static u32 *EWLmmapBuffer(const void *inst, MemallocParams params,
                          EWLLinearMem_t *buff) {
  vcx_cwl_t *enc;
  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);
  ASSERT(buff != NULL);
  u32 *mmapAddr = MAP_FAILED;

  /* Map the bus address to virtual address */
  mmapAddr = (u32 *)mmap(0, buff->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         enc->fd_memalloc, params.bus_address);
  if ((buff->mem_type != EWL_MEM_TYPE_DPB) &&
      (buff->mem_type != EWL_MEM_TYPE_VPU_ONLY)) {
    if (mmapAddr == MAP_FAILED) {
      PTRACE_E("EWLmmapBuffer: Failed to mmap busAddress: %p\n",
               (void *)params.bus_address);
      return mmapAddr;
    }
  }
  return mmapAddr;
}

/*------------------------------------------------------------------------------
Function name   : EWLunmmapBuffer
Description     : unmmap the virtual address mmapped from bus address of VPU

Return type     : void

Argument        : u32 *virtualAddress - alignment virtual address
Argument        : u32 *allocVirtualAddr - allocated virtual address
Argument        : u32 size - size in byte of buffer
------------------------------------------------------------------------------*/
static void EWLunmmapBuffer(u32 *virtualAddress, u32 *allocVirtualAddr,
                            u32 size) {
  if (allocVirtualAddr != MAP_FAILED) munmap(allocVirtualAddr, size);
}

#ifdef SUPPORT_48PA_MMU
/*------------------------------------------------------------------------------
 Function name   : EWLMMUSwitchPageTableByCmdbuf
 Description     : Switch MMU page table by cmdbuf

 Return type     : u32

Argument        : const void * instance - EWL instance
Argument        : void * params - paramters of page table switch
------------------------------------------------------------------------------*/
u32 EWLMMUSwitchPageTableByCmdbuf(const void *inst, u32 *params) {
  vcx_cwl_t *enc;
  struct page_table_switch pt_params;
  int i;

  enc = (vcx_cwl_t *)inst;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED)
    return -1;

  if (ioctl(enc->fd_enc, (int)HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF, &pt_params) != 0)
    return -1;
  if (pt_params.id < 0)
    return -1;

  /* params[0]: page table id */
  params[0] = (u32)pt_params.id;
  /* params[1]: page table flush cache line count by vmid */
  params[1] = (u32)pt_params.pt_flush.flush_cnt;
  /* params[2 - ]: need be flushed page table cache line */
  for (i = 0; i < pt_params.pt_flush.flush_cnt; i++)
    params[2 + i] = (u32)pt_params.pt_flush.flush_vmid[i];

  PTRACE_I("EWLMMUSwitchPageTableByCmdbuf: switch to page table [%d]!\n", pt_params.id);
  return 0;
}
#endif

#ifdef SUPPORT_MMU
/*------------------------------------------------------------------------------
Function name   : EWLMMUMemoryMap
Description     : MMU mmap the VPU buffer

Return type     : void

Argument        : const void * instance - EWL instance
Argument        : MemallocParams params - parameters of memory alloc
Argument        : The parameter of alignment
Argument        : EWLLinearMem_t *buff - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
static void EWLMMUMemoryMap(const void *inst, MemallocParams params,
                            u32 alignment, EWLLinearMem_t *buff) {
  vcx_cwl_t *enc;
  int ioctl_req;
  struct addr_desc addr;
  u32 *MMUMapAddr = NULL;
  u32 core_id;
  ASSERT(buff != NULL);

#ifndef SUPPORT_MEM_SYNC
  MMUMapAddr = buff->allocVirtualAddr;
#else
  struct sync_mem *priv = (struct sync_mem *)buff->priv;
  if (priv->dev_va_alloc != MAP_FAILED)
    MMUMapAddr = priv->dev_va_alloc;
  else {
    MMUMapAddr = (u32 *)buff->allocBusAddr;
    PTRACE_I("EWLMMUMemoryMap map device memory not supported yet!\n");
    return;
  }
#endif

  enc = (vcx_cwl_t *)inst;
  if (enc->mmuEnable == 1 && MMUMapAddr != MAP_FAILED) {
    addr.virtual_address = MMUMapAddr;
    addr.size = params.size;

    mlock(addr.virtual_address, addr.size);
    ioctl_req = (int)HANTRO_IOCS_MMU_MEM_MAP;
    ioctl(enc->fd_enc, ioctl_req, &addr);
    buff->busAddress = addr.bus_address;
    buff->busAddress =
        (buff->busAddress + (alignment - 1)) & (~(((u64)alignment) - 1));

#ifdef SUPPORT_SHARED_MMU
    core_id = 0;
    ioctl(enc->fd_enc, HANTRO_IOCS_MMU_FLUSH, &core_id);
#endif
  }
}

/*------------------------------------------------------------------------------
Function name   : EWLMMUMemoryUnMap
Description     : MMU unmmap the VPU buffer

Return type     : void

Argument        : const void * instance - EWL instance
Argument        : EWLLinearMem_t *buff - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
static void EWLMMUMemoryUnMap(const void *inst, EWLLinearMem_t *buff) {
  int ioctl_req;
  struct addr_desc addr;
  vcx_cwl_t *enc;
  u32 *MMUunmapAddr = MAP_FAILED;
  ASSERT(buff != NULL);

#ifndef SUPPORT_MEM_SYNC
  MMUunmapAddr = buff->allocVirtualAddr;
#else
  struct sync_mem *priv = (struct sync_mem *)buff->priv;
  if (priv->dev_va_alloc != MAP_FAILED)
    MMUunmapAddr = priv->dev_va_alloc;
  else {
    PTRACE_I("EWLMMUMemoryUnMap map device memory not supported yet!\n");
    return;
  }
#endif

  enc = (vcx_cwl_t *)inst;
  if (enc->mmuEnable == 1 && MMUunmapAddr != MAP_FAILED) {
    addr.virtual_address = MMUunmapAddr;
    ioctl_req = (int)HANTRO_IOCS_MMU_MEM_UNMAP;
    ioctl(enc->fd_enc, ioctl_req, &addr);
  }
}
#endif

/*------------------------------------------------------------------------------
Function name   : EWLMallocRefFrm
Description     : Allocate a frame buffer (contiguous linear RAM memory)

Return type     : i32 - 0 for success or a negative error code

Argument        : const void * instance - EWL instance
Argument        : u32 size - size in bytes of the requested memory
Argument        : EWLLinearMem_t *info - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocRefFrm(const void *inst, u32 size, u32 alignment,
                    EWLLinearMem_t *info) {
  vcx_cwl_t *enc;
  i32 ret;
  EWLLinearMem_t *buff;

  enc = (vcx_cwl_t *)inst;
  buff = (EWLLinearMem_t *)info;
  ASSERT(enc != NULL);
  ASSERT(buff != NULL);

  PTRACE_I("EWLMallocRefFrm\t%8d bytes\n", size);

  ret = EWLMallocLinear(enc, size, alignment, buff);

  PTRACE_I("EWLMallocRefFrm %p --> %p\n", (void *)buff->busAddress,
           buff->virtualAddress);
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
void EWLFreeRefFrm(const void *inst, EWLLinearMem_t *info) {
  vcx_cwl_t *enc;
  EWLLinearMem_t *buff;

  enc = (vcx_cwl_t *)inst;
  buff = (EWLLinearMem_t *)info;
  ASSERT(enc != NULL);
  ASSERT(buff != NULL);

  EWLFreeLinear(enc, buff);
  PTRACE_I("EWLFreeRefFrm\t%p\n", buff->virtualAddress);
}

/*------------------------------------------------------------------------------
Function name   : EWLMallocLinear
Description     : Allocate a contiguous, linear RAM  memory buffer

Return type     : i32 - 0 for success or a negative error code

Argument        : const void * instance - EWL instance
Argument        : u32 size - size in bytes of the requested memory
Argument        : EWLLinearMem_t *info - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
i32 EWLMallocLinear(const void *inst, u32 size, u32 alignment,
                    EWLLinearMem_t *info) {
  vcx_cwl_t *enc;
  EWLLinearMem_t *buff;
  MemallocParams params;
  u32 pgsize;
  i32 ret;
  enc = (vcx_cwl_t *)inst;
  buff = (EWLLinearMem_t *)info;
  ASSERT(enc != NULL);
  ASSERT(buff != NULL);
  pgsize = getpagesize();
  memset(&params, 0, sizeof(params));

  PTRACE_I("EWLMallocLinear\t%8d bytes\n", size);

  if (alignment == 0) alignment = 1;

  buff->size = buff->total_size =
      (((size + (alignment - 1)) & (~(alignment - 1))) + (pgsize - 1)) &
      (~(pgsize - 1));
  params.size = (size + (alignment - 1)) & (~(alignment - 1));

  buff->virtualAddress = 0;
  buff->busAddress = 0;
  buff->allocVirtualAddr = 0;
  buff->allocBusAddr = 0;
  params.mem_type = buff->mem_type;
  /* get memory linear memory buffers */
  ioctl(enc->fd_memalloc, MEMALLOC_IOCXGETBUFFER, &params);
  if (params.bus_address == 0) {
    PTRACE_E("EWLMallocLinear: Linear buffer not allocated\n");
    return EWL_ERROR;
  }

  /* ASIC might be in different address space */
  buff->allocBusAddr =
      BUS_CPU_TO_ASIC(params.bus_address, params.translation_offset);
  buff->busAddress =
      (buff->allocBusAddr + (alignment - 1)) & (~(((u64)alignment) - 1));
  if (sizeof(buff->busAddress) == 8 && (buff->busAddress >> 32) != 0) {
    PTRACE_I(
        "EWLInit: allocated busAddress overflow 32 bit: (%p), please ensure HW "
        "support 64bits address space\n",
        (void *)params.bus_address);
  }
  MEM_LOG_I("    Malloc Linear for Client %+4x(Type %d),"
            " %12u Bytes, Attribute: %d, Usage: (%2d)%s\n", ((ptr_t)enc&0xffff),
            enc->clientType, buff->total_size, EWLGetMemAttribute(info->mem_type),
            EWLGetMemHint(info->mem_type), EWLGetMemUsage(info->mem_type));

  buff->allocVirtualAddr = MAP_FAILED;
  ret =
      EWLMemSyncAllocHostBuffer((const void *)enc, buff->size, alignment, buff);
  if (ret == EWL_NOT_SUPPORT) {
    buff->allocVirtualAddr = EWLmmapBuffer(
        inst, params, buff); /* Map the bus address to virtual address */
    if (buff->allocVirtualAddr == MAP_FAILED) return EWL_ERROR;
    buff->virtualAddress =
        buff->allocVirtualAddr + (buff->busAddress - buff->allocBusAddr)/sizeof(u32);
  }
#ifdef MEM_SYNC_TEST  //The premise is open SUPPORT_MEM_SYNC, otherwise, it doesn't make sense
  struct sync_mem *priv = (struct sync_mem *)buff->priv;
  priv->dev_va_alloc = MAP_FAILED;
  if (buff->virtualAddress) {  //When open MEM_SYNC,It indicates that there must be a buffer on the host side,
    //If necessary test Memory sync, can map the buffer on the device side to priv
    priv->dev_pa_alloc = params.bus_address;
    priv->dev_va_alloc = EWLmmapBuffer(inst, params, buff);
    priv->dev_va = priv->dev_va_alloc + (buff->busAddress - buff->allocBusAddr);
  }
#endif

#ifdef SUPPORT_MMU
  EWLMMUMemoryMap(inst, params, alignment, buff);
#endif

#ifdef SUPPORT_MEM_STATISTIC
  pthread_mutex_lock(&linearBufferCountMutex);
  memoryInfo.maxLinearMallocTimes++;
  memoryInfo.linearMallocTimes++;
  if (memoryInfo.averageLinearMallocTimes < memoryInfo.linearMallocTimes)
    memoryInfo.averageLinearMallocTimes = memoryInfo.linearMallocTimes;
  memoryInfo.linearMemoryValue += buff->total_size;
  if (memoryInfo.maxLinearMemory < memoryInfo.linearMemoryValue)
    memoryInfo.maxLinearMemory = memoryInfo.linearMemoryValue;
  pthread_mutex_unlock(&linearBufferCountMutex);
#endif

  PTRACE_I("EWLMallocLinear %p (CPU) %p (ASIC) --> %p\n",
           (void *)params.bus_address, (void *)buff->busAddress,
           buff->virtualAddress);

  return EWL_OK;
}

/*------------------------------------------------------------------------------
Function name   : EWLFreeLinear
Description     : Release a linera memory buffer, previously allocated with
                    EWLMallocLinear.

Return type     : void

Argument        : const void * instance - EWL instance
Argument        : EWLLinearMem_t *info - linear buffer memory information
------------------------------------------------------------------------------*/
void EWLFreeLinear(const void *inst, EWLLinearMem_t *info) {
  vcx_cwl_t *enc;
  EWLLinearMem_t *buff;
  i32 ret;

  enc = (vcx_cwl_t *)inst;
  buff = (EWLLinearMem_t *)info;
  ASSERT(enc != NULL);
  ASSERT(buff != NULL);

  if (buff->size == 0)
    return;

#ifdef SUPPORT_MMU
  EWLMMUMemoryUnMap(inst, buff);
#endif

#ifdef MEM_SYNC_TEST
  struct sync_mem *priv = (struct sync_mem *)buff->priv;
  EWLunmmapBuffer(priv->dev_va, priv->dev_va_alloc, buff->size);
  priv->dev_va = NULL;
  priv->dev_va_alloc = NULL;
  PTRACE_I("EWLFreeLinear VPU buffer for Memory Sync Test\t%p\n",
           priv->dev_va_alloc);
#endif

  ret = EWLMemSyncFreeHostBuffer((const void *)enc, buff);
  if (ret == EWL_NOT_SUPPORT) {
    EWLunmmapBuffer(buff->virtualAddress, buff->allocVirtualAddr, buff->size);
    buff->virtualAddress = NULL;
    buff->allocVirtualAddr = NULL;
  }

  if (buff->allocBusAddr != 0) {
    ioctl(enc->fd_memalloc, MEMALLOC_IOCSFREEBUFFER, &buff->allocBusAddr);
    MEM_LOG_I("    Free Linear for Client %+4x(Type %d),"
              " %12u Bytes, Attribute: %d, Usage: (%2d)%s\n",
              ((ptr_t)enc&0xffff), enc->clientType, info->total_size,
              EWLGetMemAttribute(info->mem_type), EWLGetMemHint(info->mem_type),
              EWLGetMemUsage(info->mem_type));
    buff->allocBusAddr = 0;
    buff->busAddress = 0;
  }

#ifdef SUPPORT_MEM_STATISTIC
  if (buff->total_size != 0) {
    pthread_mutex_lock(&linearBufferCountMutex);
    memoryInfo.linearMallocTimes--;
    memoryInfo.linearMemoryValue -= buff->total_size;
    pthread_mutex_unlock(&linearBufferCountMutex);
  }
#endif

  PTRACE_I("EWLFreeLinear\t%p\n", buff->allocVirtualAddr);
  buff->size = 0;
  buff->total_size = 0;
}

/*******************************************************************************
 Function name   : EWLGetDec400Coreid
 Description     : get one dec400 core id
*******************************************************************************/
i32 EWLGetDec400Coreid(const void *inst) {
  vcx_cwl_t *enc;
  u32 val;
  u32 *reg_addr;
  u32 core_id = -1;
  u32 i;

  enc = (vcx_cwl_t *)inst;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    for (i = 0; i < EWLGetCoreNum((void *)enc); i++) {
      if (enc->reg_all_cores[i].core[CORE_DEC400].core_id != -1) {
        core_id = i;
        return core_id;
      }
    }
  } else {
    reg_addr = enc->vcmd_cmdbuf_info.reg_virt_addr +
                  enc->vcmd_cmdbuf_info.reg_unit_size / 4 * 0;
    reg_addr += enc->vcmd_enc_core_info.submodule_dec400_addr / 4;
    val = *(reg_addr + 0x2a);
    if (val == 0x01004000 || val == 0x01004002)
      return 0;
    else
      return -1;
  }
  return core_id;
}


void EWLSetReserveBaseData(const void *inst, u64 interrupt_ctrl,
                           u32 client_type) {
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return;
  }
  enc->reserve_cmdbuf_info.interrupt_ctrl = interrupt_ctrl;

  switch (client_type) {
    case EWL_CLIENT_TYPE_CUTREE:
      enc->reserve_cmdbuf_info.module_type = VCMD_TYPE_CUTREE;
      break;
    case EWL_CLIENT_TYPE_JPEG_ENC:
      enc->reserve_cmdbuf_info.module_type =
          enc->vcmd_enc_core_info.module_type;
      break;
    default:
      enc->reserve_cmdbuf_info.module_type = VCMD_TYPE_ENCODER;
      break;
  }
}

u16 EWLGetClientOffset(const void *inst, u16 comp_type)
{
  vcx_cwl_t *enc;
  u16 comp_offset = 0;

  enc = (vcx_cwl_t *)inst;
  switch (comp_type) {
  case EWL_CLIENT_TYPE_MAIN:
  case EWL_CLIENT_TYPE_HEVC_ENC:
  case EWL_CLIENT_TYPE_VP9_ENC:
  case EWL_CLIENT_TYPE_AV1_ENC:
  case EWL_CLIENT_TYPE_JPEG_ENC:
  case EWL_CLIENT_TYPE_CUTREE:
    comp_offset = enc->vcmd_enc_core_info.submodule_main_addr;
    break;
  case EWL_CLIENT_TYPE_DEC400:
    comp_offset = enc->vcmd_enc_core_info.submodule_dec400_addr;
    break;
  case EWL_CLIENT_TYPE_MMU0:
    comp_offset = enc->vcmd_enc_core_info.submodule_MMU_addr[0];
    break;
  case EWL_CLIENT_TYPE_MMU1:
    comp_offset = enc->vcmd_enc_core_info.submodule_MMU_addr[1];
    break;
  case EWL_CLIENT_TYPE_AXIFE:
    comp_offset = enc->vcmd_enc_core_info.submodule_axife_addr[0];
    break;
  case EWL_CLIENT_TYPE_AXIFE_1:
    comp_offset = enc->vcmd_enc_core_info.submodule_axife_addr[1];
    break;
  case EWL_CLIENT_TYPE_UFBC:
    comp_offset = enc->vcmd_enc_core_info.submodule_ufbc_addr;
    break;
  default:
    comp_offset = 0xffff;
  }

  return comp_offset;
}

u16 EWLGetClientVcmdStatusBufOffset(const void *inst, u16 comp_type)
{
  vcx_cwl_t *enc;
  u16 comp_offset = 0;

  enc = (vcx_cwl_t *)inst;
  switch (comp_type) {
  case EWL_CLIENT_TYPE_MAIN:
  case EWL_CLIENT_TYPE_HEVC_ENC:
  case EWL_CLIENT_TYPE_VP9_ENC:
  case EWL_CLIENT_TYPE_AV1_ENC:
  case EWL_CLIENT_TYPE_JPEG_ENC:
  case EWL_CLIENT_TYPE_CUTREE:
    comp_offset = enc->vcmd_enc_core_info.status_main_addr;
    break;
  case EWL_CLIENT_TYPE_DEC400:
    comp_offset = enc->vcmd_enc_core_info.status_dec400_addr;
    break;
  case EWL_CLIENT_TYPE_MMU0:
    comp_offset = enc->vcmd_enc_core_info.status_MMU_addr[0];
    break;
  case EWL_CLIENT_TYPE_MMU1:
    comp_offset = enc->vcmd_enc_core_info.status_MMU_addr[1];
    break;
  case EWL_CLIENT_TYPE_AXIFE:
    comp_offset = enc->vcmd_enc_core_info.status_axife_addr[0];
    break;
  case EWL_CLIENT_TYPE_AXIFE_1:
    comp_offset = enc->vcmd_enc_core_info.status_axife_addr[1];
    break;
  case EWL_CLIENT_TYPE_UFBC:
    comp_offset = enc->vcmd_enc_core_info.status_ufbc_addr;
    break;
  default:
    comp_offset = 0xffff;
  }

  return comp_offset;
}

#ifdef VCMD_BUILD_SUPPORT
/*******************************************************************************
 Function name   : EWLGetVcmdCoreNum
 Description     : Get the vcmd core num for same module type
*******************************************************************************/
i32 EWLGetVcmdCoreNum(const void *ctx, CLIENT_TYPE client_type) {
  u32 vcmdCoreNum = 0;
  struct config_parameter vcmdCoreInfo;
  memset(&vcmdCoreInfo, 0, sizeof(vcmdCoreInfo));
  u32 module_type = EWLGetVcmdModuleType(client_type);
  vcx_cwl_t *ewl_ctx = (vcx_cwl_t *)ctx;

  /* Check invalid parameters */
  ASSERT(ewl_ctx != NULL);

  vcmdCoreInfo.module_type = module_type;
  if (ioctl(ewl_ctx->fd_enc, HANTRO_IOCH_GET_VCMD_PARAMETER, &vcmdCoreInfo) == -1) {
      PTRACE_E("%s","ioctl HANTRO_IOCH_GET_VCMD_PARAMETER failed\n");
      goto end;
  }
  vcmdCoreNum = vcmdCoreInfo.vcmd_core_num;

end:
  PTRACE_I("EWLGetVcmdCoreNum: %d\n", vcmdCoreNum);
  return vcmdCoreNum;
}

void EWLReadVcmdPriority(const void *ctx, u32 *priority, u32 client_type)
{
  struct config_parameter core_info;
  vcx_cwl_t *ewl_ctx = (vcx_cwl_t *)ctx;

  /* Check invalid parameters */
  ASSERT(ewl_ctx != NULL);
  /* Get vcmd parameters */
  memset(&core_info, 0, sizeof(core_info));
  core_info.module_type = EWLGetVcmdModuleType(client_type);
  if (ioctl(ewl_ctx->fd_enc, HANTRO_IOCH_GET_VCMD_PARAMETER, &core_info)) {
    PTRACE_E("ioctl HANTRO_IOCH_GET_VCMD_PARAMETER failed\n");
    ASSERT(0);
  }
  for(int i = 0; i < core_info.vcmd_core_num; i++) {
    priority[i] = core_info.vcmd_priority[i];
  }
}

/*******************************************************************************
 Function name   : EWLReserveCmdebuf
 Description     : Reserve cmdbuf resource for currently encode
*******************************************************************************/
i32 EWLReserveCmdbuf(const void *inst, EWLResource_t *resource) {
  vcx_cwl_t *enc;
  i32 ret;
  struct exchange_parameter *exchange_data;
  struct cmdbuf_mem_parameter *info;

  enc = (vcx_cwl_t *)inst;
  /* Check invalid parameters */
  if (enc == NULL) return EWL_ERROR;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return EWL_OK;
  }

  exchange_data = &enc->reserve_cmdbuf_info;
  exchange_data->cmdbuf_size = resource->vcmdbuf.size * 4;
  exchange_data->core_mask = resource->vcmdbuf.core_mask & 0xffff;
  exchange_data->input_mask = 0;
  if (resource->vcmdbuf.priority)
    EXCH_S_BIT(exchange_data->input_mask, EXCH_PRIO_BIT);
#ifdef USE_END_CMD
  /* set end cmd bit into input_mask */
  EXCH_S_BIT(exchange_data->input_mask, EXCH_END_CMD_BIT);
#endif

  info = &enc->vcmd_cmdbuf_info;

  PTRACE_I("EWLReserveCmdbufHw: PID %d trying to reserve ...\n", GETPID());

  ret = ioctl(enc->fd_enc, HANTRO_IOCH_RESERVE_CMDBUF, exchange_data);

  if (ret < 0) {
    PTRACE_E("EWLReserveCmdbuf failed\n");
    return EWL_ERROR;
  } else {
    EWLWorker *worker = EWLmalloc(sizeof(EWLWorker));
    if (worker == NULL) return EWL_ERROR;

    worker->cmdbuf_id = exchange_data->cmdbuf_id;
    worker->next = NULL;
    queue_put(&enc->workers, (struct node *)worker);
    resource->vcmdbuf.id = exchange_data->cmdbuf_id;
    resource->vcmdbuf.status_ba = info->status_hw_addr +
                        info->status_unit_size * exchange_data->cmdbuf_id;
    resource->vcmdbuf.cmdbuf_va = info->cmd_virt_addr +
                        info->cmd_unit_size / 4 * exchange_data->cmdbuf_id;
    PTRACE_I("EWLReserveCmdbuf successed\n");
  }

  PTRACE_I("EWLReserveCmdbuf: ENC cmdbuf locked by PID %d\n", GETPID());
  return EWL_OK;
}

/*******************************************************************************
 Function name   : EWLLinkRunCmdbuf
 Description     : link and run current cmdbuf
*******************************************************************************/
i32 EWLLinkRunCmdbuf(const void *inst, u16 cmdbufid, u16 cmdbuf_size) {
  vcx_cwl_t *enc;
  i32 ret;
  u16 core_info_hw;
  struct exchange_parameter *exchange_data;

  enc = (vcx_cwl_t *)inst;
  /* Check invalid parameters */
  if (enc == NULL) return EWL_ERROR;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return EWL_OK;
  }
  core_info_hw = cmdbufid;
  exchange_data = &enc->reserve_cmdbuf_info;

  //FIXME: not check id for batch mode will not link the reserved one.
  //if (cmdbufid != exchange_data->cmdbuf_id) return EWL_ERROR;
  exchange_data->cmdbuf_id = cmdbufid;

  PTRACE_I("EWLLinkRunCmdbuf: PID %d trying to link and  run cmdbuf ...\n",
           GETPID());
  exchange_data->cmdbuf_size = cmdbuf_size * 4;
  ret = ioctl(enc->fd_enc, HANTRO_IOCH_LINK_RUN_CMDBUF, exchange_data);

  if (ret < 0) {
    PTRACE_E("EWLLinkRunCmdbuf failed\n");
    return EWL_ERROR;
  } else {
    PTRACE_I("EWLLinkRunCmdbuf successed\n");
  }

  PTRACE_I("EWLLinkRunCmdbuf:  cmdbuf locked by PID %d\n", GETPID());

  return EWL_OK;
}

/*******************************************************************************
 Function name   : EWLWaitCmdbuf
 Description     : wait cmdbuf run done
*******************************************************************************/
i32 EWLWaitCmdbuf(const void *inst, u16 cmdbufid, u32 *status) {
  vcx_cwl_t *enc;
  i32 ret = 0;
  u16 core_info_hw;
  u32 *ddr_addr = NULL;
  u32 ufbc_irq_offset = 0;
#ifdef POLLING_ISR
  pthread_attr_t attr;
  pthread_t tid;
#endif
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
    u32 sliceinfo_status = 0;
#endif

  enc = (vcx_cwl_t *)inst;
  core_info_hw = cmdbufid;

  /* Check invalid parameters */
  if (enc == NULL) return EWL_ERROR;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return EWL_OK;
  }

#ifdef POLLING_ISR
  {
    enc->wait_core_id_polling = enc->reserve_cmdbuf_info.core_id;
    enc->wait_polling_break = 0;
    pthread_attr_init(&attr);
    pthread_create(&tid, &attr, (void *)polling_vcmd_isr, (void *)enc);
    pthread_attr_destroy(&attr);
  }
#endif
  PTRACE_I("EWLWaitCmdbuf: PID %d wait cmdbuf ...\n", GETPID());
  ret = ioctl(enc->fd_enc, HANTRO_IOCH_WAIT_CMDBUF, &core_info_hw);

  if (ret < 0) {
    PTRACE_E("EWLWaitCmdbuf failed\n");
    *status = 0;
#ifdef POLLING_ISR
    {
      enc->wait_polling_break = 1;
      pthread_join(tid, NULL);
    }
#endif

    return EWL_HW_WAIT_ERROR;
  } else {
     PTRACE_I("EWLWaitCmdbuf successed\n");
    ddr_addr = enc->vcmd_cmdbuf_info.status_virt_addr +
               enc->vcmd_cmdbuf_info.status_unit_size / 4 * cmdbufid;
    ddr_addr += enc->vcmd_enc_core_info.status_main_addr / 4;
    if (ret == 1) {
      *status = *(ddr_addr + VCMD_SLICE_RDY_INTERRUPT);
      *(ddr_addr + 7) = *(ddr_addr + VCMD_SLICE_RDY_NUM);
    } else if (ret == 2) {
      *status = *(ddr_addr + VCMD_LINE_BUFFER_INTERRUPT);
    } else if (ret == 3) {
      *status = *(ddr_addr + VCMD_LINE_BUFFER_INTERRUPT) | *(ddr_addr + VCMD_SLICE_RDY_INTERRUPT);
      *(ddr_addr + 7) = *(ddr_addr + VCMD_SLICE_RDY_NUM);
    } else {
      *status = *(ddr_addr + 1);
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
      /* sliceinfo irq_status store in reg118 */
      sliceinfo_status = (*(ddr_addr + 118) & 0x01);
      if (sliceinfo_status)
        *status |= ASIC_STATUS_POLL_SLICEINFO_TIMEOUT;
#endif
#ifdef SUPPORT_UFBC
      if (enc->ufbcMode) {
        u32 *ddr_addr_ufbc = enc->vcmd_cmdbuf_info.status_virt_addr +
                  enc->vcmd_cmdbuf_info.status_unit_size / 4 * cmdbufid;
        ddr_addr_ufbc += enc->vcmd_enc_core_info.status_ufbc_addr / 4;
        u32 ufbc_status = (*(ddr_addr_ufbc + enc->ufbcIrqOffset) & 0x01);
        if (ufbc_status)
          *status |= ASIC_STATUS_UFBC_DEC_ERR;
      ufbc_status = (*(ddr_addr_ufbc + enc->ufbcIrqOffset) & 0x02) >> 1;
      if (ufbc_status)
        *status |= ASIC_STATUS_UFBC_CFG_ERR;
      }
#endif
      u32 sbi_status = *(ddr_addr + 349);
      if ((sbi_status >> 30) & 0x01) {
        // sbi out of sync
        *status |= ASIC_STATUS_SBI_OUT_OF_SYNC;
      } else {
        *status &= ~ASIC_STATUS_SBI_OUT_OF_SYNC;
      }
      if ((sbi_status >> 29) & 0x01) {
        // sbi timeout
        *status |= ASIC_STATUS_SBI_TIMEOUT;
      } else {
        *status &= ~ASIC_STATUS_SBI_TIMEOUT;
      }
    }
  }

#ifdef POLLING_ISR
  {
    enc->wait_polling_break = 1;
    pthread_join(tid, NULL);
  }
#endif
  PTRACE_I("EWLWaitCmdbuf:  cmdbuf locked by PID %d\n", GETPID());

  return EWL_OK;
}

void EWLGetRegsByCmdbuf(const void *inst, u16 cmdbufid, u32 *regMirror) {
  vcx_cwl_t *enc;
  enc = (vcx_cwl_t *)inst;
  i32 ret;
  u32 *ddr_addr = NULL;
  PTRACE_I("EncGetRegsByCmdbuf: PID %d wait cmdbuf ...\n", GETPID());

  ddr_addr = enc->vcmd_cmdbuf_info.status_virt_addr +
             enc->vcmd_cmdbuf_info.status_unit_size / 4 * cmdbufid;
  ddr_addr += enc->vcmd_enc_core_info.status_main_addr / 4;
  EWLmemcpy(regMirror, ddr_addr, ASIC_SWREG_AMOUNT * sizeof(u32));

  PTRACE_I("EncGetRegsByCmdbuf:  cmdbuf locked by PID %d\n", GETPID());
}

/*******************************************************************************
 Function name   : EWLReleaseCmdbuf
 Description     : wait cmdbuf run done
*******************************************************************************/
i32 EWLReleaseCmdbuf(const void *inst, u16 cmdbufid) {
  vcx_cwl_t *enc;
  i32 ret;
  u16 core_info_hw;

  enc = (vcx_cwl_t *)inst;
  core_info_hw = cmdbufid;

  /* Check invalid parameters */
  if (enc == NULL) return EWL_ERROR;

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    return EWL_OK;
  }

  PTRACE_I("EWLReleaseCmdbuf: PID %d wait cmdbuf ...\n", GETPID());
  enc->performance =
      EWLReadReg(inst, 82 * 4);  //save the performance before release hw

  ret = ioctl(enc->fd_enc, HANTRO_IOCH_RELEASE_CMDBUF, &core_info_hw);

  if (ret < 0) {
    PTRACE_E("EWLReleaseCmdbuf failed\n");
    return EWL_ERROR;
  } else {
    EWLWorker *worker = (EWLWorker *)queue_get(&enc->workers);
    EWLfree(worker);
    PTRACE_I("EWLReleaseCmdbuf successed\n");
  }

  PTRACE_I("EWLReleaseCmdbuf:  cmdbuf locked by PID %d\n", GETPID());

  return EWL_OK;
}
#endif //VCMD_BUILD_SUPPORT
/*******************************************************************************
 Function name   : EWLWaitHwRdy
 Description     : Poll the encoder interrupt register to notice IRQ
 Return type     : i32
 Argument        : void
*******************************************************************************/
#ifdef POLLING_ISR
i32 EWLWaitHwRdy(const void *inst, u32 *slicesReady, void *waitOut,
                 u32 *status_register) {
  vcx_cwl_t *enc;
  regMapping *reg = NULL;
  volatile u32 irq_stats;
  u32 prevSlicesReady = 0;
  i32 ret = EWL_HW_WAIT_TIMEOUT;
  struct timespec t;
  u32 timeout = 1000; /* Polling interval in microseconds */
  // int loop = 500;
  int loop = 500000;     /* How many times to poll before timeout */
  u32 wClr;
  u32 hwId = 0;
  u32 core_id;
  u32 core_type;

  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);
  ASSERT(sizeof(EWLCoreWaitOut_t) == sizeof(CORE_WAIT_OUT));

  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return EWL_OK;
  }

  PTRACE_I("EWLWaitHwRdy\n");
  if (waitOut != NULL) {
    u32 i, loop_num = 20;
    for (i = 0; i < loop_num; i++) {
      if (-1 == ioctl(enc->fd_enc, HANTRO_IOCG_ANYCORE_WAIT_POLLING,
                      (CORE_WAIT_OUT *)waitOut)) {
        PTRACE_E("ioctl HANTRO_IOCG_ANYCORE_WAIT_POLLING failed\n");
        ret = EWL_HW_WAIT_ERROR;
        break;
      } else if (((CORE_WAIT_OUT *)waitOut)->irq_num != 0) {
        ret = EWL_OK;
        break;
      }
    }

    return ret;
  }

  core_id = FIRST_CORE(enc);
  core_type = EWLGetCoreTypeByClientType(enc->clientType);
  reg = &enc->reg_all_cores[core_id].core[core_type];
  hwId = reg->pRegBase[0];

  /* The function should return when a slice is ready */
  if (slicesReady) prevSlicesReady = *slicesReady;

  if (timeout == (u32)(-1)) {
    loop = -1;      /* wait forever (almost) */
    timeout = 1000; /* 1ms polling interval */
  }

  t.tv_sec = 0;
  t.tv_nsec = timeout - t.tv_sec * 1000;
  // t.tv_nsec = 100 * 1000 * 1000;
  t.tv_nsec = 100 * 1000;

  do {
    /* Get the number of completed slices from ASIC registers. */
    if (slicesReady) *slicesReady = (reg->pRegBase[7] >> 17) & 0xFF;

#ifdef PCIE_FPGA_VERI_LINEBUF
    /* Only for verification purpose, to test input line buffer with hardware handshake mode. */
    if (pollInputLineBufTestFunc) pollInputLineBufTestFunc();
#endif

    irq_stats = reg->pRegBase[1];
    PTRACE_I("EWLWaitHw: IRQ stat = %08x\n", irq_stats);

    if ((irq_stats & ASIC_STATUS_ALL)) {
      /* clear all IRQ bits. */
      wClr = (HW_ID_MAJOR_NUMBER(hwId) >= 0x61 || HW_PRODUCT_VC9000(hwId) ||
              HW_PRODUCT_VC9000LE(hwId) ||
              (HW_PRODUCT_SYSTEM60(hwId) && HW_ID_MINOR_NUMBER(hwId) >= 1))
                 ? irq_stats
                 : (irq_stats & (~(ASIC_STATUS_ALL | ASIC_IRQ_LINE)));
      EWLWriteBackReg(inst, 0x04, wClr);
      ret = EWL_OK;
      loop = 0;
    }
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
    u32 value = 0;

    if (reg->pRegBase[118] & 0x01) {
      // Slice Info timeout
      EWLWriteBackReg(inst, 118*4, 0x01);
      ret = EWL_OK;
      loop = 0;

      irq_stats |= ASIC_STATUS_POLL_SLICEINFO_TIMEOUT;
    }
#endif

#ifdef SUPPORT_UFBC
    regMapping *reg_ufbc = NULL;
    u32 core_type_ufbc;
    core_type_ufbc = EWLGetCoreTypeByClientType(EWL_CLIENT_TYPE_UFBC);
    reg_ufbc = &enc->reg_all_cores[core_id].core[core_type_ufbc];
    u32 ufbc_err_status = (reg_ufbc->pRegBase[8] & 0x1) << 14;
    if (ufbc_err_status & ASIC_STATUS_UFBC_DEC_ERR) {
      EWLWriteRegbyClientType(inst, 32, 0x1,
                        EWL_CLIENT_TYPE_UFBC);
      ret = EWL_OK;
      loop = 0;
      if (!irq_stats)
        irq_stats = ufbc_err_status;
      else
        irq_stats |= ufbc_err_status;
    }
    u32 ufbc_err_cfg_status = (reg_ufbc->pRegBase[8] & 0x2) << 14;
    if (ufbc_err_cfg_status & ASIC_STATUS_UFBC_DEC_ERR) {
      EWLWriteRegbyClientType(inst, 32, 0x2,
                        EWL_CLIENT_TYPE_UFBC);
      ret = EWL_OK;
      loop = 0;
      if (!irq_stats)
        irq_stats = ufbc_err_cfg_status;
      else
        irq_stats |= ufbc_err_cfg_status;
    }
#endif

    if (slicesReady) {
      if (*slicesReady > prevSlicesReady) {
        ret = EWL_OK;
        /*loop = 0; */
      }
    }

    if (loop) {
      if (nanosleep(&t, NULL) != 0) {
        PTRACE_I("EWLWaitHw: Sleep interrupted!\n");
      }
    }
  } while (loop--);

  *status_register = irq_stats;

  asic_status = irq_stats; /* update the buffered asic status */

  if (slicesReady) {
    PTRACE_I("EWLWaitHw: slicesReady = %d\n", *slicesReady);
  }
  PTRACE_I("EWLWaitHw: asic_status = %x\n", asic_status);
  PTRACE_I("EWLWaitHw: OK!\n");

  return ret;
}
#else //POLLING_ISR
i32 EWLWaitHwRdy(const void *inst, u32 *slicesReady, void *waitOut,
                 u32 *status_register) {
  vcx_cwl_t *enc;
  i32 ret = EWL_HW_WAIT_OK;
  u32 prevSlicesReady = 0;
  u32 core_info = 0;
  u32 core_id = 0;
  u32 core_type;

  PTRACE_I("EWLWaitHw: Start\n");

  enc = (vcx_cwl_t *)inst;

  /* Check invalid parameters */
  if (enc == NULL) {
    ASSERT(0);
    return EWL_HW_WAIT_ERROR;
  }

  core_type = EWLGetCoreTypeByClientType(enc->clientType);

  if (slicesReady) prevSlicesReady = *slicesReady;


  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return EWL_OK;
  }

  ASSERT(sizeof(EWLCoreWaitOut_t) == sizeof(CORE_WAIT_OUT));

  if (waitOut != NULL) {
    if (-1 == ioctl(enc->fd_enc, HANTRO_IOCG_ANYCORE_WAIT,
                    (CORE_WAIT_OUT *)waitOut)) {
      PTRACE_E("ioctl HANTRO_IOCG_ANYCORE_WAIT failed\n");
      ret = EWL_HW_WAIT_ERROR;
    } else
      ret = EWL_OK;

    return ret;
  }

  core_info |= (FIRST_CORE(enc) << 4);
  core_info |= core_type;

  ret = core_info;
  if ((core_id = ioctl(enc->fd_enc, HANTRO_IOCG_CORE_WAIT, &ret)) == -1) {
    PTRACE_E("ioctl HANTRO_IOCG_CORE_WAIT failed\n");
    ret = EWL_HW_WAIT_ERROR;
    goto out;
  }

  if (slicesReady)
    *slicesReady =
        (enc->reg_all_cores[FIRST_CORE(enc)].core[core_type].pRegBase[7] >>
         17) &
        0xFF;

  if (FIRST_CORE(enc) != core_id) ret = EWL_HW_WAIT_ERROR;
out:
  *status_register = ret;
  PTRACE_I("EWLWaitHw: OK!\n");

  return EWL_OK;
}
#endif //POLLING_ISR

i32 EWLReserveHw(const void *inst, u32 *core_info, u32 *job_id) {
  vcx_cwl_t *enc;
  u32 c = 0;
  u32 i = 0, valid_num = 0;
  i32 ret;
  u8 subsys_mapping = 0;
  u32 core_info_hw;
  u32 core_type, job_idx;

  PTRACE_I("EWLReserveHw: PID %d trying to reserve ...\n", GETPID());

  enc = (vcx_cwl_t *)inst;
  /* Check invalid parameters */
  if (enc == NULL) return EWL_ERROR;

  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return EWL_OK;
  }

  core_info_hw = *core_info;
  core_type = EWLGetCoreTypeByClientType(enc->clientType);
  if (EWL_IS_JPEG_CLIENT(enc->clientType)) core_type = CORE_VCEJ;

  core_info_hw |= (core_type & 0xFF);
  ret = ioctl(enc->fd_enc, HANTRO_IOCH_ENC_RESERVE, &core_info_hw);

  if (ret < 0) {
    PTRACE_E("EWLReserveHw failed\n");
    return EWL_ERROR;
  } else {
    PTRACE_I("EWLReserveHw successed\n");
  }

  core_type = EWLGetCoreTypeByClientType(enc->clientType);
  subsys_mapping = (u8)(core_info_hw & 0xFF);

  if (job_id != NULL) *job_id = (core_info_hw >> 16);

  i = 0;
  while (subsys_mapping) {
    if (subsys_mapping & 0x1) {
      enc->reg.core_id = i;
      enc->reg.regSize = enc->reg_all_cores[i].core[core_type].regSize;
      enc->reg.regBase = enc->reg_all_cores[i].core[core_type].regBase;
      enc->reg.pRegBase = enc->reg_all_cores[i].core[core_type].pRegBase;
      PTRACE_I("core %d is reserved\n", i);
      break;
    }
    subsys_mapping = subsys_mapping >> 1;
    i++;
  }

#ifdef MULTICORE_SUPPORT
  pthread_mutex_lock(&ewl_mutex);
  EWLWorker *worker = (EWLWorker *)queue_tail(&enc->freelist);
  while (worker && worker->core_id != enc->reg.core_id) {
    worker = (EWLWorker *)worker->next;
  }
  if (worker) {
    queue_remove(&enc->freelist, (struct node *)worker);
    queue_put(&enc->workers, (struct node *)worker);
  }
  pthread_mutex_unlock(&ewl_mutex);
#endif

  EWLWriteReg(enc, 0x14, 0);  //disable encoder

  PTRACE_I("EWLReserveHw: ENC HW locked by PID %d, TID %d\n", GETPID(), GETTID());

  return EWL_OK;
}

/*******************************************************************************
 Function name   : EWLReleaseHw
 Description     : Release HW resource when frame is ready
*******************************************************************************/
void EWLReleaseHw(const void *inst) {
  vcx_cwl_t *enc;
  u32 val;
  u32 core_info = 0;
  u32 core_id;
  u32 core_type;

  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);

  if (enc->vcmd_mode == VCMD_MODE_ENABLED) {
    return;
  }

  core_id = FIRST_CORE(enc);
  core_type = EWLGetCoreTypeByClientType(enc->clientType);

  if (EWL_IS_JPEG_CLIENT(enc->clientType)) core_type = CORE_VCEJ;

  enc->performance =
      EWLReadReg(inst, 82 * 4);  //save the performance before release hw

  core_info |= enc->unCheckPid << 31;
  core_info |= core_id << 4;
  core_info |= core_type;

  val = EWLReadReg(inst, 0x14);
  EWLWriteBackReg(inst, 0x14, val & (~0x01)); /* reset ASIC */
  enc->reg.core_id = -1;
  enc->reg.regSize = 0;
  enc->reg.regBase = 0;
  enc->reg.pRegBase = NULL;

  PTRACE_I("EWLReleaseHw: PID %d trying to release ...\n", GETPID());

#ifdef MULTICORE_SUPPORT
  pthread_mutex_lock(&ewl_mutex);
  EWLWorker *worker = (EWLWorker *)queue_get(&enc->workers);
  if (worker) {
    queue_remove(&enc->workers, (struct node *)worker);
    queue_put(&enc->freelist, (struct node *)worker);
  }
  pthread_mutex_unlock(&ewl_mutex);
#endif

  ioctl(enc->fd_enc, HANTRO_IOCH_ENC_RELEASE, &core_info);

  PTRACE_I("EWLReleaseHw: HW released by PID %d\n", GETPID());
  return;
}

/*******************************************************************************
 Function name   : EWLGetCoreNum
 Description     : Get the total num of cores
 Return type     : u32 ID
 Argument        : void
*******************************************************************************/
u32 EWLGetCoreNum(const void *ctx) {
  static u32 core_num = 0;
  vcx_cwl_t *ewl_ctx = (vcx_cwl_t *)ctx;

  /* Check invalid parameters */
  ASSERT(ewl_ctx != NULL);

  if (ewl_ctx->vcmdEnable == 1) return core_num;

  if (core_num == 0) {
    ioctl(ewl_ctx->fd_enc, HANTRO_IOCG_CORE_NUM, &core_num);
  }

  PTRACE_I("EWLGetCoreNum: %d\n", core_num);
  return core_num;
}

void EWLTraceProfile(const void *inst, void *prof_data, i32 qp, i32 poc) {}

void EWLDCacheRangeFlush(const void *inst, EWLLinearMem_t *info) {}

void EWLDCacheRangeRefresh(const void *inst, EWLLinearMem_t *info) {}

/*******************************************************************************
 Function name   : EWLSetVCMDMode
 Description     : Set VCMD work mode to bypass(0) or enable(1)
 Return type     : void
 Argument        : mode: 0-disable, 1-enable
*******************************************************************************/
void EWLSetVCMDMode(const void *inst, u32 mode) {
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);

  if (enc->vcmdEnable) {
    /* TBD, need to communicate with kernel driver to bypass/enable vcmd,
           then set enc->vcmd_mode correspondingly.
           For now, just disable this feature here */
    //enc->vcmd_mode = mode ? VCMD_MODE_ENABLED : VCMD_MODE_DISABLED;
    (void)mode;
  }
}

/*******************************************************************************
 Function name   : EWLGetVCMDMode
 Description     : Get VCMD work mode to bypass(0) or enable(1)
 Return type     : mode: 0-disable, 1-enable
 Argument        : -
*******************************************************************************/
u32 EWLGetVCMDMode(const void *inst) {
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);

  return (enc->vcmd_mode == VCMD_MODE_DISABLED) ? 0 : 1;
}

u32 EWLGetVCMDSupport(const void *inst) {
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);

  return enc->vcmdEnable;
}

void EWLAttach(const void *ctx, int slice_idx, i32 vcmd_support) {
  (void)ctx;
  (void)slice_idx;
  (void)vcmd_support;
}

void EWLDetach() { return; }

u32 EWLReleaseEwlWorkerInst(const void *inst) { return 0; }

void EWLClearTraceProfile(const void *inst) {}

i32 EWLGetConfigRegister(const void *inst, u32 core_id, u32 client_type, u32 offset) {
  vcx_cwl_t *enc;
  u32 config_reg = 0;
  u32 *status_addr;

  enc = (vcx_cwl_t *)inst;
  u32 core_type = EWLGetCoreTypeByClientType(client_type);

  if (enc->vcmd_mode == VCMD_MODE_DISABLED) {
    regMapping *reg;
    reg = &(enc->reg_all_cores[core_id].core[core_type]);
    config_reg = *((u32 *)((u8 *)reg->pRegBase + offset));
  } else {
    status_addr = enc->vcmd_cmdbuf_info.reg_virt_addr +
                  enc->vcmd_cmdbuf_info.reg_unit_size / 4 *
                      0;
    status_addr += EWLGetClientOffset(inst, client_type) / 4;
    config_reg = *(status_addr + offset / 4);
  }
  return config_reg;
}

void EWLSetUfbcInfo(const void *inst, u32 mode, u32 irq_offset) {
  vcx_cwl_t *enc;

  enc = (vcx_cwl_t *)inst;
  ASSERT(enc != NULL);

  enc->ufbcMode= mode;
  enc->ufbcIrqOffset = irq_offset;
}
i32 EWLGetVCMDId(const void *inst) {
  vcx_cwl_t *enc;
  u32 vcmd_id = 0;
  u32 *status_addr;

  enc = (vcx_cwl_t *)inst;

  status_addr = enc->vcmd_cmdbuf_info.reg_virt_addr +
                enc->vcmd_cmdbuf_info.reg_unit_size / 4 *
                    0;
  vcmd_id = *status_addr;

  return vcmd_id;
}

u32 EWLIsVCMDSupportM2REG(const void *inst) {
  if ((EWLGetVCMDId(inst) & 0xFFFF) < 0x1506) {
    return EWL_ERROR;
  } else {
    return EWL_OK;
  }
}
