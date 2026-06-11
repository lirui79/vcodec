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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/ioctl.h>
#include <linux/moduleparam.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#ifdef PCIE_EN
#include <linux/pci.h>
#else
#if (KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE)
#include <linux/dma-contiguous.h>
#else
#include <linux/dma-map-ops.h>
#endif
#include <linux/mod_devicetable.h>
#include <linux/dma-buf.h>
#endif
#include <linux/uaccess.h>
#include <linux/ioport.h>
#include <linux/kthread.h>
#include <asm/irq.h>
#include <linux/vmalloc.h>

#include "arbiter_drv.h"
#include "arbiter_drv_priv.h"
#include "arbiterswhwregister.h"
#include "arbiter_common.h"
#include "arbiter_cfg.h"

#ifdef PCIE_EN
/*---------------------------------------------------------------
 * Macros related to PCIe Platform Config
 *---------------------------------------------------------------
 */
#ifdef EMU
  #define VCMD_BUF_POOL_OFFSET      0x4800000

  #define PCI_VENDOR_ID_HANTRO      0x1d9b//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0xface// 0x7011

  /* Base address got control register */
  #define PCI_H2_BAR              2

  /* Base address DDR register */
  #define PCI_DDR_BAR             4

#else //EMU

  #define VCMD_BUF_POOL_OFFSET      0x800000
#ifdef PLATFORM_GEN7
  #define PCI_VENDOR_ID_HANTRO      0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x9014// 0x7011

  /* Base address got control register */
  #define PCI_H2_BAR              2

  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#else
  #define PCI_VENDOR_ID_HANTRO      0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x8014// 0x7011

  /* Base address got control register */
  #define PCI_H2_BAR              4

  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#endif //PLATFORM_GEN7
#endif //EMU
#endif //PCIE_EN

arbiter_mgr_t arb_manager;

static irqreturn_t arbiter_isr(int irq, void *_dev);
static int arbiter_reset_process(struct arbiter_dev *dev);

/**
 * @brief a hook function to do further process for axife flush failed
 */
static void hook_arbiter_process(void *dev) {
	pr_err("Arbiter: axife flush failed, need to do further process!");
}


/**
 * @brief to check arbiter device's actions which need kthread to process
 * @return struct arbiter_dev *: NULL: no actions; others: have actions
 */
static struct arbiter_dev *arbiter_kthread_actions(arbiter_mgr_t *arb_mgr)
{
	int i;
	struct arbiter_dev *dev = NULL;

	for (i = 0; i < arb_mgr->arb_num; i++) {
		dev = &arb_mgr->arb_dev[i];
		if (dev->reset_flag == 1)
			break;
	}

	return dev;
}

/**
 * @brief arbiter kernel thread main function
 */
static int arbiter_kthread_fn(void *data)
{
	arbiter_mgr_t *arb_mgr = (arbiter_mgr_t *)data;
	struct arbiter_dev *dev = NULL;

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(arb_mgr->kthread_waitq,
				(arb_mgr->stop_kthread == 1) || \
				(dev = arbiter_kthread_actions(arb_mgr)))) {
			pr_err("Arbiter: %s: signaled!!!\n", __func__);
			return -ERESTARTSYS;
		}

		if (dev->reset_flag == 1) {
			dev->reset_flag = 0;
			arbiter_reset_process(dev);
			continue;
		}
	}

	return 0;
}

/**
 * @brief wake up arbiter kernel thread
 */
static void arbiter_kthread_wakeup(arbiter_mgr_t *arb_mgr)
{

	if (IS_ERR(arb_mgr->kthread))
		return;

	wake_up_interruptible_all(&arb_mgr->kthread_waitq);
}

/**
 * @brief create kernel thread for arbiter driver
 */
static void arbiter_kthread_create(arbiter_mgr_t *arb_mgr)
{
	arb_mgr->stop_kthread = 0;
	init_waitqueue_head(&arb_mgr->kthread_waitq);
	arb_mgr->kthread =
		kthread_run(arbiter_kthread_fn, (void *)arb_mgr, "arbiter_kthread");
	if (IS_ERR(arb_mgr->kthread)) {
		pr_err("Arbiter: create arbiter kthread failed\n");
		return;
	}
}

/**
 * @brief stop kernel thread arbiter driver
 */
static void arbiter_kthread_stop(arbiter_mgr_t *arb_mgr)
{
	if (!IS_ERR(arb_mgr->kthread)) {
		arb_mgr->stop_kthread = 1;
		kthread_stop(arb_mgr->kthread);
		arb_mgr->kthread = NULL;
	}
}

#ifdef PCIE_EN
/**
 * @brief Initialize PCI Hw access
 */
static int pcie_init(arbiter_mgr_t *arb_mgr)
{
	/* PCI device structure. */
	struct pci_dev *pci_handler = NULL;
	/* PCI base register address (Hardware address) */
	unsigned long pci_reg_base;
	/* Base register address Length */
	u32 pci_reg_len;

	pci_handler = pci_get_device(PCI_VENDOR_ID_HANTRO,
								PCI_DEVICE_ID_HANTRO, pci_handler);
	if (!pci_handler) {
		pr_err("Arbiter: Init: Hardware not found.\n");
		return -1;
	}

	if (pci_enable_device(pci_handler) < 0) {
		pr_err("Arbiter: Init: Device not enabled.\n");
		return -1;
	}
	arb_mgr->pcie_dev = pci_handler;

	pci_reg_base = pci_resource_start(pci_handler, PCI_H2_BAR);
	if (pci_reg_base < 0) {
		pr_err("Arbiter: Init: Base Address not set.\n");
		return -1;
	}
	arb_mgr->reg_base_offset = pci_reg_base;
	pr_info("Arbiter: Base hw val 0x%lx\n", pci_reg_base);

	pci_reg_len = pci_resource_len(pci_handler, PCI_H2_BAR);
	pr_info("Arbiter: Base hw len 0x%x\n", pci_reg_len);

	return 0;
}

static void pcie_cleanup(arbiter_mgr_t *arb_mgr)
{
	if (arb_mgr->pcie_dev) {
		pci_disable_device(arb_mgr->pcie_dev);
		arb_mgr->pcie_dev = NULL;
	}
}
#endif

/**
 * @brief enable one of the masters
 * @param u32 id: the master id
 */
static void arbiter_master_online(struct arbiter_dev *dev, u32 id)
{
	unsigned long flags;
	u32 reg_val;
	const void *hwregs = (const void *)dev->hwregs;

	reg_val = (u32)arbiter_read_reg(hwregs, ARB_MASTER_REG_OFFSET(id));
	reg_val |= ((dev->master_cfg[id].enable & 0x1) << 8);
	spin_lock_irqsave(&dev->spinlock, flags);
	arbiter_write_reg(hwregs, ARB_MASTER_REG_OFFSET(id), reg_val);
	if (dev->master_cfg[id].enable == ENABLE)
		dev->master_cfg[id].enable = ENABLE;
	else
		dev->master_cfg[id].enable = DISABLE;
	spin_unlock_irqrestore(&dev->spinlock, flags);
}

/**
 * @brief offline the master
 */
static void arbiter_master_offline(struct arbiter_dev *dev, u32 id)
{
	const void *hwregs = (const void *)dev->hwregs;
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	arbiter_write_reg(hwregs, ARB_MASTER_REG_OFFSET(id), 0x0);
	dev->master_cfg[id].enable = DISABLE;
	spin_unlock_irqrestore(&dev->spinlock, flags);
}

/**
 * @brief enable arbiter
 */
static void arbiter_enable(struct arbiter_dev *dev)
{
	unsigned long flags;
	const void *hwregs = (const void *)dev->hwregs;

	spin_lock_irqsave(&dev->spinlock, flags);
	arbiter_write_register_value(hwregs, dev->reg_mirror,
		HWIF_ARBITER_ARB_ENABLE, dev->enable & 0x1);
	if (dev->enable == ENABLE)
		dev->enable = ENABLE;
	else
		dev->enable = DISABLE;
	spin_unlock_irqrestore(&dev->spinlock, flags);
}

/**
 * @brief reset external IPs(VCE, AXIFE,...) and dedicate IPs(VCMD, MMU)
 */
static void arbiter_reset(struct arbiter_dev *dev)
{
	const void *hwregs = (const void *)dev->hwregs;
	u32 reg_val;
	u32 winer_id;
	u32 offset;

	/* write 1 to reset*/
	reg_val = (u32)arbiter_read_reg(hwregs, 4 * 4);
	reg_val |= (0xf << 17);
	arbiter_write_reg(hwregs, 4 * 4, reg_val);
	/* write 0 to release the reset signal */
	reg_val &= ~(0xf << 17);
	arbiter_write_reg(hwregs, 4 * 4, reg_val);

	/* reset the timeout or bus hack master's dedicate IPs */
	winer_id = (reg_val >> 9) & 0x1F;
	offset = ARB_MASTER_REG_OFFSET(winer_id);
	reg_val = (u32)arbiter_read_reg(hwregs, offset);
	reg_val |= (0x1 << 11);
	arbiter_write_reg(hwregs, offset, reg_val);
}

/**
 * @brief arbiter reset process when err irq received
 */
static int arbiter_reset_process(struct arbiter_dev *dev)
{
	int ret;
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	// flush axife for AXI clean
	if (dev->mod_info[SUB_MOD_AXIFE].hwregs)
		ret = axife_flush(dev->mod_info[SUB_MOD_AXIFE].hwregs);
	else
		ret = -1;
	if (ret < 0) {
		spin_unlock_irqrestore(&dev->spinlock, flags);
		hook_arbiter_process(dev);
		return ret;
	}
	arbiter_reset(dev);
	spin_unlock_irqrestore(&dev->spinlock, flags);
	arbiter_enable(dev);

	return 0;
}

/**
 * @brief get a winner of masters for arbiter
 * @return -1: no winner; others: winner id;
 */
static int arbiter_get_winner(struct arbiter_dev *dev)
{
	int id = -1, has_winner;

	has_winner = arbiter_get_register_value((const void *)dev->hwregs,
					dev->reg_mirror, HWIF_ARBITER_WINER_EXIST);
	if (has_winner == 1)
		id = arbiter_get_register_value((const void *)dev->hwregs,
					dev->reg_mirror, HWIF_ARBITER_WINER_ID);
	return id;
}

/**
 * @brief get arbiter work state
 */
static u32 arbiter_get_work_state(struct arbiter_dev *dev)
{
	u32 state;

	state = arbiter_get_register_value((const void *)dev->hwregs,
					dev->reg_mirror, HWIF_ARBITER_FSM_STATE);

	return state;
}

/**
 * @brief get arbiter device if the  arb_id and master_id is valid
 * @return struct arbiter_dev *: NULL: the arb_id or the master id is not valid,
 *         get device failed; others: get a valid device
*/
static struct arbiter_dev *_get_dev(arbiter_mgr_t *arb_mgr, u32 arb_id, u32 master_id)
{
	struct arbiter_dev *dev;

	if (arb_id >= arb_mgr->arb_num) {
		pr_err("Arbiter: the core is not valid; supported arbiter num "
			"is %d, but the arb_id is %d\n", arb_mgr->arb_num, arb_id);
		return NULL;
	}

	dev = &arb_mgr->arb_dev[arb_id];

	if (master_id != 0xFFFF && master_id >= dev->master_num) {
		pr_err("Arbiter: the master is not valid; HW maximum support masters "
			"is %d, but the master_id is %d\n", dev->master_num, master_id);
		return NULL;
	}

	return dev;
}

/**
 * @brief config master
 */
static int arbiter_master_config(struct arbiter_dev *dev, u32 id)
{
	unsigned long flags;
	volatile u8 *hwregs = dev->hwregs;
	u32 params = MASTER_PARAMS(dev->master_cfg[id].weight,
							  dev->master_cfg[id].urgent,
							  dev->master_cfg[id].bw_overflow);

	/* config master params
	 * 0x40: reg16 for master0
	 */
	spin_lock_irqsave(&dev->spinlock, flags);
	arbiter_write_reg((const void *)hwregs, ARB_MASTER_REG_OFFSET(id), params);
	spin_unlock_irqrestore(&dev->spinlock, flags);

	return 0;
}

/**
 * @brief config arbiter
 */
static void arbiter_config(struct arbiter_dev *dev)
{
	const void *hwregs = (const void *)dev->hwregs;
	u32 *reg_mirror = dev->reg_mirror;
	unsigned long flags;

	spin_lock_irqsave(&dev->spinlock, flags);
	/* 0x10: reg4[3-7] */
	arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_TIME_WINDOW_EXP,
							dev->time_window_exp);
	//arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_ARB_ENABLE,
	//						arb_dev->enable);
	if (dev->enable == DISABLE) {
		/* reg4[2]*/
		arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_BUS_HACK_CHECK_EN, 1);
		/* reg4[1]*/
		arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_TIMEOUT_CHECK_EN, 1);
		/* 0x18: reg6[1] */
		arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_IRQ_BUS_HACK_EN, 1);
		/* reg6[0] */
		arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_IRQ_FE_TIMEOUT_EN, 1);
		/* 0x1C: reg7 */
		arbiter_set_reg_mirror(reg_mirror, HWIF_ARBITER_TIMEOUT_CYCLES, 0x40000000);

		arbiter_write_reg(hwregs, 0x18, reg_mirror[0x18 / 4]);
		arbiter_write_reg(hwregs, 0x1C, reg_mirror[0x1C / 4]);
	}

	arbiter_write_reg(hwregs, 0x10, reg_mirror[0x10 / 4]);
	spin_unlock_irqrestore(&dev->spinlock, flags);
}

/**
 * @brief check any arbiter if received interrupt
 */
static int arbiter_check_irq(arbiter_mgr_t *arb_mgr, u32 *irq_status)
{
	int i;
	struct arbiter_dev *dev;

	for (i = 0; i < arb_mgr->arb_num; i++) {
		dev = &arb_mgr->arb_dev[i];
		if (dev->irq_status & ARB_IRQ_ERR_MASK) {
			irq_status[i] = dev->irq_status;
			dev->irq_status = 0;
			return 1;
		}
	}

	return 0;
}

/**
 * @brief check arbiter irq status in IRQ mode
 */
static u32 arbiter_check_irq_status(arbiter_mgr_t *arb_mgr, u32 *irq_status)
{
	if (arb_mgr->waitq_timeout == 0) {
		if (wait_event_interruptible(arb_mgr->irq_check_waitq,
					arbiter_check_irq(arb_mgr, irq_status))) {
				pr_err("Arbiter: %s: signaled!!!\n", __func__);
				return -ERESTARTSYS;
		}
	} else {
		// timeout after (arb_mgr->waitq_timeout) ms
		if (wait_event_interruptible_timeout(arb_mgr->irq_check_waitq,
					arbiter_check_irq(arb_mgr, irq_status),
					msecs_to_jiffies(arb_mgr->waitq_timeout)) == 0) {
				pr_err("Arbiter: %s: timeout or signaled!!!\n", __func__);
				return -ERESTARTSYS;
		}
	}

	return 0;
}

/**
 * @brief check arbiter irq status in polling mode
 */
static u32 arbiter_check_irq_status_polling(arbiter_mgr_t *arb_mgr)
{
	int i;
	struct arbiter_dev *dev;

	for (i = 0; i < arb_mgr->arb_num; i++) {
		dev = &arb_mgr->arb_dev[i];
		arbiter_isr(dev->irq, (void *)arb_mgr);
	}

	return 0;
}

/**
 * @brief ioctl function of arbiter driver.
 */
static long arbiter_ioctl(struct file *filp, unsigned int cmd,
				 unsigned long arg)
{
	arbiter_mgr_t *arb_mgr = (arbiter_mgr_t *)filp->private_data;
	struct arbiter_dev *dev;
	int err = 0;

	if (cmd != ARBITER_IOCH_CHECK_IRQ_STATUS_POLLING)
		pr_info("Arbiter: ioctl cmd 0x%08x\n", cmd);

	/*
	 * extract the type and number bitfields, and don't encode
	 * wrong cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != HANTRO_IOC_MAGIC)
		return -ENOTTY;
	if ((_IOC_TYPE(cmd) == HANTRO_IOC_MAGIC &&
		 _IOC_NR(cmd) > HANTRO_IOC_MAXNR))
		return -ENOTTY;

	/*
	 * the direction is a bitmask, and VERIFY_WRITE catches R/W
	 * transfers. `Type' is user-oriented, while
	 * access_ok is kernel-oriented, so the concept of "read" and
	 * "write" is reversed
	 */
	if (_IOC_DIR(cmd) & _IOC_READ)
#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
#else
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
#endif
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
#if KERNEL_VERSION(5, 0, 0) <= LINUX_VERSION_CODE
		err = !access_ok((void *)arg, _IOC_SIZE(cmd));
#else
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
#endif
	if (err)
		return -EFAULT;

	switch (cmd) {
	case ARBITER_IOCH_ARB_NUM_GET: {
		__put_user(arb_mgr->arb_num, (u32 __user *)arg);
		break;
	}
	case ARBITER_IOCH_MASTER_OFFLINE: {
		u32 arb_id, master_id;
		struct io_arb_master_offline offline_info;

		err = copy_from_user(&offline_info, (struct io_arb_master_offline __user *)arg,
				sizeof(struct io_arb_master_offline));
		if (err < 0) {
			pr_err("Arbiter: ARBITER_IOCH_MASTER_OFFLINE falied, returned %i\n", err);
			return -EFAULT;
		}
		arb_id = offline_info.arb_id;
		master_id = offline_info.master_id;
		dev = _get_dev(arb_mgr, arb_id, master_id);
		if (!dev)
			return -EFAULT;

		if (dev->master_cfg[master_id].enable == ENABLE)
			arbiter_master_offline(dev, master_id);

		break;
	}
	case ARBITER_IOCH_ARB_PARAMS_GET: {
		struct io_arb_params params;

		err = copy_from_user(&params, (struct io_arb_params __user *)arg,
				 sizeof(struct io_arb_params));
		if (err) {
			pr_err("Arbiter: ARBITER_IOCH_COMMON_PARAMS_GET: copy_from_user failed, returned %i\n", err);
			return -EFAULT;
		}

		dev = _get_dev(arb_mgr, params.arb_id, 0xFFFF);
		if (!dev)
			return -EFAULT;

		params.enable = dev->enable;
		params.time_window_exp = dev->time_window_exp;
		params.master_num = dev->master_num;
		err = copy_to_user((struct io_arb_params __user *)arg, &params,
				 sizeof(struct io_arb_params));
		if (err) {
			pr_err("Arbiter: ARBITER_IOCH_COMMON_PARAMS_GET: copy_to_user failed, returned %i\n", err);
			return -EFAULT;
		}

		break;
	}
	case ARBITER_IOCH_ARB_PARAMS_SET: {
		struct io_arb_params params;
		u32 arb_en;

		err = copy_from_user(&params, (struct io_arb_params __user *)arg,
				 sizeof(struct io_arb_params));
		if (err) {
			pr_err("Arbiter: ARB_CFG_PARAMS_SET failed, returned %i\n", err);
			return -EFAULT;
		}
		dev = _get_dev(arb_mgr, params.arb_id, 0xFFFF);
		if (!dev)
			return -EFAULT;
		arb_en = dev->enable;

		dev->time_window_exp = params.time_window_exp;
		dev->enable = params.enable;
		if (params.enable == DISABLE && arbiter_get_work_state(dev) ==
				ARB_WORK_STATE_ACK) {
			pr_err("Arbiter: one master is under service, can't disable arbiter\n");
			return -1;
		} else {
			arbiter_config(dev);
			if ((arb_en == DISABLE && params.enable == ENABLE) ||
				(arb_en == ENABLE && params.enable == DISABLE))
				arbiter_enable(dev);
		}

		break;
	}
	case ARBITER_IOCH_MASTER_PARAMS_SET: {
		struct io_master_params params;
		u32 id, mst_en;

		err = copy_from_user(&params, (struct io_master_params __user *)arg,
				 sizeof(struct io_master_params));
		if (err) {
			pr_err("Arbiter: ARB_MST_CFG_PARAMS_SET failed, returned %i\n", err);
			return -EFAULT;
		}

		dev = _get_dev(arb_mgr, params.arb_id, params.master_id);
		if (!dev)
			return -EFAULT;

		id = params.master_id;
		mst_en = dev->master_cfg[id].enable;

		dev->master_cfg[id].enable = params.enable;
		dev->master_cfg[id].weight = params.weight;
		dev->master_cfg[id].urgent = params.urgent;
		dev->master_cfg[id].bw_overflow = params.bw_overflow;
		arbiter_master_config(dev, id);
		if ((mst_en == DISABLE && params.enable == ENABLE) ||
			(mst_en == ENABLE && params.enable == DISABLE))
			arbiter_master_online(dev, id);

		break;
	}
	case ARBITER_IOCH_MASTER_PARAMS_GET: {
		struct io_master_params params;
		u32 id;

		err = copy_from_user(&params, (struct io_master_params __user *)arg,
				 sizeof(struct io_master_params));
		if (err) {
			pr_err("Arbiter: ARB_MST_CFG_PARAMS_GET: copy_from_user failed, returned %i\n", err);
			return -EFAULT;
		}

		dev = _get_dev(arb_mgr, params.arb_id, params.master_id);
		if (!dev)
			return -EFAULT;

		id = params.master_id;
		params.enable = dev->master_cfg[id].enable;
		params.weight = dev->master_cfg[id].weight;
		params.urgent = dev->master_cfg[id].urgent;
		params.bw_overflow = dev->master_cfg[id].bw_overflow;

		err = copy_to_user((struct io_master_params __user *)arg, &params,
				 sizeof(struct io_master_params));
		if (err) {
			pr_err("Arbiter: ARB_MST_CFG_PARAMS_GET: copy_to_user failed, returned %i\n", err);
			return -EFAULT;
		}

		break;
	}
	case ARBITER_IOCH_CHECK_IRQ_STATUS: {
		struct io_wait_irq wait_irq;

		err = copy_from_user(&wait_irq, (struct io_wait_irq __user *)arg,
				sizeof(struct io_wait_irq));
		if (err < 0) {
			pr_err("Arbiter: ARBITER_IOCH_CHECK_IRQ_STATUS: copy_from_user failed, returned %i\n", err);
			return -EFAULT;
		}

		arb_mgr->waitq_timeout = wait_irq.wait_timeout;
		arbiter_check_irq_status(arb_mgr, wait_irq.irq_status);
		err = copy_to_user((struct io_wait_irq __user *)arg, &wait_irq, sizeof(struct io_wait_irq));
		if (err) {
			pr_err("Arbiter: ARBITER_IOCH_CHECK_IRQ_STATUS copy_to_user failed, returned %i\n", err);
			return -EFAULT;
		}
		break;
	}
	case ARBITER_IOCH_CHECK_IRQ_STATUS_POLLING: {
		arbiter_check_irq_status_polling(arb_mgr);

		break;
	}
	}

	return 0;
}

/**
 * @brief interrupt service routine of arbiter driver.
 */

static irqreturn_t arbiter_isr(int irq, void *_dev)
{
	unsigned int handled = 0;
	arbiter_mgr_t *arb_mgr = (arbiter_mgr_t *)_dev;
	struct arbiter_dev *dev = NULL;
	volatile u8 *hwregs;
	u32 irq_status = 0;
	unsigned long flags;
	int i;

	for (i = 0; i < arb_mgr->arb_num; i++) {
		if (arb_mgr->arb_dev[i].irq == irq) {
			dev = &arb_mgr->arb_dev[i];
			break;
		}
	}

	if (dev == NULL)
		return IRQ_HANDLED;

	hwregs = dev->hwregs;

	irq_status = arbiter_read_reg((const void *)hwregs,
				   ARB_REGISTER_INT_STATUS_OFFSET);

	spin_lock_irqsave(&dev->spinlock, flags);
	if (irq_status == 0) {
		//no interrupt source from dev
		//pr_info("Arbiter: %s , irq_status is zero!\n", __func__);
		spin_unlock_irqrestore(&dev->spinlock, flags);
		return IRQ_HANDLED;
	}
	arbiter_write_reg((const void *)hwregs, ARB_REGISTER_INT_STATUS_OFFSET,
					irq_status);

	dev->irq_status = irq_status;
	dev->reg_mirror[ARB_REGISTER_INT_STATUS_OFFSET / 4] = irq_status;

	if (arbiter_get_winner(dev) == -1) {
		pr_info("Arbiter: received irq, but there is not a winner\n");
		spin_unlock_irqrestore(&dev->spinlock, flags);
		return IRQ_HANDLED;
	}
	pr_info("Arbiter: %s, irq_status is 0x%8x!\n", __func__, irq_status);

	if ((irq_status & ARB_IRQ_BUS_HACK) || (irq_status & ARB_IRQ_FE_TIMEOUT)) {
		dev->reset_flag = 1;
		arbiter_kthread_wakeup(arb_mgr);
		wake_up_interruptible_all(&arb_mgr->irq_check_waitq);
		handled++;
	}
	spin_unlock_irqrestore(&dev->spinlock, flags);
	if (!handled)
		pr_info("Arbiter: IRQ received, but not hantro's!\n");

	return IRQ_HANDLED;
}

/**
 * @brief reserve io resource
 * @param addr_t pa: the physical adress of reserve io,
 * @param size_t sz: the size of reserve io
 * @return volatile u8 *: the virtual address of io remap
 */
static volatile u8 * reserve_IO(addr_t pa, size_t sz)
{
	volatile u8 *va = NULL;

	if (!request_mem_region(pa, sz, "arbiter_driver")) {
		pr_err("Arbiter: request memory region failed!\n");
		return NULL;
	}
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
	va = (volatile u8 __force *)ioremap_nocache(pa, sz);
#else
	va = (volatile u8 __force *)ioremap(pa, sz);
#endif

	return va;
}

static void release_IO(addr_t pa, size_t sz, volatile u8 **va)
{
	iounmap((void __iomem *)(*va));
	*va = NULL;
	release_mem_region(pa, sz);
}


/**
 * @brief reserve IO resource for arbiter
 */
static int arbiter_reserve_IO(arbiter_mgr_t *arb_mgr)
{
	u32 hwid;
	int i = 0, j;
	addr_t arb_pa, mod_pa;
	size_t arb_sz, mod_sz;
	struct arbiter_dev *dev;

	pr_info("Arbiter: %s: arbiter core num is %d\n", __func__, arb_mgr->arb_num);

	for (j = 0; j < arb_mgr->arb_num; j++) {
		dev = &arb_mgr->arb_dev[j];

		arb_pa = dev->reg_base;
		arb_sz = dev->io_size;
		dev->hwregs = reserve_IO(arb_pa, arb_sz);
		if (!dev->hwregs) {
			pr_err("Arbiter: arbiter[%d]failed to ioremap HW regs\n", j);
			return -1;
		}
		/* reserve sub-module io resource when needed */
		for (i = 0; i < SUB_MOD_MAX; i++) {
			if (dev->mod_info[i].reg_base == 0)
				continue;
			mod_pa = dev->mod_info[i].reg_base;
			mod_sz = dev->mod_info[i].io_size;
			dev->mod_info[i].hwregs = reserve_IO(mod_pa, mod_sz);
			if (!dev->mod_info[i].hwregs) {
				pr_err("Arbiter: arbiter[%d]failed to ioremap sub-module[%d] HW regs\n", j, i);
				return -1;
			}
		}

		/*read hwid and check validness and store it*/
		hwid = (u32)ioread32((void __iomem *)dev->hwregs + 0x0);
		pr_info("Arbiter: %s: arb_dev.hwregs=0x%p\n",
				__func__, dev->hwregs);
		pr_info("Arbiter: hwid=0x%08x\n", hwid);

		if (((hwid >> 16) & 0xFFFF) != ARB_HW_ID) {
			pr_err("Arbiter: HW not found at 0x%lx\n", arb_pa);
			return -1;
		}

		dev->hw_version_id = hwid;

		pr_info("Arbiter: HW at base <0x%llx> with ID <0x%08x>\n",
			(unsigned long long)arb_pa, hwid);
	}

	return 0;
}

static void arbiter_fill_master_ctx(struct arbiter_dev *dev)
{
	u32 num;
	int i;

	/* 0xC: reg3[0-7] */
	num = arbiter_get_register_value((const void *)dev->hwregs,
						dev->reg_mirror,
						HWIF_ARBITER_HW_MASTERNUM);
	dev->master_num = num;

	dev->master_cfg = vmalloc(sizeof(struct master_config) * dev->master_num);
	memset(dev->master_cfg, 0, sizeof(struct master_config) * dev->master_num);
	for (i = 0; i < dev->master_num; i++) {
		dev->master_cfg[i].enable = MASTER_ENABLE;
		dev->master_cfg[i].weight = MASTER_WEIGHT;
		dev->master_cfg[i].urgent = MASTER_URGENT;
		dev->master_cfg[i].bw_overflow = MASTER_BW_OVERFLOW;
	}
}

/**
 * @brief release arbiter IO resource
 */
static void arbiter_release_IO(arbiter_mgr_t *arb_mgr)
{
	int i, j;
	struct arbiter_dev *dev;

	for (j = 0; j < arb_mgr->arb_num; j++) {
		dev = &arb_mgr->arb_dev[j];

		for (i = 0; i < SUB_MOD_MAX; i++) {
			if (!dev->mod_info[i].hwregs)
				continue;
			release_IO(dev->mod_info[i].reg_base, dev->mod_info[i].io_size,
					&dev->mod_info[i].hwregs);
		}

		if (dev->hwregs)
			release_IO(dev->reg_base, dev->io_size, &dev->hwregs);
	}
}

/**
 * @brief if irq is available, requset irq for arbiter driver
 */
static int arbiter_requst_irq(arbiter_mgr_t *arb_mgr)
{
	int i, ret;
	struct arbiter_dev *dev;

	for (i = 0; i < arb_mgr->arb_num; i++) {
		dev = &arb_mgr->arb_dev[i];

		if (dev->irq < 0) {
			pr_info("Arbiter: arbiter[%d] IRQ not in use!\n", i);
			continue;
		}

		ret = request_irq(dev->irq,
				arbiter_isr,
#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
				SA_INTERRUPT | SA_SHIRQ,
#else
				IRQF_SHARED,
#endif
				"arbiter_dev", (void *)arb_mgr);
		if (ret == -EINVAL) {
			pr_err("Arbiter: arbiter[%d] bad irq number or handler\n", i);
			return ret;
		} else if (ret == -EBUSY) {
			pr_err("Arbiter: arbiter[%d] IRQ <%d> busy, change your config\n",
				i, dev->irq);
			return ret;
		}
	}

	return 0;
}

/**
 * @brief open arbiter drv device.
 */
static int arbiter_open(struct inode *inode, struct file *filp)
{
	filp->private_data = (void *)&arb_manager;
	pr_info("open arbiter drv device!\n");
	return 0;
}

/**
 * @brief release arbiter drv device.
 */
static int arbiter_release(struct inode *inode, struct file *file)
{
	pr_info("release arbiter drv device!\n");
	return 0;
}

/**
 * Initilize the arbiter device
 */
static int dev_ctx_init(arbiter_mgr_t *arb_mgr)
{
	int i, j = 0, sub_mod;
	struct arbiter_dev *dev = arb_mgr->arb_dev;
	struct arbiter_config *cfg;

	for (i = 0; i < arb_mgr->arb_num; i++) {
		dev = &arb_mgr->arb_dev[i];
		cfg = arb_mgr->cfg[i];

		dev->arb_id = i;

		dev->time_window_exp = cfg->time_window_exp;
		dev->irq = cfg->arb_irq;
		dev->reg_base = cfg->arb_base_addr + arb_mgr->reg_base_offset;
		dev->io_size = cfg->io_size;
		dev->enable = DISABLE;

		spin_lock_init(&dev->spinlock);
		dev->reset_flag = 0;

		for (j = 0; j < SUB_MOD_MAX; j++) {
			sub_mod = cfg->sub_mod_cfg[j].sub_mod;
			dev->mod_info[sub_mod].io_size = cfg->sub_mod_cfg[j].io_size;
			dev->mod_info[sub_mod].reg_base = dev->reg_base +
										cfg->sub_mod_cfg[j].reg_off;
		}
	}

	return 0;
}

/**
 * @brief restore the valid arbiter_cfg to arbiter manager
 */
static int arbiter_cfg_restore(arbiter_mgr_t *arb_mgr)
{
	int i, j = 0, num;

	num = ARRAY_SIZE(arbiter_cfg);
	for (i = 0; i < num; i++) {
		if (arbiter_cfg[i].arb_base_addr == 0xFFFF)
			continue;
		arb_mgr->cfg[j++] = &arbiter_cfg[i];
	}
	if (j == 0) {
		pr_err("Arbiter: there is not a valid arbiter cfg!\n");
		return -1;
	}
	arb_mgr->arb_num = j;
	if (arb_mgr->arb_num > MAX_ARBITER_NUM) {
		pr_err("Arbiter: config too many arbiters in arbiter_cfg!\n");
		return -1;
	}

	return 0;
}

/**
 * @brief VFS methods and VM operations
 */
static const struct file_operations arbiter_fops = {
	.owner = THIS_MODULE,
	.open = arbiter_open,
	.release = arbiter_release,
	.unlocked_ioctl = arbiter_ioctl,
	.fasync = NULL,
};

/**
 * @brief initialize the arbiter driver
 * @return int __init: 0: successful; other: failed.
 */
static int __init arbiter_init(void)
{
	int ret, i, j;
	arbiter_mgr_t *arb_mgr = &arb_manager;
	struct arbiter_dev *dev;

	memset(arb_mgr, 0, sizeof(arbiter_mgr_t));

	ret = register_chrdev(arb_mgr->arb_dev_major, "arbiter_dev", &arbiter_fops);
	if (ret < 0) {
		pr_info("Arbiter: unable to get major <%d>\n",
			arb_mgr->arb_dev_major);
		return ret;
	} else if (ret != 0) {
		/* this is for dynamic major */
		arb_mgr->arb_dev_major = ret;
	}

#ifdef PCIE_EN
	ret = pcie_init(arb_mgr);
	if (ret < 0)
		goto err1;
#endif

	ret = arbiter_cfg_restore(arb_mgr);
	if (ret < 0)
		goto err1;
	dev = vmalloc(sizeof(struct arbiter_dev) * arb_mgr->arb_num);
	memset(dev, 0, sizeof(struct arbiter_dev) * arb_mgr->arb_num);
	arb_mgr->arb_dev = dev;

	init_waitqueue_head(&arb_mgr->irq_check_waitq);
	ret = dev_ctx_init(arb_mgr);
	if (ret < 0)
		goto err1;
	ret = arbiter_reserve_IO(arb_mgr);
	if (ret < 0)
		goto err;
	ret = arbiter_requst_irq(arb_mgr);
	if (ret < 0)
		goto err;
	for (i = 0; i < arb_mgr->arb_num; i++) {
		arbiter_fill_master_ctx(&dev[i]);
		arbiter_config(&dev[i]);
		for (j = 0; j < dev[i].master_num; j++)
		arbiter_master_config(&dev[i], j);
	}

	/* create arbiter kthread, which need to be woken up */
	arbiter_kthread_create(arb_mgr);

	return 0;

err:
	arbiter_release_IO(arb_mgr);
err1:
	unregister_chrdev(arb_mgr->arb_dev_major, "arbiter_dev");
#ifdef PCIE_EN
	pcie_cleanup(arb_mgr);
#endif

	if (arb_mgr->arb_dev) {
		vfree(arb_mgr->arb_dev);
		arb_mgr->arb_dev = NULL;
	}

	return ret;
}

/**
 * @brief cleanup arbiter driver
 */
static void __exit arbiter_cleanup(void)
{
	int i, j;
	arbiter_mgr_t *arb_mgr = &arb_manager;
	struct arbiter_dev *dev;

	arbiter_kthread_stop(arb_mgr);
	for (j = 0; j < arb_mgr->arb_num; j++) {
		dev = &arb_mgr->arb_dev[j];
		if (dev->irq != -1)
			free_irq(dev->irq, (void *)dev);

		if (dev->hwregs) {
			for (i = 0; i < dev->master_num; i++)
				arbiter_master_offline(dev, i);
			arbiter_write_reg((const void *)dev->hwregs, 4 * 4, 0x0);
		}

		if (dev->master_cfg) {
			vfree(dev->master_cfg);
			dev->master_cfg = NULL;
		}
	}

	arbiter_release_IO(arb_mgr);

	if (arb_mgr->arb_dev) {
		vfree(arb_mgr->arb_dev);
		arb_mgr->arb_dev = NULL;
	}

	unregister_chrdev(arb_mgr->arb_dev_major, "arbiter_dev");

	pr_info("Arbiter: module removed\n");
}

module_init(arbiter_init);
module_exit(arbiter_cleanup);

/* module description */
/*MODULE_LICENSE("Proprietary");*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Verisilicon");
MODULE_DESCRIPTION("arbiter driver");
