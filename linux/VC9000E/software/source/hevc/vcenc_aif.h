/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2023 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Abstract : Anti Intra Flicker Control
--
------------------------------------------------------------------------------*/

#ifndef __VCENC_AIF_H__
#define __VCENC_AIF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "base_type.h"
#include "enccommon.h"
#include "sw_picture.h"

typedef enum {
  /** Invalid aif frame */
  AIF_NONE = 0,
  /** reference frame for encoding aif */
  AIF_REFER,
  /** reconstruct frame for encoding aif */
  AIF_RECON,
  /** intra frame for aif encoding */
  AIF_INTRA
} AifFrame;

typedef enum {
  /** wait reference */
  AIF_STAGE_WAIT_REF,
  /** wait buffer for recon */
  AIF_STAGE_WAIT_BUFFER,
  /** wait intra input */
  AIF_STAGE_WAIT_INTRA,
  /** encode as P frame */
  AIF_STAGE_ENCODE_RECON,
  /** encode the intra frame */
  AIF_STAGE_ENCODE_INTRA,
  /** MAX stage */
  AIF_STAGE_MAX
} AifStage;

/** internal input parameters attached with each input frame */
typedef struct {
  /** if this input is only a internal helper job */
  u32 is_sideline;

  /** the frame type for aif processing, valid only when is_sideline is 1 */
  AifFrame type;
} VCEncAifInfo;

/** parameters to control aif */
typedef struct {
  /** enable anti intra flicker */
  u32 enable;
  /** stage to encode an anit-flicker frame. */
  AifStage stage;

  /** reference frame to encode the P frame */
  VCEncIn refer_in;
  u32 refer_poc;
  i32 refer_qp;
  u32 refer_gop_size;
  ptr_t refer_lu_base;
  ptr_t refer_ch_base;

  /** recon frame as the P frame */
  u32 recon_poc;
  struct sw_picture *recon_pic;
  ptr_t recon_lu_base;
  ptr_t recon_ch_base;
  ptr_t recon_stream;
  VCEncIn *recon_next;

  /** intra frame */
  VCEncIn input_in;
  u32 intra_idx;

  i32 qp_delta;
} VCAifCtrl;

/** check if aif conflicts with other configure */
VCEncRet VCEncAifCheckConfig(const VCEncConfig *enc_cfg,
                                     const EWLHwConfig_t *asic_cfg);
/** check if aif conflicts with other params */
VCEncRet VCEncAifCheckParams(struct vcenc_instance *inst,
                                     VCEncCodingCtrl *params);

/** schedule aif job */
VCEncRet VCEncAifSchedule(VCEncInst handle, VCEncIn *in);
/** prepare encoding parameters */
VCEncRet  VCEncAifPrepare(VCEncInst handle, VCEncIn *in, struct sw_picture *pic);
/** set for processing */
void VCEncAifProcess();
/** update internal state after coding one frame */
VCEncRet  VCEncAifUpdate(VCEncInst handle, VCEncIn *in);

#ifdef __cplusplus
}
#endif
#endif /* __VCENC_AIF_H__ */
