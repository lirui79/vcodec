/*
 * SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */


#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <assert.h>
#include <pthread.h>

#include "arbiter_get_option.h"
#include "../arbiter_drv.h"

#define ARBITER_POLLING_ISR

struct CommandLine {
  int arb_enable;
  u32 arb_id;
  u32 arb_time_window;
  int master_online;
  u32 master_offline;
  u32 master_id;
  u32 master_bw_overflow;
  u32 master_urgent;
  u32 master_weight;
  u32 check_irq_status;
  u32 wait_timeout;
};

struct arbiter_inst {
  int fd;
  u32 irq_check_break;
  u32 arb_num;
  u32 irq_status[MAX_ARBITER_NUM];
  u32 master_num;
  u32 wait_timeout;
};

static int GetParams(i32 argc, char **argv, struct CommandLine *cml)
{
  i32 ret, i, j;
  char *optarg;
  argument_s argument;
  int status = 0;
  argument.optCnt = 1;

  // default params
  cml->arb_enable = -1;
  cml->master_online = -1;

  while ((ret = ParseOption(argc, argv, options, &argument)) != -1) {
    if (ret == -2)
      status = -1;
    if (ret != 0) {
      status = -1;
      fprintf(stderr, "Error: Invalid Option %s\n", argv[argument.optCnt-1]);
    }
    optarg = argument.optArg;
    switch (argument.shortOpt) {
    case 'a':
      cml->arb_enable = atoi(optarg);
      break;
    case 'A':
      cml->arb_id = atoi(optarg);
      break;
    case 'w':
      cml->master_weight = atoi(optarg);
      break;
    case 'b':
      cml->master_bw_overflow = atoi(optarg);
      break;
    case 'u':
      cml->master_urgent = atoi(optarg);
      break;
    case 'o':
      cml->master_online = atoi(optarg);
      break;
    case 'O':
      cml->master_offline = atoi(optarg);
      break;
    case 'm':
      cml->master_id = atoi(optarg);
      break;
    case 't':
      cml->arb_time_window = atoi(optarg);
      break;
    case 'c':
      cml->check_irq_status = atoi(optarg);
    case 'W':
      cml->wait_timeout = atoi(optarg);
      break;
    }
  }

  return status;
}

static void arbiter_polling_isr(void *data)
{
  struct arbiter_inst *inst = (struct arbiter_inst *)data;

  while (1) {
    ioctl(inst->fd, ARBITER_IOCH_CHECK_IRQ_STATUS_POLLING);
    usleep(10000);  //10ms
    if (inst->irq_check_break) break;
  }

}

static int checkIrqStatus(struct arbiter_inst *arb_inst)
{
  u32 irq_status;
  struct io_wait_irq wait_irq;
  int i;

#ifdef ARBITER_POLLING_ISR
  pthread_attr_t attr;
  pthread_t tid;

  pthread_attr_init(&attr);
  pthread_create(&tid, &attr, (void *)arbiter_polling_isr, (void *)arb_inst);
  pthread_attr_destroy(&attr);
#endif

  wait_irq.wait_timeout = arb_inst->wait_timeout;
  ioctl(arb_inst->fd, ARBITER_IOCH_CHECK_IRQ_STATUS, &wait_irq);

  for (i = 0; i < arb_inst->arb_num; i++)
    arb_inst->irq_status[i] = wait_irq.irq_status[i];

#ifdef ARBITER_POLLING_ISR
  arb_inst->irq_check_break = 1;
  pthread_join(tid, NULL);
#endif

  return 0;
}

int main(i32 argc, char **argv)
{
  struct arbiter_inst arb_inst;
  struct CommandLine cml;
  const char *arb_dev = "/tmp/dev/arbiter_dev";
  struct io_arb_params com_params;
  struct io_master_params mst_params;
  struct io_arb_master_offline offline_info;
  int ret, i;
  u32 id;

  memset(&cml, 0, sizeof(struct CommandLine));
  memset(&arb_inst, 0, sizeof(struct arbiter_inst));

  if (GetParams(argc, argv, &cml) != 0) {
    fprintf(stderr, "Input parameter error\n");
    return -1;
  }

  arb_inst.fd = open(arb_dev, O_RDWR);
  if (arb_inst.fd == -1) {
    printf("Failed to open dev: %s\n", arb_dev);
    goto end;
  }

  // get supported arbiter num
  ioctl(arb_inst.fd, ARBITER_IOCH_ARB_NUM_GET, &arb_inst.arb_num);
  printf("TB: supported arbiter num is %u.\n", arb_inst.arb_num);
  if (cml.arb_id >= arb_inst.arb_num)
    printf("TB: the arb_id=%u is not valid!\n", cml.arb_id);

  com_params.arb_id = cml.arb_id;
  // arbiter common params get
  ret = ioctl(arb_inst.fd, ARBITER_IOCH_ARB_PARAMS_GET, &com_params);
  if (ret < 0) {
    printf("TB: get arbiter[%u] common params faild\n", cml.arb_id);
  }
  arb_inst.master_num = com_params.master_num;
  printf("TB: arbiter[%u] params: enable=%u, time_window=%u, master_num=%u\n",
    com_params.arb_id, com_params.enable, com_params.time_window_exp, com_params.master_num);

  // check the master id if valid
  if (cml.master_offline == 1 || cml.master_online >= 0) {
    if (cml.master_id < arb_inst.master_num) {
      mst_params.master_id = cml.master_id;
    } else {
      printf("The master id %u is greater than HW maximum supported\n", cml.master_id);
      goto end;
    }
  }

  // master offline
  if (cml.master_offline == 1) {
    offline_info.arb_id = cml.arb_id;
    offline_info.master_id = cml.master_id;
    ret = ioctl(arb_inst.fd, ARBITER_IOCH_MASTER_OFFLINE, &offline_info);
    if (ret < 0)
      printf("TB: master[%u] of arbiter[%u] offline failed!\n", cml.master_id, cml.arb_id);
    goto end;
  }

  // arbiter enable
  if (cml.arb_enable >= 0) {
    if (cml.arb_enable == 0)
      com_params.enable = DISABLE;
    else
      com_params.enable = ENABLE;
    com_params.time_window_exp = cml.arb_time_window;
    com_params.arb_id = cml.arb_id;
    ret = ioctl(arb_inst.fd, ARBITER_IOCH_ARB_PARAMS_SET, &com_params);
    if (ret < 0) {
      printf("ioctl: set arbiter common params failed\n");
      goto end;
    }
    printf("TB: After setting: arbiter[%u] params: enable=%u, time_window=%u, master_num=%u\n",
    com_params.arb_id, com_params.enable, com_params.time_window_exp, com_params.master_num);
  }

  // master online
  if (cml.master_online >= 0) {
    mst_params.arb_id = cml.arb_id;
    mst_params.master_id = cml.master_id;
    ioctl(arb_inst.fd, ARBITER_IOCH_MASTER_PARAMS_GET, &mst_params);
    printf("TB: Before setting: master[%u] config params: online=%u, urgent=%u, weight=%u,"
      " bw_overflow=%u\n", mst_params.master_id, mst_params.enable, mst_params.urgent,
      mst_params.weight, mst_params.bw_overflow);
    if (cml.master_online == 0)
      mst_params.enable = DISABLE;
    else
      mst_params.enable = ENABLE;
    mst_params.urgent = cml.master_urgent;
    mst_params.weight = cml.master_weight;
    mst_params.bw_overflow = cml.master_bw_overflow;

    ret = ioctl(arb_inst.fd, ARBITER_IOCH_MASTER_PARAMS_SET, &mst_params);
    if (ret < 0) {
      printf("ioctl: set master cfg params failed\n");
      goto end;
    }
    printf("TB: After setting: master[%u] config params: online=%u, urgent=%u, weight=%u,"
      " bw_overflow=%u\n", mst_params.master_id, mst_params.enable, mst_params.urgent,
      mst_params.weight, mst_params.bw_overflow);
  }

  if (cml.check_irq_status == 1) {
    arb_inst.wait_timeout = cml.wait_timeout;
    checkIrqStatus(&arb_inst);
  }

  for (i = 0; i < arb_inst.arb_num; i++) {
    if (arb_inst.irq_status[i] != 0)
      printf("atbiter[%d] received intterupt 0x%8x.\n", i, arb_inst.irq_status[i]);
  }

  close(arb_inst.fd);
  return 0;

end:
  close(arb_inst.fd);
  return -1;
}
