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

#include "vpxfilereader.h"

#include "dwl.h"
#include "ivf.h"
#ifdef WEBM_ENABLED
#include <stdarg.h>
#include "nestegg/nestegg.h"
#endif /* WEBM_ENABLED */

/* Module defines */
#define VP8_FOURCC (0x30385056)
#define VP9_FOURCC (0x30395056)
#define AV1_FOURCC (0x31305641)

#define HANTRO_NOK 1
#define HANTRO_OK  0
#define DEC_X170_MAX_STREAM ((1 << 30) - 1)

/* Module datatypes */
enum RdrFileFormat {
  FF_VP7,
  FF_VP8,
  FF_VP9,
  FF_AV1,
  FF_AV1_ANNEXB,
  FF_AV1_OBU,
  FF_WEBP,
  FF_WEBM,
  FF_NONE
};

enum ObuType {
  OBU_SEQUENCE_HEADER = 1,
  OBU_TEMPORAL_DELIMITER = 2,
  OBU_FRAME_HEADER = 3,
  OBU_TILE_GROUP = 4,
  OBU_METADATA = 5,
  OBU_FRAME = 6,
  OBU_REDUNDANT_FRAME_HEADER = 7,
  OBU_PADDING = 15,
};

typedef struct {
  int type;
  int header_size;
  int total_size;
  int payload_size;
  int has_extension;
  int has_size_field;
  int temporal_layer_id;
  int spatial_layer_id;
} obuHeader_t;


#ifdef WEBM_ENABLED
struct InputCtx {
  nestegg* nestegg_ctx;
  nestegg_packet* pkt;
  unsigned int chunk;
  unsigned int chunks;
  unsigned int video_track;
};
#endif /* WEBM_ENABLED */

struct FfReader {
  enum RdrFileFormat format;
  i32 bitstream_format;
  u32 ivf_headers_read;
  FILE* file;
  FILE* file_index_fp;
#ifdef WEBM_ENABLED
  struct InputCtx input_ctx;
#endif /* WEBM_ENABLED */
};

/* Module local functions. */
static i32 ReadIvfFileHeader(FILE* fin);
static i32 ReadIvfFrameHeader(FILE* fin, u32* frame_size);
static i32 FfCheckFormat(struct FfReader* inst);
#ifdef WEBM_ENABLED
static int FileIsWebm(struct InputCtx* input, FILE* infile,
                      unsigned int* fourcc, unsigned int* width,
                      unsigned int* height, unsigned int* fps_den,
                      unsigned int* fps_num);
#endif /* WEBM_ENABLED */

int leb128file(FILE *fp) {
  int s = 0;
  u8 b;
  for (int i = 0; i < 8; i++) {
    int count = fread(&b, sizeof(u8), 1, fp);
    if (count != 1) return -1;
    s |= ((u64)(b&0x7f)) << (i * 7);
    if (!(b & 0x80))
      break;
  }
  return s;
}

#if 0
int leb128(const u8 *p, int *len);
int ReadObuHeader(const u8* data_start, const u8* data_end, obuHeader_t* hdr,
                  bool annexb, int annexb_size, bool size_check);

#else
static int leb128(const u8 *p, int *len) {
  int s = 0;
  for (int i = 0; i < 8; i++) {
    u8 b = *p++;
    s |= ((u64)(b&0x7f)) << (i * 7);
    if (!(b & 0x80)) {
      *len = i+1;
      break;
    }
  }
  return s;
}

#if defined(_WINDOWS) || defined(_WIN32) || defined(_MSC_VER)

#if !defined(_WINCE)

int strcasecmp(const char* pszStr1, const char* pszStr2)
{
    return _stricmp(pszStr1, pszStr2);
}

#endif /* #if !defined(_WINCE) */

int strncasecmp(const char* pszStr1, const char* pszStr2, int len)
{
    return _strnicmp(pszStr1, pszStr2, (size_t) len);
}

#endif /* #if defined(_WINDOWS) */

static int ReadObuHeader(const u8* data_start, const u8* data_end, obuHeader_t* hdr,
                  bool annexb, int annexb_size, bool size_check) {
  const u8* local = data_start;
  if (local == data_end)
    return HANTRO_NOK;
  u8 b0 = *local++;
  if (b0 & 1) {
    // Forbidden bit. Must not be set.
    return HANTRO_NOK;
  }
  hdr->type           = (b0 >> 3) & 0xf;
  hdr->has_extension  = (b0 >> 2) & 1;
  hdr->has_size_field = (b0 >> 1) & 1;

  if (!hdr->has_size_field && !annexb) {
    // section 5 obu streams must have obu_size field set.
    return HANTRO_NOK;
  }

  if (b0 >> 7) {
    // obu_reserved_1bit must be set to 0.
    return HANTRO_NOK;
  }

  if (hdr->has_extension) {
    if (local == data_end)
      return HANTRO_NOK;
    u8 b1 = *local++;
    if (b1 & 0x7) {
      // extension_header_reserved_3bits must be set to 0.
      return HANTRO_NOK;
    }
    hdr->temporal_layer_id = (b1 >> 5) & 0x7;
    hdr->spatial_layer_id  = (b1 >> 3) & 0x3;
  }

  int size = 0;
  if(hdr->has_size_field) {
    int l = 0;
    size = leb128(local, &l);
    if (size < 0 || (size_check && size > (data_end - local)))
      return HANTRO_NOK;
    local += l;
  } else {
    if (!annexb) return HANTRO_NOK;
    size = annexb_size - (hdr->has_extension ? 2 : 1);
  }

  hdr->header_size  = local - data_start;
  hdr->total_size   = size + hdr->header_size;
  hdr->payload_size = size;

  return HANTRO_OK;
}
#endif

VpxReaderInst VpxRdrOpen(const char* filename, char* index_file_name, u32 mode, u32 low_latency) {
  struct FfReader* inst = calloc(1, sizeof(struct FfReader));
  if (inst == NULL) return NULL;
  inst->format = FF_NONE;
  inst->file = fopen(filename, "rb");
  if (inst->file == NULL) {
    free(inst);
    return NULL;
  }
  inst->bitstream_format = FfCheckFormat(inst);
  if (inst->bitstream_format == 0) {
    fclose(inst->file);
    free(inst);
    return NULL;
  }
  if(index_file_name)
    inst->file_index_fp = fopen(index_file_name, "r");
  /*not support low latency mode*/
  (void) low_latency;
  return inst;
}

int VpxRdrIdentifyFormat(VpxReaderInst inst) {
  struct FfReader* reader = (struct FfReader*)inst;
  return reader->bitstream_format;
}

void VpxRdrHeadersDecoded(VpxReaderInst inst) {
  return;
}

#ifdef SEEK_TEST
extern u32 seek_end;
#endif
int VpxRdrReadFrame(VpxReaderInst inst, u8* buffer, u8 *stream[2], i32* size, u8 is_ringbuffer) {
  u32 tmp;
  off_t frame_header_pos;
  u32 buf_len, offset;
  u32 frame_size = 0;
  u8* strm = stream[1];
  struct FfReader* ff = (struct FfReader*)inst;
  FILE* fin = ff->file;

#ifdef SEEK_TEST
  if (!seek_end) {
    fseeko(fin, 0, SEEK_SET);
    ff->ivf_headers_read = 0;
  }
#endif
  /* Read VPx IVF file header */
  if ((ff->format == FF_VP8 || ff->format == FF_VP9 || ff->format == FF_AV1) &&
      !ff->ivf_headers_read) {
    tmp = ReadIvfFileHeader(fin);
    if (tmp != 0) return tmp;
    ff->ivf_headers_read = 1;
  }

  frame_header_pos = ftello(fin);
  /* Read VPx IVF frame header */
  if (ff->format == FF_VP8 || ff->format == FF_VP9 || ff->format == FF_AV1) {
    tmp = ReadIvfFrameHeader(fin, &frame_size);
    if (tmp != 0) return tmp;
  } else if (ff->format == FF_VP7) {
    u8 size[4];
    tmp = fread(size, sizeof(u8), 4, fin);
    if (tmp != 4) return -1;

    frame_size = size[0] + (size[1] << 8) + (size[2] << 16) + (size[3] << 24);
  } else if (ff->format == FF_WEBP) {
    char signature[] = "WEBP";
    char format[] = "VP8 ";
    char tmp[4];
    fseeko(fin, 8, SEEK_CUR);
    if (!fread(tmp, sizeof(u8), 4, fin)) return -1;
    if (strncmp(signature, tmp, 4)) return -1;
    if (!fread(tmp, sizeof(u8), 4, fin)) return -1;
    if (strncmp(format, tmp, 4)) return -1;
    if (!fread(tmp, sizeof(u8), 4, fin)) return -1;
    frame_size = tmp[0] + (tmp[1] << 8) + (tmp[2] << 16) + (tmp[3] << 24);
  } else if (ff->format == FF_AV1_ANNEXB) {
    // this is actually temporal unit size, may contain several frames
    frame_size = leb128file(fin);
  } else if (ff->format == FF_AV1_OBU) {
    frame_size = 0;
    // plain obus
    u8 tmpbuf[10];
    // read until next temporal delimiter
    bool first = TRUE;
    while (1) {
      obuHeader_t hdr;
      // max size of hdr + size field is 10
      size_t tmp = fread(tmpbuf, sizeof(u8), 10, fin);
      if (tmp) {
        if (ReadObuHeader(tmpbuf, tmpbuf + tmp, &hdr, FALSE, 0, FALSE)) {
          fprintf(stderr, "reading OBU header\n");
          return -1;
        }
      }
      // OBU_TEMPORAL_DELIMITER == 2
      if ((!first && hdr.type == 2) || tmp == 0) {
        fseeko(fin, -tmp, SEEK_CUR);
        break;
      }
      fseeko(fin, hdr.total_size - tmp, SEEK_CUR);
      frame_size += hdr.total_size;
      first = FALSE;
    }
    fseeko(fin, -(size_t)frame_size, SEEK_CUR);
  }
#ifdef WEBM_ENABLED
  else { /* FF_WEBM */
    size_t buf_sz;
    u8* tmp_pkt;
    if (ff->input_ctx.chunk >= ff->input_ctx.chunks) {
      unsigned int track;

      do {
        /* End of this packet, get another. */
        if (ff->input_ctx.pkt) nestegg_free_packet(ff->input_ctx.pkt);

        if (nestegg_read_packet(ff->input_ctx.nestegg_ctx,
                                &ff->input_ctx.pkt) <= 0 ||
            nestegg_packet_track(ff->input_ctx.pkt, &track))
          return -1;

      } while (track != ff->input_ctx.video_track);

      if (nestegg_packet_count(ff->input_ctx.pkt, &ff->input_ctx.chunks))
        return -1;
      ff->input_ctx.chunk = 0;
    }

    if (nestegg_packet_data(ff->input_ctx.pkt, ff->input_ctx.chunk, &tmp_pkt,
                            &buf_sz))
      return -1;

    if (buf_sz > *size) {
      *size = buf_sz;
      return -1;
    }
    ff->input_ctx.chunk++;
    frame_size = buf_sz;

    if(is_ringbuffer) {
      buf_len = *size;
      strm = buffer + buf_len - frame_size + (frame_size == 0 ? frame_size : (rand()) % (frame_size));
      offset = (u32)(strm - buffer);
      //stream[0] = stream[1];
      stream[0] = strm;
      //buf_len = *size;
      if(offset + frame_size < buf_len) {
        memcpy((void*)strm, tmp_pkt, frame_size);
        stream[1] = strm + frame_size;
      } else {
        memcpy((void*)strm, tmp_pkt, buf_len - offset);
        memcpy((void*)buffer, tmp_pkt + (buf_len - offset), frame_size - (buf_len - offset));
        stream[1] = buffer + frame_size - (buf_len - offset);
      }
    } else {
      memcpy((void*)buffer, tmp_pkt, frame_size);
      stream[0] = buffer;
      stream[1] = buffer + frame_size;
    }

    return frame_size;
  }
#endif /* WEBM_ENABLED */

  if (feof(fin)) {
    fprintf(stderr, "EOF: Input\n");
    return -1;
  }

  if (frame_size > DEC_X170_MAX_STREAM) {
    *size = 0;
    return -1;
  }

  if (frame_size > *size) {
    fprintf(stderr, "Frame size %u > buffer size %d\n", frame_size, *size);
    fseeko(fin, frame_header_pos, SEEK_SET);
    *size = frame_size;
    return -1;
  }

  {
    size_t result;
    if(is_ringbuffer) {
      buf_len = *size;
      strm = buffer + buf_len - frame_size + (frame_size == 0 ? frame_size : (rand()) % (frame_size));
      offset = (u32)(strm - buffer);
      //stream[0] = stream[1];
      stream[0] =  strm;
      //buf_len = *size;
      if(offset + frame_size < buf_len) {
        result = fread((u8*)strm, sizeof(u8), frame_size, fin);
        stream[1] = strm + result;
      } else {
        u32 tmp_len;
        result = fread((u8*)strm, sizeof(u8), buf_len - offset, fin);
        tmp_len = fread((u8*)buffer, sizeof(u8), frame_size - (buf_len - offset), fin);
        result += tmp_len;
        stream[1] = buffer + tmp_len;
      }
    } else {
      result = fread((u8*)buffer, sizeof(u8), frame_size, fin);
      stream[0] = buffer;
      stream[1] = buffer + result;
    }

    if (result != frame_size) {
      /* fread failed. */
      return -1;
    }
  }

  return frame_size;
}

void VpxRdrClose(VpxReaderInst inst) {
  struct FfReader* reader = (struct FfReader*)inst;
#ifdef WEBM_ENABLED
  if (reader->input_ctx.nestegg_ctx)
    nestegg_destroy(reader->input_ctx.nestegg_ctx);
#endif
  if(reader->file_index_fp)
    fclose(reader->file_index_fp);
  fclose(reader->file);
  free(reader);
}

static i32 ReadIvfFileHeader(FILE* fin) {
  IVF_HEADER ivf;
  u32 tmp;

  tmp = fread(&ivf, sizeof(char), sizeof(IVF_HEADER), fin);
  if (tmp == 0) return -1;

  return 0;
}

static i32 ReadIvfFrameHeader(FILE* fin, u32* frame_size) {
  union {
    IVF_FRAME_HEADER ivf;
    u8 p[12];
  } fh;
  u32 tmp;

  tmp = fread(&fh, sizeof(char), sizeof(IVF_FRAME_HEADER), fin);
  if (tmp == 0) return -1;

  *frame_size = fh.p[0] + (fh.p[1] << 8) + (fh.p[2] << 16) + (fh.p[3] << 24);

  return 0;
}

static i32 FfCheckFormat(struct FfReader* ff) {
  char id[5] = "DKIF";
  char id2[5] = "RIFF";
  char string[5] = "";
  u32 format = 0;

#ifdef WEBM_ENABLED
  u32 tmp;
  u32 four_cc = 0;
  if (FileIsWebm(&ff->input_ctx, ff->file, &four_cc, &tmp, &tmp, &tmp, &tmp)) {
    ff->format = FF_WEBM;
    if (four_cc == VP8_FOURCC)
      format = BITSTREAM_VP8;
    else if (four_cc == VP9_FOURCC)
      format = BITSTREAM_VP9;
    else if (four_cc == AV1_FOURCC)
      format = BITSTREAM_AV1;
    nestegg_track_seek(ff->input_ctx.nestegg_ctx, ff->input_ctx.video_track, 0);
  } else if (fread(string, 1, 5, ff->file) == 5)
#else
  if (fread(string, 1, 5, ff->file) == 5)
#endif
  {
    if (!strncmp(id, string, 5)) {
      fseeko(ff->file, 8, SEEK_SET);
      if (!fread(string, 1, 4, ff->file)) return 0;
      if (!strncasecmp("VP8\0", string, 4) ||
          !strncasecmp("VP80", string, 4)) {
        ff->format = FF_VP8;
        format = BITSTREAM_VP8;
      } else if (!strncasecmp("VP90", string, 4)) {
        ff->format = FF_VP9;
        format = BITSTREAM_VP9;
      } else if (!strncasecmp("AV01", string, 4)) {
        ff->format = FF_AV1;
        format = BITSTREAM_AV1;
      } else {
        fprintf(stderr, "WARNING: unknown FOURCC (%.4s) encountered in track.\n",
                string);
      }
    } else if (!strncmp(id2, string, 4)) {
      ff->format = FF_WEBP;
      format = BITSTREAM_WEBP;
    } else {
#if 0
      ff->format = FF_VP7;
      format = BITSTREAM_VP7;
#else
      // annex-b or plain obu
      char tmp[100] = "";
      rewind(ff->file);
      u8 *data = (u8*)tmp;
      int num = fread(tmp, 1, 100, ff->file);
      (void)num;
      // annex-b should start with tu len, frame len, and temporal delimiter
      // obu
      int l = 0;
      bool annexb = FALSE;
      int tusize = leb128(data, &l);
      data += l;
      int framesize = leb128(data, &l);
      data += l;
      int obusize = leb128(data, &l);
      if (obusize < framesize && framesize < tusize) {
        obuHeader_t hdr;
        data += l;
        ReadObuHeader(data, data + 2, &hdr, TRUE, 100 - (data - (u8*)tmp),
                      FALSE);
        // OBU_TEMPORAL_DELIMITER == 2
        if (hdr.type == 2 || hdr.type == 1)
          annexb = TRUE;
      }
      ff->format = annexb ? FF_AV1_ANNEXB : FF_AV1_OBU;
      format = annexb ? BITSTREAM_AV1_ANNEXB : BITSTREAM_AV1_OBU;
#endif
    }
    rewind(ff->file);
  }
  return format;
}

#ifdef WEBM_ENABLED
static int NesteggReadCb(void* buffer, size_t length, void* userdata) {
  FILE* f = userdata;

  if (fread(buffer, 1, length, f) < length) {
    if (ferror(f)) return -1;
    if (feof(f)) return 0;
  }
  return 1;
}

static int NesteggSeekCb(int64_t offset, int whence, void* userdata) {
  switch (whence) {
  case NESTEGG_SEEK_SET:
    whence = SEEK_SET;
    break;
  case NESTEGG_SEEK_CUR:
    whence = SEEK_CUR;
    break;
  case NESTEGG_SEEK_END:
    whence = SEEK_END;
    break;
  };
  return fseeko(userdata, (off_t)offset, whence) ? -1 : 0;
}

static int64_t NesteggTellCb(void* userdata) {
  return (int64_t)ftello(userdata);
}

static int FileIsWebm(struct InputCtx* input, FILE* infile,
                      unsigned int* fourcc, unsigned int* width,
                      unsigned int* height, unsigned int* fps_den,
                      unsigned int* fps_num) {
  unsigned int i, n;
  int track_type = -1;
  off_t file_size = 0;
  int format = 0;

  nestegg_io io = {NesteggReadCb, NesteggSeekCb, NesteggTellCb, infile};
  nestegg_video_params params;

  /* Get the file size for nestegg. */
  fseeko(infile, 0, SEEK_END);
  file_size = ftello(infile);
  fseeko(infile, 0, SEEK_SET);

  if (nestegg_init(&input->nestegg_ctx, io, NULL, file_size)) goto fail;

  if (nestegg_track_count(input->nestegg_ctx, &n)) goto fail;

  for (i = 0; i < n; i++) {
    track_type = nestegg_track_type(input->nestegg_ctx, i);

    if (track_type == NESTEGG_TRACK_VIDEO)
      break;
    else if (track_type < 0)
      goto fail;
  }

  format = nestegg_track_codec_id(input->nestegg_ctx, i);
  if ((format != NESTEGG_CODEC_VP9) && (format != NESTEGG_CODEC_AV1)) {
    fprintf(stderr, "Not VPX video, quitting.\n");
    exit(1);
  }

  input->video_track = i;

  if (nestegg_track_video_params(input->nestegg_ctx, i, &params)) goto fail;

  *fps_den = 0;
  *fps_num = 0;
  if(format == NESTEGG_CODEC_VP9)
    *fourcc = VP9_FOURCC;
  else if(format == NESTEGG_CODEC_AV1)
    *fourcc = AV1_FOURCC;
  *width = params.width;
  *height = params.height;
  return 1;
fail:
  input->nestegg_ctx = NULL;
  rewind(infile);
  return 0;
}
#endif /* WEBM_ENABLED */
