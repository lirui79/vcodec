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
#include "vce_priv.h"
#include "vcx_vcmd_priv.h"



static int vce_priv_probe(struct platform_device *pdev, void *priv, int vcmd_supperted) {
	int ret = 0;

	pr_info("hantroenc: %s called\n", __func__);

	if (vcmd_supported == 0)
		ret = hantroenc_normal_init((vcx_priv_t *)priv);
	else
		ret = hantroenc_vcmd_init((vcx_priv_t *)priv);

	if (ret < 0)
		return ret;
#ifdef CONFIG_ENC_PM
	pm_runtime_enable(&pdev->dev);
#endif
	return 0;
}
static int vce_priv_remove(struct platform_device *pdev, void *priv, int vcmd_supperted) {
	if (vcmd_supported == 0)
		hantroenc_normal_cleanup((vcx_priv_t *)priv);
	else
		hantroenc_vcmd_cleanup((vcx_priv_t *)priv);
#ifdef CONFIG_ENC_PM
	pm_runtime_disable(&pdev->dev);
#endif
	return 0;
}

#ifdef CONFIG_ENC_PM
static int vce_priv_suspend(struct device *dev, void *priv, int vcmd_supperted) {
	vcx_priv_t *vcx_priv = (vcx_priv_t*) priv;
	int ret = 0;

	if (vcmd_supported == 1)
		ret = vcmd_pm_suspend(vcx_priv->priv);
	else
		ret = enc_pm_suspend(vcx_priv->priv);

	pr_info("%s: device suspend done!\n", __func__);
	return ret;
}

static int vce_priv_resume(struct device *dev, void *priv, int vcmd_supperted) {
	vcx_priv_t *vcx_priv = (vcx_priv_t*) priv;
	int ret = 0;

	if (vcmd_supported == 1)
		ret = vcmd_pm_resume(vcx_priv->priv);
	else
		ret = enc_pm_resume(vcx_priv->priv);

	pr_info("%s, device resume done!\n", __func__);
	return ret;
}

#endif

static const vcx_operations_t vce_priv_ops = {
	.probe = vce_priv_probe,
	.remove = vce_priv_remove,
#ifdef CONFIG_ENC_PM
	.suspend = vce_priv_suspend,
	.resume = vce_priv_resume,
#else
	.suspend = NULL,
	.resume = NULL,
#endif
};

void vce_init_ops(vcx_priv_t *priv) {
	if (priv == NULL)
	    return;
	priv->ops = &vce_priv_ops;
}