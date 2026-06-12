/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                     include vcx mmu private headers                          **
*********************************************************************************/

#ifndef _VCX_MMU_PRIVATE_H_
#define _VCX_MMU_PRIVATE_H_

#include "vcx_mmu.h"


#ifdef __cplusplus
extern "C" {
#endif

/* Init MMU, should be called in driver init function. */
enum MMUStatus MMUInit(volatile unsigned char *hwregs);
/* Clean up all data in MMU, should be called in driver cleanup function
 * when rmmod driver
 */
enum MMUStatus MMUCleanup(void);
/* The function should be called in driver realease function
 * when driver exit unnormally
 */
enum MMUStatus MMURelease(void *filp);

enum MMUStatus MMUSetup(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);
enum MMUStatus MMUEnable(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);

/* Used in kernel to map buffer */
enum MMUStatus MMUKernelMemNodeMap(struct kernel_addr_desc *addr, void *filp);

/* Used in kernel to unmap buffer */
enum MMUStatus MMUKernelMemNodeUnmap(struct kernel_addr_desc *addr);

/* if support 48PA MMU, create page table for multi-process */
enum MMUStatus MMUCreatePageTable(void *filp);

unsigned int GetMMUPageTableArraySize(void);

unsigned long long GetMMUAddress(void);
long MMUIoctl(unsigned int cmd, void *filp, unsigned long arg,
	      volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);

#ifdef __cplusplus
}
#endif

#endif //_VCX_MMU_PRIVATE_H_

