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

#include "mpeg2hwd_strm.h"
#include "mpeg2hwd_utils.h"
#include "mpeg2hwd_headers.h"
#include "mpeg2hwd_debug.h"
#include "dec_log.h"
/*------------------------------------------------------------------------------
    2. External identifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

enum {
  CONTINUE
};

/*------------------------------------------------------------------------------
    4. Module indentifiers
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

   5.1  Function name: mpeg2StrmDec_Decode

        Purpose: Decode MPEG2 stream. Continues decoding until END_OF_STREAM
        encountered or whole frame decoded. Returns after decoding of sequence layer header.

        Input:
            Pointer to Mpeg2DecContainer structure
                -uses and updates StrmStorage
                -uses and updates StrmDesc
                -uses and updates FrameDesc
                -uses Hdrs
                -uses MbSetDesc

        Output:
            MPEG2_RDY if everything was ok but no FRAME finished
            MPEG2_HDR_RDY if headers decoded
            MPEG2_HDR_RDY_BUF_NOT_EMPTY if headers decoded but buffer not empty
            MPEG2_PIC_RDY if whole FRAME decoded
            MPEG2_RDY_BUF_NOT_EMPTY if whole FRAME decoded but buffer not empty
            MPEG2_END_OF_STREAM if eos encountered while decoding
            MPEG2_ERROR if such an error encountered that recovery needs initial
                      headers

------------------------------------------------------------------------------*/
enum Mpeg2Result mpeg2StrmDec_Decode(Mpeg2DecContainer * dec_cont) {
  u32 status;
  u32 start_code;

  STREAMTRACE_I("%s","Entry StrmDec_Decode\n");

  status = HANTRO_OK;

  /* keep decoding till something ready or something wrong */
  do {
    start_code = mpeg2StrmDec_NextStartCode(dec_cont);

    /* parse headers */
    switch (start_code) {
      case SC_SEQUENCE: {
        /* Sequence header */
        status = mpeg2StrmDec_DecodeSequenceHeader(dec_cont);
        dec_cont->StrmStorage.valid_sequence = status == HANTRO_OK;
        if( dec_cont->StrmStorage.new_headers_change_resolution)
          return MPEG2_END_OF_STREAM;
        break;
      }
      case SC_GROUP: {
        /* GOP header */
        status = mpeg2StrmDec_DecodeGOPHeader(dec_cont);
        break;
      }
      case SC_EXTENSION: {
        /* Extension headers */
        status = mpeg2StrmDec_DecodeExtensionHeader(dec_cont);
        if(status == MPEG2_PIC_HDR_RDY_ERROR)
          return MPEG2_PIC_HDR_RDY_ERROR;
        break;
      }
      case SC_PICTURE: {
        /* Picture header */
        /* decoder still in "initialization" phase and sequence headers
        * successfully decoded -> set to normal state */
        if(dec_cont->StrmStorage.strm_dec_ready == FALSE &&
            dec_cont->StrmStorage.valid_sequence) {
          dec_cont->StrmStorage.strm_dec_ready = TRUE;
          dec_cont->StrmDesc.strm_buff_read_bits -= 32;
          dec_cont->StrmDesc.strm_curr_pos -= 4;
          return (MPEG2_HDRS_RDY);
        } else if(dec_cont->StrmStorage.strm_dec_ready) {
          status = mpeg2StrmDec_DecodePictureHeader(dec_cont);
          if(status != HANTRO_OK)
            return (MPEG2_PIC_HDR_RDY_ERROR);
          dec_cont->StrmStorage.valid_pic_header = 1;

          if(dec_cont->Hdrs.low_delay &&
              dec_cont->Hdrs.picture_coding_type == BFRAME) {
            return (MPEG2_PIC_SUPRISE_B);
          }
        }
        break;
      }
      case SC_SLICE: {
        /* start decoding picture data (HW) if decoder is in normal
        * decoding state and picture headers have been successfully
        * decoded */
        if(dec_cont->StrmStorage.strm_dec_ready == TRUE &&
            dec_cont->StrmStorage.valid_pic_header &&
            (!dec_cont->Hdrs.mpeg2_stream ||
            dec_cont->StrmStorage.valid_pic_ext_header)) {
          /* handle stream positions and return */
          dec_cont->StrmDesc.strm_buff_read_bits -= 32;
          dec_cont->StrmDesc.strm_curr_pos -= 4;
          return (MPEG2_PIC_HDR_RDY);
        }
        break;
      }
      case END_OF_STREAM: {
        return (MPEG2_END_OF_STREAM);
      }
      default:
        break;
    }
  }
  while(1); /*lint -e(506) */

  /* execution never reaches this point (hope so) */
  return (MPEG2_END_OF_STREAM); /*lint -e(527) */
  /*lint -restore */
}
