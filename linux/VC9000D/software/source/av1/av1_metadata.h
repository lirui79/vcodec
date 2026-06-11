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

#ifndef _AV1_METADATA_OBU_H_
#define _AV1_METADATA_OBU_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "dectypes.h"
#include "decsei.h"

void Av1UpdateMetadataInfo(struct SEI_buffer *sei_buffer, struct MetadataParameters *sei_param_curr);

#ifdef __cplusplus
}
#endif

#endif  // _AV1_METADATA_OBU_H_
