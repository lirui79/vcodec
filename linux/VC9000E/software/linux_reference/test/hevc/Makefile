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
#--  Abstract : Makefile for encoder testbench
#--
#-------------------------------------------------------------------------------

include ../../../build/globaldefs

HEVC ?= y
H264 ?= n
AV1 ?= n
VP9 ?= n
RCSRC ?= n
LIBVA ?= n
include ../../../build/globalrules

cmbase = ../../../..
CMBASE = $(shell cd $(cmbase); pwd )

# RC path must be added before source
ifeq ($(RCSRC),y)
INCFLAGS = -I../../../../system/models/video/common/
endif

# the path where to find header files
INCFLAGS += -I../../../inc

ifeq ($(HDR_HELPER_SUPPORT),y)
INCFLAGS += -I../../../extern/frozen
endif

ifeq ($(INTERNAL_TEST),y)
  USE_INTERNAL_INC = y
endif

ifeq ($(TRACE),y)
  INCFLAGS += -I../../debug_trace
  USE_INTERNAL_INC = y
endif

ifeq ($(USE_INTERNAL_INC),y)
  INCFLAGS +=-I../../../source/common  \
           -I../../../source/hevc
  ifeq ($(AV1),y)
    INCFLAGS +=-I../../../source/av1
  endif
  ifeq ($(VP9),y)
    INCFLAGS +=-I../../../source/vp9
  endif
  ifeq ($(INCLUDE_VS),y)
    INCFLAGS += -I../../../source/camstab
  endif
endif

FREERTOS_DIR = ../../ewl/../../../../../FreeRTOS
ifeq (freertos, $(findstring freertos, $(MAKECMDGOALS)))
  FREERTOS_POSIX_DIR = $(FREERTOS_DIR)/lib
  FREERTOS_KERNEL_DIR = $(FREERTOS_DIR)/FreeRTOS_Kernel/Source
	INCFLAGS += -I$(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/include/portable/pc/linux \
                -I$(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/include/portable \
                -I$(FREERTOS_POSIX_DIR)/FreeRTOS-Plus-POSIX/include \
                -I$(FREERTOS_POSIX_DIR)/include/private \
                -I$(FREERTOS_POSIX_DIR)/include \
                -I$(FREERTOS_KERNEL_DIR)/include \
                -I$(FREERTOS_DIR)/osal/portable/GCC/Posix \
                -I../../kernel_module/freertos
    CFLAGS += -D__FREERTOS__
endif

#ifeq ($(AV1),y)
#	INCFLAGS += -I../../../source/av1
#endif

#ifeq ($(VP9),y)
#	INCFLAGS +=	-I../../../source/vp9
#endif

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  INCFLAGS += -I$(FREERTOS_DIR)/FreeRTOS_Kernel/Source/include \
              -I$(FREERTOS_DIR)/osal/portable/GCC/Posix
  include ../../ewl/osal/freertos/freertos.mk
endif

ifneq ($(target),)
  ifneq ($(target), default)
    CFLAGS += -DTARGET -D$(target)
  endif
endif

ifneq ($(P4CL),)
	CFLAGS += -DP4CL=$(P4CL)
endif

# list of used sourcefiles
SRCS = test_bench.c test_bench_utils.c get_option.c test_bench_pic_config.c tb_flex_ref.c tb_input_file_list.c tb_parse_roi.c

ifeq ($(HDR_HELPER_SUPPORT),y)
# for hdr10 or dolby vision
SRCS += frozen.c tb_hdr_info.c
endif

ifeq ($(INTERNAL_TEST),y)
# for internal tests
SRCS += test_bench_statistic.c
endif

ifeq ($(LOW_LATENCY_SLICEINFO_SUPPORT),y)
SRCS += sliceinfo.c
endif

vpath %.c
vpath %.c ../../../source/hevc ../../../extern/frozen


ifeq ($(HEIF_SUPPORT),y)
#heif source
HEIF_PATH = ../../../source/heif
ifeq ($(USE_ANDROID_NDK_CROSS_COMPILE),y)
ifeq ($(CROSS_COMPILE), x86_64-linux-android33-clang)
  heif_lib_dir=$(EXT_HEIF_BASE)/build/release/android/x86_64/lib/
else
  heif_lib_dir=$(EXT_HEIF_BASE)/build/release/android/arm64-v8a/lib/
endif
else ifeq ($(USE_ARMLINUX_NDK_CROSS_COMPILE),y)
  heif_lib_dir=$(EXT_HEIF_BASE)/build/release/linux/arm/lib/
else
  heif_lib_dir=$(EXT_HEIF_BASE)/build/release/linux/x86/lib/
endif
HEIFLIB = -L$(CMBASE)/software/source/heif/ -L$(heif_lib_dir) -lheifwriter -lheif_static -lheif_writer_static -lstdc++  
LIBHEIF = ../../../source/heif/libheifwriter.a
CFLAGS += -DHEIF_SUPPORT
ifeq ($(AVIF_SUPPORT),y)
CFLAGS += -DAVIF_SUPPORT
endif
else
HEIFLIB =
LIBHEIF =
LIBEWLDRM =
endif
LIB = ../../libh2enc.a
ifeq ($(LIBVA), y)
LIB = ../../../lib/libh2enc.a
LIBEWLDRM = ${VSVAAPI_DIR}/usr/lib/libdrm.so ${VSVAAPI_DIR}/usr/lib/libdrm_hantro.so
CFLAGS += -DUSE_LIBVA
endif
LIB += $(SAFESTRINGLIB) $(HEIFLIB)

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  FREERTOS_KERNEL_LIB = ../../libfreertos.a
endif
FREERTOS_LIB = ../../kernel_module/freertos/libenc_freertos_driver_static.a ../../libfreertos.a
FREERTOS_LIB_PATH = -L../../ -L../../kernel_module/freertos

# System model library
MODELLIB = $(CMBASE)/$(SYSTEM)/models/ench2_asic_model.a
ifeq ($(VMAF_SUPPORT),y)
VMAFLIB = -L$(CMBASE)/software/lib/ -lvmaf -lstdc++
CFLAGS += -DVMAF_SUPPORT
endif

# name of the output executable
TARGET = hevc_testenc


#Here are rules for building codes and generating executable
all:    $(VERSION_H) tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | eval | versatile | android ]"
	@echo "system        - PC system model (== pclinux)"
	@echo "eval          - PC system model for evaluation with frame limit"
	@echo "pci           - PC with FPGA HW"
	@echo "armlinux      - ARM aarch64 linux with FPGA HW"
	@echo "arm32_linux   - ARM32 linux with QEMU"
	@echo "arm64_linux   - ARM aarch64 linux with QEMU"
	@echo "riscv32_linux - RISC-V 32 linux with QEMU"
	@echo "riscv64_linux - RISC-V 64 linux with QEMU"
	@echo "integrator    - ARM integrator with FPGA HW"
	@echo "versatile     - ARM versatile with FPGA HW"
	@echo "android       - ARM android"
	@echo "freertos      - No sepcial cpu, just build with gcc in Linux"
	@echo "NOTE! Make sure to do 'make clean libclean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

# Use other makefiles to build the libraries
$(MODELLIB):
	$(MAKE) -w -C ../../../../$(SYSTEM)/models LOGMSG=n HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)

ifeq ($(HEIF_SUPPORT),y)	
$(LIBHEIF):
	$(MAKE) -w -C $(HEIF_PATH)
endif

$(LIB): $(LIBHEIF)
	$(MAKE) -w -C ../.. $(TARGETENV) HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9) LIBVA=$(LIBVA) \
                PCIE_FPGA_VERI_LINEBUF=$(PCIE_FPGA_VERI_LINEBUF)

version.h:../../../../.git/HEAD ../../../../.git/index
	echo "const char *git_version = \"$(shell git rev-parse HEAD)\";" > $@



.PHONY: eval
evaluation: eval
eval: TARGET = hevc_enc_h2_eval
eval: TARGETENV = eval
eval: $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(VMAFLIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

pclinux: system

.PHONY: system
system: TARGETENV = system
system: $(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(VMAFLIB) $(CACHELIB) ${FREERTOS_KERNEL_LIB} -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)*
endif

.PHONY: system_multifile
system_multifile: TARGET = hevc_testenc_multifile
system_multifile: TARGETENV = system
system_multifile: $(VERSION_H) .depend $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(VMAFLIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: system_static
system_static: TARGETENV = system
system_static: $(VERSION_H) .depend $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(VMAFLIB) -lm $(PTHREAD_LIB) -o $(TARGET) -static
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)*
endif

.PHONY: versatile
versatile: TARGETENV = versatile
versatile: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: versatile_multifile
versatile_multifile: TARGETENV = versatile
versatile_multifile: TARGET = hevc_testenc_multifile
versatile_multifile: .depend $(OBJS)
	$(MAKE) -w -C ../.. versatile \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: arm32_linux
arm32_linux: TARGETENV = arm32_linux
arm32_linux: $(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB)
	echo "$(MAKE) -w -C ../.. $@ HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9) CROSS_COMPILE=$(CROSS_COMPILE) USRARCH=$(USRARCH)"
	$(MAKE) -w -C ../.. $@ HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)\
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: arm64_linux
arm64_linux: TARGETENV = arm64_linux
arm64_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)\
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: riscv32_linux
riscv32_linux: TARGETENV = riscv32_linux
riscv32_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)\
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: riscv64_linux
riscv64_linux: TARGETENV = riscv64_linux
riscv64_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)\
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: integrator
integrator: TARGETENV = integrator
integrator: $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: pci
pci: TARGETENV = pci
pci: $(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(LIBEWLDRM) $(CACHELIB) $(VMAFLIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)*
endif

.PHONY: armlinux
armlinux: TARGETENV = armlinux
armlinux: $(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB)
	$(MAKE) -w -C ../.. armlinux \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) $(VMAFLIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: freertos
freertos:TARGETENV = freertos
freertos:$(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) $(VMAFLIB) -lm $(FREERTOS_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif


system_cov: CC = covc --retain -t!HEVCTestBench.c,!EncGetOption.c g++
system_cov: TARGETENV = system_cov
system_cov: $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(VMAFLIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: android
android: TARGETENV = android
android: $(VERSION_H) $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS)  $(LIB)  -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: clean
clean:
	$(RM) .depend
	$(RM) *.o *.gcno *.gcda core* *~ $(TARGET) $(TARGET).* $(VERSION_H)

.PHONY: libclean
libclean: clean
	$(MAKE) -w -C ../.. clean

.PHONY: tags
tags:
	ctags ../../../inc/*.h *.c ../../../source/hevc/*.[ch] \
            ../../../source/common/*.[ch] ../../ewl/*c

depend .depend: $(SRCS)
	@echo -e "\n [DEP]\t $^"
	@awk '{gsub(/```/,"");print}' ../vcenc_help.md | xxd -i > ../vcenc_help.dat
	@$(CC) -M $(DEBFLAGS) $(INCFLAGS) $^ > .depend

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif

