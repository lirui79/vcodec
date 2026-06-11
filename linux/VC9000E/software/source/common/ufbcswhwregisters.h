/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--                   (C) COPYRIGHT 2014 VERISILICON                           --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#ifndef UFBC_SWHWREGISTERS_H
#define UFBC_SWHWREGISTERS_H

#include "base_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PDEBUG(fmt, args) fprintf(stderr, "\n") /* not debugging: nothing */

/* HW Register field names */
#define UFBCREG(name, ...) name,
typedef enum {
	#include "ufbcregistertable.h"
} UfbcAfbcRegName;

typedef enum {
	#include "ufbcregistertable_v4.h"
} UfbcDec400RegName;

typedef enum {
	#include "ufbcregistertable_v2.h"
} UfbcPvricRegName;

#undef UFBCREG
/* Description field only needed for system model build. */
#ifdef TEST_DATA
#define UFBCREG(name, base, mask, lsb, trace, rw, desc)                        \
	{                                                                      \
		name, base, mask, lsb, trace, rw, desc                         \
	},
#else
#define UFBCREG(name, base, mask, lsb, trace, rw, desc)                        \
	{                                                                      \
		name, base, mask, lsb, trace, rw, ""                           \
	},
#endif

/* Flags for read-only, write-only and read-write */
#define RO 1
#define WO 2
#define RW 3

/* HW Register field descriptions */
typedef struct {
	u32 name; /* Register name and index  */
	i32 base; /* Register base address  */
	u32 mask; /* Bitmask for this field */
	i32 lsb; /* LSB for this field [31..0] */
	i32 trace; /* Enable/disable writing in swreg_params.trc */
	i32 rw; /* 1=Read-only 2=Write-only 3=Read-Write */
	char *description; /* Field description */
} regUFBCField_s;

const regUFBCField_s asicUFBCAfbcRegisterDesc[] = {
#include "ufbcregistertable.h"
};

const regUFBCField_s asicUFBCDec400RegisterDesc[] = {
#include "ufbcregistertable_v4.h"
};

const regUFBCField_s asicUFBCPvricRegisterDesc[] = {
#include "ufbcregistertable_v2.h"
};

/*------------------------------------------------------------------------------
 *
 *   EncAsicSetRegisterValue
 *
 *   Set a value into a defined register field
 *
 *------------------------------------------------------------------------------
 */
static inline void UFBC_set_afbc_mirror(u32 *reg_mirror,
						  UfbcAfbcRegName name, u32 value)
{
	const regUFBCField_s *field;
	u32 regVal;

	field = &asicUFBCAfbcRegisterDesc[name];

	/* Clear previous value of field in register */
	regVal = reg_mirror[field->base / 4] & ~(field->mask);

	/* Put new value of field in register */
	reg_mirror[field->base / 4] =
		regVal | ((value << field->lsb) & field->mask);
}

static inline void UFBC_set_dec400_mirror(u32 *reg_mirror,
						  UfbcDec400RegName name, u32 value)
{
	const regUFBCField_s *field;
	u32 regVal;

	field = &asicUFBCDec400RegisterDesc[name];

	/* Clear previous value of field in register */
	regVal = reg_mirror[field->base / 4] & ~(field->mask);

	/* Put new value of field in register */
	reg_mirror[field->base / 4] =
		regVal | ((value << field->lsb) & field->mask);
}

static inline void UFBC_set_pvric_mirror(u32 *reg_mirror,
						  UfbcPvricRegName name, u32 value)
{
	const regUFBCField_s *field;
	u32 regVal;

	field = &asicUFBCPvricRegisterDesc[name];

	/* Clear previous value of field in register */
	regVal = reg_mirror[field->base / 4] & ~(field->mask);

	/* Put new value of field in register */
	reg_mirror[field->base / 4] =
		regVal | ((value << field->lsb) & field->mask);
}
#ifdef __cplusplus
}
#endif

#endif

