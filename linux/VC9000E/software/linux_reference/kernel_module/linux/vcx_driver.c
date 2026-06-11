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

#include "vcx_driver.h"
#include "vcx_vcmd_priv.h"

/****************************************************************
 * global variables declarations
 ***************************************************************/
static u32 vcmd_supported;
unsigned long vcmd_isr_polling = 1;
int vsi_kloglvl = LOGLVL_ERROR;
char *enc_dev_n = "vsi_vcx";
unsigned long ddr_offset;
u32 arbiter_weight = 0x1d;
u32 arbiter_urgent;
u32 arbiter_timewindow = 0x1d;
u32 arbiter_bw_overflow;
unsigned long sw_timeout_time = SW_TIMEOUT_TIME_FOR_ARBITER;

/***************************************************************
 * hantroenc driver functions declarations
 ***************************************************************/
int hantroenc_normal_init(void **_hantroenc_data);
int hantroenc_vcmd_init(void **_vcmd_mgr);
void hantroenc_normal_cleanup(void);
void hantroenc_vcmd_cleanup(void);
#ifdef CONFIG_ENC_PM
int vcmd_pm_suspend(void *handler);
int vcmd_pm_resume(void *handler);
int enc_pm_suspend(void *handler);
int enc_pm_resume(void *handler);
#endif

/******************************************************************
 * platform device and driver related declarations
 ******************************************************************/
#define DRIVER_NAME		"hantroenc"
struct platform_device *platformdev;
struct hantroenc_dev {
	struct device *dev;
	void *priv_data;
};

static const struct platform_device_info hantro_platform_info = {
	.name = DRIVER_NAME,
	.id = -1,
#ifndef PCIE_EN
	.dma_mask = DMA_BIT_MASK(32),
#endif
};

/* struct used for matching a device */
static const struct of_device_id of_hantroenc_match[] = {
	{
		.compatible = "vsi, vce", // used for matching device and driver
	},
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, of_hantroenc_match);

static int hantroenc_probe(struct platform_device *pdev)
{
	struct hantroenc_dev *enc_dev;
	void *handler = NULL;
	int ret = 0;

	pr_info("hantroenc: %s called\n", __func__);

	enc_dev = devm_kzalloc(&pdev->dev, sizeof(*enc_dev), GFP_KERNEL);
	if (!enc_dev)
		return -ENOMEM;

	enc_dev->dev = &pdev->dev;

	if (vcmd_supported == 0)
		ret = hantroenc_normal_init(&handler);
	else
		ret = hantroenc_vcmd_init(&handler);

	if (ret < 0)
		return ret;
	enc_dev->priv_data = handler;
	platform_set_drvdata(pdev, enc_dev);

#ifdef CONFIG_ENC_PM
	pm_runtime_enable(&pdev->dev);
#endif

	return 0;
}

#ifdef CONFIG_ENC_PM
static int hantroenc_pm_suspend(struct device *dev)
{
	struct hantroenc_dev *enc_dev;
	int ret = 0;

	enc_dev = dev_get_drvdata(dev);

	if (vcmd_supported == 1)
		ret = vcmd_pm_suspend(enc_dev->priv_data);
	else
		ret = enc_pm_suspend(enc_dev->priv_data);

	pr_info("%s: device suspend done!\n", __func__);
	return ret;
}

static int hantroenc_pm_resume(struct device *dev)
{
	struct hantroenc_dev *enc_dev;
	int ret = 0;

	enc_dev = dev_get_drvdata(dev);

	if (vcmd_supported == 1)
		ret = vcmd_pm_resume(enc_dev->priv_data);
	else
		ret = enc_pm_resume(enc_dev->priv_data);

	pr_info("%s, device resume done!\n", __func__);
	return ret;
}

static int hantroenc_pm_runtime_suspend(struct device *dev)
{
	/* Add Clk control */
	return 0;
}

static int hantroenc_pm_runtime_resume(struct device *dev)
{
	/* Add Clk control */
	return 0;
}

void hantroenc_pm_runtime_get(struct device *dev)
{
	pm_runtime_get_sync(dev);
}

void hantroenc_pm_runtime_put(struct device *dev)
{
	pm_runtime_put_sync(dev);
}

static const struct dev_pm_ops hantroenc_pm_ops = {
	//cppcheck-suppress unknownMacro
	SET_RUNTIME_PM_OPS(hantroenc_pm_runtime_suspend, hantroenc_pm_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(hantroenc_pm_suspend, hantroenc_pm_resume)
};
#endif


#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
static void hantroenc_remove(struct platform_device *dev)
{
	/*TODO*/
	//pr_info("%s: removed driver!\n", __func__);
	return;
}    // 使用新内核的 API
#else
static int hantroenc_remove(struct platform_device *dev)
{
	/*TODO*/
	//pr_info("%s: removed driver!\n", __func__);
	return 0;
}
#endif

static struct platform_driver hantroenc_driver = {
	.probe = hantroenc_probe,
	.remove = hantroenc_remove,
	.driver = {
		.name = DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(of_hantroenc_match),
#ifdef CONFIG_ENC_PM
		.pm = &hantroenc_pm_ops,
#endif
	},
};


static int __init hantroenc_init(void)
{
	int ret;

	platformdev =
			platform_device_register_full(&hantro_platform_info);
	if (!platformdev) {
		pr_err("hantroenc create platform device fail\n");
		return -ENODEV;
	}
	pr_info("Create platform device success\n");


	ret = platform_driver_register(&hantroenc_driver);
	if (ret) {
		pr_err("hantroenc register platform driver failed!\n");
		if (platformdev)
			platform_device_unregister(platformdev);
		return ret;
	}

#ifndef PCIE_EN
#if (KERNEL_VERSION(4, 18, 0) <= LINUX_VERSION_CODE)
	of_dma_configure(&platformdev->dev,
			 platformdev->dev.of_node,
	 true);
#else
	of_dma_configure(&platformdev->dev, platformdev->dev.of_node);
#endif

	if (dma_set_mask_and_coherent(&platformdev->dev,
				      DMA_BIT_MASK(48)))
		pr_err("48bit hantrodma dev: No suitable DMA available\n");

	if (dma_set_coherent_mask(&platformdev->dev, DMA_BIT_MASK(48)))
		pr_err("48bit hantrodma dev: No suitable DMA available\n");
#endif // PCIE_EN

	return 0;
}

static void __exit hantroenc_cleanup(void)
{
	if (vcmd_supported == 0)
		hantroenc_normal_cleanup();
	else
		hantroenc_vcmd_cleanup();

#ifdef CONFIG_ENC_PM
	pm_runtime_disable(&platformdev->dev);
#endif

	platform_driver_unregister(&hantroenc_driver);
	if (platformdev)
		platform_device_unregister(platformdev);
}

module_init(hantroenc_init);
module_exit(hantroenc_cleanup);
/**
 * module param:
 *  vcmd_supported      - 0: vcmd driver;
 *                        1: normal driver
 *  vcmd_isr_polling    - the mode to wait for device being aborted
 *                        0: use IRQ mode
 *                        1: use polling ISR mode
 *  vsi_kloglvl         - kernel driver log level, default is LOGLVL_ERROR
 *  enc_dev_n           - specify the device name
 *  ddr_offset          - the memory offset in ddr space
 *  arbiter_urgent      - 0: normal priority
 *                        1: urgent priority
 *  arbiter_weight      - normal priority, it indicates required bandwidth
 *                        urgent priority, it indicates urgent level
 *  arbiter_timewindow  - time window, 2^n cycles
 *  arbiter_bw_overflow - 0: can't overflow
 *                        1: can overflow
 *  sw_timeout_time     - VF timeout time after PF reset vcmd
 */
module_param(vcmd_supported, uint, 0);
module_param(vcmd_isr_polling, ulong, 0);
module_param(vsi_kloglvl, int, 0644);
module_param(enc_dev_n, charp, 0644);
module_param(ddr_offset, ulong, 0);
module_param(arbiter_urgent, uint, 0);
module_param(arbiter_weight, uint, 0);
module_param(arbiter_timewindow, uint, 0);
module_param(arbiter_bw_overflow, uint, 0);
module_param(sw_timeout_time, ulong, 0);

/* module description */
/*MODULE_LICENSE("Proprietary");*/
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Verisilicon");
MODULE_DESCRIPTION("VCX driver");
