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

#include "libav-wrapper.h"
#include "dwlthread.h"
#include "dwl.h"

#ifdef USE_LIBAV
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avstring.h"

/* for H264 in mp4 container (QuickTime / Mov), sps/pps are stored in
   avcConfigurationBox, which can be obtained from AVCodecParameters::extradata */
/* TODO(min): HEVC in mp4 */
struct AvcCBox {
    uint8_t version;
    uint8_t profile;
    uint8_t compatibility;
    uint8_t level;
    uint8_t reserved1 : 6;
    uint8_t length_size_minus_one : 2;
    uint8_t reserved2 : 3;
    uint8_t numOfSPS : 5;
    uint16_t *sps_length;
    uint8_t **sps_data;
    uint8_t numOfPPS;
    uint16_t *pps_length;
    uint8_t **pps_data;
};

/* parse avcConfigurationBox */
static struct AvcCBox *ParseAvcCBox(uint8_t *avcC_data, uint32_t avcC_size) {
    struct AvcCBox *avcC = malloc(sizeof(struct AvcCBox));
    uint8_t *p = avcC_data;
    uint32_t remaining_size = avcC_size;

    avcC->version = *p++;
    avcC->profile = *p++;
    avcC->compatibility = *p++;
    avcC->level = *p++;
    avcC->reserved1 = (*p & 0xFC) >> 2;
    avcC->length_size_minus_one = (*p++ & 0x3) + 1;
    avcC->reserved2 = (*p & 0xE0) >> 5;
    avcC->numOfSPS = *p++ & 0x1F;

    avcC->sps_length = malloc(sizeof(uint16_t) * avcC->numOfSPS);
    avcC->sps_data = malloc(sizeof(uint8_t*) * avcC->numOfSPS);
    for (int i = 0; i < avcC->numOfSPS; i++) {
        avcC->sps_length[i] = p[0] << 8 | p[1];
        p += 2;
        avcC->sps_data[i] = p;
        p += avcC->sps_length[i];
        remaining_size -= avcC->sps_length[i] + 2; // add 2 for length field
    }

    avcC->numOfPPS = *p++;
    avcC->pps_length = malloc(sizeof(uint16_t) * avcC->numOfPPS);
    avcC->pps_data = malloc(sizeof(uint8_t*) * avcC->numOfPPS);
    for (int i = 0; i < avcC->numOfPPS; i++) {
        avcC->pps_length[i] = p[0] << 8 | p[1];
        p += 2;
        avcC->pps_data[i] = p;
        p += avcC->pps_length[i];
        remaining_size -= avcC->pps_length[i] + 2; // add 2 for length field
    }

    return avcC;
}

static void FreeAvcCBox(struct AvcCBox * avcC) {
  if (avcC == NULL) return;

  if (avcC->sps_length) free(avcC->sps_length);
  if (avcC->sps_data) free(avcC->sps_data);
  if (avcC->pps_length) free(avcC->pps_length);
  if (avcC->pps_data) free(avcC->pps_data);
}

struct LibavRdr {
  AVFormatContext *p_format_ctx;
  AVCodecParameters *p_codec_par;
  struct AvcCBox *avcC_box; /* avc configuration box for QuickTime / Mov mp4 */
  struct AVPacket *pkt;
  int avcc_sps_left;  /* sps parameters left for app from avcC_box */
  int avcc_pps_left;  /* pps parameters left for app from avcC_box */

  /* extra data for mpeg4 */
  u8 *extradata;
  u32 extradata_size;

  int video_stream;
  int raw_h264;
  int quicktime;
  int data_left_in_pkt;   /* there may be several NALU in one AVPacket */
  int video_stream_type;
};

LibavReaderInst LibavRdrOpen(const char* filename, u32 mode, u32 low_latency) {
  struct LibavRdr * libav_rdr = NULL;

  AVFormatContext *p_format_ctx = NULL;
  AVCodecParameters *p_codec_par = NULL;
  AVStream *st;
  AVPacket *pkt;
  struct AvcCBox *avcC_box = NULL;
  int video_stream = -1;
  int raw_h264 = 0;
  int quicktime = 0;
  int video_stream_type;

  /* Open video file */
  if (avformat_open_input(&p_format_ctx, filename, NULL, NULL) != 0) {
    av_log(NULL, AV_LOG_ERROR, "Couldn't open file!\n");
    return NULL;
  }

  /* Retrieve stream information */
  if (avformat_find_stream_info(p_format_ctx, NULL) < 0) {
    /* this is not fatal error, yet */
    av_log(NULL, AV_LOG_ERROR, "Couldn't find stream information!\n");
  }

  /* Dump information about file onto standard error */
  av_dump_format(p_format_ctx, 0, filename, 0);

  /* Find the video stream */
  video_stream = av_find_best_stream(p_format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1,
                                     NULL, 0);

  if (video_stream < 0) {
    av_log(NULL, AV_LOG_ERROR, "Didn't find a video stream!\n");
    goto error;
  }

  /* Get a pointer to the codec context for the video stream */
  st = p_format_ctx->streams[video_stream];
  p_codec_par = st->codecpar;

  if (!av_strcasecmp(p_format_ctx->iformat->long_name, "QuickTime / MOV"))
    quicktime = 1;

  if (st->codecpar->codec_id == AV_CODEC_ID_H264) {
    video_stream_type = BITSTREAM_H264;
    if (quicktime) {
      avcC_box = ParseAvcCBox(p_codec_par->extradata, p_codec_par->extradata_size);
    }
    else if (!av_strcasecmp(p_format_ctx->iformat->long_name, "raw H.264 video format")) {
      raw_h264 = 1;
    }
  } else if (st->codecpar->codec_id == AV_CODEC_ID_VP9) {
    video_stream_type = BITSTREAM_VP9;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_HEVC) {
    video_stream_type = BITSTREAM_HEVC;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_AVS2) {
    video_stream_type = BITSTREAM_AVS2;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_MPEG4 ||
             st->codecpar->codec_id == AV_CODEC_ID_H263) {
    video_stream_type = BITSTREAM_MPEG4;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_MPEG2VIDEO) {
    video_stream_type = BITSTREAM_MPEG2;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_FLV1) {
    video_stream_type = BITSTREAM_SORENSON;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_RV30) {
    video_stream_type = BITSTREAM_RV8;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_RV40) {
    video_stream_type = BITSTREAM_RM;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_VC1) {
    video_stream_type = BITSTREAM_VC1;
  } else if (st->codecpar->codec_id == AV_CODEC_ID_CAVS) {
    video_stream_type = BITSTREAM_AVS;
  } else {
    printf("Unknow codec id = %d\n", st->codecpar->codec_id);
    goto error;
  }

  libav_rdr = DWLcalloc(1, sizeof(struct LibavRdr));
  libav_rdr->p_format_ctx = p_format_ctx;
  libav_rdr->p_codec_par = p_codec_par;
  libav_rdr->video_stream = video_stream;
  libav_rdr->avcC_box = avcC_box;
  libav_rdr->raw_h264 = raw_h264;
  libav_rdr->quicktime = quicktime;
  libav_rdr->video_stream_type = video_stream_type;
  if (avcC_box) {
    libav_rdr->avcc_sps_left = avcC_box->numOfSPS;
    libav_rdr->avcc_pps_left = avcC_box->numOfPPS;
  } else {
    libav_rdr->avcc_sps_left = 0;
    libav_rdr->avcc_pps_left = 0;
  }

  if (quicktime) {
    /* Save extradata for mpeg4. */
    libav_rdr->extradata = (u8 *)DWLcalloc(1, p_codec_par->extradata_size);
    DWLmemcpy(libav_rdr->extradata, p_codec_par->extradata, p_codec_par->extradata_size);
    libav_rdr->extradata_size = p_codec_par->extradata_size;
  } else
    libav_rdr->extradata = NULL;

  pkt = av_packet_alloc();
  pkt->data = NULL;
  pkt->size = 0;
  libav_rdr->pkt = pkt;

  return (void *)libav_rdr;

error:
  if (p_format_ctx)
    avformat_close_input(&p_format_ctx);

  return NULL;
}

int LibavRdrIdentifyFormat(LibavReaderInst inst) {
  struct LibavRdr *libav_rdr = (struct LibavRdr *)inst;

  return libav_rdr->video_stream_type;
}
void LibavRdrHeadersDecoded(LibavReaderInst inst) {

}

int LibavRdrReadFrame(LibavReaderInst inst, u8* buffer, u8** stream, i32* size, u8 rb) {
  struct LibavRdr *libav_rdr = (struct LibavRdr *)inst;
  AVPacket *pkt = libav_rdr->pkt;
  int ret;
  uint8_t *frame_data;
  int frame_size = 0;
  int free_data = 0;
  AVFormatContext *p_format_ctx = libav_rdr->p_format_ctx;
  AVCodecParameters *p_codec_par = libav_rdr->p_codec_par;
  int video_stream = libav_rdr->video_stream;
  int raw_h264 = libav_rdr->raw_h264;
  struct AvcCBox *avcC = libav_rdr->avcC_box;

  if (avcC && (libav_rdr->avcc_sps_left || libav_rdr->avcc_pps_left)) {
    int i;
    /* return parameter set from extra data if available */
    if (libav_rdr->avcc_sps_left) {
      i = avcC->numOfSPS - libav_rdr->avcc_sps_left;
      DWLmemcpy(stream[0], avcC->sps_data[i], avcC->sps_length[i]);
      libav_rdr->avcc_sps_left--;
      frame_size = avcC->sps_length[i];
    } else if (libav_rdr->avcc_pps_left) {
      i = avcC->numOfPPS - libav_rdr->avcc_pps_left;
      DWLmemcpy(stream[0], avcC->pps_data[i], avcC->pps_length[i]);
      libav_rdr->avcc_pps_left--;
      frame_size = avcC->pps_length[i];
    }
    return frame_size;
  }

  if (libav_rdr->extradata_size && libav_rdr->quicktime) {
    /* for mpeg4, read vos/vo/vol from extra data. */
    DWLmemcpy(stream[0], libav_rdr->extradata, libav_rdr->extradata_size);
    frame_size = libav_rdr->extradata_size;
    libav_rdr->extradata_size = 0;
    return frame_size;
  }

  if (!libav_rdr->data_left_in_pkt) {
    av_packet_unref(pkt);

    /* read new packet from input stream */
    do {
      ret = av_read_frame(p_format_ctx, pkt);
    } while (ret == 0 && pkt->stream_index != video_stream);

    if (ret != 0) {
      goto end;
    }
    libav_rdr->data_left_in_pkt = pkt->size;
  }

  if (libav_rdr->video_stream_type == BITSTREAM_H264) {
    if (libav_rdr->quicktime && libav_rdr->data_left_in_pkt) {
      /* still data left in last packet */
      u8 *p = pkt->data + pkt->size - libav_rdr->data_left_in_pkt;
      u32 size = 0;
      size = ((u32)p[0] << 24) | ((u32)p[1]<<16) | ((u32)p[2] << 8) | (u32)p[3];
      DWLmemcpy(stream[0], p + 4, size);
      libav_rdr->data_left_in_pkt -= size + 4;

      return size;
    }

    if (!raw_h264) {
      uint8_t *orig_extradata = NULL;
      int orig_extradata_size = 0;

      orig_extradata_size = p_codec_par->extradata_size;
      orig_extradata = av_mallocz(
                        orig_extradata_size + AV_INPUT_BUFFER_PADDING_SIZE);

      memcpy(orig_extradata, p_codec_par->extradata, orig_extradata_size);

      if (p_codec_par->extradata == NULL) {
        p_codec_par->extradata = orig_extradata;
        p_codec_par->extradata_size = orig_extradata_size;
      } else {
        av_free(orig_extradata);
      }
      DWLmemcpy(stream[0], p_codec_par->extradata, p_codec_par->extradata_size);
      frame_size = p_codec_par->extradata_size;
    }
  } else {
    frame_data = pkt->data;
    libav_rdr->data_left_in_pkt = 0;
  }
  if (*size < pkt->size) {
    *size = pkt->size;
    libav_rdr->data_left_in_pkt = pkt->size;
    return -1; /* Insufficient buffer size */
  }
  DWLmemcpy(stream[0] + frame_size, pkt->data, pkt->size);
  frame_size += pkt->size;

end:
  if (free_data)
    av_free(frame_data);

  return frame_size;
}

void LibavRdrClose(LibavReaderInst inst) {
  struct LibavRdr *libav_rdr = (struct LibavRdr *)inst;

  if (libav_rdr->p_format_ctx)
    avformat_close_input(&(libav_rdr->p_format_ctx));

  if (libav_rdr->avcC_box)
    FreeAvcCBox(libav_rdr->avcC_box);

  if (libav_rdr->pkt)
    av_packet_free(&(libav_rdr->pkt));

  if (libav_rdr->extradata)
    DWLfree(libav_rdr->extradata);

  DWLfree(libav_rdr);
}

#endif

