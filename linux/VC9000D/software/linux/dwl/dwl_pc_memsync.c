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
#include "dwl.h"
#include "dwl_swhw_sync.h"
#include "dwl_memsync.h"
#include "dec_log.h"

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
i32 DWLMemSyncAllocHostBuffer(const void *instance, u32 size, u32 alignment,
                                 struct DWLLinearMem *buff) {
  av_unused struct DWLInstance *inst = (struct DWLInstance *)instance;
  u32 mem_type = buff->mem_type & 0xFF00;

  if (mem_type == DWL_MEM_TYPE_DMA_HOST_TO_DEVICE ||
      mem_type == DWL_MEM_TYPE_DMA_DEVICE_TO_HOST ||
      mem_type == DWL_MEM_TYPE_DMA_HOST_AND_DEVICE) {
    buff->virtual_address = (u32 *)osal_aligned_malloc(alignment, size + LINMEM_ALIGN);
    if (buff->virtual_address == NULL)
      return DWL_ERROR;

    MEMTRACE_I("DWLMemSyncAllocHostBuffer: size %8d, virtual_address %p\n",
                size, (void *)buff->virtual_address);
  } else {
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
i32 DWLMemSyncFreeHostBuffer(const void *instance, struct DWLLinearMem *buff) {
  av_unused struct DWLInstance *inst = (struct DWLInstance *)instance;
  u32 mem_type = buff->mem_type & 0xFF00;

  if (mem_type == DWL_MEM_TYPE_DMA_HOST_TO_DEVICE ||
      mem_type == DWL_MEM_TYPE_DMA_DEVICE_TO_HOST ||
      mem_type == DWL_MEM_TYPE_DMA_HOST_AND_DEVICE) {
    if (buff->virtual_address != NULL) {
      MEMTRACE_I("DWLMemSyncFreeHostBuffer: size %8d, virtual_address %p\n",
                  buff->size, (void *)buff->virtual_address);
      osal_aligned_free(buff->virtual_address);
      return DWL_OK;
    }
  }

  return DWL_OK;
}

/*------------------------------------------------------------------------------
Function name   : DWLDMATransData
Description     : Synchronize the data of the host side
                  and device side Memory

Return value     : DWL_OK means successful data synchronization
                   DWL_ERROR means parameter is wrong

Argument        : const void * instance - DWL instance
Argument        : struct DWLLinearMem *mem - memory information needs to be synchronized
Argument        : i32 offset - the offset of memsync in bytes; implemented alignment in API
Argument        : u32 length - the length of the memory data synchronization.
Argument        : enum EWLMemSyncDirection dir - the direction of memory data synchronized
------------------------------------------------------------------------------*/
i32 DWLDMATransData(const void *instance, const struct DWLLinearMem *mem, i32 offset, u32 length,
                    enum DWLDMADirection dir) {
  assert(mem != NULL);
  assert(length <= mem->size);
  if (mem->virtual_address == NULL || mem->bus_address == 0) return DWL_ERROR;

  u32 *MemSyncVirtualAddress = mem->virtual_address;
  addr_t MemSyncBusAddress = mem->bus_address;
  if (offset) {
    MemSyncVirtualAddress = (u32 *)((u8 *)MemSyncVirtualAddress + offset);
    MemSyncBusAddress += offset;
  }

  if (dir == HOST_TO_DEVICE) {
    DWLmemcpy((void *)MemSyncBusAddress, (void *)MemSyncVirtualAddress, length);
  } else if (dir == DEVICE_TO_HOST) {
    DWLmemcpy((void *)MemSyncVirtualAddress, (void *)MemSyncBusAddress, length);
  }
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
i32 DWLDMATransData2(const void *instance, addr_t device_bus_addr, void *host_virtual_addr,
                      u32 length, enum DWLDMADirection dir) {
  av_unused struct DWLInstance *inst = (struct DWLInstance *)instance;

  if (length <= 0 || (dir != HOST_TO_DEVICE && dir != DEVICE_TO_HOST))
    return DWL_ERROR;

  u32 *MemSyncVirtualAddress = host_virtual_addr;
  addr_t MemSyncBusAddress = device_bus_addr;

  if (dir == HOST_TO_DEVICE) {
    DWLmemcpy((void *)MemSyncBusAddress, (void *)MemSyncVirtualAddress, length);
  } else if (dir == DEVICE_TO_HOST) {
    DWLmemcpy((void *)MemSyncVirtualAddress, (void *)MemSyncBusAddress, length);
  }

  return DWL_OK;
}
#endif
