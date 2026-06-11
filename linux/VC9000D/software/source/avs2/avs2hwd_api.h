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

#ifndef AVS2HWD_DECODER_H_
#define AVS2HWD_DECODER_H_

#include "avs2_params.h"
#include "deccfg.h"
#include "ppu.h"
#include "dectypes.h"
#include "fifo.h"
#include "commonfunction.h"

/* enumerated return values of the functions */
typedef enum {
  HWD_OK,
  HWD_WAIT,
  HWD_BUSY,
  HWD_FAIL,
  HWD_SYSTEM_ERROR,
  HWD_SYSTEM_FETAL,
  HWD_MAX
} HwdRet;

typedef enum {
  ATTRIB_SETUP,
  ATTRIB_CFG,
  ATTRIB_SEQ,
  ATTRIB_PIC,
  ATTRIB_CNT,
  ATTRIB_STREAM,
  ATTRIB_REFS,
  ATTRIB_RECON,
  ATTRIB_PP,
  ATTRIB_MAX
} ATTRIBUTE;

struct Avs2WQMatrix {
  short matrix[4 * 64];
};

/* asic interface */
struct Avs2AsicBuffers {
  /* internal */
  struct DWLLinearMem alf_tbl[MAX_ASIC_CORES];
  struct DWLLinearMem wqm_tbl[MAX_ASIC_CORES];

  /* external */
  struct DWLLinearMem *stream;
  struct DWLLinearMem *out_buffer;
  struct DWLLinearMem *out_pp_buffer;

  /* fake table params */
#ifdef USE_FAKE_RFC_TABLE
  struct DWLLinearMem fake_rfc_tbl;
  u32 tbl_sizey;
  u32 tbl_sizec;
#endif
};

/* structure derived from SPS for buffer requirement spec */
struct Avs2BufferSpec {
  struct ReferenceInfo {
    /* tile reference */
    u32 picy_size;
    u32 pic_size;

    /* dmv */
    u32 dmv_size;

    /* compressor */
    u32 tbly_size;
    u32 tblc_size;
    u32 tbl_size;

    /* total = recon+dmv+compress_table */
    u32 buff_size;

    /* dpb number */
    int max_dpb_size; /* derived according to B.2~14 according to profile/level
                        */
  } ref;

};

struct Reference {
  struct DWLLinearMem y;
  struct DWLLinearMem c;
  /* for compression table */
  struct DWLLinearMem y_tbl;
  struct DWLLinearMem c_tbl;
  /* for mv */
  struct DWLLinearMem mv;
  /* for sync word */
  struct DWLLinearMem sync;
  /* poc */
  int img_poi;
  int ref_poc[MAXREF];
  /* Unify EC */
  u32 error_ratio;
  enum DecErrorInfo error_info;
  /* is invalid ? */
  u8 is_invalid;
};

struct Avs2RefsParam {
  struct Reference ref[MAXREF];
  struct Reference background;
};

struct Avs2ConfigParam {
  u32 use_video_compressor;
  int disable_out_writing;
  u32 start_code_detected;
};

struct Avs2StreamParam {
  int is_rb; /* alwayse 0 */
  u8 *stream;
  addr_t stream_bus_addr;
  u32 stream_length;
  u32 stream_offset;
  /* ring buffer */
  struct DWLLinearMem ring_buffer;
  /* status */
  u32 pos_updated;
};

#define Avs2ReconParam Reference

struct Avs2PpoutParam {
  int type; /* 0: nv12; 1: i420 */
  int is_tile;
  union {
    struct {
      struct DWLLinearMem y;
      struct DWLLinearMem c;
    } nv12;
    struct {
      struct DWLLinearMem y;
      struct DWLLinearMem u;
      struct DWLLinearMem v;
    } i420;
  } pic;
  struct DWLLinearMem *pp_buffer;
  PpUnitIntConfig *ppu_cfg;
  const struct DecHwFeatures *hw_feature;
  u32 bottom_flag;
  u32 top_field_first;
};

struct StrmConsumedCallback {
  DecMCStreamConsumed *fn;
  const u8 *p_strm_buff; /* stream buffer passed in callback */
  const void *p_user_data; /* user data to be passed in callback */
};

struct Avs2HwRdyCallbackArg {
  const u8 *stream;
  const void *p_user_data;
  u32 out_id;
  struct Avs2DpbStorage *current_dpb;
  struct DWLLinearMem *pp_data;
  i32 ref_mem_idx[MAX_DPB_SIZE];
  u32 ref_id[16];
  struct DWLLinearMem dmv;
  u32 pic_width_in_ctbs;
  u32 pic_height_in_ctbs;
  u32 lcu_size_in_bit;
};

/* storage data structure, holds all data of a decoder instance */
struct Avs2Hwd {
  /* dwl handel */
  struct DWL *dwl;

  pthread_mutex_t mutex;

  /* id for this job */
  int job_id;
  int core_id;
  /* status */
  u32 status;
  u32 asic_running;
  /* parameter flags, bitwise defined. 1 means this param has been set. */
  u32 flags;
  u32 pp_enabled;
  DecPicAlignment align; /* buffer alignment for both reference/pp output */
  /* parameter set. */
  struct Avs2SeqParam *sps;
  struct Avs2PicParam *pps;
  struct Avs2RefsParam *refs;
  struct Avs2ReconParam *recon;
  struct Avs2PpoutParam *ppout;
  struct Avs2StreamParam *stream;
  /* hw config */
  struct Avs2ConfigParam *cfg;

  /* buffers */
  struct Avs2AsicBuffers *cmems;

  /* registers */
  u32 regs[DEC_X170_REGISTERS];
  u32 mc_refresh_regs[MAX_ASIC_CORES][REDA_STATUS_REGS_COUNT];

  /* derived */
  u32 bk_img_is_top_field;

  /* poc */
  u32 curr_img_poi;
  u16 unique_id;

  /* Unify EC */
  enum DecErrorInfo error_info;
  enum DecErrorHandling error_handling;
  u32 error_policy;
  u32 error_ratio;

  u32 vcmd_used;
  u32 cmdbuf_id;
  u32 mc_buf_id;  /* misc_linear/tile_edge index used for vcmd multicore. */


  struct LLStrmInfo *llstrminfo;
  /* multi-core relative params */
  u32 b_mc; /* flag to indicate MC mode status */
  u32 n_cores;
  u32 n_cores_available; /* cores available for HEVC */
  struct StrmConsumedCallback stream_consumed_callback;
  struct Avs2HwRdyCallbackArg *hw_rdy_callback_arg[MAX_MC_CB_ENTRIES];
  FifoInst fifo_core;

  enum {
    DEC_IDLE = 0,       /* DEC is idle */
    DEC_RUNNING         /* DEC is running */
  } dec_status[MAX_ASIC_CORES]; /* used to track each core's status in MC mode */

  short wq_matrix[2][2][64];  // wq_matrix[matrix_id][detail/undetail][coef]
  u32 secure_mode;
  u32 core_mask;
};

HwdRet Avs2HwdInit(struct Avs2Hwd *hwd, const void *dwl);
HwdRet Avs2HwdSetParams(struct Avs2Hwd *hwd, ATTRIBUTE attribute, void *data);
HwdRet Avs2HwdRun(struct Avs2Hwd *hwd, void *inst);
HwdRet Avs2HwdSync(struct Avs2Hwd *hwd, i32 timeout);
HwdRet Avs2HwdFree(struct Avs2Hwd *hwd);
HwdRet Avs2HwdRelease(struct Avs2Hwd *hwd);
HwdRet Avs2HwdStopHw(struct Avs2Hwd *hwd, i32 ID);

/* utitlity */
HwdRet Avs2HwdAllocInternals(struct Avs2Hwd *hwd);
HwdRet Avs2HwdAllocFakeTableMem(struct Avs2Hwd *hwd, struct Avs2AsicBuffers *cmems);
void Avs2HwdSetAlfTable(struct Avs2Hwd *hwd, u32 core_id);

#endif /* #ifdef AVS2HWD_DECODER_H_ */
