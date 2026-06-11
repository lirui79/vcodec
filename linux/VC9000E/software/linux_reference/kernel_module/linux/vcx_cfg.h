/*
 * SPDX_license-Identifier: GPL-2.0  WITH Linux-syscall-note OR BSD-3-Clause
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */

#ifndef __VCX__CFG_H__
#define __VCX__CFG_H__

/* subsystem definition:
 *   1: defined
 *   0: not defined
 * subsystem0 is defined in default.
 */
#define SUBSYSTEM0    1
#define SUBSYSTEM1    0

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
#define SUBSYS0_VCE_OFFSET              (0x1000)
#define SUBSYS0_MMU_OFFSET              (0x4000)
#define SUBSYS0_MMU1_OFFSET             (0xFFFF)
#define SUBSYS0_AXIFE_OFFSET            (0xFFFF)
#define SUBSYS0_AXIFE1_OFFSET           (0xFFFF)
#define SUBSYS0_DEC400_OFFSET           (0xFFFF)
#define SUBSYS0_UFBC_OFFSET             (0xFFFF)
#define SUBSYS0_L2CACHE_OFFSET          (0xFFFF)
#define SUBSYS0_AXI2TO1_OFFSET          (0xFFFF)

/* Sub-System 1 */
#ifdef EMU
#define SUBSYS_1_IO_ADDR                (0x6000000)
#else
#define SUBSYS_1_IO_ADDR                (0xA0000)
#endif
#define SUBSYS1_VCMD_OFFSET             (0)
#define SUBSYS1_CUTREE_OFFSET           (0x1000)
#define SUBSYS1_MMU_OFFSET              (0xFFFF)
#define SUBSYS1_MMU1_OFFSET             (0xFFFF)
#define SUBSYS1_AXIFE_OFFSET            (0xFFFF)
#define SUBSYS1_AXIFE1_OFFSET           (0xFFFF)
#define SUBSYS1_DEC400_OFFSET           (0xFFFF)
#define SUBSYS1_UFBC_OFFSET             (0xFFFF)
#define SUBSYS1_L2CACHE_OFFSET          (0xFFFF)
#define SUBSYS1_AXI2TO1_OFFSET          (0xFFFF)

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
#define SUBSYS_0_IO_SIZE                (10000 * 4)   /* bytes */
/* subsys core irq definition:
 *   -1: not support irq mode
 *   others: irq number, will use this number to request irq
 */
#define INT_PIN_SUBSYS_0_VCE            (-1)
#define INT_PIN_SUBSYS_0_MMU            (-1)
#define INT_PIN_SUBSYS_0_AXIFE          (-1)
#define INT_PIN_SUBSYS_0_MMU_1          (-1)
#define INT_PIN_SUBSYS_0_DEC400         (-1)
#define INT_PIN_SUBSYS_0_AXIFE_1        (-1)
#define INT_PIN_SUBSYS_0_L2CACHE        (-1)
#define INT_PIN_SUBSYS_0_UFBC           (-1)

/* Sub-System 1 */
#define SUBSYS_1_IO_SIZE                (10000 * 4)
#define INT_PIN_SUBSYS_1_CUTREE         (-1)
#define INT_PIN_SUBSYS_1_MMU            (-1)
#define INT_PIN_SUBSYS_1_AXIFE          (-1)
#define INT_PIN_SUBSYS_1_MMU_1          (-1)
#define INT_PIN_SUBSYS_1_DEC400         (-1)
#define INT_PIN_SUBSYS_1_AXIFE_1        (-1)
#define INT_PIN_SUBSYS_1_L2CACHE        (-1)
#define INT_PIN_SUBSYS_1_UFBC           (-1)

/****************************************************************
 * Macros related to PCIe Platform Config
 ****************************************************************
 */
#ifdef PCIE_EN
/*******************PCIE CONFIG*************************/
#ifdef EMU
#define VCMD_BUF_POOL_OFFSET           0x5000000
#define PCI_VENDOR_ID_HANTRO           0x1d9b//0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO           0xface//0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              2
/* Base address DDR register */
#define PCI_DDR_BAR             4
#else // EMU
#define VCMD_BUF_POOL_OFFSET           0x1000000
#define PCI_VENDOR_ID_HANTRO           0x10ee//0x16c3
#define PCI_DEVICE_ID_HANTRO           0x8014// 0x7011
/* Base address got control register */
#define PCI_H2_BAR              4
/* Base address DDR register */
#define PCI_DDR_BAR             0
#endif //EMU
#endif // PCIE_EN

#endif //__VCX__CFG_H__
