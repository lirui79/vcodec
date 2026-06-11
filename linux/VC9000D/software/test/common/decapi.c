/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#include "decapi.h"
#include "decapi_trace.h"
#include "fifo.h"
#include "sw_util.h"
#include "version.h"
#include "dwl.h"
#include "decapicommon.h"
#include "ppu.h"
#include "vcdecapi.h"

#define MAX_FIFO_CAPACITY (6)

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

/* Pointers to the DWL functionality. */
struct DWL dwl = {DWLReserveHw,          /* ReserveHw */
         DWLReleaseHw,          /* ReleaseHw */
         DWLMallocLinear,       /* MallocLinear */
         DWLFreeLinear,         /* FreeLinear */
         DWLWriteReg,           /* WriteReg */
         DWLReadReg,            /* ReadReg */
         DWLEnableHw,           /* EnableHw */
         DWLDisableHw,          /* DisableHw */
         DWLWaitHwReady,        /* WaitHwReady */
         DWLSetIRQCallback,     /* SetIRQCallback */
         DWLmalloc,                /* malloc */
         DWLfree,                  /* free */
         DWLcalloc,             /* calloc */
         DWLmemcpy,                /* memcpy */
         DWLmemset,                /* memset */
         pthread_create,        /* pthread_create */
         pthread_exit,          /* pthread_exit */
         pthread_join,          /* pthread_join */
#ifdef SEM_REPLACE_MUTEX
         sem_imp_mutex_init,
         sem_destroy,
         sem_wait,
         sem_post,
         sem_imp_cond_init,
         sem_destroy,
         sem_imp_cond_wait,
         sem_post,
#else
         pthread_mutex_init,    /* pthread_mutex_init */
         pthread_mutex_destroy, /* pthread_mutex_destroy */
         pthread_mutex_lock,    /* pthread_mutex_lock */
         pthread_mutex_unlock,  /* pthread_mutex_unlock */
         pthread_cond_init,     /* pthread_cond_init */
         pthread_cond_destroy,  /* pthread_cond_destroy */
         pthread_cond_wait,     /* pthread_co/ux/VideoIP-IPD/cn8067/LOGS/VC8000D_LOGS/g2_mcpp_407/sum.lognd_wait */
         pthread_cond_signal,   /* pthread_cond_signal */
#endif
         printf,                /* printf */
};

enum DecodingState {
  DECODER_WAITING_HEADERS
  , DECODER_WAITING_RESOURCES
  , DECODER_DECODING
  , DECODER_SHUTTING_DOWN
  /* Decoder has been shutdown due to error.
     No more decoding but can output pending pictures */
  , DECODER_ERROR_TO_SHUTTING_DOWN
  , DECODER_TERMINATED
};

/* thread for low latency feature */
typedef void* task_handle;
typedef void* (*task_func)(void*);

/* func for low latency feature */
void wait_for_task_completion(task_handle task);
task_handle run_task(task_func func, void* param);
void* send_bytestrm_task(void* param);

/* for low latency feature */
struct LowLatencyDsc_t {
  volatile struct strmInfo send_strm_info; /* use to update stream info in container */
  u32 pic_decoded;  /* decoder will set this flag as 1 after the decoding of current frame is ended */
  u32 exit_send_thread; /* when an error occurs, the decoding thread places the flag */
  u32 frame_len;
  u32 send_len;
  u32 process_end_flag;/* the flag is set to 1 when the decoding error or is complete */
  u32 frame_end; /* before each frame is decoded, the decoding thread waits for the flag to be set by send byte thread */
  u32 frame_start; /* send byte thread will waiting for the flag after a the decoding of last frame is ended to start next frame */
  struct DWLLinearMem ll_buffer;
  u8* input_strm_curr_pos; /*the input stream start before inputting*/
  u8* input_strm_buff_start; /* the start address of input stream buffer, used when ringbuffer is enabled*/
  task_handle task;
};

struct Command {
  enum {
    COMMAND_INIT
    , COMMAND_DECODE
    , COMMAND_SETBUFFERS
    , COMMAND_END_OF_STREAM
    , COMMAND_RELEASE
    , COMMAND_FLUSH
  } id;
  struct {
    struct DecConfig config;
    struct DecInput input;
  } params;
};

/* Dec Wrapper, used in decapi.c and point to struct VCDecDecoderWrapper */
typedef void* DecoderWrapper;

typedef struct DecoderInstance_ {
  DecoderWrapper dec;
  enum DecodingState state;
  FifoInst input_queue;
  pthread_t decode_thread;
  pthread_t output_thread;
  pthread_mutex_t cs_mutex;
  pthread_mutex_t eos_mutex;
  pthread_mutex_t resource_mutex;
  pthread_cond_t eos_cond;
  pthread_cond_t resource_cond;
  u8 eos_ready;
  u8 resources_acquired;
  struct Command* current_command;
  struct DecOutput buffer_status;
  struct DecClientHandle client;
  struct DecSequenceInfo sequence_info;
  struct DWL *dwl;
  const void* dwl_inst;
  u8 pending_eos;
  u32 max_num_of_decoded_pics;
  u32 num_of_decoded_pics;
  /* this is a temporary handler for stream decoded callback
   * until HEVC gets delayed sync implementation */
  void (*stream_decoded)(void* inst);
  u8 picture_in_display;
  struct DecInitConfig init_config;
  struct DecConfig config;

  struct LowLatencyDsc_t *low_latency_dsc;
  sem_t flush_sem;

  pthread_mutex_t pic_flushed_mutex; /* used to sequence changed */
  /* sync up decode thread and output thread */
  u8 has_dec_flush;
  pthread_cond_t pic_flushed_cond;
  sem_t decoded_pics_sem;
  volatile u8 is_end_of_stream;
#ifdef SEEK_TEST
  sem_t abort_sem;
  u32 seek_over_flag;
#endif
} DecoderInstance;

/* Decode loop and handlers for different states. */
static void* DecodeLoop(void* arg);
static void Initialize(DecoderInstance* inst);
static void WaitForResources(DecoderInstance* inst);
static void Decode(DecoderInstance* inst);
static void EndOfStream(DecoderInstance* inst);
static void Release(DecoderInstance* inst);
static void DecFatalError(DecInst dec_inst);

/* Output loop. */
static void* OutputLoop(void* arg);

/* Local helpers to manage and protect the state of the decoder. */
static enum DecodingState GetState(DecoderInstance* inst);
static void SetState(DecoderInstance* inst, enum DecodingState state);
static void StreamDecoded(void* dec_inst);


struct DecSwHwBuild DecGetBuild(const void *dwl_inst) {
  return VCDecGetBuild(dwl_inst, DWL_CLIENT_TYPE_HEVC_DEC);
}

struct DecApiVersion DecGetAPIVersion(void) {
  return VCDecGetAPIVersion();
}


enum DecRet DecInit(enum DecCodec codec, DecInst* decoder, struct DecInitParams *init_params,
                    struct DecClientHandle* client) {
  struct DecInitConfig* init_config = &init_params->init_config;
  struct DecConfig* config = &init_params->config;

  if (decoder == NULL || client->Initialized == NULL ||
      client->HeadersDecoded == NULL || client->BufferDecoded == NULL ||
      client->PictureReady == NULL || client->EndOfStream == NULL ||
      client->Released == NULL || client->NotifyError == NULL) {
    return DEC_PARAM_ERROR;
  }

  DecoderInstance* inst = dwl.Calloc(1, sizeof(DecoderInstance));
  if (inst == NULL) return DEC_MEMFAIL;
  inst->dwl = &dwl;
  inst->dwl_inst = init_config->dwl_inst;
  /* marco ASIC_TRACE_SUPPORT is used to generate low latency trace log for
     HW emulator and send data thread will not be called in this mode.*/
#ifndef ASIC_TRACE_SUPPORT
  if (init_config->decoder_mode & DEC_LOW_LATENCY) {
    inst->low_latency_dsc = dwl.Calloc(1, sizeof(struct LowLatencyDsc_t));
    if (inst->low_latency_dsc == NULL) return DEC_MEMFAIL;
  }
#endif

  if (FifoInit(MAX_FIFO_CAPACITY, &inst->input_queue) != FIFO_OK) {
    inst->dwl->Free(inst);
    return DEC_MEMFAIL;
  }
  inst->client = *client;
  inst->dwl->Pthread_mutex_init(&inst->cs_mutex, NULL);
  inst->dwl->Pthread_mutex_init(&inst->resource_mutex, NULL);
  inst->dwl->Pthread_cond_init(&inst->resource_cond, NULL);
  inst->dwl->Pthread_mutex_init(&inst->eos_mutex, NULL);
  inst->dwl->Pthread_cond_init(&inst->eos_cond, NULL);
  inst->eos_ready = 0;
  inst->resources_acquired = 0;
  sem_init(&inst->flush_sem, 0, 0);
  sem_init(&inst->decoded_pics_sem, 0, 0);
  inst->dwl->Pthread_mutex_init(&inst->pic_flushed_mutex, NULL);
  inst->dwl->Pthread_cond_init(&inst->pic_flushed_cond, NULL);
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  inst->dwl->Pthread_create(&inst->decode_thread, &attr, DecodeLoop, inst);
  if(inst->low_latency_dsc) {
    struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
    pLL->send_strm_info.strm_bus_addr = pLL->send_strm_info.strm_bus_start_addr = 0;
    pLL->send_strm_info.strm_vir_addr = pLL->send_strm_info.strm_vir_start_addr = NULL;
    pLL->send_strm_info.low_latency = 1;
    pLL->send_strm_info.send_len = 0;
    pLL->frame_end = 1;
    pLL->frame_start = 0;
    pLL->task = run_task(send_bytestrm_task, inst);
  }
#ifdef SEEK_TEST
  sem_init(&inst->abort_sem, 0, 0);
#endif

  SetState(inst, DECODER_WAITING_HEADERS);
  *decoder = inst;
  inst->init_config = *init_config;
  inst->config = *config;
  inst->max_num_of_decoded_pics = init_params->max_num_pics_to_decode;
  struct Command* command = inst->dwl->Calloc(1, sizeof(struct Command));
  command->id = COMMAND_INIT;
  command->params.config = *config;
  FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
  pthread_attr_destroy(&attr);
  return DEC_OK;
}

enum DecRet DecDestroy(DecInst dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  struct Command* tmp_cmd;

  if (!inst) return DEC_PARAM_ERROR;
  /* destroy is allowed only in TERMINATED state. */
  if (GetState(inst) != DECODER_TERMINATED)
    return DEC_PARAM_ERROR;

  /* Abort the current command (it may be long-lasting task). */

  /* Before FifoRelease, release the commands queued in fifo firstly. */
  do {
    tmp_cmd = NULL;
    FifoPop(inst->input_queue, (void**)&tmp_cmd,
          FIFO_EXCEPTION_ENABLE);
    if(tmp_cmd)
      inst->dwl->Free(tmp_cmd);
  }while(tmp_cmd);

  FifoRelease(inst->input_queue);

  if (inst->low_latency_dsc) {
    inst->dwl->Free(inst->low_latency_dsc);
  }
  sem_destroy(&inst->flush_sem);
  sem_destroy(&inst->decoded_pics_sem);
  inst->dwl->Pthread_mutex_destroy(&inst->cs_mutex);
  inst->dwl->Pthread_mutex_destroy(&inst->resource_mutex);
  inst->dwl->Pthread_mutex_destroy(&inst->eos_mutex);
  inst->dwl->Pthread_cond_destroy(&inst->resource_cond);
  inst->dwl->Pthread_cond_destroy(&inst->eos_cond);
  inst->dwl->Pthread_mutex_destroy(&inst->pic_flushed_mutex);
  inst->dwl->Pthread_cond_destroy(&inst->pic_flushed_cond);

  inst->dwl->Free(inst);
  return DEC_OK;
}

enum DecRet DecDecode(DecInst dec_inst, struct DecInput* input) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  if (dec_inst == NULL || input == NULL || input->data_len == 0 ||
      input->buffer.virtual_address == NULL || input->buffer.bus_address == 0) {
    return DEC_PARAM_ERROR;
  }

  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN: {
    struct Command* command = inst->dwl->Calloc(1, sizeof(struct Command));
    command->id = COMMAND_DECODE;
    inst->dwl->Memcpy(&command->params.config, &(inst->config), sizeof(struct DecConfig));
    inst->dwl->Memcpy(&command->params.input, input, sizeof(struct DecInput));
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  }
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    /* In this state, no more decoding but will still output pictures. */
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_NOT_INITIALIZED;
  }
}

enum DecRet DecGetPictureBuffersInfo(DecInst dec_inst, struct DecBufferInfo *info) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if (dec_inst == NULL || info == NULL) {
    return DEC_PARAM_ERROR;
  }

  if(GetState(inst) == DECODER_TERMINATED) {
    return DEC_PARAM_ERROR;
  }
  return (VCDecGetBufferInfo(inst->dec, info));
}

enum DecRet DecSetPictureBuffers(DecInst dec_inst,
                                 const struct DWLLinearMem* buffers,
                                 u32 num_of_buffers) {
  enum DecRet ret = DEC_OK;
  /* TODO(vmr): Enable checks once we have implementation in place. */
  /* if (dec_inst == NULL || buffers == NULL || num_of_buffers == 0)
  {
      return DEC_PARAM_ERROR;
  } */
  if (dec_inst == NULL || buffers == NULL || num_of_buffers == 0) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if(GetState(inst) == DECODER_TERMINATED) {
    return DEC_PARAM_ERROR;
  }

  /* TODO(vmr): Check the buffers and set them, if they're good. */
  inst->dwl->Pthread_mutex_lock(&inst->resource_mutex);
  for (int i = 0; i < num_of_buffers; i++) {
    ret = VCDecAddBuffer(inst->dec, (struct DWLLinearMem *)&buffers[i]);

    /* TODO(min): Check return code ... */
  }
  inst->resources_acquired = 1;
  SetState(inst, DECODER_DECODING);
  inst->dwl->Pthread_cond_signal(&inst->resource_cond);
  inst->dwl->Pthread_mutex_unlock(&inst->resource_mutex);
  return ret; //DEC_OK;
}

enum DecRet DecUseExtraFrmBuffers(DecInst dec_inst, u32 n) {
  enum DecRet ret = DEC_OK;

  if (dec_inst == NULL)
    return DEC_PARAM_ERROR;

  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if(GetState(inst) == DECODER_TERMINATED) {
    return DEC_PARAM_ERROR;
  }

  VCDecUseExtraFrmBuffers(inst->dec, n);
  return ret;
}

enum DecRet DecPictureConsumed(DecInst dec_inst, struct DecPictures* picture) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    VCDecPictureConsumed(inst->dec, picture);
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_NOT_INITIALIZED;
  }
  return DEC_OK;
}

enum DecRet DecEndOfStream(DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  struct Command* command;

  if(!inst->is_end_of_stream) {
    inst->is_end_of_stream = 1;
    sem_post(&inst->decoded_pics_sem);
  }

  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    command = inst->dwl->Calloc(1, sizeof(struct Command));
    inst->dwl->Memset(command, 0, sizeof(struct Command));
    command->id = COMMAND_END_OF_STREAM;
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_INITFAIL;
  }
}

enum DecRet DecFlush(DecInst dec_inst) {
  if (dec_inst == NULL) {
    return DEC_PARAM_ERROR;
  }
  DecoderInstance* inst = (DecoderInstance*)dec_inst;
  struct Command* command;
  switch (GetState(inst)) {
  case DECODER_WAITING_HEADERS:
  case DECODER_WAITING_RESOURCES:
  case DECODER_DECODING:
  case DECODER_SHUTTING_DOWN:
  case DECODER_ERROR_TO_SHUTTING_DOWN:
    sem_post(&inst->flush_sem);
    command = inst->dwl->Calloc(1, sizeof(struct Command));
    inst->dwl->Memset(command, 0, sizeof(struct Command));
    command->id = COMMAND_FLUSH;
    FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
    return DEC_OK;
  case DECODER_TERMINATED:
    return DEC_PARAM_ERROR;
  default:
    return DEC_INITFAIL;
  }
}

void DecRelease(DecInst dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  if(!inst->is_end_of_stream) {
    inst->is_end_of_stream = 1;
    sem_post(&inst->decoded_pics_sem);
  }

  /* If we are already terminated, do nothing. */
  if (GetState(inst) == DECODER_TERMINATED) return;
  /* If we are already shutting down, no need to do it more than once. */
  if (GetState(inst) == DECODER_SHUTTING_DOWN) return;
  /* Abort the current command (it may be long-lasting task). */
  if (GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN)
    SetState(inst, DECODER_SHUTTING_DOWN);
  struct Command* command = inst->dwl->Calloc(1, sizeof(struct Command));
  inst->dwl->Memset(command, 0, sizeof(struct Command));
  command->id = COMMAND_RELEASE;
  FifoPush(inst->input_queue, command, FIFO_EXCEPTION_DISABLE);
}

static void NextCommand(DecoderInstance* inst) {
  /* Read next command from stream, if we've finished the previous. */
  if (inst->current_command == NULL) {
    FifoPop(inst->input_queue, (void**)&inst->current_command,
            FIFO_EXCEPTION_DISABLE);
    if (inst->current_command != NULL &&
        inst->current_command->id == COMMAND_DECODE) {
      if(inst->low_latency_dsc) {
        struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
        /*allocate lowlatency buffer as a copy of stream buffer*/
        pLL->ll_buffer.mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
        pLL->ll_buffer.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
        if (!pLL->ll_buffer.virtual_address) {
          if (DWLMallocLinear(inst->dwl_inst, inst->current_command->params.input.buffer.size,
                              &pLL->ll_buffer)) {
            printf("No memory available for the lowlatency stream buffer\n");
            return;
          }
        }
        else if (pLL->ll_buffer.size < inst->current_command->params.input.buffer.size) {
          DWLFreeLinear(inst->dwl_inst,&pLL->ll_buffer);
          if(DWLMallocLinear(inst->dwl_inst, inst->current_command->params.input.buffer.size,
                            &pLL->ll_buffer)) {
            printf("No memory available for the lowlatency stream buffer\n");
            return;
          }
        }
        DWLLinearMemset(inst->dwl_inst, &pLL->ll_buffer, 0, 0, pLL->ll_buffer.size);
        inst->buffer_status.strm_curr_pos =(u8 *)pLL->ll_buffer.virtual_address  +
                                           ((u8*)inst->current_command->params.input.stream[0]
                                          - (u8*)inst->current_command->params.input.buffer.virtual_address);
        inst->buffer_status.strm_curr_bus_address = pLL->ll_buffer.bus_address  +
                                           ((u8*)inst->current_command->params.input.stream[0]
                                          - (u8*)inst->current_command->params.input.buffer.virtual_address);
        inst->buffer_status.data_left = inst->current_command->params.input.data_len;
        inst->buffer_status.strm_buff = (u8 *)pLL->ll_buffer.virtual_address;
        inst->buffer_status.strm_buff_bus_address = pLL->ll_buffer.bus_address;
        inst->buffer_status.buff_size = pLL->ll_buffer.size;
        pLL->input_strm_curr_pos = (u8*)inst->current_command->params.input.stream[0];
        pLL->input_strm_buff_start = (u8*)inst->current_command->params.input.buffer.virtual_address;
        inst->buffer_status.sei_buffer =
          inst->current_command->params.input.sei_buffer; /* SEI buffer */
      } else {
        inst->buffer_status.strm_curr_pos =
          (u8*)inst->current_command->params.input.stream[0];
        inst->buffer_status.strm_curr_bus_address =
          inst->current_command->params.input.buffer.bus_address;
        inst->buffer_status.data_left =
          inst->current_command->params.input.data_len;
        inst->buffer_status.strm_buff =
          (u8*)inst->current_command->params.input.buffer.virtual_address;
        inst->buffer_status.strm_buff_bus_address =
          inst->current_command->params.input.buffer.bus_address;
        inst->buffer_status.buff_size =
          inst->current_command->params.input.buffer.size;
        inst->buffer_status.sei_buffer =
          inst->current_command->params.input.sei_buffer; /* SEI buffer */
      }
    }
  }
}

static void CommandCompleted(DecoderInstance* inst) {
  if(inst->current_command == NULL)
    return;

  if (inst->current_command->id == COMMAND_DECODE &&
      GetState(inst) != DECODER_SHUTTING_DOWN &&
      GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN &&
      inst->init_config.mc_cfg.stream_consumed_callback == NULL) {
    /* When xxxSetInfo() returns error, decoder transits to SHUTTING_DOWN from
       WAITING_HEADERS directly, but there are still some input buffers to be
       consumed. For these buffers, stream_decoded() won't be called. Othewise
       more buffers will be fed in. */
    StreamDecoded(inst);
  }
  inst->dwl->Free(inst->current_command);
  inst->current_command = NULL;
}

static void* DecodeLoop(void* arg) {
  DecoderInstance* inst = (DecoderInstance*)arg;
  int in_flush = 0;
  while (1) {
    NextCommand(inst);
    if (inst->current_command == NULL) /* NULL command means to quit. */
      return NULL;
    /* flush process*/
    if (in_flush == 0) {
      in_flush = (sem_trywait(&inst->flush_sem) == 0) ? 1 : 0;
    }
    if (in_flush) {
        /* only flush the enqueued DECODE commands before FLUSH invoked */
      if (inst->current_command->id == COMMAND_DECODE) {
        CommandCompleted(inst);
        continue;
      }
      if (inst->current_command->id == COMMAND_FLUSH) {
        CommandCompleted(inst);
        in_flush = 0;
        continue;
      }
    }

    /* command process*/
    switch (inst->current_command->id) {
      case COMMAND_INIT:
        Initialize(inst);
        break;
      case COMMAND_DECODE:
        Decode(inst);
        break;
      case COMMAND_END_OF_STREAM:
        EndOfStream(inst);
        break;
      case COMMAND_RELEASE:
        Release(inst);
        return NULL;
      case COMMAND_SETBUFFERS:
      default:
        CommandCompleted(inst);
        break;
    }
  }
  return NULL;
}

static void Initialize(DecoderInstance* inst) {
  enum DecRet rv;

  switch (GetState(inst)) {
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
      rv = VCDecInit((const void**)&inst->dec, &inst->init_config);
      if (rv == DEC_OK)
        inst->client.Initialized(inst->client.client);
      else
        inst->client.NotifyError(inst->client.client, 0, rv);
      inst->dwl->Pthread_create(&inst->output_thread, NULL, OutputLoop, inst);
      if (inst->low_latency_dsc)
        inst->low_latency_dsc->exit_send_thread = 0;
#ifdef ASIC_TRACE_SUPPORT
      if (inst->init_config.mc_cfg.mc_enable == 1) {
        inst->init_config.mc_cfg.mc_enable = 0;
        inst->init_config.mc_cfg.stream_consumed_callback = NULL;
      }
#endif
      break;
    default:
      /* do nothing */
      break;
  }
  CommandCompleted(inst);
}

static void WaitForResources(DecoderInstance* inst) {
  inst->dwl->Pthread_mutex_lock(&inst->resource_mutex);
  while (!inst->resources_acquired)
    inst->dwl->Pthread_cond_wait(&inst->resource_cond, &inst->resource_mutex);
  SetState(inst, DECODER_DECODING);
  inst->dwl->Pthread_mutex_unlock(&inst->resource_mutex);
}

#ifdef SEEK_TEST
u32 seek_end = 0xFF;
enum DecRet SeekFlush(DecoderInstance* inst) {
  enum DecRet rv = DEC_OK;

  rv = VCDecAbort(inst->dec);
  // wait no any output
  sem_wait(&inst->abort_sem);
  // discard the remain input buffer
  SetState(inst, DECODER_SHUTTING_DOWN);
  struct DecInput input_temp;
  if (inst->current_command)
    input_temp = inst->current_command->params.input;
  while(1) {
    if(!FifoCount(inst->input_queue)) //shouldn't call the next command when no object queue.
      break;

    NextCommand(inst);
    if(inst->current_command == NULL)
      break;
    CommandCompleted(inst);
  }
  if (!inst->current_command) {
    inst->current_command = inst->dwl->Calloc(1, sizeof(struct Command));
    inst->dwl->Memcpy(&inst->current_command->params.config, &(inst->config), sizeof(struct DecConfig));
    inst->dwl->Memcpy(&inst->current_command->params.input, &input_temp, sizeof(struct DecInput));
  }
  rv = VCDecAbortAfter(inst->dec);
  SetState(inst, DECODER_DECODING);
  // prepare the next key frame input
  seek_end = 0;
  StreamDecoded(inst);
  if (inst->current_command) {
    inst->dwl->Free(inst->current_command);
    inst->current_command = NULL;
  }

  seek_end = 1;
  inst->num_of_decoded_pics = 0;

  return rv;
}
#endif

static void Decode(DecoderInstance* inst) {
  enum DecRet rv = DEC_OK;
  struct DWLLinearMem *buffer = NULL;
  struct DecInputParameters input_param;
  DWLmemset(&input_param, 0, sizeof(struct DecInputParameters));
  buffer = &input_param.stream_buffer;

  switch (GetState(inst)) {
    case DECODER_WAITING_RESOURCES:
      /* when WAITING_RESOURCES, wait for resources firstly, then do "real" decoding */
      WaitForResources(inst);
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
      if(inst->low_latency_dsc) {
        struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
	/* Wait send bytes thread rdy. */
        while (!pLL->frame_end)
          sched_yield();
        pLL->frame_end = 0;
        pLL->send_strm_info.strm_buff = (addr_t)inst->buffer_status.strm_buff;
        pLL->send_strm_info.strm_buff_bus_address = (addr_t)inst->buffer_status.strm_buff_bus_address;
        pLL->send_strm_info.buff_size = inst->buffer_status.buff_size;
        pLL->send_strm_info.strm_vir_addr = pLL->send_strm_info.strm_vir_start_addr = inst->buffer_status.strm_curr_pos;
        pLL->send_strm_info.strm_bus_addr = pLL->send_strm_info.strm_bus_start_addr = inst->buffer_status.strm_buff_bus_address +
                                       ((addr_t)inst->buffer_status.strm_curr_pos - (addr_t)inst->buffer_status.strm_buff);
        pLL->frame_len = inst->buffer_status.data_left;
        pLL->send_len = 0;
        pLL->send_strm_info.send_len = 0;
        if (inst->max_num_of_decoded_pics > 0 &&
          inst->num_of_decoded_pics >= inst->max_num_of_decoded_pics) {
          pLL->process_end_flag = 1;
          pLL->pic_decoded = 1;
        }
        pLL->frame_start = 1;
      }
      do {
        /* Skip decoding if we've decoded as many pics requested by the user. */
        if (inst->max_num_of_decoded_pics > 0 &&
            inst->num_of_decoded_pics >= inst->max_num_of_decoded_pics) {
          if (inst->low_latency_dsc)
            inst->low_latency_dsc->frame_end = 1;
#ifdef SEEK_TEST
          if (!inst->seek_over_flag) {
            rv = SeekFlush(inst);
            inst->seek_over_flag = 1;
          }
          else
#endif
          EndOfStream(inst);
          break;
        }

        if(inst->low_latency_dsc) {
          struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
          while (!pLL->send_len || !pLL->send_strm_info.send_len)
            sched_yield();
          buffer->virtual_address = (u32*)inst->buffer_status.strm_buff;
          buffer->bus_address = inst->buffer_status.strm_buff_bus_address;
          buffer->size = inst->buffer_status.buff_size;
          input_param.sei_buffer = inst->buffer_status.sei_buffer;
          input_param.p_user_data = (void *)inst->client.client;
          input_param.stream = inst->buffer_status.strm_curr_pos;
          input_param.strm_len = pLL->send_strm_info.send_len;
          input_param.strm_vir_start_addr = pLL->send_strm_info.strm_vir_start_addr;
          input_param.low_latency = 1;
          input_param.frame_len = pLL->frame_len;
          input_param.pic_decoded = pLL->pic_decoded;
          input_param.exit_send_thread = pLL->exit_send_thread;
        } else {
          buffer->virtual_address = (u32*)inst->buffer_status.strm_buff;
          buffer->bus_address = inst->buffer_status.strm_buff_bus_address;
          buffer->size = inst->buffer_status.buff_size;
          input_param.sei_buffer = inst->buffer_status.sei_buffer;
          input_param.p_user_data = (void *)inst->client.client;
          input_param.stream = inst->buffer_status.strm_curr_pos;
          input_param.strm_len = inst->buffer_status.data_left;
          input_param.frame_len = inst->buffer_status.data_left;
        }
        input_param.pic_id = inst->num_of_decoded_pics + 1;
        inst->current_command->params.input.pic_id = input_param.pic_id;
        input_param.skip_frame = inst->init_config.skip_frame;

        rv = VCDecDecode(inst->dec, &inst->buffer_status, &input_param);
        if(inst->low_latency_dsc) {
          struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
          pLL->frame_len = input_param.frame_len;
          pLL->pic_decoded = input_param.pic_decoded;
          pLL->exit_send_thread = input_param.exit_send_thread;
        }

        inst->current_command->params.input.dec_ret = rv;
        if (GetState(inst) == DECODER_SHUTTING_DOWN) {
          break;
        }

        if (rv == DEC_HDRS_RDY) {
          enum DecRet rv_info;
          struct DecConfig *config = &inst->current_command->params.config;
          u8 i;

          pthread_mutex_lock(&inst->pic_flushed_mutex);
          while (!inst->has_dec_flush)
            inst->dwl->Pthread_cond_wait(&inst->pic_flushed_cond, &inst->pic_flushed_mutex);
          inst->has_dec_flush = 0;
          pthread_mutex_unlock(&inst->pic_flushed_mutex);

          VCDecGetInfo(inst->dec, &inst->sequence_info);
          if (inst->sequence_info.h264_base_mode == 1) {
            inst->init_config.mc_cfg.mc_enable = 0;
            inst->init_config.mc_cfg.stream_consumed_callback = NULL;
          }
          if(inst->sequence_info.out_bit_depth > 0 &&
            inst->sequence_info.out_bit_depth != inst->sequence_info.bit_depth_luma &&
            inst->sequence_info.out_bit_depth == 8){
            printf("Note TB: The current frame is cast to 8bit output.\n");
            //int i;
            //struct DecConfig *config = &inst->current_command->params.config;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++)
              config->ppu_cfg[i].out_cut_8bits = 1;
          }
          //struct DecConfig *config = &inst->current_command->params.config;
          /* range mapping */
          for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
            if (!config->ppu_cfg[i].enabled) continue;
            config->ppu_cfg[i].video_range = inst->sequence_info.video_range;
          } // default video_range is 1 (full), should be overwritten by stream information (get from sps)

          /* Ajust user cropping params based on cropping params from sequence info. */
          if (inst->sequence_info.crop_params.crop_left_offset != 0 ||
              inst->sequence_info.crop_params.crop_top_offset != 0 ||
              (inst->sequence_info.crop_params.crop_out_width != inst->sequence_info.pic_width &&
               inst->sequence_info.crop_params.crop_out_width != 0) ||
              (inst->sequence_info.crop_params.crop_out_height != inst->sequence_info.pic_height &&
               inst->sequence_info.crop_params.crop_out_height != 0)) {
            //int i;
            //struct DecConfig *config = &inst->current_command->params.config;
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
              if (!config->ppu_cfg[i].enabled) continue;

              if (!config->ppu_cfg[i].crop.enabled && !inst->sequence_info.dis_comformance_window) {
                config->ppu_cfg[i].crop.x = inst->sequence_info.crop_params.crop_left_offset;
                config->ppu_cfg[i].crop.y = inst->sequence_info.crop_params.crop_top_offset;
                /*support odd crop*/
                // config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
                // config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
                config->ppu_cfg[i].crop.width = inst->sequence_info.crop_params.crop_out_width;
                config->ppu_cfg[i].crop.height = inst->sequence_info.crop_params.crop_out_height;
              } else if (config->ppu_cfg[i].crop.enabled && !inst->sequence_info.dis_comformance_window){
                config->ppu_cfg[i].crop.x += inst->sequence_info.crop_params.crop_left_offset;
                config->ppu_cfg[i].crop.y += inst->sequence_info.crop_params.crop_top_offset;
                /*support odd crop*/
                // if(!config->ppu_cfg[i].crop.width)
                //   config->ppu_cfg[i].crop.width = (inst->sequence_info.crop_params.crop_out_width+1) & ~0x1;
                // if(!config->ppu_cfg[i].crop.height)
                //   config->ppu_cfg[i].crop.height = (inst->sequence_info.crop_params.crop_out_height+1) & ~0x1;
                if(!config->ppu_cfg[i].crop.width)
                  config->ppu_cfg[i].crop.width = inst->sequence_info.crop_params.crop_out_width;
                if(!config->ppu_cfg[i].crop.height)
                  config->ppu_cfg[i].crop.height = inst->sequence_info.crop_params.crop_out_height;
              }
              config->ppu_cfg[i].enabled = 1;
              config->ppu_cfg[i].crop.enabled = 1;
            }
          }

          inst->client.HeadersDecoded(inst->client.client, inst->sequence_info, inst->current_command->params.config.ppu_cfg);

          rv_info = VCDecSetInfo(inst->dec, &inst->current_command->params.config);
          if (rv_info != DEC_OK) {
            if (inst->low_latency_dsc)
              inst->low_latency_dsc->frame_end = 1;
            SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
            DecFatalError(inst);
            inst->client.NotifyError(inst->client.client, 0, rv_info);
            break;
          }
          if (inst->buffer_status.data_left == 0) break;
        }
        else if (rv == DEC_WAITING_FOR_BUFFER) { /* Allocate buffers externally. */
          while (inst->picture_in_display)
          usleep(10);
          VCDecGetInfo(inst->dec, &inst->sequence_info);
          SetState(inst, DECODER_WAITING_RESOURCES);
          if (inst->client.ExtBufferReq(inst->client.client)) {
            inst->client.NotifyError(inst->client.client, 0, DEC_MEMFAIL);
            inst->buffer_status.data_left = 0;
            if (inst->low_latency_dsc) {
              inst->low_latency_dsc->exit_send_thread = 1;
              inst->low_latency_dsc->pic_decoded = 1;
            }
            SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
            DecFatalError(inst);
          }
          //if (inst->buffer_status.data_left == 0) break;
        }
        else if (rv == DEC_NO_DECODING_BUFFER) {
          /* NO DECODING BUFFER */
          usleep(10);
          continue;
        /* ASO/FMO detected and not supported in multicore mode */
        } else if (rv == DEC_ADVANCED_TOOLS && inst->init_config.mc_cfg.mc_enable == 1) {
          inst->client.NotifyError(inst->client.client, 0, rv);
          DecRelease(inst);
          break;
        } else if (rv < 0) { /* Error */
          if (rv == DEC_INFOPARAM_ERROR || rv == DEC_STREAM_NOT_SUPPORTED ||
              rv == DEC_MEMFAIL) {
            if (inst->low_latency_dsc)
              inst->low_latency_dsc->frame_end = 1;
            SetState(inst, DECODER_ERROR_TO_SHUTTING_DOWN);
            DecFatalError(inst);
          }
          if (rv == DEC_STRM_ERROR ||
              rv == DEC_STREAM_ERROR_DEDECTED) {
            if (inst->low_latency_dsc)
              inst->low_latency_dsc->frame_end = 1;
          }
          inst->client.NotifyError(inst->client.client, input_param.pic_id, rv);
          break; /* Give up on the input buffer. */
        }
        if (rv == DEC_PIC_DECODED) {
          inst->num_of_decoded_pics++;
          sem_post(&inst->decoded_pics_sem);
        }
      } while (inst->buffer_status.data_left > 0 &&
               GetState(inst) != DECODER_SHUTTING_DOWN &&
               GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN);
      break;

    case DECODER_SHUTTING_DOWN:
    case DECODER_ERROR_TO_SHUTTING_DOWN:
    default:
      /* do nothing */
      if (inst->low_latency_dsc)
        inst->low_latency_dsc->frame_end = 1;
      break;
  }
  CommandCompleted(inst);
}

static void EndOfStream(DecoderInstance* inst) {
  switch (GetState(inst)) {
    case DECODER_WAITING_HEADERS:
    case DECODER_DECODING:
    case DECODER_WAITING_RESOURCES:
    case DECODER_ERROR_TO_SHUTTING_DOWN:
      VCDecEndOfStream(inst->dec);

      if(!inst->is_end_of_stream) {
        inst->is_end_of_stream = 1;
        sem_post(&inst->decoded_pics_sem);
      }

      inst->dwl->Pthread_mutex_lock(&inst->eos_mutex);
      while (!inst->eos_ready)
        inst->dwl->Pthread_cond_wait(&inst->eos_cond, &inst->eos_mutex);
      inst->dwl->Pthread_mutex_unlock(&inst->eos_mutex);

      inst->pending_eos = 1;
      if (inst->current_command->id == COMMAND_END_OF_STREAM)
        CommandCompleted(inst);
      inst->client.EndOfStream(inst->client.client);
      break;

    case DECODER_SHUTTING_DOWN:
    default:
      if (inst->current_command->id == COMMAND_END_OF_STREAM)
        CommandCompleted(inst);
      break;
  }
}

static void Release(DecoderInstance* inst) {
  struct DecClientHandle client = inst->client;

  if (!inst->pending_eos) {
    VCDecEndOfStream(inst->dec); /* In case user didn't. */
  }
  if(inst->low_latency_dsc) {
    struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
    pLL->pic_decoded = 1;
    pLL->process_end_flag = 1;
    pLL->frame_start = 1;
    wait_for_task_completion(pLL->task);
    DWLFreeLinear(inst->dwl_inst,&pLL->ll_buffer);
  }
  /* Release the resource condition, if decoder is waiting for resources. */
  inst->dwl->Pthread_mutex_lock(&inst->resource_mutex);
  inst->dwl->Pthread_cond_signal(&inst->resource_cond);
  inst->dwl->Pthread_mutex_unlock(&inst->resource_mutex);
  inst->dwl->Pthread_join(inst->output_thread, NULL);
  SetState(inst, DECODER_TERMINATED);
  VCDecRelease(inst->dec);

  CommandCompleted(inst);
  client.Released(client.client);
}

static void DecFatalError(DecInst dec_inst)
{
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  CommandCompleted(inst); //Free the command with exception
  while(1) {
    if(!FifoCount(inst->input_queue)) //shouldn't call the next command when no object queue.
      break;

    NextCommand(inst);
    if(inst->current_command == NULL)
      break;
    CommandCompleted(inst);
  }
  DecRelease(inst);
}

static void* OutputLoop(void* arg) {
  DecoderInstance* inst = (DecoderInstance*)arg;
  struct DecPictures pic;
  enum DecRet rv;
  u32 no_pic_rdy = 0;
  inst->dwl->Memset(&pic, 0, sizeof(pic));
  while (1) {
    no_pic_rdy = 1;
    switch (GetState(inst)) {
      case DECODER_WAITING_RESOURCES:
        inst->dwl->Pthread_mutex_lock(&inst->resource_mutex);
        while (!inst->resources_acquired &&
               GetState(inst) != DECODER_SHUTTING_DOWN &&
               GetState(inst) != DECODER_ERROR_TO_SHUTTING_DOWN)
          inst->dwl->Pthread_cond_wait(&inst->resource_cond,
                                      &inst->resource_mutex);
        inst->dwl->Pthread_mutex_unlock(&inst->resource_mutex);
        break;
      case DECODER_WAITING_HEADERS:
      case DECODER_DECODING:
      /* flush pending output buffer in DECODER_ERROR_TO_SHUTTING_DOWN state */
      case DECODER_ERROR_TO_SHUTTING_DOWN:
      case DECODER_SHUTTING_DOWN:
        while ((rv = VCDecNextPicture(inst->dec, &pic)) ==
               DEC_PIC_RDY) {
          inst->picture_in_display = 1;
          inst->client.PictureReady(inst->client.client, pic);
          inst->picture_in_display = 0;
        }

        if(rv == DEC_FLUSHED) {
          pthread_mutex_lock(&inst->pic_flushed_mutex);
          inst->has_dec_flush = 1;
          inst->dwl->Pthread_cond_signal(&inst->pic_flushed_cond);
          pthread_mutex_unlock(&inst->pic_flushed_mutex);
          no_pic_rdy = 0;
        }

        if (rv == DEC_END_OF_STREAM) {
          inst->dwl->Pthread_mutex_lock(&inst->eos_mutex);
          inst->eos_ready = 1;
          inst->pending_eos = 0;
          inst->dwl->Pthread_cond_signal(&inst->eos_cond);
          inst->dwl->Pthread_mutex_unlock(&inst->eos_mutex);
          // inst->is_end_of_stream = 1;
          inst->dwl->Pthread_exit(0); // exit current thread
          return NULL;
        }
#ifdef SEEK_TEST
        if (rv == DEC_ABORTED) {
          sem_post(&inst->abort_sem);
        }
#endif
        if ((GetState(inst) == DECODER_SHUTTING_DOWN ||
             GetState(inst) == DECODER_ERROR_TO_SHUTTING_DOWN) &&
             rv == DEC_PARAM_ERROR)
          return NULL;
        break;
      default:
        break;
    }

    // no_pic_rdy: for resolution change
    // is_end_of_stream: num of enter output thread more than enter decoder thread
    if(!no_pic_rdy && !inst->is_end_of_stream) {
      sem_wait(&inst->decoded_pics_sem);
    } else
     sched_yield();
  }
  return NULL;
}

static enum DecodingState GetState(DecoderInstance* inst) {
  inst->dwl->Pthread_mutex_lock(&inst->cs_mutex);
  enum DecodingState state = inst->state;
  inst->dwl->Pthread_mutex_unlock(&inst->cs_mutex);
  return state;
}

static void SetState(DecoderInstance* inst, enum DecodingState state) {
  const char* states[] = {
    "DECODER_WAITING_HEADERS", "DECODER_WAITING_RESOURCES",
    "DECODER_DECODING",        "DECODER_SHUTTING_DOWN",
    "DECODER_ERROR_TO_SHUTTING_DOWN"
    , "DECODER_TERMINATED"
  };
  inst->dwl->Pthread_mutex_lock(&inst->cs_mutex);
  inst->dwl->Printf("Decoder state change: %s => %s\n", states[inst->state],
                   states[state]);
  inst->state = state;
  inst->dwl->Pthread_mutex_unlock(&inst->cs_mutex);
}

static void StreamDecoded(void* dec_inst) {
  DecoderInstance* inst = (DecoderInstance*)dec_inst;

  switch (inst->init_config.codec) {
    case DEC_VP9:
    case DEC_AV1:
    case DEC_HEVC:
    case DEC_H264:
    case DEC_AVS2:
    case DEC_VP8:
    default:
      inst->client.BufferDecoded(inst->client.client,
                                &inst->current_command->params.input);
      break;
    }
}


task_handle run_task(task_func func, void* param) {
  int ret;
  pthread_attr_t attr;
  struct sched_param par;
  pthread_t* thread_handle = malloc(sizeof(pthread_t));
  if (thread_handle == NULL)
  	return NULL;

  pthread_attr_init(&attr);
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  ASSERT(ret == 0);
  ret = pthread_attr_setschedpolicy(&attr, SCHED_RR);
  par.sched_priority = 60;
  ret = pthread_attr_setschedparam(&attr, &par);
  //ret = pthread_create(thread_handle, &attr, func, param);
  ret = pthread_create(thread_handle, NULL, func, param);
  ASSERT(ret == 0);

  if(ret != 0) {
    free(thread_handle);
    thread_handle = NULL;
  }
  (void)attr;
  (void)par;

  pthread_attr_destroy(&attr);

  return thread_handle;
}

void wait_for_task_completion(task_handle task) {
  int ret;
  ret = pthread_join(*((pthread_t*)task), NULL);
  ASSERT(ret == 0);
  (void) ret;
  free(task);
}

void SendBytesToDDR(u8* p, u32 n, u8* fp) {
  memcpy(p, fp, n);
}

void* send_bytestrm_task(void* param) {
  DecoderInstance* inst = (DecoderInstance*)param;

  u32 packet_size = LOW_LATENCY_PACKET_SIZE;
  u32 bytes = 0;
  u32 tmp_bytes = 0;
  u8 *ll_buf_curr_pos = NULL;
  u8 *fp = NULL;

  while (!inst->dec)
    sched_yield();

  struct LowLatencyDsc_t *pLL = inst->low_latency_dsc;
  while (!pLL->frame_start)
    sched_yield();
  pLL->frame_start = 0;

  /* When the decoder thread is released or all frame is decoded done, decoder loop will set process_end_flag as 1.*/
  while (!pLL->process_end_flag) {
    ll_buf_curr_pos = inst->buffer_status.strm_curr_pos;
    fp = pLL->input_strm_curr_pos;
    while (pLL->frame_len >= pLL->send_len && pLL->pic_decoded == 0) {
      /* exit_send_thread is only used here to bounce out send byte loop and need not be reset.
       * When the decoder thread is released, the sending byte thread automatically exits (process_end_flag is set to 1).*/
      if (ll_buf_curr_pos > inst->buffer_status.strm_buff + inst->buffer_status.buff_size ||
          GetState(inst) == DECODER_ERROR_TO_SHUTTING_DOWN || pLL->exit_send_thread) {
        break;
      }
      bytes = 0;
      if (pLL->frame_len - pLL->send_len > 0) {
        /* if length bigger then packet size bytes should be packet size. */
        if (pLL->frame_len - pLL->send_len > packet_size){
          pLL->send_strm_info.last_flag = 0;
          bytes = packet_size;
        } else {
          pLL->send_strm_info.last_flag = 1;
          bytes = pLL->frame_len - pLL->send_len;
        }
        /* detect if a ringbuffer is used */
        if (ll_buf_curr_pos + bytes > inst->buffer_status.strm_buff + inst->buffer_status.buff_size) {
          SendBytesToDDR(pLL->send_strm_info.strm_vir_addr, inst->buffer_status.strm_buff + inst->buffer_status.buff_size - ll_buf_curr_pos, fp);
          tmp_bytes = bytes - (inst->buffer_status.strm_buff + inst->buffer_status.buff_size - ll_buf_curr_pos);
          ll_buf_curr_pos = inst->buffer_status.strm_buff;
          fp = pLL->input_strm_buff_start;
          pLL->send_strm_info.strm_vir_addr = inst->buffer_status.strm_buff;
          pLL->send_strm_info.strm_bus_addr = inst->buffer_status.strm_buff_bus_address;
          SendBytesToDDR(pLL->send_strm_info.strm_vir_addr, tmp_bytes, fp);
          ll_buf_curr_pos += tmp_bytes;
          fp += tmp_bytes;
          pLL->send_strm_info.strm_bus_addr += tmp_bytes;
          pLL->send_strm_info.strm_vir_addr += tmp_bytes;
          pLL->send_strm_info.send_len += bytes;
        } else {
          SendBytesToDDR(pLL->send_strm_info.strm_vir_addr, bytes, fp);
          ll_buf_curr_pos += bytes;
          fp += bytes;
          pLL->send_strm_info.strm_bus_addr += bytes;
          pLL->send_strm_info.strm_vir_addr += bytes;
          pLL->send_strm_info.send_len += bytes;
        }
        DWLDMATransData(inst->dwl_inst, &pLL->ll_buffer, 0, pLL->ll_buffer.size, HOST_TO_DEVICE); //simply handle, dma all buffer size
      }
      usleep(500); /* delay 1 ms */
      /* if stream info do not update buf hw didn't work last time */
      VCDecUpdateStrmInfoCtrl(inst->dec, pLL->send_strm_info);
      pLL->send_len += bytes;
    }

    if(pLL->pic_decoded == 1) {
      /* reset flag & update variable */
      pLL->pic_decoded = 0;
      pLL->send_strm_info.last_flag = 0;
      pLL->send_strm_info.send_len = 0;
      /* do sync with main thread after one frame decoded */
      pLL->frame_end = 1;
      while (!pLL->frame_start)
        sched_yield();
      pLL->frame_start = 0;
    }
  }
  return NULL;
}
