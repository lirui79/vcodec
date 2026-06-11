#------------------------------------------------------------------------------
#-       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
#-         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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

#
# Abstract : Makefile containing common settings and rules used to compile
#            Hantro VC9000D decoder software.

################################################################################
## External macros & variables

# Compiling environment setting, x86_linux by default
ENV ?= x86_linux

# Set this to 'y' to enable static link library
STATIC ?= n

# Set this to 'y' to enable release build
RELEASE ?= n

# Set this to 'y' to enable coverage information
USE_COVERAGE ?= n

# Set this to 'y' to enable gprof profiling information
USE_PROFILING ?= n

# Use prebuilt cmodel library to generate binary for cmodel simulation.
# Set full path to the cmodel libary file (including the libaray file). E.g.,
#   USE_MODEL_LIB=/path/to/libvc9dk.a
# If not set, build system model lib from source files when USE_MODEL_SIMULATION=y.
USE_MODEL_LIB ?=

# Disable SW support for given format from SDK.
# All formats (HEVC/VP9/H264/AVS2/AV1) are supported by default
DISABLE_VVC ?= y
DISABLE_HEVC ?= n
DISABLE_VP9 ?= n
DISABLE_H264 ?= n
DISABLE_AVS2 ?= n
DISABLE_AV1 ?= n

# Enable or disable 64-bit compiling
# The default value depends on OS bit-width
OSWIDTH = $(shell getconf LONG_BIT)
ifeq ($(strip $(OSWIDTH)), 64)
  USE_64BIT_ENV ?= y
else
  USE_64BIT_ENV ?= n
endif

# Set this to 'y' to choose HW minimum supported resolution as
# decoding picture size limitation
USE_HW_PIC_DIMENSIONS ?= n

# Set this to 'y' for enabling IRQ mode for the decoder. You will need
# the vc9000d kernel driver loaded and a /dev/hantrodec device node created
USE_DEC_IRQ ?= n

# Set this 'y' for enabling sw reg tracing. NOTE! not all sw reagisters are
# traced; only hw "return" values.
USE_INTERNAL_TEST ?= n

# Set this to 'y' to enable asic traces only when using cmodel simulation
USE_ASIC_TRACE ?= n

# Set this to 'y' to enable jpeg asic traces for debug
JPEGDEC_ASIC_TRACE ?= n

# Set this to 'y' to enable freertos sim mode
USE_FREERTOS_SIMULATOR ?= n
FREERTOS_DIR ?= software/../../../FreeRTOS/

# Set this to 'y' to enable webm support, needs nestegg lib as demuxer
WEBM_ENABLED ?= n
NESTEGG ?= $(HOME)/nestegg

# Set this to 'y' to enable MMU support
USE_MMU ?= n

# Set this to 'y' to enable VCMD, if not support VCMD, please USE_VCMD=n when build binary
# But For CMODEL test bench not use vcmd as default, please add --use-vcmd if wanna use
USE_VCMD ?= y

# Set this to 'y' to enable DMA
USE_DMA ?= n

# Set this to 'y' to enable VCMD_M2M
USE_M2M ?= n

# Set this to 'y' to enable suffix search function
USE_SUFFIX_SEARCH ?= y

# Set this to 'y' to enable SDL support, needs sdl lib
USE_SDL ?= n
SDL_CFLAGS ?= $(shell sdl-config --cflags)
SDL_LDFLAGS ?= $(shell sdl-config --libs)

# Set this to 'y' to clear SPS/PPS parameters after abort decoder
CLEAR_HDRINFO_IN_SEEK ?= n

# Set this to 'y' to discard repeated pictures from decoder
USE_PICTURE_DISCARD ?= n

# Set this to 'y' to enable random error test
USE_RANDOM_ERROR_TEST ?= n

# Set this to 'y' to enable HW/SW performance test
USE_PERFORMANCE_TEST ?= n

# Set this to 'y' to enable vdk virtual platform test
USE_VIRTUAL_PLATFORM_TEST ?= n

# Set this to 'y' to dump input streams to file
USE_DUMP_INPUT_STREAM ?= n

# Enable or disable non-blocking of decoder when get free output buffer
USE_NON_BLOCKING ?= y

# Enable or disable non-blocking of decoder when output decoded buffer
USE_ONE_THREAD_WAIT ?= y

# Set this to 'y' to enable supporting middle ware framework
USE_OMXIL_BUFFER ?= n

#Set this 'y' to indicate use separated thread to handle output
MULTI_THREAD_OUTPUT ?= n

# Set thie to 'y' to enable random latency mode
USE_RANDOM_LATENCY ?= n

SEEK_TEST ?= n

# Set this to 'y' to enable supporting END CMD
USE_END_CMD ?= n
# To be removed
USE_SW_PERFORMANCE ?= n
DISABLE_PIC_FREEZE_FLAG ?= y
USE_CUSTOM_FMT ?= n

# To be refined
RESOLUTION_1080P ?= n

# Force RFC output of each CBS is word-aligned.
USE_RFC_WORD_ALIGN ?= y

# Use lanczos filter size in default.
USE_FLEXIBLE_FILTER ?= y

# Set path to ffmpeg libary (excluding directory "lib")
# E.g., if libavformat.a is located in /path/to/libav/lib/libavformat.a, then set
# LIBAV=path/to/ffmpeg
# If set, use libav to as demuxer
LIBAV ?=

#Set path to heif-matsr libary
HEIF_DIR ?= ./heif/heif-master

# trace all register accesses & output dwl debug info
SW_DEBUG ?= n

# parse SEI info ?
SUPPORT_SEI ?= n

# support GDR default, it will support sei automatically ?
SUPPORT_GDR ?= y

# error case just return in SW.
USE_ERROR_RETURN ?= y

################################################################################
# Internal macros & Variables

# Set this to 'y' to enable multicore support, needs libav lib
# Only set it when generating multicore simulation trace files.
USE_MULTI_CORE ?= n

# Set this to 'y' to enable on-line sim mode
USE_ONL_SIM ?= n

# Randomly corrupt RFC buffer/table for HW robustness testing
USE_RFC_CORRUPT ?= n

# stack and heap size statistic
USE_STACK_HEAP_STAT ?= n

# n-207D, y-701
USE_PPU_V9_2_3 ?= n
ifeq ($(USE_PPU_V9_2_3), y)
  DEFINES += -DPPU_V9_2_3
	SYS_DIR_BASE ?= system_v9_2_3/
# Where is system model lib saved to?
	MODEL_LIB_DIR ?= system_v9_2_3/models
else
  DEFINES += -DPPU_V9_2_1_2
	SYS_DIR_BASE ?= system
# Where is system model lib saved to?
	MODEL_LIB_DIR ?= system/models
endif

# Default: system/models
LDFLAGS += -L$(MODEL_LIB_DIR)

USE_ANDROID ?= n

#complie binary in arm env
USE_ARM_ENV ?= n

# Obsolete
SUPPORT_MMU ?= n
# Set this to 'y' to use 48pa MMU
SUPPORT_48PA_MMU ?= n
# Set this to 'y' to enable DEC400, 207D HW need to set 'y', 701 Cmodel can set 'y' for internal test
SUPPORT_DEC400 ?= n
# used to DEC400 header info
DEFINES += -DTS_HEADER_BUFFER

#not complie for x86 linux, only used to Cmodel for DEC400
NOT_IN_X86_LINUX ?= n

# Set this to 'y' to extract jpeg thumbnail
EXTRCT_THUMBNAIL ?= n

# Set this to 'y' to enable dma test
DMA_TRANS_TEST ?= n

# Set this to 'y' to enable parity generation in trace
USE_FUSA ?= n
ifeq ($(USE_FUSA), y)
  DEFINES += -DSUPPORT_FUSA
endif

ifeq ($(SUPPORT_DEC400), y)
  DEFINES += -DSUPPORT_DEC400
endif

# Enable random exit() test
RANDOM_EXIT_TEST ?= n
ifeq ($(RANDOM_EXIT_TEST), y)
  DEFINES += -DRANDOM_EXIT_TEST
endif

ifeq ($(USE_STACK_HEAP_STAT), y)
  DEFINES += -DSTACK_STAT
  DEFINES += -D_DWL_PERFORMANCE
endif

# Enable full formats
USE_FULL_FORMATS ?= n

ifeq ($(USE_MULTI_CORE), y)
  DEFINES += -DSUPPORT_MULTI_CORE
  CORES = 2
endif

#disable cmodel bandwidth statistics
SUPPORT_BW_STAT ?= n
ifeq ($(strip $(SUPPORT_BW_STAT)), y)
    DEFINES += -DCMODEL_BW_STAT
endif
# disable L2cache
USE_L2CACHE ?= n
L2 ?=
ifeq ($(strip $(USE_L2CACHE)), y)
    DEFINES += -DSUPPORT_L2CACHE
    INCLUDE += -Isystem/models/l2cache/interface/
    L2 = -lstdc++
else
    #INCLUDE += -Ithird_party/l2cache/interface/
    #LIBS    += third_party/l2cache/lib/libl2cache.a
    #LDFLAGS += -Lthird_party/l2cache/lib
    #L2      += -ll2cache -lstdc++ -lm
endif

ifneq ($(strip $(TILE4x4)),y)
    DEFINES += -DTILE_8x8
else ifeq ($(strip $(FORCE_RFC_BYPASS)),y)
    DEFINES += -DFORCE_COMPRESSOR_OVERFLOW
endif


ifeq ($(USE_FULL_FORMATS), y)
  DISABLE_HEVC ?= n
  DISABLE_H264 ?= n
  DISABLE_VP9 ?= n
  DISABLE_AV1 ?= n
  DISABLE_AVS2 ?= n
  DISABLE_AVS ?= n
  DISABLE_JPEG ?= n
  DISABLE_MPEG2 ?= n
  DISABLE_MPEG4 ?= n
  DISABLE_RV ?= n
  DISABLE_VC1 ?= n
  DISABLE_VP6 ?= n
  DISABLE_VP8 ?= n
  DISABLE_PP ?= n
endif

# 2 cores supported by default
CORES ?= 2
DEFINES += -DCORES=2

DEFINES += -DFIFO_DATATYPE=void*

# Case information printing
# if yes, print case information and save in caseprint.txt. If no, don't print case information
CASE_INFO_STAT ?= n
ifeq ($(strip $(CASE_INFO_STAT)),y)
  DEFINES += -DCASE_INFO_STAT
endif

# use for afbc verification
USE_FBC_CORE_INPUT ?= n
ifeq ($(strip $(USE_FBC_CORE_INPUT)),y)
  DEFINES += -DUSE_FBC_CORE_INPUT
endif

DEFINES += -DUSE_FAKE_RFC_TABLE
DEFINES += -DFAKE_RFC_TBL_LITTLE_ENDIAN

# SOFTWARE VERSION
VCDSW_VERSION_MAJOR=2
VCDSW_VERSION_MINOR=4
VCDSW_VERSION_MICRO=83
VCDSWVERSION = $(VCDSW_VERSION_MAJOR).$(VCDSW_VERSION_MINOR).$(VCDSW_VERSION_MICRO)

export VCDSWVERSION VCDSW_VERSION_MAJOR VCDSW_VERSION_MINOR VCDSW_VERSION_MICRO

################################################################################
ifeq ($(strip $(ENV)),x86_linux)
  ARCH ?=
  CROSS ?=
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  ifeq ($(strip $(USE_64BIT_ENV)),n)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  CFLAGS += -fpic
  USE_MODEL_SIMULATION ?= y
  ifeq ($(strip $(USE_OMXIL_BUFFER)),y)
    NOT_IN_X86_LINUX = y
  endif
endif

ifeq ($(strip $(ENV)),FreeRTOS)
  ARCH ?=
  #CROSS ?= aarch64-linux-gnu-
  CROSS ?=
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  CFLAGS += -fpic
  DISABLE_AVS2 = y
  USE_MODEL_SIMULATION ?= n
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/subsys_driver/ \
             -Isoftware/linux/subsys_driver/freertos
  DEFINES += -D__FREERTOS__ -D_ASSERT_USED
  include software/linux/dwl/osal/freertos/freertos.mk
  INCLUDE += $(FREERTOS_INCLUDE)
  NOT_IN_X86_LINUX = y
endif

ifeq ($(strip $(ENV)),arm_linux)
  ARCH ?=
  #CROSS ?= arm-none-linux-gnueabi-
  CROSS ?= aarch64-linux-gnu-
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  CFLAGS += -fpic
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/subsys_driver/
  USE_MODEL_SIMULATION ?= n
  USE_ARM_ENV = y
  NOT_IN_X86_LINUX = y
endif

ifeq ($(strip $(ENV)),arm32_linux)
  ARCH ?=
  CROSS ?= arm-none-linux-gnueabihf-
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  CFLAGS += -fpic
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/subsys_driver/
  USE_MODEL_SIMULATION ?= n
  USE_ARM_ENV = y
  NOT_IN_X86_LINUX = y
endif

ifeq ($(strip $(ENV)),arm64_linux)
  ARCH ?=
  CROSS ?= aarch64-none-linux-gnu-
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  CFLAGS += -fpic
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/subsys_driver/
  USE_MODEL_SIMULATION ?= n
  USE_ARM_ENV = y
  NOT_IN_X86_LINUX = y
endif

ifeq ($(strip $(ENV)),arm_pclinux)
  ARCH ?=
  CROSS ?= aarch64-linux-gnu-
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  ifeq ($(strip $(USE_64BIT_ENV)),n)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  CFLAGS += -fpic
  USE_MODEL_SIMULATION ?= y
  USE_ARM_ENV = y
  NOT_IN_X86_LINUX = y
endif

ifeq ($(strip $(ENV)),x86_linux_pci)
  ARCH ?=
  CROSS ?=
  AR  = $(CROSS)ar rcs
  CC  = $(CROSS)gcc
  STRIP = $(CROSS)strip
  ifeq ($(strip $(USE_64BIT_ENV)),n)
    CFLAGS += -m32
    LDFLAGS += -m32
  endif
  INCLUDE += -Isoftware/linux/memalloc \
             -Isoftware/linux/subsys_driver
  USE_MODEL_SIMULATION ?= n
  NOT_IN_X86_LINUX = y
  DEFINES += -DPC_PCI_FPGA_DEMO
endif

ifneq (,$(findstring h264dec_osfree,$(MAKECMDGOALS)))
  DEFINES += -DNON_PTHREAD_H
endif

ifeq ($(strip $(USE_64BIT_ENV)),y)
  DEFINES += -DUSE_64BIT_ENV
else
  NOT_IN_X86_LINUX = y
endif

ifeq ($(strip $(NOT_IN_X86_LINUX)),y)
  DEFINES += -DNOT_IN_X86_LINUX
endif

ifeq ($(strip $(RESOLUTION_1080P)),y)
  DEFINES += -DRESOLUTION_1080P
endif

ifeq ($(USE_HW_PIC_DIMENSIONS), y)
  DEFINES += -DHW_PIC_DIMENSIONS
endif

# Common error flags for all targets
CFLAGS  += -Wall -ansi -std=c99 -pedantic

# DWL uses variadic macros for debug prints
CFLAGS += -Wno-variadic-macros

# Common libraries
LDFLAGS += -L$(OBJDIR) -pthread

# MACRO for cleaning object -files
RM  = rm -f

# LogMsg
LOG_MSG ?= y
ifeq ($(LOG_MSG), y)
  DEFINES += -DVCD_LOGMSG
endif

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  RELEASE = y # TODO Alarm clock when debug, next to check
  DEFINES += -D__FREERTOS__ -DFREERTOS_SIMULATOR
endif

# Flags for >2GB file support.
DEFINES += -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -D_LARGEFILE64_SOURCE

ifeq ($(strip $(USE_ANDROID)),y)
  DEFINES += -DANDROID=1
  DEFINES += -DNOT_IN_X86_LINUX
endif

ifeq ($(strip $(USE_SDL)),y)
  DEFINES += -DSDL_ENABLED
  CFLAGS += $(SDL_CFLAGS)
  LDFLAGS += $(SDL_LDFLAGS)
  LIBS += -lSDL -ldl -lrt -lm
endif
# pvric ldl
USE_PVRIC ?= n
ifeq ($(strip $(USE_PVRIC)),y)
  DEFINES += -DPVRIC_ENABLE
  CFLAGS += -ldl
  LIBS += -ldl
endif

ifeq ($(strip $(RELEASE)),n)
  CFLAGS += -g -O0
  ifeq ($(strip $(USE_ANDROID)),n)
  ifeq (,$(findstring osfree, $(MAKECMDGOALS)))
    CFLAGS += -Werror
  endif
  endif
  DEFINES += -DDEBUG -D_ASSERT_USED -D_RANGE_CHECK -D_ERROR_PRINT
  BUILDCONFIG = debug
  USE_STRIP ?= n
else
  CFLAGS   += -O2
  DEFINES += -DNDEBUG -D_ERROR_PRINT
  BUILDCONFIG = release
  USE_STRIP ?= y
endif

# Directory where object and library files are placed.
BUILDROOT=out
OBJDIR=$(strip $(BUILDROOT))/$(strip $(ENV))/$(strip $(BUILDCONFIG))
ifeq ($(USE_MODEL_SIMULATION), y)
  DEFINES += -DMODEL_SIMULATION
else
  override USE_ASIC_TRACE = n
endif

ifeq ($(USE_ASIC_TRACE), y)
  DEFINES += -DASIC_TRACE_SUPPORT
  DEFINES += -DWRITE_TRACES
endif

ifeq ($(JPEGDEC_ASIC_TRACE), y)
  DEFINES += -DJPEGDEC_ASIC_TRACE
  DEFINES += -DJPEGDEC_TRACE
endif

ifeq ($(USE_ONL_SIM),y)
  DEFINES += -DASIC_ONL_SIM
  ifeq ($(PP_REORDER_CHECK),y)
    DEFINES += -DPP_REORDER_CHECK
  endif

  ifeq ($(PP_CH_CHECK),y)
    DEFINES += -DPP_CH_CHECK
  endif

  ifeq ($(VRFD_CHECK),y)
    DEFINES += -DVRFD_CHECK
  endif

  ifeq ($(PP_FAST_MODE),y)
    DEFINES += -DPP_FAST_MODE
  endif
endif

ifeq ($(USE_COVERAGE), y)
  CFLAGS += -coverage -fprofile-arcs -ftest-coverage
  LDFLAGS += -coverage
endif

ifeq ($(USE_PROFILING), y)
  CFLAGS += -pg
  LDFLAGS += -pg
endif

ifeq ($(findstring heifdec,$(MAKECMDGOALS)),heifdec)
  DEFINES += -DSUPPORT_HEIF
  DEFINES += -DUSE_OMXIL_BUFFER
  INCLUDE += -I./heif
  INCLUDE += -I$(HEIF_DIR)/srcs/api/reader
  INCLUDE += -I$(HEIF_DIR)/srcs/api/common
endif

ifeq ($(STATIC), y)
  LDFLAGS += -static
else
  CFLAGS += -fPIC
endif

ifeq ($(FPGA_REAL_INT), y)
  DEFINES +=  -DFPGA_REAL_INT
endif

ifeq ($(USE_DEC_IRQ), y)
  DEFINES +=  -DDWL_USE_DEC_IRQ
endif

ifeq ($(CLEAR_HDRINFO_IN_SEEK), y)
  DEFINES += -DCLEAR_HDRINFO_IN_SEEK
endif

ifeq ($(USE_PICTURE_DISCARD), y)
  DEFINES += -DUSE_PICTURE_DISCARD
endif

ifeq ($(USE_RANDOM_ERROR_TEST), y)
  DEFINES += -DUSE_RANDOM_ERROR_TEST
endif

ifeq ($(USE_PERFORMANCE_TEST), y)
  DEFINES += -DPERFORMANCE_TEST
endif

ifeq ($(USE_VIRTUAL_PLATFORM_TEST), y)
  DEFINES += -DVIRTUAL_PLATFORM_TEST
endif

ifeq ($(VDK_STRICT_TEST), y)
  DEFINES += -DVDK_STRICT_TEST
endif

ifeq ($(USE_DUMP_INPUT_STREAM), y)
  DEFINES += -DUSE_DUMP_INPUT_STREAM
endif

ifeq ($(USE_OMXIL_BUFFER), y)
  DEFINES += -DUSE_OMXIL_BUFFER
endif

ifeq ($(MULTI_THREAD_OUTPUT), y)
  DEFINES += -DMULTI_THREAD_OUTPUT
endif

ifeq ($(USE_FLEXIBLE_FILTER), y)
  DEFINES += -DSUPPORT_FLEXIBLE_FILTER
endif

ifeq ($(USE_RFC_CORRUPT),y)
  DEFINES += -DRANDOM_CORRUPT_RFC
endif

# If decoder can not get free buffer, return DEC_NO_DECODING_BUFFER.
ifeq ($(USE_NON_BLOCKING), y)
  DEFINES += -DGET_FREE_BUFFER_NON_BLOCK
endif

ifeq ($(USE_ONE_THREAD_WAIT), y)
  DEFINES += -DGET_OUTPUT_BUFFER_NON_BLOCK
endif

ifneq ($(DEF_HWBUILD_ID),)
  DEFINES += -DDEC_HW_BUILD_ID=0x$(DEF_HWBUILD_ID)
endif

ifeq ($(strip $(USE_VCMD)),y)
  DEFINES += -DSUPPORT_VCMD
  DEFINES += -DDWL_USE_DEC_IRQ
endif

ifeq ($(strip $(USE_RFC_WORD_ALIGN)),y)
  DEFINES += -DRFC_WORD_ALIGN
endif

# disable pp: only to clear the recon(decoder picture buffer)
# enable pp: only to clear the pp output buffer
# As you see the name, not define it for the real application,
# only make the decode result of hw match with the gloden data from CModel for the error case,
# and compress result, like the rfc(reference frame compress), dec400 compress data...
# because test bench will output the yuv[rgb, compress data] to the file based on the frame size.
ifeq ($(strip $(ENABLE_FPGA_VERIFICATION)),y)
  DEFINES += -DENABLE_FPGA_VERIFICATION
  ifeq (,$(findstring osfree, $(MAKECMDGOALS)))
    DEFINES += -DEXT_BUF_SAFE_RELEASE
  endif
endif

# Memest pp buffer to 0x80 to match with JM
ifeq ($(strip $(ENABLE_JM_VERIFICATION)),y)
  DEFINES += -DENABLE_JM_VERIFICATION
endif

# Ignore frame gap error
DEFINES += -DH264_IGNORE_FRAME_GAP

# Update data elements from repeat sequence header and repeat sequence extension header
# This is not allowed by the mpeg2 specification
DEFINES += -DENABLE_NON_STANDARD_FEATURES

ifeq ($(strip $(FPGA_PERF_AND_BW)),y)
  DEFINES += -DFPGA_PERF_AND_BW
endif

DEFINES += -DRV_RAW_STREAM_SUPPORT

#make clean && make -j g2dec SEEK_TEST=y
ifeq ($(SEEK_TEST), y)
  DEFINES += -DSEEK_TEST
  DEFINES += -DUSE_OMXIL_BUFFER
  DEFINES += -DCLEAR_HDRINFO_IN_SEEK
endif

ifeq ($(USE_SW_PERFORMANCE), y)
  DEFINES += -DSW_PERFORMANCE
endif

ifeq ($(DISABLE_PIC_FREEZE_FLAG), y)
  DEFINES += -D_DISABLE_PIC_FREEZE
endif
ifeq ($(WEBM_ENABLED), y)
  DEFINES += -DWEBM_ENABLED
  INCLUDE += -I$(NESTEGG)/include
  LIBS    += $(NESTEGG)/lib/libnestegg.a
endif


ifeq ($(SUPPORT_MMU), y)
  DEFINES += -DSUPPORT_MMU
  ifeq ($(SUPPORT_48PA_MMU), y)
    DEFINES += -DSUPPORT_48PA_MMU
  endif
else ifeq ($(USE_MMU), y)
  DEFINES += -DSUPPORT_MMU
  ifeq ($(SUPPORT_48PA_MMU), y)
    DEFINES += -DSUPPORT_48PA_MMU
  endif
endif

# mostly, system will manage the access right of ddr for the different buffer from DWL layer interface named "DWLMallocLinear",
# thus some buffer can't be accessed by CPU, now can define it.
# and achieve the interface DWLDMATransData according to the own transfer engine, like DMA
ifeq ($(SUPPORT_DMA), y)
  DEFINES += -DSUPPORT_DMA
else ifeq ($(USE_DMA), y)
  DEFINES += -DSUPPORT_DMA
endif

ifeq ($(DMA_TRANS_TEST), y)
  DEFINES += -DDMA_TRANS_TEST
  # DEFINES += -DENABLE_FPGA_VERIFICATION
endif

ifeq ($(SUPPORT_M2M), y)
  DEFINES += -DSUPPORT_VCMD_M2M
else ifeq ($(USE_M2M), y)
  DEFINES += -DSUPPORT_VCMD_M2M
endif

ifeq ($(SUPPORT_GDR), y)
  SUPPORT_SEI = y
  DEFINES += -DSUPPORT_GDR
  DEFINES += -DUSE_FAKE_RFC_TABLE
  DEFINES += -DREORDER_ERROR_FIX
else ifeq ($(USE_GDR), y)
  SUPPORT_SEI = y
  DEFINES += -DSUPPORT_GDR
  DEFINES += -DUSE_FAKE_RFC_TABLE
  DEFINES += -DREORDER_ERROR_FIX
endif

ifeq ($(strip $(SUPPORT_SEI)), y)
  DEFINES += -DSUPPORT_SEI
  DEFINES += -DSUPPORT_METADATA
endif

ifeq ($(SUPPORT_SUFFIX_SEARCH), y)
  DEFINES += -DSUPPORT_SUFFIX_SEARCH
else ifeq ($(USE_SUFFIX_SEARCH), y)
  DEFINES += -DSUPPORT_SUFFIX_SEARCH
endif

ifeq ($(SUPPORT_ERROR_RETURN), y)
  DEFINES += -DSUPPORT_ERROR_RETURN
else ifeq ($(USE_ERROR_RETURN), y)
  DEFINES += -DSUPPORT_ERROR_RETURN
endif

ifeq ($(ENABLE_2ND_CHROMA_FLAG), y)
  ENABLE_2ND_CHROMA=-D_ENABLE_2ND_CHROMA
  CFLAGS += $(ENABLE_2ND_CHROMA)
endif

# USE THIS ONLY FOR DEBUGGING PURPOSES
ifneq ($(SET_EMPTY_PICTURE_DATA),)
  DEBFLAGS += -DSET_EMPTY_PICTURE_DATA=$(SET_EMPTY_PICTURE_DATA)
endif

ifneq ($(USE_DEC_IRQ), )
  ifeq ($(USE_DEC_IRQ), y)
    DEBFLAGS += -DDEC_X170_USING_IRQ=1
  else
    DEBFLAGS += -DDEC_X170_USING_IRQ=0
  endif
endif

# Enalbe libav when LIBAV is set in command line.
ifeq ($(strip $(LIBAV)),)
  USE_LIBAV = n
else
  USE_LIBAV = y
  DEFINES += -DUSE_LIBAV
endif

ifeq ($(SUPPORT_RANDOM_LATENCY), y)
  DEFINES += -DSUPPORT_RANDOM_LATENCY
else ifeq ($(USE_RANDOM_LATENCY), y)
  DEFINES += -DSUPPORT_RANDOM_LATENCY
endif

ifeq ($(SW_DEBUG), y)
  #DEFINES += -DDWL_DISABLE_REG_PRINTS # Do not trace all register accesses
  DEFINES += -D_DWL_DEBUG            # DWL debug info
else
  DEFINES += -DDWL_DISABLE_REG_PRINTS # Do not trace all register accesses
  #DEFINES += -D_DWL_DEBUG            # DWL debug info
endif

ifeq ($(USE_END_CMD), y)
  DEFINES += -DUSE_END_CMD
endif

# disable missing field initializer from causing error since it is interpreted
# wrongly by some older GCC versions.
#CFLAGS += -Wno-missing-field-initializers

ifdef DWL_PRESET_FAILING_ALLOC
  DEFINES += -DDWL_PRESET_FAILING_ALLOC=$(DWL_PRESET_FAILING_ALLOC)
endif

ifeq ($(EXTRACT_THUMBNAIL), y)
  DEFINES += -DEXTRACT_THUMBNAIL_JPG
endif

#set these for evaluation

DEFINES += -D_GNU_SOURCE
DEFINES += -DSKIP_OPENB_FRAME
DEFINES += -DENABLE_DPB_RECOVER
#DEFINES += -DREORDER_ERROR_FIX
# DEFINES += -DMEMSET_FAKE_REF_BUFFER

DEFINES += ${BUILD_OPTION}
