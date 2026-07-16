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
**                           source code test decode                            **
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

#include "vcd_cfg.h"
#include "basetype.h"
#include "hantrodec.h"
#include "hantrovcmd.h"
#include "testbench_vcodec.h"

#define MAX_ASIC_CORES 4



static int  test_core_subsys_info(int fd);

static int  test_vcmd_config(int fd);

static int  test_vcmd_cmdbuf(int fd);

static int  test_dec_reg(int fd);

static int  test_apb_axi_filter(int fd);

static int  test_dec_cmd(int fd);


int main_decode(int argc, char **argv, const char *optarg) {
//  const char *hdec = "/dev/hantrodec";
  int   fd = -1;

  fd = open(optarg, O_RDWR);
  if (fd == -1) {
    printf("Failed to open dev: %s\n", optarg);
    goto end;
  }

  if (test_core_subsys_info(fd) < 0) {
    goto end;
  }

  if (test_dec_reg(fd) < 0) {
    goto end;
  }

  if (test_apb_axi_filter(fd) < 0) {
    goto end;
  }

  if (test_vcmd_config(fd) < 0) {
    goto end;
  }

  if (test_vcmd_cmdbuf(fd) < 0) {
    goto end;
  }

  if (test_dec_cmd(fd) < 0) {
    goto end;
  }

  close(fd);
  return 0;

end:
  close(fd);
  return -1;
}

int  test_core_subsys_info(int fd) {
  struct subsys_desc subsys = {0x33, 0xf44};
  struct core_param param = {0};
  u32 i, hw_build_id, core_id;
  unsigned int num_cores = 4;
  long tmp = 0;

  if (ioctl(fd, HANTRODEC_IOC_MC_CORES, &num_cores) == -1) {
    printf("%s","ioctl HANTRODEC_IOC_MC_CORES failed\n");
    return -1;
  }
  
  printf("%s %s %d %d\n", __FILE__, __func__, __LINE__, num_cores);

  if (ioctl(fd, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    printf("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    return -1;
  }

  printf("%s %s %d %d %d\n", __FILE__, __func__, __LINE__, subsys.subsys_num, subsys.subsys_vcmd_num);
  for (i=0; i<num_cores; i++) {
      core_id = i;
      param.slice = 0;
      param.id = core_id;
      param.type = HW_VCD;
      tmp = ioctl(fd, HANTRODEC_IOX_ASIC_ID, &param);
      if (tmp < 0) {
        printf("%s", "ioctl HANTRODEC_IOX_ASIC_ID failed\n");
        break;
      }

      printf("%s %s %d get asic id %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct core_param), tmp, param.id);
      printf("%x %x %x %x\n", param.slice, param.type, param.size, param.asic_id);
      hw_build_id = core_id;
      tmp = ioctl(fd, HANTRODEC_IOX_ASIC_BUILD_ID, &hw_build_id);
      if ( tmp < 0) {
        printf("%s", "ioctl HANTRODEC_IOX_ASIC_BUILD_ID failed\n");
        break;
      }

      printf("%s %s %d get build id %ld:%x %x\n", __FILE__, __func__, __LINE__, tmp, core_id, hw_build_id);
    }

  return 0x00;
}

int test_vcmd_config(int fd) {
      /* Get VCMD configuration. */
    struct config_parameter param = {0};
    struct cmdbuf_mem_parameter mem = {0};
    unsigned long multicorebase[MAX_ASIC_CORES] = {0};
    long tmp = 0, i = 0;
    param.module_type = VCMD_TYPE_DECODER;
    tmp = ioctl(fd, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &param);
    if (tmp == -1) {
      printf("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      return -1;
    }

    printf("%s %s %d VCMD get vcmd config parameter %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct config_parameter), tmp, param.module_type);
    printf("%x %x %x %x %x %x %x\n", param.vcmd_core_num, param.submodule_main_addr, param.submodule_dec400_addr, param.submodule_MMU_addr, param.submodule_MMUWrite_addr, param.submodule_axife_addr, param.vcmd_hw_version_id);

    tmp = ioctl(fd, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER, &mem);
    if (tmp == -1) {
      printf("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      return -1;
    }
    printf("%s %s %d VCMD get cmdbuf parameter %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct cmdbuf_mem_parameter), tmp, mem.base_ddr_addr);

    printf("cmdbuf:%x %x %x %x %x\n", mem.phy_cmdbuf_addr, mem.mmu_phy_cmdbuf_addr, mem.virt_cmdbuf_addr, mem.cmdbuf_total_size, mem.cmdbuf_unit_size);

    printf("status:%x %x %x %x %x\n", mem.phy_status_cmdbuf_addr, mem.mmu_phy_status_cmdbuf_addr, mem.virt_status_cmdbuf_addr, mem.status_cmdbuf_total_size, mem.status_cmdbuf_unit_size);

    printf("regbuf:%x %x %x %x %x\n", mem.phy_vcmd_regbuf_addr, mem.mmu_phy_vcmd_regbuf_addr, mem.virt_vcmd_regbuf_addr, mem.vcmd_regbuf_total_size, mem.vcmd_regbuf_unit_size);

    tmp = ioctl(fd, HANTRODEC_IOC_MC_OFFSETS, multicorebase);

    if ( tmp == -1) {
        printf("%s","ioctl HANTRODEC_IOC_MC_OFFSETS failed\n");
        return -1;
    }
    printf("%s %s %d get multi core base %ld:%lx %lx %lx %lx\n", __FILE__, __func__, __LINE__, tmp, multicorebase[0], multicorebase[1], multicorebase[2], multicorebase[3]);

    return 0x00;
}

int  test_vcmd_cmdbuf(int fd) {
  struct exchange_parameter param = {0};
  u32 width  = 1920;
  u32 height = 1080;
  u16 cmdbuf_id = 0xffff;
  int tmp = 0;
  u16 core_info_hw = cmdbuf_id;

  param.executing_time = width * height;
  param.module_type = VCMD_TYPE_DECODER;
  param.core_mask = 0xffff;
  param.owner = (void*)0x11111;

  tmp = ioctl(fd, HANTRO_VCMD_IOCH_RESERVE_CMDBUF, &param);
  if (tmp < 0) {
    printf("%s", "DWLReserveCmdBuf failed\n");
    return -1;
  }

  printf("%s %s %d VCMD Reserve CMDBUF %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct exchange_parameter), tmp, param.module_type);
  printf("%p %x %x %x %x %x %x\n", param.owner, param.executing_time, param.cmdbuf_size, param.cmdbuf_id, param.core_id, param.core_mask, param.input_mask);

  cmdbuf_id = param.cmdbuf_id;
  printf("reserve cmd buf %d\n", cmdbuf_id);

  param.cmdbuf_size = 10;
  param.cmdbuf_id = cmdbuf_id;
  param.input_mask = 0;

  tmp = ioctl(fd, HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF, &param);
  if (tmp < 0) {
    printf("%s", "DWLEnableCmdBuf failed\n");
    return -1;
  }

  printf("enable cmd buf %d\n", cmdbuf_id);
  core_info_hw = 0xffff;
  tmp = ioctl(fd, HANTRO_VCMD_IOCH_WAIT_CMDBUF, &core_info_hw);
  if (tmp < 0) {
    printf("%s", "DWLWaitCmdBufReady failed\n");
    return -1;
  } else {
    cmdbuf_id = core_info_hw;
    printf("DWLWaitCmdBufReady %d succeed\n", cmdbuf_id);
  }

  tmp = ioctl(fd, HANTRO_VCMD_IOCH_RELEASE_CMDBUF, &cmdbuf_id);

  if (tmp < 0) {
     printf("%s", "DWLReleaseCmdBuf failed\n");
     return -1;
  }

  printf("release cmd buf %d\n", cmdbuf_id);

  return 0x00;
}

int  test_dec_reg(int fd) {
// HANTRODEC_IOCGHWIOSIZE  HANTRODEC_IOCS_DEC_PUSH_REG HANTRODEC_IOCS_DEC_WRITE_REG HANTRODEC_IOCS_DEC_READ_REG  HANTRODEC_IOCS_DEC_PULL_REG HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG HANTRODEC_IOCX_DEC_WAIT
    long tmp = 0;
    {
        struct regsize_desc core = {0};
        core.type = HW_VCD;
        core.id   = 0x01;
        tmp = ioctl(fd, HANTRODEC_IOCGHWIOSIZE, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
            return -1;
        }

        if (core.size != MAX_REG_COUNT * sizeof(u32)) {
            printf("%s","failed dec reg size from kernel\n");
            return -1;
        }

        printf("%s %s %d dec core reg %ld %ld:%x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct regsize_desc), tmp, core.type, core.slice, core.id, core.size);
    }

    {
        struct core_desc    core = {0};
        core.id = 01;
        core.regs = (u32*)0x2345618;
        core.size = MAX_REG_COUNT * 4;
        core.type = HW_VCD;
        core.reg_id = 0x23;

        tmp = ioctl(fd, HANTRODEC_IOCS_DEC_PUSH_REG, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCS_DEC_PUSH_REG failed\n");
            return -1;
        }

        if (core.size != MAX_REG_COUNT * sizeof(u32)) {
            printf("%s","failed core desc reg size from kernel\n");
            return -1;
        }

        printf("%s %s %d dec core reg %ld %ld:%x %x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct core_desc), tmp, core.type, core.regs, core.id, core.size, core.reg_id);
    }

    {
        struct core_desc    core = {0};
        core.id = 00;
        core.regs = (u32*)0x43234328;
        core.size = MAX_REG_COUNT * 4;
        core.type = HW_VCD;
        core.reg_id = 0x54;

        tmp = ioctl(fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCS_DEC_WRITE_REG failed\n");
            return -1;
        }

        if (core.size != MAX_REG_COUNT * sizeof(u32)) {
            printf("%s","failed core desc reg size from kernel\n");
            return -1;
        }

        printf("%s %s %d dec core reg %ld %ld:%x %x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct core_desc), tmp, core.type, core.regs, core.id, core.size, core.reg_id);
    }

    {
        struct core_desc    core = {0};
        core.id = 00;
        core.type = HW_VCD;
        core.reg_id = 0x76;

        tmp = ioctl(fd, HANTRODEC_IOCS_DEC_READ_REG, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
            return -1;
        }

        if (core.size != MAX_REG_COUNT * sizeof(u32)) {
            printf("%s","failed core desc reg size from kernel\n");
            return -1;
        }

        printf("%s %s %d dec core reg %ld %ld:%x %x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct core_desc), tmp, core.type, core.regs, core.id, core.size, core.reg_id);
    }

    {
        struct core_desc    core = {0};
        core.id = 00;
        core.type = HW_VCD;
        core.reg_id = 0x98;

        tmp = ioctl(fd, HANTRODEC_IOCS_DEC_PULL_REG, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCS_DEC_PULL_REG failed\n");
            return -1;
        }

        if (core.size != MAX_REG_COUNT * sizeof(u32)) {
            printf("%s","failed core desc reg size from kernel\n");
            return -1;
        }

        printf("%s %s %d dec core reg %ld %ld:%x %x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct core_desc), tmp, core.type, core.regs, core.id, core.size, core.reg_id);
    }

    {
        struct core_desc    core = {0};
        core.id = 00;
        core.type = HW_VCD;
        core.reg_id = 0x98;

        tmp = ioctl(fd, HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCS_DEC_WRITE_APBFILTER_REG failed\n");
            return -1;
        }

        if (core.size != MAX_REG_COUNT * sizeof(u32)) {
            printf("%s","failed core desc reg size from kernel\n");
            return -1;
        }

        printf("%s %s %d dec core apb reg %ld %ld:%x %x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct core_desc), tmp, core.type, core.regs, core.id, core.size, core.reg_id);
    }

    {
        struct core_desc    core = {0};
        core.id = 1;
        core.regs = (void *)0x4000;
        core.size = MAX_REG_COUNT * 4;
        core.type = HW_VCD;
        tmp = ioctl(fd, HANTRODEC_IOCX_DEC_WAIT, &core);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCX_DEC_WAIT failed\n");
            return -1;
        }

        printf("%s %s %d dec wait %ld %ld:%x %x %x %x %x\n", __FILE__, __func__, __LINE__, sizeof(struct core_desc), tmp, core.type, core.regs, core.id, core.size, core.reg_id);
    }

    return 0x00;
}

int  test_apb_axi_filter(int fd) {//  HANTRODEC_IOC_APBFILTER_CONFIG  HANTRODEC_IOC_AXIFE_CONFIG
    long tmp = 0;
    {
        struct apbfilter_cfg tmp_apbfilter;
        tmp_apbfilter.id = 0;
        tmp_apbfilter.type = HW_VCD;
        tmp = ioctl(fd, HANTRODEC_IOC_APBFILTER_CONFIG, &tmp_apbfilter);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOC_APBFILTER_CONFIG failed\n");
            return -1;
        }
        printf("%s %s %d apb filter config %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct apbfilter_cfg), tmp, tmp_apbfilter.type);
        printf("config:%x %x %x %x %x %x %x\n", tmp_apbfilter.id, tmp_apbfilter.has_apbfilter, tmp_apbfilter.nbr_mask_regs, tmp_apbfilter.mask_reg_offset, tmp_apbfilter.page_sel_addr, tmp_apbfilter.num_mode, tmp_apbfilter.mask_bits_per_reg);
    }

    {
        struct axife_cfg tmp_axife;
        tmp_axife.id = 1;
        tmp = ioctl(fd, HANTRODEC_IOC_AXIFE_CONFIG, &tmp_axife);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOC_AXIFE_CONFIG failed\n");
            return -1;
        }
        printf("%s %s %d axife config %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(struct axife_cfg), tmp, tmp_axife.id);
        printf("config:%x %x %x %x %x %x\n", tmp_axife.id, tmp_axife.axi_rd_chn_num, tmp_axife.axi_wr_chn_num, tmp_axife.axi_rd_burst_length, tmp_axife.axi_wr_burst_length, tmp_axife.fe_mode);
    }

    return 0x00;
}

int  test_dec_cmd(int fd) {// HANTRODEC_IOCH_DEC_RESERVE  HANTRODEC_IOCT_DEC_RELEASE  HANTRODEC_IOCG_CORE_WAIT  HANTRODEC_IOCH_WAIT_OWNER_DONE
    long tmp = 0;
    {
        long core_id = -1;
        struct req_core_info core_info = {0};
        core_info.format   = 0x8888888;
        core_info.dec_inst = (void *)0x222222;

        core_id = ioctl(fd, HANTRODEC_IOCH_DEC_RESERVE, &core_info);

        /* negative value signals an error */
        if (core_id < 0) {
            printf("ioctl HANTRODEC_IOCH_DEC_RESERVE failed, %d\n", core_id);
            return -1;
        }
        printf("%s %s %d dec reserve %ld %ld:%x %p\n", __FILE__, __func__, __LINE__, sizeof(struct req_core_info), core_id, core_info.format, core_info.dec_inst);
    }

    {
        int32_t core_id = 0;
        tmp = ioctl(fd, HANTRODEC_IOCT_DEC_RELEASE, &core_id);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCT_DEC_RELEASE failed\n");
            return -1;
        }
        printf("%s %s %d dec release %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(int32_t), tmp, core_id);
    }

    {
        int id = -1;
        tmp = ioctl(fd, HANTRODEC_IOCG_CORE_WAIT, &id);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCG_CORE_WAIT failed\n");
            return -1;
        }
        printf("%s %s %d core wait %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(int), tmp, id);
    }

    {
        struct req_core_info core_info = {0};
        void *owner = &core_info;
        tmp = ioctl(fd, HANTRODEC_IOCH_WAIT_OWNER_DONE, owner);
        if (tmp == -1) {
            printf("%s","ioctl HANTRODEC_IOCH_WAIT_OWNER_DONE failed\n");
            return -1;
        }
        printf("%s %s %d wait done %ld %ld:%x\n", __FILE__, __func__, __LINE__, sizeof(void *), tmp, owner);
    }
    return 0x00;
}