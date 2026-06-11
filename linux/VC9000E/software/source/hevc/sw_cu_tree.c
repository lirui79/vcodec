/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------*/
#ifdef CUTREE_BUILD_SUPPORT

#include "vsi_string.h"
#include "hevcencapi.h"
#include "hevcencapi_utils.h"
#include "sw_cu_tree.h"
#include "instance.h"
#include "pool.h"
#include "tools.h"
#include "sw_slice.h"
#include "osal.h"
#include "ewl.h"

#ifdef TEST_DATA
#include "enctrace.h"
#endif

void setFrameTypeChar(struct Lowres *frame);
VCEncRet StartCuTreeThread(struct cuTreeCtr *m_param);
static void GopDecision(struct AGopInfo **gopInfoAddr, const u32 nGopInfo);
static u32 GetNextGopSize(struct cuTreeCtr *m_param);

void statisAheadData(struct cuTreeCtr *m_param, struct Lowres **frames,
                     int numframes, bool bFirst) {
  i32 i;
  i32 idx = !bFirst;
  i32 gopSize = (numframes > 0 ? frames[1]->gopSize : 0);
  u64 costGop[4] = {0}, costAvg[4] = {0};

  /* store the costs */
  for (i = 0; i < 4; i++) {
    m_param->costAvgInt[i] = m_param->costGopInt[i] = 0;
    m_param->FrameTypeNum[i] = m_param->FrameNumGop[i] = 0;
  }
  for (i = idx; i <= numframes; i++) {
    i32 id = frames[i]->predId;
    m_param->FrameTypeNum[id]++;
    costAvg[id] += frames[i]->cost;
  }
  for (i = 1; i <= MIN(numframes, gopSize); i++) {
    i32 id = frames[i]->predId;
    m_param->FrameNumGop[id]++;
    costGop[id] += frames[i]->cost;
  }
  for (i = 0; i < 4; i++) {
    if (m_param->FrameTypeNum[i])
      m_param->costAvgInt[i] = (costAvg[i] + m_param->FrameTypeNum[i] / 2) /
                               m_param->FrameTypeNum[i];

    if (m_param->FrameNumGop[i])
      m_param->costGopInt[i] =
          (costGop[i] + m_param->FrameNumGop[i] / 2) / m_param->FrameNumGop[i];
  }
}

/* collect input frame info from pass 1 instance (in lookahead thread) */
static void initFrameFromEncInst(struct Lowres *cur_frame,
                                 struct cuTreeCtr *m_param,
                                 struct vcenc_instance *pEncInst,
                                 VCEncIn *pEncIn, VCEncOut *pEncOut) {
  int i, j;
  encOutForCutree_s *out = &pEncInst->outForCutree;

  cur_frame->poc = out->poc;
  cur_frame->frameNum = m_param->frameNum++;
  cur_frame->qp = (out->qp >> QP_FRACTIONAL_BITS);

  cur_frame->cost = 0.0;
  cur_frame->gopEncOrder = pEncIn->gopPicIdx;
  if (out->codingType == VCENC_PREDICTED_FRAME) {
    cur_frame->sliceType = VCENC_FRAME_TYPE_P;
  } else if (out->codingType == VCENC_INTRA_FRAME) {
    if (pEncInst->intraPeriod > 0 && cur_frame->poc != 0) {
      cur_frame->sliceType = VCENC_FRAME_TYPE_I;
    }
    else {
      cur_frame->sliceType = VCENC_FRAME_TYPE_IDR;
    }
  } else if (out->codingType == VCENC_BIDIR_PREDICTED_FRAME) {
    cur_frame->sliceType =
        cur_frame->gopEncOrder ? VCENC_FRAME_TYPE_B : VCENC_FRAME_TYPE_BLDY;
  }
  setFrameTypeChar(cur_frame);
  cur_frame->predId = getFramePredId(cur_frame->sliceType);
  cur_frame->gopSize =
      (cur_frame->sliceType == VCENC_FRAME_TYPE_IDR ? 1 : pEncIn->gopSize);
  /* adjust gopEncOrder for leading pictures */
  if (cur_frame->sliceType == VCENC_FRAME_TYPE_IDR && pEncInst->enableLeadingPictures) {
    cur_frame->gopSize = pEncIn->gopSize;
    cur_frame->gopEncOrder = 0;
  } else if(pEncIn->poc < 0) {
    cur_frame->gopEncOrder = cur_frame->gopSize - pEncIn->gopPicIdx;
  }
  cur_frame->gopEnd = cur_frame->gopEncOrder == (cur_frame->gopSize - 1);
  m_param->dsRatio = out->dsRatio + 1;
  if (out->extDSRatio) m_param->dsRatio = out->extDSRatio + 1;

  if (cur_frame->sliceType != VCENC_FRAME_TYPE_IDR &&
      cur_frame->sliceType != VCENC_FRAME_TYPE_I)
    cur_frame->p0 = out->p0;
  if (IS_CODING_TYPE_B(cur_frame->sliceType)) cur_frame->p1 = out->p1;

  m_param->roiMapEnable = out->roiMapEnable && out->pRoiMapDelta;
  if (m_param->bHWMultiPassSupport) {
    cur_frame->motionScore[0][0] = out->motionScore[0][0];
    cur_frame->motionScore[0][1] = out->motionScore[0][1];
    cur_frame->motionScore[1][0] = out->motionScore[1][0];
    cur_frame->motionScore[1][1] = out->motionScore[1][1];
    cur_frame->outRoiMapDeltaQpIdx = INVALID_INDEX;
    cur_frame->cuDataIdx = out->cuDataIdx;
    cur_frame->inRoiMapDeltaBinIdx = INVALID_INDEX;
    m_param->roiMapEnable = m_param->roiMapEnable && out->roiMapDeltaQpAddr;

    if (m_param->roiMapEnable) {
      if (m_param->inRoiMapDeltaBin_Base == 0) {
        m_param->inRoiMapDeltaBin_Base = out->roiMapDeltaQpAddr;
        m_param->inRoiMapDeltaBin_frame_size = out->roiMapDeltaSize;
      }
      cur_frame->inRoiMapDeltaBinIdx =
          ((ptr_t)out->roiMapDeltaQpAddr - m_param->inRoiMapDeltaBin_Base) /
          m_param->inRoiMapDeltaBin_frame_size;
    }
  }
}

static void releaseFrame(struct Lowres *cur_frame, void *cutreeJobBufferPool,
                         void *jobBufferPool) {
  int i, j;

  if (cur_frame == NULL)
  	return;

  if (cur_frame->propagateCost) {
    EWLfree(cur_frame->propagateCost);
    cur_frame->propagateCost = NULL;
  }
  if (cur_frame->qpCuTreeOffset) {
    EWLfree(cur_frame->qpCuTreeOffset);
    cur_frame->qpCuTreeOffset = NULL;
  }
  if (cur_frame->qpAqOffset) {
    EWLfree(cur_frame->qpAqOffset);
    cur_frame->qpAqOffset = NULL;
  }
  if (cur_frame->intraCost) {
    EWLfree(cur_frame->intraCost);
    cur_frame->intraCost = NULL;
  }
  if (cur_frame->invQscaleFactor) {
    EWLfree(cur_frame->invQscaleFactor);
    cur_frame->invQscaleFactor = NULL;
  }
  for (i = 0; i <= MAX_GOP_SIZE + 1; i++)
    for (j = 0; j <= MAX_GOP_SIZE + 1; j++) {
      if (cur_frame->lowresCosts[i][j]) {
        EWLfree(cur_frame->lowresCosts[i][j]);
        cur_frame->lowresCosts[i][j] = NULL;
      }
    }
  for (i = 0; i <= 1; i++)
    for (j = 0; j <= MAX_GOP_SIZE + 1; j++) {
      if (cur_frame->lowresMvs[i][j]) {
        EWLfree(cur_frame->lowresMvs[i][j]);
        cur_frame->lowresMvs[i][j] = NULL;
      }
    }
  if (cur_frame->job) {
    PutBufferToPool(jobBufferPool, (void **)&cur_frame->job);
  }
  if (cur_frame) {
    PutBufferToPool(cutreeJobBufferPool, (void **)&cur_frame);
  }
}
void remove_one_frame(struct cuTreeCtr *m_param) {
  //remove one frame from queue
  struct Lowres *out_frame = *m_param->lookaheadFrames;
  void *cutreeJobBufferPool =
      ((struct vcenc_instance *)m_param->pEncInst)->cutreeJobBufferPool;
  void *jobBufferPool =
      ((struct vcenc_instance *)m_param->pEncInst)->lookahead.jobBufferPool;
  releaseFrame(out_frame, cutreeJobBufferPool, jobBufferPool);

  *m_param->lookaheadFrames = NULL;
  ++m_param->lookaheadFrames;
  --m_param->nLookaheadFrames;
  --m_param->lastGopEnd;

  if (m_param->lookaheadFrames - m_param->lookaheadFramesBase >=
      m_param->nLookaheadFrames) {
    memcpy(m_param->lookaheadFramesBase, m_param->lookaheadFrames,
           m_param->nLookaheadFrames * sizeof(struct Lowres *));
    m_param->lookaheadFrames = m_param->lookaheadFramesBase;
  }
}

void setFrameTypeChar(struct Lowres *frame) {
  char type = 0;
  switch (frame->sliceType) {
    case VCENC_FRAME_TYPE_I:
    case VCENC_FRAME_TYPE_IDR:
      type = 'I';
      break;
    case VCENC_FRAME_TYPE_P:
      type = 'P';
      break;
    case VCENC_FRAME_TYPE_B:
      type = 'b';
      break;
    case VCENC_FRAME_TYPE_BREF:
      type = 'B';
      break;
    case VCENC_FRAME_TYPE_BLDY:
      type = 'L';
      break;
    default:
      break;
  }
  frame->typeChar = type;
}

i32 getFramePredId(i32 type) {
  i32 id = 0;
  switch (type) {
    case VCENC_FRAME_TYPE_I:
    case VCENC_FRAME_TYPE_IDR:
    case 'I':
      id = I_SLICE;
      break;

    case VCENC_FRAME_TYPE_P:
    case VCENC_FRAME_TYPE_BLDY:
    case 'P':
    case 'L':
      id = P_SLICE;
      break;

    case VCENC_FRAME_TYPE_B:
    case 'b':
      id = B_SLICE;
      break;

    case VCENC_FRAME_TYPE_BREF:
    case 'B':
      id = 3;
      break;

    default:
      break;
  }
  return id;
}

static i32 process_one_frame(struct cuTreeCtr *m_param) {
  struct Lowres *frames[VCENC_LOOKAHEAD_MAX + MAX_GOP_SIZE + 4];
  struct Lowres *out_frame = *m_param->lookaheadFrames;
  bool bIntra = (out_frame->sliceType == VCENC_FRAME_TYPE_I ||
                 out_frame->sliceType == VCENC_FRAME_TYPE_IDR);
  struct Lowres *middle_frame, *last_nonb;
  int i;

  if (m_param->bHWMultiPassSupport) {
    return VCEncCuTreeProcessOneFrame(m_param);
  }
  ASSERT(0 && "sw cutree support removed!");
  return NOK;
}

//Init cuTree
VCEncRet cuTreeInit(struct cuTreeCtr *m_param, VCEncInst inst,
                    const VCEncConfig *config) {
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  m_param->pEncInst = inst;
  i32 i;

  //alg default parameter
  m_param->bEnableWeightedBiPred = 0;
  m_param->bBPyramid = 1;
  m_param->bBHierachy = 1;
  m_param->lookaheadDepth = config->lookaheadDepth;
  m_param->qgSize = 32;
  m_param->qCompress = 0.6;
  m_param->m_cuTreeStrength =
      (int)(5.0 * (1.0 - m_param->qCompress) * 256.0 + 0.5);
  m_param->gopSize = config->gopSize;

  /* Irq type cutree mask */
  m_param->asic.regs.irq_type_bus_error_mask =
      (config->irqTypeCutreeMask >> 5) & 0x01;
  m_param->asic.regs.irq_type_timeout_mask =
      (config->irqTypeCutreeMask >> 4) & 0x01;
  m_param->asic.regs.irq_type_frame_rdy_mask = config->irqTypeCutreeMask & 0x01;

  m_param->asic.secure_mode = config->secure_mode;
  m_param->priority = config->priority;
  m_param->core_mask = config->core_mask;

  //from encoder
  m_param->unitSize = 16;
  m_param->widthInUnit =
      (vcenc_instance->width + m_param->unitSize - 1) / m_param->unitSize;
  m_param->heightInUnit =
      (vcenc_instance->height + m_param->unitSize - 1) / m_param->unitSize;
  m_param->unitCount = (m_param->widthInUnit) * (m_param->heightInUnit);
  m_param->fpsNum = vcenc_instance->rateControl.outRateNum;
  m_param->fpsDenom = vcenc_instance->rateControl.outRateDenom;
  m_param->width = vcenc_instance->width;
  m_param->height = vcenc_instance->height;
  m_param->max_cu_size = vcenc_instance->max_cu_size;
  m_param->roiMapEnable = vcenc_instance->roiMapEnable;
  m_param->codecFormat = vcenc_instance->codecFormat;
  m_param->imFrameCostRefineEn = vcenc_instance->rateControl.pass1EstCostAll;
  m_param->outRoiMapDeltaQpBlockUnit = 1;
  /* In sharp_visual mode, input down-sample is disabled for better subjective quality,
       and so the H264 qp_delta size is block 16 instead of block 32 */
#ifdef H264_LA_BLOCK_SIZE
  if (IS_H264(m_param->codecFormat) &&
      config->tune == VCENC_TUNE_SHARP_VISUAL &&
      vcenc_instance->asic.regs.asicCfg->tuneToolsSet2Support)
    m_param->outRoiMapDeltaQpBlockUnit = H264_LA_BLOCK_SIZE;
#endif

  //temp buffer for cutree propagate
  m_param->m_scratch = EWLmalloc(sizeof(int64_t) * m_param->widthInUnit);

  m_param->nLookaheadFrames = 0;
  m_param->lastGopEnd = 0;
  m_param->lookaheadFrames = m_param->lookaheadFramesBase;
  m_param->frameNum = 0;
  for (i32 i = 0; i < 4; i++) {
    m_param->FrameTypeNum[i] = 0;
    m_param->costAvgInt[i] = 0;
    m_param->FrameNumGop[i] = 0;
    m_param->costGopInt[i] = 0;
  }
  m_param->bUpdateGop = config->bPass1AdaptiveGop;
  m_param->latestGopSize = 0;
  m_param->maxHieDepth = DEFAULT_MAX_HIE_DEPTH;
  m_param->bHWMultiPassSupport =
      vcenc_instance->asic.regs.asicCfg->bMultiPassSupport;

  m_param->asic.regs.vcmd.vcmdBufSize = 0;
  m_param->enableLeadingPictures = config->enableLeadingPictures;

  /*AXI max burst length */
  if (0 == config->burstMaxLength)
    m_param->asic.regs.AXI_burst_max_length =
        ENCH2_DEFAULT_BURST_LENGTH;  //default
  else
    m_param->asic.regs.AXI_burst_max_length = config->burstMaxLength;

  /* Init segment qps */
  m_param->segmentCountEnable = IS_VP9(vcenc_instance->codecFormat);
  for (i = 0; i < MAX_SEGMENTS; i++) {
    m_param->segment_qp[i] = segment_delta_qp[i];
  }

  {
    //allocate delta qp map memory.
    // 4 bits per block.
    int block_size =
        ((vcenc_instance->width + vcenc_instance->max_cu_size - 1) &
         (~(vcenc_instance->max_cu_size - 1))) *
        ((vcenc_instance->height + vcenc_instance->max_cu_size - 1) &
         (~(vcenc_instance->max_cu_size - 1))) /
        (8 * 8 * 2);
    // 8 bits per block if ipcm map/absolute roi qp is supported
    if (vcenc_instance->asic.regs.asicCfg->roiMapVersion >= 1) block_size *= 2;
    i32 in_loop_ds_ratio = vcenc_instance->preProcess.inLoopDSRatio + 1;
    block_size *= in_loop_ds_ratio * in_loop_ds_ratio;

    /* Put VP9 Segment count buffer at end of each roi buffer */
    if (IS_VP9(vcenc_instance->codecFormat)) {
      block_size += 8 * sizeof(u32);
      m_param->roiMapDeltaQpMemFactory[0].mem_type |= CPU_RD;
    }
    block_size = ((block_size + 63) & (~63));
    m_param->roiMapDeltaQpMemFactory[0].mem_type =
        CPU_WR | VPU_WR | VPU_RD | EWL_MEM_TYPE_VPU_WORKING;
    SET_MEM_USAGE(m_param->roiMapDeltaQpMemFactory[0].mem_type, EWL_MEM_USAGE_IN_QPMAP,
                  m_param->asic.secure_mode);
    if (EWLMallocLinear(
            vcenc_instance->asic.ewl,
            block_size * CUTREE_BUFFER_NUM + ROIMAP_PREFETCH_EXT_SIZE, 0,
            &m_param->roiMapDeltaQpMemFactory[0]) != EWL_OK) {
      for (i = 0; i < CUTREE_BUFFER_NUM; i++) {
        m_param->roiMapDeltaQpMemFactory[i].virtualAddress = NULL;
      }
      m_param->bStatus = THREAD_STATUS_CUTREE_ERROR;
      cuTreeRelease(m_param, 1);
      return VCENC_EWL_MEMORY_ERROR;
    } else {
      i32 total_size = m_param->roiMapDeltaQpMemFactory[0].size;
      memset(m_param->roiMapDeltaQpMemFactory[0].virtualAddress, 0,
             block_size * CUTREE_BUFFER_NUM);
      EWLSyncMemData(&m_param->roiMapDeltaQpMemFactory[0], 0,
                     block_size * CUTREE_BUFFER_NUM, HOST_TO_DEVICE);

      for (i = 0; i < CUTREE_BUFFER_NUM; i++) {
        m_param->roiMapDeltaQpMemFactory[i].virtualAddress =
            (u32 *)((ptr_t)m_param->roiMapDeltaQpMemFactory[0].virtualAddress +
                    i * block_size);
        m_param->roiMapDeltaQpMemFactory[i].busAddress =
            m_param->roiMapDeltaQpMemFactory[0].busAddress + i * block_size;
        m_param->roiMapDeltaQpMemFactory[i].size =
            (i < CUTREE_BUFFER_NUM - 1
                 ? block_size
                 : total_size - (CUTREE_BUFFER_NUM - 1) * block_size);
        m_param->roiMapRefCnt[i] = 0;
      }
      m_param->outRoiMapSegmentCountOffset =
          m_param->roiMapDeltaQpMemFactory[1].busAddress -
          m_param->roiMapDeltaQpMemFactory[0].busAddress -
          MAX_SEGMENTS * sizeof(u32);
    }
  }

  m_param->ctx = vcenc_instance->ctx;
  m_param->slice_idx = vcenc_instance->slice_idx;
  m_param->bStatus = THREAD_STATUS_OK;
  if (m_param->bHWMultiPassSupport) {
    VCEncRet ret;
    ret = VCEncCuTreeInit(m_param);
    if (ret != VCENC_OK) {
      m_param->bStatus = THREAD_STATUS_CUTREE_ERROR;
      cuTreeRelease(m_param, 1);
      return ret;
    }
  }
  queue_init(&m_param->jobs);
  queue_init(&m_param->agop);
  m_param->job_cnt = 0;
  m_param->output_cnt = 0;
  m_param->total_frames = 0;
  vcenc_instance->asic.regs.bInitUpdate = 1;

  if (m_param->tid_cutree == NULL) StartCuTreeThread(m_param);

  return VCENC_OK;
}
u8 *GetRoiMapBufferFromBufferPool(struct cuTreeCtr *m_param, ptr_t *busAddr) {
  int i;
  u8 *ret = NULL;

  pthread_mutex_lock(&m_param->roibuf_mutex);
  while (ret == NULL) {
    for (i = 0; i < CUTREE_BUFFER_NUM; i++)
      if (m_param->roiMapRefCnt[i] == 0) {
        m_param->roiMapRefCnt[i]++;
        ret = (u8 *)m_param->roiMapDeltaQpMemFactory[i].virtualAddress;
        *busAddr = m_param->roiMapDeltaQpMemFactory[i].busAddress;
        break;
      }
    pthread_mutex_lock(&m_param->status_mutex);
    THREAD_STATUS bStatus = m_param->bStatus;
    pthread_mutex_unlock(&m_param->status_mutex);
    if (bStatus >= THREAD_STATUS_LOOKAHEAD_ERROR && ret == NULL) {
      break;
    }
    if (ret == NULL)
      pthread_cond_wait(&m_param->roibuf_cond, &m_param->roibuf_mutex);
  }
  pthread_mutex_unlock(&m_param->roibuf_mutex);
  return ret;
}
void PutRoiMapBufferToBufferPool(struct cuTreeCtr *m_param, ptr_t addr) {
  int i;
  if (addr == 0) return;
  pthread_mutex_lock(&m_param->roibuf_mutex);
  for (i = 0; i < CUTREE_BUFFER_NUM; i++)
    if (m_param->roiMapDeltaQpMemFactory[i].busAddress == addr) {
      m_param->roiMapRefCnt[i]--;
      break;
    }
  pthread_cond_signal(&m_param->roibuf_cond);
  pthread_mutex_unlock(&m_param->roibuf_mutex);
}

//Release cuTree resource
void cuTreeRelease(struct cuTreeCtr *m_param, u8 error) {
  TerminateCuTreeThread(m_param, error);
  while (m_param->nLookaheadFrames) remove_one_frame(m_param);

  struct vcenc_instance *vcenc_instance =
      (struct vcenc_instance *)m_param->pEncInst;
  EWLFreeLinear(vcenc_instance->asic.ewl, &m_param->roiMapDeltaQpMemFactory[0]);

  if (m_param->bHWMultiPassSupport) VCEncCuTreeRelease(m_param);

  if (m_param->m_scratch) EWLfree(m_param->m_scratch);

  m_param->m_scratch = NULL;
}

void DestroyThread(VCEncLookahead *p2_lookahead, struct cuTreeCtr *m_param) {
  /* release cutree thread related resource */
  {
    pthread_mutex_destroy(&m_param->cutree_mutex);
    pthread_cond_destroy(&m_param->cutree_cond);
    pthread_mutex_destroy(&m_param->roibuf_mutex);
    pthread_cond_destroy(&m_param->roibuf_cond);
    pthread_mutex_destroy(&m_param->cuinfobuf_mutex);
    pthread_cond_destroy(&m_param->cuinfobuf_cond);

    while (m_param->nLookaheadFrames) remove_one_frame(m_param);

    ReleaseBufferPool(
        &((struct vcenc_instance *)m_param->pEncInst)->cutreeJobBufferPool);

    while (m_param->agop.head != NULL) {
      struct agop_res *res = (struct agop_res *)queue_get(&m_param->agop);
      VCENC_FREE(res);
    }

    struct vcenc_instance *vcenc_instance =
        (struct vcenc_instance *)m_param->pEncInst;
    EWLFreeLinear(vcenc_instance->asic.ewl,
                  &m_param->roiMapDeltaQpMemFactory[0]);

    if (m_param->bHWMultiPassSupport) VCEncCuTreeRelease(m_param);

    if (m_param->m_scratch) VCENC_FREE(m_param->m_scratch);

    m_param->m_scratch = NULL;
  }

  /* release lookahead thread related resource */
  {
    struct vcenc_instance *vcenc_inst_pass1 =
        (struct vcenc_instance *)(p2_lookahead->priv_inst);
    VCEncLookahead *p1_lookahead = &vcenc_inst_pass1->lookahead;

    pthread_mutex_destroy(&p2_lookahead->job_mutex);
    pthread_mutex_destroy(&p1_lookahead->output_mutex);
    pthread_cond_destroy(&p2_lookahead->job_cond);
    pthread_cond_destroy(&p1_lookahead->output_cond);

    VCEncLookaheadJob *job = NULL;
    while ((job = (VCEncLookaheadJob *)queue_get(&p2_lookahead->jobs)) !=
           NULL) {
      PutBufferToPool(p2_lookahead->jobBufferPool, (void **)&job);
    }

    while ((job = (VCEncLookaheadJob *)queue_get(&p2_lookahead->output)) !=
           NULL) {
      VCENC_FREE(job);  // no jobs in p2_lookahead->output
    }

    while ((job = (VCEncLookaheadJob *)queue_get(&p1_lookahead->output)) !=
           NULL) {
      PutBufferToPool(p2_lookahead->jobBufferPool, (void **)&job);
    }
  }

  /* release job buffer pool */
  ReleaseBufferPool(&p2_lookahead->jobBufferPool);
}

/* Collect frame info for cutree in lookahead thread */
VCEncRet cuTreeAddFrame(VCEncInst inst, VCEncLookaheadJob *job,
                        void *agopInfoBuf, i32 bGopSizeDecideByUser) {
  struct vcenc_instance *pEncInst = (struct vcenc_instance *)inst;
  VCEncLookahead *p1_lookahead = &pEncInst->lookahead;
  VCEncIn *pEncIn = &job->encIn;
  VCEncOut *pEncOut = &job->encOut;
  struct cuTreeCtr *m_param = &pEncInst->cuTreeCtl;
  struct AGopInfo *agopInfo = NULL;
  struct AGopInfo **gopInfoAddr = pEncInst->gopInfoAddr;
  u32 *nGopInfo = &pEncInst->nGopInfo;  //number of agopInfo
  struct Lowres *cur_frame = NULL;
  u32 bGopEnd = 0;
  VCEncRet ret =
      GetBufferFromPool(pEncInst->cutreeJobBufferPool, (void **)&cur_frame);
  if (VCENC_OK != ret || !cur_frame) {
    //notify pass2 error happened
    job->status = VCENC_ERROR;
    LookaheadEnqueueOutput(p1_lookahead, job);
    return ret;
  }
  memset(cur_frame, 0, sizeof(struct Lowres));

  initFrameFromEncInst(cur_frame, m_param, pEncInst, pEncIn, pEncOut);
  cur_frame->job = job;
  bGopEnd = cur_frame->gopEnd;

  if (m_param->bHWMultiPassSupport && m_param->bUpdateGop &&
      !bGopSizeDecideByUser) {
    //save motionscore
    if (cur_frame->sliceType != VCENC_FRAME_TYPE_IDR &&
        cur_frame->sliceType != VCENC_FRAME_TYPE_I &&
        (cur_frame->gopSize == 4 ||
         cur_frame->gopSize ==
             8))  //only gop=4 and gop=8 need make agop decision
    {
      GetBufferFromPool(agopInfoBuf, (void **)&agopInfo);
      if (!agopInfo) {
        printf("Get AgopInfo buffer failed!\n");
        //notify pass2 error happened
        job->status = VCENC_ERROR;
        LookaheadEnqueueOutput(p1_lookahead, job);
        return VCENC_ERROR;
      }
      memset(agopInfo, 0, sizeof(struct AGopInfo));
      agopInfo->poc = cur_frame->poc;
      agopInfo->aGopSize = cur_frame->aGopSize;
      agopInfo->sliceType = cur_frame->sliceType;
      agopInfo->gopSize = cur_frame->gopSize;
      memcpy(agopInfo->motionScore, cur_frame->motionScore, 4 * sizeof(u32));

      //save agopInfo' address according to display order.
      u32 i = (*nGopInfo)++;
      while (i > 0 && gopInfoAddr[i - 1]->poc > agopInfo->poc) {
        gopInfoAddr[i] = gopInfoAddr[i - 1];
        --i;
      }
      gopInfoAddr[i] = agopInfo;

      // make gop decision
      if (cur_frame->gopEnd && *nGopInfo >= 8) {
        GopDecision(gopInfoAddr, *nGopInfo);
        if (!gopInfoAddr[0]->aGopSize && 4 == gopInfoAddr[0]->gopSize &&
            4 == m_param->latestGopSize) {
          //if not change agop size, current gop size and next gop size are 4, save last 4 gopinfo for next gop decision.
          for (u32 i = 0; i < 4; i++) {
            PutBufferToPool(agopInfoBuf, (void **)&gopInfoAddr[i]);
          }
          *nGopInfo = 4;
          memmove(gopInfoAddr, gopInfoAddr + 4, sizeof(struct AGopInfo *) * 4);
          memset(gopInfoAddr + 4, 0, sizeof(struct AGopInfo *) * 4);
        } else {
          if (gopInfoAddr[0]->aGopSize) {
            cur_frame->aGopSize = gopInfoAddr[0]->aGopSize;
            m_param->latestGopSize = gopInfoAddr[0]->aGopSize;
          }
          //reset agopInfo after gopDecision.
          for (u32 i = 0; i < *nGopInfo; i++) {
            PutBufferToPool(agopInfoBuf, (void **)&gopInfoAddr[i]);
          }
          memset(gopInfoAddr, 0, sizeof(struct AGopInfo *) * 8);
          *nGopInfo = 0;
        }
      }
    } else  //for force intra frame
    {
      //reset agopInfo after gopDecision.
      for (u32 i = 0; i < *nGopInfo; i++) {
        PutBufferToPool(agopInfoBuf, (void **)&gopInfoAddr[i]);
      }
      memset(gopInfoAddr, 0, sizeof(struct AGopInfo *) * 8);
      *nGopInfo = 0;
    }
  }

  pthread_mutex_lock(&m_param->cutree_mutex);
  queue_put(&m_param->jobs, (struct node *)cur_frame);
#ifdef GLOBAL_MV_ON_SEARCH_RANGE
  extern i32 globalMv[2][3];
  memcpy(cur_frame->job->frame.gmv, globalMv, sizeof(globalMv));
#endif
  m_param->job_cnt++;
  m_param->total_frames++;
  pthread_cond_signal(&m_param->cutree_cond);
  pthread_mutex_unlock(&m_param->cutree_mutex);

  if (bGopSizeDecideByUser)  //encode in input order, don't make agop decision
  {
    //reset agopInfo
    for (u32 i = 0; i < *nGopInfo; i++) {
      PutBufferToPool(agopInfoBuf, (void **)&gopInfoAddr[i]);
    }
    memset(gopInfoAddr, 0, sizeof(struct AGopInfo *) * 8);
    *nGopInfo = 0;
  }

  pthread_mutex_lock(&m_param->status_mutex);
  THREAD_STATUS bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  if (bStatus == THREAD_STATUS_CUTREE_FLUSH ||
      m_param->total_frames >=
          CUTREE_BUFFER_CNT(m_param->lookaheadDepth, m_param->gopSize)) {
    return VCENC_FRAME_READY;
  } else if (bStatus >= THREAD_STATUS_LOOKAHEAD_ERROR)
    return VCENC_ERROR;
  else
    return VCENC_FRAME_ENQUEUE;
}

/* Handle input frame in cutree thread */
i32 cuTreeHandleInputFrame(struct Lowres *cur_frame,
                           struct cuTreeCtr *m_param) {
  VCEncLookaheadJob *job = cur_frame->job;
  VCEncIn *pEncIn = &job->encIn;
  VCEncOut *pEncOut = &job->encOut;
  i32 width = m_param->width;
  i32 height = m_param->height;
  i32 ctuPerRow = (width + m_param->max_cu_size - 1) / m_param->max_cu_size;
  i32 ctuPerCol = (height + m_param->max_cu_size - 1) / m_param->max_cu_size;
  VCEncCuInfo cuInfo;
  u32 *ctuTable = pEncOut->cuOutData.ctuOffset;
  i32 iCtuX, iCtuY, iCu, iPu = 0, iX, iY, iBlk;
  i32 iCtu = 0;
  i32 nblks;
  int widthInUnit = m_param->widthInUnit;
  i32 p0 = 0;
  i32 p1 = 0;
  int i;
  u64 totalCost = 0;

  //insert to lookahead queue
  i = m_param->nLookaheadFrames++;
  if (cur_frame->sliceType != VCENC_FRAME_TYPE_IDR) {
    while (i > 0 && m_param->lookaheadFrames[i - 1]->poc > cur_frame->poc
        && !(cur_frame->poc < 0 && m_param->lookaheadFrames[i - 1]->poc >= 0)) {
      m_param->lookaheadFrames[i] = m_param->lookaheadFrames[i - 1];
      --i;
    }
    if(cur_frame->poc < 0 && m_param->lookaheadFrames[i-1]->poc == 0) {
      // move beyond the first IDR
      m_param->lookaheadFrames[i] = m_param->lookaheadFrames[i - 1];
      --i;
      // stop before the next IDR or normal frames
      while (i > 0 && m_param->lookaheadFrames[i - 1]->poc > cur_frame->poc
          && !(cur_frame->poc < 0 && m_param->lookaheadFrames[i - 1]->poc >= 0)) {
        m_param->lookaheadFrames[i] = m_param->lookaheadFrames[i - 1];
        --i;
      }
    }
  }
  m_param->lookaheadFrames[i] = cur_frame;

  //if hw support MultiPass, make agop decision in pass1
  if (m_param->bHWMultiPassSupport) {
    if (m_param->bUpdateGop && cur_frame->gopEnd && cur_frame->aGopSize &&
        m_param->nLookaheadFrames > 8) {
      m_param->lookaheadFrames[m_param->nLookaheadFrames - 1]->aGopSize =
          cur_frame->aGopSize;
      m_param->lookaheadFrames[m_param->nLookaheadFrames - 1 - 4]->aGopSize =
          cur_frame->aGopSize;
    }
  }
  // hw not support multiPass, make agop decision in cutree, and send to pass1
  else {
    ASSERT(0 && "sw cutree support removed!");
  }
  // record number of frames in queue on gopEnd
  if (cur_frame->gopEnd) m_param->lastGopEnd = m_param->nLookaheadFrames;

  /* one frame analysis
     for GOP16 & IM, process cutree job when queue size exceeds lookaheadDepth by half GOP, ignoring incomplete GOP.
  */
  i32 gopEnd = cur_frame->gopEnd;
  i32 gopSize = cur_frame->gopSize;  //avoid invalid read
  //m_param->lookaheadDepth + 1: 1 represent the last I/P have been processed but stiil in queue
  //so max nLookaheadFrames is 40+1+16/2 = 49 (current max gopsize=16)
  while ((m_param->nLookaheadFrames >= m_param->lookaheadDepth && gopEnd) ||
         (m_param->nLookaheadFrames >=
          (m_param->lookaheadDepth + 1 + gopSize / 2)))
    if (process_one_frame(m_param) != OK) return NOK;

  return OK;
}

void cuTreeFlush(struct cuTreeCtr *m_param) {
  if (!m_param->tid_cutree) return;
  pthread_mutex_lock(&m_param->cutree_mutex);
  pthread_cond_signal(&m_param->cutree_cond);
  pthread_mutex_unlock(&m_param->cutree_mutex);

  pthread_mutex_lock(&m_param->roibuf_mutex);
  pthread_cond_signal(&m_param->roibuf_cond);
  pthread_mutex_unlock(&m_param->roibuf_mutex);
}

/*********************
 * Lookahead thread  *
 *********************/

/* prepare pass 1 job */
VCEncRet AddPictureToLookahead(struct vcenc_instance *vcenc_instance,
                               const VCEncIn *pEncIn, VCEncOut *pEncOut) {
  VCEncRet ret = VCENC_ERROR;
  VCEncLookaheadJob *job = NULL;
  VCEncLookahead *lookahead = &vcenc_instance->lookahead;
  ptr_t *tmpBusLuma, *tmpBusU, *tmpBusV;
  u32 iBuf, tileId;

  ret = GetBufferFromPool(lookahead->jobBufferPool, (void **)&job);
  if (VCENC_OK != ret || !job) return ret;
  memset(job, 0, sizeof(VCEncLookaheadJob));

  memcpy(&job->encIn, pEncIn, sizeof(VCEncIn));
  memcpy(&job->encOut, pEncOut, sizeof(VCEncOut));

  if (vcenc_instance->num_tile_columns > 1)
    job->encIn.tileExtra =
        (VCEncInTileExtra *)((u8 *)job + sizeof(VCEncLookaheadJob));

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

    if (tileId > 0) {
      job->encOut.tileExtra[tileId - 1].pNaluSizeBuf =
          pEncOut->tileExtra[tileId - 1].pNaluSizeBuf;
      job->encOut.tileExtra[tileId - 1].streamSize =
          pEncOut->tileExtra[tileId - 1].streamSize;
      job->encOut.tileExtra[tileId - 1].numNalus =
          pEncOut->tileExtra[tileId - 1].numNalus;
      job->encOut.tileExtra[tileId - 1].cuOutData =
          pEncOut->tileExtra[tileId - 1].cuOutData;
    }
  }
  job->encIn.gopConfig.pGopPicCfg = pEncIn->gopConfig.pGopPicCfgPass1;

  //match frame with parameter
  EncCodingCtrlParam *pEncCodingCtrlParam = (EncCodingCtrlParam *)queue_head(
      &vcenc_instance->codingCtrl.codingCtrlQueue);
  job->pCodingCtrlParam = (void *)pEncCodingCtrlParam;
  if (pEncCodingCtrlParam) {
    if (pEncCodingCtrlParam->startPicCnt <
        0)  //whether latest parameter is set from current frame
      pEncCodingCtrlParam->startPicCnt = pEncIn->picture_cnt;
    pEncCodingCtrlParam->refCnt++;
  }
  EncRateCtrlParam *pEncRateCtrlParam = (EncRateCtrlParam *)queue_head(
      &vcenc_instance->rateCtrl.rateCtrlQueue);
  job->pRateCtrlParam = (void *)pEncRateCtrlParam;
  if (pEncRateCtrlParam) {
    if (pEncRateCtrlParam->startPicCnt <
        0)  //whether latest parameter is set from current frame
      pEncRateCtrlParam->startPicCnt = pEncIn->picture_cnt;
    pEncRateCtrlParam->refCnt++;
  }

  pthread_mutex_lock(&lookahead->job_mutex);
  if (pEncIn->bIsIDR && (lookahead->nextIdrCnt > pEncIn->picture_cnt ||
                         lookahead->nextIdrCnt < 0)) {
    // current next idr > picCnt or there is no next idr, reset next idr.
    lookahead->nextIdrCnt = pEncIn->picture_cnt;
  }
  queue_put(&lookahead->jobs, (struct node *)job);
  lookahead->enqueueJobcnt++;
  pthread_cond_signal(&lookahead->job_cond);
  pthread_mutex_unlock(&lookahead->job_mutex);

  return VCENC_OK;
}

VCEncLookaheadJob *GetLookaheadOutputByEncodingOrder(VCEncLookahead *p1_lookahead, VCEncLookahead *p2_lookahead,
                                                                struct vcenc_instance *vcenc_instance)
{
  u32 agop4to1 = vcenc_instance->pass2Agop4to1;
  VCEncLookaheadJob *output = NULL;
  VCEncLookaheadJob * job = NULL;
  p2_lookahead->agopPass1GopPicIdx = -1;
  vcenc_instance->pass1CutGopto1 = 0;

  job = (VCEncLookaheadJob *)queue_tail(&p1_lookahead->output);

  //if pass2 gop4->gop1, then should get frame by poc order rather than gop4 encoding order
  if (!(agop4to1 == 1 && job != NULL && job->frame.gopSize <= 4 && job->frame.gopSize > 1))
  {
    output = (VCEncLookaheadJob *)queue_get(&p1_lookahead->output);
    if (output != NULL && output->frame.gopSize == 1 && output->encIn.codingType != VCENC_INTRA_FRAME)
      vcenc_instance->pass1CutGopto1 = 1;
  }
  else if (p2_lookahead->lastPoc != UNUSED_POC)
  {
    while (job != NULL) {
    if ((job->frame.poc == (p2_lookahead->lastPoc + (p2_lookahead->lastGopPicIdx == 0 ? 1 : 2))))
    {
      output = job;
      queue_remove(&p1_lookahead->output, (struct node *)job);
      p2_lookahead->agopPass1GopPicIdx = output->encIn.gopPicIdx;
      output->encIn.gopPicIdx = 0;
      output->encIn.codingType = VCENC_PREDICTED_FRAME;
      output->frame.gopSize = 1;
      break;
    }

      job = (VCEncLookaheadJob *)((struct node *)job->next);
    }
  }

  return output;

}

/* Get cutree analyse result */
VCEncLookaheadJob *GetLookaheadOutput(VCEncLookahead *p2_lookahead,
                                      bool bFlush, struct vcenc_instance *vcenc_instance) {
  VCEncLookahead *p1_lookahead =
      &((struct vcenc_instance *)(p2_lookahead->priv_inst))->lookahead;
  struct cuTreeCtr *m_param =
      &(((struct vcenc_instance *)(p2_lookahead->priv_inst))->cuTreeCtl);

  if (bFlush) {
    /* When VCEncFlush */
    pthread_mutex_lock(&m_param->status_mutex);
    if (m_param->bStatus < THREAD_STATUS_LOOKAHEAD_FLUSH)
      m_param->bStatus = THREAD_STATUS_LOOKAHEAD_FLUSH;
    pthread_mutex_unlock(&m_param->status_mutex);
    pthread_cond_signal(&p2_lookahead->job_cond);
  }

  VCEncLookaheadJob *output = NULL;
  pthread_mutex_lock(&p1_lookahead->output_mutex);
  output = GetLookaheadOutputByEncodingOrder(p1_lookahead, p2_lookahead, vcenc_instance);

  pthread_mutex_lock(&m_param->status_mutex);
  THREAD_STATUS bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  //if cutree flush end, finish lookahead encode
  while (output == NULL && bStatus < THREAD_STATUS_CUTREE_FLUSH_END) {
    pthread_cond_wait(&p1_lookahead->output_cond, &p1_lookahead->output_mutex);
    output = GetLookaheadOutputByEncodingOrder(p1_lookahead, p2_lookahead, vcenc_instance);

    pthread_mutex_lock(&m_param->status_mutex);
    bStatus = m_param->bStatus;
    pthread_mutex_unlock(&m_param->status_mutex);
  }
  if (output == NULL) {
    pthread_mutex_unlock(&p1_lookahead->output_mutex);
    return NULL;
  }
  pthread_mutex_unlock(&p1_lookahead->output_mutex);
  if (output->status == VCENC_FRAME_READY) {
    /* collect gop info */
    i32 lastPoc = output->encIn.poc;
    i32 lastGopPicIdx = output->encIn.gopPicIdx;
    VCEncPictureCodingType lastCodingType = output->encIn.codingType;
    if (p2_lookahead->lastPoc != UNUSED_POC) {
      output->encIn.poc = p2_lookahead->lastPoc;
      output->encIn.gopPicIdx = p2_lookahead->lastGopPicIdx;
      output->encIn.codingType = p2_lookahead->lastCodingType;
    }
    p2_lookahead->lastPoc = lastPoc;
    p2_lookahead->lastGopPicIdx = lastGopPicIdx;
    p2_lookahead->lastCodingType = lastCodingType;
    /* gopConfig selection */
    output->encIn.gopConfig.pGopPicCfg =
        output->encIn.gopConfig.pGopPicCfgPass2;
  }
  return output;
}
/* Release roi map buffer after pass 2 encoding */
void ReleaseLookaheadPicture(VCEncLookahead *lookahead,
                             VCEncLookaheadJob *output, ptr_t inputbufBusAddr) {
  if (inputbufBusAddr != 0 &&
      (*(struct vcenc_instance *)(lookahead->priv_inst)).num_tile_columns > 1) {
    free(output->encIn.tileExtra);
  }

  if (output) {
    PutRoiMapBufferToBufferPool(
        &((struct vcenc_instance *)(lookahead->priv_inst))->cuTreeCtl,
        output->encIn.roiMapDeltaQpAddr);
    PutBufferToPool(lookahead->jobBufferPool, (void **)&output);
  }
}

/* Enqueue cutree analyse result for pass 2 use */
bool LookaheadEnqueueOutput(VCEncLookahead *lookahead,
                            VCEncLookaheadJob *output) {
  pthread_mutex_lock(&lookahead->output_mutex);
  if (output->status != VCENC_FRAME_READY &&
      output->status != VCENC_FRAME_ENQUEUE) {
    struct node *p;
    while ((p = queue_get(&lookahead->output)) != NULL) {
      PutBufferToPool(lookahead->jobBufferPool, (void **)&p);
    }
  }
  queue_put(&lookahead->output, (struct node *)output);
  pthread_cond_signal(&lookahead->output_cond);
  pthread_mutex_unlock(&lookahead->output_mutex);

  return HANTRO_TRUE;
}

VCEncLookaheadJob *findLookaheadJob(struct queue *jobQueue, int picture_cnt)
{
  VCEncLookaheadJob *job = NULL;
  job = (VCEncLookaheadJob *)queue_tail(jobQueue);

  /* get job from job queue */
  while (NULL != job) {
    if (picture_cnt == job->encIn.picture_cnt)
      break;
    job = (VCEncLookaheadJob *)job->next;
  }
  return job;
}
/*------------------------------------------------------------------------------
    Function name   : GetLookaheadJob
    Description     : pass1: get job to be handled according to given picture cnt
    Return type     : VCEncLookaheadJob *  job to be handled
    Argument        : VCEncLookahead *lookahead              [in/out]
    Argument        : const i32 picCnt                       [in]       picture cnt
    Argument        : i32* enqueueNum                        [out]      enqueue job count
------------------------------------------------------------------------------*/
static VCEncLookaheadJob *GetLookaheadJob(VCEncLookahead *lookahead,
                                          VCEncIn *pEncIn, i32 *enqueueNum,
                                          VCEncPicConfig *lastPicCfg,
                                          i32 *pNextGopSize,
                                          i32 *pGopSizeFromUser,
                                          VCEncLookaheadJob **pendingJob) {
  struct vcenc_instance *pass1Instance =
      (struct vcenc_instance *)(lookahead->priv_inst);
  struct cuTreeCtr *m_param = &(pass1Instance->cuTreeCtl);

  pthread_mutex_lock(&lookahead->job_mutex);

  VCEncLookaheadJob *job = NULL, *prevJob = NULL;
  job = prevJob = (VCEncLookaheadJob *)queue_tail(&lookahead->jobs);

  pthread_mutex_lock(&m_param->status_mutex);
  THREAD_STATUS bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  while ((NULL == job && bStatus < THREAD_STATUS_LOOKAHEAD_FLUSH) ||
         bStatus == THREAD_STATUS_MAIN_STOP) {
    /* if status is STOP, then remove the job to buffer pool if it's not empty*/
    if (bStatus == THREAD_STATUS_MAIN_STOP) {
      lookaheadClear(lookahead, pendingJob);
      pthread_mutex_unlock(&lookahead->stop_mutex);
      lookahead->bStop = 1;
      pthread_cond_signal(&lookahead->stop_cond);
      pthread_mutex_unlock(&lookahead->stop_mutex);
    }

    pthread_cond_wait(&lookahead->job_cond, &lookahead->job_mutex);
    job = prevJob = (VCEncLookaheadJob *)queue_tail(&lookahead->jobs);

    pthread_mutex_lock(&m_param->status_mutex);
    bStatus = m_param->bStatus;
    pthread_mutex_unlock(&m_param->status_mutex);
  }

  if (NULL != job) {
    *pGopSizeFromUser = job->encIn.gopSize;
  }
  VCEncLookaheadJob *nextJob = (VCEncLookaheadJob *)queue_tail(&lookahead->jobs);
  bool fpsChange = false;
  VCEncRateCtrl *pRateCtrl = NULL;
  if(nextJob) {
    pRateCtrl = &((EncRateCtrlParam *)nextJob->pRateCtrlParam)->encRateCtrl;
    VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(pEncIn->gopConfig));
    int next_picture_cnt = lastPicCfg->picture_cnt - gopCfg->pGopPicCfg[lastPicCfg->gopPicIdx].poc + lastPicCfg->gopSize + 1;
    fpsChange = pEncIn->gopPicIdx == 0 && checkFpsUpdate((struct vcenc_instance *)(lookahead->priv_inst), next_picture_cnt);
  }
  /*if there's an IDR frame before next frame to encode or fps change
      or gopSize specified by user is not equal to NextGopSize, refind next picture */
  if ((lookahead->nextIdrCnt > 0 &&
       lookahead->nextIdrCnt <= pEncIn->picture_cnt && !m_param->enableLeadingPictures) ||
      (NULL != job && 0 != job->encIn.picture_cnt && 0 != *pGopSizeFromUser &&
       *pGopSizeFromUser != *pNextGopSize) || fpsChange) {
    if (0 != *pGopSizeFromUser && *pGopSizeFromUser != *pNextGopSize)
      *pNextGopSize = *pGopSizeFromUser;

    SetPicCfgToEncIn(lastPicCfg, pEncIn);
    //find next picture
    FindNextPic(lookahead->priv_inst, pEncIn, *pNextGopSize,
                pEncIn->gopConfig.gopCfgOffset, lookahead->nextIdrCnt, fpsChange);
    if(fpsChange) {
      nextJob = findLookaheadJob(&lookahead->jobs, pEncIn->picture_cnt);
      EncFpsUpdate((struct vcenc_instance *)(lookahead->priv_inst), &lookahead->jobs, nextJob->pRateCtrlParam, pEncIn);
      m_param->fpsNum = pRateCtrl->frameRateNum;
      m_param->fpsDenom = pRateCtrl->frameRateDenom;
    }
  }

  /*get job from job queue*/
  while (NULL != job) {
    if (pEncIn->picture_cnt == job->encIn.picture_cnt) {
      queue_remove(&lookahead->jobs, (struct node *)job);
      break;
    }
    prevJob = job;
    job = (VCEncLookaheadJob *)job->next;
  }

  pthread_mutex_lock(&m_param->status_mutex);
  bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  //not find the job to be processed, wait for new enqueue job or stop happens then cancel jobs
  while ((job == NULL && bStatus < THREAD_STATUS_LOOKAHEAD_FLUSH) ||
         bStatus == THREAD_STATUS_MAIN_STOP) {
    /* if status is STOP, then remove the job to buffer pool if it's not empty*/
    if (bStatus == THREAD_STATUS_MAIN_STOP) {
      // if find job of picture_cnt, then put to buffer pool
      if (job != NULL) PutBufferToPool(lookahead->jobBufferPool, (void **)&job);

      // return all remaining jobs in queue to buffer pool if there exist
      lookaheadClear(lookahead, pendingJob);

      pthread_mutex_unlock(&lookahead->stop_mutex);
      lookahead->bStop = 1;
      pthread_cond_signal(&lookahead->stop_cond);
      pthread_mutex_unlock(&lookahead->stop_mutex);
    }

    pthread_cond_wait(&lookahead->job_cond, &lookahead->job_mutex);
    if (NULL == prevJob)
      prevJob = job = (VCEncLookaheadJob *)queue_tail(&lookahead->jobs);
    else
      job = (VCEncLookaheadJob *)prevJob->next;

    if (bStatus == THREAD_STATUS_MAIN_STOP) {
      prevJob = job = NULL;
      pthread_mutex_lock(&m_param->status_mutex);
      bStatus = m_param->bStatus;
      pthread_mutex_unlock(&m_param->status_mutex);
      continue;
    }

    //if there's an IDR frame before next frame to encode, refind next picture
    if (lookahead->nextIdrCnt > 0 &&
        lookahead->nextIdrCnt <= pEncIn->picture_cnt && !m_param->enableLeadingPictures) {
      SetPicCfgToEncIn(lastPicCfg, pEncIn);
      //find next picture
      FindNextPic(lookahead->priv_inst, pEncIn, *pNextGopSize,
                  pEncIn->gopConfig.gopCfgOffset, lookahead->nextIdrCnt, false);
      //if next to be encoded frame's picture count is less than current job's picture count,
      // reset current job to find the next to be encoded frame
      if (pEncIn->picture_cnt < job->encIn.picture_cnt)
        job = prevJob = (VCEncLookaheadJob *)queue_tail(&lookahead->jobs);
    }

    while (NULL != job) {
      if (pEncIn->picture_cnt == job->encIn.picture_cnt) {
        queue_remove(&lookahead->jobs, (struct node *)job);
        break;
      }
      prevJob = job;
      job = (VCEncLookaheadJob *)job->next;
    }

    pthread_mutex_lock(&m_param->status_mutex);
    bStatus = m_param->bStatus;
    pthread_mutex_unlock(&m_param->status_mutex);
  }

end:
  *enqueueNum = lookahead->enqueueJobcnt;
  pthread_mutex_unlock(&lookahead->job_mutex);
  return job;
}

/*------------------------------------------------------------------------------
    Function name   : GopDecision
    Description     : pass1: make agop decision
    Return type     : void
    Argument        : struct AGopInfo** gopInfoAddr              [in/out]   gop informations
    Argument        : const u32 nGopInfo                         [int]      current gopinfo count
------------------------------------------------------------------------------*/
static void GopDecision(struct AGopInfo **gopInfoAddr, const u32 nGopInfo) {
  if (nGopInfo < 8) return;

  i32 i;
  u32 th4 = AGOP_MOTION_TH * 4;
  u32 th8 = AGOP_MOTION_TH * 8;
  i32 aGopSize = 0;

  /* convert gop4 to gop8  */
  if (IS_CODING_TYPE_P(gopInfoAddr[nGopInfo - 5]->sliceType) &&
      (gopInfoAddr[nGopInfo - 5]->gopSize == 4) &&
      (gopInfoAddr[nGopInfo - 5]->aGopSize == 0) &&
      IS_CODING_TYPE_P(gopInfoAddr[nGopInfo - 1]->sliceType) &&
      (gopInfoAddr[nGopInfo - 1]->gopSize == 4) &&
      (gopInfoAddr[nGopInfo - 1]->aGopSize == 0)) {
    if ((gopInfoAddr[nGopInfo - 5]->motionScore[0][0] <= th4) &&
        (gopInfoAddr[nGopInfo - 5]->motionScore[0][1] <= th4) &&
        (gopInfoAddr[nGopInfo - 1]->motionScore[0][0] <= th4) &&
        (gopInfoAddr[nGopInfo - 1]->motionScore[0][1] <= th4)) {
      aGopSize = 8;
    }
  }
  /* convert gop8 to gop4  */
  else if (IS_CODING_TYPE_P(gopInfoAddr[nGopInfo - 1]->sliceType) &&
           (gopInfoAddr[nGopInfo - 1]->gopSize == 8)) {
    if ((gopInfoAddr[nGopInfo - 1]->motionScore[0][0] > th8) ||
        (gopInfoAddr[nGopInfo - 1]->motionScore[0][1] > th8)) {
      aGopSize = 4;
    }
  }

  gopInfoAddr[0]->aGopSize = aGopSize;

  return;
}
/* lookahead thread performs pass 1 encoding, passing result to cutree thread */
void *LookaheadThread(void *arg) {
  VCEncRet ret = VCENC_OK;
  VCEncLookaheadJob *job = NULL;
  VCEncLookahead *p2_lookahead = (VCEncLookahead *)arg;
  VCEncLookahead *p1_lookahead =
      &((struct vcenc_instance *)(p2_lookahead->priv_inst))->lookahead;
  struct cuTreeCtr *m_param =
      &(((struct vcenc_instance *)(p2_lookahead->priv_inst))->cuTreeCtl);
  struct vcenc_instance *pass1Instance =
      (struct vcenc_instance *)(p2_lookahead->priv_inst);
  VCEncLookaheadJob output;
  /* just a temp storage to flush a maximun of MAX_CORE_NUM - 1 times */
  VCEncLookaheadJob *pendingJob[MAX_CORE_NUM - 1] = {NULL};
  VCEncIn *pEncIn = &p2_lookahead->encIn;
  m_param->lastGopSize = m_param->latestGopSize = pEncIn->gopSize;
  i32 nextGopSize = pEncIn->gopSize;
  i32 gopSizeFromUser = 0;
  void *agopInfoBuf = NULL;
  VCEncPicConfig lastPicCfg;
  memset(&lastPicCfg, 0, sizeof(VCEncPicConfig));
  lastPicCfg.poc = -1;
  i32 enqueueNum = 0;
  u32 pendingJobIdx = 0;
  THREAD_STATUS bStatus = THREAD_STATUS_OK;
  ret = InitBufferPool(&agopInfoBuf, sizeof(struct AGopInfo), MAX_AGOPINFO_NUM);
  if (VCENC_OK != ret) goto end;

  while (HANTRO_TRUE) {
    pthread_mutex_lock(&m_param->status_mutex);
    bStatus = m_param->bStatus;
    pthread_mutex_unlock(&m_param->status_mutex);
    if (bStatus >= THREAD_STATUS_LOOKAHEAD_ERROR)  //error handle
      break;
    //get job from buffer pool according to next encode picCnt
    /* the job has been prepared by AddPictureToLookahead in VCEncStrmEncodeExt when pass2 */
    job = GetLookaheadJob(p2_lookahead, pEncIn, &enqueueNum, &lastPicCfg,
                          &nextGopSize, &gopSizeFromUser, &pendingJob[0]);
    if (NULL == job) {
      u64 numer, denom;
      denom = (u64)pEncIn->gopConfig.inputRateNumer *
              (u64)pEncIn->gopConfig.outputRateDenom;
      numer = (u64)pEncIn->gopConfig.inputRateDenom *
              (u64)pEncIn->gopConfig.outputRateNumer;
      i32 bNornalEnd =
          (((pEncIn->gopConfig.lastPic - pEncIn->gopConfig.firstPic + 1) *
            numer / denom) <= enqueueNum)
              ? 1
              : 0;
      if (bNornalEnd)  //normal end
        break;
      else {
        if (lastPicCfg.poc == -1) {
          ret = VCENC_ERROR;
          goto end;
        }
        //memcpy(pEncInFor1pass, &lastEncIn, sizeof(VCEncIn));
        SetPicCfgToEncIn(&lastPicCfg, pEncIn);
        pEncIn->gopConfig.lastPic =
            pEncIn->gopConfig.firstPic + enqueueNum * denom / numer - 1;
        //find next picture
        FindNextPic(p2_lookahead->priv_inst, pEncIn, nextGopSize,
                    pEncIn->gopConfig.gopCfgOffset, p2_lookahead->nextIdrCnt, false);
        continue;
      }
    } else {
      //set picture configs to job
      pEncIn->picture_cnt = job->encIn.picture_cnt;
      SetPictureCfgToJob(pEncIn, &job->encIn, pass1Instance->gdrDuration);
      pthread_mutex_lock(&p2_lookahead->job_mutex);
      if (job->encIn.bIsIDR) {
        //updata next idr frame count
        i32 nextDefaultIdrCnt =
            pEncIn->picture_cnt + pEncIn->gopConfig.idr_interval;
        i32 nextForceIdrCnt = FindNextForceIdr(&p2_lookahead->jobs);
        i32 curIdrCnt = p2_lookahead->nextIdrCnt;
        if (pEncIn->gopConfig.idr_interval > 0)  // exist default idr interval
        {
          if (nextForceIdrCnt > curIdrCnt &&
              nextForceIdrCnt < nextDefaultIdrCnt)
            p2_lookahead->nextIdrCnt = nextForceIdrCnt;
          else
            p2_lookahead->nextIdrCnt = nextDefaultIdrCnt;
        } else  // not exist default idr interval
        {
          if (nextForceIdrCnt > curIdrCnt)
            p2_lookahead->nextIdrCnt = nextForceIdrCnt;
          else
            p2_lookahead->nextIdrCnt = -1;
        }
      }
      i32 nextIdrCnt = p2_lookahead->nextIdrCnt;
      pthread_mutex_unlock(&p2_lookahead->job_mutex);
      //update coding ctrl for pass1
      EncUpdateCodingCtrlForPass1(
          p2_lookahead->priv_inst,
          (EncCodingCtrlParam *)(job->pCodingCtrlParam));
      //update rate ctrl for pass1
      EncUpdateRateCtrlParam(
          (struct vcenc_instance *)p2_lookahead->priv_inst,
          (EncRateCtrlParam *)(job->pRateCtrlParam),
          &job->encIn);
      //pass1 encode
      /* every job has been used to encode, just there are a maximum of MAX_CORE_NUM - 1 jobs don't wait irq */
      job->status = VCEncStrmEncode(p2_lookahead->priv_inst, &job->encIn,
                                    &job->encOut, NULL, NULL);

      if (job->status == VCENC_FRAME_READY) {
        ASSERT(job->encOut.codingType != VCENC_NOTCODED_FRAME);
#ifdef TEST_DATA
        EncTraceCuInformation(p2_lookahead->priv_inst, &job->encOut,
                              job->encOut.picture_cnt, job->encIn.poc);
#endif
        //make gop decision and add job to cutree
        ret = cuTreeAddFrame(p2_lookahead->priv_inst, job, agopInfoBuf,
                             0 != gopSizeFromUser);
        if (ret == VCENC_ERROR) goto end;
      } else {
        if (job->status == VCENC_FRAME_ENQUEUE) /* for multi-core flush */
        {
          ASSERT(pendingJobIdx < MAX_CORE_NUM - 1);
          pendingJob[pendingJobIdx++] = job;
        } else {
          p2_lookahead->status = p1_lookahead->status = job->status;
          LookaheadEnqueueOutput(p1_lookahead, job);
          ret = job->status;
          goto end;
        }
      }

      if (m_param->latestGopSize && 0 == gopSizeFromUser)
        nextGopSize = m_param->latestGopSize;
      //save last picture config
      SavePicCfg(pEncIn, &lastPicCfg);
      VCEncLookaheadJob *nextJob = (VCEncLookaheadJob *)queue_tail(&p2_lookahead->jobs);
      bool fpsChange = false;
      VCEncRateCtrl *pRateCtrl = NULL;
      if(nextJob)
      {
        pRateCtrl = &((EncRateCtrlParam *)nextJob->pRateCtrlParam)->encRateCtrl;
        int nextGopPicIdx = (pEncIn->gopPicIdx + 1) % pEncIn->gopSize;
        VCEncGopConfig *gopCfg = (VCEncGopConfig *)(&(pEncIn->gopConfig));
        int next_picture_cnt = pEncIn->picture_cnt - gopCfg->pGopPicCfg[pEncIn->gopPicIdx].poc + pEncIn->gopSize + 1;
        fpsChange = (nextGopPicIdx == 0 && checkFpsUpdate((struct vcenc_instance *)(p2_lookahead->priv_inst), next_picture_cnt));
      }
      //calculate next to-be-encoded picture count
      FindNextPic(p2_lookahead->priv_inst, pEncIn, nextGopSize,
                  pEncIn->gopConfig.gopCfgOffset, nextIdrCnt, false);
      if(fpsChange && pEncIn->bIsIDR) {
      }
      if(fpsChange) {
        nextJob = findLookaheadJob(&p2_lookahead->jobs, pEncIn->picture_cnt);
        if(nextJob) {
          EncFpsUpdate((struct vcenc_instance *)(p2_lookahead->priv_inst), &p2_lookahead->jobs, nextJob->pRateCtrlParam, pEncIn);
          m_param->fpsNum = pRateCtrl->frameRateNum;
          m_param->fpsDenom = pRateCtrl->frameRateDenom;
        }
      }
    }
  }

  pthread_mutex_lock(&m_param->status_mutex);
  bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  pendingJobIdx = 0;
  while (((struct vcenc_instance *)(p2_lookahead->priv_inst))->reservedCore >
             0 &&
         bStatus != THREAD_STATUS_MAIN_STOP) {
    ASSERT(pendingJobIdx < MAX_CORE_NUM - 1);
	if (pendingJob[pendingJobIdx]) {
      job = pendingJob[pendingJobIdx++];
      /* job->encIn and job->encOut will be updated VCEncStrmGetOutput when pass1 */
      job->status = VCEncStrmGetOutput(p2_lookahead->priv_inst, &job->encIn,
                                        &job->encOut, NULL, NULL);
      ret = job->status;
      if (job->status == VCENC_FRAME_READY) {
        ASSERT(job->encOut.codingType != VCENC_NOTCODED_FRAME);
        ret = cuTreeAddFrame(p2_lookahead->priv_inst, job, agopInfoBuf,
                             0 != gopSizeFromUser);
      } else {
        p2_lookahead->status = p1_lookahead->status = job->status;
        LookaheadEnqueueOutput(p1_lookahead, job);
      }
	}
  }

end:
  ReleaseBufferPool(&agopInfoBuf);
  /* Set error for all thread
     Or set lookahead flush */
  pthread_mutex_lock(&m_param->status_mutex);
  if (ret < VCENC_OK)
    m_param->bStatus = THREAD_STATUS_LOOKAHEAD_ERROR;
  else if (m_param->bStatus < THREAD_STATUS_CUTREE_FLUSH &&
           m_param->bStatus !=
               THREAD_STATUS_MAIN_STOP)  //if status is STOP, just keep it
    m_param->bStatus = THREAD_STATUS_CUTREE_FLUSH;
  pthread_mutex_unlock(&m_param->status_mutex);

  /* notify encoding thread it's stopped */
  pthread_mutex_unlock(&p2_lookahead->stop_mutex);
  p2_lookahead->bStop = 1;
  pthread_cond_signal(&p2_lookahead->stop_cond);
  pthread_mutex_unlock(&p2_lookahead->stop_mutex);

  cuTreeFlush(&((struct vcenc_instance *)p2_lookahead->priv_inst)->cuTreeCtl);
  return NULL;
}

/* cutree analyse thread. */
void *cuTreeThread(void *arg) {
  struct cuTreeCtr *m_param = (struct cuTreeCtr *)arg;
  struct Lowres *cur_frame = NULL;
  u32 remove_frame_happened = HANTRO_FALSE;
  i32 ret = OK;

  //get bstatus.
  pthread_mutex_lock(&m_param->status_mutex);
  THREAD_STATUS bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  while (bStatus < THREAD_STATUS_CUTREE_FLUSH || m_param->job_cnt > 0) {
    pthread_mutex_lock(&m_param->cutree_mutex);

    //update bstatus.
    pthread_mutex_lock(&m_param->status_mutex);
    bStatus = m_param->bStatus;
    pthread_mutex_unlock(&m_param->status_mutex);

    while ((bStatus < THREAD_STATUS_CUTREE_FLUSH && m_param->job_cnt == 0) ||
           bStatus == THREAD_STATUS_MAIN_STOP) {
      if (bStatus == THREAD_STATUS_MAIN_STOP) {
        cuTreeClear(m_param);
        pthread_mutex_lock(&m_param->stop_mutex);
        m_param->bStop = 1;
        pthread_cond_signal(&m_param->stop_cond);
        pthread_mutex_unlock(&m_param->stop_mutex);
      }

      pthread_cond_wait(&m_param->cutree_cond, &m_param->cutree_mutex);

      //update bstatus.
      pthread_mutex_lock(&m_param->status_mutex);
      bStatus = m_param->bStatus;
      pthread_mutex_unlock(&m_param->status_mutex);
    }

    if (bStatus >= THREAD_STATUS_LOOKAHEAD_ERROR) {
      pthread_mutex_unlock(&m_param->cutree_mutex);
      goto end;
    }
    if (m_param->job_cnt > 0) {
      cur_frame = (struct Lowres *)queue_get(&m_param->jobs);
	  if (cur_frame == NULL) {
	  	pthread_mutex_unlock(&m_param->cutree_mutex);
	  	return NULL;
	  }
      m_param->job_cnt--;

      pthread_mutex_lock(&m_param->status_mutex);
      bStatus = m_param->bStatus;
      pthread_mutex_unlock(&m_param->status_mutex);

      if (bStatus == THREAD_STATUS_MAIN_STOP) {
        PutBufferToPool(
            ((struct vcenc_instance *)m_param->pEncInst)->cutreeJobBufferPool,
            (void **)&cur_frame);
        pthread_mutex_unlock(&m_param->cutree_mutex);
        continue;
      }

      pthread_mutex_unlock(&m_param->cutree_mutex);
      ret = cuTreeHandleInputFrame(cur_frame, m_param);
      if (ret != OK) goto end;
    } else
      pthread_mutex_unlock(&m_param->cutree_mutex);
  }
  // Flush remaining frames

  struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;
  pthread_mutex_lock(&m_param->status_mutex);
  bStatus = m_param->bStatus;
  pthread_mutex_unlock(&m_param->status_mutex);
  if (bStatus <= THREAD_STATUS_CUTREE_FLUSH &&
      bStatus != THREAD_STATUS_MAIN_STOP) {
    while (m_param->nLookaheadFrames > 1) {
      ret = process_one_frame(m_param);
      if (ret) goto end;
    }
    if (m_param->nLookaheadFrames == 1 &&
        m_param->lookaheadFrames[0]->sliceType == VCENC_FRAME_TYPE_IDR && ret == OK) {
      ret = process_one_frame(m_param);
      if (ret) goto end;
    }
  }

end:
  if (bStatus >= THREAD_STATUS_LOOKAHEAD_ERROR) {
    //when error happend, need to clear cutree job queue
    while (m_param->job_cnt > 0) {
      pthread_mutex_lock(&m_param->cutree_mutex);
      cur_frame = (struct Lowres *)queue_get(&m_param->jobs);
      m_param->job_cnt--;
      pthread_mutex_unlock(&m_param->cutree_mutex);

      void *cutreeJobBufferPool =
          ((struct vcenc_instance *)m_param->pEncInst)->cutreeJobBufferPool;
      void *jobBufferPool =
          ((struct vcenc_instance *)m_param->pEncInst)->lookahead.jobBufferPool;
      releaseFrame(cur_frame, cutreeJobBufferPool, jobBufferPool);
    }
  }

  ASSERT(m_param->job_cnt <= 0);
  while (m_param->nLookaheadFrames > 0) {
    remove_one_frame(m_param);
    remove_frame_happened = HANTRO_TRUE;
  }

  /* Set error for all thread
     Or set cutree flush for pass 2 to stop wait agop */
  pthread_mutex_lock(&m_param->status_mutex);
  if (ret != OK)
    m_param->bStatus = THREAD_STATUS_CUTREE_ERROR;
  else if (m_param->bStatus < THREAD_STATUS_CUTREE_FLUSH_END &&
           m_param->bStatus != THREAD_STATUS_MAIN_STOP)
    m_param->bStatus = THREAD_STATUS_CUTREE_FLUSH_END;
  pthread_mutex_unlock(&m_param->status_mutex);

  /* because some frames are removed, need to signal such message to main thread, that may wait for
     CU info analysis result */
  if (remove_frame_happened == HANTRO_TRUE) {
    struct vcenc_instance *enc = (struct vcenc_instance *)m_param->pEncInst;
    pthread_mutex_lock(&enc->lookahead.output_mutex);
    pthread_cond_signal(&enc->lookahead.output_cond);
    pthread_mutex_unlock(&enc->lookahead.output_mutex);
  }

  /* notify encoding thread it's stopped */
  pthread_mutex_lock(&m_param->stop_mutex);
  m_param->bStop = 1;
  pthread_cond_signal(&m_param->stop_cond);
  pthread_mutex_unlock(&m_param->stop_mutex);

  return NULL;
}

/* Start lookahead (pass1) thread. */
VCEncRet StartLookaheadThread(VCEncLookahead *lookahead) {
  VCEncLookahead *lookahead2 =
      &((struct vcenc_instance *)(lookahead->priv_inst))->lookahead;

  pthread_attr_t attr;
  pthread_t *tid_lookahead = (pthread_t *)malloc(sizeof(pthread_t));
  pthread_mutexattr_t mutexattr;
  pthread_condattr_t condattr;
  if (tid_lookahead == NULL)
  	return VCENC_ERROR;

  queue_init(&lookahead->jobs);
  queue_init(&lookahead2->output);
  lookahead->lastPoc = UNUSED_POC;
  lookahead->last_idr_picture_cnt = lookahead->picture_cnt = 0;

  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&lookahead->job_mutex, &mutexattr);
  pthread_mutex_init(&lookahead2->output_mutex, &mutexattr);
  pthread_mutex_init(&lookahead->stop_mutex, &mutexattr);
  pthread_mutexattr_destroy(&mutexattr);
  pthread_condattr_init(&condattr);
  pthread_cond_init(&lookahead->job_cond, &condattr);
  pthread_cond_init(&lookahead2->output_cond, &condattr);
  pthread_cond_init(&lookahead->stop_cond, &condattr);
  pthread_condattr_destroy(&condattr);
  pthread_attr_init(&attr);
  pthread_create(tid_lookahead, &attr, &LookaheadThread, lookahead);
  pthread_attr_destroy(&attr);

  lookahead->enqueueJobcnt = 0;
  lookahead->nextIdrCnt = 0;
  lookahead->tid_lookahead = tid_lookahead;
  lookahead->bFlush = HANTRO_FALSE;
  lookahead->status = lookahead2->status = VCENC_OK;
  return tid_lookahead ? VCENC_OK : VCENC_ERROR;
}
/* Start cutree analyse thread. */
VCEncRet StartCuTreeThread(struct cuTreeCtr *m_param) {
  pthread_attr_t attr;
  pthread_t *tid_cutree = (pthread_t *)malloc(sizeof(pthread_t));
  pthread_mutexattr_t mutexattr;
  pthread_condattr_t condattr;

  if (tid_cutree == NULL)
  	return VCENC_ERROR;

  pthread_mutexattr_init(&mutexattr);
  pthread_mutex_init(&m_param->cutree_mutex, &mutexattr);
  pthread_mutex_init(&m_param->roibuf_mutex, &mutexattr);
  pthread_mutex_init(&m_param->cuinfobuf_mutex, &mutexattr);
  pthread_mutex_init(&m_param->agop_mutex, &mutexattr);
  pthread_mutex_init(&m_param->status_mutex, &mutexattr);
  pthread_mutex_init(&m_param->stop_mutex, &mutexattr);
  pthread_mutexattr_destroy(&mutexattr);
  pthread_condattr_init(&condattr);
  pthread_cond_init(&m_param->cutree_cond, &condattr);
  pthread_cond_init(&m_param->roibuf_cond, &condattr);
  pthread_cond_init(&m_param->cuinfobuf_cond, &condattr);
  m_param->cuInfoToRead = 0;
  pthread_cond_init(&m_param->agop_cond, &condattr);
  pthread_cond_init(&m_param->stop_cond, &condattr);
  pthread_condattr_destroy(&condattr);
  pthread_attr_init(&attr);

  m_param->bStatus = THREAD_STATUS_OK;
  pthread_create(tid_cutree, &attr, &cuTreeThread, m_param);
  pthread_attr_destroy(&attr);

  m_param->tid_cutree = tid_cutree;
  return tid_cutree ? VCENC_OK : VCENC_ERROR;
}

void lookaheadFlush(VCEncLookahead *p2_lookahead,
                    VCEncLookahead *p1_lookahead2) {
  pthread_mutex_lock(&p2_lookahead->job_mutex);
  pthread_cond_signal(&p2_lookahead->job_cond);
  pthread_mutex_unlock(&p2_lookahead->job_mutex);

  pthread_mutex_lock(&p1_lookahead2->output_mutex);
  pthread_cond_signal(&p1_lookahead2->output_cond);
  pthread_mutex_unlock(&p1_lookahead2->output_mutex);
}

VCEncRet lookaheadClear(VCEncLookahead *lookahead,
                        VCEncLookaheadJob **pendingJob) {
  VCEncLookaheadJob *job = NULL, *prevJob = NULL;
  job = prevJob = (VCEncLookaheadJob *)queue_tail(&lookahead->jobs);
  struct vcenc_instance *vcenc_instance =
      (struct vcenc_instance *)lookahead->priv_inst;
  asicData_s *asic = &vcenc_instance->asic;
  u32 reservedCore = vcenc_instance->reservedCore;
  u32 jobIdx = 0;

  /* release jobs for multi-core pending jobs*/
  while (reservedCore > 0 && !asic->regs.bVCMDEnable) {
    PutBufferToPool(lookahead->jobBufferPool, (void **)&pendingJob[jobIdx++]);
    reservedCore--;
  }

  /* wait for encoding job to finish*/
  VCEncClear(lookahead->priv_inst);

  /* remove the job in queue to buffer pool if it's not empty*/
  while (job != NULL) {
    prevJob = job;
    queue_remove(&lookahead->jobs, (struct node *)job);
    PutBufferToPool(lookahead->jobBufferPool, (void **)&job);
    job = (VCEncLookaheadJob *)prevJob->next;
  }

  /* reset lookahead status*/
  lookahead->lastPoc = UNUSED_POC;
  lookahead->last_idr_picture_cnt = lookahead->picture_cnt = 0;

  lookahead->enqueueJobcnt = 0;
  lookahead->nextIdrCnt = 0;

  lookahead->bFlush = HANTRO_FALSE;
  lookahead->status = VCENC_OK;

  return VCENC_OK;
}

VCEncRet cuTreeClear(struct cuTreeCtr *m_param) {
  i32 i;
  struct Lowres *cur_frame = NULL;

  //clear job queue
  while (m_param->job_cnt > 0) {
    cur_frame = (struct Lowres *)queue_get(&m_param->jobs);
    PutBufferToPool(
        ((struct vcenc_instance *)m_param->pEncInst)->cutreeJobBufferPool,
        (void **)&cur_frame);
    m_param->job_cnt--;
  }

  //clear lookahead frame buffer
  while (m_param->nLookaheadFrames > 0) {
    remove_one_frame(m_param);
  }

  //reset cutree parameters
  m_param->nLookaheadFrames = 0;
  m_param->lastGopEnd = 0;
  m_param->lookaheadFrames = m_param->lookaheadFramesBase;
  m_param->frameNum = 0;
  for (i32 i = 0; i < 4; i++) {
    m_param->FrameTypeNum[i] = 0;
    m_param->costAvgInt[i] = 0;
    m_param->FrameNumGop[i] = 0;
    m_param->costGopInt[i] = 0;
  }

  m_param->latestGopSize = 0;

  /* Init segment qps */
  m_param->segmentCountEnable =
      IS_VP9(((struct vcenc_instance *)m_param->pEncInst)->codecFormat);
  for (i = 0; i < MAX_SEGMENTS; i++) {
    m_param->segment_qp[i] = segment_delta_qp[i];
  }

  m_param->job_cnt = 0;
  m_param->output_cnt = 0;
  m_param->total_frames = 0;

  return VCENC_OK;
}

/* Stop lookahead (pass1) thread. */
VCEncRet StopLookaheadThread(VCEncLookahead *p2_lookahead, u8 error) {
  if (!p2_lookahead->tid_lookahead) return VCENC_OK;
  struct cuTreeCtr *m_param =
      &(((struct vcenc_instance *)(p2_lookahead->priv_inst))->cuTreeCtl);

  pthread_mutex_lock(&m_param->status_mutex);
  if (error)
    m_param->bStatus = THREAD_STATUS_MAIN_ERROR;
  else if (m_param->bStatus <= THREAD_STATUS_CUTREE_FLUSH)
    m_param->bStatus = THREAD_STATUS_MAIN_STOP;
  pthread_mutex_unlock(&m_param->status_mutex);

  pthread_mutex_lock(&p2_lookahead->job_mutex);
  pthread_cond_signal(&p2_lookahead->job_cond);
  pthread_mutex_unlock(&p2_lookahead->job_mutex);

  /* wait for lookahead thread stop*/
  pthread_mutex_lock(&p2_lookahead->stop_mutex);

  while (p2_lookahead->bStop == 0) {
    pthread_cond_wait(&p2_lookahead->stop_cond, &p2_lookahead->stop_mutex);
  }

  pthread_mutex_unlock(&p2_lookahead->stop_mutex);
  return VCENC_OK;
}

/* Stop cutree analyse thread. */
VCEncRet StopCuTreeThread(struct cuTreeCtr *m_param, u8 error) {
  if (!m_param->tid_cutree) return VCENC_OK;

  pthread_mutex_lock(&m_param->status_mutex);
  if (error)
    m_param->bStatus = THREAD_STATUS_MAIN_ERROR;
  else if (m_param->bStatus <= THREAD_STATUS_CUTREE_FLUSH)
    m_param->bStatus = THREAD_STATUS_MAIN_STOP;
  pthread_mutex_unlock(&m_param->status_mutex);

  pthread_mutex_lock(&m_param->cutree_mutex);
  pthread_cond_signal(&m_param->cutree_cond);
  pthread_mutex_unlock(&m_param->cutree_mutex);

  /* wait for cutree thread stop*/
  pthread_mutex_lock(&m_param->stop_mutex);

  while (m_param->bStop == 0) {
    pthread_cond_wait(&m_param->stop_cond, &m_param->stop_mutex);
  }

  pthread_mutex_unlock(&m_param->stop_mutex);

  return VCENC_OK;
}

/* Terminate lookahead (pass1) thread. */
VCEncRet TerminateLookaheadThread(VCEncLookahead *p2_lookahead, u8 error) {
  // destroy lookahead thread
  if (!p2_lookahead->tid_lookahead) return VCENC_OK;
  struct cuTreeCtr *m_param =
      &(((struct vcenc_instance *)(p2_lookahead->priv_inst))->cuTreeCtl);

  pthread_mutex_lock(&m_param->status_mutex);
  if (error)
    m_param->bStatus = THREAD_STATUS_MAIN_ERROR;
  else if (m_param->bStatus < THREAD_STATUS_CUTREE_FLUSH)
    m_param->bStatus = THREAD_STATUS_CUTREE_FLUSH;
  pthread_mutex_unlock(&m_param->status_mutex);

  VCEncLookahead *p1_lookahead =
      &((struct vcenc_instance *)(p2_lookahead->priv_inst))->lookahead;
  lookaheadFlush(p2_lookahead, p1_lookahead);

  /* Wait thread finish */
  if (p2_lookahead->tid_lookahead) {
    pthread_join(*p2_lookahead->tid_lookahead, NULL);
    VCENC_FREE(p2_lookahead->tid_lookahead);
    p2_lookahead->tid_lookahead = NULL;
  }

  return VCENC_OK;
}

/* Terminate cutree analyse thread. */
VCEncRet TerminateCuTreeThread(struct cuTreeCtr *m_param, u8 error) {
  if (!m_param->tid_cutree) return VCENC_OK;

  pthread_mutex_lock(&m_param->status_mutex);
  if (error)
    m_param->bStatus = THREAD_STATUS_MAIN_ERROR;
  else if (m_param->bStatus < THREAD_STATUS_CUTREE_FLUSH)
    m_param->bStatus = THREAD_STATUS_CUTREE_FLUSH;

  pthread_mutex_unlock(&m_param->status_mutex);
  cuTreeFlush(m_param);

  /* Wait thread finish */
  if (m_param->tid_cutree) {
    pthread_join(*m_param->tid_cutree, NULL);
    VCENC_FREE(m_param->tid_cutree);
    m_param->tid_cutree = NULL;
  }

  return VCENC_OK;
}
VCEncRet waitCuInfoBufPass1(struct vcenc_instance *vcenc_instance) {
  struct cuTreeCtr *m_param = &vcenc_instance->cuTreeCtl;
  pthread_mutex_lock(&m_param->cuinfobuf_mutex);

  while (m_param->cuInfoToRead == vcenc_instance->numCuInfoBuf)
    pthread_cond_wait(&m_param->cuinfobuf_cond, &m_param->cuinfobuf_mutex);
  m_param->cuInfoToRead++;
  pthread_mutex_unlock(&m_param->cuinfobuf_mutex);
  return VCENC_OK;
}

#endif
