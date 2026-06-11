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

#include "common_sink.h"
#include "file_sink.h"
#include "md5_sink.h"
#include "null_sink.h"
#include "md5.h"
#include "sw_util.h"  /* NEXT_MULTIPLE */

#define MAX4(a, b, c, d) MAX(MAX(a, b), MAX(c, d))

void* CreateYuvSink(struct TestParams *test_params) {

  YuvSink *yuvsink = (YuvSink *)malloc(sizeof(YuvSink));
  if (yuvsink == NULL)
    return NULL;

  switch (test_params->sink_type) {
  case SINK_FILE_SEQUENCE:
    yuvsink->Open = FilesinkOpen;
    yuvsink->WritePicture = FilesinkWritePic;
    yuvsink->Close = FilesinkClose;
    break;
  case SINK_FILE_PICTURE:
    yuvsink->Open = NullsinkOpen;
    yuvsink->WritePicture = FilesinkWriteSinglePic;
    yuvsink->Close = NullsinkClose;
    break;
  case SINK_MD5_SEQUENCE:
    yuvsink->Open = Md5sinkOpen;
    yuvsink->WritePicture = Md5sinkWritePic;
    yuvsink->Close = Md5sinkClose;
    break;
  case SINK_MD5_PICTURE:
    yuvsink->Open = md5perpicsink_open;
    yuvsink->WritePicture = md5perpicsink_write_pic;
    yuvsink->Close = md5perpicsink_close;
    break;
  case SINK_MD5_VTM:
    yuvsink->Open = md5perpicsink_open;
    yuvsink->WritePicture = md5perpicsink_write_pic_vtm;
    yuvsink->Close = md5perpicsink_close;
    break;
#ifdef SDL_ENABLED
  case SINK_SDL:
    yuvsink->Open = SdlSinkOpen;
    yuvsink->WritePicture = SdlSinkWrite;
    yuvsink->Close = SdlSinkClose;
    break;
#endif
  case SINK_NULL:
    yuvsink->Open = NullsinkOpen;
    yuvsink->WritePicture = NullsinkWrite;
    yuvsink->Close = NullsinkClose;
    break;
  default:
    assert(0);
    free(yuvsink);
	return NULL;
  }
  yuvsink->inst = yuvsink->Open((const char **)test_params->out_file_name);
  return (void*)yuvsink;
}

void ReleaseYuvSink(YuvSink* yuvsink) {
  if (yuvsink) {
    if (yuvsink->inst) {
      yuvsink->Close(yuvsink->inst);
    }
    free(yuvsink);
//    yuvsink = NULL;
  }
}

u32 GetPixelWidth(PpUnitConfig *ppu_cfg, u32 pixel_width) {
  u32 pixel_width_pp = 8;

  pixel_width_pp = (ppu_cfg->out_cut_8bits || pixel_width == 8) ? 8 :
                      ((ppu_cfg->out_p010 || ppu_cfg->out_I010 || ppu_cfg->out_L010 ||
                        ppu_cfg->rgb || ppu_cfg->rgb_planar ||
                      (ppu_cfg->tiled_e && pixel_width > 8)) ? 16 : pixel_width);

  if (!(ppu_cfg->out_cut_8bits || pixel_width == 8)
      && ppu_cfg->tiled_e && !(ppu_cfg->rgb || ppu_cfg->rgb_planar) && ppu_cfg->tile_mode == TILED64x64)
    pixel_width_pp = 10;

  if (ppu_cfg->out_uyvy || ppu_cfg->out_yuyv)
    pixel_width_pp = 32;

  if (ppu_cfg->rgb || ppu_cfg->rgb_planar) {
    if (!ppu_cfg->rgb_planar) {
      /* packed RGB */
      if (IS_PIC_32BIT(ppu_cfg->rgb_format))
        pixel_width_pp = 32;
      else if (IS_PIC_24BIT(ppu_cfg->rgb_format))
        pixel_width_pp = 24;
      else if (IS_PIC_16BIT(ppu_cfg->rgb_format))
        pixel_width_pp = 16;
      else if (IS_PIC_8BIT(ppu_cfg->rgb_format))
        pixel_width_pp = 8;
      else if (IS_PIC_48BIT(ppu_cfg->rgb_format))
        pixel_width_pp = 48;
    } else {  /* planar RGB  */
      if (IS_PIC_48BIT(ppu_cfg->rgb_format))
        pixel_width_pp = 16;
      else
        pixel_width_pp = 8;
    }
  }
  return pixel_width_pp;
}

void GenerateOutputFileName(struct TestParams *test_params, struct OutFileInfo *info) {
  u32 i = 0;
  u32 j = 0;
  u32 index = 0;
  char local[NAME_MAX+1] = "";
  char alt_file_name[NAME_MAX+1] = "";

  if (test_params->out_file_name[0] == NULL) {
    u32 w = info->pic_width;
    u32 h = info->pic_height;
    char* fourcc[] = {"tiled4x4", "tiled8x8", "tiled16x16", "tiled128x2", "strgb", /* 0-4 */
    "tiled16x16_iyuv_fbc", "tiled32x8_iyuv_fbc", "nv12", "iyuv", "nv21", "rgb", /* 5 - 10 */
    "yuyv", "yvyu", "uyvy", "vyuy", "styuv420" /* 11-15 */};

    if (!test_params->pp_enabled) {
      test_params->out_file_name[0] = (char *)calloc(NAME_MAX, 1);
      if (test_params->out_file_name[0] == NULL) return;
      sprintf(test_params->out_file_name[0], "out_%ux%u_%s_%ub.yuv", w, h,
              test_params->compress_bypass ?  "tile4x4" : "rfc", info->bit_depth);
      if (!test_params->compress_bypass) {
        char* file_name = test_params->out_file_name[1] = (char *)calloc(NAME_MAX, 1);
        sprintf(file_name, "out_%ux%u_rfc_table_%ub.bin", w, h, info->bit_depth);
      }
    } else {
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        u32 bit_depth = GetPixelWidth(&test_params->ppu_cfg[i], info->bit_depth);

        if (test_params->ppu_cfg[i].rgb || test_params->ppu_cfg[i].rgb_planar)
          index = 10; // rgb
        else if (test_params->ppu_cfg[i].out_yuyv ) {
          if (test_params->ppu_cfg[i].cr_first)
            index = 12;
          else
            index = 11;
        } else if (test_params->ppu_cfg[i].out_uyvy ) {
          if (test_params->ppu_cfg[i].cr_first)
            index = 14;
          else
            index = 13;
        } else if (test_params->ppu_cfg[i].cr_first)
          index = 9; // nv21
        else if (test_params->ppu_cfg[i].planar) {
          if (test_params->align == DEC_ALIGN_16B && test_params->align_h == DEC_ALIGN_16B) {
            index = 5; // tiled16x16_iyuv_fbc
          } else if (test_params->align == DEC_ALIGN_32B && test_params->align_h == DEC_ALIGN_8B) {
            index = 6; // tiled32x8_iyuv_fbc
          } else {
            index = 8; // iyuv
          }
        }
        else if (test_params->ppu_cfg[i].tiled_e) {
          if (test_params->ppu_cfg[i].tile_mode == TILED8x8) {
            index = 1; // tiled8x8
          } else if (test_params->ppu_cfg[i].tile_mode == TILED16x16) {
            index = 2; // tiled16x16
          } else if (test_params->ppu_cfg[i].tile_mode == TILED128x2) {
            index = 3; // tiled128x2
          } else if (test_params->ppu_cfg[i].tile_mode == TILED64x64) {
            if(IS_PIC_RGB(test_params->ppu_cfg[i].rgb_format)) {
              index = 4; // strgb
            } else {
              index = 15; // styuv420
            }
          } else if (test_params->align == DEC_ALIGN_16B && test_params->align_h == DEC_ALIGN_16B) {
            index = 5; // tiled16x16_fbc
          } else if (test_params->align == DEC_ALIGN_32B && test_params->align_h == DEC_ALIGN_8B) {
            index = 6; // tiled32x8_fbc
          } else {
            index = 0; // tiled4x4
          }
        } else
          index = 7; // nv12
        if (test_params->ppu_cfg[i].scale.scale_by_ratio) {
          if (info->is_thumbnail) {
            w = info->pic_width;
            h = info->pic_height;
          } else {
            u32 dscale_shift[9] = {0, 0, 1, 0, 2, 0, 0, 0, 3};
            u32 dscale_x_shift = dscale_shift[test_params->ppu_cfg[i].scale.ratio_x];
            u32 dscale_y_shift = dscale_shift[test_params->ppu_cfg[i].scale.ratio_y];
            w = test_params->ppu_cfg[i].crop.width ?
              test_params->ppu_cfg[i].crop.width : info->pic_width;
            h = test_params->ppu_cfg[i].crop.height ?
              test_params->ppu_cfg[i].crop.height : info->pic_height;
            /*support odd crop, output reality resolution no need align 2 pixel*/
            if (test_params->crop_align)
              w = ((w / 2) >> dscale_x_shift) << 1;
            else if (test_params->ppu_cfg[i].scale.ratio_x > 1)
              w = (w >> dscale_x_shift);
            /*support odd crop*/
            if (test_params->crop_align) {
              if (info->is_interlaced)
                h = ((h / 4) >> dscale_y_shift) << 2;
              else
                h = ((h / 2) >> dscale_y_shift) << 1;
            } else {
              if (info->is_interlaced)
                h = ((h / 4) >> dscale_y_shift) << 2;/*TODO need to support odd output*/
              /*support odd crop, output reality resolution no need align 2 pixel*/
              else if (test_params->ppu_cfg[i].scale.ratio_y > 1)
                h = (h >> dscale_y_shift);
            }
            if (test_params->ppu_cfg[i].pad.mode) {
              w += test_params->ppu_cfg[i].pad.l_off + test_params->ppu_cfg[i].pad.r_off;
              h += test_params->ppu_cfg[i].pad.t_off + test_params->ppu_cfg[i].pad.b_off;
            }
          }
        } else if (test_params->ppu_cfg[i].enabled) {
          if (info->is_thumbnail) {
            w = info->pic_width;
            h = info->pic_height;
          } else {
            w = test_params->ppu_cfg[i].crop2.enabled ?
                test_params->ppu_cfg[i].crop2.width :
                (test_params->ppu_cfg[i].scale.width ?
                test_params->ppu_cfg[i].scale.width :
                  (test_params->ppu_cfg[i].crop.width ?
                    test_params->ppu_cfg[i].crop.width : info->pic_width));
            h = test_params->ppu_cfg[i].crop2.enabled ?
                test_params->ppu_cfg[i].crop2.height :
                (test_params->ppu_cfg[i].scale.height ?
                test_params->ppu_cfg[i].scale.height :
                  (test_params->ppu_cfg[i].crop.height ?
                    test_params->ppu_cfg[i].crop.height : info->pic_height));
            if (test_params->ppu_cfg[i].pad.mode) {
              w += test_params->ppu_cfg[i].pad.l_off + test_params->ppu_cfg[i].pad.r_off;
              h += test_params->ppu_cfg[i].pad.t_off + test_params->ppu_cfg[i].pad.b_off;
            }
          }
        }
        test_params->out_file_name[i] = (char *)calloc(NAME_MAX, 1);
        if (test_params->out_file_name[i] == NULL) return;
        if(info->is_thumbnail)
          sprintf(test_params->out_file_name[i], "outthumb_%ux%u_%s_%ub_%u.yuv", w, h, fourcc[index], bit_depth, i);
        else {
          sprintf(test_params->out_file_name[i], "out_%ux%u_%s_%ub_%u.yuv", w, h, fourcc[index], bit_depth, i);
        }
      }
      for (j = DEC_MAX_OUT_COUNT, i = DEC_MAX_OUT_COUNT; j < DEC_MAX_OUT_COUNT*2; i++, j++) {
        if(test_params->out_file_name[j-DEC_MAX_OUT_COUNT] != NULL) {
          if (test_params->out_file_name[i] != NULL)
            free(test_params->out_file_name[i]);
          test_params->out_file_name[i] = (char *)calloc(NAME_MAX, 1);
          if (test_params->out_file_name[i] == NULL) return;
          strncpy(alt_file_name, test_params->out_file_name[j-DEC_MAX_OUT_COUNT], sizeof(alt_file_name)-1);
          alt_file_name[sizeof(alt_file_name)-1] = '\0';
          sprintf(alt_file_name+MIN((strlen(alt_file_name)-4), 236), "_dec400_table.bin");
          strncpy(test_params->out_file_name[i],alt_file_name, strlen(alt_file_name));
        }
      }
    }
  } else {
    if (test_params->pp_enabled) {
      strncpy(local, test_params->out_file_name[0], sizeof(local)-1);
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        u32 bit_depth = GetPixelWidth(&test_params->ppu_cfg[i], info->bit_depth);

        if(test_params->out_file_name[i] != NULL)
          free(test_params->out_file_name[i]);
        test_params->out_file_name[i] = (char *)calloc(NAME_MAX, 1);
        if (test_params->out_file_name[i] == NULL) return;
        strncpy(alt_file_name, local, sizeof(alt_file_name)-1);
        sprintf(alt_file_name+MIN((strlen(alt_file_name)-4), 236), "_%ub_%u.yuv", (bit_depth & 0xFF), i);
        strncpy(test_params->out_file_name[i], alt_file_name, strlen(alt_file_name));
      }
      for (j = DEC_MAX_OUT_COUNT, i = DEC_MAX_OUT_COUNT; j < DEC_MAX_OUT_COUNT*2; i++, j++) {
        if (test_params->out_file_name[i] != NULL)
          free(test_params->out_file_name[i]);
        test_params->out_file_name[i] = (char *)calloc(NAME_MAX, 1);
        if (test_params->out_file_name[i] == NULL) return;
        strncpy(alt_file_name, local, sizeof(alt_file_name)-1);
        sprintf(alt_file_name+MIN((strlen(alt_file_name)-4), 233), "_pp_%u_dec400_table.bin", i-DEC_MAX_OUT_COUNT);
        strncpy(test_params->out_file_name[i],alt_file_name, strlen(alt_file_name));
      }
    }
    else if (!test_params->compress_bypass) {
      /* rename rfc table */
      for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
        if (i==1 && test_params->out_file_name[i] != NULL) {
          strncpy(local, test_params->out_file_name[i], strlen(test_params->out_file_name[i]));
          free(test_params->out_file_name[i]);
          test_params->out_file_name[i] = (char *)calloc(NAME_MAX, 1);
          if (test_params->out_file_name[i] == NULL) return;
          strncpy(alt_file_name, local, strlen(local));
          sprintf(alt_file_name+MIN((strlen(alt_file_name)-4), 216), "_rfc_table_%db.bin", (info->bit_depth>8)?10:8);
          strncpy(test_params->out_file_name[i],alt_file_name, strlen(alt_file_name));
          break;
        }
      }
    }
  }
}

void PrintOutputFileName(struct TestParams *test_params) {
  u32 i = 0;
  if (test_params->pp_enabled) {
    for (i = 0; i < DEC_MAX_OUT_COUNT; i++) {
      if (test_params->ppu_cfg[i].enabled == 1) {
        printf("[TB] Output file: %s\n", test_params->out_file_name[i]);
      }
    }
  } else {
    if (test_params->out_file_name[0])
      printf("[TB] Output file: %s\n", test_params->out_file_name[0]);
    if (test_params->out_file_name[1])
      printf("[TB] Output file: %s\n", test_params->out_file_name[1]);
  }
}

void FreeOutputFileName(struct TestParams *test_params) {
  u32 i = 0;
  for (i = 0; i < 2 * DEC_MAX_OUT_COUNT; i++) {
    if (test_params->out_file_name[i]) {
      free(test_params->out_file_name[i]);
      test_params->out_file_name[i] = NULL;
    }
  }
}

static int output_field_num = 0;
/* common function to dump one picture or md5 to file[0] */
/**
 * \brief This function dumps a picture to file \n
 * \ingroup common_group
 * \param [in]     file[2]        file[0] for YUV/md5 and file[1] for table.
 * \param [in]     pic            picture to be output.
 * \param [in]     ct             dump bits to md5 context MD5Context* ct
 * \param [in]     md5            0: YUV 1: md5 (calculating per picture checksums/calculating sequence checksum)
 */
void CommonWriteOnePic(FILE *file[2], u32 md5, struct DecPicture *pic, void *ct) {
  u32 w;   /* real bits in a line */
  u32 h,hc,wc;   /* real lines */
  u32 s;   /* stride of a line in bytes */
  u32 bd = 0;
  u8* p;

  u32 width_in_block = 0, height_in_block = 0;
  u32 payload_size = 0, payload_size_align = 0, block_payload_size = 0;
  u32 header_size = 0, header_size_align = 0;
  u8 value[128] = {0};
  u8 pixel_type = 1;

  struct MD5Context *ctx = (struct MD5Context*) ct;

  p = (u8*)pic->luma.virtual_address;

  if (IS_PIC_RFC(pic->picture_info.format)) {
    u32 bd = (pic->sequence_info.bit_depth_luma == 8 &&
              pic->sequence_info.bit_depth_chroma == 8) ? 8 : 10;
    w = pic->pic_width * 8 * bd;
    h = pic->pic_height / 8;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) {
      Md5UpdateBits(p, w, h, s, ctx);
    }
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
    }
    /* luma rfc table */
    fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
    if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
      if (!md5) fflush(file[0]);
      return;
    }
    w = pic->pic_width * 4 * bd;
    h = pic->pic_height / 8;
    s = pic->pic_stride_ch;
    p = (u8 *)pic->chroma.virtual_address;
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else DumpBitsToFile(p, w, h, s, file[0]);
    /* chroma rfc table */
    fwrite((u8*)pic->chroma_table.virtual_address, 1,pic->chroma_table.logical_size, file[1]);
  } else if (IS_PIC_DEC400(pic->picture_info.format)) {
    if (IS_PIC_TILE(pic->picture_info.format))
    {
      if (IS_PIC_TILED8x8(pic->picture_info.format))
      {
       /* for dec400, we should output pading data */
        w = pic->pic_stride * 8;
        h = NEXT_MULTIPLE(pic->pic_height, 8)/ 8;
        s = pic->pic_stride;
        p = (u8 *)pic->luma.virtual_address;
        if (md5) {
          w = pic->pic_stride;
          Md5UpdateBits(p, w, h, s, ctx);
          fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
          if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) return;
          w = pic->pic_stride_ch;
          s = pic->pic_stride_ch;
          p = (u8 *)pic->chroma.virtual_address;
          Md5UpdateBits(p, w, h, s, ctx);
        }
        else {
          DumpBitsToFile(p, w, h, s, file[0]);
          fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
          /* chroma */
          if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
            fflush(file[0]);
            return;
          }
          h = NEXT_MULTIPLE((pic->pic_height + 1)/2, 4)/ 4;
          w = pic->pic_stride_ch * 8;
          s = pic->pic_stride_ch;
          p = (u8 *)pic->chroma.virtual_address;
          DumpBitsToFile(p, w, h, s, file[0]);
        }
        fwrite((u8*)pic->chroma_table.virtual_address, 1,pic->chroma_table.logical_size, file[1]);
      } else if (!IS_PIC_RGB(pic->picture_info.format))
      {
       /* for dec400, we should output pading data */
        w = pic->pic_stride * 8;
        h = NEXT_MULTIPLE(pic->pic_height, 4)/ 4;
        s = pic->pic_stride;
        p = (u8 *)pic->luma.virtual_address;
        if (md5) {
          w = pic->pic_stride;
          Md5UpdateBits(p, w, h, s, ctx);
          fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
          if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) return;
          w = pic->pic_stride_ch;
          s = pic->pic_stride_ch;
          p = (u8 *)pic->chroma.virtual_address;
          Md5UpdateBits(p, w, h, s, ctx);
        }
        else {
          DumpBitsToFile(p, w, h, s, file[0]);
          fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
          /* chroma */
          if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
            fflush(file[0]);
            return;
          }
          h = NEXT_MULTIPLE(pic->pic_height/2, 4)/ 4;
          s = pic->pic_stride_ch;
          p = (u8 *)pic->chroma.virtual_address;
          DumpBitsToFile(p, w, h, s, file[0]);
        }
        fwrite((u8*)pic->chroma_table.virtual_address, 1,pic->chroma_table.logical_size, file[1]);
      } else {
       /* for dec400, we should output pading data */
        w = pic->pic_stride * 8;
        h = NEXT_MULTIPLE(pic->pic_height, 64)/ 64;
        s = pic->pic_stride;
        p = (u8 *)pic->luma.virtual_address;
        if (md5) {
          w = pic->pic_stride;
          Md5UpdateBits(p, w, h, s, ctx);
          fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
        }
        else {
          DumpBitsToFile(p, w, h, s, file[0]);
          fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
        }
      }
    } else if(IS_PIC_RGB(pic->picture_info.format) || IS_PIC_422PACKED(pic->picture_info.format)) {
      /* for dec400, we should output pading data */
      w = pic->pic_stride * 8;
      h = pic->pic_height;
      s = pic->pic_stride;
      p = (u8 *)pic->luma.virtual_address;
      if (md5) {
        w = pic->pic_stride;
        Md5UpdateBits(p, w, h, s, ctx);
        fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
      }
      else {
        DumpBitsToFile(p, w, h, s, file[0]);
        fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
      }
    } else {
      if (md5) {
        h = pic->pic_height;
        w = pic->pic_stride * 8;
        s = pic->pic_stride;
        p = (u8 *)pic->luma.virtual_address;
        Md5UpdateBits(p, w, h, s, ctx);
        fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
        if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
          return;
        }
        w = pic->pic_stride_ch * 8;
        s = pic->pic_stride_ch;
        h = pic->pic_height / 2;
        p = (u8 *)pic->chroma.virtual_address;
        Md5UpdateBits(p, w, h, s, ctx);
        fwrite((u8*)pic->chroma_table.virtual_address, 1,pic->chroma_table.logical_size, file[1]);
      }
      else {
        /* luma */
        /* for dec400, we should output pading data */
        w = pic->pic_stride * 8;
        h = pic->pic_height;
        s = pic->pic_stride;
        p = (u8 *)pic->luma.virtual_address;
        DumpBitsToFile(p, w, h, s, file[0]);
        fwrite((u8*)pic->luma_table.virtual_address, 1,pic->luma_table.logical_size, file[1]);
        /* chroma */
        if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
          fflush(file[0]);
          return;
        }
        /* for dec400, we should output pading data */
        w = pic->pic_stride_ch * 8;
        h = pic->pic_height / 2;
        s = pic->pic_stride_ch;
        p = (u8 *)pic->chroma.virtual_address;

        DumpBitsToFile(p, w, h, s, file[0]);

        p = (u8 *)pic->chroma.virtual_address + NEXT_MULTIPLE(s * h, PLANE_ALIGNMENT);
        if (IS_PIC_PLANAR(pic->picture_info.format)) {
          DumpBitsToFile(p, w, h, s, file[0]);
        }
        fwrite((u8*)pic->chroma_table.virtual_address, 1,pic->chroma_table.logical_size, file[1]);
        fflush(file[0]);
        return;
      }
    }
  } else if (IS_PIC_PVFBC(pic->picture_info.format)) {/*only support 420SP*/
    u64 payload_size, header_size, plane0_size, plane1_size;
    /*plan0*/
    w = pic->pic_width;
    h = pic->pic_height;
    w = IS_PIC_8BIT(pic->picture_info.format) ? w : w * 2;
    payload_size = NEXT_MULTIPLE(w, 64) * NEXT_MULTIPLE(h, 4);
    header_size = NEXT_MULTIPLE(payload_size / 256, 256);
    plane0_size = payload_size + header_size;
    p = (u8 *)pic->luma.virtual_address;
    fwrite(p, 1, plane0_size, file[0]);
    /*plan1*/
    if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
      fflush(file[0]);
      return;
    }
    w = NEXT_MULTIPLE(pic->pic_width, 2);
    h = (pic->pic_height + 1) / 2;
    w = IS_PIC_8BIT(pic->picture_info.format) ? w : w * 2;
    payload_size = NEXT_MULTIPLE(w, 64) * NEXT_MULTIPLE(h, 4);
    header_size = NEXT_MULTIPLE(payload_size / 256, 256);
    plane1_size = payload_size + header_size;
    p = (u8 *)pic->chroma.virtual_address;
    fwrite(p, 1, plane1_size, file[0]);
    fflush(file[0]);
    return;
  } else if (IS_PIC_TILED4x4(pic->picture_info.format)) {
      bd = IS_PIC_10BIT(pic->picture_info.format) ? 10 : (IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16);

      w = NEXT_MULTIPLE(pic->pic_width, 4) * 4 * bd;
      h = NEXT_MULTIPLE(pic->pic_height, 4)/ 4;
      s = pic->pic_stride;
      p = (u8 *)pic->luma.virtual_address;
      if (md5) {
        Md5UpdateBits(p, w, h, s, ctx);
      }
      else {
        DumpBitsToFile(p, w, h, s, file[0]);
      }
      if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
        if (!md5) fflush(file[0]);
        return;
      }
      w = NEXT_MULTIPLE(pic->pic_width, 4) * 4 * bd;
      h = NEXT_MULTIPLE(pic->pic_height/2, 4)/ 4;
      s = pic->pic_stride_ch;
      p = (u8 *)pic->chroma.virtual_address;
      if (md5) Md5UpdateBits(p, w, h, s, ctx);
      else {
        DumpBitsToFile(p, w, h, s, file[0]);
        fflush(file[0]);
        return;
      }
  } else if(IS_PIC_REF_TILED8x8(pic->picture_info.format)) {
    bd = IS_PIC_10BIT(pic->picture_info.format) ? 10 : (IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16);
    w = NEXT_MULTIPLE(pic->pic_width, 8) * 8 * bd;
    h = NEXT_MULTIPLE(pic->pic_height, 8)/ 8;
    s = pic->pic_stride; // chroma -> tile4x4, luma -> tile8x8
    p = (u8 *)pic->luma.virtual_address;
    if (md5) {
      Md5UpdateBits(p, w, h, s, ctx);
    }
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
    }
    if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
      if (!md5) fflush(file[0]);
      return;
    }
    w = NEXT_MULTIPLE(pic->pic_width, 4) * 4 * bd;
    h = NEXT_MULTIPLE(pic->pic_height/2, 4)/ 4;
    s = pic->pic_stride_ch;
    p = (u8 *)pic->chroma.virtual_address;
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  } else if (IS_PIC_TILED8x8(pic->picture_info.format)) {
    bd = IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16;
    w = NEXT_MULTIPLE(pic->pic_width, 8) * 8 * bd;
    h = NEXT_MULTIPLE(pic->pic_height, 8)/ 8;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) {
      Md5UpdateBits(p, w, h, s, ctx);
      if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
        return;
      }
      w = NEXT_MULTIPLE(pic->pic_width, 4) * 4 * bd;
      h = NEXT_MULTIPLE(pic->pic_height/2, 4)/ 4;
      s = pic->pic_stride_ch;
      p = (u8 *)pic->chroma.virtual_address;
      Md5UpdateBits(p, w, h, s, ctx);
    }
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
        fflush(file[0]);
        return;
      }
      w = NEXT_MULTIPLE(pic->pic_width, 4) * 4 * bd;
      h = NEXT_MULTIPLE(pic->pic_height/2, 4)/ 4;
      s = pic->pic_stride_ch;
      p = (u8 *)pic->chroma.virtual_address;
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  }
  else if (IS_PIC_TILED16x16(pic->picture_info.format)) {
    bd = IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16;

    w = NEXT_MULTIPLE(pic->pic_width, 16) * 16 * 3 / 2 * bd;
    h = NEXT_MULTIPLE(pic->pic_height, 16)/ 16;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) {
      Md5UpdateBits(p, w, h, s, ctx);
    }
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  } else if (IS_PIC_TILED16x16_COMP(pic->picture_info.format)) {
    bd = IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16;
    block_payload_size = bd == 8 ? 384 : 512;
    width_in_block = NEXT_MULTIPLE(pic->pic_width, 16) >> 4;
    height_in_block = NEXT_MULTIPLE(pic->pic_height, 16) >> 4;
    header_size = width_in_block * height_in_block * 16;
    header_size_align = NEXT_MULTIPLE(width_in_block * height_in_block * 16, 128);
    p = (u8 *)pic->luma.virtual_address;
    fwrite(p, 1, header_size, file[0]);
    if (header_size_align - header_size)
      fwrite(value, 1,  header_size_align - header_size, file[0]);

    p = (u8*)pic->luma.virtual_address + header_size_align;
    if(p == NULL) return;
    payload_size = width_in_block * height_in_block * block_payload_size;
    payload_size_align =NEXT_MULTIPLE(width_in_block * height_in_block * block_payload_size, 128);
    fwrite(p, 1, payload_size, file[0]);
    if (payload_size_align - payload_size)
      fwrite(value, 1, payload_size_align - payload_size, file[0]);
    fflush(file[0]);
    return;
  } else if (IS_PIC_TILED32x8_COMP(pic->picture_info.format)) {

    bd = IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16;
    p = (u8*)pic->luma.virtual_address;
    block_payload_size = bd == 8 ? 384 : 512;

    width_in_block = NEXT_MULTIPLE(pic->pic_width, 32) >> 5;
    height_in_block = NEXT_MULTIPLE(pic->pic_height, 8) >> 3;
    header_size = width_in_block * height_in_block * 16;
    header_size_align = NEXT_MULTIPLE(width_in_block * height_in_block * 16, 128);
    fwrite(p, 1, header_size, file[0]);
    if (header_size_align - header_size)
      fwrite(value, 1,  header_size_align - header_size, file[0]);

    p = (u8*)pic->luma.virtual_address + header_size_align;
    if(p == NULL) return;
    width_in_block = NEXT_MULTIPLE(pic->pic_width, 32) >> 5;
    height_in_block = NEXT_MULTIPLE(pic->pic_height, 8) >> 3;
    payload_size = width_in_block * height_in_block * block_payload_size;
    payload_size_align = NEXT_MULTIPLE(width_in_block * height_in_block * block_payload_size,128);
    fwrite(p, 1, payload_size, file[0]);
    if (payload_size_align - payload_size)
      fwrite(value, 1, payload_size_align - payload_size, file[0]);
    fflush(file[0]);
    return;
  } else if (IS_PIC_TILED128x2(pic->picture_info.format)) {
      bd = IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16;
      w = NEXT_MULTIPLE(pic->pic_width, 128) * 2 * bd;
      h = pic->pic_height / 2;
      s = pic->pic_stride;
      p = (u8 *)pic->luma.virtual_address;
      if (md5) {
        Md5UpdateBits(p, w, h, s, ctx);
        if (IS_PIC_MONOCHROME(pic->picture_info.format)) {
          return;
        }
      }
      else{
        DumpBitsToFile(p, w, h, s, file[0]);
        if (IS_PIC_MONOCHROME(pic->picture_info.format))
          return;
      }
      if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
        if (!md5) fflush(file[0]);
        return;
      }
      w = NEXT_MULTIPLE(pic->pic_width, 128) * 2 * bd;
      h = pic->pic_height / 4;
      s = pic->pic_stride_ch;
      p = (u8 *)pic->chroma.virtual_address;
      if (md5) {
        Md5UpdateBits(p, w, h, s, ctx);
      }
      else {
        DumpBitsToFile(p, w, h, s, file[0]);
        fflush(file[0]);
        return;
      }
  } else if (IS_PIC_RGB_TILED64x64(pic->picture_info.format)) {
    bd = 32;
    w = NEXT_MULTIPLE(pic->pic_width, 64) * 64 * bd;
    h = NEXT_MULTIPLE(pic->pic_height, 64) / 64;
    // s = NEXT_MULTIPLkE(pic->pic_stride / 4, 64) * 64 * 4;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  }
  else if (IS_PIC_TILED64x64(pic->picture_info.format)) {
    bd = IS_PIC_10BIT(pic->picture_info.format) ? 10 : (IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16);
    // bd = IS_PIC_8BIT(pic->picture_info.format) ? 8 : 16;
    w = NEXT_MULTIPLE(pic->pic_width, 64) * 64 * bd;
    h = NEXT_MULTIPLE(pic->pic_height, 64) / 64;
    // s = NEXT_MULTIPLkE(pic->pic_stride / 4, 64) * 64 * 4;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) {
      Md5UpdateBits(p, w, h, s, ctx);
      if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
        return;
      }
      w = NEXT_MULTIPLE(pic->pic_width, 64) * 64 * bd;
      h = NEXT_MULTIPLE(pic->pic_height / 2, 64) / 64;
      s = pic->pic_stride_ch;
      p = (u8 *)pic->chroma.virtual_address;
      Md5UpdateBits(p, w, h, s, ctx);
    }
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
        fflush(file[0]);
        return;
      }
      w = NEXT_MULTIPLE((pic->pic_width + 1) / 2, 32) * 32 * bd * 2; // Distinguish U and V
      h = NEXT_MULTIPLE((pic->pic_height + 1) / 2, 32) / 32;
      s = pic->pic_stride_ch / 2;
      p = (u8 *)pic->chroma.virtual_address;
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  }
  else if (IS_PIC_PACKED_RGB(pic->picture_info.format)) {
    if (IS_PIC_24BIT(pic->picture_info.format))
      bd = 24;
    else if (IS_PIC_32BIT(pic->picture_info.format))
      bd = 32;
    else if (IS_PIC_48BIT(pic->picture_info.format))
      bd = 48;
    w = pic->pic_width * bd * pixel_type;
    h = pic->pic_height;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  }
  else if (IS_PIC_PLANAR_RGB(pic->picture_info.format)) {
    if (IS_PIC_8BIT(pic->picture_info.format))
      bd = 8;
    else if (IS_PIC_16BIT(pic->picture_info.format))
      bd = 16;
    /* R */
    w = pic->pic_width * bd * pixel_type;
    h = pic->pic_height;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else DumpBitsToFile(p, w, h, s, file[0]);
    /* G */
    p = (u8*)pic->luma.virtual_address + NEXT_MULTIPLE(pic->sequence_info.pic_height * pic->pic_stride, PLANE_ALIGNMENT);
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else DumpBitsToFile(p, w, h, s, file[0]);
    /* B */
    p = (u8*)pic->luma.virtual_address + NEXT_MULTIPLE(pic->sequence_info.pic_height * pic->pic_stride, PLANE_ALIGNMENT) * 2;
    if (md5) Md5UpdateBits(p, w, h, s, ctx);
    else {
      DumpBitsToFile(p, w, h, s, file[0]);
      fflush(file[0]);
      return;
    }
  }
  else if (pic->sequence_info.is_interlaced && pic->fields_in_picture)
  {
    if (IS_PIC_8BIT(pic->picture_info.format))
      bd = 8;
    else if (IS_PIC_16BIT(pic->picture_info.format))
      bd = 16;
    w = pic->pic_width;
    h = pic->pic_height;
    p = (u8 *)pic->luma.virtual_address;
    if(pic->top_field_first&&pic->fields_in_picture) {
      if(pic->fields_in_picture==1) {
        /* only dump top field */
        s = pic->pic_stride*2;
        if (md5) Md5UpdateBits(p, w*bd, h/2, s, ctx);
        else DumpBitsToFile(p, w*bd, h/2, s, file[0]);
        if (pic->chroma.virtual_address) {
          /* round odd picture dimensions to next multiple of two for chroma */
          wc=(w+1)/2;
          hc= (h+3)/4;
          p = (u8*)pic->chroma.virtual_address;
          if (IS_PIC_FIELD_PLANAR(pic->picture_info.format)) {
            /* pic_stride_ch = pic_stride / 2 */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p, wc*bd, hc, s, file[0]);
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p+s*hc, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p+s*hc, wc*bd, hc, s, file[0]);
          } else {
            /* pic_stride_ch = pic_stride */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p, 2*wc*bd, hc, s, ctx);
            else DumpBitsToFile(p, 2*wc*bd, hc, s, file[0]);
          }
        }
        output_field_num++;
      } else {
        /* Dump top field firstly. */
        p = (u8*)pic->luma.virtual_address;
        s = pic->pic_stride*2;
        if (md5) Md5UpdateBits(p, w*bd, h/2, s, ctx);
        else DumpBitsToFile(p, w*bd, h/2, s, file[0]);
        if (pic->chroma.virtual_address)
        {
          /* round odd picture dimensions to next multiple of two for chroma */
          wc=(w+1)/2;
          hc= (h+3)/4;
          p = (u8*)pic->chroma.virtual_address;
          if (IS_PIC_FIELD_PLANAR(pic->picture_info.format)) {
            /* pic_stride_ch = pic_stride / 2 */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p, wc*bd, hc, s, file[0]);
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p+s*hc, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p+s*hc, wc*bd, hc, s, file[0]);
          } else {
            /* pic_stride_ch = pic_stride */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p, 2*wc*bd, hc, s, ctx);
            else DumpBitsToFile(p, 2*wc*bd, hc, s, file[0]);
          }
        }
        output_field_num++;
        /* then bottom field */
        {
          p = (u8*)pic->luma.virtual_address;
          s = pic->pic_stride*2;
          if (md5) Md5UpdateBits(p+s/2, w*bd, h/2, s, ctx);
          else DumpBitsToFile(p+s/2, w*bd, h/2, s, file[0]);
          if (pic->chroma.virtual_address)
          {  /* round odd picture dimensions to next multiple of two for chroma */
            wc=(w+1)/2;
            hc= (h+3)/4;
            p = (u8*)pic->chroma.virtual_address;
            if (IS_PIC_FIELD_PLANAR(pic->picture_info.format)) {
              /* pic_stride_ch = pic_stride / 2 */
              s = pic->pic_stride_ch*2;
              if (md5) Md5UpdateBits(p+s/2, wc*bd, hc, s, ctx);
              else DumpBitsToFile(p+s/2, wc*bd, hc, s, file[0]);
              s = pic->pic_stride_ch*2;
              if (md5) Md5UpdateBits(p+s*hc+s/2, wc*bd, hc, s, ctx);
              else DumpBitsToFile(p+s*hc+s/2, wc*bd, hc, s, file[0]);
            } else {
              /* pic_stride_ch = pic_stride */
              s = pic->pic_stride_ch*2;
              if (md5) Md5UpdateBits(p+s/2, 2*wc*bd, hc, s, ctx);
              else DumpBitsToFile(p+s/2, 2*wc*bd, hc, s, file[0]);
            }
          }
        }
        output_field_num++;
      }
    } else if(!pic->top_field_first&&pic->fields_in_picture) {
      p = (u8*)pic->luma.virtual_address;
      if(pic->fields_in_picture==1) {
        /* only bottom field */
        s = pic->pic_stride*2;
        if (md5) Md5UpdateBits(p+s/2, w*bd, h/2, s, ctx);
        else DumpBitsToFile(p+s/2, w*bd, h/2, s, file[0]);
        if (pic->chroma.virtual_address)
        {
          /* round odd picture dimensions to next multiple of two for chroma */
          wc=(w+1)/2;
          hc= (h+3)/4;
          p = (u8*)pic->chroma.virtual_address;
          if (IS_PIC_FIELD_PLANAR(pic->picture_info.format)) {
            /* pic_stride_ch = pic_stride / 2 */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p+s/2, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p+s/2, wc*bd, hc, s, file[0]);
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p+s*hc+s/2, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p+s*hc+s/2, wc*bd, hc, s, file[0]);
          } else {
            /* pic_stride_ch = pic_stride */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p+s/2, 2*wc*bd, hc, s, ctx);
            else DumpBitsToFile(p+s/2, 2*wc*bd, hc, s, file[0]);
          }
        }
        output_field_num++;
      } else {
        /* top field first */
        p = (u8*)pic->luma.virtual_address;
        s = pic->pic_stride*2;
        if (md5) Md5UpdateBits(p, w*bd, h/2, s, ctx);
        else DumpBitsToFile(p, w*bd, h/2, s, file[0]);
        if (pic->chroma.virtual_address) {
          /* round odd picture dimensions to next multiple of two for chroma */
          wc=(w+1)/2;
          hc= (h+3)/4;
          p = (u8*)pic->chroma.virtual_address;
          if (IS_PIC_FIELD_PLANAR(pic->picture_info.format)) {
            /* pic_stride_ch = pic_stride / 2 */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p, wc*bd, hc, s, file[0]);
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p+s*hc, wc*bd, hc, s, ctx);
            else DumpBitsToFile(p+s*hc, wc*bd, hc, s, file[0]);
          } else {
            /* pic_stride_ch = pic_stride */
            s = pic->pic_stride_ch*2;
            if (md5) Md5UpdateBits(p, 2*wc*bd, hc, s, ctx);
            else DumpBitsToFile(p, 2*wc*bd, hc, s, file[0]);
          }
        }
        output_field_num++;
        {
          /* then bottom field */
          p = (u8*)pic->luma.virtual_address;
          s = pic->pic_stride*2;
          if (md5) Md5UpdateBits(p+s/2, w*bd, h/2, s, ctx);
          else DumpBitsToFile(p+s/2, w*bd, h/2, s, file[0]);
          if (pic->chroma.virtual_address)
          {
            /* round odd picture dimensions to next multiple of two for chroma */
            wc=(w+1)/2;
            hc= (h+3)/4;
            p = (u8*)pic->chroma.virtual_address;
            if (IS_PIC_FIELD_PLANAR(pic->picture_info.format)) {
              /* pic_stride_ch = pic_stride / 2 */
              s = pic->pic_stride_ch*2;
              if (md5) Md5UpdateBits(p+s/2, wc*bd, hc, s, ctx);
              else DumpBitsToFile(p+s/2, wc*bd, hc, s, file[0]);
              s = pic->pic_stride_ch*2;
              if (md5) Md5UpdateBits(p+s*hc+s/2, wc*bd, hc, s, ctx);
              else DumpBitsToFile(p+s*hc+s/2, wc*bd, hc, s, file[0]);
            } else {
              /* pic_stride_ch = pic_stride */
              s = pic->pic_stride_ch*2;
              if (md5) Md5UpdateBits(p+s/2, 2*wc*bd, hc, s, ctx);
              else DumpBitsToFile(p+s/2, 2*wc*bd, hc, s, file[0]);
            }
          }
        }
        output_field_num++;
      }
    }
   if (md5 == OUT_YUV) {
      fflush(file[0]);
      return;
   }
  }
  else if(IS_PIC_422PACKED(pic->picture_info.format)) {
    if (IS_PIC_8BIT(pic->picture_info.format))
      bd = 8;
    else if (IS_PIC_10BIT(pic->picture_info.format))
      bd = 10;
    else
      bd = 16;
    w = pic->pic_width * bd * 2;
    h = pic->pic_height;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
    if (md5)
      Md5UpdateBits(p, w, h, s, ctx);
    else
      DumpBitsToFile(p, w, h, s, file[0]);
  }
  else {
    /*YCbCr 400/420/422 */
    if (IS_PIC_8BIT(pic->picture_info.format))
      bd = 8;
    else if (IS_PIC_10BIT(pic->picture_info.format))
      bd = 10;
    else if (IS_PIC_12BIT(pic->picture_info.format))
      bd = 12;
    else if (IS_PIC_16BIT(pic->picture_info.format))
      bd = 16;

/* 32 bits / 3 pixels + 10 bits/pixel) */
#define LINE_BITS_1010(pixels) ((pixels) / 3 * 32 + ((pixels) % 3) * 10)
    /* luma */
    w = pic->pic_width * bd * pixel_type;
    if (IS_PIC_1010(pic->picture_info.format))
      w = LINE_BITS_1010(pic->pic_width);
    h = pic->pic_height;
    s = pic->pic_stride;
    p = (u8 *)pic->luma.virtual_address;
#ifdef MODEL_SIMULATION
    if(alignheight == 8 || alignheight == 16) {
      w = w / bd;
      bd = IS_PIC_8BIT(pic->picture_info.format) ? 1 : 2;
      DumpBitsToFBCcoreInput(p, bd, w, h, s, 0, file[0]);
    } else
#endif
    if (md5)
      Md5UpdateBits(p, w, h, s, ctx);
    else
      DumpBitsToFile(p, w, h, s, file[0]);

    /* chroma */
    if (IS_PIC_MONOCHROME(pic->picture_info.format) || !pic->chroma.virtual_address) {
      if(!md5) fflush(file[0]);
      return;
    }
    if (IS_PIC_PLANAR(pic->picture_info.format)) {
      if (IS_PIC_1010(pic->picture_info.format)) {
        if (IS_PIC_YCbCr444(pic->picture_info.format)) {
          //w = LINE_BITS_1010(NEXT_MULTIPLE(pic->pic_width, 2));
          w = LINE_BITS_1010(pic->pic_width * bd);
        }
        else {
          //w = LINE_BITS_1010(NEXT_MULTIPLE(pic->pic_width, 2) /2);
          w = LINE_BITS_1010(NEXT_MULTIPLE(pic->pic_width, 2) / 2 * bd);
        }
      }
      else {
        if (IS_PIC_YCbCr444(pic->picture_info.format)) {
          /*support odd crop*/
          //w = NEXT_MULTIPLE(pic->pic_width, 2) * bd;
          w = pic->pic_width * bd;
        }
        else {
          w = NEXT_MULTIPLE(pic->pic_width, 2) / 2* bd;/*support odd crop, no need modify, luma: 2N+1, chroma: N+1*/
        }
      }
    } else {
      if (IS_PIC_1010(pic->picture_info.format)) {
        if (IS_PIC_YCbCr444(pic->picture_info.format)) {
          //w = LINE_BITS_1010(NEXT_MULTIPLE(pic->pic_width*2, 2));
          w = LINE_BITS_1010(pic->pic_width * 2 * bd);
        }
        else {
          //w = LINE_BITS_1010(NEXT_MULTIPLE(pic->pic_width, 2));
          w = LINE_BITS_1010(NEXT_MULTIPLE(pic->pic_width, 2) * bd);
        }
      }
      else {
        if (IS_PIC_YCbCr444(pic->picture_info.format)) {
          //w = NEXT_MULTIPLE(pic->pic_width * 2, 2) * bd;
          w = pic->pic_width * 2 * bd;
        } else {
          w = NEXT_MULTIPLE(pic->pic_width, 2) * bd;
        }
      }
    }
    if (IS_PIC_YCbCr422(pic->picture_info.format) || IS_PIC_YCbCr444(pic->picture_info.format)) {
      h = pic->pic_height;
    }
    else {
      h = NEXT_MULTIPLE(pic->pic_height, 2) / 2;
    }
    s = pic->pic_stride_ch;
    p = (u8 *)pic->chroma.virtual_address;
#ifdef MODEL_SIMULATION
    if(alignheight == 8 || alignheight == 16) {
      w = w / bd;
      bd = IS_PIC_8BIT(pic->picture_info.format) ? 1 : 2;
      DumpBitsToFBCcoreInput(p, bd, w, h, s, 1, file[0]);
    } else
#endif
    if (md5)
      Md5UpdateBits(p, w, h, s, ctx);
    else
      DumpBitsToFile(p, w, h, s, file[0]);
    p = (u8 *)pic->chroma.virtual_address + NEXT_MULTIPLE(s * h, PLANE_ALIGNMENT);
    if (IS_PIC_PLANAR(pic->picture_info.format)) {
#ifdef MODEL_SIMULATION
      if(alignheight == 8 || alignheight == 16) {
        bd = IS_PIC_8BIT(pic->picture_info.format) ? 1 : 2;
        DumpBitsToFBCcoreInput(p, bd, w, h, s, 1, file[0]);
      } else
#endif
      if (md5)
        Md5UpdateBits(p, w, h, s, ctx);
      else
        DumpBitsToFile(p, w, h, s, file[0]);
    }
    if (!md5)
      fflush(file[0]);
  }
}
