/*
 * SPDX_license-Identifier: GPL-2.0  WITH Linux-syscall-note OR BSD-3-Clause
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */

#ifndef _VCX_MMU_H_
#define _VCX_MMU_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __FREERTOS__
#elif defined(__linux__)
#include <linux/fs.h>
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

#define MMU_VMID_MAX          (15) //bit[0-3]
#define MMU_PAGE_TABLE_ID_MAX (0xFFFF) // bit[0-16]
#define PAGE_TABLE_ARRAY_SIZE (1 << 16)

#define MMU_REG_OFFSET              0
#define MMU_REG_HW_ID          (MMU_REG_OFFSET + 6 * 4)
#define MMU_REG_FLUSH          (MMU_REG_OFFSET + 97 * 4)
#define MMU_REG_PAGE_TABLE_ID  (MMU_REG_OFFSET + 107 * 4)
#define MMU_REG_PTFLUSH        (MMU_REG_OFFSET + 109 * 4)
#define MMU_REG_CONTROL        (MMU_REG_OFFSET + 226 * 4)
#define MMU_REG_ADDRESS        (MMU_REG_OFFSET + 227 * 4)
#define MMU_REG_ADDRESS_MSB    (MMU_REG_OFFSET + 228 * 4)
#define MMU_REG_ARRAY_SIZE     (MMU_REG_OFFSET + 229 * 4)
#define MMU_REG_PDENTRY0       (MMU_REG_OFFSET + 237 * 4)

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

typedef unsigned long long addr64_t;

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

struct addr_desc {
	void *virtual_address; /* buffer virtual address */
	addr64_t bus_address; /* buffer physical address */
	unsigned int size; /* physical size */
};

struct kernel_addr_desc {
	addr64_t bus_address; /* buffer virtual address */
	addr64_t mmu_bus_address; /* buffer physical address in MMU*/
	unsigned int size; /* physical size */
};

struct page_table_flush {
	unsigned int flush_vmid[MMU_VMID_MAX + 1]; /* flush useless map cache line */
	int flush_cnt; /* count of need flush vmid */
};

struct page_table_switch {
	int id; /* the id of switched table page */
	struct page_table_flush pt_flush; /* flush page table by vmid*/
};


#define HANTRO_IOC_MMU  'm'

#define HANTRO_IOCS_MMU_MEM_MAP    _IOWR(HANTRO_IOC_MMU, 1, struct addr_desc *)
#define HANTRO_IOCS_MMU_MEM_UNMAP  _IOWR(HANTRO_IOC_MMU, 2, struct addr_desc *)
#define HANTRO_IOCS_MMU_FLUSH      _IOWR(HANTRO_IOC_MMU, 3, unsigned int *)
#define HANTRO_IOCS_MMU_SWITCH_PAGETABLE           _IOWR(HANTRO_IOC_MMU, 4, unsigned int *)
#define HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF _IOWR(HANTRO_IOC_MMU, 5, struct page_table_switch *)
//#define HANTRO_IOCS_MMU_ENABLE     _IOWR(HANTRO_IOC_MMU, 4, unsigned int *)
#define HANTRO_IOC_MMU_MAXNR 5
#define MAX_SUBSYS_NUM 4 /* up to 4 subsystem (temporary) */
#define HXDEC_MAX_CORES MAX_SUBSYS_NUM /* used in hantro_dec.c */
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
#endif
