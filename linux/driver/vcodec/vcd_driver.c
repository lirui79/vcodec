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
**                           *.c vcd decode source code                         **
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
#include "vcd_priv.h"

#include "vcx_vcmd_priv.h"



static int vcd_priv_probe(struct platform_device *pdev, void *priv, int vcmd_supperted) {
	int ret = 0;

	pr_info("hantrodec: %s called\n", __func__);
	ret = hantrodec_normal_init((vcx_priv_t *)priv, vcmd_supperted);
/*
	if (vcmd_supported == 0)
		ret = hantrodec_normal_init((vcx_priv_t *)priv);
	else
		ret = hantrodec_vcmd_init((vcx_priv_t *)priv);
*/
	if (ret < 0)
		return ret;
#ifdef CONFIG_DEC_PM
	pm_runtime_enable(&pdev->dev);
#endif
	return 0;
}
static int vcd_priv_remove(struct platform_device *pdev, void *priv, int vcmd_supperted) {
	hantrodec_normal_cleanup((vcx_priv_t *)priv);
/*
	if (vcmd_supported == 0)
		hantrodec_normal_cleanup((vcx_priv_t *)priv);
	else
		hantrodec_vcmd_cleanup((vcx_priv_t *)priv);
*/
#ifdef CONFIG_DEC_PM
	pm_runtime_disable(&pdev->dev);
#endif
	return 0;
}

#ifdef CONFIG_DEC_PM
static int vcd_priv_suspend(struct device *dev, void *priv, int vcmd_supperted) {
	vcx_priv_t *vcx_priv = (vcx_priv_t*) priv;
	int ret = 0;
	ret = hantrodec_pm_suspend(priv);
/*
	if (vcmd_supported == 1)
		ret = vcmd_pm_suspend(vcx_priv->priv);
	else
		ret = enc_pm_suspend(vcx_priv->priv);
*/
	pr_info("%s: device suspend done!\n", __func__);
	return ret;
}

static int vcd_priv_resume(struct device *dev, void *priv, int vcmd_supperted) {
	vcx_priv_t *vcx_priv = (vcx_priv_t*) priv;
	int ret = 0;
	ret = hantrodec_pm_resume(priv);
/*
	if (vcmd_supported == 1)
		ret = vcmd_pm_resume(vcx_priv->priv);
	else
		ret = enc_pm_resume(vcx_priv->priv);
*/
	pr_info("%s, device resume done!\n", __func__);
	return ret;
}

#endif

static const vcx_operations_t vcd_priv_ops = {
	.probe = vcd_priv_probe,
	.remove = vcd_priv_remove,
#ifdef CONFIG_DEC_PM
	.suspend = vcd_priv_suspend,
	.resume = vcd_priv_resume,
#else
	.suspend = NULL,
	.resume = NULL,
#endif
};



void vcd_init_ops(vcx_priv_t *priv) {
	if (priv == NULL)
	    return;
	priv->ops = &vcd_priv_ops;
}