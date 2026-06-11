/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#ifndef __FIFO_H__
#define __FIFO_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "base_type.h"

/* FIFO_DATATYPE must be defined to hold specific type of objects. If it is not
 * defined, we need to report an error. */
#define FIFO_DATATYPE ptr_t
#ifndef FIFO_DATATYPE
#error "You must define FIFO_DATATYPE to use this module."
#endif /* FIFO_DATATYPE */

typedef FIFO_DATATYPE FifoObject;

/* Possible return values. */
enum FifoRet {
  FIFO_OK,             /* Operation was successful. */
  FIFO_ERROR_MEMALLOC, /* Failed due to memory allocation error. */
  FIFO_EMPTY,
  FIFO_FULL,
  FIFO_NOK,
  FIFO_ABORT = 0x7FFFFFFF
};

enum FifoException { FIFO_EXCEPTION_DISABLE, FIFO_EXCEPTION_ENABLE };

typedef void* FifoInst;

#ifndef FIFO_BUILD_SUPPORT
#define FifoInit(num_of_slots, instance) (FIFO_OK)
#define FifoPush(inst, object, exception_enable) (FIFO_OK)
#define FifoPop(inst, object, exception_enable) (FIFO_OK)

#define FifoCount(inst) (0)
#define FifoHasObject(inst, object) (1)

#define FifoRelease(inst) ;
#define FifoSetAbort(inst) ;
#define FifoClearAbort(inst) ;
#else
/* FifoInit initializes the queue.
 * |num_of_slots| defines how many slots to reserve at maximum.
 * |instance| is output parameter holding the instance. */
enum FifoRet FifoInit(u32 num_of_slots, FifoInst* instance);

/* FifoPush pushes an object to the back of the queue. Ownership of the
 * contained object will be moved from the caller to the queue. Returns OK
 * if the object is successfully pushed into fifo.
 *
 * |inst| is the instance push to.
 * |object| holds the pointer to the object to push into queue.
 * |exception_enable| enable FIFO_FULL return value */
enum FifoRet FifoPush(FifoInst inst, FifoObject object,
                      enum FifoException exception_enable);

/* FifoPop returns object from the front of the queue. Ownership of the popped
 * object will be moved from the queue to the caller. Returns OK if the object
 * is successfully popped from the fifo.
 *
 * |inst| is the instance to pop from.
 * |object| holds the pointer to the object popped from the queue.
 * |exception_enable| enable FIFO_EMPTY return value */
enum FifoRet FifoPop(FifoInst inst, FifoObject* object,
                     enum FifoException exception_enable);

/* Ask how many objects there are in the fifo. */
u32 FifoCount(FifoInst inst);

/* Check if object is contained in fifo */
u32 FifoHasObject(FifoInst inst, FifoObject object);

/* FifoRelease releases and deallocated queue. User needs to make sure the
 * queue is empty and no threads are waiting in FifoPush or FifoPop.
 * |inst| is the instance to release. */
void FifoRelease(FifoInst inst);
void FifoSetAbort(FifoInst inst);
void FifoClearAbort(FifoInst inst);
#endif

#endif /* __FIFO_H__ */
