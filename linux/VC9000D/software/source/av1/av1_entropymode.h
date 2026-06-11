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

#ifndef AV1_ENTROPYMODE_H_
#define AV1_ENTROPYMODE_H_

#include "av1_commondec.h"
#include "av1_treecoder.h"
#include "av1hwd_decoder.h"

#define DEFAULT_COMP_INTRA_PROB 32

#define AV1_DEF_INTERINTRA_PROB 248
#define AV1_UPD_INTERINTRA_PROB 192
#define SEPARATE_INTERINTRA_UV 0

extern const av1_tree_index av1hwd_intra_mode_tree[];

extern const av1_prob av1_kf_default_bmode_probs[AV1_INTRA_MODES]
                                                [AV1_INTRA_MODES]
                                                [AV1_INTRA_MODES - 1];

extern const av1_tree_index av1hwd_intra_mode_tree[];

extern const av1_tree_index av1_sb_mv_ref_tree[];

/* probability models for partition information */
extern const av1_tree_index av1hwd_partition_tree[];
extern struct av1_token av1_partition_encodings[PARTITION_TYPES];
extern const av1_prob av1_partition_probs[NUM_FRAME_TYPES]
                                         [NUM_PARTITION_CONTEXTS]
                                         [PARTITION_TYPES];

void Av1EntropyModeInit(void);

struct AV1Common;

void Av1InitMbmodeProbs(struct Av1Decoder *x);

extern void Av1InitModeContexts(struct Av1Decoder *pc);

extern const enum InterpolationFilterType
    av1hwd_switchable_interp[AV1_SWITCHABLE_FILTERS];

extern const int av1hwd_switchable_interp_map[SWITCHABLE + 1];

extern const av1_tree_index
    av1hwd_switchable_interp_tree[2 * (AV1_SWITCHABLE_FILTERS - 1)];
#ifdef DUAL_FILTER
extern const av1_tree_index
    av1hwd_switchable_interp_ext_tree[2 * (AV1_SWITCHABLE_EXT_FILTERS - 1)];
#endif
extern struct av1_token
    av1hwd_switchable_interp_encodings[AV1_SWITCHABLE_FILTERS];

extern const av1_prob av1hwd_switchable_interp_prob[AV1_SWITCHABLE_FILTERS + 1]
                                                   [AV1_SWITCHABLE_FILTERS - 1];
#ifdef DUAL_FILTER
extern const av1_prob
    av1hwd_switchable_interp_ext_prob[AV1_SWITCHABLE_EXT_FILTERS + 1]
                                     [AV1_SWITCHABLE_EXT_FILTERS - 1];
#endif

extern const av1_prob av1_default_tx_probs_32x32p[TX_SIZE_CONTEXTS]
                                                 [TX_SIZE_MAX_SB - 1];
extern const av1_prob av1_default_tx_probs_16x16p[TX_SIZE_CONTEXTS]
                                                 [TX_SIZE_MAX_SB - 2];
extern const av1_prob av1_default_tx_probs_8x8p[TX_SIZE_CONTEXTS]
                                               [TX_SIZE_MAX_SB - 3];

extern const av1_prob av1_default_intra_ext_tx_prob[EXT_TX_SIZES][TX_TYPES]
                                                   [TX_TYPES - 1];
extern const av1_prob av1_default_inter_ext_tx_prob[EXT_TX_SIZES][TX_TYPES - 1];

extern const av1_tree_index av1_segment_tree[];

#endif
