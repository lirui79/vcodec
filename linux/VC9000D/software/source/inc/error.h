/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2014 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------*/

#ifndef ERROR_H
#define ERROR_H

#include <errno.h>
#include "basetype.h"

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)
#define ERR "Error: " __FILE__ ", line " TOSTRING(__LINE__) ": "
#define SYSERR "System error message"

void Error(i32 numArgs, ...);

#endif
