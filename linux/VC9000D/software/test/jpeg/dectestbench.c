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

#include "decapicommon.h"
#include "vcdecapi.h"
#include "dec_log.h"
#ifdef MODEL_SIMULATION
  #ifdef ASIC_TRACE_SUPPORT
    #include "trace.h"
  #endif
  #include "asic.h"
#endif
#include "deccfg.h"
#include "tb_cfg.h"
#include "regdrv.h"
#include "common_sink.h"
#include "tb_sw_performance.h"
#include "command_line_parser.h"
#include "vcd_tools.h"
#include <ctype.h>
#include "math.h"
#ifndef MAX_PATH_
#define MAX_PATH   256  /* maximum lenght of the file path */
#endif
#define DEFAULT -1
#define JPEG_INPUT_BUFFER 0x5120
#define JPEG_INPUT_BUFFER_SIZE  (8192 * 8192)
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))
#define ALIGN(a) (1 << (a))
#define MAX_BUFFERS 4
#define PERFETCH_EOI_NUM 100
#define RI_MAX_NUM  128

#ifndef MIN
  /* macro to get smaller of two values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifdef ASIC_ONL_SIM
  u32 dec_done;
//  #else
//  sem_t dec_done;  /* Semaphore which is signalled when decoding is done. */
#endif

#ifdef _ASSERT_USED
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

/* SW/SW testing, read stream trace file */
FILE *f_stream_trace = NULL;

/* memory parameters */
static u32 out_pic_size_luma;
static u32 out_pic_size_chroma;
static struct DWLLinearMem output_address_y;
static struct DWLLinearMem output_address_cb_cr;
static u32 frame_ready = 0;
static u32 sliced_output_used = 0;
static u32 mode = 0;
static u32 ThumbDone = 0;
static u32 write_output = 1;
static u32 size_luma = 0;
static u32 size_chroma = 0;
static u32 pp_enabled = 1;
static u32 slice_to_user = 0;
static u32 slice_size = 0;
static u32 non_interleaved = 0;
static i32 full_slice_counter = -1;
static u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
static u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
/* Use independent chroma buffers (currently not supported). */
/* otherwise chroma buffers is allocated in luma buffers. */

static u32 md5sum = 0;
/* secure mode */
u32 secure_mode;
/* stream start address */
u8 *byte_strm_start;

/* user allocated output */
struct DWLLinearMem user_alloc_output_buffer[MAX_BUFFERS] = {{0}};

pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS] = {{0}};

/* progressive parameters */
static u32 scan_counter = 0;
static u32 progressive = 0;
static u32 scan_ready = 0;
static u32 nbr_of_images = 0;
static u32 nbr_of_thumb_images = 0;
static u32 nbr_of_images_total = 0;
static u32 nbr_of_thumb_images_total = 0;
static u32 nbr_of_images_to_out = 0;
static u32 nbr_of_thumb_images_to_out = 0;
static u32 jpeg_exif_in_stream = 0;
static u32 thumb_in_stream = 0;
static u32 next_soi = 0;
static u32 stream_info_check = 0;
static u32 prev_output_width = 0;
static u32 prev_output_height = 0;
static u32 prev_output_format = 0;
static u32 prev_output_width_tn = 0;
static u32 prev_output_height_tn = 0;
static u32 prev_output_format_tn = 0;
static u32 pic_counter = 0;
static u32 num_of_decoded_pics = 0;

/* thread for low latency feature */
typedef void* task_handle;
typedef void* (*task_func)(void*);

/* variables for low latency feature */

static volatile struct strmInfo send_strm_info;
const char* in_file_name;
u32 send_strm_len;
static u32 pic_decoded;
sem_t send_sem;
sem_t frame_sem;
u32 sw_hw_bound;
u32 bytes_go_back;
u32 tmp_len;
/* func for low latency feature */
static void wait_for_task_completion(task_handle task);
static task_handle run_task(task_func func, void* param);
static void* send_bytestrm_task(void* param);
u32 FindImageData(u8 * p_stream, u32 stream_length);

typedef void *JpegDecInst;
JpegDecInst jpeg;
YuvSink* yuvsink[2]; /* Yuvsink instance. */
static u32 process_end_flag;

/* prototypes */
static u32 allocMemory(JpegDecInst dec_inst, struct DecSequenceInfo * image_info,
                struct DecInputParameters* jpeg_in, const void *dwl, u64 buffer_size);
static u32 inputMemoryAlloc(JpegDecInst dec_inst, struct DecSequenceInfo * image_info,
                     struct DecInputParameters* jpeg_in, const void *dwl, struct DecBufferInfo *mem_info);
static void calcSize(struct DecSequenceInfo * image_info, u32 pic_mode, struct DecPictures* jpeg_out, u32 i);
static void WriteOutputLuma(u8 * data_luma, u32 pic_size_luma, u32 pic_mode);
static void WriteOutputChroma(u8 * data_chroma, u32 pic_size_chroma, u32 pic_mode);
static void WriteFullOutput(char** out_file_name, u32 pic_mode);

static void handleSlicedOutput(struct DecSequenceInfo * image_info, struct DecInputParameters* jpeg_in,
                        struct DecPictures* jpeg_out);

static void WriteCroppedOutput(struct DecSequenceInfo * info, u8 * data_luma, u8 * data_cb,
                        u8 * data_cr);

static void WriteProgressiveOutput(char** out_file_name, u32 size_luma, u32 size_chroma,
                                   u32 mode, u8 * data_luma, u8 * data_cb, u8 * data_cr);
static void *JpegDecMalloc(unsigned int size);
/* static void *JpegDecMemset(void *ptr, int c, unsigned int size); */
static void JpegDecFree(void *ptr);

static void PrintJpegRet(enum DecRet jpeg_ret);
static void PrintGetImageInfo(struct DecSequenceInfo * image_info);
static u32 FindImageInfoEnd(u8 * stream, u32 stream_length, u32 * p_offset,
                     u8 * buffer, u32 buf_len);
static u32 FindImageEnd(u8 * stream, u32 stream_length, u32 * p_offset,
                 u8 * buffer, u32 buf_len);
static u32 FindImageEOI(u8 * stream, u32 stream_length, u32 * p_offset,
                 u8 * buffer, u32 buf_len);
static u32 FindImageTnEOI(u8 * stream, u32 stream_length, u32 * p_offset,
                   u32 mode, u32 thumb_exist, u8 * buffer, u32 buf_len);

static u32 FindImageAllRI(u8 *img_buf, u32 img_len, u32 *ri_array, u32 ri_count);
static u32 GetBytes(u8 * stream, u32 idx, u8 *buffer, u32 buffer_length);
static void SetPpConfig(struct TestParams *params, struct DecSequenceInfo image_info,
                      struct DecConfig *config, u32 crop_step_rshift, u32 image_type);

enum SCALE_MODE {
  NON_SCALE,
  FIXED_DOWNSCALE,
  FLEXIBLE_SCALE
} scale_mode;

u32 planar_output = 0;
u32 only_full_resolution = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 128 bytes alignment */
u32 output_thread_run = 0;
u32 last_pic_flag = 0;
u32 crop = 0;
u32 mc_enable = 0;
/* restart interval based multicore decoding */
u32 ri_mc_enable = 0;
u32 ri_count = 0;
u32 *ri_array = NULL;
u32 first_open = 1;
pthread_t output_thread;
u32 add_external_buffer = 0;

#define MAX_STRM_BUFFERS    (MAX_ASIC_CORES + 1)

static struct DWLLinearMem stream_mem_input[MAX_STRM_BUFFERS];
static u32 stream_mem_status[MAX_STRM_BUFFERS];
static u32 frame_mem_status[MAX_BUFFERS];
u32 allocated_buffers = MAX_STRM_BUFFERS;
u32 output_buffers = MAX_BUFFERS;
u32 wr_id;

static sem_t stream_buff_free;
static sem_t frame_buff_free;
#ifdef _HAVE_PTHREAD_H
#ifdef SEM_REPLACE_MUTEX
static sem_t           strm_buff_stat_lock;
static sem_t           frm_buff_stat_lock;
#else
static pthread_mutex_t strm_buff_stat_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t frm_buff_stat_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
#else
static pthread_mutex_t strm_buff_stat_lock = {0, };
static pthread_mutex_t frm_buff_stat_lock = {0, };
#endif //_HAVE_PTHREAD_H
const void *dwl;

#ifdef ASIC_TRACE_SUPPORT
u32 pic_number;
#endif

struct TBCfg tb_cfg;

#ifdef JPEG_EVALUATION
extern u32 g_hw_ver;
#endif

/* These global values are found from commonconfig.c.
 * partial tb_cfg.dec_params parameters. */
extern struct DecParams dec_params;
struct TOOL_PARAMS tool_params;

#ifdef USE_RANDOM_ERROR_TEST
/* These global values are found from vcdecapi.c.
 * partial tb_cfg.tb_params parameters for random error injectionn. */
extern struct ErrorParams random_error_params;
#endif

typedef struct
{
  int argc;
  char **argv;
} MainArgs;

#ifdef __FREERTOS__
  #ifndef FREERTOS_SIMULATOR
  #include "user_freertos.h"
  #endif
  typedef void * RET_TYPE;
  static pthread_t tid_task;
#else
  typedef int RET_TYPE;
#endif

static RET_TYPE MainTask(void * args);

void StreamBufferConsumed(u8 *stream, void *p_user_data) {
  int idx;
  pthread_mutex_lock(&strm_buff_stat_lock);

  idx = 0;
  do {
    if ((u8*)stream >= (u8*)stream_mem_input[idx].virtual_address &&
        (u8*)stream < (u8*)stream_mem_input[idx].virtual_address + stream_mem_input[idx].size) {
      stream_mem_status[idx] = 0;
      assert(p_user_data == stream_mem_input[idx].virtual_address);
      break;
    }
    idx++;
  } while(idx < allocated_buffers);

  assert(idx < allocated_buffers);

  pthread_mutex_unlock(&strm_buff_stat_lock);

  sem_post(&stream_buff_free);
}

int GetFreeStreamBuffer() {
  int idx;
  sem_wait(&stream_buff_free);

  pthread_mutex_lock(&strm_buff_stat_lock);

  idx = 0;
  while(stream_mem_status[idx]) {
    idx++;
  }
  assert(idx < allocated_buffers);

  stream_mem_status[idx] = 1;

  pthread_mutex_unlock(&strm_buff_stat_lock);

  return idx;
}

static void* JpegOutputThread(void* arg) {
  struct DecPictures dec_picture;
  DWLmemset(&dec_picture, 0, sizeof(dec_picture));
  struct TestParams *params = (struct TestParams *)arg;
  u32 pic_display_number = 1;
  u32 i = 0, j = 0;

  while(output_thread_run) {
    enum DecRet ret;
    u32 first_index = 0;
    u32 first_index_enable = 0;
    u32 only_once = 0, ext_id = 0xFFFF;

    ret = VCDecNextPicture(jpeg, &dec_picture);
    if(ret == DEC_PIC_RDY) {
      u32 output_width = 0, output_height = 0, display_width = 0, display_height = 0;
      u32* host_base = NULL;
      av_unused struct DecPicture* in = NULL;
      fprintf(stdout, "[TB] \t-JPEG: DEC_PIC_RDY\n");
      fprintf(stdout, "[TB] \t-JPEG: Instance %p\n", (void *)jpeg);

      if(!mode)
        pic_display_number++;

      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        in = &dec_picture.pictures[i];
        if (!params->ppu_cfg[i].enabled || in->luma.bus_address == 0)
          continue;

        if (only_once == 0) {
          SwMatchOuputBufferId(&tool_params, &in->luma, &ext_id);
          if (ext_id < MAX_BUFFERS) {
            if (tool_params.ext_buffers[ext_id].virtual_address == NULL) {
              host_base = DWLmalloc(tool_params.ext_buffers[ext_id].size);
              tool_params.ext_buffers[ext_id].virtual_address = host_base;
            }
            DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                            tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
          }
          only_once = 1;
        }
        SwSetHostOutbaseAfterDma(host_base, in, &tool_params.ext_buffers[ext_id]);

        if (!first_index_enable) {
          first_index = i;
          first_index_enable = 1;
        }

        if (!mode) {
          output_width = in->sequence_info.pic_width;
          output_height = in->sequence_info.pic_height;
          display_width = in->sequence_info.scaled_width;
          display_height = in->sequence_info.scaled_height;
        } else {
          output_width = in->sequence_info.pic_width_thumb;
          output_height = in->sequence_info.pic_height_thumb;
          display_width = in->sequence_info.scaled_width_thumb;
          display_height = in->sequence_info.scaled_height_thumb;
        }
        fprintf(stdout, "\n[TB] \t-JPEG: The decoded image information[pp %u]\n", i);
        fprintf(stdout, "[TB] \t-JPEG: image output width(stride): %u\n", output_width);
        fprintf(stdout, "[TB] \t-JPEG: image output height:        %u\n", output_height);
        fprintf(stdout, "[TB] \t-JPEG: image decoded(HW) width: %u\n", NEXT_MULTIPLE(display_width, 16u));
        fprintf(stdout, "[TB] \t-JPEG: image decoded(HW) height: %u\n", NEXT_MULTIPLE(display_height, 8u));
        fprintf(stdout, "[TB] \t-JPEG: image DISPLAY width:  %u\n", display_width);
        fprintf(stdout, "[TB] \t-JPEG: image DISPLAY height: %u\n", display_height);

        /* calculate size for output */
        calcSize(&dec_picture.pictures[0].sequence_info, mode, &dec_picture, i);
        fprintf(stdout, "\n[TB] \t-JPEG: ++++++++++ FULL RESOLUTION[PP %u] ++++++++++\n", i);
        if (dec_picture.pictures[0].picture_info.cycles_per_mb) {
          fprintf(stdout, "[TB] \t-JPEG: %4u cycles / mb \n", dec_picture.pictures[0].picture_info.cycles_per_mb);
        }
        fprintf(stdout, "[TB] \t-JPEG: Luma output: %p size: %u\n",
                (void *)in->luma.virtual_address, size_luma);
        fprintf(stdout, "[TB] \t-JPEG: Chroma output: %p size: %u\n",
                (void *)in->chroma.virtual_address, size_chroma);
        fprintf(stdout, "[TB] \t-JPEG: Luma output bus: 0x%16llx\n",
                in->luma.bus_address);
        fprintf(stdout, "[TB] \t-JPEG: Chroma output bus: 0x%16llx\n",
                in->chroma.bus_address);

        if (write_output) {
          struct DecPicture out_picture = dec_picture.pictures[i];
          out_picture.pic_width = display_width;
          out_picture.pic_height = display_height;
          yuvsink[mode]->WritePicture(yuvsink[mode]->inst, &out_picture, i);

          if(crop)
            WriteCroppedOutput(&dec_picture.pictures[0].sequence_info,
                               (u8*)in->luma.virtual_address,
                               (u8*)in->chroma.virtual_address,
                               (u8*)in->chroma_cr.virtual_address);
          SwClearHostOutbaseAfterWriteFile(host_base, in);
        }

        for (j = 0; j < output_buffers; j++) {
          if(dec_picture.pictures[first_index].luma.bus_address ==
               user_alloc_output_buffer[j].bus_address) {
            pthread_mutex_lock(&frm_buff_stat_lock);
            frame_mem_status[j] = 0;
            pthread_mutex_unlock(&frm_buff_stat_lock);

            sem_post(&frame_buff_free);
          }
        }
      }
      if (host_base != NULL) {
        DWLfree(host_base);
        tool_params.ext_buffers[ext_id].virtual_address = NULL;
      }
      VCDecPictureConsumed(jpeg, &dec_picture);
    } else if(ret == DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      break;
    }
  }
  return NULL;
}

#ifdef ASIC_ONL_SIM
int MultiStreamId;
int main_onl(int argc, char* sv_str, int id) {
  char* argv[argc];
  char* delim=" " ;
  int   j_new=0   ;
  MainArgs args;
  MultiStreamId = id;

  char* p=strtok(sv_str,delim);
  argv[j_new]=p;
  while(p!=NULL){
    p=strtok(NULL,delim);
    j_new++;
    argv[j_new]=p;
  }

  args.argc = argc;
  args.argv = argv;
  osal_thread_init();
  MainTask(&args);

  return 0;
}

#else

RET_TYPE main(int argc, char *argv[]) {
  osal_thread_init();
  MainArgs args = { argc, argv };
#ifdef __FREERTOS__

#ifndef FREERTOS_SIMULATOR
  Platform_init();
#endif

  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&tid_task, &attr, &MainTask, &args);
  vTaskStartScheduler();
  pthread_attr_destroy(&attr);

  return 0; //should not arrive here if start scheduler successfully in real env
#else
  return MainTask(&args);
#endif
}
#endif

RET_TYPE MainTask(void *args)
{
  MainArgs *args_ = (MainArgs *)args;
  int argc = args_->argc;
  char ** argv = args_->argv;
  int return_status = 0;

  u32 len = 0;
  u32 stream_total_len = 0;
  u32 stream_seek_len = 0;

  u32 stream_in_file = 0;
#ifdef SLICE_MODE_LARGE_PIC
  u32 mcu_size_divider = 0;
  u32 amount_of_mcus = 0;
  u32 mcu_in_row = 0;
#endif

  i32 i, j = 0;
  u32 tmp = 0;
  u32 input_read_type = 0;
  u32 frame_len_prefetch[PERFETCH_EOI_NUM] = {0};
  u32 frame_len = 0;
  u32 frame_len_index = 0;
  u32 temp_len = 0;
  int ret = 0;
  struct OutFileInfo outfile_info;

  enum DecRet jpeg_ret;
  struct DecBufferInfo hbuf;
  enum DecRet rv;
  struct DecSequenceInfo image_info;
  struct DecInputParameters jpeg_in;
  DWLmemset(&jpeg_in, 0, sizeof(struct DecInputParameters));
  struct DecPictures jpeg_out;
  struct DecInitConfig init_config;
  DWLmemset(&image_info, 0, sizeof(struct DecSequenceInfo));
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  struct DWLLinearMem stream_mem;
#ifdef SEM_REPLACE_MUTEX
  sem_init(&strm_buff_stat_lock, 0, 1);
  sem_init(&frm_buff_stat_lock, 0, 1);
#endif
  pthread_mutex_init(&ext_buffer_contro, NULL);

  u8 *p_image = NULL;

  FILE *f_in = NULL;

  u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
  u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
  u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
  u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
  u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
  u32 low_latency_sim = 0;
  u32 low_latency = 0;
  task_handle task = NULL;

  FILE *f_tbcfg;
  u32 image_info_length = 0;
  u32 prev_ret = DEC_STRM_ERROR;
  u32 max_num_pics_to_decode;
  struct TestParams params;
  struct TestParams tmp_params = {0};
  struct DWLInitParam dwl_init;
  const struct DecHwFeatures *hw_feature = NULL;

  SetupDefaultParams(&params);
  if (argc < 2) {
    PrintUsage(argv[0], JPEG_DEC);
    ret = 0;
    goto return_;
  }

  /* let's disable ring buffer by default because H/W doesn't support it */
  params.is_ringbuffer = 0;

#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */

  /* expiry stuff */
  {
    char tm_buf[7];
    time_t sys_time;
    struct tm *tm;
    u32 tmp1;

    /* Check expiry date */
    time(&sys_time);
    tm = localtime(&sys_time);
    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000 + atoi(tm_buf);
    if(tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1) {
      fprintf(stderr,
              "[TB] EVALUATION PERIOD EXPIRED.\n"
              "[TB] Please contact On2 Sales.\n");
      return_status = -1;
      goto return_;
    }
  }

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;
  for(i = 0; i < MAX_STRM_BUFFERS; i++) {
    stream_mem_input[i].virtual_address = NULL;
    stream_mem_input[i].bus_address = 0;
  }

  INIT_SW_PERFORMANCE;

  fprintf(stdout, "\n[TB] * * * * * * * * * * * * * * * * \n\n\n[TB] "
          "      "
          "X170 JPEG TESTBENCH\n" "\n\n[TB] * * * * * * * * * * * * * * * * \n");

  /* reset input */
  DWLmemset(&jpeg_in, 0, sizeof(struct DecInputParameters));

  /* reset output */
  for (i = 0 ; i < DEC_MAX_OUT_COUNT; i++) {
    memset(&jpeg_out.pictures[i], 0, sizeof(jpeg_out.pictures[i]));
  }

  /* reset image_info */
  DWLmemset(&image_info, 0, sizeof(struct DecSequenceInfo));

  /* Use common command line parser to parse options. */
  if ((ret = ParseParams(argc, argv, &params))) {
    if (ret == 1) {
      printf("[TB] Failed to parse params.\n\n");
      PrintUsage(argv[0], JPEG_DEC);
      ret = 1;
      goto return_;
    }
    else {
      ret = 0;
      goto return_;
    }
  }
  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  char *tb_path = (params.in_tb_cfg_file_name == NULL) ? "tb.cfg" : params.in_tb_cfg_file_name;
  f_tbcfg = fopen(tb_path, "r");
  if(f_tbcfg == NULL) {
    printf("[TB] UNABLE TO OPEN INPUT FILE: \"tb.cfg of %s\"\n", tb_path);
    printf("[TB] USING DEFAULT CONFIGURATION\n");
  } else {
    fclose(f_tbcfg);
    if(TBParseConfig(tb_path, TBReadParam, &tb_cfg) == TB_FALSE) {
      return_status = -1;
      goto return_;
    }
    if(TBCheckCfg(&tb_cfg) != 0) {
      return_status = -1;
      goto return_;
    }
  }

  /* set secure mode */
  secure_mode = (params.decoder_mode & DEC_SECURITY) ? 1 : 0;
  if (f_tbcfg != NULL) {
    /* this dec_params used in commonconfig.c */
    /* set parameters with dec_params */
    dec_params.bus_burst_length = tb_cfg.dec_params.bus_burst_length;
    dec_params.clk_gate_decoder = tb_cfg.dec_params.clk_gate_decoder;
    dec_params.non_seq_clk = tb_cfg.dec_params.non_seq_clk;
    dec_params.seq_clk = tb_cfg.dec_params.seq_clk;
    dec_params.apf_threshold_value = tb_cfg.dec_params.apf_threshold_value;
    dec_params.axi_wr_outstand = tb_cfg.dec_params.axi_wr_outstand;
    dec_params.axi_rd_outstand = tb_cfg.dec_params.axi_rd_outstand;
  }

  if (!params.pp_enabled) {
    int i;
    int ppe = 0;
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      ppe |= tb_cfg.pp_units_params[i].unit_enabled;
    }
    if (!ppe) {
      params.ppu_cfg[0].scale.scale_by_ratio = 1;
      params.ppu_cfg[0].scale.ratio_x = 1;
      params.ppu_cfg[0].scale.ratio_y = 1;
      params.ppu_cfg[0].enabled = 1;
      params.pp_enabled = 1;
    }
  }

  /* get the value form params */
  in_file_name = params.in_file_name;
  write_output = (params.sink_type != SINK_NULL);
  pp_enabled = params.pp_enabled;
  only_full_resolution = params.only_full_resolution;
  md5sum = (params.sink_type == SINK_MD5_PICTURE);
  align = params.align;
  mc_enable = params.mc_enable;
  ri_mc_enable = params.ri_mc_enable;
  if (params.stream_trace != NULL)
    f_stream_trace = fopen(params.stream_trace, "r");
#ifdef ASIC_TRACE_SUPPORT
  crop = params.use_ref_idct;
#endif
  if ((params.decoder_mode & DEC_LOW_LATENCY) != 0) {
#ifdef MODEL_SIMULATION
      low_latency_sim = 1;
#else
      low_latency = 1;
#endif
  }

  /*TBPrintCfg(&tb_cfg); */
  jpeg_in.buffer_size = tb_cfg.dec_params.jpeg_input_buffer_size;
  jpeg_in.slice_mb_set = tb_cfg.dec_params.jpeg_mcus_slice;
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);
  printf("[TB] Decoder Jpeg Input Buffer Size %u\n", jpeg_in.buffer_size);
  printf("[TB] Decoder Slice MB Set %u\n", jpeg_in.slice_mb_set);
  printf("[TB] Decoder Clock Gating %u\n", clock_gating);
  printf("[TB] Decoder Data Discard %u\n", data_discard);
  printf("[TB] Decoder Latency Compensation %u\n", latency_comp);
  printf("[TB] Decoder Output Picture Endian %u\n", output_picture_endian);
  printf("[TB] Decoder Bus Burst Length %u\n", bus_burst_length);
  printf("[TB] Decoder Asic Service Priority %u\n", asic_service_priority);

#ifdef USE_RANDOM_ERROR_TEST
  random_error_params.seed = tb_cfg.tb_params.seed_rnd;
  strcpy(random_error_params.truncate_stream_odds, tb_cfg.tb_params.stream_truncate);
  strcpy(random_error_params.swap_bit_odds, tb_cfg.tb_params.stream_bit_swap);
  strcpy(random_error_params.packet_loss_odds, tb_cfg.tb_params.stream_packet_loss);
#endif

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
  if (params.align && params.align_h) {
    alignwidth = 1 << params.align;
    alignheight = 1 << params.align_h;
  }
  in_lib_name = params.in_lib_name;
#endif
#ifdef ASIC_TRACE_SUPPORT
  tmp = OpenAsicTraceFiles();
  if(!tmp) {
    fprintf(stdout, "[TB] Unable to open trace file(s)\n");
  }
#endif

  #ifdef ASIC_ONL_SIM
       dec_done = 0;
//  #else
//     sem_init(dec_done, 0, 0);
  #endif

  dwl_init.client_type = DWL_CLIENT_TYPE_JPEG_DEC;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
#ifdef SUPPORT_RANDOM_LATENCY
  dwl_init.axi_lg_r = params.axi_lg_r;
  dwl_init.axi_lg_w = params.axi_lg_w;
#endif
  /* Initialize Wrapper */
  dwl = DWLInit(&dwl_init);
  if(dwl == NULL) {
    goto end;
  }

  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl, dwl_init.client_type, &core_mask);
  if(core_mask == 0) {
    APITRACEERR("%s","JpegDecInit# ERROR: JPEG not supported in HW\n");
    return DEC_FORMAT_NOT_SUPPORTED;
  }

  /* Print API and build version numbers */
  {
    struct DecSwHwBuild build;
    struct DecApiVersion version;

    build = VCDecGetBuild(dwl, dwl_init.client_type);
    version = VCDecGetAPIVersion();
    printf("[TB] VC9000 Decoder JPEG API version %u.%u.%u\n", version.major,
          version.minor, version.micro);
    printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
  }

  /* after thumnails done ==> decode full images */
start_full_decode:

  /******** PHASE 1 ********/
  fprintf(stdout, "\n[TB] Phase 1: INIT JPEG DECODER\n");
  add_external_buffer = 0;
  /* Jpeg initialization */
  START_SW_PERFORMANCE;
  init_config.codec = DEC_JPEG;
  init_config.decoder_mode = DEC_NORMAL;
  init_config.error_handling = params.error_handling;
  init_config.error_ratio = params.error_ratio;
  if (low_latency || low_latency_sim)
    init_config.decoder_mode = DEC_LOW_LATENCY;
  init_config.mc_cfg.mc_enable = params.mc_enable;
  init_config.mc_cfg.stream_consumed_callback = (DecMCStreamConsumed *)StreamBufferConsumed;
  init_config.dwl_inst = dwl;
  init_config.use_adaptive_buffers = 1;
  jpeg_ret = VCDecInit((const void**)&jpeg, &init_config);
  END_SW_PERFORMANCE;
  if(jpeg_ret != DEC_OK) {
    /* Handle here the error situation */
    PrintJpegRet(jpeg_ret);
    goto end;
  }

  sem_init(&stream_buff_free, 0, allocated_buffers);
  sem_init(&frame_buff_free, 0, output_buffers);

  if (params.mc_enable) {
    for(i = 0; i < allocated_buffers; i++) {
      stream_mem_input[i].mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
      stream_mem_input[i].mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
      SET_MEM_USAGE(stream_mem_input[i].mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
      if(DWLMallocLinear(dwl,
                         JPEG_INPUT_BUFFER_SIZE, stream_mem_input + i) != DWL_OK) {
        printf("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
        goto end;
      }
    }
  }

  fprintf(stdout, "[TB] PHASE 1: INIT JPEG DECODER successful\n");

  /******** PHASE 2 ********/
  fprintf(stdout, "\n[TB] Phase 2: OPEN/READ FILE \n");

reallocate_input_buffer:

  /* Reading input file */
  f_in = fopen(params.in_file_name, "rb");
  if(f_in == NULL) {
    fprintf(stdout, "[TB] Unable to open input file\n");
    exit(-1);
  }

  /* file i/o pointer to full */
  fseek(f_in, 0L, SEEK_END);
  len = ftell(f_in);
  rewind(f_in);

  if(!stream_info_check) {
    fprintf(stdout, "\n[TB] Phase 2: CHECK THE CONTENT OF STREAM BEFORE ACTIONS\n");

    /* NOTE: The DWL should not be used outside decoder SW
     * here we call it because it is the easiest way to get
     * dynamically allocated linear memory
     * */

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    stream_mem_input[i].mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
    stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
    if(DWLMallocLinear(dwl, len, &stream_mem) != DWL_OK) {
      fprintf(stdout, "[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
      goto end;
    }

    fprintf(stdout, "[TB] \t-Input: Allocated buffer: virt: %p bus: 0x%16llx\n",
            (void *)stream_mem.virtual_address, stream_mem.bus_address);

    /* memset input */
    (void) DWLmemset(stream_mem.virtual_address, 0, len);

    byte_strm_start = (u8 *) stream_mem.virtual_address;
    if(byte_strm_start == NULL) {
      fprintf(stdout, "[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
      goto end;
    }

    /* read input stream from file to buffer and close input file */
    ret = fread(byte_strm_start, sizeof(u8), len, f_in);
    (void) ret;

    fclose(f_in);

    jpeg_ret = FindImageEnd(byte_strm_start, len, &image_info_length,
                           (u8 *)stream_mem.virtual_address, len);
    max_num_pics_to_decode = params.num_of_decoded_pics ? params.num_of_decoded_pics : nbr_of_images;
    if(jpeg_ret != 0) {
      printf("[TB] EOI missing from end of file!\n");
    }

    if(stream_mem.virtual_address != NULL)
      DWLFreeLinear(dwl, &stream_mem);

    /* set already done */
    stream_info_check = 1;

    fprintf(stdout, "[TB] PHASE 2: CHECK THE CONTENT OF STREAM BEFORE ACTIONS successful\n\n");
  }

  /* Reading input file */
  f_in = fopen(params.in_file_name, "rb");
  if(f_in == NULL) {
    fprintf(stdout, "[TB] Unable to open input file\n");
    exit(-1);
  }

  /* file i/o pointer to full */
  fseek(f_in, 0L, SEEK_END);
  len = ftell(f_in);
  rewind(f_in);

  /* Handle input buffer load */
  if(jpeg_in.buffer_size) {
    if(len > jpeg_in.buffer_size) {
      stream_total_len = len;
      len = jpeg_in.buffer_size;
    } else {
      stream_total_len = len;
      len = stream_total_len;
      jpeg_in.buffer_size = 0;
    }
  } else {
    jpeg_in.buffer_size = 0;
    stream_total_len = len;
  }

  if(prev_ret != DEC_PIC_RDY && !stream_in_file)
    stream_in_file = stream_total_len;

  /* NOTE: The DWL should not be used outside decoder SW
   * here we call it because it is the easiest way to get
   * dynamically allocated linear memory
   * */

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
  stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif

  SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if(DWLMallocLinear(dwl, len, &stream_mem) != DWL_OK) {
    fprintf(stdout, "[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
    goto end;
  }

  fprintf(stdout, "[TB] \t-Input: Allocated buffer: virt: %p bus: 0x%16p\n",
          (void *)stream_mem.virtual_address, (void *)stream_mem.bus_address);

  byte_strm_start = (u8 *) stream_mem.virtual_address;
  fseek(f_in, stream_seek_len, SEEK_SET);
  if(byte_strm_start == NULL) {
    fprintf(stdout, "[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
    goto end;
  }

  /* file i/o pointer to full */
  if (!params.mc_enable) {
    if(prev_ret == DEC_PIC_RDY)
      fseek(f_in, stream_seek_len, SEEK_SET);
  }

  /* read input stream from file to buffer and close input file */
  if (params.is_ringbuffer == 0) {
    if(low_latency) {
        process_end_flag = 0;
        sw_hw_bound = 0;
        send_strm_len = len;
        send_strm_info.strm_bus_addr = send_strm_info.strm_bus_start_addr = stream_mem.bus_address;
        send_strm_info.strm_vir_addr = send_strm_info.strm_vir_start_addr = (u8 *)stream_mem.virtual_address;
        sem_init(&send_sem, 0, 0);
        sem_init(&frame_sem, 0, 0);
        task = run_task(send_bytestrm_task, NULL);
    } else {
      ret = fread(byte_strm_start, sizeof(u8), len, f_in);
    }
    (void) ret;
  } else {
    u32 offset = (rand() + rand()) % (len + 1);  //64;//((len - 64)/16)*16; //random ?
    /* turnaround */
    ret = fread((u8 *)stream_mem.virtual_address + offset, sizeof(u8), len - offset, f_in);
    ret = fread((u8 *)stream_mem.virtual_address, sizeof(u8), offset, f_in);
    byte_strm_start = (u8 *)stream_mem.virtual_address + offset;
    (void) ret;
  }

  fclose(f_in);

  if (params.mc_enable) {
    u32 i = 0;
    u32 tmp = 0;
    int id;

    frame_len_index = 0;
    while (i < PERFETCH_EOI_NUM) {
      jpeg_ret = FindImageEOI(byte_strm_start + tmp, len - tmp,
                              &frame_len_prefetch[i],
                              byte_strm_start + tmp, len - tmp);
      if(jpeg_ret != 0) {
        break;
      }
      tmp += frame_len_prefetch[i];
      i++;
    }

    if(jpeg_ret != 0 && i == 0) {
      printf("[TB] NO MORE IMAGES!\n");
      goto end;
    }

    id = GetFreeStreamBuffer();
    if (stream_mem_input[id].size < frame_len_prefetch[frame_len_index]) {
      /* Default input buffer size is not enough, reallocate it. */
      DWLFreeLinear(dwl, stream_mem_input + id);
      stream_mem_input[id].mem_type = DWL_MEM_TYPE_CPU;
#ifdef SUPPORT_DMA
      stream_mem_input[id].mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
      SET_MEM_USAGE(stream_mem_input[i].mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
      if(DWLMallocLinear(dwl,
                      frame_len_prefetch[frame_len_index],
                      stream_mem_input + id) != DWL_OK) {
        printf("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
        goto end;
      }
    }
    jpeg_in.stream_buffer = stream_mem_input[id];
    jpeg_in.stream = (u8 *)stream_mem_input[id].virtual_address;

    /* stream processed callback param */
    jpeg_in.p_user_data = stream_mem_input[id].virtual_address;
    jpeg_in.strm_len = frame_len = frame_len_prefetch[frame_len_index++];
    //TODO(yc): need to support ring buffer mode
    DWLmemcpy(jpeg_in.stream_buffer.virtual_address, byte_strm_start, frame_len);
    temp_len += frame_len;  //after 1st frame buffer set, set temp_len
  } else {
    /* initialize JpegDecDecode input structure */
    jpeg_in.stream_buffer = stream_mem;
    jpeg_in.stream = (u8 *) byte_strm_start;

    if(!pic_counter)
      jpeg_in.strm_len = stream_total_len;
    else
      jpeg_in.strm_len = stream_in_file;
  }

  if(write_output)
    fprintf(stdout, "[TB] \t-File: Write output: YES: %u\n", write_output);
  else
    fprintf(stdout, "[TB] \t-File: Write output: NO: %u\n", write_output);
  fprintf(stdout, "[TB] \t-File: MbRows/slice: %u\n", jpeg_in.slice_mb_set);
  fprintf(stdout, "[TB] \t-File: Buffer size: %u\n", jpeg_in.buffer_size);
  fprintf(stdout, "[TB] \t-File: Stream size: %u\n", jpeg_in.strm_len);

  if(params.instant_buffer)
    fprintf(stdout, "[TB] \t-File: Output allocated by Instant Buffer mode: %u\n",
            params.instant_buffer);
  else
    fprintf(stdout, "[TB] \t-File: Output allocated by Pre-added Buffer mode: %u\n",
            params.instant_buffer);

  fprintf(stdout, "\n[TB] Phase 2: OPEN/READ FILE successful\n");

  /* jump here is frames still left */
decode:

  if(low_latency) {
    sem_wait(&send_sem);
    jpeg_in.strm_len = sw_hw_bound;
  }

  /******** PHASE 3 ********/
  fprintf(stdout, "\n[TB] Phase 3: GET IMAGE INFO\n");
  if (!params.mc_enable) {
    jpeg_ret = FindImageInfoEnd(byte_strm_start, len, &image_info_length,
        (u8 *)stream_mem.virtual_address, stream_mem.logical_size);
    printf("[TB] \timage_info_length %u\n", image_info_length);
  } else {
    jpeg_ret = FindImageInfoEnd((u8*)jpeg_in.stream_buffer.virtual_address,
        frame_len, &image_info_length, (u8*)jpeg_in.stream_buffer.virtual_address, frame_len);
    printf("[TB] \timage_info_length %u\n", image_info_length);
  }

  /* Get image information of the JFIF and decode JFIF header */
  START_SW_PERFORMANCE;
  image_info.jpeg_input_info = jpeg_in;
  if(only_full_resolution)
    image_info.thumbnail_done = 1;
  else
    image_info.thumbnail_done = ThumbDone;

  jpeg_ret = VCDecGetInfo(jpeg, &image_info);

  if(jpeg_exif_in_stream && !ThumbDone &&
     image_info.thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    nbr_of_thumb_images++;
    nbr_of_thumb_images_total = nbr_of_thumb_images;
  }
  END_SW_PERFORMANCE;
  if(jpeg_ret != DEC_OK) {
    /* Handle here the error situation */
    PrintJpegRet(jpeg_ret);
    if(DEC_INCREASE_INPUT_BUFFER == jpeg_ret) {
      DWLFreeLinear(dwl, &stream_mem);
      jpeg_in.buffer_size += 256;
      goto reallocate_input_buffer;
    } else if (DEC_FORMAT_NOT_SUPPORTED == jpeg_ret){
      goto end;
    } else {
      /* printf JpegDecGetImageInfo() info */
      fprintf(stdout, "\n[TB] \t--------------------------------------\n");
      fprintf(stdout, "[TB] \tNote! IMAGE INFO WAS CHANGED!!!\n");
      fprintf(stdout, "[TB] \t--------------------------------------\n\n");
      PrintGetImageInfo(&image_info);
      fprintf(stdout, "[TB] \t--------------------------------------\n");

      /* check if MJPEG stream and Thumb decoding ==> continue to FULL */
      if(mode) {
        if( nbr_of_thumb_images &&
            image_info.pic_width_thumb == prev_output_width_tn &&
            image_info.pic_height_thumb == prev_output_height_tn &&
            image_info.output_format_thumb == prev_output_format_tn) {
          fprintf(stdout, "\n[TB] \t--------------------------------------\n");
          fprintf(stdout, "[TB] \tNote! THUMB INFO NOT CHANGED ==> DECODE!!!\n");
          fprintf(stdout, "[TB] \t--------------------------------------\n\n");
        } else {
          ThumbDone = 1;
          nbr_of_thumb_images = 0;
          pic_counter = 0;
          stream_seek_len = 0;
          stream_in_file = 0;
          goto end;
        }
      } else {
        /* if MJPEG and only THUMB changed ==> continue */
        if( image_info.pic_width == prev_output_width &&
            image_info.pic_height == prev_output_height &&
            image_info.output_format == prev_output_format) {
          fprintf(stdout, "\n[TB] \t--------------------------------------\n");
          fprintf(stdout, "[TB] \tNote! FULL IMAGE INFO NOT CHANGED ==> DECODE!!!\n");
          fprintf(stdout, "[TB] \t--------------------------------------\n\n");
        } else {
          nbr_of_images = 0;
          pic_counter = 0;
          stream_seek_len = 0;
          stream_in_file = 0;
          goto end;
        }
      }
    }
  }

  /* update the alignment setting in "image_info" data structure
   and output picture width */;
  config.align = align;
  if (align == DEC_ALIGN_1B) config.align = DEC_ALIGN_64B;
  image_info.pic_width = NEXT_MULTIPLE(image_info.pic_width, ALIGN(config.align));
  image_info.pic_width_thumb = NEXT_MULTIPLE(image_info.pic_width_thumb, ALIGN(config.align));

  /* save for MJPEG check */
  /* full */
  prev_output_width = image_info.pic_width;
  prev_output_height = image_info.pic_height;
  prev_output_format = image_info.output_format;
  /* thumbnail */
  prev_output_width_tn = image_info.pic_width_thumb;
  prev_output_height_tn = image_info.pic_height_thumb;
  prev_output_format_tn = image_info.output_format_thumb;

  /* printf JpegDecGetImageInfo() info */
  PrintGetImageInfo(&image_info);
#if 0
  /* update output_width/height for output buffer allocation if scaled is enabled */
  if ( scale_enabled ) {
    image_info.pic_width = NEXT_MULTIPLE(config.ppu_config[0].scale.width, ALIGN(config.align));
    image_info.pic_width_thumb = NEXT_MULTIPLE(config.ppu_config[0].scale.width, ALIGN(config.align));
    image_info.pic_height = init_config.ppu_config[0].scale.height;
    image_info.pic_height_thumb = init_config.ppu_config[0].scale.height;
  }
#endif

  /*  ******************** THUMBNAIL **************************** */
  /* Select if Thumbnail or full resolution image will be decoded */
  if(image_info.thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    /* if all thumbnails processed (MJPEG) */
    if(!ThumbDone)
      jpeg_in.dec_image_type = JPEGDEC_THUMBNAIL;
    else
      jpeg_in.dec_image_type = JPEGDEC_IMAGE;

    thumb_in_stream = 1;
  } else if(image_info.thumbnail_type == JPEGDEC_NO_THUMBNAIL)
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;
  else if(image_info.thumbnail_type == JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT) {
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;
    ThumbDone = 1;
    goto end;
  }

  /* check if forced to decode only full resolution images
      ==> discard thumbnail */
  if(only_full_resolution) {
    /* decode only full resolution image */
    fprintf(stdout,
            "\n[TB] \tNote! FORCED BY USER TO DECODE ONLY FULL RESOLUTION IMAGE\n");
    jpeg_in.dec_image_type = JPEGDEC_IMAGE;
  }

  fprintf(stdout, "[TB] PHASE 3: GET IMAGE INFO successful\n");

  /* TB SPECIFIC == LOOP IF THUMBNAIL IN JFIF */
  /* Decode JFIF */
  if(jpeg_in.dec_image_type == JPEGDEC_THUMBNAIL)
    mode = 1; /* TODO KIMA */
  else
    mode = 0;

  if (params.ppu_cfg[0].scale.scale_by_ratio) {
    params.ppu_cfg[0].scale.width = 0;
    params.ppu_cfg[0].scale.height = 0;
  }

  /* Set PP info after image info and decoding mode is determined. */
  SetPpConfig(&params, image_info, &config, hw_feature->crop_step_rshift, jpeg_in.dec_image_type);
  /* for color remapping (3dlut) */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!config.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      u32 size = 18 * 18 * 18 * 3 * sizeof(u16);
      // ppu_int_cfg.table_3dlut.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      config.table_3dlut_buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                        DWL_MEM_TYPE_CPU;
      if (DWLMallocLinear(dwl, size, &config.table_3dlut_buffer)) {
        return -1;
      }
      DWLmemset(config.table_3dlut_buffer.virtual_address, 0, size);
      u16 lut3DTableData[18 * 18 * 18][3] = {{0}};
      memset(lut3DTableData, 0, sizeof(lut3DTableData));
      u16 tmpDataNew[18 * 18 * 18][3] = {{0}};
      if (params.table_3dlut_name != NULL) {
        dump3DLutData(lut3DTableData, params.table_3dlut_name);
      } else {
        calculate3DLutTableFor2020To709(lut3DTableData);
      }

      u16 cnt=0;
      for (u16 row_shift1 = 0; row_shift1 < 18 * 18 * 18 / 8; row_shift1 += 18 * 18 / 4) {
        for (u16 col_shift1 = 0; col_shift1 < 8; col_shift1 += 4) { // 4
          for (u16 row_shift2 = 0; row_shift2 < 18 * 18 / 4; row_shift2 += 18 / 2) {
            for (u16 col_shift2 = 0; col_shift2 < 4; col_shift2 += 2) {
              for (u16 row_shift3 = 0; row_shift3 < 18 / 2; row_shift3++) {
                u16 i = 8 * (row_shift1 + row_shift2 + row_shift3) + col_shift1 + col_shift2;
                tmpDataNew[i][0]=lut3DTableData[cnt][0];
                tmpDataNew[i][1]=lut3DTableData[cnt][1];
                tmpDataNew[i][2]=lut3DTableData[cnt][2];
                cnt++;
                tmpDataNew[i+1][0]=lut3DTableData[cnt][0];
                tmpDataNew[i+1][1]=lut3DTableData[cnt][1];
                tmpDataNew[i+1][2]=lut3DTableData[cnt][2];
                cnt++;
              }
            }
          }
        }
      }
      for(u16 i = 0; i < 18 * 18 * 18; i++) {
        lut3DTableData[i][2] = tmpDataNew[i][0]; // R
        lut3DTableData[i][1] = tmpDataNew[i][1]; // G
        lut3DTableData[i][0] = tmpDataNew[i][2]; // B
      }
      memcpy((u32 *)config.table_3dlut_buffer.virtual_address, lut3DTableData, 18 * 18 * 18 * 3 * sizeof(u16));
    }
  }

  tmp = VCDecSetInfo(jpeg, &config);
  if (tmp != DEC_OK)
    goto end;
#ifdef ASIC_TRACE_SUPPORT
  /* Handle incorrect slice size for HW testing */
  if(jpeg_in.slice_mb_set > (image_info.pic_height >> 4)) {
    jpeg_in.slice_mb_set = (image_info.pic_height >> 4);
    printf("[TB] FIXED Decoder Slice MB Set %u\n", jpeg_in.slice_mb_set);
  }
#endif

  params.crop_align = hw_feature->crop_step_rshift;
  /* Create output sink after output file names are determined. */
  // for (i = 0; i < sizeof(yuvsink) / sizeof(yuvsink[0]); i++)
  {
    if (yuvsink[mode] == NULL) {
      outfile_info.bitstream_format = BITSTREAM_JPEG;
      outfile_info.bit_depth = (image_info.bit_depth_luma == 8 && image_info.bit_depth_chroma == 8) ? 8 : 10;
      outfile_info.is_interlaced = 0;
      outfile_info.is_thumbnail = mode;
      params.compress_bypass = TRUE; /* G1 don't support rfc */
      if (!mode) {
        outfile_info.pic_width = image_info.scaled_width;
        outfile_info.pic_height = image_info.scaled_height;
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink[mode] = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          goto end;
        }
      } else {
        outfile_info.pic_width = image_info.scaled_width_thumb;
        outfile_info.pic_height = image_info.scaled_height_thumb;
        tmp_params = params;
        DWLmemset(tmp_params.out_file_name, 0, sizeof(tmp_params.out_file_name));
        GenerateOutputFileName(&tmp_params, &outfile_info);
        if ((yuvsink[mode] = CreateYuvSink(&tmp_params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          goto end;
        }
      }
    }
  }

  /* no slice mode supported in progressive || non-interleaved ==> force to full mode */
  if((jpeg_in.dec_image_type == JPEGDEC_THUMBNAIL &&
      image_info.coding_mode_thumb == JPEG_PROGRESSIVE) ||
      (jpeg_in.dec_image_type == JPEGDEC_IMAGE &&
       image_info.coding_mode == JPEG_PROGRESSIVE))
    jpeg_in.slice_mb_set = 0;

  /******** PHASE 4 ********/
  /* Image mode to decode */
  if(mode)
    fprintf(stdout, "\n[TB] Phase 4: DECODE FRAME: THUMBNAIL\n");
  else
    fprintf(stdout, "\n[TB] Phase 4: DECODE FRAME: FULL RESOLUTION\n");

  /* if input (only full, not tn) > 4096 MCU      */
  /* ==> force to slice mode                                      */
  if(mode == 0 && !params.mc_enable) {
#ifdef SLICE_MODE_LARGE_PIC
#ifdef ASIC_TRACE_SUPPORT
    /* calculate MCU's */
    if(image_info.output_format == DEC_OUT_FRM_YUV400 ||
        image_info.output_format == DEC_OUT_FRM_YUV444SP) {
      amount_of_mcus =
        ((image_info.pic_width * image_info.pic_height) / 64);
      mcu_in_row = (image_info.pic_width / 8);
    } else if(image_info.output_format == DEC_OUT_FRM_YUV420SP) {
      /* 265 is the amount of luma samples in MB for 4:2:0 */
      amount_of_mcus =
        ((image_info.pic_width * image_info.pic_height) / 256);
      mcu_in_row = (image_info.pic_width / 16);
    } else if(image_info.output_format == DEC_OUT_FRM_YUV422SP) {
      /* 128 is the amount of luma samples in MB for 4:2:2 */
      amount_of_mcus =
        ((image_info.pic_width * image_info.pic_height) / 128);
      mcu_in_row = (image_info.pic_width / 16);
    } else if(image_info.output_format == DEC_OUT_FRM_YUV440) {
      /* 128 is the amount of luma samples in MB for 4:4:0 */
      amount_of_mcus =
        ((image_info.pic_width * image_info.pic_height) / 128);
      mcu_in_row = (image_info.pic_width / 8);
    } else if(image_info.output_format == DEC_OUT_FRM_YUV411SP) {
      amount_of_mcus =
        ((image_info.pic_width * image_info.pic_height) / 256);
      mcu_in_row = (image_info.pic_width / 32);
    }

    /* set mcu_size_divider for slice size count */
    if(image_info.output_format == DEC_OUT_FRM_YUV400 ||
        image_info.output_format == DEC_OUT_FRM_YUV440 ||
        image_info.output_format == DEC_OUT_FRM_YUV444SP)
      mcu_size_divider = 2;
    else
      mcu_size_divider = 1;

    /* 8190 and over 16M ==> force to slice mode */
    if((jpeg_in.slice_mb_set == 0) &&
        ((image_info.pic_width * image_info.pic_height) >
          (image_info.img_max_dec_width * image_info.img_max_dec_height))) {
      do {
        jpeg_in.slice_mb_set++;
      } while(((jpeg_in.slice_mb_set * (mcu_in_row / mcu_size_divider)) +
                (mcu_in_row / mcu_size_divider)) <
              JPEGDEC_MAX_SLICE_SIZE_8190);
      printf
      ("[TB] Force to slice mode (over 16M) ==> Decoder Slice MB Set %u\n",
        jpeg_in.slice_mb_set);
    }
#else
      /* 8190 and over 16M ==> force to slice mode */
      if((jpeg_in.slice_mb_set == 0) &&
          ((image_info.pic_width * image_info.pic_height) >
           (image_info.img_max_dec_width * image_info.img_max_dec_height))) {
        do {
          jpeg_in.slice_mb_set++;
        } while(((jpeg_in.slice_mb_set * (mcu_in_row / mcu_size_divider)) +
                 (mcu_in_row / mcu_size_divider)) <
                JPEGDEC_MAX_SLICE_SIZE_8190);
        printf
      ("[TB] Force to slice mode (over 16M) ==> Decoder Slice MB Set %u\n",
         jpeg_in.slice_mb_set);
    }
#endif
#endif
  }

allocate_buffer:
  rv = VCDecGetBufferInfo(jpeg, &hbuf);
  if(rv != DEC_WAITING_FOR_BUFFER && rv != DEC_OK && rv != DEC_PARAM_ERROR) {
    /* Handle here the error situation */
    PrintJpegRet(rv);
    goto error;
  }
  if (rv == DEC_PARAM_ERROR) {
    PrintJpegRet(rv);
    goto end;
  }
  printf("[TB] JpegDecGetBufferInfo ret %d\n", rv);
  printf("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
         (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);

  /* if Instant Buffer mode & !mc*/
  if((params.instant_buffer && !params.mc_enable && !add_external_buffer) ||
     (rv == DEC_WAITING_FOR_BUFFER && hbuf.buf_to_free.virtual_address != NULL && !params.mc_enable)) {
    fprintf(stdout, "\n[TB] \t-JPEG: USER ALLOCATED MEMORY\n");
    if (rv == DEC_WAITING_FOR_BUFFER && hbuf.buf_to_free.virtual_address != NULL) {
      if(user_alloc_output_buffer[0].virtual_address != NULL)
        DWLFreeRefFrm(dwl, &user_alloc_output_buffer[0]);
    }
    jpeg_ret = allocMemory(jpeg, &image_info, &jpeg_in, dwl, hbuf.next_buf_size);
    if(jpeg_ret != DEC_OK) {
      /* Handle here the error situation */
      PrintJpegRet(jpeg_ret);
      goto end;
    }
    add_external_buffer = 1;
    fprintf(stdout, "[TB] \t-JPEG: USER ALLOCATED MEMORY successful\n\n");
  } else if(!params.instant_buffer && !params.mc_enable) {
    struct DWLLinearMem mem = {0};

    if (hbuf.buf_to_free.bus_address != 0) {
      if(ext_buffers[0].bus_address != 0) {
        ASSERT(ext_buffers[0].virtual_address == hbuf.buf_to_free.virtual_address);
        DWLFreeLinear(dwl, &ext_buffers[0]);
      }
      add_external_buffer = 0;  //reset the flag to reallocate buffer
    }

    if(hbuf.next_buf_size && !add_external_buffer) {
      /* Only add minimum required buffers at first. */
      for(i = 0; i < hbuf.buf_num; i++) {
        mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
        mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
        if (pp_enabled)
          SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
        else
          SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
        if(DWLMallocLinear(dwl, hbuf.next_buf_size, &mem) != DWL_OK) {
          printf("[TB] UNABLE TO ALLOCATE DEC BUFFER MEMORY\n");
          goto end;
        }
        rv = VCDecAddBuffer(jpeg, &mem);

        printf("[TB] VCDecAddBuffer ret %d\n", rv);
        if(rv != DEC_OK) {
          DWLFreeLinear(dwl, &mem);
        } else {
          ext_buffers[i] = mem;
        }
      }
      /* Extra buffers are allowed when minimum required buffers have been added.*/
      add_external_buffer = 1;
    }
  }

  if(params.mc_enable) {
    if (params.instant_buffer) {
      fprintf(stdout, "\n[TB] \t-JPEG: USER ALLOCATED MEMORY\n");
      jpeg_ret = inputMemoryAlloc(jpeg, &image_info, &jpeg_in, dwl, &hbuf);
      if(jpeg_ret != DEC_OK) {
        /* Handle here the error situation */
        PrintJpegRet(jpeg_ret);
        goto end;
      }
      fprintf(stdout, "[TB] \t-JPEG: USER ALLOCATED MEMORY successful\n\n");
    } else {
      struct DWLLinearMem mem = {0};

      pthread_mutex_lock(&ext_buffer_contro);

      if (hbuf.buf_to_free.bus_address != 0) {
        for (i = 0; i < output_buffers; i++) {
          if(ext_buffers[i].virtual_address == hbuf.buf_to_free.virtual_address) {
            DWLFreeLinear(dwl, &ext_buffers[i]);
            break;
          }
        }
        ASSERT(i < output_buffers);
        mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
        mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif

        if (pp_enabled)
          SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
        else
          SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
        if(DWLMallocLinear(dwl, hbuf.next_buf_size, &mem) != DWL_OK) {
          printf("[TB] UNABLE TO ALLOCATE DEC BUFFER MEMORY\n");
          goto end;
        }
        rv = VCDecAddBuffer(jpeg, &mem);

        printf("[TB] VCDecAddBuffer ret %d\n", rv);
        if(rv != DEC_OK) {
          DWLFreeLinear(dwl, &mem);
        } else {
          ext_buffers[i] = mem;
        }
      }

      if(hbuf.next_buf_size && !add_external_buffer) {
        for(i = 0; i < output_buffers; i++) {
          mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
          mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
          if (pp_enabled)
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
          else
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
          if(DWLMallocLinear(dwl, hbuf.next_buf_size, &mem) != DWL_OK) {
            printf("[TB] UNABLE TO ALLOCATE DEC BUFFER MEMORY\n");
            goto end;
          }
          rv = VCDecAddBuffer(jpeg, &mem);

          printf("[TB] VCDecAddBuffer ret %d\n", rv);
          if(rv != DEC_OK) {
            DWLFreeLinear(dwl, &mem);
          } else {
            ext_buffers[i] = mem;
          }
        }
        add_external_buffer = 1;
      }
      pthread_mutex_unlock(&ext_buffer_contro);
    }
  }

  if (ri_mc_enable) {
    ri_array = (u32 *)DWLmalloc(RI_MAX_NUM * sizeof(u32));
    ri_count = FindImageAllRI((u8 *)jpeg_in.stream_buffer.virtual_address,
                              jpeg_in.strm_len,
                              ri_array, RI_MAX_NUM);
    if (ri_count > RI_MAX_NUM) {
      /* If buffer not enough, reallocate and re-parse. */
      DWLfree(ri_array);
      ri_array = (u32 *)DWLmalloc(ri_count * sizeof(u32));
      ri_count = FindImageAllRI((u8 *)jpeg_in.stream_buffer.virtual_address,
                                jpeg_in.strm_len,
                                ri_array, ri_count);
    }
  }
  jpeg_in.ri_count = ri_count;
  jpeg_in.ri_array = ri_array;
  struct DecOutput jpeg_output;
  u32 output_width = 0, output_height = 0, display_width = 0, display_height = 0;
  if (params.instant_buffer) {
    tool_params.ext_buffers = user_alloc_output_buffer;
  } else {
    tool_params.ext_buffers = ext_buffers;
  }
  tool_params.max_buffers = &output_buffers;
  tool_params.dwl_inst = dwl;

  /* decode */
  do {
    START_SW_PERFORMANCE;
    /* jpeg_ret = JpegDecDecode(jpeg, &jpeg_in, &jpeg_out); */
    jpeg_ret = VCDecDecode(jpeg, &jpeg_output, &jpeg_in); /* do not support output in here. */
    END_SW_PERFORMANCE;

    /* release curr stream buffer */
    if (params.mc_enable && jpeg_ret != DEC_PIC_RDY) {
      StreamBufferConsumed((u8 *)jpeg_in.stream_buffer.virtual_address,
              jpeg_in.stream_buffer.virtual_address);
    }

    if(jpeg_ret == DEC_PIC_RDY) {
      if (params.instant_buffer) {
        (void)DWLmemcpy(&jpeg_out, &jpeg_output.pic, sizeof(struct DecPictures));
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          if(jpeg_output.pic.pictures[i].luma.virtual_address != NULL) {
            fprintf(stdout, "[TB] \t-JPEG: DEC_PIC_RDY\n");
            if (!mode) {
              output_width = jpeg_out.pictures[0].sequence_info.pic_width;
              output_height = jpeg_out.pictures[0].sequence_info.pic_height;
              display_width = jpeg_out.pictures[0].sequence_info.scaled_width;
              display_height = jpeg_out.pictures[0].sequence_info.scaled_height;
            } else {
              output_width = jpeg_out.pictures[0].sequence_info.pic_width_thumb;
              output_height = jpeg_out.pictures[0].sequence_info.pic_height_thumb;
              display_width = jpeg_out.pictures[0].sequence_info.scaled_width_thumb;
              display_height = jpeg_out.pictures[0].sequence_info.scaled_height_thumb;
            }
            fprintf(stdout, "\n[TB] \t-JPEG: The decoded image information[pp %d]\n", i);
            fprintf(stdout, "[TB] \t-JPEG: image output width(stride): %u\n", output_width);
            fprintf(stdout, "[TB] \t-JPEG: image output height:        %u\n", output_height);
            fprintf(stdout, "[TB] \t-JPEG: image decoded(HW) width: %u\n", NEXT_MULTIPLE(display_width, 16));
            fprintf(stdout, "[TB] \t-JPEG: image decoded(HW) height: %u\n", NEXT_MULTIPLE(display_height, 8));
            fprintf(stdout, "[TB] \t-JPEG: image DISPLAY width:  %u\n", display_width);
            fprintf(stdout, "[TB] \t-JPEG: image DISPLAY height: %u\n", display_height);
            break;
          }
        }
      } else {
        if(!output_thread_run) {
          output_thread_run = 1;
          pthread_create(&output_thread, NULL, JpegOutputThread, &params);
        }
      }

      /* check if progressive ==> planar output */
      if((image_info.coding_mode == JPEG_PROGRESSIVE && mode == 0) ||
          (image_info.coding_mode_thumb == JPEG_PROGRESSIVE &&
           mode == 1)) {
        progressive = 1;
      }

      if((image_info.coding_mode == JPEG_NONINTERLEAVED && mode == 0)
          || (image_info.coding_mode_thumb == JPEG_NONINTERLEAVED &&
              mode == 1))
        non_interleaved = 1;
      else
        non_interleaved = 0;

      if(jpeg_in.slice_mb_set && full_slice_counter == -1)
        sliced_output_used = 1;

      /* info to handleSlicedOutput */
      frame_ready = 1;
      if(!mode)
        nbr_of_images_to_out++;

      /* for input buffering */
      prev_ret = DEC_PIC_RDY;
#ifdef ASIC_TRACE_SUPPORT
      pic_number++;
#endif
    } else if(jpeg_ret == DEC_SCAN_PROCESSED) {
      /* TODO! Progressive scan ready... */
      fprintf(stdout, "[TB] \t-JPEG: DEC_SCAN_PROCESSED\n");
      if(!output_thread_run) {
        output_thread_run = 1;
        pthread_create(&output_thread, NULL, JpegOutputThread, &params);
      }

      /* progressive ==> planar output */
      if(image_info.coding_mode == JPEG_PROGRESSIVE)
        progressive = 1;

      /* info to handleSlicedOutput */
      printf("[TB] SCAN %u READY\n", scan_counter);

      if(image_info.coding_mode == JPEG_PROGRESSIVE) {
        /* calculate size for output */
        calcSize(&image_info, mode, &jpeg_out, 0);
        printf("[TB] size_luma %u and size_chroma %u\n", size_luma, size_chroma);
        scan_counter++;
      }

      /* update/reset */
      progressive = 0;
      scan_ready = 0;
    } else if(jpeg_ret == DEC_SLICE_RDY) {
      fprintf(stdout, "[TB] \t-JPEG: DEC_SLICE_RDY\n");

      sliced_output_used = 1;

      /* calculate/write output of slice
       * and update output budder in case of
       * user allocated memory */
      if(jpeg_out.pictures[0].luma.virtual_address != NULL)
        handleSlicedOutput(&image_info, &jpeg_in, &jpeg_out);

      scan_counter++;
    } else if(jpeg_ret == DEC_STRM_PROCESSED) {
      fprintf(stdout,
              "[TB] \t-JPEG: DEC_STRM_PROCESSED ==> Load input buffer\n");

      /* update seek value */
      stream_seek_len += len;

      if(stream_in_file < len) {
        fprintf(stdout, "[TB] \t\t==> Unable to load input buffer\n");
        fprintf(stdout,
                "[TB] \t\t\t==> TRUNCATED INPUT ==> DEC_STRM_ERROR\n");
        jpeg_ret = DEC_STRM_ERROR;
        goto strm_error;
      }

      stream_in_file -= len;
      if(stream_in_file < len) {
        len = stream_in_file;
      }

      /* update the buffer size in case last buffer
         doesn't have the same amount of data as defined */
      if(len < jpeg_in.buffer_size) {
        jpeg_in.buffer_size = len;
      }

      /* Reading input file */
      f_in = fopen(params.in_file_name, "rb");
      if(f_in == NULL) {
        fprintf(stdout, "[TB] Unable to open input file\n");
        exit(-1);
      }

      /* file i/o pointer to full */
      fseek(f_in, stream_seek_len, SEEK_SET);
      /* read input stream from file to buffer and close input file */
      ret = fread(byte_strm_start, sizeof(u8), len, f_in);
      (void) ret;
      fclose(f_in);

      /* update */
      jpeg_in.stream_buffer.virtual_address = (u32 *) byte_strm_start;
      jpeg_in.stream_buffer.bus_address = stream_mem.bus_address;
    } else if(jpeg_ret == DEC_NO_DECODING_BUFFER) {
      PrintJpegRet(jpeg_ret);
      usleep(10000);
    } else if(jpeg_ret == DEC_WAITING_FOR_BUFFER) {
      PrintJpegRet(jpeg_ret);
      goto allocate_buffer;
    } else if(jpeg_ret == DEC_STRM_ERROR) {
strm_error:

      if(jpeg_in.slice_mb_set && full_slice_counter == -1)
        sliced_output_used = 1;

      if(!output_thread_run && !params.instant_buffer) {
        output_thread_run = 1;
        pthread_create(&output_thread, NULL, JpegOutputThread, &params);
      }

      /* calculate/write output of slice
       * and update output budder in case of
       * user allocated memory */
      if(sliced_output_used &&
          jpeg_out.pictures[0].luma.virtual_address != NULL)
        handleSlicedOutput(&image_info, &jpeg_in, &jpeg_out);

      /* info to handleSlicedOutput */
      frame_ready = 1;
      sliced_output_used = 0;

      /* Handle here the error situation */
      PrintJpegRet(jpeg_ret);
      if(mode == 1)
        break;
      else
        goto error;
    } else if(jpeg_ret == DEC_END_OF_STREAM){
      PrintJpegRet(jpeg_ret);
      goto end;
    } else {
      /* Handle here the error situation */
      PrintJpegRet(jpeg_ret);
      if(mode == 1)
        break;
      else
        goto error;;
    }
  } while(jpeg_ret != DEC_PIC_RDY);

error:

  /* release curr frame buffer */
  if (params.mc_enable && jpeg_ret != DEC_PIC_RDY) {
    for (j = 0; j < output_buffers; j++) {
      if(jpeg_in.picture_buffer_y.virtual_address ==
          user_alloc_output_buffer[j].virtual_address) {
        pthread_mutex_lock(&frm_buff_stat_lock);
        frame_mem_status[j] = 0;
        pthread_mutex_unlock(&frm_buff_stat_lock);
        sem_post(&frame_buff_free);
        break;
      }
    }
  }

  /* calculate/write output of slice */
  if(sliced_output_used && jpeg_out.pictures[0].luma.virtual_address != NULL) {
    handleSlicedOutput(&image_info, &jpeg_in, &jpeg_out);
    sliced_output_used = 0;
  }

  if(jpeg_out.pictures[0].luma.virtual_address != NULL && !params.mc_enable) {
    /* calculate size for output */
    calcSize(&image_info, mode, &jpeg_out, 0);

    /* Thumbnail || full resolution */
    if(!mode)
      fprintf(stdout, "\n[TB] \t-JPEG: ++++++++++ FULL RESOLUTION ++++++++++\n");
    else
      fprintf(stdout, "[TB] \t-JPEG: ++++++++++ THUMBNAIL ++++++++++\n");
    if (jpeg_out.pictures[0].picture_info.cycles_per_mb) {
      fprintf(stdout, "[TB] \t-JPEG: %4u cycles / mb \n", jpeg_out.pictures[0].picture_info.cycles_per_mb);
    }
    fprintf(stdout, "[TB] \t-JPEG: Instance %p\n", (void *) jpeg);
    fprintf(stdout, "[TB] \t-JPEG: Luma output: %p size: %u\n",
            (void *)jpeg_out.pictures[0].luma.virtual_address, size_luma);
    fprintf(stdout, "[TB] \t-JPEG: Chroma output: %p size: %u\n",
            (void *)jpeg_out.pictures[0].chroma.virtual_address, size_chroma);
    fprintf(stdout, "[TB] \t-JPEG: Luma output bus: 0x%16llx\n",
            jpeg_out.pictures[0].luma.bus_address);
    fprintf(stdout, "[TB] \t-JPEG: Chroma output bus: 0x%16llx\n",
           jpeg_out.pictures[0].chroma.bus_address);
  }

  fprintf(stdout, "[TB] PHASE 4: DECODE FRAME successful\n");

  /* if output write not disabled by TB */
  if(write_output && params.instant_buffer && !params.mc_enable) {
    /******** PHASE 5 ********/
    fprintf(stdout, "[TB] \nPhase 5: WRITE OUTPUT\n");

    if(image_info.coding_mode == JPEG_PROGRESSIVE)
      progressive = 1;

    /* write output */
    u32* host_base = NULL;
    struct DecPicture* in = &jpeg_out.pictures[0];
    u32 ext_id = 0xFFFF;
    if(jpeg_in.slice_mb_set) {
      if(image_info.output_format != DEC_OUT_FRM_YUV400)
        WriteFullOutput(params.out_file_name, mode);
    } else {
      if(image_info.coding_mode != JPEG_PROGRESSIVE) {
        u32 only_once = 0, ext_id = 0xFFFF;

        for (j = 0; j < DEC_MAX_OUT_COUNT; j++) {
          if (!params.ppu_cfg[j].enabled)
            continue;

          in = &jpeg_out.pictures[j];
          if (only_once == 0) {
            SwMatchOuputBufferId(&tool_params, &in->luma, &ext_id);
            if (ext_id < MAX_BUFFERS) {
              if (tool_params.ext_buffers[ext_id].virtual_address == NULL) {
                host_base = DWLmalloc(tool_params.ext_buffers[ext_id].size);
                tool_params.ext_buffers[ext_id].virtual_address = host_base;
              }
              DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                              tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
            }
            only_once = 1;
          }
          SwSetHostOutbaseAfterDma(host_base, in, &tool_params.ext_buffers[ext_id]);

          if (!jpeg_in.dec_image_type) {
            in->pic_width = display_width;
            in->pic_height = display_height;
            yuvsink[mode]->WritePicture(yuvsink[mode]->inst, in, j);
          }
          SwClearHostOutbaseAfterWriteFile(host_base, in);
        }
      } else {
        /* calculate size for output */
        calcSize(&image_info, mode, &jpeg_out, 0);

        printf("[TB] size_luma %u and size_chroma %u\n", size_luma, size_chroma);
        SwMatchOuputBufferId(&tool_params, &in->luma, &ext_id);
        if (ext_id < MAX_BUFFERS) {
          if (tool_params.ext_buffers[ext_id].virtual_address == NULL) {
            host_base = DWLmalloc(tool_params.ext_buffers[ext_id].size);
            tool_params.ext_buffers[ext_id].virtual_address = host_base;
          }
          DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                          tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
        }
        SwSetHostOutbaseAfterDma(host_base, in, &tool_params.ext_buffers[ext_id]);
        WriteProgressiveOutput(params.out_file_name, size_luma, size_chroma, mode,
                               (u8*)in->luma.virtual_address,
                               (u8*)in->chroma.virtual_address,
                               (u8*)in->chroma_cr.virtual_address);
        SwClearHostOutbaseAfterWriteFile(host_base, in);
      }
      if (host_base != NULL) {
        DWLfree(host_base);
        tool_params.ext_buffers[ext_id].virtual_address = NULL;
      }
    }

    if(crop) {
      in = &jpeg_out.pictures[0];
      if (in->luma.virtual_address == NULL) {
        SwMatchOuputBufferId(&tool_params, &in->luma, &ext_id);
        if (ext_id < MAX_BUFFERS) {
          host_base = DWLmalloc(tool_params.ext_buffers[ext_id].size);
          tool_params.ext_buffers[ext_id].virtual_address = host_base;
        }
        DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                        tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
      }
      WriteCroppedOutput(&image_info,
                         (u8*)in->luma.virtual_address,
                         (u8*)in->chroma.virtual_address,
                         (u8*)in->chroma_cr.virtual_address);
      if (host_base != NULL) {
        DWLfree(host_base);
        tool_params.ext_buffers[ext_id].virtual_address = NULL;
      }
    }

    progressive = 0;
    fprintf(stdout, "[TB] PHASE 5: WRITE OUTPUT successful from user buffer\n");
  } else {
    fprintf(stdout, "\n[TB] Phase 5: WRITE OUTPUT DISABLED from user buffer\n");
  }

  /* more images to decode? */
  if(nbr_of_images || (nbr_of_thumb_images && !only_full_resolution)|| jpeg_output.data_left) {
    pic_counter++;
    if (max_num_pics_to_decode <= num_of_decoded_pics) {
      pic_counter = 0;
      stream_seek_len = 0;
      stream_in_file = 0;
      goto end;
    }

    if(mode) {
      nbr_of_thumb_images--;
      nbr_of_thumb_images_to_out++;
      if(nbr_of_thumb_images == 0 ||
         (params.mc_enable && (nbr_of_thumb_images+1)%nbr_of_images==0)) {
        /* set */
        ThumbDone = 1;
        pic_counter = 0;
        thumb_in_stream = 0;
        stream_seek_len = 0;
        stream_in_file = 0;
        goto end;
      }
    } else {
      if (nbr_of_images) {
        num_of_decoded_pics ++;
        nbr_of_images--;
      }
      if(nbr_of_images == 0 && jpeg_output.data_left == 0) {
        /* set */
        pic_counter = 0;
        stream_seek_len = 0;
        stream_in_file = 0;
        goto end;
      } else if (nbr_of_images == 0) {
        //byte_strm_start = jpeg_output.strm_curr_pos;
      }
    }

    /* if input buffered load */
    if(jpeg_in.buffer_size) {
      u32 counter = 0;
      /* seek until next pic start */
      do {
        /* Seek next EOI */
        jpeg_ret = FindImageTnEOI(byte_strm_start, len, &image_info_length, mode,
            thumb_in_stream, byte_strm_start, len);

        /* check result */
        if(jpeg_ret == DEC_OK && next_soi)
          break;
        else {
          jpeg_ret = -1;
          counter++;
        }

        /* update seek value */
        stream_in_file -= len;
        stream_seek_len += len;

        if(stream_in_file <= 0) {
          fprintf(stdout, "[TB] \t\t==> Unable to load input buffer\n");
          fprintf(stdout,
                  "[TB] \t\t\t==> TRUNCATED INPUT ==> DEC_STRM_ERROR\n");
          jpeg_ret = DEC_STRM_ERROR;
          goto end;
        }

        if(stream_in_file < len)
          len = stream_in_file;

        /* Reading input file */
        f_in = fopen(params.in_file_name, "rb");
        if(f_in == NULL) {
          fprintf(stdout, "[TB] Unable to open input file\n");
          exit(-1);
        }

        /* file i/o pointer to full */
        fseek(f_in, stream_seek_len, SEEK_SET);
        if(low_latency) {
            tmp_len = len;
            bytes_go_back = stream_seek_len;
            sem_post(&frame_sem);
        } else {
          /* read input stream from file to buffer and close input file */
          ret = fread(byte_strm_start, sizeof(u8), len, f_in);
        }
        (void) ret;
        fclose(f_in);
      } while(jpeg_ret != 0);
    } else {
      /* Find next image */
      if (!params.mc_enable)
        jpeg_ret = FindImageTnEOI(byte_strm_start, jpeg_in.strm_len - jpeg_output.data_left, &image_info_length, mode,
            thumb_in_stream, (u8 *)stream_mem.virtual_address, stream_mem.logical_size);
    }
    /* If image info is not found */
    if(jpeg_ret != 0 && !params.mc_enable) {
      if (jpeg_output.data_left == 0) {
        printf("[TB] NO MORE IMAGES!\n");
        goto end;
      } else {
        image_info_length = jpeg_in.strm_len - jpeg_output.data_left;
        byte_strm_start = jpeg_in.stream;
      }
    }

    if(jpeg_ret == 0 && low_latency) {
      sem_post(&frame_sem);
    }

    /* update seek value */
    if (!params.mc_enable) {
      pic_decoded = 1;
      stream_in_file -= image_info_length;
      stream_seek_len += image_info_length;
      tmp_len = stream_in_file;
      bytes_go_back = stream_seek_len;

      if(stream_in_file <= 0) {
        fprintf(stdout, "[TB] \t\t==> Unable to load input buffer\n");
        fprintf(stdout,
                "[TB] \t\t\t==> TRUNCATED INPUT ==> DEC_STRM_ERROR\n");
        jpeg_ret = DEC_STRM_ERROR;
        goto strm_error;
      }

      if(stream_in_file < len) {
        len = stream_in_file;
      }

      /* update the buffer size in case last buffer
         doesn't have the same amount of data as defined */
      if(len < jpeg_in.buffer_size) {
        jpeg_in.buffer_size = len;
      }

#if 0
      /* Reading input file */
      f_in = fopen(argv[argc - 1], "rb");
      if(f_in == NULL) {
        fprintf(stdout, "Unable to open input file\n");
        exit(-1);
      }

      /* file i/o pointer to full */
      fseek(f_in, stream_seek_len, SEEK_SET);
      /* read input stream from file to buffer and close input file */
      ret = fread(byte_strm_start, sizeof(u8), len, f_in);
      (void) ret;
      fclose(f_in);

      /* update */
      jpeg_in.stream_buffer.virtual_address = (u32 *) byte_strm_start;
      jpeg_in.stream_buffer.bus_address = stream_mem.bus_address;
      jpeg_in.strm_len = stream_in_file;
#else
      /* Don't need to read a new picture from file again and again.
         Just use the buffer with whole stream in it. */
      byte_strm_start += image_info_length;
      if (params.is_ringbuffer) {
        if (byte_strm_start > (u8 *)jpeg_in.stream_buffer.virtual_address +
            jpeg_in.stream_buffer.logical_size)
          byte_strm_start -= jpeg_in.stream_buffer.logical_size;
      }

      /* update */
      jpeg_in.stream_buffer.bus_address = stream_mem.bus_address;
      jpeg_in.stream_buffer.virtual_address = (u32 *) stream_mem.virtual_address;
      jpeg_in.stream = byte_strm_start;
      jpeg_in.strm_len = stream_in_file;
#endif

      /* loop back to start */
    } else {
      if (frame_len_index >= PERFETCH_EOI_NUM) {
        jpeg_ret = FindImageEOI(byte_strm_start + temp_len, len - temp_len,
            &frame_len, byte_strm_start + temp_len, len - temp_len);
        if(jpeg_ret != 0) {
          printf("[TB] NO MORE IMAGES!\n");
          goto end;
        }
      } else {
        frame_len = frame_len_prefetch[frame_len_index++];
      }
      {
        int id = GetFreeStreamBuffer();
        jpeg_in.stream_buffer = stream_mem_input[id];
        jpeg_in.stream = (u8 *)stream_mem_input[id].virtual_address;
        /* stream processed callback param */
        jpeg_in.p_user_data = stream_mem_input[id].virtual_address;
      }
      jpeg_in.strm_len = frame_len;
      DWLmemcpy(jpeg_in.stream_buffer.virtual_address, byte_strm_start + temp_len, frame_len);
      temp_len += frame_len;
    }
    goto decode;
  }

end:

  if(low_latency) {
    process_end_flag = 1;
    wait_for_task_completion(task);
  }

  /******** PHASE 6 ********/
  fprintf(stdout, "\n[TB] Phase 6: RELEASE JPEG DECODER\n");

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!config.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      if (config.table_3dlut_buffer.virtual_address) {
        DWLFreeLinear(dwl, &config.table_3dlut_buffer);
        config.table_3dlut_buffer.virtual_address = NULL;
      }
    }
  }

  /* reset output write option */
  progressive = 0;
  VCDecEndOfStream(jpeg);

  if(output_thread_run) {
    pthread_join(output_thread, NULL);
    output_thread_run = 0;
  }
  /* have to release stream buffers before releasing decoder as we need DWL */
  sem_destroy(&stream_buff_free);
  sem_destroy(&frame_buff_free);
  for(i = 0; i < allocated_buffers; i++) {
    if(stream_mem_input[i].virtual_address != NULL) {
      DWLFreeLinear(dwl, &stream_mem_input[i]);
    }
  }

  if (ri_array)
    DWLfree(ri_array);

  if(stream_mem.bus_address != 0)
    DWLFreeLinear(dwl, &stream_mem);

  if (!params.mc_enable) {
    if (params.instant_buffer) {
      if(user_alloc_output_buffer[0].bus_address != 0)
        DWLFreeRefFrm(dwl, &user_alloc_output_buffer[0]);
    } else {
      if(ext_buffers[0].bus_address != 0)
          DWLFreeLinear(dwl, &ext_buffers[0]);
    }
  } else {
    if(params.instant_buffer) {
      for (i = 0; i < output_buffers; i++) {
        if(user_alloc_output_buffer[i].bus_address != 0)
          DWLFreeLinear(dwl, &user_alloc_output_buffer[i]);
        user_alloc_output_buffer[i].virtual_address = NULL;
      }
    } else {
      pthread_mutex_lock(&ext_buffer_contro);
      for(i=0; i < output_buffers; i++) {
        if(ext_buffers[i].bus_address != 0)
          DWLFreeLinear(dwl, &ext_buffers[i]);
        DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
      }
      pthread_mutex_unlock(&ext_buffer_contro);
    }
  }
  /* release decoder instance */
  START_SW_PERFORMANCE;
  VCDecRelease(jpeg);
  END_SW_PERFORMANCE;

  #ifdef ASIC_ONL_SIM
       dec_done = 1;
//  #else
//     sem_post(dec_done);
  #endif

  fprintf(stdout, "[TB] PHASE 6: RELEASE JPEG DECODER successful\n\n");

  /* check if (thumbnail + full) ==> decode all full images */
  if(ThumbDone && nbr_of_images) {
    prev_ret = DEC_STRM_ERROR;
    goto start_full_decode;
  }

  if(input_read_type) {
    if(f_in) {
      fclose(f_in);
    }
  }

  if( f_stream_trace)
    fclose(f_stream_trace);

  #ifdef ASIC_ONL_SIM
  while(!dec_done) {
    usleep(100);
  }
//  #else
//  sem_wait(dec_done);
  #endif

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  /* Leave properly */
  JpegDecFree(p_image);

  pthread_mutex_destroy(&ext_buffer_contro);

  DWLRelease(dwl);
  VCDecLogDestory();
  FINALIZE_SW_PERFORMANCE;

  PrintOutputFileName(&params);
  if (tmp_params.out_file_name[0] != NULL) {
    printf("[TB] Output Thumbnail file: %s\n", tmp_params.out_file_name[0]);
  }
  FreeOutputFileName(&params);
  FreeOutputFileName(&tmp_params);
  for (i = 0; i < sizeof(yuvsink) / sizeof(yuvsink[0]); i++) {
    if (yuvsink[i]) ReleaseYuvSink(yuvsink[i]);
  }

  /* amounf of decoded frames */
  if(nbr_of_images_to_out) {
    fprintf(stdout, "[TB] Information of decoded pictures:\n");
    fprintf(stdout, "[TB] \tPictures decoded: \t%u of total %u\n", nbr_of_images_to_out, nbr_of_images_total);
    if(nbr_of_thumb_images_to_out)
      fprintf(stdout, "[TB] \tThumbnails decoded: \t%u of total %u\n", nbr_of_thumb_images_to_out, nbr_of_thumb_images_total);

    if( (nbr_of_images_to_out != nbr_of_images_total) ||
        (nbr_of_thumb_images_to_out != nbr_of_thumb_images_total)) {
      fprintf(stdout, "\n[TB] \t-NOTE! \tCheck decoding log for the reason of \n");
      fprintf(stdout, "[TB] \t\tnot decoded, failed or unsupported pictures!\n");
      /* only full resolution */
      if(only_full_resolution)
        fprintf(stdout,"\n[TB] \t-NOTE! Forced by user to decode only full resolution image!\n");
    }

    fprintf(stdout, "[TB] \n");
  }

  fprintf(stdout, "[TB] TB: ...released\n");

  return_status = 0;
#ifdef ASIC_ONL_SIM
	  dpi_display_info("case_end","NULL", MultiStreamId);
#endif
return_:

#ifdef __FREERTOS__
#ifdef FREERTOS_SIMULATOR
  vTaskEndScheduler(); // need to open in simulator, and need to close on real env
#endif
  UNUSED(return_status);

  return (void *)NULL;
#else
  return return_status;
#endif
}

/*------------------------------------------------------------------------------

    Function name:  WriteOutputLuma

    Purpose:
        Write picture pointed by data to file. Size of the
        picture in pixels is indicated by picSize.

------------------------------------------------------------------------------*/
void WriteOutputLuma(u8 * data_luma, u32 pic_size_luma, u32 pic_mode) {
  u32 i;
  FILE *foutput = NULL;
  u8 *p_yuv_out = NULL;

  /* foutput is global file pointer */
  if(foutput == NULL) {
    if(pic_mode == 0) {
      foutput = fopen("out.yuv", "ab");
    } else {
      foutput = fopen("out_tn.yuv", "ab");
    }

    if(foutput == NULL) {
      fprintf(stdout, "[TB] UNABLE TO OPEN OUTPUT FILE\n");
      return;
    }
  }

  if(foutput && data_luma) {
    if(1) {
      fprintf(stdout, "[TB] \t-JPEG: Luminance\n");
      /* write decoder output to file */
      p_yuv_out = data_luma;
      for(i = 0; i < (pic_size_luma >> 2); i++) {
#ifndef ASIC_TRACE_SUPPORT
        if(DEC_X170_BIG_ENDIAN == output_picture_endian) {
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput);
        } else {
#endif
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput);
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput);
#ifndef ASIC_TRACE_SUPPORT
        }
#endif
      }
    }
  }

  fclose(foutput);
}

/*------------------------------------------------------------------------------

    Function name:  WriteOutputChroma

    Purpose:
        Write picture pointed by data to file. Size of the
        picture in pixels is indicated by picSize.

------------------------------------------------------------------------------*/
void WriteOutputChroma(u8 * data_chroma, u32 pic_size_chroma, u32 pic_mode) {
  u32 i;
  FILE *foutput_chroma = NULL;
  u8 *p_yuv_out = NULL;

  /* file pointer */
  if(foutput_chroma == NULL) {
    if(pic_mode == 0) {
      if(!progressive) {
        if(full_slice_counter == 0)
          foutput_chroma = fopen("out_chroma.yuv", "wb");
        else
          foutput_chroma = fopen("out_chroma.yuv", "ab");
      } else {
        if(!sliced_output_used) {
          foutput_chroma = fopen("out_chroma.yuv", "wb");
        } else {
          if(scan_counter == 0 || full_slice_counter == 0)
            foutput_chroma = fopen("out_chroma.yuv", "wb");
          else
            foutput_chroma = fopen("out_chroma.yuv", "ab");
        }
      }
    } else {
      if(!progressive) {
        if(full_slice_counter == 0)
          foutput_chroma = fopen("out_chroma_tn.yuv", "wb");
        else
          foutput_chroma = fopen("out_chroma_tn.yuv", "ab");
      } else {
        if(!sliced_output_used) {
          foutput_chroma = fopen("out_chroma_tn.yuv", "wb");
        } else {
          if(scan_counter == 0 || full_slice_counter == 0)
            foutput_chroma = fopen("out_chroma_tn.yuv", "wb");
          else
            foutput_chroma = fopen("out_chroma_tn.yuv", "ab");
        }
      }
    }

    if(foutput_chroma == NULL) {
      fprintf(stdout, "[TB] UNABLE TO OPEN OUTPUT FILE\n");
      return;
    }
  }

  if(foutput_chroma && data_chroma) {
    fprintf(stdout, "[TB] \t-JPEG: Chrominance\n");
    /* write decoder output to file */
    p_yuv_out = data_chroma;

    if(!progressive) {
      for(i = 0; i < (pic_size_chroma >> 2); i++) {
#ifndef ASIC_TRACE_SUPPORT
        if(DEC_X170_BIG_ENDIAN == output_picture_endian) {
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput_chroma);
        } else {
#endif
          fwrite(p_yuv_out + (4 * i) + 0, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 1, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 2, sizeof(u8), 1, foutput_chroma);
          fwrite(p_yuv_out + (4 * i) + 3, sizeof(u8), 1, foutput_chroma);
#ifndef ASIC_TRACE_SUPPORT
        }
#endif
      }
    } else {
      printf("[TB] PROGRESSIVE PLANAR OUTPUT CHROMA\n");
      for(i = 0; i < pic_size_chroma; i++)
        fwrite(p_yuv_out + (1 * i), sizeof(u8), 1, foutput_chroma);
    }
  }
  fclose(foutput_chroma);
}

/*------------------------------------------------------------------------------

    Function name:  WriteFullOutput

    Purpose:
        Write picture pointed by data to file.

------------------------------------------------------------------------------*/
void WriteFullOutput(char** out_file_name, u32 pic_mode) {
  u32 i;
  FILE *foutput = NULL;
  u8 *p_yuv_out_chroma = NULL;
  FILE *f_input_chroma = NULL;
  u32 length = 0;
  u32 chroma_len = 0;
  int ret;
  char filename[NAME_MAX + 8];

  fprintf(stdout, "[TB] \t-JPEG: WriteFullOutput\n");

  /* if semi-planar output */
  if(!planar_output) {
    /* Reading chroma file */
    if(pic_mode == 0)
      ret = system("cat out_chroma.yuv >> out.yuv");
    else
      ret = system("cat out_chroma_tn.yuv >> out_tn.yuv");
  } else {
    /* Reading chroma file */
    if(pic_mode == 0)
      f_input_chroma = fopen("out_chroma.yuv", "rb");
    else
      f_input_chroma = fopen("out_chroma_tn.yuv", "rb");

    if(f_input_chroma == NULL) {
      fprintf(stdout, "[TB] Unable to open chroma output tmp file\n");
      exit(-1);
    }

    /* file i/o pointer to full */
    fseek(f_input_chroma, 0L, SEEK_END);
    length = ftell(f_input_chroma);
    rewind(f_input_chroma);

    /* check length */
    chroma_len = length;

    p_yuv_out_chroma = JpegDecMalloc(sizeof(u8) * (chroma_len));
    if (p_yuv_out_chroma == NULL) {
      fclose(f_input_chroma);
	  return;
    }
    /* read output stream from file to buffer and close input file */
    ret = fread(p_yuv_out_chroma, sizeof(u8), chroma_len, f_input_chroma);
    (void) ret;

    fclose(f_input_chroma);

    /* foutput is global file pointer */
    if(pic_mode == 0)
      foutput = fopen(out_file_name[0], "ab");
    else {
      strncpy(filename, out_file_name[0], NAME_MAX);
      filename[NAME_MAX] = '\0';
      sprintf(filename+strlen(filename), "_tn.yuv");
      foutput = fopen(filename, "ab");
    }
    if(foutput == NULL) {
      fprintf(stdout, "[TB] UNABLE TO OPEN OUTPUT FILE\n");
	    JpegDecFree(p_yuv_out_chroma);
      return;
    }

    if(foutput && p_yuv_out_chroma) {
      fprintf(stdout, "[TB] \t-JPEG: Chrominance\n");
      if(!progressive) {
        if(!planar_output) {
          /* write decoder output to file */
          for(i = 0; i < (chroma_len >> 2); i++) {
            fwrite(p_yuv_out_chroma + (4 * i) + 0, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 1, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 2, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 3, sizeof(u8), 1, foutput);
          }
        } else {
          for(i = 0; i < chroma_len / 2; i++)
            fwrite(p_yuv_out_chroma + 2 * i, sizeof(u8), 1, foutput);
          for(i = 0; i < chroma_len / 2; i++)
            fwrite(p_yuv_out_chroma + 2 * i + 1, sizeof(u8), 1, foutput);
        }
      } else {
        if(!planar_output) {
          /* write decoder output to file */
          for(i = 0; i < (chroma_len >> 2); i++) {
            fwrite(p_yuv_out_chroma + (4 * i) + 0, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 1, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 2, sizeof(u8), 1, foutput);
            fwrite(p_yuv_out_chroma + (4 * i) + 3, sizeof(u8), 1, foutput);
          }
        } else {
          printf("[TB] PROGRESSIVE FULL CHROMA %u\n", chroma_len);
          for(i = 0; i < chroma_len; i++)
            fwrite(p_yuv_out_chroma + i, sizeof(u8), 1, foutput);
        }
      }
    }
    fclose(foutput);

    /* Leave properly */
    JpegDecFree(p_yuv_out_chroma);
  }
}

/*------------------------------------------------------------------------------

    Function name:  handleSlicedOutput

    Purpose:
        Calculates size for slice and writes sliced output

------------------------------------------------------------------------------*/
void handleSlicedOutput(struct DecSequenceInfo * image_info,
                   struct DecInputParameters* jpeg_in, struct DecPictures* jpeg_out) {
  struct DecPicture* in = &jpeg_out->pictures[0];
  u32 ext_id = 0xFFFF;
  u32* host_base = NULL;
  /* for output name */
  full_slice_counter++;

  /******** PHASE X ********/
  if(jpeg_in->slice_mb_set)
    fprintf(stdout, "\n[TB] Phase SLICE: HANDLE SLICE %d\n", full_slice_counter);

  /* save start pointers for whole output */
  if(full_slice_counter == 0) {
    /* virtual address */
    output_address_y.virtual_address =
      in->luma.virtual_address;
    output_address_cb_cr.virtual_address =
      in->chroma.virtual_address;

    /* bus address */
    output_address_y.bus_address = in->luma.bus_address;
    output_address_cb_cr.bus_address = in->chroma.bus_address;
  }

  /* if output write not disabled by TB */
  if(write_output) {
    /******** PHASE 5 ********/
    fprintf(stdout, "\n[TB] Phase 5: WRITE OUTPUT\n");

    if(image_info->output_format) {
      if(!frame_ready) {
        slice_size = jpeg_in->slice_mb_set * 16;
      } else {
        if(mode == 0)
          slice_size =
            (image_info->pic_height -
             ((full_slice_counter) * (slice_size)));
        else
          slice_size =
            (image_info->pic_height_thumb -
             ((full_slice_counter) * (slice_size)));
      }
    }

    /* slice interrupt from decoder */
    slice_to_user = 1;

    /* calculate size for output */
    calcSize(image_info, mode, jpeg_out, 0);

    /* test printf */
    fprintf(stdout, "[TB] \t-JPEG: ++++++++++ SLICE INFORMATION ++++++++++\n");
    fprintf(stdout, "[TB] \t-JPEG: Luma output: %p size: %u\n",
            (void *)jpeg_out->pictures[0].luma.virtual_address, size_luma);
    fprintf(stdout, "[TB] \t-JPEG: Chroma output: %p size: %u\n",
            (void *)jpeg_out->pictures[0].chroma.virtual_address, size_chroma);
    fprintf(stdout, "[TB] \t-JPEG: Luma output bus: 0x%16llx\n",
             jpeg_out->pictures[0].luma.bus_address);
    fprintf(stdout, "[TB] \t-JPEG: Chroma output bus: 0x%16llx\n",
             jpeg_out->pictures[0].chroma.bus_address);

    /* write slice output */
    SwMatchOuputBufferId(&tool_params, &in->luma, &ext_id);
    if (ext_id < MAX_BUFFERS) {
      if (tool_params.ext_buffers[ext_id].virtual_address == NULL) {
        host_base = DWLmalloc(tool_params.ext_buffers[ext_id].size);
        tool_params.ext_buffers[ext_id].virtual_address = host_base;
      }
      DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                      tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
    }
    SwSetHostOutbaseAfterDma(host_base, in, &tool_params.ext_buffers[ext_id]);

    if (!jpeg_in->dec_image_type) {
      yuvsink[mode]->WritePicture(yuvsink[mode]->inst, &jpeg_out->pictures[0], 0);
    } else {
      yuvsink[mode]->WritePicture(yuvsink[mode]->inst, &jpeg_out->pictures[0], 0);
    }
    /* write luma to final output file */
    WriteOutputLuma(((u8 *) in->luma.virtual_address), size_luma, mode);

    if(image_info->output_format != DEC_OUT_FRM_YUV400) {
      /* write chroam to tmp file */
      WriteOutputChroma(((u8 *) in->chroma.virtual_address), size_chroma, mode);
    }
    SwClearHostOutbaseAfterWriteFile(host_base, in);
    if (host_base != NULL) {
      DWLfree(host_base);
      tool_params.ext_buffers[ext_id].virtual_address = NULL;
    }
    fprintf(stdout, "[TB] PHASE 5: WRITE OUTPUT successful\n");
  } else {
    fprintf(stdout, "\n[TB] Phase 5: WRITE OUTPUT DISABLED\n");
  }

  if(frame_ready) {
    /* give start pointers for whole output write */

    /* virtual address */
    in->luma.virtual_address =
      output_address_y.virtual_address;
    in->chroma.virtual_address =
      output_address_cb_cr.virtual_address;

    /* bus address */
    in->luma.bus_address = output_address_y.bus_address;
    in->chroma.bus_address = output_address_cb_cr.bus_address;
  }

  if(frame_ready) {
    frame_ready = 0;
    slice_to_user = 0;

    /******** PHASE X ********/
    if(jpeg_in->slice_mb_set)
      fprintf(stdout, "\n[TB] Phase SLICE: HANDLE SLICE %d successful\n",
              full_slice_counter);

    full_slice_counter = -1;
  } else {
    /******** PHASE X ********/
    if(jpeg_in->slice_mb_set)
      fprintf(stdout, "\n[TB] Phase SLICE: HANDLE SLICE %d successful\n",
              full_slice_counter);
  }

}

/*------------------------------------------------------------------------------

    Function name:  calcSize

    Purpose:
        Calculate size

------------------------------------------------------------------------------*/
void calcSize(struct DecSequenceInfo * image_info, u32 pic_mode, struct DecPictures* jpeg_out, u32 i) {
  size_luma = 0;
  size_chroma = 0;
  if(pp_enabled) {
    if(pic_mode == 0) {
      size_luma = jpeg_out->pictures[i].sequence_info.pic_width * jpeg_out->pictures[i].sequence_info.pic_height;
    } else {
      size_luma = jpeg_out->pictures[i].sequence_info.pic_width_thumb * jpeg_out->pictures[i].sequence_info.pic_height_thumb;
    }
    size_chroma = (size_luma / 2);
  }
}

/*------------------------------------------------------------------------------

    Function name:  allocMemory

    Purpose:
        Allocates user specific memory for output.

------------------------------------------------------------------------------*/
u32
allocMemory(JpegDecInst dec_inst, struct DecSequenceInfo * image_info,
            struct DecInputParameters* jpeg_in, const void *dwl, u64 buffer_size) {

  out_pic_size_luma = 0;
  out_pic_size_chroma = 0;
  jpeg_in->picture_buffer_y.virtual_address = NULL;
  jpeg_in->picture_buffer_y.bus_address = 0;
  jpeg_in->picture_buffer_cb_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cb_cr.bus_address = 0;
  jpeg_in->picture_buffer_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cr.bus_address = 0;

#if 1
  {
    fprintf(stdout, "[TB] \t\t-JPEG: USER OUTPUT MEMORY ALLOCATION\n");

    jpeg_in->picture_buffer_y.virtual_address = NULL;
    jpeg_in->picture_buffer_cb_cr.virtual_address = NULL;
    jpeg_in->picture_buffer_cr.virtual_address = NULL;

    /**** memory area ****/

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    user_alloc_output_buffer[0].virtual_address = NULL;
    user_alloc_output_buffer[0].bus_address = 0;

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    SET_MEM_USAGE(user_alloc_output_buffer[0].mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
    if(DWLMallocRefFrm
        (dwl, buffer_size,
         &user_alloc_output_buffer[0]) != DWL_OK) {
      fprintf(stdout, "[TB] UNABLE TO ALLOCATE USER LUMA OUTPUT MEMORY\n");
      return DEC_MEMFAIL;
    }

    /* Luma Bus */
    jpeg_in->picture_buffer_y.virtual_address = user_alloc_output_buffer[0].virtual_address;
    jpeg_in->picture_buffer_y.bus_address = user_alloc_output_buffer[0].bus_address;
    jpeg_in->picture_buffer_y.logical_size = user_alloc_output_buffer[0].logical_size;
    jpeg_in->picture_buffer_y.mem_type = user_alloc_output_buffer[0].mem_type;
    jpeg_in->picture_buffer_y.size = user_alloc_output_buffer[0].size;

    /* memset output to gray */
    DWLLinearMemset(dwl, &jpeg_in->picture_buffer_y, 0, 128, jpeg_in->picture_buffer_y.size);
  }
#endif /* #ifdef LINUX */

#if 0
  {
    fprintf(stdout, "\t\t-JPEG: MALLOC\n");

    /* allocate luma */
    jpeg_in->picture_buffer_y.virtual_address =
      (u32 *) JpegDecMalloc(sizeof(u8) * out_pic_size_luma);

    JpegDecMemset(jpeg_in->picture_buffer_y.virtual_address, 128,
                  out_pic_size_luma);

    /* allocate chroma */
    if(out_pic_size_chroma) {
      jpeg_in->picture_buffer_cb_cr.virtual_address =
        (u32 *) JpegDecMalloc(sizeof(u8) * out_pic_size_chroma);

      JpegDecMemset(jpeg_in->picture_buffer_cb_cr.virtual_address, 128,
                    out_pic_size_chroma);
    }
  }
#endif /* #ifndef LINUX */

  /*fprintf(stdout, "\t\t-JPEG: Allocate: Luma virtual %zx bus %zx size %d\n",
          (addr_t)jpeg_in->picture_buffer_y.virtual_address,
          jpeg_in->picture_buffer_y.bus_address, out_pic_size_luma);

  if(separate_chroma == 0) {
    fprintf(stdout,
            "\t\t-JPEG: Allocate: Chroma virtual %zx bus %zx size %d\n",
            (addr_t)jpeg_in->picture_buffer_cb_cr.virtual_address,
            jpeg_in->picture_buffer_cb_cr.bus_address, out_pic_size_chroma);
  } else {
    fprintf(stdout,
            "\t\t-JPEG: Allocate: Cb virtual %zx bus %zx size %d\n",
            (addr_t)jpeg_in->picture_buffer_cb_cr.virtual_address,
            jpeg_in->picture_buffer_cb_cr.bus_address,
            (out_pic_size_chroma / 2));

    fprintf(stdout,
            "\t\t-JPEG: Allocate: Cr virtual %zx bus %zx size %d\n",
            (addr_t)jpeg_in->picture_buffer_cr.virtual_address,
            jpeg_in->picture_buffer_cr.bus_address, (out_pic_size_chroma / 2));
  }*/

  return DEC_OK;
}


u32 inputMemoryAlloc(JpegDecInst dec_inst, struct DecSequenceInfo * image_info,
                     struct DecInputParameters* jpeg_in, const void *dwl, struct DecBufferInfo *mem_info) {
  u32 i, idx;
  out_pic_size_luma = 0;
  out_pic_size_chroma = 0;
  jpeg_in->picture_buffer_y.virtual_address = NULL;
  jpeg_in->picture_buffer_y.bus_address = 0;
  jpeg_in->picture_buffer_cb_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cb_cr.bus_address = 0;
  jpeg_in->picture_buffer_cr.virtual_address = NULL;
  jpeg_in->picture_buffer_cr.bus_address = 0;

  if (user_alloc_output_buffer[0].virtual_address == NULL) {
    for (i = 0; i < output_buffers; i++) {
      user_alloc_output_buffer[i].mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
      user_alloc_output_buffer[i].mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
      if(DWLMallocLinear(dwl, mem_info->next_buf_size,
                         &user_alloc_output_buffer[i]) != 0)
        return (DEC_MEMFAIL);
    }
  }

  sem_wait(&frame_buff_free);
  pthread_mutex_lock(&frm_buff_stat_lock);
  idx = 0;
  while(frame_mem_status[idx]) {
    idx++;
  }
  assert(idx < output_buffers);
  frame_mem_status[idx] = 1;

  /* reallocate buffer if needed. */
  if (mem_info->buf_to_free.bus_address != 0) {
    DWLFreeLinear(dwl, &user_alloc_output_buffer[idx]);
    user_alloc_output_buffer[idx].mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
    user_alloc_output_buffer[idx].mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
    SET_MEM_USAGE(user_alloc_output_buffer[idx].mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
    if(DWLMallocLinear(dwl, mem_info->next_buf_size, &user_alloc_output_buffer[idx]) != 0) {
	  pthread_mutex_unlock(&frm_buff_stat_lock);
      return (DEC_MEMFAIL);
    }
  }
  pthread_mutex_unlock(&frm_buff_stat_lock);

  /* Luma Bus */
  jpeg_in->picture_buffer_y.virtual_address = user_alloc_output_buffer[idx].virtual_address;
  jpeg_in->picture_buffer_y.bus_address = user_alloc_output_buffer[idx].bus_address;
  jpeg_in->picture_buffer_y.logical_size = user_alloc_output_buffer[idx].logical_size;
  jpeg_in->picture_buffer_y.size = user_alloc_output_buffer[idx].size;
  jpeg_in->picture_buffer_y.mem_type = user_alloc_output_buffer[idx].mem_type;
  /* memset output to gray */
  DWLLinearMemset(dwl, &jpeg_in->picture_buffer_y, 0, 128, jpeg_in->picture_buffer_y.size);

#if 0
  if (user_alloc_chroma[0].size == 0) {
    for (i = 0; i < output_buffers; i++) {
      if(DWLMallocLinear(dwl,
                         out_pic_size_chroma, &user_alloc_chroma[i]) != 0) {
        free (&user_alloc_luma[i]);
        return (DEC_MEMFAIL);
      }
    }
  }

  /* Chroma Bus */
  jpeg_in->picture_buffer_cb_cr.virtual_address =
    user_alloc_chroma[idx].virtual_address;
  jpeg_in->picture_buffer_cb_cr.bus_address =
    user_alloc_chroma[idx].bus_address;

  /* memset output to gray */
  (void) DWLmemset(jpeg_in->picture_buffer_cb_cr.virtual_address, 128,
                   out_pic_size_chroma);
  /* Chroma Bus */
  jpeg_in->picture_buffer_cb_cr.virtual_address =
    user_alloc_luma[idx].virtual_address + out_pic_size_luma / 4;
  jpeg_in->picture_buffer_cb_cr.bus_address =
    user_alloc_chroma[idx].bus_address + out_pic_size_luma;
#endif
  return DEC_OK;
}
/*-----------------------------------------------------------------------------

Print JPEG api return value

-----------------------------------------------------------------------------*/
void PrintJpegRet(enum DecRet jpeg_ret) {
  static enum DecRet prev_retval = 0xFFFFFF;
  static enum DecRet prev_ret = DEC_NO_DECODING_BUFFER;
  if (jpeg_ret == prev_ret) return;
  prev_ret = jpeg_ret;
  switch (jpeg_ret) {
  case DEC_PIC_RDY:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_PIC_RDY\n");
    break;
  case DEC_OK:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_OK\n");
    break;
  case DEC_ERROR:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_ERROR\n");
    break;
  case DEC_HW_TIMEOUT:
    fprintf(stdout, "[TB]  jpeg API returned : JPEGDEC_HW_TIMEOUT\n");
    break;
  case DEC_UNSUPPORTED:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_UNSUPPORTED\n");
    break;
  case DEC_PARAM_ERROR:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_PARAM_ERROR\n");
    break;
  case DEC_MEMFAIL:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_MEMFAIL\n");
    break;
  case DEC_INITFAIL:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_INITFAIL\n");
    break;
  case DEC_HW_BUS_ERROR:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_HW_BUS_ERROR\n");
    break;
  case DEC_SYSTEM_ERROR:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_SYSTEM_ERROR\n");
    break;
  case DEC_DWL_ERROR:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_DWL_ERROR\n");
    break;
  case DEC_INVALID_STREAM_LENGTH:
    fprintf(stdout,
            "[TB]  jpeg API returned : DEC_INVALID_STREAM_LENGTH\n");
    break;
  case DEC_STRM_ERROR:
    fprintf(stdout, "[TB]  jpeg API returned : DEC_STRM_ERROR\n");
    break;
  case DEC_INVALID_INPUT_BUFFER_SIZE:
    fprintf(stdout,
            "[TB]  jpeg API returned : DEC_INVALID_INPUT_BUFFER_SIZE\n");
    break;
  case DEC_INCREASE_INPUT_BUFFER:
    fprintf(stdout,
            "[TB]  jpeg API returned : DEC_INCREASE_INPUT_BUFFER\n");
    break;
  case DEC_SLICE_MODE_UNSUPPORTED:
    fprintf(stdout,
            "[TB]  jpeg API returned : DEC_SLICE_MODE_UNSUPPORTED\n");
    break;
  case DEC_NO_DECODING_BUFFER:
    if (prev_retval == DEC_NO_DECODING_BUFFER) break;
    fprintf(stdout,
            "[TB]  jpeg API returned : DEC_NO_DECODING_BUFFER\n");
    break;
  case DEC_WAITING_FOR_BUFFER:
    fprintf(stdout,
            "[TB]  jpeg API returned : DEC_WAITING_FOR_BUFFER\n");
    break;
  case DEC_FORMAT_NOT_SUPPORTED:
    fprintf(stdout,
            "[TB] : jpeg API returned : DEC_FORMAT_NOT_SUPPORTED\n");
    break;
  case DEC_END_OF_STREAM:
    fprintf(stdout,
            "[TB] : jpeg API returned : DEC_END_OF_STREAM\n");
    break;
  default:
    fprintf(stdout, "[TB]  jpeg API returned unknown status\n");
    break;
  }
  prev_retval = jpeg_ret;
}

void PrintGetExifInfo(struct DecSequenceInfo * image_info) {
  fprintf(stdout, "[TB] \t-JPEG Exif INFO\n");

  fprintf(stdout, "[TB] \t-1)JPEG IFD0 INFO\n");
  fprintf(stdout, "[TB] \t\t-JPEG ImageDescription: %s\n", image_info->exif_info.ifd0_info.ImageDescription);
  fprintf(stdout, "[TB] \t\t-JPEG Artist: %s\n", image_info->exif_info.ifd0_info.Artist);
  fprintf(stdout, "[TB] \t\t-JPEG Camera Make: %s\n", image_info->exif_info.ifd0_info.CameraMake);
  fprintf(stdout, "[TB] \t\t-JPEG Camera Model: %s\n", image_info->exif_info.ifd0_info.CameraModel);
  fprintf(stdout, "[TB] \t\t-JPEG Orientation: %u\n", image_info->exif_info.ifd0_info.Orientation);//1:normal, obtain the normal image by rotating it
  fprintf(stdout, "[TB] \t\t-JPEG XResolution: %u\n", image_info->exif_info.ifd0_info.XResolution);
  fprintf(stdout, "[TB] \t\t-JPEG YResolution: %u\n", image_info->exif_info.ifd0_info.YResolution);
  fprintf(stdout, "[TB] \t\t-JPEG ResolutionUnit: %s\n", image_info->exif_info.ifd0_info.ResolutionUnit);
  fprintf(stdout, "[TB] \t\t-JPEG Software: %s\n", image_info->exif_info.ifd0_info.Software);
  fprintf(stdout, "[TB] \t\t-JPEG DateTime: %s\n", image_info->exif_info.ifd0_info.DateTime);
  fprintf(stdout, "[TB] \t\t-JPEG YCbCrPositioning: %u\n", image_info->exif_info.ifd0_info.YCbCrPositioning);
  fprintf(stdout, "[TB] \t\t-JPEG ExifOffset: %u\n", image_info->exif_info.ifd0_info.ExifOffset);

  fprintf(stdout, "[TB] \t-2)JPEG ExifSubIFD INFO\n");
  fprintf(stdout, "[TB] \t\t-JPEG ExposureTime: %u/%u s\n", image_info->exif_info.exif_subifd_info.ExposureTime.numerator,
                                                            image_info->exif_info.exif_subifd_info.ExposureTime.denominator);
  fprintf(stdout, "[TB] \t\t-JPEG FNumber: F%0.1f\n", image_info->exif_info.exif_subifd_info.FNumber);
  fprintf(stdout, "[TB] \t\t-JPEG ExposureProgram: %s\n", image_info->exif_info.exif_subifd_info.ExposureProgram);
  fprintf(stdout, "[TB] \t\t-JPEG ISOSpeedRatings: %u\n", image_info->exif_info.exif_subifd_info.ISOSpeedRatings);
  fprintf(stdout, "[TB] \t\t-JPEG ExifVersion: %s\n", image_info->exif_info.exif_subifd_info.ExifVersion);
  fprintf(stdout, "[TB] \t\t-JPEG DateTimeOriginal: %s\n", image_info->exif_info.exif_subifd_info.DateTimeOriginal);
  fprintf(stdout, "[TB] \t\t-JPEG DateTimeDigitized: %s\n", image_info->exif_info.exif_subifd_info.DateTimeDigitized);
  fprintf(stdout, "[TB] \t\t-JPEG ExposureBiasValue: %0.2f eV\n", image_info->exif_info.exif_subifd_info.ExposureBiasValue);
  fprintf(stdout, "[TB] \t\t-JPEG MaxApertureValue: %0.2f\n", image_info->exif_info.exif_subifd_info.MaxApertureValue);
  fprintf(stdout, "[TB] \t\t-JPEG MeteringMode: %s\n", image_info->exif_info.exif_subifd_info.MeteringMode);
  fprintf(stdout, "[TB] \t\t-JPEG Lightsource: %s\n", image_info->exif_info.exif_subifd_info.Lightsource);
  fprintf(stdout, "[TB] \t\t-JPEG Flash: %s\n", image_info->exif_info.exif_subifd_info.Flash);
  fprintf(stdout, "[TB] \t\t-JPEG FocalLength: %0.2f mm\n", image_info->exif_info.exif_subifd_info.FocalLength);
  fprintf(stdout, "[TB] \t\t-JPEG ColorSpace: %s\n", image_info->exif_info.exif_subifd_info.ColorSpace);
  fprintf(stdout, "[TB] \t\t-JPEG ExifImageWidth: %u\n", image_info->exif_info.exif_subifd_info.ExifImageWidth);
  fprintf(stdout, "[TB] \t\t-JPEG ExifImageHeight: %u\n", image_info->exif_info.exif_subifd_info.ExifImageHeight);

  fprintf(stdout, "[TB] \t-3)JPEG IFD1 INFO\n");
  fprintf(stdout, "[TB] \t\t-JPEG ImageWidth: %u\n", image_info->exif_info.ifd1_info.ImageWidth);
  fprintf(stdout, "[TB] \t\t-JPEG ImageHeight: %u\n", image_info->exif_info.ifd1_info.ImageHeight);
  fprintf(stdout, "[TB] \t\t-JPEG Compression: %s\n", image_info->exif_info.ifd1_info.Compression);
  fprintf(stdout, "[TB] \t\t-JPEG PhotometricInterpretation: %u\n", image_info->exif_info.ifd1_info.PhotometricInterpretation);
  fprintf(stdout, "[TB] \t\t-JPEG XResolution: %u\n", image_info->exif_info.ifd1_info.XResolution);
  fprintf(stdout, "[TB] \t\t-JPEG YResolution: %u\n", image_info->exif_info.ifd1_info.YResolution);
  fprintf(stdout, "[TB] \t\t-JPEG ResolutionUnit: %s\n", image_info->exif_info.ifd1_info.ResolutionUnit);
  fprintf(stdout, "[TB] \t\t-JPEG JpegIFOffset: %u\n", image_info->exif_info.ifd1_info.JpegIFOffset);
  fprintf(stdout, "[TB] \t\t-JPEG JpegIFByteCount: %u\n", image_info->exif_info.ifd1_info.JpegIFByteCount);
  fprintf(stdout, "[TB] \t\t-JPEG YCbCrCoefficients: %0.3f\n", image_info->exif_info.ifd1_info.YCbCrCoefficients);
  fprintf(stdout, "[TB] \t\t-JPEG YCbCrPositioning: %u\n", image_info->exif_info.ifd1_info.YCbCrPositioning);
}
/*-----------------------------------------------------------------------------

Print JpegDecGetImageInfo values

-----------------------------------------------------------------------------*/
void PrintGetImageInfo(struct DecSequenceInfo * image_info) {
  assert(image_info);

  /* Select if Thumbnail or full resolution image will be decoded */
  if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    /* decode thumbnail */
    fprintf(stdout, "[TB] \t-JPEG THUMBNAIL IN STREAM\n");
    fprintf(stdout, "[TB] \t-JPEG THUMBNAIL INFO\n");
    fprintf(stdout, "[TB] \t\t-JPEG thumbnail display resolution(W x H): %u x %u\n",
            image_info->scaled_width_thumb, image_info->scaled_height_thumb);
    fprintf(stdout, "[TB] \t\t-JPEG thumbnail HW decoded RESOLUTION(W x H): %u x %u\n",
            NEXT_MULTIPLE(image_info->scaled_width_thumb, 16),
            NEXT_MULTIPLE(image_info->scaled_height_thumb, 8));
    fprintf(stdout, "[TB] \t\t-JPEG thumbnail OUTPUT SIZE(Stride x H): %u x %u\n",
            image_info->pic_width_thumb, image_info->pic_height_thumb);

    /* stream type */
    if(image_info->coding_mode_thumb) {
      switch (image_info->coding_mode_thumb) {
      case JPEG_BASELINE:
        fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_BASELINE\n");
        break;
      case JPEG_PROGRESSIVE:
        fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_PROGRESSIVE\n");
        break;
      case JPEG_NONINTERLEAVED:
        fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_NONINTERLEAVED\n");
        break;
      case JPEG_EXTENDED:
        fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_EXTENDED\n");
        break;
      default:
        fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_NOT_SUPPORTED\n");
        break;
      }
    }

    if(image_info->output_format_thumb) {
      switch (image_info->output_format_thumb) {
      case DEC_OUT_FRM_YUV400:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: DEC_OUT_FRM_YUV400\n");
        break;
      case DEC_OUT_FRM_YUV420SP:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: DEC_OUT_FRM_YUV420SP\n");
        break;
      case DEC_OUT_FRM_YUV422SP:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: DEC_OUT_FRM_YUV422SP\n");
        break;
      case DEC_OUT_FRM_YUV440:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: DEC_OUT_FRM_YUV440\n");
        break;
      case DEC_OUT_FRM_YUV411SP:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: DEC_OUT_FRM_YUV411SP\n");
        break;
      case DEC_OUT_FRM_YUV444SP:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: DEC_OUT_FRM_YUV444SP\n");
        break;
      default:
        fprintf(stdout,
                "[TB] \t\t-JPEG: THUMBNAIL OUTPUT: NOT SUPPORT\n");
        break;
      }
    }
  } else if(image_info->thumbnail_type == JPEGDEC_NO_THUMBNAIL) {
    /* decode full image */
    fprintf(stdout,
            "[TB] \t-NO THUMBNAIL IN STREAM ==> Decode full resolution image\n");
  } else if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT) {
    /* decode full image */
    fprintf(stdout,
            "[TB] \tNot SUPPORTED THUMBNAIL IN STREAM ==> Decode full resolution image\n");
  }

  fprintf(stdout, "[TB] \t-JPEG FULL RESOLUTION INFO\n");
  fprintf(stdout, "[TB] \t\t-JPEG display resolution(W x H): %u x %u\n",
          image_info->scaled_width, image_info->scaled_height);
  fprintf(stdout, "[TB] \t\t-JPEG HW decoded RESOLUTION(W x H): %u x %u\n",
          NEXT_MULTIPLE(image_info->scaled_width, 8),
          NEXT_MULTIPLE(image_info->scaled_height, 8));
  fprintf(stdout, "[TB] \t\t-JPEG OUTPUT SIZE(Stride x H): %u x %u\n",
          image_info->pic_width, image_info->pic_height);
  if(image_info->output_format) {
    switch (image_info->output_format) {
    case DEC_OUT_FRM_YUV400:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: DEC_OUT_FRM_YUV400\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_0_0 = 1;
#endif
      break;
    case DEC_OUT_FRM_YUV420SP:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: DEC_OUT_FRM_YUV420SP\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_2_0 = 1;
#endif
      break;
    case DEC_OUT_FRM_YUV422SP:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: DEC_OUT_FRM_YUV422SP\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_2_2 = 1;
#endif
      break;
    case DEC_OUT_FRM_YUV440:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: DEC_OUT_FRM_YUV440\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_4_0 = 1;
#endif
      break;
    case DEC_OUT_FRM_YUV411SP:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: DEC_OUT_FRM_YUV411SP\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_1_1 = 1;
#endif
      break;
    case DEC_OUT_FRM_YUV444SP:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: DEC_OUT_FRM_YUV444SP\n");
#ifdef ASIC_TRACE_SUPPORT
      decoding_tools.sampling_4_4_4 = 1;
#endif
      break;
    default:
      fprintf(stdout,
              "[TB] \t\t-JPEG: FULL RESOLUTION OUTPUT: NOT SUPPORT\n");
      break;
    }
  }

  /* stream type */
  switch (image_info->coding_mode) {
  case JPEG_BASELINE:
    fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_BASELINE\n");
    break;
  case JPEG_PROGRESSIVE:
    fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_PROGRESSIVE\n");
#ifdef ASIC_TRACE_SUPPORT
    decoding_tools.progressive = 1;
#endif
    break;
  case JPEG_NONINTERLEAVED:
    fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_NONINTERLEAVED\n");
    break;
  case JPEG_EXTENDED:
    fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_EXTENDED\n");
    break;
  default:
    fprintf(stdout, "[TB] \t\t-JPEG: STREAM TYPE: JPEG_NOT_SUPPORTED");
    break;
  }

  if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_JPEG) {
    fprintf(stdout, "[TB] \t-JPEG ThumbnailType: JPEG\n");
#ifdef ASIC_TRACE_SUPPORT
    decoding_tools.thumbnail = 1;
#endif
  } else if(image_info->thumbnail_type == JPEGDEC_NO_THUMBNAIL)
    fprintf(stdout, "[TB] \t-JPEG ThumbnailType: NO THUMBNAIL\n");
  else if(image_info->thumbnail_type == JPEGDEC_THUMBNAIL_NOT_SUPPORTED_FORMAT)
    fprintf(stdout, "[TB] \t-JPEG ThumbnailType: NOT SUPPORTED THUMBNAIL\n");
  if (jpeg_exif_in_stream && !ThumbDone)
    PrintGetExifInfo(image_info);
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecMalloc

------------------------------------------------------------------------------*/
void *JpegDecMalloc(unsigned int size) {
  void *mem_ptr = (char *) malloc(size);

  return mem_ptr;
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecMemset

------------------------------------------------------------------------------*/
/*
void *JpegDecMemset(void *ptr, int c, unsigned int size) {
  void *rv = NULL;

  if(ptr != NULL) {
    rv = memset(ptr, c, size);
  }
  return rv;
}
*/

/*------------------------------------------------------------------------------

    Function name:  JpegDecFree

------------------------------------------------------------------------------*/
void JpegDecFree(void *ptr) {
  if(ptr != NULL) {
    free(ptr);
  }
}

/*------------------------------------------------------------------------------

    Function name:  JpegDecTrace

    Purpose:
        Example implementation of JpegDecTrace function. Prototype of this
        function is given in jpegdecapi.h. This implementation appends
        trace messages to file named 'dec_api.trc'.

------------------------------------------------------------------------------*/
void JpegDecTrace(const char *string) {
  FILE *fp;

  /* in MC mode, the operation to this file cause sgement fault. TBD */
  if (mc_enable)
    return;

  fp = fopen("dec_api.trc", "at");

  if(!fp)
    return;

  fwrite(string, 1, strlen(string), fp);
  fwrite("\n", 1, 1, fp);

  fclose(fp);
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageInfoEnd

    Purpose:
        Finds 0xFFC4 from the stream and p_offset includes number of bytes to
        this marker. In case of an error returns != 0
        (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageInfoEnd(u8 * stream, u32 stream_length, u32 * p_offset,
                     u8 * buffer, u32 buf_len) {
  u32 i;

  *p_offset = 0;
  for(i = 0; i < stream_length; ++i) {
    if(0xFF == GetBytes(stream, i, buffer, buf_len)) {
      if(((i + 1) < stream_length) &&
          0xC4 == GetBytes(stream, i + 1, buffer, buf_len)) {
        *p_offset = i;
        return 0;
      }
    }
  }
  return -1;
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageEnd

    Purpose:
        Finds 0xFFD9 from the stream and p_offset includes number of bytes to
        this marker. In case of an error returns != 0
        (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageEnd(u8 * stream, u32 stream_length, u32 * p_offset,
                 u8 * buffer, u32 buf_len) {
  u32 i,j;
  u32 jpeg_thumb_in_stream = 0;
  u32 tmp, tmp1, tmp_total = 0;
  u32 last_marker = 0;

  *p_offset = 0;
  for(i = 0; i < stream_length; ++i) {
    if(0xFF == GetBytes(stream, i, buffer, buf_len)) {
      /* if 0xFFE1 to 0xFFFD ==> skip  */
      if(
        /* (((i + 1) < stream_length) &&
          0xE1 == GetBytes(stream, i + 1, buffer, buf_len)) || */
          (((i + 1) < stream_length) &&
          0xE2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE4 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEA == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEB == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEC == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xED == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEE == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEF == GetBytes(stream, i + 1, buffer, buf_len)) /*||
          (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFD == stream[i + 1]) */) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = GetBytes(stream, i, buffer, buf_len);
        tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += (tmp_total-1);
        continue;
      }

      /* if 0xFFC2 to 0xFFCB ==> skip  */
      if( (((i + 1) < stream_length) &&
          0xC0 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC1 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xCA == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xCB == GetBytes(stream, i + 1, buffer, buf_len)) ) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = GetBytes(stream, i, buffer, buf_len);
        tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total;

        /* look for EOI */
        for(j = i; j < stream_length;++j) {
          if(0xFF == GetBytes(stream, j, buffer, buf_len)) {
            /* EOI */
            if(((j + 1) < stream_length) &&
                0xD9 == GetBytes(stream, j + 1, buffer, buf_len)) {
              /* check length vs. data */
              if((j + 2) >= (stream_length)) {
                nbr_of_images++;
                nbr_of_images_total = nbr_of_images;
                nbr_of_thumb_images_total = nbr_of_thumb_images;
                last_marker = 2;
                return (0);
              } else {
                if(jpeg_thumb_in_stream) {
                  nbr_of_thumb_images++;
                  jpeg_thumb_in_stream = 0;
                } else {
                  nbr_of_images++;
                }
              }
              /* update */
              i = j;
              /* stil data left ==> break this loop to continue */
              break;
            }
          }
        }
      }

      /* check if thumbnails in stream */
      if(((i + 1) < stream_length) &&
          0xE0 == GetBytes(stream, i + 1, buffer, buf_len)) {
        if( ((i + 9) < stream_length) &&
            0x4A == GetBytes(stream, i + 4, buffer, buf_len) &&
            0x46 == GetBytes(stream, i + 5, buffer, buf_len) &&
            0x58 == GetBytes(stream, i + 6, buffer, buf_len) &&
            0x58 == GetBytes(stream, i + 7, buffer, buf_len) &&
            0x00 == GetBytes(stream, i + 8, buffer, buf_len) &&
            0x10 == GetBytes(stream, i + 9, buffer, buf_len)) {
          jpeg_thumb_in_stream = 1;
        }
        last_marker = 1;
      }

      /* check if exif in stream */
      if(((i + 1) < stream_length) &&
          0xE1 == GetBytes(stream, i + 1, buffer, buf_len)) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = GetBytes(stream, i, buffer, buf_len);
        tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        if( ((i + 7) < stream_length) &&
            0x45 == GetBytes(stream, i + 2, buffer, buf_len) &&
            0x78 == GetBytes(stream, i + 3, buffer, buf_len) &&
            0x69 == GetBytes(stream, i + 4, buffer, buf_len) &&
            0x66 == GetBytes(stream, i + 5, buffer, buf_len) &&
            0x00 == GetBytes(stream, i + 6, buffer, buf_len) &&
            0x00 == GetBytes(stream, i + 7, buffer, buf_len)) {
          jpeg_exif_in_stream = 1;
        }
        last_marker++;
        i += (tmp_total-1);
      }
    }
  }

  /* update total amount of pictures */
  nbr_of_images_total = nbr_of_images;
  nbr_of_thumb_images_total = nbr_of_thumb_images;

  /* continue until amount of frames counted */
  if(last_marker == 2)
    return 0;
  else
    return -1;
}


static u32 GetBytes(u8 * stream, u32 idx, u8 *buffer, u32 buffer_length) {
  u32 offset = (addr_t) stream - (addr_t) buffer;
  if (offset + idx < buffer_length)
    return buffer[offset + idx];
   else
     return buffer[offset + idx - buffer_length];
}



/*-----------------------------------------------------------------------------

    Function name:  FindImageEOI

    Purpose:
        Finds 0xFFD9 from the stream and p_offset includes number of bytes to
        this marker. In case of an error returns != 0
        (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageEOI(u8 * stream, u32 stream_length, u32 * p_offset,
                 u8 * buffer, u32 buf_len) {
  u32 i,j;
  u32 jpeg_thumb_in_stream = 0;
  u32 tmp, tmp1, tmp_total = 0;

  *p_offset = 0;
  for(i = 0; i < stream_length; ++i) {
    if(0xFF == GetBytes(stream, i, buffer, buf_len)) {
      /* if 0xFFE1 to 0xFFFD ==> skip  */

      if( (((i + 1) < stream_length) &&
          0xE1 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE4 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xE9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEA == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEB == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEC == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xED == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEE == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xEF == GetBytes(stream, i + 1, buffer, buf_len)) /*||
          (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
          (((i + 1) < stream_length) && 0xFD == stream[i + 1])*/ ) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = GetBytes(stream, i, buffer, buf_len);
        tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;
        continue;
      }

      /* if 0xFFC2 to 0xFFCB ==> skip  */
      if( (((i + 1) < stream_length) &&
          0xC1 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xC9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xCA == GetBytes(stream, i + 1, buffer, buf_len)) ||
          (((i + 1) < stream_length) &&
          0xCB == GetBytes(stream, i + 1, buffer, buf_len)) ) {
        /* increase counter */
        i += 2;

        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = GetBytes(stream, i, buffer, buf_len);
        tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;

        /* look for EOI */
        for(j = i; j < stream_length; ++j) {
          if(0xFF == GetBytes(stream, j, buffer, buf_len)) {
            /* EOI */
            if(((j + 1) < stream_length) &&
                0xD9 == GetBytes(stream, j + 1, buffer, buf_len)) {
              /* check length vs. data */
              if((j + 2) >= (stream_length)) {
                *p_offset = j+2;
                return (0);
              }
              /* update */
              i = j;
              /* stil data left ==> continue */
              continue;
            }
          }
        }
      }

      /* check if thumbnails in stream */
      if(((i + 1) < stream_length) &&
          0xE0 == GetBytes(stream, i + 1, buffer, buf_len)) {
        if( ((i + 9) < stream_length) &&
            0x4A == GetBytes(stream, i + 4, buffer, buf_len) &&
            0x46 == GetBytes(stream, i + 5, buffer, buf_len) &&
            0x58 == GetBytes(stream, i + 6, buffer, buf_len) &&
            0x58 == GetBytes(stream, i + 7, buffer, buf_len) &&
            0x00 == GetBytes(stream, i + 8, buffer, buf_len) &&
            0x10 == GetBytes(stream, i + 9, buffer, buf_len)) {
          jpeg_thumb_in_stream = 1;
        }
      }

      /* EOI */
      if(((i + 1) < stream_length) &&
          0xD9 == GetBytes(stream, i + 1, buffer, buf_len)) {
        *p_offset = i + 2;
        /* update amount of thumbnail or full resolution image */
        if(jpeg_thumb_in_stream) {
          jpeg_thumb_in_stream = 0;
        } else
          return 0;

      }
    }
  }

  return -1;
}

/*-----------------------------------------------------------------------------

    Function name:  FindImageTnEOI

    Purpose:
        Finds 0xFFD9 and next 0xFFE0 (containing THUMB) from the stream and
        p_offset includes number of bytes to this marker. In case of an error
        returns != 0 (i.e., the marker not found).

-----------------------------------------------------------------------------*/
u32 FindImageTnEOI(u8 * stream, u32 stream_length, u32 * p_offset,
                   u32 mode, u32 thumb_exist, u8 * buffer, u32 buf_len) {
  u32 i,j,k;
  u32 h = 0;

  /* reset */
  next_soi = 0;
  *p_offset = 0;

  for(i = 0; i < stream_length; ++i) {
    if(0xFF == GetBytes(stream, i, buffer, buf_len)) {
      if(((i + 1) < stream_length) &&
          0xD9 == GetBytes(stream, i + 1, buffer, buf_len)) {
        if(thumb_exist) {
          for(j = (i+2); j < stream_length; ++j) {
            if(0xFF == GetBytes(stream, j, buffer, buf_len)) {
              /* seek for next thumbnail in stream */
              if(((j + 1) < stream_length) &&
                  0xE0 == GetBytes(stream, j + 1, buffer, buf_len)) {
                if( ((j + 9) < stream_length) &&
                    0x4A == GetBytes(stream, j + 4, buffer, buf_len) &&
                    0x46 == GetBytes(stream, j + 5, buffer, buf_len) &&
                    0x58 == GetBytes(stream, j + 6, buffer, buf_len) &&
                    0x58 == GetBytes(stream, j + 7, buffer, buf_len) &&
                    0x00 == GetBytes(stream, j + 8, buffer, buf_len) &&
                    0x10 == GetBytes(stream, j + 9, buffer, buf_len)) {
                  next_soi = 1;
                  *p_offset = h;
                  return 0;
                }
              } else if(((j + 1) < stream_length) &&
                        0xD9 == GetBytes(stream, j + 1, buffer, buf_len)) {
                k = j+2;
                /* return if FULL */
                if(!mode) {
                  *p_offset = k;
                  return 0;
                }
              } else if(((j + 1) < stream_length) &&
                        0xD8 == GetBytes(stream, j + 1, buffer, buf_len)) {
                if(j) {
                  h = j;
                }
              }
            }
          }

          /* if no THUMB, but found next SOI ==> return */
          if(h) {
            *p_offset = h;
            next_soi = 1;
            return 0;
          } else
            return -1;
        } else {
          next_soi = 1;
          *p_offset = i+2;
          return 0;
        }
      }
      /* if 0xFFE1 to 0xFFFD ==> skip  */
      else if( (((i + 1) < stream_length) &&
                0xE1 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE2 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE3 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE4 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE5 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE6 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE7 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE8 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xE9 == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xEA == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xEB == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xEC == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xED == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xEE == GetBytes(stream, i + 1, buffer, buf_len)) ||
               (((i + 1) < stream_length) &&
                0xEF == GetBytes(stream, i + 1, buffer, buf_len)) /*||
               (((i + 1) < stream_length) && 0xF0 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF1 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF2 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF3 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF4 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF5 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF6 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF7 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF8 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xF9 == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFA == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFB == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFC == stream[i + 1]) ||
               (((i + 1) < stream_length) && 0xFD == stream[i + 1])*/ ) {
        u32 tmp, tmp1, tmp_total = 0;

        /* increase counter */
        i += 2;
        /* check length vs. data */
        if((i + 1) > (stream_length))
          return (-1);

        /* get length */
        tmp = GetBytes(stream, i, buffer, buf_len);
        tmp1 = GetBytes(stream, i + 1, buffer, buf_len);
        tmp_total = (tmp << 8) | tmp1;

        /* check length vs. data */
        if((tmp_total + i) > (stream_length))
          return (-1);
        /* update */
        i += tmp_total-1;
        continue;
      }
    }
  }
  return -1;
}

static u32 FindImageAllRI(u8 *img_buf, u32 img_len, u32 *ri_array, u32 ri_count) {
  u8 *p = img_buf;
  u32 rst_markers = 0;
  u32 last_rst = 0, i;
  ri_array[0] = 0;
  while (p < img_buf + img_len) {
    if (p[0] == 0xFF && p[1] >= 0xD0 && p[1] <= 0xD7) {
      if (!rst_markers) {
        rst_markers++;
      } else {
        /* missing restart intervals */
        u32 missing_rst_count = (p[1] - 0xD7 + 8 - last_rst - 1) % 8;
        for (i = 0; i < missing_rst_count; i++) {
          if (++rst_markers < ri_count)
            ri_array[rst_markers] = 0;
        }
        rst_markers++;
      }
      if (rst_markers < ri_count) {
        ri_array[rst_markers]  = p + 2 - img_buf;
      }
      last_rst = p[1] - 0xD7;
      printf("[TB] %u RST%d @ offset %u: %02X%02X\n", rst_markers,  p[1] - 0xD0,
            (u32)(p - img_buf), p[0], p[1]);
    }
    p++;
  }
  rst_markers++;
  return (rst_markers);
}


void WriteCroppedOutput(struct DecSequenceInfo * info, u8 * data_luma, u8 * data_cb,
                        u8 * data_cr) {
  u32 i, j;
  FILE *foutput = NULL;
  u8 *p_yuv_out = NULL;
  u32 luma_w, luma_h, chroma_w, chroma_h, chroma_output_width;

  fprintf(stdout, "[TB]  WriteCroppedOut, display_w %u, display_h %u\n",
          info->scaled_width, info->scaled_height);

  foutput = fopen("[TB] cropped.yuv", "wb");
  if(foutput == NULL) {
    fprintf(stdout, "[TB] UNABLE TO OPEN OUTPUT FILE\n");
    return;
  }

  if(info->output_format == DEC_OUT_FRM_YUV420SP) {
    luma_w = (info->scaled_width + 1) & ~0x1;
    luma_h = (info->scaled_height + 1) & ~0x1;
    chroma_w = luma_w / 2;
    chroma_h = luma_h / 2;
    chroma_output_width = info->pic_width / 2;
  } else if(info->output_format == DEC_OUT_FRM_YUV422SP) {
    luma_w = (info->scaled_width + 1) & ~0x1;
    luma_h = info->scaled_height;
    chroma_w = luma_w / 2;
    chroma_h = luma_h;
    chroma_output_width = info->pic_width / 2;
  } else if(info->output_format == DEC_OUT_FRM_YUV440) {
    luma_w = info->scaled_width;
    luma_h = (info->scaled_height + 1) & ~0x1;
    chroma_w = luma_w;
    chroma_h = luma_h / 2;
    chroma_output_width = info->pic_width;
  } else if(info->output_format == DEC_OUT_FRM_YUV411SP) {
    luma_w = (info->scaled_width + 3) & ~0x3;
    luma_h = info->scaled_height;
    chroma_w = luma_w / 4;
    chroma_h = luma_h;
    chroma_output_width = info->pic_width / 4;
  } else if(info->output_format == DEC_OUT_FRM_YUV444SP) {
    luma_w = info->scaled_width;
    luma_h = info->scaled_height;
    chroma_w = luma_w;
    chroma_h = luma_h;
    chroma_output_width = info->pic_width;
  } else {
    luma_w = info->scaled_width;
    luma_h = info->scaled_height;
    chroma_w = 0;
    chroma_h = 0;
    chroma_output_width = 0;

  }

  /* write decoder output to file */
  p_yuv_out = data_luma;
  for(i = 0; i < luma_h; i++) {
    fwrite(p_yuv_out, sizeof(u8), luma_w, foutput);
    p_yuv_out += info->pic_width;
  }

  p_yuv_out += (info->pic_height - luma_h) * info->pic_width;

  /* baseline -> output in semiplanar format */
  if(info->coding_mode != JPEG_PROGRESSIVE) {
    for(i = 0; i < chroma_h; i++)
      for(j = 0; j < chroma_w; j++)
        fwrite(p_yuv_out + i * chroma_output_width * 2 + j * 2,
               sizeof(u8), 1, foutput);
    for(i = 0; i < chroma_h; i++)
      for(j = 0; j < chroma_w; j++)
        fwrite(p_yuv_out + i * chroma_output_width * 2 + j * 2 + 1,
               sizeof(u8), 1, foutput);
  } else {
    p_yuv_out = data_cb;
    for(i = 0; i < chroma_h; i++) {
      fwrite(p_yuv_out, sizeof(u8), chroma_w, foutput);
      p_yuv_out += chroma_output_width;
    }
    /*p_yuv_out += (chroma_output_height-chroma_h)*chroma_output_width; */
    p_yuv_out = data_cr;
    for(i = 0; i < chroma_h; i++) {
      fwrite(p_yuv_out, sizeof(u8), chroma_w, foutput);
      p_yuv_out += chroma_output_width;
    }
  }

  fclose(foutput);
}

void WriteProgressiveOutput(char** out_file_name, u32 size_luma, u32 size_chroma,
                            u32 mode, u8 * data_luma, u8 * data_cb, u8 * data_cr) {
  FILE *foutput = NULL;

  fprintf(stdout, "[TB]  WriteProgressiveOutput\n");

  if (strlen(out_file_name[0]) >= 4)
    sprintf(out_file_name[0] + MIN((strlen(out_file_name[0])-4), 250), "_0.yuv");

  foutput = fopen(out_file_name[0], "ab");
  if(foutput == NULL) {
    fprintf(stdout, "[TB] UNABLE TO OPEN OUTPUT FILE\n");
    return;
  }

  /* write decoder output to file */
  fwrite(data_luma, sizeof(u8), size_luma + size_chroma, foutput);
  //fwrite(data_cb, sizeof(u8), size_chroma / 2, foutput);
  //fwrite(data_cr, sizeof(u8), size_chroma / 2, foutput);

  fclose(foutput);
}

static task_handle run_task(task_func func, void* param) {
  int ret;
  pthread_attr_t attr;
  struct sched_param par;
  pthread_t* thread_handle = malloc(sizeof(pthread_t));
  if (thread_handle == NULL) return NULL;

  pthread_attr_init(&attr);
  ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
  assert(ret == 0);
  ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
  par.sched_priority = 60;
  ret = pthread_attr_setschedparam(&attr, &par);

  ret = pthread_create(thread_handle, &attr, func, param);
  assert(ret == 0);

  if(ret != 0)
  {
    free(thread_handle);
    thread_handle = NULL;
  }

  pthread_attr_destroy(&attr);
  return thread_handle;
}

static void wait_for_task_completion(task_handle task) {
  int ret = pthread_join(*((pthread_t*)task), NULL);
  assert(ret == 0);
  UNUSED(ret);
  free(task);
}


void JpegSendBytesToDDR(u8* p, u32 n, FILE* fp) {
  int ret = fread(p, 1, n, fp);
  assert(ret >= 0);
  UNUSED(ret);
}


static void* send_bytestrm_task(void* param) {
  FILE* fp  = fopen(in_file_name, "rb");
  if (fp == NULL) return NULL;

  u32 packet_size = LOW_LATENCY_PACKET_SIZE;
  u32 send_len = 0;
  u32 bytes = 0;

  while(!process_end_flag) {
    if(send_strm_len - send_len > packet_size) {
      bytes = packet_size;
      JpegSendBytesToDDR(send_strm_info.strm_vir_addr, packet_size, fp);
      send_strm_info.strm_bus_addr += packet_size;
      send_len += packet_size;
      send_strm_info.last_flag = 0;
      send_strm_info.strm_vir_addr += packet_size;
    } else {
      bytes = send_strm_len - send_len;
      JpegSendBytesToDDR(send_strm_info.strm_vir_addr, bytes, fp);
      send_strm_info.strm_bus_addr += bytes;
      send_len += bytes;
      send_strm_info.last_flag = 1;
      send_strm_info.strm_vir_addr += bytes;
    }

    if(sw_hw_bound == 0) {
      sw_hw_bound = FindImageData(send_strm_info.strm_vir_start_addr+bytes_go_back, send_len);
      if (sw_hw_bound) sw_hw_bound = send_strm_len;
      if(sw_hw_bound || send_strm_info.last_flag)
        sem_post(&send_sem);
      else
        continue;
    }

    VCDecUpdateStrmInfoCtrl(jpeg, send_strm_info);
#ifdef ENABLE_FPGA_VERIFICATION
    if (send_strm_len > 10000000)
      usleep(100);
    else
      usleep(1000);
#endif

    if(pic_decoded == 1) {
      pic_decoded = 0;
      send_strm_info.last_flag = 0;
      send_strm_info.strm_bus_addr = send_strm_info.strm_bus_start_addr + bytes_go_back;
      send_strm_info.strm_vir_addr = send_strm_info.strm_vir_start_addr + bytes_go_back;
      sem_wait(&frame_sem);
      send_len = 0;
      sw_hw_bound = 0;
      send_strm_len = tmp_len;
      fseek(fp, bytes_go_back, SEEK_SET);
    }
  }
  fclose(fp);

  return NULL;
}

u32 FindImageData(u8 * p_stream, u32 stream_length)
{
  u32 read_bits;
  u32 marker_byte;
  u32 current_byte;
  u32 header_length;
  /* check pointers & parameters */
  if(p_stream == NULL)
    return 0;
  /* Check the stream lenth */
   if(stream_length < 1)
     return 0;

  read_bits = 0;

  /* Read decoding parameters */
  while(read_bits  < stream_length) {
    /* Look for marker prefix byte from stream */
    marker_byte = p_stream[read_bits];
    if(marker_byte == 0xFF) {
      if(read_bits + 1 >= stream_length)
        return 0;
      current_byte = p_stream[read_bits + 1];
      /* switch to certain header decoding */
      switch (current_byte)
      {
      case 0xC0: //SOF0:
      case 0xC2: //SOF2:
      case 0xDB: //DQT:
      case 0xC4: //DHT:
      case 0xDD: //DRI:
      case 0xE1: //APP1:
      case 0xE2: //APP2:
      case 0xE3: //APP3:
      case 0xE4: //APP4:
      case 0xE5: //APP5:
      case 0xE6: //APP6:
      case 0xE7: //APP7:
      case 0xE8: //APP8:
      case 0xE9: //APP9:
      case 0xEA: //APP10:
      case 0xEB: //APP11:
      case 0xEC: //APP12:
      case 0xED: //APP13:
      case 0xEE: //APP14:
      case 0xEF: //APP15:
      case 0xDC: //DNL:
      case 0xFE: //COM:
/*
      case 0xC1: //SOF1:
      case 0xC3: //SOF3:
      case 0xC5: //SOF5:
      case 0xC6: //SOF6:
      case 0xC7: //SOF7:
      case 0xC8: //SOF9:
      case 0xCA: //SOF10:
      case 0xCB: //SOF11:
      case 0xCD: //SOF13:
      case 0xCE: //SOF14:
      case 0xCF: //SOF15:
      case 0xCC: //DAC:
      case 0xDE: //DHP:
*/
      case 0xE0: //APP0:
          if(read_bits + 3 >= stream_length)
            return 0;
          header_length = (p_stream[read_bits + 2] << 8) + p_stream[read_bits + 3];
          if((read_bits + header_length) > stream_length)
            return 0;
          if(header_length != 0)
            read_bits += (header_length + 1);
          break;

      case 0xDA: //SOS:
          /* SOS length */
          if(read_bits + 3 >= stream_length)
            return 0;
          header_length = (p_stream[read_bits + 2] << 8) + p_stream[read_bits + 3];
          if((read_bits + header_length) > stream_length)
            return 0;
          /* jump over SOS header */
          if(header_length != 0)
            read_bits += (header_length + 1);

         if(read_bits >= stream_length)
           return 0;
         else
           /* return a big value to control sw */
           return DEC_X170_MAX_STREAM_VCD;
         break;
      default:
          read_bits++;
          break;
      }
    } else {
      read_bits++;
    }
  }
  return 0;
}

static void SetPpConfig(struct TestParams *params, struct DecSequenceInfo image_info,
                      struct DecConfig *config, u32 crop_step_rshift, u32 image_type) {
  u32 display_width = 0, display_height = 0;
  u32 display_width_thumb = 0, display_height_thumb = 0;
  u32 scale_width_thumb = 0, scale_height_thumb = 0;
  u32 crop_w, crop_h;
  u32 i = 0;

  params->crop_align = crop_step_rshift;
  if (params->ppu_cfg[0].scale.scale_by_ratio) {
    params->ppu_cfg[0].scale.width = 0;
    params->ppu_cfg[0].scale.height = 0;
  }

  /* Set PP info after image info and decoding mode is determined. */
  if (params->ppu_cfg[0].scale.scale_by_ratio) {
    /*support odd crop*/
    if (crop_step_rshift) {
      display_width = (image_info.scaled_width + 1) & ~0x1;
      display_height = (image_info.scaled_height + 1) & ~0x1;
      display_width_thumb = (image_info.scaled_width_thumb + 1) & ~0x1;
      display_height_thumb = (image_info.scaled_height_thumb + 1) & ~0x1;
    } else {
      display_width = image_info.scaled_width;
      display_height = image_info.scaled_height;
      display_width_thumb = image_info.scaled_width_thumb;
      display_height_thumb = image_info.scaled_height_thumb;
    }

    if (!params->ppu_cfg[0].crop.set_by_user) {
      params->ppu_cfg[0].crop.width = mode ? display_width_thumb: display_width;
      params->ppu_cfg[0].crop.height = mode ? display_height_thumb: display_height;
      params->ppu_cfg[0].crop.enabled = 1;
    }
    crop_w = params->ppu_cfg[0].crop.width;
    crop_h = params->ppu_cfg[0].crop.height;
    planar_output = params->ppu_cfg[0].planar;

    if (mode == 1) {
      /*support odd crop*/
      if (crop_step_rshift) {
        scale_width_thumb = NEXT_MULTIPLE(crop_w / params->ppu_cfg[0].scale.ratio_x - 1, 2);
        scale_height_thumb = NEXT_MULTIPLE(crop_h / params->ppu_cfg[0].scale.ratio_y - 1, 2);
        image_info.pic_width = NEXT_MULTIPLE(scale_width_thumb, ALIGN(config->align));
        image_info.pic_width_thumb = NEXT_MULTIPLE(scale_width_thumb, ALIGN(config->align));
      } else {
        scale_width_thumb = params->ppu_cfg[0].scale.ratio_x == 1 ? crop_w : (crop_w + 1) / params->ppu_cfg[0].scale.ratio_x;
        scale_height_thumb = params->ppu_cfg[0].scale.ratio_y == 1 ? crop_h : (crop_h + 1) / params->ppu_cfg[0].scale.ratio_y;
        image_info.pic_width = scale_width_thumb;
        image_info.pic_width_thumb = scale_width_thumb;
      }
      image_info.pic_height = scale_height_thumb;
      image_info.pic_height_thumb = scale_height_thumb;
    } else {
      /*support odd crop*/
      if (crop_step_rshift) {
        params->ppu_cfg[0].scale.width = NEXT_MULTIPLE(crop_w / params->ppu_cfg[0].scale.ratio_x - 1, 2);
        params->ppu_cfg[0].scale.height = NEXT_MULTIPLE(crop_h / params->ppu_cfg[0].scale.ratio_y - 1, 2);
        image_info.pic_width = NEXT_MULTIPLE(params->ppu_cfg[0].scale.width, ALIGN(config->align));
        image_info.pic_width_thumb = NEXT_MULTIPLE(params->ppu_cfg[0].scale.width, ALIGN(config->align));
      } else {
        params->ppu_cfg[0].scale.width = params->ppu_cfg[0].scale.ratio_x == 1 ? crop_w : (crop_w + 1) / params->ppu_cfg[0].scale.ratio_x;
        params->ppu_cfg[0].scale.height = params->ppu_cfg[0].scale.ratio_y == 1 ? crop_h : (crop_h + 1) / params->ppu_cfg[0].scale.ratio_y;
        image_info.pic_width = params->ppu_cfg[0].scale.width;
        image_info.pic_width_thumb = params->ppu_cfg[0].scale.width;
      }
      image_info.pic_height = params->ppu_cfg[0].scale.height;
      image_info.pic_height_thumb = params->ppu_cfg[0].scale.height;
    }
    params->ppu_cfg[0].scale.enabled = 1;
    params->ppu_cfg[0].enabled = 1;
    params->ppu_cfg[0].align = params->align;  //set align for pp0
    params->pp_enabled = 1;
  } else {
    display_width = image_info.scaled_width;
    display_height = image_info.scaled_height;
    display_width_thumb = image_info.scaled_width_thumb;
    display_height_thumb = image_info.scaled_height_thumb;

    if (!params->ppu_cfg[0].crop.set_by_user) {
      params->ppu_cfg[0].crop.width = mode ? display_width_thumb: display_width;
      params->ppu_cfg[0].crop.height = mode ? display_height_thumb: display_height;
      params->ppu_cfg[0].crop.enabled = 1;
    }
  }
  pp_enabled = params->pp_enabled;
  memcpy(config->ppu_cfg, params->ppu_cfg, sizeof(params->ppu_cfg));
  memcpy(config->delogo_params, params->delogo_params, sizeof(params->delogo_params));
  if (params->ppu_cfg[0].enabled == 1) {
    if (params->ppu_cfg[0].scale.width) {
      /*support odd crop*/
      if(crop_step_rshift) {
        image_info.pic_width = NEXT_MULTIPLE(params->ppu_cfg[0].scale.width, ALIGN(config->align));
        image_info.pic_width_thumb = NEXT_MULTIPLE(params->ppu_cfg[0].scale.width, ALIGN(config->align));
      } else {
        image_info.pic_width = params->ppu_cfg[0].scale.width;
        image_info.pic_width_thumb = params->ppu_cfg[0].scale.width;
      }
    }
    if (params->ppu_cfg[0].scale.height) {
      image_info.pic_height = params->ppu_cfg[0].scale.height;
      image_info.pic_height_thumb = params->ppu_cfg[0].scale.height;
    }
  }

  config->dec_image_type = image_type;
  config->chroma_format = image_info.output_format;
  if (config->dec_image_type == JPEGDEC_THUMBNAIL) {
    config->chroma_format = image_info.output_format_thumb;
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (config->ppu_cfg[i].scale.enabled == 1) {
        config->ppu_cfg[i].scale.scale_by_ratio = 0;
        if (crop_step_rshift) {
          config->ppu_cfg[i].scale.width = (image_info.scaled_width_thumb + 1) & ~0x1;
          config->ppu_cfg[i].scale.height = (image_info.scaled_height_thumb + 1) & ~0x1;
        } else {
          config->ppu_cfg[i].scale.width = image_info.scaled_width_thumb;
          config->ppu_cfg[i].scale.height = image_info.scaled_height_thumb;
        }
      }
      if (config->ppu_cfg[i].crop.enabled == 1) {
        config->ppu_cfg[i].crop.enabled = 0;
        config->ppu_cfg[i].crop.set_by_user = 0;
      }
      if (config->ppu_cfg[i].crop2.enabled == 1) {
        config->ppu_cfg[i].crop2.x = 0;
        config->ppu_cfg[i].crop2.y = 0;
        config->ppu_cfg[i].crop2.width = config->ppu_cfg[i].scale.width;
        config->ppu_cfg[i].crop2.height = config->ppu_cfg[i].scale.height;
      }
    }
  }
}

