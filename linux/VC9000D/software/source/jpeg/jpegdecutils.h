/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef JPEGDECUTILS_H
#define JPEGDECUTILS_H

/*------------------------------------------------------------------------------
    1. Include headers
------------------------------------------------------------------------------*/
#include "jpegdeccontainer.h"
#include "basetype.h"

/*------------------------------------------------------------------------------
    2. Module defines
------------------------------------------------------------------------------*/
#define STRM_ERROR 0xFFFFFFFFU

#ifndef OK
#define OK 0
#endif
#ifndef NOK
#define NOK -1
#endif
#ifndef STATIC
#define STATIC static
#endif

/*------------------------------------------------------------------------------
    3. Data types
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Function prototypes
------------------------------------------------------------------------------*/

u32 JpegDecGet2Bytes(StreamStorage * stream);
u32 JpegDecGetByte(StreamStorage * stream);
u32 JpegDecShowBits(StreamStorage * stream);
u32 JpegDecFlushBits(StreamStorage * stream, u32 bits);
u32 JpegDecShowByte(StreamStorage * stream, i32 idx);
u32 JpegGet16u(StreamStorage* strm, u32 is_big_endian);
u32 JpegGet32u(StreamStorage* strm, u32 is_big_endian);
i32 JpegGet32s(StreamStorage* strm, u32 is_big_endian);
float JpegGetRationalValue(StreamStorage* strm, u32 is_big_endian);
float JpegGetSRationalValue(StreamStorage* strm, u32 is_big_endian);
Fraction JpegGetSepRationalValue(StreamStorage* strm, u32 is_big_endian);
u32 JpegJumpBytes(StreamStorage* strm, u32 bytes);
void JpegGet16uToString(char *str, u32 tag, StreamStorage* strm, bool is_big_endian);
void JpegGetStringToVersion(char *str, char* curr_pos, u32 tot_bytes);
#endif /* #ifdef MODULE_H */
