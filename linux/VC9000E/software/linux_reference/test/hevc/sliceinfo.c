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
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT

#include <stdio.h>
#include "sliceinfo.h"
#ifdef SUPPORT_HEVC
#include "instance.h"
#endif
#include "sw_test_id.h"

static void *SliceInfoSimulation(void *inputSliceInfo_data) {
  u32 line_cnt = 0;
  u32 update_cnt_time = 0;
  u32 max_line_cnt = 0;
  inputSliceInfo_s *inputSliceInfo_tmp =
                (inputSliceInfo_s *)inputSliceInfo_data;

  ASSERT(inputSliceInfo_tmp);

  line_cnt = inputSliceInfo_tmp->line_cnt;
  *(inputSliceInfo_tmp->sliceinfo_vir_base) = line_cnt;
  max_line_cnt = inputSliceInfo_tmp->pic_height;

  do {
    pthread_mutex_lock(&inputSliceInfo_tmp->frmrdy_mutex);
    if (inputSliceInfo_tmp->frm_rdy == VCENC_FRAME_READY) {
      inputSliceInfo_tmp->line_cnt = 1;
      inputSliceInfo_tmp->frm_rdy = 0;
      pthread_mutex_unlock(&inputSliceInfo_tmp->frmrdy_mutex);
      break;
    }
    pthread_mutex_unlock(&inputSliceInfo_tmp->frmrdy_mutex);

    if (inputSliceInfo_tmp->poll_timeout == POLL_TIMEOUT)
      break;

    line_cnt += rand() % UPDATE_LINECNT_MAX;
    /* HW deal with, need not align the line cnt */
    //line_cnt = (line_cnt + (cnt_alignment - 1)) & (~((cnt_alignment - 1)));

    if (line_cnt >= max_line_cnt)
      line_cnt = max_line_cnt;
    if (inputSliceInfo_tmp->timeouttest_en == TIMEOUTTEST_EN)
      update_cnt_time = SLICEINFOPOLL_TIMEOUT;
    else
      update_cnt_time = rand() % SLICEINFO_UPDATETIME;

    /* one hw cycles is HW_CYCLE_TO_TIME ns, delay update_cnt_time hw cycles to
     * update line cnt
     */
    usleep(update_cnt_time*HW_CYCLE_TO_TIME/1000);
    inputSliceInfo_tmp->line_cnt = line_cnt;
    *(inputSliceInfo_tmp->sliceinfo_vir_base) = line_cnt;
  } while (line_cnt < max_line_cnt);

  return NULL;
}

void InitSliceInfo(const void *inst, inputSliceInfo_s *inputSliceInfo_data) {
  pthread_attr_t attr;

#ifdef INTERNAL_TEST
  struct vcenc_instance *vcenc_instance = (struct vcenc_instance *)inst;
  HevcSliceInfoPollTimeoutTest(vcenc_instance,
                              &inputSliceInfo_data->timeouttest_en);
#endif

  pthread_attr_init(&attr);
  pthread_mutex_init(&inputSliceInfo_data->frmrdy_mutex, NULL);
  pthread_create(inputSliceInfo_data->tid, &attr, &SliceInfoSimulation,
                 inputSliceInfo_data);
  pthread_attr_destroy(&attr);

  return;
}

#endif
