#------------------------------------------------------------------------------
#-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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

BELLAGIO_ROOT = $(shell pwd)/../../../libomxil-bellagio-0.9.3
INSTALL_DIR = $(BELLAGIO_ROOT)/build/lib/bellagio

## Set correct encoder API version
ENCODER_API_VERSION ?= vc9000e
# ENCODER_API_VERSION ?= vc8000e
#ENCODER_API_VERSION = h2v41
#ENCODER_API_VERSION = h2
#ENCODER_API_VERSION = h1v7
#ENCODER_API_VERSION = h1v6
#ENCODER_API_VERSION = h1v5
#ENCODER_API_VERSION = h1
#ENCODER_API_VERSION = 8290
#ENCODER_API_VERSION = 7280

ifeq ($(ENCODER_API_VERSION), vc9000e)
  ifeq ($(ENC_PATH),)
     $(error Please export the ENC_PATH, which include 1st subdirectory named "software")
	endif
else ifeq ($(ENCODER_API_VERSION), vc8000e)
	ifeq ($(ENC_PATH),)
     $(error Please export the ENC_PATH, which include 1st subdirectory named "software")
	endif
endif
$(info )
$(info Env info: ENCODER_API_VERSION=$(ENCODER_API_VERSION) with ENC_PATH=$(ENC_PATH))
$(info )

## Default build is for 32-bit platform
## Set this to 'y' when target is 64-bit platform
64BIT_BUILD = y

# Enable C-Model to have tracefile generated
#  Used for HW verification purpose
TRACE = n

## Enable Jpeg encoder
CODECS += -DENABLE_JPEG

## Debug trace flags
#CFLAGS += -DENABLE_DBGT_TRACE
CFLAGS += -DDBGT_CONFIG_AUTOVAR

ifeq ($(TRACE),y)
  CFLAGS += -DTEST_DATA
endif

## Use EWL for linear memory allocation
CFLAGS += -DUSE_EWL

## OMX tunneling support
#CFLAGS +=-DOMX_ENCODER_TUNNELING_SUPPORT

## Enable only for OMX IL conformance testing
#CFLAGS += -DCONFORMANCE

## valid for vc9ke
# CFLAGS += -DUSE_TEMP_INPUTBUFFER

## Enable if encoder is not able to write output stream directly to output buffer
# CFLAGS += -DUSE_TEMP_OUTPUTBUFFER

## valid for vc9ke
# CFLAGS += -DUSE_OUPUTBUFFER_NO_PA

## valid for legacy
# CFLAGS += -DUSE_INTERNAL_INOUT_BUF

## Video stabilization requires two consecutive input pictures. The first is
## encoded and the second picture goes to stabilization.
## Enable if the component has to wait for the second buffer from client
#CFLAGS += -DUSE_STAB_TEMP_INPUTBUFFER

## These flags disable dynamic calculation of OMX port buffer size.
## Default sizes are defined in codec.h
#CFLAGS += -DUSE_DEFAULT_INPUT_BUFFER_SIZE
#CFLAGS += -DUSE_DEFAULT_OUTPUT_BUFFER_SIZE

# If the machine is 64-bit and we are compiling for PC, force 32-bit compilation
ifeq ($(shell uname -m),x86_64)
	ifeq (,$(findstring pclinux,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
	ifeq (,$(findstring install,$(MAKECMDGOALS)))
		export M32 = -m32
	endif
endif

ifeq ($(ENCODER_API_VERSION), vc9000e)
	ENCODER_BASE_PATH = $(ENC_PATH)
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
	VCE_API_VER = 6
else ifeq ($(ENCODER_API_VERSION), vc8000e)
	ENCODER_BASE_PATH = $(ENC_PATH)
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
## VCE_API_VER definition
#    1: available @ CL221157
#    2: available @ CL244132
#    3: available @ CL307325
#    4: available @ CL335915
#    5: available @ CL380680
#    6: available @ CL423105
	VCE_API_VER = 5
else ifeq ($(ENCODER_API_VERSION), h2v41)
	ENCODER_BASE_PATH = ../../../../../H2/H2V4.1_H264
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
else
	ENCODER_BASE_PATH = ../../../../../H1/develop
	ENCODER_RELEASE  = $(ENCODER_BASE_PATH)
	MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
endif

INCDIR = software/inc
CUR_PATH = $(shell pwd)
CFLAGS += -I.\
					-I$(CUR_PATH)/.. \
					-I$(CUR_PATH)/../../headers \
					-I$(ENCODER_RELEASE)/$(INCDIR) \
					-I$(MEMALLOC_RELEASE)

base_SRCS = ../msgque.c ../OSAL.c ../basecomp.c ../port.c ../util.c encoder_perf.c

libhantrovideoenc_SRCS = encoder_constructor_video.c
libhantroimageenc_SRCS = encoder_constructor_image.c

ifeq ($(ENCODER_API_VERSION), vc9000e)
	ENCODER_DUT_C_PATH = vpu_vc9000e
else
	ENCODER_DUT_C_PATH = legacy
	CFLAGS += -Ilegacy
endif

CFLAGS += $(BUILD_OPTION)

ifeq ($(ENCODER_API_VERSION), vc9000e)
	VPU_PATH = vpu_vc9000e
	ENCODER_VERSION = h2
	CFLAGS += -Ivpu_vc9000e
	CFLAGS += -DENCVC9000E -DVCE_API_VER=$(VCE_API_VER)
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_video.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_jpeg.c
	IMAGELIB = libOMX.hantro.H2.image.encoder.so

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.
else ifeq ($(ENCODER_API_VERSION), vc8000e)
	VPU_PATH = legacy/vpu_vc8000e
	ENCODER_VERSION = h2
	CFLAGS += -Ilegacy/vpu_vc8000e
	CFLAGS += -DENCVC8000E -DVCE_API_VER=$(VCE_API_VER)
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_video.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_jpeg.c
	IMAGELIB = libOMX.hantro.H2.image.encoder.so

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h2v41)
	VPU_PATH = legacy/vpu_h2
	ENCODER_VERSION = h2
	CFLAGS += -Ilegacy/vpu_h2
	CFLAGS += -I$(ENCODER_RELEASE)/software/source/common
	CFLAGS += -I$(ENCODER_RELEASE)/software/source/hevc
	CFLAGS += -DENCH2V41
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_h2v41.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h2)
	VPU_PATH = legacy/vpu_h2
	ENCODER_VERSION = h2
	CFLAGS += -Ilegacy/vpu_h2
	CFLAGS += -DENCH2
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_hevc.c
	VIDEOLIB = libOMX.hantro.H2.video.encoder.so
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1v7)
	VPU_PATH = legacy/vpu_h1
	ENCODER_VERSION = h1
	CFLAGS += -Ilegacy/vpu_h1
	CFLAGS += -DENCH1
	CFLAGS += -DH1V6
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_vp8.c $(VPU_PATH)/encoder_h264.c
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1v6)
	VPU_PATH = legacy/vpu_h1
	ENCODER_VERSION = h1
	CFLAGS += -Ilegacy/vpu_h1
	CFLAGS += -DENCH1
	CFLAGS += -DH1V6
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_vp8.c $(VPU_PATH)/encoder_h264.c
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1v5)
	VPU_PATH = legacy/vpu_h1
	ENCODER_VERSION = h1
	CFLAGS += -Ilegacy/vpu_h1
	CFLAGS += -DENCH1
	CFLAGS += -DH1V5
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_vp8.c $(VPU_PATH)/encoder_h264.c
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), h1)
	VPU_PATH = legacy/vpu_h1
	ENCODER_VERSION = h1
	CFLAGS += -Ilegacy/vpu_h1
	CFLAGS += -DENCH1
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_vp8.c $(VPU_PATH)/encoder_h264.c
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_webp.c
	VIDEOLIB = libOMX.hantro.H1.video.encoder.so
	IMAGELIB = libOMX.hantro.H1.image.encoder.so

	ifneq (,$(findstring -DENABLE_JPEG, $(CODECS)))
		libhantroimageenc_SRCS += encoder_jpeg.c
	endif

	CFLAGS += $(CODECS)
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), 8290)
	VPU_PATH = legacy/vpu_8290
	ENCODER_VERSION = 8290
	CFLAGS += -Ilegacy/vpu_8290
	CFLAGS += -DENC8290
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_h264.c
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_jpeg.c
	VIDEOLIB = libOMX.hantro.8290.video.encoder.so
	IMAGELIB = libOMX.hantro.8290.image.encoder.so
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.

else ifeq ($(ENCODER_API_VERSION), 7280)
	VPU_PATH = legacy/vpu_7280
	ENCODER_VERSION = 7280
	CFLAGS += -Ilegacy/vpu_7280
	CFLAGS += -DENC7280
	libhantrovideoenc_SRCS += $(VPU_PATH)/encoder_mpeg4.c $(VPU_PATH)/encoder_h263.c $(VPU_PATH)/encoder_h264.c
	libhantroimageenc_SRCS += $(VPU_PATH)/encoder_jpeg.c
	VIDEOLIB = libOMX.hantro.7280.video.encoder.so
	IMAGELIB = libOMX.hantro.7280.image.encoder.so
	CFLAGS += -I$(BELLAGIO_ROOT)/src  # To avoid queue.h pathing wrongly.
endif

ifeq ($(64BIT_BUILD), y)
export M32 = -m64
CFLAGS += -DADDRESS_WIDTH_64
endif

libhantrovideoenc_OBJS = $(base_SRCS:.c=.o) $(libhantrovideoenc_SRCS:.c=.o) encoder_video.o library_entry_point_video.o
libhantroimageenc_OBJS = $(base_SRCS:.c=.o) $(libhantroimageenc_SRCS:.c=.o) encoder_image.o library_entry_point_image.o

ENCODER_SW = $(ENCODER_RELEASE)/software/linux_reference/lib$(ENCODER_VERSION)enc.a
ENCODER_SYS = $(ENCODER_RELEASE)/system/models/enc$(ENCODER_VERSION)_asic_model.a

all:
	@echo ""
	@echo "MAKE for OpenMAX IL Encoder Component"
	@echo ""
	@echo " make [target] ENCODER_API_VERSION=[verion] [variables]"
	@echo ""
	@echo " Available targets:"
	@echo ""
	@echo "  pclinux          build image AND video encoder omxil component for HW model testing"
	@echo "  pci              build image AND video encoder omxil component for x86_linux pcie platform"
	@echo "  arm              build image AND video encoder omxil component for ARM platform"
	@echo "  install          installs omxil component with Bellagio"
	@echo "  clean            deletes generated output"
	@echo ""
	@echo " Available versions:"
	@echo ""
	@echo "  vc9000e {default}"
	@echo "  vc8000e"
	@echo "  h2v41"
	@echo "  h2"
	@echo "  h1v7"
	@echo "  h1v6"
	@echo "  h1v5"
	@echo "  h1"
	@echo "  8290"
	@echo "  7280"
	@echo ""
	@echo " Available variables:"
	@echo ""
	@echo "  64BIT_BUILD = [y/n] -- enables or disables 64-bit compiling"
	@echo ""
	@echo " Current settings:"
	@echo ""
	@echo "  Encoder API Version:           $(ENCODER_API_VERSION)"
	@echo "  Encoder Directory:             $(ENCODER_RELEASE)"
	@echo "  Bellagio Root:                 $(BELLAGIO_ROOT)"
	@echo "  Install Dir:                   $(INSTALL_DIR)"
	@echo "  64BIT_BUILD:                   $(strip $(64BIT_BUILD))"
	@echo ""

.PHONY: pclinux
pclinux: CC = gcc
pclinux: CFLAGS += $(M32) -g -O0 -finline-functions -W -Wno-inline -Wall -fPIC -Wunused-variable -DNDEBUG -pthread
pclinux: LDFLAGS = $(M32)
pclinux: HANTRO_LIBS = $(ENCODER_SW) $(ENCODER_SYS)
pclinux: videolib imagelib

.PHONY: pci
pci: CC = gcc
pci: CFLAGS += $(M32) -g -O0 -finline-functions -W -Wno-inline -Wall -fPIC -Wunused-variable -DNDEBUG -pthread
pci: LDFLAGS = $(M32)
pci: HANTRO_LIBS = $(ENCODER_SW)
pci: videolib imagelib

.PHONY: arm
arm: CC = aarch64-none-linux-gnu-gcc
arm: CFLAGS += -O2  -fpic
arm: LDFLAGS =
arm: HANTRO_LIBS = $(ENCODER_SW)
arm: videolib imagelib
ifeq ($(ENCODER_API_VERSION), h2)
install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)

else ifeq ($(ENCODER_API_VERSION), h2v41)

install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)

else

install:
	cp -vf $(VIDEOLIB) $(INSTALL_DIR)/
	cp -vf $(IMAGELIB) $(INSTALL_DIR)/
	$(BELLAGIO_ROOT)/build/bin/omxregister-bellagio $(INSTALL_DIR)/

clean:
	rm -f $(libhantrovideoenc_OBJS) $(VIDEOLIB) $(INSTALL_DIR)/$(VIDEOLIB)
	rm -f $(libhantroimageenc_OBJS) $(IMAGELIB) $(INSTALL_DIR)/$(IMAGELIB)

endif

videolib: $(libhantrovideoenc_OBJS)
	echo "[L] $@"
	$(CC) $(LDFLAGS) -o $(VIDEOLIB) -Wl,-h -Wl,$(VIDEOLIB) -shared -Wl,--start-group $(libhantrovideoenc_OBJS) $(HANTRO_LIBS) -lrt -Wl,--end-group -pthread -lm

imagelib: $(libhantroimageenc_OBJS)
	echo "[L] $@"
	$(CC) $(LDFLAGS) -o $(IMAGELIB) -Wl,-h -Wl,$(IMAGELIB) -shared -Wl,--start-group $(libhantroimageenc_OBJS) $(HANTRO_LIBS) -lrt -Wl,--end-group -pthread -lm


encoder_video.o: $(ENCODER_DUT_C_PATH)/encoder.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_VIDEO_DOMAIN -c $< -o $@

encoder_image.o: $(ENCODER_DUT_C_PATH)/encoder.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_IMAGE_DOMAIN -c $< -o $@

library_entry_point_video.o: library_entry_point.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_VIDEO_DOMAIN -c $< -o $@

library_entry_point_image.o: library_entry_point.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -DOMX_ENCODER_IMAGE_DOMAIN -c $< -o $@

%.o : %.c
	echo "[C] $<"
	$(CC) $(CFLAGS) -c $< -o $@
