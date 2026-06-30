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
**                        include vcx vcmd headers                              **
*********************************************************************************/

#ifndef _VCX_VCMD_H_
#define _VCX_VCMD_H_

//#include "vcx_vcmd_priv.h"

#ifdef __cplusplus
extern "C" {
#endif

void vce_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);

void vcd_proc_add_done_job(vcmd_mgr_t *vcmd_mgr, struct cmdbuf_obj *obj);

#ifdef __cplusplus
}
#endif

#endif //_VCX_VCMD_H_

