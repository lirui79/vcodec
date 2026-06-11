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

#ifndef _VCX_DRIVER_H_
#define _VCX_DRIVER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vcx_defs.h"
#include "vcx_type.h"
#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */
#include <linux/types.h>

#ifdef SUPPORT_MMU
#include "vcx_mmu.h"
#endif

#ifdef SUPPORT_AXIFE
#include "vcx_axife.h"
#endif

/*
 * Macros to help debugging
 */

#undef PDEBUG /* undef it, just in case */
#ifdef HANTRO_DRIVER_DEBUG
#ifdef __KERNEL__
/* This one if debugging is on, and kernel space */
#define PDEBUG(fmt, args...) pr_info("vcx: " fmt, ##args)
#else
/* This one for user space */
#define PDEBUG(fmt, args...) printf(__FILE__ ":%d: " fmt, __LINE__, ##args)
#endif
#else
#define PDEBUG(fmt, ...) /* not debugging: nothing */
#endif


#define ENC_HW_ID1                  0x48320100
#define ENC_HW_ID2                  0x90004200
#define CORE_INFO_MODE_OFFSET       31
#define CORE_INFO_AMOUNT_OFFSET     28

/* Use 'k' as magic number */
#define HANTRO_IOC_MAGIC 'k'

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": G and S atomically
 * H means "sHift": T and Q atomically
 */

#define HANTRO_IOCG_HWOFFSET                           _IOR(HANTRO_IOC_MAGIC,  3, unsigned long *)
#define HANTRO_IOCG_HWIOSIZE                           _IOR(HANTRO_IOC_MAGIC,  4, unsigned int *)
#define HANTRO_IOC_CLI                                 _IO(HANTRO_IOC_MAGIC,  5)
#define HANTRO_IOC_STI                                 _IO(HANTRO_IOC_MAGIC,  6)
#define HANTRO_IOCX_VIRT2BUS                           _IOWR(HANTRO_IOC_MAGIC,  7, unsigned long *)
#define HANTRO_IOCH_ARDRESET                           _IO(HANTRO_IOC_MAGIC, 8)   /* debugging tool */
#define HANTRO_IOCG_SRAMOFFSET                         _IOR(HANTRO_IOC_MAGIC,  9, unsigned long *)
#define HANTRO_IOCG_SRAMEIOSIZE                        _IOR(HANTRO_IOC_MAGIC,  10, unsigned int *)
#define HANTRO_IOCH_ENC_RESERVE                        _IOR(HANTRO_IOC_MAGIC, 11, unsigned int *)
#define HANTRO_IOCH_ENC_RELEASE                        _IOR(HANTRO_IOC_MAGIC, 12, unsigned int *)
#define HANTRO_IOCG_CORE_NUM                           _IOR(HANTRO_IOC_MAGIC, 13, unsigned int *)
#define HANTRO_IOCG_CORE_INFO                          _IOR(HANTRO_IOC_MAGIC, 14, SUBSYS_CORE_INFO *)
#define HANTRO_IOCG_CORE_WAIT                          _IOR(HANTRO_IOC_MAGIC, 15, unsigned int *)
#define HANTRO_IOCG_ANYCORE_WAIT                       _IOR(HANTRO_IOC_MAGIC, 16, CORE_WAIT_OUT *)
#define HANTRO_IOCG_ANYCORE_WAIT_POLLING               _IOR(HANTRO_IOC_MAGIC, 17, CORE_WAIT_OUT *)
#define HANTRO_IOCG_ENABLE_CORE                        _IOR(HANTRO_IOC_MAGIC, 18, unsigned int *)

#define HANTRO_IOCH_GET_CMDBUF_PARAMETER               _IOWR(HANTRO_IOC_MAGIC, 25, struct cmdbuf_mem_parameter *)
#define HANTRO_IOCH_GET_CMDBUF_POOL_SIZE               _IOWR(HANTRO_IOC_MAGIC, 26, unsigned long)
#define HANTRO_IOCH_SET_CMDBUF_POOL_BASE               _IOWR(HANTRO_IOC_MAGIC, 27, unsigned long)
#define HANTRO_IOCH_GET_VCMD_PARAMETER                 _IOWR(HANTRO_IOC_MAGIC, 28, struct config_parameter *)
#define HANTRO_IOCH_RESERVE_CMDBUF                     _IOWR(HANTRO_IOC_MAGIC, 29, struct exchange_parameter *)
#define HANTRO_IOCH_LINK_RUN_CMDBUF                    _IOR(HANTRO_IOC_MAGIC, 30, u16 *)
#define HANTRO_IOCH_WAIT_CMDBUF                        _IOR(HANTRO_IOC_MAGIC, 31, u16 *)
#define HANTRO_IOCH_RELEASE_CMDBUF                     _IOR(HANTRO_IOC_MAGIC, 32, u16 *)
#define HANTRO_IOCH_POLLING_CMDBUF                     _IOR(HANTRO_IOC_MAGIC, 33, u16 *)

#define HANTRO_IOCH_GET_VCMD_ENABLE                    _IOWR(HANTRO_IOC_MAGIC, 50, unsigned long)
#define HANTRO_IOCH_GET_MMU_ENABLE                     _IOWR(HANTRO_IOC_MAGIC, 51, unsigned long)
#define HANTRO_IOCH_GET_PM_SUPPORT                     _IOWR(HANTRO_IOC_MAGIC, 52, unsigned long)
#define HANTRO_IOCH_WRITE_CORE_REGS                    _IOW(HANTRO_IOC_MAGIC, 53, struct core_regs_wr *)

#define GET_ENCODER_IDX(type_info)                    (CORE_VCE)
#define CORETYPE(core)                                (1 << (core))
#define HANTRO_IOC_MAXNR                              60


struct cmdbuf_mem_parameter {
	u32 *cmd_virt_addr; //cmdbuf pool base virtual address
	ptr_t cmd_phy_addr; //cmdbuf pool base physical address, it's for cpu
	ptr_t cmd_hw_addr; //cmdbuf pool base hardware address, it's for hardware ip
	u32 cmd_total_size; //cmdbuf pool total size in bytes.
	u16 cmd_unit_size; //one cmdbuf size in bytes. all cmdbuf have same size.
	u32 *status_virt_addr;
	ptr_t status_phy_addr; //status cmdbuf pool base physical address, it's for cpu
	ptr_t status_hw_addr; //status cmdbuf pool base hardware address, it's for hardware ip
	u32 status_total_size; //status cmdbuf pool total size in bytes.
	u16 status_unit_size; //one status cmdbuf size in bytes. all status cmdbuf have same size.
	ptr_t base_ddr_addr; //for pcie interface, hw can only access phy_cmdbuf_addr-pcie_base_ddr_addr.
		//for other interface, this value should be 0?
	u32 *reg_virt_addr; //register cmdbuf pool base virtual address
	ptr_t reg_phy_addr; //register cmdbuf pool base physical address, it's for cpu
	ptr_t reg_hw_addr; //register cmdbuf pool base hardware address, it's for hardware ip
	u32 reg_total_size; //register cmdbuf pool total size in bytes.
	u32 reg_unit_size; //one reg cmdbuf size in bytes. all status cmdbuf have same size.
};

struct config_parameter {
	u16 module_type; //input vce=0,cutree=1,vcd=2，jpege=3, jpegd=4
	u16 vcmd_core_num; //output, how many vcmd cores are there with corresponding module_type.
	u16 submodule_main_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_main_addr; //output, main ip offset instatus buffer.
	u16 submodule_dec400_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_dec400_addr; //output, dec400 ip offset instatus buffer.
	u16 submodule_L2Cache_addr; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_L2Cache_addr; //output, L2Cache ip offset instatus buffer.

	u16 submodule_MMU_addr[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_MMU_addr[2]; //output, MMU ip offset instatus buffer.
	u16 submodule_axife_addr[2]; //output,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_axife_addr[2]; //output, axife ip offset instatus buffer.
	u16 submodule_ufbc_addr; //output ,if submodule addr == 0xffff, this submodule does not exist.
	u16 status_ufbc_addr; //output, ufbc ip offset instatus buffer.
	u32 vcmd_hw_version_id;
	u32 vcmd_priority[MAX_VCMD_CORE_NUM]; //specify the priority of vcmd
};

/*need to consider how many memory should be allocated for status.*/
struct exchange_parameter {
	/** control interrupt mode when generate JMP command
	 * bit31 is mode_flag.
	 *	- when mode_flag is 0, adapative interrupt mode is selected. in such
	 *	  mode, bit[30:0] is executing time estimated for current job;
	 *	- when mode_flag is 1, manual interrupt mode is selected. in such mode,
	 *	  bit[0] is used to set IE flag in JMP command.
	 *	  bit[39:32] is batch count.
	 */
	u64 interrupt_ctrl; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	u16 module_type; //input input vce=0,IM=1,vcd=2，jpege=3, jpegd=4
	u16 cmdbuf_size; //input, reserve is not used; link and run is input.
	u16 cmdbuf_id; //output ,it is unique in driver.
	u16 core_id; //just used for polling.
	u16 core_mask; //core_mask for user to select cores
	/* input, bit[0]: priority    - normal=0, high/live=1
	 *        bit[1]: has_end_cmd - last cmd is JMP (0) or END (1) command
	 */
	u16 input_mask;
};

#define CORE_REGS_WR_NUM    16
struct core_regs_wr {
	u32 id;         /* id of core to be written */
	u32 type;      /* type of core to be written */
	u32 reg_num;      /* num of register to be written */
	u32 reg_id;    /* start id of reigster to be written */
	u32 reg_val[CORE_REGS_WR_NUM]; /* value of reigster to be written */
};

typedef struct CoreWaitOut {
	u32 job_id[4];
	u32 irq_status[4];
	u32 irq_num;
} CORE_WAIT_OUT;

typedef struct {
	u32 type_info; //indicate which IP is contained in this subsystem and each uses one bit of this variable
	unsigned long offset[CORE_MAX];
	unsigned long regSize[CORE_MAX];
	int irq[CORE_MAX];
} SUBSYS_CORE_INFO;


#ifdef __cplusplus
}
#endif

#endif /* !_VCX_DRIVER_H_ */
