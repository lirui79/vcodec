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

#ifndef MPEG2STRMDEC_H_DEFINED
#define MPEG2STRMDEC_H_DEFINED

#include "mpeg2hwd_container.h"

/* StrmDec_Decode() function return values
 * MPEG2_RDY -> everything ok but no frames finished
 * MPEG2_PIC_RDY -> pic finished
 * MPEG2_PIC_RDY_BUF_NOT_EMPTY -> pic finished but stream buffer not empty
 * MPEG2_ERROR -> nothing finished, decoder not ready, cannot continue decoding
 *      before some headers received
 * MPEG2_ERROR_BUF_NOT_EMPTY -> same as above but stream buffer not empty
 * MPEG2_END_OF_STREAM -> nothing finished, no data left in stream
 * MPEG2_OUT_OF_BUFFER -> out of rlc buffer
 * MPEG2_HDRS_RDY -> either vol header decoded or short video source format
 *      determined
 * MPEG2_HDRS_RDY_BUF_NOT_EMPTY -> same as above but stream buffer not empty
 *
 * Bits in the output have following meaning:
 * 0    ready
 * 1    frame ready
 * 2    buffer not empty
 * 3    error
 * 4    end of stream
 * 5    out of buffer
 * 6    video object sequence end
 * 7    headers ready
 */
enum Mpeg2Result {
  MPEG2_RDY = 0x01,
  MPEG2_PIC_RDY = 0x03,
  MPEG2_PIC_RDY_BUF_NOT_EMPTY = 0x07,
  MPEG2_ERROR = 0x08,
  MPEG2_ERROR_BUF_NOT_EMPTY = 0x0C,
  MPEG2_END_OF_STREAM = 0x10,
  MPEG2_OUT_OF_BUFFER = 0x20,
  MPEG2_HDRS_RDY = 0x80,
  MPEG2_HDRS_RDY_BUF_NOT_EMPTY = 0x84,
  MPEG2_PIC_HDR_RDY = 0x100,
  MPEG2_PIC_HDR_RDY_ERROR = 0x108,
  MPEG2_PIC_SUPRISE_B = 0x1000
};

/* function prototypes */
enum Mpeg2Result mpeg2StrmDec_Decode(Mpeg2DecContainer * dec_cont);

#endif /* #ifndef MPEG2STRMDEC_H_DEFINED */
