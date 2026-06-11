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
 ********************************************************************************
 */

/* Register interface based on the document version v3.0.0 */
	HWIF_ARBITER_HW_ID,
	HWIF_ARBITER_HW_VERSION,
	HWIF_ARBITER_HW_BUILDDATE,
	HWIF_ARBITER_HW_BUILDID,
	HWIF_ARBITER_SERVED_BUS_IDLE,
	HWIF_ARBITER_FSM_STATE,
	HWIF_ARBITER_HW_MASTERNUM,
	HWIF_ARBITER_RESET_EXTERNAL,
	HWIF_ARBITER_RESET_SELF,
	HWIF_ARBITER_WINER_ID,
	HWIF_ARBITER_WINER_EXIST,
	HWIF_ARBITER_TIME_WINDOW_EXP,
	HWIF_ARBITER_BUS_HACK_CHECK_EN,
	HWIF_ARBITER_TIMEOUT_CHECK_EN,
	HWIF_ARBITER_ARB_ENABLE,
	HWIF_ARBITER_IRQ_BUS_HACK,
	HWIF_ARBITER_IRQ_FE_TIMEOUT,
	HWIF_ARBITER_IRQ_BUS_HACK_EN,
	HWIF_ARBITER_IRQ_FE_TIMEOUT_EN,
	HWIF_ARBITER_TIMEOUT_CYCLES,
	HWIF_ARBITER_MST0_BUS_IDLE,
	HWIF_ARBITER_MST0_GRP_INFO,
	HWIF_ARBITER_MST0_ARB_ACK,
	HWIF_ARBITER_MST0_ARB_REQ,
	HWIF_ARBITER_MST0_SW_RESET,
	HWIF_ARBITER_MST0_BW_OVERFLOW,
	HWIF_ARBITER_MST0_URGENT,
	HWIF_ARBITER_MST0_ENABLE,
	HWIF_ARBITER_MST0_WEIGHT,
	HWIF_ARBITER_MST0_SATISFACTION,
	HWIF_ARBITER_MST1_,
	HWIF_ARBITER_MST1_SATISFACTION,
	HWIF_ARBITER_MST2_,
	HWIF_ARBITER_MST2_SATISFACTION,
	HWIF_ARBITER_MST3_,
	HWIF_ARBITER_MST3_SATISFACTION,
