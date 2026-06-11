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

#ifndef VCD_DEC_HDR_H_
#define VCD_DEC_HDR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "basetype.h"
#include "decsei.h"
#include "vcd_tools.h"

/**
 * ATSC Candidate Standard: A/341 Amendment – 2094-40
 * user_data_registered_itu_t_t35() of 2094-40 metadata message, which is in order to provide
 * dynamic information about the video signal, also meams "Dynamic Metadata for Color Vlume Trancform"
 */
typedef struct ITU_T_T35_HDR10PLUS {
  u8 itu_t_t35_country_code;
  u16 itu_t_t35_terminal_provider_code;
  u16 itu_t_t35_terminal_provider_oriented_code;
  u8 application_identifier;
  u8 application_version;
  u8 num_windows; /* just 2 bits are valid */
  u16 window_upper_left_corner_x[4];
  u16 window_upper_left_corner_y[4];
  u16 window_lower_right_corner_x[4];
  u16 window_lower_right_corner_y[4];
  u16 center_of_ellipse_x[4];
  u16 center_of_ellipse_y[4];
  u8 rotation_angle[4];
  u16 semimajor_axis_internal_ellipse[4];
  u16 semimajor_axis_external_ellipse[4];
  u16 semiminor_axis_external_ellipse[4];
  u8 overlap_process_option[4]; /* only 1 bit is valid */
  u32 targeted_system_display_maximum_luminance;  /* only 27 bits are valid */
  u8 targeted_system_display_actual_peak_luminance_flag; /* only 1 bit is valid */
  u8 num_rows_targeted_system_display_actual_peak_luminance; /* only 5 bits are valid */
  u8 num_cols_targeted_system_display_actual_peak_luminance; /* only 5 bits are valid */
  u8 targeted_system_display_actual_peak_luminance[32][32]; /* only 4 bits are valid */
  u32 maxscl[4][3]; /* only 17 bits are valid */
  u32 average_maxrgb[4]; /* only 17 bits are valid */
  u8 num_distribution_maxrgb_percentiles[4];  /* just 4 bits are valid */
  u8 distribution_maxrgb_percentages[4][16]; /* just 7 bits are valid */
  u32 distribution_maxrgb_percentiles[4][16]; /* just 17 bits are valid */
  u16 fraction_bright_pixels[4]; /* just 10 bits are valid */
  u8 mastering_display_actual_peak_luminance_flag; /* just 1 bit is valid */
  u8 num_rows_mastering_display_actual_peak_luminance; /* just 5 bits are valid */
  u8 num_cols_mastering_display_actual_peak_luminance; /* just 5 bits are valid */
  u8 mastering_display_actual_peak_luminance[32][32]; /* just 4 bits are valid */
  u8 tone_mapping_flag[4]; /* just 1 bit is valid */
  u16 knee_point_x[4]; /* just 12 bits are valid */
  u16 knee_point_y[4]; /* just 12 bits are valid */
  u8 num_bezier_curve_anchors[4]; /* just 4 bits are valid */
  u16 bezier_curve_anchors[4][16]; /* just 10 bits are valid */
  u8 color_saturation_mapping_flag[4]; /* just 1 bit is valid */
  u8 color_saturation_weight[4]; /* just 6 bits are valid */
} T35_HDR10Plus;

#define MAX_BLOCK_LEVEL 254

typedef struct ITU_T_T35_Dobly_Vision {
  u8 itu_t_t35_country_code;
  u16 itu_t_t35_terminal_provider_code;
  u16 itu_t_t35_terminal_provider_oriented_code;
  u32 app_identifier; //ue(v)
  u32 app_version; //ue(v)
  u8 metadata_refresh_flag; //u(1)
  u8 num_ext_blocks; //ue(v)
  u32 ext_block_length[MAX_BLOCK_LEVEL]; //ue(v)
  u8 ext_block_level[MAX_BLOCK_LEVEL]; //u(8)
  /* ext_block_level[i] == 1 */
  u16 min_PQ[MAX_BLOCK_LEVEL]; //u(12)
  u16 max_PQ[MAX_BLOCK_LEVEL];
  u16 avg_PQ[MAX_BLOCK_LEVEL];
  /* ext_block_level[i] == 2 */
  u16 target_max_PQ[MAX_BLOCK_LEVEL];  //u(12)
  u16 trim_slope[MAX_BLOCK_LEVEL];
  u16 trim_offset[MAX_BLOCK_LEVEL];
  u16 trim_power[MAX_BLOCK_LEVEL];
  u16 trip_chroma_weight[MAX_BLOCK_LEVEL];
  u16 trim_saturation_grin[MAX_BLOCK_LEVEL];
  i16 ms_weight[MAX_BLOCK_LEVEL];  //i(13)
  /* ext_block_level[i] == 5 */
  u16 active_area_left_offset[MAX_BLOCK_LEVEL];  //u(13)
  u16 active_area_right_offset[MAX_BLOCK_LEVEL];
  u16 active_area_top_offset[MAX_BLOCK_LEVEL];
  u16 active_area_bottom_offset[MAX_BLOCK_LEVEL];
} T35_Dobly_Vision;

typedef struct T35_HDR_Param {
  T35_HDR10Plus *t35_hdr10plus;
  u32 hdr10plus_counter;
  T35_Dobly_Vision *t35_dobly_vision;
  u32 dobly_vision_counter;
} T35_HDR_Param;

/**
 * \brief Output structure for T35_HDR_Param.
 * parse all T35_HDR_Param of one output pciture. \n
 * \ingroup tbcommon_group *
 * \param [in]    t35_param  Pointer to a T35Parameters structure
 * \param [out]   t35_hdr_parma  Pointer to a T35_HDR_Param structure
 * \param [out]   i32  value: HANTRO_OK, HANTRO_NOK(no fill t35_hdr_parma)
 */
i32 VcdParseT35HDR(struct T35Parameters *t35_param, T35_HDR_Param *t35_hdr_parma);

#ifdef __cplusplus
}
#endif

#endif /* #ifdef VCD_DEC_HDR_H_ */
