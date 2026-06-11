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

#include "av1hwd_probs.h"
#include <stdio.h>
#include "av1_commondec.h"
#include "av1_default_coef_probs.h"
#include "av1_entropymode.h"
#include "av1_entropymv.h"
#include "av1_treecoder.h"
#include "basetype.h"
#include "dwl.h"
#include "sw_debug.h"
#include "dec_log.h"

/* Define if it's necessary to trace probability updates */
//#define TRACE_HDR_PROBS
//#define TRACE_PROB_TABLES
//#define MODE_COUNT_TESTING

/* Array indices are identical to previously-existing CONTEXT_NODE indices */

#ifdef TRACE_PROB_TABLES
#define _COMMON_DEFS_H
#include "counter_mem_layout.h"
#include "emd_mem_layout.h"
#endif

const av1_tree_index av1hwd_coef_tree[22] = /* corresponding _CONTEXT_NODEs */
    {
        -DCT_EOB_TOKEN,
        2, /* 0 = EOB */
        -ZERO_TOKEN,
        4, /* 1 = ZERO */
        -ONE_TOKEN,
        6, /* 2 = ONE */
        8,
        12, /* 3 = LOW_VAL */
        -TWO_TOKEN,
        10, /* 4 = TWO */
        -THREE_TOKEN,
        -FOUR_TOKEN, /* 5 = THREE */
        14,
        16, /* 6 = HIGH_LOW */
        -DCT_VAL_CATEGORY1,
        -DCT_VAL_CATEGORY2, /* 7 = CAT_ONE */
        18,
        20, /* 8 = CAT_THREEFOUR */
        -DCT_VAL_CATEGORY3,
        -DCT_VAL_CATEGORY4, /* 9 = CAT_THREE */
        -DCT_VAL_CATEGORY5,
        -DCT_VAL_CATEGORY6 /* 10 = CAT_FIVE */
};

const av1_tree_index av1_coefmodel_tree[6] = {
    -DCT_EOB_MODEL_TOKEN, 2,          /* 0 = EOB */
    -ZERO_TOKEN,          4,          /* 1 = ZERO */
    -ONE_TOKEN,           -TWO_TOKEN, /* 2 = ONE */
};

void Av1ResetProbs(struct Av1Decoder *dec) {
  i32 i, j, k, l, m;

  Av1InitModeContexts(dec);
  Av1InitMbmodeProbs(dec);
  Av1InitMvProbs(dec);

  /* Copy the default probs into two separate prob tables: part1 and part2. */
  for (i = 0; i < BLOCK_TYPES; i++) {
    for (j = 0; j < REF_TYPES; j++) {
      for (k = 0; k < COEF_BANDS; k++) {
        for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
          if (l >= 3 && k == 0) continue;

          for (m = 0; m < UNCONSTRAINED_NODES; m++) {
            dec->entropy.a.prob_coeffs[i][j][k][l][m] =
                default_coef_probs_4x4[i][j][k][l][m];
            dec->entropy.a.prob_coeffs8x8[i][j][k][l][m] =
                default_coef_probs_8x8[i][j][k][l][m];
            dec->entropy.a.prob_coeffs16x16[i][j][k][l][m] =
                default_coef_probs_16x16[i][j][k][l][m];
            dec->entropy.a.prob_coeffs32x32[i][j][k][l][m] =
                default_coef_probs_32x32[i][j][k][l][m];
          }
        }
      }
    }
  }

#ifdef TRACE_PROB_TABLES
  {
    const int num_intra_modes = AV1_INTRA_MODES;
    static const char *const kTraceProbsFmtStr =
        "%-40s %5td, %5td, %5zuB, %5u, %5ub%s\n";
    static const char *const kTraceCntsFmtStr =
        "%-40s %5td, %5td, %5zu, %5u%s\n";
    static const char *const cmp_match = "";
    static const char *const cmp_mismatch = " *";
    struct Av1EntropyProbs p;
    struct Av1EntropyCounts c;
    printf("Probability structs:\n");
    printf("Num Intra Modes = %d\n", num_intra_modes);
    printf("sizeof(struct Av1EntropyProbs) = %ldB\n",
           sizeof(struct Av1EntropyProbs));
    printf("sizeof(struct Av1EntropyCounts) = %ldB\n",
           sizeof(struct Av1EntropyCounts));
    printf("EMD Sram Amount (AV1) = %d addrs = %dB\n", kAv1EmdSramAddrAmount,
           kAv1EmdSramAddrAmount * 32);
    printf("EMD Sram Amount (AV1) = %d addrs = %dB\n", kEmdSramAddrAmount,
           kEmdSramAddrAmount * 32);
    printf("Counter Sram Amount (AV1) = %d address = %dB\n", (countMax + 7) / 8,
           countMax * 4);
    printf("Counter Sram Amount (AV1) = %d address = %dB\n",
           (countMaxAv1 + 7) / 8, countMaxAv1 * 4);

#define __TRACE_PROB_FIELD(struct_field, hw_addr, hw_offs)                  \
  {                                                                         \
    ptrdiff_t o = (u8 *)&(struct_field) - (u8 *)&p;                         \
    size_t s = sizeof(struct_field);                                        \
    u32 a = hw_addr;                                                        \
    u32 b = hw_offs;                                                        \
    const char *cmp = (o == a * 32 + b / 8) ? cmp_match : cmp_mismatch;     \
    printf(kTraceProbsFmtStr, #struct_field, o / 32, o % 32, s, a, b, cmp); \
  }
    printf(
        "Prob tables aligned to addresses: base, firstbyte, size, HW addr, HW "
        "offset\n");
    __TRACE_PROB_FIELD(p.kf_bmode_prob, kKfBmodeProb, kKfBmodeProbOffs);
    __TRACE_PROB_FIELD(p.ref_pred_probs, kRefPredProbs, kRefPredProbsOffs);
    __TRACE_PROB_FIELD(p.ref_scores, kRefScores, kRefScoresOffs);
    __TRACE_PROB_FIELD(p.prob_comppred, kProbCompPred, kProbCompPredOffs);
    __TRACE_PROB_FIELD(p.kf_uv_mode_prob, kKfUvModeProb, kKfUvModeProbOffs);
    __TRACE_PROB_FIELD(p.a.inter_mode_prob, kInterModeProbs,
                       kInterModeProbsOffs);
    __TRACE_PROB_FIELD(p.a.intra_inter_prob, kIntraInterProbs,
                       kIntraInterProbsOffs);
    __TRACE_PROB_FIELD(p.a.uv_mode_prob, kUvModeProb, kUvModeProbOffs);
    __TRACE_PROB_FIELD(p.a.tx8x8_prob, kTx8x8Probs, kTx8x8ProbsOffs);
    __TRACE_PROB_FIELD(p.a.tx16x16_prob, kTx16x16Probs, kTx16x16ProbsOffs);
    __TRACE_PROB_FIELD(p.a.tx32x32_prob, kTx32x32Probs, kTx32x32ProbsOffs);
    __TRACE_PROB_FIELD(p.a.sb_ymode_prob, kSbYmodeProb, kSbYmodeProbOffs);
    __TRACE_PROB_FIELD(p.a.partition_prob, kPartitionProb, kPartitionProbOffs);
    __TRACE_PROB_FIELD(p.a.switchable_interp_prob, kSwitchableInterpProb,
                       kSwitchableInterpProbOffs);
    __TRACE_PROB_FIELD(p.a.comp_inter_prob, kCompInterProb, kCompInterProbOffs);
    __TRACE_PROB_FIELD(p.a.single_ref_prob, kSingleRefProb, kSingleRefProbOffs);
    __TRACE_PROB_FIELD(p.a.comp_ref_prob, kCompRefProb, kCompRefProbOffs);
    __TRACE_PROB_FIELD(p.a.mb_segment_tree_probs, kMbSegmentTreeProbs,
                       kMbSegmentTreeProbsOffs);
    __TRACE_PROB_FIELD(p.a.segment_pred_probs, kSegmentPredProbs,
                       kSegmentPredProbsOffs);

    __TRACE_PROB_FIELD(p.a.prob_coeffs, kProbCoeffs, 0);
    __TRACE_PROB_FIELD(p.a.prob_coeffs8x8, kProbCoeffs8x8, 0);
    __TRACE_PROB_FIELD(p.a.prob_coeffs16x16, kProbCoeffs16x16, 0);
    __TRACE_PROB_FIELD(p.a.prob_coeffs32x32, kProbCoeffs32x32, 0);

    // AV1 stuff
    __TRACE_PROB_FIELD(p.a.av1_intra_ext_tx_prob, kAv1ExtTx, kAv1ExtTxOffs);
    __TRACE_PROB_FIELD(p.a.motion_mode_prob, kMotionModeProb,
                       kMotionModeProbOffs);
#ifdef DUAL_FILTER
    __TRACE_PROB_FIELD(p.a.switchable_interp_ext_prob_0,
                       kSwitchableInterpExtProb + 0,
                       kSwitchableInterpExtProbOffs);
    __TRACE_PROB_FIELD(p.a.switchable_interp_ext_prob_1,
                       kSwitchableInterpExtProb + 1,
                       kSwitchableInterpExtProbOffs);
#endif

    __TRACE_PROB_FIELD(p.a.mbskip_probs, kMbSkipProbs, kMbSkipProbOffs);
    __TRACE_PROB_FIELD(p.a.ref_mv_nmvc.joints_sign[0].joints, kNmvcJoints,
                       kNmvcJointsOffs);
    __TRACE_PROB_FIELD(p.a.ref_mv_nmvc.joints_sign[0].sign, kNmvcSign,
                       kNmvcSignOffs);
    __TRACE_PROB_FIELD(p.a.ref_mv_nmvc.magnitude[0].class0, kNmvcClass0,
                       kNmvcClass0Offs);
    __TRACE_PROB_FIELD(p.a.ref_mv_nmvc.magnitude[0].classes, kNmvcClasses,
                       kNmvcClassesOffs);
    __TRACE_PROB_FIELD(p.a.ref_mv_nmvc.magnitude[0].class0_fp, kNmvcClass0Fp,
                       kNmvcClass0FpOffs);
    __TRACE_PROB_FIELD(p.a.ref_mv_nmvc.magnitude[0].bits, kNmvcBits,
                       kNmvcBitsOffs);
    printf("\n");
#undef __TRACE_PROB_FIELD

#define __TRACE_COUNT_FIELD(struct_field, hw_addr)                        \
  {                                                                       \
    ptrdiff_t o = (u8 *)&(struct_field) - (u8 *)&c;                       \
    size_t s = sizeof(struct_field);                                      \
    u32 a = hw_addr;                                                      \
    const char *cmp = (o == a * 4) ? cmp_match : cmp_mismatch;            \
    printf(kTraceCntsFmtStr, #struct_field, o / 32, o % 32 / 4, s / 4, a, \
           cmp);                                                          \
  }
    printf(
        "Ctx counters aligned to addresses: base, first_counter, "
        "num_counters\n");
    __TRACE_COUNT_FIELD(c.inter_mode_counts, inter_mode_counts);
    __TRACE_COUNT_FIELD(c.sb_ymode_counts, sb_ymode_counts);
    __TRACE_COUNT_FIELD(c.uv_mode_counts, uv_mode_counts);
    __TRACE_COUNT_FIELD(c.partition_counts, partition_counts);
    __TRACE_COUNT_FIELD(c.switchable_interp_counts, switchable_interp_counts);
    __TRACE_COUNT_FIELD(c.intra_inter_count, intra_inter_count);
    __TRACE_COUNT_FIELD(c.comp_inter_count, comp_inter_count);
    __TRACE_COUNT_FIELD(c.single_ref_count, single_ref_count);
    __TRACE_COUNT_FIELD(c.comp_ref_count, comp_ref_count);
    __TRACE_COUNT_FIELD(c.tx32x32_count, tx32x32_count);
    __TRACE_COUNT_FIELD(c.tx16x16_count, tx16x16_count);
    __TRACE_COUNT_FIELD(c.tx8x8_count, tx8x8_count);
    __TRACE_COUNT_FIELD(c.mbskip_count, mbskip_count);
    __TRACE_COUNT_FIELD(c.count_coeffs, countCoeffs);
    __TRACE_COUNT_FIELD(c.count_coeffs8x8, countCoeffs8x8);
    __TRACE_COUNT_FIELD(c.count_coeffs16x16, countCoeffs16x16);
    __TRACE_COUNT_FIELD(c.count_coeffs32x32, countCoeffs32x32);
    __TRACE_COUNT_FIELD(c.count_eobs, countEobs);

    // AV1 stuff
    __TRACE_COUNT_FIELD(c.av1_intra_ext_tx_count, countIntraExtTx);
    __TRACE_COUNT_FIELD(c.av1_inter_ext_tx_count, countInterExtTx);
    __TRACE_COUNT_FIELD(c.av1_seg_tree, countSegTree);
    __TRACE_COUNT_FIELD(c.av1_seg_pred, countSegPred);
#ifdef DUAL_FILTER
    __TRACE_COUNT_FIELD(c.switchable_interp_ext_counts, countDualFilter);
#endif  // DUAL_FILTER
    __TRACE_COUNT_FIELD(c.motion_mode_count, countMotionMode);
    __TRACE_COUNT_FIELD(c.newmv_mode_count, newmv_mode_counts);
    __TRACE_COUNT_FIELD(c.zeromv_mode_count, zeromv_mode_counts);
    __TRACE_COUNT_FIELD(c.refmv_mode_count, refmv_mode_counts);
    __TRACE_COUNT_FIELD(c.drl_mode_count, drl_mode_counts);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].joints, nmv_counts_base + joints);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].sign, nmv_counts_base + sign);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].classes,
                        nmv_counts_base + classes);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].class0, nmv_counts_base + class0);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].bits, nmv_counts_base + bits);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].class0_fp,
                        nmv_counts_base + class0_fp);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].fp, nmv_counts_base + fp);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].class0_hp,
                        nmv_counts_base + class0_hp);
    __TRACE_COUNT_FIELD(c.ref_mv_nmvcount[0].hp, nmv_counts_base + hp);
    printf("\n");
  }
#undef __TRACE_COUNT_FIELD
#endif  // TRACE_PROB_TRABLES
}

void Av1GetProbs(struct Av1Decoder *dec) {
  /* Frame context tells which frame is used as reference, make
   * a copy of the context to use as base for this frame probs. */
  dec->entropy = dec->entropy_last[dec->frame_context_idx];
}

void Av1StoreProbs(struct Av1Decoder *dec) {
  /* After adaptation the probs refresh either ARF or IPF probs. */
  if (dec->refresh_entropy_probs) {
    dec->entropy_last[dec->frame_context_idx] = dec->entropy;
  }
}

void Av1StoreAdaptProbs(struct Av1Decoder *dec) {
  /* Adaptation is based on previous ctx before update. */
  dec->prev_ctx = dec->entropy.a;
}

static void UpdateNmv(struct VpBoolCoder *bc, av1_prob *const p,
                      const av1_prob upd_p) {
  if (Av1DecodeBool(bc, 252)) *p = Av1ReadProbDiffUpdate(bc, *p);
}

u32 Av1DecodeMvUpdate(struct VpBoolCoder *bc, struct Av1Decoder *dec) {
  u32 i, j, k;
  struct NmvContext *joints_sign = &dec->entropy.a.nmvc;
  struct NmvContext *magnitude = &dec->entropy.a.nmvc;

  for (j = 0; j < MV_JOINTS - 1; ++j) {
    UpdateNmv(bc, &joints_sign->joints[j], AV1_NMV_UPDATE_PROB);
  }
  for (i = 0; i < 2; ++i) {
    UpdateNmv(bc, &joints_sign->sign[i], AV1_NMV_UPDATE_PROB);
    for (j = 0; j < MV_CLASSES - 1; ++j) {
      UpdateNmv(bc, &magnitude->classes[i][j], AV1_NMV_UPDATE_PROB);
    }
    for (j = 0; j < CLASS0_SIZE - 1; ++j) {
      UpdateNmv(bc, &magnitude->class0[i][j], AV1_NMV_UPDATE_PROB);
    }
    for (j = 0; j < MV_OFFSET_BITS; ++j) {
      UpdateNmv(bc, &magnitude->bits[i][j], AV1_NMV_UPDATE_PROB);
    }
  }

  for (i = 0; i < 2; ++i) {
    for (j = 0; j < CLASS0_SIZE; ++j) {
      for (k = 0; k < 3; ++k)
        UpdateNmv(bc, &magnitude->class0_fp[i][j][k], AV1_NMV_UPDATE_PROB);
    }
    for (j = 0; j < 3; ++j) {
      UpdateNmv(bc, &magnitude->fp[i][j], AV1_NMV_UPDATE_PROB);
    }
  }

  if (dec->allow_high_precision_mv) {
    for (i = 0; i < 2; ++i) {
      UpdateNmv(bc, &magnitude->class0_hp[i], AV1_NMV_UPDATE_PROB);
      UpdateNmv(bc, &magnitude->hp[i], AV1_NMV_UPDATE_PROB);
    }
  }

  return HANTRO_OK;
}

static int MergeIndex(int v, int n, int modulus) {
  int max1 = (n - 1 - modulus / 2) / modulus + 1;
  if (v < max1)
    v = v * modulus + modulus / 2;
  else {
    int w;
    v -= max1;
    w = v;
    v += (v + modulus - modulus / 2) / modulus;
    while (v % modulus == modulus / 2 ||
           w != v - (v + modulus - modulus / 2) / modulus)
      v++;
  }
  return v;
}

static int Av1InvRecenterNonneg(int v, int m) {
  if (v > (m << 1))
    return v;
  else if ((v & 1) == 0)
    return (v >> 1) + m;
  else
    return m - ((v + 1) >> 1);
}

static int InvRemapProb(int v, int m) {
  const int n = 255;
  v = MergeIndex(v, n - 1, MODULUS_PARAM);
  m--;
  if ((m << 1) <= n) {
    return 1 + Av1InvRecenterNonneg(v + 1, m);
  } else {
    return n - Av1InvRecenterNonneg(v + 1, n - 1 - m);
  }
}

av1_prob Av1ReadProbDiffUpdate(struct VpBoolCoder *bc, int oldp) {
  int delp = Av1DecodeSubExp(bc, 4, 255);
  return (av1_prob)InvRemapProb(delp, oldp);
}

u32 Av1DecodeCoeffUpdate(
    struct VpBoolCoder *bc,
    u8 prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                  [ENTROPY_NODES_PART1]) {
  u32 i, j, k, l, m;
  u32 tmp;

  tmp = Av1ReadBits(bc, 1);
  STREAM_TRACE("coeff_prob_update_flag", (int)tmp);
  if (!tmp) return HANTRO_OK;

  for (i = 0; i < BLOCK_TYPES; i++) {
    for (j = 0; j < REF_TYPES; j++) {
      for (k = 0; k < COEF_BANDS; k++) {
        for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
          if (l >= 3 && k == 0) continue;

          for (m = 0; m < UNCONSTRAINED_NODES; m++) {
            tmp = Av1DecodeBool(bc, 252);
            CHECK_END_OF_STREAM(tmp);
            if (tmp) {
              u8 old, newp;
              old = prob_coeffs[i][j][k][l][m];
              newp = Av1ReadProbDiffUpdate(bc, old);
              STREAM_TRACE("coeff_prob_delta_subexp", newp);
              CHECK_END_OF_STREAM(tmp);
#ifdef TRACE_HDR_PROBS
              printf("hdr prob[%u][%u][%u][%u][%u] %3u -> %3u\n", i, j, k, l, m,
                     old, newp);
#endif
              prob_coeffs[i][j][k][l][m] = newp;
            }
          }
        }
      }
    }
  }
  return HANTRO_OK;
}

#define COEF_COUNT_SAT 24
#define COEF_MAX_UPDATE_FACTOR 112
#define COEF_COUNT_SAT_KEY 24
#define COEF_MAX_UPDATE_FACTOR_KEY 112
#define COEF_COUNT_SAT_AFTER_KEY 24
#define COEF_MAX_UPDATE_FACTOR_AFTER_KEY 128

static void UpdateCoefProbs(
    u8 dst_coef_probs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                     [ENTROPY_NODES_PART1],
    u8 pre_coef_probs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                     [ENTROPY_NODES_PART1],
    av1_coeff_count *coef_counts,
    u32 (*eob_counts)[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS], int count_sat,
    int update_factor) {
  int t, i, j, k, l, count;
  unsigned int branch_ct[ENTROPY_NODES][2];
  av1_prob coef_probs[ENTROPY_NODES];
  int factor;

  for (i = 0; i < BLOCK_TYPES; ++i)
    for (j = 0; j < REF_TYPES; ++j)
      for (k = 0; k < COEF_BANDS; ++k) {
        for (l = 0; l < PREV_COEF_CONTEXTS; ++l) {
          if (l >= 3 && k == 0) continue;
          Av1TreeProbsFromDistribution(av1_coefmodel_tree, coef_probs,
                                       branch_ct, coef_counts[i][j][k][l], 0);
          branch_ct[0][1] = eob_counts[i][j][k][l] - branch_ct[0][0];
          coef_probs[0] = GetBinaryProb(branch_ct[0][0], branch_ct[0][1]);
          for (t = 0; t < UNCONSTRAINED_NODES; ++t) {
            count = branch_ct[t][0] + branch_ct[t][1];
            count = count > count_sat ? count_sat : count;
            factor = (update_factor * count / count_sat);
            dst_coef_probs[i][j][k][l][t] = WeightedProb(
                pre_coef_probs[i][j][k][l][t], coef_probs[t], factor);
#ifdef TRACE_HDR_PROBS
            if (pre_coef_probs[i][j][k][l][t] != dst_coef_probs[i][j][k][l][t])
              printf("prob[%d][%d][%d][%d][%d] %3d -> %3d\n", i, j, k, l, t,
                     pre_coef_probs[i][j][k][l][t],
                     dst_coef_probs[i][j][k][l][t]);
#endif
          }
        }
      }
}

void Av1AdaptCoefProbs(struct Av1Decoder *cm) {
  int count_sat;
  int update_factor; /* denominator 256 */

  if (cm->key_frame || cm->intra_only) {
    update_factor = COEF_MAX_UPDATE_FACTOR_KEY;
    count_sat = COEF_COUNT_SAT_KEY;
  } else if (cm->prev_is_key_frame) {
    update_factor = COEF_MAX_UPDATE_FACTOR_AFTER_KEY; /* adapt quickly */
    count_sat = COEF_COUNT_SAT_AFTER_KEY;
  } else {
    update_factor = COEF_MAX_UPDATE_FACTOR;
    count_sat = COEF_COUNT_SAT;
  }

  {
    i32 i, j, k, t, l;
    STREAMTRACE_D("%s",
        "static const unsigned int\ncoef_counts"
        "[BLOCK_TYPES][REF_TYPES][COEF_BANDS]"
        "[PREV_COEF_CONTEXTS] [UNCONSTRAINED_NODES+1] = {\n");
    for (i = 0; i < BLOCK_TYPES; ++i) {
      STREAMTRACE_D_NP("%s","  {\n");
      for (l = 0; l < REF_TYPES; ++l) {
        STREAMTRACE_D_NP("%s","    {\n");
        for (j = 0; j < COEF_BANDS; ++j) {
          STREAMTRACE_D_NP("%s","      {\n");
          for (k = 0; k < PREV_COEF_CONTEXTS; ++k) {
            STREAMTRACE_D_NP("%s","        {");
            for (t = 0; t < UNCONSTRAINED_NODES + 1; ++t)
              STREAMTRACE_D_NP("%d, ", cm->ctx_ctr.count_coeffs[i][l][j][k][t]);
            STREAMTRACE_D_NP("%s","        },\n");
          }
          STREAMTRACE_D_NP("%s","      },\n");
        }
        STREAMTRACE_D_NP("%s","    },\n");
      }
      STREAMTRACE_D_NP("%s","  },\n");
    }
    STREAMTRACE_D_NP("%s","};\n");
    STREAMTRACE_D("%s",
        "static const unsigned int\ncoef_counts_8x8"
        "[BLOCK_TYPES_8X8][REF_TYPES][COEF_BANDS]"
        "[PREV_COEF_CONTEXTS] [UNCONSTRAINED_NODES+1] = {\n");
    for (i = 0; i < BLOCK_TYPES; ++i) {
      STREAMTRACE_D_NP("%s","  {\n");
      for (l = 0; l < REF_TYPES; ++l) {
        STREAMTRACE_D_NP("%s","    {\n");
        for (j = 0; j < COEF_BANDS; ++j) {
          STREAMTRACE_D_NP("%s","      {\n");
          for (k = 0; k < PREV_COEF_CONTEXTS; ++k) {
            STREAMTRACE_D_NP("%s","        {");
            for (t = 0; t < UNCONSTRAINED_NODES + 1; ++t)
              STREAMTRACE_D_NP("%d, ", cm->ctx_ctr.count_coeffs8x8[i][l][j][k][t]);
            STREAMTRACE_D_NP("%s","        },\n");
          }
          STREAMTRACE_D_NP("%s","      },\n");
        }
        STREAMTRACE_D_NP("%s","    },\n");
      }
      STREAMTRACE_D_NP("%s","  },\n");
    }
    STREAMTRACE_D_NP("%s","};\n");
  }

  UpdateCoefProbs(cm->entropy.a.prob_coeffs, cm->prev_ctx.prob_coeffs,
                  cm->ctx_ctr.count_coeffs, cm->ctx_ctr.count_eobs[TX_4X4],
                  count_sat, update_factor);
  UpdateCoefProbs(cm->entropy.a.prob_coeffs8x8, cm->prev_ctx.prob_coeffs8x8,
                  cm->ctx_ctr.count_coeffs8x8, cm->ctx_ctr.count_eobs[TX_8X8],
                  count_sat, update_factor);
  UpdateCoefProbs(cm->entropy.a.prob_coeffs16x16, cm->prev_ctx.prob_coeffs16x16,
                  cm->ctx_ctr.count_coeffs16x16,
                  cm->ctx_ctr.count_eobs[TX_16X16], count_sat, update_factor);
  UpdateCoefProbs(cm->entropy.a.prob_coeffs32x32, cm->prev_ctx.prob_coeffs32x32,
                  cm->ctx_ctr.count_coeffs32x32,
                  cm->ctx_ctr.count_eobs[TX_32X32], count_sat, update_factor);
}

void av1_tx_counts_to_branch_counts_32x32(unsigned int *tx_count_32x32p,
                                          unsigned int (*ct_32x32p)[2]) {
  ct_32x32p[0][0] = tx_count_32x32p[TX_4X4];
  ct_32x32p[0][1] = tx_count_32x32p[TX_8X8] + tx_count_32x32p[TX_16X16] +
                    tx_count_32x32p[TX_32X32];
  ct_32x32p[1][0] = tx_count_32x32p[TX_8X8];
  ct_32x32p[1][1] = tx_count_32x32p[TX_16X16] + tx_count_32x32p[TX_32X32];
  ct_32x32p[2][0] = tx_count_32x32p[TX_16X16];
  ct_32x32p[2][1] = tx_count_32x32p[TX_32X32];
}

void av1_tx_counts_to_branch_counts_16x16(unsigned int *tx_count_16x16p,
                                          unsigned int (*ct_16x16p)[2]) {
  ct_16x16p[0][0] = tx_count_16x16p[TX_4X4];
  ct_16x16p[0][1] = tx_count_16x16p[TX_8X8] + tx_count_16x16p[TX_16X16];
  ct_16x16p[1][0] = tx_count_16x16p[TX_8X8];
  ct_16x16p[1][1] = tx_count_16x16p[TX_16X16];
}

void av1_tx_counts_to_branch_counts_8x8(unsigned int *tx_count_8x8p,
                                        unsigned int (*ct_8x8p)[2]) {
  ct_8x8p[0][0] = tx_count_8x8p[TX_4X4];
  ct_8x8p[0][1] = tx_count_8x8p[TX_8X8];
}

#define MODE_COUNT_SAT 20
#define MODE_MAX_UPDATE_FACTOR 128
#define MAX_PROBS 32
static void UpdateModeProbs(int n_modes, const av1_tree_index *tree,
                            unsigned int *cnt, av1_prob *pre_probs,
                            av1_prob *dst_probs, u32 tok0_offset) {
  av1_prob probs[MAX_PROBS];
  unsigned int branch_ct[MAX_PROBS][2];
  int t, count, factor;

  ASSERT(n_modes - 1 < MAX_PROBS);
  Av1TreeProbsFromDistribution(tree, probs, branch_ct, cnt, tok0_offset);
  for (t = 0; t < n_modes - 1; ++t) {
    count = branch_ct[t][0] + branch_ct[t][1];
    count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
    factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
    dst_probs[t] = WeightedProb(pre_probs[t], probs[t], factor);
  }
}

static int UpdateModeCt(av1_prob pre_prob, av1_prob prob,
                        unsigned int branch_ct[2]) {
  int factor, count = branch_ct[0] + branch_ct[1];
  count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
  factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
  return WeightedProb(pre_prob, prob, factor);
}

static int update_mode_ct2(av1_prob pre_prob, unsigned int branch_ct[2]) {
  return UpdateModeCt(pre_prob, GetBinaryProb(branch_ct[0], branch_ct[1]),
                      branch_ct);
}

void Av1AdaptIntraFrameProbs(struct Av1Decoder *cm) {
  i32 i, j;
  struct Av1AdaptiveEntropyProbs *fc = &cm->entropy.a;

#ifdef MODE_COUNT_TESTING
  int t;

  printf(
      "static const unsigned int\nymode_counts"
      "[%d] = {\n",
      MAX_INTRA_MODES);
  for (t = 0; t < MAX_INTRA_MODES; ++t)
    printf("%d, ", cm->ctx_ctr.ymode_counts[t]);
  printf("};\n");
  printf(
      "static const unsigned int\nuv_mode_counts"
      "[%d][%d] = {\n", MAX_INTRA_MODES, MAX_INTRA_MODES);
  for (i = 0; i < MAX_INTRA_MODES; ++i) {
    printf("  {");
    for (t = 0; t < MAX_INTRA_MODES; ++t)
      printf("%u, ", cm->ctx_ctr.uv_mode_counts[i][t]);
    printf("},\n");
  }
  printf("};\n");
  printf(
      "static const unsigned int\nbmode_counts"
      "[%d] = {\n", MAX_INTRA_MODES);
  for (t = 0; t < MAX_INTRA_MODES; ++t)
    printf("%d, ", cm->ctx_ctr.bmode_counts[t]);
  printf("};\n");
  printf("static const unsigned int\ntx8x8_counts = {\n");
  for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
    for (j = 0; j < TX_SIZE_MAX_SB - 2; ++j) {
      printf("%u, ", cm->ctx_ctr.tx8x8_count[i][j]);
    }
    printf("\n");
  }
  printf("};\n");
  printf("static const unsigned int\ntx16x16_counts = {\n");
  for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
    for (j = 0; j < TX_SIZE_MAX_SB - 1; ++j) {
      printf("%u, ", cm->ctx_ctr.tx16x16_count[i][j]);
    }
    printf("\n");
  }
  printf("};\n");
  printf("static const unsigned int\ntx32x32_counts = {\n");
  for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
    for (j = 0; j < TX_SIZE_MAX_SB; ++j) {
      printf("%u, ", cm->ctx_ctr.tx32x32_count[i][j]);
    }
    printf("\n");
  }
  printf("};\n");
#endif

  const av1_tree_index *intra_mode_tree = av1hwd_intra_mode_tree;
  int num_intra_modes = AV1_INTRA_MODES;

  for (i = 0; i < num_intra_modes; ++i) {
    UpdateModeProbs(num_intra_modes, intra_mode_tree,
                    cm->ctx_ctr.uv_mode_counts[i], cm->prev_ctx.uv_mode_prob[i],
                    cm->entropy.a.uv_mode_prob[i], 0);
  }
  for (i = 0; i < NUM_PARTITION_CONTEXTS; i++)
    UpdateModeProbs(PARTITION_TYPES, av1hwd_partition_tree,
                    cm->ctx_ctr.partition_counts[i],
                    cm->prev_ctx.partition_prob[INTER_FRAME][i],
                    cm->entropy.a.partition_prob[INTER_FRAME][i], 0);

  if (cm->transform_mode == TX_MODE_SELECT) {
    unsigned int branch_ct_8x8p[TX_SIZE_MAX_SB - 3][2];
    unsigned int branch_ct_16x16p[TX_SIZE_MAX_SB - 2][2];
    unsigned int branch_ct_32x32p[TX_SIZE_MAX_SB - 1][2];
    for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
      av1_tx_counts_to_branch_counts_8x8(cm->ctx_ctr.tx8x8_count[i],
                                         branch_ct_8x8p);
      for (j = 0; j < TX_SIZE_MAX_SB - 3; ++j) {
        int factor;
        int count = branch_ct_8x8p[j][0] + branch_ct_8x8p[j][1];
        av1_prob prob =
            GetBinaryProb(branch_ct_8x8p[j][0], branch_ct_8x8p[j][1]);
        count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
        factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
        fc->tx8x8_prob[i][j] =
            WeightedProb(cm->prev_ctx.tx8x8_prob[i][j], prob, factor);
      }
    }
    for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
      av1_tx_counts_to_branch_counts_16x16(cm->ctx_ctr.tx16x16_count[i],
                                           branch_ct_16x16p);
      for (j = 0; j < TX_SIZE_MAX_SB - 2; ++j) {
        int factor;
        int count = branch_ct_16x16p[j][0] + branch_ct_16x16p[j][1];
        av1_prob prob =
            GetBinaryProb(branch_ct_16x16p[j][0], branch_ct_16x16p[j][1]);
        count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
        factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
        fc->tx16x16_prob[i][j] =
            WeightedProb(cm->prev_ctx.tx16x16_prob[i][j], prob, factor);
      }
    }
    for (i = 0; i < TX_SIZE_CONTEXTS; ++i) {
      av1_tx_counts_to_branch_counts_32x32(cm->ctx_ctr.tx32x32_count[i],
                                           branch_ct_32x32p);
      for (j = 0; j < TX_SIZE_MAX_SB - 1; ++j) {
        int factor;
        int count = branch_ct_32x32p[j][0] + branch_ct_32x32p[j][1];
        av1_prob prob =
            GetBinaryProb(branch_ct_32x32p[j][0], branch_ct_32x32p[j][1]);
        count = count > MODE_COUNT_SAT ? MODE_COUNT_SAT : count;
        factor = (MODE_MAX_UPDATE_FACTOR * count / MODE_COUNT_SAT);
        fc->tx32x32_prob[i][j] =
            WeightedProb(cm->prev_ctx.tx32x32_prob[i][j], prob, factor);
      }
    }
  }
  for (i = 0; i < MBSKIP_CONTEXTS; ++i)
    fc->mbskip_probs[i] = update_mode_ct2(cm->prev_ctx.mbskip_probs[i],
                                          cm->ctx_ctr.mbskip_count[i]);
}

#define MVREF_COUNT_SAT 20
#define MVREF_MAX_UPDATE_FACTOR 128
void Av1AdaptInterFrameProbs(struct Av1Decoder *cm) {
  int i, j;
  struct Av1AdaptiveEntropyProbs *fc = &cm->entropy.a;
  u32(*mode_ct)[AV1_INTER_MODES - 1][2] = cm->ctx_ctr.inter_mode_counts;

  for (j = 0; j < INTER_MODE_CONTEXTS; j++) {
    for (i = 0; i < AV1_INTER_MODES - 1; i++) {
      int count = mode_ct[j][i][0] + mode_ct[j][i][1], factor;

      count = count > MVREF_COUNT_SAT ? MVREF_COUNT_SAT : count;
      factor = (MVREF_MAX_UPDATE_FACTOR * count / MVREF_COUNT_SAT);
      cm->entropy.a.inter_mode_prob[j][i] = WeightedProb(
          cm->prev_ctx.inter_mode_prob[j][i],
          GetBinaryProb(mode_ct[j][i][0], mode_ct[j][i][1]), factor);
    }
  }
  for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
    fc->intra_inter_prob[i] = update_mode_ct2(cm->prev_ctx.intra_inter_prob[i],
                                              cm->ctx_ctr.intra_inter_count[i]);
  for (i = 0; i < COMP_INTER_CONTEXTS; i++)
    fc->comp_inter_prob[i] = update_mode_ct2(cm->prev_ctx.comp_inter_prob[i],
                                             cm->ctx_ctr.comp_inter_count[i]);
  for (i = 0; i < REF_CONTEXTS; i++)
    fc->comp_ref_prob[i] = update_mode_ct2(cm->prev_ctx.comp_ref_prob[i],
                                           cm->ctx_ctr.comp_ref_count[i]);
  for (i = 0; i < REF_CONTEXTS; i++)
    for (j = 0; j < 2; j++)
      fc->single_ref_prob[i][j] =
          update_mode_ct2(cm->prev_ctx.single_ref_prob[i][j],
                          cm->ctx_ctr.single_ref_count[i][j]);

  const av1_tree_index *intra_mode_tree = av1hwd_intra_mode_tree;
  int num_intra_modes = AV1_INTRA_MODES;

  for (i = 0; i < BLOCK_SIZE_GROUPS; ++i) {
    UpdateModeProbs(
        num_intra_modes, intra_mode_tree, cm->ctx_ctr.sb_ymode_counts[i],
        cm->prev_ctx.sb_ymode_prob[i], cm->entropy.a.sb_ymode_prob[i], 0);
  }

  if (cm->mcomp_filter_type == SWITCHABLE) {
    for (i = 0; i <= AV1_SWITCHABLE_FILTERS; ++i) {
      UpdateModeProbs(AV1_SWITCHABLE_FILTERS, av1hwd_switchable_interp_tree,
                      cm->ctx_ctr.switchable_interp_counts[i],
                      cm->prev_ctx.switchable_interp_prob[i],
                      cm->entropy.a.switchable_interp_prob[i], 0);
    }
  }
}

void Av1GetCDFs(struct Av1Decoder *dec, int ref_idx) {
  dec->cdfs = &dec->cdfs_last[ref_idx];
  dec->cdfs_addr = dec->cdfs_last_addr + ((addr_t)(&dec->cdfs_last[ref_idx]) - (addr_t)&dec->cdfs_last[0]);
//  dec->cdfs_ndvc = &dec->cdfs_last_ndvc[ref_idx];
}

void Av1StoreCDFs(struct Av1Decoder *dec) {
  for (int i = 0; i < NUM_REF_FRAMES; i++) {
    if (dec->refresh_frame_flags & (1 << i)) {
      if (&dec->cdfs_last[i] != dec->cdfs) {
        dec->cdfs_last[i] = *dec->cdfs;
        //dec->cdfs_last_ndvc[i] = *dec->cdfs_ndvc;
      }
    }
  }
}
