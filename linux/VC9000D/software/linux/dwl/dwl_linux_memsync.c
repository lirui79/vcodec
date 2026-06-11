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

/*------------------------------------------------------------------------------
1. Include headers
------------------------------------------------------------------------------*/
#include "dwl.h"
#include "dwl_linux.h"
#include "dwl_memsync.h"
#include "dec_log.h"
#include "sw_util.h"

#ifdef USE_DMA_DRIVER
#include "dma_drv.h"
#endif

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#ifdef SUPPORT_DMA

/*------------------------------------------------------------------------------
Function name   : DWLMemSyncAllocHostBuffer
Description     : Allocate a contiguous, linear RAM  memory buffer in host
                  when need independent memory instead of shared memory

Return type     : i32 - 0 for success or a negative error code

Argument        : const void * instance - DWL instance
Argument        : u32 alignment - alignment in bytes of the requested memory
Argument        : struct DWLLinearMem *buff - place where the allocated memory
                  buffer parameters are returned
Argument        : MemallocParams params - the parameters of alloc buffer
------------------------------------------------------------------------------*/
i32 DWLMemSyncAllocHostBuffer(const void *inst, u32 size, u32 alignment,
                              struct DWLLinearMem *buff) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;
  UNUSED(alignment);

  av_unused struct sync_mem *priv = (struct sync_mem *)buff->priv;
  /*host buffer Need client to adjust by their platform*/
  u32 mem_type = buff->mem_type & 0xFF00;
  if (mem_type == DWL_MEM_TYPE_DMA_HOST_TO_DEVICE ||
      mem_type == DWL_MEM_TYPE_DMA_DEVICE_TO_HOST ||
      mem_type == DWL_MEM_TYPE_DMA_HOST_AND_DEVICE) {
    buff->alloc_virtual_addr = (u32 *)DWLcalloc(1, size + LINMEM_ALIGN);
    buff->virtual_address = (u32 *)NEXT_ALIGNED(buff->alloc_virtual_addr);
  } else {
    buff->alloc_virtual_addr = NULL;
    buff->virtual_address = NULL;
  }

  return DWL_OK;
}

/*------------------------------------------------------------------------------
Function name   : DWLMemSyncFreeHostBuffer
Description     : Release a linera memory buffer in host side,
                  when need independent memory instead of shared memory.

Return type     : i32 - 0 for success or a negative error code


Argument        : const void * instance - DWL instance
Argument        : struct DWLLinearMem *buff - linear buffer memory information
------------------------------------------------------------------------------*/
i32 DWLMemSyncFreeHostBuffer(const void *inst, struct DWLLinearMem *buff) {
  UNUSED(inst);

  /*free host buffer: Need client to adjust by their platform*/
  if (buff->alloc_virtual_addr != NULL) {
    DWLfree(buff->alloc_virtual_addr);
    buff->alloc_virtual_addr = NULL;
  }
  return DWL_OK;
}

#ifdef USE_DMA_DRIVER
static void TransDataByDmaDriver(const void *inst, addr_t device_bus_addr, u32 *host_virtual_addr,
                                            u32 length, enum DWLDMADirection dir) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;
  DWLDmaNode *dev = (DWLDmaNode *)dec_dwl->dma_dev;
  dma_data_cfg tmp;
  if (host_virtual_addr == NULL)
    return;

  if ((u8 *)device_bus_addr == (u8 *)host_virtual_addr)
    return;

  if (length <= 0 || (dir != HOST_TO_DEVICE && dir != DEVICE_TO_HOST))
    return;

  tmp.dev_bus_address = device_bus_addr;
  tmp.host_virtual_address = host_virtual_addr;
  tmp.length = length;
  tmp.dir = dir;
  u32 bytes = 64 * 3 * 1024 * 1024;
  u32 dataleft = length;
  while(dataleft > 0) {
    if(tmp.length > bytes) {
      tmp.length = bytes;
      ioctl(dev->base.fd, IO_DATA_DMA_TRANS, &tmp);
      dataleft -= bytes;
      tmp.length = dataleft;
      tmp.host_virtual_address = (u8 *)tmp.host_virtual_address + bytes;
      tmp.dev_bus_address += bytes;
    } else {
      ioctl(dev->base.fd, IO_DATA_DMA_TRANS, &tmp);
      dataleft = 0;
    }
  }
  /* Customer implement their own DMA data transfer function here. */
 /* if (dir == HOST_TO_DEVICE) {
    DWLmemcpy((u8 *)device_bus_addr, (u8 *)host_virtual_addr, length);
  } else if (dir == DEVICE_TO_HOST) {
    DWLmemcpy((u8 *)host_virtual_addr, (u8 *)device_bus_addr, length);
  }*/
}
#endif

/**
 * \brief DMA data transfer between host memory and device memory. \n
 * Function will copy the data from host memory to device memory after
 * SW process the data in that host memory and copy the data from device
 * memory to host memory after HW process the data in that device memory.
 * \ingroup common_group
 * \param[in]    inst       Pointer to DWL instance.
 * \param[in]    mem        memory information needs to be synchronized
 * \param[in]    offset     the offset of memsync in bytes; implemented alignment in API
 * \param[in]    length     copy data length(byte)
 * \param[in]    dir        DMA transfer direction, \see enum DWLDMADirection
 * \param[out]   i32       Return value     :
 *                    DWL_OK means successful data synchronization
                      DWL_ERROR means parameter is wrong
                      DWL_HW_ERROR means failed to synchronize data with HW
*/
i32 DWLDMATransData(const void *inst, const struct DWLLinearMem *mem, i32 offset, u32 length,
                    enum DWLDMADirection dir) {
  av_unused struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;

  if (mem == NULL || mem->virtual_address == NULL || mem->bus_address == 0)
    return DWL_ERROR;
  if ((u8 *)mem->virtual_address == (u8 *)mem->bus_address)
    return DWL_ERROR;
  if (length > mem->size || length <= 0) return DWL_ERROR;
  if (dir != HOST_TO_DEVICE && dir != DEVICE_TO_HOST) return DWL_ERROR;

  u32 *MemSyncVirtualAddress = mem->virtual_address;
  addr_t MemSyncBusAddress = mem->bus_address;

#ifndef USE_DMA_DRIVER
#ifdef DMA_TRANS_TEST  //Just for test; Customer should make modification based on the actual DMA platform
  struct sync_mem *priv = (struct sync_mem *)mem->priv;
  MemSyncBusAddress = (addr_t)priv->dev_va;
  if (offset) {
    MemSyncVirtualAddress = (u32 *)((u8 *)MemSyncVirtualAddress + offset);
    MemSyncBusAddress += offset;
  }
  /* Customer implement their own DMA data transfer function here. */
  if (dir == HOST_TO_DEVICE) {
    DWLmemcpy((void *)MemSyncBusAddress, (void *)MemSyncVirtualAddress, length);
  } else if (dir == DEVICE_TO_HOST) {
    DWLmemcpy((void *)MemSyncVirtualAddress, (void *)MemSyncBusAddress, length);
  }
#endif
#else
  TransDataByDmaDriver(inst, MemSyncBusAddress, MemSyncVirtualAddress, length, dir);
#endif
  return DWL_OK;
}

/**
 * DMA data transfer between host memory and device memory.
 * \ingroup common_group
 * \param [in]     inst The dwl instance.
 * \param [in]     device_bus_addr                  Device memory address
 * \param [in]     host_virtual_addr                Host memory address
 * \param [in]     length                           Copy data length(byte)
 * \param [in]     dir                              DMA transfer direction. -HOST_TO_DEVICE. -DEVICE_TO_HOST.
 * \return <tt>DWL_OK</tt>
 * \return <tt>DWL_ERROR</tt>
 */
i32 DWLDMATransData2(const void *inst, addr_t device_bus_addr, void *host_virtual_addr,
                      u32 length, enum DWLDMADirection dir) {
  if (length <= 0 || (dir != HOST_TO_DEVICE && dir != DEVICE_TO_HOST))
    return DWL_ERROR;

  u32 *MemSyncVirtualAddress = (u32 *)host_virtual_addr;
  addr_t MemSyncBusAddress = device_bus_addr;
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)inst;
  DWLMemNode *dev = (DWLMemNode *)dec_dwl->mem_dev;

#ifndef USE_DMA_DRIVER
#ifdef DMA_TRANS_TEST
  /* Map the aligned bus address to virtual address */
  addr_t map_bus_addr = 0;
  u32 offset = 0;
  u32 *MemSyncMapBusAddress = NULL;
  u32 total_len = 0;
#ifdef SUPPORT_MMU
  DTRACE_E("%s","DWLMMUMemoryMap map device memory (VPU only) not support DMA trans yet!\n");
  return;
#else
  offset = MemSyncBusAddress & (PAGE_SIZE - 1);
  addr_t aligned_base_addr = MemSyncBusAddress - offset;
  map_bus_addr = BUS_ASIC_TO_CPU(aligned_base_addr, dec_dwl->mem_dev->translation_offset);
#endif
  total_len = offset + length;
  if (map_bus_addr)
    MemSyncMapBusAddress = (u32 *)mmap(0, total_len, PROT_READ | PROT_WRITE, MAP_SHARED,
                                       dev->base.fd, map_bus_addr);
  if (MemSyncMapBusAddress == MAP_FAILED || !MemSyncMapBusAddress) {
    assert(0);
    return DWL_ERROR;
  }
  /* Customer implement their own DMA data transfer function here. */
  u32 *MemSyncStartBusAddress = (u32 *)((u8 *)MemSyncMapBusAddress + offset);
  // u32 *MemSyncStartBusAddress = (u32 *)(map_bus_addr + offset);
  if (dir == HOST_TO_DEVICE) {
    DWLmemcpy((void *)MemSyncStartBusAddress,(void *) MemSyncVirtualAddress, length);
  } else if (dir == DEVICE_TO_HOST) {
    DWLmemcpy((void *)MemSyncVirtualAddress, (void *)MemSyncStartBusAddress, length);
  }
 if (map_bus_addr)
    munmap(MemSyncMapBusAddress, total_len);
#endif
#else
  TransDataByDmaDriver(inst, MemSyncBusAddress, MemSyncVirtualAddress, length, dir);
#endif

  return DWL_OK;
}

#endif