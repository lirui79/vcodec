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
#include <linux/platform_device.h>


#ifdef __cplusplus
extern "C" {
#endif

#ifndef SUPPORT_48PA_MMU
#define MMU_VA_BITS   32
#endif

#if (MMU_VA_BITS == 40)
#define MMU_IOVA_BITS_MASK           0xFFFFFFFFFF
#elif (MMU_VA_BITS == 41)
#define MMU_IOVA_BITS_MASK           0x1FFFFFFFFFF
#else
#define MMU_IOVA_BITS_MASK           0xFFFFFFFF
#endif

#define MMU_REGIN_SIZE              ((MMU_IOVA_BITS_MASK + 1L) / 4)

#define REGION_IN_START          0x0
#define REGION_IN_END            MMU_REGIN_SIZE
#define REGION_OUT_START         REGION_IN_END
#define REGION_OUT_END           (REGION_OUT_START + MMU_REGIN_SIZE)
#define REGION_PRIVATE_START     REGION_OUT_END
#define REGION_PRIVATE_END       (REGION_PRIVATE_START + MMU_REGIN_SIZE)

#define REGION_IN_MMU_START      0x1000
#define REGION_IN_MMU_END        MMU_REGIN_SIZE
#define REGION_OUT_MMU_START     REGION_IN_MMU_END
#define REGION_OUT_MMU_END       (REGION_OUT_MMU_START + MMU_REGIN_SIZE)
#define REGION_PRIVATE_MMU_START REGION_OUT_MMU_END
#define REGION_PRIVATE_MMU_END   (REGION_PRIVATE_MMU_START + MMU_REGIN_SIZE)

extern unsigned long ddr_offset;
#ifdef EMU
  /************* 40VA to 48PA version *************/
  #define PDT_PCIE_START_ADDRESS      0x04100000

  #define PD2_PCIE_START_ADDRESS      0x04200000

  #define PD1_PCIE_START_ADDRESS      0x04300000

  #define DYNAMIC_PAGE_START_ADDRESS  0x04400000

  #define DYNAMIC_PAGE_END_ADDRESS    0x05000000

  /************* Master&Slave version *************/
  #define MTLB_PCIE_START_ADDRESS  0x04100000
  /* page_table_entry start address */
  #define PAGE_PCIE_START_ADDRESS  0x04200000

  #define STLB_PCIE_START_ADDRESS  0x04300000
#else
  /************* 40VA to 48PA version *************/
  #define PDT_PCIE_START_ADDRESS      (0x00100000 + ddr_offset)

  #define PD2_PCIE_START_ADDRESS      (0x00200000 + ddr_offset)

  #define PD1_PCIE_START_ADDRESS      (0x00300000 + ddr_offset)

  #define DYNAMIC_PAGE_START_ADDRESS  (0x00400000 + ddr_offset)

  #define DYNAMIC_PAGE_END_ADDRESS    (0x01000000 + ddr_offset)

  /************* Master&Slave version *************/
  #define MTLB_PCIE_START_ADDRESS       (0x00100000 + ddr_offset)
  /* page_table_entry start address */
  #define PAGE_PCIE_START_ADDRESS       (0x00200000 + ddr_offset)

  #define STLB_PCIE_START_ADDRESS       (0x00300000 + ddr_offset)
#endif
#define PTD_ENTRY_SIZE 16


enum MMUVersion {
	MMU_32VA_40PA = 1,
	MMU_40VA_48PA = 2,
	MMU_41VA_48PA = 3,

	MMU_NOT_SUPPORT = -1,
};

enum MMUStatus {
	MMU_STATUS_OK = 0,
	MMU_STATUS_PT_EXIST = 1,

	MMU_STATUS_FALSE = -1,
	MMU_STATUS_INVALID_ARGUMENT = -2,
	MMU_STATUS_INVALID_OBJECT = -3,
	MMU_STATUS_OUT_OF_MEMORY = -4,
	MMU_STATUS_NOT_FOUND = -19,
};

/* just use in 48PA MMU */
enum MMUPageLevel {
	MMU_PD2 = 0,
	MMU_PD1 = 1,
	MMU_PD0 = 2,
	MMU_PT = 3,
	MMU_NONE = -1,
};

struct kernel_addr_desc {
	addr64_t bus_address; /* buffer virtual address */
	addr64_t mmu_bus_address; /* buffer physical address in MMU*/
	unsigned int size; /* physical size */
};


#ifdef SUPPORT_MMU
extern struct sub_ip_init_cfg mmu_init_cfg[];
#endif


/* Init MMU, should be called in driver init function. */
enum MMUStatus MMUInit(volatile unsigned char *hwregs);
/* Clean up all data in MMU, should be called in driver cleanup function
 * when rmmod driver
 */
enum MMUStatus MMUCleanup(struct platform_device *platformdev);
/* The function should be called in driver realease function
 * when driver exit unnormally
 */
enum MMUStatus MMURelease(void *filp);

enum MMUStatus MMUSetup(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);

enum MMUStatus MMUEnable(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2], struct platform_device *platformdev);

/* Used in kernel to map buffer */
enum MMUStatus MMUKernelMemNodeMap(struct kernel_addr_desc *addr, void *filp);

/* Used in kernel to unmap buffer */
enum MMUStatus MMUKernelMemNodeUnmap(struct kernel_addr_desc *addr);

/* if support 48PA MMU, create page table for multi-process */
enum MMUStatus MMUCreatePageTable(void *filp);

unsigned int GetMMUPageTableArraySize(void);

unsigned long long GetMMUAddress(void);

long MMUIoctl(unsigned int cmd, void *filp, unsigned long arg, volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2]);

#ifdef __cplusplus
}
#endif

#endif //_VCX_MMU_PRIVATE_H_

