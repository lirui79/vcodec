
# Options

The chapter describes the options available in the test bench for configuring the pixel processor.

- To specify a long option, separate the option and the value with an equal sign (=). For example, <code>-\-num-pictures=10</code>.
- To specify a short option, there is no character between the option and the value. For example, <code>-N10</code>.

In this chapter:

- In short options, [s] requires a string value and [n] requires an integer value.
- The value range of an option is provided at the beginning of its description, with the boundaries separated by a couple of periods . Both boundaries are inclusive.
- String \<s\> in the value of an option means s is optional.
- The default value of an option is marked by parentheses (default) at the end of the first paragraph of the option description.

## Help Info options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -H   | --help                 | Print command line parameters help info.                          |

## Testbench Common Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -X   | --no-write             | Disable output writing.                                           |
| -O[s] | --output-file=[s]      | Write output to the specified file.                               |
| -N[n] | --num-pictures=[n]     | Stop after outputting [n] frames.                                 |
|  -M   | --md5                  | Write MD5 sum to output instead of yuv.                           |
|  -m   | --md5-per-pic          | Write MD5 sum for each picture to output instead of yuv.          |
|       |                        | Note: -m (-M) not guarantee sync up with the third md5 tools.     |

## Input Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
| -S[n] | --in-stride=[n]        | Set input stream stride to n bytes.                               |
| -W[n] | --width=[n]            | Set input stream width to n pixels (MUST).                        |
| -H[n] | --height=[n]           | Set input stream height to n pixels (MUST).                       |

### Input Format
NOTE: If not specified explicitly, the default input format will be assumed as semi-planar (a.k.a., NV12).
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --in-fmt=[s]           | Specify the input format. [NV12]                                |
|       |                        | Valid values:                                                     |
|       |                        | - YUV formats: NV12, NV21, 420P, NV12_P010,                       |
|       |                        |  NV21_P010, 420P_P010, 422NV16, 422NV61,422YUYV, 422YVYU,         |
|       |                        |  422UYVY, 422VYUY, 422NV16_P010, 422NV61_P010, 444P, 444P_P010    |
|       |                        | - Packed RGB formats: RGB888, BGR888, ARGB888,                    |
|       |                        |  ABGR888, RGBA888, BGRA888, A2R10G10B10, A2B10G10R10,             |
|       |                        |  R10G10B10A2, B10G10R10A2, XRGB888, XBGR888                       |
|       |                        | - Planar RGB formats: RGB888_P, BGR888_P,                         |
|       |                        |  R16G16B16_P, B16G16R16_P                                         |
|       |                        | - DEC400 compressed input formats: TILED8x8, TILED8x8_P010,       |
|       |                        |  TILED8x8_YUV400, TILED8x8_P010_YUV400, TILED64x64_ARGB888,       |
|       |                        |  TILED64x64_XRGB888, TILED64x64_A2R10G10B10,                      |
|       |                        |  TILED64x64_X2R10G10B10                                           |
|       | --inI010               | Specifies that the input YUV data is in I010 format (10-bit       |
|       |                        |  pixel in LSB of 16 bits).                                        |
|       |                        | I010 is supported only by the software. For details about this    |
|       |                        |  format, see Vivante GC820T Series Memory Buffer and Format       |
|       |                        |  Organization.                                                    |
|       | --in400                | Specify input chroma format in 400.                               |
|       | --in420                | Specify input chroma format in 420.                               |

### DEC400 Input Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --indec400             | Specify input dec400 data.                                        |
|       | --indec400-align=[n]   | [32,64] Alignment set for dec400d:                                |
|       |                        | 32 - 32B                                                          |
|       |                        | 64 - 64B                                                          |
|       | --TS=[s]               | Specify input ts table for dec400 decompress.                     |

### RFC Input Options

Code snippet written in C demonstrates the calculation of parameters relating to RFC input. This document
assumes that the reader understands the fundamentals of the C-language and basic video concepts.

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --IL=[s]               | Specify input luma RFC data file.                                 |
|       | --IC=[s]               | Specify input chroma RFC data file.                               |
|       | --SL=[n]               | Set input luma RFC data stride in bytes.                          |
|       |                        | Luma_stride calculation:                                          |
|       |                        | align = alignment of input data                                   |
|       |                        | pixel_width = (bit_depth_luma == 8 && bit_depth_chroma == 8) ?    |
|       |                        |  8 : 10;                                                          |
|       |                        | luma_stride = NextMultiple(8 * frame_width * pixel_width ,        |
|       |                        |  align * 8) / 8;                                                  |
|       | --SC=[n]               | Set input chroma RFC data stride in bytes.                        |
|       |                        | Chroma_stride calculation:                                        |
|       |                        | align = alignment of input data                                   |
|       |                        | pixel_width = (bit_depth_luma == 8 && bit_depth_chroma == 8) ?    |
|       |                        |  8 : 10;                                                          |
|       |                        | chroma_stride = NextMultiple(4 * frame_width * pixel_width,       |
|       |                        |  align * 8) / 8;                                                  |
|       | --LT=[s]               | Specify input luma table file.                                    |
|       |                        | Luma table size calculation:                                      |
|       |                        | pic_width_in_cbs = (pic_width + 8 - 1) / 8;                       |
|       |                        | pic_height_in_cbs = pic_height / 8;                               |
|       |                        | table_stride = NextMultiple(pic_width_in_cbs, 16);                |
|       |                        | luma_table_size = table_stride * pic_height_in_cbs;               |
|       | --CT=[s]               | Specify input chroma table file.                                  |
|       |                        | Chroma table size calculation:                                    |
|       |                        | pic_width_in_cbs = (pic_width + 16 - 1) / 16                      |
|       |                        | pic_height_in_cbs =  pic_height / 8  ;                            |
|       |                        | table_stride = NextMultiple(pic_width_in_cbs, 16);                |
|       |                        | chroma_table_size = table_stride * pic_height_in_cbs;             |
|       | --AT=[s]               | Specify input luma table + input chroma table.                    |
|       | --LB=[n]               | Specify input luma bitdepth.                                      |
|       | --CB=[n]               | Specify input chroma bitdepth.                                    |

## Pixel Processing Options

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --pp=[n]               | Enable pp channel [n]. (0)                                        |
|       |                        | Options following it and preceding next --pp are all params for   |
|       |                        |  pp channel [n].                                                  |
| -A[n] | --align=[n]            | Sets the stride alignment, in bytes. [16]                         |
|       |                        | Valid values: 1, 8, 16, 32, 64, 128, 256, 512, 1024, and 2048.    |

### Crop/Scale Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
| -C[s][n] | --cp=[s]            | Set either the offset or width / height of the cropped window.    |
|       |                        | Should be one of the 4 options below:                             |
|       |                        | -Cx[n] - left offset [n] from left boundary                       |
|       |                        | -Cy[n] - top offset [n] from top boundary                         |
|       |                        | -Cw[n] - width of the cropping window is [n]                      |
|       |                        | -Ch[n] - height of the cropping window is [n]                     |
|       |                        | E.g.,                                                             |
|       |                        | -Cx8 -Cy16 : Crop from (8, 16)                                    |
|       |                        | -Cw720 -Ch480 : Crop size  720x480                                |
|       | --crop=[s]       | Set the offset or width / height of the cropped window in one     |
|       |                        |  param string [s], which is in format like wxh@(x,y), meaning to  |
|       |                        |  crop a rectangle of size wxh from (x,y), where x, y, w, h are    |
|       |                        |  all integers.                                                    |
|       |                        | Above options are same as -\-crop=720x480@(8,16)                  |
|       | --crop-win=[n]         | 1..4 Multiply windows crop. (1)                                   |
|       |                        | Indicate specific windows area for following cropping options.    |
|       |                        | E.g., following options set the 3rd cropping window to            |
|       |                        |  720x480@(8,16):                                                  |
|       |                        | -\-crop-win=3 -Cx8 -Cy16 -Cw720 -Ch480                            |
|       |                        | NOTE: multiple cropping is exclusive with scaling.                |
|       | --pic-stitch=[n]       | Specifies the picture stitch mode.                                |
|       |                        | Valid values:                              |
|       |                        | <code>4x1</code> - Stitches image by 1 rows and 4 column.         |
|       |                        | <code>1x4</code> - Stitches image by 4 row and 1 columns.         |
|       |                        | <code>2x2</code> - Stitches image by 2 rows and 2 columns.        |
|       | --crop=[wxh@(x,y)]     | Specifies the cropping region.                                    |
| -d[x<:y>]| --down_scale=[x<:y>]| Set down scale ratios. (same as -\-scale=[-dx<:y>])               |
|       |                        |  x, y are integers from [1,2,4,8], which means to be scaled to    |
|       |                        |  1/x, 1/y in horizontal/vertical respectively.                    |
|       |                        | E.g.,:                                                            |
|       |                        | -d1 : Just enable pp without down-scaling (1:1 output)            |
|       |                        | -d2 : Down-scaling to 1/2 in both horizontal/vertical directions  |
|       |                        | -d2:4 : Down-scaling to 1/2 in horizontal direction, and 1/2 in   |
|       |                        |  vertical                                                         |
| -D[s] | --scale=[s]            | [s] is in format wxh, specifying PP output size wxh (either by    |
|       |                        |  down-scaling or up-scaling), where both w and h are integers.    |
|       |                        | E.g.,                                                             |
|       |                        | -D1280x720 : Set PP output size to 1280x720                       |
|       | --second-crop          | Enable the crop after scale, followed by crop parameter -C[s].    |

### Filter Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --pp-filter=[s]        | Specify filtering algothrim for scaling. (LANCZOS)                |
|       |                        | VSI_LINEAR                                                        |
|       |                        | LANCZOS                                                           |
|       |                        | NEAREST                                                           |
|       |                        | BILINEAR                                                          |
|       |                        | BICUBIC                                                           |
|       |                        | SPLINE                                                            |
|       |                        | BOX                                                               |
|       |                        | AREA                                                              |
|       | --filter-param=[mxn]   | Specify the fitering window size mxn. (ONLY 2x2 is allowed).      |
|       | --pp-src-sel=[n]       | Specify how to select the source for scaling. (DOWN_ROUND)        |
|       |                        | DOWN_ROUND                                                        |
|       |                        | NO_ROUND                                                          |
|       |                        | UP_ROUND                                                          |
|       | --scaling-pad-yuv=[Y,U,V] | Set the values for source pixels out of picture when scaling.  |
|       | --antialias            | 0..1 Flag to enable anti-aliasing effect of scaling. (1)          |
|       |                        | 0 - Disable anti-aliasing by using fixed bicubic or bilinar       |
|       |                        |  filter size. This is the similiar behaviour with OpenCV and      |
|       |                        |  torchvision.                                                     |
|       |                        | 1 - Enable anti-aliasing by using flexible bicubic or bilinar     |
|       |                        |  filter size relevant to scaling ratio. This is the similiar      |
|       |                        |  behaviour with ffmpeg.                                           |

### Output Format Options

NOTE: If not specified explicitly, the default output format will be assumed as semi-planar (a.k.a., NV12) for 8-bit input. For 10-bit input, the default output format is assumed as NV12 P010.

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --pp=[n]               | Enable pp channel [n].                                            |
| -E[s] | --enable=[s]           | Set hw output format to one of following:                         |
|       |                        | rs - Raster scan conversion (a.k.a., semi-planar)                 |
|       |                        | pack10 - YUV420pack10 for 10-bit and 12-bit stream                |
|       |                        | p010 - P010 format for 10-bit and 12-bit stream                   |
|       |                        | I010 - I010 format for 10-bit and 12-bit stream                   |
|       |                        | L010 - L010 format for 10-bit and 12-bit stream                   |
|       |                        | 1010 - 1010 format for 10-bit and 12-bit stream                   |
|  -f   | --force-8bits          | Force output in 8 bits per pixel for 10-bit stream.               |
|       | --cr-first             | PP outputs chroma in CrCb order                                   |
|       | --pp-shaper            | Enable shaper for pp <n>. (Default support)                       |
|       | --pp-shaper-dis        | Disable shaper for pp <n>.                                        |
|       | --pp-shaper-no-pad     | Shaper no need to pad for frame right edge.                       |
|       | --pp-ycbcr=[s]         | Set the pp output chroma format. By default CbCr are stored in    |
|       |                        |  one plane (a.k.a., semi-planar format) if "--pp-planar" is not   |
|       |                        |  enabled explicitly.                                              |
|       |                        | YUV420                                                            |
|       |                        | YUV422                                                            |
|       |                        | YUV444                                                            |
|  -a   | --pp-planar            | Enable PP output in planar format. Default is semi-planar.        |
|  -U   | --pp-tiled-out         | Enable PP output in tile format. Default is 4x4 tile.             |
|       | --pp-luma-only         | Enable PP output YUV400, a.k.a., monochroma.                      |
|       | --pp-comp              | Enable compression to PP output.                                  |
|       | --dec400-align         | Alignment set for dec400 compression data.                        |
|       |                        | 32 - 32Bytes                                                      |
|       |                        | 64 - 64Bytes                                                      |
| -q[s] | --tiled-mode=[s]       | Specify the tile mode/size. Always enabled for FBC.               |
|       |                        | TILED8x8                                                          |
|       |                        | TILED16x16                                                        |
|       |                        | TILED128x2                                                        |
|       |                        | TILED64x64 (a.k.a., SuperTileX)                                   |
|       | --pp-packed-mode=[s]   | Specify the YUV packed mode for customed format.                  |
|       |                        | YUYV                                                              |
|       |                        | UYVY                                                              |
| -s[s] | --out_stride=[yc][n]   | Set PP stride of y/c plane to [n]. For short option, [s] is one   |
|       |                        |  of y[n] or c[n].                                                 |
|       |                        | E.g.,                                                             |
|       |                        | -sy720 -sc360 : Set stride of luma/chroma to 720 and 360.         |
|       | --lc-stripe=[n]        | Set the lines number increasement to trigger updating to line     |
|       |                        |  counter register. Set n = 0 to disable line count update.        |

### Delogo Options (Optional)
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --delogo=[n]           | 0..1 Specify the delogo area. There are two delogo area supported.|
|       |                        | Need to configure following parameters for a given delogo area:   |
|       | --pos=[s]              | Set delogo rectangle area parameter, which is in format wxh@(x,y),|
|       |                        |  where w, h, x, y are all integers. E.g.,--pos=36x24@(64,48)      |
|       | --show=[n]             | 0..1 Show the delogo border.                                      |
|       |                        | 0 Hide the delogo border.                                         |
|       |                        | 1 Show the delogo border.                                         |
|       | --mode=[s]             | Select the delogo mode.                                           |
|       |                        | PIXEL_INTERPOLATION - Delogo with interpolated pixels.            |
|       |                        | PIXEL_REPLACE - Delogo with given pixels from option --YUV        |
|       | --YUV=[(r,g,b)]        | Set the replacing value if PIXEL_REPLACE mode is enabled.         |
|       |                        | E.g.,-\-YUV=(113,142,129)                                         |

### YUV2RGB Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --pp-rgb               | Enable compact YUV2RGB.                                           |
|       | --pp-rgb-planar        | Enable planar YUV2RGB, with each component in separted planes.    |
|       | --rgb-fmat=[s]         | Set the RGB output format.                                        |
|       |                        | RGB888/BGR888 (Packed mode)                                       |
|       |                        | R16G16B16/B16G16R16 (Packed mode)                                 |
|       |                        | RGB888_P/BGR888_P (Planar mode)                                   |
|       |                        | R16G16B16_P/B16G16R16_P (Planar mode)                             |
|       |                        | ARGB888/ABGR888/RGBA888/BGRA888 (Packed mode)                     |
|       |                        | A2R10G10B10/A2B10G10R10 (Packed mode)                             |
|       |                        | R10G10B10A2/B10G10R10A2 (Packed mode)                             |
|       |                        | XRGB888/XBGR888 (Packed mode)                                     |
|       |                        | X2R10G10B10/X2B10G10R10 (Packed mode)                             |
|       | --rgb-std=[s]          | Set the standard coeff to do YUV2RGB.                             |
|       |                        | BT601                                                             |
|       |                        | BT601_L                                                           |
|       |                        | BT709                                                             |
|       |                        | BT709_L                                                           |
|       |                        | BT2020                                                            |
|       |                        | BT2020_L                                                          |
|       | --rgb-alpha=[n]        | Set the alpha value to [n] when output is in ARGB format.         |


### Range Mapping Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --source-range         | Set the source YUV/RGB range to do YUV/RGB range mapping.         |
|       |                        | FULL                                                              |
|       |                        | LIMITED                                                           |
|       | --target-range         | Set the target YUV/RGB range to do YUV/RGB range mapping.         |
|       |                        | FULL                                                              |
|       |                        | LIMITED                                                           |

### Color Remapping Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --3dlut                | Enable 3dlut (colore remapping) when RGB output.                  |
|       | --table-3dlut=[s]      | Set [s] as the path to 3dlut table file (.txt).                   |
|       |                        | The 3dlut input table should be a 17*17*17 lines *.txt file, where|
|       |                        | each line has the form:                                           |
|       |                        |                    index0,index1,index2:R,G,B,                    |
|       |                        | The index increase order is index2 (B dimension) incerases firstly|
|       |                        | from 0 to 16, then index1 (G dimension) incerases from 0 to 16,   |
|       |                        | and index0 (R dimension) incerases from 0 to 16 at last.          |

### PVFBC Compression Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --libfile=[s]          | Specify input PVFBC libfile                                       |
|       |                        |  (only for SW test)                                               |
|       | --const-pix=[n:n:n:n]  | Set the constant pixel value, luma:luma:chroma:chroma             |
|       |                        | E.g. --const-pix=100:20:128:200                                   |

## Logmsg Options
If logging is enabled by setting LOGMSG=y when you compile the test bench, you can configure the log output by
setting the following environment variables in Linux Bash before running the test bench.

- The following example commands enable pixel processor IRQ logs and register configuration logs of the all information to be output.
 E,g. <code>-\-logtracemap=IRQ|REGS</code>   <code>-\-logoutlevel=4</code>

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --logoutlevel=[n]      | 0..4 Control log message output level.                            |
|       |                        | 0 - quiet                                                         |
|       |                        | 1 - error (default)                                               |
|       |                        | 2 - info                                                          |
|       |                        | 3 - debug                                                         |
|       |                        | 4 - all                                                           |
|       | --logtracemap=[s]      | Indicate what kind of log should be output for debug.             |
|       |                        | E.g. --logtracemap=API...                                         |
|       |                        | CFG - config (dump full command line feature list and tb.cfg)     |
|       |                        | API - api (dump api call and some key param) (default)            |
|       |                        | DWL - dwl (dump dwl call, replace -D_DWL_DEBUG)                   |
|       |                        | MEM - mem (dump memory usage)                                     |
|       |                        | DPB - dpb (replace -DDPB_LOCK_TRACE)                              |
|       |                        | REGS - regs (dump registers)                                      |
|       |                        | VCMD - vcmd (dump vcmd instruction)                               |
|       |                        | IRQ - irq (dump IRQ status)                                       |
|       |                        | PERF - perf (perforamnce information)                             |
|       |                        | ALL - all (dump all trace)                                        |
|       | --logoutdir=[s]        | 0..3 Control log message output dir                               |
|       |                        | 0 - all log output to stdout (default)                            |
|       |                        | 1 - all log use one log file                                      |
|       |                        | 2 - each thread of instance has its own file                      |
|       |                        | 3 - all log output to stderr                                      |