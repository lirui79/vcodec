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
--------------------------------------------------------------------------------
--
-- Description : HEVC SEI Messages.
--
------------------------------------------------------------------------------*/
#ifdef HDR_HELPER_SUPPORT

#include "string.h"
#include "osal.h"
#include "sw_put_bits.h"
#include "vcenc_hdr_info.h"

static u8 bit_width_processing_windows[11] = {16, 16, 16, 16, 16, 16, 8, 16, 16, 16, 1};

static int _putHdr10(HDR10Info *hdr10_info, u32 buffer_size, u8 *buffer) {
  struct buffer sei_stream;
  struct buffer *sp = &sei_stream;
  u32 stream_cnt = 0;

  memset(sp, 0, sizeof(struct buffer));
  sei_stream.stream = (u8 *)buffer;
  sei_stream.size = buffer_size;
  sei_stream.cnt = &stream_cnt;

  u32 *p_vb1;
  int i, j, t;
  u32 len_tmp;
  // start from [hdr_info][itu_t_t35_country_code] - [hdr_info][num_windows] 28:42 Byte
  put_bit_32(sp, hdr10_info->hdr10_attr.itu_t_t35_country_code, 8);
  put_bit_32(sp, hdr10_info->hdr10_attr.itu_t_t35_terminal_provider_code, 16);
  put_bit_32(sp, hdr10_info->hdr10_attr.itu_t_t35_terminal_provider_oriented_code, 16);
  put_bit_32(sp, hdr10_info->hdr10_attr.application_identifier, 8);
  put_bit_32(sp, hdr10_info->hdr10_attr.application_version, 8);
  put_bit_32(sp, hdr10_info->hdr10_attr.num_windows, 2);

  // [hdr_info][processing_windows]
  p_vb1 = &(hdr10_info->hdr10_attr.processing_windows->window_upper_left_corner_x);
  len_tmp = sizeof(bit_width_processing_windows) * (hdr10_info->hdr10_attr.num_windows - 1);
  for (i = 0; i < len_tmp; i++, p_vb1++) {
    put_bit_32(sp, *p_vb1, bit_width_processing_windows[i % 11]);
  }

  // start from [hdr_info][targeted_system_display_maximum_luminance] to [hdr_info][targeted_system_display_maximum_luminance] 40:51 Bytes
  put_bit_32(sp, hdr10_info->hdr10_attr.targeted_system_display_maximum_luminance, 27);
  put_bit_32(sp, hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance_flag, 1);

  // [hdr_info][targeted_system_display_actual_peak_luminance]
  if (hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance_flag) {
    put_bit_32(sp,
               hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance
                   .num_rows_targeted_system_display_actual_peak_luminance,
               5);
    put_bit_32(sp,
               hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance
                   .num_cols_targeted_system_display_actual_peak_luminance,
               5);
    len_tmp = hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance
                  .num_rows_targeted_system_display_actual_peak_luminance *
              hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance
                  .num_cols_targeted_system_display_actual_peak_luminance;
    // process SEI_info[targeted_system_display_actual_peak_luminance][targeted_system_display_actual_peak_luminance],yeah,same name
    p_vb1 = hdr10_info->hdr10_attr.targeted_system_display_actual_peak_luminance
                .targeted_system_display_actual_peak_luminance;
    for (i = 0; i < len_tmp; i++, p_vb1++) {
      put_bit_32(sp, *p_vb1, 4);  //fixed value for targeted_system_display_actual_peak_luminance
    }
  }

  // process [hdr_info][distribution], it's different form above, 71:92 Bytes
  t = 0;
  for (i = 0; i < hdr10_info->hdr10_attr.num_windows; i++) {
    for (j = 0; j < 3; j++) {
      put_bit_32(sp, hdr10_info->hdr10_attr.distribution.maxscl[j + 3 * i], 17);
    }
    put_bit_32(sp, hdr10_info->hdr10_attr.distribution.average_maxrgb[i], 17);
    put_bit_32(sp, hdr10_info->hdr10_attr.distribution.num_distribution_maxrgb_percentiles[i], 4);
    for (j = 0; j < hdr10_info->hdr10_attr.distribution.num_distribution_maxrgb_percentiles[i]; j++, t++) {
      put_bit_32(sp, hdr10_info->hdr10_attr.distribution.distribution_maxrgb_percentages[t], 7);
      put_bit_32(sp, hdr10_info->hdr10_attr.distribution.distribution_maxrgb_percentiles[t], 17);
    }
    put_bit_32(sp, hdr10_info->hdr10_attr.distribution.fraction_bright_pixels[i], 10);
  }

  // start from [hdr_info][mastering]
  put_bit_32(sp, hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance_flag, 1);
  if (hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance_flag) {
    put_bit_32(sp, hdr10_info->hdr10_attr.mastering.num_rows_mastering_display_actual_peak_luminance, 5);
    put_bit_32(sp, hdr10_info->hdr10_attr.mastering.num_cols_mastering_display_actual_peak_luminance, 5);
    len_tmp = hdr10_info->hdr10_attr.mastering.num_rows_mastering_display_actual_peak_luminance *
              hdr10_info->hdr10_attr.mastering.num_cols_mastering_display_actual_peak_luminance;
    p_vb1 = hdr10_info->hdr10_attr.mastering.mastering_display_actual_peak_luminance;
    for (i = 0; i < len_tmp; i++, p_vb1++) {
      put_bit_32(sp, *p_vb1, 4);
    }
  }

  // process [hdr_info][bezier_curve]
  t = 0;
  for (i = 0; i < hdr10_info->hdr10_attr.num_windows; i++) {
    put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.tone_mapping_flag[i], 1);
    if (1 == hdr10_info->hdr10_attr.bezier_curve.tone_mapping_flag[i]) {
      put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.knee_point_x[i], 12);
      put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.knee_point_y[i], 12);
      put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.num_bezier_curve_anchors[i], 4);
      for (j = 0; j < hdr10_info->hdr10_attr.bezier_curve.num_bezier_curve_anchors[i]; j++, t++) {
        put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.bezier_curve_anchors[t], 10);
      }
    }
    put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.color_saturation_mapping_flag[i], 1);
    if (1 == hdr10_info->hdr10_attr.bezier_curve.color_saturation_mapping_flag[i]) {
      put_bit_32(sp, hdr10_info->hdr10_attr.bezier_curve.color_saturation_weight[i], 6);
    }
  }

  rbsp_trailing_bits(sp);
#ifdef DEBUG_JSON
  printf("totle num: --- \n%u BYtes, cache= 0x%08x\n", *sp->cnt, sp->cache);
  FILE *outfile = fopen("out_json_hdr.bin", "ab");
  if (outfile == NULL) {
    return -1;
  }

  fwrite(buffer, sizeof(u8), *sp->cnt, outfile);
  fclose(outfile);
#endif
  int payloadSize = *sp->cnt;
  return payloadSize;
}

static int _putDolbyVision(HDR10Info *hdr10_info, u32 buffer_size, u8 *buffer) {
  struct buffer sei_stream;
  struct buffer *sp = &sei_stream;
  u32 stream_cnt = 0;

  memset(sp, 0, sizeof(struct buffer));
  sei_stream.stream = (u8 *)buffer;
  sei_stream.size = buffer_size;
  sei_stream.cnt = &stream_cnt;

  u32 len_tmp = 0;
  // start from [hdr_info][itu_t_t35_country_code] - [hdr_info][num_ext_blocks]
  put_bit_32(sp, hdr10_info->dolby_attr.itu_t_t35_country_code, 8);
  put_bit_32(sp, hdr10_info->dolby_attr.itu_t_t35_terminal_provider_code, 16);
  put_bit_32(sp, hdr10_info->dolby_attr.itu_t_t35_terminal_provider_oriented_code, 32);
  put_bit_32(sp, hdr10_info->dolby_attr.data_type_code, 8);
  put_bit_ue(sp, hdr10_info->dolby_attr.application_identifier);
  put_bit_ue(sp, hdr10_info->dolby_attr.application_version);
  put_bit_32(sp, hdr10_info->dolby_attr.metadata_refrensh_flag, 1);
  put_bit_ue(sp, hdr10_info->dolby_attr.metadata.num_ext_blocks);
  printf("[num_ext_blocks] %u\n", hdr10_info->dolby_attr.metadata.num_ext_blocks);
  if (sp->bit_cnt % 8) {
    put_bit(sp, 0, (8 - sp->bit_cnt % 8));
  }

  for (int i = 0; i < hdr10_info->dolby_attr.metadata.num_ext_blocks; i++) {
    printf("[ext_block_length] %u\n", hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length);
    put_bit_ue(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length);
    put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_level, 8);
    len_tmp = 8 * hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_length;
    switch (hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_level) {
      case 1:
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].min_PQ, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].max_PQ, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].avg_PQ, 12);
        len_tmp = len_tmp - 36;
        ASSERT(len_tmp > 0);
        put_bit_32(sp, 0, len_tmp);
        break;
      case 2:
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].target_max_PQ, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_slope, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_offset, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_power, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_chroma_weight, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].trim_saturation_gain, 12);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ms_weight, 13);
        len_tmp = len_tmp - 85;
        ASSERT(len_tmp > 0);
        put_bit_32(sp, 0, len_tmp);
        break;
      case 5:
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_left_offset, 13);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_right_offset, 13);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_top_offset, 13);
        put_bit_32(sp, hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].active_area_bottom_offset, 13);
        len_tmp = len_tmp - 52;
        ASSERT(len_tmp > 0);
        put_bit_32(sp, 0, len_tmp);
        break;
      default:
        printf("this type %u is not be process!!\n",
               hdr10_info->dolby_attr.metadata.ext_dm_data_block[i].ext_block_level);
        break;
    }
  }
  if (sp->bit_cnt % 8) {
    put_bit(sp, 0, (8 - sp->bit_cnt % 8));
  }
  put_bit(sp, 0xff, 8);
  rbsp_flush_bits(sp);
  // rbsp_trailing_bits(sp);
#ifdef DEBUG_JSON
  printf("totle num: --- \n%u BYtes, cache= 0x%08x\n", *sp->cnt, sp->cache);
  FILE *outfile = fopen("out_json_dolby.bin", "ab");
  if (outfile == NULL) {
    return -1;
  }

  fwrite(buffer, sizeof(u8), *sp->cnt, outfile);
  fclose(outfile);
#endif

  int payloadSize = (int)(*sp->cnt);
  return payloadSize;

}

int VCEncPutHDR10Info(HDR10Info *hdr10_info, u32 buffer_size, u8 *buffer, u8 hdrType) {
  int payloadSize = 0;
  if (HDR_TYPE_DOLBY_VISION == hdrType) {
    payloadSize = _putDolbyVision(hdr10_info, buffer_size, buffer);
  } else {
    payloadSize = _putHdr10(hdr10_info, buffer_size, buffer);
  }
  return payloadSize;
}
#endif  //HDR_HELPER_SUPPORT end
