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

#include "vcd_dec_hdr.h"
#include "vcd_tools.h"

void ParseHDR10PlusFromT35Buffer(struct StrmData *strm_desc, T35_HDR10Plus *t35_hdr10plus) {
  u32 j, w, row, col;

  t35_hdr10plus->application_identifier = SwGetBits(strm_desc, 8);
  t35_hdr10plus->application_version = SwGetBits(strm_desc, 8);
  t35_hdr10plus->num_windows = MIN(SwGetBits(strm_desc, 2), 4);
  for(w=0; w<t35_hdr10plus->num_windows; w++) {
    t35_hdr10plus->window_upper_left_corner_x[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->window_upper_left_corner_y[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->window_lower_right_corner_x[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->window_lower_right_corner_y[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->center_of_ellipse_x[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->center_of_ellipse_y[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->rotation_angle[w] = SwGetBits(strm_desc, 8);
    t35_hdr10plus->semimajor_axis_internal_ellipse[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->semimajor_axis_external_ellipse[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->semiminor_axis_external_ellipse[w] = SwGetBits(strm_desc, 16);
    t35_hdr10plus->overlap_process_option[w] = SwGetBits(strm_desc, 1);
  }
  t35_hdr10plus->targeted_system_display_maximum_luminance = SwGetBits(strm_desc, 27);
  t35_hdr10plus->targeted_system_display_actual_peak_luminance_flag = SwGetBits(strm_desc, 1);
  if(t35_hdr10plus->targeted_system_display_actual_peak_luminance_flag) {
    u32 rows = t35_hdr10plus->num_rows_targeted_system_display_actual_peak_luminance = MIN(SwGetBits(strm_desc, 5), 32);
    u32 cols = t35_hdr10plus->num_cols_targeted_system_display_actual_peak_luminance = MIN(SwGetBits(strm_desc, 5), 32);
    for(row=0; row<rows; row++) {
      for(col=0; col<cols; col++)
        t35_hdr10plus->targeted_system_display_actual_peak_luminance[row][col] = SwGetBits(strm_desc, 4);
    }
    for(w=0; w<t35_hdr10plus->num_windows; w++) {
      for(j=0; j<3; j++)
        t35_hdr10plus->maxscl[w][j] = SwGetBits(strm_desc, 17);
      t35_hdr10plus->average_maxrgb[w] = SwGetBits(strm_desc, 17);
      t35_hdr10plus->num_distribution_maxrgb_percentiles[w] = MIN(SwGetBits(strm_desc, 4), 16);
      for(j=0; j<t35_hdr10plus->num_distribution_maxrgb_percentiles[w]; j++) {
        t35_hdr10plus->distribution_maxrgb_percentages[w][j] = SwGetBits(strm_desc, 7);
        t35_hdr10plus->distribution_maxrgb_percentiles[w][j] = SwGetBits(strm_desc, 17);
      }
      t35_hdr10plus->fraction_bright_pixels[w] = SwGetBits(strm_desc, 10);
    }
  }
  t35_hdr10plus->mastering_display_actual_peak_luminance_flag = SwGetBits(strm_desc, 1);
  if(t35_hdr10plus->mastering_display_actual_peak_luminance_flag) {
    u32 rows = t35_hdr10plus->num_rows_mastering_display_actual_peak_luminance = MIN(SwGetBits(strm_desc, 5), 32);
    u32 cols = t35_hdr10plus->num_cols_mastering_display_actual_peak_luminance = MIN(SwGetBits(strm_desc, 5), 32);
    for(row=0; row<rows; row++) {
      for(col=0; col<cols; col++)
        t35_hdr10plus->mastering_display_actual_peak_luminance[row][col] = SwGetBits(strm_desc, 4);
    }
    for(w=0; w<t35_hdr10plus->num_windows; w++) {
      t35_hdr10plus->tone_mapping_flag[w] = SwGetBits(strm_desc, 1);
      if(t35_hdr10plus->tone_mapping_flag[w]) {
        t35_hdr10plus->knee_point_x[w] = SwGetBits(strm_desc, 12);
        t35_hdr10plus->knee_point_y[w] = SwGetBits(strm_desc, 12);
        t35_hdr10plus->num_bezier_curve_anchors[w] = MIN(SwGetBits(strm_desc, 4), 16);
        for(j=0; j<t35_hdr10plus->num_bezier_curve_anchors[w]; j++)
          t35_hdr10plus->bezier_curve_anchors[w][j] = SwGetBits(strm_desc, 10);
      }
      t35_hdr10plus->color_saturation_mapping_flag[w] = SwGetBits(strm_desc, 1);
      if(t35_hdr10plus->color_saturation_mapping_flag[w])
        t35_hdr10plus->color_saturation_weight[w] = SwGetBits(strm_desc, 6);
    }
  }
}

u32 ParseDoblyVisionFromT35Buffer(struct StrmData *strm_desc, T35_Dobly_Vision *t35_dobly_vision) {
  u32 tmp = HANTRO_OK, value, i;

  /* app_identifier */
  tmp = VcdExpGolombUnsigned(strm_desc, &value);
  if (tmp != HANTRO_OK) return (tmp);
  t35_dobly_vision->app_identifier = value;
  /* app_version */
  tmp = VcdExpGolombUnsigned(strm_desc, &value);
  if (tmp != HANTRO_OK) return (tmp);
  t35_dobly_vision->app_version = value;

  t35_dobly_vision->metadata_refresh_flag = SwGetBits(strm_desc, 1);
  if(t35_dobly_vision->metadata_refresh_flag) {
    /* num_ext_blocks */
    tmp = VcdExpGolombUnsigned(strm_desc, &value);
    if (tmp != HANTRO_OK) return (tmp);
    t35_dobly_vision->num_ext_blocks = value;

    if(t35_dobly_vision->num_ext_blocks) {
      // byte align
      SwFlushBits(strm_desc, 8 - strm_desc->bit_pos_in_word);
      for(i=0; i<t35_dobly_vision->num_ext_blocks; i++) {
        /* ext_block_length */
        tmp = VcdExpGolombUnsigned(strm_desc, &value);
        if (tmp != HANTRO_OK) return (tmp);
        u16 ext_block_length = t35_dobly_vision->ext_block_length[i] = value;

        /* ext_block_level */
        tmp = VcdExpGolombUnsigned(strm_desc, &value);
        if (tmp != HANTRO_OK) return (tmp);
        u16 ext_block_level = t35_dobly_vision->ext_block_level[i] = value;
        u32 ext_block_len_bits = 8 * ext_block_length;
        u32 ext_block_use_bits = 0;
        if(ext_block_level == 1) {
          t35_dobly_vision->min_PQ[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->max_PQ[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->avg_PQ[i] = SwGetBits(strm_desc, 12);
          ext_block_use_bits += 36;
        }
        else if(ext_block_level == 2) {
          t35_dobly_vision->target_max_PQ[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->trim_slope[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->trim_offset[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->trim_power[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->trip_chroma_weight[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->trim_saturation_grin[i] = SwGetBits(strm_desc, 12);
          t35_dobly_vision->ms_weight[i] = SwGetBits(strm_desc, 13);
          ext_block_use_bits += 85;
        }
        else if(ext_block_level == 5) {
          t35_dobly_vision->active_area_left_offset[i] = SwGetBits(strm_desc, 13);
          t35_dobly_vision->active_area_right_offset[i] = SwGetBits(strm_desc, 13);
          t35_dobly_vision->active_area_top_offset[i] = SwGetBits(strm_desc, 13);
          t35_dobly_vision->active_area_bottom_offset[i] = SwGetBits(strm_desc, 13);
          ext_block_use_bits += 52;
        }
        while(ext_block_use_bits++ < ext_block_len_bits)
          SwFlushBits(strm_desc, 1);
      }
    }
  }
  // byte align, here codes can be removed
  SwFlushBits(strm_desc, 8 - strm_desc->bit_pos_in_word);

  return tmp;
}

i32 VcdParseT35HDR(struct T35Parameters *t35_param, T35_HDR_Param *t35_hdr_parma) {
  u32 i, buffer_size, ret = HANTRO_OK;
  u8 *buffer_start = NULL;
  ASSERT(t35_param);
  ASSERT(t35_hdr_parma);
  ASSERT(t35_param->counter <= MAX_PAYLOAD_NUM);

  t35_hdr_parma->hdr10plus_counter = 0;
  t35_hdr_parma->dobly_vision_counter = 0;
  buffer_start = t35_param->payload_byte.buffer;
  for(i=0; i<t35_param->counter; i++) {
    buffer_size = t35_param->payload_byte_length[i];
    if(!buffer_size) continue; //no need to parse for trailing bit
    struct StrmData strm_desc = {buffer_start, buffer_start, 0, buffer_size, buffer_size, 0};

    u32 country_code = t35_param->itu_t_t35_country_code[i];
    u32 terminal_provided_code = SwGetBits(&strm_desc, 16);
    u32 terminal_provided_oriented_code = SwGetBits(&strm_desc, 16);

    if(country_code == 0xB5) {
      if((terminal_provided_code==0x3C) && (terminal_provided_oriented_code==0x1)) {
        T35_HDR10Plus *t35_hdr10plus = t35_hdr_parma->t35_hdr10plus;
        u32 index = t35_hdr_parma->hdr10plus_counter;
        t35_hdr10plus[index].itu_t_t35_country_code = 0xB5;
        t35_hdr10plus[index].itu_t_t35_terminal_provider_code = 0x3C;
        t35_hdr10plus[index].itu_t_t35_terminal_provider_oriented_code = 0x1;
        ParseHDR10PlusFromT35Buffer(&strm_desc, t35_hdr10plus);
        t35_hdr_parma->hdr10plus_counter++;
      }
      else if ((terminal_provided_code==0x3B) && (terminal_provided_oriented_code==0x0))  {
        T35_Dobly_Vision *t35_dobly_vision = t35_hdr_parma->t35_dobly_vision;
        u32 index = t35_hdr_parma->dobly_vision_counter;
        t35_dobly_vision[index].itu_t_t35_country_code = 0xB5;
        t35_dobly_vision[index].itu_t_t35_terminal_provider_code = 0x3B;
        t35_dobly_vision[index].itu_t_t35_terminal_provider_oriented_code = 0x0;
        ParseDoblyVisionFromT35Buffer(&strm_desc, t35_dobly_vision);
        t35_hdr_parma->dobly_vision_counter++;
      }
    }

    buffer_start += t35_param->payload_byte_length[i];
  }
  if((t35_hdr_parma->hdr10plus_counter==0) && (t35_hdr_parma->dobly_vision_counter==0))
    ret = HANTRO_NOK;

  return ret;
}
