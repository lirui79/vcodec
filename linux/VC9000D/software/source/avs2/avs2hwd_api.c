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

#include "sw_debug.h"
#include "avs2hwd_api.h"
#include "avs2hwd_asic.h"
#include "avs2decmc_internals.h"
#include "commonconfig.h"
#include "commonfunction.h"
#include "wquant.h"
#include "dec_log.h"

/* Set/Get Paramters */
static HwdRet Avs2HwdSetSeqParam(struct Avs2Hwd *hwd,
                                 struct Avs2SeqParam *param) {
  // TODO: check parameter range.
  hwd->sps = param;
  return HWD_OK;
}

static HwdRet Avs2HwdSetPicParam(struct Avs2Hwd *hwd,
                                 struct Avs2PicParam *pps) {
  // TODO: check parameter range.
  hwd->pps = pps;

  if (pps->type == I_IMG && pps->typeb == BACKGROUND_IMG) {  // G/GB frame
    hwd->bk_img_is_top_field = pps->is_top_field;
  }
  return HWD_OK;
}

static HwdRet Avs2HwdSetConfig(struct Avs2Hwd *hwd,
                               struct Avs2ConfigParam *param) {
  // TODO: check parameter range.
  hwd->cfg = param;
  return HWD_OK;
}

static HwdRet Avs2HwdSetRecon(struct Avs2Hwd *hwd,
                              struct Avs2ReconParam *param) {
  // TODO: check parameter range.
  hwd->recon = param;
  return HWD_OK;
}

static HwdRet Avs2HwdSetReference(struct Avs2Hwd *hwd,
                                  struct Avs2RefsParam *refs) {
#if 0
  u32 i, j;
  u32 list0[16] = {0};
  u32 list1[16] = {0};

  /* list 0, short term before + short term after + long term */
  for (i = 0, j = 0; i < dpb->num_poc_st_curr; i++)
    list0[j++] = dpb->ref_pic_set_st[i];
  for (i = 0; i < dpb->num_poc_lt_curr; i++)
    list0[j++] = dpb->ref_pic_set_lt[i];

  /* fill upto 16 elems, copy over and over */
  i = 0;
  while (j < 16) list0[j++] = list0[i++];

  /* list 1, short term after + short term before + long term */
  /* after */
  for (i = dpb->num_poc_st_curr_before, j = 0; i < dpb->num_poc_st_curr; i++)
    list1[j++] = dpb->ref_pic_set_st[i];
  /* before */
  for (i = 0; i < dpb->num_poc_st_curr_before; i++)
    list1[j++] = dpb->ref_pic_set_st[i];
  for (i = 0; i < dpb->num_poc_lt_curr; i++)
    list1[j++] = dpb->ref_pic_set_lt[i];

  /* fill upto 16 elems, copy over and over */
  i = 0;
  while (j < 16) list1[j++] = list1[i++];

  /* TODO: size? */
  for (i = 0; i < MAX_DPB_SIZE; i++) {
    SetDecRegister(hwd->regs, ref_pic_list0[i], list0[i]);
    SetDecRegister(hwd->regs, ref_pic_list1[i], list1[i]);
  }
#endif
  hwd->refs = refs;

  return HWD_OK;
}

/* Initializes the decoder instance. */
HwdRet Avs2HwdInit(struct Avs2Hwd *hwd, const void *dwl) {

  /* Variables */
//  HwdRet ret;

  /* Code */

  ASSERT(hwd);
  hwd->dwl = (struct DWL *)dwl;
  if (hwd->dwl == NULL) return HWD_FAIL;

  pthread_mutex_init(&hwd->mutex, NULL);

  Avs2SetModeRegs(hwd);

  Avs2HwdSetParams(hwd, ATTRIB_SETUP, NULL);

  return HWD_OK;
}

void Avs2HwdSetAlfTable(struct Avs2Hwd *hwd, u32 core_id) {
  u8 *p;
  struct Avs2PicParam *pps = hwd->pps;
  struct Avs2AsicBuffers *cmems = hwd->cmems;

  /*set alf matrix*/
  p = (u8*)cmems->alf_tbl[core_id].virtual_address;
  DWLmemcpy(p, (u8*)&pps->alf_param, sizeof(struct Avs2AlfParams));
}

HwdRet Avs2HwdAllocInternals(struct Avs2Hwd *hwd) {
  i32 i, dwl_ret, size;
  struct Avs2AsicBuffers *cmems = hwd->cmems;

  for (i = 0; i < hwd->n_cores; i++) {
    /* alf_tbl */
    if (cmems->alf_tbl[i].virtual_address == NULL) {
      size = sizeof(struct Avs2AlfParams);
      cmems->alf_tbl[i].mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(cmems->alf_tbl[i].mem_type, DWL_MEM_USAGE_IN_AVS2ALF_TABLE,
                hwd->secure_mode);
      dwl_ret = DWLMallocLinear(hwd->dwl, size, &cmems->alf_tbl[i]);
      if (dwl_ret != DWL_OK) {
        APITRACEERR("%s","[dwl] cannot allocate alf table.\n");
        return HWD_FAIL;
      }
    }

    /* wqm_tbl */
    if (cmems->wqm_tbl[i].virtual_address == NULL) {
      size = sizeof(struct Avs2WQMatrix);
      cmems->wqm_tbl[i].mem_type |= DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
      SET_MEM_USAGE(cmems->wqm_tbl[i].mem_type, DWL_MEM_USAGE_IN_AVS2WQM_TABLE,
                hwd->secure_mode);
      dwl_ret = DWLMallocLinear(hwd->dwl, size, &cmems->wqm_tbl[i]);
      if (dwl_ret != DWL_OK) {
        APITRACEERR("%s","[dwl] cannot allocate wqm table.\n");
        return HWD_FAIL;
      }
      DWLmemset((void*)cmems->wqm_tbl[i].virtual_address, 1 << WQ_FLATBASE_INBIT, size);
    }
  }

  return HWD_OK;
}

HwdRet Avs2HwdAllocFakeTableMem(struct Avs2Hwd *hwd,
                                struct Avs2AsicBuffers *cmems) {
  i32 dwl_ret = HWD_OK;
  u32 size = 0;
  struct Avs2SeqParam *sps = hwd->sps;

#ifdef USE_FAKE_RFC_TABLE
  if (hwd->cfg->use_video_compressor) {
    /* Allocate and initialize fake RFC table for robustness. */
    u32 ref_buffer_align = MAX(16, ALIGN(hwd->align));
    u32 pic_width_in_cbsy, pic_height_in_cbsy;
    u32 pic_width_in_cbsc, pic_height_in_cbsc;
    u32 luma_table_size, chroma_table_size;
    u32 bit_depth = sps->sample_bit_depth;

    pic_width_in_cbsy = ((sps->pic_width + 8 - 1)/8);
    pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
    pic_width_in_cbsc = ((sps->pic_width + 16 - 1)/16);
    pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
    pic_height_in_cbsy = (sps->pic_height + 8 - 1)/8;
    pic_height_in_cbsc = (sps->pic_height/2 + 4 - 1)/4;

    /* luma table size */
    luma_table_size = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, ref_buffer_align);
    /* chroma table size */
    chroma_table_size = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, ref_buffer_align);
    size = luma_table_size + chroma_table_size;
    cmems->tbl_sizey = luma_table_size;
    cmems->tbl_sizec = chroma_table_size;

    cmems->fake_rfc_tbl.mem_type = DWL_MEM_TYPE_VPU_WORKING | DWL_MEM_TYPE_DMA_HOST_TO_DEVICE;
    SET_MEM_USAGE(cmems->fake_rfc_tbl.mem_type, DWL_MEM_USAGE_TMP_FAKERFC_TABLE, hwd->secure_mode);
#ifdef ASIC_TRACE_SUPPORT
    dwl_ret = DWLMallocRefFrm(hwd->dwl, size, &cmems->fake_rfc_tbl);
#else
    dwl_ret = DWLMallocLinear(hwd->dwl, size, &cmems->fake_rfc_tbl);
#endif
    if (dwl_ret) return -1;
    GenerateFakeRFCTable((u8 *)cmems->fake_rfc_tbl.virtual_address,
                          pic_width_in_cbsy, pic_height_in_cbsy,
                          pic_width_in_cbsc, pic_height_in_cbsc,
                          bit_depth, ref_buffer_align);
    DWLDMATransData(hwd->dwl, &cmems->fake_rfc_tbl, 0,
                    cmems->fake_rfc_tbl.size, HOST_TO_DEVICE);
  }
#else
  (void)size;
  (void)sps;
#endif

  return HWD_OK;
}

/* Set PP configure Paramters */
static HwdRet Avs2HwdSetPp(struct Avs2Hwd *hwd, struct Avs2PpoutParam *param) {
  hwd->ppout = param;
  return HWD_OK;
}

HwdRet Avs2HwdSetParams(struct Avs2Hwd *hwd, ATTRIBUTE attribute, void *data) {
  HwdRet ret = HWD_OK;

  pthread_mutex_lock(&hwd->mutex);
  switch (attribute) {
    case ATTRIB_SETUP:
      SetCommonConfigRegs(hwd->regs);
      break;
    case ATTRIB_CFG:
      ret = Avs2HwdSetConfig(hwd, data);
      break;
    case ATTRIB_SEQ:
      ret = Avs2HwdSetSeqParam(hwd, data);
      break;
    case ATTRIB_PIC:
      ret = Avs2HwdSetPicParam(hwd, data);
      break;
    case ATTRIB_RECON:
      ret = Avs2HwdSetRecon(hwd, data);
      break;
    case ATTRIB_PP:
      ret = Avs2HwdSetPp(hwd, data);
      break;
    case ATTRIB_REFS:
      ret = Avs2HwdSetReference(hwd, data);
      break;
    case ATTRIB_STREAM:
      hwd->stream = data;
      break;
    default:
      ret = HWD_FAIL;
      break;
  }

  if (ret == HWD_OK) {
    hwd->flags |= (1 << attribute);
  }
  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}

HwdRet Avs2HwdGetBufferSpec(struct Avs2Hwd *hwd,
                            struct Avs2BufferSpec *buffer_spec) {
  if (hwd->flags & (1 << ATTRIB_SEQ)) {
    APITRACEERR("%s","[avs2hwd] Cannot Get buffer spec before sps is got.\n");
    return HWD_FAIL;
  }

  // FIXME:
  return HWD_FAIL;
}

HwdRet Avs2HwdUpdateStream(struct Avs2Hwd *hwd, u32 asic_status) {
  struct Avs2StreamParam *stream = hwd->stream;
  HwdRet ret = HWD_OK;

  addr_t last_read_address;
  u32 bytes_processed;
  const addr_t start_address = stream->stream_bus_addr & (~DEC_HW_ALIGN_MASK);
  const u32 offset_bytes = stream->stream_bus_addr & DEC_HW_ALIGN_MASK;

  last_read_address = GET_ADDR_REG(hwd->regs, HWIF_STREAM_BASE);

  if (asic_status == DEC_HW_IRQ_RDY &&
      last_read_address == stream->stream_bus_addr) {
    last_read_address = start_address + stream->stream_length;  // FIXME:
  }

  if (last_read_address <= start_address)
    bytes_processed =
        stream->ring_buffer.size - (u32)(start_address - last_read_address);
  else
    bytes_processed = (u32)(last_read_address - start_address);

  /* workaround: Abnormal IRQ force data consumption. */
  if (bytes_processed == 0 &&
      (asic_status == DEC_HW_IRQ_BUS ||
       asic_status == DEC_HW_IRQ_TIMEOUT ||
       asic_status == DEC_HW_IRQ_EXT_TIMEOUT)) {
    bytes_processed = stream->stream_length;
  }

  APITRACEDEBUG("HW updated stream position: %08x\n", last_read_address);
  APITRACEDEBUG("           processed bytes: %8d\n", bytes_processed);
  APITRACEDEBUG("     of which offset bytes: %8d\n", offset_bytes);

  /* from start of the buffer add what HW has decoded */
  /* end - start smaller or equal than maximum */
  // FIXME:
  if ((bytes_processed - offset_bytes) > stream->stream_length) {

    if ((asic_status & DEC_HW_IRQ_RDY) || (asic_status & DEC_HW_IRQ_BUFFER)) {
      APITRACEDEBUG("%s","New stream position out of range!\n");
      // ASSERT(0);
      stream->stream += stream->stream_length;
      stream->stream_bus_addr += stream->stream_length;
      stream->stream_length = 0; /* no bytes left */

      /* Though asic_status returns DEC_HW_IRQ_BUFFER, the stream consumed is abnormal,
       * so we consider it's an errorous stream and release HW. */
      if (asic_status & DEC_HW_IRQ_BUFFER) {
        hwd->asic_running = 0;
        if (hwd->vcmd_used) {
          (void) DWLReleaseCmdBuf(hwd->dwl, hwd->cmdbuf_id);
        } else {
          (void) DWLReleaseHw(hwd->dwl, hwd->core_id);
        }
        ret = HWD_FAIL;
      }
      return ret;
    }

    /* consider all buffer processed */
    stream->stream += stream->stream_length;
    stream->stream_bus_addr += stream->stream_length;
    stream->stream_length = 0; /* no bytes left */
  } else {
    stream->stream_length -= (bytes_processed - offset_bytes);
    stream->stream += (bytes_processed - offset_bytes);
    stream->stream_bus_addr += (bytes_processed - offset_bytes);
  }

  /* if turnaround */
  if (stream->stream >
      ((u8 *)stream->ring_buffer.virtual_address + stream->ring_buffer.size)) {
    stream->stream -= stream->ring_buffer.size;
    stream->stream_bus_addr -= stream->ring_buffer.size;
  }

  return ret;
}

HwdRet Avs2HwdRun(struct Avs2Hwd *hwd, void *inst) {
  HwdRet ret = HWD_OK;
  i32 reserve_ret = 0;
  u32 core_id = 0;
  struct DWLReqInfo info = {0};
  const struct DecHwFeatures *hw_feature = NULL;

  if (hwd->ppout && hwd->ppout->hw_feature)
    hw_feature = hwd->ppout->hw_feature;
  else
    hw_feature = SwGetHwFeature(hwd->dwl, CORE_MASK(hwd->core_mask),
                                DWL_CLIENT_TYPE_AVS2_DEC);

  pthread_mutex_lock(&hwd->mutex);

  info.core_mask = hwd->core_mask;
  info.width = hwd->sps->pic_width;
  info.height = hwd->sps->pic_height;
  info.owner = (void *)hwd;
  if (hwd->vcmd_used) {
    hwd->core_id = 0;
    hwd->cmdbuf_id = 0;
    if (hwd->b_mc) {
      FifoObject obj;
      FifoPop(hwd->fifo_core, &obj, FIFO_EXCEPTION_DISABLE);
      hwd->mc_buf_id = (i32)(addr_t)obj;
    }
    reserve_ret = DWLReserveCmdBuf(hwd->dwl, &info, &hwd->cmdbuf_id);
  } else {
    reserve_ret = DWLReserveHw(hwd->dwl, &info, &hwd->core_id);
  }
  if (reserve_ret != DWL_OK) {
    hwd->status = X170_DEC_HW_RESERVED;
    goto out;
  }

  /* Set dec_status to RUNNING in MC mode */
  if (hwd->b_mc && !hwd->vcmd_used) {
    hwd->dec_status[hwd->core_id] = DEC_RUNNING;
  }
  /* core id is to used as index of buffer, generatlly, it is 0 if not multi core decode */
  core_id = hwd->b_mc ? (hwd->vcmd_used ? hwd->mc_buf_id : hwd->core_id) : 0;
  hwd->asic_running = 1;

  /* config asic buff params */
  if (hwd->sps->weight_quant_enable_flag &&
      hwd->pps->pic_weight_quant_enable_flag) {
    InitFrameQuantParam(hwd);
    FrameUpdateWQMatrix(hwd, core_id);  // M2148 2007-09
  }
  if (hwd->sps->alf_enable &&
      (hwd->pps->alf_flag[0] ||
       hwd->pps->alf_flag[1] ||
       hwd->pps->alf_flag[2])) {
    Avs2HwdSetAlfTable(hwd, core_id);
  }

  Avs2SetRegs(hwd, inst);

  /* Warning: only single core are currently supported (core_id = 0) */
  if (hwd->pp_enabled)
    PPSetLancozsScaleRegs(hwd->regs, hw_feature, hwd->ppout->ppu_cfg, core_id);

  if (hwd->pp_enabled) {
    if (hwd->vcmd_used) {
      DWLReadPpConfigure(hwd->dwl, hwd->cmdbuf_id, hwd->ppout->ppu_cfg, 0);
    } else {
      DWLReadPpConfigure(hwd->dwl, hwd->core_id, hwd->ppout->ppu_cfg, 0);
    }
  }
  if (hwd->vcmd_used) {
    if (hwd->mc_buf_id >= hwd->n_cores_available) {
      hwd->mc_buf_id = 0;
    }
    DWLFlushRegister(hwd->dwl, hwd->cmdbuf_id, hwd->regs,
                     hwd->mc_refresh_regs[hwd->mc_buf_id], hwd->mc_buf_id);
  } else {
    FlushDecRegisters(hwd->dwl, hwd->core_id, hwd->regs, hw_feature->max_ppu_count);
  }

  /* when b_mc is 0, the callback callback_arg has been clear up in DWLReserveHw/DWLReserveCmdBuf */
  if(hwd->b_mc) {
    Avs2MCSetHwRdyCallback(inst);
  }

  SetDecRegister(hwd->regs, HWIF_DEC_E, 1);
  if (hwd->vcmd_used)
    DWLEnableCmdBuf(hwd->dwl, hwd->cmdbuf_id);
  else
    DWLEnableHw(hwd->dwl, hwd->core_id, 4 * 1, hwd->regs[1]);

  hwd->status = DEC_HW_IRQ_RDY;

out:
  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}


HwdRet Avs2HwdSync(struct Avs2Hwd *hwd, i32 timeout) {
  HwdRet ret = HWD_OK;
  i32 val;
  const struct DecHwFeatures *hw_feature = NULL;

  if (hwd->ppout && hwd->ppout->hw_feature)
    hw_feature = hwd->ppout->hw_feature;
  else
    hw_feature = SwGetHwFeature(hwd->dwl, CORE_MASK(hwd->core_mask),
                                DWL_CLIENT_TYPE_AVS2_DEC);

  if(hwd->b_mc) {
    /* reset shadow HW status reg values so that we dont end up writing
     * some garbage to next core regs */
    SetDecRegister(hwd->regs, HWIF_DEC_E, 0);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0);

    hwd->asic_running = 0;
    hwd->status = DEC_HW_IRQ_RDY;
    return HWD_OK;
  }

  pthread_mutex_lock(&hwd->mutex);

  if (hwd->vcmd_used)
    val = DWLWaitCmdBufReady(hwd->dwl, hwd->cmdbuf_id);
  else
    val = DWLWaitHwReady(hwd->dwl, hwd->core_id, timeout);

  if (val != DWL_HW_WAIT_OK) {
    APITRACEERR("%s","DWLWaitHwReady: DWL_HW_WAIT_NOK\n");
    APITRACEDEBUG("DWLWaitHwReady returned: %d\n", ret);
    /* Reset HW */
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0);

    hwd->asic_running = 0;
    if (hwd->vcmd_used) {
      DWLReleaseCmdBuf(hwd->dwl, hwd->cmdbuf_id);
    }
    else {
      DWLDisableHw(hwd->dwl, hwd->core_id, 4 * 1, hwd->regs[1]);
      DWLReleaseHw(hwd->dwl, hwd->core_id);
    }
    hwd->status =
          (val == DWL_HW_WAIT_ERROR) ? X170_DEC_SYSTEM_ERROR : X170_DEC_TIMEOUT;
    ret = HWD_FAIL;
  }
  else {
    if(hwd->vcmd_used)
      DWLRefreshRegister(hwd->dwl, hwd->cmdbuf_id, hwd->regs);
    else
      RefreshDecRegisters(hwd->dwl, hwd->core_id, hwd->regs, hw_feature->max_ppu_count);

    hwd->status = GetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0); /* just in case */

    if (!(hwd->status & DEC_HW_IRQ_BUFFER)) {
      /* HW done, release it! */
      hwd->asic_running = 0;

      if (hwd->vcmd_used) {
        (void) DWLReleaseCmdBuf(hwd->dwl, hwd->cmdbuf_id);
      } else {
        (void) DWLReleaseHw(hwd->dwl, hwd->core_id);
      }
      ret = HWD_OK;
    }

    /* update stream offset */
    ret = Avs2HwdUpdateStream(hwd, hwd->status);
  }

  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}

HwdRet Avs2HwdDone(struct Avs2Hwd *hwd, i32 ID) {
  HwdRet ret = HWD_OK;
  pthread_mutex_lock(&hwd->mutex);

  // FIXME:

  pthread_mutex_unlock(&hwd->mutex);
  return ret;
}

HwdRet Avs2HwdRelease(struct Avs2Hwd *hwd) {
  // Free resources
  if (hwd->cmems) {
    for (int i = 0; i < MAX_ASIC_CORES; i++) {
      if (hwd->cmems->alf_tbl[i].virtual_address != NULL) {
        DWLFreeLinear(hwd->dwl, &hwd->cmems->alf_tbl[i]);
        hwd->cmems->alf_tbl[i].virtual_address = NULL;
      }
      if (hwd->cmems->wqm_tbl[i].virtual_address != NULL) {
        DWLFreeLinear(hwd->dwl, &hwd->cmems->wqm_tbl[i]);
        hwd->cmems->wqm_tbl[i].virtual_address = NULL;
      }
    }
#ifdef USE_FAKE_RFC_TABLE
    if (hwd->cmems->fake_rfc_tbl.virtual_address != NULL) {
#ifdef ASIC_TRACE_SUPPORT
      DWLFreeRefFrm(hwd->dwl, &hwd->cmems->fake_rfc_tbl);
#else
      DWLFreeLinear(hwd->dwl, &hwd->cmems->fake_rfc_tbl);
#endif
      hwd->cmems->fake_rfc_tbl.virtual_address = NULL;
    }
#endif
  }

  // DWLRelease() should be called at APP.

  return HWD_OK;
}

HwdRet Avs2HwdStopHw(struct Avs2Hwd *hwd, i32 ID) {
  SetDecRegister(hwd->regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(hwd->regs, HWIF_DEC_IRQ, 0);
  SetDecRegister(hwd->regs, HWIF_DEC_E, 0);
  if (hwd->vcmd_used) {
    DWLReleaseCmdBuf(hwd->dwl, hwd->cmdbuf_id);
  }
  else {
    DWLDisableHw(hwd->dwl, ID, 4 * 1, hwd->regs[1]);
    DWLReleaseHw(hwd->dwl, ID); /* release HW lock */
  }

  return HWD_OK;
}
