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
#ifndef _DWL_LINUX_H_
#define _DWL_LINUX_H_

#include "basetype.h"
#include "dwl.h"
#include "dwl_activity_trace.h"
#include "ppu.h"
#include "sw_performance.h"
#include "dec_log.h"
#include "sw_util.h"
#include "deccfg.h"

#include "hantrodec_defs.h"
#include "hantrodec.h"
#include "hantrovcmd.h"

#ifdef __FREERTOS__
#include "memalloc_freertos.h"
#elif defined(__linux__)
#include "memalloc.h"
#else //For other os
//TODO...
#endif

#ifdef __FREERTOS__
//nothing
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if !ANDROID
#include <sys/timeb.h>
#endif
#include <sys/types.h>
#endif

#define HANTRODECPP_REG_START 0x400
#define HANTRODEC_REG_START 0x4

#define HANTRODECPP_FUSE_CFG_G1 99
#define HANTRODECPP_FUSE_CFG_G2 99
#define HANTRODECPP_FUSE_CFG 61
#define HANTRODEC_FUSE_CFG 57

#define OFFSET_NOT_EXIST  0xFFFF  /* offset indicating submodule does not exist */

#define DWL_DECODER_INT \
  ((DWLReadReg(dec_dwl, HANTRODEC_REG_START) >> 11) & 0xFFU)
#define DWL_PP_INT      ((DWLReadReg(dec_dwl, HANTRODECPP_REG_START) >> 11) & 0xFFU)

#define IS_DECMODE_JPEG(swreg3) (((swreg3)>>27) == 3)
#define IS_DECMODE_VP78(swreg3) ((((swreg3)>>27) == 9) ||(((swreg3)>>27) == 10))
#define IS_PJPEG(swreg3) ((((swreg3)>>27) == 3) && (((swreg3)>>24) & 0x1))
#define IS_PJPEG_LAST_SCAN(swreg3) (IS_PJPEG(swreg3) && (((swreg3)>>23) & 0x1))
#define IS_PJPEG_INTER_SCAN(swreg3) (IS_PJPEG(swreg3) && !(((swreg3)>>23) & 0x1))
#define IS_PP0_RGB(swreg322) ((((swreg322)>>18) & 0x1F) == 11)


#define DEC_IRQ_ABORT (1 << 11)
#define DEC_IRQ_RDY (1 << 12)
#define DEC_IRQ_BUS (1 << 13)
#define DEC_IRQ_BUFFER (1 << 14)
#define DEC_IRQ_ASO (1 << 15)
#define DEC_IRQ_ERROR (1 << 16)
#define DEC_IRQ_SLICE (1 << 17)
#define DEC_IRQ_TIMEOUT (1 << 18)
#define DEC_IRQ_LAST_SLICE_INT (1 << 19)
#define DEC_IRQ_NO_SLICE_INT (1 << 20)
#define DEC_IRQ_EXT_TIMEOUT (1 << 21)
#define DEC_IRQ_SCAN_RDY (1 << 25)

#define DEC_HW_IRQ_BUFFER 0x08
#define DEC_HW_IRQ_EXT_TIMEOUT 0x400

#define PP_IRQ_RDY             (1 << 12)
#define PP_IRQ_BUS             (1 << 13)
#define DEC_ABORT              0x20
#define DEC_ENABLE             0x01

#ifndef SDRAM_LM_BASE
#define SDRAM_LM_BASE 0x00000000
#endif

#ifdef RANDOM_EXIT_TEST
#define random_exit() do { \
    int r =  random() % 40; \
    if(r == 0) { \
        fprintf(stdout, "random_exit at %s, %s: %d\n", __FUNCTION__, __FILE__, __LINE__); \
        exit(1); \
    } \
} while(0)
#else
#define random_exit() do {} while(0)
#endif

#ifdef _DWL_HW_PERFORMANCE
/* signal that decoder/pp is enabled */
void DwlDecoderEnable(void);
#endif

#define REG_UNIQUE_ID_IDX 327

/* macro to convert CPU bus address to ASIC bus address */
#ifdef PC_PCI_FPGA_DEMO
//#define BUS_CPU_TO_ASIC(address)    (((address) & (~0xff000000)) | SDRAM_LM_BASE)
#define BUS_CPU_TO_ASIC(address, offset) ((address) - (offset))
#define BUS_ASIC_TO_CPU(address, offset) ((address) + (offset))
#else
#define BUS_CPU_TO_ASIC(address, offset) ((address) | SDRAM_LM_BASE)
#define BUS_ASIC_TO_CPU(address, offset) ((address) & ~SDRAM_LM_BASE)
#endif

struct VcmdBuf {
  u8 *cmd_buf;  /* va of cmd buffer */
  u32 cmd_buf_size;   /* cmd buffer bytes allocated */
  u32 cmd_buf_used;   /* bytes used in current buffer */
  u8 *status_buf;     /* va of vcmd status buffer */
  addr_t status_bus_addr;  /* ba of to vcmd status buffer */
  mmu_iova mmu_status_bus_addr;  /* iova of vcmd status buffer */
  u32 status;         /* DEC HW status (swreg1) after decoding */
  u16 core_id;  /* core allocated for this vcmd. */
  u32 *reg_mirror; /* regs of decoder config */
  u32 *mc_fresh_reg_mirror; /* regs of decoder status info, used to mc after irq */
  u32 mc_buf_id;

  PpUnitIntConfig *ppu_cfg;
  u32 secure_mode;

#ifdef SUPPORT_VCMD_M2M
  struct VcmdDataMvInfo vcmd_data_mv;/* save data mv info */
#endif
};

#define HW_ENABLE(A)           (((A) & 1) << 0)
#define HW_G_ENABLE(A)         (((A) >> 0) & 1)
#define SECURE_ENABLE(A)       (((A) & 1) << 1)
#define SECURE_G_ENABLE(A)     (((A) >> 1) & 1)

//VCMD MISC BIT definition
#define VM_BIT_USE_VCMD             0
#define VM_BIT_HAS_M2M              1
#define VM_BIT_HAS_M2MP             2
#define DEV_USE_VCMD(dev)           ((((dev)->vcmd_misc_ctrl) >> VM_BIT_USE_VCMD) & 1)
#define DEV_HAS_M2M(dev)            ((((dev)->vcmd_misc_ctrl) >> VM_BIT_HAS_M2M) & 1)
#define DEV_HAS_M2MP(dev)            ((((dev)->vcmd_misc_ctrl) >> VM_BIT_HAS_M2MP) & 1)

#define DEV_SET_VCMD_USE(dev)       (((dev)->vcmd_misc_ctrl) |= (1 << VM_BIT_USE_VCMD))
#define DEV_SET_M2M(dev)            (((dev)->vcmd_misc_ctrl) |= (1 << VM_BIT_HAS_M2M))
#define DEV_SET_M2MP(dev)            (((dev)->vcmd_misc_ctrl) |= (1 << VM_BIT_HAS_M2MP))

enum NODE_TYPE {
  DEC_NODE,
  MEM_NODE,
  DMA_NODE
};

struct DevBase {
  int fd;                         /* The device file */
  char *name;                     /* The device name of driver. */
  u32 ref_cnt;                    /* The dwl number of use same fd */
  enum NODE_TYPE node_type;       /* The node type of current device */
  u32 node_id;                    /* The dwl device id */
  pthread_mutex_t _mutex;         /* The mutex to protect the dev node */
};

typedef struct {
  /* common */
  struct DevBase base;
  u32 num_cores;   /* total core num under vcmd and non-vcmd */
  u32 asic_id[MAX_ASIC_CORES];
  u32 hw_build_id[MAX_ASIC_CORES];
  const struct DecHwFeatures *hw_features[MAX_ASIC_CORES];
  bool b_stopped; /* if exit vcmd polling and mc listener */

  /* for non vcmd */
  /* bit0-hw_enable, bit1-secure_mode */
  u32 dec_misc_ctrl[MAX_ASIC_CORES];
  u32 *dec_shadow_regs; /* shadow HW registers */
  u32 *dec400_shadow_regs;
  u32 *axife_shadow_regs;
  PpUnitIntConfig *ppu_cfg[MAX_ASIC_CORES];

  /* for vcmd */
  /* bit0-vcmd_used, bit1-vcmd_m2m_support bit2-vcmd_m2mp */
  u32 vcmd_misc_ctrl;
  struct config_parameter vcmd_params;
  struct cmdbuf_mem_parameter vcmd_mem_params;
  struct VcmdBuf *vcmdb;
#if defined(DWL_USE_DEC_IRQ) && !defined(FPGA_REAL_INT)
  pthread_t vcmd_polling_thread;
#endif

  /* for mc decoding synchronization */
  pthread_t *mc_listener_thread;
  sem_t sc_dec_rdy_sem[MAX_MC_CB_ENTRIES];
  DWLIRQCallbackFn *cb_func[MAX_MC_CB_ENTRIES];
  void *cb_arg[MAX_MC_CB_ENTRIES];
  /* increase this counter by 1 when a job need to decode*/
  u32 job_cnt;
  pthread_mutex_t job_mutex;
  pthread_cond_t job_cv;
  /* increase this counter by 1 after any decoded job completed */
  u32 job_done_cnt;
  pthread_mutex_t job_done_mutex;

  /* for only internal test with single stream instance */
#ifdef PERFORMANCE_TEST
  struct ActivityTrace activity;
#endif
  /* for only hw core 0 */ //TODO check which core?
#ifdef FPGA_PERF_AND_BW
  /* every instance can stat own bw */
  u64 last_axife_rd;
  u64 last_axife_wr;
  u32 bw_axife_rd_wr[MAX_ASIC_CORES][2]; /* Save the infomation of bw from regs */
  u32 bytes_consumed_perf[MAX_ASIC_CORES];
  addr_t start_address_perf[MAX_ASIC_CORES];
#endif
} DWLDecNode;

typedef struct {
  struct DevBase base;
#ifdef DMA_TRANS_TEST
  u32 translation_offset;
#endif
} DWLMemNode;

#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
typedef struct {
  struct DevBase base;
} DWLDmaNode;
#endif

/* wrapper information, every stream has own dwl instance */
struct HANTRODWL {
  enum DWLClientType client_type;
  void *dev;      /* decoder (main) device */
  DWLMemNode *mem_dev;
#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
  DWLDecNode *dma_dev;
#endif

  /* counters for core usage statistics of current dwl instance */
  u32 core_usage_counts[MAX_ASIC_CORES];
  pthread_mutex_t owner_mutex;

#ifdef _DWL_PERFORMANCE
  u64 hw_reference_total_max;
  u64 hw_linear_total_max;
#endif
};

u32 *DWLMapRegisters(int mem_dev, /*unsigned long*/ addr_t base, unsigned int reg_size, u32 write);
void DWLUnmapRegisters(const void *io, unsigned int reg_size);
void PrintIrqType(u32 core_id, u32 status);

void DWLWriteCoreRegs(const void *instance, u32 subsys_id,
                         u32 *regs, u32 reg_id, u32 count, enum CoreType type);
void DWLReadCoreRegs(const void *instance, u32 subsys_id,
                         u32 *regs, u32 reg_id, u32 count, enum CoreType type);

#endif /* _DWL_LINUX_H_ */
