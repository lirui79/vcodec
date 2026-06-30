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
**                           include c queue header                             **
*********************************************************************************/

#ifndef _C_QUEUE_H_
#define _C_QUEUE_H_

#include "cmd_inc.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef void*  CQueueHandle_t;

CQueueHandle_t CQueueCreate(uint32_t qSize, uint32_t iSize);

void           CQueueDelete(CQueueHandle_t handle);

uint32_t       CQueueCapacity(CQueueHandle_t handle);

uint32_t       CQueueLength(CQueueHandle_t handle);

uint32_t       CQueueItemSize(CQueueHandle_t handle);

int32_t        CQueueEnqueue(CQueueHandle_t handle, void* item);

void*          CQueueDequeue(CQueueHandle_t handle);

void*          CQueuePeek(CQueueHandle_t handle);

int32_t        CQueueEnqueueFromISR(CQueueHandle_t handle, void* item);

void*          CQueueDequeueFromISR(CQueueHandle_t handle);

void*          CQueuePeekFromISR(CQueueHandle_t handle);


#ifdef __cplusplus
}
#endif

#endif /*_C_QUEUE_H_*/
