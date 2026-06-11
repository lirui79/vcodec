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

#ifndef VCD_TOOL_H_
#define VCD_TOOL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "basetype.h"
#include "sw_stream.h"
#include "sw_util.h"
#include "dectypes.h"

#ifdef _ASSERT_USED
#ifndef ASSERT
#define ASSERT(expr) assert(expr)
#endif
#else
#define ASSERT(expr)
#endif

struct TOOL_PARAMS {
  struct DWLLinearMem *ext_buffers;
  u32 *max_buffers;
  const void *dwl_inst;
  // u32 *use_mp_output;
  // u32 use_separated_pp;
  u32 init_value;
  u32 cur_buf_index;
  // u32 CBPS;
};

u32 VcdExpGolombUnsigned(struct StrmData *stream, u32 *value); /* ue(v) */
// u32 VcdExpGolombSigned(struct StrmData *stream, i32 *value);

void SwMatchOuputBufferId(const struct TOOL_PARAMS *tool_params, const struct DWLLinearMem *out_buf,
                          u32 *ext_id);
void SwSetHostOutbaseAfterDma(u32* host_base, struct DecPicture* pic, struct DWLLinearMem* buf);
void SwClearHostOutbaseAfterWriteFile(u32* host_base, struct DecPicture* pic);
void SetHostBase(struct DWLLinearMem* buf, u32* host_base, addr_t device_base);

#ifdef __cplusplus
}
#endif

#endif /* #ifdef VCD_TOOL_H_ */
