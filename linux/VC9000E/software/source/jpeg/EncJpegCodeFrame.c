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
--------------------------------------------------------------------------------


--
--  Abstract  :    JPEG Code Frame control
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "enccommon.h"
#include "ewl.h"

#include "EncJpegCodeFrame.h"
#include "encdec400.h"
#include "encufbc.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/
static void NextHeader(stream_s *stream, jpegData_s *jpeg);
static void EndRi(stream_s *stream, jpegData_s *jpeg);
static void jpegSetNewFrame(jpegInstance_s *inst);
static u32 jpegEstimateExecutingTime(u32 width, u32 height, u32 vlc_optimize)
{
  return width * height * (vlc_optimize+1);
}
/*------------------------------------------------------------------------------

    EncJpegCodeFrame

------------------------------------------------------------------------------*/
jpegEncodeFrame_e EncJpegCodeFrameRun(jpegInstance_s *inst) {
  jpegEncodeFrame_e ret;
  u32 vcmd_en;

  vcmd_en = EWLGetVCMDSupport(inst->asic.ewl);

  /* set output stream start point in case of whole encoding mode
     * and no rst used */
  if (inst->stream.byteCnt == 0) {
    inst->jpeg.streamStartAddress = inst->stream.stream;
  }

  /* set new frame encoding parameters */
  jpegSetNewFrame(inst);

  if (vcmd_en == 0) {
    /* start hw encoding */
    EncAsicFrameStart(inst->asic.ewl, &inst->asic.regs, inst->asic.dumpRegister);
  } else {
  #ifdef VCMD_BUILD_SUPPORT
    u32 executing_time = jpegEstimateExecutingTime(inst->jpeg.width,
                    inst->jpeg.height, 0);
    //priority = 0, will be set in command line.
    EncSetReseveInfo(inst->asic.ewl, executing_time,
                     EWL_CLIENT_TYPE_JPEG_ENC);
    EncReseveCmdbuf(inst->asic.ewl, &inst->asic.regs.vcmd);
    if (EncMakeCmdbufData(&inst->asic, &inst->asic.regs) ==
        JPEGENC_INVALID_ARGUMENT) {
      return JPEGENCODE_INVALID_ARGUMENT;
    }

    EncLinkRunCmdbuf(inst->asic.ewl, &inst->asic.regs.vcmd);
  #endif
  }
  return JPEGENCODE_OK;
}

jpegEncodeFrame_e EncJpegCodeFrameWait(jpegInstance_s *inst) {
  jpegEncodeFrame_e ret;
  asicData_s *asic = &inst->asic;
  u32 status = ASIC_STATUS_ERROR;
  u32 vcmd_en;
  u32 flag=0;

  vcmd_en = EWLGetVCMDSupport(asic->ewl);

  do {
    /* Encode one frame */
    i32 ewl_ret = EWL_ERROR;

#ifndef JPEG_SIMULATION
    /* Wait for IRQ */
    if (vcmd_en == 0) {
      ewl_ret = EWLWaitHwRdy(asic->ewl, NULL, NULL, &status);
    } else {
#ifdef VCMD_BUILD_SUPPORT
      ewl_ret = EncWaitCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid, &status);
#endif
    }
#else
    return JPEGENCODE_OK;
#endif
    if (ewl_ret != EWL_OK) {
      status = ASIC_STATUS_ERROR;

      if (ewl_ret == EWL_ERROR) {
        /* IRQ error => Stop and release HW */
        ret = JPEGENCODE_SYSTEM_ERROR;
      } else /*if(ewl_ret == EWL_HW_WAIT_TIMEOUT) */
      {
        /* IRQ Timeout => Stop and release HW */
        ret = JPEGENCODE_TIMEOUT;
      }

      EncAsicStop(asic->ewl);
      /* Release HW so that it can be used by other codecs */
      if (vcmd_en == 0) {
        if (asic->dec400_data->dec400Enable != 0) {
          VCEncDisableDec400(asic->ewl, NULL);
        }
#ifdef SUPPORT_AXIFE
        if (asic->axife_data->mode != 0) VCEncAxiFeDisable(asic->ewl, NULL);
#endif
  //stop ufbc
        if(inst->ufbcParam.mode) {
          EncUfbcAsicStop(asic->ewl, NULL, inst->ufbcParam.mode);
        }
        EWLReleaseHw(asic->ewl);
      } else {
#ifdef VCMD_BUILD_SUPPORT
        EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
#endif
      }

    } else {
      flag = (status & (ASIC_STATUS_LINE_BUFFER_DONE |
            ASIC_STATUS_SEGMENT_READY));
      do {
      status &= ASIC_STATUS_ALL;
      u32 cstatus = EncAsicCheckStatus_V2(asic, status);

      if (cstatus != ASIC_STATUS_LINE_BUFFER_DONE &&
          cstatus != ASIC_STATUS_SEGMENT_READY) {
        if (vcmd_en == 0) {
          if (asic->dec400_data->dec400Enable != 0) {
            VCEncDisableDec400(asic->ewl, NULL);
          }
#ifdef SUPPORT_AXIFE
          if (asic->axife_data->mode != 0) VCEncAxiFeDisable(asic->ewl, NULL);
#endif
          if(inst->ufbcParam.mode) {
            EncUfbcAsicStop(asic->ewl, NULL, inst->ufbcParam.mode);
          }
        }
      }

      switch (cstatus) {
        case ASIC_STATUS_ERROR:
          if (vcmd_en == 0) {
            EWLReleaseHw(asic->ewl);
          } else {
            EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
          }
          ret = JPEGENCODE_HW_ERROR;
          flag=0;
          status=0;
          break;
        case ASIC_STATUS_BUFF_FULL:
          if (vcmd_en == 0) {
            EWLReleaseHw(asic->ewl);
          } else {
            EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
          }
          ret = JPEGENCODE_OK;
          inst->stream.overflow = ENCHW_YES;
          flag=0;
          status=0;
          break;
        case ASIC_STATUS_HW_RESET:
          if (vcmd_en == 0) {
            EWLReleaseHw(asic->ewl);
          } else {
            EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
          }
          ret = JPEGENCODE_HW_RESET;
          flag=0;
          status=0;
          break;
        case ASIC_STATUS_HW_TIMEOUT:
        case ASIC_STATUS_POLL_SLICEINFO_TIMEOUT:
        case ASIC_STATUS_UFBC_DEC_ERR:
          if (vcmd_en == 0) {
            EWLReleaseHw(asic->ewl);
          } else {
            EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
          }
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
          if (cstatus & ASIC_STATUS_POLL_SLICEINFO_TIMEOUT)
            inst->sliceinfo_status = ASIC_STATUS_POLL_SLICEINFO_TIMEOUT;
#endif
#ifdef SUPPORT_UFBC
         if (cstatus & ASIC_STATUS_UFBC_DEC_ERR) {
           inst->ufbc_status = ASIC_STATUS_UFBC_DEC_ERR;
         }
        if (cstatus & ASIC_STATUS_UFBC_CFG_ERR) {
           inst->ufbc_status = ASIC_STATUS_UFBC_CFG_ERR;
         }
#endif
          ret = JPEGENCODE_TIMEOUT;
          flag=0;
          status=0;
          break;
        case ASIC_STATUS_FRAME_READY:
          inst->stream.byteCnt -=
              (asic->regs.firstFreeBit /
               8); /* last not full 64-bit counted in HW data */
          inst->stream.byteCnt += asic->regs.outputStrmSize[0];
          ret = JPEGENCODE_OK;
          if (vcmd_en == 0) {
            EWLReleaseHw(asic->ewl);
          } else {
            EWLReleaseCmdbuf(asic->ewl, asic->regs.vcmd.cmdbufid);
          }
          flag=0;
          status=0;
          break;
        case ASIC_STATUS_LINE_BUFFER_DONE:
          ret = JPEGENCODE_OK;
          /* SW handshaking: Software will clear the line buffer interrupt and then update the
          *   line buffer write pointer, when the next line buffer is ready. The encoder will
          *   continue to run when detected the write pointer is updated.  */
          if (!inst->inputLineBuf.inputLineBufHwModeEn) {
            if (inst->inputLineBuf.cbFunc)
              inst->inputLineBuf.cbFunc(inst->inputLineBuf.cbData);
          }
          break;
        case ASIC_STATUS_SEGMENT_READY:
          ASSERT(inst->streamMultiSegment.streamMultiSegmentMode != 0);
          ret = JPEGENCODE_OK;
          u32 outLength = EncAsicGetRegisterValue(asic->ewl, asic->regs.regMirror,
                  HWIF_ENC_OUTPUT_STRM_BUFFER_LIMIT);
          u32 wrCnt = outLength / inst->streamMultiSegment.streamMultiSegmentSize;
          //wait until enough data ready
          while (inst->streamMultiSegment.rdCnt < wrCnt) {
            if (inst->streamMultiSegment.cbFunc)
              inst->streamMultiSegment.cbFunc(inst->streamMultiSegment.cbData); //, outLength
            /*note: must make sure the data of one segment is read by
            * app then rd counter can increase*/
            inst->streamMultiSegment.rdCnt++;
          }
          break;
        default:
          /* should never get here */
          ASSERT(0);
          ret = JPEGENCODE_HW_ERROR;
      }
      status &= ~cstatus;
      } while(status);
    }
  } while (flag);

  /* Handle EOI */
  if (ret == JPEGENCODE_OK) {
    /* update mcu count */
    if (inst->jpeg.codingType == ENC_PARTIAL_FRAME) {
      if (inst->jpeg.losslessEn) {
        u32 rstMbs = (inst->jpeg.width + 15) / 16 * inst->jpeg.rstMbRows;

        if ((inst->jpeg.mbNum + rstMbs) < ((u32)inst->jpeg.mbPerFrame)) {
          inst->jpeg.mbNum += rstMbs;
          inst->jpeg.row += inst->jpeg.sliceRows;
        } else {
          inst->jpeg.mbNum += (inst->jpeg.mbPerFrame - inst->jpeg.mbNum);
        }
      } else {
        u32 rstMbs = (inst->jpeg.width + 15) / 16 * inst->jpeg.rstMbRows;

        if ((inst->jpeg.mbNum + rstMbs) < ((u32)inst->jpeg.mbPerFrame)) {
          inst->jpeg.mbNum += rstMbs;
          inst->jpeg.row += inst->jpeg.sliceRows;
        } else {
          inst->jpeg.mbNum += (inst->jpeg.mbPerFrame - inst->jpeg.mbNum);
        }
      }
    } else {
      inst->jpeg.mbNum += inst->jpeg.mbPerFrame;
    }

    EndRi(&inst->stream, &inst->jpeg);
  }

  return ret;
}

/*------------------------------------------------------------------------------

    Write the header data (frame header or restart marker) to the stream.

------------------------------------------------------------------------------*/
void NextHeader(stream_s *stream, jpegData_s *jpeg) {
  if (jpeg->mbNum == 0) {
    (void)EncJpegHdr(stream, jpeg);
  }
}

/*------------------------------------------------------------------------------

    Write the end of current coding unit (RI / FRAME) into stream.

------------------------------------------------------------------------------*/
void EndRi(stream_s *stream, jpegData_s *jpeg) {
  /* not needed anymore, ASIC generates EOI marker */
}

/*------------------------------------------------------------------------------

    Set encoding parameters at the beginning of a new frame.

------------------------------------------------------------------------------*/
void jpegSetNewFrame(jpegInstance_s *inst) {
  regValues_s *regs = &inst->asic.regs;
  u8 offsetTo8BytesAlignment = 0;
  ptr_t strmBaseTmp = regs->outputStrmBase[0];

  /* Write next header if needed */
  NextHeader(&inst->stream, &inst->jpeg);
  if (inst->streamMultiSegment.streamMultiSegmentMode > 0 &&
  regs->streamMultiSegSize > 0){
    // regs->streamMultiSegOffset = (inst->stream.byteCnt & (~0x07)) % regs->streamMultiSegSize;
    regs->streamMultiSegOffset = inst->stream.byteCnt & (~0x07);
  }
  if (HW_PRODUCT_VC9000LE(regs->asicHwId) && HW_ID_MINOR_NUMBER(regs->asicHwId) == 0x0 &&
    HW_ID_MAJOR_NUMBER(regs->asicHwId) == 0x10) {
    offsetTo8BytesAlignment =
        (u8)((regs->outputStrmBase[0] + inst->stream.byteCnt) & 0x07);
    regs->outputStrmSize[0] -= (inst->stream.byteCnt - offsetTo8BytesAlignment);
    inst->invalidBytesInBuf0Tail = regs->outputStrmSize[0] & 0x07;
    regs->outputStrmSize[0] &= (~0x07); /* 8 multiple size */

    /* 64-bit aligned stream base address */
    regs->outputStrmBase[0] =
      ((regs->outputStrmBase[0] + inst->stream.byteCnt) & (~0x07));
  } else {
    /* calculate output start point for hw */
    regs->outputStrmSize[0] -= inst->stream.byteCnt;
    regs->outputStrmBase[0] = regs->outputStrmBase[0] + inst->stream.byteCnt;
  }

  /* bit offset in the last 64-bit word */
  regs->firstFreeBit = (offsetTo8BytesAlignment)*8;
  regs->jpegHeaderLength = (u32)(regs->outputStrmBase[0] - strmBaseTmp);
  hash(&inst->jpeg.hashctx, inst->jpeg.streamStartAddress,
       (inst->stream.byteCnt - offsetTo8BytesAlignment));
  regs->hashtype = inst->jpeg.hashctx.hash_type;
  hash_getstate(&inst->jpeg.hashctx, &regs->hashval, &regs->hashoffset);

  /* header remainder is byte aligned, max 7 bytes = 56 bits */
  if (regs->firstFreeBit != 0) {
    /* 64-bit aligned stream pointer */
    u8 *pTmp = (u8 *)((ptr_t)(inst->stream.stream) & (~0x07));
    u32 val;

    /* Clear remaining bits */
    for (val = 6; val >= regs->firstFreeBit / 8; val--) pTmp[val] = 0;

    val = (u32)pTmp[0] << 24;
    val |= (u32)pTmp[1] << 16;
    val |= (u32)pTmp[2] << 8;
    val |=(u32) pTmp[3];

    regs->strmStartMSB = val; /* 32 bits to MSB */

    if (regs->firstFreeBit > 32) {
      val = (u32)pTmp[4] << 24;
      val |= (u32)pTmp[5] << 16;
      val |= (u32)pTmp[6] << 8;

      regs->strmStartLSB = val;
    } else
      regs->strmStartLSB = 0;
  } else {
    regs->strmStartMSB = regs->strmStartLSB = 0;
  }

  /* low latency: configure related register.*/
  regs->lineBufferEn = inst->inputLineBuf.inputLineBufEn;
  regs->lineBufferHwHandShake = inst->inputLineBuf.inputLineBufHwModeEn;
  regs->lineBufferLoopBackEn = inst->inputLineBuf.inputLineBufLoopBackEn;
  regs->lineBufferDepth = inst->inputLineBuf.inputLineBufDepth;
  regs->amountPerLoopBack = inst->inputLineBuf.amountPerLoopBack;
  regs->initSegNum = inst->inputLineBuf.initSegNum;
  regs->mbWrPtr = inst->inputLineBuf.wrCnt;
  regs->mbRdPtr = 0;
  regs->lineBufferInterruptEn =
      ENCH2_INPUT_BUFFER_INTERRUPT & regs->lineBufferEn &
      (regs->lineBufferHwHandShake == 0) & (regs->lineBufferDepth > 0);
  regs->sbi_id_0 = inst->inputLineBuf.sbi_id_0;
  regs->sbi_id_1 = inst->inputLineBuf.sbi_id_1;
  regs->sbi_id_2 = inst->inputLineBuf.sbi_id_2;
  regs->segmentUnitHeight = inst->inputLineBuf.segmentUnitHeight;

#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
    /* low latency: config poll input sliceinfo register */
  regs->sliceinfo_poll_enable = inst->sliceinfoEn;
#endif
}
