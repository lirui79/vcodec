/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------*/

#ifndef ENCBASETYPE_H
#define ENCBASETYPE_H

#include <stdint.h>
#include <stdio.h>
#ifndef NDEBUG
#include <assert.h>
#endif
#include "enccommon.h"
#ifndef V60_MODEL
#include "av1/common/enums.h"
#include "av1enccommon.h"
#endif
#include "vp9enccommon.h"

//Added by HongyanWu, To print information for CABAC.RDOQ
#define RDOQ_HONGYWU(str)
#define VP9_CABAC_DEBUG_BITS(str)
#define VP9_CABAC_DEBUG_STXS(str)
#define SCABAC_HONGYWU(str)
//#define SCABAC_HONGYWU(str) printf str

#endif /* ENCBASETYPE_H */
