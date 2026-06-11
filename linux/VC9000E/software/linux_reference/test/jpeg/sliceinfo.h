/*-------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2023 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------*/
#ifndef ENC_INPUTSLICEINFO_H
#define ENC_INPUTSLICEINFO_H

#ifdef __cplusplus
extern "C" {
#endif

#include "base_type.h"
#include "enccommon.h"

#define LINE_CNT_MAX          0xffff /* low 16 bits are line counter*/
#define TIMEOUT_MAX_CYCLES    2048  /* poll sliceinfo timeout max cycles */
#define SLICEINFOPOLL_TIMEOUT (TIMEOUT_MAX_CYCLES * 256) /* timeout */
#define SLICEINFO_UPDATETIME  (2 * 256) /* 2*256 cycles to update sliceinfo line count */
#define UPDATE_LINECNT_MAX    16 /* max line cnt to update */
#define TIMEOUTTEST_EN        1  /* enable timeout test */
#define POLL_TIMEOUT          1  /* flag to notify poll timeout */
#define HW_CYCLE_TO_TIME      200 /* one hw cycle is 200ns */

typedef struct {
  u32 *sliceinfo_vir_base;
  u32 line_cnt;
  u32 pic_height;
  u32 timeouttest_en;
  u32 frm_rdy;
  u32 poll_timeout;
  pthread_t *tid;
  pthread_mutex_t frmrdy_mutex;
} inputSliceInfo_s;

#ifndef LOW_LATENCY_SLICEINFO_SUPPORT
#define InitSliceInfo(inst, inputSliceInfo_data)
#else
void InitSliceInfo(const void *inst, inputSliceInfo_s *inputSliceInfo_data);
#endif

#ifdef __cplusplus
}
#endif

#endif