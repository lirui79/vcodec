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
include ../../../build/globalrules

cmbase = ../../../..
CMBASE = $(shell cd $(cmbase); pwd )


# the path where to find header files
INCFLAGS = -I../../../inc

ifeq ($(TRACE),y)
  INCFLAGS += -I../../debug_trace \
      -I../../../source/common \
      -I../../../source/jpeg
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

ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  INCFLAGS += -I$(FREERTOS_DIR)/FreeRTOS_Kernel/Source/include \
              -I$(FREERTOS_DIR)/osal/portable/GCC/Posix
  include ../../ewl/osal/freertos/freertos.mk
endif

# list of used sourcefiles
SRCS = JpegTestBench.c EncGetOption.c

ifeq ($(LOW_LATENCY_SLICEINFO_SUPPORT),y)
  SRCS += sliceinfo.c
endif

vpath %.c
vpath %.c ../../../source/jpeg

# name of the control code library 
LIB = ../../libh2enc.a $(SAFESTRINGLIB)
ifeq ($(strip $(USE_FREERTOS_SIMULATOR)),y)
  FREERTOS_KERNEL_LIB = ../../libfreertos.a
endif
FREERTOS_LIB = ../../kernel_module/freertos/libenc_freertos_driver_static.a ../../libfreertos.a 
FREERTOS_LIB_PATH = -L../../ -L../../kernel_module/freertos

# System model library
MODELLIB = $(CMBASE)/system/models/ench2_asic_model.a

# name of the output executable
TARGET = jpeg_testenc


#Here are rules for building codes and generating executable
all:    $(VERSION_H) tags
	@echo ---------------------------------------
	@echo "Usage: make [ system | eval | versatile ]"
	@echo "system        - PC system model (== pclinux)"
	@echo "eval          - PC system model for evaluation with frame limit"
	@echo "pci           - PC with FPGA HW"
	@echo "hwlinux       - control SW with HW, define CROSS_COMPILE to use PC/ARM"
	@echo "armlinux      - ARM aarch64 linux with FPGA HW"
	@echo "arm32_linux   - ARM32 linux with QEMU"
	@echo "arm64_linux   - ARM aarch64 linux with QEMU"
	@echo "riscv32_linux - RISC-V 32 linux with QEMU"
	@echo "riscv64_linux - RISC-V 64 linux with QEMU"
	@echo "integrator    - ARM integrator with FPGA HW"
	@echo "versatile     - ARM versatile with FPGA HW"
	@echo "freertos      - No sepcial cpu, just build with gcc in Linux"
	@echo "NOTE! Make sure to do 'make clean libclean'"
	@echo "between compiling to different targets!"
	@echo ---------------------------------------

# Use other makefiles to build the libraries
$(MODELLIB):
	$(MAKE) -w -C ../../../../$(SYSTEM)/models LOGMSG=n HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)

ifeq (freertos, $(findstring freertos, $(MAKECMDGOALS)))
$(LIB):
	$(MAKE) -w -C ../.. $(TARGETENV) HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9)
else
$(LIB):
	$(MAKE) -w -C ../.. $(TARGETENV) HEVC=$(HEVC) H264=$(H264) AV1=$(AV1) VP9=$(VP9) \
                PCIE_FPGA_VERI_LINEBUF=$(PCIE_FPGA_VERI_LINEBUF)
endif

version.h:../../../../.git/HEAD ../../../../.git/index
	echo "const char *git_version = \"$(shell git rev-parse HEAD)\";" > $@



.PHONY: eval
evaluation: eval
eval: TARGET = jpeg_enc_h2_eval
eval: TARGETENV = eval
eval: $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

pclinux: system

.PHONY: system
system: TARGETENV = system
system: $(VERSION_H) .depend  $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(LIB) ${FREERTOS_KERNEL_LIB} -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: system_multifile
system_multifile: TARGET = jpeg_testenc_multifile
system_multifile: TARGETENV = system
system_multifile: $(VERSION_H) .depend $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: system_static
system_static: TARGETENV = system
system_static: $(VERSION_H) .depend $(LIB) $(OBJS) $(MODELLIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET) -static
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: versatile
versatile: TARGETENV = versatile
versatile: $(VERSION_H) $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: versatile_multifile
versatile_multifile: TARGETENV = versatile
versatile_multifile: TARGET = jpeg_testenc_multifile
versatile_multifile: $(OBJS)
	$(MAKE) -w -C ../.. versatile \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: arm32_linux
arm32_linux: TARGETENV = arm32_linux
arm32_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: arm64_linux
arm64_linux: TARGETENV = arm64_linux
arm64_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: riscv32_linux
riscv32_linux: TARGETENV = riscv32_linux
riscv32_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm -lpthread -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: riscv64_linux
riscv64_linux: TARGETENV = riscv64_linux
riscv64_linux: $(VERSION_H) .depend $(OBJS)
	$(MAKE) -w -C ../.. $@ \
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
pci: $(VERSION_H) .depend $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: hwlinux
hwlinux: TARGETENV = hwlinux
hwlinux: $(VERSION_H) .depend $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: armlinux
armlinux: TARGETENV = armlinux
armlinux: $(VERSION_H) .depend $(LIB) $(OBJS)
	$(MAKE) -w -C ../.. armlinux \
            CROSS_COMPILE=$(CROSS_COMPILE) USRARCH="$(USRARCH)"
	$(CC) $(CFLAGS) $(OBJS) $(LIB) -lm $(PTHREAD_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif

.PHONY: freertos
freertos:TARGETENV = freertos
freertos:$(VERSION_H) .depend $(LIB) $(OBJS) $(CACHELIB)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(CACHELIB) -lm $(FREERTOS_LIB) -o $(TARGET)
ifeq ($(strip $(USE_STRIP)),y)
	@echo -e "[STRIP]\t$(TARGET)"
	@$(STRIP) --strip-all $(TARGET)
endif


system_cov: CC = covc --retain -t!JpegTestBench.c,!EncGetOption.c g++
system_cov: TARGETENV = system_cov
system_cov: $(MODELLIB) $(LIB) $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) $(LIB) $(MODELLIB) $(LIB) -o $(TARGET)
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
	ctags ../../../inc/*.h *.c ../../../source/jpeg/*.[ch] \
            ../../../source/common/*.[ch] ../../ewl/*c
            
depend .depend: $(SRCS)
	@echo -e "\n [DEP]\t $^"
	@awk '{gsub(/```/,"");print}' ../jpgenc_help.md | xxd -i > ../jpgenc_help.dat
	@$(CC) -M $(DEBFLAGS) $(INCFLAGS) $^ > .depend

ifeq (,$(findstring clean, $(MAKECMDGOALS)))
ifeq (.depend, $(wildcard .depend))
include .depend
endif
endif

