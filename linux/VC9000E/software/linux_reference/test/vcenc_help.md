# Options

The chapter describes the options available in the test bench for configuring the video encoder.

- To specify a long option, separate the option and the value with an equal sign (=). For example,
<code>-\-qpHdr=-1</code>.
- To specify a short option, separate the option and the value with an equal sign (=) or omit the
equal sign. For example, <code>-q=-1</code> or <code>-q-1</code>.

In this chapter:

- In short options, [s] requires a string value and [n] requires an integer value.
- The value range of an option is provided at the beginning of its description, with the boundaries
separated by a couple of periods (..). Both boundaries are inclusive.
- The default value of an option is marked by square brackets [] at the end of the first paragraph
of the option description.

For example use cases, see the *Application Examples and Test Utility* chapter.


## Encoder Input and Output

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -i[s] | --input                | The file from which the video encoder reads input video        |
|       |                        |  sequence. [input.yuv]                                         |
| -o[s] | --output               | The file to which the video encoder writes output stream.      |
|       |                        |  [stream.hevc]                                                 |
| -a[n] | --firstPic             | The first picture to be encoded in the input file. [0]         |
| -b[n] | --lastPic              | The last picture to be encoded in the input file. [100]        |
|       | --outReconFrame        | Whether to output reconstructed frames. [1]                    |
|       |                        | 0 - do not output reconstructed frames.                        |
|       |                        | 1 - output reconstructed frames.                               |
| -j[n] | --inputRateNumer       | 1..1048575 The numerator used to calculate the input frame     |
|       |                        |  rate. [30]                                                    |
| -J[n] | --inputRateDenom       | 1..1048575 The denominator used to calculate the input frame   |
|       |                        |  rate. [1]                                                     |
| -f[n] | --outputRateNumer      | 1..1048575 The numerator used to calculate the output frame    |
|       |                        |  rate. [Same as input]                                         |
| -F[n] | --outputRateDenom      | 1..1048575 The denominator used to calculate the output frame  |
|       |                        |  rate. [Same as input]                                         |
|       | --writeReconToDDR      | Whether to write reconstructed frames to DDR. [1]              |
|       |                        | 0 - do not write reconstructed frames to DDR.                  |
|       |                        | 1 - write reconstructed frames to DDR.                         |
|       |                        | The option is valid only for I-frame only encoding.            |


### Input and Encoded Frame Resolutions and Cropping

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -w[n] | --lumWidthSrc          | The width of the input picture, in pixels. [176]               |
| -h[n] | --lumHeightSrc         | The height of the input picture, in pixels. [144]              |
| -x[n] | --width                | The width of the encoded picture, in pixels. [--lumWidthSrc]   |
| -y[n] | --height               | The height of the encoded picture, in pixels. [--lumHeightSrc] |
| -X[n] | --horOffsetSrc         | The horizontal offset, in pixels, of the top-left corner of    |
|       |                        |  the encoded picture relative to the input picture. [0]        |
|       |                        | The option value must be an even integer.                      |
| -Y[n] | --verOffsetSrc         | The vertical offset, in pixels, of the top-left corner of the  |
|       |                        |  encoded picture relative to the input picture. [0]            |
|       |                        | The option value must be an even integer.                      |
|       | --inputFileList        | The path to a file that contains a list of input files.        |
|       |                        | The option is useful for testing resolution change.            |
|       |                        | Supported sub-options in the file include:                     |
|       |                        | -i: the input file path                                        |
|       |                        | -a: the start picture                                          |
|       |                        | -b: the end picture                                            |
|       |                        | -w: the picture width in pixels                                |
|       |                        | -h: the picture height in pixels                               |
|       |                        | -o: the path to the output file                                |
|       |                        | If inputFileList is specified, the setting of the --input      |
|       |                        |  option is ignored. The video encoder reads inputs from the    |
|       |                        |  listed files. In this case, the --width and --height          |
|       |                        |  indicates the allowed maximum width and height used in        |
|       |                        |  initialization.                                               |



## Pre-Processing

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -l[n] | --inputFormat          | The input color format. [0]                                    |
|       |                        | 0 - YUV420P8b_Raster_A16N                                      |
|       |                        | 1 - YUV420SP8b_Raster_A16N                                     |
|       |                        | 2 - YVU420SP8b_Raster_A16N                                     |
|       |                        | 3 - YUV422YUYV8b_Raster_A16N                                   |
|       |                        | 4 - YUV422UYVY8b_Raster_A16N                                   |
|       |                        | 5 - RGB565_Raster_A16N                                         |
|       |                        | 6 - BGR565_Raster_A16N                                         |
|       |                        | 7 - XRGB1555_Raster_A16N                                       |
|       |                        | 8 - XBGR1555_Raster_A16N                                       |
|       |                        | 9 - XRGB4444_Raster_A16N                                       |
|       |                        | 10 - XBGR4444_Raster_A16N                                      |
|       |                        | 11 - XRGB8888_Raster_A16N                                      |
|       |                        | 12 - XBGR8888_Raster_A16N                                      |
|       |                        | 13 - XRGB2101010_Raster_A16N                                   |
|       |                        | 14 - XBGR2101010_Raster_A16N                                   |
|       |                        | 15 - YUV420P10bWL_Raster_A16N                                  |
|       |                        | 16 - YUV420SP10bWH_Raster_A16N                                 |
|       |                        | 17 - YUV420P10b_Raster_A16N                                    |
|       |                        | 18 - YUV420Y0L210b_Raster_A16N                                 |
|       |                        | 19 - (Reserved) YUV420P8b_Tile32x32_A16N for HEVC (H.265)      |
|       |                        | 20 - (Reserved) YUV420P8b_Tile16x16_A16N for H.264 (AVC)       |
|       |                        | 21 - YUV420SP8b_YuvSp4x4_A16N                                  |
|       |                        | 22 - YVU420SP8b_YuvSp4x4_A16N                                  |
|       |                        | 23 - YUV420SP10bWH_YuvSp4x4_A32N                               |
|       |                        | 24 - YUV420SP10bDWL_Raster_A16N                                |
|       |                        | 26 - (Reserved) YUV420YVU8b_Tile64x4_A16N                      |
|       |                        | 27 - (Reserved) YUV420YUV8b_Tile64x4_A16N                      |
|       |                        | 28 - (Reserved) YUV420YUV10b_Tile32x4_A16N                     |
|       |                        | 29 - (Reserved) YUV420YUV10b_Tile48x4_A16N                     |
|       |                        | 30 - (Reserved) YUV420YVU10b_Tile48x4_A16N                     |
|       |                        | 31 - (Reserved) YUV420YVU8b_Tile128x2_A16N                     |
|       |                        | 32 - (Reserved) YUV420YUV8b_Tile128x2_A16N                     |
|       |                        | 33 - (Reserved) YUV420YUV10b_Tile96x2_A16N                     |
|       |                        | 34 - (Reserved) YUV420YVU10b_Tile96x2_A16N                     |
|       |                        | 35 - YUV420SP8b_YuvSp8x8_A64N                                  |
|       |                        | 36 - YUV420SP10bWH_YuvSp8x8_A128N                              |
|       |                        | 37 - YUV420PYVU8b_Raster_A16N                                  |
|       |                        | 38 - (Reserved) YVU420SP8b_Tile64x2_A16N                       |
|       |                        | 39 - (Reserved) YVU420SP10b_Tile128x2_A16N                     |
|       |                        | 40 - RGB888_Raster_A16N                                        |
|       |                        | 41 - BGR888_Raster_A16N                                        |
|       |                        | 42 - RBG888_Raster_A16N                                        |
|       |                        | 43 - GBR888_Raster_A16N                                        |
|       |                        | 44 - BRG888_Raster_A16N                                        |
|       |                        | 45 - GRB888_Raster_A16N                                        |
|       |                        | 47 - YUV444P8b_Raster_A16N (YCbCr 4:4:4 planar(I444))          |
|       |                        | 48 - YUV444XYUV8888_Raster_A16N                                |
|       |                        | 49 - YUV444XYUV2101010_Raster_A16N                             |
|       |                        | 50 - RGBX8888_Raster_A16N                                      |
|       |                        | 51 - BGRX8888_Raster_A16N                                      |
|       |                        | 52 - RGBX1010102_Raster_A16N                                   |
|       |                        | 53 - BGRX1010102_Raster_A16N                                   |
|       |                        | 54 - YVU420SP10bWH_Raster_A16N                                 |
|       |                        | 55 - Y8b_Raster_A16N (YCbCr 4:0:0 ) Monochrome 8-bit           |
|       |                        | 56 - Y10bWL_Raster_A16N (YCbCr 4:0:0 ) Monochrome 10-bit       |
|       |                        |  occupies 2B [9:0]                                             |
|       |                        | 57 - Y10bWH_Raster_A16N (YCbCr 4:0:0 ) Monochrome 10-bit       |
|       |                        |  occupies 2B [15:6]                                            |
|       |                        | 58 - YUV420SP10b_Raster_A16N                                   |
|       |                        | 59 - YUV422P10bWL_Raster_A16N                                  |
|       |                        | 60 - Y8b_Tile8x8_A16N (YCbCr 4:0:0 ) Monochrome 8-bit          |
|       |                        | 61 - Y10bWH_Tile8x8_A16N (YCbCr 4:0:0 ) Monochrome 10-bit      |
|       |                        |  occupies 2B [15:6]                                            |
|       |                        | 62 - YUV422YVYU8b_Raster_A16N                                  |
|       |                        | 63 - YUV422VYUY8b_Raster_A16N                                  |
|       |                        | 64 - YVU420SP8b_YuvSp8x8_A64N                                  |
|       |                        | 65 - Y10b_Raster_A16N                                          |
|       |                        | For details about the color formats, see Hantro VC9000E        |
|       |                        |  Series Memory Buffer and Format Organization.                 |
| -O[n] | --colorConversion      | The RGB-to-YUV color conversion type. [0]                      |
|       |                        | 0 - conversion to YUV values without any range change according|
|       |                        |  to Rec. ITU-R BT.601.                                         |
|       |                        | 1 - conversion to YUV values without any range change according|
|       |                        |  to Rec. ITU-R BT.709.                                         |
|       |                        | 2 - conversion using custom coefficients.                      |
|       |                        | 3 - conversion according to Rec. ITU-R BT.2020.                |
|       |                        | 4 - conversion from full range RGB to limited range YUV        |
|       |                        |  according to Rec. ITU-R BT.601.                               |
|       |                        | 5 - (just for test) conversion from RGB in range of (0~219) to |
|       |                        |  limited range YUV according to Rec. ITU-R BT.601.             |
|       |                        | 6 - conversion from full range RGB to limited range YUV        |
|       |                        |   according to Rec. ITU-R BT.709.                              |
|       | --mirror               | The mirroring mode to pre-process the input image. [0]         |
|       |                        | 0 - do not mirror                                              |
|       |                        | 1 - mirror                                                     |
| -r[n] | --rotation             | The rotation mode to pre-process the input image. [0]          |
|       |                        | 0 - do not rotate                                              |
|       |                        | 1 - rotate 90 degrees clockwise                                |
|       |                        | 2 - roatate 90 degrees counterclockwise                        |
|       |                        | 3 - rotate 180 degrees clockwise                               |
| -Z[n] | --videoStab            | Whether to enable video stabilization. [0]                     |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | When this option is set to 1, the video stabilization module   |
|       |                        |  also performs scene change detection if the input picture     |
|       |                        |  resolution is the same as the encoded picture esolution.      |
|       | --scaledWidth          | 0..width The width of the down-scaled picture. [0]             |
|       |                        | The value must a multiple of 4.                                |
|       |                        | If either this option or --scaledHeight is set to 0,           |
|       |                        |  down-scaling is disabled.                                     |
|       |                        | NOTE: The down-scaled pictures is stored in the                |
|       |                        |  scaled.yuv file.                                              |
|       | --scaledHeight         | 0..height The height of the down-scaled picture. [0]           |
|       |                        | The value must a multiple of 4.                                |
|       |                        | If either this option or --scaledWidth is set to 0,            |
|       |                        |  down-scaling is disabled.                                     |
|       |                        | NOTE: The down-scaled pictures is stored in the                |
|       |                        |  scaled.yuv file.                                              |
|       | --scaledOutputFormat   | The color format of the output down-scaled picture. [0]        |
|       |                        | 0 - YUV422 interleaved                                         |
|       |                        | 1 - YUV420 semiplanar (NV12)                                   |
|       | --interlacedFrame      | Whether the input frames are progressive or interlaced. [0]    |
|       |                        | 0 - progressive frame input                                    |
|       |                        | 1 - interlaced framed input                                    |
|       | --fieldOrder           | The interlaced field order. [0]                                |
|       |                        | 0 - bottom field first                                         |
|       |                        | 1 - top field first                                            |
|       | --codedChromaIdc       | The chroma sampling modes relative to luma sampling. [1]       |
|       |                        | 0 - 4:0:0 for HEVC (H.265) and H.264 (AVC) only                |
|       |                        | 1 - 4:2:0                                                      |
|       |                        | 2 - 4:2:2 (under development)                                  |
|       |                        | 3 - 4:4:4 for HEVC (H.265) only                                |
|       |                        | This field is used as the SPS syntax element                   |
|       |                        |  chroma_format_idc specified in ITU-T Rec. H.265 and H.264.    |
|       | --scanType             | The scan type of input image. [0]                              |
|       |                        |   0 - raster                                                   |
|       |                        |   1 - supertileX                                               |

### Input Conversion to Customized Formats

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --formatCustomizedType | -1..13 (For test use) Converts the input image to the          |
|       |                        |  specified custom format. The data after format conversion is  |
|       |                        |  fed to the video encoder as its input. [-1]                   |
|       |                        | An option value in range from 0 to 13 indicates a pre-defined  |
|       |                        |  custom format.                                                |
|       |                        | If this option is set to -1, format converion before input to  |
|       |                        |  the encoder is disabled.                                      |
|       |                        | If this option is specified, dividing the input image into     |
|       |                        |  multiple tile columns and rows is not supported.              |

### DEC400 Compression Table (Tile Status)

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --dec400TableInput     | The file from which the video encoder reads the DEC400         |
|       |                        |  compression table for video layer input.                      |
|       |                        |  [dec400CompTableinput.bin]                                    |
|       | --osdDec400TableInputN | The file from which the video encoder reads the DEC400         |
|       |                        |  compression table for OSD input. [NULL]                       |
|       | --dec400TileSize       |  0 - default                                                   |
|       |                        |  1 - 128byte.                                                  |
|       |                        |  2 - 512byte.                                                  |
|       |                        |  3 - 256byte.                                                  |
|       | --dec400TileMode       |  0 - default.[0]                                               |
|       |                        |  1 - TILE_8x8_x.                                               |
|       |                        |  2 - TILE_8x8_y.                                               |
|       |                        |  3 - TILE_16x4.                                                |
|       |                        |  4 - TILE_8x4.                                                 |
|       |                        |  5 - TILE_4x8.                                                 |
|       |                        |  6 - RASTER_16x4.                                              |
|       |                        |  7 - TILE_64x4.                                                |
|       |                        |  8 - TILE_32x4.                                                |
|       |                        |  9 - RASTER256x1.                                              |
|       |                        |  10 - RASTER128x1.                                             |
|       |                        |  11 - RASTER64x1.                                              |
|       |                        |  12 - TILE_16x8.                                               |
|       |                        |  13 - RASTER32x4.                                              |
|       |                        |  14 - RASTER32x1.                                              |
|       |                        |  15 - RASTER16x1.                                              |
|       |                        |  16 - TILE_8x4_s.                                              |
|       |                        |  17 - TILE_16x4_s.                                             |
|       |                        |  18 - TILE_32x4_s.                                             |
|       |                        |  19 - TILE_32x8.                                               |
|       | --dec400DataAlignment  |  0 - default                                                   |
|       |                        |  1 - 32byte.                                                   |
|       |                        |  2 - 64byte.                                                   |
|       |                        |  3 - 1byte.                                                    |
|       |                        |  4 - 16byte.                                                   |
|       | --dec400RGBAX          |  0 - ARGB.[0]                                                  |
|       |                        |  1 - XRGB.                                                     |
|       | --dec400RGBFormat      |  0 - default                                                   |
|       |                        |  1 - RGB8.                                                     |
|       |                        |  2 - RGB10.                                                    |
|       |                        |  3 - RGB4.                                                     |
|       |                        |  4 - RGB1555.                                                  |
|       |                        |  5 - RGB565.                                                   |
|       | --dec400TSHeaderEnable | Whether to process dec400 tile status header buffer.(128byte)  |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |

### UFBC Parameters

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --ufbcMode             | The core mode of UFBC                                          |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - afbc (only support 32x8 pixels)                            |
|       |                        | 2 - afbc (support 32x8 and 16x16 pixels)                       |
|       |                        | 3 - dec400                                                     |
|       |                        | 4 - pvric                                                      |
|       | --ufbcYuvTrans         | Whether to disable interal YUV transformation. [0]             |
|       |                        | 0 - enable                                                     |
|       |                        | 1 - disable                                                    |
|       | --ufbcBlockType        | The size of the superblock.                                    |
|       |                        | Version 1.x                                                    |
|       |                        | 0 - 32x8 pixels                                                |
|       |                        | 1 - 16x16 pixels                                               |
|       |                        | Version 2.x                                                    |
|       |                        | 0 - 8x8 pixels                                                 |
|       |                        | 1 - 16x4 pixels                                                |
|       |                        | 2 - 32x2 pixels                                                |
|       | --ufbcBlockSplit       | Whether to enable the superblock split mode for version 1.x.   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --ufbcConstantVal      | The constant color for version 2.x.                            |

## Stream Coding Tools

### Output Stream Format

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --codecFormat          | The video codec standard. [0]                                  |
|       |                        | 0 or hevc - HEVC (H.265)                                       |
|       |                        | 1 or h264 - H.264 (AVC)                                        |
|       |                        | 2 or av1 - AV1                                                 |
|       |                        | 3 or vp9 - VP9                                                 |
| -P[n] | --profile              | The stream profile. [-1]                                       |
|       |                        |-1 - Adaptive profile                                           |
|       |                        | HEVC (H.265) profiles:                                         |
|       |                        | 0 - Main profile                                               |
|       |                        | 1 - Main Still Picture profile                                 |
|       |                        | 2 - Main 10 profile                                            |
|       |                        | 3 - Monochrome profile                                         |
|       |                        | 4 - Monochrome 10 profile                                      |
|       |                        | 5 - Main 10 Still Picture profile                              |
|       |                        | 6 - Main 422 10 profile                                        |
|       |                        | 7 - Main 444 10 profile                                        |
|       |                        | 8 - SCC Main profile                                           |
|       |                        | 9 - SCC 444 10 profile                                         |
|       |                        | H.264 (AVC) profiles:                                          |
|       |                        | 9 - Baseline profile                                           |
|       |                        | 10 - Main profile                                              |
|       |                        | 11 - High profile                                              |
|       |                        | 12 - High 10 profile                                           |
|       |                        | AV1 profiles:                                                  |
|       |                        | 0 - Main profile                                               |
|       |                        | VP9 profiles:                                                  |
|       |                        | 0 - Main profile                                               |
|       |                        | 2 - High profile                                               |
| -L[n] | --level                | The stream level.                                              |
|       |                        | If the option is set to 0xFFFF, the encoder software           |
|       |                        |  automatically selects the level.                              |
|       |                        | HEVC (H.265) levels [180]:                                     |
|       |                        | Levels up to level 5.1 High tier are supported in real-time.   |
|       |                        | Levels 5.0 and 5.1 with resolutions up to 4096x2048 are        |
|       |                        |  supported with 4K@60FPS performance.                          |
|       |                        | Levels greater than level 5.1 are supported in non-realtime    |
|       |                        |  mode.                                                         |
|       |                        | Value - Level - Resolution - Main tier max bit rate - High     |
|       |                        |  tier max bit rate                                             |
|       |                        | 30 - 1.0 - QCIF -128 kbps - N/A                                |
|       |                        | 60 - 2.0 - CIF - 1.5 Mbps - N/A                                |
|       |                        | 63 - 2.1 - Q720p - 3.0 Mbps - N/A                              |
|       |                        | 90 - 3.0 - QHD - 6.0 Mbps - N/A                                |
|       |                        | 93 - 3.1 - 1280x720 - 10.0 Mbps - N/A                          |
|       |                        | 120 - 4.0 - 2Kx1080 - 12.0 Mbps - 30 Mbps                      |
|       |                        | 123 - 4.1 - 2Kx1080 - 20.0 Mbps - 50 Mbps                      |
|       |                        | 150 - 5.0 - 4096x2160 - 25.0 Mbps - 100 Mbps                   |
|       |                        | 153 - 5.1 - 4096x2160 - 40.0 Mbps - 160 Mbps                   |
|       |                        | 156 - 5.2 - 4096x2160 - 60.0 Mbps - 240 Mbps                   |
|       |                        | 180 - 6.0 - 8192x4320 - 60.0 Mbps - 240 Mbps                   |
|       |                        | 183 - 6.1 - 8192x4320 - 120.0 Mbps - 480 Mbps                  |
|       |                        | 186 - 6.2 - 8192x4320 - 240.0 Mbps - 800 Mbps                  |
|       |                        | H.264 levels [51]:                                             |
|       |                        | 10 - H264_LEVEL_1                                              |
|       |                        | 99 - H264_LEVEL_1_b                                            |
|       |                        | 11 - H264_LEVEL_1_1                                            |
|       |                        | 12 - H264_LEVEL_1_2                                            |
|       |                        | 13 - H264_LEVEL_1_3                                            |
|       |                        | 20 - H264_LEVEL_2                                              |
|       |                        | 21 - H264_LEVEL_2_1                                            |
|       |                        | 22 - H264_LEVEL_2_2                                            |
|       |                        | 30 - H264_LEVEL_3                                              |
|       |                        | 31 - H264_LEVEL_3_1                                            |
|       |                        | 32 - H264_LEVEL_3_2                                            |
|       |                        | 40 - H264_LEVEL_4                                              |
|       |                        | 41 - H264_LEVEL_4_1                                            |
|       |                        | 42 - H264_LEVEL_4_2                                            |
|       |                        | 50 - H264_LEVEL_5                                              |
|       |                        | 51 - H264_LEVEL_5_1                                            |
|       |                        | 52 - H264_LEVEL_5_2                                            |
|       |                        | 60 - H264_LEVEL_6                                              |
|       |                        | 61 - H264_LEVEL_6_1                                            |
|       |                        | 62 - H264_LEVEL_6_2                                            |
|       |                        | AV1 levels [13]:                                               |
|       |                        | 0 - AV1_LEVEL_2.0                                              |
|       |                        | 1 - AV1_LEVEL_2.1                                              |
|       |                        | 2 - AV1_LEVEL_2.2                                              |
|       |                        | 3 - AV1_LEVEL_2.3                                              |
|       |                        | 4 - AV1_LEVEL_3.0                                              |
|       |                        | 5 - AV1_LEVEL_3.1                                              |
|       |                        | 6 - AV1_LEVEL_3.2                                              |
|       |                        | 7 - AV1_LEVEL_3.3                                              |
|       |                        | 8 - AV1_LEVEL_4.0                                              |
|       |                        | 9 - AV1_LEVEL_4.1                                              |
|       |                        | 10 - AV1_LEVEL_4.2                                             |
|       |                        | 11 - AV1_LEVEL_4.3                                             |
|       |                        | 12 - AV1_LEVEL_5.0                                             |
|       |                        | 13 - AV1_LEVEL_5.1                                             |
|       | --tier                 | The stream tier for HEVC (H.265) or AV1. [0]                   |
|       |                        | 0 - Main tier                                                  |
|       |                        | 1 - High tier                                                  |
|       | --bitDepthLuma         | The bit depth of luma samples in the encoded stream. [8]       |
|       |                        | 8 - 8-bit luma samples                                         |
|       |                        | 10 - 10-bit luma samples                                       |
|       | --bitDepthChroma       | The bit depth of chroma samples in the encoded stream. [8]     |
|       |                        | 8 - 8-bit Chroma samples                                       |
|       |                        | 10 - 10-bit Chroma samples                                     |
|       |                        | The value of bitDepthChroma must be the same as that of        |
|       |                        |  bitDepthLuma.                                                 |
| -N[n] | --byteStream           | (Only for HEVC/H.264) The stream type. [1]                     |
|       |                        | 0 - NAL unit stream. NAL sizes are stored in the               |
|       |                        |  nal_sizes.txt file.                                           |
|       |                        | 1 - Byte stream.                                               |
|       |                        | For AV1, only the byte stream type is supported.               |
|       | --ivf                  | (For AV1/VP9 only) The IVF encapsulation. [1]                  |
|       |                        | 0 - OBU raw output                                             |
|       |                        | 1 - IVF output                                                 |
|       | --sendAUD              | Whether to enable the access unit delimiter (AUD). [0]         |
|       |                        | 0 - do not send                                                |
|       |                        | 1 - send                                                       |
|       | --smoothingIntra       | (For HEVC only) Whether to enable normal or strong smoothing.  |
|       |                        |  [1]                                                           |
|       |                        | 0 - normal                                                     |
|       |                        | 1 - strong                                                     |
| -p[n] | --cabacInitFlag        | (Obsoleted) The initialization value for CABAC. [0]            |
|       |                        | The option value is fixed to 0.                                |
| -K[n] | --enableCabac          | (Only for H.264) The entropy coding mode. [1]                  |
|       |                        | 0 - content-adaptie variable-length coding (CAVLC)             |
|       |                        | 1 - content-based adaptive binary arithmetic coding (CABAC)    |
| -e[n] | --sliceSize            | (For HEVC/H.264/only) 0..height/ctu_size The number of CTB     |
|       |                        |  or MB rows in each slice. [0]                                 |
|       |                        | If the option is set to 0, the encoder encodes a picture in    |
|       |                        |  each slice.                                                   |
| -D[n] | --disableDeblocking    | Whether to disable inloop de-blocking filters. [0]             |
|       |                        | 0 - enable. This value provides better quality.                |
|       |                        | 1 - disable.                                                   |
|       | --enableTS             | Disable/Enable HEVC Transform Skip [0]                         |
|       |                        | 0 - disable hevc transform skip                                |
|       |                        | 1 - enable hevc transform skip                                 |
|       | --log2MaxTSBlockSizeMinus2 |   0..2 Max transform skip size minus 2 [0]                 |
| -M[n] | --enableSao            | Whether to enable sample adaptive offset (SAO) for HEVC        |
|       |                        |  (H.265) or CDEF filter for AV1. [1]                           |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
| -W[n] | --tc_Offset            | -6..6 The de-blocking parameter tc_offset for HEVC (H.265) or  |
|       |                        |  alpha_c0_offset for H.264 (AVC). [0]                          |
| -E[n] | --beta_Offset          | -6..6 The de-blocking parameter beta_offset for HEVC (H.265)   |
|       |                        |  and H.264 (AVC). [0]                                          |
|       | --enableDeblockOverride| (Only for HEVC) Whether to enable de-blocking override         |
|       |                        |  between frames. [0]                                           |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --deblockOverride      | (Only for HEVC) Whether the frame has de-blocking override     |
|       |                        |  information in the slice header. [0]                          |
|       |                        | 0 - does not have                                              |
|       |                        | 1 - has                                                        |
|       | --tile=a:b:c           | (For HEVC only) The tile settings. [1:1:1]                     |
|       |                        | - a indicates the number of tile columns to divide the picture.|
|       |                        | - b indicates the number of tile rows to divide the picture.   |
|       |                        | - c indicates whether to enable the loop filter across the     |
|       |                        |  tile boundary.                                                |
|       |                        | Value 0 indicates to disable and value 1 indicates to enable.  |
|       |                        | Tiling is enabled when a * b > 1.                              |
|       | --enableScalingList    | (For HEVC only) Whether to use the average or default scaling  |
|       |                        |  list. [0]                                                     |
|       |                        | 0 - average scaling list                                       |
|       |                        | 1 - default scaling list                                       |
|       | --RPSInSliceHeader     | (For HEVC only) Whether to encode RPS in the slice header. [0] |
|       |                        | 0 - do not encode                                              |
|       |                        | 1 - encode                                                     |
|       | --resendParamSet       | Whether to re-send VPS, PPS, and SPS before each IDR frame. [0]|
|       |                        | 0 - do not re-send                                             |
|       |                        | 1 - re-send                                                    |
|       | --POCConfig=a:b:c      | The POC settings. [0:16:12]                                    |
|       |                        | - a indicates the method to encode POC in the slice header.    |
|       |                        | - b indicates the number of bits used to encode POC in the     |
|       |                        |  slice header.                                                 |
|       |                        | - c indicates the number of bits used to encode frame_num in   |
|       |                        |  the slice header.                                             |
|       | --psyFactor            | 0..4.0 The strength of psycho-visual encoding. <float> [0]     |
|       |                        | If the option is set to 0, psycho-visual encoding is disabled. |
|       | --layerInRefIdc        | (Only for H.264) Whether to enable layer information in        |
|       |                        |  the nal_ref_idc syntax element. [0]                           |
|       |                        | 0 - disable. In this case, the value of nal_ref_idc can be 0   |
|       |                        |  or 1.                                                         |
|       |                        | 1 - enable. In this case, the value of nal_ref_idc can be 0 to |
|       |                        |  3.                                                            |
|       | --prefixNalSvcFlag     | (Only for H.264) Whether to add prefix NAL units betwwen       |
|       |                        |  slices for temporal scalable video coding (SVC-T). [0]        |
|       |                        | 0 - do not add                                                 |
|       |                        | 1 - add                                                        |
|       | --svctEnable           | (Only for H.264) Whether to enable SVC-T. [0]                  |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --enableTMVP           | Whether to enable temporal motion vector prediction            |
|       |                        |  (TMVP) in inter prediction.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | The default value is 1 for HEVC, AV1, and VP9 if TMVP is       |
|       |                        |  supported, and 0 in other cases.                              |
|       | --Heifcfg              | The path to the HEIF configuration file.                       |
|       |                        | ( Only support codec format HEVC and AV1 )                     |

#### Constant Chroma

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --enableConstChroma    | Whether to encode the picture with constant U and V            |
|       |                        |  values. [0]                                                   |
|       |                        | 0 - do not encode with a constant U or V value                 |
|       |                        | 1 - encode with a constant U or V value                        |
|       | --constCb              | The constant U value.                                          |
|       |                        | The option value range for 8-bit encoding color space is from  |
|       |                        |  0 to 255, inclusive. The default value is 128.                |
|       |                        | The option value range for 10-bit encoding color space is      |
|       |                        |  from 0 to 1023, inclusive. The default value is 512.          |
|       |                        | This option is valid only if enableConstChroma is set to 1.    |
|       | --constCr              | The constant V value.                                          |
|       |                        | The option value range for 8-bit encoding color space is from  |
|       |                        |  0 to 255, inclusive. The default value is 128.                |
|       |                        | The option value range for 10-bit encoding color space is      |
|       |                        |  from 0 to 1023, inclusive. The default value is 512.          |
|       |                        | This option is valid only if enableConstChroma is set to 1.    |


### GOP Structure

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --smoothPsnrInGOP      | (Obsoleted) Whether to enable smooth PSNR for frames in a      |
|       |                        |  GOP. [0]                                                      |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | This option is valid only if gopSize is set to 1.              |
|       | --gopSize              | 0..8 The size of the GOP structure. [0]                        |
|       |                        | If the option is set to 0, the encoder adaptively selects a    |
|       |                        |  GOP structure size.                                           |
|       |                        | If the option is set to a value from 1 to 8, the encoder uses  |
|       |                        |  a fixed GOP structure size with a pre-defined GOP structure.  |
|       | --gopMaxBSize          | 0..7[default -255, disable] The GOP max B frame number for AGOP|
|       | --gopConfig            | The path to a custom GOP configuration file, which defines     |
|       |                        |  the GOP structure table.                                      |
|       |                        | If the gopConfig option is not specified, the default          |
|       |                        |  configurations take effect.                                   |
|       |                        | The GOP configuration file must include gopSize normal         |
|       |                        |  configuration lines. Each normal configuration line must      |
|       |                        |  contain the following fields in order:                        |
|       |                        | - FrmN: the ID of the frame, where N is an integer ranging     |
|       |                        |  from 1 to gopSize.                                            |
|       |                        | In the configuration file, frames must be listed in decoding   |
|       |                        |  order.                                                        |
|       |                        | - Type: the slice type, which can be P, B, or nrefB.           |
|       |                        | - POC: the display order of the frame within the GOP, ranging  |
|       |                        |  from 1 to gopSize.                                            |
|       |                        | - QPoffset: the offset to be added to the QP parameter to      |
|       |                        |  obtain the final QP value used for the frame.                 |
|       |                        | - QPfactor: the weight used during rate distortion             |
|       |                        |  optimization.                                                 |
|       |                        | - TemporalId: the temporal layer ID.                           |
|       |                        | - num_ref_pics: the number of reference frames kept for the    |
|       |                        |  current frame.                                                |
|       |                        | The reference frames can be used by either the current frame   |
|       |                        |  or a subsequent frame.                                        |
|       |                        | - ref_pics: a list of num_ref_pics integers, each indicating   |
|       |                        |  the POC offset of the reference frame relative to the current |
|       |                        |  frame or the LTR index.                                       |
|       |                        | - used_by_cur: a list of num_ref_pics binaries, each           |
|       |                        |  indicating whether the corresponding frame in the ref_pics    |
|       |                        |  list is used as a reference for encoding of the current       |
|       |                        |  frame.                                                        |
|       |                        | Each special configuration line must contain the following     |
|       |                        |  fields in order:                                              |
|       |                        | - Frame0: indicates that the current line contains special     |
|       |                        |  GOP configurations (for example, for long-term reference).    |
|       |                        | - Type: the slice type, which can be either P, B, or I. [-255] |
|       |                        | If the value is not -255, it overrides the value specified in  |
|       |                        |  the normal configurations.                                    |
|       |                        | - QPoffset: the offset to be added to the QP parameter to      |
|       |                        |  obtain the final QP value used for this frame. [-255]         |
|       |                        | If the value is not -255, it overrides the value specified in  |
|       |                        |  the normal configurations.                                    |
|       |                        | - QPfactor: the weight used during rate distortion             |
|       |                        |  optimization. [-255]                                          |
|       |                        | If the value is not -255, it overrides the value specified in  |
|       |                        |  the normal configurations.                                    |
|       |                        | - TemporalId: temporal layer ID. [-255]                        |
|       |                        | If the value is not -255, it overrides the value specified in  |
|       |                        |  the normal configurations.                                    |
|       |                        | - num_ref_pics: the number of reference frames kept for the    |
|       |                        |  current frame.                                                |
|       |                        | The reference frames can be used by the current frame or a     |
|       |                        |  subsequent frame.                                             |
|       |                        | - ref_pics: a list of num_ref_pics integers, each indicating   |
|       |                        |  the POC offset of the reference frame relative to the current |
|       |                        |  frame or the LTR index.                                       |
|       |                        | - used_by_cur: a list of num_ref_pics binaries, each           |
|       |                        |  indicating whether the frame is used as a reference for       |
|       |                        |  encoding of the current frame.                                |
|       |                        | - LTR: the LTR index of the frame.                             |
|       |                        | Value 0 indicates a common frame that uses LTR.                |
|       |                        | A value in range from 1 to VCENC_MAX_LT_REF_FRAMES indicates   |
|       |                        |  an LTR frame.                                                 |
|       |                        | - Offset: If LTR equals 0, the Offset field indicates the POC  |
|       |                        |  delta between the LTR frame and the first subsequent frame    |
|       |                        |  that uses the LTR frame as reference.                         |
|       |                        | If LTR falls in the range from 1 to VCENC_MAX_LT_REF_FRAMES,   |
|       |                        |  the Offset field indicates the POC delta between the first    |
|       |                        |  LTR frame and the first encoded frame.                        |
|       |                        | - Interval: If LTR equals 0, the Interval field indicates the  |
|       |                        |  POC delta between two adjacent frames that use the same LTR   |
|       |                        |  frame as reference.                                           |
|       |                        | If LTR falls in the range from 1 to VCENC_MAX_LT_REF_FRAMES,   |
|       |                        |  the Interval field indicates the POC delta between two        |
|       |                        |  adjacent frames with the same LTR index.                      |
|       | --gopLowdelay          | Whether to use the default low-delay GOP configuration.        |
|       |                        |  [0]                                                           |
|       |                        | 0 - do not use                                                 |
|       |                        | 1 - use                                                        |
|       |                        | The option is valid only if --gopConfig is not specified       |
|       |                        |  and the value of gopSize is less than or equal to 4.          |
|       | --numRefP              | 1..2 The number of reference frames kept for each P frame. [1] |
|       |                        |   Change the number of reference frames                        |
|       |                        |   for predefined GOP structure P frame to use                  |
|       | --lowdelayB            | 0..1 Use lowdelay B frame to replace regular P frame           |
|       |                        | 0 - turn off                                                   |
|       |                        | 1 - to replace predefined GOP structure P frame                |
|       |                        |   Default 0 for GOP1, default 1 for GOP size > 1 and 2pass     |
|       | --flexRefs             | The path to a custom reference description file.               |
|       |                        | For details, see Section Flexible Referenc Configuration in    |
|       |                        |  Hantro VC9000E Video Encoder Test Bench User Manual.          |
| -V[n] | --bFrameQpDelta        | -1..51 The QP delta of B-frames to the target QP. [-1]         |
|       |                        | If the option is set to -1, it indicates that no QP delta is   |
|       |                        |  set.                                                          |
|       |                        | The option is valid only if --gopConfig is not specified.      |
|       | --refRingBufEnable     | Whether to enable ring buffer for reference frame store. [0]   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | The option is valid for P or I frame encoding only. Since only |
|       |                        |  one reference frame is stored, some coding tools are invalid, |
|       |                        |  such as lookahead, reEncode, B frame, two reference P frame,  |
|       |                        |  multi-core and parallel encoding, LTR.                        |


### Rate Distortion Optimization

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --preset               | The pre-defined RDO scheme.                                    |
|       |                        | A higher value leads to higher quality but worse performance,  |
|       |                        |  and vice versa.                                               |
|       |                        | To use this option, explicitly claim it.                       |
|       | --rdoLevel             | The RDO level. [3]                                             |
|       |                        | The option value range is from 1 to 3, inclusive.              |
|       |                        | A higher value leads to higher quality but better performance, |
|       |                        |  and vice versa.                                               |
|       | --enableDynamicRdo     | Whether to enable dynamic RDO level selection. [0]             |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --dynamicRdoCu16Bias   | The bias used for 16x16 CU cost calculation in dynamic RDO     |
|       |                        |  level selection. [3]                                          |
|       |                        | The option value range is from 0 to 255, inclusive.            |
|       | --dynamicRdoCu16Factor | The factor used for 16x16 CU cost calculation in dynamic RDO   |
|       |                        |  level selection. [80]                                         |
|       |                        | The option value range is from 0 to 255, inclusive.            |
|       | --dynamicRdoCu32Bias   | The bias used for 32x32 CU cost calculation in dynamic RDO     |
|       |                        |  level selection. [2]                                          |
|       |                        | The option value range is from 0 to 255, inclusive.            |
|       | --dynamicRdoCu32Factor | The factor used for 32x32 CU cost calculation in dynamic RDO   |
|       |                        |  level selection. [32]                                         |
|       |                        | The option value range is from 0 to 255, inclusive.            |
|       | --enableRdoQuant       | Whether to enable rate-distortion optimized quantization       |
|       |                        |  (RDOQ).                                                       |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | The default value is 0 if the hardware does not support RDOQ,  |
|       |                        |  and 1 if the hardware supports RDOQ.                          |


### VUI and SEI

#### SEI

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -S[n] | --sei                  | (Only for HEVC/H.264) Whether to insert picture timing and     |
|       |                        |  buffering period SEI messages into the stream. [0]            |
|       |                        | 0 - do not insert                                              |
|       |                        | 1 - insert                                                     |
| -z[s] | --userData             | The path to an SEI user data file.                             |
|       |                        | The file is parsed and inserted as an SEI message before the   |
|       |                        |  first frame.                                                  |
|       | --extSEI               | The path to an external SEI data file.                         |
|       |                        | The file is parsed and inserted as SEI messages for each frame.|
|       | --jsonHDR              | The file with dynamic HDR10+ and/or Dolby Vision information.  |
|       |                        | If this option is specified, add the HDR_HELPER_SUPPORT=y      |
|       |                        |  option when generating the executable binary.                 |
|       | --t35=a:b              | The country code and area code. [0:0]                          |
|       |                        | - a indicates the country code, in one byte.                   |
|       |                        | - b indicates the area code, in one byte.                      |
|       |                        | For details, see Annex A and B of ITU-T Rec. T.35.             |
|       | --payloadT35File       | The path to a file that contains the T.35 payload data         |
|       |                        |  (t35_payload_bytes) in binary bytes.                          |

#### VUI

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --vuiAspectRatio       | The sample aspect ratio in the VUI. [0:0]                      |
|       |  [=aspectratioWidth    | - aspectratioWidth: The sample aspect ratio in horizontal      |
|       |                        |  direction, in an arbitrary unit.                              |
|       |  :aspectratioHeight]   | The value range is from 0 to 65535, inclusive.                 |
|       |                        | - aspectratioHeight: The sample aspect ratio in vertical       |
|       |                        |  direction, in the same arbitrary unit as aspectratioWidth.    |
|       |                        | The value range is from 0 to 65535, inclusive.                 |
|       |                        | If either aspectratioWidth or scaledHeight is set to 0, it     |
|       |                        |  indicates that the sample aspect ratio is not specified.      |
|       | --vuiVideosignalPresent| Whether to present the video signal type in the VUI. [0]       |
|       |                        | 0 - do not present                                             |
|       |                        | 1 - present                                                    |
|       | --vuiVideoFormat       | The video format in the VUI. [5]                               |
|       |                        | 0 - component                                                  |
|       |                        | 1 - PAL                                                        |
|       |                        | 2 - NTSC                                                       |
|       |                        | 3 - SECAM                                                      |
|       |                        | 4 - MAC                                                        |
|       |                        | 5 - UNDEF                                                      |
| -k[n] | --videoRange           | The video signal sample range in the encoded stream. [0]       |
|       |                        | 0 - Y samples range from 16 to 235 and UV samples range in     |
|       |                        |  from 16 to 240.                                               |
|       |                        | 1 - YUV samples range from 0 to 255.                           |
|       | --vuiColordescription  | The color description in the VUI.                              |
|       |  [=primary:transfer    | - primary: The index of chromaticity coordinates. [2]          |
|       |                        | Value 2 indicates unspecified.                                 |
|       |    :matrix]            | The value range is from 0 to 255, inclusive.For details, see   |
|       |                        | Table E.3 in ITU-T Rec. H.265.                                 |
|       |                        | - transfer: The reference of the opto-electronic transfer      |
|       |                        |  function of the source picture. [2]                           |
|       |                        | Value 1 indicates IT-R BT.709-6                                |
|       |                        | Value 2 indicates unspecified.                                 |
|       |                        | Value 14 indicates ITU-R Rec. BT.2020-2.                       |
|       |                        | Value 18 indicates ARIB STD-B67.                               |
|       |                        | For details, see Table E.4 in ITU-T Rec. H.265.                |
|       |                        | - matrix: The index of matrix coefficients used for deriving   |
|       |                        |  luma and chroma signals from green, blue, and red or Y, Z,    |
|       |                        |  and X primaries. [2]                                          |
|       |                        | Value 2 indicates unspecified.                                 |
|       |                        | The value range is from 0 to 255, inclusive.                   |
|       |                        | For details, see Table E.5 in ITU-T Rec. H.265.                |


#### HDR10

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --writeOnceHDR10       | Whether to write HDR10 information only before the first IDR   |
|       |                        |  frame. [0]                                                    |
|       |                        | 0 - write before each IDR frame.                               |
|       |                        | 1 - write only before the first IDR frame.                     |
|       | --HDR10_display        | The color volume SEI message of the mastering display.         |
|       |  [=dx0:dy0:dx1:dy1:    | - dx0: the normalized X chromaticity coordinate of component   |
|       |                        |  0. [0]                                                        |
|       |   dx2:dy2:wx:wy:       | The value range is from 0 to 50000, inclusive.                 |
|       |   max:min]             | - dy0: the normalized Y chromaticity coordinate of component   |
|       |                        |  0. [0]                                                        |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - dx1: the normalized X chromaticity coordinate of component   |
|       |                        |  1. [0]                                                        |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - dy1: the normalized Y chromaticity coordinate of component   |
|       |                        |  1. [0]                                                        |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - dx2: the normalized X chromaticity coordinate of component   |
|       |                        |  2. [0]                                                        |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - dy2: the normalized Y chromaticity coordinate of component   |
|       |                        |  2. [0]                                                        |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - wx: the normalized X chromaticity coordinate of the white    |
|       |                        |  point. [0]                                                    |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - wy: the normalized Y chromaticity coordinate of the white    |
|       |                        |  point. [0]                                                    |
|       |                        | The value range is from 0 to 50000, inclusive.                 |
|       |                        | - max: the nominal maximum display luminance. [0]              |
|       |                        | - min: the nominal minimum display luminance. [0]              |
|       | --HDR10_lightlevel     | The content light level information SEI message.               |
|       |  [=maxlevel:avglevel]  | - maxlevel: the maximum content light level.                   |
|       |                        | - avglevel: the maximum picture average light level.           |


### (Obsoleted) Noise Reduction

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --noiseReductionEnable | (Obsoleted) Whether to enable noise reduction (3DNR). [0]      |
|       |                        | 0 - disable 3DNR                                               |
|       |                        | 1 - enable 3DNR                                                |
|       | --noiseLow             | (Obsoleted) The minimum noise value. [10]                      |
|       |                        | The option value range is from 1 to 30, inclusive.             |
|       | --noiseFirstFrameSigma | (Obsoleted) The noise estimation for start frames. [11]        |
|       |                        | The option value range is from 1 to 30, inclusive.             |
|       | --noiseReductionStrength_IntraY | 0..31 denoise strength for intra Y channel [7]        |
|       | --noiseReductionStrength_IntraU | 0..31 denoise strength for intra U channel [7]        |
|       | --noiseReductionStrength_IntraV | 0..31 denoise strength for intra V channel [7]        |
|       | --noiseReductionStrength_InterY | 0..31 denoise strength for inter Y channel [7]        |
|       | --noiseReductionStrength_InterU | 0..31 denoise strength for inter U channel [7]        |
|       | --noiseReductionStrength_InterV | 0..31 denoise strength for inter V channel [7]        |
|       | --noiseReduction_ChromaMaxMV    | 0..15 only denoise U/V with MV x/y <= ChromaMaxMV [4] |


### (Obsoleted) Smart Background Detection

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --smartConfig          | (Obsoleted) The path to the custom configuration file for the  |
|       |                        |  smart algorithm.                                              |


### End-of-Sequence Disabling

| Short | Long Option   | Description                                                             |
| ----- | -----------   | ----------------------------------------------------------------------- |
|       | --disableEOS  | Whether to write end-of-sequence (EOS) bytes before stream is closed.   |
|       |               |  [0]                                                                    |
|       |               | 0 - do not write                                                        |
|       |               | 1 - write                                                               |

### Parameters for Enable intraRecon

| Short | Long Option         | Description                                                       |
| ----- | ------------------- | ----------------------------------------------------------------- |
|       | --intraReconEnable  | Enable/disable intra recon when HW support. [1]                   |
|       |                     |  0 = Disable intraRecon.                                          |
|       |                     |  1 = Enable intraRecon.                                           |

### AV1 Verification for FFmpeg Framework

| Short | Long Option              | Description                                                  |
| ----- | ------------------------ | -------------------------------------------------------------|
|       | --modifiedTileGroupSize  | Whether to modify the space size of OBU_TILE_GROUP coded by  |
|       |                          |  the hardware. [0]                                           |
|       |                          | 0 - do not modify                                            |
|       |                          | 1 - modify                                                   |
|       |                          | Use this opion for AV1 verifiction only when the FFmpeg      |
|       |                          |  framework is used.                                          |


### Re-Encoding

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --reEncode             | Whether to re-encode frames in exceptional scenarios such as   |
|       |                        |  output buffer overflow. [0]                                   |
|       |                        | 0 - do not re-encode frames                                    |
|       |                        | 1 - re-encode frames                                           |
|       |                        | For example, when an output buffer overflows, it can be        |
|       |                        |  reallocated through a callback function. Then, if this option |
|       |                        |  is set to 1, the encoder re-encodes the current frame with    |
|       |                        |  the same settings but the new  output buffer.                 |
|       |                        | Re-encoding is not supported if the value of --parallelCoreNum |
|       |                        |  is greater than 1.                                            |

### Motion Estimation and Global Motion

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --gmvFile              | (Reserved) The path to a file that contains ME search range    |
|       |                        |  offsets for reference list 0.                                 |
|       |                        | In the file, the search offsets of each frame must be listed   |
|       |                        |  sequentially line by line.                                    |
|       |                        | To set a frame-level offset, there should be only one pair of  |
|       |                        |  offsets per line for the frame. For example:                  |
|       |                        | (MVX, MVY)                                                     |
|       |                        | When SEARCH_RANGE_ROW_OFFSET_TEST is enabled, to set           |
|       |                        |  CTU-row-level offsets, there should be multiple offset pairs  |
|       |                        |  per line for all CTU rows. For example:                       |
|       |                        | (MVX0, MVY0) (MVX1, MVY1) ...                                  |
|       |                        | Common delimiters such as white spaces, commas (,), and        |
|       |                        |  semicolons (;) are allowed between offsets.                   |
|       | --gmv=MVX:MVY          | The frame-level ME search range offsets for reference list 0.  |
|       |                        |  [0:0]                                                         |
|       |                        | - MVX: the horizontal offset, in pixels.                       |
|       |                        | The value must be 64 aligned and within the range from -128    |
|       |                        |  to +128, inclusive.                                           |
|       |                        | - MVY: the vertical offset, in pixels.                         |
|       |                        | The value must be 16 aligned and within the range from -128    |
|       |                        |  to +128, inclusive.                                           |
|       |                        | If both gmvFile and gmv are specified, the settings of         |
|       |                        |  gmvFile take effect.                                          |
|       | --gmvList1=MVX:MVY     | (Reserved) The frame level ME search range offsets for         |
|       |                        |   reference list 1. [0:0]                                      |
|       |                        | - MVX: the horizontal offset, in pixels.                       |
|       |                        | The value must be 64 aligned and within the range from -128    |
|       |                        |  to +128, inclusive.                                           |
|       |                        | - MVY: the vertical offset, in pixels.                         |
|       |                        | The value must be 16 aligned and within the range from -128    |
|       |                        |  to +128, inclusive.                                           |
|       |                        | If both --gmvList1File and --gmvList1 are specified, the       |
|       |                        |  settings of --gmvList1File take effect.                       |
|       | --gmvList1File         | (Reserved) The path to a file that contains ME search range    |
|       |                        |  offsets for reference list 1.                                 |
|       |                        | In the file, the search offsets of each frame must be listed   |
|       |                        |  sequentially line by line.                                    |
|       |                        | To set a frame-level offset, there should be only one pair of  |
|       |                        |  offsets per line for the frame. For example:                  |
|       |                        | (MVX, MVY)                                                     |
|       |                        | When SEARCH_RANGE_ROW_OFFSET_TEST is enabled, to set           |
|       |                        |  CTU-row-level offsets, there should be multiple offset pairs  |
|       |                        |  per line for all CTU rows. For example:                       |
|       |                        | (MVX0, MVY0) (MVX1, MVY1) ...                                  |
|       |                        | Common delimiters such as parentheses (), white spaces,        |
|       |                        |  commas (,), and semicolons (;) are allowed between offsets.   |
|       | --MEVertRange          | The ME vertical search range, in pixels. [0]                   |
|       |                        | For HEVC (H.265) and AV1, valid option values include 0, 40,   |
|       |                        |  and 64.                                                       |
|       |                        | For H.264 (AVC), valid option values include 0, 24, 48, and   |
|       |                        |  64.                                                           |
|       |                        | If the option is set to 0, the maximum supported search range  |
|       |                        |  specified through EWLHwConfig_t.meVertSearchRangeHEVC or      |
|       |                        |  EWLHwConfig_t.meVertSearchRangeHEVC is used.                  |
|       |                        | This option is valid only if the value of                      |
|       |                        |  EWLHwConfig_t.meVertRangeProgramable is 1.                    |


## Rate Control

### Rate Control Mode

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --rcMode               | The rate control mode. [0]                                     |
|       |                        | 0 or cvbr - constrained variable bit rate (CVBR) mode          |
|       |                        | The CVBR mode reduces encoded bits in slow-motion and simple   |
|       |                        |  scenes, and uses the reduced bits for complex scenes to       |
|       |                        |  improve overall quality.                                      |
|       |                        | If --rcMode is set to 0 with options set as --cpbSize=x,       |
|       |                        |  --vbr=1, or --crf=x, the applied rate control mode is         |
|       |                        |  adjusted to another mode instead of CVBR. However, if rcMode  |
|       |                        |  is set to another value, the rcMode setting has a higher      |
|       |                        |  priority than these options.                                  |
|       |                        | 1 or cbr - constant bit rate (CBR) mode                        |
|       |                        | The CBR mode ensures output at a constant bit rate to the      |
|       |                        |  transmission path.                                            |
|       |                        | In CBR mode, make sure that cpbSize is set to an appropriate   |
|       |                        |  value.                                                        |
|       |                        | - A greater value of cpbSize helps avoid data overflow. In     |
|       |                        |  this case, to avoid underflow, you can set --hrdConformance=1.|
|       |                        | - A smaller value generates a more stable bit rate, but        |
|       |                        |  increases the risk of data overflow.                          |
|       |                        | A value of (2 x bitPerSecond) is recommended. Values less      |
|       |                        |  than bitPerSecond are not recommended.                        |
|       |                        | 2 or vbr - variable bit rate (VBR)                             |
|       |                        | The VBR mode allows for bit rate control with limitations on   |
|       |                        |  the minimum QP and maximum bit rate.                          |
|       |                        | It is recommended that --qpMin be set to an appropriate value. |
|       |                        |  --qpMin=10 is recommended.                                    |
|       |                        | NOTE: Setting --rcMode=2 is equivalent to setting --rcmode=0   |
|       |                        |  --vbr=1.                                                      |
|       |                        | 3 or abr - average bit rate (ABR) mode                         |
|       |                        | The ABR mode tunes AP based on the gap between the actual      |
|       |                        |  target bit rates instead of scene complexity. This ensures    |
|       |                        |  the average bit rate approximates the target bit rate.        |
|       |                        | 4 or crf - constant rate factor (CRF) mode                     |
|       |                        | The CRF mode ensures a certain level of quality with a         |
|       |                        |  constant slice QP derived from a given rate factor and frame  |
|       |                        |  rate.                                                         |
|       |                        | The CRF mode is available only if look-ahead encoding is       |
|       |                        |  enabled.                                                      |
|       |                        | If both CRF and VBR modes are set to be enabled, only the CRF  |
|       |                        |  mode is enabled and the VBR mode is disabled.                 |
|       |                        | In CRF mode, make sure that --crf is set to an appropriate     |
|       |                        |  value.                                                        |
|       |                        | 5 or cqp - constant quantization parameter (CQP) mode          |
|       |                        | In CQP value, a constant QP value is used for all slices,      |
|       |                        |  with a separate QP delta (--intraQpDelta) for I slices.       |
|       |                        | In CQP mode, the value of --picRc is fixed to 0.               |


### Output Stream Bit Rate

| Short | Long Option            | Description                                                  |
|-------|------------------------|--------------------------------------------------------------|
| -R[n] | --intraPicRate         | The interval between two IDR frames. [0]                     |
|       |                        | The encoder forces a frame to be encoded as an IDR frame     |
|       |                        |  every N frames.                                             |
|       |                        | If the option is set to 0, the encode does not force any     |
|       |                        |  frame to be encoded as an IDR.                              |
|       | --intraPeriod          | The interval between two non-IDR intra frames. [0]           |
|       |                        | Starting from an IDR frame, the encoder forces a frame to be |
|       |                        |  encoded as an intra frame every N frames.                   |
| -B[n] | --bitPerSecond         | The target bit rate for rate control, in bits per second.    |
|       |                        |  [1000000]                                                   |
|       |                        | The option value range is from 10000 to the maximum limited  |
|       |                        |  by the stream level, inclusive.                             |
|       |                        | The option value must be greater than or equal to 10000.     |
|       |                        | If HRD is enabled, the maximum target bit rate is limited by |
|       |                        |  the stream tier and level selected during encoder           |
|       |                        |  initialization. For detailed limitations, see ITU-T Rec.    |
|       |                        |  H.265.                                                      |
|       | --tolMovingBitRate     | The tolerance percentage of the maximum moving bit rate      |
|       |                        |  over the target bit rate. [100]                             |
|       |                        | The average bit rate of the monitored frames is limited to   |
|       |                        |  [bitPerSecond * (1 + tolMovingBitRate/100)].                |
|       |                        | The option value range is from 0 to 2000, inclusive.         |
|       | --monitorFrames        | The number of frames to be monitored for calculating the     |
|       |                        |  moving bit rate. [outputRateNumer/outputRateDenom]          |
|       |                        | The option value range is from 10 to 120, inclusive.         |
|       | --bitVarRangeI         | (Obsoleted) The permitting variance percentage over average  |
|       |                        |  bits per I-frame from the target bit rate. [10000]          |
|       |                        | The option value range is from 10 to 10000, inclusive.       |
|       | --bitVarRangeP         | The permitting variance percentage over average bits per     |
|       |                        |  P-frame from the target bit rate. [10000]                   |
|       |                        | The option value range is from 10 to 10000, inclusive.       |
|       | --bitVarRangeB         | The permitting variance percentage over average bits per     |
|       |                        |  B-frame from the target bit rate. [10000]                   |
|       |                        | The option value range is from 10 to 10000, inclusive.       |
|       | --staticSceneIbitPercent| The percentage of I-frame bits in static scenes. [80]       |
|       |                        | The option value range is from 0 to 100, inclusive.          |
|       | --crf[n]               | The constant rate factor. [-1]                               |
|       |                        | The option value range is from -1 to 51, inclusive.          |
|       |                        | If the option is set to -1, the constant rate factor mode    |
|       |                        |  is disabled.                                                |
| -U[n] | --picRc                | Whether to enable picture-level rate control to adjust QP    |
|       |                        |  between pictures. [0]                                       |
|       |                        | 0 - disable                                                  |
|       |                        | 1 - enable                                                   |
|       | --tolRcUnderflow       | [0, 99] Tolerance percent of frame RC underflow bitrate [50] |
|       |                        | Percentage of underflow bitrate tolerance for better quality |
|       |                        |   Save bits in simple scene for complex scene                |
|       |                        |   Higher the value, larger underflow may happen              |
|       |                        |   Worst underflow bitrate:TargetBits*(100-tolRcUnderflow)/100|
|       | --picQpDeltaRange      | The QP delta range in picture-level rate control.            |
|       |  [=Min:Max]            | - Min indicates the minimum QP delta in picture-level rate   |
|       |                        |  control. [-4]                                               |
|       |                        | The value range is from -10 to -1, inclusive.                |
|       |                        | - Max indicates the maximum QP delta in picture-level rate   |
|       |                        |  control. [4]                                                |
|       |                        | The value range is from 1 to 10, inclusive.                  |
|       |                        | This QP delta range applies only to two adjacent frames of   |
|       |                        |  the same coding type. It does not apply if HRD overflow     |
|       |                        |  occurs.                                                     |
|       | --fillerData           | Whether to fill data when HRD is disabled. [0]               |
|       |                        | 0 - do not fill                                              |
|       |                        | 1 - fill                                                     |
|       |                        | If this option is set to 1, cpbSize must be specified.       |
| -C[n] | --hrdConformance       | (Only for HEVC/H.264) Whether to enable HRD conformance      |
|       |                        |  checking. [0]                                               |
|       |                        | 0 - disable                                                  |
|       |                        | 1 - enable                                                   |
|       |                        | If this option is set to 1, the encoder uses the HRD model   |
|       |                        |  to restrict the bit rate variance.                          |
| -c[n] | --cpbSize              | The size of the CPB in bits. [0]                             |
|       |                        | When HRD is enabled, each encoded frame cannot be larger     |
|       |                        |  than the specified size.                                    |
|       |                        | By default, the video encoder uses the maximum size allowed  |
|       |                        |  by the stream level. Setting this field to 0 restores the   |
|       |                        |  default value. A value of (2 * bitPerSecond) is recommended.|
|       | --cpbMaxRate           | The maximum bit rate of the CPB, in bits per second. [0]     |
|       |                        | If --cpbSize is specified and --cpbMaxRate is unspecified or |
|       |                        |  less than --bitPerSecond, --cpbMaxRate is fixed to the same |
|       |                        |  value as --bitPerSecond regardless of its setting.          |
|       |                        | If --cpbMaxRate is equal to --bitPerSecond, the rate control |
|       |                        |  algorithm works in CBR mode.                                |
|       |                        | If --cpbMaxRate is greater tham --bitPerSecond, the rate     |
|       |                        |  control algorithm works in VBR mode.                        |
| -g[n] | --bitrateWindow        | 1..300 The number of frames within which the rate control    |
|       |                        |  algorithm tries to achieve the target bit rate.             |
|       |                        |  [intraPicRate]                                              |
|       |                        | The rate control algorithm allocates bits for each window    |
|       |                        |  and tries to match the target bit rate at the end of the    |
|       |                        |  window.                                                     |
|       |                        | Typical windows begin with an intra frame, which is not      |
|       |                        |  mandatory.                                                  |
|       | --LTR=a:b:c[:d]        | The long term reference (LTR) settings.                      |
|       |                        | - a: the POC delta between two LTR frames.                   |
|       |                        | - b: the POC delta between the LTR frame and the first       |
|       |                        |  subsequent frame that uses the LTR frame as reference.      |
|       |                        | - c: the POC delta between two adjacent frames that use the  |
|       |                        |  same LTR frame as reference.                                |
|       |                        | - d: the QP delta for frames using LTR as reference. [0]     |
| -s[n] | --picSkip              | Whether to allow skippint pictures for bit rate control. [0] |
|       |                        | 0 - disallow                                                 |
|       |                        | 1 - allow                                                    |
| -q[n] | --qpHdr                | -1..51 The default or initial picture-level QP value.        |
|       |                        | Default 26 for fixed QP method.                              |
|       |                        | Default -1 for frameRC method. when qpHdr is -1, the encoder |
|       |                        | calculates the initialQP according to the target bit rate    |
|       |                        | when enable FrameRc.                                         |
| -n[n] | --qpMin                | 0..51 The minimum QP for all slices. [0]                     |
| -m[n] | --qpMax                | 0..51 The maximum QP for all slices. [51]                    |
|       | --qpMinI               | 0..51 The minimum QP for I slices. [0]                       |
|       |                        | For I slices, if both --qpMin and --qpMinI are specified,    |
|       |                        |  --qpMinI takes precedence.                                  |
|       | --qpMaxI               | 0..51 The maximum QP for I slices. [51]                      |
|       |                        | For I slices, if both --qpMax and --qpMaxI are specified,    |
|       |                        |  --qpMaxI takes precedence.                                  |
| -A[n] | --intraQpDelta         | -51..51 The delta between the target QP and intra QP values. |
|       |                        |  [-5]                                                        |
| -G[n] | --fixedIntraQp         | 0..51 The fixed QP value for all intra frames. [0]           |
|       |                        | If this option is set to 0, the QP value for intra frames is |
|       |                        |  not fixed.                                                  |
| -I[n] | --chromaQpOffset       | -12..12 The chroma QP offset. [0]                            |
|       | --vbr                  | Whether to enable variable bit rate control based on the     |
|       |                        |  minimum QP allowed. [0]                                     |
|       |                        | 0 - disable                                                  |
|       |                        | 1 - enable                                                   |
|       | --sceneChange          | The frames with scene changes.                               |
|       |                        | Set the option value in format Frame1:Frame2:...:Frame20.    |
|       |                        |  Separate frame numbers with colons (:).                     |
|       |                        |  A maximum of 20 scene change frames can be specified.       |
|       | --gdrDuration          | The number of the P frame to be refreshed as an I frame in   |
|       |                        |  gradual decoder refresh (GDR). [0]                          |
|       |                        | If the option is set to 0, GDR is disabled.                  |
|       |                        | NOTE: The starting point of GDR is the frame with type set   |
|       |                        |  to VCENC_INTRA_FRAME. intraArea and roi1Area are used to    |
|       |                        |  implement the GDR function. The GDR begin to work from the  |
|       |                        |  second IDR frame.                                           |
|       | --skipFramePOC         | The POC of the frame to be force encoded as a skip frame.    |
|       |                        |  [0]                                                         |
|       |                        | If this option is set to 0, the encoder does not force       |
|       |                        |  enocde any frame as a skip frame.                           |
|       | --insertIDR            | The picture_cnt of the frame to be force encoded as an IDR   |
|       |                        |  frame.                                                      |
|       |                        | If this option is set to 0, the encoder does not force       |
|       |                        |  enocde any frame as an IDR frame.                           |
|       | --minIprop             | 0..100  The minimum ratio of I-frame bits to P-frame bits    |
|       |                        |  (I/P ratio).                                                |
|       | --maxIprop             | minIprop..100 The maximum ratio of I/P ratio.                |
|       |                        | The range specified by the --minIprop and --maxIprop options |
|       |                        |  is used to clamp the I/P ratio.                             |
|       |                        | A larger value of --minIprop leads to clear I-frames and     |
|       |                        |  blurred P-frames.                                           |
|       |                        | It is not recommended to restrict it, in order to avoid      |
|       |                        |  breathing effects and rate fluctuations.                    |
|       | --changePos            | 50..100 The ratio of the bit rate at the initialization of   |
|       |                        |  QP adjustment to the maximun bit rate.                      |
|       | --hieQpDeltaEnable     | [0,1] Frame QP delta based on GOP hierarchical layer. [1]    |
|       |                        |   Only available in single pass encoding, default 0 for GOP1 |
|       |                        |  0 : Disable.                                                |
|       |                        |  1 : Enable.                                                 |
|       | --aifEnable            | Whether to enable anti-intra flicker (AIF). [0]              |
|       |                        |  0 - disable                                                 |
|       |                        |  1 - enable                                                  |
|       |                        | NOTE: AIF works only for 1-pass 8-bit encoding. It is not    |
|       |                        |  available for multi-task, GDR, or low-latency encoding or   |
|       |                        |  if the input is tiled.                                      |
|       | --aifQpDelta           | -51..51 The QP delta of AIF frames to their reference        |
|       |                        |  frames. [-5]                                                |


### Block-level rate control

| Short | Long Option              | Description                                                  |
|-------|--------------------------|--------------------------------------------------------------|
| -u[n] | --ctbRc                  | The block-level rate control mode for adjusting QP inside a  |
|       |                          |  frame. [0]                                                  |
|       |                          | 0 - block-level rate control disabled                        |
|       |                          | 1 - subjective rate control only                             |
|       |                          | 2 - precise rate control only                                |
|       |                          | This value is available only if the value of                 |
|       |                          |  EWLHwConfig_t.CtbRcVersion is greater than or equal to 1.   |
|       |                          | 3 - mixed subjective and precise rate control                |
|       |                          | This value is available only if the value of                 |
|       |                          |  EWLHwConfig_t.CtbRcVersion is greater than or equal to 1.   |
|       |                          | 4 - (Reserved) precise rate control only                     |
|       |                          | 6 - (Reserved) mixed subjective and precise rate control     |
|       | --blockRCSize            | The block size for block-level rate control [0]              |
|       |                          | 0 - 64x64 pixels                                             |
|       |                          | 1 - 32x32 pixels                                             |
|       |                          | 2 - 16x16 pixels                                             |
|       | --rcQpDeltaRange         | The maximum absolute delta between block-level and           |
|       |                          |  picture-level QP values in block-level rate control. [10]   |
|       |                          | If the value of EWLHwConfig_t.CtbRcVersion is less than 1,   |
|       |                          |  the option value range is from 0 to 15, inclusive.          |
|       |                          | If the value of EWLHwConfig_t.CtbRcVersion is greater than   |
|       |                          |  or equal to 1, the option value range is from 0 to 51,      |
|       |                          |  inclusive.                                                  |
|       | --rcBaseMBComplexity     | The MB complexity threshold for subjective block-level rate  |
|       |                          |  control. [15]                                               |
|       |                          | When the MB complexity equals the threshold, the             |
|       |                          |  block-level QP is not adjusted.                             |
|       |                          | The QP value increases when the MB complexity exceeds the    |
|       |                          |  threshold, and vice versa.                                  |
|       |                          | The option value range is from 0 to 31, inclusive.           |
|       | --tolCtbRcInter          | The tolerance of block-level rate control for inter frames.  |
|       |                          |  [-1.0]                                                      |
|       |                          | The rate control algorithm tries to limit inter frame bits   |
|       |                          |  within range from targetPicSize/(1 + tolCtbRcInter) to      |
|       |                          |  targetPicSize * (1 + tolCtbRcInter).                        |
|       |                          | The --tolCtbRcInter option value is a floating-point number. |
|       |                          | A negative value indicates no bit rate limit in block-level  |
|       |                          |  rate control.                                               |
|       | --tolCtbRcIntra          | The tolerance of block-level rate control for intra frames.  |
|       |                          |  [-1.0]                                                      |
|       |                          | The rate control algorithm tries to limit intra frame bits   |
|       |                          |  within range from targetPicSize/(1 + tolCtbRcIntra) to      |
|       |                          |  targetPicSize * (1 + tolCtbRcIntra).                        |
|       |                          | The --tolCtbRcIntra option value is a floating-point number. |
|       |                          | A negative value indicates no bit rate limit in block-level  |
|       |                          |  rate control.                                               |
|       | --ctbRcRowQpStep         | The maximum accumulated QP adjustment step per CTB Row       |
|       |                          |  allowed by CTB rate control                                 |
|       |                          |  ctbrcv1:  default value is [4]                              |
|       |                          |  ctbrcv2:  0..7 and default [2]                              |
|       | --ctbRcRowQpDeltaRange   | 0..31 Qp Delta based on picQP[1].                            |
|       |                          | Only work when ctbrcV2                                       |
|       | --ctbRcDirection         | 0..15 Subscript of minus direction threshold. [8]            |
|       |                          | It used with ctbRcThreshold to decrease/increase block QP.   |
|       |                          | When the block complexity is between ctbRcThreshold[0] and   |
|       |                          |  ctbRcThreshold[Direction] will decrease QP, and the         |
|       |                          |  complexity is bigger than ctbRcThreshold[Direction] will    |
|       |                          |  increase QP.                                                |
|       |                          | It is used for subjective block-level rate control.          |
|       | --ctbRcThresholdI        | 0..255 A threshold array, it used with ctbRcDirection to     |
|       |  =a0:a1:a2:a3:a4:a5:a6:a7:|  decrease/increase block QP of Intra frame.                 |
|       |  a8:a9:a10:a11:a12:a13:  |  [0:0:0:0:3:3:5:5:6:6:6:11:11:13:17:17]                      |
|       |  a14:a15                 | It is used for subjective block-level rate control.          |
|       | --ctbRcThresholdP        | 0..255 A threshold array, it used with ctbRcDirection to     |
|       |  =b0:b1:b2:b3:b4:b5:b6:b7:|  decrease/increase block QP of P frame.                     |
|       |  b8:b9:b10:b11:b12:b13:  |  [0:0:0:0:3:3:5:5:6:6:6:11:11:13:17:17]                      |
|       |  b14:b15                 | It is used for subjective block-level rate control.          |
|       | --ctbRcThresholdB        | 0..255 A threshold array, it used with ctbRcDirection to     |
|       |  =c0:c1:c2:c3:c4:c5:c6:c7:|  decrease/increase block QP of B frame.                     |
|       |  c8:c9:c10:c11:c12:c13:  |  [0:0:0:0:3:3:5:5:6:6:6:11:11:13:17:17]                      |
|       |  c14:c15                 | It is used for subjective block-level rate control.          |
|       | --ctbRcSkinQPDelta       | 0..7 Negative qp delta of skin area for subjective           |
|       |                          |  block-level rate control. [2]                               |
|       | --ctbRcSkinMinQPDelta    | 0..15 It subtract ctbRcDirection value is the minimal        |
|       |                          |  qp delta of skin area for subjective block-level rate       |
|       |                          |  control. [3]                                                |
|       |                          | Skin area not set too small QPDelta, so use                  |
|       |                          | ctbRcSkinMinQPDelta - direction to limit it.                 |
|       |                          | ctbRcSkinMinQPDelta - direction always less than 0.          |
|       | --ctbRcSkinCbRange=a:b   | a:b 0..255 Cb value range to be regarded as skin Area for    |
|       |                          |  subjective block-level rate control.[0:0]                   |
|       |                          | We use Skin area color range to detect skin Area, otherwize  |
|       |                          |  use default algorithm to detect skin area.                  |
|       | --ctbRcSkinCrRange=a:b   | a:b 0..255 Cr value range to be regarded as skin Area for    |
|       |                          |  subjective block-level rate control.[0:0]                   |
|       | --ctbRcSkinLumRange=a:b  | a:b 0..255 Lum value range to be regarded as skin Area for   |
|       |                          |  subjective block-level rate control.[0:255]                 |

### Look-Ahead Encoding

| Short | Long Option      | Description                                                          |
| ----- | ---------------- | ---------------------------------------------------------------------|
|       | --lookaheadDepth | The number of look-ahead frames. [0]                                 |
|       |                  | 0 - disables look-ahead encoding.                                    |
|       |                  | 4..40 - enables look-ahead encoding with the specified number of     |
|       |                  |  look-ahead frames.                                                  |
|       | --halfDsInput    | The path to the file that contains the external provided             |
|       |                  |  half-downsampled YUV input.                                         |
|       | --aq_mode        | The adaptive quantization mode. [0]                                  |
|       |                  | 0 - none                                                             |
|       |                  | 1 - uniform AQ                                                       |
|       |                  | 2 - auto variance                                                    |
|       |                  | 3 - auto variance with bias to dark scenes                           |
|       | --aq_strength    | 0..3.0 The strength of adaptive quantization. [1.0]                  |
|       |                  | A great value reduces blocking and blurring in flat and textured     |
|       |                  |  areas.                                                              |
|       | --tune           | The type of the target based on which some encoding settings         |
|       |                  |  are forced for quality tuing. [0]                                   |
|       |                  | 0 or psnr - PSNR with forced settings --aq_mode=0 --psyFactor=0      |
|       |                  | 1 or ssim - SSIM with forced settings --aq_mode=2 --psyFactor=0      |
|       |                  | 2 or visual - Visual with forced settings --aq_mode=2                |
|       |                  |  --psyFactor=0.75                                                    |
|       |                  | 3 or sharpness_visual - sharpness-based visual with forced           |
|       |                  |  parameters --aq_mode=2 --psyFactor=0.75 --inLoopDSRatio=0           |
|       |                  |  --enableRdoQuant=0                                                  |
|       |                  | 4 or vmaf - (Reserved) VMAF pre-processing enabled                   |
|       | --inLoopDSRatio  | The in-loop down-scaling ratio for the pass-1 encoder. [1]           |
|       |                  | 0 - down-scaling disabled                                            |
|       |                  | 1 - 1/2 down-scaling ratio                                           |

## Visual Quality

### Parameters Affecting Visual Tools
| Short | Long Option              | Description                                                  |
|-------| -------------------------|--------------------------------------------------------------|
|       | --ctbRcTrailStrengthMax  | 0..3 Trailing area detection max strength. [0]               |
|       |                          | 0 turns off trailing detection. Suggested value is [3]       |
|       |                          | Only available with GOP size 1 and ctbRc Version=2           |
|       | --ctbRcTrailDeltaQp      | -7..0 Trailing area base qp delta. [-4]                      |
|       |                          | QP delta for 16x16 trailing CU, adjusted by CU size          |
|       | --IntraBiasChromaStrength| 0..10 Chroma error detection strength. Default off; to       |
|       |                          |   activate this feature, the suggested value is 7 [0]        |
|       |                          | 0 turns off chroma error detection.                          |
|       | --IntraBiasStrength      | 0..15 Strength of intra mode is preferred which will         |
|       |                          |   guarantee visual quality.  Default off; to activate this   |
|       |                          |   feature, the suggested value is 10 [0]                     |
|       |                          | 0 turns off intra bias.                                      |
|       | --IntraBiasMvThreshold   | Int Mv limit for intra bias decision.[5]                     |
|       |                          | 0 no mv limit for intra bias decision.                       |
|       | --bTrailAvoidIntraBias   | Whether intraBias is disabled in the trailing area.[1]       |
|       |                          | 0 enable intraBias in the trailing area.                     |
|       |                          | 1 disable intraBias in the trailing area.                    |
|       | --visualBitRateTolerance | -1..1000 Percent of target bitrate tolerance allowed for     |
|       |                          |   visual tools to exceed. [50]                               |
|       |                          | -1 no limit for visual tools to spend extra bits             |
|       |                          | Trail Reduce and Intra Bias will adjust strength at frame    |
|       |                          |   level adaptively to keep bitrate less than                 |
|       |                          |   (1 + visualBitRateTolerance/100)*target bitrate            |
|       |                          | Only available with GOP size 1                               |

## ROI

### ROI-Based Coding Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --cir                  | Forces one MB or CTB to be encoded with intra mode at an       |
|       |  =[start:interval]     |                                                                |
|       |                        | - start: the order count of the start MB or CTB.               |
|       |                        | - interval: the interval.                                      |
|       | --intraArea            | The ROI for forcing the intra mode.                            |
|       |  =left:top:right:bottom| - left: the leftmost MB or CTB column inside the ROI.          |
|       |                        | - top: the top MB or CTB column inside the ROI.                |
|       |                        | - right: the rightmost MB or CTB column inside the ROI.        |
|       |                        | - bottom: the bottom MB or CTB column inside the ROI.          |
|       | --ipcm1Area            | ROI 1 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom| - left: the leftmost MB or CTB column inside the ROI.          |
|       |                        | - top: the top MB or CTB column inside the ROI.                |
|       |                        | - right: the rightmost MB or CTB column inside the ROI.        |
|       |                        | - bottom: the bottom MB or CTB column inside the ROI.          |
|       | --ipcm2Area            | ROI 2 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcm3Area            | ROI 3 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcm4Area            | ROI 4 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcm5Area            | ROI 5 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcm6Area            | ROI 6 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcm7Area            | ROI 7 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcm8Area            | ROI 8 for forcing the IPCM mode.                               |
|       |  =left:top:right:bottom|                                                                |
|       | --ipcmMapEnable        | Whether to enable the ROI map for forcing IPCM. [0]            |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --ipcmMapFile          | The path to the file that contains the IPCM map.               |
|       |                        | The IPCM map defines the IPCM flag for each block in the frame.|
|       |                        | For the IPCM map, the block size is 64x64 pixels for HEVC      |
|       |                        |  (H.265) and 16x16 pixels for H.264 (AVC).                     |
|       | --roi1Area             | ROI 1 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom| - left: the leftmost MB or CTB column inside the ROI.          |
|       |                        | - top: the top MB or CTB column inside the ROI.                |
|       |                        | - right: the rightmost MB or CTB column inside the ROI.        |
|       |                        | - bottom: the bottom MB or CTB column inside the ROI.          |
|       | --roi2Area             | ROI 2 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi3Area             | ROI 3 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi4Area             | ROI 4 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi5Area             | ROI 5 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi6Area             | ROI 6 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi7Area             | ROI 7 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi8Area             | ROI 8 for forcing a QP delta or an absolute QP value.          |
|       |  =left:top:right:bottom|                                                                |
|       | --roi1DeltaQp          | The QP delta value for blocks in ROI 1. [0]                    |
|       |                        | If absolute QP is supported, the                               |
|       |                        |  option value range is from -51 to 51, inclusive.              |
|       |                        | If absolute QP is not supported, the                           |
|       |                        |  option value range is from -30 to 0, inclusive.               |
|       |                        | Set either the QP delta or absolute QP value for each ROI.     |
|       | --roi2DeltaQp          | The QP delta value for blocks in ROI 2. [0]                    |
|       | --roi3DeltaQp          | The QP delta value for blocks in ROI 3. [0]                    |
|       | --roi4DeltaQp          | The QP delta value for blocks in ROI 4. [0]                    |
|       | --roi5DeltaQp          | The QP delta value for blocks in ROI 5. [0]                    |
|       | --roi6DeltaQp          | The QP delta value for blocks in ROI 6. [0]                    |
|       | --roi7DeltaQp          | The QP delta value for blocks in ROI 7. [0]                    |
|       | --roi8DeltaQp          | The QP delta value for blocks in ROI 8. [0]                    |
|       | --roi1Qp               | 0..51 The absolute QP value for blocks in ROI 1. [-1]          |
|       |                        | Negative values are invalid.                                   |
|       |                        | Options --roi1Qp to --roi8Qp are valid only if absolute QP is  |
|       |                        |  supported.                                                    |
|       |                        | Set either the QP delta or absolute QP value for each ROI.     |
|       | --roi2Qp               | 0..51 The absolute QP value for blocks in ROI 2. [-1]          |
|       | --roi3Qp               | 0..51 The absolute QP value for blocks in ROI 3. [-1]          |
|       | --roi4Qp               | 0..51 The absolute QP value for blocks in ROI 4. [-1]          |
|       | --roi5Qp               | 0..51 The absolute QP value for blocks in ROI 5. [-1]          |
|       | --roi6Qp               | 0..51 The absolute QP value for blocks in ROI 6. [-1]          |
|       | --roi7Qp               | 0..51 The absolute QP value for blocks in ROI 7. [-1]          |
|       | --roi8Qp               | 0..51 The absolute QP value for blocks in ROI 8. [-1]          |
|       | --roiMapDeltaQpEnable  | Whether to enable the ROI map for forcing QP delta or absolute |
|       |                        |  QP values. [0]                                                |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | The block size is specified by --roiMapDeltaQpBlockUnit.       |
|       |                        | The QP values are specified by --roiMapDeltaQpFile in text     |
|       |                        |  format or --roiMapInfoBinFile in binary format.               |
|       |                        | The map size is calculated according to the picture width and  |
|       |                        |  height after being aligned to 64 pixels.                      |
|       | --roiMapDeltaQpBlockUnit| The block size for the QP ROI map. [0]                        |
|       |                        | 0 - 64x64 pixels                                               |
|       |                        | 1 - 32x32 pixels                                               |
|       |                        | 2 - 16x16 pixels                                               |
|       |                        | 3 - 8x8 pixels                                                 |
|       |                        | For H.264 (AVC), 8x8 pixel block is not supported.             |
|       | --roiMapConfigFile     | The path to the ROI definition file.                           |
|       | --roiMapDeltaQpFile    | The path to the file that contains the QP map in text format.  |
|       |                        | The QP map defines the delta QP or absolute QP value for each  |
|       |                        |  block in the frame.                                           |
|       |                        | If --RoiQpDeltaVer is set to 1, 2, or 3, both QP delta and     |
|       |                        |  absolute QP are supported.                                    |
|       |                        | - The QP delta value range is from -51 to 51, inclusive.       |
|       |                        | - The absolute QP value range is from 0 to 51, inclusive. Each |
|       |                        |  absolute QP value must be prefixed with 'a', for example,     |
|       |                        |  'a26'.                                                        |
|       |                        | If --RoiQpDeltaVer is set to 4, only QP delta is supported.    |
|       |                        | - The QP delta value range is from -32 to 31, inclusive.       |
|       | --roiMapInfoBinFile    | The path to the file that contains the QP map in binary format.|
|       |                        | The QP map in binary format uses one byte to define the delta  |
|       |                        |  QP or absolute QP value for each block in the frame.          |
|       |                        |  The block size is specified by --roiMapDeltaQpBlockUnit.      |
|       |                        | The byte bitmap of the QP map depends on the setting of        |
|       |                        |  --RoiQpDeltaVer.                                              |
|       |                        | If --RoiQpDeltaVer is set to 1, 2, or 3, both QP delta and     |
|       |                        |  absolute QP are supported.                                    |
|       |                        | - The QP delta value range is from -51 to 51, inclusive.       |
|       |                        | - The absolute QP value range is from 0 to 51, inclusive.      |
|       |                        | If --RoiQpDeltaVer is set to 4, only QP delta is supported.    |
|       |                        | - The QP delta value range is from -32 to 31, inclusive.       |
|       |                        | This option is valid only when the value of --lookaheadDepth   |
|       |                        |  is equal to 0.                                                |
|       | --RoiQpDeltaVer        | 1..4 The format version of the ROI map.                        |
|       |                        | This option determines the format used in the file specified   |
|       |                        |  by --roiMapInfoBinFile.                                       |
|       |                        | For details about ROI map formats, see Hantro VC9000E Series   |
|       |                        |  Memory Buffer and Format Organization.                        |
|       | --RoimapCuCtrlInfoBinFile| The path to the file that contains the CU control map        |
|       |                        |  in binary format.                                             |
|       |                        | The CU control map defines the CU control information for each |
|       |                        |  block in the frame. The block size is specified by            |
|       |                        |  --roiMapDeltaQpBlockUnit.                                     |
|       |                        | If both a CU control map and a QP map are enabled and          |
|       |                        |  specified, the QP map takes precedence and the QP information |
|       |                        |  in the CU control map is ignored.                             |
|       |                        | The CU control map size depends on the setting of              |
|       |                        |  --RoiCuCtrlVer.                                               |
|       |                        | The encoded picture is divided into controlled blocks          |
|       |                        |  according to the picture width and height after being aligned |
|       |                        |  to 64 pixels.                                                 |
|       | --RoiCuCtrlVer         | 0, 3..7 The format version of the CU control map.              |
|       |                        | This option determines the format used in the file specified   |
|       |                        |  by --RoimapCuCtrlInfoBinFile. Setting it to 0 indicates to    |
|       |                        |  disable the CU control map.                                   |
|       |                        | For details about CU control map formats, see Hantro VC9000E   |
|       |                        |  Series Memory Buffer and Format Organization.                 |
|       | --roiMapDeltaQpBinFile | The path to the file that contains the QP map in binary format |
|       |                        |  for pass-2 encoding.                                          |
|       |                        | Each byte in the file indicates the delta QP of a block unit.  |
|       |                        |  The block unit size is defined by --roiMapDeltaQpBlockUnit.   |
|       |                        | The encoded picture is divided into controlled blocks          |
|       |                        |  according to the picture width and height after being aligned |
|       |                        |  to 64 pixels.                                                 |
|       |                        | In the file, data is arranged in raster-scan order of blocks   |
|       |                        |  within each frame and frame data is organized in display      |
|       |                        |  order.                                                        |
|       | --RoimapCuCtrlIndexBinFile | (Reserved) The path to the CU control map index binary     |
|       |                        |  file.                                                         |
|       |                        | The file specifies the ROIs for which the ROI information      |
|       |                        |  in the --RoimapCuCtrlInfoBinFile is valid.                    |
|       |                        | If --RoimapCuCtrlInfoBinFile is not specified, all ROI         |
|       |                        |  information in the --RoimapCuCtrlInfoBinFile is valid.        |

### Skip-Mode Map

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --skipMapEnable        | Whether to enable the skip-mode map. [0]                       |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | To enable skip-mode map, you also need to specify              |
|       |                        |  --RoiQpDeltaVer=2 if one of the following condition is met:   |
|       |                        | - The value of EWLHwConfig_t.ROIMapVersion is 2 with           |
|       |                        |  HW_ID_MAJOR >= 0x82 or (HW_ID_MAJOR == 0x60 &&                |
|       |                        |  HW_ID_MINOR>=0x010).                                          |
|       |                        | - The value of EWLHwConfig_t.ROIMapVersion is 3.               |
|       |                        | In other cases, specifying --RoiQpDeltaVer is not requried.    |
|       | --skipMapBlockUnit     | The block size for the skip-mode map. [0]                      |
|       |                        | 0 - 64x64 pixels                                               |
|       |                        | 1 - 32x32 pixels                                               |
|       |                        | 2 - 16x16 pixels                                               |
|       |                        | For HEVC (H.265), only 64x64 and 32x32 pixel blocks are        |
|       |                        |  supported.                                                    |
|       | --skipMapFile          | The path to the file that contains the skip-mode map.          |
|       |                        | The skip-mode map defines whether each block in the frame      |
|       |                        |  is force encoded in skip mode.                                |
|       |                        | - Value 0 indicates not to force encode the block in skip mode.|
|       |                        | - Value 1 indicates to force encode the block in skip mode.    |
|       |                        | The block width and height are calculated based on the picture |
|       |                        |  width and height after alignment to 64 pixels should be used. |


### RDOQ Map

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --rdoqMapEnable        | Whether to enable the RDOQ map. [0]                            |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |


## Coding Statistics Output

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --enableOutputCuInfo   | Whether to enable CU/MB statistics output to DDR. [0]          |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | 2 - enable and write the statistics to the cuInfo.txt file.    |
|       | --cuInfoVersion        | The CU/MB statistics format. [-1]                              |
|       |                        | -1 - format automatically decided based on software and        |
|       |                        |  hardware support.by VCE and HW support.                       |
|       |                        | 0 - format 0, which is available only if the value of          |
|       |                        |  EWLHwConfig_t.cuInforVersion is 0.                            |
|       |                        | 1 - format 1, which is available if the value of               |
|       |                        |  EWLHwConfig_t.cuInforVersion is 1 or 2.                       |
|       |                        | 2 - format 2, which is available only if the value of          |
|       |                        |  EWLHwConfig_t.cuInforVersion is 2.                            |
|       |                        | If look-ahead encoding is enabled, --cuInfoVersion only        |
|       |                        |  affects collected statistics of the pass-2 encoder output.    |
|       |                        |  The pass-1 encoder is collected as IM input in format 2.      |
|       |                        | For CuTree, only format 1 is supported.                        |
|       | --enableOutputCtbBits  | Whether to enable bit statistics output to DDR. [0]            |
|       |                        | 0 - disable.                                                   |
|       |                        | 1 - enable with 2-byte data per CTB, in raster scan order      |
|       |                        | of CTBs                                                        |
|       |                        | 2 - enable and write the statistics to the ctbBits.txt file.   |
|       | --hashtype             | The method to calculate the hash value of the output stream.   |
|       |                        |  [0]                                                           |
|       |                        | 0 - hash value calculation disabled                            |
|       |                        | 1 - standard CRC32                                             |
|       |                        | 2 - 32-bit checksum                                            |
|       | --ssim                 | Whether to enable SSIM calculation. [1]                        |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --psnr                 | Whether to enable PSNR calculation. [1]                        |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --enableVuiTimingInfo  | Whether to write VUI timing information in SPS. [1]            |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --enableFrameInfoVersion| Whether to enable frame statistics and luma statistics        |
|       |                        | output to DDR. [0]                                             |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - frame information                                          |
|       |                        | 2 - luma information and  frame information                    |
|       | --sse0Enable           | Whether to enable region 0 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse1Enable           | Whether to enable region 1 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse2Enable           | Whether to enable region 2 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse3Enable           | Whether to enable region 3 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse4Enable           | Whether to enable region 4 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse5Enable           | Whether to enable region 5 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse6Enable           | Whether to enable region 6 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse7Enable           | Whether to enable region 7 for SSE statistics.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse0Rect             | Region 0 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse1Rect             | Region 1 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse2Rect             | Region 2 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse3Rect             | Region 3 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse4Rect             | Region 4 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse5Rect             | Region 5 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse6Rect             | Region 6 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --sse7Rect             | Region 7 for SSE statistics.                                   |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |


## Peripheral and Hardware Setup

### Reference Frame Compression

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --compressor           | Whether to enable embedded reference frame compression. [0]    |
|       |                        | 0 - disable compression                                        |
|       |                        | 3 - enable compression for both luma and chroma data           |
|       | --enableP010Ref        | Whether to store reference frames in tiled or raster P010      |
|       |                        |  format in buffers. [0]                                        |
|       |                        | 0 - tiled                                                      |
|       |                        | 1 - raster                                                     |


### OSD & Mosaic

#### OSD Control

The encoder supports a maximum of 12 OSD regions. The same CTB cannot be shared across OSD regions.

In the following table, index N can be 01 to 12. For example, the option to configure the input file of OSD region 1 is <code>\-\-olInput01</code>.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --overlayEnables       | Each bit indicates whether to enable the corresponding overlay |
|       |                        |  region. [0]                                                   |
|       |                        | For example:                                                   |
|       |                        | - Value 1 indicates region 1 enabled.                          |
|       |                        | - Value 2 indicates region 2 enabled.                          |
|       |                        | - Value 3 indicates regions 1 and 2 enabled.                   |
|       | --olInputN             | The path to the file that contains the input file to each      |
|       |                        |  OSD region. [olInputi.yuv]                                    |
|       | --olFormatN            | The input format of each OSD region. [0]                       |
|       |                        | 0 - ARGB8888                                                   |
|       |                        | 1 - NV12                                                       |
|       |                        | 2 - bitmap                                                     |
|       | --olSuperTileN         | Whether the OSD input data is organized in supertile mode. [0] |
|       |                        | 0 - non-supertile mode.                                        |
|       |                        | 1 - X-major supertile mode.                                    |
|       |                        | 2 - Y-major supertile mode.                                    |
|       |                        | NOTE: super tile is only valid when input format is ARGB8888.  | 
|       | --olAlphaN             | 0..255 The global alpha value for the OSD region. [0]          |
|       |                        | This option is invalid for ARGB8888.                           |
|       | --olWidthN             | The width of each OSD input. [0]                               |
|       |                        | If a region is enabled, this option must be specified for the  |
|       |                        |  region.                                                       |
|       |                        | The option value must be 8 aligned for the bitmap format.      |
|       | --olHeightN            | The height of each OSD input. [0]                              |
|       |                        | If a region is enabled, this option must be specified for the  |
|       |                        |  region.                                                       |
|       |                        | The option value must be 8 aligned for the bitmap format.      |
|       | --olXoffsetN           | The horizontal offset, in pixels, of the top-left corner of    |
|       |                        |  each OSD region relative to the encoder picture. [0]          |
|       |                        | The option value must be 2 aligned.                            |
|       | --olYoffsetN           | The vertical offset, in pixels, of the top-left corner of      |
|       |                        |  each OSD region relative to the encoder picture. [0]          |
|       |                        | The option value must be 2 aligned.                            |
|       | --olYStrideN           | The Y stride of each OSD input, in bytes.                      |
|       |                        | For ARGB8888, the default value is (olWidthi * 4).             |
|       |                        | For NV12, the default value is olWidthi                        |
|       |                        | For bitmap, the default value is (olWidthi/8).                 |
|       | --olUVStrideN          | The UV stride of each OSD input, in bytes.                     |
|       |                        | The default value depends on the Y stride.                     |
|       | --olCropXoffsetN       | The horizontal offset, in pixels, of the top-left corner of    |
|       |                        |  the crop area in each OSD input relative to the overlay       |
|       |                        |  input picture. [0]                                            |
|       |                        | The option value must be 8 aligned for the bitmap format, and  |
|       |                        |  2 aligned for other formats.                                  |
|       | --olCropYoffsetN       | The vertical offset, in pixels, of the top-left corner of      |
|       |                        |  the crop area in each OSD input relative to the overlay       |
|       |                        |  input picture. [0]                                            |
|       |                        | The option value must be 8 aligned for the bitmap format, and  |
|       |                        |  2 aligned for other formats.                                  |
|       | --olCropWidthN         | The width of the crop area in each OSD input, which is the     |
|       |                        |  width of the final OSD region. [olWidthi]                     |
|       |                        | The option value must be 8 aligned for the bitmap format.      |
|       | --olCropHeightN        | The height of the crop area in each OSD input, which is the    |
|       |                        |  height of the final OSD region. [olHeighti]                   |
|       |                        | The option value must be 8 aligned for the bitmap format.      |
|       | --olBitmapYN           | The Y value for the bitmap format. [0]                         |
|       | --olBitmapUN           | The U value for the bitmap format. [0]                         |
|       | --olBitmapVN           | The V value for the bitmap format. [0]                         |

#### OSD Map

The encoder supports a maximum of 12 colors for the OSD map.

In the following table, index N can be 01 to 12. For example, the option to specify the alpha value for OSD color 1 is <code>--osdMapAlpha01</code>.

Compared with overlay and mosaic regions, the OSD map has the lowest priority. The OSD map and OSD regions with super tile input cannot be enabled at the same time.


| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --osdMapEnable         | The OSD map enable. [0]                                        |
|       |                        |   0:disable                                                    |
|       |                        |   1:enable                                                     |
|       | --osdMapInput          | The input file for the OSD map. [NULL]                         |
|       | --osdMapStride         | The OSD map stride in bytes. [0]                               |
|       |                        | The stride must be greater than or equal to                    |
|       |                        |  (codingWidth + 15[or 7]) >> 4[or 3] where codingWidth equals  |
|       |                        |  the value of --width, because the basic unit of the 4-bit OSD |
|       |                        |  map is a 8x8 or 4x4 pixel block.                              |
|       | --osdMapBlockSize      | OSD Map different block size in pixel. [0]                     |
|       |                        |   8:8x8                                                        |
|       |                        |   4:4x4                                                        |
|       | --osdMapAlphaN         | 0..255 The alpha value for color N. [0]                        |
|       | --osdMapYN             | The Y value of the OSD map color N. [0]                        |
|       | --osdMapUN             | The U value of the OSD map color N. [0]                        |
|       | --osdMapVN             | The V value of the OSD map color N. [0]                        |


#### Mosaic Control

The encoder supports a maximum of 12 mosaic regions. Each mosaic region must be aligned to the CTB size.

In the following table, index N can be 01 to 12. For example, the option to specify mosaic region 1 is <code>--mosArea01</code>.

Mosaic and OSD regions of the same index cannot be enabled at the same time. If a mosaic region and an OSD region of the same index are enabled, the OSD region is invalid. For example:
* OSD region 1 cannot be enabled if mosaic region 1 is enabled.
* OSD region 1 can be enabled if any of mosaic regions 2 to 12 is enabled.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --mosaicEnables        | Each bit indicates whether to enable the corresponding mosaic  |
|       |                        |  region. [0]                                                   |
|       |                        | For example:                                                   |
|       |                        | - Value 1 indicates region 1 enabled.                          |
|       |                        | - Value 2 indicates region 2 enabled.                          |
|       |                        | - Value 3 indicates regions 1 and 2 enabled.                   |
|       | --mosSizeIndex         | different Mosaic size.[0]                                      |
|       |                        |   0:8x8                                                        |
|       |                        |   1:16x16                                                      |
|       |                        |   2:32x32                                                      |
|       |                        |   3:64x64                                                      |
|       |                        |   4:128x128                                                    |
|       | --mosAreaN             | Mosaic region N.                                               |
|       |  =left:top:right:bottom| - left: the leftmost pixel inside the region.                  |
|       |                        | - top: the top pixel inside the region.                        |
|       |                        | - right: the rightmost pixel inside the region.                |
|       |                        | - bottom: the bottom pixel inside the region.                  |
|       |                        | Make sure that all mosaic regions are aligned to CTB.          |

### Low-Latency Encoding

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --inputLineBufferMode  | The low-latency encoding mode. [0]                             |
|       |                        | 0 - disable low-latency encoding                               |
|       |                        | 1 - enable low-latency encoding with software handshaking and  |
|       |                        |  loopback enabled.                                             |
|       |                        | 2 - enable low-latency encoding with hardware handshaking and  |
|       |                        |  loopback enabled.(effective only when the upstream IP is used,|
|       |                        |  only VCE IP cannot be tested)                                 |
|       |                        | 3 - enable low-latency encoding with software handshaking      |
|       |                        |  enabled and loopback disabled.                                |
|       |                        | 4 - enable low-latency encoding with hardware handshaking      |
|       |                        |  enabled and loopback disabled.(effective only when the        |
|       |                        |  upstream IP is used, oingle VCE IP cannot be tested)          |
|       | --inputLineBufferDepth | 0..511 The number of CTB/MB rows to control loopback           |
|       |                        | and handshaking. [1]                                           |
|       |                        | If the loopback mode is enabled, there are two continuous      |
|       |                        |  ping-pong input line buffers. Each contains                   |
|       |                        |  inputLineBufferDepth CTB or MB rows.                          |
|       |                        | If hardware handshaking is enabled, handshaking signals are    |
|       |                        |  processed per inputLineBufferDepth CTB/MB rows.               |
|       |                        | If software handshaking is enabled, IRQ is issued and the read |
|       |                        |  count register is updated each time inputLineBufferDepth      |
|       |                        |  CTB/MB rows are read.                                         |
|       |                        | Value 0 is supported only with --inputLineBufferMode=3. In     |
|       |                        |  this case, IRQ is not sent and the read count register is not |
|       |                        |  updated.                                                      |
|       | --inputLineBufferAmountPerLoopback | 0..1023 The number of handshake synchronizations   |
|       |                        |             every loopback. [0]                                |
|       |                        | If the option is set to 0, input line buffer is disabled.      |
|       | --segmentUnitHeight    | 8, 16 The height of each FLEXA SBI segment unit. [16]          |
|       |                        | This option is valid only if the value of                      |
|       |                        |  EWLHwConfig_t.prpSbiSupport is 1.                             |
|       | --sliceIrqEnable       | Whether to enable the hardware to issue an interrupt each time |
|       |                        |  it finishes encoding a slice.  [0]                            |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --inputSliceInfoEn     | Whether to enable low-latency encoding in DDR mode.            |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       |                        | If low-latency encoding in DDR mode is enabled, the hardware   |
|       |                        |  polls a 64-byte DDR space for slice information.              |
|       | --lowlatGatingDisable  | 0..1 Lowlatency handshake auto gating disable[1]               |
|       |                        |      0: enable lowlatency handshake auto gating.               |
|       |                        |      1: disalbe lowlatency handshake auto gating.              |
|       | --lowlatGatingType     | 0..1 Lowlatency handshake auto gating type.[0]                 |
|       |                        |      0: check bus idle counter.                                |
|       |                        |      1: check emc, recon idle.                                 |
|       | --lowlatGatingCyc      | 0..255 Bus idle counter max value. It would be                 |
|       |                        |  (sw_enc_lowlat_gating_cyc + 1) * 256 cycles when counting.    |
|       |                        |   default: hevc - [31], h264 - [7]                             |


### Input Frame and Reference Frame Buffer Alignment

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --inputAlignmentExp    | The input frame buffer alignment. [6]                          |
|       |                        | 0 - alignment disabled                                         |
|       |                        | 4..12 - The base address and each line of the input frame      |
|       |                        |  buffer are aligned to two to the power of inputAlignmentExp.  |
|       | --refAlignmentExp      | The reference frame buffer alignment. [6]                      |
|       |                        | 0 - alignment disabled                                         |
|       |                        | 4..12 - The base address and each line of the reference frame  |
|       |                        |  buffer are aligned to two to the power of refAlignmentExp.    |
|       | --refChromaAlignmentExp| The alignment of the chroma reference frame buffer. [6]        |
|       |                        | 0 - alignment disabled                                         |
|       |                        | 4..12 - The base address and each line of the chroma reference |
|       |                        |  frame buffer are aligned to two to the power of               |
|       |                        |  refChromaAlignmentExp.                                        |
|       | --aqInfoAlignmentExp   | The alignment of the adaptive quantization output buffer. [0]  |
|       |                        | 0 - alignment disabled                                         |
|       |                        | 4..12 - The base address and each line of the adaptive         |
|       |                        |  quantization output buffer are aligned to two to the power of |
|       |                        |  aqInfoAlignmentExp.                                           |
|       | --tileStreamAlignmentExp| 0..15 The tile stream buffer alignment. [0]                   |
|       |                        | The base address and size of the tile stream buffer are        |
|       |                        |  aligned to two to the power of --tileStreamAlignmentExp.      |

### Output Stream Buffer

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --streamBufChain       | Whether to enable one or two output stream buffers. [0]        |
|       |                        | 0 - one output stream buffer                                   |
|       |                        | 1 - two chained output stream buffers                          |
|       |                        | NOTE: The minimum allowed size of the first stream buffer is   |
|       |                        |  11 KB.                                                        |

### (Reserved) Multi-Segment Stream Output

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --streamMultiSegmentMode | (Reserved) The multi-segment mode of output stream buffers. [0]|
|       |                        | 0 - single-segment mode                                        |
|       |                        | 1 - multi-segment mode with software handshaking disabled      |
|       |                        |  and loopback enabled                                          |
|       |                        | 2 - multi-segment mode with software handshaking and loopback  |
|       |                        |  enabled.                                                      |
|       | --streamMultiSegmentAmount| (Reserved) 2..16 The number of segments to control          |
|       |                        |  loopback, software handshaking, and IRQ. [4]                  |

### Parallel Flow Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --parallelCoreNum      | 1..4 The number of cores that run in parallel at the frame     |
|       |                        |  level. [1]                                                    |
|       |                        | If the number of tile columns for picture division is set to 1 |
|       |                        |  through --tile, set the --parallelCoreNum option to 1.        |
|       | --batchEnable          | 0..1 enable or disable multi-frame aggregation. [0]            |
|       |                        |  0 - disable batch mode.                                       |
|       |                        |  1 - enable batch mode.                                        |
|       | --batchCount           | 1..[parallelCoreNum-1] frame numbers for aggregation encoding. |
|       |                        | It should be smaller than parallelCoreNum. It can be modified  |
|       |                        |  during the encoding process by setting after @frame_number    |

### AXI Settings

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --AXIAlignment         | The AXI alignment in hexadecimal format.                       |
|       |                        |  [data from fuse for each bit]                                 |
|       |                        | - Bits 35:32: AXI_burst_align_wr_cuinfo                        |
|       |                        | - Bits 31:28: AXI_burst_align_wr_common                        |
|       |                        | - Bits 27:24: AXI_burst_align_wr_stream                        |
|       |                        | - Bits 23:20: AXI_burst_align_wr_chroma_ref                    |
|       |                        | - Bits 19:16: AXI_burst_align_wr_luma_ref                      |
|       |                        | - Bits 15:12: AXI_burst_align_rd_common                        |
|       |                        | - Bits 11:8: AXI_burst_align_rd_prp                            |
|       |                        | - Bits 7:4: AXI_burst_align_rd_ch_ref_prefetch                 |
|       |                        | - Bits 3:0: AXI_burst_align_rd_lu_ref_prefetch                 |
|       | --burstMaxLength       | The maximum AXI burst length, in the unit of AXI bus width.    |
|       |                        |  [16]                                                          |


### IRQ Control

#### IRQ Type Mask of Encoder

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --irqTypeMask          | The IRQ type mask setting in binary format. [111110000]        |
|       |                        | - Bit 8: irq_type_sw_reset_mask                                |
|       |                        | - Bit 7: irq_type_fuse_error_mask                              |
|       |                        | - Bit 6: irq_type_buffer_full_mask                             |
|       |                        | - Bit 5: irq_type_bus_error_mask                               |
|       |                        | - Bit 4: irq_type_timeout_mask                                 |
|       |                        | - Bit 3: irq_type_strm_segment_mask                            |
|       |                        | - Bit 2: irq_type_line_buffer_mask                             |
|       |                        | - Bit 1: irq_type_slice_rdy_mask                               |
|       |                        | - Bit 0: irq_type_frame_rdy_mask                               |


#### IRQ Type Mask of CuTree

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --irqTypeCutreeMask    | The IRQ type mask setting of CuTree, in binary format.         |
|       |                        |  [111110000]                                                   |
|       |                        | - irq_type_sw_reset_mask                                       |
|       |                        | - irq_type_fuse_error_mask                                     |
|       |                        | - irq_type_buffer_full_mask                                    |
|       |                        | - irq_type_bus_error_mask                                      |
|       |                        | - irq_type_timeout_mask                                        |
|       |                        | - irq_type_strm_segment_mask                                   |
|       |                        | - irq_type_line_buffer_mask                                    |
|       |                        | - irq_type_slice_rdy_mask                                      |
|       |                        | - irq_type_frame_rdy_mask                                      |
|       |                        | The option supports 9 bits for consistency with --irqTypeMask. |
|       |                        |  However, only irq_type_bus_error_mask, irq_type_timeout_mask  |
|       |                        |  and irq_type_frame_rdy_mask are valid.                        |


### (Only for C-Model) Peripheral Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --useVcmd              | (Only valid fo CModel) Whether to enable VCMD.                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --useDec400            | (Reserved)Whether to enable DEC400.                            |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --useL2Cache           | (Reserved)Whether to enable L2Cache.                           |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |


### External SRAM

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --extSramLumHeightBwd  | The capacity of external SRAM for luma backward reference.     |
|       |                        | 0 - no external SRAM                                           |
|       |                        | 1..16 - the number of lines = 4 * extSramLumHeightBwd          |
|       |                        | The default value is 16 for HEVC (H.265) and 12 for H.264      |
|       |                        |  (AVC).                                                        |
|       | --extSramChrHeightBwd  | The capacity of external SRAM for chroma backward reference.   |
|       |                        | 0 - no external SRAM                                           |
|       |                        | 1..16 - the number of lines = 4 * extSramChrHeightBwd          |
|       |                        | The default value is 8 for HEVC (H.265) and 6 for H.264 (AVC)  |
|       | --extSramLumHeightFwd  | The capacity of external SRAM for luma forward reference.      |
|       |                        | 0 - no external SRAM                                           |
|       |                        | 1..16 - the number of lines = 4 * extSramLumHeightFwd          |
|       |                        | The default value is 16 for HEVC (H.265) and 12 for H.264      |
|       |                        |  (AVC).                                                        |
|       | --extSramChrHeightFwd  | The capacity of external SRAM for chroma forward reference.    |
|       |                        | 0 - no external SRAM                                           |
|       |                        | 1..16 - the number of lines = 4 * extSramChrHeightFwd          |
|       |                        | The default value is 8 for HEVC (H.265) and 6 for H.264 (AVC). |


## Debugging and Testing

### Debugging

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --dumpRegister         | Whether to dump register values. [0]                           |
|       |                        | 0 - do not dump                                                |
|       |                        | 1 - dump                                                       |
|       | --rasterscan           | Whether to dump reconstructed YUV frames in tiled or raster    |
|       |                        |  format when the encoder runs on FPGA and hardware. [0]        |
|       |                        | 0 - tiled format                                               |
|       |                        | 1 - raster format                                              |


### Testing

<b>NOTE:</b> Options listed in this section are designed for testing multiple encoder instances in
multi-thread or multi-process mode.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --multimode            | The parallel running mode. [0]                                 |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - multi-thread mode                                          |
|       |                        | 2 - multi-process mode                                         |
|       | --streamcfg            | The path to the file that contains encoder options.            |

### vcmd priority and core bit mask

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --priority             | The priority of current instance in vcmd mode. [0]             |
|       |                        | 0 - normal priority                                            |
|       |                        | 1 - high priority                                              |
|       | --core_mask            | The core bit mask of current instance. [0]                     |
|       |                        | 0 - not specify, anycore.                                      |
|       |                        | 1 - bit[0]=1: specify core 0.                                  |
|       |                        | 2 - bit[1]=1: specify core 1.                                  |

### Log Output Control

| Short | Long Option            | Description                                                    |
| ----- | ---------------------- | -------------------------------------------------------------- |
|       | --logOutDir            | The log output mode. [0]                                       |
|       |                        | 0 - outputs all logs to stdout                                 |
|       |                        | 1 - outputs all logs to the same file                          |
|       |                        | 2 - outputs logs of each thread to a separate log file         |
|       | --logOutLevel          | The log level. [3]                                             |
|       |                        | 0 - no logs output                                             |
|       |                        | 1 - the fatal level                                            |
|       |                        | 2 - the error level                                            |
|       |                        | 3 - the warning level                                          |
|       |                        | 4 - the infomation level                                       |
|       |                        | 5 - the debugging level:                                       |
|       |                        | 6 - all logs output                                            |
|       | --logTraceMap          | Whether to output each type of trace logs. [63]=b'0111111      |
|       |                        | Bit 0 - encoder API call logs                                  |
|       |                        | Bit 1 - register configuration logs                            |
|       |                        | Bit 2 - EWL API call logs                                      |
|       |                        | Bit 3 - memory usage logs                                      |
|       |                        | Bit 4 - rate control status logs                               |
|       |                        | Bit 5 - command line logs                                      |
|       |                        | Bit 6 - performance logs                                       |
|       |                        | Value 0 indicates not to output the type of logs.              |
|       |                        | Value 1 indicates to output the type of logs.                  |
|       | --logCheckMap          | (Reserved) Whether to output each type of check logs.          |
|       |                        |  [1]=b`00001                                                   |
|       |                        | Bit 0 - reconstructed YUV data                                 |
|       |                        | Bit 1 - PSNR/SSIM for each frame                               |
|       |                        | Bit 2 - VBV information for rate control checking              |
|       |                        | Bit 3 - rate control information for rate control profiling    |
|       |                        | Bit 4 - feature information for coverage checking              |
|       |                        | Value 0 indicates not to output the type of logs.              |
|       |                        | Value 1 indicates to output the type of logs.                  |


### (Obsoleted) Runtime Log Output Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --verbose              | (Obsoleted) The log printing mode. [0]                         |
|       |                        | 0 - prints brief information                                   |
|       |                        | 1 - prints detailed information                                |


## Internal Use

<b>NOTE:</b> Options listed in this section are for internal use only. The features may be under
development. Specifying these options is not recommended.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --testId               | The internal test ID. [0]                                      |
|       | --rdLog                | Whether to ouput rate distortion logs to the profile.log file. |
|       |                        | 0 - do not output                                              |
|       |                        | 1 - output                                                     |
|       | --TxTypeSearchEnable   | (For AV1 only) Whether to enable TX type search. [0]           |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --av1InterFiltSwitch   | (For AV1 only) Whether to enable interpolation filter switch.  |
|       |                        |  [1]                                                           |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - enable                                                     |
|       | --replaceMvFile        | The path to the file that contains ME1N MVs.                   |
|       |                        | Data in the file is organized in raster scan of 32x32 pixel    |
|       |                        |  blocks.                                                       |
|       |                        | For each block, the file contins 21 MVs at the following       |
|       |                        |  positions: 32x32(0,0), 16x16(0,0), 16x16(16,0), 16x16(0,16),  |
|       |                        |  16x16(16,16), 8x8(0,0), 8x8(8,0), 8x8(16,0), 8x8(24,0),       |
|       |                        |  8x8(0,8), 8x8(8,8), 8x8(16,8), 8x8(24,8), 8x8(0,16),          |
|       |                        |  8x8(8,16), 8x8(16,16), 8x8(24,16), 8x8(0,24), 8x8(8,24),      |
|       |                        |  8x8(16,24), and 8x8(24,24).                                   |
|       |                        | For each MV, (Hor + Ver) * sizeof(i16) * (L0 + L1) = 8 bytes   |
|       | --SRDEnable            | Detect static region and reduce number of bits spent in these  |
|       |                        | regions. [0]                                                   |
|       |                        |  0 = Disable SRD.                                              |
|       |                        |  1 = Enable SRD.                                               |
|       | --SRDThreshold         | 0..10 Threshold for static region detection [4]                |
|       |                        | Bigger threshold value cause more regions detected as static.  |
|       | --disableTU32          | Whether to disable TU32 dased on tu32Enable fuse value.[0]     |
|       |                        | HW to do, only for test.                                       |
|       |                        |  1 = Disable TU32.                                             |
