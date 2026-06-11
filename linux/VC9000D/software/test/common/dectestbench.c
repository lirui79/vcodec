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
#define _SW_DEBUG_PRINT

#include "dwlthread.h"
#include "fifo.h"
#include "regdrv.h"
#include "basetype.h"
#include "decapi.h"
#include "dwl.h"
#include "bytestream_parser.h"
#include "command_line_parser.h"
#include "error_simulator.h"
#include "common_sink.h"
#include "file_sink.h"
#include "md5_sink.h"
#include "null_sink.h"
#ifdef SDL_ENABLED
#include "sdl_sink.h"
#endif
#include "tb_cfg.h"
#ifdef MODEL_SIMULATION
#include "asic.h"
#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#endif
#include "trace_hooks.h"
#include "vpxfilereader.h"
#include "error.h"
#include "vcd_dec_hdr.h" /* sei and hdr */
#include "vcdecapi.h"
#include "dec_log.h"

#define NUM_OF_STREAM_BUFFERS (MAX_ASIC_CORES + 1)
#define DEFAULT_STREAM_BUFFER_SIZE (1024 * 1024)
#define NEXT_MULTIPLE(value, n) (((value) + (n) - 1) & ~((n) - 1))

#ifndef MIN
/* macro to get smaller of two values */
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define MAX_BUFFERS 40

#define NUM_REF_BUFFERS 10

#define MAXARGS 128

struct Client {
  Demuxer demuxer; /* Demuxer instance. */
  DecInst decoder; /* Decoder instance. */
  YuvSink *yuvsink; /* Yuvsink instance. */
  const void* dwl; /* OS wrapper layer instance and function pointers. */
  #ifdef ASIC_ONL_SIM
  u32 dec_done;
  #else
  sem_t dec_done;  /* Semaphore which is signalled when decoding is done. */
  #endif
  struct DecInput buffers[NUM_OF_STREAM_BUFFERS]; /* Stream buffers. */
  struct SEI_buffer sei_buffer; /* SEI_buffer */
  T35_HDR_Param t35_hdr_parma; /* t35 hdr */
  u32 num_of_output_pics; /* Counter for number of pictures decoded. */
  FifoInst pic_fifo;      /* Picture FIFO to parallelize output. */
  pthread_t parallel_output_thread; /* Parallel output thread handle. */
  struct TestParams test_params;    /* Parameters for the testing. */
  u8 eos; /* Flag telling whether client has encountered end of stream. */
  u32 cycle_count; /* Sum of average cycles/mb counts */
#ifdef FPGA_PERF_AND_BW
  u32 bitrate_count;/* Sum of bitrate for all frame*/
  long int bw_rd_count;/* Sum of read bandwidth */
  long int bw_wr_count;/* Sum of write bandwidth*/
#endif
  u8 dec_initialized; /* Flag indicating whether decoder has been initialized successfully. */
  u8 new_hdr;         /* Flag indicating whether a new header is coming. */
  struct DWLLinearMem ext_buffers[MAX_BUFFERS];
  u32 max_buffers;
  u32 num_pics_to_display;  /* Picture numbers to be displayed. */
#ifdef ASIC_TRACE_SUPPORT
  struct DWLLinearMem ext_frm_buffers[MAX_BUFFERS];
  u32 max_frm_buffers;
#endif
  struct Client * multi_client[MAX_STREAMS];
  i32 argc;
  char **argv;      /* Command line parameter... */
};

#ifdef SEM_REPLACE_MUTEX
static sem_t    mutex;
#else
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

typedef struct {
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

/* Client actions */
static void DispatchBufferForDecoding(struct Client* client,
                                      struct DecInput* buffer);
static void DispatchEndOfStream(struct Client* client);
static void PostProcessPicture(struct Client* client,
                               struct DecPictures* picture);

/* Callbacks from decoder. */
static void InitializedCb(ClientInst inst);
static void HeadersDecodedCb(ClientInst inst,
                             struct DecSequenceInfo sequence_info, PpUnitConfig *ppu_cfg);
static u32 BufferRequestCb(ClientInst inst);
static void BufferDecodedCb(ClientInst inst, struct DecInput* buffer);
static void PictureReadyCb(ClientInst inst, struct DecPictures picture);
static void EndOfStreamCb(ClientInst inst);
static void ReleasedCb(ClientInst inst);
static void NotifyErrorCb(ClientInst inst, u32 pic_id, enum DecRet rv);

/* Output thread functionality. */
static void* ParallelOutput(void* arg); /* Output loop. */

/* Helper functionality. */
static const void* CreateDemuxer(struct Client* client);
static void ReleaseDemuxer(struct Client* client);

// static u8 ErrorSimulationNeeded(struct Client* client);
static i32 GetStreamBufferCount(struct Client* client);

static void StreamBufferConsumedMC(void *stream, void *p_user_data);

/* (Unfortunate) Hack globals to carry around data for model. */
struct TBCfg tb_cfg;
static RET_TYPE MainTask(void * args);
static void *ThreadMain(void *args);
static int RunInstance(struct Client  *client);
static void SetClientByTBCfg(struct Client* client);
static void OpenTestHooks(const struct Client* client);
static void CloseTestHooks(struct Client* client);
static int HwconfigOverride(DecInst dec_inst, struct TBCfg* tbcfg);
static int ComParseStreamCfg(char *streamcfg, struct Client *client);
#ifdef SUPPORT_SEI
static void VcdSEIInit(struct Client *client);
static void VcdSEIInfoReady(struct Client *client, struct DecPictures *picture);
#endif

#ifdef ASIC_ONL_SIM
u32 max_pics_decode;
int MultiStreamId;

int main_onl(int argc, char* sv_str, int id) {
  char* argv[argc];
  char* delim=" " ;
  int   j=0       ;
  MainArgs args;
  MultiStreamId = id;

  char* p=strtok(sv_str,delim);
  argv[j]=p;
  while(p!=NULL){
    p=strtok(NULL,delim);
    j++;
    argv[j]=p;
  }

  args.argc = argc;
  args.argv = argv;
  osal_thread_init();
  MainTask(&args);
  return 0;
}

#else
int main(int argc, char* argv[]) {
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

  return 0; //should not arrive here if start scheduler successfully in the real env
#else
  return MainTask(&args);
#endif //__FREERTOS__
}
#endif

RET_TYPE MainTask(void * args)
{
  MainArgs *args_ = (MainArgs *)args;
  int argc = args_->argc;
  char **argv = args_->argv;

  struct Client client;
  int ret = 0, i = 0;
#ifdef SEM_REPLACE_MUTEX
  sem_init(&mutex, 0 , 1);
#endif
  memset(&client, 0, sizeof(struct Client));

  printf("\n[TB] Hantro G2 decoder command line interface\n\n");
  SetupDefaultParams(&client.test_params);

  if (argc < 2) {
    PrintUsage(argv[0], G2DEC);
    ret = 0;
    goto return_;
  }
  if ((ret = ParseParams(argc, argv, &client.test_params))) {
    if (ret == 1) {
      printf("[TB] Failed to parse params.\n\n");
      PrintUsage(argv[0], G2DEC);
      ret = 1;
      goto return_;
    }
    else {
      ret = 0;
      goto return_;
    }
  }

#ifdef ASIC_ONL_SIM
  max_pics_decode = client.test_params.num_of_decoded_pics;
#endif

  OpenTestHooks(&client);

#ifdef FPGA_PERF_AND_BW
//printf CMD option
  printf("\n[TB] CMD options:");
  for(int i = 1;i < argc -1;i++)
    printf("%s ",argv[i]);
  printf("\n");
  printf("[TB] HwBuildId:%X\n\n",tb_cfg.dec_params.hw_build_id);
#endif
  if(client.test_params.nstrmcfg > 0) {
    if(client.test_params.multimode == 1) {
      if (client.test_params.nstrmcfg > MAX_STREAMS) {
        printf("[TB] Warning: the maximum supported streams are %d, but test params has %d.\n",
            MAX_STREAMS, client.test_params.nstrmcfg);
        client.test_params.nstrmcfg = MAX_STREAMS;
      }
      /* multi thread */
      /* method 2: only one dec.cfg record all command lines. */
      if(1) {
        char buf[1024];//buffer one command line
        int n_stream = 0, i = 0;
        for (i = 0; i < client.test_params.nstrmcfg;i++) {
          FILE *fp = fopen(client.test_params.streamcfg[i], "r");
          if (fp == NULL) {
            printf("UNABLE TO OPEN STREAM CFG FILE\n");
            ret = 1;
            goto return_;
          }
          while(fgets(buf, 1024, fp)) {
            if (buf[0] == '#') continue;
            client.multi_client[n_stream] = (struct Client *)DWLcalloc(1, sizeof(struct Client));
            ComParseStreamCfg(buf,client.multi_client[n_stream]);
            pthread_attr_t attr;
            pthread_t *tid= (pthread_t*)DWLcalloc(1, sizeof(pthread_t));
            if (tid == NULL){
              fclose(fp);
              printf("Failed to alloc tid.\n\n");
              ret = 1;
              goto return_;
            }
            pthread_attr_init(&attr);
            pthread_create(tid, &attr, &ThreadMain, client.multi_client[n_stream]);
            client.test_params.multi_stream_id.tid[n_stream] = tid;
            pthread_attr_destroy(&attr);
            n_stream++;
          }
          fclose(fp);
          client.test_params.nstream = n_stream;
        }
      }
    } else if(client.test_params.multimode == 2) {
      /* multi process */
      printf("[TB] Warning: multimode 2 is reserved for future use.\nPlease use --multimode=1 instead.\n");
      exit(-1); // will exit the whole process, should not use it in thread
    } else if(client.test_params.multimode == 0) {
      printf("[TB] multi-stream disabled, ignore extra stream configs\n");
    } else {
      printf("[TB] Invalid multi stream mode\n");
      exit(-1); // will exit the whole process, should not use it in thread
    }
  } else {
    client.argc = argc;
    client.argv = argv;
    ret = RunInstance(&client);
  }
  if(client.test_params.multimode == 1)
  {
    for(i = 0; i < client.test_params.nstream; i++)
    {
      if(client.test_params.multi_stream_id.tid[i] != NULL)
        pthread_join(*client.test_params.multi_stream_id.tid[i], NULL);
    }
  }

  else if (client.test_params.multimode == 2)
  {
    //Next to do
  }
  if(client.test_params.multimode != 0)
  {
    for(i = 0; i < client.test_params.nstream; i++)
    {
      free(client.multi_client[i]);
      client.multi_client[i] = NULL;
      free(client.test_params.multi_stream_id.tid[i]);
      client.test_params.multi_stream_id.tid[i] = NULL;
    }
  }
#ifdef ASIC_ONL_SIM
	  dpi_display_info("case_end","NULL", MultiStreamId);
#endif
return_:
#ifdef __FREERTOS__
#ifdef FREERTOS_SIMULATOR
  vTaskEndScheduler(); // need to open in simulator, and need to close on real env
#endif
  UNUSED(ret);
  return (void *)NULL;
#else
#ifndef ASIC_ONL_SIM
  sem_destroy(&client.dec_done);
#endif
  return ret;
#endif
}
void *ThreadMain(void *args)
{
  RunInstance((struct Client *)args);
  pthread_exit(NULL);
  return NULL;
}

int RunInstance(struct Client * client)
{
  struct DecClientHandle client_if = {
    client,         InitializedCb, HeadersDecodedCb, BufferDecodedCb,
    BufferRequestCb,
    PictureReadyCb, EndOfStreamCb, ReleasedCb,       NotifyErrorCb};

#ifdef SUPPORT_SEI
  VcdSEIInit(client);
#endif

  client->max_buffers = 0;
  DWLmemset(client->ext_buffers, 0, sizeof(client->ext_buffers));
#ifdef ASIC_TRACE_SUPPORT
  client->max_frm_buffers = 0;
  DWLmemset(client->ext_frm_buffers, 0, sizeof(client->ext_frm_buffers));
#endif
  struct DWLInitParam dwl_params = {DWL_CLIENT_TYPE_HEVC_DEC};

  SetClientByTBCfg(client);

  client->demuxer.inst = CreateDemuxer(client);
  if (client->demuxer.inst == NULL) {
    printf("[TB] Failed to open demuxer (file missing or of wrong type?)\n");
    return -1;
  }

  FILE* fp_dmv, *fp_qp;
  if(client->test_params.auxinfo){
    fp_dmv = fopen("dmv.bin", "wb");
    fp_qp = fopen("qp.bin", "wb");
  }else{
    fp_dmv = NULL;
    fp_qp = NULL;
  }

  if (client->test_params.extra_output_thread)
    /* Create the fifo to enable parallel output processing */
    FifoInit(2, &client->pic_fifo);

#ifdef ASIC_ONL_SIM
  client->dec_done = 0;
#else
  sem_init(&client->dec_done, 0, 0);
#endif

  enum DecCodec codec = DEC_HEVC;
  switch (client->demuxer.GetVideoFormat(client->demuxer.inst)) {
  case BITSTREAM_VVC:
    codec = DEC_VVC;
    dwl_params.client_type = DWL_CLIENT_TYPE_VVC_DEC;
    break;
  case BITSTREAM_HEVC:
    codec = DEC_HEVC;
    dwl_params.client_type = DWL_CLIENT_TYPE_HEVC_DEC;
    break;
  case BITSTREAM_H264:
    codec = DEC_H264;
    dwl_params.client_type = DWL_CLIENT_TYPE_H264_DEC;
    break;
  case BITSTREAM_VP9:
    codec = DEC_VP9;
    dwl_params.client_type = DWL_CLIENT_TYPE_VP9_DEC;
    if (client->test_params.read_mode == STREAMREADMODE_FULLSTREAM) {
      fprintf(stderr, "[TB] Full-stream (-F) is not supported in VP9.\n");
      goto err;
    }
    if (client->test_params.disable_display_order) {
      fprintf(stderr, "[TB] Disable display reorder (-R) is not supported in VP9.\n");
      goto err;
    }
    break;
  case BITSTREAM_VP7:
    fprintf(stderr, "[TB] Please check whether you are decoding a webm stream without WEBM_ENABLED.\n");
    break;
  case BITSTREAM_AVS2:
    codec = DEC_AVS2;
    dwl_params.client_type = DWL_CLIENT_TYPE_AVS2_DEC;
    break;
  case BITSTREAM_AV1:
  case BITSTREAM_AV1_ANNEXB:
  case BITSTREAM_AV1_OBU:
    dwl_params.client_type = DWL_CLIENT_TYPE_AV1_DEC;
    codec = DEC_AV1;
    break;
  default:
	goto err;
  }
  dwl_params.dec_dev = client->test_params.dec_dev;
  dwl_params.mem_dev = client->test_params.mem_dev;
#ifdef SUPPORT_DMA
  dwl_params.dma_dev = client->test_params.dma_dev;
#endif
#ifdef SUPPORT_RANDOM_LATENCY
  dwl_params.axi_lg_r = client->test_params.axi_lg_r;
  dwl_params.axi_lg_w = client->test_params.axi_lg_w;
#endif

  /* Create struct DWL. */
  client->dwl = DWLInit(&dwl_params);
  {
    struct DecSwHwBuild build;
    struct DecApiVersion version;

    build = DecGetBuild(client->dwl);
    version = DecGetAPIVersion();
    printf("[TB] VC9000 Decoder API version %u.%u.%u\n", version.major,
          version.minor, version.micro);
    printf("[TB] Hardware Build ID: 0x%x, ASIC_ID 0x%x, Software Build: 0x%x\n", build.hw_build_id[0], build.asic_id, build.sw_build);
  }
  /* Init two params :
     1. DecInitConfig will be used in VCDecInit only.
     2. DecConfig will be used in VCDecSetInfo only. */
  /* 1. Init DecInitConfig. */
  struct DecInitParams init_params;
  DWLmemset(&init_params, 0, sizeof(init_params));
  struct DecInitConfig *init_config = &init_params.init_config;
  init_config->dwl_inst = client->dwl;
  init_config->codec = codec;
  init_config->multi_frame_input_flag = client->test_params.read_mode == STREAMREADMODE_FULLSTREAM ? 1 : 0;
  init_config->disable_picture_reordering = client->test_params.disable_display_order;
  init_config->error_handling = client->test_params.error_handling;
  init_config->error_ratio = client->test_params.error_ratio;
  init_config->use_video_compressor = client->test_params.compress_bypass ? 0 : 1;
  init_config->guard_size = 0;
  init_config->use_adaptive_buffers = 1;
  init_config->rlc_mode = client->test_params.rlc_mode;
  init_config->decoder_mode = client->test_params.decoder_mode;
  init_params.max_num_pics_to_decode = client->test_params.num_of_decoded_pics;
#ifdef SEEK_TEST
  if (!init_params.max_num_pics_to_decode) {
    DEBUG_PRINT(("error... must set -N when seek test.\n"));
    return -1;
  }
#endif
  init_config->mvc = client->test_params.mvc;
  /* VVC don't support hw ec now. */
  if (codec == DEC_VVC) {
    if (client->test_params.mc_enable)
      client->test_params.hw_conceal = 1;
    else
      client->test_params.hw_conceal = 0;
  }
  if (client->test_params.mc_enable) {
    init_config->mc_cfg.mc_enable = 1;
    /* AV1/VP9 use tile-level mc, no need to register stream consume callback */
    if (codec == DEC_AV1 || codec == DEC_VP9)
      init_config->mc_cfg.stream_consumed_callback = NULL;
    else {
      init_config->mc_cfg.stream_consumed_callback = StreamBufferConsumedMC;
      /* HW EC is slice_level default, but MC don't support BUF_EMPTY. */
      if (client->test_params.hw_conceal &&
          (codec == DEC_HEVC || codec == DEC_H264 || codec == DEC_AV1 || codec == DEC_VVC))
        client->test_params.disable_slice = 1;
    }
  } else {
    init_config->mc_cfg.mc_enable = 0;
    init_config->mc_cfg.stream_consumed_callback = NULL;
  }
  init_config->annexb = client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_AV1_ANNEXB;
  init_config->plainobu = client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_AV1_OBU;
  init_config->tile_transpose = client->test_params.tile_transpose;
  init_config->oppoints = client->test_params.oppoints;
  init_config->num_frame_buffers = client->test_params.num_buffers;
  init_config->skip_frame = client->test_params.skip_frame;
  init_config->auxinfo = client->test_params.auxinfo;
  init_config->support_asofmo_stream = client->test_params.support_asofmo_stream;
  /* 2. Init DecConfig. */
  struct DecConfig *config = &init_params.config;
  config->hw_conceal = client->test_params.hw_conceal;
  config->disable_slice = client->test_params.disable_slice;
  config->align = client->test_params.align;
  config->max_temporal_layer = client->test_params.max_temporal_layer;
  DWLmemcpy(config->ppu_cfg, client->test_params.ppu_cfg, sizeof(config->ppu_cfg));
  DWLmemcpy(config->delogo_params, client->test_params.delogo_params, sizeof(config->delogo_params));

  /* for color remapping (3dlut) */
  int i;
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!config->ppu_cfg[i].enabled) continue;
    if (client->test_params.ppu_cfg[i].enable_3dlut == 1) {
      u32 size = 18 * 18 * 18 * 3 * sizeof(u16);
      // ppu_int_cfg.table_3dlut.mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      config->table_3dlut_buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                        DWL_MEM_TYPE_CPU;
      if (DWLMallocLinear(client->dwl, size, &config->table_3dlut_buffer)) {
        return -1;
      }
      DWLmemset(config->table_3dlut_buffer.virtual_address, 0, size);
      u16 lut3DTableData[18 * 18 * 18][3] = {{0}};
      memset(lut3DTableData, 0, sizeof(lut3DTableData));
      u16 tmpDataNew[18 * 18 * 18][3] = {{0}};
      if (client->test_params.table_3dlut_name != NULL) {
        dump3DLutData(lut3DTableData, client->test_params.table_3dlut_name);
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
      memcpy((u32 *)config->table_3dlut_buffer.virtual_address, lut3DTableData, 18 * 18 * 18 * 3 * sizeof(u16));
    }
  }


  /* Initialize the decoder. */
  if (DecInit(codec, &client->decoder, &init_params, &client_if) != DEC_OK) {
	goto err;
  }

  /* The rest is driven by the callbacks and this thread just has to Wait
  * until decoder has finished it's job. */
#ifdef ASIC_ONL_SIM
  while (!client->dec_done) {
    usleep(100);
  }
#else
  sem_wait(&client->dec_done);
#endif
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    if (!config->ppu_cfg[i].enabled) continue;
    if (client->test_params.ppu_cfg[i].enable_3dlut == 1) {
      if (config->table_3dlut_buffer.virtual_address) {
        DWLFreeLinear(client->dwl, &config->table_3dlut_buffer);
        config->table_3dlut_buffer.virtual_address = NULL;
      }
    }
  }

  PrintOutputFileName(&client->test_params);
  /* release output file name buffer */
  FreeOutputFileName(&client->test_params);
  if (client->yuvsink) ReleaseYuvSink(client->yuvsink);
  ReleaseDemuxer(client);
  if (fp_dmv) fclose(fp_dmv);
  if (fp_qp) fclose(fp_qp);
  if (client->pic_fifo != NULL) FifoRelease(client->pic_fifo);
  if (client->dwl != NULL) DWLRelease(client->dwl);
  if (client->sei_buffer.buffer != NULL) DWLfree(client->sei_buffer.buffer); /* sei_buffer */

  DecDestroy(client->decoder);
  VCDecLogDestory();
  CloseTestHooks(client);

  return 0;

err:
  if (fp_dmv) fclose(fp_dmv);
  if (fp_qp) fclose(fp_qp);
  return -1;
}

#ifdef SEEK_TEST
extern u32 seek_end;
#endif
static void DispatchBufferForDecoding(struct Client* client,
                                      struct DecInput* dec_input) {
  enum DecRet rv;
  i32 size = dec_input->buffer.size;
  //memset(buffer->buffer.virtual_address, 0, size);
  i32 len = client->demuxer.ReadPacket(
              client->demuxer.inst,
              (u8*)dec_input->buffer.virtual_address,
              dec_input->stream, &size,
              client->test_params.is_ringbuffer);
  if (len <= 0) {/* EOS or error. */
    /* If reading was due to insufficient buffer size, try to realloc with
       sufficient size. */
    if (size > dec_input->buffer.size) {
      i32 i;
      DEBUG_PRINT(("[TB] Trying to reallocate buffer to fit next buffer.\n"));
      for (i = 0; i < GetStreamBufferCount(client); i++) {
        if (DWL_DEVMEM_COMPARE_V2(client->buffers[i].buffer, dec_input->buffer)) {
          DWLFreeLinear(client->dwl, &client->buffers[i].buffer);
#ifdef SUPPORT_DMA
          client->buffers[i].buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                               DWL_MEM_TYPE_CPU;
#endif
          SET_MEM_USAGE(client->buffers[i].buffer.mem_type, DWL_MEM_USAGE_IN_STRM,
                  (client->test_params.decoder_mode & DEC_SECURITY) ? 1 : 0);
          /* size + 16 to avoid buffer not enough when (buff_size-strm_size) < 16  (0<=start bits/8<16)*/
          if (DWLMallocLinear(client->dwl, size+16, &client->buffers[i].buffer)) {
            fprintf(stderr, "[TB] No memory available for the stream buffer\n");
            DispatchEndOfStream(client);
            return;
          }
          /* Initialize stream to buffer start */
          client->buffers[i].stream[0] = (u8*)client->buffers[i].buffer.virtual_address;
          client->buffers[i].stream[1] = (u8*)client->buffers[i].buffer.virtual_address;
          DispatchBufferForDecoding(client, &client->buffers[i]);
          return;
        }
      }
    } else {
#ifdef SEEK_TEST
      if (seek_end == 0xFF) {
        DecDecode(client->decoder, dec_input);
      }
      if (seek_end == 1)
#endif
      DispatchEndOfStream(client);
      return;
    }
  }
  dec_input->data_len = len;
  if (client->eos) return; /* Don't dispatch new buffers if EOS already done. */

  /* Decode the contents of the input stream buffer. */
  switch (rv = DecDecode(client->decoder, dec_input)) {
  case DEC_OK:
    /* Everything is good, keep on going. */
    break;
  default:
    fprintf(stderr, "[TB] UNKNOWN ERROR!\n");
    DispatchEndOfStream(client);
    break;
  }
}

static void DispatchEndOfStream(struct Client* client) {
  if (!client->eos) {
    client->eos = 1;
    DecEndOfStream(client->decoder);
  }
}

static void InitializedCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;
  /* Internal testing feature: Override HW configuration parameters */
  HwconfigOverride(client->decoder, &tb_cfg);
  client->dec_initialized = 1;

  if (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_H264) {

  }

  /* Start the output handling thread, if needed. */
  if (client->test_params.extra_output_thread)
    pthread_create(&client->parallel_output_thread, NULL, ParallelOutput,
                   client);
  for (int i = 0; i < GetStreamBufferCount(client); i++) {
#ifdef SUPPORT_DMA
    client->buffers[i].buffer.mem_type = DWL_MEM_TYPE_DMA_HOST_TO_DEVICE |
                                         DWL_MEM_TYPE_CPU;
#endif

    SET_MEM_USAGE(client->buffers[i].buffer.mem_type, DWL_MEM_USAGE_IN_STRM,
                  (client->test_params.decoder_mode & DEC_SECURITY) ? 1 : 0);
    if (client->test_params.input_buffer_size > 0) {
      if (DWLMallocLinear(client->dwl, client->test_params.input_buffer_size,
                          &client->buffers[i].buffer)) {
        fprintf(stderr, "[TB] No memory available for the stream buffer\n");
        DecRelease(client->decoder);
        return;
      }
    } else {
      if (DWLMallocLinear(client->dwl, DEFAULT_STREAM_BUFFER_SIZE,
                          &client->buffers[i].buffer)) {
        fprintf(stderr, "[TB] No memory available for the stream buffer\n");
        DecRelease(client->decoder);
        return;
      }
    }
    /* Initialize stream to buffer start */
    client->buffers[i].stream[0] = (u8*)client->buffers[i].buffer.virtual_address;
    client->buffers[i].stream[1] = (u8*)client->buffers[i].buffer.virtual_address;
    client->buffers[i].sei_buffer = &client->sei_buffer; /* SEI buffer */
    /* Dispatch the first buffers for decoding. When decoder finished
     * decoding each buffer it will be refilled within the callback. */
    DispatchBufferForDecoding(client, &client->buffers[i]);
  }
}

static void HeadersDecodedCb(ClientInst inst, struct DecSequenceInfo info, PpUnitConfig *ppu_cfg) {
  struct OutFileInfo outfile_info;
  struct Client* client = (struct Client*)inst;
  const struct DecHwFeatures *hw_feature = NULL;
  av_unused u32 i = 0, core_mask = 0;
  enum DWLClientType client_type = DWL_CLIENT_TYPE_HEVC_DEC;
  u32 format = client->demuxer.GetVideoFormat(client->demuxer.inst);

  DEBUG_PRINT(("[TB] NOTE: for INTERLACED stream, the resolution is of a FRAME.\n"));
  DEBUG_PRINT(
    ("[TB] Headers: Width %u Height %u\n", info.pic_width, info.pic_height));
  DEBUG_PRINT(
    ("[TB] Headers: Cropping params: (%u, %u) %ux%u\n",
     info.crop_params.crop_left_offset, info.crop_params.crop_top_offset,
     info.crop_params.crop_out_width, info.crop_params.crop_out_height));
  DEBUG_PRINT(("[TB] Headers: MonoChrome = %u\n", info.is_mono_chrome));
  DEBUG_PRINT(("[TB] Headers: Pictures in DPB = %u\n", info.num_of_ref_frames));
  DEBUG_PRINT(("[TB] Headers: video_range %d\n", info.video_range));
  DEBUG_PRINT(("[TB] Headers: matrix_coefficients %u\n", info.matrix_coefficients));
  DEBUG_PRINT(("[TB] Headers: %s sequence\n", info.is_interlaced ? "INTERLACED" : "PROGRESSIVE"));
  DEBUG_PRINT(("[TB] Headers: bit_depth = Y%uC%u\n", info.bit_depth_luma, info.bit_depth_chroma));
  DEBUG_PRINT(("[TB] Headers: chroma_format_idc = %u\n", info.chroma_format_idc));

  if (info.h264_base_mode == 1) {
    client->test_params.is_ringbuffer = 0;
    client->test_params.mc_enable = 0;
  }
#ifdef ASIC_TRACE_SUPPORT
  if (client->test_params.mc_enable && (format == BITSTREAM_HEVC || format == BITSTREAM_H264 ||
                                        format == BITSTREAM_VVC ||format == BITSTREAM_AVS2)) {
    client->test_params.mc_enable = 0;
  }
#else
  UNUSED(format);
#endif

  /* maybe generate the new sewuence info, adjust the ppu_cfg, which will be push by setInfo in the next step */
#if 0
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++, ppu_cfg++) {
    if (!ppu_cfg->enabled)
      continue;

    if(ppu_cfg->crop.enabled)
    {
      if((ppu_cfg->crop.width  < PP_CROP_MIN_WIDTH && ppu_cfg->crop.width != info.crop_params.crop_out_width) ||
         (ppu_cfg->crop.height < PP_CROP_MIN_HEIGHT && ppu_cfg->crop.height != info.crop_params.crop_out_height) ||
         (ppu_cfg->crop.x + ppu_cfg->crop.width > info.crop_params.crop_out_width) ||
         (ppu_cfg->crop.y + ppu_cfg->crop.height > info.crop_params.crop_out_height))
      {
        ppu_cfg->crop.x = 0;
        ppu_cfg->crop.y = 0;

        DEBUG_PRINT(("Headers: ppu_cfg[%d].crop.width from %d ", i, ppu_cfg->crop.width));
        ppu_cfg->crop.width = (info.crop_params.crop_out_width+1) & ~0x1;
        DEBUG_PRINT(("to %d\n", ppu_cfg->crop.width));

        DEBUG_PRINT(("Headers: ppu_cfg[%d].crop.height from %d ", i, ppu_cfg->crop.height));
        ppu_cfg->crop.height = (info.crop_params.crop_out_height+1) & ~0x1;
        DEBUG_PRINT(("to %d\n", ppu_cfg->crop.height));
      }
    }

    if(ppu_cfg->scale.enabled)
    {
      if((ppu_cfg->scale.ratio_x + ppu_cfg->scale.width > info.crop_params.crop_out_width) ||
          (ppu_cfg->scale.ratio_y + ppu_cfg->scale.height > info.crop_params.crop_out_height))
      {
        ppu_cfg->scale.ratio_x = 0;
        ppu_cfg->scale.ratio_y = 0;

        DEBUG_PRINT(("Headers: ppu_cfg[%d].scale.width from %d ", i, ppu_cfg->scale.width));
        ppu_cfg->scale.width = info.crop_params.crop_out_width;
        DEBUG_PRINT(("to %d\n", ppu_cfg->scale.width));

        DEBUG_PRINT(("Headers: ppu_cfg[%d].scale.height from %d ", i, ppu_cfg->scale.height));
        ppu_cfg->scale.height = info.crop_params.crop_out_height;
        DEBUG_PRINT(("to %d\n", ppu_cfg->scale.height));
      }
    }
  }
#endif

  /* Ajust user cropping params based on cropping params from sequence info. */
  /* what is the purpose of the next adjust about the client->test_params.ppu_cfg */
  if (info.crop_params.crop_left_offset != 0 ||
      info.crop_params.crop_top_offset != 0 ||
      info.crop_params.crop_out_width != info.pic_width ||
      info.crop_params.crop_out_height != info.pic_height) {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (!client->test_params.ppu_cfg[i].enabled)
        continue;

      if (!client->test_params.ppu_cfg[i].crop.enabled && !info.dis_comformance_window) {
        client->test_params.ppu_cfg[i].crop.x = info.crop_params.crop_left_offset;
        client->test_params.ppu_cfg[i].crop.y = info.crop_params.crop_top_offset;
        client->test_params.ppu_cfg[i].crop.width = (info.crop_params.crop_out_width +1) & ~0x1;
        client->test_params.ppu_cfg[i].crop.height = (info.crop_params.crop_out_height +1) & ~0x1;
      } else if(client->test_params.ppu_cfg[i].crop.enabled && !info.dis_comformance_window){
        client->test_params.ppu_cfg[i].crop.x += info.crop_params.crop_left_offset;
        client->test_params.ppu_cfg[i].crop.y += info.crop_params.crop_top_offset;
        if(!client->test_params.ppu_cfg[i].crop.width)
          client->test_params.ppu_cfg[i].crop.width = (info.crop_params.crop_out_width+1) & ~0x1;
        if(!client->test_params.ppu_cfg[i].crop.height)
          client->test_params.ppu_cfg[i].crop.height = (info.crop_params.crop_out_height+1) & ~0x1;
      }
      client->test_params.ppu_cfg[i].enabled = 1;
      client->test_params.ppu_cfg[i].crop.enabled = 1;
      client->test_params.pp_enabled = 1;
    }
  }

  switch (client->demuxer.GetVideoFormat(client->demuxer.inst)) {
    case BITSTREAM_VVC:
      client_type = DWL_CLIENT_TYPE_VVC_DEC; break;
    case BITSTREAM_HEVC:
      client_type = DWL_CLIENT_TYPE_HEVC_DEC; break;
    case BITSTREAM_H264:
      client_type = DWL_CLIENT_TYPE_H264_DEC; break;
    case BITSTREAM_VP9:
      client_type =  DWL_CLIENT_TYPE_VP9_DEC; break;
    case BITSTREAM_AVS2:
      client_type = DWL_CLIENT_TYPE_AVS2_DEC; break;
    case BITSTREAM_AV1:
    case BITSTREAM_AV1_ANNEXB:
    case BITSTREAM_AV1_OBU:
      client_type = DWL_CLIENT_TYPE_AV1_DEC; break;
    default:
      break;
  }
  hw_feature = DWLGetHwFeaturesByClientType(client->dwl, client_type, &core_mask);
  client->test_params.crop_align = hw_feature->crop_step_rshift;
  if (client->yuvsink == NULL) {
    outfile_info.bitstream_format = client->demuxer.GetVideoFormat(client->demuxer.inst);
    outfile_info.pic_width = info.pic_width;
    outfile_info.pic_height = info.pic_height;
    outfile_info.bit_depth = (info.bit_depth_luma == 8 && info.bit_depth_chroma == 8) ? 8 : 10;
    outfile_info.is_interlaced = info.is_interlaced;
    GenerateOutputFileName(&client->test_params, &outfile_info);
    client->yuvsink = CreateYuvSink(&client->test_params);
    if ((client->yuvsink == NULL) || (client->yuvsink->inst == NULL)) {
      fprintf(stderr, "[TB] Failed to create YUV sink\n");
      DispatchEndOfStream(client);
    }
  }
  client->demuxer.HeadersDecoded(client->demuxer.inst);
  client->new_hdr = 1;

//  if (info.num_of_ref_frames < NUM_REF_BUFFERS)
//    DecUseExtraFrmBuffers(client->decoder, NUM_REF_BUFFERS - info.num_of_ref_frames);
}

static u32 BufferRequestCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;

  struct DWLLinearMem mem = {0};
  struct DecBufferInfo info;
  i32 i;

  DWLmemset(&info, 0, sizeof(info));
  if (client->test_params.num_buffers)
    DecGetPictureBuffersInfo(client->decoder, &info);
/*
  if (client->new_hdr && !info.add_extra_ext_buf &&
      ((client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_HEVC)||
      (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_VVC)
      (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_AVS2))) {
    while (client->num_pics_to_display) sched_yield();
    // Release all external buffers for VVC,AVS2.
    //   For H264/HEVC, buffers to be freed are returned in DecGetPictureBuffersInfo(),
    //   then freed one by one.
    for (i = 0; i < client->max_buffers; i++) {
      DWLFreeLinear(client->dwl, &client->ext_buffers[i]);
      DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
    }
#ifdef ASIC_TRACE_SUPPORT
    for (i = 0; i < client->max_frm_buffers; i++) {
      DWLFreeRefFrm(client->dwl, &client->ext_frm_buffers[i]);
      DWLmemset(&client->ext_frm_buffers[i], 0, sizeof(client->ext_frm_buffers[i]));
    }
    client->max_frm_buffers = 0;
#endif
    client->max_buffers = 0;
    client->new_hdr = 0;
  }
*/
#ifdef SEEK_TEST
    for (i = 0; i < client->max_buffers; i++) {
      if (client->ext_buffers[i].virtual_address) {
        DWLFreeLinear(client->dwl, &client->ext_buffers[i]);
        DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
      }
    }
    client->max_buffers = 0;
#endif
  while (1) {
    DWLmemset(&info, 0, sizeof(info));
    if (DecGetPictureBuffersInfo(client->decoder, &info) != DEC_WAITING_FOR_BUFFER)
      break;
    if (info.buf_to_free.virtual_address != NULL) {
#ifndef ASIC_TRACE_SUPPORT
      for (i = 0; i < client->max_buffers; i++) {
        if (client->ext_buffers[i].virtual_address == info.buf_to_free.virtual_address) {
          DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
          break;
        }
      }
      DWLFreeLinear(client->dwl, &info.buf_to_free);
      ASSERT(i < client->max_buffers);
      if (i == client->max_buffers - 1) client->max_buffers--;
#else
      if (!client->test_params.pp_enabled) {
        for (i = 0; i < client->max_frm_buffers; i++) {
          if (client->ext_frm_buffers[i].virtual_address == info.buf_to_free.virtual_address) {
            DWLmemset(&client->ext_frm_buffers[i], 0, sizeof(client->ext_frm_buffers[i]));
            break;
          }
        }
        DWLFreeRefFrm(client->dwl, &info.buf_to_free);
        ASSERT(i < client->max_frm_buffers);
        if (i == client->max_frm_buffers - 1) client->max_frm_buffers--;
      } else {
        for (i = 0; i < client->max_buffers; i++) {
          if (client->ext_buffers[i].virtual_address == info.buf_to_free.virtual_address) {
            DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
            break;
          }
        }
        DWLFreeLinear(client->dwl, &info.buf_to_free);
        ASSERT(i < client->max_buffers);
        if (i == client->max_buffers - 1) client->max_buffers--;
      }
#endif
    }

    if (info.next_buf_size != 0) {
#ifndef ASIC_TRACE_SUPPORT
      mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
      mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
      SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP,
                    (client->test_params.decoder_mode & DEC_SECURITY) ? 1 : 0);
      if (DWLMallocLinear(client->dwl, info.next_buf_size, &mem) != DWL_OK)
        return -1;

      for (i = 0; i < client->max_buffers; i++) {
        if (DWL_DEVMEM_COMPARE(client->ext_buffers[i], DWL_DEVMEM_INIT)) {
          break;
        }
      }
      if (i == client->max_buffers) client->max_buffers++;
      ASSERT(i < MAX_BUFFERS);
      client->ext_buffers[i] = mem;
#else
      if (!client->test_params.pp_enabled) {
        mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
        mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
        SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_REFERENCE,
                      (client->test_params.decoder_mode & DEC_SECURITY) ? 1 : 0);
        if (DWLMallocRefFrm(client->dwl, info.next_buf_size, &mem) != DWL_OK)
          return -1;

        for (i = 0; i < client->max_frm_buffers; i++) {
          if (client->ext_frm_buffers[i].virtual_address == NULL) {
            break;
          }
        }
        if (i == client->max_frm_buffers) client->max_frm_buffers++;
        ASSERT(i < MAX_BUFFERS);
        client->ext_frm_buffers[i] = mem;
      } else {
        mem.mem_type = DWL_MEM_TYPE_DPB;
#ifdef SUPPORT_DMA
        mem.mem_type |= DWL_MEM_TYPE_DMA_DEVICE_ONLY;
#endif
        SET_MEM_USAGE(mem.mem_type, DWL_MEM_USAGE_OUT_PP,
                      (client->test_params.decoder_mode & DEC_SECURITY) ? 1 : 0);
        if (DWLMallocLinear(client->dwl, info.next_buf_size, &mem) != DWL_OK)
          return -1;
        for (i = 0; i < client->max_buffers; i++) {
          if (client->ext_buffers[i].virtual_address == NULL) {
            break;
          }
        }
        if (i == client->max_buffers) client->max_buffers++;
        ASSERT(i < MAX_BUFFERS);
        client->ext_buffers[i] = mem;
      }
#endif
      if (DecSetPictureBuffers(client->decoder, &mem, 1) != DEC_WAITING_FOR_BUFFER)
        break;
    }
  }
  return 0;
}

static void BufferDecodedCb(ClientInst inst, struct DecInput* buffer) {
  struct Client* client = (struct Client*)inst;
  if (!client->eos) {
    DispatchBufferForDecoding(client, buffer);
    sched_yield(); //for multi instance by multi pthread
  }
}

static void PictureReadyCb(ClientInst inst, struct DecPictures picture) {
  static char* pic_types[] = {"        IDR", "Non-IDR (P)", "Non-IDR (B)"};
  static char* avs2_types[] = {" I", " P", " B", " F", " S", " G", "GB"};
  struct Client* client = (struct Client*)inst;
  client->num_of_output_pics++;
  client->num_pics_to_display++;
  if (1 == client->num_of_output_pics) {
    DEBUG_PRINT(("[TB] Note: the mb number in cycles/mb is calculated by original size of stream, not\n"));
    DEBUG_PRINT(("[TB] final output size of PostProcessing!\n"));
  }
  if (BITSTREAM_AVS2 == client->demuxer.GetVideoFormat(client->demuxer.inst)) {
    DEBUG_PRINT(("[TB] PIC %2u/%2u, type %s,", client->num_of_output_pics,
                 picture.pictures[0].picture_info.pic_id,
                 avs2_types[picture.pictures[0].picture_info.pic_coding_type]));
  } else {
    DEBUG_PRINT(("[TB] PIC %2u/%2u, type %s,", client->num_of_output_pics,
                 picture.pictures[0].picture_info.pic_id,
                 pic_types[picture.pictures[0].picture_info.pic_coding_type]));
  }
  if (picture.pictures[0].picture_info.cycles_per_mb) {
    client->cycle_count += picture.pictures[0].picture_info.cycles_per_mb;
    DEBUG_PRINT(("[TB] %4u cycles / mb,", picture.pictures[0].picture_info.cycles_per_mb));
  }
#ifdef FPGA_PERF_AND_BW
  if (picture.pictures[0].picture_info.bitrate_4k60fps) {
     client->bitrate_count += picture.pictures[0].picture_info.bitrate_4k60fps/ 1024/ 1024;
     DEBUG_PRINT((" %4u Mbps (4k@60fps),", picture.pictures[0].picture_info.bitrate_4k60fps / 1024 / 1024));
  }


  if (picture.pictures[0].picture_info.bwrd_in_fs && picture.pictures[0].picture_info.bwwr_in_fs) {
     client->bw_rd_count += picture.pictures[0].picture_info.bwrd_in_fs;
     client->bw_wr_count += picture.pictures[0].picture_info.bwwr_in_fs;
     DEBUG_PRINT((" BW R/W: %0.3f/%0.3f (x FRAME 4k@60fps),",
                    (double)picture.pictures[0].picture_info.bwrd_in_fs/ 1000/ 2.157,
                    (double)picture.pictures[0].picture_info.bwwr_in_fs/ 1000/ 2.157));
  }
#endif
  DEBUG_PRINT(("%u x %u, Crop: (%u, %u), %u x %u %s\n",
               picture.pictures[0].sequence_info.pic_width,
               picture.pictures[0].sequence_info.pic_height,
               picture.pictures[0].sequence_info.crop_params.crop_left_offset,
               picture.pictures[0].sequence_info.crop_params.crop_top_offset,
               picture.pictures[0].sequence_info.crop_params.crop_out_width,
               picture.pictures[0].sequence_info.crop_params.crop_out_height,
               picture.pictures[0].picture_info.is_corrupted ? "CORRUPT" : ""));

#ifdef SUPPORT_SEI
  VcdSEIInfoReady(client, &picture);
#endif

#ifdef SUPPORT_GDR
  if (picture.pictures[0].is_gdr_frame)
    /* GDR process frame */
    DEBUG_PRINT(("[TB] GDR : Current frame is GDR frame.\n"));
#endif

  if (client->test_params.extra_output_thread) {
    struct DecPictures* copy = malloc(sizeof(struct DecPictures));
	if (copy == NULL) {
 	  printf("Failed to alloc memory for struct DecPicturePpu.\n");
	  return;
	}
    *copy = picture;
    FifoPush(client->pic_fifo, copy, FIFO_EXCEPTION_DISABLE);
  } else {
    PostProcessPicture(client, &picture);
  }
  client->num_pics_to_display--;
  sched_yield();
}

static void EndOfStreamCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;
  client->eos = 1;

  if (client->test_params.extra_output_thread) {
    /* We're done, wait for the output to Finish it's job. */
    FifoPush(client->pic_fifo, NULL, FIFO_EXCEPTION_DISABLE);
    pthread_join(client->parallel_output_thread, NULL);
  }
  DecRelease(client->decoder);
}

static void ReleasedCb(ClientInst inst) {
  struct Client* client = (struct Client*)inst;
  for (int i = 0; i < NUM_OF_STREAM_BUFFERS; i++) {
    if (DWL_GET_DEVMEM_ADDR(client->buffers[i].buffer)) {
      DWLFreeLinear(client->dwl, &client->buffers[i].buffer);
    }
  }
  if(client->cycle_count && client->num_of_output_pics)
    DEBUG_PRINT(("\n[TB] Average cycles/MB: %4u\n", client->cycle_count/client->num_of_output_pics));

#ifdef FPGA_PERF_AND_BW
  if(client->bitrate_count && client->num_of_output_pics)
     DEBUG_PRINT(("\n[TB] Average bitrate/frame (4k@60fps): %4u Mbps\n", (client->bitrate_count/client->num_of_output_pics)));

  if(client->bw_rd_count && client->bw_wr_count && client->num_of_output_pics){
     DEBUG_PRINT(("\n[TB] Average BW R/W (x FRAME 4k@60fps): %0.3f / %0.3f (%0.3f) \n",
                (double)client->bw_rd_count/ client->num_of_output_pics/ 1000/ 2.157,
                (double)client->bw_wr_count/ client->num_of_output_pics/ 1000/ 2.157,
                (double)(client->bw_wr_count + client->bw_rd_count)/ client->num_of_output_pics/ 1000/ 2.157));
  }
#endif
  for (i32 i = 0; i < client->max_buffers; i++) {
    if (DWL_DEVMEM_VAILD(client->ext_buffers[i])) {
      DWLFreeLinear(client->dwl, &client->ext_buffers[i]);
      DWLmemset(&client->ext_buffers[i], 0, sizeof(client->ext_buffers[i]));
    }
  }
  client->max_buffers = 0;
#ifdef ASIC_TRACE_SUPPORT
  for (i32 i = 0; i < client->max_frm_buffers; i++) {
    if (client->ext_frm_buffers[i].virtual_address != NULL) {
      DWLFreeRefFrm(client->dwl, &client->ext_frm_buffers[i]);
      DWLmemset(&client->ext_frm_buffers[i], 0, sizeof(client->ext_frm_buffers[i]));
    }
  }
  client->max_frm_buffers = 0;
#endif

  if (client->test_params.mc_enable &&
     (client->demuxer.GetVideoFormat(client->demuxer.inst) == BITSTREAM_H264)) {

  }

  if(client->t35_hdr_parma.t35_hdr10plus) {
    DWLfree(client->t35_hdr_parma.t35_hdr10plus);
  }
  if(client->t35_hdr_parma.t35_dobly_vision) {
    DWLfree(client->t35_hdr_parma.t35_dobly_vision);
  }

#ifdef ASIC_ONL_SIM
  client->dec_done = 1;
#else
  sem_post(&client->dec_done);
#endif
}

static void NotifyErrorCb(ClientInst inst, u32 pic_id, enum DecRet rv) {
  struct Client* client = (struct Client*)inst;

  if (!client->dec_initialized) {
    fprintf(stderr, "[TB] Decoder initialize error: %s [%d]\n", VCDecRetStr(rv), rv);
    DecRelease(client->decoder);
  } else {
    fprintf(stderr, "[TB] Decoding error on pic_id %u: %s [%d]\n", pic_id, VCDecRetStr(rv), rv);
    if (rv == DEC_MEMFAIL ||
        rv == DEC_PARAM_ERROR ||
        rv == DEC_INFOPARAM_ERROR ||
        rv == DEC_SYSTEM_ERROR ||
        rv == DEC_FATAL_SYSTEM_ERROR ||
        rv == DEC_HW_EXT_TIMEOUT) {
      if (rv == DEC_SYSTEM_ERROR) {
        /* TODO: Process fatal system error here, such as reset HW. */
      }
      /* There's serious decoding error, so we'll consider it as end of stream to
         get the pending pictures out of the decoder. */
      DispatchEndOfStream(client);
    }
  }
}

static void PostProcessPicture(struct Client* client,
                               struct DecPictures* picture) {
  struct DecPicture* in = &picture->pictures[0];
  u32 i, only_once = 0, ext_id = 0xFFFF;
  static u32 pic_count = 0;
  av_unused struct TOOL_PARAMS tool_params = {client->ext_buffers, &client->max_buffers, client->dwl};
  u32* host_base = NULL;

  pic_count++;
  if (pic_count == 10)
    pic_count = 1;

  //fwrite((u8 *)in.dmv.virtual_address, 1, in.dmv.logical_size, fp_dmv);
  //fwrite((u8 *)in.qp.virtual_address, 1, in.qp.logical_size, fp_qp);
  for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
    in = &picture->pictures[i];
    if (in->luma.bus_address == 0)
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

    /* Current output is available */
    if (client->test_params.sink_type == SINK_NULL /*|| pic_count != 2*/) goto PIC_CONSUMED;
    client->yuvsink->WritePicture(client->yuvsink->inst, in, i);
    SwClearHostOutbaseAfterWriteFile(host_base, in);
  }
  if (host_base != NULL) {
    DWLfree(host_base);
    tool_params.ext_buffers[ext_id].virtual_address = NULL;
  }

PIC_CONSUMED:
  DecPictureConsumed(client->decoder, picture);
}

static void* ParallelOutput(void* arg) {
  struct Client* client = arg;
  struct DecPictures* pic = NULL;
  do {
    FifoPop(client->pic_fifo, (void**)&pic, FIFO_EXCEPTION_DISABLE);
    if (pic == NULL) {
      DEBUG_PRINT(("[TB] END-OF-STREAM received in output thread\n"));
      return NULL;
    }
    PostProcessPicture(client, pic);
  } while (1);
  return NULL;
}

static const void* CreateDemuxer(struct Client* client) {
  Demuxer demuxer;
  enum FileFormat ff = client->test_params.file_format;
  if (ff == FILEFORMAT_AUTO_DETECT) {
    if (strstr(client->test_params.in_file_name, ".ivf") ||
        strstr(client->test_params.in_file_name, ".vp9") ||
        strstr(client->test_params.in_file_name, ".av1"))
      ff = FILEFORMAT_IVF;
    else if (strstr(client->test_params.in_file_name, ".webm"))
      ff = FILEFORMAT_WEBM;
    else if ((strstr(client->test_params.in_file_name, ".avs")) ||
              (strstr(client->test_params.in_file_name, ".avs2")))
      ff = FILEFORMAT_AVS2;
    else if (strstr(client->test_params.in_file_name, ".hevc") ||
             strstr(client->test_params.in_file_name, ".h265") ||
             strstr(client->test_params.in_file_name, ".265"))
      ff = FILEFORMAT_BYTESTREAM_HEVC;
    else if (strstr(client->test_params.in_file_name, ".vvc") ||
             strstr(client->test_params.in_file_name, ".h266") ||
             strstr(client->test_params.in_file_name, ".266"))
      ff = FILEFORMAT_BYTESTREAM_VVC;
    else if (strstr(client->test_params.in_file_name, ".avc")  ||
             strstr(client->test_params.in_file_name, ".h264") ||
             strstr(client->test_params.in_file_name, ".264"))
      ff = FILEFORMAT_BYTESTREAM_H264;
  }
  if (ff == FILEFORMAT_IVF || ff == FILEFORMAT_WEBM) {
    demuxer.Open = VpxRdrOpen;
    demuxer.GetVideoFormat = VpxRdrIdentifyFormat;
    demuxer.HeadersDecoded = VpxRdrHeadersDecoded;
    demuxer.ReadPacket = VpxRdrReadFrame;
    demuxer.Close = VpxRdrClose;
  } else if (ff == FILEFORMAT_AVS2) {
    demuxer.Open = ByteStreamParserOpen;
    demuxer.GetVideoFormat = ByteStreamParserIdentifyFormatAvs2;
    demuxer.ReadPacket = ByteStreamParserReadFrameAvs2;
    demuxer.HeadersDecoded = ByteStreamParserHeadersDecoded;
    demuxer.Close = ByteStreamParserClose;
  } else if (ff == FILEFORMAT_BYTESTREAM_HEVC || ff == FILEFORMAT_BYTESTREAM_VVC) {
    demuxer.Open = ByteStreamParserOpen;
    if (ff == FILEFORMAT_BYTESTREAM_VVC) {
      demuxer.GetVideoFormat = ByteStreamParserIdentifyFormatVvc;
      demuxer.ReadPacket = ByteStreamParserReadFrameVvc;
    } else {
      demuxer.GetVideoFormat = ByteStreamParserIdentifyFormat;
      demuxer.ReadPacket = ByteStreamParserReadFrame;
    }
    demuxer.HeadersDecoded = ByteStreamParserHeadersDecoded;
    demuxer.Close = ByteStreamParserClose;
  } else if (ff == FILEFORMAT_BYTESTREAM_H264) {
    demuxer.Open = ByteStreamParserOpen;
    demuxer.GetVideoFormat = ByteStreamParserIdentifyFormatH264;
    demuxer.HeadersDecoded = ByteStreamParserHeadersDecoded;
    demuxer.ReadPacket = ByteStreamParserReadFrameH264;
    demuxer.Close = ByteStreamParserClose;
  } else {
    /* TODO(vmr): In addition to file suffix, consider also looking into
     *            shebang of the files. */
    return NULL;
  }

  demuxer.inst = demuxer.Open(client->test_params.in_file_name,
                              client->test_params.index_file_name,
                              client->test_params.read_mode,
                              (client->test_params.decoder_mode & DEC_LOW_LATENCY) ? 1 : 0);
  // /* If needed, instantiate error simulator to process data from demuxer. */
  // if (ErrorSimulationNeeded(client))
  //   demuxer.inst =
  //     ErrorSimulatorInject(&demuxer, client->test_params.error_sim); //Mark for klocwork: It's freed -- By Chen Min.
  client->demuxer = demuxer;
  return demuxer.inst;
}

static void ReleaseDemuxer(struct Client* client) {
  client->demuxer.Close(client->demuxer.inst);
}

// static u8 ErrorSimulationNeeded(struct Client* client) {
//   return client->test_params.error_sim.corrupt_headers ||
//          client->test_params.error_sim.truncate_stream ||
//          client->test_params.error_sim.swap_bits_in_stream ||
//          client->test_params.error_sim.lose_packets;
// }

/* TODO H264/HEVC slice mode + L2 cache only need one buffer */
static i32 GetStreamBufferCount(struct Client* client) {
  return (client->test_params.read_mode == STREAMREADMODE_FULLSTREAM ||
          client->test_params.read_mode == STREAMREADMODE_PACKETIZE)
         ? 1
         : NUM_OF_STREAM_BUFFERS;
}

/* These global values are found from commonconfig.c */
extern u32 dec_stream_swap;
extern u32 dec_pic_swap;
extern u32 dec_dirmv_swap;
extern u32 dec_burst_length;
extern u32 dec_bus_width;
extern u32 dec_apf_treshold;
extern u32 dec_apf_disable;
extern u32 dec_clock_gating;
extern u32 dec_timeout_cycles;
extern u32 dec_axi_id_rd;
extern u32 dec_axi_id_rd_unique_enable;
extern u32 dec_axi_id_wr;
extern u32 dec_axi_id_wr_unique_enable;
/* partial tb_cfg.dec_params parameters. */
extern struct DecParams dec_params;
#ifdef USE_RANDOM_ERROR_TEST
/* These global values are found from vcdecapi.c.
 * partial tb_cfg.tb_params parameters for random error injectionn. */
extern struct ErrorParams random_error_params;
#endif

static void SetClientByTBCfg(struct Client* client)
{
  client->test_params.rlc_mode = TBGetDecRlcModeForced(&tb_cfg);

#if 1
#ifdef USE_RANDOM_ERROR_TEST
  random_error_params.seed = tb_cfg.tb_params.seed_rnd;
  strcpy(random_error_params.truncate_stream_odds, tb_cfg.tb_params.stream_truncate);
  strcpy(random_error_params.swap_bit_odds, tb_cfg.tb_params.stream_bit_swap);
  strcpy(random_error_params.packet_loss_odds, tb_cfg.tb_params.stream_packet_loss);
#endif
#else
   /* Read the error simulation parameters. */
  struct ErrorSimulationParams* error_params = &client->test_params.error_sim;
  error_params->seed = tb_cfg.tb_params.seed_rnd;
  // error_params->truncate_stream = TBGetTBStreamTruncate(&tb_cfg);
  error_params->corrupt_headers = TBGetTBStreamHeaderCorrupt(&tb_cfg);
  if (strcmp(tb_cfg.tb_params.stream_truncate, "0") != 0) {
    error_params->truncate_stream = 1;
    memcpy(error_params->truncate_stream_odds, tb_cfg.tb_params.stream_truncate,
           sizeof(tb_cfg.tb_params.stream_truncate));
  }
  if (strcmp(tb_cfg.tb_params.stream_bit_swap, "0") != 0) {
    error_params->swap_bits_in_stream = 1;
    memcpy(error_params->swap_bit_odds, tb_cfg.tb_params.stream_bit_swap,
           sizeof(tb_cfg.tb_params.stream_bit_swap));
  }
  if (strcmp(tb_cfg.tb_params.stream_packet_loss, "0") != 0) {
    error_params->lose_packets = 1;
    memcpy(error_params->packet_loss_odds, tb_cfg.tb_params.stream_packet_loss,
           sizeof(tb_cfg.tb_params.stream_packet_loss));
  }

#ifndef USE_RANDOM_ERROR_TEST
  if (error_params->swap_bits_in_stream || error_params->lose_packets ||
      error_params->truncate_stream || error_params->corrupt_headers) {
    /* tb random error inject: do nothing */
  } else {
    srand(time(0)); /* set random seed for rand(). */
  }
#endif
  DEBUG_PRINT(("[TB] TB Seed Rnd %u\n", error_params->seed));
  DEBUG_PRINT(("[TB] TB Stream Truncate %d\n", error_params->truncate_stream));
  DEBUG_PRINT(("[TB] TB Stream Header Corrupt %d\n", error_params->corrupt_headers));
  DEBUG_PRINT(("[TB] TB Stream Bit Swap %d; odds %s\n",
               error_params->swap_bits_in_stream, error_params->swap_bit_odds));
  DEBUG_PRINT(("[TB] TB Stream Packet Loss %d; odds %s\n",
               error_params->lose_packets, error_params->packet_loss_odds));
#endif
}

static void OpenTestHooks(const struct Client* client) {
  /* set test bench configuration */
  TBSetDefaultCfg(&tb_cfg);
  char *tb_path = (client->test_params.in_tb_cfg_file_name == NULL) ? "tb.cfg" : client->test_params.in_tb_cfg_file_name;
  FILE* f_tbcfg = fopen(tb_path, "r");
  if (f_tbcfg == NULL) {
    DEBUG_PRINT(("[TB] UNABLE TO OPEN INPUT FILE: \"tb.cfg of %s\"\n", tb_path));
    DEBUG_PRINT(("[TB] USING DEFAULT CONFIGURATION\n"));
  } else {
    fclose(f_tbcfg);
    if (TBParseConfig(tb_path, TBReadParam, &tb_cfg) == TB_FALSE) return;
    if (TBCheckCfg(&tb_cfg) != 0) return;
  }

#ifdef MODEL_SIMULATION
  g_hw_ver = tb_cfg.dec_params.hw_version;
  g_hw_id = tb_cfg.dec_params.hw_build;
  g_hw_build_id = tb_cfg.dec_params.hw_build_id;
  if (client->test_params.align && client->test_params.align_h) {
    alignwidth = 1 << client->test_params.align;
    alignheight = 1 << client->test_params.align_h;
  }
  in_lib_name = client->test_params.in_lib_name;
#endif

#if 0
  client->test_params.rlc_mode = TBGetDecRlcModeForced(&tb_cfg);
#endif

  if (client->test_params.trace_target) tb_cfg.tb_params.extra_cu_ctrl_eof = 1;

  if (client->test_params.hw_traces) {
#ifdef ASIC_TRACE_SUPPORT
    if (!OpenTraceFiles())
      DEBUG_PRINT(
        ("[TB] UNABLE TO OPEN TRACE FILE(S) Do you have a trace.cfg "
         "[TB] file?\n"));
#else
    DEBUG_PRINT(
        ("[TB] UNABLE TO OPEN TRACE FILE(S) "
         "[TB] Do you enable ASIC_TRACE_SUPPORT when building?\n"));
#endif
  }

  if (f_tbcfg != NULL) {
    dec_stream_swap = tb_cfg.dec_params.strm_swap;
    dec_pic_swap = tb_cfg.dec_params.pic_swap;
    dec_dirmv_swap = tb_cfg.dec_params.dirmv_swap;
    dec_burst_length = tb_cfg.dec_params.max_burst;
    dec_bus_width = TBGetDecBusWidth(&tb_cfg);
    dec_apf_treshold = tb_cfg.dec_params.apf_threshold_value;
    dec_apf_disable = tb_cfg.dec_params.apf_disable;
    dec_clock_gating = TBGetDecClockGating(&tb_cfg);
    dec_timeout_cycles = tb_cfg.dec_params.timeout_cycles;
    dec_axi_id_rd = tb_cfg.dec_params.axi_id_rd;
    dec_axi_id_rd_unique_enable = tb_cfg.dec_params.axi_id_rd_unique_enable;
    dec_axi_id_wr = tb_cfg.dec_params.axi_id_wr;
    dec_axi_id_wr_unique_enable = tb_cfg.dec_params.axi_id_wr_unique_enable;

    /* this dec_params used in commonconfig.c */
    /* set parameters with dec_params */
    dec_params.bus_burst_length = tb_cfg.dec_params.bus_burst_length;
    dec_params.clk_gate_decoder = tb_cfg.dec_params.clk_gate_decoder;
    dec_params.non_seq_clk = tb_cfg.dec_params.non_seq_clk;
    dec_params.seq_clk = tb_cfg.dec_params.seq_clk;
    dec_params.apf_threshold_value = tb_cfg.dec_params.apf_threshold_value;
    dec_params.axi_wr_outstand = tb_cfg.dec_params.axi_wr_outstand;
    dec_params.axi_rd_outstand = tb_cfg.dec_params.axi_rd_outstand;

    ResolvePpParamsOverlap((struct TestParams *)&client->test_params, !tb_cfg.pp_params.pipeline_e);
  }

#ifdef ASIC_TRACE_SUPPORT
  /* determine test case id from input file name (if contains "case_") */
  if (client->test_params.in_file_name) {
    char* pc, *pe;
    char in[256] = {0};
    strncpy(in, client->test_params.in_file_name,
            strnlen(client->test_params.in_file_name, 256));
    pc = strstr(in, "case_");
    if (pc != NULL) {
      pc += 5;
      pe = strstr(pc, "/");
      if (pe == NULL) pe = strstr(pc, ".");
      if (pe != NULL) {
        *pe = '\0';
        test_case_id = atoi(pc);
      }
    }
  }
#endif
}

static void CloseTestHooks(struct Client* client) {
  if (client->test_params.hw_traces) {
#ifdef ASIC_TRACE_SUPPORT
    CloseTraceFiles();
#endif
  }
}

static int HwconfigOverride(DecInst dec_inst, struct TBCfg* tbcfg) {
  u32 data_discard = TBGetDecDataDiscard(&tb_cfg);
  u32 latency_comp = tb_cfg.dec_params.latency_compensation;
  u32 output_picture_endian = TBGetDecOutputPictureEndian(&tb_cfg);
  u32 bus_burst_length = tb_cfg.dec_params.bus_burst_length;
  u32 asic_service_priority = tb_cfg.dec_params.asic_service_priority;
  u32 service_merge_disable = TBGetDecServiceMergeDisable(&tb_cfg);

  DEBUG_PRINT(("[TB] struct TBCfg: Decoder Data Discard %u\n", data_discard));
  DEBUG_PRINT(("[TB] struct TBCfg: Decoder Latency Compensation %u\n", latency_comp));
  DEBUG_PRINT(
    ("[TB] struct TBCfg: Decoder Output Picture Endian %u\n", output_picture_endian));
  DEBUG_PRINT(("[TB] struct TBCfg: Decoder Bus Burst Length %u\n", bus_burst_length));
  DEBUG_PRINT(
    ("[TB] struct TBCfg: Decoder Asic Service Priority %u\n", asic_service_priority));
  DEBUG_PRINT(
    ("[TB] struct TBCfg: Decoder Service Merge Disable %u\n", service_merge_disable));

  return 0;
}

void StreamBufferConsumedMC(void *stream, void *p_user_data) {
  int i;
  int found = 0;

  if (p_user_data == NULL)
    return;

  pthread_mutex_lock(&mutex);
  struct Client* client = (struct Client*)p_user_data;
  for (i = 0; i < GetStreamBufferCount(client); i++) {
    if ((u8 *)stream >= (u8 *)client->buffers[i].buffer.virtual_address &&
        (u8 *)stream < (u8 *)client->buffers[i].buffer.virtual_address + client->buffers[i].buffer.size) {
      found = 1;
      break;
    }
  }

  if (found == 0)
    printf("[TB] Stream buffer not found.\n");

  BufferDecodedCb(client, &client->buffers[i]);
  pthread_mutex_unlock(&mutex);
}

static int ComParseStreamCfg(char *streamcfg, struct Client *client)
{
  u32 len = strlen(streamcfg);
  char *p = NULL;
  i32 ret = 0, i = 0;
  SetupDefaultParams(&client->test_params);
  if(len < 1) {
    ret = NOK;
    return ret;
  }
  client->argv = (char **)malloc(MAXARGS*sizeof(char *));
  if (client->argv == NULL){
    ret = NOK;
    return ret;
  }
  client->argv[0] = (char *)malloc(len + 1);
  if (client->argv[0] == NULL){
    free(client->argv);
    ret = NOK;
    return ret;
  }
  memcpy(client->argv[0],streamcfg,len);
  p = client->argv[0];
  client->argv[0][len] = 0;
  for(i = 1; i < MAXARGS; i++) {
    while(p - client->argv[0] < len && *p && *p <= 32)
      ++p;
    if(p - client->argv[0] >= len || !*p) break;
    client->argv[i] = p;
    while(*p > 32)
      ++p;
    *p = 0; ++p;
  }
  client->argc = i;
  if (ParseParams(client->argc, client->argv, &client->test_params))
  {
#ifndef WIN32
    Error(2, ERR, "Input parameter error");
#endif
    free(client->argv[0]);
    free(client->argv);
    return NOK;
  }

  free(client->argv[0]);
  client->argv[0] = NULL;
  free(client->argv);
  client->argv = NULL;

  return OK;
}

#ifdef SUPPORT_SEI
void VcdSEIInit(struct Client * client) {
  /* SEI_buffer : this for test, it can be removed */
  u32 i = 0, pay_load_type = 0;
  u8 tmp;
  if (0) {
    /* initialize */
    DWLmemset(&client->sei_buffer, 0, sizeof(struct SEI_buffer));
    for (i = 0; i < 32; i++)
      client->sei_buffer.bitmask[i] = 0x00;
    /* sei_type = pay_load_type */
    pay_load_type = 4; /* 4 */
    tmp = *(client->sei_buffer.bitmask + (pay_load_type >> 3));
    tmp = ((tmp | 0x01) << (pay_load_type & 7));
    *(client->sei_buffer.bitmask + pay_load_type / 8) = tmp;

    client->sei_buffer.total_size = 2048;
    client->sei_buffer.buffer = DWLmalloc(client->sei_buffer.total_size);
    DWLmemset(client->sei_buffer.buffer, 0, client->sei_buffer.total_size);
    client->sei_buffer.available_size = 1;
  } else {
    for (i = 0; i < 32; i++)
      client->sei_buffer.bitmask[i] = 0x00;
    client->sei_buffer.total_size = 0;
    client->sei_buffer.buffer = NULL;
    client->sei_buffer.available_size = 0;
  }

  /* t35 hdr */
  client->t35_hdr_parma.t35_hdr10plus = (T35_HDR10Plus *)DWLcalloc(MAX_PAYLOAD_NUM, sizeof(T35_HDR10Plus));
  if(!client->t35_hdr_parma.t35_hdr10plus) {
    DEBUG_PRINT(("[TB] t35_hdr10plus calloc failed\n"));
  }
  client->t35_hdr_parma.t35_dobly_vision = (T35_Dobly_Vision *)DWLcalloc(MAX_PAYLOAD_NUM, sizeof(T35_Dobly_Vision));
  if(!client->t35_hdr_parma.t35_dobly_vision) {
    DEBUG_PRINT(("[TB] t35_dobly_vision calloc failed\n"));
  }
}

static void HevcSEIInfoReady(struct Client* client, struct HevcSEIParameters *hevc_sei,
                             u32 pic_id) {
  if ((hevc_sei != NULL) &&
      (hevc_sei->decode_id == pic_id) &&
      (hevc_sei->bufperiod_present_flag ||
       hevc_sei->pictiming_present_flag ||
       hevc_sei->t35_present_flag ||
       hevc_sei->userdata_unreg_present_flag ||
       hevc_sei->recovery_point_present_flag ||
       hevc_sei->active_ps_present_flag ||
       hevc_sei->mastering_display_present_flag ||
       hevc_sei->lightlevel_present_flag)) {
    DEBUG_PRINT(("[TB] SEI successful: got the SEI(HEVC), which belongs to PIC %2u/%2u \n",
                  client->num_of_output_pics, pic_id));
    /* User can add function to process this SEI in here. */
    if(hevc_sei->t35_present_flag) {
      VcdParseT35HDR(&hevc_sei->t35_param, &client->t35_hdr_parma);
      DEBUG_PRINT(("[TB] VcdParseT35HDR successful: got the hdr10plus_counter %u, dobly_vision_counter %u \n",
                  client->t35_hdr_parma.hdr10plus_counter, client->t35_hdr_parma.dobly_vision_counter));
    }
  }
  // else {
  //   DEBUG_PRINT(("[TB] SEI : no SEI(HEVC) of interest for current PIC %2d/\n",
  //                 client->num_of_output_pics));
  // }
}

static void H264SEIInfoReady(struct Client* client, struct H264SEIParameters *h264_sei[],
                             u32 pic_id, u32 is_interlaced) {
  u32 i = 0;
  u32 index = is_interlaced ? 2 : 1;
  for(i=0; i<index; i++) {
    struct H264SEIParameters *p_h264_sei = h264_sei[i];
    if ((p_h264_sei != NULL)) {
      u32 sei_decode_id = p_h264_sei->decode_id;
      if(i==0 && is_interlaced)
        sei_decode_id += 1;
      if ((sei_decode_id == pic_id) &&
         (p_h264_sei->bufperiod_present_flag ||
         p_h264_sei->pictiming_present_flag ||
         p_h264_sei->t35_present_flag ||
         p_h264_sei->userdata_unreg_present_flag ||
         p_h264_sei->recovery_point_present_flag ||
         p_h264_sei->mastering_display_present_flag ||
         p_h264_sei->lightlevel_present_flag)) {
        DEBUG_PRINT(("[TB] SEI successful: got the SEI(H264) from sei_param.h264[%u], which belongs to PIC %2u/%2u \n", i,
                      client->num_of_output_pics, p_h264_sei->decode_id));
      }
      /* User can add function to process this SEI in here. */
      if(p_h264_sei->t35_present_flag) {
        VcdParseT35HDR(&p_h264_sei->t35_param, &client->t35_hdr_parma);
        DEBUG_PRINT(("[TB] VcdParseT35HDR successful: got the hdr10plus_counter %u, dobly_vision_counter %u from sei_param.h264[%u]\n", i,
                    client->t35_hdr_parma.hdr10plus_counter, client->t35_hdr_parma.dobly_vision_counter));
      }
    }
    // else {
    //   DEBUG_PRINT(("[TB] SEI : no SEI(H264) of interest for current PIC %2d/\n",
    //                 client->num_of_output_pics));
    // }
  }
}

static void Av1MetadataInfoReady(struct Client* client, struct MetadataParameters *av1_metadata,
                                 u32 pic_id) {
  if ((av1_metadata != NULL) &&
      (av1_metadata->decode_id == pic_id) &&
      (av1_metadata->t35_present_flag ||
       av1_metadata->mastering_display_present_flag ||
       av1_metadata->lightlevel_present_flag)) {
    DEBUG_PRINT(("[TB] METADATA successful: got the METADATA(AV1), which belongs to PIC %2u/%2u \n",
                  client->num_of_output_pics, pic_id));
    /* User can add function to process this METADATA in here. */
    if(av1_metadata->t35_present_flag) {
      VcdParseT35HDR(&av1_metadata->t35_param, &client->t35_hdr_parma);
      DEBUG_PRINT(("[TB] VcdParseT35HDR successful: got the hdr10plus_counter %u, dobly_vision_counter %u \n",
                  client->t35_hdr_parma.hdr10plus_counter, client->t35_hdr_parma.dobly_vision_counter));
    }
  }
  // else {
  //   DEBUG_PRINT(("[TB] METADATA : no METADATA(AV1) of interest for current PIC %2d/\n",
  //                 client->num_of_output_pics));
  // }
}

static void VcdSEIInfoReady(struct Client* client, struct DecPictures *picture) {
  struct DecSEIParameters sei_param = picture->pictures[0].sei_param;
  u32 pic_id = picture->pictures[0].picture_info.pic_id;
  u32 bitstream_format = client->demuxer.GetVideoFormat(client->demuxer.inst);

  /* SEI parser data */
  switch (bitstream_format) {
    case BITSTREAM_HEVC: {
      HevcSEIInfoReady(client, sei_param.hevc, pic_id);
      break;
    }
    case BITSTREAM_H264: {
      u32 is_laced = picture->pictures[0].sequence_info.is_interlaced;
      H264SEIInfoReady(client, sei_param.h264, pic_id, is_laced);
      break;
    }
    case BITSTREAM_AV1:
    case BITSTREAM_AV1_ANNEXB:
    case BITSTREAM_AV1_OBU: {
      Av1MetadataInfoReady(client, sei_param.av1, pic_id);
      break;
    }
    case BITSTREAM_VP9:
    case BITSTREAM_VP7:
    case BITSTREAM_AVS2:
    default:
      // DEBUG_PRINT(("SEI : Don't support this decoder.\n"));
      break;
  }
}
#endif
