/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2015 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------

--
--  Abstract : VCX CODEC Wrapper Layer for OS services
--
------------------------------------------------------------------------------*/
#ifndef __EWL_LOCAL_H__
#define __EWL_LOCAL_H__

#include "vsi_queue.h"
#include "osal.h"
#include "vcx_driver.h"
#include "ewl_common.h"

/* the encoder device driver nod */
#ifndef MEMALLOC_MODULE_PATH
#define MEMALLOC_MODULE_PATH "/tmp/dev/memalloc"
#endif

#ifndef ENC_MODULE_PATH
#define ENC_MODULE_PATH "/tmp/dev/vsi_vcx"
#endif

#ifndef SDRAM_LM_BASE
#define SDRAM_LM_BASE 0x00000000
#endif

enum {
  VCMD_MODE_DISABLED = 0, /* vcmd not present or disabled */
  VCMD_MODE_ENABLED = 1,  /* vcmd present and enabled */
};

typedef struct {
  i32 core_id;  //physical core id
  u32 regSize;  /* IO mem size */
  u32 regBase;
  volatile u32 *pRegBase; /* IO mem base */
} regMapping;

typedef struct {
  u32 subsys_id;
  u32 *pRegBase;
  u32 regSize;
  regMapping core[CORE_MAX];
} subsysReg;

#ifdef MULTICORE_SUPPORT
#define FIRST_CORE(inst) (((EWLWorker *)(inst->workers.tail))->core_id)
#define LAST_CORE(inst) (((EWLWorker *)(inst->workers.head))->core_id)
#define FIRST_CMDBUF_ID(inst) (((EWLWorker *)(inst->workers.tail))->cmdbuf_id)
#define LAST_CMDBUF_ID(inst) (((EWLWorker *)(inst->workers.head))->cmdbuf_id)
#else
#define FIRST_CORE(inst) 0
#define LAST_CORE(inst) 0
#define FIRST_CMDBUF_ID(inst) 0
#define LAST_CMDBUF_ID(inst) 0
#endif

typedef struct {
  u32 clientType;
  int fd_mem;      /* /dev/mem */
  int fd_enc;      /* /dev/vcx_vcmd_driver */
  int fd_memalloc; /* /dev/memalloc */
  regMapping reg;  //register for reserved cores
  subsysReg *reg_all_cores;
  u32 coreAmout;
  u32 performance;
  struct queue freelist;
  struct queue workers;

  /* loopback line buffer in on-chip SRAM*/
  u32 lineBufSramBase;        /* bus addr */
  volatile u32 *pLineBufSram; /* virtual addr */
  u32 lineBufSramSize;
  u32 vcmdEnable;
  u32 mmuEnable;
  u32 dec400Enable;
  u32 ufbcMode;
  u32 ufbcIrqOffset;
  u32 unCheckPid;
  /*vcmd*/
  struct config_parameter vcmd_enc_core_info;
  struct cmdbuf_mem_parameter vcmd_cmdbuf_info;

  struct exchange_parameter reserve_cmdbuf_info;
  u16 wait_core_id_polling;
  u16 wait_polling_break;
  u32 *main_module_init_reg_addr;

  u32 vcmd_mode; /* vcmd's working mode: disable or enable */

  /** The device name of enc driver. */
  char *enc_dev_n;
  /** The device name of memalloc driver. */
  char *mem_dev_n;
   /** the power management support: 0 - not support, 1 - support*/
  u32 pm_support;
} vcx_cwl_t;

#endif /*__EWL_LOCAL_H__*/
