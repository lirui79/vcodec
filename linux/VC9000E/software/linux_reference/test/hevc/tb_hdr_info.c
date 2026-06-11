/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
-- Description : HEVC SEI Messages.
--
------------------------------------------------------------------------------*/
#ifdef HDR_HELPER_SUPPORT

#include "osal.h"

#include "frozen.h"
#include "tb_hdr_info.h"

/**
 * \brief Final used this data to keep all related info for multi-frames
 */
typedef struct {
  /** Json_info[] in top level of json file first level */
  char *json_info;
  /** all content of json file */
  char *json_content;
  /** string which content multi-frames SEI info */
  char *hdr_info_str;
  /**  as SEI_num to record which information is processed now. */
  u32 info_num;
  /** previous and current HDR informatin */
  HDR10Info hdr_info[2];
  /** number of hdr_info parsed, not use, keep for future */
  u32 num_hdr_info;
} HDR10Data;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define JSON_INT_TO_STRUCT(cur_str, name, value, length) \
  do {                                                   \
    json_scanf(cur_str, strlen(cur_str), name, &value);  \
    bit_cnt += length;                                   \
  } while (0)

#define JSON_UE_TO_STRUCT(cur_str, name, value)         \
  do {                                                  \
    json_scanf(cur_str, strlen(cur_str), name, &value); \
    bit_cnt += _get_ue_bit_num(value);                  \
  } while (0)

#define VAL_TO_STRUCT(dest, value, length) \
  do {                                     \
    dest = value;                          \
    bit_cnt += length;                     \
  } while (0)

static u8 bit_width_processing_windows[11] = {16, 16, 16, 16, 16, 16, 8, 16, 16, 16, 1};

int get_array(char *str_in, struct json_token *t_array, int val[]) {
  int i;
  for (i = 0; json_scanf_array_elem(str_in, strlen(str_in), "", i, t_array) > 0; i++) {
    char s[3] = "";
    strncpy(s, t_array->ptr, t_array->len);
    val[i] = atoi(s);
  }
  return i;
}

char *_getSEIArray(char *json_content, char *Json_info) {
  char *SEI_array_str;
  json_scanf(json_content, strlen(json_content), "{ Json_info:%Q, SEI_info:%Q }", &Json_info,
             &SEI_array_str);
  return SEI_array_str;
}

static u32 _get_ue_bit_num(i32 val) {
  i32 tmp = 0;

  ASSERT(val >= 0);
  val++;
  while (val >> ++tmp)
    ;
  tmp = tmp * 2 - 1;
  return tmp;
}

static int _parseHDR10Data(HDR10Info *hdr10_info, struct json_token *ptoken_sei) {
  static char *distribute_item_name_str[6] = {"{ maxscl:%Q }",
                                              "{ average_maxrgb:%Q }",
                                              "{ num_distribution_maxrgb_percentiles:%Q }",
                                              "{ distribution_maxrgb_percentages:%Q }",
                                              "{ distribution_maxrgb_percentiles:%Q }",
                                              "{ fraction_bright_pixels:%Q }"};
  static char *bezier_curve_item_name_str[7] = {
      "{ tone_mapping_flag:%Q }",      "{ knee_point_x:%Q }",
      "{ knee_point_y:%Q }",           "{ num_bezier_curve_anchors:%Q }",
      "{ bezier_curve_anchors:%Q }",   "{ color_saturation_mapping_flag:%Q }",
      "{ color_saturation_weight:%Q }"};
  struct json_token token_array;
  int len_tmp;
  int arr_value[7][45];  //column and row num use max of distribute[6][45] and bezier_curve[7][9]
  char *tmp_item_str = NULL;
  u32 *p_tmp_vb1, *p_tmp_vb2;
  u32 num_windows;
  int flag_tmp;
  u32 bit_cnt;
  int t = 0;

  bit_cnt = 0;  // init bit length summary

  // because it's HDR10 plus type, so set type value = 1
  hdr10_info->hdr_type = HDR_TYPE_HDR10_PLUS;

  // process SEI_info[itu_t_t35_country_code] to SEI_info[num_windows]
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ frame_num:%d }", hdr10_info->frame_num, 0);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ itu_t_t35_country_code:%d }",
                     hdr10_info->hdr10_attr.itu_t_t35_country_code, 8);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ itu_t_t35_terminal_provider_code:%d }",
                     hdr10_info->hdr10_attr.itu_t_t35_terminal_provider_code, 16);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ itu_t_t35_terminal_provider_oriented_code:%d }",
                     hdr10_info->hdr10_attr.itu_t_t35_terminal_provider_oriented_code, 16);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ application_identifier:%d }",
                     hdr10_info->hdr10_attr.application_identifier, 8);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ application_version:%d }", hdr10_info->hdr10_attr.application_version,
                     8);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ num_windows:%d }", hdr10_info->hdr10_attr.num_windows, 2);
  num_windows = hdr10_info->hdr10_attr.num_windows;

  // process SEI_info[processing_windows]
  char *processing_windows = NULL;
  json_scanf(ptoken_sei->ptr, strlen(ptoken_sei->ptr), "{ processing_windows:%Q }",
             &processing_windows);
  len_tmp = get_array(processing_windows, &token_array, arr_value[0]);
  if (0 == len_tmp) {
    printf("error happen! not get distribute item value! \n");
    return -1;
  }
#ifdef DEBUG_JSON1
  printf("\nprocessing_windows: %d\n", len_tmp);
  for (int i = 0; i < len_tmp; i++) {
    printf("%d ", arr_value[0][i]);
  }
  printf("\n");
#endif
  p_tmp_vb1 = &(hdr10_info->hdr10_attr.processing_windows->window_upper_left_corner_x);
  for (int i = 0; i < len_tmp; i++, p_tmp_vb1++) {
    VAL_TO_STRUCT(*p_tmp_vb1, arr_value[0][i], bit_width_processing_windows[i % 11]);
  }
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ targeted_system_display_maximum_luminance:%d }",
                     hdr10_info->hdr10_attr.targeted_system_display_maximum_luminance, 27);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ targeted_system_display_actual_peak_luminance_flag:%d }",
                     hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance_flag, 1);
  flag_tmp = hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance_flag;
#ifdef DEBUG_JSON1
  printf("targeted_system_display_actual_peak_luminance_flag = %d\n", flag_tmp);
#endif
  if (flag_tmp) {
    char *targeted_system_display_actual_peak_luminance = NULL;

    // process SEI_info[targeted_system_display_actual_peak_luminance]
    json_scanf(ptoken_sei->ptr, strlen(ptoken_sei->ptr),
               "{ targeted_system_display_actual_peak_luminance:%Q }",
               &targeted_system_display_actual_peak_luminance);
    // process SEI_info[targeted_system_display_actual_peak_luminance][num*]
    PeakLum *p_tmp_pl = &hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance;
    JSON_INT_TO_STRUCT(targeted_system_display_actual_peak_luminance,
                       "{ num_rows_targeted_system_display_actual_peak_luminance:%d }",
                       p_tmp_pl->num_rows_targeted_system_display_actual_peak_luminance, 5);
    JSON_INT_TO_STRUCT(targeted_system_display_actual_peak_luminance,
                       "{ num_cols_targeted_system_display_actual_peak_luminance:%d }",
                       p_tmp_pl->num_cols_targeted_system_display_actual_peak_luminance, 5);

    // process SEI_info[targeted_system_display_actual_peak_luminance][targeted_system_display_actual_peak_luminance],yeah,same name
    char *low_luminance = NULL;
    json_scanf(targeted_system_display_actual_peak_luminance,
               strlen(targeted_system_display_actual_peak_luminance),
               "{ targeted_system_display_actual_peak_luminance:%Q }", &low_luminance);
    len_tmp = get_array(low_luminance, &token_array, arr_value[0]);
    if (0 == len_tmp) {
      printf("error happen! not get distribute item value! \n");
      return -1;
    }
    p_tmp_vb1 = p_tmp_pl->targeted_system_display_actual_peak_luminance;
    for (int i = 0; i < len_tmp; i++, p_tmp_vb1++) {
      VAL_TO_STRUCT(*p_tmp_vb1, arr_value[0][i],
                    4);  //fixed value for targeted_system_display_actual_peak_luminance
    }
#ifdef DEBUG_JSON2
    printf("\ntargeted_system_display_actual_peak_luminance: %d\n", len_tmp);
    for (int i = 0; i < len_tmp; i++) {
      printf("%u ", p_tmp_pl->targeted_system_display_actual_peak_luminance[i]);
    }
    printf("\n");
#endif
  }
  // process [SEI_info][distribution]
  char *distribution = NULL;
  json_scanf(ptoken_sei->ptr, strlen(ptoken_sei->ptr), "{ distribution:%Q }", &distribution);

  // get array value from [SEI_info][distribution][...]
  for (int i = 0; i < ARRAY_SIZE(distribute_item_name_str); i++) {
    json_scanf(distribution, strlen(distribution), distribute_item_name_str[i], &tmp_item_str);
    len_tmp = get_array(tmp_item_str, &token_array, arr_value[i]);
    if (0 == len_tmp) {
      printf("error happen! not get distribute item value! \n");
      return -1;
    }
#ifdef DEBUG_JSON3
    printf("\n%s: %d\n", distribute_item_name_str[i], len_tmp);
    for (int j = 0; j < len_tmp; j++) {
      printf("%d ", arr_value[i][j]);
    }
    printf("\n");
#endif
  }
  //put array below distribution value to struct sei
  t = 0;
  p_tmp_vb1 = hdr10_info->hdr10_attr.distribution.distribution_maxrgb_percentages;
  p_tmp_vb2 = hdr10_info->hdr10_attr.distribution.distribution_maxrgb_percentiles;
  for (int i = 0; i < num_windows; i++) {
    for (int j = 0; j < 3; j++) {
      VAL_TO_STRUCT(hdr10_info->hdr10_attr.distribution.maxscl[j + 3 * i], arr_value[0][j + 3 * i],
                    17);  //maxscl
    }
    VAL_TO_STRUCT(hdr10_info->hdr10_attr.distribution.average_maxrgb[i], arr_value[1][i],
                  17);  // average_maxrgb
    VAL_TO_STRUCT(hdr10_info->hdr10_attr.distribution.num_distribution_maxrgb_percentiles[i], arr_value[2][i],
                  4);
    for (int j = 0; j < arr_value[2][i]; j++, p_tmp_vb1++, p_tmp_vb2++, t++) {
      VAL_TO_STRUCT(*p_tmp_vb1, arr_value[3][t], 7);   //distribution_maxrgb_percentages 7
      VAL_TO_STRUCT(*p_tmp_vb2, arr_value[4][t], 17);  //distribution_maxrgb_percentiles 17
    }
    VAL_TO_STRUCT(hdr10_info->hdr10_attr.distribution.fraction_bright_pixels[i], arr_value[5][i],
                  10);  //fraction_bright_pixels 10
  }
#ifdef DEBUG_JSON4
  printf("\ndistribution.maxscl : %u\n", num_windows * 3);
  for (int j = 0; j < num_windows * 3; j++) {
    printf("%u ", hdr10_info->hdr10_attr.distribution.maxscl[j]);
  }
  printf("\ndistribution.average_maxrgb : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.distribution.average_maxrgb[j]);
  }
  printf("\ndistribution.num_distribution_maxrgb_percentiles : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.distribution.num_distribution_maxrgb_percentiles[j]);
  }
  printf("\ndistribution.distribution_maxrgb_percentage : %d\n", t);
  for (int j = 0; j < t; j++) {
    printf("%u ", hdr10_info->hdr10_attr.distribution.distribution_maxrgb_percentages[j]);
  }
  printf("\ndistribution.distribution_maxrgb_percentiles : %d\n", t);
  for (int j = 0; j < t; j++) {
    printf("%u ", hdr10_info->hdr10_attr.distribution.distribution_maxrgb_percentiles[j]);
  }
  printf("\ndistribution.fraction_bright_pixels\n");
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.distribution.fraction_bright_pixels[j]);
  }
  printf("\n");
#endif
  // process SEI_info[mastering]
  char *mastering = NULL;
  json_scanf(ptoken_sei->ptr, strlen(ptoken_sei->ptr), "{ mastering:%Q }", &mastering);
  JSON_INT_TO_STRUCT(mastering, "{ mastering_display_actual_peak_luminance_flag:%d }",
                     hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance_flag, 1);
  flag_tmp = hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance_flag;
  if (flag_tmp) {
    JSON_INT_TO_STRUCT(mastering, "{ num_rows_mastering_display_actual_peak_luminance:%d }",
                       hdr10_info->hdr10_attr.mastering.num_rows_mastering_display_actual_peak_luminance, 5);
    JSON_INT_TO_STRUCT(mastering, "{ num_cols_mastering_display_actual_peak_luminance:%d }",
                       hdr10_info->hdr10_attr.mastering.num_cols_mastering_display_actual_peak_luminance, 5);
    json_scanf(mastering, strlen(mastering), "{ mastering_display_actual_peak_luminance:%Q }",
               &tmp_item_str);
    int lumi_tmp[961];  // 0b11111 * 0b11111
    len_tmp = get_array(tmp_item_str, &token_array, lumi_tmp);
    for (int i = 0; i < len_tmp; i++) {
      VAL_TO_STRUCT(hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance[i], lumi_tmp[i],
                    4);
    }
#ifdef DEBUG_JSON4
    printf("mastering_display_actual_peak_luminance : %d\n", len_tmp);
    p_tmp_vb1 = hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance;
    for (int i = 0; i < len_tmp; i++) {
      printf("%u ", *p_tmp_vb1);
      p_tmp_vb1++;
    }
    printf("\n");
#endif
  }
  // process SEI_info[bezier_curve]
  char *bezier_curve = NULL;
  json_scanf(ptoken_sei->ptr, strlen(ptoken_sei->ptr), "{ bezier_curve:%Q }", &bezier_curve);
  // get matrix value of [SEI_info][bezier_curve][...]
  for (int i = 0; i < ARRAY_SIZE(bezier_curve_item_name_str); i++) {
    json_scanf(bezier_curve, strlen(bezier_curve), bezier_curve_item_name_str[i], &tmp_item_str);
    len_tmp = get_array(tmp_item_str, &token_array, arr_value[i]);
    if (0 == len_tmp) {
      printf("error happen! not get distribute item value! \n");
      return -1;
    }
#ifdef DEBUG_JSON5
    printf("\n%s: %d\n", bezier_curve_item_name_str[i], len_tmp);
    for (int j = 0; j < len_tmp; j++) {
      printf("%d ", arr_value[i][j]);
    }
    printf("\n");
#endif
  }

  //put array bleow bezier_curve value to struct
  t = 0;
  for (int i = 0; i < num_windows; i++) {
    VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.tone_mapping_flag[i], arr_value[0][i],
                  1);  //tone_mapping_flag 1
    if (1 == arr_value[0][i]) {
      VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.knee_point_x[i], arr_value[1][i],
                    12);  //knee_point_x 12
      VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.knee_point_y[i], arr_value[2][i],
                    12);  //knee_point_y 12
      VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.num_bezier_curve_anchors[i], arr_value[3][i],
                    4);  //num_bezier_curve_anchors 4
      for (int j = 0; j < arr_value[3][i]; j++, t++) {
        VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.bezier_curve_anchors[t], arr_value[4][t], 10);
      }
    }
    VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.color_saturation_mapping_flag[i], arr_value[5][i], 1);
    if (1 == arr_value[5][i]) {
      VAL_TO_STRUCT(hdr10_info->hdr10_attr.bezier_curve.color_saturation_weight[i], arr_value[6][i], 6);
    }
  }
#ifdef DEBUG_JSON5
  printf("\nbezier_curve.tone_mapping_flag : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.tone_mapping_flag[j]);
  }
  printf("\nbezier_curve.knee_point_x : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.knee_point_x[j]);
  }
  printf("\nbezier_curve.knee_point_y : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.knee_point_y[j]);
  }
  printf("\nbezier_curve.num_bezier_curve_anchors : %d\n", t);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.num_bezier_curve_anchors[j]);
  }
  printf("\nbezier_curve.bezier_curve_anchors : %d\n", t);
  for (int j = 0; j < t; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.bezier_curve_anchors[j]);
  }
  printf("\nbezier_curve.color_saturation_mapping_flag : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.color_saturation_mapping_flag[j]);
  }
  printf("\nbezier_curve.color_saturation_weight : %u\n", num_windows);
  for (int j = 0; j < num_windows; j++) {
    printf("%u ", hdr10_info->hdr10_attr.bezier_curve.color_saturation_weight[j]);
  }
  printf("\n");
#endif
  return 0;
}

static int _parseDolbyData(HDR10Info *hdr10_info, struct json_token *ptoken_sei) {
  struct json_token token_ext_dm;
  char *metadata_str = NULL;
  char *ext_dm_str = NULL;
  u32 bit_cnt = 0;  // init bit length summary

  // because it's dolby vision type, so set type value = 2
  hdr10_info->hdr_type = HDR_TYPE_DOLBY_VISION;

  // process SEI_info[itu_t_t35_country_code] to SEI_info[metadata_refrensh_flag]
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ frame_num:%d }", hdr10_info->frame_num, 0);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ itu_t_t35_country_code:%d }",
                     hdr10_info->dolby_attr.itu_t_t35_country_code, 8);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ itu_t_t35_terminal_provider_code:%d }",
                     hdr10_info->dolby_attr.itu_t_t35_terminal_provider_code, 16);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ itu_t_t35_terminal_provider_oriented_code:%d }",
                     hdr10_info->dolby_attr.itu_t_t35_terminal_provider_oriented_code, 32);
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ data_type_code:%d }", hdr10_info->dolby_attr.data_type_code, 8);
  JSON_UE_TO_STRUCT(ptoken_sei->ptr, "{ app_identifier:%d }",
                    hdr10_info->dolby_attr.application_identifier);                                      //TODO
  JSON_UE_TO_STRUCT(ptoken_sei->ptr, "{ app_version:%d }", hdr10_info->dolby_attr.application_version);  //TODO
  JSON_INT_TO_STRUCT(ptoken_sei->ptr, "{ metadata_refrensh_flag:%d }",
                     hdr10_info->dolby_attr.metadata_refrensh_flag, 1);

  json_scanf(ptoken_sei->ptr, strlen(ptoken_sei->ptr), "{ metadata:%Q }", &metadata_str);
  JSON_UE_TO_STRUCT(metadata_str, "{ num_ext_blocks:%d }",
                    hdr10_info->dolby_attr.metadata.num_ext_blocks);  //TODO

  json_scanf(metadata_str, strlen(metadata_str), "{ ext_dm_data_block:%Q }", &ext_dm_str);
  for (int i = 0; i < hdr10_info->dolby_attr.metadata.num_ext_blocks; i++) {
    if (json_scanf_array_elem(ext_dm_str, strlen(ext_dm_str), "", i, &token_ext_dm) > 0) {
      int tail_bit_cnt = 0;
      JSON_UE_TO_STRUCT(token_ext_dm.ptr, "{ ext_block_length:%d }",
                        hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length);  //TODO
      JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ ext_block_level:%d }",
                         hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_level, 8);  //TODO
      switch (hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_level) {
        case 1:
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ min_PQ:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].min_PQ, 12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ max_PQ:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].max_PQ, 12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ avg_PQ:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].avg_PQ, 12);  //TODO
          tail_bit_cnt = 8 * hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length - 36;
          bit_cnt += tail_bit_cnt;
          break;
        case 2:
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ target_max_PQ:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].target_max_PQ, 12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ trim_slope:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_slope, 12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ trim_offset:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_offset, 12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ trim_power:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_power, 12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ trim_chroma_weight:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_chroma_weight,
                             12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ trim_saturation_gain:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_saturation_gain,
                             12);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ ms_weight:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ms_weight, 13);  //TODO
          tail_bit_cnt = 8 * hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length - 85;
          bit_cnt += tail_bit_cnt;
          break;
        case 5:
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ active_area_left_offset:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_left_offset,
                             13);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ active_area_right_offset:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_right_offset,
                             13);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ active_area_top_offset:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_top_offset,
                             13);  //TODO
          JSON_INT_TO_STRUCT(token_ext_dm.ptr, "{ active_area_bottom_offset:%d }",
                             hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_bottom_offset,
                             13);  //TODO
          tail_bit_cnt = 8 * hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length - 52;
          bit_cnt += tail_bit_cnt;
          break;
        default:
          break;
      }
    }
  }

  return 0;
}

int _parseOneHDR10Data(char *SEI_info_str, u32 SEI_num, HDR10Info *hdr10_info) {
  struct json_token token_sei;
  u32 dolby_vision_flag = 0;
  if (json_scanf_array_elem(SEI_info_str, strlen(SEI_info_str), "", SEI_num, &token_sei) > 0) {
    json_scanf(token_sei.ptr, strlen(token_sei.ptr), "{ metadata_refrensh_flag:%d }",
               &dolby_vision_flag);
    if (dolby_vision_flag) {
      _parseDolbyData(hdr10_info, &token_sei);
    } else {
      _parseHDR10Data(hdr10_info, &token_sei);
    }
  } else {
    return -1;
  }
  return 0;
}

u8 shift_hdr_info(HDR10Info *hdr_info, HDR10Info *hdr_info_next){
  memcpy(hdr_info, hdr_info_next, sizeof(HDR10Info));
  memset(hdr_info_next, 0, sizeof(HDR10Info));
  return 0;
}

/**
 * @brief      Read designated external sei data from string which contents multi-frames SEI info
 *
 * @param[in]  hdr_info_str: string, which contents multi-frames SEI info
 * @param[out] hdr_data: save hdr data after parsed
 * @param[in]  pSEI_num: number of multi-frames SEI
 * @return 0 fro success, the other for error occurs
*/
int _readNextHdrData(HDR10Data *hdr_data) {
  // i32 byteCnt;
  // u32 seiCnt = 0;
  // u8 *data;
  i32 ret;
  HDR10Info *hdr_info_cur, *hdr_info_next;

  hdr_info_cur = &(hdr_data->hdr_info[0]);
  hdr_info_next = &(hdr_data->hdr_info[1]);
  /* Read seiCnt and struct from file */
  if(hdr_data->info_num == 0){
    //if first frame be encoded, get two SEI info as current and next
    ret = _parseOneHDR10Data(hdr_data->hdr_info_str, hdr_data->info_num, hdr_info_cur);
    if(ret < 0){
      printf("json file have no valid content!!!\n");
      return -1;
    }
    hdr_data->info_num = 1;
  }
  else{
    // next -> current
    shift_hdr_info(hdr_info_cur, hdr_info_next);
  }
  //get frame number of NEXT SEI info
  ret = _parseOneHDR10Data(hdr_data->hdr_info_str, hdr_data->info_num, hdr_info_next);
  if(ret < 0){
    hdr_info_next->frame_num = -1;
  } else {
    hdr_data->info_num += 1;
    // if next SEI_info have same frame number with current's
    if(hdr_info_next->frame_num == hdr_info_cur->frame_num) {
      if(hdr_info_next->hdr_type == HDR_TYPE_HDR10_PLUS) {  //HDR10 plus
        hdr_info_cur->hdr10_attr = hdr_info_next->hdr10_attr;
      } else { //Dolby vision
        hdr_info_cur->dolby_attr = hdr_info_next->dolby_attr;
      }
      hdr_info_cur->hdr_type = HDR_TYPE_BOTH_HDR10_DOLBY; //mix HDR10 plus and Dolby vision
      memset(hdr_info_next, 0, sizeof(HDR10Info)); //clean and read one more
      ret = _parseOneHDR10Data(hdr_data->hdr_info_str, hdr_data->info_num, hdr_info_next);
      if(ret < 0){
        hdr_info_next->frame_num = -1;
      } else {
        hdr_data->info_num += 1;
      }
    }
  }
  printf("Read next HDR10 info No.%u frame idx: %u \n", hdr_data->info_num, hdr_info_cur->frame_num);
  return 0;
}


/** get hdr information from json file
*
* \param [in] hdr_data handle for context of JSON and meta
* \param [in] frame_index the frame index where the HDR information belong to
* \return NULL if not get the correct information
* \return info pointer to structure which contain the HDR information of frame with index
 *         of frame_index. when info->frame_num < frame_index, it means the information is
*         same as frame_num.
*/

HDR10Info *VCEncMetadataGet(void *ctx, int frame_index)
{
    HDR10Data *data = (HDR10Data *)ctx;

    if (data->hdr_info[CURRENT_HDR].frame_num >= frame_index) {
        return data->hdr_info;//return current
    } else {
      //read next and return
      int ret = _readNextHdrData(data);
      if (ret != 0)
        return NULL;
      return data->hdr_info;
    }
}

/** Initial the meta data context
*
* \param [in] json_file file name of JSON which content is HDR10 Plus and Dolby Vision
* \return handle for context of JSON and meta, if success
* \return NULL, if fail
*/

void *VCEncMetadataInit(const char *json_file)
{
  HDR10Info *hdr_info_cur, *hdr_info_next;
  HDR10Data *data = (HDR10Data *)malloc(sizeof(HDR10Data));
  i32 ret;

  data->json_content = json_fread(json_file);
  data->hdr_info_str = _getSEIArray(data->json_content, data->json_info);
  data->info_num = 0;

  //read first information
  ret = _readNextHdrData(data);
  if (ret != 0)
    return NULL;

  return (void*)data;
}

/** release the meta data context
*
* \param [in] hdr_data handle for context of JSON and meta
* \return 0 if success
* \return -1 if fail
*/

int VCEncMetadataRelease(void *ctx)
{
    HDR10Data *data = (HDR10Data *)ctx;
    free(data->json_content);
    free(data);

    return 0;
}

#endif  //HDR_HELPER_SUPPORT end
