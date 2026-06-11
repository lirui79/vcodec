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

#ifndef __COMMAND_LINE_PARSER_H__
#define __COMMAND_LINE_PARSER_H__

#include "basetype.h"
#include "decapicommon.h"
#include "commonconfig.h"
#include "error_simulator.h"
#include "dectypes.h"
#include "tb_defs.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_STREAMS 16

enum {
  VP8DEC_DECODER_ALLOC = 0,
  VP8DEC_EXTERNAL_ALLOC = 1,
  VP8DEC_EXTERNAL_ALLOC_ALT = 2
};

enum FileFormat {
  FILEFORMAT_AUTO_DETECT = 0,
  FILEFORMAT_BYTESTREAM_HEVC,
  FILEFORMAT_IVF,
  FILEFORMAT_WEBM,
  FILEFORMAT_BYTESTREAM_H264,
  FILEFORMAT_AVS2,
  FILEFORMAT_BYTESTREAM_VVC,
  FILEFORMAT_MAX
};

enum StreamReadMode {
  STREAMREADMODE_FRAME = 0,
  STREAMREADMODE_NALUNIT = 1,
  STREAMREADMODE_FULLSTREAM = 2,
  STREAMREADMODE_PACKETIZE = 3
};

enum SinkType {
  SINK_FILE_SEQUENCE = 0,
  SINK_FILE_PICTURE,
  SINK_MD5_SEQUENCE,
  SINK_MD5_PICTURE,
  SINK_MD5_VTM,
  SINK_SDL,
  SINK_CRC_SEQUENCE,
  SINK_NULL
};

enum TestBenchFormat {
  H264DEC = 0x1,
  MPEG4DEC = 0x2,
  JPEG_DEC = 0x4,
  VC1DEC = 0x8,
  MPEG2DEC = 0x10,
  VP6DEC = 0x20,
  RVDEC = 0x40,
  VP8DEC = 0x80,
  AVSDEC = 0x100,
  G2DEC = 0x200,  /* hevc/vvc/vp9/av1/avs2 */
  PPDEC = 0x400,
  ALLFORMATS = 0xFFFFFFF
};

struct TestParams {
  char* in_tb_cfg_file_name;
  char in_file_name[1024];
  char* out_file_name[2*DEC_MAX_OUT_COUNT];
  u32 num_of_decoded_pics;
  i8 max_temporal_layer; /**< The maximum temporal layer that the VCD supports decoding. */
  enum DecPictureFormat hw_format;
  enum SinkType sink_type;
  u32 pp_enabled;
  u8 hw_traces;
  u8 trace_target;
  u8 extra_output_thread;
  u8 disable_display_order;
  enum DecSkipFrameMode skip_frame; /* Skip some frames when decoding. */
  char* index_file_name; /* Use index file get stream length to decoder. */
  u32 input_buffer_size;
  enum DecErrorHandling error_handling; /**< Error handling mode. */
  u32 error_ratio; /**< Error ratio, used for DEC_EC_OUT_DECISION. */
  u8 crop_align; /** 0: 1 pixel align, 1: 2 pixel align **/
  char* table_3dlut_name; /**< for color remapping (3dlut). */
  char *in_lib_name; /**< \brief input library file*/

  /* RFC formats only */
  char in_luma_file[1024];
  char in_chroma_file[1024];
  char in_luma_table_file[1024];
  char in_chroma_table_file[1024];
  char in_table_file[1024];
  /* G1 formats only */
  u32 convert_tiled_output; /* Convert tiled output pictures to raster scan */
  u32 save_index; /* Save index file */
  u32 num_buffers; /* To use n frame buffers in decoder specified by user */
  enum DecDpbMode dpb_mode; /* DPB stores interlaced content as fields (default: frames) */
  u32 convert_to_frame_dpb; /* Convert output to frame mode even if field DPB mode used */
  u32 use_extra_buffers; /* Add extra external buffer randomly */
  u32 allocate_extra_buffers_in_output; /* Add extra external buffer in ouput thread */
  char *stream_trace; /* file.hex stream control trace file */
  u32 use_ref_idct; /* Use reference idct (implies cropping) */
  /* H264 only */
  u32 support_asofmo_stream; /* 1: aso/fmo stream, 0: non-aso/fmo stream */

  /* JPEG only */
  u8 only_full_resolution;  /* jpeg full resolution only without thumbnail */
  u8 ri_mc_enable; /* restart interval based on multicore decoding (JPEG) */
  u32 instant_buffer; /* jpeg output buffer provided by user */

  /* VP8 only */
  u32 snap_shot; /* Set decode format to webp */
  i32 extra_strm_buffer_bits; /* Add n bytes of extra space after stream buffer for decoder */
  u32 user_mem_alloc; /* User allocates picture buffers */

  /* VP6 only */
  u32 alpha; /* Stream contains alpha channel */

  /* MPEG4 only */
  enum DecInputFormat strm_fmt_sorenson; /* Decode Sorenson Spark stream */
  enum DecInputFormat strm_fmt_custom; /* Decode DivX4 or DivX5 stream */
  struct {
    u32 dimensions;  /* Implicates that raw bitstream does not carry frame dimensions */
    char* widthxheight;    /* cropping width and height */
  } custom; /* Decode DivX3 stream of resolution width x height\n") */

  /* VC1 only */
  u32 long_stream; /* Enable support for long streams. */
  u32 enable_frame_picture; /* Enable frame picture writing in multiresolutin output. */

  /* RV only */
  u32 rv_input; /* Input file in RealVideo format */

  enum FileFormat file_format;
  enum StreamReadMode read_mode;
  struct ErrorSimulationParams error_sim;
  enum DecDecoderMode decoder_mode;
  PpUnitConfig ppu_cfg[DEC_MAX_OUT_COUNT];
  DelogoConfig delogo_params[2];
  DecPicAlignment align;
  DecPicAlignment align_h;
  u8 compress_bypass;   /* compressor bypass flag */
  u8 is_ringbuffer;     /* ringbuffer mode by default */
  u32 pp_standalone;    /* PP in standalone mode */
  /*only used for rtl simulation*/
  u32 out_format_conv;   /* output format conversion enabled in TB (via -T/-S/-P options) */
  u32 mc_enable;
  u32 mvc;              /* MVC for H264. */

  /* for AV1 compatability */
  unsigned ref_cache_size;
  bool use_pdec_for_dump;
  bool print_hw_perf;
  u32 default_bs;
  bool tile_transpose;
  u32 oppoints;
  u32 hw_conceal;
  u32 auxinfo;
  u32 disable_slice;

  u32 rgb_stan;
  u32 rgb_alpha;
  u32 video_range;
  u32 rlc_mode;
  u32 pp_filter;
  u32 x_filter_param;
  u32 y_filter_param;
  u32 low_cost_enable;
  /*for Multi-stream*/
  i32 nstream;
  i32 nstrmcfg;
  i32 multimode;  // Multi-stream mode, 0--disable, 1--mult-thread //Next: 2--multi-process
  char *streamcfg[MAX_STREAMS];
  union {
	  int pid[MAX_STREAMS];

	  pthread_t *tid[MAX_STREAMS];

  } multi_stream_id;
  /* hantrodec memalloc select */
  char *dec_dev;
  char *mem_dev;
#ifdef SUPPORT_DMA
  /* dma select */
  char *dma_dev;
#endif

#ifdef SUPPORT_RANDOM_LATENCY
  /* set axi latency for FPGA test */
  u32 axi_lg_r;
  u32 axi_lg_w;
#endif

  /* for logmsg */
  u32 logOutDir;
  u32 logOutLevel;
  u32 logTraceMap;
  u32 logCheckMap;

  /* for multi-crop*/
  u8 crop_id;
};

void PrintUsage(char* executable, enum TestBenchFormat fmt);
void SetupDefaultParams(struct TestParams* params);
int ParseParams(int argc, char* argv[], struct TestParams* params);
int ResolveOverlap(struct TestParams* params);
int ResolvePpParamsOverlap(struct TestParams* params,
                           u32 standalone);

/* for log trace */
int ParseLogTraceMapByName(char str[]);
int ParseLogTraceMap(char str[]);
u32 FP32TOU32(float f);
void dump3DLutData(u16 threeDLutTableData[][3], char *fileName);
void calculate3DLutTableFor2020To709(u16 lut3DTableData[][3]);

#ifdef __cplusplus
}
#endif

#endif /* __COMMAND_LINE_PARSER_H__ */
