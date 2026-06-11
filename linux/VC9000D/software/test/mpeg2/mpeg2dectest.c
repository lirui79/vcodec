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

#include <limits.h>
#include <inttypes.h>
#include <ctype.h>
#include "dec_log.h"
#ifndef MPEG2DEC_EXTERNAL_ALLOC_DISABLE
#include <fcntl.h>
#endif

#include "vcdecapi.h"
#ifdef USE_EFENCE
#include "efence.h"
#endif

#include "regdrv.h"
#include "ppu.h"

#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

#include "deccfg.h"
#include "tb_cfg.h"
#include "common_sink.h"
#include "tb_sw_performance.h"
#include "command_line_parser.h"
#include "commonconfig.h"
#include "vcd_tools.h"

/*#define DISABLE_WHOLE_STREAM_BUFFERING 0*/
#define DEFAULT -1
#define VOP_START_CODE 0xb6010000

/* Size of stream buffer */
#define STREAMBUFFER_BLOCKSIZE 0xfffff
#define MPEG2_WHOLE_STREAM_SAFETY_LIMIT (10*10*1024)
#define MAX_BUFFERS 34

#define MPEG2_FRAME_BUFFER_SIZE ((1280 * 720 * 3) / 2)  /* 720p frame */
#define MPEG2_NUM_BUFFERS 3 /* number of output buffers for ext alloc */
#define ASSERT(expr) assert(expr)

/* Function prototypes */

void printTimeCode(struct DecTime *timecode);
static u32 readDecodeUnit(FILE * fp, u8 * frame_buffer);
void decRet(enum DecRet ret);
void decNextPictureRet(enum DecRet ret);
void printMpeg2Version(u32 client_type);
i32 AllocatePicBuffers(struct DWLLinearMem * buffer);
void printMpeg2PicCodingType(u32 pic_type);

/* secure mdoe */
u32 secure_mode;
/* stream start address */
u8 *byte_strm_start;

/* stream used in SW decode */
u32 trace_used_stream = 0;
u32 previous_used = 0;
/* SW/SW testing, read stream trace file */
FILE *f_stream_trace = NULL;
static u32 StartCode;
i32 strm_rew = 0;
u32 length = 0;
u32 write_output = 1;
u32 crop_output = 0;
u8 disable_resync = 0;
u8 strm_end = 0;
u8 *stream_stop = NULL;

u32 stream_size = 0;
u32 stop_decoding = 0;
struct TBCfg tb_cfg;

/* Give stream to decode as one chunk */
u32 whole_stream_mode = 0;
u32 cumulative_error_mbs = 0;

u32 planar_output = 0;

/* flag to enable md5sum output */
u32 md5sum = 0;

u32 use_peek_output = 0;
enum DecSkipFrameMode skip_frame = 0;

u32 pic_display_number = 0;
u32 frame_number = 0;
u32 cycle_count = 0; /* Sum of average cycles/mb counts */

u32 dpb_mode = DEC_DPB_FRAME;
u32 convert_to_frame_dpb = 0;
u32 convert_tiled_output = 0;

FILE *findex = NULL;
u32 save_index = 0;
u32 use_index = 0;
off64_t cur_index = 0;
size_t next_index = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 pp_enabled = 0;

#ifdef MPEG2_EVALUATION
extern u32 g_hw_ver;
#endif

/* These global values are found from commonconfig.c.
 * partial tb_cfg.dec_params parameters. */
extern struct DecParams dec_params;

#ifdef USE_RANDOM_ERROR_TEST
/* These global values are found from vcdecapi.c.
 * partial tb_cfg.tb_params parameters for random error injectionn. */
extern struct ErrorParams random_error_params;
#endif

typedef void *Mpeg2DecInst;
Mpeg2DecInst decoder;
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
u32 buffer_release_flag = 1;
struct TOOL_PARAMS tool_params;

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && num_buffers < MAX_BUFFERS) {
      struct DWLLinearMem mem = {0};
      i32 dwl_ret;
#ifdef SUPPORT_DMA
      mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
      if (pp_enabled)
        SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
      else
        SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
      if (pp_enabled)
        dwl_ret = DWLMallocLinear(dwl_inst, buffer_size, &mem);
      else
        dwl_ret = DWLMallocRefFrm(dwl_inst, buffer_size, &mem);
      if(dwl_ret == DWL_OK) {
        enum DecRet rv = VCDecAddBuffer(decoder, &mem);
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
  printf("[TB] Releasing %u external frame buffers\n", num_buffers);
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<num_buffers; i++) {
    printf("[TB] Freeing buffer %p\n", (void *)ext_buffers[i].virtual_address);
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

struct DecSequenceInfo Decinfo;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
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
      VCDecPictureConsumed(decoder, &buf_list[list_pop_index]);
      buf_status[list_pop_index] = 0;
      list_pop_index++;
      if(list_pop_index == 100)
        list_pop_index = 0;

      if(allocate_extra_buffers_in_output) {
        pthread_mutex_lock(&ext_buffer_contro);
        if(add_extra_flag && num_buffers < MAX_BUFFERS) {
          struct DWLLinearMem mem = {0};
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
            enum DecRet rv = VCDecAddBuffer(decoder, &mem);
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
    if(last_pic_flag &&  buf_status[list_pop_index] == 0) {
      sem_destroy(&buf_release_sem);
      break;
    }
    usleep(5000);
  }
  return (NULL);
}

/* Output thread entry point. */
static void* mpeg2_output_thread(void* arg) {
  struct DecPictures DecPic;
  u32 pic_display_number = 1;

  while(output_thread_run) {
    enum DecRet ret;
    u32 i;

    ret = VCDecNextPicture(decoder, &DecPic);
    if(ret == DEC_PIC_RDY) {
      struct DecPicture* in = &DecPic.pictures[0];
      u32 only_once = 0, ext_id = 0xFFFF;
      u32* host_base = NULL;

      if (in->sequence_info.is_interlaced &&
          in->picture_info.field_picture &&
          in->picture_info.output_other_field) {
          continue; // do not send one field frame twice
      }
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        in = &DecPic.pictures[i];
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
          /* print result */
          decNextPictureRet(ret);
          /* printf info */
          printf("PIC %2u/%2u, type %s, ", pic_display_number, (in->picture_info.pic_id + 1),
                 in->picture_info.is_intra_frame ? "key picture" : "non key picture");
          /* pic coding type */
          if(in->picture_info.field_picture && !in->picture_info.top_field) {
            printMpeg2PicCodingType(in->picture_info.pic_coding_type_field);
          } else {
            printMpeg2PicCodingType(DecPic.pictures[0].picture_info.pic_coding_type);
          }
          if (in->picture_info.cycles_per_mb) {
            cycle_count += in->picture_info.cycles_per_mb;
            printf("%4u cycles / mb,", in->picture_info.cycles_per_mb);
          }
          printf("%u x %u, Crop: (%u, %u), %u x %u\n",
                 in->sequence_info.scaled_width,
                 in->sequence_info.scaled_height,
                 in->sequence_info.crop_params.crop_left_offset,
                 in->sequence_info.crop_params.crop_top_offset,
                 in->sequence_info.crop_params.crop_out_width,
                 in->sequence_info.crop_params.crop_out_height);
          if(in->picture_info.field_picture)
            printf("[TB] %s", in->picture_info.top_field ? "top field, " : "bottom field, ");
          else
            printf("[TB] frame picture, ");
          printTimeCode(&(in->picture_info.time_code));
          if(in->picture_info.nbr_of_err_mbs) {
            printf("[TB] , %u/%u error mbs\n",
                   in->picture_info.nbr_of_err_mbs,
                   (in->sequence_info.pic_width >> 4) *
                   (in->sequence_info.pic_height >> 4));
            cumulative_error_mbs += in->picture_info.nbr_of_err_mbs;
          } else {
            printf("[TB] \n");
          }
          printf("[TB] in->picture_info.first_field %u\n", in->picture_info.first_field);
          yuvsink->WritePicture(yuvsink->inst, in, i);
        }
        SwClearHostOutbaseAfterWriteFile(host_base, in);
      }
      if (host_base != NULL) {
        DWLfree(host_base);
        tool_params.ext_buffers[ext_id].virtual_address = NULL;
      }
      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = DecPic;
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
//        usleep(50000);
  }
  return (NULL);
}

int main(int argc, char **argv) {
  FILE *f_tbcfg;
  u8 *p_strm_data = 0;
  u32 max_num_frames;   /* todo! */
  u32 num_frame_buffers = 0;

  u32 i, stream_len = 0;
  u32 vp_num = 0;
  u32 allocate_buffers = 0;    /* Allocate buffers in test bench */
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  u32 tmp = 0;
  /*
   * Decoder API structures
   */
  struct DecBufferInfo hbuf;
  enum DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  enum DecRet ret;
  enum DecRet info_ret;
  struct DecInputParameters DecIn;
  DWLmemset(&DecIn, 0, sizeof(struct DecInputParameters));
  struct DecOutput DecOut;
  struct DWLLinearMem stream_mem;
  struct DecPictures DecPic;
  u32 pic_id = 0;

  FILE *f_in = NULL;
  struct OutFileInfo outfile_info;
  struct DWLLinearMem pic_buffer[MPEG2_NUM_BUFFERS] = { {0} };

  u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
  u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
  u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
  u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
  u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
  u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
  i32 corrupted_bytes = 0;

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
    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000+atoi(tm_buf);
    if (tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1 ) {
      fprintf(stderr,
              "EVALUATION PERIOD EXPIRED.\n"
              "Please contact On2 Sales.\n");
      return -1;
    }
  }

  INIT_SW_PERFORMANCE;

  /* Use common command line parser to parse options. */
  if ((ret = ParseParams(argc, argv, &params))) {
    if (ret == 1) {
      printf("[TB] Failed to parse params.\n\n");
      PrintUsage(argv[0], MPEG2DEC);
      ret = 1;
      return 1;
    }
    else {
      ret = 0;
      return 0;
    }
  }

  /* set secure mode */
  secure_mode = (params.decoder_mode & DEC_SECURITY) ? 1 : 0;
  /* get the value form params */
  write_output = (params.sink_type != SINK_NULL);
  pp_enabled = params.pp_enabled;
  planar_output = params.ppu_cfg[0].planar;
  max_num_frames = params.num_of_decoded_pics;
  use_peek_output = params.disable_display_order;
  whole_stream_mode = params.read_mode;
  crop_output = params.ppu_cfg[0].crop.enabled;
  md5sum = (params.sink_type == SINK_MD5_PICTURE);
  align = params.align;
  convert_tiled_output = params.convert_tiled_output;
  num_frame_buffers = params.num_buffers;
  if (num_frame_buffers > MAX_BUFFERS) num_frame_buffers = MAX_BUFFERS;
  skip_frame = params.skip_frame;
  dpb_mode = params.dpb_mode;
  convert_to_frame_dpb = params.convert_to_frame_dpb;
  use_extra_buffers = params.use_extra_buffers;
  allocate_extra_buffers_in_output = params.allocate_extra_buffers_in_output;
  if (params.stream_trace != NULL)
    f_stream_trace = fopen(params.stream_trace, "r");
#if defined(ASIC_TRACE_SUPPORT) || defined(SYSTEM_VERIFICATION)
  use_mpeg2_idct = params.use_ref_idct;
#endif
  save_index = params.save_index;
  // tool_params.use_mp_output = params.use_mp_output;
  // tool_params.use_separated_pp = params.use_separated_pp;
  tool_params.ext_buffers = ext_buffers;
  tool_params.max_buffers = &num_buffers;

  /* open data file */
  f_in = fopen(argv[argc - 1], "rb");
  if(f_in == NULL) {
    printf("[TB] Unable to open input file %s\n", argv[argc - 1]);
    exit(100);
  }

  /* open index file for saving */
  if(save_index) {
    findex = fopen("stream.cfg", "w");
    if(findex == NULL) {
      printf("[TB] UNABLE TO OPEN INDEX FILE\n");
	  fclose(f_in);
      return -1;
    }
  } else {
    findex = fopen("stream.cfg", "r");
    if(findex != NULL) {
      use_index = 1;
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
      fclose(f_in);
      return -1;
    }
    if(TBCheckCfg(&tb_cfg) != 0) {
	    fclose(f_in);
      return -1;
    }
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

  /*TBPrintCfg(&tb_cfg); */
  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  printf("[TB] Decoder Clock Gating %u\n", clock_gating);
  printf("[TB] Decoder Data Discard %u\n", data_discard);
  printf("[TB] Decoder Latency Compensation %u\n", latency_comp);
  printf("[TB] Decoder Output Picture Endian %u\n", output_picture_endian);
  printf("[TB] Decoder Bus Burst Length %u\n", bus_burst_length);
  printf("[TB] Decoder Asic Service Priority %u\n", asic_service_priority);
  printf("[TB] Decoder Disable Service Merge %u\n", service_merge_disable);

#ifdef USE_RANDOM_ERROR_TEST
  random_error_params.seed = tb_cfg.tb_params.seed_rnd;
  strcpy(random_error_params.truncate_stream_odds, tb_cfg.tb_params.stream_truncate);
  strcpy(random_error_params.swap_bit_odds, tb_cfg.tb_params.stream_bit_swap);
  strcpy(random_error_params.packet_loss_odds, tb_cfg.tb_params.stream_packet_loss);
#endif
  disable_resync = TBGetTBPacketByPacket(&tb_cfg);

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;

  length = STREAMBUFFER_BLOCKSIZE;

  rewind(f_in);

  /* check size of the input file -> length of the stream in bytes */
  fseek(f_in, 0L, SEEK_END);
  stream_size = (u32) ftell(f_in);
  rewind(f_in);

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
    printf("[TB] UNABLE TO OPEN TRACE FILES(S)\n");
  }
#endif

  dwl_init.client_type = DWL_CLIENT_TYPE_MPEG2_DEC;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
  tool_params.dwl_inst = dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto end2;
  }
  printMpeg2Version(dwl_init.client_type);

  /* Initialize the decoder */
  init_config.codec = DEC_MPEG2;
  init_config.error_handling = params.error_handling;
  init_config.error_ratio = params.error_ratio;
  init_config.num_frame_buffers = num_frame_buffers;
  init_config.use_adaptive_buffers = 1;
  init_config.guard_size = 0;
  init_config.dwl_inst = dwl_inst;

  /* for color remapping (3dlut) */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      u32 size = 18 * 18 * 18 * 3 * sizeof(u16);
      // ppu_int_cfg.table_3dlut.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      config.table_3dlut_buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                        DWL_MEM_TYPE_CPU;
      if (DWLMallocLinear(dwl_inst, size, &config.table_3dlut_buffer)) {
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

  START_SW_PERFORMANCE;
  ret = VCDecInit((const void**)&decoder, &init_config);
  END_SW_PERFORMANCE;

  if(ret != DEC_OK) {
    printf("[TB] Could not initialize decoder\n");
    goto end2;
  }

  /* Set ref buffer test mode */
#ifdef SUPPORT_DMA
  stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if(DWLMallocLinear(dwl_inst, STREAMBUFFER_BLOCKSIZE, &stream_mem) != DWL_OK) {
    printf(("[TB]UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end2;
  }
  byte_strm_start = (u8 *) stream_mem.virtual_address;
  if (byte_strm_start == NULL) {
    printf(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto end2;
  }
  stream_stop = byte_strm_start + length;
  DecIn.skip_frame = skip_frame;
  DecIn.stream = byte_strm_start;
  DecIn.stream_bus_address = stream_mem.bus_address;
  DecIn.stream_buffer = stream_mem;

  /* allocate output buffers if necessary */
  if(allocate_buffers) {
    if(AllocatePicBuffers(pic_buffer))
      goto end2;
  }

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  info_ret = VCDecGetInfo(decoder, &Decinfo);
  END_SW_PERFORMANCE;
  if(info_ret) {
    decRet(info_ret);
  }

  p_strm_data = (u8 *) DecIn.stream;

  /* Read sequence headers */
  stream_len = readDecodeUnit(f_in, p_strm_data);

  i = StartCode;
  /* decrease 4 because previous function call
   * read the first sequence start code */

  stream_len -= 4;
  DecIn.strm_len = stream_len;
  DecOut.data_left = 0;

  printf("[TB] Start decoding\n");

  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_MPEG2_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;
  do {
    if (ret != DEC_NO_DECODING_BUFFER)
      printf("[TB] DecIn.strm_len %u\n", DecIn.strm_len);
    DecIn.pic_id = pic_id;
    if(ret != DEC_STRM_PROCESSED &&
        ret != DEC_BUF_EMPTY &&
        ret != DEC_NO_DECODING_BUFFER &&
        ret != DEC_NONREF_PIC_SKIPPED )
      printf("[TB] Starting to decode picture ID %u\n", pic_id + 1);

    assert(DecOut.data_left == DecIn.strm_len || !DecOut.data_left);

    START_SW_PERFORMANCE;
    ret = VCDecDecode(decoder, &DecOut, &DecIn);
    END_SW_PERFORMANCE;
    decRet(ret);

    /*
     * Choose what to do now based on the decoder return value
     */

    switch (ret) {
    case DEC_HDRS_RDY: {
      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      info_ret = VCDecGetInfo(decoder, &Decinfo);
      END_SW_PERFORMANCE;
      if(info_ret) {
        decRet(info_ret);
      }

      if (Decinfo.is_interlaced)
        printf("[TB] INTERLACED SEQUENCE\n");

      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if(!params.ppu_cfg[i].enabled) continue;

        if(!params.ppu_cfg[i].crop.set_by_user) {
          params.ppu_cfg[i].crop.width = Decinfo.scaled_width;
          params.ppu_cfg[i].crop.height = Decinfo.scaled_height;
          params.ppu_cfg[i].crop.enabled = 1;
        }
      }

      pp_enabled = params.pp_enabled;
      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      tmp = VCDecSetInfo(decoder, &config);
      if (tmp != DEC_OK)
        goto end2;

      if(!frame_number) {
        /*disable_resync = 0; */
        /* #define MPEG2 1 in mpeg2hwd_cfg.h
           #define MPEG1 0 in mpeg2hwd_cfg.h */
        if(Decinfo.stream_format)
          printf("[TB] MPEG-2 stream\n");
        else
          printf("[TB] MPEG-1 stream\n");

        printf("[TB] Profile and level %u\n",
               Decinfo.profile_and_level_indication);
        switch (Decinfo.display_aspect_ratio) {
        case DEC_1_1:
          printf("[TB] Display Aspect ratio 1:1\n");
          break;
        case DEC_4_3:
          printf("[TB] Display Aspect ratio 4:3\n");
          break;
        case DEC_16_9:
          printf("[TB] Display Aspect ratio 16:9\n");
          break;
        case DEC_2_21_1:
          printf("[TB] Display Aspect ratio 2.21:1\n");
          break;
        }
        printf("[TB] Output format %s\n",
               Decinfo.output_format == DEC_OUT_FRM_YUV420SP
               ? "DEC_OUT_FRM_YUV420SP" :
               "DEC_OUT_FRM_TILED_4X4");
      }

      if (ret != DEC_NO_DECODING_BUFFER)
        printf("[TB] DecOut.data_left %u \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.strm_len - DecOut.data_left);
        DecIn.strm_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {
        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          /* stream ended */
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.strm_len = stream_len;

        corrupted_bytes = 0;

      }

      /* Create output sink after output file names are determined. */
      if (yuvsink == NULL) {
        outfile_info.bitstream_format = BITSTREAM_MPEG2;
        outfile_info.pic_width = Decinfo.pic_width;
        outfile_info.pic_height = Decinfo.pic_height;
        outfile_info.bit_depth = (Decinfo.bit_depth_luma == 8 && Decinfo.bit_depth_chroma == 8) ? 8 : 10;
        outfile_info.is_interlaced = Decinfo.is_interlaced;
        params.compress_bypass = TRUE; /* G1 don't support rfc */
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          goto end2;
        }
      }

      break;
    }
    case DEC_WAITING_FOR_BUFFER:
      rv = VCDecGetBufferInfo(decoder, &hbuf);
      printf("[TB] MREG2DecGetBufferInfo ret %d\n", rv);
      printf("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
             (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
      if (hbuf.buf_to_free.virtual_address != NULL) {
        add_extra_flag = 0;
        ReleaseExtBuffers();
        buffer_release_flag = 1;
        num_buffers = 0;
      }
      if(buffer_release_flag && hbuf.next_buf_size) {
        /* Only add minimum required buffers at first. */
        //extra_buffer_num = hbuf.buf_num - min_buffer_num;
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem = {0};
#ifdef SUPPORT_DMA
        mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
        for(i=0; i<hbuf.buf_num; i++) {
          SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_TMP_EXTRA, secure_mode);
          if (pp_enabled) {
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          } else {
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          }
          rv = VCDecAddBuffer(decoder, &mem);
          printf("[TB] VCDecAddBuffer ret %d\n", rv);
          if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
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
      break;

    case DEC_PIC_DECODED:
      /* Picture is ready */
      pic_id++;
#if 0
      if (pic_id == 10) {
        info_ret = VCDecAbort(decoder);
        info_ret = VCDecAbortAfter(decoder);
        pic_id = 0;
        rewind(f_in);

        /* Read sequence headers */
        stream_len = readDecodeUnit(f_in, p_strm_data);

        i = StartCode;
        /* decrease 4 because previous function call
         * read the first sequence start code */

        stream_len -= 4;
        DecIn.strm_len = stream_len;
        DecIn.stream = byte_strm_start;
        DecIn.stream_bus_address = stream_mem.bus_address;
        DecOut.data_left = 0;
        break;
      }
#endif

      /* Read what kind of stream is coming */
      info_ret = VCDecGetInfo(decoder, &Decinfo);
      if(info_ret) {
        decRet(info_ret);
      }
      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, mpeg2_output_thread, &params);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if (use_peek_output &&
          VCDecPeek(decoder, &DecPic) == DEC_PIC_RDY) {
        struct DecPicture* in = &DecPic.pictures[0];
        u32* host_base = NULL;
        u32 ext_id = 0xFFFF;

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

        pic_display_number++;
        printf("[TB] DECPIC %u, %s", in->picture_info.pic_id,
               in->picture_info.is_intra_frame ? "key picture, " :
               "non key picture, ");
        if (in->picture_info.cycles_per_mb) {
          cycle_count += in->picture_info.cycles_per_mb;
          printf("[TB]  %4u cycles / mb,", in->picture_info.cycles_per_mb);
        }
        /* pic coding type */
        printMpeg2PicCodingType(in->picture_info.pic_coding_type);
        yuvsink->WritePicture(yuvsink->inst, in, 0);
        SwClearHostOutbaseAfterWriteFile(host_base, in);
        if (host_base != NULL) {
          DWLfree(host_base);
          tool_params.ext_buffers[ext_id].virtual_address = NULL;
        }
      }

      frame_number++;
      vp_num = 0;

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      if (ret != DEC_NO_DECODING_BUFFER)
        printf("[TB] DecOut.data_left %u \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.strm_len - DecOut.data_left);
        DecIn.strm_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {

        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.strm_len = stream_len;

        corrupted_bytes = 0;

      }

      if(max_num_frames && (frame_number >= max_num_frames)) {
        printf("[TB]  Max num of pictures reached\n");
        DecIn.strm_len = 0;
        goto end2;
      }

      break;

    case DEC_DISCARD_INTERNAL:
    case DEC_STRM_PROCESSED:
    case DEC_BUF_EMPTY:
    case DEC_NONREF_PIC_SKIPPED:
      fprintf(stdout,
              "[TB] Frame Number: %u, pic: %u\n", vp_num++, frame_number);
    /* Used to indicate that picture decoding needs to
     * finalized prior to corrupting next picture */
#ifdef GET_FREE_BUFFER_NON_BLOCK
    case DEC_NO_DECODING_BUFFER:
    /* Just for simulation: if no buffer, sleep 0.5 second and try decoding again. */
#endif

    /* fallthrough */

    case DEC_OK:

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      info_ret = VCDecGetInfo(decoder, &Decinfo);
      END_SW_PERFORMANCE;

      if(info_ret) {
        decRet(info_ret);
      }

      /*
       **  Write output picture to the file
       */

      /*
       *    Read next decode unit. Because readDecodeUnit
       *   reads VOP start code in previous
       *   function call, Insert this start code
       *   in to first word
       *   of stream buffer, and increase
       *   stream buffer pointer by 4 in
       *   the function call.
       */
      if (ret != DEC_NO_DECODING_BUFFER)
        printf("[TB] DecOut.data_left %u \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.strm_len - DecOut.data_left);
        DecIn.strm_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {

        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.strm_len = stream_len;

        corrupted_bytes = 0;

      }

      break;

    case DEC_PARAM_ERROR:
      printf("[TB] INCORRECT STREAM PARAMS\n");
      goto end2;
      break;

    case DEC_STRM_ERROR:
      printf("[TB] STREAM ERROR\n");

      printf("[TB] DecOut.data_left %u \n", DecOut.data_left);
      if(DecOut.data_left) {
        corrupted_bytes -= (DecIn.strm_len - DecOut.data_left);
        DecIn.strm_len = DecOut.data_left;
        DecIn.stream = DecOut.strm_curr_pos;
        DecIn.stream_bus_address = DecOut.strm_curr_bus_address;
      } else {

        *(u32 *) p_strm_data = StartCode;
        DecIn.stream = (u8 *) p_strm_data;
        DecIn.stream_bus_address = stream_mem.bus_address;

        if(strm_end) {
          stream_len = 0;
          DecIn.stream = NULL;
        } else {
          /*u32 streamPacketLossTmp = stream_packet_loss;

          if(!pic_rdy)
              stream_packet_loss = 0;*/
          stream_len = readDecodeUnit(f_in, p_strm_data + 4);
          /*stream_packet_loss = streamPacketLossTmp;*/
        }
        DecIn.strm_len = stream_len;

      }
      break;

    default:
      decRet(ret);
      goto end2;
    }
    /*
     * While there is stream
     */

  } while(DecIn.strm_len > 0);
end2:

  /* Output buffered images also... */
  START_SW_PERFORMANCE;
  add_buffer_thread_run = 0;

  VCDecEndOfStream(decoder);

  if (output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }

  END_SW_PERFORMANCE;

  START_SW_PERFORMANCE;
  VCDecGetInfo(decoder, &Decinfo);
  END_SW_PERFORMANCE;

  /*
   * Release the decoder
   */
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
  START_SW_PERFORMANCE;
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  VCDecRelease(decoder);
  DWLRelease(dwl_inst);
  VCDecLogDestory();
  END_SW_PERFORMANCE;

  if(Decinfo.pic_width < 1921)
    printf("[TB] Width %u Height %u\n", Decinfo.pic_width,
           Decinfo.pic_height);
  if(cumulative_error_mbs) {
    printf("[TB] Cumulative errors: %u/%u macroblocks, ",
           cumulative_error_mbs,
           (Decinfo.pic_width >> 4) * (Decinfo.pic_height >> 4) *
           frame_number);
  }
  printf("[TB] decoded %u pictures\n", frame_number);
  if(cycle_count && frame_number)
    printf("[TB] Average cycles/MB: %4u\n", cycle_count/frame_number);

  if(f_in)
    fclose(f_in);

  if( f_stream_trace)
    fclose(f_stream_trace);

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  if(save_index || use_index) {
    fclose(findex);
  }

  PrintOutputFileName(&params);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  FINALIZE_SW_PERFORMANCE;

  if(cumulative_error_mbs || !frame_number) {
    printf("[TB] ERRORS FOUND\n");
    return (1);
  } else
    return (0);

}

/*------------------------------------------------------------------------------
        readDecodeUnit
        Description : search pic start code and read one decode unit at a time
------------------------------------------------------------------------------*/
static u32 readDecodeUnit(FILE * fp, u8 * frame_buffer) {

  u32 idx = 0, VopStart = 0;
  u8 temp = 0;
  int ret = 0;

  StartCode = 0;

  if(use_index) {
    u32 amount = 0;

    /* get next index */
    ret = fscanf(findex, "%zu", &next_index);
    if ( next_index >= 65536 ) return 0;

    amount = next_index - cur_index;

    /* read data */
    idx = fread(frame_buffer, 1, amount, fp);
    if (idx < 4) return 0;

    VopStart = 1;
    StartCode = ((frame_buffer[idx - 1] << 24) |
                 (frame_buffer[idx - 2] << 16) |
                 (frame_buffer[idx - 3] << 8) |
                 frame_buffer[idx - 4]);

    /* end of stream */
    if(next_index == stream_size) {
      strm_end = 1;
      idx += 4;
    }

    cur_index = next_index;
  } else {
    while(!VopStart) {

      ret = fread(&temp, sizeof(u8), 1, fp);

      if(feof(fp)) {

        fprintf(stdout, "TB: End of stream noticed in readDecodeUnit\n");
        strm_end = 1;
        idx += 4;
        break;
      }
      /* Reading the whole stream at once must be limited to buffer size */
      if((idx > (length - MPEG2_WHOLE_STREAM_SAFETY_LIMIT)) &&
          whole_stream_mode) {

        whole_stream_mode = 0;
        idx = 0;
        fseeko(fp, 0, SEEK_SET);
        continue;
      }

      frame_buffer[idx] = temp;

      if(idx >= (frame_buffer == byte_strm_start ? 4 : 3)) {
        if(!whole_stream_mode) {
          if(disable_resync) {
            /*-----------------------------------
                Slice by slice
            -----------------------------------*/
            if((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                (frame_buffer[idx - 1] == 0x01)) {
              if(frame_buffer[idx] > 0x00) {
                if(frame_buffer[idx] <= 0xAF) {
                  VopStart = 1;
                  StartCode = (((u32)frame_buffer[idx] << 24) |
                               ((u32)frame_buffer[idx - 1] << 16) |
                               ((u32)frame_buffer[idx - 2] << 8) |
                               (u32)frame_buffer[idx - 3]);
                  /*printf("[TB] SLICE FOUND\n");*/
                }
              } else if(frame_buffer[idx] == 0x00) {
                VopStart = 1;
                StartCode = (((u32)frame_buffer[idx] << 24) |
                             ((u32)frame_buffer[idx - 1] << 16) |
                             ((u32)frame_buffer[idx - 2] << 8) |
                             (u32)frame_buffer[idx - 3]);
                /* MPEG2 start code found */
              }
            }
          } else {
            /*-----------------------------------
                MPEG2 Start code
            -----------------------------------*/
            if(((frame_buffer[idx - 3] == 0x00) &&
                (frame_buffer[idx - 2] == 0x00) &&
                (((frame_buffer[idx - 1] == 0x01) &&
                  (frame_buffer[idx] == 0x00))))) {
              VopStart = 1;
              StartCode = (((u32)frame_buffer[idx] << 24) |
                           ((u32)frame_buffer[idx - 1] << 16) |
                           ((u32)frame_buffer[idx - 2] << 8) |
                           (u32)frame_buffer[idx - 3]);
              /* MPEG2 start code found */
            }
          }
        }
      }
      if(idx >= length) {
        fprintf(stdout, "idx = %u,lenght = %u \n", idx, length);
        fprintf(stdout, "[TB] Out Of Stream Buffer\n");
        break;
      }
      if(idx > strm_rew + 128) {
        idx -= strm_rew;
      }
      idx++;
    }
  }
  UNUSED(ret);

  if(save_index && !use_index) {
    fprintf(findex, "%" PRId64"\n", ftello64(fp));
  }

  trace_used_stream = previous_used;
  previous_used += idx;

  /*printf("[TB] READ DECODE UNIT %d\n", idx); */
  printf("[TB] No Packet Loss\n");
  return (idx);
}

/*------------------------------------------------------------------------------
        printTimeCode
        Description : Print out time code
------------------------------------------------------------------------------*/

void printTimeCode(struct DecTime * timecode) {

  fprintf(stdout, "hours %u,"
          " minutes %u,"
          " seconds %u,"
          " time_pictures %u\n",
          timecode->hours,
          timecode->minutes, timecode->seconds, timecode->pictures);
}

/*------------------------------------------------------------------------------
        decRet
        Description : Print out Decoder return values
------------------------------------------------------------------------------*/

void decRet(enum DecRet ret) {
  static enum DecRet prev_ret = DEC_NO_DECODING_BUFFER;
  if (ret == prev_ret) return;
  prev_ret = ret;
  printf("[TB] Decode result: ");

  switch (ret) {
  case DEC_OK:
    printf("DEC_OK\n");
    break;
  case DEC_DISCARD_INTERNAL:
    printf("DEC_DISCARD_INTERNAL\n");
    break;
  case DEC_STRM_PROCESSED:
    printf("DEC_STRM_PROCESSED\n");
    break;
  case DEC_BUF_EMPTY:
    printf("DEC_BUF_EMPTY\n");
    break;
  case DEC_NO_DECODING_BUFFER:
    printf("DEC_NO_DECODING_BUFFER\n");
    break;
  case DEC_NONREF_PIC_SKIPPED:
    printf("DEC_NONREF_PIC_SKIPPED\n");
    break;
  case DEC_PIC_RDY:
    printf("DEC_PIC_RDY\n");
    break;
  case DEC_HDRS_RDY:
    printf("DEC_HDRS_RDY\n");
    break;
  case DEC_PIC_DECODED:
    printf("DEC_PIC_DECODED\n");
    break;
  case DEC_PARAM_ERROR:
    printf("DEC_PARAM_ERROR\n");
    break;
  case DEC_STRM_ERROR:
    printf("DEC_STRM_ERROR\n");
    break;
  case DEC_NOT_INITIALIZED:
    printf("DEC_NOT_INITIALIZED\n");
    break;
  case DEC_MEMFAIL:
    printf("DEC_MEMFAIL\n");
    break;
  case DEC_DWL_ERROR:
    printf("DEC_DWL_ERROR\n");
    break;
  case DEC_HW_BUS_ERROR:
    printf("DEC_HW_BUS_ERROR\n");
    break;
  case DEC_SYSTEM_ERROR:
    printf("DEC_SYSTEM_ERROR\n");
    break;
  case DEC_HW_TIMEOUT:
    printf("DEC_HW_TIMEOUT\n");
    break;
  case DEC_HDRS_NOT_RDY:
    printf("DEC_HDRS_NOT_RDY\n");
    break;
  case DEC_STREAM_NOT_SUPPORTED:
    printf("DEC_STREAM_NOT_SUPPORTED\n");
    break;
  default:
    printf("Other %d\n", ret);
    break;
  }
}

/*------------------------------------------------------------------------------
        decNextPictureRet
        Description : Print out NextPicture return values
------------------------------------------------------------------------------*/
void decNextPictureRet(enum DecRet ret) {
  printf("[TB] next picture returns: ");

  decRet(ret);
}

/*------------------------------------------------------------------------------

    Function name:            printMpeg2PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printMpeg2PicCodingType(u32 pic_type) {
  switch (pic_type) {
  case DEC_PIC_TYPE_I:
    printf("DEC_PIC_TYPE_I, ");
    break;
  case DEC_PIC_TYPE_P:
    printf("DEC_PIC_TYPE_P, ");
    break;
  case DEC_PIC_TYPE_B:
    printf("DEC_PIC_TYPE_B, ");
    break;
  case DEC_PIC_TYPE_D:
    printf("DEC_PIC_TYPE_D, ");
    break;
  default:
    printf("Other %u\n", pic_type);
    break;
  }
}


/*------------------------------------------------------------------------------
        printMpeg2Version
        Description : Print out decoder version info
------------------------------------------------------------------------------*/

void printMpeg2Version(u32 client_type) {
  struct DecSwHwBuild build;
  struct DecApiVersion version;

  build = VCDecGetBuild(dwl_inst, client_type);
  version = VCDecGetAPIVersion();
  printf("[TB] VC9000 Decoder MPEG2 API version %u.%u.%u\n", version.major,
        version.minor, version.micro);
  printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
}

/*------------------------------------------------------------------------------

    Function name: allocatePicBuffers

    Functional description: Allocates frame buffers

    Inputs:     DWLLinearMem * buffer       pointers stored here

    Outputs:    NONE

    Returns:    nonzero if err

------------------------------------------------------------------------------*/

i32 AllocatePicBuffers(struct DWLLinearMem * buffer) {

  u32 offset = (MPEG2_FRAME_BUFFER_SIZE + 0xFFF) & ~(0xFFF);
  u32 i = 0;

#ifndef MPEG2DEC_EXTERNAL_ALLOC_DISABLE
  if (pp_enabled)
    SET_MEM_USAGE(buffer->mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
  else
    SET_MEM_USAGE(buffer->mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
  if(DWLMallocRefFrm(dwl_inst, offset * MPEG2_NUM_BUFFERS,
                     (struct DWLLinearMem *) buffer) != DWL_OK) {
    printf(("[TB] UNABLE TO ALLOCATE OUTPUT BUFFER MEMORY\n"));
    return 1;
  }

  buffer[1].virtual_address = buffer[0].virtual_address + offset / 4;
  buffer[1].bus_address = buffer[0].bus_address + offset;

  buffer[2].virtual_address = buffer[1].virtual_address + offset / 4;
  buffer[2].bus_address = buffer[1].bus_address + offset;

  for(i = 0; i < MPEG2_NUM_BUFFERS; i++) {
    printf("[TB] buff %u vir %p bus 0x%llx\n", i,
           (void *)buffer[i].virtual_address, buffer[i].bus_address);
  }

#endif
  return 0;
}
