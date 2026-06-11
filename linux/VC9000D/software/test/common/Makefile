#------------------------------------------------------------------------------
#-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
#-         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
#-         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
#-                                                                            --
#- This software is confidential and proprietary and may be used only as      --
#-   expressly authorized by VeriSilicon in a written licensing agreement.    --
#-                                                                            --
#-         This entire notice must be reproduced on all copies                --
#-                       and may not be removed.                              --
#-                                                                            --
#-------------------------------------------------------------------------------
#- Redistribution and use in source and binary forms, with or without         --
#- modification, are permitted provided that the following conditions are met:--
#-   * Redistributions of source code must retain the above copyright notice, --
#-       this list of conditions and the following disclaimer.                --
#-   * Redistributions in binary form must reproduce the above copyright      --
#-       notice, this list of conditions and the following disclaimer in the  --
#-       documentation and/or other materials provided with the distribution. --
#-   * Neither the names of Google nor the names of its contributors may be   --
#-       used to endorse or promote products derived from this software       --
#-       without specific prior written permission.                           --
#-------------------------------------------------------------------------------
#- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
#- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
#- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
#- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
#- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
#- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
#- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
#- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
#- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
#- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
#- POSSIBILITY OF SUCH DAMAGE.                                                --
#-------------------------------------------------------------------------------
#-----------------------------------------------------------------------------*/

G1_SRCS += $(DWL_SRCS)
G1_SRCS += $(COMMON_SRCS)
G1_SRCS += $(TEST_SRCS)

G2_SRCS += $(DWL_SRCS)
G2_SRCS += $(COMMON_SRCS)
G2_SRCS += $(TEST_SRCS)

LIB_DWL          = libdwl.a
LIB_COMMON       = libcommon.a
LIB_TBCOMMON     = libtbcommon.a
LIB_AVS          = libdecavs.a
LIB_JPEG         = libdecjpeg.a
LIB_MPEG2        = libdecmpeg2.a
LIB_MPEG4        = libdecmpeg4.a
LIB_RV           = libdecrv.a
LIB_VC1          = libdecvc1.a
LIB_VP6          = libdecvp6.a
LIB_VP8          = libdecvp8.a
LIB_H264         = libdech264.a

LIB_VVC          = libdecvvc.a
LIB_HEVC         = libdechevc.a
LIB_VP9          = libdecvp9.a
LIB_AVS2         = libdecavs2.a
LIB_AV1          = libdecav1.a
LIB_PP           = libpp.a

LIB_VCD_HW       = libvc9kd.a

LIB_FREERTOS        = libfreertos.a
LIB_FREERTOS_DRIVER = libdec_freertos_driver_static.a

ifeq ($(findstring arm,$(ENV)), arm)
    RV_TARGET=arm
else
    RV_TARGET=
endif

DEC_SRCS += software/test/common/utils/vcd_tools.c

ifeq ($(findstring avsdec,$(MAKECMDGOALS)),avsdec)
  G1_SRCS += $(AVS_SRCS)
  DEC_SRCS += software/test/avs/avsdectest.c
  LIBS += $(LIB_AVS)
  DEFINES += -DENABLE_AVS_SUPPORT
  SHARED_LIBS += $(LIB_AVS)
  SHARED_LIB = lib_vcd_avs.so
endif
ifeq ($(findstring h264dec,$(MAKECMDGOALS)),h264dec)
  # target: h264dec or h264dec_osfree
  G1_SRCS += $(H264_SRCS)
  DEC_SRCS += software/test/h264high/bytestream_parser.c \
              software/test/h264high/libav-wrapper.c
  LIBS += $(LIB_H264)
  DEFINES += -DENABLE_H264_SUPPORT
  SHARED_LIBS += $(LIB_H264)
  SHARED_LIB = lib_vcd_h264.so
endif
ifeq ($(findstring jpegdec,$(MAKECMDGOALS)),jpegdec)
  G1_SRCS += $(JPEG_SRCS)
  G1_SRCS += $(UT_SRCS)
  DEC_SRCS += software/test/jpeg/dectestbench.c
  DEFINES += -DENABLE_JPEG_SUPPORT
  LIBS += $(LIB_JPEG)
  SHARED_LIBS += $(LIB_JPEG)
  SHARED_LIB = lib_vcd_jpeg.so
endif
ifeq ($(USE_64BIT_ENV), y)
    export M32 = -m64
else
    export M32 = -m32
endif
ifeq ($(findstring heifdec,$(MAKECMDGOALS)),heifdec)
  G2_SRCS += ${HEVC_SRCS}
  DEFINES += -DENABLE_HEVC_SUPPORT
  LIBS += $(LIB_HEVC)
  SHARED_LIBS += $(LIB_HEVC)
#avif
  G2_SRCS += $(AV1_SRCS)
  DEFINES += -DENABLE_AV1_SUPPORT
  LIBS += $(LIB_AV1)
  SHARED_LIBS += $(LIB_AV1)
  SHARED_LIB = lib_vcd_heif.so

  HEIF_SRC := heif-wrapper.c
  HEIFLIB = libheif-wrapper.a
  HEIF_MAKE_DIR := ./software/test/heif/

  LIBS += $(HEIFLIB)

  HEIF_LIBS = -lheif_static -lheif-wrapper -lstdc++
ifeq ($(strip $(USE_ANDROID)),y)
  #HEIF_FLAGS = -L./heif -L$(HEIF_DIR)/build/release/android/x86_64/lib/
  ifeq ($(findstring x86_64-linux-android33-clang, $(CC)), x86_64-linux-android33-clang)
    HEIF_FLAGS = -L./heif -L$(HEIF_DIR)/build/release/android/x86_64/lib/
  else
    HEIF_FLAGS = -L./heif -L$(HEIF_DIR)/build/release/android/arm64-v8a/lib/
  endif
else
  HEIF_FLAGS = -L./heif -L$(HEIF_DIR)/build/release/linux/x86/lib/
  ifeq ($(strip $(USE_ARM_ENV)),y)
    HEIF_FLAGS = -L./heif -L$(HEIF_DIR)/build/release/linux/arm/lib/
  endif
endif
  C++ = $(CROSS)g++ -std=c++11
  # C -compiler name, can be replaced by another compiler(replace gcc)
  ifeq ($(strip $(RELEASE)),n)
  ifeq ($(strip $(USE_ANDROID)),y)
    DEBFLAGS = -O0 -g -Wno-unused
  else
    DEBFLAGS = -O0 -g -Werror -Wno-unused
  endif
  else
    DEBFLAGS = -O2 -Wno-unused
  endif
  # compiler switches
  ifeq ($(USE_64BIT_ENV), y)
    C++FLAGS = -Wall $(DEBFLAGS) $(HEIF_INCLUDE)
  else
    C++FLAGS = $(M32) -Wall $(DEBFLAGS) $(HEIF_INCLUDE)
  endif
  C++FLAGS += -Wsign-compare -Wmissing-field-initializers
  #show the path to compiler, where to find header files
  HEIF_INCLUDE += -I$(HEIF_DIR)/srcs/api/reader/ \
	          -I$(HEIF_DIR)/srcs/api/common
endif

ifeq ($(findstring mpeg4dec,$(MAKECMDGOALS)),mpeg4dec)
  G1_SRCS += $(MPEG4_SRCS)
  DEC_SRCS += software/test/mpeg4/mpeg4dectest.c \
              software/test/mpeg4/rm_length.c
  LIBS += $(LIB_MPEG4)
  DEFINES += -DENABLE_MPEG4_SUPPORT
  SHARED_LIBS += $(LIB_MPEG4)
  SHARED_LIB = lib_vcd_mpeg4.so
endif

ifeq ($(findstring rvdec,$(MAKECMDGOALS)),rvdec)
  G1_SRCS += $(RV_SRCS)
  DEC_SRCS += software/test/rv/hantro_rv_test.c
  LIBS += $(LIB_RV)
  DEFINES += -DENABLE_RV_SUPPORT
  SHARED_LIBS += $(LIB_RV)
  SHARED_LIB = lib_vcd_rv.so
  LIBS += librvparser.a
  LDFLAGS += -Lsoftware/test/rv/rm_parser


ifeq ($(STATIC), n)
    export SETTING_FLAGS = -fPIC
endif

librvparser.a:
	+make -C software/test/rv/rm_parser clean
	+make -C software/test/rv/rm_parser $(RV_TARGET)
endif

ifeq ($(findstring vc1dec,$(MAKECMDGOALS)),vc1dec)
  G1_SRCS += $(VC1_SRCS)
  DEC_SRCS += software/test/vc1/dectestbench.c
  LIBS += $(LIB_VC1)
  DEFINES += -DENABLE_VC1_SUPPORT
  SHARED_LIBS += $(LIB_VC1)
  SHARED_LIB = lib_vcd_vc1.so
endif

ifeq ($(findstring vp6dec,$(MAKECMDGOALS)),vp6dec)
  G1_SRCS += $(VP6_SRCS)
  DEC_SRCS += software/test/vp6/on2vp6dec.c
  LIBS += $(LIB_VP6)
  DEFINES += -DENABLE_VP6_SUPPORT
  SHARED_LIBS += $(LIB_VP6)
  SHARED_LIB = lib_vcd_vp6.so
endif

ifeq ($(findstring vp8dec,$(MAKECMDGOALS)),vp8dec)
  G1_SRCS += $(VP8_SRCS)
  DEC_SRCS += software/test/vp8/dectestbench.c \
              software/test/vp8/ivf.c \
              software/test/vp8/vp8filereader.c
              #software/test/vp8/vp8writeoutput.c \
              #software/test/vp8/vp8bufferalloc.c
  LIBS += $(LIB_VP8)
  DEFINES += -DENABLE_VP8_SUPPORT
  SHARED_LIBS += $(LIB_VP8)
  SHARED_LIB = lib_vcd_vp8.so
endif

ifeq ($(findstring ppdec,$(MAKECMDGOALS)),ppdec)
  G2_SRCS += $(PP_SRCS)
  LIBS += $(LIB_PP)
  DEFINES += -DENABLE_PPDEC_SUPPORT
  SHARED_LIBS += $(LIB_PP)
  SHARED_LIB = lib_vcd_pp.so
endif

ifeq ($(findstring hevcdec,$(MAKECMDGOALS)),hevcdec)
  G2_SRCS += $(HEVC_SRCS)
  LIBS += $(LIB_HEVC)
  DEFINES += -DENABLE_HEVC_SUPPORT
  SHARED_LIBS += $(LIB_HEVC)
  SHARED_LIB = lib_vcd_hevc.so
else
ifeq ($(findstring vp9dec,$(MAKECMDGOALS)),vp9dec)
  G2_SRCS += $(VP9_SRCS)
  LIBS += $(LIB_VP9)
  DEFINES += -DENABLE_VP9_SUPPORT
  SHARED_LIBS += $(LIB_VP9)
  SHARED_LIB = lib_vcd_vp9.so
else
ifeq ($(findstring avs2dec,$(MAKECMDGOALS)),avs2dec)
  G2_SRCS += $(AVS2_SRCS)
  LIBS += $(LIB_AVS2)
  DEFINES += -DENABLE_AVS2_SUPPORT
  SHARED_LIBS += $(LIB_AVS2)
  SHARED_LIB = lib_vcd_avs2.so
else
ifeq ($(findstring av1dec,$(MAKECMDGOALS)),av1dec)
  G2_SRCS += $(AV1_SRCS)
  LIBS += $(LIB_AV1)
  DEFINES += -DENABLE_AV1_SUPPORT
  SHARED_LIBS += $(LIB_AV1)
  SHARED_LIB = lib_vcd_av1.so
endif # av1dec
endif # avs2dec
endif # vp9dec
endif # hevcdec

# Avoid the confliction: mpeg2dec, g2dec have same suffix "g2dec"
ifeq ($(findstring mpeg2dec,$(MAKECMDGOALS)),mpeg2dec)
  G1_SRCS += $(MPEG2_SRCS)
  DEC_SRCS += software/test/mpeg2/mpeg2dectest.c
  LIBS += $(LIB_MPEG2)
  DEFINES += -DENABLE_MPEG2_SUPPORT
  SHARED_LIBS += $(LIB_MPEG2)
  SHARED_LIB = lib_vcd_mpeg2.so
else

# There are any of targets "g2dec"/"vcdec" in $(MAKECMDGOALS)
COMMON_TB_TARGETS=g2dec vcdec v4l2lib
ifneq ($(words $(COMMON_TB_TARGETS)), $(words $(filter-out $(MAKECMDGOALS),$(COMMON_TB_TARGETS))))

  SHARED_LIB = lib_vcd.so
ifeq ($(findstring v4l2lib,$(MAKECMDGOALS)),v4l2lib)
  DEFINES += -DUSE_OMXIL_BUFFER -DENABLE_FPGA_VERIFICATION
  override CFLAGS:=$(filter-out -Werror,$(CFLAGS))
  SHARED_LIB = lib_vc9000d_v4l2.so
endif
# vcdec, enable all formats.
ifeq ($(findstring vcdec,$(MAKECMDGOALS)),vcdec)
  DISABLE_VVC = n
  DISABLE_HEVC = n
  DISABLE_VP9 = n
  DISABLE_H264 = n
  DISABLE_AVS2 = n
  DISABLE_AV1 = n
  DISABLE_MPEG4 = n
  DISABLE_MPEG2 = n
  DISABLE_JPEG = n
  DISABLE_AVS = n
  DISABLE_RV = n
  DISABLE_VC1 = n
  DISABLE_VP6 = n
  DISABLE_VP8 = n
endif

ifeq ($(DISABLE_VVC),n)
  G2_SRCS += $(VVC_SRCS)
  DEFINES += -DENABLE_VVC_SUPPORT
  LIBS += $(LIB_VVC)
  SHARED_LIBS += $(LIB_VVC)
endif
ifeq ($(DISABLE_HEVC),n)
  G2_SRCS += $(HEVC_SRCS)
  DEFINES += -DENABLE_HEVC_SUPPORT
  LIBS += $(LIB_HEVC)
  SHARED_LIBS += $(LIB_HEVC)
endif
ifeq ($(DISABLE_VP9),n)
  G2_SRCS += $(VP9_SRCS)
  DEFINES += -DENABLE_VP9_SUPPORT
  LIBS += $(LIB_VP9)
  SHARED_LIBS += $(LIB_VP9)
endif
ifeq ($(DISABLE_H264),n)
  G2_SRCS += $(H264_SRCS)
  DEFINES += -DENABLE_H264_SUPPORT
  LIBS += $(LIB_H264)
  SHARED_LIBS += $(LIB_H264)
endif
ifeq ($(DISABLE_AVS2),n)
  G2_SRCS += $(AVS2_SRCS)
  DEFINES += -DENABLE_AVS2_SUPPORT
  LIBS += $(LIB_AVS2)
  SHARED_LIBS += $(LIB_AVS2)
endif
ifeq ($(DISABLE_AV1),n)
  G2_SRCS += $(AV1_SRCS)
  DEFINES += -DENABLE_AV1_SUPPORT
  LIBS += $(LIB_AV1)
  SHARED_LIBS += $(LIB_AV1)
endif
ifeq ($(DISABLE_PP),n)
  G2_SRCS += $(PP_SRCS)
  DEFINES += -DENABLE_PP_SUPPORT
  LIBS += $(LIB_PP)
  SHARED_LIBS += $(LIB_PP)
endif
ifeq ($(DISABLE_MPEG4),n)
  G2_SRCS += $(MPEG4_SRCS)
  DEFINES += -DENABLE_MPEG4_SUPPORT
  LIBS += $(LIB_MPEG4)
  SHARED_LIBS += $(LIB_MPEG4)
endif
ifeq ($(DISABLE_MPEG2),n)
  G2_SRCS += $(MPEG2_SRCS)
  DEFINES += -DENABLE_MPEG2_SUPPORT
  LIBS += $(LIB_MPEG2)
  SHARED_LIBS += $(LIB_MPEG2)
endif
ifeq ($(DISABLE_JPEG),n)
  G2_SRCS += $(JPEG_SRCS)
  DEFINES += -DENABLE_JPEG_SUPPORT
  LIBS += $(LIB_JPEG)
  SHARED_LIBS += $(LIB_JPEG)
endif
ifeq ($(DISABLE_AVS),n)
  G2_SRCS += $(AVS_SRCS)
  DEFINES += -DENABLE_AVS_SUPPORT
  LIBS += $(LIB_AVS)
  SHARED_LIBS += $(LIB_AVS)
endif
ifeq ($(DISABLE_RV),n)
  G2_SRCS += $(RV_SRCS)
  DEFINES += -DENABLE_RV_SUPPORT
  LIBS += $(LIB_RV)
  SHARED_LIBS += $(LIB_RV)
endif
ifeq ($(DISABLE_VC1),n)
  G2_SRCS += $(VC1_SRCS)
  DEFINES += -DENABLE_VC1_SUPPORT
  LIBS += $(LIB_VC1)
  SHARED_LIBS += $(LIB_VC1)
endif
ifeq ($(DISABLE_VP6),n)
  G2_SRCS += $(VP6_SRCS)
  DEFINES += -DENABLE_VP6_SUPPORT
  LIBS += $(LIB_VP6)
  SHARED_LIBS += $(LIB_VP6)
endif
ifeq ($(DISABLE_VP8),n)
  G2_SRCS += $(VP8_SRCS)
  DEFINES += -DENABLE_VP8_SUPPORT
  LIBS += $(LIB_VP8)
  SHARED_LIBS += $(LIB_VP8)
endif
ifeq ($(DISABLE_PPDEC),n)
  G2_SRCS += $(PP_SRCS)
  DEFINES += -DENABLE_PPDEC_SUPPORT
  LIBS += $(LIB_PP)
  SHARED_LIBS += $(LIB_PP)
endif

# libav stuff
ifeq ($(USE_LIBAV), y)
  PKG_CONFIG_LIBAV_DIR=$(LIBAV)
  PKG_CONFIG_LIBDIR="$(LIBAV)/lib/pkgconfig"
  LDFLAGS += -L$(LIBAV)/lib
  LIBAV_CFLAGS += -I$(LIBAV)/include
  LIBAV_LIBS = -lavformat -lavcodec -lavutil
endif
endif
endif

G2_SRCS += $(TB_COMMON)

# Default for "vcdec hevcdec vp9dec avs2dec av1dec h264dec"
DEC_TB_SRCS = software/test/common/bytestream_parser.c \
              software/test/common/error_simulator.c \
              software/test/common/vpxfilereader.c \
              software/test/common/libav-wrapper.c \
              software/test/common/command_line_parser.c

ifeq ($(findstring heifdec,$(MAKECMDGOALS)),heifdec)
  DEC_TB_SRCS += software/test/heif/dectestbench.c
else
  DEC_TB_SRCS += software/test/common/vcdectestbench.c
endif

ifeq ($(findstring ppdec,$(MAKECMDGOALS)),ppdec)
  DEC_TB_SRCS = software/test/pp/external_mode/pptestbench.c
else
ifeq ($(findstring g2dec,$(MAKECMDGOALS)),g2dec)
  DEC_TB_SRCS = software/test/common/bytestream_parser.c \
                software/test/common/error_simulator.c \
                software/test/common/vpxfilereader.c \
                software/test/common/decapi.c \
                software/test/common/dectestbench.c
endif
endif

ifeq ($(SUPPORT_SEI), y)
  DEC_TB_SRCS += software/test/common/utils/vcd_dec_hdr.c
endif

DEC_TB_SRCS += software/test/common/utils/vcd_tools.c

ifeq ($(strip $(USE_SDL)),y)
  DEC_TB_SRCS += software/test/common/sdl_sink.c
endif

ifeq ($(strip $(TILE4x4)),y)
ifeq ($(strip $(FORCE_RFC_BYPASS)),y)
    DEFINES += -DFORCE_COMPRESSOR_OVERFLOW
endif
endif

DEFINES += -DFIFO_DATATYPE=void*

ifeq ($(strip $(ENV)),FreeRTOS)
  override G2_SRCS:=$(filter-out $(AVS2_SRCS),$(G2_SRCS))
endif
SRCS += $(G1_SRCS)
SRCS += $(G2_SRCS)

G1_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(G1_SRCS:.c=.o)))
DEC_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(DEC_SRCS:.c=.o)))

G2_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(G2_SRCS:.c=.o)))
DEC_TB_OBJS := $(sort $(patsubst %,$(OBJDIR)/%,$(DEC_TB_SRCS:.c=.o)))

LIBS += $(LIB_DWL)
LIBS += $(LIB_COMMON)
SHARED_LIBS += $(LIB_DWL)
SHARED_LIBS += $(LIB_COMMON)

ifneq ($(strip $(USE_MODEL_LIB)),)
  LDFLAGS += -L$(dir $(USE_MODEL_LIB))
  MODEL_LIB += $(patsubst lib%, -l%, $(basename $(notdir $(USE_MODEL_LIB))))
ifeq ($(strip $(SUPPORT_DEC400)),y)
  MODEL_LIB += -pthread -lstdc++
endif
endif

ifneq ($(strip $(NOT_IN_X86_LINUX)),y)
ifeq ($(strip $(USE_MODEL_SIMULATION)),y)
ifeq ($(strip $(SUPPORT_DEC400)),y)
  MODEL_LIB += -pthread -lstdc++
endif
endif
endif

LIBS += $(LIB_TBCOMMON)   # for both legacy formats and advanced formats

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  FREERTOS_SRCS = $(notdir $(FREERTOS_KERNEL_C_SRCS))
  LIBS += $(LIB_FREERTOS) -lrt
  SHARED_LIBS += $(LIB_FREERTOS)
  override CFLAGS:=$(filter-out -Werror,$(CFLAGS))
endif

#for split the library building with the testbench's
ifeq ($(strip $(ENV)),FreeRTOS)
  LIBS_FORMAT += $(LIB_VVC)
  LIBS_FORMAT += $(LIB_HEVC)
  LIBS_FORMAT += $(LIB_VP9)
  LIBS_FORMAT += $(LIB_H264)
  LIBS_FORMAT += $(LIB_AV1)
  LIBS_FORMAT += $(LIB_JPEG)
  FREERTOS_SRCS = $(notdir $(FREERTOS_C_SRCS))
  LIBS += $(LIB_FREERTOS) # placed rightmost, it's a basic lib
  LIBS += $(LIB_FREERTOS_DRIVER) # driver lib
  SHARED_LIBS += $(LIB_FREERTOS)
  SHARED_LIBS += $(LIB_FREERTOS_DRIVER)

  override LIBS:=$(filter-out $(LIB_AVS2),$(LIBS))
  override SHARED_LIBS:=$(filter-out $(LIB_AVS2),$(SHARED_LIBS))

  override LDFLAGS:=$(filter-out -pthread,$(LDFLAGS))
  override CFLAGS:=$(filter-out -pedantic,$(CFLAGS))
  override CFLAGS:=$(filter-out -Werror,$(CFLAGS))
endif

ifeq ($(findstring valib,$(MAKECMDGOALS)),valib)
  COMMON_SRCS = $(VA_COMMON_SRCS)
  DWL_SRCS =
  TBCOMMON_SRCS =
  LOG_MSG=n
  DEFINES += -DSUPPORT_DEC400 -DUSE_LIBVA
  LIBS = libvacommon.a
ifeq ($(strip $(LIBVA_DEBUG)),y)
  DEFINES += -DLIBVA_DEBUG
endif
ifeq ($(strip $(VAGEN_CMODEL)),y)
  LIBS += libvc9kd.a
endif
ifeq ($(strip $(DISABLE_VP9)),n)
  LIBS += libvavp9.a
endif
endif

ifeq ($(findstring h264dec_osfree,$(MAKECMDGOALS)),h264dec_osfree)
  override DEFINES:=$(filter-out -DDWL_USE_DEC_IRQ,$(DEFINES))
endif

# special treatment for libav-wrapper.c, since it will refer to ffmpeg libs.
$(OBJDIR)/software/test/common/libav-wrapper.o: software/test/common/libav-wrapper.c
	@echo -e "[CC]\t $<"
	@test -d $(@D) || mkdir -p $(@D)
	@$(CC) -c $(CFLAGS) $(INCLUDE) $(ARCH) $(LIBAV_CFLAGS) $(DEFINES) $< -o $@

###############################################################################
# lib generation
###############################################################################

libg1hw.a: $(MODEL_LIB_DIR)/$(LIB_HW)

libg2hw.a: $(MODEL_LIB_DIR)/$(LIB_HW)

libpphw.a: $(MODEL_LIB_DIR)/$(LIB_PP_HW)

libvc9kd.a: $(MODEL_LIB_DIR)/$(LIB_VCD_HW)

libdwl.a: $(sort $(patsubst %,$(OBJDIR)/%,$(DWL_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libcommon.a: $(sort $(patsubst %,$(OBJDIR)/%,$(COMMON_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^
libvacommon.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VA_COMMON_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^
libvavp9.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VA_VP9_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecavs.a: $(sort $(patsubst %,$(OBJDIR)/%,$(AVS_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdech264.a: $(sort $(patsubst %,$(OBJDIR)/%,$(H264_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecjpeg.a: $(sort $(patsubst %,$(OBJDIR)/%,$(JPEG_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecmpeg2.a: $(sort $(patsubst %,$(OBJDIR)/%,$(MPEG2_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecmpeg4.a: $(sort $(patsubst %,$(OBJDIR)/%,$(MPEG4_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecrv.a: $(sort $(patsubst %,$(OBJDIR)/%,$(RV_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvc1.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VC1_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvp6.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VP6_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvp8.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VP8_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libtbcommon.a: $(sort $(patsubst %,$(OBJDIR)/%,$(TBCOMMON_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvvc.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VVC_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdechevc.a: $(sort $(patsubst %,$(OBJDIR)/%,$(HEVC_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecvp9.a: $(sort $(patsubst %,$(OBJDIR)/%,$(VP9_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdech10p.a: $(sort $(patsubst %,$(OBJDIR)/%,$(H264_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecavs2.a: $(sort $(patsubst %,$(OBJDIR)/%,$(AVS2_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libdecav1.a: $(sort $(patsubst %,$(OBJDIR)/%,$(AV1_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

libpp.a: $(sort $(patsubst %,$(OBJDIR)/%,$(PP_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^

ifeq ($(findstring heifdec,$(MAKECMDGOALS)),heifdec)
ifeq ($(strip $(USE_ANDROID)),y)
$(sort $(patsubst %,$(OBJDIR)/%,$(HEIF_SRC:.c=.o))): $(sort $(patsubst %,$(HEIF_MAKE_DIR)/%,$(HEIF_SRC))) 
	$(CC)++ -c $(HEIF_LIBS) $(C++FLAGS) $(ENVSET) $< -o $@
else
$(sort $(patsubst %,$(OBJDIR)/%,$(HEIF_SRC:.c=.o))): $(sort $(patsubst %,$(HEIF_MAKE_DIR)/%,$(HEIF_SRC))) 
	$(C++) -c $(HEIF_LIBS) $(C++FLAGS) $(ENVSET) $< -o $@
endif

$(HEIFLIB): $(sort $(patsubst %,$(OBJDIR)/%,$(HEIF_SRC:.c=.o)))
	@$(AR) $(OBJDIR)/$@ $^
endif

# freertos source file search paths
vpath %.c
vpath %.c  $(dir $(FREERTOS_C_SRCS))

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
libfreertos.a: $(sort $(patsubst %,$(OBJDIR)/%,$(FREERTOS_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^
endif

ifeq ($(strip $(ENV)),FreeRTOS)
libfreertos.a: $(sort $(patsubst %,$(OBJDIR)/%,$(FREERTOS_SRCS:.c=.o)))
	@echo "[AR] $(OBJDIR)/$@"
	@$(AR) $(OBJDIR)/$@ $^
libdec_freertos_driver_static.a:
	@cd software/linux/subsys_driver/freertos && ./drive_cmake.sh
	@install software/linux/subsys_driver/freertos/$@ $(OBJDIR)
endif

.phony:lib freertos_lib
lib: $(LIBS)
ifeq ($(strip $(ENV)),FreeRTOS)
freertos_lib: $(LIBS) $(LIBS_FORMAT)
endif
###############################################################################
# execuatables
###############################################################################
# Legacy formats
###############################################################################
avsdec jpegdec mpeg2dec mpeg4dec rvdec vc1dec vp6dec vp8dec: env depend lib $(LIBS) $(G1_OBJS) $(DEC_OBJS)
ifeq ($(STATIC), n)
	@echo -e "[LINK]\t$(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(SHARED_LIBS:lib%.a=%)"
	$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)
endif
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@echo -e "[LINK]\t$(LIBS:lib%.a=%)"


ifeq ($(findstring jpegdec,$(MAKECMDGOALS)),jpegdec)
ifeq ($(strip $(USE_ONL_SIM)),y)
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -shared -o $(OBJDIR)/$@.so
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP1]\t$(OBJDIR)/$@.so"
	@$(STRIP) --strip-all $(OBJDIR)/$@.so
endif
else
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_OBJS)) -Wl,--start-group $(FREERTOS_DRIVER_LIB) $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm  $(LIBAV_LIBS) -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP2]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif
endif
endif

ifneq ($(findstring jpegdec,$(MAKECMDGOALS)),jpegdec)
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -o $(OBJDIR)/$@
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif
endif

###############################################################################
# Advanced formats
###############################################################################
g2dec ppdec vcdec hevcdec vp9dec av1dec avs2dec h264dec h264dec_osfree heifdec: env depend lib $(LIBS) $(G2_OBJS) $(DEC_TB_OBJS)
ifeq ($(STATIC), n)
	@echo -e "[LINK]\t$(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(SHARED_LIBS:lib%.a=%)"
	@echo "$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)"
	@$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)
endif
	@echo -e "[LINK]\t$(OBJDIR)/$@"
	@echo -e "[LINK]\t$(LIBS:lib%.a=%)"
ifeq ($(strip $(USE_ONL_SIM)),y)
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(filter %.o, $(DEC_TB_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) -Wl,--end-group -lm $(LIBAV_LIBS) -shared -o $(OBJDIR)/$@.so
else
	$(CC) $(LDFLAGS) $(LIBAV_CFLAGS) $(HEIF_FLAGS) $(filter %.o, $(DEC_TB_OBJS)) -Wl,--start-group $(patsubst lib%.a, -l%, $(LIBS)) $(MODEL_LIB) $(L2) -Wl,--end-group -lm $(LIBAV_LIBS) $(HEIF_LIBS) -o $(OBJDIR)/$@
endif

ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t $(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif

###############################################################################
# Plugins target
###############################################################################
valib: env depend lib $(LIBS)

ifeq ($(strip $(VAGEN_CMODEL)),y)
ifeq ($(strip $(ENV)),arm_pclinux)
	@cp $(MODEL_LIB_DIR)/lib$(MODEL_NAME).a $(MODEL_LIB_DIR)/lib$(MODEL_NAME)_aarch64.a -rf 2>/dev/null || :
else
	@cp $(MODEL_LIB_DIR)/lib$(MODEL_NAME).a $(MODEL_LIB_DIR)/lib$(MODEL_NAME)_x86.a -rf 2>/dev/null || :
endif
endif
v4l2lib: env depend lib $(LIBS) $(G2_OBJS)
ifeq ($(STATIC), n)
	@echo -e "$(LIBS)"
	@echo -e "$(DISABLE_H264)"
	@echo -e "[LINK]\t$(OBJDIR)/$(SHARED_LIB)"
	@echo -e "[LINK]\t$(SHARED_LIBS:lib%.a=%)"
	@echo "$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)"
	@$(CC) $(LDFLAGS) -L$(OBJDIR) -Wl,--whole-archive $(SHARED_LIBS:lib%.a=-l%) -Wl,--no-whole-archive -fPIC -shared -lm -o $(OBJDIR)/$(SHARED_LIB)
endif

ifeq ($(strip $(USE_STRIP)),y)
ifeq ($(strip $(USE_ONL_SIM)),y)
	@echo -e "[STRIP]\t$(OBJDIR)/$@.so"
	@$(STRIP) --strip-all $(OBJDIR)/$@.so
else
ifneq ($(findstring v4l2lib,$(MAKECMDGOALS)),v4l2lib)
	@echo -e "[STRIP]\t$(OBJDIR)/$@"
	@$(STRIP) --strip-all $(OBJDIR)/$@
endif
endif
endif
