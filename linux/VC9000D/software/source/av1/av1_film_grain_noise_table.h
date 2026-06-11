/*-------------------------------------------------------------------------------
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
--------------------------------------------------------------------------------*/

#ifndef __FILM_GRAIN_NOISE_TABLE_H__
#define __FILM_GRAIN_NOISE_TABLE_H__

#include "basetype.h"

void GenerateLumaGrainBlock(int luma_grain_block[][82], int bitdepth,
                            u8 num_y_points, int grain_scale_shift,
                            int ar_coeff_lag, int ar_coeffs_y[],
                            int ar_coeff_shift, int grain_min, int grain_max,
                            u16 random_seed);

void GenerateChromaGrainBlock(
    int luma_grain_block[][82], int cb_grain_block[][44],
    int cr_grain_block[][44], int bitdepth, u8 num_y_points, u8 num_cb_points,
    u8 num_cr_points, int grain_scale_shift, int ar_coeff_lag,
    int ar_coeffs_cb[], int ar_coeffs_cr[], int ar_coeff_shift, int grain_min,
    int grain_max, u8 chroma_scaling_from_luma, u16 random_seed);

#endif  // __FILM_GRAIN_DEC_H__
