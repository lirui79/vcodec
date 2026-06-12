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
**                      *.c vcodec driver source code                           **
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
#include "vcx_defs.h"
#include "vcx_priv.h"



/****************************************************************
 * global variables declarations
 ***************************************************************/
/**
 * module param:
 *  vcmd_supported      - 0: normal driver;
 *                        1: vcmd driver
 *                        default 1
 *  vcmd_isr_polling    - the mode to wait for device being aborted, default 0
 *                        0: use IRQ mode
 *                        1: use polling ISR mode
 *  vsi_kloglvl         - kernel driver log level, default is LOGLVL_ERROR（5）
 *  ddr_offset          - the memory offset in ddr space, default 0
 *  arbiter_urgent      - 0: normal priority
 *                        1: urgent priority
 *                        default 0
 *  arbiter_weight      - normal priority, it indicates required bandwidth
 *                        urgent priority, it indicates urgent level
 *                        default 0x1d
 *  arbiter_timewindow  - time window, 2^n cycles，default 0x1d
 *  arbiter_bw_overflow - 0: can't overflow
 *                        1: can overflow
 *                        default 0
 *  sw_timeout_time     - VF timeout time after PF reset vcmd, default SW_TIMEOUT_TIME_FOR_ARBITER（1000000）
 */

u32 vcmd_supported             = 1;
unsigned long vcmd_isr_polling = 0;
int vsi_kloglvl                = 5;//LOGLVL_ERROR;
unsigned long ddr_offset       = 0x00;
u32 arbiter_weight             = 0x1d;
u32 arbiter_urgent             = 0x00;
u32 arbiter_timewindow         = 0x1d;
u32 arbiter_bw_overflow        = 0x00;
unsigned long sw_timeout_time  = SW_TIMEOUT_TIME_FOR_ARBITER;

#ifdef PCIE_EN
unsigned long gBaseDDRHw       = 0x00;/* PCI base register address (memalloc) */
#endif

module_param(vcmd_supported, uint, 0);
module_param(vcmd_isr_polling, ulong, 0);
module_param(vsi_kloglvl, int, 0644);
module_param(ddr_offset, ulong, 0);
module_param(arbiter_urgent, uint, 0);
module_param(arbiter_weight, uint, 0);
module_param(arbiter_timewindow, uint, 0);
module_param(arbiter_bw_overflow, uint, 0);
module_param(sw_timeout_time, ulong, 0);


#define             VCX_DRIVER_NAME  "hantrovcodec"
static const   char *CLASS_NAME   = "hantroclass";
static const   int  DEVICE_COUNT  =  3;  // 注册3个设备

static struct   class *vcx_class = NULL;
static dev_t    base_dev_no = 0;   // 起始设备号
static int      major_num = 0;



// ------------------------------------------------------------------
// 1. Platform Device 结构体定义
// ------------------------------------------------

static vcx_priv_t vcx_priv[] = {
    {
       .name  =  NULL,
       .devno =  0x00,
       .devid =  DEVID_VCX,
       .priv  =  NULL,
       .pdev  =  NULL,
       .dev   =  NULL,
       .ops   =  NULL,
    },
    {
       .name  =  NULL,
       .devno =  0x00,
       .devid =  DEVID_VCE,
       .priv  =  NULL,
       .pdev  =  NULL,
       .dev   =  NULL,
       .ops   =  NULL,
    },
    {
       .name  =  NULL,
       .devno =  0x00,
       .devid =  DEVID_VCD,
       .priv  =  NULL,
       .pdev  =  NULL,
       .dev   =  NULL,
       .ops   =  NULL,
    },
};


// ------------------------------------------------------------------
// 2. Platform Driver 回调 (Probe/Remove)
// ------------------------------------------------------------------

int  vcx_create_devnode(vcx_priv_t *priv, const struct file_operations *ops) {
    int ret = 0, id = 0;
    id = priv->pdev->id;
    // 2. 计算设备号: 主设备号相同，次设备号 = id
    priv->devno = MKDEV(major_num, id);

    // 3. 初始化并添加 cdev
    cdev_init(&priv->cdev, ops);
    priv->cdev.owner = THIS_MODULE;

    ret = cdev_add(&priv->cdev, priv->devno, 1);
    if (ret) {
        dev_err(&priv->pdev->dev, "Failed to add cdev for ID %d\n", id);
        return ret;
    }

    // 4. 创建设备节点 /dev/ 下
    // 这会在 /sys/class/hantroclass/ 下创建条目，并触发 udev 创建 /dev 节点
    priv->dev = device_create(vcx_class, &priv->pdev->dev, priv->devno, NULL, "%s", priv->name);
    if (IS_ERR(priv->dev)) {
        cdev_del(&priv->cdev);
        dev_err(&priv->pdev->dev, "Failed to create device node for name %s %d\n", priv->name, priv->devid);
        return PTR_ERR(priv->dev);
    }
    return 0;
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


static int vcx_vcodec_probe(struct platform_device *pdev)
{
    vcx_priv_t *priv = platform_get_drvdata(pdev);
    int ret = 0, id = 0;

    // 获取设备 ID。
    // 如果是通过设备树匹配，通常用 of_alias_get_id。
    // 这里我们是手动注册的 pdev，直接使用 pdev->id。
    id = pdev->id;
    if (id < 0 || id >= DEVICE_COUNT) {
        dev_err(&pdev->dev, "Invalid device ID: %d\n", id);
        return -EINVAL;
    }

    dev_info(&pdev->dev, "Probing device index: %d\n", id);
    if ((priv->ops != NULL) &&  priv->ops->probe != NULL) {
        ret = priv->ops->probe(pdev, priv, vcmd_supported);
    }
    return ret;
}

// 注意：Linux 6.x+ 内核 remove 返回 void
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 0, 0)
static void vcx_vcodec_remove(struct platform_device *pdev)
{
    vcx_priv_t *priv = platform_get_drvdata(pdev);

    if (priv) {
        if ((priv->ops != NULL) &&  priv->ops->remove != NULL) {
                int ret = priv->ops->remove(pdev, priv, vcmd_supported);
            }

        dev_info(&pdev->dev, "Removing device ID: %d\n", priv->devid);

        // 销毁设备节点
        device_destroy(vcx_class, priv->devno);

        // 删除 cdevs
        cdev_del(&priv->cdev);

        // 注意：inst 内存由 devm_kzalloc 分配，会在 driver detach 后自动释放，
        // 或者在这里手动 kfree 也可以，但 devm 更安全。
    }
}
#else
static int vcx_vcodec_remove(struct platform_device *dev)
{
    vcx_priv_t *priv = platform_get_drvdata(pdev);
    int ret = 0;

    if (priv) {
        if ((priv->ops != NULL) &&  priv->ops->remove != NULL) {
               ret = priv->ops->remove(pdev, priv, vcmd_supported);
            }

        dev_info(&pdev->dev, "Removing device ID: %d\n", priv->devid);

        // 销毁设备节点
        device_destroy(vcx_class, priv->devno);

        // 删除 cdev
        cdev_del(&priv->cdev);

        // 注意：inst 内存由 devm_kzalloc 分配，会在 driver detach 后自动释放，
        // 或者在这里手动 kfree 也可以，但 devm 更安全。
    }
    return ret;
}
#endif

#if defined(CONFIG_ENC_PM) || defined(CONFIG_DEC_PM)
static int vcx_vcodec_pm_suspend(struct device *dev)
{
    vcx_priv_t *vcx_priv = (vcx_priv_t *)dev_get_drvdata(dev);
    if ((vcx_priv->ops != NULL) &&  vcx_priv->ops->suspend != NULL) {
        return vcx_priv->ops->suspend(dev, vcx_priv, vcmd_supported);
    }

	pr_info("%s: device suspend done!\n", __func__);
    return 0;
}

static int vcx_vcodec_pm_resume(struct device *dev)
{
    vcx_priv_t *vcx_priv = (vcx_priv_t *)dev_get_drvdata(dev);
    if ((vcx_priv->ops != NULL) &&  vcx_priv->ops->resume != NULL) {
        return vcx_priv->ops->resume(dev, vcx_priv, vcmd_supported);
    }
	pr_info("%s, device resume done!\n", __func__);
    return 0;
}

static int vcx_vcodec_pm_runtime_suspend(struct device *dev)
{
	/* Add Clk control */
	return 0;
}

static int vcx_vcodec_pm_runtime_resume(struct device *dev)
{
	/* Add Clk control */
	return 0;
}

void vcx_vcodec_pm_runtime_get(vcx_priv_t *priv)
{
	pm_runtime_get_sync(&priv->pdev->dev);
}

void vcx_vcodec_pm_runtime_put(vcx_priv_t *priv)
{
	pm_runtime_put_sync(&priv->pdev->dev);
}

static const struct dev_pm_ops vcx_vcodec_pm_ops = {
	//cppcheck-suppress unknownMacro
	SET_RUNTIME_PM_OPS(vcx_vcodec_pm_runtime_suspend, vcx_vcodec_pm_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(vcx_vcodec_pm_suspend, vcx_vcodec_pm_resume)
};
#endif

// ------------------------------------------------------------------
// 3. 驱动与设备定义
// ------------------------------------------------------------------

static const struct of_device_id vcx_vcodec_of_match[] = {
    { .compatible = "vsi, vce" },
    { .compatible = "vsi, vcd" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, vcx_vcodec_of_match);

// ------------------------------------------------------------------
// 4. Platform Driver 结构体定义
// ------------------------------------------------------------------
static struct platform_driver vcx_vcodec_driver = {
    .probe  = vcx_vcodec_probe,
    .remove = vcx_vcodec_remove,
    .driver = {
        .name = VCX_DRIVER_NAME,
        .of_match_table = vcx_vcodec_of_match,
#if defined(CONFIG_ENC_PM) || defined(CONFIG_DEC_PM)
		.pm = &vcx_vcodec_pm_ops,
#endif
    },
};

// ------------------------------------------------------------------
// 5. 模块初始化与退出 (手动注册 3 个平台设备)
// ------------------------------------------------------------------

static int __init vcx_vcodec_init(void)
{
    int ret = 0, i = 0;
    struct platform_device *pdev = NULL;
    vcx_priv_t *priv = NULL;
    static const char *devName[] = {
        "hantrovcx",
        "hantroenc",
        "hantrodec",
    };

    // 1. 创建类
    vcx_class = class_create(CLASS_NAME);
    if (IS_ERR(vcx_class)) {
        pr_err("Failed to create class\n");
        return PTR_ERR(vcx_class);
    }

    // 2. 动态分配一组设备号 (主设备号自动分配，次设备号预留 0~2)
    ret = alloc_chrdev_region(&base_dev_no, 0, DEVICE_COUNT, VCX_DRIVER_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate chrdev region\n");
        goto err_class;
    }
    major_num = MAJOR(base_dev_no);
    pr_info("Allocated Major Number: %d\n", major_num);

    // 3. 注册平台驱动
    ret = platform_driver_register(&vcx_vcodec_driver);
    if (ret) {
        pr_err("Failed to register platform driver\n");
        goto err_chrdev;
    }

    // 4. 手动注册 3 个平台设备
    for (i = 0; i < DEVICE_COUNT; i++) {
        // 分配平台设备结构
        pdev = platform_device_alloc(VCX_DRIVER_NAME, i); // name, id
        if (!pdev) {
            ret = -ENOMEM;
            goto err_unregister_devices;
        }

        // 设置 compatible 属性以匹配驱动 (模拟设备树行为)
        // 注意：手动注册时，通常不需要显式设置 of_node，除非你构建了完整的 of_node 结构。
        // 这里依靠 driver.name 和 pdev->name 匹配，或者依赖 id 匹配。
        // 更严谨的做法是构建一个简单的 of_node，但为了演示简洁，我们依赖 name/id 匹配机制。
        // 如果驱动中有 .of_match_table，内核会尝试匹配。如果没有 of_node，它会 fallback 到 name 匹配。

        priv = &vcx_priv[i];
        switch (priv->devid) {
            case DEVID_VCX:
                vcx_init_ops(priv);
                break;
            case DEVID_VCE:
                vce_init_ops(priv);
                break;
            case DEVID_VCD:
                vcd_init_ops(priv);
                break;
            default:
                vcx_init_ops(priv);
                break;
              break;
        }
        priv->name = devName[priv->devid];
        platform_set_drvdata(pdev, priv);
        ret = platform_device_add(pdev);
        if (ret) {
            platform_device_put(pdev);
            goto err_unregister_devices;
        }
        pr_info("Registered platform device: %s.%d\n", VCX_DRIVER_NAME, i);
        priv->pdev = pdev;
    }

    return 0;

err_unregister_devices:
    // 清理已注册的设备
    while (--i >= 0) {
        platform_device_unregister(vcx_priv[i].pdev);
    }
    platform_driver_unregister(&vcx_vcodec_driver);
err_chrdev:
    unregister_chrdev_region(base_dev_no, DEVICE_COUNT);
err_class:
    class_destroy(vcx_class);
    return ret;
}

static void __exit vcx_vcodec_exit(void)
{
    int i;

    // 1. 注销所有平台设备 (这会触发驱动的 my_remove)
    for (i = 0; i < DEVICE_COUNT; i++) {
        if (vcx_priv[i].pdev) {
            platform_device_unregister(vcx_priv[i].pdev);
        }
    }

    // 2. 注销驱动
    platform_driver_unregister(&vcx_vcodec_driver);

    // 3. 释放设备号
    unregister_chrdev_region(base_dev_no, DEVICE_COUNT);

    // 4. 销毁类
    class_destroy(vcx_class);

    pr_info("module exited\n");
}

module_init(vcx_vcodec_init);
module_exit(vcx_vcodec_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Stone Li");
MODULE_DESCRIPTION("VCX driver");