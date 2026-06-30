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
#include <linux/types.h>
#include <linux/device.h>
#include <linux/pagemap.h>
#include <linux/sched.h>
#if (KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE)
#include <linux/dma-contiguous.h>
#else
#include <linux/dma-map-ops.h>
#endif
#include <linux/dma-buf.h>
#include <linux/stddef.h>
#include <linux/delay.h>
#include "vcx_axi2to1.h"
#include "vcx_kthread.h"
#include "vcx_vcmd_priv.h"

#ifdef AXI2TO1_SUPPORT
struct sub_ip_init_cfg axi2to1_init_cfg[] = {
	{AXI2TO1_REG6_SW_IRQ_EN, 0xffffffff}, // axi2to1 irq enable
	{AXI2TO1_REG7_SW_TIMEOUT_CYCLES, 0x40000000} // axife flush timeout cycles
};
#endif

/**
 * @brief hook function for system-driver to do further process
 */
static void hook_AXI2TO1_flush(void)
{
	//need to do NoC or Soc reset
	pr_err("AXI2to1 flush failed, need to do Noc or SoC reset!\n");
}

/**
 * @brief enable AXI2to1 flush
 */
static int AXI2TO1_flush_enable(volatile u8 *hwregs)
{
	u32 val;

	val = ioread32((void __iomem *)(hwregs + AXI2TO1_REG4_SW_FLUSH_EN));
	val |= 0x1;
	iowrite32(val, (void __iomem *)(hwregs + AXI2TO1_REG4_SW_FLUSH_EN));

	return 0;
}

/**
 * @brief AXI2TO1 do flush when subsystem exceptions happened
 */
int AXI2TO1_flush(volatile u8 *hwregs)
{
	u32 irq = 0;

	AXI2TO1_flush_enable(hwregs);

	while (irq == 0) {
		mdelay(10); // 10ms
		irq = ioread32((void __iomem *)(hwregs + AXI2TO1_REG5_SW_IRQ));
	}

	/* clear irq status */
	if (irq)
		iowrite32(irq, (void __iomem *)(hwregs + AXI2TO1_REG5_SW_IRQ));
	else
		pr_err("%s: AXI2TO1 flush timeout!\n", __func__);

	if (irq & AXI2TO1_IRQ_FLUSH_DONE) {
		pr_info("%s: AXI2to1 flush done!\n", __func__);
	} else {
		hook_AXI2TO1_flush();
		return -1;
	}

	return 0;
}

/**
 * @brief check if AXI2to1's AXI bus is clean
 * @return 0: bus clean, 1: not clean
 */
int AXI2TO1_bus_clean_check(volatile u8 *hwregs)
{
	if (ioread32((void __iomem *)(hwregs + AXI2TO1_REG8_SW_MST_OT_CNT)))
		return 1;
	else
		return 0;
}

/**
 * @brief AXI2TO1 init
 */
int AXI2TO1_init(volatile u8 *hwregs)
{
#ifndef HANTROVCMD_ENABLE_IP_SUPPORT
	u32 hw_id;

	if (!hwregs)
		return 0;

	hw_id = ioread32((void __iomem *)(hwregs + AXI2TO1_REG0_SW_HWID));
	if (((hw_id >> 16) & 0xFFFF) != AXI2TO1_HW_ID) {
		pr_err("%s: AXI2TO1 HW not flund!\n", __func__);
		return -1;
	}

	pr_info("AXI2TO1 HW ID: 0x%8x\n", hw_id);

	/* interrupt enable */
	iowrite32(0xffffffff, (void __iomem *)(hwregs + AXI2TO1_REG6_SW_IRQ_EN));
	/* set flush timeout threads */
	iowrite32(0x40000000, (void __iomem *)(hwregs + AXI2TO1_REG7_SW_TIMEOUT_CYCLES));
#endif
	/* when has arbiter, need use init cmds to config AXI2TO1 */

	return 0;
}
