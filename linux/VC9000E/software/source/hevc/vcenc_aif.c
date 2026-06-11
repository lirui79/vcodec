/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2019 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : VC Encoder Anti Intra Flicker
--
------------------------------------------------------------------------------*/


/* Includes */

#include <math.h> /* for pow */
#include "vsi_string.h"
#include "osal.h"

#include "vce_version.h"

#include "hevcencapi.h"
#include "hevcencapi_utils.h"

#include "base_type.h"
#include "error.h"
#include "instance.h"
#include "vcenc_aif.h"
#include "pool.h"
#include "sw_parameter_set.h"
#include <rate_control_picture.h>
#include "sw_slice.h"
#include "tools.h"
#include "enccommon.h"
#include "enccfg.h"
#include "hevcenccache.h"
#include "encdec400.h"
#include "enc_log.h"


/* External compiler flags */

/* Module defines */
/* Local function prototypes */

/**
 * add an sideline job for aif
 *
 * \return  >=0 the poc of added sideline job;
 * \return -1 not find intra frame;
 * \return -2 job buffers invalid;
 * \return -3 not in correct time;
 */
static i32 VCEncAifAddJob(struct vcenc_instance *inst, VCEncIn *in)
{
  VCEncRet ret = VCENC_ERROR;
  VCEncJob *intra_job = NULL;
  VCEncJob *job;
  VCEncInfo *info;

  /* wait until recon not impact the multi-core pipeline */
  //if ((inst->nextIdrCnt - in->picture_cnt) > inst->parallelCoreNum) {
  //  return -3;
  //}

  /* find the intra farme job from job queue */
  job = (VCEncJob *)queue_tail(&inst->jobQueue);
  while (NULL != job) {
    if (inst->nextIdrCnt == job->encIn.picture_cnt) {
      intra_job = job;
      break;
    }
    job = (VCEncJob *)job->next;
  }

  if (intra_job==NULL) {
    APITRACEERR("Error: Cannot find IDR frame for aif intra job.\n");
    return -1;
  }

  if (VCENC_OK != SinglePassEnqueueJob(inst, &intra_job->encIn))
    return -2;

  job = (VCEncJob *)queue_head(&inst->jobQueue);
  SetPicCfgToEncIn(&inst->lastPicCfg, &job->encIn);

  ret = VceTaskCreate(inst, &job->encIn);
  if (ret==VCENC_ERROR) {
    queue_remove(&inst->jobQueue, (struct node *)intra_job);
    return -2;
  }

  job->encIn.bIsIDR = 0; /* mark the recon frame, TODO: use macro */
  job->encIn.gopPicIdx = 0;
  job->encIn.gopConfig.id = 0;
  job->encIn.codingType = VCENC_PREDICTED_FRAME;
  job->encIn.poc = inst->aif.refer_poc + inst->lastPicCfg.gopSize;
  job->encIn.u8IdxEncodedAsLTR = 0;
  job->encIn.i8SpecialRpsIdx = -1;
  job->encIn.picture_cnt = intra_job->encIn.picture_cnt;

  info = (VCEncInfo *)job->encIn.internal;
  info->aif.type = AIF_RECON;
  info->aif.is_sideline = 1;

  inst->enqueueJobNum++;

  return job->encIn.poc;
}

VCEncRet VCEncAifCheckConfig(const VCEncConfig *enc_cfg, const EWLHwConfig_t *asic_cfg)
{
  ASSERT(enc_cfg->aifEnable);

  /* should define valid idr interval and disable GDR and 8 bits pixel depth */
  if (enc_cfg->bitDepthLuma !=8 || enc_cfg->bitDepthChroma !=8 ) {
    APITRACEERR("Error: aif not support 10 bit.\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* cannot work with parallel core and lookahead now */
  if (enc_cfg->parallelCoreNum>1 || enc_cfg->lookaheadDepth>0) {
    APITRACEERR("Error: aif not support multi-task and lookahead now.\n");
    return VCENC_INVALID_ARGUMENT;
  }

  /* todo: not support tile now */
  if (enc_cfg->tiles_enabled_flag) {
    APITRACEERR("Error: aif not support multi-tile encoding.\n");
    return VCENC_INVALID_ARGUMENT;
  }

  if (asic_cfg->tile4x4FormatSupport==0) {
    APITRACEERR("Error: aif require hardware to support tile 4x4.\n");
    return VCENC_INVALID_ARGUMENT;
  }

  return VCENC_OK;
}

VCEncRet VCEncAifCheckParams(struct vcenc_instance *inst, VCEncCodingCtrl *params)
{
  if (inst->aif.enable==0)
    return VCENC_OK;

  /* should define valid idr interval and disable GDR and 8 bits pixel depth */
  if (( params->gdrDuration !=0) || (params->inputLineBufEn)) {
    APITRACEERR("Error: aif is conflict with gdr and low-latency.\n");
    return VCENC_INVALID_ARGUMENT;
  }

  return VCENC_OK;
}


/**
 * Mark jobs with aif frame type and insert new job for AIF_RECON
 *
 * \param [out] inst->jobQueue a new sideline job is added when found AIF_REF frame
 * \param [out] job->info is updated.
 * \return VCENC_OK
 */
VCEncRet VCEncAifSchedule(VCEncInst handle, VCEncIn *in)
{
  struct vcenc_instance *inst = (struct vcenc_instance *)handle;
  VCEncInfo *info = (VCEncInfo*)(in->internal);
  VCEncRet ret = VCENC_OK;

  if (in->gopConfig.idr_interval <= inst->parallelCoreNum) {
    APITRACEWRN("aif is invalid when IDR interval is too small.\n");
    return ret;
  }

  switch(inst->aif.stage) {
  case AIF_STAGE_WAIT_INTRA:
    if (inst->has_sideline == 0) {
      i32 poc = VCEncAifAddJob(inst, in);
      if (poc>=0) {
        inst->aif.stage = AIF_STAGE_ENCODE_RECON;
        inst->aif.recon_lu_base = inst->asic.regs.reconLumBase;
        inst->aif.recon_ch_base = inst->asic.regs.reconCbBase;
        inst->has_sideline = 1;
        inst->sideline_poc = poc;
      }
    }
    break;

  case AIF_STAGE_WAIT_REF:
  case AIF_STAGE_ENCODE_INTRA:
    break;

  default:
    //ASSERT(0);
    APITRACEERR("Error: Invalid aif stage %d\n", inst->aif.stage);
    break;
  }

  return ret;
}

#if 0
static sw_picture *GetRecon(struct vcenc_instance *inst)
{
  container *c = get_container(inst);
  struct sw_picture *pic = NULL;

  /* Get free picture (if any) and remove it from picture store */
  if (!(pic = get_picture(c, -1))) {
    APITRACEERR("Error: cannot get empty recon buffer.");
    return NULL;
  }
  queue_remove(&c->picture, (struct node *)pic);

  if (pic->picture_memeory_init == 0) {
    VCEncInitPicture(vcenc_instance, 0, NULL, pic);
  }

  return pic;
}
#endif

VCEncRet VCEncAifPrepare(VCEncInst handle, VCEncIn *in, struct sw_picture *pic)
{
  struct vcenc_instance *inst = (struct vcenc_instance *)handle;
  VCEncInfo *info = (VCEncInfo *)in->internal;
  VCEncRet ret = VCENC_OK;

  switch(inst->aif.stage) {
  case AIF_STAGE_WAIT_REF:
    if ((in->gopConfig.idr_interval - in->poc) <= inst->parallelCoreNum) {

      inst->aif.stage = AIF_STAGE_WAIT_BUFFER;
      inst->aif.refer_poc = in->poc;
      inst->aif.refer_gop_size = inst->lastPicCfg.gopSize;
      memcpy(&inst->aif.refer_in, in, sizeof(VCEncIn));
      inst->aif.refer_lu_base = inst->asic.regs.reconLumBase;
      inst->aif.refer_ch_base = inst->asic.regs.reconCbBase;
      /* qp of reference */
      inst->aif.refer_qp = inst->rateControl.qpHdr;
      /* record the IDR frame index */
      inst->aif.intra_idx = inst->nextIdrCnt;

      info->aif.type = AIF_REFER;
      info->aif.is_sideline = 0;

      //memcpy(&inst->aif.rec_regs, &inst->asic.regs, sizeof(regValues_s));
    }
    break;
  case AIF_STAGE_ENCODE_RECON:
    {
      regValues_s *regs = &inst->asic.regs;
      if (inst->has_sideline) {
        inst->aif.recon_pic = pic;
        info->aif.type = AIF_RECON;
        info->aif.is_sideline = 1;
#if 0
        //inst->aif.recon_pic = pic;
        ASSERT(inst->aif.recon_pic);

        memcpy(&inst->aif.cur_regs, regs, sizeof(regValues_s));
        memcpy(regs, &inst->aif.rec_regs, sizeof(regValues_s));
        inst->aif.recon_lu_base = pic->recon.lum;
        inst->aif.recon_ch_base = pic->recon.cb;
        /* recon */
        regs->reconLumBase = pic->recon.lum;
        regs->reconCbBase = pic->recon.cb;
        regs->reconCrBase = pic->recon.cr;
        regs->reconL4nBase = pic->recon_4n_base;
        regs->mc_sync_rec_addr = pic->mc_sync_addr;
        regs->frameCtx_base = pic->framectx_base;
        regs->colctbs_store_base = pic->colctbs_store_base;
        regs->mvInfoBase = pic->mvInfoBase;

        /* input */
        regs->inputLumBase = in->busLuma;
        regs->inputCbBase = in->busChromaU;
        regs->inputCrBase = in->busChromaV;
#endif
        /* save the recon buffer info */
        inst->aif.recon_lu_base = regs->reconLumBase;
        inst->aif.recon_ch_base = regs->reconCbBase;

        /* rfc need to disable for the recon */
        regs->recon_luma_compress = 0;
        regs->recon_chroma_compress = 0;

        //TODO: coding ctrl update: -roi
        //TODO: pre-process update: crop - osd

        inst->has_sideline = 0;

      }
      break;
    }

  case AIF_STAGE_WAIT_INTRA:
    break;

  case AIF_STAGE_ENCODE_INTRA:
    {
      if (in->bIsIDR) {
        TileCtrl *tile = &inst->tileCtrl[0];
        regValues_s *regs = &inst->asic.regs;
        tile->inputLumBase = inst->aif.recon_lu_base;
        tile->inputCbBase = inst->aif.recon_ch_base;
        tile->inputCrBase = inst->aif.recon_ch_base+regs->recon_chroma_half_size;
        //crop
        tile->inputLumaBaseOffset = 0;
        tile->inputChromaBaseOffset = 0;
        regs->xFill = 0;

        regs->bPreprocessUpdate = 1;
        regs->inputImageRotation = 0;
        regs->inputImageMirror = 0;
#ifdef SYSTEM_BUILD
        regs->inputImageFormat = ASIC_INPUT_YUV420PLANAR;
        //stride
        tile->input_luma_stride = regs->ref_frame_stride/4;
        tile->input_chroma_stride = regs->ref_frame_stride_ch/8;
        regs->pixelsOnRow = regs->ref_frame_stride/4;
#else
        regs->inputImageFormat = ASIC_INPUT_YUV420_TILE4;
        //stride
        tile->input_luma_stride = regs->ref_frame_stride;
        tile->input_chroma_stride = regs->ref_frame_stride_ch;
        regs->pixelsOnRow = regs->ref_frame_stride;
        //swap
        regs->asic_pic_swap = 0;
#endif
        info->aif.type = AIF_INTRA;
        info->aif.is_sideline = 0;
      }
      break;
    }

  default:
    //ASSERT(0);
    APITRACEERR("Error: Invalid aif stage %d\n", inst->aif.stage);
    break;
  }

  return ret;
}

VCEncRet VCEncAifUpdate(VCEncInst handle, VCEncIn *in)
{
  struct vcenc_instance *inst = (struct vcenc_instance *)handle;
  VCEncInfo *info = (VCEncInfo *)in->internal;
  VCEncRet ret = VCENC_FRAME_READY;

  if (inst->aif.stage == AIF_STAGE_WAIT_REF) {
    /* do nothing if reference has not ready */
    return ret;
  }

  if (inst->aif.stage == AIF_STAGE_WAIT_BUFFER) {
    if (inst->aif.intra_idx - info->encode_cnt <= inst->parallelCoreNum)
      inst->aif.stage = AIF_STAGE_WAIT_INTRA;
  }

  if (info==NULL) {
    /** not valid for aif  process */
    return ret;
  }

  /* process the output */
  switch(info->aif.type) {
  case AIF_NONE:
  case AIF_REFER:
    /* record recon information */
    break;
  case AIF_RECON:
    {
      inst->aif.recon_pic->ref_cnt = 1;
      ret = VCENC_FRAME_CONTINUE;
      //memset((void *)inst->aif.recon_lu_base+384*16, 0, 384*16);
      inst->aif.stage = AIF_STAGE_ENCODE_INTRA;
    }
    break;
  case AIF_INTRA:
    {
      inst->asic.regs.bPreprocessUpdate = 1;
      inst->aif.recon_pic->ref_cnt = 0;
      inst->aif.stage = AIF_STAGE_WAIT_REF;
    }
    break;
  default:
    //ASSERT(0);
    APITRACEERR("Error: Invalid aif frame type %d\n", info->aif.type);
    break;
  }

  return ret;
}
