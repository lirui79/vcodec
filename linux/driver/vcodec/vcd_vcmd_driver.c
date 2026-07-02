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
 */

#include "hantrodec_defs.h"
#include <linux/kernel.h>
#include <linux/module.h>
/* needed for __init,__exit directives */
#include <linux/init.h>
/* needed for remap_page_range
 * SetPageReserved
 * ClearPageReserved
 */
#include <linux/mm.h>
/* obviously, for kmalloc */
#include <linux/slab.h>
/* for struct file_operations, register_chrdev() */
#include <linux/fs.h>
/* standard error codes */
#include <linux/errno.h>

#include <linux/moduleparam.h>
/* request_irq(), free_irq() */
#include <linux/interrupt.h>
#include <linux/sched.h>

#include <linux/semaphore.h>
#include <linux/spinlock.h>
/* needed for virt_to_phys() */
#include <asm/io.h>
#ifdef PCIE_EN
#include <linux/pci.h>
#endif
#include <linux/uaccess.h>
#include <linux/ioport.h>

#include <asm/irq.h>

#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>

/* our own stuff */

#include "cmda78_msg.h"
#include "cmda78_mgr.h"
#include "vcd_priv.h"
#include "hantrovcmd.h"
#include "vcx_kthread.h"
#include "vcx_vcmd_priv.h"
#include "vcd_vcmd.h"
#ifdef SUPPORT_AXIFE
#include "vcx_axife.h"
#endif
#include "vcx_vcmd_dbgfs.h"
#include "vcd_vcmd_cfg.h"
#ifdef AXI2TO1_SUPPORT
#include "vcx_axi2to1.h"
#endif

#include "hantrodec.h"
#include "vcx_mmu_priv.h"
#include "vcx_vcmd.h"

/*---------------------------------------------------------------
 * Macros related to sub-system config
 *---------------------------------------------------------------
 */
/* VCMD master_out_clk mode */
#define APB_CLK_ON	(1)
#define APB_CLK_OFF	(0)
#define APB_CLK_MODE	APB_CLK_ON

/*---------------------------------------------------------------
 * Macros related to debug
 *---------------------------------------------------------------
 */

//#define VCMD_DEBUG_INTERNAL

/*---------------------------------------------------------------
 * Macros related to dev/process workload management
 *---------------------------------------------------------------
 */
#define VCMD_WORKLOAD_UNIT   (8192L * 4096L)
#define VCMD_INTR_INTERVAL   (VCMD_WORKLOAD_UNIT * 8)
#define PROCESS_MAX_WORKLOAD (VCMD_WORKLOAD_UNIT * 64)

/****************************************************************
 * external/global variables declarations
 ***************************************************************/
/* PCI base register address (memalloc) */
static vcmd_mgr_t *vcmd_manager;

#ifdef SUPPORT_MMU
extern unsigned int mmu_enable;
#ifdef PCIE_EN
extern unsigned long gBaseDDRHw;
#endif
#endif

/****************************************************************
 * local functions declarations
 ***************************************************************/
static struct cmd_jmp_t *_get_jmp_cmd(struct cmdbuf_obj *obj);
static struct cmd_end_t *_get_end_cmd(struct cmdbuf_obj *obj);

/****************************************************************
 * watchdog, irq_simulation, kernel thread and debug functions
 ***************************************************************/

#ifdef VCMD_DEBUG_INTERNAL
static void _dbg_log_last_cmd(struct cmdbuf_obj *obj)
{
	u32 *p, len, offset, size;
	char log_buf[512];

	vcmd_klog(LOGLVL_FLOW, "last cmdbuf content:\n");
	if (obj->has_jmp_cmd) {
		p = (void *)_get_jmp_cmd(obj);
		len = sizeof(struct cmd_jmp_t) / sizeof(u32);
	} else {
		p = (void *)_get_end_cmd(obj);
		len = sizeof(struct cmd_end_t) / sizeof(u32);
	}

	offset = p - obj->cmd_va;
	_dbg_log_instr(offset, *p, &size, log_buf);
	vcmd_klog(LOGLVL_FLOW, "%s", log_buf);
	len--;
	offset++;

	while (len--) {
		vcmd_klog(LOGLVL_FLOW, "current cmdbuf data %d = 0x%x\n",
						offset, *(obj->cmd_va + offset));
		offset++;
	}
}

static void _dbg_log_cmdbuf(struct cmdbuf_obj *obj)
{
	u32 i, instr = 0, size = 0;
	char log_buf[512];

	vcmd_klog(LOGLVL_FLOW, "vcmd link, current cmdbuf content\n");
	for (i = 0; i < obj->cmdbuf_size / 4; i++) {
		if (i == instr) {
			//memset(log_buf, 0, sizeof(log_buf));
			_dbg_log_instr(i, *(obj->cmd_va + i), &size, log_buf);
			vcmd_klog(LOGLVL_FLOW, "%s", log_buf);
			instr += size;
		} else {
			vcmd_klog(LOGLVL_FLOW, "current cmdbuf data %d = 0x%x\n",
					i, *(obj->cmd_va + i));
		}
	}
}

static void _dbg_log_dev_regs(struct hantrovcmd_dev *dev, u32 dump)
{
	u32 i, reg_val;

	if (dump) {
		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			reg_val = vcmd_read_reg((const void *)dev->hwregs, i * 4);
			vcmd_klog(LOGLVL_FLOW, "vcmd swreg%d: 0x%x\n", i, reg_val);
		}
	} else {
		for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
			reg_val = *(dev->reg_mem_va + i);
			vcmd_klog(LOGLVL_FLOW, "ddr vcmd swreg%d: 0x%x\n", i, reg_val);
		}
	}
}
#endif

static void printk_vcmd_register_debug(const void *hwregs, char *info)
{
#ifdef VCMD_DEBUG_INTERNAL
	u32 i, fordebug;

	for (i = 0; i < ASIC_VCMD_SWREG_AMOUNT; i++) {
		fordebug = vcmd_read_reg((const void *)hwregs, i * 4);
		vcmd_klog(LOGLVL_FLOW, "%s vcmd register %d:0x%x\n", info, i,
			fordebug);
	}
#endif
}

static void _log_ioctl_cmd(unsigned int cmd)
{
	static unsigned int last_cmd;

	if ((cmd == HANTRO_VCMD_IOCH_POLLING_CMDBUF) && (last_cmd == cmd))
		return;

	last_cmd = cmd;
	vcmd_klog(LOGLVL_FLOW, "ioctl cmd 0x%08x\n", cmd);
}

/*======================= cmdbuf object management ================*/

/**
 * @brief initialize all of cmdbuf objs of vcmd driver handler.
 */
static void vcmd_init_objs(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct cmdbuf_obj *obj;
	struct noncache_mem *m0, *m1;

	m0 = &vcmd_mgr->mem_vcmd;
	m1 = &vcmd_mgr->mem_status;
	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		obj = &vcmd_mgr->objs[i];
		obj->cmdbuf_id = i;
		obj->cmdbuf_size = SLOT_SIZE_CMDBUF;
		obj->cmd_va = m0->va + CMDBUF_OFF_32(i);
		obj->cmd_pa = m0->pa + CMDBUF_OFF(i);
		obj->mmu_cmd_ba = m0->mmu_ba + CMDBUF_OFF(i);

		obj->status_size = SLOT_SIZE_STATUSBUF;
		obj->status_va = m1->va + STATUSBUF_OFF_32(i);
		obj->status_pa = m1->pa + STATUSBUF_OFF(i);
		obj->mmu_status_ba = m1->mmu_ba + STATUSBUF_OFF(i);
	}
}

/**
 * @brief initialize all of cmdbuf nodes of vcmd driver handler.
 */
static void vcmd_init_nodes(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;

	for (i = 0; i < SLOT_NUM_CMDBUF; i++)
		vcmd_mgr->nodes[i].data = (void *)&vcmd_mgr->objs[i];

}

/**
 * @brief reset flag/status of cmdbuf obj specified by id.
 */
static void reset_cmdbuf_obj(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct cmdbuf_obj *obj = &vcmd_mgr->objs[id];

	obj->executing_status = CMDBUF_EXE_STATUS_OK;
	obj->cmdbuf_run_done = 0;
	obj->slice_run_done = 0;
	obj->cmdbuf_linked = 0;
	obj->cmdbuf_need_remove = 0;
	obj->core_id = 0xFFFF;
	obj->cmdbuf_size = SLOT_SIZE_CMDBUF;

	obj->has_jmp_cmd = 1;
	obj->jmp_ie = 0;
}

/**
 * @brief reset pointer of cmdbuf node specified by id.
 */
static void reset_cmdbuf_node(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct bi_list_node  *node;

	node = &vcmd_mgr->nodes[id];
	node->next = NULL;
	node->prev = NULL;

	node = &vcmd_mgr->po_jobs[id];
	node->next = NULL;
	node->prev = NULL;
	node->data = NULL;
}

/**
 * @brief put a node to tail of si-list.
 */
static void _si_list_put(struct si_linked_list *list,
							struct si_linked_node *node)
{
	if (list->head == NULL) {
		list->head = list->tail = node;
	} else {
		list->tail->next = node;
		list->tail = node;
	}
	node->next = NULL;
}

/**
 * @brief get & remove a node from head of si-list.
 * @return struct si_linked_node *: NULL: no node got; other: the node.
 */
static struct si_linked_node *_si_list_get(struct si_linked_list *list)
{
	struct si_linked_node *node = list->head;

	if (list->head) {
		list->head = node->next;
		if (list->head == NULL)
			list->tail = NULL;
	}

	return node;
}

/**
 * @brief init a si-list.
 */
static void vcmd_init_si_list(struct si_linked_list *list)
{

	spin_lock_init(&list->spinlock);
	list->head = NULL;
	list->tail = NULL;
}

/**
 * @brief init free obj (cmdbuf) list of vcmd driver handler.
 */
static void vcmd_init_free_obj_list(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct si_linked_list *list = &vcmd_mgr->free_obj_list;

	vcmd_init_si_list(list);
	sema_init(&vcmd_mgr->free_obj_sema, SLOT_NUM_CMDBUF);

	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		list->nodes[i].data = (void *)&vcmd_mgr->objs[i];
		_si_list_put(list, &list->nodes[i]);
	}
}

/**
 * @brief acquire a free cmdbuf.
 * @param u32 *id: to store acquired free cmdbuf id.
 * @return int: 0: succeed; otherwise: failed.
 */
static int acquire_cmdbuf(vcmd_mgr_t *vcmd_mgr, u32 *id)
{
	struct si_linked_list *list = &vcmd_mgr->free_obj_list;
	struct si_linked_node *node;
	struct cmdbuf_obj *obj;

	if (down_interruptible(&vcmd_mgr->free_obj_sema)) {
		vcmd_klog(LOGLVL_ERROR, "%s: sema-down is interrupted!", __func__);
		return -1;
	}

	spin_lock(&list->spinlock);
	node = _si_list_get(list);
	spin_unlock(&list->spinlock);

	if (node == NULL) {
		vcmd_klog(LOGLVL_ERROR, "%s: get NULL node!", __func__);
		return -1;
	}

	obj = (struct cmdbuf_obj *)node->data;
	*id = obj->cmdbuf_id;
	return 0;
}

/**
 * @brief return (release) a cmdbuf specified by id.
 */
static void return_cmdbuf(vcmd_mgr_t *vcmd_mgr, u32 id)
{
	struct si_linked_list *list = &vcmd_mgr->free_obj_list;
	struct si_linked_node *node = &list->nodes[id];
	struct cmdbuf_obj *obj = (struct cmdbuf_obj *)node->data;

	obj->po = NULL;

	spin_lock(&list->spinlock);
	_si_list_put(list, node);
	spin_unlock(&list->spinlock);
	up(&vcmd_mgr->free_obj_sema);
}

/**
 * @brief get the va of vcmdbuf obj's JMP cmd.
 */
static struct cmd_jmp_t *_get_jmp_cmd(struct cmdbuf_obj *obj)
{
	u8 *p = (u8 *)obj->cmd_va;

	p += obj->cmdbuf_size - sizeof(struct cmd_jmp_t);
	return (struct cmd_jmp_t *)p;
}

/**
 * @brief get the va of vcmdbuf obj's END cmd.
 */
static struct cmd_end_t *_get_end_cmd(struct cmdbuf_obj *obj)
{
	u8 *p = (u8 *)obj->cmd_va;

	p += obj->cmdbuf_size - sizeof(struct cmd_end_t);
	return (struct cmd_end_t *)p;
}

/**
 * @brief create a process object
 */
static struct proc_obj *create_process_object(void)
{
	struct proc_obj *po;

	po = vmalloc(sizeof(struct proc_obj));
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: vmalloc failed!\n", __func__);
		return NULL;
	}

	memset(po, 0, sizeof(struct proc_obj));
	spin_lock_init(&po->spinlock);
	init_waitqueue_head(&po->resource_waitq);
	init_waitqueue_head(&po->job_waitq);

	spin_lock_init(&po->job_lock);
	init_bi_list(&po->job_done_list);
	return po;
}

/**
 * @brief free a process object
 */
static void free_process_object(struct proc_obj *po)
{
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: po is NULL!\n", __func__);
		return;
	}
	vfree(po);
}

/**
 * @brief add done obj to job_done_list of its po, wake-up job_waitq if needed.
 */
void vcd_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj)
{
	u16 id = obj->cmdbuf_id;
	struct proc_obj *po;
	struct bi_list *list;
	unsigned long flags;
	u32 is_empty, is_wait;
	struct hantrovcmd_dev *dev;

	if (!obj->po) {
		vcmd_klog(LOGLVL_BRIEF, "%s: the po and cmdbufs of this po has been released!\n",
						__func__);
		return;
	}

	po = obj->po;
	list = &po->job_done_list;

	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	wake_up_interruptible_all(&dev->buff_empty_waitq);

	spin_lock_irqsave(&po->job_lock, flags);

	if (vcmd_mgr->po_jobs[id].data) {
		//already in job-done list, do nothing
		spin_unlock_irqrestore(&po->job_lock, flags);
		return;
	}

	vcmd_mgr->po_jobs[id].data = (void *)&vcmd_mgr->objs[id];
	is_empty = (list->head == NULL);
	is_wait = po->in_wait;
	po->in_wait = 0;
	bi_list_insert_node_tail(list, &vcmd_mgr->po_jobs[id]);

	spin_unlock_irqrestore(&po->job_lock, flags);

	if (is_empty || is_wait)
		wake_up_interruptible_all(&po->job_waitq);
}

/**
 * @brief get & remove a done obj from po's job_done_list.
 * @param struct cmdbuf_obj **pobj: *pobj==NULL: get head node from list.
 *									otherwise, get specified node from list.
 * @return int: 0: no done obj; 1: obj is done.
 */
static int proc_get_done_job(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
									struct cmdbuf_obj **pobj)
{

	struct bi_list *list = &po->job_done_list;
	bi_list_node *node;
	unsigned long flags;
	struct cmdbuf_obj *obj = NULL;
	int is_done = 0, i;

	spin_lock_irqsave(&po->job_lock, flags);
	node = list->head;

	if (node == NULL) {
		// job done list is empty
		po->in_wait = 1;
		for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
			obj = &vcmd_mgr->objs[i];
			if (obj->po == po)
				break;
		}
		if (i == SLOT_NUM_CMDBUF) {
			spin_unlock_irqrestore(&po->job_lock, flags);
			return 1; /* used to drop_cmd_ids!=NULL when seek */
		}
		spin_unlock_irqrestore(&po->job_lock, flags);

		return 0;
	}

	if (*pobj == NULL) {
		//any po's cmdbuf ready, return head of job_done_list
		*pobj = (struct cmdbuf_obj *)node->data;
		is_done = 1;
	} else {
		//specified cmdbuf ready?
		obj = *pobj;
		if (obj->cmdbuf_run_done || obj->slice_run_done) {
			// check if the obj is in job done list
			while (node && node->data != (void *)obj)
				node = node->next;

			if (node == NULL) {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done, but not in done-list!\n",
						__func__, obj->cmdbuf_id);
			} else {
				vcmd_klog(LOGLVL_BRIEF, "%s: cmdbuf[%d] is done!\n", __func__, obj->cmdbuf_id);
				is_done = 1;
			}
		}
	}

	if (is_done) {
		bi_list_remove_node(list, &vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id]);
		vcmd_mgr->po_jobs[(*pobj)->cmdbuf_id].data = NULL;
	} else {
		po->in_wait = 1;
	}

	spin_unlock_irqrestore(&po->job_lock, flags);

	return is_done;
}


#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
/**
 * @brief set reg id/val of specified sub-ip to init-cmd regs.
 */
static void _set_module_init_cmds(struct hantrovcmd_dev *dev, u32 module_id,
									struct sub_ip_init_cfg *module_cfg)
{
	u16 reg_off = dev->subsys_info->reg_off[module_id];
	u32 opcode = OPCODE_WREG | WREG_MODE(WREG_ADDR_FIX) | WREG_LEN(1);

	if (reg_off == 0xffff)
		return;

	while (module_cfg->reg_id != 0xffff) {
		dev->reg_mirror[dev->init_cmd_idx++] = opcode |
												(reg_off + module_cfg->reg_id);
		dev->reg_mirror[dev->init_cmd_idx++] = module_cfg->reg_val;
		module_cfg++;
	}
}
#endif

/**
 * @brief set init-cmd regs to init sub-ips if necessary.
 */
static void vcmd_set_init_cmds(struct hantrovcmd_dev *dev)
{
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 i;

	dev->init_cmd_idx = VCMD_REG_ID_SW_INIT_CMD0;

#ifdef SUPPORT_AXIFE
	//enable AXIFE by VCMD
	_set_module_init_cmds(dev, SUB_MOD_AXIFE, axife_init_cfg);
#endif

#ifdef AXI2TO1_SUPPORT
	_set_module_init_cmds(dev, SUB_MOD_AXI2TO1, axi2to1_init_cfg);
#endif

#ifdef SUPPORT_MMU
	//enable MMU by VCMD
	if (dev->mmu_enable) {
		u64 mmu_addr = GetMMUAddress();

		vcmd_klog(LOGLVL_FLOW, "%s: mmu address = 0x%llx", __func__, mmu_addr);
		mmu_init_cfg[0].reg_val = (u32)mmu_addr;
		mmu_init_cfg[1].reg_val = (u32)(mmu_addr >> 32);
#ifdef SUPPORT_48PA_MMU
		mmu_init_cfg[2].reg_val = GetMMUPageTableArraySize();
#endif
		_set_module_init_cmds(dev, SUB_MOD_MMU, mmu_init_cfg);
		_set_module_init_cmds(dev, SUB_MOD_MMU_WR, mmu_init_cfg);
	}
#endif //SUPPORT_MMU

	//finished with END command
	dev->reg_mirror[dev->init_cmd_idx++] = OPCODE_END;
	dev->reg_mirror[dev->init_cmd_idx++] = 0x00;

	for (i = VCMD_REG_ID_SW_INIT_CMD0; i < dev->init_cmd_idx; i++)
		vcmd_write_reg((const void *)dev->hwregs, i*4, dev->reg_mirror[i]);
#endif //HANTROVCMD_ENABLE_IP_SUPPORT
}

/**
 * @brief reset all vcmd hw devices.
 */
static void vcmd_reset_asic(vcmd_mgr_t *vcmd_mgr)
{
	int n;
	u32 status;
	struct hantrovcmd_dev *dev = vcmd_mgr->dev_ctx;

	for (n = 0; n < vcmd_mgr->subsys_num; n++) {
		if (dev[n].hwregs) {
			//disable interrupt at first
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_CTL_OFFSET, 0x0000);
			//reset core
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_CONTROL_OFFSET, 0x0004);
			//read status register
			status = vcmd_read_reg((const void *)dev[n].hwregs,
						   VCMD_REGISTER_INT_STATUS_OFFSET);
			//clean status register
			vcmd_write_reg((const void *)dev[n].hwregs,
					   VCMD_REGISTER_INT_STATUS_OFFSET, status);
			//when reset core need clear reg[3]
			vcmd_write_reg((const void *)dev[n].hwregs,
							VCMD_REGISTER_EXE_CMDBUF_COUNT_OFFSET, 0x0000);
		}
	}
}

#ifdef VCARB_REQUEST
/**
 * @brief vcmd get online for arbiter
 */
void vcmd_online_for_arbiter(const volatile u8 *hwregs)
{
	u32 arbiter_cfg;

	arbiter_cfg = ioread32((void __iomem *)(hwregs +
							VCMD_REGISTER_ARBITER_CONFIG_OFFSET));
	iowrite32((arbiter_cfg | VCMD_ARBITER_ENABLE),
			(void __iomem *)(hwregs +
			VCMD_REGISTER_ARBITER_CONFIG_OFFSET));
}

/**
 * @brief vcmd get offline for arbiter
 */
void vcmd_offline_for_arbiter(const volatile u8 *hwregs)
{
	// vcmd offline
	iowrite32(0, (void __iomem *)(hwregs +
			VCMD_REGISTER_ARBITER_CONFIG_OFFSET));
}

/**
 * @brief vcmd request arbiter manully
 */
void vcmd_request_arbiter(void *_vcmd_mgr, u32 subsys_id)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[subsys_id];
	volatile u8 *hwregs = dev->hwregs;

	if (dev->hw_feature.vcarb_ver == 0)
		return;
	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0)
		vcmd_online_for_arbiter(hwregs);

	iowrite32(0x0, (void __iomem *)(hwregs +
			VCMD_REGISTER_ARB_OFFSET));
	iowrite32(0x1, (void __iomem *)(hwregs +
			VCMD_REGISTER_ARB_OFFSET));
	while ((ioread32((void __iomem *)(hwregs +
		VCMD_REGISTER_ARB_OFFSET)) & 0x4) == 0)
		schedule();
}

/**
 * @brief release arbiter from keep serving vcmd
 */
void vcmd_release_arbiter(void *_vcmd_mgr, u32 subsys_id)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[subsys_id];
	volatile u8 *hwregs = dev->hwregs;

	if (dev->hw_feature.vcarb_ver == 0)
		return;
	if (dev->hw_feature.vcarb_ver == VCARB_VERSION_2_0)
		vcmd_offline_for_arbiter(hwregs);

	// write arb_fe to clear arb_req
	iowrite32(0x2, (void __iomem *)(hwregs +
			VCMD_REGISTER_ARB_OFFSET));
}
#endif // VCARB_REQUEST

/**
 * @brief acquire workload from process object.
 * @return int: 0: succeed; Others: failed.
 */
static int wait_process_resource_rdy(struct proc_obj *po)
{
	return po->total_workload <= PROCESS_MAX_WORKLOAD;
}

static int acquire_process_resource(struct proc_obj *po, u64 workload)
{
	spin_lock(&po->spinlock);
	po->total_workload += workload;
	spin_unlock(&po->spinlock);
	if (wait_event_interruptible(po->resource_waitq,
									wait_process_resource_rdy(po))) {
		vcmd_klog(LOGLVL_ERROR, "%s: wait event is interrupted!", __func__);
		return -1;
	}

	return 0;
}

/**
 * @brief return cmdbuf obj's workload to process object.
 */
static void return_process_resource(struct proc_obj *po,
									struct cmdbuf_obj *obj)
{
	if (po && obj->workload) {
		spin_lock(&po->spinlock);
		po->total_workload -= obj->workload;
		spin_unlock(&po->spinlock);
		obj->workload = 0;
		wake_up_interruptible_all(&po->resource_waitq);
	}
}

/**
 * @brief reserve a cmdbuf for specified process object.
 * @param struct exchange_parameter *param: the param of cmdbuf to reserve.
 * @return long: 0: succeed; oters: failed.
 */
static long reserve_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
			   struct exchange_parameter *param)
{
	struct cmdbuf_obj *obj;
	u32 cmdbuf_id = 0;

	if (param->cmdbuf_size > SLOT_SIZE_CMDBUF) {
		vcmd_klog(LOGLVL_ERROR, "%s size is larger than slot size !!\n", __func__);
		return -1;
	}

	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -1;
	}
	vcmd_klog(LOGLVL_FLOW, "reserve cmdbuf by filp %p\n", (void *)po->filp);

	if (acquire_process_resource(po, param->executing_time))
		return -1;

	if (acquire_cmdbuf(vcmd_mgr, &cmdbuf_id))
		return -ERESTARTSYS;

	reset_cmdbuf_obj(vcmd_mgr, cmdbuf_id);
	reset_cmdbuf_node(vcmd_mgr, cmdbuf_id);

	obj = &vcmd_mgr->objs[cmdbuf_id];
	obj->module_type = param->module_type;
	obj->priority = EXCH_G_BIT(param->input_mask, EXCH_PRIO_BIT);
	obj->workload = param->executing_time;
	obj->interrupt_ctrl = param->executing_time;
	obj->filp = po->filp;
	obj->po = po;
	obj->owner = param->owner;
	obj->core_mask = param->core_mask;

	param->cmdbuf_size = SLOT_SIZE_CMDBUF;
	param->cmdbuf_id = cmdbuf_id;
	vcmd_klog(LOGLVL_FLOW, "%s, filp[%p] reserved cmdbuf[%d]: obj %p, node %p of owner %p\n",
			__func__, (void *)obj->filp, cmdbuf_id, (void *)obj,
			(void *)&vcmd_mgr->nodes[cmdbuf_id], obj->owner);
#ifdef SUPPORT_DBGFS
	_dbgfs_record_reserved_time(vcmd_mgr->dev_ctx[0].dbgfs_info,
								param->cmdbuf_id);
#endif

	return 0;
}

/**
 * @brief release a specified cmdbuf.
 * @return long: 0: succeed; oters: failed.
 */
static long release_cmdbuf(vcmd_mgr_t *vcmd_mgr,
							struct proc_obj *po, u16 cmdbuf_id)
{
	struct cmdbuf_obj *obj = NULL;
	bi_list_node *curr_node = NULL;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;
	if (obj->po != po) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] po not match: owned by %p, released by %p!!\n",
						__func__, cmdbuf_id, obj->po, po);
		return -1;
	}

	obj->owner = NULL;
	obj->cmdbuf_need_remove = 1;
	obj = (struct cmdbuf_obj *)curr_node->data;
	return_process_resource(obj->po, obj);
	return_cmdbuf(vcmd_mgr, cmdbuf_id);
	return 0;
}

/**
 * @brief add/insert a cmdbuf (job) to suitable device,
 *  and start the device hw if it is not working.
 * @param struct exchange_parameter *param: the param of cmdbuf to link & run.
 * @return long: 0: succeed; oters: failed.
 */
static long link_and_run_cmdbuf(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
				struct exchange_parameter *param)
{
	struct cmdbuf_obj *obj;
	bi_list_node *curr_node;
	struct exchange_cmda78_param  cmd_param;
	u16 cmdbuf_id = param->cmdbuf_id;
	long retCode = 0;

	if (cmdbuf_id >= SLOT_NUM_CMDBUF) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf_id %d!!\n", __func__, cmdbuf_id);
		return -1;
	}

	curr_node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)curr_node->data;
	if (obj->po != po) {
		//should not happen
		vcmd_klog(LOGLVL_ERROR, "%s: cmdbuf[%d] po not match: owned by %p, released by %p!!\n",
						__func__, cmdbuf_id, obj->po, po);
		return -1;
	}

	obj->cmdbuf_size = param->cmdbuf_size;
	obj->workload    = param->executing_time;
#ifdef VCMD_DEBUG_INTERNAL
	_dbg_log_cmdbuf(obj);
#endif

	if (down_interruptible(&vcmd_mgr->module_mgr[obj->module_type].sem))
		return -ERESTARTSYS;
    cmd_param.owner = param->owner;
	cmd_param.cmdbuf_id = param->cmdbuf_id;
	cmd_param.core_mask = param->core_mask;
	cmd_param.cmdbuf_size = param->cmdbuf_size;
	cmd_param.vcmdmgr_id = VCMD_MGR_ID_DEC;
	cmd_param.input_mask = param->input_mask;
	cmd_param.interrupt_ctrl = param->executing_time;
	cmd_param.module_type = param->module_type;
	cmd_param.core_id = param->core_id;

	retCode = cmda78_gen_run_cmdbuf(po, &cmd_param);
	param->core_id = cmd_param.core_id;
	up(&vcmd_mgr->module_mgr[obj->module_type].sem);

	return retCode;
}

/**
 * @brief wait a cmdbuf runs done.
 * @param u16 cmdbuf_id: the id of cmdbuf to wait.
 * @param u16 *done_id: point to the id of done cmdbuf.
 * @return long: 0: succeed; oters: failed.
 */
static long wait_cmdbuf_ready(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po,
								u16 cmdbuf_id, u16 *done_id)
{
	struct cmdbuf_obj *obj = NULL;

	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -ERESTARTSYS;
	}

	if (cmdbuf_id != ANY_CMDBUF_ID) {
		vcmd_klog(LOGLVL_FLOW, "%s\n", __func__);
		obj = &vcmd_mgr->objs[cmdbuf_id];
		if (obj->po != po) {
			//should not happen
			vcmd_klog(LOGLVL_ERROR, "%s: ERROR cmdbuf filp not match!\n", __func__);
			return -1;
		}
	}

	if (wait_event_interruptible(po->job_waitq,
						proc_get_done_job(vcmd_mgr, po, &obj))) {
		vcmd_klog(LOGLVL_ERROR, "vcmd_wait_queue_0 interrupted\n");
		return -ERESTARTSYS;
	}

	if (obj == NULL)
		*done_id = ANY_CMDBUF_ID;
	else
		*done_id = obj->cmdbuf_id;

	return 0;
}

#ifndef PCIE_EN
/**
 * @brief allocate a memory pool, for non-PCIe platform.
 */
static int _vcmd_alloc_mem(vcmd_mgr_t *vcmd_mgr, struct noncache_mem *mem)
{
	dma_addr_t dma_handle = 0;

	/* command buffer */
	mem->va = (u32 *)dma_alloc_coherent(&vcmd_mgr->platformdev->dev,
							mem->size, &dma_handle,
							GFP_KERNEL | __GFP_DMA32);
	mem->pa = (unsigned long long)dma_handle;

	vcmd_klog(LOGLVL_FLOW, "Init[%s]: mem pa=0x%llx, va=0x%llx.\n", __func__,
			(unsigned long long)mem->pa, (unsigned long long)mem->va);
#ifdef SUPPORT_MMU
	if (vcmd_mgr->mmu_enable) {
		struct kernel_addr_desc mmu_addr;

		mmu_addr.bus_address = mem->pa;
		mmu_addr.size = mem->size;
		if (MMUKernelMemNodeMap(&mmu_addr, NULL) != MMU_STATUS_OK) {
			vcmd_klog(LOGLVL_ERROR, "Init[%s] mmu map mem failed\n", __func__);
			return -1;
		}
		mem->mmu_ba = mmu_addr.mmu_bus_address;
		vcmd_klog(LOGLVL_FLOW, "Init[%s]: mem->mmu_ba=0x%llx.\n", __func__,
				(unsigned long long)mem->mmu_ba);
	}
#endif
	return 0;
}

/**
 * @brief free the specified memory pool, for non-PCIe platform.
 */
static void _vcmd_free_mem(vcmd_mgr_t *vcmd_mgr, struct noncache_mem *mem)
{
	if (mem->va) {
#ifdef SUPPORT_MMU
		struct kernel_addr_desc mmu_addr;

		if (vcmd_mgr->mmu_enable && mem->mmu_ba) {
			mmu_addr.bus_address = mem->pa;
			mmu_addr.size = mem->size;
			MMUKernelMemNodeUnmap(&mmu_addr);
			mem->mmu_ba = 0;
		}
#endif
		dma_free_coherent(&vcmd_mgr->platformdev->dev,
							mem->size, mem->va, (dma_addr_t)mem->pa);
		mem->va = NULL;
	}
}
#endif

#ifdef PCIE_EN
/**
 * @brief clean-up reserved PCIe resources.
 */
static void _vcmd_pcie_cleanup(vcmd_mgr_t *vcmd_mgr)
{
#ifdef SUPPORT_MMU
	struct kernel_addr_desc mmu_addr;

	if (vcmd_mgr->mmu_enable && vcmd_mgr->pcie_pool.mmu_ba) {
		mmu_addr.bus_address = vcmd_mgr->pcie_pool.pa - vcmd_mgr->pa_trans_offset;
		mmu_addr.size = vcmd_mgr->pcie_pool.size;
		MMUKernelMemNodeUnmap(&mmu_addr);
		vcmd_mgr->pcie_pool.mmu_ba = 0;
	}
#endif
	if (vcmd_mgr->pcie_pool.va) {
		iounmap((void __iomem *)vcmd_mgr->pcie_pool.va);
		release_mem_region(vcmd_mgr->pcie_pool.pa, vcmd_mgr->pcie_pool.size);
		vcmd_mgr->pcie_pool.va = NULL;
	}
}
#endif

/**
 * @brief allocate memory pools.
 * @return int: 0: succeed; other: failed.
 */
static int vcmd_init(vcmd_mgr_t *vcmd_mgr)
{
	u8 *va;
#ifdef PCIE_EN
	struct noncache_mem *mem_pcie = &vcmd_mgr->pcie_pool;
	u32 offset;
	unsigned long pci_ddr_base = gBaseDDRHw;
#endif
#ifdef SUPPORT_MMU
	struct kernel_addr_desc mmu_addr;

	vcmd_mgr->mmu_enable = mmu_enable;
#endif
	vcmd_klog(LOGLVL_CONFIG, "MMU %s.\n", vcmd_mgr->mmu_enable ? "ENABLE" : "DISABLE");

#ifdef PCIE_EN
	mem_pcie->size = vcmd_mgr->mem_vcmd.size +
						vcmd_mgr->mem_status.size +
						vcmd_mgr->mem_regs.size;

	vcmd_mgr->pa_trans_offset = pci_ddr_base;

	/* allocate a single pool for all noncache_mems*/
	mem_pcie->pa = pci_ddr_base + ddr_offset + VCMD_BUF_POOL_OFFSET;
	if (!request_mem_region(mem_pcie->pa,
							mem_pcie->size,
							"vcx_vcmd_driver")) {
		vcmd_klog(LOGLVL_ERROR, "%s: failed to request vcmd pool memory region .\n", __func__);
		return -1;
	}
	vcmd_klog(LOGLVL_CONFIG, "Init: pcie pool pa=0x%llx.\n", (unsigned long long)mem_pcie->pa);
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
	mem_pcie->va = (void __force *)ioremap_nocache(mem_pcie->pa, mem_pcie->size);
#else
	mem_pcie->va = (void __force *)ioremap(mem_pcie->pa, mem_pcie->size);
#endif

	if (!mem_pcie->va) {
		vcmd_klog(LOGLVL_ERROR, "Init: failed to ioremap.\n");
		return -1;
	}
	vcmd_klog(LOGLVL_CONFIG, "%s: pcie pool va=0x%p.\n", __func__, mem_pcie->va);
#ifdef SUPPORT_MMU
	if (vcmd_mgr->mmu_enable) {
		mmu_addr.bus_address = mem_pcie->pa - pci_ddr_base;
		mmu_addr.size = mem_pcie->size;
		if (MMUKernelMemNodeMap(&mmu_addr, NULL) != MMU_STATUS_OK)
			return -1;
		mem_pcie->mmu_ba = mmu_addr.mmu_bus_address;
		vcmd_klog(LOGLVL_CONFIG, "%s: pool mmu_ba=0x%llx.\n", __func__,
			(unsigned long long)mem_pcie->mmu_ba);
	}
#endif

	/* layout this pool to noncache_mems*/
	offset = 0;
	va = (u8 *)mem_pcie->va;

	vcmd_mgr->mem_vcmd.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_vcmd.va = (u32 *)(va + offset);
	vcmd_mgr->mem_vcmd.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_vcmd.size;

	vcmd_mgr->mem_status.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_status.va = (u32 *)(va + offset);
	vcmd_mgr->mem_status.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_status.size;

	vcmd_mgr->mem_regs.pa = mem_pcie->pa + offset;
	vcmd_mgr->mem_regs.va = (u32 *)(va + offset);
	vcmd_mgr->mem_regs.mmu_ba = mem_pcie->mmu_ba + offset;


#else //PCIE_EN
	/* use dma_alloc_coherent for non-pcie env */
#if 0
	pci_ddr_base = alloc_base;
	g_vcmd_base_len = alloc_size * 1024 * 1024;
#else

	if (_vcmd_alloc_mem(vcmd_mgr, &vcmd_mgr->mem_vcmd))
		return -1;
	if (_vcmd_alloc_mem(vcmd_mgr, &vcmd_mgr->mem_status))
		return -1;
	if (_vcmd_alloc_mem(vcmd_mgr, &vcmd_mgr->mem_regs))
		return -1;
#endif
#endif //PCIE_EN

	return 0;
}

/**
 * @brief initialize context of all vcmd dev
 */
static void dev_ctx_init(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct hantrovcmd_dev *dev;
	u32 m_type;
	struct vcmd_module_mgr *module;

	memset(vcmd_mgr->dev_ctx,
			0, sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		dev->handler = (void *)vcmd_mgr;
		dev->subsys_info = &vcmd_mgr->core_array[i];
		m_type = dev->subsys_info->sub_module_type;

		dev->core_id = i;
		dev->state = VCMD_STATE_POWER_ON;
		dev->sw_cmdbuf_rdy_num = 0;
		/* the abnormal interrupts source from VCD */
		dev->abn_irq_mask = DEC_ABN_IRQ_MASK;
		dev->vcd_abn_irq_mask = DEC_ABN_IRQ_MASK;
#ifdef AXI2TO1_SUPPORT
		/* the abnormal interrupts source from AXI2TO1.
		 * currently, the axi2to1 interrupt has been connect to abnormal by HW,
		 * SW does't need to set the interrupt type as abnormal
		 */
		dev->abn_irq_mask |= AXI2TO1_ABN_IRQ_MASK;
#endif
		dev->intr_gate_mask = 0xFFFFFFFF & (~dev->abn_irq_mask);

		dev->timeout_timer_active = 0;
		dev->watchdog_active = 0;
		dev->arb_reset_irq = 0;
		dev->arb_err_irq = 0;
		dev->mmu_enable = vcmd_mgr->mmu_enable;
		dev->abort_mode = 0;
		dev->init_mode = VCMD_INIT_NORMAL;

		dev->spinlock = &dev->owner_lock_vcmd;
		spin_lock_init(dev->spinlock);
		spin_lock_init(&dev->abn_irq_lock);
		dev->abort_waitq = &dev->abort_queue_vcmd;
		init_waitqueue_head(dev->abort_waitq);
		init_waitqueue_head(&dev->buff_empty_waitq);

		dev->reg_mem_ba = vcmd_mgr->mem_regs.pa +
							i * SLOT_SIZE_REGBUF - vcmd_mgr->pa_trans_offset;
		dev->mmu_reg_mem_ba = vcmd_mgr->mem_regs.mmu_ba +
								i * SLOT_SIZE_REGBUF;
		dev->reg_mem_va = vcmd_mgr->mem_regs.va + i * SLOT_SIZE_REGBUF / 4;
		dev->reg_mem_sz = SLOT_SIZE_REGBUF;
#ifdef VCMD_ALLOC_MEM
		memset(dev->reg_mem_va, 0, dev->reg_mem_sz);
#endif
		dev->pa_trans_offset = vcmd_mgr->pa_trans_offset;

		module = &vcmd_mgr->module_mgr[m_type];
		if (module->num == 0)
			sema_init(&module->sem, 1);
		module->dev[module->num++] = dev;

		vcmd_klog(LOGLVL_CONFIG, "module init - vcmdcore[%d] addr =0x%llx\n",
			i, (unsigned long long)dev->subsys_info->reg_base);
	}
}

/**
 * @brief fill cmdbuf to read main module's all regs.
 */
#define VCMD_READ_CMD(cmd, n, off, addr) \
	do { \
		(cmd)->opcode = OPCODE_RREG | RREG_MODE(RREG_ADDR_INC) | \
						RREG_LEN(n) | \
						RREG_START_ADDR(off); \
		CMD_SET_ADDR(cmd, (addr)+(off)); \
		(cmd)->padding = 0; \
		cmd++; \
	} while (0)

static void create_read_all_registers_cmdbuf(vcmd_mgr_t *vcmd_mgr,
											struct exchange_parameter *param)
{
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;
	struct cmd_rreg_t *cmd_rreg;
	struct cmd_end_t *cmd_end;
	u8 *p_cmdbuf;
	addr_t reg_ba;

	dev = &vcmd_mgr->dev_ctx[param->core_id];
	subsys = dev->subsys_info;

	reg_ba = dev->reg_mem_ba;
	if (dev->mmu_enable)
		reg_ba = (addr_t)dev->mmu_reg_mem_ba;

	p_cmdbuf = (u8 *)vcmd_mgr->mem_vcmd.va + CMDBUF_OFF(param->cmdbuf_id);

	cmd_rreg = (struct cmd_rreg_t *)(p_cmdbuf + 0);
	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd executing cmdbuf id registers to ddr for balancing core load.
		VCMD_READ_CMD(cmd_rreg, 1, REG_ID_CMDBUF_EXE_ID * 4, 0);
	}
	//read main IP all registers
	VCMD_READ_CMD(cmd_rreg,
				subsys->io_size[SUB_MOD_MAIN] / 4,
				subsys->reg_off[SUB_MOD_MAIN] + 0,
				reg_ba);

#ifdef SUPPORT_AXIFE
	if (subsys->io_size[SUB_MOD_AXIFE])
		VCMD_READ_CMD(cmd_rreg, 1,
					subsys->reg_off[SUB_MOD_AXIFE] + 0,
					reg_ba);
#endif

	if (dev->hw_version_id > HW_ID_1_0_C) {
		//read vcmd registers to ddr, to compliant with user cmdbuf
		VCMD_READ_CMD(cmd_rreg, 27, 0, 0);
	}
	//end cmd
	cmd_end = (struct cmd_end_t *)cmd_rreg;
	cmd_end->opcode = OPCODE_END;
	cmd_end->padding = 0;
	cmd_end++;

	param->cmdbuf_size = (u8 *)cmd_end - p_cmdbuf;
}

/**
 * @brief release submodule IO resource for vcmd driver
 */
static void vcmd_release_submodule_IO(struct vcmd_subsys_info *subsys, u32 sub_mod_id)
{
	if (subsys->hwregs[sub_mod_id]) {
		iounmap((volatile u8 __iomem *)subsys->hwregs[sub_mod_id]);
		release_mem_region(subsys->reg_base + subsys->reg_off[sub_mod_id],
			subsys->io_size[sub_mod_id]);
		subsys->hwregs[sub_mod_id] = NULL;
	}
}

/**
 * @brief reserve submodule IO resource for vcmd driver
 */
static u32 vcmd_reserve_submodule_IO(struct vcmd_subsys_info *subsys, u32 sub_mod_id)
{
	addr_t pa = subsys->reg_base + subsys->reg_off[sub_mod_id];
	size_t sz = subsys->io_size[sub_mod_id];

	if (!request_mem_region(pa, sz, "vcx_vcmd_driver"))
		return -EBUSY;

#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
	subsys->hwregs[sub_mod_id] = (volatile u8 __force *)ioremap_nocache(pa, sz);
#else
	subsys->hwregs[sub_mod_id] = (volatile u8 __force *)ioremap(pa, sz);
#endif

	return 0;
}

#ifdef SUPPORT_MMU
/**
 * @brief MMU kernel map for vcmd driver
 */
static int vcmd_mmu_kernel_map(vcmd_mgr_t *vcmd_mgr, struct file *filp)
{
	struct kernel_addr_desc mmu_addr;

#ifdef PCIE_EN
	struct noncache_mem *mem_pcie = &vcmd_mgr->pcie_pool;
	u32 offset = 0;

	mmu_addr.bus_address = mem_pcie->pa - vcmd_mgr->pa_trans_offset;
	mmu_addr.size = mem_pcie->size;
	if (MMUKernelMemNodeMap(&mmu_addr, filp) != MMU_STATUS_OK)
		return -1;
	mem_pcie->mmu_ba = mmu_addr.mmu_bus_address;
	vcmd_klog(LOGLVL_CONFIG, "%s: pool mmu_ba=0x%llx.\n", __func__,
		(unsigned long long)mem_pcie->mmu_ba);

	vcmd_mgr->mem_vcmd.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_vcmd.size;
	vcmd_mgr->mem_status.mmu_ba = mem_pcie->mmu_ba + offset;
	offset += vcmd_mgr->mem_status.size;
	vcmd_mgr->mem_regs.mmu_ba = mem_pcie->mmu_ba + offset;
#else
	struct noncache_mem mem[3];
	int i;

	mem[0] = vcmd_mgr->mem_vcmd,
	mem[1] = vcmd_mgr->mem_status,
	mem[2] = vcmd_mgr->mem_regs

	for (i = 0; i < 3; i++) {
		mmu_addr.bus_address = mem[i].pa;
		mmu_addr.size = mem[i].size;
		if (MMUKernelMemNodeMap(&mmu_addr, filp) != MMU_STATUS_OK) {
			vcmd_klog(LOGLVL_ERROR, "Init[%s] mmu map mem failed\n", __func__);
			return -1;
		}
		mem[i].mmu_ba = mmu_addr.mmu_bus_address;
		vcmd_klog(LOGLVL_FLOW, "Init[%s]: mem->mmu_ba=0x%llx.\n", __func__,
				(unsigned long long)mem[i].mmu_ba);
	}
#endif

	return 0;
}
#endif

/**
 * @brief config modules for vcmd driver
 */
static int vcmd_config_modules(vcmd_mgr_t *vcmd_mgr)
{
	int i;
	struct vcmd_subsys_info *subsys;
#ifdef SUPPORT_MMU
	enum MMUStatus mmu_status = MMU_STATUS_FALSE;
	int ret = 0;
#endif

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		/* config AXIFE*/
#ifdef SUPPORT_AXIFE
		if (subsys->hwregs[SUB_MOD_AXIFE0])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE0]);//AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE0], 1);
		if (subsys->hwregs[SUB_MOD_AXIFE1])
			AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE1]);//AXIFEEnable(subsys->hwregs[SUB_MOD_AXIFE1], 1);
#endif
		/* config MMU */
#ifdef SUPPORT_MMU
		if (subsys->hwregs[SUB_MOD_MMU0]) {
			mmu_status = MMUInit(subsys->hwregs[SUB_MOD_MMU0]);
			if (mmu_status == MMU_STATUS_NOT_FOUND) {
				vcmd_klog(LOGLVL_ERROR, "MMU does not exist!\n");
				return -1;
			} else if (mmu_status != MMU_STATUS_OK) {
				return -2;
			} else {
				vcmd_klog(LOGLVL_BRIEF, "MMU detected!\n");
			}
		}
#endif

		/* config AXI2TO1 */
#ifdef AXI2TO1_SUPPORT
		if (subsys->hwregs[SUB_MOD_AXI2TO1]) {
			if (AXI2TO1_init(subsys->hwregs[SUB_MOD_AXI2TO1]) < 0)
				return -5;
		}
#endif
	}
	/* config MMU*/
#ifdef SUPPORT_MMU
	mmu_status = MMUEnable(vcmd_mgr->mmu_hwregs, vcmd_mgr->platformdev);
	if (mmu_status != MMU_STATUS_OK) {
		vcmd_klog(LOGLVL_ERROR, "MMUEnable: MMU enable failed\n");
		return -3;
	}
	vcmd_mgr->mmu_enable = mmu_enable;
	vcmd_klog(LOGLVL_CONFIG, "MMU %s.\n", vcmd_mgr->mmu_enable ? "ENABLE" : "DISABLE");

	if (vcmd_mgr->mmu_enable) {
		ret = vcmd_mmu_kernel_map(vcmd_mgr, NULL);
		if (ret < 0)
			return -4;
	}
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		vcmd_mgr->dev_ctx[i].mmu_enable = vcmd_mgr->mmu_enable;
		vcmd_mgr->dev_ctx[i].mmu_reg_mem_ba = vcmd_mgr->mem_regs.mmu_ba +
							i * SLOT_SIZE_REGBUF;
	}
#endif

	return 0;
}

/**
 * @brief reserve IO resources for vcmd driver.
 */
static int vcmd_reserve_IO(vcmd_mgr_t *vcmd_mgr)
{
	u32 hwid;
	int i;
	u32 found_hw = 0;
	struct hantrovcmd_dev *dev;
	struct vcmd_subsys_info *subsys;
	u32 reg_val;
	int ret, has_arbiter;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		subsys = dev->subsys_info;
		dev->hwregs = NULL;

		ret = vcmd_reserve_submodule_IO(subsys, SUB_MOD_VCMD);
		if (ret < 0) {
			vcmd_klog(LOGLVL_ERROR, "failed to reserve HW regs for vcmd[%d]\n",
						i);
			vcmd_klog(LOGLVL_ERROR,
						"vcmd_base_addr = %llx, iosize = %d\n",
						subsys->reg_base, subsys->io_size[SUB_MOD_VCMD]);
			continue;
		}
		if (!subsys->hwregs[SUB_MOD_VCMD]) {
			vcmd_klog(LOGLVL_ERROR, "failed to ioremap HW regs\n");
			release_mem_region(
				subsys->reg_base, subsys->io_size[SUB_MOD_VCMD]);
			continue;
		}
		dev->hwregs = subsys->hwregs[SUB_MOD_VCMD];
//		vcmd_core_array[i].submodule_vcmd_virtual_address = dev->hwregs;

		/*read hwid and check validness and store it*/
		hwid = (u32)ioread32((void __iomem *)dev->hwregs);
		vcmd_klog(LOGLVL_CONFIG, "hantrovcmd: vcmd[%d] hwid=0x%08x\n", i, hwid);
		dev->hw_version_id = hwid;

		/* check for vcmd HW ID */
		if (((hwid >> 16) & 0xFFFF) != VCMD_HW_ID) {
			vcmd_klog(LOGLVL_WARNING, "HW not found at 0x%llx\n",
				(unsigned long long)subsys->reg_base);
			iounmap((void __iomem *)dev->hwregs);
			release_mem_region(subsys->reg_base,
								subsys->io_size[SUB_MOD_VCMD]);
			dev->hwregs = NULL;
			continue;
		}

		reg_val = (u32)ioread32((void __iomem *)dev->hwregs +
				   VCMD_REGISTER_HW_APB_ARBITER_MODE_OFFSET);
		has_arbiter = (u8)((reg_val >> HW_APB_ARBITER_MODE_BIT) & 0x01);
		dev->hw_feature.vcarb_ver = 0;
		if (has_arbiter)
			dev->hw_feature.vcarb_ver = hwid < HW_ID_1_5_9 ? VCARB_VERSION_2_0 :
										VCARB_VERSION_3_0;

		reg_val = (u32)ioread32((void __iomem *)dev->hwregs +
				   VCMD_REGISTER_HW_INIT_MODE_OFFSET);
		dev->hw_feature.has_init_mode = (u8)((reg_val >> HW_INIT_MODE_BIT) &
										0x01);
		dev->hw_feature.has_cmdbuf_timeout = 0;
		if (hwid >= HW_ID_1_5_10) {
			dev->hw_feature.has_cmdbuf_timeout = 1;
			dev->abn_irq_mask &= ~DEC_ABN_IRQ_MASK;
			dev->abn_irq_mask |= CORE_IP_ABN_IRQ_MASK;
			dev->vcd_abn_irq_mask = CORE_IP_ABN_IRQ_MASK;
			dev->intr_gate_mask = 0xFFFFFFFF & (~dev->abn_irq_mask);
		}
#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
		if (dev->hw_feature.has_init_mode)
			dev->init_mode = DEFAULT_VCMD_INIT_MODE;
#endif

		found_hw = 1;
		vcmd_klog(LOGLVL_CONFIG, "HW at base <0x%llx> with ID <0x%08x>\n",
					(unsigned long long)subsys->reg_base, hwid);
	}

	if (found_hw == 0) {
		vcmd_klog(LOGLVL_ERROR, "NO ANY HW found!!\n");
		return -1;
	}

	return 0;
}

/**
 * @brief release IO resources of vcmd driver.
 */
static void vcmd_release_IO(vcmd_mgr_t *vcmd_mgr)
{
	u32 i;
	struct vcmd_subsys_info *subsys;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		subsys = &vcmd_mgr->core_array[i];
		vcmd_release_submodule_IO(subsys, SUB_MOD_VCMD);
		vcmd_mgr->dev_ctx[i].hwregs = NULL;
	}
}

/**
 * @brief get a device submodule regs virtual address
 */
u32 *get_submodule_regs_va(void *_vcmd_mgr, u32 subsys_id, u32 sub_mod_id)
{
	u32 *regs_va;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	struct hantrovcmd_dev *dev = &vcmd_mgr->dev_ctx[subsys_id];

	regs_va = dev->reg_mem_va +
				dev->subsys_info->reg_off[sub_mod_id] / 4 + 0;

	return regs_va;
}

/**
 * @brief read main module's all regs for all vcmd hw devices.
 */
static void read_main_module_all_registers(vcmd_mgr_t *vcmd_mgr)
{
	int ret;
	struct exchange_parameter param[MAX_SUBSYS_NUM];
	u16 done_id = 0;
	struct proc_obj *po;
	u32 i;
	struct hantrovcmd_dev *dev;
	struct vcmd_module_mgr *module;
	u32 *main_regs_va;

	po = vcmd_mgr->init_po;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];
		param[i].executing_time = 0;
		param[i].input_mask = 0;
		param[i].cmdbuf_size = 0;
		param[i].module_type = dev->subsys_info->sub_module_type;
		param[i].core_mask = 1 << dev->core_id;
		param[i].core_id = dev->core_id;
		/* normal priority and last cmd is end */
		EXCH_S_BIT(param[i].input_mask, EXCH_END_CMD_BIT);
		module = &vcmd_mgr->module_mgr[param[i].module_type];

		ret = reserve_cmdbuf(vcmd_mgr, po, &param[i]);
		create_read_all_registers_cmdbuf(vcmd_mgr, &param[i]);
		link_and_run_cmdbuf(vcmd_mgr, po, &param[i]);
	}

	/* make sure vcmd can complete job, and clear irq
	 */
	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		dev = &vcmd_mgr->dev_ctx[i];

		wait_cmdbuf_ready(vcmd_mgr, po, param[i].cmdbuf_id, &done_id);
		main_regs_va = dev->reg_mem_va +
				dev->subsys_info->reg_off[SUB_MOD_MAIN] / 4 + 0;
		vcmd_klog(LOGLVL_CONFIG, "main module register 0:0x%x\n",
			*main_regs_va);
		vcmd_klog(LOGLVL_CONFIG, "main module register 50:0x%08x\n",
			*(main_regs_va + 50));
		vcmd_klog(LOGLVL_CONFIG, "main module register 54:0x%08x\n",
			*(main_regs_va + 54));
		vcmd_klog(LOGLVL_CONFIG, "main module register 56:0x%08x\n",
			*(main_regs_va + 56));
		vcmd_klog(LOGLVL_CONFIG, "main module register 309:0x%x\n",
			*(main_regs_va + 309));

		release_cmdbuf(vcmd_mgr, po, param[i].cmdbuf_id);

	}
}

/**
 * @brief convert subsys info to core array info of vcmd driver context.
 */
static void SubsysToVcmdCoreCfg(vcmd_mgr_t *vcmd_mgr)
{
	int i, k, array_sz;
	struct vcmd_subsys_info *subsys;
	struct sub_mod_cfg *mod_cfg;
	enum subsys_module_id mod_id;

	array_sz = ARRAY_SIZE(vcmd_core_array);

	/* To plug into vcx_vcmd_driver.c */
	for (i = 0; i < array_sz; i++) {
		if (vcmd_core_array[i].vcmd_base_addr) {
			subsys = &vcmd_mgr->core_array[vcmd_mgr->subsys_num];
			subsys->irq = vcmd_core_array[i].vcmd_irq;
			/* reg_base = vcmd_base_addr + reg_base_offset,
			 * but now reg_base_offset is 0, will be added later
			 */
			subsys->reg_base = vcmd_core_array[i].vcmd_base_addr +
							   /* vcmd_mgr->reg_base_offset */ 0;
			subsys->sub_module_type = vcmd_core_array[i].sub_module_type;
			subsys->vcmd_priority = vcmd_core_array[i].priority;

			for (k = 0; k < SUB_MOD_MAX; k++) {
				mod_cfg = &vcmd_core_array[i].submodule_cfg[k];
				mod_id = mod_cfg->sub_mod_id;
				if (mod_id < SUB_MOD_MAX) {
					subsys->reg_off[mod_id] = mod_cfg->io_off;
					if (mod_cfg->io_off != 0xffff) {
						subsys->io_size[mod_id] = mod_cfg->io_size;
						subsys->rreg_id[mod_id] = mod_cfg->rreg_id;
						subsys->rreg_num[mod_id] = mod_cfg->rreg_num;
					}
				} else {
					vcmd_klog(LOGLVL_ERROR, "wrong sub-module id in vcmd_core_array[%d].submodule_cfg[%d]\n",
							  i, k);
				}
			}

			vcmd_mgr->subsys_num++;
		}
	}
	vcmd_klog(LOGLVL_CONFIG, "%d VCMD cores found\n", vcmd_mgr->subsys_num);
}


/**
 * @brief vcmd driver initialization
 * @param struct subsys_config *subsys: info of sub-systems.
 * @param int subsys_num: number of sub-systems.
 * @param void *platformdev: platform device handler
 * @return void *: NULL: failed; other: vcmd driver handler.
 */

/**
 * @brief de-init vcmd driver.
 */
void hantrodec_vcmd_cleanup(vcx_priv_t *priv)
{
	int i = 0;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)priv->priv;
	struct hantrovcmd_dev *dev_ctx = vcmd_mgr->dev_ctx;

	_vcmd_kthread_stop(vcmd_mgr);

#ifdef SUPPORT_DBGFS
	_dbgfs_cleanup((void *)vcmd_mgr);
#endif
#ifdef SUPPORT_MMU
		MMUCleanup(vcmd_mgr->platformdev);
#endif
	vcmd_release_IO(vcmd_mgr);
	vfree(dev_ctx);

	//release_vcmd_non_cachable_memory();
#ifdef PCIE_EN
	_vcmd_pcie_cleanup(vcmd_mgr);
#else
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_vcmd);
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_status);
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_regs);
#endif

	free_process_object(vcmd_mgr->init_po);
	vfree(vcmd_mgr);
	priv->priv = NULL;
	vcmd_klog(LOGLVL_FLOW, "module removed\n");
}

/**
 * @brief flush slice regs in vcmd driver
 */
static long flush_slice_regs(vcmd_mgr_t *vcmd_mgr, struct subsys_regs_desc *slice_regs, struct proc_obj *po)
{
	u32 cmdbuf_id = slice_regs->id;
	struct hantrovcmd_dev *dev = NULL;
	struct cmdbuf_obj *obj = NULL;
	bi_list_node *node = NULL;
	volatile u8 *hwregs;
	unsigned long flags;
	u32 reg_id;
	int i;

	node = &vcmd_mgr->nodes[cmdbuf_id];
	obj = (struct cmdbuf_obj *)node->data;

	dev = &vcmd_mgr->dev_ctx[obj->core_id];
	hwregs = dev->subsys_info->hwregs[SUB_MOD_MAIN];

	for (i = 0; i < slice_regs->reg_num - 1; i++) {
		reg_id = slice_regs->regs[i].reg_id;
		iowrite32(slice_regs->regs[i].reg_data, (void __iomem *)(hwregs + 4 * reg_id));
	}

	spin_lock_irqsave(&dev->abn_irq_lock, flags);
	iowrite32(slice_regs->regs[i].reg_data, (void __iomem *)(hwregs + 0x4));
	obj->slice_run_done = 0;
	spin_unlock_irqrestore(&dev->abn_irq_lock, flags);
    return cmda78_gen_ctrl_cmdbuf(po, VCMD_MGR_ID_DEC, CMD_REQ_PUSH_SLICE_REG, cmdbuf_id);
}

/**
 * @brief drop cmdbufs_id or releae cmd from this po
 */
static long drop_release_cmdbufs(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, void *owner)
{
	struct hantrovcmd_dev *dev;
	struct cmdbuf_obj *obj;

	u32 i;
	unsigned long flags;
	long dropped_cmdbuf_num = 0;

	dropped_cmdbuf_num = cmda78_gen_drop_owner(po, (uint64_t)owner, VCMD_MGR_ID_DEC);
	// remove cmdbuf reserved but not in work_list or has been dropped
	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		obj = &vcmd_mgr->objs[i];
		if (obj->po == po) {
			if ((owner == NULL) || (owner == obj->owner)) {
				if (obj->core_id == 0xFFFF) {
					return_cmdbuf(vcmd_mgr, i);
				} else {
					dev = &vcmd_mgr->dev_ctx[obj->core_id];
					spin_lock_irqsave(dev->spinlock, flags);
					return_cmdbuf(vcmd_mgr, i);
					spin_unlock_irqrestore(dev->spinlock, flags);
				}
			}
		}
	}

	if (owner && dropped_cmdbuf_num) {
		wake_up_interruptible_all(&po->job_waitq);
	}

	return dropped_cmdbuf_num;
}

/**
 * @brief drop owner from current po
 */
static long drop_owner(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, void *owner)
{
	u32 module_type;
	long dropped_cmdbuf_num = 0;

	module_type = po->module_type;
	if (down_interruptible(&vcmd_mgr->module_mgr[module_type].sem))
		return -ERESTARTSYS;

	dropped_cmdbuf_num = drop_release_cmdbufs(vcmd_mgr, po, owner);

	up(&vcmd_mgr->module_mgr[module_type].sem);
	return dropped_cmdbuf_num;
}

/**
 * @brief wait the cmdbuf with special owner done
 */
static long wait_owner_done(vcmd_mgr_t *vcmd_mgr, struct proc_obj *po, void *owner)
{
	struct cmdbuf_obj *obj = NULL;
	u32 module_type;
	u32 i, ret;

	ret = 0;
	module_type = po->module_type;
	if (down_interruptible(&vcmd_mgr->module_mgr[module_type].sem))
		return -ERESTARTSYS;

	for (i = 0; i < SLOT_NUM_CMDBUF; i++) {
		obj = &vcmd_mgr->objs[i];
		if (obj->po == po && obj->owner == owner) {
			ret = 1;
			break;
		}
	}

	up(&vcmd_mgr->module_mgr[module_type].sem);
	return ret;
}

/**
 * @brief ioctl function of vcmd driver.
 */
static long hantrovcmd_dec_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	long tmp;
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)filp->private_data;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;

	_log_ioctl_cmd(cmd);
	/*
	 * extract the type and number bitfields, and don't encode
	 * wrong cmds: return ENOTTY (inappropriate ioctl)
	 * before access_ok()
	 */
	if (_IOC_TYPE(cmd) != HANTRO_VCMD_IOC_MAGIC) {
#ifdef SUPPORT_MMU
		if (_IOC_TYPE(cmd) != HANTRO_IOC_MMU) {
			return -ENOTTY;
		}
#else
		return -ENOTTY;
#endif
	}


	if ((_IOC_TYPE(cmd) == HANTRO_VCMD_IOC_MAGIC && _IOC_NR(cmd) > HANTRO_VCMD_IOC_MAXNR)) {
		return -ENOTTY;
	} else {
#ifdef SUPPORT_MMU
		 if((_IOC_TYPE(cmd) == HANTRO_IOC_MMU && _IOC_NR(cmd) > HANTRO_IOC_MMU_MAXNR)) {
			return -ENOTTY;
		 }
#endif
	}

		/*
		 * the direction is a bitmask, and VERIFY_WRITE catches R/W
		 * transfers. `Type' is user-oriented, while
		 * access_ok is kernel-oriented, so the concept of "read" and
		 * "write" is reversed
		 */
#if (KERNEL_VERSION(5, 0, 0) > LINUX_VERSION_CODE)
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg,
				 _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg,
				 _IOC_SIZE(cmd));
#else
	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
#endif
	if (err)
		return -EFAULT;

	switch (cmd) {
/*
	case HANTRO_IOCH_GET_VCMD_ENABLE: {
		__put_user(1, (unsigned long __user *)arg);
		break;
	}

	case HANTRO_IOCH_GET_MMU_ENABLE: {
		__put_user(vcmd_mgr->mmu_enable, (unsigned int __user  *)arg);
		break;
	}*/

	case HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER: {
		struct cmdbuf_mem_parameter mem;

		vcmd_klog(LOGLVL_CONFIG, "VCMD get cmdbuf parameter\n");
		mem.cmdbuf_unit_size = SLOT_SIZE_CMDBUF;
		mem.status_cmdbuf_unit_size = SLOT_SIZE_STATUSBUF;
		mem.vcmd_regbuf_unit_size = SLOT_SIZE_REGBUF;
		mem.cmdbuf_total_size = vcmd_mgr->mem_vcmd.size;
		mem.phy_cmdbuf_addr = vcmd_mgr->mem_vcmd.pa;
		mem.status_cmdbuf_total_size = vcmd_mgr->mem_status.size;
		mem.phy_status_cmdbuf_addr = vcmd_mgr->mem_status.pa;
		mem.vcmd_regbuf_total_size = vcmd_mgr->mem_regs.size;
		mem.phy_vcmd_regbuf_addr = vcmd_mgr->mem_regs.pa;
		mem.mmu_phy_status_cmdbuf_addr = vcmd_mgr->mem_status.mmu_ba;
		mem.mmu_phy_cmdbuf_addr = vcmd_mgr->mem_vcmd.mmu_ba;
		mem.mmu_phy_vcmd_regbuf_addr = vcmd_mgr->mem_regs.mmu_ba;
		mem.base_ddr_addr = vcmd_mgr->pa_trans_offset;
		tmp = copy_to_user((struct cmdbuf_mem_parameter __user *)arg,
				 &mem, sizeof(struct cmdbuf_mem_parameter));
		if (tmp) {
			vcmd_klog(LOGLVL_ERROR, "copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		break;
	}
	case HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER: {
		struct config_parameter param;
		struct proc_obj *po = NULL;
		u16 m_type;
		struct hantrovcmd_dev *dev;
		struct vcmd_subsys_info *info;

		vcmd_klog(LOGLVL_CONFIG, "VCMD get vcmd config parameter\n");
		tmp = copy_from_user(&param,
				   (struct config_parameter __user *)arg,
				   sizeof(struct config_parameter));
		if (tmp) {
			vcmd_klog(LOGLVL_ERROR, "copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		po = (struct proc_obj *)ctx->po;
		if (!po) {
			vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
			return -1;
		}
		spin_lock(&po->spinlock);
		po->module_type = param.module_type;
		m_type = param.module_type;
		spin_unlock(&po->spinlock);
		if (vcmd_mgr->module_mgr[m_type].num) {
			dev = vcmd_mgr->module_mgr[m_type].dev[0];
			info = dev->subsys_info;
			param.submodule_main_addr = info->reg_off[SUB_MOD_MAIN];
			param.submodule_dec400_addr = info->reg_off[SUB_MOD_DEC400];
			param.submodule_MMU_addr = info->reg_off[SUB_MOD_MMU];
			param.submodule_MMUWrite_addr = info->reg_off[SUB_MOD_MMU_WR];
			param.submodule_axife_addr = info->reg_off[SUB_MOD_AXIFE];
			param.vcmd_hw_version_id =	dev->hw_version_id;
			param.vcmd_core_num = vcmd_mgr->module_mgr[m_type].num;
		} else {
			param.submodule_main_addr = 0xffff;
			param.submodule_dec400_addr = 0xffff;
			param.submodule_MMU_addr = 0xffff;
			param.submodule_MMUWrite_addr = 0xffff;
			param.submodule_axife_addr = 0xffff;
			param.vcmd_core_num = 0;
			param.vcmd_hw_version_id = HW_ID_1_0_C;
		}
		tmp = copy_to_user((struct config_parameter __user *)arg, &param,
				 sizeof(struct config_parameter));
		if (tmp) {
			vcmd_klog(LOGLVL_ERROR, "copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		break;
	}
	case HANTRO_VCMD_IOCH_RESERVE_CMDBUF: {
		long ret;
		struct exchange_parameter param;
		struct proc_obj *po = _GET_PO(filp);

		tmp = copy_from_user(&param,
				   (struct exchange_parameter __user *)arg,
				   sizeof(struct exchange_parameter));
		if (tmp) {
			vcmd_klog(LOGLVL_ERROR, "copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		ret = reserve_cmdbuf(vcmd_mgr, po, &param);
		if (ret == 0) {
			tmp = copy_to_user((struct exchange_parameter __user *)arg,
					 &param,
					 sizeof(struct exchange_parameter));
			if (tmp) {
				vcmd_klog(LOGLVL_ERROR, "copy_to_user failed, returned %li\n", tmp);
				return -EFAULT;
			}
		}
		vcmd_klog(LOGLVL_CONFIG, "VCMD Reserve CMDBUF %d\n", param.cmdbuf_id);
		return ret;
	}

	case HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF: {
		struct exchange_parameter param;
		long retVal;
		struct proc_obj *po = _GET_PO(filp);

		tmp = copy_from_user(&param,
				   (struct exchange_parameter __user *)arg,
				   sizeof(struct exchange_parameter));
		if (tmp) {
			vcmd_klog(LOGLVL_ERROR, "copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		vcmd_klog(LOGLVL_CONFIG, "VCMD link and run CMDBUF %d\n", param.cmdbuf_id);
		retVal = link_and_run_cmdbuf(vcmd_mgr, po, &param);
		tmp = copy_to_user((struct exchange_parameter __user *)arg,
				 &param, sizeof(struct exchange_parameter));
		if (tmp) {
			vcmd_klog(LOGLVL_ERROR, "copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		return retVal;
	}

	case HANTRO_VCMD_IOCH_WAIT_CMDBUF: {
		u16 cmdbuf_id;
		long tmp;
		struct proc_obj *po = _GET_PO(filp);

		__get_user(cmdbuf_id, (u16 __user *)arg);
		/*high 16 bits are core id, low 16 bits are cmdbuf_id*/

		vcmd_klog(LOGLVL_FLOW, "VCMD wait for CMDBUF %d finishing.\n", cmdbuf_id);

		tmp = wait_cmdbuf_ready(vcmd_mgr, po, cmdbuf_id, &cmdbuf_id);
		if (tmp >= 0) {
			__put_user(cmdbuf_id, (u16 __user *)arg);
			return tmp;
		}
		__put_user(0, (u16 __user *)arg);
		return -1;

		break;
	}

	case HANTRO_VCMD_IOCH_PUSH_SLICE_REG: {
		struct subsys_regs_desc slice_regs;
		struct proc_obj *po = _GET_PO(filp);

		vcmd_klog(LOGLVL_FLOW, "VCMD push slice reg for buffer empty.\n");
		/* get registers from user space*/
		tmp = copy_from_user(&slice_regs, (void __user *)arg,
				     sizeof(struct subsys_regs_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		return flush_slice_regs(vcmd_mgr, &slice_regs, po);
	}

	case HANTRO_VCMD_IOCH_ABORT_CMDBUF: {
		u16 cmdbuf_id;
		struct proc_obj *po = _GET_PO(filp);

		vcmd_klog(LOGLVL_FLOW, "VCMD abort slice for seek.\n");
		/* get registers from user space*/
		tmp = __get_user(cmdbuf_id, (u16 __user *)arg);
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		tmp = cmda78_gen_ctrl_cmdbuf(po, VCMD_MGR_ID_DEC, CMD_REQ_ABORT_CMDBUF, cmdbuf_id);
		if (tmp) {
			PDEBUG("abort_cmdbuf failed, returned %li\n", tmp);
			return -EFAULT;
		}

		break;
	}

	case HANTRO_VCMD_IOCH_DROP_OWNER: {
		long dropped_cmdbuf_num = 0;
		struct proc_obj *po = _GET_PO(filp);

		vcmd_klog(LOGLVL_FLOW, "VCMD drop owner %p for seek.\n", (void *)arg);

		tmp = drop_owner(vcmd_mgr, po, (void *)arg);
		if (tmp < 0) {
			vcmd_klog(LOGLVL_ERROR, "drop_owner failed, returned %li\n", tmp);
			return -EFAULT;
		}
		dropped_cmdbuf_num = tmp;

		return dropped_cmdbuf_num;
	}

	case HANTRO_VCMD_IOCH_WAIT_OWNER_DONE: {
		struct proc_obj *po = _GET_PO(filp);

		vcmd_klog(LOGLVL_FLOW, "VCMD wait owner %p done.\n", (void *)arg);

		tmp = wait_owner_done(vcmd_mgr, po, (void *)arg);
		if (tmp < 0) {
			PDEBUG("wait_cmdbuf_done failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return 0;
	}

	case HANTRO_VCMD_IOCH_RELEASE_CMDBUF: {
		u16 cmdbuf_id;
		struct proc_obj *po = _GET_PO(filp);

		__get_user(cmdbuf_id, (u16 __user *)arg);
		/*16 bits are cmdbuf_id*/

		vcmd_klog(LOGLVL_FLOW, "VCMD release CMDBUF %d\n", cmdbuf_id);

		release_cmdbuf(vcmd_mgr, po, cmdbuf_id);
		return 0;
		//break;
	}
	case HANTRO_VCMD_IOCH_POLLING_CMDBUF: {
		u16 core_id;
		struct proc_obj *po = _GET_PO(filp);

		__get_user(core_id, (u16 __user *)arg);
		/*16 bits are cmdbuf_id*/
		if (core_id >= vcmd_mgr->subsys_num)
			return -1;
		if (down_interruptible(&vcmd_mgr->isr_polling_sema))
			return -ERESTARTSYS;
		cmda78_gen_ctrl_cmdbuf(po, VCMD_MGR_ID_DEC, CMD_REQ_POLLING_CMDBUF, core_id);
		up(&vcmd_mgr->isr_polling_sema);
		return 0;
	}
	default: {
#ifdef SUPPORT_MMU
		if (_IOC_TYPE(cmd) == HANTRO_IOC_MMU) {
			return MMUIoctl(cmd, filp, arg, vcmd_mgr->mmu_hwregs);
		}
#endif
		break;
	}

	}
	return 0;
}

#if defined SUPPORT_MMU && defined SUPPORT_48PA_MMU
int _vcmd_memory_map(void *_vcmd_mgr, struct file *filp)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	struct kernel_addr_desc mmu_addr;

	if (vcmd_mgr->mmu_enable) {
#ifdef PCIE_EN
		mmu_addr.bus_address = vcmd_mgr->pcie_pool.pa - vcmd_mgr->pa_trans_offset;
		mmu_addr.size = vcmd_mgr->pcie_pool.size;
		if (MMUKernelMemNodeMap(&mmu_addr, filp) != MMU_STATUS_OK)
			return -EINVAL;
#else
		mmu_addr.bus_address = vcmd_mgr->mem_vcmd.pa;
		mmu_addr.size = vcmd_mgr->mem_vcmd.size;
		if (MMUKernelMemNodeMap(&mmu_addr, filp) != MMU_STATUS_OK)
			return -EINVAL;
		mmu_addr.bus_address = vcmd_mgr->mem_status.pa;
		mmu_addr.size = vcmd_mgr->mem_status.size;
		if (MMUKernelMemNodeMap(&mmu_addr, filp) != MMU_STATUS_OK)
			return -EINVAL;
		mmu_addr.bus_address = vcmd_mgr->mem_regs.pa;
		mmu_addr.size = vcmd_mgr->mem_regs.size;
		if (MMUKernelMemNodeMap(&mmu_addr, filp) != MMU_STATUS_OK)
			return -EINVAL;
#endif // PCIE_EN
	}
	return 0;
}
#endif // SUPPORT_MMU && SUPPORT_48PA_MMU

/**
 * @brief open hantro vcmd device.
 */
static int hantrovcmd_dec_open(struct inode *inode, struct file *filp) {
	PDEBUG("dev opened\n");
	struct vcmd_priv_ctx *ctx = NULL;
	struct proc_obj *po;

	ctx = vmalloc(sizeof(struct vcmd_priv_ctx));
	if (!ctx) {
		PDEBUG("Create vcmd private context failed!\n");
		return -EINVAL;
	}
	memset(ctx, 0, sizeof(struct vcmd_priv_ctx));

	filp->private_data = (void *)ctx;
	ctx->vcmd_mgr = (void *)vcmd_manager;

#ifdef SUPPORT_DBGFS
	/* for debugfs */
	_dbgfs_init_ctx(ctx->vcmd_mgr, 1);
#endif
	po = create_process_object();
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "Create process object failed!\n");
		vfree(ctx);
		return -EINVAL;
	}

	if (cmda78_gen_open_session(po, R52_CORE_MASK_VDEC) < 0) {
		vcmd_klog(LOGLVL_ERROR, "Open session failed!\n");
		free_process_object(po);
		vfree(ctx);
		return -EINVAL;
	}

	po->filp = filp;
	po->module_type = ((vcmd_mgr_t *)ctx->vcmd_mgr)->core_array[0].sub_module_type;
	ctx->po = (void *)po;
#ifdef SUPPORT_48PA_MMU
	if (((vcmd_mgr_t *)ctx->vcmd_mgr)->mmu_enable) {
		int ret = MMUCreatePageTable(filp);

		if (ret < 0) {
			pr_err("MMU Create page table failed!\n");
			return -EINVAL;
		}
#ifdef MMU_PAGE_TABLE_SWITCH
		if (ret != MMU_STATUS_PT_EXIST)
			vcmd_mmu_kernel_map((vcmd_mgr_t *)ctx->vcmd_mgr, filp);
#endif
	}
#endif

#ifdef CONFIG_DEC_PM
	vcx_vcodec_pm_runtime_get((vcx_priv_t*)vcmd_manager->priv);
#endif

	vcmd_klog(LOGLVL_FLOW, "dev opened\n");
	vcmd_klog(LOGLVL_FLOW, "process obj %p for filp opened %p\n", (void *)po, (void *)filp);
	return 0;
}

/**
 * @brief release hantro vcmd device.
 */
static int hantrovcmd_dec_release(struct inode *inode, struct file *filp) {
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)filp->private_data;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	struct proc_obj *po;

	u32 module_type;

	vcmd_klog(LOGLVL_FLOW, "%s process %p start release\n", __func__, (void *)filp);
	po = (struct proc_obj *)ctx->po;
	if (!po) {
		vcmd_klog(LOGLVL_ERROR, "%s: not find process obj!\n", __func__);
		return -1;
	}

	module_type = po->module_type;
	if (down_interruptible(&vcmd_mgr->module_mgr[module_type].sem))
		return -ERESTARTSYS;

	drop_release_cmdbufs(vcmd_mgr, po, NULL);

	vcmd_klog(LOGLVL_FLOW, "process obj %p for filp to be removed: %p\n",
			(void *)po, (void *)po->filp);

	if (cmda78_gen_close_session(po, R52_CORE_MASK_VDEC) < 0) {
		vcmd_klog(LOGLVL_ERROR, "Close session failed!\n");
		//return -1;
	}

	free_process_object(ctx->po);
	ctx->po = NULL;
	vfree(ctx);
	up(&vcmd_mgr->module_mgr[module_type].sem);

#ifdef CONFIG_DEC_PM
    vcx_vcodec_pm_runtime_put((vcx_priv_t*)vcmd_manager->priv);
#endif
	return 0;
}

static int hantrovcmd_dec_mmap(struct file *filp, struct vm_area_struct *vma);

/* VFS methods */
static const struct file_operations hantrovcmd_dec_fops = {
	.owner = THIS_MODULE,
	.open = hantrovcmd_dec_open,
	.mmap = hantrovcmd_dec_mmap,
	.release = hantrovcmd_dec_release,
	.unlocked_ioctl = hantrovcmd_dec_ioctl,
	.fasync = NULL,
};

static const struct vm_operations_struct hantrovcmd_dec_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};

int hantrodec_vcmd_init(vcx_priv_t *priv) {
	int result;
	struct hantrovcmd_dev *dev_ctx;
	vcmd_mgr_t *vcmd_mgr;

	vcmd_mgr = vmalloc(sizeof(vcmd_mgr_t));
	if (!vcmd_mgr)
		return -1;

	memset(vcmd_mgr, 0, sizeof(vcmd_mgr_t));
	SubsysToVcmdCoreCfg(vcmd_mgr);//	SubsysToVcmdCoreCfg(subsys, subsys_num, vcmd_mgr);

	if (!vcmd_mgr->subsys_num)
		goto err;

	vcmd_mgr->platformdev = priv->pdev;

	vcmd_mgr->mem_vcmd.size = ALIGN_4K(SLOT_NUM_CMDBUF * SLOT_SIZE_CMDBUF);
	vcmd_mgr->mem_status.size = ALIGN_4K(SLOT_NUM_CMDBUF * SLOT_SIZE_STATUSBUF);
	vcmd_mgr->mem_regs.size = ALIGN_4K(vcmd_mgr->subsys_num * SLOT_SIZE_REGBUF);
#ifdef VCMD_ALLOC_MEM
	result = vcmd_init(vcmd_mgr);
	if (result)
		goto err;
#endif
	dev_ctx = vmalloc(sizeof(struct hantrovcmd_dev) * vcmd_mgr->subsys_num);
	if (!dev_ctx) {
		vcmd_klog(LOGLVL_ERROR, "Create dev ctx failed!\n");
		goto err;
	}

	vcmd_mgr->dev_ctx = dev_ctx;
	dev_ctx_init(vcmd_mgr);

	sema_init(&vcmd_mgr->isr_polling_sema, 1);

	vcmd_mgr->init_po = create_process_object();
	if (!vcmd_mgr->init_po)
		goto err;

	result = vcx_create_devnode(priv, &hantrovcmd_dec_fops);//	result = register_chrdev(vcmd_mgr->hantrovcmd_major, enc_dev_n, &hantrovcmd_dec_fops);
	if (result < 0) {
		//vcmd_klog(LOGLVL_ERROR, "vcx_vcmd_driver: unable to get major <%d>\n",
		//	vcmd_mgr->hantrovcmd_major);
        vcmd_klog(LOGLVL_ERROR, "vcx_vcmd_driver: unable to create node <%d>\n",	result);
		goto err0;
	} else if (result != 0) {
		/* this is for dynamic major */
		//vcmd_mgr->hantrovcmd_major = result;
	}
#ifdef VCMD_ALLOC_MEM
	result = vcmd_reserve_IO(vcmd_mgr);
	if (result < 0)
		goto err2;

	result = vcmd_config_modules(vcmd_mgr);
	if (result < 0)
		goto err2;

	vcmd_reset_asic(vcmd_mgr);
#endif

#ifdef SUPPORT_DBGFS
	/* for debugfs */
	if (_dbgfs_init((void *)vcmd_mgr))
		goto err2;
	_dbgfs_init_ctx((void *)vcmd_mgr, 0);
#endif
	/* get the IRQ line */

	vcmd_init_objs(vcmd_mgr);
	vcmd_init_nodes(vcmd_mgr);
	vcmd_init_free_obj_list(vcmd_mgr);

	/* create vcmd kthread, which need to be woken up */
	_vcmd_kthread_create(vcmd_mgr, "vcmd_kthread_vcd");

	/* read all registers of main-module for each dev
	 * for analyzing configuration in cwl
	 */
	cmda78_set_vcmd_mgr(VCMD_MGR_ID_DEC, vcmd_mgr);
	priv->priv = vcmd_mgr;
	vcmd_manager = vcmd_mgr;
#ifdef MAILBOX_CLIENT
	if (cmda78_gen_open_session(vcmd_mgr->init_po, R52_CORE_MASK_VDEC) < 0) {
		vcmd_klog(LOGLVL_ERROR, "Open session failed!\n");
		_vcmd_kthread_stop(vcmd_mgr);
		goto err2;
	}

	read_main_module_all_registers(vcmd_mgr);
#endif

	return 0;
err2:
    cdev_del(&priv->cdev);
err0:
	vcmd_release_IO(vcmd_mgr);

err:
#ifdef PCIE_EN
	_vcmd_pcie_cleanup(vcmd_mgr);
#else
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_vcmd);
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_status);
	_vcmd_free_mem(vcmd_mgr, &vcmd_mgr->mem_regs);
#endif

	free_process_object(vcmd_mgr->init_po);

	if (vcmd_mgr->dev_ctx)
		vfree(vcmd_mgr->dev_ctx);
	if (vcmd_mgr)
		vfree(vcmd_mgr);
	vcmd_klog(LOGLVL_ERROR, "module not inserted!\n");
	return -1;
}

/**
 * @brief suspend for vcmd driver power management
 */
//int vcmd_pm_suspend(void *_vcmd_mgr)
int vcmddec_pm_suspend(void *_vcmd_mgr)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;

	return 0;
}

/**
 * @brief resume for vcmd driver power management
 */
//int vcmd_pm_resume(void *_vcmd_mgr)
int vcmddec_pm_resume(void *_vcmd_mgr)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;

	return 0;
}

/**
 * @brief check whether the mmap in vcmd memory pools region
 * @param unsigned long start: the start address of mmap
 * @param unsigned long end: (end - start) is the size of mmap
 * @return int: 0 - in vcmd buffer pool region, other - not in
 */
#define IN_MEM_RANGE(mem, s, e) ((s) >= (mem).pa && \
								 (e) <= ((mem).pa + (mem).size))
int in_vcmd_memory_region(void *_vcmd_mgr, unsigned long start, unsigned long end)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)_vcmd_mgr;
	int matched = 0;

	matched |= IN_MEM_RANGE(vcmd_mgr->mem_status, start, end);
	matched |= IN_MEM_RANGE(vcmd_mgr->mem_vcmd, start, end);
	matched |= IN_MEM_RANGE(vcmd_mgr->mem_regs, start, end);

	if (matched == 0)
		return -1;

	return 0;
}

static int hantrovcmd_dec_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct vcmd_priv_ctx *ctx = (struct vcmd_priv_ctx *)filp->private_data;
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)ctx->vcmd_mgr;
	size_t size = vma->vm_end - vma->vm_start;
	int ret = 0;
	unsigned long start = 0, end = 0;
	int matched = 0;

	vcmd_klog(LOGLVL_FLOW, "%s %08lx-%08lx -> %08lx, %s\n", __func__,
		   (long)(vma->vm_pgoff << PAGE_SHIFT),
		   (long)(vma->vm_pgoff << PAGE_SHIFT) + (int)(size),
		   (long)vma->vm_start,
		   (filp->f_flags & O_SYNC) ? "uncached" : "cached");

	start = vma->vm_pgoff << PAGE_SHIFT;
	end = start + size;

	matched |= IN_MEM_RANGE(vcmd_mgr->mem_status, start, end);
	matched |= IN_MEM_RANGE(vcmd_mgr->mem_vcmd, start, end);
	matched |= IN_MEM_RANGE(vcmd_mgr->mem_regs, start, end);

	if (matched == 0) {
		vcmd_klog(LOGLVL_ERROR, "Invalid adress %08lx-%08lx\n", start, end);
		return -EINVAL;
	}

	// support only uncached mode
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	vma->vm_ops = &hantrovcmd_dec_vm_ops;

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
		  vma->vm_page_prot);
	if (ret != 0) {
		vcmd_klog(LOGLVL_ERROR, "remap_pfn_range() failed.\n");
		goto err_out;
	}

	return 0;

err_out:
	return ret;
}
