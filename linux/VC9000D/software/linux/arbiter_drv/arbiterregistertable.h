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
	ARBITERREG(HWIF_ARBITER_HW_ID,                        0, 0xffff0000,       16,        0, RO, "ID of HW ASIIC(AR)"),
	ARBITERREG(HWIF_ARBITER_HW_VERSION,                   0, 0x0000ffff,        0,        0, RO, "Version of HW (3.0.0) [15:12]-Major [11:8]-Minor [7:0]-Build"),
	ARBITERREG(HWIF_ARBITER_HW_BUILDDATE,                 4, 0xffffffff,        0,        0, RO, "HW package generation date in BCD code;For example 0x20240105"),
	ARBITERREG(HWIF_ARBITER_HW_BUILDID,                   8, 0xffffffff,        0,        0, RO, "HW package generation ID"),
	ARBITERREG(HWIF_ARBITER_SERVED_BUS_IDLE,             12, 0x00000800,       11,        0, RO, "Debug signal to indicate external Ips in this Subsystem are in idle state."),
	ARBITERREG(HWIF_ARBITER_FSM_STATE,                   12, 0x00000700,        8,        0, RO, "For debug; The FSM of VCARB"),
	ARBITERREG(HWIF_ARBITER_HW_MASTERNUM,                12, 0x000000ff,        0,        0, RO, "Number of masters supported in this HW. Legal value: 2-32"),
	ARBITERREG(HWIF_ARBITER_RESET_EXTERNAL,              16, 0x001e0000,       17,        0, RW, "SW write 1 to this bit will reset external IPs (VCE VCDVCEJIM.) clear this bit 0 will release the reset signal accordingly."),
	ARBITERREG(HWIF_ARBITER_RESET_SELF,                  16, 0x00010000,       16,        0, RW, "SW write 1 to this bit will reset VCARB HW core logic"),
	ARBITERREG(HWIF_ARBITER_WINER_ID,                    16, 0x00003e00,        9,        0, RO, "Show the winer ID which is under service when (sw_arb_enable==1). SW can only write this field when (sw_arb_enable==0) for some debug purpose."),
	ARBITERREG(HWIF_ARBITER_WINER_EXIST,                 16, 0x00000100,        8,        0, RO, "Show whether a mater win the arbitration when (sw_arb_enable==1). SW can only write this field when (sw_arb_enable==0) for some debug purpose."),
	ARBITERREG(HWIF_ARBITER_TIME_WINDOW_EXP,             16, 0x000000f8,        3,        0, RW, "This is the bandwidth calculation time window for VCARB."),
	ARBITERREG(HWIF_ARBITER_BUS_HACK_CHECK_EN,           16, 0x00000004,        2,        0, RW, "Enable bit for checking the AXI bus status when received frame end signal. If the AXI bus is not idle (sw_served_bus_idle!=1) when received frame end signal that probably means the served master has been hacked and trying to break the bus protocal. so our HW will trigger sw_irq_bus_hack and clear sw_arb_enable to 0 and get into stall state (sw_fsm_state==STALL)."),
	ARBITERREG(HWIF_ARBITER_TIMEOUT_CHECK_EN,            16, 0x00000002,        1,        0, RW, "Enable bit for checking the served cycles of one master. If the total served cycles of one frame exceed the value of sw_timeout_cycles but the frame end signal has not been received that probably means the served master has been hacked or it has bug. so our HW will trigger sw_irq_fe_timeout and clear sw_arb_enable to 0 and get into stall state (sw_fsm_state==STALL)."),
	ARBITERREG(HWIF_ARBITER_ARB_ENABLE,                  16, 0x00000001,        0,        0, RW, "SW write 1 to enable VCARB and write 0 to disable VCARB HW."),
	ARBITERREG(HWIF_ARBITER_IRQ_BUS_HACK,                20, 0x00000002,        1,        0, RW, "Interrupt source which is triggered when a frame end signal is received but the AXI bus is not idle (sw_served_bus_idle!=1). Write 1 to clear this bit."),
	ARBITERREG(HWIF_ARBITER_IRQ_FE_TIMEOUT,              20, 0x00000001,        0,        0, RW, "interrupt source which is triggered by END command.Write 1 to clear this bit."),
	ARBITERREG(HWIF_ARBITER_IRQ_BUS_HACK_EN,             24, 0x00000002,        1,        0, RW, "interrupt enable for sw_irq_bus_hack;Turning off this switch only prevents interrupts from being sent to the CPU but won't impact the interrupt source generation"),
	ARBITERREG(HWIF_ARBITER_IRQ_FE_TIMEOUT_EN,           24, 0x00000001,        0,        0, RW, "interrupt enable for sw_irq_fe_timeout;Turning off this switch only prevents interrupts from being sent to the CPU but won't impact the interrupt source generation"),
	ARBITERREG(HWIF_ARBITER_TIMEOUT_CYCLES,              28, 0xffffffff,        0,        0, RW, "The timeout threshold value used when (sw_timeout_check_en==1)"),
	ARBITERREG(HWIF_ARBITER_MST0_BUS_IDLE,               64, 0x00100000,       20,        0, RO, "For debug; Whether this AXI/APB bus of VCMD0 is idle"),
	ARBITERREG(HWIF_ARBITER_MST0_GRP_INFO,               64, 0x000c0000,       18,        0, RO, "For debug; The group info of master0"),
	ARBITERREG(HWIF_ARBITER_MST0_ARB_ACK,                64, 0x00020000,       17,        0, RO, "For debug; The acknowlege signal to master0"),
	ARBITERREG(HWIF_ARBITER_MST0_ARB_REQ,                64, 0x00010000,       16,        0, RO, "For debug; The request signal from master0"),
	ARBITERREG(HWIF_ARBITER_MST0_SW_RESET,               64, 0x00000800,       11,        0, RW, "Set this bit to 1 will reset the dedicate IPs for master0 (e.g. VCMD0 MMU0) clear this bit 0 will release the reset signal accordingly."),
	ARBITERREG(HWIF_ARBITER_MST0_BW_OVERFLOW,            64, 0x00000400,       10,        0, RW, "Set this bit to 1 means mater0 can use more computing bandwidth comparing to the bandwith set by sw_mst0_weight when other master don't use the bandwidth. This register is only effective when (sw_mst0_urgent==0)"),
	ARBITERREG(HWIF_ARBITER_MST0_URGENT,                 64, 0x00000200,        9,        0, RW, "Set this bit to 1 means master0 has higher priority."),
	ARBITERREG(HWIF_ARBITER_MST0_ENABLE,                 64, 0x00000100,        8,        0, RW, "Once master0 get offline/online SW will change this bit accordingly."),
	ARBITERREG(HWIF_ARBITER_MST0_WEIGHT,                 64, 0x0000001f,        0,        0, RW, "* When in normal priority (sw_mst0_urgent==0) this register set required bandwidth of master0"),
	ARBITERREG(HWIF_ARBITER_MST0_SATISFACTION,           68, 0xffffffff,        0,        0, RO, "For debug; The satisfaction value of master0"),
	ARBITERREG(HWIF_ARBITER_MST1_,                      72, 0xffffffff,        0,        0, RW, "For master1 for the details please ref to sw_mst0_*"),
	ARBITERREG(HWIF_ARBITER_MST1_SATISFACTION,           76, 0xffffffff,        0,        0, RO, " "),
	ARBITERREG(HWIF_ARBITER_MST2_,                      80, 0xffffffff,        0,        0, RW, "For master2 for the details please ref to sw_mst0_*"),
	ARBITERREG(HWIF_ARBITER_MST2_SATISFACTION,           84, 0xffffffff,        0,        0, RO, " "),
	ARBITERREG(HWIF_ARBITER_MST3_,                      88, 0xffffffff,        0,        0, RW, "For master3 for the details please ref to sw_mst0_*"),
	ARBITERREG(HWIF_ARBITER_MST3_SATISFACTION,           92, 0xffffffff,        0,        0, RO, " "),
