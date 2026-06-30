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
**                            *.c queue source code                             **
*********************************************************************************/


#include "cqueue.h"


typedef struct {
    uint8_t*    data;
    uint32_t    front;
    uint32_t    rear;
    uint32_t    qSize;
    uint32_t    count;
    uint32_t    iSize;
	spinlock_t  spinlock;
} CQueue_t;


CQueueHandle_t CQueueCreate(uint32_t qSize, uint32_t iSize) {
    CQueue_t* queue = NULL;
    if (qSize < 1 || iSize < 1) {
        return NULL;
    }

    queue = (CQueue_t*)vmalloc(sizeof(CQueue_t));
    if (queue == NULL) {
        return NULL;
    }
    queue->front = 0;
    queue->rear  = 0;
    queue->qSize = qSize;
    queue->count = 0;
    queue->iSize = iSize;
    queue->data  = vmalloc(qSize * iSize);
    if (queue->data == NULL) {
        vfree(queue);
        return NULL;
    }
    spin_lock_init(&queue->spinlock);
    return queue;
}

void CQueueDelete(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    if (queue == NULL) {
        return;
    }
    if (queue->data != NULL) {
        vfree(queue->data);
    }
    vfree(queue);
}

uint32_t  CQueueCapacity(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }

    return queue->qSize;
}

uint32_t  CQueueLength(CQueueHandle_t handle) {
    uint32_t    count = 0;
    CQueue_t* queue = (CQueue_t*)handle;
    if (queue == NULL) {
        return count;
    }
    spin_lock(&queue->spinlock);
    count = queue->count;
    spin_unlock(&queue->spinlock);
    return count;
}


uint32_t  CQueueItemSize(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    if (queue == NULL) {
        return 0;
    }
    return queue->iSize;
}

int32_t  CQueueEnqueue(CQueueHandle_t handle, void* item) {
    CQueue_t* queue = (CQueue_t*)handle;
    spin_lock(&queue->spinlock);
    if ((queue == NULL) || (queue->count >= queue->qSize)) {
        spin_unlock(&queue->spinlock);
        return -1;
    }
    memcpy(queue->data + queue->rear * queue->iSize, item, queue->iSize);
    queue->rear = (queue->rear + 1) % queue->qSize;
    queue->count++;
    spin_unlock(&queue->spinlock);
    return 0;
}

void*     CQueueDequeue(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    void *item = NULL;
    spin_lock(&queue->spinlock);
    if ((queue == NULL) || (queue->count <= 0)) {
        spin_unlock(&queue->spinlock);
        return NULL;
    }
    item = queue->data + queue->front * queue->iSize;
    queue->front = (queue->front + 1) % queue->qSize;
    queue->count--;
    spin_unlock(&queue->spinlock);
    return item;
}

void*     CQueuePeek(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    void *item = NULL;
    spin_lock(&queue->spinlock);
    if ((queue == NULL) || (queue->count <= 0)) {
        spin_unlock(&queue->spinlock);
        return NULL;
    }
    item = queue->data + queue->front * queue->iSize;
    spin_unlock(&queue->spinlock);
    return item;
}


int32_t  CQueueEnqueueFromISR(CQueueHandle_t handle, void* item) {
    CQueue_t* queue = (CQueue_t*)handle;
	unsigned long flags;
	spin_lock_irqsave(&queue->spinlock, flags);
    if ((queue == NULL) || (queue->count >= queue->qSize)) {
		spin_unlock_irqrestore(&queue->spinlock, flags);
        return -1;
    }
    memcpy(queue->data + queue->rear * queue->iSize, item, queue->iSize);
    queue->rear = (queue->rear + 1) % queue->qSize;
    queue->count++;
    spin_unlock_irqrestore(&queue->spinlock, flags);
    return 0;
}

void*     CQueueDequeueFromISR(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    void *item = NULL;
	unsigned long flags;
	spin_lock_irqsave(&queue->spinlock, flags);
    if ((queue == NULL) || (queue->count <= 0)) {
		spin_unlock_irqrestore(&queue->spinlock, flags);
        return NULL;
    }
    item = queue->data + queue->front * queue->iSize;
    queue->front = (queue->front + 1) % queue->qSize;
    queue->count--;
    spin_unlock_irqrestore(&queue->spinlock, flags);
    return item;
}

void*     CQueuePeekFromISR(CQueueHandle_t handle) {
    CQueue_t* queue = (CQueue_t*)handle;
    void *item = NULL;
	unsigned long flags;
	spin_lock_irqsave(&queue->spinlock, flags);
    if ((queue == NULL) || (queue->count <= 0)) {
		spin_unlock_irqrestore(&queue->spinlock, flags);
        return NULL;
    }
    item = queue->data + queue->front * queue->iSize;
    spin_unlock_irqrestore(&queue->spinlock, flags);
    return item;
}
