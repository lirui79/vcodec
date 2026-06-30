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
**                      include vcx define headers                              **
*********************************************************************************/

#ifndef _VCX_DEFINE_H_
#define _VCX_DEFINE_H_

#include "vcx_module_type.h"

/****************************************************************
 * Macros related to PCIe Platform Config
 ****************************************************************
 */
#ifdef EMU
  #define VCMD_BUF_POOL_OFFSET      0x5000000

  #define PCI_VENDOR_ID_HANTRO      0x1d9b//0x16c3//0x1ae0
  #define PCI_DEVICE_ID_HANTRO      0xface//0x7011//0x001a

  /* Base address got control register */
  #define PCI_CONTROL_BAR              2
  #define PCI_H2_BAR                   2
  /* Base address DDR register */
  #define PCI_DDR_BAR             4

#else //EMU

  #define VCMD_BUF_POOL_OFFSET      0x1000000
#ifdef PLATFORM_GEN7
  #define PCI_VENDOR_ID_HANTRO      0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x9014// 0x7011

  /* Base address got control register */
  #define PCI_CONTROL_BAR              2
  #define PCI_H2_BAR                   2
  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#else //PLATFORM_GEN7
  #define PCI_VENDOR_ID_HANTRO      0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x8014// 0x7011

  /* Base address got control register */
  #define PCI_CONTROL_BAR              4
  #define PCI_H2_BAR                   4
  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#endif //PLATFORM_GEN7
#endif //EMU

/*---------------------------------------------------------------
 * Macros related to dev/process workload management
 *---------------------------------------------------------------
 */

#ifndef MAX_SUBSYS_NUM
#define MAX_SUBSYS_NUM     4 /* up to 4 subsystem (temporary) */
#endif
#ifndef HXDEC_MAX_CORES
#define HXDEC_MAX_CORES    MAX_SUBSYS_NUM
#endif
#define MAX_VCMD_CORE_NUM  MAX_SUBSYS_NUM

#define DEC_MAX_PPU_COUNT 2

#ifdef PPU_V9_2_3
#define PPU_REG_RANGE 128
#else //PPU_V9_2_1_2
#define PPU_REG_RANGE 64
#endif
#define PP0_START_REG 384
#define MAX_REG_COUNT (PP0_START_REG + PPU_REG_RANGE * DEC_MAX_PPU_COUNT)

/*priority support*/
#define MAX_CMDBUF_PRIORITY_TYPE   2 //0:normal priority,1:high priority

#define CMDBUF_PRIORITY_NORMAL     0
#define CMDBUF_PRIORITY_HIGH       1

/* exchange parameter input bit mask */
#define EXCH_PRIO_BIT                    0
#define EXCH_END_CMD_BIT                 1

#define EXCH_BIT_MASK(bit)               (1 << (bit))
#define EXCH_S_BIT(param, bit)           ((param) |= EXCH_BIT_MASK(bit)) // set bit
#define EXCH_G_BIT(param, bit)           (((param) & EXCH_BIT_MASK(bit)) >> (bit)) // get bit

/******************************************************************************
 * VCMD Command Definitions
 ******************************************************************************/
#define UNSIGNED_OPCODE(A)        ((u32)(A))
#define OPCODE_MASK               (UNSIGNED_OPCODE(0x1F) << 27)
#define OPCODE_WREG               (UNSIGNED_OPCODE(0x01) << 27)
#define OPCODE_END                (UNSIGNED_OPCODE(0x02) << 27)
#define OPCODE_NOP                (UNSIGNED_OPCODE(0x03) << 27)
#define OPCODE_RREG               (UNSIGNED_OPCODE(0x16) << 27)
#define OPCODE_INT                (UNSIGNED_OPCODE(0x18) << 27)
#define OPCODE_JMP                (UNSIGNED_OPCODE(0x19) << 27)
#define OPCODE_STALL              (UNSIGNED_OPCODE(0x09) << 27)
#define OPCODE_CLRINT             (UNSIGNED_OPCODE(0x1a) << 27)
#define OPCODE_JMP_RDY0           (UNSIGNED_OPCODE(0x19) << 27)
#define OPCODE_JMP_RDY1           ((UNSIGNED_OPCODE(0x19) << 27) | (1 << 26))
#define OPCODE_M2M                (UNSIGNED_OPCODE(0x1b) << 27)
#define OPCODE_MSET               (UNSIGNED_OPCODE(0x1c) << 27)
#define OPCODE_M2MP               (UNSIGNED_OPCODE(0x1d) << 27)
#define JMP_IE_1                  BIT(25)
#define JMP_RDY_1                 BIT(26)

#define CLRINT_OPTYPE_READ_WRITE_1_CLEAR         0
#define CLRINT_OPTYPE_READ_WRITE_0_CLEAR         1
#define CLRINT_OPTYPE_READ_CLEAR                 2

#define VCE_FRAME_RDY_INT_MASK                        0x0001
#define VCE_CUTREE_RDY_INT_MASK                       0x0002
#define VCE_DEC400_INT_MASK                           0x0004
#define VCE_L2CACHE_INT_MASK                          0x0008
#define VCE_MMU_INT_MASK                              0x0010
#define CUTREE_MMU_INT_MASK                               0x0020

#define VCD_FRAME_RDY_INT_MASK                        0x0100
#define VCD_DEC400_INT_MASK                           0x0400
#define VCD_L2CACHE_INT_MASK                          0x0800
#define VCD_MMU_INT_MASK                              0x1000

#define VCD_DEC400_INT_MASK_1_1_1                      0x0200
#define VCD_L2CACHE_INT_MASK_1_1_1                     0x0400
#define VCD_MMU_INT_MASK_1_1_1                         0x0800

/******************************************************************************
 * VCMD HW version IDs
 ******************************************************************************/
#define VCMD_HW_ID                 0x4342
#define HW_ID_1_0_C                0x43421001
#define HW_ID_1_1_1                0x43421101
#define HW_ID_1_1_2                0x43421102
#define HW_ID_1_1_3                0x43421103
#define HW_ID_1_2_1                0x43421201
#define HW_ID_1_5_0                0x43421500
#define HW_ID_1_5_2                0x43421502
#define HW_ID_1_5_3                0x43421503
#define HW_ID_1_5_9                0x43421509
#define HW_ID_1_5_10               0x4342150A
#define HW_ID_1_6_0                0x43421600

/****************************************************************************
 * VCARB version
 ****************************************************************************/
#define VCARB_VERSION_2_0         0x2  // arbiter2.0, dual-os version
#define VCARB_VERSION_3_0         0x3  // arbiter3.0, virtulization version

#define VCMD_LINE_BUFFER_INTERRUPT 509
#define VCMD_SLICE_RDY_INTERRUPT   510
#define VCMD_SLICE_RDY_NUM         511

/* platform frequency: need adjust it according your platform
 *   such as define it as 5000000 for 5Mhz fpga platform
 */
#define PLATFORM_FREQUENCY                        (5 * 1000000L)
/* time to wotchdog wait for one job */
#define ONE_JOB_WAIT_TIME                        (100 * 1000)	//For FPGA platform (ms)
//#define ONE_JOB_WAIT_TIME                        (500)	//For SoC platform
#define ONE_SLICE_WAIT_TIME                      (500) //ms

/* for 5Mhz fpga platform, one hw cycle is 200ns
 */
#define CYCLE_TO_CPU_TIME                     (1000000000L / PLATFORM_FREQUENCY) //ns

/* if vcmd arbiter is hang, reset it.
 * reset time is one time_window.
 */

#define VCMD_ARBITER_RESET_TIME ((1 << arbiter_timewindow) * \
								  CYCLE_TO_CPU_TIME / 1000000) //ms


/* VF timeout time for arbiter fe_timeout/bus_hack to reset vcmd */
#define SW_TIMEOUT_TIME_FOR_ARBITER              (ONE_JOB_WAIT_TIME * 10) // customer to set.

#ifndef ASIC_SWREG_AMOUNT
#define ASIC_SWREG_AMOUNT                  512 //from encswhwregister.h
#endif

#ifdef HANTROVCMD_ENABLE_IP_SUPPORT
#define VCMD_REG_ID_SW_INIT_CMD0        32
#define ASIC_VCMD_SWREG_AMOUNT          64
#else
#define ASIC_VCMD_SWREG_AMOUNT          31
#endif

/*these size need to be modified according to hw config.*/
#define ENCODER_REGISTER_SIZE              ASIC_SWREG_AMOUNT
#define DECODER_REGISTER_SIZE              512
#define IM_REGISTER_SIZE                   ASIC_SWREG_AMOUNT
#define JPEG_ENCODER_REGISTER_SIZE         ASIC_SWREG_AMOUNT
#define JPEG_DECODER_REGISTER_SIZE         512
#define AXI2TO1_REGISTER_SIZE             11

#define VCMD_REGISTER_SIZE                 ASIC_VCMD_SWREG_AMOUNT

#define DEC400_REGISTER_SIZE               1600
#define MMU_REGISTER_SIZE                  500
#define L2CACHE_REGISTER_SIZE              500
#define AXIFE_REGISTER_SIZE                500
#define UFBC_REGISTER_SIZE                 32
#define SLCIE_ADDR_OFFSET                  0x100

#define ASIC_STATUS_SEGMENT_READY       0x1000
#define ASIC_STATUS_FUSE_ERROR          0x200
#define ASIC_STATUS_SLICE_READY         0x100
#define ASIC_STATUS_LINE_BUFFER_DONE    0x080  /* low latency */
#define ASIC_STATUS_POLL_SLICEINFO_TIMEOUT 0x2000
#ifdef LOW_LATENCY_SLICEINFO_SUPPORT
#define SLICEINFO_ADDR_OFFSET              0x200
#endif

#define ASIC_STATUS_UFBC_DEC_ERR 0x4000  //bit14
#define ASIC_STATUS_UFBC_CFG_ERR 0x8000  //bit15

#define ASIC_STATUS_SBI_OUT_OF_SYNC 0x20000
#define ASIC_STATUS_SBI_TIMEOUT 0x10000

#define ASIC_STATUS_HW_TIMEOUT 0x040

#define ASIC_STATUS_BUFF_FULL           0x020
#define ASIC_STATUS_HW_RESET            0x010
#define ASIC_STATUS_ERROR               0x008
#define ASIC_STATUS_FRAME_READY         0x004
#define ASIC_IRQ_LINE                   0x001
#define ASIC_STATUS_ALL       (ASIC_STATUS_SEGMENT_READY |\
			       ASIC_STATUS_FUSE_ERROR |\
			       ASIC_STATUS_SLICE_READY |\
			       ASIC_STATUS_LINE_BUFFER_DONE |\
			       ASIC_STATUS_HW_TIMEOUT |\
			       ASIC_STATUS_BUFF_FULL |\
			       ASIC_STATUS_HW_RESET |\
			       ASIC_STATUS_ERROR |\
			       ASIC_STATUS_FRAME_READY |\
				   ASIC_STATUS_UFBC_DEC_ERR |\
			       ASIC_STATUS_SBI_OUT_OF_SYNC |\
			       ASIC_STATUS_SBI_TIMEOUT     |\
				   ASIC_STATUS_UFBC_CFG_ERR |\
				   ASIC_STATUS_POLL_SLICEINFO_TIMEOUT)


#endif /* _VCX_DEFINE_H_ */