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
--  Abstract  :
--
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    Table of contents

    1. Include headers
    2. External compiler flags
    3. Module defines
    4. Local function prototypes
    5. Functions

------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "EncJpegInit.h"

#include "EncJpegCommon.h"
#include "ewl.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

    JpegInit

------------------------------------------------------------------------------*/
JpegEncRet JpegInit(const JpegEncCfg *pEncCfg, jpegInstance_s **instAddr,
                    void **ctx) {
  jpegInstance_s *inst = NULL;
  const void *ewl = NULL;

  JpegEncRet ret = JPEGENC_OK;
  EWLInitParam_t param;

  ASSERT(pEncCfg);
  ASSERT(instAddr);

  *instAddr = NULL;

  /* Init EWL */
  param.context = *ctx;
  param.clientType = EWL_CLIENT_TYPE_JPEG_ENC;
  param.slice_idx = pEncCfg->slice_idx;
  param.enc_dev = pEncCfg->enc_dev;
  param.mem_dev = pEncCfg->mem_dev;
  param.useVcmd = pEncCfg->useVcmd;
  if ((ewl = EWLInit(&param)) == NULL) {
    printf("JpegInit: Fail to init ewl\n");
    return JPEGENC_EWL_ERROR;
  }
  *ctx = (void *)ewl;

  /* Encoder instance */
  inst = (jpegInstance_s *)EWLcalloc(1, sizeof(jpegInstance_s));

  if (inst == NULL) {
    printf("JpegInit: fail to malloc memory for encoder instance\n");
    ret = JPEGENC_MEMORY_ERROR;
    goto err;
  }

  /* Default values */
  EncJpegInit(&inst->jpeg);

  inst->jpeg.codingMode = pEncCfg->codingMode;

  /* lossless mode */
  inst->jpeg.losslessEn = pEncCfg->losslessEn;
  if (inst->jpeg.losslessEn) {
    inst->jpeg.predictMode = pEncCfg->predictMode;
    inst->jpeg.ptransValue = pEncCfg->ptransValue;
    ASSERT(inst->jpeg.predictMode > 0);
    ASSERT(inst->jpeg.predictMode < 8);
  } else {
    inst->jpeg.predictMode = 0;
    inst->jpeg.ptransValue = 0;
  }

  /* Set parameters depending on user config */

  if (pEncCfg->quality == -1) {
    /* Choose quantization tables */
    inst->jpeg.qTable.pQlumi = QuantLuminance[pEncCfg->qLevel];
    inst->jpeg.qTable.pQchromi = QuantChrominance[pEncCfg->qLevel];

    /* If user specified quantization tables, use them */
    if (pEncCfg->qTableLuma) {
      JpegSetQTable(inst->jpeg.qTableLuma, pEncCfg->qTableLuma);
      inst->jpeg.qTable.pQlumi = inst->jpeg.qTableLuma;
    }

    if (pEncCfg->qTableChroma) {
      JpegSetQTable(inst->jpeg.qTableChroma, pEncCfg->qTableChroma);
      inst->jpeg.qTable.pQchromi = inst->jpeg.qTableChroma;
    }
  } else {
    JpegEncQuality quality;
    quality.factor = pEncCfg->quality;
    JpegEncSetQuailty(inst, &quality);
  }

  /* Comment header data */
  if (pEncCfg->comLength > 0 && pEncCfg->pCom != NULL) {
    inst->jpeg.com.comLen = pEncCfg->comLength;
    inst->jpeg.com.pComment = pEncCfg->pCom;
    inst->jpeg.com.comEnable = 1;
  }

  /* Units type */
  if (pEncCfg->unitsType == JPEGENC_NO_UNITS) {
    inst->jpeg.appn.units = ENC_NO_UNITS;
    inst->jpeg.appn.Xdensity = 1;
    inst->jpeg.appn.Ydensity = 1;
  } else {
    inst->jpeg.appn.units = pEncCfg->unitsType;
    inst->jpeg.appn.Xdensity = pEncCfg->xDensity;
    inst->jpeg.appn.Ydensity = pEncCfg->yDensity;
  }

  /* Marker type */
  if (pEncCfg->markerType == JPEGENC_SINGLE_MARKER) {
    inst->jpeg.markerType = ENC_SINGLE_MARKER;
  } else {
    inst->jpeg.markerType = pEncCfg->markerType;
  }

#if 0
    /* Rotation type */
    if(pEncCfg->rotation == JPEGENC_ROTATE_90R)
    {
        inst->jpeg.rotation = ROTATE_90R;
    }
    else if(pEncCfg->rotation == JPEGENC_ROTATE_90L)
    {
        inst->jpeg.rotation = ROTATE_90L;
    }
    else
    {
        inst->jpeg.rotation = ROTATE_0;
    }
#endif

  /* Copy quantization tables to ASIC internal memories */
  EncAsicSetQuantTable(&inst->asic, inst->jpeg.qTable.pQlumi,
                       inst->jpeg.qTable.pQchromi);

  if (pEncCfg->enableRoimap) {
    JpegEncSetNonRoi(inst, pEncCfg->filter);
  }

  /* Initialize ASIC */
  inst->asic.ewl = ewl;
  (void)EncAsicControllerInit(&inst->asic, *ctx, param.clientType);

  *instAddr = inst;

  return ret;

err:
  if (inst != NULL) EWLfree(inst);
  if (ewl != NULL) (void)EWLRelease(ewl);

  return ret;
}

/*------------------------------------------------------------------------------

    JpegShutdown

    Function frees the encoder instance.

    Input   instance_s *    Pointer to the encoder instance to be freed.
                            After this the pointer is no longer valid.

------------------------------------------------------------------------------*/
void JpegShutdown(jpegInstance_s *data) {
  const void *ewl;

  ASSERT(data);

  ewl = data->asic.ewl;
  if (data->asic.axife_data) EWLfree(data->asic.axife_data);
  EncAsicMemFree_V2(&data->asic);

  EWLfree(data);

  (void)EWLRelease(ewl);
}

