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

#ifndef AV1HWD_CONTAINER_H
#define AV1HWD_CONTAINER_H

#include "av1_commondec.h"
#include "av1decapi.h"
#include "av1hwd_bool.h"
#include "av1hwd_buffer_queue.h"
#include "av1hwd_decoder.h"
#include "basetype.h"
#include "decapicommon.h"
#include "deccfg.h"
#include "decppif.h"
#include "dwl.h"
#include "fifo.h"
#include "ppu.h"
#include "low_latency.h"
#include "sw_debug.h"
#include "swreg_struct.h"
#include "commonfunction.h"

#define AV1DEC_UNINITIALIZED 0U
#define AV1DEC_INITIALIZED 1U
#define AV1DEC_NEW_HEADERS 3U
#define AV1DEC_DECODING 4U
#define AV1DEC_END_OF_STREAM 5U
#define AV1DEC_WAITING_FOR_BUFFER 6U

#define AV1DEC_DYNAMIC_PIC_LIMIT 10

#define AV1_UNDEFINED_BUFFER (i32)(-1)

#define EMPTY_MARKER        (-5)
#define NO_OUTPUT_MARKER    (-4)
#define FLUSH_MARKER        (-3)
#define ABORT_MARKER        (-2)

struct PicCallbackArg {
  u32 display_number;
  i32 index; /* Buffer index of the output buffer. */
  i32 index_a;
  i32 index_g;
  i32 index_p;
  u32 show_frame;
  u32 show_existing_frame;
  FifoInst fifo_out;        /* Output FIFO instance. */
  struct Av1DecPicture pic; /* Information needed for outputting the frame. */
};

struct SegmentInfo {
  u32 segment_feature_enable[MAX_MB_SEGMENTS][SEG_AV1_LVL_MAX];
  i32 segment_feature_data[MAX_MB_SEGMENTS][SEG_AV1_LVL_MAX];
};

/* asic interface */
struct DecAsicBuffers {
  u32 width, height;
  struct DWLLinearMem prob_tbl;
  struct DWLLinearMem prob_tbl_out; /* Separate output buffer is needed for
                                       multicore */
  struct DWLLinearMem tile_info;
  struct DWLLinearMem global_model;
  struct DWLLinearMem film_grain_mem;
  struct DWLLinearMem filter_mem;
  struct DWLLinearMem filter_control;
  struct DWLLinearMem default_cdfs_mem;
  struct DWLLinearMem default_cdfs_ndvc_mem;
  struct DWLLinearMem cdfs_last_mem;
  u32 db_data_col_offset;
  u32 db_ctrl_col_offset;
  u32 cdef_col_offset;
  u32 sr_col_offset;
  u32 lr_col_offset;
  u32 db_data_col_tsize;
  u32 db_ctrl_col_tsize;
  u32 cdef_col_tsize;
  u32 sr_col_tsize;
  u32 lr_col_tsize;
  u32 rfc_col_offset;
  u32 rfc_col_size;
  u32 pp_reorder_offset;
  u32 pp_reorder_size;
  u32 pp_scale_offset;
  u32 pp_scale_size;
  u32 pp_scale_out_offset;
  u32 pp_scale_out_size;
  struct DWLLinearMem tile_bitstream;
  u32 tile_bitstream_len;
  //struct DWLLinearMem secondary_column_buffer;
  struct DWLLinearMem multicore_sync_buffers;
  struct DWLLinearMem fbc_tile;
  u32 fbc_size;

  u32 display_index[MAX_PIC_BUFFERS];
  i32 first_show[MAX_PIC_BUFFERS];
  /* Concurrent access to following picture arrays is controlled indirectly
   * through buffer queue. */
  struct DWLLinearMem pictures[MAX_PIC_BUFFERS];
  struct DWLLinearMem dpb_parasitic_buf[MAX_PIC_BUFFERS];
  u32 pictures_c_offset[MAX_PIC_BUFFERS];
  u32 dir_mvs_offset[MAX_PIC_BUFFERS];
  u32 cbs_y_tbl_offset[MAX_PIC_BUFFERS];
  u32 cbs_c_tbl_offset[MAX_PIC_BUFFERS];
  u32 pp_y_offset[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 pp_c_offset[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 ds_stride[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 ds_stride_ch[MAX_PIC_BUFFERS][DEC_MAX_PPU_COUNT];
  u32 out_stride[MAX_PIC_BUFFERS];
  struct DWLLinearMem pp_pictures[MAX_PIC_BUFFERS];
  struct Av1DecPicture picture_info[MAX_PIC_BUFFERS];
  struct DWLLinearMem fake_rfc_tbl;
  u32 tbl_sizey;
  u32 tbl_sizec;
  u32 picture_size;
  u32 dpb_parasitic_buf_size;
  u64 pp_size;
  u32 realloc_out_buffer; /* 1 indidates there is a pending out buffer hold.*/
  i32 pp_buffer_map[MAX_PIC_BUFFERS]; /* reference buffer -> pp buffer */
  struct SegmentInfo segment_info[MAX_PIC_BUFFERS];
  struct Av1FilmGrainParams fg_params[MAX_PIC_BUFFERS];
  struct DWLLinearMem
      bodp_cfg[MAX_PIC_BUFFERS]; /* info needed by bodp */

  i32 reference_list[AV1_REF_LIST_SIZE]; /* Contains indexes to full list of
                                            picture */
  // struct frameComp fc[MAX_PIC_BUFFERS];
  u8 log2_tile_columns[MAX_PIC_BUFFERS];
  /* Indexes for picture buffers in pictures[] array */
  i32 out_buffer_i;
  // i32 prev_out_buffer_i;

  /* Indexes for picture buffers in pp array */
  i32 out_pp_buffer_i;
  u32 pp_out_ctrl[MAX_PIC_BUFFERS];

  u32 whole_pic_concealed;
  u32 disable_out_writing;
  u32 segment_map_size;
  u32 partition1_base;
  u32 partition1_bit_offset;
  u32 partition2_base;

  struct WarpedMotionParams global_models[MAX_PIC_BUFFERS]
                                         [GM_GLOBAL_MODELS_PER_FRAME];
};

struct Av1HwRdyCallbackArg {
  u32 tile_index;
};

struct Av1DecContainer {
  const void *checksum;
  u32 dec_mode;
  u32 dec_stat;
  u32 pic_number;
  u32 asic_running;
  u32 width;
  u32 superres_width;
  u32 height;
  u32 bit_depth;
  u32 multicore;
  struct DecAsicBuffers asic_buff[1];
  const void *dwl; /* DWL instance */
  i32 core_id;
  u32 secure_mode;

  struct Av1Decoder decoder;
  /* one metadata_param corresponds to one dpb_buffer */
  struct MetadataParameters *metadata_param[MAX_PIC_BUFFERS];
  struct MetadataParameters *metadata_param_curr;
  u32 metadata_param_num;

  u32 picture_broken;
  u32 skip_no_intra;
  u32 out_count;
  u32 num_buffers;
  u32 num_buffers_reserved;

  BufferQueue bq;
  BufferQueue pp_bq; /* pp buffer queue for free raster output buffer. */
  u32 num_pp_buffers;
  u32 min_buffer_num; /* Minimum external buffer needed. */

  u32 intra_only;
  u32 hw_conceal;
  enum DecErrorInfo error_info;
  enum DecErrorHandling error_handling;
  u32 error_policy;
  u32 error_ratio;
  u32 prev_is_key;
  u32 prev_is_monochrome;
  struct PicCallbackArg pic_callback_arg;
  void *usr_ptr;
  u32 use_multicore;
  u32 use_multicore_backup;
  u32 n_cores;
  u32 n_cores_available;  /* cores count for AV1 */

  u32 cycle_sum;
  /* Output related variables. */
  FifoInst fifo_out;     /* Fifo for output indices. */
  FifoInst fifo_display; /* Store of output indices for display reordering. */
  u32 display_number;
  pthread_mutex_t sync_out; /* protects access to pictures in output fifo. */
  pthread_cond_t sync_out_cv;

  u32 t_fifo_out_disp_num[MAX_PIC_BUFFERS];
  u32 t_fifo_out_disp_wr;
  u32 t_fifo_out_disp_rd;

  u32 ext_buffer_config;
  u32 heif_mode; /* set as 1 when dec avif stream */
  u32 dynamic_buffer_limit; /* limit for dynamic frame buffer count */
  u32 next_buf_size;
  u32 next_dpb_parasitic_buf_size;
  u32 buf_num;
  struct DWLLinearMem *buf_to_free;
  enum DecBufferType buf_type;
  u32 buffer_index;
  u32 add_buffer;    /* flag to add the newly allocated buffer. */
  u32 buf_not_added; /* A reallocated buffer has been added to decoder or not */
  u32 buffer_num_added; /* num of external buffers added */

  struct Av1PpConfig pp;
  u8 pack_pixel_to_msbs;

  u32 initial_bitdepth;
  // struct SwRegisters base_swregs;
  struct SwRegisters sw_ctrl;
  struct SwRegisters multi_sw_ctrl[AV1_MAX_TILE_COLS];
  // struct SwRegisters multi_swregs[AV1_MAX_TILE_COLS];
  u32 av1_regs[AV1_MAX_TILE_COLS][DEC_X170_REGISTERS]; //TODO LXJ optimize the mem size
  u32 mc_refresh_regs[MAX_ASIC_CORES][REDA_STATUS_REGS_COUNT];
  struct Av1HwRdyCallbackArg *hw_rdy_callback_arg[MAX_ASIC_CORES];
  enum {
    TILE_TODO = 0,       /* tile col is going to be decoded */
    TILE_DONE            /* tile col is decoded */
  } tile_status[AV1_MAX_TILE_COLS]; /* used to track each tile col status in MC mode */

  void *job;

  u32 vcmd_m2m; /* flag that if hw support m2m */

  // BodpCfg bodp_cfg;
  u32 use_video_compressor;
  bool host_accessible_frames;
  // struct frameComp fc;  // current frame compression modes

  struct Av1SecondaryOutConfig secondary_out_cfg;
  PpUnitIntConfig ppu_cfg[DEC_MAX_OUT_COUNT];
  PpUnitConfig  reserved_ppu_cfg[DEC_MAX_OUT_COUNT];
  bool pp_enabled;
  bool annexb;
  bool plainobu;

  u32 multicore_poll_period;
  u32 input_data_len;

  const struct DecHwFeatures *hw_feature;
  struct DWLLinearMem output_buffer[MAX_PIC_BUFFERS];

  u8 enable_3dlut;
  u32 no_decoding_buffer;
  u32 abort;
  pthread_mutex_t protect_mutex;
  u32 min_dec_pic_width;
  u32 min_dec_pic_height;
  DecPicAlignment align;

  /* low latency */
  u32 low_latency;
  struct LLStrmInfo llstrminfo;
  struct LLTileInfo lltileinfo;
  struct StrmData llhrddata;

  /* VCMD */
  u32 vcmd_used;
  u32 cmdbuf_id;
  u32 mc_buf_id;
  FifoInst fifo_core;

  u32 core_mask;
#ifdef ASIC_TRACE_SUPPORT
  FILE *film_grain_params_fid;
#endif
};

#endif /* #ifdef AV1HWD_CONTAINER_H */
