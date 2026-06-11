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

#ifndef __DELOGO_U__
#define __DELOGO_U__

#include "basetype.h"
#include "decapicommon.h"
#include "vpufeature.h"
u32 CheckDelogo(DelogoConfig *delogo_cfg, u32 luma_bit_depth, u32 chroma_bit_depth);
void DelogoSetRegs(u32 *pp_regs,
                   const struct DecHwFeatures *p_hw_feature,
                   DelogoConfig *delogo_cfg);
#endif
