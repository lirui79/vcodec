# Options

This chapter describes various options regarding different features. You can run options in the
following way:

./jpeg_testenc [options] -i inputfile

The default value of an option is marked by square brackets in the option description.

## Help Information

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -H    | --help                 | Display help information.                                      |

## Pre-processing Frames

### Input Frame Resolutions and Cropping

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -i[s] | --input                | Reads input from file. [input.yuv]                             |
| -a[n] | --firstPic             | First picture of input file. [0]                               |
| -b[n] | --lastPic              | Last picture of input file. [0]                                |
| -w[n] | --lumWidthSrc          | Source image width. [176]                                      |
| -h[n] | --lumHeightSrc         | Source image height. [144]                                     |
| -x[n] | --width                | Output image width. [lumWidthSrc]                              |
| -y[n] | --height               | Output image height. [lumHeightSrc]                            |
| -X[n] | --horOffsetSrc         | Horizontal offset of output image. [0]                         |
| -Y[n] | --verOffsetSrc         | Vertical offset of output image. [0]                           |
| -W[n] | --write                | Whether to write output. [1]  <br>0 - NO <br>1 - YES           |

### Input Picture Format and Controls

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -g[n] | --frameType            | Input color format. [0]                                        |
|       |                        | 0 - YUV420P8b_Raster_A16N (YUV 4:2:0 planar(IYUV/I420))        |
|       |                        | 1 - YUV420SP8b_Raster_A16N (YUV 4:2:0 semi-planar(NV12))       |
|       |                        | 2 - YVU420SP8b_Raster_A16N (YUV 4:2:0 semi-planar(NV21))       |
|       |                        | 3 - YUV422YUYV8b_Raster_A16N (YUYV422 interleaved (YUYV/YUY2)  |
|       |                        | 4 - YUV422UYVY8b_Raster_A16N (UYVY422 interleaved (UYVY/Y422)  |
|       |                        | 5 - RGB565_Raster_A16N (RGB565)                                |
|       |                        | 6 - BGR565_Raster_A16N (BGR565)                                |
|       |                        | 7 - XRGB1555_Raster_A16N (RGB555)                              |
|       |                        | 8 - XBGR1555_Raster_A16N (BGR555)                              |
|       |                        | 9 - XRGB4444_Raster_A16N (RGB444)                              |
|       |                        | 10 - XBGR4444_Raster_A16N (BGR444)                             |
|       |                        | 11 - XRGB8888_Raster_A16N (RGB888)                             |
|       |                        | 12 - XBRG8888_Raster_A16N (BGR888)                             |
|       |                        | 13 - XRGB2101010_Raster_A16N (RGB101010)                       |
|       |                        | 14 - XBGR2101010_Raster_A16N (BGR101010)                       |
|       |                        | 15 - YUV420P10bWL_Raster_A16N (I010)                           |
|       |                        | 16 - YUV420SP10bWH_Raster_A16N (P010)                          |
|       |                        | 17 - YUV420P10b_Raster_A16N                                    |
|       |                        | 18 - YUV420Y0L210b_Raster_A16N                                 |
|       |                        | 19 - YUV420 customer private tile for HEVC                     |
|       |                        | 20 - YUV420 customer private tile for H.264                    |
|       |                        | 21 - YUV420SP8b_YuvSp4x4_A16N (YUV 4:2:0 semi-planar tile)     |
|       |                        | 22 - YVU420SP8b_YuvSp4x4_A16N (YUV 4:2:0 semi-planar tile)     |
| -g[n] | --frameType            | 23 - YUV420SP10bWH_YuvSp4x4_A32N (P010 tile)                   |
|       |                        | 24 - YUV420SP10bDWL_Raster_A16N (YUV 4:2:0 semi-planar 10)     |
|       |                        | 25 - YUV422SP8b_Raster_A16N (YUV 4:2:2 semi-planar)            |
|       |                        | 32 - YUV420YUV8b_Yuv128x2_A16N (YUV420 UV 8bit tile128x2)      |
|       |                        | 35 - YUV420SP8b_YuvSp8x8_A16N                                  |
|       |                        |         (YUV420 semi-planar UV 8bit tile8x8)                   |
|       |                        | 36 - YUV420SP10bWH_YuvSp8x8_A16N                               |
|       |                        |         (YUV420 semiplanar UV 10bit tile8x8)                   |
|       |                        | 37 - YUV420PYVU8b_Raster_A16N (YVU420 UV 8bit planar)          |
|       |                        | 38 - YVU420SP8b_YuvSp64x2_A16N                                 |
|       |                        |         (YUV420 semi-planar UV 8bit tile64x2)                  |
|       |                        | 39 - YVU420SP10b_YuvSp128x2_A16N                               |
|       |                        |         (YUV420 semi-planar UV 10bit tile128x2)                |
|       |                        | 40 - RGB888_Raster_A16N (RGB888)                               |
|       |                        | 41 - BGR888_Raster_A16N (BGR888)                               |
|       |                        | 42 - RBG888_Raster_A16N (RBG888)                               |
|       |                        | 43 - GBR888_Raster_A16N (GBR888)                               |
|       |                        | 44 - BRG888_Raster_A16N (BRG888)                               |
|       |                        | 45 - GRB888_Raster_A16N (GRB888)                               |
|       |                        | 46 - YVU422SP8b_Raster_A16N (YUV 4:2:2 semi-planar)            |
|       |                        | 50 - RGBX8888_Raster_A16N                                      |
|       |                        | 51 - BGRX8888_Raster_A16N                                      |
|       |                        | 52 - RGBX1010102_Raster_A16N                                   |
|       |                        | 53 - BGRX1010102_Raster_A16N                                   |
|       |                        | 54 - YVU420SP10bWH_Raster_A16N (P010 CrCb)                     |
|       |                        | 55 - Y8b_Raster_A16N (YCbCr 4:0:0 ) Monochrome 8-bit           |
|       |                        | 56 - Y10bWL_Raster_A16N (YCbCr 4:0:0 ) Monochrome 10-bit       |
|       |                        |  occupies 2B [9:0]                                             |
|       |                        | 57 - Y10bWH_Raster_A16N (YCbCr 4:0:0 ) Monochrome 10-bit       |
|       |                        |  occupies 2B [16:5]                                            |
|       |                        | 58 - YUV420SP10b_Raster_A16N                                   |
|       |                        | 62 - YUV422YVYU8b_Raster_A16N (YVYU422)                        |
|       |                        | 63 - YUV422VYUY8b_Raster_A16N (VYUY422)                        |
|       |                        | 64 - YVU420SP8b_YuvSp8x8_A64N                                  |
|       |                        | 65 - Y10b_Raster_A16N                                          |
|       |                        | For more details, see <i>Frame Buffer Format                   |
|       |                        |  Specifications</i>.                                           |
| -v[n] | --colorConversion      | RGB-to-YUV color conversion type. [0]                          |
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
| -G[n] | --rotation             | Input image rotation mode. [0]                                 |
|       |                        | 0 - Disabled                                                   |
|       |                        | 1 - 90 degrees right                                           |
|       |                        | 2 - 90 degrees left                                            |
|       |                        | 3 - 180 degrees                                                |
| -M[n] | --mirror               | Whether to enable horizontal mirroring for input image. [0]    |
|       |                        | 0 - Disable                                                    |
|       |                        | 1 - Enable                                                     |
| -Q[n] | --inputAlignmentExp    | Alignment value of input frame buffer. [4]                     |
|       |                        | 0 - Disable alignment                                          |
|       |                        | 4 to 12 - Base address of input frame buffer and each line     |
|       |                        |  are aligned to 2^inputAlignmentExp                            |
| -d[n] | --enableConstChroma    | Whether to set chroma to a constant pixel value. [0]           |
|       |                        | Value range: 0 and 1                                           |
|       |                        | 0 - Disable                                                    |
|       |                        | 1 - Enable                                                     |
| -e[n] | --constCb              | Constant pixel value for the U component. [128]    <br> Value  |
|       |                        |  range: 0 to 255                                               |
| -f[n] | --constCr              | Constant pixel value for the V component. [128]    <br> Value  |
|       |                        |  range: 0 to 255                                               |
|       | --scanType             | The scan type of input image. [0]                              |
|       |                        |   0 - raster                                                   |
|       |                        |   1 - supertileX                                               |

### OSD Overlay Control

A maximum of 12 OSD regions are supported. Two OSD regions cannot share the same MCU.

| Short | Long Option <br>(N = 01 to 12)   | Description                                           |
|-------|------------------------|----------------------------------------------------------------|
|       | --overlayEnables       | Overlay region status, with 12 bits representing 12 regions    |
|       |                        |  respectively. [0]                                             |
|       |                        | 1: Region 1 enabled                                            |
|       |                        | 2: Region 2 enabled                                            |
|       |                        | 3: Region 1 and 2 enabled                                      |
|       |                        | and so on.                                                     |
|       | --olInputN             | Input file for overlay region. [olInputi.yuv]                  |
|       | --olFormatN            | Overlay input format. [0]                                      |
|       |                        | Value range: 0 to 2                                            |
|       |                        | 0: ARGB8888                                                    |
|       |                        | 1: NV12                                                        |
|       |                        | 2: Bitmap                                                      |
|       | --olAlphaN             | Global alpha value for NV12 and bitmap overlay format. [0]     |
|       |                        |  <br> Value range: 0 to 255                                    |
|       | --olWidthN             | Overlay region width. It can only be set when a region is      |
|       |                        |  enabled. [0]                                                  |
|       |                        | It must be under eight-pixel aligned for bitmap format.        |
|       | --olHeightN            | Overlay region height. It can only be set when a region is     |
|       |                        |  enabled. [0]                                                  |
|       | --olXoffsetN           | Horizontal offset of overlay region top-left pixel. [0]        |
|       |                        | It must be two-pixel aligned. [0]                              |
|       | --olYoffsetN           | Vertical offset of overlay region top left pixel. [0]          |
|       |                        | It must be two-pixel aligned. [0]                              |
|       | --olYStrideN           | Luma stride in bytes. The default value depends on format.     |
|       |                        |  <br>Value range:                                              |
|       |                        | [olWidthi * 4] when the format is ARGB8888.                    |
|       |                        | [olWidthi] when the format is NV12.                            |
|       |                        | [olWidthi / 8] when the format is bitmap.                      |
|       | --olUVStrideN          | Chroma stride in bytes. The default value depends on the luma  |
|       |                        |  stride.                                                       |
|       | --olCropXoffsetN       | Top left horizontal offset for OSD cropping. [0]               |
|       |                        | For non-bitmap formats, it must be under two-pixel aligned.    |
|       |                        | For bitmap format, it must be under eight-pixel aligned.       |
|       | --olCropYoffsetN       | Top left vertical offset for OSD cropping. [0]                 |
|       |                        | For non-bitmap formats, it must be under two-pixel aligned.    |
|       |                        | For bitmap format, it must be under eight-pixel aligned.       |
|       | --olCropWidthN         | OSD cropping width. [olWidthi]                                 |
|       |                        | For bitmap format, it must be under eight-pixel aligned.       |
|       | --olCropHeightN        | OSD cropping height. [olHeighti]                               |
|       | --olBitmapYN           | Y value of the OSD bitmap format. [0]                          |
|       | --olBitmapUN           | U value of the OSD bitmap format. [0]                          |
|       | --olBitmapVN           | V value of the OSD bitmap format. [0]                          |
|       | --olSuperTileN         | Whether the OSD input data is organized in supertile mode. [0] |
|       |                        | 0 - non-supertile mode.                                        |
|       |                        | 1 - X-major supertile mode.                                    |
|       |                        | 2 - Y-major supertile mode.                                    |
|       | --olScaleWidthN        | The width of each OSD input. [0]                               |
|       |                        | If a region is enabled, this option must be specified for the  |
|       |                        |  region.                                                       |
|       |                        | The option value must be 8 aligned for the bitmap format.      |
|       | --olScaleHeightN       | The height of each OSD input. [0]                              |
|       |                        | If a region is enabled, this option must be specified for the  |
|       |                        |  region.                                                       |

### Mosaic Area Control

A maximum 12 mosaic regions are supported. Any region should be aligned to CTB.

Mosaic and OSD region cannot be enabled at the same time. Mosaic region has a higher priority.

For example:

* When mosaic region 01 is enabled, OSD region 1 cannot be enabled.

* When mosaic region 02 to 12 is enabled, OSD region 1 can be enabled.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --mosaicEnables        | Enabling status of mosaic regions, with one bit for one        |
|       |                        |  region. (Not support Monochrome encoding if enable) [0]       |
|       |                        | 1: Region 1 enabled                                            |
|       |                        | 2: Region 2 enabled                                            |
|       |                        | 3: Region 1 and 2 enabled                                      |
|       |                        | and so on.                                                     |
|       | --mosSizeIndex         | different Mosaic size.[0]                                      |
|       |                        |   0:8x8                                                        |
|       |                        |   1:16x16                                                      |
|       |                        |   2:32x32                                                      |
|       |                        |   3:64x64                                                      |
|       |                        |   4:128x128                                                    |
|       | --mosAreaN             | Mosaic region parameters, including                            |
|       |  (N = 01, 02, ..., 12) |                                                                |
|       |                        |  coordinates of the left, top, right, and bottom pixels.       |
|       |                        | All coordinates must be CTB/MCU aligned.                       |

### OSDMap Controls

OSDMap has the lowest priority Comparing with overlay and mosaic. OSDMap and overlay supertile
should not be turned on at the same time.

Maximum 12 color attribute (Y[i],U[i],V[i],Alpha[i]) are supported, where i=01,02,..,12. 

Each 4bits in OSDMap defines a color attribut applied on one 8x8 block. So 1 byte OSDMap data
defines a region of 16x8. osdMapStride should not less than (codingWidth + 15) >> 4.

Following options is example option for color "01". The other color should use corresponding index.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --osdMapEnable         | The OSD map enable. [0]                                        |
|       |                        |   0:disable                                                    |
|       |                        |   1:enable                                                     |
|       | --osdMapInput          | input file for OSD Map. [NULL]                                 |
|       | --osdMapStride         | OSD Map stride in byte. [width]                                |
|       | --osdMapBlockSize      | OSD Map different block size in pixel. [0]                     |
|       |                        |   8:8x8                                                        |
|       |                        |   4:4x4                                                        |
|       | --osdMapAlphaN         | 0..255 Specify the alpha value for color[i]. [0]               |
|       | --osdMapYN             | OSD Map format color[1] Y value. [0]                           |
|       | --osdMapUN             | OSD Map format color[1] U value. [0]                           |
|       | --osdMapVN             | OSD Map format color[1] V value. [0]                           |

### DEC400 Tile Status

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --dec400TableInput     | DEC400 compressed table input from file.                       |
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
|       | --ufbcMode             | The core mode of UFBC.                                         |
|       |                        | 0 - disable                                                    |
|       |                        | 1 - afbc (only support 32x8 pixels)                            |
|       |                        | 2 - afbc (support 32x8 and 16x16 pixels)                       |
|       |                        | 3 - dec400                                                     |
|       |                        | 4 - pvric                                                      |
|       | --ufbcYuvTrans         | Whether to enable interal YUV transformation.                  |
|       |                        | 1 - enable                                                     |
|       |                        | 0 - disable                                                    |
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

## Coding Tools and Syntax Control

### Coding Modes and Tools

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -R[n] | --restartInterval      | Restart interval in MCU rows. [0]                              |
|       | --quality              | Set quality factor instead of qLevel. [-1]                     |
|       |                        | -1 - use quantization table defined by qLevel or fixedQP.      |
|       |                        | 1..100 - use quantization table generated accordingly. Value   |
|       |                        |  100 stands for the best quality.                              |
| -q[n] | --qLevel               | Quantization scale. [1]                                        |
|       |                        | Value range: 0 to 10                                           |
|       |                        | The value 10 indicates the testbench-defined quantization table|
|       |                        |  is used.                                                      |
|       | --qTableFile           | External quantization file to be read when quantization level  |
|       |                        |  is 10. The file has 16 lines separated by space to specify two|
|       |                        |  quantization tables, with the first 8 lines indicating luma   |
|       |                        |  table, and the rest 8 lines indicating chroma table. Each line|
|       |                        |  has 8 values ranging from 1 to 255.                           |
| -p[n] | --codingType           | Encoding type. [0]                                             |
|       |                        | 0 - Whole frame encoding                                       |
|       |                        | 1 - Partial frame encoding                                     |
| -m[n] | --codingMode           | Encoding mode. [0]                                             |
|       |                        | 0 - YUV420                                                     |
|       |                        | 1 - YUV422 (only for input YUV422SP and YVU422SP)              |
|       |                        | 2 - Monochrome                                                 |
| -t[n] | --markerType           | Quantization/Huffman table markers. [0]                        |
|       |                        | 0 - Single marker                                              |
|       |                        | 1 - Multiple markers                                           |

### APP0 information

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -u[n] | --units                | Unit of X- and Y-density. [0]                                  |
|       |                        | 0 - Pixel aspect ratio                                         |
|       |                        | 1 - Dots per inch                                              |
|       |                        | 2 - Dots per centimetre                                        |
| -k[n] | --xdensity             | X-density to APP0 header. [1]                                  |
| -l[n] | --ydensity             | Y-density to APP0 header. [1]                                  |
| -I[s] | --inputThumb           | Reads thumbnail input from file. [thumbnail.jpg]               |
| -T[n] | --thumbnail            | Thumbnail to stream. [0]                                       |
|       |                        | 0 - NO                                                         |
|       |                        | 1 - JPEG                                                       |
|       |                        | 2 - RGB8                                                       |
|       |                        | 3 - RGB24                                                      |
| -K[n] | --widthThumb           | Thumbnail output image width. [32]                             |
| -L[n] | --heightThumb          | Thumbnail output image height. [32]                            |
| -c[n] | --comLength            | Comment header data length. [0]                                |
| -C[s] | --comFile              | Comment header data file. [com.txt]                            |

### Lossless Encoding

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --lossless             | Lossless encoding mode. [0]   <br> Value range: 0 to 7         |
|       |                        |   0 - Disable                                                  |
|       |                        |   1 to 7 - Enable, with selected prediction modes 1 to 7       |
|       | --ptrans               | Point transform value for lossless encoding. [0] <br> Value    |
|       |                        |  range: 0 to 7                                                 |

### ROI Map

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --roimapFile           | Input file for ROI map region. [NULL]                          |
|       |                        | NULL - Disable ROI Map.                                        |
|       |                        | "roimap.roi" - text file name to describe ROI regions          |
|       | --nonRoiFilter         | Input file for non-ROI map region filter. [NULL]               |
|       |                        | NULL - NO exteranl filter, use pre-defined filter level        |
|       |                        | "filter.txt" - specify external filter                         |
|       | --nonRoiLevel          | Non-ROI filter level [5]                                       |
|       |                        | Value range: 0 to 9. from strong to weak.                      |

### Motion JPEG and Rate Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -J[n] | --mjpeg                | Whether to enable motion JPEG [0]                              |
|       |                        | 0 - Disable                                                    |
|       |                        | 1 - Enable                                                     |
| -B[n] | --bitPerSecond         | Target bits per second. [0]                                    |
|       |                        | 0 - Disable RC                                                 |
|       |                        | None-zero value - RC enabled with the specified target bits    |
|       |                        |  per second.                                                   |
| -n[n] | --frameRateNum         | Output frame rate numerator. [30]                              |
|       |                        | Value range: 1 to 1048575                                      |
| -r[n] | --frameRateDenom       | Output frame rate denominator. [1]                             |
|       |                        | Value range: 1 to 1048575                                      |
| -V[n] | --rcMode               | JPEG/MJPEG RC mode. [1]                                        |
|       |                        | Value range: 0 to 2                                            |
|       |                        | 0 - Single frame RC mode.                                      |
|       |                        | 1 - Video RC with CBR.                                         |
|       |                        | 2 - Video RC with VBR.                                         |
| -U[s] | --picQpDeltaRange      | QP delta range in picture-level rate control.                  |
|       |      =[Min:Max]        | Min: Minimum Qp_Delta in picture RC. [-2]  <br> Value range:   |
|       |                        |  -10 to -1                                                     |
|       |                        | Max: Maximum Qp_Delta in picture RC. [3]  <br> Value range:    |
|       |                        |  1 to 10                                                       |
|       |                        | The value ranges only apply to two neighboring frames.         |
| -E[n] | --qpMin                | Minimum frame QP. [0] <br> Value range: 0 to 51                |
| -F[n] | --qpMax                | Maximum frame QP. [51] <br> Value range: 0 to 51               |
| -O[n] | --fixedQP              | Fixed QP for every frame. [-1]                                 |
|       |                        | Value range: -1 to 51                                          |
|       |                        | -1   - Disable fixed QP mode.                                  |
|       |                        | 0-51 - Fixed QP value.                                         |

### SRAM Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --sramPowerdownDisable | Whether to disenableSRAM power down [0]                        |
|       |                        | 0 - Enable sram power down                                     |
|       |                        | 1 - Disable sram power down                                    |
|       | --sramPowerdownMode    | SRAM hw model [0]                                              |
|       |                        | 0 - Mode 00                                                    |
|       |                        | 1 - Mode 01                                                    |
|       |                        | 2 - Mode 10                                                    |
|       |                        | 3 - Mode 11                                                    |

## Stream Output Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -o[s] | --output               | Writes output to file. [stream.jpg]                            |
|       | --streamBufChain       | Stream buffer output modes. [0]                                |
|       |                        | 0 - Single output stream buffer.                               |
|       |                        | 1 - Two output stream buffers chained together.                |
|       |                        | <b>Note</b>: Minimum size of the first stream buffer is 1KB    |
|       |                        |  plus the thumbnail data size.                                 |
|       | --hashtype             | The method to calculate the hash value of the output stream.   |
|       |                        |  [0]                                                           |
|       |                        | 0 - hash value calculation disabled                            |
|       |                        | 1 - standard CRC32                                             |
|       |                        | 2 - 32-bit checksum                                            |

### Stream Multi-Segment for Output Low Latency

| Short | Long Option                | Description                                                |
|-------|----------------------------|------------------------------------------------------------|
|       | --streamMultiSegmentMode   | Stream multi-segment mode. [0]  <br> Value range: 0 to 3   |
|       |                            | 0 - Disable                                                |
|       |                            | 1 (Reserved) - Enable (hardware handshaking, loop-back     |
|       |                            |  enabled)                                                  |
|       |                            | 2 (Reserved) - Enable (software handshaking, loop-back     |
|       |                            |  enabled)                                                  |
|       |                            | 3 - Enable (IRQ only, no loop-back)                        |
|       | --streamMultiSegmentSize   | Segment size in byte. [1024] <br> Value range: 512 byte    |
|       |                            |  to 16KB                                                   |
|       |                            | It should be a multiple of 16 bytes.                       |
|       | --streamMultiSegmentAmount | Total amount of segments. [4] <br> Value range: 2 to 1024  |
|       |                            | 0 - Full image size = Width x Height x 2                   |

## Input Low Latency Mode

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -S[n] | --inputLineBufferMode  | Input line buffer mode. [0]                                    |
|       |                        | Value range: 0 to 4                                            |
|       |                        | 0 = Disable                                                    |
|       |                        | 1 = Enable (software handshaking, loopback enabled)            |
|       |                        | 2 = Enable (hardware handshaking, loopback enabled)            |
|       |                        |     (effective only when the upstream IP is used,              |
|       |                        |      only VCE IP cannot be tested)                             |
|       |                        | 3 = Enable (software handshaking, loopback disabled)           |
|       |                        | 4 = Enable (hardware handshaking, loopback disabled)           |
|       |                        |     (effective only when the upstream IP is used,              |
|       |                        |      only VCE IP cannot be tested)                             |
| -N[n] | --inputLineBufferDepth | Number of MCU rows to control loop-back or handshaking [1]     |
|       |                        | Value range: 0 to 511                                          |
|       |                        |  - When loop-back is enabled, each of the two continuous       |
|       |                        |  ping-pong input buffers contains MCU rows specified by this   |
|       |                        |  option.                                                       |
|       |                        |  - When hardware handshaking is enabled, handshaking signal    |
|       |                        |  is processed per CTB/MCU rows specified by this option.       |
|       |                        |  - When software handshaking is enabled, IRQ is sent and       |
|       |                        |  Read Count Register is updated each time a number of MCU      |
|       |                        |  rows specified by this option have been read.                 |
|       |                        | <b>Note</b>: This option can be set to 0 only when             |
|       |                        |  inputLineBufferMode is set to 3. In this case, IRQ is not     |
|       |                        |  sent and Read Count Register is not updated.                  |
| -s[n] | --inputLineBufferAmountPerLoopback | Line buffer amount in the case of buffer           |
|       |                        |             read/write address loopback. [0] <br> Value range: |
|       |                        |             0 to 1023                                          |
|       | --segmentUnitHeight    | Segment unit height when low latency is in SBI mode. [16]      |
|       |                        | Value range: 8 and 16                                          |
|       | --inputSliceInfoEn     | Control DDR low-latency mode. [0]                              |
|       |                        | 0 - Disable                                                    |
|       |                        | 1 - Enable                                                     |
|       |                        | <b>Note</b>: HW will poll slice info structure saved in DDR,   |
|       |                        |  which in a 64 byte ddr space                                  |


## Hardware Control

### AXI Interface

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --AXIAlignment         | AXI alignment setting (in hexadecimal format). [0]             |
|       |                        | bit[31:28] AXI_burst_align_wr_common                           |
|       |                        | bit[27:24] AXI_burst_align_wr_stream                           |
|       |                        | bit[23:20] AXI_burst_align_wr_chroma_ref                       |
|       |                        | bit[19:16] AXI_burst_align_wr_luma_ref                         |
|       |                        | bit[15:12] AXI_burst_align_rd_common                           |
|       |                        | bit[11:8] AXI_burst_align_rd_prp                               |
|       |                        | bit[7:4] AXI_burst_align_rd_ch_ref_prefetch                    |
|       |                        | bit[3:0] AXI_burst_align_rd_lu_ref_prefetch                    |
|       | --burstMaxLength       | Maximum AXI burst length. [16]                                 |

### IRQ Type

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --irqTypeMask          | IRQ type mask setting (in binary format). [111110000]          |
|       |                        | swreg1 bit24 irq_type_sw_reset_mask, default 1                 |
|       |                        | swreg1 bit23 irq_type_fuse_error_mask, default 1               |
|       |                        | swreg1 bit22 irq_type_buffer_full_mask, default 1              |
|       |                        | swreg1 bit21 irq_type_bus_error_mask, default 1                |
|       |                        | swreg1 bit20 irq_type_timeout_mask, default 1                  |
|       |                        | swreg1 bit19 irq_type_strm_segment_mask, default 0             |
|       |                        | swreg1 bit18 irq_type_line_buffer_mask, default 0              |
|       |                        | swreg1 bit17 irq_type_slice_rdy_mask, default 0                |
|       |                        | swreg1 bit16 irq_type_frame_rdy_mask, default 0                |

### Peripherals Control (C-Model Only)

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --useVcmd              | (Only valid fo CModel) Whether to enable VCMD.                 |
|       |                        | 0 - Disable                                                    |
|       |                        | 1 - Enable                                                     |

### Low latency gating

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --lowlatGatingDisable  | 0..1 Lowlatency handshake auto gating disable[1]               |
|       |                        |      0: enable lowlatency handshake auto gating.               |
|       |                        |      1: disalbe lowlatency handshake auto gating.              |
|       |                        | If enable, check emc and  recon idle.                          |
## Debugging

### Dump Registers

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --dumpRegister         |  Whether to dump register. [0]                                 |
|       |                        |  Value range:                                                  |
|       |                        |  0 - Dump no register.                                         |
|       |                        |  1 - Dump print register.                                      |
|       |                        |  When it is set to 1, software dumps register values to        |
|       |                        |  stdout or  a log file according to the --logOutDir setting.   |

### Log and Print Message Control

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
|       | --logOutDir            | Log message output directory. [0]                              |
|       |                        | 0 - All logs output to stdout.                                 |
|       |                        | 1 - One log file for all logs.                                 |
|       |                        | 2 - One log file of each instance thread.                      |
|       | --logOutLevel          | Log message output level. [3]                                  |
|       |                        | 0 -"quiet", no output                                          |
|       |                        | 1 - "fatal", serious errors                                    |
|       |                        | 2 - "error", error occurs                                      |
|       |                        | 3 - "warn", warning                                            |
|       |                        | 4 - "info", general information                                |
|       |                        | 5 - "debug", debug information                                 |
|       |                        | 6 - "all", all log information                                 |
|       | --logTraceMap          | Trace information of log message output for prompting and      |
|       |                        |  debugging. [63]=b`0111111                                     |
|       |                        | Bit 0 - Dump API call.                                         |
|       |                        | Bit 1 - Dump registers.                                        |
|       |                        | Bit 2 - Dump EWL.                                              |
|       |                        | Bit 3 - Dump memory usage.                                     |
|       |                        | Bit 4 - Dump Rate Control Status                               |
|       |                        | Bit 5 (This bit is reserved for future use) - Output full      |
|       |                        |  command line                                                  |
|       |                        | Bit 6 - Output performance information                         |

### Testing

These options are not supported for end users.

| Short | Long Option            | Description                                                    |
|-------|------------------------|----------------------------------------------------------------|
| -P[n] | --trigger              | Logic analyzer trigger for the nth picture. [-1]               |
|       |                        | -1 - Disable the trigger.                                      |
| -D[n] | --XformCustomerPrivateFormat | Customer private format to be converted from YUV420. [-1]|
|       |                        | -1 - Disable conversion                                        |
|       |                        | 0 - Customer private tile format for HEVC                      |
|       |                        | 1 - Customer private tile format for H.264                     |
|       |                        | 2 - Customer private YUV422SP_888                              |
|       |                        | 3 - Common data 8-bit tile 4x4                                 |
|       |                        | 4 - Common data 10-bit tile 4x4                                |
|       |                        | 5 - Customer private tile format for JPEG                      |
|       | --osdDec400TableInput  | OSD DEC400 compressed table input from file.                   |
|       |                        | [osdDec400CompTableinput.bin]                                  |

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

