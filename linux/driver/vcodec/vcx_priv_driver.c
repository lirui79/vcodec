/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                           *.c vcd vcodec source code                         **
*********************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/of.h>
#include <linux/version.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/pm_runtime.h>
#ifndef PCIE_EN
/* for dma_alloc_coherent to allocate mmu &
 * vcmd linear buffers for non-pcie env
 */
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
#include <linux/dma-contiguous.h>
#else
#include <linux/dma-map-ops.h>
#endif
#include <linux/mod_devicetable.h>
#include <linux/dma-buf.h>
#endif
#include "vcx_priv.h"

#include "vcx_vcmd_priv.h"


static int vcx_priv_probe(struct platform_device *pdev, void *priv, int vcmd_supperted) {
	int ret = 0;

	pr_info("vcx: %s called\n", __func__);
/*
	if (vcmd_supported == 0)
		ret = hantroenc_normal_init((vcx_priv_t *)priv);
	else
		ret = hantroenc_vcmd_init((vcx_priv_t *)priv);
*/

    return ret;
}

static int vcx_priv_remove(struct platform_device *pdev, void *priv, int vcmd_supperted) {
/*
	if (vcmd_supported == 0)
		hantroenc_normal_cleanup((vcx_priv_t *)priv);
	else
		hantroenc_vcmd_cleanup((vcx_priv_t *)priv);
*/

	return 0;
}


static const vcx_operations_t vcx_priv_ops = {
	.probe = vcx_priv_probe,
	.remove = vcx_priv_remove,
	.suspend = NULL,
	.resume = NULL,
};



void vcx_init_ops(vcx_priv_t *priv) {
	if (priv == NULL)
	    return;
	priv->ops = &vcx_priv_ops;
}

void _dbg_log_instr(u32 offset, u32 instr, u32 *size, char *str)
{
	u32 opcode = instr & OPCODE_MASK;

	if (opcode == OPCODE_WREG) {
		int length = ((instr >> 16) & 0x3FF);

		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s %d 0x%x]\n",
			offset, instr, "WREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			   length, (instr & 0xFFFF));
		*size = ((length + 2) >> 1) << 1;
	} else if (opcode == OPCODE_END) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n", offset,
			instr, "END");
		*size = 2;
	} else if (opcode == OPCODE_NOP) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n", offset,
			instr, "NOP");
		*size = 2;
	} else if (opcode == OPCODE_RREG) {
		int length = ((instr >> 16) & 0x3FF);

		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s %d 0x%x]\n",
			offset, instr, "RREG", ((instr >> 26) & 0x1) ? "FIX" : "",
			   length, (instr & 0xFFFF));
		*size = 4;
	} else if (opcode == OPCODE_JMP) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s %s]\n",
			offset, instr, "JMP", ((instr >> 26) & 0x1) ? "RDY" : "",
			   ((instr >> 25) & 0x1) ? "IE" : "");
		*size = 4;
	} else if (opcode == OPCODE_STALL) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %s 0x%x]\n",
			offset, instr, "STALL", ((instr >> 26) & 0x1) ? "IM" : "",
			   (instr & 0xFFFF));
		*size = 2;
	} else if (opcode == OPCODE_CLRINT) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s %u 0x%x]\n",
			offset, instr, "CLRINT", (instr >> 25) & 0x3,
			   (instr & 0xFFFF));
		*size = 2;
	} else if (opcode == OPCODE_M2M) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n",
			offset, instr, "M2M");
		*size = 6;
	} else if (opcode == OPCODE_MSET) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n",
			offset, instr, "MSET");
		*size = 4;
	} else if (opcode == OPCODE_M2MP) {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n",
			offset, instr, "M2MP");
		*size = 6;
	} else {
		sprintf(str, "current cmdbuf data %u = 0x%08x => [%s]\n",
			offset, instr, "UNKNOWN CMD");
		*size = 1;
	}
}
