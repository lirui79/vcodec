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

#include "sw_util.h"  /* NEXT_MULTIPLE */
#include "decapicommon.h"
#include "file_sink.h"
#include "commonconfig.h"
#include "common_sink.h"

struct FileSink {
  u8* frame_pic;
  FILE* file[2*DEC_MAX_OUT_COUNT];
  char filename[2*DEC_MAX_OUT_COUNT][NAME_MAX];
};


const void* FilesinkOpen(const char** fname) {
  int i;
  struct FileSink* inst = DWLcalloc(1, sizeof(struct FileSink));
  if (inst == NULL) return NULL;
  for (i = 0; i < 2*DEC_MAX_OUT_COUNT; i++) {
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

void FilesinkClose(const void* inst) {
  struct FileSink* output = (struct FileSink*)inst;
  int i;
  for (i = 0; i < 2*DEC_MAX_OUT_COUNT; i++) {
    if (output->file[i] != NULL) {
      /* Close the file and if it is empty, remove it. */
      off_t file_size;
      fseeko(output->file[i], 0, SEEK_END);
      file_size = ftello(output->file[i]);
      fclose(output->file[i]);
      if (file_size == 0) {
        remove(output->filename[i]);
      }
    }
  }
  if (output->frame_pic != NULL) {
    free(output->frame_pic);
  }
  free(output);
}

/* Write @h lines of @bits_in_line bits to @file from @buf, @s is the stride between lines. */
void DumpBitsToFile(u8 *buf, u32 bits_in_line, u32 h, u32 s, FILE *file) {
  u32 nbytes = bits_in_line / 8;
  u32 last_bits = bits_in_line % 8;
  u8 last_byte;
  u32 i;

  for (i = 0; i < h; i++) {
    if (nbytes)
      fwrite(buf, 1, nbytes, file);
    if (last_bits) {
      last_byte = buf[nbytes] & ((1 << last_bits) - 1);
      fwrite(&last_byte, 1, 1, file);
    }
    buf += s;
  }
}

void FilesinkWritePic(const void* inst, struct DecPicture *pic, int index) {
  struct FileSink* output = (struct FileSink*)inst;
  FILE *file[2];
  u32 index2 = index + DEC_MAX_OUT_COUNT;

  if (IS_PIC_RFC(pic->picture_info.format))
    index2 = 1;

  file[0] = output->file[index];
  file[1] = output->file[index2];
  CommonWriteOnePic(file, OUT_YUV, pic, NULL);
}

void FilesinkWriteSinglePic(const void* inst, struct DecPicture *pic, int index) {
  static int frame_num = 0;
  char name[NAME_MAX];
  FILE* file[2] = { NULL, NULL };

  memset(name, 0, sizeof(name));
  frame_num++;
  sprintf(name, "out_%03d_%ux%u.yuv", frame_num,
           pic->sequence_info.pic_width, pic->sequence_info.pic_height);
  file[0] = fopen(name, "wb");

  if (IS_PIC_DEC400(pic->picture_info.format)) {
    sprintf(name, "out_%03d_%ux%u_dec400_table.bin", frame_num,
            pic->sequence_info.pic_width, pic->sequence_info.pic_height);
    file[1] = fopen(name, "wb");
  }
  else if (IS_PIC_RFC(pic->picture_info.format)) {
    sprintf(name, "out_%03d_%ux%u_rfc_table.bin", frame_num,
            pic->sequence_info.pic_width, pic->sequence_info.pic_height);
    file[1] = fopen(name, "wb");
  }

  if (file[0]) {
    CommonWriteOnePic(file, OUT_YUV, pic, NULL);
  }

  if (file[0])
    fclose(file[0]);
  if (file[1])
    fclose(file[1]);
}

#ifdef MODEL_SIMULATION
void DumpBitsToFBCcoreInput(u8 *buf, u32 bd, u32 w, u32 h, u32 s, u32 is_chroma, FILE *file) {

  u32 i;
  u8 value[32768] = {0};
  u32 align_height, align_width ;
  if(!is_chroma) {
    align_height = NEXT_MULTIPLE(h, alignheight);
    align_width = NEXT_MULTIPLE(w, alignwidth);
  } else {
    align_height = NEXT_MULTIPLE(h, alignheight / 2);
    align_width = NEXT_MULTIPLE(w, alignwidth / 2);
  }

  for (i = 0; i < align_height; i++) {
    if(i < h) {
      fwrite(buf, 1, w * bd, file);
      if (align_width > w){
        fwrite(value, 1, (align_width - w) * bd, file);
      }
    }
    else {
      fwrite(value, 1, align_width * bd, file);
    }
    buf += s;
  }
}
#endif
