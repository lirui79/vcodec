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

#include "basetype.h"
#include "ivf.h"
#include "dwl.h"
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include "vp8filereader.h"
#include "dec_log.h"
#ifdef USE_EFENCE
#include "efence.h"
#endif
#include "regdrv.h"
#include "tb_cfg.h"
#include "deccfg.h"
#include "common_sink.h"
#include "tb_sw_performance.h"
#include "command_line_parser.h"
#include "vcd_tools.h"

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif
#include "vcdecapi.h"
#include "dec_log.h"


struct TBCfg tb_cfg;

u32 number_of_written_frames = 0;
u32 cycle_count = 0; /* Sum of average cycles/mb counts */
u32 num_frame_buffers = 0;
// /* SW/SW testing, read stream trace file */
// FILE * f_stream_trace = NULL;

u32 disable_output_writing = 0;
u32 display_order = 0;
u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
u32 planar_output = 0;
u32 height_crop = 0;
u32 convert_tiled_output = 0;

u32 use_peek_output = 0;
u32 snap_shot = 0;
u32 forced_slice_mode = 0;

// u32 stream_packet_loss = 0;
// u32 stream_truncate = 0;
// u32 stream_header_corrupt = 0;
// u32 stream_bit_swap = 0;
// u32 hdrs_rdy = 0;
// u32 pic_rdy = 0;

/*secure mode */
u32 secure_mode;

u8 *p_frame_pic[DEC_MAX_OUT_COUNT] = {NULL,NULL};
u32 include_strides = 0;
i32 extra_strm_buffer_bits = 0; /* For HW bug workaround */

u32 user_mem_alloc = 0;
u32 interleaved_user_buffers = 0;
struct DWLLinearMem user_alloc_luma[16];
struct DWLLinearMem user_alloc_chroma[16];
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 pp_enabled = 0;
/* user input arguments */

u8 *pic_big_endian = NULL;
size_t pic_big_endian_size = 0;

void writeSlice(FILE * fp, struct DecPictures* dec_pic);

/* These global values are found from commonconfig.c.
 * partial tb_cfg.dec_params parameters. */
extern struct DecParams dec_params;

struct TOOL_PARAMS tool_params;

#ifdef USE_RANDOM_ERROR_TEST
/* These global values are found from vcdecapi.c.
 * partial tb_cfg.tb_params parameters for random error injectionn. */
extern struct ErrorParams random_error_params;
#endif

/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

#define VP8_MAX_STREAM_SIZE  DEC_X170_MAX_STREAM_VCD>>1

#define MAX_BUFFERS 34

#ifdef ADS_PERFORMANCE_SIMULATION
include_strides
volatile u32 tttttt = 0;

void trace_perf() {
  tttttt++;
}

#undef START_SW_PERFORMANCE
#undef END_SW_PERFORMANCE

#define START_SW_PERFORMANCE trace_perf();
#define END_SW_PERFORMANCE trace_perf();

#endif

typedef void *VP8DecInst;
VP8DecInst dec_inst;
YuvSink* yuvsink; /* Yuvsink instance. */
const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size;
u32 num_buffers;  /* external buffers allocated yet. */
u32 add_buffer_thread_run = 0;
pthread_t add_buffer_thread;
pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS];

u32 add_extra_flag = 0;
/* Fixme: this value should be set based on option "-d" when invoking testbench. */

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && (num_buffers < MAX_BUFFERS)) {
      struct DWLLinearMem mem={0};
      i32 dwl_ret;
#ifdef SUPPORT_DMA
      mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
      if (pp_enabled) {
        SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      } else {
        SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      }
      if(dwl_ret == DWL_OK) {
        enum DecRet rv = VCDecAddBuffer(dec_inst, &mem);
        if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
          if (pp_enabled)
            DWLFreeLinear(dwl_inst, &mem);
          else
            DWLFreeRefFrm(dwl_inst, &mem);
        } else {
          ext_buffers[num_buffers++] = mem;
        }
      }
    }
    pthread_mutex_unlock(&ext_buffer_contro);
    sched_yield();
  }
  return NULL;
}

void ReleaseExtBuffers() {
  int i;
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<num_buffers; i++) {
    DEBUG_PRINT(("[TB] Freeing buffer %p\n", (void *)ext_buffers[i].virtual_address));
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

FILE *fout[DEC_MAX_OUT_COUNT] = {NULL, NULL};
u32 md5sum = 0; /* flag to enable md5sum output */
u32 slice_mode = 0;
pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;

sem_t buf_release_sem;
struct DecPictures buf_list[100];
u32 buf_status[100] = {0};
u32 list_pop_index = 0;
u32 list_push_index = 0;
u32 last_pic_flag = 0;

/* buf release thread entry point. */
static void* buf_release_thread(void* arg) {
  while(1) {
    /* Pop output buffer from buf_list and consume it */
    if(buf_status[list_pop_index]) {
      sem_wait(&buf_release_sem);
      VCDecPictureConsumed(dec_inst, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;

      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && num_buffers < MAX_BUFFERS) {
          struct DWLLinearMem mem={0};
          i32 dwl_ret;
#ifdef SUPPORT_DMA
          mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
          if (pp_enabled) {
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
            dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
          } else {
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
            dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
          }
          if(dwl_ret == DWL_OK) {
            enum DecRet rv = VCDecAddBuffer(dec_inst, &mem);
            if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              ext_buffers[num_buffers++] = mem;
            }
          }
        }
        pthread_mutex_unlock(&ext_buffer_contro);
      }
    }
    if(last_pic_flag && buf_status[list_pop_index] == 0) {
      sem_destroy(&buf_release_sem);
      break;
    }
    usleep(10000);
  }
  return (NULL);
}

/* Output thread entry point. */
static void* vp8_output_thread(void* arg) {
  struct DecPictures dec_picture;
  u32 pic_display_number = 1;
  while(output_thread_run) {
    enum DecRet ret;
    u32 i, only_once = 0, ext_id = 0xFFFF;
    struct DecPicture* in = &dec_picture.pictures[0];
    u32* host_base = NULL;

    ret = VCDecNextPicture(dec_inst, &dec_picture);
    if(ret == DEC_PIC_RDY) {
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        in = &dec_picture.pictures[i];
        if (!in->sequence_info.pic_width ||
            !in->sequence_info.pic_height)
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

        if(!use_peek_output) {
          if (in->picture_info.num_slice_rows) {
            writeSlice(fout[i], &dec_picture);
            slice_mode = 1;
          } else {
            DEBUG_PRINT(("[TB] PIC %2u/%2u, type %s,", pic_display_number, (in->picture_info.pic_id + 1),
                 in->picture_info.pic_coding_type ? "key picture    " : "non key picture"));
            if (in->picture_info.cycles_per_mb) {
              cycle_count += in->picture_info.cycles_per_mb;
              DEBUG_PRINT(("[TB] %4u cycles / mb,", in->picture_info.cycles_per_mb));
            }
            DEBUG_PRINT((" %u x %u, Crop: (%u, %u), %u x %u\n",
                        dec_picture.pictures[0].sequence_info.scaled_width,
                        dec_picture.pictures[0].sequence_info.scaled_height,
                        dec_picture.pictures[0].sequence_info.crop_params.crop_left_offset,
                        dec_picture.pictures[0].sequence_info.crop_params.crop_top_offset,
                        dec_picture.pictures[0].sequence_info.crop_params.crop_out_width,
                        dec_picture.pictures[0].sequence_info.crop_params.crop_out_height));
            yuvsink->WritePicture(yuvsink->inst, in, i);
          }
        }
        SwClearHostOutbaseAfterWriteFile(host_base, in);
      }
      if (host_base != NULL) {
        DWLfree(host_base);
        tool_params.ext_buffers[ext_id].virtual_address = NULL;
      }

      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = dec_picture;
      buf_status[list_push_index] = 1;
      list_push_index++;
      if(list_push_index == 100)
        list_push_index = 0;

      sem_post(&buf_release_sem);

      pic_display_number++;
    }

    else if(ret == DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      add_buffer_thread_run = 0;
      break;
    }
    usleep(10000);
  }

  return (NULL);
}

/*------------------------------------------------------------------------------
    Local functions
------------------------------------------------------------------------------*/
static u32 FfReadFrame( reader_inst reader, const u8 *buffer,
                        u32 max_buffer_size, u32 *frame_size, u32 pedantic ,u32 pic_decode_number);
/*------------------------------------------------------------------------------

    main

        Main function

------------------------------------------------------------------------------*/
int main(int argc, char**argv) {
  reader_inst reader;
  u32 i, tmp;
  u32 size = 0;
  u32 more_frames = 0;
  u32 pedantic_reading = 1;
  u32 max_num_pics = 0;
  u32 pic_decode_number = 0;
  u32 stream_len = 1024 * 1024; /* initial input stream buffer size */
  u32 buffer_release_flag = 1;
  enum DecRet ret;
  enum DecInputFormat dec_format = 0;
  struct DWLLinearMem stream_mem;
  DWLmemset(&stream_mem, 0, sizeof(struct DWLLinearMem));
  struct DecInputParameters dec_input;
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  struct DecOutput dec_output;
  DWLmemset(&dec_output, 0, sizeof(struct DecOutput));
  struct DecPictures dec_picture;
  DWLmemset(&dec_picture, 0, sizeof(struct DecPictures));
  struct DecSequenceInfo info;
  DWLmemset(&info, 0, sizeof(struct DecSequenceInfo));
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  struct DecBufferInfo hbuf;
  DWLmemset(&hbuf, 0, sizeof(struct DecBufferInfo));
  DWLmemset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VP8_DEC;
  u32 webp_loop = 0;
  FILE *f_tbcfg;
  // u32 seed_rnd = 0;
  struct OutFileInfo outfile_info;

  struct TestParams params;
  SetupDefaultParams(&params);

#ifndef EXPIRY_DATE
#define EXPIRY_DATE (u32)0xFFFFFFFF
#endif /* EXPIRY_DATE */

  /* expiry stuff */
  {
    char tm_buf[7];
    time_t sys_time;
    struct tm * tm;
    u32 tmp1;

    /* Check expiry date */
    time(&sys_time);
    tm = localtime(&sys_time);
    if (!tm) {
      fprintf(stderr,
              "Can't get local time.\n"
              );
      return -1;
    }
    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000+atoi(tm_buf);
    if (tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1 ) {
      fprintf(stderr,
              "EVALUATION PERIOD EXPIRED.\n"
              "Please contact On2 Sales.\n");
      return -1;
    }
  }

  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;
  dec_picture.pictures[0].picture_info.num_slice_rows = 0;

  /* Use common command line parser to parse options. */
  i32 ret_tmp;
  if ((ret_tmp = ParseParams(argc, argv, &params))) {
    if (ret_tmp == 1) {
      printf("Failed to parse params.\n\n");
      PrintUsage(argv[0], VP8DEC);
      ret_tmp = 1;
      return 1;
    }
    else {
      ret_tmp = 0;
      return 0;
    }
  }

  /* set secure mode */
  secure_mode = (params.decoder_mode & DEC_SECURITY) ? 1 : 0;
  /* get the value form params */
  disable_output_writing = (params.sink_type == SINK_NULL);
  pp_enabled = params.pp_enabled;
  planar_output = params.ppu_cfg[0].planar;
  max_num_pics = params.num_of_decoded_pics;
  use_peek_output = params.disable_display_order;
  md5sum = (params.sink_type == SINK_MD5_PICTURE);
  align = params.align;
  convert_tiled_output = params.convert_tiled_output;
  num_frame_buffers = params.num_buffers;
  if (num_frame_buffers > 16) num_frame_buffers = 16;
  use_extra_buffers = params.use_extra_buffers;
  allocate_extra_buffers_in_output = params.allocate_extra_buffers_in_output;
  snap_shot = params.snap_shot;
  extra_strm_buffer_bits = params.extra_strm_buffer_bits;
  user_mem_alloc = params.user_mem_alloc;
  // tool_params.use_mp_output = params.use_mp_output;
  // tool_params.use_separated_pp = params.use_separated_pp;
  tool_params.ext_buffers = ext_buffers;
  tool_params.max_buffers = &num_buffers;
  /* open data file */
  reader = rdr_open(argv[argc-1]);
  if (reader == NULL) {
    fprintf(stderr, "Unable to open input file\n");
    exit(100);
  }

  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  char *tb_path = (params.in_tb_cfg_file_name == NULL) ? "tb.cfg" : params.in_tb_cfg_file_name;
  f_tbcfg = fopen(tb_path, "r");
  if (f_tbcfg == NULL) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT FILE: \"tb.cfg of %s\"\n", tb_path));
    DEBUG_PRINT(("[TB] USING DEFAULT CONFIGURATION\n"));
  } else {
    fclose(f_tbcfg);
    if (TBParseConfig(tb_path, TBReadParam, &tb_cfg) == TB_FALSE)
      goto end;
    if (TBCheckCfg(&tb_cfg) != 0)
      goto end;
  }

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

    ResolvePpParamsOverlap(&params, !tb_cfg.pp_params.pipeline_e);
  }

  user_mem_alloc |= TBGetDecMemoryAllocation(&tb_cfg);
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  DEBUG_PRINT(("[TB] Decoder Clock Gating %u\n", clock_gating));
  DEBUG_PRINT(("[TB] Decoder Data Discard %u\n", data_discard));
  DEBUG_PRINT(("[TB] Decoder Latency Compensation %u\n", latency_comp));
  DEBUG_PRINT(("[TB] Decoder Output Picture Endian %u\n", output_picture_endian));
  DEBUG_PRINT(("[TB] Decoder Bus Burst Length %u\n", bus_burst_length));
  DEBUG_PRINT(("[TB] Decoder Asic Service Priority %u\n", asic_service_priority));

#ifdef USE_RANDOM_ERROR_TEST
  random_error_params.seed = tb_cfg.tb_params.seed_rnd;
  strcpy(random_error_params.truncate_stream_odds, tb_cfg.tb_params.stream_truncate);
  strcpy(random_error_params.swap_bit_odds, tb_cfg.tb_params.stream_bit_swap);
  strcpy(random_error_params.packet_loss_odds, tb_cfg.tb_params.stream_packet_loss);
#endif

  INIT_SW_PERFORMANCE;
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
  if (!tmp) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN TRACE FILE(S)\n"));
  }
#endif
  /* check format */
  switch (rdr_identify_format(reader)) {
  case G1_BITSTREAM_VP7:
    dec_format = DEC_INPUT_VP7;
    break;
  case G1_BITSTREAM_VP8:
    dec_format = DEC_INPUT_VP8;
    break;
  case G1_BITSTREAM_WEBP:
    dec_format = DEC_INPUT_WEBP;
    pedantic_reading = 0;  /* With WebP we rely on non-pedantic reading
                                      mode for correct operation. */
    break;
  }
  if (snap_shot)
    dec_format = DEC_INPUT_WEBP;
  if (dec_format == DEC_INPUT_WEBP)
    snap_shot = 1;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
  tool_params.dwl_inst = dwl_inst = DWLInit(&dwl_init);
  if(dwl_inst == NULL) {
    DEBUG_PRINT(("[TB] DWLInit# ERROR: DWL Init failed\n"));
    goto end;
  }

  {
    struct DecSwHwBuild build;
    struct DecApiVersion version;

    build = VCDecGetBuild(dwl_inst, dwl_init.client_type);
    version = VCDecGetAPIVersion();
    printf("[TB] VC9000 Decoder VP8 API version %u.%u.%u\n", version.major,
          version.minor, version.micro);
    printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
  }

  init_config.codec = DEC_VP8;
  init_config.dec_format = dec_format;
  init_config.error_handling = params.error_handling;
  init_config.error_ratio = params.error_ratio;
  init_config.num_frame_buffers = num_frame_buffers;
  init_config.use_adaptive_buffers = 1;
  init_config.guard_size = 0;
  init_config.dwl_inst = dwl_inst;
  /* initialize decoder. If unsuccessful -> exit */
  START_SW_PERFORMANCE;
  ret = VCDecInit((const void**)&dec_inst, &init_config);
  END_SW_PERFORMANCE;

  if (ret != DEC_OK) {
    fprintf(stderr,"DECODER INITIALIZATION FAILED\n");
    goto end;
  }

  // TBInitializeRandom(seed_rnd);

  /* Allocate stream memory */
#ifdef SUPPORT_DMA
  stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if(DWLMallocLinear(dwl_inst, stream_len, &stream_mem) != DWL_OK) {
    fprintf(stderr,"UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
    goto end;
  }
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  DWLmemset(&dec_output, 0, sizeof(struct DecOutput));

  dec_input.slice_height = tb_cfg.dec_params.jpeg_mcus_slice;

  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_VP8_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;

  /* Start decode loop */
  do {
    /* read next frame from file format */
    if (!webp_loop && !dec_output.data_left) {
      tmp = FfReadFrame( reader, (u8*)stream_mem.virtual_address,
                         stream_mem.size, &size, pedantic_reading, pic_decode_number );
      /* ugly hack for large webp streams. drop reserved stream buffer
       * and reallocate new with correct stream size. */
      if (tmp != OK && size < VP8_MAX_STREAM_SIZE &&
          (size + extra_strm_buffer_bits > stream_mem.size)) {
        DWLFreeLinear(dwl_inst, &stream_mem);
        stream_len = size + extra_strm_buffer_bits;
        /* Allocate MORE stream memory */
#ifdef SUPPORT_DMA
        stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
        SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
        if (DWLMallocLinear(dwl_inst,
                           stream_len,
                           &stream_mem) != DWL_OK ) {
          fprintf(stderr,"UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n");
          goto end;
        }
        /* try again */
        tmp = FfReadFrame( reader, (u8*)stream_mem.virtual_address,
                           stream_mem.size, &size, pedantic_reading ,pic_decode_number);

      }
      more_frames = (tmp==OK) ? TRUE : FALSE;
      if (!more_frames) {
        fprintf(stderr,"NOT SUPPORT THIS STREAM\n");
        goto end;
      }

      dec_input.pic_id++;
    }
    if( (more_frames && size != (u32)-1) || dec_output.data_left) {
      /* Decode */
      if (!webp_loop && !dec_output.data_left) {
        dec_input.stream = (u8*)stream_mem.virtual_address;
        dec_input.strm_len = size + extra_strm_buffer_bits;
        dec_input.stream_bus_address = (addr_t)stream_mem.bus_address;
        dec_input.stream_buffer = stream_mem;
        dec_input.stream_buffer.size = stream_mem.size + extra_strm_buffer_bits;
      }

      START_SW_PERFORMANCE;
      do {
        ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
      } while(dec_input.strm_len == 0 && ret == DEC_NO_DECODING_BUFFER)
      END_SW_PERFORMANCE;
      if (ret == DEC_HDRS_RDY) {
        ret = VCDecGetBufferInfo(dec_inst, &hbuf);
        DEBUG_PRINT(("[TB] VCDecGetBufferInfo ret %d\n", ret));
        DEBUG_PRINT(("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
                     (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));
#ifdef SLICE_MODE_LARGE_PIC
        i32 mcu_in_row;
        i32 mcu_size_divider = 1;
#endif
        // hdrs_rdy = 1;
        ret = VCDecGetInfo((void *)dec_inst, &info);
#ifdef SLICE_MODE_LARGE_PIC
        mcu_in_row = (info.pic_width / 16);
#endif

        if (dec_format == DEC_INPUT_WEBP && !params.pp_enabled && !user_mem_alloc) {
          params.ppu_cfg[0].scale.enabled = 1;
          params.ppu_cfg[0].scale.ratio_x = 1;
          params.ppu_cfg[0].scale.ratio_y = 1;
          params.ppu_cfg[0].scale.scale_by_ratio = 1;
          params.ppu_cfg[0].enabled = 1;
        }
        pp_enabled = params.pp_enabled;

        if (params.ppu_cfg[0].enabled) {
          if(!params.ppu_cfg[0].crop.set_by_user) {
            params.ppu_cfg[0].crop.width = ((info.crop_params.crop_out_width + 1) >> 1) << 1;
            params.ppu_cfg[0].crop.height = ((info.crop_params.crop_out_height + 1) >> 1) << 1;
            params.ppu_cfg[0].crop.enabled = 1;
          }
        }
        config.align = align;
        if (align == DEC_ALIGN_1B) config.align = DEC_ALIGN_64B;
        memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
        tmp = VCDecSetInfo(dec_inst, &config);
        if (tmp != DEC_OK)
          goto end;
#ifdef ASIC_TRACE_SUPPORT
        /* Handle incorrect slice size for HW testing */
        if(dec_input.slice_height > (info.pic_height >> 4)) {
          dec_input.slice_height = (info.pic_height >> 4);
          printf("FIXED Decoder Slice MB Set %u\n", dec_input.slice_height);
        }
#endif /* ASIC_TRACE_SUPPORT */
#ifdef SLICE_MODE_LARGE_PIC
#ifdef ASIC_TRACE_SUPPORT
        /* 8190 and over 16M ==> force to slice mode */
        if((dec_input.slice_height == 0) &&
            (snap_shot) &&
            ((info.pic_width * info.pic_height) >
             VP8DEC_MAX_PIXEL_AMOUNT)) {
          do {
            dec_input.slice_height++;
          } while(((dec_input.slice_height * (mcu_in_row / mcu_size_divider)) +
                   (mcu_in_row / mcu_size_divider)) <
                  VP8DEC_MAX_SLICE_SIZE);
          printf
          ("Force to slice mode (over 16M) ==> Decoder Slice MB Set %u\n",
           dec_input.slice_height);
          forced_slice_mode = 1;
        }
#else
        /* 8190 and over 16M ==> force to slice mode */
        if((dec_input.slice_height == 0) &&
            (snap_shot) &&
            ((info.pic_width * info.pic_height) >
             VP8DEC_MAX_PIXEL_AMOUNT)) {
          do {
            dec_input.slice_height++;
          } while(((dec_input.slice_height * (mcu_in_row / mcu_size_divider)) +
                   (mcu_in_row / mcu_size_divider)) <
                  VP8DEC_MAX_SLICE_SIZE);
          printf
          ("Force to slice mode (over 16M) ==> Decoder Slice MB Set %u\n",
           dec_input.slice_height);
          forced_slice_mode = 1;
        }
#endif /* ASIC_TRACE_SUPPORT */
#endif
        DEBUG_PRINT(("[TB] Stream info:\n"));
        DEBUG_PRINT(("[TB] VP Version %u, Profile %u\n", info.vp_version, info.vp_profile));
        DEBUG_PRINT(("[TB] Frame size %ux%u\n", info.pic_width, info.pic_height));
        DEBUG_PRINT(("[TB] Coded size %ux%u\n", info.crop_params.crop_out_width, info.crop_params.crop_out_height));
        DEBUG_PRINT(("[TB] Scaled size %ux%u\n", info.scaled_width, info.scaled_height));
        DEBUG_PRINT(("[TB] Output format %s\n", info.output_format == DEC_OUT_FRM_YUV420SP
                     ? "DEC_OUT_FRM_YUV420SP" : "DEC_OUT_FRM_YUV420TILE"));

        if (user_mem_alloc && dec_format == DEC_INPUT_WEBP) {
          u32 size_luma;
          u32 size_chroma;
          u32 slice_height = 0;
          u32 width_y, width_c;

          width_y = info.pic_width;
          width_c = info.pic_width;

          for( i = 0 ; i < 16 ; ++i ) {
            if(user_alloc_luma[i].virtual_address)
              DWLFreeRefFrm(dwl_inst, &user_alloc_luma[i]);
            if(user_alloc_chroma[i].virtual_address)
              DWLFreeRefFrm(dwl_inst, &user_alloc_chroma[i]);
          }

          DEBUG_PRINT(("[TB] User allocated memory,width=%u,height=%u\n",
                       info.pic_width, info.pic_height));

          slice_height = dec_output.slice_height;
          if(forced_slice_mode)
            slice_height = dec_input.slice_height;
          size_luma = slice_height ?
                      (slice_height + 1) * 16 * width_y :
                      info.pic_height * width_y;

          size_chroma = slice_height ?
                        (slice_height + 1) * 8 * width_c :
                        info.pic_height * width_c / 2;

          SET_MEM_USAGE(user_alloc_luma[0].mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
          if (DWLMallocRefFrm(dwl_inst,
                              size_luma, &user_alloc_luma[0]) != DWL_OK) {
            fprintf(stderr,"UNABLE TO ALLOCATE PICTURE MEMORY\n");
            goto end;
          }
          SET_MEM_USAGE(user_alloc_chroma[0].mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
          if (DWLMallocRefFrm(dwl_inst,
                              size_chroma, &user_alloc_chroma[0]) != DWL_OK) {
            fprintf(stderr,"UNABLE TO ALLOCATE PICTURE MEMORY\n");
            goto end;
          }

          dec_input.p_pic_buffer_y = user_alloc_luma[0].virtual_address;
          dec_input.pic_buffer_bus_address_y = user_alloc_luma[0].bus_address;
          dec_input.p_pic_buffer_c = user_alloc_chroma[0].virtual_address;
          dec_input.pic_buffer_bus_address_c = user_alloc_chroma[0].bus_address;
        }
        START_SW_PERFORMANCE;

        /* Create output sink after output file names are determined. */
        if (yuvsink == NULL) {
          dec_format = DEC_INPUT_VP7;
          outfile_info.bitstream_format = (dec_format == DEC_INPUT_VP7 ?
                                           BITSTREAM_VP7 :
                                           (dec_format == DEC_INPUT_VP8 ?
                                            BITSTREAM_VP8 : BITSTREAM_WEBP));
          outfile_info.pic_width = info.pic_width;
          outfile_info.pic_height = info.pic_height;
          outfile_info.bit_depth = (info.bit_depth_luma == 8 && info.bit_depth_chroma == 8) ? 8 : 10;
          outfile_info.is_interlaced = info.is_interlaced;
          params.compress_bypass = TRUE; /* G1 don't support rfc */
          GenerateOutputFileName(&params, &outfile_info);
          if ((yuvsink = CreateYuvSink(&params)) == NULL) {
            fprintf(stderr, "[TB] Failed to create YUV sink\n");
            goto end;
          }
        }

        ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
        END_SW_PERFORMANCE;
      }
      if (ret == DEC_WAITING_FOR_BUFFER) {
        DEBUG_PRINT(("[TB] Waiting for frame buffers\n"));
        struct DWLLinearMem mem = {0};

        ret = VCDecGetBufferInfo(dec_inst, &hbuf);
        DEBUG_PRINT(("[TB] VCDecGetBufferInfo ret %d\n", ret));
        DEBUG_PRINT(("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
                     (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));
        while (ret == DEC_WAITING_FOR_BUFFER) {
          if (hbuf.buf_to_free.virtual_address != NULL) {
            add_extra_flag = 0;
            ReleaseExtBuffers();
            buffer_release_flag = 1;
            num_buffers = 0;
          }
          ret = VCDecGetBufferInfo(dec_inst, &hbuf);
        }

        buffer_size = hbuf.next_buf_size;
        if(buffer_release_flag && hbuf.next_buf_size) {
          /* Only add minimum required buffers at first. */
          //extra_buffer_num = hbuf.buf_num - min_buffer_num;
#ifdef SUPPORT_DMA
          mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
          for(i=0; i<hbuf.buf_num; i++) {
            if (pp_enabled) {
              SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
              DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
            } else {
              SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
              DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
            }
            ret = VCDecAddBuffer(dec_inst, &mem);
            DEBUG_PRINT(("[TB] VCDecAddBuffer ret %d\n", ret));
            if(ret != DEC_OK && ret != DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              ext_buffers[i] = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers = hbuf.buf_num;
          add_extra_flag = 1;
        }
        ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
        END_SW_PERFORMANCE;
      }
      if (ret == DEC_STRM_PROCESSED) {
        ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
        END_SW_PERFORMANCE;
      }
      else if (ret != DEC_PIC_DECODED &&
               ret != DEC_SLICE_RDY) {
        if (ret != DEC_NO_DECODING_BUFFER)
          fprintf(stderr, "Decoding error on pic_id %u: %s [%d]\n", dec_input.pic_id, VCDecRetStr(ret), ret);
        continue;
      }

      if (!output_thread_run) {
        for (u32 i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          if (fout[i] == NULL) {
            fout[i] = fopen(params.out_file_name[i], "w");
          } else {
            fout[i] = fopen(params.out_file_name[i], "a");
          }
        }
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, vp8_output_thread, &params);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      webp_loop = (ret == DEC_SLICE_RDY);

      // pic_rdy = 1;

      START_SW_PERFORMANCE;
      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      if (use_peek_output && (VCDecPeek(dec_inst, &dec_picture) == DEC_PIC_RDY)) {
        END_SW_PERFORMANCE;
        u32 ext_id = 0xFFFF;
        u32* host_base = NULL;
        struct DecPicture* in = &dec_picture.pictures[0];

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
        yuvsink->WritePicture(yuvsink->inst, in, 0);
        SwClearHostOutbaseAfterWriteFile(host_base, in);
        if (host_base != NULL) {
          DWLfree(host_base);
          tool_params.ext_buffers[ext_id].virtual_address = NULL;
        }
        END_SW_PERFORMANCE;
      }
      END_SW_PERFORMANCE;
    }
    if (ret != DEC_SLICE_RDY)
      pic_decode_number++;
#if 0
    if (pic_decode_number == 10) {
      enum DecRet tmp_ret = VP8DecAbort(dec_inst);
      tmp_ret = VP8DecAbortAfter(dec_inst);
      if(reader)
        rdr_close(reader);
      reader = rdr_open(argv[argc-1]);
    }
#endif
  } while( more_frames && (pic_decode_number != max_num_pics || !max_num_pics) );

  START_SW_PERFORMANCE;

end:
  add_buffer_thread_run = 0;

  VCDecEndOfStream(dec_inst);

  if (output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }

  if(stream_mem.virtual_address)
    DWLFreeLinear(dwl_inst, &stream_mem);
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      if (config.table_3dlut_buffer.virtual_address) {
        DWLFreeLinear(dwl_inst, &config.table_3dlut_buffer);
        config.table_3dlut_buffer.virtual_address = NULL;
      }
    }
  }
  if (user_mem_alloc) {
    for( i = 0 ; i < 16 ; ++i ) {
      if(user_alloc_luma[i].virtual_address)
        DWLFreeRefFrm(dwl_inst, &user_alloc_luma[i]);
      if(user_alloc_chroma[i].virtual_address)
        DWLFreeRefFrm(dwl_inst, &user_alloc_chroma[i]);
    }
  }

  printf("Pictures decoded: %u\n", pic_decode_number);
  if(cycle_count && pic_decode_number)
    printf("Average cycles/MB: %4u\n", cycle_count/pic_decode_number);

  START_SW_PERFORMANCE;
  VCDecRelease(dec_inst);
  END_SW_PERFORMANCE;
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  DWLRelease(dwl_inst);
  VCDecLogDestory();

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  if(reader)
    rdr_close(reader);
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if(fout[i]) {
      fclose(fout[i]);
    }
  }

  PrintOutputFileName(&params);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if(p_frame_pic[i])
      free(p_frame_pic[i]);
  }

  if (pic_big_endian)
    free(pic_big_endian);

  return 0;
}


void writeSlice(FILE * fp, struct DecPictures* dec_pic) {

  int luma_size = include_strides ? dec_pic->pictures[0].picture_info.luma_stride * dec_pic->pictures[0].sequence_info.pic_height :
                  dec_pic->pictures[0].sequence_info.pic_width * dec_pic->pictures[0].sequence_info.pic_height;
//  int slice_size = include_strides ? dec_pic->pictures[0].picture_info.luma_stride * dec_pic->pictures[0].picture_info.num_slice_rows :
//                   dec_pic->pictures[0].sequence_info.pic_width * dec_pic->pictures[0].picture_info.num_slice_rows;
  int chroma_size = include_strides ? dec_pic->pictures[0].picture_info.chroma_stride  * dec_pic->pictures[0].sequence_info.pic_height / 2:
                    dec_pic->pictures[0].sequence_info.pic_width * dec_pic->pictures[0].sequence_info.pic_height / 2;
  static u8 *tmp_ch = NULL;
  static u32 row_count = 0;
  u32 slice_rows;
  u32 luma_stride, chroma_stride;
  u32 i, j;
  u8 *ptr = (u8*)dec_pic->pictures[0].picture_info.format;

  tmp_ch = (u8*)malloc(chroma_size);
   if(tmp_ch == NULL)
   	return;


  slice_rows = dec_pic->pictures[0].picture_info.num_slice_rows;

  DEBUG_PRINT(("[TB] WRITING SLICE, rows %u\n",dec_pic->pictures[0].picture_info.num_slice_rows));

  luma_stride = dec_pic->pictures[0].picture_info.luma_stride;
  chroma_stride = dec_pic->pictures[0].picture_info.chroma_stride ;

  if(!height_crop) {
    /*fwrite(dec_pic->p_output_frame, 1, slice_size, fp);*/
    for ( i = 0 ; i < dec_pic->pictures[0].picture_info.num_slice_rows ; ++i ) {
      fwrite(ptr, 1, include_strides ? luma_stride : dec_pic->pictures[0].sequence_info.crop_params.crop_out_width, fp );
      ptr += luma_stride;
    }
  } else {
    if(row_count + slice_rows > dec_pic->pictures[0].sequence_info.scaled_height )
      slice_rows -=
        (dec_pic->pictures[0].sequence_info.pic_height - dec_pic->pictures[0].sequence_info.scaled_height);
    for ( i = 0 ; i < slice_rows ; ++i ) {
      fwrite(ptr, 1, include_strides ? luma_stride : dec_pic->pictures[0].sequence_info.crop_params.crop_out_width, fp );
      ptr += luma_stride;
    }
  }

  for( i = 0 ; i < dec_pic->pictures[0].picture_info.num_slice_rows/2 ; ++i ) {
    memcpy(tmp_ch + ((i+(row_count/2)) * (include_strides ? chroma_stride : dec_pic->pictures[0].sequence_info.pic_width)),
           dec_pic->pictures[0].chroma.virtual_address + (i*chroma_stride)/4,
           include_strides ? chroma_stride : dec_pic->pictures[0].sequence_info.pic_width );
  }
  /*    memcpy(tmp_ch + row_count/2 * dec_pic->frame_width, dec_pic->p_output_frame_c,
          slice_size/2);*/

  row_count += dec_pic->pictures[0].picture_info.num_slice_rows;

  if (row_count == dec_pic->pictures[0].sequence_info.pic_height) {
    if(!height_crop) {
      if (!planar_output) {
        fwrite(tmp_ch, luma_size/2, 1, fp);
        /*
        ptr = tmp_ch;
        for ( i = 0 ; i < dec_pic->pictures[0].picture_info.num_slice_rows/2 ; ++i )
        {
            fwrite(ptr, 1, dec_pic->coded_width, fp );
            ptr += chroma_stride;
        } */
      } else {
        u32 i, tmp;
        tmp = chroma_size / 2;
        for(i = 0; i < tmp; i++)
          fwrite(tmp_ch + i * 2, 1, 1, fp);
        for(i = 0; i < tmp; i++)
          fwrite(tmp_ch + 1 + i * 2, 1, 1, fp);
      }
    } else {
      if(!planar_output) {
        ptr = tmp_ch;
        for ( i = 0 ; i < dec_pic->pictures[0].sequence_info.scaled_height/2 ; ++i ) {
          fwrite(ptr, 1, dec_pic->pictures[0].sequence_info.crop_params.crop_out_width, fp );
          ptr += dec_pic->pictures[0].sequence_info.pic_width;
        }
      } else {
        ptr = tmp_ch;
        for ( i = 0 ; i < dec_pic->pictures[0].sequence_info.scaled_height/2 ; ++i ) {
          for( j = 0 ; j < dec_pic->pictures[0].sequence_info.crop_params.crop_out_width/2 ; ++j )
            fwrite(ptr + 2*j, 1, 1, fp );
          ptr += dec_pic->pictures[0].sequence_info.pic_width;
        }
        ptr = tmp_ch+1;
        for ( i = 0 ; i < dec_pic->pictures[0].sequence_info.scaled_height/2 ; ++i ) {
          for( j = 0 ; j < dec_pic->pictures[0].sequence_info.crop_params.crop_out_width/2 ; ++j )
            fwrite(ptr + 2*j, 1, 1, fp );
          ptr += dec_pic->pictures[0].sequence_info.pic_width;
        }
      }
    }
  }

}

u32 FfReadFrame( reader_inst reader, const u8 *buffer, u32 max_buffer_size,
                 u32 *frame_size, u32 pedantic ,  u32 pic_decode_number) {
  u32 ret = OK;
  ret = rdr_read_frame(reader, buffer, max_buffer_size, frame_size, pedantic, pic_decode_number);
  return ret;
}
