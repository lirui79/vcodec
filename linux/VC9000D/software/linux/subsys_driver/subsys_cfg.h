/*
 * SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
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
 */

/*---------------------------------------------------------------
 * Macros related to PCIe Platform Config
 *---------------------------------------------------------------
 */

#ifndef _SUBSYS_CFG_H_
#define _SUBSYS_CFG_H_

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

static struct SubsysDesc subsys_array[] = {
	{0, 0, 0x600000},
	{0, 1, 0x700000},
};
static struct CoreDesc core_array[] = {
	{0, 0, HW_VCD, 0x1000, MAX_REG_COUNT*4, -1},
	{0, 0, HW_MMU, 0x4000, 238*4, -1},
	{0, 0, HW_VCMD, 0x0000, 30*4, -1},
	{0, 0, HW_ARB,0x6000, 23*4, -1},
	{0, 0, HW_DEC400, 0x2000, 1568*4, -1},
	{0,0,HW_AXIFE,0x5000,64*4,-1},
	{0, 1, HW_VCD, 0x1000, MAX_REG_COUNT*4, -1},
	{0, 1, HW_MMU, 0x4000, 238*4, -1},
	{0, 1, HW_VCMD, 0x0000, 30*4, -1},
	{0, 1, HW_DEC400, 0x2000, 1568*4, -1},
	{0,1,HW_AXIFE,0x5000,64*4,-1},
};
#endif //_SUBSYS_CFG_H_
