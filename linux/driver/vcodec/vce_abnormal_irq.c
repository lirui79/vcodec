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

#include <linux/ioctl.h>
#include <linux/types.h>
#include <asm/io.h>
#include "vcx_kthread.h"
#include "vce_abnormal_irq.h"


extern void vce_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);

/**
 * @brief check if has abnormal run done
 * @return int 0: not run done; > 0: run done
 */
int _is_abnormal_run_done(struct cmdbuf_obj *obj)
{
	if ((obj->slice_run_done && obj->line_buffer_run_done) == 1)
		return 3;
	else if (obj->slice_run_done == 1)
		return 1;
	else if (obj->line_buffer_run_done == 1)
		return 2;

	return 0;
}

/**
 * @brief clear abnormal run done flag
 */
void _abnormal_run_done_clear(struct cmdbuf_obj *obj)
{
	if (obj->slice_run_done && obj->line_buffer_run_done) {
		obj->slice_run_done = 0;
		obj->line_buffer_run_done = 0;
	} else if (obj->slice_run_done == 1)
		obj->slice_run_done = 0;
	else if (obj->line_buffer_run_done == 1)
		obj->line_buffer_run_done = 0;
}

/**
 * process vce multi-slice abnormal irq
 */
static void process_vce_slice_irq(vcmd_mgr_t *vcmd_mgr, volatile void *hwregs,
								  struct cmdbuf_obj *obj, u32 irq)
{
	u32 slice_rdy_num;
	u32 *status_va;
	u32 irq_ori = irq;

	if (obj->slice_run_done == 0) {
		/* status_main_addr is 0 */
		status_va = obj->status_va + 0;
		// clear VCE slice_rdy irq (bit 8 of reg1)
		irq &= 0xffff903;
		iowrite32(irq, (void __iomem *)(hwregs + 0x4));

		// read out & backup slice irq related info
		slice_rdy_num = (u32)ioread32((void __iomem *)(hwregs + 0x1C));

		*(status_va + VCMD_SLICE_RDY_INTERRUPT) = irq & 0x100;
		*(status_va + VCMD_SLICE_RDY_NUM) = slice_rdy_num;
		obj->slice_run_done = 1;
		if (!(irq_ori & ASIC_STATUS_LINE_BUFFER_DONE))
			vce_proc_add_done_job(vcmd_mgr, obj);
	}
	// TODO, more info need to be read for Xiaomi project.
}

static void process_vce_line_buffer_irq(vcmd_mgr_t *vcmd_mgr,
				volatile void *hwregs, struct cmdbuf_obj *obj, u32 irq)
{
	u32 *status_va;
	u32 ctb_row;

	if (obj->line_buffer_run_done == 0) {
		/* status_main_addr is 0 */
		status_va = obj->status_va + 0;
		// clear VCE input line buffer irq (bit 7 of reg1)
		irq &= 0xffff883;
		iowrite32(irq, (void __iomem *)(hwregs + 0x4));
		*(status_va + VCMD_LINE_BUFFER_INTERRUPT) = irq & 0x80;
		/* low-latency control reg: 196*/
		ctb_row = (u32)ioread32((void __iomem *)(hwregs + 196 * 4));
		*(status_va + 196) = ctb_row;
		/* jpeg ctb_row_rd_ptr&ctb_row_wr_ptr : reg197[0-9]*/
		ctb_row = (u32)ioread32((void __iomem *)(hwregs + 197 * 4));
		*(status_va + 197) = ctb_row & 0x3ff;
		obj->line_buffer_run_done = 1;
		vce_proc_add_done_job(vcmd_mgr, obj);
	}
}

void process_vce_abn_irq(vcmd_mgr_t *vcmd_mgr, struct hantrovcmd_dev *dev,
						struct cmdbuf_obj *obj)
{
	struct vcmd_subsys_info *subsys = dev->subsys_info;

	volatile void *hwregs;
	unsigned long flags;
	u32 irq;

	hwregs = subsys->hwregs[SUB_MOD_MAIN];

	spin_lock_irqsave(&dev->abn_irq_lock, flags);
	// read VCE int_status
	irq = (u32)ioread32((void __iomem *)(hwregs + 0x04));
	if (irq & ASIC_STATUS_SLICE_READY) {
		/* slice rdy process*/
		process_vce_slice_irq(vcmd_mgr, hwregs, obj, irq);
	}
	if (irq & ASIC_STATUS_LINE_BUFFER_DONE) {
		/* input line buffer done  process*/
		process_vce_line_buffer_irq(vcmd_mgr, hwregs, obj, irq);
	}
	spin_unlock_irqrestore(&dev->abn_irq_lock, flags);

}


