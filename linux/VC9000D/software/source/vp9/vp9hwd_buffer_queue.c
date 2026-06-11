/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#include "vp9hwd_buffer_queue.h"
#include "vp9hwd_decoder.h"
#include "vp9hwd_container.h"
#include "dwlthread.h"
#include "dec_log.h"
#include "fifo.h"

#ifdef BUFFER_QUEUE_PRINT_STATUS
#define PRINT_COUNTS(x) PrintCounts(x)
#else
#define PRINT_COUNTS(x)
#endif /* BUFFER_QUEUE_PRINT_STATUS */

/* Data structure to hold this picture buffer queue instance data. */
struct BQueue {
  pthread_mutex_t cs; /* Critical section to protect data. */
  i32 n_buffers;      /* Number of buffers contained in total. */
  i32 n_references[MAX_PIC_BUFFERS]; /* Reference counts on buffers.
                                              Index is buffer#.  */
  i32 ref_status[VP9_REF_LIST_SIZE]; /* Reference status of the decoder. Each
                                       element contains index to buffer to an
                                       active reference. */
  FifoInst empty_fifo; /* Queue holding empty, unreferred buffer indices. */
};

static void IncreaseRefCount(struct BQueue* q, i32 i);
static void DecreaseRefCount(struct BQueue* q, i32 i);
static void ClearRefCount(struct BQueue* q, i32 i);
#ifdef BUFFER_QUEUE_PRINT_STATUS
static inline void PrintCounts(struct BQueue* q);
#endif

u32 Vp9BufferQueueCountReferencedBuffers(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  u32 i,j;
  u32 is_referenced, ref_count = 0;
  struct BQueue* q = (struct BQueue*)queue;
  pthread_mutex_lock(&q->cs);


  for (i = 0; i < MAX_PIC_BUFFERS; i++) {
    is_referenced = 0;

    for (j = 0; j < VP9_REF_LIST_SIZE; j++) {
      if (q->ref_status[j] == i)
        is_referenced++;
    }
    if(is_referenced) ref_count++;
  }
  pthread_mutex_unlock(&q->cs);

#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("# %i\n", ref_count);
#endif /* BUFFER_QUEUE_PRINT_STATUS */

  return ref_count;
}

void Vp9BufferQueueResetReferences(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  i32 i;
  struct BQueue* q = (struct BQueue*)queue;
  pthread_mutex_lock(&q->cs);
  for (i = 0; i < sizeof(q->ref_status) / sizeof(q->ref_status[0]); i++) {
    q->ref_status[i] = REFERENCE_NOT_SET;
  }
  PRINT_COUNTS(q);
  pthread_mutex_unlock(&q->cs);
}

BufferQueue Vp9BufferQueueInitialize(i32 n_buffers) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(n_buffers=%i)", n_buffers);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  ASSERT(n_buffers >= 0);
  i32 i;
  FifoObject j;
  enum FifoRet ret;
  struct BQueue* q = (struct BQueue*)DWLcalloc(1, sizeof(struct BQueue));
  if (q == NULL) {
    return NULL;
  }
  DWLmemset(q, 0, sizeof(struct BQueue));

  if (FifoInit(MAX_PIC_BUFFERS, &q->empty_fifo) != FIFO_OK ||
      pthread_mutex_init(&q->cs, NULL)) {
    Vp9BufferQueueRelease(q, 1);
    return NULL;
  }
  /* Add picture buffers among empty picture buffers. */
  for (i = 0; i < n_buffers; i++) {
    j = (FifoObject)(addr_t)i;
    ret = FifoPush(q->empty_fifo, j, FIFO_EXCEPTION_ENABLE);
    if (ret != FIFO_OK) {
      Vp9BufferQueueRelease(q, 1);
      return NULL;
    }
    q->n_buffers++;
  }
  Vp9BufferQueueResetReferences(q);
  return q;
}

void Vp9BufferQueueRelease(BufferQueue queue, i32 safe) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  ASSERT(queue);
  struct BQueue* q = (struct BQueue*)queue;
  if (q == NULL) return;

  if (q->empty_fifo) {/* Empty the fifo before releasing. */
    if (safe) {
      i32 i;
      FifoObject j;
      enum FifoRet ret;
      for (i = 0; i < q->n_buffers; i++) {
        ret = FifoPop(q->empty_fifo, &j, FIFO_EXCEPTION_ENABLE);
        ASSERT(ret == FIFO_OK || ret == FIFO_EMPTY || ret == FIFO_ABORT);
        (void)ret;
      }
    }
    FifoRelease(q->empty_fifo);
  }
  pthread_mutex_destroy(&q->cs);
  free(q);
}

void Vp9BufferQueueUpdateRef(BufferQueue queue, u32 ref_flags, i32 buffer) {
  u32 i = 0;
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(ref_flags=0x%X, buffer=%i)", ref_flags, buffer);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  if (queue == NULL) return;
  ASSERT(queue);
  struct BQueue* q = (struct BQueue*)queue;
  ASSERT((buffer >= 0 || buffer == REFERENCE_NOT_SET) && buffer < q->n_buffers);
  pthread_mutex_lock(&q->cs);

  for (i = 0; i < VP9_REF_LIST_SIZE; i++) {
    if ((ref_flags & (1 << i)) && buffer != q->ref_status[i]) {
      if (q->ref_status[i] != REFERENCE_NOT_SET) {
        DecreaseRefCount(q, q->ref_status[i]);
      }
      q->ref_status[i] = buffer;
      if (buffer != REFERENCE_NOT_SET)
        IncreaseRefCount(q, buffer);
    }
    ASSERT(q->ref_status[i] == REFERENCE_NOT_SET || q->n_references[q->ref_status[i]] != 0);
  }

  PRINT_COUNTS(q);
  pthread_mutex_unlock(&q->cs);
}

i32 Vp9BufferQueueGetRef(BufferQueue queue, u32 index) {
  struct BQueue* q = (struct BQueue*)queue;
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(%u)", index);
  APITRACE(" # %d\n", q->ref_status[index]);
  PRINT_COUNTS(q);
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  if(q->ref_status[index]>0 && q->ref_status[index] < q->n_buffers)
    return q->ref_status[index];
  else
    return 0;
}

i32 Vp9BufferQueueAddRef(BufferQueue queue, i32 buffer) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(buffer=%i)", buffer);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  struct BQueue* q = (struct BQueue*)queue;
  if (q == NULL) return HANTRO_NOK;
  if(!(buffer >= 0 && buffer < q->n_buffers))
    return HANTRO_NOK;
  pthread_mutex_lock(&q->cs);
  IncreaseRefCount(q, buffer);
  pthread_mutex_unlock(&q->cs);
  return HANTRO_OK;
}

void Vp9BufferQueueRemoveRef(BufferQueue queue, i32 buffer) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(buffer=%i)", buffer);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  if (queue == NULL) return;
  struct BQueue* q = (struct BQueue*)queue;
  ASSERT(buffer >= 0 && buffer < q->n_buffers);
  pthread_mutex_lock(&q->cs);
  DecreaseRefCount(q, buffer);
  pthread_mutex_unlock(&q->cs);
}

i32 Vp9BufferQueueGetBuffer(BufferQueue queue, u32 limit) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  i32 i;
  FifoObject j;
  enum FifoRet ret;
  struct BQueue* q = (struct BQueue*)queue;
  ASSERT(q->empty_fifo);
  while(1) {
    pthread_mutex_lock(&q->cs);
    ret = FifoPop(q->empty_fifo, &j, FIFO_EXCEPTION_ENABLE);
    if (ret == FIFO_EMPTY) {
      if (q->n_buffers < limit) {/* return and allocate new buffer */
        pthread_mutex_unlock(&q->cs);
        return -1;
      } else {/* wait for free one */
#ifndef GET_FREE_BUFFER_NON_BLOCK
        ret = FifoPop(q->empty_fifo, &j, FIFO_EXCEPTION_DISABLE);
        if (ret == FIFO_ABORT) {
          pthread_mutex_unlock(&q->cs);
          return ABORT_MARKER;
        }
#else
        pthread_mutex_unlock(&q->cs);
        return EMPTY_MARKER;
#endif
      }
    } else {
      if (ret == FIFO_ABORT) {
        pthread_mutex_unlock(&q->cs);
        return ABORT_MARKER;
      }
    }

    ASSERT(ret == FIFO_OK);

    i = (i32)((addr_t)j);
    if (q->n_references[i] > 0)
      ret = FifoPush(q->empty_fifo, j, FIFO_EXCEPTION_ENABLE);
    else {
      pthread_mutex_unlock(&q->cs);
      break;
    }
    pthread_mutex_unlock(&q->cs);
  }

  pthread_mutex_lock(&q->cs);
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("# %i\n", i);
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  IncreaseRefCount(q, i);
  pthread_mutex_unlock(&q->cs);
  return i;
}

void Vp9BufferQueueSetAbort(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  struct BQueue* q = (struct BQueue*)queue;
  if (!q) return;
  ASSERT(q->empty_fifo);
  FifoSetAbort(q->empty_fifo);
}

void Vp9BufferQueueClearAbort(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  struct BQueue* q = (struct BQueue*)queue;
  if (!q) return;
  ASSERT(q->empty_fifo);
  FifoClearAbort(q->empty_fifo);
}

void Vp9BufferQueueEmptyRef(BufferQueue queue, i32 buffer) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(buffer=%i)", buffer);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  struct BQueue* q = (struct BQueue*)queue;
  if (!q) return;
  pthread_mutex_lock(&q->cs);
  ClearRefCount(q, buffer);
  pthread_mutex_unlock(&q->cs);
}


void Vp9BufferQueueWaitPending(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  ASSERT(queue);
  struct BQueue* q = (struct BQueue*)queue;
  /* TODO(mheikkinen): cherry-pick non-busyloop implementation from g1. */
  while (FifoCount(q->empty_fifo) != (u32)q->n_buffers) sched_yield();
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("#\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
}

void Vp9BufferQueueAddBuffer(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  ASSERT(queue);
  enum FifoRet ret;
  struct BQueue* q = (struct BQueue*)queue;
  FifoObject j;
  pthread_mutex_lock(&q->cs);
  /* Add one picture buffer among empty picture buffers. */
  j = (FifoObject)(addr_t)q->n_buffers;
  ret = FifoPush(q->empty_fifo, j, FIFO_EXCEPTION_ENABLE);
  ASSERT(ret == FIFO_OK);
  (void)ret;
  q->n_buffers++;
  pthread_mutex_unlock(&q->cs);

#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("#\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
}

static void IncreaseRefCount(struct BQueue* q, i32 i) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(buffer=%i)", i);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  q->n_references[i]++;
  ASSERT(q->n_references[i] >= 0); /* No negative references. */
  PRINT_COUNTS(q);
}

static void DecreaseRefCount(struct BQueue* q, i32 i) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(buffer=%i)", i);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  enum FifoRet ret;
  FifoObject j;
  if(q->n_references[i] > 0)
    q->n_references[i]--;
  else
    return;
  ASSERT(q->n_references[i] >= 0); /* No negative references. */
  PRINT_COUNTS(q);
  if (q->n_references[i] == 0) {
    /* Once picture buffer is no longer referred to, it can be put to
       the empty fifo. */
#ifdef BUFFER_QUEUE_PRINT_STATUS
    APITRACE("Buffer #%i put to empty pool\n", i);
#endif /* BUFFER_QUEUE_PRINT_STATUS */
    j = (FifoObject)(addr_t)i;
    if (!FifoHasObject(q->empty_fifo, j)) {
      ret = FifoPush(q->empty_fifo, j, FIFO_EXCEPTION_ENABLE);
      ASSERT(ret == FIFO_OK);
      (void)ret;
    }
  }
}

static void ClearRefCount(struct BQueue* q, i32 i) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("(buffer=%i)", i);
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  enum FifoRet ret;
  FifoObject j;
  if(q->n_references[i] > 0)
    q->n_references[i] = 0;
  else
    return;
  ASSERT(q->n_references[i] >= 0); /* No negative references. */
  PRINT_COUNTS(q);
  if (q->n_references[i] == 0) {
    /* Once picture buffer is no longer referred to, it can be put to
       the empty fifo. */
#ifdef BUFFER_QUEUE_PRINT_STATUS
    APITRACE("Buffer #%i put to empty pool\n", i);
#endif /* BUFFER_QUEUE_PRINT_STATUS */
    j = (FifoObject)(addr_t)i;
    ret = FifoPush(q->empty_fifo, j, FIFO_EXCEPTION_ENABLE);
    ASSERT(ret == FIFO_OK);
    (void)ret;
  }
}

void Vp9BufferQueueReset(BufferQueue queue) {
#ifdef BUFFER_QUEUE_PRINT_STATUS
  APITRACE(__FUNCTION__);
  APITRACE("()");
  APITRACE("\n");
#endif /* BUFFER_QUEUE_PRINT_STATUS */
  ASSERT(queue);
  struct BQueue* q = (struct BQueue*)queue;
  i32 i;
  FifoObject j;
  enum FifoRet ret;
  if (q->empty_fifo) {/* Empty the fifo before releasing. */
    FifoRelease(q->empty_fifo);
  }
  pthread_mutex_destroy(&q->cs);
  pthread_mutex_init(&q->cs, NULL);

  ret = FifoInit(MAX_PIC_BUFFERS, &q->empty_fifo);
  if (FIFO_ERROR_MEMALLOC == ret)
    return;
  ASSERT(q->empty_fifo);
#ifdef USE_OMXIL_BUFFER
  q->n_buffers = 0;
  DWLmemset(q->n_references, 0, sizeof(q->n_references));
  Vp9BufferQueueResetReferences(q);
#else
  //Vp9BufferQueueResetReferences(q);
  for (i = 0; i < q->n_buffers; i++) {
    if (!q->n_references[i]) {
      /* Push the buffers that are in fifo back. */
      j = (FifoObject)(addr_t)i;
      ret = FifoPush(q->empty_fifo, j, FIFO_EXCEPTION_ENABLE);
      ASSERT(ret == FIFO_OK);
      (void)ret;
    }
  }
#endif
  (void)i;
  (void)j;
  (void)ret;
}


#ifdef BUFFER_QUEUE_PRINT_STATUS
static inline void PrintCounts(struct BQueue* q) {
  i32 i = 0;
  for (i = 0; i < q->n_buffers; i++) APITRACE("%u", q->n_references[i]);
  APITRACE("|");
  for (i = 0; i < sizeof(q->ref_status) / sizeof(q->ref_status[0]); i++)
    APITRACE("[%u]:%i|", i, q->ref_status[i]);
  APITRACE("\n");
}
#endif /* BUFFER_QUEUE_PRINT_STATUS */
