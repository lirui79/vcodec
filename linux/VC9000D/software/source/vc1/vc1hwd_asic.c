/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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

#include "vc1hwd_asic.h"
#include "regdrv.h"
#include "vc1hwd_util.h"
#include "vc1hwd_headers.h"
#include "vc1hwd_decoder.h"
#include "vpufeature.h"
#include "sw_util.h"
/*------------------------------------------------------------------------------
    External compiler flags
------------------------------------------------------------------------------*/

#ifndef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          do{}while(0)
#else
#undef TRACE_PP_CTRL
#define TRACE_PP_CTRL(...)          printf(__VA_ARGS__)
#endif
/*------------------------------------------------------------------------------
    Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    Local function prototypes
------------------------------------------------------------------------------*/

static void SetIntensityCompensationParameters(decContainer_t *dec_cont);

static void SetReferenceBaseAddress(decContainer_t *dec_cont,
                                    const struct DecHwFeatures *hw_feature);

/*------------------------------------------------------------------------------
    Functions
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------
    Function name   : vc1RefreshRegs
    Description     :
    Return type     : void
    Argument        : decContainer_t *dec_cont
------------------------------------------------------------------------------*/
void vc1RefreshRegs(decContainer_t * dec_cont) {
  addr_t i;
  u32 *dec_regs = dec_cont->vc1_regs;

  if(dec_cont->vcmd_used) {
    DWLRefreshRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->vc1_regs);
  } else {
    for(i = 1; i < DEC_X170_REGISTERS; i++) {
      dec_regs[i] = DWLReadReg(dec_cont->dwl, dec_cont->core_id, 4 * i);
    }
  }
}

/*------------------------------------------------------------------------------
    Function name   : vc1FlushRegs
    Description     :
    Return type     : void
    Argument        : decContainer_t *dec_cont
------------------------------------------------------------------------------*/
void vc1FlushRegs(decContainer_t * dec_cont) {
  i32 i;
  u32 *dec_regs = dec_cont->vc1_regs;

  if (dec_cont->vcmd_used) {
    DWLFlushRegister(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->vc1_regs,
                     dec_cont->mc_refresh_regs[dec_cont->core_id], dec_cont->core_id);
  } else {
    for(i = 2; i < DEC_X170_REGISTERS; i++) {
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * i, dec_regs[i]);
    }
  }
}

/*------------------------------------------------------------------------------

    Function name: VC1RunAsic

        Functional description:

        Inputs:
            dec_cont        Pointer to decoder container
            p_strm           Pointer to stream data structure
            strm_bus_addr     Stream bus address for hw

        Outputs:
            None

        Returns:
            asic_status      Hardware status

------------------------------------------------------------------------------*/
u32 VC1RunAsic(decContainer_t *dec_cont, strmData_t *p_strm, addr_t strm_bus_addr,
               const struct DecHwFeatures *hw_feature) {

  addr_t tmp;
  i32 ret;
  i32 itmp;
  u32 asic_status = 0;
  addr_t mask;
  u32 irq = 0;
  struct DWLReqInfo info = {0};

  ASSERT(dec_cont);

  mask = 15;

  if (!dec_cont->asic_running) {
    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_INTERLACE_E,
                   (dec_cont->storage.pic_layer.fcm != PROGRESSIVE) ? 1 : 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_FIELDMODE_E,
                   (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) ? 1 : 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_B_E,
                   dec_cont->storage.pic_layer.pic_type&2 ? 1 : 0);

    if (dec_cont->storage.pic_layer.pic_type == PTYPE_B) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_FWD_INTERLACE_E,
                     (dec_cont->storage.p_pic_buf[dec_cont->storage.work1].fcm ==
                      PROGRESSIVE) ? 0 : 1);
    } else if (dec_cont->storage.pic_layer.pic_type == PTYPE_P) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_FWD_INTERLACE_E,
                     (dec_cont->storage.p_pic_buf[(i32)dec_cont->storage.work0].fcm ==
                      PROGRESSIVE) ? 0 : 1);
    }

    switch(dec_cont->storage.pic_layer.pic_type) {
    case PTYPE_I:
    case PTYPE_BI:
      tmp = 0;
      break;
    case PTYPE_P:
    case PTYPE_B:
      tmp = 1;
      break;
    default:
      tmp = 1;
    }
    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_INTER_E, tmp);

    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_TOPFIELD_E,
                   dec_cont->storage.pic_layer.is_ff ==
                   dec_cont->storage.pic_layer.tff);
    SetDecRegister(dec_cont->vc1_regs, HWIF_TOPFIELDFIRST_E,
                   dec_cont->storage.pic_layer.tff);


    if (dec_cont->storage.profile == VC1_ADVANCED) {
      u32 pic_width_in_cbs = dec_cont->storage.pic_width_in_mbs << 1;
      u32 pic_height_in_cbs = dec_cont->storage.pic_height_in_mbs << 1;
      if ((dec_cont->storage.cur_coded_width & 0xF) <= 8 &&
          (dec_cont->storage.cur_coded_width & 0xF))
        pic_width_in_cbs--;
      if ((dec_cont->storage.cur_coded_height & 0xF) <= 8 &&
          (dec_cont->storage.cur_coded_height & 0xF))
        pic_height_in_cbs--;
      SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_WIDTH_IN_CBS,
                      pic_width_in_cbs);
      SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_HEIGHT_IN_CBS,
                      pic_height_in_cbs);
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_WIDTH_IN_CBS,
                      dec_cont->storage.pic_width_in_mbs << 1);
      SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_HEIGHT_IN_CBS,
                      dec_cont->storage.pic_height_in_mbs << 1);
    }


    /* set offset only for advanced profile */

    if (dec_cont->storage.profile == VC1_ADVANCED) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_MB_WIDTH_OFF,
                      (dec_cont->storage.cur_coded_width & 0x7));
      SetDecRegister(dec_cont->vc1_regs, HWIF_MB_HEIGHT_OFF,
                      (dec_cont->storage.cur_coded_height & 0x7));
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_MB_WIDTH_OFF, 0);
      SetDecRegister(dec_cont->vc1_regs, HWIF_MB_HEIGHT_OFF, 0);
    }


    if ( (dec_cont->storage.pic_layer.pic_type == PTYPE_P) &&
         (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) &&
         (dec_cont->storage.pic_layer.num_ref == 0) ) {
      /* Current field and reference field shall have same TFF value.
       * If reference frame is coded as a progressive or an interlaced
       * frame then the TFF value of the reference frame shall be
       * assumed to be the same as the TFF value of the current frame
       * [10.3.1] */
      if (dec_cont->storage.pic_layer.ref_field == 0) {
        tmp = dec_cont->storage.pic_layer.top_field ^ 1;
      } else { /* (ref_field == 1) */
        tmp = dec_cont->storage.pic_layer.top_field;
      }
      SetDecRegister(dec_cont->vc1_regs, HWIF_REF_TOPFIELD_E, tmp);
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_REF_TOPFIELD_E, 0 );
    }

    SetDecRegister(dec_cont->vc1_regs, HWIF_FILTERING_DIS,
                   dec_cont->storage.loop_filter ? 0 : 1);
    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_FIXED_QUANT,
                   (dec_cont->storage.dquant == 0) ? 1 : 0);

    if (dec_cont->storage.pic_layer.pic_type == PTYPE_P) {
      if(dec_cont->storage.max_bframes) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_WRITE_MVS_E, 1);
        ASSERT((dec_cont->direct_mvs.bus_address != 0));
      }

    } else if (dec_cont->storage.pic_layer.pic_type == PTYPE_B) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_WRITE_MVS_E, 0);
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_WRITE_MVS_E, 0);
    }
    SetDecRegister(dec_cont->vc1_regs, HWIF_SYNC_MARKER_E,
                   dec_cont->storage.sync_marker);
    SetDecRegister(dec_cont->vc1_regs,
                   HWIF_DQ_PROFILE,
                   dec_cont->storage.pic_layer.dq_profile ==
                   DQPROFILE_ALL_MACROBLOCKS ? 1 : 0 );
    SetDecRegister(dec_cont->vc1_regs,
                   HWIF_DQBI_LEVEL,
                   dec_cont->storage.pic_layer.dqbi_level );

    if (dec_cont->storage.pic_layer.pic_type == PTYPE_B) {
      /* force RANGEREDFRM to be same as in the subsequent anchor frame.
       * Reference decoder seems to work this way when current frame and
       * backward reference frame have different (illagal) values
       * [7.1.1.3] */
      SetDecRegister(dec_cont->vc1_regs, HWIF_RANGE_RED_FRM_E,
                     (u32)(dec_cont->storage.
                           p_pic_buf[(i32)dec_cont->storage.work0].range_red_frm));
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_RANGE_RED_FRM_E,
                     (u32)(dec_cont->storage.
                           p_pic_buf[dec_cont->storage.work_out].range_red_frm));
    }
    SetDecRegister(dec_cont->vc1_regs, HWIF_FAST_UVMC_E,
                   dec_cont->storage.fast_uv_mc);
    SetDecRegister(dec_cont->vc1_regs, HWIF_TRANSDCTAB,
                   dec_cont->storage.pic_layer.intra_transform_dc_index);
    SetDecRegister(dec_cont->vc1_regs, HWIF_TRANSACFRM,
                   dec_cont->storage.pic_layer.ac_coding_set_index_cb_cr);
    SetDecRegister(dec_cont->vc1_regs, HWIF_TRANSACFRM2,
                   dec_cont->storage.pic_layer.ac_coding_set_index_y);
    SetDecRegister(dec_cont->vc1_regs, HWIF_MB_MODE_TAB,
                   dec_cont->storage.pic_layer.mb_mode_tab);
    SetDecRegister(dec_cont->vc1_regs, HWIF_MVTAB,
                   dec_cont->storage.pic_layer.mv_table_index);
    SetDecRegister(dec_cont->vc1_regs, HWIF_CBPTAB,
                   dec_cont->storage.pic_layer.cbp_table_index);
    SetDecRegister(dec_cont->vc1_regs, HWIF_2MV_BLK_PAT_TAB,
                   dec_cont->storage.pic_layer.mvbp_table_index2);
    SetDecRegister(dec_cont->vc1_regs, HWIF_4MV_BLK_PAT_TAB,
                   dec_cont->storage.pic_layer.mvbp_table_index4);


    SetDecRegister(dec_cont->vc1_regs, HWIF_REF_FRAMES,
                   dec_cont->storage.pic_layer.num_ref);

    SetDecRegister(dec_cont->vc1_regs, HWIF_START_CODE_E,
                   (dec_cont->storage.profile == VC1_ADVANCED) ? 1 : 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_INIT_QP,
                   dec_cont->storage.pic_layer.pquant);

    if( dec_cont->storage.pic_layer.pic_type == PTYPE_P) {
      if( dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE0_E, 0 );
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE1_E, 0 );
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE2_E, 0 );
      } else {
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE0_E,
                       dec_cont->storage.pic_layer.raw_mask & MB_SKIPPED ? 0 : 1);
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE1_E,
                       dec_cont->storage.pic_layer.raw_mask & MB_4MV ? 0 : 1);
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE2_E, 0 );
      }
    } else if( dec_cont->storage.pic_layer.pic_type == PTYPE_B ) {
      if (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE0_E,
                       dec_cont->storage.pic_layer.raw_mask & MB_FORWARD_MB ? 0 : 1);
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE1_E, 0 );
      } else {
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE0_E,
                       dec_cont->storage.pic_layer.raw_mask & MB_SKIPPED ? 0 : 1);
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE1_E,
                       dec_cont->storage.pic_layer.raw_mask & MB_DIRECT ? 0 : 1);
      }
      SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE2_E, 0 );
    } else {
      /* acpred flag */
      SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE0_E,
                     dec_cont->storage.pic_layer.raw_mask & MB_AC_PRED ? 0 : 1);
      SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE1_E,
                     dec_cont->storage.pic_layer.raw_mask & MB_OVERLAP_SMOOTH ? 0 : 1);
      if (dec_cont->storage.pic_layer.fcm == FRAME_INTERLACE) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE2_E,
                       dec_cont->storage.pic_layer.raw_mask & MB_FIELD_TX ? 0 : 1);
      } else {
        SetDecRegister(dec_cont->vc1_regs, HWIF_BITPLANE2_E, 0 );
      }
    }
    SetDecRegister(dec_cont->vc1_regs, HWIF_ALT_PQUANT,
                   dec_cont->storage.pic_layer.alt_pquant );
    SetDecRegister(dec_cont->vc1_regs, HWIF_DQ_EDGES,
                   dec_cont->storage.pic_layer.dq_edges );
    SetDecRegister(dec_cont->vc1_regs, HWIF_TTMBF,
                   dec_cont->storage.pic_layer.mb_level_transform_type_flag);
    SetDecRegister(dec_cont->vc1_regs, HWIF_PQINDEX,
                   dec_cont->storage.pic_layer.pq_index);
    SetDecRegister(dec_cont->vc1_regs, HWIF_BILIN_MC_E,
                   dec_cont->storage.pic_layer.pic_type &&
                   (dec_cont->storage.pic_layer.mvmode == MVMODE_1MV_HALFPEL_LINEAR));
    SetDecRegister(dec_cont->vc1_regs, HWIF_UNIQP_E,
                   dec_cont->storage.pic_layer.uniform_quantizer);
    SetDecRegister(dec_cont->vc1_regs, HWIF_HALFQP_E,
                   dec_cont->storage.pic_layer.half_qp);
    SetDecRegister(dec_cont->vc1_regs, HWIF_TTFRM,
                   dec_cont->storage.pic_layer.tt_frm);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DQUANT_E,
                   dec_cont->storage.pic_layer.dquant_in_frame );
    SetDecRegister(dec_cont->vc1_regs, HWIF_VC1_ADV_E,
                   (dec_cont->storage.profile == VC1_ADVANCED) ? 1 : 0);

    if ( (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) &&
         (dec_cont->storage.pic_layer.pic_type == PTYPE_B)) {
      /* Calculate forward and backward reference distances */
      tmp =
        ( dec_cont->storage.pic_layer.ref_dist *
          dec_cont->storage.pic_layer.scale_factor ) >> 8;
      SetDecRegister(dec_cont->vc1_regs, HWIF_REF_DIST_FWD, tmp);
      itmp = (i32) dec_cont->storage.pic_layer.ref_dist - tmp - 1;
      if (itmp < 0) itmp = 0;
      SetDecRegister(dec_cont->vc1_regs, HWIF_REF_DIST_BWD, itmp );
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_REF_DIST_FWD,
                     dec_cont->storage.pic_layer.ref_dist);
      SetDecRegister(dec_cont->vc1_regs, HWIF_REF_DIST_BWD, 0);
    }

    SetDecRegister(dec_cont->vc1_regs, HWIF_MV_SCALEFACTOR,
                   dec_cont->storage.pic_layer.scale_factor );

    if (dec_cont->storage.pic_layer.pic_type != PTYPE_I)
      SetIntensityCompensationParameters(dec_cont);

    tmp = p_strm->strm_curr_pos - p_strm->p_strm_buff_start;
    tmp = strm_bus_addr + tmp;

    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

    SetDecRegister(dec_cont->vc1_regs, HWIF_STRM_START_BIT,
                   8*(tmp & (mask)) + p_strm->bit_pos_in_word);
    tmp = p_strm->strm_buff_size - ((tmp & ~(mask)) - strm_bus_addr);
    SetDecRegister(dec_cont->vc1_regs, HWIF_STREAM_LEN, tmp );
    SetDecRegister(dec_cont->vc1_regs, HWIF_STRM_BUFFER_LEN, tmp);
    SetDecRegister(dec_cont->vc1_regs, HWIF_STRM_START_OFFSET, 0);

    if( dec_cont->storage.profile == VC1_ADVANCED &&
        p_strm->strm_curr_pos > p_strm->p_strm_buff_start &&
        p_strm->p_strm_buff_start + p_strm->strm_buff_size
        - p_strm->strm_curr_pos >= 3 &&
        p_strm->strm_curr_pos[-1] == 0x00 &&
        p_strm->strm_curr_pos[ 0] == 0x00 &&
        p_strm->strm_curr_pos[ 1] == 0x03 ) {
      SetDecRegister(dec_cont->vc1_regs,
                     HWIF_2ND_BYTE_EMUL_E, HANTRO_TRUE );
    } else {
      SetDecRegister(dec_cont->vc1_regs,
                     HWIF_2ND_BYTE_EMUL_E, HANTRO_FALSE );
    }

    DWLDMATransData2(dec_cont->dwl, (addr_t)strm_bus_addr,
                    (void *)p_strm->p_strm_buff_start,
                    p_strm->strm_buff_size, HOST_TO_DEVICE);
    if (dec_cont->storage.pic_layer.top_field) {
      /* start of top field line or frame */
      SET_ADDR_REG(dec_cont->vc1_regs, HWIF_DEC_OUT_BASE,
                   (dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].
                    data.bus_address));
      SetDecRegister( dec_cont->vc1_regs, HWIF_DPB_ILACE_MODE,
                      dec_cont->dpb_mode );
    } else {
      /* start of bottom field line */
      if(dec_cont->dpb_mode == DEC_DPB_FRAME ) {
        SET_ADDR_REG(dec_cont->vc1_regs, HWIF_DEC_OUT_BASE,
                     (dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].
                      data.bus_address + (dec_cont->storage.pic_width_in_mbs<<4)));
      } else if( dec_cont->dpb_mode == DEC_DPB_INTERLACED_FIELD ) {
        SET_ADDR_REG(dec_cont->vc1_regs, HWIF_DEC_OUT_BASE,
                     (dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].
                      data.bus_address));
      } else {
        ASSERT(0);
      }
      SetDecRegister( dec_cont->vc1_regs, HWIF_DPB_ILACE_MODE,
                      dec_cont->dpb_mode );
    }

    SetDecRegister(dec_cont->vc1_regs, HWIF_PP_OUT_E_U, dec_cont->pp_enabled);
    SetDecRegister(dec_cont->vc1_regs, HWIF_UNIQUE_ID, UNIQUE_ID(0));

    if (dec_cont->pp_enabled) {
      PpUnitIntConfig *ppu_cfg = &dec_cont->ppu_cfg[0];
      struct DWLLinearMem *pp_buffer = dec_cont->storage.p_pic_buf[dec_cont->storage.work_out].
                                pp_data;
      u32 bottom_flag = (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) &&
                         !(dec_cont->storage.pic_layer.is_ff ==
                          dec_cont->storage.pic_layer.tff);
      u32 pp_out_ctrl = InputQueueGetPPOutCtrl(dec_cont->pp_buffer_queue, pp_buffer);
      struct PpParams pp_args = {hw_feature,
                                 ppu_cfg,
                                 pp_buffer,
                                 0,
                                 bottom_flag,
                                 pp_out_ctrl
                                };
      InitPpUnitBoundCoeff(hw_feature, (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE), ppu_cfg);
      PPSetRegs(dec_cont->vc1_regs, &pp_args);
      /* Warning: only single core are currently supported (core_id = 0) */
      PPSetLancozsScaleRegs(dec_cont->vc1_regs, hw_feature, ppu_cfg, 0);
      SetDecRegister(dec_cont->vc1_regs, HWIF_PP_IN_FORMAT_U, 1);
    }

    /* Stride registers only available since g1v8_2 */
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_OUT_Y_STRIDE,
                    NEXT_MULTIPLE(dec_cont->storage.pic_width_in_mbs * 4 * 16, ALIGN(dec_cont->align)));
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_OUT_C_STRIDE,
                    NEXT_MULTIPLE(dec_cont->storage.pic_width_in_mbs * 4 * 16, ALIGN(dec_cont->align)));

    if (dec_cont->storage.range_map_yflag) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_PP_RANGEMAP_Y_E_U, 1);
      SetDecRegister(dec_cont->vc1_regs, HWIF_PP_RANGEMAP_COEF_Y_U,
                     dec_cont->storage.range_map_y + 9);
    }
    if (dec_cont->storage.range_map_uv_flag) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_PP_RANGEMAP_C_E_U, 1);
      SetDecRegister(dec_cont->vc1_regs, HWIF_PP_RANGEMAP_COEF_C_U,
                     dec_cont->storage.range_map_uv + 9);
    }

    if (dec_cont->storage.pic_layer.pic_type != PTYPE_I)
      SetReferenceBaseAddress(dec_cont, hw_feature);

    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_HEADER_LEN,
                   dec_cont->storage.pic_layer.pic_header_bits);
    SetDecRegister(dec_cont->vc1_regs, HWIF_PIC_4MV_E,
                   dec_cont->storage.pic_layer.pic_type &&
                   (dec_cont->storage.pic_layer.mvmode == MVMODE_MIXEDMV));

    /* is inter */
    if( dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE ) {
      tmp = (u32)( dec_cont->storage.pic_layer.is_ff ==
                   dec_cont->storage.pic_layer.tff ) ^ 1;
    } else {
      tmp = 0;
    }

    SetDecRegister(dec_cont->vc1_regs, HWIF_PREV_ANC_TYPE,
                   dec_cont->storage.anchor_inter[ tmp ] );

    if (dec_cont->storage.profile != VC1_ADVANCED) {
      if (dec_cont->storage.pic_layer.pic_type == PTYPE_P) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_RANGE_RED_REF_E,
                       (dec_cont->storage.p_pic_buf[(i32)dec_cont->storage.work0].range_red_frm));
      } else if (dec_cont->storage.pic_layer.pic_type == PTYPE_B) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_RANGE_RED_REF_E,
                       (i32)(dec_cont->storage.
                             p_pic_buf[dec_cont->storage.work1].range_red_frm));
      }
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_RANGE_RED_REF_E, 0 );
    }
    if (dec_cont->storage.profile == VC1_ADVANCED) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_VC1_DIFMV_RANGE,
                     dec_cont->storage.pic_layer.dmv_range);
    }
    SetDecRegister(dec_cont->vc1_regs, HWIF_MV_RANGE,
                   dec_cont->storage.pic_layer.mv_range);

    if ( dec_cont->storage.overlap &&
         ((dec_cont->storage.pic_layer.pic_type ==  PTYPE_I) ||
          (dec_cont->storage.pic_layer.pic_type ==  PTYPE_P) ||
          (dec_cont->storage.pic_layer.pic_type ==  PTYPE_BI)) ) {
      /* reset this */
      SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_E, 0);

      if (dec_cont->storage.pic_layer.pquant >= 9) {
        SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_E, 1);
        SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_METHOD, 1);
      } else {
        if(dec_cont->storage.pic_layer.pic_type ==  PTYPE_P ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_E, 0);
          SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_METHOD, 0);
        } else { /* I and BI */
          switch (dec_cont->storage.pic_layer.cond_over) {
          case 0:
            tmp = 0;
            break;
          case 2:
            tmp = 1;
            break;
          case 3:
            tmp = 2;
            break;
          default:
            tmp = 0;
            break;
          }
          SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_METHOD, tmp);
          if (tmp != 0)
            SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_E, 1);
        }
      }
    } else {
      SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_E, 0);
      SetDecRegister(dec_cont->vc1_regs, HWIF_OVERLAP_METHOD, 0);
    }

    SetDecRegister(dec_cont->vc1_regs, HWIF_MV_ACCURACY_FWD,
                   dec_cont->storage.pic_layer.pic_type &&
                   (dec_cont->storage.pic_layer.mvmode == MVMODE_1MV_HALFPEL ||
                    dec_cont->storage.pic_layer.mvmode ==
                    MVMODE_1MV_HALFPEL_LINEAR) ? 0 : 1);
    SetDecRegister(dec_cont->vc1_regs, HWIF_MPEG4_VC1_RC,
                   dec_cont->storage.rnd ? 0 : 1);

    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_BITPL_CTRL_BASE,
                 dec_cont->bit_plane_ctrl.bus_address);

    /* For field pictures, select offset to correct field */
    tmp = dec_cont->direct_mvs.bus_address;

    if( dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE &&
        dec_cont->storage.pic_layer.is_ff != dec_cont->storage.pic_layer.tff ) {
      tmp += 4 * 2 * ((dec_cont->storage.pic_width_in_mbs *
                       ((dec_cont->storage.pic_height_in_mbs+1)/2)+3)&~0x3);
    }

    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_DIR_MV_BASE,
                 tmp);

    info.core_mask = dec_cont->core_mask;
    info.width = dec_cont->storage.cur_coded_width;
    info.height = dec_cont->storage.cur_coded_height;
    info.owner = (void *)dec_cont;
    if (dec_cont->vcmd_used) {
      dec_cont->core_id = 0;
      ret = DWLReserveCmdBuf(dec_cont->dwl, &info, &dec_cont->cmdbuf_id);
    } else {
      ret = DWLReserveHw(dec_cont->dwl, &info, &dec_cont->core_id);
    }
    if (ret != DWL_OK) {
      return X170_DEC_HW_RESERVED;
    }

    /* Reserve HW, enable, release HW */
    dec_cont->asic_running = 1;

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 0x4, 0);
    vc1FlushRegs(dec_cont);

    if (dec_cont->vcmd_used)
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->cmdbuf_id, dec_cont->ppu_cfg, 0);
    else
      DWLReadPpConfigure(dec_cont->dwl, dec_cont->core_id, dec_cont->ppu_cfg, 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_E, 1);
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                  dec_cont->vc1_regs[1]);
  } else { /* buffer was empty and now we restart with new stream values */
    tmp = p_strm->strm_curr_pos - p_strm->p_strm_buff_start;
    tmp = strm_bus_addr + tmp;

    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_RLC_VLC_BASE, tmp & ~(mask));

    SetDecRegister(dec_cont->vc1_regs, HWIF_STRM_START_BIT,
                   8*(tmp & (mask)) + p_strm->bit_pos_in_word);
    SetDecRegister(dec_cont->vc1_regs, HWIF_STREAM_LEN,
                   p_strm->strm_buff_size - ((tmp & ~(mask)) - strm_bus_addr));
    SetDecRegister(dec_cont->vc1_regs, HWIF_STRM_BUFFER_LEN,
                   p_strm->strm_buff_size - ((tmp & ~(mask)) - strm_bus_addr));
    SetDecRegister(dec_cont->vc1_regs, HWIF_STRM_START_OFFSET, 0);

    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 5,
                dec_cont->vc1_regs[5]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 6,
                dec_cont->vc1_regs[6]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 258,
                dec_cont->vc1_regs[258]);
    DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 259,
                dec_cont->vc1_regs[259]);
    if (IS_LEGACY(dec_cont->vc1_regs[0]))
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 12, dec_cont->vc1_regs[12]);
    else
      DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 169, dec_cont->vc1_regs[169]);
    if (sizeof(addr_t) == 8) {
      if(hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->vc1_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, dec_cont->vc1_regs[122]);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, dec_cont->vc1_regs[168]);
      } else {
        ASSERT(dec_cont->vc1_regs[122] == 0);
        ASSERT(dec_cont->vc1_regs[168] == 0);
      }
    } else {
      if(hw_feature->addr64_support) {
        if (IS_LEGACY(dec_cont->vc1_regs[0]))
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 122, 0);
        else
          DWLWriteReg(dec_cont->dwl, dec_cont->core_id, 4 * 168, 0);
      }
    }
    /* start HW by clearing IRQ_BUFFER_EMPTY status bit */
    if (dec_cont->vcmd_used)
      DWLEnableCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
    else
      DWLEnableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1, dec_cont->vc1_regs[1]);
  }

  if (dec_cont->vcmd_used)
    ret = DWLWaitCmdBufReady(dec_cont->dwl, dec_cont->cmdbuf_id);
  else
    ret = DWLWaitHwReady(dec_cont->dwl, dec_cont->core_id, (u32)DEC_X170_TIMEOUT_LENGTH);

  if (dec_cont->enable_3dlut)
    DWLDMATransData(dec_cont->dwl, &dec_cont->ppu_cfg[0].table_3dlut_buffer, 0,
                    dec_cont->ppu_cfg[0].table_3dlut_buffer.size, HOST_TO_DEVICE);
  vc1RefreshRegs(dec_cont);

  /* Get current stream position from HW */
  tmp = GET_ADDR_REG(dec_cont->vc1_regs, HWIF_RLC_VLC_BASE);


  if (ret == DWL_HW_WAIT_OK) {
    asic_status = GetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT);
  } else {
    /* reset HW */
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ, 0); /* just in case */
    SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_E, 0);
    if (!dec_cont->vcmd_used) {
      DWLDisableHw(dec_cont->dwl, dec_cont->core_id, 4 * 1,
                   dec_cont->vc1_regs[1]);
    }

    asic_status = (ret == DWL_HW_WAIT_TIMEOUT) ?
                  X170_DEC_TIMEOUT : X170_DEC_SYSTEM_ERROR;

    return (asic_status);
  }

  if(asic_status & (DEC_HW_IRQ_BUFFER|DEC_HW_IRQ_RDY)) {
    if ( (tmp - strm_bus_addr) <= (strm_bus_addr + p_strm->strm_buff_size)) {
      /* current stream position is in the valid range */
      p_strm->strm_curr_pos =
        p_strm->p_strm_buff_start +
        (tmp - strm_bus_addr);
    } else {
      /* error situation, consider that whole stream buffer was consumed */
      p_strm->strm_curr_pos = p_strm->p_strm_buff_start + p_strm->strm_buff_size;
    }

    p_strm->strm_buff_read_bits =
      8 * (p_strm->strm_curr_pos - p_strm->p_strm_buff_start);
    p_strm->bit_pos_in_word = 0;
  }

  SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ_STAT, 0);
  SetDecRegister(dec_cont->vc1_regs, HWIF_DEC_IRQ, 0);    /* just in case */

  if (!(asic_status & DEC_HW_IRQ_BUFFER)) {
    if (dec_cont->vcmd_used) {
      irq = DWLReleaseCmdBuf(dec_cont->dwl, dec_cont->cmdbuf_id);
      UNUSED(irq);
    } else {
      (void)DWLReleaseHw(dec_cont->dwl, dec_cont->core_id);
    }
    /* HW done, release it! */
    dec_cont->asic_running = 0;
  }

  return(asic_status);

}



/*------------------------------------------------------------------------------

    Function name: SetReferenceBaseAddress

        Functional description:
            Updates base addresses of reference pictures

        Inputs:
            dec_cont        Decoder container

        Outputs:
            None

        Returns:
            None

------------------------------------------------------------------------------*/
void SetReferenceBaseAddress(decContainer_t *dec_cont,
                             const struct DecHwFeatures *hw_feature) {
  u32 tmp, tmp1, tmp2;

  if (dec_cont->storage.max_bframes) {
    /* forward reference */
    if (dec_cont->storage.pic_layer.pic_type == PTYPE_B) {
      if ( (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) ) {
        if (dec_cont->storage.pic_layer.is_ff == HANTRO_TRUE) {
          /* Use 1st and 2nd fields of temporally
           * previous anchor frame */
          SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                       (dec_cont->storage.
                        p_pic_buf[dec_cont->storage.work1].
                        data.bus_address));
          SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                       (dec_cont->storage.
                        p_pic_buf[dec_cont->storage.work1].
                        data.bus_address));
        } else {
          /* Use 1st field of current frame and 2nd
           * field of prev frame */
          if( dec_cont->storage.pic_layer.tff ) {
            SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                         (dec_cont->storage.
                          p_pic_buf[dec_cont->storage.work_out].
                          data.bus_address));
            SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                         (dec_cont->storage.
                          p_pic_buf[dec_cont->storage.work1].
                          data.bus_address));
          } else {
            SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                         (dec_cont->storage.
                          p_pic_buf[dec_cont->storage.work1].
                          data.bus_address));
            SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                         (dec_cont->storage.
                          p_pic_buf[dec_cont->storage.work_out].
                          data.bus_address));
          }
        }
      } else {
        SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                     (dec_cont->storage.p_pic_buf[dec_cont->storage.work1].
                      data.bus_address));
        SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                     (dec_cont->storage.p_pic_buf[dec_cont->storage.work1].
                      data.bus_address));

      }
    } else { /* PTYPE_P */
      if ( (dec_cont->storage.pic_layer.fcm == FIELD_INTERLACE) ) {
        if( dec_cont->storage.pic_layer.num_ref == 0 ) {
          if( dec_cont->storage.pic_layer.is_ff == HANTRO_FALSE &&
              dec_cont->storage.pic_layer.ref_field == 0 ) {
            tmp = dec_cont->storage.work_out;
          } else {
            tmp = dec_cont->storage.work0;
          }
          SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                       (dec_cont->storage.p_pic_buf[tmp].data.bus_address));
          SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                       (dec_cont->storage.p_pic_buf[tmp].data.bus_address));
        } else { /* NUMREF == 1 */
          /* check that reference available */
          if( dec_cont->storage.work0 == INVALID_ANCHOR_PICTURE )
            tmp2 = dec_cont->storage.work_out_prev;
          else
            tmp2 = dec_cont->storage.work0;

          if( dec_cont->storage.pic_layer.is_ff == HANTRO_TRUE ) {
            /* Use both fields from previous frame */
            tmp = tmp2;
            tmp1 = tmp2;
          } else {
            /* Use previous field from this frame and second field
             * from previous frame */
            if( dec_cont->storage.pic_layer.tff ) {
              tmp = dec_cont->storage.work_out;
              tmp1 = tmp2;
            } else {
              tmp = tmp2;
              tmp1 = dec_cont->storage.work_out;
            }
          }
          SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                       (dec_cont->storage.p_pic_buf[tmp].data.bus_address));
          SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                       (dec_cont->storage.p_pic_buf[tmp1].data.bus_address));
        }
      } else {
        SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                     (dec_cont->storage.p_pic_buf[(i32)dec_cont->storage.work0].
                      data.bus_address));
        SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE,
                     (dec_cont->storage.p_pic_buf[(i32)dec_cont->storage.work0].
                      data.bus_address));
      }
    }

    /* check that reference available */
    if( dec_cont->storage.work0 == INVALID_ANCHOR_PICTURE )
      tmp2 = dec_cont->storage.work_out_prev;
    else
      tmp2 = dec_cont->storage.work0;

    /* backward reference */
    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER2_BASE,
                 (dec_cont->storage.p_pic_buf[tmp2].
                  data.bus_address));

    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER3_BASE,
                 (dec_cont->storage.p_pic_buf[tmp2].
                  data.bus_address));

  } else {
    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER0_BASE,
                 (dec_cont->storage.p_pic_buf[(i32)dec_cont->storage.work0].
                  data.bus_address));
    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER1_BASE, (addr_t)0);
    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER2_BASE, (addr_t)0);
    SET_ADDR_REG(dec_cont->vc1_regs, HWIF_REFER3_BASE, (addr_t)0);
  }

}

/*------------------------------------------------------------------------------

    Function name: SetIntensityCompensationParameters

        Functional description:
            Updates intensity compensation parameters

        Inputs:
            dec_cont        Decoder container

        Outputs:
            None

        Returns:
            None

------------------------------------------------------------------------------*/
void SetIntensityCompensationParameters(decContainer_t *dec_cont) {
  picture_t *p_pic;
  i32 w0;
  i32 w1;
  u32 first_top;

  p_pic = (picture_t*)dec_cont->storage.p_pic_buf;

  if (dec_cont->storage.pic_layer.pic_type == PTYPE_B) {
    w0   = dec_cont->storage.work0;
    w1   = dec_cont->storage.work1;
  } else {
    w0   = dec_cont->storage.work_out;
    w1   = dec_cont->storage.work0;
  }

  /* first disable all */
  SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP0_E, 0);
  SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 0);
  SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP2_E, 0);
  SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP3_E, 0);
  SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP4_E, 0);

  SetDecRegister(dec_cont->vc1_regs, HWIF_REFTOPFIRST_E,
                 p_pic[w1].is_top_field_first);

  /* frame coded */
  if (p_pic[w0].fcm != FIELD_INTERLACE) {
    /* set first */
    if (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) {
      SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP0_E, 1);
      SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE0,
                     (u32)p_pic[w0].field[0].i_scale_a);
      SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT0,
                     (u32)p_pic[w0].field[0].i_shift_a);
    }
    /* field picture */
    if (p_pic[w1].fcm == FIELD_INTERLACE) {
      /* set second */
      if (p_pic[w1].is_top_field_first) {
        if ( (p_pic[w1].field[1].int_comp_f == IC_BOTH_FIELDS) ||
             (p_pic[w1].field[1].int_comp_f == IC_TOP_FIELD) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE1,
                         (u32)p_pic[w1].field[1].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT1,
                         (u32)p_pic[w1].field[1].i_shift_a);
        }
      } else { /* bottom field first */
        if ( (p_pic[w1].field[1].int_comp_f == IC_BOTH_FIELDS) ||
             (p_pic[w1].field[1].int_comp_f == IC_BOTTOM_FIELD) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE1,
                         (u32)p_pic[w1].field[1].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT1,
                         (u32)p_pic[w1].field[1].i_shift_b);
        }
      }
    }
  } else { /* field */
    if (p_pic[w0].is_first_field) {
      if (p_pic[w0].is_top_field_first) {
        first_top = HANTRO_TRUE;
        if ( (p_pic[w0].field[0].int_comp_f == IC_BOTTOM_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP0_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE0,
                         (u32)p_pic[w0].field[0].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT0,
                         (u32)p_pic[w0].field[0].i_shift_b);
        }

        if ( (p_pic[w0].field[0].int_comp_f == IC_TOP_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE1,
                         (u32)p_pic[w0].field[0].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT1,
                         (u32)p_pic[w0].field[0].i_shift_a);
        }

      } else { /* bottom field first */
        first_top = HANTRO_FALSE;
        if ( (p_pic[w0].field[0].int_comp_f == IC_TOP_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP0_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE0,
                         (u32)p_pic[w0].field[0].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT0,
                         (u32)p_pic[w0].field[0].i_shift_a);
        }

        if ( (p_pic[w0].field[0].int_comp_f == IC_BOTTOM_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE1,
                         (u32)p_pic[w0].field[0].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT1,
                         (u32)p_pic[w0].field[0].i_shift_b);
        }
      }

      if (p_pic[w1].fcm == FIELD_INTERLACE) {
        if (first_top) {
          if ( (p_pic[w1].field[1].int_comp_f == IC_TOP_FIELD) ||
               (p_pic[w1].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
            SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP2_E, 1);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE2,
                           (u32)p_pic[w1].field[1].i_scale_a);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT2,
                           (u32)p_pic[w1].field[1].i_shift_a);
          }
        } else {
          if ( (p_pic[w1].field[1].int_comp_f == IC_BOTTOM_FIELD) ||
               (p_pic[w1].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
            SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP2_E, 1);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE2,
                           (u32)p_pic[w1].field[1].i_scale_b);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT2,
                           (u32)p_pic[w1].field[1].i_shift_b);
          }
        }
      }
    } else { /* second field */
      if (p_pic[w0].is_top_field_first) {
        first_top = HANTRO_TRUE;
        if ( (p_pic[w0].field[1].int_comp_f == IC_TOP_FIELD) ||
             (p_pic[w0].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP0_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE0,
                         (u32)p_pic[w0].field[1].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT0,
                         (u32)p_pic[w0].field[1].i_shift_a);
        }

        if ( (p_pic[w0].field[1].int_comp_f == IC_BOTTOM_FIELD) ||
             (p_pic[w0].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE1,
                         (u32)p_pic[w0].field[1].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT1,
                         (u32)p_pic[w0].field[1].i_shift_b);
        }

        if ( (p_pic[w0].field[0].int_comp_f == IC_BOTTOM_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP2_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE2,
                         (u32)p_pic[w0].field[0].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT2,
                         (u32)p_pic[w0].field[0].i_shift_b);
        }

        if ( (p_pic[w0].field[0].int_comp_f == IC_TOP_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP3_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE3,
                         (u32)p_pic[w0].field[0].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT3,
                         (u32)p_pic[w0].field[0].i_shift_a);
        }

        if (p_pic[w1].fcm == FIELD_INTERLACE) {
          if ( (p_pic[w1].field[1].int_comp_f == IC_TOP_FIELD) ||
               (p_pic[w1].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
            SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP4_E, 1);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE4,
                           (u32)p_pic[w1].field[1].i_scale_a);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT4,
                           (u32)p_pic[w1].field[1].i_shift_a);
          }
        }

      } else { /* bottom field first */
        first_top = HANTRO_FALSE;
        if ( (p_pic[w0].field[1].int_comp_f == IC_BOTTOM_FIELD) ||
             (p_pic[w0].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP0_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE0,
                         (u32)p_pic[w0].field[1].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT0,
                         (u32)p_pic[w0].field[1].i_shift_b);
        }

        if ( (p_pic[w0].field[1].int_comp_f == IC_TOP_FIELD) ||
             (p_pic[w0].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP1_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE1,
                         (u32)p_pic[w0].field[1].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT1,
                         (u32)p_pic[w0].field[1].i_shift_a);
        }

        if ( (p_pic[w0].field[0].int_comp_f == IC_TOP_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP2_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE2,
                         (u32)p_pic[w0].field[0].i_scale_a);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT2,
                         (u32)p_pic[w0].field[0].i_shift_a);
        }

        if ( (p_pic[w0].field[0].int_comp_f == IC_BOTTOM_FIELD) ||
             (p_pic[w0].field[0].int_comp_f == IC_BOTH_FIELDS) ) {
          SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP3_E, 1);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE3,
                         (u32)p_pic[w0].field[0].i_scale_b);
          SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT3,
                         (u32)p_pic[w0].field[0].i_shift_b);
        }

        if (p_pic[w1].fcm == FIELD_INTERLACE) {
          if ( (p_pic[w1].field[1].int_comp_f == IC_BOTTOM_FIELD) ||
               (p_pic[w1].field[1].int_comp_f == IC_BOTH_FIELDS) ) {
            SetDecRegister(dec_cont->vc1_regs, HWIF_ICOMP4_E, 1);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISCALE4,
                           (u32)p_pic[w1].field[1].i_scale_b);
            SetDecRegister(dec_cont->vc1_regs, HWIF_ISHIFT4,
                           (u32)p_pic[w1].field[1].i_shift_b);
          }
        }

      }
    }
  }
}
#ifdef _DEC_PP_USAGE
/*------------------------------------------------------------------------------

    Function name: PrintDecPpUsage

        Functional description:

        Inputs:
            dec_cont    Pointer to decoder structure
            ff          Is current field first or second field of the frame
            pic_index    Picture buffer index
            dec_status   DEC / PP print
            pic_id       Picture ID of the field/frame

        Outputs:
            None

        Returns:
            None

------------------------------------------------------------------------------*/
void PrintDecPpUsage( decContainer_t *dec_cont,
                      u32 ff,
                      u32 pic_index,
                      u32 dec_status,
                      u32 pic_id) {
  FILE *fp;
  picture_t *p_pic;
  p_pic = (picture_t*)dec_cont->storage.p_pic_buf;

  fp = fopen("dec_pp_usage.txt", "at");
  if (fp == NULL)
    return;

  if (dec_status) {
    if (dec_cont->storage.pic_layer.is_ff) {
      fprintf(fp, "\n======================================================================\n");

      fprintf(fp, "%10s%10s%10s%10s%10s%10s%10s\n",
              "Component", "PicId", "PicType", "Fcm", "Field",
              "PPMode", "BuffIdx");
    }
    /* Component and PicId */
    fprintf(fp, "\n%10.10s%10u", "DEC", pic_id);
    /* Pictype */
    switch (dec_cont->storage.pic_layer.pic_type) {
    case PTYPE_P:
      fprintf(fp, "%10s","P");
      break;
    case PTYPE_I:
      fprintf(fp, "%10s","I");
      break;
    case PTYPE_B:
      fprintf(fp, "%10s","B");
      break;
    case PTYPE_BI:
      fprintf(fp, "%10s","BI");
      break;
    case PTYPE_Skip:
      fprintf(fp, "%10s","Skip");
      break;
    }
    /* Frame coding mode */
    switch (dec_cont->storage.pic_layer.fcm) {
    case PROGRESSIVE:
      fprintf(fp, "%10s","PR");
      break;
    case FRAME_INTERLACE:
      fprintf(fp, "%10s","FR");
      break;
    case FIELD_INTERLACE:
      fprintf(fp, "%10s","FI");
      break;
    }
    /* Field */
    if (dec_cont->storage.pic_layer.top_field)
      fprintf(fp, "%10s","TOP");
    else
      fprintf(fp, "%10s","BOT");

    /* PPMode and BuffIdx */
    /* fprintf(fp, "%10s%10d\n", "---",pic_index);*/
    switch (dec_cont->pp_control.multi_buf_stat) {
    case MULTIBUFFER_FULLMODE:
      fprintf(fp, "%10s%10u\n", "FULL",pic_index);
      break;
    case MULTIBUFFER_SEMIMODE:
      fprintf(fp, "%10s%10u\n", "SEMI",pic_index);
      break;
    case MULTIBUFFER_DISABLED:
      fprintf(fp, "%10s%10u\n", "DISA",pic_index);
      break;
    case MULTIBUFFER_UNINIT:
      fprintf(fp, "%10s%10u\n", "UNIN",pic_index);
      break;
    default:
      break;
    }
  } else { /* pp status */
    /* Component and PicId */
    fprintf(fp, "%10s%10u", "PP", pic_id);
    /* Pictype */
    switch (p_pic[pic_index].field[!ff].type) {
    case PTYPE_P:
      fprintf(fp, "%10s","P");
      break;
    case PTYPE_I:
      fprintf(fp, "%10s","I");
      break;
    case PTYPE_B:
      fprintf(fp, "%10s","B");
      break;
    case PTYPE_BI:
      fprintf(fp, "%10s","BI");
      break;
    case PTYPE_Skip:
      fprintf(fp, "%10s","Skip");
      break;
    }
    /* Frame coding mode */
    switch (p_pic[pic_index].fcm) {
    case PROGRESSIVE:
      fprintf(fp, "%10s","PR");
      break;
    case FRAME_INTERLACE:
      fprintf(fp, "%10s","FR");
      break;
    case FIELD_INTERLACE:
      fprintf(fp, "%10s","FI");
      break;
    }
    /* Field */
    if (p_pic[pic_index].is_top_field_first == ff)
      fprintf(fp, "%10s","TOP");
    else
      fprintf(fp, "%10s","BOT");

    /* PPMode and BuffIdx */
    switch (p_pic[pic_index].field[!ff].dec_pp_stat) {
    case STAND_ALONE:
      fprintf(fp, "%10s%10u\n", "STAND",pic_index);
      break;
    case PARALLEL:
      fprintf(fp, "%10s%10u\n", "PARAL",pic_index);
      break;
    case PIPELINED:
      fprintf(fp, "%10s%10u\n", "PIPEL",pic_index);
      break;
    default:
      break;
    }
  }

  if (fp) {
    fclose(fp);
    fp = NULL;
  }
}
#endif


u32 VC1GetRefFrmSize(decContainer_t *dec_cont) {
  u32 pic_size;
  u32 out_w, out_h;

  out_w = NEXT_MULTIPLE(4 * dec_cont->storage.pic_width_in_mbs * 16,
                            ALIGN(dec_cont->align));
  out_h = dec_cont->storage.pic_height_in_mbs * 4;
  pic_size = out_w * out_h * 3 / 2;

  return pic_size;
}
