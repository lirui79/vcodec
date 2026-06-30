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
**                      include vce private headers                             **
*********************************************************************************/

#ifndef _VCE_PRIVATE_H_
#define _VCE_PRIVATE_H_

#include "vcx_priv.h"


#ifdef __cplusplus
extern "C" {
#endif

int hantroenc_vcmd_init(vcx_priv_t *priv);

void hantroenc_vcmd_cleanup(vcx_priv_t *priv);

int hantroenc_normal_init(vcx_priv_t *priv);

void hantroenc_normal_cleanup(vcx_priv_t *priv);

int abort_vce(volatile u8 *reg_base);

#ifdef CONFIG_ENC_PM

int vcmd_pm_suspend(void *handler);

int vcmd_pm_resume(void *handler);

int enc_pm_suspend(void *handler);

int enc_pm_resume(void *handler);

#endif

#ifdef __cplusplus
}
#endif

#endif //_VCE_PRIVATE_H_



