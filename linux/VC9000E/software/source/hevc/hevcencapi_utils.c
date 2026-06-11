/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2019 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : VC Encoder API utils
--
------------------------------------------------------------------------------*/
#include <math.h>

#include "hevcencapi.h"
#include "hevcencapi_utils.h"
#include "encasiccontroller.h"
#include "sw_put_bits.h"
#include "pool.h"
#include "sw_slice.h"
#include "tools.h"
#include "axife.h"
#include "ewl.h"
#include "apbfilter.h"
#include "encdec400.h"
#include "hevcenccache.h"
#include "enc_log.h"
#include "encufbc.h"
#include "rate_control_picture.h"

#ifdef INTERNAL_TEST
#include "sw_test_id.h"
#endif

#ifdef TEST_DATA
#include "enctrace.h"
#endif

#include "enctrace.h"

#ifdef VIDEOSTAB_ENABLED
#include "vidstabcommon.h"
#endif

#ifdef SUPPORT_VP9
#include "vp9encapi.h"
#endif

#ifdef SUPPORT_AV1
#include "av1encapi.h"
#endif

#define MAX_SLICE_SIZE_V0 127
#define DEFAULT -255

#ifdef MULTI_FRAME_SUPPORT
/* Batch Mode to supporting multi-frame aggregation */
static VCEncRet vcencBatchCheckCodingCtrlParam(struct vcenc_instance *pEncInst,
                            const VCEncCodingCtrl *pCodeParams);
static VCEncRet vcencBatchUpdateCodingCtrlParam(struct vcenc_instance *pEncInst,
                            const VCEncCodingCtrl *pCodeParams);
#endif /* MULTI_FRAME_SUPPORT */

/*------------------------------------------------------------------------------

    VCEncShutdown

    Function frees the encoder instance.

    Input   inst    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
void VCEncShutdown(VCEncInst inst) {
  struct vcenc_instance *pEncInst = (struct vcenc_instance *)inst;
  const void *ewl;

  ASSERT(inst);

  ewl = pEncInst->asic.ewl;

  if (pEncInst->cb_try_new_params)
    if (pEncInst->regs_bak) EWLfree(pEncInst->regs_bak);

  if (pEncInst->asic.dec400_data) EWLfree(pEncInst->asic.dec400_data);

  if (pEncInst->asic.axife_data) EWLfree(pEncInst->asic.axife_data);

  if (pEncInst->asic.ufbc) EWLfree(pEncInst->asic.ufbc);

  EncAsicMemFree_V2(&pEncInst->asic);

  EWLfree(pEncInst);

  (void)EWLRelease(ewl);
}

VCEncRet VCEncClear(VCEncInst inst) {
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  asicData_s *asic = &vcenc_instance->asic;
  i32 ret = VCENC_OK;
  u32 status = ASIC_STATUS_ERROR;
  EWLCoreWaitJob_t *out = NULL;
  u32 waitCoreJobid;
  u32 next_core_index;

  /* wait for all running job and put to pool in multi-core cases*/
  while (vcenc_instance->reservedCore > 0 && !asic->regs.bVCMDEnable) {
    next_core_index =
        (vcenc_instance->jobCnt + 1) % vcenc_instance->parallelCoreNum;
    waitCoreJobid = vcenc_instance->waitJobid[next_core_index];

    if ((out = (EWLCoreWaitJob_t *)EWLDequeueCoreOutJob(asic->ewl,
                                                        waitCoreJobid)) == NULL)
      break;

    EWLPutJobtoPool(asic->ewl, (struct node *)out);
    vcenc_instance->reservedCore--;
    vcenc_instance->jobCnt++;
  }

  /* clear job from job queue only in 1pass */
  if (0 == vcenc_instance->pass) {
    VCEncJob *job = NULL, *prevJob = NULL;
    job = prevJob = (VCEncJob *)queue_tail(&vcenc_instance->jobQueue);

    /* remove the job in queue to buffer pool if it's not empty*/
    while (job != NULL) {
      prevJob = job;
      queue_remove(&vcenc_instance->jobQueue, (struct node *)job);
      PutBufferToPool(vcenc_instance->jobBufferPool, (void **)&job);
      job = (VCEncJob *)prevJob->next;
    }
  }

  return ret;
}

/*------------------------------------------------------------------------------

    Function name : VCEncStop
    Description   : stop encoding process and clear jobs in queue

    Return type   : VCEncRet
    Argument      : inst - the instance to be stopped
------------------------------------------------------------------------------*/
VCEncRet VCEncStop(VCEncInst inst) {
  struct vcenc_instance *pEncInst = (struct vcenc_instance *)inst;
  struct container *c;
  VCEncRet ret = VCENC_OK;

  APITRACE("VCEncRelease#\n");

  /* Check for illegal inputs */
  if (pEncInst == NULL) {
    APITRACEERR("VCEncRelease: ERROR Null argument\n");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance */
  if (pEncInst->inst != inst) {
    APITRACEERR("VCEncRelease: ERROR Invalid instance\n");
    return VCENC_INSTANCE_ERROR;
  }

  /*for 2pass, need to stop lookahead and cutree thread*/
  if (pEncInst->pass == 2 && pEncInst->lookahead.priv_inst) {
    struct vcenc_instance *pEncInst_priv =
        (struct vcenc_instance *)pEncInst->lookahead.priv_inst;

    /* Stop lookahead thread first, put correct status */
    StopLookaheadThread(&pEncInst->lookahead,
                        pEncInst->encStatus == VCENCSTAT_ERROR);

    StopCuTreeThread(&pEncInst_priv->cuTreeCtl,
                     pEncInst->encStatus == VCENCSTAT_ERROR);
  }

  /* just wait for encoding job and clear all of them*/
  if (pEncInst->pass != 1) VCEncClear(inst);

  return ret;
}

/*------------------------------------------------------------------------------
  next_picture calculates next input picture depending input and output
  frame rates.
------------------------------------------------------------------------------*/
u64 CalNextPic(VCEncGopConfig *cfg, int picture_cnt) {
  u64 numer, denom;

  numer = (u64)cfg->inputRateNumer * (u64)cfg->outputRateDenom;
  denom = (u64)cfg->inputRateDenom * (u64)cfg->outputRateNumer;

  return numer * (picture_cnt / (1 << cfg->interlacedFrame)) / denom;
}

/*------------------------------------------------------------------------------
Function name : GenNextPicConfig
Description   : generate the pic reference configure befor one picture encoded
Return type   : void
Argument      : VCEncIn *pEncIn
Argument      : u8 *gopCfgOffset
Argument      : i32 codecH264
Argument      : i32 i32LastPicPoc
------------------------------------------------------------------------------*/
void GenNextPicConfig(VCEncIn *pEncIn, const u8 *gopCfgOffset,
                      i32 i32LastPicPoc,
                      struct vcenc_instance *vcenc_instance) {
  i32 i, j, k, i32Poc, i32LTRIdx, numRefPics, numRefPics_org = 0;
  u8 u8CfgStart, u8IsLTR_ref, u8IsUpdated;
  i32 i32MaxpicOrderCntLsb = 1 << 16;

  ASSERT(pEncIn != NULL);
  ASSERT(gopCfgOffset != NULL);

  u8CfgStart = gopCfgOffset[pEncIn->gopSize];
  memcpy(&pEncIn->gopCurrPicConfig,
         &(pEncIn->gopConfig.pGopPicCfg[pEncIn->gopConfig.id]),
         sizeof(VCEncGopPicConfig));

  pEncIn->i8SpecialRpsIdx = -1;
  pEncIn->i8SpecialRpsIdx_next = -1;
  memset(pEncIn->bLTR_used_by_cur, 0, sizeof(u32) * VCENC_MAX_LT_REF_FRAMES);
  if (0 == pEncIn->gopConfig.special_size) return;
  /* special config does not apply to leading pictures */
  if (pEncIn->poc < 0) return;

  /* update ltr */
  i32 i32RefIdx;
  for (i32RefIdx = 0; i32RefIdx < pEncIn->gopConfig.ltrcnt; i32RefIdx++) {
    if (HANTRO_TRUE == pEncIn->bLTR_need_update[i32RefIdx])
      pEncIn->long_term_ref_pic[i32RefIdx] = i32LastPicPoc;
  }

  memset(pEncIn->bLTR_need_update, 0, sizeof(u32) * pEncIn->gopConfig.ltrcnt);
  if (0 != pEncIn->bIsIDR && !vcenc_instance->gdrDuration) {
    i32Poc = 0;
    pEncIn->gopCurrPicConfig.temporalId = 0;
    for (i = 0; i < pEncIn->gopConfig.ltrcnt; i++) {
      for (j = 0; j < pEncIn->gopConfig.special_size; j++) {
        if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) ||
            (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0) ||
            (((pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr > 0) &&
              (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr - 1) != i)))
          continue;

        pEncIn->long_term_ref_pic[i] = INVALITED_POC;
      }
    }
  } else
    i32Poc = pEncIn->poc;
  /* check the current picture encoded as LTR*/
  pEncIn->u8IdxEncodedAsLTR = 0;
  for (j = 0; j < pEncIn->gopConfig.special_size; j++) {
    if (pEncIn->bIsPeriodUsingLTR == HANTRO_FALSE) break;

    true_e bLTRUpdatePeriod =
        pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval > 0;
    true_e bLTRUpdateOneTimes =
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr > 0) &&
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0) &&
        (pEncIn
             ->long_term_ref_pic[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr -
                                 1] == INVALITED_POC);
    if (!(bLTRUpdatePeriod || bLTRUpdateOneTimes) ||
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr == 0))
      continue;

    i32Poc = i32Poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

    if (i32Poc < 0) {
      i32Poc += i32MaxpicOrderCntLsb;
      if (i32Poc > (i32MaxpicOrderCntLsb >> 1)) i32Poc = -1;
    }

    i32 interval = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval
                       ? pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval
                       : i32MaxpicOrderCntLsb;
    if ((i32Poc >= 0) && (i32Poc % interval == 0)) {
      /* more than one LTR at the same frame position */
      if (0 != pEncIn->u8IdxEncodedAsLTR) {
        // reuse the same POC LTR
        pEncIn->bLTR_need_update[pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr -
                                 1] = HANTRO_TRUE;
        continue;
      }

      pEncIn->gopCurrPicConfig.codingType =
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
           FRAME_TYPE_RESERVED)
              ? pEncIn->gopCurrPicConfig.codingType
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
      pEncIn->gopCurrPicConfig.nonReference =
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].nonReference ==
           FRAME_TYPE_RESERVED)
              ? pEncIn->gopCurrPicConfig.nonReference
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].nonReference;
      pEncIn->gopCurrPicConfig.numRefPics =
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
           NUMREFPICS_RESERVED)
              ? pEncIn->gopCurrPicConfig.numRefPics
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
      pEncIn->gopCurrPicConfig.QpFactor =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED)
              ? pEncIn->gopCurrPicConfig.QpFactor
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
      pEncIn->gopCurrPicConfig.QpOffset =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED)
              ? pEncIn->gopCurrPicConfig.QpOffset
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
      pEncIn->gopCurrPicConfig.temporalId =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId ==
           TEMPORALID_RESERVED)
              ? pEncIn->gopCurrPicConfig.temporalId
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

      if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics !=
           NUMREFPICS_RESERVED)) {
        for (k = 0; k < (i32)pEncIn->gopCurrPicConfig.numRefPics; k++) {
          pEncIn->gopCurrPicConfig.refPics[k].ref_pic =
              pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic;
          pEncIn->gopCurrPicConfig.refPics[k].used_by_cur =
              pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].used_by_cur;
        }
      }

      pEncIn->u8IdxEncodedAsLTR = pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Ltr;
      pEncIn->bLTR_need_update[pEncIn->u8IdxEncodedAsLTR - 1] = HANTRO_TRUE;
    }
  }

  if (0 != pEncIn->bIsIDR && !vcenc_instance->gdrDuration) return;

  u8IsUpdated = 0;
  for (j = 0; j < pEncIn->gopConfig.special_size; j++) {
    if (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) continue;

    /* no changed */
    if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
         FRAME_TYPE_RESERVED) &&
        ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
         NUMREFPICS_RESERVED) &&
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor ==
         QPFACTOR_RESERVED) &&
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset ==
         QPOFFSET_RESERVED) &&
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId ==
         TEMPORALID_RESERVED))
      continue;

    /* only consider LTR ref config */
    if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
         NUMREFPICS_RESERVED)) {
      /* reserved for later */
      pEncIn->i8SpecialRpsIdx = -1;
      u8IsUpdated = 1;
      continue;
    }

    if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
         VCENC_INTRA_FRAME) &&
        (pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == 0))
      numRefPics = 1;
    else
      numRefPics = pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;

    numRefPics_org = pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;

    // check if all LTR are OK for current cfg
    true_e bUsedCurrCfg = HANTRO_FALSE;
    for (k = 0; k < numRefPics; k++) {
      u8IsLTR_ref = IS_LONG_TERM_REF_DELTAPOC(
          pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic);
      if ((u8IsLTR_ref == HANTRO_FALSE) &&
          (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
        continue;
      i32LTRIdx =
          (u8IsLTR_ref == HANTRO_TRUE)
              ? LONG_TERM_REF_DELTAPOC2ID(
                    pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic)
              : 0;

      if ((pEncIn->long_term_ref_pic[i32LTRIdx] == INVALITED_POC) &&
          (u8IsLTR_ref != HANTRO_FALSE) &&
          (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
        continue;

      i32Poc = pEncIn->poc - pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

      if (i32Poc < 0) {
        i32Poc += i32MaxpicOrderCntLsb;
        if (i32Poc > (i32MaxpicOrderCntLsb >> 1)) i32Poc = -1;
      }

      if ((i32Poc >= 0) &&
          (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval == 0)) {
        bUsedCurrCfg = HANTRO_TRUE;
      } else {
        bUsedCurrCfg = HANTRO_FALSE;
        break;
      }
    }

    if (bUsedCurrCfg == HANTRO_TRUE) {
      pEncIn->gopCurrPicConfig.codingType =
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
           FRAME_TYPE_RESERVED)
              ? pEncIn->gopCurrPicConfig.codingType
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType;
      pEncIn->gopCurrPicConfig.nonReference =
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].nonReference ==
           FRAME_TYPE_RESERVED)
              ? pEncIn->gopCurrPicConfig.nonReference
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].nonReference;
      pEncIn->gopCurrPicConfig.numRefPics =
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
           NUMREFPICS_RESERVED)
              ? pEncIn->gopCurrPicConfig.numRefPics
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
      pEncIn->gopCurrPicConfig.QpFactor =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor == QPFACTOR_RESERVED)
              ? pEncIn->gopCurrPicConfig.QpFactor
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor;
      pEncIn->gopCurrPicConfig.QpOffset =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset == QPOFFSET_RESERVED)
              ? pEncIn->gopCurrPicConfig.QpOffset
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset;
      pEncIn->gopCurrPicConfig.temporalId =
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId ==
           TEMPORALID_RESERVED)
              ? pEncIn->gopCurrPicConfig.temporalId
              : pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId;

      if ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics !=
          NUMREFPICS_RESERVED) {
        for (i = 0; i < (i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
             i++) {
          pEncIn->gopCurrPicConfig.refPics[i].ref_pic =
              pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[i].ref_pic;
          pEncIn->gopCurrPicConfig.refPics[i].used_by_cur =
              pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[i].used_by_cur;
        }
      }
      pEncIn->i8SpecialRpsIdx = j;
      u8IsUpdated = 1;
    }

    // if curr cfg is OK, not check next special cfg
    if (0 != u8IsUpdated) break;
  }

  if (IS_H264(vcenc_instance->codecFormat)) {
    u8IsUpdated = 0;
    for (j = 0; j < pEncIn->gopConfig.special_size; j++) {
      if (pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval <= 0) continue;

      /* no changed */
      if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
           FRAME_TYPE_RESERVED) &&
          ((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
           NUMREFPICS_RESERVED) &&
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpFactor ==
           QPFACTOR_RESERVED) &&
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].QpOffset ==
           QPOFFSET_RESERVED) &&
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].temporalId ==
           TEMPORALID_RESERVED))
        continue;

      /* only consider LTR ref config */
      if (((i32)pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics ==
           NUMREFPICS_RESERVED)) {
        /* reserved for later */
        pEncIn->i8SpecialRpsIdx_next = -1;
        u8IsUpdated = 1;
        continue;
      }

      if ((pEncIn->gopConfig.pGopPicSpecialCfg[j].codingType ==
           VCENC_INTRA_FRAME) &&
          (pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics == 0))
        numRefPics = 1;
      else
        numRefPics = pEncIn->gopConfig.pGopPicSpecialCfg[j].numRefPics;
      true_e bUsedCurrCfg = HANTRO_FALSE;
      for (k = 0; k < numRefPics; k++) {
        u8IsLTR_ref = IS_LONG_TERM_REF_DELTAPOC(
            pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic);
        if ((u8IsLTR_ref == HANTRO_FALSE) &&
            (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
          continue;
        i32LTRIdx =
            (u8IsLTR_ref == HANTRO_TRUE)
                ? LONG_TERM_REF_DELTAPOC2ID(
                      pEncIn->gopConfig.pGopPicSpecialCfg[j].refPics[k].ref_pic)
                : 0;

        if ((pEncIn->long_term_ref_pic[i32LTRIdx] == INVALITED_POC) &&
            (u8IsLTR_ref != HANTRO_FALSE) &&
            (0 == pEncIn->gopConfig.pGopPicSpecialCfg[j].i32short_change))
          continue;

        i32Poc = pEncIn->poc + pEncIn->gopConfig.delta_poc_to_next -
                 pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Offset;

        if (i32Poc < 0) {
          i32Poc += i32MaxpicOrderCntLsb;
          if (i32Poc > (i32MaxpicOrderCntLsb >> 1)) i32Poc = -1;
        }

        if ((i32Poc >= 0) &&
            (i32Poc % pEncIn->gopConfig.pGopPicSpecialCfg[j].i32Interval ==
             0)) {
          bUsedCurrCfg = HANTRO_TRUE;
        } else {
          bUsedCurrCfg = HANTRO_FALSE;
          break;
        }
      }

      if (bUsedCurrCfg == HANTRO_TRUE) {
        pEncIn->i8SpecialRpsIdx_next = j;
        u8IsUpdated = 1;
      }

      if (0 != u8IsUpdated) break;
    }
  }

  if (pEncIn->gopCurrPicConfig.codingType == VCENC_INTRA_FRAME)
    pEncIn->gopCurrPicConfig.numRefPics = numRefPics_org;

  j = 0;
  for (k = 0; k < (i32)pEncIn->gopCurrPicConfig.numRefPics; k++) {
    if (0 != pEncIn->gopCurrPicConfig.refPics[k].used_by_cur) j++;

    u8IsLTR_ref =
        IS_LONG_TERM_REF_DELTAPOC(pEncIn->gopCurrPicConfig.refPics[k].ref_pic);
    if (u8IsLTR_ref == HANTRO_FALSE) continue;
    i32LTRIdx =
        LONG_TERM_REF_DELTAPOC2ID(pEncIn->gopCurrPicConfig.refPics[k].ref_pic);

    pEncIn->bLTR_used_by_cur[i32LTRIdx] =
        pEncIn->gopCurrPicConfig.refPics[k].used_by_cur ? HANTRO_TRUE
                                                        : HANTRO_FALSE;
  }

  // check LTRs is same picture or NOT
  for (i = 0; i < pEncIn->gopConfig.ltrcnt; i++) {
    for (i32 jj = 0; jj < i; jj++) {
      if (pEncIn->bLTR_used_by_cur[i] && pEncIn->bLTR_used_by_cur[jj] &&
          (pEncIn->long_term_ref_pic[i] == pEncIn->long_term_ref_pic[jj])) {
        pEncIn->bLTR_used_by_cur[i] = HANTRO_FALSE;
        j--;
      }
    }
  }

  /* check whether need to set current frame as a b frame */
  u32 client_type = VCEncGetClientType(vcenc_instance->codecFormat);
  const EWLHwConfig_t *cfg =
    EncAsicGetAsicConfig(client_type, vcenc_instance->ctx);
  if (!cfg) {
    printf("Cannot Get Valid Configure!\n");
    return;
  }
  u8 b2RefP = (vcenc_instance->numRefP == 2) &&  cfg->maxRefNumList0Minus1 &&
       (IS_H264(vcenc_instance->codecFormat)
       || IS_HEVC(vcenc_instance->codecFormat));
  if (cfg->bFrameEnabled && j > 1 && !b2RefP)
    pEncIn->gopCurrPicConfig.codingType = VCENC_BIDIR_PREDICTED_FRAME;
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeTraceEncInPara
    Description   : Trace EncIn parameters
    Return type   : void
    Argument      : pEncIn - input parameters provided by user
                    vcenc_instance - encoder instance
------------------------------------------------------------------------------*/
void StrmEncodeTraceEncInPara(VCEncIn *pEncIn,
                              struct vcenc_instance *vcenc_instance) {
  u32 tileId;
  ptr_t inputBus;
  u32 *pOutBuf;
  ptr_t busOutBuf;
  u32 outBufSize;
  APITRACE("VCEncStrmEncode#\n");
  if (pEncIn != NULL) {
    APITRACEPARAM(" %s : %d\n", "timeIncrement", pEncIn->timeIncrement);

    for (tileId = 0; tileId < vcenc_instance->num_tile_columns; tileId++) {
      inputBus = (tileId == 0) ? pEncIn->busLuma
                               : pEncIn->tileExtra[tileId - 1].busLuma;
      APITRACEPARAM_X(" %s : %p\n", "busLuma", inputBus);
      inputBus = (tileId == 0) ? pEncIn->busChromaU
                               : pEncIn->tileExtra[tileId - 1].busChromaU;
      APITRACEPARAM_X(" %s : %p\n", "busChromaU", inputBus);
      inputBus = (tileId == 0) ? pEncIn->busChromaV
                               : pEncIn->tileExtra[tileId - 1].busChromaV;
      APITRACEPARAM_X(" %s : %p\n", "busChromaV", inputBus);

      pOutBuf = (tileId == 0) ? pEncIn->pOutBuf[0]
                              : pEncIn->tileExtra[tileId - 1].pOutBuf[0];
      APITRACEPARAM_X(" %s : %p\n", "pOutBuf%d", tileId, pOutBuf);
      busOutBuf = (tileId == 0) ? pEncIn->busOutBuf[0]
                                : pEncIn->tileExtra[tileId - 1].busOutBuf[0];
      APITRACEPARAM_X(" %s : %p\n", "busOutBuf%d", tileId, busOutBuf);
      outBufSize = (tileId == 0) ? pEncIn->outBufSize[0]
                                 : pEncIn->tileExtra[tileId - 1].outBufSize[0];
      APITRACEPARAM(" %s : %d\n", "outBufSize%d", tileId, outBufSize);
      if (vcenc_instance->asic.regs.asicCfg->streamBufferChain) {
        pOutBuf = (tileId == 0) ? pEncIn->pOutBuf[1]
                                : pEncIn->tileExtra[tileId - 1].pOutBuf[1];
        APITRACEPARAM_X(" %s : %p\n", "pOutBuf1", pOutBuf);
        busOutBuf = (tileId == 0) ? pEncIn->busOutBuf[1]
                                  : pEncIn->tileExtra[tileId - 1].busOutBuf[1];
        APITRACEPARAM_X(" %s : %p\n", "busOutBuf1", busOutBuf);
        outBufSize = (tileId == 0)
                         ? pEncIn->outBufSize[1]
                         : pEncIn->tileExtra[tileId - 1].outBufSize[1];
        APITRACEPARAM(" %s : %d\n", "outBufSize1", outBufSize);
      }
    }

    APITRACEPARAM(" %s : %d\n", "codingType", pEncIn->codingType);
    APITRACEPARAM(" %s : %d\n", "poc", pEncIn->poc);
    APITRACEPARAM(" %s : %d\n", "gopSize", pEncIn->gopSize);
    APITRACEPARAM(" %s : %d\n", "gopPicIdx", pEncIn->gopPicIdx);
    APITRACEPARAM_X(" %s : %p\n", "roiMapDeltaQpAddr",
                    pEncIn->roiMapDeltaQpAddr);
  }
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeCheckPara
    Description   : check illegal condition and return a status
    Return type   : VCEncRet
    Argument      : vcenc_instance - encoder instance
                    pEncIn - input parameters provided by user
                    pEncOut - place where output info is returned
                    asic - asic data
------------------------------------------------------------------------------*/
VCEncRet StrmEncodeCheckPara(struct vcenc_instance *vcenc_instance,
                             VCEncIn *pEncIn, VCEncOut *pEncOut,
                             asicData_s *asic, u32 client_type) {
  u32 asicId;
  u32 tileId;
  /* Check for illegal inputs. */
  if ((vcenc_instance == NULL) || (pEncIn == NULL) || (pEncOut == NULL)) {
    APITRACEERR("VCEncStrmEncode: ERROR Null argument\n");
    return VCENC_NULL_ARGUMENT;
  }

  /* Check for existing instance. */
  if (vcenc_instance->inst != vcenc_instance) {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid instance\n");
    return VCENC_INSTANCE_ERROR;
  }
  /* Check status, INIT and ERROR not allowed. */
  if ((vcenc_instance->encStatus != VCENCSTAT_START_STREAM) &&
      (vcenc_instance->encStatus != VCENCSTAT_START_FRAME)) {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid status\n");
    return VCENC_INVALID_STATUS;
  }

  /* Check for features not compiled.*/
#ifndef SUPPORT_AXIFE
  if (vcenc_instance->axiFEEnable != 0) {
    APITRACEERR("VCEncStrmEncode: AXIFE not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef CHECKSUM_CRC_BUILD_SUPPORT
  if (vcenc_instance->hashctx.hash_type != 0) {
    APITRACEERR(
        "VCEncStrmEncode: hash(crc&checksum) not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef CUTREE_BUILD_SUPPORT
  if (vcenc_instance->cuTreeCtl.lookaheadDepth != 0) {
    APITRACEERR("VCEncStrmEncode: cutree not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef VCMD_BUILD_SUPPORT
  if (vcenc_instance->asic.regs.bVCMDAvailable != 0) {
    APITRACEERR("VCEncStrmEncode: VCMD not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#if !defined (SUPPORT_DEC400) && !defined (SUPPORT_UFBC)
  if (pEncIn->dec400Enable != 0) /**< \brief 1: bypass 2: enable */
  {
    APITRACEERR("VCEncStrmEncode: DEC400 not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef LOW_LATENCY_BUILD_SUPPORT
  if (vcenc_instance->inputLineBuf.inputLineBufEn != 0) {
    APITRACEERR(
        "VCEncStrmEncode: inputLineBuffer not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef LOW_LATENCY_SLICEINFO_SUPPORT
  if (vcenc_instance->inputSliceInfoPollEn != 0) {
      APITRACEERR(
          "VCEncStrmEncode: inputSliceInfo not supported at compile time.\n");
      return VCENC_ERROR;
  }
#endif
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
  if (!asic->regs.asicCfg->prpLowlatencySignalbyDDR) {
    APITRACEERR(
         "VCEncStrmEncode: HW not support ddr low latency.\n");
    return VCENC_ERROR;
  }
  if (vcenc_instance->inputLineBuf.inputLineBufEn) {
    APITRACEERR(
         "If enable linebuffer mode, low latency not use polling sliceinfo.\n");
    return VCENC_ERROR;
  }
  if (vcenc_instance->inputSliceInfoPollEn &&
      vcenc_instance->preProcess.rotation != 0) {
    APITRACEERR(
         "VCEncStrmEncode: inputSliceInfo NOT support PRP rotation currently!.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef SEI_BUILD_SUPPORT
  if (vcenc_instance->rateControl.sei.enabled != 0 ||
      pEncIn->externalSEICount > 0) {
    APITRACEERR("VCEncStrmEncode: sei not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef RATE_CONTROL_BUILD_SUPPORT
  if (vcenc_instance->rateControl.picRc == 1 &&
      vcenc_instance->rateControl.rcMode != 5) {
    APITRACEERR(
        "VCEncStrmEncode: rateControl not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef INTERNAL_TEST
  if (vcenc_instance->testId != 0) {
    APITRACEERR("VCEncStrmEncode: test id not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef CUINFO_BUILD_SUPPORT
  if (vcenc_instance->outputCuInfo != 0) {
    APITRACEERR("VCEncStrmEncode: cuinfo not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef ROI_BUILD_SUPPORT
  if (vcenc_instance->roiMapEnable != 0) {
    APITRACEERR("VCEncStrmEncode: roi not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif
#ifndef STREAM_CTRL_BUILD_SUPPORT
  if (vcenc_instance->bInputfileList != 0) {
    APITRACEERR(
        "VCEncStrmEncode: stream ctrl not supported at compile time.\n");
    return VCENC_ERROR;
  }
#endif

  asicId = EncAsicGetAsicHWid(client_type, vcenc_instance->ctx);
  /* Check for invalid input values. */
  if ((pEncIn->gopSize > 1) &&
      ((HW_ID_MAJOR_NUMBER(asicId) <= 1) && HW_PRODUCT_H2(asicId))) {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid gopSize\n");
    return VCENC_INVALID_ARGUMENT;
  }

  if (pEncIn->codingType > VCENC_NOTCODED_FRAME) {
    APITRACEERR("VCEncStrmEncode: ERROR Invalid coding type\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Check output stream buffers. */
  u32 *pOutBuf;
  ptr_t busOutBuf;
  u32 outBufSize;
  for (tileId = 0; tileId < vcenc_instance->num_tile_columns; tileId++) {
    pOutBuf = (tileId == 0) ? pEncIn->pOutBuf[0]
                            : pEncIn->tileExtra[tileId - 1].pOutBuf[0];
    busOutBuf = (tileId == 0) ? pEncIn->busOutBuf[0]
                              : pEncIn->tileExtra[tileId - 1].busOutBuf[0];
    outBufSize = (tileId == 0) ? pEncIn->outBufSize[0]
                               : pEncIn->tileExtra[tileId - 1].outBufSize[0];
    if ((busOutBuf == 0) || (pOutBuf == NULL)) {
      APITRACEERR("VCEncStrmEncode: ERROR Invalid output stream buffer\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if ((vcenc_instance->streamMultiSegment.streamMultiSegmentMode == 0) &&
        (outBufSize < VCENC_STREAM_MIN_BUF0_SIZE)) {
      APITRACEERR("VCEncStrmEncode: ERROR Too small output stream buffer\n");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if (pEncIn->busOutBuf[1] || pEncIn->pOutBuf[1] || pEncIn->outBufSize[1]) {
    if (!asic->regs.asicCfg->streamBufferChain) {
      APITRACEERR("VCEncStrmEncode: ERROR Two stream buffer not supported\n");
      return VCENC_INVALID_ARGUMENT;
    } else if ((pEncIn->busOutBuf[1] == 0) || (pEncIn->pOutBuf[1] == NULL)) {
      APITRACEERR("VCEncStrmEncode: ERROR Invalid output stream buffer1\n");
      return VCENC_INVALID_ARGUMENT;
    } else if (vcenc_instance->streamMultiSegment.streamMultiSegmentMode != 0) {
      APITRACEERR(
          "VCEncStrmEncode:two output buffer not support multi-segment\n");
      return VCENC_INVALID_ARGUMENT;
    } else if (IS_VP9(vcenc_instance->codecFormat) || IS_AV1(vcenc_instance->codecFormat)) {
      APITRACEERR(
          "VCEncStrmEncode: ERROR Two stream buffer not supported by VP9 and AV1\n");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  if (vcenc_instance->streamMultiSegment.streamMultiSegmentMode != 0 &&
      vcenc_instance->parallelCoreNum > 1) {
    APITRACEERR("VCEncStrmEncode: multi-segment not support multi-core\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* Check GDR. */
  if ((vcenc_instance->gdrEnabled) &&
      (pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME)) {
    APITRACEERR("VCEncStrmEncode: ERROR gdr not support B frame\n");
    return VCENC_INVALID_ARGUMENT;
  }
  /* Check limitation for H.264 baseline profile. */
  if (IS_H264(vcenc_instance->codecFormat) && vcenc_instance->profile ==
      VCENC_H264_BASE_PROFILE &&
      pEncIn->codingType == VCENC_BIDIR_PREDICTED_FRAME) {
    APITRACEERR(
        "VCEncStrmEncode: ERROR Invalid frame type for baseline profile\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* VCENC_CHROMA_IDC_422 and inputformat: HW TODO */
  /* Check inoutformat for VCENC_CHROMA_IDC_422 */
  if (vcenc_instance->asic.regs.codedChromaIdc == VCENC_CHROMA_IDC_422 &&
      ((vcenc_instance->preProcess.inputFormat >= VCENC_YUV420_PLANAR_8BIT_TILE_32_32 &&
        vcenc_instance->preProcess.inputFormat <= VCENC_YUV420_10BIT_TILE_8_8) ||
       (vcenc_instance->preProcess.inputFormat >= VCENC_YUV420_UV_8BIT_TILE_64_2 &&
        vcenc_instance->preProcess.inputFormat <= VCENC_YUV420_UV_10BIT_TILE_128_2) ||
        vcenc_instance->preProcess.inputFormat == VCENC_YUV444_PLANAR)) {
    APITRACEERR(
         "VCEncStrmEncode: ERROR The input format not supported by CodedChromaIdc 422\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* VCENC_CHROMA_IDC_444 and inputformat: HW TODO */
  /* Check inoutformat for VCENC_CHROMA_IDC_444 */
  if (vcenc_instance->asic.regs.codedChromaIdc == VCENC_CHROMA_IDC_444 &&
      ((vcenc_instance->preProcess.inputFormat >= VCENC_YUV420_PLANAR_8BIT_TILE_32_32 &&
        vcenc_instance->preProcess.inputFormat <= VCENC_YUV420_10BIT_TILE_8_8) ||
       (vcenc_instance->preProcess.inputFormat >= VCENC_YUV420_UV_8BIT_TILE_64_2 &&
        vcenc_instance->preProcess.inputFormat <= VCENC_YUV420_UV_10BIT_TILE_128_2))) {
    APITRACEERR(
         "VCEncStrmEncode: ERROR The input format not supported by CodedChromaIdc 444\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* VCENC_YUV444_PLANAR only supported by CodedChromaIdc 444 */
  if (vcenc_instance->preProcess.inputFormat == VCENC_YUV444_PLANAR &&
      vcenc_instance->asic.regs.codedChromaIdc != VCENC_CHROMA_IDC_444) {
    APITRACEERR(
         "VCEncStrmEncode: ERROR YUV444P format only supported by CodedChromaIdc 444\n");
    return VCENC_INVALID_ARGUMENT;
  }

  switch (vcenc_instance->preProcess.inputFormat) {
    case VCENC_YUV420_PLANAR:
    case VCENC_YVU420_PLANAR:
    case VCENC_YUV420_PLANAR_10BIT_I010:
    case VCENC_YUV422P10bWL:
    case VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR:
    case VCENC_YUV420_PLANAR_8BIT_TILE_32_32:
    case VCENC_YUV444_PLANAR:
      if (!VCENC_BUS_CH_ADDRESS_VALID(pEncIn->busChromaV)) {
        APITRACEERR("VCEncStrmEncode: ERROR Invalid input busChromaV\n");
        return VCENC_INVALID_ARGUMENT;
      }
    /* Fall through. */
    case VCENC_YUV420_SEMIPLANAR:
    case VCENC_YUV420_SEMIPLANAR_VU:
    case VCENC_YUV420_PLANAR_10BIT_P010:
    case VCENC_YVU420_PLANAR_10BIT_P010:
    case VCENC_YUV420_SEMIPLANAR_8BIT_TILE_4_4:
    case VCENC_YUV420_SEMIPLANAR_VU_8BIT_TILE_4_4:
    case VCENC_YUV420_PLANAR_10BIT_P010_TILE_4_4:
    case VCENC_YUV420_SEMIPLANAR_101010:
    case VCENC_YUV420SP10b:
    case VCENC_YUV420_8BIT_TILE_64_4:
    case VCENC_YUV420_UV_8BIT_TILE_64_4:
    case VCENC_YUV420_10BIT_TILE_32_4:
    case VCENC_YUV420_10BIT_TILE_48_4:
    case VCENC_YUV420_VU_10BIT_TILE_48_4:
    case VCENC_YUV420_8BIT_TILE_128_2:
    case VCENC_YUV420_UV_8BIT_TILE_128_2:
    case VCENC_YUV420_10BIT_TILE_96_2:
    case VCENC_YUV420_VU_10BIT_TILE_96_2:
    case VCENC_YUV420_8BIT_TILE_8_8:
    case VCENC_YVU420_8BIT_TILE_8_8:
    case VCENC_YUV420_10BIT_TILE_8_8:
    case VCENC_YUV420_UV_8BIT_TILE_64_2:
    case VCENC_YUV420_UV_10BIT_TILE_128_2:
      if (!VCENC_BUS_ADDRESS_VALID(pEncIn->busChromaU)) {
        APITRACEERR("VCEncStrmEncode: ERROR Invalid input busChromaU\n");
        return VCENC_INVALID_ARGUMENT;
      }
    /* Fall through. */
    case VCENC_YUV400_PLANAR_10BIT_PACKED:
    case VCENC_YUV422_INTERLEAVED_YUYV:
    case VCENC_YUV422_INTERLEAVED_UYVY:
    case VCENC_YUV422_INTERLEAVED_YVYU:
    case VCENC_YUV422_INTERLEAVED_VYUY:
    case VCENC_RGB565:
    case VCENC_BGR565:
    case VCENC_RGB555:
    case VCENC_BGR555:
    case VCENC_RGB444:
    case VCENC_BGR444:
    case VCENC_RGB888:
    case VCENC_BGR888:
    case VCENC_RGB888_24BIT:
    case VCENC_BGR888_24BIT:
    case VCENC_RBG888_24BIT:
    case VCENC_GBR888_24BIT:
    case VCENC_BRG888_24BIT:
    case VCENC_GRB888_24BIT:
    case VCENC_RGB101010:
    case VCENC_BGR101010:
    case VCENC_YUV420_10BIT_PACKED_Y0L2:
    case VCENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4:
    case VCENC_YUV444_XYUV8888:
    case VCENC_YUV444_XYUV2101010:
    case VCENC_RGBX8888:
    case VCENC_BGRX8888:
    case VCENC_RGBX1010102:
    case VCENC_BGRX1010102:
    case VCENC_Y8b:
    case VCENC_Y10bWL:
    case VCENC_Y10bWH:
    case VCENC_Y8b_TILE_8_8:
    case VCENC_Y10bWH_TILE_8_8:
      if (!VCENC_BUS_ADDRESS_VALID(pEncIn->busLuma)) {
        APITRACEERR("VCEncStrmEncode: ERROR Invalid input busLuma\n");
        return VCENC_INVALID_ARGUMENT;
      }
      break;
    default:
      APITRACEERR("VCEncStrmEncode: ERROR Invalid input format\n");
      return VCENC_INVALID_ARGUMENT;
  }

  if (vcenc_instance->preProcess.videoStab) {
    if (!VCENC_BUS_ADDRESS_VALID(pEncIn->busLumaStab)) {
      APITRACE("VCEncStrmEncodeExt: ERROR Invalid input busLumaStab\n");
      return VCENC_INVALID_ARGUMENT;
    }
  }

  /* Stride feature only support YUV420SP and YUV422. */
  if ((vcenc_instance->input_alignment > 1) &&
      ((vcenc_instance->preProcess.inputFormat ==
        VCENC_YUV420_PLANAR_10BIT_PACKED_PLANAR) ||
       (vcenc_instance->preProcess.inputFormat ==
        VCENC_YUV420_10BIT_PACKED_Y0L2) ||
       (vcenc_instance->preProcess.inputFormat ==
        VCENC_YUV420_PLANAR_8BIT_TILE_32_32) ||
       (vcenc_instance->preProcess.inputFormat ==
        VCENC_YUV420_PLANAR_8BIT_TILE_16_16_PACKED_4))) {
    APITRACEERR(
        "VCEncStrmEncode: WARNING alignment doesn't support input format\n");
  }
  return VCENC_OK;
}

void StrmEncodeMeqnSimpleBinCostConfig(struct vcenc_instance *vcenc_instance,
                                       asicData_s *asic) {
  u32 offset = 0;
  if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_HEVC)
    offset = 13;
  else if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_H264)
    offset = 9;
  else if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_AV1)
    offset = 13;
  else if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_VP9)
    offset = 15;

  if (HW_PRODUCT_SYSTEM60(asic->regs.asicHwId) ||
      HW_PRODUCT_VC9000LE(asic->regs.asicHwId)) {
  if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_HEVC)
    offset = 12;
  else if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_H264)
    offset = 12;
  }
  asic->regs.meqnSimpleBinCostOffset = offset;
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeSmartskipConfig
    Description   : configure smart skip parameter
    Return type   : void
    Argument      : asic - asic data
                    pEncIn - input parameters provided by user
                    vcenc_instance - encoder instance
------------------------------------------------------------------------------*/
void StrmEncodeSmartskipConfig(i32 PrevQp, asicData_s *asic,
                              VCEncIn *pEncIn,
                              struct vcenc_instance *vcenc_instance) {
  int i;
  /* firstly use max pixel vaule as default */
  i32 minSmartMeanTh = 255;
  i32 smartQPDelta = (vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS) -
                     (PrevQp >> QP_FRACTIONAL_BITS);
  //noise condition
  i32 iSigmaCurFrm = vcenc_instance->iSigmaCalcd == 0xFFFF ? -1 : vcenc_instance->iSigmaCalcd / 64;
  if (pEncIn->poc == 0)
    vcenc_instance->smartMeanThPrev = vcenc_instance->smartMeanTh[0];//default
  i32 instMeanTh[4] = {vcenc_instance->smartMeanTh[0],
                       vcenc_instance->smartMeanTh[1],
                       vcenc_instance->smartMeanTh[2],
                       vcenc_instance->smartMeanTh[3]};
  i32 smartMeanTh = vcenc_instance->smartMeanTh[0];//default
  i32 defaultTh = 4;//6 as mv0 logic default level
  i32 halfTh = defaultTh / 2;//3 as mv0 logic default half level of RC
  if (iSigmaCurFrm >= 0 && iSigmaCurFrm <= 4) {
    smartMeanTh = MAX(defaultTh, iSigmaCurFrm * 2 + 1);
  } else if (iSigmaCurFrm >= 5 && iSigmaCurFrm <= 11) {
    smartMeanTh = iSigmaCurFrm + 5;
  } else if (iSigmaCurFrm >= 12 && iSigmaCurFrm <= 15) {
    smartMeanTh = 16 + (iSigmaCurFrm - 10) / 2;
  } else if (iSigmaCurFrm >= 16 && iSigmaCurFrm <= 27) {
    smartMeanTh = 19 + (iSigmaCurFrm - 16) / 3;
  } else if (iSigmaCurFrm >= 28) {
    smartMeanTh = 23;
  }
  i32 tmpPrev = vcenc_instance->smartMeanThPrev;
  i32 tmpMeanTh = vcenc_instance->smartMeanTh[0];//need separate 6 to 20 when noise.
  for (i = 0; i < 4; i++) {
    instMeanTh[i] = vcenc_instance->smartMeanTh[i] < defaultTh ?
                    vcenc_instance->smartMeanTh[i] :
                    MAX(MAX(smartMeanTh, defaultTh), vcenc_instance->smartMeanThPrev - 2);
  }
  if (vcenc_instance->smartMeanThPrev < smartMeanTh) {
    vcenc_instance->smartMeanThPrev = smartMeanTh;
  }
#ifdef SYSTEM_BUILD
#ifndef SYSTEM60_BUILD
  extern u32 hwSmartModeNoiseEnable;
  hwSmartModeNoiseEnable =
#endif
#endif
  vcenc_instance->smartModeNoiseEn = vcenc_instance->smartModeEnable;
  asic->regs.smartModeNoiseEn = vcenc_instance->smartModeNoiseEn;
  asic->regs.smartModeEnable = vcenc_instance->smartModeEnable;
  asic->regs.smartH264LumDcTh = vcenc_instance->smartH264LumDcTh;
  asic->regs.smartH264CbDcTh = vcenc_instance->smartH264CbDcTh;
  asic->regs.smartH264CrDcTh = vcenc_instance->smartH264CrDcTh;
  for (i = 0; i < 3; i++) {
    asic->regs.smartHevcLumDcTh[i] = vcenc_instance->smartHevcLumDcTh[i];
    asic->regs.smartHevcChrDcTh[i] = vcenc_instance->smartHevcChrDcTh[i];
    asic->regs.smartHevcLumAcNumTh[i] = vcenc_instance->smartHevcLumAcNumTh[i];
    asic->regs.smartHevcChrAcNumTh[i] = vcenc_instance->smartHevcChrAcNumTh[i];
  }
  asic->regs.smartH264Qp = vcenc_instance->smartH264Qp;
  asic->regs.smartHevcLumQp = vcenc_instance->smartHevcLumQp;
  asic->regs.smartHevcChrQp = vcenc_instance->smartHevcChrQp;
  asic->regs.smartPixNumCntTh = vcenc_instance->smartPixNumCntTh;
  for (i = 0; i < 4; i++) {
    minSmartMeanTh = MIN(minSmartMeanTh, instMeanTh[i]);
    /* scaled smartMeanTh to 1/2;
       SRD could own 20 levels, every 2 levels same smartMeanTh and different pixNum*/
    asic->regs.smartMeanTh[i] = (instMeanTh[i] + 1) / 2 + 2;
  }

  i32 pixNum = 0;
  i32 meanThExt = 0;
  if (minSmartMeanTh % 2 == 0 && minSmartMeanTh <= 10) {
    pixNum = MIN(minSmartMeanTh * 2 + 2, 12);
  } else if (minSmartMeanTh > 10 && minSmartMeanTh <= 14) {
    pixNum = (minSmartMeanTh % 2 != 0) ? 12 : 6;
  } else if (minSmartMeanTh == 15) {
    meanThExt = 1;
    pixNum = 8;
  } else if (minSmartMeanTh == 16) {
    meanThExt = 1;
    pixNum = 12;
  } else if (minSmartMeanTh == 17) {
    meanThExt = 2;
    pixNum = 8;
  } else if (minSmartMeanTh == 18) {
    meanThExt = 2;
    pixNum = 12;
  } else if (minSmartMeanTh == 19) {
    meanThExt = 3;
    pixNum = 8;
  } else if (minSmartMeanTh == 20) {
    meanThExt = 3;
    pixNum = 12;
  } else if (minSmartMeanTh == 21) {
    meanThExt = 4;
    pixNum = 12;
  } else if (minSmartMeanTh == 22) {
    meanThExt = 6;
    pixNum = 12;
  } else if (minSmartMeanTh == 23) {
    meanThExt = 8;
    pixNum = 12;
  }

  asic->regs.smartPixNumCntTh = vcenc_instance->smartPixNumCntTh = pixNum;
  for (i = 0; i < 4; i++) {
    i32 deltaTh = tmpMeanTh > defaultTh ? tmpMeanTh - defaultTh : 0;
    asic->regs.smartMeanTh[i] += meanThExt;
    asic->regs.smartMeanTh[i] = MIN(asic->regs.smartMeanTh[i], 13) + deltaTh;
  }

#if SRD_ENABLE
  vcenc_instance->smartRecoverGap++;
  asic->regs.smartModeEnable = (vcenc_instance->smartModeEnable > 0);//reg width: 1
  if (vcenc_instance->rateControl.picRc == ENCHW_YES &&
      (smartQPDelta < -1 || (smartQPDelta < 0 && vcenc_instance->smartRecoverGap >= 10))) {
    vcenc_instance->smartRecoverGap = 0;
    if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_H264) {
      if (vcenc_instance->smartModeEnable == 2) asic->regs.smartModeEnable = 0;
    } else if (vcenc_instance->codecFormat == VCENC_VIDEO_CODEC_HEVC) {
      if (vcenc_instance->smartModeEnable == 2) asic->regs.smartModeEnable = 0;
    }
    for (i = 0; i < 4; i++)
      asic->regs.smartMeanTh[i] = halfTh;//half default Meanth
    asic->regs.smartPixNumCntTh = 0;
  } else if (vcenc_instance->rateControl.picRc == ENCHW_NO && pEncIn->poc % 10 == 0) {
    asic->regs.smartModeEnable = 0;
  }

#if 0
  //tmp tune info
  i32 curQp = vcenc_instance->rateControl.qpHdr >> QP_FRACTIONAL_BITS;
  printf("poc %d enable %d qp%d noise %d MeanTh %d regMeanTh %d pixNum %d vcencMean %d tmpMeanTh %d tmpPrev %d PrevTh %d\n",
          pEncIn->poc, asic->regs.smartModeEnable, curQp, iSigmaCurFrm, minSmartMeanTh,
          asic->regs.smartMeanTh[0], pixNum, vcenc_instance->smartMeanTh[0], tmpMeanTh,
          tmpPrev, vcenc_instance->smartMeanThPrev);
#endif
#endif
}
/*------------------------------------------------------------------------------

    Function name : StrmEncodeGlobalmvConfig
    Description   : configure global MV parameter
    Return type   : void
    Argument      : asic - asic data
                    pic - picture
                    pEncIn - input parameters provided by user
                    vcenc_instance - encoder instance
------------------------------------------------------------------------------*/
void StrmEncodeGlobalmvConfig(asicData_s *asic, struct sw_picture *pic,
                              VCEncIn *pEncIn,
                              struct vcenc_instance *vcenc_instance) {
#ifdef GLOBAL_MV_ON_SEARCH_RANGE
  int inLoopDSRatio = (vcenc_instance->pass == 2) ? 2 : 1;
  //int inLoopDSRatio = (vcenc_instance->extDSRatio)? (vcenc_instance->extDSRatio + 1) : (vcenc_instance->inLoopDSRatio + 1);
  asic->regs.gmv[0][0] = asic->regs.gmv[0][1] = asic->regs.gmv[1][0] =
      asic->regs.gmv[1][1] = 0;
  if (pic->sliceInst->type != I_SLICE) {
    asic->regs.gmv[0][0] = (i16)(pEncIn->gmv[0][0] * inLoopDSRatio);
    asic->regs.gmv[0][1] = (i16)(pEncIn->gmv[0][1] * inLoopDSRatio);
  }
  if (pic->sliceInst->type == B_SLICE) {
    asic->regs.gmv[1][0] = (i16)(pEncIn->gmv[1][0] * inLoopDSRatio);
    asic->regs.gmv[1][1] = (i16)(pEncIn->gmv[1][1] * inLoopDSRatio);
  }
  if (vcenc_instance->pass == 2) {
    printf("    gmv[0]=(%d,%d), gmv[1]=(%d,%d)\n", asic->regs.gmv[0][0],
           asic->regs.gmv[0][1], asic->regs.gmv[1][0], asic->regs.gmv[1][1]);
  }
#else
  asic->regs.gmv[0][0] = asic->regs.gmv[0][1] = asic->regs.gmv[1][0] =
      asic->regs.gmv[1][1] = 0;
  if (pic->sliceInst->type != I_SLICE) {
    asic->regs.gmv[0][0] = pEncIn->gmv[0][0];
    asic->regs.gmv[0][1] = pEncIn->gmv[0][1];
  }
  if (pic->sliceInst->type == B_SLICE) {
    asic->regs.gmv[1][0] = pEncIn->gmv[1][0];
    asic->regs.gmv[1][1] = pEncIn->gmv[1][1];
  }
#endif

  /* Check GMV */
  if (asic->regs.asicCfg->gmvSupport) {
    i16 maxX, maxY;
    getGMVRange(&maxX, &maxY, 0, IS_H264(vcenc_instance->codecFormat),
                pic->sliceInst->type == B_SLICE);

    if ((asic->regs.gmv[0][0] > maxX) || (asic->regs.gmv[0][0] < -maxX) ||
        (asic->regs.gmv[0][1] > maxY) || (asic->regs.gmv[0][1] < -maxY) ||
        (asic->regs.gmv[1][0] > maxX) || (asic->regs.gmv[1][0] < -maxX) ||
        (asic->regs.gmv[1][1] > maxY) || (asic->regs.gmv[1][1] < -maxY)) {
      asic->regs.gmv[0][0] = CLIP3(-maxX, maxX, asic->regs.gmv[0][0]);
      asic->regs.gmv[0][1] = CLIP3(-maxY, maxY, asic->regs.gmv[0][1]);
      asic->regs.gmv[1][0] = CLIP3(-maxX, maxX, asic->regs.gmv[1][0]);
      asic->regs.gmv[1][1] = CLIP3(-maxY, maxY, asic->regs.gmv[1][1]);
      APITRACEERR("VCEncStrmEncode: Global MV out of valid range\n");
      APIPRINT(1,
               "VCEncStrmEncode: Clip Global MV to valid range: (%d, %d) for "
               "list0 and (%d, %d) for list1.\n",
               asic->regs.gmv[0][0], asic->regs.gmv[0][1], asic->regs.gmv[1][0],
               asic->regs.gmv[1][1]);
    }

    if (asic->regs.gmv[0][0] || asic->regs.gmv[0][1] || asic->regs.gmv[1][0] ||
        asic->regs.gmv[1][1]) {
      if ((pic->sps->width < 320) ||
          ((pic->sps->width * pic->sps->height) < (320 * 256))) {
        asic->regs.gmv[0][0] = asic->regs.gmv[0][1] = asic->regs.gmv[1][0] =
            asic->regs.gmv[1][1] = 0;
        APITRACEERR(
            "VCEncStrmEncode: Video size is too small to support Global MV, "
            "reset Global MV zero\n");
      }
    }
  }
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeOverlayConfig
    Description   : configure OSD and mosaic parameter
    Return type   : void
    Argument      : asic - asic data
                    pEncIn - input parameters provided by user
                    vcenc_instance - encoder instance
------------------------------------------------------------------------------*/
void StrmEncodeOverlayConfig(asicData_s *asic, VCEncIn *pEncIn,
                             struct vcenc_instance *vcenc_instance) {
  int i;
    /* overlay regs */
  if (pEncIn->osdMapEnable) {
    i= MAX_OVERLAY_NUM - 1;
    int j = MAX_OVERLAY_NUM - 1;
    for (j = MAX_OVERLAY_NUM - 1; j >= 0; j--) {
      if (vcenc_instance->preProcess.overlayEnable[j] ||
      vcenc_instance->preProcess.mosEnable[j]) {
        if (vcenc_instance->preProcess.mosEnable[j]) {
          asic->regs.overlayEnable[i] = (vcenc_instance->pass != 1);
          asic->regs.overlayFormat[i] = 3;
          asic->regs.overlayXoffset[i] = vcenc_instance->preProcess.mosXoffset[j];
          asic->regs.overlayYoffset[i] = vcenc_instance->preProcess.mosYoffset[j];
          asic->regs.overlayWidth[i] = vcenc_instance->preProcess.mosWidth[j];
          asic->regs.overlayHeight[i] = vcenc_instance->preProcess.mosHeight[j];
        } else {
          //Offsets and cropping are handled in software prp, so we don't need to adjust them here
          asic->regs.overlayYAddr[i] = vcenc_instance->preProcess.overlayInputYAddr[j];
        asic->regs.overlayUAddr[i] = vcenc_instance->preProcess.overlayInputUAddr[j];
        asic->regs.overlayVAddr[i] = vcenc_instance->preProcess.overlayInputVAddr[j];
          asic->regs.overlayEnable[i] =
              (vcenc_instance->pass == 1) ? 0 : pEncIn->overlayEnable[j];
          asic->regs.overlayFormat[i] = vcenc_instance->preProcess.overlayFormat[j];
          asic->regs.overlayAlpha[i] = vcenc_instance->preProcess.overlayAlpha[j];
          asic->regs.overlayXoffset[i] = vcenc_instance->preProcess.overlayXoffset[j];
          asic->regs.overlayYoffset[i] = vcenc_instance->preProcess.overlayYoffset[j];
          asic->regs.overlayWidth[i] = vcenc_instance->preProcess.overlayWidth[j];
          asic->regs.overlayHeight[i] = vcenc_instance->preProcess.overlayHeight[j];
          asic->regs.overlayYStride[i] = vcenc_instance->preProcess.overlayYStride[j];
          asic->regs.overlayUVStride[i] =
              vcenc_instance->preProcess.overlayUVStride[j];
          asic->regs.overlayBitmapY[i] = vcenc_instance->preProcess.overlayBitmapY[j];
          asic->regs.overlayBitmapU[i] = vcenc_instance->preProcess.overlayBitmapU[j];
          asic->regs.overlayBitmapV[i] = vcenc_instance->preProcess.overlayBitmapV[j];
        }
        i--;
      }
    }
  } else {
      for (i = 0; i < MAX_OVERLAY_NUM; i++) {
      /* Reuse OSD registers */
      if (vcenc_instance->preProcess.mosEnable[i]) {
        asic->regs.overlayEnable[i] = (vcenc_instance->pass != 1);
        asic->regs.overlayFormat[i] = 3;
        asic->regs.overlayXoffset[i] = vcenc_instance->preProcess.mosXoffset[i];
        asic->regs.overlayYoffset[i] = vcenc_instance->preProcess.mosYoffset[i];
        asic->regs.overlayWidth[i] = vcenc_instance->preProcess.mosWidth[i];
        asic->regs.overlayHeight[i] = vcenc_instance->preProcess.mosHeight[i];
      } else {
        //Offsets and cropping are handled in software prp, so we don't need to adjust them here
        asic->regs.overlayYAddr[i] = vcenc_instance->preProcess.overlayInputYAddr[i];
        asic->regs.overlayUAddr[i] = vcenc_instance->preProcess.overlayInputUAddr[i];
        asic->regs.overlayVAddr[i] = vcenc_instance->preProcess.overlayInputVAddr[i];
        asic->regs.overlayEnable[i] =
            (vcenc_instance->pass == 1) ? 0 : pEncIn->overlayEnable[i];
        asic->regs.overlayFormat[i] = vcenc_instance->preProcess.overlayFormat[i];
        asic->regs.overlayAlpha[i] = vcenc_instance->preProcess.overlayAlpha[i];
        asic->regs.overlayXoffset[i] = vcenc_instance->preProcess.overlayXoffset[i];
        asic->regs.overlayYoffset[i] = vcenc_instance->preProcess.overlayYoffset[i];
        asic->regs.overlayWidth[i] = vcenc_instance->preProcess.overlayWidth[i];
        asic->regs.overlayHeight[i] = vcenc_instance->preProcess.overlayHeight[i];
        asic->regs.overlayYStride[i] = vcenc_instance->preProcess.overlayYStride[i];
        asic->regs.overlayUVStride[i] =
            vcenc_instance->preProcess.overlayUVStride[i];
        asic->regs.overlayBitmapY[i] = vcenc_instance->preProcess.overlayBitmapY[i];
        asic->regs.overlayBitmapU[i] = vcenc_instance->preProcess.overlayBitmapU[i];
        asic->regs.overlayBitmapV[i] = vcenc_instance->preProcess.overlayBitmapV[i];
      }
    }
  }


  if (vcenc_instance->preProcess.overlaySuperTile[0]) {
    //To use 20 bit reg support 4k, times 64 in Cmodel
    asic->regs.overlayYStride[0] =
        vcenc_instance->preProcess.overlayYStride[0] / 64;
    asic->regs.overlayUVStride[0] =
        vcenc_instance->preProcess.overlayUVStride[0] / 64;
  }
  asic->regs.overlaySuperTile = vcenc_instance->preProcess.overlaySuperTile[0];
  asic->regs.overlayScaleWidth =
      vcenc_instance->preProcess.overlayScaleWidth[0];
  asic->regs.overlayScaleHeight =
      vcenc_instance->preProcess.overlayScaleHeight[0];
  if (vcenc_instance->preProcess.overlayScaleWidth[0] != 0) {
    asic->regs.overlayScaleStepW =
         (u16)((double)(vcenc_instance->preProcess.overlayCropWidth[0] << 16) /
              vcenc_instance->preProcess.overlayScaleWidth[0]);
  }
  if (vcenc_instance->preProcess.overlayScaleHeight[0] != 0) {
    asic->regs.overlayScaleStepH =
        (u16)((double)(vcenc_instance->preProcess.overlayCropHeight[0] << 16) /
              vcenc_instance->preProcess.overlayScaleHeight[0]);
  }
}


/*------------------------------------------------------------------------------

    Function name : StrmEncodeOSDMapConfig
    Description   : configure OSDMap parameter, work after overlay cropping
    Return type   : void
    Argument      : asic - asic data
                    pEncIn - input parameters provided by user
                    vcenc_instance - encoder instance
------------------------------------------------------------------------------*/
void StrmEncodeOSDMapConfig(asicData_s *asic, VCEncIn *pEncIn,
                             struct vcenc_instance *vcenc_instance) {
  /* OSD_MAP region(reuse OSD registers)  to  do*/
  if (pEncIn->osdMapEnable && vcenc_instance->pass != 1) {


    if (!asic->regs.overlayEnable[0]) {
      asic->regs.osdMapEnable = pEncIn->osdMapEnable; //osdMap disable if osd region full.
      asic->regs.overlayYAddr[0] = pEncIn->osdMapInputAddr;
      asic->regs.overlayYStride[0] = vcenc_instance->preProcess.osdMapStride; //pixel stride
      asic->regs.osdMapBlockType = vcenc_instance->preProcess.osdMapBlockSize == 4 ? 1 : 0; // block index
      for (int i = 0; i < MAX_OSDMAP_COLOR_NUM; i++) {
        if (asic->regs.overlayEnable[i])
          break;
        /* Reuse OSD small index region registers */
        asic->regs.overlayAlpha[i] = vcenc_instance->preProcess.osdMapAlpha[i];
        asic->regs.overlayBitmapY[i] = vcenc_instance->preProcess.osdMapY[i];
        asic->regs.overlayBitmapU[i] = vcenc_instance->preProcess.osdMapU[i];
        asic->regs.overlayBitmapV[i] = vcenc_instance->preProcess.osdMapV[i];
      }
    }
  }
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodePrefixSei
    Description   : PREFIX sei message
    Return type   : void
    Argument      : vcenc_instance - encoder instance
                    s - sps
                    pEncOut - place where output info is returned
                    pic - picture
                    pEncIn - input parameters provided by user
------------------------------------------------------------------------------*/
void StrmEncodePrefixSei(struct vcenc_instance *vcenc_instance, struct sps *s,
                         VCEncOut *pEncOut, struct sw_picture *pic,
                         VCEncIn *pEncIn) {
  i32 tmp;

  if (pEncIn->bIsIDR &&
      (IS_HEVC(vcenc_instance->codecFormat) ||
       IS_H264(vcenc_instance->codecFormat)) &&
      (vcenc_instance->Hdr10Display.hdr10_display_enable == (u8)HDR10_CFGED ||
       vcenc_instance->Hdr10LightLevel.hdr10_lightlevel_enable ==
           (u8)HDR10_CFGED))
    VCEncEncodeSeiHdr10(vcenc_instance, pEncOut);

  if (IS_HEVC(vcenc_instance->codecFormat)) {
    sei_s *sei = &vcenc_instance->rateControl.sei;

    if (sei->enabled == ENCHW_YES || sei->userDataEnabled == ENCHW_YES ||
        sei->insertRecoveryPointMessage == ENCHW_YES ||
        pEncIn->externalSEICount > 0) {
      if (sei->activated_sps == 0) {
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT,
                       sei->byteStream);
        HevcActiveParameterSetsSei(&vcenc_instance->stream, sei, &s->vui);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf(" activated_sps sei size=%u\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
        sei->activated_sps = 1;
        pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
        pEncOut->PreNaluNum++;
      }
      if (sei->enabled == ENCHW_YES) {
        if ((pic->sliceInst->type == I_SLICE) && (sei->hrd == ENCHW_YES)) {
          tmp = vcenc_instance->stream.byteCnt;
          HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT,
                         sei->byteStream);
          HevcBufferingSei(&vcenc_instance->stream, sei, &s->vui);
          rbsp_trailing_bits(&vcenc_instance->stream);

          sei->nalUnitSize = vcenc_instance->stream.byteCnt;
          printf("BufferingSei sei size=%u\n", vcenc_instance->stream.byteCnt);
          //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
          VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
          pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
          pEncOut->PreNaluNum++;
        }
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT,
                       sei->byteStream);
        HevcPicTimingSei(&vcenc_instance->stream, sei, &s->vui);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("PicTiming sei size=%u\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
        pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
        pEncOut->PreNaluNum++;
      }
      if (sei->userDataEnabled == ENCHW_YES) {
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT,
                       sei->byteStream);
        HevcUserDataUnregSei(&vcenc_instance->stream, sei);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("UserDataUnreg sei size=%u\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
        pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
        pEncOut->PreNaluNum++;
      }
      if (sei->insertRecoveryPointMessage == ENCHW_YES) {
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT,
                       sei->byteStream);
        HevcRecoveryPointSei(&vcenc_instance->stream, sei);
        rbsp_trailing_bits(&vcenc_instance->stream);

        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("RecoveryPoint sei size=%u\n", vcenc_instance->stream.byteCnt);
        //pEncOut->streamSize+=vcenc_instance->stream.byteCnt;
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
        pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
        pEncOut->PreNaluNum++;
      }
      if (pEncIn->externalSEICount > 0 && pEncIn->pExternalSEI != NULL) {
        for (int k = 0; k < pEncIn->externalSEICount; k++) {
          /* Skip only explicit SUFFIX_SEI_NUT */
          if (pEncIn->pExternalSEI[k].nalType == SUFFIX_SEI_NUT) continue;
          tmp = vcenc_instance->stream.byteCnt;
          HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, ENCHW_YES);

          u8 type = pEncIn->pExternalSEI[k].payloadType;
          u8 *content = pEncIn->pExternalSEI[k].pPayloadData;
          u32 size = pEncIn->pExternalSEI[k].payloadDataSize;
          HevcExternalSei(&vcenc_instance->stream, type, content, size);

          rbsp_trailing_bits(&vcenc_instance->stream);
          sei->nalUnitSize = vcenc_instance->stream.byteCnt;
          printf("External sei %d, size=%d\n", k,
                 (i32)vcenc_instance->stream.byteCnt - tmp);
          VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
          pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
          pEncOut->PreNaluNum++;
        }
      }
    }
  } else if (IS_H264(vcenc_instance->codecFormat)) {
    sei_s *sei = &vcenc_instance->rateControl.sei;
    if (sei->enabled == ENCHW_YES || sei->userDataEnabled == ENCHW_YES ||
        sei->insertRecoveryPointMessage == ENCHW_YES ||
        pEncIn->externalSEICount > 0) {
      tmp = vcenc_instance->stream.byteCnt;
      H264NalUnitHdr(&vcenc_instance->stream, 0, H264_SEI, sei->byteStream);
      if (sei->enabled == ENCHW_YES) {
        if ((pic->sliceInst->type == I_SLICE) && (sei->hrd == ENCHW_YES)) {
          H264BufferingSei(&vcenc_instance->stream, sei);
          printf("H264BufferingSei, ");
        }
        H264PicTimingSei(&vcenc_instance->stream, sei);
        printf("PicTiming, ");
      }
      if (sei->userDataEnabled == ENCHW_YES) {
        H264UserDataUnregSei(&vcenc_instance->stream, sei);
        printf("UserDataUnreg, ");
      }
      if (sei->insertRecoveryPointMessage == ENCHW_YES) {
        H264RecoveryPointSei(&vcenc_instance->stream, sei);
        printf("RecoveryPoint, ");
      }
      if (pEncIn->externalSEICount > 0 && pEncIn->pExternalSEI != NULL) {
        for (int k = 0; k < pEncIn->externalSEICount; k++) {
          u8 type = pEncIn->pExternalSEI[k].payloadType;
          u8 *content = pEncIn->pExternalSEI[k].pPayloadData;
          u32 size = pEncIn->pExternalSEI[k].payloadDataSize;
          H264ExternalSei(&vcenc_instance->stream, type, content, size);
          printf("External %d, ", k);
        }
      }
      rbsp_trailing_bits(&vcenc_instance->stream);
      sei->nalUnitSize = vcenc_instance->stream.byteCnt;
      printf("h264 sei total size=%u \n", vcenc_instance->stream.byteCnt);
      VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
      pEncOut->PreDataSize += (vcenc_instance->stream.byteCnt - tmp);
      pEncOut->PreNaluNum++;
    }
  }
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeSuffixSei
    Description   : SUFFIX SEI message
    Return type   : void
    Argument      : vcenc_instance - encoder instance
                    pEncIn - input parameters provided by user
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
void StrmEncodeSuffixSei(struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                         VCEncOut *pEncOut) {
  if (IS_HEVC(vcenc_instance->codecFormat)) {
    u32 tmp;
    sei_s *sei = &vcenc_instance->rateControl.sei;
    if (pEncIn->externalSEICount > 0 && pEncIn->pExternalSEI != NULL) {
      for (int k = 0; k < pEncIn->externalSEICount; k++) {
        if (pEncIn->pExternalSEI[k].nalType != SUFFIX_SEI_NUT) continue;
        u8 type = pEncIn->pExternalSEI[k].payloadType;
        u8 *content = pEncIn->pExternalSEI[k].pPayloadData;
        u32 size = pEncIn->pExternalSEI[k].payloadDataSize;
        if (!(type == 3 || type == 4 || type == 5 || type == 17 || type == 22 ||
              type == 132 || type == 146)) {
          printf("Payload type %d not allowed at SUFFIX_SEI_NUT\n", type);
          ASSERT(0);
        }
        tmp = vcenc_instance->stream.byteCnt;
        HevcNalUnitHdr(&vcenc_instance->stream, SUFFIX_SEI_NUT, ENCHW_YES);
        HevcExternalSei(&vcenc_instance->stream, type, content, size);
        rbsp_trailing_bits(&vcenc_instance->stream);
        sei->nalUnitSize = vcenc_instance->stream.byteCnt;
        printf("External sei %d, size=%u\n", k,
               vcenc_instance->stream.byteCnt - tmp);
        VCEncAddNaluSize(pEncOut, vcenc_instance->stream.byteCnt - tmp, 0);
        pEncOut->PostDataSize += (vcenc_instance->stream.byteCnt - tmp);
      }
    }
  }
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeGradualDecoderRefresh
    Description   : configure GDR parameters
    Return type   : void
    Argument      : vcenc_instance - encoder instance
                    asic - asic data
                    pEncIn - input parameters provided by user
                    codingType - picture coding type
                    cfg - ewl HW config
------------------------------------------------------------------------------*/
void StrmEncodeGradualDecoderRefresh(struct vcenc_instance *vcenc_instance,
                                     asicData_s *asic, VCEncIn *pEncIn,
                                     VCEncPictureCodingType *codingType,
                                     const EWLHwConfig_t *cfg) {
  i32 top_pos, bottom_pos;
  i32 overlap_rows;

  if ((vcenc_instance->gdrEnabled == 1) &&
      (vcenc_instance->encStatus == VCENCSTAT_START_FRAME) &&
      (vcenc_instance->gdrFirstIntraFrame == 0)) {
    asic->regs.intraAreaTop = asic->regs.intraAreaLeft =
        asic->regs.intraAreaBottom = asic->regs.intraAreaRight = INVALID_POS;
    asic->regs.roi1Top = asic->regs.roi1Left = asic->regs.roi1Bottom =
        asic->regs.roi1Right = INVALID_POS;
    //asic->regs.roi1DeltaQp = 0;
    asic->regs.roi1Qp = -1;
    if (pEncIn->codingType == VCENC_INTRA_FRAME) {
      //vcenc_instance->gdrStart++ ;
      *codingType = VCENC_PREDICTED_FRAME;
    }
    if (vcenc_instance->gdrStart) {
      if (vcenc_instance->gdrCount == 0)
        vcenc_instance->rateControl.sei.insertRecoveryPointMessage = ENCHW_YES;
      else
        vcenc_instance->rateControl.sei.insertRecoveryPointMessage = ENCHW_NO;
      top_pos = (vcenc_instance->gdrCount / (1 + vcenc_instance->interlaced)) *
                vcenc_instance->gdrAverageMBRows;
      bottom_pos = 0;
      if (vcenc_instance->gdrMBLeft) {
        if ((vcenc_instance->gdrCount / (1 + (i32)vcenc_instance->interlaced)) <
            vcenc_instance->gdrMBLeft) {
          top_pos += (vcenc_instance->gdrCount /
                      (1 + (i32)vcenc_instance->interlaced));
          bottom_pos += 1;
        } else {
          top_pos += vcenc_instance->gdrMBLeft;
        }
      }

      /* We need overlap rows between two GDR frame to make sure inter prediction
         vertical search range is within region which was refreshed by intra area. */
      if (IS_H264(vcenc_instance->codecFormat)) {
        overlap_rows = (cfg->meVertSearchRangeH264*8 + 15) / 16;
      } else {
        overlap_rows = (cfg->meVertSearchRangeHEVC*8 + 63) / 64;
      }
      ASSERT(overlap_rows>=1);

      bottom_pos += top_pos + vcenc_instance->gdrAverageMBRows + overlap_rows-1;
      if (bottom_pos > ((i32)vcenc_instance->ctbPerCol - 1)) {
        bottom_pos = vcenc_instance->ctbPerCol - 1;
      }

      VCEncCheckMsg(vcenc_instance, VCENC_LOG_DEBUG, VCENC_LOG_CHECK_FEATURE,
          "[GDR] Pic%d, MEV=%d, Intra from %d to %d\n", pEncIn->picture_cnt,
          (IS_H264(vcenc_instance->codecFormat)?cfg->meVertSearchRangeH264*8:
            cfg->meVertSearchRangeHEVC*8), top_pos, bottom_pos);

      asic->regs.intraAreaTop = top_pos;
      asic->regs.intraAreaLeft = 0;
      asic->regs.intraAreaBottom = bottom_pos;
      asic->regs.intraAreaRight = vcenc_instance->ctbPerRow - 1;

      //to make video quality in intra area is close to inter area.
      asic->regs.roi1Top = top_pos;
      asic->regs.roi1Left = 0;
      asic->regs.roi1Bottom = bottom_pos;
      asic->regs.roi1Right = vcenc_instance->ctbPerRow - 1;

      /*
          roi1DeltaQp from user setting, or if user not provide roi1DeltaQp, use default roi1DeltaQp=3
      */
      if (!asic->regs.roi1DeltaQp) asic->regs.roi1DeltaQp = 3;

      asic->regs.rcRoiEnable = 0x01;
    }
    asic->regs.roiUpdate = 1; /* ROI has changed from previous frame. */
  }
}

/*------------------------------------------------------------------------------

    Function name : StrmEncodeRegionOfInterest
    Description   : configure ROI parameters
    Return type   : void
    Argument      : vcenc_instance - encoder instance
                    asic - asic data
------------------------------------------------------------------------------*/
void StrmEncodeRegionOfInterest(struct vcenc_instance *vcenc_instance,
                                asicData_s *asic) {
  asic->regs.offsetSliceQp = 0;
  if (asic->regs.qp >= 35) {
    asic->regs.offsetSliceQp = 35 - asic->regs.qp;
  }
  if (asic->regs.qp <= 15) {
    asic->regs.offsetSliceQp = 15 - asic->regs.qp;
  }

  if ((vcenc_instance->asic.regs.rcRoiEnable & 0x0c) == 0) {
    if (vcenc_instance->asic.regs.rcRoiEnable & 0x3) {
      if (asic->regs.asicCfg->roiAbsQpSupport) {
        i32 minDelta = (i32)asic->regs.qp - 51;

        asic->regs.roi1DeltaQp =
            CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi1DeltaQp);
        asic->regs.roi2DeltaQp =
            CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi2DeltaQp);

        if (asic->regs.roi1Qp >= 0)
          asic->regs.roi1Qp = CLIP3((i32)asic->regs.qpMin,
                                    (i32)asic->regs.qpMax, asic->regs.roi1Qp);

        if (asic->regs.roi2Qp >= 0)
          asic->regs.roi2Qp = CLIP3((i32)asic->regs.qpMin,
                                    (i32)asic->regs.qpMax, asic->regs.roi2Qp);

        if (asic->regs.asicCfg->ROI8Support) {
          asic->regs.roi3DeltaQp =
              CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi3DeltaQp);
          asic->regs.roi4DeltaQp =
              CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi4DeltaQp);
          asic->regs.roi5DeltaQp =
              CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi5DeltaQp);
          asic->regs.roi6DeltaQp =
              CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi6DeltaQp);
          asic->regs.roi7DeltaQp =
              CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi7DeltaQp);
          asic->regs.roi8DeltaQp =
              CLIP3(minDelta, (i32)asic->regs.qp, asic->regs.roi8DeltaQp);

          if (asic->regs.roi3Qp >= 0)
            asic->regs.roi3Qp = CLIP3((i32)asic->regs.qpMin,
                                      (i32)asic->regs.qpMax, asic->regs.roi3Qp);

          if (asic->regs.roi4Qp >= 0)
            asic->regs.roi4Qp = CLIP3((i32)asic->regs.qpMin,
                                      (i32)asic->regs.qpMax, asic->regs.roi4Qp);

          if (asic->regs.roi5Qp >= 0)
            asic->regs.roi5Qp = CLIP3((i32)asic->regs.qpMin,
                                      (i32)asic->regs.qpMax, asic->regs.roi5Qp);

          if (asic->regs.roi6Qp >= 0)
            asic->regs.roi6Qp = CLIP3((i32)asic->regs.qpMin,
                                      (i32)asic->regs.qpMax, asic->regs.roi6Qp);

          if (asic->regs.roi7Qp >= 0)
            asic->regs.roi7Qp = CLIP3((i32)asic->regs.qpMin,
                                      (i32)asic->regs.qpMax, asic->regs.roi7Qp);

          if (asic->regs.roi8Qp >= 0)
            asic->regs.roi8Qp = CLIP3((i32)asic->regs.qpMin,
                                      (i32)asic->regs.qpMax, asic->regs.roi8Qp);
        }
      } else {
        asic->regs.roi1DeltaQp =
            CLIP3(0, 15 - asic->regs.offsetSliceQp, asic->regs.roi1DeltaQp);
        asic->regs.roi2DeltaQp =
            CLIP3(0, 15 - asic->regs.offsetSliceQp, asic->regs.roi2DeltaQp);
      }

      if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
          asic->regs.roi1DeltaQp) {
        asic->regs.roi1DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
      }

      if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
          asic->regs.roi2DeltaQp) {
        asic->regs.roi2DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
      }

      if (asic->regs.asicCfg->ROI8Support) {
        if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
            asic->regs.roi3DeltaQp) {
          asic->regs.roi3DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
        }

        if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
            asic->regs.roi4DeltaQp) {
          asic->regs.roi4DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
        }

        if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
            asic->regs.roi5DeltaQp) {
          asic->regs.roi5DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
        }

        if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
            asic->regs.roi6DeltaQp) {
          asic->regs.roi6DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
        }

        if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
            asic->regs.roi7DeltaQp) {
          asic->regs.roi7DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
        }

        if (((i32)asic->regs.qp - (i32)asic->regs.qpMin) <
            asic->regs.roi8DeltaQp) {
          asic->regs.roi8DeltaQp = (i32)asic->regs.qp - (i32)asic->regs.qpMin;
        }
      }
    }
  }
}

/*------------------------------------------------------------------------------

    Function name : EncGetSSIM
    Description   : Calculate SSIM
    Return type   : VCEncRet
    Argument      : vcenc_instance - encoder instance
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
VCEncRet EncGetSSIM(struct vcenc_instance *inst, VCEncOut *pEncOut) {
  if ((inst == NULL) || (pEncOut == NULL)) return VCENC_ERROR;

  pEncOut->ssim[0] = pEncOut->ssim[1] = pEncOut->ssim[2] = 0.0;

  asicData_s *asic = &inst->asic;
  if ((!asic->regs.asicCfg->ssimSupport) || (!asic->regs.ssim))
    return VCENC_ERROR;

  i64 ssim_numerator_y = (i32)EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_NUMERATOR_MSB);
  i64 ssim_numerator_u = (i32)EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_U_NUMERATOR_MSB);
  i64 ssim_numerator_v = (i32)EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_V_NUMERATOR_MSB);
  u32 ssim_denominator_y = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_DENOMINATOR);
  u32 ssim_denominator_uv = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_UV_DENOMINATOR);

  ssim_numerator_y = ssim_numerator_y << 32;
  ssim_numerator_u = ssim_numerator_u << 32;
  ssim_numerator_v = ssim_numerator_v << 32;
  ssim_numerator_y |= (i64)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                              HWIF_ENC_SSIM_Y_NUMERATOR_LSB);
  ssim_numerator_u |= (i64)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                              HWIF_ENC_SSIM_U_NUMERATOR_LSB);
  ssim_numerator_v |= (i64)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                              HWIF_ENC_SSIM_V_NUMERATOR_LSB);

  CalculateSSIM(inst, pEncOut, ssim_numerator_y, ssim_numerator_u,
                ssim_numerator_v, ssim_denominator_y, ssim_denominator_uv);

  return VCENC_OK;
}

void CalculateSSIM(struct vcenc_instance *inst, VCEncOut *pEncOut,
                   i64 ssim_numerator_y, i64 ssim_numerator_u,
                   i64 ssim_numerator_v, u32 ssim_denominator_y,
                   u32 ssim_denominator_uv) {
  const i32 shift_y = (inst->sps->bit_depth_luma_minus8 == 0)
                          ? SSIM_FIX_POINT_FOR_8BIT
                          : SSIM_FIX_POINT_FOR_10BIT;
  const i32 shift_uv = (inst->sps->bit_depth_chroma_minus8 == 0)
                           ? SSIM_FIX_POINT_FOR_8BIT
                           : SSIM_FIX_POINT_FOR_10BIT;
  if (ssim_denominator_y)
    pEncOut->ssim[0] =
        ssim_numerator_y * 1.0 / (1 << shift_y) / ssim_denominator_y;

  if (ssim_denominator_uv) {
    pEncOut->ssim[1] =
        ssim_numerator_u * 1.0 / (1 << shift_uv) / ssim_denominator_uv;
    pEncOut->ssim[2] =
        ssim_numerator_v * 1.0 / (1 << shift_uv) / ssim_denominator_uv;
  }

#if 0
  double ssim = pEncOut->ssim[0] * 0.8 + 0.1 * (pEncOut->ssim[1] + pEncOut->ssim[2]);
  printf("    SSIM %.4f SSIM Y %.4f U %.4f V %.4f\n", ssim, pEncOut->ssim[0], pEncOut->ssim[1], pEncOut->ssim[2]);
#endif
}

/*------------------------------------------------------------------------------

    Function name : EncGetPSNR
    Description   : operations about PSNR under several conditions
    Return type   : VCEncRet
    Argument      : vcenc_instance - encoder instance
                    pEncOut - place where output info is returned
------------------------------------------------------------------------------*/
VCEncRet EncGetPSNR(struct vcenc_instance *inst, VCEncOut *pEncOut) {
  if ((inst == NULL) || (pEncOut == NULL)) return VCENC_ERROR;

  asicData_s *asic = &inst->asic;

  /* calculate PSNR_Y,
  HW calculate SSE, SW calculate log10f
  In HW, 8 bottom lines in every 64 height unit is skipped to save line buffer
  SW log10f operation will waste too much CPU cycles,
  we suggest not use it, comment it out by default*/
  asic->regs.SSEDivide256 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSE_DIV_256);
  {
    //pEncOut->psnr_y = (asic->regs.SSEDivide256 == 0)? 999999.0 :
    //                  10.0 * log10f((float)((256 << asic->regs.outputBitWidthLuma) - 1) * ((256 << asic->regs.outputBitWidthLuma) - 1) * asic->regs.picWidth * (asic->regs.picHeight - ((asic->regs.picHeight>>6)<<3))/ (float)(asic->regs.SSEDivide256 * ((256 << asic->regs.outputBitWidthLuma) << asic->regs.outputBitWidthLuma)));
  }

  /*PSNR for DJI*/
  asic->regs.lumSSEDivide256 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_LUM_SSE_DIV_256);
  asic->regs.cbSSEDivide64 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_CB_SSE_DIV_64);
  asic->regs.crSSEDivide64 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_CR_SSE_DIV_64);

  CalculatePSNR(inst, pEncOut, inst->width);

  return VCENC_OK;
}

void CalculatePSNR(struct vcenc_instance *inst, VCEncOut *pEncOut, u32 width) {
  asicData_s *asic = &inst->asic;

#ifdef DJI
  pEncOut->psnr_y_predeb =
      (asic->regs.lumSSEDivide256 == 0)
          ? 999999.0
          : 10.0 * log10f((float)((256 << asic->regs.outputBitWidthLuma) - 1) *
                          ((256 << asic->regs.outputBitWidthLuma) - 1) *
                          asic->regs.picWidth * asic->regs.picHeight /
                          (float)(asic->regs.lumSSEDivide256 *
                                  ((256 << asic->regs.outputBitWidthLuma)
                                   << asic->regs.outputBitWidthLuma)));
  pEncOut->psnr_cb_predeb =
      (asic->regs.cbSSEDivide64 == 0)
          ? 999999.0
          : 10.0 *
                log10f((float)((256 << asic->regs.outputBitWidthChroma) - 1) *
                       ((256 << asic->regs.outputBitWidthChroma) - 1) *
                       (asic->regs.picWidth / 2) * (asic->regs.picHeight / 2) /
                       (float)(asic->regs.cbSSEDivide64 *
                               ((64 << asic->regs.outputBitWidthChroma)
                                << asic->regs.outputBitWidthChroma)));
  pEncOut->psnr_cr_predeb =
      (asic->regs.crSSEDivide64 == 0)
          ? 999999.0
          : 10.0 *
                log10f((float)((256 << asic->regs.outputBitWidthChroma) - 1) *
                       ((256 << asic->regs.outputBitWidthChroma) - 1) *
                       (asic->regs.picWidth / 2) * (asic->regs.picHeight / 2) /
                       (float)(asic->regs.crSSEDivide64 *
                               ((64 << asic->regs.outputBitWidthChroma)
                                << asic->regs.outputBitWidthChroma)));
#endif

  if (asic->regs.asicCfg->psnrSupport && asic->regs.psnr) {
    inst->rateControl
      .hierarchial_sse[inst->rateControl.hierarchial_bit_allocation_GOP_size -
                       1][inst->rateControl.gopPoc] = asic->regs.lumSSEDivide256;
  } else if (asic->regs.asicCfg->reconSSEsupport && inst->codecFormat == VCENC_VIDEO_CODEC_HEVC) {
    inst->rateControl
      .hierarchial_sse[inst->rateControl.hierarchial_bit_allocation_GOP_size -
                       1][inst->rateControl.gopPoc] = asic->regs.SSEDivide256;
  }

  /*calculate accurate PSNR*/
  if (asic->regs.asicCfg->psnrSupport && asic->regs.psnr) {
    pEncOut->psnr[0] = pEncOut->psnr[1] = pEncOut->psnr[2] = 0.0;

    long long lum_sse, cb_sse, cr_sse;
    lum_sse = ((u32)asic->regs.lumSSEDivide256)
              << 8 << inst->sps->bit_depth_luma_minus8
              << inst->sps->bit_depth_luma_minus8;
    cb_sse = ((u32)asic->regs.cbSSEDivide64)
             << 6 << inst->sps->bit_depth_chroma_minus8
             << inst->sps->bit_depth_chroma_minus8;
    cr_sse = ((u32)asic->regs.crSSEDivide64)
             << 6 << inst->sps->bit_depth_chroma_minus8
             << inst->sps->bit_depth_chroma_minus8;

    float lum_mse, cb_mse, cr_mse;
    lum_mse = (float)lum_sse / (float)(width * inst->height);
    cb_mse = ((float)cb_sse / (float)(width * inst->height)) * 4;
    cr_mse = ((float)cr_sse / (float)(width * inst->height)) * 4;

    int lum_max_value, cbcr_max_value;
    lum_max_value = (1 << inst->sps->bit_depth_luma_minus8 << 8) - 1;
    cbcr_max_value = (1 << inst->sps->bit_depth_chroma_minus8 << 8) - 1;

    if (lum_mse == 0.0)
      pEncOut->psnr[0] = 999999.0;
    else
      pEncOut->psnr[0] = 10.0 * log10f(lum_max_value * lum_max_value / lum_mse);

    if (cb_mse == 0.0)
      pEncOut->psnr[1] = 999999.0;
    else
      pEncOut->psnr[1] =
          10.0 * log10f(cbcr_max_value * cbcr_max_value / cb_mse);

    if (cr_mse == 0.0)
      pEncOut->psnr[2] = 999999.0;
    else
      pEncOut->psnr[2] =
          10.0 * log10f(cbcr_max_value * cbcr_max_value / cr_mse);
  }
}

VCEncRet GenralRefPicMarking(struct vcenc_instance *vcenc_instance,
                             struct container *c, struct rps *r,
                             VCEncPictureCodingType codingType, VCEncIn *pEncIn) {
  VCEncRet ret = VCENC_OK;

  /* For VP9 TMVP, we may need to store a pic which is not for referencing
     Since VP9 TMVP use last coded frame but not reference frame */
  i32 savePrePoc = -1;
#ifdef SUPPORT_VP9
  if (IS_VP9(vcenc_instance->codecFormat)) {
    if (vcenc_instance->enableTMVP && (codingType != VCENC_INTRA_FRAME)) {
      savePrePoc = vcenc_instance->vp9_inst.previousPoc;
    }
  }
#endif

  if (ref_pic_marking(c, r, savePrePoc, vcenc_instance, pEncIn)) ret = VCENC_ERROR;

  return ret;
}

/**
 * Flush reference list
 *
 * \param [in] useful_poc keep the picture with this poc
 * \return VCENC_OK done successfully
 */
VCEncRet IdrRefPicFlush(struct vcenc_instance *vcenc_instance,
                        struct container *c, i32 useful_poc, VCEncIn *pEncIn)
{
  struct rps r;

  r.before_cnt = r.after_cnt = r.follow_cnt = 0;
  r.lt_current_cnt = r.lt_follow_cnt = 0;

  ref_pic_marking(c, &r, useful_poc, vcenc_instance, pEncIn);

  return VCENC_OK;
}

void VCEncVisualStrengthUpdate(struct vcenc_instance *vcenc_instance,
                        const VCEncIn *pEncIn)
{
  if (vcenc_instance->visualBitRateTolerance < 0) return;
  vcencRateControl_s *rc = &vcenc_instance->rateControl;
  double bpLimit = 1.0 + (double)vcenc_instance->visualBitRateTolerance / 100;
  i32 prevPredId = rc->sliceTypePrev;
  i32 actualBits = rc->rcPred[prevPredId].prevFrameBits;
  i32 targetBits = rc->rcPred[prevPredId].prevTargetBits;
  i32 qp = rc->rcPred[prevPredId].qp >> QP_FRACTIONAL_BITS;
  i32 actualBitRate =  vcenc_instance->currentActualBitRate;
  i32 targetBitRate = rc->virtualBuffer.bitRate;
  asicData_s *asic = &vcenc_instance->asic;

  /* intra bias strength frame level adaptive adjustment */
  if (rc->picRc == ENCHW_YES && vcenc_instance->IntraBiasStrengthMax) {
    double ratio1 = MAX(bpLimit * 0.9, 1.0);
    double ratio2 = 0.1;
    double ratio3 = 0.2;

    if (((qp  == 51 && actualBitRate > targetBitRate*ratio1
            && asic->regs.intraBiasExtraBits > actualBits * ratio2)
         || (asic->regs.intraBiasExtraBits > actualBits * ratio3))
        && vcenc_instance->IntraBiasStrength > 1)
    {
      vcenc_instance->IntraBiasStrength --;
    }
    else if (vcenc_instance->IntraBiasStrength < vcenc_instance->IntraBiasStrengthMax
      && actualBitRate < targetBitRate*ratio1)
    {
      vcenc_instance->IntraBiasStrength ++;
    }
#if 0
    printf ("====== sliceType[%d] qp[%d] actualBitRate[%d] targetBitRate[%d] actualBits[%d] targetBits[%d] intraExtraBits[%d] AB/TB[%f] A/T[%f] I/A[%f]\n",
       prevPredId, qp, actualBitRate, targetBitRate, actualBits, targetBits,
       asic->regs.intraBiasExtraBits, actualBitRate*1.0/targetBitRate,
       actualBits*1.0/targetBits, asic->regs.intraBiasExtraBits*1.0/actualBits);
    printf ("====== poc[%d], intraBiasStrength[%d]\n",
       vcenc_instance->poc, vcenc_instance->IntraBiasStrength);
#endif
  }

  // Trail strength frame level adaptive adjustment
  if (vcenc_instance->ctbRcTrailStrengthMax) {
    u32 totalUnit16 = STRIDE(vcenc_instance->width, 16) * STRIDE(vcenc_instance->height, 16) >> 8;
    double trailPercent = (double)asic->regs.trailAreaUnit16 * 100 / totalUnit16;
    // Last frame trail detected, check its influence
    if (rc->picRc == ENCHW_YES && asic->regs.trailDetectEnable) {
      // Check whether need to reduce strength
      u8 strengthReduce = 0;
      if (vcenc_instance->ctbRcTrailStrength > 1) {
        //Too large trail area
        strengthReduce = trailPercent >= (20 - 5 *(vcenc_instance->ctbRcTrailStrength <= 2));
        // Maximum QP with trail area and exceeding target bitrate
        strengthReduce |= trailPercent > 10 && qp  == 51 &&
                          actualBitRate > targetBitRate*bpLimit && actualBits > targetBits;
        vcenc_instance->ctbRcTrailStrength -= strengthReduce;
      }

      // Check whether need to increase strength
      if (!strengthReduce &&
          vcenc_instance->ctbRcTrailStrength < vcenc_instance->ctbRcTrailStrengthMax)
      {
        // Small QP with some trail area and bitrate is less than target
        u8 strengthInc = trailPercent < 10 && qp < 45 && actualBitRate < targetBitRate*bpLimit;
        // Extreamly little trail area and bitrate is less than target
        strengthInc |= trailPercent < 5 && actualBitRate < targetBitRate*bpLimit;
        vcenc_instance->ctbRcTrailStrength += strengthReduce;
      }
      //Update to previous strength
      vcenc_instance->ctbRcTrailStrengthPrev = vcenc_instance->ctbRcTrailStrength;
    }

    // Check camera moving regardless of last frame trail detection
    {
      i32 width8 = STRIDE(vcenc_instance->width, 8) / 8;
      i32 height8 = STRIDE(vcenc_instance->height, 8) / 8;
      double intraPercent = (double)asic->regs.intraCu8Num / (double)(width8 * height8);
      double interPercent = 1.0 - intraPercent;
      double skipPercent = (double)asic->regs.skipCu8Num / (double)(width8* height8);
      double movingPercent = (double)asic->regs.movingCu8Num/ (double)(width8 * height8);
      //Turn off trailing reduce if camera move
      if (movingPercent > 0.4 || (movingPercent > 0.2 && intraPercent > 0.2)) {
        vcenc_instance->ctbRcTrailStrength = 0;
      } else {
        vcenc_instance->ctbRcTrailStrength = vcenc_instance->ctbRcTrailStrengthPrev;
      }
      #if 0
        printf ("Trail Update POC %d trailDetectStrength[%d] trailArea percent %0.2f;",
            vcenc_instance->poc, vcenc_instance->ctbRcTrailStrength, trailPercent/100);
        printf(" Intra percent %0.2f; Inter percent %0.2f; Skip percent %0.2f; Moving percent %0.2f\n",
          intraPercent, interPercent, skipPercent, movingPercent);
      #endif
    }
  }
}

VCEncRet VCEncVisualGenConfig(struct vcenc_instance *vcenc_instance, struct sw_picture *pic, VCEncIn *pEncIn) {
  asicData_s *asic = &vcenc_instance->asic;
  asic->regs.ctbRcSkinQPDelta = (vcenc_instance->ctbRcMode & 1) ? vcenc_instance->ctbRcSkinQPDelta : 0;
  asic->regs.ctbRcSkinMinQPDelta = vcenc_instance->ctbRcSkinMinQPDelta;
  asic->regs.ctbRcSkinCbMin = vcenc_instance->ctbRcSkinCbMin;
  asic->regs.ctbRcSkinCbMax = vcenc_instance->ctbRcSkinCbMax;
  asic->regs.ctbRcSkinCrMin = vcenc_instance->ctbRcSkinCrMin;
  asic->regs.ctbRcSkinCrMax = vcenc_instance->ctbRcSkinCrMax;
  asic->regs.ctbRcSkinLumMin = vcenc_instance->ctbRcSkinLumMin;
  asic->regs.ctbRcSkinLumMax = vcenc_instance->ctbRcSkinLumMax;
  asic->regs.ctbRcSkinGradTh = vcenc_instance->ctbRcSkinGradTh;
  asic->regs.ctbRcDirection = vcenc_instance->ctbRcDirection;
  asic->regs.ctbRcEdgeTh = vcenc_instance->ctbRcEdgeTh;
  //which decrease direction means edge area's max qpDelta
  asic->regs.ctbRcEdgeMaxQpDelta = vcenc_instance->ctbRcEdgeMaxQpDelta;
  /* I/P/B frame could own diff complexity table */
  for (int i = 0; i < 16; i++){
    asic->regs.ctbRcThreshold[i] = vcenc_instance->ctbRcThresholdCurFrame[i];
  }

  /* Set trail detection related registers
     Including TMVP registers, only when trail strength max is set */
  if (vcenc_instance->ctbRcTrailStrengthMax) {
    if (pic->sliceInst->type != I_SLICE && pic->rpl[0][0]->sliceInst->type != I_SLICE) {
      asic->regs.trailDetectEnable = vcenc_instance->ctbRcTrailStrength > 0;
      asic->regs.tmvpRefMvInfoBaseL0 = pic->rpl[0][0]->mvInfoBase;
      asic->regs.colFrameFromL0 = 1;
    } else {
      asic->regs.trailDetectEnable = 0;
    }
    asic->regs.writeTMVinfoDDR = pic->sliceInst->type != I_SLICE;
    asic->regs.tmvpMvInfoBase = (ptr_t)pic->mvInfoBase;
    asic->regs.trailDeltaQp = vcenc_instance->ctbRcTrailDeltaQp;
    asic->regs.trailDetectPrevMvTh = 5 + 5 * (3 - vcenc_instance->ctbRcTrailStrength);
    asic->regs.trailDetectCurMvTh = (vcenc_instance->ctbRcTrailStrength >= 2)? 1 : 0;
    //Only detect CU size >= trailDetectMinCuLogSize, = 3 + trailDetectMinCuLogSize
    asic->regs.trailDetectMinCuLogSize = (vcenc_instance->ctbRcTrailStrength <= 2)? 1 : 0;
    asic->regs.trailSplitThCU16 = 0.9 * (1 << SUBJECT_THRESH_FIX_POINT);
    asic->regs.trailSplitThCU32 = 0.8 * (1 << SUBJECT_THRESH_FIX_POINT);
    //Currently only apply small lambda in trail area for H264
    asic->regs.trailUseLambdaMinQp = (IS_H264(vcenc_instance->codecFormat)? 45 : 51) - 20;
    double newQpFactor = 0.3;
    asic->regs.trailQpFactorSAD = (u32)((newQpFactor * (1 << QPFACTOR_FIX_POINT)) + 0.5);
    asic->regs.trailQpFactorSSE = (u32)((newQpFactor * newQpFactor * (1 << QPFACTOR_FIX_POINT)) + 0.5);
    asic->regs.trailSaoChrMaxQp = 30;
    asic->regs.trailSaoNumCu16Ctb = (vcenc_instance->ctbRcTrailStrength >= 2)? 1 : 0;
  }

  /* Set Mode subject prefer registers */
  const i32 IntraBiasRatio1[VISUAL_INTRA_BIAS_MAX_STRENGTH] =
  {64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 67, 70, 74};
  const i32 IntraBiasRatio2[VISUAL_INTRA_BIAS_MAX_STRENGTH] =
  {13, 19, 26, 32, 38, 45, 48, 51, 54, 58, 61, 64, 64, 64, 64};
  const i32 ChromaDetectionThreshold[VISUAL_CHROMA_DETECTION_MAX_STRENGTH] =
  {100, 90, 80, 70, 60, 50, 40, 30, 20, 10};
  if (asic->regs.asicCfg->modSubjPrefer || 0x2168 == asic->regs.asicCfg->hw_build_id)
  {
    bool depth0 = HANTRO_TRUE;
    if (pic->sliceInst->type != I_SLICE)
      depth0 = (pEncIn->gopCurrPicConfig.poc % pEncIn->gopSize) ? HANTRO_FALSE
                                                              : HANTRO_TRUE;
    /* init mode subject prefer register */
    asic->regs.iBiasChromaMseThr = 0;
    asic->regs.iBiasCostRatioThr = 0;
    asic->regs.iBiasDistRatioThr1 = 0;
    asic->regs.iBiasDistRatioThr2 = 0;
    asic->regs.IntraBiasMvThreshold = 0;
    asic->regs.trailAvoidIntraBias = 0;
    asic->regs.disableIntraDistRatioThr = 0;

    if (asic->regs.asicCfg->modSubjPrefer)
    {
      if (vcenc_instance->pass != 1 && depth0 == HANTRO_TRUE) // I/P Slice
      {
        //H264 skip candidate distortion threshold
        asic->regs.skipBinCostMaxDistTh = 1 << (SUBJECT_THRESH_FIX_POINT - 1);
        /*reduce chroma mode bin cost to get better chr mode*/
        /*intraChrBincostRatio = 0~64, represent ratio 0~1*/
        if (IS_H264(vcenc_instance->codecFormat) && !asic->regs.intraReconEnable)
        {
          //for h264 chr bin cost reduction only be support when intraRecon enabled
          asic->regs.intraChrBincostRatio = 1 << SUBJECT_THRESH_FIX_POINT;
        }
        else
          asic->regs.intraChrBincostRatio = 1 << (SUBJECT_THRESH_FIX_POINT - 1); //used in h264 and hevc
      }
      else {
        asic->regs.skipBinCostMaxDistTh = 1 << SUBJECT_THRESH_FIX_POINT;
        asic->regs.intraChrBincostRatio = 1 << SUBJECT_THRESH_FIX_POINT;
      }
    }

    if (vcenc_instance->pass != 1 &&
      (pic->sliceInst->type != I_SLICE && depth0 == HANTRO_TRUE)) // P Slice
    {
      /* mode decision related registers */
      if (vcenc_instance->IntraBiasChromaStrength)
      {
        u32 chrCheckIdx = MIN( vcenc_instance->IntraBiasChromaStrength - 1, VISUAL_CHROMA_DETECTION_MAX_STRENGTH - 1 );
        asic->regs.iBiasChromaMseThr = ChromaDetectionThreshold[chrCheckIdx];
      }

      if (vcenc_instance->IntraBiasStrength)
      {
        u32 iBiasIdx = MIN( vcenc_instance->IntraBiasStrength - 1, VISUAL_INTRA_BIAS_MAX_STRENGTH - 1 );
        asic->regs.iBiasCostRatioThr = IntraBiasRatio1[iBiasIdx];
        asic->regs.iBiasDistRatioThr1 = IntraBiasRatio2[iBiasIdx];
        asic->regs.iBiasDistRatioThr2 = IntraBiasRatio2[0];
        asic->regs.IntraBiasMvThreshold = vcenc_instance->IntraBiasMvThreshold;
        asic->regs.trailAvoidIntraBias = vcenc_instance->bTrailAvoidIntraBias;
      }
      asic->regs.disableIntraDistRatioThr = 77; /*represent ratio 77/64*/
    }
  }
  asic->regs.movingCu8Thresh = 1;

  return VCENC_OK;
}

VCEncRet TemporalMvpGenConfig(struct vcenc_instance *vcenc_instance,
                              VCEncIn *pEncIn, struct container *c,
                              struct sw_picture *pic,
                              VCEncPictureCodingType codingType) {
  i32 i;
  asicData_s *asic = &vcenc_instance->asic;
  asic->regs.spsTmvpEnable = vcenc_instance->enableTMVP;

  u8 earlyExit = (!vcenc_instance->enableTMVP || IS_H264(vcenc_instance->codecFormat));
  if (earlyExit) {
    asic->regs.tmvpMvInfoBase = 0;
    asic->regs.sliceTmvpEnable = 0;
    asic->regs.tmvpRefMvInfoBaseL0 = 0;
    asic->regs.tmvpRefMvInfoBaseL1 = 0;
    asic->regs.writeTMVinfoDDR = 0;
    return VCENC_OK;
  }

  asic->regs.tmvpMvInfoBase = (ptr_t)pic->mvInfoBase;
  asic->regs.sliceTmvpEnable = (codingType != VCENC_INTRA_FRAME) &&
                               (!IS_H264(vcenc_instance->codecFormat)) &&
                               vcenc_instance->enableTMVP;
  asic->regs.writeTMVinfoDDR = codingType != VCENC_INTRA_FRAME;

  if (pic->sliceInst->type != I_SLICE) {
    pic->deltaPocL0[0] = pic->rpl[0][0]->poc - pic->poc;
    if (pic->sliceInst->active_l0_cnt > 1)
      pic->deltaPocL0[1] = pic->rpl[0][1]->poc - pic->poc;
    else
      pic->deltaPocL0[1] = 0;
    asic->regs.tmvpRefMvInfoBaseL0 = pic->rpl[0][0]->mvInfoBase;
#ifdef SUPPORT_VP9
    if (IS_VP9(vcenc_instance->codecFormat)) {
      struct sw_picture *previousPic =
          get_picture(c, vcenc_instance->vp9_inst.previousPoc);
      asic->regs.tmvpRefMvInfoBaseL0 = previousPic->mvInfoBase;
    }
#endif
  }

  if (pic->sliceInst->type == B_SLICE) {
    pic->deltaPocL1[0] = pic->rpl[1][0]->poc - pic->poc;
    if (pic->sliceInst->active_l1_cnt > 1)
      pic->deltaPocL1[1] = pic->rpl[1][1]->poc - pic->poc;
    else
      pic->deltaPocL1[1] = 0;
    asic->regs.tmvpRefMvInfoBaseL1 = pic->rpl[1][0]->mvInfoBase;
  }

  if (pic->sliceInst->type != I_SLICE) {
    asic->regs.rplL0DeltaPocL0[0] = pic->rpl[0][0]->deltaPocL0[0];
    asic->regs.rplL0DeltaPocL0[1] = pic->rpl[0][0]->deltaPocL0[1];
    asic->regs.rplL0DeltaPocL1[0] = pic->rpl[0][0]->deltaPocL1[0];
    asic->regs.rplL0DeltaPocL1[1] = pic->rpl[0][0]->deltaPocL1[1];
  }

  if (pic->sliceInst->type == B_SLICE) {
    asic->regs.rplL1DeltaPocL0[0] = pic->rpl[1][0]->deltaPocL0[0];
    asic->regs.rplL1DeltaPocL0[1] = pic->rpl[1][0]->deltaPocL0[1];
    asic->regs.rplL1DeltaPocL1[0] = pic->rpl[1][0]->deltaPocL1[0];
    asic->regs.rplL1DeltaPocL1[1] = pic->rpl[1][0]->deltaPocL1[1];
  }

  if (!IS_AV1(vcenc_instance->codecFormat) &&
      !IS_VP9(vcenc_instance->codecFormat)) {
    asic->regs.colFrameFromL0 = 1;
    if (codingType == VCENC_BIDIR_PREDICTED_FRAME) {
      /* Use Closest frame as collacated frame */
      asic->regs.colFrameFromL0 =
          (ABS((int)pic->deltaPocL0[0]) < ABS((int)pic->deltaPocL1[0])) ? 1 : 0;

      //Do not use Intra frame
      if (pic->rpl[!asic->regs.colFrameFromL0][0]->sliceInst->type == I_SLICE)
        asic->regs.colFrameFromL0 = !asic->regs.colFrameFromL0;
    }
    /* Use closest ref */
    asic->regs.colFrameRefIdx = 0;

    //If collocated frame is intra, disable tmvp
    if (pic->sliceInst->type != I_SLICE &&
        pic->rpl[!asic->regs.colFrameFromL0][0]->sliceInst->type == I_SLICE) {
      asic->regs.sliceTmvpEnable = 0;
    }

    //not-used-as-reference frame does not need to write TMVP info
    if (!pic->reference) asic->regs.writeTMVinfoDDR = 0;
  }

#ifdef SUPPORT_AV1
  /* We need to store order hint for av1 */
  if (IS_AV1(vcenc_instance->codecFormat)) {
    for (i = 0; i < SW_REF_FRAMES; i++) {
      if (pic->sliceInst->type == I_SLICE)
        pic->av1OrderHint[i] = 0;
      else {
        pic->av1OrderHint[i] = pic->rpl[0][0]->poc;
        if (pic->sliceInst->type == B_SLICE && i > GOLDEN_FRAME)
          pic->av1OrderHint[i] = pic->rpl[1][0]->poc;
      }
    }

    if (pic->sliceInst->type != I_SLICE)
      asic->regs.av1LastAltRefOrderHint =
          pic->rpl[0][0]->av1OrderHint[ALTREF_FRAME];

    /* For AV1, enable controled by allow_ref_frame_mvs */
    asic->regs.sliceTmvpEnable = vcenc_instance->av1_inst.allow_ref_frame_mvs;

    //not-used-as-reference frame does not need to write TMVP info
    if (!pic->reference)
      asic->regs.writeTMVinfoDDR = 0;
    else {
      /* AV1 may need to access TMV info from Intra frame, although it won't use intra CU.
         To avoid clear tmv buffer in software, ask HW to always write to DDR
      */
      asic->regs.writeTMVinfoDDR = 1;
    }
  }
#endif

#ifdef SUPPORT_VP9
  /* For VP9, tmvp is only enabled when last encoded frame is showable
       Or if last frame is intra */
  if (IS_VP9(vcenc_instance->codecFormat)) {
    if (!vcenc_instance->vp9_inst.lastFrameShowed ||
        vcenc_instance->vp9_inst.last_frame_type == VP9_KEY_FRAME) {
      asic->regs.sliceTmvpEnable = 0;
    }

    /* For vp9, no need to write out tmv if not show frame */
    asic->regs.writeTMVinfoDDR &= vcenc_instance->vp9_inst.show_frame;
  }
#endif
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : VCEncAddNaluSize
    Description   : Adds the size of a NAL unit into NAL size output buffer.

    Return type   : void
    Argument      : pEncOut - encoder output structure
    Argument      : naluSizeBytes - size of the NALU in bytes
------------------------------------------------------------------------------*/
void VCEncAddNaluSize(VCEncOut *pEncOut, u32 naluSizeBytes, u32 tileId) {
  if (tileId == 0) {
    if (pEncOut->pNaluSizeBuf != NULL) {
      pEncOut->pNaluSizeBuf[pEncOut->numNalus++] = naluSizeBytes;
      pEncOut->pNaluSizeBuf[pEncOut->numNalus] = 0;
    }
  } else {
    if (pEncOut->tileExtra[tileId - 1].pNaluSizeBuf != NULL) {
      pEncOut->tileExtra[tileId - 1]
          .pNaluSizeBuf[pEncOut->tileExtra[tileId - 1].numNalus++] =
          naluSizeBytes;
      pEncOut->tileExtra[tileId - 1]
          .pNaluSizeBuf[pEncOut->tileExtra[tileId - 1].numNalus] = 0;
    }
  }
}

/*------------------------------------------------------------------------------
    Function name : VCEncCodecPrepareEncode
    Description   : prepare encode for av1/vp9.
    Return type   : VCEncRet
------------------------------------------------------------------------------*/
VCEncRet VCEncCodecPrepareEncode(struct vcenc_instance *vcenc_instance,
                                 const VCEncIn *pEncIn, VCEncOut *pEncOut,
                                 VCEncPictureCodingType codingType,
                                 struct sw_picture *pic, struct container *c,
                                 u32 *segcounts) {
#ifdef SUPPORT_VP9
  if (IS_VP9(vcenc_instance->codecFormat))
    return VCEncCodecPrepareEncodeVP9(vcenc_instance, pEncIn, pEncOut,
                                      codingType, pic, c, segcounts);
#endif

#ifdef SUPPORT_AV1
  if (IS_AV1(vcenc_instance->codecFormat))
    return VCEncCodecPrepareEncodeAV1(vcenc_instance, pEncIn, pEncOut,
                                      codingType, pic, c);
#endif

  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name : VCEncCodecPostEncodeUpdate
    Description   : post encode update for av1/vp9.
    Return type   : VCEncRet
------------------------------------------------------------------------------*/
VCEncRet VCEncCodecPostEncodeUpdate(struct vcenc_instance *vcenc_instance,
                                    const VCEncIn *pEncIn, VCEncOut *pEncOut,
                                    VCEncPictureCodingType codingType,
                                    struct sw_picture *pic) {
  VCEncRet ret = VCENC_OK;
#ifdef SUPPORT_VP9
  if (IS_VP9(vcenc_instance->codecFormat)) {
    ret = VCEncCodecPostEncodeUpdateVP9(vcenc_instance, pEncIn, pEncOut,
                                        codingType, pic);
  }
#endif

#ifdef SUPPORT_AV1
  if (IS_AV1(vcenc_instance->codecFormat)) {
    EWLLinearMem_t FrameCtxAV1 =
        vcenc_instance->asic.internalFrameContext[pic->picture_memeory_id];
    EWLSyncMemData(&FrameCtxAV1, 0, FRAME_CONTEXT_LENGTH, DEVICE_TO_HOST);
    ret = VCEncCodecPostEncodeUpdateAV1(vcenc_instance, pEncIn, pEncOut,
                                        codingType);
  }
#endif

  return ret;
}

/*------------------------------------------------------------------------------
    Function name : VCEncFlushDisplay
    Description   : flush display frames for av1/vp9.
    Return type   : VCEncRet
------------------------------------------------------------------------------*/
VCEncRet VCEncFlushDisplay(struct vcenc_instance *vcenc_instance,
                                    const VCEncIn *pEncIn, VCEncOut *pEncOut,
                                    VCEncPictureCodingType codingType) {
  VCEncRet ret = VCENC_OK;
#ifdef SUPPORT_VP9
  if (IS_VP9(vcenc_instance->codecFormat)) {
    ret = VCEncFlushDisplayVP9(vcenc_instance, pEncIn, pEncOut, codingType);
  }
#endif

#ifdef SUPPORT_AV1
  if (IS_AV1(vcenc_instance->codecFormat)) {
    ret = VCEncFlushDisplayAV1(vcenc_instance, pEncIn, pEncOut, codingType);
  }
#endif

  return ret;
}

/* Write end-of-stream code */
void EndOfSequence(struct vcenc_instance *vcenc_instance, const VCEncIn *pEncIn,
                   VCEncOut *pEncOut) {
  if (IS_H264(vcenc_instance->codecFormat))
    H264EndOfSequence(&vcenc_instance->stream,
                      vcenc_instance->asic.regs.streamMode);
  else if (IS_HEVC(vcenc_instance->codecFormat))
    HEVCEndOfSequence(&vcenc_instance->stream,
                      vcenc_instance->asic.regs.streamMode);
#ifdef SUPPORT_AV1
  else if (IS_AV1(vcenc_instance->codecFormat))
    AV1EndOfSequence(vcenc_instance, pEncIn, pEncOut,
                     &vcenc_instance->stream.byteCnt);
#endif
#ifdef SUPPORT_VP9
  else if (IS_VP9(vcenc_instance->codecFormat))
    VP9EndOfSequence(vcenc_instance, pEncIn, pEncOut,
                     &vcenc_instance->stream.byteCnt);
#endif
}

i32 EncInitLookAheadBufCnt(const VCEncConfig *config, i32 lookaheadDepth) {
  /* calculate lookahead frame buffer count / delay
   * maxLaFrames = lookAheadDepth + maxGopSize/2
   * +-------------------+----------+----------------+-----------------------------------------+
   * | input down-sample | rdoLevel | lookAheadDepth |      delay or buffer number             |
   * +-------------------+----------+----------------+-----------------------------------------+
   * |       0           |    1     |     > 20       | maxLaFrames + maxGopSize + maxGopSize   |
   * |       0           |    1     |    <= 20       | maxLaFrames + maxGopSize + maxGopSize/2 |
   * |       0           |   > 1    |     > 20       | maxLaFrames + maxGopSize + maxGopSize/2 |
   * |       0           |   > 1    |    <= 20       | maxLaFrames + maxGopSize + maxGopSize/4 |
   * |       1           |    1     |     > 20       | maxLaFrames + maxGopSize + maxGopSize/4 |
   * |       1           |    1     |    <= 20       | maxLaFrames + maxGopSize                |
   * |       1           |   > 1    |     > 20       | maxLaFrames + maxGopSize                |
   * |       1           |   > 1    |    <= 20       | maxLaFrames + maxGopSize                |
   * +-------------------+----------+----------------+-----------------------------------------+ */
  i32 lookAheadBufCnt =
      CUTREE_MAX_LOOKAHEAD_FRAMES(lookaheadDepth, config->gopSize) +
      MAX_GOP_SIZE_INUSE(config->gopSize);
  i32 extraDelay = MAX_GOP_SIZE_INUSE(config->gopSize);
  /* check by 0 (not 1 in above table) since config->rdoLevel (0~2) = rdoLevel_by_user(1~3) - 1 */
  if (config->rdoLevel > 0) extraDelay /= 2;
  if (lookaheadDepth <= 20) extraDelay /= 2;
  if (config->inLoopDSRatio)
    extraDelay -= MAX_GOP_SIZE_INUSE(config->gopSize) * 3 / 4;
  extraDelay = MAX(0, extraDelay);
  lookAheadBufCnt += extraDelay;

  return lookAheadBufCnt;
}

/*------------------------------------------------------------------------------
    Function name   : FindNextForceIdr
    Description     : find next idr frame in queue
    Return type     : i32 next picture idr
    Argument        : struct queue *jobQueue                 [in/out]   jobQueue
------------------------------------------------------------------------------*/
i32 FindNextForceIdr(struct queue *jobQueue) {
  VCEncJob *job = (VCEncJob *)queue_tail(jobQueue);
  i32 idrPicCnt = -1;
  /*get job from job queue*/
  while (NULL != job) {
    if (HANTRO_TRUE == job->encIn.bIsIDR) {
      idrPicCnt = job->encIn.picture_cnt;
      break;
    }
    job = (VCEncJob *)job->next;
  }
  return idrPicCnt;
}

/*------------------------------------------------------------------------------
    Function name   : AGopDecision
    Description     : single pass: adaptive gop
    Return type     : i32   next gop size
    Argument        : struct vcenc_instance *vcenc_instance  [in]           instance
    Argument        : VCEncIn *pEncIn                        [in]
    Argument        : const VCEncOut *pEncOut                [in]
    Argument        : i32 *pNextGopSize                      [out]          next gop size
    Argument        : VCENCAdapGopCtr * agop                 [in/out]       agop information
------------------------------------------------------------------------------*/
i32 AGopDecision(const struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                 const VCEncOut *pEncOut, i32 *pNextGopSize,
                 VCENCAdapGopCtr *agop) {
  i32 nextGopSize = -1;

  u32 uiIntraCu8Num = pEncOut->cuStatis.intraCu8Num;
  u32 uiSkipCu8Num = pEncOut->cuStatis.skipCu8Num;
  u32 uiPBFrameCost = pEncOut->cuStatis.PBFrame4NRdCost;
  i32 width = vcenc_instance->width;
  i32 height = vcenc_instance->height;
  u32 maxGopSize = vcenc_instance->gopMaxBSize != DEFAULT ? vcenc_instance->gopMaxBSize + 1 : 8;
  maxGopSize = MIN(maxGopSize,8);
  double dIntraVsInterskip =
      (double)uiIntraCu8Num / (double)((width / 8) * (height / 8));
  double dSkipVsInterskip =
      (double)uiSkipCu8Num / (double)((width / 8) * (height / 8));
  u8 typePorLowB =
    (vcenc_instance->pass == 0 && !(pEncIn->gopCurrPicConfig.poc % pEncIn->gopSize))? 1 :
    (pEncIn->codingType == VCENC_PREDICTED_FRAME);
  agop->gop_frm_num++;
  agop->sum_intra_vs_interskip += dIntraVsInterskip;
  agop->sum_skip_vs_interskip += dSkipVsInterskip;
  agop->sum_costP += typePorLowB ? uiPBFrameCost : 0;
  agop->sum_costB += !typePorLowB ? uiPBFrameCost : 0;
  agop->sum_intra_vs_interskipP += typePorLowB ? dIntraVsInterskip : 0;
  agop->sum_intra_vs_interskipB += !typePorLowB ? dIntraVsInterskip : 0;

  if (pEncIn->gopPicIdx ==
      pEncIn->gopSize -
          1)  //last frame of the current gop. decide the gopsize of next gop.
  {
    dIntraVsInterskip = agop->sum_intra_vs_interskip / agop->gop_frm_num;
    dSkipVsInterskip = agop->sum_skip_vs_interskip / agop->gop_frm_num;
    agop->sum_costB = (agop->gop_frm_num > 1)
                          ? (agop->sum_costB / (agop->gop_frm_num - 1))
                          : 0xFFFFFFF;
    agop->sum_intra_vs_interskipB =
        (agop->gop_frm_num > 1)
            ? (agop->sum_intra_vs_interskipB / (agop->gop_frm_num - 1))
            : 0xFFFFFFF;
    //Enabled adaptive GOP size for large resolution
    if (((width * height) >= (1280 * 720)) ||
        ((MAX_ADAPTIVE_GOP_SIZE > 3) && ((width * height) >= (416 * 240)))) {
      if ((((double)agop->sum_costP / (double)agop->sum_costB) < 1.1) &&
          (dSkipVsInterskip >= 0.95)) {
        agop->last_gopsize = nextGopSize = 1;
      } else if (((double)agop->sum_costP / (double)agop->sum_costB) > 5) {
        nextGopSize = agop->last_gopsize;
      } else {
        if (((agop->sum_intra_vs_interskipP > 0.40) &&
             (agop->sum_intra_vs_interskipP < 0.70) &&
             (agop->sum_intra_vs_interskipB < 0.10))) {
          agop->last_gopsize++;
          if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
            agop->last_gopsize++;
          }
          agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
          nextGopSize = agop->last_gopsize;
        } else if (dIntraVsInterskip >= 0.30) {
          agop->last_gopsize = nextGopSize = 1;  //No B
        } else if (dIntraVsInterskip >= 0.20) {
          agop->last_gopsize = nextGopSize = 2;  //One B
        } else if (dIntraVsInterskip >= 0.10) {
          agop->last_gopsize--;
          if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
            agop->last_gopsize--;
          }
          agop->last_gopsize = MAX(agop->last_gopsize, 3);
          nextGopSize = agop->last_gopsize;
        } else {
          agop->last_gopsize++;
          if (agop->last_gopsize == 5 || agop->last_gopsize == 7) {
            agop->last_gopsize++;
          }
          agop->last_gopsize = MIN(agop->last_gopsize, MAX_ADAPTIVE_GOP_SIZE);
          nextGopSize = agop->last_gopsize;
        }
      }
    } else {
      nextGopSize = 3;
    }
    agop->gop_frm_num = 0;
    agop->sum_intra_vs_interskip = 0;
    agop->sum_skip_vs_interskip = 0;
    agop->sum_costP = 0;
    agop->sum_costB = 0;
    agop->sum_intra_vs_interskipP = 0;
    agop->sum_intra_vs_interskipB = 0;

    nextGopSize = MIN(nextGopSize, MAX_ADAPTIVE_GOP_SIZE);
    nextGopSize = MIN(maxGopSize, nextGopSize);
    if (nextGopSize == 5) nextGopSize--;
    else if(nextGopSize == 7) nextGopSize--;
    agop->last_gopsize = nextGopSize;
  }
  if (nextGopSize != -1) *pNextGopSize = nextGopSize;

  return nextGopSize;
}

void VCEncPass2CheckAgop(VCEncInst inst, VCEncIn *pEncIn, VCEncOut *pEncOut,
                               VCEncRet ret)
{
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  if (vcenc_instance->pass != 2 || vcenc_instance->gopSize != 0)
    return;

  {
    i32 *pNextGopSize = &vcenc_instance->nextGopSize;
    if (!((ret == VCENC_FRAME_ENQUEUE || pEncOut->streamSize == 0) ||
      pEncOut->codingType == VCENC_NOTCODED_FRAME ||
      ret == VCENC_SBI_ERROR ||
      ret == VCENC_OUTPUT_BUFFER_OVERFLOW))
    {
      //Adaptive GOP size decision
      if ( pEncOut->codingType != VCENC_INTRA_FRAME &&
           0 == vcenc_instance->pass1CutGopto1 )
      {
        AGopDecisionRefine(vcenc_instance, pEncIn, pEncOut, pNextGopSize, &vcenc_instance->agop);
        if (*pNextGopSize == 1)
          vcenc_instance->pass2Agop4to1 = 1;
        else
          vcenc_instance->pass2Agop4to1 = 0;
      }
      else if ( pEncOut->codingType == VCENC_INTRA_FRAME )
      {
        vcenc_instance->agop.gop_frm_num = 0;
      }
    }
  }
}

i32 AGopDecisionRefine(const struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                 const VCEncOut *pEncOut, i32 *pNextGopSize,
                 VCENCAdapGopCtr *agop) {
  i32 nextGopSize = -1;
  i32 currentGopSize = *pNextGopSize;
  i32 maxGopSize = vcenc_instance->gopMaxBSize != DEFAULT ? vcenc_instance->gopMaxBSize + 1 : 8;
  i32 i;
  u8 pass = vcenc_instance->pass;

  u32 uiIntraCu8Num = pEncOut->cuStatis.intraCu8Num;
  u32 uiSkipCu8Num = pEncOut->cuStatis.skipCu8Num;
  u32 uiPBFrameCost = pEncOut->cuStatis.PBFrame4NRdCost;
  i32 width = vcenc_instance->width;
  i32 height = vcenc_instance->height;
  double dIntraVsInterskip =
      (double)uiIntraCu8Num / (double)((width / 8) * (height / 8));
  double dSkipVsInterskip =
      (double)uiSkipCu8Num / (double)((width / 8) * (height / 8));

  //agop->gop_frm_num is encode order
  u32 idx = agop->gop_frm_num;
  VCEncVideoCodecFormat format = vcenc_instance->codecFormat;

  agop->gop_info[idx].sliceType = pEncOut->codingType;
  if (pass == 0) {
    if (pEncIn->gopCurrPicConfig.poc % pEncIn->gopSize)
      agop->gop_info[idx].sliceType = 0x0006; //VCENC_FRAME_TYPE_BLDY
  }
  agop->gop_info[idx].gopSize = currentGopSize;
  agop->gop_info[idx].poc = pEncOut->poc;
  agop->gop_info[idx].intra_ratio = dIntraVsInterskip;
  agop->gop_info[idx].skip_ratio = dSkipVsInterskip;
  agop->gop_info[idx].PBFrame4NRdCost = uiPBFrameCost;
  memcpy(agop->gop_info[idx].motionScore, vcenc_instance->asic.regs.motionScore,
         sizeof(vcenc_instance->asic.regs.motionScore));

  agop->gop_frm_num++;

  #define IS_CODING_TYPE_P_1PASS(x, pass) ((pass == 0) && ((x) == VCENC_PREDICTED_FRAME || (x) == 0x0006))

  //GOP 4 <-> GOP 1
  if (agop->gop_frm_num == 8)
  {
    double intra_ratio_average[2] = {0, 0};
    u32 PBFrame4NRdCost_average[2] = {0, 0};
    double skip_ratio_average[2] = {0, 0};

    for (i = 0; i < 2; i++)
    {
      intra_ratio_average[0] += agop->gop_info[i].intra_ratio;
      intra_ratio_average[1] += agop->gop_info[i+4].intra_ratio;
      PBFrame4NRdCost_average[0] += agop->gop_info[i].PBFrame4NRdCost;
      PBFrame4NRdCost_average[1] += agop->gop_info[i+4].PBFrame4NRdCost;
      skip_ratio_average[0] += agop->gop_info[i].skip_ratio;
      skip_ratio_average[1] += agop->gop_info[i+4].skip_ratio;
    }

    for (i = 0; i < 2; i++)
    {
      intra_ratio_average[i] /= 2;
      PBFrame4NRdCost_average[i] /= 2;
      skip_ratio_average[i] /= 2;
    }

    {
      /* convert gop4 to gop1 */
      if ((agop->gop_info[0].gopSize == 4) &&
          (agop->gop_info[4].gopSize == 4)) {
        //printf("4->1 intra_ratio_average=%f,skip_ratio_average=%f,agop->gop_info[0].intra_ratio=%f,agop->gop_info[0].skip_ratio=%f,agop->gop_info[4].intra_ratio=%f,agop->gop_info[4].skip_ratio=%f,agop->gop_info[7].intra_ratio=%f,agop->gop_info[7].skip_ratio=%f,agop->gop_info[5].intra_ratio=%f,agop->gop_info[5].skip_ratio=%f\n",intra_ratio_average[1],skip_ratio_average[1],agop->gop_info[0].intra_ratio,agop->gop_info[0].skip_ratio,agop->gop_info[4].intra_ratio,agop->gop_info[4].skip_ratio,agop->gop_info[7].intra_ratio,agop->gop_info[7].skip_ratio,agop->gop_info[5].intra_ratio,agop->gop_info[5].skip_ratio);
        if (EXCEPT_VP9_GOP4_TO_GOP1_1PASS(format, intra_ratio_average[1], skip_ratio_average[1], agop->gop_info[5], agop->gop_info[7], vcenc_instance->pass) ||
            EXCEPT_VP9_GOP4_TO_GOP1_PASS2(format, intra_ratio_average[1], skip_ratio_average[1], agop->gop_info[7], vcenc_instance->pass) ||
            VP9_GOP4_TO_GOP1(format, intra_ratio_average[1], agop->gop_info[5], vcenc_instance->pass)
           )
          {
            nextGopSize = 1;
            //printf("----gop 4->1\n");
          }
      }
      /* convert gop1 to gop4 */
      else if ((agop->gop_info[0].gopSize == 1) &&
               (agop->gop_info[4].gopSize == 1) && maxGopSize >= 4)
      {
        if ((PBFrame4NRdCost_average[1] <= (PBFrame4NRdCost_average[0] *90 /100)) &&
             (intra_ratio_average[1] <= (intra_ratio_average[0] *90 /100)))
        {
          nextGopSize = 4;
          //printf("----gop 1->4\n");
        }
      }
    }

    if (nextGopSize == -1)
      nextGopSize = currentGopSize;

    if (!((currentGopSize == 4 && nextGopSize == 4) || currentGopSize == 8))//may change to gop8
    {
      goto END;
    }

  }


  if (agop->gop_frm_num == 8 && vcenc_instance->pass == 0)
  {
    u32 th4 = AGOP_MOTION_TH * 32;
    u32 th8 = AGOP_MOTION_TH * 54;

    /* convert gop4 to gop8  */
    if (IS_CODING_TYPE_P_1PASS(agop->gop_info[0].sliceType, pass) &&
        (agop->gop_info[0].gopSize == 4) &&
        IS_CODING_TYPE_P_1PASS(agop->gop_info[4].sliceType, pass) &&
        (agop->gop_info[4].gopSize == 4)){
#if 0
      printf("4->8 agop->gop_info[0].motionScore[0][0]=%d,agop->gop_info[0].motionScore[0][1]=%d,"
                     "agop->gop_info[4].motionScore[0][0]=%d,agop->gop_info[4].motionScore[0][1]=%d,"
                     "agop->gop_info[0].intra_ratio=%f,agop->gop_info[0].skip_ratio=%f,agop->gop_info[4].intra_ratio=%f,agop->gop_info[4].skip_ratio=%f,agop->gop_info[5].intra_ratio=%f,agop->gop_info[5].skip_ratio=%f\n"
                     ,agop->gop_info[0].motionScore[0][0],agop->gop_info[0].motionScore[0][1],agop->gop_info[4].motionScore[0][0],agop->gop_info[4].motionScore[0][1],
                     agop->gop_info[0].intra_ratio,agop->gop_info[0].skip_ratio,agop->gop_info[4].intra_ratio,agop->gop_info[4].skip_ratio,agop->gop_info[5].intra_ratio,agop->gop_info[5].skip_ratio
                     );
#endif
      if ((agop->gop_info[0].motionScore[0][0] <= th4) &&
          (agop->gop_info[0].motionScore[0][1] <= th4) &&
          (agop->gop_info[4].motionScore[0][0] <= th4) &&
          (agop->gop_info[4].motionScore[0][1] <= th4) &&
          GOP4_TO_GOP8(format, agop->gop_info[5]) && maxGopSize >= 8){
        //printf("----4->8\n");
        nextGopSize = 8;
      }
    }
    /* convert gop8 to gop4  */
    else if (IS_CODING_TYPE_P_1PASS(agop->gop_info[0].sliceType, pass) &&
             (agop->gop_info[0].gopSize == 8)) {
      //printf("8->4 agop->gop_info[0].motionScore[0][0]=%d,agop->gop_info[0].motionScore[0][1]=%d,agop->gop_info[0].intra_ratio=%f,agop->gop_info[0].skip_ratio=%f,agop->gop_info[1].intra_ratio=%f,agop->gop_info[1].skip_ratio=%f\n",agop->gop_info[0].motionScore[0][0],agop->gop_info[0].motionScore[0][1],agop->gop_info[0].intra_ratio,agop->gop_info[0].skip_ratio,agop->gop_info[1].intra_ratio,agop->gop_info[1].skip_ratio);
      if ((((agop->gop_info[0].motionScore[0][0] > th8) ||
          (agop->gop_info[0].motionScore[0][1] > th8)) ||
          EXCEPT_VP9_GOP8_TO_GOP4(format, agop->gop_info[0], agop->gop_info[1]) ||
          VP9_GOP8_TO_GOP4(format, agop->gop_info[0], agop->gop_info[1])) && maxGopSize >= 4)
      {
        //printf("----8->4\n");
        nextGopSize = 4;
      }
    }

    if (nextGopSize == -1)
      nextGopSize = currentGopSize;

  }

END:
  if (agop->gop_frm_num == 8)
  {
    if ((currentGopSize == 4 && nextGopSize == 4) ||
          (currentGopSize == 1 && nextGopSize == 1))
      agop->gop_frm_num = 4;
    else
      agop->gop_frm_num = 0;

    for (i = 0; i < agop->gop_frm_num; i++)
    {
      agop->gop_info[i] = agop->gop_info[4 + i];
    }

    if (agop->gop_frm_num == 0)
        memset(agop->gop_info, 0, sizeof(agop->gop_info));
  }

  if (nextGopSize == -1)
      nextGopSize = currentGopSize;

  if (nextGopSize != -1)
  {
    *pNextGopSize = nextGopSize;
  }

  return nextGopSize;
}

/*------------------------------------------------------------------------------
    Function name   : SavePicCfg
    Description     : single pass: save encIn' picture config
    Return type     : void
    Argument        : const VCEncIn * pEncIn                 [in]
    Argument        : VCEncPicConfig* pPicCfg                [out]        picture config
------------------------------------------------------------------------------*/
void SavePicCfg(const VCEncIn *pEncIn, VCEncPicConfig *pPicCfg) {
  if (NULL == pEncIn || NULL == pPicCfg) return;

  pPicCfg->codingType = pEncIn->codingType;
  pPicCfg->poc = pEncIn->poc;
  pPicCfg->bIsIDR = pEncIn->bIsIDR;
  pPicCfg->bIsIntraOnly = pEncIn->bIsIntraOnly;

  memcpy(&pPicCfg->gopConfig, &pEncIn->gopConfig, sizeof(pPicCfg->gopConfig));
  pPicCfg->gopSize = pEncIn->gopSize;
  pPicCfg->gopPicIdx = pEncIn->gopPicIdx;
  pPicCfg->picture_cnt = pEncIn->picture_cnt;
  pPicCfg->picture_gopIdx = pEncIn->picture_gopIdx;
  pPicCfg->last_idr_picture_cnt = pEncIn->last_idr_picture_cnt;

  pPicCfg->bIsPeriodUsingLTR = pEncIn->bIsPeriodUsingLTR;
  pPicCfg->bIsPeriodUpdateLTR = pEncIn->bIsPeriodUpdateLTR;
  memcpy(&pPicCfg->gopCurrPicConfig, &pEncIn->gopCurrPicConfig,
         sizeof(pPicCfg->gopCurrPicConfig));
  memcpy(pPicCfg->long_term_ref_pic, pEncIn->long_term_ref_pic,
         sizeof(pPicCfg->long_term_ref_pic));
  memcpy(pPicCfg->bLTR_used_by_cur, pEncIn->bLTR_used_by_cur,
         sizeof(pPicCfg->bLTR_used_by_cur));
  memcpy(pPicCfg->bLTR_need_update, pEncIn->bLTR_need_update,
         sizeof(pPicCfg->bLTR_need_update));

  pPicCfg->i8SpecialRpsIdx = pEncIn->i8SpecialRpsIdx;
  pPicCfg->i8SpecialRpsIdx_next = pEncIn->i8SpecialRpsIdx_next;
  pPicCfg->u8IdxEncodedAsLTR = pEncIn->u8IdxEncodedAsLTR;

  return;
}

/*------------------------------------------------------------------------------
    Function name   : SinglePassEnqueueJob
    Description     : single pass: add job to job queue
    Return type     : VCEncRet
    Argument        : struct vcenc_instance *vcenc_instance  [in/out]   instance
    Argument        : const VCEncIn *pEncIn                  [in]       encIn for the job
------------------------------------------------------------------------------*/
VCEncRet SinglePassEnqueueJob(struct vcenc_instance *vcenc_instance,
                              const VCEncIn *pEncIn) {
  VCEncRet ret = VCENC_ERROR;
  VCEncJob *job = NULL;
  ptr_t *tmpBusLuma, *tmpBusU, *tmpBusV;
  u32 iBuf, tileId;
  ret = GetBufferFromPool(vcenc_instance->jobBufferPool, (void **)&job);
  if (VCENC_OK != ret || !job) return ret;

  memset(job, 0, sizeof(VCEncJob));
  memcpy(&job->encIn, pEncIn, sizeof(VCEncIn));

  if (vcenc_instance->num_tile_columns > 1)
    job->encIn.tileExtra = (VCEncInTileExtra *)((u8 *)job + sizeof(VCEncJob));

  for (tileId = 0; tileId < vcenc_instance->num_tile_columns; tileId++) {
    for (iBuf = 0; iBuf < MAX_STRM_BUF_NUM; iBuf++) {
      if (tileId == 0) {
        job->encIn.pOutBuf[iBuf] = pEncIn->pOutBuf[iBuf];
        job->encIn.busOutBuf[iBuf] = pEncIn->busOutBuf[iBuf];
        job->encIn.outBufSize[iBuf] = pEncIn->outBufSize[iBuf];
        job->encIn.cur_out_buffer[iBuf] = pEncIn->cur_out_buffer[iBuf];

        if (iBuf == 0) {
          job->encIn.busLuma = pEncIn->busLuma;
          job->encIn.busChromaU = pEncIn->busChromaU;
          job->encIn.busChromaV = pEncIn->busChromaV;
        }
      } else {
        job->encIn.tileExtra[tileId - 1].pOutBuf[iBuf] =
            pEncIn->tileExtra[tileId - 1].pOutBuf[iBuf];
        job->encIn.tileExtra[tileId - 1].busOutBuf[iBuf] =
            pEncIn->tileExtra[tileId - 1].busOutBuf[iBuf];
        job->encIn.tileExtra[tileId - 1].outBufSize[iBuf] =
            pEncIn->tileExtra[tileId - 1].outBufSize[iBuf];
        job->encIn.tileExtra[tileId - 1].cur_out_buffer[iBuf] =
            pEncIn->tileExtra[tileId - 1].cur_out_buffer[iBuf];
        if (iBuf == 0) {
          job->encIn.tileExtra[tileId - 1].busLuma =
              pEncIn->tileExtra[tileId - 1].busLuma;
          job->encIn.tileExtra[tileId - 1].busChromaU =
              pEncIn->tileExtra[tileId - 1].busChromaU;
          job->encIn.tileExtra[tileId - 1].busChromaV =
              pEncIn->tileExtra[tileId - 1].busChromaV;
        }
      }
    }
  }

  if (pEncIn->bIsIDR && (vcenc_instance->nextIdrCnt > pEncIn->picture_cnt ||
                         vcenc_instance->nextIdrCnt < 0)) {
    // current next idr > picCnt or there is no next idr, reset next idr.
    vcenc_instance->nextIdrCnt = pEncIn->picture_cnt;
  }
  //match frame with parameter
  EncCodingCtrlParam *pEncCodingCtrlParam = (EncCodingCtrlParam *)queue_head(
      &vcenc_instance->codingCtrl.codingCtrlQueue);
  job->pCodingCtrlParam = pEncCodingCtrlParam;
  if (pEncCodingCtrlParam) {
    if (pEncCodingCtrlParam->startPicCnt <
        0)  //whether latest parameter is set from current frame
      pEncCodingCtrlParam->startPicCnt = pEncIn->picture_cnt;
    pEncCodingCtrlParam->refCnt++;
  }
  EncRateCtrlParam *pEncRateCtrlParam = (EncRateCtrlParam *)queue_head(
      &vcenc_instance->rateCtrl.rateCtrlQueue);
  job->pRateCtrlParam = pEncRateCtrlParam;
  if (pEncRateCtrlParam) {
    if (pEncRateCtrlParam->startPicCnt <
        0)  //whether latest parameter is set from current frame
      pEncRateCtrlParam->startPicCnt = pEncIn->picture_cnt;
    pEncRateCtrlParam->refCnt++;
  }
  queue_put(&vcenc_instance->jobQueue, (struct node *)job);
  vcenc_instance->enqueueJobNum++;
  return VCENC_OK;
}

/*------------------------------------------------------------------------------
    Function name   : InitAgop
    Description     : single pass: init agop for AGopDecision
    Return type     : void
    Argument        : VCENCAdapGopCtr * agop                 [out]       agop information
------------------------------------------------------------------------------*/
void InitAgop(VCENCAdapGopCtr *agop) {
  //Adaptive Gop variables
  agop->last_gopsize = MAX_ADAPTIVE_GOP_SIZE;
  agop->gop_frm_num = 0;
  agop->sum_intra_vs_interskip = 0;
  agop->sum_skip_vs_interskip = 0;
  agop->sum_intra_vs_interskipP = 0;
  agop->sum_intra_vs_interskipB = 0;
  agop->sum_costP = 0;
  agop->sum_costB = 0;
  memset(agop->gop_info, 0, sizeof(agop->gop_info));
}

/*------------------------------------------------------------------------------
    Function name   : SinglePassGetNextJob
    Description     : single pass: get job to be handled according to given picture cnt
    Return type     : VCEncJob *  job to be handled
    Argument        : struct vcenc_instance *vcenc_instance  [in/out]   instance
    Argument        : const i32 picCnt                       [in]       picture cnt
------------------------------------------------------------------------------*/
VCEncJob *SinglePassGetNextJob(struct vcenc_instance *vcenc_instance,
                               const i32 picCnt) {
  VCEncJob *job = NULL, *prevJob = NULL;
  job = prevJob = (VCEncJob *)queue_tail(&vcenc_instance->jobQueue);

  /* get job from job queue */
  while (NULL != job) {
    if (picCnt == job->encIn.picture_cnt) {
      queue_remove(&vcenc_instance->jobQueue, (struct node *)job);
      break;
    }
    job = (VCEncJob *)job->next;
  }

  /* get the sideline job firstly */
  if ((job!=NULL)&&(vcenc_instance->has_sideline)) {
    vcenc_instance->aif.recon_next = &job->encIn;
    queue_put(&vcenc_instance->jobQueue, (struct node *)job);

    job = (VCEncJob *)queue_tail(&vcenc_instance->jobQueue);
    while (NULL != job) {
      if (job->encIn.poc == vcenc_instance->sideline_poc) {
        queue_remove(&vcenc_instance->jobQueue, (struct node *)job);
        break;
      }
      job = (VCEncJob *)job->next;
    }
  }

  return job;


}

/*------------------------------------------------------------------------------
    Function name   : SetPicCfgToEncIn
    Description     : single pass: set picture config to encIn
    Return type     : void
    Argument        : const VCEncPicConfig* pPicCfg          [in]           picture config
    Argument        : VCEncIn * pEncIn                       [out]
------------------------------------------------------------------------------*/
void SetPicCfgToEncIn(const VCEncPicConfig *pPicCfg, VCEncIn *pEncIn) {
  if (NULL == pPicCfg || NULL == pEncIn) return;

  pEncIn->codingType = pPicCfg->codingType;
  pEncIn->poc = pPicCfg->poc;
  pEncIn->bIsIDR = pPicCfg->bIsIDR;
  pEncIn->bIsIntraOnly = pPicCfg->bIsIntraOnly;

  //don't reset lastPic to make sure encode can end early if necessary.
  i32 lastPic = pEncIn->gopConfig.lastPic;
  memcpy(&pEncIn->gopConfig, &pPicCfg->gopConfig, sizeof(pEncIn->gopConfig));
  pEncIn->gopConfig.lastPic = lastPic;

  pEncIn->gopSize = pPicCfg->gopSize;
  pEncIn->gopPicIdx = pPicCfg->gopPicIdx;
  pEncIn->picture_cnt = pPicCfg->picture_cnt;
  pEncIn->picture_gopIdx = pPicCfg->picture_gopIdx;
  pEncIn->last_idr_picture_cnt = pPicCfg->last_idr_picture_cnt;

  pEncIn->bIsPeriodUsingLTR = pPicCfg->bIsPeriodUsingLTR;
  pEncIn->bIsPeriodUpdateLTR = pPicCfg->bIsPeriodUpdateLTR;
  memcpy(&pEncIn->gopCurrPicConfig, &pPicCfg->gopCurrPicConfig,
         sizeof(pEncIn->gopCurrPicConfig));
  memcpy(pEncIn->long_term_ref_pic, pPicCfg->long_term_ref_pic,
         sizeof(pEncIn->long_term_ref_pic));
  memcpy(pEncIn->bLTR_used_by_cur, pPicCfg->bLTR_used_by_cur,
         sizeof(pEncIn->bLTR_used_by_cur));
  memcpy(pEncIn->bLTR_need_update, pPicCfg->bLTR_need_update,
         sizeof(pEncIn->bLTR_need_update));

  pEncIn->i8SpecialRpsIdx = pPicCfg->i8SpecialRpsIdx;
  pEncIn->i8SpecialRpsIdx_next = pPicCfg->i8SpecialRpsIdx_next;
  pEncIn->u8IdxEncodedAsLTR = pPicCfg->u8IdxEncodedAsLTR;

  return;
}

/*------------------------------------------------------------------------------
    Function name : FindNextPic
    Description   : get gopSize/rps for next frame
    Return type   : void
    Argument      : inst - encoder instance
------------------------------------------------------------------------------*/
VCEncPictureCodingType FindNextPic(VCEncInst inst, VCEncIn *encIn,
                                   i32 nextGopSize, const u8 *gopCfgOffset,
                                   i32 nextIdrCnt, bool fpsChange) {
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  VCEncPictureCodingType nextCodingType;
  int idx, offset, cur_poc, delta_poc_to_next;
  bool bIsCodingTypeChanged;
  int next_gop_size = nextGopSize;
  VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(encIn->gopConfig));
  i32 *p_picture_cnt = &encIn->picture_cnt;
  i32 *p_picture_gopIdx = &encIn->picture_gopIdx;
  i32 last_idr_picture_cnt = encIn->last_idr_picture_cnt;
  int picture_cnt_tmp = *p_picture_cnt;
  int picture_gopIdx_tmp = *p_picture_gopIdx;
  i32 i32LastPicPoc;
  i32 idr_interval = 0;
  u32 bIsLP = 0;

  //update idr interval
  if (nextIdrCnt >= 0)
    idr_interval = nextIdrCnt - last_idr_picture_cnt;
  else
    idr_interval = 0;

  //get current poc within GOP
  if (encIn->codingType == VCENC_INTRA_FRAME && (encIn->poc == 0)) {
    // last is an IDR
    cur_poc = 0;
    encIn->gopPicIdx = 0;
    if(vcenc_instance->enableLeadingPictures && encIn->gopSize > 1) {
      encIn->gopPicIdx = encIn->gopSize - 1;
      bIsLP = 1;
    }
  }
  else if(encIn->poc < 0) {
    idx = encIn->gopPicIdx + gopCfg->gopCfgOffsetLP;
    cur_poc = gopCfg->pGopPicCfg[idx].poc;
    --encIn->gopPicIdx;
    bIsLP = (encIn->gopPicIdx > 0);
  } else {
    //Update current idx and poc within a GOP
    idx = encIn->gopPicIdx + gopCfgOffset[encIn->gopSize];
    cur_poc = gopCfg->pGopPicCfg[idx].poc;
    encIn->gopPicIdx = (encIn->gopPicIdx + 1) % encIn->gopSize;
    if (encIn->gopPicIdx == 0) cur_poc -= encIn->gopSize;
  }
  if(fpsChange && encIn->gopPicIdx == 0 && vcenc_instance->bInsertIDRonFPSAdjust)
    next_gop_size = 1;

  //leading picture
  if (bIsLP)
    offset = gopCfg->gopCfgOffsetLP;
  //a GOP end, to start next GOP
  else if (encIn->gopPicIdx == 0)
    offset = gopCfgOffset[next_gop_size];
  else
    offset = gopCfgOffset[encIn->gopSize];

  //get next poc within GOP, and delta_poc
  idx = encIn->gopPicIdx + offset;
  delta_poc_to_next = gopCfg->pGopPicCfg[idx].poc - cur_poc;
  //next picture cnt
  *p_picture_cnt = picture_cnt_tmp + delta_poc_to_next;
  *p_picture_gopIdx = picture_gopIdx_tmp + delta_poc_to_next;

  //Handle Tail (seqence end or cut by an I frame)
  {
    //just finished a GOP and will jump to a P frame
    if (encIn->gopPicIdx == 0 && delta_poc_to_next > 1) {
      int gop_end_pic = *p_picture_cnt;
      int gop_shorten = 0, gop_shorten_idr = 0, gop_shorten_tail = 0;

      //cut  by an IDR
      if ((idr_interval) &&
          ((gop_end_pic - last_idr_picture_cnt) >= idr_interval) &&
          !vcenc_instance->gdrDuration)
        gop_shorten_idr =
            1 + ((gop_end_pic - last_idr_picture_cnt) - idr_interval);

      //handle sequence tail
      while (((CalNextPic(gopCfg, gop_end_pic--) + gopCfg->firstPic) >
              gopCfg->lastPic) &&
             (gop_shorten_tail < next_gop_size - 1))
        gop_shorten_tail++;

      gop_shorten = gop_shorten_idr > gop_shorten_tail ? gop_shorten_idr
                                                       : gop_shorten_tail;

      if (gop_shorten_idr > gop_shorten_tail && vcenc_instance->enableLeadingPictures) {
        // jump to IDR, mark remaining frames before IDR as leading pictures
        next_gop_size -= gop_shorten - 1;
        delta_poc_to_next -= gop_shorten_idr - 1;
        *p_picture_cnt = picture_cnt_tmp + delta_poc_to_next;
      } else if (gop_shorten >= next_gop_size) {
        //for gopsize = 1
        *p_picture_cnt = picture_cnt_tmp + 1 - cur_poc;
        *p_picture_gopIdx = picture_gopIdx_tmp + 1 - cur_poc;
      } else if (gop_shorten > 0) {
        //reduce gop size
        const int max_reduced_gop_size = gopCfg->gopLowdelay ? 1 : 4;
        next_gop_size -= gop_shorten;
        if (next_gop_size > max_reduced_gop_size)
          next_gop_size = max_reduced_gop_size;

        idx = gopCfgOffset[next_gop_size];
        delta_poc_to_next = gopCfg->pGopPicCfg[idx].poc - cur_poc;
        *p_picture_cnt = picture_cnt_tmp + delta_poc_to_next;
        *p_picture_gopIdx = picture_gopIdx_tmp + delta_poc_to_next;
      }
    }

    if (encIn->gopPicIdx == 0) encIn->gopSize = next_gop_size;

    i32LastPicPoc = encIn->poc;
    encIn->poc += *p_picture_cnt - picture_cnt_tmp;
    bIsCodingTypeChanged = HANTRO_FALSE;
    //next coding type
    bool forceIntra =
        (idr_interval &&
         ((*p_picture_cnt - last_idr_picture_cnt) >= idr_interval))
        || (fpsChange && encIn->gopPicIdx == 0 && vcenc_instance->bInsertIDRonFPSAdjust);
    bool forceIntraOnly = 0;
    if (vcenc_instance->intraPeriod > 0 &&
        ((*p_picture_cnt - last_idr_picture_cnt) % vcenc_instance->intraPeriod == 0)) {
        forceIntraOnly = 1;
    }
    if (forceIntra) {
      nextCodingType = VCENC_INTRA_FRAME;
      encIn->bIsIDR = HANTRO_TRUE;
      bIsCodingTypeChanged = HANTRO_TRUE;
    } else {
      encIn->bIsIDR = HANTRO_FALSE;
      encIn->bIsIntraOnly = HANTRO_FALSE;
      idx = encIn->gopPicIdx + gopCfgOffset[encIn->gopSize];
      nextCodingType = gopCfg->pGopPicCfg[idx].codingType;
      if(forceIntraOnly) {
        nextCodingType = VCENC_INTRA_FRAME;
        encIn->bIsIntraOnly = HANTRO_TRUE;
        bIsCodingTypeChanged = HANTRO_TRUE;
      }
    }
  }
  gopCfg->id = encIn->gopPicIdx + (bIsLP ? gopCfg->gopCfgOffsetLP : gopCfgOffset[encIn->gopSize]);
  {
    // guess next rps needed for H.264 DPB management (MMO), assume gopSize unchanged.
    // gopSize change only occurs on adaptive GOP or tail GOP (lowdelay = 0).
    // then the next RPS is 1st of default RPS of some gopSize, which only includes the P frame of last GOP
    i32 next_poc = gopCfg->pGopPicCfg[gopCfg->id].poc;
    i32 gopPicIdx = (encIn->gopPicIdx + 1) % encIn->gopSize;
    i32 gopSize = encIn->gopSize;
    if (gopPicIdx == 0) {
      next_poc -= gopSize;
    }
    gopCfg->id_next = gopPicIdx + gopCfgOffset[gopSize];
    if (vcenc_instance->enableLeadingPictures) {
      next_poc = gopCfg->pGopPicCfg[gopCfg->id].poc;
      if(encIn->bIsIDR && encIn->gopSize > 1)
        gopCfg->id_next = encIn->gopSize - 1 + gopCfg->gopCfgOffsetLP;
      else if(bIsLP && encIn->gopPicIdx > 1)
        gopCfg->id_next = encIn->gopPicIdx - 1 + gopCfg->gopCfgOffsetLP;
      else if(bIsLP && encIn->gopPicIdx == 1)
        gopCfg->id_next = gopCfgOffset[gopSize];
      else if(gopPicIdx == 0)
        next_poc -= gopSize;
    }
    gopCfg->delta_poc_to_next =
        gopCfg->pGopPicCfg[gopCfg->id_next].poc - next_poc;

    if ((gopPicIdx == 0) && (gopCfg->delta_poc_to_next > 1) &&
        (idr_interval &&
         ((encIn->poc + gopCfg->delta_poc_to_next) >= idr_interval))) {
      i32 i32gopsize;
      i32gopsize = idr_interval - encIn->poc - 2;

      if (i32gopsize > 0) {
        int max_reduced_gop_size = gopCfg->gopLowdelay ? 1 : 4;
        if (i32gopsize > max_reduced_gop_size)
          i32gopsize = max_reduced_gop_size;

        idx = gopCfgOffset[i32gopsize];
        delta_poc_to_next = gopCfg->pGopPicCfg[idx].poc - cur_poc;

        gopCfg->id_next = gopPicIdx + gopCfgOffset[i32gopsize];
        gopCfg->delta_poc_to_next =
            gopCfg->pGopPicCfg[gopCfg->id_next].poc - next_poc;
      }
    }

    if (vcenc_instance->gdrDuration == 0 && idr_interval &&
        (encIn->poc + gopCfg->delta_poc_to_next) % idr_interval == 0)
      gopCfg->id_next = -1;
  }

  //only can work for fix gop
  if (0 /*vcenc_instance->lookaheadDepth == 0*/) {
    //Handle the first few frames for lowdelay
    if ((nextCodingType != VCENC_INTRA_FRAME)) {
      int i;
      VCEncGopPicConfig *cfg = &(gopCfg->pGopPicCfg[gopCfg->id]);
      for (i = 0; i < cfg->numRefPics; i++) {
        if ((encIn->poc + cfg->refPics[i].ref_pic) < 0) {
          int curCfgEnd = gopCfgOffset[encIn->gopSize] + encIn->gopSize;
          int cfgOffset = encIn->poc - 1;
          if ((curCfgEnd + cfgOffset) > gopCfg->size) cfgOffset = 0;

          gopCfg->id = curCfgEnd + cfgOffset;
          nextCodingType = gopCfg->pGopPicCfg[gopCfg->id].codingType;
          bIsCodingTypeChanged = HANTRO_TRUE;
          break;
        }
      }
    }
  }

  GenNextPicConfig(encIn, gopCfg->gopCfgOffset, i32LastPicPoc, vcenc_instance);
  if (bIsCodingTypeChanged == HANTRO_FALSE)
    nextCodingType = encIn->gopCurrPicConfig.codingType;
  if (nextCodingType == VCENC_INTRA_FRAME &&
      ((encIn->poc == 0) || (encIn->bIsIDR))) {
    // refresh IDR POC for !GDR
    if (!vcenc_instance->gdrDuration) encIn->poc = 0;
    encIn->last_idr_picture_cnt = encIn->picture_cnt;
  }

  encIn->codingType = (encIn->poc == 0) ? VCENC_INTRA_FRAME : nextCodingType;
  if (encIn->codingType == VCENC_INTRA_FRAME)
    encIn->picture_gopIdx = 0;

  return nextCodingType;
}

static VCEncRet vcencCheckRoiCtrl(struct vcenc_instance *pEncInst,
                                  const VCEncCodingCtrl *pCodeParams) {
  i32 has_roiarea_change;
  // get last coding ctrl parameter set
  EncCodingCtrlParam *pLastCodingCtrlParam =
      (EncCodingCtrlParam *)queue_head(&pEncInst->codingCtrl.codingCtrlQueue);
  VCEncCodingCtrl *pLastCodingCtrl = &pLastCodingCtrlParam->encCodingCtrl;
  if (pLastCodingCtrlParam) {
    has_roiarea_change =
        (pLastCodingCtrl->roi1Area.enable != pCodeParams->roi1Area.enable) ||
        (pLastCodingCtrl->roi2Area.enable != pCodeParams->roi2Area.enable) ||
        (pLastCodingCtrl->roi3Area.enable != pCodeParams->roi3Area.enable) ||
        (pLastCodingCtrl->roi4Area.enable != pCodeParams->roi4Area.enable) ||
        (pLastCodingCtrl->roi5Area.enable != pCodeParams->roi5Area.enable) ||
        (pLastCodingCtrl->roi6Area.enable != pCodeParams->roi6Area.enable) ||
        (pLastCodingCtrl->roi7Area.enable != pCodeParams->roi7Area.enable) ||
        (pLastCodingCtrl->roi8Area.enable != pCodeParams->roi8Area.enable);
  } else {
    has_roiarea_change =
        (pEncInst->roi1Enable != pCodeParams->roi1Area.enable) ||
        (pEncInst->roi2Enable != pCodeParams->roi2Area.enable) ||
        (pEncInst->roi3Enable != pCodeParams->roi3Area.enable) ||
        (pEncInst->roi4Enable != pCodeParams->roi4Area.enable) ||
        (pEncInst->roi5Enable != pCodeParams->roi5Area.enable) ||
        (pEncInst->roi6Enable != pCodeParams->roi6Area.enable) ||
        (pEncInst->roi7Enable != pCodeParams->roi7Area.enable) ||
        (pEncInst->roi8Enable != pCodeParams->roi8Area.enable);
  }

  if ((pEncInst->encStatus >= VCENCSTAT_START_STREAM) && (has_roiarea_change)) {
    struct container *c = get_container(pEncInst);
	if (c == NULL) {
      return VCENC_INVALID_ARGUMENT;
    }
    struct pps *p =
        (struct pps *)get_parameter_set(c, PPS_NUT, pEncInst->pps_id);
    if (p == NULL){
      return VCENC_INVALID_ARGUMENT;
    }

    if (p->cu_qp_delta_enabled_flag == 0) {
      return VCENC_INVALID_ARGUMENT;
    }
  }

  return VCENC_OK;
}

static void VCEncHEVCDnfSetParameters(struct vcenc_instance *inst,
                                      const VCEncCodingCtrl *pCodeParams) {
  regValues_s *regs = &inst->asic.regs;

  //wiener denoise paramter set
  regs->noiseReductionEnable = inst->uiNoiseReductionEnable =
      pCodeParams
          ->noiseReductionEnable;  //0: disable noise reduction; 1: enable noise reduction
  // regs->noiseLow =
  //     pCodeParams->noiseLow;  //0: use default value; valid value range: [1, 30]

#if USE_TOP_CTRL_DENOISE
  // inst->iNoiseL = pCodeParams->noiseLow << FIX_POINT_BIT_WIDTH;
  // regs->nrSigmaCur = inst->iSigmaCur = inst->iFirstFrameSigma =
  //     pCodeParams->firstFrameSigma << FIX_POINT_BIT_WIDTH;
//  printf("nrframe VCEncHEVCDnfSetParameters: init seq uiNoiseReductionEnable = %u, iNoiseL = %d, nrSigmaCur = %u, firstFrameSigma = %u\n", inst->uiNoiseReductionEnable, inst->iNoiseL, regs->nrSigmaCur,  pCodeParams->firstFrameSigma);

  // regs->nrSigmaYCur = inst->iSigmaYCur =
  //     pCodeParams->noiseSigmaY << FIX_POINT_BIT_WIDTH;
  // regs->nrSigmaUCur = inst->iSigmaUCur =
  //     pCodeParams->noiseSigmaU << FIX_POINT_BIT_WIDTH;
  // regs->nrSigmaVCur = inst->iSigmaVCur =
  //     pCodeParams->noiseSigmaV << FIX_POINT_BIT_WIDTH;
  // printf("pCodeParams->firstFrameSigma %d\n", pCodeParams->firstFrameSigma);
  // printf("pCodeParams->noiseSigmaEst %d\n", pCodeParams->noiseSigmaEst);
  // printf("pCodeParams->noiseSigmaY %d\n", pCodeParams->noiseSigmaY);
  // printf("pCodeParams->noiseReductionStrength_IntraY %d\n", pCodeParams->noiseReductionStrength_IntraY);
  // printf("pCodeParams->noiseReductionStrength_InterV %d\n", pCodeParams->noiseReductionStrength_InterV);
  // printf("%d %d %d\n", regs->noiseSigmaEst, inst->noiseSigmaEst, pCodeParams->noiseSigmaEst);
  // regs->noiseSigmaEst = inst->noiseSigmaEst = pCodeParams->noiseSigmaEst;  //0: disable noise reduction; 1: enable noise reduction

  // inst->iSigmaYCur = inst->iSigmaCur;
  // inst->iSigmaUCur = inst->iSigmaCur;
  // inst->iSigmaVCur = inst->iSigmaCur;
  // regs->noiseSigmaY = inst->noiseSigmaY = pCodeParams->noiseSigmaY;
  // regs->noiseSigmaU = inst->noiseSigmaU = pCodeParams->noiseSigmaU;
  // regs->noiseSigmaV = inst->noiseSigmaV = pCodeParams->noiseSigmaV;

  regs->noiseReductionStrength_IntraY = inst->noiseReductionStrength_IntraY = pCodeParams->noiseReductionStrength_IntraY;
  regs->noiseReductionStrength_IntraU = inst->noiseReductionStrength_IntraU = pCodeParams->noiseReductionStrength_IntraU;
  regs->noiseReductionStrength_IntraV = inst->noiseReductionStrength_IntraV = pCodeParams->noiseReductionStrength_IntraV;

  regs->noiseReductionStrength_InterY = inst->noiseReductionStrength_InterY = pCodeParams->noiseReductionStrength_InterY;
  regs->noiseReductionStrength_InterU = inst->noiseReductionStrength_InterU = pCodeParams->noiseReductionStrength_InterU;
  regs->noiseReductionStrength_InterV = inst->noiseReductionStrength_InterV = pCodeParams->noiseReductionStrength_InterV;

  regs->noiseReduction_ChromaMaxMV = inst->noiseReduction_ChromaMaxMV = pCodeParams->noiseReduction_ChromaMaxMV;

  // printf("3DNR DEBUG: nrframe VCEncHEVCDnfSetParameters: init seq, \nnoiseReductionEnable %u noiseSigmaEst %d \nnoiseSigma %d %d %d \nnoiseReductionStrength intra %d %d %d inter %d %d %d ChromaMaxMV %d\n", inst->uiNoiseReductionEnable, inst->noiseSigmaEst,inst->noiseSigmaY,inst->noiseSigmaU,inst->noiseSigmaV,inst->noiseReductionStrength_IntraY,inst->noiseReductionStrength_IntraU,inst->noiseReductionStrength_IntraV,inst->noiseReductionStrength_InterY,inst->noiseReductionStrength_InterU,inst->noiseReductionStrength_InterV, inst->noiseReduction_ChromaMaxMV);
  // printf("\n\n");

#endif
}

/**
 * Get any empty Task Slot from the task pool and initialize it.
 *
 * \return VCENC_OR    when task buffer is get from the pool successfully;
 * \return VCENC_ERROR when there's no empty slot in the task pool buffers;
 */
VCEncRet VceTaskCreate(struct vcenc_instance *inst, VCEncIn *in)
{
  VCEncRet ret = GetBufferFromPool(inst->task_pool, (void **)&in->internal);
  VCEncInfo *info;
  if (VCENC_OK != ret) {
    in->internal = NULL;
    APITRACEERR("No Buffer Slot from Task Pool. \n\n");
    return ret;
  }

  queue_put(&inst->task_queue, (struct node *)in->internal);

  info = (VCEncInfo *)in->internal;
  memset(info, 0, sizeof(VCEncInfo));

  info->picture_cnt = in->picture_cnt;
  info->ref_cnt = 1; //FIXME: add protection if used in multi-thread.

  return ret;
}

/**
 * Release the task slot and return it to task pool if it is not referenced
 * anymore.
 *
 * \return VCENC_OR    when task buffer is get from the pool successfully;
 * \return VCENC_ERROR when the task information is null;
 */
VCEncRet VceTaskRelease(struct vcenc_instance *inst, VCEncIn *in)
{
  VCEncInfo *info = (VCEncInfo *)in->internal;

  if (info==NULL)
    return VCENC_ERROR;

  info->ref_cnt--;  //FIXME: add protection if used in multi-thread.

  if (info->ref_cnt==0) {
    PutBufferToPool(inst->task_pool, &in->internal);
    in->internal = NULL;
  }

  return VCENC_OK;
}

i32 GetCtbRC_Models(struct vcenc_instance *vcenc_instance,i32 idxType) {

  u32 idx;
  i32 Fcnt = -1;
  idx = 1;

  vcencRateControl_s *rc = &(vcenc_instance->rateControl);
  i32 i;

  for(i=0; i<(2+MAX_CORE_NUM); i++){
    if(rc->ctbRateCtrl.models[i].idxType == idxType){
      if(rc->ctbRateCtrl.models[i].frameCnt > Fcnt){
        Fcnt =  rc->ctbRateCtrl.models[i].frameCnt;
        idx = i;
      }
    }
  }

  return idx;
}
void TileInfoConfig(struct vcenc_instance *vcenc_instance,
                    struct sw_picture *pic, u32 tileId,
                    VCEncPictureCodingType codingType, VCEncOut *pEncOut) {
  u32 leftTileWidth;
  u32 luma_off, chroma_off;
  asicData_s *asic = &vcenc_instance->asic;

  if (tileId >= HEVC_MAX_TILE_COLS) {
    APITRACEERR("tileId can not large than HEVC_MAX_TILE_COLS. \n\n");
  	return;
  }
  /* basic tile info */
  vcenc_instance->asic.regs.tileLeftStart =
      vcenc_instance->tileCtrl[tileId].tileLeft;
  vcenc_instance->asic.regs.tileWidthIn8 =
      MIN((vcenc_instance->tileCtrl[tileId].tileRight + 1 -
           vcenc_instance->tileCtrl[tileId].tileLeft) *
              (vcenc_instance->max_cu_size / 8),
          vcenc_instance->asic.regs.picWidth / 8 -
              vcenc_instance->tileCtrl[tileId].tileLeft *
                  (vcenc_instance->max_cu_size / 8));
  vcenc_instance->asic.regs.startTileIdx =
      vcenc_instance->tileCtrl[tileId].startTileIdx;

  /* output buffer. tile0 stream address is already configured considering some header size before */
  if (vcenc_instance->pass != 1 && tileId > 0) {
    vcenc_instance->asic.regs.outputStrmBase[0] =
        vcenc_instance->tileCtrl[tileId].busOutBuf;
    vcenc_instance->asic.regs.outputStrmSize[0] =
        vcenc_instance->tileCtrl[tileId].outBufSize;
  }

  /* input YUV buffer */
  vcenc_instance->asic.regs.input_luma_stride =
      vcenc_instance->tileCtrl[tileId].input_luma_stride;
  vcenc_instance->asic.regs.input_chroma_stride =
      vcenc_instance->tileCtrl[tileId].input_chroma_stride;
  vcenc_instance->asic.regs.inputLumBase =
      vcenc_instance->tileCtrl[tileId].inputLumBase;
  vcenc_instance->asic.regs.inputCbBase =
      vcenc_instance->tileCtrl[tileId].inputCbBase;
  vcenc_instance->asic.regs.inputCrBase =
      vcenc_instance->tileCtrl[tileId].inputCrBase;
  vcenc_instance->asic.regs.inputChromaBaseOffset =
      vcenc_instance->tileCtrl[tileId].inputChromaBaseOffset;
  vcenc_instance->asic.regs.inputLumaBaseOffset =
      vcenc_instance->tileCtrl[tileId].inputLumaBaseOffset;
  vcenc_instance->asic.regs.stabNextLumaBase =
      vcenc_instance->tileCtrl[tileId].stabNextLumaBase;

  /* NAL size table */
  vcenc_instance->asic.regs.sizeTblBase =
      (vcenc_instance->asic.regs.tiles_enabled_flag)
          ? (vcenc_instance->asic.sizeTbl[tileId].busAddress)
          : (asic->sizeTbl[vcenc_instance->jobCnt %
                           vcenc_instance->parallelCoreNum]
                 .busAddress);
  if (tileId == 0)
    vcenc_instance->asic.regs.sizeTblBase += pEncOut->numNalus * sizeof(u32);

  /* deblock+SAO+SAOPP left buffer data for multi-tile */
  if (vcenc_instance->tiles_enabled_flag && tileId > 0) {
    vcenc_instance->asic.regs.tileSyncReadBase =
        vcenc_instance->asic.regs.tileSyncWriteBase;
    vcenc_instance->asic.regs.mc_sync_l0_addr =
        vcenc_instance->asic.regs.mc_sync_rec_addr;
    if (tileId != vcenc_instance->num_tile_columns - 1) {
      vcenc_instance->asic.regs.tileSyncWriteBase +=
          (DEB_TILE_SYNC_SIZE + SAODEC_TILE_SYNC_SIZE + SAOPP_TILE_SYNC_SIZE) *
          ((vcenc_instance->height + 63) / 64);
      vcenc_instance->asic.regs.mc_sync_rec_addr += 4;
    }
  }

  /* Ctb bits output */
  if (asic->regs.enableOutputCtbBits) {
    asic->regs.ctbBitsDataBase = pic->ctbBitsDataBase;
    /* for multi-tile case, add the address offset with alignment */
    if (vcenc_instance->num_tile_columns > 1) {
      i32 offset = vcenc_instance->ctbPerCol *
                   vcenc_instance->asic.regs.tileLeftStart * 2;
      offset = (offset + vcenc_instance->ref_alignment - 1) /
               vcenc_instance->ref_alignment * vcenc_instance->ref_alignment;
      asic->regs.ctbBitsDataBase += offset;
    }
  }

  /* ctbRc config */
  asic->regs.targetPicSize = vcenc_instance->rateControl.targetPicSize;
  if (asic->regs.asicCfg->ctbRcVersion &&
      IS_CTBRC_FOR_BITRATE(vcenc_instance->rateControl.ctbRc)) {
    // New ctb rc testing
    vcencRateControl_s *rc = &(vcenc_instance->rateControl);

    float f_tolCtbRc = (codingType == VCENC_INTRA_FRAME) ? rc->tolCtbRcIntra
                                                         : rc->tolCtbRcInter;
    const i32 tolScale = 1 << TOL_CTB_RC_FIX_POINT;
    i32 tolCtbRc = (i32)(f_tolCtbRc * tolScale);

    if (tolCtbRc >= 0) {
      /* tolCtbRc >= 0: ctbRc mode 2 is set by user */
      i32 baseSize = rc->virtualBuffer.bitPerPic;
      if ((codingType != VCENC_INTRA_FRAME) && (rc->targetPicSize < baseSize))
        baseSize = rc->targetPicSize;

      i32 minDeltaSize = rcCalculate(baseSize, tolCtbRc, (tolScale + tolCtbRc));
      i32 maxDeltaSize = rcCalculate(baseSize, tolCtbRc, tolScale);

      // set minPicSize/maxPicSize based on baseSize for I frame/P only, for better rc control
      if (codingType != VCENC_INTRA_FRAME && vcenc_instance->rateControl.hierarchial_bit_allocation_GOP_size > 1)
        baseSize = rc->targetPicSize;
      asic->regs.minPicSize = MAX(0, (baseSize - minDeltaSize));
      asic->regs.maxPicSize = baseSize + maxDeltaSize;

      asic->regs.minPicSize = MAX(asic->regs.minPicSize, rc->minPicSize);
      if (rc->maxPicSize > 0)
        asic->regs.maxPicSize = MIN(asic->regs.maxPicSize, rc->maxPicSize);
    } else {
      /* ctbRc mode 2 is enabled by RC forcibly for cpb control */
      asic->regs.minPicSize = asic->regs.maxPicSize = 0;

      if (rc->minPicSize > 0) asic->regs.minPicSize = rc->minPicSize;

      if (rc->maxPicSize > 0)
        asic->regs.maxPicSize = rc->maxPicSize;

    }

    enum slice_type idxType = rc->predLayerId < 3 ? rc->predLayerId : B_SLICE;
    // if (rc->ctbRateCtrl.models[idxType].started == 0) {
    //   if ((idxType == B_SLICE) && rc->ctbRateCtrl.models[P_SLICE].started)
    //     idxType = P_SLICE;
    //   else if (rc->ctbRateCtrl.models[I_SLICE].started)
    //     idxType = I_SLICE;
    // }
    asic->regs.targetPicSize = MIN(rc->targetPicSize, asic->regs.maxPicSize);
    asic->regs.ctbRcRowFactor = rc->ctbRateCtrl.rowFactor;
    asic->regs.ctbRcQpStep = rc->ctbRateCtrl.qpStep;
    asic->regs.ctbRcRowQpDeltaRange = rc->ctbRateCtrl.ctbRcRowQpDeltaRange;
    asic->regs.ctbRcMemAddrCur = rc->ctbRateCtrl.Cur[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum].ctbMemCurAddr;

    i32 idxModels = GetCtbRC_Models(vcenc_instance,idxType);
    asic->regs.ctbRcModelParamMin = rc->ctbRateCtrl.models[idxModels].xMin;

    asic->regs.ctbRcModelParam0 = rc->ctbRateCtrl.models[idxModels].x0;
    asic->regs.ctbRcModelParam1 = rc->ctbRateCtrl.models[idxModels].x1;
    asic->regs.prevPicLumMad = rc->ctbRateCtrl.models[idxModels].preFrameMad;
    asic->regs.ctbRcMemAddrPre = rc->ctbRateCtrl.models[idxModels].ctbMemPreAddr;
    rc->ctbRateCtrl.Cur[vcenc_instance->jobCnt%vcenc_instance->parallelCoreNum].ctbRcModelsIdx = idxModels;
    //rc->ctbRateCtrl.models[idxModels].frameCnt = vcenc_instance->frameCnt;
    rc->ctbRateCtrl.models[idxModels].idxType = idxType;
    rc->ctbRateCtrl.models[idxModels].refCount = rc->ctbRateCtrl.models[idxModels].refCount + 1;

    /* for multi-tile case, add the address offset with alignment */
    if (vcenc_instance->num_tile_columns > 1) {
      i32 offset =
          vcenc_instance->ctbPerCol * vcenc_instance->asic.regs.tileLeftStart;
      offset = (offset + vcenc_instance->ref_alignment - 1) /
               vcenc_instance->ref_alignment * vcenc_instance->ref_alignment;
      asic->regs.ctbRcMemAddrCur += offset;
      asic->regs.ctbRcMemAddrPre += offset;
    }

    asic->regs.ctbRcPrevMadValid =
        rc->ctbRateCtrl.models[idxModels].started ? 1 : 0;
    asic->regs.ctbRcDelay = IS_H264(vcenc_instance->codecFormat) ? 5 : 2;
    if (pic->sliceInst->type != I_SLICE) {
      i32 bpBlk = rc->targetPicSize / (rc->picArea >> 6);
      i32 bpTh = 16;
      if (bpBlk >= bpTh)
        asic->regs.ctbRcDelay = IS_H264(vcenc_instance->codecFormat) ? 7 : 3;
    }
    if (vcenc_instance->num_tile_columns > 1) {
      i32 tileWidthInCtb = vcenc_instance->tileCtrl[tileId].tileRight + 1 -
                           vcenc_instance->tileCtrl[tileId].tileLeft;

      asic->regs.ctbRcRowFactor = asic->regs.ctbRcRowFactor *
                                  vcenc_instance->ctbPerRow / tileWidthInCtb;
      asic->regs.ctbRcRowFactor = MIN(0xffff, asic->regs.ctbRcRowFactor);
      asic->regs.targetPicSize =
          asic->regs.targetPicSize * tileWidthInCtb / vcenc_instance->ctbPerRow;
      asic->regs.minPicSize =
          asic->regs.minPicSize * tileWidthInCtb / vcenc_instance->ctbPerRow;
      asic->regs.maxPicSize =
          asic->regs.maxPicSize * tileWidthInCtb / vcenc_instance->ctbPerRow;
      asic->regs.prevPicLumMad = vcenc_instance->tileCtrl[tileId].ctbRcFrameMad;
    }
  }
  /* cuInfo config. only cuinfo_v1 is supported when multi-tile. */
  vcenc_instance->asic.regs.cuInfoTableBase =
      vcenc_instance->tileCtrl[tileId].cuinfoTableBase;
  vcenc_instance->asic.regs.cuInfoDataBase =
      vcenc_instance->tileCtrl[tileId].cuinfoDataBase;
}

u32 TileTotalStreamSize(struct vcenc_instance *vcenc_instance) {
  u32 totalStreamSize = 0, tileId;
  for (tileId = 0; tileId < vcenc_instance->asic.regs.num_tile_columns;
       tileId++) {
    totalStreamSize =
        totalStreamSize + vcenc_instance->tileCtrl[tileId].streamSize;
  }
  return totalStreamSize;
}

void TileInfoCollect(struct vcenc_instance *vcenc_instance, u32 tileId,
                     u32 numNalu) {
  asicData_s *asic = &vcenc_instance->asic;

  vcenc_instance->tileCtrl[tileId].streamSize = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
  vcenc_instance->tileCtrl[tileId].numNalu = numNalu;

#ifdef CTBRC_STRENGTH
  vcenc_instance->tileCtrl[tileId].sumOfQP =
      EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                              HWIF_ENC_QP_SUM) |
      (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                               HWIF_ENC_QP_SUM_MSB)
       << 26);
  vcenc_instance->tileCtrl[tileId].sumOfQPNumber =
      EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                              HWIF_ENC_QP_NUM) |
      (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                               HWIF_ENC_QP_NUM_MSB)
       << 20);
  vcenc_instance->tileCtrl[tileId].picComplexity =
      EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                              HWIF_ENC_PIC_COMPLEXITY) |
      (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                               HWIF_ENC_PIC_COMPLEXITY_MSB)
       << 23);
#endif

  vcenc_instance->tileCtrl[tileId].intraCu8Num =
      EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                              HWIF_ENC_INTRACU8NUM) |
      (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                               HWIF_ENC_INTRACU8NUM_MSB)
       << 20);
  vcenc_instance->tileCtrl[tileId].skipCu8Num =
      EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                              HWIF_ENC_SKIPCU8NUM) |
      (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                               HWIF_ENC_SKIPCU8NUM_MSB)
       << 20);
  vcenc_instance->tileCtrl[tileId].PBFrame4NRdCost = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_PBFRAME4NRDCOST);

  vcenc_instance->tileCtrl[tileId].SSEDivide256 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSE_DIV_256);
  vcenc_instance->tileCtrl[tileId].lumSSEDivide256 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_LUM_SSE_DIV_256);
  vcenc_instance->tileCtrl[tileId].cbSSEDivide64 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_CB_SSE_DIV_64);
  vcenc_instance->tileCtrl[tileId].crSSEDivide64 = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_CR_SSE_DIV_64);

  vcenc_instance->tileCtrl[tileId].ssim_numerator_y =
      (i32)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                   HWIF_ENC_SSIM_Y_NUMERATOR_MSB);
  vcenc_instance->tileCtrl[tileId].ssim_numerator_u =
      (i32)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                   HWIF_ENC_SSIM_U_NUMERATOR_MSB);
  vcenc_instance->tileCtrl[tileId].ssim_numerator_v =
      (i32)EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                   HWIF_ENC_SSIM_V_NUMERATOR_MSB);
  vcenc_instance->tileCtrl[tileId].ssim_denominator_y = EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_DENOMINATOR);
  vcenc_instance->tileCtrl[tileId].ssim_denominator_uv =
      EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                              HWIF_ENC_SSIM_UV_DENOMINATOR);

  vcenc_instance->tileCtrl[tileId].ssim_numerator_y =
      vcenc_instance->tileCtrl[tileId].ssim_numerator_y << 32;
  vcenc_instance->tileCtrl[tileId].ssim_numerator_u =
      vcenc_instance->tileCtrl[tileId].ssim_numerator_u << 32;
  vcenc_instance->tileCtrl[tileId].ssim_numerator_v =
      vcenc_instance->tileCtrl[tileId].ssim_numerator_v << 32;

  vcenc_instance->tileCtrl[tileId].ssim_numerator_y |= (i64)EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_Y_NUMERATOR_LSB);
  vcenc_instance->tileCtrl[tileId].ssim_numerator_u |= (i64)EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_U_NUMERATOR_LSB);
  vcenc_instance->tileCtrl[tileId].ssim_numerator_v |= (i64)EncAsicGetRegisterValue(
      asic->ewl, asic->regs.regMirror, HWIF_ENC_SSIM_V_NUMERATOR_LSB);

  if ((asic->regs.asicCfg->ctbRcVersion) && (asic->regs.rcRoiEnable & 0x08)) {
    vcenc_instance->tileCtrl[tileId].ctbRcX0 = EncAsicGetRegisterValue(
        asic->ewl, asic->regs.regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM0);
    vcenc_instance->tileCtrl[tileId].ctbRcX1 = EncAsicGetRegisterValue(
        asic->ewl, asic->regs.regMirror, HWIF_ENC_CTB_RC_MODEL_PARAM1);
    vcenc_instance->tileCtrl[tileId].ctbRcFrameMad = EncAsicGetRegisterValue(
        asic->ewl, asic->regs.regMirror, HWIF_ENC_PREV_PIC_LUM_MAD);
    vcenc_instance->tileCtrl[tileId].ctbRcQpSum =
        EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                HWIF_ENC_CTB_QP_SUM_FOR_RC) |
        (EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                                 HWIF_ENC_CTB_QP_SUM_FOR_RC_MSB)
         << 24);
  }
}

u32 FindIndexBywaitCoreJobid(struct vcenc_instance *vcenc_instance,
                             u32 waitCoreJobid) {
  u32 i;
  for (i = 0; i < vcenc_instance->num_tile_columns; i++) {
    if (waitCoreJobid == vcenc_instance->tileCtrl[i].job_id) return i;
  }
  return 0;
}

void FillVCEncout(struct vcenc_instance *vcenc_instance, VCEncOut *pEncOut) {
  u32 i;
  u32 mulitTileEnable = (vcenc_instance->num_tile_columns > 1);
  /* output the statistics data of cus. */
  pEncOut->cuStatis.intraCu8Num =
      mulitTileEnable ? 0 : vcenc_instance->asic.regs.intraCu8Num;
  pEncOut->cuStatis.skipCu8Num =
      mulitTileEnable ? 0 : vcenc_instance->asic.regs.skipCu8Num;
  pEncOut->cuStatis.PBFrame4NRdCost =
      mulitTileEnable ? 0 : vcenc_instance->asic.regs.PBFrame4NRdCost;

  if (mulitTileEnable) {
    vcenc_instance->asic.regs.SSEDivide256 = 0;
    vcenc_instance->asic.regs.lumSSEDivide256 = 0;
    vcenc_instance->asic.regs.cbSSEDivide64 = 0;
    vcenc_instance->asic.regs.crSSEDivide64 = 0;

    i64 ssim_numerator_y = 0;
    i64 ssim_numerator_u = 0;
    i64 ssim_numerator_v = 0;
    u32 ssim_denominator_y = 0;
    u32 ssim_denominator_uv = 0;

    for (i = 0; i < vcenc_instance->num_tile_columns; i++) {
      /* accumulate statistics data of cus when multi tile */
      pEncOut->cuStatis.intraCu8Num += vcenc_instance->tileCtrl[i].intraCu8Num;
      pEncOut->cuStatis.skipCu8Num += vcenc_instance->tileCtrl[i].skipCu8Num;
      pEncOut->cuStatis.PBFrame4NRdCost +=
          vcenc_instance->tileCtrl[i].PBFrame4NRdCost;

      /* accumulate sse when multi-tile*/
      vcenc_instance->asic.regs.SSEDivide256 +=
          vcenc_instance->tileCtrl[i].SSEDivide256;
      vcenc_instance->asic.regs.lumSSEDivide256 +=
          vcenc_instance->tileCtrl[i].lumSSEDivide256;
      vcenc_instance->asic.regs.cbSSEDivide64 +=
          vcenc_instance->tileCtrl[i].cbSSEDivide64;
      vcenc_instance->asic.regs.crSSEDivide64 +=
          vcenc_instance->tileCtrl[i].crSSEDivide64;

      /* accumulate data about ssim when multi-tile*/
      if (vcenc_instance->asic.regs.asicCfg->ssimSupport &&
          vcenc_instance->asic.regs.ssim) {
        ssim_numerator_y += vcenc_instance->tileCtrl[i].ssim_numerator_y;
        ssim_numerator_u += vcenc_instance->tileCtrl[i].ssim_numerator_u;
        ssim_numerator_v += vcenc_instance->tileCtrl[i].ssim_numerator_v;
        ssim_denominator_y += vcenc_instance->tileCtrl[i].ssim_denominator_y;
        ssim_denominator_uv += vcenc_instance->tileCtrl[i].ssim_denominator_uv;
      }
    }
    /* calculate psnr in frame level when multi-tile*/
    CalculatePSNR(
        vcenc_instance, pEncOut,
        vcenc_instance->width - (vcenc_instance->num_tile_columns - 1) * 8);

    /* calculate ssim in frame level when multi-tile*/
    if (vcenc_instance->asic.regs.asicCfg->ssimSupport &&
        vcenc_instance->asic.regs.ssim) {
      CalculateSSIM(vcenc_instance, pEncOut, ssim_numerator_y, ssim_numerator_u,
                    ssim_numerator_v, ssim_denominator_y, ssim_denominator_uv);
    }
  }
}

/**
 * convert VCEncVideoCodecFormat to CodingType used for Register HWIF_ENC_MODE
 *
 * \return 0xffff if codecFormat is not valid;
 */
u32 EncGetCodingMode(VCEncVideoCodecFormat codecFormat)
{
  if (IS_H264(codecFormat))
    return ASIC_H264;
  else if (IS_HEVC(codecFormat))
    return ASIC_HEVC;
  else if (IS_VP9(codecFormat))
    return ASIC_VP9;
  else if (IS_AV1(codecFormat))
    return ASIC_AV1;
  else {
    ASSERT(0);
    APITRACEERR("Error: Invalid codecFormat %d\n", codecFormat);
    return 0xffff;
  }
}

/*------------------------------------------------------------------------------
    Function name : EncMaxSliceSize
    Description   : get the limited slice size when it is greater than the limits.
    Return type   : u32 - limited sliceSize
    Argument      : pEncInst - encoder instance
    Argument      : sliceSize - slice size
------------------------------------------------------------------------------*/
static u32 EncMaxSliceSize(struct vcenc_instance *pEncInst, u32 sliceSize) {
  u32 sliceSize_temp = sliceSize;
  u32 client_type = VCEncGetClientType(pEncInst->codecFormat);
  const EWLHwConfig_t *cfg = EncAsicGetAsicConfig(client_type, pEncInst->ctx);
  if (!cfg) {
    printf("EncMaxSliceSize: ERROR Null argument\n");
    return 0;
  }

  /* limit slice size*/
  if (sliceSize_temp > ((1 << cfg->encSliceSizeBits) - 1))
    sliceSize_temp = (1 << cfg->encSliceSizeBits) - 1;

  return sliceSize_temp;
}

/*------------------------------------------------------------------------------
    Function name : EncUpdateCodingCtrlParam
    Description   : [pass2/single pass]update coding ctrl parameters matched with current frame into instance
    Return type   : void
    Argument      : inst - encoder instance
    Argument      : pCodingCtrlParam - coding ctrl
    Argumnet      : picCnt - current picture cnt
------------------------------------------------------------------------------*/
void EncUpdateCodingCtrlParam(struct vcenc_instance *pEncInst,
                              EncCodingCtrlParam *pCodingCtrlParam,
                              const i32 picCnt) {
  // if pCodingCtrlParam==NULL, there's no coding ctrl parameters need to be updated into instance.
  if (!pEncInst || !pCodingCtrlParam) return;
  u32 i;
  VCEncCodingCtrl *pCodeParams = &pCodingCtrlParam->encCodingCtrl;
  regValues_s *regs = &pEncInst->asic.regs;
  bool bAdvanceHW =
      (pEncInst->asic.regs.asicCfg->roiMapVersion == 2) &&
      ((HW_ID_MAJOR_NUMBER(pEncInst->asic.regs.asicCfg->hw_asic_id) >= 0x82) ||
       (HW_PRODUCT_VC9000(pEncInst->asic.regs.asicCfg->hw_asic_id)) ||
       HW_PRODUCT_SYSTEM6010(pEncInst->asic.regs.asicCfg->hw_asic_id) ||
       (HW_PRODUCT_VC9000LE(pEncInst->asic.regs.asicCfg->hw_asic_id)));

  /*for parameters need to be enforced in encode order,
    be enforce if current frame is the frame when this set of parameters setted,
    otherwise, keep the value in instance*/
  u32 gdrDuration = pEncInst->gdrDuration;
  //whether need to update parameters enforced in encode order
  u32 bUpdateEncOrderEnforceParam = (picCnt == pCodingCtrlParam->startPicCnt ||
                                     VCENCSTAT_INIT == pEncInst->encStatus)
                                        ? HANTRO_TRUE
                                        : HANTRO_FALSE;
  //whether need to update parameters enforced in input order
  u32 bUpdateInputOrderEnforceParam =
      (pEncInst->codingCtrl.pCodingCtrlParam != pCodingCtrlParam ||
       VCENCSTAT_INIT == pEncInst->encStatus)
          ? HANTRO_TRUE
          : HANTRO_FALSE;

  if (!bUpdateInputOrderEnforceParam &&
      !bUpdateEncOrderEnforceParam)  //don't need to update parameters
    goto end;

  pEncInst->gopMaxBSize = pCodeParams->gopMaxBSize;
  /*update parameters in instance*/
  /*update parameters enforced in input order*/
  if (bUpdateInputOrderEnforceParam) {
    //update codingctrl pointer
    pEncInst->codingCtrl.pCodingCtrlParam = pCodingCtrlParam;

    pEncInst->featureToSupport.deNoiseEnabled =
        (pCodeParams->noiseReductionEnable == 1) ? 1 : 0;

    regs->meAssignedVertRange = pCodeParams->meVertSearchRange >> 3;

    pEncInst->itu_t_t35 = pCodeParams->itu_t_t35;

    pEncInst->write_once_HDR10 = pCodeParams->write_once_HDR10;
    if (pEncInst->Hdr10Display.hdr10_display_enable == (u8)HDR10_NOCFGED) {
      pEncInst->Hdr10Display = pCodeParams->Hdr10Display;
      pEncInst->Hdr10Display.hdr10_display_enable =
          pCodeParams->Hdr10Display.hdr10_display_enable ? ((u8)HDR10_CFGED)
                                                         : ((u8)HDR10_NOCFGED);
    }

    pEncInst->vuiColorDescription = pCodeParams->vuiColorDescription;

    if (pEncInst->Hdr10LightLevel.hdr10_lightlevel_enable ==
        (u8)HDR10_NOCFGED) {
      pEncInst->Hdr10LightLevel = pCodeParams->Hdr10LightLevel;
      pEncInst->Hdr10LightLevel.hdr10_lightlevel_enable =
          pCodeParams->Hdr10LightLevel.hdr10_lightlevel_enable
              ? ((u8)HDR10_CFGED)
              : ((u8)HDR10_NOCFGED);
    }

    pEncInst->vuiVideoSignalTypePresentFlag =
        pCodeParams->vuiVideoSignalTypePresentFlag;
    pEncInst->vuiVideoFormat = pCodeParams->vuiVideoFormat;

    pEncInst->sarWidth = pCodeParams->sampleAspectRatioWidth;
    pEncInst->sarHeight = pCodeParams->sampleAspectRatioHeight;
    pEncInst->vuiVideoFullRange = pCodeParams->vuiVideoFullRange;

    pEncInst->RpsInSliceHeader =
        IS_H264(pEncInst->codecFormat) ? 0 : pCodeParams->RpsInSliceHeader;

    if (pCodeParams->sliceSize) {
      /* Multi-slice mode is not supported by VCMD, so need to disable (bypass) VCMD */
      EWLSetVCMDMode(pEncInst->asic.ewl, 0);
      regs->bVCMDEnable = EWLGetVCMDMode(pEncInst->asic.ewl) ? true : false;
    }

    regs->sliceSize = (pCodeParams->sliceSize <= MAX_SLICE_SIZE_V0)
                          ? pCodeParams->sliceSize
                          : EncMaxSliceSize(pEncInst, pCodeParams->sliceSize);

    regs->sliceNum =
        (regs->sliceSize == 0)
            ? 1
            : ((pEncInst->ctbPerCol + (regs->sliceSize - 1)) / regs->sliceSize);

    if ((IS_AV1(pEncInst->codecFormat) || IS_VP9(pEncInst->codecFormat)) &&
        regs->sliceNum > 1) {
      APITRACEERR(
          "EncUpdateCodingCtrlParam: WARNING No multi slice support in AV1 or "
          "VP9\n");
      regs->sliceSize = 0;
      regs->sliceNum = 1;
    }

    /* limit slice num to make it smaller than MAX_SLICE_NUM. If slice size is limited by
     * EncMaxSliceSize() previously, it will not go to following do-while loop due to the
     * corresponding slice num will not be greater than 9, which less than MAX_SLICE_NUM.
     */
    if (regs->sliceNum > MAX_SLICE_NUM) {
      do {
        regs->sliceSize++;
        regs->sliceNum =
            (pEncInst->ctbPerCol + (regs->sliceSize - 1)) / regs->sliceSize;
      } while (regs->sliceNum > MAX_SLICE_NUM);
    }

    pEncInst->enableScalingList = pCodeParams->enableScalingList;

    if (pEncInst->pass == 0 && (pCodeParams->roiMapDeltaQpEnable ||
                                pCodeParams->RoimapCuCtrl_enable)) {
      i32 log2_ctu_size = IS_H264(pEncInst->codecFormat) ? 4 : 6;
      pEncInst->log2_qp_size =
          CLIP3(3, log2_ctu_size, (6 - pCodeParams->roiMapDeltaQpBlockUnit));
    }

    /** set qp delta */
    if (pEncInst->aif.enable) {
      pEncInst->aif.qp_delta = pCodeParams->aifQpDelta;
    }

    /* Set CIR, intra forcing and ROI parameters */
    regs->cirStart = pCodeParams->cirStart;
    regs->cirInterval = pCodeParams->cirInterval;

    pEncInst->pcm_enabled_flag =
        pCodeParams->pcm_enabled_flag || (pCodeParams->RoimapCuCtrl_ver > 3);
    regs->ipcmFilterDisable = pEncInst->pcm_loop_filter_disabled_flag =
        pCodeParams->pcm_loop_filter_disabled_flag;
    if (pEncInst->asic.regs.asicCfg->roiMapVersion == 2 &&
        pCodeParams->RoiQpDelta_ver == 2 &&
        pCodeParams->skipMapEnable == 1 &&
        pCodeParams->enableSao == 1) {
        regs->ipcmFilterDisable = 0;
        pEncInst->pcm_loop_filter_disabled_flag = 0;
    }
    regs->ipcmMapEnable = pCodeParams->ipcmMapEnable;

    if (pCodeParams->rect0.enable) {
      regs->sse0Top = pCodeParams->rect0.top;
      regs->sse0Left = pCodeParams->rect0.left;
      regs->sse0Bottom = pCodeParams->rect0.bottom;
      regs->sse0Right = pCodeParams->rect0.right;
      pEncInst->sse0Enable = 1;
    } else {
      regs->sse0Top = regs->sse0Left = regs->sse0Bottom = regs->sse0Right =
          INVALID_POS;
      pEncInst->sse0Enable = 0;
    }
    if (pCodeParams->rect1.enable) {
      regs->sse1Top = pCodeParams->rect1.top;
      regs->sse1Left = pCodeParams->rect1.left;
      regs->sse1Bottom = pCodeParams->rect1.bottom;
      regs->sse1Right = pCodeParams->rect1.right;
      pEncInst->sse1Enable = 1;
    } else {
      regs->sse2Top = regs->sse2Left = regs->sse2Bottom = regs->sse2Right =
          INVALID_POS;
      pEncInst->sse2Enable = 0;
    }
    if (pCodeParams->rect2.enable) {
      regs->sse2Top = pCodeParams->rect2.top;
      regs->sse2Left = pCodeParams->rect2.left;
      regs->sse2Bottom = pCodeParams->rect2.bottom;
      regs->sse2Right = pCodeParams->rect2.right;
      pEncInst->sse2Enable = 1;
    } else {
      regs->sse2Top = regs->sse2Left = regs->sse2Bottom = regs->sse2Right =
          INVALID_POS;
      pEncInst->sse2Enable = 0;
    }
    if (pCodeParams->rect3.enable) {
      regs->sse3Top = pCodeParams->rect3.top;
      regs->sse3Left = pCodeParams->rect3.left;
      regs->sse3Bottom = pCodeParams->rect3.bottom;
      regs->sse3Right = pCodeParams->rect3.right;
      pEncInst->sse3Enable = 1;
    } else {
      regs->sse3Top = regs->sse3Left = regs->sse3Bottom = regs->sse3Right =
          INVALID_POS;
      pEncInst->sse3Enable = 0;
    }
    if (pCodeParams->rect4.enable) {
      regs->sse4Top = pCodeParams->rect4.top;
      regs->sse4Left = pCodeParams->rect4.left;
      regs->sse4Bottom = pCodeParams->rect4.bottom;
      regs->sse4Right = pCodeParams->rect4.right;
      pEncInst->sse4Enable = 1;
    } else {
      regs->sse4Top = regs->sse4Left = regs->sse4Bottom = regs->sse4Right =
          INVALID_POS;
      pEncInst->sse4Enable = 0;
    }
    if (pCodeParams->rect5.enable) {
      regs->sse5Top = pCodeParams->rect5.top;
      regs->sse5Left = pCodeParams->rect5.left;
      regs->sse5Bottom = pCodeParams->rect5.bottom;
      regs->sse5Right = pCodeParams->rect5.right;
      pEncInst->sse5Enable = 1;
    } else {
      regs->sse5Top = regs->sse5Left = regs->sse5Bottom = regs->sse5Right =
          INVALID_POS;
      pEncInst->sse5Enable = 0;
    }
    if (pCodeParams->rect6.enable) {
      regs->sse6Top = pCodeParams->rect6.top;
      regs->sse6Left = pCodeParams->rect6.left;
      regs->sse6Bottom = pCodeParams->rect6.bottom;
      regs->sse6Right = pCodeParams->rect6.right;
      pEncInst->sse6Enable = 1;
    } else {
      regs->sse6Top = regs->sse6Left = regs->sse6Bottom = regs->sse6Right =
          INVALID_POS;
      pEncInst->sse6Enable = 0;
    }
    if (pCodeParams->rect7.enable) {
      regs->sse7Top = pCodeParams->rect7.top;
      regs->sse7Left = pCodeParams->rect7.left;
      regs->sse7Bottom = pCodeParams->rect7.bottom;
      regs->sse7Right = pCodeParams->rect7.right;
      pEncInst->sse7Enable = 1;
    } else {
      regs->sse7Top = regs->sse7Left = regs->sse7Bottom = regs->sse7Right =
          INVALID_POS;
      pEncInst->sse7Enable = 0;
    }

    if (pCodeParams->roi2Area.enable) {
      regs->roi2Top = pCodeParams->roi2Area.top;
      regs->roi2Left = pCodeParams->roi2Area.left;
      regs->roi2Bottom = pCodeParams->roi2Area.bottom;
      regs->roi2Right = pCodeParams->roi2Area.right;
      pEncInst->roi2Enable = 1;
    } else {
      regs->roi2Top = regs->roi2Left = regs->roi2Bottom = regs->roi2Right =
          INVALID_POS;
      pEncInst->roi2Enable = 0;
    }
    if (pCodeParams->roi3Area.enable) {
      regs->roi3Top = pCodeParams->roi3Area.top;
      regs->roi3Left = pCodeParams->roi3Area.left;
      regs->roi3Bottom = pCodeParams->roi3Area.bottom;
      regs->roi3Right = pCodeParams->roi3Area.right;
      pEncInst->roi3Enable = 1;
    } else {
      regs->roi3Top = regs->roi3Left = regs->roi3Bottom = regs->roi3Right =
          INVALID_POS;
      pEncInst->roi3Enable = 0;
    }
    if (pCodeParams->roi4Area.enable) {
      regs->roi4Top = pCodeParams->roi4Area.top;
      regs->roi4Left = pCodeParams->roi4Area.left;
      regs->roi4Bottom = pCodeParams->roi4Area.bottom;
      regs->roi4Right = pCodeParams->roi4Area.right;
      pEncInst->roi4Enable = 1;
    } else {
      regs->roi4Top = regs->roi4Left = regs->roi4Bottom = regs->roi4Right =
          INVALID_POS;
      pEncInst->roi4Enable = 0;
    }
    if (pCodeParams->roi5Area.enable) {
      regs->roi5Top = pCodeParams->roi5Area.top;
      regs->roi5Left = pCodeParams->roi5Area.left;
      regs->roi5Bottom = pCodeParams->roi5Area.bottom;
      regs->roi5Right = pCodeParams->roi5Area.right;
      pEncInst->roi5Enable = 1;
    } else {
      regs->roi5Top = regs->roi5Left = regs->roi5Bottom = regs->roi5Right =
          INVALID_POS;
      pEncInst->roi5Enable = 0;
    }
    if (pCodeParams->roi6Area.enable) {
      regs->roi6Top = pCodeParams->roi6Area.top;
      regs->roi6Left = pCodeParams->roi6Area.left;
      regs->roi6Bottom = pCodeParams->roi6Area.bottom;
      regs->roi6Right = pCodeParams->roi6Area.right;
      pEncInst->roi6Enable = 1;
    } else {
      regs->roi6Top = regs->roi6Left = regs->roi6Bottom = regs->roi6Right =
          INVALID_POS;
      pEncInst->roi6Enable = 0;
    }
    if (pCodeParams->roi7Area.enable) {
      regs->roi7Top = pCodeParams->roi7Area.top;
      regs->roi7Left = pCodeParams->roi7Area.left;
      regs->roi7Bottom = pCodeParams->roi7Area.bottom;
      regs->roi7Right = pCodeParams->roi7Area.right;
      pEncInst->roi7Enable = 1;
    } else {
      regs->roi7Top = regs->roi7Left = regs->roi7Bottom = regs->roi7Right =
          INVALID_POS;
      pEncInst->roi7Enable = 0;
    }
    if (pCodeParams->roi8Area.enable) {
      regs->roi8Top = pCodeParams->roi8Area.top;
      regs->roi8Left = pCodeParams->roi8Area.left;
      regs->roi8Bottom = pCodeParams->roi8Area.bottom;
      regs->roi8Right = pCodeParams->roi8Area.right;
      pEncInst->roi8Enable = 1;
    } else {
      regs->roi8Top = regs->roi8Left = regs->roi8Bottom = regs->roi8Right =
          INVALID_POS;
      pEncInst->roi8Enable = 0;
    }

    regs->roi1DeltaQp = -pCodeParams->roi1DeltaQp;
    regs->roi2DeltaQp = -pCodeParams->roi2DeltaQp;
    regs->roi1Qp = pCodeParams->roi1Qp;
    regs->roi2Qp = pCodeParams->roi2Qp;
    regs->roi3DeltaQp = -pCodeParams->roi3DeltaQp;
    regs->roi4DeltaQp = -pCodeParams->roi4DeltaQp;
    regs->roi5DeltaQp = -pCodeParams->roi5DeltaQp;
    regs->roi6DeltaQp = -pCodeParams->roi6DeltaQp;
    regs->roi7DeltaQp = -pCodeParams->roi7DeltaQp;
    regs->roi8DeltaQp = -pCodeParams->roi8DeltaQp;
    regs->roi3Qp = pCodeParams->roi3Qp;
    regs->roi4Qp = pCodeParams->roi4Qp;
    regs->roi5Qp = pCodeParams->roi5Qp;
    regs->roi6Qp = pCodeParams->roi6Qp;
    regs->roi7Qp = pCodeParams->roi7Qp;
    regs->roi8Qp = pCodeParams->roi8Qp;

    regs->roiUpdate = 1; /* ROI has changed from previous frame. */

    pEncInst->roiMapEnable = 0;
    if (pEncInst->asic.regs.asicCfg->roiMapVersion >= 3) {
      pEncInst->RoimapCuCtrl_index_enable =
          pCodeParams->RoimapCuCtrl_index_enable;
      pEncInst->RoimapCuCtrl_enable = pCodeParams->RoimapCuCtrl_enable;

      pEncInst->RoimapCuCtrl_ver = pCodeParams->RoimapCuCtrl_ver;
      pEncInst->RoiQpDelta_ver = pCodeParams->RoiQpDelta_ver;
    }
    if (bAdvanceHW) pEncInst->RoiQpDelta_ver = pCodeParams->RoiQpDelta_ver;

    if (pEncInst->pass == 2 &&
        !((pEncInst->asic.regs.asicCfg->roiMapVersion == 4) &&
          (pEncInst->RoiQpDelta_ver == 4)))
      pEncInst->RoiQpDelta_ver = 1;

#ifndef CTBRC_STRENGTH
    if (pCodeParams->roiMapDeltaQpEnable == 1) {
      pEncInst->roiMapEnable = 1;
      regs->rcRoiEnable = RCROIMODE_ROIMAP_ENABLE;
    } else if (pCodeParams->roi1Area.enable || pCodeParams->roi2Area.enable)
      regs->rcRoiEnable = RCROIMODE_ROIAREA_ENABLE;
    else
      regs->rcRoiEnable = RCROIMODE_RC_ROIMAP_ROIAREA_DIABLE;
#else
    /* rdoqSkipOnlyMapEnableFlag just for 2pass when only rdoq or skip , no Qp */
    true_e rdoqSkipOnlyMapEnableFlag =
        (pEncInst->pass != 0) &&
        (pCodeParams->skipMapEnable || pCodeParams->rdoqMapEnable) &&
        (pEncInst->asic.regs.asicCfg->roiMapVersion == 4) &&
        (pEncInst->RoiQpDelta_ver == 4);
    /*rcRoiEnable : bit2: rc control bit, bit1: roi map control bit, bit 0: roi area control bit*/
    if (((pCodeParams->roiMapDeltaQpEnable == 1) || rdoqSkipOnlyMapEnableFlag ||
         pCodeParams->RoimapCuCtrl_enable) &&
        (pCodeParams->roi1Area.enable == 0 &&
         pCodeParams->roi2Area.enable == 0 &&
         pCodeParams->roi3Area.enable == 0 &&
         pCodeParams->roi4Area.enable == 0 &&
         pCodeParams->roi5Area.enable == 0 &&
         pCodeParams->roi6Area.enable == 0 &&
         pCodeParams->roi7Area.enable == 0 &&
         pCodeParams->roi8Area.enable == 0)) {
      if (pCodeParams->roiMapDeltaQpEnable == 1) pEncInst->roiMapEnable = 1;
      regs->rcRoiEnable = 0x02;
    } else if ((pCodeParams->roiMapDeltaQpEnable == 0) &&
               (pCodeParams->RoimapCuCtrl_enable == 0) &&
               (pCodeParams->roi1Area.enable != 0 ||
                pCodeParams->roi2Area.enable != 0 ||
                pCodeParams->roi3Area.enable != 0 ||
                pCodeParams->roi4Area.enable != 0 ||
                pCodeParams->roi5Area.enable != 0 ||
                pCodeParams->roi6Area.enable != 0 ||
                pCodeParams->roi7Area.enable != 0 ||
                pCodeParams->roi8Area.enable != 0)) {
      regs->rcRoiEnable = 0x01;
    } else if (((pCodeParams->roiMapDeltaQpEnable != 0) ||
                rdoqSkipOnlyMapEnableFlag ||
                (pCodeParams->RoimapCuCtrl_enable != 0)) &&
               (pCodeParams->roi1Area.enable != 0 ||
                pCodeParams->roi2Area.enable != 0 ||
                pCodeParams->roi3Area.enable != 0 ||
                pCodeParams->roi4Area.enable != 0 ||
                pCodeParams->roi5Area.enable != 0 ||
                pCodeParams->roi6Area.enable != 0 ||
                pCodeParams->roi7Area.enable != 0 ||
                pCodeParams->roi8Area.enable != 0)) {
      if (pCodeParams->roiMapDeltaQpEnable != 0) pEncInst->roiMapEnable = 1;
      regs->rcRoiEnable = 0x03;
    } else
      regs->rcRoiEnable = 0x00;
#endif

    regs->skipMapEnable = pCodeParams->skipMapEnable ? 1 : 0;
    regs->lowlatGatingDisable = pCodeParams->lowlatGatingDisable;
    regs->lowlatGatingType = pCodeParams->lowlatGatingType;
    regs->lowlatGatingCyc = pCodeParams->lowlatGatingCyc;

    /* low latency */
    pEncInst->inputLineBuf.inputLineBufEn = pCodeParams->inputLineBufEn;
    pEncInst->inputLineBuf.inputLineBufLoopBackEn =
        pCodeParams->inputLineBufLoopBackEn;
    pEncInst->inputLineBuf.inputLineBufDepth = pCodeParams->inputLineBufDepth;
    pEncInst->inputLineBuf.amountPerLoopBack = pCodeParams->amountPerLoopBack;
    pEncInst->inputLineBuf.inputLineBufHwModeEn =
        pCodeParams->inputLineBufHwModeEn;
    pEncInst->inputLineBuf.cbFunc = pCodeParams->inputLineBufCbFunc;
    pEncInst->inputLineBuf.cbData = pCodeParams->inputLineBufCbData;
    pEncInst->inputLineBuf.sbi_id_0 = pCodeParams->sbi_id_0;
    pEncInst->inputLineBuf.sbi_id_1 = pCodeParams->sbi_id_1;
    pEncInst->inputLineBuf.sbi_id_2 = pCodeParams->sbi_id_2;
    pEncInst->inputLineBuf.segmentUnitHeight = pCodeParams->segmentUnitHeight;
	pEncInst->inputLineBuf.enable_slice_irq = pCodeParams->enable_slice_irq;
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
    /* poll input sliceinfo for low latency */
    pEncInst->inputSliceInfoPollEn = pCodeParams->inputSliceInfoPollEn;
#endif

    /*stream multi-segment*/
    pEncInst->streamMultiSegment.streamMultiSegmentMode =
        pCodeParams->streamMultiSegmentMode;
    pEncInst->streamMultiSegment.streamMultiSegmentAmount =
        pCodeParams->streamMultiSegmentAmount;
    pEncInst->streamMultiSegment.cbFunc = pCodeParams->streamMultiSegCbFunc;
    pEncInst->streamMultiSegment.cbData = pCodeParams->streamMultiSegCbData;

    /* smart */
    pEncInst->smartModeEnable = pCodeParams->smartModeEnable;
    pEncInst->smartH264LumDcTh = pCodeParams->smartH264LumDcTh;
    pEncInst->smartH264CbDcTh = pCodeParams->smartH264CbDcTh;
    pEncInst->smartH264CrDcTh = pCodeParams->smartH264CrDcTh;
    for (i = 0; i < 3; i++) {
      pEncInst->smartHevcLumDcTh[i] = pCodeParams->smartHevcLumDcTh[i];
      pEncInst->smartHevcChrDcTh[i] = pCodeParams->smartHevcChrDcTh[i];
      pEncInst->smartHevcLumAcNumTh[i] = pCodeParams->smartHevcLumAcNumTh[i];
      pEncInst->smartHevcChrAcNumTh[i] = pCodeParams->smartHevcChrAcNumTh[i];
    }
    pEncInst->smartH264Qp = pCodeParams->smartH264Qp;
    pEncInst->smartHevcLumQp = pCodeParams->smartHevcLumQp;
    pEncInst->smartHevcChrQp = pCodeParams->smartHevcChrQp;
    for (i = 0; i < 4; i++)
      pEncInst->smartMeanTh[i] = pCodeParams->smartMeanTh[i];
    pEncInst->smartPixNumCntTh = pCodeParams->smartPixNumCntTh;

    /* ctbRc Mode1 */
    pEncInst->ctbRcSkinQPDelta = pCodeParams->ctbRcSkinQPDelta;
    pEncInst->ctbRcSkinMinQPDelta = pCodeParams->ctbRcSkinMinQPDelta;
    pEncInst->ctbRcSkinCbMin = pCodeParams->ctbRcSkinCbMin;
    pEncInst->ctbRcSkinCbMax = pCodeParams->ctbRcSkinCbMax;
    pEncInst->ctbRcSkinCrMin = pCodeParams->ctbRcSkinCrMin;
    pEncInst->ctbRcSkinCrMax = pCodeParams->ctbRcSkinCrMax;
    pEncInst->ctbRcSkinLumMin = pCodeParams->ctbRcSkinLumMin;
    pEncInst->ctbRcSkinLumMax = pCodeParams->ctbRcSkinLumMax;
    pEncInst->ctbRcSkinGradTh = 23;//pCodeParams->ctbRcSkinGradTh;//0 to 31 default 23, disable when set 0
    pEncInst->ctbRcDirection = pCodeParams->ctbRcDirection;
    pEncInst->ctbRcEdgeTh = 56;//pCodeParams->ctbRcEdgeTh;//0 to 127
    pEncInst->ctbRcEdgeMaxQpDelta = MIN(6 + pCodeParams->ctbRcDirection, 15);
                                    //pCodeParams->ctbRcEdgeMaxQpDelta;//and test_bench
    for (int idx = 0; idx < 16; idx++) {
      pEncInst->ctbRcThresholdI[idx] = pCodeParams->ctbRcThresholdI[idx];
      pEncInst->ctbRcThresholdB[idx] = pCodeParams->ctbRcThresholdB[idx];
      pEncInst->ctbRcThresholdP[idx] = pCodeParams->ctbRcThresholdP[idx];
    }
    /* trail part detection */
    pEncInst->ctbRcTrailStrengthMax =
    ((pEncInst->ctbRcMode & 1) && (pEncInst->gopSize == 1) && regs->asicCfg->ctbRcVersion == 2) ?
        MIN(pCodeParams->ctbRcTrailStrengthMax, 3) : 0;
    pEncInst->ctbRcTrailDeltaQp = CLIP3(-7, 0, pCodeParams->ctbRcTrailDeltaQp);
    /* visual quality prior mode decision */
    pEncInst->visualBitRateTolerance = pCodeParams->visualBitRateTolerance;
    pEncInst->IntraBiasChromaStrength = pCodeParams->IntraBiasChromaStrength;
    pEncInst->IntraBiasStrengthMax = pCodeParams->IntraBiasStrength;
    pEncInst->IntraBiasStrength = pCodeParams->IntraBiasStrength;
    pEncInst->IntraBiasMvThreshold = pCodeParams->IntraBiasMvThreshold;
    pEncInst->bTrailAvoidIntraBias = pCodeParams->bTrailAvoidIntraBias;

    /* psy factor */
    pEncInst->psyFactor = pCodeParams->psyFactor;
    regs->psyFactor =
        (u32)(pEncInst->psyFactor * (1 << PSY_FACTOR_SCALE_BITS) + 0.5);

    /* intra recon*/
    pEncInst->intraReconEnable = pCodeParams->intraReconEnable;

    /* multipass */
    pEncInst->cuTreeCtl.inQpDeltaBlkSize =
        64 >> pCodeParams->roiMapDeltaQpBlockUnit;
    pEncInst->cuTreeCtl.aq_mode = pCodeParams->aq_mode;
    pEncInst->cuTreeCtl.aqStrength = pCodeParams->aq_strength;
  }

  /*update parameters enforced in encode order*/
  if (bUpdateEncOrderEnforceParam) {
    gdrDuration = pCodeParams->gdrDuration;
    pEncInst->rateControl.sei.insertRecoveryPointMessage = ENCHW_NO;
    pEncInst->rateControl.sei.recoveryFrameCnt = gdrDuration;
    if (pEncInst->encStatus < VCENCSTAT_START_FRAME) {
      pEncInst->gdrFirstIntraFrame = (1 + pEncInst->interlaced);
    }

    pEncInst->gdrEnabled = (gdrDuration > 0);
    bool gdrInit =
        pEncInst->gdrEnabled && (pEncInst->gdrDuration != (i32)gdrDuration);
    pEncInst->gdrDuration = gdrDuration;

    if (gdrInit) {
      pEncInst->gdrStart = 0;
      pEncInst->gdrCount = 0;

      pEncInst->gdrAverageMBRows =
          (pEncInst->ctbPerCol - 1) / pEncInst->gdrDuration;
      pEncInst->gdrMBLeft = pEncInst->ctbPerCol - 1 -
                            pEncInst->gdrAverageMBRows * pEncInst->gdrDuration;

      if (pEncInst->gdrAverageMBRows == 0) {
        pEncInst->rateControl.sei.recoveryFrameCnt = pEncInst->gdrMBLeft;
        pEncInst->gdrDuration = pEncInst->gdrMBLeft;
      }
    }

    if (!pEncInst->gdrEnabled) {
      /* Allow user to turn off gdr */
      pEncInst->gdrStart = 0;
      pEncInst->gdrCount = 0;
      pEncInst->gdrAverageMBRows = 0;
      pEncInst->gdrMBLeft = 0;
      pEncInst->rateControl.sei.recoveryFrameCnt = 0;
      pEncInst->gdrDuration = 0;
    }
  }

  /*whether parameters enforced in input order or parameters enforced in encode order are updated,
    need to update follow parameters*/
  if (pCodeParams->intraArea.enable && gdrDuration == 0) {
    regs->intraAreaTop = pCodeParams->intraArea.top;
    regs->intraAreaLeft = pCodeParams->intraArea.left;
    regs->intraAreaBottom = pCodeParams->intraArea.bottom;
    regs->intraAreaRight = pCodeParams->intraArea.right;
  } else {
    regs->intraAreaTop = regs->intraAreaLeft = regs->intraAreaBottom =
        regs->intraAreaRight = INVALID_POS;
  }
  if (pCodeParams->ipcm1Area.enable && gdrDuration == 0) {
    regs->ipcm1AreaTop = pCodeParams->ipcm1Area.top;
    regs->ipcm1AreaLeft = pCodeParams->ipcm1Area.left;
    regs->ipcm1AreaBottom = pCodeParams->ipcm1Area.bottom;
    regs->ipcm1AreaRight = pCodeParams->ipcm1Area.right;
  } else {
    regs->ipcm1AreaTop = regs->ipcm1AreaLeft = regs->ipcm1AreaBottom =
        regs->ipcm1AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm2Area.enable && gdrDuration == 0) {
    regs->ipcm2AreaTop = pCodeParams->ipcm2Area.top;
    regs->ipcm2AreaLeft = pCodeParams->ipcm2Area.left;
    regs->ipcm2AreaBottom = pCodeParams->ipcm2Area.bottom;
    regs->ipcm2AreaRight = pCodeParams->ipcm2Area.right;
  } else {
    regs->ipcm2AreaTop = regs->ipcm2AreaLeft = regs->ipcm2AreaBottom =
        regs->ipcm2AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm3Area.enable && gdrDuration == 0) {
    regs->ipcm3AreaTop = pCodeParams->ipcm3Area.top;
    regs->ipcm3AreaLeft = pCodeParams->ipcm3Area.left;
    regs->ipcm3AreaBottom = pCodeParams->ipcm3Area.bottom;
    regs->ipcm3AreaRight = pCodeParams->ipcm3Area.right;
  } else {
    regs->ipcm3AreaTop = regs->ipcm3AreaLeft = regs->ipcm3AreaBottom =
        regs->ipcm3AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm4Area.enable && gdrDuration == 0) {
    regs->ipcm4AreaTop = pCodeParams->ipcm4Area.top;
    regs->ipcm4AreaLeft = pCodeParams->ipcm4Area.left;
    regs->ipcm4AreaBottom = pCodeParams->ipcm4Area.bottom;
    regs->ipcm4AreaRight = pCodeParams->ipcm4Area.right;
  } else {
    regs->ipcm4AreaTop = regs->ipcm4AreaLeft = regs->ipcm4AreaBottom =
        regs->ipcm4AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm5Area.enable && gdrDuration == 0) {
    regs->ipcm5AreaTop = pCodeParams->ipcm5Area.top;
    regs->ipcm5AreaLeft = pCodeParams->ipcm5Area.left;
    regs->ipcm5AreaBottom = pCodeParams->ipcm5Area.bottom;
    regs->ipcm5AreaRight = pCodeParams->ipcm5Area.right;
  } else {
    regs->ipcm5AreaTop = regs->ipcm5AreaLeft = regs->ipcm5AreaBottom =
        regs->ipcm5AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm6Area.enable && gdrDuration == 0) {
    regs->ipcm6AreaTop = pCodeParams->ipcm6Area.top;
    regs->ipcm6AreaLeft = pCodeParams->ipcm6Area.left;
    regs->ipcm6AreaBottom = pCodeParams->ipcm6Area.bottom;
    regs->ipcm6AreaRight = pCodeParams->ipcm6Area.right;
  } else {
    regs->ipcm6AreaTop = regs->ipcm6AreaLeft = regs->ipcm6AreaBottom =
        regs->ipcm6AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm7Area.enable && gdrDuration == 0) {
    regs->ipcm7AreaTop = pCodeParams->ipcm7Area.top;
    regs->ipcm7AreaLeft = pCodeParams->ipcm7Area.left;
    regs->ipcm7AreaBottom = pCodeParams->ipcm7Area.bottom;
    regs->ipcm7AreaRight = pCodeParams->ipcm7Area.right;
  } else {
    regs->ipcm7AreaTop = regs->ipcm7AreaLeft = regs->ipcm7AreaBottom =
        regs->ipcm7AreaRight = INVALID_POS;
  }

  if (pCodeParams->ipcm8Area.enable && gdrDuration == 0) {
    regs->ipcm8AreaTop = pCodeParams->ipcm8Area.top;
    regs->ipcm8AreaLeft = pCodeParams->ipcm8Area.left;
    regs->ipcm8AreaBottom = pCodeParams->ipcm8Area.bottom;
    regs->ipcm8AreaRight = pCodeParams->ipcm8Area.right;
  } else {
    regs->ipcm8AreaTop = regs->ipcm8AreaLeft = regs->ipcm8AreaBottom =
        regs->ipcm8AreaRight = INVALID_POS;
  }

  //keep pcm_enabled_flag the same in sw and cmodel
  if ((pCodeParams->ipcm1Area.enable || pCodeParams->ipcm2Area.enable ||
       pCodeParams->ipcm3Area.enable || pCodeParams->ipcm4Area.enable ||
       pCodeParams->ipcm5Area.enable || pCodeParams->ipcm6Area.enable ||
       pCodeParams->ipcm7Area.enable || pCodeParams->ipcm8Area.enable ||
       pCodeParams->ipcmMapEnable) == 0 &&
      pEncInst->sps->pcm_enabled_flag == 1) {
    regs->ipcm1AreaTop = 1;
    regs->ipcm1AreaBottom = 0;
    regs->ipcm1AreaLeft = 1;
    regs->ipcm1AreaRight = 0;
  }

  if (pCodeParams->roi1Area.enable && gdrDuration == 0) {
    regs->roi1Top = pCodeParams->roi1Area.top;
    regs->roi1Left = pCodeParams->roi1Area.left;
    regs->roi1Bottom = pCodeParams->roi1Area.bottom;
    regs->roi1Right = pCodeParams->roi1Area.right;
    pEncInst->roi1Enable = 1;
  } else {
    regs->roi1Top = regs->roi1Left = regs->roi1Bottom = regs->roi1Right =
        INVALID_POS;
    pEncInst->roi1Enable = 0;
  }

#ifdef MULTI_FRAME_SUPPORT
  /* batch mode update */
  vcencBatchUpdateCodingCtrlParam(pEncInst, pCodeParams);
#endif /* MULTI_FRAME_SUPPORT */

  pEncInst->asic.regs.bCodingCtrlUpdate = 1;

  /* these parameters only update when init*/
  if (pEncInst->encStatus == VCENCSTAT_INIT) {
    if (IS_H264(pEncInst->codecFormat)) {
      pEncInst->layerInRefIdc = pCodeParams->layerInRefIdcEnable;
      pEncInst->prefixNalSvcFlag = pCodeParams->prefixNalSvcFlag;
      pEncInst->svctEnable = pCodeParams->svctEnable;
    }

    regs->bRDOQEnable = pCodeParams->enableRdoQuant;
    regs->rdoqMapEnable = pCodeParams->rdoqMapEnable ? 1 : 0;

    regs->dynamicRdoEnable = pCodeParams->enableDynamicRdo;

    if (regs->dynamicRdoEnable) {
      regs->dynamicRdoCu16Bias = pCodeParams->dynamicRdoCu16Bias;
      regs->dynamicRdoCu16Factor = pCodeParams->dynamicRdoCu16Factor;
      regs->dynamicRdoCu32Bias = pCodeParams->dynamicRdoCu32Bias;
      regs->dynamicRdoCu32Factor = pCodeParams->dynamicRdoCu32Factor;
    }
    pEncInst->chromaQpOffset = pCodeParams->chroma_qp_offset;
    pEncInst->fieldOrder = pCodeParams->fieldOrder ? 1 : 0;
    pEncInst->disableDeblocking = pCodeParams->disableDeblockingFilter;
    pEncInst->enableDeblockOverride = pCodeParams->enableDeblockOverride;
    if (pEncInst->enableDeblockOverride) {
      regs->slice_deblocking_filter_override_flag =
          pCodeParams->deblockOverride;
    } else {
      regs->slice_deblocking_filter_override_flag = 0;
    }
    if (IS_H264(pEncInst->codecFormat)) {
      /* always enable deblocking override for H.264 */
      pEncInst->enableDeblockOverride = 1;
      regs->slice_deblocking_filter_override_flag = 1;
    }
    pEncInst->tc_Offset = pCodeParams->tc_Offset;
    pEncInst->beta_Offset = pCodeParams->beta_Offset;
    pEncInst->enableSao = pCodeParams->enableSao;

    pEncInst->enableTS = pCodeParams->enableTS;
    pEncInst->log2MaxTSBlockSizeMinus2 = pCodeParams->log2MaxTSBlockSizeMinus2;

    VCEncHEVCDnfSetParameters(pEncInst, pCodeParams);

    regs->cabac_init_flag = pCodeParams->cabacInitFlag;
    regs->entropy_coding_mode_flag = (pCodeParams->enableCabac ? 1 : 0);

    /* SEI messages are written in the beginning of each frame */
    if (pCodeParams->seiMessages)
      pEncInst->rateControl.sei.enabled = ENCHW_YES;
    else
      pEncInst->rateControl.sei.enabled = ENCHW_NO;

    /* SRAM power down mode */
    regs->sramPowerdownDisable = pCodeParams->sramPowerdownDisable;
    regs->sramPowerdownTimerDiv32 = pCodeParams->sramPowerdownTimerDiv32;
    regs->sramPowerdownMode = pCodeParams->sramPowerdownMode;

    if(IS_H264(pEncInst->codecFormat) ||
      (IS_HEVC(pEncInst->codecFormat) && (pEncInst->enableDeblockOverride == 1))) {
      pEncInst->disableDeblocking = pCodeParams->disableDeblockingFilter;
      pEncInst->tc_Offset = pCodeParams->tc_Offset;
      pEncInst->beta_Offset = pCodeParams->beta_Offset;
    }

    /* sw skip frame */
    pEncInst->sw_skip_flag = pCodeParams->sw_skip_flag;

    //when init don't need to adjust refCnt
    return;
  }

end:
  pCodingCtrlParam->refCnt--;
  //remove useless parameters from queue
  if (0 == pCodingCtrlParam->refCnt) {
    queue_remove(&pEncInst->codingCtrl.codingCtrlQueue,
                 (struct node *)pCodingCtrlParam);
    DynamicPutBufferToPool(&pEncInst->codingCtrl.codingCtrlBufPool,
                           (void *)pCodingCtrlParam);
  }

  return;
}

/*------------------------------------------------------------------------------
    Function name : EncUpdateCodingCtrlForPass1
    Description   : [pass1]update coding ctrl parameters matched with current frame into instance
    Return type   : void
    Argument      : inst - encoder instance
    Argument      : pCodingCtrl - coding ctrl
------------------------------------------------------------------------------*/
void EncUpdateCodingCtrlForPass1(VCEncInst instAddr,
                                 EncCodingCtrlParam *pCodingCtrlParam) {
  // if pCodingCtrlParam==NULL, there's no coding ctrl parameters need to be updated into instance.
  if (!instAddr || !pCodingCtrlParam) return;
  struct vcenc_instance *pEncInst = (struct vcenc_instance *)instAddr;
  //if this frame's codingCtrl is same with codingCtrl in instance, don't need to reset.
  if (pEncInst->codingCtrl.pCodingCtrlParam == pCodingCtrlParam) return;

  VCEncCodingCtrl *pCodingCtrl = &pCodingCtrlParam->encCodingCtrl;
  regValues_s *regs = &pEncInst->asic.regs;

  pEncInst->codingCtrl.pCodingCtrlParam = pCodingCtrlParam;
  pEncInst->roiMapEnable = (pCodingCtrl->roiMapDeltaQpEnable == 1) ? 1 : 0;
  regs->rcRoiEnable = 0x00;
  pEncInst->cuTreeCtl.inQpDeltaBlkSize =
      64 >> pCodingCtrl->roiMapDeltaQpBlockUnit;
  pEncInst->cuTreeCtl.aq_mode = pCodingCtrl->aq_mode;
  pEncInst->cuTreeCtl.aqStrength = pCodingCtrl->aq_strength;
  /* psy factor (for quality)*/
  pEncInst->psyFactor = pCodingCtrl->psyFactor;
  regs->psyFactor =
      (u32)(pEncInst->psyFactor * (1 << PSY_FACTOR_SCALE_BITS) + 0.5);
  if (IS_H264(pEncInst->codecFormat)) {
    /* always enable deblocking override for H.264 */
    pEncInst->enableDeblockOverride = 1;
    regs->slice_deblocking_filter_override_flag = 1;
  }
  return;
}

static VCEncRet encCheckVuiParam(struct vcenc_instance *pEncInst,
                                 VCEncCodingCtrl *pCodeParams) {

  VCEncRet ret = VCENC_OK;

  if (pCodeParams->vuiVideoSignalTypePresentFlag==0) {
    if ((pCodeParams->vuiVideoFullRange != 0) ||
        (pCodeParams->vuiVideoFormat != 5)) {
        APITRACEERR("video range or video format in vui is not default "
                    "value, need enable video_signal_type_present_flag\n");
        ret = VCENC_INVALID_ARGUMENT;
    }
  }

  if ((pCodeParams->vuiVideoSignalTypePresentFlag==0)||
      (pCodeParams->vuiColorDescription.vuiColorDescripPresentFlag==0)) {
    if ((pCodeParams->vuiColorDescription.vuiColorPrimaries != 2) ||
          (pCodeParams->vuiColorDescription.vuiTransferCharacteristics != 2)||
          (pCodeParams->vuiColorDescription.vuiMatrixCoefficients != 2)) {
      APITRACEERR("color description information in vui is not default "
                    "value, need enable both video_signal_type_present_flag "
                    "and colour_description_present_flag.\n");
      ret = VCENC_INVALID_ARGUMENT;
    }
  }

  return ret;
}

/*------------------------------------------------------------------------------
    Function name : EncCheckCodingCtrlParam
    Description   : check inputed coding ctrl parameters
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pCodeParams - coding ctrl
------------------------------------------------------------------------------*/
VCEncRet EncCheckCodingCtrlParam(struct vcenc_instance *pEncInst,
                                 VCEncCodingCtrl *pCodeParams) {
  regValues_s *regs = &pEncInst->asic.regs;
  VCEncRet ret = VCENC_INVALID_ARGUMENT;

  do {
    /* Check for illegal inputs */
    if ((pEncInst == NULL) || (pCodeParams == NULL)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Null argument\n");
      break;
    }

    u32 client_type = VCEncGetClientType(pEncInst->codecFormat);

    i32 core_id = -1;

    const EWLHwConfig_t *cfg = EncAsicGetAsicConfig(client_type, pEncInst->ctx);
    if (EWL_IS_H264_CLIENT(client_type) &&
        cfg->h264Enabled == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR codecFormat=h264 unsupported by HW\n");
      break;
    }

    if (EWL_IS_HEVC_CLIENT(client_type) &&
        cfg->hevcEnabled == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR codecFormat=hevc unsupported by HW\n");
      break;
    }

    if (EWL_IS_AV1_CLIENT(client_type) &&
        cfg->av1Enabled == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR codecFormat=av1 unsupported by HW\n");
      break;
    }

    if (EWL_IS_VP9_CLIENT(client_type) &&
        cfg->vp9Enabled == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR codecFormat=vp9 unsupported by HW\n");
      break;
    }

    if (cfg->deNoiseEnabled == EWL_HW_CONFIG_NOT_SUPPORTED &&
        pCodeParams->noiseReductionEnable == 1) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Denoise unsupported by HW\n");
      break;
    }

    if (pCodeParams->streamMultiSegmentMode != 0 &&
        cfg->streamMultiSegment == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR stream multiSegment mode unsupported by "
          "HW\n");
      break;
    }

    if (((HW_ID_MAJOR_NUMBER(regs->asicHwId) < 2 /* H2V1 */) &&
         HW_PRODUCT_H2(regs->asicHwId)) &&
        (pCodeParams->roiMapDeltaQpEnable == 1)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR ROI MAP not supported\n");
      break;
    }

    if ((pEncInst->asic.regs.asicCfg->roiMapVersion < 3) &&
        pCodeParams->RoimapCuCtrl_index_enable) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR ROI MAP cu ctrl index not supported\n");
      break;
    }
    if ((pEncInst->asic.regs.asicCfg->roiMapVersion < 3) &&
        pCodeParams->RoimapCuCtrl_enable) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR ROI MAP cu ctrl not supported\n");
      break;
    }

    bool bAdvanceHW =
        (pEncInst->asic.regs.asicCfg->roiMapVersion == 2) &&
        ((HW_ID_MAJOR_NUMBER(pEncInst->asic.regs.asicCfg->hw_asic_id) >= 0x82) ||
         (HW_PRODUCT_VC9000(pEncInst->asic.regs.asicCfg->hw_asic_id)) ||
         HW_PRODUCT_SYSTEM6010(pEncInst->asic.regs.asicCfg->hw_asic_id) ||
         (HW_PRODUCT_VC9000LE(pEncInst->asic.regs.asicCfg->hw_asic_id)));

    if (pCodeParams->RoimapCuCtrl_enable &&
        ((pCodeParams->RoimapCuCtrl_ver < 3) ||
         (pCodeParams->RoimapCuCtrl_ver > 7) ||
         IS_AV1(pEncInst->codecFormat))) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid RoiCuCtrlVer\n");
      break;
    }
    if ((pCodeParams->RoimapCuCtrl_enable == 0) &&
        pCodeParams->RoimapCuCtrl_ver != 0) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid RoimapCuCtrl_ver\n");
      break;
    }

    true_e qpMapInvalid =
        pCodeParams->roiMapDeltaQpEnable && (pCodeParams->RoiQpDelta_ver == 0);
    true_e ipcmMapInvalid = pCodeParams->ipcmMapEnable &&
                            (((pCodeParams->RoiQpDelta_ver != 1) &&
                              (pCodeParams->RoimapCuCtrl_enable == 0)) ||
                             (pCodeParams->RoimapCuCtrl_enable &&
                              (4 > pCodeParams->RoimapCuCtrl_ver)));
    true_e skipMapInvalid = pCodeParams->skipMapEnable &&
                            (((pCodeParams->RoiQpDelta_ver < 2) &&
                              (pCodeParams->RoimapCuCtrl_enable == 0)) ||
                             (pCodeParams->RoimapCuCtrl_enable &&
                              (4 > pCodeParams->RoimapCuCtrl_ver)));
    true_e rdoqMapInvalid =
        pCodeParams->rdoqMapEnable && (pCodeParams->RoiQpDelta_ver != 4);
    true_e roiMapVersion3QpDeltaNotValid =
        (pEncInst->asic.regs.asicCfg->roiMapVersion == 3) &&
        (qpMapInvalid || ipcmMapInvalid || skipMapInvalid ||
         (pCodeParams->RoiQpDelta_ver > 3));
    true_e roiMapVersion4QpDeltaNotValid =
        (pEncInst->asic.regs.asicCfg->roiMapVersion == 4) &&
        (qpMapInvalid || ipcmMapInvalid || skipMapInvalid || rdoqMapInvalid);
    if (roiMapVersion3QpDeltaNotValid || roiMapVersion4QpDeltaNotValid ||
        ((pEncInst->asic.regs.asicCfg->roiMapVersion < 3) &&
         (pCodeParams->RoiQpDelta_ver > 2)) ||
        (pCodeParams->RoiQpDelta_ver > 4)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid RoiQpDelta_ver\n");
      break;
    }
    if (bAdvanceHW &&
        (pCodeParams->ipcmMapEnable || pCodeParams->skipMapEnable) &&
        (pCodeParams->RoiQpDelta_ver == 0)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid RoiQpDelta_ver when advance HW\n");
      break;
    }

    /* roiMap can be enabled only if cuQpDelta is enabled in pps */
    i32 roiMapEnableAllowed = pEncInst->encStatus < VCENCSTAT_START_STREAM ||
                              pEncInst->asic.regs.cuQpDeltaEnabled;
    if (!roiMapEnableAllowed && pCodeParams->roiMapDeltaQpEnable) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi Map "
          "Enable\n");
      break;
    }

    if ((pEncInst->encStatus >= VCENCSTAT_START_STREAM) &&
        (pEncInst->RoimapCuCtrl_enable != pCodeParams->RoimapCuCtrl_enable)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid encoding status to config roi Map "
          "cu ctrl Enable\n");
      break;
    }

    /* Check for invalid values */
    if (pCodeParams->sliceSize > (u32)pEncInst->ctbPerCol) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sliceSize\n");
      break;
    }
    if ((IS_AV1(pEncInst->codecFormat) || IS_VP9(pEncInst->codecFormat)) &&
        (pCodeParams->sliceSize != 0)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid sliceSize for AV1 or VP9, it "
          "should be 0\n");
      break;
    }

    if (pEncInst->tiles_enabled_flag && (pCodeParams->sliceSize != 0)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: sliceSize NOT supported when multi tiles\n");
      break;
    }

    if (pCodeParams->cirStart > (u32)pEncInst->ctbPerFrame ||
        pCodeParams->cirInterval > (u32)pEncInst->ctbPerFrame) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid CIR value\n");
      break;
    }
    if (pCodeParams->intraArea.enable) {
      if (!(pCodeParams->intraArea.top <= pCodeParams->intraArea.bottom &&
            pCodeParams->intraArea.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->intraArea.left <= pCodeParams->intraArea.right &&
            pCodeParams->intraArea.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid intraArea\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite intraArea");
      }
    }

    true_e ipcmAreaSupport =
        pCodeParams->ipcm1Area.enable || pCodeParams->ipcm2Area.enable ||
        pCodeParams->ipcm3Area.enable || pCodeParams->ipcm4Area.enable ||
        pCodeParams->ipcm5Area.enable || pCodeParams->ipcm6Area.enable ||
        pCodeParams->ipcm7Area.enable || pCodeParams->ipcm8Area.enable;
    if ((IS_AV1(pEncInst->codecFormat) || IS_VP9(pEncInst->codecFormat)) &&
        (ipcmAreaSupport || pCodeParams->ipcmMapEnable)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR IPCM not supported for AV1 or VP9\n");
      break;
    }

    if (pEncInst->encStatus != VCENCSTAT_INIT &&
        pEncInst->pcm_enabled_flag == 0 &&
        (ipcmAreaSupport || pCodeParams->ipcmMapEnable)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid ipcmArea, ipcm is disabled \n");
      break;
    }

    if (pCodeParams->ipcm1Area.enable) {
      if (!(pCodeParams->ipcm1Area.top <= pCodeParams->ipcm1Area.bottom &&
            pCodeParams->ipcm1Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm1Area.left <= pCodeParams->ipcm1Area.right &&
            pCodeParams->ipcm1Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm1Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm1Area");
      }
    }
    if (pCodeParams->ipcm2Area.enable) {
      if (!(pCodeParams->ipcm2Area.top <= pCodeParams->ipcm2Area.bottom &&
            pCodeParams->ipcm2Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm2Area.left <= pCodeParams->ipcm2Area.right &&
            pCodeParams->ipcm2Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm2Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm2Area");
      }
    }

    if ((regs->asicCfg->IPCM8Support == 0) &&
        (pCodeParams->ipcm3Area.enable || pCodeParams->ipcm4Area.enable ||
         pCodeParams->ipcm5Area.enable || pCodeParams->ipcm6Area.enable ||
         pCodeParams->ipcm7Area.enable || pCodeParams->ipcm8Area.enable)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcmArea\n");
      break;
    }

    if (pCodeParams->ipcm3Area.enable) {
      if (!(pCodeParams->ipcm3Area.top <= pCodeParams->ipcm3Area.bottom &&
            pCodeParams->ipcm3Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm3Area.left <= pCodeParams->ipcm3Area.right &&
            pCodeParams->ipcm3Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm3Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm3Area");
      }
    }

    if (pCodeParams->ipcm4Area.enable) {
      if (!(pCodeParams->ipcm4Area.top <= pCodeParams->ipcm4Area.bottom &&
            pCodeParams->ipcm4Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm4Area.left <= pCodeParams->ipcm4Area.right &&
            pCodeParams->ipcm4Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm4Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm4Area");
      }
    }

    if (pCodeParams->ipcm5Area.enable) {
      if (!(pCodeParams->ipcm5Area.top <= pCodeParams->ipcm5Area.bottom &&
            pCodeParams->ipcm5Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm5Area.left <= pCodeParams->ipcm5Area.right &&
            pCodeParams->ipcm5Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm5Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm5Area");
      }
    }

    if (pCodeParams->ipcm6Area.enable) {
      if (!(pCodeParams->ipcm6Area.top <= pCodeParams->ipcm6Area.bottom &&
            pCodeParams->ipcm6Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm6Area.left <= pCodeParams->ipcm6Area.right &&
            pCodeParams->ipcm6Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm6Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm6Area");
      }
    }

    if (pCodeParams->ipcm7Area.enable) {
      if (!(pCodeParams->ipcm7Area.top <= pCodeParams->ipcm7Area.bottom &&
            pCodeParams->ipcm7Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm7Area.left <= pCodeParams->ipcm7Area.right &&
            pCodeParams->ipcm7Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm7Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm7Area");
      }
    }

    if (pCodeParams->ipcm8Area.enable) {
      if (!(pCodeParams->ipcm8Area.top <= pCodeParams->ipcm8Area.bottom &&
            pCodeParams->ipcm8Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->ipcm8Area.left <= pCodeParams->ipcm8Area.right &&
            pCodeParams->ipcm8Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ipcm8Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite ipcm8Area");
      }
    }

    if ((pEncInst->frameInfo == 0) &&
        (pCodeParams->rect1.enable || pCodeParams->rect2.enable ||
         pCodeParams->rect3.enable || pCodeParams->rect4.enable ||
         pCodeParams->rect5.enable || pCodeParams->rect6.enable ||
         pCodeParams->rect7.enable || pCodeParams->rect0.enable)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR sseinfo Area not supported\n");
      break;
    }

    if (pCodeParams->rect0.enable) {
      if (!(pCodeParams->rect0.top <= pCodeParams->rect0.bottom &&
            pCodeParams->rect0.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect0.left <= pCodeParams->rect0.right &&
            pCodeParams->rect0.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect0\n");
        break;
      }
    }

    if (pCodeParams->rect1.enable) {
      if (!(pCodeParams->rect1.top <= pCodeParams->rect1.bottom &&
            pCodeParams->rect1.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect1.left <= pCodeParams->rect1.right &&
            pCodeParams->rect1.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect1\n");
        break;
      }
    }

    if (pCodeParams->rect2.enable) {
      if (!(pCodeParams->rect2.top <= pCodeParams->rect2.bottom &&
            pCodeParams->rect2.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect2.left <= pCodeParams->rect2.right &&
            pCodeParams->rect2.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect2\n");
        break;
      }
    }

    if (pCodeParams->rect3.enable) {
      if (!(pCodeParams->rect3.top <= pCodeParams->rect3.bottom &&
            pCodeParams->rect3.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect3.left <= pCodeParams->rect3.right &&
            pCodeParams->rect3.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect3\n");
        break;
      }
    }

    if (pCodeParams->rect4.enable) {
      if (!(pCodeParams->rect4.top <= pCodeParams->rect4.bottom &&
            pCodeParams->rect4.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect4.left <= pCodeParams->rect4.right &&
            pCodeParams->rect4.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect4\n");
        break;
      }
    }

    if (pCodeParams->rect5.enable) {
      if (!(pCodeParams->rect5.top <= pCodeParams->rect5.bottom &&
            pCodeParams->rect5.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect5.left <= pCodeParams->rect5.right &&
            pCodeParams->rect5.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect5\n");
        break;
      }
    }

    if (pCodeParams->rect6.enable) {
      if (!(pCodeParams->rect6.top <= pCodeParams->rect6.bottom &&
            pCodeParams->rect6.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect6.left <= pCodeParams->rect6.right &&
            pCodeParams->rect6.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect6\n");
        break;
      }
    }

    if (pCodeParams->rect7.enable) {
      if (!(pCodeParams->rect7.top <= pCodeParams->rect7.bottom &&
            pCodeParams->rect7.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->rect7.left <= pCodeParams->rect7.right &&
            pCodeParams->rect7.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sse info rect7\n");
        break;
      }
    }

    if (VCENC_OK != vcencCheckRoiCtrl(pEncInst, pCodeParams)) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid encoding status to config "
          "roi1Area\n");
      break;
    }

    if (pCodeParams->roi1Area.enable) {
      if (!(pCodeParams->roi1Area.top <= pCodeParams->roi1Area.bottom &&
            pCodeParams->roi1Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi1Area.left <= pCodeParams->roi1Area.right &&
            pCodeParams->roi1Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi1Area\n");
        break;
      }
      if ((pCodeParams->gdrDuration > 0)) {
        APITRACEERR("VCEncSetCodingCtrl: WARNING GDR will overwrite roi1Area");
      }
    }

    if (pCodeParams->roi2Area.enable) {
      if (!(pCodeParams->roi2Area.top <= pCodeParams->roi2Area.bottom &&
            pCodeParams->roi2Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi2Area.left <= pCodeParams->roi2Area.right &&
            pCodeParams->roi2Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi2Area\n");
        break;
      }
    }

    if ((regs->asicCfg->ROI8Support == 0) &&
        (pCodeParams->roi3Area.enable || pCodeParams->roi4Area.enable ||
         pCodeParams->roi5Area.enable || pCodeParams->roi6Area.enable ||
         pCodeParams->roi7Area.enable || pCodeParams->roi8Area.enable)) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roiArea\n");
      break;
    }

    if (pCodeParams->roi3Area.enable) {
      if (!(pCodeParams->roi3Area.top <= pCodeParams->roi3Area.bottom &&
            pCodeParams->roi3Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi3Area.left <= pCodeParams->roi3Area.right &&
            pCodeParams->roi3Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi3Area\n");
        break;
      }
    }

    if (pCodeParams->roi4Area.enable) {
      if (!(pCodeParams->roi4Area.top <= pCodeParams->roi4Area.bottom &&
            pCodeParams->roi4Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi4Area.left <= pCodeParams->roi4Area.right &&
            pCodeParams->roi4Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi4Area\n");
        break;
      }
    }

    if (pCodeParams->roi5Area.enable) {
      if (!(pCodeParams->roi5Area.top <= pCodeParams->roi5Area.bottom &&
            pCodeParams->roi5Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi5Area.left <= pCodeParams->roi5Area.right &&
            pCodeParams->roi5Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi5Area\n");
        break;
      }
    }

    if (pCodeParams->roi6Area.enable) {
      if (!(pCodeParams->roi6Area.top <= pCodeParams->roi6Area.bottom &&
            pCodeParams->roi6Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi6Area.left <= pCodeParams->roi6Area.right &&
            pCodeParams->roi6Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi6Area\n");
        break;
      }
    }

    if (pCodeParams->roi7Area.enable) {
      if (!(pCodeParams->roi7Area.top <= pCodeParams->roi7Area.bottom &&
            pCodeParams->roi7Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi7Area.left <= pCodeParams->roi7Area.right &&
            pCodeParams->roi7Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi7Area\n");
        break;
      }
    }

    if (pCodeParams->roi8Area.enable) {
      if (!(pCodeParams->roi8Area.top <= pCodeParams->roi8Area.bottom &&
            pCodeParams->roi8Area.bottom < (u32)pEncInst->ctbPerCol &&
            pCodeParams->roi8Area.left <= pCodeParams->roi8Area.right &&
            pCodeParams->roi8Area.right < (u32)pEncInst->ctbPerRow)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid roi8Area\n");
        break;
      }
    }

    if ((((!bAdvanceHW) && (regs->asicCfg->roiMapVersion != 1 &&
                            regs->asicCfg->roiMapVersion != 3)) ||
         (bAdvanceHW && pCodeParams->RoiQpDelta_ver != 1)) &&
        pCodeParams->ipcmMapEnable) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR IPCM MAP not supported\n");
      break;
    }

    if (pCodeParams->skipMapEnable) {
      if (IS_AV1(pEncInst->codecFormat) || IS_VP9(pEncInst->codecFormat)) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR SKIP MAP not supported for AV1/VP9\n");
        break;
      } else if (((!bAdvanceHW) &&
                  (regs->asicCfg->roiMapVersion != 2 &&
                   regs->asicCfg->roiMapVersion != 3) &&
                  regs->asicCfg->roiMapVersion != 4) ||
                 (bAdvanceHW && pCodeParams->RoiQpDelta_ver != 2)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR SKIP MAP not supported by HW\n");
        break;
      }
    }

    if (pCodeParams->rdoqMapEnable) {
      if ((regs->asicCfg->roiMapVersion != 4) &&
          (pCodeParams->RoiQpDelta_ver != 4)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR RDOQ MAP not supported by HW\n");
        break;
      }
    }

    if (pCodeParams->aq_mode > 3) {
      APITRACEERR(
          "VCEncSetCodingCtrl ERROR: INVALID aq_mode, should be within [0, "
          "3]\n");
      break;
    }

    if (pCodeParams->aq_mode > 0 &&
         regs->asicCfg->encVisualTuneSupport == EWL_HW_CONFIG_NOT_SUPPORTED) {
      APITRACEERR(
          "VCEncSetCodingCtrl ERROR: aq_mode is not supported by HW\n");
      break;
    }

    if ((pCodeParams->aq_strength > 3.0) || (pCodeParams->aq_strength < 0.0)) {
      APITRACEERR(
          "VCEncSetCodingCtrl ERROR: INVALID aq_strength, should be within "
          "[0.0, 3.0]\n");
      break;
    }

    if (regs->asicCfg->cuInforVersion < 2 && pCodeParams->aq_mode != AQ_NONE) {
      APITRACEERR(
          "VCEncSetCodingCtrl ERROR: aqInfo is not supported by HW\n");
      break;
    }

    if ((pCodeParams->psyFactor > 4.0) || (pCodeParams->psyFactor < 0.0)) {
      APITRACEERR(
          "VCEncSetCodingCtrl ERROR: INVALID psyFactor, should be within [0.0, "
          "4.0]\n");
      break;
    } else if (pCodeParams->psyFactor > 0.0 && !regs->asicCfg->encPsyTuneSupport) {
      APITRACEERR("VCEncSetCodingCtrl ERROR: psy is not supported by HW\n");
      break;
    }

    if (regs->asicCfg->roiAbsQpSupport) {
      if (pCodeParams->roi1DeltaQp < -51 || pCodeParams->roi1DeltaQp > 51 ||
          pCodeParams->roi2DeltaQp < -51 || pCodeParams->roi2DeltaQp > 51 ||
          pCodeParams->roi3DeltaQp < -51 || pCodeParams->roi3DeltaQp > 51 ||
          pCodeParams->roi4DeltaQp < -51 || pCodeParams->roi4DeltaQp > 51 ||
          pCodeParams->roi5DeltaQp < -51 || pCodeParams->roi5DeltaQp > 51 ||
          pCodeParams->roi6DeltaQp < -51 || pCodeParams->roi6DeltaQp > 51 ||
          pCodeParams->roi7DeltaQp < -51 || pCodeParams->roi7DeltaQp > 51 ||
          pCodeParams->roi8DeltaQp < -51 || pCodeParams->roi8DeltaQp > 51 ||
          pCodeParams->roi1Qp > 51 || pCodeParams->roi2Qp > 51 ||
          pCodeParams->roi3Qp > 51 || pCodeParams->roi4Qp > 51 ||
          pCodeParams->roi5Qp > 51 || pCodeParams->roi6Qp > 51 ||
          pCodeParams->roi7Qp > 51 || pCodeParams->roi8Qp > 51) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ROI delta QP\n");
        break;
      }
    } else {
      if (pCodeParams->roi1DeltaQp < -30 ||
          pCodeParams->roi1DeltaQp > 0 ||
          pCodeParams->roi2DeltaQp < -30 ||
          pCodeParams->roi2DeltaQp > 0 /*||
         pCodeParams->adaptiveRoi < -51 ||
         pCodeParams->adaptiveRoi > 0*/)
      {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ROI delta QP\n");
        break;
      }
    }

    /* low latency : Input Line buffer check */
    if (pCodeParams->inputLineBufEn) {
      /* check zero depth */
      /* 6.0 does not check for amoutPerLoopBack */
      bool loopBackFalse = (pCodeParams->amountPerLoopBack == 0) &&
                           (HW_ID_MAJOR_NUMBER(regs->asicHwId) == 0x62);
      if ((pCodeParams->inputLineBufDepth == 0 || loopBackFalse) &&
          (pCodeParams->inputLineBufLoopBackEn ||
           pCodeParams->inputLineBufHwModeEn)) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid input buffer depth\n");
        break;
      }

      if (regs->asicCfg->prpSbiSupport &&
          !(pCodeParams->segmentUnitHeight == 8 ||
            pCodeParams->segmentUnitHeight == 16)) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid segment unit height for flexa\n");
        break;
      }
    }

    if (pCodeParams->streamMultiSegmentMode > 2 ||
        ((pCodeParams->streamMultiSegmentMode > 0) &&
         (pCodeParams->streamMultiSegmentAmount <= 1 ||
          pCodeParams->streamMultiSegmentAmount > 16))) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR Invalid stream multi-segment config\n");
      break;
    }

    if (pCodeParams->streamMultiSegmentMode != 0 &&
        pCodeParams->sliceSize != 0) {
      APITRACEERR(
          "VCEncSetCodingCtrl: ERROR multi-segment not support slice "
          "encoding\n");
      break;
    }

    if ((pCodeParams->Hdr10Display.hdr10_display_enable == ENCHW_YES) ||
        (pCodeParams->Hdr10LightLevel.hdr10_lightlevel_enable == ENCHW_YES)) {
      // parameters check
      if (!IS_HEVC(pEncInst->codecFormat) && !IS_AV1(pEncInst->codecFormat) &&
          !IS_H264(pEncInst->codecFormat)) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid Encoder Type, It should be HEVC "
            "or AV1 or H264!\n\n");
        break;
      }

      if (IS_HEVC(pEncInst->codecFormat) &&
          pEncInst->profile != VCENC_HEVC_MAIN_10_PROFILE) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid profile with HEVC, It should be "
            "HEVC_MAIN_10_PROFILE!\n\n");
        break;
      }

      if (IS_H264(pEncInst->codecFormat) &&
          pEncInst->profile != VCENC_H264_HIGH_10_PROFILE) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid profile with H264, It should be "
            "VCENC_H264_HIGH_10_PROFILE!\n\n");
        break;
      }

      /*
      if (pCodeParams->Hdr10Color.hdr10_color_enable == ENCHW_YES)
      {
        if ((pCodeParams->Hdr10Color.hdr10_matrix != 9) || (pCodeParams->Hdr10Color.hdr10_primary != 9))
        {
          APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid HDR10 colour transfer VUI parameter\n\n");
          return VCENC_INVALID_ARGUMENT;
        }
      }
      */
    }

    /* Programable ME vertical search range */
    if (pCodeParams->meVertSearchRange) {
      u8 permitVertRange[3] = {
          0,
      };
      u8 bPermit = 0;
      if (!regs->asicCfg->meVertRangeProgramable) {
        APITRACEERR(
            "VCEncSetCodingCtrl: Programable ME vertical search range not "
            "supported\n");
        break;
      }

      if (IS_H264(pEncInst->codecFormat)) {
        permitVertRange[0] = 24;
        permitVertRange[1] = 48;
        permitVertRange[2] = regs->asicCfg->meVertSearchRangeH264 * 8;
      } else {
        permitVertRange[0] = 40;
        permitVertRange[1] = 64;
        permitVertRange[2] = regs->asicCfg->meVertSearchRangeHEVC * 8;
      }

      /* check if the range is permissible */
      for (i32 i = 0; i < 3; i++) {
        if (permitVertRange[i] &&
            pCodeParams->meVertSearchRange == permitVertRange[i]) {
          bPermit = 1;
          break;
        }
      }

      if (!bPermit) {
        APITRACEERR(
            "VCEncSetCodingCtrl: Invalid ME vertical search range. Should be "
            "24|48|64 for H264; 40|64 for others.\n");
        break;
      }
    }

    if (pCodeParams->smartModeEnable &&
        !pEncInst->asic.regs.asicCfg->backgroundDetSupport) {
      APITRACEERR("VCEncSetCodingCtrl: Smart background Mode not supported\n");
      break;
    }

    if (pCodeParams->enableSao > 1 || pCodeParams->roiMapDeltaQpEnable > 1 ||
        pCodeParams->disableDeblockingFilter > 1 ||
        pCodeParams->RoimapCuCtrl_enable > 1 || pCodeParams->seiMessages > 1 ||
        pCodeParams->vuiVideoFullRange > 1) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid enable/disable\n");
      break;
    }

    if (pCodeParams->enableSao &&
        (!cfg->saoSupport && cfg->hw_asic_id >= MIN_ASIC_ID_WITH_BUILD_ID)) {
      APITRACEERR("VCEncSetCodingCtrl: SAO not support\n");
      break;
    }

    if (pCodeParams->svctEnable &&
        pEncInst->refRingBufEnable ) {
      APITRACEERR("VCEncSetCodingCtrl: refRingBuffer not support svct\n");
      break;
    }

    if (pCodeParams->sampleAspectRatioWidth > 65535 ||
        pCodeParams->sampleAspectRatioHeight > 65535) {
      APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid sampleAspectRatio\n");
      break;
    }

    if (pEncInst->aif.enable) {
      if (VCENC_OK != VCEncAifCheckParams(pEncInst, pCodeParams))
        break;
    }

#ifdef MULTI_FRAME_SUPPORT
    if (VCENC_OK != vcencBatchCheckCodingCtrlParam(pEncInst, pCodeParams))
      break;
#endif /* MULTI_FRAME_SUPPORT */

    //follow parameters only enforce when init
    if (pEncInst->encStatus == VCENCSTAT_INIT) {
      if (pCodeParams->cabacInitFlag > 1) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid cabacInitIdc\n");
        break;
      }
      if (pCodeParams->enableCabac > 2) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid enableCabac\n");
        break;
      }

      /* check limitation for H.264 baseline profile */
      if (IS_H264(pEncInst->codecFormat) && pEncInst->profile ==
          VCENC_H264_BASE_PROFILE && pCodeParams->enableCabac > 0) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid entropy coding mode for "
            "baseline profile\n");
        break;
      }

      if ((IS_H264(pEncInst->codecFormat) &&
           pCodeParams->roiMapDeltaQpBlockUnit >= 3) ||
          (IS_AV1(pEncInst->codecFormat) &&
           pCodeParams->roiMapDeltaQpBlockUnit >=
               1))  //AV1 qpDelta in CTU level
      {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid roiMapDeltaQpBlockUnit\n");
        break;
      }

      if (pCodeParams->enableRdoQuant || pCodeParams->rdoqMapEnable) {
        i32 rdoqSupport =
            (IS_H264(pEncInst->codecFormat) && regs->asicCfg->RDOQSupportH264) ||
            (IS_AV1(pEncInst->codecFormat) && regs->asicCfg->RDOQSupportAV1) ||
            (IS_HEVC(pEncInst->codecFormat) && regs->asicCfg->RDOQSupportHEVC) ||
            (IS_VP9(pEncInst->codecFormat) && regs->asicCfg->RDOQSupportVP9);
        if ((pCodeParams->enableRdoQuant || pCodeParams->rdoqMapEnable) &&
            (rdoqSupport == 0)) {
          APITRACEERR(
              "VCEncSetCodingCtrl: RDO Quant is not supported by this HW "
              "version\n");
          break;
        }

        if (VCENC_OK!=encCheckVuiParam(pEncInst, pCodeParams)) {
          break;
        }

        /* RDO Quant is not supported when cavlc is enabled for H264 encoder */
        if (IS_H264(pEncInst->codecFormat) && (pCodeParams->enableCabac == 0)) {
          if (pCodeParams->rdoqMapEnable) {
            APITRACEERR(
                "VCEncSetCodingCtrl: WARNING. RDO Quant map is not supported "
                "by H264 CAVLC and forced to be disabled\n");
            pCodeParams->rdoqMapEnable = 0;
          }
          if (pCodeParams->enableRdoQuant) {
            APITRACEERR(
                "VCEncSetCodingCtrl: WARNING. RDO Quant is not supported by "
                "H264 CAVLC and forced to be disabled\n");
            pCodeParams->enableRdoQuant = 0;
          }
        }
        /* RDO Quant is not supported by YUV444 */
        if (pEncInst->asic.regs.codedChromaIdc == VCENC_CHROMA_IDC_444) {
          if (pCodeParams->rdoqMapEnable) {
            APITRACEERR(
                "VCEncSetCodingCtrl: WARNING. RDO Quant map is not supported "
                "by codedChromaIdc 444 and forced to be disabled\n");
            pCodeParams->rdoqMapEnable = 0;
          }
          if (pCodeParams->enableRdoQuant) {
            APITRACEERR(
                "VCEncSetCodingCtrl: WARNING. RDO Quant is not supported by "
                "codedChromaIdc 444 and forced to be disabled\n");
            pCodeParams->enableRdoQuant = 0;
          }
        }
      }

      if (pCodeParams->enableDynamicRdo) {
        if (regs->asicCfg->dynamicRdoSupport == 0) {
          pCodeParams->enableDynamicRdo = 0;
          APITRACEERR(
              "VCEncSetCodingCtrl: Dynamic RDO is not supported by HW and "
              "forced to be disabled\n");
        }
      }

      if (pCodeParams->intraReconEnable) {
        if (regs->asicCfg->intraReconSupport == 0) {
          pCodeParams->intraReconEnable = 0;
        }
      }

      if (pCodeParams->lowlatGatingDisable > 1 || pCodeParams ->lowlatGatingType > 1
          || pCodeParams ->lowlatGatingCyc > 255)
          {
            APITRACEERR(
              "VCEncSetCodingCtrl: lowlatGating parameters out of range.\n");
            break;
          }
    }
    ret = VCENC_OK;
  } while (0);

  return ret;
}

#define NAL_HRD_PARA_FLAG ENCHW_YES
u32 EncGetMaxBR(VCEncVideoCodecFormat codecFormat, i32 levelIdx, i32 profile,
             i32 tier, true_e nalHrdParaFlag);
u32 EncGetMaxCPBS(VCEncVideoCodecFormat codecFormat, i32 levelIdx, i32 profile,
               i32 tier, true_e nalHrdParaFlag);
VCEncLevel getLevel(VCEncVideoCodecFormat codecFormat, i32 levelIdx);
i32 EncGetLevelIdx(VCEncVideoCodecFormat codecFormat, VCEncLevel level);

/* recalculate level based on RC parameters */
static VCEncLevel rc_recalculate_level(struct vcenc_instance *inst, u32 cpbSize,
                                       u32 bps) {
  i32 i = 0, j = 0, levelIdx = inst->levelIdx;
  i32 maxLevel = 0;
  switch (inst->codecFormat) {
    case VCENC_VIDEO_CODEC_HEVC:
      maxLevel = HEVC_LEVEL_NUM - 1;
      break;
    case VCENC_VIDEO_CODEC_H264:
      maxLevel = H264_LEVEL_NUM - 1;
      break;

    case VCENC_VIDEO_CODEC_AV1:
      maxLevel = AV1_VALID_MAX_LEVEL - 1;
      break;

    case VCENC_VIDEO_CODEC_VP9:
      maxLevel = VP9_VALID_MAX_LEVEL - 1;
      break;

    default:
      break;
  }

  if (cpbSize >
      EncGetMaxCPBS(inst->codecFormat, maxLevel, inst->profile, inst->tier, NAL_HRD_PARA_FLAG)) {
    APITRACEERR("rc_recalculate_level: WARNING Invalid cpbSize.\n");
    i = j = maxLevel;
  }
  if (bps > EncGetMaxBR(inst->codecFormat, maxLevel, inst->profile, inst->tier, NAL_HRD_PARA_FLAG)) {
    APITRACEERR("rc_recalculate_level: WARNING Invalid bitsPerSecond.\n");
    i = j = maxLevel;
  }
  for (i = 0; i < maxLevel; i++) {
    if (cpbSize <= EncGetMaxCPBS(inst->codecFormat, i, inst->profile, inst->tier, NAL_HRD_PARA_FLAG))
      break;
  }
  for (j = 0; j < maxLevel; j++) {
    if (bps <= EncGetMaxBR(inst->codecFormat, j, inst->profile, inst->tier, NAL_HRD_PARA_FLAG)) break;
  }

  levelIdx = MAX(levelIdx, MAX(i, j));
  return (getLevel(inst->codecFormat, levelIdx));
}
/*------------------------------------------------------------------------------
    Function name : EncCheckRateCtrlParam
    Description   : check inputed rate ctrl parameters
    Return type   : VCEncRet
    Argument      : inst - encoder instance
    Argument      : pRateCtrl - rate ctrl
------------------------------------------------------------------------------*/
VCEncRet EncCheckRateCtrlParam(struct vcenc_instance *vcenc_instance,
                                 VCEncRateCtrl *pRateCtrl) {
  regValues_s *regs = &vcenc_instance->asic.regs;
  i32 ctbRcAllowed;
  u32 i, tmp;
  VCEncRet ret = VCENC_INVALID_ARGUMENT;

  ctbRcAllowed = vcenc_instance->encStatus < VCENCSTAT_START_STREAM ||
                 vcenc_instance->asic.regs.cuQpDeltaEnabled;

  /* Check for existing instance */
  if (vcenc_instance->inst != vcenc_instance) {
    APITRACEERR("VCEncSetRateCtrl: ERROR Invalid instance\n");
    return VCENC_INSTANCE_ERROR;
  }

  do {
    if (pRateCtrl->ctbRcQpDeltaReverse > 1) {
      APITRACEERR("VCEncSetRateCtrl: ERROR ctbRcQpDeltaReverse out of range\n");
      return VCENC_INVALID_ARGUMENT;
    }
    /* after stream was started with HRD ON,
     * it is not allowed to change RC params */
    if (vcenc_instance->encStatus == VCENCSTAT_START_FRAME &&
        pRateCtrl->hrd == ENCHW_YES) {
      APITRACEERR(
          "VCEncSetRateCtrl: ERROR Stream started with HRD ON. Not allowed to "
          "change any parameters\n");
      return VCENC_INVALID_STATUS;
    }

    /* check ctbRc setting */
    if (pRateCtrl->ctbRc) {
      if (pRateCtrl->ctbRc > 3) {
        APITRACEERR("VCEncSetCodingCtrl: ERROR Invalid ctbRc mode\n");
        return VCENC_INVALID_ARGUMENT;
      }
      if (ctbRcAllowed == 0) {
        APITRACEERR(
            "VCEncSetCodingCtrl: ERROR Invalid encoding status to enable "
            "ctbRc\n");
        return VCENC_INVALID_ARGUMENT;
      }
      if (HW_ID_MAJOR_NUMBER(regs->asicHwId) < 2 /* H2V1 */) {
        APITRACEERR("VCEncSetRateCtrl: ERROR CTB RC not supported\n");
        return VCENC_INVALID_ARGUMENT;
      }
      if (pRateCtrl->blockRCSize > 2 ||
          (IS_AV1(vcenc_instance->codecFormat) &&
           pRateCtrl->blockRCSize > 0))  //AV1 qpDelta in CTU level
      {
        APITRACEERR("VCEncSetRateCtrl: ERROR Invalid blockRCSize\n");
        return VCENC_INVALID_ARGUMENT;
      }
      if (pRateCtrl->ctbRc >= 2 && pRateCtrl->crf >= 0) {
        APITRACEERR("VCEncSetRateCtrl: ERROR crf is set with ctbRc>=2\n");
        return VCENC_INVALID_ARGUMENT;
      }
    }

    /* Check for invalid input values */
    if (pRateCtrl->pictureRc > 1 || pRateCtrl->pictureSkip > 1 ||
        pRateCtrl->hrd > 1) {
      APITRACEERR("VCEncSetRateCtrl: ERROR Invalid enable/disable value\n");
      return VCENC_INVALID_ARGUMENT;
    }

    /* Check the fillData when hrd off */
    if (pRateCtrl->fillerData &&
        (pRateCtrl->hrdCpbSize == 0 || pRateCtrl->pictureRc != 1)) {
      APITRACEERR(
          "VCEncSetRateCtrl: ERROR Invalid cpbSize value or didn't open the RC "
          "when fillerData is 1\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if (pRateCtrl->qpHdr > 51 || pRateCtrl->qpMinPB > 51 ||
        pRateCtrl->qpMaxPB > 51 || pRateCtrl->qpMaxPB < pRateCtrl->qpMinPB ||
        pRateCtrl->qpMinI > 51 || pRateCtrl->qpMaxI > 51 ||
        pRateCtrl->qpMaxI < pRateCtrl->qpMinI) {
      APITRACEERR("VCEncSetRateCtrl: ERROR Invalid QP\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if ((u32)(pRateCtrl->intraQpDelta + 51) > 102) {
      APITRACEERR("VCEncSetRateCtrl: ERROR intraQpDelta out of range\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if (pRateCtrl->fixedIntraQp > 51) {
      APITRACEERR("VCEncSetRateCtrl: ERROR fixedIntraQp out of range\n");
      return VCENC_INVALID_ARGUMENT;
    }
    if (pRateCtrl->bitrateWindow < 1 || pRateCtrl->bitrateWindow > 300) {
      APITRACEERR("VCEncSetRateCtrl: ERROR Invalid GOP length\n");
      return VCENC_INVALID_ARGUMENT;
    }
    if (pRateCtrl->monitorFrames < LEAST_MONITOR_FRAME ||
        pRateCtrl->monitorFrames > 120) {
      APITRACEERR("VCEncSetRateCtrl: ERROR Invalid monitorFrames\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if (pRateCtrl->blockRCSize > 2 || pRateCtrl->ctbRc > 3 ||
        (IS_AV1(vcenc_instance->codecFormat) &&
         pRateCtrl->blockRCSize > 0))  //AV1 qpDelta in CTU level
    {
      APITRACEERR("VCEncSetRateCtrl: ERROR Invalid blockRCSize\n");
      return VCENC_INVALID_ARGUMENT;
    }

    /* Frame rate may change. */
    if (pRateCtrl->frameRateDenom == 0 || pRateCtrl->frameRateNum == 0) {
      APITRACEERR(
          "VCEncSetRateCtrl: ERROR Invalid frameRateDenom, frameRateNum\n");
      return VCENC_INVALID_ARGUMENT;
    }

    /* Bitrate affects only when rate control is enabled */
    if ((pRateCtrl->pictureRc || pRateCtrl->pictureSkip || pRateCtrl->hrd) &&
        (((pRateCtrl->bitPerSecond < 10000) &&
          (pRateCtrl->frameRateNum > pRateCtrl->frameRateDenom)) ||
         ((((pRateCtrl->bitPerSecond * pRateCtrl->frameRateDenom) / pRateCtrl->frameRateNum) <
           10000) &&
          (pRateCtrl->frameRateNum < pRateCtrl->frameRateDenom)) ||
         pRateCtrl->bitPerSecond > VCENC_MAX_BITRATE)) {
      APITRACEERR("VCEncSetRateCtrl: ERROR Invalid bitPerSecond\n");
      return VCENC_INVALID_ARGUMENT;
    }

    /* HRD and VBR are conflict */
    if (pRateCtrl->hrd && pRateCtrl->vbr) {
      APITRACEERR(
          "VCEncSetRateCtrl: ERROR HRD and VBR can not be enabled at the same "
          "time\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if ((pRateCtrl->picQpDeltaMin > -1) || (pRateCtrl->picQpDeltaMin < -10) ||
        (pRateCtrl->picQpDeltaMax < 1) || (pRateCtrl->picQpDeltaMax > 10)) {
      APITRACEERR(
          "VCEncSetRateCtrl: ERROR picQpRange out of range. Min:Max should be in "
          "[-1,-10]:[1,10]\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if (pRateCtrl->ctbRc >= 2 && pRateCtrl->crf >= 0) {
      APITRACEERR("VCEncSetRateCtrl: ERROR crf is set with ctbRc>=2\n");
      return VCENC_INVALID_ARGUMENT;
    }

    if (vcenc_instance->pass == 0 && pRateCtrl->crf >= 0) {
      APITRACEERR("VCEncSetRateCtrl: ERROR crf is set when one-pass encoding\n");
      return VCENC_INVALID_ARGUMENT;
    }
    {
      u32 cpbSize = pRateCtrl->hrdCpbSize;
      u32 bps = pRateCtrl->bitPerSecond;
      u32 level = vcenc_instance->levelIdx;

      /* Limit maximum bitrate based on resolution and frame rate */
      /* Saturates really high settings */
      /* bits per unpacked frame */
      tmp = (vcenc_instance->sps->bit_depth_chroma_minus8 / 2 +
          vcenc_instance->sps->bit_depth_luma_minus8 + 12) *
        vcenc_instance->ctbPerFrame * vcenc_instance->max_cu_size *
        vcenc_instance->max_cu_size;
      /* bits per second */
      tmp =
        MIN((((i64)rcCalculate(tmp, pRateCtrl->frameRateNum, pRateCtrl->frameRateDenom)) * 5 / 3),
            I32_MAX);
      if (bps > (u32)tmp) bps = (u32)tmp;

      if (vcenc_instance->bAutoLevel) {
        level = rc_recalculate_level(vcenc_instance, cpbSize, bps);
        if (level == -1) {
          return VCENC_INVALID_ARGUMENT;
        }
        level = EncGetLevelIdx(vcenc_instance->codecFormat, level);
      }
      /* if HRD is ON we have to obay all its limits */
      if (pRateCtrl->hrd != 0) {
        if (cpbSize == 0)
          cpbSize = EncGetMaxCPBS(vcenc_instance->codecFormat, level,
              vcenc_instance->profile, vcenc_instance->tier, NAL_HRD_PARA_FLAG);
        else if (cpbSize == (u32)(-1))
          cpbSize = bps;
        if (cpbSize > 0x7FFFFFFF) cpbSize = 0x7FFFFFFF;

        /* Limit minimum CPB size based on average bits per frame */
        tmp = rcCalculate(bps, pRateCtrl->frameRateDenom, pRateCtrl->frameRateNum);
        cpbSize = MAX(cpbSize, tmp);

        /* cpbSize must be rounded so it is exactly the size written in stream */
        i = 0;
        tmp = cpbSize;
        while (4095 < (tmp >> (4 + i++)))
          ;

        cpbSize = (tmp >> (4 + i)) << (4 + i);

        if (cpbSize > EncGetMaxCPBS(vcenc_instance->codecFormat, level,
              vcenc_instance->profile, vcenc_instance->tier, NAL_HRD_PARA_FLAG)) {
          APITRACEERR(
              "VCEncSetRateCtrl: ERROR. HRD is ON. hrdCpbSize higher than "
              "maximum allowed for stream level\n");
          return VCENC_INVALID_ARGUMENT;
        }

        if (bps > EncGetMaxBR(vcenc_instance->codecFormat, level,
              vcenc_instance->profile, vcenc_instance->tier, NAL_HRD_PARA_FLAG)) {
          APITRACEERR(
              "VCEncSetRateCtrl: ERROR. HRD is ON. bitPerSecond higher than "
              "maximum allowed for stream level\n");
          return VCENC_INVALID_ARGUMENT;
        }
      }
      if (pRateCtrl->ctbRc) {
        u32 maxCtbRcQpDelta = (regs->asicCfg->ctbRcVersion > 1) ? 51 : 15;

        /* If CTB QP adjustment for Rate Control not supported, just disable it, not return error. */
        if (IS_CTBRC_FOR_BITRATE(pRateCtrl->ctbRc) &&
            (regs->asicCfg->ctbRcVersion == 0)) {
          CLR_CTBRC_FOR_BITRATE(pRateCtrl->ctbRc);
          APITRACEERR(
              "VCEncSetRateCtrl: ERROR CTB QP adjustment for Rate Control not "
              "supported, Disabled it\n");
        }

        if (pRateCtrl->rcQpDeltaRange > maxCtbRcQpDelta) {
          pRateCtrl->rcQpDeltaRange = maxCtbRcQpDelta;
          APITRACEERR(
              "VCEncSetRateCtrl: rcQpDeltaRange too big, Clipped it into valid "
              "range\n");
        }

      }
    }

    ret = VCENC_OK;
  } while (0);

  return ret;
}

/* insert req into fpsRequestQueue in decending order of frameRateUpdatePicCnt */
bool EnqueueFpsRequest(struct queue *fpsRequestQueue, EncFpsRequest *req)
{
  struct node *prev = NULL, *p = NULL;
  EncFpsRequest *pReq;

  pReq = (EncFpsRequest *)(fpsRequestQueue->tail);
  if (fpsRequestQueue->tail == NULL || pReq->frameRateUpdatePicCnt <= req->frameRateUpdatePicCnt) {
    if(pReq && pReq->frameRateUpdatePicCnt == req->frameRateUpdatePicCnt) {
      pReq->frameRateNum = req->frameRateNum;
      pReq->frameRateDenom = req->frameRateDenom;
      return false;
    }
    queue_put_tail(fpsRequestQueue, (struct node *)req);
    return true;
  }

  for (prev = fpsRequestQueue->tail, p = fpsRequestQueue->tail->next; p; prev = p, p = p->next) {
    pReq = (EncFpsRequest *)p;
    if(pReq->frameRateUpdatePicCnt <= req->frameRateUpdatePicCnt)
      break;
  }
  if(!p) {
    queue_put(fpsRequestQueue, (struct node *)req);
    return true;
  } else if (pReq->frameRateUpdatePicCnt == req->frameRateUpdatePicCnt) {
    pReq->frameRateNum = req->frameRateNum;
    pReq->frameRateDenom = req->frameRateDenom;
    return false;
  } else {
    req->next = prev->next;
    prev->next = (struct node *)req;
    return true;
  }
  return false;
}

/* check fps adjust request for picture_cnt */
bool checkFpsUpdate(struct vcenc_instance *vcenc_instance, int picture_cnt)
{
  struct queue *fpsRequestQueue = &vcenc_instance->rateCtrl.fpsRequestQueue;
  EncFpsRequest *req = (EncFpsRequest *)queue_head(fpsRequestQueue);
  if(!req || req->frameRateUpdatePicCnt > picture_cnt)
    return false;
  return true;
}

VCEncJob *findJob(struct queue *jobQueue, int picture_cnt)
{
  VCEncJob *job = NULL;
  job = (VCEncJob *)queue_tail(jobQueue);

  /* get job from job queue */
  while (NULL != job) {
    if (picture_cnt == job->encIn.picture_cnt)
      break;
    job = (VCEncJob *)job->next;
  }
  return job;
}
void replaceRateParam(struct vcenc_instance *vcenc_instance, struct queue *jobQueue, EncRateCtrlParam *param, EncRateCtrlParam *paramNew)
{
  VCEncJob *job = NULL;
  job = (VCEncJob *)queue_tail(jobQueue);

  /* get job from job queue */
  while (NULL != job) {
    if (param == job->pRateCtrlParam) {
      job->pRateCtrlParam = paramNew;
      param->refCnt--;
      paramNew->refCnt++;
    }
    job = (VCEncJob *)job->next;
  }
  EncRateCtrlParam *top = (EncRateCtrlParam *)queue_head(&vcenc_instance->rateCtrl.rateCtrlQueue);
  if(top == param) {
    queue_remove(&vcenc_instance->rateCtrl.rateCtrlQueue, (struct node *)top);
    queue_put(&vcenc_instance->rateCtrl.rateCtrlQueue, (struct node *)paramNew);
    param->refCnt--;
    paramNew->refCnt++;
  }
}
void replaceRateParamPass1(struct vcenc_instance *vcenc_instance, struct queue *jobQueue, EncRateCtrlParam *param, EncRateCtrlParam *paramNew)
{
  VCEncLookaheadJob *job = NULL;
  job = (VCEncLookaheadJob *)queue_tail(jobQueue);

  /* get job from job queue */
  while (NULL != job) {
    if (param == job->pRateCtrlParam) {
      job->pRateCtrlParam = paramNew;
      param->refCnt--;
      paramNew->refCnt++;
    }
    job = (VCEncLookaheadJob *)job->next;
  }
  EncRateCtrlParam *top = (EncRateCtrlParam *)queue_head(&vcenc_instance->rateCtrl.rateCtrlQueue);
  if(top == param) {
    queue_remove(&vcenc_instance->rateCtrl.rateCtrlQueue, (struct node *)top);
    queue_put(&vcenc_instance->rateCtrl.rateCtrlQueue, (struct node *)paramNew);
    param->refCnt--;
    paramNew->refCnt++;
  }
}

/* apply fps update for picture_cnt */
VCEncRet EncFpsUpdate(struct vcenc_instance *vcenc_instance,
                  struct queue *jobQueue,
                  EncRateCtrlParam *pRateCtrlParam,
                  const VCEncIn *pEncIn) {
  VCEncRet ret = VCENC_OK;
  struct queue *fpsRequestQueue = &vcenc_instance->rateCtrl.fpsRequestQueue;
  vcencRateControl_s *rc = &vcenc_instance->rateControl;
  /* Find the recent request matching given picture_cnt */
  EncFpsRequest *req = (EncFpsRequest *)queue_head(fpsRequestQueue);
  if(!req || req->frameRateUpdatePicCnt > pEncIn->picture_cnt)
    return ret;
  queue_remove(fpsRequestQueue, (struct node *)req);
  EncFpsRequest *req1 = (EncFpsRequest *)queue_head(fpsRequestQueue);
  while(req1 && req->frameRateUpdatePicCnt <= pEncIn->picture_cnt) {
    DynamicPutBufferToPool(&vcenc_instance->rateCtrl.fpsRequestBufPool,
        (void *)req);
    req = req1;
    queue_remove(fpsRequestQueue, (struct node *)req);
  }
  /* duplicate rate ctrl param and update frame rate */
  EncRateCtrlParam *pRateCtrlParamNew = (EncRateCtrlParam *)DynamicGetBufferFromPool(
      &vcenc_instance->rateCtrl.rateCtrlBufPool, sizeof(EncRateCtrlParam));
  if (!pRateCtrlParamNew) {
    APITRACEERR("EncUpdateFPS: ERROR Get rate ctrl buffer failed\n");
    return VCENC_ERROR;
  }
  VCEncRateCtrl *pRateCtrl = &pRateCtrlParamNew->encRateCtrl;
  memcpy(pRateCtrl, &pRateCtrlParam->encRateCtrl, sizeof(VCEncRateCtrl));
  pRateCtrl->bitPerSecond = ((rc->virtualBuffer.bitRate * req->frameRateNum * rc->outRateDenom)
              / (req->frameRateDenom * rc->outRateNum));
  //printf("%u/%u %u -> %u/%u %u\n", rc->outRateNum, rc->outRateDenom, rc->virtualBuffer.bitRate, req->frameRateNum, req->frameRateDenom, pRateCtrl->bitPerSecond);
  pRateCtrl->frameRateNum = req->frameRateNum;
  pRateCtrl->frameRateDenom = req->frameRateDenom;
  pRateCtrl->frameRateUpdatePicCnt = req->frameRateUpdatePicCnt;
  DynamicPutBufferToPool(&vcenc_instance->rateCtrl.fpsRequestBufPool,
      (void *)req);
  pRateCtrlParamNew->startPicCnt = pEncIn->picture_cnt;
  pRateCtrlParamNew->refCnt = 0;
  if(vcenc_instance->pass == 0)
    replaceRateParam(vcenc_instance, jobQueue, pRateCtrlParam, pRateCtrlParamNew);
  else
    replaceRateParamPass1(vcenc_instance, jobQueue, pRateCtrlParam, pRateCtrlParamNew);
  /* remove old if unused */
  if (0 == pRateCtrlParam->refCnt) {
    queue_remove(&vcenc_instance->rateCtrl.rateCtrlQueue,
        (struct node *)pRateCtrlParam);
    vcenc_instance->rateCtrl.pRateCtrlParam = NULL;
    DynamicPutBufferToPool(&vcenc_instance->rateCtrl.rateCtrlBufPool,
        (void *)pRateCtrlParam);
  }
  return ret;
}

/*------------------------------------------------------------------------------
    Function name : EncUpdateRateCtrlParam
    Description   : update rate ctrl parameters matched with current frame into instance
    Return type   : void
    Argument      : inst - encoder instance
    Argument      : pRateCtrlParam - rate ctrl
    Argumnet      : picCnt - current picture cnt
------------------------------------------------------------------------------*/
void EncUpdateRateCtrlParam(struct vcenc_instance *vcenc_instance,
                              EncRateCtrlParam *pRateCtrlParam,
                              const VCEncIn *pEncIn) {
  vcencRateControl_s *rc;
  u32 i, tmp;
  i32 prevBitrate;

  if ((vcenc_instance == NULL) || (pRateCtrlParam == NULL))
    return;

  VCEncRateCtrl *pRateCtrl = &pRateCtrlParam->encRateCtrl;
  regValues_s *regs = &vcenc_instance->asic.regs;
  i32 bitrateWindow;
  i32 rate_control_mode_change = 0;
  i32 frame_rate_change = 0;
  i32 ctbRcAllowed;
  rc = &vcenc_instance->rateControl;

  if (vcenc_instance->encStatus != VCENCSTAT_INIT && vcenc_instance->rateCtrl.pRateCtrlParam == pRateCtrlParam)
    return;
  vcenc_instance->rateCtrl.pRateCtrlParam = pRateCtrlParam;

  ctbRcAllowed = vcenc_instance->encStatus < VCENCSTAT_START_STREAM ||
                 vcenc_instance->asic.regs.cuQpDeltaEnabled;

  rc->ctbRcQpDeltaReverse = pRateCtrl->ctbRcQpDeltaReverse;

  /* rate control mode has been changed. Only consider change between cbr and vbr. Constrain couple (hrd=1,vbr=0),(hrd=0,vbr=1)*/
  if (rc->hrd != (true_e)pRateCtrl->hrd || rc->vbr != (true_e)pRateCtrl->vbr) {
    rate_control_mode_change = 1;
  }

  if ((vcenc_instance->encStatus == VCENCSTAT_INIT || pRateCtrl->frameRateUpdatePicCnt >= 0) && (rc->outRateNum != (i32)pRateCtrl->frameRateNum ||
      rc->outRateDenom != (i32)pRateCtrl->frameRateDenom)) {
    /*Though only when ((rc->outRateNum/rc->outRateDenom) != (i32)(pRateCtrl->frameRateNum/pRateCtrl->frameRateDenom)) the frame rate is changed,
    we consider all the situation as frame rate is changed, and sps will change.*/
    frame_rate_change = 1;
    rc->outRateNum = pRateCtrl->frameRateNum;
    rc->outRateDenom = pRateCtrl->frameRateDenom;
  }

  {
    u32 cpbSize = pRateCtrl->hrdCpbSize;
    u32 bps = pRateCtrl->bitPerSecond;
    u32 level = vcenc_instance->levelIdx;
    u32 cpbMaxRate = pRateCtrl->cpbMaxRate;

    /* Limit maximum bitrate based on resolution and frame rate */
    /* Saturates really high settings */
    /* bits per unpacked frame */
    tmp = (vcenc_instance->sps->bit_depth_chroma_minus8 / 2 +
           vcenc_instance->sps->bit_depth_luma_minus8 + 12) *
          vcenc_instance->ctbPerFrame * vcenc_instance->max_cu_size *
          vcenc_instance->max_cu_size;
    rc->i32MaxPicSize = tmp;
    /* bits per second */
    tmp =
        MIN((((i64)rcCalculate(tmp, rc->outRateNum, rc->outRateDenom)) * 5 / 3),
            I32_MAX);
    if (bps > (u32)tmp) bps = (u32)tmp;

    if (vcenc_instance->bAutoLevel) {
      vcenc_instance->level =
          rc_recalculate_level(vcenc_instance, cpbSize, bps);
      level = vcenc_instance->levelIdx =
          EncGetLevelIdx(vcenc_instance->codecFormat, vcenc_instance->level);
    }
    /* if HRD is ON we have to obay all its limits */
    if (pRateCtrl->hrd != 0) {
      if (cpbSize == 0)
        cpbSize = EncGetMaxCPBS(vcenc_instance->codecFormat, level,
          vcenc_instance->profile, vcenc_instance->tier, NAL_HRD_PARA_FLAG);
      else if (cpbSize == (u32)(-1))
        cpbSize = bps;
      if (cpbSize > 0x7FFFFFFF) cpbSize = 0x7FFFFFFF;

      if (cpbMaxRate == 0) cpbMaxRate = bps;

      /* Limit minimum CPB size based on average bits per frame */
      tmp = rcCalculate(bps, rc->outRateDenom, rc->outRateNum);
      cpbSize = MAX(cpbSize, tmp);

      /* cpbSize must be rounded so it is exactly the size written in stream */
      i = 0;
      tmp = cpbSize;
      while (4095 < (tmp >> (4 + i++)))
        ;

      cpbSize = (tmp >> (4 + i)) << (4 + i);

    }

    if (cpbMaxRate || cpbSize) {
      /* if cpbMaxRate or cpbSize is set, cpbMaxRate can't < bps */
      cpbMaxRate = MAX(bps, cpbMaxRate);

      /* if cpbMaxRate is not 0, cpbSize mustn't be 0 and is set to default 2*bps*/
      if (cpbSize == 0) cpbSize = 2 * bps;
    }

    rc->virtualBuffer.bufferSize = cpbSize;
    rc->virtualBuffer.maxBitRate = cpbMaxRate;
    rc->virtualBuffer.bufferRate =
        cpbMaxRate * rc->outRateDenom / rc->outRateNum;

    if (cpbMaxRate > bps)
      rc->cbr_flag = 0;
    else
      rc->cbr_flag = 1;

    /* Set the parameters to rate control */
    if (pRateCtrl->pictureRc != 0)
      rc->picRc = ENCHW_YES;
    else
      rc->picRc = ENCHW_NO;

    /* CTB_RC */
    rc->rcQpDeltaRange = pRateCtrl->rcQpDeltaRange;
    rc->rcBaseMBComplexity = pRateCtrl->rcBaseMBComplexity;
    rc->picQpDeltaMin = pRateCtrl->picQpDeltaMin;
    rc->picQpDeltaMax = pRateCtrl->picQpDeltaMax;
    rc->ctbRc = pRateCtrl->ctbRc;
    rc->tolCtbRcInter = pRateCtrl->tolCtbRcInter;
    rc->tolCtbRcIntra = pRateCtrl->tolCtbRcIntra;
    /* look-ahead always use fixed-QP; enable ctbRc if cpb is set */
    if (vcenc_instance->pass == 1)
      rc->ctbRc = 0;
    else if (rc->virtualBuffer.bufferSize && ctbRcAllowed &&
             IS_CTBRC_FOR_BITRATE(rc->ctbRc) == 0 &&
             vcenc_instance->pass != 2) {
      /* With cpb, ctbRc mode 2 is enabled for 1-pass encoding */
//      rc->ctbRc |= 2;
      if (regs->asicCfg->ctbRcVersion != 2)
      {
        rc->tolCtbRcInter = rc->tolCtbRcIntra = -1.0;
      }
    }

    if (rc->ctbRc) {
      u32 maxCtbRcQpDelta = (regs->asicCfg->ctbRcVersion > 1) ? 51 : 15;

      /* If CTB QP adjustment for Rate Control not supported, just disable it, not return error. */
      if (IS_CTBRC_FOR_BITRATE(rc->ctbRc) &&
          (regs->asicCfg->ctbRcVersion == 0)) {
        CLR_CTBRC_FOR_BITRATE(rc->ctbRc);
      }

      if (rc->rcQpDeltaRange > maxCtbRcQpDelta) {
        rc->rcQpDeltaRange = maxCtbRcQpDelta;
      }

      if (IS_CTBRC_FOR_BITRATE(rc->ctbRc)) rc->picRc = ENCHW_YES;

      if (IS_CTBRC_FOR_QUALITY(rc->ctbRc)) {
        vcenc_instance->blockRCSize = pRateCtrl->blockRCSize;
        vcenc_instance->log2_qp_size =
            MIN(6 - vcenc_instance->blockRCSize, vcenc_instance->log2_qp_size);
      }
    }

    if (pRateCtrl->pictureSkip != 0)
      rc->picSkip = ENCHW_YES;
    else
      rc->picSkip = ENCHW_NO;

    rc->fillerData = pRateCtrl->fillerData;
    rc->tolUnderflow = CLIP3(0, 99, pRateCtrl->tolRcUnderflow);
    if (pRateCtrl->hrd != 0) {
      rc->hrd = ENCHW_YES;
      rc->picRc = ENCHW_YES;
    } else
      rc->hrd = ENCHW_NO;

    rc->vbr = pRateCtrl->vbr ? ENCHW_YES : ENCHW_NO;
    rc->qpHdr = pRateCtrl->qpHdr << QP_FRACTIONAL_BITS;
    rc->qpMinPB = pRateCtrl->qpMinPB << QP_FRACTIONAL_BITS;
    rc->qpMaxPB = pRateCtrl->qpMaxPB << QP_FRACTIONAL_BITS;
    rc->qpMinI = pRateCtrl->qpMinI << QP_FRACTIONAL_BITS;
    rc->qpMaxI = pRateCtrl->qpMaxI << QP_FRACTIONAL_BITS;
    rc->rcMode = pRateCtrl->rcMode;
    prevBitrate = rc->virtualBuffer.bitRate;
    rc->virtualBuffer.bitRate = bps;
    bitrateWindow = rc->bitrateWindow;
    rc->bitrateWindow = pRateCtrl->bitrateWindow;
    rc->maxPicSizeI =
        MIN(((((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) / 100) *
             (100 + pRateCtrl->bitVarRangeI)),
            rc->i32MaxPicSize);
    rc->maxPicSizeP =
        MIN(((((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) / 100) *
             (100 + pRateCtrl->bitVarRangeP)),
            rc->i32MaxPicSize);
    rc->maxPicSizeB =
        MIN(((((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) / 100) *
             (100 + pRateCtrl->bitVarRangeB)),
            rc->i32MaxPicSize);

    rc->minPicSizeI =
        (i32)(((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) * 100 /
              (100 + pRateCtrl->bitVarRangeI));
    rc->minPicSizeP =
        (i32)(((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) * 100 /
              (100 + pRateCtrl->bitVarRangeP));
    rc->minPicSizeB =
        (i32)(((i64)rcCalculate(bps, rc->outRateDenom, rc->outRateNum)) * 100 /
              (100 + pRateCtrl->bitVarRangeB));

    rc->tolMovingBitRate = pRateCtrl->tolMovingBitRate;
    //    rc->f_tolMovingBitRate = (float)rc->tolMovingBitRate;		//floating tolMovingBitRate seems no use
    rc->monitorFrames = pRateCtrl->monitorFrames;
    rc->u32StaticSceneIbitPercent = pRateCtrl->u32StaticSceneIbitPercent;
    rc->tolCtbRcInter = pRateCtrl->tolCtbRcInter;
    rc->tolCtbRcIntra = pRateCtrl->tolCtbRcIntra;
    rc->ctbRateCtrl.qpStep = regs->asicCfg->ctbRcVersion == 2 ? pRateCtrl->ctbRcRowQpStep :
        (((pRateCtrl->ctbRcRowQpStep << CTB_RC_QP_STEP_FIXP) + rc->ctbCols / 2) /
        rc->ctbCols);
    rc->ctbRateCtrl.ctbRcRowQpDeltaRange = regs->asicCfg->ctbRcVersion == 2 ? pRateCtrl->ctbRcRowQpDeltaRange : 0;
    rc->maxIprop = pRateCtrl->maxIprop;
    rc->minIprop = pRateCtrl->minIprop;
    rc->changePos = pRateCtrl->changePos;
  }

  rc->intraQpDelta = pRateCtrl->intraQpDelta << QP_FRACTIONAL_BITS;
  rc->fixedIntraQp = pRateCtrl->fixedIntraQp << QP_FRACTIONAL_BITS;
  rc->frameQpDelta = 0 << QP_FRACTIONAL_BITS;
  rc->smooth_psnr_in_gop = pRateCtrl->smoothPsnrInGOP;
  rc->longTermQpDelta = pRateCtrl->longTermQpDelta << QP_FRACTIONAL_BITS;

  /*CRF parameters: pb and ip offset is set to default value
    Todo: set it available to user*/
  rc->crf = pRateCtrl->crf;
  //rc->pbOffset = 6.0 * LOG2(1.3f);
  //rc->ipOffset = 6.0 * LOG2(1.4f);
#ifndef RCP_FIXED_POINT_OPT
  rc->pbOffset = 6.0 * 0.378512;
  rc->ipOffset = 6.0 * 0.485427;
#endif
  rc->hieQpDeltaEnable = pRateCtrl->hieQpDeltaEnable && (vcenc_instance->pass == 0);
  /* New parameters checked already so ignore return value.
  * Reset RC bit counters when changing bitrate. */
  (void)VCEncInitRc(rc, (vcenc_instance->encStatus == VCENCSTAT_INIT) ||
                            (bitrateWindow != rc->bitrateWindow) ||
                            rate_control_mode_change);

  if (vcenc_instance->encStatus <= VCENCSTAT_START_STREAM &&
      vcenc_instance->bInputfileList) {
    memcpy(&vcenc_instance->rateControl_ori, pRateCtrl, sizeof(*pRateCtrl));
  }

  vcenc_instance->asic.regs.bRateCtrlUpdate = 1;

end:
  if(vcenc_instance->pass != 1) {
    pRateCtrlParam->refCnt--;
    //remove useless parameters from queue
    if (0 == pRateCtrlParam->refCnt) {
      queue_remove(&vcenc_instance->rateCtrl.rateCtrlQueue,
          (struct node *)pRateCtrlParam);
      DynamicPutBufferToPool(&vcenc_instance->rateCtrl.rateCtrlBufPool,
          (void *)pRateCtrlParam);
    }
  }
}

/* get client type according to coding type */
CLIENT_TYPE VCEncGetClientType(VCEncVideoCodecFormat codecFormat) {
  u32 clientType = EWL_CLIENT_TYPE_JPEG_ENC;
  if (IS_H264(codecFormat))
    clientType = EWL_CLIENT_TYPE_H264_ENC;
  else if (IS_AV1(codecFormat))
    clientType = EWL_CLIENT_TYPE_AV1_ENC;
  else if (IS_VP9(codecFormat))
    clientType = EWL_CLIENT_TYPE_VP9_ENC;
  else if (IS_HEVC(codecFormat))
    clientType = EWL_CLIENT_TYPE_HEVC_ENC;
  else {
    ASSERT(0 && "Unsupported codecFormat");
  }
  return clientType;
}

/** hevc and h264 HDR10 mdc and cll */
void VCEncEncodeSeiHdr10(struct vcenc_instance *vcenc_instance,
                         VCEncOut *pEncOut) {
  u32 tmp = 0;
  u32 byte_stream = (vcenc_instance->sps->streamMode == ASIC_VCENC_BYTE_STREAM);
  u32 enc_status = (vcenc_instance->encStatus == VCENCSTAT_INIT);

  if ((vcenc_instance->Hdr10Display.hdr10_display_enable == (u8)HDR10_CFGED)) {
    u32 u32SeiCnt = 0, nal_size = 0;
    if (enc_status)
      vcenc_instance->stream.cnt =
          &u32SeiCnt;  //just used to initial the cnt when StrmStart()
    else
      tmp = vcenc_instance->stream.byteCnt;

    if (IS_HEVC(vcenc_instance->codecFormat))
      HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, byte_stream);
    else if (IS_H264(vcenc_instance->codecFormat))
      H264NalUnitHdr(&vcenc_instance->stream, 0, H264_SEI, byte_stream);

    HevcMasteringDisplayColourSei(&vcenc_instance->stream,
                                  &vcenc_instance->Hdr10Display);
    if (enc_status)
      nal_size = u32SeiCnt;
    else
      nal_size = vcenc_instance->stream.byteCnt - tmp;

    pEncOut->streamSize += nal_size;
    VCEncAddNaluSize(pEncOut, nal_size, 0);
    pEncOut->PreDataSize += nal_size;
    pEncOut->PreNaluNum++;
    hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, nal_size);
  }

  if ((vcenc_instance->Hdr10LightLevel.hdr10_lightlevel_enable ==
       (u8)HDR10_CFGED)) {
    u32 u32SeiCnt = 0, nal_size = 0;
    if (enc_status)
      vcenc_instance->stream.cnt =
          &u32SeiCnt;  //just used to initial the cnt when StrmStart()
    else
      tmp = vcenc_instance->stream.byteCnt;

    if (IS_HEVC(vcenc_instance->codecFormat))
      HevcNalUnitHdr(&vcenc_instance->stream, PREFIX_SEI_NUT, byte_stream);
    else if (IS_H264(vcenc_instance->codecFormat))
      H264NalUnitHdr(&vcenc_instance->stream, 0, H264_SEI, byte_stream);

    HevcContentLightLevelSei(&vcenc_instance->stream,
                             &vcenc_instance->Hdr10LightLevel);
    if (enc_status)
      nal_size = u32SeiCnt;
    else
      nal_size = vcenc_instance->stream.byteCnt - tmp;

    pEncOut->streamSize += nal_size;
    VCEncAddNaluSize(pEncOut, nal_size, 0);
    pEncOut->PreDataSize += nal_size;
    pEncOut->PreNaluNum++;
    hash(&vcenc_instance->hashctx, vcenc_instance->stream.stream, nal_size);
  }
}

/* Some params for different quality config and different HW version */
void VideoTuneConfig(const VCEncConfig *config,
                     struct vcenc_instance *vcenc_inst) {
  if (!config || !vcenc_inst) return;
  regValues_s *regs = &(vcenc_inst->asic.regs);
  vcencRateControl_s *rc = &(vcenc_inst->rateControl);

  vcenc_inst->cuTreeCtl.aqStrength = 1.0;
  vcenc_inst->cuTreeCtl.aq_mode = 0;
  vcenc_inst->psyFactor = 0.0;
  rc->IFrameQpVisualTuning = rc->pass1EstCostAll = rc->visualLmdTuning =
      rc->initialQpTuning = 0;
  if (regs->asicCfg->tuneToolsSet2Support) {
#ifdef RDO_CHECK_ZERO_TU
    /* Temporarily disable bRdoCheckChromaZeroTu
       when using YUV444 HW encode YUV420 */
    if(vcenc_inst->asic.regs.asicCfg->HevcYUV444Support &&
       config->codedChromaIdc == VCENC_CHROMA_IDC_420)
      regs->bRdoCheckChromaZeroTu = 0;
    else
      regs->bRdoCheckChromaZeroTu = 1;
#endif
#ifdef REFINE_MEXN_BINCOST
    regs->lambdaCostScaleMExN[0] = 2;
    regs->lambdaCostScaleMExN[1] = 1;
    regs->lambdaCostScaleMExN[2] = 0;
    if (regs->asicCfg->modSubjPrefer && IS_H264(vcenc_inst->codecFormat))
      regs->lambdaCostScaleMExN[1] = 0;
#endif
  }

  switch (config->tune) {
    case VCENC_TUNE_PSNR:
#ifdef HEVC_QUALITY_LA_TUNE
      if (regs->asicCfg->tuneToolsSet2Support) {
        /* currently these tunings are only applied to 2-pass encoding including 1st and 2nd pass. */
        if (config->pass) {
          rc->pass1EstCostAll = rc->visualLmdTuning = 1;
          rc->initialQpTuning = FIRST_I_TUNE;

          if (config->pass == 1) {
            regs->RefFrameUsingInputFrameEnable = 1;
            regs->InLoopDSBilinearEnable = 1;
            regs->H264Intramode4x4Disable = 1;
            regs->H264Intramode8x8Disable = 1;
            regs->HevcSimpleRdoAssign = 2;
            regs->PredModeBySatdEnable = 1;
          }
        }
      }
#endif
      break;
    case VCENC_TUNE_SSIM:
      vcenc_inst->cuTreeCtl.aq_mode = 2;
      break;
    case VCENC_TUNE_VISUAL:
    case VCENC_TUNE_SHARP_VISUAL:
      vcenc_inst->cuTreeCtl.aq_mode = 2;
      rc->IFrameQpVisualTuning = rc->pass1EstCostAll = rc->visualLmdTuning = 1;

      /* PSY factor */
      if (regs->asicCfg->encPsyTuneSupport)
        vcenc_inst->psyFactor = 0.75;

      /* encoder mode decision tools tuning */
      if (regs->asicCfg->encVisualTuneSupport ||
          regs->asicCfg->tuneToolsSet2Support) {
#ifdef H264_LA_INTRA_MODE_NON_4x4
        regs->H264Intramode4x4Disable = (config->pass == 1);
#endif
#ifdef H264_LA_INTRA_MODE_NON_8x8
        regs->H264Intramode8x8Disable = (config->pass == 1);
#endif
#ifdef LA_REFERENCE_USE_INPUT
        regs->RefFrameUsingInputFrameEnable = (config->pass == 1);
#endif
      }

      if (regs->asicCfg->tuneToolsSet2Support) {
#ifdef RDOQ_LAMBDA_ADJ
        vcenc_inst->bRdoqLambdaAdjust = 1;
#endif
#ifdef BILINEAR_DOWNSAMPLE
        regs->InLoopDSBilinearEnable = (config->pass == 1);
#endif
#ifdef LA_INTRA_BY_SATD
        regs->PredModeBySatdEnable = (config->pass == 1);
#endif
#ifdef LA_HEVC_SIMPLE_RDO
        if ((config->pass == 1) && !IS_H264(config->codecFormat))
          regs->HevcSimpleRdoAssign = 2;
#endif
      }
      break;
    default:
      break;
  }

  regs->psyFactor =
      (u32)(vcenc_inst->psyFactor * (1 << PSY_FACTOR_SCALE_BITS) + 0.5);
}

/*------------------------------------------------------------------------------
    Function name   : SetPictureCfgToJob
    Description     : set picture config to job' encIn
    Return type     : void
    Argument        : const VCEncIn* pEncIn              [in]
    Argument        : VCEncIn* pJobEncIn                 [out]
------------------------------------------------------------------------------*/
void SetPictureCfgToJob(const VCEncIn *pEncIn, VCEncIn *pJobEncIn,
                        u8 gdrDuration) {
  if (NULL == pEncIn || NULL == pJobEncIn) return;

  if (HANTRO_TRUE == pJobEncIn->bIsIDR && !gdrDuration) {
    pJobEncIn->codingType = VCENC_INTRA_FRAME;
    pJobEncIn->poc = 0;
  } else {
    pJobEncIn->codingType = pEncIn->codingType;
    pJobEncIn->poc = pEncIn->poc;
    pJobEncIn->bIsIDR = pEncIn->bIsIDR;
    pJobEncIn->bIsIntraOnly = pEncIn->bIsIntraOnly;
  }
  memcpy(&pJobEncIn->gopConfig, &pEncIn->gopConfig,
         sizeof(pJobEncIn->gopConfig));
  pJobEncIn->gopSize = pEncIn->gopSize;
  pJobEncIn->gopPicIdx = pEncIn->gopPicIdx;
  pJobEncIn->picture_cnt = pEncIn->picture_cnt;
  pJobEncIn->picture_gopIdx = pEncIn->picture_gopIdx;
  pJobEncIn->last_idr_picture_cnt = pEncIn->last_idr_picture_cnt;

  pJobEncIn->bIsPeriodUsingLTR = pEncIn->bIsPeriodUsingLTR;
  pJobEncIn->bIsPeriodUpdateLTR = pEncIn->bIsPeriodUpdateLTR;
  memcpy(&pJobEncIn->gopCurrPicConfig, &pEncIn->gopCurrPicConfig,
         sizeof(pJobEncIn->gopCurrPicConfig));
  memcpy(pJobEncIn->long_term_ref_pic, pEncIn->long_term_ref_pic,
         sizeof(pJobEncIn->long_term_ref_pic));
  memcpy(pJobEncIn->bLTR_used_by_cur, pEncIn->bLTR_used_by_cur,
         sizeof(pJobEncIn->bLTR_used_by_cur));
  memcpy(pJobEncIn->bLTR_need_update, pEncIn->bLTR_need_update,
         sizeof(pJobEncIn->bLTR_need_update));

  pJobEncIn->i8SpecialRpsIdx = pEncIn->i8SpecialRpsIdx;
  pJobEncIn->i8SpecialRpsIdx_next = pEncIn->i8SpecialRpsIdx_next;
  pJobEncIn->u8IdxEncodedAsLTR = pEncIn->u8IdxEncodedAsLTR;
}

#ifndef RATE_CONTROL_BUILD_SUPPORT
/* set qpHdr and fixedQp. */
bool_e VCEncInitQp(vcencRateControl_s *rc, u32 newStream) {
  if (rc->qpHdr == (-1 << QP_FRACTIONAL_BITS))
    rc->qpHdr = QP_DEFAULT << QP_FRACTIONAL_BITS;
  rc->fixedQp = rc->qpHdr;
  return 0;
}

/* calculate qpHdr for current picture. */
void VCEncBeforeCQP(vcencRateControl_s *rc, u32 timeInc, u32 sliceType,
                    bool use_ltr_cur, struct sw_picture *pic) {
  rc->sliceTypeCur = sliceType;

  if (rc->pass == 1) {
    // pass 1 uses constant qp10 to encode
    rc->qpHdr = CU_TREE_QP << QP_FRACTIONAL_BITS;
    return;
  }

  if (rc->rcMode == VCE_RC_CQP) {
    /* calculate qp */
    rc->qpHdr = rc->fixedQp;
    if (rc->sliceTypeCur == I_SLICE) {
      if (rc->fixedIntraQp)
        rc->qpHdr = rc->fixedIntraQp;
      else if (rc->sliceTypePrev != I_SLICE) {
        rc->qpHdr += rc->intraQpDelta;
      }
    } else {
      //B frm not referenced
      if (rc->picRc != ENCHW_YES) rc->qpHdr += rc->frameQpDelta;
      if (use_ltr_cur) rc->qpHdr += rc->longTermQpDelta;
      if (rc->qpHdr > (51 << QP_FRACTIONAL_BITS))
        rc->qpHdr = (51 << QP_FRACTIONAL_BITS);
    }

    /* quantization parameter user defined limitations */
    rc->qpHdr = MIN(rc->qpMax, MAX(rc->qpMin, rc->qpHdr));
    rc->sliceTypePrev = rc->sliceTypeCur;
  }
}
#endif

/*init coding ctrl parameter queue*/
void EncInitCodingCtrlQueue(VCEncInst inst) {
  struct vcenc_instance *pEncInst = (struct vcenc_instance *)inst;
  if (NULL == pEncInst->codingCtrl.codingCtrlQueue.head) {
    //get buffer
    EncCodingCtrlParam *pEncCodingCtrlParam =
        (EncCodingCtrlParam *)DynamicGetBufferFromPool(
            &pEncInst->codingCtrl.codingCtrlBufPool,
            sizeof(EncCodingCtrlParam));
    if (!pEncCodingCtrlParam) {
      APITRACEWRN(
          "EncInitCodingCtrlQueue: ERROR Get coding ctrl buffer failed\n");
      return;
    }
    VCEncCodingCtrl *pCodeParams = &pEncCodingCtrlParam->encCodingCtrl;

    //get coding ctrl parameters.
    VCEncGetCodingCtrl(inst, pCodeParams);

    /*save default parameters in codingctrl queue*/
    /*The newest parameter's refCnt need be realRefCnt+1,
      so that next frame can also use these parameters if no new parameters are set*/
    pEncCodingCtrlParam->refCnt = 1;
    pEncCodingCtrlParam->startPicCnt = -1;
    queue_put(&pEncInst->codingCtrl.codingCtrlQueue,
              (struct node *)pEncCodingCtrlParam);
  }

  return;
}

/*init rate ctrl parameter queue*/
void EncInitRateCtrlQueue(VCEncInst inst) {
  struct vcenc_instance *pEncInst = (struct vcenc_instance *)inst;
  if (NULL == pEncInst->rateCtrl.rateCtrlQueue.head) {
    //get buffer
    EncRateCtrlParam *pEncRateCtrlParam =
        (EncRateCtrlParam *)DynamicGetBufferFromPool(
            &pEncInst->rateCtrl.rateCtrlBufPool,
            sizeof(EncRateCtrlParam));
    if (!pEncRateCtrlParam) {
      APITRACEWRN(
          "EncInitRateCtrlQueue: ERROR Get rate ctrl buffer failed\n");
      return;
    }
    VCEncRateCtrl *pRateParams = &pEncRateCtrlParam->encRateCtrl;

    //get rate ctrl parameters.
    VCEncGetRateCtrl(inst, pRateParams);

    /*save default parameters in ratectrl queue*/
    /*The newest parameter's refCnt need be realRefCnt+1,
      so that next frame can also use these parameters if no new parameters are set*/
    pEncRateCtrlParam->refCnt = 1;
    pEncRateCtrlParam->startPicCnt = -1;
    queue_put(&pEncInst->rateCtrl.rateCtrlQueue,
              (struct node *)pEncRateCtrlParam);
  }

  return;
}

/*------------------------------------------------------------------------------
  VCEnc get frame Info, gopPicIdx, picNum.
------------------------------------------------------------------------------*/
void VCEncGetFrameInfo(struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn, asicData_s *asic, VCEncOut *pEncOut) {
  if (!asic->regs.enableFrameInfo) return;

  pEncOut->frameInfo = (VCEncOutFrameInfo*)asic->framInfoMem.virtualAddress;

  u32 sumOfQP = asic->regs.sumOfQP;//8*8 blk qp Sum
  u32 sumOfQPNumber = asic->regs.sumOfQPNumber;//8*8 qp blk cnt
  u32 meanQp = sumOfQP / sumOfQPNumber;
  if (IS_HEVC(vcenc_instance->codecFormat)) {
    pEncOut->frameInfo->hevcInfo.gopPicIdx = pEncIn->picture_gopIdx;
    pEncOut->frameInfo->hevcInfo.picNum = pEncIn->picture_cnt;
    pEncOut->frameInfo->hevcInfo.meanQp = meanQp;
  } else if (IS_H264(vcenc_instance->codecFormat)) {
    pEncOut->frameInfo->h264Info.gopPicIdx = pEncIn->picture_gopIdx;
    pEncOut->frameInfo->h264Info.picNum = pEncIn->picture_cnt;
    pEncOut->frameInfo->h264Info.meanQp = meanQp;
  }
}

/* Config AxiFe */
void VCEncCfgAxiFe(struct vcenc_instance *inst, asicData_s *asic, u32 cutree, u32 tileId) {
#ifdef SUPPORT_AXIFE
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  struct VCAxiFeData *af_cfg;
  struct ChnDesc *rd_ch;
  struct ChnDesc *wr_ch;

  af_cfg = asic->axife_data;
  af_cfg->mode = vcenc_instance->axiFEEnable;
  af_cfg->ewl = (const void *)asic->ewl;
  af_cfg->vcmd = NULL;

  if (af_cfg->mode == 0) {
    return;
  }

  //cutree not support security mode
  if (cutree && (af_cfg->mode == 3)) {
      af_cfg->mode = 0;
      return;
  }

  if (asic->regs.bVCMDEnable)
    af_cfg->vcmd = &asic->regs.vcmd;

  if (af_cfg->mode == 3) {
    memset(&af_cfg->channelCfg, 0, sizeof(struct AxiFeChns));
    memset(&af_cfg->commonCfg, 0, sizeof(struct AxiFeCommonCfg));

    //example to config AXIFE
    af_cfg->channelCfg.nbr_rd_chns = 1;
    af_cfg->channelCfg.nbr_wr_chns = 1;
    rd_ch = &af_cfg->channelCfg.rd_chns[0];
    wr_ch = &af_cfg->channelCfg.wr_chns[0];

    rd_ch->sw_axi_base_addr_id = asic->regs.inputLumBase >> 32;
    rd_ch->sw_axi_start_addr = asic->regs.inputLumBase & 0xFFFFFFFF;
    rd_ch->sw_axi_end_addr =
        (asic->regs.inputLumBase +
         asic->regs.input_luma_stride *
             vcenc_instance->preProcess.lumHeightSrc[tileId]) &
        0xFFFFFFFF;
    rd_ch->sw_axi_user = 0;
    rd_ch->sw_axi_ns = 0;

    wr_ch->sw_axi_base_addr_id = asic->regs.reconLumBase >> 32;
    wr_ch->sw_axi_start_addr = asic->regs.reconLumBase & 0xFFFFFFFF;
    wr_ch->sw_axi_end_addr = asic->regs.reconLumBase +
                                 asic->internalreconLuma[0].size;
    wr_ch->sw_axi_user = 0;
    wr_ch->sw_axi_ns = 0;

    af_cfg->commonCfg.sw_secure_mode = 1;
    af_cfg->commonCfg.sw_axi_user_mode = 3;
    af_cfg->commonCfg.sw_axi_prot_mode = 1;
  }
#endif
}

/* Set ApbFilter */
void VCEncSetApbFilter(void *inst) {
#ifdef SUPPORT_APBFT
  ApbFilterREG *regs;
  struct ApbFilterCfg cfg;
  struct ApbFilterMask *mask;
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  asicData_s *asic = &vcenc_instance->asic;
  u32 i;
  u32 protect_reg_size = 400;  //for VCE, only 400 regs will be protected

  //example to config ApbFilter
  regs = EWLcalloc(protect_reg_size, sizeof(ApbFilterREG));
  mask = EWLcalloc(protect_reg_size, sizeof(struct ApbFilterMask));

  for (i = 0; i < protect_reg_size; i++) {
    mask[i].page_idx = 0;
    mask[i].reg_idx = i;
    mask[i].value = 0;
  }
  cfg.reg_mask = mask;
  cfg.num_regs = protect_reg_size;

  ConfigApbFilter(regs, &cfg);

  //write registers to AXIFE
  for (i = 0; i < protect_reg_size; i++) {
    if (regs[i].flag)
      EWLWriteRegbyClientType(asic->ewl, regs[i].offset, regs[i].value,
                              EWL_CLIENT_TYPE_APBFT);
  }

  //set page select
  SelApbFilterPage(&regs[0], 0, protect_reg_size * 4);
  EWLWriteRegbyClientType(asic->ewl, regs[0].offset, regs[0].value,
                          EWL_CLIENT_TYPE_APBFT);

  EWLfree(regs);
  EWLfree(mask);
#endif
}

/* Set waitJobCfg */
void VCEncSetwaitJobCfg(VCEncIn *pEncIn, struct vcenc_instance *vcenc_instance,
                        asicData_s *asic, u32 waitCoreJobid) {
  EWLWaitJobCfg_t waitJobCfg;
  CacheData_t cache_data;
  memset(&waitJobCfg, 0, sizeof(EWLWaitJobCfg_t));
  waitJobCfg.waitCoreJobid = waitCoreJobid;
#ifdef SUPPORT_DEC400
  waitJobCfg.dec400_enable = asic->dec400_data->dec400Enable;
  waitJobCfg.dec400_callback = VCEncDisableDec400;
#endif
#ifdef SUPPORT_AXIFE
  waitJobCfg.axife_enable = pEncIn->axiFEEnable;
  waitJobCfg.axife_callback = VCEncAxiFeDisable;
#endif
#ifdef SUPPORT_CACHE
  waitJobCfg.l2cache_enable = 1;
#endif
  cache_data.cache = &vcenc_instance->cache;
  waitJobCfg.l2cache_data = &cache_data;
  waitJobCfg.l2cache_callback = DisableCache;
#if defined SUPPORT_UFBC
  waitJobCfg.ufbcMode = vcenc_instance->ufbcParam.mode;
  waitJobCfg.ufbc_callback = EncUfbcAsicStop;
#endif

  EWLEnqueueWaitjob(asic->ewl, &waitJobCfg);
}

/* enable DEC400 to get input data */  //TODO, rename and optimize, cfgDec400
void VCEncCfgDec400(VCEncIn *pEncIn,
                    struct vcenc_instance *vcenc_instance, asicData_s *asic,
                    u32 tileId) {
  u32 is_dec400_in_work_mode = 0;
  is_dec400_in_work_mode = ((pEncIn->dec400Enable & 0xF) == 2) ? 1 : 0;
  memset(asic->dec400_data, 0, sizeof(VCDec400data));
  asic->dec400_data->streams[0].pix_fmt = vcenc_instance->preProcess.inputFormat;
  asic->dec400_data->streams[0].width = vcenc_instance->preProcess.lumWidthSrc[tileId];
  asic->dec400_data->streams[0].height = vcenc_instance->preProcess.lumHeightSrc[tileId];
  asic->dec400_data->input_alignment = vcenc_instance->preProcess.input_alignment;
  asic->dec400_data->streams[0].table_base[0] = pEncIn->dec400LumTableBase;
  asic->dec400_data->streams[0].table_base[1] = pEncIn->dec400CbTableBase;
  asic->dec400_data->streams[0].table_base[2] = pEncIn->dec400CrTableBase;
  asic->dec400_data->streams[0].dec400Enable = pEncIn->dec400Enable;
  asic->dec400_data->dec400Enable = pEncIn->dec400Enable;
  asic->dec400_data->ewl_inst = asic->ewl;
  asic->dec400_data->vcmd = &asic->regs.vcmd;
  asic->dec400_data->streams[0].data_base[2] = pEncIn->busChromaV;
  asic->dec400_data->streams[0].data_base[1] = pEncIn->busChromaU;
  asic->dec400_data->streams[0].data_base[0] = pEncIn->busLuma;
  asic->dec400_data->streams[0].super_tile = vcenc_instance->preProcess.scanType;
  asic->dec400_data->streams[0].fastclearEnable[0] = pEncIn->fcEnable[0];
  asic->dec400_data->streams[0].fastclearEnable[1] = pEncIn->fcEnable[1];
  asic->dec400_data->streams[0].fastclearEnable[2] = pEncIn->fcEnable[2];
  asic->dec400_data->streams[0].fastclear_val[0] = pEncIn->clearColorLow[0];
  asic->dec400_data->streams[0].fastclear_val[1] = pEncIn->clearColorLow[1];
  asic->dec400_data->streams[0].fastclear_val[2] = pEncIn->clearColorLow[2];
  if (pEncIn->dec400TSHeaderEnable) {
    asic->dec400_data->streams[0].table_base[0] =
      (u32)asic->dec400_data->streams[0].table_base[0] + DEC400_HEADER_BUF_SIZE;
    asic->dec400_data->streams[0].table_base[1] =
      (u32)asic->dec400_data->streams[0].table_base[1] + DEC400_HEADER_BUF_SIZE;
    asic->dec400_data->streams[0].table_base[2] =
      (u32)asic->dec400_data->streams[0].table_base[2] + DEC400_HEADER_BUF_SIZE;
  }

  /* enable DEC400 to get decompressed input data */
  for (u32 i = 0; i < MAX_OVERLAY_NUM; i++) {
    asic->dec400_data->streams[i + 1].pix_fmt= vcenc_instance->preProcess.overlayFormat[i];
    asic->dec400_data->streams[i + 1].width = vcenc_instance->preProcess.overlayWidth[i];
    asic->dec400_data->streams[i + 1].height = vcenc_instance->preProcess.overlayHeight[i];
    asic->dec400_data->streams[i + 1].table_base[0] = pEncIn->osdDec400TableBase[i][0];
    asic->dec400_data->streams[i + 1].table_base[1] = pEncIn->osdDec400TableBase[i][1];
    asic->dec400_data->streams[i + 1].table_base[2] = pEncIn->osdDec400TableBase[i][2];
    asic->dec400_data->streams[i + 1].dec400Enable = pEncIn->osdDec400Enable[i];
    asic->dec400_data->streams[i + 1].is_osd = 1;
    asic->dec400_data->streams[i + 1].data_base[2] = asic->regs.overlayVAddr[i];
    asic->dec400_data->streams[i + 1].data_base[1] = asic->regs.overlayUAddr[i];
    asic->dec400_data->streams[i + 1].data_base[0] = asic->regs.overlayYAddr[i];
    asic->dec400_data->streams[i + 1].super_tile =
        vcenc_instance->preProcess.overlaySuperTile[i];

    if (!is_dec400_in_work_mode && (pEncIn->osdDec400Enable[i] & 0xF) == 2)
      is_dec400_in_work_mode = 1;
  }

  if (is_dec400_in_work_mode)
    asic->dec400_data->dec400Enable = asic->dec400_data->dec400Enable | 0x2;

}

/* enable UFBC to get input data */  //TODO, rename and optimize, cfgUfbc
i32 VCEncCfgUfbc(VCEncIn *pEncIn,
                    struct vcenc_instance *vcenc_instance, asicData_s *asic,
                    EncUfbcParam *ufbc_param) {
#ifdef SUPPORT_UFBC
  ufbc_param->format = vcenc_instance->preProcess.inputFormat;
  ufbc_param->width = vcenc_instance->preProcess.lumWidthSrc[0];
  ufbc_param->height = vcenc_instance->preProcess.lumHeightSrc[0];
  ufbc_param->xOffset = vcenc_instance->preProcess.horOffsetSrc[0];
  ufbc_param->yOffset = vcenc_instance->preProcess.verOffsetSrc[0];
  ufbc_param->alignment = vcenc_instance->preProcess.input_alignment;
  ufbc_param->mode = vcenc_instance->ufbcParam.mode;
  ufbc_param->vcmd = &asic->regs.vcmd;
  ufbc_param->baseAddress[0] = pEncIn->busLuma;
  ufbc_param->baseAddress[1] = pEncIn->busChromaU;
  ufbc_param->baseAddress[2] = pEncIn->busChromaV;
  if (vcenc_instance->ufbcParam.mode == UFBC_MODE_DEC400) {
    ufbc_param->param.dec400.headerAddress[0] = pEncIn->dec400LumTableBase;
    ufbc_param->param.dec400.headerAddress[1] = pEncIn->dec400CbTableBase;
    ufbc_param->param.dec400.tileSize = UFBC_TILE_SIZE;
  } else if (vcenc_instance->ufbcParam.mode == UFBC_MODE_AFBC_0 || vcenc_instance->ufbcParam.mode == UFBC_MODE_AFBC_1) {
    ufbc_param->param.afbc.yuvTrans = vcenc_instance->ufbcParam.param.afbc.yuvTrans;
    ufbc_param->param.afbc.blockType = vcenc_instance->ufbcParam.param.afbc.blockType;
    ufbc_param->param.afbc.blockSplit = vcenc_instance->ufbcParam.param.afbc.blockSplit;
  } else if (vcenc_instance->ufbcParam.mode == UFBC_MODE_PVRIC) {
    ufbc_param->param.pvric.blockType = vcenc_instance->ufbcParam.param.pvric.blockType;
    for (u32 i = 0; i<4; i++) {
      ufbc_param->param.pvric.consColorVal[i] = vcenc_instance->ufbcParam.param.pvric.consColorVal[i];
    }
  }
  if (EncUfbcSetParams(asic->ufbc, ufbc_param)) {
    return 1;
  }
#endif
  return 0;
}

/* Enable Subsystem L2Cache/dec400/AXIFE/APBFilter */
VCEncRet VCEncSetSubsystem(struct vcenc_instance *vcenc_instance,
                           VCEncIn *pEncIn, asicData_s *asic,
                           struct sw_picture *pic, u32 tileId) {
  //L2CACHE
  if (EnableCache(vcenc_instance, asic, pic, tileId) != VCENC_OK) {
    APITRACEERR("Encoder enable cache failed!!\n");
    return VCENC_ERROR;
  }

  //dec400
  if (asic->ufbc->mode != UFBC_MODE_NONE) {
    if (VCEncSetDec400(asic) != VCENC_OK) {
      EWLReleaseHw(asic->ewl);
      return VCENC_INVALID_ARGUMENT;
    }
  }

  //AXIFE
#ifdef SUPPORT_AXIFE
  if (asic->axife_data->mode) {
    VCEncAxiFeEnable(asic->axife_data);
  }
#endif

  //APBfilter
  if (pEncIn->apbFTEnable) {
    VCEncSetApbFilter((void *)vcenc_instance);
  }

  //ufbc
  if(asic->ufbc->has_ufbc) {
    EncUfbcAsicStart(asic->ufbc);
  }
  return VCENC_OK;
}

/* Reset callback struct */
void VCEncResetCallback(VCEncSliceReady *slice_callback,
                        struct vcenc_instance *vcenc_instance, VCEncIn *pEncIn,
                        VCEncOut *pEncOut, u32 next_core_index,
                        u32 multicore_flag) {
  slice_callback->slicesReadyPrev = 0;
  slice_callback->slicesReady = 0;
  slice_callback->sliceSizes =
      (u32 *)vcenc_instance->asic.sizeTbl[next_core_index].virtualAddress;
  //slice_callback.sliceSizes += vcenc_instance->numNalus;
  slice_callback->nalUnitInfoNum = vcenc_instance->numNalus[next_core_index];
  slice_callback->nalUnitInfoNumPrev = 0;
  slice_callback->streamBufs = vcenc_instance->streamBufs[next_core_index];
  slice_callback->pAppData = vcenc_instance->pAppData;
  for (i32 i = 0; i < MAX_STRM_BUF_NUM; i++) {
    slice_callback->outbufMem[i] = (EWLLinearMem_t *)pEncIn->cur_out_buffer[i];
  }
  if (!multicore_flag)  //MultiCorFlush do not set parameters
  {
    slice_callback->PreNaluNum = pEncOut->PreNaluNum;
    slice_callback->PreDataSize = pEncOut->PreDataSize;
  }
}

/*set RingBuffer registers */
void VCEncSetRingBuffer(struct vcenc_instance *vcenc_instance, asicData_s *asic, struct sw_picture *pic)
{
  if (vcenc_instance->refRingBufEnable)
  {
    asic->regs.refRingBufEnable=1;
    asic->regs.pRefRingBuf_base=asic->RefRingBuf.busAddress;

    //each block buffer size
    u32 lumaBufSize = asic->regs.refRingBuf_luma_size;
    u32 chromaBufSize = asic->regs.refRingBuf_chroma_size;
    u32 lumaBufSize4N = asic->regs.refRingBuf_4n_size;
    u32 chromaHalfSize = asic->regs.recon_chroma_half_size;

    //each block base address offset from RingBuffer base address.
    u32 lumaBaseAddress = asic->RefRingBuf.busAddress;
    u32 chromaBaseAddress = asic->RefRingBuf.busAddress + lumaBufSize;
    u32 luma4NBaseAddress = asic->RefRingBuf.busAddress + lumaBufSize + chromaBufSize;

    if (pic->sliceInst->type == I_SLICE)//I slice  CHECK
    {
      pic->recon.lum = asic->RefRingBuf.busAddress;
      pic->recon.cb = asic->RefRingBuf.busAddress + lumaBufSize;
      pic->recon.cr = pic->recon.cb + chromaHalfSize;
      pic->recon_4n_base = pic->recon.cb + chromaBufSize;
    }
    else
    {
      //reference data reading offset from each block base address.
      u32 lumaReadOffset=pic->rpl[0][0]->recon.lum - lumaBaseAddress;
      u32 cbReadOffset=pic->rpl[0][0]->recon.cb - chromaBaseAddress;
      u32 crReadOffset=pic->rpl[0][0]->recon.cr - chromaBaseAddress;
      u32 luma4NReadOffset=pic->rpl[0][0]->recon_4n_base - luma4NBaseAddress;

      asic->regs.refRingBuf_luma_rd_offset = lumaReadOffset;
      asic->regs.refRingBuf_chroma_rd_offset = cbReadOffset;
      asic->regs.refRingBuf_4n_rd_offset= luma4NReadOffset;

      u32 width,width_4n,height;
      width = ((vcenc_instance->width + 63) >> 6) << 6;
      width_4n = ((vcenc_instance->width + 15) / 16) * 4;

      //actual luma/chroma/4N size
      u32 lumaSize = lumaBufSize - vcenc_instance->RefRingBufExtendHeight * asic->regs.ref_frame_stride /4;
      u32 chromaSize = chromaBufSize - vcenc_instance->RefRingBufExtendHeight/2 * asic->regs.ref_frame_stride_ch /4;
      u32 lumaSize4N = lumaBufSize4N - vcenc_instance->RefRingBufExtendHeight/4 * asic->regs.ref_ds_luma_stride /4;

      //update recon address
      if (lumaReadOffset + lumaSize < lumaBufSize)
        pic->recon.lum = pic->rpl[0][0]->recon.lum + lumaSize;
      else
        pic->recon.lum = pic->rpl[0][0]->recon.lum + lumaSize - lumaBufSize;

      if (cbReadOffset + chromaSize < chromaBufSize)
        pic->recon.cb = pic->rpl[0][0]->recon.cb + chromaSize;
      else
        pic->recon.cb = pic->rpl[0][0]->recon.cb + chromaSize - chromaBufSize;

      if (crReadOffset + chromaSize < chromaBufSize)
        pic->recon.cr = pic->rpl[0][0]->recon.cr + chromaSize;
      else
        pic->recon.cr = pic->rpl[0][0]->recon.cr + chromaSize - chromaBufSize;

      if (luma4NReadOffset + lumaSize4N < lumaBufSize4N)
        pic->recon_4n_base = pic->rpl[0][0]->recon_4n_base + lumaSize4N;
      else
        pic->recon_4n_base = pic->rpl[0][0]->recon_4n_base + lumaSize4N - lumaBufSize4N;
    }
    //recon data writing offset from each block base address.
    asic->regs.refRingBuf_luma_wr_offset = pic->recon.lum - lumaBaseAddress;
    asic->regs.refRingBuf_chroma_wr_offset = pic->recon.cb - chromaBaseAddress;
    asic->regs.refRingBuf_4n_wr_offset= pic->recon_4n_base - luma4NBaseAddress;
  }
}

/* Get ChromaIdc */
u32 VCEncGetChromaIdc(u32 InputChromaIdc, struct vcenc_instance *vcenc_inst) {
  if ((InputChromaIdc == VCENC_CHROMA_IDC_400 &&
       vcenc_inst->asic.regs.asicCfg->MonoChromeSupport == EWL_HW_CONFIG_NOT_SUPPORTED) ||
      (InputChromaIdc == VCENC_CHROMA_IDC_422 && IS_HEVC(vcenc_inst->codecFormat) &&
       vcenc_inst->asic.regs.asicCfg->HevcYUV422Support == EWL_HW_CONFIG_NOT_SUPPORTED) ||
      (InputChromaIdc == VCENC_CHROMA_IDC_422 && IS_H264(vcenc_inst->codecFormat) &&
       vcenc_inst->asic.regs.asicCfg->H264YUV422Support == EWL_HW_CONFIG_NOT_SUPPORTED) ||
      (InputChromaIdc == VCENC_CHROMA_IDC_444 && IS_HEVC(vcenc_inst->codecFormat) &&
       vcenc_inst->asic.regs.asicCfg->HevcYUV444Support == EWL_HW_CONFIG_NOT_SUPPORTED) ||
      (InputChromaIdc == VCENC_CHROMA_IDC_444 && IS_H264(vcenc_inst->codecFormat) &&
       vcenc_inst->asic.regs.asicCfg->H264YUV444Support == EWL_HW_CONFIG_NOT_SUPPORTED)) {
    APITRACEERR(
       "WARNING: ChromaIdc force to be 'VCENC_CHROMA_IDC_420' for current ChromaIdc not supported by HW\n");
    return VCENC_CHROMA_IDC_420;
  }
   else return InputChromaIdc;
}

void VCEncSetCropOffset(struct vcenc_instance *vcenc_instance, preProcess_s *pp_tmp)
{
  pp_tmp->frameCropping = ENCHW_NO;
  pp_tmp->frameCropLeftOffset = 0;
  pp_tmp->frameCropRightOffset = 0;
  pp_tmp->frameCropTopOffset = 0;
  pp_tmp->frameCropBottomOffset = 0;
  /* Set cropping parameters if required */
  u32 alignment = (IS_H264(vcenc_instance->codecFormat) ? 16 : 8);
  if (vcenc_instance->preProcess.lumWidth % alignment || vcenc_instance->preProcess.lumHeight % alignment)
  {
    u32 fillRight = (vcenc_instance->preProcess.lumWidth + alignment-1) / alignment * alignment -
                    vcenc_instance->preProcess.lumWidth;
    u32 fillBottom = (vcenc_instance->preProcess.lumHeight + alignment-1) / alignment * alignment -
                     vcenc_instance->preProcess.lumHeight;
    VCEncPictureRotation rotation = (VCEncPictureRotation) pp_tmp->rotation;

    pp_tmp->frameCropping = ENCHW_YES;

    if (rotation == VCENC_ROTATE_0)     /* No rotation */
    {
      if (pp_tmp->mirror)
        pp_tmp->frameCropLeftOffset = fillRight >> pp_tmp->subsample_x;
      else
        pp_tmp->frameCropRightOffset = fillRight >> pp_tmp->subsample_x;
      pp_tmp->frameCropBottomOffset = fillBottom >> pp_tmp->subsample_y;
    }
    else if (rotation == VCENC_ROTATE_90R)        /* Rotate right */
    {
      pp_tmp->frameCropLeftOffset = fillRight >> pp_tmp->subsample_x;
      if (pp_tmp->mirror)
        pp_tmp->frameCropTopOffset = fillBottom >> pp_tmp->subsample_y;
      else
        pp_tmp->frameCropBottomOffset = fillBottom >> pp_tmp->subsample_y;
    }
    else if (rotation == VCENC_ROTATE_90L)        /* Rotate left */
    {
      pp_tmp->frameCropRightOffset = fillRight >> pp_tmp->subsample_x;
      if (pp_tmp->mirror)
        pp_tmp->frameCropBottomOffset = fillBottom >> pp_tmp->subsample_y;
      else
        pp_tmp->frameCropTopOffset = fillBottom >> pp_tmp->subsample_y;
    }
    else if (rotation == VCENC_ROTATE_180R)        /* Rotate 180 degree left */
    {
      if (pp_tmp->mirror)
        pp_tmp->frameCropRightOffset = fillRight >> pp_tmp->subsample_x;
      else
        pp_tmp->frameCropLeftOffset = fillRight >> pp_tmp->subsample_x;
      pp_tmp->frameCropTopOffset = fillBottom >> pp_tmp->subsample_y;
    }
  }
}

/*tune qpfactor and deltaQPRange for excess bitrate case*/
void VCEncExcessBitrateConfigTuning(struct vcenc_instance *vcenc_instance, double * qpfactor, VCEncPictureCodingType codingType)
{
  double dQPFactor = *qpfactor;
  double defaultQpFactor = dQPFactor;

  vcencRateControl_s *rc = &vcenc_instance->rateControl;
  asicData_s *asic = &vcenc_instance->asic;

  /*if picRc is not enable, qpfactor and deltaQPRange will not be tuned.*/
  if (vcenc_instance->pass == 1 ||  rc->picRc != ENCHW_YES)
  {
    return;
  }

  /*only if qp is set as maxqp, bitrate info will be used for qpfactor and deltaQPRange tuning*/
  //u8 bMaxQP = (codingType == VCENC_INTRA_FRAME ? (rc->fixedIntraQp ? 0 : (rc->qpHdr >= rc->qpMax+rc->intraQpDelta)) : rc->qpHdr >= rc->qpMax);
  u8 bMaxQP = (codingType != VCENC_INTRA_FRAME) && (rc->qpHdr >= rc->qpMax);
  double bitRateRatio = (bMaxQP ?
    (double)vcenc_instance->currentActualBitRate/vcenc_instance->rateControl.virtualBuffer.bitRate : 1.0);
  double rcQpfactor = bitRateRatio;

#ifdef RCP_FIXED_POINT_OPT
  rcQpfactor = MAX(bitRateRatio, (double)(vcenc_instance->rateControl.qpFactor)/LL_FIXED);
#else
  rcQpfactor = MAX(bitRateRatio, vcenc_instance->rateControl.qpFactor);
#endif

  if (rcQpfactor > 1.0)
    dQPFactor *= rcQpfactor;

  double bpLimit = 1.0 + (double)vcenc_instance->visualBitRateTolerance / 100;
  u8 bExceedBitTolerance = vcenc_instance->currentActualBitRate > vcenc_instance->rateControl.virtualBuffer.bitRate * bpLimit;
  /*excess bitrate tolerance*/
  if (bExceedBitTolerance && bMaxQP)
  {
    vcenc_instance->rateControl.qpMin = vcenc_instance->rateControl.qpHdr;
    dQPFactor = MIN(dQPFactor, 1.4);
    *qpfactor = dQPFactor;
    vcenc_instance->qpfactorSSE = dQPFactor * dQPFactor;
  }
  else
  {
    /*delta qp range tuning*/
    if (asic->regs.asicCfg->ctbRcVersion == 2)
    {
      /* for ctbrcVersion2, set ctbrcThreshold according to bitRateRatio*/
      if (bitRateRatio > 1 )
      {
        u32 *ctbrcThreshold = vcenc_instance->ctbRcThresholdCurFrame;
        u32 ctbrcDirection = vcenc_instance->ctbRcDirection;
        if (bitRateRatio <= 1.2)
        {
          for (u32 i = 0; i < ctbrcDirection-1; i ++)
            ctbrcThreshold[i] = 0;

          ctbrcThreshold[ctbrcDirection-1] = MIN(ctbrcThreshold[ctbrcDirection-1], 3);
        }
        else if (bitRateRatio <= 1.3)
        {
          for (u32 i = 0; i < ctbrcDirection; i ++)
            ctbrcThreshold[i] = 0;
        }
        else
        {
          vcenc_instance->rateControl.qpMin = vcenc_instance->rateControl.qpHdr;
        }
      }
    }
    else
    {
      if (vcenc_instance->rateControl.qpFactor > RCP_DATA_TYPE(1.2))
        vcenc_instance->rateControl.qpMin = vcenc_instance->rateControl.qpHdr;
    }

    /*qpfactor tuning*/
    if (rcQpfactor > 1.0)
    {
      double qpFactorMax = 0.9;
      if (codingType != VCENC_INTRA_FRAME)
      {
        if (bitRateRatio < 1.1)
        {
          qpFactorMax = 1;
        }
        else if (bitRateRatio < 1.2)
        {
          qpFactorMax = 1.1;
        }
        else if (bitRateRatio < 1.3)
          qpFactorMax = 1.2;
        else
          qpFactorMax = 1.4;
      }
      dQPFactor = MIN(dQPFactor, qpFactorMax);
    }

    vcenc_instance->qpfactorSSE = dQPFactor * dQPFactor;
    /*increased qpfactor only used in refine stage if modSubjPrefer!=0*/
    if (vcenc_instance->asic.regs.asicCfg->modSubjPrefer)
    {
      dQPFactor = defaultQpFactor;
    }
    *qpfactor = dQPFactor;
  }

#if 0
  double rcQpfactorPrint;
#ifdef RCP_FIXED_POINT_OPT
    rcQpfactorPrint = (double)(vcenc_instance->rateControl.qpFactor)/LL_FIXED;
#else
    rcQpfactorPrint = vcenc_instance->rateControl.qpFactor;
#endif

  printf("rcQPfactor %f, bitrateRatio %f, dQPFactor %f, qpfactorSSE %f, bMaxQP %u, qp %d, intraDeltaQP %d, qpmin %d, qpmax %d \n",
     rcQpfactorPrint, (double)vcenc_instance->currentActualBitRate/vcenc_instance->rateControl.virtualBuffer.bitRate,
     dQPFactor, vcenc_instance->qpfactorSSE, bMaxQP,
     rc->qpHdr >> QP_FRACTIONAL_BITS, rc->intraQpDelta >> QP_FRACTIONAL_BITS, vcenc_instance->rateControl.qpMin >> QP_FRACTIONAL_BITS, vcenc_instance->rateControl.qpMax >> QP_FRACTIONAL_BITS);
#endif

  return;
}


#ifdef MULTI_FRAME_SUPPORT

static VCEncRet vcencBatchCheckCodingCtrlParam(struct vcenc_instance *pEncInst,
                            const VCEncCodingCtrl *pCodeParams)
{
  if (pEncInst->batchEnable) {
    APITRACEPARAM(" %s : %d\n", "batchCount", pCodeParams->batchCount);
    if (pCodeParams->batchCount >= pEncInst->parallelCoreNum) {
      APITRACEERR(" batchCount (%d) should be smaller than parallel number (%d).\n",
        pCodeParams->batchCount, pEncInst->parallelCoreNum);
      return VCENC_INVALID_ARGUMENT;
    }
  } else {
    if (0 != pCodeParams->batchCount) {
      APITRACEWRN(" batchCount will be be set to zero if not enable batch mode.\n");
    }
  }
  return VCENC_OK;
}

static VCEncRet vcencBatchUpdateCodingCtrlParam(struct vcenc_instance *pEncInst,
                            const VCEncCodingCtrl *pCodeParams)
{
  if (pEncInst->batchEnable) {
    pEncInst->batchCount = pCodeParams->batchCount;
  } else {
    pEncInst->batchCount = 0;
  }
  return VCENC_OK;
}
#endif /* MULTI_FRAME_SUPPORT */

static void consumeInputBuffer(VCEncIn *pEncIn, VCEncOut *pEncOut)
{
    /* consume buffer of current frame */
    pEncOut->consumedAddr.inputbufBusAddr =
      pEncIn->busLuma;
    pEncOut->consumedAddr.dec400TableBusAddr =
      pEncIn->dec400LumTableBase;
    pEncOut->consumedAddr.roiMapDeltaQpBusAddr =
      pEncIn->roiMapDeltaQpAddr;
    pEncOut->consumedAddr.roimapCuCtrlInfoBusAddr =
      pEncIn->RoimapCuCtrlAddr;
    memcpy(pEncOut->consumedAddr.overlayInputBusAddr,
        pEncIn->overlayInputYAddr,
        MAX_OVERLAY_NUM * sizeof(ptr_t));
    pEncOut->consumedAddr.osdMapInputBusAddr =
      pEncIn->osdMapInputAddr;
    pEncOut->consumedAddr.outbufBusAddr =
      pEncIn->busOutBuf[0];
}
VCEncJob *handleSkipFrame(struct vcenc_instance *vcenc_instance, i32 *pNextGopSize, VCEncOut *pEncOut, VCEncIn *pEncInFor1pass, VCEncJob *job)
{
  if(vcenc_instance->rateControl.skipGop && pEncInFor1pass->poc != 0 && pEncInFor1pass->gopPicIdx != 0) {
    /* follow current GOP, handling frame skip in API */
    FindNextPic((VCEncInst)vcenc_instance, pEncInFor1pass, *pNextGopSize,
        pEncInFor1pass->gopConfig.gopCfgOffset,
        vcenc_instance->nextIdrCnt, false);
    consumeInputBuffer(&job->encIn, pEncOut);
  } else {
    if(pEncInFor1pass->gopSize > 1) {
      /* put current frame back to input queue */
      queue_put(&vcenc_instance->jobQueue, (struct node *)job);
      vcenc_instance->enqueueJobNum++;
      /* retrieve frame that need to be released due to GOP sliding
       * current frame: N
       * GOP before skip frame: N-gopSize+1, N-GopSize+2, ..., N
       * GOP after skip frame: N-gopSize+2, N-GopSize+3, ..., N+1 */
      job = SinglePassGetNextJob(vcenc_instance, pEncInFor1pass->picture_cnt - pEncInFor1pass->gopSize + 1);
      consumeInputBuffer(&job->encIn, pEncOut);
    }
    /* restart with yuv of next frame for IDR or GOP start */
    pEncInFor1pass->picture_cnt++;
    pEncInFor1pass->picture_gopIdx++;

    //if overflow, idr frame need to be put off
    if (vcenc_instance->nextIdrCnt > 0)
    {
      vcenc_instance->nextIdrCnt++;
    }
  }
  return job;
}

