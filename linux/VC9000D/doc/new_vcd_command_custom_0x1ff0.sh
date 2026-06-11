#!/bin/bash
#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                               Verisilicon.                                 --
#-                                                                            --
#-                       (C) COPYRIGHT 2023 VERISILICON.                      --
#-                              ALL RIGHTS RESERVED                           --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------

stream_path=xxx #change to your stream path
binary_path=xxx #change to your binary path

#[CASE RV10@0 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV10@0_CModel.yuv ${stream_path}/stream_96x96_min.rv10
#[CASE RV10@1 CModel]:
${binary_path}/rvdec -N2 -A256 -n -b -OCASE_RV10@1_CModel.yuv ${stream_path}/stream_1920x1080_max.rv10
#[CASE RV10@3 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_RV10@3_CModel.yuv ${stream_path}/stream_1920x1088_sub.rv10
#[CASE RV10@4 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV10@4_CModel.yuv ${stream_path}/stream_1920x1088_per.rv10
#[CASE DIVX4@0 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX4@0_CModel.yuv ${stream_path}/stream_496x272_min.divx4
#[CASE DIVX4@1 CModel]:
${binary_path}/mpeg4dec -N2 -A256 -n -b -OCASE_DIVX4@1_CModel.yuv ${stream_path}/stream_1920x1080_max.divx4
#[CASE DIVX4@3 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_DIVX4@3_CModel.yuv ${stream_path}/stream_496x272_sub.divx4
#[CASE DIVX4@4 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX4@4_CModel.yuv ${stream_path}/stream_640x368_per.divx4
#[CASE DIVX5@0 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX5@0_CModel.yuv ${stream_path}/stream_96x112_min.divx5
#[CASE DIVX5@1 CModel]:
${binary_path}/mpeg4dec -N2 -A256 -n -b -OCASE_DIVX5@1_CModel.yuv ${stream_path}/stream_1920x1088_max.divx5
#[CASE DIVX5@2 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX5@2_CModel.yuv ${stream_path}/stream_1920x1088_err.divx5
#[CASE DIVX5@3 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_DIVX5@3_CModel.yuv ${stream_path}/stream_352x288_sub.divx5
#[CASE DIVX5@4 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX5@4_CModel.yuv ${stream_path}/stream_1920x1088_per.divx5
#[CASE DIVX6@0 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX6@0_CModel.yuv ${stream_path}/stream_96x96_min.divx6
#[CASE DIVX6@1 CModel]:
${binary_path}/mpeg4dec -N2 -A256 -n -b -OCASE_DIVX6@1_CModel.yuv ${stream_path}/stream_720x576_max.divx6
#[CASE DIVX6@3 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_DIVX6@3_CModel.yuv ${stream_path}/stream_352x288_sub.divx6
#[CASE DIVX6@4 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_DIVX6@4_CModel.yuv ${stream_path}/stream_720x480_per.divx6
#[CASE AV1@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@0_CModel.yuv ${stream_path}/stream_72x72_min.av1
#[CASE AV1@2 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@2_CModel.yuv ${stream_path}/stream_192x128_err.av1
#[CASE AV1@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_AV1@3_CModel.yuv ${stream_path}/stream_416x240_sub.av1
#[CASE AV1@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@4_CModel.yuv ${stream_path}/stream_1920x1080_per.av1
#[CASE AV1@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@5_CModel.yuv ${stream_path}/stream_1920x1080_per10b.av1
#[CASE AV1@7 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --mc -OCASE_AV1@7_CModel.yuv ${stream_path}/stream_352x288_tile.av1
#[CASE AV1@20 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_AV1@20_CModel.yuv ${stream_path}/stream_1080x720_ram0.av1
#[CASE AV1@21 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_AV1@21_CModel.yuv ${stream_path}/stream_220x256_ram3.av1
#[CASE AV1@23 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_AV1@23_CModel.yuv ${stream_path}/stream_201x139_ram2.av1
#[CASE AV1@24 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp=1 -OCASE_AV1@24_CModel.yuv ${stream_path}/stream_192x128_ram1.av1
#[CASE DIVX3@0 CModel]:
${binary_path}/mpeg4dec --custom=320x240 -N5 -A256 -n -b -OCASE_DIVX3@0_CModel.yuv ${stream_path}/stream_320x240_min.divx3
#[CASE DIVX3@1 CModel]:
${binary_path}/mpeg4dec --custom=720x480 -N2 -A256 -n -b -OCASE_DIVX3@1_CModel.yuv ${stream_path}/stream_720x480_max.divx3
#[CASE DIVX3@3 CModel]:
${binary_path}/mpeg4dec --custom=320x240 -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_DIVX3@3_CModel.yuv ${stream_path}/stream_320x240_sub.divx3
#[CASE DIVX3@4 CModel]:
${binary_path}/mpeg4dec --custom=720x480 -N5 -A256 -n -b -OCASE_DIVX3@4_CModel.yuv ${stream_path}/stream_720x480_per.divx3
#[CASE VP7@0 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b -OCASE_VP7@0_CModel.yuv ${stream_path}/stream_88x88_min.vp7
#[CASE VP7@1 CModel]:
${binary_path}/vp8dec -N2 -A256 -n -b -OCASE_VP7@1_CModel.yuv ${stream_path}/stream_640x480_max.vp7
#[CASE VP7@3 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_VP7@3_CModel.yuv ${stream_path}/stream_352x288_sub.vp7
#[CASE VP7@4 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b -OCASE_VP7@4_CModel.yuv ${stream_path}/stream_640x480_per.vp7
#[CASE WEBP@0 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b -OCASE_WEBP@0_CModel.yuv ${stream_path}/stream_48x48_min.webp
#[CASE WEBP@1 CModel]:
${binary_path}/vp8dec -N2 -A256 -n -b -OCASE_WEBP@1_CModel.yuv ${stream_path}/stream_160x16384_max.webp
#[CASE WEBP@3 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_WEBP@3_CModel.yuv ${stream_path}/stream_352x288_sub.webp
#[CASE WEBP@4 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b -OCASE_WEBP@4_CModel.yuv ${stream_path}/stream_4288x2848_per.webp
#[CASE MPEG4@0 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_MPEG4@0_CModel.yuv ${stream_path}/stream_48x48_min.mpeg4
#[CASE MPEG4@1 CModel]:
${binary_path}/mpeg4dec -N2 -A256 -n -b -OCASE_MPEG4@1_CModel.yuv ${stream_path}/stream_1920x1088_max.mpeg4
#[CASE MPEG4@2 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_MPEG4@2_CModel.yuv ${stream_path}/stream_48x48_err.mpeg4
#[CASE MPEG4@3 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_MPEG4@3_CModel.yuv ${stream_path}/stream_352x288_sub.mpeg4
#[CASE MPEG4@4 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_MPEG4@4_CModel.yuv ${stream_path}/stream_1920x1080_per.mpeg4
#[CASE MPEG4@17 CModel]:
${binary_path}/mpeg4dec -N1 -A256 -n -b -OCASE_MPEG4@17_CModel.yuv ${stream_path}/stream_352x288_ram0.mpeg4
#[CASE H264@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@0_CModel.yuv ${stream_path}/stream_48x48_min.h264
#[CASE H264@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_H264@1_CModel.yuv ${stream_path}/stream_8192x8192_max.h264
#[CASE H264@2 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@2_CModel.yuv ${stream_path}/stream_176x144_err.h264
#[CASE H264@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_H264@3_CModel.yuv ${stream_path}/stream_352x288_sub.h264
#[CASE H264@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@4_CModel.yuv ${stream_path}/stream_1920x1080_per.h264
#[CASE H264@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@5_CModel.yuv ${stream_path}/stream_1920x1080_per10b.h264
#[CASE H264@21 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --mvc -OCASE_H264@21_CModel.yuv ${stream_path}/stream_640x480_mvc.h264
#[CASE H264@22 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_H264@22_CModel.yuv ${stream_path}/stream_352x288_svc.h264
#[CASE H264@23 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_H264@23_CModel.yuv ${stream_path}/stream_200x200_ram0.h264
#[CASE SORENSON@0 CModel]:
${binary_path}/mpeg4dec --strm-sorenson -N5 -A256 -n -b -OCASE_SORENSON@0_CModel.yuv ${stream_path}/stream_64x64_min.sorenson
#[CASE SORENSON@1 CModel]:
${binary_path}/mpeg4dec --strm-sorenson -N2 -A256 -n -b -OCASE_SORENSON@1_CModel.yuv ${stream_path}/stream_1920x1080_max.sorenson
#[CASE SORENSON@3 CModel]:
${binary_path}/mpeg4dec --strm-sorenson -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_SORENSON@3_CModel.yuv ${stream_path}/stream_320x240_sub.sorenson
#[CASE SORENSON@4 CModel]:
${binary_path}/mpeg4dec --strm-sorenson -N5 -A256 -n -b -OCASE_SORENSON@4_CModel.yuv ${stream_path}/stream_640x480_per.sorenson
#[CASE H263@0 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b -OCASE_H263@0_CModel.yuv ${stream_path}/stream_48x48_min.h263
#[CASE H263@1 CModel]:
${binary_path}/mpeg4dec -N2 -A256 -n -b -OCASE_H263@1_CModel.yuv ${stream_path}/stream_720x576_max.h263
#[CASE H263@3 CModel]:
${binary_path}/mpeg4dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_H263@3_CModel.yuv ${stream_path}/stream_352x288_sub.h263
#[CASE VC1@0 CModel]:
${binary_path}/vc1dec -N5 -A256 -n -b -OCASE_VC1@0_CModel.yuv ${stream_path}/stream_48x48_min.vc1
#[CASE VC1@1 CModel]:
${binary_path}/vc1dec -N2 -A256 -n -b -OCASE_VC1@1_CModel.yuv ${stream_path}/stream_4096x176_max.vc1
#[CASE VC1@2 CModel]:
${binary_path}/vc1dec -N5 -A256 -n -b -OCASE_VC1@2_CModel.yuv ${stream_path}/stream_48x48_err.vc1
#[CASE VC1@3 CModel]:
${binary_path}/vc1dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_VC1@3_CModel.yuv ${stream_path}/stream_352x288_sub.vc1
#[CASE VC1@4 CModel]:
${binary_path}/vc1dec -N5 -A256 -n -b -OCASE_VC1@4_CModel.yuv ${stream_path}/stream_1920x1080_per.vc1
#[CASE VC1@15 CModel]:
${binary_path}/vc1dec -N1 -A256 -n -b -OCASE_VC1@15_CModel.yuv ${stream_path}/stream_720x96_ram0.vc1
#[CASE VC1@16 CModel]:
${binary_path}/vc1dec -N1 -A256 -n -b -OCASE_VC1@16_CModel.yuv ${stream_path}/stream_1280x96_ram1.vc1
#[CASE VC1@17 CModel]:
${binary_path}/vc1dec -N1 -A256 -n -b -OCASE_VC1@17_CModel.yuv ${stream_path}/stream_1920x96_ram2.vc1
#[CASE AVS_PLUS@0 CModel]:
${binary_path}/avsdec -N5 -A256 -n -b -OCASE_AVS_PLUS@0_CModel.yuv ${stream_path}/stream_240x320_min.avs_plus
#[CASE AVS_PLUS@1 CModel]:
${binary_path}/avsdec -N2 -A256 -n -b -OCASE_AVS_PLUS@1_CModel.yuv ${stream_path}/stream_1920x1080_max.avs_plus
#[CASE AVS_PLUS@3 CModel]:
${binary_path}/avsdec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_AVS_PLUS@3_CModel.yuv ${stream_path}/stream_352x288_sub.avs_plus
#[CASE AVS_PLUS@13 CModel]:
${binary_path}/avsdec -N5 -A256 -n -b -OCASE_AVS_PLUS@13_CModel.yuv ${stream_path}/stream_352x288_ram0.avs_plus
#[CASE MPEG1@0 CModel]:
${binary_path}/mpeg2dec -N5 -A256 -n -b -OCASE_MPEG1@0_CModel.yuv ${stream_path}/stream_48x48_min.mpeg1
#[CASE MPEG1@1 CModel]:
${binary_path}/mpeg2dec -N2 -A256 -n -b -OCASE_MPEG1@1_CModel.yuv ${stream_path}/stream_352x288_max.mpeg1
#[CASE MPEG1@3 CModel]:
${binary_path}/mpeg2dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_MPEG1@3_CModel.yuv ${stream_path}/stream_352x288_sub.mpeg1
#[CASE MPEG2@0 CModel]:
${binary_path}/mpeg2dec -N5 -A256 -n -b -OCASE_MPEG2@0_CModel.yuv ${stream_path}/stream_48x48_min.mpeg2
#[CASE MPEG2@1 CModel]:
${binary_path}/mpeg2dec -N2 -A256 -n -b -OCASE_MPEG2@1_CModel.yuv ${stream_path}/stream_1920x1088_max.mpeg2
#[CASE MPEG2@2 CModel]:
${binary_path}/mpeg2dec -N5 -A256 -n -b -OCASE_MPEG2@2_CModel.yuv ${stream_path}/stream_48x48_err.mpeg2
#[CASE MPEG2@3 CModel]:
${binary_path}/mpeg2dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_MPEG2@3_CModel.yuv ${stream_path}/stream_352x288_sub.mpeg2
#[CASE MPEG2@4 CModel]:
${binary_path}/mpeg2dec -N5 -A256 -n -b -OCASE_MPEG2@4_CModel.yuv ${stream_path}/stream_1920x1080_per.mpeg2
#[CASE VP6@0 CModel]:
${binary_path}/vp6dec -N5 -A256 -n -b -OCASE_VP6@0_CModel.yuv ${stream_path}/stream_48x48_min.vp6
#[CASE VP6@1 CModel]:
${binary_path}/vp6dec -N2 -A256 -n -b -OCASE_VP6@1_CModel.yuv ${stream_path}/stream_1920x1080_max.vp6
#[CASE VP6@2 CModel]:
${binary_path}/vp6dec -N5 -A256 -n -b -OCASE_VP6@2_CModel.yuv ${stream_path}/stream_48x48_err.vp6
#[CASE VP6@3 CModel]:
${binary_path}/vp6dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_VP6@3_CModel.yuv ${stream_path}/stream_320x240_sub.vp6
#[CASE VP6@4 CModel]:
${binary_path}/vp6dec -N5 -A256 -n -b -OCASE_VP6@4_CModel.yuv ${stream_path}/stream_1920x1088_per.vp6
#[CASE HEVC@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@0_CModel.yuv ${stream_path}/stream_72x72_min.hevc
#[CASE HEVC@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_HEVC@1_CModel.yuv ${stream_path}/stream_8192x8192_max.hevc
#[CASE HEVC@2 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@2_CModel.yuv ${stream_path}/stream_200x200_err.hevc
#[CASE HEVC@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_HEVC@3_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE HEVC@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@4_CModel.yuv ${stream_path}/stream_1920x1080_per.hevc
#[CASE HEVC@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@5_CModel.yuv ${stream_path}/stream_1920x1080_per10b.hevc
#[CASE HEVC@6 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --low-latency --dis-slice -OCASE_HEVC@6_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE HEVC@7 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_HEVC@7_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE HEVC@109 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT601_L --rgb-fmat=ARGB888 -qTILED64x64 --rgb-alpha=202 -OCASE_HEVC@109_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@110 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT601 --rgb-fmat=A2R10G10B10 -qTILED64x64 --rgb-alpha=110 -OCASE_HEVC@110_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE HEVC@21 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_HEVC@21_CModel.yuv ${stream_path}/stream_352x288_400.hevc
#[CASE HEVC@22 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx456 -Cy164 -Cw528 -Ch360 -D48x288 --pp=1 -Cx456 -Cy164 -Cw528 -Ch360 -D48x288 -OCASE_HEVC@22_CModel.yuv ${stream_path}/stream_1280x720_ram0.hevc
#[CASE HEVC@23 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx2 -Cy0 -Cw100 -Ch200 -Epack10 --pp=1 -Cx2 -Cy0 -Cw100 -Ch200 -Epack10 -OCASE_HEVC@23_CModel.yuv ${stream_path}/stream_256x256_ram1.hevc
#[CASE HEVC@24 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=1 -Cx14 -Cy0 -Cw100 -Ch200 -U -f -OCASE_HEVC@24_CModel.yuv ${stream_path}/stream_256x256_ram1.hevc
#[CASE HEVC@25 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx46 -Cy0 -Cw100 -Ch220 -Epack10 --pp=1 -Cx46 -Cy0 -Cw100 -Ch220 -Epack10 -OCASE_HEVC@25_CModel.yuv ${stream_path}/stream_320x256_ram3.hevc
#[CASE HEVC@30 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Epack10 --pp=1 -Epack10 -OCASE_HEVC@30_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@31 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -EI010 --pp=1 -EL010 -OCASE_HEVC@31_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE VP9@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_VP9@0_CModel.yuv ${stream_path}/stream_72x72_min.vp9
#[CASE VP9@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_VP9@3_CModel.yuv ${stream_path}/stream_352x288_sub.vp9
#[CASE VP9@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_VP9@4_CModel.yuv ${stream_path}/stream_3840x2160_per.vp9
#[CASE VP9@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_VP9@5_CModel.yuv ${stream_path}/stream_3840x2160_per10b.vp9
#[CASE VP8@0 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b -OCASE_VP8@0_CModel.yuv ${stream_path}/stream_48x48_min.vp8
#[CASE VP8@1 CModel]:
${binary_path}/vp8dec -N2 -A256 -n -b -OCASE_VP8@1_CModel.yuv ${stream_path}/stream_4096x2160_max.vp8
#[CASE VP8@3 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_VP8@3_CModel.yuv ${stream_path}/stream_352x288_sub.vp8
#[CASE VP8@4 CModel]:
${binary_path}/vp8dec -N5 -A256 -n -b -OCASE_VP8@4_CModel.yuv ${stream_path}/stream_4096x2160_per.vp8
#[CASE JPEG@0 CModel]:
${binary_path}/jpegdec -N5 -A256 -n -b -OCASE_JPEG@0_CModel.yuv ${stream_path}/stream_48x48_min.jpeg
#[CASE JPEG@3 CModel]:
${binary_path}/jpegdec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_JPEG@3_CModel.yuv ${stream_path}/stream_352x288_sub.jpeg
#[CASE JPEG@4 CModel]:
${binary_path}/jpegdec -N5 -A256 -n -b -OCASE_JPEG@4_CModel.yuv ${stream_path}/stream_1024x1024_per.jpeg
#[CASE JPEG@10 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@10_CModel.yuv ${stream_path}/stream_352x288_400.pjpeg
#[CASE JPEG@11 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@11_CModel.yuv ${stream_path}/stream_352x288_411.pjpeg
#[CASE JPEG@12 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@12_CModel.yuv ${stream_path}/stream_352x288_420.pjpeg
#[CASE JPEG@13 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@13_CModel.yuv ${stream_path}/stream_352x288_422.pjpeg
#[CASE JPEG@14 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@14_CModel.yuv ${stream_path}/stream_352x288_440.pjpeg
#[CASE JPEG@15 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@15_CModel.yuv ${stream_path}/stream_352x288_444.pjpeg
#[CASE JPEG@16 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@16_CModel.yuv ${stream_path}/stream_1920x1080_400.jpeg
#[CASE JPEG@17 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@17_CModel.yuv ${stream_path}/stream_1920x1080_411.jpeg
#[CASE JPEG@18 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@18_CModel.yuv ${stream_path}/stream_1920x1080_420.jpeg
#[CASE JPEG@19 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@19_CModel.yuv ${stream_path}/stream_1920x1080_422.jpeg
#[CASE JPEG@20 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@20_CModel.yuv ${stream_path}/stream_1920x1080_440.jpeg
#[CASE JPEG@21 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@21_CModel.yuv ${stream_path}/stream_1920x1080_444.jpeg
#[CASE JPEG@22 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_JPEG@22_CModel.yuv ${stream_path}/stream_1920x1080_12b.jpeg
#[CASE JPEG@23 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b --pp=0 -Cx98 -Cy20 -Cw50 -Ch56 -Epack10 --pp=1 -Cx98 -Cy20 -Cw50 -Ch56 -Epack10 -OCASE_JPEG@23_CModel.yuv ${stream_path}/stream_176x144_ram0.jpeg
#[CASE RV9@0 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV9@0_CModel.yuv ${stream_path}/stream_48x48_min.rv9
#[CASE RV9@1 CModel]:
${binary_path}/rvdec -N2 -A256 -n -b -OCASE_RV9@1_CModel.yuv ${stream_path}/stream_1920x1080_max.rv9
#[CASE RV9@3 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_RV9@3_CModel.yuv ${stream_path}/stream_352x288_sub.rv9
#[CASE RV9@4 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV9@4_CModel.yuv ${stream_path}/stream_704x400_per.rv9
#[CASE RV8@0 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV8@0_CModel.yuv ${stream_path}/stream_48x48_min.rv8
#[CASE RV8@1 CModel]:
${binary_path}/rvdec -N2 -A256 -n -b -OCASE_RV8@1_CModel.yuv ${stream_path}/stream_1920x48_max.rv8
#[CASE RV8@2 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV8@2_CModel.yuv ${stream_path}/stream_96x96_err.rv8
#[CASE RV8@3 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_RV8@3_CModel.yuv ${stream_path}/stream_352x288_sub.rv8
#[CASE RV8@4 CModel]:
${binary_path}/rvdec -N5 -A256 -n -b -OCASE_RV8@4_CModel.yuv ${stream_path}/stream_1920x48_per.rv8
#[CASE PPONLY@3 CModel GenRecon]:
#[CASE PPONLY@3 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@3_CModel_GenRecon_in.yuv ${stream_path}/stream_352x288_sub.hevc
${binary_path}/ppdec -H288 -W352 -S352 --in-fmt=NV12  -N5 -A256 --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48  -OCASE_PPONLY@3_CModel.yuv CASE_PPONLY@3_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@30 CModel GenRecon]:
#[CASE PPONLY@30 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@30_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920 -S1920 --in-fmt=NV12  -N5 -A256 -b --pp=0 -Epack10 --pp=1 -Epack10  -OCASE_PPONLY@30_CModel.yuv CASE_PPONLY@30_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@31 CModel GenRecon]:
#[CASE PPONLY@31 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@31_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920 -S3840 --in-fmt=NV12_P010  -N5 -A256 -b --pp=0 -Ep010 --pp=1 -EI010  -OCASE_PPONLY@31_CModel.yuv CASE_PPONLY@31_CModel_GenRecon_in_16b_0.yuv
#[CASE INT@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_INT@0_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE INT@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -p -b -OCASE_INT@4_CModel.yuv ${stream_path}/stream_720x96_int0.h264
#[CASE AVS@0 CModel]:
${binary_path}/avsdec -N5 -A256 -n -b -OCASE_AVS@0_CModel.yuv ${stream_path}/stream_48x48_min.avs
#[CASE AVS@4 CModel]:
${binary_path}/avsdec -N5 -A256 -n -b -OCASE_AVS@4_CModel.yuv ${stream_path}/stream_1920x1088_per.avs
#[CASE AVS2@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AVS2@0_CModel.yuv ${stream_path}/stream_128x128_min.avs2
#[CASE AVS2@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 -OCASE_AVS2@3_CModel.yuv ${stream_path}/stream_352x288_sub.avs2
#[CASE AVS2@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AVS2@4_CModel.yuv ${stream_path}/stream_3840x2160_per.avs2
#[CASE AVS2@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AVS2@5_CModel.yuv ${stream_path}/stream_4096x2160_per10b.avs2

