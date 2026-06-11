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
**                      include vcd private headers                             **
*********************************************************************************/

#ifndef _VCD_PRIVATE_H_
#define _VCD_PRIVATE_H_

#include "vcx_priv.h"


#ifdef __cplusplus
extern "C" {
#endif


//int hantrodec_vcmd_init(vcx_priv_t *priv);

void  hantrodec_vcmd_cleanup(vcx_priv_t *priv);

int   hantrodec_normal_init(vcx_priv_t *priv, int vcmd_supported);

void  hantrodec_normal_cleanup(vcx_priv_t *priv);

int   abort_vcd(volatile u8 *reg_base);

void vcd_vcmd_watchdog_process(void *handler);

void vcd_vcmd_bus_err_process(void *handler);

#ifdef AXI2TO1_SUPPORT
int vcd_process_subsystem_exceptions(void *handler);
#endif

#ifdef CONFIG_DEC_PM

int   hantrodec_pm_suspend(void *handler);

int   hantrodec_pm_resume(void *handler);

int   vcmddec_pm_suspend(void *handler);

int   vcmddec_pm_resume(void *handler);
/*
int dec_pm_suspend(void *handler);

int dec_pm_resume(void *handler);
*/

#endif

#ifdef __cplusplus
}
#endif

#endif //_VCD_PRIVATE_H_



