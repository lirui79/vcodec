# Options

The chapter describes the options available in the test bench for configuring the video decoder.

- To specify a long option, separate the option and the value with an equal sign (=). For example, <code>-\-num-pictures=10</code>.
- To specify a short option, separate the option and the value with an equal sign (=) or omit the equal sign. For example, <code>-N10</code>.

In this chapter:

- In short options, [s] requires a string value and [n] requires an integer value.
- The value range of an option is provided at the beginning of its description, with the boundaries separated by a couple of periods . Both boundaries are inclusive. For limit values available, the valid values are listed in set, e.g.,  [value1, value2, ..., valueN].
- String \<s\> in the value of an option means s is optional.
- The default value of an option is marked by parentheses (default) at the end of the first paragraph of the option description.

## Help Info Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -H   | --help                 | Print command line parameters help info.                          |

## Testbench Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --tb-cfg=[s]           | Specify the path of tb.cfg file, e.g. ./tb.cfg, e.g. ./xxx.cfg    |

### Input Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -i   | --input-format=[s]     | Force input file format interpretation.                           |
|       |                        | Format can be one of the following.                               |
|       |                        | bs - bytestream format                                            |
|       |                        | ivf - IVF file format                                             |
|       |                        | webm - WebM file format                                           |
|  -p   | --packet-by-packet     | Packetize input bitstream                                         |
|  -F   | --full-stream          | Read full-stream into single buffer. Only with bytestream.        |
|  -u   | --nalu                 | NALU input bitstream (without start code)                         |
|  -n   | --non-ringbuffer       | Disable ringbuffer mode for stream input buffer.                  |
|       |                        | By default testbench may store input stream in a ring buffer mode.|

### Threading Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -Z   |--separate-output-thread| Run output handling in separate thread.                           |
|       | --multimode=[n]        | Temporary testing multiple decoder instances in multi-thread      |
|       |                        |  mode or multi-process mode (only valid for g2dec). (0)           |
|       |                        | Specify decoders running in parallel in multi-thread or multi-    |
|       |                        |  process mode.                                                    |
|       |                        | 0 - Disable                                                       |
|       |                        | 1 - Multi-thread mode                                             |
|       |                        | 2 - Multi-process mode (reserved)                                 |
|       | --streamcfg=[s]        | Specify the filename storing decoder options.                     |

### Output Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -t   | --trace-files          | Generate hardware trace files.                                    |
|  -r   | --rtl-trace            | Trace file format for RTL (extra CU ctrl).                        |
|  -R   | --disable-display-order| Output in decoding order.                                         |
|  -X   | --no-write             | Disable output writing.                                           |
|  -O[s]| --output-file=[s]      | Write output to the specified file.                               |
|  -Q   | --single-frames-out    | Output single frames.                                             |
| -N[n] | --num-pictures=[n]     | Stop after outputting [n] frames.                                 |
|  -M   | --md5                  | Write MD5 sum to output instead of yuv.                           |
|  -m   | --md5-per-pic          | Write MD5 sum for each picture to output instead of yuv.          |
|       |                        | Note: -m (-M) not guarantee sync up with the third md5 tools.     |

### Logmsg Options
If logging is enabled by setting LOGMSG=y when you compile the test bench, you can configure the log output by
setting the following environment variables in Linux Bash before running the test bench.

- The following example commands enable decoder IRQ logs and register configuration logs of the all information to be output.
 E,g. <code>-\-logtracemap=IRQ|REGS</code>  <code>-\-logoutlevel=4</code>

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --logoutlevel=[n]      | 0..4 Control log message output level. (1)                        |
|       |                        | 0 - quiet                                                         |
|       |                        | 1 - error                                                         |
|       |                        | 2 - info                                                          |
|       |                        | 3 - debug                                                         |
|       |                        | 4 - all                                                           |
|       | --logtracemap=[s]      | Indicate what kind of log should be output for debug. (API)       |
|       |                        | E.g. --logtracemap=API...                                         |
|       |                        | CFG - config (dump full command line feature list and tb.cfg)     |
|       |                        | API - api (dump api call and some key param)                      |
|       |                        | DWL - dwl (dump dwl call, replace -D_DWL_DEBUG)                   |
|       |                        | MEM - mem (dump memory usage)                                     |
|       |                        | DPB - dpb (replace -DDPB_LOCK_TRACE)                              |
|       |                        | REGS - regs (dump registers)                                      |
|       |                        | VCMD - vcmd (dump vcmd instruction)                               |
|       |                        | IRQ - irq (dump IRQ status)                                       |
|       |                        | PERF - perf (perforamnce information)                             |
|       |                        | ALL - all (dump all trace)                                        |
|       | --logoutdir=[s]        | 0..3 Control log message output dir. (0)                          |
|       |                        | 0 - all log output to stdout                                      |
|       |                        | 1 - all log use one log file                                      |
|       |                        | 2 - each thread of instance has its own file                      |
|       |                        | 3 - all log output to stderr                                      |

## Decoding Flow Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --low-latency          | Enable low latency platform running flag.                         |
|       | --secure               | Enable secure mode flag.                                          |
|       | --mc                   | Enable multi-core decoding (frame-based for H264/HEVC/VVC/AVS2/AVS3,|
|       |                        |  while tile-based for AV1/VP9).                                   |
|       | --intra-only           | Decode intra frames only.                                         |

## Driver Selection Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --dec-dev=[s]          | Set decoder device file name.  (/tmp/dev/hantrodec)               |
|       | --mem-dev=[s]          | Set memalloc device file name. (/tmp/dev/memalloc)                |

## Decoding Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -b   | --compress-bypass      | Bypass reference frame compression.                               |
|       | --num-buffers=[n]      | 1..15 Set [n] frame buffers when initialization and then start    |
|       |                        |  decoding.                                                        |
|       | --skip-frame=[s]       | Skip some frames when decoding.                                   |
|       |                        | non_ref_recon - Decode all frame, but decoder HW doesn't          |
|       |                        |  write recon data of non-reference frames. Only for hevc &        |
|       |                        |  h264_high10.                                                     |
|       |                        | non_ref - Don't decode non-reference frames.                      |
|       |                        | none - Close this feature (don't skip anything).                  |
|       | --index-file=[s]       | Set the length of data that demuxer transmits to decoder via      |
|       |                        |  index file.                                                      |
|       |                        | Set [path/to/file] as the path of index file.                     |
|       |                        | Use macro USE_DUMP_INPUT_STREAM to get index file.                |
| -A[n] | --align=[n]            | Set stride alignment to n bytes. (16)                             |
|       |                        |  [1,8,16,32,64,128,256,512,1024,2048]                             |
|       | --align-height=[n]     | [1,8,16,32,64,128] Set height aligned to n pixel lines.           |
|       | --auxinfo=[n]          | 0..3 Specify auxiliary info dumping type (HEVC/H264). (0)         |
|       |                        | 0 - Disable                                                       |
|       |                        | 1 - QP dumping                                                    |
|       |                        | 2 - MV dumping                                                    |
|       |                        | 3 - Both QP and MV dumping                                        |
|       | --tlayer=[n]           | -1..7 Maximum Temporal Layer to be decoded. -1 means to decode    |
|       |                        |  all temporal layers.                                             |

### Error Concealment Options
There are three typical software error concealment polices. User can choose an appropriate policy based on your demand.

- For clarity first decoding, which means just to display correct pictures, use <code>-\-ec=3</code> or <code>-\-ec=no_error</code>to enable this mode.
- For fluency first decoding, that is, to display as many pictures as you can, including error pictures, use <code>-\-ec=2</code> or <code>-\-ec=ignore_error</code> to enable this mode.
- To trade off between clarity and fluency, please set an error tolerent ratio. This mode is enabled by default.

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --error-ratio=[n]      | Tolerable error ratio [n], the frame with an error ratio less     |
|       |                        |  than or equal to n% will be output.                              |
|       |                        | Only used when \-\-ec=tolerant_error mode.                        |
|       | --ec=[s]               | Set error handling policy:                                        |
|       |                        | tolerant_error - Balanced strategy ([1])                          |
|       |                        | ignore_error - Fluency first strategy [2]                         |
|       |                        | no_error - Clarity first strategy [3]                             |
|       | --dis-ec               | Disable HW error concealment (currently for HEVC/H264 only).      |
|       | --dis-slice            | Disable slice decoding (currently for HEVC/H264 only).            |

## Post Processing Options

### Crop/Scale Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
| -C[s][n] | --crop=[s]          | For long options, s is in format like wxh@(x,y) crop a rectangle  |
|       |                        |  with size wxh from (x,y), where x, y, w, h are all integers.     |
|       |                        | For short option, it's one of the 4 options below, specifying     |
|       |                        |  either the offset or width / height of the cropped window.       |
|       |                        | -Cx[n] - left offset [n] from left boundary                       |
|       |                        | -Cy[n] - top offset [n] from top boundary                         |
|       |                        | -Cw[n] - width of the cropping window is [n]                      |
|       |                        | -Ch[n] - height of the cropping window is [n]                     |
|       |                        | E.g.,                                                             |
|       |                        | -Cx8 -Cy16 : Crop from (8, 16)                                    |
|       |                        | -Cw720 -Ch480 : Crop size  720x480                                |
|       |                        | Above options are same as --crop=720x480@(8,16)                   |
| -d[x<:y>] | --down_scale=[x<:y>]  | Set down scale ratios. (same as --scale=[-dx<:y>])             |
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
|       | --filter-param=[mxn]   | Specify the fitering window size mxn. (ONLY 2x2 is allowed).      |
|       | --pp-src-sel=[n]       | Specify how to select the source for scaling. (DOWN_ROUND)        |
|       |                        | DOWN_ROUND                                                        |
|       |                        | NO_ROUND                                                          |
|       |                        | UP_ROUND                                                          |
|       | --scaling-pad-yuv=[Y,U,V] | Set the values for source pixels out of picture when scaling.  |
|       | --antialias            | 0..1 Flag to enable anti-aliasing effect of scaling. (1)          |
|       |                        | 0 - Disable anti-aliasing by using fixed bicubic or bilinar       |
|       |                        |  filter size.                                                     |
|       |                        | 1 - Enable anti-aliasing by using flexible bicubic or bilinar     |
|       |                        |  filter size relevant to scaling ratio.                           |

### Output Format Options
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
|       | --dec400-align         | Sets the alignment for DEC400 compressed data.                    |
|       |                        | Valid values:                                                     |
|       |                        | 32 - 32-byte alignment.                                           |
|       |                        | 64 - 64-byte alignment.                                           |
|       | --tiled-mode=[s]       | Specify the tile mode/size. Always enabled for FBC.               |
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
|       | --pos=[s]              | Set delog rectangle area parameter, which is in format wxh@(x,y), |
|       |                        |  where w, h, x, y are all integers. E.g.,--pos=36x24@(64,48)      |
|       | --show=[n]             | 0..1 Show the delogo border.                                      |
|       |                        | 0 Hide the delogo border.                                         |
|       |                        | 1 Show the delogo border.                                         |
|       | --mode=[s]             | Select the delogo mode.                                           |
|       |                        | PIXEL_INTERPOLATION - Delogo with interpolated pixels.            |
|       |                        | PIXEL_REPLACE - Delogo with given pixels from option --YUV        |
|       | --YUV=[(r,g,b)]        | Set the replacing value if PIXEL_REPLACE mode is enabled.         |
|       |                        | E.g.,--YUV=(113,142,129)                                          |

### YUV2RGB Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --pp-rgb               | Enable compact YUV2RGB.                                           |
|       | --pp-rgb-planar        | Enable planar YUV2RGB, with each component in separted planes.    |
|       | --rgb-fmat=[s]         | Set the RGB output format.                                        |
|       |                        | RGB888/BGR888                                                     |
|       |                        | R16G16B16/B16G16R16                                               |
|       |                        | RGB888_P/BGR888_P (Planar mode)                                   |
|       |                        | R16G16B16_P/B16G16R16_P (Planar mode)                             |
|       |                        | ARGB888/ABGR888/RGBA888/BGRA888 (Planar not supported)            |
|       |                        | A2R10G10B10/A2B10G10R10 (Planar not supported)                    |
|       |                        | R10G10B10A2/B10G10R10A2 (Planar not supported)                    |
|       |                        | XRGB888/XBGR888 (Planar not supported)                            |
|       | --rgb-std=[s]          | Set the standard coeff to do YUV2RGB.                             |
|       |                        | BT601                                                             |
|       |                        | BT601_L                                                           |
|       |                        | BT709                                                             |
|       |                        | BT709_L                                                           |
|       |                        | BT2020                                                            |
|       |                        | BT2020_L                                                          |
|       | --rgb-alpha=[n]        | Set the alpha value to [n] when output is in ARGB format.         |

### Range Mapping

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --source-range         | Sets the source YUV or RGB range for range mapping.               |
|       |                        | Valid values: FULL and LIMITED.                                   |
|       | --target-range         | Sets the target YUV or RGB range for range mapping.               |
|       |                        | Valid values: FULL and LIMITED.                                   |


### 3D LUT

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --3dlut                | Enables 3D LUT based color remapping for RGB output.              |
|       | --table-3dlut=[s]      | Specifies the path to the 3D LUT table file.                      |
|       |                        | The 3D LUT table must be a .txt file that contains 17x17x17       |
|       |                        |  lines, with each line in the following format:                   |
|       |                        |  index0,index1,index2:R,G,B,                                      |
|       |                        | The index increase order is that index2 (B dimension) first       |
|       |                        |  increases from 0 to 16, then index1 (G dimension) increases from |
|       |                        |  0 to 16, and finally index0 (R dimension) increases from 0 to 16.|


### PVFBC

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --libfile=[s]          | Specifies the input PVFBC libfile.                                |
|       |                        | The file is used only for software testing.                       |
|       | --const-pix=[n:n:n:n]  | Sets the constant pixel value in format luma:luma:chroma:chroma.  |
|       |                        | For example, you can specify -\-const-pix=100:20:128:200.         |

## Format Specific Options
This section describes the specific options of different video formats.

### H264 Only Features
The video decoder supports multi view sequence, it's enabled by setting <code>-\-mvc</code> in command line.
It's also supported sequence which is flexible marcroblock ordering or abitray slice ordering. You should use
<code>-\-is-asofmo</code> to specify this sequence has this characteristic so that the decoder can do related process.

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --mvc                  | Enable MVC decoding (H264 only).                                  |
|       | --is-asofmo            | Input file may include ASO/FMO feature.                           |

### AV1 Only Features
The options only supported in av1 are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --oppoint              | Just decode operating point 0. If not specified, all the layers   |
|       |                        |  will be decoded.                                                 |

### JPEG Only Features
The options only supported in jpeg are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --full-only            | Force to full resolution decoding only and ignore thumbnail.      |
|       | --ri-mc-enable         | Enable restart interval based multicore decoding.                 |
|       | --instant-buffer       | Output buffer provided by user. (History Customized)              |

### VP8 Only Features
The options only supported in vp8 are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --snap-shot            | Set decode format to webp.                                        |
|       | --extra-bits=[n]       | Add n bytes of extra space after stream buffer for decoder.       |
|       | --user-mem-alloc       | User allocates picture buffers.                                   |

### VP6 Only Features
The options only supported in vp6 are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --alpha                | Stream contains alpha channel.                                    |

### MPEG4 Only Features
The options only supported in mpeg4 are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --strm-sorenson        | Decode Sorenson Spark stream.                                     |
|       | --strm-custom          | Decode DivX4 or DivX5 stream.                                     |
|       | --custom=[s]           | Set DivX3 stream resolution. [s] is in format wxh, with w, h      |
|       |                        |  both integers.                                                   |

### VC1 Only Features
The options only supported in vc1 are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --long-stream          | Enable support for long streams.                                  |
|       | --frame-picture        | Enable frame picture writing in multiresolutin output.            |

### RV Only Features
The options only supported in rv are listed as follows:

| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --rv-input             | Specify that input file is in RealVideo format.                   |

## Post Processing (Standalone) Testbenh Features

Post processing testbench shares the same "Logmsg Options" and "Post Processing Options" above. Besides, it has dedicated options listed below.

### Testbench Input Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
| -S[n] | --in-stride=[n]        | Set input stream stride to n bytes.                               |
| -W[n] | --width=[n]            | Set input stream width to n pixels (MUST).                        |
| -H[n] | --height=[n]           | Set input stream height to n pixels (MUST).                       |

### Testbench Output Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|  -X   | --no-write             | Disable output writing.                                           |
| -O[s] | --output-file=[s]      | Write output to the specified file.                               |
| -N[n] | --num-pictures=[n]     | Stop after outputting [n] frames.                                 |
|  -M   | --md5                  | Write MD5 sum to output instead of yuv.                           |
|  -m   | --md5-per-pic          | Write MD5 sum for each picture to output instead of yuv.          |
|       |                        | Note: -m (-M) not guarantee sync up with the third md5 tools.     |

### Input Format Options
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --in-fmt=[s]           | Specify input format:                                             |
|       |                        | NV12, NV21, 420P, 422YUYV, 422YVYU, 422UYVY, 422VYUY              |
|       |                        | NV12_P010, NV21_P010, 420P_P010                                   |
|       |                        | TILED4x4, TILED4x4_P010                                           |
|       |                        | TILED8x8, TILED8x8_P010 (Only supported in dec400d)               |
|       |                        | TILED8x8_YUV400, TILED8x8_P010_YUV400 Only supported in dec400d)  |
|       |                        | TILED64x64_ARGB888 (Only supported in dec400d)                    |
|       |                        | TILED64x64_A2R10G10B10 (Only supported in dec400d)                |
|       |                        | RGB888/BGR888 (Packed mode)                                       |
|       |                        | RGB888_P/BGR888_P (Planar mode)                                   |
|       |                        | ARGB888/ABGR888 (Packed mode)                                     |
|       |                        | A2R10G10B10/A2B10G10R10 (Packed mode)                             |
|       |                        | X2R10G10B10/X2B10G10R10 (Packed mode)                             |
|       |                        | R16G16B16_P/B16G16R16_P (Planar mode)                             |
|       | --inI010               | Specify input YUV in I010 format (10-bit pixel in LSB of 16 bits).|
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
| Short | Long Option            | Description                                                       |
|-------|------------------------|-------------------------------------------------------------------|
|       | --IL=[s]               | Specify input luma RFC data.                                      |
|       | --IC=[s]               | Specify input chroma RFC data.                                    |
|       | --SL=[s]               | Set input luma RFC data stride.                                   |
|       |                        | Luma_stride calculation:                                          |
|       |                        | align = alignment of input data                                   |
|       |                        | pixel_width = (bit_depth_luma == 8 && bit_depth_chroma == 8) ?    |
|       |                        |  8 : 10;                                                          |
|       |                        | luma_stride = NextMultiple(8 * frame_width * pixel_width ,        |
|       |                        |  align * 8) / 8;                                                  |
|       | --SC=[n]               | Set input chroma RFC data stride.                                 |
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
