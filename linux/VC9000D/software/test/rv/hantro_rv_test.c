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

#include <ctype.h>
#include <pthread.h>
#include "vcdecapi.h"
#include "rm_parse.h"
#include "rv_depack.h"
#include "rv_decode.h"
#include "deccfg.h"
#include "common_sink.h"
#include "hantro_rv_test.h"
#include "tb_cfg.h"
#include "regdrv.h"
#include "command_line_parser.h"
#include "vcd_tools.h"

#include "dec_log.h"
#ifdef MODEL_SIMULATION
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#endif

#include "tb_sw_performance.h"

#define RM2YUV_INITIAL_READ_SIZE    16
#define MAX_BUFFERS 34

#define MB_MULTIPLE(x)  (((x)+15)&~15)

/* secure mode */
u32 secure_mode;
u32 max_coded_width = 0;
u32 max_coded_height = 0;
u32 max_frame_width = 0;
u32 max_frame_height = 0;
u32 enable_frame_picture = 1;
u8 *frame_buffer;
FILE *frame_out = NULL;
u32 disable_output_writing = 0;
u32 pic_display_number = 0;
u32 frame_number = 0;
u32 cycle_count = 0; /* Sum of average cycles/mb counts */
u32 number_of_written_frames = 0;
u32 num_frame_buffers = 0;
u32 convert_tiled_output = 0;
struct TestParams params;
struct OutFileInfo outfile_info;

void FramePicture( u8 *p_in, i32 in_width, i32 in_height,
                   i32 in_frame_width, i32 in_frame_height,
                   u8 *p_out, i32 out_width, i32 out_height );

u32 rv_input = 0;

u32 use_peek_output = 0;
enum DecSkipFrameMode skip_frame = 0;


#ifdef RV_TIME_TRACE
#include "../timer/timer.h"
#define TIMER_MAX_COUNTER   0xFFFFFFFF
#define TIMER_FACTOR_MSECOND 1000
u32 ul_start_time_DEC, ul_stop_time_DEC;    /* HW decode time (us) */
u32 ul_start_time_LCD, ul_stop_time_LCD;    /* display time (us) */
u32 ul_start_time_Full, ul_stop_time_Full;  /* total time (us), including parser(sw), driver(sw) & decoder(hw) */
double f_max_time_DEC, f_time_DEC, f_total_time_DEC;
double f_max_time_LCD, f_time_LCD, f_total_time_LCD;
double f_max_time_Full, f_time_Full, f_total_time_Full;
char str_time_info[0x100];
#endif

#ifdef RV_DISPLAY
#include "../../video/v830video_type.h"
#include "../../video/highapi/hbridge.h"
#include "../../video/driver/dipp.h"
#include "../../video/panel/panel.h"

static TSize g_srcimg;
static TSize g_dstimg;
static u32 g_bufmode = LBUF_TWOBUF;
static u32 g_delay_time = 0;
#endif

#ifdef RV_ERROR_SIM
#include "../../tools/random.h"
static u32 g_randomerror = 0;
#endif

#define DWLFile FILE

#define DWL_SEEK_CUR    SEEK_CUR
#define DWL_SEEK_END    SEEK_END
#define DWL_SEEK_SET    SEEK_SET

#define DWLfopen(filename,mode)                 fopen(filename, mode)
#define DWLfread(buf,size,_size_t,filehandle)   fread(buf,size,_size_t,filehandle)
#define DWLfwrite(buf,size,_size_t,filehandle)  fwrite(buf,size,_size_t,filehandle)
#define DWLftell(filehandle)                    ftell(filehandle)
#define DWLfseek(filehandle,offset,whence)      fseek(filehandle,offset,whence)
#define DWLfrewind(filehandle)                  rewind(filehandle)
#define DWLfclose(filehandle)                   fclose(filehandle)
#define DWLfeof(filehandle)                     feof(filehandle)
#define DWLremove(filehandle)                   remove(filehandle)

#undef ERROR_PRINT
#define ERROR_PRINT(msg) printf msg
#undef ASSERT
#define ASSERT(s)

struct TBCfg tb_cfg;
u32 clock_gating = DEC_X170_INTERNAL_CLOCK_GATING;
u32 data_discard = DEC_X170_DATA_DISCARD_ENABLE;
u32 latency_comp = DEC_X170_LATENCY_COMPENSATION;
u32 output_picture_endian = DEC_X170_OUTPUT_PICTURE_ENDIAN;
u32 bus_burst_length = DEC_X170_BUS_BURST_LENGTH;
u32 asic_service_priority = DEC_X170_ASIC_SERVICE_PRIORITY;
u32 service_merge_disable = DEC_X170_SERVICE_MERGE_DISABLE;

u32 pic_big_endian_size = 0;
u8* pic_big_endian = NULL;

i32 corrupted_bytes = 0;
u32 pic_rdy = 0;
u32 packetize = 0;
u32 md5sum = 0;

static u32 g_iswritefile = 0;
static u32 g_isdisplay = 0;
static u32 g_displaymode = 0;
static u32 g_bfirstdisplay = 0;
static u32 g_rotationmode = 0;
static u32 g_blayerenable = 0;
#ifdef RV_TIME_TRACE
static u32 g_islog2std = 0;
static u32 g_islog2file = 0;
#endif
static DWLFile *g_fp_time_log = NULL;

/* Decoder instance */
typedef void *RvDecInst;
static RvDecInst dec_inst = NULL;
YuvSink* yuvsink; /* Yuvsink instance. */

typedef struct {
  char         file_path[0x100];
  char         out_file_name[DEC_MAX_OUT_COUNT][256];
  DWLFile*     fp_out;
  rv_depack*   p_depack;
  rv_decode*   p_decode;
  BYTE*        p_out_frame;
  UINT32       ul_out_frame_size;
  UINT32       ul_width;
  UINT32       ul_height;
  UINT32       ul_num_input_frames;
  UINT32       ul_num_output_frames;
  UINT32       ul_max_num_dec_frames;
  UINT32       ul_start_frame_id;    /* 0-based */
  UINT32       b_dec_end;
  UINT32       ul_total_time;       /* ms */
} rm2yuv_info;

static HX_RESULT rv_frame_available(void* p_avail, UINT32 ul_sub_stream_num, rv_frame* p_frame);
static HX_RESULT rv_decode_stream_decode(rv_decode* p_front_end, struct DecPictures* output);
static void rv_decode_frame_flush(rm2yuv_info *p_info, struct DecPictures *output);

static UINT32 rm_io_read(void* p_user_read, BYTE* p_buf, UINT32 ul_bytes_to_read);
static void rm_io_seek(void* p_user_read, UINT32 ulOffset, UINT32 ul_origin);
static void* rm_memory_malloc(void* p_user_mem, UINT32 ul_size);
static void rm_memory_free(void* p_user_mem, void* ptr);
static void* rm_memory_create_ncnb(void* p_user_mem, UINT32 ul_size);
static void rm_memory_destroy_ncnb(void* p_user_mem, void* ptr);

static void parse_path(const char *path, char *dir, char *file);
static void rm2yuv_error(void* p_error, HX_RESULT result, const char* psz_msg);
static void printRvVersion(u32 client_type);
#ifdef RV_DISPLAY
static void displayOnScreen(struct DecPictures *p_dec_picture);
#endif
static void RV_Time_Init(i32 islog2std, i32 islog2file);
static void RV_Time_Full_Reset(void);
static void RV_Time_Full_Pause(void);
static void RV_Time_Dec_Reset(void);
static void RV_Time_Dec_Pause(u32 picid, u32 pictype);
static void printRvPicCodingType(u32 pic_type);

static u32 max_num_pics = 0;
static u32 pics_decoded = 0;
DecPicAlignment align = DEC_ALIGN_128B;  /* default: 16 bytes alignment */
u32 pp_enabled = 0;

const void *dwl_inst = NULL;
u32 use_extra_buffers = 0;
u32 allocate_extra_buffers_in_output = 0;
u32 buffer_size;
u32 num_buffers;  /* external buffers allocated yet. */
u32 add_buffer_thread_run = 0;
pthread_t add_buffer_thread;
pthread_mutex_t ext_buffer_contro;
struct DWLLinearMem ext_buffers[MAX_BUFFERS];
struct DWLLinearMem rpr_ext_buffers;

u32 add_extra_flag = 0;
/* Fixme: this value should be set based on option "-d" when invoking testbench. */
u32 buffer_release_flag = 1;
struct TOOL_PARAMS tool_params;

/* These global values are found from commonconfig.c.
 * partial tb_cfg.dec_params parameters. */
extern struct DecParams dec_params;

#ifdef USE_RANDOM_ERROR_TEST
/* These global values are found from vcdecapi.c.
 * partial tb_cfg.tb_params parameters for random error injectionn. */
extern struct ErrorParams random_error_params;
#endif

static void *AddBufferThread(void *arg) {
  usleep(100000);
  while(add_buffer_thread_run) {
    pthread_mutex_lock(&ext_buffer_contro);
    if(add_extra_flag && num_buffers < MAX_BUFFERS) {
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

  if(rpr_ext_buffers.virtual_address) {
    if (pp_enabled)
      DWLFreeLinear(dwl_inst, &rpr_ext_buffers);
    else
      DWLFreeRefFrm(dwl_inst, &rpr_ext_buffers);
    DWLmemset(&rpr_ext_buffers, 0, sizeof(rpr_ext_buffers));
  }
  pthread_mutex_unlock(&ext_buffer_contro);
}

pthread_t output_thread;
pthread_t release_thread;
int output_thread_run = 0;
u32 is_rm_file = 0;
rm2yuv_info info;

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
  return (NULL);
}



/* Output thread entry point. */
static void* rv_output_thread(void* arg) {
  struct DecPictures output[1];

  while(output_thread_run) {
    enum DecRet ret;
    u32 i, only_once = 0, ext_id = 0xFFFF;

    ret = VCDecNextPicture(dec_inst, output);
    if(ret == DEC_PIC_RDY) {
      if(use_peek_output) continue;
      if(!is_rm_file) {
        for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          if (!output->pictures[i].sequence_info.pic_width ||
              !output->pictures[i].sequence_info.pic_height)
            continue;

          if (only_once == 0) {
            SwMatchOuputBufferId(&tool_params, &output->pictures[i].luma, &ext_id);
            if (ext_id < MAX_BUFFERS)
              DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                              tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
            only_once = 1;
          }
          yuvsink->WritePicture(yuvsink->inst, &output->pictures[i], i);
        }
      } else rv_decode_frame_flush(&info, output);

      //DEBUG_PRINT(("[TB] Get frame %d.\n", p_info->ul_num_output_frames));
      DEBUG_PRINT(("[TB] PIC %2u/%2u, type %s,", pic_display_number + 1,
                 output->pictures[0].picture_info.decode_id + 1,
                 output->pictures[0].picture_info.is_intra_frame ? "key picture" : "non key picture"));
      /* pic coding type */
      printRvPicCodingType(output->pictures[0].picture_info.pic_coding_type);
      /* printf : cycles per mbs */
      if (output->pictures[0].picture_info.cycles_per_mb) {
        cycle_count += output->pictures[0].picture_info.cycles_per_mb;
        DEBUG_PRINT((" %4u cycles / mb,", output->pictures[0].picture_info.cycles_per_mb));
      }
      DEBUG_PRINT((" %u x %u, Crop: (%u, %u), %u x %u\n",
                 output->pictures[0].sequence_info.scaled_width,
                 output->pictures[0].sequence_info.scaled_height,
                 output->pictures[0].sequence_info.crop_params.crop_left_offset,
                 output->pictures[0].sequence_info.crop_params.crop_top_offset,
                 output->pictures[0].sequence_info.crop_params.crop_out_width,
                 output->pictures[0].sequence_info.crop_params.crop_out_height));
      /* Push output buffer into buf_list and wait to be consumed */
      buf_list[list_push_index] = output[0];
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

  if(cycle_count && pic_display_number)
    printf("[TB] Average cycles/MB: %4u\n", cycle_count/pic_display_number);

  return (NULL);
}



/*-----------------------------------------------------------------------------------------------------
function:
    demux input rm file, decode rv stream threrein, and display on LCD or write to file if necessary

input:
    filename:       the file path and name to be display
    startframeid:   the beginning frame of the whole input stream to be decoded
    maxnumber:      the maximal number of frames to be decoded
    iswritefile:    write the decoded frame to local file or not, 0: no, 1: yes
    isdisplay:      send the decoded data to display device or not, 0: no, 1: yes
    displaymode:    display mode on LCD, 0: auto size, 1: full screen
    rotationmode:   rotation mode on LCD, 0: normal, 1: counter-clockwise, 2: clockwise
    islog2std:      output time log to console
    islog2file:     output time log to file
    errconceal:     error conceal mode (reserved)

return:
    NULL
------------------------------------------------------------------------------------------------------*/
u32 NextPacket(u8 ** p_strm, u8* stream_stop, u32 is_rv8) {
  u32 index;
  u32 max_index;
  static u32 prev_index = 0;
  static u8 *stream = NULL;
  u8 *p;

  index = 0;

  if(stream == NULL)
    stream = *p_strm;
  else
    stream += prev_index;

  max_index = (u32) (stream_stop - stream);

  if(stream > stream_stop)
    return (0);

  if(max_index == 0)
    return (0);

  /* Search stream for next start code prefix */
  /*lint -e(716) while(1) used consciously */
  p = stream + 1;
  while(p < stream_stop) {
    /* RV9 */
    if (is_rv8) {
      if (p[0] == 0   && p[1] == 0   && p[2] == 1  &&
          !(p[3]&170) && !(p[4]&170) && !(p[5]&170) && (p[6]&170) == 2)
        break;
    } else {
      if (p[0] == 85  && p[1] == 85  && p[2] == 85  && p[3] == 85 &&
          !(p[4]&170) && !(p[5]&170) && !(p[6]&170) && (p[7]&170) == 2)
        break;
    }
    p++;
  }

  index = p - stream;
  /* Store pointer to the beginning of the packet */
  *p_strm = stream;
  prev_index = index;

  return (index);
}

/* Stuff for parsing/reading RealVideo streams */
typedef struct {
  FILE *fid;
  u32 w, h;
  u32 len;
  u32 is_rv8;
  u32 num_frame_sizes;
  u32 frame_sizes[18];
  u32 num_segments;
  struct DecSliceInfo slice_info[128];
} rvParser_t;

u32 RvParserInit(rvParser_t *parser, char *file) {
  u32 tmp;
  char buff[100];
  int ret;

  parser->fid = fopen(file, "rb");
  if (parser->fid == NULL) {
    return (HXR_FAIL);
  }

  ret = fread(buff, 1, 4, parser->fid);
  (void) ret;

  /* length */
  tmp = (buff[0] << 24) |
        (buff[1] << 16) |
        (buff[2] <<  8) |
        (buff[3] <<  0);

  tmp = fread(buff, 1, tmp-4, parser->fid);
  if (tmp <= 0)
    return(HXR_FAIL);

  /* 32b len
   * 32b 'VIDO'
   * 32b 'RV30'/'RV40'
   * 16b width
   * 16b height
   * 80b stuff */

  if (strncmp(buff+4, "RV30", 4) == 0)
    parser->is_rv8 = 1;
  else
    parser->is_rv8 = 0;

  parser->w = (buff[8] << 8) | buff[9];
  parser->h = (buff[10] << 8) | buff[11];

  if (parser->is_rv8) {
    u32 i;
    char *p = buff + 22; /* header size 26 - four byte length field */
    parser->num_frame_sizes = 1 + (p[1] & 0x7);
    p += 8;
    parser->frame_sizes[0] = parser->w;
    parser->frame_sizes[1] = parser->h;
    for (i = 1; i < parser->num_frame_sizes; i++) {
      parser->frame_sizes[2*i + 0] = (*p++) << 2;
      parser->frame_sizes[2*i + 1] = (*p++) << 2;
    }
  }
  return (HXR_OK);
}

void RvParserRelease(rvParser_t *parser) {
  if (parser->fid)
    fclose(parser->fid);

  parser->fid = NULL;
}

u32 RvParserReadFrame(rvParser_t *parser, u8 *p) {
  u32 i, tmp;
  u8 buff[256];

  tmp = fread(buff, 1, 20, parser->fid);
  if (tmp < 20)
    return (HXR_FAIL);
  parser->len = (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
  parser->num_segments =
    (buff[16] << 24) | (buff[17] << 16) | (buff[18] << 8) | buff[19];

  for (i = 0; i < parser->num_segments; i++) {
    tmp = fread(buff, 1, 8, parser->fid);
    parser->slice_info[i].is_valid =
      (buff[0] << 24) | (buff[1] << 16) | (buff[2] << 8) | buff[3];
    parser->slice_info[i].offset =
      (buff[4] << 24) | (buff[5] << 16) | (buff[6] << 8) | buff[7];
  }

  tmp = fread(p, 1, parser->len, parser->fid);

  if (tmp > 0)
    return HXR_OK;
  else
    return HXR_FAIL;
}

void rv_mode(const char *filename,
             char *filename_pp_cfg,
             int startframeid,
             int maxnumber,
             int iswritefile,
             int isdisplay,
             int displaymode,
             int rotationmode,
             int islog2std,
             int islog2file,
             int blayerenable,
             int errconceal,
             int randomerror) {
  UINT32              i              = 0;
  char                rv_outfilename[256];
  UINT8               size[9] = {0,1,1,2,2,3,3,3,3};
  rvParser_t*         parser         = HXNULL;

  INIT_SW_PERFORMANCE;

  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  struct DecBufferInfo hbuf;
  DWLmemset(&hbuf, 0, sizeof(struct DecBufferInfo));
  enum DecRet rv;
  struct DWLInitParam dwl_init;
  enum DecRet rv_ret;
  struct DecInputParameters dec_input;
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  struct DecOutput dec_output;
  DWLmemset(&dec_output, 0, sizeof(struct DecOutput));
  struct DecSequenceInfo dec_info;
  DWLmemset(&dec_info, 0, sizeof(struct DecSequenceInfo));
  u32 stream_len;
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  struct DWLLinearMem stream_mem;
  DWLmemset(&stream_mem, 0, sizeof(struct DWLLinearMem));
  DWLFile *fp_out = HXNULL;
  DWLmemset(rv_outfilename, 0, sizeof(rv_outfilename));

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
  if (!OpenAsicTraceFiles())
    DEBUG_PRINT(("[TB] UNABLE TO OPEN TRACE FILE(S)\n"));
#endif

  /* Initialize all static global variables */
  dec_inst = NULL;
  g_iswritefile = iswritefile;
  g_isdisplay = isdisplay;
  g_displaymode = displaymode;
  g_rotationmode = rotationmode;
  g_bfirstdisplay = 0;
  g_blayerenable = blayerenable;

#ifdef RV_ERROR_SIM
  g_randomerror = randomerror;
#endif


  RV_Time_Init(islog2std, islog2file);

  parser = (rvParser_t *)malloc(sizeof(rvParser_t));
  if (parser == NULL)
  	return;
  memset(parser, 0, sizeof(rvParser_t));

  RvParserInit(parser, (char *)filename);

  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
  tool_params.dwl_inst = dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("[TB] ERROR: DWL Init failed"));
    goto cleanup;
  }

  printRvVersion(dwl_init.client_type);

  init_config.codec = DEC_RV;
  init_config.error_handling = params.error_handling;
  init_config.frame_code_length = size[parser->num_frame_sizes];
  init_config.frame_sizes = parser->frame_sizes;
  init_config.rv_version = !parser->is_rv8;
  init_config.max_frame_width = parser->w;
  init_config.max_frame_height = parser->h;
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
        return;
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
  rv_ret = VCDecInit((const void**)&dec_inst, &init_config);
  END_SW_PERFORMANCE;
  if (rv_ret != DEC_OK) {
    ERROR_PRINT(("[TB] VCDecInit fail! \n"));
    goto cleanup;
  }

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  rv_ret = VCDecGetInfo(dec_inst, &dec_info);
  END_SW_PERFORMANCE;
  if(rv_ret) {
    printf("[TB] VCDecGetInfo ret: %s\n", VCDecRetStr(rv_ret));
  }

  /* memory for stream buffer */
  stream_len = 10000000;
#ifdef SUPPORT_DMA
  stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if (DWLMallocLinear(dwl_inst, stream_len, &stream_mem) != DWL_OK) {
    DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    goto cleanup;
  }
  dec_input.stream_buffer = stream_mem;

  dec_input.skip_frame = skip_frame;
  dec_input.pic_id = 0;
  dec_input.timestamp = 0;

  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_RV_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;

  while (dec_input.strm_len == dec_output.data_left ||
    RvParserReadFrame(parser, (u8*)stream_mem.virtual_address) == HXR_OK) {
    dec_input.strm_len = parser->len;
    dec_input.stream = (u8*)stream_mem.virtual_address;
    dec_input.stream_bus_address = stream_mem.bus_address;
    dec_input.pic_id = pics_decoded;
    dec_input.slice_info_num = parser->num_segments;
    dec_input.slice_info = parser->slice_info;

    START_SW_PERFORMANCE;
    rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
    END_SW_PERFORMANCE;

    printf("[TB] Decoding on pic_id %u VCDecDecode ret: %s\n", dec_input.pic_id, VCDecRetStr(rv_ret));

    if (rv_ret == DEC_HDRS_RDY) {
      rv_ret = VCDecGetInfo(dec_inst, &dec_info);
      printf("[TB] *********************** VCDecGetInfo()/RAW ******************\n");
      printf("[TB] dec_info.pic_width %u\n",(dec_info.pic_width));
      printf("[TB] dec_info.pic_height %u\n",(dec_info.pic_height));
      printf("[TB] dec_info.scaled_width %u\n",dec_info.scaled_width);
      printf("[TB] dec_info.scaled_height %u\n",dec_info.scaled_height);
      if(dec_info.output_format == DEC_OUT_FRM_YUV420SP)
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420SP\n");
      else
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420TILE\n");
      printf("[TB] ******************* RAW MODE ********************************\n");
      pp_enabled = params.pp_enabled;
      if (params.ppu_cfg[0].enabled) {
        if (!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.scaled_width;
          params.ppu_cfg[0].crop.height = dec_info.scaled_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }
      }

      /* Create output sink after output file names are determined. */
      if (yuvsink == NULL) {
        outfile_info.bitstream_format = BITSTREAM_RM;
        outfile_info.pic_width = dec_info.pic_width;
        outfile_info.pic_height = dec_info.pic_height;
        outfile_info.bit_depth = (dec_info.bit_depth_luma == 8 && dec_info.bit_depth_chroma == 8) ? 8 : 10;
        outfile_info.is_interlaced = dec_info.is_interlaced;
        params.compress_bypass = TRUE; /* G1 don't support rfc */
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          goto cleanup;
        }
      }

      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      u32 tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK)
        goto cleanup;

      START_SW_PERFORMANCE;
      rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
      END_SW_PERFORMANCE;
      if (rv_ret == DEC_WAITING_FOR_BUFFER) {
        rv = VCDecGetBufferInfo(dec_inst, &hbuf);

        printf("[TB] VCDecGetBufferInfo ret %d\n", rv);
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
          buffer_size = hbuf.next_buf_size;
          struct DWLLinearMem mem = {0};
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
            rv = VCDecAddBuffer(dec_inst, &mem);
            printf("[TB] VCDecAddBuffer ret %d\n", rv);

            if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              if(i <= (hbuf.buf_num - 1))
                ext_buffers[i] = mem;
              else
                rpr_ext_buffers = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers = hbuf.buf_num;
          add_extra_flag = 1;
        }

        START_SW_PERFORMANCE;
        rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
        END_SW_PERFORMANCE;
      }
    }

    switch(rv_ret) {
    case DEC_OK:
    case DEC_PIC_DECODED:
      printf("[TB] Pic %u decoded\n", pics_decoded);
      pics_decoded++;
#if 0
      if (pics_decoded == 10) {
        rv_ret = VCDecAbort(dec_inst);
        rv_ret = VCDecAbortAfter(dec_inst);
      }
#endif

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, rv_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }

      break;

    case DEC_HDRS_RDY:
      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      rv_ret = VCDecGetInfo(dec_inst, &dec_info);
      END_SW_PERFORMANCE;
      if(rv_ret) {
        printf("[TB] VCDecGetInfo ret: %s\n", VCDecRetStr(rv_ret));
      }

      printf("[TB] *********************** VCDecGetInfo()/RAW ******************\n");
      printf("[TB] dec_info.pic_width %u\n",dec_info.pic_width);
      printf("[TB] dec_info.pic_height %u\n",dec_info.pic_height);
      printf("[TB] dec_info.scaled_width %u\n",dec_info.scaled_width);
      printf("[TB] dec_info.scaled_height %u\n",dec_info.scaled_height);
      if(dec_info.output_format == DEC_OUT_FRM_YUV420SP)
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420SP\n");
      else
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420TILE\n");
      printf("[TB] ******************* RAW MODE ********************************\n");

      pp_enabled = params.pp_enabled;
      if (params.ppu_cfg[0].enabled) {
        if (!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.scaled_width;
          params.ppu_cfg[0].crop.height = dec_info.scaled_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }
      }

      /* Create output sink after output file names are determined. */
      if (yuvsink == NULL) {
        outfile_info.bitstream_format = BITSTREAM_RM;
        outfile_info.pic_width = dec_info.pic_width;
        outfile_info.pic_height = dec_info.pic_height;
        outfile_info.bit_depth = (dec_info.bit_depth_luma == 8 && dec_info.bit_depth_chroma == 8) ? 8 : 10;
        outfile_info.is_interlaced = dec_info.is_interlaced;
        params.compress_bypass = TRUE; /* G1 don't support rfc */
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          goto cleanup;
        }
      }

      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      u32 tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK)
        goto cleanup;

      break;
    case DEC_WAITING_FOR_BUFFER:
      rv = VCDecGetBufferInfo(dec_inst, &hbuf);
      printf("[TB] VCDecGetBufferInfo ret %d\n", rv);
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
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem = {0};
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
          rv = VCDecAddBuffer(dec_inst, &mem);
          printf("[TB] VCDecAddBuffer ret %d\n", rv);

          if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            if(i <= (hbuf.buf_num - 1))
              ext_buffers[i] = mem;
            else
              rpr_ext_buffers = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = hbuf.buf_num;
        add_extra_flag = 1;
      }
      break;

    case DEC_NONREF_PIC_SKIPPED:
    case DEC_NO_DECODING_BUFFER:
      break;

    default:
      /*
      * DEC_NOT_INITIALIZED
      * DEC_PARAM_ERROR
      * DEC_STRM_ERROR
      * DEC_STRM_PROCESSED
      * DEC_SYSTEM_ERROR
      * DEC_MEMFAIL
      * DEC_HW_TIMEOUT
      * DEC_HW_RESERVED
      * DEC_HW_BUS_ERROR
      */
      DEBUG_PRINT(("[TB] Fatal Error, Decode end!\n"));
      break;
    }

    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
  }

  VCDecEndOfStream(dec_inst);

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

  if (pic_big_endian != NULL)
    free(pic_big_endian);

  TIME_PRINT(("[TB] \n"));

cleanup:
  add_buffer_thread_run = 0;

  if(output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }
  RvParserRelease(parser);

  /* Close the output file */
  if (fp_out) {
    DWLfclose(fp_out);
  }

  printf("[TB] Output file: %s\n", params.out_file_name[0]);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  /* Release the decoder instance */
  if (dec_inst) {
    START_SW_PERFORMANCE;
    ReleaseExtBuffers();
    pthread_mutex_destroy(&ext_buffer_contro);
    VCDecRelease(dec_inst);
    DWLRelease(dwl_inst);
    END_SW_PERFORMANCE;
  }

  FINALIZE_SW_PERFORMANCE;

  free(parser);
#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif

}

void raw_mode(DWLFile* fp_in,
              char *filename_pp_cfg,
              int startframeid,
              int maxnumber,
              int iswritefile,
              int isdisplay,
              int displaymode,
              int rotationmode,
              int islog2std,
              int islog2file,
              int blayerenable,
              int errconceal,
              int randomerror) {
  UINT32              i              = 0;
  char                rv_outfilename[256];
  char                tmp_stream[4];
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;

  INIT_SW_PERFORMANCE;

  struct DecBufferInfo hbuf;
  DWLmemset(&hbuf, 0, sizeof(struct DecBufferInfo));
  enum DecRet rv;
  struct DWLInitParam dwl_init;
  enum DecRet rv_ret;
  struct DecOutput dec_output;
  DWLmemset(&dec_output, 0, sizeof(struct DecOutput));
  struct DecInputParameters dec_input;
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  struct DecSequenceInfo dec_info;
  DWLmemset(&dec_info, 0, sizeof(struct DecSequenceInfo));
  struct DWLLinearMem stream_mem;
  DWLmemset(&stream_mem, 0, sizeof(struct DWLLinearMem));
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  u32 stream_len;
  u8 *stream_stop;
  u8 *ptmpstream;
  u32 tmp;
  u32 is_rv8;
  int ret;

  DWLFile *fp_out = HXNULL;
  DWLmemset(rv_outfilename, 0, sizeof(rv_outfilename));

  DEBUG_PRINT(("[TB] Start decode ...\n"));

  ret = DWLfread(tmp_stream, sizeof(u8), 4, fp_in);
  (void) ret;
  DWLfseek(fp_in, 0, DWL_SEEK_SET);

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
  dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("[TB] ERROR: DWL Init failed"));
    return;
  }

  /* for color remapping (3dlut) */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      u32 size = 18 * 18 * 18 * 3 * sizeof(u16);
      // ppu_int_cfg.table_3dlut.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      config.table_3dlut_buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                        DWL_MEM_TYPE_CPU;
      if (DWLMallocLinear(dwl_inst, size, &config.table_3dlut_buffer)) {
        return;
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

  /* RV9 raw stream */
  if (tmp_stream[0] == 0 && tmp_stream[1] == 0 &&
      tmp_stream[2] == 1) {
    init_config.codec = DEC_RV;
    init_config.error_handling = params.error_handling;
    init_config.frame_code_length = 0;
    init_config.frame_sizes = 0;
    init_config.rv_version = 0;
    init_config.max_frame_width = 0;
    init_config.max_frame_height = 0;
    init_config.num_frame_buffers = num_frame_buffers;
    init_config.use_adaptive_buffers = 1;
    init_config.guard_size = 0;
    init_config.dwl_inst = dwl_inst;
    START_SW_PERFORMANCE;
    rv_ret = VCDecInit((const void**)&dec_inst, &init_config);
    END_SW_PERFORMANCE;
    is_rv8 = 1;
  } else {
    init_config.codec = DEC_RV;
    init_config.error_handling = params.error_handling;
    init_config.frame_code_length = 0;
    init_config.frame_sizes = 0;
    init_config.rv_version = 1;
    init_config.max_frame_width = 0;
    init_config.max_frame_height = 0;
    init_config.num_frame_buffers = num_frame_buffers;
    init_config.use_adaptive_buffers = 1;
    init_config.guard_size = 0;
    init_config.dwl_inst = dwl_inst;
    START_SW_PERFORMANCE;
    rv_ret = VCDecInit((const void**)&dec_inst, &init_config);
    END_SW_PERFORMANCE;
    is_rv8 = 0;
  }

  if (rv_ret != DEC_OK) {
    ERROR_PRINT(("[TB] VCDecInit fail! \n"));
    return;
  }

  stream_len = 10000000;
#ifdef SUPPORT_DMA
  stream_mem.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(stream_mem.mem_type, DWL_MEM_USAGE_IN_STRM, secure_mode);
  if(DWLMallocLinear(dwl_inst,
                     stream_len, &stream_mem) != DWL_OK) {
    DEBUG_PRINT(("[TB] UNABLE TO ALLOCATE STREAM BUFFER MEMORY\n"));
    return;
  }
  dec_input.stream_buffer = stream_mem;

  stream_len = DWLfread(stream_mem.virtual_address, sizeof(u8), stream_len, fp_in);
  stream_stop = (u8*)stream_mem.virtual_address+stream_len;

  dec_input.skip_frame = skip_frame;
  dec_input.stream = (u8*)stream_mem.virtual_address;
  dec_input.stream_bus_address = stream_mem.bus_address;
  dec_input.pic_id = 0;
  dec_input.timestamp = 0;

  ptmpstream = dec_input.stream;
  tmp = NextPacket((u8**)(&dec_input.stream),stream_stop, is_rv8);
  dec_input.strm_len = tmp;
  dec_input.stream_bus_address += (u32)(dec_input.stream-ptmpstream);

  dec_input.slice_info_num = 0;
  dec_input.slice_info = NULL;

  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_RV_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;
  /* Now keep getting packets until we receive an error */
  do {
    dec_input.pic_id = pics_decoded;
    START_SW_PERFORMANCE;
    rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
    END_SW_PERFORMANCE;
    if (rv_ret == DEC_HDRS_RDY) {
      rv_ret = VCDecGetInfo(dec_inst, &dec_info);
      pp_enabled = params.pp_enabled;
      if (params.ppu_cfg[0].enabled) {
        if (!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.scaled_width;
          params.ppu_cfg[0].crop.height = dec_info.scaled_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }
      }

      /* Create output sink after output file names are determined. */
      if (yuvsink == NULL) {
        outfile_info.bitstream_format = BITSTREAM_RM;
        outfile_info.pic_width = dec_info.pic_width;
        outfile_info.pic_height = dec_info.pic_height;
        outfile_info.bit_depth = (dec_info.bit_depth_luma == 8 && dec_info.bit_depth_chroma == 8) ? 8 : 10;
        outfile_info.is_interlaced = dec_info.is_interlaced;
        params.compress_bypass = TRUE; /* G1 don't support rfc */
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          return;
        }
      }

      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK)
        return;

      START_SW_PERFORMANCE;
      rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
      END_SW_PERFORMANCE;
      if (rv_ret == DEC_WAITING_FOR_BUFFER) {
        rv = VCDecGetBufferInfo(dec_inst, &hbuf);
        printf("[TB] VCDecGetBufferInfo ret %d\n", rv);
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
          buffer_size = hbuf.next_buf_size;
          struct DWLLinearMem mem;
		  DWLmemset(&mem, 0, sizeof(struct DWLLinearMem));
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
            rv = VCDecAddBuffer(dec_inst, &mem);
            printf("[TB] VCDecAddBuffer ret %d\n", rv);

            if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
              if (pp_enabled)
                DWLFreeLinear(dwl_inst, &mem);
              else
                DWLFreeRefFrm(dwl_inst, &mem);
            } else {
              if(i <= (hbuf.buf_num - 1))
                ext_buffers[i] = mem;
              else
                rpr_ext_buffers = mem;
            }
          }
          /* Extra buffers are allowed when minimum required buffers have been added.*/
          num_buffers = hbuf.buf_num;
          add_extra_flag = 1;
        }

        START_SW_PERFORMANCE;
        rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
        END_SW_PERFORMANCE;
      }
    }

    switch(rv_ret) {
    case DEC_OK:
    case DEC_PIC_DECODED:
      pics_decoded++;
#if 0
      if (pics_decoded == 10) {
        rv_ret = VCDecAbort(dec_inst);
        rv_ret = VCDecAbortAfter(dec_inst);
      }
#endif
      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, rv_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      break;

    case DEC_HDRS_RDY:
      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      rv_ret = VCDecGetInfo(dec_inst, &dec_info);
      END_SW_PERFORMANCE;
      if(rv_ret) {
        printf("[TB] VCDecGetInfo ret: %s\n", VCDecRetStr(rv_ret));
      }

      printf("[TB] *********************** VCDecGetInfo()/RAW ******************\n");
      printf("[TB] dec_info.pic_width %u\n",dec_info.pic_width);
      printf("[TB] dec_info.pic_height %u\n",dec_info.pic_height);
      printf("[TB] dec_info.scaled_width %u\n",dec_info.scaled_width);
      printf("[TB] dec_info.scaled_height %u\n",dec_info.scaled_height);
      if(dec_info.output_format == DEC_OUT_FRM_YUV420SP)
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420SP\n");
      else
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420TILE\n");
      printf("[TB] ******************* RAW MODE ********************************\n");

      pp_enabled = params.pp_enabled;
      if (params.ppu_cfg[0].enabled) {
        if (!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.scaled_width;
          params.ppu_cfg[0].crop.height = dec_info.scaled_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }
      }

      /* Create output sink after output file names are determined. */
      if (yuvsink == NULL) {
        outfile_info.bitstream_format = BITSTREAM_RM;
        outfile_info.pic_width = dec_info.pic_width;
        outfile_info.pic_height = dec_info.pic_height;
        outfile_info.bit_depth = (dec_info.bit_depth_luma == 8 && dec_info.bit_depth_chroma == 8) ? 8 : 10;
        outfile_info.is_interlaced = dec_info.is_interlaced;
        params.compress_bypass = TRUE; /* G1 don't support rfc */
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          return;
        }
      }

      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      u32 tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK)
        return;

      break;
    case DEC_WAITING_FOR_BUFFER:
      rv = VCDecGetBufferInfo(dec_inst, &hbuf);
      printf("[TB] VCDecGetBufferInfo ret %d\n", rv);
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
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;
		DWLmemset(&mem, 0, sizeof(struct DWLLinearMem));
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
          rv = VCDecAddBuffer(dec_inst, &mem);
          printf("[TB] VCDecAddBuffer ret %d\n", rv);

          if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            if(i <= (hbuf.buf_num - 1))
              ext_buffers[i] = mem;
            else
              rpr_ext_buffers = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = hbuf.buf_num;
        add_extra_flag = 1;
      }
      break;

    case DEC_NONREF_PIC_SKIPPED:
    case DEC_NO_DECODING_BUFFER:
      break;

    default:
      /*
      * DEC_NOT_INITIALIZED
      * DEC_PARAM_ERROR
      * DEC_STRM_ERROR
      * DEC_STRM_PROCESSED
      * DEC_SYSTEM_ERROR
      * DEC_MEMFAIL
      * DEC_HW_TIMEOUT
      * DEC_HW_RESERVED
      * DEC_HW_BUS_ERROR
      */
      DEBUG_PRINT(("[TB] Fatal Error, Decode end!\n"));
      break;
    }

    if (rv_ret != DEC_NO_DECODING_BUFFER) {
      ptmpstream = dec_input.stream;
      tmp = NextPacket((u8**)(&dec_input.stream),stream_stop, is_rv8);
      dec_input.strm_len = tmp;
      dec_input.stream_bus_address += (u32)(dec_input.stream-ptmpstream);
    }

    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
  } while(dec_input.strm_len > 0);

  VCDecEndOfStream(dec_inst);
  if(output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      if (config.table_3dlut_buffer.virtual_address) {
        DWLFreeLinear(dwl_inst, &config.table_3dlut_buffer);
        config.table_3dlut_buffer.virtual_address = NULL;
      }
    }
  }
  DWLFreeLinear(dwl_inst, &stream_mem);

  if(fp_out)
    DWLfclose(fp_out);
  if (pic_big_endian != NULL)
    free(pic_big_endian);

  FINALIZE_SW_PERFORMANCE;
}

void rv_display(const char *filename,
                char *filename_pp_cfg,
                int startframeid,
                int maxnumber,
                int iswritefile,
                int isdisplay,
                int displaymode,
                int rotationmode,
                int islog2std,
                int islog2file,
                int blayerenable,
                int errconceal,
                int randomerror) {
  HX_RESULT           ret_val         = HXR_OK;
  DWLFile*            fp_in           = HXNULL;
  INT32               l_bytes_read     = 0;
  UINT32              ul_num_streams   = 0;
  UINT32              i              = 0;
  UINT16              us_stream_num    = 0;
  UINT32              ul_out_frame_size = 0;
  rm_parser*          p_parser        = HXNULL;
  rm_stream_header*   p_hdr           = HXNULL;
  rm_packet*          p_packet        = HXNULL;
  rv_depack*          p_depack        = HXNULL;
  rv_decode*          p_decode        = HXNULL;
  rv_format_info*     p_info          = HXNULL;
  BYTE                uc_buf[RM2YUV_INITIAL_READ_SIZE];
  char                rv_outfilename[256];

#ifdef RV_TIME_TRACE
  UINT32              ul_start_time = TIMER_GET_NOW_TIME();
  UINT32              ul_stop_time = 0;
#endif

  INIT_SW_PERFORMANCE;

  enum DecRet rv_ret;
  /*RVDecInstInit RVInstInit;*/
  struct DecSequenceInfo dec_info;
  DWLmemset(&dec_info, 0, sizeof(struct DecSequenceInfo));

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
#endif

#ifdef ASIC_TRACE_SUPPORT
  if (!OpenAsicTraceFiles())
    DEBUG_PRINT(("[TB] UNABLE TO OPEN TRACE FILE(S)\n"));
#endif

  /* Initialize all static global variables */
  dec_inst = NULL;
  g_iswritefile = iswritefile;
  g_isdisplay = isdisplay;
  g_displaymode = displaymode;
  g_rotationmode = rotationmode;
  g_bfirstdisplay = 0;
  g_blayerenable = blayerenable;

#ifdef RV_ERROR_SIM
  g_randomerror = randomerror;
#endif


  RV_Time_Init(islog2std, islog2file);

  DWLmemset(rv_outfilename, 0, sizeof(rv_outfilename));

  /* NULL out the info */
  DWLmemset(info.file_path, 0, sizeof(info.file_path));
  info.fp_out             = HXNULL;
  info.p_depack           = HXNULL;
  info.p_decode           = HXNULL;
  info.p_out_frame         = HXNULL;
  info.ul_width           = 0;
  info.ul_height          = 0;
  info.ul_out_frame_size    = 0;
  info.ul_num_input_frames  = 0;
  info.ul_num_output_frames = 0;
  info.ul_max_num_dec_frames = maxnumber;
  info.ul_start_frame_id    = startframeid;
  info.b_dec_end           = 0;
  info.ul_total_time       = 0;

  /* Open the input file */
#ifdef WIN32
  fp_in = DWLfopen((const char*) filename, "rb");
#else
  fp_in = DWLfopen((const char*) filename, "r");
#endif

  DEBUG_PRINT(("[TB] filename=%s\n",filename));
  if (!fp_in) {
    ERROR_PRINT(("[TB] Could not open %s for reading.\n", filename));
    goto cleanup;
  }

  /* Read the first few bytes of the file */
  l_bytes_read = (INT32) DWLfread((void*) uc_buf, 1, RM2YUV_INITIAL_READ_SIZE, fp_in);
  if (l_bytes_read != RM2YUV_INITIAL_READ_SIZE) {
    ERROR_PRINT(("[TB] Could not read %d bytes at the beginning of %s\n",
                 RM2YUV_INITIAL_READ_SIZE, filename));
    goto cleanup;
  }
  /* Seek back to the beginning */
  DWLfseek(fp_in, 0, DWL_SEEK_SET);

  /* Make sure this is an .rm file */
  if (!rm_parser_is_rm_file(uc_buf, RM2YUV_INITIAL_READ_SIZE)) {
    ERROR_PRINT(("[TB] %s is not an .rm file.\n", filename));
    raw_mode(fp_in,
             filename_pp_cfg,
             startframeid,
             maxnumber,
             iswritefile,
             isdisplay,
             displaymode,
             rotationmode,
             islog2std,
             islog2file,
             blayerenable,
             errconceal,
             randomerror);
    goto cleanup;
  }

  is_rm_file = 1;
  /* Create the parser struct */
  p_parser = rm_parser_create2(HXNULL,
                               rm2yuv_error,
                               HXNULL,
                               rm_memory_malloc,
                               rm_memory_free);
  if (!p_parser) {
    goto cleanup;
  }

  /* Set the file into the parser */
  ret_val = rm_parser_init_io(p_parser,
                              (void *)fp_in,
                              (rm_read_func_ptr)rm_io_read,
                              (rm_seek_func_ptr)rm_io_seek);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Read all the headers at the beginning of the .rm file */
  ret_val = rm_parser_read_headers(p_parser);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Get the number of streams */
  ul_num_streams = rm_parser_get_num_streams(p_parser);
  if (ul_num_streams == 0) {
    ERROR_PRINT(("[TB] Error: rm_parser_get_num_streams() returns 0\n"));
    goto cleanup;
  }

  /* Now loop through the stream headers and find the video */
  for (i = 0; i < ul_num_streams && ret_val == HXR_OK; i++) {
    ret_val = rm_parser_get_stream_header(p_parser, i, &p_hdr);
    if (ret_val == HXR_OK) {
      if (rm_stream_is_realvideo(p_hdr)) {
        us_stream_num = (UINT16) i;
        break;
      } else {
        /* Destroy the stream header */
        rm_parser_destroy_stream_header(p_parser, &p_hdr);
      }
    }
  }

  /* Do we have a RealVideo stream in this .rm file? */
  if (!p_hdr) {
    ERROR_PRINT(("[TB] There is no RealVideo stream in this file.\n"));
    goto cleanup;
  }

  /* Create the RealVideo depacketizer */
  p_depack = rv_depack_create2_ex((void*) &info,
                                  rv_frame_available,
                                  HXNULL,
                                  rm2yuv_error,
                                  HXNULL,
                                  rm_memory_malloc,
                                  rm_memory_free,
                                  rm_memory_create_ncnb,
                                  rm_memory_destroy_ncnb);
  if (!p_depack) {
    goto cleanup;
  }

  /* Assign the rv_depack pointer to the info struct */
  info.p_depack = p_depack;

  /* Initialize the RV depacketizer with the RealVideo stream header */
  ret_val = rv_depack_init(p_depack, p_hdr);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Get the bitstream header information, create rv_infor_format struct and init it */
  ret_val = rv_depack_get_codec_init_info(p_depack, &p_info);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /*
  * Print out the width and height so the user
  * can input this into their YUV-viewing program.
  */
  //DEBUG_PRINT(("[TB] Video in %s has dimensions %lu x %lu pixels (width x height)\n",
  //  (const char*) filename, info.ul_width, info.ul_height));

  /*
  * Get the frames per second. This value is in 32-bit
  * fixed point, where the upper 16 bits is the integer
  * part of the fps, and the lower 16 bits is the fractional
  * part. We're going to truncate to integer, so all
  * we need is the upper 16 bits.
  */

  /* Create an rv_decode object */
  p_decode = rv_decode_create2(HXNULL,
                               rm2yuv_error,
                               HXNULL,
                               rm_memory_malloc,
                               rm_memory_free);
  if (!p_decode) {
    goto cleanup;
  }

  /* Assign the decode object into the rm2yuv_info struct */
  info.p_decode = p_decode;

  /* Init the rv_decode object */
  ret_val = rv_decode_init(p_decode, p_info);
  if (ret_val != HXR_OK) {
    goto cleanup;
  }

  /* Get the size of an output frame */
  ul_out_frame_size = rv_decode_max_output_size(p_decode);
  if (!ul_out_frame_size) {
    goto cleanup;
  }
  info.ul_out_frame_size = ul_out_frame_size;

  DEBUG_PRINT(("[TB] Start decode ...\n"));

  /* Init RV decode instance */
  /*RVInstInit.isrv8 = p_decode->bIsRV8;*/
  /*RVInstInit.pctszSize = p_decode->ulPctszSize;*/
  /*RVInstInit.pSizes = p_decode->pDimensions;*/
  /*RVInstInit.maxwidth = p_decode->ulLargestPels;*/
  /*RVInstInit.maxheight = p_decode->ulLargestLines;*/

  max_coded_width = p_decode->ulLargestPels;
  max_coded_height = p_decode->ulLargestLines;
  max_frame_width = MB_MULTIPLE( max_coded_width );
  max_frame_height = MB_MULTIPLE( max_coded_height );

  struct DWLInitParam dwl_init;
  dwl_init.client_type = DWL_CLIENT_TYPE_RV_DEC;
  dwl_init.dec_dev = params.dec_dev;
  dwl_init.mem_dev = params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_init.dma_dev = params.dma_dev;
#endif
  tool_params.dwl_inst = dwl_inst = DWLInit(&dwl_init);

  if(dwl_inst == NULL) {
    fprintf(stdout, ("[TB] ERROR: DWL Init failed"));
    goto cleanup;
  }
  struct DecInitConfig init_config;
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));
  init_config.codec = DEC_RV;
  init_config.error_handling = params.error_handling;
  init_config.error_ratio = params.error_ratio;
  init_config.frame_code_length = p_decode->ulPctszSize;
  init_config.frame_sizes = (u32*)p_decode->pDimensions;
  init_config.rv_version = !p_decode->bIsRV8;
  init_config.max_frame_width = p_decode->ulLargestPels;
  init_config.max_frame_height = p_decode->ulLargestLines;
  init_config.num_frame_buffers = num_frame_buffers;
  init_config.use_adaptive_buffers = 1;
  init_config.guard_size = 0;
  init_config.dwl_inst = dwl_inst;
  START_SW_PERFORMANCE;
  rv_ret = VCDecInit((const void**)&dec_inst, &init_config);
  END_SW_PERFORMANCE;
  if (rv_ret != DEC_OK) {
    ERROR_PRINT(("[TB] VCDecInit fail! \n"));
    goto cleanup;
  }

  /* Read what kind of stream is coming */
  START_SW_PERFORMANCE;
  rv_ret = VCDecGetInfo(dec_inst, &dec_info);
  END_SW_PERFORMANCE;
  if(rv_ret) {
    printf("[TB] VCDecGetInfo ret: %s\n", VCDecRetStr(rv_ret));
  }

  /* Fill in the width and height */
  info.ul_width  = (p_decode->ulLargestPels+15)&0xFFFFFFF0;
  info.ul_height = (p_decode->ulLargestLines+15)&0xFFFFFFF0;

  /* Parse the output file path */
  if(g_iswritefile) {
    parse_path(filename, info.file_path, NULL);
  }

#ifdef RV_DISPLAY
  if (g_displaymode) {
    Panel_GetSize(&g_dstimg);
  } else {
    g_dstimg.cx = 0;
    g_dstimg.cy = 0;
  }
#endif

  RV_Time_Full_Reset();

  /* Now keep getting packets until we receive an error */
  while (ret_val == HXR_OK && !info.b_dec_end) {
    /* Get the next packet */
    ret_val = rm_parser_get_packet(p_parser, &p_packet);
    if (ret_val == HXR_OK) {
      /* Is this a video packet? */
      if (p_packet->usStream == us_stream_num) {
        /*
        * Put the packet into the depacketizer. When frames
        * are available, we will get a callback to
        * rv_frame_available().
        */
        ret_val = rv_depack_add_packet(p_depack, p_packet);
      }
      /* Destroy the packet */
      rm_parser_destroy_packet(p_parser, &p_packet);
    }

    if (max_num_pics && pics_decoded >= max_num_pics)
      break;
  }

  TIME_PRINT(("[TB] \n"));

  /* Output the last picture in decoded picture memory pool */
  START_SW_PERFORMANCE;

  VCDecEndOfStream(dec_inst);

  END_SW_PERFORMANCE;

  /* Display results */
  DEBUG_PRINT(("[TB] Video stream in '%s' decoding complete: %u input frames, %u output frames\n",
               (const char*) filename, info.ul_num_input_frames, info.ul_num_output_frames));

#ifdef RV_TIME_TRACE
  ul_stop_time = TIMER_GET_NOW_TIME();
  if (ul_stop_time <= ul_start_time)
    info.ul_total_time = (TIMER_MAX_COUNTER-ul_start_time+ul_stop_time)/TIMER_FACTOR_MSECOND;
  else
    info.ul_total_time = (ul_stop_time-ul_start_time)/TIMER_FACTOR_MSECOND;

  TIME_PRINT(("[TB] Video stream in '%s' decoding complete:\n", (const char*) filename));
  TIME_PRINT(("[TB]     width is %u, height is %u\n",info.ul_width, info.ul_height));
  TIME_PRINT(("[TB]     %u input frames, %u output frames\n",info.ul_num_input_frames, info.ul_num_output_frames));
  TIME_PRINT(("[TB]     total time is %u ms, avarage time is %u ms, fps is %6.2f\n",
              info.ul_total_time, info.ul_total_time/info.ul_num_output_frames, 1000.0/(info.ul_total_time/info.ul_num_output_frames)));
#endif

  /* If the error was just a normal "out-of-packets" error,
  then clean it up */
  if (ret_val == HXR_NO_DATA) {
    ret_val = HXR_OK;
  }

cleanup:

  /* Destroy the codec init info */
  add_buffer_thread_run = 0;

  if(output_thread_run) {
    pthread_join(output_thread, NULL);
    pthread_join(release_thread, NULL);
  }

  if (p_info) {
    rv_depack_destroy_codec_init_info(p_depack, &p_info);
  }
  /* Destroy the depacketizer */
  if (p_depack) {
    rv_depack_destroy(&p_depack);
  }
  /* If we have a packet, destroy it */
  if (p_packet) {
    rm_parser_destroy_packet(p_parser, &p_packet);
  }
  /* Destroy the stream header */
  if (p_hdr) {
    rm_parser_destroy_stream_header(p_parser, &p_hdr);
  }
  /* Destroy the rv_decode object */
  if (p_decode) {
    rv_decode_destroy(p_decode);
    p_decode = HXNULL;
  }
  /* Destroy the parser */
  if (p_parser) {
    rm_parser_destroy(&p_parser);
  }
  /* Close the input file */
  if (fp_in) {
    DWLfclose(fp_in);
    fp_in = HXNULL;
  }

  /* Close the output file */
  if (info.fp_out) {
    DWLfclose(info.fp_out);
    info.fp_out = HXNULL;
  }

  /* Close the time log file */
  if (g_fp_time_log) {
    DWLfclose(g_fp_time_log);
    g_fp_time_log = HXNULL;
  }

  PrintOutputFileName(&params);
  FreeOutputFileName(&params);
  if (yuvsink) ReleaseYuvSink(yuvsink);

  /* Release the decoder instance */
  if (dec_inst) {
    START_SW_PERFORMANCE;
    ReleaseExtBuffers();
    pthread_mutex_destroy(&ext_buffer_contro);
    VCDecRelease(dec_inst);
    DWLRelease(dwl_inst);
    VCDecLogDestory();
    END_SW_PERFORMANCE;
  }

  FINALIZE_SW_PERFORMANCE;

#ifdef ASIC_TRACE_SUPPORT
  CloseAsicTraceFiles();
#endif
}

/*------------------------------------------------------------------------------
function
    rv_frame_available()

purpose
    If enough data have been extracted from rm packte(s) for an entire frame,
    this function will be called to decode the available stream data.
------------------------------------------------------------------------------*/

HX_RESULT rv_frame_available(void* p_avail, UINT32 ul_sub_stream_num, rv_frame* p_frame) {
  HX_RESULT ret_val = HXR_FAIL;
  struct DecPictures dec_output;
  DWLmemset(&dec_output, 0, sizeof(struct DecPictures));

  if (p_avail && p_frame) {
    /* Get the info pointer */
    rm2yuv_info* p_info = (rm2yuv_info*) p_avail;
    if (p_info->p_depack && p_info->p_decode) {
      /* Put the frame into rv_decode */
      ret_val = rv_decode_stream_input(p_info->p_decode, p_frame);
      if (HX_SUCCEEDED(ret_val)) {
        /* Increment the number of input frames */
        p_info->ul_num_input_frames++;

        /* Decode frames until there aren't any more */
        do {
          /* skip all B-Frames before Frame g_startframeid */
          if (p_info->p_decode->pInputFrame->usSequenceNum < p_info->ul_start_frame_id
              && p_info->p_decode->pInputFrame->ucCodType == 3/*RVDEC_TRUEBPIC*/) {
            ret_val = HXR_OK;
            break;
          }
          ret_val = rv_decode_stream_decode(p_info->p_decode, &dec_output);
          RV_Time_Full_Pause();
          RV_Time_Full_Reset();

          if (HX_SUCCEEDED(ret_val)) {
          } else {
            p_info->b_dec_end = 1;
          }
        } while (HX_SUCCEEDED(ret_val) && rv_decode_more_frames(p_info->p_decode));
      }

      rv_depack_destroy_frame(p_info->p_depack, &p_frame);
    }
  }

  return ret_val;
}


/*------------------------------------------------------------------------------
function
    rv_decode_stream_decode()

purpose
    Calls the decoder backend (HW) to decode issued frame
    and output a decoded frame if any possible.

return
    Returns zero on success, negative result indicates failure.

------------------------------------------------------------------------------*/
HX_RESULT rv_decode_stream_decode(rv_decode *p_front_end, struct DecPictures *output) {
  HX_RESULT ret_val = HXR_FAIL;
  enum DecRet rv_ret;
  struct DecInputParameters dec_input;
  struct DecSequenceInfo dec_info;
  struct DecOutput dec_output;
  //u32 prev_width = 0, prev_height = 0;
  //u32 min_buffer_num = 0;
  struct DecBufferInfo hbuf;
  enum DecRet rv;
  u32 more_data = 1;
  UINT32 size = 0;
  UINT32 i = 0;
  struct DecSliceInfo slice_info[128];
  struct DecInitConfig init_config;
  DWLmemset(&dec_input, 0, sizeof(struct DecInputParameters));
  DWLmemset(&dec_info, 0, sizeof(struct DecSequenceInfo));
  DWLmemset(&hbuf, 0, sizeof(struct DecBufferInfo));
  DWLmemset(&init_config, 0, sizeof(struct DecInitConfig));
  struct DecConfig config;
  DWLmemset(&config, 0, sizeof(struct DecConfig));

  INIT_SW_PERFORMANCE;

#ifdef RV_ERROR_SIM
  DWLFile *fp = NULL;
  char flnm[0x100] = "";

  if (g_randomerror) {
    CreateError((char *)(p_front_end->pInputFrame->pData + 100), (int)(p_front_end->pInputParams.dataLength), 20);
    sprintf(flnm, "stream_%u.bin", p_front_end->pInputFrame->usSequenceNum);
    fp = DWLfopen(flnm, "w");
    if(fp) {
      DWLfwrite(p_front_end->pInputFrame->pData, 1, p_front_end->pInputParams.dataLength, fp);
      DWLfclose(fp);
    }
  }
#endif

  dec_input.skip_frame = skip_frame;
  dec_input.stream = p_front_end->pInputFrame->pData;
  dec_input.stream_bus_address = p_front_end->pInputFrame->ulDataBusAddress;
  dec_input.pic_id = p_front_end->pInputFrame->usSequenceNum;
  dec_input.timestamp = p_front_end->pInputFrame->ulTimestamp;
  dec_input.strm_len = p_front_end->pInputParams.dataLength;
  dec_input.slice_info_num = p_front_end->pInputParams.numDataSegments+1;
  /* Configure slice_info, which will be accessed by HW */
  size = sizeof(slice_info);
  dec_input.slice_info = slice_info;
  DWLmemset(dec_input.slice_info, 0, size);
  for (i = 0; i < dec_input.slice_info_num; i++) {
    slice_info[i].offset = p_front_end->pInputParams.pDataSegments[i].ulOffset;
    slice_info[i].is_valid = p_front_end->pInputParams.pDataSegments[i].bIsValid;
  }
  const struct DecHwFeatures *hw_feature = NULL;
  u32 core_mask = 0;
  hw_feature = DWLGetHwFeaturesByClientType(dwl_inst, DWL_CLIENT_TYPE_RV_DEC, &core_mask);
  params.crop_align = hw_feature->crop_step_rshift;
  /* for color remapping (3dlut) */
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      u32 size = 18 * 18 * 18 * 3 * sizeof(u16);
      // ppu_int_cfg.table_3dlut.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      config.table_3dlut_buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                        DWL_MEM_TYPE_CPU;
      if (DWLMallocLinear(dwl_inst, size, &config.table_3dlut_buffer)) {
        return ret_val;
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
  do {
    more_data = 0;

    //printf("[TB] \n%d\n",dec_input.timestamp);
    //printf("[TB] \n%d\n",dec_input.pic_id);

    RV_Time_Dec_Reset();
    START_SW_PERFORMANCE;
    rv_ret = VCDecDecode(dec_inst, &dec_output, &dec_input);
    END_SW_PERFORMANCE;
    RV_Time_Dec_Pause(0,0);

    if (rv != DEC_NO_DECODING_BUFFER)
      printf("[TB] VCDecDecode ret: %s\n", VCDecRetStr(rv_ret));
    p_front_end->pOutputParams.notes &= ~RV_DECODE_MORE_FRAMES;

    switch(rv_ret) {
    case DEC_HDRS_RDY:
      more_data = 1;

      /* Read what kind of stream is coming */
      START_SW_PERFORMANCE;
      rv_ret = VCDecGetInfo(dec_inst, &dec_info);
      END_SW_PERFORMANCE;
      if(rv_ret) {
        printf("[TB] VCDecGetInfo ret: %s\n", VCDecRetStr(rv_ret));
      }

      pp_enabled = params.pp_enabled;
      if (params.ppu_cfg[0].enabled) {
        if (!params.ppu_cfg[0].crop.set_by_user) {
          params.ppu_cfg[0].crop.width = dec_info.scaled_width;
          params.ppu_cfg[0].crop.height = dec_info.scaled_height;
          params.ppu_cfg[0].crop.enabled = 1;
        }
      }

      /* Create output sink after output file names are determined. */
      if (yuvsink == NULL) {
        outfile_info.bitstream_format = BITSTREAM_RM;
        outfile_info.pic_width = dec_info.pic_width;
        outfile_info.pic_height = dec_info.pic_height;
        outfile_info.bit_depth = (dec_info.bit_depth_luma == 8 && dec_info.bit_depth_chroma == 8) ? 8 : 10;
        outfile_info.is_interlaced = dec_info.is_interlaced;
        params.compress_bypass = TRUE; /* G1 don't support rfc */
        GenerateOutputFileName(&params, &outfile_info);
        if ((yuvsink = CreateYuvSink(&params)) == NULL) {
          fprintf(stderr, "[TB] Failed to create YUV sink\n");
          ret_val = HXR_FAIL;
          return ret_val;
        }
      }

      config.align = align;
      memcpy(config.ppu_cfg, params.ppu_cfg, sizeof(params.ppu_cfg));
      u32 tmp = VCDecSetInfo(dec_inst, &config);
      if (tmp != DEC_OK)
        return ret_val;
      printf("[TB] *********************** VCDecGetInfo() **********************\n");
      printf("[TB] dec_info.pic_width %u\n",dec_info.pic_width);
      printf("[TB] dec_info.pic_height %u\n",dec_info.pic_height);
      printf("[TB] dec_info.scaled_width %u\n",dec_info.scaled_width);
      printf("[TB] dec_info.scaled_height %u\n",dec_info.scaled_height);
      if(dec_info.output_format == DEC_OUT_FRM_YUV420SP)
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420SP\n");
      else
        printf("[TB] dec_info.output_format DEC_OUT_FRM_YUV420TILE\n");
      printf("[TB] *************************************************************\n");

      break;
    case DEC_WAITING_FOR_BUFFER:
      more_data = 1;
      rv = VCDecGetBufferInfo(dec_inst, &hbuf);
      printf("[TB] VCDecGetBufferInfo ret %d\n", rv);
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
        buffer_size = hbuf.next_buf_size;
        struct DWLLinearMem mem;
		DWLmemset(&mem, 0, sizeof(struct DWLLinearMem));
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
          rv = VCDecAddBuffer(dec_inst, &mem);
          printf("[TB] VCDecAddBuffer ret %d\n", rv);

          if(rv != DEC_OK && rv != DEC_WAITING_FOR_BUFFER) {
            if (pp_enabled)
              DWLFreeLinear(dwl_inst, &mem);
            else
              DWLFreeRefFrm(dwl_inst, &mem);
          } else {
            if(i <= (hbuf.buf_num - 1))
              ext_buffers[i] = mem;
            else
              rpr_ext_buffers = mem;
          }
        }
        /* Extra buffers are allowed when minimum required buffers have been added.*/
        num_buffers = hbuf.buf_num;
        add_extra_flag = 1;
      }
      break;

    case DEC_STRM_PROCESSED:
      //if (dec_input.strm_len == dec_output.data_left)
      //  more_data = 1;
    case DEC_DISCARD_INTERNAL:
    case DEC_BUF_EMPTY:
    case DEC_NONREF_PIC_SKIPPED:
    case DEC_NO_DECODING_BUFFER:
      /* In case picture size changes we need to reinitialize memory model */
      START_SW_PERFORMANCE;
      VCDecGetInfo(dec_inst, &dec_info);
      rv_ret = dec_info.out_pic_stat;
      END_SW_PERFORMANCE;
      if (rv_ret == DEC_PIC_RDY) {
        p_front_end->pOutputParams.width = dec_info.out_pic_coded_width;
        p_front_end->pOutputParams.height = dec_info.out_pic_coded_height;
        p_front_end->pOutputParams.notes &= ~RV_DECODE_DONT_DRAW;
        p_front_end->pOutputParams.notes |= RV_DECODE_MORE_FRAMES;
        ret_val = HXR_OK;

        /* update frame number */
        frame_number++;
      } else if (rv_ret == DEC_OK) {
        p_front_end->pOutputParams.notes |= RV_DECODE_DONT_DRAW;
        ret_val = HXR_OK;
      } else {
        ret_val = HXR_FAIL;
      }

      break;

    case DEC_OK:
    case DEC_PIC_DECODED:
      pics_decoded++;
#if 0
      if (pics_decoded == 10) {
        rv_ret = VCDecAbort(dec_inst);
        rv_ret = VCDecAbortAfter(dec_inst);
      }
#endif

      if (!output_thread_run) {
        output_thread_run = 1;
        sem_init(&buf_release_sem, 0, 0);
        pthread_create(&output_thread, NULL, rv_output_thread, NULL);
        pthread_create(&release_thread, NULL, buf_release_thread, NULL);
      }

      if(use_extra_buffers && !add_buffer_thread_run) {
        add_buffer_thread_run = 1;
        pthread_create(&add_buffer_thread, NULL, AddBufferThread, NULL);
      }
      if (use_peek_output) {
        u32 only_once = 0, ext_id = 0xFFFF;
        rv_ret = VCDecPeek(dec_inst, output);
        if(rv_ret == DEC_PIC_RDY) {
          if(!is_rm_file) {
            for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
              if (!output->pictures[i].sequence_info.pic_width ||
                  !output->pictures[i].sequence_info.pic_height)
                continue;
              if (only_once == 0) {
                SwMatchOuputBufferId(&tool_params, &output->pictures[i].luma, &ext_id);
                if (ext_id < MAX_BUFFERS)
                  DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                                  tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
                only_once = 1;
              }
              yuvsink->WritePicture(yuvsink->inst, &output->pictures[i], i);
            }
          } else rv_decode_frame_flush(&info, output);

          //DEBUG_PRINT(("[TB] Get frame %d.\n", p_info->ul_num_output_frames));
          DEBUG_PRINT(("[TB] \nPIC %2u/%2u, type %s,", pic_display_number + 1,
                     output->pictures[0].picture_info.decode_id + 1,
                     output->pictures[0].picture_info.is_intra_frame ? "key picture    " : "non key picture"));
          /* pic coding type */
          printRvPicCodingType(output->pictures[0].picture_info.pic_coding_type);
          /* printf : cycles per mbs */
          if (output->pictures[0].picture_info.cycles_per_mb) {
            cycle_count += output->pictures[0].picture_info.cycles_per_mb;
            DEBUG_PRINT(("[TB]  %4u cycles / mb,", output->pictures[0].picture_info.cycles_per_mb));
          }
          DEBUG_PRINT(("[TB]  %u x %u, Crop: (%u, %u), %u x %u\n",
                     output->pictures[0].sequence_info.scaled_width,
                     output->pictures[0].sequence_info.scaled_height,
                     output->pictures[0].sequence_info.crop_params.crop_left_offset,
                     output->pictures[0].sequence_info.crop_params.crop_top_offset,
                     output->pictures[0].sequence_info.crop_params.crop_out_width,
                     output->pictures[0].sequence_info.crop_params.crop_out_height));
          /* Push output buffer into buf_list and wait to be consumed */
          buf_list[list_push_index] = output[0];
          buf_status[list_push_index] = 1;
          list_push_index++;
          if(list_push_index == 100)
            list_push_index = 0;

          sem_post(&buf_release_sem);

          pic_display_number++;
        }
      } else {
        START_SW_PERFORMANCE;
        VCDecGetInfo(dec_inst, &dec_info);
        rv_ret = dec_info.out_pic_stat;
        END_SW_PERFORMANCE;
      }

      if (rv_ret == DEC_PIC_RDY) {
        p_front_end->pOutputParams.width = dec_info.out_pic_coded_width;
        p_front_end->pOutputParams.height = dec_info.out_pic_coded_height;
        p_front_end->pOutputParams.notes &= ~RV_DECODE_DONT_DRAW;
        ret_val = HXR_OK;

        /* pic coding type */
        //printRvPicCodingType(output->pictures[0].picture_info.pic_coding_type);

        /* update frame number */
        frame_number++;
      } else if (rv_ret == DEC_OK) {
        p_front_end->pOutputParams.notes |= RV_DECODE_DONT_DRAW;
        ret_val = HXR_OK;
      } else {
        DEBUG_PRINT(("[TB] Error, Decode end!\n"));
        ret_val = HXR_FAIL;
      }

      pic_rdy = 1;
      corrupted_bytes = 0;
      break;
    default:
      /*
      * DEC_NOT_INITIALIZED
      * DEC_PARAM_ERROR
      * DEC_STRM_ERROR
      * DEC_STRM_PROCESSED
      * DEC_SYSTEM_ERROR
      * DEC_MEMFAIL
      * DEC_HW_TIMEOUT
      * DEC_HW_RESERVED
      * DEC_HW_BUS_ERROR
      */
      DEBUG_PRINT(("[TB] Fatal Error, Decode end!\n"));
      ret_val = HXR_FAIL;
      break;
    }
  } while(more_data);

  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!params.ppu_cfg[i].enabled) continue;
    if (params.ppu_cfg[i].enable_3dlut == 1) {
      if (config.table_3dlut_buffer.virtual_address) {
        DWLFreeLinear(dwl_inst, &config.table_3dlut_buffer);
        config.table_3dlut_buffer.virtual_address = NULL;
      }
    }
  }

  FINALIZE_SW_PERFORMANCE;
  return ret_val;
}

/*------------------------------------------------------------------------------
function
    rv_decode_frame_flush()

purpose
    update rm2yuv_info
    write the decoded picture to a local file or display on screen, if neccesary
------------------------------------------------------------------------------*/
void rv_decode_frame_flush(rm2yuv_info *p_info, struct DecPictures *output) {
  UINT32 ul_width, ul_height, ul_out_frame_size;
  u32 i;
  if (p_info == NULL || output == NULL)
    return;

  /* Is there a valid output frame? */
  {
    /* Increment the number of output frames */
    p_info->ul_num_output_frames++;
    /* Get the dimensions and size of output frame */
    ul_width = output->pictures[0].sequence_info.scaled_width;
    ul_height = output->pictures[0].sequence_info.scaled_height;
    ul_out_frame_size = (ul_width * ul_height * 12) >> 3;
    /* Check to see if dimensions have changed */
    if (ul_width != p_info->ul_width || ul_height != p_info->ul_height ) {
      DEBUG_PRINT(("[TB] Warning: YUV output dimensions changed from "
                   "%ux%u to %ux%u at frame #: %u (in decoding order)\n",
                   p_info->ul_width, p_info->ul_height, ul_width, ul_height,
                   output->pictures[0].picture_info.pic_id));
      /* Don't bother resizing to display dimensions */
      p_info->ul_width = ul_width;
      p_info->ul_height = ul_height;
      p_info->ul_out_frame_size = ul_out_frame_size;
      /* close the opened output file handle if necessary */
      /*
      if (p_info->fp_out)
      {
          DWLfclose(p_info->fp_out);
          p_info->fp_out = HXNULL;
      }
      */
      /* clear the flag of first display */
      g_bfirstdisplay = 0;
    }

    u32 only_once = 0, ext_id = 0xFFFF;
    if(g_iswritefile) {
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
          if (!output->pictures[i].sequence_info.pic_width ||
            !output->pictures[i].sequence_info.pic_height)
            continue;
          if (only_once == 0) {
            SwMatchOuputBufferId(&tool_params, &output->pictures[i].luma, &ext_id);
            if (ext_id < MAX_BUFFERS)
              DWLDMATransData(tool_params.dwl_inst, &tool_params.ext_buffers[ext_id], 0,
                              tool_params.ext_buffers[ext_id].size, DEVICE_TO_HOST);
            only_once = 1;
          }
        yuvsink->WritePicture(yuvsink->inst, &output->pictures[i], i);
      }
    }
#ifdef RV_DISPLAY
    g_srcimg.cx = output->pictures[0].frame_width;  // 16-pixel up rounding
    g_srcimg.cy = output->pictures[0].frame_height; // 16-pixel up rounding
    if(g_isdisplay) {
      displayOnScreen(output);
    }
#endif

    if (p_info->ul_max_num_dec_frames && (p_info->ul_num_output_frames >= p_info->ul_max_num_dec_frames)) {
      p_info->b_dec_end = 1;
    }

  }
}

/*------------------------------------------------------------------------------
function
    rm2yuv_error

purpose
    print error message
------------------------------------------------------------------------------*/
void rm2yuv_error(void* p_error, HX_RESULT result, const char* psz_msg) {
  UNUSED(p_error);
  UNUSED(result);
  UNUSED(psz_msg);
  ERROR_PRINT(("[TB] rm2yuv_error p_error=%p result=0x%08x msg=%s\n", p_error, result, psz_msg));
}

/*------------------------------------------------------------------------------

    Function name:            printRvPicCodingType

    Functional description:   Print out picture coding type value

------------------------------------------------------------------------------*/
static void printRvPicCodingType(u32 pic_type) {
  switch (pic_type) {
  case DEC_PIC_TYPE_I:
    printf("  DEC_PIC_TYPE_I,");
    break;
  case DEC_PIC_TYPE_P:
    printf("  DEC_PIC_TYPE_P,");
    break;
  case DEC_PIC_TYPE_B:
    printf("  DEC_PIC_TYPE_B,");
    break;
  case DEC_PIC_TYPE_FI:
    printf("  DEC_PIC_TYPE_FI,");
    break;
  default:
    printf("  Other %u,", pic_type);
    break;
  }
}

/*------------------------------------------------------------------------------
function
    parse_path

purpose
    parse filename and path

parameter
    path(I):    full path of input file
    dir(O):     directory path
    file(O):    file name
-----------------------------------------------------------------------------*/
void parse_path(const char *path, char *dir, char *file) {
  int len = 0;
  const char *ptr = NULL;

  if (path == NULL)
    return;

  len = strlen(path);
  if (len == 0)
    return;

  ptr = path + len - 1;    // point to the last char
  while (ptr != path) {
    if ((*ptr == '\\') || (*ptr == '/'))
      break;

    ptr--;
  }

  if (file != NULL) {
    if (ptr != path)
      strcpy(file, ptr + 1);
    else
      strcpy(file, ptr);
  }

  if ((ptr != path) && (dir != NULL)) {
    len = ptr - path + 1;    // with the "/" or "\"
    DWLmemcpy(dir, path, len);
    dir[len] = '\0';
  }
}

/*------------------------------------------------------------------------------
function
    rm_io_read

purpose
    io read interface for rm parse/depack
-----------------------------------------------------------------------------*/
UINT32 rm_io_read(void* p_user_read, BYTE* p_buf, UINT32 ul_bytes_to_read) {
  UINT32 ul_ret = 0;

  if (p_user_read && p_buf && ul_bytes_to_read) {
    /* The void* is a DWLFile* */
    DWLFile *fp = (DWLFile *) p_user_read;
    /* Read the number of bytes requested */
    ul_ret = (UINT32) DWLfread(p_buf, 1, ul_bytes_to_read, fp);
  }

  return ul_ret;
}

/*------------------------------------------------------------------------------
function
    rm_io_seek

purpose
    io seek interface for rm parse/depack
-----------------------------------------------------------------------------*/
void rm_io_seek(void* p_user_read, UINT32 ulOffset, UINT32 ul_origin) {
  if (p_user_read) {
    /* The void* is a DWLFile* */
    DWLFile *fp = (DWLFile *) p_user_read;
    /* Do the seek */
    DWLfseek(fp, ulOffset, ul_origin);
  }
}

/*------------------------------------------------------------------------------
function
    rm_memory_malloc

purpose
    memory (sw only) allocation interface for rm parse/depack
-----------------------------------------------------------------------------*/
void* rm_memory_malloc(void* p_user_mem, UINT32 ul_size) {
  UNUSED(p_user_mem);

  return (void*)DWLmalloc(ul_size);
}

/*------------------------------------------------------------------------------
function
    rm_memory_free

purpose
    memory (sw only) free interface for rm parse/depack
-----------------------------------------------------------------------------*/
void rm_memory_free(void* p_user_mem, void* ptr) {
  UNUSED(p_user_mem);

  DWLfree(ptr);
}

/*------------------------------------------------------------------------------
function
    rm_memory_create_ncnb

purpose
    memory (sw/hw share) allocation interface for rm parse/depack
-----------------------------------------------------------------------------*/
void* rm_memory_create_ncnb(void* p_user_mem, UINT32 ul_size) {
  struct DWLLinearMem *p_buf = NULL;
  const void *dwl = dwl_inst;

  UNUSED(p_user_mem);

  ASSERT(dwl != NULL);

  p_buf = (struct DWLLinearMem *)DWLmalloc(sizeof(struct DWLLinearMem));
  if (p_buf == NULL)
    return NULL;

#ifdef SUPPORT_DMA
  p_buf->mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
#endif
  SET_MEM_USAGE(p_buf->mem_type, DWL_MEM_USAGE_OUT_RVRM_NCNB, secure_mode);
  if (DWLMallocLinear(dwl, ul_size, p_buf)) {
    DWLfree(p_buf);
    return NULL;
  } else {
    return p_buf;
  }
}

/*------------------------------------------------------------------------------
function
    rm_memory_destroy_ncnb

purpose
    memory (sw/hw share) release interface for rm parse/depack
-----------------------------------------------------------------------------*/
void rm_memory_destroy_ncnb(void* p_user_mem, void* ptr) {
  struct DWLLinearMem *p_buf = NULL;
  const void *dwl = dwl_inst;

  UNUSED(p_user_mem);

  ASSERT(dwl != NULL);

  if (ptr == NULL)
    return;

  p_buf = *(struct DWLLinearMem **)ptr;

  if (p_buf == NULL)
    return;

  DWLFreeLinear(dwl, p_buf);
  DWLfree(p_buf);
  *(struct DWLLinearMem **)ptr = NULL;
}

/*------------------------------------------------------------------------------
Function name:  RvDecTrace()

Functional description:
    This implementation appends trace messages to file named 'dec_api.trc'.

Argument:
    string - trace message, a null terminated string
------------------------------------------------------------------------------*/

/*void RvDecTrace(const char *str) {
  DWLFile *fp = NULL;

  fp = DWLfopen("dec_api.trc", "a");
  if(!fp)
    return;

  DWLfwrite(str, 1, strlen(str), fp);
  DWLfwrite("\n", 1, 1, fp);

  DWLfclose(fp);
}
*/

#ifdef RV_DISPLAY
void displayOnScreen(struct DecPictures *p_dec_picture) {
  UNUSED(p_dec_picture);

#ifdef RV_TIME_TRACE
  ul_stop_time_Full = TIMER_GET_NOW_TIME();
  if (ul_stop_time_Full <= ul_start_time_Full)
    f_time_Full += (double)((TIMER_MAX_COUNTER-ul_start_time_Full+ul_stop_time_Full)/TIMER_FACTOR_MSECOND);
  else
    f_time_Full += (double)((ul_stop_time_Full-ul_start_time_Full)/TIMER_FACTOR_MSECOND);

  f_time_LCD = 0.0;
  ul_start_time_LCD = TIMER_GET_NOW_TIME();
#endif

  VIM_DISPLAY((g_srcimg, g_dstimg, (char *)p_dec_picture->pData, &g_bfirstdisplay,
               g_delay_time, g_rotationmode, g_blayerenable));

#ifdef RV_TIME_TRACE
  ul_stop_time_LCD = TIMER_GET_NOW_TIME();
  if (ul_stop_time_LCD <= ul_start_time_LCD)
    f_time_LCD += (double)((TIMER_MAX_COUNTER-ul_start_time_LCD+ul_stop_time_LCD)/TIMER_FACTOR_MSECOND);
  else
    f_time_LCD += (double)((ul_stop_time_LCD-ul_start_time_LCD)/TIMER_FACTOR_MSECOND);

  if(f_time_LCD > f_max_time_LCD)
    f_max_time_LCD = f_time_LCD;

  f_total_time_LCD += f_time_LCD;

  if (g_islog2std) {
    TIME_PRINT(("[TB]  | \tDisplay:  ID= %-6d  PicType= %-4d  time= %-6.0f ms\t", p_dec_picture->pic_id, 0, f_time_LCD));
  }
  if (g_islog2file && g_fp_time_log) {
    sprintf(str_time_info, " | \tDisplay:  ID= %-6d  PicType= %-4d  time= %-6.0f ms\t", p_dec_picture->pic_id, 0, f_time_LCD);
    DWLfwrite(str_time_info, 1, strlen(str_time_info), g_fp_time_log);
  }

  ul_start_time_Full = TIMER_GET_NOW_TIME();
#endif
}
#endif

void RV_Time_Init(i32 islog2std, i32 islog2file) {
#ifdef RV_TIME_TRACE
  g_islog2std = islog2std;
  g_islog2file = islog2file;

  if (g_islog2file) {
#ifdef WIN32
    g_fp_time_log = DWLfopen("rvtime.log", "wb");
#else
    g_fp_time_log = DWLfopen("rvtime.log", "w");
#endif
  }

  f_max_time_DEC = 0.0;
  f_max_time_LCD = 0.0;
  f_max_time_Full = 0.0;
  f_total_time_DEC = 0.0;
  f_total_time_LCD = 0.0;
  f_total_time_Full = 0.0;
#endif
}

void RV_Time_Full_Reset(void) {
#ifdef RV_TIME_TRACE
  f_time_Full = 0.0;
  ul_start_time_Full = TIMER_GET_NOW_TIME();
#endif
}

void RV_Time_Full_Pause(void) {
#ifdef RV_TIME_TRACE
  ul_stop_time_Full = TIMER_GET_NOW_TIME();
  if (ul_stop_time_Full <= ul_start_time_Full)
    f_time_Full += (double)((TIMER_MAX_COUNTER-ul_start_time_Full+ul_stop_time_Full)/TIMER_FACTOR_MSECOND);
  else
    f_time_Full += (double)((ul_stop_time_Full-ul_start_time_Full)/TIMER_FACTOR_MSECOND);

  if(f_time_Full > f_max_time_Full)
    f_max_time_Full = f_time_Full;

  f_total_time_Full += f_time_Full;

  if (g_islog2std) {
    TIME_PRINT(("[TB]  | \tFull_time= %-6.0f ms", f_time_Full));
  }
  if (g_islog2file && g_fp_time_log) {
    sprintf(str_time_info, " | \tFull_time= %-6.0f ms", f_time_Full);
    DWLfwrite(str_time_info, 1, strlen(str_time_info), g_fp_time_log);
  }
#endif
}

void RV_Time_Dec_Reset(void) {
#ifdef RV_TIME_TRACE
  f_time_DEC = 0.0;
  ul_start_time_DEC = TIMER_GET_NOW_TIME();
#endif
}

void RV_Time_Dec_Pause(u32 picid, u32 pictype) {
#ifdef RV_TIME_TRACE
  ul_stop_time_Full = TIMER_GET_NOW_TIME();
  if (ul_stop_time_Full <= ul_start_time_Full)
    f_time_Full += (double)((TIMER_MAX_COUNTER-ul_start_time_Full+ul_stop_time_Full)/TIMER_FACTOR_MSECOND);
  else
    f_time_Full += (double)((ul_stop_time_Full-ul_start_time_Full)/TIMER_FACTOR_MSECOND);

  ul_stop_time_DEC = TIMER_GET_NOW_TIME();
  if (ul_stop_time_DEC <= ul_start_time_DEC)
    f_time_DEC += (double)((TIMER_MAX_COUNTER-ul_start_time_DEC+ul_stop_time_DEC)/TIMER_FACTOR_MSECOND);
  else
    f_time_DEC += (double)((ul_stop_time_DEC-ul_start_time_DEC)/TIMER_FACTOR_MSECOND);

  if(f_time_DEC > f_max_time_DEC)
    f_max_time_DEC = f_time_DEC;

  f_total_time_DEC += f_time_DEC;

  if (g_islog2std) {
    TIME_PRINT(("[TB] \nDecode:  ID= %-6u  PicType= %-4u  time= %-6.0f ms\t", picid, pictype, f_time_DEC));
  }
  if (g_islog2file && g_fp_time_log) {
    sprintf(str_time_info, "\nDecode:  ID= %-6u  PicType= %-4u  time= %-6.0f ms\t", picid, pictype, f_time_DEC);
    DWLfwrite(str_time_info, 1, strlen(str_time_info), g_fp_time_log);
  }

  ul_start_time_Full = TIMER_GET_NOW_TIME();
#endif
}

int main(int argc, char *argv[]) {
  int startframeid = 0;
  int maxnumber = 1000000000;
  int iswritefile = 1;
  int isdisplay = 0;
  int displaymode = 0;
  int rotationmode = 0;
  int islog2std = 1;
  int islog2file = 0;
  int blayerenable = 0;
  int errconceal = 0;
  int randomerror = 0;
  FILE *f_tbcfg;
  int ret;
  memset(ext_buffers, 0, sizeof(ext_buffers));
  pthread_mutex_init(&ext_buffer_contro, NULL);

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
     		"[TB] Localtime() Failed.\n");
     return -1;
    }

    strftime(tm_buf, sizeof(tm_buf), "%y%m%d", tm);
    tmp1 = 1000000+atoi(tm_buf);
    if (tmp1 > (EXPIRY_DATE) && (EXPIRY_DATE) > 1 ) {
      fprintf(stderr,
              "[TB] EVALUATION PERIOD EXPIRED.\n"
              "Please contact On2 Sales.\n");
      return -1;
    }
  }

  SetupDefaultParams(&params);
  /* set test bench configuration */
#ifdef ASIC_TRACE_SUPPORT
  g_hw_ver = 10000; // G1
#endif

  /* Use common command line parser to parse options. */
  if ((ret = ParseParams(argc, argv, &params))) {
    if (ret == 1) {
      printf("[TB] Failed to parse params.\n\n");
      PrintUsage(argv[0], RVDEC);
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
  max_num_pics = params.num_of_decoded_pics;
  use_peek_output = params.disable_display_order;
  md5sum = (params.sink_type == SINK_MD5_PICTURE);
  align = params.align;
  convert_tiled_output = params.convert_tiled_output;
  num_frame_buffers = params.num_buffers;
  if (num_frame_buffers > MAX_BUFFERS) num_frame_buffers = MAX_BUFFERS;
  rv_input = params.rv_input;
  skip_frame = params.skip_frame;
  use_extra_buffers = params.use_extra_buffers;
  allocate_extra_buffers_in_output = params.allocate_extra_buffers_in_output;
  tool_params.ext_buffers = ext_buffers;
  tool_params.max_buffers = &num_buffers;

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

#ifdef ASIC_TRACE_SUPPORT
    g_hw_ver = tb_cfg.dec_params.hw_version;
#endif
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

  if (rv_input) {
    rv_mode(argv[argc-1],
            NULL,
            startframeid,
            maxnumber,
            iswritefile,
            isdisplay,
            displaymode,
            rotationmode,
            islog2std,
            islog2file,
            blayerenable,
            errconceal,
            randomerror);
    if(frame_out) {
      fclose(frame_out);
      free(frame_buffer);
    }
    return 0;
  }
  rv_display(argv[argc-1],
             NULL,
             startframeid,
             maxnumber,
             iswritefile,
             isdisplay,
             displaymode,
             rotationmode,
             islog2std,
             islog2file,
             blayerenable,
             errconceal,
             randomerror);

  if(frame_out) {
    fclose(frame_out);
    free(frame_buffer);
  }

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
  u8 *p_out_cb, *p_out_cr;

  /* Code */

  memset( p_out, 128, out_width*out_height*3/2 );

  /* Luma */
  for ( y = 0 ; y < in_height ; ++y ) {
    for( x = 0 ; x < in_width; ++x )
      *p_out++ = *p_in++;
    p_in += ( in_frame_width - in_width );
    p_out += ( out_width - in_width );
  }

  p_in += in_frame_width * ( in_frame_height - in_height );
  p_out += out_width * ( out_height - in_height );

  in_frame_height /= 2;
  in_frame_width /= 2;
  out_height /= 2;
  out_width /= 2;
  in_height /= 2;
  in_width /= 2;

  /* Chroma */
  /*p_out += 2 * out_width * ( out_height - in_height );*/
  p_out_cb = p_out;
  p_out_cr = p_out + out_width*out_height;
  for ( y = 0 ; y < in_height ; ++y ) {
    for( x = 0 ; x < in_width; ++x ) {
      *p_out_cb++ = *p_in++;
      *p_out_cr++ = *p_in++;
    }
    p_in += 2 * ( in_frame_width - in_width );
    p_out_cb += ( out_width - in_width );
    p_out_cr += ( out_width - in_width );
  }
}

/*------------------------------------------------------------------------------
        printRvVersion
        Description : Print out decoder version info
------------------------------------------------------------------------------*/

void printRvVersion(u32 client_type) {
  struct DecSwHwBuild build;
  struct DecApiVersion version;

  build = VCDecGetBuild(dwl_inst, client_type);
  version = VCDecGetAPIVersion();
  printf("[TB] VC9000 Decoder RV API version %u.%u.%u\n", version.major,
        version.minor, version.micro);
  printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
}
