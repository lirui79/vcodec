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

#include "hantrodec.h"
#include "vcx_mmu_priv.h"
#include "vcmdswhwregisters.h"
#include "hantrodec_defs.h"
#include <asm/io.h>
#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#ifdef PCIE_EN
#include <linux/pci.h>
#endif
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/wait.h>
#include <linux/timer.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/mod_devicetable.h>
#include <linux/delay.h>
#if (KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE)
#include <linux/dma-contiguous.h>
#else
#include <linux/dma-map-ops.h>
#endif
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/dma-buf.h>
#include <linux/of_device.h>
#include <linux/vmalloc.h>
#include <linux/kthread.h>
#include <linux/pm_runtime.h>
#ifdef SUPPORT_DBGFS
#include <linux/debugfs.h>
#endif
#include <linux/time.h>

#include "vcd_normal_cfg.h"
#ifdef SUPPORT_AXIFE
#include "vcx_axife.h"
#endif
#ifdef SUPPORT_AFBC
#include "vcx_afbc.h"
#endif

#ifdef AXI2TO1_SUPPORT
#include "vcx_axi2to1.h"
#endif
#include "vcd_priv.h"

#include "vcd_vcmd.h"
#include "vcx_vcmd_priv.h"

#undef PDEBUG
#ifdef HANTRODEC_DEBUG
#ifdef __KERNEL__
#define PDEBUG(fmt, args...) pr_info("hantrodec: " fmt, ##args)
#else
#define PDEBUG(fmt, args...) fprintf(stderr, fmt, ##args)
#endif
#else
#define PDEBUG(fmt, args...)
#endif

/* base register address (Hardware address) */
static unsigned long gBaseHdwr = 0;
#ifdef PCIE_EN
/* TODO(mheikkinen) Implement multicore support. */
static struct pci_dev *gDev; /* PCI device structure. */
//unsigned long gBaseDDRHw; /* PCI base register address (memalloc) */
#endif
/* If HW support VCMD, user must define HW_VCMD in core_array and
 * the variable vcmd will be set as 1. Then user could use the
 * following module paramter use_vcmd to choose vcmd mode or normal mode.
 */
static unsigned int vcmd;

/* hantro VCD reg config */
#define HANTRO_VCD_REGS             MAX_REG_COUNT /*VCD total regs*/
#define HANTRO_VCD_FIRST_REG        0
#define HANTRO_VCD_LAST_REG         (HANTRO_VCD_REGS - 1)
#define HANTRODEC_HWBUILD_ID_OFF        (309 * 4)

/* Logic module IRQs */
#define HXDEC_NO_IRQ                    -1

#define MAX(a, b)                       (((a) > (b)) ? (a) : (b))

#define DEC_IO_SIZE_MAX                 (HANTRO_VCD_REGS * 4)

/* User should modify these configuration if do porting to own platform. */
/* Please guarantee the base_addr, io_size, dec_irq belong to same core. */

/* Defines use kernel clk cfg or not**/
//#define CLK_CFG
#ifdef CLK_CFG
/*this id should conform with platform define*/
#define CLK_ID   "hantrodec_clk"
#endif

/* Logic module base address */

#define DEC_IO_SIZE_0                   DEC_IO_SIZE_MAX /* bytes */
#define DEC_IO_SIZE_1                   DEC_IO_SIZE_MAX /* bytes */

#define DEC_IRQ_0                       HXDEC_NO_IRQ
#define DEC_IRQ_1                       HXDEC_NO_IRQ

#define IS_G1(hw_id)                    (((hw_id) == 0x6731) ? 1 : 0)
#define IS_G2(hw_id)                    (((hw_id) == 0x6732) ? 1 : 0)
#define IS_VCD(hw_id)               (((hw_id) == 0x9001) ? 1 : 0)
#define IS_BIGOCEAN(hw_id)              (((hw_id) == 0xB16D) ? 1 : 0)

/* Some IPs HW configuration parameters for APB Filter */
/* Because now such information can't be
 * read from APB filter configuration registers
 */
/* The fixed value have to be used */
#define VCD_NUM_MASK_REG            336
#define VCD_NUM_MODE                4
#define VCD_MASK_REG_OFFSET         4096
#define VCD_MASK_BITS_PER_REG       1

#define VCDJ_NUM_MASK_REG           332
#define VCDJ_NUM_MODE               1
#define VCDJ_MASK_REG_OFFSET        4096
#define VCDJ_MASK_BITS_PER_REG      1

#define AV1_NUM_MASK_REG                303
#define AV1_NUM_MODE                    1
#define AV1_MASK_REG_OFFSET             4096
#define AV1_MASK_BITS_PER_REG           1

#define AXIFE_NUM_MASK_REG              144
#define AXIFE_NUM_MODE                  1
#define AXIFE_MASK_REG_OFFSET           4096
#define AXIFE_MASK_BITS_PER_REG         1

#define CORE_MASK_CLIENTTYPE_BITMAP 16

#define DRIVER_NAME "hantrodec"

/***********local variable declaration********/

static const int DecHwId[] = {
	0x6731, /* G1 */
	0x6732, /* G2 */
	0xB16D, /* BigOcean */
	0x9001 /* VCD */
};


#ifdef CLK_CFG
struct clk *clk_cfg;
int is_clk_on;
struct timer_list timer;
#endif


#ifdef SUPPORT_DBGFS
/*debugfs for performance statistics*/
static struct dentry *debug_root;
static int N_reserved = 50;
#define MAX_RESERVED_TIME 256
#define IDLE 0
#define RESERVED 1
#define DECODING 2
#define DONE 3
//static char *subsys_name[MAX_SUBSYS_NUM] = {"reg_subsys0", "reg_subsys1", "reg_subsys2", "reg_subsys3"};
#endif

struct hantrodec_dev {
	struct device *dev;
	void *priv_data;
};

/* a watchdog which belongs to each device */
typedef struct {
	struct timer_list timer; //a timer to check if there is no response from hardware.
	u8 active; // 1: watchdog is start
	u8 triggered; // 1: watchdog is triggered
	u32 core_id; // corresponding to which device
} watchdog_t;

typedef struct {
	char *buffer;

	volatile unsigned int iosize[HXDEC_MAX_CORES];
	/* mapped address to different HW cores regs*/
	volatile u8 *hwregs[HXDEC_MAX_CORES][HW_CORE_MAX];
	/* mapped address to different HW cores regs*/
	volatile u8 *apbfilter_hwregs[HXDEC_MAX_CORES][HW_CORE_MAX];
	volatile int irq[HXDEC_MAX_CORES];
	int hw_id[HXDEC_MAX_CORES][HW_CORE_MAX];
	/* Requested client type for given core,
	 * used when a subsys has multiple
	 * decoders, e.g., VCD+VCDJ+BigOcean
	 */
	int cores;
	watchdog_t watchdog[HXDEC_MAX_CORES];
#ifdef CONFIG_DEC_PM
	int hw_active[HXDEC_MAX_CORES];
#endif
} hantrodec_t;

struct core_cfg {
	u32 cfg[HXDEC_MAX_CORES];              /* indicate the supported format */
};

struct core_bind {
	const struct file *filp;
	const void *dec_inst;
};

/* internal config struct (translated from SubsysDesc & CoreDesc) */
struct subsys_cfg {
	unsigned long base_addr;
	int irq;
	/* identifier for each subsys vc8000e=0,
	 * IM=1,vcd=2,jpege=3,jpegd=4
	 */
	u32 subsys_type;
	u32 submodule_offset[HW_CORE_MAX]; /* in bytes */
	u16 submodule_iosize[HW_CORE_MAX]; /* in bytes */

	volatile u8 *submodule_hwregs[HW_CORE_MAX]; /* virtual address */
	int has_apbfilter[HW_CORE_MAX];
};

static struct SubsysMgr {
	struct platform_device *platformdev;
	/* for non-vcmd */
	unsigned long multicorebase[HXDEC_MAX_CORES];
	int irq[HXDEC_MAX_CORES];
	int iosize[HXDEC_MAX_CORES];
	struct subsys_cfg    vpu_subsys[MAX_SUBSYS_NUM];
	struct apbfilter_cfg apbfilter_cfg[MAX_SUBSYS_NUM][HW_CORE_MAX];
	struct axife_cfg axife_cfg[MAX_SUBSYS_NUM];
	int elements;
	hantrodec_t hantrodec_data; /* dynamic allocation? */
	int hantrodec_major; /* dynamic allocation */

	u32 dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX / 4];
	u32 apbfilter_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX / 4 + 1];
	/* shadow_regs used to compare whether it's necessary to write to registers */
	u32 shadow_dec_regs[HXDEC_MAX_CORES][DEC_IO_SIZE_MAX / 4];

	struct semaphore dec_core_sem;

	int dec_irq;
	int pp_irq;

	atomic_t irq_rx;
	atomic_t irq_tx;

	struct core_bind core_owner[HXDEC_MAX_CORES];
	struct core_cfg config;

	spinlock_t owner_lock;

	wait_queue_head_t dec_wait_queue;
	wait_queue_head_t pp_wait_queue;
	wait_queue_head_t hw_queue;

	struct task_struct *kthread;
	u8 stop_kthread;
	wait_queue_head_t kthread_waitq;

	/* for VCMD */
	void *vcmd_mgr;   /* allocated in hantro_vcmd.c */
#ifdef SUPPORT_DBGFS
	/* for debugfs */
	unsigned long hw_return_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long reserved_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long start_hw_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long release_time[HXDEC_MAX_CORES][MAX_RESERVED_TIME];
	unsigned long active_start_time[MAX_RESERVED_TIME];
	unsigned long active_return_time[MAX_RESERVED_TIME];
	int r_index[HXDEC_MAX_CORES], hw_r_index[HXDEC_MAX_CORES], start_hw_index[HXDEC_MAX_CORES], release_index[HXDEC_MAX_CORES];
	int active_hw_index;
	int decode_state[HXDEC_MAX_CORES];
#endif
	void     *priv;
} subsys_mgr;

static int ReserveIO(void);
static void ReleaseIO(void);
static void ResetAsic(hantrodec_t *dev);
static int CheckHwId(hantrodec_t *dev);

#ifdef HANTRODEC_DEBUG
static void dump_regs(hantrodec_t *dev);
#endif

/* IRQ handler */
#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
static irqreturn_t hantrodec_isr(int irq, void *dev_id, struct pt_regs *regs);
#else
static irqreturn_t hantrodec_isr(int irq, void *dev_id);
#endif

static int hantrodec_mmap(struct file *filp, struct vm_area_struct *vma);

static int CoreHasFormat(const u32 *cfg, int core, u32 format);

static int use_vcmd = 0;
static unsigned int reg_access_opt = 0;	
/**
 * @brief stop vcd normall
 */
int abort_vcd(volatile u8 *reg_base)
{
	u32 status;

	status = (u32)ioread32((void __iomem *)(reg_base + HANTRODEC_IRQ_STAT_DEC_OFF));
	if (status & 0x1) {
		//abort vcd
		status |= HANTRODEC_DEC_ABORT;
		iowrite32(status, (void __iomem *)(reg_base + HANTRODEC_IRQ_STAT_DEC_OFF));

		return 1;
	}

	return 0;
}



/**
 * @brief To check subsys_mgr/dev's actions which need kthread to process
 * @return int: 0: no actions; 1: have actions
 */
static int _kthread_actions(watchdog_t **watchdog)
{
	int ret = 0, i;

	if (subsys_mgr.stop_kthread == 1)
		return 1;

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		*watchdog = &subsys_mgr.hantrodec_data.watchdog[i];
		if ((*watchdog)->triggered == 1) {
			ret = 1;
			break;
		} else {
			ret = 0;
			/*TODO*/
		}
	}

	return ret;
}

/**
 * @brief hantrodec kernel thread main function
 */
static int _kthread_fn(void *data)
{
	watchdog_t *watchdog = NULL;

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(subsys_mgr.kthread_waitq,
				_kthread_actions(&watchdog))) {
			pr_err("hantrodec: %s: signaled!!!\n", __func__);
			return -ERESTARTSYS;
		}

		if (watchdog == NULL)
			continue;

	}

	return 0;
}

/**
 * @brief wake up hantrodec kernel thread
 */
static void hantrodec_kthread_wakeup(void)
{

	if (IS_ERR(subsys_mgr.kthread))
		return;

	wake_up_interruptible_all(&subsys_mgr.kthread_waitq);
}

/**
 * @brief create kernel thread for hantrodec driver
 */
static void hantrodec_kthread_create(void)
{
	subsys_mgr.stop_kthread = 0;
	init_waitqueue_head(&subsys_mgr.kthread_waitq);
	subsys_mgr.kthread =
		kthread_run(_kthread_fn, NULL, "hantrodec_kthread");
	if (IS_ERR(subsys_mgr.kthread)) {
		pr_err("hantrodec: create hantrodec kthread failed\n");
		return;
	}
}

/**
 * @brief stop kernel thread hantrodec driver
 */
static void hantrodec_kthread_stop(void)
{
	if (!IS_ERR(subsys_mgr.kthread)) {
		subsys_mgr.stop_kthread = 1;
		kthread_stop(subsys_mgr.kthread);
		subsys_mgr.kthread = NULL;
	}
}

#ifdef SUPPORT_WATCHDOG
/**
 * @brief timer callback function of watchdog
 */
#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
static void _watchdog_cb(unsigned long arg)
#else
static void _watchdog_cb(struct timer_list *timer)
#endif
{
	struct timer_list *watchdog_timer;
	watchdog_t *watchdog;

#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	watchdog_timer = (struct timer_list *)arg;
#else
	watchdog_timer = timer;
#endif

	watchdog = container_of(watchdog_timer, watchdog_t, timer);

	watchdog->triggered = 1;
	hantrodec_kthread_wakeup();
}

/**
 * @brief start watchdog
 */
static void _watchdog_start(watchdog_t *watchdog)
{
	struct timer_list *timer = &watchdog->timer;

#if (KERNEL_VERSION(4, 14, 0) > LINUX_VERSION_CODE)
	init_timer(timer);
	timer->function = _watchdog_cb;
	timer->data = (unsigned long)timer;
#else
	timer_setup(timer, _watchdog_cb, 0);
#endif
	watchdog->active = 1;
}

/**
 * stop watchdog
 */
static void _watchdog_stop(watchdog_t *watchdog)
{
	if (watchdog->active) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
		timer_delete(&watchdog->timer);
#else
		del_timer(&watchdog->timer);
#endif
		watchdog->active = 0;
	}
}

/**
 * feed and start watchdog
 */
static void _watchdog_feed(watchdog_t *watchdog)
{
	if (watchdog->active == 0)
		_watchdog_start(watchdog);

	mod_timer(&watchdog->timer,
		(jiffies + HZ * ONE_JOB_WAIT_TIME / 1000));
}
#endif // SUPPORT_WATCHDOG

#ifdef CONFIG_DEC_PM
static long DecRestoreRegs(hantrodec_t *dev, u32 id)
{
	long i;

	/* write all regs to hardware */
	for (i = 1; i < HANTRO_VCD_REGS; i++)
		iowrite32(subsys_mgr.shadow_dec_regs[id][i],
			(void __iomem *)(dev->hwregs[id][HW_VCD] + i*4));

	return 0;
}

static long DecStoreRegs(hantrodec_t *dev, u32 id)
{
	long i;

	/* read all registers from hardware */
	for (i = 0; i < HANTRO_VCD_REGS; i++)
		subsys_mgr.shadow_dec_regs[id][i] =
			ioread32((void __iomem *)(dev->hwregs[id][HW_VCD] + i*4));

	return 0;
}

static int dec_pm_suspend(void *handler) {
	u32 i;
	hantrodec_t *dev = (hantrodec_t *)handler;

	for (i = 0; i < dev->cores; i++) {
		if (subsys_mgr.core_owner[i].filp) {
			/* polling until hw is idle */
			while (dev->hw_active[i])
				usleep_range(5000, 10000);

			/* let's backup all registers from H/W to shadow register to support suspend */
			DecStoreRegs(dev, i);
		}
	}

	return 0;
}

static int dec_pm_resume(void *handler) {
	int i;
	hantrodec_t *dev = (hantrodec_t *)handler;

	for (i = 0; i < dev->cores; i++) {
		if (subsys_mgr.core_owner[i].filp) {
			/* let's restore registers from shadow register to H/W to support resume */
			DecRestoreRegs(dev, i);
		}
	}

	return 0;
}

int hantrodec_pm_suspend(void *handler) {
	struct SubsysMgr *owner = (struct SubsysMgr *) handler;
	int ret = 0;
	ret = dec_pm_suspend(&owner->hantrodec_data);
/*
	vcx_priv_t *vcx_priv = (vcx_priv_t*) handler;
    struct SubsysMgr *owner = (struct SubsysMgr *) vcx_priv->priv;
	int ret = 0;
	if (use_vcmd)
		ret = vcmddec_pm_suspend(owner->vcmd_mgr);
	else
		ret = dec_pm_suspend(&owner->hantrodec_data);
*/
	pr_info("%s: device suspend done!\n", __func__);
	return ret;
}

int hantrodec_pm_resume(void *handler)
{
	struct SubsysMgr *owner = (struct SubsysMgr *) handler;
	int ret = 0;

	ret = dec_pm_resume(&owner->hantrodec_data);
/*
	vcx_priv_t *vcx_priv = (vcx_priv_t*) handler;
    struct SubsysMgr *owner = (struct SubsysMgr *) vcx_priv->priv;
	int ret = 0;

	if (use_vcmd)
		ret = vcmddec_pm_resume(owner->vcmd_mgr);
	else
		ret = dec_pm_resume(&owner->hantrodec_data);*/
	pr_info("%s, device resume done!\n", __func__);
	return ret;
}

#endif

#ifdef SUPPORT_DBGFS
#if 0
static char *subsys_state_read(int i)
{
	if (subsys_mgr.decode_state[i] == RESERVED)
		return "reserved\n";
	else if (subsys_mgr.decode_state[i] ==  IDLE)
		return "idle\n";
	else if (subsys_mgr.decode_state[i] == DECODING)
		return "decoding\n";
	else
		return "done\n";
}
#endif
static ssize_t subsys_state(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *val, v[50];
	int ret, i;

	val = kzalloc(100, GFP_KERNEL);
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		sprintf(v, "core[%d] work state\n", i);
		strcat(val, v);
		if (subsys_mgr.decode_state[i] == RESERVED)
			sprintf(v, "reserved\n");
		else if (subsys_mgr.decode_state[i] ==  IDLE)
			sprintf(v, "idle\n");
		else if (subsys_mgr.decode_state[i] == DECODING)
			sprintf(v, "decoding\n");
		else
			sprintf(v, "done\n");
		strcat(val, v);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, val, strlen(val));
	kfree(val);
	return ret;
}

const struct file_operations fileop_subsys_state = {
	.read = subsys_state,
};

static ssize_t Hw_Register_Print(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *v0, v1[30];
	int num_regs = 0, NUM_REGS = 0;
	int swreg_mes, j, i, k, ret = 0;
	hantrodec_t hantrodec_data;

	hantrodec_data = subsys_mgr.hantrodec_data;
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		for (j = HW_VCD; j < HW_CORE_MAX - 1; j++)
			num_regs += subsys_mgr.vpu_subsys[i].submodule_iosize[j] / 4;
	}
	v0 = kzalloc(num_regs * 100, GFP_KERNEL);
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		sprintf(v1, "core[%d]\n", i);
		strcat(v0, v1);
		for (k = HW_VCD; k < HW_CORE_MAX - 1; k++) {
			if (k == HW_VCD && hantrodec_data.hwregs[i][HW_VCD]) {
				sprintf(v1, "[VCD]\n");
				strcat(v0, v1);
				for (j = 0; j < MAX_REG_COUNT;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCD] + j * 4));
					sprintf(v1, "%d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < MAX_REG_COUNT) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCD] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < MAX_REG_COUNT) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCD] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < MAX_REG_COUNT) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCD] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
						j++;
					}
				}
			}
			if (k == HW_VCDJ && hantrodec_data.hwregs[i][HW_VCDJ]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "[VCDJ]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_VCDJ] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_BIGOCEAN && hantrodec_data.hwregs[i][HW_BIGOCEAN]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[BIGOCEAN]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_BIGOCEAN] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_MMU && hantrodec_data.hwregs[i][HW_MMU]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[MMU]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_MMU] + j * 4));
					sprintf(v1, "%d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_MMU] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_MMU] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][HW_MMU] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
						j++;
					}
				}
			}
			if (k == HW_MMU_WR && hantrodec_data.hwregs[i][HW_MMU_WR]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[MMU_WR]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_DEC400 && hantrodec_data.hwregs[i][HW_DEC400]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[DEC400]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
						j++;
					}
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
						j++;
					}
				}
			}
			if (k == HW_AXIFE && hantrodec_data.hwregs[i][HW_AXIFE]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[AXIFE]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
			if (k == HW_AFBC && hantrodec_data.hwregs[i][HW_AFBC]) {
				NUM_REGS = subsys_mgr.vpu_subsys[i].submodule_iosize[k] / 4;
				sprintf(v1, "\n[AFBC]\n");
				strcat(v0, v1);
				for (j = 0; j < NUM_REGS;) {
					swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
					sprintf(v1, "%3d: %08x ", j, swreg_mes);
					strcat(v0, v1);
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x ", swreg_mes);
						strcat(v0, v1);
					}
					j++;
					if (j < NUM_REGS) {
						swreg_mes = ioread32((void __iomem *)(hantrodec_data.hwregs[i][k] + j * 4));
						sprintf(v1, "%08x\n", swreg_mes);
						strcat(v0, v1);
					}
					j++;
				}
			}
		}
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
	kfree(v0);
	return ret;
}

const struct file_operations fileop_hw_reg_print = {
	.read = Hw_Register_Print,
};

static ssize_t hw_cycles(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char *v0;
	char v1[60];
	u32 swreg_mes = 0;
	int i;
	int ret;

	v0 = kzalloc(subsys_mgr.hantrodec_data.cores * 40, GFP_KERNEL);
	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		if (subsys_mgr.hantrodec_data.hwregs[i][HW_VCD])
			swreg_mes = ioread32((void __iomem *)(subsys_mgr.hantrodec_data.hwregs[i][HW_VCD] + 63 * 4));
		if (subsys_mgr.hantrodec_data.hwregs[i][HW_VCDJ])
			swreg_mes = ioread32((void __iomem *)(subsys_mgr.hantrodec_data.hwregs[i][HW_VCDJ] + 63 * 4));
		sprintf(v1, "hw cycles(core[%d] swreg 63) = %u\n", i, swreg_mes);
		strcat(v0, v1);
	}
	ret = simple_read_from_buffer(user_buf, count, ppos, v0, strlen(v0));
	kfree(v0);
	return ret;
}

const struct file_operations fileop_cycles = {
	.read = hw_cycles,
};

static ssize_t perf_statistic_write(struct file *file, const char __user *user_buf, size_t count, loff_t *ppos)
{
	char In_fo[10];
	int bytes_to_read = 4;

	simple_write_to_buffer(In_fo, count, ppos, user_buf, bytes_to_read);
	kstrtoint(In_fo, 10, &N_reserved);
	//sscanf(In_fo, "%d", &N_reserved);

	return count;
}

static ssize_t perf_statistic_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos)
{
	char v[200], *t;
	int i, j, index, ret;
	u32 total_time_eachcore[HXDEC_MAX_CORES] = {0, 0,};
	u32 total_time = 0;

	t = kmalloc(N_reserved * 60 * subsys_mgr.hantrodec_data.cores, GFP_KERNEL);
	for (j = 0; j < subsys_mgr.hantrodec_data.cores; j++) {
		sprintf(v, "core[%d]\n", j);
		strcat(t, v);
		strcat(t, "[reserved time, enable time - reserved time, done_time - reserved time, release_time - reserved time, decode_done_time - decode_starting_time] /ms\n");
		index = subsys_mgr.r_index[j];
		if (index >= N_reserved) {
			for (i = 0; i < N_reserved; i++) {
				sprintf(v, "[%lu, ", subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.start_hw_time[j][i + index - N_reserved] - subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.hw_return_time[j][i + index - N_reserved] - subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.release_time[j][i + index - N_reserved] - subsys_mgr.reserved_time[j][i + index - N_reserved]);
				strcat(t, v);
				sprintf(v, "%lu]\n", subsys_mgr.hw_return_time[j][i + index - N_reserved] - subsys_mgr.start_hw_time[j][i + index - N_reserved]);
				strcat(t, v);
				total_time_eachcore[j] += subsys_mgr.hw_return_time[j][i + index - N_reserved] - subsys_mgr.start_hw_time[j][i + index - N_reserved];
			}
		} else {
			for (i = 0; i < index; i++) {
				sprintf(v, "[%lu, ", subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.start_hw_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.hw_return_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.release_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu]\n", subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i]);
				strcat(t, v);
				total_time_eachcore[j] += subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i];
			}

			for (i = MAX_RESERVED_TIME - 1; i > MAX_RESERVED_TIME - 1 - (N_reserved - index); i--) {
				sprintf(v, "[%lu, ", subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.start_hw_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.hw_return_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu, ", subsys_mgr.release_time[j][i] - subsys_mgr.reserved_time[j][i]);
				strcat(t, v);
				sprintf(v, "%lu]\n", subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i]);
				strcat(t, v);
				total_time_eachcore[j] += subsys_mgr.hw_return_time[j][i] - subsys_mgr.start_hw_time[j][i];
			}
		}
		sprintf(v, "core[%d] total decoding time: %u\n", j, total_time_eachcore[j]);
		strcat(t, v);
	}
	if (subsys_mgr.hantrodec_data.cores == 1) {
		for (j = 0; j < subsys_mgr.hantrodec_data.cores; j++) {
			//for (i = 0; i < MAX_RESERVED_TIME; i++) {
			total_time += total_time_eachcore[j];
			//}
		}
	} else {
		for (i = 0; i < subsys_mgr.active_hw_index; i++)
			total_time += subsys_mgr.active_return_time[i] - subsys_mgr.active_start_time[i];
	}
	sprintf(v, "total decoding time: %u\n", total_time);
	strcat(t, v);
	ret = simple_read_from_buffer(user_buf, count, ppos, t, strlen(t));
	kfree(t);
	return ret;
}

const struct file_operations fileop_perfor_statistic = {
	.write = perf_statistic_write,
	.read = perf_statistic_read,
};
#endif

#ifdef CLK_CFG
DEFINE_SPINLOCK(clk_lock);
#endif

#define DWL_CLIENT_TYPE_H264_DEC        1U
#define DWL_CLIENT_TYPE_MPEG4_DEC       2U
#define DWL_CLIENT_TYPE_JPEG_DEC        3U
#define DWL_CLIENT_TYPE_PP              4U
#define DWL_CLIENT_TYPE_VC1_DEC         5U
#define DWL_CLIENT_TYPE_MPEG2_DEC       6U
#define DWL_CLIENT_TYPE_VP6_DEC         7U
#define DWL_CLIENT_TYPE_AVS_DEC         8U
#define DWL_CLIENT_TYPE_RV_DEC          9U
#define DWL_CLIENT_TYPE_VP8_DEC         10U
#define DWL_CLIENT_TYPE_VP9_DEC         11U
#define DWL_CLIENT_TYPE_HEVC_DEC        12U
#define DWL_CLIENT_TYPE_ST_PP           14U
#define DWL_CLIENT_TYPE_H264_MAIN10     15U
#define DWL_CLIENT_TYPE_AVS2_DEC        16U
#define DWL_CLIENT_TYPE_AV1_DEC         17U
#define DWL_CLIENT_TYPE_VVC_DEC         18U

#define CORE_TYPE_STR_CASE(ct) { case (ct): return(#ct); }

static char *CoreTypeStr(enum CoreType ct)
{
	switch (ct) {
		CORE_TYPE_STR_CASE(HW_VCD)
		CORE_TYPE_STR_CASE(HW_VCDJ)
		CORE_TYPE_STR_CASE(HW_VCMD)
		CORE_TYPE_STR_CASE(HW_MMU)
		CORE_TYPE_STR_CASE(HW_MMU_WR)
		CORE_TYPE_STR_CASE(HW_DEC400)
		CORE_TYPE_STR_CASE(HW_AXIFE)
		CORE_TYPE_STR_CASE(HW_AFBC)
		CORE_TYPE_STR_CASE(HW_ARB)
	default :
		return "Invalid core type";
	}
}

#ifdef HANTRODEC_DEBUG

#define IOCTL_CMD_STR_CASE(cmd) { case (cmd): return(#cmd); }

static char *IoctlCmdStr(unsigned int cmd)
{
	switch (cmd) {
		IOCTL_CMD_STR_CASE(HANTRODEC_IOC_MC_CORES)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCGHWOFFSET)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCGHWIOSIZE)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOC_MC_OFFSETS)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOC_CLI)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOC_STI)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCS_DEC_PUSH_REG)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCS_DEC_PULL_REG)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCH_DEC_RESERVE)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCT_DEC_RELEASE)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCX_DEC_WAIT)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCG_CORE_WAIT)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOX_ASIC_ID)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCG_CORE_ID)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCS_DEC_WRITE_REG)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCS_DEC_READ_REG)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOX_ASIC_BUILD_ID)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCX_POLL)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOX_SUBSYS)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG)
		IOCTL_CMD_STR_CASE(HANTRODEC_DEBUG_STATUS)
#ifdef SUPPORT_MMU
		/* MMU */
		IOCTL_CMD_STR_CASE(HANTRO_IOCS_MMU_MEM_MAP)
		IOCTL_CMD_STR_CASE(HANTRO_IOCS_MMU_MEM_UNMAP)
		IOCTL_CMD_STR_CASE(HANTRO_IOCS_MMU_FLUSH)
		IOCTL_CMD_STR_CASE(HANTRO_IOCS_MMU_SWITCH_PAGETABLE)
		IOCTL_CMD_STR_CASE(HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF)
#endif
		/* VCMD */
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_GET_CMDBUF_POOL_SIZE)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_SET_CMDBUF_POOL_BASE)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_RESERVE_CMDBUF)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_WAIT_CMDBUF)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_RELEASE_CMDBUF)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_POLLING_CMDBUF)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_DROP_OWNER)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_PUSH_SLICE_REG)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_ABORT_CMDBUF)
		IOCTL_CMD_STR_CASE(HANTRO_VCMD_IOCH_WAIT_OWNER_DONE)
		/* AXI FE / APB filter */
		IOCTL_CMD_STR_CASE(HANTRODEC_IOC_APBFILTER_CONFIG)
		IOCTL_CMD_STR_CASE(HANTRODEC_IOC_AXIFE_CONFIG)
	default :
		return "Invalid ioctl cmd";
	}
}
#endif

static void ReadCoreConfig(hantrodec_t *dev)
{
	int c, j;
	u32 reg, tmp, mask;
	u32 *regs_va;
	u32 hw_build_id;

	memset(subsys_mgr.config.cfg, 0, sizeof(subsys_mgr.config.cfg));

	for (c = 0; c < dev->cores; c++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (j != HW_VCD && j != HW_VCDJ)
				continue;
			if (use_vcmd)
				regs_va = get_submodule_regs_va(subsys_mgr.vcmd_mgr, c, SUB_MOD_MAIN);
			/* NOT defined core type */
			if (!dev->hwregs[c][j])
				continue;
			/* Decoder configuration */
			if (IS_VCD(dev->hw_id[c][j])) {
				if (!use_vcmd)
					reg = ioread32((void __iomem *)(dev->hwregs[c][j] +
						HANTRODEC_SYNTH_CFG * 4));
				else
					reg = *(regs_va + HANTRODEC_SYNTH_CFG);

				pr_info("hantrodec: subsys[%d] swreg[%d] = 0x%08x\n",
					c, HANTRODEC_SYNTH_CFG, reg);

				tmp = (reg >> DWL_H264_E) & 0x3U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has H264\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

				tmp = (reg >> DWL_H264HIGH10_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has H264HIGH10\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_H264_DEC : 0;

				tmp = (reg >> DWL_AVS2_E) & 0x03U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has AVS2\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_AVS2_DEC : 0;

				tmp = (reg >> DWL_AV1_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has AV1\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_AV1_DEC : 0;

				tmp = (reg >> DWL_JPEG_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has JPEG\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

				tmp = (reg >> DWL_HJPEG_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has HJPEG\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_JPEG_DEC : 0;

				tmp = (reg >> DWL_VVC_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has VVC\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VVC_DEC : 0;

				tmp = (reg >> DWL_MPEG4_E) & 0x3U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has MPEG4\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_MPEG4_DEC : 0;

				tmp = (reg >> DWL_VC1_E) & 0x3U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has VC1\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VC1_DEC : 0;

				tmp = (reg >> DWL_MPEG2_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has MPEG2\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_MPEG2_DEC : 0;

				tmp = (reg >> DWL_VP6_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has VP6\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VP6_DEC : 0;

				if (!use_vcmd)
					reg = ioread32((void __iomem *)(dev->hwregs[c][j] +
						HANTRODEC_SYNTH_CFG_2 * 4));
				else
					reg = *(regs_va + HANTRODEC_SYNTH_CFG_2);

				pr_info("hantrodec: subsys[%d] swreg[%d] = 0x%08x\n",
					c, HANTRODEC_SYNTH_CFG_2, reg);

				if (!use_vcmd)
					hw_build_id = ioread32((void __iomem *)(dev->hwregs[c][j] +
							HANTRODEC_HWBUILD_ID_OFF));
				else
					hw_build_id = *(regs_va + HANTRODEC_HWBUILD_ID_OFF);
				if (hw_build_id != 0x1F70) {
					/* VP7 and WEBP is part of VP8 */
					mask = (1 << DWL_VP8_E) |
						(1 << DWL_VP7_E) |
						(1 << DWL_WEBP_E);
					tmp = (reg & mask);
					if (tmp & (1 << DWL_VP8_E))
						pr_info("hantrodec: subsys[%d] has VP8\n",
							c);
					if (tmp & (1 << DWL_VP7_E))
						pr_info("hantrodec: subsys[%d] has VP7\n",
							c);
					if (tmp & (1 << DWL_WEBP_E))
						pr_info("hantrodec: subsys[%d] has WebP\n",
							c);
					subsys_mgr.config.cfg[c] |=
					  tmp ? 1 << DWL_CLIENT_TYPE_VP8_DEC :
					  0;
				}

				tmp = (reg >> DWL_AVS_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has AVS\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_AVS_DEC : 0;

				tmp = (reg >> DWL_RV_E) & 0x03U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has RV\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_RV_DEC : 0;

				if (!use_vcmd)
					reg = ioread32((void __iomem *)(dev->hwregs[c][j] +
							HANTRODEC_SYNTH_CFG_3 * 4));
				else
					reg = *(regs_va + HANTRODEC_SYNTH_CFG_3);
				pr_info("hantrodec: subsys[%d] swreg[%d] = 0x%08x\n",
					c, HANTRODEC_SYNTH_CFG_3, reg);

				tmp = (reg >> DWL_HEVC_E) & 0x07U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has HEVC\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_HEVC_DEC : 0;

				tmp = (reg >> DWL_VP9_E) & 0x07U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has VP9\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_VP9_DEC : 0;

				/* Post-processor configuration */
				if (!use_vcmd)
					reg = ioread32((void __iomem *)(dev->hwregs[c][j] +
							HANTRODECPP_CFG_STAT * 4));
				else
					reg = *(regs_va + HANTRODECPP_CFG_STAT);

				tmp = (reg >> DWL_PP_E) & 0x01U;
				if (tmp)
					pr_info("hantrodec: subsys[%d] has PP\n",
						c);
				subsys_mgr.config.cfg[c] |=
					tmp ? 1 << DWL_CLIENT_TYPE_PP : 0;

				subsys_mgr.config.cfg[c] |= 1 << DWL_CLIENT_TYPE_ST_PP;
			}
		}
	}
}

static int CoreHasFormat(const u32 *cfg, int core, u32 format)
{
	return (cfg[core] & (1 << format)) ? 1 : 0;
}

static int IsValideCore(long core, unsigned long format)
{
	u32 core_mask = format & (0xffffffff >>
							CORE_MASK_CLIENTTYPE_BITMAP);

	if ((core_mask >> core) & 0x01)
		return 1;
	return 0;
}

static int GetDecCore(long core, hantrodec_t *dev, struct file *filp,
		      struct req_core_info *core_info)
{
	int success = 0;
	unsigned long flags;
#ifdef SUPPORT_DBGFS
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;
#else
	struct timespec64 time_now;
#endif
#endif

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);
	if (IsValideCore(core, core_info->format) && !subsys_mgr.core_owner[core].filp) {
		subsys_mgr.core_owner[core].filp = filp;
		subsys_mgr.core_owner[core].dec_inst = core_info->dec_inst;
		success = 1;
#ifdef SUPPORT_DBGFS
		subsys_mgr.decode_state[core] = RESERVED;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
		do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
		ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
		subsys_mgr.reserved_time[core][subsys_mgr.r_index[core]] = time_num;
			subsys_mgr.r_index[core] = subsys_mgr.r_index[core] + 1;
		if (subsys_mgr.r_index[core] == MAX_RESERVED_TIME)
			subsys_mgr.r_index[core] = 0;
#endif
	}
	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	return success;
}

static int GetDecCoreAny(long *core, hantrodec_t *dev,
			 struct file *filp, struct req_core_info *core_info)
{
	int success = 0;
	long c;

	*core = -1;
	for (c = 0; c < dev->cores; c++) {
		/* a free core that has format */
		if (GetDecCore(c, dev, filp, core_info)) {
			success = 1;
			*core = c;
			break;
		}
	}

	return success;
}

static int GetDecCoreID(hantrodec_t *dev,
			struct file *filp,
			unsigned long format)
{
	long c;
	unsigned long flags;

	int core_id = -1;

	for (c = 0; c < dev->cores; c++) {
		/* a core that has format */
		spin_lock_irqsave(&subsys_mgr.owner_lock, flags);
		if (CoreHasFormat(subsys_mgr.config.cfg, c, format)) {
			core_id = c;
			spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
			break;
		}
		spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
	}
	return core_id;
}

static long ReserveDecoder(hantrodec_t *dev,
			   struct file *filp, struct req_core_info *core_info)
{
	long core = -1;

	/* reserve a core */
	if (down_interruptible(&subsys_mgr.dec_core_sem))
		return -ERESTARTSYS;

	/* lock a core that has specific format*/
	if (wait_event_interruptible(subsys_mgr.hw_queue,
				     GetDecCoreAny(&core, dev, filp, core_info) != 0))

		return -ERESTARTSYS;

	return core;
}

static void ReleaseDecoder(hantrodec_t *dev, long core)
{
	u32 status;
	unsigned long flags;
	u32 core_type = HW_VCD;
#ifdef SUPPORT_DBGFS
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;
#else
	struct timespec64 time_now;
#endif
#endif
	if (!dev->hwregs[core][core_type])
		core_type = HW_VCDJ;
	PDEBUG("%s %ld\n", __func__, core);

	status = ioread32((void __iomem *)
			(dev->hwregs[core][core_type] +
			HANTRODEC_IRQ_STAT_DEC_OFF));

	/* make sure HW is disabled */
	if (status & HANTRODEC_DEC_E) {
		pr_info("hantrodec: DEC[%li] still enabled -> reset\n",
			core);

		/* abort decoder */
		status |= HANTRODEC_DEC_ABORT | HANTRODEC_DEC_IRQ_DISABLE;
		iowrite32(status, (void __iomem *)
		  (dev->hwregs[core][core_type] +
		  HANTRODEC_IRQ_STAT_DEC_OFF));
	}
#ifdef SUPPORT_WATCHDOG
	/* if interrupt uses polling mode */
	_watchdog_stop(&dev->watchdog[core]);
#endif

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

	subsys_mgr.core_owner[core].filp = NULL;
	subsys_mgr.core_owner[core].dec_inst = NULL;
#ifdef SUPPORT_DBGFS
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	do_gettimeofday(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
	ktime_get_real_ts64(&time_now);
	time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
	subsys_mgr.release_time[core][subsys_mgr.release_index[core]] = time_num;
	subsys_mgr.release_index[core] = subsys_mgr.release_index[core] + 1;
	if (subsys_mgr.release_index[core] == MAX_RESERVED_TIME)
		subsys_mgr.release_index[core] = 0;
	subsys_mgr.decode_state[core] = IDLE;
#endif
	subsys_mgr.dec_irq &= ~(1 << core);

	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	up(&subsys_mgr.dec_core_sem);

	wake_up_interruptible_all(&subsys_mgr.hw_queue);
}

#ifdef HANTRODEC_DEBUG
static u32 flush_count; /* times of calling of DecFlushRegs */
static u32 flush_regs; /* total number of registers flushed */
#endif

static long DecFlushRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret = 0, i;
#ifdef HANTRODEC_DEBUG
	int reg_wr = 2;
#endif
#ifdef SUPPORT_DBGFS
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;
#else
	struct timespec64 time_now;
#endif
#endif
	u32 id = core->id;
	u32 type = core->type;

	PDEBUG("hantrodec: %S\n", __func__);
	PDEBUG("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
		   core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    core->type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		return -EINVAL;

	PDEBUG("hantrodec: submodule_iosize = %d\n",
	       subsys_mgr.vpu_subsys[id].submodule_iosize[type]);
	PDEBUG("hantrodec: reg count = %d\n", reg_count[id]);

	ret = copy_from_user(subsys_mgr.dec_regs[id], (__u32 __user *)core->regs,
			     subsys_mgr.vpu_subsys[id].submodule_iosize[type]);
	if (ret) {
		PDEBUG("copy_from_user failed, returned %li\n", ret);
		return -EFAULT;
	}

	if (type == HW_VCD || type == HW_BIGOCEAN || type == HW_VCDJ) {
		/* write all regs but the status reg[1] to hardware */
		if (reg_access_opt) {
			for (i = 3;
				 i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
				 i++) {
				/* check whether register value is updated. */
				if (subsys_mgr.dec_regs[id][i] != subsys_mgr.shadow_dec_regs[id][i]) {
					iowrite32(subsys_mgr.dec_regs[id][i], (void __iomem *)
					  (dev->hwregs[id][type] + i * 4));
					subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i];

#ifdef HANTRODEC_DEBUG
					reg_wr++;
#endif
				}
			}
		} else {
			for (i = 3;
				 i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
				 i++) {
				iowrite32(subsys_mgr.dec_regs[id][i], (void __iomem *)
						  (dev->hwregs[id][type] + i * 4));

#ifdef VALIDATE_REGS_WRITE
	if (subsys_mgr.dec_regs[id][i] !=
			ioread32((void *)(dev->hwregs[id][type] +
			  i * 4)))
		pr_info("hantrodec: swreg[%ld]: read %08x != write %08x *\n",
			i,
				ioread32((void *)(dev->hwregs[id][type] + i * 4)),
				subsys_mgr.dec_regs[id][i]);
#endif
			}
#ifdef HANTRODEC_DEBUG
			reg_wr = subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4 - 1;
#endif
		}

		/* write swreg2 for AV1, in which bit0 is the start bit */
		iowrite32(subsys_mgr.dec_regs[id][2],
			  (void __iomem *)(dev->hwregs[id][type] + 8));
		subsys_mgr.shadow_dec_regs[id][2] = subsys_mgr.dec_regs[id][2];

#ifdef CONFIG_DEC_PM
		if (subsys_mgr.dec_regs[id][1] & 0x1)
			dev->hw_active[id] = 1;
#endif

		/* write the status register, which may start the decoder */
		iowrite32(subsys_mgr.dec_regs[id][1],
			  (void __iomem *)(dev->hwregs[id][type] + 4));
#ifdef SUPPORT_DBGFS

	if ((subsys_mgr.dec_regs[id][1] & 0x1) == 1) {
		subsys_mgr.decode_state[id] = DECODING;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
		do_gettimeofday(&time_now);
		time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else
		ktime_get_real_ts64(&time_now);
		time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
		subsys_mgr.start_hw_time[id][subsys_mgr.start_hw_index[id]] = time_num;
		subsys_mgr.start_hw_index[id] = subsys_mgr.start_hw_index[id] + 1;
		if (subsys_mgr.start_hw_index[id] == MAX_RESERVED_TIME)
			subsys_mgr.start_hw_index[id] = 0;
		if (subsys_mgr.hantrodec_data.cores > 1) {
			int active_flag = 1;

			for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
				if (id == i)
					continue;
				if (subsys_mgr.decode_state[i] ==  DECODING)
					active_flag = 0;
			}
			if (active_flag == 1)
				subsys_mgr.active_start_time[subsys_mgr.active_hw_index] = time_num;
		}
	}
#endif
		subsys_mgr.shadow_dec_regs[id][1] = subsys_mgr.dec_regs[id][1];

#ifdef HANTRODEC_DEBUG
		flush_count++;
		flush_regs += reg_wr;
#endif

		PDEBUG("flushed registers on core %d\n", id);
		PDEBUG("%d %s: flushed %d/%d registers (dec_mode = %d, avg %d regs per flush)\n",
		       flush_count, __func__,
			   reg_wr, flush_regs,
			   subsys_mgr.dec_regs[id][3] >> 27,
			   flush_regs / flush_count);
	} else {
		/* write all regs but the status reg[1] to hardware */
		for (i = 0; i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
			 i++) {
			iowrite32(subsys_mgr.dec_regs[id][i],
				  (void __iomem *)(dev->hwregs[id][type] +
				  i * 4));
#ifdef VALIDATE_REGS_WRITE
			if (subsys_mgr.dec_regs[id][i] !=
				ioread32((void *)(dev->hwregs[id][type] + i * 4)))
				pr_info(
					   "hantrodec: swreg[%ld]: read %08x != write %08x *\n",
					   i,
					   ioread32((void *)(dev->hwregs[id][type] +
					   i * 4)),
					   subsys_mgr.dec_regs[id][i]);
#endif
		}
	}
#ifdef SUPPORT_WATCHDOG
	//feed the watchdog
	_watchdog_feed(&dev->watchdog[id]);
#endif


	return 0;
}

static long DecWriteRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret = 0;
	u32 i = core->reg_id;
	u32 id = core->id;
	u32 type = core->type;

	PDEBUG("hantrodec: %s\n", __func__);
	PDEBUG("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
		   core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type] ||
		(core->size & 0x3) || core->reg_id * 4 + core->size >
		subsys_mgr.vpu_subsys[id].submodule_iosize[type])

		return -EINVAL;

	ret = copy_from_user(subsys_mgr.dec_regs[id], (__u32 __user *)core->regs,
			     core->size);
	if (ret) {
		PDEBUG("copy_from_user failed, returned %li\n", ret);
		return -EFAULT;
	}

	for (i = core->reg_id; i < core->reg_id + core->size / 4; i++) {
		PDEBUG("hantrodec: write %08x to reg[%d] core %d\n",
		       subsys_mgr.dec_regs[id][i - core->reg_id], i, id);
		iowrite32(subsys_mgr.dec_regs[id][i - core->reg_id],
			  (void __iomem *)(dev->hwregs[id][type] + i * 4));
		if (type == HW_VCD)
			subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i - core->reg_id];
	}
	return 0;
}

static long DecWriteApbFilterRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret = 0;
	u32 i = core->reg_id;
	u32 id = core->id;

	PDEBUG("hantrodec: %s\n", __func__);
	PDEBUG("hantrodec: id = %d, type = %d, size = %d, reg_id = %d\n",
	       core->id, core->type, core->size, core->reg_id);

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    core->type >= HW_CORE_MAX ||
		!subsys_mgr.vpu_subsys[id].submodule_hwregs[core->type] ||
		(core->size & 0x3) ||
		core->reg_id * 4 + core->size >
		subsys_mgr.vpu_subsys[id].submodule_iosize[core->type] + 4)

		return -EINVAL;

	ret = copy_from_user(subsys_mgr.apbfilter_regs[id], (__u32 __user *)core->regs,
			     core->size);
	if (ret) {
		PDEBUG("copy_from_user failed, returned %li\n", ret);
		return -EFAULT;
	}

	for (i = core->reg_id; i < core->reg_id + core->size / 4; i++) {
		PDEBUG("hantrodec: write %08x to reg[%d] core %d\n",
		       subsys_mgr.dec_regs[id][i - core->reg_id], i, id);
		iowrite32(subsys_mgr.apbfilter_regs[id][i - core->reg_id],
			  (void __iomem *)
				  (dev->apbfilter_hwregs[id][core->type] +
				  i * 4));
	}
	return 0;
}

static long DecReadRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret;
	u32 id = core->id;
	u32 i = core->reg_id;
	u32 type = core->type;
#ifdef SUPPORT_DBGFS
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;
#else
	struct timespec64 time_now;
#endif
#endif
	PDEBUG("hantrodec: %s\n", __func__);
	PDEBUG("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
	 core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type] ||
			(core->size & 0x3) ||
			core->reg_id * 4 + core->size >
			subsys_mgr.vpu_subsys[id].submodule_iosize[type])
		return -EINVAL;

	/* read specific registers from hardware */
	for (i = core->reg_id; i < core->reg_id + core->size / 4; i++) {
		subsys_mgr.dec_regs[id][i] = ioread32(
			(void __iomem *)(dev->hwregs[id][type] + i * 4));
#ifdef SUPPORT_DBGFS
		if ((i == 1) && ((subsys_mgr.dec_regs[id][1] & 0x1) == 0)) {
			subsys_mgr.decode_state[id] = DONE;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

			do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else

			ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
			subsys_mgr.hw_return_time[id][subsys_mgr.hw_r_index[id]] = time_num;
			subsys_mgr.hw_r_index[id] = subsys_mgr.hw_r_index[id] + 1;
			if (subsys_mgr.hw_r_index[id] == MAX_RESERVED_TIME)
				subsys_mgr.hw_r_index[id] = 0;

			if (subsys_mgr.hantrodec_data.cores > 1) {
				int active_flag = 1;

				for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
					if (id == i)
						continue;
					if (subsys_mgr.decode_state[i] ==  DECODING)
						active_flag = 0;
				}
				if (active_flag == 1) {
					subsys_mgr.active_return_time[subsys_mgr.active_hw_index] = time_num;
					subsys_mgr.active_hw_index += 1;
				}
		}
		}
#endif
		PDEBUG("hantrodec: read %08x from reg[%d] core %d\n",
		       subsys_mgr.dec_regs[id][i], i, id);
		if (type == HW_VCD)
			subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i];
	}

	/* put registers to user space*/
	ret = copy_to_user((__u32 __user *)core->regs, &subsys_mgr.dec_regs[id][core->reg_id],
			   core->size);
	if (ret) {
		PDEBUG("copy_to_user failed, returned %li\n", ret);
		return -EFAULT;
	}
	return 0;
}

static long DecRefreshRegs(hantrodec_t *dev, struct core_desc *core)
{
	long ret, i;
	u32 id = core->id;
	u32 type = core->type;

	PDEBUG("hantrodec: %s\n", __func__);
	PDEBUG("hantrodec: id = %d, type = %d [ %s ], size = %d, reg_id = %d\n",
	       core->id, core->type, CoreTypeStr(core->type), core->size,
	 core->reg_id);

	if (type == HW_VCD && !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		type = HW_VCDJ;

	if (id >= MAX_SUBSYS_NUM || !subsys_mgr.vpu_subsys[id].base_addr ||
	    type >= HW_CORE_MAX || !subsys_mgr.vpu_subsys[id].submodule_hwregs[type])
		return -EINVAL;

	PDEBUG("hantrodec: submodule_iosize = %d\n",
	       vpu_subsys[id].submodule_iosize[type]);

	if (!reg_access_opt) {
		for (i = 0; i < subsys_mgr.vpu_subsys[id].submodule_iosize[type] / 4;
	 i++) {
			subsys_mgr.dec_regs[id][i] = ioread32((
	void __iomem *)(dev->hwregs[id][type] + i * 4));
		}
	} else {
		// only need to read swreg1,62(?),63,168,169
#define REFRESH_REG(idx)                                                       \
	do {                                                                   \
		i = (idx);                                                     \
		subsys_mgr.shadow_dec_regs[id][i] = subsys_mgr.dec_regs[id][i] = ioread32(           \
			(void __iomem *)(dev->hwregs[id][type] + i * 4));      \
	} while (0)

		REFRESH_REG(0);
		REFRESH_REG(1);
		REFRESH_REG(62);
		REFRESH_REG(63);
		REFRESH_REG(168);
		REFRESH_REG(169);
#undef REFRESH_REG
	}

	ret = copy_to_user((__u32 __user *)core->regs, subsys_mgr.dec_regs[id],
			   subsys_mgr.vpu_subsys[id].submodule_iosize[type]);
	if (ret) {
		PDEBUG("copy_to_user failed, returned %li\n", ret);
		return -EFAULT;
	}
	return 0;
}

static int CheckDecIrq(hantrodec_t *dev, int id)
{
	unsigned long flags;
	int rdy = 0;

	const u32 irq_mask = (1 << id);

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

	if (subsys_mgr.dec_irq & irq_mask) {
		/* reset the wait condition(s) */
		subsys_mgr.dec_irq &= ~irq_mask;
		rdy = 1;
	}

	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	return rdy;
}

static long WaitDecReadyAndRefreshRegs(hantrodec_t *dev, struct core_desc *core)
{
	u32 id = core->id;
	long ret;

	PDEBUG("wait_event_interruptible DEC[%d]\n", id);
#ifdef USE_SW_TIMEOUT
	u32 status;

	ret = wait_event_interruptible_timeout(subsys_mgr.dec_wait_queue,
					       CheckDecIrq(dev, id),
		  msecs_to_jiffies(2000));
	if (ret < 0) {
		PDEBUG("DEC[%d]  wait_event_interruptible interrupted\n", id);
		return -ERESTARTSYS;
	} else if (ret == 0) {
		PDEBUG("DEC[%d]  wait_event_interruptible timeout\n", id);
		status = ioread32((void *)(dev->hwregs[id][HW_VCD] +
		HANTRODEC_IRQ_STAT_DEC_OFF));
		/* check if HW is enabled */
		if (status & HANTRODEC_DEC_E) {
			pr_info("hantrodec: DEC[%d] reset because of timeout\n",
				id);

			/* abort decoder */
			status |= HANTRODEC_DEC_ABORT |
					  HANTRODEC_DEC_IRQ_DISABLE;
			iowrite32(status, (void *)(dev->hwregs[id][HW_VCD] +
		HANTRODEC_IRQ_STAT_DEC_OFF));
		}
	}
#else
	ret = wait_event_interruptible(subsys_mgr.dec_wait_queue, CheckDecIrq(dev, id));
	if (ret) {
		PDEBUG("DEC[%d]  wait_event_interruptible interrupted\n", id);
		return -ERESTARTSYS;
	}
#endif
	atomic_inc(&subsys_mgr.irq_tx);

	/* refresh registers */
	return DecRefreshRegs(dev, core);
}

static int CheckCoreIrq(hantrodec_t *dev, const struct file *filp, int *id)
{
	unsigned long flags;
	int rdy = 0, n = 0;

	do {
		u32 irq_mask = (1 << n);

		spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

		if (subsys_mgr.dec_irq & irq_mask) {
			if (subsys_mgr.core_owner[n].filp == filp) {
				/* we have an IRQ for our client */

				/* reset the wait condition(s) */
				subsys_mgr.dec_irq &= ~irq_mask;

				/* signal ready core no. for our client */
				*id = n;

				rdy = 1;

				spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
				break;
			} else if (!subsys_mgr.core_owner[n].filp) {
				/* zombie IRQ */
				pr_info("IRQ on core[%d], but no owner!!!\n",
					n);

				/* reset the wait condition(s) */
				subsys_mgr.dec_irq &= ~irq_mask;
			}
		}

		spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

		n++; /* next core */
	} while (n < dev->cores);

	if (n == dev->cores) {
		spin_lock_irqsave(&subsys_mgr.owner_lock, flags);
		for (n = 0; n < dev->cores; n++) {
			if (subsys_mgr.core_owner[n].filp == filp)
				break;
		}
		if (n == dev->cores) {
			spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
			return 1; /* used to drop_dec when seek */
		}
		spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);
	}

	return rdy;
}
/**
 * @brief drop owner from current filp
 */
static long wait_owner_done(hantrodec_t *dev, const struct file *filp, void *owner)
{
	u32 i, ret = 0;

	for (i = 0; i < dev->cores; i++) {
		if (subsys_mgr.core_owner[i].filp == filp &&
				subsys_mgr.core_owner[i].dec_inst == owner) {
			ret = 1;
			break;
		}
	}

	return ret;
}

static long WaitCoreReady(hantrodec_t *dev, const struct file *filp, int *id)
{
	long ret;

	PDEBUG("wait_event_interruptible CORE\n");
#ifdef USE_SW_TIMEOUT
	u32 i, status;

	ret = wait_event_interruptible_timeout(subsys_mgr.dec_wait_queue,
					       CheckCoreIrq(dev, filp, id),
		msecs_to_jiffies(2000));
	if (ret < 0) {
		PDEBUG("CORE  wait_event_interruptible interrupted\n");
		return -ERESTARTSYS;
	} else if (ret == 0) {
		PDEBUG("CORE  wait_event_interruptible timeout\n");
		for (i = 0; i < dev->cores; i++) {
			status = ioread32((void *)(dev->hwregs[i][HW_VCD] +
			HANTRODEC_IRQ_STAT_DEC_OFF));
			/* check if HW is enabled */
			if ((status & HANTRODEC_DEC_E) &&
			    subsys_mgr.core_owner[i].filp == filp) {
				pr_info("hantrodec: CORE[%d] reset because of timeout\n",
					i);
				*id = i;
				/* abort decoder */
				status |= HANTRODEC_DEC_ABORT |
						HANTRODEC_DEC_IRQ_DISABLE;
				iowrite32(status,
					  (void *)(dev->hwregs[i][HW_VCD] +
					HANTRODEC_IRQ_STAT_DEC_OFF));
				break;
			}
		}
	}
#else
	ret = wait_event_interruptible(subsys_mgr.dec_wait_queue,
				       CheckCoreIrq(dev, filp, id));
	if (ret) {
		PDEBUG("CORE[%d] wait_event_interruptible interrupted with 0x%x\n",
		       *id, (unsigned int)ret);
		return -ERESTARTSYS;
	}
#endif
	atomic_inc(&subsys_mgr.irq_tx);

	return 0;
}

/*---------------------------------------------------------
 * Function name : hantrodec_ioctl
 * Description   : communication method to/from the user space
 * Return type   : long
 *----------------------------------------------------------
 */
static long hantrodec_ioctl(struct file *filp, unsigned int cmd,
			    unsigned long arg)
{
	int err = 0;
	long tmp;
#ifdef CLK_CFG
	unsigned long flags;
#endif

#ifdef HW_PERFORMANCE
	struct timeval *end_time_arg;
#endif

	PDEBUG("ioctl cmd 0x%08x [ %s ]\n", cmd, IoctlCmdStr(cmd));
	/*
	 * extract the type and number bitfields, and don't decode
	 * wrong cmds: return ENOTTY (inappropriate ioctl)
	 * before access_ok()
	 */
	if (_IOC_TYPE(cmd) != HANTRODEC_IOC_MAGIC &&
#ifdef SUPPORT_MMU
	    _IOC_TYPE(cmd) != HANTRO_IOC_MMU &&
#endif
		_IOC_TYPE(cmd) != HANTRO_VCMD_IOC_MAGIC)

		return -ENOTTY;
	if ((_IOC_TYPE(cmd) == HANTRODEC_IOC_MAGIC &&
	     _IOC_NR(cmd) > HANTRODEC_IOC_MAXNR) ||
#ifdef SUPPORT_MMU
		 (_IOC_TYPE(cmd) == HANTRO_IOC_MMU &&
		 _IOC_NR(cmd) > HANTRO_IOC_MMU_MAXNR) ||
#endif
		 (_IOC_TYPE(cmd) == HANTRO_VCMD_IOC_MAGIC &&
		 _IOC_NR(cmd) > HANTRO_VCMD_IOC_MAXNR))

		return -ENOTTY;

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

#ifdef CLK_CFG
	spin_lock_irqsave(&clk_lock, flags);
	if (clk_cfg && !IS_ERR(clk_cfg) && (is_clk_on == 0)) {
		pr_info("turn on clock by user\n");
		if (clk_enable(clk_cfg)) {
			spin_unlock_irqrestore(&clk_lock, flags);
			return -EFAULT;
		}
		is_clk_on = 1;
	}

	spin_unlock_irqrestore(&clk_lock, flags);
	/*the interval is 10s*/
	mod_timer(&timer, jiffies + 10 * HZ);
#endif

	switch (cmd) {
	case HANTRODEC_IOC_CLI: {
		__u32 id;

		__get_user(id, (__u32 __user *)arg);

		if (id >= subsys_mgr.hantrodec_data.cores)
			return -EFAULT;
		disable_irq(subsys_mgr.hantrodec_data.irq[id]);
		break;
	}
	case HANTRODEC_IOC_STI: {
		__u32 id;

		__get_user(id, (__u32 __user *)arg);

		if (id >= subsys_mgr.hantrodec_data.cores)
			return -EFAULT;
		enable_irq(subsys_mgr.hantrodec_data.irq[id]);
		break;
	}
	case HANTRODEC_IOCGHWOFFSET: {
		__u32 id;

		__get_user(id, (__u32 __user *)arg);

		if (id >= subsys_mgr.hantrodec_data.cores)
			return -EFAULT;

		__put_user(subsys_mgr.multicorebase[id],
			   (unsigned long __user *)arg);
		break;
	}
	case HANTRODEC_IOCGHWIOSIZE: {
		struct regsize_desc core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct regsize_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		if (core.id >= MAX_SUBSYS_NUM)
			return -EFAULT;

		{
			core.size =
			  subsys_mgr.vpu_subsys[core.id].submodule_iosize[core.type];
			if (core.type == HW_VCD && !core.size &&
			    subsys_mgr.vpu_subsys[core.id].submodule_hwregs[HW_VCDJ])
				/* If VCD doesn't exists, while VCDJ
				 * exists, return VCDJ.
				 */
				core.size = subsys_mgr.vpu_subsys[core.id].submodule_iosize[HW_VCDJ];
		}
		tmp = copy_to_user((u32 __user *)arg, &core,
			     sizeof(struct regsize_desc));
		if (tmp) {
			PDEBUG("copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return 0;
	}
	case HANTRODEC_IOC_MC_OFFSETS: {
		tmp = copy_to_user((unsigned long __user *)arg,
				   subsys_mgr.multicorebase,
		 sizeof(subsys_mgr.multicorebase));
		if (tmp) {
			PDEBUG("copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		break;
	}
	case HANTRODEC_IOC_MC_CORES:
		__put_user(subsys_mgr.hantrodec_data.cores, (unsigned int __user *)arg);
		PDEBUG("subsys_mgr.hantrodec_data.cores=%d\n", subsys_mgr.hantrodec_data.cores);
		break;
	case HANTRODEC_IOCS_DEC_PUSH_REG: {
		struct core_desc core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return DecFlushRegs(&subsys_mgr.hantrodec_data, &core);
	}
	case HANTRODEC_IOCS_DEC_WRITE_REG: {
		struct core_desc core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return DecWriteRegs(&subsys_mgr.hantrodec_data, &core);
	}
	case HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG: {
		struct core_desc core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		return DecWriteApbFilterRegs(&subsys_mgr.hantrodec_data, &core);
	}
	case HANTRODEC_IOCS_PP_PUSH_REG: {

	/* get registers from user space*/

		return -EINVAL;
	}
	case HANTRODEC_IOCS_DEC_PULL_REG: {
		struct core_desc core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return DecRefreshRegs(&subsys_mgr.hantrodec_data, &core);
	}
	case HANTRODEC_IOCS_DEC_READ_REG: {
		struct core_desc core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return DecReadRegs(&subsys_mgr.hantrodec_data, &core);
	}
	case HANTRODEC_IOCS_PP_PULL_REG: {
		return -EINVAL;
	}
	case HANTRODEC_IOCH_DEC_RESERVE: {
		struct req_core_info core_info;

		/* get registers from user space*/
		tmp = copy_from_user(&core_info, (void __user *)arg,
				     sizeof(struct req_core_info));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		PDEBUG("Reserve DEC core, format = %d\n", core_info->format);
		return ReserveDecoder(&subsys_mgr.hantrodec_data, filp, &core_info);
	}
	case HANTRODEC_IOCT_DEC_RELEASE: {
		u32 core = 0;

		__get_user(core, (unsigned long __user *)arg);
		if (core >= subsys_mgr.hantrodec_data.cores || subsys_mgr.core_owner[core].filp != filp) {
			PDEBUG("bogus DEC release, core = %d\n", core);
			return -EFAULT;
		}

		PDEBUG("Release DEC, core = %d\n", core);

		ReleaseDecoder(&subsys_mgr.hantrodec_data, core);

		break;
	}
	case HANTRODEC_IOCQ_PP_RESERVE:
		return -EINVAL;
	case HANTRODEC_IOCT_PP_RELEASE: {
		return -EINVAL;
	}
	case HANTRODEC_IOCX_DEC_WAIT: {
		struct core_desc core;

		/* get registers from user space */
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_desc));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return WaitDecReadyAndRefreshRegs(&subsys_mgr.hantrodec_data, &core);
	}
	case HANTRODEC_IOCX_PP_WAIT: {
		return -EINVAL;
	}
	case HANTRODEC_IOCG_CORE_WAIT: {
		int id;

		tmp = WaitCoreReady(&subsys_mgr.hantrodec_data, filp, &id);
		__put_user(id, (int __user *)arg);
		return tmp;
	}
	case HANTRODEC_IOCH_WAIT_OWNER_DONE: {
		PDEBUG("Dec drop owner %p for seek.\n", (void *)arg);

		tmp = wait_owner_done(&subsys_mgr.hantrodec_data, filp, (void *)arg);
		if (tmp < 0) {
			PDEBUG("drop_owner failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return tmp;
	}
	case HANTRODEC_IOX_ASIC_ID: {
		struct core_param core;

		/* get registers from user space*/
		tmp = copy_from_user(&core, (void __user *)arg,
				     sizeof(struct core_param));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}
		/*subsys_mgr.hantrodec_data.cores*/
		if (core.id >= MAX_SUBSYS_NUM  ||
		    ((core.type == HW_VCD ||
	core.type == HW_VCDJ) &&
	!subsys_mgr.vpu_subsys[core.id]
	.submodule_iosize[core.type == HW_VCD] &&
	!subsys_mgr.vpu_subsys[core.id]
	.submodule_iosize[core.type == HW_VCDJ]) ||
	((core.type != HW_VCD && core.type != HW_VCDJ) &&
	!subsys_mgr.vpu_subsys[core.id].submodule_iosize[core.type]))
			return -EFAULT;

		core.size = subsys_mgr.vpu_subsys[core.id].submodule_iosize[core.type];
		if (subsys_mgr.vpu_subsys[core.id].submodule_hwregs[core.type]) {
			core.asic_id = ioread32((void __iomem *)
				subsys_mgr.hantrodec_data.hwregs[core.id][core.type]);
		} else if (core.type == HW_VCD &&
			 subsys_mgr.hantrodec_data.hwregs[core.id][HW_VCDJ]) {
			core.asic_id =
				ioread32((void __iomem *)
				subsys_mgr.hantrodec_data.hwregs[core.id][HW_VCDJ]);
		} else {
			core.asic_id = 0;
		}
		tmp = copy_to_user((u32 __user *)arg, &core,
			     sizeof(struct core_param));
		if (tmp) {
			PDEBUG("copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return 0;
	}
	case HANTRODEC_IOCG_CORE_ID: {
		u32 format = 0;

		__get_user(format, (unsigned long __user *)arg);

		PDEBUG("Get DEC Core_id, format = %d\n", format);
		return GetDecCoreID(&subsys_mgr.hantrodec_data, filp, format);
	}
	case HANTRODEC_IOX_ASIC_BUILD_ID: {
		u32 id, hw_id;

		__get_user(id, (u32 __user *)arg);

		if (id >= subsys_mgr.hantrodec_data.cores)
			return -EFAULT;
		if (subsys_mgr.hantrodec_data.hwregs[id][HW_VCD] ||
		    subsys_mgr.hantrodec_data.hwregs[id][HW_VCDJ]) {
			volatile u8 *hwregs;
			/* VCD first if it exists, otherwise VCDJ. */
			if (subsys_mgr.hantrodec_data.hwregs[id][HW_VCD])
				hwregs = subsys_mgr.hantrodec_data.hwregs[id][HW_VCD];
			else
				hwregs = subsys_mgr.hantrodec_data.hwregs[id][HW_VCDJ];

			hw_id = ioread32((void __iomem *)(hwregs +
						HANTRODEC_HW_BUILD_ID_OFF));
			__put_user(hw_id, (u32 __user *)arg);
		}
		return 0;
	}
	case HANTRODEC_DEBUG_STATUS: {
		pr_info("hantrodec: subsys_mgr.dec_irq = 0x%08x\n", subsys_mgr.dec_irq);
		pr_info("hantrodec: pp_irq = 0x%08x\n", subsys_mgr.pp_irq);

		pr_info("hantrodec: IRQs received/sent2user = %d / %d\n",
			atomic_read(&subsys_mgr.irq_rx), atomic_read(&subsys_mgr.irq_tx));

		for (tmp = 0; tmp < subsys_mgr.hantrodec_data.cores; tmp++) {
			pr_info("hantrodec: dec_core[%li] %s\n", tmp,
				!subsys_mgr.core_owner[tmp].filp ? "FREE" : "RESERVED");
		}
		return 0;
	}
	case HANTRODEC_IOX_SUBSYS: {
		struct subsys_desc subsys = { 0 };
		/* TODO(min): check all the subsys */
		if (use_vcmd) {
			subsys.subsys_vcmd_num = 1;
			subsys.subsys_num = subsys.subsys_vcmd_num;
		} else {
			subsys.subsys_num = subsys_mgr.hantrodec_data.cores;
			subsys.subsys_vcmd_num = 0;
		}
		tmp = copy_to_user((u32 __user *)arg, &subsys,
			     sizeof(struct subsys_desc));
		if (tmp) {
			PDEBUG("copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return 0;
	}
	case HANTRODEC_IOCX_POLL: {
		hantrodec_isr(0, &subsys_mgr.hantrodec_data);
		return 0;
	}
	case HANTRODEC_IOC_APBFILTER_CONFIG: {
		struct apbfilter_cfg tmp_apbfilter;

		/* get registers from user space*/
		tmp = copy_from_user(&tmp_apbfilter, (void __user *)arg,
				     sizeof(struct apbfilter_cfg));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		if (tmp_apbfilter.id >= MAX_SUBSYS_NUM ||
		    tmp_apbfilter.type >= HW_CORE_MAX)
			return -EFAULT;

		subsys_mgr.apbfilter_cfg[tmp_apbfilter.id][tmp_apbfilter.type].id =
			tmp_apbfilter.id;
		subsys_mgr.apbfilter_cfg[tmp_apbfilter.id][tmp_apbfilter.type].type =
			tmp_apbfilter.type;

		memcpy(&tmp_apbfilter,
		       &subsys_mgr.apbfilter_cfg[tmp_apbfilter.id][tmp_apbfilter.type],
		 sizeof(struct apbfilter_cfg));

		tmp = copy_to_user((u32 __user *)arg, &tmp_apbfilter,
			     sizeof(struct apbfilter_cfg));
		if (tmp) {
			PDEBUG("copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return 0;
	}
	case HANTRODEC_IOC_AXIFE_CONFIG: {
		struct axife_cfg tmp_axife;

		/* get registers from user space*/
		tmp = copy_from_user(&tmp_axife, (void __user *)arg,
				     sizeof(struct axife_cfg));
		if (tmp) {
			PDEBUG("copy_from_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		if (tmp_axife.id >= MAX_SUBSYS_NUM)
			return -EFAULT;

		subsys_mgr.axife_cfg[tmp_axife.id].id = tmp_axife.id;

		memcpy(&tmp_axife, &subsys_mgr.axife_cfg[tmp_axife.id],
		       sizeof(struct axife_cfg));

		tmp = copy_to_user((u32 __user *)arg, &tmp_axife,
			     sizeof(struct axife_cfg));
		if (tmp) {
			PDEBUG("copy_to_user failed, returned %li\n", tmp);
			return -EFAULT;
		}

		return 0;
	}
	default: {
#ifdef SUPPORT_MMU
		if (_IOC_TYPE(cmd) == HANTRO_IOC_MMU) {
			volatile u8 *mmu_hwregs[MAX_SUBSYS_NUM][2];
			long ret;
			u32 i = 0;

			for (i = 0; i < MAX_SUBSYS_NUM; i++) {
				mmu_hwregs[i][0] =
				  subsys_mgr.hantrodec_data.hwregs[i][HW_MMU];
				mmu_hwregs[i][1] =
				  subsys_mgr.hantrodec_data.hwregs[i][HW_MMU_WR];
			}
			ret = MMUIoctl(cmd, filp, arg, mmu_hwregs);
			return ret;
		}
#endif
		if (_IOC_TYPE(cmd) == HANTRO_VCMD_IOC_MAGIC)
			//return hantrovcmd_ioctl(filp, cmd, arg);

		return -ENOTTY;
	}
	}

	return 0;
}

#ifdef DTB_SUPPORT
/* get base address and irq number from DTB */
static int get_of_property(void)
{
	struct resource res;
	int irq_num;
	struct device_node *np;
	int ret;
	long vcd_base = 0;

	np = of_find_compatible_node(NULL, NULL, "verisilicon,vcd");

	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_err("can't get VCD base address from DTB. ret=%d,\n", ret);
		return -EINVAL;
	}
	vcd_base = res.start;
	core_array[1].offset = 0;
	core_array[1].core_type = HW_VCD;
	pr_info("get VCD base address 0x%lx\n", vcd_base);

	irq_num = of_irq_get_byname(np, "vcd_irq");
	if (irq_num > 0) {
		pr_info("get VCD irq number %d\n", irq_num);
		core_array[1].irq = irq_num;
	}
	{
		np = of_find_compatible_node(NULL, NULL, "verisilicon,vcmd");

		ret = of_address_to_resource(np, 0, &res);
		if (ret) {
			pr_err("can't get VCMD base address from DTB. ret=%d,\n", ret);
			return -EINVAL;
		}
		subsys_array[0].base = res.start;
		pr_info("get VCMD base address 0x%lx\n", subsys_array[0].base);
		core_array[0].offset = 0;
		core_array[0].core_type = HW_VCMD;
		core_array[1].offset = vcd_base - res.start;

		irq_num = of_irq_get_byname(np, "vcmd_irq");
		if (irq_num > 0) {
			pr_info("get VCMD irq number %d\n", irq_num);
			core_array[0].irq = irq_num;
			core_array[1].irq = -1;
		}
	}

	return 0;
}
#endif /* DTB_SUPPORT */

/*
 * Function name   : hantrodec_open
 * Description     : open method

 * Return type     : int
 */
static int hantrodec_open(struct inode *inode, struct file *filp)
{
	PDEBUG("dev opened\n");
	#ifdef SUPPORT_DBGFS
	if (!use_vcmd) {
		int i, j;

		for (i = 0; i < HXDEC_MAX_CORES; i++) {
			for (j = 0; j < MAX_RESERVED_TIME; j++) {
				subsys_mgr.hw_return_time[i][j] = 0;
				subsys_mgr.reserved_time[i][j] = 0;
				subsys_mgr.start_hw_time[i][j] = 0;
				subsys_mgr.active_start_time[j] = 0;
				subsys_mgr.active_return_time[j] = 0;
				subsys_mgr.release_time[i][j] = 0;
			}
			subsys_mgr.r_index[i] = 0;
			subsys_mgr.hw_r_index[i] = 0;
			subsys_mgr.start_hw_index[i] = 0;
			subsys_mgr.release_index[i] = 0;
		}
		subsys_mgr.active_hw_index = 0;
	}
#endif

#if defined SUPPORT_MMU && defined SUPPORT_48PA_MMU
	if (subsys_mgr.hantrodec_data.hwregs[0][HW_MMU]) {
		int ret = MMUCreatePageTable(filp);

		if (ret < 0) {
			pr_err("MMU Create page table failed!\n");
			return -EINVAL;
		}
#ifdef MMU_PAGE_TABLE_SWITCH
		if (use_vcmd && ret != MMU_STATUS_PT_EXIST)
			_vcmd_memory_map(subsys_mgr.vcmd_mgr, filp);
#endif
	}
#endif

#ifdef CONFIG_DEC_PM
	vcx_vcodec_pm_runtime_get((vcx_priv_t*)subsys_mgr.priv);//	hantrodec_pm_runtime_get(&subsys_mgr.platformdev->dev);
#endif
	return 0;
}

/*
 * Function name   : hantrodec_release
 * Description     : Release driver

 * Return type     : int
 */

static int hantrodec_release(struct inode *inode,
			     struct file *filp)
{
	int n;
	hantrodec_t *dev = &subsys_mgr.hantrodec_data;

	PDEBUG("closing ...\n");

	for (n = 0; n < dev->cores; n++) {
		if (subsys_mgr.core_owner[n].filp == filp) {
			PDEBUG("releasing dec core %i lock\n", n);
			ReleaseDecoder(dev, n);
		}
	}
#ifdef SUPPORT_MMU
	if (subsys_mgr.hantrodec_data.hwregs[0][HW_MMU])
		MMURelease(filp);
#endif

#ifdef CONFIG_DEC_PM
    vcx_vcodec_pm_runtime_put((vcx_priv_t*)subsys_mgr.priv);//hantrodec_pm_runtime_put(&subsys_mgr.platformdev->dev);
#endif

	PDEBUG("closed\n");
	return 0;
}

#ifdef CLK_CFG
void hantrodec_disable_clk(unsigned long value)
{
	unsigned long flags;
	/* entering this function means decoder
	 * is idle over expiry.So disable clk
	 */
	if (clk_cfg && !IS_ERR(clk_cfg)) {
		spin_lock_irqsave(&clk_lock, flags);
		if (is_clk_on == 1) {
			clk_disable(clk_cfg);
			is_clk_on = 0;
			pr_info("turned off hantrodec clk\n");
		}
		spin_unlock_irqrestore(&clk_lock, flags);
	}
}
#endif

/* VFS methods */
static const struct file_operations hantrodec_fops = {
	.owner = THIS_MODULE,
	.open = hantrodec_open,
	.mmap = hantrodec_mmap,
	.release = hantrodec_release,
	.unlocked_ioctl = hantrodec_ioctl,
	.fasync = NULL,
};

static const struct vm_operations_struct hantrodec_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys,
#endif
};

/**
 * @brief init apbfilter ctx
 * @param u32 hwid: hardware build id
 */
static void apbfilter_ctx_init(u32 subsys_id, u32 core_id, volatile u8 *hwregs, u32 hwid)
{
	u32 i = subsys_id, j = core_id;

	if (subsys_mgr.vpu_subsys[i].has_apbfilter[j]) {
		subsys_mgr.apbfilter_cfg[i][j].has_apbfilter = 1;

		if (hwid == 0x1F58 && j == HW_VCD) {
			subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs = VCD_NUM_MASK_REG;
			subsys_mgr.apbfilter_cfg[i][j].num_mode = VCD_NUM_MODE;
			subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset = VCD_MASK_REG_OFFSET;
			subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg = VCD_MASK_BITS_PER_REG;
			subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
			subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
			subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
		}
		if (hwid == 0x1F59 && j == HW_VCDJ) {
			subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs = VCDJ_NUM_MASK_REG;
			subsys_mgr.apbfilter_cfg[i][j].num_mode = VCDJ_NUM_MODE;
			subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset = VCDJ_MASK_REG_OFFSET;
			subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg = VCDJ_MASK_BITS_PER_REG;
			subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
			subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
			subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
		}

		if ((hwid == 0x1F58 || hwid == 0x1F58) && j == HW_AXIFE) {
			subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs = AXIFE_NUM_MASK_REG;
			subsys_mgr.apbfilter_cfg[i][j].num_mode = AXIFE_NUM_MODE;
			subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset = AXIFE_MASK_REG_OFFSET;
			subsys_mgr.apbfilter_cfg[i][j].mask_bits_per_reg = AXIFE_MASK_BITS_PER_REG;
			subsys_mgr.apbfilter_cfg[i][j].page_sel_addr =
			subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset +
			subsys_mgr.apbfilter_cfg[i][j].nbr_mask_regs * 4;
		}

		if (hwid != 0x1F58 && hwid != 0x1F58)
			pr_info("hantrodec: furture APBFILTER can read those configure parameters from REG\n");

			subsys_mgr.hantrodec_data.apbfilter_hwregs[i][j] =
					hwregs + subsys_mgr.apbfilter_cfg[i][j].mask_reg_offset;
		} else {
			subsys_mgr.apbfilter_cfg[i][j].has_apbfilter = 0;
		}
}

/**
 * @brief init axife ctx
 * @param u32 hwid: hardware build id
 */
static void axife_ctx_init(u32 subsys_id, u32 core_id, volatile u8 *hwregs, u32 hwid)
{
	u32 i = subsys_id, j = core_id;
	u32 axife_config;
	u32 *regs_va;

	if (j == HW_AXIFE) {
		if (!use_vcmd) {
			axife_config = ioread32((void __iomem *)hwregs);
		} else {
			regs_va = get_submodule_regs_va(subsys_mgr.vcmd_mgr, i, SUB_MOD_AXIFE);
			axife_config = *(regs_va + 0);
		}

		subsys_mgr.axife_cfg[i].axi_rd_chn_num = axife_config & 0x7F;
		subsys_mgr.axife_cfg[i].axi_wr_chn_num = (axife_config >> 7) & 0x7F;
		subsys_mgr.axife_cfg[i].axi_rd_burst_length = (axife_config >> 14) & 0x1F;
		subsys_mgr.axife_cfg[i].axi_wr_burst_length = (axife_config >> 22) & 0x1F;
		subsys_mgr.axife_cfg[i].fe_mode = 0; /*need to read from reg in furture*/
		if (hwid == 0x1F66)
			subsys_mgr.axife_cfg[i].fe_mode = 1;
	}
}

/**
 * @brief init auxiliary core ctx
 */
static void auxcore_ctx_init(void)
{
	int i, j;
	u32 hwid = 0;
	u32 core_type = HW_VCD; /* vcd */
	volatile u8 *hwregs;
	u32 *regs_va;

	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (j == HW_VCMD) {
				hwregs = ((vcmd_mgr_t*) subsys_mgr.vcmd_mgr)->dev_ctx[i].subsys_info->hwregs[SUB_MOD_VCMD];//hwregs = vcmd_core_array[i].submodule_vcmd_virtual_address;
			 } else {
				hwregs = subsys_mgr.hantrodec_data.hwregs[i][j];
			}

			if (!hwregs)
				continue;

			if (!use_vcmd)
				hwid = ioread32((void __iomem *)
						(subsys_mgr.hantrodec_data.hwregs[i][core_type] +
						HANTRODEC_HW_BUILD_ID_OFF));
			else {
				regs_va = get_submodule_regs_va(subsys_mgr.vcmd_mgr, i, SUB_MOD_MAIN);
				hwid = *(regs_va + HANTRODEC_HW_BUILD_ID_OFF / 4);
			}

			// init apbfilter config
			apbfilter_ctx_init(i, j, hwregs, hwid);
			// inti axife config
			axife_ctx_init(i, j, hwregs, hwid);

		}
	}
}

/**
 * @brief set interrupt gate and not gate the interrupts from IPs to CPU
 */
static void dec_interrupt_gate_set(volatile u8 *hwregs)
{
	iowrite32(0x0000, (void __iomem *)(hwregs + 25 * 4));
}

#ifdef PCIE_EN
static int PcieInit(void)
{
	/* Base register address Length */
	u32 pci_reg_len, pci_ddr_len;

	gDev = pci_get_device(PCI_VENDOR_ID_HANTRO,
			      PCI_DEVICE_ID_HANTRO,
			gDev);
	if (!gDev) {
		pr_info("Init: Hardware not found.\n");
		goto out;
	}

	if (pci_enable_device(gDev) < 0) {
		pr_info("%s: Device not enabled.\n", __func__);
		goto out;
	}

	gBaseHdwr = pci_resource_start(gDev, PCI_CONTROL_BAR);
	if (gBaseHdwr == 0) {
		pr_info("%s: Base Address not set.\n", __func__);
		goto out_pci_disable_device;
	}
	pr_info("Base hw val 0x%lX\n", gBaseHdwr);

	pci_reg_len = pci_resource_len(gDev, PCI_CONTROL_BAR);
	pr_info("Base hw len 0x%x\n", pci_reg_len);

	gBaseDDRHw = pci_resource_start(gDev, PCI_DDR_BAR);
	if (gBaseDDRHw == 0) {
		pr_info("%s: Base Address not set.\n", __func__);
		goto out_pci_disable_device;
	}
	pr_info("Base memory val 0x%lx\n", gBaseDDRHw);

	pci_ddr_len = pci_resource_len(gDev, PCI_DDR_BAR);
	pr_info("Base memory len 0x%x\n", pci_ddr_len);

	return 0;

out_pci_disable_device:
	pci_disable_device(gDev);

out:
	return -1;
}
#endif


static void CheckSubsysCoreArray(struct subsys_cfg *subsys, int *subsys_num, int *vcmd)
{
	int num = ARRAY_SIZE(subsys_array);
	int i, j;

	memset(subsys, 0, sizeof(struct subsys_cfg) * MAX_SUBSYS_NUM);
	for (i = 0; i < num; i++) {
		subsys[i].base_addr = subsys_array[i].base_addr + gBaseHdwr;
		subsys[i].irq = -1;
		for (j = 0; j < HW_CORE_MAX; j++) {
			subsys[i].submodule_offset[j] = 0xffff;
			subsys[i].submodule_iosize[j] = 0;
			subsys[i].submodule_hwregs[j] = NULL;
		}
	}

	for (i = 0; i < ARRAY_SIZE(core_array); i++) {
		if (!subsys[core_array[i].subsys_idx].base_addr) {
			/* undefined subsystem */
			continue;
		}

		if (core_array[i].offset == 0xFFFF)
			continue;
		if (core_array[i].core_type == HW_VCDJ)
			core_array[i].core_type = HW_VCD;
		subsys[core_array[i].subsys_idx].submodule_offset[core_array[i].core_type] =
			core_array[i].offset;
		subsys[core_array[i].subsys_idx].submodule_iosize[core_array[i].core_type] =
			core_array[i].reg_size;
		if (subsys[core_array[i].subsys_idx].irq != -1 &&
		    core_array[i].irq != -1) {
			if (subsys[core_array[i].subsys_idx].irq !=
			    core_array[i].irq) {
				pr_info("hantrodec: hw core type %d irq %d != subsystem irq %d\n",
					core_array[i].core_type,
				       core_array[i].irq,
				       subsys[core_array[i].subsys_idx].irq);
				pr_info("hantrodec: hw cores of a subsystem should have same irq\n");
			}
		} else if (core_array[i].irq != -1) {
			subsys[core_array[i].subsys_idx].irq = core_array[i].irq;
		}
		subsys[core_array[i].subsys_idx].has_apbfilter[core_array[i].core_type] = 0x00;//			core_array[i].has_apb;
		/* vcmd found */
		if (core_array[i].core_type == HW_VCMD)
			*vcmd = 1;
		else if (core_array[i].core_type == HW_VCD)
			subsys[core_array[i].subsys_idx].subsys_type = VCMD_TYPE_DECODER; /* vcd */
	}

	pr_info("hantrodec: vcmd = %d\n", *vcmd);

	*subsys_num = num;
}


/*
 *Function name   : hantrodec_init
 *Description     : Initialize the driver

 *Return type     : int
 */

int hantrodec_normal_init(vcx_priv_t *priv, int vcmd_supported)
{
	int result = 0, i;
	int subsys_num;
	struct SubsysMgr *owner = &subsys_mgr;
	owner->platformdev = priv->pdev;
	use_vcmd = vcmd_supported;

	PDEBUG("module init\n");

#ifdef DTB_SUPPORT
	result = get_of_property();
	if (result)
		goto err;
#endif

#ifdef PCIE_EN
	result = PcieInit();
	if (result)
		goto err;
#else

#if KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE
	of_dma_configure(&subsys_mgr.platformdev->dev,
			 subsys_mgr.platformdev->dev.of_node,
	 true);
#else
	of_dma_configure(&subsys_mgr.platformdev->dev, subsys_mgr.platformdev->dev.of_node);
#endif

	if (dma_set_mask_and_coherent(&subsys_mgr.platformdev->dev,
				      DMA_BIT_MASK(48)))
		pr_err("48bit hantrodma dev: No suitable DMA available\n");

	if (dma_set_coherent_mask(&subsys_mgr.platformdev->dev, DMA_BIT_MASK(48)))
		pr_err("48bit hantrodma dev: No suitable DMA available\n");

	//result = platform_driver_register
	//(&hantro_drm_platform_driver);
	//pr_info(KERN_NOTICE
	//"Platform driver status is %d\n", result);
#endif

    CheckSubsysCoreArray(subsys_mgr.vpu_subsys, &subsys_num, &vcmd);

	if (vcmd == 0)
		use_vcmd = 0;
	if (use_vcmd == 1)
		pr_info("hantrodec: use vcmd mode!");
	else
		pr_info("hantrodec: use normal mode!");

	pr_info("hantrodec: dec/pp kernel module.\n");

	memset(subsys_mgr.multicorebase, 0, sizeof(subsys_mgr.multicorebase[0]) * HXDEC_MAX_CORES);
	for (i = 0; i < subsys_num; i++) {
		u32 core_type = HW_VCD; /* vcd */

		subsys_mgr.multicorebase[i] = subsys_mgr.vpu_subsys[i].base_addr + subsys_mgr.vpu_subsys[i].submodule_offset[core_type];
		subsys_mgr.irq[i] = subsys_mgr.vpu_subsys[i].irq;
		subsys_mgr.iosize[i] = subsys_mgr.vpu_subsys[i].submodule_iosize[core_type];
		pr_info("hantrodec: [%d] multicorebase 0x%08lx, iosize %d\n", i, subsys_mgr.multicorebase[i], subsys_mgr.iosize[i]);
	}
	pr_info("hantrodec: Init multi core[0] at 0x%16lx\n"
			" core[1] at 0x%16lx\n"
			" core[2] at 0x%16lx\n"
			" core[3] at 0x%16lx\n"
			" IRQ_0=%i\n"
			" IRQ_1=%i\n",
			subsys_mgr.multicorebase[0], subsys_mgr.multicorebase[1],
			subsys_mgr.multicorebase[2],
			subsys_mgr.multicorebase[3], subsys_mgr.irq[0], subsys_mgr.irq[1]);

	subsys_mgr.hantrodec_data.cores = 0;

	subsys_mgr.hantrodec_data.iosize[0] = DEC_IO_SIZE_0;
	subsys_mgr.hantrodec_data.irq[0] = subsys_mgr.irq[0];
	subsys_mgr.hantrodec_data.iosize[1] = DEC_IO_SIZE_1;
	subsys_mgr.hantrodec_data.irq[1] = subsys_mgr.irq[1];

	for (i = 0; i < HXDEC_MAX_CORES; i++) {
		int j;

		for (j = 0; j < HW_CORE_MAX; j++)
			subsys_mgr.hantrodec_data.hwregs[i][j] = NULL;
		/* If user gave less core bases that we have
		 * by default,invalidate default bases
		 */
		if (subsys_mgr.elements && i >= subsys_mgr.elements)
			subsys_mgr.multicorebase[i] = 0;
	}


	result = vcx_create_devnode(priv, &hantrodec_fops);	//result = register_chrdev(subsys_mgr.hantrodec_major, dec_dev_n, &hantrodec_fops);
	if (result < 0) {
		pr_info("hantrodec: unable to get major %d\n",
			subsys_mgr.hantrodec_major);
		goto err;
	} else if (result != 0) {
		/* this is for dynamic major */
		subsys_mgr.hantrodec_major = result;
	}

#ifdef CLK_CFG
	/* first get clk instance pointer */
	clk_cfg = clk_get(NULL, CLK_ID);
	if (!clk_cfg || IS_ERR(clk_cfg)) {
		pr_err("get handrodec clk failed!\n");
		goto err;
	}

	/* prepare and enable clk */
	if (clk_prepare_enable(clk_cfg)) {
		pr_err("try to enable handrodec clk failed!\n");
		goto err;
	}
	is_clk_on = 1;

	/* init a timer to disable clk */
	init_timer(&timer);
	timer.function = &hantrodec_disable_clk;
	/* the expires time is 100s */
	timer.expires = jiffies + 100 * HZ;
	add_timer(&timer);
#endif

	result = ReserveIO();
	if (result < 0)
		goto err;

	/* for non-vcmd mode: unmap and release mem region for VCMD,
	 * since it will be mapped and reserved again in hantro_vcmd.c
	 */
	if (vcmd && !use_vcmd) {
		for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
			u32 vcmd_offset;
			u32 vcmd_iosize;

			if (!subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD])
				continue;

			vcmd_offset = 0;
			vcmd_iosize = 30 * 4;
			// interrupt gate set
			dec_interrupt_gate_set(subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD]);

			iounmap((void __iomem *)subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD]);
			release_mem_region(subsys_mgr.vpu_subsys[i].base_addr + vcmd_offset, vcmd_iosize);
			subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD] = NULL;
		}
	}

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
#ifdef SUPPORT_AXIFE
		AXIFEEnable(subsys_mgr.hantrodec_data.hwregs[i][HW_AXIFE]);
#endif
#ifdef AXI2TO1_SUPPORT
		AXI2TO1_init(subsys_mgr.hantrodec_data.hwregs[i][HW_AXI2TO1]);
#endif
	}

#if 0
#ifdef SUPPORT_AFBC
		for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++)
			AFBCBypass(subsys_mgr.hantrodec_data.hwregs[i][HW_AFBC]);
#endif
#endif

#ifdef SUPPORT_MMU
	/* MMU only initial once No matter how many MMU we have */
	if (subsys_mgr.hantrodec_data.hwregs[0][HW_MMU]) {
		enum MMUStatus status = MMUInit(subsys_mgr.hantrodec_data.hwregs[0][HW_MMU]);
		volatile u8 *mmu_hwregs[MAX_SUBSYS_NUM][2];

		if (status == MMU_STATUS_NOT_FOUND)
			pr_info("MMU does not exist!\n");
		else if (status != MMU_STATUS_OK)
			goto err;
		else
			pr_info("MMU detected!\n");

		for (i = 0; i < MAX_SUBSYS_NUM; i++) {
			mmu_hwregs[i][0] = subsys_mgr.hantrodec_data.hwregs[i][HW_MMU];
			mmu_hwregs[i][1] = subsys_mgr.hantrodec_data.hwregs[i][HW_MMU_WR];
		}
       MMUEnable(mmu_hwregs, subsys_mgr.platformdev);
	}
#endif

#ifdef SUPPORT_DBGFS
	if (!use_vcmd) {
		int j = 0;

		for (i = 0; i < HXDEC_MAX_CORES; i++) {
			for (j = 0; j < MAX_RESERVED_TIME; j++) {
				subsys_mgr.hw_return_time[i][j] = 0;
				subsys_mgr.reserved_time[i][j] = 0;
				subsys_mgr.start_hw_time[i][j] = 0;
				subsys_mgr.release_time[i][j] = 0;
				subsys_mgr.active_start_time[j] = 0;
				subsys_mgr.active_return_time[j] = 0;
			}
			subsys_mgr.r_index[i] = 0;
			subsys_mgr.hw_r_index[i] = 0;
			subsys_mgr.start_hw_index[i] = 0;
			subsys_mgr.release_index[i] = 0;
			subsys_mgr.decode_state[i] = 0;
		}
		subsys_mgr.active_hw_index = 0;
		debug_root = debugfs_create_dir("VCDEC", NULL);
		debugfs_create_file("decode_cycles", 0444, debug_root, NULL, &fileop_cycles);
		debugfs_create_file("performance_statistic", 0444, debug_root, NULL, &fileop_perfor_statistic);
		debugfs_create_file("subsys_state", 0444, debug_root, NULL, &fileop_subsys_state);
		debugfs_create_file("regprint", 0444, debug_root, NULL, &fileop_hw_reg_print);
	}
#endif
/*
	if (use_vcmd) {
		subsys_mgr.vcmd_mgr = hantrovcmd_init(priv);
		if (!subsys_mgr.vcmd_mgr)
			goto err;
	}*/

	/* check for correct HW */
	if (!CheckHwId(&subsys_mgr.hantrodec_data)) {
		result = -EBUSY;
		goto err;
	}

	/* read configuration for all cores */
	ReadCoreConfig(&subsys_mgr.hantrodec_data);

	auxcore_ctx_init();

	if (use_vcmd) {
		return 0;
	}

	priv->priv = owner;
    owner->priv = priv;

	memset(subsys_mgr.core_owner, 0, sizeof(subsys_mgr.core_owner));

	sema_init(&subsys_mgr.dec_core_sem, subsys_mgr.hantrodec_data.cores);

	atomic_set(&subsys_mgr.irq_rx, 0);
	atomic_set(&subsys_mgr.irq_tx, 0);
	spin_lock_init(&subsys_mgr.owner_lock);
	init_waitqueue_head(&subsys_mgr.dec_wait_queue);
	init_waitqueue_head(&subsys_mgr.pp_wait_queue);
	init_waitqueue_head(&subsys_mgr.hw_queue);

	/* reset hardware */
	ResetAsic(&subsys_mgr.hantrodec_data);

	/* register irq for each core */
	if (subsys_mgr.irq[0] > 0) {
		result = request_irq(subsys_mgr.irq[0], hantrodec_isr,
				     #if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
			 SA_INTERRUPT | SA_SHIRQ,
#else
			 IRQF_SHARED,
#endif
			 "hantrodec",
			 (void *)&subsys_mgr.hantrodec_data);

		if (result != 0) {
			if (result == -EINVAL) {
				pr_err("hantrodec: Bad irq number or handler\n");
			} else if (result == -EBUSY) {
				pr_err("hantrodec: IRQ <%d> busy, change your config\n",
				       subsys_mgr.hantrodec_data.irq[0]);
			}
			goto err;
		}
	} else {
		pr_info("hantrodec: IRQ not in use!\n");
	}

	if (subsys_mgr.irq[1] > 0) {
		result = request_irq(subsys_mgr.irq[1], hantrodec_isr,
				     #if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
			 SA_INTERRUPT | SA_SHIRQ,
#else
			 IRQF_SHARED,
#endif
			 "hantrodec", (void *)&subsys_mgr.hantrodec_data);

		if (result != 0) {
			if (result == -EINVAL) {
				pr_err("hantrodec: Bad irq number or handler\n");
			} else if (result == -EBUSY) {
				pr_err("hantrodec: IRQ <%d> busy, change your config\n",
				       subsys_mgr.hantrodec_data.irq[1]);
			}

			goto err;
		}
	} else {
		pr_info("hantrodec: IRQ not in use!\n");
	}

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		u32 core_type = HW_VCD;
		volatile u8 *hwregs = subsys_mgr.hantrodec_data.hwregs[i][core_type];

		if (!hwregs) {
			core_type = HW_VCDJ;
			hwregs = subsys_mgr.hantrodec_data.hwregs[i][core_type];
		}

		if (hwregs) {
			pr_info("hantrodec: %s [%d] has build id 0x%08x\n",
				CoreTypeStr(core_type), i,
				ioread32((void __iomem *)
				(hwregs + HANTRODEC_HWBUILD_ID_OFF)));
		}
#ifdef SUPPORT_WATCHDOG
		subsys_mgr.hantrodec_data.watchdog[i].triggered = 0;
		subsys_mgr.hantrodec_data.watchdog[i].core_id = i;
		subsys_mgr.hantrodec_data.watchdog[i].active = 0;
#endif
	}

	/* create kthread for hantrodec driver, but it in sleep status, should wake up it when needed */
	if (!use_vcmd)
		hantrodec_kthread_create();

	pr_info("hantrodec: module inserted. Major = %d\n",
		subsys_mgr.hantrodec_major);

	/* Please call the TEE functions to
	 * set VCD DRM relative registers here
	 */

	return 0;

err:
#ifdef PCIE_EN
	if (gDev) {
		pci_disable_device(gDev);
		gDev = NULL;
	}
#endif
	ReleaseIO();
	pr_info("hantrodec: module not inserted\n");
//	unregister_chrdev(subsys_mgr.hantrodec_major, dec_dev_n);
	return result;
}

/*
 * Function name   : hantrodec_normal_cleanup
 * Description     : clean up
 * Return type     : int
 */
void  hantrodec_normal_cleanup(vcx_priv_t *priv)
{
	struct SubsysMgr *owner = (struct SubsysMgr *) priv->priv;
	hantrodec_t *dev = &owner->hantrodec_data;
	int i, n = 0;
#ifdef SUPPORT_MMU
	int has_mmu = 0;
#endif

	for (i = 0; i < MAX_SUBSYS_NUM; i++) {
		if (!owner->vpu_subsys[i].base_addr)
			continue;

#if 0
#ifdef VCARB_REQUEST
		vcmd_request_arbiter(owner->vcmd_mgr, i);
#endif

		if (dev->hwregs[i][HW_DEC400]) {
			/* disable dec400 when rmmod driver. */
			iowrite32(0x00810002,
				  (void __iomem *)(dev->hwregs[i][HW_DEC400] +
				 0x800));
		}

#ifdef VCARB_REQUEST
		vcmd_release_arbiter(owner->vcmd_mgr, i);
#endif
#endif
#ifdef SUPPORT_MMU
		if (dev->hwregs[i][HW_MMU])
			has_mmu = 1;
#endif
	}
	if (use_vcmd) {
//		hantrodec_vcmd_cleanup(priv);//hantrovcmd_cleanup(owner->vcmd_mgr);
	} else {
		/* reset hardware */
		ResetAsic(dev);

		/* free the IRQ */
		for (n = 0; n < dev->cores; n++) {
			if (dev->irq[n] != -1)
				free_irq(dev->irq[n], (void *)dev);
		}
		hantrodec_kthread_stop();
	}
#ifdef SUPPORT_MMU
	if (has_mmu)
		MMUCleanup(owner->platformdev);
#endif
	ReleaseIO();
#ifdef SUPPORT_DBGFS
	debugfs_remove_recursive(debug_root);
	debug_root = NULL;
#endif

#ifdef CLK_CFG
	if (clk_cfg && !IS_ERR(clk_cfg)) {
		clk_disable_unprepare(clk_cfg);
		is_clk_on = 0;
		pr_info("turned off hantrodec clk\n");
	}

	/*delete timer*/
#if LINUX_VERSION_CODE >= KERNEL_VERSION(7, 0, 0)
		timer_delete(&timer);
#else
		del_timer(&timer);
#endif

#endif

#ifdef PCIE_EN
	if (gDev) {
		pci_disable_device(gDev);
		gDev = NULL;
	}
#endif

	//unregister_chrdev(subsys_mgr.hantrodec_major, dec_dev_n);
    priv->priv = NULL;
	pr_info("hantrodec: module removed\n");
}

/*
 *Function name   : CheckHwId
 *Return type     : int
 */
static int CheckHwId(hantrodec_t *dev)
{
	int hwid;
	int i, j;
	size_t num_hw = sizeof(DecHwId) / sizeof(*DecHwId);
	u32 *regs_va;

	int found = 0;

	for (i = 0; i < dev->cores; i++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if ((j == HW_VCD || j == HW_VCDJ) &&
			    dev->hwregs[i][j]) {
				if (!use_vcmd) {
					hwid = ioread32((void __iomem *)dev->hwregs[i][j]);
				} else {
					regs_va = get_submodule_regs_va(subsys_mgr.vcmd_mgr, i, SUB_MOD_MAIN);
					hwid = *regs_va;
				}
				pr_info("hantrodec: core %d:%d HW ID=0x%08x [ %s ]\n",
					i, j, hwid, CoreTypeStr(j));
				/* product version only */
				hwid = (hwid >> 16) & 0xFFFF;
				while (num_hw--) {
					if (hwid == DecHwId[num_hw]) {
						pr_info("hantrodec: Supported HW found at 0x%16lx\n",
							subsys_mgr.vpu_subsys[i].base_addr +
							subsys_mgr.vpu_subsys[i].submodule_offset[j]);
						found++;
						dev->hw_id[i][j] = hwid;
						break;
					}
				}

				if (!found) {
					pr_info("hantrodec: Unknown HW found at 0x%16lx\n",
						subsys_mgr.multicorebase[i]);
					return 0;
				}

				found = 0;
				num_hw = sizeof(DecHwId) / sizeof(*DecHwId);
			}
		}
	}

	return 1;
}

/*
 * Function name   : ReserveIO
 * Description     : IO reserve
 * Return type     : int
 */
static int ReserveIO(void)
{
	int i, j;
	u32 vcmd_offset;
	u32 vcmd_iosize;
	volatile u8 *vcmd_hwregs;

	memcpy((unsigned int *)(subsys_mgr.hantrodec_data.iosize), subsys_mgr.iosize,
	       HXDEC_MAX_CORES * sizeof(unsigned int));
	memcpy((unsigned int *)(subsys_mgr.hantrodec_data.irq), subsys_mgr.irq,
	       HXDEC_MAX_CORES * sizeof(int));

	for (i = 0; i < MAX_SUBSYS_NUM; i++) {

		if (!subsys_mgr.vpu_subsys[i].base_addr)
			continue;

		for (j = 0; j < HW_CORE_MAX; j++) {
			if (subsys_mgr.vpu_subsys[i].submodule_iosize[j]) {
				pr_info("hantrodec: subsys %d: core %d [ %s ]\n", i, j, CoreTypeStr(j));
				pr_info("hantrodec: base=0x%08llx, iosize=%d\n",
					(unsigned long long)subsys_mgr.vpu_subsys[i].base_addr +
					subsys_mgr.vpu_subsys[i].submodule_offset[j],
					subsys_mgr.vpu_subsys[i].submodule_iosize[j]);

				if (j == HW_VCMD || j == HW_ARB)
					continue;

				if (!request_mem_region(subsys_mgr.vpu_subsys[i].base_addr +
					subsys_mgr.vpu_subsys[i].submodule_offset[j],
					subsys_mgr.vpu_subsys[i].submodule_iosize[j],
						"hantrodec0")) {
					pr_info("hantrodec: failed to reserve HW %d regs\n",
						j);
					return -EBUSY;
				}
#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
				subsys_mgr.vpu_subsys[i].submodule_hwregs[j] =
					subsys_mgr.hantrodec_data.hwregs[i][j] =
					(volatile u8 __force *)
					ioremap_nocache(subsys_mgr.vpu_subsys[i].base_addr +
					subsys_mgr.vpu_subsys[i].submodule_offset[j],
					subsys_mgr.vpu_subsys[i].submodule_iosize[j]);
#else
				subsys_mgr.vpu_subsys[i].submodule_hwregs[j] =
					subsys_mgr.hantrodec_data.hwregs[i][j] =
						(volatile u8 *)ioremap(subsys_mgr.vpu_subsys[i].base_addr +
							subsys_mgr.vpu_subsys[i].submodule_offset[j],
							subsys_mgr.vpu_subsys[i].submodule_iosize[j]);
#endif

				if (!subsys_mgr.hantrodec_data.hwregs[i][j]) {
					pr_info("hantrodec: failed to ioremap HW %d regs\n",
						j);
					release_mem_region(
						subsys_mgr.vpu_subsys[i].base_addr +
						subsys_mgr.vpu_subsys[i].submodule_offset[j],
						subsys_mgr.vpu_subsys[i].submodule_iosize[j]);

					return -EBUSY;
				}
			} else {
				subsys_mgr.hantrodec_data.hwregs[i][j] = NULL;
			}
		}

		/* non-vcmd need map vcmd io reource for reset interrupt gate */
		if ((vcmd && !use_vcmd) && !subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD]) {
			vcmd_offset = 0;
			vcmd_iosize = 30 * 4;
			if (!request_mem_region(subsys_mgr.vpu_subsys[i].base_addr + vcmd_offset,
									vcmd_iosize, "vcmd_driver")) {
				pr_info("hantrodec: failed to reserve VCMD regs\n");
				return -EBUSY;
			}

#if (KERNEL_VERSION(4, 17, 0) > LINUX_VERSION_CODE)
			vcmd_hwregs = (volatile u8 __force *)ioremap_nocache(subsys_mgr.vpu_subsys[i].base_addr +
						   vcmd_offset, vcmd_iosize);
#else
			vcmd_hwregs = (volatile u8 __force *)ioremap(subsys_mgr.vpu_subsys[i].base_addr +
							vcmd_offset, vcmd_iosize);
#endif
			if (!vcmd_hwregs) {
				release_mem_region(subsys_mgr.vpu_subsys[i].base_addr +
						vcmd_offset, vcmd_iosize);

				return -EBUSY;
			}

			subsys_mgr.hantrodec_data.hwregs[i][HW_VCMD] = vcmd_hwregs;
		}

		subsys_mgr.hantrodec_data.cores++;
	}

	return 0;
}

/*
 * Function name   : releaseIO
 * Description     : release
 * Return type     : void
 */

static void ReleaseIO(void)
{
	int i, j;

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		for (j = 0; j < HW_CORE_MAX; j++) {
			if (subsys_mgr.hantrodec_data.hwregs[i][j]) {
				iounmap((void __iomem *)
				subsys_mgr.hantrodec_data.hwregs[i][j]);
				release_mem_region(subsys_mgr.vpu_subsys[i].base_addr +
				  subsys_mgr.vpu_subsys[i].submodule_offset[j],
				  subsys_mgr.vpu_subsys[i].submodule_iosize[j]);
				subsys_mgr.hantrodec_data.hwregs[i][j] = NULL;
			}
		}
	}
}

/*
 * Function name   : hantrodec_isr
 * Description     : interrupt handler

 * Return type     : irqreturn_t
 */
#if (KERNEL_VERSION(2, 6, 18) > LINUX_VERSION_CODE)
static irqreturn_t hantrodec_isr(int irq, void *dev_id, struct pt_regs *regs)
#else
static irqreturn_t hantrodec_isr(int irq, void *dev_id)
#endif
{
	unsigned long flags;
	unsigned int handled = 0;
	int i;
	volatile u8 *hwregs;
#ifdef SUPPORT_DBGFS
	u64 time_num;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)
	struct timeval time_now;
#else
	struct timespec64 time_now;
#endif
#endif
	hantrodec_t *dev = (hantrodec_t *)dev_id;
	u32 irq_status_dec;

	spin_lock_irqsave(&subsys_mgr.owner_lock, flags);

	for (i = 0; i < dev->cores; i++) {
		u32 core_type = HW_VCD; /* vcd */

		if (!dev->hwregs[i][core_type] && dev->hwregs[i][HW_VCDJ])
			core_type = HW_VCDJ; /* vcdj */

		hwregs = dev->hwregs[i][core_type];

		/* interrupt status register read */
		irq_status_dec = ioread32((void __iomem *)
			(hwregs + HANTRODEC_IRQ_STAT_DEC_OFF));

		if (irq_status_dec & HANTRODEC_DEC_IRQ) {
#ifdef SUPPORT_WATCHDOG
			//if there is an interrupt from VCD, should stop the watchdog.
			_watchdog_stop(&dev->watchdog[i]);
#endif
#ifdef SUPPORT_DBGFS
			subsys_mgr.decode_state[i] = DONE;
#if (KERNEL_VERSION(4, 19, 94) > LINUX_VERSION_CODE)

			do_gettimeofday(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_usec / 1000;
#else

			ktime_get_real_ts64(&time_now);
			time_num = time_now.tv_sec * 1000 + time_now.tv_nsec / 1000000;
#endif
			subsys_mgr.hw_return_time[i][subsys_mgr.hw_r_index[i]] = time_num;
			subsys_mgr.hw_r_index[i] = subsys_mgr.hw_r_index[i] + 1;
			if (subsys_mgr.hw_r_index[i] == MAX_RESERVED_TIME)
				subsys_mgr.hw_r_index[i] = 0;
#endif
		/* clear dec IRQ */

			irq_status_dec &= (~HANTRODEC_DEC_IRQ);
			iowrite32(irq_status_dec,
				  (void __iomem *)(hwregs +
				 HANTRODEC_IRQ_STAT_DEC_OFF));

			PDEBUG("decoder IRQ received! core %d\n", i);
#ifdef CONFIG_DEC_PM
			dev->hw_active[i] = 0;
#endif
			atomic_inc(&subsys_mgr.irq_rx);

			subsys_mgr.dec_irq |= (1 << i);

			wake_up_interruptible_all(&subsys_mgr.dec_wait_queue);
			handled++;
		}
	}

	spin_unlock_irqrestore(&subsys_mgr.owner_lock, flags);

	if (!handled)
		PDEBUG("IRQ received, but not hantrodec's!\n");

	(void)hwregs;
	return IRQ_RETVAL(handled);
}

/*
 * Function name   : ResetAsic
 * Description     : reset asic (only VCD supports reset)

 * Return type     :
 */
static void ResetAsic(hantrodec_t *dev)
{
	int i, j;
	u32 status;

	for (j = 0; j < dev->cores; j++) {
		if (!dev->hwregs[j][HW_VCD])
			continue;

		status = ioread32((void __iomem *)(dev->hwregs[j][HW_VCD] +
						  HANTRODEC_IRQ_STAT_DEC_OFF));

		if (status & HANTRODEC_DEC_E) {
			/* abort with IRQ disabled */
			status = HANTRODEC_DEC_ABORT |
					 HANTRODEC_DEC_IRQ_DISABLE;
			iowrite32(status, (void __iomem *)
					  (dev->hwregs[j][HW_VCD] +
					  HANTRODEC_IRQ_STAT_DEC_OFF));
		}

		if (IS_G1(dev->hw_id[j][HW_VCD]))
			/* reset PP */
			iowrite32(0, (void __iomem *)
					  (dev->hwregs[j][HW_VCD] +
					  HANTRO_IRQ_STAT_PP_OFF));

		for (i = 4; i < dev->iosize[j]; i += 4)
			iowrite32(0, (void __iomem *)
					  (dev->hwregs[j][HW_VCD] +
					  i));
	}
}

/*
 * Function name   : dump_regs
 * Description     : Dump registers

 * Return type     :
 */
#ifdef HANTRODEC_DEBUG
void dump_regs(hantrodec_t *dev)
{
	int i, c;

	PDEBUG("Reg Dump Start\n");
	for (c = 0; c < dev->cores; c++) {
		for (i = 0; i < dev->iosize[c]; i += 4 * 4) {
			PDEBUG("\toffset %04X: %08X  %08X  %08X  %08X\n", i,
			       ioread32(dev->hwregs[c][HW_VCD] + i),
	ioread32(dev->hwregs[c][HW_VCD] + i + 4),
	ioread32(dev->hwregs[c][HW_VCD] + i + 16),
	ioread32(dev->hwregs[c][HW_VCD] + i + 24));
		}
	}
	PDEBUG("Reg Dump End\n");
}
#endif

static int hantrodec_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	int ret = 0;
	unsigned long start = 0, end = 0;

	PDEBUG("%s %08lx-%08lx -> %08lx, %s\n", __func__,
		   (long)(vma->vm_pgoff << PAGE_SHIFT),
		   (long)(vma->vm_pgoff << PAGE_SHIFT) + (int)(size),
		   (long)vma->vm_start,
		   (filp->f_flags & O_SYNC) ? "uncached" : "cached");

	start = vma->vm_pgoff << PAGE_SHIFT;
	end = start + size;

	if (use_vcmd) {
		ret = in_vcmd_memory_region(subsys_mgr.vcmd_mgr, start, end);
		if (ret == 0)
			goto mem_mmap;
	}

#ifdef SUPPORT_RANDOM_LATENCY
	int i;

	for (i = 0; i < subsys_mgr.hantrodec_data.cores; i++) {
		unsigned long base_start = subsys_mgr.multicorebase[i] + 0x3800;
		unsigned long base_end   = ((subsys_mgr.multicorebase[i] + 0x3800 + 0xFF)
								  / PAGE_SIZE + 1) * PAGE_SIZE;
		if (start < base_start || base_end < end) {
			pr_info("Random Latency base mmap: it is a invalid addr in core %d!\n", i);
			if ((i + 1) == subsys_mgr.hantrodec_data.cores)
				ret = -1;
		} else {
			goto mem_mmap;
		}
	}
#endif

	if (ret != 0)
		goto err_adrr;

mem_mmap:
	// support only uncached mode
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	vma->vm_ops = &hantrodec_vm_ops;

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
		  vma->vm_page_prot);
	if (ret != 0) {
		pr_err("remap_pfn_range() failed.\n");
		goto err_out;
	}

	return 0;

err_adrr:
	pr_err("Invalid address %08lx-%08lx\n", start, end);
	return -EINVAL;

err_out:
	return ret;
}
