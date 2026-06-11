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
**                      include vcx private headers                             **
*********************************************************************************/

#ifndef _VCX_PRIVATE_H_
#define _VCX_PRIVATE_H_


#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>



typedef struct {
    int (*probe)(struct platform_device *pdev, void *priv, int vcmd_supperted);
    int (*remove)(struct platform_device *pdev, void *priv, int vcmd_supperted);
    int (*suspend)(struct device *dev, void *priv, int vcmd_supperted);
    int (*resume)(struct device *dev, void *priv, int vcmd_supperted);
} vcx_operations_t;


typedef struct {
    char                            *name;////// 设备节点   /dev/hantrovcx  /dev/hantroenc  /dev/hantrodec
    struct cdev                      cdev;////// 字符设备核心结构
    dev_t                            devno;///// 完整的设备号 (Major + Minor)
    unsigned int                     devid;///// 设备索引 (0, 1, 2)   0- /dev/hantrovcx   1- /dev/hantroenc  2- /dev/hantrodec
    void                            *priv;////// vcmdmgr pointer
    struct platform_device          *pdev;////// 指向 struct platform_device 结构的指针
    struct device                   *dev;/////// 指向 struct device 结构的指针
    const vcx_operations_t          *ops;
} vcx_priv_t;

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

extern u32 vcmd_supported;
extern unsigned long vcmd_isr_polling;
extern int vsi_kloglvl;
extern unsigned long ddr_offset;
extern u32 arbiter_weight;
extern u32 arbiter_urgent;
extern u32 arbiter_timewindow;
extern u32 arbiter_bw_overflow;
extern unsigned long sw_timeout_time;

#ifdef PCIE_EN
extern unsigned long gBaseDDRHw;/* PCI base register address (memalloc) */
#endif


#ifdef __cplusplus
extern "C" {
#endif

#if defined(CONFIG_ENC_PM) || defined(CONFIG_DEC_PM)

void vcx_vcodec_pm_runtime_get(vcx_priv_t *priv);

void vcx_vcodec_pm_runtime_put(vcx_priv_t *priv);

#endif

void vcx_init_ops(vcx_priv_t *priv);

void vce_init_ops(vcx_priv_t *priv);

void vcd_init_ops(vcx_priv_t *priv);


int  vcx_create_devnode(vcx_priv_t *priv, const struct file_operations *ops);

void _dbg_log_instr(u32 offset, u32 instr, u32 *size, char *str);

#ifdef __cplusplus
}
#endif

#endif //_VCX_PRIVATE_H_



