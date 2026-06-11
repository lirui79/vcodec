/*------------------------------------------------------------------------------

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

#include <assert.h>
#include <string.h>
#include <signal.h>

#include "dwl_swhw_sync.h"
#include "sw_performance.h"
#include "dec_log.h"
#include "regdrv.h"
#include "vpufeature.h"
#include "tb_cfg.h"
#include "dwl_memsync.h"
#ifdef VIRTUAL_PLATFORM_TEST
#include "buffer_info.h"
#endif

#ifdef INTERNAL_TEST
#include "internal_test.h"
#endif

#ifdef ASIC_TRACE_SUPPORT
#include "trace.h"
#endif
#include "asic.h"
#include "dwl_vcmd_common.h"
#include "vwl_pc.h"

#ifdef FPGA_PERF_AND_BW
#include "dwl_perf_info.h"
#endif

/* Constants related to registers */
#define DEC_X170_REGS MAX_REG_COUNT
#define PP_FIRST_REG 60
#define DEC_STREAM_START_REG 12

#ifdef VIRTUAL_PLATFORM_TEST
#define DEC_MODE_G1H264 0
#define DEC_MODE_MPEG2 5
#define DEC_MODE_MPEG4 1
#define DEC_MODE_VP8 10
#define DEC_MODE_AVS 11
#define DEC_MODE_H263 2
#endif

#define VCMD_BUF_SIZE (8*1024)  /* 8KB for each command buffer */

#define IS_PIPELINE_ENABLED(val) ((val) & 0x02)

#define IS_DECMODE_VP78(swreg3) ((((swreg3)>>27) == 9) ||(((swreg3)>>27) == 10))

#ifdef DWL_PRESET_FAILING_ALLOC
#define FAIL_DURING_ALLOC DWL_PRESET_FAILING_ALLOC
#endif

#ifdef ASIC_TRACE_SUPPORT
extern struct TBCfg tb_cfg;
#endif

#ifdef ASIC_ONL_SIM
extern u8 l2_allocate_buf;
extern u64 HW_TB_PP_LUMA_BASE;
extern int MultiStreamId;

#ifdef SUPPORT_MULTI_CORE
u32* core_reg_base_array[2];
int cur_core_id = 0;
int start_mc_load = 0;
int mc_load_end = 0;
int cur_pic = 0;
int finished_pic = 0;
extern u32 max_pics_decode;
#endif

#endif

#ifdef SUPPORT_VCMD_M2M
struct VcmdDataMvInfo {
  addr_t before_dec[MAX_VCMD_M2M_NUM][VCMD_M2M_INFO];
  addr_t after_dec[MAX_VCMD_M2M_NUM][VCMD_M2M_INFO];
};
#endif

char* dec_module_path;
char* memalloc_module_path;

/* bytes used before freeing and reallocating ref frame buffer */
u32 last_ref_frm_size = 0;
#ifdef ASIC_TRACE_SUPPORT
u32 free_ref_buffer = 0;
u32 last_ref_frm_num = 0;
u32 same_ref_size_seq_num = 0;
#endif
static i32 DWLTestRandomFail(void);


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

void PrintIrqType(u32 core_id, u32 status) {
  if (status & DEC_IRQ_ABORT)
    IRQTRACE_I("DEC[%d] IRQ ABORT\n", core_id);
  else if (status & DEC_IRQ_RDY)
    IRQTRACE_I("DEC[%d] IRQ READY\n", core_id);
  else if (status & DEC_IRQ_BUS)
    IRQTRACE_I("DEC[%d] IRQ BUS ERROR\n", core_id);
  else if (status & DEC_IRQ_BUFFER)
    IRQTRACE_I("DEC[%d] IRQ BUFFER\n", core_id);
  else if (status & DEC_IRQ_ASO)
    IRQTRACE_I("DEC[%d] IRQ ASO\n", core_id);
  else if (status & DEC_IRQ_ERROR)
    IRQTRACE_I("DEC[%d] IRQ STREAM ERROR\n", core_id);
  else if (status & DEC_IRQ_SLICE)
    IRQTRACE_I("DEC[%d] IRQ SLICE\n", core_id);
  else if (status & DEC_IRQ_TIMEOUT)
    IRQTRACE_I("DEC[%d] IRQ TIMEOUT\n", core_id);
  else if (status & DEC_IRQ_LAST_SLICE_INT)
    IRQTRACE_I("DEC[%d] IRQ LAST_SLICE_INT\n", core_id);
  else if (status & DEC_IRQ_NO_SLICE_INT)
    IRQTRACE_I("DEC[%d] IRQ NO_SLICE_INT\n", core_id);
  else if (status & DEC_IRQ_EXT_TIMEOUT)
    IRQTRACE_I("DEC[%d] IRQ EXT_TIMEOUT\n", core_id);
  else if (status & DEC_IRQ_SCAN_RDY)
    IRQTRACE_I("DEC[%d] IRQ SCAN_RDY\n", core_id);
  else
    IRQTRACE_I("DEC[%d] IRQ UNKNOWN 0x%08x\n", core_id, status);
}



#ifdef VIRTUAL_PLATFORM_TEST
void DWLVirtualPlatformTestOut(i32 core_id, Core instance);
void DWLVirtualPlatformTestIn(i32 core_id, Core instance, HwEnableStatus hw_status);
static u32 VdkBufLen[MAX_ASIC_CORES][DEC_X170_REGS] = {0};
static u32 *VirtualPlatformBuf[MAX_MC_CB_ENTRIES][DEC_X170_REGS] = {NULL};
static u32 *CmodelUseOrigBuf[MAX_MC_CB_ENTRIES][DEC_X170_REGS] = {NULL};
#endif

#ifdef _DWL_PERFORMANCE
u64 reference_total_max = 0;
u64 linear_total_max = 0;
u32 malloc_total_max = 0;
#endif

#ifdef FAIL_DURING_ALLOC
u32 failed_alloc_count = 0;
#endif

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
extern void DWLDecF1Configure(const void *instance, i32 core_id);
#ifdef ASIC_ONL_SIM
extern void dpi_DWLDecF1Configure(u32 *reg_base, const void *instance,i32 core_id);
#endif
#endif
#endif

/* a mutex protecting the wrapper init */
pthread_mutex_t dwl_init_mutex = PTHREAD_MUTEX_INITIALIZER;
static int n_dwl_instance_count = 0;

/* single core PP mutex */
static pthread_mutex_t pp_mutex = PTHREAD_MUTEX_INITIALIZER;

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicID
    Description     : Read the HW ID. Does not need a DWL instance to run

    Return type     : u32 - the HW ID
------------------------------------------------------------------------------*/
u32 DWLReadAsicID(const void *instance, enum DWLClientType client_type) {
  u32 build = 0;

  /* Set HW info from TB config */
  g_hw_ver = g_hw_ver ? g_hw_ver : 19001;
  build = g_hw_id;

  build = (build / 1000) * 0x1000 + ((build / 100) % 10) * 0x100 +
          ((build / 10) % 10) * 0x10 + ((build) % 10) * 0x1;
  (void)client_type;

  switch (g_hw_ver) {
  case 10000:
    return 0x67310000 + build;  /* G1 */
  case 10001:
    return 0x67320000 + build;  /* G2 */
  default:
    return 0x90010000 + build;  /* VC9000D */
  }
}

/* HW Build ID is for feature list query */
/* For G1/G2, HwBuildId is reg0; otherwise it's reg309. */
static u32 DWLReadHwBuildID(const void *instance, enum DWLClientType client_type) {
  g_hw_ver = g_hw_ver ? g_hw_ver : 19001;

  if (g_hw_ver == 10000 || g_hw_ver == 10001)
    return DWLReadAsicID(instance, client_type);
  else
    return g_hw_build_id;
}

static u32 DWLGetCoreIdByClientType(const void *instance, enum DWLClientType client_type,
                                    const struct DecHwFeatures *hw_features, u32 *core_mask) {
  u32 num_cores = DWLReadAsicCoreCount(instance);
  u32 i = 0, matched_1st_core_id = num_cores;

  *core_mask = 0;
#define IS_SUPPORT(a, b)  (client_type == (a) && hw_features->b)

  for (i=0; i<num_cores; i++) {
    if (IS_SUPPORT(DWL_CLIENT_TYPE_H264_DEC, h264_support) ||
        IS_SUPPORT(DWL_CLIENT_TYPE_H264_DEC, h264_adv_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_MPEG4_DEC, mpeg4_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_JPEG_DEC, jpeg_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_PP, pp_standalone))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VC1_DEC, vc1_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_MPEG2_DEC, mpeg2_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VP6_DEC, vp6_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_AVS_DEC, avs_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_RV_DEC, rv_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VP8_DEC, vp8_support) ||
             IS_SUPPORT(DWL_CLIENT_TYPE_VP8_DEC, vp7_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VP9_DEC, vp9_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_HEVC_DEC, hevc_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_ST_PP, pp_standalone))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_H264_MAIN10, h264_high10_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_AVS2_DEC, avs2_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_AV1_DEC, av1_support))
      *core_mask |= 1 << i;
    else if (IS_SUPPORT(DWL_CLIENT_TYPE_VVC_DEC, vvc_support))
      *core_mask |= 1 << i;

    if (matched_1st_core_id == num_cores && *core_mask != 0)
      matched_1st_core_id = i;
  }

  return matched_1st_core_id;
}

u32 DWLReadCoreHwBuildID(const void *instance, u32 core_id) {
  return g_hw_build_id;
}

const void *DWLGetHwFeaturesByID(const void *instance, u32 core_id) {
  struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  const struct DecHwFeatures *hw_feature = NULL;

  /* Check invalid parameters */
  ASSERT(dec_dwl != NULL);
  u32 hw_build_id = DWLReadCoreHwBuildID(instance, core_id);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);

  return hw_feature;
}

const void *DWLGetHwFeaturesByClientType(const void *instance, enum DWLClientType client_type, u32 *core_mask) {
  u32 hw_build_id, cores, core_id;

  const struct DecHwFeatures *hw_feature = NULL;

  *core_mask = 0;
  hw_build_id = DWLReadHwBuildID(instance, client_type);
  GetReleaseHwFeaturesByID(hw_build_id, &hw_feature);
  cores = DWLReadAsicCoreCount(instance);
  ASSERT(cores);
  core_id = DWLGetCoreIdByClientType(instance, client_type, hw_feature, core_mask);
  if (core_id == cores) {
    ASSERT(*core_mask == 0);
    DTRACE_E("ERROR! this device not support this client_type %d\n", client_type);
    return 0;
  }

  return hw_feature;
}

/*------------------------------------------------------------------------------
    Function name   : DWLInit
    Description     : Initialize a DWL instance

    Return type     : const void * - pointer to a DWL instance

    Argument        : struct DWLInitParam * param - initialization params
------------------------------------------------------------------------------*/
const void *DWLInit(struct DWLInitParam *param) {
  static struct HANTRODWL *dwl_inst = NULL;
  unsigned int i;
  int ret;

  pthread_mutex_lock(&dwl_init_mutex);

  if (n_dwl_instance_count) {
    n_dwl_instance_count++;
    pthread_mutex_unlock(&dwl_init_mutex);
    return dwl_inst;
  }

  dwl_inst = (struct HANTRODWL *)DWLcalloc(1, sizeof(struct HANTRODWL));
  assert(dwl_inst);

  if (dwl_inst == NULL){
      DTRACE_D("%s","DWL initial failed\n");
      goto err;
  }

  memset(dwl_inst, 0, sizeof(struct HANTRODWL));

  switch (param->client_type) {
  case DWL_CLIENT_TYPE_H264_DEC:
    DTRACE_I("%s","DWL initialized by an H264 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_MPEG4_DEC:
    DTRACE_I("%s","DWL initialized by an MPEG4 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_JPEG_DEC:
    DTRACE_I("%s","DWL initialized by a JPEG decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_PP:
    DTRACE_I("%s","DWL initialized by a PP instance...\n");
    break;
  case DWL_CLIENT_TYPE_VC1_DEC:
    DTRACE_I("%s","DWL initialized by an VC1 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_MPEG2_DEC:
    DTRACE_I("%s","DWL initialized by an MPEG2 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_AVS_DEC:
    DTRACE_I("%s","DWL initialized by an AVS decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_RV_DEC:
    DTRACE_I("%s","DWL initialized by an RV decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VP6_DEC:
    DTRACE_I("%s","DWL initialized by a VP6 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VP8_DEC:
    DTRACE_I("%s","DWL initialized by a VP8 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_HEVC_DEC:
    DTRACE_I("%s","DWL initialized by an HEVC decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VP9_DEC:
    DTRACE_I("%s","DWL initialized by a VP9 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_AVS2_DEC:
    DTRACE_I("%s","DWL initialized by a AVS2 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_AV1_DEC:
    DTRACE_I("%s","DWL initialized by a AV1 decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_VVC_DEC:
    DTRACE_I("%s","DWL initialized by a VVC decoder instance...\n");
    break;
  case DWL_CLIENT_TYPE_ST_PP:
    DTRACE_I("%s","DWL initialized by a standalone PP instance...\n");
    break;
  default:
    DTRACE_E("%s","ERROR: DWL client type has to be always specified!\n");
    return NULL;
  }

#ifdef INTERNAL_TEST
  InternalTestInit();
#endif

  dwl_inst->client_type = param->client_type;
  dwl_inst->frm_base = NULL;
  dwl_inst->free_ref_frm_mem = NULL;

  dwl_inst->vcmd_enabled = DWLVcmdIsUsed(dwl_inst);
  pthread_mutex_init(&dwl_inst->mem_mutex, NULL);

  if (dwl_inst->vcmd_enabled) {
    /*******************************************/
    /* VCMD related initialization. */

    ret = CmodelVcmdInit();
    if (ret) {
      DTRACE_E("%s","CmodelVcmdInit() failed\n");
      goto err;
    }

    /* Get VCMD configuration. */
    dwl_inst->vcmd_params.module_type = 2; /* VCD */
    if (CmodelIoctlGetVcmdParameter(&dwl_inst->vcmd_params) == -1) {
      DTRACE_E("%s","ioctl HANTRO_VCMD_IOCH_GET_VCMD_PARAMETER failed\n");
      goto err;
    }

    if (CmodelIoctlGetCmdbufParameter(&dwl_inst->vcmd_mem_params) == -1) {
      DTRACE_E("%s","ioctl HANTRO_VCMD_IOCH_GET_CMDBUF_PARAMETER failed\n");
      goto err;
    }
#ifdef SUPPORT_VCMD_M2M
    if(dwl_inst->vcmd_params.vcmd_hw_version_id >= 0x43421400)
      dwl_inst->vcmd_m2m = 1;
    else
      dwl_inst->vcmd_m2m = 0;
#endif
    /* vcmd init */
    DWLmemset(dwl_inst->vcmd, 0, sizeof(dwl_inst->vcmd));
    /* VCMD initialization done. */
    /*******************************************/
  }

  /* Allocate cores just once */
  if (!n_dwl_instance_count) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    dwl_inst->hw_core_array = InitializeCoreArray();
    if (dwl_inst->hw_core_array == NULL) {
      free(dwl_inst);
      dwl_inst = NULL;
	  pthread_attr_destroy(&attr);
	  goto err;
    }

    dwl_inst->sync_params.n_dec_cores = GetCoreCount();

    for (i = 0; i < dwl_inst->sync_params.n_dec_cores; i++) {
      Core c = GetCoreById(dwl_inst->hw_core_array, i);

      dwl_inst->sync_params.reg_base[i] = HwCoreGetBaseAddress(c);
    }

    for (i = 0; i < MAX_MC_CB_ENTRIES; i++)
      sem_init(dwl_inst->sync_params.sc_dec_rdy_sem + i, 0, 0);
    for (i = 0; i < MAX_ASIC_CORES; i++)
      dwl_inst->sync_params.callback[i] = NULL;
    dwl_inst->sync_params.b_stopped = 0;
    pthread_create(&dwl_inst->mc_listener_thread, &attr, ThreadMCListener,
                    dwl_inst);
    pthread_attr_destroy(&attr);
  }

  n_dwl_instance_count++;
#ifdef PERFORMANCE_TEST
  ActivityTraceInit(&dwl_inst->activity);
#endif

  pthread_mutex_unlock(&dwl_init_mutex);

  return (void *)dwl_inst;

err:

  pthread_mutex_unlock(&dwl_init_mutex);
  if (dwl_inst->hw_core_array != NULL) ReleaseCoreArray(dwl_inst->hw_core_array);
  free(dwl_inst);
  dwl_inst = NULL;

  return (void *)dwl_inst;
}

/*------------------------------------------------------------------------------
    Function name   : DWLRelease
    Description     : Release a DWl instance

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - instance to be released
------------------------------------------------------------------------------*/
enum DWLRet DWLRelease(const void *instance) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  unsigned int i;

  assert(dwl_inst != NULL);

  if (dwl_inst == NULL) return DWL_OK;

  pthread_mutex_lock(&dwl_init_mutex);

  n_dwl_instance_count--;

  if (n_dwl_instance_count) {
    pthread_mutex_unlock(&dwl_init_mutex);
    return DWL_OK;
  }

  assert(dwl_inst->reference_total == 0);
  assert(dwl_inst->reference_alloc_count == 0);
  assert(dwl_inst->linear_total == 0);
  assert(dwl_inst->linear_alloc_count == 0);

#ifdef INTERNAL_TEST
  InternalTestFinalize();
#endif

  dwl_inst->sync_params.b_stopped = 1;
  if (dwl_inst->vcmd_enabled) {
    /* Release Vcmd cmodel after setting b_stopped to 1, so that listener thread can return. */
    CmodelVcmdRelease();
  }

  /* Release the signal handling and cores just when
   * nobody is referencing them anymore
   */
  StopCoreArray(dwl_inst->hw_core_array);
  pthread_join(dwl_inst->mc_listener_thread, NULL);
  ReleaseCoreArray(dwl_inst->hw_core_array);

  pthread_mutex_unlock(&dwl_init_mutex);

  /* print core usage stats */
  {
    u32 total_usage = 0;
    u32 cores = DWLReadAsicCoreCount(instance);
    for (i = 0; i < cores; i++) {
      total_usage += dwl_inst->core_usage_counts[i];
    }
    /* avoid zero division */
    total_usage = total_usage ? total_usage : 1;

    printf("\nMulti-core usage statistics:\n");
    for (i = 0; i < cores; i++)
      printf("\tCore[%2u] used %6u times (%2u%%)\n", i, dwl_inst->core_usage_counts[i],
             (dwl_inst->core_usage_counts[i] * 100) / total_usage);

    printf("\n");
  }

#ifdef PERFORMANCE_TEST
  ActivityTraceRelease(&dwl_inst->activity);
#endif

#ifdef _DWL_PERFORMANCE
  printf("Total allocated reference mem = %8llu\n", reference_total_max);
  printf("Total allocated linear mem    = %8llu\n", linear_total_max);
  printf("Total allocated SWSW mem      = %8u\n", malloc_total_max);
#endif

  for (i = 0; i < MAX_MC_CB_ENTRIES; i++) {
    sem_destroy(dwl_inst->sync_params.sc_dec_rdy_sem + i);
  }

  if (dwl_inst->vcmd_enabled) {
    /* Free VCMD buffers. */
    for (i = 0; i < MAX_VCMD_ENTRIES; i++)
      ASSERT(!dwl_inst->vcmd[i].cmd_buf || !dwl_inst->vcmd[i].owner);
    dwl_inst->vcmd_enabled = 0;

  }
  pthread_mutex_destroy(&dwl_inst->mem_mutex);
  free((void *)dwl_inst);
  dwl_inst = NULL;

  return DWL_OK;
}

void DWLSetIRQCallback(const void *instance, i32 core_id,
                       DWLIRQCallbackFn *callback_fn, void *arg) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  dwl_inst->sync_params.callback[core_id] = callback_fn;
  dwl_inst->sync_params.callback_arg[core_id] = arg;
}

/*------------------------------------------------------------------------------
    Function name   : DWLMallocRefFrm
    Description     : Allocate a frame buffer (contiguous linear RAM memory)

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : struct DWLLinearMem *info - place where the allocated
memory
                        buffer parameters are returned
------------------------------------------------------------------------------*/
enum DWLRet DWLMallocRefFrm(const void *instance, u64 size, struct DWLLinearMem *info) {

  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  if (DWLTestRandomFail()) {
    return DWL_ERROR;
  }
#ifdef ASIC_TRACE_SUPPORT
  u64 memory_size;

  MEMTRACE_I("DWLMallocRefFrm: %8d\n", size);

  if (dwl_inst->frm_base == NULL) {
    if (tb_cfg.tb_params.ref_frm_buffer_size == -1) {
      /* Max value based on limits set by Level 5.2 */
      /* Use tb.cfg to overwrite this limit */
      u32 cores = DWLReadAsicCoreCount(instance);
      /* Updated to Level6.2. */
      u64 max_frame_buffers = (139264 * (5 + 1 + cores)) * (384 + 64) * 10 / 8;
      /* TODO MIN function of "size" is not working here if we are
         not allocating all buffers in decoder init... */
      /* FIXME(min): extra 10 buffers for multicore top simulation. */
      size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);
      memory_size = MAX(max_frame_buffers, (u64)size * (16 + 1 + cores + 10));
      memory_size = MAX(memory_size, 0x50000000);
      if ( (u64)size * (16 + 1 + cores + 10) <= (u64)size) {
        /* overflow 4G size */
        memory_size = 0x7FFFF000;
      }

      if (dwl_inst->client_type == DWL_CLIENT_TYPE_AV1_DEC &&
          tb_cfg.tb_params.first_trace_frame)
        memory_size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT) * 20;

      if (sizeof(unsigned long long) == 4)
        memory_size = MIN(memory_size, 0x7FFFF000);
      else
        memory_size = MIN(memory_size, 0x500000000);
      tb_cfg.tb_params.ref_frm_buffer_size = memory_size;
    } else {
      /* Use tb.cfg to set max size for nonconforming streams */
      memory_size = tb_cfg.tb_params.ref_frm_buffer_size;
    }
    memory_size = NEXT_MULTIPLE(memory_size, DEC_X170_BUS_ADDR_ALIGNMENT);
    dwl_inst->frm_base = (u8 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, memory_size);
    if (dwl_inst->frm_base == NULL) return DWL_ERROR;

    dwl_inst->reference_total = 0;
    dwl_inst->reference_maximum = memory_size;
    dwl_inst->free_ref_frm_mem = dwl_inst->frm_base;

    /* for DPB offset tracing */
    dpb_base_address = real_dpb_base_address = (u8 *)dwl_inst->frm_base;
    if (last_ref_frm_num != 0 && last_ref_frm_size / last_ref_frm_num == NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT)) {
      same_ref_size_seq_num = (same_ref_size_seq_num + 1) % MAX_PIC_BUFFERS;
      dpb_base_address -= (last_ref_frm_size + NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT) * same_ref_size_seq_num);
    } else {
      same_ref_size_seq_num = 0;
      dpb_base_address -= last_ref_frm_size;
    }
    free_ref_buffer = 0;
  }

  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);

  /* Check that we have enough memory to spare */
  if (dwl_inst->free_ref_frm_mem + size > dwl_inst->frm_base + dwl_inst->reference_maximum)
    return DWL_ERROR;

  info->virtual_address = (u32 *)dwl_inst->free_ref_frm_mem;
  info->bus_address = (addr_t)info->virtual_address;
  info->size = size;

  dwl_inst->free_ref_frm_mem += size;
#else
  i32 ret;
  MEMTRACE_I("DWLMallocRefFrm: %8d\n", size);
  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);
  info->size = size;
  info->bus_address = (addr_t)(u32 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, size);
  if (info->bus_address == 0) return DWL_ERROR;

  info->virtual_address = NULL;
  ret = DWLMemSyncAllocHostBuffer(instance, size, DEC_X170_BUS_ADDR_ALIGNMENT, info);
  if (ret == DWL_NOT_SUPPORT)
    info->virtual_address = (u32 *)info->bus_address;
  else if (ret == DWL_ERROR)
    return ret;
#ifdef _DWL_MEMSET_BUFFER
  if (info->virtual_address) DWLmemset(info->virtual_address, 0x80, size);
#endif
#endif /* ASIC_TRACE_SUPPORT */

#ifdef _DWL_PERFORMANCE
  reference_total_max += size;
#endif /* _DWL_PERFORMANCE */

  pthread_mutex_lock(&dwl_inst->mem_mutex);
  dwl_inst->reference_total += size;
  dwl_inst->reference_alloc_count++;
  MEMTRACE_I("DWLMallocRefFrm: memory allocated %8d bytes in %2d buffers @ %p (type %d)\n",
         dwl_inst->reference_total, dwl_inst->reference_alloc_count, (void *)info->virtual_address, info->mem_type);
  pthread_mutex_unlock(&dwl_inst->mem_mutex);
  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeRefFrm
    Description     : Release a frame buffer previously allocated with
                        DWLMallocRefFrm.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : struct DWLLinearMem *info - frame buffer memory
information
------------------------------------------------------------------------------*/
void DWLFreeRefFrm(const void *instance, struct DWLLinearMem *info) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  av_unused i32 ret;
  assert(dwl_inst != NULL);

#ifdef ASIC_TRACE_SUPPORT
  if (free_ref_buffer == 0) {
    free_ref_buffer = 1;
    last_ref_frm_num = dwl_inst->reference_alloc_count;
  }
#endif

  MEMTRACE_I("DWLFreeRefFrm: %8d\n", info->size);
  pthread_mutex_lock(&dwl_inst->mem_mutex);
  dwl_inst->reference_total -= info->size;
  dwl_inst->reference_alloc_count--;
  MEMTRACE_I("DWLFreeRefFrm: not freed %8d bytes in %2d buffers @ %p\n",
         dwl_inst->reference_total, dwl_inst->reference_alloc_count, (void *)info->virtual_address);
  pthread_mutex_unlock(&dwl_inst->mem_mutex);
#ifdef ASIC_TRACE_SUPPORT
  /* Release memory when calling DWLFreeRefFrm for the last buffer */
  if (dwl_inst->frm_base && (dwl_inst->reference_alloc_count == 0)) {
    assert(dwl_inst->reference_total == 0);
    last_ref_frm_size = dwl_inst->free_ref_frm_mem - dwl_inst->frm_base;
	osal_aligned_free(dwl_inst->frm_base);
    dwl_inst->frm_base = NULL;
    dpb_base_address = NULL;
  }
#else
  if (info->bus_address != 0)
    osal_aligned_free((u32 *)info->bus_address);
  ret = DWLMemSyncFreeHostBuffer(instance, info);
#endif /* ASIC_TRACE_SUPPORT */
  info->virtual_address = NULL;
  info->bus_address = 0;
  info->size = 0;
}
#ifdef SUPPORT_VCMD_M2M
/*------------------------------------------------------------------------------
    Function name   : DWLCheckVcmdM2M
    Description     : Check if vcmd support m2m

    Return type     : u32

    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
u32 DWLCheckVcmdM2M(const void *instance) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  return dwl_inst->vcmd_m2m;
}
#endif
/*------------------------------------------------------------------------------
    Function name   : DWLMallocLinear
    Description     : Allocate a contiguous, linear RAM  memory buffer

    Return type     : i32 - 0 for success or a negative error code

    Argument        : const void * instance - DWL instance
    Argument        : u32 size - size in bytes of the requested memory
    Argument        : struct DWLLinearMem *info - place where the allocated
                        memory buffer parameters are returned
------------------------------------------------------------------------------*/
enum DWLRet DWLMallocLinear(const void *instance, u64 size, struct DWLLinearMem *info) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 ret;

  if (DWLTestRandomFail()) return DWL_ERROR;

#ifdef ASIC_TRACE_SUPPORT
  alignment = DEC_X170_BUS_ADDR_ALIGNMENT;
#endif

  /* allocate device bus_address */
  info->logical_size = size;
  size = NEXT_MULTIPLE(size, DEC_X170_BUS_ADDR_ALIGNMENT);
  info->size = size;
  info->bus_address = (addr_t)(u32 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, size);
  MEMTRACE_I("DWLMallocLinear: %8d\n", size);
  if (info->bus_address == 0) return DWL_ERROR;

  info->virtual_address = NULL;
  ret = DWLMemSyncAllocHostBuffer(instance, size, DEC_X170_BUS_ADDR_ALIGNMENT, info);
  if (ret == DWL_NOT_SUPPORT)
    info->virtual_address = (u32 *)info->bus_address;
  else if (ret == DWL_ERROR)
    return ret;

  pthread_mutex_lock(&dwl_inst->mem_mutex);
  dwl_inst->linear_alloc_count++;
  dwl_inst->linear_total += size;
  MEMTRACE_I("DWLMallocLinear: allocated total %8d bytes in %2d buffers @ %p (type %d)\n",
         dwl_inst->linear_total, dwl_inst->linear_alloc_count, (void *)info->virtual_address, info->mem_type);
  pthread_mutex_unlock(&dwl_inst->mem_mutex);

#ifdef ENABLE_FPGA_VERIFICATION
  if(info->virtual_address != NULL) {
    DWLmemset(info->virtual_address, 0, info->size);
  } else {
    DWLLinearMemset(dwl_inst, info, 0, 0, info->size);
  }
#endif

#ifdef _DWL_PERFORMANCE
  linear_total_max += size;
#endif /* _DWL_PERFORMANCE */

  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLFreeLinear
    Description     : Release a linera memory buffer, previously allocated with
                        DWLMallocLinear.

    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : struct DWLLinearMem *info - linear buffer memory
information
------------------------------------------------------------------------------*/
void DWLFreeLinear(const void *instance, struct DWLLinearMem *info) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  av_unused i32 ret;
  assert(dwl_inst != NULL);

  MEMTRACE_I("DWLFreeLinear: %8d\n", info->size);
  if (info->bus_address != 0)
    osal_aligned_free((u32 *)info->bus_address);

  ret = DWLMemSyncFreeHostBuffer(instance, info);

  pthread_mutex_lock(&dwl_inst->mem_mutex);
  dwl_inst->linear_alloc_count--;
  dwl_inst->linear_total -= info->size;
  MEMTRACE_I("DWLFreeLinear: not freed %8d bytes in %2d buffers @ %p\n",
         dwl_inst->linear_total, dwl_inst->linear_alloc_count, (void *)info->virtual_address);
  pthread_mutex_unlock(&dwl_inst->mem_mutex);
  info->bus_address = 0;
  info->virtual_address = NULL;
  info->size = 0;
  info->logical_size = 0;

}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteReg
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLWriteReg(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  REGTRACE_I("core[%d] DWLWriteReg swreg[%d] at offset 0x%02X = %08X\n", core_id, offset / 4,
            offset, value);
#ifdef INTERNAL_TEST
  u32 *core_reg_base = dwl_inst->dwl_shadow_regs[core_id];
  InternalTestDumpWriteSwReg(core_id, offset >> 2, value, core_reg_base);
#endif
  assert(offset <= DEC_X170_REGS * 4);
  dwl_inst->dwl_shadow_regs[core_id][offset >> 2] = value;
  UNUSED(dwl_inst);
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteRegs
    Description     : Write a value to a hardware IO register

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32* from - the start address of the copy
    Argument        : u32 num_regs - the num of regs value to be written out
    Argument        : u32 print_flag - whether print regs info
------------------------------------------------------------------------------*/
void DWLWriteRegs(const void *instance, i32 core_id, u32 offset, u32* from, u32 num_regs, u32 print_flag) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  //Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  //u32 *core_reg_base = HwCoreGetBaseAddress(c);
#ifndef DWL_DISABLE_REG_PRINTS
  if (print_flag) {
    u32 i;
    for (i = 0; i < num_regs; i++)
      DTRACE_D("core[%d] swreg[%d] at offset 0x%02X = %08X\n", core_id, (offset + 4 * i) / 4,
              (offset + 4 * i), *(from + i));
  }
#endif

#ifdef VCD_LOGMSG
  if (print_flag) {
    u32 i;
    for (i = 0; i < num_regs; i++)
      REGTRACE_I("core[%d] DWLWriteReg swreg[%d] at offset 0x%02X = %08X\n", core_id, (offset + i * 4) / 4,
              (offset + i * 4), *(from + i));
  }
#endif

#ifdef INTERNAL_TEST
  for (i = 0; i < num_regs; i++)
    InternalTestDumpWriteSwReg(core_id, (offset + 4 * i) >> 2, *(from + i), core_reg_base);
#endif
  assert(offset <= DEC_X170_REGS * 4);

  DWLmemcpy(&dwl_inst->dwl_shadow_regs[core_id][offset >> 2], from, num_regs * 4);
  UNUSED(dwl_inst);
}

/*------------------------------------------------------------------------------
    Function name   : DWLWriteRegToHw
    Description     : Write a value to a hardware IO register
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLWriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value)
{
    DWLWriteReg(instance, core_id, offset, value);
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadRegFromHW
    Description     : Read the value of a hardware IO register
    Return type     : u32 - the value stored in the register
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadRegFromHw(const void *instance, i32 core_id, u32 offset)
{
    return DWLReadReg(instance, core_id, offset);
}


/*------------------------------------------------------------------------------
    Function name   : DWLEnableHw
    Description     :
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLEnableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  u32 *core_reg_base = HwCoreGetBaseAddress(c);
  u32 *core_reg_base_onl = dwl_inst->dwl_shadow_regs[core_id];
  u32 i;
  u32 muti_core_support = 0;
  u32 allow_intrabc = 0, dec_mode;
#ifdef VIRTUAL_PLATFORM_TEST
  HwEnableStatus hw_status;
  DWLmemset(&hw_status, 0, sizeof(HwEnableStatus));
#endif

  StackConsumption(__func__);

  (void)core_reg_base;
  (void)core_reg_base_onl;
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_MULTI_CORE
  cur_core_id = core_id;
  core_reg_base_array[core_id] = core_reg_base_onl;
  if ( cur_pic < max_pics_decode ){
  	start_mc_load = 1;
    while(!mc_load_end){
      usleep(50);
    }
    mc_load_end = 0;
  }
  cur_pic += 1;
#endif

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
  int max_wait_time_dec400 = 10000; /* 10s in ms */
#endif
#endif

#endif

#ifdef FPGA_PERF_AND_BW
  dwl_inst->start_address_perf[core_id] = DWLReadStartAddress(core_id, &dwl_inst->dwl_shadow_regs[0][0]);
#endif

  /* There are some duplicated variables named "cmodel_xxx",
     they are used in cmodel instead of tb_cfg. */
#ifdef ASIC_TRACE_SUPPORT
  cmodel_first_trace_frame = tb_cfg.tb_params.first_trace_frame;
  cmodel_pipeline_e = tb_cfg.pp_params.pipeline_e;
  cmodel_extra_cu_ctrl_eof = tb_cfg.tb_params.extra_cu_ctrl_eof;
  cmodel_ref_frm_buffer_size = tb_cfg.tb_params.ref_frm_buffer_size;
  cmodel_in_width = tb_cfg.pp_params.in_width;
  cmodel_in_height = tb_cfg.pp_params.in_height;
#endif

  dec_mode = (DWLReadReg(dwl_inst, core_id, 4*3) >> 27) & 0x1F;
  if (dec_mode == DEC_MODE_AV1)
    allow_intrabc  = ((DWLReadReg(dwl_inst, core_id, 4*5) >> 4) & 0x1);

#ifdef ASIC_TRACE_SUPPORT
  muti_core_support = dwl_inst->sim_mc;
#else
  muti_core_support = ((DWLReadReg(dwl_inst, core_id, 4*58) >> 30) & 0x1);
#endif

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
  DWLWriteReg(dwl_inst, core_id, 4*317, cache_e);

  /*recon_shaper*/
  if (allow_intrabc) {
    DWLWriteReg(dwl_inst, core_id, 4*3, (core_reg_base_onl[3] & 0xFFFFFFF7));
  } else {
    DWLWriteReg(dwl_inst, core_id, 4*3, (core_reg_base_onl[3] | 0x8));
  }

#ifndef PPU_V9_2_3
#ifdef SUPPORT_DEC400
  DWLDecF1Configure(instance, core_id);
#endif
#endif

#ifndef PPU_V9_2_3
#ifdef ASIC_ONL_SIM
#ifdef SUPPORT_DEC400
  /*
  do {
    const unsigned int usec_dec400 = 1000; //1 ms polling interval
    if (dec_dwl->dec400_enable[core_id] == 0)
      break;
    usleep(usec_dec400);

    max_wait_time_dec400--;
  } while (max_wait_time_dec400 > 0);
  */
  dpi_DWLDecF1Configure(core_reg_base_onl,instance,core_id);
  //dec_dwl->dec400_enable[core_id] = 1;
#endif
#endif
#endif

  DWLWriteReg(dwl_inst, core_id, offset, value);

  PERFORMANCE_STATIC_END(decode_pre_hw);
  /* For AVS, DWLEnableHw may be called again before exiting xxxDecDecode(),
     so need to end post_hw time statistic here. */
  PERFORMANCE_STATIC_END(decode_post_hw);
#ifdef PERFORMANCE_TEST
  ActivityTraceStartDec(&dwl_inst->activity);
#endif

  DTRACE_D("%s %d enabled by previous DWLWriteReg\n", "DEC", core_id);

#ifdef VIRTUAL_PLATFORM_TEST
  hw_status.cmodel_enable_hw_bit = GetDecRegister(core_reg_base, HWIF_DEC_E);
#endif
  /* Flush dwl_shadow_regs to core registers. */
  if (core_reg_base != NULL) {
  for(i = DEC_X170_REGISTERS - 1; i >= 1; --i) {
    core_reg_base[i] = dwl_inst->dwl_shadow_regs[core_id][i];
  }
  }
#ifdef VIRTUAL_PLATFORM_TEST
  hw_status.driver_enable_hw_bit = GetDecRegister(core_reg_base, HWIF_DEC_E);
#endif

#ifdef VIRTUAL_PLATFORM_TEST
  DWLVirtualPlatformTestIn(core_id, c, hw_status);
#endif

  if (dwl_inst->client_type != DWL_CLIENT_TYPE_PP) {
    HwCoreDecEnable(c);
  } else {
    /* standalone PP start */
    HwCorePpEnable(c, dwl_inst->last_dec_core != NULL ? 0 : 1);
  }

  dwl_inst->core_usage_counts[core_id]++;
}

/*------------------------------------------------------------------------------
    Function name   : DWLDisableHw
    Description     :
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be written
    Argument        : u32 value - value to be written out
------------------------------------------------------------------------------*/
void DWLDisableHw(const void *instance, i32 core_id, u32 offset, u32 value) {
  DWLWriteReg(instance, core_id, offset, value);
  DTRACE_I("%s %d disabled by previous DWLWriteReg\n", "DEC", core_id);
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadReg
    Description     : Read the value of a hardware IO register
    Return type     : u32 - the value stored in the register
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
------------------------------------------------------------------------------*/
u32 DWLReadReg(const void *instance, i32 core_id, u32 offset) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 *core_reg_base = dwl_inst->dwl_shadow_regs[core_id];
  u32 val;

#ifdef INTERNAL_TEST
  InternalTestDumpReadSwReg(core_id, offset >> 2, core_reg_base[offset >> 2],
                            core_reg_base);
#endif

  assert(offset <= DEC_X170_REGS * 4);

  val = core_reg_base[offset >> 2];

  REGTRACE_I("core[%d] DWLReadReg swreg[%d] at offset 0x%02X = %08X\n", core_id, offset / 4,
            offset, val);
  UNUSED(dwl_inst);

  return val;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadRegs
    Description     : Read the value of a hardware IO register
    Return type     : u32 - the value stored in the register
    Argument        : const void * instance - DWL instance
    Argument        : u32 offset - byte offset of the register to be read
    Argument        : u32* dest - the destination register
    Argument        : u32 num - the number of register to be read
------------------------------------------------------------------------------*/
void DWLReadRegs(const void *instance, i32 core_id, u32 offset, u32* dest, u32 num) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 *core_reg_base = dwl_inst->dwl_shadow_regs[core_id];
#ifdef INTERNAL_TEST
  u32 i;
  for (i = 0; i < num; i++)
  InternalTestDumpReadSwReg(core_id, (offset + i * 4) >> 2, core_reg_base[(offset + i * 4) >> 2],
                            core_reg_base);
#endif

  assert(offset <= DEC_X170_REGS * 4);

#ifdef VCD_LOGMSG
  u32 i;
  for (i = 0; i < num; i++)
    REGTRACE_I("core[%d] DWLReadReg swreg[%d] at offset 0x%02X = %08X\n", core_id, (offset + i * 4) / 4,
            (offset + i * 4), core_reg_base[(offset + i * 4) >> 2]);
#endif
  UNUSED(dwl_inst);

  DWLmemcpy(dest, &core_reg_base[offset>>2], num*4);
}


/*------------------------------------------------------------------------------
    Function name   : DWLWaitDecHwReady
    Description     : Wait until decoder hardware has stopped running.
                      Used for synchronizing software runs with the hardware.
                      The wait could succed, timeout, or fail with an error.
    Return type     : i32 - one of the values DWL_HW_WAIT_OK
                                              DWL_HW_WAIT_TIMEOUT
                                              DWL_HW_WAIT_ERROR
    Argument        : const void * instance - DWL instance
------------------------------------------------------------------------------*/
static i32 DWLWaitDecHwReady(const void *instance, i32 core_id, u32 timeout) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  (void)timeout;

  if (HwCoreWaitDecRdy(c) != 0) {
    return (i32)DWL_HW_WAIT_ERROR;
  }

  return (i32)DWL_HW_WAIT_OK;
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
  Core c = GetCoreById(dec_dwl->hw_core_array, core_id);
  u32 *core_reg_base = HwCoreGetBaseAddress(c);

  i32 ret;
  assert(dec_dwl);
  switch (dec_dwl->client_type) {
  case DWL_CLIENT_TYPE_HEVC_DEC:
  case DWL_CLIENT_TYPE_VP9_DEC:
  case DWL_CLIENT_TYPE_AVS2_DEC:
  case DWL_CLIENT_TYPE_AV1_DEC:
  case DWL_CLIENT_TYPE_H264_DEC:
  case DWL_CLIENT_TYPE_VVC_DEC:
  case DWL_CLIENT_TYPE_MPEG4_DEC:
  case DWL_CLIENT_TYPE_JPEG_DEC:
  case DWL_CLIENT_TYPE_VC1_DEC:
  case DWL_CLIENT_TYPE_MPEG2_DEC:
  case DWL_CLIENT_TYPE_AVS_DEC:
  case DWL_CLIENT_TYPE_RV_DEC:
  case DWL_CLIENT_TYPE_VP6_DEC:
  case DWL_CLIENT_TYPE_VP8_DEC:
  case DWL_CLIENT_TYPE_ST_PP:
    ret = DWLWaitDecHwReady(dec_dwl, core_id, timeout);
    break;
  default:
    assert(0); /* should not happen */
    ret = DWL_HW_WAIT_ERROR;
    break;
  }
#ifdef PERFORMANCE_TEST
  ActivityTraceStopDec(&dec_dwl->activity);
  dec_dwl->hw_time_use = dec_dwl->activity.active_time / 100;
#endif
  PERFORMANCE_STATIC_START(decode_post_hw);

  /* Refresh dwl_shadow_regs from hw core registers. */
  if (core_reg_base != NULL) {
    DWLWriteRegs(dec_dwl, core_id, 0, core_reg_base, DEC_X170_REGISTERS, 0);
  }


  {
    u32 irq_stats = DWLReadReg(instance, core_id, 4);

    PrintIrqType(core_id, irq_stats);

#ifdef FPGA_PERF_AND_BW
    dec_dwl->bytes_consumed_perf[core_id] += DWLGetConsumedBytes(core_id, &dec_dwl->start_address_perf[0], &dec_dwl->dwl_shadow_regs[0][0]);
#endif
  }

  return ret;
}

/*------------------------------------------------------------------------------
    Function name   : DWLmalloc
    Description     : Allocate a memory block. Same functionality as
                      the ANSI C malloc()
    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available
    Argument        : u32 n - Bytes to allocate
------------------------------------------------------------------------------*/
void *DWLmalloc(size_t n) {
  if (DWLTestRandomFail()) {
    return NULL;
  }
  MEMTRACE_I("DWLmalloc Size# %8d\n", (u32)n);

#ifdef _DWL_PERFORMANCE
  malloc_total_max += n;
#endif /* _DWL_PERFORMANCE */

  return malloc(n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLfree
    Description     : Deallocates or frees a memory block. Same functionality as
                      the ANSI C free()
    Return type     : void
    Argument        : void *p - Previously allocated memory block to be freed
------------------------------------------------------------------------------*/
void DWLfree(void *p) {
  free(p);
}

/*------------------------------------------------------------------------------
    Function name   : DWLcalloc
    Description     : Allocates an array in memory with elements initialized
                      to 0. Same functionality as the ANSI C calloc()
    Return type     : void pointer to the allocated space, or NULL if there
                      is insufficient memory available
    Argument        : u32 n - Number of elements
    Argument        : u32 s - Length in bytes of each element.
------------------------------------------------------------------------------*/
void *DWLcalloc(size_t n, size_t s) {
  if (DWLTestRandomFail()) {
    return NULL;
  }
  MEMTRACE_I("DWLcalloc Size# %8ld\n", (long int)n * s);
#ifdef _DWL_PERFORMANCE
  malloc_total_max += n * s;
#endif /* _DWL_PERFORMANCE */

  return calloc(n, s);
}

/*------------------------------------------------------------------------------
    Function name   : DWLmemcpy
    Description     : Copies characters between buffers. Same functionality as
                      the ANSI C memcpy()
    Return type     : The value of destination d
    Argument        : void *d - Destination buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *DWLmemcpy(void *d, const void *s, size_t n) {
  return memcpy(d, s, n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLmemset
    Description     : Sets buffers to a specified character. Same functionality
                      as the ANSI C memset()
    Return type     : The value of destination d
    Argument        : void *d - Pointer to destination
    Argument        : i32 c - Character to set
    Argument        : u32 n - Number of characters
------------------------------------------------------------------------------*/
void *DWLmemset(void *d, i32 c, size_t n) {
  return memset(d, (int)c, n);
}


/*------------------------------------------------------------------------------
    Function name   : DWLLinearMemcpy
    Description     : Copies characters from a buffer to Physical linear buffer.
                      The specific implementation is completed by the customer.

    Return type     : void

    Argument        : const void * instance - DWL instance
    Argument        : addr_t device_bus_addr - Physical linear buffer
    Argument        : const void *s - Buffer to copy from
    Argument        : u32 n - Number of bytes to copy
------------------------------------------------------------------------------*/
void *DWLLinearMemcpy(const void *instance, addr_t device_bus_addr, const void *s, size_t n) {
  //struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  return memcpy((void *)device_bus_addr, s, n);
}

/**
 * Sets Physical linear buffer to a specified character. Customer can adjust it.
 * \ingroup common_group
 * \param [in]     instance   Pointer to a DWL instance.
 * \param[in]      mem        memory information needs to be synchronized
 * \param[in]      offset     The offset of memsync in bytes; implemented alignment in API
 * \param [in]     c          Character to set
 * \param [in]     n          Number of characters
 */
void *DWLLinearMemset(const void *instance, struct DWLLinearMem *mem, i32 offset, i32 c, size_t n) {
  //struct HANTRODWL *dec_dwl = (struct HANTRODWL *)instance;
  addr_t device_bus_addr = mem->bus_address + offset;

  return memset((void *)device_bus_addr, (int)c, n);
}

/*------------------------------------------------------------------------------
    Function name   : DWLReserveHw
    Description     :
    Return type     : i32
    Argument        : const void *instance
------------------------------------------------------------------------------*/
enum DWLRet DWLReserveHw(const void *instance, struct DWLReqInfo *info, i32 *core_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 core_mask = info->core_mask & 0xffff;

  if (dwl_inst->client_type == DWL_CLIENT_TYPE_PP) {
    pthread_mutex_lock(&pp_mutex);

    if (dwl_inst->last_dec_core == NULL) /* Blocks until core available. */
      dwl_inst->current_core = BorrowHwCore(dwl_inst->hw_core_array, core_mask);
    else
      /* We rely on the fact that in combined mode the PP is always reserved
       * after the decoder
       */
      dwl_inst->current_core = dwl_inst->last_dec_core;
  } else {
    /* Blocks until core available. */
    dwl_inst->current_core = BorrowHwCore(dwl_inst->hw_core_array, core_mask);
    dwl_inst->last_dec_core = dwl_inst->current_core;
  }

  *core_id = HwCoreGetid(dwl_inst->current_core);
  dwl_inst->secure_mode[*core_id] = (info->core_mask >> 31);
  dwl_inst->sync_params.callback[*core_id] = NULL;
  dwl_inst->sync_params.callback_arg[*core_id] = NULL;
  DTRACE_I("Reserved %s core %d\n",
            dwl_inst->client_type == DWL_CLIENT_TYPE_PP ? "PP" : "DEC",
            *core_id);
#ifdef FPGA_PERF_AND_BW
  dwl_inst->bytes_consumed_perf[*core_id] = 0;
#endif
  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReleaseHw
    Description     :
    Return type     : void
    Argument        : const void *instance
------------------------------------------------------------------------------*/
enum DWLRet DWLReleaseHw(const void *instance, i32 core_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  Core c = GetCoreById(dwl_inst->hw_core_array, core_id);
  if (dwl_inst->client_type == DWL_CLIENT_TYPE_PP) {
    DTRACE_I("Released PP core %d\n", core_id);

    pthread_mutex_unlock(&pp_mutex);

    /* core will be released by decoder */
    if (dwl_inst->last_dec_core != NULL) return (-1);
  }

  /* PP reserved by decoder in DWLReserveHwPipe */
  if (dwl_inst->b_reserved_pipe) pthread_mutex_unlock(&pp_mutex);

  dwl_inst->b_reserved_pipe = 0;

  ReturnHwCore(dwl_inst->hw_core_array, c);
  dwl_inst->last_dec_core = NULL;
  DTRACE_I("Released %s core %d\n",
            dwl_inst->client_type == DWL_CLIENT_TYPE_PP ? "PP" : "DEC",
            core_id);
  return 0;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadAsicCoreCount
    Description     : Return number of ASIC cores, static implementation
    Return type     : u32
    Argument        : void
------------------------------------------------------------------------------*/
u32 DWLReadAsicCoreCount(const void *instance) {
  return GetCoreCount();
}

i32 DWLTestRandomFail(void) {
#ifdef FAIL_DURING_ALLOC
  if (!failed_alloc_count) {
    srand(time(NULL));
  }
  failed_alloc_count++;

  /* If fail preset to this alloc occurance, failt it */
  if (failed_alloc_count == FAIL_DURING_ALLOC) {
    DTRACE_E("DWL: Preset allocation fail during alloc %u\n", failed_alloc_count);
    return DWL_ERROR;
  }
  /* If failing point is preset, no randomization */
  if (FAIL_DURING_ALLOC > 0) return DWL_OK;

  if ((rand() % 100) > 90) {
    DTRACE_E("DWL: Testing a failure in memory allocation number %u\n",
           failed_alloc_count);
    return DWL_ERROR;
  } else {
    return DWL_OK;
  }
#endif
  return DWL_OK;
}

/*------------------------------------------------------------------------------
    Function name   : DWLReadPpConfigure
    Description     : Read the pp configure
    Return type     : void
    Argument        : const void * instance - DWL instance
    Argument        : PpUnitIntConfig *ppu_cfg
------------------------------------------------------------------------------*/
void DWLReadPpConfigure(const void *instance, u32 core_id, void *ppu_cfg, u32 pjpeg_coeff_buffer_size)
{
#if 1
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  if (!dwl_inst->vcmd_enabled) {
    dwl_inst->ppu_cfg[core_id] = ppu_cfg;
  } else {
    /* core_id: for vcmd, it's cmd buf id; otherwise, it's real core id. */
    struct VcmdBuf *vcmd = &dwl_inst->vcmd[core_id];

    vcmd->ppu_cfg = ppu_cfg;
  }
#endif
}

/* Reserve one valid command buffer. */
enum DWLRet DWLReserveCmdBuf(const void *instance,  struct DWLReqInfo *info, u32 *cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  enum DWLRet ret = DWL_ERROR;
  u32 width;
  u32 height;
  u32 client_type;
  struct exchange_parameter params = {0};

  width = info->width;
  height = info->height;
  client_type = (info->core_mask >> 16) & 0x7FFF;

  params.executing_time = width * height;
  params.module_type = VCMD_TYPE_DECODER;
  params.cmdbuf_size = VCMD_BUF_SIZE;

  params.core_mask = info->core_mask & 0xffff;

  DTRACE_I("%s", "enter\n");

  ret = CmodelIoctlReserveCmdbuf(&params);
  if (ret < 0) {
    DTRACE_E("%s", "DWLReserveCmdBuf failed\n");
    ret = DWL_ERROR;
  } else {
    int cmdbuf_id = params.cmdbuf_id;
    DTRACE_I("reserve cmd buf id %d\n", cmdbuf_id);
    dwl_inst->vcmd[cmdbuf_id].owner = info->owner;
    dwl_inst->vcmd[cmdbuf_id].client_type = client_type;
    dwl_inst->vcmd[cmdbuf_id].cmd_buf_size = params.cmdbuf_size;
    dwl_inst->vcmd[cmdbuf_id].cmd_buf_used = 0;
    dwl_inst->vcmd[cmdbuf_id].cmd_buf = (u8 *)dwl_inst->vcmd_mem_params.virt_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmd[cmdbuf_id].status_buf = (u8 *)dwl_inst->vcmd_mem_params.virt_status_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmd[cmdbuf_id].status_bus_addr = dwl_inst->vcmd_mem_params.phy_status_cmdbuf_addr +
                            dwl_inst->vcmd_mem_params.status_cmdbuf_unit_size * cmdbuf_id;
    dwl_inst->vcmd[cmdbuf_id].secure_mode = (info->core_mask >> 31);
    *cmd_buf_id = cmdbuf_id;
    dwl_inst->sync_params.callback[cmdbuf_id] = NULL;
    dwl_inst->sync_params.callback_arg[cmdbuf_id] = NULL;

    ret = DWL_OK;
  }

  return ret;
}

#if 0
/* Append an instruction to command buffer. Instruction payload is in @data. */
i32 DWLAppendInstToCmdBuf(const void *instance, u32 cmd_buf_id,
                           u32 opcode, u32 opdata1, u32 opdata2, u32 opdata3,
                           void *data, u32 size) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 core_id;
  pthread_mutex_lock(&vcmd_mutex);

  pthread_mutex_unlock(&vcmd_mutex);
  return DWL_OK;
}
#endif

enum DWLRet DWLFlushRegister(const void *instance, u32 cmd_buf_id, u32 *dec_regs, u32 *mc_fresh_regs, u32 mc_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  struct VcmdBuf *vcmd;

  if (!dwl_inst || cmd_buf_id>=MAX_VCMD_ENTRIES || !dec_regs) {
    return DWL_ERROR;
  }

  vcmd = &dwl_inst->vcmd[cmd_buf_id];
  if (!vcmd->owner) {
    // not occupied yet
    return DWL_ERROR;
  }

  vcmd->reg_mirror = dec_regs;
  vcmd->mc_fresh_reg_mirror = mc_fresh_regs;
  vcmd->mc_buf_id = mc_buf_id;

  return DWL_OK;
}

enum DWLRet DWLRefreshRegister(const void *instance, u32 cmd_buf_id, u32 *dec_regs)
{
  //nothing needs to do, already refresh in DWLWaitCmdBufReady().
  return DWL_OK;
}

enum DWLRet DWLVcmdMCRefreshStatusRegs(const void *instance, u32 *dec_regs, u32 cmdbuf_id)
{
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmdbuf_id];

  dec_regs[0] = vcmd->mc_fresh_reg_mirror[0];	  //0-0
  dec_regs[1] = vcmd->mc_fresh_reg_mirror[1];	  //1-1
  dec_regs[261] = vcmd->mc_fresh_reg_mirror[2]; //2-261
  dec_regs[270] = vcmd->mc_fresh_reg_mirror[3]; //3-270
  dec_regs[168] = vcmd->mc_fresh_reg_mirror[4]; //4-168
  dec_regs[169] = vcmd->mc_fresh_reg_mirror[5];	//5-169
  dec_regs[62] = vcmd->mc_fresh_reg_mirror[6];	//6-62
  dec_regs[63] = vcmd->mc_fresh_reg_mirror[7];	//7-63

  return DWL_OK;
}

i32 DWLGetVcmdMCVirtualCoreId(const void *instance, u32 cmdbuf_id)
{
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmdbuf_id];

  return vcmd->mc_buf_id;
}

/* Reserve one valid command buffer. */
enum DWLRet DWLEnableCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmd_buf_id];
  struct exchange_parameter params = {0, 0, 0, 0, 0, 0, 0, 0};
  enum DWLRet ret;
  u32 allow_intrabc = 0, dec_mode;
  u32 muti_core_support = 0;

  DTRACE_I("enable cmd buf id %d\n", cmd_buf_id);


  dec_mode = (vcmd->reg_mirror[3] >> 27) & 0x1F;
  if (dec_mode == DEC_MODE_AV1)
    allow_intrabc  = ((vcmd->reg_mirror[5] >> 4) & 0x1);

#ifdef ASIC_TRACE_SUPPORT
  muti_core_support = dwl_inst->sim_mc;
#else
  muti_core_support = ((vcmd->reg_mirror[58] >> 30) & 0x1);
#endif

  vcmd->reg_mirror[3] |= 0x40;
  if (dec_mode == DEC_MODE_JPEG)
    vcmd->reg_mirror[317] = 0x0;
  else if (allow_intrabc) {
    vcmd->reg_mirror[317] = 0x588;
  } else {
    if (muti_core_support)
      vcmd->reg_mirror[317] = 0x58a;
    else if (dec_mode == DEC_MODE_VVC)
      /* DMV is uncachable for vvc */
      vcmd->reg_mirror[317] = 0x1d8a;
    else
      vcmd->reg_mirror[317] = 0x1d9a;
  }

  /****************************************************************************/
  /* Start to generate VCMD instructions. */
  if (dwl_inst->vcmd_params.vcmd_hw_version_id > VCMD_HW_ID_1_0_C) {
    /* Read VCMD buffer ID (last VCMD registers). */
    CWLCollectReadRegData(vcmd,
                          26, 1, /* VCMD command buffer ID register */
                          0);

  }
#ifdef SUPPORT_VCMD_M2M
  /* M2M mv data  */
  if(dwl_inst->vcmd_m2m) {
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

  /* Configure VCD instruction */
  CWLCollectWriteRegData(vcmd,
                        &vcmd->reg_mirror[2], //&dwl_shadow_regs[0][2],
                        dwl_inst->vcmd_params.submodule_main_addr/4 + 2 , /* register offset in bytes to vcmd base address */
                        MAX_REG_COUNT - 2);
  /* Write swreg1 to enable dec. */
  CWLCollectWriteRegData(vcmd,
                         &vcmd->reg_mirror[0], //&dwl_shadow_regs[0][0],
                         dwl_inst->vcmd_params.submodule_main_addr/4 + 0, /* register offset in bytes to vcmd base address */
                         2);

  /* Wait for interruption */
  CWLCollectStallData(vcmd,
                      VCD_FRAME_RDY_INT_MASK);
#ifdef SUPPORT_VCMD_M2M
  /* M2M mv data  */
  if(dwl_inst->vcmd_m2m) {
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
  /* Read swreg0 for debug */
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 0, 1, /* swreg0 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr);
  /* Read status (swreg1) register */
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 1, 1, /* swreg1 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4);
  /* Read swreg261 register HWIF_ERROR_INFO*/
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 261, 1, /* swreg261 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 2);
  /* Read swreg270 register HWIF_TOTAL_ERROR_CTBS*/
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 270, 1, /* swreg270 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 3);
  /* swreg168/169 */
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 168, 2, /* swreg168/169 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 4);
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);
  /* swreg62 - mb pos for mc decoding */
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 62, 1, /* swreg62 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 6);
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);
  /* swreg63 - sw_perf_cycle_count */
  CWLCollectReadRegData(vcmd,
                      dwl_inst->vcmd_params.submodule_main_addr/4 + 63, 1, /* swreg63 */
                      vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 7);
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);
  if (IS_DECMODE_VP78(vcmd->reg_mirror[3])) {
    /* swreg7/8 */
    CWLCollectReadRegData(vcmd,
                        dwl_inst->vcmd_params.submodule_main_addr/4 + 7, 2, /* swreg7/8 */
                        vcmd->status_bus_addr + dwl_inst->vcmd_params.submodule_main_addr + 4 * 8);
  }
  ASSERT((dwl_inst->vcmd[cmd_buf_id].cmd_buf_used & 3) == 0);

  if (dwl_inst->vcmd_params.vcmd_hw_version_id > VCMD_HW_ID_1_0_C) {
    /* Dump all vcmd registers. */
    CWLCollectReadRegData(vcmd,
                          0, 27, /* VCMD registers count */
                          0);
  }

  /* Jmp */
  CWLCollectJmpData(vcmd);

  params.cmdbuf_size = dwl_inst->vcmd[cmd_buf_id].cmd_buf_used;
  params.cmdbuf_id = cmd_buf_id;
  params.client_type = dwl_inst->client_type;
  params.module_type = 2;   /* VCD */

#ifdef ASIC_TRACE_SUPPORT
  {
    cmodel_first_trace_frame = tb_cfg.tb_params.first_trace_frame;
    cmodel_pipeline_e = tb_cfg.pp_params.pipeline_e;
    cmodel_extra_cu_ctrl_eof = tb_cfg.tb_params.extra_cu_ctrl_eof;
    cmodel_ref_frm_buffer_size = tb_cfg.tb_params.ref_frm_buffer_size;
    cmodel_in_width = tb_cfg.pp_params.in_width;
    cmodel_in_height = tb_cfg.pp_params.in_height;
  }
#endif

  ret = CmodelIoctlEnableCmdbuf(&params);
  if (ret < 0) {
    DTRACE_E("%s", "CmodelIoctlEnableCmdbuf failed\n");
    return DWL_ERROR;
  }

  vcmd->core_id = params.core_id;

  dwl_inst->core_usage_counts[vcmd->core_id]++;

  return DWL_OK;
}
#ifdef SUPPORT_VCMD_M2M
/* use cmd SetDefaultCDFs */
void DWLCmdM2MSendData(const void *instance, struct CDF_INFO *head_data,
  struct CDF_INFO *tail_data, u32 cmdbuf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmdbuf_id];

  vcmd->vcmd_data_mv.before_dec = head_data;
  vcmd->vcmd_data_mv.after_dec = tail_data;
}
#endif

/* Wait cmd buffer ready. Used only in single core decoding. */
enum DWLRet DWLWaitCmdBufReady(const void *instance, u16 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  u32 * status = NULL;
  struct VcmdBuf *vcmd = &dwl_inst->vcmd[cmd_buf_id];
  i32 ret = 0;

  /* Check invalid parameters */
  if(dwl_inst == NULL)
    return DWL_ERROR;

  DTRACE_I("%s", "DWLWaitCmdBufReady\n");
#if 0
  ret = CmodelIoctlWaitCmdbuf(&cmd_buf_id);
#else
  sem_wait(dwl_inst->sync_params.sc_dec_rdy_sem+cmd_buf_id);
#endif
  if (ret < 0) {
    DTRACE_E("%s", "DWLWaitCmdBufReady failed\n");
    return DWL_HW_WAIT_ERROR;
  } else {
    DTRACE_I("%s", "DWLWaitCmdBufReady succeed\n");
    status = (u32 *)(vcmd->status_buf + dwl_inst->vcmd_params.submodule_main_addr);
    status++; // skip swreg0

    vcmd->reg_mirror[1] = *status++;
    vcmd->reg_mirror[261] = *status++;
    vcmd->reg_mirror[270] = *status++;
    vcmd->reg_mirror[168] = *status++;
    vcmd->reg_mirror[169] = *status++;
    vcmd->reg_mirror[62] = *status++;
    vcmd->reg_mirror[63] = *status++;
    if (IS_DECMODE_VP78(vcmd->reg_mirror[3])) {
      vcmd->reg_mirror[7] = *status++;
      vcmd->reg_mirror[8] = *status++;
    }
  }

  return DWL_OK;
}

void DWLCmdPushSliceRegs(const void *instance, u32 cmd_buf_id, u32 *regs) {
}

void DWLAbortCmdbuf(const void *instance, u16 cmd_buf_id) {
}

void DWLWaitOwnerDone(const void *instance, const void *owner) {
}


void DWLDropCmdbufs(const void *instance, const void *owner){
}

/* Reserve one valid command buffer. */
enum DWLRet DWLReleaseCmdBuf(const void *instance, u32 cmd_buf_id) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  i32 ret;

  ASSERT (cmd_buf_id < MAX_VCMD_ENTRIES);
  dwl_inst->vcmd[cmd_buf_id].owner = NULL;

  DTRACE_I("release cmd buf id %d\n", cmd_buf_id);

  ret = CmodelIoctlReleaseCmdbuf(cmd_buf_id);
  if (ret) {
    DTRACE_E("%s", "DWLReleaseCmdBuf failed\n");
    return DWL_ERROR;
  }

  return DWL_OK;
}

enum DWLRet DWLWaitCmdbufsDone(const void *instance, const void *owner) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  u32 i;
  /* cmdbuf 0 and 1 not used for user */
  for (i = 2; i < MAX_VCMD_ENTRIES; i++) {
    while(dwl_inst->vcmd[i].owner == owner)
      sched_yield();
  }
  return 0;
}
extern bool use_vcmd;
u32 DWLVcmdIsUsed(const void *instance) {
  if (use_vcmd == 1)
    return 1;
  else
    return 0;
}

#ifdef FPGA_PERF_AND_BW
u32 DWLReadBw(const void *instance, u32 core_id, u32 num){
  return 0;
}
void DWLPerfInfoCollect(const void *instance, i32 core_id, struct DWLPerfInfo *perf_info) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;

  perf_info->bitrate = dwl_inst->bytes_consumed_perf[core_id];
  perf_info->cycles = dwl_inst->dwl_shadow_regs[core_id][63];
  perf_info->write_bw = 0;
  perf_info->read_bw = 0;
}
#endif

#ifdef ASIC_TRACE_SUPPORT
void DWLSetSimMc(const void *instance) {
  struct HANTRODWL *dwl_inst = (struct HANTRODWL *)instance;
  dwl_inst->sim_mc = 1;
  return;
}
#endif

#ifdef VIRTUAL_PLATFORM_TEST
int decrypt_addr = 1;
static int HasReplacedVdkBuf(u32 *p);
static int HasRevertCtrlswBuf(u32 *p);
static void DataCopy(u8 *dst, u8 *src, SysBufferInfo *buf_info);
static void wait_ref_ready(i32 core_id, int reg_id);
static u32* GetRegsBufAddr(u32 *regs, u32 reg_lsb_idx, u32 reg_msb_idx, int flag);
// static void ClearRegsBufAddr(u32 *regs, u32 reg_lsb_idx, u32 reg_msb_idx);
static void ReplaceRegsBufAddr(u32 *regs, addr_t new_addr, u32 reg_lsb_idx, u32 reg_msb_idx, int flag);
static int FindSameCtrlSwBufIndex(u32 **buf, u32 *len, u32 *p, u32 curr_idx);
static int CheckIfReleaseVdkBuf(u32 **buf, u32 *len, u32 *p, u32 curr_idx);

/* code_id is cmdbuf_id when vcmd mode */
void DWLVirtualPlatformTestIn(i32 core_id, Core instance, HwEnableStatus hw_status) /* before hw enable */
{
  int ret = 0, i = 0;
  u32 buf_len = 0;
  u32 *vdk_simu_buf_addr = NULL, *ctrlsw_buf_addr = NULL, *regs = NULL;
  SysBufferInfo buf_info = {0};

//For defense, one day maybe help for debug
  CoreDecCheckRegID();
#ifndef SUPPORT_VCMD
  regs = HwCoreGetBaseAddress(instance);
#else
  regs = HwVcmdCoreGetVcdBaseAddress((void *)instance);
#endif

#ifdef XS_DEBUG
  static int decoded_id=0;
  decoded_id++;
#endif
  for(i = 0; i < MAX_REG_COUNT; i++) {
    u32 buf_addr_offset = 0;

    if (HasReplacedVdkBuf(VirtualPlatformBuf[core_id][i])) {
      continue;
    }
    ret = CoreDecGetMemoryDescription((void *)instance, i, 0, &buf_info, hw_status);
    if (ret == 0) {
      ctrlsw_buf_addr = GetRegsBufAddr(regs, i, buf_info.reg_msb_idx, decrypt_addr);
      buf_len = buf_info.stride * buf_info.height;
      VdkBufLen[core_id][i] = buf_len;
#ifdef XS_DEBUG
      printf("\t Testin decoded pic %d, reg %d: width:%u, stride:%u, height:%u, size:%u\n", decoded_id, \
                                        i, buf_info.width, buf_info.stride, buf_info.height, buf_len);
#endif
      int reg_index = FindSameCtrlSwBufIndex(CmodelUseOrigBuf[core_id], VdkBufLen[core_id], ctrlsw_buf_addr, i); //find the cmodel buf index with same base and buf_len
      if (reg_index != -1) {
        //use the allocated vdk buf, not realloc under the same cmodel buf
        vdk_simu_buf_addr = VirtualPlatformBuf[core_id][reg_index];
      } else {
        //need allocate vdk buffer
        if (buf_info.flag.bits.is_pp_out &&
            (buf_info.flag.bits.opcode & OP_REVERSE_OFFSET)) {
          buf_len += buf_info.reverse_offset; // only for pp bottom field offset
        }
        vdk_simu_buf_addr = (u32 *)osal_aligned_malloc(DEC_X170_BUS_ADDR_ALIGNMENT, buf_len);
        DWLmemset(vdk_simu_buf_addr, 0 , buf_len);
      }
      ASSERT(vdk_simu_buf_addr);
      CmodelUseOrigBuf[core_id][i] = ctrlsw_buf_addr;
      VirtualPlatformBuf[core_id][i] = vdk_simu_buf_addr;
      if (buf_info.flag.bits.opcode & OP_REVERSE_OFFSET) {
        buf_addr_offset += buf_info.reverse_offset; //only for sync word size and pp bottom field offset
      }
      if (buf_info.flag.bits.opcode & OP_WAIT_REF_RDY_MC) {
        wait_ref_ready(core_id, i); // for mc ref frame
        //TODO:need update the buffer data of reference frame reg, or not decoded correctly
      }
      if (buf_info.flag.bits.opcode & OP_KEEP_LAST_2BITS) {
        buf_addr_offset |= ((addr_t)ctrlsw_buf_addr & 0x3); //for saving the last two bits info
      }

      if (buf_info.flag.bits.opcode & OP_ADJ_OUT_DBASE_G1H264) {
        //this flag set when meet OBASE register, also need replace DBASE
        u32 *obase = GetRegsBufAddr(regs, hw_dec_reg_spec[HWIF_DEC_OUT_BASE_LSB][0], buf_info.reg_msb_idx,decrypt_addr);
        u32 *dbase = GetRegsBufAddr(regs, hw_dec_reg_spec[HWIF_DIR_MV_BASE_LSB][0], hw_dec_reg_spec[HWIF_DIR_MV_BASE_MSB][0],decrypt_addr);
        u32 dmv_offset = 0;
        if (dbase) {
          dmv_offset = (addr_t)dbase - ((addr_t)obase & ~0x3);
        }
        u32 *dmv_addr = (u32 *)((u8 *)vdk_simu_buf_addr + dmv_offset);
        ReplaceRegsBufAddr(regs, ((addr_t)dmv_addr), hw_dec_reg_spec[HWIF_DIR_MV_BASE_LSB][0],
                hw_dec_reg_spec[HWIF_DIR_MV_BASE_MSB][0], 0);
        assert(buf_info.flag.bits.io_mode & OUTPUT);
      }

      ReplaceRegsBufAddr(regs, ((addr_t)vdk_simu_buf_addr + buf_addr_offset), i, buf_info.reg_msb_idx, 0);
      if(buf_info.flag.bits.io_mode & REG_INPUT) {
        DataCopy((u8 *)vdk_simu_buf_addr, (u8 *)ctrlsw_buf_addr - buf_addr_offset, &buf_info);
      }
    }
  }
}

void DWLVirtualPlatformTestOut(i32 core_id, Core instance) /* after get hw interrupt */
{
  int ret = 0, i = 0;
  u32 *ctrlsw_buf_addr = NULL, *vdk_simu_buf_addr = NULL, *regs = NULL;
  SysBufferInfo buf_info = {0};

#ifndef SUPPORT_VCMD
  regs = HwCoreGetBaseAddress(instance);
#else
  regs = HwVcmdCoreGetVcdBaseAddress((void *)instance);
#endif

#ifdef XS_DEBUG
  static int decoded_id=0;
  decoded_id++;
#endif
  HwEnableStatus hw_status;
  DWLmemset(&hw_status, 0, sizeof(HwEnableStatus));
  for(i = 0; i < MAX_REG_COUNT; i++) {
  //clear the values in the loop
    u32 buf_addr_offset = 0;
    u32 strm_bytes_processed = 0;
    u32 is_encrypt = decrypt_addr;

    if (HasRevertCtrlswBuf(VirtualPlatformBuf[core_id][i]))
      continue;
    ret = CoreDecGetMemoryDescription((void *)instance, i, 1, &buf_info, hw_status);
    if(ret == 0) {
      vdk_simu_buf_addr = GetRegsBufAddr(regs, i, buf_info.reg_msb_idx, 0);
      ctrlsw_buf_addr = CmodelUseOrigBuf[core_id][i];
#ifdef XS_DEBUG
      printf("\t Testout decoded pic %d, reg %d: width:%u, stride:%u, height:%u, saved size:%u, actual size:%u\n", decoded_id, \
                i, buf_info.width, buf_info.stride, buf_info.height, VdkBufLen[core_id][i], buf_info.stride * buf_info.height);
#endif
      if (buf_info.flag.bits.opcode & OP_KEEP_LAST_2BITS) {
        buf_addr_offset |= ((addr_t)ctrlsw_buf_addr & 0x3);
      }
      if (buf_info.flag.bits.opcode & OP_REVERSE_OFFSET) {
        buf_addr_offset += buf_info.reverse_offset;
      }

      if (buf_info.flag.bits.opcode & OP_UPDATE_REG_DATA) {
        strm_bytes_processed = (addr_t)vdk_simu_buf_addr - (addr_t)VirtualPlatformBuf[core_id][i];
        is_encrypt = 0; // ctrl-sw need update the address, not use encrypt
      }
      ReplaceRegsBufAddr(regs, (addr_t)ctrlsw_buf_addr + strm_bytes_processed, i, buf_info.reg_msb_idx,is_encrypt);
      if(buf_info.flag.bits.io_mode & OUTPUT) {
        DataCopy((u8 *)ctrlsw_buf_addr - buf_addr_offset,
           (u8 *)vdk_simu_buf_addr - buf_addr_offset, &buf_info);
      }
    }

    if (GetDecRegister(regs, HWIF_DEC_IRQ_STAT) != DEC_HW_IRQ_BUFFER || i == hw_dec_reg_spec[HWIF_STREAM_BASE_LSB][0]) { /* when error case IRQ is buf_empty, other regs buffer not free */
      ASSERT(VirtualPlatformBuf[core_id][i]);
      if (VirtualPlatformBuf[core_id][i]) {
        //release vdk buffer when it would not be used
        if (CheckIfReleaseVdkBuf(VirtualPlatformBuf[core_id], VdkBufLen[core_id], VirtualPlatformBuf[core_id][i], i)) {
          osal_aligned_free(VirtualPlatformBuf[core_id][i]);
        }
        VirtualPlatformBuf[core_id][i] = NULL;
        CmodelUseOrigBuf[core_id][i] = NULL;
      }
    }
  }
}

#ifdef VDK_STRICT_TEST
#define ENCRYP_CODE (0xbeef)
#else
#define ENCRYP_CODE (0)
#endif

static u32* GetRegsBufAddr(u32 *regs, u32 reg_lsb_idx, u32 reg_msb_idx, int flag)
{
  u32 *buf_addr = NULL;
  addr_t addr_lsb = 0, addr_msb = 0;
  addr_lsb = regs[reg_lsb_idx];
  addr_msb = regs[reg_msb_idx];

  if (flag == 1 && addr_msb) {
    addr_msb = addr_msb^ENCRYP_CODE;
  }

  if (sizeof(addr_t) == 8)
    buf_addr = (u32*)((addr_msb << 32) | addr_lsb);
  else
    buf_addr = (u32*)addr_lsb;
  return buf_addr;
}

// static void ClearRegsBufAddr(u32 *regs, u32 reg_lsb_idx, u32 reg_msb_idx)
// {
//     regs[reg_lsb_idx] = 0;
//     regs[reg_msb_idx] = 0;
// }

static void ReplaceRegsBufAddr(u32 *regs, addr_t new_addr, u32 reg_lsb_idx, u32 reg_msb_idx, int flag)
{
    regs[reg_lsb_idx] = new_addr & 0xffffffff;
    if(sizeof(addr_t) == 8) {
      regs[reg_msb_idx] = (new_addr >> 32) & 0xffffffff;
      if (flag == 1 && regs[reg_msb_idx] !=0) {
        regs[reg_msb_idx] = regs[reg_msb_idx] ^ ENCRYP_CODE;
      }
    } else {
      regs[reg_msb_idx] = 0;
    }
}

static int FindSameCtrlSwBufIndex(u32 **buf, u32 *len, u32 *p, u32 curr_idx) {
  u32 i = 0;
  for (i = 0; i < curr_idx; i++){
    if (p == buf[i] && len[curr_idx] == len[i]) {
      return i;
    }
  }
  return -1; //invaild index
}

static int CheckIfReleaseVdkBuf(u32 **buf, u32 *len, u32 *p, u32 curr_idx) {
  u32 i = 0;
  for (i = curr_idx + 1; i < MAX_REG_COUNT; i++) {
    if (p == buf[i] && len[curr_idx] == len[i]) {
      return 0;
    }
  }
  return 1;
}

static void DataCopy(u8 *dst, u8 *src, SysBufferInfo *buf_info)
{
  u32 i = 0;
  for(i = 0; i < buf_info->height; i++) {
    DWLmemcpy(dst, src, buf_info->width);
    dst = (dst + buf_info->stride);
    src = (src + buf_info->stride);
  }
}


static void wait_ref_ready(i32 core_id, int reg_id)
{
  u32 sync_word_size = 32;
  u32 *curr_dbase_addr, *ref_dbase = NULL;
  curr_dbase_addr = CmodelUseOrigBuf[core_id][hw_dec_reg_spec[HWIF_DEC_OUT_DBASE_LSB][0]];
  assert(curr_dbase_addr != NULL);

  ref_dbase = CmodelUseOrigBuf[core_id][reg_id];
  if (ref_dbase &&
    curr_dbase_addr != ref_dbase){
    u8 *sync_word =(u8 *)ref_dbase - sync_word_size;
    //wait reference frame ready
    while (sync_word[1] == 0 && sync_word[0] == 0)
      usleep(1000);
  }
}

static int HasReplacedVdkBuf(u32 *p)
{
  return (p != NULL) ? 1 : 0;
}

static int HasRevertCtrlswBuf(u32 *p)
{
  return (p == NULL) ? 1 : 0;
}

#endif
