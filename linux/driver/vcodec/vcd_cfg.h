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
**                           include vcd cfg header                            **
*********************************************************************************/

#ifndef __VCD__CFG_H__
#define __VCD__CFG_H__

/* subsystem definition:
 *   1: defined
 *   0: not defined
 * subsystem0 is defined in default.
 */
#define SUBSYSTEM0    1
#define SUBSYSTEM1    1

/*******************************************************************
 *     subsys IO base and subsys core offset cfg
 *******************************************************************/
/* Sub-System 0 */
#ifdef EMU
/*customer specify according to own platform*/
#define SUBSYS_0_IO_ADDR                (0x6020000)
#else
/*customer specify according to own platform*/
#define SUBSYS_0_IO_ADDR                (0x600000)
#endif
/* subsys core offset definition:
 *   0xFFFF: not support this subsys core
 *   others: support this subsys core
 */
#define SUBSYS0_VCMD_OFFSET             (0)
#define SUBSYS0_VCD_OFFSET              (0x1000)
#define SUBSYS0_MMU_OFFSET              (0x4000)
#define SUBSYS0_MMU_WR_OFFSET           (0xFFFF)
#define SUBSYS0_AXIFE_OFFSET            (0x5000)
#define SUBSYS0_DEC400_OFFSET           (0x2000)
#define SUBSYS0_UFBC_OFFSET             (0xFFFF)
#define SUBSYS0_AXI2TO1_OFFSET          (0xFFFF)
#define SUBSYS0_ARB_OFFSET              (0x6000)
#define SUBSYS0_NOUSED_OFFSET           (0xFFFF)

/* Sub-System 1 */
#ifdef EMU
#define SUBSYS_1_IO_ADDR                (0x6000000)
#else
#define SUBSYS_1_IO_ADDR                (0x700000)
#endif
#define SUBSYS1_VCMD_OFFSET             (0)
#define SUBSYS1_VCD_OFFSET              (0x1000)
#define SUBSYS1_MMU_OFFSET              (0x4000)
#define SUBSYS1_MMU_WR_OFFSET           (0xFFFF)
#define SUBSYS1_AXIFE_OFFSET            (0x5000)
#define SUBSYS1_DEC400_OFFSET           (0x2000)
#define SUBSYS1_UFBC_OFFSET             (0xFFFF)
#define SUBSYS1_AXI2TO1_OFFSET          (0xFFFF)
#define SUBSYS1_ARB_OFFSET              (0x6000)
#define SUBSYS1_NOUSED_OFFSET           (0xFFFF)


/******************************************************************
 *         irq cfg for vcmd driver
 ******************************************************************/
#define VCMD0_IRQ                       -1
#define VCMD1_IRQ                       -1


/*******************************************************************
 *          io size and irq cfg for normal driver
 *******************************************************************/
/*0:no resource sharing inter subsystems 1: existing resource sharing*/
#define RESOURCE_SHARED_INTER_SUBSYS        0
/* Sub-System 0 */
#define SUBSYS_0_IO_SIZE                (0x7000)   /* bytes */
/* subsys core irq definition:
 *   -1: not support irq mode
 *   others: irq number, will use this number to request irq
 */
#define INT_PIN_SUBSYS_0_VCD            (-1)
#define INT_PIN_SUBSYS_0_MMU            (-1)
#define INT_PIN_SUBSYS_0_AXIFE          (-1)
#define INT_PIN_SUBSYS_0_MMU_WR         (-1)
#define INT_PIN_SUBSYS_0_DEC400         (-1)
#define INT_PIN_SUBSYS_0_UFBC           (-1)
#define INT_PIN_SUBSYS_0_NO             (-1)

/* Sub-System 1 */
#define SUBSYS_1_IO_SIZE                (0x7000)
#define INT_PIN_SUBSYS_1_VCD            (-1)
#define INT_PIN_SUBSYS_1_MMU            (-1)
#define INT_PIN_SUBSYS_1_AXIFE          (-1)
#define INT_PIN_SUBSYS_1_MMU_WR         (-1)
#define INT_PIN_SUBSYS_1_DEC400         (-1)
#define INT_PIN_SUBSYS_1_UFBC           (-1)
#define INT_PIN_SUBSYS_1_NO             (-1)

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

  /* Base address DDR register */
  #define PCI_DDR_BAR             4

#else //EMU

  #define VCMD_BUF_POOL_OFFSET      0x1000000
#ifdef PLATFORM_GEN7
  #define PCI_VENDOR_ID_HANTRO      0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x9014// 0x7011

  /* Base address got control register */
  #define PCI_CONTROL_BAR              2

  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#else //PLATFORM_GEN7
  #define PCI_VENDOR_ID_HANTRO      0x10ee//0x16c3
  #define PCI_DEVICE_ID_HANTRO      0x8014// 0x7011

  /* Base address got control register */
  #define PCI_CONTROL_BAR              4

  /* Base address DDR register */
  #define PCI_DDR_BAR             0
#endif //PLATFORM_GEN7
#endif //EMU


#define MAX_SUBSYS_NUM 4 /* up to 4 subsystem (temporary) */
#define HXDEC_MAX_CORES MAX_SUBSYS_NUM /* used in hantro_dec_xxx.c */

#define DEC_MAX_PPU_COUNT 2

#ifdef PPU_V9_2_3
#define PPU_REG_RANGE 128
#else //PPU_V9_2_1_2
#define PPU_REG_RANGE 64
#endif
#define PP0_START_REG 384
#define MAX_REG_COUNT (PP0_START_REG + PPU_REG_RANGE * DEC_MAX_PPU_COUNT)

#endif //__VCD__CFG_H__
