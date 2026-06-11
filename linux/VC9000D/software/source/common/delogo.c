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
------------------------------------------------------------------------------*/

#include "deccfg.h"
#include "regdrv.h"
#include "commonconfig.h"
#include "sw_util.h"
#include "sw_debug.h"
#include "delogo.h"

#ifdef DELOGO_SUPPORT

/* check the delogo area is overlaped. */
u32 CheckDelogo(DelogoConfig *delogo_cfg, u32 luma_bit_depth, u32 chroma_bit_depth) {
  u32 luma_max_value = (1 << luma_bit_depth) - 1;
  u32 chroma_max_value = (1 << chroma_bit_depth) - 1;
  if (delogo_cfg[0].Y > luma_max_value || delogo_cfg[0].U > chroma_max_value ||
      delogo_cfg[0].V > chroma_max_value || delogo_cfg[1].Y > luma_max_value ||
      delogo_cfg[1].U > chroma_max_value || delogo_cfg[1].V > chroma_max_value)
    return 1;
  if (delogo_cfg[0].enabled && ((delogo_cfg[0].w > 512 || delogo_cfg[0].w < 8) ||
      (delogo_cfg[0].h > 256 || delogo_cfg[0].h < 4)))
    return 1;
  if (delogo_cfg[1].enabled && (delogo_cfg[1].w < 8 || delogo_cfg[1].h < 4))
    return 1;
  if (delogo_cfg[0].enabled && (delogo_cfg[0].x < 1 || delogo_cfg[0].y < 1))
    return 1;
  if (delogo_cfg[1].enabled && (delogo_cfg[1].x < 1 || delogo_cfg[1].y < 1))
    return 1;
  if (delogo_cfg[0].enabled && ((delogo_cfg[0].x & 0x1) || (delogo_cfg[0].y & 0x1) ||
      (delogo_cfg[0].w & 0x1) || (delogo_cfg[0].h & 0x1)))
    return 1;
  if (delogo_cfg[1].enabled && ((delogo_cfg[1].x & 0x1) || (delogo_cfg[1].y & 0x1) ||
      (delogo_cfg[1].w & 0x1) || (delogo_cfg[1].h & 0x1)))
    return 1;
  if ((delogo_cfg[0].x + delogo_cfg[0].w > delogo_cfg[1].x) &&
      (delogo_cfg[1].x + delogo_cfg[1].w > delogo_cfg[0].x) &&
      (delogo_cfg[0].y + delogo_cfg[0].h > delogo_cfg[1].y) &&
      (delogo_cfg[1].y + delogo_cfg[1].h > delogo_cfg[0].y))
    return 1;
  else
    return 0;
}

void DelogoSetRegs(u32 *pp_regs,
                   const struct DecHwFeatures *p_hw_feature,
                   DelogoConfig *delogo_cfg) {
}

#else

u32 CheckDelogo(DelogoConfig *delogo_cfg, u32 luma_bit_depth, u32 chroma_bit_depth) {
  return 0;
}
void DelogoSetRegs(u32 *pp_regs,
                   const struct DecHwFeatures *p_hw_feature,
                   DelogoConfig *delogo_cfg) {
}

#endif
