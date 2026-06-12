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
**                          *.c vcm manager source code                         **
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
#include "vcm_priv.h"

#include "vcx_vcmd_priv.h"


static int hantrovcmg_open(struct inode *inode, struct file *filp);
static int hantrovcmg_release(struct inode *inode, struct file *filp);
static int hantrovcmg_mmap(struct file *filp, struct vm_area_struct *vma);
/**
 * @brief ioctl function of vcmd driver.
 */
static long hantrovcmg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);


/**
 * @brief VFS methods and VM operations
 */
static const struct file_operations hantrovcmg_fops = {
	.owner = THIS_MODULE,
	.open = hantrovcmg_open,
	.mmap = hantrovcmg_mmap,
	.release = hantrovcmg_release,
	.unlocked_ioctl = hantrovcmg_ioctl,
	.fasync = NULL,
};

static int vcm_init(vcx_priv_t *priv) {
	int result = 0;
	pr_info("vcx: %s called\n", __func__);
	
	result = vcx_create_devnode(priv, &hantrovcmg_fops);
	return result;
}

static int vcm_exit(vcx_priv_t *priv) {
	pr_info("vcx: %s called\n", __func__);
	return 0;
}

static int vcm_probe(struct platform_device *pdev, void *priv, int vcmd_supperted) {
	int ret = 0;

	pr_info("vcx: %s called\n", __func__);
	ret = vcm_init((vcx_priv_t *)priv);

    return ret;
}

static int vcm_remove(struct platform_device *pdev, void *priv, int vcmd_supperted) {

	return vcm_exit((vcx_priv_t *)priv);
}


static const vcx_operations_t vcm_priv_ops = {
	.probe = vcm_probe,
	.remove = vcm_remove,
	.suspend = NULL,
	.resume = NULL,
};



void vcx_init_ops(vcx_priv_t *priv) {
	if (priv == NULL)
	    return;
	priv->ops = &vcm_priv_ops;
}


static int hantrovcmg_open(struct inode *inode, struct file *filp) {
	return 0;
}

static int hantrovcmg_release(struct inode *inode, struct file *filp) {
	return 0;
}

static int hantrovcmg_mmap(struct file *filp, struct vm_area_struct *vma) {
	return 0;
}

/**
 * @brief ioctl function of vcmd driver.
 */
static long hantrovcmg_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    return 0;
}
