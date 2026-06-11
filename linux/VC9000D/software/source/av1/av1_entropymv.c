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

#include "av1_entropymv.h"
#include <string.h>
#include "av1_modecont.h"
#include "sw_debug.h"
#include "dec_log.h"


#define MV_COUNT_SAT 20
#define MV_MAX_UPDATE_FACTOR 128

const av1_tree_index av1hwd_mv_joint_tree[2 * MV_JOINTS - 2] = {
    -MV_JOINT_ZERO, 2, -MV_JOINT_HNZVZ, 4, -MV_JOINT_HZVNZ, -MV_JOINT_HNZVNZ};

const av1_tree_index av1hwd_mv_class_tree[2 * MV_CLASSES - 2] = {
    -MV_CLASS_0, 2,           -MV_CLASS_1, 4,           6,
    8,           -MV_CLASS_2, -MV_CLASS_3, 10,          12,
    -MV_CLASS_4, -MV_CLASS_5, -MV_CLASS_6, 14,          16,
    18,          -MV_CLASS_7, -MV_CLASS_8, -MV_CLASS_9, -MV_CLASS_10,
};

const av1_tree_index av1hwd_mv_class0_tree[2 * CLASS0_SIZE - 2] = {
    -0,
    -1,
};

const av1_tree_index av1hwd_mv_fp_tree[2 * 4 - 2] = {-0, 2, -1, 4, -2, -3};

const struct NmvContext av1_default_nmv_context = {
    {32, 64, 96},                 /* joints */
    {128, 128},                   /* sign */
    {{216}, {208}},               /* class0 */
    {{64, 96, 64}, {64, 96, 64}}, /* fp */
    {160, 160},                   /* class0_hp bit */
    {128, 128},                   /* hp */
    {{224, 144, 192, 168, 192, 176, 192, 198, 198, 245},
     {216, 128, 176, 160, 176, 176, 192, 198, 198, 208}}, /* class */
    {{{128, 128, 64}, {96, 112, 64}},
     {{128, 128, 64}, {96, 112, 64}}}, /* class0_fp */
    {{136, 140, 148, 160, 176, 192, 224, 234, 234, 240},
     {136, 140, 148, 160, 176, 192, 224, 234, 234, 240}}, /* bits */
};

#define MvClassBase(c) ((c) ? (CLASS0_SIZE << (c + 2)) : 0)

enum MvClassType Av1GetMvClass(int z, int *offset) {
  enum MvClassType c = MV_CLASS_0;
  if (z < CLASS0_SIZE * 8)
    c = MV_CLASS_0;
  else if (z < CLASS0_SIZE * 16)
    c = MV_CLASS_1;
  else if (z < CLASS0_SIZE * 32)
    c = MV_CLASS_2;
  else if (z < CLASS0_SIZE * 64)
    c = MV_CLASS_3;
  else if (z < CLASS0_SIZE * 128)
    c = MV_CLASS_4;
  else if (z < CLASS0_SIZE * 256)
    c = MV_CLASS_5;
  else if (z < CLASS0_SIZE * 512)
    c = MV_CLASS_6;
  else if (z < CLASS0_SIZE * 1024)
    c = MV_CLASS_7;
  else if (z < CLASS0_SIZE * 2048)
    c = MV_CLASS_8;
  else if (z < CLASS0_SIZE * 4096)
    c = MV_CLASS_9;
  else if (z < CLASS0_SIZE * 8192)
    c = MV_CLASS_10;
  else {
    ASSERT(0);
  }
  if (offset) *offset = z - MvClassBase(c);
  return c;
}

static void AdaptProb(av1_prob *dest, av1_prob prep, unsigned int ct[2]) {
  const int count = MIN(ct[0] + ct[1], MV_COUNT_SAT);
  if (count) {
    const av1_prob newp = GetBinaryProb(ct[0], ct[1]);
    const int factor = MV_MAX_UPDATE_FACTOR * count / MV_COUNT_SAT;
    *dest = WeightedProb(prep, newp, factor);
  } else {
    *dest = prep;
  }
}

static unsigned int AdaptProbs(unsigned int i, av1_tree tree,
                               av1_prob this_probs[],
                               const av1_prob last_probs[],
                               const unsigned int num_events[]) {
  av1_prob this_prob;

  const u32 left = tree[i] <= 0 ? num_events[-tree[i]]
                                : AdaptProbs(tree[i], tree, this_probs,
                                             last_probs, num_events);

  const u32 right = tree[i + 1] <= 0 ? num_events[-tree[i + 1]]
                                     : AdaptProbs(tree[i + 1], tree, this_probs,
                                                  last_probs, num_events);

  u32 weight = left + right;
  if (weight) {
    this_prob = GetBinaryProb(left, right);
    weight = weight > MV_COUNT_SAT ? MV_COUNT_SAT : weight;
    this_prob = WeightedProb(last_probs[i >> 1], this_prob,
                             MV_MAX_UPDATE_FACTOR * weight / MV_COUNT_SAT);
  } else {
    this_prob = last_probs[i >> 1];
  }
  this_probs[i >> 1] = this_prob;
  return left + right;
}

void Av1AdaptNmvProbs(struct Av1Decoder *cm) {
  i32 usehp = cm->allow_high_precision_mv;
  i32 i, j;
  struct NmvContextCounts *nmvcount = &cm->ctx_ctr.nmvcount;
  struct NmvContext *joints_sign = &cm->entropy.a.nmvc;
  struct NmvContext *pre_joints_sign = &cm->prev_ctx.nmvc;
  struct NmvContext *magnitude = &cm->entropy.a.nmvc;
  struct NmvContext *pre_magnitude = &cm->prev_ctx.nmvc;
  i32 k;
  STREAMTRACE_D("%s","joints count: ");
  for (j = 0; j < MV_JOINTS; ++j) STREAMTRACE_D_NP("%d ", nmvcount->joints[j]);
  STREAMTRACE_D_NP("%s","\n");
  fflush(stdout);
  STREAMTRACE_D("%s","signs count:\n");
  for (i = 0; i < 2; ++i)
    STREAMTRACE_D_NP("%d/%d ", nmvcount->sign[i][0], nmvcount->sign[i][1]);
  STREAMTRACE_D_NP("%s","\n");
  fflush(stdout);
  STREAMTRACE_D("%s","classes count:\n");
  for (i = 0; i < 2; ++i) {
    for (j = 0; j < MV_CLASSES; ++j) STREAMTRACE_D_NP("%d ", nmvcount->classes[i][j]);
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
  }
  STREAMTRACE_D("%s","class0 count:\n");
  for (i = 0; i < 2; ++i) {
    for (j = 0; j < CLASS0_SIZE; ++j) STREAMTRACE_D_NP("%d ", nmvcount->class0[i][j]);
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
  }
  STREAMTRACE_D("%s","bits count:\n");
  for (i = 0; i < 2; ++i) {
    for (j = 0; j < MV_OFFSET_BITS; ++j)
      STREAMTRACE_D_NP("%d/%d ", nmvcount->bits[i][j][0], nmvcount->bits[i][j][1]);
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
  }
  STREAMTRACE_D("%s","class0_fp count:\n");
  for (i = 0; i < 2; ++i) {
    for (j = 0; j < CLASS0_SIZE; ++j) {
      STREAMTRACE_D_NP("%s","{");
      for (k = 0; k < 4; ++k) STREAMTRACE_D_NP("%d ", nmvcount->class0_fp[i][j][k]);
      STREAMTRACE_D_NP("%s","}, ");
    }
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
  }
  STREAMTRACE_D("%s","fp count:\n");
  for (i = 0; i < 2; ++i) {
    for (j = 0; j < 4; ++j) STREAMTRACE_D_NP("%d ", nmvcount->fp[i][j]);
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
  }
  if (usehp) {
    STREAMTRACE_D("%s","class0_hp count:\n");
    for (i = 0; i < 2; ++i)
      STREAMTRACE_D_NP("%d/%d ", nmvcount->class0_hp[i][0], nmvcount->class0_hp[i][1]);
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
    STREAMTRACE_D("%s","hp count:\n");
    for (i = 0; i < 2; ++i)
      STREAMTRACE_D_NP("%d/%d ", nmvcount->hp[i][0], nmvcount->hp[i][1]);
    STREAMTRACE_D_NP("%s","\n");
    fflush(stdout);
  }

  AdaptProbs(0, av1hwd_mv_joint_tree, joints_sign->joints,
             pre_joints_sign->joints, nmvcount->joints);
  for (i = 0; i < 2; ++i) {
    AdaptProb(&joints_sign->sign[i], pre_joints_sign->sign[i],
              nmvcount->sign[i]);
    AdaptProbs(0, av1hwd_mv_class_tree, magnitude->classes[i],
               pre_magnitude->classes[i], nmvcount->classes[i]);
    AdaptProbs(0, av1hwd_mv_class0_tree, magnitude->class0[i],
               pre_magnitude->class0[i], nmvcount->class0[i]);
    for (j = 0; j < MV_OFFSET_BITS; ++j) {
      AdaptProb(&magnitude->bits[i][j], pre_magnitude->bits[i][j],
                nmvcount->bits[i][j]);
    }
  }
  for (i = 0; i < 2; ++i) {
    for (j = 0; j < CLASS0_SIZE; ++j) {
      AdaptProbs(0, av1hwd_mv_fp_tree, magnitude->class0_fp[i][j],
                 pre_magnitude->class0_fp[i][j], nmvcount->class0_fp[i][j]);
    }
    AdaptProbs(0, av1hwd_mv_fp_tree, magnitude->fp[i], pre_magnitude->fp[i],
               nmvcount->fp[i]);
  }
  if (usehp) {
    for (i = 0; i < 2; ++i) {
      AdaptProb(&magnitude->class0_hp[i], pre_magnitude->class0_hp[i],
                nmvcount->class0_hp[i]);
      AdaptProb(&magnitude->hp[i], pre_magnitude->hp[i], nmvcount->hp[i]);
    }
  }
}

void Av1InitMvProbs(struct Av1Decoder *x) {
  memcpy(&x->entropy.a.nmvc, &av1_default_nmv_context,
         sizeof(struct NmvContext));
}
