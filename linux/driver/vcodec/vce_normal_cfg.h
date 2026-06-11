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

#ifndef __VCE_NORMAL_CFG_H__
#define __VCE_NORMAL_CFG_H__

#include "vce_cfg.h"
#include "vcx_normal_cfg.h"

/* Configure information without CMD, fill according to System Memory Map*/

/* Subsystem configure
 * base_addr, iosize, resource_shared
 */
static SUBSYS_CONFIG subsys_array[] = {
	{ SUBSYS_0_IO_ADDR, SUBSYS_0_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS },
#if SUBSYSTEM1
	{SUBSYS_1_IO_ADDR, SUBSYS_1_IO_SIZE, RESOURCE_SHARED_INTER_SUBSYS},
#endif
};

/* Core configure
 * slice_idx, core_type, offset, reg_size, irq
 */
static CORE_CONFIG core_array[] = {
	{0, CORE_VCE, SUBSYS0_VCE_OFFSET, ENCODER_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_VCE},
	{0, CORE_MMU, SUBSYS0_MMU_OFFSET, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_MMU},
	{0, CORE_AXIFE, SUBSYS0_AXIFE_OFFSET, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_AXIFE},
	{0, CORE_MMU_1, SUBSYS0_MMU1_OFFSET, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_MMU_1},
	{0, CORE_AXIFE_1, SUBSYS0_AXIFE1_OFFSET, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_AXIFE_1},
	{0, CORE_DEC400, SUBSYS0_DEC400_OFFSET, DEC400_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_DEC400},
	{0, CORE_L2CACHE, SUBSYS0_L2CACHE_OFFSET, L2CACHE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_L2CACHE},
	{0, CORE_UFBC, SUBSYS0_UFBC_OFFSET, UFBC_REGISTER_SIZE * 4, INT_PIN_SUBSYS_0_UFBC},
	{0, CORE_VCMD, SUBSYS0_VCMD_OFFSET, VCMD_REGISTER_SIZE * 4, -1},
#if SUBSYSTEM1
	{1, CORE_CUTREE, SUBSYS1_CUTREE_OFFSET, IM_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_CUTREE},
	{1, CORE_MMU, SUBSYS1_MMU_OFFSET, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_MMU},
	{1, CORE_AXIFE, SUBSYS1_AXIFE_OFFSET, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_AXIFE},
	{1, CORE_MMU_1, SUBSYS1_MMU1_OFFSET, MMU_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_MMU_1},
	{1, CORE_AXIFE_1, SUBSYS1_AXIFE1_OFFSET, AXIFE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_AXIFE_1},
	{1, CORE_DEC400, SUBSYS1_DEC400_OFFSET, DEC400_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_DEC400},
	{1, CORE_L2CACHE, SUBSYS1_L2CACHE_OFFSET, L2CACHE_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_L2CACHE},
	{1, CORE_UFBC, SUBSYS1_UFBC_OFFSET, UFBC_REGISTER_SIZE * 4, INT_PIN_SUBSYS_1_UFBC},
	{1, CORE_VCMD, SUBSYS1_VCMD_OFFSET, VCMD_REGISTER_SIZE * 4, -1},
#endif
};

#endif //__VCE_NORMAL_CFG_H__
