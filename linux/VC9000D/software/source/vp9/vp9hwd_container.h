/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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

#ifndef VP9HWD_CONTAINER_H
#define VP9HWD_CONTAINER_H

#include "basetype.h"
#include "commonvp9.h"
#include "deccfg.h"
#include "decppif.h"
#include "dwl.h"
#include "fifo.h"
#include "sw_debug.h"
#include "vp9decapi.h"
#include "vp9hwd_bool.h"
#include "vp9hwd_buffer_queue.h"
#include "vp9hwd_decoder.h"
#include "input_queue.h"
#include "low_latency.h"
#include "ppu.h"
#include "commonfunction.h"

#define VP9DEC_UNINITIALIZED 0U
#define VP9DEC_INITIALIZED 1U
#define VP9DEC_NEW_HEADERS 3U
#define VP9DEC_DECODING 4U
#define VP9DEC_END_OF_STREAM 5U
#define VP9DEC_WAITING_FOR_BUFFER 6U

#define VP9DEC_DYNAMIC_PIC_LIMIT 10

#define VP9_UNDEFINED_BUFFER (i32)(-1)

#define EMPTY_MARKER -5

#if 0
enum DecBufferType {
  REFERENCE_BUFFER = 0, /* reference + compression table + DMV*/
  RASTERSCAN_OUT_BUFFER,
  DOWNSCALE_OUT_BUFFER,
  TILE_EDGE_BUFFER,  /* filter mem + bsd control mem */
  SEGMENT_MAP_BUFFER, /* segment map */
  MISC_LINEAR_BUFFER, /* tile info + prob table + entropy context counter */
  BUFFER_TYPE_NUM
};

#define IS_EXTERNAL_BUFFER(config, type) (((config) >> (type)) & 1)
#endif

struct StrmConsumedCallback {
  DecMCStreamConsumed *fn;
  const u8 *p_strm_buff; /* stream buffer passed in callback */
  const void *p_user_data; /* user data to be passed in callback */
};

struct PicCallbackArg {
  u32 core_id;
  u32 display_number;
  i32 index; /* Buffer index of the output buffer. */
  i32 prev_index; /* Buffer index of the previous output buffer. */
  i32 index_a;
  i32 index_g;
  i32 index_p;
  u32 show_frame;
  u32 show_existing_frame;
  u8* p_ref_status;
  FifoInst fifo_out;        /* Output FIFO instance. */
  u32 refresh_frame_flags;
  u32 reset_frame_flags;
  u32 index_ref[VP9_ACTIVE_REFS];
  struct Vp9DecPicture pic; /* Information needed for outputting the frame. */
  const u8 *stream     ;    /* Input buffer virtual address. */
  void* p_user_data;        /* User data associated with input buffer. */
};

/* asic interface */
struct DecAsicBuffers {
  u32 width, height;

  struct DWLLinearMem tile_edge;
  struct DWLLinearMem filter_control;
  struct DWLLinearMem misc_linear;
  struct DWLLinearMem segment_map;
  struct DWLLinearMem ctx_counters;
  u32 prob_tbl_offset;
  //u32 segment_map_offset[2];
  u32 tile_info_offset;
  u32 filter_mem_offset[VP9_MAX_TILE_COLS];
  u32 filter_control_offset[VP9_MAX_TILE_COLS];
  u32 rfc_offset[VP9_MAX_TILE_COLS];
  u32 segment_map_size_new;
  u32 display_index[MAX_PIC_BUFFERS];
  struct DWLLinearMem multicore_sync_buffers;

  /* Concurrent access to following picture arrays is controlled indirectly
   * through buffer queue. */
  struct DWLLinearMem pictures[MAX_PIC_BUFFERS];
//  struct DWLLinearMem dir_mvs[MAX_PIC_BUFFERS]; /* colocated MVs */
  struct DWLLinearMem dpb_parasitic_buf[MAX_PIC_BUFFERS]; /* contain dmv, mc sync word, rfc table */
  struct DWLLinearMem pp_pictures[MAX_PIC_BUFFERS];
//  struct DWLLinearMem cbs_table[MAX_PIC_BUFFERS];
#ifdef USE_FAKE_RFC_TABLE
  struct DWLLinearMem fake_rfc_tbl;
  u32 tbl_sizey;
  u32 tbl_sizec;
#endif
  u32 pictures_c_offset[MAX_PIC_BUFFERS];
  u32 dir_mvs_offset[MAX_PIC_BUFFERS];
  u32 sync_mc_offset[MAX_PIC_BUFFERS];
  u32 pp_y_offset[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 pp_c_offset[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 cbs_y_tbl_offset[MAX_PIC_BUFFERS];
  u32 cbs_c_tbl_offset[MAX_PIC_BUFFERS];
  u32 delta_probs_offset[MAX_PIC_BUFFERS];
  u32 out_y_stride[MAX_PIC_BUFFERS];
  u32 out_c_stride[MAX_PIC_BUFFERS];
  u32 rs_stride[MAX_PIC_BUFFERS];
  u32 ds_stride[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 ds_stride_ch[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 picture_size;
  u32 dpb_parasitic_buf_size;
  u64 pp_size;
  i32 pp_buffer_map[MAX_PIC_BUFFERS];  /* reference buffer -> rs/ds buffer */

  u32 realloc_out_buffer;   /* 1 indidates there is a pending out buffer hold.*/
  u32 realloc_seg_map_buffer; /* 1 indidates a segment map is being reallocated. */
  u32 realloc_tile_edge_mem;  /* 1 indicates we are allocating/reallocating a tile edge buffer. */
  struct Vp9DecPicture picture_info[MAX_PIC_BUFFERS];
  i32 first_show[MAX_PIC_BUFFERS];
  i32 reference_list[VP9_REF_LIST_SIZE]; /* Contains indexes to full list of
                                            picture */

  /* Indexes for picture buffers in pictures[] array */
  i32 out_buffer_i;
  i32 prev_out_buffer_i;

  /* Indexes for picture buffers in raster[]/dscale[] array */
  i32 out_pp_buffer_i;
  u32 pp_out_ctrl[MAX_PIC_BUFFERS];

  u32 whole_pic_concealed;
  u32 disable_out_writing;
  u32 segment_map_size;
  u32 partition1_base;
  u32 partition1_bit_offset;
  u32 partition2_base;
};

struct Vp9HwRdyCallbackArg {
  u32 tile_index;
};

struct Vp9DecContainer {
  const void *checksum;
  u32 dec_mode;
  u32 dec_stat;
  u32 pic_number;
  u32 asic_running;
  u32 width;
  u32 height;
  u32 vp9_regs[VP9_MAX_TILE_COLS][DEC_X170_REGISTERS]; //TODO LXJ need to optimize the mem size
  u32 *tile_reg;         /* pointer to current tile reg */
  u32 mc_refresh_regs[MAX_ASIC_CORES][REDA_STATUS_REGS_COUNT];

  struct Vp9HwRdyCallbackArg *hw_rdy_callback_arg[MAX_ASIC_CORES];
  enum {
    TILE_TODO = 0,       /* tile col is going to be decoded */
    TILE_DONE            /* tile col is decoded */
  } tile_status[VP9_MAX_TILE_COLS]; /* used to track each tile col status in MC mode */

  void *job;
  struct DecAsicBuffers asic_buff[1];
  const void *dwl; /* struct DWL instance */
  i32 core_id;

  struct Vp9Decoder decoder;
  struct VpBoolCoder bc;

  enum DecErrorInfo error_info;
  enum DecErrorHandling error_handling;
  u32 error_policy;
  u32 error_ratio;
  u32 picture_broken;
  u32 out_count;
  u32 num_buffers;
  u32 num_buffers_reserved;
  u32 active_segment_map;
  u32 n_extra_frm_buffers;

  BufferQueue bq;
  u32 num_pp_buffers;
  BufferQueue pp_bq;  /* raster/dscale buffer queue for free raster output buffer. */
  u32 min_buffer_num; /* Minimum external buffer needed. */

  u32 use_adaptive_buffers;
  u32 guard_size;
  u32 secure_mode;

  u32 intra_only;
  u32 conceal;
  u32 prev_is_key;
  u32 prob_refresh_detected;
  struct PicCallbackArg pic_callback_arg;
  /* Output related variables. */
  FifoInst fifo_out;     /* Fifo for output indices. */
  FifoInst fifo_display; /* Store of output indices for display reordering. */
  u32 display_number;
  pthread_mutex_t sync_out; /* protects access to pictures in output fifo. */
  pthread_cond_t sync_out_cv;

  u32 dynamic_buffer_limit; /* limit for dynamic frame buffer count */

  u32 pp_enabled;
  u32 down_scale_x;
  u32 down_scale_y;
  u32 down_scale_x_shift;
  u32 down_scale_y_shift;

  PpUnitIntConfig ppu_cfg[DEC_MAX_PPU_COUNT];

  DelogoConfig delogo_params[2];
  u32 vp9_10bit_support;
  u32 use_video_compressor;

  u32 ext_buffer_config;
  u32 next_buf_size;
  u32 buf_num;
  u32 next_dpb_parasitic_buf_size;
  struct DWLLinearMem *buf_to_free;
  enum DecBufferType buf_type;

  u8 enable_3dlut;
  u32 buffer_index;
  u32 add_buffer; /* flag to add the newly allocated buffer. */
  u32 buf_not_added; /* A reallocated buffer has been added to decoder or not */
  u32 buffer_num_added;   /* num of external buffers added */
  u32 abort;
  pthread_mutex_t protect_mutex;

  u32 input_data_len;

  // u32 entropy_broken;
  u32 no_decoding_buffer;
  u32 multicore_poll_period;

  const struct DecHwFeatures *hw_feature;
  DecPicAlignment align;  /* buffer alignment for both reference/pp output */
  u32 first_tile_empty;

  /* low latency */
  u32 low_latency; /* flag for using low latency */
  addr_t llstrm_curr_address;
  struct LLStrmInfo llstrminfo;
  struct LLTileInfo lltileinfo;

  /* muticore relative */
  u32 n_cores;
  u32 n_cores_available;  /* cores count for VP9 */
  u32 b_mc;
  u32 cycle_sum;

  /* VCMD */
  u32 vcmd_used;
  u32 cmdbuf_id;
  u32 mc_buf_id;
  FifoInst fifo_core;

#ifdef FPGA_PERF_AND_BW
  DecPerfInfo perf_info;
#endif
  u32 core_mask;
};

#endif /* #ifdef VP9HWD_CONTAINER_H */
