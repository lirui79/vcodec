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
**                         include vcd vcmd cfg header                          **
*********************************************************************************/

#ifndef __VCD_VCMD_CFG_H__
#define __VCD_VCMD_CFG_H__

#include "vcd_cfg.h"
#include "vcx_vcmd_cfg.h"
#include "vcx_module_type.h"

// vcmd_base_addr, vcmd_irq, sub_module_type/*input vce=0,IM=1,vcd=2，jpege=3, jpegd=4*/, priority
//        sub_mod_id, io_off, io_size, rreg_id, rreg_num
static struct vcmd_cfg vcmd_core_array[] = {
/* subsys 0 base_addr,       vcmd_irq,         sub_module_type,    priority*/
	{SUBSYS_0_IO_ADDR,       VCMD0_IRQ,       VCMD_TYPE_DECODER,     0,
//        sub_mod_id,               io_off,                    io_size,        rreg_id,     rreg_num
		{ {SUB_MOD_VCMD,            SUBSYS0_VCMD_OFFSET,        30*4,           0xFFFF,         0},
		{SUB_MOD_MAIN,              SUBSYS0_VCD_OFFSET,     MAX_REG_COUNT*4,       0,       MAX_REG_COUNT},
		{SUB_MOD_MMU,               SUBSYS0_MMU_OFFSET,        238*4,           0xFFFF,         0},
		{SUB_MOD_MMU_WR,            SUBSYS0_MMU_WR_OFFSET,        0,            0xFFFF,         0},
		{SUB_MOD_DEC400,            SUBSYS0_DEC400_OFFSET,   1568 * 4,             0,          0x2b},
		{SUB_MOD_AXIFE,             SUBSYS0_AXIFE_OFFSET,      64 * 4,             0,           1},
		{SUB_MOD_UFBC,              SUBSYS0_UFBC_OFFSET,       32 * 4,             0,          10},
		{SUB_MOD_AXI2TO1,           SUBSYS0_AXI2TO1_OFFSET,    11 * 4,          0xFFFF,         0} },
	},
#if SUBSYSTEM1
/* subsys 1 base_addr,       vcmd_irq,         sub_module_type,    priority*/
	{SUBSYS_1_IO_ADDR,       VCMD1_IRQ,        VCMD_TYPE_DECODER,     0,
//        sub_mod_id,               io_off,                    io_size,        rreg_id,     rreg_num
		{ {SUB_MOD_VCMD,            SUBSYS1_VCMD_OFFSET,       30 * 4,          0xFFFF,         0},
		{SUB_MOD_MAIN,              SUBSYS1_VCD_OFFSET,    MAX_REG_COUNT * 4,      0,       MAX_REG_COUNT},
		{SUB_MOD_MMU,               SUBSYS1_MMU_OFFSET,       238 * 4,          0xFFFF,         0},
		{SUB_MOD_MMU_WR,            SUBSYS1_MMU_WR_OFFSET,        0,            0xFFFF,         0},
		{SUB_MOD_DEC400,            SUBSYS1_DEC400_OFFSET,   1568 * 4,             0,          0x2b},
		{SUB_MOD_AXIFE,             SUBSYS1_AXIFE_OFFSET,      64 * 4,             0,           1},
		{SUB_MOD_UFBC,              SUBSYS1_UFBC_OFFSET,       32 * 4,             0,          10},
		{SUB_MOD_AXI2TO1,           SUBSYS0_AXI2TO1_OFFSET,    11 * 4,          0xFFFF,         0} },
	},
#endif
};

#endif /*__VCD_VCMD_CFG_H__ */
