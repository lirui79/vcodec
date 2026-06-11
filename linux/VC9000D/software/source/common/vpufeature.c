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

#include "vpufeature.h"
#include "sw_util.h"
#include "dec_log.h"
#include "parselogmsg.h"
struct DecHwFeatures feature_list[] = {
#include "vpu_features_list.h"
};

static int flag = 0;
extern int num_pp_flag;

void GetReleaseHwFeaturesByID(u32 hw_build_id, const struct DecHwFeatures **hw_feature) {
  struct DecHwFeatures *cfg = feature_list;
  u32 i = 0, feature_list_num;

  if (hw_feature == NULL) return;

  feature_list_num = sizeof(feature_list)/sizeof(struct DecHwFeatures);
  for (i = 0; i < feature_list_num; i++) {
    if ((hw_build_id & cfg->id_mask) == cfg->id)
      break;

    cfg++;
  }
  if (i == feature_list_num) {
    APITRACEERR("Get Hw feature of HW BUILD ID %d failed\n", hw_build_id); // not should happen
    // cfg = NULL;
  }

  *hw_feature = cfg;
  if (flag == 0 && num_pp_flag == 1) {
    ParseFeatureList(cfg);
    flag += 1;
  }
}

u32 CheckHwCfgAgainstRelease(u32 id, struct DecHwFeatures *hw_feature) {
  UNUSED(id);
  UNUSED(hw_feature);
  return 0;
}

