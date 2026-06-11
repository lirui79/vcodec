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

#include "md5_sink.h"
#include "md5.h"
#include "sw_util.h"  /* NEXT_MULTIPLE */
#include "common_sink.h"

struct MD5Sink {
  FILE* file[2 * DEC_MAX_OUT_COUNT];
  char filename[2 * DEC_MAX_OUT_COUNT][NAME_MAX];
  struct MD5Context ctx[2 * DEC_MAX_OUT_COUNT];
};

const void* Md5sinkOpen(const char** fname) {
  int i;
  struct MD5Sink* inst = DWLcalloc(1, sizeof(struct MD5Sink));
  if (inst == NULL) return NULL;
  for (i = 0; i < 2 * DEC_MAX_OUT_COUNT; i++) {
    if (fname[i] == NULL)
      continue;
    inst->file[i] = fopen(fname[i], "wb");
    if (inst->file[i] == NULL) {
      free(inst);
      return NULL;
    }
    strncpy(inst->filename[i], fname[i], NAME_MAX);
    MD5Init(&inst->ctx[i]);
  }
  return inst;
}

void Md5sinkClose(const void* inst) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  int j;
  for (j = 0; j < 2 * DEC_MAX_OUT_COUNT; j++) {
    if (md5sink->file[j] != NULL) {
      u32 file_size = 0;
      unsigned char digest[16] = {0};
      if (md5sink->ctx[j].bits[0] != 0) {
        MD5Final(digest, &md5sink->ctx[j]);
        for (int i = 0; i < sizeof digest; i++) {
          fprintf(md5sink->file[j], "%02x", digest[i]);
        }
        fprintf(md5sink->file[j], "  %s\n", md5sink->filename[j]);
        fflush(md5sink->file[j]);
      }
      /* Close the file and if it is empty, remove it. */
      fseek(md5sink->file[j], 0, SEEK_END);
      file_size = ftell(md5sink->file[j]);
      fclose(md5sink->file[j]);
      if (file_size == 0) remove(md5sink->filename[j]);
    }
  }
  free(md5sink);
}

/* Update @h lines of @bits_in_line bits to md5 context @ctx from @buf, @s is the stride between lines. */
void Md5UpdateBits(u8 *buf, u32 bits_in_line, u32 h, u32 s, struct MD5Context* ctx) {
  u32 nbytes = bits_in_line / 8;
  u32 last_bits = bits_in_line % 8;
  u8 last_byte;
  u32 i;

  for (i = 0; i < h; i++) {
    if (nbytes)
      MD5Update(ctx, buf, nbytes);
    if (last_bits) {
      last_byte = buf[nbytes] & ((1 << last_bits) - 1);
      MD5Update(ctx, &last_byte, 1);
    }
    buf += s;
  }
}

void Md5sinkWritePic(const void* inst, struct DecPicture *pic, int index) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  struct MD5Context* ctx = &md5sink->ctx[index];
  FILE *file[2];
  u32 index2 = index + DEC_MAX_OUT_COUNT;

  if (IS_PIC_RFC(pic->picture_info.format))
    index2 = 1;
  file[0] = md5sink->file[index];
  file[1] = md5sink->file[index2];
  CommonWriteOnePic(file, OUT_MD5, pic, ctx);
}

const void* md5perpicsink_open(const char** fname) {
  struct MD5Sink* inst = DWLcalloc(1, sizeof(struct MD5Sink));
  int i;
  if (inst == NULL) return NULL;
  for (i = 0; i < 2 * DEC_MAX_OUT_COUNT; i++) {
    if (fname[i] == NULL)
      continue;
    inst->file[i] = fopen(fname[i], "wb");
    if (inst->file[i] == NULL) {
      free(inst);
      return NULL;
    }
    strncpy(inst->filename[i], fname[i], NAME_MAX);
  }
  return inst;
}

void md5perpicsink_close(const void* inst) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  int i;
  for (i = 0; i < 2 * DEC_MAX_OUT_COUNT; i++) {
    if (md5sink->file[i] != NULL) {
      /* Close the file and if it is empty, remove it. */
      u32 file_size;
      fseek(md5sink->file[i], 0, SEEK_END);
      file_size = ftell(md5sink->file[i]);
      fclose(md5sink->file[i]);
      if (file_size == 0) remove(md5sink->filename[i]);
    }
  }
  free(md5sink);
}
void md5perpicsink_write_pic(const void* inst, struct DecPicture *pic, int index) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  FILE *file[2];
  struct MD5Context ctx;
  unsigned char digest[16];
  u32 index2 = index + DEC_MAX_OUT_COUNT;

  if (IS_PIC_RFC(pic->picture_info.format))
    index2 = 1;
  file[0] = md5sink->file[index];
  file[1] = md5sink->file[index2];
  MD5Init(&ctx);
  CommonWriteOnePic(file, OUT_MD5, pic, &ctx);
  MD5Final(digest, &ctx);
  for (int i = 0; i < sizeof(digest); i++) {
    fprintf(file[0], "%02X", digest[i]);
  }
  fprintf(file[0], "\n");
  fflush(file[0]);
}

void md5perpicsink_write_pic_vtm(const void* inst, struct DecPicture *pic, int index) {
  struct MD5Sink* md5sink = (struct MD5Sink*)inst;
  struct MD5Context ctx;
  unsigned char digest[16];

  u32 w;   /* real bits in a line */
  u32 h;   /* real lines */
  u32 s;   /* stride of a line in bytes */
  u32 bd = 0;
  u8* p = (u8*)pic->luma.virtual_address;

  MD5Init(&ctx);
  /*YCbCr 400/420/422 */
  if (IS_PIC_8BIT(pic->picture_info.format))
    bd = 8;
  else if (IS_PIC_10BIT(pic->picture_info.format))
    bd = 10;
  else if (IS_PIC_16BIT(pic->picture_info.format))
    bd = 16;

/* 32 bits / 3 pixels + 10 bits/pixel) */
#define LINE_BITS_1010(pixels) ((pixels) / 3 * 32 + ((pixels) % 3) * 10)

  /* luma */
  w = pic->pic_width * bd;
  if (IS_PIC_1010(pic->picture_info.format))
    w = LINE_BITS_1010(pic->pic_width);
  h = pic->pic_height;
  s = pic->pic_stride;
  p = (u8 *)pic->luma.virtual_address;
  Md5UpdateBits(p, w, h, s, &ctx);
  MD5Final(digest, &ctx);
  for (int i = 0; i < sizeof(digest); i++) {
    fprintf(md5sink->file[index], "%02x", digest[i]);
  }
  fflush(md5sink->file[index]);

  /* chroma */
  if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
    goto FLUSH_MD5;
  }

  if (IS_PIC_PLANAR(pic->picture_info.format)) {
    if (IS_PIC_1010(pic->picture_info.format))
      w = LINE_BITS_1010(pic->pic_width/2);
    else
      w = pic->pic_width / 2 * bd;
  } else {
    if (IS_PIC_1010(pic->picture_info.format))
      w = LINE_BITS_1010(pic->pic_width);
    else
      w = pic->pic_width * bd;
  }
  if (IS_PIC_YCbCr422(pic->picture_info.format))
    h = pic->pic_height;
  else
    h = pic->pic_height / 2;
  MD5Init(&ctx);
  s = pic->pic_stride_ch;
  p = (u8 *)pic->chroma.virtual_address;
  Md5UpdateBits(p, w, h, s, &ctx);
  MD5Final(digest, &ctx);
  fprintf(md5sink->file[index], ",");
  for (int i = 0; i < sizeof(digest); i++) {
    fprintf(md5sink->file[index], "%02x", digest[i]);
  }
  fflush(md5sink->file[index]);

  MD5Init(&ctx);
  p = (u8 *)pic->chroma.virtual_address + NEXT_MULTIPLE(s * h, PLANE_ALIGNMENT);
  if (IS_PIC_PLANAR(pic->picture_info.format)) {
    Md5UpdateBits(p, w, h, s, &ctx);
    MD5Final(digest, &ctx);
    fprintf(md5sink->file[index], ",");
    for (int i = 0; i < sizeof(digest); i++) {
      fprintf(md5sink->file[index], "%02x", digest[i]);
    }
    fflush(md5sink->file[index]);
  }

FLUSH_MD5:
  fprintf(md5sink->file[index], "\n");
}
