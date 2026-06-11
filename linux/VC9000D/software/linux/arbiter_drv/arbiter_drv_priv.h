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

#ifndef _ARBITER_DRV_PRIVE_H_
#define _ARBITER_DRV_PRIVE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "arbiter_drv.h"
#include "arbiter_cfg.h"

#define ARB_WORK_STATE_IDLE    0
#define ARB_WORK_STATE_ARBIT   1
#define ARB_WORK_STATE_ACK     2
#define ARB_WORK_STATE_SORT    3
#define ARB_WORK_STATE_STALL   4

struct sub_module_info {
	addr_t reg_base;
	u32 io_size;
	volatile u8 *hwregs;
};

struct master_config {
	u32 enable;
	u32 weight;
	u32 urgent;
	u32 bw_overflow;
};

struct arbiter_dev {
	u32 hw_version_id;    /* arbiter HW version */
	u32 arb_id;          /* arbiter core id for driver and sw internal use */
	addr_t reg_base;      /* physical reg base address of arbiter */
	u32 io_size;          /* reg size if arbiter */
	u32 time_window_exp;  /* the bandwidth calculation time window for arbiter */
	u32 enable;           /* arbiter enable */
	int irq;              /* arbiter irq number */
	volatile u8 *hwregs;  /* registers IO mem base */
	u32 reg_mirror[ASIC_ARB_SWREG_AMOUNT]; /* a array for storing reg val */

	u32 irq_status;                    /* arbiter interrupt status */

	spinlock_t spinlock;            /* device spinlock  */

	struct master_config *master_cfg; /* master config params */
	u32 master_num;                   /* master num */

	struct sub_module_info mod_info[SUB_MOD_MAX]; /* sub-module infomarion */

	u8 reset_flag;    // a flag to indicate that if need reset
};

typedef struct {
	u32 arb_num;                              /* supported arbiter num */
	struct arbiter_dev *arb_dev;              /* arbiter device */
	struct arbiter_config *cfg[MAX_ARBITER_NUM]; /* restore the valid arbiter cfg */

	struct pci_dev *pcie_dev;       /* pcie handler */
	/* the offset of arbiter reg base, if enable pcie, it is the start address of pcie H2 BAR */
	unsigned long reg_base_offset;
	int arb_dev_major;              /* use 0 for dynamic allocation (recommended) */

	wait_queue_head_t irq_check_waitq; /* a wait queue for checking arbiter irq */
	u32 waitq_timeout;                 /* 0: keep waiting: others: timeout time for waitq */

	// kernel thread related
	struct task_struct *kthread;     /* kernel thread task struct */
	u8 stop_kthread;                 /* a flag to indicate that if stop kernel thread */
	wait_queue_head_t kthread_waitq; /* a wait queuq for blocking or wake up kernel thread */

} arbiter_mgr_t;


#ifdef __cplusplus
}
#endif
#endif // _ARBITER_DRV_PRIVE_H_