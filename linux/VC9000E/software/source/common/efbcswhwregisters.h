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

#ifndef EFBC_SWHWREGISTERS_H
#define EFBC_SWHWREGISTERS_H

#include "base_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PDEBUG(fmt, args) fprintf(stderr, "\n") /* not debugging: nothing */
/* HW Register field names */
typedef enum {
#include "efbcregisterenum.h"
} regEFBCName;
#define EFBC_REG_AMOUNT 10
/* HW Register field descriptions */
typedef struct {
	u32 name; /* Register name and index  */
	i32 base; /* Register base address  */
	u32 mask; /* Bitmask for this field */
	i32 lsb; /* LSB for this field [31..0] */
	i32 trace; /* Enable/disable writing in swreg_params.trc */
	i32 rw; /* 1=Read-only 2=Write-only 3=Read-Write */
	char *description; /* Field description */
} regEFBCField_s;

/* Description field only needed for system model build. */
#ifdef TEST_DATA
#define EFBCREG(name, base, mask, lsb, trace, rw, desc)                        \
	{                                                                      \
		name, base, mask, lsb, trace, rw, desc                         \
	}
#else
#define EFBCREG(name, base, mask, lsb, trace, rw, desc)                        \
	{                                                                      \
		name, base, mask, lsb, trace, rw, ""                           \
	}
#endif

const regEFBCField_s asicEFBCRegisterDesc[] = {
#include "efbcregistertable.h"
};


/*------------------------------------------------------------------------------
 *
 *   EncAsicSetRegisterValue
 *
 *   Set a value into a defined register field
 *
 *------------------------------------------------------------------------------
 */
static inline void EFBC_set_register_mirror_value(u32 *reg_mirror,
						  regEFBCName name, u32 value)
{
	const regEFBCField_s *field;
	u32 regVal;

	field = &asicEFBCRegisterDesc[name];

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
