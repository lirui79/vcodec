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

stream_path=xxx  #change to your stream path
binary_path=xxx #change to your binary path

#[CASE AV1@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@0_CModel.yuv ${stream_path}/stream_72x72_min.av1
#[CASE AV1@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_AV1@1_CModel.yuv ${stream_path}/stream_4096x2160_max.av1
#[CASE AV1@2 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@2_CModel.yuv ${stream_path}/stream_192x128_err.av1
#[CASE AV1@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -OCASE_AV1@3_CModel.yuv ${stream_path}/stream_416x240_sub.av1
#[CASE AV1@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@4_CModel.yuv ${stream_path}/stream_1920x1080_per.av1
#[CASE AV1@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AV1@5_CModel.yuv ${stream_path}/stream_1920x1080_per10b.av1
#[CASE AV1@20 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_AV1@20_CModel.yuv ${stream_path}/stream_1080x720_ram0.av1
#[CASE AV1@21 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_AV1@21_CModel.yuv ${stream_path}/stream_220x256_ram3.av1
#[CASE AV1@23 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_AV1@23_CModel.yuv ${stream_path}/stream_201x139_ram2.av1
#[CASE AV1@24 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp=1 -OCASE_AV1@24_CModel.yuv ${stream_path}/stream_192x128_ram1.av1
#[CASE H264@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@0_CModel.yuv ${stream_path}/stream_48x48_min.h264
#[CASE H264@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_H264@1_CModel.yuv ${stream_path}/stream_4096x2160_max.h264
#[CASE H264@2 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@2_CModel.yuv ${stream_path}/stream_176x144_err.h264
#[CASE H264@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -OCASE_H264@3_CModel.yuv ${stream_path}/stream_352x288_sub.h264
#[CASE H264@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@4_CModel.yuv ${stream_path}/stream_1920x1080_per.h264
#[CASE H264@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_H264@5_CModel.yuv ${stream_path}/stream_1920x1080_per10b.h264
#[CASE H264@23 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_H264@23_CModel.yuv ${stream_path}/stream_200x200_ram0.h264
#[CASE PJPEG@10 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_PJPEG@10_CModel.yuv ${stream_path}/stream_352x288_400.pjpeg
#[CASE PJPEG@11 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_PJPEG@11_CModel.yuv ${stream_path}/stream_352x288_411.pjpeg
#[CASE PJPEG@12 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_PJPEG@12_CModel.yuv ${stream_path}/stream_352x288_420.pjpeg
#[CASE PJPEG@13 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_PJPEG@13_CModel.yuv ${stream_path}/stream_352x288_422.pjpeg
#[CASE PJPEG@14 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_PJPEG@14_CModel.yuv ${stream_path}/stream_352x288_440.pjpeg
#[CASE PJPEG@15 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b -OCASE_PJPEG@15_CModel.yuv ${stream_path}/stream_352x288_444.pjpeg
#[CASE HEVC@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@0_CModel.yuv ${stream_path}/stream_72x72_min.hevc
#[CASE HEVC@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_HEVC@1_CModel.yuv ${stream_path}/stream_4096x2160_max.hevc
#[CASE HEVC@2 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@2_CModel.yuv ${stream_path}/stream_200x200_err.hevc
#[CASE HEVC@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -OCASE_HEVC@3_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE HEVC@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@4_CModel.yuv ${stream_path}/stream_1920x1080_per.hevc
#[CASE HEVC@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_HEVC@5_CModel.yuv ${stream_path}/stream_1920x1080_per10b.hevc
#[CASE HEVC@6 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --low-latency --dis-slice -OCASE_HEVC@6_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE HEVC@129 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT709 --rgb-fmat=A2R10G10B10 -qTILED64x64 --rgb-alpha=8 -OCASE_HEVC@129_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE HEVC@8 CModel]:
${binary_path}/g2dec -N2 -A256 -n -b --pp=0 -D4096x2160 --pp-filter=NEAREST --pp=1 -OCASE_HEVC@8_CModel.yuv ${stream_path}/stream_4096x2160_max.hevc
#[CASE HEVC@9 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw512 -Ch512 --pp-comp --dec400-align=32 --pp=1 -Cx92 -Cy20 -Cw512 -Ch512 -OCASE_HEVC@9_CModel.yuv ${stream_path}/stream_1920x1080_per.hevc
#[CASE HEVC@128 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT2020_L --rgb-fmat=ARGB888 -qTILED64x64 --rgb-alpha=192 -OCASE_HEVC@128_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@13 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -D48x48 --pp-filter=BILINEAR --antialias=0 --pp=1 -OCASE_HEVC@13_CModel.yuv ${stream_path}/stream_320x256_ram2.hevc
#[CASE HEVC@21 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_HEVC@21_CModel.yuv ${stream_path}/stream_352x288_400.hevc
#[CASE HEVC@22 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx456 -Cy164 -Cw528 -Ch360 -D48x288 --pp-filter=NEAREST --pp=1 -Cx456 -Cy164 -Cw528 -Ch360 -OCASE_HEVC@22_CModel.yuv ${stream_path}/stream_1280x720_ram0.hevc
#[CASE HEVC@23 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx2 -Cy0 -Cw100 -Ch200 -Epack10 --pp=1 -Cx2 -Cy0 -Cw100 -Ch200 -Epack10 -OCASE_HEVC@23_CModel.yuv ${stream_path}/stream_256x256_ram1.hevc
#[CASE HEVC@25 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Cx46 -Cy0 -Cw100 -Ch220 -Epack10 --pp=1 -Cx46 -Cy0 -Cw100 -Ch220 -Epack10 -OCASE_HEVC@25_CModel.yuv ${stream_path}/stream_320x256_ram3.hevc
#[CASE HEVC@26 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --tiled-mode=TILED8x8 -Epack10 -D128x128 --pp-filter=NEAREST -OCASE_HEVC@26_CModel.yuv ${stream_path}/stream_320x256_ram2.hevc
#[CASE HEVC@30 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -Epack10 --pp=1 -Epack10 -OCASE_HEVC@30_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@31 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 -EI010 --pp=1 -EI010 -OCASE_HEVC@31_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE HEVC@92 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --tiled-mode=TILED8x8 -Epack10 -OCASE_HEVC@92_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@93 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --tiled-mode=TILED8x8 -Ep010 -OCASE_HEVC@93_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE HEVC@98 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --tiled-mode=TILED8x8 --pp-luma-only -Epack10 -OCASE_HEVC@98_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@99 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --tiled-mode=TILED8x8 --pp-luma-only -Ep010 -OCASE_HEVC@99_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE HEVC@110 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT2020_L --rgb-fmat=RGB888 -OCASE_HEVC@110_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@112 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT709 --rgb-fmat=R16G16B16 -OCASE_HEVC@112_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE HEVC@115 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT2020 --rgb-fmat=ARGB888 --rgb-alpha=240 -OCASE_HEVC@115_CModel.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
#[CASE HEVC@117 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b --pp=0 --pp-rgb --rgb-std=BT709_L --rgb-fmat=A2R10G10B10 -OCASE_HEVC@117_CModel.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
#[CASE VP9@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_VP9@0_CModel.yuv ${stream_path}/stream_72x72_min.vp9
#[CASE VP9@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_VP9@1_CModel.yuv ${stream_path}/stream_4096x2160_max.vp9
#[CASE VP9@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -OCASE_VP9@3_CModel.yuv ${stream_path}/stream_352x288_sub.vp9
#[CASE VP9@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_VP9@4_CModel.yuv ${stream_path}/stream_3840x2160_per.vp9
#[CASE VP9@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_VP9@5_CModel.yuv ${stream_path}/stream_3840x2160_per10b.vp9
#[CASE JPEG@0 CModel]:
${binary_path}/jpegdec -N5 -A256 -n -b -OCASE_JPEG@0_CModel.yuv ${stream_path}/stream_48x48_min.jpeg
#[CASE JPEG@3 CModel]:
${binary_path}/jpegdec -N5 -A256 -n -b --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -OCASE_JPEG@3_CModel.yuv ${stream_path}/stream_352x288_sub.jpeg
#[CASE JPEG@4 CModel]:
${binary_path}/jpegdec -N5 -A256 -n -b -OCASE_JPEG@4_CModel.yuv ${stream_path}/stream_1024x1024_per.jpeg
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
#[CASE JPEG@24 CModel]:
${binary_path}/jpegdec -N1 -A256 -n -b --pp=0 --tiled-mode=TILED8x8 -Epack10 -D128x128 --pp-filter=NEAREST -OCASE_JPEG@24_CModel.yuv ${stream_path}/stream_352x288_sub.jpeg
#[CASE PPONLY@128 CModel GenRecon]:
#[CASE PPONLY@128 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@128_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 --pp-rgb --rgb-std=BT601_L --rgb-fmat=ARGB888 -qTILED64x64 --rgb-alpha=234  -OCASE_PPONLY@128_CModel.yuv CASE_PPONLY@128_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@129 CModel GenRecon]:
#[CASE PPONLY@129 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@129_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920     --in-fmt=NV12  -N5 -A256 -b --pp=0 --pp-rgb --rgb-std=BT709 --rgb-fmat=A2R10G10B10 -qTILED64x64 --rgb-alpha=86  -OCASE_PPONLY@129_CModel.yuv CASE_PPONLY@129_CModel_GenRecon_in_16b_0.yuv
#[CASE PPONLY@98 CModel GenRecon]:
#[CASE PPONLY@98 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@98_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 --tiled-mode=TILED8x8 --pp-luma-only -Epack10  -OCASE_PPONLY@98_CModel.yuv CASE_PPONLY@98_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@3 CModel GenRecon]:
#[CASE PPONLY@3 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@3_CModel_GenRecon_in.yuv ${stream_path}/stream_352x288_sub.hevc
${binary_path}/ppdec -H288 -W352  --in-fmt=NV12  -N5 -A256 --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100  -OCASE_PPONLY@3_CModel.yuv CASE_PPONLY@3_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@8 CModel GenRecon]:
#[CASE PPONLY@8 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@8_CModel_GenRecon_in.yuv ${stream_path}/stream_4096x2160_max.hevc
${binary_path}/ppdec -H2160 -W4096  --in-fmt=NV12  -N2 -A256 -b --pp=0 -D4096x2160 --pp-filter=NEAREST --pp=1  -OCASE_PPONLY@8_CModel.yuv CASE_PPONLY@8_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@110 CModel GenRecon]:
#[CASE PPONLY@110 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@110_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 --pp-rgb --rgb-std=BT601_L --rgb-fmat=RGB888  -OCASE_PPONLY@110_CModel.yuv CASE_PPONLY@110_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@112 CModel GenRecon]:
#[CASE PPONLY@112 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@112_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920     --in-fmt=NV12  -N5 -A256 -b --pp=0 --pp-rgb --rgb-std=BT709_L --rgb-fmat=R16G16B16  -OCASE_PPONLY@112_CModel.yuv CASE_PPONLY@112_CModel_GenRecon_in_16b_0.yuv
#[CASE PPONLY@115 CModel GenRecon]:
#[CASE PPONLY@115 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@115_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 --pp-rgb --rgb-std=BT601_L --rgb-fmat=ARGB888 --rgb-alpha=188  -OCASE_PPONLY@115_CModel.yuv CASE_PPONLY@115_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@99 CModel GenRecon]:
#[CASE PPONLY@99 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@99_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920     --in-fmt=NV12  -N5 -A256 -b --pp=0 --tiled-mode=TILED8x8 --pp-luma-only -Ep010  -OCASE_PPONLY@99_CModel.yuv CASE_PPONLY@99_CModel_GenRecon_in_16b_0.yuv
#[CASE PPONLY@117 CModel GenRecon]:
#[CASE PPONLY@117 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@117_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920    --in-fmt=NV12  -N5 -A256 -b --pp=0 --pp-rgb --rgb-std=BT709 --rgb-fmat=A2R10G10B10  -OCASE_PPONLY@117_CModel.yuv CASE_PPONLY@117_CModel_GenRecon_in_16b_0.yuv
#[CASE PPONLY@92 CModel GenRecon]:
#[CASE PPONLY@92 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@92_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 --tiled-mode=TILED8x8 -Epack10  -OCASE_PPONLY@92_CModel.yuv CASE_PPONLY@92_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@93 CModel GenRecon]:
#[CASE PPONLY@93 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@93_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920   --in-fmt=NV12  -N5 -A256 -b --pp=0 --tiled-mode=TILED8x8 -Ep010  -OCASE_PPONLY@93_CModel.yuv CASE_PPONLY@93_CModel_GenRecon_in_16b_0.yuv
#[CASE PPONLY@30 CModel GenRecon]:
#[CASE PPONLY@30 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@30_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp8b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 -Epack10 --pp=1 -Epack10  -OCASE_PPONLY@30_CModel.yuv CASE_PPONLY@30_CModel_GenRecon_in_8b_0.yuv
#[CASE PPONLY@31 CModel GenRecon]:
#[CASE PPONLY@31 CModel]:
${binary_path}/g2dec -d1 -N10 -OCASE_PPONLY@31_CModel_GenRecon_in.yuv ${stream_path}/stream_1920x1080_pp10b.hevc
${binary_path}/ppdec -H1080 -W1920  --in-fmt=NV12  -N5 -A256 -b --pp=0 -Ep010 --pp=1 -Ep010  -OCASE_PPONLY@31_CModel.yuv CASE_PPONLY@31_CModel_GenRecon_in_16b_0.yuv
#[CASE INT@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -b -OCASE_INT@0_CModel.yuv ${stream_path}/stream_352x288_sub.hevc
#[CASE AVS2@0 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AVS2@0_CModel.yuv ${stream_path}/stream_128x128_min.avs2
#[CASE AVS2@1 CModel]:
${binary_path}/g2dec -N2 -A256 -n -OCASE_AVS2@1_CModel.yuv ${stream_path}/stream_4096x2160_max.avs2
#[CASE AVS2@3 CModel]:
${binary_path}/g2dec -N5 -A256 -n --pp=0 -Cx92 -Cy20 -Cw100 -Ch100 -D48x48 --pp-filter=NEAREST --pp=1 -Cx92 -Cy20 -Cw100 -Ch100 -OCASE_AVS2@3_CModel.yuv ${stream_path}/stream_352x288_sub.avs2
#[CASE AVS2@4 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AVS2@4_CModel.yuv ${stream_path}/stream_3840x2160_per.avs2
#[CASE AVS2@5 CModel]:
${binary_path}/g2dec -N5 -A256 -n -OCASE_AVS2@5_CModel.yuv ${stream_path}/stream_4096x2160_per10b.avs2

