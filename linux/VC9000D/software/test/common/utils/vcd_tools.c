/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#include "vcd_tools.h"

#define BIG_CODE_NUM 0xFFFFFFFFU
/* Decodes unsigned Exp-Golomb codeword. This is the same as code_num used in
 * other Exp-Golomb code mappings.
 *
 * Code num (i.e. the decoded symbol) is determined as
 *
 * code_num = 2^leading_zeros - 1 + SwGetBits(leading_zeros)
 *
 * Normal decoded symbols are in the range [0, 2^32 - 2]. Symbol
 * 2^32-1 is indicated by BIG_CODE_NUM with return value HANTRO_OK
 * while symbol 2^32  is indicated by BIG_CODE_NUM with return value
 * HANTRO_NOK.  These two symbols are special cases with code length
 * of 65, i.e.  32 '0' bits, a '1' bit, and either 0 or 1 represented
 * by 32 bits.
 *
 * Symbol 2^32 is out of unsigned 32-bit range but is needed for
 * DecodeExpGolombSigned to express value -2^31. */
u32 VcdExpGolombUnsigned(struct StrmData *stream, u32 *code_num) {

  u32 bits, num_zeros;

  ASSERT(stream);
  ASSERT(code_num);

  bits = SwShowBits(stream, 32);

  /* first bit is 1 -> code length 1 */
  if (bits >= 0x80000000) {
    if (SwFlushBits(stream, 1) == END_OF_STREAM) return (HANTRO_NOK);
    *code_num = 0;
    return (HANTRO_OK);
  }
  /* second bit is 1 -> code length 3 */
  else if (bits >= 0x40000000) {
    if (SwFlushBits(stream, 3) == END_OF_STREAM) return (HANTRO_NOK);
    *code_num = 1 + ((bits >> 29) & 0x1);
    return (HANTRO_OK);
  }
  /* third bit is 1 -> code length 5 */
  else if (bits >= 0x20000000) {
    if (SwFlushBits(stream, 5) == END_OF_STREAM) return (HANTRO_NOK);
    *code_num = 3 + ((bits >> 27) & 0x3);
    return (HANTRO_OK);
  }
  /* fourth bit is 1 -> code length 7 */
  else if (bits >= 0x10000000) {
    if (SwFlushBits(stream, 7) == END_OF_STREAM) return (HANTRO_NOK);
    *code_num = 7 + ((bits >> 25) & 0x7);
    return (HANTRO_OK);
  }
  /* other code lengths */
  else {
    num_zeros = 4 + SwCountLeadingZeros(bits, 28);

    /* all 32 bits are zero */
    if (num_zeros == 32) {
      *code_num = 0;
      if (SwFlushBits(stream, 32) == END_OF_STREAM) return (HANTRO_NOK);
      bits = SwGetBits(stream, 1);
      /* check 33rd bit, must be 1 */
      if (bits == 1) {
        /* cannot use SwGetBits, limited to 31 bits */
        bits = SwShowBits(stream, 32);
        if (SwFlushBits(stream, 32) == END_OF_STREAM) return (HANTRO_NOK);
        /* code num 2^32 - 1, needed for unsigned mapping */
        if (bits == 0) {
          *code_num = BIG_CODE_NUM;
          return (HANTRO_OK);
        }
        /* code num 2^32, needed for unsigned mapping
         * (results in -2^31) */
        else if (bits == 1) {
          *code_num = BIG_CODE_NUM;
          return (HANTRO_NOK);
        }
      }
      /* if more zeros than 32, it is an error */
      return (HANTRO_NOK);
    } else if (SwFlushBits(stream, num_zeros + 1) == END_OF_STREAM)
      return (HANTRO_NOK);

    bits = SwGetBits(stream, num_zeros);
    if (bits == END_OF_STREAM) return (HANTRO_NOK);

    *code_num = (1 << num_zeros) - 1 + bits;
  }

  return (HANTRO_OK);
}


void SwMatchOuputBufferId(const struct TOOL_PARAMS *tool_params, const struct DWLLinearMem *out_buf,
                          u32 *ext_id) {
  u32 i;
  const struct DWLLinearMem *ext_buffers = tool_params->ext_buffers;
  u32 max_buffers = *tool_params->max_buffers;

  /* match the ext buffer and pic buffers */
  for (i = 0; i < max_buffers; i++) {
    if (DWL_DEVMEM_IN_RANGE(ext_buffers[i], out_buf->bus_address)) {
      *ext_id = i;
      break;
    }
  }
  ASSERT(i < max_buffers);
}

void SetHostBase(struct DWLLinearMem* buf, u32* host_base, addr_t device_base) {
  if (buf->bus_address != 0)
    buf->virtual_address = (u32*)((u8*)host_base + (buf->bus_address - device_base));
}

void SwSetHostOutbaseAfterDma(u32* host_base, struct DecPicture* pic,
                              struct DWLLinearMem* buf) {
  if (host_base != NULL) {
    av_unused addr_t device_base = buf->bus_address;

    SetHostBase(&pic->luma, host_base, device_base);
    SetHostBase(&pic->chroma, host_base, device_base);
    SetHostBase(&pic->chroma_cr, host_base, device_base);
    SetHostBase(&pic->luma_table, host_base, device_base);
    SetHostBase(&pic->chroma_table, host_base, device_base);
  }
}

void SwClearHostOutbaseAfterWriteFile(u32* host_base, struct DecPicture* pic) {
  if (host_base != NULL) {
    pic->luma.virtual_address = NULL;
    pic->chroma.virtual_address = NULL;
    pic->chroma_cr.virtual_address = NULL;
    if (pic->luma_table.size != 0) {
      pic->luma_table.virtual_address = NULL;
    }
    if (pic->chroma_table.size != 0) {
      pic->chroma_table.virtual_address = NULL;
    }
  }
}