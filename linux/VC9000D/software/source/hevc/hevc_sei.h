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

#ifndef HEVC_SEI_H_
#define HEVC_SEI_H_

#include "basetype.h"
#include "sw_stream.h"
#include "hevc_cfg.h"
#include "hevc_seq_param_set.h"
#include "decsei.h" /* sei_param */

/**
 * \enum HevcSEIType
 * \brief SEI types of HEVC and corresponding num.
 * \ingroup hevc_group
 */
enum HevcSEIType {
  SEI_BUFFERING_PERIOD = 0,
  SEI_PIC_TIMING = 1,
  SEI_PAN_SCAN_RECT = 2,
  SEI_FILLER_PAYLOAD = 3,
  SEI_USER_DATA_REGISTERED_ITU_T_T35 = 4,
  SEI_USER_DATA_UNREGISTERED = 5,
  SEI_RECOVERY_POINT = 6,
  SEI_SCENE_INFO = 9,
  SEI_PICTURE_SNAPSHOT = 15,
  SEI_PROGRESSIVE_REFINEMENT_SEGMENT_START = 16,
  SEI_PROGRESSIVE_REFINEMENT_SEGMENT_END = 17,
  SEI_FILM_GRAIN_CHARACTERISTICS = 19,
  SEI_POST_FILTER_HINTS = 22,
  SEI_TONE_MAPPING_INFO = 23,
  SEI_FRAME_PACKING_ARRANGEMENT = 45,
  SEI_DISPLAY_ORIENTATION = 47,
  SEI_STRUCTURE_OF_PICTURES_INFO = 128,
  SEI_ACTIVE_PARAMETER_SETS = 129,
  SEI_DECODING_UNIT_INFO = 130,
  SEI_TEMPORAL_SUB_LAYER_ZERO_INDEX = 131,
  SEI_DECODED_PICTURE_HASH = 132,
  SEI_SCALABLE_NESTING = 133,
  SEI_REGION_REFRESH_INFO = 134,
  SEI_TIME_CODE = 136,
  SEI_MASTERING_DISPLAY_COLOR_VOLUME = 137,
  SEI_CONTENT_LIGHT_LEVEL_INFO  = 144,
  SEI_ALTERNATIVE_TRANSFER_CHARACTERISTICS = 147,
  SEI_ALPHA_CHANNEL_INFO = 165
};

u32 HevcPrepareCurrentSEIParameters(struct HevcSEIParameters **sei_param,
                                    struct HevcSEIParameters **sei_param_curr,
                                    u32 sei_param_num, u32 pic_id);

u32 HevcAllocateSEIParameters(struct HevcSEIParameters **sei_param,
                              struct HevcSEIParameters **sei_param_curr,
                              u32 *sei_param_num, u32 ext_buffer_num);

u32 HevcResetSEIParameters(struct HevcSEIParameters *sei_param_curr,
                           u32 sei_param_num, u32 pic_id);

u32 HevcDecodeSEIParameters(struct StrmData *stream, int layerid,
                            struct HevcSEIParameters *sei_params,
                            struct SeqParamSet **sps, u32 decode_id);

u32 HevcGetSEIStreamDatas(struct StrmData *stream,
                          struct SEI_buffer *sei_buffer);

void HevcUpdateSeiInfo(struct SEI_buffer *sei_buffer, struct HevcSEIParameters *sei_param_curr);

void HevcSetSeiUnused(struct HevcSEIParameters *sei_param);
#endif /* #ifdef HEVC_SEI_H */
