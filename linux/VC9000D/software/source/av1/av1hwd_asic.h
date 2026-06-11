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

#ifndef __AV1HWD_ASIC_H__
#define __AV1HWD_ASIC_H__

#include "av1hwd_container.h"
#include "basetype.h"
#include "decapicommon.h"

#define AV1HWDEC_HW_RESERVED 0x0100
#define AV1HWDEC_SYSTEM_ERROR 0x0200
#define AV1HWDEC_SYSTEM_TIMEOUT 0x0300

#if 0
#define DEC_HW_ALIGN_MASK (32 - 1)  // 256 bus

#define DEC_HW_IRQ_RDY 0x01
#define DEC_HW_IRQ_BUS 0x02
#define DEC_HW_IRQ_ERROR 0x04
#define DEC_HW_IRQ_TIMEOUT 0x08
#define DEC_HW_IRQ_EXT_TIMEOUT 0x10
#define DEC_HW_IRQ_RESI_LOST 0x20
#endif
struct JobData1 {
  i32 core_id;  // core id used for his job
  void* dwl;    // pointer to dwl instance

  /* VCMD */
  u32 cmdbuf_id;
};

void Av1AsicInit(const struct DWLCodecConfig *config,
                 struct Av1DecContainer *dec_cont,
                 const int multicore_poll_period);
i32 Av1AsicAllocateMem(const void *dwl, struct DecAsicBuffers *asic_buff, u32 secure_mode);
void Av1AsicReleaseMem(const void *dwl, struct DecAsicBuffers *asic_buff);
void Av1SetExternalBufferInfo(struct Av1DecContainer *dec_cont);

void Av1ReleaseLowLatencyMem(struct Av1DecContainer *dec_cont);
i32 Av1AllocatLowLatencyMem(struct Av1DecContainer *dec_cont);

i32 Av1AsicAllocateFilterBlockMem(const void *dwl, struct Av1Decoder *dec, struct DecAsicBuffers *asic_buff, PpUnitIntConfig *ppu_cfg, u32 use_multicore_backup, u32 secure_mode);
void Av1AsicReleaseFilterBlockMem(const void *dwl, struct DecAsicBuffers *asic_buff);
i32 Av1AsicAllocateFbcMem(struct Av1DecContainer *dec_cont);
void Av1AsicReleaseFbcMem(struct Av1DecContainer *dec_cont);
i32 Av1AsicAllocatePictures(struct Av1DecContainer *dec_cont);
void Av1AsicReleasePictures(struct Av1DecContainer *dec_cont);
i32 Av1AllocParasiticBuf(const void *dwl, u32 size, struct DWLLinearMem *info, u32 secure_mode);
void Av1ReleaseParasiticBuf(const void *dwl, struct DWLLinearMem *info);
i32 Av1ReleaseParasiticBufs(struct Av1DecContainer *dec_cont);
i32 Av1AllocateFrame(struct Av1DecContainer *dec_cont, u32 index);
i32 Av1ReallocateFrame(struct Av1DecContainer *dec_cont, u32 index);
void Av1AsicInitPicture(struct Av1DecContainer *dec_cont, const struct Av1DecInput *input);
void Av1AsicStrmPosUpdate(struct Av1DecContainer *dec_cont, addr_t bus_address,
                          u32 data_len, addr_t buf_bus_address, u32 buf_len);
u32 Av1AsicRun(struct Av1DecContainer *dec_cont, u32 pic_id);
u32 Av1AsicSync(struct Av1DecContainer *dec_cont);

void Av1AsicProbUpdate(struct Av1Decoder *dec,
    struct DecAsicBuffers *asic_buff,
    struct SwRegisters *sw_ctrl,
    u32 vcmd_m2m);

void Av1UpdateRefs(struct Av1DecContainer *dec_cont);

i32 Av1GetRefFrm(struct Av1DecContainer *dec_cont, const struct Av1DecInput *input);
void Av1UpdateProbabilities(struct Av1DecContainer *dec_cont);

void Av1AsicReset(struct Av1DecContainer *dec_cont);

#endif /* __AV1HWD_ASIC_H__ */
