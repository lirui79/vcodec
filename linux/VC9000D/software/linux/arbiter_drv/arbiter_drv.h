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

#ifndef _ARBITER_DRV_H_
#define _ARBITER_DRV_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef u64 ptr_t;
typedef int    i32;

#define MAX_ARBITER_NUM              4  // maximum supported abiter num
#define MAX_MASTER_NUM               32 //HW maximum supported master num
#define ASIC_ARB_SWREG_AMOUNT        23 // for 4 masters
#define MASTER_REG_NUM               2
#define DISABLE        0
#define ENABLE         1

/* Use 'a' as magic number */
#define HANTRO_IOC_MAGIC                         'a'
#define HANTRO_IOC_MAXNR                         20
#define ARBITER_IOCH_ARB_NUM_GET                 _IOR(HANTRO_IOC_MAGIC, 10, unsigned int)
#define ARBITER_IOCH_ARB_PARAMS_GET              _IOWR(HANTRO_IOC_MAGIC, 11, struct exchange_common_params *)
#define ARBITER_IOCH_ARB_PARAMS_SET              _IOWR(HANTRO_IOC_MAGIC, 12, struct exchange_common_params *)
#define ARBITER_IOCH_MASTER_PARAMS_SET           _IOWR(HANTRO_IOC_MAGIC, 13, struct exchange_master_params *)
#define ARBITER_IOCH_MASTER_PARAMS_GET           _IOWR(HANTRO_IOC_MAGIC, 14, struct exchange_master_params *)
#define ARBITER_IOCH_MASTER_OFFLINE              _IOW(HANTRO_IOC_MAGIC, 15, unsigned int)
#define ARBITER_IOCH_CHECK_IRQ_STATUS            _IOWR(HANTRO_IOC_MAGIC, 16, unsigned int *)
#define ARBITER_IOCH_CHECK_IRQ_STATUS_POLLING    _IO(HANTRO_IOC_MAGIC, 17)


/* a ioctl struct for specify any master of any arbiter to offline */
struct io_arb_master_offline {
	// arbiter core id
	u32 arb_id;
	// master id
	u32 master_id;
};

struct io_arb_params {
	// arbiter enable
	u32 enable;
	// arbiter core id
	u32 arb_id;
	// bandwidth calculation time window for arbiter
	u32 time_window_exp;
	// the master number of HW supported
	u32 master_num;
};

struct io_master_params {
	// online master
	u32 enable;
	// arbiter core id
	u32 arb_id;
	// the master id
	u32 master_id;
	// if set 1, master's bandwidth can overflow
	u32 bw_overflow;
	// if set 1, master has a higher priority
	u32 urgent;
	/* for normal priority, it means requried bandwidth
	 * for urgent priority, it means urgent level
	 */
	u32 weight;
};

/* a struct for wait err irq  */
struct io_wait_irq {
	u32 irq_status[MAX_ARBITER_NUM]; /* store arbiter err irq status */
	u32 wait_timeout;                /* 0: keep waiting; other: after (wait_timeout)ms waiting, will timeout */
};

#ifdef __cplusplus
}
#endif
#endif // _ARBITER_DRV_H_