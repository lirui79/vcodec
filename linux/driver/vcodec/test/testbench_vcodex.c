/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                        source code test vcodex manager                       **
*********************************************************************************/



#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <assert.h>
#include <pthread.h>

#include "testbench_vcodec.h"



int main_vcodex(int argc, char **argv, const char *optarg) {
//  const char *hdec = "/dev/hantrovcx";
  int   fd = -1;

  fd = open(optarg, O_RDWR);
  if (fd == -1) {
    printf("Failed to open dev: %s\n", optarg);
    goto end;
  }
/*
  if (test_core_subsys_info(fd) < 0) {
    goto end;
  }

  if (test_vcmd_config(fd) < 0) {
    goto end;
  }

  if (test_vcmd_cmdbuf(fd) < 0) {
    goto end;
  }
*/
  close(fd);
  return 0;

end:
  close(fd);
  return -1;
}