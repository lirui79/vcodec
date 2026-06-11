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

#include <string.h>
#include <stdio.h>
#include "dectypes.h"
#include "case_info.h"

CaseInfo case_info;

static void WriteInfo(u32* info, u32 num,  FILE* caseprint) {
  u8 i = 0;

  if(num == 0) {
    fprintf(caseprint, "%s", " |");
    return;
  }
  for(i = 0; i < num ;i++) {
    if(i == 0)
      fprintf(caseprint, "%s", "[");
    if(i == 0 && num == 1)
      fprintf(caseprint, "%u%s|",info[i], "]");
    else if(i == num - 1)
      fprintf(caseprint,"%u%s|", info[i],"]");
    else
      fprintf(caseprint,"%u,", info[i]);
  }
}

static void GetFormatString(CaseInfo* case_info, char* f) {
  switch(case_info->format) {
    case DEC_MPEG4:
      if(!case_info->codec)  case_info->codec = DEC_MODE_MPEG4;
      sprintf(f, "%s", "mpeg4");
      break;
    case DEC_MPEG2:
      if(!case_info->codec)  case_info->codec = DEC_MODE_MPEG2;
      sprintf(f, "%s", "mpeg2");
      break;
    case DEC_VP6:
      if(!case_info->codec)  case_info->codec = DEC_MODE_VP6;
      sprintf(f, "%s", "vp6");
      break;
    case DEC_VP8:
      if(!case_info->codec)  case_info->codec = DEC_MODE_VP8;
      sprintf(f, "%s", "vp8");
      break;
    case DEC_VP9:
      if(!case_info->codec)  case_info->codec = DEC_MODE_VP8;
      sprintf(f, "%s", "vp9");
      break;
    case DEC_HEVC:
      if(!case_info->codec)  case_info->codec = DEC_MODE_HEVC;
      sprintf(f, "%s", "hevc");
      break;
    case DEC_H264:
      sprintf(f, "%s", "h264"); break;
    case DEC_AVS:
      if(!case_info->codec)  case_info->codec = DEC_MODE_AVS;
      sprintf(f, "%s", "avs");
      break;
    case DEC_AVS2:
      if(!case_info->codec)  case_info->codec = DEC_MODE_AVS2;
      sprintf(f, "%s", "avs2");
      break;
    case DEC_AV1:
      if(!case_info->codec)  case_info->codec = DEC_MODE_AV1;
      sprintf(f, "%s", "av1");
      break;
    case DEC_JPEG:
      if(!case_info->codec)  case_info->codec = DEC_MODE_JPEG;
      sprintf(f, "%s", "jpeg");
      break;
    case DEC_VC1:
      if(!case_info->codec)  case_info->codec = DEC_MODE_VC1;
      sprintf(f, "%s", "vc1");
      break;
    case DEC_RV:
      if(!case_info->codec)  case_info->codec = DEC_MODE_RV;
      sprintf(f, "%s", "rv");
      break;
    case DEC_VVC:
      if(!case_info->codec)  case_info->codec = DEC_MODE_VVC;
      sprintf(f, "%s", "vvc");
      break;
	  // case DEC_AVS3:
    //   if(!case_info->codec)  case_info->codec = DEC_MODE_AVS3;
    //   sprintf(f, "%s", "avs3");
    //   break;
    // case DEC_PNG:
    //   if(!case_info->codec)  case_info->codec = DEC_MODE_PNG;
    //   sprintf(f, "%s", "png");
    //   break;
  }
}

static void GetFrameTypeString(CaseInfo* case_info, char* s) {
  u32 i = 0, j = 0;
  for(i=0; i<20 && j<case_info->frame_num && j<10; i++) {
    if(case_info->codec == DEC_JPEG) break;
    switch(case_info->frame_type[i]) {
      case I_FRAME: strcat(s, "I,"); break;
      case B_FRAME: strcat(s, "B,"); break;
      case P_FRAME: strcat(s, "P,"); break;
      case GB_FRAME: strcat(s, "GB,"); break;
      case F_FRAME: strcat(s, "F,"); break;
      case S_FRAME: strcat(s, "S,"); break;
      case G_FRAME: strcat(s, "G,"); break;
      case BI_FRAME: strcat(s, "BI,"); break;
      case FIELD_I_I: strcat(s, "[I,I],"); break;
      case FIELD_I_P: strcat(s, "[I,P],"); break;
      case FIELD_P_I: strcat(s, "[I,P],"); break;
      case FIELD_P_P: strcat(s, "[P,P],"); break;
      case FIELD_B_B: strcat(s, "[B,B],"); break;
      case FIELD_BI_B: strcat(s, "[BI,B],"); break;
      case FIELD_B_BI: strcat(s, "[B,BI],"); break;
      case FIELD_BI_BI: strcat(s, "[BI,BI],"); break;
      case I_FIELED_0:
        if (i < 19  && case_info->frame_type[i+1] > B_FIELED_0 )
          strcat(s, "[I:");
        else
          strcat(s, "I,");
        break;
      case P_FIELED_0:
        if (i < 19 && case_info->frame_type[i+1] > B_FIELED_0 )
          strcat(s, "[P:");
        else
          strcat(s, "P,");
        break;
      case B_FIELED_0:
        if (i < 19 && case_info->frame_type[i+1] > B_FIELED_0 )
          strcat(s, "[B:");
        else
          strcat(s, "B,");
        break;
      case I_FIELED_1: strcat(s, "I],"); break;
      case P_FIELED_1: strcat(s, "P],"); break;
      case B_FIELED_1: strcat(s, "B],"); break;
    }
    if(case_info->frame_type[i] >= I_FIELED_1 || case_info->frame_type[i] < I_FIELED_0)
      j++;
  }
  s[strlen(s)-1] = '\0';
}

static void GetChromaSampleTypeString(CaseInfo* case_info, char* p) {
  if(case_info->codec == DEC_MODE_PNG) {
    switch(case_info->chroma_format_id) {
      case 0: sprintf(p, "%s", "YUV400"); break;
      case 2:
      case 3: sprintf(p, "%s", "RGB888"); break;
      case 4: sprintf(p, "%s", "YUV400_A"); break;
      case 6: sprintf(p, "%s", "ARGB888"); break;
    }
  } else {
    switch(case_info->chroma_format_id) {
      case 0: sprintf(p, "%s", "YUV400"); break;
      case 1: sprintf(p, "%s", "YUV420"); break;
      case 2: sprintf(p, "%s", "YUV422"); break;
      case 3: sprintf(p, "%s", "YUV411"); break;
      case 4: sprintf(p, "%s", "YUV440"); break;
      case 5: sprintf(p, "%s", "YUV444"); break;
      default: sprintf(p, "%s", "NOT_DEFINE"); break;
    }
  }
}


void VCDecInfoCollect(CaseInfo* case_info){
  FILE* cp = fopen("caseprint.txt", "wb");
  if(cp == NULL)
    return;

  char f[10];
  u32 num;
  char p[12],s[100];
  memset(s, 0, 100);

  GetFormatString(case_info, f);

  GetFrameTypeString(case_info, s);

  GetChromaSampleTypeString(case_info, p);

  fprintf(cp, "%s|%u|%s%s%s|%u|%u|%u|%u|%d|%u|%u|%s%u,%u%s|%u|%u|%s|",
          f, case_info->codec,"[", s, "]", case_info->frame_num > 0? case_info->frame_num : 1, case_info->scan_count,
          case_info->min_cb_size, case_info->max_cb_size, case_info->bit_depth,
          case_info->display_width, case_info->display_height, "(",case_info->crop_x, case_info->crop_y, ")",
          case_info->decode_width, case_info->decode_height, case_info->frame_num > 0? p : " " );

  num = case_info->frame_num > 10 ? 10 : case_info->frame_num;
  WriteInfo(case_info->slice_num, num, cp);
  WriteInfo(case_info->ppin_luma_size, num, cp);
  WriteInfo(case_info->bit_depth_y_minus8, num, cp);
  WriteInfo(case_info->bit_depth_c_minus8, num, cp);
  WriteInfo(case_info->blackwhite_e, num, cp);
  WriteInfo(case_info->num_tile_cols_8k, num, cp);
  WriteInfo(case_info->fieldpic_flag, num, cp);
  WriteInfo(case_info->fieldmode, num, cp);

  if(case_info->frame_num)
    fprintf(cp,"%u|",case_info->bitrate/case_info->frame_num);
  else
    fprintf(cp,"%s|", "invalid");

  fprintf(cp, "%s|%s|%s|%s|%s|%s%s%s%s%s%s%s%s%s%s", case_info->crop_flag? "x":" ", case_info->chroma_flag? "x":" ",
          case_info->depth_flag?  "x":" ", case_info->resolution_flag? "x":" ", case_info->show_existing_frame? "x":" ",
          case_info->interlace_flag? "interlace ":"", case_info->intrabc_flag? "intrabc ":"",
          case_info->tile_flag? "tile ":"", case_info->aso_flag? "aso ":"",
          case_info->fmo_flag? "fmo ":"", case_info->pjpeg_flag? "pjpeg ":"",
          case_info->interintra ? "interintra ":"", case_info->palette_mode ? "palette_mode ":"",
          case_info->filter_intra_pred ? "filter_intra_pred ":"", case_info->intra_edge_filter? "intra_edge_filter ":"");

  if(!case_info->interlace_flag && !case_info->intrabc_flag && !case_info->tile_flag && !case_info->aso_flag
     && !case_info->fmo_flag && !case_info->pjpeg_flag && !case_info->thumb_flag && !case_info->cavlc_flag
     && !case_info->interintra && !case_info->palette_mode && !case_info->filter_intra_pred
     && !case_info->intra_edge_filter)
     fprintf(cp, "%s", " ");
  if(cp) fclose(cp);
}