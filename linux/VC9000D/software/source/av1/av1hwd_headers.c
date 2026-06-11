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

#include "av1hwd_headers.h"
#include "av1_entropymode.h"
#include "av1hwd_probs.h"
#include "basetype.h"
#include "dwl.h"
#include "sw_debug.h"
#include "sw_stream.h"

#include "av1_film_grain_dec.h"
#include "av1_global_motion_dec.h"
#include "dec_log.h"

/* Define here if it's necessary to print errors */
#define DEC_HDRS_ERR(x)

static int SignExt(u32 v, int bits) {
  const int nbits = sizeof(u32) * 8 - bits - 1;
  v <<= nbits;
  return ((int)v) >> nbits;
}

int Av1CompareSequenceHeader(obuSequenceHeaderInput_t *seq_hdr_a,
                             obuSequenceHeaderInput_t *seq_hdr_b);
void Av1CheckSequenceHeader(struct Av1Decoder *decoder,
                            obuSequenceHeaderInput_t *seq_hdr);
void Av1ModifySequenceHeader(struct Av1Decoder *decoder);
static void DecodeSegmentationDataAV1(struct StrmData *rb,
                                      struct Av1DecContainer *dec_cont);
void StoreLfParams(struct Av1DecContainer *dec_cont);
void LoadLfParams(struct Av1DecContainer *dec_cont, i32 pri_idx);
static void DecodeLfParams(struct StrmData *rb,
                           struct Av1DecContainer *dec_cont,
                           int coded_lossless);
static i32 DecodeQuantizerDelta(struct StrmData *rb);
// static u32 ReadTileSize(const u8 *cx_size);
// static void GetTileNBits(struct Av1Decoder *dec, u32 *min_log2_ntiles_ptr,
//                         u32 *delta_log2_ntiles);
static void ExistingRefRegister(struct Av1Decoder *dec, u32 map);

void ExistingRefRegister(struct Av1Decoder *dec, u32 map) {
  dec->existing_ref_map |= map;
}

#if 0
void GetTileNBits(struct Av1Decoder *dec, u32 *min_log2_ntiles_ptr,
                  u32 *delta_log2_ntiles) {
  const int sb_cols = (dec->width + 63) >> 6;
  u32 min_log2_ntiles, max_log2_ntiles;

  for (max_log2_ntiles = 0; (sb_cols >> max_log2_ntiles) >= MIN_TILE_WIDTH_SBS;
       max_log2_ntiles++) {
  }
  if (max_log2_ntiles > 0) max_log2_ntiles--;
  for (min_log2_ntiles = 0; (MAX_TILE_WIDTH_SBS << min_log2_ntiles) < sb_cols;
       min_log2_ntiles++) {
  }

  ASSERT(max_log2_ntiles >= min_log2_ntiles);
  *min_log2_ntiles_ptr = min_log2_ntiles;
  *delta_log2_ntiles = max_log2_ntiles - min_log2_ntiles;
}
#endif

int tile_log2(int blk_size, int target) {
  int k;
  for (k = 0; (blk_size << k) < target; k++) {
  }
  return k;
}

void GetTileLimits(struct Av1Decoder *dec, u32 *min_log2_tile_cols,
                   u32 *max_log2_tile_cols, u32 *min_log2_tiles,
                   u32 *max_log2_tile_rows) {
  const int sb_cols =
      dec->sb_size ? ((dec->width + 127) >> 7) : ((dec->width + 63) >> 6);
  const int sb_rows =
      dec->sb_size ? ((dec->height + 127) >> 7) : ((dec->height + 63) >> 6);

  const int max_tile_width_sb = MAX_TILE_WIDTH_SBS >> (dec->sb_size ? 1 : 0);
  const int max_tile_area_sb = MAX_TILE_AREA >> (2 * (dec->sb_size ? 7 : 6));

  *min_log2_tile_cols = tile_log2(max_tile_width_sb, sb_cols);
  *max_log2_tile_cols = tile_log2(1, MIN(sb_cols, AV1_MAX_TILE_COLS));
  *max_log2_tile_rows = tile_log2(1, MIN(sb_rows, AV1_MAX_TILE_ROWS));
  *min_log2_tiles = tile_log2(max_tile_area_sb, sb_cols * sb_rows);
  *min_log2_tiles = MAX(*min_log2_tiles, *min_log2_tile_cols);
}

// TODO(dkhe): move these into a common header
// HORIZONLY_FRAME_SUPERRES
void SetupSuperres(struct StrmData *rb, struct Av1Decoder *dec) {
#define SUPERRES_SCALE_BITS 3
#define SCALE_NUMERATOR 8
#define SUPERRES_SCALE_DENOMINATOR_MIN (SCALE_NUMERATOR + 1)

#define RS_SUBPEL_BITS 6
#define RS_SUBPEL_MASK ((1 << RS_SUBPEL_BITS) - 1)
#define RS_SCALE_SUBPEL_BITS 14
#define RS_SCALE_SUBPEL_MASK ((1 << RS_SCALE_SUBPEL_BITS) - 1)
#define RS_SCALE_EXTRA_BITS (RS_SCALE_SUBPEL_BITS - RS_SUBPEL_BITS)
#define RS_SCALE_EXTRA_OFF (1 << (RS_SCALE_EXTRA_BITS - 1))

  u8 superres_scale_denominator = SCALE_NUMERATOR;
  dec->scale_denom_minus9 = 0;
  dec->superres_is_scaled = 0;

  if (dec->enable_superres) {
    // superres stuff, TODO
    if (SwGetBits(rb, 1)) {
      dec->superres_is_scaled = 1;
      superres_scale_denominator = SwGetBits(rb, SUPERRES_SCALE_BITS);
      dec->scale_denom_minus9 = superres_scale_denominator;
      superres_scale_denominator += SUPERRES_SCALE_DENOMINATOR_MIN;
    }
  }

  // See Section 5.7.7
  dec->superres_width = dec->width;
  if (superres_scale_denominator > SCALE_NUMERATOR) {
    dec->width = (dec->superres_width * SCALE_NUMERATOR +
                  (superres_scale_denominator / 2)) /
                 superres_scale_denominator;
    u32 min_w = MIN(16, dec->superres_width);
    if (dec->width < min_w) dec->width = min_w;
    if (dec->width == dec->superres_width) {
      dec->scale_denom_minus9 = 0;
      dec->superres_is_scaled = 0;
      dec->superres_luma_step = RS_SCALE_SUBPEL_BITS;
      dec->superres_chroma_step = RS_SCALE_SUBPEL_BITS;
      dec->superres_luma_step_invra = RS_SCALE_SUBPEL_BITS;
      dec->superres_chroma_step_invra = RS_SCALE_SUBPEL_BITS;
      dec->superres_init_luma_subpel_x = 0;
      dec->superres_init_chroma_subpel_x = 0;
      return;
    }

    const int upscaledLumaPlaneW = dec->superres_width;
    const int downscaledLumaPlaneW = dec->width;

    const int downscaledChromaPlaneW = (downscaledLumaPlaneW + 1) >> 1;
    const int upscaledChromaPlaneW = (upscaledLumaPlaneW + 1) >> 1;

    const int stepLumaX = ((downscaledLumaPlaneW << RS_SCALE_SUBPEL_BITS) +
                           (upscaledLumaPlaneW / 2)) /
                          upscaledLumaPlaneW;
    const int stepChromaX = ((downscaledChromaPlaneW << RS_SCALE_SUBPEL_BITS) +
                             (upscaledChromaPlaneW / 2)) /
                            upscaledChromaPlaneW;
    const int errLuma = (upscaledLumaPlaneW * stepLumaX) -
                        (downscaledLumaPlaneW << RS_SCALE_SUBPEL_BITS);
    const int errChroma = (upscaledChromaPlaneW * stepChromaX) -
                          (downscaledChromaPlaneW << RS_SCALE_SUBPEL_BITS);
    const int initialLumaSubpelX =
        ((-((upscaledLumaPlaneW - downscaledLumaPlaneW)
            << (RS_SCALE_SUBPEL_BITS - 1)) +
          upscaledLumaPlaneW / 2) /
             upscaledLumaPlaneW +
         (1 << (RS_SCALE_EXTRA_BITS - 1)) - errLuma / 2) &
        RS_SCALE_SUBPEL_MASK;
    const int initialChromaSubpelX =
        ((-((upscaledChromaPlaneW - downscaledChromaPlaneW)
            << (RS_SCALE_SUBPEL_BITS - 1)) +
          upscaledChromaPlaneW / 2) /
             upscaledChromaPlaneW +
         (1 << (RS_SCALE_EXTRA_BITS - 1)) - errChroma / 2) &
        RS_SCALE_SUBPEL_MASK;

    dec->superres_luma_step = stepLumaX;
    dec->superres_chroma_step = stepChromaX;
    dec->superres_luma_step_invra =
        ((upscaledLumaPlaneW << RS_SCALE_SUBPEL_BITS) +
         (downscaledLumaPlaneW / 2)) /
        downscaledLumaPlaneW;
    dec->superres_chroma_step_invra =
        ((upscaledChromaPlaneW << RS_SCALE_SUBPEL_BITS) +
         (downscaledChromaPlaneW / 2)) /
        downscaledChromaPlaneW;
    dec->superres_init_luma_subpel_x = initialLumaSubpelX;
    dec->superres_init_chroma_subpel_x = initialChromaSubpelX;
  } else {
    dec->superres_luma_step = RS_SCALE_SUBPEL_BITS;
    dec->superres_chroma_step = RS_SCALE_SUBPEL_BITS;
    dec->superres_luma_step_invra = RS_SCALE_SUBPEL_BITS;
    dec->superres_chroma_step_invra = RS_SCALE_SUBPEL_BITS;
    dec->superres_init_luma_subpel_x = 0;
    dec->superres_init_chroma_subpel_x = 0;
  }
}
//                              2.0  2.1  2.2 2.3  3.0 3.1  3.2  3.3  4.0  4.1
//                              4.2   4.3   5.0  5.1   5.2   5.3  6.0    6.1 6.2
//                              6.3
static u32 max_width_level[24] = {2048, 2816, 4352,  4352,  4352,  5504, 6144,
                                  6144, 6144, 6144,  8192,  8192,  8192, 8192,
                                  8192, 8192, 16384, 16384, 16384, 16384};
static u32 max_height_level[24] = {1152, 1584, 2448, 2448, 2448, 3096, 3456,
                                   3456, 3456, 3456, 4352, 4352, 4352, 4352,
                                   4352, 4352, 8704, 8704, 8704, 8704};

static int SetupFrameSize(struct StrmData *rb, struct Av1DecContainer *dec_cont,
                          int frame_size_override_flag) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  int num_bits_w = dec->num_bits_w;
  int num_bits_h = dec->num_bits_h;
  if (frame_size_override_flag) {
    /* Frame width */
    dec->width = SwGetBits(rb, num_bits_w) + 1;
    STREAMTRACE_I("set up frame_width: %d\n", dec->width);

    /* Frame height */
    dec->height = SwGetBits(rb, num_bits_h) + 1;
#if 1
    if (!(dec->key_frame || dec->intra_only)) {
      u32 tmp1, tmp2, i;
      struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
      u8 n_active_refs = AV1_ACTIVE_REFS_EX, max_ref_frames = MAX_REF_FRAMES_EX;

      u32 index_ref[AV1_ACTIVE_REFS_EX];
      u32 index_info[AV1_ACTIVE_REFS_EX];

      for (i = 0; i < n_active_refs; i++) {
        index_ref[i] =
            Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
      }

      if (!dec_cont->pp_enabled) {
        for (i = 0; i < n_active_refs; i++) {
          index_info[i] = index_ref[i];
        }
      } else {
        for (i = 0; i < n_active_refs; i++)
          index_info[i] = Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
      }

      for (int i = LAST_FRAME; i < max_ref_frames; i++) {
        u32 ref = i - 1;
        int idx;
        {
          idx = index_info[ref];
          tmp1 = asic_buff->picture_info[idx].superres_width;
          tmp2 = asic_buff->picture_info[idx].coded_height;
          if (tmp1 > (2 * dec->width)) {
            // there are error for dec->width
            return HANTRO_NOK;
          }
          if (tmp2 > (2 * dec->height)) {
            // there are error for dec->height
            return HANTRO_NOK;
          }
          if (tmp1 < (dec->width / 16)) {
            // there are error for dec->width
            return HANTRO_NOK;
          }
          if (tmp2 < (dec->height / 16)) {
            // there are error for dec->height
            return HANTRO_NOK;
          }
        }
      }
    }
#endif
    STREAMTRACE_I("set up frame_height: %d\n", dec->height);
    if (dec->width > dec->max_width || dec->height > dec->max_height ||
        dec->width == 0 || dec->height == 0)
      return HANTRO_NOK;
  } else {
    dec->width = dec->max_width;
    dec->height = dec->max_height;
  }
  if (dec->level[0] > 31) {
    return HANTRO_NOK;
  } else if (dec->level[0] == 31) {
    // not check
  } else if (dec->level[0] < 31 && dec->level[0] >= 20) {
    return HANTRO_NOK;
  } else {
    if (dec->width > max_width_level[dec->level[0]]) return HANTRO_NOK;
    if (dec->height > max_height_level[dec->level[0]]) return HANTRO_NOK;
  }
  // HORIZONLY_FRAME_SUPERRES
  SetupSuperres(rb, dec);

  dec->scaling_active = SwGetBits(rb, 1);
  STREAMTRACE_I("Scaling active: %d\n", dec->scaling_active);

  if (dec->scaling_active) {
    /* Scaled frame width */
    dec->scaled_width = SwGetBits(rb, 16) + 1;
    STREAMTRACE_I("scaled_frame_width: %d\n", dec->scaled_width);

    /* Scaled frame height */
    dec->scaled_height = SwGetBits(rb, 16) + 1;
    STREAMTRACE_I("scaled_frame_height: %d\n", dec->scaled_height);
  }
  return HANTRO_OK;
}

static int SetupFrameSizeWithRefs(struct StrmData *rb,
                                  struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 tmp, index;
  i32 found, i, max_ref_frames;
  u32 prev_width, prev_height;

  found = 0;
  dec->resolution_change = 0;
  prev_width = dec->width;
  prev_height = dec->height;
  max_ref_frames = ALLOWED_REFS_PER_FRAME_EX;
  for (i = 0; i < max_ref_frames; ++i) {
    tmp = SwGetBits(rb, 1);
    STREAMTRACE_I("use_prev_frame_size: %d\n", tmp);
    if (tmp) {
      found = 1;
      /* Get resolution from frame buffer [active_ref_idx[i]] */
      index = Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
      if (index >= MAX_PIC_BUFFERS) {
        return HANTRO_NOK;
      }

      if (dec_cont->pp_enabled)
        index = Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);

      dec->width = asic_buff->picture_info[index].superres_width;

      dec->height = asic_buff->picture_info[index].coded_height;
      // TODO: AV1 baseline should copy also scaling_width/height
      STREAMTRACE_I("coded frame_width: %d\n", dec->width);
      STREAMTRACE_I("coded frame_height: %d\n", dec->height);
      SetupSuperres(rb, dec);
      STREAMTRACE_I("superres frame_width: %d\n", dec->superres_width);
      break;
    }
  }
#if 1
  if (!(dec->key_frame || dec->intra_only) && found) {
    u32 tmp1, tmp2, i;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
    u8 n_active_refs = AV1_ACTIVE_REFS_EX, max_ref_frames = MAX_REF_FRAMES_EX;

    u32 index_ref[AV1_ACTIVE_REFS_EX];
    u32 index_info[AV1_ACTIVE_REFS_EX];

    for (i = 0; i < n_active_refs; i++) {
      index_ref[i] = Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
    }

    if (!dec_cont->pp_enabled) {
      for (i = 0; i < n_active_refs; i++) {
        index_info[i] = index_ref[i];
      }
    } else {
      for (i = 0; i < n_active_refs; i++)
        index_info[i] = Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
    }

    for (int i = LAST_FRAME; i < max_ref_frames; i++) {
      u32 ref = i - 1;
      int idx;
      {
        idx = index_info[ref];
        tmp1 = asic_buff->picture_info[idx].superres_width;
        tmp2 = asic_buff->picture_info[idx].coded_height;
        if (tmp1 > (2 * dec->width)) {
          // there are error for dec->width
          return HANTRO_NOK;
        }
        if (tmp2 > (2 * dec->height)) {
          // there are error for dec->height
          return HANTRO_NOK;
        }
        if (tmp1 < (dec->width / 16)) {
          // there are error for dec->width
          return HANTRO_NOK;
        }
        if (tmp2 < (dec->height / 16)) {
          // there are error for dec->height
          return HANTRO_NOK;
        }
      }
    }
  }
#endif

  if (!found) {
    if (SetupFrameSize(rb, dec_cont, 1) == HANTRO_NOK) return HANTRO_NOK;
  }

  // TODO (dkhe): Check if this should be changed to dec->superres_width
  if (dec->width != prev_width || dec->height != prev_height)
    dec->resolution_change = 1; /* Signal resolution change for this frame */

  return HANTRO_OK;
}

#define RESERVED \
  if ((tmp = SwGetBits(rb, 1))) STREAMTRACE_I("Reserved bit, must be zero: %d\n", tmp)

u8 getMsb(u16 val) {
  static u8 lk[] = {0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3};
  u8 i = 0;

  if ((val & 0xff00) != 0) {
    val >>= 8;
    i += 8;
  }

  if ((val & 0xf0) != 0) {
    val >>= 4;
    i += 4;
  }

  return i + lk[val];
}

#if 1
static int get_msb(unsigned int n) {
  int log = 0;
  unsigned int value = n;
  int i;

  ASSERT(n != 0);

  for (i = 4; i >= 0; --i) {
    const int shift = (1 << i);
    const unsigned int x = value >> shift;
    if (x != 0) {
      value = x;
      log += shift;
    }
  }
  return log;
}

#endif

static u16 Av1ReadUniform(struct StrmData *rbp, u16 n) {
  if (n <= 1) return 0;
  const int l = get_msb(n - 1) + 1;
  const int m = (1 << l) - n;
  const int v = SwGetBits(rbp, l - 1);
  return v < m ? v : (v << 1) - m + SwGetBits(rbp, 1);
}

static u16 Av1ReadPrimitiveQUniform(struct StrmData *stream, u16 n) {
  if (n <= 1) return 0;
  const int l = getMsb(n - 1) + 1;
  const int m = (1 << l) - n;
  const int v = SwGetBits(stream, l - 1);
  return v < m ? v : (v << 1) - m + SwGetBits(stream, 1);
}

// static
u16 Av1ReadPrimitiveSubexpfin(struct StrmData *stream, u16 n, u16 k) {
  int i = 0;
  int mk = 0;
  u16 v;
  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);
    if (n <= mk + 3 * a) {
      v = Av1ReadPrimitiveQUniform(stream, n - mk) + mk;
      break;
    } else {
      if (SwGetBits(stream, 1)) {
        i = i + 1;
        mk += a;
      } else {
        v = SwGetBits(stream, b) + mk;
        break;
      }
    }
  }
  return v;
}

#define FRAME_OFFSET_BITS 5
#define PRIMARY_REF_BITS 3
int frame_is_intra_only(struct Av1Decoder *dec) {
  return dec->key_frame || dec->intra_only;
}

void AV1InitCDFs(struct Av1Decoder *x);

typedef struct {
  int map_idx;
  int buf_idx;
  int sort_idx;
} REF_FRAME_INFO;

int GetRelativeDist1(struct Av1Decoder *dec, int a, int b) {
  if (!dec->enable_order_hint) return 0;
  const int bits = dec->order_hint_bits_minus1;

  int diff = a - b;
  int m = 1 << bits;
  diff = (diff & (m - 1)) - (diff & m);
  return diff;
}

static int compare_ref_frame_info(const void *arg_a, const void *arg_b) {
  const REF_FRAME_INFO *info_a = (REF_FRAME_INFO *)arg_a;
  const REF_FRAME_INFO *info_b = (REF_FRAME_INFO *)arg_b;

  if (info_a->sort_idx < info_b->sort_idx) return -1;
  if (info_a->sort_idx > info_b->sort_idx) return 1;
  return (info_a->map_idx < info_b->map_idx)
             ? -1
             : ((info_a->map_idx > info_b->map_idx) ? 1 : 0);
}

void set_ref_frame_info(struct Av1Decoder *dec, int frame_idx,
                        REF_FRAME_INFO *ref_info) {
  dec->active_ref_idx[frame_idx] = ref_info->map_idx;
}

#define INTER_REFS_PER_FRAME (MAX_REF_FRAMES_EX - 1)
u32 av1dec_set_frame_refs(struct Av1DecContainer *dec_cont,
                          struct DecAsicBuffers *asic_buff,
                          i32 lst_map_idx, i32 gld_map_idx) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  int lst_frame_sort_idx = -1;
  int gld_frame_sort_idx = -1;

  int cur_offset = dec->frame_offset;
  int cur_frame_sort_idx = 1 << dec->order_hint_bits_minus1;

  REF_FRAME_INFO ref_frame_info[NUM_REF_FRAMES];
  int ref_flag_list[7] = {0};

  for (int i = 0; i < NUM_REF_FRAMES; i++) {
    ref_frame_info[i].map_idx = i;
    ref_frame_info[i].sort_idx = -1;
    int buf_idx = Av1BufferQueueGetRef(dec_cont->bq, i);
    ref_frame_info[i].buf_idx = buf_idx;
    if (buf_idx < 0) continue;

    if (dec_cont->pp_enabled)
      buf_idx = Av1BufferQueueGetRef(dec_cont->pp_bq, i);

    int offset = asic_buff->picture_info[buf_idx].frame_offset;
    ref_frame_info[i].sort_idx =
        (offset == -1)
            ? cur_frame_sort_idx
            : cur_frame_sort_idx + GetRelativeDist1(dec, offset, cur_offset);
    if (i == lst_map_idx) lst_frame_sort_idx = ref_frame_info[i].sort_idx;
    if (i == gld_map_idx) gld_frame_sort_idx = ref_frame_info[i].sort_idx;
  }
  if (lst_frame_sort_idx == -1 || lst_frame_sort_idx >= cur_frame_sort_idx) {
    return HANTRO_NOK;
  }
  if (gld_frame_sort_idx == -1 || gld_frame_sort_idx >= cur_frame_sort_idx) {
    return HANTRO_NOK;
  }
  qsort(ref_frame_info, NUM_REF_FRAMES, sizeof(REF_FRAME_INFO),
        compare_ref_frame_info);

  // Identify forward and backward reference frames.
  // Forward  reference: offset < cur_frame_offset
  // Backward reference: offset >= cur_frame_offset
  int fwd_start_idx = 0, fwd_end_idx = NUM_REF_FRAMES - 1;

  for (int i = 0; i < NUM_REF_FRAMES; i++) {
    if (ref_frame_info[i].sort_idx == -1) {
      fwd_start_idx++;
      continue;
    }

    if (ref_frame_info[i].sort_idx >= cur_frame_sort_idx) {
      fwd_end_idx = i - 1;
      break;
    }
  }

  int bwd_start_idx = fwd_end_idx + 1;
  int bwd_end_idx = NUM_REF_FRAMES - 1;

  // === Backward Reference Frames ===

  // == ALTREF_FRAME ==
  if (bwd_start_idx <= bwd_end_idx) {
    set_ref_frame_info(dec, ALTREF_FRAME_EX - LAST_FRAME,
                       &ref_frame_info[bwd_end_idx]);
    ref_flag_list[ALTREF_FRAME_EX - LAST_FRAME] = 1;
    bwd_end_idx--;
  }

  // == BWDREF_FRAME ==
  if (bwd_start_idx <= bwd_end_idx) {
    set_ref_frame_info(dec, BWDREF_FRAME_EX - LAST_FRAME,
                       &ref_frame_info[bwd_start_idx]);
    ref_flag_list[BWDREF_FRAME_EX - LAST_FRAME] = 1;
    bwd_start_idx++;
  }

  // == ALTREF2_FRAME ==
  if (bwd_start_idx <= bwd_end_idx) {
    set_ref_frame_info(dec, ALTREF2_FRAME_EX - LAST_FRAME,
                       &ref_frame_info[bwd_start_idx]);
    ref_flag_list[ALTREF2_FRAME_EX - LAST_FRAME] = 1;
  }

  // === Forward Reference Frames ===

  for (int i = fwd_start_idx; i <= fwd_end_idx; ++i) {
    // == LAST_FRAME ==
    if (ref_frame_info[i].map_idx == lst_map_idx) {
      set_ref_frame_info(dec, LAST_FRAME - LAST_FRAME, &ref_frame_info[i]);
      ref_flag_list[LAST_FRAME - LAST_FRAME] = 1;
    }

    // == GOLDEN_FRAME ==
    if (ref_frame_info[i].map_idx == gld_map_idx) {
      set_ref_frame_info(dec, GOLDEN_FRAME_EX - LAST_FRAME, &ref_frame_info[i]);
      ref_flag_list[GOLDEN_FRAME_EX - LAST_FRAME] = 1;
    }
  }

  /*
  assert(ref_flag_list[LAST_FRAME - LAST_FRAME] == 1 &&
         ref_flag_list[GOLDEN_FRAME_EX - LAST_FRAME] == 1);
         */

  // == LAST2_FRAME ==
  // == LAST3_FRAME ==
  // == BWDREF_FRAME ==
  // == ALTREF2_FRAME ==
  // == ALTREF_FRAME ==

  // Set up the reference frames in the anti-chronological order.
  static const int ref_frame_list[INTER_REFS_PER_FRAME - 2] = {
      LAST2_FRAME_EX, LAST3_FRAME_EX, BWDREF_FRAME_EX, ALTREF2_FRAME_EX,
      ALTREF_FRAME_EX};

  int ref_idx;
  for (ref_idx = 0; ref_idx < (INTER_REFS_PER_FRAME - 2); ref_idx++) {
    const int ref_frame = ref_frame_list[ref_idx];

    if (ref_flag_list[ref_frame - LAST_FRAME] == 1) continue;

    while (fwd_start_idx <= fwd_end_idx &&
           (ref_frame_info[fwd_end_idx].map_idx == lst_map_idx ||
            ref_frame_info[fwd_end_idx].map_idx == gld_map_idx)) {
      fwd_end_idx--;
    }
    if (fwd_start_idx > fwd_end_idx) break;

    set_ref_frame_info(dec, ref_frame - LAST_FRAME,
                       &ref_frame_info[fwd_end_idx]);
    ref_flag_list[ref_frame - LAST_FRAME] = 1;

    fwd_end_idx--;
  }

  // Assign all the remaining frame(s), if any, to the earliest reference frame.
  for (; ref_idx < (INTER_REFS_PER_FRAME - 2); ref_idx++) {
    const int ref_frame = ref_frame_list[ref_idx];
    if (ref_flag_list[ref_frame - LAST_FRAME] == 1) continue;
    set_ref_frame_info(dec, ref_frame - LAST_FRAME,
                       &ref_frame_info[fwd_start_idx]);
    ref_flag_list[ref_frame - LAST_FRAME] = 1;
  }
  return HANTRO_OK;
}

//#define MIN(a, b) ((a) < (b) ? (a) : (b))
//#define MAX(a, b) ((a) > (b) ? (a) : (b))

void Av1SetFrameSignBias(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (!dec->enable_order_hint || dec->intra_only || dec->key_frame) return;

  // Identify the nearest forward and backward references.
  for (int i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    int ref_frame_idx;
    if (!dec_cont->pp_enabled) {
      ref_frame_idx =
        Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
    } else {
      ref_frame_idx =
        Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
    }

    if (ref_frame_idx != kReferenceNotSet) {
      int ref_frame_offset =
          asic_buff->picture_info[ref_frame_idx].frame_offset;

      int rel_off = GetRelativeDist1(dec, ref_frame_offset, dec->frame_offset);
      dec->ref_frame_sign_bias[i + 1] = (rel_off <= 0) ? 0 : 1;
    }
  }
}

int Av1SkipModeAllowed(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

  if (!dec->enable_order_hint || dec->intra_only || dec->key_frame ||
      dec->comp_pred_mode == SINGLE_PREDICTION_ONLY) {
    return 0;
  }

  // Identify the nearest forward and backward references.
  int ref0 = NONE, ref1 = NONE;
  int ref0_off = -1, ref1_off = -1;
  for (int i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
    int ref_frame_idx;
    if (!dec_cont->pp_enabled) {
      ref_frame_idx =
        Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
    } else {
      ref_frame_idx =
        Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
    }

    if (ref_frame_idx != kReferenceNotSet) {
      int ref_frame_offset =
          asic_buff->picture_info[ref_frame_idx].frame_offset;

      int rel_off = GetRelativeDist1(dec, ref_frame_offset, dec->frame_offset);
      // Forward reference
      if (rel_off < 0 &&
          (ref0_off == -1 ||
           GetRelativeDist1(dec, ref_frame_offset, ref0_off) > 0)) {
        ref0 = i + LAST_FRAME;
        ref0_off = ref_frame_offset;
      }
      // Backward reference
      if (rel_off > 0 &&
          (ref1_off == -1 ||
           GetRelativeDist1(dec, ref_frame_offset, ref1_off) < 0)) {
        ref1 = i + LAST_FRAME;
        ref1_off = ref_frame_offset;
      }
    }
  }

  if (ref0 != NONE && ref1 != NONE) {
    // == Bi-directional prediction ==
    dec->skip_ref0 = MIN(ref0, ref1);
    dec->skip_ref1 = MAX(ref0, ref1);
    return 1;
  } else if (ref0 != NONE) {
    // == Forward prediction only ==
    // Identify the second nearest forward reference.
    for (int i = 0; i < AV1_ACTIVE_REFS_EX; i++) {
      int ref_frame_idx;
      if (!dec_cont->pp_enabled) {
        ref_frame_idx =
          Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[i]);
      } else {
        ref_frame_idx =
          Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[i]);
      }
      if (ref_frame_idx != kReferenceNotSet) {
        int ref_frame_offset =
            asic_buff->picture_info[ref_frame_idx].frame_offset;
        // Forward reference
        if (GetRelativeDist1(dec, ref_frame_offset, ref0_off) < 0 &&
            (ref1_off == -1 ||
             GetRelativeDist1(dec, ref_frame_offset, ref1_off) > 0)) {
          ref1 = i + LAST_FRAME;
          ref1_off = ref_frame_offset;
        }
      }
    }
    if (ref1 != NONE) {
      dec->skip_ref0 = MIN(ref0, ref1);
      dec->skip_ref1 = MAX(ref0, ref1);
      return 1;
    }
  }

  return 0;
}

u32 ReadTileInfo(struct Av1Decoder *dec, struct StrmData *rbp) {
  int sb_size_log2 = dec->sb_size ? 7 : 6;
  int width_sb = (dec->width + (1 << sb_size_log2) - 1) >> sb_size_log2;
  int height_sb = (dec->height + (1 << sb_size_log2) - 1) >> sb_size_log2;
  int max_tile_width_sb = MAX_TILE_WIDTH / (dec->sb_size ? 128 : 64);
  int max_tile_area_sb = (width_sb * height_sb);
  int widest_tile_sb = 0;
  u32 i, tmp, start_sb;

  if (0) {  // large_scale_tile
    if (dec->sb_size) {
    } else {
      dec->tile_width = SwGetBits(rbp, 5) + 1;
      dec->tile_height = SwGetBits(rbp, 5) + 1;
    }
  } else {
    /* Tile dimensions */
    u32 min_log2_tile_cols, max_log2_tile_cols, min_log2_tiles,
        max_log2_tile_rows;
    GetTileLimits(dec, &min_log2_tile_cols, &max_log2_tile_cols,
                  &min_log2_tiles, &max_log2_tile_rows);
    dec->uniform_tile_spacing = SwGetBits(rbp, 1);
    if (dec->uniform_tile_spacing) {
      u32 log2_col = min_log2_tile_cols;
      while (log2_col < max_log2_tile_cols) {
        tmp = SwGetBits(rbp, 1);
        if (tmp == END_OF_STREAM) return HANTRO_NOK;
        if (tmp) {
          log2_col++;
        } else {
          break;
        }
      }

      dec->log2_tile_columns = log2_col;
      u8 tile_width_sb = (width_sb + (1 << log2_col) - 1) >> log2_col;
      if (tile_width_sb)
        dec->av1_tile_cols = (width_sb + tile_width_sb - 1) / tile_width_sb;

      for (i = 0; i < dec->av1_tile_cols + 1; i++) {
        dec->tile_col_start_sb[i] = MIN(i * tile_width_sb, width_sb);
        if (i > 0)
          dec->tile_col_width_sb[i - 1] = dec->tile_col_start_sb[i] -
                                          dec->tile_col_start_sb[i - 1];
      }
      widest_tile_sb = tile_width_sb;
      STREAMTRACE_I("log2_tile_columns: %d\n", dec->log2_tile_columns);
    } else {  // custom tile
      for (i = 0, start_sb = 0; width_sb > 0 && i < AV1_MAX_TILE_COLS; i++) {
        const int size_sb =
            1 + Av1ReadUniform(rbp, MIN(width_sb, max_tile_width_sb));
        dec->tile_col_start_sb[i] = start_sb;
        start_sb += size_sb;
        width_sb -= size_sb;
        widest_tile_sb = MAX(size_sb, widest_tile_sb);
      }
      dec->av1_tile_cols = i;
      dec->log2_tile_columns = tile_log2(1, i);
      dec->tile_col_start_sb[i] = start_sb;
      for (i = 1; i < dec->av1_tile_cols + 1; i++) {
        dec->tile_col_width_sb[i - 1] = dec->tile_col_start_sb[i] -
                                        dec->tile_col_start_sb[i - 1];
      }

      if (min_log2_tiles) {
        max_tile_area_sb >>= (min_log2_tiles + 1);
      }
    }

    if (dec->uniform_tile_spacing) {
      u32 log2_row = MAX((int)(min_log2_tiles - dec->log2_tile_columns), 0);
      while (log2_row < max_log2_tile_rows) {
        tmp = SwGetBits(rbp, 1);
        if (tmp == END_OF_STREAM) return HANTRO_NOK;
        if (tmp) {
          log2_row++;
        } else {
          break;
        }
      }

      dec->log2_tile_rows = log2_row;
      u8 tile_height_sb = (height_sb + (1 << log2_row) - 1) >> log2_row;
      if (tile_height_sb)
        dec->av1_tile_rows = (height_sb + tile_height_sb - 1) / tile_height_sb;

      for (int i = 0; i < dec->av1_tile_rows + 1; i++) {
        dec->tile_row_start_sb[i] = MIN(i * tile_height_sb, height_sb);
        if (i > 0)
          dec->tile_row_height_sb[i - 1] = dec->tile_row_start_sb[i] -
                                           dec->tile_row_start_sb[i - 1];
      }
      STREAMTRACE_I("log2_tile_rows: %d\n", dec->log2_tile_rows);
    } else {  // custom tile
      int max_tile_height_sb;
      /* take widest_tile_sb = 0 into consideration */
      if (widest_tile_sb)
        max_tile_height_sb = MAX(max_tile_area_sb / widest_tile_sb, 1);
      else
        max_tile_height_sb = 1;

      for (i = 0, start_sb = 0; height_sb > 0 && i < AV1_MAX_TILE_ROWS; i++) {
        const int size_sb =
            1 + Av1ReadUniform(rbp, MIN(height_sb, max_tile_height_sb));
        dec->tile_row_start_sb[i] = start_sb;
        start_sb += size_sb;
        height_sb -= size_sb;
      }
      dec->av1_tile_rows = i;
      dec->log2_tile_rows = tile_log2(1, i);
      dec->tile_row_start_sb[i] = start_sb;

      for (i = 1; i < dec->av1_tile_rows + 1; i++) {
        dec->tile_row_height_sb[i - 1] = dec->tile_row_start_sb[i] -
                                         dec->tile_row_start_sb[i - 1];
      }
    }

    if (dec->log2_tile_rows || dec->log2_tile_columns) {
      dec->context_update_tile_id =
          SwGetBits(rbp, dec->log2_tile_rows + dec->log2_tile_columns);
      if (dec->context_update_tile_id >=
          dec->av1_tile_cols * dec->av1_tile_rows)
        return HANTRO_NOK;
      dec->tile_sz_mag = SwGetBits(rbp, 2);
    } else {
      dec->context_update_tile_id = 0;
      dec->tile_sz_mag = 3;
    }
  }
  if (dec->av1_tile_cols * dec->av1_tile_rows > AV1_MAX_TILES) {
    return HANTRO_NOK;
  }

  return HANTRO_OK;
}
void check_same_sequence_header(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  u32 i;
  dec->input_same_seqheadr = 0;
  if (dec->input_sequence_num >= 2) {
    // check if there are two sequence headers with the same parameters.
    // if yes, we think there are no error in the sequence header data.
    int j;
    int same_seq_headers[15];
    int max_num = 0;
    int pos = 0;
    for (i = 0; i < 15; i++) same_seq_headers[i] = 1;
    for (i = 0; i < dec->input_sequence_num - 1; i++) {
      for (j = i + 1; j < dec->input_sequence_num; j++) {
        if (Av1CompareSequenceHeader(dec->seq_hdr[i], dec->seq_hdr[j]) == 0) {
          // same_seq_headers
          same_seq_headers[i]++;
        }
      }
    }
    for (i = 0; i < 15; i++) {
      if (max_num < same_seq_headers[i]) {
        max_num = same_seq_headers[i];
        pos = i;
      }
    }
    dec->input_same_seqheadr = max_num;
    if (max_num >= 2) {
      // there are at least two sequence headers with the same parameters.
      // so we modify dec values with the correct value.
      Av1CheckSequenceHeader(&dec_cont->decoder, dec->seq_hdr[pos]);
    }
  }
}

void update_ref_frame_id(struct Av1Decoder *dec, int frame_id) {
  int refresh_frame_flags = dec->refresh_frame_flags;
  for (int i = 0; i < NUM_REF_FRAMES; i++) {
    if ((refresh_frame_flags >> i) & 1) {
      dec->ref_frame_id[i] = frame_id;
      dec->valid_for_referencing[i] = 1;
    }
  }
}

void Av1SetCDFS(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;

  if (!dec_cont->vcmd_m2m)
    Av1StoreCDFs(dec);
#ifdef SUPPORT_VCMD_M2M
  else {
    struct CDF_INFO *cdfs_load_info = NULL;
    for(int i = 0; i < NUM_REF_FRAMES; i++) {
      if (dec->refresh_frame_flags & (1 << i)) {
        if (&dec_cont->decoder.cdfs_last[i] != dec_cont->decoder.cdfs) {
          cdfs_load_info = &dec->cdfs_info.cdfs_load_info[dec->cdfs_info.load_num];
          cdfs_load_info->cdf_size = sizeof(struct AV1CDFs);
          cdfs_load_info->src_cdf_addr = dec->cdfs_addr;
          cdfs_load_info->dst_prob_addr = dec->cdfs_last_addr +
            ((addr_t)(&dec->cdfs_last[i]) - (addr_t)&dec->cdfs_last[0]);
          dec->cdfs_info.load_num++;
        }
      }
    }
  }
#endif
}

u32 Av1DecodeFrameTag(struct StrmData *rb, struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  /* Use common bit parse but don't remove emulation prevention bytes. */
  u32 frame_type = 0;
  u32 err, tmp, i;
  int frame_size_override_flag = 1;
  u32 read_bits = rb->strm_buff_read_bits;

  {
    const u32 max_ref_frames = ALLOWED_REFS_PER_FRAME_EX;
    struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;

    dec->primary_ref_frame = ALLOWED_REFS_PER_FRAME_EX;
    dec->reset_frame_flags = 0;
    dec->frame_refs_short_signaling = 0;
    dec->show_existing_frame =
        dec->reduced_still_picture_hdr ? 0 : SwGetBits(rb, 1);
    STREAMTRACE_I("Show existing frame: %d\n", dec->show_existing_frame);
    if (dec->show_existing_frame) {
      i32 idx_to_show = SwGetBits(rb, 3);
      dec->show_existing_frame_index = dec->ref_frame_map[idx_to_show];

      dec->loop_filter_level = 0;

      if (dec->decoder_model_info_present_flag &&
          dec->equal_picture_interval == 0) {
        dec->frame_presentation_time =
            SwGetBits(rb, dec->frame_presentation_time_length);
      }

      dec->refresh_frame_flags = 0;

      i32 pic_idx = Av1BufferQueueGetRef(dec_cont->bq, dec->show_existing_frame_index);
      struct Av1DecPicture *pic;
      if (!dec_cont->pp_enabled)
        pic = &dec_cont->asic_buff->picture_info[pic_idx];
      else
        pic = &dec_cont->asic_buff->picture_info[dec_cont->asic_buff->pp_buffer_map[pic_idx]];

      // KEY_FRAME refreshes allFrames
      if (pic->is_intra_frame) {
        dec->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;
        Av1BufferQueueUpdateRef(dec_cont->bq, dec->refresh_frame_flags,
                                pic_idx);
        Av1BufferQueueUpdateRef(dec_cont->pp_bq, dec->refresh_frame_flags,
                                dec_cont->asic_buff->pp_buffer_map[pic_idx]);
        Av1GetCDFs(dec, dec->show_existing_frame_index);
        Av1SetCDFS(dec_cont);
        LoadLfParams(dec_cont, dec->show_existing_frame_index);
        StoreLfParams(dec_cont);
      }

      if (dec->frame_id_numbers_present_flag) {
        int frame_id_length = dec->frame_id_length;
        dec->display_frame_id = SwGetBits(rb, frame_id_length);
        if (dec->display_frame_id != dec->ref_frame_id[idx_to_show] ||
            !dec->valid_for_referencing[idx_to_show])
          return HANTRO_NOK;
      }

      if (dec->film_grain_params_present) {
        // load_grain_params( idx_to_show )
        // ASSERT(0);  //TODO
      }
      if (pic->is_intra_frame && dec->frame_id_numbers_present_flag) {
        dec->curr_frame_id = dec->ref_frame_id[idx_to_show];
        update_ref_frame_id(dec, dec->ref_frame_id[idx_to_show]);
      }

      return HANTRO_OK;
    }

    frame_type = dec->reduced_still_picture_hdr ? 0 : SwGetBits(rb, 2);
    dec->key_frame = !frame_type;
    dec->frame_type = frame_type;
    dec->show_frame = dec->reduced_still_picture_hdr ? 1 : SwGetBits(rb, 1);
    dec->intra_only = dec->frame_type == 2;
    if (dec->still_picture && (!dec->key_frame || !dec->show_frame))
      return HANTRO_NOK;
    if (dec->show_frame) {
      if (dec->decoder_model_info_present_flag &&
          dec->equal_picture_interval == 0) {
        dec->frame_presentation_time =
            SwGetBits(rb, dec->frame_presentation_time_length);
      }
      dec->showable_frame = !dec->key_frame;
    } else {
      dec->showable_frame = SwGetBits(rb, 1);
    }
    if (dec->reduced_still_picture_hdr || (dec->key_frame && dec->show_frame) ||
        frame_type == 3)
      dec->error_resilient = 1;
    else
      dec->error_resilient = SwGetBits(rb, 1);
    if (dec->error_resilient == END_OF_STREAM) return HANTRO_NOK;

    dec->allow_intrabc = 0;

    STREAMTRACE_I("Key frame: %d\n", dec->key_frame);
    STREAMTRACE_I("Show frame: %d\n", dec->show_frame);
    STREAMTRACE_I("Error resilient: %d\n", dec->error_resilient);

    dec->disable_cdf_update = SwGetBits(rb, 1);
    if (dec->force_screen_content_tools == 2)
      dec->allow_screen_content_tools = SwGetBits(rb, 1);
    else
      dec->allow_screen_content_tools = dec->force_screen_content_tools;

    if (dec->allow_screen_content_tools) {
      if (dec->force_integer_mv == 2)
        dec->frm_force_integer_mv = SwGetBits(rb, 1);
      else
        dec->frm_force_integer_mv = dec->force_integer_mv;
    } else
      dec->frm_force_integer_mv = 0;

    if (!dec->reduced_still_picture_hdr) {
      if (dec->frame_id_numbers_present_flag) {
        int frame_id_length = dec->frame_id_length;
        int diff_len = dec->delta_frame_id_length;
        (void)diff_len;
        i32 prev_frame_id = 0;
        if (frame_type != 0 || !dec->show_frame)
          prev_frame_id = dec->curr_frame_id;
        dec->curr_frame_id = SwGetBits(rb, frame_id_length);

        if (frame_type != 0 || !dec->show_frame) {
          int diff_frame_id = 0;
          if (dec->curr_frame_id > prev_frame_id)
            diff_frame_id = dec->curr_frame_id - prev_frame_id;
          else {
            diff_frame_id =
                (1 << frame_id_length) + dec->curr_frame_id - prev_frame_id;
          }
          if (prev_frame_id == dec->curr_frame_id ||
              diff_frame_id >= (1 << (frame_id_length - 1)))
            return HANTRO_NOK;
          /*current_frame_id specifies the frame id number for the current
frame. Frame id numbers are additional information that do not affect the
decoding process, but provide decoders with a way of detecting missing reference
frames so that appropriate action can be taken.*/
        }

        for (int i = 0; i < NUM_REF_FRAMES; i++) {
          if (dec->key_frame && dec->show_frame) {
            dec->valid_for_referencing[i] = 0;
          } else if (dec->curr_frame_id - (1 << diff_len) > 0) {
            if (dec->ref_frame_id[i] > dec->curr_frame_id ||
                dec->ref_frame_id[i] < dec->curr_frame_id - (1 << diff_len))
              dec->valid_for_referencing[i] = 0;
          } else {
            if (dec->ref_frame_id[i] > dec->curr_frame_id &&
                dec->ref_frame_id[i] < (1 << frame_id_length) +
                                           dec->curr_frame_id - (1 << diff_len))
              dec->valid_for_referencing[i] = 0;
          }
        }
      }
      frame_size_override_flag = frame_type == 3 ? 1 : SwGetBits(rb, 1);

      dec->frame_offset = SwGetBits(rb, dec->order_hint_bits_minus1 + 1);
      dec->current_video_frame = dec->frame_offset;

      if (!dec->error_resilient && !frame_is_intra_only(dec)) {
        dec->primary_ref_frame = SwGetBits(rb, PRIMARY_REF_BITS);
      }
    } else {
      frame_size_override_flag = 0;
    }

    if (dec->decoder_model_info_present_flag) {
      dec->buffer_removal_delay_present_flag = SwGetBits(rb, 1);
      if (dec->buffer_removal_delay_present_flag) {
        for (int i = 0; i < (int)dec->operating_points_cnt; i++) {
          if (dec->decoder_model_present[i]) {
            u32 opPtIdc = dec->operating_point_idc[i];
            u32 inTemporalLayer =
                (opPtIdc >> dec->obu_hdr.temporal_layer_id) & 1;
            u32 inSpatialLayer =
                (opPtIdc >> (dec->obu_hdr.spatial_layer_id + 8)) & 1;
            if (opPtIdc == 0 || (inTemporalLayer && inSpatialLayer)) {
              u32 n = dec->buffer_removal_time_length;
              dec->buffer_removal_time[i] = SwGetBits(rb, n);
            }
          }
        }
      }
    }

    if (dec->key_frame) {
      dec->reset_frame_flags = dec->show_frame;
      if (!dec->show_frame)
        dec->refresh_frame_flags = SwGetBits(rb, NUM_REF_FRAMES);
      else
        dec->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;

      for (i = 0; i < max_ref_frames; i++) {
        dec->active_ref_idx[i] = 0; /* TODO next free frame buffer */
        ExistingRefRegister(dec, 1 << i);
      }
    } else {
      if (dec->intra_only || frame_type != 3) {
        dec->refresh_frame_flags = SwGetBits(rb, NUM_REF_FRAMES);
        if (dec->intra_only && dec->refresh_frame_flags == 0xFF)
          return HANTRO_NOK;
      } else
        dec->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;
    }

    // TODO: this needs to be done for fwd keyframes -> outside the branch
    if ((!frame_is_intra_only(dec) || dec->refresh_frame_flags != 0xFF) &&
        dec->enable_order_hint && dec->error_resilient) {
      for (int i = 0; i < NUM_REF_FRAMES; i++) {
        u32 frame_offset = SwGetBits(rb, dec->order_hint_bits_minus1 + 1);
        int buf_idx = dec->ref_frame_map[i];

        if (buf_idx == -1) {
          // TODO: handle missing
          (void)buf_idx;
          (void)frame_offset;
        }
      }
    }

    dec->use_ref_frame_mvs = 0;
    if (frame_is_intra_only(dec)) {
      check_same_sequence_header(dec_cont);
      if (SetupFrameSize(rb, dec_cont, frame_size_override_flag) == HANTRO_NOK)
        return HANTRO_NOK;
      if (dec_cont->initial_bitdepth == 0 /* Not set */) {
        dec_cont->initial_bitdepth = dec->bit_depth;
      }
      u32 superres_unscaled = (dec->width < dec->superres_width) ? 0 : 1;
      if (dec->allow_screen_content_tools && superres_unscaled)
        dec->allow_intrabc = SwGetBits(rb, 1);
    } else {
      // frame_refs_signaling
      if (dec->enable_order_hint)
        dec->frame_refs_short_signaling = SwGetBits(rb, 1);

      if (dec->frame_refs_short_signaling) {
        int last_ref = SwGetBits(rb, NUM_REF_FRAMES_LG2);
        // int last_idx = dec->ref_frame_map[last_ref];

        int gld_ref = SwGetBits(rb, NUM_REF_FRAMES_LG2);
        // int gld_idx = dec->ref_frame_map[gld_ref];

        if (av1dec_set_frame_refs(dec_cont, asic_buff, last_ref,
                                  gld_ref) == HANTRO_NOK)
          return HANTRO_NOK;
      }

      for (i = 0; i < max_ref_frames; i++) {  // 7
        i32 ref_frame_num = 0;
        if (!dec->frame_refs_short_signaling) {
          ref_frame_num = SwGetBits(rb, NUM_REF_FRAMES_LG2);
          if (ref_frame_num == END_OF_STREAM) return HANTRO_NOK;
          i32 mapped_ref = dec->ref_frame_map[ref_frame_num];
          STREAMTRACE_I("active_reference_frame_idx: %d\n", ref_frame_num);
          STREAMTRACE_I("active_reference_frame_num: %d\n", mapped_ref);
          dec->active_ref_idx[i] = mapped_ref;
        } else {
          ref_frame_num = dec->active_ref_idx[i];
        }
        dec->ref_frame_sign_bias[i + 1] = 0;

        if (dec->frame_id_numbers_present_flag) {
          int frame_id_length = dec->frame_id_length;
          int diff_len = dec->delta_frame_id_length;
          int delta_frame_id_minus1 = SwGetBits(rb, diff_len);
          int ref_frame_id = (dec->curr_frame_id - (delta_frame_id_minus1 + 1) +
                              (1 << frame_id_length)) %
                             (1 << frame_id_length);
          (void)ref_frame_id;
          if (ref_frame_id != dec->ref_frame_id[ref_frame_num] ||
              !dec->valid_for_referencing[ref_frame_num]) {
            return HANTRO_NOK;
          }
        }
      }

#if 1
      check_same_sequence_header(dec_cont);
      // add the following for error concealment, carl.
      // see spec. p216, 7.5 Ordering of OBUs.
      // the first frame header has frame_type equal to KEY_FRAME,show_frame=1
      // show_existing_frame equal to 0,and temporal_id equal to 0
      if (dec_cont->decoder.key_frame && dec_cont->decoder.show_frame &&
          dec_cont->decoder.show_existing_frame == 0 &&
          dec_cont->decoder.obu_hdr.temporal_layer_id == 0) {
        // dec_cont->decoder.obu_seq_hdr_checked.sequencer_ok=0;
      } else {
        // check if sequence data are reasonable.
        Av1ModifySequenceHeader(&dec_cont->decoder);
      }
#endif

      if (dec->error_resilient == 0 && frame_size_override_flag) {
        if (SetupFrameSizeWithRefs(rb, dec_cont))  // 8
          return HANTRO_NOK;
      } else {
        if (SetupFrameSize(rb, dec_cont, frame_size_override_flag) ==
            HANTRO_NOK)
          return HANTRO_NOK;
      }

      if (dec->frm_force_integer_mv)
        dec->allow_high_precision_mv = 0;
      else
        dec->allow_high_precision_mv = SwGetBits(rb, 1);
      STREAMTRACE_I("high_precision_mv: %d\n", dec->allow_high_precision_mv);

      tmp = SwGetBits(rb, 1);
      STREAMTRACE_I("mb_switchable_mcomp_filt: %d\n", tmp);
      if (tmp) {
        dec->mcomp_filter_type = SWITCHABLE;
      } else {
        dec->mcomp_filter_type = SwGetBits(rb, 2);
        STREAMTRACE_I("mcomp_filter_type: %d\n", dec->mcomp_filter_type);
      }
      dec->switchable_motion_mode = SwGetBits(rb, 1);

      dec->allow_comp_inter_inter = 1;
      if (/*!dec->intra_only &&*/ !dec->error_resilient &&
          dec->enable_ref_frame_mvs && dec->enable_order_hint) {
        dec->use_ref_frame_mvs = SwGetBits(rb, 1);
      } else {
        dec->use_ref_frame_mvs = 0;
      }
      ExistingRefRegister(dec, dec->refresh_frame_flags);
    }

    if (dec->key_frame || dec->error_resilient || dec->intra_only)
      Av1ResetDecoder(&dec_cont->decoder);

    // TODO: update frame ids in ref buffer

    Av1SetFrameSignBias(dec_cont);

    if (!dec->reduced_still_picture_hdr && !dec->disable_cdf_update) {
      /* Refresh entropy probs,
       * 0 == No refresh
       * 1 == Forward refresh
       * 2 == Backward refresh
       */
      dec->refresh_entropy_probs = SwGetBits(rb, 1) ? 0 : 1;
      STREAMTRACE_I("Refresh frame context: %d\n", dec->refresh_entropy_probs);
    } else {
      dec->refresh_entropy_probs = 0;
    }

    dec->frame_context_idx = 0;  // TODO

    err = ReadTileInfo(dec, rb);
    if (err == HANTRO_NOK) return err;

    /* Quantizers */
    dec->qp_yac = SwGetBits(rb, 8);
    STREAMTRACE_I("qp_y_ac: %d\n", dec->qp_yac);
    dec->qp_ydc = DecodeQuantizerDelta(rb);
    if (!dec->monochrome) {
      int diff_uv_delta = 0;
      if (dec->separate_uv_delta_q) {
        diff_uv_delta = SwGetBits(rb, 1);
      }
      dec->qp_ch_dc = DecodeQuantizerDelta(rb);
      dec->qp_ch_ac = DecodeQuantizerDelta(rb);
      if (diff_uv_delta) {
        dec->qp_cv_dc = DecodeQuantizerDelta(rb);
        dec->qp_cv_ac = DecodeQuantizerDelta(rb);
      } else {
        dec->qp_cv_dc = dec->qp_ch_dc;
        dec->qp_cv_ac = dec->qp_ch_ac;
      }
    } else {
      dec->qp_ch_dc = 0;
      dec->qp_ch_ac = 0;
      dec->qp_cv_dc = 0;
      dec->qp_cv_ac = 0;
    }

#define QM_LEVEL_BITS 4
    dec->using_qmatrix = SwGetBits(rb, 1);
    if (dec->using_qmatrix) {
      dec->qm_y = SwGetBits(rb, QM_LEVEL_BITS);
      dec->qm_u = SwGetBits(rb, QM_LEVEL_BITS);
      if (!dec->separate_uv_delta_q)
        dec->qm_v = dec->qm_u;
      else
        dec->qm_v = SwGetBits(rb, QM_LEVEL_BITS);
    } else {
      dec->qm_y = 0;
      dec->qm_u = 0;
      dec->qm_v = 0;
    }

#if DEBUG_AOM_QM
    fprintf(stderr, "cm->using_qmatrix %u y %u u %u v %u\n", dec->using_qmatrix,
            dec->qm_y, dec->qm_u, dec->qm_v);
#endif

    /* Setup segment based adjustments */
    DecodeSegmentationDataAV1(rb, dec_cont);  // 11

    bool segment_quantizer_active = FALSE;
    for (int i = 0; i < MAX_MB_SEGMENTS; i++) {  // 12
      int q = dec->segment_enabled && dec->segment_feature_enable[i][0]
                  ? (dec->segment_feature_mode == AV1_SEG_FEATURE_ABS
                         ? dec->segment_feature_data[i][0]
                         : dec->segment_feature_data[i][0] + dec->qp_yac)
                  : dec->qp_yac;
      q = CLIP3(0, 255, q);
      dec->lossless[i] = q == 0 && dec->qp_ydc == 0 && dec->qp_ch_dc == 0 &&
                         dec->qp_ch_ac == 0 && dec->qp_cv_dc == 0 &&
                         dec->qp_cv_ac == 0;
      if (dec->segment_feature_enable[i][0]) segment_quantizer_active = TRUE;
    }

    (void)segment_quantizer_active;

    int coded_lossless = dec->lossless[0];
    if (dec->segment_enabled) {
      for (int i = 1; i < MAX_MB_SEGMENTS; i++) {
        coded_lossless &= dec->lossless[i];
      }
    }

    int all_lossless = coded_lossless && (dec->width == dec->superres_width);

    dec->av1_delta_q_present = 0;
    dec->delta_lf_present_flag = 0;
    if (dec->qp_yac > 0) {
      dec->av1_delta_q_present = SwGetBits(rb, 1);
      if (dec->av1_delta_q_present) {
        dec->av1_delta_q_res_log = SwGetBits(rb, 2);
        dec->delta_lf_present_flag = 0;
        if (!dec->allow_intrabc) dec->delta_lf_present_flag = SwGetBits(rb, 1);
        if (dec->delta_lf_present_flag) {
          dec->av1_delta_lf_res_log = SwGetBits(rb, 2);
          dec->av1_delta_lf_multi = SwGetBits(rb, 1);
        }
      }
    }

    DecodeLfParams(rb, dec_cont, coded_lossless);

    if (!all_lossless) {
      // printf("allow_intrabc=%d, enable_cdef=%d, enable_restoration=%d\n",
      // dec->allow_intrabc, dec->enable_cdef, dec->enable_restoration);

      if (!dec->allow_intrabc) {
        if (dec->enable_cdef && !coded_lossless) {
          // cdef_damping corresponds to cdef_pri_damping and cdef_sec_damping
          // in libaom The value needs to be adjusted by adding 3 when it is
          // used in filter_cdef_proc
          dec->cdef_damping = SwGetBits(rb, 2);
          dec->cdef_bits = SwGetBits(rb, 2);
          STREAMTRACE_I("cdef_damping: %d\n", dec->cdef_damping);
          STREAMTRACE_I("cdef_bits: %d\n", dec->cdef_bits);
          // printf("CDEF: Parsing damping=%d, bits=%d\n",
          // int(dec->cdef_damping+3), int(dec->cdef_bits));
          for (int i = 0; i < 8; i++) {
            if (i == (1 << dec->cdef_bits)) break;
            dec->cdef_luma_primary_strength[i] = SwGetBits(rb, 4);
            dec->cdef_luma_secondary_strength[i] = SwGetBits(rb, 2);
            if (!dec->monochrome) {
              dec->cdef_chroma_primary_strength[i] = SwGetBits(rb, 4);
              dec->cdef_chroma_secondary_strength[i] = SwGetBits(rb, 2);
            } else {
              dec->cdef_chroma_primary_strength[i] = 0;
              dec->cdef_chroma_secondary_strength[i] = 0;
            }
            // printf("CDEF: Parsing (%d, %d)\n",
            // int(dec->cdef_luma_primary_strength[i]*4
            // +dec->cdef_luma_secondary_strength[i]),
            //                                   int(dec->cdef_chroma_primary_strength[i]*4
            //                                   +
            //                                   dec->cdef_chroma_secondary_strength[i]));
          }

        } else {
          dec->cdef_damping = 0;
          dec->cdef_bits = 0;
          dec->cdef_luma_primary_strength[0] = 0;
          dec->cdef_luma_secondary_strength[0] = 0;
          dec->cdef_chroma_primary_strength[0] = 0;
          dec->cdef_chroma_secondary_strength[0] = 0;
        }

        STREAMTRACE_I("bits read before lr: %d\n", rb->strm_buff_read_bits);
        // TODO (dkhe): Should not this be inside if (!all_lossless) as above?
        const int n_planes = dec->monochrome ? 1 : 3;
        if (dec->enable_restoration) {
          bool all_none = TRUE;
          bool chroma_none = TRUE;
          for (int pl = 1; pl < 3; pl++) {
            dec->lr_type[pl] = 0;
            dec->lr_unit_size[pl] = 3;
          }
          for (int pl = 0; pl < n_planes; pl++) {
            if (SwGetBits(rb, 1)) {
              dec->lr_type[pl] = (SwGetBits(rb, 1)) ? 2 : 1;
            } else {
              dec->lr_type[pl] = (SwGetBits(rb, 1)) ? 3 : 0;
            }
            if (dec->lr_type[pl] != 0) {
              all_none = 0;
              chroma_none &= (pl == 0);
            }
          }
          // printf("stream read bits=%d\n", int(rb.strm_buff_read_bits));

          if (!all_none) {
            for (int pl = 0; pl < n_planes; pl++) {
              dec->lr_unit_size[pl] = (dec->sb_size) ? 2 : 1;  // 128 : 64
            }
            if (dec->sb_size == 0) {
              dec->lr_unit_size[0] += SwGetBits(rb, 1);
            }
            if (dec->lr_unit_size[0] > 1) {
              dec->lr_unit_size[0] = 2 + SwGetBits(rb, 1);
            }
          } else {
            for (int pl = 0; pl < n_planes; pl++) {
              dec->lr_unit_size[pl] = 3;  // 256
            }
          }
          // TODO (dkhe): These variables need to be decoded.
          dec->subsampling_x = dec->subsampling_y = 1;

          // printf("stream read bits=%d\n", int(rb.strm_buff_read_bits));
          // printf("chroma_none=%d, sub_x=%d, sub_y=%d\n",
          //        int(chroma_none), int(dec->subsampling_x),
          //        int(dec->subsampling_y));
          if (!chroma_none && (dec->subsampling_x && dec->subsampling_y)) {
            dec->lr_unit_size[1] = dec->lr_unit_size[0] - SwGetBits(rb, 1);
            dec->lr_unit_size[2] = dec->lr_unit_size[1];
          } else {
            dec->lr_unit_size[1] = dec->lr_unit_size[0];
            dec->lr_unit_size[2] = dec->lr_unit_size[0];
          }

          for (int pl = 0; pl < n_planes; pl++) {
            STREAMTRACE_I("RESTORATION: p= %d\n", (int)(pl));
            STREAMTRACE_I("RESTORATION: type= %d\n", (int)(dec->lr_type[pl]));
            STREAMTRACE_I("RESTORATION: size= %d\n", (int)(dec->lr_unit_size[pl]));
            // printf("RESTORATION: header pl=%d, type=%d, size=%d\n",
            //       int(pl), int(dec->lr_type[pl]),
            //       int(dec->lr_unit_size[pl]));
          }
        } else {
          for (int pl = 0; pl < 3; pl++) {
            dec->lr_type[pl] = 0;
            dec->lr_unit_size[pl] = 3;
          }
        }
      }
    }
    if (all_lossless || dec->allow_intrabc) {
      dec->loop_filter_level = 0;
      dec->loop_filter_level_r = 0;
      dec->loop_filter_level_u = 0;
      dec->loop_filter_level_v = 0;
      dec->cdef_damping = 0;
      dec->cdef_bits = 0;
      dec->cdef_luma_primary_strength[0] = 0;
      dec->cdef_luma_secondary_strength[0] = 0;
      dec->cdef_chroma_primary_strength[0] = 0;
      dec->cdef_chroma_secondary_strength[0] = 0;
      for (int pl = 0; pl < 3; pl++) {
        dec->lr_type[pl] = 0;
        dec->lr_unit_size[pl] = 3;
      }
    }

    STREAMTRACE_I("bits read: %d\n", rb->strm_buff_read_bits);
    dec->transform_mode =
        coded_lossless ? ONLY_4X4 :  // 13
                                     // simplify_tx_mode
            (SwGetBits(rb, 1) ? TX_MODE_SELECT : TX_MODE_LARGEST);
    STREAMTRACE_I("transform_mode: %d\n", dec->transform_mode);
    if (!frame_is_intra_only(dec)) {
      tmp = SwGetBits(rb, 1);
      dec->comp_pred_mode = tmp ? HYBRID_PREDICTION : SINGLE_PREDICTION_ONLY;
      STREAMTRACE_I("comp_pred_mode: %d\n", dec->comp_pred_mode);
      // printf("HEADER: comp_red_mode=%d\n", int(dec->comp_pred_mode));
    } else {
      dec->comp_pred_mode = SINGLE_PREDICTION_ONLY;
    }

    dec->skip_mode_flag = 0;
    if (Av1SkipModeAllowed(dec_cont)) {
      dec->skip_mode_flag = SwGetBits(rb, 1);
    }
    if (!frame_is_intra_only(dec) && !dec->error_resilient &&
        dec->enable_warped_motion)
      dec->allow_warped_motion = SwGetBits(rb, 1);
    else
      dec->allow_warped_motion = 0;

    dec->reduced_tx_set_used = SwGetBits(rb, 1);
    if (!frame_is_intra_only(dec)) {
      u32 err = DecodeGlobalMotionParams(dec_cont, rb);
      if (err != HANTRO_OK) {
        return HANTRO_NOK;
      }
    }

    // film_grain params
    err = DecodeFilmGrainParams(dec_cont, rb);
    if (err != HANTRO_OK) {
      return HANTRO_NOK;
    }

    if (dec->error_resilient || frame_is_intra_only(dec) ||
        dec->primary_ref_frame == ALLOWED_REFS_PER_FRAME_EX)
      AV1InitCDFs(dec);
    else
      Av1GetCDFs(dec, dec->active_ref_idx[dec->primary_ref_frame]);

    // Set the past CDFs to whatever we ended up with.
    // Later when the frame decoding is done we'll update it one more time in
    // the case of backward refresh.
    Av1SetCDFS(dec_cont);
    if (dec->frame_id_numbers_present_flag)
      update_ref_frame_id(dec, dec->curr_frame_id);
  }
  STREAMTRACE_I("bits read: %d\n", rb->strm_buff_read_bits);

  dec->frame_tag_size = ((rb->strm_buff_read_bits - read_bits) + 7) / 8;
  //  if (rb->strm_buff_read_bits & 0x7)
  //  SwFlushBits(rb, (8 - (rb->strm_buff_read_bits & 0x7)));
  dec->frame_tag_decoded = 1;

  STREAMTRACE_I("First partition size: %d\n", dec->offset_to_dct_parts);
  STREAMTRACE_I("Frame tag size: %d\n", dec->frame_tag_size);

  return HANTRO_OK;
}

u32 Av1DecodeTileGroupHeader(struct StrmData *rb,
                             struct Av1DecContainer *dec_cont,
                             bool *last_tile_group, bool tile_start_implicit) {
  struct StrmData tmp_rb = *rb;
  u32 read_bits = tmp_rb.strm_buff_read_bits;
  struct Av1Decoder *dec = &dec_cont->decoder;
  const int log2_num_tiles = dec->log2_tile_rows + dec->log2_tile_columns;
  int previous_tile_end = dec->tile_end;  // init to -1 for first tile group
  int num_tiles = dec->av1_tile_rows * dec->av1_tile_cols;
  bool tile_start_and_end_present_flag = 0;

  if (num_tiles > 1) {
    tile_start_and_end_present_flag = SwGetBits(&tmp_rb, 1);
  }

  if (tile_start_implicit && tile_start_and_end_present_flag) {
    // "For OBU_FRAME type obu tile_start_and_end_present_flag must be 0"
    return HANTRO_NOK;
  }

  if (num_tiles == 1 || !tile_start_and_end_present_flag) {
    dec->tile_start = 0;
    dec->tile_end = num_tiles - 1;
    dec->tile_start_concealment = dec->tile_end;
  } else {
    dec->tile_start = SwGetBits(&tmp_rb, log2_num_tiles);
    if (dec->tile_start != (dec->tile_start_concealment + 1)) {
      dec->tile_start = (dec->tile_start_concealment + 1);
    }
    dec->tile_end = SwGetBits(&tmp_rb, log2_num_tiles);
    dec->tile_start_concealment = dec->tile_end;
  }

  dec->tile_group_hdr_size = ((tmp_rb.strm_buff_read_bits - read_bits) + 7) / 8;

  *last_tile_group = dec->tile_end == num_tiles - 1;
  if (dec->tile_start <= previous_tile_end || dec->tile_start >= num_tiles ||
      dec->tile_end >= num_tiles || dec->tile_start > dec->tile_end) {
    return HANTRO_NOK;
  }
  return HANTRO_OK;
}

#if 0
u32 Av1DecodeFrameHeader(const u8 *stream, u32 strm_len,
                         struct Av1Decoder *dec) {
  u32 tmp, i, j, k;
  struct Av1EntropyProbs *fc = &dec->entropy; /* Frame context */

  if (dec->width == 0 || dec->height == 0) {
    DEC_HDRS_ERR("Invalid size!\n");
    return HANTRO_NOK;
  }

  const u32 kNumIntraModes = AV1_INTRA_MODES;

  /* Store probs for context backward adaptation. */
  Av1StoreAdaptProbs(dec);
  struct VpBoolCoder bc;
  {
    /* Start bool coder */
    Av1BoolStart(&bc, stream, strm_len);
  }

  /* Setup transform mode and probs */
  {
    if (dec->transform_mode == TX_MODE_SELECT) {
      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
        for (j = 0; j < TX_SIZE_MAX_SB - 3; ++j) {
          tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
          STREAMTRACE_I("tx8x8_prob_update", tmp);
          if (tmp) {
            u8 *prob = &fc->a.tx8x8_prob[i][j];
            *prob = Av1ReadProbDiffUpdate(&bc, *prob);
            STREAMTRACE_I("tx8x8_prob", *prob);
          }
        }
      }

      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
          for (j = 0; j < TX_SIZE_MAX_SB - 2; ++j) {
          tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
          STREAMTRACE_I("tx16x16_prob_update", tmp);
          if (tmp) {
            u8 *prob = &fc->a.tx16x16_prob[i][j];
            *prob = Av1ReadProbDiffUpdate(&bc, *prob);
            STREAMTRACE_I("tx16x16_prob", *prob);
          }
        }
      }

      for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
        for (j = 0; j < TX_SIZE_MAX_SB - 1; ++j) {
          tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
          STREAMTRACE_I("tx32x32_prob_update", tmp);
          if (tmp) {
            u8 *prob = &fc->a.tx32x32_prob[i][j];
            *prob = Av1ReadProbDiffUpdate(&bc, *prob);
            STREAMTRACE_I("tx32x32_prob", *prob);
          }
        }
      }
    }

  /* Coefficient probability update */
    // CB4X4
    // 2x2 txfm probs, parsed but unused with CHROMA_SUB8X8
    tmp = Av1DecodeCoeffUpdate(&bc, fc->a.prob_coeffs);
    if (tmp != HANTRO_OK) return (tmp);

    /* Coefficient probability update */
    tmp = Av1DecodeCoeffUpdate(&bc, fc->a.prob_coeffs);
    if (tmp != HANTRO_OK) return (tmp);
    if (dec->transform_mode > ONLY_4X4) {
      tmp = Av1DecodeCoeffUpdate(&bc, fc->a.prob_coeffs8x8);
      if (tmp != HANTRO_OK) return (tmp);
    }
    if (dec->transform_mode > ALLOW_8X8) {
      tmp = Av1DecodeCoeffUpdate(&bc, fc->a.prob_coeffs16x16);
      if (tmp != HANTRO_OK) return (tmp);
    }
    if (dec->transform_mode > ALLOW_16X16) {
      tmp = Av1DecodeCoeffUpdate(&bc, fc->a.prob_coeffs32x32);
      if (tmp != HANTRO_OK) return (tmp);
    }
  }
  dec->probs_decoded = 1;

  for (k = 0; k < MBSKIP_CONTEXTS; ++k) {
    tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
    STREAMTRACE_I("mbskip_prob_update", tmp);
    if (tmp) {
      fc->a.mbskip_probs[k] = Av1ReadProbDiffUpdate(&bc, fc->a.mbskip_probs[k]);
      STREAMTRACE_I("mbskip_prob", fc->a.mbskip_probs[k]);
    }
  }

  {
    if (dec->segment_enabled && dec->segment_map_update) {
      if (dec->segment_map_temporal_update) {
        for (i = 0; i < PREDICTION_PROBS; i++) {
          tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
          STREAMTRACE_I("segment_pred_prob_update", tmp);
          if (tmp) {
            fc->a.segment_pred_probs[i] = Av1ReadProbDiffUpdate(&bc, fc->a.segment_pred_probs[i]);
            STREAMTRACE_I("segment_pred_prob", tmp);
          }
        }
      }
      for (i = 0; i < MB_SEG_TREE_PROBS; i++) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        if (tmp) {
          fc->a.mb_segment_tree_probs[i] = Av1ReadProbDiffUpdate(&bc, fc->a.mb_segment_tree_probs[i]);
          STREAMTRACE_I("segment_pred_prob", tmp);
        }
      }
    }

    for (j = 0; j < kNumIntraModes; j++) {
      for (i = 0; i < kNumIntraModes - 1; i++) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        STREAMTRACE_I("uv_mode_prob_update", tmp);
        if (tmp) {
          u8 *prob = &fc->a.uv_mode_prob[j][i];
          *prob = Av1ReadProbDiffUpdate(&bc, *prob);
          STREAMTRACE_I("uv_mode_prob", *prob);
        }
      }
    }

    // keyframe uses same uv_mode probs
    memcpy(fc->kf_uv_mode_prob, fc->a.uv_mode_prob, sizeof(fc->kf_uv_mode_prob));

    for (j = 0; j < NUM_PARTITION_CONTEXTS; j++) {
      for (i = 0; i < PARTITION_TYPES - 1; i++) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        STREAMTRACE_I("partition_prob_update", tmp);
        if (tmp) {
          u8 *prob = &fc->a.partition_prob[INTER_FRAME][j][i];
          *prob = Av1ReadProbDiffUpdate(&bc, *prob);
          STREAMTRACE_I("partition_prob", *prob);
        }
      }
    }
    // keyframe uses same partition probs
    memcpy(fc->a.partition_prob[KEY_FRAME], fc->a.partition_prob[INTER_FRAME], sizeof(fc->a.partition_prob[KEY_FRAME]));

    if (dec->key_frame || dec->intra_only) {
      for (i = 0; i < kNumIntraModes; i++) {
        for (j = 0; j < kNumIntraModes; j++) {
          for (k = 0; k < kNumIntraModes - 1; k++) {
            u8 *prob = &fc->kf_bmode_prob[i][j][k];
            *prob = av1_kf_default_bmode_probs[i][j][k];
            tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
            STREAMTRACE_I("kf_y_prob_update", tmp);
            if (tmp) {
              *prob = Av1ReadProbDiffUpdate(&bc, *prob);
              STREAMTRACE_I("kf_y_prob", *prob);
            }
          }
        }
      }
    }
  }  // av1_mode

  if (!dec->key_frame && !dec->intra_only) {
    for (i = 0; i < INTER_MODE_CONTEXTS; i++) {
      for (j = 0; j < AV1_INTER_MODES - 1; j++) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        STREAMTRACE_I("inter_mode_prob_update", tmp);
        if (tmp) {
          u8 *prob = &fc->a.inter_mode_prob[i][j];
          *prob = Av1ReadProbDiffUpdate(&bc, *prob);
          STREAMTRACE_I("inter_mode_prob", *prob);
        }
      }
    }

    if (dec->mcomp_filter_type == SWITCHABLE) {
      for (j = 0; j < AV1_SWITCHABLE_FILTERS + 1; ++j) {
        for (i = 0; i < AV1_SWITCHABLE_FILTERS - 1; ++i) {
          tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
          STREAMTRACE_I("switchable_filter_prob_update", tmp);
          if (tmp) {
            u8 *prob = &fc->a.switchable_interp_prob[j][i];
            *prob = Av1ReadProbDiffUpdate(&bc, *prob);
            STREAMTRACE_I("switchable_interp_prob", *prob);
          }
        }
      }
    }

    for (i = 0; i < INTRA_INTER_CONTEXTS; i++) {
      tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
      STREAMTRACE_I("intra_inter_prob_update", tmp);
      if (tmp) {
        u8 *prob = &fc->a.intra_inter_prob[i];
        *prob = Av1ReadProbDiffUpdate(&bc, *prob);
        STREAMTRACE_I("intra_inter_prob", *prob);
      }
    }

    if (dec->comp_pred_mode == HYBRID_PREDICTION) {
      for (i = 0; i < COMP_INTER_CONTEXTS; i++) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        STREAMTRACE_I("comp_inter_prob_update", tmp);
        if (tmp) {
          u8 *prob = &fc->a.comp_inter_prob[i];
          *prob = Av1ReadProbDiffUpdate(&bc, *prob);
          STREAMTRACE_I("comp_inter_prob", *prob);
        }
      }
    }
    if (dec->comp_pred_mode != COMP_PREDICTION_ONLY) {
      for (i = 0; i < REF_CONTEXTS; i++) {
        for (j = 0; j < 2; ++j) {
          tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
          STREAMTRACE_I("single_ref_prob_update", tmp);
          if (tmp) {
            u8 *prob = &fc->a.single_ref_prob[i][j];
            *prob = Av1ReadProbDiffUpdate(&bc, *prob);
            STREAMTRACE_I("single_ref_prob", *prob);
          }
        }
      }
    }

    if (dec->comp_pred_mode != SINGLE_PREDICTION_ONLY) {
      for (i = 0; i < REF_CONTEXTS; i++) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        STREAMTRACE_I("comp_ref_prob_update", tmp);
        if (tmp) {
          u8 *prob = &fc->a.comp_ref_prob[i];
          *prob = Av1ReadProbDiffUpdate(&bc, *prob);
          STREAMTRACE_I("comp_ref_prob", *prob);
        }
      }
    }

    /* Superblock intra luma pred mode probabilities */
    for (j = 0; j < BLOCK_SIZE_GROUPS; ++j) {
      for (i = 0; i < kNumIntraModes - 1; ++i) {
        tmp = Av1DecodeBool(&bc, AV1_DEF_UPDATE_PROB);
        STREAMTRACE_I("ymode_prob_update", tmp);
        if (tmp) {
          fc->a.sb_ymode_prob[j][i] =
              Av1ReadProbDiffUpdate(&bc, fc->a.sb_ymode_prob[j][i]);
          STREAMTRACE_I("ymode_prob", fc->a.sb_ymode_prob[j][i]);
        }
      }
    }

    /* Motion vector tree update */
    tmp = Av1DecodeMvUpdate(&bc, dec);

    if (tmp != HANTRO_OK) return (tmp);
  }


  STREAMTRACE_I("decoded_header_bytes: %d\n", bc.pos);

  /* When first tile row has zero-height skip it. Cannot be done when pic height
   * smaller than 3 SB rows, in this case more than one tile rows may be skipped
   * and needs to be handled in stream parsing.
   * For AV1 (nextgenv2), zero-height tiles are not skipped */

  if (bc.strm_error) return (HANTRO_NOK);

  return (HANTRO_OK);
}
#endif

i32 DecodeQuantizerDelta(struct StrmData *rb) {
  i32 delta;

  if (SwGetBits(rb, 1)) {
    STREAMTRACE_I("qp_delta_present: %d\n", 1);
    delta = SignExt(SwGetBits(rb, 7), 6);
    return delta;
  } else {
    STREAMTRACE_I("qp_delta_present: %d\n", 0);
    return 0;
  }
}

u32 Av1SetPartitionOffsets(const u8 *stream, u32 len, struct Av1Decoder *dec) {
  u32 offset = 0;
  u32 base_offset;
  u32 ret_val = HANTRO_OK;

  UNUSED(stream);
//  stream += dec->frame_tag_size;  //Seems no use
//  stream += dec->offset_to_dct_parts;

  base_offset = dec->frame_tag_size + dec->offset_to_dct_parts;

  dec->dct_partition_offsets[0] = base_offset + offset;
  if (dec->dct_partition_offsets[0] > len) {
    dec->dct_partition_offsets[0] = len - 1;
    ret_val = HANTRO_NOK;
  }

  return ret_val;
}

#if 0
u32 ReadTileSize(const u8 *cx_size) {
  u32 size;
  size = (u32)(*(cx_size + 0)) + ((u32)(*(cx_size + 1)) << 8) +
         ((u32)(*(cx_size + 2)) << 16) + ((u32)(*(cx_size + 3)) << 24);
  return size;
}
#endif

void DecodeSegmentationDataAV1(struct StrmData *rb,
                               struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct DecAsicBuffers *asic_buff = dec_cont->asic_buff;
  u32 sign, i, j;
  /* Segmentation enabled? */
  dec->segment_enabled = SwGetBits(rb, 1);
  STREAMTRACE_I("segment_enabled: %d\n", dec->segment_enabled);

  dec->preskip_segid = 0;
  dec->last_active_seg = 0;

  dec->segment_map_update = 0;
  dec->segment_map_temporal_update = 0;

  DWLmemset(dec->segment_feature_enable, 0,
            sizeof(dec->segment_feature_enable));
  DWLmemset(dec->segment_feature_data, 0, sizeof(dec->segment_feature_data));

  if (!dec->segment_enabled) return;

  u32 update_data;
  if (dec->primary_ref_frame == ALLOWED_REFS_PER_FRAME_EX) {
    dec->segment_map_update = 1;
    dec->segment_map_temporal_update = 0;
    update_data = 1;
  } else {
    dec->segment_map_update = SwGetBits(rb, 1);
    if (dec->segment_map_update)
      dec->segment_map_temporal_update = SwGetBits(rb, 1);
    update_data = SwGetBits(rb, 1);

    // Copying segment data from the primary ref frame
    int prim_buf_idx = Av1BufferQueueGetRef(
        dec_cont->bq, dec->active_ref_idx[dec->primary_ref_frame]);
    if (!update_data && prim_buf_idx != kReferenceNotSet) {
      DWLmemcpy(dec->segment_feature_enable,
                asic_buff->segment_info[prim_buf_idx].segment_feature_enable,
                sizeof(dec->segment_feature_enable));
      DWLmemcpy(dec->segment_feature_data,
                asic_buff->segment_info[prim_buf_idx].segment_feature_data,
                sizeof(dec->segment_feature_data));
      for (i = 0; i < MAX_MB_SEGMENTS; i++) {
        for (j = 0; j < SEG_AV1_LVL_MAX; j++) {
          if (dec->segment_feature_enable[i][j]) {
            dec->preskip_segid |= j >= SEG_AV1_LVL_REF_FRAME;
            dec->last_active_seg = MAX(i, dec->last_active_seg);
          }
        }
      }
    }
  }

  // STREAMTRACE_I("segment_data_update", tmp);
  if (update_data) {
    /* Absolute/relative mode */
    dec->segment_feature_mode = 0;
    STREAMTRACE_I("segment_abs_delta: %d\n", dec->segment_feature_mode);

    /* Clear all previous segment data */
    for (i = 0; i < MAX_MB_SEGMENTS; i++) {
      for (j = 0; j < SEG_AV1_LVL_MAX; j++) {
        dec->segment_feature_enable[i][j] = SwGetBits(rb, 1);
        STREAMTRACE_I("segment_feature_enable: %d\n",
                     dec->segment_feature_enable[i][j]);

        if (dec->segment_feature_enable[i][j]) {
          dec->preskip_segid |= j >= SEG_AV1_LVL_REF_FRAME;
          dec->last_active_seg = MAX(i, dec->last_active_seg);
          /* Sign if needed */
          int data_signed = av1_seg_feature_data_signed[j];
          int data_max = av1_seg_feature_data_max[j];
          int data_bits = av1_seg_feature_data_bits[j];
          int data = 0;
          if (data_signed) {
            sign = SwGetBits(rb, 1);
            data = SwGetBits(rb, data_bits);
            if (sign) data -= (1 << data_bits);
          } else {
            data = SwGetBits(rb, data_bits);
          }
          data = CLIP3(-data_max, data_max, data);
          /* Feature data, bits changes for every feature */
          dec->segment_feature_data[i][j] = data;
          STREAMTRACE_I("segment_feature_data: %d\n", dec->segment_feature_data[i][j]);
        }
      }
    }
  }
}

void StoreLfParams(struct Av1DecContainer *dec_cont) {
  struct Av1Decoder *dec = &dec_cont->decoder;

  for (int pic_idx = 0; pic_idx < NUM_REF_FRAMES; pic_idx++) {
    if (dec->refresh_frame_flags & (1 << pic_idx)) {
      for (int i = 0; i < MAX_REF_LF_DELTAS_EX; i++) {
        dec->prev_ref_lf_delta[pic_idx][i] = dec->mb_ref_lf_delta[i];
      }
      for (int i = 0; i < MAX_MODE_LF_DELTAS; i++) {
        dec->prev_mode_lf_delta[pic_idx][i] = dec->mb_mode_lf_delta[i];
      }
    }
  }
}

void LoadLfParams(struct Av1DecContainer *dec_cont, i32 pri_idx) {
  struct Av1Decoder *dec = &dec_cont->decoder;

  for (int i = 0; i < MAX_REF_LF_DELTAS_EX; i++) {
    dec->mb_ref_lf_delta[i] = dec->prev_ref_lf_delta[pri_idx][i];
  }
  for (int i = 0; i < MAX_MODE_LF_DELTAS; i++) {
    dec->mb_mode_lf_delta[i] = dec->prev_mode_lf_delta[pri_idx][i];
  }
}

void DecodeLfParams(struct StrmData *rb, struct Av1DecContainer *dec_cont,
                    int coded_lossless) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  (void)coded_lossless;

  // INTRABC
  {
    bool use_default = dec->key_frame || dec->error_resilient ||
                       dec->intra_only || dec->allow_intrabc ||
                       coded_lossless ||
                       dec->primary_ref_frame == ALLOWED_REFS_PER_FRAME_EX;
    if (use_default) {
      DWLmemset(dec->mb_ref_lf_delta, 0, sizeof(dec->mb_ref_lf_delta));
      DWLmemset(dec->mb_mode_lf_delta, 0, sizeof(dec->mb_mode_lf_delta));
      dec->mb_ref_lf_delta[0] = 1;
      dec->mb_ref_lf_delta[1] = 0;
      dec->mb_ref_lf_delta[2] = 0;
      dec->mb_ref_lf_delta[3] = 0;
      dec->mb_ref_lf_delta[4] = -1;
      dec->mb_ref_lf_delta[5] = 0;
      dec->mb_ref_lf_delta[6] = -1;
      dec->mb_ref_lf_delta[7] = -1;
      if (dec->allow_intrabc || coded_lossless) {
        StoreLfParams(dec_cont);
        dec->loop_filter_level = 0;
        dec->loop_filter_level_r = 0;
        dec->loop_filter_level_u = 0;
        dec->loop_filter_level_v = 0;
        return;
      }
    } else {
      LoadLfParams(dec_cont, dec->active_ref_idx[dec->primary_ref_frame]);
    }
  }

  /* Loop filter adjustments */
  dec->loop_filter_level = SwGetBits(rb, 6);
  dec->loop_filter_level_r = SwGetBits(rb, 6);
  if (!dec->monochrome &&
      (dec->loop_filter_level || dec->loop_filter_level_r)) {
    dec->loop_filter_level_u = SwGetBits(rb, 6);
    dec->loop_filter_level_v = SwGetBits(rb, 6);
  }
  dec->loop_filter_sharpness = SwGetBits(rb, 3);
  STREAMTRACE_I("loop_filter_level: %d\n", dec->loop_filter_level);
  STREAMTRACE_I("loop_filter_sharpness: %d\n", dec->loop_filter_sharpness);

  /* Adjustments enabled? */
  dec->mode_ref_lf_enabled = SwGetBits(rb, 1);
  STREAMTRACE_I("loop_filter_adj_enable: %d\n", dec->mode_ref_lf_enabled);

  if (dec->mode_ref_lf_enabled) {
    u32 tmp, j;

    /* Mode update? */
    u32 mode_ref_delta_update = SwGetBits(rb, 1);
    STREAMTRACE_I("loop_filter_adj_update: %d\n", mode_ref_delta_update);
    if (mode_ref_delta_update) {
      /* Reference frame deltas */
      u32 max_ref_lf_deltas = MAX_REF_LF_DELTAS_EX;
      for (j = 0; j < max_ref_lf_deltas; j++) {
        tmp = SwGetBits(rb, 1);
        STREAMTRACE_I("ref_frame_delta_update: %d\n", tmp);
        if (tmp) {
          dec->mb_ref_lf_delta[j] = SignExt(SwGetBits(rb, 7), 6);
          STREAMTRACE_I("mb_ref_lf_delta: %d\n", dec->mb_ref_lf_delta[j]);
        }
      }

      /* Mode deltas */
      for (j = 0; j < MAX_MODE_LF_DELTAS; j++) {
        tmp = SwGetBits(rb, 1);
        STREAMTRACE_I("mb_type_delta_update: %d\n", tmp);
        if (tmp) {
          dec->mb_mode_lf_delta[j] = SignExt(SwGetBits(rb, 7), 6);
          STREAMTRACE_I("mb_mode_lf_delta: %d\n", dec->mb_mode_lf_delta[j]);
        }
      }
    }
  } /* Mb mode/ref lf adjustment */

  StoreLfParams(dec_cont);
}
