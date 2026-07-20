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

#include "vcx_cfg.h"
#include <linux/fs.h>

#ifdef __cplusplus
extern "C" {
#endif


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


typedef unsigned long long addr64_t;

struct addr_desc {
	void *virtual_address; /* buffer virtual address */
	addr64_t bus_address; /* buffer physical address */
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
#define HANTRO_IOC_MMU_MAXNR 6
#define MAX_SUBSYS_NUM 4 /* up to 4 subsystem (temporary) */
#define HXDEC_MAX_CORES MAX_SUBSYS_NUM /* used in hantro_dec.c */


#ifdef __cplusplus
}
#endif

#endif
