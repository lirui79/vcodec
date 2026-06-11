#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Verisilicon.                                    --
#-                                                                            --
#-                   (C) COPYRIGHT 2015 VERISILICON.                          --
#-                            ALL RIGHTS RESERVED                             --
#-                                                                            --
#-                 The entire notice above must be reproduced                 --
#-                  on all copies and should not be removed.                  --
#-                                                                            --
#-------------------------------------------------------------------------------
#-
#--  Abstract : Makefile for encoder library
#--
#-------------------------------------------------------------------------------



HEVC ?= y
H264 ?= y
AV1 ?= n
VP9 ?= n
RCSRC ?= n

include ../build/globaldefs
include ../build/globalrules

cmbase = ../..
CMBASE = $(shell cd $(cmbase); pwd )

ifeq (pci,$(findstring pci, $(MAKECMDGOALS)))
# this is just for DEMO
include Baseaddress
endif

ifeq ($(LIBVA), y)
    VCMD_BUILD_SUPPORT=y
    CFLAGS += -DSUPPORT_DEC400 \
    		-DUSE_LIBVA
    POLLING_ISR=n
ifeq (pci,$(findstring pci, $(MAKECMDGOALS)))
    SUPPORT_AXIFE=y
    CFLAGS += -DSUPPORT_AXIFE
endif
endif

# System model library
MODELLIB = ../../$(SYSTEM)/models/ench2_asic_model.a

# RC path must be added before source path
ifeq ($(RCSRC),y)
INCLUDE = -I../../system/models/video/common/
endif

# show the path to compiler, where to find header files
INCLUDE += -I../inc -I../source/common -Idebug_trace

ifeq ($(INCLUDE_VIDEO),y)
INCLUDE += -I../source/hevc -I../source/av1 -I../source/vp9
endif

ifeq ($(INCLUDE_JPEG),y)
INCLUDE += -I../source/jpeg
endif

ifeq ($(INCLUDE_VS),y)
INCLUDE += -I../source/camstab
endif

# avoid reporting -Werror=misleading-indentation when code-obfuscator
ifeq ($(RCSRC),n)
CFLAGS += -Wno-misleading-indentation
endif

ifeq ($(RCP_FIXED_POINT_OPT),y)
  CFLAGS += -DRCP_FIXED_POINT_OPT
endif

ifeq ($(strip $(USE_SAFESTRING)),y)
  CFLAGS += -DSAFESTRING
  INCLUDE += -I../inc/safestring
endif

ifeq (pci,$(findstring pci, $(MAKECMDGOALS)))
  INCLUDE += -Ikernel_module/linux -Imemalloc
else
ifeq (freertos, $(findstring freertos, $(MAKECMDGOALS)))
  INCLUDE += -Ikernel_module/freertos -Ikernel_module/linux
ifeq ($(SUPPORT_UFBC),y)
INCLUDE += -Iewl
endif
else
ifeq (freertos_lib, $(findstring freertos_lib, $(MAKECMDGOALS)))
  INCLUDE += -Ikernel_module/freertos -Ikernel_module/linux
else
ifeq (armlinux, $(findstring armlinux, $(MAKECMDGOALS)))
  INCLUDE += -Ikernel_module/linux -Imemalloc/
else
  #INCLUDE += -Ikernel_module -Imemalloc
  INCLUDE += -Imemalloc -Ikernel_module/linux
endif
endif
endif
endif

FREERTOS_DIR = ewl/../../../../../FreeRTOS
ifeq (freertos, $(findstring freertos, $(MAKECMDGOALS)))
  include ewl/osal/freertos/freertos.mk
  INCLUDE += $(FREERTOS_INCLUDE)
  CFLAGS += -DEWLHWCFGFILE=\"hwcfg/ewl_hwcfg$(FEATURES).c\"
  override CFLAGS:=$(filter-out -pedantic,$(CFLAGS))
  override CFLAGS:=$(filter-out -Werror,$(CFLAGS))
endif
ifeq (freertos_lib, $(findstring freertos_lib, $(MAKECMDGOALS)))
  include ewl/osal/freertos/freertos.mk
  INCLUDE += $(FREERTOS_INCLUDE)
  CFLAGS += -DEWLHWCFGFILE=\"hwcfg/ewl_hwcfg$(FEATURES).c\"
  override CFLAGS:=$(filter-out -pedantic,$(CFLAGS))
  override CFLAGS:=$(filter-out -Werror,$(CFLAGS))
endif
FREERTOS_KERNEL_DIR = $(FREERTOS_DIR)/FreeRTOS_Kernel/Source
FREERTOS_POSIX_SOURCE_DIR = $(FREERTOS_DIR)/lib/FreeRTOS-Plus-POSIX/source

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  INCLUDE += -I$(FREERTOS_DIR)/FreeRTOS_Kernel/Source/include \
             -I$(FREERTOS_DIR)/osal/portable/GCC/Posix \
             -Ikernel_module/freertos
  include ewl/osal/freertos/freertos.mk
endif

# list of used sourcefiles
SRC_ENC_COMMON := encasiccontroller_v2.c \
                  encasiccontroller.c \
                  queue.c \
                  hash.c \
                  tools.c \
                  encpreprocess.c \
                  apbfilter.c \
		              pool.c

ifeq ($(INCLUDE_VIDEO), y)
SRC_ENC_COMMON += sw_put_bits.c
endif

ifeq ($(SUPPORT_AXIFE),y)
SRC_ENC_COMMON += axife.c
endif

ifeq ($(CHECKSUM_CRC_BUILD_SUPPORT),y)
SRC_ENC_COMMON += checksum.c
SRC_ENC_COMMON += crc.c
endif

ifeq ($(LOGMSG),y)
SRC_ENC_COMMON += enc_log.c
endif

SRC_ENC_COMMON += encdec400.c
SRC_ENC_COMMON += encufbc.c

ifeq ($(VCMD_BUILD_SUPPORT),y)
SRC_ENC_COMMON += vcmdbuf.c
endif

ifeq ($(LOW_LATENCY_BUILD_SUPPORT),y)
SRC_ENC_COMMON += encinputlinebuffer.c
endif

ifeq ($(ERROR_BUILD_SUPPORT),y)
SRC_ENC_COMMON += error.c
endif

ifeq ($(FIFO_BUILD_SUPPORT),y)
SRC_ENC_COMMON += fifo.c
endif

ifeq ($(INCLUDE_VIDEO), y)
ifeq ($(RATE_CONTROL_BUILD_SUPPORT),y)
RC_CLEAN_PATH = ../../system/models/video/common/rate_control_picture.o ../source/hevc/rate_control_picture.o
ifeq ($(RCSRC),y)
CFLAGS += -DRCSRC_INCLUDE
SRC_ENC_COMMON += ../../system/models/video/common/rate_control_picture.c
else
SRC_ENC_COMMON += rate_control_picture.c
endif
endif
endif


SRC_HEVC := hevcencapi.c \
            hevcencapi_utils.c \
            vcenc_aif.c \
            sw_picture.c \
            sw_parameter_set.c \
            sw_slice.c\
            sw_skip_frame.c \
            sw_nal_unit.c

ifeq ($(INCLUDE_VIDEO), y)
ifeq ($(CUTREE_BUILD_SUPPORT),y)
SRC_ENC_COMMON += cutree_stream_ctrl.c
SRC_ENC_COMMON += cutreeasiccontroller.c
SRC_ENC_COMMON += sw_cu_tree.c
endif

ifeq ($(SUPPORT_CACHE),y)
SRC_ENC_COMMON += hevcenccache.c
endif

ifeq ($(SEI_BUILD_SUPPORT),y)
SRC_ENC_COMMON += hevcSei.c
endif

ifeq ($(CUINFO_BUILD_SUPPORT),y)
SRC_ENC_COMMON += vcenc_cuinfo.c
endif

ifeq ($(ROI_BUILD_SUPPORT),y)
SRC_ENC_COMMON += vcenc_fillroimap.c
endif

ifeq ($(STREAM_CTRL_BUILD_SUPPORT),y)
SRC_ENC_COMMON += vcenc_stream_ctrl.c
endif

ifeq ($(HDR_HELPER_SUPPORT),y)
# for hdr10 or dolby vision
SRC_ENC_COMMON += vcenc_hdr_info.c
endif

endif


ifeq ($(VMAF_SUPPORT),y)
SRC_HEVC += vmaf.c
VMAF_MODEL = vmaf_v0.6.1
CFLAGS += -DVMAF_MODEL=$(VMAF_MODEL)
../source/common/vmaf.pkl.h: ../lib/$(VMAF_MODEL).pkl
	xxd -i $< $@
../source/common/vmaf.pkl.model.h: ../lib/$(VMAF_MODEL).pkl.model
	xxd -i $< $@
# generate headers before dependency check
depend .depend: ../source/common/vmaf.pkl.h ../source/common/vmaf.pkl.model.h
endif

SRC_VP9  := 	vp9encapi.c \
		vp9_bitstream.c \
		vp9_subexponential.c \
		vp9_entropymode.c \
		vp9_entropy_mv.c

SRC_AV1 := av1encapi.c \
           av1_prob_init.c \
           av1enc_metadata.c


SRC_JPEG := EncJpeg.c\
            EncJpegInit.c\
            EncJpegCodeFrame.c\
            EncJpegPutBits.c\
            JpegEncApi.c \
            MjpegEncApi.c

ifeq ($(RATE_CONTROL_BUILD_SUPPORT),y)
SRC_JPEG += rate_control_jpeg.c
endif

SRC_VS   := vidstabalg.c\
            vidstabapi.c\
            vidstabcommon.c\
            vidstabinternal.c


SRC_TRACE = enctrace.c

#test_data.c

# Source files for test case specific test IDs, compiler flag INTERNAL_TEST
# If these are not included some tests will fail

ifeq ($(INCLUDE_VIDEO), y)

ifeq ($(INTERNAL_TEST),y)
SRC_TESTING = sw_test_id.c
endif

endif

SRC_EWL_PC := ewl_vcne_file.c
SRC_EWL_ARM = ewl.c
SRC_EWL_FreeRTOS = ewl.c


#ifeq ($(VCMD_BUILD_SUPPORT),y)
#    SRC_EWL_ARM += cwl_vcx_vcmd_common.c
#    SRC_EWL_FreeRTOS += cwl_vcx_vcmd_common.c
#endif

ifeq ($(LINUX_LOCK_BUILD_SUPPORT),y)
    SRC_EWL_ARM += ewl_linux_lock.c
endif

ifeq ($(SUPPORT_MEM_SYNC),y)
    SRC_EWL_ARM += ewl_memsync.c
    SRC_EWL_FreeRTOS += ewl_memsync.c
endif

ifeq ($(POLLING_ISR),y)
    CFLAGS += -DPOLLING_ISR
    CFLAGS += -DENCH2_IRQ_DISABLE=1
endif
SRC_EWL_SYS := ewl_vcmd_system.c\
               vwl.c
SRC_EWL_SYS += ewl_system.c
SRC_EWL_SYS += ewl_cmodel.c
SRC_EWL_SYS += ewl_memsync_sys.c

ifeq ($(LIBVA), y)
    SRC_EWL_ARM = ./ewl/drm/ewl_drm_hantro.c
endif

# encswhwregisters.c
SRCS = ewl_common.c buffer_info.c

# common parts only for encoder and not for video stab
INCLUDE_ENC_COMMON=y


# Combine the lists of all the source files included in the library build
ifeq ($(INCLUDE_HEVC),y)
    SRCS += $(SRC_HEVC)
endif
ifeq ($(INCLUDE_AV1),y)
    SRCS += $(SRC_AV1)
endif
ifeq ($(INCLUDE_VP9),y)
    SRCS += $(SRC_VP9)
endif
ifeq ($(INCLUDE_JPEG),y)
    SRCS += $(SRC_JPEG)
endif
ifeq ($(INCLUDE_VS),y)
    SRCS += $(SRC_VS)
    CFLAGS += -DVIDEOSTAB_ENABLED
endif
ifeq ($(INTERNAL_TEST),y)
    SRCS += $(SRC_TESTING)
endif

# add common encoder files
ifeq ($(INCLUDE_ENC_COMMON),y)
    SRCS += $(SRC_ENC_COMMON)
endif

ifeq ($(MULTICORE_SUPPORT),y)
    SRCS += ewl_multicore.c
endif

# if tracing flags are defined we need to compile the tracing functions
ifeq ($(TRACE),y)
  ifeq ($(INCLUDE_VIDEO),y)
    SRCS += $(SRC_TRACE)
  endif
endif

ifneq ($(target),)
  ifneq ($(target),default)
    CFLAGS += -DTARGET -D$(target)
  endif
endif

CFLAGS += -DEWLHWCFGFILE=\"hwcfg/ewl_hwcfg$(FEATURES).c\"

# choose EWL source, system model uses its own EWL
ifneq (,$(findstring pclinux, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl -Iewl/system
    EWL_DIR = system
else
ifneq (,$(findstring system, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl -Iewl/system
    EWL_DIR = system
else
ifneq (,$(findstring testdata, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl -Iewl/system
    EWL_DIR = system
else
ifneq (,$(findstring eval, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_SYS)
    INCLUDE += -I../../system/models/inc -Iewl -Iewl/system
    EWL_DIR = system
else
ifeq (freertos_lib, $(findstring freertos_lib, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_FreeRTOS)
    INCLUDE += -Iewl
    EWL_DIR =
else
ifeq (freertos, $(findstring freertos, $(MAKECMDGOALS)))
    SRCS += $(SRC_EWL_FreeRTOS)
    INCLUDE += -Iewl
    EWL_DIR =
else
    SRCS += $(SRC_EWL_ARM)
    INCLUDE += -Iewl
    EWL_DIR =
endif
endif
endif
endif
endif
endif

#source search path
vpath %.c
vpath %.c ../source/common ../source/hevc ../source/vp9 ../source/av1 ../source/jpeg ../source/camstab ./ewl/$(EWL_DIR) ./ewl/ ./debug_trace

# name of the outputfile (library)
ENCFREERTOS_LIB = libfreertos.a
FREERTOS_OBJS = $(FREERTOS_C_SRCS:.c=.o)
FREERTOS_KERNEL_OBJS = $(FREERTOS_KERNEL_C_SRCS:.c=.o)

ENCLIB = libh2enc.a
ENCSLIB = libh2enc.so
ifeq ($(LIBVA), y)
ENCSLIB =
ENCLIB = ../lib/libh2enc.a
endif

#Here are rules for building codes and generating object library.
all:    tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | testdata | versatile | integrator | android]"
	@echo "system        - PC system model (== pclinux)"
	@echo "testdata      - PC system model for test data creation"
	@echo "eval          - PC system model for evaluation with frame limit"
	@echo "versatile     - ARM versatile with FPGA HW"
	@echo "integrator    - ARM integrator with FPGA HW"
	@echo "android       - ARM android"
	@echo "armlinux      - ARM aarch64 linux with FPGA HW"
	@echo "arm32_linux   - ARM32 linux with QEMU"
	@echo "arm64_linux   - ARM aarch64 linux with QEMU"
	@echo "riscv32_linux - RISC-V 32 linux with QEMU"
	@echo "riscv64_linux - RISC-V 64 linux with QEMU"
	@echo "NOTE! Make sure to do 'make clean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

.PHONY: pclinux system_lib system testdata integrator versatile android freertos_lib freertos clean tags depend armlinux

evaluation: eval
eval: system

pclinux: testdata
system_static: testdata
system: testdata

# for libva
system_lib: system

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
testdata: .depend $(ENCLIB) $(ENCFREERTOS_LIB)
else
testdata: .depend $(ENCLIB)
endif

integrator: $(ENCLIB)
versatile: $(ENCLIB)

arm32_linux: $(ENCLIB)
arm64_linux: $(ENCLIB)
riscv32_linux: $(ENCLIB)
riscv64_linux: $(ENCLIB)

android: $(ENCLIB)

hwlinux: $(ENCLIB)

versatile_tb: versatile
	$(MAKE) -w -C test/vp9 versatile
	$(MAKE) -w -C test/hevc versatile



pcdemo: $(SRC_HEVC:.c=.o) $(SRC_ENC_COMMON:.c=.o)
	$(AR) $(ENCLIB) $+


.PHONY: pci
pci:
pci: $(ENCLIB) $(ENCSLIB)

.PHONY: armlinux
armlinux:
armlinux: $(ENCLIB) #$(ENCSLIB)

freertos: freertos_lib
freertos_lib: $(ENCFREERTOS_LIB) $(ENCLIB)
	cd ./kernel_module/freertos && ./drive_cmake.sh

.PHONY: version
version:
	@../scripts/get_configcode.sh $(CFLAGS) > vce_build_config.log
	@../../system/tools/p4/p4_gen_ver.sh vce_build_config.log

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
$(ENCFREERTOS_LIB): $(FREERTOS_KERNEL_OBJS)
	@echo "[AR] $@"
	@$(AR) $@ $^
else
$(ENCFREERTOS_LIB): $(FREERTOS_OBJS)
	@echo "[AR] $@"
	@$(AR) $@ $^
endif

system_cov: $(ENCLIB)
system_cov: CC = covc --retain -t!debug_trace/enctrace.c,!debug_trace/enctracestream.c g++

$(ENCLIB): $(OBJS)
	@echo "[AR] $@"
	$(AR) $(ENCLIB) $(OBJS)

ifeq (freertos_lib, $(findstring freertos_lib, $(MAKECMDGOALS)))
$(ENCSLIB): $(OBJS)
	$(CC) -fPIC -Wl,-no-undefined -shared $(CFLAGS) $(OBJS) -lm -L. -lfreertos -o libh2enc.so
else ifeq ($(SUPPORT_CACHE),y)
$(ENCSLIB): $(OBJS)
	$(CC) -fPIC -Wl,-no-undefined -shared $(CFLAGS) $(OBJS) -lm -lpthread -L$(CMBASE)/cache_software/linux_reference/ -lcache -o libh2enc.so
else ifeq ($(strip $(USE_SAFESTRING)),y)
$(ENCSLIB): $(OBJS)
	$(CC) -fPIC -Wl,-no-undefined -shared $(CFLAGS) $(OBJS) -lm -lpthread -L$(CMBASE)/software/lib/ -lsafestring -o libh2enc.so
else
$(ENCSLIB): $(OBJS)
	$(CC) -fPIC -Wl,-no-undefined -shared $(CFLAGS) $(OBJS) -lm -lpthread -o libh2enc.so
endif


%.o: %.c
	$(CC) -c $(CFLAGS) $(ENVSET) $(CONFFLAGS) $< -o $@

clean:
	$(RM) $(ENCLIB) $(ENCSLIB)
	$(RM) ../lib/libh2enc.a
	$(RM) ../lib/ench*.a
	$(RM) .depend
	$(RM) *.o *.gcno *.gcda $(RC_CLEAN_PATH)
	$(RM) ewl/drm/*.o
	$(RM) $(ENCFREERTOS_LIB)
	$(RM) $(FREERTOS_KERNEL_DIR)/*.o
	$(RM) $(FREERTOS_KERNEL_DIR)/portable/MemMang/*.o
	$(RM) $(FREERTOS_DIR)/osal/portable/GCC/Posix/*.o
	$(RM) $(FREERTOS_POSIX_SOURCE_DIR)/*.o
	$(RM) ../source/heif/libheifwriter.a
	$(RM) ../source/heif/*.o
	$(RM) ../source/heif/.depend_heif

tags:
	ctags ../inc/*h ../source/common/*[ch] ../source/hevc/*[ch] ../source/vp9/*[ch]
	#../source/camstab/*[ch]\


lint: $(SRC_HEVC) $(SRC_VP9) $(SRC_JPEG) $(SRC_VS) # take out any not relevant sources
        # -e537 removes warnings about multiple include
        # -e641 removes warnings about converting enum to int
	-lint-nt -w2 -e537 -e641 $(LINT_DEF) $(INCLUDE) $^ > pc-lint-report.txt

depend .depend: $(SRCS)
	$(CC) $(CFLAGS) $(ENVSET) $(CONFFLAGS) -M  $^ > .depend

ifneq (clean, $(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif
