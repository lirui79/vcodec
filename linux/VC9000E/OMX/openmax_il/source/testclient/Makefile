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

#vcd/vce
BIN ?=
#vc9000e/vc8000e/vcnanoe
ENCODER_API_VERSION ?= vc9000e
#vc9000d/vc8000d/vcnanod
DECODER_API_VERSION ?= vc9000d

ifneq ($(findstring $(BIN), vcd),)
	include ../decoder/set_dec_env.mk
endif
BELLAGIO_ROOT = $(shell pwd)/../../../libomxil-bellagio-0.9.3
INSTALL_DIR = $(BELLAGIO_ROOT)/build/bin

## Default build is for 32-bit platform
## Set this to 'y' when target is 64-bit platform
64BIT_BUILD = y

LOG_MSG ?= y

ENABLE_RV ?= n

ifeq ("$(LOG_MSG)", "y")
	CFLAGS += -DVCD_LOGMSG
endif

ifneq ($(findstring $(BIN), vcd),)
## Set this to 'n' or 'y' in order to disable/enable realvideo support. G1/vc8000d/vc9000d only
	ifeq ("$(RV)", "y")
		ENABLE_RV = y
	endif
endif

# If the machine is 64-bit and we are compiling for PC, force 32-bit compilation
ifeq ($(shell uname -m),x86_64)
#	ifneq (,$(findstring arm,$(MAKECMDGOALS)))
	export M32 = -m32
#	endif
endif

ifeq ($(64BIT_BUILD), y)
	export M32 = -m64
# CFLAGS += -DADDRESS_WIDTH_64
endif

ifeq ("$(H264)", "y")
	CFLAGS += -DENABLE_CODEC_H264
endif
ifeq ("$(HEVC)", "y")
	CFLAGS += -DENABLE_CODEC_HEVC
endif
ifeq ("$(VP9)", "y")
	CFLAGS += -DENABLE_CODEC_VP9
	ifeq ("$(WEBM_ENABLED)", "y")
		CFLAGS += -DWEBM_ENABLED
	endif
endif
ifeq ("$(AV1)", "y")
	CFLAGS += -DENABLE_CODEC_AV1
endif
ifeq ("$(AVS2)", "y")
	CFLAGS += -DENABLE_CODEC_AVS2
endif
ifeq ("$(VVC)", "y")
	CFLAGS += -DENABLE_CODEC_VVC
endif
ifeq ("$(JPEG)", "y")
	CFLAGS += -DENABLE_CODEC_MJPEG
	CFLAGS += -DENABLE_CODEC_JPEG
endif
ifeq ("$(MPEG4)", "y")
	CFLAGS += -DENABLE_CODEC_MPEG4
	CFLAGS += -DENABLE_CODEC_H263
	CFLAGS += -DENABLE_CODEC_DIVX
endif
ifeq ("$(MPEG2)", "y")
	CFLAGS += -DENABLE_CODEC_MPEG2
endif
ifeq ("$(VC1)", "y")
	CFLAGS += -DENABLE_CODEC_VC1
endif
ifeq ("$(RV)", "y")
	CFLAGS += -DENABLE_CODEC_RV
endif
ifeq ("$(AVS)", "y")
	CFLAGS += -DENABLE_CODEC_AVS
endif
ifeq ("$(VP8)", "y")
	CFLAGS += -DENABLE_CODEC_VP8
	CFLAGS += -DENABLE_CODEC_WEBP
endif
ifeq ("$(VP6)", "y")
	CFLAGS += -DENABLE_CODEC_VP6
endif

CFLAGS += -g -fno-inline -W -Wall -pthread -Wall
CFLAGS += -I. \
	-I.. \
	-I../../headers

base_SRCS = ../msgque.c ../OSAL.c ../basecomp.c ../port.c ../util.c
base_OBJS = $(base_SRCS:.c=.o)

# encoder
ifneq ($(findstring $(BIN), vce),)
	ifneq ($(findstring $(ENCODER_API_VERSION), vc9000e vc8000e),)
		ifeq ($(ENC_PATH),)
      $(error Please export the ENC_PATH, which include 1st subdirectory named "software")
		endif
	else ifeq ($(ENCODER_API_VERSION), vcnanoe)
		ifeq ($(ENC_PATH),)
      $(error Please export the ENC_PATH, //TODO)
		endif
	endif
  $(info )
  $(info Enc env info: ENCODER_API_VERSION=$(ENCODER_API_VERSION) with ENC_PATH=$(ENC_PATH))

	ifeq ($(ENCODER_API_VERSION), vc9000e)
		ENCODER_BASE_PATH = $(ENC_PATH)
		MEMALLOC_RELEASE = $(ENC_PATH)/software/linux_reference/memalloc
		CFLAGS += -DENCVC9000E
		VCE_API_VER = 6

		CFLAGS += -DVCE_API_VER=$(VCE_API_VER)
	else ifeq ($(ENCODER_API_VERSION), vc8000e)
		ENCODER_BASE_PATH = $(ENC_PATH)
		MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
		CFLAGS += -DENCVC8000E
		VCE_API_VER = 3

		CFLAGS += -DVCE_API_VER=$(VCE_API_VER)
	else ifeq ($(ENCODER_API_VERSION), h2v41)
		ENCODER_BASE_PATH = ../../../../../H2/H2V4.1_H264
		MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
		CFLAGS += -DENCH2V41
	else
		ENCODER_BASE_PATH = ../../../../../H1/develop
		MEMALLOC_RELEASE = $(ENCODER_BASE_PATH)/software/linux_reference/memalloc
		CFLAGS += -DENCH1
	endif

	CFLAGS += -I../encoder/ \
						-I"$(MEMALLOC_RELEASE)"

	omxenc_HDRS += omxencparameters.h omxtestcommon.h
	omxenc_SRCS += omxencparameters.c omxenctest.c omxtestcommon.c
endif
omxenc_OBJS = $(base_OBJS) $(omxenc_SRCS:.c=.o)

# decoder
ifneq ($(findstring $(BIN), vcd),)
	ifneq ($(findstring $(DECODER_API_VERSION), vc9000d vc8000d),)
		ifeq ($(DEC_PATH),)
      $(error Please export the DEC_PATH, which include 1st subdirectory named "software")
		endif
	else ifeq ($(DECODER_API_VERSION), vcnaond)
		ifeq ($(DEC_PATH),)
      $(error Please export the DEC_PATH, //TODO")
		endif
	endif
  $(info )
  $(info Dec env info: DECODER_API_VERSION=$(DECODER_API_VERSION) with DEC_PATH=$(DEC_PATH))

	# G1_DEC_DIR ?= $(DEC_PATH)
	DECODER_BASE_PATH ?= $(DEC_PATH)

	CFLAGS +=	-I../decoder/ \
						-I"$(DECODER_BASE_PATH)/software/source/inc" \
						-I"$(DECODER_BASE_PATH)/software/source/config" \
						-I"$(DECODER_BASE_PATH)/software/test/vp9" \
						-I"$(DECODER_BASE_PATH)/software/test/common" \
						-I"$(DECODER_BASE_PATH)/software/source/common" \
						-I"$(DECODER_BASE_PATH)/software/test/common/swhw" \
						-I"$(DECODER_BASE_PATH)/system/models"
	DECODER_CFLAGS = -DDECODER
	omxdec_HDRS += omxtestcommon.h \
				$(DECODER_BASE_PATH)/software/source/common/commonconfig.h \
				$(DECODER_BASE_PATH)/software/source/common/ppu.h

	ifeq ($(ENABLE_RV), y)
		DEC_RV_CFLAGS = -DENABLE_CODEC_RV
		CFLAGS += -I"$(DECODER_BASE_PATH)/software/test/rv/rm_parser/include" \
							-I"$(DECODER_BASE_PATH)/software/test/rv/rm_parser/common/pub" \
							-I"$(DECODER_BASE_PATH)/software/source/rv"
		omxdec_SRCS += rv_ff_read.c \
									$(DECODER_BASE_PATH)/software/test/rv/rm_parser/rm_parser/rm_parser.c
		RV_LIBS = $(DECODER_BASE_PATH)/software/test/rv/rm_parser/librvparser.a
	endif
	ifeq ("$(VP9)", "y")
		omxdec_SRCS += $(DECODER_BASE_PATH)/software/test/common/vpxfilereader.c
		ifeq ("$(WEBM_ENABLED)", "y")
      $(info NESTEGG path: $(NESTEGG))
			CFLAGS += -I"$(NESTEGG)/include"
			WEBM_LIBS = $(NESTEGG)/lib/libnestegg.a
		endif
	endif
	omxdec_SRCS += omxdectest.c \
								omxtestcommon.c \
								file_reader.c \
								$(DECODER_BASE_PATH)/software/test/common/bytestream_parser.c \
								$(DECODER_BASE_PATH)/software/test/common/command_line_parser.c \
								$(DECODER_BASE_PATH)/software/source/common/dec_log.c \
								$(DECODER_BASE_PATH)/software/test/common/swhw/tb_params.c \
								$(DECODER_BASE_PATH)/software/test/common/swhw/tb_cfg.c
	TB_LIBS += $(DECODER_BASE_PATH)/out/*linux*/*/libtbcommon.a
endif
omxdec_OBJS = $(base_OBJS) $(omxdec_SRCS:.c=.o)

all:
	@echo ""
	@echo "MAKE for OpenMAX IL Encoder/Decoder Component Test Client"
	@echo "  make [target] BIN=[vce/vcd] ENCODER_API_VERSION=[version] [variables]"
	@echo "  make [target] DECODER_API_VERSION=[version] [variables]"
	@echo ""
	@echo "Available targets:"
	@echo "  pclinux          build image AND video decoder & encoder omxil test client for HW model testing"
	@echo "  arm              build image AND video decoder & encoder omxil test client for ARM platform"
	@echo "  x86_linux_pci    build image AND video decoder omxil component for x86_linux pcie platform"
	@echo "  install          installs omxil component with Bellagio"
	@echo "  clean            deletes generated output"
	@echo ""
	@echo "Available encoder api versions:"
	@echo "  vc9000e (default)"
	@echo "  vc8000e"
	@echo "  vcnanoe"
	@echo "  h2v41"
	@echo "  h2"
	@echo "  h1v7"
	@echo "  h1v6"
	@echo "  h1v5"
	@echo "  h1"
	@echo "  8290"
	@echo "  7280"
	@echo ""
	@echo "Available decoder api versions:"
	@echo "  vc9000d (default)"
	@echo "  vc8000d"
	@echo "  vcnanod"
	@echo ""
	@echo "Available variables:"
	@echo "  64BIT_BUILD = [y/n] -- enables or disables 64-bit compiling"
	@echo ""
	@echo "Current settings:"
ifeq ($(BIN), vce)
	@echo "  Compile BIN:                vce"
	@echo "    Encoder API Version:      $(ENCODER_API_VERSION)"
	@echo "    Encoder Directory:        $(ENCODER_BASE_PATH)"
else ifeq ($(BIN), vcd)
	@echo "  Compile BIN:                $(BIN)"
	@echo "    Decoder API Version:      $(DECODER_API_VERSION)"
	@echo "    Decoder Directory:        $(DECODER_BASE_PATH)"
endif
	@echo "  Bellagio Root:              $(BELLAGIO_ROOT)"
	@echo "  Install Dir:                $(INSTALL_DIR)"
	@echo "  64BIT_BUILD:                $(strip $(64BIT_BUILD))"
	@echo ""

.PHONY: pclinux
pclinux: CC = gcc
pclinux: CFLAGS += $(M32)
pclinux: BELLAGIO_LIB = $(BELLAGIO_ROOT)/build/lib/libomxil-bellagio.a
ifeq ($(BIN), vce)
pclinux: omxenctest
else ifeq ($(BIN), vcd)
pclinux: omxdectest
endif

.PHONY: arm
arm: CC = aarch64-none-linux-gnu-gcc
arm: CFLAGS += 
arm: BELLAGIO_LIB = $(BELLAGIO_ROOT)/build/lib/libomxil-bellagio.a
ifeq ($(BIN), vce)
arm: omxenctest
else ifeq ($(BIN), vcd)
arm: omxdectest
endif

.PHONY: x86_linux_pci
x86_linux_pci: CC = gcc
x86_linux_pci: CFLAGS += -fpic
x86_linux_pci: BELLAGIO_LIB = $(BELLAGIO_ROOT)/build/lib/libomxil-bellagio.a
ifeq ($(BIN), vce)
x86_linux_pci: omxenctest
else ifeq ($(BIN), vcd)
x86_linux_pci: omxdectest
endif

clean:
	rm -f $(omxenc_OBJS) $(omxdec_OBJS) *.o omxdectest omxenctest
	rm -f $(INSTALL_DIR)/omxenctest $(INSTALL_DIR)/omxdectest

ifeq ($(BIN), vce)
install: omxenctest
	cp -vf omxenctest $(INSTALL_DIR)
else ifeq ($(BIN), vcd)
install: omxdectest
	cp -vf omxdectest $(INSTALL_DIR)
endif

ifneq ($(findstring $(BIN), vcd),)
omxdectest: $(omxdec_OBJS)
	$(CC) -o omxdectest  $(TB_LIBS) $(omxdec_OBJS) $(BELLAGIO_LIB) $(RV_LIBS) $(WEBM_LIBS) -ldl -lpthread -lm $(M32)
file_reader.o : file_reader.c
	$(CC) $(CFLAGS)  $(DECODER_CFLAGS) $(DEC_RV_CFLAGS) -c $< -o $@
endif

ifneq ($(findstring $(BIN), vce),)
omxenctest: $(omxenc_OBJS)
	$(CC) -o omxenctest $(omxenc_OBJS) $(BELLAGIO_LIB) -ldl -lpthread $(M32)
endif

omxtestcommon.o : omxtestcommon.c
	$(CC) $(CFLAGS)  $(DECODER_CFLAGS) $(DEC_RV_CFLAGS) $(HWMODEL_CFLAGS) -c $< -o $@

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@