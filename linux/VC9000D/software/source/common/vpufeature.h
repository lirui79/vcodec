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

#ifndef __VPU_FEATURES__
#define __VPU_FEATURES__

#include "basetype.h"
#include "vpu_features_struct.h"

void GetReleaseHwFeaturesByID(u32 hw_build_id, const struct DecHwFeatures **hw_feature);

#endif /* __VPU_FEATURES__ */
