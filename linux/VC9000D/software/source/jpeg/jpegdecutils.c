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

#include "jpegdecutils.h"
#include "jpegdecmarkers.h"

/*------------------------------------------------------------------------------
    2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
    3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
    5. Functions
------------------------------------------------------------------------------*/

static u32 GetByte(u8 * stream, i32 idx, u8 *buffer, u32 buf_len) {
  i32 offset = (addr_t) stream - (addr_t) buffer;
  if (offset + idx < buf_len)
    return buffer[offset + idx];
  else
    return buffer[offset + idx - buf_len];
}

/*------------------------------------------------------------------------------

    Function name: JpegDecShowByte

        Functional description:
          Shows one byte (8 bits) from stream and returns the bits

          Note! This function does not skip the 0x00 byte if the previous
          byte value was 0xFF!!!

        Inputs:
          StreamStorage *stream   Pointer to structure

        Outputs:
          Returns 8 bit value if ok
          else returns 0

------------------------------------------------------------------------------*/
u32 JpegDecShowByte(StreamStorage * stream, i32 idx) {
  u8 *buffer = stream->p_start_of_buffer;
  u32 buf_len = stream->strm_buff_size;
  u8 *p_data = stream->p_curr_pos;
  u32 bytes;

  /* bytes left in buffer */
  bytes = (i32) (stream->stream_length - (stream->read_bits >> 3));
  if(!bytes || idx >= bytes)
    return (0);

  i32 offset = (addr_t) p_data - (addr_t) buffer;
  if (offset + idx < buf_len)
    return buffer[offset + idx];
  else
    return buffer[offset + idx - buf_len];
}


/*------------------------------------------------------------------------------

    Function name: JpegDecGetByte

        Functional description:
          Reads one byte (8 bits) from stream and returns the bits

          Note! This function does not skip the 0x00 byte if the previous
          byte value was 0xFF!!!

        Inputs:
          StreamStorage *stream   Pointer to structure

        Outputs:
          Returns 8 bit value if ok
          else returns STRM_ERROR (0xFFFFFFFF)

------------------------------------------------------------------------------*/
u32 JpegDecGetByte(StreamStorage * stream) {
  u32 tmp;
  u8 *p_data = stream->p_curr_pos;
  u32 bytes = 0;

  if((stream->read_bits + 8) > (8 * stream->stream_length))
    return (STRM_ERROR);

  tmp = p_data[bytes];
  bytes++;
  tmp = (tmp << 8) | GetByte(p_data, bytes,
      stream->p_start_of_buffer, stream->strm_buff_size);

  tmp = (tmp >> (8 - stream->bit_pos_in_byte)) & 0xFF;
  stream->read_bits += 8;

  stream->p_curr_pos += bytes;

  if (stream->read_bits > stream->stream_length * 8)
    return (STRM_ERROR);

  if ((addr_t)stream->p_curr_pos >=
     ((addr_t)stream->p_start_of_buffer + stream->strm_buff_size))
    stream->p_curr_pos -= stream->strm_buff_size;

  return (tmp);
}

/*------------------------------------------------------------------------------

    Function name: JpegDecGet2Bytes

        Functional description:
          Reads two bytes (16 bits) from stream and returns the bits

          Note! This function does not skip the 0x00 byte if the previous
          byte value was 0xFF!!!

        Inputs:
          StreamStorage *stream   Pointer to structure

        Outputs:
          Returns 16 bit value

------------------------------------------------------------------------------*/
u32 JpegDecGet2Bytes(StreamStorage * stream) {
  u32 tmp;
  u8 *p_data = stream->p_curr_pos;
  u32 bytes = 0;

  if((stream->read_bits + 16) > (8 * stream->stream_length))
    return (STRM_ERROR);

  tmp = GetByte(p_data, bytes,
      stream->p_start_of_buffer, stream->strm_buff_size);
  bytes++;
  tmp = (tmp << 8) | GetByte(p_data, bytes,
      stream->p_start_of_buffer, stream->strm_buff_size);

  bytes++;
  tmp = (tmp << 8) | GetByte(p_data, bytes,
      stream->p_start_of_buffer, stream->strm_buff_size);

  tmp = (tmp >> (8 - stream->bit_pos_in_byte)) & 0xFFFF;
  stream->read_bits += 16;

  stream->p_curr_pos += bytes;

  if (stream->read_bits > stream->stream_length * 8)
    return (STRM_ERROR);

  if ((addr_t)stream->p_curr_pos >=
     ((addr_t)stream->p_start_of_buffer + stream->strm_buff_size))
    stream->p_curr_pos -= stream->strm_buff_size;

  return (tmp);
}

/*------------------------------------------------------------------------------

    Function name: JpegDecShowBits

        Functional description:
          Reads 32 bits from stream and returns the bits, does not update
          stream pointers. If there are not enough bits in data buffer it
          reads the rest of the data buffer bits and fills the lsb of return
          value with zero bits.

          Note! This function will skip the byte valued 0x00 if the previous
          byte value was 0xFF!!!

        Inputs:
          StreamStorage *stream   Pointer to structure

        Outputs:
          Returns  32 bit value

------------------------------------------------------------------------------*/
u32 JpegDecShowBits(StreamStorage * stream) {
  i32 bits;
  u32 read_bits;
  u32 out = 0;
  u8 *p_data = stream->p_curr_pos;
  i32 bytes = 0;

  /* bits left in buffer */
  bits = (i32) (8 * stream->stream_length - stream->read_bits);
  if(!bits)
    return (0);

  read_bits = 0;
  do {
    if(stream->read_bits > 8) {
      /* FF 00 bytes in stream -> jump over 00 byte */
      if((GetByte(p_data, bytes - 1, stream->p_start_of_buffer,
          stream->strm_buff_size) == 0xFF) &&
         (GetByte(p_data, bytes, stream->p_start_of_buffer,
          stream->strm_buff_size) == 0x00)) {
        bytes++;
        bits -= 8;
      }
    }
    if(read_bits == 32 && stream->bit_pos_in_byte) {
      out <<= stream->bit_pos_in_byte;
      out |= GetByte(p_data, bytes, stream->p_start_of_buffer,
             stream->strm_buff_size) >>
             (8 - stream->bit_pos_in_byte);

      read_bits = 0;
      break;
    }
    out = (out << 8) | *p_data++;
    out = (out << 8) | GetByte(p_data, bytes,
        stream->p_start_of_buffer, stream->strm_buff_size);
    bytes++;

    read_bits += 8;
    bits -= 8;
  } while(read_bits < (32 + stream->bit_pos_in_byte) && bits > 0);

  if(bits <= 0 &&
      ((read_bits + stream->read_bits) >= (stream->stream_length * 8))) {
    /* not enough bits in stream, fill with zeros */
    out = (out << (32 - (read_bits - stream->bit_pos_in_byte)));
  }

  return (out);
}

/*------------------------------------------------------------------------------

    Function name: JpegDecFlushBits

        Functional description:
          Updates stream pointers, flushes bits from stream

          Note! This function will skip the byte valued 0x00 if the previous
          byte value was 0xFF!!!

        Inputs:
          StreamStorage *stream   Pointer to structure
          u32 bits                 Number of bits to be flushed

        Outputs:
          OK
          STRM_ERROR

------------------------------------------------------------------------------*/
u32 JpegDecFlushBits(StreamStorage * stream, u32 bits) {
  u32 tmp;
  u32 extra_bits = 0;
  u8 *p_data = stream->p_curr_pos;
  u32 bytes = 0;


  if((stream->read_bits + bits) > (8 * stream->stream_length)) {
    /* there are not so many bits left in buffer */
    /* stream pointers to the end of the stream  */
    /* and return value STRM_ERROR               */
    extra_bits = 8 * stream->stream_length - stream->read_bits;
    stream->read_bits += extra_bits;
    stream->bit_pos_in_byte = 0;
    stream->p_curr_pos += (extra_bits >> 3);
    return (STRM_ERROR);
  } else {
    tmp = 0;
    while(tmp < bits) {
      if(bits - tmp < 8) {
        if((8 - stream->bit_pos_in_byte) > (bits - tmp)) {
          /* inside one byte */
          stream->bit_pos_in_byte += bits - tmp;
          tmp = bits;
        } else {
          if (GetByte(p_data, bytes, stream->p_start_of_buffer,
              stream->strm_buff_size) == 0xFF &&
              GetByte(p_data, bytes + 1, stream->p_start_of_buffer,
              stream->strm_buff_size) == 0x00) {
            extra_bits += 8;
            bytes += 2;
          } else {
            bytes++;
          }
          tmp += 8 - stream->bit_pos_in_byte;
          stream->bit_pos_in_byte = 0;
          stream->bit_pos_in_byte = bits - tmp;
          tmp = bits;
        }
      } else {
        tmp += 8;
        if(stream->appn_flag) {
          bytes++;
        } else {
          if (GetByte(p_data, bytes, stream->p_start_of_buffer,
              stream->strm_buff_size) == 0xFF &&
              GetByte(p_data, bytes + 1, stream->p_start_of_buffer,
              stream->strm_buff_size) == 0x00) {
            extra_bits += 8;
            bytes += 2;
          } else {
            bytes++;
          }
        }
      }
    }
    /* update stream pointers */
    stream->read_bits += bits + extra_bits;
    stream->p_curr_pos += bytes;

    if (stream->read_bits > stream->stream_length * 8)
      return (STRM_ERROR);

    if ((addr_t)stream->p_curr_pos >=
       ((addr_t)stream->p_start_of_buffer + stream->strm_buff_size))
      stream->p_curr_pos -= stream->strm_buff_size;

    return (OK);
  }
}

u32 JpegGet16u(StreamStorage* strm, u32 is_big_endian) {
  u8 data[2];
  data[0] = JpegDecGetByte(strm);
  data[1] = JpegDecGetByte(strm);
  if (is_big_endian)
    return (data[0]<<8 | data[1]);
  return (data[1]<<8 | data[0]);
}

u32 JpegGet32u(StreamStorage* strm, u32 is_big_endian) {
  u8 data[4];
  data[0] = JpegDecGetByte(strm);
  data[1] = JpegDecGetByte(strm);
  data[2] = JpegDecGetByte(strm);
  data[3] = JpegDecGetByte(strm);
  if (is_big_endian)
    return ((data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3]);
  return ((data[3]<<24) | (data[2]<<16) | (data[1]<<8) | data[0]);
}

i32 JpegGet32s(StreamStorage* strm, u32 is_big_endian) {
  i8 data[4];
  data[0] = (i8)JpegDecGetByte(strm);
  data[1] = (i8)JpegDecGetByte(strm);
  data[2] = (i8)JpegDecGetByte(strm);
  data[3] = (i8)JpegDecGetByte(strm);
  if (is_big_endian)
    return ((data[0]<<24) | (data[1]<<16) | (data[2]<<8) | data[3]);
  return ((data[3]<<24) | (data[2]<<16) | (data[1]<<8) | data[0]);
}

float JpegGetRationalValue(StreamStorage* strm, u32 is_big_endian) {
  u32 numerator, denominator;

  numerator = JpegGet32u(strm, is_big_endian);
  denominator = JpegGet32u(strm, is_big_endian);
  if (denominator != 0)
    return (float)numerator / denominator;
  return 0;
}

float JpegGetSRationalValue(StreamStorage* strm, u32 is_big_endian) {
  i32 numerator, denominator;

  numerator = JpegGet32s(strm, is_big_endian);
  denominator = JpegGet32s(strm, is_big_endian);
  if (denominator != 0)
    return (float)numerator / denominator;
  return 0;
}

Fraction JpegGetSepRationalValue(StreamStorage* strm, u32 is_big_endian) {
  Fraction num;
  DWLmemset(&num, 0, sizeof(Fraction));
  num.numerator = JpegGet32u(strm, is_big_endian);
  num.denominator = JpegGet32u(strm, is_big_endian);
  return num;
}

u32 JpegJumpBytes(StreamStorage* strm, u32 bytes) {
  strm->read_bits += (bytes * 8);
  strm->p_curr_pos += bytes;

  if (strm->read_bits > strm->stream_length * 8)
    return (HANTRO_NOK);

  if (strm->p_curr_pos >=
      (strm->p_start_of_buffer + strm->strm_buff_size))
    strm->p_curr_pos -= strm->strm_buff_size;
  return HANTRO_OK;
}

void JpegGet16uToString(char *str, u32 tag, StreamStorage* strm, bool is_big_endian) {
  u32 value = JpegGet16u(strm, is_big_endian);
  static const ExifMetaDataDescriptor *metadata_name = NULL;
  u32 arr_num = 0;

  if (tag == TAG_EXPOSURE_PROGRAM) {
    metadata_name = ExposureProgram;
    arr_num = sizeof(ExposureProgram)/sizeof(ExposureProgram[0]);
  } else if (tag == TAG_COLORSPACE) {
    metadata_name = ColorSpace;
    arr_num = sizeof(ColorSpace)/sizeof(ColorSpace[0]);
  } else if(tag == TAG_FLASH) {
    metadata_name = FlashWay;
    arr_num = sizeof(FlashWay)/sizeof(FlashWay[0]);
  } else if (tag == TAG_METERING_MODE) {
    metadata_name = MeteringMode;
    arr_num = sizeof(MeteringMode)/sizeof(MeteringMode[0]);
  } else if (tag == TAG_LIGHTSOURCE) {
    metadata_name = LightSoure;
    arr_num = sizeof(LightSoure)/sizeof(LightSoure[0]);
  } else if (tag == TAG_COMPRESSION) {
    metadata_name = Compression;
    arr_num = sizeof(Compression)/sizeof(Compression[0]);
  } else if (tag == TAG_RESOLUTION_UNIT) {
    metadata_name = Resolution_unit;
    arr_num = sizeof(Resolution_unit)/sizeof(Resolution_unit[0]);
  }
  if (!metadata_name || !arr_num) return;

  do{
    if (value == metadata_name->value) {
      strcpy(str, metadata_name->name);
      break;
    }
    metadata_name++;
    arr_num--;
  }while(arr_num > 0);

}

void JpegGetStringToVersion(char *str, char* curr_pos, u32 tot_bytes) {
  char data[32];
  int num;

  ASSERT(tot_bytes<=4);
  strncpy(data, curr_pos, tot_bytes);
  data[tot_bytes] = '\0';
  num = atoi(data);
  sprintf(data, "V%d.%d", num/100, num%100);
  strcat(str, data);
}
