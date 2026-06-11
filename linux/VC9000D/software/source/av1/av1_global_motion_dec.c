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

#include "av1_global_motion_dec.h"

#include "av1hwd_bool.h"
#include "av1hwd_container.h"
#include "av1hwd_decoder.h"

#define GLOBAL_TRANS_TYPES TRANS_TYPES

// Bits of precision used for the model
#define WARPEDMODEL_PREC_BITS 16
#define WARPEDMODEL_ROW3HOMO_PREC_BITS 16

// Bits of subpel precision for warped interpolation
#define WARPEDPIXEL_PREC_BITS 6
#define WARPEDPIXEL_PREC_SHIFTS (1 << WARPEDPIXEL_PREC_BITS)

#define SUBEXPFIN_K 3
#define GM_TRANS_PREC_BITS 6
#define GM_ABS_TRANS_BITS 12
#define GM_ABS_TRANS_ONLY_BITS (GM_ABS_TRANS_BITS - GM_TRANS_PREC_BITS + 3)
#define GM_TRANS_PREC_DIFF (WARPEDMODEL_PREC_BITS - GM_TRANS_PREC_BITS)
#define GM_TRANS_ONLY_PREC_DIFF (WARPEDMODEL_PREC_BITS - 3)
#define GM_TRANS_DECODE_FACTOR (1 << GM_TRANS_PREC_DIFF)
#define GM_TRANS_ONLY_DECODE_FACTOR (1 << GM_TRANS_ONLY_PREC_DIFF)

#define GM_ALPHA_PREC_BITS 15
#define GM_ABS_ALPHA_BITS 12
#define GM_ALPHA_PREC_DIFF (WARPEDMODEL_PREC_BITS - GM_ALPHA_PREC_BITS)
#define GM_ALPHA_DECODE_FACTOR (1 << GM_ALPHA_PREC_DIFF)

#define GM_ROW3HOMO_PREC_BITS 16
#define GM_ABS_ROW3HOMO_BITS 11
#define GM_ROW3HOMO_PREC_DIFF \
  (WARPEDMODEL_ROW3HOMO_PREC_BITS - GM_ROW3HOMO_PREC_BITS)
#define GM_ROW3HOMO_DECODE_FACTOR (1 << GM_ROW3HOMO_PREC_DIFF)

#define GM_ROW3HOMO_MAX (1 << GM_ABS_ROW3HOMO_BITS)

#define GM_TRANS_MAX (1 << GM_ABS_TRANS_BITS)
#define GM_ALPHA_MAX (1 << GM_ABS_ALPHA_BITS)
#define GM_ROW3HOMO_MAX (1 << GM_ABS_ROW3HOMO_BITS)

#define GM_TRANS_MIN -GM_TRANS_MAX
#define GM_ALPHA_MIN -GM_ALPHA_MAX
#define GM_ROW3HOMO_MIN -GM_ROW3HOMO_MAX

#define WARP_PARAM_REDUCE_BITS 6
#define WARPEDMODEL_PREC_BITS 16

#define ACCT_STR_PARAM , int unused
#define ACCT_STR_NAME unused

#ifndef INT16_MAX
#define INT16_MAX 0x7fff
#endif

#ifndef INT16_MIN
#define INT16_MIN (-INT16_MAX - 1)
#endif

// namespace {

static int is_affine_valid(const struct WarpedMotionParams *const wm) {
  const i32 *mat = wm->wmmat;
  return (mat[2] > 0);
}

static int clamp(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

#define DIV_LUT_PREC_BITS 14
#define DIV_LUT_BITS 8
#define DIV_LUT_NUM (1 << DIV_LUT_BITS)

static const u16 div_lut[DIV_LUT_NUM + 1] = {
    16384, 16320, 16257, 16194, 16132, 16070, 16009, 15948, 15888, 15828, 15768,
    15709, 15650, 15592, 15534, 15477, 15420, 15364, 15308, 15252, 15197, 15142,
    15087, 15033, 14980, 14926, 14873, 14821, 14769, 14717, 14665, 14614, 14564,
    14513, 14463, 14413, 14364, 14315, 14266, 14218, 14170, 14122, 14075, 14028,
    13981, 13935, 13888, 13843, 13797, 13752, 13707, 13662, 13618, 13574, 13530,
    13487, 13443, 13400, 13358, 13315, 13273, 13231, 13190, 13148, 13107, 13066,
    13026, 12985, 12945, 12906, 12866, 12827, 12788, 12749, 12710, 12672, 12633,
    12596, 12558, 12520, 12483, 12446, 12409, 12373, 12336, 12300, 12264, 12228,
    12193, 12157, 12122, 12087, 12053, 12018, 11984, 11950, 11916, 11882, 11848,
    11815, 11782, 11749, 11716, 11683, 11651, 11619, 11586, 11555, 11523, 11491,
    11460, 11429, 11398, 11367, 11336, 11305, 11275, 11245, 11215, 11185, 11155,
    11125, 11096, 11067, 11038, 11009, 10980, 10951, 10923, 10894, 10866, 10838,
    10810, 10782, 10755, 10727, 10700, 10673, 10645, 10618, 10592, 10565, 10538,
    10512, 10486, 10460, 10434, 10408, 10382, 10356, 10331, 10305, 10280, 10255,
    10230, 10205, 10180, 10156, 10131, 10107, 10082, 10058, 10034, 10010, 9986,
    9963,  9939,  9916,  9892,  9869,  9846,  9823,  9800,  9777,  9754,  9732,
    9709,  9687,  9664,  9642,  9620,  9598,  9576,  9554,  9533,  9511,  9489,
    9468,  9447,  9425,  9404,  9383,  9362,  9341,  9321,  9300,  9279,  9259,
    9239,  9218,  9198,  9178,  9158,  9138,  9118,  9098,  9079,  9059,  9039,
    9020,  9001,  8981,  8962,  8943,  8924,  8905,  8886,  8867,  8849,  8830,
    8812,  8793,  8775,  8756,  8738,  8720,  8702,  8684,  8666,  8648,  8630,
    8613,  8595,  8577,  8560,  8542,  8525,  8508,  8490,  8473,  8456,  8439,
    8422,  8405,  8389,  8372,  8355,  8339,  8322,  8306,  8289,  8273,  8257,
    8240,  8224,  8208,  8192,
};

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

static int av1_rb_read_bit(struct StrmData *r) {
  int ret = SwGetBits(r, 1);
  return ret;
}

static int av1_rb_read_literal(struct StrmData *r, int bits) {
  int ret = SwGetBits(r, bits);
  return ret;
}

// Inverse recenters a non-negative literal v around a reference r
static u16 inv_recenter_nonneg(u16 r, u16 v) {
  if (v > (r << 1))
    return v;
  else if ((v & 1) == 0)
    return (v >> 1) + r;
  else
    return r - ((v + 1) >> 1);
}

// Inverse recenters a non-negative literal v in [0, n-1] around a
// reference r also in [0, n-1]
static u16 inv_recenter_finite_nonneg(u16 n, u16 r, u16 v) {
  if ((r << 1) <= n) {
    return inv_recenter_nonneg(r, v);
  } else {
    return n - 1 - inv_recenter_nonneg(n - 1 - r, v);
  }
}

static u16 av1_rb_read_primitive_quniform(struct StrmData *rb, u16 n) {
  if (n <= 1) return 0;
  const int l = get_msb(n - 1) + 1;
  const int m = (1 << l) - n;
  const int v = av1_rb_read_literal(rb, l - 1);
  return v < m ? v : (v << 1) - m + av1_rb_read_bit(rb);
}

static u16 av1_rb_read_primitive_subexpfin(struct StrmData *rb, u16 n, u16 k) {
  int i = 0;
  int mk = 0;

  while (1) {
    int b = (i ? k + i - 1 : k);
    int a = (1 << b);

    if (n <= mk + 3 * a) {
      return av1_rb_read_primitive_quniform(rb, n - mk) + mk;
    }

    if (!av1_rb_read_bit(rb)) {
      return av1_rb_read_literal(rb, b) + mk;
    }

    i = i + 1;
    mk += a;
  }

  ASSERT(0);
  return 0;
}

static u16 av1_rb_read_primitive_refsubexpfin(struct StrmData *rb, u16 n, u16 k,
                                              u16 ref) {
  return inv_recenter_finite_nonneg(n, ref,
                                    av1_rb_read_primitive_subexpfin(rb, n, k));
}

int16 av1_rb_read_signed_primitive_refsubexpfin(struct StrmData *rb, u16 n,
                                                  u16 k, int16 ref) {
  ref += n - 1;
  const u16 scaled_n = (n << 1) - 1;
  return av1_rb_read_primitive_refsubexpfin(rb, scaled_n, k, ref) - n + 1;
}

static i16 resolve_divisor_32(u32 D, i16 *shift) {
  i32 e, f;
  *shift = get_msb(D);
  // e is obtained from D after resetting the most significant 1 bit.
  e = D - ((u32)1 << *shift);
  // Get the most significant DIV_LUT_BITS (8) bits of e into f
  if (*shift > DIV_LUT_BITS)
    f = ROUND_POWER_OF_TWO(e, *shift - DIV_LUT_BITS);
  else
    f = e << (DIV_LUT_BITS - *shift);
  ASSERT(f <= DIV_LUT_NUM);
  *shift += DIV_LUT_PREC_BITS;
  // Use f as lookup into the precomputed table of multipliers
  return div_lut[f];
}

static int is_affine_shear_allowed(i16 alpha, i16 beta, i16 gamma, i16 delta) {
  if ((4 * ABS(alpha) + 7 * ABS(beta) >= (1 << WARPEDMODEL_PREC_BITS)) ||
      (4 * ABS(gamma) + 4 * ABS(delta) >= (1 << WARPEDMODEL_PREC_BITS)))
    return 0;
  else
    return 1;
}

// Returns 1 on success or 0 on an invalid affine set
int get_shear_params_dec(struct WarpedMotionParams *wm) {
  const i32 *mat = wm->wmmat;
  if (!is_affine_valid(wm)) return 0;
  wm->alpha =
      clamp(mat[2] - (1 << WARPEDMODEL_PREC_BITS), INT16_MIN, INT16_MAX);
  wm->beta = clamp(mat[3], INT16_MIN, INT16_MAX);
  i16 shift;
  i16 y = resolve_divisor_32(ABS(mat[2]), &shift) * (mat[2] < 0 ? -1 : 1);
  i64 v;
  v = ((i64)mat[4] * (1 << WARPEDMODEL_PREC_BITS)) * y;
  wm->gamma =
      clamp((int)ROUND_POWER_OF_TWO_SIGNED_64(v, shift), INT16_MIN, INT16_MAX);
  v = ((i64)mat[3] * mat[4]) * y;
  wm->delta = clamp(mat[5] - (int)ROUND_POWER_OF_TWO_SIGNED_64(v, shift) -
                        (1 << WARPEDMODEL_PREC_BITS),
                    INT16_MIN, INT16_MAX);
  wm->alpha = ROUND_POWER_OF_TWO_SIGNED(wm->alpha, WARP_PARAM_REDUCE_BITS) *
              (1 << WARP_PARAM_REDUCE_BITS);
  wm->beta = ROUND_POWER_OF_TWO_SIGNED(wm->beta, WARP_PARAM_REDUCE_BITS) *
             (1 << WARP_PARAM_REDUCE_BITS);
  wm->gamma = ROUND_POWER_OF_TWO_SIGNED(wm->gamma, WARP_PARAM_REDUCE_BITS) *
              (1 << WARP_PARAM_REDUCE_BITS);
  wm->delta = ROUND_POWER_OF_TWO_SIGNED(wm->delta, WARP_PARAM_REDUCE_BITS) *
              (1 << WARP_PARAM_REDUCE_BITS);

  if (!is_affine_shear_allowed(wm->alpha, wm->beta, wm->gamma, wm->delta)) {
    return 0;
  }
  return 1;
}

static int read_global_motion_params(
    struct WarpedMotionParams *params,
    const struct WarpedMotionParams *ref_params, struct StrmData *rb,
    int allow_hp) {
  TransformationType type = (TransformationType)av1_rb_read_bit(rb);
  if (type != IDENTITY) {
    if (av1_rb_read_bit(rb))
      type = ROTZOOM;
    else
      type = av1_rb_read_bit(rb) ? TRANSLATION : AFFINE;
  }

  *params = sDefaultParams;
  params->wmtype = type;

  if (type >= ROTZOOM) {
    params->wmmat[2] = av1_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[2] >> GM_ALPHA_PREC_DIFF) -
                               (1 << GM_ALPHA_PREC_BITS)) *
                           GM_ALPHA_DECODE_FACTOR +
                       (1 << WARPEDMODEL_PREC_BITS);
    params->wmmat[3] = av1_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[3] >> GM_ALPHA_PREC_DIFF)) *
                       GM_ALPHA_DECODE_FACTOR;
  }

  if (type >= AFFINE) {
    params->wmmat[4] = av1_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[4] >> GM_ALPHA_PREC_DIFF)) *
                       GM_ALPHA_DECODE_FACTOR;
    params->wmmat[5] = av1_rb_read_signed_primitive_refsubexpfin(
                           rb, GM_ALPHA_MAX + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[5] >> GM_ALPHA_PREC_DIFF) -
                               (1 << GM_ALPHA_PREC_BITS)) *
                           GM_ALPHA_DECODE_FACTOR +
                       (1 << WARPEDMODEL_PREC_BITS);
  } else {
    params->wmmat[4] = -params->wmmat[3];
    params->wmmat[5] = params->wmmat[2];
  }

  if (type >= TRANSLATION) {
    const int trans_bits = (type == TRANSLATION)
                               ? GM_ABS_TRANS_ONLY_BITS - !allow_hp
                               : GM_ABS_TRANS_BITS;
    const int trans_dec_factor =
        (type == TRANSLATION) ? GM_TRANS_ONLY_DECODE_FACTOR * (1 << !allow_hp)
                              : GM_TRANS_DECODE_FACTOR;
    const int trans_prec_diff = (type == TRANSLATION)
                                    ? GM_TRANS_ONLY_PREC_DIFF + !allow_hp
                                    : GM_TRANS_PREC_DIFF;
    params->wmmat[0] = av1_rb_read_signed_primitive_refsubexpfin(
                           rb, (1 << trans_bits) + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[0] >> trans_prec_diff)) *
                       trans_dec_factor;
    params->wmmat[1] = av1_rb_read_signed_primitive_refsubexpfin(
                           rb, (1 << trans_bits) + 1, SUBEXPFIN_K,
                           (ref_params->wmmat[1] >> trans_prec_diff)) *
                       trans_dec_factor;
  }

  if (params->wmtype <= AFFINE) {
    int good_shear_params = get_shear_params_dec(params);
    if (!good_shear_params) return 0;
  }

  return 1;
}

//}  // namespace

const struct WarpedMotionParams sDefaultParams = {
    IDENTITY,
    {0, 0, (1 << WARPEDMODEL_PREC_BITS), 0, 0, (1 << WARPEDMODEL_PREC_BITS)},
    0,
    0,
    0,
    0};

u32 DecodeGlobalMotionParams(struct Av1DecContainer *dec_cont,
                             struct StrmData *rb) {
  struct Av1Decoder *dec = &dec_cont->decoder;
  struct WarpedMotionParams prev_models[GM_GLOBAL_MODELS_PER_FRAME];

  for (int i = 0; i < GM_GLOBAL_MODELS_PER_FRAME; ++i)
    prev_models[i] = sDefaultParams;

  if (dec->primary_ref_frame != ALLOWED_REFS_PER_FRAME_EX) {
    int prim_buf_idx = Av1BufferQueueGetRef(
        dec_cont->bq, dec->active_ref_idx[dec->primary_ref_frame]);
    if (prim_buf_idx != kReferenceNotSet) {
      memcpy(prev_models, dec_cont->asic_buff->global_models[prim_buf_idx],
             sizeof(struct WarpedMotionParams) * GM_GLOBAL_MODELS_PER_FRAME);
    }
    else
      return HANTRO_NOK;
  }

  for (int frame = 0; frame < GM_GLOBAL_MODELS_PER_FRAME; ++frame) {
    const struct WarpedMotionParams *ref_params = &prev_models[frame];
    struct WarpedMotionParams *params = &dec->models[frame];
    int cur_width = dec->width;
    int cur_height = dec->height;
    int idx_ref;
    idx_ref = Av1BufferQueueGetRef(dec_cont->bq, dec->active_ref_idx[frame]);
    if (dec_cont->pp_enabled)
      idx_ref = Av1BufferQueueGetRef(dec_cont->pp_bq, dec->active_ref_idx[frame]);
    if (idx_ref == kReferenceNotSet)
      return HANTRO_NOK;
    int ref_width = dec_cont->asic_buff->picture_info[idx_ref].superres_width;
    int ref_height = dec_cont->asic_buff->picture_info[idx_ref].coded_height;

    int good_params = read_global_motion_params(params, ref_params, rb,
                                                dec->allow_high_precision_mv);
    bool ref_uses_scaling = cur_width != ref_width || cur_height != ref_height;

    if (!good_params || ref_uses_scaling) {
      // Force params to invalid values that are easy for HW to detect
      params->alpha = -32768;
      params->beta = -32768;
      params->gamma = -32768;
      params->delta = -32768;
    }
  }

  return HANTRO_OK;
}
