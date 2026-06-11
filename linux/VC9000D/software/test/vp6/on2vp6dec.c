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

#include <getopt.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include "tb_cfg.h"
#include "command_line_parser.h"
#include "regdrv.h"
#include "common_sink.h"
#include "vcdecapi.h"
#include "dectypes.h"
#include "basetype.h"
#include "deccfg.h"
#include "dwl.h"
#include "dec_log.h"
#include "vcd_tools.h"
#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

typedef unsigned long ulong;
typedef unsigned short ushort;
typedef unsigned char uchar;

/* Size of stream buffer */
#define STREAMBUFFER_BLOCKSIZE 2*2097151
struct DWLLinearMem stream_mem;

u32 asic_trace_enabled; /* control flag for trace generation */
#ifdef ASIC_TRACE_SUPPORT
extern u32 asic_trace_enabled; /* control flag for trace generation */
/* Stuff to enable ref buffer model support */
//#include "../../../system/models/g1hw/ref_bufferd.h"
/* Ref buffer support stuff ends */
#endif

#ifdef VP6_EVALUATION
/* Stuff to enable ref buffer model support */
#include "../../../system/models/g1hw/ref_bufferd.h"
/* Ref buffer support stuff ends */
#endif

#ifdef VP6_EVALUATION
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

/* for tracing */
static void printVp6PicCodingType(u32 pic_type);

#define MAX_BUFFERS 34

/* secure mode */
u32 secure_mode;

const char *const short_options = "HO:N:m_mt_pab:EGZ";

addr_t input_stream_bus_address = 0;

u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;

u32 pic_big_endian_size = 0;
u8* pic_big_endian = NULL;

u32 disable_output_writing = 0;
i32 corrupted_bytes = 0;  /*  */
u32 num_buffers = 3;
u32 convert_tiled_output = 0;
u32 md5sum = 0;
u32 cycle_count = 0; /* Sum of average cycles/mb counts */

u32 use_peek_output = 0;
u32 ds_ratio_x, ds_ratio_y;
u32 pp_tile_out = 0;    /* PP tiled output */
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 pp_enabled = 0;
/* user input arguments */
struct TestParams params;
typedef void *VP6DecInst;
VP6DecInst dec_inst;
YuvSink* yuvsink; /* Yuvsink instance. */
const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size;
u32 external_buf_num;  /* external buffers allocated yet. */
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
    if(add_extra_flag && (external_buf_num < MAX_BUFFERS)) {
      struct DWLLinearMem mem= {0};
      i32 dwl_ret;
#ifdef SUPPORT_DMA
      mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_TO_HOST;
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
          ext_buffers[external_buf_num++] = mem;
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
  printf("[TB] Releasing %u external frame buffers\n", external_buf_num);
  pthread_mutex_lock(&ext_buffer_contro);
  for(i=0; i<external_buf_num; i++) {
    printf("[TB] Freeing buffer %p\n", (void *)ext_buffers[i].virtual_address);
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &ext_buffers[i]);
    else
      DWLFreeRefFrm(dwl_inst, &ext_buffers[i]);
    DWLmemset(&ext_buffers[i], 0, sizeof(ext_buffers[i]));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

typedef struct {
  const char *input;
  char output[DEC_MAX_OUT_COUNT][256];
  const char *pp_cfg;
  int last_frame;
  int md5;
  int planar;
  int alpha;
  int f,d,s;
} options_s;

struct TBCfg tb_cfg;

pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;
options_s options;

sem_t buf_release_sem;
struct DecPictures buf_list[100];

u32 buf_status[100] = {0};
u32 list_pop_index = 0;
u32 list_push_index = 0;
u32 last_pic_flag = 0;
unsigned int current_video_frame = 0;

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
        if(add_extra_flag && (external_buf_num < MAX_BUFFERS)) {
          struct DWLLinearMem mem = {0};
          i32 dwl_ret;
#ifdef SUPPORT_DMA
          mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_TO_HOST;
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
    if(last_pic_flag &&  buf_status[list_pop_index] == 0) {
      sem_destroy(&buf_release_sem);
      break;
    }
    usleep(5000);
  }
  return(NULL);
}


/* Output thread entry point. */
static void* vp6_output_thread(void* arg) {
  struct DecPictures dec_picture;
  u32 pic_display_number = 1;

  while(output_thread_run) {
    enum DecRet ret;
    u32 i, only_once = 0, ext_id = 0xFFFF;
    ret = VCDecNextPicture(dec_inst, &dec_picture);
    if(ret == DEC_PIC_RDY) {
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (!dec_picture.pictures[i].sequence_info.pic_width ||
            !dec_picture.pictures[i].sequence_info.pic_height)
          continue;

        if (only_once == 0) {
          SwMatchOuputBufferId(&tool_params, &dec_picture.pictures[i].luma, &ext_id);
          if (ext_id < MAX_BUFFERS)
            DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                            tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
          only_once = 1;
        }

        if(!use_peek_output) {
          yuvsink->WritePicture(yuvsink->inst, &dec_picture.pictures[i], i);
        }
      }
      printf("[TB] PIC %2u/%2u, type %s,", pic_display_number, (dec_picture.pictures[0].picture_info.pic_id + 1),
              dec_picture.pictures[0].picture_info.is_intra_frame ? "key picture    " : "non key picture");
      /* pic coding type */
      printVp6PicCodingType(dec_picture.pictures[0].picture_info.pic_coding_type);
      /* cycles per mbs */
      if (dec_picture.pictures[0].picture_info.cycles_per_mb) {
        cycle_count += dec_picture.pictures[0].picture_info.cycles_per_mb;
        printf("%4u cycles / mb\n", dec_picture.pictures[0].picture_info.cycles_per_mb);
      }
      printf(" %u x %u, Crop: (%u, %u), %u x %u\n",
             dec_picture.pictures[0].sequence_info.scaled_width,
             dec_picture.pictures[0].sequence_info.scaled_height,
             dec_picture.pictures[0].sequence_info.crop_params.crop_left_offset,
             dec_picture.pictures[0].sequence_info.crop_params.crop_top_offset,
             dec_picture.pictures[0].sequence_info.crop_params.crop_out_width,
             dec_picture.pictures[0].sequence_info.crop_params.crop_out_height);
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
  }

  return(NULL);
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

#define leRushort( P, V) { \
  register ushort v = (uchar) *P++; \
  (V) = v + ( (ushort) (uchar) *P++ << 8); \
}
#define leRushortF( F, V) { \
  char b[2], *p = b; \
  fread( (void *) b, 1, 2, F); \
  leRushort( p, V); \
}

#define leRulong( P, V) { \
  register ulong v = (uchar) *P++; \
  v += (ulong) (uchar) *P++ << 8; \
  v += (ulong) (uchar) *P++ << 16; \
  (V) = v + ( (ulong) (uchar) *P++ << 24); \
}
#define leRulongF( F, V) { \
  char b[4] = {0,0,0,0}, *p = b; \
    V = 0; \
  ret = fread( (void *) b, 1, 4, F); \
  leRulong( p, V); \
}

#if 0
#  define leWchar( P, V)  { * ( (char *) P)++ = (char) (V);}
#else
#  define leWchar( P, V)  { *P++ = (char) (V);}
#endif

#define leWshort( P, V)  { \
  register short v = (V); \
  leWchar( P, v) \
  leWchar( P, v >> 8) \
}
#define leWshortF( F, V) { \
  char b[2], *p = b; \
  leWshort( p, V); \
  fwrite( (void *) b, 1, 2, F); \
}

#define leWlong( P, V)  { \
  register long v = (V); \
  leWchar( P, v) \
  leWchar( P, v >> 8) \
  leWchar( P, v >> 16) \
  leWchar( P, v >> 24) \
}
#define leWlongF( F, V)  { \
  char b[4], *p = b; \
  leWlong( p, V); \
  fwrite( (void *) b, 1, 4, F); \
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

int readCompressedFrame(FILE * fp) {
  ulong frame_size = 0;
  int ret;

  leRulongF(fp, frame_size);

  if( frame_size >= STREAMBUFFER_BLOCKSIZE ) {
    /* too big a frame */
    return 0;
  }

  ret = fread(stream_mem.virtual_address, 1, frame_size, fp);
  if(!ret) {
    DEBUG_PRINT(("[TB] READ COMPRESSED FRAME FAILED!\n"));
  }

  return (int) frame_size;
}

/*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/

int decode_file(const options_s * opts, u32 ds_x, u32 ds_y) {
  u32 tmp, i;
  enum DecRet ret;
  u32 buffer_release_flag = 1;
  struct DecBufferInfo hbuf;
  DWLmemset(&hbuf, 0, sizeof(struct DecBufferInfo));
  struct DecInputParameters dec_input;
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  struct DecOutput dec_output;
  DWLmemset(&dec_output, 0, sizeof(struct DecOutput));
  struct DecPictures dec_picture;
  DWLmemset(&dec_picture, 0, sizeof(struct DecPictures));
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  DWLmemset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_VP6_DEC;
  struct OutFileInfo outfile_info;
  FILE *input_file = NULL;

  if (opts != NULL && opts->input != NULL) {
    input_file = fopen(opts->input, "rb");
    if(input_file == NULL) {
      perror(opts->input);
      return -1;
    }
  } else {
    // perror(opts->input);
    return -1;
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  tmp = OpenAsicTraceFiles();
  if (!tmp) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN TRACE FILE(S)\n"));
  }
#endif
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
    printf("[TB] VC9000 Decoder VP6 API version %u.%u.%u\n", version.major,
          version.minor, version.micro);
    printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
  }
  init_config.codec = DEC_VP6;
  init_config.error_handling = params.error_handling;
  init_config.error_ratio = params.error_ratio;
  init_config.num_frame_buffers = num_buffers;
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


  ret = VCDecInit((const void**)&dec_inst, &init_config);
  if (ret != DEC_OK) {
    printf("[TB] DECODER INITIALIZATION FAILED\n");
    goto end;
  }

  /* Set ref buffer test mode */
  // SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_LATENCY,
  //                latency_comp);
  // SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_CLK_GATE_E,
  //                clock_gating);
  // SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_OUT_ENDIAN,
  //                output_picture_endian);
  // SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_MAX_BURST,
  //                bus_burst_length);
  // SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_DEC_DATA_DISC_E,
  //                data_discard);
  // SetDecRegister(((VP6DecContainer_t *) dec_inst)->vp6_regs, HWIF_SERV_MERGE_DIS,
  //                service_merge_disable);

  /* allocate memory for stream buffer. if unsuccessful -> exit */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;
#ifdef SUPPORT_DMA
  stream_mem.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if(DWLMallocLinear(dwl_inst, STREAMBUFFER_BLOCKSIZE, &stream_mem) != DWL_OK) {
    printf(("UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    exit(-1);
  }
  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_VP6_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;

  while(!feof(input_file) &&
        current_video_frame < (unsigned int) opts->last_frame) {
    fflush(stdout);

    dec_input.strm_len = readCompressedFrame(input_file);
    dec_input.stream = (u8*)stream_mem.virtual_address;
    dec_input.stream_bus_address = (addr_t)stream_mem.bus_address;
    dec_input.stream_buffer = stream_mem;
    dec_input.stream_buffer.size = dec_input.strm_len;
    dec_input.pic_id = current_video_frame;

    if(dec_input.strm_len == 0)
      break;

    if( opts->alpha ) {
      if( dec_input.strm_len < 3 )
        break;
      dec_input.strm_len -= 3;
      dec_input.stream += 3;
      dec_input.stream_bus_address += 3;
    }

    do {
      ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
      /* printf("[TB] VCDecDecode retruned: %d\n", ret); */

      if(ret == DEC_HDRS_RDY) {
        ret = VCDecGetBufferInfo(dec_inst, &hbuf);
        printf("[TB] VCDecGetBufferInfo ret %d\n", ret);
        printf("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
               (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
        struct DecSequenceInfo dec_info;
        DWLmemset(&dec_info, 0, sizeof(struct DecSequenceInfo));

        ret = VCDecGetInfo(dec_inst, &dec_info);
        if (ret != DEC_OK) {
          DEBUG_PRINT("ERROR in getting stream info!\n");
          goto end;
        }

        fprintf(stdout, "\n"
                "[TB] Stream info: vp6_version  = 6.%u\n"
                "                  vp6_profile  = %s\n"
                "                  coded size  = %u x %u\n"
                "                  scaled size = %u x %u\n",
                dec_info.vp_version - 6,
                dec_info.vp_profile == 0 ? "SIMPLE" : "ADVANCED",
                dec_info.pic_width, dec_info.pic_height,
                dec_info.scaled_width, dec_info.scaled_height);

        pp_enabled = params.pp_enabled;
        if (align == DEC_ALIGN_1B) config.align = DEC_ALIGN_64B;
        config.align = align;
        memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));

        tmp = VCDecSetInfo(dec_inst, &config);
        if (tmp != DEC_OK) {
          DEBUG_PRINT(("[TB] Invalid pp parameters\n"));
          goto end;
        }

        /* Create output sink after output file names are determined. */
        if (yuvsink == NULL) {
          outfile_info.bitstream_format = BITSTREAM_VP6;
          outfile_info.pic_width = dec_info.pic_width;
          outfile_info.pic_height = dec_info.pic_height;
          outfile_info.bit_depth = (dec_info.bit_depth_luma == 8 && dec_info.bit_depth_chroma == 8) ? 8 : 10;
          outfile_info.is_interlaced = dec_info.is_interlaced;
          params.compress_bypass = TRUE; /* G1 don't support rfc */
          GenerateOutputFileName(&params, &outfile_info);
          if ((yuvsink = CreateYuvSink(&params)) == NULL) {
            DEBUG_PRINT(("[TB] Failed to create YUV sink\n"));
            goto end;
          }
        }

        ret = DEC_HDRS_RDY;  /* restore */
      }
      if (ret == DEC_WAITING_FOR_BUFFER) {
        DEBUG_PRINT(("[TB] Waiting for frame buffers\n"));
        struct DWLLinearMem mem = {0};

        ret = VCDecGetBufferInfo(dec_inst, &hbuf);
        printf("[TB] VCDecGetBufferInfo ret %d\n", ret);
        printf("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
               (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
        while (ret == DEC_WAITING_FOR_BUFFER) {
          if (hbuf.buf_to_free.virtual_address != NULL) {
            add_extra_flag = 0;
            ReleaseExtBuffers();
            buffer_release_flag = 1;
            external_buf_num = 0;
          }
          ret = VCDecGetBufferInfo(dec_inst, &hbuf);
        }

        buffer_size = hbuf.next_buf_size;
        if(buffer_release_flag && hbuf.next_buf_size) {
          /* Only add minimum required buffers at first. */
#ifdef SUPPORT_DMA
          mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_TO_HOST;
#endif
          for(i = 0; i < hbuf.buf_num; i++) {
            if (pp_enabled) {
              SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
              DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
            } else {
              SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
              DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
            }
            ret = VCDecAddBuffer(dec_inst, &mem);
            printf("[TB] VCDecAddBuffer ret %d\n", ret);
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
          external_buf_num = hbuf.buf_num;
          add_extra_flag = 1;
        }
        ret = DEC_HDRS_RDY;
      }
    } while(ret == DEC_HDRS_RDY || ret == DEC_NO_DECODING_BUFFER);

    if(ret == DEC_PIC_DECODED) {
      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, vp6_output_thread, &params);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      if (use_peek_output && (VCDecPeek(dec_inst, &dec_picture) == DEC_PIC_RDY)) {
        yuvsink->WritePicture(yuvsink->inst, &dec_picture.pictures[0], 0);
      }

      current_video_frame++;
#if 0
      if (current_video_frame == 10) {
        ret = VCDecAbort(dec_inst);
        ret = VCDecAbortAfter(dec_inst);
        rewind(input_file);
      }
#endif
    }

    corrupted_bytes = 0;
  }

end:

  VCDecEndOfStream(dec_inst);

  if (output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }

  add_buffer_thread_run = 0;

  printf("[TB] Pictures decoded: %u\n", current_video_frame);
  if(cycle_count && current_video_frame)
    printf("[TB] Average cycles/MB: %4u\n", cycle_count/current_video_frame);

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
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  VCDecRelease(dec_inst);
  DWLRelease(dwl_inst);
  VCDecLogDestory();

  if (input_file) fclose(input_file);

  PrintOutputFileName(&params);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  if (pic_big_endian != NULL)
    free(pic_big_endian);

  return 0;
}

int main(int argc, char *argv[]) {
  int i, ret;
  FILE *f_tbcfg;
  const char default_out[] = "out.yuv";

  memset(&options, 0, sizeof(options_s));

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
	if (tm == NULL) {
      fprintf(stderr,
              "localtime failed.\n");
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

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++)
    strcpy(options.output[i], default_out);
  options.input = argv[argc - 1];
  options.md5 = 0;
  options.last_frame = -1;
  options.planar = 0;

  /* Use common command line parser to parse options. */
  if ((ret = ParseParams(argc, argv, &params))) {
    if (ret == 1) {
      printf("[TB] Failed to parse params.\n\n");
      PrintUsage(argv[0], VP6DEC);
      ret = 1;
      return 1;
    }
    else {
      ret = 0;
      return 0;
    }
  }
#ifdef MODEL_SIMULATION
  if (params.align && params.align_h) {
    alignwidth = 1 << params.align;
    alignheight = 1 << params.align_h;
  }
  in_lib_name = params.in_lib_name;
#endif

  /* set secure mode */
  secure_mode = (params.decoder_mode & DEC_SECURITY) ? 1 : 0;
  /* get the value form params */
  disable_output_writing = (params.sink_type == SINK_NULL);
  pp_enabled = params.pp_enabled;
  options.planar = params.ppu_cfg[0].planar;
  if (params.num_of_decoded_pics != 0)
    options.last_frame = params.num_of_decoded_pics;
  use_peek_output = params.disable_display_order;
  md5sum = (params.sink_type == SINK_MD5_PICTURE);
  options.md5 = md5sum;
  align = params.align;
  convert_tiled_output = params.convert_tiled_output;
  num_buffers = params.num_buffers;
  use_extra_buffers = params.use_extra_buffers;
  allocate_extra_buffers_in_output = params.allocate_extra_buffers_in_output;
  ds_ratio_x = params.ppu_cfg[0].scale.ratio_x;
  ds_ratio_y = params.ppu_cfg[0].scale.ratio_y;
  options.alpha = params.alpha;

  tool_params.ext_buffers = ext_buffers;
  tool_params.max_buffers = &external_buf_num;
#ifdef ASIC_TRACE_SUPPORT
  asic_trace_enabled = params.hw_traces + 1;
#else
  fprintf(stdout, "\nWarning! Trace generation not supported!\n");
#endif

  /* check if traces shall be enabled */
#ifdef ASIC_TRACE_SUPPORT
  {
    char trace_string[80];
    FILE *fid = fopen("trace.cfg", "r");
    if (fid) {
      /* all tracing enabled if any of the recognized keywords found */
      while(fscanf(fid, "%79s\n", trace_string) != EOF) {
        if (!strcmp(trace_string, "toplevel") ||
            !strcmp(trace_string, "all"))
          asic_trace_enabled = 2;
        else if(!strcmp(trace_string, "fpga") ||
                !strcmp(trace_string, "decoding_tools"))
          if(asic_trace_enabled == 0)
            asic_trace_enabled = 1;
      }
    }
  }
#endif

  TBSetDefaultCfg(&tb_cfg);
  char *tb_path = (params.in_tb_cfg_file_name == NULL) ? "tb.cfg" : params.in_tb_cfg_file_name;
  f_tbcfg = fopen(tb_path, "r");
  if(f_tbcfg == NULL) {
    printf("[TB] UNABLE TO OPEN INPUT FILE: \"tb.cfg of %s\"\n", tb_path);
    printf("[TB] USING DEFAULT CONFIGURATION\n");
  } else {
    fclose(f_tbcfg);
    if(TBParseConfig(tb_path, TBReadParam, &tb_cfg) == TB_FALSE)
      return -1;
    if(TBCheckCfg(&tb_cfg) != 0)
      return -1;
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

#ifdef ASIC_TRACE_SUPPORT

  /*
      {
          extern refBufferd_t    vp6Refbuffer;
          refBufferd_Reset(&vp6Refbuffer);
      }
      */

#endif /* ASIC_TRACE_SUPPORT */

#ifdef VP6_EVALUATION

  /*
      {
          extern refBufferd_t    vp6Refbuffer;
          refBufferd_Reset(&vp6Refbuffer);
      }
      */

#endif /* VP6_EVALUATION */

  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

#ifdef USE_RANDOM_ERROR_TEST
  random_error_params.seed = tb_cfg.tb_params.seed_rnd;
  strcpy(random_error_params.truncate_stream_odds, tb_cfg.tb_params.stream_truncate);
  strcpy(random_error_params.swap_bit_odds, tb_cfg.tb_params.stream_bit_swap);
  strcpy(random_error_params.packet_loss_odds, tb_cfg.tb_params.stream_packet_loss);
#endif

  ret = decode_file(&options, ds_ratio_x, ds_ratio_y);

  return ret;
}

/*------------------------------------------------------------------------------

    Function name:            printVp6PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printVp6PicCodingType(u32 pic_type) {
  switch (pic_type) {
  case DEC_PIC_TYPE_I:
    printf("DEC_PIC_TYPE_I");
    break;
  case DEC_PIC_TYPE_P:
    printf("DEC_PIC_TYPE_P");
    break;
  default:
    printf("Other %u", pic_type);
    break;
  }
}
