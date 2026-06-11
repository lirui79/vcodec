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

#include "vcdecapi.h"
#include <time.h>
#include <unistd.h>
#include <inttypes.h>
#include <ctype.h>
#include <pthread.h>
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

/* Define RCV format metadata max size. Standard specifies 44 bytes, add one
 * to support some non-compliant streams */
#define RCV_METADATA_MAX_SIZE   (44+1)
#define MAX_BUFFERS 34

/* secure mode */
u32 secure_mode;

void VC1DecTrace(const char *string) {
  printf("%s\n", string);
}

/*------------------------------------------------------------------------------
Module defines
------------------------------------------------------------------------------*/

#define VC1_MAX_STREAM_SIZE  DEC_X170_MAX_STREAM_VCD>>1


u32 NextPacket(u8 ** p_strm);
// u32 CropPicture(u8 *p_out_image, u8 *p_in_image,
//                 u32 pic_width, u32 pic_height, u32 out_width, u32 out_height );

void FramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height );
u32 fillBuffer(u8 *stream);

/* Global variables for stream handling */
u32 rcv_v2;
u32 rcv_metadata_size = 0;
u8 *stream_stop = NULL;
FILE *finput;

i32 DecodeRCV(u8 *stream, u32 strm_len, struct DecMetaData *meta_data);
u32 DecodeFrameLayerData(u8 *stream);
static u32 GetNextDuSize(const u8* stream, const u8* stream_start,
                         u32 strm_len, u32 *skipped_bytes);
void printVc1PicCodingType(u32 *pic_type);

/* stream start address */
u8 *byte_strm_start;

u32 enable_frame_picture = 0;
u32 number_of_written_frames = 0;
u32 cycle_count = 0; /* Sum of average cycles/mb counts */
u32 num_frame_buffers = 0;
/* index */
u32 save_index = 0;
u32 use_index = 0;
FILE *f_index = NULL;

off64_t cur_index = 0;
size_t next_index = 0;
off64_t last_stream_pos = 0;

/* SW/SW testing, read stream trace file */
FILE * f_stream_trace = NULL;

u32 disable_output_writing = 0;
u32 display_order = 0;
u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;
u32 slice_ud_in_packet = 0;
u32 use_peek_output = 0;
enum DecSkipFrameMode skip_frame = 0;
u32 slice_mode = 0;
u32 field_output = 0;
u32 md5sum = 0;

u32 dpb_mode = DEC_DPB_FRAME;
u32 dump_dpb_contents = 1;
u32 convert_tiled_output = 0;
u32 convert_to_frame_dpb = 0;

struct TBCfg tb_cfg;
/* for tracing */
#ifdef ASIC_TRACE_SUPPORT
extern u32 g_hw_ver;
#endif

#ifdef VC1_EVALUATION
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

typedef void *VC1DecInst;
VC1DecInst dec_inst;
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
/* For VC-1, always enabled PP for range-map feature may be enabled in bitstream. */
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 pp_enabled = 0;
struct TOOL_PARAMS tool_params;

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
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
        if( rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
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
    //DEBUG_PRINT(("DWLFreeLinear ret %d\n", rv));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

struct DecSequenceInfo info;
struct DecMetaData meta_data;
u32 crop_display = 0;
u8 *tmp_image = NULL;
u32 coded_pic_width = 0;
u32 coded_pic_height = 0;
u32 end_of_stream = 0;
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
            if( rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
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
    usleep(50000);
  }
  return (NULL);
}

/* Output thread entry point. */
static void* vc1_output_thread(void* arg) {
  struct DecPictures dec_picture;
  u32 pic_display_number = 1;
  while(output_thread_run) {
    enum DecRet ret;
    u32 i, only_once = 0, ext_id = 0xFFFF;
    u32* host_base = NULL;
    struct DecPicture* in = &dec_picture.pictures[0];

    ret = VCDecNextPicture(dec_inst, &dec_picture);
    if (ret == DEC_PIC_RDY && info.is_interlaced &&
        in->picture_info.first_field &&
        !in->sequence_info.is_interlaced) {
      VCDecPictureConsumed(dec_inst, &dec_picture);
    }
    if (ret == DEC_PIC_RDY &&
       ((!in->picture_info.first_field && info.is_interlaced)
         || !info.is_interlaced)) {

      /* Increment display number for every displayed picture */
      pic_display_number++;
      number_of_written_frames++;

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
          DEBUG_PRINT(("[TB] VCDecNextPicture returned: %s\n",VCDecRetStr(ret)));
          DEBUG_PRINT(("[TB] PIC %2u/%2u, type %s,", (pic_display_number - 1), (in->picture_info.pic_id + 1),
                        in->picture_info.is_intra_frame ? "key picture    " : "non key picture"));
          /* pic coding type */
          u32 pic_coding_type[2];
          pic_coding_type[0] = in->picture_info.pic_coding_type;
          pic_coding_type[1] = in->picture_info.pic_coding_type_field;
          printVc1PicCodingType(pic_coding_type);
          if (in->picture_info.cycles_per_mb) {
            cycle_count += in->picture_info.cycles_per_mb;
            DEBUG_PRINT(("[TB] %4u cycles / mb,", in->picture_info.cycles_per_mb));
          }
          DEBUG_PRINT((" %u x %u, Crop: (%u, %u), %u x %u\n",
                        in->sequence_info.scaled_width,
                        in->sequence_info.scaled_height,
                        in->sequence_info.crop_params.crop_left_offset,
                        in->sequence_info.crop_params.crop_top_offset,
                        in->sequence_info.crop_params.crop_out_width,
                        in->sequence_info.crop_params.crop_out_height));
          if (in->sequence_info.is_interlaced &&
              in->picture_info.field_picture)
            DEBUG_PRINT(("[TB] Interlaced field %s, ", in->picture_info.top_field ? "(Top)" : "(Bottom)"));
          else if (in->sequence_info.is_interlaced && !in->picture_info.field_picture)
            DEBUG_PRINT(("[TB] Interlaced frame, "));
          else
            DEBUG_PRINT(("[TB] Progressive, "));
          DEBUG_PRINT((" ERR MBs %u,", in->picture_info.nbr_of_err_mbs));
          DEBUG_PRINT(("\n\n"));

          if (coded_pic_width == 0) {
            coded_pic_width = in->sequence_info.scaled_width;
            coded_pic_height = in->sequence_info.scaled_height;
          }
#if 0
          /* Write output picture to file */
          if (crop_display &&
              ( in->sequence_info.pic_width > in->sequence_info.scaled_width ||
                in->sequence_info.pic_height > in->sequence_info.scaled_height ) ) {
            struct DecPicture out_picture = dec_picture.pictures[0];
            tmp = CropPicture(tmp_image, (u8*)in->luma.virtual_address,
                              in->sequence_info.pic_width, in->sequence_info.pic_height,
                              in->sequence_info.scaled_width, in->sequence_info.scaled_height );
            if (tmp) return NULL;
            out_picture.luma.virtual_address = (u32*)tmp_image;
            yuvsink->WritePicture(yuvsink->inst, &out_picture, 0);
          } else
#endif
          {
            if( (enable_frame_picture
                 && ( in->sequence_info.scaled_width !=
                      meta_data.max_coded_width ||
                      in->sequence_info.scaled_height !=
                      meta_data.max_coded_height ) ) ) {
              if (i != 0) {
                SwClearHostOutbaseAfterWriteFile(host_base, in);
                in = &dec_picture.pictures[0];
                SwSetHostOutbaseAfterDma(host_base, in, &tool_params.ext_buffers[ext_id]);
              }
              yuvsink->WritePicture(yuvsink->inst, in, 0);
            } else {
              yuvsink->WritePicture(yuvsink->inst, in, i);
            }
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
    }

    else if(ret == DEC_END_OF_STREAM) {
      last_pic_flag = 1;
      break;
    }
  }
  return (NULL);
}

/*------------------------------------------------------------------------------

 Function name:  main

  Purpose:
  main function of decoder testbench. Provides command line interface
  with file I/O for H.264 decoder. Prints out the usage information
  when executed without arguments.

------------------------------------------------------------------------------*/

int main(int argc, char **argv) {

  u32 i, tmp;
  u32 max_num_pics = 0;
  u32 strm_len = 0;
  u32 max_pic_size = 0;
  enum DecRet ret;
  struct DecInputParameters dec_input;
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  struct DecOutput dec_output;
  struct DecPictures dec_picture;
  struct DWLLinearMem stream_mem;
  u8* tmp_strm = 0;
  u32 pic_id = 0;
  u32 new_headers = 0;

  u32 pic_decode_number = 0;
  u32 pic_display_number = 1;
  u32 num_errors = 0;
  int ra;

  FILE *f_tbcfg;
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));

  u32 advanced = 0;
  u32 skipped_bytes = 0;
  off64_t stream_pos = 0; /* For advanced profile long streams */
  u32 min_buffer_num = 0;
  struct DecBufferInfo hbuf;
  enum DecRet rv;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);
  struct DWLInitParam dwl_init;
  struct OutFileInfo outfile_info;
  u32 long_stream = 0;
  i32 corrupted_bytes = 0;
  u32 filed_num = 0;

  struct TestParams params;

  SetupDefaultParams(&params);

  /* Use common command line parser to parse options. */
  if ((ret = ParseParams(argc, argv, &params))) {
    if (ret == 1) {
      printf("[TB] Failed to parse params.\n\n");
      PrintUsage(argv[0], VC1DEC);
      ret = 1;
      return 1;
    }
    else {
      ret = 0;
      return 0;
    }
  }

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

  /* set secure mode */
  secure_mode = (params.decoder_mode & DEC_SECURITY) ? 1 : 0;
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
      return -1;
    if (TBCheckCfg(&tb_cfg) != 0)
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

  INIT_SW_PERFORMANCE;

  /* Check that enough command line arguments given, if not -> print usage
  * information out */
  /*
  if(argc < 2) {
    DEBUG_PRINT(("Usage: %s [options] file.rcv\n", argv[0]));
    DEBUG_PRINT(("\t-Nn forces decoding to stop after n pictures\n"));
    DEBUG_PRINT(("\t-Ooutfile write output to \"outfile\" (default out_wxxxhyyy.yuv)\n"));
    DEBUG_PRINT(("\t-X Disable output file writing\n"));
    DEBUG_PRINT(("\t-C display cropped image (default decoded image)\n"));
    DEBUG_PRINT(("\t-Sfile.hex stream control trace file\n"));
    DEBUG_PRINT(("\t-L enable support for long streams.\n"));
    DEBUG_PRINT(("\t-P write planar output.(--planar)\n"));
    DEBUG_PRINT(("\t-a Enable PP planar output.(--pp-planar)\n"));
    DEBUG_PRINT(("\t-Bn to use n frame buffers in decoder\n"));
    DEBUG_PRINT(("\t-I save index file\n"));
    DEBUG_PRINT(("\t-E use tiled reference frame format.\n"));
    DEBUG_PRINT(("\t-G convert tiled output pictures to raster scan\n"));
    DEBUG_PRINT(("\t-Y Write output as Interlaced Fields (instead of Frames).\n"));
    DEBUG_PRINT(("\t-F Enable frame picture writing in multiresolutin output.\n"));
    DEBUG_PRINT(("\t-Q Skip decoding non-reference pictures.\n"));
    DEBUG_PRINT(("\t-Z output pictures using VCDecPeek() function\n"));
    DEBUG_PRINT(("\t-m Output md5 for each picture(--md5-per-pic)\n"));
    DEBUG_PRINT(("\t--separate-fields-in-dpb DPB stores interlaced content"\
                 " as fields (default: frames)\n"));
    DEBUG_PRINT(("\t--output-frame-dpb Convert output to frame mode even if"\
                 " field DPB mode used\n"));
    DEBUG_PRINT(("\t-e add extra external buffer randomly\n"));
    DEBUG_PRINT(("\t-k allocate extra external buffer in output thread\n"));
    DEBUG_PRINT(("\t-An Set stride aligned to n bytes (valid value: 8/16/32/64/128/256/512)\n"));
    DEBUG_PRINT(("\t-d[x[:y]] Fixed down scale ratio (1/2/4/8). E.g.,\n"));
    DEBUG_PRINT(("\t  -d2 -- down scale to 1/2 in both directions\n"));
    DEBUG_PRINT(("\t  -d2:4 -- down scale to 1/2 in horizontal and 1/4 in vertical\n"));
    DEBUG_PRINT(("\t--cr-first PP outputs chroma in CrCb order.\n"));
    DEBUG_PRINT(("\t-C[xywh]NNN Cropping parameters. E.g.,\n"));
    DEBUG_PRINT(("\t  -Cx8 -Cy16        Crop from (8, 16)\n"));
    DEBUG_PRINT(("\t  -Cw720 -Ch480     Crop size  720x480\n"));
    DEBUG_PRINT(("\t-Dwxh  PP output size wxh. E.g.,\n"));
    DEBUG_PRINT(("\t  -D1280x720        PP output size 1280x720\n"));
    DEBUG_PRINT(("\t--pp-rgb Enable Yuv2RGB.\n"));
    DEBUG_PRINT(("\t--pp-rgb-planar Enable planar Yuv2RGB.\n"));
    DEBUG_PRINT(("\t--rgb-fmat set the RGB output format.\n"));
    DEBUG_PRINT(("\t--rgb-std set the standard coeff to do Yuv2RGB.\n\n"));
    return 0;
  }
  */

  /* get the value form params */
  disable_output_writing = (params.sink_type == SINK_NULL);
  pp_enabled = params.pp_enabled;
  max_num_pics = params.num_of_decoded_pics;
  use_peek_output = params.disable_display_order;
  md5sum = (params.sink_type == SINK_MD5_PICTURE);
  align = params.align;
  convert_tiled_output = params.convert_tiled_output;
  num_frame_buffers = params.num_buffers;
  skip_frame = params.skip_frame;
  dpb_mode = params.dpb_mode;
  convert_to_frame_dpb = params.convert_to_frame_dpb;
  use_extra_buffers = params.use_extra_buffers;
  allocate_extra_buffers_in_output = params.allocate_extra_buffers_in_output;
  if (params.stream_trace != NULL)
    f_stream_trace = fopen(params.stream_trace, "r");
  save_index = params.save_index;
  long_stream = params.long_stream;
  enable_frame_picture = params.enable_frame_picture;
  // tool_params.use_mp_output = params.use_mp_output;
  // tool_params.use_separated_pp = params.use_separated_pp;
  tool_params.ext_buffers = ext_buffers;
  tool_params.max_buffers = &num_buffers;

  /* set stream pointers to null */
  stream_mem.virtual_address = NULL;
  stream_mem.bus_address = 0;

  /* open input file for reading, file name given by user. If file open
  * fails -> exit */
  finput = fopen(argv[argc - 1], "rb");
  if(finput == NULL) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT FILE: %s\n", argv[argc - 1]));
    return -1;
  }

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
  if(save_index) {
    f_index = fopen("stream.cfg", "w");
    if(f_index == NULL) {
      DEBUG_PRINT(("[TB] UNABLE TO OPEN INDEX FILE: \"stream.cfg\"\n"));
      return -1;
    }
  } else {
    f_index = fopen("stream.cfg", "r");
    if(f_index != NULL) {
      use_index = 1;
    }
  }

  clock_gating = TBGetDecClockGating(&tb_cfg);
  data_discard = TBGetDecDataDiscard(&tb_cfg);
  latency_comp = tb_cfg.dec_params.latency_compensation;
  output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);
  /*#if MD5SUM
      output_picture_endian = DEC_X170_LITTLE_ENDIAN;
      printf("Decoder Output Picture Endian forced to %d\n", output_picture_endian);
  #endif*/

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

  tmp_strm = (u8*) malloc(100);
  if (NULL == tmp_strm) {
    DEBUG_PRINT(("[TB] MALLOC FAILED\n"));
    return -1;
  }

  /* read metadata (max size (5+4)*4 bytes,
   * DecodeRCV checks that size is at
   * least RCV_METADATA_MAX_SIZE bytes) */
  ra = fread(tmp_strm, sizeof(u8), RCV_METADATA_MAX_SIZE, finput);
  (void) ra;
  rewind(finput);

  /* Advanced profile if startcode prefix found */
  if ( (tmp_strm[0] == 0x00) && (tmp_strm[1] == 0x00) && (tmp_strm[2] == 0x01) )
    advanced = 1;

  /* reset metadata structure */
  DWLmemset(&meta_data, 0, sizeof(meta_data));

  if (!advanced) {
    /* decode row coded video header (coded metadata). DecodeRCV function reads
    * image dimensions from struct A, struct C information is parsed by
    * VCDecUnpackMetaData function */
    tmp = DecodeRCV(tmp_strm, RCV_METADATA_MAX_SIZE, &meta_data);
    if (tmp != 0) {
      DEBUG_PRINT(("[TB] DECODING RCV FAILED\n"));
      free(tmp_strm);
      return -1;
    }
  } else {
    meta_data.profile = VC1_ADVANCED_PROFILE;
  }

  if (!advanced) {
    START_SW_PERFORMANCE
    tmp = VCDecUnpackMetaData(DEC_VC1, tmp_strm + 8, 4, &meta_data);
    END_SW_PERFORMANCE
    if (tmp != DEC_OK) {
      DEBUG_PRINT(("[TB] UNPACKING META DATA FAILED\n"));
      free(tmp_strm);
      return -1;
    }
    DEBUG_PRINT(("[TB] meta_data.vs_transform %u\n", meta_data.vs_transform));
    DEBUG_PRINT(("[TB] meta_data.overlap %u\n", meta_data.overlap));
    DEBUG_PRINT(("[TB] meta_data.sync_marker %u\n", meta_data.sync_marker));
    DEBUG_PRINT(("[TB] meta_data.quantizer %u\n", meta_data.quantizer));
    DEBUG_PRINT(("[TB] meta_data.frame_interp %u\n", meta_data.frame_interp));
    DEBUG_PRINT(("[TB] meta_data.max_bframes %u\n", meta_data.max_bframes));
    DEBUG_PRINT(("[TB] meta_data.fast_uv_mc %u\n", meta_data.fast_uv_mc));
    DEBUG_PRINT(("[TB] meta_data.extended_mv %u\n", meta_data.extended_mv));
    DEBUG_PRINT(("[TB] meta_data.multi_res %u\n", meta_data.multi_res));
    DEBUG_PRINT(("[TB] meta_data.range_red %u\n", meta_data.range_red));
    DEBUG_PRINT(("[TB] meta_data.dquant %u\n", meta_data.dquant));
    DEBUG_PRINT(("[TB] meta_data.loop_filter %u\n", meta_data.loop_filter));
    DEBUG_PRINT(("[TB] meta_data.profile %d\n", meta_data.profile));

  }

  dwl_init.client_type = DWL_CLIENT_TYPE_VC1_DEC;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif

  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("ERROR: DWL Init failed"));
    goto end;
  }

  {
    struct DecSwHwBuild build;
    struct DecApiVersion version;

    build = VCDecGetBuild(dwl_inst, dwl_init.client_type);
    version = VCDecGetAPIVersion();
    printf("[TB] VC9000 Decoder VC1 API version %u.%u.%u\n", version.major,
          version.minor, version.micro);
    printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
  }

  init_config.codec = DEC_VC1;
  init_config.meta_data = meta_data;
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

  /* initialize decoder. If unsuccessful -> exit */
  START_SW_PERFORMANCE;
  ret = VCDecInit((const void**)&dec_inst, &init_config);
  END_SW_PERFORMANCE;

  if (ret != DEC_OK) {
    DEBUG_PRINT(("[TB] DECODER INITIALIZATION FAILED\n"));
    goto end;
  }

  VCDecGetInfo(dec_inst, &info);

  dec_input.skip_frame = skip_frame;

  if (!advanced) {
    pp_enabled = params.pp_enabled;
    config.align = align;
    memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
    tmp = VCDecSetInfo(dec_inst, &config);
    if (tmp != DEC_OK) {
      DEBUG_PRINT(("[TB] VCDecSetInfo returned: %s\n",VCDecRetStr(tmp)));
      goto end;
    }
  }

  if (!long_stream) {
    /* check size of the input file -> length of the stream in bytes */
    fseek(finput, 0L, SEEK_END);
    strm_len = (u32) ftell(finput);
    rewind(finput);

    /* allocate memory for stream buffer. if unsuccessful -> exit */
    stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
    if(DWLMallocLinear(dwl_inst, strm_len, &stream_mem)
        != DWL_OK ) {
      DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
      goto end;
    }
    dec_input.stream_buffer = stream_mem;

    byte_strm_start = (u8 *) stream_mem.virtual_address;

    if(byte_strm_start == NULL) {
      DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
      goto end;
    }

    /* read input stream from file to buffer and close input file */
    ra = fread(byte_strm_start, sizeof(u8), strm_len, finput);
    (void) ra;
    fclose(finput);

    /* initialize VCDecDecode() input structure */
    stream_stop = byte_strm_start + strm_len;

    if (!advanced) {
      /* size of Sequence layer data structure */
      dec_input.stream = byte_strm_start + ( 4 + 4 * rcv_v2 ) *4 + rcv_metadata_size;
      dec_input.strm_len = DecodeFrameLayerData((u8*)dec_input.stream);
      dec_input.stream += 4 + 4 * rcv_v2; /* size of Frame layer data structure */
      dec_input.stream_bus_address = stream_mem.bus_address +
                                     (dec_input.stream - byte_strm_start);
    } else {
      dec_input.stream = byte_strm_start;
      dec_input.strm_len = strm_len;
      dec_input.stream_bus_address = stream_mem.bus_address;
    }
  }
  /* LONG STREAM */
  else {
    stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
    if(DWLMallocLinear(dwl_inst,
                       VC1_MAX_STREAM_SIZE,
                       &stream_mem) != DWL_OK ) {
      DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
      goto end;
    }
    dec_input.stream_buffer = stream_mem;
    byte_strm_start = (u8 *) stream_mem.virtual_address;
    dec_input.stream =  (u8 *) stream_mem.virtual_address;
    dec_input.stream_bus_address = stream_mem.bus_address;

    if (!advanced) {
      /* Read meta data and frame layer data */
      ra = fread(tmp_strm, sizeof(u8), ( 4 + 4 * rcv_v2 ) *4 + 4 + 4 * rcv_v2 + rcv_metadata_size, finput);
      (void) ra;
      if (ferror(finput)) {
        DEBUG_PRINT(("[TB] STREAM READ ERROR\n"));
        goto end;
      }
      if (feof(finput)) {
        DEBUG_PRINT(("[TB] END OF STREAM\n"));
        goto end;
      }

      dec_input.strm_len = DecodeFrameLayerData(tmp_strm + ( 4 + 4 * rcv_v2 ) *4 + rcv_metadata_size);
      ra = fread( (u8*)dec_input.stream, sizeof(u8), dec_input.strm_len, finput );
      (void) ra;
    } else {
      if(use_index) {
        dec_input.strm_len = fillBuffer((u8*)dec_input.stream);
        strm_len = dec_input.strm_len;
      } else {
        dec_input.strm_len =
          fread((u8*)dec_input.stream, sizeof(u8), VC1_MAX_STREAM_SIZE, finput);
        strm_len = dec_input.strm_len;
      }
    }

    if (ferror(finput)) {
      DEBUG_PRINT(("[TB] STREAM READ ERROR\n"));
      goto end;
    }
    if (feof(finput)) {
      DEBUG_PRINT(("[TB] STREAM WILL END\n"));
      /*goto end;*/
    }
  }

  if (!advanced) {
    /* max picture size */
    max_pic_size = (
                     ( ( meta_data.max_coded_width + 15 ) & ~15 ) *
                     ( ( meta_data.max_coded_height + 15 ) & ~15 ) ) * 3 / 2;
  }

  if( (crop_display || meta_data.multi_res) && !advanced ) {
    tmp_image = (u8*)malloc(max_pic_size * sizeof(u8) );
  }

  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_VC1_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;

  /* main decoding loop */
  do {
    dec_input.pic_id = pic_id;
    if (ret != DEC_NO_DECODING_BUFFER)
    DEBUG_PRINT(("[TB] Starting to decode picture ID %u\n", pic_id + 1));

    /* call API function to perform decoding */
    /* if stream size bigger than decoder can handle, skip the frame */
    /* checking error response for oversized pic done in API testing */
    if(dec_input.strm_len <= VC1_MAX_STREAM_SIZE) {

      START_SW_PERFORMANCE;
      ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
      END_SW_PERFORMANCE;
      /*DEBUG_PRINT(("dec_output.data_left %d\n", dec_output.data_left));*/
      DEBUG_PRINT(("[TB] VCDecDecode returned: %s\n",VCDecRetStr(ret)));

#if 1
      /* there is some data left */
      /* if simple or main, test bench does not care of dec_output.data_left but rather skips to next picture */
      /* if advanced but long stream mode, stream is read again from file */
      /* if slice mode, test bench does not care of dec_output.data_left but also skips to next slice */
      if(dec_output.data_left && advanced && !long_stream && !slice_mode) {
        printf("[TB] dec_output.data_left %u\n", dec_output.data_left);
        corrupted_bytes -= (dec_input.strm_len - dec_output.data_left);
      } else {
        corrupted_bytes = 0;
      }
#endif
    } else {
      ret = DEC_STRM_PROCESSED;
      DEBUG_PRINT(("[TB] Oversized stream for picture, ignoring... \n"));
      break;
    }
    switch(ret) {
    case DEC_RESOLUTION_CHANGED:
      /* get and set info */
      VCDecGetInfo(dec_inst, &info);

      pp_enabled = params.pp_enabled;
      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK) {
        DEBUG_PRINT(("[TB] VCDecSetInfo returned: %s\n",VCDecRetStr(tmp)));
        goto end;
      }
      break;

    case DEC_HDRS_RDY:
      /* Set a flag to indicate that headers are ready */
      rv = VCDecGetBufferInfo(dec_inst, &hbuf);
      printf("[TB] VCDecGetBufferInfo ret %d\n", rv);
      printf("[TB] buf_to_free %p, next_buf_size %llu, buf_num %u\n",
             (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num);
      new_headers = 1;

      if( !long_stream ) {
        tmp = (dec_output.strm_curr_pos - dec_input.stream);
        stream_pos += tmp;
        dec_input.stream = dec_output.strm_curr_pos;
        dec_input.stream_bus_address += tmp;
        if (slice_mode) {
          dec_input.strm_len =
            GetNextDuSize(dec_output.strm_curr_pos,
                          byte_strm_start, strm_len, &skipped_bytes);
          dec_input.stream += skipped_bytes;
          dec_input.stream_bus_address += skipped_bytes;
          stream_pos += skipped_bytes;
        } else
          dec_input.strm_len = dec_output.data_left;
      } else { /* LONG STREAM */
        if(use_index) {
          if(dec_output.data_left != 0) {
            dec_input.stream_bus_address += (dec_output.strm_curr_pos - dec_input.stream);
            dec_input.strm_len = dec_output.data_left;
            dec_input.stream = dec_output.strm_curr_pos;
          } else {
            dec_input.stream_bus_address = stream_mem.bus_address;
            dec_input.stream =  (u8 *) stream_mem.virtual_address;
            dec_input.strm_len = fillBuffer((u8*)dec_input.stream);
          }
        } else {
          tmp = (dec_output.strm_curr_pos - dec_input.stream);
          stream_pos += tmp;
          fseeko64( finput, stream_pos, SEEK_SET );
          dec_input.strm_len =
            fread((u8*)dec_input.stream, sizeof(u8), VC1_MAX_STREAM_SIZE, finput);

          if (slice_mode) {
            dec_input.strm_len =
              GetNextDuSize((u8*)dec_input.stream,
                            dec_input.stream, dec_input.strm_len, &skipped_bytes);
            stream_pos += skipped_bytes;
            fseeko64( finput, stream_pos, SEEK_SET );
            if(save_index) {
              if(dec_input.stream[0] == 0 &&
                  dec_input.stream[1] == 0 &&
                  dec_input.stream[2] == 1) {
                fprintf(f_index, "%" PRId64"\n", stream_pos);
              }
            }
            dec_input.strm_len =
              fread((u8*)dec_input.stream, sizeof(u8),
                    dec_input.strm_len, finput);
          }
        }
        if (ferror(finput)) {
          DEBUG_PRINT(("STREAM READ ERROR\n"));
          goto end;
        }
        if (feof(finput)) {
          DEBUG_PRINT(("STREAM WILL END\n"));
          /*goto end;*/
        }
      }

      /* get info */
      VCDecGetInfo(dec_inst, &info);

      pp_enabled = params.pp_enabled;
      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK) {
        DEBUG_PRINT(("[TB] VCDecSetInfo returned: %s\n",VCDecRetStr(tmp)));
        goto end;
      }
      dpb_mode = info.dpb_mode;

      if(new_headers) {
        if (info.is_interlaced) {
          DEBUG_PRINT(("[TB] Interlaced sequence\n"));
          field_output = 1;
        } else
          DEBUG_PRINT(("[TB] Progressive sequence\n"));

        DEBUG_PRINT(("[TB] Max size %ux%u\n", info.pic_width, info.pic_height));
        DEBUG_PRINT(("[TB] Coded size %ux%u\n", info.scaled_width, info.scaled_height));
        DEBUG_PRINT(("[TB] Output format %s\n",
                     info.output_format == DEC_OUT_FRM_YUV420SP
                     ? "DEC_OUT_FRM_YUV420SP" :
                     "DEC_OUT_FRM_YUV420TILE"));
      }
      /* max picture size */
      max_pic_size = (info.pic_width * info.pic_height * 3)>>1;

      if( (crop_display || enable_frame_picture) && (tmp_image == NULL) )
        tmp_image = (u8*)malloc(max_pic_size * sizeof(u8) );
      break;

    case DEC_WAITING_FOR_BUFFER:
      rv = VCDecGetBufferInfo(dec_inst, &hbuf);
      DEBUG_PRINT(("VCDecGetBufferInfo ret %d\n", rv));
      DEBUG_PRINT(("buf_to_free %p, next_buf_size %llu, buf_num %u\n",
                   (void *)hbuf.buf_to_free.virtual_address, hbuf.next_buf_size, hbuf.buf_num));
      min_buffer_num = hbuf.buf_num;
      if (hbuf.buf_to_free.virtual_address != NULL) {
        add_extra_flag = 0;
        ReleaseExtBuffers();
        num_buffers = 0;
      }

      if(hbuf.next_buf_size) {
        /* Only add minimum required buffers at first. */
        //extra_buffer_num = hbuf.buf_num - min_buffer_num;
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem = {0};
        mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
        mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
        for(i=0; i<min_buffer_num; i++) {
          if (pp_enabled) {
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP, secure_mode);
            DWLMallocLinear(dwl_inst, hbuf.next_buf_size, &mem);
          } else {
            SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE, secure_mode);
            DWLMallocRefFrm(dwl_inst, hbuf.next_buf_size, &mem);
          }
          rv = VCDecAddBuffer(dec_inst, &mem);
          DEBUG_PRINT(("[TB] VCDecAddBuffer ret %d\n", rv));
          if( rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            ext_buffers[i] = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = min_buffer_num;
        add_extra_flag = 1;
      }
      break;

    case DEC_PIC_DECODED:
      /* Picture is now ready */
      new_headers = 0;
      /* get info */
      VCDecGetInfo(dec_inst, &info);
      dpb_mode = info.dpb_mode;
      if (!output_thread_run) {
        /* Create output sink after output file names are determined. */
        if (yuvsink == NULL) {
          outfile_info.bitstream_format = BITSTREAM_VC1;
          outfile_info.pic_width = info.pic_width;
          outfile_info.pic_height = info.pic_height;
          outfile_info.bit_depth = (info.bit_depth_luma == 8 && info.bit_depth_chroma == 8) ? 8 : 10;
          outfile_info.is_interlaced = info.is_interlaced;
          params.compress_bypass = TRUE; /* G1 don't support rfc */
          GenerateOutputFileName(&params, &outfile_info);
          if ((yuvsink = CreateYuvSink(&params)) == NULL) {
            fprintf(stderr, "[TB] Failed to create YUV sink\n");
            return -1;
          }
        }
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, vc1_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      if(info.is_interlaced)
        filed_num++;
      /* Increment decoding number for every decoded picture */
      if(!info.is_interlaced || (info.is_interlaced && filed_num == 2)) {
        pic_decode_number++;
        filed_num = 0;
      }

      if (!long_stream && !advanced) {
        dec_input.stream += dec_input.strm_len;
        dec_input.stream_bus_address += dec_input.strm_len;
      }
#if 0
      if (pic_decode_number == 10) {
        enum DecRet tmp_ret = VCDecAbort(dec_inst);
        tmp_ret = VCDecAbortAfter(dec_inst);
      }
#endif

      if (use_peek_output &&
          VCDecPeek(dec_inst, &dec_picture) == DEC_PIC_RDY) {
        /* Increment display number for every displayed picture */
        pic_display_number++;
        number_of_written_frames++;
        struct DecPicture* in = &dec_picture.pictures[0];
        u32 only_once = 0, ext_id = 0xFFFF;
        u32* host_base = NULL;

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
          DEBUG_PRINT(("[TB] VCDecPeek returned: %s\n", VCDecRetStr(DEC_PIC_RDY)));
          DEBUG_PRINT(("[TB] PIC %2u/%2u, type %s,", (pic_display_number - 1), (in->picture_info.pic_id + 1),
                        in->picture_info.is_intra_frame ? "key picture    " : "non key picture"));
          /* pic coding type */
          u32 pic_coding_type[2];
          pic_coding_type[0] = in->picture_info.pic_coding_type;
          pic_coding_type[1] = in->picture_info.pic_coding_type_field;
          printVc1PicCodingType(pic_coding_type);
          if (in->picture_info.cycles_per_mb) {
            cycle_count += in->picture_info.cycles_per_mb;
            DEBUG_PRINT(("[TB] %4u cycles / mb,", in->picture_info.cycles_per_mb));
          }
          DEBUG_PRINT(("[TB] %u x %u, Crop: (%u, %u), %u x %u\n",
                        in->sequence_info.scaled_width,
                        in->sequence_info.scaled_height,
                        in->sequence_info.crop_params.crop_left_offset,
                        in->sequence_info.crop_params.crop_top_offset,
                        in->sequence_info.crop_params.crop_out_width,
                        in->sequence_info.crop_params.crop_out_height));
          if (in->sequence_info.is_interlaced &&
              in->picture_info.field_picture)
            DEBUG_PRINT(("[TB] Interlaced field %s, ", in->picture_info.top_field ? "(Top)" : "(Bottom)"));
          else if (in->sequence_info.is_interlaced && !in->picture_info.field_picture)
            DEBUG_PRINT(("[TB] Interlaced frame, "));
          else
            DEBUG_PRINT(("[TB] Progressive, "));
          DEBUG_PRINT(("[TB] ERR MBs %u,", in->picture_info.nbr_of_err_mbs));
          DEBUG_PRINT(("\n\n"));

          if (coded_pic_width == 0) {
            coded_pic_width = in->sequence_info.scaled_width;
            coded_pic_height = in->sequence_info.scaled_height;
          }
#if 0
          /* Write output picture to file */
          if (crop_display &&
              ( in->sequence_info.pic_width > in->sequence_info.scaled_width ||
                in->sequence_info.pic_height > in->sequence_info.scaled_height ) ) {
            struct DecPicture out_picture = dec_picture.pictures[0];
            tmp = CropPicture(tmp_image, (u8*)in->luma.virtual_address,
                              in->sequence_info.pic_width, in->sequence_info.pic_height,
                              in->sequence_info.scaled_width, in->sequence_info.scaled_height );
            if (tmp) continue;
            out_picture.luma.virtual_address = (u32*)tmp_image;
            yuvsink->WritePicture(yuvsink->inst, &out_picture, 0);
          } else
#endif
          {
            if( (enable_frame_picture
                 && ( in->sequence_info.scaled_width !=
                      meta_data.max_coded_width ||
                      in->sequence_info.scaled_height !=
                      meta_data.max_coded_height ) ) ) {
              if (i != 0) {
                SwClearHostOutbaseAfterWriteFile(host_base, in);
                in = &dec_picture.pictures[0];
                SwSetHostOutbaseAfterDma(host_base, in, &tool_params.ext_buffers[ext_id]);
              }
              yuvsink->WritePicture(yuvsink->inst, in, 0);
            } else {
              yuvsink->WritePicture(yuvsink->inst, in, i);
            }
          }
        }
        SwClearHostOutbaseAfterWriteFile(host_base, in);
        if (host_base != NULL) {
          DWLfree(host_base);
          tool_params.ext_buffers[ext_id].virtual_address = NULL;
        }
      }

      /* Update pic Id */
      pic_id++;

      /* If enough pictures decoded -> force decoding to end
      * by setting that no more stream is available */
      if ((max_num_pics && pic_decode_number == max_num_pics) ||
          (dec_input.stream >= stream_stop && !long_stream))
        dec_input.strm_len = 0;
      else {
        /* Decode frame layer metadata */
        if( !long_stream ) {
          if( !advanced ) {
            dec_input.strm_len =
              DecodeFrameLayerData((u8*)dec_input.stream);
            dec_input.stream += 4 + 4 * rcv_v2;
            if ((dec_input.stream + dec_input.strm_len) > stream_stop) {
              dec_input.strm_len = stream_stop > dec_input.stream ?
                                      stream_stop-dec_input.stream : 0;
            }
            dec_input.stream_bus_address += 4 + 4 * rcv_v2;
          } else {
            tmp = (dec_output.strm_curr_pos - dec_input.stream);
            stream_pos += tmp;
            dec_input.stream = dec_output.strm_curr_pos;
            dec_input.stream_bus_address += tmp;
            if (slice_mode) {
              dec_input.strm_len =
                GetNextDuSize(dec_output.strm_curr_pos,
                              byte_strm_start, strm_len, &skipped_bytes);
              dec_input.stream += skipped_bytes;
              dec_input.stream_bus_address += skipped_bytes;
              stream_pos += skipped_bytes;
            } else
              dec_input.strm_len = dec_output.data_left;
          }
        } else { /* LONG STREAM */
          if (!advanced) {
            ra = fread(tmp_strm, sizeof(u8),  4 + 4 * rcv_v2, finput);
            (void) ra;
            if (ferror(finput)) {
              DEBUG_PRINT(("STREAM READ ERROR\n"));
              goto end;
            }
            if (feof(finput)) {
              DEBUG_PRINT(("END OF STREAM\n"));
              dec_input.strm_len = 0;
              continue;
            }
            dec_input.strm_len = DecodeFrameLayerData(tmp_strm);
            ra = fread((u8*)dec_input.stream, sizeof(u8), dec_input.strm_len, finput);
            (void) ra;
          } else {
            if(use_index) {
              if(dec_output.data_left != 0) {
                dec_input.stream_bus_address += (dec_output.strm_curr_pos - dec_input.stream);
                dec_input.strm_len = dec_output.data_left;
                dec_input.stream = dec_output.strm_curr_pos;
              } else {
                dec_input.stream_bus_address = stream_mem.bus_address;
                dec_input.stream =  (u8 *) stream_mem.virtual_address;
                dec_input.strm_len = fillBuffer((u8*)dec_input.stream);
              }
            } else {
              tmp = (dec_output.strm_curr_pos - dec_input.stream);
              stream_pos += tmp;
              fseeko64( finput, stream_pos, SEEK_SET );

              dec_input.strm_len =
                fread((u8*)dec_input.stream, sizeof(u8), VC1_MAX_STREAM_SIZE, finput);

              if(save_index && !slice_mode) {
                if(dec_input.stream[0] == 0 &&
                    dec_input.stream[1] == 0 &&
                    dec_input.stream[2] == 1) {
                  fprintf(f_index, "%" PRId64"\n", stream_pos);
                }
              }


              if (slice_mode) {
                dec_input.strm_len =
                  GetNextDuSize((u8*)dec_input.stream, dec_input.stream,
                                dec_input.strm_len, &skipped_bytes);
                stream_pos += skipped_bytes;
                fseeko64( finput, stream_pos, SEEK_SET );

                if(save_index) {
                  if(dec_input.stream[0] == 0 &&
                      dec_input.stream[1] == 0 &&
                      dec_input.stream[2] == 1) {
                    fprintf(f_index, "%" PRId64"\n", stream_pos);
                  }
                }

                dec_input.strm_len =
                  fread((u8*)dec_input.stream, sizeof(u8),
                        dec_input.strm_len, finput);
              }
            }
          }
          if (ferror(finput)) {
            DEBUG_PRINT(("STREAM READ ERROR\n"));
            goto end;
          }
          if (feof(finput)) {
            DEBUG_PRINT(("STREAM WILL END\n"));
            /*goto end;*/
          }
        }
      }
      break;

    case DEC_STRM_PROCESSED:
    case DEC_DISCARD_INTERNAL:
    case DEC_NONREF_PIC_SKIPPED:
      /* Used to indicate that picture decoding needs to finalized prior
         to corrupting next picture */

      /* If enough pictures decoded -> force decoding to end
      * by setting that no more stream is available */
      if ((max_num_pics && pic_decode_number == max_num_pics) ||
          ((dec_input.stream >= stream_stop ) && !long_stream))
        dec_input.strm_len = 0;
      else {
        /* Decode frame layer metadata */
        if( !long_stream ) {
          if( !advanced ) {
            dec_input.stream += dec_input.strm_len;
            dec_input.stream_bus_address += dec_input.strm_len;

            dec_input.strm_len =
              DecodeFrameLayerData((u8*)dec_input.stream);
            dec_input.stream += 4 + 4 * rcv_v2;
            if ((dec_input.stream + dec_input.strm_len) > stream_stop) {
              dec_input.strm_len = stream_stop > dec_input.stream ?
                                      stream_stop-dec_input.stream : 0;
            }
            dec_input.stream_bus_address += 4 + 4 * rcv_v2;
          } else {
            tmp = (dec_output.strm_curr_pos - dec_input.stream);
            stream_pos += tmp;
            dec_input.stream = dec_output.strm_curr_pos;
            dec_input.stream_bus_address += tmp;
            if (slice_mode) {
              dec_input.strm_len =
                GetNextDuSize(dec_output.strm_curr_pos,
                              byte_strm_start, strm_len, &skipped_bytes);
              dec_input.stream += skipped_bytes;
              dec_input.stream_bus_address += skipped_bytes;
              stream_pos += skipped_bytes;
            } else
              dec_input.strm_len = dec_output.data_left;
          }
        } else { /* LONG STREAM */
          if (!advanced) {
            ra = fread(tmp_strm, sizeof(u8),  4 + 4 * rcv_v2, finput);
            (void) ra;
            if (ferror(finput)) {
              DEBUG_PRINT(("STREAM READ ERROR\n"));
              goto end;
            }
            if (feof(finput)) {
              DEBUG_PRINT(("END OF STREAM\n"));
              dec_input.strm_len = 0;
              continue;
            }
            dec_input.strm_len = DecodeFrameLayerData(tmp_strm);
            ra = fread((u8*)dec_input.stream, sizeof(u8), dec_input.strm_len, finput);
            (void) ra;
          } else {
            if(use_index) {
              if(dec_output.data_left != 0) {
                dec_input.stream_bus_address += (dec_output.strm_curr_pos - dec_input.stream);
                dec_input.strm_len = dec_output.data_left;
                dec_input.stream = dec_output.strm_curr_pos;
              } else {
                dec_input.stream_bus_address = stream_mem.bus_address;
                dec_input.stream =  (u8 *) stream_mem.virtual_address;
                dec_input.strm_len = fillBuffer((u8*)dec_input.stream);
              }

            } else {
              tmp = (dec_output.strm_curr_pos - dec_input.stream);
              stream_pos += tmp;
              fseeko64( finput, stream_pos, SEEK_SET );

              dec_input.strm_len =
                fread((u8*)dec_input.stream, sizeof(u8),
                      VC1_MAX_STREAM_SIZE, finput);
              if (slice_mode) {
                dec_input.strm_len =
                  GetNextDuSize((u8*)dec_input.stream, dec_input.stream,
                                dec_input.strm_len, &skipped_bytes);
                stream_pos += skipped_bytes;
                fseeko64( finput, stream_pos, SEEK_SET );

                if(save_index) {
                  if(dec_input.stream[0] == 0 &&
                      dec_input.stream[1] == 0 &&
                      dec_input.stream[2] == 1) {
                    fprintf(f_index, "%" PRId64"\n", stream_pos);
                  }
                }

                dec_input.strm_len =
                  fread((u8*)dec_input.stream, sizeof(u8),
                        dec_input.strm_len, finput);
              }
            }
          }
          if (ferror(finput)) {
            DEBUG_PRINT(("STREAM READ ERROR\n"));
            goto end;
          }
          if (feof(finput)) {
            DEBUG_PRINT(("STREAM WILL END\n"));
            /*goto end;*/
          }
        }
      }
      break;

    case DEC_NO_DECODING_BUFFER:
      break;

    case DEC_END_OF_SEQ:
      dec_input.strm_len = 0;
      break;

    default:
      DEBUG_PRINT(("FATAL ERROR: %d\n", ret));
      goto end;
    }
    /* keep decoding until all data from input stream buffer consumed */
  } while(dec_input.strm_len > 0);

  printf("[TB] STREAM END ENCOUNTERED\n");
  if(save_index && advanced) {
    tmp = (dec_output.strm_curr_pos - dec_input.stream);
    stream_pos += tmp;
    fprintf(f_index, "%" PRId64"\n", stream_pos);
  }
  if(save_index || use_index) {
    fclose(f_index);
  }


end:
    VCDecEndOfStream(dec_inst);

  if(output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }

  add_buffer_thread_run = 0;
  DEBUG_PRINT(("\nWidth %u Height %u\n", coded_pic_width, coded_pic_height));

  if( stream_mem.virtual_address != NULL)
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
  /* release decoder instance */
  ReleaseExtBuffers();
  pthread_mutex_destroy(&ext_buffer_contro);
  START_SW_PERFORMANCE;
  VCDecRelease(dec_inst);
  END_SW_PERFORMANCE;
  DWLRelease(dwl_inst);
  VCDecLogDestory();

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

  if( f_stream_trace)
    fclose(f_stream_trace);
  if (long_stream && finput)
    fclose(finput);

  /* free allocated buffers */
  if (tmp_image)
    free(tmp_image);
  if (tmp_strm)
    free(tmp_strm);

  PrintOutputFileName(&params);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  DEBUG_PRINT(("[TB] NUMBER OF WRITTEN FRAMES %u\n", number_of_written_frames));
  if(cycle_count && number_of_written_frames)
    DEBUG_PRINT(("[TB] Average cycles/MB: %4u\n", cycle_count/number_of_written_frames));

  FINALIZE_SW_PERFORMANCE;

  DEBUG_PRINT(("[TB] DECODING DONE\n"));

  if(num_errors) {
    DEBUG_PRINT(("[TB] ERRORS FOUND in %u out of %u PICTURES\n",
                 num_errors, pic_decode_number));
    return 1;
  }

  return 0;
}

/*------------------------------------------------------------------------------

 Function name:  GetNextDuSize

  Purpose:
    Get stream slice by slice...

------------------------------------------------------------------------------*/
u32 GetNextDuSize(const u8* stream, const u8* stream_start, u32 strm_len, u32 *skipped_bytes) {
  const u8* p;
  u8 byte;
  u32 zero = 0;
  u32 size = 0;
  u32 total_size = 0;
  u32 sc_prefix = 0;
  u8 next_packet = 0;
  u32 tmp = 0;

  *skipped_bytes = 0;
  strm_len -= (stream - stream_start);
  p = stream;

  if (strm_len < 3)
    return strm_len;

  /* Seek start of the next slice */
  while(1) {
    byte = *p++;
    size++;
    total_size++;

    if (total_size >= strm_len)
      return size;

    if (!byte)
      zero++;
    else if ( (byte == 0x01) && (zero >=2) )
      sc_prefix = 1;
    else if (sc_prefix && (byte>=0x0A && byte<=0x0F) && slice_ud_in_packet) {
      DEBUG_PRINT(("slice_ud_in_packet\n"));
      if (tmp) {
        zero = 0;
        sc_prefix = 0;
        tmp = 0;
      } else {
        *skipped_bytes += (size-4);
        size -= *skipped_bytes;
        zero = 0;
        sc_prefix = 0;

        if (next_packet)
          size = 0;

        break;
      }
    } else if (sc_prefix && ((byte>=0x0A && byte<=0x0F) || (byte>=0x1B && byte<=0x1F)) && !slice_ud_in_packet) {
      DEBUG_PRINT(("No slice_ud_in_packet\n"));
      if (tmp) {
        zero = 0;
        sc_prefix = 0;
        tmp = 0;
      } else {
        *skipped_bytes += (size-4);
        size -= *skipped_bytes;
        zero = 0;
        sc_prefix = 0;

        if (next_packet)
          size = 0;

        break;
      }
    } else {
      zero = 0;
      sc_prefix = 0;
    }
  }
  zero = 0;
  /* Seek end of the next slice */
  while(1) {
    byte = *p++;
    size++;
    total_size++;

    if (total_size >= strm_len)
      return size;

    if (!byte)
      zero++;
    else if ( (byte == 0x01) && (zero >=2) )
      sc_prefix = 1;
    else if (sc_prefix && (byte>=0x0A && byte<=0x0F) && slice_ud_in_packet) {
      DEBUG_PRINT(("slice_ud_in_packet\n"));
      size -= 4;
      break;
    } else if (sc_prefix && ((byte>=0x0A && byte<=0x0F) || (byte>=0x1B && byte<=0x1F)) && !slice_ud_in_packet) {
      DEBUG_PRINT(("No slice_ud_in_packet\n"));
      size -= 4;
      break;
    } else {
      zero = 0;
      sc_prefix = 0;
    }
  }

  return size;
}

#if 0
/*------------------------------------------------------------------------------

    Function name: CropPicture

     Purpose:
     Perform cropping for picture. Input picture p_in_image with dimensions
     pic_width x pic_height is cropped into out_width x out_height and the
     resulting picture is stored in p_out_image.

------------------------------------------------------------------------------*/
u32 CropPicture(u8 *p_out_image, u8 *p_in_image,
                u32 pic_width, u32 pic_height, u32 out_width, u32 out_height ) {

  u32 i, j;
  u8 *p_out, *p_in;

  if (p_out_image == NULL || p_in_image == NULL ||
      !out_width || !out_height ||
      !pic_width || !pic_height) {
    /* just to prevent lint warning, returning non-zero will result in
        * return without freeing the memory */
    free(p_out_image);
    return(1);
  }

  /* Calculate starting pointer for luma */
  p_in = p_in_image;
  p_out = p_out_image;

  /* Copy luma pixel values */
  for (i = out_height; i; i--) {
    for (j = out_width; j; j--) {
      *p_out++ = *p_in++;
    }
    p_in += pic_width - out_width;
  }

  out_width >>= 1;
  out_height >>= 1;

  /* Calculate starting pointer for cb */
  p_in = p_in_image + pic_width*pic_height;

  /* Copy chroma pixel values */
  for (i = out_height; i; i--) {
    for (j = out_width*2; j; j--) {
      *p_out++ = *p_in++;
    }
    p_in += pic_width - out_width*2;
  }

  return (0);
}
#endif

/*------------------------------------------------------------------------------

    Function name:  VC1DecTrace

     Purpose:
     Example implementation of H264DecTrace function. Prototype of this
     function is given in H264DecApi.h. This implementation appends
     trace messages to file named 'dec_api.trc'.

------------------------------------------------------------------------------*/
/*void VC1DecTrace(const char *string)
{
    FILE *fp;

    fp = fopen("dec_api.trc", "at");

    if(!fp)
        return;

    fwrite(string, 1, strlen(string), fp);
    fwrite("\n", 1, 1, fp);

    fclose(fp);
}*/

#define SHOW1(p) (p[0]); p+=1;
#define SHOW2(p) (p[0]) | (p[1]<<8); p+=2;
#define SHOW3(p) (p[0]) | (p[1]<<8) | (p[2]<<16); p+=3;
#define SHOW4(p) (p[0]) | (p[1]<<8) | (p[2]<<16) | (p[3]<<24); p+=4;

#define    BIT0(tmp)  ((tmp & 1)   >>0);
#define    BIT1(tmp)  ((tmp & 2)   >>1);
#define    BIT2(tmp)  ((tmp & 4)   >>2);
#define    BIT3(tmp)  ((tmp & 8)   >>3);
#define    BIT4(tmp)  ((tmp & 16)  >>4);
#define    BIT5(tmp)  ((tmp & 32)  >>5);
#define    BIT6(tmp)  ((tmp & 64)  >>6);
#define    BIT7(tmp)  ((tmp & 128) >>7);

/*------------------------------------------------------------------------------

    Function name:  DecodeFrameLayerData

     Purpose:
     Decodes initialization frame layer from rcv format.

      Returns:
      Frame size in bytes.

------------------------------------------------------------------------------*/
u32 DecodeFrameLayerData(u8 *stream) {
  u32 tmp = 0;
  av_unused u32 time_stamp = 0;
  u32 frame_size = 0;
  u8 *p = stream;

  frame_size = SHOW3(p);
  tmp = SHOW1(p);
  tmp = BIT7(tmp);
  if( rcv_v2 ) {
    time_stamp = SHOW4(p);
    if (tmp == 1)
      DEBUG_PRINT(("[TB] INTRA FRAME timestamp: %u size: %u\n",
                   time_stamp, frame_size));
    else
      DEBUG_PRINT(("[TB] INTER FRAME timestamp: %u size: %u\n",
                   time_stamp, frame_size));
  } else {
    if (tmp == 1)
      DEBUG_PRINT(("[TB] INTRA FRAME size: %u\n", frame_size));
    else
      DEBUG_PRINT(("[TB] INTER FRAME size: %u\n", frame_size));
  }

  return frame_size;
}

/*------------------------------------------------------------------------------

    Function name:  DecodeRCV

     Purpose:
     Decodes initialization metadata from rcv format.

------------------------------------------------------------------------------*/
i32 DecodeRCV(u8 *stream, u32 strm_len, struct DecMetaData *meta_data) {
  u32 tmp1 = 0;
  u8 *p;
  p = stream;

  if (strm_len < 9*4+8)
    return -1;

  tmp1 = SHOW3(p);
  DEBUG_PRINT(("[TB] Numframes: \t %u\n", tmp1));
  tmp1 = SHOW1(p);
  DEBUG_PRINT(("[TB] 0xC5: \t\t %0X\n", tmp1));
  if( tmp1 & 0x40 )   rcv_v2 = 1;
  else                rcv_v2 = 0;
  DEBUG_PRINT(("[TB] RCV_VERSION: \t\t %u\n", rcv_v2+1 ));

  rcv_metadata_size = SHOW4(p);
  DEBUG_PRINT(("[TB] 0x04: \t\t %0X\n", rcv_metadata_size));

  /* Decode image dimensions */
  p += rcv_metadata_size;
  tmp1 = SHOW4(p);
  meta_data->max_coded_height = tmp1;
  tmp1 = SHOW4(p);
  meta_data->max_coded_width = tmp1;

  return 0;
}

/*------------------------------------------------------------------------------

    FramePicture

        Create frame of max-coded-width*max-coded-height around output image.
        Useful for system model verification, this way we can directly compare
        our output to the DecRef_* output generated by the reference decoder..

------------------------------------------------------------------------------*/
void FramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height ) {

  /* Variables */

  i32 x, y;

  /* Code */

  memset( p_out, 0, out_width*out_height*3/2 );

  /* Luma */
  p_out += out_width * ( out_height - in_height );
  for ( y = 0 ; y < in_height ; ++y ) {
    p_out += ( out_width - in_width );
    for( x = 0 ; x < in_width; ++x )
      *p_out++ = *p_in++;
    p_in += ( in_frame_width - in_width );
  }

  p_in += in_frame_width * ( in_frame_height - in_height );

  in_frame_height /= 2;
  in_frame_width /= 2;
  out_height /= 2;
  out_width /= 2;
  in_height /= 2;
  in_width /= 2;

  /* Chroma */
  p_out += 2 * out_width * ( out_height - in_height );
  for ( y = 0 ; y < in_height ; ++y ) {
    p_out += 2 * ( out_width - in_width );
    for( x = 0 ; x < 2*in_width; ++x )
      *p_out++ = *p_in++;
    p_in += 2 * ( in_frame_width - in_width );
  }

}

/*------------------------------------------------------------------------------

    Function name:            printVc1PicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
void printVc1PicCodingType(u32 *pic_type) {
  switch (pic_type[0]) {
  case DEC_PIC_TYPE_I:
    printf(" [I:");
    break;
  case DEC_PIC_TYPE_P:
    printf(" [P:");
    break;
  case DEC_PIC_TYPE_B:
    printf(" [B:");
    break;
  case DEC_PIC_TYPE_BI:
    printf(" [BI:");
    break;
  default:
    printf(" [Other %u:", *pic_type);
    break;
  }

  switch (pic_type[1]) {
  case DEC_PIC_TYPE_I:
    printf("I],");
    break;
  case DEC_PIC_TYPE_P:
    printf("P],");
    break;
  case DEC_PIC_TYPE_B:
    printf("B],");
    break;
  case DEC_PIC_TYPE_BI:
    printf("BI],");
    break;
  default:
    printf("Other %u],", *pic_type);
    break;
  }
}

/*------------------------------------------------------------------------------

    Function name: fillBuffer

------------------------------------------------------------------------------*/
u32 fillBuffer(u8 *stream) {
  u32 amount = 0;
  u32 data_len = 0;
  off64_t pos = ftello64(finput);
  int ret;
  if(cur_index != pos) {
    fseeko64(finput, cur_index, SEEK_SET);
  }
  ret = fscanf(f_index, "%zx", &next_index);
  (void) ret;

  if (next_index >= 65536) return 0;
  amount += next_index - cur_index;
  cur_index = next_index;

  /* read data */
  data_len = fread(stream, 1, amount, finput);

  return data_len;
}

