/*
 * SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
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


#include <linux/version.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#if (KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE)
#include <linux/dma-contiguous.h>
#else
#include <linux/dma-map-ops.h>
#endif

#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/dma-buf.h>
#include <asm/io.h>
#include <linux/stddef.h>

#include "vcx_mmu_priv.h"



//#define PD_MODE

/***** 48PA MMU Defination **********/
#ifdef SUPPORT_48PA_MMU
#define MMU_VA_LEN    MMU_VA_BITS

#define MMU_PD2_SHIFT 39
#define MMU_PD1_SHIFT 30
#define MMU_PD0_SHIFT 21
#define MMU_PT_64K_SHIFT 16
#define MMU_PT_4K_SHIFT 12

#define MMU_PD2_BITS          (MMU_VA_LEN - MMU_PD2_SHIFT)
#define MMU_PD1_BITS          (MMU_PD2_SHIFT - MMU_PD1_SHIFT)
#define MMU_PD0_BITS          (MMU_PD1_SHIFT - MMU_PD0_SHIFT)
#define MMU_PT_64K_BITS       (MMU_PD0_SHIFT - MMU_PT_64K_SHIFT)
#define MMU_PT_4K_BITS        (MMU_PD0_SHIFT - MMU_PT_4K_SHIFT)

#define MMU_PD2_ENTRY_NUM     BIT(MMU_PD2_BITS)
#define MMU_PD2_SIZE          (MMU_PD2_ENTRY_NUM << 3)
#define MMU_PD1_ENTRY_NUM     BIT(MMU_PD1_BITS)
#define MMU_PD1_SIZE          (MMU_PD1_ENTRY_NUM << 3)
#define MMU_PD0_ENTRY_NUM     BIT(MMU_PD0_BITS)
#define MMU_PD0_SIZE          (MMU_PD0_ENTRY_NUM << 3)
#define MMU_PT_64K_ENTRY_NUM  BIT(MMU_PT_64K_BITS)
#define MMU_PT_64K_SIZE       (MMU_PT_64K_ENTRY_NUM << 3)
#define MMU_PT_4K_ENTRY_NUM   BIT(MMU_PT_4K_BITS)
#define MMU_PT_4K_SIZE        (MMU_PT_4K_ENTRY_NUM << 3)

#define MMU_64K_SHIFT         MMU_PT_64K_SHIFT
#define MMU_4k_SHIFT          MMU_PT_4K_SHIFT
#else
/***** 40PA MMU Defination **********/
#define MMU_MTLB_SHIFT 22
#define MMU_STLB_4K_SHIFT 12
#define MMU_STLB_64K_SHIFT 16

#define MMU_MTLB_BITS            (32 - MMU_MTLB_SHIFT)
#define MMU_PAGE_4K_BITS         MMU_STLB_4K_SHIFT
#define MMU_STLB_4K_BITS         (32 - MMU_MTLB_BITS - MMU_PAGE_4K_BITS)
#define MMU_PAGE_64K_BITS        MMU_STLB_64K_SHIFT
#define MMU_STLB_64K_BITS        (32 - MMU_MTLB_BITS - MMU_PAGE_64K_BITS)

#define MMU_MTLB_ENTRY_NUM       BIT(MMU_MTLB_BITS)
#define MMU_MTLB_SIZE            (MMU_MTLB_ENTRY_NUM << 2)
#define MMU_STLB_4K_ENTRY_NUM    BIT(MMU_STLB_4K_BITS)
#define MMU_STLB_4K_SIZE         (MMU_STLB_4K_ENTRY_NUM << 2)
#define MMU_PAGE_4K_SIZE         BIT(MMU_STLB_4K_SHIFT)
#define MMU_STLB_64K_ENTRY_NUM   BIT(MMU_STLB_64K_BITS)
#define MMU_STLB_64K_SIZE        (MMU_STLB_64K_ENTRY_NUM << 2)
#define MMU_PAGE_64K_SIZE        BIT(MMU_STLB_64K_SHIFT)

#define MMU_MTLB_MASK            (~((1U << MMU_MTLB_SHIFT) - 1))
#define MMU_STLB_4K_MASK         ((~0U << MMU_STLB_4K_SHIFT) ^ MMU_MTLB_MASK)
#define MMU_PAGE_4K_MASK         (MMU_PAGE_4K_SIZE - 1)
#define MMU_STLB_64K_MASK        ((~((1U << MMU_STLB_64K_SHIFT) - 1)) ^ MMU_MTLB_MASK)
#define MMU_PAGE_64K_MASK        (MMU_PAGE_64K_SIZE - 1)

/* Page offset definitions. */
#define MMU_OFFSET_4K_BITS       (32 - MMU_MTLB_BITS - MMU_STLB_4K_BITS)
#define MMU_OFFSET_4K_MASK       ((1U << MMU_OFFSET_4K_BITS) - 1)
#define MMU_OFFSET_16K_BITS      (32 - MMU_MTLB_BITS - MMU_STLB_16K_BITS)
#define MMU_OFFSET_16K_MASK      ((1U << MMU_OFFSET_16K_BITS) - 1)

#define MMU_MTLB_ENTRY_HINTS_BITS 6
#define MMU_MTLB_ENTRY_STLB_MASK  (~((1U << MMU_MTLB_ENTRY_HINTS_BITS) - 1))

#define MMU_MTLB_PRESENT         0x00000001
#define MMU_MTLB_EXCEPTION       0x00000002
#define MMU_MTLB_4K_PAGE         0x00000000

#define MMU_STLB_PRESENT         0x00000001
#define MMU_STLB_EXCEPTION       0x00000002
#define MMU_STLB_4K_PAGE         0x00000000

#define MMU_64K_SHIFT         MMU_STLB_64K_SHIFT
#define MMU_4k_SHIFT          MMU_STLB_4K_SHIFT
#endif

#if (MMU_VA_BITS == 40)
#define PTD_FILLED_START_BIT   6
#else
/* 32 bit va or 41 - 48 bit va */
#define PTD_FILLED_START_BIT   12
#endif

#ifdef SUPPORT_48PA_MMU
#define PD2_SIZE_ALIGN   (1 << PTD_FILLED_START_BIT)
#define PD1_SIZE_ALIGN   (1 << 12)
#endif

/** upper alignment */
#ifndef _ALIGN
#define _ALIGN(size, align) (((size) + (align) - 1) & ~((align) - 1))
#endif

#define PAGE_NODE_FREE 1
#define PAGE_NODE_USE  0

#define MMU_FALSE                0
#define MMU_TRUE                 1

#define MMU_ERR_OS_FAIL          (0xffff)
#define MMU_EFAULT               MMU_ERR_OS_FAIL
#define MMU_ENOTTY               MMU_ERR_OS_FAIL

#define MMU_INFINITE             ((u32)~0U)

#define MAX_NOPAGED_SIZE         0x20000
#define MMU_SUPPRESS_OOM_MESSAGE 1

#if MMU_SUPPRESS_OOM_MESSAGE
#define MMU_NOWARN __GFP_NOWARN
#else
#define MMU_NOWARN 0
#endif

#define MMU_IS_ERROR(status)     ((status) < 0)
#define MMU_NO_ERROR(status)     ((status) >= 0)
#define MMU_IS_SUCCESS(status)   ((status) == MMU_STATUS_OK)

//#define HANTROMMU_DEBUG
#undef MMUDEBUG
#ifdef HANTROMMU_DEBUG
#ifdef __KERNEL__
#define MMUDEBUG(fmt, args...) pr_info("vcx_mmu: " fmt, ##args)
#else
#define MMUDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define MMUDEBUG(fmt, args...)
#endif

#define MMU_ON_ERROR(func)                                                     \
	do {                                                                   \
		status = func;                                                 \
		if (MMU_IS_ERROR(status)) {                                    \
			goto onerror;                                          \
		}                                                              \
	} while (MMU_FALSE)

#define WritePageEntry(page_entry, entry_value)                                \
	(*(unsigned int *)(page_entry) = (unsigned int)(entry_value))

#define WritePDEntry(pd_entry, entry_value)                                           \
	do {                                                                          \
		*((unsigned int *)(pd_entry)) = *((unsigned int *)entry_value);       \
		*((unsigned int *)pd_entry + 1) = *((unsigned int *)entry_value + 1); \
	} while (MMU_FALSE)

#define ReadPageEntry(page_entry) (*(unsigned int *)(page_entry))

#if KERNEL_VERSION(6, 5, 0) > LINUX_VERSION_CODE
#define PTE_UNLOCK(pte, ptl)    pte_unmap_unlock(pte, ptl)
#else
#define PTE_UNLOCK(pte, ptl)    spin_unlock(ptl)
#endif

/* simple map mode: generate mmu address which is same as input bus address*/
static unsigned int simple_map;
/* this shift should be an integral multiple of mmu page size(4096).
 * It can generate a mmu address shift in simple map mode
 */
static unsigned int map_shift;
/* set dma_used as 1 when dma is using, and set the reserved base address in host */
unsigned int dma_used;
unsigned long host_base;

/* module_param(name, type, perm) */
module_param(simple_map, uint, 0);
module_param(map_shift, uint, 0);
module_param(dma_used, uint, 0);
module_param(host_base, ulong, 0);
enum MMURegion {
	MMU_REGION_IN,
	MMU_REGION_OUT,
	MMU_REGION_PRIVATE,
	MMU_REGION_PUB,

	MMU_REGION_COUNT
};

struct MMUNode {
	void *buf_virtual_address;
	addr64_t buf_bus_address; /* used in kernel map mode */

	// start&end not the real bus_addr(addr >> 12, 4k page size)
	addr64_t addr_align_start;
	addr64_t addr_align_end;

	unsigned int page_count;
	int process_id;
	struct file *filp;

	struct MMUNode *next;
	struct MMUNode *prev;
};

struct MMUPageNode {
	/* start&end not the real bus_addr(addr >> 3, 8 bytes per entry) */
	addr64_t page_start;
	addr64_t page_end;

	addr64_t page_offset;
	enum MMUPageLevel page_level;
	/* page node in used and page entry are bound each other */
	void *page_table_entry;

	int process_id;
	unsigned int page_count;
	/* how many entry in used for current entry(4k mode: max=512, min=0) */
	int use_count;
	struct file *filp;

	struct MMUPageNode *next;
	struct MMUPageNode *prev;
};

struct MMUDDRRegion {
	addr64_t physical_address;
	addr64_t virtual_address;
	unsigned int page_count;

	void *node_mutex;
	struct MMUNode *simple_map_head;
	struct MMUNode *simple_map_tail;
	struct MMUNode *free_map_head;
	struct MMUNode *map_head;
	struct MMUNode *free_map_tail;
	struct MMUNode *map_tail;
};

struct MMUProcessObject {
	/* descriptor ID: bit[0-3]: VMID; bit[4-15]: page table id */
	unsigned int desc_id;
	/* process file pointer*/
	struct file *filp;
	/* process id*/
	int process_id;

	/* PTD(Page table descriptor) information */
	addr64_t ptd_physical;
	void *ptd_virtual;

	/* PD2(Page Directory) information */
	addr64_t pd2_physical;
	void *pd2_virtual;

	/* PD1 information */
	addr64_t pd1_physical;
	void *pd1_virtual;

	/* region node
	 * if want the MMU maped bus address is repeated, it should be
	 * associated with process.
	 */
	struct MMUDDRRegion region[MMU_REGION_COUNT];

	/* next mmu process object */
	struct MMUProcessObject *next;
	unsigned int pd_filled;
};

struct MMU {
	/************* Master&Slave version *************/
	/* Master TLB information. */
	unsigned int mtlb_size;
	addr64_t mtlb_physical;
	void *mtlb_virtual;
	unsigned int mtlb_entries;

	/* Slave TLB information */
	unsigned int stlb_size;
	addr64_t stlb_physical;
	void *stlb_virtual;

	/************* 48PA version *************/
	/* MMU process object list */
	struct MMUProcessObject *mmu_po;
	/* current MMU Process object */
	struct MMUProcessObject *mmu_po_curr;
	/* last MMU process object */
	struct MMUProcessObject *mmu_po_tail;

	/* PD2(Page Directory) information */
	unsigned int pd2_size; // all page table pd2 size
	unsigned int pd2_unit_size; // one page table pd2 size
	addr64_t pd2_physical; // page table p2 physical start address
	void *pd2_virtual; // page table p2 virtual start address

	/* PD1 information */
	unsigned int pd1_size;
	unsigned int pd1_unit_size;
	addr64_t pd1_physical;
	void *pd1_virtual;

	/* dynamic page information (common page pool for different page table) */
	unsigned int dy_pg_size;
	addr64_t dy_pg_physical;
	void *dy_pg_virtual;
	addr64_t dy_pg_physical_base;
	struct MMUPageNode *page_use_head;
	struct MMUPageNode *page_use_tail;
	struct MMUPageNode *page_free_head;
	struct MMUPageNode *page_free_tail;

	/************* common  *************/
	void *page_table_mutex;
	int enabled;
	unsigned int mmu_version;
	unsigned int page_table_array_size;
	unsigned int page_table_array_unit_size;
	addr64_t page_table_array_physical;
	void *page_table_array;
	int page_table_id;
	int init_process_id;
	/* Record the vmid if the process has been destroyed.
	 * Avoid dead vmid occupied the cache line.
	 */
	unsigned int vmid_waiting_flush[MMU_VMID_MAX + 1];
};

static struct MMU *g_mmu;
#ifdef PCIE_EN
extern unsigned long gBaseDDRHw;
#endif
unsigned int mmu_enable = MMU_FALSE;
static unsigned int mmu_init = MMU_FALSE;

#ifndef PCIE_EN
extern struct platform_device *platformdev;
#endif

#ifdef SUPPORT_48PA_MMU
static unsigned int mmu_page_shift[4] = {
		MMU_PD2_SHIFT - MMU_PT_4K_SHIFT,
		MMU_PD1_SHIFT - MMU_PT_4K_SHIFT,
		MMU_PD0_SHIFT - MMU_PT_4K_SHIFT,
		0
	};
static unsigned int mmu_page_bits[4] = {
		(1 << MMU_PD2_BITS) - 1,
		(1 << MMU_PD1_BITS) - 1,
		(1 << MMU_PD0_BITS) - 1,
		(1 << MMU_PT_4K_BITS) - 1
	};
#endif

#ifdef SUPPORT_MMU
struct sub_ip_init_cfg mmu_init_cfg[] = {
	{MMU_REG_ADDRESS, 0x0000},		//not move, sw will update mmu addr LSB to index0
	{MMU_REG_ADDRESS_MSB, 0x0000},	//not move, sw will update mmu addr MSB to index1
#ifdef SUPPORT_48PA_MMU
	{MMU_REG_ARRAY_SIZE, 0x0001},
#endif
	{MMU_REG_PAGE_TABLE_ID, 0x10000},
	{MMU_REG_PAGE_TABLE_ID, 0x00000},
	{MMU_REG_CONTROL, 0x0001},
	{0xffff, }	//end guard
};
#endif

static inline int GetProcessID(void)
{
	return current->tgid;
}

static enum MMUStatus ZeroMemory(void *memory, unsigned int bytes)
{
	memset(memory, 0, bytes);

	return MMU_STATUS_OK;
}

static enum MMUStatus AllocateMemory(unsigned int bytes, void **memory)
{
	void *pointer;
	enum MMUStatus status;

	if (bytes > MAX_NOPAGED_SIZE) {
		pointer = vmalloc(bytes);
		MMUDEBUG(" *****VMALLOC size*****%u\n", bytes);
	} else {
		pointer = kmalloc(bytes, GFP_KERNEL | MMU_NOWARN | __GFP_DMA32);
		MMUDEBUG(" *****KMALLOC size*****%u\n", bytes);
	}

	if (!pointer) {
		/* Out of memory. */
		status = MMU_STATUS_OUT_OF_MEMORY;
		goto onerror;
	}

	/* Return pointer to the memory allocation. */
	*memory = pointer;

	return MMU_STATUS_OK;

onerror:
	/* Return the status. */
	return status;
}

static enum MMUStatus FreeMemory(void *memory)
{
	/* Free the memory from the OS pool. */
	if (is_vmalloc_addr(memory)) {
		MMUDEBUG("*****VFREE*****%p\n", memory);
		vfree(memory);
	} else {
		MMUDEBUG("*****KFREE*****%p\n", memory);
		kfree(memory);
	}
	return MMU_STATUS_OK;
}

static enum MMUStatus SMDeleteNode(struct MMUNode **pp)
{
	(*pp)->prev->next = (*pp)->next;
	(*pp)->next->prev = (*pp)->prev;

	MMUDEBUG(" *****DeleteNode size*****%u\n", (*pp)->page_count);
	FreeMemory(*pp);

	return MMU_STATUS_OK;
}

static enum MMUStatus DeleteNode(struct MMUNode **pp)
{
	(*pp)->prev->next = (*pp)->next;
	(*pp)->next->prev = (*pp)->prev;

	MMUDEBUG(" *****%s size*****%u\n", __func__, (*pp)->page_count);
	FreeMemory(*pp);

	return MMU_STATUS_OK;
}

#ifdef SUPPORT_48PA_MMU
static enum MMUStatus DeletePageNode(struct MMUPageNode *p)
{
	p->prev->next = p->next;
	p->next->prev = p->prev;

	MMUDEBUG("*****%s size*****%u\n", __func__, p->page_count);
	FreeMemory(p);

	return MMU_STATUS_OK;
}
#endif

static enum MMUStatus MergeNode(struct MMUNode *h, struct MMUNode **pp)
{
	struct MMUNode *tmp0 = h->next;
	struct MMUNode *tmp1 = h->next;

	while (tmp0) {
		/* 1th step: find front contiguous memory node */
		if (tmp0->addr_align_end == (*pp)->addr_align_start) {
			tmp0->addr_align_end = (*pp)->addr_align_end;
			tmp0->page_count += (*pp)->page_count;
			DeleteNode(pp);
			MMUDEBUG("**first merge to front. node size**%u\n",
				 tmp0->page_count);
			/* after merge to front contiguous memory node,
			 * find if there is behind contiguous memory node
			 */
			while (tmp1) {
				/* merge */
				if (tmp1->addr_align_start == tmp0->addr_align_end) {
					tmp1->addr_align_start = tmp0->addr_align_start;
					tmp1->page_count += tmp0->page_count;
					MMUDEBUG("**second merge to behind. node size**%u\n",
						 tmp1->page_count);
					DeleteNode(&tmp0);
					return MMU_STATUS_OK;
				}
				tmp1 = tmp1->next;
			}
			return MMU_STATUS_OK;
			/* 1th step: find behind contiguous memory node */
		} else if (tmp0->addr_align_start == (*pp)->addr_align_end) {
			tmp0->addr_align_start = (*pp)->addr_align_start;
			tmp0->page_count += (*pp)->page_count;
			DeleteNode(pp);
			MMUDEBUG("**first merge to behind. node size**%u\n",
				 tmp0->page_count);
			/* after merge to behind contiguous memory node,
			 * find if there is front contiguous memory node
			 */
			while (tmp1) {
				/* merge */
				if (tmp1->addr_align_end == tmp0->addr_align_start) {
					tmp1->addr_align_end = tmp0->addr_align_end;
					tmp1->page_count += tmp0->page_count;
					MMUDEBUG("**second merge to front. node size**%u\n",
						 tmp1->page_count);
					DeleteNode(&tmp0);
					return MMU_STATUS_OK;
				}
				tmp1 = tmp1->next;
			}
			return MMU_STATUS_OK;
		}
		tmp0 = tmp0->next;
	}
	return MMU_STATUS_FALSE;
}

#ifdef SUPPORT_48PA_MMU
static enum MMUStatus MergePageNode(struct MMUPageNode *h, struct MMUPageNode *p)
{
	struct MMUPageNode *tmp0 = h->next;
	struct MMUPageNode *tmp1 = h->next;

	while (tmp0) {
		/* 1th step: find front contiguous memory node */
		if (tmp0->page_end == p->page_start) {
			tmp0->page_end = p->page_end;
			tmp0->page_count += p->page_count;
			DeletePageNode(p);
			MMUDEBUG("** Page node first merge to front. node size**%u\n",
				 tmp0->page_count);
			/* after merge to front contiguous memory node,
			 * find if there is behind contiguous memory node
			 */
			while (tmp1) {
				/* merge */
				if (tmp1->page_start == tmp0->page_end) {
					tmp1->page_start = tmp0->page_start;
					tmp1->page_count += tmp0->page_count;
					MMUDEBUG("** Page node second merge to behind. node size**%u\n",
						 tmp1->page_count);
					DeletePageNode(tmp0);
					return MMU_STATUS_OK;
				}
				tmp1 = tmp1->next;
			}
			return MMU_STATUS_OK;
			/* 1th step: find behind contiguous memory node */
		} else if (tmp0->page_start == p->page_end) {
			tmp0->page_start = p->page_start;
			tmp0->page_count += p->page_count;
			DeletePageNode(p);
			MMUDEBUG("** Page node first merge to behind. node size**%u\n",
				 tmp0->page_count);
			/* after merge to behind contiguous memory node,
			 * find if there is front contiguous memory node
			 */
			while (tmp1) {
				/* merge */
				if (tmp1->page_end == tmp0->page_start) {
					tmp1->page_end = tmp0->page_end;
					tmp1->page_count += tmp0->page_count;
					MMUDEBUG("** Page node second merge to front. node size**%u\n",
						 tmp1->page_count);
					DeletePageNode(tmp0);
					return MMU_STATUS_OK;
				}
				tmp1 = tmp1->next;
			}
			return MMU_STATUS_OK;
		}
		tmp0 = tmp0->next;
	}
	return MMU_STATUS_FALSE;
}
#endif

/* Insert a node to map list */
static enum MMUStatus SMInsertNode(enum MMURegion e, struct MMUNode **pp,
						struct MMUProcessObject *mmu_po)
{
	struct MMUNode *h;

	h = mmu_po->region[e].simple_map_head;

	h->next->prev = *pp;
	(*pp)->next = h->next;
	(*pp)->prev = h;
	h->next = *pp;
	MMUDEBUG(" *****insert bm node*****%u\n", (*pp)->page_count);

	return MMU_STATUS_OK;
}

#ifdef SUPPORT_48PA_MMU
/* Insert Page node */
static enum MMUStatus InsertPageNode(struct MMUPageNode *p, unsigned int free)
{
	enum MMUStatus status;
	struct MMUPageNode *h;

	if (free) {
		h = g_mmu->page_free_head;
		p->page_level = MMU_NONE;
		status = MergePageNode(h, p);
		MMUDEBUG(" *****insert Page node free***** %u\n", p->page_count);
		if (MMU_IS_ERROR(status)) {
			/* remove from map*/
			if (p->prev && p->next) {
				p->prev->next = p->next;
				p->next->prev = p->prev;
			}
			/* insert to free map */
			h->next->prev = p;
			p->next = h->next;
			p->prev = h;
			h->next = p;
		}
	} else {
		h = g_mmu->page_use_head;
		h->next->prev = p;
		p->next = h->next;
		p->prev = h;
		h->next = p;
		MMUDEBUG(" *****insert Pt node used***** %u\n", p->page_count);
	}
	return MMU_STATUS_OK;
}
#endif

static enum MMUStatus InsertNode(enum MMURegion e, struct MMUNode **pp,
				 unsigned int free, struct MMUProcessObject *mmu_po)
{
	enum MMUStatus status;
	struct MMUNode *h;

	if (free) {
		h = mmu_po->region[e].free_map_head;
		status = MergeNode(h, pp);
		MMUDEBUG(" *****insert free*****%u\n", (*pp)->page_count);
		if (MMU_IS_ERROR(status)) {
			/* remove from map*/
			if ((*pp)->prev && (*pp)->next) {
				(*pp)->prev->next = (*pp)->next;
				(*pp)->next->prev = (*pp)->prev;
			}
			/* insert to free map */
			h->next->prev = *pp;
			(*pp)->next = h->next;
			(*pp)->prev = h;
			h->next = *pp;
		}
	} else {
		h = mmu_po->region[e].map_head;

		h->next->prev = *pp;
		(*pp)->next = h->next;
		(*pp)->prev = h;
		h->next = *pp;
		MMUDEBUG(" *****insert unfree*****%u\n", (*pp)->page_count);
	}

	return MMU_STATUS_OK;
}

/* Create a Node */
static enum MMUStatus SMCreateNode(enum MMURegion e, struct MMUNode **node,
				   unsigned int page_count, struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p, **new;

	p = kmalloc(sizeof(*p), GFP_KERNEL | MMU_NOWARN);
	new = &p;
	(*new)->addr_align_start = -1;
	(*new)->addr_align_end = -1;
	(*new)->process_id = 0;
	(*new)->filp = NULL;
	(*new)->page_count = 0;
	(*new)->prev = NULL;
	(*new)->next = NULL;
	/* Insert a uncomplete Node, it will be initialized later */
	SMInsertNode(e, new, mmu_po);

	/* return a new node for map buffer */
	*node = *new;
	return MMU_STATUS_OK;
}

/* Create initial Nodes */
static enum MMUStatus SMCreateNodes(struct MMUProcessObject *mmu_po)
{
	struct MMUNode *simple_map_head;
	struct MMUNode *simple_map_tail;
	int i;

	/* Init each region map node */
	for (i = 0; i < MMU_REGION_COUNT; i++) {
		simple_map_head = kmalloc(sizeof(struct MMUNode),
					  GFP_KERNEL | MMU_NOWARN);
		simple_map_tail = kmalloc(sizeof(struct MMUNode),
					  GFP_KERNEL | MMU_NOWARN);

		simple_map_head->addr_align_start = -1;
		simple_map_head->addr_align_end = -1;
		simple_map_head->process_id = 0;
		simple_map_head->filp = NULL;
		simple_map_head->page_count = 0;
		simple_map_head->prev = NULL;
		simple_map_head->next = simple_map_tail;
		simple_map_tail->addr_align_start = -1;
		simple_map_tail->addr_align_end = -1;
		simple_map_tail->process_id = 0;
		simple_map_tail->filp = NULL;
		simple_map_tail->page_count = 0;
		simple_map_tail->prev = simple_map_head;
		simple_map_tail->next = NULL;

		mmu_po->region[i].simple_map_head = simple_map_head;
		mmu_po->region[i].simple_map_tail = simple_map_tail;
	}
	return MMU_STATUS_OK;
}

static enum MMUStatus CreateNode(struct MMUProcessObject *mmu_po)
{
	struct MMUNode *free_map_head, *map_head, *p, **pp;
	struct MMUNode *free_map_tail, *map_tail;
	int i;
	unsigned int page_count;
	unsigned int prev_addr = 0;

	/* Init each region map node */
	for (i = 0; i < MMU_REGION_COUNT; i++) {
		free_map_head = kmalloc(sizeof(struct MMUNode),
					GFP_KERNEL | MMU_NOWARN);
		map_head = kmalloc(sizeof(struct MMUNode),
				   GFP_KERNEL | MMU_NOWARN);
		free_map_tail = kmalloc(sizeof(struct MMUNode),
					GFP_KERNEL | MMU_NOWARN);
		map_tail = kmalloc(sizeof(struct MMUNode),
				   GFP_KERNEL | MMU_NOWARN);

		free_map_head->addr_align_start = map_head->addr_align_start = -1;
		free_map_head->addr_align_end = map_head->addr_align_end = -1;
		free_map_head->process_id = map_head->process_id = 0;
		free_map_head->filp       = map_head->filp       = NULL;
		free_map_head->page_count = map_head->page_count = 0;
		free_map_head->prev       = map_head->prev       = NULL;
		free_map_head->next       = free_map_tail;
		map_head->next            = map_tail;

		free_map_tail->addr_align_start = map_tail->addr_align_start = -1;
		free_map_tail->addr_align_end = map_tail->addr_align_end = -1;
		free_map_tail->process_id = map_tail->process_id = 0;
		free_map_tail->filp       = map_tail->filp       = NULL;
		free_map_tail->page_count = map_tail->page_count = 0;
		free_map_tail->prev       = free_map_head;
		map_tail->prev            = map_head;
		free_map_tail->next       = map_tail->next       = NULL;

		mmu_po->region[i].free_map_head = free_map_head;
		mmu_po->region[i].map_head = map_head;
		mmu_po->region[i].free_map_tail = free_map_tail;
		mmu_po->region[i].map_tail = map_tail;

		p = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
		pp = &p;

		switch (i) {
		case MMU_REGION_IN:
			page_count = (REGION_IN_END - REGION_IN_START) / PAGE_SIZE;
			//hold mmu addr: 0x0
			// set bits[ - :12] as addr
			p->addr_align_start = REGION_IN_MMU_START >> MMU_4k_SHIFT & 0xFFFFFFF;
			p->addr_align_end = prev_addr = REGION_IN_MMU_END >> MMU_4k_SHIFT & 0xFFFFFFF;
			p->page_count = page_count - 1; //hold mmu addr: 0x0
			break;
		case MMU_REGION_OUT:
			page_count = (REGION_OUT_END - REGION_OUT_START) / PAGE_SIZE;
			p->addr_align_start = REGION_OUT_MMU_START >> MMU_4k_SHIFT & 0xFFFFFFF;
			p->addr_align_end = prev_addr = REGION_OUT_MMU_END >> MMU_4k_SHIFT & 0xFFFFFFF;
			p->page_count = page_count;
			break;
		case MMU_REGION_PRIVATE:
			page_count = (REGION_PRIVATE_END - REGION_PRIVATE_START) / PAGE_SIZE;
			p->addr_align_start = REGION_PRIVATE_MMU_START >> MMU_4k_SHIFT & 0xFFFFFFF;
			p->addr_align_end = prev_addr = REGION_PRIVATE_MMU_END >> MMU_4k_SHIFT & 0xFFFFFFF;
			p->page_count = page_count;
			break;
		case MMU_REGION_PUB:
			p->addr_align_start = prev_addr;
#ifdef SUPPORT_48PA_MMU
			p->addr_align_end = (((MMU_PD2_ENTRY_NUM - 1) << 27) & 0x8000000) |
				(((MMU_PD1_ENTRY_NUM - 1) << 18) & 0x7FC0000) |
				(((MMU_PD0_ENTRY_NUM - 1) << 9) & 0x3FE00) |
				((MMU_PT_4K_ENTRY_NUM - 1) & 0x1FF);
			//for 4k page size mode
			p->page_count = p->addr_align_end - p->addr_align_start;
#else
			p->addr_align_end = (((MMU_MTLB_ENTRY_NUM - 1) << 10) & 0xFFC00) | ((MMU_STLB_4K_ENTRY_NUM - 1) & 0x3FF);
			//for 4k page size mode
			p->page_count = p->addr_align_end - p->addr_align_start;
#endif
			break;
		default:
			pr_err(" *****MMU Region Error*****\n");
			break;
		}

		p->process_id = 0;
		p->filp = NULL;
		p->next = p->prev = NULL;

		InsertNode(i, pp, 1, mmu_po);
	}

	return MMU_STATUS_OK;
}

#ifdef SUPPORT_48PA_MMU
/*
 * Creat Page node to keep info of page table for haven't enough
 * mem to save all Page entry.
 */
static enum MMUStatus PageCreateNode(void)
{
	struct MMUPageNode *page_free_head, *page_free_tail, *p;
	struct MMUPageNode *page_use_head, *page_use_tail;

	page_free_head = kmalloc(sizeof(*page_free_head),
			GFP_KERNEL | MMU_NOWARN);
	page_free_tail = kmalloc(sizeof(*page_free_tail),
			GFP_KERNEL | MMU_NOWARN);
	page_use_head = kmalloc(sizeof(*page_use_head),
			GFP_KERNEL | MMU_NOWARN);
	page_use_tail = kmalloc(sizeof(*page_use_tail),
			GFP_KERNEL | MMU_NOWARN);
	/* Init node */
	page_free_head->page_start = page_use_head->page_start = -1;
	page_free_head->page_end = page_use_head->page_end = -1;
	page_free_head->page_offset = page_use_head->page_offset = 0;
	page_free_head->page_level = page_use_head->page_level = MMU_NONE;
	page_free_head->page_table_entry = page_use_head->page_table_entry = NULL;
	page_free_head->process_id = page_use_head->process_id = 0;
	page_free_head->filp = page_use_head->filp = NULL;
	page_free_head->page_count = page_use_head->page_count = 0;
	page_free_head->prev = page_use_head->prev = NULL;
	page_free_head->next = page_free_tail;
	page_use_head->next = page_use_tail;

	page_free_tail->page_start = page_use_tail->page_start = -1;
	page_free_tail->page_end = page_use_tail->page_end = -1;
	page_free_tail->page_offset = page_use_tail->page_offset = 0;
	page_free_tail->page_level = page_use_tail->page_level = MMU_NONE;
	page_free_tail->page_table_entry = page_use_tail->page_table_entry = NULL;
	page_free_tail->process_id = page_use_tail->process_id = 0;
	page_free_tail->filp = page_use_tail->filp = NULL;
	page_free_tail->page_count = page_use_tail->page_count = 0;
	page_free_tail->next = page_use_tail->next = NULL;
	page_free_tail->prev = page_free_head;
	page_use_tail->prev = page_use_head;

	g_mmu->page_use_head = page_use_head;
	g_mmu->page_use_tail = page_use_tail;
	g_mmu->page_free_head = page_free_head;
	g_mmu->page_free_tail = page_free_tail;
	p = kmalloc(sizeof(*p), GFP_KERNEL | MMU_NOWARN);
	/* 8 bytes per entry */
	p->page_start = DYNAMIC_PAGE_START_ADDRESS >> 3;
	p->page_end = DYNAMIC_PAGE_END_ADDRESS >> 3;
	p->page_count = (DYNAMIC_PAGE_END_ADDRESS - DYNAMIC_PAGE_START_ADDRESS) >> 3;
	p->use_count = 0;
	p->process_id = 0;
	p->filp = NULL;
	p->next = p->prev = NULL;

	InsertPageNode(p, PAGE_NODE_FREE);
	return MMU_STATUS_OK;
}
#endif

/* A simpile function to check if the map buffer
 *is existed. it needs more complex version
 */
static enum MMUStatus SMCheckAddress(enum MMURegion e, void *virtual_address,
					struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p;

	p = mmu_po->region[e].simple_map_head->next;

	while (p) {
		if (p->buf_virtual_address == virtual_address)
			return MMU_STATUS_FALSE;
		p = p->next;
	}
	return MMU_STATUS_OK;
}

static enum MMUStatus FindFreeNode(enum MMURegion e, struct MMUNode **node,
				   unsigned int page_count, struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p;

	p = mmu_po->region[e].free_map_head->next;

	while (p) {
		if (p->page_count >= page_count) {
			*node = p;
			return MMU_STATUS_OK;
		}
		p = p->next;
	}
	return MMU_STATUS_FALSE;
}

#ifdef SUPPORT_48PA_MMU
//Find Free page node, every time get 512 page count for 4k mode.
static enum MMUStatus FindFreePageNode(struct MMUPageNode **node, enum MMUPageLevel page_level)
{
	struct MMUPageNode *p;
	int page_count;

	p = g_mmu->page_free_head->next;
	switch (page_level) {
	case MMU_PD2: {
		page_count = MMU_PD2_ENTRY_NUM;
		break;
	}
	case MMU_PD1: {
		page_count = MMU_PD1_ENTRY_NUM;
		break;
	}
	case MMU_PD0: {
		page_count = MMU_PD0_ENTRY_NUM;
		break;
	}
	case MMU_PT: {
		page_count = MMU_PT_4K_ENTRY_NUM;
		break;
	}
	default:
		return MMU_STATUS_FALSE;
	}

	while (p) {
		if (p->page_count >= page_count) {
			*node = p;
			return MMU_STATUS_OK;
		}
		p = p->next;
	}
	return MMU_STATUS_FALSE;
}
#endif

static enum MMUStatus SplitFreeNode(enum MMURegion e, struct MMUNode **node,
				    unsigned int page_count, struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p, **new;

	p = kmalloc(sizeof(struct MMUNode), GFP_KERNEL | MMU_NOWARN);
	new = &p;

	**new = **node;
	(*new)->addr_align_start = (*node)->addr_align_start;
	(*new)->addr_align_end = (*node)->addr_align_start + page_count;
	(*new)->process_id = (*node)->process_id;
	(*new)->filp = (*node)->filp;
	(*new)->page_count = page_count;
	MMUDEBUG(" *****new addr_start*****%lld\n", (*new)->addr_align_start);
	MMUDEBUG(" *****new addr_end*****%lld\n", (*new)->addr_align_end);
	/* Insert a new node in map */
	InsertNode(e, new, 0, mmu_po);

	/* Update free node in free map*/
	(*node)->page_count -= page_count;
	if ((*node)->page_count == 0) {
		DeleteNode(node);
		MMUDEBUG(" *****old node deleted*****\n");
	} else {
		(*node)->addr_align_start = (*new)->addr_align_end;
		MMUDEBUG(" *****old addr_start*****%llu\n", (*node)->addr_align_start);
		MMUDEBUG(" *****old addr_end*****%llu\n", (*node)->addr_align_end);
	}
	/* return a new node for map buffer */
	*node = *new;

	return MMU_STATUS_OK;
}

#ifdef SUPPORT_48PA_MMU
// split page mem, split 512*4*2 bytes every time.
static enum MMUStatus SplitFreePageNode(struct MMUPageNode **node, enum MMUPageLevel page_level)
{
	struct MMUPageNode *p, **new;
	int page_count;

	switch (page_level) {
	case MMU_PD2: {
		page_count = MMU_PD2_ENTRY_NUM;
		break;
	}
	case MMU_PD1: {
		page_count = MMU_PD1_ENTRY_NUM;
		break;
	}
	case MMU_PD0: {
		page_count = MMU_PD0_ENTRY_NUM;
		break;
	}
	case MMU_PT: {
		page_count = MMU_PT_4K_ENTRY_NUM;
		break;
	}
	default:
		return MMU_STATUS_FALSE;
	}
	p = kmalloc(sizeof(*p), GFP_KERNEL | MMU_NOWARN);
	new = &p;
	//FindFreePtNode(node);
	**new = **node;
	(*new)->page_start = (*node)->page_start;
	(*new)->page_end = (*node)->page_start + page_count;
	(*new)->page_offset = (*node)->page_offset;
	(*new)->page_level = page_level;
	(*new)->page_table_entry = (*node)->page_table_entry;
	(*new)->process_id = (*node)->process_id;
	(*new)->filp = (*node)->filp;
	(*new)->page_count = page_count;
	(*new)->use_count = 0;
	MMUDEBUG(" *****new page_start*****%lld\n", (*new)->page_start);
	MMUDEBUG(" *****new page_end*****%lld\n", (*new)->page_end);
	/* Insert a new pt node in map */
	InsertPageNode(*new, PAGE_NODE_USE);

	/* Update free node in free map*/
	(*node)->page_count -= page_count;
	if ((*node)->page_count == 0) {
		DeletePageNode(*node);
		MMUDEBUG(" *****old node deleted*****\n");
	} else {
		(*node)->page_start = (*new)->page_end;
		MMUDEBUG(" *****old page_start*****%llu\n", (*node)->page_start);
		MMUDEBUG(" *****old page_end*****%llu\n", (*node)->page_end);
	}
	/* return a new node for map buffer */
	*node = *new;

	return MMU_STATUS_OK;
}
#endif

static enum MMUStatus SMRemoveNode(enum MMURegion e, void *buf_virtual_address,
				   struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p, **pp;
	int process_id;

	p = mmu_po->region[e].simple_map_head->next;
	pp = &p;
#ifdef MMU_PAGE_TABLE_SWITCH
	process_id = mmu_po->process_id;
#else
	process_id = GetProcessID();
#endif

	while (*pp) {
		if ((*pp)->buf_virtual_address == buf_virtual_address &&
		    (*pp)->process_id == process_id) {
			SMDeleteNode(pp);
			break;
		}
		*pp = (*pp)->next;
	}

	return MMU_STATUS_OK;
}

static enum MMUStatus RemoveNode(enum MMURegion e, void *buf_virtual_address,
				 struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p, **pp;
	int process_id;

	p = mmu_po->region[e].map_head->next;
	pp = &p;
#ifdef MMU_PAGE_TABLE_SWITCH
	process_id = mmu_po->process_id;
#else
	process_id = GetProcessID();
#endif

	while (*pp) {
		if ((*pp)->buf_virtual_address == buf_virtual_address &&
		    (*pp)->process_id == process_id) {
			InsertNode(e, pp, 1, mmu_po);
			break;
		}
		*pp = (*pp)->next;
	}

	return MMU_STATUS_OK;
}

#ifdef SUPPORT_48PA_MMU
static enum MMUStatus RemovePageNode(struct MMUPageNode *pagenode,
				     unsigned int process_id)
{
	if (pagenode->process_id == process_id) {
		InsertPageNode(pagenode, PAGE_NODE_FREE);
		return MMU_STATUS_OK;
	}

	return MMU_STATUS_FALSE;
}
#endif

static enum MMUStatus SMRemoveKernelNode(enum MMURegion e,
					 unsigned int buf_bus_address,
					 struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p, **pp;
	int process_id;

	p = mmu_po->region[e].simple_map_head->next;
	pp = &p;
#ifdef MMU_PAGE_TABLE_SWITCH
	process_id = mmu_po->process_id;
#else
	process_id = GetProcessID();
#endif

	while (*pp) {
		if ((*pp)->buf_bus_address == buf_bus_address &&
		    ((*pp)->process_id == g_mmu->init_process_id || (*pp)->process_id == process_id)) {
			SMDeleteNode(pp);
			break;
		}
		*pp = (*pp)->next;
	}

	return MMU_STATUS_OK;
}

static enum MMUStatus RemoveKernelNode(enum MMURegion e,
					unsigned int buf_bus_address,
					struct MMUProcessObject *mmu_po)
{
	struct MMUNode *p, **pp;
	int process_id;

	p = mmu_po->region[e].map_head->next;
	pp = &p;
#ifdef MMU_PAGE_TABLE_SWITCH
	process_id = mmu_po->process_id;
#else
	process_id = GetProcessID();
#endif

	while (*pp) {
		if ((*pp)->buf_bus_address == buf_bus_address &&
		    ((*pp)->process_id == g_mmu->init_process_id || (*pp)->process_id == process_id)) {
			InsertNode(e, pp, 1, mmu_po);
			break;
		}
		*pp = (*pp)->next;
	}

	return MMU_STATUS_OK;
}

static enum MMUStatus Delay(unsigned int delay)
{
	if (delay > 0) {
#if (KERNEL_VERSION(2, 6, 28) <= LINUX_VERSION_CODE)
		ktime_t dl = ktime_set((delay / MSEC_PER_SEC),
			(delay % MSEC_PER_SEC) * NSEC_PER_MSEC);
		__set_current_state(TASK_UNINTERRUPTIBLE);
		schedule_hrtimeout(&dl, HRTIMER_MODE_REL);
#else
		msleep(delay);
#endif
	}

	return MMU_STATUS_OK;
}

static enum MMUStatus CreateMutex(void **mtx)
{
	enum MMUStatus status;

	/* Allocate the mutex structure. */
	status = AllocateMemory(sizeof(struct mutex), mtx);
	if (MMU_IS_SUCCESS(status)) {
		/* Initialize the mutex. */
		mutex_init(*(struct mutex **)mtx);
	}

	return status;
}

static enum MMUStatus DeleteMutex(void *mtx)
{
	/* Destroy the mutex. */
	mutex_destroy((struct mutex *)mtx);

	/* Free the mutex structure. */
	FreeMemory(mtx);

	return MMU_STATUS_OK;
}

static enum MMUStatus AcquireMutex(void *mtx, unsigned int timeout)
{
	if (timeout == MMU_INFINITE) {
		/* Lock the mutex. */
		mutex_lock(mtx);

		/* Success. */
		return MMU_STATUS_OK;
	}

	for (;;) {
		/* Try to acquire the mutex. */
		if (mutex_trylock(mtx)) {
			/* Success. */
			return MMU_STATUS_OK;
		}

		if (timeout-- == 0)
			break;

		/* Wait for 1 millisecond. */
		Delay(1);
	}

	return MMU_STATUS_OK;
}

static enum MMUStatus ReleaseMutex(void *mtx)
{
	/* Release the mutex. */
	mutex_unlock(mtx);

	return MMU_STATUS_OK;
}

static inline enum MMUStatus QueryProcessPageTable(void *logical,
						   addr64_t *address)
{
	unsigned long lg = (unsigned long)logical;
	unsigned long offset = lg & ~PAGE_MASK;
	struct vm_area_struct *vma;
	spinlock_t *ptl;
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (is_vmalloc_addr(logical)) {
		/* vmalloc area. */
		*address = page_to_phys(vmalloc_to_page(logical)) | offset;
		return MMU_STATUS_OK;
	} else if (virt_addr_valid(lg)) {
		/* Kernel logical address. */
		*address = virt_to_phys(logical);
		return MMU_STATUS_OK;
	}
	/* Try user VM area. */
	if (!current->mm)
		return MMU_STATUS_NOT_FOUND;

#if (KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE)
	down_read(&current->mm->mmap_lock);
#else
	down_read(&current->mm->mmap_sem);
#endif
	vma = find_vma(current->mm, lg);
#if (KERNEL_VERSION(5, 8, 0) <= LINUX_VERSION_CODE)
	up_read(&current->mm->mmap_lock);
#else
	up_read(&current->mm->mmap_sem);
#endif

	/* To check if mapped to user. */
	if (!vma)
		return MMU_STATUS_NOT_FOUND;

	pgd = pgd_offset(current->mm, lg);
	if (pgd_none(*pgd) || pgd_bad(*pgd))
		return MMU_STATUS_NOT_FOUND;

#if (defined(__arm__) || defined(__aarch64__))
	#if KERNEL_VERSION(5, 7, 0) <= LINUX_VERSION_CODE
		pud = pud_offset((p4d_t *)pgd, lg);
	#else
		pud = pud_offset(pgd, lg);
	#endif
#elif (defined(CONFIG_CPU_CSKYV2) || defined(CONFIG_X86)) &&                     \
	(KERNEL_VERSION(4, 12, 0) <= LINUX_VERSION_CODE)
	pud = pud_offset((p4d_t *)pgd, lg);
#elif (defined(CONFIG_CPU_CSKYV2)) &&                                          \
	(KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE)
	pud = pud_offset((p4d_t *)pgd, lg);
#else
	pud = pud_offset(pgd, lg);
#endif
	if (pud_none(*pud) || pud_bad(*pud))
		return MMU_STATUS_NOT_FOUND;

	pmd = pmd_offset(pud, lg);
	if (pmd_none(*pmd) || pmd_bad(*pmd))
		return MMU_STATUS_NOT_FOUND;

#if KERNEL_VERSION(6, 5, 0) > LINUX_VERSION_CODE
	pte = pte_offset_map_lock(current->mm, pmd, lg, &ptl);
#else
	ptl = pte_lockptr(current->mm, pmd);
	pte = pte_offset_kernel(pmd, lg);
	spin_lock(ptl);
#endif
	if (!pte) {
		spin_unlock(ptl);
		return MMU_STATUS_NOT_FOUND;
	}

	if (!pte_present(*pte)) {
		PTE_UNLOCK(pte, ptl);
		return MMU_STATUS_NOT_FOUND;
	}

	*address = (pte_pfn(*pte) << PAGE_SHIFT) | offset;
	PTE_UNLOCK(pte, ptl);

	if (dma_used) {
		*address -= host_base;
	} else {
#ifdef PCIE_EN
		*address -= gBaseDDRHw;
#else
		*address -= 0;
#endif
	}

	//MMUDEBUG(" QueryProcessPageTable map: virt %p -> %p\n", logical, (void *)*address);

	return MMU_STATUS_OK;
}

#if (KERNEL_VERSION(2, 6, 25) > LINUX_VERSION_CODE)
static inline int is_vmalloc_addr(void *addr)
{
	addr64_t addr = (addr64_t)Addr;

	return addr >= VMALLOC_START && addr < VMALLOC_END;
}
#endif

static enum MMUStatus GetPhysicalAddress(void *logical,
					 addr64_t *address)
{
	enum MMUStatus status;

	status = QueryProcessPageTable(logical, address);

	return status;
}

#ifdef SUPPORT_48PA_MMU
/* use virtual address get the page node */
static enum MMUStatus GetPageNode(addr64_t page_offset,
						enum MMUPageLevel page_level,
						struct MMUPageNode **node,
						struct MMUProcessObject *mmu_po)
{
	struct MMUPageNode *p;

	p = g_mmu->page_use_head->next;
	while (p) {
		if (page_offset == p->page_offset && page_level == p->page_level
#ifdef MMU_PAGE_TABLE_SWITCH
			&& (p->process_id == mmu_po->process_id || p->process_id == g_mmu->init_process_id)
#endif
			) {
			*node = p;
			return MMU_STATUS_OK;
		}
		p = p->next;
	}
	return MMU_STATUS_FALSE;
}
#endif

static enum MMUStatus GetPageEntry(struct MMUNode *node,
				   unsigned int **page_table_entry,
				   struct MMUProcessObject *mmu_po,
				   unsigned int i, enum MMUPageLevel page_level)
{
#ifdef SUPPORT_48PA_MMU
	addr64_t page_offset = 0;
	addr64_t addr = 0;
	unsigned int pd_entry_value[2];
	unsigned int *tmp_entry;
	struct MMUPageNode *page_node;

	//get pd2 entry at first
	if (page_level > MMU_PD2) {
		if (GetPageEntry(node, &tmp_entry, mmu_po, i, page_level - 1) == MMU_STATUS_FALSE)
			return MMU_STATUS_FALSE;
	} else if (page_level == MMU_PD2) {
		addr = node->addr_align_start + i;
		page_offset = (addr >> mmu_page_shift[page_level]) & mmu_page_bits[page_level];
		*page_table_entry = (unsigned int *)mmu_po->pd2_virtual + page_offset * 2;
		return MMU_STATUS_OK;
	} else {
		return MMU_STATUS_FALSE;
	}
		/* next level page addr bits[47:32] */
	addr = (((addr64_t)(*(tmp_entry + 1)) & 0xFFFF) << 32)
		/* next level page addr bits[31:12] */
		| (*tmp_entry & 0xFFFFF000);
	if (addr) {
		/* find start addr for current level page */
		switch (page_level) {
		case MMU_PD1: {
			tmp_entry = (unsigned int *)(mmu_po->pd1_virtual+
				(addr - mmu_po->pd1_physical));
			break;
		}
		case MMU_PD0:
		case MMU_PT:
		{
			tmp_entry = (unsigned int *)(g_mmu->dy_pg_virtual +
				(addr - g_mmu->dy_pg_physical));
			break;
		}
		default:
			return MMU_STATUS_FALSE;
		}
	} else {
		FindFreePageNode(&page_node, page_level);
		/* page_node process_id and filp same as node */
		page_node->process_id = node->process_id;
		page_node->filp = node->filp;
		/* set up level offset as flag */
		addr = node->addr_align_start + i;
		page_offset = (addr >> mmu_page_shift[page_level - 1]) & mmu_page_bits[page_level - 1];
		page_node->page_offset = page_offset;
		page_node->page_table_entry = (void *)tmp_entry;
		/* Split 512*4*2 bytes for new page entry */
		SplitFreePageNode(&page_node, page_level);
		/* not real addr(addr >> 3) */
		addr = page_node->page_start;
		if (page_level == MMU_PT) {
			/* addr << 3) >> 32, PT bus address bits [47:32] */
			pd_entry_value[1] = (addr >> 29) & 0xFFFF;
			pd_entry_value[0] =
				/* PT bus address bits [32:8] */
				((addr << 3) & 0xFFFFF000)
				/* writable */
				| (1 << 4)
				/* PS:0 (4k page size) */
				| (0 << 2)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
		} else {
			/* addr << 3) >> 32, PT bus address bits [47:32] */
			pd_entry_value[1] = (addr >> 29) & 0xFFFF;
			pd_entry_value[0] =
				/* PT bus address bits [32:8] */
				((addr << 3) & 0xFFFFF000)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
		}
		/* Write Page entry for up level */
		WritePDEntry(tmp_entry, pd_entry_value);
		tmp_entry = (unsigned int *)(g_mmu->dy_pg_virtual + ((addr << 3) - g_mmu->dy_pg_physical));
		/* new PT page table allocate and add in PD0 */
		if (page_level == MMU_PT) {
			addr = node->addr_align_start + i;
			page_offset = (addr >> mmu_page_shift[MMU_PD0 - 1]) &
					mmu_page_bits[MMU_PD0 - 1];
			GetPageNode(page_offset, MMU_PD0, &page_node, mmu_po);
			page_node->use_count = page_node->use_count >= page_node->page_count ?
						page_node->page_count : page_node->use_count + 1;
		}
	}
	addr = node->addr_align_start + i;
	page_offset = (addr >> mmu_page_shift[page_level]) & mmu_page_bits[page_level];
	*page_table_entry = tmp_entry + page_offset * 2;
#else
	addr64_t mtlb_start = 0, stlb_start = 0;
	int num = 0;

	mtlb_start = (node->addr_align_start >> 10) & 0x3FF;
	stlb_start = node->addr_align_start & 0x3FF;
	num = mtlb_start * MMU_STLB_4K_ENTRY_NUM +
		  stlb_start + i;
	*page_table_entry = (unsigned int *)g_mmu->stlb_virtual + num;
#endif

	return MMU_STATUS_OK;
}

#ifdef PCIE_EN
/*
 * Function name: PcieReserveMemory
 * Description:
 *  in pcie env, reserve memory for PD2/PD1/PD0/PT/PTD/MTLB
 * Parameter:
 *  pa: the start physical address of reserve memory
 *  sz: the size of reserve memory
 *  n: the object name of reserve memory
 */
void *PcieReserveMemory(addr64_t pa, unsigned int sz, char *n)
{
	void *va = NULL;

	va = (void __force *)ioremap(gBaseDDRHw + pa, sz);
	MMUDEBUG("gBaseDDRHw=0x%lx, %s virtual=%p\n",
			 gBaseDDRHw, n, va);
	MMUDEBUG("***** %s physical = 0x%llx\n",
			 n, pa);
	if (va)
		ZeroMemory(va, sz);

	return va;
}

/*
 * Function name: PcieReleaseMemory
 * Description:
 *  release memory for PD2/PD1/PD0/PT/PTD/MTLB
 * Parameter:
 *  va: the virtual address of release memory
 *  n: the object name of release memory
 */
void PcieReleaseMemory(void **va, char *n)
{
	iounmap((void __iomem *)(*va));
	*va = NULL;
	MMUDEBUG("*****release %s memory!\n", n);
}

#else
/*
 * Function name: DmaAllocMemory
 * Description:
 *  dma alloc memory for PD2/PD1/PD0/PT/PTD/MTLB
 * Parameter:
 *  platformdev: platform device
 *  sz: the size of alloc memory
 *  n: the object name of alloc memory
 *  pa: the output physical address of alloc memory
 */
void *DmaAllocMemory(struct platform_device *platformdev,
					unsigned int sz, char *n, addr64_t *pa)
{
	void *va = NULL;
	dma_addr_t dma_handle;

	va = dma_alloc_coherent(&platformdev->dev, sz,
					   &dma_handle, GFP_KERNEL | GFP_DMA);
	MMUDEBUG("***** %s virtual = 0x%p\n", n, va);
	*pa = (addr64_t)dma_handle;
	MMUDEBUG("*****%s physical = 0x%llx\n", n, *pa);

	if (va)
		ZeroMemory(va, sz);

	return va;
}

/*
 * Function name: DmaFreeMemory
 * Description:
 *  free memory for PD2/PD1/PD0/PT/PTD/MTLB
 * Parameter:
 *  platformdev: platform device
 *  sz: the size of free memory
 *  n: the object name of free memory
 *  va: the virtual address of free memory
 *  pa: the physical address of free memory
 */
void DmaFreeMemory(struct platform_device *platformdev, unsigned int sz,
				 void **va, addr64_t pa, char *n)
{
	dma_free_coherent(&platformdev->dev, sz, *va, (dma_addr_t)pa);
	*va = NULL;
	MMUDEBUG("*****free %s memory!\n", n);
}
#endif

/*
 * Function name: MMUCleanupMemory
 * Description:
 *  cleanup all memory
 * Parameter:
 *  platformdev: platform device
 *  sz: the size of free memory
 *  n: the object name of free memory
 *  va: the virtual address of free memory
 *  pa: the physical address of free memory
 */
void MMUCleanupMemory(void)
{
#ifdef PCIE_EN
#ifdef SUPPORT_48PA_MMU
	if (g_mmu->pd2_virtual)
		PcieReleaseMemory(g_mmu->pd2_virtual, "PD2");
	if (g_mmu->pd1_virtual)
		PcieReleaseMemory(g_mmu->pd1_virtual, "PD1");
	if (g_mmu->dy_pg_virtual)
		PcieReleaseMemory(g_mmu->dy_pg_virtual, "PD0_PT(dynamic)");
#else
	if (g_mmu->stlb_virtual)
		PcieReleaseMemory(g_mmu->stlb_virtual, "STLB");
	if (g_mmu->mtlb_virtual)
		PcieReleaseMemory(g_mmu->mtlb_virtual, "MTLB");
#endif // SUPPORT_48PA_MMU
	if (g_mmu->page_table_array)
		PcieReleaseMemory(g_mmu->page_table_array, "PTD");
#else // PCIE_EN

#ifdef SUPPORT_48PA_MMU
	if (g_mmu->pd2_virtual)
		DmaFreeMemory(platformdev, g_mmu->pd2_size,
					  &g_mmu->pd2_virtual,
					  (dma_addr_t)g_mmu->pd2_physical,
					  "PD2");
	if (g_mmu->pd1_virtual)
		DmaFreeMemory(platformdev, g_mmu->pd1_size,
					  &g_mmu->pd1_virtual,
					  (dma_addr_t)g_mmu->pd1_physical,
					  "PD1");
	if (g_mmu->dy_pg_virtual)
		DmaFreeMemory(platformdev, g_mmu->dy_pg_size,
					  &g_mmu->dy_pg_virtual,
					  (dma_addr_t)g_mmu->dy_pg_physical,
					  "PD0_PT(dynamic)");
#else
	if (g_mmu->stlb_virtual)
		DmaFreeMemory(platformdev, g_mmu->stlb_size,
					  &g_mmu->stlb_virtual,
					  (dma_addr_t)g_mmu->stlb_physical,
					  "STLB");
	if (g_mmu->mtlb_virtual)
		DmaFreeMemory(platformdev, g_mmu->mtlb_size,
					  &g_mmu->mtlb_virtual,
					  (dma_addr_t)g_mmu->mtlb_physical,
					  "MTLB");
#endif // SUPPORT_48PA_MMU
	if (g_mmu->page_table_array)
		DmaFreeMemory(platformdev,
					  g_mmu->page_table_array_size,
					  &g_mmu->page_table_array,
					  (dma_addr_t)g_mmu->page_table_array_physical,
					  "PTD");
#endif
}

#ifdef MMU_PAGE_TABLE_SWITCH
/*
 * Function name: _HasCreatedPageTable
 * Description:
 *  check the page table of current process if created
 * Return value:
 *  1: has created
 *  0: has not
 */
static int _HasCreatedPageTable(int process_id)
{
	struct MMUProcessObject *mmu_po = g_mmu->mmu_po;

	while (mmu_po) {
		if (mmu_po->process_id == process_id)
			return 1;
		mmu_po = mmu_po->next;
	}

	return 0;
}
#endif

/*
 * Function name: _InMemoryRegion
 * Description:
 *  check the address if in reserved memory region
 * Return value:
 *  1: exceeded the memory region
 *  0: in memory region
 */
static int _InMemoryRegion(addr64_t addr, addr64_t region_end)
{
	return addr <= region_end ? 0 : 1;
}

/*
 * Function name: _FindFreeMemory
 * Description:
 *  find free memory from reserved memory region
 * Return value:
 *  -1: failed finding
 *  others: the offset in memory region
 */
static int _FindFreeMemory(void *region_start, void *region_end,
				unsigned int unit_size)
{
	int offset = 0;
	unsigned int *start = (unsigned int *)region_start;
	unsigned int *end = (unsigned int *)region_end;

	while (start < end) {
		if (*start == 0)
			break;
		start += unit_size / 4;
		offset++;
	}

	if (start > end)
		return -1;

	return offset;
}

/*
 * Function name: MMUSplitPTDPD2PD1
 * Description:
 *  split the ptd, pd2 and pd1 pool for new page table
 * Parameter:
 *  mmu_po: the mmu process object
 */
enum MMUStatus MMUSplitPTDPD2PD1(struct MMUProcessObject *mmu_po)
{
	struct MMUProcessObject *mmu_po_head = g_mmu->mmu_po;
	int ret = 0, offset;
	void *region_start = NULL, *region_end = NULL;

	mmu_po->ptd_physical = mmu_po_head->ptd_physical + g_mmu->page_table_id * g_mmu->page_table_array_unit_size;
	mmu_po->ptd_virtual = (unsigned int *)mmu_po_head->ptd_virtual + g_mmu->page_table_id * g_mmu->page_table_array_unit_size / 4;
	mmu_po->pd2_physical = mmu_po_head->pd2_physical + g_mmu->page_table_id * g_mmu->pd2_unit_size;
	mmu_po->pd2_virtual = (unsigned int *)mmu_po_head->pd2_virtual + g_mmu->page_table_id * g_mmu->pd2_unit_size / 4;
	mmu_po->pd1_physical = mmu_po_head->pd1_physical + g_mmu->page_table_id * g_mmu->pd1_unit_size;
	mmu_po->pd1_virtual = (unsigned int *)mmu_po_head->pd1_virtual + g_mmu->page_table_id * g_mmu->pd1_unit_size / 4;

	if ((*(unsigned int *)mmu_po->ptd_virtual) != 0) {
		ret = 1;
	} else {
		ret = _InMemoryRegion(mmu_po->ptd_physical + g_mmu->page_table_array_unit_size,
			g_mmu->page_table_array_physical + g_mmu->page_table_array_size);
		ret |= _InMemoryRegion(mmu_po->pd2_physical + g_mmu->pd2_unit_size,
			g_mmu->pd2_physical + g_mmu->pd2_size);
		ret |= _InMemoryRegion(mmu_po->pd1_physical + g_mmu->pd1_unit_size,
			g_mmu->pd1_physical + g_mmu->pd1_size);
	}
	if (ret) {
		region_start = g_mmu->page_table_array;
		region_end = g_mmu->page_table_array + (g_mmu->page_table_id - 1) * g_mmu->page_table_array_unit_size;
		offset = _FindFreeMemory(region_start, region_end, g_mmu->page_table_array_unit_size);
		if (offset < 0)
			return MMU_STATUS_FALSE;
		mmu_po->ptd_physical =  g_mmu->page_table_array_physical + offset * g_mmu->page_table_array_unit_size;
		mmu_po->ptd_virtual = (unsigned int *)g_mmu->page_table_array + offset * g_mmu->page_table_array_unit_size / 4;
		mmu_po->pd2_physical = g_mmu->pd2_physical + offset * g_mmu->pd2_unit_size;
		mmu_po->pd2_virtual = (unsigned int *)g_mmu->pd2_virtual + offset * g_mmu->pd2_unit_size / 4;
		mmu_po->pd1_physical = g_mmu->pd1_physical + offset * g_mmu->pd1_unit_size;
		mmu_po->pd1_virtual = (unsigned int *)g_mmu->pd1_virtual + offset * g_mmu->pd1_unit_size / 4;
		g_mmu->page_table_id = offset;
		mmu_po->desc_id = offset;
	}

	return MMU_STATUS_OK;
}

#ifdef SUPPORT_48PA_MMU
/*
 * Function name: MMUWritePTD
 * Description:
 *  write pd2 physical address to page table descriptor
 * Parameter:
 *  mmu_po: the mmu process object
 */
enum MMUStatus MMUWritePTD(struct MMUProcessObject *mmu_po)
{
	MMUDEBUG("%s for page_table[%d]!\n", __func__, g_mmu->page_table_id);

	*((unsigned int *)mmu_po->ptd_virtual) =
		/* pd2 base address 48bits: bit[31:-] */
		(mmu_po->pd2_physical & (0xFFFFFFFF << PTD_FILLED_START_BIT))
		/* sharable */
		| (0 << 1)
		/* securable */
		| (0 << 0);
	*((unsigned int *)mmu_po->ptd_virtual + 1) =
		/* pd2 base address 48bits: bit[47:32] */
		(u32)(mmu_po->pd2_physical >> 32) & 0xFFFF;

#if 0
	/*TODO: need check if need config page_table_arary[2] */
	*((unsigned int *)mmu_po->ptd_virtual + 2) =
		/* pd2 base address 48bits: bit[31:6] */
		((mmu_po->pd2_physical + 8) & 0xFFFFFFC0)
		/* sharable */
		| (0 << 1)
		/* securable */
		| (0 << 0);
	*((unsigned int *)mmu_po->ptd_virtual + 3) =
		/* pd2 base address 48bits: bit[47:32] */
		(u32)((mmu_po->pd2_physical + 8) >> 32) & 0xFFFF;
#endif

	MMUDEBUG(" Page table Descriptor array[0]: lsb = 0x%08x\n",
		 ((int *)mmu_po->ptd_virtual)[0]);
	MMUDEBUG("                      msb = 0x%08x\n",
		 ((int *)mmu_po->ptd_virtual)[1]);

	return MMU_STATUS_OK;
}

/*
 * Function name: MMUWritePD2Entry
 * Description:
 *  write the pd2 entries(with pd1 physical)
 * Parameter:
 *  mmu_po: the mmu process object
 */
enum MMUStatus MMUWritePD2Entry(struct MMUProcessObject *mmu_po)
{
	addr64_t pd1_address;
	unsigned int pd2_num_entries, pd1_num_entries;
	unsigned int pd_entry[2];
	unsigned int *pd2_virtual = NULL;
	unsigned int *pd1_virtual = NULL;
	int i;

	pd2_num_entries = MMU_PD2_ENTRY_NUM;
	pd1_num_entries = MMU_PD1_ENTRY_NUM;

	pd2_virtual = (unsigned int *)mmu_po->pd2_virtual;
	pd1_virtual = (unsigned int *)mmu_po->pd1_virtual;
	pd1_address = mmu_po->pd1_physical;
	for (i = 0; i < pd2_num_entries; i++) {
		pd_entry[1] = (pd1_address >> 32) & 0xFFFF;
		pd_entry[0] = (pd1_address & 0xFFFFF000)
					/* Ignore exception */
					| (0 << 1)
					/* Present */
					| (1 << 0);
		WritePDEntry(pd2_virtual, pd_entry);
		MMUDEBUG(" *****pd2[%d] pd1 entry base:0x%llx**\n", i, pd1_address);
		pd2_virtual += 2;
		pd1_address += pd1_num_entries * 8;
	}

	return MMU_STATUS_OK;
}
#endif

/*
 * Function name: MMUGetProcessObject
 * Description:
 *  get mmu process object for current process
 */
struct MMUProcessObject *MMUGetProcessObject(int process_id)
{
	struct MMUProcessObject *mmu_po = g_mmu->mmu_po;

#if defined(SUPPORT_48PA_MMU) && defined(MMU_PAGE_TABLE_SWITCH)
	while (mmu_po) {
		if (g_mmu->init_process_id == process_id ||  mmu_po->process_id == process_id)
			return mmu_po;
		mmu_po = mmu_po->next;
	}
#endif

	return mmu_po;
}

/*
 * Function name: MMUGetPTD
 * Description:
 *  get page directory descriptor address
 * Parameter:
 *  addr: the lower 32bits of the MMU page directory descripto
 *  addr_msb: the upper 32bits of the MMU page directory descriptor
 */
static enum MMUStatus MMUGetPTDAddr(struct MMUProcessObject *mmu_po,
							unsigned int *addr, unsigned int *addr_msb)
{
	addr64_t addr_tmp;

	addr_tmp = mmu_po->ptd_physical;
	*addr = addr_tmp & 0xFFFFFFFF;
#ifdef SUPPORT_48PA_MMU
	*addr_msb = (unsigned int)(addr_tmp >> 32) & 0xFFFF;
#else //SUPPORT_48PA_MMU
#ifdef PCIE_EN
	*addr_msb = 0;
#else
	*addr_msb = (unsigned int)(addr_tmp >> 32) & 0xFF;
#endif //PCIE_EN
#endif //SUPPORT_48PA_MMU

	return MMU_STATUS_OK;
}

/*
 * Function name: MMUGetPageTableArrayAddr
 * Description:
 *  get page table array address, which is configured into MMU_REG_ADDRESS
 * Parameter:
 *  addr: the lower 32bits of page table array
 *  addr_msb: the upper 32bits of page table array
 */
static enum MMUStatus MMUGetPageTableArrayAddr(struct MMUProcessObject *mmu_po,
						unsigned int *addr, unsigned int *addr_msb)
{
	unsigned int tmp;

	MMUGetPTDAddr(mmu_po, addr, addr_msb);
	pr_info("%s: PTD lsb: 0x%8x, msb: 0x%8x\n", __func__,  *addr, *addr_msb);
	tmp = *addr_msb;

	/* shifted 4bits to left for PTD bit[40-47], and it is 0 for 48bit MMU */
	*addr_msb = ((tmp << 4) & 0xFF000) | 0 | (tmp & 0xFF);

	return MMU_STATUS_OK;
}

/*
 * Function name: MMUFlushPageTable
 * Description:
 *  flush MMU page table
 * Parameter:
 *  hwregs: MMU hardware register virtual address
 *  flush_mode: 0 - flush by VMID, 1 - flush all
 */
enum MMUStatus MMUFlushPageTable(u32 core_id,
		volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2],
		char flush_mode)
{
	int i;

	for (i = 0; i <= MMU_VMID_MAX; i++) {
		if (!g_mmu->vmid_waiting_flush[i])
			continue;

		iowrite32(i | flush_mode << 31, (void __iomem *)(hwregs[core_id][0] + MMU_REG_PTFLUSH));
		if (hwregs[core_id][1])
			iowrite32(i | flush_mode << 31, (void __iomem *)(hwregs[core_id][1] + MMU_REG_PTFLUSH));
		g_mmu->vmid_waiting_flush[i] = 0;
	}

	return MMU_STATUS_OK;
}

#ifdef MMU_PAGE_TABLE_SWITCH
/*
 * Function name: MMUSwitchPageTable
 * Description:
 *  switch MMU page table
 * Parameter:
 *  hwregs: MMU hardware register virtual address
 */
enum MMUStatus MMUSwitchPageTable(struct file *filp, u32 core_id,
		volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2])
{
	struct MMUProcessObject *mmu_po;
	unsigned int id;
	int process_id = GetProcessID();
	u32 val = 0;

	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);

	if (g_mmu->mmu_po_curr && g_mmu->mmu_po_curr->process_id == process_id) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_OK;
	}

	MMUFlushPageTable(core_id, hwregs, 0);

	mmu_po = MMUGetProcessObject(process_id);
	id = mmu_po->desc_id & 0xFFFF;
	g_mmu->mmu_po_curr = mmu_po;

	pr_info("----------------switch to page table[%d]-------------\n", id);

	val = (u32)ioread32((void __iomem *)(hwregs[core_id][0] + MMU_REG_PAGE_TABLE_ID)) >> 16;
	val = (val << 16) | id;
	iowrite32(val, (void __iomem *)(hwregs[core_id][0] + MMU_REG_PAGE_TABLE_ID));
	if (hwregs[core_id][1]) {
		val = (u32)ioread32((void __iomem *)(hwregs[core_id][1] + MMU_REG_PAGE_TABLE_ID)) >> 16;
		val = (val << 16) | id;
		iowrite32(val, (void __iomem *)(hwregs[core_id][1] + MMU_REG_PAGE_TABLE_ID));
	}

	ReleaseMutex(g_mmu->page_table_mutex);

	return MMU_STATUS_OK;
}
#endif

/*
 * Function name: MMUFlushPageTableByCmdBuf
 * Description:
 *  flush MMU page table in vcmd mode
 * Parameter:
 *  pt_flush: the struct of page table flush, which record the flush vmid and flush count
 *  flush_mode: 0 - flush by VMID, 1 - flush all
 */
enum MMUStatus MMUFlushPageTableByCmdBuf(struct page_table_flush *pt_flush,
			char flush_mode)
{
	int i, cnt = 0;

	for (i = 0; i <= MMU_VMID_MAX; i++) {
		if (!g_mmu->vmid_waiting_flush[i])
			continue;

		pt_flush->flush_vmid[cnt++] = i | flush_mode << 31;
		g_mmu->vmid_waiting_flush[i] = 0;
	}

	pt_flush->flush_cnt = cnt;

	return MMU_STATUS_OK;
}

#ifdef MMU_PAGE_TABLE_SWITCH
/*
 * Function name: MMUSwitchPageTableByCmdBuf
 * Description:
 *  switch MMU page table in vcmd mode
 * Parameter:
 *  params: the page table id and pt flush data for switch page table
 */
enum MMUStatus MMUSwitchPageTableByCmdBuf(struct file *filp, struct page_table_switch *params)
{
	struct MMUProcessObject *mmu_po;
	unsigned int id;
	int process_id = GetProcessID();

	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);

#if 0
	/* for multi-process, cmdbufs linked order not sure */
	if (g_mmu->mmu_po_curr && g_mmu->mmu_po_curr->process_id == process_id) {
		params->id = -1;
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_OK;
	}
#endif

	mmu_po = MMUGetProcessObject(process_id);
	if (!mmu_po) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_FALSE;
	}

	MMUFlushPageTableByCmdBuf(&params->pt_flush, 0);

	id = mmu_po->desc_id & 0xFFFF;
	params->id = id;
	//g_mmu->mmu_po_curr = mmu_po;

	pr_info("switch to page table [%d]\n", id);

	ReleaseMutex(g_mmu->page_table_mutex);

	return MMU_STATUS_OK;
}
#endif

/*
 * Function name: MMUAddProcessObject
 * Description:
 *  add mmu process object into po list
 * Parameter:
 *  mmu_po: need be added po
 */
enum MMUStatus MMUAddProcessObject(struct MMUProcessObject *mmu_po)
{
	struct MMUProcessObject *mmu_po_head = g_mmu->mmu_po;

	if (!mmu_po_head) {
		g_mmu->mmu_po = mmu_po;
		g_mmu->mmu_po_curr = mmu_po;
		g_mmu->mmu_po_tail = mmu_po;
	} else {
		g_mmu->mmu_po_tail->next = mmu_po;
		g_mmu->mmu_po_tail = mmu_po;
	}

	return MMU_STATUS_OK;
}

/*
 * Function name: MMUDeleteProcessObject
 * Description:
 *  delete mmu process object from po list
 * Parameter:
 *  mmu_po: need be deleted po
 */
enum MMUStatus MMUDeleteProcessObject(struct MMUProcessObject *prev,
			struct MMUProcessObject *mmu_po)
{
	struct MMUProcessObject *mmu_po_tail = g_mmu->mmu_po_tail;

	if (mmu_po_tail && mmu_po_tail == mmu_po) {
		prev->next = NULL;
		g_mmu->mmu_po_tail = prev;
	} else {
		prev->next = mmu_po->next;
	}

	if (mmu_po == g_mmu->mmu_po_curr)
		g_mmu->mmu_po_curr = NULL;

	return MMU_STATUS_OK;
}

/*
 * Function name: MMUCreateProcessObject
 * Description:
 *  create mmu process object for new page table
 */
struct MMUProcessObject *MMUCreateProcessObject(void)
{
	int i;
	enum MMUStatus status;
	struct MMUProcessObject *mmu_po;
	int page_table_id = g_mmu->page_table_id;

	if (AllocateMemory(sizeof(struct MMUProcessObject), (void **)&mmu_po) < 0)
		return NULL;
	ZeroMemory(mmu_po, sizeof(struct MMUProcessObject));
	page_table_id++;
	mmu_po->next = NULL;
	mmu_po->desc_id = page_table_id;
	mmu_po->filp = NULL;
	mmu_po->process_id = 0;
	g_mmu->page_table_id = page_table_id;

	for (i = 0; i < MMU_REGION_COUNT; i++)
		MMU_ON_ERROR(CreateMutex(&mmu_po->region[i].node_mutex));

	return mmu_po;

onerror:
	pr_err(" *****MMU Create process object failed!*****\n");
	return NULL;
}

/*
 * Function name: MMUCreatePageTable
 * Description:
 *  create page table for multi-context selecttion
 * Parameter:
 *  filp: file pointer, which corresponding to process
 */
enum MMUStatus MMUCreatePageTable(void *filp)
{
	struct MMUProcessObject *mmu_po;
	int process_id = GetProcessID();
#ifdef MMU_PAGE_TABLE_SWITCH
	enum MMUStatus status;

	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);

	if (_HasCreatedPageTable(process_id) == 1) {
		MMUDEBUG("the process [%d] has created page table!\n", process_id);
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_PT_EXIST;
	}

	mmu_po = MMUCreateProcessObject();
	if (!mmu_po) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_FALSE;
	}
	/* get PTD, PD2 and PD1 */
	status = MMUSplitPTDPD2PD1(mmu_po);
	if (status < 0) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return status;
	}
	/* write ptd: set the pd2 phsical address to PTD */
	MMUWritePTD(mmu_po);
	/* write PD2 entries: set the pd1 physical to pd2 */
	MMUWritePD2Entry(mmu_po);
	if (simple_map)
		SMCreateNodes(mmu_po);
	else
		CreateNode(mmu_po);
	/* added new mmu po into list */
	MMUAddProcessObject(mmu_po);
#else
	mmu_po = g_mmu->mmu_po;
#endif
	mmu_po->filp = (struct file *)filp;
	mmu_po->process_id = process_id;

	ReleaseMutex(g_mmu->page_table_mutex);

	pr_info("%s: page table [%d] created!\n", __func__, g_mmu->page_table_id);

	return MMU_STATUS_OK;
}

/*
 * Function name: MMUReleaseRegionMapNode
 * Description:
 *  release region mapped node
 */
enum MMUStatus MMUReleaseRegionMapNode(struct MMUProcessObject *mmu_po,
				enum MMURegion e)
{
	struct MMUNode *p, *tmp;
	struct MMUNode *fp;

	if (simple_map) {
		p = mmu_po->region[e].simple_map_head;
		while (p) {
			tmp = p->next;
			FreeMemory(p);
			p = tmp;
			MMUDEBUG(" *****clean node*****\n");
		}
	} else {
		fp = mmu_po->region[e].free_map_head;
		p = mmu_po->region[e].map_head;
		while (fp) {
			tmp = fp->next;
			FreeMemory(fp);
			fp = tmp;
			MMUDEBUG(" *****clean free node*****\n");
		}

		while (p) {
			tmp = p->next;
			FreeMemory(p);
			p = tmp;
			MMUDEBUG(" *****clean node*****\n");
		}
	}
	return MMU_STATUS_OK;
}

#ifdef MMU_PAGE_TABLE_SWITCH
/*
 * Function name: MMUReleasePageTable
 * Description:
 *  release page table when process closed
 * Parameter:
 *  filp: file pointer, which corresponding to process
 */
enum MMUStatus MMUReleasePageTable(struct file *filp)
{
	struct MMUProcessObject *mmu_po = g_mmu->mmu_po;
	struct MMUProcessObject *prev = g_mmu->mmu_po;
	unsigned int vmid;
	int process_id = GetProcessID();
	int i;

	while (mmu_po && (mmu_po->process_id != process_id)) {
		prev = mmu_po;
		mmu_po = mmu_po->next;
	}

	if (mmu_po) {
		/*clear released page table's PTD */
		*((unsigned int *)mmu_po->ptd_virtual) = 0;
		*((unsigned int *)mmu_po->ptd_virtual + 1) = 0;

		vmid = mmu_po->desc_id & 0xF;
		g_mmu->vmid_waiting_flush[vmid] = 1;

		MMUDeleteProcessObject(prev, mmu_po);

		for (i = 0; i < MMU_REGION_COUNT; i++) {
			DeleteMutex(mmu_po->region[i].node_mutex);
			MMUReleaseRegionMapNode(mmu_po, i);
		}
		MMUDEBUG("%s: release page table[%d]\n", __func__,
				 mmu_po->desc_id & 0xFFFF);
		FreeMemory(mmu_po);
	} else {
		MMUDEBUG("%s: release page table failed\n", __func__);
	}

	return MMU_STATUS_OK;
}
#endif

enum MMUStatus MMUInit(volatile unsigned char *hwregs)
{
	enum MMUStatus status;
	void *pointer;
	struct MMUProcessObject *mmu_po;
#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 mmu_version = 0;
#endif

	if (mmu_init == MMU_TRUE) {
		/* All mmu use common table and dev, just initial once*/
		pr_notice(" *****MMU Already Initialed*****\n");
		return MMU_STATUS_OK;
	}

	if (!hwregs)
		return MMU_STATUS_NOT_FOUND;

#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
	mmu_version = ioread32((void __iomem *)(hwregs + MMU_REG_HW_ID));

	if ((mmu_version >> 16) != 0x4D4D)
		return MMU_STATUS_NOT_FOUND;

	pr_info("%s: mmu version=0x%8x\n", __func__, mmu_version);
#endif

	pr_notice(" *****MMU Init*****\n");

	/* Allocate memory for the MMU object. */
	MMU_ON_ERROR(AllocateMemory(sizeof(struct MMU), &pointer));
	ZeroMemory(pointer, sizeof(struct MMU));

	g_mmu = pointer;

	/* create MMU process object */
	g_mmu->page_table_id = -1;
	mmu_po = MMUCreateProcessObject();
	if (mmu_po)
		MMUAddProcessObject(mmu_po);
	else
		goto onerror;

	g_mmu->init_process_id = GetProcessID();
	g_mmu->page_table_mutex = NULL;

#ifdef SUPPORT_48PA_MMU
#if (MMU_VA_BITS == 40)
	g_mmu->mmu_version = MMU_40VA_48PA;
#elif (MMU_VA_BITS == 41)
	g_mmu->mmu_version = MMU_41VA_48PA;
#else
	g_mmu->mmu_version = MMU_NOT_SUPPORT;
#endif
#else
	g_mmu->mmu_version = MMU_32VA_40PA;
#endif

	if (g_mmu->mmu_version == MMU_NOT_SUPPORT) {
		goto onerror;
		status = MMU_STATUS_NOT_FOUND;
	}

	simple_map = 0;

	/* Create the page table mutex. */
	MMU_ON_ERROR(CreateMutex(&g_mmu->page_table_mutex));

	mmu_init = MMU_TRUE;
	return MMU_STATUS_OK;

onerror:
	pr_err(" *****MMU Init Error*****\n");
	if (g_mmu)
		FreeMemory(g_mmu);
	if (mmu_po)
		FreeMemory(mmu_po);
	return status;
}

enum MMUStatus MMURelease(void *filp)
{
	int i, j;
	struct MMUNode *p, *tmp;
	addr64_t address;
	unsigned int *page_table_entry;
	struct MMUProcessObject *mmu_po = NULL;
	int process_id = GetProcessID();
#ifdef SUPPORT_48PA_MMU
	struct MMUPageNode *page_node;
	addr64_t tmp_offset;
	/* MAX offset only 9bits  */
	addr64_t pd0_offset = 0xFFF;
	unsigned int pd_entry_value[2];
	unsigned int *tmp_entry;
#endif

	mmu_po = MMUGetProcessObject(process_id);
	if (!mmu_po)
		return MMU_STATUS_FALSE;

	/* if mmu or TLB not enabled, return */
	if (simple_map) {
		if (!g_mmu || !mmu_po->region[0].simple_map_head)
			return MMU_STATUS_OK;
	} else {
		if (!g_mmu || !mmu_po->region[0].map_head)
			return MMU_STATUS_OK;
	}

	pr_notice(" *****MMU Release*****\n");

	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);

	if (mmu_po->pd_filled == 0) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_OK;
	}

	if (simple_map) {
		for (i = 0; i < MMU_REGION_COUNT; i++) {
			p = mmu_po->region[i].simple_map_head->next;

			while (p) {
				tmp = p->next;
				if (p->filp == (struct file *)filp) {
					for (j = 0; j < p->page_count; j++) {
#ifdef SUPPORT_48PA_MMU
						address = (p->addr_align_start << MMU_PT_4K_SHIFT) + (j * 1 << MMU_PT_4K_SHIFT);
						tmp_offset = (address >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
						if (pd0_offset != tmp_offset) {
							GetPageEntry(p,
								     &page_table_entry, mmu_po,
									 j, MMU_PT);
							pd0_offset = tmp_offset;
						} else {
							page_table_entry = page_table_entry +  2;
						}

						pd_entry_value[1] = 0;
						pd_entry_value[0] = 0;
						WritePDEntry(page_table_entry, pd_entry_value);
						/* for pt */
						GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
						page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
						if (!page_node->use_count) {
							tmp_entry = (unsigned int *)page_node->page_table_entry;
							/* release used page node here */
							RemovePageNode(page_node, p->process_id);
							WritePDEntry(tmp_entry, pd_entry_value);
							/* for pd0 */
							tmp_offset = (address >> MMU_PD1_SHIFT) & mmu_page_bits[MMU_PD1];
							GetPageNode(tmp_offset, MMU_PD0, &page_node, mmu_po);
							page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
							if (!page_node->use_count) {
								tmp_entry = (unsigned int *)page_node->page_table_entry;
								/* release used page node here */
								RemovePageNode(page_node, p->process_id);
								WritePDEntry(tmp_entry, pd_entry_value);
							}
						}
#else
						GetPageEntry(p,
							     &page_table_entry, mmu_po,
								 j, MMU_NONE);
						address = 0;
						WritePageEntry(page_table_entry,
							       address);
#endif
					}
					SMRemoveNode(i, p->buf_virtual_address,
						     mmu_po);
				}
				p = tmp;
#ifdef SUPPORT_48PA_MMU
				pd0_offset = 0xFFF;
#endif
			}
		}
	} else {
		for (i = 0; i < MMU_REGION_COUNT; i++) {
			p = mmu_po->region[i].map_head->next;

			while (p) {
				tmp = p->next;
				if (p->filp == (struct file *)filp) {
					for (j = 0; j < p->page_count; j++) {
#ifdef SUPPORT_48PA_MMU
						address = (p->addr_align_start << MMU_PT_4K_SHIFT) + (j * 1 << MMU_PT_4K_SHIFT);
						tmp_offset = (address >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
						if (pd0_offset != tmp_offset) {
							GetPageEntry(p,
								     &page_table_entry, mmu_po,
									 j, MMU_PT);
							pd0_offset = tmp_offset;
						} else {
							page_table_entry = page_table_entry + 2;
						}

						pd_entry_value[1] = 0;
						pd_entry_value[0] = 0;
						WritePDEntry(page_table_entry, pd_entry_value);
						/* for pt */
						GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
						page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
						if (!page_node->use_count) {
							tmp_entry = (unsigned int *)page_node->page_table_entry;
							/* release used page node here */
							RemovePageNode(page_node, p->process_id);
							WritePDEntry(tmp_entry, pd_entry_value);
							/* for pd0 */
							tmp_offset = (address >> MMU_PD1_SHIFT) & mmu_page_bits[MMU_PD1];
							GetPageNode(tmp_offset, MMU_PD0, &page_node, mmu_po);
							page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
							if (!page_node->use_count) {
								tmp_entry = (unsigned int *)page_node->page_table_entry;
								RemovePageNode(page_node, p->process_id);
								WritePDEntry(tmp_entry, pd_entry_value);
							}
						}
#else
						GetPageEntry(p,
							     &page_table_entry, mmu_po,
								 j, MMU_NONE);

						address = 0;
						WritePageEntry(page_table_entry,
							       address);
#endif
					}

					RemoveNode(i, p->buf_virtual_address,
						   mmu_po);
				}
				p = tmp;
#ifdef SUPPORT_48PA_MMU
				pd0_offset = 0xFFF;
#endif
			}
		}
	}
#ifdef MMU_PAGE_TABLE_SWITCH
	MMUReleasePageTable(filp);
#endif

	//TODO(Yixiao):need check still have pt node used here ?
	ReleaseMutex(g_mmu->page_table_mutex);

	return MMU_STATUS_OK;
}

enum MMUStatus MMUCleanup(void)
{
	int i;
#ifdef SUPPORT_48PA_MMU
	struct MMUPageNode *page, *tmpage;
#endif
	struct MMUProcessObject *mmu_po = g_mmu->mmu_po, *tmp_po;

	pr_info(" *****MMU cleanup*****\n");

	MMUCleanupMemory();
	DeleteMutex(g_mmu->page_table_mutex);

	while (mmu_po) {
		tmp_po = mmu_po;
		for (i = 0; i < MMU_REGION_COUNT; i++) {
			DeleteMutex(mmu_po->region[i].node_mutex);
			MMUReleaseRegionMapNode(mmu_po, i);
		}
		FreeMemory(mmu_po);
		mmu_po = tmp_po->next;
	}
#ifdef SUPPORT_48PA_MMU
	page = g_mmu->page_free_head;
	while (page) {
		tmpage = page->next;
		FreeMemory(page);
		page = tmpage;
		MMUDEBUG(" *****clean free page node*****\n");
	}
	page = g_mmu->page_use_head;
	while (page) {
		tmpage = page->next;
		FreeMemory(page);
		page = tmpage;
		MMUDEBUG(" *****clean free page node*****\n");
	}
#endif
	FreeMemory(g_mmu);

	mmu_enable = 0;
	mmu_init = 0;

	return MMU_STATUS_OK;
}

static enum MMUStatus SetupDynamicSpace(void)
{
#ifndef SUPPORT_48PA_MMU
	int i;
	unsigned int stlb_entry, num_entries;
	addr64_t address;
	unsigned int *mtlb_virtual = (unsigned int *)g_mmu->mtlb_virtual;
#endif

	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);

#ifdef SUPPORT_48PA_MMU
	MMUWritePD2Entry(g_mmu->mmu_po);
	PageCreateNode();
#else
	address = g_mmu->stlb_physical;
	num_entries = MMU_MTLB_ENTRY_NUM;
	for (i = 0; i < num_entries; i++) {
		stlb_entry = address
				 /* 4KB page size */
				 | (0 << 2)
				 /* Ignore exception */
				 | (0 << 1)
				 /* Present */
				 | (1 << 0);
		WritePageEntry(mtlb_virtual++, stlb_entry);
		address += MMU_STLB_4K_SIZE;
	}
#endif

	ReleaseMutex(g_mmu->page_table_mutex);

	/* Initial map info. */
	if (simple_map)
		SMCreateNodes(g_mmu->mmu_po);
	else
		CreateNode(g_mmu->mmu_po);

	return MMU_STATUS_OK;
}

enum MMUStatus MMUSetup(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2])
{
#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
	unsigned int address, address_ext, size;
	u32 i = 0;

	MMUGetPageTableArrayAddr(g_mmu->mmu_po, &address, &address_ext);
	size = g_mmu->page_table_array_size;

	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		if (hwregs[i][0]) {
			MMUDEBUG("hwregs[%u][0]=%p, id=0x%08x", i, hwregs[i][0],
				 ioread32((void __iomem *)(hwregs[i][0] +
				     MMU_REG_HW_ID)));
#ifndef PD_MODE
			iowrite32(address, (void __iomem *)(hwregs[i][0] +
					MMU_REG_ADDRESS));
			iowrite32(address_ext, (void __iomem *)(hwregs[i][0] +
					MMU_REG_ADDRESS_MSB));
#ifdef SUPPORT_48PA_MMU
			iowrite32(size, (void __iomem *)(hwregs[i][0] +
						  MMU_REG_ARRAY_SIZE));
			pr_info("page table array size: 0x%8x\n", size);
#endif
			iowrite32(0x1, (void __iomem *)(hwregs[i][0] +
					  MMU_REG_CONTROL));
			iowrite32(0x10000, (void __iomem *)(hwregs[i][0] +
					  MMU_REG_PAGE_TABLE_ID));
			iowrite32(0x00000, (void __iomem *)(hwregs[i][0] +
					  MMU_REG_PAGE_TABLE_ID));
			//TODO: if write page table before enable, it will BYPASS MMU
			//iowrite32(0x1, (void __iomem *)(hwregs[i][0] +
			//		  MMU_REG_CONTROL));
#else
			pr_info(" *****MMU PD Mode Enabled*****\n");
			iowrite32((MTLB_PCIE_START_ADDRESS>>8)&0xfffffff0, (void __iomem *)(hwregs[i][0] +
					  MMU_REG_PDENTRY0));
			iowrite32(0x21, (void __iomem *)(hwregs[i][0] +
					  MMU_REG_CONTROL));
#endif
		}
		if (hwregs[i][1]) {
			MMUDEBUG("hwregs[%u][1]=%p, id=0x%08x", i, hwregs[i][1],
				 ioread32((void __iomem *)(hwregs[i][1] +
				     MMU_REG_HW_ID)));
#ifndef PD_MODE
			iowrite32(address, (void __iomem *)(hwregs[i][1] +
					  MMU_REG_ADDRESS));
			iowrite32(address_ext, (void __iomem *)(hwregs[i][1] +
				      MMU_REG_ADDRESS_MSB));
#ifdef SUPPORT_48PA_MMU
			iowrite32(size, (void __iomem *)(hwregs[i][1] +
						  MMU_REG_ARRAY_SIZE));
#endif
			iowrite32(1, (void __iomem *)(hwregs[i][1] +
				      MMU_REG_CONTROL));
			iowrite32(0x10000, (void __iomem *)(hwregs[i][1] +
				      MMU_REG_PAGE_TABLE_ID));
			iowrite32(0x00000, (void __iomem *)(hwregs[i][1] +
				      MMU_REG_PAGE_TABLE_ID));
			//iowrite32(1, (void __iomem *)(hwregs[i][1] +
			//	      MMU_REG_CONTROL));
#else
			iowrite32((MTLB_PCIE_START_ADDRESS>>8)&0xfffffff0, (void __iomem *)(hwregs[i][1] +
					  MMU_REG_PDENTRY0));
			iowrite32(0x21, (void __iomem *)(hwregs[i][1] +
				      MMU_REG_CONTROL));
#endif
		}
	}
#endif

	return MMU_STATUS_OK;
}

/*
 *-----------------------------------------------------------------------------
 *  Function name: MMUEnable
 *	Description:
 *	    Create TLB, set registers and enable MMU
 *
 *  For pcie, TLB buffers come from FPGA memory and The distribution is as follows
 *	   MTLB:              start from: 0x00100000, size: 4K bits
 *	   page table array:              0x00200000        64 bits
 *	   STLB:                          0x00300000        4M bits
 *-----------------------------------------------------------------------------
 */
enum MMUStatus MMUEnable(volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2])
{
	enum MMUStatus status = MMU_STATUS_FALSE;
	unsigned int mutex = MMU_FALSE;

	if (mmu_enable == MMU_TRUE) {
		pr_info(" *****MMU Already Enabled*****\n");
		return MMU_STATUS_OK;
	}

	pr_info(" *****MMU Enable...*****\n");

	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
	mutex = MMU_TRUE;
#ifdef PCIE_EN
#ifdef SUPPORT_48PA_MMU
	g_mmu->pd2_unit_size = _ALIGN(MMU_PD2_SIZE, PD2_SIZE_ALIGN);
	g_mmu->pd2_size = PD1_PCIE_START_ADDRESS - PD2_PCIE_START_ADDRESS;
	g_mmu->pd2_physical = PD2_PCIE_START_ADDRESS;
	g_mmu->pd2_virtual = PcieReserveMemory(g_mmu->pd2_physical, g_mmu->pd2_size, "PD2");
	if (!g_mmu->pd2_virtual)
		goto onerror;

	g_mmu->mmu_po->pd2_physical = g_mmu->pd2_physical;
	g_mmu->mmu_po->pd2_virtual = g_mmu->pd2_virtual;
	g_mmu->pd1_unit_size = _ALIGN(MMU_PD1_SIZE * MMU_PD2_ENTRY_NUM, PD1_SIZE_ALIGN);
	g_mmu->pd1_size = DYNAMIC_PAGE_START_ADDRESS - PD1_PCIE_START_ADDRESS;
	g_mmu->pd1_physical = PD1_PCIE_START_ADDRESS;
	g_mmu->pd1_virtual = PcieReserveMemory(g_mmu->pd1_physical, g_mmu->pd1_size, "PD1");
	if (!g_mmu->pd1_virtual)
		goto onerror;

	g_mmu->mmu_po->pd1_physical = g_mmu->pd1_physical;
	g_mmu->mmu_po->pd1_virtual = g_mmu->pd1_virtual;
	/* for 4k and 64k mode, can't get enough mem for page table */
	g_mmu->dy_pg_size = DYNAMIC_PAGE_END_ADDRESS - DYNAMIC_PAGE_START_ADDRESS;
	g_mmu->dy_pg_physical = DYNAMIC_PAGE_START_ADDRESS;
	g_mmu->dy_pg_virtual = PcieReserveMemory(g_mmu->dy_pg_physical, g_mmu->dy_pg_size, "PD0_PT(dynamic)");
	if (!g_mmu->dy_pg_virtual)
		goto onerror;

	g_mmu->dy_pg_physical_base = g_mmu->dy_pg_physical;
	g_mmu->page_use_head = NULL;
	g_mmu->page_use_tail = NULL;
	g_mmu->page_free_head = NULL;
	g_mmu->page_free_tail = NULL;
	/* set PTD base addr in page_table_array */
	g_mmu->page_table_array_unit_size = PTD_ENTRY_SIZE / 2;
	g_mmu->page_table_array_size = PD2_PCIE_START_ADDRESS - PDT_PCIE_START_ADDRESS;//PTD_ENTRY_SIZE;
	g_mmu->page_table_array_physical = PDT_PCIE_START_ADDRESS;
#else
	g_mmu->mtlb_size = MMU_MTLB_SIZE;
	g_mmu->mtlb_physical = MTLB_PCIE_START_ADDRESS;
	g_mmu->mtlb_virtual = PcieReserveMemory(g_mmu->mtlb_physical, g_mmu->mtlb_size, "MTLB");
	if (!g_mmu->mtlb_virtual)
		goto onerror;

	g_mmu->stlb_size = MMU_MTLB_ENTRY_NUM * MMU_STLB_4K_SIZE;
	g_mmu->stlb_physical = STLB_PCIE_START_ADDRESS;
	g_mmu->stlb_virtual = PcieReserveMemory(g_mmu->stlb_physical, g_mmu->stlb_size, "STLB");
	if (!g_mmu->stlb_virtual)
		goto onerror;

	g_mmu->page_table_array_size = PTD_ENTRY_SIZE;
	g_mmu->page_table_array_physical = PAGE_PCIE_START_ADDRESS;
#endif

	g_mmu->page_table_array =
		PcieReserveMemory(g_mmu->page_table_array_physical, g_mmu->page_table_array_size, "PTD");
	if (!g_mmu->page_table_array)
		goto onerror;
#else
#ifdef SUPPORT_48PA_MMU
	/* Allocate the 4K mode MTLB table. */
	g_mmu->pd2_unit_size = _ALIGN(MMU_PD2_SIZE, PD2_SIZE_ALIGN);
	g_mmu->pd2_size = PD1_PCIE_START_ADDRESS - PD2_PCIE_START_ADDRESS;
	g_mmu->pd2_virtual = DmaAllocMemory(platformdev, g_mmu->pd2_size,
				   "PD2", &g_mmu->pd2_physical);
	if (!g_mmu->pd2_virtual) {
		pr_err("hantrodec alloc buffer fail\n");
		goto onerror;
	}
	g_mmu->mmu_po->pd2_physical = g_mmu->pd2_physical;
	g_mmu->mmu_po->pd2_virtual = g_mmu->pd2_virtual;

	g_mmu->pd1_unit_size = _ALIGN(MMU_PD1_SIZE * MMU_PD2_ENTRY_NUM, PD1_SIZE_ALIGN);
	g_mmu->pd1_size = DYNAMIC_PAGE_START_ADDRESS - PD1_PCIE_START_ADDRESS;
	g_mmu->pd1_virtual = DmaAllocMemory(platformdev, g_mmu->pd1_size,
				   "PD1", &g_mmu->pd1_physical);
	if (!g_mmu->pd1_virtual) {
		pr_err("hantrodec alloc buffer fail\n");
		goto onerror;
	}
	g_mmu->mmu_po->pd1_physical = g_mmu->pd1_physical;
	g_mmu->mmu_po->pd1_virtual = g_mmu->pd1_virtual;

	/* for 4k and 64k mode, can't get enough mem for page table */
	g_mmu->dy_pg_size = DYNAMIC_PAGE_END_ADDRESS - DYNAMIC_PAGE_START_ADDRESS;
	g_mmu->dy_pg_virtual = DmaAllocMemory(platformdev, g_mmu->dy_pg_size,
					"PD0_PT(dynamic)", &g_mmu->dy_pg_physical);
	if (!g_mmu->dy_pg_virtual) {
		pr_err("hantrodec alloc buffer fail\n");
		goto onerror;
	}
	g_mmu->dy_pg_physical_base = g_mmu->dy_pg_physical;
	g_mmu->page_use_head = NULL;
	g_mmu->page_use_tail = NULL;
	g_mmu->page_free_head = NULL;
	g_mmu->page_free_tail = NULL;
	g_mmu->page_table_array_size = PAGE_TABLE_ARRAY_SIZE;
	g_mmu->page_table_array_unit_size = PTD_ENTRY_SIZE / 2;
#else
	/* Allocate the 4K mode MTLB table. */
	g_mmu->mtlb_size = MMU_MTLB_SIZE;
	g_mmu->mtlb_virtual = DmaAllocMemory(platformdev, g_mmu->mtlb_size,
				   "MTLB", &g_mmu->mtlb_physical);
	if (!g_mmu->mtlb_virtual) {
		pr_err("hantrodec alloc buffer fail\n");
		goto onerror;
	}
	g_mmu->stlb_size = MMU_MTLB_ENTRY_NUM * MMU_STLB_4K_SIZE;
	g_mmu->stlb_virtual = DmaAllocMemory(platformdev, g_mmu->stlb_size,
				   "STLB", &g_mmu->stlb_physical);
	if (!g_mmu->stlb_virtual) {
		pr_err("hantrodec alloc buffer fail\n");
		goto onerror;
	}
	g_mmu->page_table_array_size = PTD_ENTRY_SIZE;
#endif
	g_mmu->page_table_array = DmaAllocMemory(platformdev,
			g_mmu->page_table_array_size,
			"PTD", &g_mmu->page_table_array_physical);
	if (!g_mmu->page_table_array) {
		pr_err("hantrodec alloc buffer fail\n");
		goto onerror;
	}
#endif
	g_mmu->mmu_po->ptd_physical = g_mmu->page_table_array_physical;
	g_mmu->mmu_po->ptd_virtual = g_mmu->page_table_array;

#ifdef SUPPORT_48PA_MMU
	MMUWritePTD(g_mmu->mmu_po);
#else
	*((unsigned int *)g_mmu->page_table_array) =
		(g_mmu->mtlb_physical & 0xFFFFF000) | (0 << 0);//(g_mmu->mtlb_physical & 0xFFFFFC00) | (0 << 0);//(g_mmu->mtlb_physical & 0xFFFFF000) | (0 << 0);
	*((unsigned int *)g_mmu->page_table_array + 1) =
		(u32)(g_mmu->mtlb_physical >> 32) & 0xff;
#if 0
	/*TODO: need check if need config page_table_arary[2] */
	*((unsigned int *)g_mmu->page_table_array + 2) =
		(g_mmu->mtlb_physical & 0xFFFFF000) | (0 << 0);//(g_mmu->mtlb_physical & 0xFFFFFC00) | (0 << 0);//(g_mmu->mtlb_physical & 0xFFFFF000) | (0 << 0);
	*((unsigned int *)g_mmu->page_table_array + 3) =
		(u32)(g_mmu->mtlb_physical >> 32) & 0xff;
#endif

		MMUDEBUG(" Page table array[0]: lsb = 0x%08x\n",
			 ((int *)g_mmu->page_table_array)[0]);
		MMUDEBUG("                      msb = 0x%08x\n",
			 ((int *)g_mmu->page_table_array)[1]);
#endif

	ReleaseMutex(g_mmu->page_table_mutex);

	MMU_ON_ERROR(SetupDynamicSpace());

	/* set regs of all MMUs */
	MMUSetup(hwregs);

	mmu_enable = MMU_TRUE;
	return MMU_STATUS_OK;

onerror:
	MMUCleanupMemory();
	if (mutex)
		ReleaseMutex(g_mmu->page_table_mutex);
	MMUDEBUG(" *****MMU Enable Error*****\n");
	return status;
}

/*
 *------------------------------------------------------------------------------
 *  Function name: MMUFlush
 *	Description:
 *	    Flush MMU reg to update cache in MMU.
 *------------------------------------------------------------------------------
 */
static enum MMUStatus MMUFlush(u32 core_id, volatile unsigned char *hwregs[MAX_SUBSYS_NUM][2])
{
	enum MMUStatus status;
	unsigned int mutex = MMU_FALSE;

	MMUDEBUG(" *****MMU Flush*****\n");
	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
	mutex = MMU_TRUE;

	if (hwregs[core_id][0]) {
#ifdef SUPPORT_48PA_MMU
		iowrite32(0x80000000,
			  (void __iomem *)(hwregs[core_id][0] + MMU_REG_PTFLUSH));
#else
		iowrite32(0x10,
			  (void __iomem *)(hwregs[core_id][0] + MMU_REG_FLUSH));
		iowrite32(0x00,
			  (void __iomem *)(hwregs[core_id][0] + MMU_REG_FLUSH));
#endif
	} else {
		pr_err("hantrodec alloc buffer fail\n");
		status = MMU_STATUS_FALSE;
		goto onerror;
	}
	if (hwregs[core_id][1]) {
#ifdef SUPPORT_48PA_MMU
		iowrite32(0x80000000,
			  (void __iomem *)(hwregs[core_id][1] + MMU_REG_PTFLUSH));
#else
		iowrite32(0x10,
			  (void __iomem *)(hwregs[core_id][1] + MMU_REG_FLUSH));
		iowrite32(0x00,
			  (void __iomem *)(hwregs[core_id][1] + MMU_REG_FLUSH));
#endif
	}

	ReleaseMutex(g_mmu->page_table_mutex);
	return MMU_STATUS_OK;

onerror:
	if (mutex)
		ReleaseMutex(g_mmu->page_table_mutex);

	MMUDEBUG(" *****MMU Flush Error*****\n");
	return status;
}

static enum MMUStatus MMUMemNodeMap(struct addr_desc *addr, struct file *filp)
{
	enum MMUStatus status;
	unsigned int page_count = 0;
	unsigned int i = 0;
	struct MMUNode *p;
	addr64_t address = 0x0, addr_tmp = 0x0;
	unsigned int *page_table_entry;
	enum MMURegion e;
	unsigned int mutex = MMU_FALSE;
	struct MMUProcessObject *mmu_po = NULL;
	int process_id =  GetProcessID();
#ifdef SUPPORT_48PA_MMU
	struct MMUPageNode *page_node;
	addr64_t tmp_offset = 0x0;
	addr64_t pd0_offset = 0xFFF;
	u32 pd_entry_value[2];
#else
	u32 ext_addr;
	u32 page_entry_value = 0;
#endif

	MMUDEBUG(" *****MMU Map*****\n");
	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
	mutex = MMU_TRUE;

	page_count = (addr->size - 1) / PAGE_SIZE + 1;

	GetPhysicalAddress(addr->virtual_address, &address);
	addr_tmp = address;
	/* select the region */
	address &= MMU_IOVA_BITS_MASK;
	MMUDEBUG(" *****MMU map address*****%llx\n", address);
	if (address >= REGION_IN_START && address + addr->size < REGION_IN_END)
		e = MMU_REGION_IN;
	else if (address >= REGION_OUT_START &&
		 address + addr->size < REGION_OUT_END)
		e = MMU_REGION_OUT;
	else if (address >= REGION_PRIVATE_START &&
		 address + addr->size < REGION_PRIVATE_END)
		e = MMU_REGION_PRIVATE;
	else
		e = MMU_REGION_PUB;

	address = addr_tmp;

	mmu_po = MMUGetProcessObject(process_id);
	if (!mmu_po) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_FALSE;
	}

	if (simple_map) {
		MMU_ON_ERROR(SMCheckAddress(e, addr->virtual_address, mmu_po));

		SMCreateNode(e, &p, page_count, mmu_po);
		MMUDEBUG(" *****Node map size*****%u\n", page_count);

		p->buf_virtual_address = addr->virtual_address;
		p->process_id = process_id;
		p->filp = filp;
		p->addr_align_start = (address >> MMU_4k_SHIFT) & 0xFFFFFFF;
		p->addr_align_end = p->addr_align_start + page_count;
		p->page_count = page_count;

		for (i = 0; i < page_count; i++) {
			GetPhysicalAddress(addr->virtual_address +
							 i * PAGE_SIZE,
						 &address);
#ifdef SUPPORT_48PA_MMU
			tmp_offset = (((p->addr_align_start + i) << MMU_PT_4K_SHIFT) >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
			if (pd0_offset != tmp_offset) {
				GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_PT);
				pd0_offset = tmp_offset;
			} else {
				page_table_entry = page_table_entry + 2;
			}

			/* physical address bits [48:32] */
			pd_entry_value[1] = (address >> 32) & 0xFFFF;
			/* physical address bits [32:12] */
			pd_entry_value[0] = (address & 0xFFFFF000)
				/* writable */
				| (1 << 4)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
			WritePDEntry(page_table_entry, pd_entry_value);
			/* for pt */
			GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
			/* use_count should not bigger than PT_ENTRY_NUM */
			page_node->use_count = (page_node->use_count >= MMU_PT_4K_ENTRY_NUM) ?
						MMU_PT_4K_ENTRY_NUM : page_node->use_count + 1;
#else
			GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_NONE);

			ext_addr = ((u32)(address >> 32)) & 0xff;
			page_entry_value =
					(address & 0xFFFFF000)
					/* ext address , physical address bits [39,32]*/
					| (ext_addr << 4)
					/* writable */
					| (1 << 2)
					/* Ignore exception */
					| (0 << 1)
					/* Present */
					| (1 << 0);
			WritePageEntry(page_table_entry, page_entry_value);
#endif
		}

		/* Purpose of Bare_metal mode: input bus address==mmu address*/
		addr->bus_address = (p->addr_align_start << 12) & 0xFFFFFFFFFFULL;
	} else {
		MMU_ON_ERROR(FindFreeNode(e, &p, page_count, mmu_po));

		SplitFreeNode(e, &p, page_count, mmu_po);
		MMUDEBUG(" *****Node map size*****%u\n", p->page_count);

		p->buf_virtual_address = addr->virtual_address;
		p->process_id = process_id;
		p->filp = filp;

		for (i = 0; i < page_count; i++) {
			GetPhysicalAddress(addr->virtual_address +
							 i * PAGE_SIZE,
						 &address);
#ifdef SUPPORT_48PA_MMU
			tmp_offset = (((p->addr_align_start + i) << MMU_PT_4K_SHIFT) >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
			if (pd0_offset != tmp_offset) {
				GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_PT);
				pd0_offset = tmp_offset;
			} else {
				page_table_entry = page_table_entry + 2;
			}

			/* physical address bits [48:32] */
			pd_entry_value[1] = (address >> 32) & 0xFFFF;
			/* physical address bits [32:12] */
			pd_entry_value[0] = (address & 0xFFFFF000)
				/* writable */
				| (1 << 4)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
			WritePDEntry(page_table_entry, pd_entry_value);
			/* for pt */
			GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
			/* use_count should not bigger than PT_ENTRY_NUM */
			page_node->use_count = (page_node->use_count >= page_node->page_count) ?
						page_node->page_count : page_node->use_count + 1;
#else
			GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_NONE);

			ext_addr = ((u32)(address >> 32)) & 0xff;
			page_entry_value =
					(address & 0xFFFFF000)
					/* ext address , physical address bits [39,32]*/
					| (ext_addr << 4)
					/* writable */
					| (1 << 2)
					/* Ignore exception */
					| (0 << 1)
					/* Present */
					| (1 << 0);
			WritePageEntry(page_table_entry, page_entry_value);
#endif
		}
		addr->bus_address = (p->addr_align_start << 12) & 0xFFFFFFFFFFULL;
	}

	MMUDEBUG(" %s map total %u pages in region %d\n",
		__func__, page_count, e);
	MMUDEBUG(" %s map %p -> 0x%llx\n", __func__, addr->virtual_address,
		 addr->bus_address);

	mmu_po->pd_filled = 1;
	ReleaseMutex(g_mmu->page_table_mutex);

	return MMU_STATUS_OK;

onerror:
	if (mutex)
		ReleaseMutex(g_mmu->page_table_mutex);

	MMUDEBUG(" *****MMU Map Error*****\n");
	return status;
}

static enum MMUStatus MMUMemNodeUnmap(struct addr_desc *addr)
{
	unsigned int i;
	addr64_t address = 0x0, addr_tmp = 0x0;
	unsigned int *page_table_entry;
	int process_id = GetProcessID();
	enum MMURegion e = MMU_REGION_COUNT;
	enum MMUStatus status = MMU_STATUS_OUT_OF_MEMORY;
	struct MMUNode *p;
	unsigned int mutex = MMU_FALSE;
	struct MMUProcessObject *mmu_po = NULL;
#ifdef SUPPORT_48PA_MMU
	addr64_t tmp_offset;
	/* MAX offset only 9bits  */
	addr64_t pd0_offset = 0xFFF;
	unsigned int *tmp_entry;
	unsigned int pd_entry_value[2];
	struct MMUPageNode *page_node;
#endif

	MMUDEBUG(" *****MMU Unmap*****\n");
	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
	mutex = MMU_TRUE;

	GetPhysicalAddress(addr->virtual_address, &address);
	addr_tmp = address;
	/* select the region */
	address &= MMU_IOVA_BITS_MASK;

	if (address >= REGION_IN_START && address < REGION_IN_END)
		e = MMU_REGION_IN;
	else if (address >= REGION_OUT_START && address < REGION_OUT_END)
		e = MMU_REGION_OUT;
	else if (address >= REGION_PRIVATE_START &&
		 address < REGION_PRIVATE_END)
		e = MMU_REGION_PRIVATE;
	else
		e = MMU_REGION_PUB;

	address = addr_tmp;

	mmu_po = MMUGetProcessObject(process_id);
	if (!mmu_po) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_FALSE;
	}

	if (simple_map)
		p = mmu_po->region[e].simple_map_head->next;
	else
		p = mmu_po->region[e].map_head->next;
	/* Reset STLB of the node */
	while (p) {
		if (p->buf_virtual_address == addr->virtual_address &&
		    p->process_id == process_id) {
			for (i = 0; i < p->page_count; i++) {
#ifdef SUPPORT_48PA_MMU
				/* get the virtual address for current page */
				address = (p->addr_align_start << MMU_PT_4K_SHIFT) + (i << MMU_PT_4K_SHIFT);
				tmp_offset = (address >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
				if (pd0_offset != tmp_offset) {
					GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_PT);
					pd0_offset = tmp_offset;
				} else {
					page_table_entry = page_table_entry + 2;
				}

				pd_entry_value[1] = 0;
				pd_entry_value[0] = 0;
				WritePDEntry(page_table_entry, pd_entry_value);
				/* for pt */
				GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
				page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
				if (!page_node->use_count) {
					tmp_entry = (unsigned int *)page_node->page_table_entry;
					/* release used pt node here */
					RemovePageNode(page_node, p->process_id);
					WritePDEntry(tmp_entry, pd_entry_value);
					/* for pd0 */
					tmp_offset = (address >> MMU_PD1_SHIFT) & mmu_page_bits[MMU_PD1];
					GetPageNode(tmp_offset, MMU_PD0, &page_node, mmu_po);
					page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
					if (!page_node->use_count) {
						tmp_entry = (unsigned int *)page_node->page_table_entry;
						/* release used page node here */
						RemovePageNode(page_node, p->process_id);
						WritePDEntry(tmp_entry, pd_entry_value);
					}
				}
#else
				GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_NONE);

				address = 0;
				WritePageEntry(page_table_entry, address);
#endif
			}
			break;
		}
		p = p->next;
	}
	if (!p)
		goto onerror;

	if (simple_map)
		SMRemoveNode(e, addr->virtual_address, mmu_po);
	else
		RemoveNode(e, addr->virtual_address, mmu_po);

	ReleaseMutex(g_mmu->page_table_mutex);
	return MMU_STATUS_OK;

onerror:
	if (mutex)
		ReleaseMutex(g_mmu->page_table_mutex);

	MMUDEBUG(" *****MMU Unmap Error*****\n");
	return status;
}

enum MMUStatus MMUKernelMemNodeMap(struct kernel_addr_desc *addr, void *filp)
{
	enum MMUStatus status;
	unsigned int page_count = 0;
	unsigned int i = 0;
	struct MMUNode *p;
	addr64_t address = 0x0;
	unsigned int *page_table_entry;
	enum MMURegion e;
	unsigned int mutex = MMU_FALSE;
	struct MMUProcessObject *mmu_po = NULL;
	int process_id =  GetProcessID();
#ifdef SUPPORT_48PA_MMU
	struct MMUPageNode *page_node;
	addr64_t tmp_offset = 0x0;
	addr64_t pd0_offset = 0xFFF;
	u32 pd_entry_value[2];
#else
	u32 ext_addr;
	u32 page_entry_value = 0;
#endif

	MMUDEBUG(" *****MMU Map*****\n");
	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
	mutex = MMU_TRUE;

	page_count = (addr->size - 1) / PAGE_SIZE + 1;

	address = addr->bus_address;
	/* select the region */
	address &= MMU_IOVA_BITS_MASK;
	MMUDEBUG(" *****MMU map address*****%llx\n", address);
	if (address >= REGION_IN_START && address + addr->size < REGION_IN_END)
		e = MMU_REGION_IN;
	else if (address >= REGION_OUT_START &&
		 address + addr->size < REGION_OUT_END)
		e = MMU_REGION_OUT;
	else if (address >= REGION_PRIVATE_START &&
		 address + addr->size < REGION_PRIVATE_END)
		e = MMU_REGION_PRIVATE;
	else
		e = MMU_REGION_PUB;

	address = addr->bus_address;

	mmu_po = MMUGetProcessObject(process_id);
	if (!mmu_po) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_FALSE;
	}

	if (simple_map) {
		//TODO: should check bus addr
		//MMU_ON_ERROR(SMCheckAddress(e, addr->virtual_address, mmu_po));

		SMCreateNode(e, &p, page_count, mmu_po);
		MMUDEBUG(" *****Node map size*****%u\n", page_count);

		p->buf_bus_address = addr->bus_address;
		p->process_id = process_id;
		p->filp = (struct file *)filp;

		p->addr_align_start = (address >> MMU_4k_SHIFT) & 0xFFFFFFF;
		p->addr_align_end = p->addr_align_start + page_count;
		p->page_count = page_count;

		for (i = 0; i < page_count; i++) {
			/* this function used in kernel only,
			 *so we think it's a contunuous buffer
			 */
			address += (i ? PAGE_SIZE : 0);
#ifdef SUPPORT_48PA_MMU
			tmp_offset = (((p->addr_align_start + i) << MMU_PT_4K_SHIFT) >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
			if (pd0_offset != tmp_offset) {
				GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_PT);
				pd0_offset = tmp_offset;
			} else {
				page_table_entry = page_table_entry + 2;
			}

			/* physical address bits [48:32] */
			pd_entry_value[1] = (address >> 32) & 0xFFFF;
			/* physical address bits [32:12] */
			pd_entry_value[0] = (address & 0xFFFFF000)
				/* writable */
				| (1 << 4)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
			WritePDEntry(page_table_entry, pd_entry_value);
			/* for pt */
			GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
			/* use_count should not bigger than PT_ENTRY_NUM */
			page_node->use_count = (page_node->use_count >= MMU_PT_4K_ENTRY_NUM) ?
						MMU_PT_4K_ENTRY_NUM : page_node->use_count + 1;
#else
			GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_NONE);

			ext_addr = ((u32)(address >> 32)) & 0xff;
			page_entry_value =
				(address & 0xFFFFF000)
				/* ext address , physical address bits [39,32]*/
				| (ext_addr << 4)
				/* writable */
				| (1 << 2)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
			WritePageEntry(page_table_entry, page_entry_value);
#endif
		}

		/* Purpose of Bare_metal mode: input bus address==mmu address*/
		addr->mmu_bus_address = (p->addr_align_start << 12) & 0xFFFFFFFFFFULL;
	} else {
		MMU_ON_ERROR(FindFreeNode(e, &p, page_count, mmu_po));

		SplitFreeNode(e, &p, page_count, mmu_po);
		MMUDEBUG(" *****Node map size*****%u\n", p->page_count);

		p->buf_bus_address = addr->bus_address;
		p->process_id = process_id;
		p->filp = (struct file *)filp;
		for (i = 0; i < page_count; i++) {
			/* this function used in kernel only,
			 *so we think it's a contunuous buffer
			 */
			address += (i ? PAGE_SIZE : 0);
#ifdef SUPPORT_48PA_MMU
			tmp_offset = (((p->addr_align_start + i) << MMU_PT_4K_SHIFT) >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
			if (pd0_offset != tmp_offset) {
				GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_PT);
				pd0_offset = tmp_offset;
			} else {
				page_table_entry = page_table_entry + 2;
			}

			/* physical address bits [48:32] */
			pd_entry_value[1] = (address >> 32) & 0xFFFF;
			/* physical address bits [32:12] */
			pd_entry_value[0] = (address & 0xFFFFF000)
				/* writable */
				| (1 << 4)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
			WritePDEntry(page_table_entry, pd_entry_value);
			/* for pt */
			GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
			/* use_count should not bigger than PT_ENTRY_NUM */
			page_node->use_count = (page_node->use_count >= MMU_PT_4K_ENTRY_NUM) ?
						MMU_PT_4K_ENTRY_NUM : page_node->use_count + 1;
#else
			GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_NONE);

			ext_addr = ((u32)(address >> 32)) & 0xff;
			page_entry_value =
				(address & 0xFFFFF000)
				/* ext address , physical address bits [39,32]*/
				| (ext_addr << 4)
				/* writable */
				| (1 << 2)
				/* Ignore exception */
				| (0 << 1)
				/* Present */
				| (1 << 0);
			WritePageEntry(page_table_entry, page_entry_value);
#endif
		}
		addr->mmu_bus_address = (p->addr_align_start << 12) & 0xFFFFFFFFFFULL;
	}

	MMUDEBUG(" %s map total %u pages in region %d\n",
		__func__, page_count, e);
	MMUDEBUG(" %s map 0x%llx -> 0x%llx\n", __func__, addr->bus_address,
		 addr->mmu_bus_address);

	ReleaseMutex(g_mmu->page_table_mutex);
	return MMU_STATUS_OK;

onerror:
	if (mutex)
		ReleaseMutex(g_mmu->page_table_mutex);

	MMUDEBUG(" *****MMU Map Error*****\n");
	return status;
}

enum MMUStatus MMUKernelMemNodeUnmap(struct kernel_addr_desc *addr)
{
	unsigned int i;
	addr64_t address = 0x0;
	unsigned int *page_table_entry;
	int process_id = GetProcessID();
	enum MMURegion e = MMU_REGION_COUNT;
	enum MMUStatus status = MMU_STATUS_OUT_OF_MEMORY;
	struct MMUNode *p;
	unsigned int mutex = MMU_FALSE;
	struct MMUProcessObject *mmu_po = NULL;
#ifdef SUPPORT_48PA_MMU
	addr64_t tmp_offset;
	/* MAX offset only 9bits  */
	addr64_t pd0_offset = 0xFFF;
	unsigned int *tmp_entry;
	unsigned int pd_entry_value[2];
	struct MMUPageNode *page_node;
#endif

	MMUDEBUG(" *****MMU Unmap*****\n");
	AcquireMutex(g_mmu->page_table_mutex, MMU_INFINITE);
	mutex = MMU_TRUE;

	address = addr->bus_address;
	/* select the region */
	address &= MMU_IOVA_BITS_MASK;
	if (address >= REGION_IN_START && address < REGION_IN_END)
		e = MMU_REGION_IN;
	else if (address >= REGION_OUT_START && address < REGION_OUT_END)
		e = MMU_REGION_OUT;
	else if (address >= REGION_PRIVATE_START &&
		 address < REGION_PRIVATE_END)
		e = MMU_REGION_PRIVATE;
	else
		e = MMU_REGION_PUB;

	address = addr->bus_address;

	if (g_mmu->mmu_po->next)
		mmu_po = MMUGetProcessObject(process_id);
	else
		mmu_po = g_mmu->mmu_po;
	if (!mmu_po) {
		ReleaseMutex(g_mmu->page_table_mutex);
		return MMU_STATUS_FALSE;
	}

	if (simple_map) {
		if (mmu_po->region[e].simple_map_head)
			p = mmu_po->region[e].simple_map_head->next;
	} else {
		if (mmu_po->region[e].map_head)
			p = mmu_po->region[e].map_head->next;
	}
	/* Reset STLB of the node */
	while (p) {
		if (p->buf_bus_address == addr->bus_address &&
			(p->process_id == process_id || p->process_id == g_mmu->init_process_id/* p->filp == NULL*/)) {
			for (i = 0; i < p->page_count; i++) {
#ifdef SUPPORT_48PA_MMU
				/* get the virtual address for current page */
				address = (p->addr_align_start << MMU_PT_4K_SHIFT) + (i << MMU_PT_4K_SHIFT);
				tmp_offset = (address >> MMU_PD0_SHIFT) & mmu_page_bits[MMU_PD0];
				if (pd0_offset != tmp_offset) {
					GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_PT);
					pd0_offset = tmp_offset;
				} else {
					page_table_entry = page_table_entry + 2;
				}

				pd_entry_value[1] = 0;
				pd_entry_value[0] = 0;
				WritePDEntry(page_table_entry, pd_entry_value);
				/* for pt */
				GetPageNode(tmp_offset, MMU_PT, &page_node, mmu_po);
				page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
				if (!page_node->use_count) {
					tmp_entry = (unsigned int *)page_node->page_table_entry;
					/* release used pt node here */
					RemovePageNode(page_node, p->process_id);
					WritePDEntry(tmp_entry, pd_entry_value);
					/* for pd0 */
					tmp_offset = (address >> MMU_PD1_SHIFT) & mmu_page_bits[MMU_PD1];
					GetPageNode(tmp_offset, MMU_PD0, &page_node, mmu_po);
					page_node->use_count = (page_node->use_count > 0) ? (page_node->use_count - 1) : 0;
					if (!page_node->use_count) {
						tmp_entry = (unsigned int *)page_node->page_table_entry;
						/* release used page node here */
						RemovePageNode(page_node, p->process_id);
						WritePDEntry(tmp_entry, pd_entry_value);
					}
				}
#else
				GetPageEntry(p, &page_table_entry, mmu_po, i, MMU_NONE);

				address = 0;
				WritePageEntry(page_table_entry, address);
#endif
			}
			break;
		}
		p = p->next;
	}
	if (!p)
		goto onerror;

	if (simple_map)
		SMRemoveKernelNode(e, addr->bus_address, mmu_po);
	else
		RemoveKernelNode(e, addr->bus_address, mmu_po);

	ReleaseMutex(g_mmu->page_table_mutex);
	return MMU_STATUS_OK;

onerror:
	if (mutex)
		ReleaseMutex(g_mmu->page_table_mutex);
	MMUDEBUG(" *****MMU Unmap Error*****\n");
	return status;
}

static long MMUCtlBufferMap(struct file *filp, unsigned long arg)
{
	struct addr_desc addr;
	long tmp;

	tmp = copy_from_user(&addr, (void __user *)arg, sizeof(struct addr_desc));
	if (tmp) {
		MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
		return -MMU_EFAULT;
	}

	MMUMemNodeMap(&addr, filp);

	tmp = copy_to_user((void __user *)arg, &addr, sizeof(struct addr_desc));
	if (tmp) {
		MMUDEBUG("copy_to_user failed, returned %li\n", tmp);
		return -MMU_EFAULT;
	}
	return 0;
}

static long MMUCtlBufferUnmap(unsigned long arg)
{
	struct addr_desc addr;
	long tmp;

	tmp = copy_from_user(&addr, (void __user *)arg, sizeof(struct addr_desc));
	if (tmp) {
		MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
		return -MMU_EFAULT;
	}

	MMUMemNodeUnmap(&addr);
	return 0;
}

static long MMUCtlFlush(unsigned long arg,
			volatile unsigned char *hwregs[HXDEC_MAX_CORES][2])
{
	unsigned int core_id;
	long tmp;

	tmp = copy_from_user(&core_id, (void __user *)arg, sizeof(unsigned int));
	if (tmp) {
		MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
		return -MMU_EFAULT;
	}

	MMUFlush(core_id, hwregs);

	return 0;
}

static long MMUCtlSwitchPageTable(struct file *filp, unsigned long arg,
			volatile unsigned char *hwregs[HXDEC_MAX_CORES][2])
{
	long tmp = 1;
#ifdef MMU_PAGE_TABLE_SWITCH
	unsigned int core_id;

	tmp = copy_from_user(&core_id, (void __user *)arg,
			     sizeof(unsigned int));
	if (tmp) {
		MMUDEBUG("copy_from_user failed, returned %li\n", tmp);
		return -MMU_EFAULT;
	}

	MMUSwitchPageTable(filp, core_id, hwregs);
#endif

	return tmp;
}

static long MMUCtlSwitchPageTableByCmdBuf(struct file *filp, unsigned long arg)
{
	long tmp = 1;
#ifdef MMU_PAGE_TABLE_SWITCH
	struct page_table_switch params;

	MMUSwitchPageTableByCmdBuf(filp, &params);

	tmp = copy_to_user((void __user *)arg, &params,
		sizeof(struct page_table_switch));
	if (tmp) {
		MMUDEBUG("copy_to_user failed, returned %li\n", tmp);
		return -MMU_EFAULT;
	}
#endif

	return tmp;
}

long MMUIoctl(unsigned int cmd, void *filp, unsigned long arg,
	      volatile unsigned char *hwregs[HXDEC_MAX_CORES][2])
{
	int i;

	for (i = 0; i < HXDEC_MAX_CORES; i++) {
		if (hwregs[i][0])
			MMUDEBUG("mmu_hwregs[%d][0]=%p", i, hwregs[i][0]);
		if (hwregs[i][1])
			MMUDEBUG("mmu_hwregs[%d][1]=%p", i, hwregs[i][1]);
	}

	switch (cmd) {
	case HANTRO_IOCS_MMU_MEM_MAP: {
		return MMUCtlBufferMap((struct file *)filp, arg);
	}
	case HANTRO_IOCS_MMU_MEM_UNMAP: {
		return MMUCtlBufferUnmap(arg);
	}
	case HANTRO_IOCS_MMU_FLUSH: {
		return MMUCtlFlush(arg, hwregs);
	}
	case HANTRO_IOCS_MMU_SWITCH_PAGETABLE: {
		return MMUCtlSwitchPageTable((struct file *)filp, arg, hwregs);
	}
	case HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF: {
		return MMUCtlSwitchPageTableByCmdBuf((struct file *)filp, arg);
	}
	default:
		return -MMU_ENOTTY;
	}
}

addr64_t GetMMUAddress(void)
{
	addr64_t address = 0;
	unsigned int addr = 0, addr_msb = 0;

	MMUGetPageTableArrayAddr(g_mmu->mmu_po, &addr, &addr_msb);
	address = addr | (addr64_t)addr_msb << 32;

	return address;
}

unsigned int GetMMUPageTableArraySize(void)
{
	return g_mmu->page_table_array_size;
}

#if 0
unsigned int GetMMUPageTableId(void)
{
	int process_id = GetProcessID();
	struct MMUProcessObject *mmu_po = NULL;

	mmu_po = MMUGetProcessObject(process_id);
	if (!mmu_po)
		return -1;

	return mmo_po->desc_id;
}
#endif
