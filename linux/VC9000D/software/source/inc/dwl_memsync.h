/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2021 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------

--
--  Abstract : header file of Encoder Wrapper Layer Common Part
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/

#ifndef __DWL_MEMSYNC_H__
#define __DWL_MEMSYNC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "dwl.h"

#ifdef SUPPORT_DMA
/**
 * \addtogroup dwl_mem
 *
 * @{
 */
/** \brief The address information of the buffer for test verification of memory synchronization.
 * \n <b>NOTE</b>: This structure can be overwritten. */
struct sync_mem {
  /** \brief The aligned virtual address of the buffer from the host view. */
  u32 *dev_va;
  /** \brief The aligned physical address of the buffer from the host view. */
  addr_t dev_pa_alloc;
  /** \brief The original virtual address of the buffer in device from the host view. */
  u32 *dev_va_alloc;
};

/** Transfers data between the host and device.
 *
 * This function is available only if memory synchronization is supported.
 *
 * \param [in] inst The dwl instance.
 * \param [in] mem The buffer information used for synchronization.
 * \param [in] offset The offset related to the hardware register base address, in bytes.
 * \param [in] length The number of bytes to be synchronized.
 * \param [in] dir The direction of transferring data.
 * \return <tt>DWL_OK</tt>
 * \return <tt>DWL_ERROR</tt>
 */
i32 DWLDMATransData(const void *inst, const struct DWLLinearMem *mem, i32 offset, u32 length,
                    enum DWLDMADirection dir);

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
                     u32 length, enum DWLDMADirection dir);

/** Allocates memory for a buffer in the host.
 *
 * This function is available only if memory synchronization is supported.
 *
 * \param [in] alignment The alignment for allocation.
 * \param [in] size The bytes to be allocated.
 * \param [out] buff The information of the memory allocated as a buffer in the host.
 * \return <tt>DWL_OK</tt>
 */
i32 DWLMemSyncAllocHostBuffer(const void *inst, u32 size, u32 alignment,
                              struct DWLLinearMem *buff);

/** Releases the memory of a buffer in the host.
 *
 * This function is available only if memory synchronization is supported.
 *
 * \param [in] buff  The information of the memory to be released.
 * \return <tt>DWL_OK</tt>
 */
i32 DWLMemSyncFreeHostBuffer(const void *inst, struct DWLLinearMem *buff);

/**@}*/
#else

#ifdef DMA_TRANS_TEST
#error "If want to define DMA_TRANS_TEST, the premise is open SUPPORT_DMA, otherwise, it doesn't make sense"
#endif

#define DWLDMATransData(inst, mem, offset, length, dir)
#define DWLDMATransData2(inst, ba, va, length, dir)
#define DWLMemSyncAllocHostBuffer(inst, size, alignment, buff) (DWL_NOT_SUPPORT)
#define DWLMemSyncFreeHostBuffer(inst, buff) (DWL_NOT_SUPPORT)
#endif

#ifdef __cplusplus
}
#endif

#endif /* __DWL_MEMSYNC_H__ */
