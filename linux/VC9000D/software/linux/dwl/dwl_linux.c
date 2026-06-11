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

#include "dwl_linux.h"
#include "regdrv.h"
#ifdef __FREERTOS__
#include "dev_common_freertos.h"
#include "user_freertos.h"
#include "memalloc_freertos.h"
#elif defined(__linux__)
#include "dwl_linux_tb.h"
#include "memalloc.h"
#else //For other os
//TODO...
#endif
#include "sw_util.h"
#include "deccfg.h"
#include "dwl_memsync.h"

#ifdef __FREERTOS__
//nothing
#elif defined(__linux__)
#include <semaphore.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#if !ANDROID
#include <sys/timeb.h>
#endif
#include <sys/types.h>
#include <unistd.h>
#endif
#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
#include "dwl_linux_dec400.h"
#endif
#endif
#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#ifdef FPGA_PERF_AND_BW
#include "dwl_perf_info.h"
#endif

#define DWL_PJPEG_E 22    /* 1 bit */
#define DWL_REF_BUFF_E 20 /* 1 bit */

#define DWL_JPEG_EXT_E 31        /* 1 bit */
#define DWL_REF_BUFF_ILACE_E 30  /* 1 bit */
#define DWL_MPEG4_CUSTOM_E 29    /* 1 bit */
#define DWL_REF_BUFF_DOUBLE_E 28 /* 1 bit */

#define DWL_MVC_E 20 /* 2 bits */

#define DWL_DEC_TILED_L 17   /* 2 bits */
#define DWL_DEC_PIC_W_EXT 14 /* 2 bits */
#define DWL_EC_E 12          /* 2 bits */
#define DWL_STRIDE_E 11      /* 1 bit */
#define DWL_FIELD_DPB_E 10   /* 1 bit */
#define DWL_AVS_PLUS_E       6  /* 1 bit */
#define DWL_64BIT_ENV_E      5  /* 1 bit */

#define DWL_CFG_E 24         /* 4 bits */
#define DWL_PP_IN_TILED_L 14 /* 2 bits */

#define DWL_SORENSONSPARK_E 11 /* 1 bit */

#define DWL_DOUBLEBUFFER_E 1 /* 1 bit */

#define DWL_H264_FUSE_E 31          /* 1 bit */
#define DWL_MPEG4_FUSE_E 30         /* 1 bit */
#define DWL_MPEG2_FUSE_E 29         /* 1 bit */
#define DWL_SORENSONSPARK_FUSE_E 28 /* 1 bit */
#define DWL_JPEG_FUSE_E 27          /* 1 bit */
#define DWL_VP6_FUSE_E 26           /* 1 bit */
#define DWL_VC1_FUSE_E 25           /* 1 bit */
#define DWL_PJPEG_FUSE_E 24         /* 1 bit */
#define DWL_CUSTOM_MPEG4_FUSE_E 23  /* 1 bit */
#define DWL_RV_FUSE_E 22            /* 1 bit */
#define DWL_VP7_FUSE_E 21           /* 1 bit */
#define DWL_VP8_FUSE_E 20           /* 1 bit */
#define DWL_AVS_FUSE_E 19           /* 1 bit */
#define DWL_MVC_FUSE_E 18           /* 1 bit */
#define DWL_G2_HEVC_FUSE_E 17          /* 1 bit */
#define DWL_HEVC_FUSE_E 11          /* 1 bit */
#define DWL_G2_VP9_FUSE_E 6            /* 1 bit */
#define DWL_VP9_FUSE_E 10            /* 1 bit */

#define DWL_DEC_MAX_4K_FUSE_E 16   /* 1 bit */
#define DWL_DEC_MAX_1920_FUSE_E 15 /* 1 bit */
#define DWL_DEC_MAX_1280_FUSE_E 14 /* 1 bit */
#define DWL_DEC_MAX_720_FUSE_E 13  /* 1 bit */
#define DWL_DEC_MAX_352_FUSE_E 12  /* 1 bit */
#define DWL_REF_BUFF_FUSE_E 7      /* 1 bit */

#define DWL_PP_FUSE_E 31             /* 1 bit */
#define DWL_PP_DEINTERLACE_FUSE_E 30 /* 1 bit */
#define DWL_PP_ALPHA_BLEND_FUSE_E 29 /* 1 bit */
#define DWL_PP_MAX_4096_FUSE_E 16    /* 1 bit */
#define DWL_PP_MAX_1920_FUSE_E 15    /* 1 bit */
#define DWL_PP_MAX_1280_FUSE_E 14    /* 1 bit */
#define DWL_PP_MAX_720_FUSE_E 13     /* 1 bit */
#define DWL_PP_MAX_352_FUSE_E 12     /* 1 bit */

#define DWL_G2_FORMAT_CUSTOMER1_E 6     /* 1 bit */
#define DWL_G2_FORMAT_P010_E 5          /* 1 bit */

#define DWL_FORMAT_CUSTOMER1_E 26     /* 1 bit */
#define DWL_FORMAT_P010_E 25          /* 1 bit */
#define DWL_ADDR_64_E 4              /* 1 bit */


DECLARE_PERFORMANCE_STATIC(decode_push_reg)
DECLARE_PERFORMANCE_STATIC(decode_pull_reg)

#define G1_ASIC_ID_IDX 0
#define G2_ASIC_ID_IDX 1

#ifdef _DWL_PERFORMANCE
u32 hw_malloc_total_max;
#endif

#ifdef _DWL_FAKE_HW_TIMEOUT
static void DWLFakeTimeout(u32 *status);
#endif

static inline u32 CheckRegOffset(struct HANTRODWL *dec_dwl, u32 offset) {
  return offset < MAX_REG_COUNT * 4;
}

void PrintIrqType(u32 core_id, u32 status) {
  if (status & DEC_IRQ_ABORT)
    IRQTRACE_I("DEC[%d] IRQ ABORT\n", core_id);
  else if (status & DEC_IRQ_RDY)
    IRQTRACE_I("DEC[%d] IRQ READY\n", core_id);
  else if (status & DEC_IRQ_BUS)
    IRQTRACE_I("DEC[%d] IRQ BUS ERROR\n", core_id);
  else if (status & DEC_IRQ_BUFFER)
    IRQTRACE_I("DEC[%d] IRQ BUFFER\n", core_id);
  else if (status & DEC_IRQ_ASO)
    IRQTRACE_I("DEC[%d] IRQ ASO\n", core_id);
  else if (status & DEC_IRQ_ERROR)
    IRQTRACE_I("DEC[%d] IRQ STREAM ERROR\n", core_id);
  else if (status & DEC_IRQ_SLICE)
    IRQTRACE_I("DEC[%d] IRQ SLICE\n", core_id);
  else if (status & DEC_IRQ_TIMEOUT)
    IRQTRACE_I("DEC[%d] IRQ TIMEOUT\n", core_id);
  else if (status & DEC_IRQ_LAST_SLICE_INT)
    IRQTRACE_I("DEC[%d] IRQ LAST_SLICE_INT\n", core_id);
  else if (status & DEC_IRQ_NO_SLICE_INT)
    IRQTRACE_I("DEC[%d] IRQ NO_SLICE_INT\n", core_id);
  else if (status & DEC_IRQ_EXT_TIMEOUT)
    IRQTRACE_I("DEC[%d] IRQ EXT_TIMEOUT\n", core_id);
  else if (status & DEC_IRQ_SCAN_RDY)
    IRQTRACE_I("DEC[%d] IRQ SCAN RDY\n", core_id);
  else
    IRQTRACE_I("DEC[%d] IRQ UNKNOWN 0x%08x\n", core_id, status);
}

/*------------------------------------------------------------------------------
    Function name   : DWLMapRegisters
    Description     :

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 *DWLMapRegisters(int mem_dev, /*unsigned long*/ addr_t base, unsigned int reg_size,
                     u32 write) {
  const int page_size = getpagesize();
  const int page_alignment = page_size - 1;

  size_t map_size;
  const char *io = MAP_FAILED;

  /* increase mapping size with unaligned part */
  map_size = reg_size + (base & page_alignment);

  /* map page aligned base */
  if (write)
    io = (char *)mmap(0, map_size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_dev,
                      base & ~page_alignment);
  else
    io = (char *)mmap(0, map_size, PROT_READ, MAP_SHARED, mem_dev,
                      base & ~page_alignment);

  /* add offset from alignment to the io start address */
  if (io != MAP_FAILED) io += (base & page_alignment);

  return (u32 *)io;
}

void DWLUnmapRegisters(const void *io, unsigned int reg_size) {
  const int page_size = getpagesize();
 av_unused const int page_alignment = page_size - 1;

  munmap((void *)((long)io & (~page_alignment)),
         reg_size + ((long)io & page_alignment));
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicCoreCount
    Description     : Return the number of hardware cores available

------------------------------------------------------------------------------*/
u32 DWLReadAsicCoreCount(const void *instance) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  return dev->num_cores;
}

static u32 DWLGetCoreIdByClientType(const void *instance, enum DWLClientType client_type,
                                    u32 *core_mask) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 i = 0, matched_1st_core_id = dev->num_cores;

  *core_mask = 0;
#define IS_SUPPORT(a, b)  (client_type == (a) && dev->hw_features[i]->b)

  for (i=0; i<dev->num_cores; i++) {
    if (IS_SUPPORT(DWL_CLIENT_TYPE_H264_DEC, h264_support) ||
        IS_SUPPORT(DWL_CLIENT_TYPE_H264_DEC, h264_adv_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_MPEG4_DEC, mpeg4_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_JPEG_DEC, jpeg_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_PP, pp_standalone))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VC1_DEC, vc1_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_MPEG2_DEC, mpeg2_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VP6_DEC, vp6_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_AVS_DEC, avs_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_RV_DEC, rv_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VP8_DEC, vp8_support) ||
             IS_SUPPORT(DWL_CLIENT_TYPE_VP8_DEC, vp7_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VP9_DEC, vp9_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_HEVC_DEC, hevc_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_ST_PP, pp_standalone))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_H264_MAIN10, h264_high10_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_AVS2_DEC, avs2_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_AV1_DEC, av1_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VVC_DEC, vvc_support))
      *core_mask |= 1 << i;

    if (matched_1st_core_id == dev->num_cores && *core_mask != 0)
      matched_1st_core_id = i;
  }

  return matched_1st_core_id;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadAsicID(const void *instance, enum DWLClientType client_type) {
  u32 asic_id = 0, core_id, core_mask = 0;
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  DTRACE_I("client_type=%d\n", client_type);
  ASSERT(client_type < DWL_CLIENT_TYPE_MAX);

  core_id = DWLGetCoreIdByClientType(instance, client_type, &core_mask);
  if (core_id == dev->num_cores) {
    ASSERT(core_mask == 0);
    DTRACE_E("ERROR! this device not support this client_type %d\n", client_type);
    return 0;
  }

  asic_id = dev->asic_id[core_id];
  DTRACE_I("asic_id=%x\n", asic_id);

  return asic_id;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadCoreHwBuildID
    Description     : Read the HW build ID. Does not need a DWL instance to run

    Return type     : u32 - the HW Build ID
------------------------------------------------------------------------------*/
u32 DWLReadCoreHwBuildID(const void *instance, u32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  return dev->hw_build_id[core_id];
}

const void *DWLGetHwFeaturesByID(const void *instance, u32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  return dev->hw_features[core_id];
}

const void *DWLGetHwFeaturesByClientType(const void *instance, enum DWLClientType client_type, u32 *core_mask)
{
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  u32 core_id;
  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  core_id = DWLGetCoreIdByClientType(instance, client_type, core_mask);
  if (core_id == dev->num_cores) {
    ASSERT(*core_mask == 0);
    DTRACE_E("ERROR! this device not support this client_type %d\n", client_type);
    return 0;
  }

  return dev->hw_features[core_id];
}

/*------------------------------------------------------------------------------
    Function name   : DWLMallocRefFrm
    Description     : Allocate a frame buffer (contiguous linear RAM memory)

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : void *info - place where the allocated memory buffer
                        parameters are returned
------------------------------------------------------------------------------*/
enum DWLRet DWLMallocRefFrm(const void *instance, u64 size, struct DWLLinearMem *info) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  MEMTRACE_I("DWLMallocRefFrm\t%8d bytes\n", size);

#ifdef _DWL_PERFORMANCE
  dec_dwl->hw_reference_total_max += size;
#endif

  return DWLMallocLinear(instance, size, info);
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        DWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : void *info - frame buffer memory information
------------------------------------------------------------------------------*/
void DWLFreeRefFrm(const void *instance, struct DWLLinearMem *info) {
  MEMTRACE_I("DWLFreeRefFrm: %8d\n", info->size);
  DWLFreeLinear(instance, info);
}
#ifdef SUPPORT_VCMD_M2M
/*------------------------------------------------------------------------------
    Function name   : DWLCheckVcmdM2M
    Description     : Check if vcmd support m2m

    Return type     : u32

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
u32 DWLCheckVcmdM2M(const void *instance) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  return DEV_HAS_M2M(dev);
}
#endif

/*------------------------------------------------------------------------------
Function name   : DWLmmapBuffer
Description     : mmap the bus address of VPU buffer to virtual address

Return type     : u32* - virtual address after mmap

Argument        : const void * instance - DWL instance
Argument        : MemallocParams params - parameters of memory alloc
Argument        : struct DWLLinearMem *buff - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
static u32 *DWLmmapBuffer(const void *inst, MemallocParams params,
                          struct DWLLinearMem *buff) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;
  ASSERT(dec_dwl != NULL);
  ASSERT(buff != NULL);
  u32 *mmap_addr = MAP_FAILED;
  u32 mem_type = buff->mem_type & 0x00FF;
  DWLMemNode *dev = (DWLMemNode *)dec_dwl->mem_dev;


  /* Map the bus address to virtual address */
  mmap_addr = (u32 *)mmap(0, buff->size, PROT_READ | PROT_WRITE, MAP_SHARED,
                         dev->base.fd, params.bus_address);

  if ((mem_type != DWL_MEM_TYPE_DPB) &&
      (mem_type != DWL_MEM_TYPE_VPU_ONLY)) {
    if (mmap_addr == MAP_FAILED) {
      MEMTRACE_I("DWLmmapBuffer: Failed to mmap busAddress: %p\n",
               (void *)params.bus_address);
    }
  }

  return mmap_addr;
}

/*------------------------------------------------------------------------------
Function name   : DWLunmmapBuffer
Description     : unmmap the virtual address mmapped from bus address of VPU

Return type     : void

Argument        : u32 *virtual_address - alignment virtual address
Argument        : u32 *alloc_virtual_addr - allocated virtual address
Argument        : u32 size - size in byte of buffer
------------------------------------------------------------------------------*/
static void DWLunmmapBuffer(u32 *virtual_address, u32 *alloc_virtual_addr,
                            u32 size) {
  if (virtual_address != MAP_FAILED)
    munmap(alloc_virtual_addr, size);
}

#ifdef SUPPORT_MMU
/*------------------------------------------------------------------------------
Function name   : DWLMMUMemoryMap
Description     : MMU mmap the VPU buffer

Return type     : void

Argument        : const void * instance - DWL instance
Argument        : MemallocParams params - parameters of memory alloc
Argument        : The parameter of alignment
Argument        : struct DWLLinearMem *buff - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
static void DWLMMUMemoryMap(const void *inst, MemallocParams params,
                            u32 alignment, struct DWLLinearMem *buff) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  struct addr_desc addr;
  u32 *mmu_map_addr = NULL;
  ASSERT(buff != NULL);

#ifndef DMA_TRANS_TEST
  mmu_map_addr = buff->alloc_virtual_addr;
#else
  struct sync_mem *priv = (struct sync_mem *)buff->priv;
  if (priv->dev_va_alloc != MAP_FAILED)
    mmu_map_addr = priv->dev_va_alloc;
  else {
    mmu_map_addr = (u32 *)buff->alloc_bus_addr;
    DTRACE_E("%s","DWLMMUMemoryMap map device memory (VPU only) not supported yet!\n");
    return;
  }
#endif

  if (mmu_map_addr != MAP_FAILED) {
    addr.virtual_address = mmu_map_addr;
    addr.size = params.size;

    mlock(addr.virtual_address, addr.size);
    ioctl(dev->base.fd, HANTRO_IOCS_MMU_MEM_MAP, &addr);
    buff->bus_address = NEXT_MULTIPLE(addr.bus_address, alignment);
  }
}

/*------------------------------------------------------------------------------
Function name   : DWLMMUMemoryUnMap
Description     : MMU unmmap the VPU buffer

Return type     : void

Argument        : const void * instance - DWL instance
Argument        : struct DWLLinearMem *buff - place where the allocated memory
                  buffer parameters are returned
------------------------------------------------------------------------------*/
static void DWLMMUMemoryUnMap(const void *inst, struct DWLLinearMem *buff) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  struct addr_desc addr = {0};
  u32 *mmu_unmap_addr = MAP_FAILED;
  ASSERT(buff != NULL);

#ifndef DMA_TRANS_TEST
  mmu_unmap_addr = buff->alloc_virtual_addr;
#else
  struct sync_mem *priv = (struct sync_mem *)buff->priv;
  if (priv->dev_va_alloc != MAP_FAILED)
    mmu_unmap_addr = priv->dev_va_alloc;
  else {
    DTRACE_E("%s","DWLMMUMemoryUnMap map device memory(VPU only) not supported yet!\n");
    return;
  }
#endif

  if (mmu_unmap_addr != MAP_FAILED) {
    addr.virtual_address = mmu_unmap_addr;
    ioctl(dev->base.fd, HANTRO_IOCS_MMU_MEM_UNMAP, &addr);
  }
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : void *info - place where the allocated memory buffer
                        parameters are returned
------------------------------------------------------------------------------*/
enum DWLRet DWLMallocLinear(const void *instance, u64 size, struct DWLLinearMem *info) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  MemallocParams params = {0};
  u64 alignment = MAX(DEC_X170_BUS_ADDR_ALIGNMENT, getpagesize());
  i32 ret = DWL_OK;

  ASSERT(dec_dwl != NULL);
  ASSERT(info != NULL);
  DWLMemNode *mem_dev = (DWLMemNode *)dec_dwl->mem_dev;

  MEMTRACE_I("DWLMallocLinear\t%8d bytes \n", size);

  info->logical_size = size;
  size = NEXT_MULTIPLE(size, alignment);
  info->size = size;
  info->virtual_address = MAP_FAILED;
  info->bus_address = 0;

  params.size = info->size;
  params.mem_type = info->mem_type;
  /* allocate device bus_address */
  ioctl(mem_dev->base.fd, MEMALLOC_IOCXGETBUFFER, &params);
  if (params.bus_address == 0) {
    DTRACE_E("%s", "ERROR! No linear buffer available\n");
    return DWL_ERROR;
  }
  /* The bus address for mmap and HW may be different. translation_offset
   * is used to calculate the bus address for HW access. If no translation is
   * needed memalloc-driver sets it to 0.
   * Notice: info->bus_address is the device address in normal case. And if MMU
   * is used, the info->bus_address is the mmu address and info->alloc_bus_addr
   * is the device address */
  info->alloc_bus_addr = BUS_CPU_TO_ASIC(params.bus_address, params.translation_offset);
  info->bus_address = NEXT_MULTIPLE(info->alloc_bus_addr, alignment);
  if (sizeof(info->bus_address) == 8 && (info->bus_address >> 32) != 0) {
    MEMTRACE_I("DWLMallocLinear: allocated bus_address overflow 32 bit: (%p),"
               " please ensure HW support 64bits address space\n",  (void *)params.bus_address);
  }

  info->alloc_virtual_addr = MAP_FAILED;
  ret = DWLMemSyncAllocHostBuffer(instance, info->size, alignment, info);
  if (ret == DWL_NOT_SUPPORT) {
    info->alloc_virtual_addr = DWLmmapBuffer(instance, params, info); /* Map the bus address to virtual address */
    if (info->alloc_virtual_addr == MAP_FAILED)
      return DWL_ERROR;
    info->virtual_address = info->alloc_virtual_addr + (info->bus_address - info->alloc_bus_addr);
  }

#ifdef DMA_TRANS_TEST  //The premise is open DMA_TRANS_TEST, otherwise, it doesn't make sense
  mem_dev->translation_offset = params.translation_offset;
  info->priv = DWLmalloc(sizeof(struct sync_mem));
  struct sync_mem *priv = (struct sync_mem *)info->priv;
  priv->dev_va_alloc = MAP_FAILED;

  if (info->virtual_address) { //When open DMA_TRANS_TEST, It indicates that there must be a buffer on the host side,
    //If necessary test Memory sync, can map the buffer on the device side to priv
    priv->dev_pa_alloc = params.bus_address;
    priv->dev_va_alloc = DWLmmapBuffer(instance, params, info);
    priv->dev_va = priv->dev_va_alloc + (info->bus_address - info->alloc_bus_addr);
    MEMTRACE_I("DWLMallocLinear VPU buffer for Memory Sync Test\t%p\n", priv->dev_va_alloc);
  }
#endif

#ifdef SUPPORT_MMU
  DWLMMUMemoryMap(instance, params, alignment, info);
#endif

  MEMTRACE_I("DWLMallocLinear 0x%llx virtual_address: %p (type %d)\n", info->bus_address,
         info->virtual_address, info->mem_type);

#ifdef ENABLE_FPGA_VERIFICATION
  if(info->virtual_address != NULL) {
    DWLmemset(info->virtual_address, 0, info->size);
  } else {
    DWLLinearMemset(dec_dwl, info, 0, 0, info->size);
  }
#endif

#ifdef _DWL_PERFORMANCE
  dec_dwl->hw_linear_total_max += size;
#endif


  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeLinear
    Description     : Release a linera memory buffer, previously allocated with
                        DWLMallocLinear.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : void *info - linear buffer memory information
------------------------------------------------------------------------------*/
void DWLFreeLinear(const void *instance, struct DWLLinearMem *info) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLMemNode *dev;
  i32 ret;

  ASSERT(dec_dwl != NULL);
  ASSERT(info != NULL);

#ifdef SUPPORT_MMU
  DWLMMUMemoryUnMap(instance, info);
#endif

#ifdef DMA_TRANS_TEST
  struct sync_mem *priv = (struct sync_mem *)info->priv;
  DWLunmmapBuffer(priv->dev_va, priv->dev_va_alloc, info->size);
  MEMTRACE_I("DWLFreeLinear VPU buffer for Memory Sync Test\t%p\n", priv->dev_va_alloc);
  priv->dev_va = NULL;
  priv->dev_va_alloc = NULL;
  DWLfree(info->priv);
  info->priv = NULL;
#endif

  ret = DWLMemSyncFreeHostBuffer(instance, info);
  if (ret == DWL_NOT_SUPPORT) {
    DWLunmmapBuffer(info->virtual_address, info->alloc_virtual_addr, info->size);
    info->alloc_virtual_addr = NULL;
  }

  dev = (DWLMemNode *)dec_dwl->mem_dev;
  if (info->bus_address != 0) {
    ioctl(dev->base.fd, MEMALLOC_IOCSFREEBUFFER, &info->alloc_bus_addr);
    info->alloc_bus_addr = 0;
  }

  info->bus_address = 0;
  info->virtual_address = NULL;
  info->size = 0;
  info->logical_size = 0;
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteReg
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/

void DWLWriteReg(const void *instance, i32 core_id, u32 offset, u32 value) {
  DWLDecNode *dev  = NULL;
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  REGTRACE_I("core[%d] DWLWriteReg swreg[%d] at offset 0x%02X = %08X\n", core_id, offset / 4,
            offset, value);
  ASSERT(dec_dwl != NULL);
  ASSERT(CheckRegOffset(dec_dwl, offset));

  dev = (DWLDecNode *)dec_dwl->dev;
  ASSERT(core_id < (i32)dev->num_cores);

  if (!DEV_USE_VCMD(dev)) {
    u32 *reg_base = &dev->dec_shadow_regs[core_id];

    offset = offset / 4;

    reg_base[offset] = value;

#ifdef INTERNAL_TEST
    InternalTestDumpWriteSwReg(core_id, offset, value, reg_base);
#endif
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteRegs
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32* from - the start address of the copy
    Argument        : u32 num_regs - the num of regs value to be written out
    Argument        : u32 print_flag - whether print regs info
------------------------------------------------------------------------------*/
void DWLWriteRegs(const void *instance, i32 core_id, u32 offset, u32* from, u32 num_regs, u32 print_flag) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
#ifdef VCD_LOGMSG
  if (print_flag) {
    u32 i;
    for (i = 0; i < num_regs; i++)
      REGTRACE_I("core[%d] DWLWriteReg swreg[%d] at offset 0x%02X = %08X\n", core_id, (offset + 4 * i) >> 2,
              (offset + 4 * i), *(from + i));
  }
#endif
  ASSERT(dec_dwl != NULL);
  ASSERT(CheckRegOffset(dec_dwl, offset));
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];
  ASSERT(core_id < (i32)dev->num_cores);

  DWLmemcpy(&reg_base[offset >> 2], from, num_regs * 4);

#ifdef INTERNAL_TEST
  if (print_flag) {
    u32 i;
    for (i = 0; i < num_regs; i++)
      InternalTestDumpWriteSwReg(core_id, (offset + 4 * i) >> 2, *(from + i), reg_base);
  }
#endif
}



#ifdef FPGA_PERF_AND_BW
/*------------------------------------------------------------------------------
    Function name   : DWLReadBw
    Description     : Read the bw infomation when cache using

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 num - 0 is read bandwidth and 1 is write bandwidth
------------------------------------------------------------------------------*/
u32 DWLReadBw(const void *instance, u32 core_id, u32 num){

  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  u32 val;

  ASSERT(dec_dwl != NULL);
  ASSERT(num < 2 && num >= 0);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  ASSERT((u32)core_id < dev->num_cores);

#ifdef SUPPORT_AXIFE
  val = dev->bw_axife_rd_wr[core_id][num];
  return val;
#endif

  if(num == 0)
    val = (DWLReadReg(dec_dwl, core_id, 300*4));
  else
    val = (DWLReadReg(dec_dwl, core_id, 304*4));

  return val;
}
void DWLPerfInfoCollect(const void *instance, i32 core_id, struct DWLPerfInfo *perf_info) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

  perf_info->bitrate = dev->bytes_consumed_perf[core_id] ;

  perf_info->cycles = reg_base[63];

#ifdef SUPPORT_AXIFE
  perf_info->read_bw = dev->bw_axife_rd_wr[core_id][0];
  perf_info->write_bw = dev->bw_axife_rd_wr[core_id][1];
#else
  perf_info->read_bw = reg_base[300];
  perf_info->write_bw = reg_base[304];
#endif
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLReadReg
    Description     : Read the value of a hardware IO register

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadReg(const void *instance, i32 core_id, u32 offset) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  u32 val;

  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

  ASSERT(CheckRegOffset(dec_dwl, offset));
  ASSERT(core_id < (i32)dev->num_cores);

  offset = offset / 4;

  val = reg_base[offset];

  REGTRACE_I("core[%d] DWLReadReg swreg[%d] at offset 0x%02X = %08X\n", core_id, offset,
            offset * 4, val);
#ifdef INTERNAL_TEST
  InternalTestDumpReadSwReg(core_id, offset, val, reg_base);
#endif
  (void)dec_dwl;
  return val;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadRegs
    Description     : Read the value of a hardware IO register
    Return type     : u32 - the value stored in the register
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the first register to be read
    Argument        : u32* dest - the destination register
    Argument        : u32 num - the number of register to be read
------------------------------------------------------------------------------*/
void DWLReadRegs(const void *instance, i32 core_id, u32 offset, u32* dest, u32 num) {

  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  u32 i;
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

  for (i = 0; i < num; i++)
    ASSERT(CheckRegOffset(dec_dwl, (offset + i * 4)));

  ASSERT(core_id < (i32)dev->num_cores);

  DWLmemcpy(dest, &reg_base[offset>>2], num*4);

#ifdef VCD_LOGMSG
  for (i = 0; i < num; i++)
    REGTRACE_I("core[%d] DWLReadReg swreg[%d] at offset 0x%02X = %08X\n", core_id, (offset + i * 4) >> 2,
            offset + i * 4, reg_base[(offset + i * 4) >> 2]);
#endif

#ifdef INTERNAL_TEST
  for (i = 0; i < num; i++)
    InternalTestDumpReadSwReg(core_id, (offset + i * 4) >> 2, reg_base[(offset + i * 4) >> 2], reg_base);
#endif
  (void)dec_dwl;
}

void DWLWriteCoreRegs(const void *instance, u32 subsys_id,
                         u32 *regs, u32 reg_id, u32 count, enum CoreType type) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  core.id = subsys_id;
  core.regs = regs;
  core.reg_id = reg_id;
  core.size = count * 4;
  core.type = type;

  ASSERT(dec_dwl);
  ASSERT(regs);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_E("%s","ioctl HANTRODEC_IOCS_DEC_WRITE_REG failed\n");
    ASSERT(0);
  }
}

void DWLReadCoreRegs(const void *instance, u32 subsys_id,
                         u32 *regs, u32 reg_id, u32 count, enum CoreType type) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  core.id = subsys_id;
  core.regs = regs;
  core.reg_id = reg_id;
  core.size = count * 4;
  core.type = type;

  ASSERT(dec_dwl);
  ASSERT(regs);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
    DTRACE_E("%s","ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
    ASSERT(0);
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteRegToHw
    Description     : Write a value to a hardware IO register(when hardware is running)
                      Only used in low latency mode.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLWriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value)
{
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  struct core_desc core;
  u32 tmp_regs[MAX_REG_COUNT] = {0};

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;


  core.id = core_id;
  core.reg_id = offset / 4;
  tmp_regs[core.reg_id] = value;
  core.regs = &tmp_regs[core.reg_id];
  core.size = 4;
  core.type = HW_VCD;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
    DTRACE_E("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}


/*------------------------------------------------------------------------------
    Function name   : DWLReadRegFromHw
    Description     : Read the value of a hardware IO register(when hardware is running)
                      Only used in low latency mode.

    Return type     : u32 - the value stored in the register

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/

u32 DWLReadRegFromHw(const void *instance, i32 core_id, u32 offset)
{
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *) instance;
  struct core_desc core;
  u32 tmp_regs[MAX_REG_COUNT] = {0};

  ASSERT(dec_dwl);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  core.id = core_id;
  core.reg_id = offset / 4;
  core.regs = &tmp_regs[core.reg_id];
  core.size = 4;
  core.type = HW_VCD;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
    DTRACE_E("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }

  return tmp_regs[core.reg_id];
}

/*------------------------------------------------------------------------------
    Function name   : DWLmalloc
    Description     : Allocate a memory block. Same functionality as
                      the ANSI C malloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

    Argument        : u32 n - Bytes to allocate
------------------------------------------------------------------------------*/
void *DWLmalloc(size_t n) {

  MEMTRACE_I("DWLmalloc\t%8d bytes\n", n);

#ifdef _DWL_PERFORMANCE
  hw_malloc_total_max += n;
#endif


  return malloc(n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()

    Return type     : void

    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void DWLfree(void *p) {
  if (p != NULL) free(p);
}

/*------------------------------------------------------------------------------
    Function name   : DWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()

    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available

}
    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *DWLcalloc(size_t n, size_t s) {

  MEMTRACE_I("DWLcalloc\t%8d bytes\n", n * s);
  void *p = NULL;
#ifdef __FREERTOS__
    p = malloc((n)*(s));
    if(p)
      memset(p, 0, (n)*(s));
#else
    p = calloc(n, s);
#endif
#ifdef _DWL_PERFORMANCE
  hw_malloc_total_max += n * s;
#endif

    return p;
}

/*------------------------------------------------------------------------------
    Function name   : DWLmemcpy
    Description     : Copies characters between buffers. Same functionality as
                      the ANSI C memcpy()

    Return type     : The value of destination d

    Argument        : void *d - Destination buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *DWLmemcpy(void *d, const void *s, size_t n) {
  return memcpy(d, s, n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLmemset
    Description     : Sets buffers to a specified character. Same functionality
                      as the ANSI C memset()

    Return type     : The value of destination d

    Argument        : void *d - Pointer to destination
    Argument        : i32 c - Character to set
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
void *DWLmemset(void *d, i32 c, size_t n) {
  return memset(d, (int)c, n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLLinearMemcpy
    Description     : Copies characters from a buffer to Physical linear buffer.
                      The specific implementation is completed by the customer.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : addr_t device_bus_addr - Physical linear buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *DWLLinearMemcpy(const void *instance, addr_t device_bus_addr, const void *s, size_t n) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLMemNode *dev = (DWLMemNode *)dec_dwl->mem_dev;

  // reference codes
  u32 *virtual_address;
  u32 size = NEXT_MULTIPLE(n, DEC_X170_BUS_ADDR_ALIGNMENT);
  virtual_address=(u32 *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, dev->base.fd,
                      device_bus_addr);
  ASSERT(virtual_address == MAP_FAILED);
  void *ret = memcpy((void *)virtual_address, s, size);
  DWLDMATransData2(instance, device_bus_addr, (void *)virtual_address, size, HOST_TO_DEVICE);
  munmap(virtual_address, size);

  return ret;
}

/**
 * Sets Physical linear buffer to a specified character. Customer can adjust it.
 * \ingroup common_group
 * \param [in]     instance   Pointer to a DWL instance.
 * \param[in]      mem        memory information needs to be synchronized
 * \param[in]      offset     The offset of memsync in bytes; implemented alignment in API
 * \param [in]     c          Character to set
 * \param [in]     n          Number of characters
 */
void *DWLLinearMemset(const void *instance, struct DWLLinearMem *mem, i32 offset, i32 c, size_t n) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  // reference codes
  // u32 size = NEXT_MULTIPLE(n, DEC_X170_BUS_ADDR_ALIGNMENT);
  u32 size = n;
  u8 *host_vir_addr = NULL;
  av_unused addr_t device_bus_addr = mem->bus_address + offset;
  av_unused u8 buf[64];

  if (mem->virtual_address == NULL) {
    if (size > 64) {
      host_vir_addr = (u8 *)DWLmalloc(size);
      if (host_vir_addr == NULL) return (void *)NULL;
    } else {
      host_vir_addr = (u8 *)buf;
    }
  } else {
    host_vir_addr = (u8 *)mem->virtual_address + offset;
  }

  ASSERT(host_vir_addr != MAP_FAILED);
  DWLmemset((void *)host_vir_addr, (int)c, size);

  DWLDMATransData2(instance, device_bus_addr, (void *)host_vir_addr, size, HOST_TO_DEVICE);

  if (mem->virtual_address == NULL && (size > 64))
    DWLfree(host_vir_addr);

  return NULL;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFakeTimeout
    Description     : Testing help function that changes HW stream errors info
                        HW timeouts. You can check how the SW behaves or not.
    Return type     : void
    Argument        : void
------------------------------------------------------------------------------*/

#ifdef _DWL_FAKE_HW_TIMEOUT
void DWLFakeTimeout(u32 *status) {

  if ((*status) & DEC_IRQ_ERROR) {
    *status &= ~DEC_IRQ_ERROR;
    *status |= DEC_IRQ_TIMEOUT;
    DTRACE_E("%s","\nDwl: Change stream error to hw timeout\n");
  }
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLReadPpConfigure
    Description     : Read the pp configure
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : PpUnitIntConfig *ppu_cfg
------------------------------------------------------------------------------*/
void DWLReadPpConfigure(const void *instance, u32 core_id, void *ppu_cfg, u32 pjpeg_coeff_buffer_size)
{
#if 1
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  if (!DEV_USE_VCMD(dev)) {
    dev->ppu_cfg[core_id] = ppu_cfg;
  } else {
    /* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
    struct VcmdBuf *vcmd = &dev->vcmdb[core_id];

    vcmd->ppu_cfg = ppu_cfg;
  }
#endif
}

u32 DWLVcmdIsUsed(const void *instance) {
  struct subsys_desc subsys = {0};
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  if (ioctl(dev->base.fd, HANTRODEC_IOX_SUBSYS, &subsys) == -1)
    DTRACE_E("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");

  return subsys.subsys_vcmd_num;
}

