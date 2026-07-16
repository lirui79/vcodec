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
**                           source code test encode                            **
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

#include "base_type.h"
#include "vcx_driver.h"
#include "vcx_module_type.h"
#include "testbench_vcodec.h"




//  HANTRO_IOCS_MMU_MEM_MAP  HANTRO_IOCS_MMU_MEM_UNMAP  HANTRO_IOCS_MMU_FLUSH HANTRO_IOCS_MMU_SWITCH_PAGETABLE HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF


static int  test_get_config(int fd);

static int  test_vcmd_config(int fd);

static int  test_vcmd_cmdbuf(int fd);



int main_encode(int argc, char **argv, const char *optarg) {
//  const char *hdec = "/dev/hantroenc";
  int   fd = -1;

  fd = open(optarg, O_RDWR);
  if (fd == -1) {
    printf("Failed to open dev: %s\n", optarg);
    goto end;
  }

  if (test_get_config(fd) < 0) {
    goto end;
  }

  if (test_vcmd_config(fd) < 0) {
    goto end;
  }

  if (test_vcmd_cmdbuf(fd) < 0) {
    goto end;
  }

  close(fd);
  return 0;

end:
  close(fd);
  return -1;
}

int  test_get_config(int fd) {//  HANTRO_IOCH_GET_VCMD_ENABLE  HANTRO_IOCH_GET_MMU_ENABLE HANTRO_IOCH_WRITE_CORE_REGS
    long tmp = 0;
    {
        u32  vcmdEnable = 0xff, mmuEnable = 0xff;
        tmp = ioctl(fd, HANTRO_IOCH_GET_VCMD_ENABLE, &vcmdEnable);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCH_GET_VCMD_ENABLE failed\n");
            return -1;
        }

        printf("%s %s %d get vcmdEnable %d\n", __FILE__, __func__, __LINE__, vcmdEnable);

        tmp = ioctl(fd, HANTRO_IOCH_GET_MMU_ENABLE, &mmuEnable);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCH_GET_MMU_ENABLE failed \n");
            return -1;
        }
        printf("%s %s %d get mmuEnable %d\n", __FILE__, __func__, __LINE__, mmuEnable);
    }

    {
        struct core_regs_wr core = {0};
        u32 i = 0, num = 16;
        core.type = CORE_VCE;
        core.id   = 1;
        core.reg_num = num;
        core.reg_id = 0x1000 / 4;
        for (i = 0; i < num; i++)
          core.reg_val[i] = 0x2000 + i;

        tmp = ioctl(fd, HANTRO_IOCH_WRITE_CORE_REGS, &core);
        if (tmp == -1) {
            printf("ERROR:ioctl HANTRO_IOCH_WRITE_CORE_REGS failed \n");
            return -1;
        }

        printf("%s %s %d Core Regs %ld %ld:%x %x %x %x \nregs:\n", __FILE__, __func__, __LINE__, sizeof(struct core_regs_wr), tmp, core.type, core.id, core.reg_id, core.reg_num);
        for (i = 0; i < core.reg_num; i++) {
            if (i == 8) {
                printf("\n");
            }
            printf(" %x", core.reg_val[i]);
        }
        printf("\n");
    }

  return 0x00;
}

int test_vcmd_config(int fd) {// HANTRO_IOCH_GET_CMDBUF_PARAMETER HANTRO_IOCH_GET_VCMD_PARAMETER
    long tmp = 0;
    {
        struct config_parameter param = {0};
        struct cmdbuf_mem_parameter mem = {0};
        /* Get vcmd parameters */
        param.module_type = VCMD_TYPE_ENCODER;
        tmp = ioctl(fd, HANTRO_IOCH_GET_VCMD_PARAMETER, &param);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCH_GET_VCMD_PARAMETER failed\n");
            return -1;
        }

        printf("%s %s %d VCMD get vcmd config parameter %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct config_parameter), tmp, param.module_type);
        printf("config:%x %x %x %x %x %x %x\n", param.vcmd_core_num, param.submodule_main_addr, param.status_main_addr, param.submodule_dec400_addr, param.status_dec400_addr, param.submodule_L2Cache_addr, param.status_L2Cache_addr);
        printf("config:%x %x %x %x %x %x %x %x\n", param.submodule_MMU_addr[0], param.submodule_MMU_addr[1], param.status_MMU_addr[0], param.status_MMU_addr[1], param.submodule_axife_addr[0], param.submodule_axife_addr[1], param.status_axife_addr[0], param.status_axife_addr[1]);
        printf("config:%x %x %x %x %x %x %x\n", param.submodule_ufbc_addr, param.status_ufbc_addr, param.vcmd_hw_version_id, param.vcmd_priority[0], param.vcmd_priority[1], param.vcmd_priority[2], param.vcmd_priority[3]);

        if (param.vcmd_core_num == 0) {
            printf("ERROR: There is no proper vcmd  for encoder \n");
            return -1;
        }

        /* Get cmd-buf parameters */
        tmp = ioctl(fd, HANTRO_IOCH_GET_CMDBUF_PARAMETER, &mem);
        if (tmp == -1) {
            printf("ERROR:ioctl HANTRO_IOCH_GET_CMDBUF_PARAMETER failed \n");
            return -1;
        }

        printf("%s %s %d VCMD get cmdbuf parameter %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct cmdbuf_mem_parameter), tmp, mem.base_ddr_addr);
        printf("cmdbuf:%x %x %x %x %x\n", mem.cmd_virt_addr, mem.cmd_phy_addr, mem.cmd_hw_addr, mem.cmd_total_size, mem.cmd_unit_size);
        printf("status:%x %x %x %x %x\n", mem.status_virt_addr, mem.status_phy_addr, mem.status_hw_addr, mem.status_total_size, mem.status_unit_size);
        printf("regbuf:%x %x %x %x %x\n", mem.reg_virt_addr, mem.reg_phy_addr, mem.reg_hw_addr, mem.reg_total_size, mem.reg_unit_size);
    }
   return 0x00;
}

int  test_vcmd_cmdbuf(int fd) {//  HANTRO_IOCH_RESERVE_CMDBUF HANTRO_IOCH_LINK_RUN_CMDBUF HANTRO_IOCH_WAIT_CMDBUF HANTRO_IOCH_RELEASE_CMDBUF HANTRO_IOCH_POLLING_CMDBUF
  struct exchange_parameter param = {0};
  u32 width  = 1920;
  u32 height = 1080;
  u16 cmdbuf_id = 0xffff;
  long tmp = 0;
  u16 core_id = 0xffff;  //0xffff means polling all cores
  u16 core_info_hw = cmdbuf_id;

  param.interrupt_ctrl = width * height * 2 * 2;//input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
  param.module_type = VCMD_TYPE_ENCODER;
  param.core_mask = 0xffff;

  tmp = ioctl(fd, HANTRO_IOCH_RESERVE_CMDBUF, &param);
  if (tmp < 0) {
    printf("%s", "DWLReserveCmdBuf failed\n");
    return -1;
  }

  printf("%s %s %d VCMD Reserve CMDBUF %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct exchange_parameter), tmp, param.module_type);
  printf("%x %x %x %x %x %x\n", param.interrupt_ctrl, param.cmdbuf_size, param.cmdbuf_id, param.core_id, param.core_mask, param.input_mask);

  cmdbuf_id = param.cmdbuf_id;
  printf("reserve cmd buf %d\n", cmdbuf_id);

  param.cmdbuf_size = 80;
  param.cmdbuf_id = cmdbuf_id;
  param.input_mask = 0;

  tmp = ioctl(fd, HANTRO_IOCH_LINK_RUN_CMDBUF, &param);
  if (tmp < 0) {
    printf("%s", "DWLEnableCmdBuf failed\n");
    return -1;
  }

  printf("enable cmd buf %d\n", cmdbuf_id);
  core_info_hw = cmdbuf_id;
  tmp = ioctl(fd, HANTRO_IOCH_WAIT_CMDBUF, &core_info_hw);
  if (tmp < 0) {
    printf("%s", "DWLWaitCmdBufReady failed\n");
    return -1;
  } else {
    cmdbuf_id = core_info_hw;
    printf("DWLWaitCmdBufReady %d succeed\n", cmdbuf_id);
  }

  tmp = ioctl(fd, HANTRO_IOCH_RELEASE_CMDBUF, &cmdbuf_id);

  if (tmp < 0) {
     printf("%s", "DWLReleaseCmdBuf failed\n");
     return -1;
  }

  printf("release cmd buf %d %d\n", tmp, cmdbuf_id);

  tmp = ioctl(fd, HANTRO_IOCH_POLLING_CMDBUF, &core_id);
  if (tmp < 0) {
     printf("%s", "DWLPollingCmdBuf failed\n");
     return -1;
  }

  printf("polling cmd buf %d %d\n", tmp, core_id);

  return 0x00;
}
