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

#ifndef ARBITER_SWHWREGISTERS_H
#define ARBITER_SWHWREGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "arbiter_drv.h"

#define ARB_HW_ID                       0x4152
#define HW_ID_3_0_0                     0x41523000

#define ARB_REGISTER_INT_STATUS_OFFSET   0x14
#define ARB_MASTER0_REG_OFFSET           0x40
#define ARB_MASTER_REG_OFFSET(id)        \
	(ARB_MASTER0_REG_OFFSET + (id) * MASTER_REG_NUM * 4)

#define ARB_IRQ_BUS_HACK                 (1 << 0)
#define ARB_IRQ_FE_TIMEOUT               (1 << 1)
#define ARB_IRQ_ERR_MASK                 \
    (ARB_IRQ_BUS_HACK |                  \
    ARB_IRQ_FE_TIMEOUT)

#define MASTER_PARAMS(w, u, o)            \
    (((w) & 0x1F)| (((u) & 0x1) << 9) | (((o) & 0x1) << 10))

/* HW Register field names */
typedef enum {
#include "arbiterregisterenum.h"
	ArbiterRegisterAmount
} regArbiterName;

/* HW Register field descriptions */
typedef struct {
	u32 name; /* Register name and index  */
	i32 base; /* Register base address  */
	u32 mask; /* Bitmask for this field */
	i32 lsb; /* LSB for this field [31..0] */
	i32 trace; /* Enable/disable writing in swreg_params.trc */
	i32 rw; /* 1=Read-only 2=Write-only 3=Read-Write */
	char *description; /* Field description */
} regArbiterField_s;

/* Flags for read-only, write-only and read-write */
#define RO 1
#define WO 2
#define RW 3

/* Description field only needed for system model build. */
#define ARBITERREG(name, base, mask, lsb, trace, rw, desc)                        \
	{                                                                      \
		name, base, mask, lsb, trace, rw, desc                         \
	}

extern const regArbiterField_s asicArbiterRegisterDesc[];

/**
 * @brief set a value into a defined register field
 */
static inline void arbiter_set_reg_mirror(u32 *reg_mirror,
						  regArbiterName name, u32 value)
{
	const regArbiterField_s *field;
	u32 regVal;

	field = &asicArbiterRegisterDesc[name];

	/* Clear previous value of field in register */
	regVal = reg_mirror[field->base / 4] & ~(field->mask);

	/* Put new value of field in register */
	reg_mirror[field->base / 4] =
		regVal | ((value << field->lsb) & field->mask);
}

static inline u32 arbiter_get_reg_mirror(u32 *reg_mirror,
						 regArbiterName name)
{
	const regArbiterField_s *field;
	u32 regVal;

	field = &asicArbiterRegisterDesc[name];

	regVal = reg_mirror[field->base / 4];
	regVal = (regVal & field->mask) >> field->lsb;

	return regVal;
}

u32 arbiter_read_reg(const void *hwregs, u32 offset);

void arbiter_write_reg(const void *hwregs, u32 offset, u32 val);

void arbiter_write_register_value(const void *hwregs, u32 *reg_mirror,
			       regArbiterName name, u32 value);

u32 arbiter_get_register_value(const void *hwregs, u32 *reg_mirror,
			    regArbiterName name);

#ifdef __cplusplus
}
#endif
#endif /* VCMD_SWHWREGISTERS_H */
