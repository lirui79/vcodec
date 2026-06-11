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

#ifndef _ARBITER_CFG_H_
#define _ARBITER_CFG_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned long long u64;
typedef u64 addr_t;

// arbiter 0
#define ARB0_BASE_ADDRESS             0x6A0000 //apb base, if 0xFFFF, indicate that it's not valid
#define ARB0_TIME_WINDOW_EXP          29 //arbiter timewindow_exe
#define ARB0_IRQ                      -1 // if not have interrupt env, set as a negative value and different with other arbiter

// arbiter 1
#define ARB1_BASE_ADDRESS             0xFFFF //apb base
#define ARB1_TIME_WINDOW_EXP          29
#define ARB1_IRQ                      -2

#define MASTER_ENABLE         0
#define MASTER_WEIGHT         7
#define MASTER_URGENT         0
#define MASTER_BW_OVERFLOW    0

enum sub_module {
	SUB_MOD_AXIFE,

	SUB_MOD_MAX
};

struct sub_module_config {
	u32 sub_mod;
	u32 reg_off;
	u32 io_size;
};

struct arbiter_config {
	addr_t arb_base_addr;
	u32 io_size;
	u32 time_window_exp;
	int arb_irq;
	struct sub_module_config sub_mod_cfg[SUB_MOD_MAX];
};

static struct arbiter_config arbiter_cfg[] = {
	// arbiter0
	{ ARB0_BASE_ADDRESS,
	ASIC_ARB_SWREG_AMOUNT * 4,
	ARB0_TIME_WINDOW_EXP,
	ARB0_IRQ,
	{ {SUB_MOD_AXIFE, 0x5000, 64 * 4} }},

	// arbiter1
	{ ARB1_BASE_ADDRESS,
	ASIC_ARB_SWREG_AMOUNT * 4,
	ARB1_TIME_WINDOW_EXP,
	ARB1_IRQ,
	{ {SUB_MOD_AXIFE, 0x5000, 64 * 4} }},

};
#ifdef __cplusplus
}
#endif
#endif // _ARBITER_CFG_H_
