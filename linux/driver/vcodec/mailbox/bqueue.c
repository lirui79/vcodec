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
**                         *.c buffer queue source code                         **
*********************************************************************************/

#include "cqueue.h"
#include "bqueue.h"



typedef struct {
    uint8_t*        data;
    CQueueHandle_t  queue;
    CQueueHandle_t  free;
    uint32_t        qSize;
    uint32_t        iSize;
} BQueue_t;


BQueueHandle_t BQueueCreate(uint32_t qSize, uint32_t iSize) {
    BQueue_t* queue = NULL;
    uint8_t*  item  = NULL;
    uint32_t  i     = 0;
    if (qSize < 1 || iSize < 1) {
        return NULL;
    }

    queue = (BQueue_t*)vmalloc(sizeof(BQueue_t));
    if (queue == NULL) {
        return NULL;
    }

    queue->qSize = qSize;
    queue->iSize = iSize;
    queue->data  = vmalloc(qSize * iSize);
    if (queue->data == NULL) {
        vfree(queue);
        return NULL;
    }

    queue->free  = CQueueCreate(qSize, iSize * sizeof(uint8_t*));
    if (queue->free == NULL) {
        vfree(queue->data);
        vfree(queue);
        return NULL;
    }
    queue->queue  = CQueueCreate(qSize, iSize * sizeof(uint8_t*));
    if (queue->queue == NULL) {
        CQueueDelete(queue->free);
        vfree(queue->data);
        vfree(queue);
        return NULL;
    }

    for (i = 0; i < qSize; ++i) {
        item = queue->data + (i * iSize);
        CQueueEnqueue(queue->free, &item);
    }

    return queue;
}

void BQueueDelete(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return;
    }
    CQueueDelete(queue->queue);
    CQueueDelete(queue->free);
    if (queue->data != NULL) {
        vfree(queue->data);
    }
    vfree(queue);
}

uint32_t  BQueueCapacity(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return queue->qSize;
}

uint32_t  BQueueLength(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return CQueueLength(queue->queue);
}


uint32_t  BQueueItemSize(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return queue->iSize;
}


void*          BQueueAcquire(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return NULL;
    }
    return CQueueDequeue(queue->queue);
}

int32_t        BQueueRelease(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueue(queue->free, item);
}

void*          BQueueDequeue(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return NULL;
    }
    return CQueueDequeue(queue->free);
}

int32_t        BQueueQueue(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueue(queue->queue, item);
}

int32_t        BQueueCancel(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueue(queue->free, item);
}

void*          BQueueAcquireFromISR(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return NULL;
    }
    return CQueueDequeueFromISR(queue->queue);
}

int32_t        BQueueReleaseFromISR(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueueFromISR(queue->free, item);
}

void*          BQueueDequeueFromISR(BQueueHandle_t handle) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return NULL;
    }
    return CQueueDequeueFromISR(queue->free);
}

int32_t        BQueueQueueFromISR(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueueFromISR(queue->queue, item);
}

int32_t        BQueueCancelFromISR(BQueueHandle_t handle, void* item) {
    BQueue_t* queue = (BQueue_t*)handle;
    if (queue == NULL) {
        return -1;
    }
    return CQueueEnqueueFromISR(queue->free, item);
}
