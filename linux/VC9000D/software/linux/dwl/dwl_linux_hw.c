/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#include "basetype.h"
#include "decapicommon.h"
#include "dwl_linux.h"
#include "dwl.h"
#include "dwlthread.h"
#include "hantrodec.h"
#include "hantrovcmd.h"
#include "dwl_vcmd_common.h"
#include "regdrv.h"
#include "dec_log.h"
#ifdef __FREERTOS__
#include "dev_common_freertos.h"
#include "user_freertos.h"
#include "memalloc_freertos.h"
#elif defined(__linux__)
#include "dwl_linux_tb.h"
#include "memalloc.h"
#else //For other os
//TODO...
#endif

#include "sw_util.h"

#ifdef __FREERTOS__
/* nothing */
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#else //For other os
//TODO...
#endif

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
#include "dwl_linux_dec400.h"
#endif
#endif

#ifdef SUPPORT_MMU
#include "dwl_linux_mmu.h"
#endif
#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#ifdef FPGA_PERF_AND_BW
#include "dwl_perf_info.h"
#endif

#ifdef _DWL_PERFORMANCE
extern u32 hw_malloc_total_max;
#endif

#define DEC_MODE_H264      0
#define DEC_MODE_JPEG      3
#define DEC_MODE_HEVC      12
#define DEC_MODE_VP9       13
#define DEC_MODE_H264_H10P 15
#define DEC_MODE_AVS2      16
#define DEC_MODE_AV1       17
#define DEC_MODE_VVC       18

// #define DEFAULT_ENABLE_MC_LISTENER
/* globle dwl device info */
/* a mutex protecting the device node init */
#ifdef SEM_REPLACE_MUTEX
sem_t                  _nodes_init_mutex;
#else
static pthread_mutex_t _nodes_init_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

static void *nodes_vpu[MAX_DEC_DEV_NODES];
static void *nodes_mem[MAX_MEM_DEV_NODES];

#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
static void *nodes_dma[MAX_DMA_DEV_NODES];
#endif

static enum DWLRet DWLProcessHwAsicStatus(DWLDecNode *dev, u32 id);

static enum DWLRet DWLCheckInitParam(struct DWLInitParam *param);

static enum DWLRet DWLAllocDecNodeResource(DWLDecNode *dev);
static void DWLFreeDecNodeResource(DWLDecNode *dev);

static void *DWLInitDevNode(enum NODE_TYPE node_type, const char *node_name);
static void DWLReleaseDevNode(struct DevBase *base);

void *ThreadMCListener(void *args) {
  DWLDecNode *dev = (DWLDecNode *)args;

#ifndef DWL_USE_DEC_IRQ
  const unsigned int usec = 1000; /* 1 ms polling interval */
#endif
  av_unused u32 irq_stats, id, opcode;
  enum DWLRet ret;
  u32 *reg_base = NULL;
#ifdef PERFORMANCE_TEST
  av_unused u32 first_tile_decoded = 0;
  u32 start_timing = 0;
#endif

//    u16 cmdbuf_id = ANY_CMDBUF_ID;    //TODO: adjust HANTRO_VCMD_IOCH_WAIT_CMDBUF param from u16 to u32
  opcode = HANTRODEC_IOCG_CORE_WAIT;
  if (DEV_USE_VCMD(dev)) {
    opcode = HANTRO_VCMD_IOCH_WAIT_CMDBUF;
  }

  while (!dev->b_stopped) {
#ifdef DWL_USE_DEC_IRQ
    id = ANY_ID;          //ANY_ID equals to ANY_CMDBUF_ID

    if (dev->job_done_cnt < dev->job_cnt) {
      DTRACE_I("%s", "ioctl wait for interrupt\n");
      if (ioctl(dev->base.fd, opcode, &id)) {
        DTRACE_E("ioctl 0x%x failed\n", opcode);
        if (dev->b_stopped) break;
        continue;
      }
    }

    if (id != ANY_ID) {
      pthread_mutex_lock(&dev->job_done_mutex);
      dev->job_done_cnt++;
      pthread_mutex_unlock(&dev->job_done_mutex);
    }
    else {
      pthread_mutex_lock(&dev->job_mutex);
      while (!dev->mc_listener_thread && dev->job_done_cnt == dev->job_cnt) {
        pthread_cond_wait(&dev->job_cv, &dev->job_mutex);
      }
      pthread_mutex_unlock(&dev->job_mutex);
    }

    if (dev->b_stopped) break;

    if (!DEV_USE_VCMD(dev)) {
      if (id == ANY_ID) continue;
      ret = DWLProcessHwAsicStatus(dev, id);
      reg_base = &dev->dec_shadow_regs[id];

      irq_stats = reg_base[HANTRODEC_IRQ_STAT_DEC];
      irq_stats = (irq_stats >> 11) & 0x5FFF;
      if (ret != DWL_HW_WAIT_OK || irq_stats == 0)
        continue;

#ifdef FPGA_PERF_AND_BW
      *(dev->bytes_consumed_perf + id) = DWLGetConsumedBytes(id, dev->start_address_perf, &reg_base[0]);
#endif
      DTRACE_I("DEC IRQ by Core %d\n", id);

      if (dev->cb_func[id] != NULL)
        dev->cb_func[id](dev->cb_arg[id], id);
      else
        sem_post(&dev->sc_dec_rdy_sem[id]);
    } else {
      u32 cmdbuf_id = id;
      if (cmdbuf_id == ANY_CMDBUF_ID) continue;

      DTRACE_I("VCMD IRQ by cmd buf %d\n", cmdbuf_id);
      struct VcmdBuf* vcmd = &dev->vcmdb[cmdbuf_id];
      u32 *status = (u32 *)(vcmd->status_buf + dev->vcmd_params.submodule_main_addr/2);

      u32 irq_stats = status[HANTRODEC_IRQ_STAT_DEC];
      PrintIrqType(0, irq_stats);
      if (dev->cb_func[cmdbuf_id] != NULL) {
        /* VCMD multicore decoding: updated registers to vcmd[cmdbuf_id].mc_fresh_reg_mirror */
        vcmd->mc_fresh_reg_mirror[0] = *status++; //0-0
        vcmd->mc_fresh_reg_mirror[1] = *status++; //1-1
        vcmd->mc_fresh_reg_mirror[2] = *status++; //2-261
        vcmd->mc_fresh_reg_mirror[3] = *status++; //3-270
        vcmd->mc_fresh_reg_mirror[4] = *status++; //4-168
        vcmd->mc_fresh_reg_mirror[5] = *status++; //5-169
        vcmd->mc_fresh_reg_mirror[6] = *status++; //6-62
        vcmd->mc_fresh_reg_mirror[7] = *status++; //7-63
        dev->cb_func[cmdbuf_id](dev->cb_arg[cmdbuf_id], cmdbuf_id);
      }
      else {
        sem_post(&dev->sc_dec_rdy_sem[cmdbuf_id]);
      }
    }

#else   //DWL_USE_DEC_IRQ
    for (id = 0; id < dev->num_cores; id++) {
      /* Skip cores that are not part of multicore, they call directly
       * DWLWaitHwReady(), which does its own polling.
       */
      if (dev->cb_func[id] == NULL)
        continue;
      if (!HW_G_ENABLE(dev->dec_misc_ctrl[id]))
        continue;

      ret = DWLProcessHwAsicStatus(dev, id);
      reg_base = &dev->dec_shadow_regs[id];

      irq_stats = reg_base[HANTRODEC_IRQ_STAT_DEC];
      irq_stats = (irq_stats >> 11) & 0x5FFF;
      if (ret != DWL_HW_WAIT_OK || irq_stats == 0)
        continue;

#ifdef PERFORMANCE_TEST
      u32 i;
      for (i = 0; i < MAX_ASIC_CORES; i++) {
        if (reg_base[HANTRODEC_IRQ_STAT_DEC] & 0x01)
          break;
      }
      if (i != MAX_ASIC_CORES) {
        if(!start_timing) {
          ActivityTraceStartDec(&dev->activity);
          start_timing = 1;
        }
      } else {
        ActivityTraceStopDec(&dev->activity);
        start_timing = 0;
      }
      if(!first_tile_decoded && !(reg_base[HANTRODEC_IRQ_STAT_DEC] & 0x01)){
        ActivityTraceStopDecTile(&dev->activity);
        first_tile_decoded = 1;
      }
#endif

#ifdef FPGA_PERF_AND_BW
      *(dev->bytes_consumed_perf + id) = DWLGetConsumedBytes(id, dev->start_address_perf, &reg_base[0]);
#endif
      DTRACE_I("DEC IRQ by Core %d\n", id);
      dev->cb_func[id](dev->cb_arg[id], id);
    }
    usleep(usec);  /* Sleep after one loop of checking all the cores. */
#endif
  }

  return NULL;
}

/* for vcmd mode */
#if defined(DWL_USE_DEC_IRQ) && !defined(FPGA_REAL_INT)
/* Use polling command to simulate interrupt mode. */
void* FpgaInterruptSimWithPoll(void *args) {
  DWLDecNode *dev = (DWLDecNode *)args;
  ASSERT(DEV_USE_VCMD(dev));
  u16 dummy = 0;

  while (!dev->b_stopped) {
    ioctl(dev->base.fd, HANTRO_VCMD_IOCH_POLLING_CMDBUF, &dummy);

    usleep(10 * 1000); //10ms
    dummy++;
    if (dummy >= dev->num_cores) dummy = 0;
  }

  return NULL;
}
#endif

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : void * param - not in use, application passes NULL
------------------------------------------------------------------------------*/
const void *DWLInit(struct DWLInitParam *param) {
  struct HANTRODWL *dec_dwl = NULL;

#ifdef SUPPORT_RANDOM_LATENCY
  /* base addr of AXI LG HW register */
  u32 *axi_lg_base[MAX_ASIC_CORES];
  /* the register that can change the AXI latency */
  u32 *axi_lg_ctrl[MAX_ASIC_CORES];
  /* set random latency range for AW channel */
  u32 *axi_lg_aw_rand[MAX_ASIC_CORES];
  /* set random latency range for AR channel */
  u32 *axi_lg_ar_rand[MAX_ASIC_CORES];
#endif

  DTRACE_I("%s","DWLInit INITIALIZE\n");

  if (DWLCheckInitParam(param))
    goto err;

  dec_dwl = (struct HANTRODWL *)DWLcalloc(1, sizeof(struct HANTRODWL));
  if (dec_dwl == NULL) {
    DTRACE_E("%s","failed to alloc struct HANTRODWL struct\n");
    goto err;
  }

  dec_dwl->dev = DWLInitDevNode(DEC_NODE, param->dec_dev);
  if (dec_dwl->dev == NULL)
    goto err;
  dec_dwl->client_type = param->client_type;
  pthread_mutex_init(&dec_dwl->owner_mutex, NULL);

  dec_dwl->mem_dev = DWLInitDevNode(MEM_NODE, param->mem_dev);
  if (dec_dwl->mem_dev == NULL)
    goto err;

#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
  dma_dwl->dma_dev = DWLInitDevNode(DMA_NODE, param->dma_dev);
  if (dma_dwl->dma_dev = NULL)
    goto err;
#endif

#ifdef INTERNAL_TEST
  InternalTestInit();
#endif

#ifdef SUPPORT_RANDOM_LATENCY
  unsigned long multicore_base[MAX_ASIC_CORES];
  u32 *F, i;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  int dec_fd = dev->base.fd;

  if (ioctl(dec_fd, HANTRODEC_IOC_MC_OFFSETS, multicore_base) == -1) {
    DTRACE_E("%s","ioctl HANTRODEC_IOC_MC_OFFSETS failed\n");
    goto err;
  }

  for (i = 0; i < dev->num_cores; i++){
    axi_lg_base[i] = (u32 *)mmap(NULL, 0xFF, PROT_READ|PROT_WRITE, MAP_SHARED, dec_fd, (multicore_base[i] + 0x3800));
    axi_lg_ctrl[i] = (u32 *)(axi_lg_base[i] + (0x44/ sizeof(u32)));
    axi_lg_aw_rand[i] = (u32 *)(axi_lg_base[i] + (0x48/ sizeof(u32)));
    axi_lg_ar_rand[i] = (u32 *)(axi_lg_base[i] + (0x4C/ sizeof(u32)));
    F = axi_lg_base[i] + (0x40/sizeof(u32));
    *F = 0x3fff8080;
    *(axi_lg_aw_rand[i]) = 1;
    *(axi_lg_ar_rand[i]) = 1;
    if (param->axi_lg_r && param->axi_lg_w) {
      *(axi_lg_ctrl[i]) = (param->axi_lg_w << 16) | param->axi_lg_r;
    } else {
      *(axi_lg_aw_rand[i]) = (12 << 16) | 1;
      *(axi_lg_ar_rand[i]) = (12 << 16) | 1;
    }
    munmap((void *)axi_lg_base[i], 0xFF);
  }
#endif

#ifdef PERFORMANCE_TEST
  ActivityTraceInit(&((DWLDecNode *)dec_dwl->dev)->activity);
#endif

  DTRACE_I("%s","DWLInit SUCCESS\n");
  return dec_dwl;
err:
  DTRACE_E("%s","FAILED\n");
  if (dec_dwl) DWLRelease(dec_dwl);

  return NULL;
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
enum DWLRet DWLRelease(const void *instance) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  u32 i = 0, cores = 0;
  DWLDecNode *dev = NULL;

  DTRACE_I("%s","DWLRelease RELEASE\n");

  if (dec_dwl == NULL) return DWL_OK;

#ifdef FPGA_PERF_AND_BW
#ifdef SUPPORT_AXIFE
  DWLDisableAxiFe(dec_dwl, 0);
#endif
#endif

  if (dec_dwl->dev) {
    dev = (DWLDecNode *)dec_dwl->dev;
    cores = dev->num_cores;

#ifdef PERFORMANCE_TEST
  ActivityTraceRelease(&dev->activity);
#endif
    DWLReleaseDevNode((struct DevBase *)dec_dwl->dev);
  }

  if (dec_dwl->mem_dev) {
    DWLReleaseDevNode((struct DevBase *)dec_dwl->mem_dev);
  }

#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
  if (dec_dwl->dma_dev) {
    DWLReleaseDevNode((struct DevBase *)dec_dwl->dma_dev);
  }
#endif

  PERFORMANCE_STATIC_REPORT(decode_push_reg);
  PERFORMANCE_STATIC_REPORT(decode_pull_reg);

#ifdef _DWL_PERFORMANCE
  printf("Total allocated reference mem = %llu\n", dec_dwl->hw_reference_total_max);
  printf("Total allocated linear mem    = %llu\n", dec_dwl->hw_linear_total_max);
  printf("Total allocated SWSW mem      = %8u\n", hw_malloc_total_max);
#endif

  /* print core usage stats */
 {
    u32 total_usage = 0;
    for (i = 0; i < cores; i++)
      total_usage += dec_dwl->core_usage_counts[i];

    /* avoid zero division */
    total_usage = total_usage ? total_usage : 1;

    printf("\nMulti-core usage statistics:\n");
    for (i = 0; i < cores; i++)
      printf("\tCore[%2u] used %6u times (%2u%%)\n", i,
             dec_dwl->core_usage_counts[i],
             (dec_dwl->core_usage_counts[i] * 100) / total_usage);

    printf("\n");
  }

#ifdef INTERNAL_TEST
  InternalTestFinalize();
#endif

  pthread_mutex_destroy(&dec_dwl->owner_mutex);
  DWLfree(dec_dwl);
  dec_dwl = NULL;

  DTRACE_I("%s","DWLRelease SUCCESS\n");

  return (DWL_OK);
}

/* HW locking */

/*------------------------------------------------------------------------------
    Function name   : DWLReserveHw
    Description     :
    Return type     : i32
    Argument        : const void *instance
    Argument        : i32 *core_id - ID of the reserved HW core
------------------------------------------------------------------------------*/
enum DWLRet DWLReserveHw(const void *instance, struct DWLReqInfo *info, i32 *core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  struct req_core_info core_info = {info->core_mask & 0xffff, info->owner};

  DTRACE_I(" %s\n", "DEC");
  random_exit();
  *core_id = ioctl(dev->base.fd, HANTRODEC_IOCH_DEC_RESERVE, &core_info);

  /* negative value signals an error */
  if (*core_id < 0) {
    DTRACE_E("ioctl HANTRODEC_IOCS_%s_RESERVE failed, %d\n", "DEC", *core_id);
    return DWL_ERROR;
  }
  random_exit();
  dev->dec_misc_ctrl[*core_id] |= SECURE_ENABLE((info->core_mask >> 31));
  dev->cb_func[*core_id] = NULL;
  dev->cb_arg[*core_id] = NULL;
  sem_trywait(&dev->sc_dec_rdy_sem[*core_id]);

  DTRACE_I("Reserved %s core %d\n", "DEC", *core_id);
#ifdef FPGA_PERF_AND_BW
  dev->bytes_consumed_perf[*core_id] = 0;
#endif

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLEnableHw
    Description     : Enable hw by writing to register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLEnableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core;
  u32 allow_intrabc = 0, dec_mode;
  u32 muti_core_support = 0;

  StackConsumption(__func__);

  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

#ifdef SUPPORT_MMU
#ifdef SUPPORT_48PA_MMU
  ioctl(dev->base.fd, (int)HANTRO_IOCS_MMU_SWITCH_PAGETABLE, &core_id);
#endif
#endif

#ifdef SUPPORT_SUBSYSTEM_TB
  DWLTbEnableAxiFe(dec_dwl, core_id, 1);
#endif

#ifdef SUPPORT_AXIFE
  if (!HW_G_ENABLE(dev->dec_misc_ctrl[core_id]))
    DWLEnableAxiFe(dec_dwl, core_id, SECURE_G_ENABLE(dev->dec_misc_ctrl[core_id]));
#endif

#ifdef FPGA_PERF_AND_BW
  dev->start_address_perf[core_id] = DWLReadStartAddress(core_id, dev->dec_shadow_regs);
#endif

  dec_mode = (DWLReadReg(dec_dwl, core_id, 4*3) >> 27) & 0x1F;
  if (dec_mode == DEC_MODE_AV1)
    allow_intrabc  = ((DWLReadReg(dec_dwl, core_id, 4*5) >> 4) & 0x1);
  muti_core_support = ((DWLReadReg(dec_dwl, core_id, 4*58) >> 30) & 0x1);
  /*cache*/
  u32 cache_e = 0;
  if (dec_mode == DEC_MODE_JPEG)
    cache_e = 0;
  else if (dec_mode < DEC_MODE_HEVC)
    cache_e = 0xa;  /* g1 */
  else if (allow_intrabc)
    cache_e = 0x188;  /* cache other channels, since ref/cbs are all non-cachable */
  else {
    if (muti_core_support) {
      if (dec_mode == DEC_MODE_VP9 || dec_mode == DEC_MODE_AV1)
        cache_e = 0x1812;  /* for tile-based multicore like VP9/AV1,
                              ref data/table is cachable for multicore */
      else
        cache_e = 0x18a;
    } else if (dec_mode == DEC_MODE_VVC)
      cache_e = 0x1802; /* DMV is uncachable for vvc */
    else
      cache_e = 0x1812;
  }
  DWLWriteReg(dec_dwl, core_id, 4*317, cache_e);
  /*recon_shaper*/
  if (allow_intrabc) {
    DWLWriteReg(dec_dwl, core_id, 4*3, (reg_base[3] & 0xFFFFFFF7));
  } else {
    DWLWriteReg(dec_dwl, core_id, 4*3, (reg_base[3] | 0x8));
  }

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
  if (!HW_G_ENABLE(dev->dec_misc_ctrl[core_id]))
    DWLDecF1Configure(instance, core_id);
#endif
#endif

  //DWLWriteReg(dec_dwl, core_id, 8, 0x400);
  //DWLWriteReg(dec_dwl, core_id, 4*58, 0x6210);
  DWLWriteReg(dec_dwl, core_id, offset, value);
  //dec_shadow_regs[core_id][13] = dec_shadow_regs[core_id][326];

  DTRACE_I("%s %d enabled by previous DWLWriteReg\n", "DEC", core_id);

  core.id = core_id;
  core.regs = reg_base;
  core.size = MAX_REG_COUNT * 4;
  core.type = HW_VCD;
  core.reg_id = 0;

#ifdef PERFORMANCE_TEST
  u32 i;
  for (i = 0; i < MAX_ASIC_CORES; i++) {
    if (HW_G_ENABLE(dev->dec_misc_ctrl[i]))
      break;
  }

  if (i == MAX_ASIC_CORES)
    ActivityTraceStartDec(&dev->activity);
#endif

  PERFORMANCE_STATIC_END(decode_pre_hw);
  /* For AVS, DWLEnableHw may be called again before exiting xxxDecDecode(),
     so need to end post_hw time statistic here. */
  PERFORMANCE_STATIC_END(decode_post_hw);
  PERFORMANCE_STATIC_START(decode_push_reg);

#ifdef SUPPORT_MMU
  ioctl(dev->base.fd, HANTRO_IOCS_MMU_FLUSH, &core.id);
#endif

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_PUSH_REG, &core)) {
    DTRACE_E("%s","ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
  random_exit();
  PERFORMANCE_STATIC_END(decode_push_reg);
  dev->dec_misc_ctrl[core_id] |= HW_ENABLE(1);

  pthread_mutex_lock(&dec_dwl->owner_mutex);
  dec_dwl->core_usage_counts[core_id]++;
  pthread_mutex_unlock(&dec_dwl->owner_mutex);

  if(dev->mc_listener_thread) {
    pthread_mutex_lock(&dev->job_mutex);
    dev->job_cnt++;
    pthread_cond_signal(&dev->job_cv);
    pthread_mutex_unlock(&dev->job_mutex);
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLWaitHwReady
    Description     : Wait until hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.

    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
enum DWLRet DWLWaitHwReady(const void *instance, i32 core_id, u32 timeout) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  i32 ret = DWL_HW_WAIT_OK;

  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

#ifndef DWL_USE_DEC_IRQ /* Polling */
  u32 irq_stats;
  /* XXX: when decoing large res JPEG/HEVC on FPGA platform in polling mode,
  this value maybe not enough, setting to 120000(120s) for FPGA */
  int max_wait_time = 240000; /* 120s->240s(for pjpeg 8137) in ms */
  random_exit();
  do {
    const unsigned int usec = 1000; /* 1 ms polling interval */

    ret = DWLProcessHwAsicStatus(dev, core_id);
    irq_stats = reg_base[HANTRODEC_IRQ_STAT_DEC];
    irq_stats = (irq_stats >> 11) & 0x5FFF;

    if (ret == DWL_HW_WAIT_OK) {
      if (irq_stats != 0) {
#ifdef PERFORMANCE_TEST
        ActivityTraceStopDec(&dev->activity);
#endif
#ifdef FPGA_PERF_AND_BW
        dev->bytes_consumed_perf[core_id] += DWLGetConsumedBytes(core_id, &dev->start_address_perf[0],reg_base);
#endif
        break; /* decoded success */
      }
      else
        max_wait_time--;
    } else
      break;
    usleep(usec);
  } while (max_wait_time > 0);
#else
  if (dev->mc_listener_thread) {
    sem_wait(&dev->sc_dec_rdy_sem[core_id]);
  }
  else {
    struct core_desc core;
    core.id = core_id;
    core.regs = reg_base;
    core.size = MAX_REG_COUNT * 4;
    core.type = HW_VCD;
    if (ioctl(dev->base.fd, HANTRODEC_IOCX_DEC_WAIT, &core)) {
      DTRACE_E("%s", "ioctl HANTRODEC_IOCX_DEC_WAIT failed\n");
      ASSERT(0);
      return DWL_HW_WAIT_ERROR;
    }
    DWLProcessHwAsicStatus(dev, core_id);
  }
#endif
  PERFORMANCE_STATIC_START(decode_post_hw);

  DTRACE_I("DEC IRQ by Core %d\n", core_id);

  (void) timeout;
  random_exit();
  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : DWLDisableHw
    Description     : Disable hw by writing to register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLDisableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  struct core_desc core = {0};

  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;
  u32 *reg_base = &dev->dec_shadow_regs[core_id];

  DWLWriteReg(dec_dwl, core_id, offset, value);

  DTRACE_I("%s %d disabled by previous DWLWriteReg\n", "DEC", core_id);

  core.id = core_id;
  core.regs = reg_base;
  core.size = MAX_REG_COUNT * 4;
  core.type = HW_VCD;
  core.reg_id = 0;

  if (ioctl(dev->base.fd, HANTRODEC_IOCS_DEC_PUSH_REG, &core)) {
    DTRACE_E("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }
}

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
enum DWLRet DWLReleaseHw(const void *instance, i32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;

  ASSERT(dec_dwl != NULL);
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  ASSERT((u32)core_id < dev->num_cores);
  u32 ret = DWL_OK;
  random_exit();
  if ((u32)core_id >= dev->num_cores) {
    ASSERT(0);
    return DWL_OK;
  }

  DTRACE_I(" %s core %d\n", "DEC", core_id);

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
  if(DWLDecF1Fuse(instance, core_id) == 0)
    DTRACE_I("DECF1[%d] flush success!!\n", core_id);
  else
    DTRACE_E("DECF1[%d] dec flush failed!!\n", core_id);
#endif
#endif

#ifdef SUPPORT_SUBSYSTEM_TB
  DWLTbDisableAxiFe(dec_dwl, core_id);
#endif

  ioctl(dev->base.fd, HANTRODEC_IOCT_DEC_RELEASE, &core_id);
  random_exit();
#ifdef FPGA_PERF_AND_BW
#ifdef SUPPORT_AXIFE
  dev->bw_axife_rd_wr[core_id][0] = DWLReadAxiFeBw(dec_dwl, core_id, 0) - dec_dwl->save_rd;
  dev->save_rd = DWLReadAxiFeBw(dec_dwl, core_id, 0);
  dev->bw_axife_rd_wr[core_id][1] = DWLReadAxiFeBw(dec_dwl, core_id, 1) - dec_dwl->save_wr;
  dev->save_wr = DWLReadAxiFeBw(dec_dwl, core_id, 1);
#endif
#endif

#ifdef SUPPORT_AXIFE
  //DWLDisableAxiFe(dec_dwl, core_id);
#endif

  return ret;
}

static enum DWLRet DWLProcessHwAsicStatus(DWLDecNode *dev, u32 id) {
  struct core_desc core;
  u32 irq_stats;
  u32 *reg_base = &dev->dec_shadow_regs[id];
  int fd = dev->base.fd;

#ifndef DWL_USE_DEC_IRQ
  static u32 count = 0;
  int max_wait_time = 240000;
#endif

  core.id = id;
  core.regs = &reg_base[HANTRODEC_IRQ_STAT_DEC];
  core.size = 4;
  core.reg_id = 1;
  core.type = HW_VCD;

  DTRACE_I("DEC IRQ by Core %d\n", id);

  if (ioctl(fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
    DTRACE_E("%s", "ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
    return DWL_HW_WAIT_ERROR;
  }

  irq_stats = reg_base[HANTRODEC_IRQ_STAT_DEC];
  /* If SW timeout is triggered, ABORT HW.*/
  if (((irq_stats & DEC_ENABLE) && ((irq_stats >> 11) & 0xFF) == 0)
#ifndef DWL_USE_DEC_IRQ
        && count++ >= max_wait_time
#endif
      ) {
    reg_base[HANTRODEC_IRQ_STAT_DEC] |= DEC_ABORT;
    core.regs = &reg_base[HANTRODEC_IRQ_STAT_DEC];
    if (ioctl(fd, HANTRODEC_IOCS_DEC_WRITE_REG, &core)) {
      DTRACE_E("%s", "ioctl HANTRODEC_IOCS_*_WRITE_REG failed\n");
      ASSERT(0);
      return DWL_HW_WAIT_ERROR;
    }
#ifdef DWL_USE_DEC_IRQ
    if (ioctl(fd, HANTRODEC_IOCG_CORE_WAIT, &id)) {
      DTRACE_E("%s", "ioctl HANTRODEC_IOCG_CORE_WAIT failed\n");
      ASSERT(0);
      return DWL_HW_WAIT_ERROR;
    }
#endif
    if (ioctl(fd, HANTRODEC_IOCS_DEC_READ_REG, &core)) {
      DTRACE_E("%s", "ioctl HANTRODEC_IOCS_DEC_READ_REG failed\n");
      ASSERT(0);
      return DWL_HW_WAIT_ERROR;
    }
    irq_stats = reg_base[HANTRODEC_IRQ_STAT_DEC];
  }

  irq_stats = (irq_stats >> 11) & 0x5FFF;

  if (irq_stats != 0) {
    dev->dec_misc_ctrl[id] &= (0xFFFFFFFE | HW_ENABLE(0));
    core.regs = &reg_base[0];
    core.size = MAX_REG_COUNT * 4;
#ifndef DWL_USE_DEC_IRQ
    count = 0;
#endif
    PERFORMANCE_STATIC_START(decode_pull_reg);
    /* Pull all registers when hw fininshed */
    if (ioctl(fd, HANTRODEC_IOCS_DEC_PULL_REG, &core)) {
      DTRACE_E("%s", "ioctl HANTRODEC_IOCS_*_PULL_REG failed\n");
      ASSERT(0);
      return DWL_HW_WAIT_ERROR;
    }
    PERFORMANCE_STATIC_END(decode_pull_reg);
    irq_stats = reg_base[HANTRODEC_IRQ_STAT_DEC];
    PrintIrqType(id, irq_stats);
  }

  return DWL_HW_WAIT_OK;
}

static enum DWLRet DWLCreateMcListenerThread(DWLDecNode *dev) {
  u32 i;
  pthread_attr_t attr;

  /* just create once, if listener thread exit, need to recreate when multi core */
  if(dev->mc_listener_thread == NULL) {
    dev->mc_listener_thread = (pthread_t *)DWLmalloc(sizeof(pthread_t));
    if (dev->mc_listener_thread == NULL) {
      DTRACE_E("%s", "malloc pthread_t failed, please check\n");
      return DWL_ERROR;
    }

    pthread_mutex_init(&dev->job_mutex, NULL);
    pthread_mutex_init(&dev->job_done_mutex, NULL);
    pthread_cond_init(&dev->job_cv, NULL);

    pthread_attr_init(&attr);
    if (pthread_create(dev->mc_listener_thread, &attr, ThreadMCListener, dev) != 0) {
      DWLfree(dev->mc_listener_thread);
      pthread_attr_destroy(&attr);
      DTRACE_E("%s", "the listener thread create error, please check\n");
      return DWL_ERROR;
    }
    pthread_attr_destroy(&attr);
    for (i = 0; i < MAX_MC_CB_ENTRIES; i++)
      sem_init(&dev->sc_dec_rdy_sem[i], 0, 0);
  }

  return DWL_OK;
}

void DWLSetIRQCallback(const void *instance, i32 core_id,
                       DWLIRQCallbackFn *callback_fn, void *arg) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dec_dwl->dev;

  dev->cb_func[core_id] = callback_fn;
  dev->cb_arg[core_id] = arg;
  /* when vcmd single core, the listeren thread is unnecessary */
  if(callback_fn != NULL) {
    pthread_mutex_lock(&dev->base._mutex);
    DWLCreateMcListenerThread(dev);
    pthread_mutex_unlock(&dev->base._mutex);
  }
}

/* Reserve one valid command buffer. */
enum DWLRet DWLReserveCmdBuf(const void *instance, struct DWLReqInfo *info, u32 *cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  enum DWLRet ret;
  struct exchange_parameter params = {0};
  u32 width;
  u32 height;

  width = info->width;
  height = info->height;
  params.executing_time = width * height;
  params.module_type = VCMD_TYPE_DECODER;
  params.core_mask = info->core_mask & 0xffff;
  params.owner = info->owner;

  ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_RESERVE_CMDBUF, &params);
  if (ret < 0) {
    DTRACE_E("%s", "DWLReserveCmdBuf failed\n");
    return DWL_ERROR;
  }
  else{
    u32 cmdbuf_id = params.cmdbuf_id;
    ASSERT(cmdbuf_id < MAX_VCMD_ENTRIES);

    dev->vcmdb[cmdbuf_id].cmd_buf_size = params.cmdbuf_size;
    dev->vcmdb[cmdbuf_id].cmd_buf_used = 0;
    dev->vcmdb[cmdbuf_id].cmd_buf = (u8 *)dev->vcmd_mem_params.virt_cmdbuf_addr +
                            dev->vcmd_mem_params.cmdbuf_unit_size * cmdbuf_id;
    dev->vcmdb[cmdbuf_id].status_buf = (u8 *)dev->vcmd_mem_params.virt_status_cmdbuf_addr +
                            dev->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dev->vcmdb[cmdbuf_id].status_bus_addr = dev->vcmd_mem_params.phy_status_cmdbuf_addr -
                            dev->vcmd_mem_params.base_ddr_addr +
                            dev->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dev->vcmdb[cmdbuf_id].mmu_status_bus_addr = dev->vcmd_mem_params.mmu_phy_status_cmdbuf_addr +
                            dev->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dev->vcmdb[cmdbuf_id].secure_mode = (info->core_mask >> 31);
    *cmd_buf_id = cmdbuf_id;
    if (cmdbuf_id < MAX_MC_CB_ENTRIES) {
      dev->cb_func[cmdbuf_id] = NULL;
      dev->cb_arg[cmdbuf_id] = NULL;
      // try to consume the semaphore with cmd_buf_id to prevent the case
      // that ListenerThread maybe sem_post, but DWLWaitCmdBufReady has seet the ioctl for the same cmd_buf_id
      sem_trywait(dev->sc_dec_rdy_sem + *cmd_buf_id);
    }

    DTRACE_I("reserve cmd buf %d\n", cmdbuf_id);
    random_exit();
  }

  return DWL_OK;
}

/* Enable one command buffer: link and enable.
 * Note: The first command of cmd buf should be RREG to read cmd buf id,
 *       other commands should be behind of the RREG command.*/
enum DWLRet DWLEnableCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];
  struct exchange_parameter params = {0};
  i32 ret;
  u32 read_reg_num = 0;
  addr_t status_buf_addr = 0;
  u32 allow_intrabc = 0, dec_mode;
  u32 muti_core_support = 0;

  dec_mode = (vcmd->reg_mirror[3] >> 27) & 0x1F;
  /*Set the buffer empty interrupt to abnormal interrupt. */
  if ((dev->vcmd_params.vcmd_hw_version_id >= HW_ID_1_5_0) &&
      ((dec_mode == DEC_MODE_HEVC) || (dec_mode == DEC_MODE_H264) ||
       (dec_mode == DEC_MODE_H264_H10P))) {
  	vcmd->reg_mirror[297] = 0x404000;
  } else {
  	vcmd->reg_mirror[3] |= 0x40;
  }
  if (dec_mode == DEC_MODE_AV1)
    allow_intrabc  = ((vcmd->reg_mirror[5] >> 4) & 0x01);
  muti_core_support = ((vcmd->reg_mirror[58] >> 30) & 0x1);
  /*cache*/
  u32 cache_e;
  if (dec_mode == DEC_MODE_JPEG)
    cache_e = 0;
  else if (dec_mode < DEC_MODE_HEVC)
    cache_e = 0xa;  /* g1 */
  else if (allow_intrabc)
    cache_e = 0x188;  /* cache other channels, since ref/cbs are all non-cachable */
  else {
    if (muti_core_support) {
      if (dec_mode == DEC_MODE_VP9 || dec_mode == DEC_MODE_AV1)
        cache_e = 0x1812;  /* for tile-based multicore like VP9/AV1,
                              ref data/table is cachable for multicore */
      else
        cache_e = 0x18a;
    } else if (dec_mode == DEC_MODE_VVC)
      cache_e = 0x1802; /* DMV is uncachable for vvc */
    else
      cache_e = 0x1812;
  }
  vcmd->reg_mirror[317] = cache_e;
  /*recon_shaper*/
  if (allow_intrabc) {
    vcmd->reg_mirror[3] &= 0xFFFFFFF7;
  } else {
    vcmd->reg_mirror[3] |= 0x8;
  }
  random_exit();

  /****************************************************************************/
  /* Start to generate VCMD instructions. */
  if (dev->vcmd_params.vcmd_hw_version_id > HW_ID_1_0_C) {
    /* Read VCMD buffer ID (last VCMD registers). */
    CWLCollectReadRegData(vcmd,
                          26, 1, /* VCMD command buffer ID register */
                          0);
  /* Not gate the abnormal interrupt from IPs to CPU.*/
    if (dev->vcmd_params.vcmd_hw_version_id >= HW_ID_1_5_0) {
      u32 reg_25 = 0xfeffffff;
      CWLCollectWriteRegData(vcmd, &reg_25, 25 , 1);
    }
  }
#ifdef SUPPORT_VCMD_M2M
  /* M2M mv data  */
  if(DEV_HAS_M2M(dev)) {
    struct CDF_INFO *cdf_info = NULL;
    for(int i = 0; i < MAX_VCMD_M2M_NUM; i++) {
      cdf_info = &vcmd->vcmd_data_mv.before_dec[i];
      if(cdf_info->cdf_size > 0)
        CWLCollectM2MData(vcmd, cdf_info->dst_prob_addr, cdf_info->src_cdf_addr, cdf_info->cdf_size);
      else
        break;
    }
  }
#endif

#ifdef SUPPORT_AXIFE
  u32 hw_secure_mode = vcmd->secure_mode;
  DWLConfigureCmdBufForAxiFe(dwl_inst, cmd_buf_id, hw_secure_mode);
#endif

#ifdef SUPPORT_MMU
#ifdef SUPPORT_48PA_MMU
  DWLSwitchMMUPageTableByCmdBuf(dwl_inst, cmd_buf_id);
#endif
  DWLFlushCmdBufForMMU(dwl_inst, cmd_buf_id);
#endif

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
  DWLConfigureCmdBufForDec400(dwl_inst, cmd_buf_id);
#endif
#endif

  /* Configure VCD instruction */
#if 0
  if (IS_DECMODE_JPEG(vcmd->reg_mirror[3])) {
    /* JPEG */
    int reg_seg, reg_segs;
    /* Modify file following register definition for different pp configuration. */
    struct RegFmtDef jpeg_regs_def[] = {{2, 7}, {12, 1}, {15, 23}, {50, 1}, {54, 1}, {56, 3}, {60, 8}, {132, 2},
                                        {168, 2}, {174, 2}, {258, 3}, {265, 3}, {299, 16}, {317, 23}, {352, 61}};

    struct RegFmtDef *reg_def;
    {
      reg_segs = sizeof(jpeg_regs_def)/sizeof(jpeg_regs_def[0]);
      reg_def = jpeg_regs_def;
    }

    for (reg_seg = 0; reg_seg < reg_segs; reg_seg++) {
      int si = reg_def[reg_seg].start_index;
      int num = reg_def[reg_seg].continous_num;
      CWLCollectWriteRegData(vcmd,
                             &vcmd->reg_mirror[si],
                             dev->vcmd_params.submodule_main_addr/4 + si, /* set continous registers sections */
                             num);
#ifndef DWL_DISABLE_REG_PRINTS
      for (int tmp = si; tmp < num + si; tmp++) {
        DTRACE_E("swreg[%d] at offset 0x%02X = %08X\n", tmp,
                  tmp*4, vcmd->reg_mirror[tmp]);
      }
#endif
    }
  } else
#endif
  {
    random_exit();
    /* flush all regs for all formats to vcmd_buf. */
    CWLCollectWriteRegData(vcmd,
                           &vcmd->reg_mirror[2],
                           dev->vcmd_params.submodule_main_addr/4 + 2 , /* register offset in bytes to vcmd base address */
                           MAX_REG_COUNT - 2);
  }

  /* Write swreg1 to enable dec. */
  CWLCollectWriteRegData(vcmd,
                        &vcmd->reg_mirror[0],
                         dev->vcmd_params.submodule_main_addr/4 + 0, /* register offset in bytes to vcmd base address */
                         2);
  random_exit();
  /* Wait for interruption */
  CWLCollectStallData(vcmd,
                      VCD_FRAME_RDY_INT_MASK);
#ifdef SUPPORT_VCMD_M2M
  /* M2M mv data  */
  if(DEV_HAS_M2M(dev)){
    struct CDF_INFO *cdf_info = NULL;
    for(int i = 0; i < MAX_VCMD_M2M_NUM; i++) {
      cdf_info = &vcmd->vcmd_data_mv.after_dec[i];
      if(cdf_info->cdf_size > 0)
        CWLCollectM2MData(vcmd, cdf_info->dst_prob_addr, cdf_info->src_cdf_addr, cdf_info->cdf_size);
      else
        break;
    }
  }
#endif

#ifdef SUPPORT_MMU
  status_buf_addr = vcmd->mmu_status_bus_addr;
#else
  status_buf_addr = vcmd->status_bus_addr;
#endif

  status_buf_addr += dev->vcmd_params.submodule_main_addr/2;
  /* Read swreg0 for debug */
  CWLCollectReadRegData(vcmd,
                      dev->vcmd_params.submodule_main_addr/4 + 0, 1, /* swreg0 */
                      status_buf_addr + 4 * read_reg_num);
  read_reg_num += 1;
  /* Read status (swreg1) register */
  CWLCollectReadRegData(vcmd,
                      dev->vcmd_params.submodule_main_addr/4 + 1, 1, /* swreg1 */
                      status_buf_addr + 4 * read_reg_num);
  read_reg_num += 1;
  /* swreg261 HWIF_ERROR_INFO*/
  CWLCollectReadRegData(vcmd,
                      dev->vcmd_params.submodule_main_addr/4 + 261, 1, /* swreg261 */
                      status_buf_addr + 4 * read_reg_num);
  read_reg_num += 1;
  /* swreg270 HWIF_TOTAL_ERROR_CTBS*/
  CWLCollectReadRegData(vcmd,
                      dev->vcmd_params.submodule_main_addr/4 + 270, 1, /* swreg270 */
                      status_buf_addr + 4 * read_reg_num);
  read_reg_num += 1;
  /* swreg168/169 */
  CWLCollectReadRegData(vcmd,
                      dev->vcmd_params.submodule_main_addr/4 + 168, 2, /* swreg168/169 */
                      status_buf_addr + 4 * read_reg_num);
  read_reg_num += 2;
  ASSERT((dev->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);
  /* swreg62/63 - sw_cu_location/sw_perf_cycle_count */
  CWLCollectReadRegData(vcmd,
                      dev->vcmd_params.submodule_main_addr/4 + 62, 2, /* swreg62/63 */
                      status_buf_addr + 4 * read_reg_num);
  read_reg_num += 2;
  random_exit();
  if (IS_DECMODE_VP78(vcmd->reg_mirror[3])) {
    /* swreg7/8 */
    CWLCollectReadRegData(vcmd,
                        dev->vcmd_params.submodule_main_addr/4 + 7, 2, /* swreg7/8 */
                        status_buf_addr + 4 * read_reg_num);
    read_reg_num += 2;
  }

  ASSERT((dev->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);

  /* Clear decoder int */
  CWLCollectClrIntData(vcmd,
                       CLRINT_OPTYPE_READ_WRITE_0_CLEAR,
                       dev->vcmd_params.submodule_main_addr/4 + 1,  /* swreg1 */
                       0x3FFFF00);
  ASSERT((dev->vcmdb[cmd_buf_id].cmd_buf_used & 3) == 0);

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
  DWLFuseCmdBufForDec400(instance, cmd_buf_id, &read_reg_num);
#endif
#endif

  if (dev->vcmd_params.vcmd_hw_version_id > HW_ID_1_0_C) {
    /* Dump all vcmd registers. */
    CWLCollectReadRegData(vcmd,
                          0, 27, /* VCMD registers count */
                          4 * read_reg_num);
    read_reg_num += 27;
  }

#ifdef USE_END_CMD
  /* end */
  CWLCollectEndData(vcmd);
#else
  /* Jmp */
  CWLCollectJmpData(vcmd);
#endif

  /* End of command buffer instructions. */
  /****************************************************************************/
  #ifdef VCD_LOGMSG
    for (int tmp = 2; tmp < MAX_REG_COUNT - 2; tmp++) {
      REGTRACE_I("Write swreg[%d] at offset 0x%02X = %08X\n", tmp,
                tmp*4, vcmd->reg_mirror[tmp]);
    }
  #endif
  params.cmdbuf_size = dev->vcmdb[cmd_buf_id].cmd_buf_used;
  params.cmdbuf_id = cmd_buf_id;
  params.input_mask = 0;
#ifdef USE_END_CMD
  /* set end cmd bit into input_mask */
  EXCH_S_BIT(params.input_mask, EXCH_END_CMD_BIT);
#endif
  ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_LINK_RUN_CMDBUF, &params);
  if (ret < 0) {
    DTRACE_E("%s", "DWLEnableCmdBuf failed\n");
    return DWL_ERROR;
  }
  random_exit();
  DTRACE_I("enable cmd buf %d\n", cmd_buf_id);

  if(dev->mc_listener_thread) {
    pthread_mutex_lock(&dev->job_mutex);
    dev->job_cnt++;
    pthread_cond_signal(&dev->job_cv);
    pthread_mutex_unlock(&dev->job_mutex);
  }

  vcmd->core_id = params.core_id;
#ifdef FPGA_PERF_AND_BW
#ifdef SUPPORT_AXIFE
  DWLEnableAxiFe(dwl_inst, vcmd->core_id , hw_secure_mode);
#endif
#endif

  pthread_mutex_lock(&dwl_inst->owner_mutex);
  dwl_inst->core_usage_counts[vcmd->core_id]++;
  pthread_mutex_unlock(&dwl_inst->owner_mutex);

  return DWL_OK;
}

/* Wait cmd buffer ready. */
enum DWLRet DWLWaitCmdBufReady(const void *instance, u16 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 ret;
  u16 core_info_hw = cmd_buf_id;
  u32 * status = NULL;
  /* Check invalid parameters */
  if(dwl_inst == NULL)
    return DWL_ERROR;

  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmd_buf_id];

  random_exit();
  DTRACE_I("%s", "DWLWaitCmdBufReady\n");
  if(dev->mc_listener_thread) {
    ret = sem_wait(&dev->sc_dec_rdy_sem[cmd_buf_id]);
  }
  else {
    ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_WAIT_CMDBUF, &core_info_hw);
  }
  if (ret < 0) {
    DTRACE_E("%s", "DWLWaitCmdBufReady failed\n");
    return DWL_HW_WAIT_ERROR;
  } else {
    DTRACE_I("DWLWaitCmdBufReady %d succeed\n", cmd_buf_id);
    status = (u32 *)(vcmd->status_buf + dev->vcmd_params.submodule_main_addr/2);
    vcmd->reg_mirror[1] = status[1];
    vcmd->reg_mirror[261] = status[2];
    vcmd->reg_mirror[270] = status[3];
    vcmd->reg_mirror[168] = status[4];
    vcmd->reg_mirror[169] = status[5];
    vcmd->reg_mirror[62] = status[6];
    vcmd->reg_mirror[63] = status[7];
    if (IS_DECMODE_VP78(vcmd->reg_mirror[3])) {
      vcmd->reg_mirror[7] = status[8];
      vcmd->reg_mirror[8] = status[9];
    }
    #ifdef VCD_LOGMSG
    for (int tmp = 2; tmp < MAX_REG_COUNT - 2; tmp++) {
      REGTRACE_I("Read swreg[%d] at offset 0x%02X = %08X\n", tmp,
                tmp*4, vcmd->reg_mirror[tmp]);
    }
    #endif
  }
  random_exit();
  u32 irq_stats =  vcmd->reg_mirror[HANTRODEC_IRQ_STAT_DEC];
  PrintIrqType(0, irq_stats);
  return DWL_OK;
}

/* Release one command buffer. */
enum DWLRet DWLReleaseCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  i32 ret;

  ASSERT(cmd_buf_id < MAX_VCMD_ENTRIES);

  random_exit();
  ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_RELEASE_CMDBUF, &cmd_buf_id);
  random_exit();
  if (ret < 0) {
     DTRACE_E("%s", "DWLReleaseCmdBuf failed\n");
     return DWL_ERROR;
  }
  random_exit();
  DTRACE_I("release cmd buf %d\n", cmd_buf_id);

  return DWL_OK;
}

enum DWLRet DWLFlushRegister(const void *instance, u32 cmd_buf_id, u32 *dec_regs, u32 *mc_fresh_regs, u32 mc_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev;
  struct VcmdBuf *vcmd;

  if (dwl_inst==NULL || cmd_buf_id>=MAX_VCMD_ENTRIES || !dec_regs) {
    return DWL_ERROR;
  }

  dev = (DWLDecNode *)dwl_inst->dev;

  vcmd = &dev->vcmdb[cmd_buf_id];

  vcmd->reg_mirror = dec_regs;
  vcmd->mc_fresh_reg_mirror = mc_fresh_regs;
  vcmd->mc_buf_id = mc_buf_id;

  return DWL_OK;
}

enum DWLRet DWLRefreshRegister(const void *instance, u32 cmd_buf_id, u32 *dec_regs) {
  //nothing needs to do, already refresh in DWLWaitCmdBufReady().
  return DWL_OK;
}

enum DWLRet DWLVcmdMCRefreshStatusRegs(const void *instance, u32 *dec_regs, u32 cmdbuf_id)
{
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmdbuf_id];

  dec_regs[0] = vcmd->mc_fresh_reg_mirror[0];	  //0-0
  dec_regs[1] = vcmd->mc_fresh_reg_mirror[1];	  //1-1
  dec_regs[261] = vcmd->mc_fresh_reg_mirror[2];	  //2-261
  dec_regs[270] = vcmd->mc_fresh_reg_mirror[3];	  //3-270
  dec_regs[168] = vcmd->mc_fresh_reg_mirror[4]; //4-168
  dec_regs[169] = vcmd->mc_fresh_reg_mirror[5];	//5-169
  dec_regs[62] = vcmd->mc_fresh_reg_mirror[6];	//6-62
  dec_regs[63] = vcmd->mc_fresh_reg_mirror[7];	//7-63

 return DWL_OK;
}

void DWLFillSliceRegs(u32 *regs, struct subsys_regs_desc *slice_regs,
                      u32 cmd_buf_id) {
  struct reg_desc *regs_off;

  slice_regs->id = cmd_buf_id;
  slice_regs->reg_num = 10;

  ASSERT(slice_regs->reg_num <= STORE_REGS_NUM);

  regs_off = &slice_regs->regs[0];
  regs_off->reg_id = 5;
  regs_off->reg_data = regs[5];
  regs_off++;
  regs_off->reg_id = 6;
  regs_off->reg_data = regs[6];
  regs_off++;
  regs_off->reg_id = 8;
  regs_off->reg_data = regs[8];
  regs_off++;
  regs_off->reg_id = 12;
  regs_off->reg_data = regs[12];
  regs_off++;
  regs_off->reg_id = 122;
  regs_off->reg_data = regs[122];
  regs_off++;
  regs_off->reg_id = 168;
  regs_off->reg_data = regs[168];
  regs_off++;
  regs_off->reg_id = 169;
  regs_off->reg_data = regs[169];
  regs_off++;
  regs_off->reg_id = 258;
  regs_off->reg_data = regs[258];
  regs_off++;
  regs_off->reg_id = 259;
  regs_off->reg_data = regs[259];
  regs_off++;
  regs_off->reg_id = 1;
  regs_off->reg_data = regs[1];
}

void DWLCmdPushSliceRegs(const void *instance, u32 cmd_buf_id, u32 *regs) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct subsys_regs_desc slice_regs;
  int ret;

  DWLFillSliceRegs(regs, &slice_regs, cmd_buf_id);

  ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_PUSH_SLICE_REG, &slice_regs);
  if (ret) {
    DTRACE_E("%s", "ioctl HANTRODEC_IOCS_*_PUSH_REG failed\n");
    ASSERT(0);
  }

  if(dev->mc_listener_thread) {
    pthread_mutex_lock(&dev->job_mutex);
    dev->job_cnt++;
    pthread_cond_signal(&dev->job_cv);
    pthread_mutex_unlock(&dev->job_mutex);
  }
}

void DWLAbortCmdbuf(const void *instance, u16 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  i32 ret;

  ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_ABORT_CMDBUF, &cmd_buf_id);
  if (ret) {
    DTRACE_E("%s", "ioctl HANTRO_VCMD_IOCH_ABORT_SLICE failed\n");
    ASSERT(0);
  }

  if(dev->mc_listener_thread) {
    pthread_mutex_lock(&dev->job_mutex);
    dev->job_cnt++;
    pthread_cond_signal(&dev->job_cv);
    pthread_mutex_unlock(&dev->job_mutex);
  }

  DWLWaitCmdBufReady(instance, cmd_buf_id);
  if (!dev->cb_func[cmd_buf_id])
    DWLReleaseCmdBuf(instance, cmd_buf_id);
}

void DWLWaitOwnerDone(const void *instance, const void *owner) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  i32 ret;

  while (1) {
    ret = ioctl(dev->base.fd, HANTRODEC_IOCH_WAIT_OWNER_DONE, owner);
    if (ret == 0)
      break;
    else if (ret < 0) {
      DTRACE_E("%s", "ioctl HANTRO_VCMD_IOCH_DROP_CMDBUF failed\n");
      ASSERT(0);
    }
    sched_yield();
  }
}

void DWLDropCmdbufs(const void *instance, const void *owner) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  i32 ret;
  u16 dropped_cmdbuf_num = 0;

  ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_DROP_OWNER, owner);
  if (ret < 0) {
    DTRACE_E("%s", "ioctl HANTRO_VCMD_IOCH_DROP_CMDBUF failed\n");
    ASSERT(0);
  }
  dropped_cmdbuf_num = ret;

  if (dropped_cmdbuf_num && dev->mc_listener_thread) {
    pthread_mutex_lock(&dev->job_done_mutex);
    dev->job_done_cnt += dropped_cmdbuf_num;
    pthread_mutex_unlock(&dev->job_done_mutex);
  }
}

i32 DWLGetVcmdMCVirtualCoreId(const void *instance, u32 cmdbuf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmdbuf_id];

  return vcmd->mc_buf_id;
}

struct RegFmtDef {
  int start_index;
  int continous_num;
};

#ifdef SUPPORT_VCMD_M2M
/* use vcmd M2M transfer data */
void DWLCmdM2MSendData(const void *instance, struct CDF_INFO *head_data,
  struct CDF_INFO *tail_data, u32 cmdbuf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  struct VcmdBuf *vcmd = &dev->vcmdb[cmdbuf_id];

  vcmd->vcmd_data_mv.before_dec = head_data;
  vcmd->vcmd_data_mv.after_dec = tail_data;
}
#endif

enum DWLRet DWLWaitCmdbufsDone(const void *instance, const void *owner) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;
  i32 ret;

  while (1) {
    ret = ioctl(dev->base.fd, HANTRO_VCMD_IOCH_WAIT_OWNER_DONE, owner);
    if (ret == 0)
      break;
    sched_yield();
  }

  return DWL_OK;
}

void DWLGetActiveTime(const void *instance, unsigned long long *tile_active_time, unsigned long long *active_time) {
#ifdef PERFORMANCE_TEST
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  DWLDecNode *dev = (DWLDecNode *)dwl_inst->dev;

  *tile_active_time = dev->activity.tile_active_time;
  *active_time = dev->activity.active_time;
#endif
}

static enum DWLRet DWLCheckInitParam(struct DWLInitParam *param) {
  switch (param->client_type) {
    case DWL_CLIENT_TYPE_H264_DEC:
    case DWL_CLIENT_TYPE_MPEG4_DEC:
    case DWL_CLIENT_TYPE_JPEG_DEC:
    case DWL_CLIENT_TYPE_VC1_DEC:
    case DWL_CLIENT_TYPE_MPEG2_DEC:
    case DWL_CLIENT_TYPE_VP6_DEC:
    case DWL_CLIENT_TYPE_VP8_DEC:
    case DWL_CLIENT_TYPE_RV_DEC:
    case DWL_CLIENT_TYPE_AVS_DEC:
    case DWL_CLIENT_TYPE_HEVC_DEC:
    case DWL_CLIENT_TYPE_VP9_DEC:
    case DWL_CLIENT_TYPE_AVS2_DEC:
    case DWL_CLIENT_TYPE_AV1_DEC:
    case DWL_CLIENT_TYPE_VVC_DEC:
    case DWL_CLIENT_TYPE_ST_PP: {
      break;
    }
    default: {
      DTRACE_E("Unknown client type no. %u\n", param->client_type);
      return DWL_ERROR;
    }
  }

  if (strlen(param->dec_dev) == 0) {
    DTRACE_E("%s", "please specify the device name!\n");
    return DWL_ERROR;
  }

  if (strlen(param->mem_dev) == 0) {
    DTRACE_E("%s", "please specify the memory allocator device name!\n");
    return DWL_ERROR;
  }

#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
  if (strlen(param->dma_dev) == 0) {
    DTRACE_E("%s", "please specify the dma device name!\n");
    return DWL_ERROR;
  }
#endif

  return DWL_OK;
}

static enum DWLRet DWLInitDevName(char **dev_name, const char *node_name) {
  *dev_name = DWLmalloc(strlen(node_name) + 1);

  if (*dev_name == NULL) {
    DTRACE_E("%s","DWLInitDevName malloc dev_name failed\n");
    return DWL_ERROR;
  }
  if (*dev_name)
    strcpy(*dev_name, node_name);

  return DWL_OK;
}

static enum DWLRet DWLAllocDecNodeResource(DWLDecNode *dev) {
  if (DEV_USE_VCMD(dev)) {
    ASSERT(MAX_VCMD_ENTRIES == SLOT_NUM_CMDBUF);

    dev->vcmdb = (struct VcmdBuf *)DWLcalloc(1, MAX_VCMD_ENTRIES * sizeof(struct VcmdBuf));
    if (dev->vcmdb == NULL) {
      DTRACE_E("%s","malloc struct VcmdBuf failed\n");
      return DWL_ERROR;
    }
    /* Get VCMD configuration. */
    dev->vcmd_params.module_type = VCMD_TYPE_DECODER;
    if (ioctl(dev->base.fd, HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER, &dev->vcmd_params) == -1) {
      DTRACE_E("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      return DWL_ERROR;
    }

    if (ioctl(dev->base.fd, HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER, &dev->vcmd_mem_params) == -1) {
      DTRACE_E("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      return DWL_ERROR;
    }

    dev->vcmd_mem_params.virt_cmdbuf_addr =
      (u32 *) mmap(0, dev->vcmd_mem_params.cmdbuf_total_size, PROT_READ|PROT_WRITE,
                   MAP_SHARED, dev->base.fd,
                   dev->vcmd_mem_params.phy_cmdbuf_addr);
    dev->vcmd_mem_params.virt_status_cmdbuf_addr =
      (u32 *) mmap(0, dev->vcmd_mem_params.status_cmdbuf_total_size, PROT_READ|PROT_WRITE,
                   MAP_SHARED, dev->base.fd,
                   dev->vcmd_mem_params.phy_status_cmdbuf_addr);
    dev->vcmd_mem_params.virt_vcmd_regbuf_addr =
      (u32 *) mmap(0, dev->vcmd_mem_params.vcmd_regbuf_total_size, PROT_READ,
                   MAP_SHARED, dev->base.fd,
                   dev->vcmd_mem_params.phy_vcmd_regbuf_addr);
  } else {
    struct regsize_desc reg_size = {0};
    reg_size.type = HW_VCD;
    if (ioctl(dev->base.fd, HANTRODEC_IOCGHWIOSIZE, &reg_size) == -1) {
      DTRACE_E("%s","ioctl HANTRODEC_IOCGHWIOSIZE failed\n");
      return DWL_ERROR;
    }
    if (reg_size.size != MAX_REG_COUNT * sizeof(u32)) {
      DTRACE_E("%s","failed dec reg size from kernel\n");
      return DWL_ERROR;
    }
    dev->dec_shadow_regs = (u32 *)DWLcalloc(1, sizeof(u32) * MAX_ASIC_CORES * MAX_REG_COUNT);
    if (dev->dec_shadow_regs == NULL) {
      DTRACE_E("%s","malloc shadow_regs failed\n");
      return DWL_ERROR;
    }
#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
    dev->dec400_shadow_regs = (u32 *)DWLcalloc(1, sizeof(u32) * MAX_ASIC_CORES * DEC400_REG_NUM);
    if (dev->dec400_shadow_regs == NULL) {
      DTRACE_E("%s","malloc shadow_regs failed\n");
      return DWL_ERROR;
    }
#endif
#endif

#ifdef SUPPORT_AXIFE
    dev->axife_shadow_regs = (u32 *)DWLcalloc(1, sizeof(u32) * MAX_ASIC_CORES * AXIFE_REG_NUM);
    if (dev->axife_shadow_regs == NULL) {
      DTRACE_E("%s","malloc shadow_regs failed\n");
      return DWL_ERROR;
    }
#endif
  }

  return DWL_OK;
}

static void DWLFreeDecNodeResource(DWLDecNode *dev) {
  u32 i;

  if (DEV_USE_VCMD(dev)) {
    if (dev->vcmdb)
      DWLfree(dev->vcmdb);

    if (dev->vcmd_mem_params.virt_cmdbuf_addr != MAP_FAILED)
      munmap(dev->vcmd_mem_params.virt_cmdbuf_addr,
             dev->vcmd_mem_params.cmdbuf_total_size);
    if (dev->vcmd_mem_params.virt_status_cmdbuf_addr != MAP_FAILED)
      munmap(dev->vcmd_mem_params.virt_status_cmdbuf_addr,
             dev->vcmd_mem_params.status_cmdbuf_total_size);
    if (dev->vcmd_mem_params.virt_vcmd_regbuf_addr != MAP_FAILED)
      munmap(dev->vcmd_mem_params.virt_vcmd_regbuf_addr,
             dev->vcmd_mem_params.vcmd_regbuf_total_size);
  }
  else {
    if (dev->dec_shadow_regs)
      DWLfree(dev->dec_shadow_regs);
    if (dev->dec400_shadow_regs)
      DWLfree(dev->dec400_shadow_regs);
    if (dev->axife_shadow_regs)
      DWLfree(dev->axife_shadow_regs);
  }

  if (dev->mc_listener_thread) {
    for (i = 0; i < MAX_MC_CB_ENTRIES; i++)
      sem_destroy(&dev->sc_dec_rdy_sem[i]);

    pthread_mutex_destroy(&dev->job_mutex);
    pthread_mutex_destroy(&dev->job_done_mutex);
    pthread_cond_destroy(&dev->job_cv);
    DWLfree(dev->mc_listener_thread);
    dev->mc_listener_thread = NULL;
  }
}

static int DWLOpenNode(char *dev_name, enum NODE_TYPE node_type) {
  int fd = -1;
  int mode = O_RDWR;

  if (node_type == MEM_NODE)
    mode |= O_SYNC;

  fd = open(dev_name, mode);
  if (fd == -1)
    DTRACE_E("failed to open '%s'\n", dev_name);

  return fd;
}

static enum DWLRet DWLInitNodeBaseInfo(struct DevBase *dev_base,
                                        enum NODE_TYPE node_type,
                                        u32 node_id, const char *node_name)  {
  if (DWLInitDevName(&dev_base->name, node_name))
    return DWL_ERROR;

  /* open the device */
  dev_base->fd = DWLOpenNode(dev_base->name, node_type);

  if (dev_base->fd == -1)
    return DWL_ERROR;

  dev_base->ref_cnt = 1;
  dev_base->node_type = node_type;
  dev_base->node_id = node_id;

  pthread_mutex_init(&dev_base->_mutex, NULL);

  return DWL_OK;
}

static enum DWLRet DWLSetDecNode(void *node) {
  struct subsys_desc subsys = {0};
  u32 *reg_mem_vir = NULL;
  struct core_param params = {0};
  u32 i, hw_build_id, core_id;
  DWLDecNode *dev = (DWLDecNode *)node;

  if (ioctl(dev->base.fd, HANTRODEC_IOC_MC_CORES, &dev->num_cores) == -1) {
    DTRACE_E("%s","ioctl HANTRODEC_IOC_MC_CORES failed\n");
    return DWL_ERROR;
  }
  ASSERT(dev->num_cores <= MAX_ASIC_CORES);

  if (ioctl(dev->base.fd, HANTRODEC_IOX_SUBSYS, &subsys) == -1) {
    DTRACE_E("%s","ioctl HANTRODEC_IOX_SUBSYS failed\n");
    return DWL_ERROR;
  }
  if (subsys.subsys_vcmd_num)
    DEV_SET_VCMD_USE(dev);

  if (DWLAllocDecNodeResource(dev))
    goto err;

#ifdef SUPPORT_VCMD_M2M
  if(dev->vcmd_params.vcmd_hw_version_id >= 0x43421400)
    DEV_SET_M2M(dev);
#endif

#if defined(DWL_USE_DEC_IRQ) && !defined(FPGA_REAL_INT)
  if (DEV_USE_VCMD(dev)) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (pthread_create(&dev->vcmd_polling_thread, &attr, FpgaInterruptSimWithPoll,
                       dev) != 0) {
      pthread_attr_destroy(&attr);
      goto err;
    }
    pthread_attr_destroy(&attr);
  }
#endif

#ifdef DEFAULT_ENABLE_MC_LISTENER // always create mc listener thread
  if (DEV_USE_VCMD(dev)) {
    if (DWLCreateMcListenerThread(dev))
      goto err;
  }
#endif

  /* init hw_build_id, asic_id, hw_features */
  for (i=0; i<dev->num_cores; i++) {
    if (DEV_USE_VCMD(dev)) {
      reg_mem_vir = dev->vcmd_mem_params.virt_vcmd_regbuf_addr +
          dev->vcmd_params.submodule_main_addr / 4 +
          (i * SLOT_SIZE_REGBUF) / 4 + 0;
      dev->asic_id[i] = *reg_mem_vir;
      dev->hw_build_id[i] = *(reg_mem_vir + 309);
    }
    else {
      core_id = i;

      /*input cord_id and output asic_id*/
      params.slice = 0;
      params.id = core_id;
      params.type = HW_VCD;
      if (ioctl(dev->base.fd, HANTRODEC_IOX_ASIC_ID, &params) < 0) {
        DTRACE_E("%s", "ioctl HANTRODEC_IOX_ASIC_ID failed\n");
        goto err;
      }
      dev->asic_id[i] = params.asic_id;

      hw_build_id = core_id;
      if (ioctl(dev->base.fd, HANTRODEC_IOX_ASIC_BUILD_ID, &hw_build_id) < 0) {
        DTRACE_E("%s", "ioctl HANTRODEC_IOX_ASIC_BUILD_ID failed\n");
        goto err;
      }
      dev->hw_build_id[i] = hw_build_id;
    }
    GetReleaseHwFeaturesByID(dev->hw_build_id[i], &dev->hw_features[i]);
  }

  return DWL_OK;

err:
  DWLFreeDecNodeResource(dev);
  return DWL_ERROR;
}

static void DWLDestroyNode(struct DevBase *base) {
  if (base->fd)
    close(base->fd);

  if(base->name)
    DWLfree(base->name);

  pthread_mutex_destroy(&base->_mutex);
  DWLfree(base);
}

static void *DWLCreateNode(enum NODE_TYPE node_type, u32 node_id, u32 node_sz,
                           const char *node_name) {
  enum DWLRet ret;
  struct DevBase *base;
  void *node;

  node = DWLcalloc(1, node_sz);
  if (node == NULL) {
    DTRACE_E("%s failed to calloc for device %s\n", __func__, node_name);
    return NULL;
  }

  base = (struct DevBase *)node;

  ret = DWLInitNodeBaseInfo(base, node_type, node_id, node_name);

  if (ret == DWL_OK) {
    if (node_type == DEC_NODE)
      ret = DWLSetDecNode(node);
  }

  if (ret) {
    DWLDestroyNode(base);
    node = NULL;
  }

  return node;
}

/**
 * init a dev node and return the exact node id based on node_name
*/
static void *DWLInitDevNode(enum NODE_TYPE node_type, const char *node_name) {
  i32 i, max_num;
  void **nodes = NULL;
  void *node = NULL;
  struct DevBase *base = NULL;
  u32 free_id = -1;
  u32 node_sz;

  switch (node_type) {
    case DEC_NODE: {
      max_num = MAX_DEC_DEV_NODES;
      nodes = nodes_vpu;
      node_sz = sizeof(DWLDecNode);
      break;
    }
    case MEM_NODE: {
      max_num = MAX_MEM_DEV_NODES;
      nodes = nodes_mem;
      node_sz = sizeof(DWLMemNode);
      break;
    }
#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
    case DMA_NODE: {
      max_num = MAX_DMA_DEV_NODES;
      nodes = nodes_dma;
      node_sz = sizeof(DWLDmaNode);
      break;
    }
#endif
    default: {
      DTRACE_E("%s failed, no such node type %d\n", __func__, node_type);
      return NULL;
    }
  }

  pthread_mutex_lock(&_nodes_init_mutex);
  for (i=0; i<max_num; i++) {
    if (nodes[i]) {
      //valid node
      base = (struct DevBase *)(nodes[i]);
      if (strcmp(base->name, node_name) == 0) {
        //device already opened
        base->ref_cnt++;
        pthread_mutex_unlock(&_nodes_init_mutex);
        return nodes[i];
      }
    } else {
      //empty node
      if (free_id == -1)
        free_id = i;
    }
  }

  if (free_id != -1) {
    node = DWLCreateNode(node_type, free_id, node_sz, node_name);
    if (node)
      nodes[free_id] = node;
    else
      DTRACE_E("%s create dev node failed.\n", __func__);
  }
  else {
    DTRACE_E("%s failed, no empty slot for device %s, you can increase max nodes %d\n",
      __func__, node_name, max_num);
  }
  pthread_mutex_unlock(&_nodes_init_mutex);

  return node;
}

static void DWLReleaseDevNode(struct DevBase *base) {
  u32 node_id = base->node_id;
  void **nodes = NULL;

  pthread_mutex_lock(&_nodes_init_mutex);

  ASSERT(base->ref_cnt >= 1);
  base->ref_cnt--;

  if (base->ref_cnt == 0) {
    switch (base->node_type) {
      case DEC_NODE: {
        DWLDecNode *dev = (DWLDecNode *)base;

        nodes = nodes_vpu;
        dev->b_stopped = HANTRO_TRUE;
#if defined(DWL_USE_DEC_IRQ) && !defined(FPGA_REAL_INT)
        pthread_join(dev->vcmd_polling_thread, NULL);
#endif
        if (dev->mc_listener_thread)
          pthread_join(*dev->mc_listener_thread, NULL);
        DWLFreeDecNodeResource(dev);
        break;
      }
      case MEM_NODE: {
        nodes = nodes_mem;
        break;
      }
#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
      case DMA_NODE: {
        nodes = nodes_dma;
        break;
      }
#endif
      default: {
        DTRACE_E("%s failed, no such node type %d\n", __func__, base->node_type);
        break;
      }
    }
    if (nodes)
      nodes[node_id] = NULL;
    DWLDestroyNode(base);
  }
  pthread_mutex_unlock(&_nodes_init_mutex);
}
