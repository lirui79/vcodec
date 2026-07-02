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
**                       include c buffer queue header                          **
*********************************************************************************/

#ifndef _BUFFER_QUEUE_H_
#define _BUFFER_QUEUE_H_

#include "inc.h"

#ifdef __cplusplus
extern "C" {
#endif



typedef void*  BQueueHandle_t;

BQueueHandle_t BQueueCreate(uint32_t qSize, uint32_t iSize);

void           BQueueDelete(BQueueHandle_t handle);

uint32_t       BQueueCapacity(BQueueHandle_t handle);

uint32_t       BQueueLength(BQueueHandle_t handle);

uint32_t       BQueueItemSize(BQueueHandle_t handle);

void*          BQueueAcquire(BQueueHandle_t handle);

int32_t        BQueueRelease(BQueueHandle_t handle, void* item);

void*          BQueueDequeue(BQueueHandle_t handle);

int32_t        BQueueQueue(BQueueHandle_t handle, void* item);

int32_t        BQueueCancel(BQueueHandle_t handle, void* item);

void*          BQueueAcquireFromISR(BQueueHandle_t handle);

int32_t        BQueueReleaseFromISR(BQueueHandle_t handle, void* item);

void*          BQueueDequeueFromISR(BQueueHandle_t handle);

int32_t        BQueueQueueFromISR(BQueueHandle_t handle, void* item);

int32_t        BQueueCancelFromISR(BQueueHandle_t handle, void* item);


#ifdef __cplusplus
}
#endif

#endif /*_BUFFER_QUEUE_H_*/
