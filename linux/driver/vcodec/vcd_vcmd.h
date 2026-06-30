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
**                           include vcd vcmd header                           **
*********************************************************************************/

#ifndef __VCD_VCMD_H__
#define __VCD_VCMD_H__

#include <linux/fs.h>
#include "vcx_module_type.h"

#ifdef __cplusplus
extern "C" {
#endif


#ifdef VCARB_REQUEST
void vcmd_request_arbiter(void *_vcmd_mgr, u32 subsys_id);
void vcmd_release_arbiter(void *_vcmd_mgr, u32 subsys_id);
#endif//VCARB_REQUEST
//int vcmd_pm_suspend(void *_vcmd_mgr);
//int vcmd_pm_resume(void *_vcmd_mgr);
u32 *get_submodule_regs_va(void *_vcmd_mgr, u32 subsys_id, u32 sub_mod_id);
int in_vcmd_memory_region(void *_vcmd_mgr, unsigned long start, unsigned long end);
#ifdef SUPPORT_48PA_MMU
int _vcmd_memory_map(void *_vcmd_mgr, struct file *filp);
#endif



#ifdef __cplusplus
}
#endif

#endif /*__VCD_VCMD_H__ */
