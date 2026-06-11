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

#ifndef __DWL_H__
#define __DWL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "dwlthread.h"

#define MAX_PIC_BUFFERS 34

#define MAX_DEC_DEV_NODES 1
#define MAX_MEM_DEV_NODES 1
#if defined(SUPPORT_DMA) && defined(USE_DMA_DRIVER)
#define MAX_DMA_DEV_NODES 1
#endif

/**
 * \brief DMA transfer direction
 * \ingroup common_group
 */
enum DWLDMADirection {
  HOST_TO_DEVICE = 0, /**< \brief copy the data from host memory to device memory */
  DEVICE_TO_HOST = 1  /**< \brief copy the data from device memory to host memory */
};

/**
 * \brief Specifies the DWL API function return values.
 * \ingroup common_group
 */
enum DWLRet {
  DWL_OK = 0,  /**< \brief the hardware is working correctly */
  DWL_ERROR = -1,  /**< \brief the hardware is running in error */
  DWL_CACHE_FATAL_RECOVERY = 2, /**< \brief the hw is running in error */
  DWL_CACHE_FATAL_UNRECOVERY = 3, /**< \brief the hw is running in unrecoveried error */
  DWL_CACHE_OK, /**< \brief the hw is working correctly */
  DWL_HW_WAIT_OK = DWL_OK,  /**< \brief the hardware is working correctly */
  DWL_HW_WAIT_ERROR = DWL_ERROR,  /**< \brief the hardware is running in error */
  DWL_HW_WAIT_TIMEOUT = 1, /**< \brief the hardware is time out */
  DWL_NOT_SUPPORT = 2 /**< \brief not support */
};

/**
 * \brief Specifies the DWL client type.
 * \ingroup common_group
 */
enum DWLClientType {
  DWL_CLIENT_TYPE_H264_DEC = 1 , /**< \brief The client type is h264*/
  DWL_CLIENT_TYPE_MPEG4_DEC = 2, /**< \brief The client type is mpeg4*/
  DWL_CLIENT_TYPE_JPEG_DEC = 3, /**< \brief The client type is jpeg*/
  DWL_CLIENT_TYPE_PP = 4, /**< \brief The client type is pp*/
  DWL_CLIENT_TYPE_VC1_DEC = 5, /**< \brief The client type is vc1*/
  DWL_CLIENT_TYPE_MPEG2_DEC = 6, /**< \brief The client type is mpeg2*/
  DWL_CLIENT_TYPE_VP6_DEC = 7, /**< \brief The client type is vp6*/
  DWL_CLIENT_TYPE_AVS_DEC = 8, /**< \brief The client type is avs*/
  DWL_CLIENT_TYPE_RV_DEC = 9, /**< \brief The client type is rv*/
  DWL_CLIENT_TYPE_VP8_DEC = 10, /**< \brief The client type is vp8*/
  DWL_CLIENT_TYPE_VP9_DEC = 11, /**< \brief The client type is vp9*/
  DWL_CLIENT_TYPE_HEVC_DEC =12, /**< \brief The client type is hevc*/
  DWL_CLIENT_TYPE_ST_PP = 14, /**< \brief The client type is pp*/
  DWL_CLIENT_TYPE_H264_MAIN10 = 15, /**< \brief The client type is h264_main10*/
  DWL_CLIENT_TYPE_AVS2_DEC = 16, /**< \brief The client type is avs2*/
  DWL_CLIENT_TYPE_AV1_DEC = 17, /**< \brief The client type is av1*/
  DWL_CLIENT_TYPE_VVC_DEC =18 , /**< \brief The client type is vvc*/
  DWL_CLIENT_TYPE_MAX = 19
};

/** \brief The page size and alignment for linear memory chunks. */
#define LINMEM_ALIGN 4096
/** \brief The nearest aligned linear memory address before the input address x. */
#define NEXT_ALIGNED(x) \
  ((((addr_t)(x)) + LINMEM_ALIGN - 1) / LINMEM_ALIGN * LINMEM_ALIGN)

/**
 * \brief only CPU use the buffer. non-secure CMA memory.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_CPU                 0x0000U
/**
 * \brief VPU can read, CAAM can write.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_SLICE               0x0001U
/**
 * \brief VPU can read and wirte, Render can read.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_DPB                 0x0002U
/**
 * \brief VPU can read, CPU can write. non-secure memory.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_VPU_WORKING         0x0003U
/**
 * \brief VPU can write, CPU can read and wirte. only for VP9 counter context table.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_VPU_WORKING_SPECIAL 0x0004U
/**
 * \brief only VPU can read and wirte the buffer.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_VPU_ONLY            0x0005U
/**
 * \brief VPU&CPU both can read and write the buffer.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_VPU_CPU             0x0006U
/**
 * \brief only host or only device use the buffer.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_DMA_DEVICE_ONLY     0x0100U
/**
 * \brief the buffer need transfer frome host to device.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_DMA_HOST_TO_DEVICE  0x0200U
/**
 * \brief the buffer need transfer frome device to host.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_DMA_DEVICE_TO_HOST  0x0300U
/**
 * \brief the buffer need transfer in both directions.
 * \ingroup common_group
*/
#define DWL_MEM_TYPE_DMA_HOST_AND_DEVICE 0x0400U

#ifdef SUPPORT_VCMD_M2M
#define MAX_VCMD_M2M_NUM 20
struct CDF_INFO {
  u32 cdf_size;
  addr_t src_cdf_addr;
  addr_t dst_prob_addr;
};
#endif

/** bit 22:16 is used to indicate the usage hint of the buffer */
#define DWL_MEM_USAGE_HINT_MASK  0x7F0000U
#define DWL_MEM_USAGE_HINT_SHIFT 16
/** bit 23 is used to indicate the mem attribute(META/CORE) */
#define DWL_MEM_ATTR_SHIFT 23
enum MEM_ATTR {
  META,
  CORE
};

#define ATTR_NOT_DEF META

#define DWL_HINT_LIST(DEF) \
  DEF(DWL_MEM_USAGE_UNDEFINED, ATTR_NOT_DEF) /* invalid*/ \
  DEF(DWL_MEM_USAGE_IN_STRM, CORE) /* R0 */ \
  DEF(DWL_MEM_USAGE_IN_STRM_SLICE, CORE) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_AVS2ALF_TABLE, META) /* R8 */ \
  DEF(DWL_MEM_USAGE_IN_AVS2WQM_TABLE, META) /* R10 */ \
  DEF(DWL_MEM_USAGE_IN_MBCTRL, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_MBRLC, CORE) /* R0 */ \
  DEF(DWL_MEM_USAGE_IN_MBDC_COEFF, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_MV, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_INTRA_PRED, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_RESIDUAL, CORE) /* R0 */ \
  DEF(DWL_MEM_USAGE_IN_CABAC_INIT, META) /* R3, R8, R10 */ \
  DEF(DWL_MEM_USAGE_IN_QTABLE, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_PROBTABLE, META) /* R8 */ \
  DEF(DWL_MEM_USAGE_IN_SEGMENT_MAP, META) /* R7, W8 */ \
  DEF(DWL_MEM_USAGE_IN_PPULANCZOS_TABLE, META) /* R(18,21,24,27) */ \
  DEF(DWL_MEM_USAGE_IN_AV1TILEINFO, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_IN_AV1GLOBAL_MODEL, META) /* R8 */ \
  DEF(DWL_MEM_USAGE_IN_AV1FILM_GRAIN, META) /* R2 */ \
  DEF(DWL_MEM_USAGE_IN_QUANT_MAT, META) /* R3 */ \
  DEF(DWL_MEM_USAGE_OUT_SURFACE, CORE) /* W0, W1*/ \
  DEF(DWL_MEM_USAGE_OUT_BITPLANE_CTRL, CORE) /* R0 */ \
  DEF(DWL_MEM_USAGE_OUT_RVRM_NCNB, ATTR_NOT_DEF) /* dev */ \
  DEF(DWL_MEM_USAGE_OUT_VP8DONWSCALE, ATTR_NOT_DEF) /* dev */\
  DEF(DWL_MEM_USAGE_OUT_REFERENCE, CORE) /* W0, W1, W10, W11*/ \
  DEF(DWL_MEM_USAGE_OUT_EXTRA_B, ATTR_NOT_DEF) /* dev */ \
  DEF(DWL_MEM_USAGE_OUT_PP, CORE) /* W(0, 1, 3, 4, 12, 13, (18-21)) */ \
  DEF(DWL_MEM_USAGE_OUT_CTXCOUNT, META) /* W30 */ \
  DEF(DWL_MEM_USAGE_TMP_AV1MULTICORE_SYNC, META) /* R13, W7 */ \
  DEF(DWL_MEM_USAGE_TMP_AV1AFBC_TILE, ATTR_NOT_DEF) /* dev */\
  DEF(DWL_MEM_USAGE_TMP_AV1FILTER, CORE) /* R/W5, W6, R/W14, R/W15, R/W16, R/W17, R(19,20,22,23,25,26,28,29), W(22-29) */\
  DEF(DWL_MEM_USAGE_TMP_DIRMV, META) /* R4, R13, W2, W7*/ \
  DEF(DWL_MEM_USAGE_TMP_PPUTILE_EDGE, CORE) /* R/W5, R/W6, R/W16, R/W17, R(19,20,22,23,25,26,28,29), W(22-29)*/ \
  DEF(DWL_MEM_USAGE_TMP_FILTER_CONTROL, META) /*R/W9*/ \
  DEF(DWL_MEM_USAGE_TMP_COEEFT, META) /* R4, W2 */ \
  DEF(DWL_MEM_USAGE_TMP_RPRWORK, ATTR_NOT_DEF) /* dev */\
  DEF(DWL_MEM_USAGE_TMP_PP, CORE) /* W3, W4, W12, W13, W(18-21)*/ \
  DEF(DWL_MEM_USAGE_TMP_EXTRA, ATTR_NOT_DEF) /* test */\
  DEF(DWL_MEM_USAGE_TMP_EXTERNAL, ATTR_NOT_DEF) /* test */\
  DEF(DWL_MEM_USAGE_TMP_EXTERNAL_FRM, ATTR_NOT_DEF) /* test */\
  DEF(DWL_MEM_USAGE_TMP_PROBTABLE_OUT, META) /* R10 */ \
  DEF(DWL_MEM_USAGE_TMP_MISC_LINEAR, META) /* R3, R10 */ \
  DEF(DWL_MEM_USAGE_TMP_VP9MULTICORE_SYNC,META) /* R13, W7*/ \
  DEF(DWL_MEM_USAGE_TMP_FAKERFC_TABLE, CORE) /* R11 */\
  DEF(DWL_MEM_USAGE_IN_STRM_STATUS, META) /* R11 */\
  DEF(DWL_MEM_USAGE_IN_AV1CDFS, ATTR_NOT_DEF) /* dev */\
  DEF(DWL_MEM_USAGE_IN_AV1CDFS_NDVC, ATTR_NOT_DEF) /* dev */\
  DEF(DWL_MEM_USAGE_IN_AV1CDFS_LAST, ATTR_NOT_DEF) /* dev */\


#define DEF_ENUM(enum, attr) enum,
enum DWLMemUsageHint {
  DWL_HINT_LIST(DEF_ENUM)
};

#define DEF_ATTR(enum, attr) enum##_Attr=attr,
enum DWLMemAttrHint {
  DWL_HINT_LIST(DEF_ATTR)
};

#define STR(str) #str

#define USAGE_ATTR(usage) usage##_Attr
#define SET_MEM_USAGE(mem_type, usage, is_secure) do { \
      (mem_type) |= (((usage)<<DWL_MEM_USAGE_HINT_SHIFT)| \
      ((USAGE_ATTR(usage)&(is_secure))<<DWL_MEM_ATTR_SHIFT)); \
      LOG_MEM_SECURE((is_secure), STR(usage), USAGE_ATTR(usage)); \
  } while(0)

#define DWLGetMemHint(mem_type) (((mem_type)&DWL_MEM_USAGE_HINT_MASK)>>DWL_MEM_USAGE_HINT_SHIFT)
#define DWLGetMemAttribute(mem_type) ((mem_type>>DWL_MEM_ATTR_SHIFT)&0x1)

/**
 * \brief Linear memory area descriptor.
 * \ingroup common_group
 */
struct DWLLinearMem {
  u32 *virtual_address; /**< \brief Pointer to the aligned virtual address of stream linear buffer */
  addr_t bus_address; /**< \brief Bus address of the aligned linear buffer. The start of the stream
                           buffer must be in a 128-bit aligned position or accessible
                           for reading from the previous 128-bit aligned position. */
  u32 *alloc_virtual_addr; /**< \brief The original virtual address of the memory for CPU to access. */
  addr_t alloc_bus_addr;    /**< \brief The original bus address of the memory for VPU to access. */
  u64 size;         /**< physical size in bytes (rounded to page multiple) */
  u64 logical_size; /**< requested size in bytes */
  u32 mem_type; /**< \brief The owner and attributes of the memory.
                 * \n Bits [15:8] indicate which IP is allowed to access the memory.
                 * \n Bits [7:0] indicate the attributes of the memory.*/
  unsigned long id;      /**< \brief The ID of the buffer. */
  void *priv; /**< private extension interface */
};

/**
 * \brief A structure used to pass parameters during initialization of the decoder wrapper layer.
 * \ingroup common_group
 */
struct DWLInitParam {
  enum DWLClientType client_type; /**< \brief Decoded stream type */
  char *dec_dev;  /**< \brief hantrodec select */
  char *mem_dev;  /**< \brief memalloc select */
#ifdef SUPPORT_DMA
  char *dma_dev;  /**< \brief dma select */
#endif
  void *priv; /**<\brief private extension interface */
/* set axi latency for FPGA test */
#ifdef SUPPORT_RANDOM_LATENCY
  u32 axi_lg_r;  /**< \brief set the base latency for AXI AR channel */
  u32 axi_lg_w;  /**< \brief set the base latency for AXI AW channel */
#endif
};

/**
 * \brief A structure containing HW fuse configuration information.
 * \ingroup common_group
 */
struct DWLHwFuseStatus {
  u32 vp6_support_fuse;            /**< \brief HW supports VP6 */
  u32 vp7_support_fuse;            /**< \brief HW supports VP7 */
  u32 vp8_support_fuse;            /**< \brief HW supports VP8 */
  u32 vp9_support_fuse;            /**< \brief HW supports VP9 */
  u32 h264_support_fuse;           /**< \brief HW supports H.264 */
  u32 HevcSupportFuse;             /**< \brief HW supports HEVC */
  u32 mpeg4_support_fuse;          /**< \brief HW supports MPEG-4 */
  u32 mpeg2_support_fuse;          /**< \brief HW supports MPEG-2 */
  u32 sorenson_spark_support_fuse; /**< \brief HW supports Sorenson Spark */
  u32 jpeg_support_fuse;           /**< \brief HW supports JPEG */
  u32 vc1_support_fuse;            /**< \brief HW supports VC-1 Simple */
  u32 pp_standalone_fuse;          /**< \brief HW supports post-processor */
  u32 pp_config_fuse;              /**< \brief HW post-processor functions bitmask */
  u32 max_dec_pic_width_fuse;      /**< \brief Maximum video decoding width supported  */
  u32 max_pp_out_pic_width_fuse;   /**< \brief Maximum output width of Post-Processor */
  u32 avs_support_fuse;            /**< \brief HW supports AVS */
  u32 rv_support_fuse;             /**< \brief HW supports RealVideo codec */
  u32 custom_mpeg4_support_fuse;   /**< \brief Fuse for custom MPEG-4 */
};


/**
 * \brief Used for IP-integration codec config.DWLCodecConfig is used for IP-integration codec config.
 * \ingroup common_group
 */
struct DWLCodecConfig {
  u32 input_yuv_swap; /**< \brief Set swaps for encoder frame input data. From MSB to LSB: swap each
                           128 bits, 64, 32, 16, 8. */
  u32 output_swap; /**< \brief Set swaps for encoder frame output (stream) data. From MSB to LSB:
		        swap each 128 bits, 64, 32, 16, 8. */
  u32 prob_table_swap; /**< \brief Set swaps for encoder probability tables. From MSB to LSB: swap each
		 	    128 bits, 64, 32, 16, 8. */
  u32 ctx_counter_swap; /**< \brief Set swaps for encoder context counters. From MSB to LSB: swap each
			     128 bits, 64, 32, 16, 8. */
  u32 statistics_swap; /**< \brief Set swaps for encoder statistics. From MSB to LSB: swap each 128
                            bits, 64, 32, 16, 8. */
  u32 fgbg_map_swap; /**< \brief Set swaps for encoder foreground/background map. From MSB to LSB:
                          swap each 128 bits, 64, 32, 16, 8. */

  bool irq_enable; /**< \brief ASIC interrupt enable.
                        This enables/disables the ASIC to generate interrupt. */

  u32 axi_read_id_base; /**< \brief AXI master read ID base. Read service type specific values are added
                             to this. */
  u32 axi_write_id_base; /**< \brief AXI master write id base. Write service type specific values are
			      added to this. */
  u32 axi_read_max_burst; /**< \brief AXI maximum read burst issued by the core [1..511] */

  u32 axi_write_max_burst; /**< \brief AXI maximum write burst issued by the core [1..511] */

  bool clock_gating_enabled; /**< \brief ASIC clock gating enable.
			          This enables/disables the ASIC to utilize clock gating.
				  If this is 'true', ASIC core clock is automatically shut down when
				  the encoder enable bit is '0' (between frames). */

  u32 timeout_period; /**< \brief ASIC internal timeout period in clocks.
			   Timeout counter acts as a watchdog that asserts a timeout interrupt
			   if bus is idle for the set period while the encoder is enabled.
			   Set to zero to disable. */
};
/**
 * \brief DWLReqInfo is used for DWLReserveHw and DWLReserveCmdBuf.Send the VCD source information.
 * \ingroup common_group
 */
struct DWLReqInfo {
  /** \brief core_mask for user to select cores: [0,15]core mask, [16,30]client type, [31]secure_mode */
  u32 core_mask;
  /** \brief Picture width */
  u32 width;
  /** \brief Picture height */
  u32 height;
  /** \brief instance ctx */
  void *owner;
};

/**
 * \brief low latency mode descriptor.
 * \ingroup common_group
 */
struct strmInfo {
  u32 low_latency; /**< \brief Flag that if stream run in low latecny mode. */
  u32 send_len; /**< \brief Data len perpare for hw. */
  addr_t strm_bus_addr; /**< \brief Host physical address decode node for current stream in buffer. */
  addr_t strm_bus_start_addr; /**< \brief Host physical start address for current buffer. */
  u8* strm_vir_addr; /**< \brief Virtual address decode node for current stream in buffer. */
  u8* strm_vir_start_addr; /**< \brief Virtual start address for current buffer. */
  u32 last_flag; /**< \brief Flag that last data for cureent stream. */
  addr_t strm_buff; /**< \brief input stream buffer virtual address. */
  addr_t buff_size; /**< \brief input stream buffer size. */
  addr_t strm_buff_bus_address; /**< \brief input stream buffer bus address. */
  u32 is_back_to_buffer_begin;/**< \brief for h264, set high when flushbits to buffer begin */
};

#ifdef FPGA_PERF_AND_BW
struct DWLPerfInfo{
  u64 bitrate;
  u64 write_bw;
  u64 read_bw;
  u32 cycles;
  u32 pic_num;
};
#endif

/**
 * Read the HW ID of the first core that supports the client type.
 * \ingroup common_group
 * \param [in]     instance       Pointer to a DWL instance.
 * \param [in]     client_type    Decoded stream type
 * \return         u32            The HW ID
 */
u32 DWLReadAsicID(const void *instance, enum DWLClientType client_type);

/**
 * Read the HW build ID.
 * \ingroup common_group
 * \param [in]     core_id        The core ID
 * \param [in]     client_type    Decoded stream type
 * \return         u32            The HW Build ID
 */
u32 DWLReadCoreHwBuildID(const void *instance, u32 core_id);

/**
 * Read the HW feature pointer according to core_id.
 * \ingroup common_group
 * \param [in]     instance            Pointer to a DWL instance.
 * \param [in]     core_id             The core ID
 * \return         const void *        The  HW feature pointer
 */
const void *DWLGetHwFeaturesByID(const void *instance, u32 core_id);

/**
 * Get the first HW feature list found by client type.
 * \ingroup common_group
 * \param [in]     instance            Pointer to a DWL instance.
 * \param [in]     hw_feature          Pointer to hardware feature list.
 * \param [in]     client_type         Decoded stream type
 * \param [in]     core_mask           Pointer of core mask of support client type, bit0-core0...bit31-core31, 0 means falied.
 * \return         const void *        The HW feature pointer
 */
const void *DWLGetHwFeaturesByClientType(const void *instance, enum DWLClientType client_type, u32 *core_mask);

/**
 * Get the number of ASIC cores, which is the all subsys numbers.
 * \ingroup common_group
 * \param [in]     instance            Pointer to a DWL instance.
 * \return         u32                 The number of ASIC cores.
 */
u32 DWLReadAsicCoreCount(const void *instance);

/**
 * Initialize a DWL instance.
 * \ingroup common_group
 * \param [in]     param                         Pointer to the DWLInitParam structure.
 * \return         void*                         Pointer to a DWL instance.
 */
const void *DWLInit(struct DWLInitParam *param);

/**
 * Release a DWl instance.
 * \ingroup common_group
 * \param [in]     instance                     Pointer to the DWL instance to be released.
 * \return         DWLRet                       DWL_OK, DWL_ERROR
 */
enum DWLRet DWLRelease(const void *instance);

/**
 * Reserves the hardware resource for one codec instance. \n
 * This function is part of the hardware sharing mechanism in use for multi-instance port-processor support.
 * When called it shall block and return when exclusive access to the required hardware can be granted to the calling client.
 * \ingroup common_group
 * \param [in]     instance                   Pointer to a DWL instance.
 * \param [in]     core_id                    Pointer to core_id.
 * \param [in]     client_type                Decoded stream type.
 * \return         DWLRet                     DWL_OK, DWL_ERROR
 */
enum DWLRet DWLReserveHw(const void *instance, struct DWLReqInfo *info, i32 *core_id);

/**
 * Releases a previously reserved hardware resource.
 * This function is called when the hardware has finished decoding frames.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Pointer to core_id.
 * \return         DWLRet                    DWL_OK.
 */
enum DWLRet DWLReleaseHw(const void *instance, i32 core_id);

/**
 * Allocates a contiguous linear block of memory for the reference frame buffer.
 * The buffer has to be a contiguous, linear memory buffer residing in the physical memory.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     size                      Size of the requested memory buffer (in bytes).
 *                                           Upon return, this parameter contains the exact size of the allocated memory area.
 * \param [in]     info                      Pointer to the DWLLinearMem structure for the returned memory block.
 * \return         DWLRet                    DWL_OK, DWL_ERROR
 */
enum DWLRet DWLMallocRefFrm(const void *instance, u64 size, struct DWLLinearMem *info);

/**
 * Frees a block of memory for the reference frame buffer previously allocated with DWLMallocRefFrm.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     info                      Pointer to the DWLLinearMem structure for the returned memory block.
 * \return         void                      None
 */
void DWLFreeRefFrm(const void *instance, struct DWLLinearMem *info);

/**
 * Allocates a contiguous, linear block of memory for the software/hardware shared memory buffer.
 * The buffer has to be a contiguous, linear memory buffer residing in the physical memory.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     size                      Size of the requested memory buffer (in bytes).
 * \param [in]     info                      Pointer to the DWLLinearMem structure for the memory block allocated.
 * \return         DWLRet                    DWL_OK, DWL_ERROR
 */
enum DWLRet DWLMallocLinear(const void *instance, u64 size, struct DWLLinearMem *info);

/**
 * Frees a block of memory for the software/hardware shared memory previously allocated with DWLMallocLinear.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     info                      Pointer to the DWLLinearMem structure for the memory block to free.
 * \return         void                      None
 */
void DWLFreeLinear(const void *instance, struct DWLLinearMem *info);

/**
 * Writes a 32-bit value to a specified hardware register.
 * All registers are written at once at the beginning of frame decoding.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset is relative to the hardware ID register (#0) in byte.
 * \param [in]     value                     Value to write to the hardware register.
 * \return         void                      None
 */
void DWLWriteReg(const void *instance, i32 core_id, u32 offset, u32 value);


/**
 * Writes num 32-bit value to a specified hardware register.
 * All registers are written at once at the beginning of frame decoding.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset is relative to the hardware ID register (#0) in byte.
 * \param [in]     from                      Source Register address.
 * \param [in]     num_regs                  The number of Registers should be Written.
 * \param [in]     print_flag                Whether print regs info.
 */
void DWLWriteRegs(const void *instance, i32 core_id, u32 offset, u32* from, u32 num_regs, u32 print_flag);
#ifdef FPGA_PERF_AND_BW
u32 DWLReadBw(const void *instance, u32 core_id, u32 num);
void DWLPerfInfoCollect(const void *instance, i32 core_id, struct DWLPerfInfo *perf_info);
#endif

/**
 * Reads and returns a 32-bit value from a specified hardware register.
 * The status register is read after every macroblock decoding by software.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset is relative to the hardware ID register (#0) in byte.
 * \return         u32                       The value of the specified hardware register.
 */
u32 DWLReadReg(const void *instance, i32 core_id, u32 offset);

/**
  * Reads num 32-bit value from a specified hardware register.
  * The status register is read after every macroblock decoding by software.
  * \ingroup common_group
  * \param [in]     instance                  Pointer to a DWL instance.
  * \param [in]     core_id                   Core ID of the hardware resource to release.
  * \param [in]     offset                    The first Register offset is relative to the hardware ID register (#0) in byte.
  * \param [in]     dest                      The destination Register address.
  * \param [in]     num                       The number of Registers should be copied.
------------------------------------------------------------------------------*/
void DWLReadRegs(const void *instance, i32 core_id, u32 offset, u32* dest, u32 num);
/**
 * Enables the hardware.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset.
 * \param [in]     value                     Value to enable hardware.
 * \return         void                      None
 */
void DWLEnableHw(const void *instance, i32 core_id, u32 offset, u32 value);

/**
 * This interface is a particular register writing function
 * that stops and disables the hardware by writing a specific register.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset.
 * \param [in]     value                     Value to disable hardware.
 * \return         void                      None
 */
void DWLDisableHw(const void *instance, i32 core_id, u32 offset, u32 value);

/**
 * Writes a 32-bit value to a specified hardware register.
 * This register is written during the frame decoding.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset is relative to the hardware ID register (#0) in bytes.
 * \param [in]     value                     Value to write to the hardware register.
 * \return         void                      None
 */
void DWLWriteRegToHw(const void *instance, i32 core_id, u32 offset, u32 value);

/**
 * Reads and returns a 32-bit value from a specified hardware register when hardware is running.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     offset                    Register offset is relative to the hardware ID register (#0) in bytes.
 * \return         u32                       The value of the specified hardware register.
 */
u32 DWLReadRegFromHw(const void *instance, i32 core_id, u32 offset);

/**
 * Synchronizes the software with the hardware.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     timeout                   Timeout period in millisecond. 0: do not wait. -1: infinite wait.
 * \return         DWLRet                    DWL_HW_WAIT_OK, DWL_HW_WAIT_ERROR, DWL_HW_WAIT_TIMEOUT
 */
enum DWLRet DWLWaitHwReady(const void *instance, i32 core_id, u32 timeout);

typedef void DWLIRQCallbackFn(void *arg, i32 core_id);

/**
 * Sets the IRQ callback function.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     callback_fn               Pointer to the callback function.
 * \param [in]     arg                       Arguments set for callback function.
 * \return         void                      None
 */
void DWLSetIRQCallback(const void *instance, i32 core_id,
                       DWLIRQCallbackFn *callback_fn, void *arg);
/**
 * Read the pp configuration.
 * \ingroup common_group
 * \param [in]     instance                  Pointer to a DWL instance.
 * \param [in]     core_id                   Core ID of the hardware resource to release.
 * \param [in]     ppu_cfg                   Pointer to the PpUnitIntConfig stucture.
 * \param [in]     pjpeg_coeff_buffer_size   The pjpeg coeff buffer size.(Pjpeg only).
 * \return         void                      None
 */
void DWLReadPpConfigure(const void *instance, u32 core_id, void *ppu_cfg, u32 pjpeg_coeff_buffer_size);

/**
 * Allocates a block of n bytes of memory, returning a pointer to the beginning of the block.
 * \ingroup common_group
 * \param [in]     n                         Size of memory block in bytes
 * \return         void*                     On success, a pointer to the memory block allocated by the function.
 *                                           If the function failed to allocate the requested block of memory, a NULL pointer is returned.
 */
void *DWLmalloc(size_t n);

/**
 * Deallocates a block of memory, previously allocated by a call to DWLmalloc() or DWLcalloc().
 * \ingroup common_group
 * \param [in]     p                         Pointer to a memory block previously allocated with DWLmalloc() or DWLcalloc().
 * \return         void                      None
 */
void DWLfree(void *p);

/**
 * Allocates a block of memory for an array of n elements, each of them s bytes long, and initializes all its bits to zero.
 * \ingroup common_group
 * \param [in]     n                         Number of elements to allocate.
 * \param [in]     s                         Size of each element.
 * \return         void *                    On success, a pointer to the memory block is allocated by the function.
 *                                           If the function failed to allocate the requested block of memory, a NULL pointer is returned.
 */
void *DWLcalloc(size_t n, size_t s);

/**
 * Copies the values of n bytes from the location pointed to by s directly to the memory block pointed to by d.
 * \ingroup common_group
 * \param [in]     d                         Pointer to the destination array where the content is to be copied.
 * \param [in]     s                         Pointer to the source of data to be copied.
 * \param [in]     n                         Number of bytes to copy.
 * \return         void *                    A pointer to the destination array, d, is returned.
 */
void *DWLmemcpy(void *d, const void *s, size_t n);

/**
 * Allocates a block of memory for an array of n elements, each of them s bytes long, and initializes all its bits to zero.
 * \ingroup common_group
 * \param [in]     d                         Pointer to the block of memory to fill.
 * \param [in]     c                         Value to be set.
 * \param [in]     n                         Number of bytes to be set to the value c.
 * \return         void *                    A pointer to the block of memory to fill, d, is returned.
 */
void *DWLmemset(void *d, i32 c, size_t n);

/**
 * Copies characters from a specail host buffer to Physical linear buffer with offset. Customer can adjust it.
 * \ingroup common_group
 * \param [in]     instance        Pointer to a DWL instance.
 * \param[in]      device_bus_addr Device memory information needs to be synchronized
 * \param [in]     s               The special host Buffer to copy
 * \param [in]     n               Number of bytes to copy
 */
void *DWLLinearMemcpy(const void *instance, addr_t device_bus_addr, const void *s, size_t n);

/**
 * Sets Physical linear buffer to a specified character. Customer can adjust it.
 * \ingroup common_group
 * \param [in]     instance   Pointer to a DWL instance.
 * \param[in]      mem        memory information needs to be synchronized
 * \param[in]      offset     The offset of memsync in bytes; implemented alignment in API
 * \param [in]     c          Character to set
 * \param [in]     n          Number of characters
 */
void *DWLLinearMemset(const void *instance, struct DWLLinearMem *mem, i32 offset, i32 c, size_t n);

/**
 * Indicates if vcmd used.
 * \ingroup common_group
 * \param [in]     instance                    Pointer to a DWL instance.
 * \return         u32                         The num of subsystems with vcmd.
 */
u32 DWLVcmdIsUsed(const void *instance);

/**
 * Reserve one valid command buffer.
 * \ingroup common_group
 * \param [in]     instance                       Pointer to a DWL instance.
 * \param [in]     client_type                    Decoded stream type.
 * \param [in]     width                          The video decoding width.
 * \param [in]     height                         The video decoding height.
 * \param [in]     cmd_buf_id                     A pointer to the cmd buf id
 * \return         DWLRet                         DWL_OK, DWL_ERROR
 */
enum DWLRet DWLReserveCmdBuf(const void *instance, struct DWLReqInfo *info, u32 *cmd_buf_id);

/**
 * Enable one command buffer: link and enable.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     cmd_buf_id                    A pointer to the cmd buf id.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLEnableCmdBuf(const void *instance, u32 cmd_buf_id);

/**
 * Wait cmd buffer ready.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     cmd_buf_id                    A pointer to the cmd buf id.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLWaitCmdBufReady(const void *instance, u16 cmd_buf_id);

/**
 * Release one command buffer.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     cmd_buf_id                    A pointer to the cmd buf id.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLReleaseCmdBuf(const void *instance, u32 cmd_buf_id);

/**
 * Wait all left cmd bufs done.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     owner                         A pointer to decoder instance.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLWaitCmdbufsDone(const void *instance, const void *owner);

/**
 * Flush vcmd register.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     cmd_buf_id                    A pointer to a decoder instance.
 * \param [in]     dec_regs                      A pointer to decoder regs.
 * \param [in]     mc_fresh_regs                 A pointer to mc fresh regs based the current core
 * \param [in]     mc_buf_id                     The mc buf id.
 * \param [in]     owner                         A pointer to a  decoder instance.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLFlushRegister(const void *instance, u32 cmd_buf_id, u32 *dec_regs, u32 *mc_fresh_regs, u32 mc_buf_id);

/**
 * Refresh register, nothing needs to do, already refresh in DWLWaitCmdBufReady().
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     cmd_buf_id                    The cmd buf id.
 * \param [in]     dec_regs                      The pointer to decoder regs.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLRefreshRegister(const void *instance, u32 cmd_buf_id, u32 *dec_regs);

/**
 * Refresh the status regs of requirement for Vcmd multicore.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     dec_regs                      The pointer to decoder regs.
 * \param [in]     cmdbuf_id                     The cmd buf id.
 * \return         DWLRet                        DWL_OK, DWL_ERROR
 */
enum DWLRet DWLVcmdMCRefreshStatusRegs(const void *instance, u32 *dec_regs, u32 cmdbuf_id);

/**
 * \brief  Get virtual MC buf id of Vcmd.
 * \ingroup common_group
 * \param [in]     instance                      Pointer to a DWL instance.
 * \param [in]     cmdbuf_id                     The cmd buf id.
 * \return         i32                           The current core id.
 */
i32 DWLGetVcmdMCVirtualCoreId(const void *instance, u32 cmdbuf_id);

void DWLCmdPushSliceRegs(const void *instance, u32 cmd_buf_id, u32 *regs);
void DWLAbortCmdbuf(const void *instance, u16 cmd_buf_id);
/* wait all dec done of the owner */
void DWLWaitOwnerDone(const void *instance, const void *owner);
/* drop cmd job of owner */
void DWLDropCmdbufs(const void *instance, const void *owner);
#ifdef SUPPORT_VCMD_M2M
void DWLCmdM2MSendData(const void *instance, struct CDF_INFO *head_data,
  struct CDF_INFO *tail_data, u32 cmdbuf_id);
u32 DWLCheckVcmdM2M(const void *instance);
#endif

/**
 * Read one byte from a given pointer.
 * \ingroup common_group
 * \param [in]     p                         Pointer to the memory block to read.
 * \return         u8                        One byte data from the given address.
 */
u8 DWLReadByte(const u8 *p);

/**
 * Read one byte from the memory block pointed to by p. \n
 * This function is just an interface to access private or protected buffer; it should be realized by customer.
 * \ingroup common_group
 * \param [in]     p                         Pointer to a virtual address in the private buffer.
 * \return         u8                        One byte data from the given address.
 */
u8 DWLPrivateAreaReadByte(const u8 *p);

/**
 * \brief  Write one byte data to the memory block pointed to by p. \n
 * This function is just an interface to access private or protected buffer; it should be realized by customer.
 * \ingroup common_group
 * \param [in]     p                         Pointer to the memory block to write.
 * \param [in]     data                      One byte of data to write.
 * \return         void                      None
 */
void DWLPrivateAreaWriteByte(u8 *p, u8 data);

/**
 *  \brief Copies the values of n bytes from the location pointed to by s directly to the memory block pointed to by d. \n
 * This function is just an interface to access private or protected buffer; it should be realized by customer.
 * \ingroup common_group
 * \param [in]     d                         Pointer to the destination array where the content is to be copied.
 * \param [in]     s                         Pointer to the source of data to be copied.
 * \param [in]     n                         Number of bytes to copy.
 * \return         void *                    A pointer to the destination array, d, is returned.
 */
void * DWLPrivateAreaMemcpy(void *d,  const void *s,  u32 n);

/**
 * Sets the first n bytes of the block of memory pointed to by d to the specified value c. \n
 * This function is just an interface to access private or protected buffer; it should be realized by customer.
 * \ingroup common_group
 * \param [in]     p                         Pointer to the block of memory to fill.
 * \param [in]     c                         Value to be set.
 * \param [in]     n                         number of bytes to be set to the value c.
 * \return         void *                    A pointer to the block of memory to fill, d, is returned.
 */
void * DWLPrivateAreaMemset(void *p,  i32 c, u32 n);

void DWLGetActiveTime(const void *instance, unsigned long long *tile_active_time, unsigned long long *active_time);

#ifdef ASIC_TRACE_SUPPORT
void DWLSetSimMc(const void *instance);
#endif

/* For hw simulation purpose, HW need use DWLMallocRefFrm to generate trace files */
#ifdef ASIC_TRACE_SUPPORT
#define DWLMALLOC_LINEAR_MEM2 DWLMallocRefFrm
#define DWLFREE_LINEAR_MEM2 DWLFreeRefFrm
#else
#define DWLMALLOC_LINEAR_MEM2 DWLMallocLinear
#define DWLFREE_LINEAR_MEM2 DWLFreeLinear
#endif

#ifdef _HAVE_PTHREAD_H
/**
 * \brief Decoder wrapper layer functionality.
 * \ingroup common_group
 */
struct DWL {
  /**< \brief HW sharing */
  enum DWLRet (*ReserveHw)(const void *instance, struct DWLReqInfo *info, i32 *core_id);
  enum DWLRet (*ReleaseHw)(const void *instance, i32 core_id);
  /**< \brief Physical, linear memory functions */
  enum DWLRet (*MallocLinear)(const void *instance, u64 size,
                      struct DWLLinearMem *info);
  void (*FreeLinear)(const void *instance, struct DWLLinearMem *info);
  /**< \brief Register access */
  void (*WriteReg)(const void *instance, i32 core_id, u32 offset, u32 value);
  u32 (*ReadReg)(const void *instance, i32 core_id, u32 offset);
  /**< \brief HW starting/stopping */
  void (*EnableHw)(const void *instance, i32 core_id, u32 offset, u32 value);
  void (*DisableHw)(const void *instance, i32 core_id, u32 offset, u32 value);
  /**< \brief HW synchronization */
  enum DWLRet (*WaitHwReady)(const void *instance, i32 core_id, u32 timeout);
  void (*SetIRQCallback)(const void *instance, i32 core_id,
                         DWLIRQCallbackFn *callback_fn, void *arg);
  /**< \brief Virtual memory functions. */
  void *(*Malloc)(size_t n);
  void (*Free)(void *p);
  void *(*Calloc)(size_t n, size_t s);
  void *(*Memcpy)(void *d, const void *s, size_t n);
  void *(*Memset)(void *d, i32 c, size_t n);
  /**< \brief POSIX compatible threading functions. */
  i32 (*Pthread_create)(pthread_t *tid, const pthread_attr_t *attr,
                        void *(*start)(void *), void *arg);
  void (*Pthread_exit)(void *value_ptr);
  i32 (*Pthread_join)(pthread_t thread, void **value_ptr);
  i32 (*Pthread_mutex_init)(pthread_mutex_t *mutex,
                            const pthread_mutexattr_t *attr);
  i32 (*Pthread_mutex_destroy)(pthread_mutex_t *mutex);
  i32 (*Pthread_mutex_lock)(pthread_mutex_t *mutex);
  i32 (*Pthread_mutex_unlock)(pthread_mutex_t *mutex);
  i32 (*Pthread_cond_init)(pthread_cond_t *cond,
                           const pthread_condattr_t *attr);
  i32 (*Pthread_cond_destroy)(pthread_cond_t *cond);
  i32 (*Pthread_cond_wait)(pthread_cond_t *cond, pthread_mutex_t *mutex);
  i32 (*Pthread_cond_signal)(pthread_cond_t *cond);
  /**< \brief API trace function. Set to NULL if no trace wanted. */
  int (*Printf)(const char *string, ...);
};
#endif
#ifdef __cplusplus
}
#endif

#endif /* __DWL_H__ */
