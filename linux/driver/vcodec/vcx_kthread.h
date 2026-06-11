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
**                          include vcx kthread header                          **
*********************************************************************************/

#ifndef _VCX_KTHREAD_H_
#define _VCX_KTHREAD_H_

#include "vcx_vcmd_priv.h"

#ifdef __cplusplus
extern "C" {
#endif


void _vcmd_kthread_create(vcmd_mgr_t *vcmd_mgr, const char *name);

void _vcmd_kthread_wakeup(vcmd_mgr_t *vcmd_mgr);

void _vcmd_kthread_stop(vcmd_mgr_t *vcmd_mgr);


#ifdef __cplusplus
}
#endif

#endif /*_VCX_KTHREAD_H_*/
