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
**                        include vcd normal cfg header                         **
*********************************************************************************/

#ifndef __VCD_NORMAL_CFG_H__
#define __VCD_NORMAL_CFG_H__

#include "vcd_cfg.h"
#include "vcx_normal_cfg.h"
#include "vcx_module_type.h"

/* Configure information without CMD, fill according to System Memory Map*/

/* Subsystem configure
 * base_addr,            iosize,           resource_shared
 */
static SUBSYS_CONFIG subsys_array[] = {
	{ SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS },
#if SUBSYSTEM1
	{ SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS },
#endif
};

/* Core configure
 * slice_idx, core_type,       offset,                       reg_size,       irq
 */
static CORE_CONFIG core_array[] = {
	{0,       HW_VCD,         SUBSYS0_VCD_OFFSET,         MAX_REG_COUNT * 4, INT_PIN_SUBSYS_0_VCD},
	{0,       HW_MMU,         SUBSYS0_MMU_OFFSET,            238 * 4,        INT_PIN_SUBSYS_0_MMU},
	{0,       HW_VCMD,        SUBSYS0_VCMD_OFFSET,            30 * 4,        VCMD0_IRQ},
	{0,       HW_ARB,         SUBSYS0_ARB_OFFSET,             23 * 4,        INT_PIN_SUBSYS_0_NO},
	{0,       HW_DEC400,      SUBSYS0_DEC400_OFFSET,        1568 * 4,        INT_PIN_SUBSYS_0_DEC400},
	{0,       HW_AXIFE,       SUBSYS0_AXIFE_OFFSET,           64 * 4,        INT_PIN_SUBSYS_0_AXIFE},
#if SUBSYSTEM1
	{1,       HW_VCD,         SUBSYS1_VCD_OFFSET,         MAX_REG_COUNT * 4, INT_PIN_SUBSYS_1_VCD},
	{1,       HW_MMU,         SUBSYS1_MMU_OFFSET,            238 * 4,        INT_PIN_SUBSYS_1_MMU},
	{1,       HW_VCMD,        SUBSYS1_VCMD_OFFSET,            30 * 4,        VCMD1_IRQ},
	{1,       HW_DEC400,      SUBSYS1_DEC400_OFFSET,        1568 * 4,        INT_PIN_SUBSYS_1_DEC400},
	{1,       HW_AXIFE,       SUBSYS1_AXIFE_OFFSET,           64 * 4,        INT_PIN_SUBSYS_1_AXIFE},
#endif
};

#endif //__VCD_NORMAL_CFG_H__
