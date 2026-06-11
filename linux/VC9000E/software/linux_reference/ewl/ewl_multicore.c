/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2015 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Multi-core Job Management in Direct Mode
--
------------------------------------------------------------------------------*/

#include "base_type.h"
#include "ewl.h"
#include "ewl_common.h"
//#include "ewl_local.h"
#include "encswhwregisters.h"
#include "encdec400.h"
#include "enc_log.h"
#include "ewl_memsync.h"
#include "encufbc.h"

#ifdef __FREERTOS__
#include "user_freertos.h"
#include "dev_common_freertos.h"
#include "memalloc_freertos.h"
#elif defined(__linux__)
#include "memalloc.h"
#else
#endif

#ifdef __FREERTOS__
//nothing
#elif defined(__linux__)
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <unistd.h>
#endif


/*HW core wait*/
pthread_mutex_t ewl_mutex = PTHREAD_MUTEX_INITIALIZER;

static EWLCoreWait_t coreWait;
static pthread_mutex_t ewl_refer_counter_mutex = PTHREAD_MUTEX_INITIALIZER;

static inline u32 LastCore(struct queue *workers) {
  EWLWorker *last = (EWLWorker *)(workers->head);
  return last->core_id;
}

static inline u32 FirstCore(struct queue *workers) {
  EWLWorker *first = (EWLWorker *)(workers->tail);
  return first->core_id;
}


/* Get one job from queue */
EWLCoreWaitJob_t *EWLDequeueCoreOutJob(const void *inst, u32 waitCoreJobid) {
  EWLCoreWait_t *pCoreWait = (EWLCoreWait_t *)&coreWait;
  EWLCoreWaitJob_t *job, *out;

  while (!pCoreWait->bFlush) {
    out = NULL;
    pthread_mutex_lock(&pCoreWait->out_mutex);

    job = (EWLCoreWaitJob_t *)queue_tail(&pCoreWait->out);

    while (job != NULL) {
      if (job->id == waitCoreJobid) {
        out = job;
        queue_remove(&pCoreWait->out, (struct node *)job);
        break;
      }

      job = (EWLCoreWaitJob_t *)((struct node *)job->next);
    }

    while (job == NULL && !pCoreWait->bFlush) {
      pthread_cond_wait(&pCoreWait->out_cond, &pCoreWait->out_mutex);
      job = (EWLCoreWaitJob_t *)queue_tail(&pCoreWait->out);
    }

    pthread_mutex_unlock(&pCoreWait->out_mutex);
    if (out != NULL) return out;
  }

  return NULL;
}

/* Get one job from queue */
static EWLCoreWaitJob_t *EWLDequeueCoreWaitJob(EWLCoreWait_t *coreWait) {
  pthread_mutex_lock(&coreWait->job_mutex);
  EWLCoreWaitJob_t *job = (EWLCoreWaitJob_t *)queue_tail(&coreWait->jobs);
  while (job == NULL && !coreWait->bFlush) {
    pthread_cond_wait(&coreWait->job_cond, &coreWait->job_mutex);
    job = (EWLCoreWaitJob_t *)queue_tail(&coreWait->jobs);
  }
  pthread_mutex_unlock(&coreWait->job_mutex);
  return job;
}

/* Get one job from job pool and if no available allocate a new one */
static EWLCoreWaitJob_t *EWLGetJobfromPool(EWLCoreWait_t *coreWait) {
  EWLCoreWaitJob_t *job = (EWLCoreWaitJob_t *)queue_get(&coreWait->job_pool);
  if (job == NULL)
    job = (EWLCoreWaitJob_t *)EWLmalloc(sizeof(EWLCoreWaitJob_t));

  return job;
}

/*Put un-used job to pool for other usage*/
void EWLPutJobtoPool(const void *inst, struct node *job) {
  pthread_mutex_lock(&coreWait.job_mutex);
  queue_put(&coreWait.job_pool, job);
  pthread_mutex_unlock(&coreWait.job_mutex);
}

void EWLGetRegsAfterFrameDone(const void *inst, EWLCoreWaitJob_t *job,
                              u32 irq_status) {
  //vcx_cwl_t *enc = (vcx_cwl_t *)inst;
  int i;

  if (irq_status == ASIC_STATUS_FRAME_READY) {
    for (i = 0; i < ASIC_SWREG_AMOUNT; i++) {
      job->VCE_reg[i] = EWLReadReg(inst, i * 4);
    }
  }

  if (job->dec400_enable == 2) {
#ifndef SYSTEM_BUILD
    job->dec400_callback(inst, NULL);
#endif
  }

  //stop ufbc
  if(job->ufbcMode) {
    job->ufbc_callback(inst, NULL, job->ufbcMode);
  }

  if (job->axife_callback) job->axife_callback(job->inst, NULL);

  if (job->l2cache_enable) {
    job->l2cache_callback(inst, &job->l2cache_data);
  }

  EWLReleaseHw(inst);
}

void EWLGetCoreOutRel(const void *inst, i32 ewl_ret, EWLCoreWaitJob_t *job) {
  u32 i = 0;
  u32 status = job->out_status;
  u32 core_id = FirstCore(EWLGetWorkers(inst)); //FIRST_CORE(enc);
  //enc->unCheckPid = 1;
  ewlSetUncheckPidFlag(inst);

  /*move the core received irq to queue tail for subsequent register operation is for core on the tail*/
  if (core_id != job->core_id) {
    pthread_mutex_lock(&ewl_mutex);
    struct queue *workers = EWLGetWorkers(inst);
    EWLWorker *worker = (EWLWorker *)queue_tail(workers);
    while (worker && worker->core_id != job->core_id) {
      worker = (EWLWorker *)worker->next;
    }
	if(worker) {
      queue_remove(workers, (struct node *)worker);
      queue_put_tail(workers, (struct node *)worker);
	}
    pthread_mutex_unlock(&ewl_mutex);
  }

  if (ewl_ret != EWL_OK) {
    job->out_status = ASIC_STATUS_ERROR;

    PTRACE_E("EWLGetCoreOutRel: ERROR Core return != EWL_OK.");
    EWLDisableHW(inst, ASIC_REG_INDEX_STATUS * 4, 0);

    /* Release Core so that it can be used by other codecs */
    EWLGetRegsAfterFrameDone(inst, job, job->out_status);
  } else {
    /* Check ASIC status bits and release HW */
    status &= ASIC_STATUS_ALL;

    if (status & ASIC_STATUS_ERROR) {
      /* Get registers for debugging */
      status = ASIC_STATUS_ERROR;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_FUSE_ERROR) {
      /* Get registers for debugging */
      status = ASIC_STATUS_ERROR;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_HW_TIMEOUT) {
      /* Get registers for debugging */
      status = ASIC_STATUS_HW_TIMEOUT;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_BUFF_FULL) {
      /* ASIC doesn't support recovery from buffer full situation,
             * at the same time with buff full ASIC also resets itself. */
      status = ASIC_STATUS_BUFF_FULL;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_SBI_TIMEOUT) {
      status = ASIC_STATUS_SBI_TIMEOUT;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_SBI_OUT_OF_SYNC) {
      status = ASIC_STATUS_SBI_OUT_OF_SYNC;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_HW_RESET) {
      status = ASIC_STATUS_HW_RESET;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if (status & ASIC_STATUS_FRAME_READY) {
      /* read out all register */
      status = ASIC_STATUS_FRAME_READY;
      EWLGetRegsAfterFrameDone(inst, job, status);
    } else if ((status & ASIC_STATUS_LINE_BUFFER_DONE) && (status & ASIC_STATUS_SLICE_READY)) {
      status = ASIC_STATUS_SLICE_READY | ASIC_STATUS_LINE_BUFFER_DONE;
      job->slices_rdy = ((EWLReadReg(inst, 7 * 4) >> 17) & 0xFF);
      job->VCE_reg[196] = EWLReadReg(inst, 196 * 4);

      //save sw handshake rd pointer if sw handshake is enabled
      if ((!(job->VCE_reg[196] >> 31)) &&
          (job->low_latency_rd < ((job->VCE_reg[196] & 0x000ffc00) >> 10)))
        job->low_latency_rd = ((job->VCE_reg[196] & 0x000ffc00) >> 10);
      else
        status = 0;
    } else if (status & ASIC_STATUS_SLICE_READY) {
      status = ASIC_STATUS_SLICE_READY;
      job->slices_rdy = ((EWLReadReg(inst, 7 * 4) >> 17) & 0xFF);
    } else if (status & ASIC_STATUS_LINE_BUFFER_DONE) {
      status = ASIC_STATUS_LINE_BUFFER_DONE;
      job->VCE_reg[196] = EWLReadReg(inst, 196 * 4);

      //save sw handshake rd pointer if sw handshake is enabled
      if ((!(job->VCE_reg[196] >> 31)) &&
          (job->low_latency_rd < ((job->VCE_reg[196] & 0x000ffc00) >> 10)))
        job->low_latency_rd = ((job->VCE_reg[196] & 0x000ffc00) >> 10);
      else
        status = 0;
    } else if (status & ASIC_STATUS_SEGMENT_READY) {
      status = ASIC_STATUS_SEGMENT_READY;
      for (i = 1; i < ASIC_SWREG_AMOUNT; i++) {
        job->VCE_reg[i] = EWLReadReg(inst, i * 4);
      }
    }

    job->out_status = status;
  }
}


/* get register setting from api, reserve a core and set registers*/
static void *EWLCoreWaitThread(void *pCoreWait) {
  EWLCoreWait_t *coreWait = (EWLCoreWait_t *)pCoreWait;
  struct node *job;
  u32 reserve_core_info = 0;
  u32 i, client_type_previous;
  //FIXME: vcx_cwl_t ewl;
  EWLCoreWaitOut_t waitOut;
  i32 ewl_ret = EWL_OK;
  u32 has_output = 0;
  u32 wait_error = 0;
  const void *inst;

  while ((job = (struct node *)EWLDequeueCoreWaitJob(coreWait)) != NULL) {
#if 0 /* FIXME: why use an fake ewl here ? */
    ewl.fd_enc = ((vcx_cwl_t *)(((EWLCoreWaitJob_t *)job)->inst))->fd_enc;
    memset(&waitOut, 0, sizeof(CORE_WAIT_OUT));

    /* wait for VCE Core done */
    if (wait_error == 0 &&
        ((ewl_ret = EWLWaitHwRdy(&ewl, NULL, &waitOut, NULL)) != EWL_OK)) {
      wait_error = 1;
    }
#else
    memset(&waitOut, 0, sizeof(EWLCoreWaitOut_t));
    inst = ((EWLCoreWaitJob_t *)job)->inst;
     /* wait for VCE Core done */
    if (wait_error == 0 &&
        ((ewl_ret = EWLWaitHwRdy(inst, NULL, &waitOut, NULL)) != EWL_OK)) {
      wait_error = 1;
    }
#endif

    pthread_mutex_lock(&coreWait->job_mutex);
    job = queue_tail(&coreWait->jobs);

    while (job != NULL) {
      struct node *next = job->next;

      for (i = 0; i < waitOut.irq_num; i++) {
        if (waitOut.job_id[i] == ((EWLCoreWaitJob_t *)job)->id) {
          ((EWLCoreWaitJob_t *)job)->out_status = waitOut.irq_status[i];

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
          ((EWLCoreWaitJob_t *)job)->out_poll_sliceinfo_status =
                waitOut.irq_status[i] & ASIC_STATUS_POLL_SLICEINFO_TIMEOUT;
#endif

#ifdef SUPPORT_UFBC
          ((EWLCoreWaitJob_t *)job)->out_ufbc_status =
                (waitOut.irq_status[i] & ASIC_STATUS_UFBC_DEC_ERR)|
                (waitOut.irq_status[i] & ASIC_STATUS_UFBC_CFG_ERR);
#endif
          u32 sbi_status = EWLReadReg(inst, 349 * 4);
          if ((sbi_status >> 30) & 0x01) {
            // sbi out of sync
            ((EWLCoreWaitJob_t *)job)->out_status |= ASIC_STATUS_SBI_OUT_OF_SYNC;
          } else {
            ((EWLCoreWaitJob_t *)job)->out_status &= ~ASIC_STATUS_SBI_OUT_OF_SYNC;
          }
          if ((sbi_status >> 29) & 0x01) {
            // sbi timeout
            ((EWLCoreWaitJob_t *)job)->out_status |= ASIC_STATUS_SBI_TIMEOUT;
          } else {
            ((EWLCoreWaitJob_t *)job)->out_status &= ~ASIC_STATUS_SBI_TIMEOUT;
          }

          EWLGetCoreOutRel(((EWLCoreWaitJob_t *)job)->inst, ewl_ret,
                           (EWLCoreWaitJob_t *)job);

          if (((EWLCoreWaitJob_t *)job)->out_status &
              (ASIC_STATUS_FUSE_ERROR | ASIC_STATUS_HW_TIMEOUT |
               ASIC_STATUS_BUFF_FULL | ASIC_STATUS_HW_RESET |
               ASIC_STATUS_ERROR | ASIC_STATUS_FRAME_READY |
               ASIC_STATUS_SBI_TIMEOUT | ASIC_STATUS_SBI_OUT_OF_SYNC |
               ASIC_STATUS_UFBC_DEC_ERR | ASIC_STATUS_UFBC_CFG_ERR |
			   ASIC_STATUS_POLL_SLICEINFO_TIMEOUT)) {
            queue_remove(&coreWait->jobs, (struct node *)job);
            pthread_mutex_lock(&coreWait->out_mutex);
            queue_put(&coreWait->out, job);
            pthread_mutex_unlock(&coreWait->out_mutex);
            has_output = 1;
          } else if (((EWLCoreWaitJob_t *)job)->out_status) {
            struct node *tmp_out = (struct node *)EWLGetJobfromPool(coreWait);
            if (tmp_out == NULL)
             break;
            memcpy(tmp_out, job, sizeof(EWLCoreWaitJob_t));
            pthread_mutex_lock(&coreWait->out_mutex);
            queue_put(&coreWait->out, tmp_out);
            pthread_mutex_unlock(&coreWait->out_mutex);
            has_output = 1;
          }
          break;
        }
      }

      if (wait_error == 1) {
        EWLGetCoreOutRel(((EWLCoreWaitJob_t *)job)->inst, ewl_ret,
                         (EWLCoreWaitJob_t *)job);
        queue_remove(&coreWait->jobs, (struct node *)job);
        pthread_mutex_lock(&coreWait->out_mutex);
        queue_put(&coreWait->out, job);
        pthread_mutex_unlock(&coreWait->out_mutex);
        has_output = 1;
      }

      job = next;
    }

    pthread_mutex_unlock(&coreWait->job_mutex);

    if (has_output == 1) {
      pthread_cond_broadcast(&coreWait->out_cond);
      has_output = 0;
    }
  }

  return NULL;
}

void EWLEnqueueWaitjob(const void *inst, EWLWaitJobCfg_t *cfg) {
  //vcx_cwl_t *enc = (vcx_cwl_t *)inst;
  //if (enc == NULL) return;

  //if (enc->vcmd_mode == VCMD_MODE_ENABLED) return;

  //enqueue the job for core wait thread
  pthread_mutex_lock(&coreWait.job_mutex);

  EWLCoreWaitJob_t *job = EWLGetJobfromPool(&coreWait);
  if (job == NULL) {
	pthread_mutex_unlock(&coreWait.job_mutex);
	return;
  }
  memset(job, 0, sizeof(EWLCoreWaitJob_t));
  job->id = cfg->waitCoreJobid;
  job->core_id = LastCore(EWLGetWorkers(inst));
  job->inst = inst;
  job->dec400_enable = cfg->dec400_enable;
  job->dec400_callback = cfg->dec400_callback;
  job->axife_enable = cfg->axife_enable;
  job->axife_callback = cfg->axife_callback;
  job->l2cache_enable = cfg->l2cache_enable;
  memcpy(&job->l2cache_data, cfg->l2cache_data, sizeof(CacheData_t));
  job->l2cache_callback = cfg->l2cache_callback;
  job->ufbcMode = cfg->ufbcMode;
  job->ufbc_callback = cfg->ufbc_callback;

  queue_put(&coreWait.jobs, (struct node *)job);
  pthread_cond_signal(&coreWait.job_cond);
  pthread_mutex_unlock(&coreWait.job_mutex);
}

/* create ewl core wait thread and now just support VCE*/
static void EwlCreateCoreWait(void) {
  EWLCoreWait_t *pCoreWait = &coreWait;

  pthread_attr_t attr;
  pthread_t *tid_CoreWait = (pthread_t *)EWLmalloc(sizeof(pthread_t));
  pthread_mutexattr_t mutexattr;
  pthread_condattr_t condattr;

  if (tid_CoreWait == NULL)
  	return;

  queue_init(&coreWait.jobs);
  queue_init(&coreWait.out);
  queue_init(&coreWait.job_pool);

  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&coreWait.job_mutex, &mutexattr);
  pthread_mutex_init(&coreWait.out_mutex, &mutexattr);
  pthread_mutexattr_destroy(&mutexattr);
  pthread_condattr_init(&condattr);
  pthread_cond_init(&coreWait.job_cond, &condattr);
  pthread_cond_init(&coreWait.out_cond, &condattr);
  pthread_condattr_destroy(&condattr);

  pthread_attr_init(&attr);
  pthread_create(tid_CoreWait, &attr, &EWLCoreWaitThread, pCoreWait);
  pthread_attr_destroy(&attr);

  coreWait.tid_CoreWait = tid_CoreWait;

  return;
}

void EwlReleaseCoreWait(void *inst) {
  /* Wait for Core Wait Thread finish */
  pthread_mutex_lock(&ewl_refer_counter_mutex);
  if (coreWait.tid_CoreWait && coreWait.refer_counter == 0) {
    pthread_join(*coreWait.tid_CoreWait, NULL);

    pthread_mutex_destroy(&coreWait.job_mutex);
    pthread_mutex_destroy(&coreWait.out_mutex);
    pthread_cond_destroy(&coreWait.job_cond);
    pthread_cond_destroy(&coreWait.out_cond);

    EWLfree(coreWait.tid_CoreWait);
    coreWait.tid_CoreWait = NULL;

    free_nodes(coreWait.jobs.tail);
    free_nodes(coreWait.out.tail);
    free_nodes(coreWait.job_pool.tail);

#ifdef SUPPORT_MEM_STATISTIC
    EwlShowMemoryStats();
#endif
  }
  pthread_mutex_unlock(&ewl_refer_counter_mutex);
}

void EWLInitMulticore(u32 clientType)
{
  if (EWL_IS_VIDEO_CLIENT(clientType)) {
    pthread_mutex_lock(&ewl_refer_counter_mutex);
    if (coreWait.refer_counter == 0){
      coreWait.bFlush = false;
      EwlCreateCoreWait();
    }
    coreWait.refer_counter++;
    pthread_mutex_unlock(&ewl_refer_counter_mutex);
  }
}

/**
 * Release multicore thread if there's no instance accessing it
 *
 * \return 0 if success;
 * \return -1 when the reference counter is abnormal;
 */
i32 EWLReleaseMulticore(u32 clientType)
{
  i32 ret=0;

  if (EWL_IS_VIDEO_CLIENT(clientType)) {
    pthread_mutex_lock(&ewl_refer_counter_mutex);
    if (coreWait.refer_counter > 0) {
      coreWait.refer_counter--;

      if (coreWait.refer_counter == 0) {
        pthread_mutex_lock(&coreWait.job_mutex);
        coreWait.bFlush = HANTRO_TRUE;
        pthread_cond_signal(&coreWait.job_cond);
        pthread_mutex_unlock(&coreWait.job_mutex);
      }
    } else {
      PTRACE_E("EWLReleaseMulticore: ERROR value of coreWait.refer_counter.");
      ret = -1;
    }
    pthread_mutex_unlock(&ewl_refer_counter_mutex);
  }
  return ret;
}
