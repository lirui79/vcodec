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
**                      include mhu v3 private headers                          **
*********************************************************************************/

#ifndef _MHU_V3_PRIVATE_H_
#define _MHU_V3_PRIVATE_H_

#include <linux/platform_device.h>


#ifdef __cplusplus
extern "C" {
#endif
/*
 * Probe function: Initialize mailbox client and request channels
 */
int mhu_v3_client_probe(struct platform_device *pdev);

/*
 * Remove function: Cleanup resources
 */
int mhu_v3_client_remove(struct platform_device *pdev);


#ifdef __cplusplus
}
#endif

#endif //_MHU_V3_PRIVATE_H_

