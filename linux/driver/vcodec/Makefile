################################################################################
# SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
# Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
#
################################################################################
#
# GPL-2.0
#
################################################################################
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; version 2.
#
# This program is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
# Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
################################################################################
#
# Alternatively, This software may be distributed under the terms of
# BSD-3-Clause, in which case the following provisions apply instead of the ones
# mentioned above :
#
################################################################################
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its contributors
# may be used to endorse or promote products derived from this software without
# specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
################################################################################

vcodec_dev ?= vcodec

PCIE_EN ?= n
EMU ?= n
ARM_CROSS_COMPILE ?= n
SUPPORT_VCMD_ENABLE_IP ?= n

# set it to 'y' to enable MMU
SUPPORT_MMU ?= y
# set it to 'y' to enable 48PA MMU
SUPPORT_48PA_MMU ?= y
# MMU VA lenth
MMU_VA_BITS ?= 40
# set it to 'y' to enable 48PA MMU page table switch
MMU_PAGE_TABLE_SWITCH ?= y
# set it to 'y' to enable AXIFE
SUPPORT_AXIFE ?= y
# set it to 'y' to enable AXI2TO1
SUPPORT_AXI2TO1 ?= y
# set it to 'y' to enable AFBC
SUPPORT_AFBC ?= y
# set it to 'y' to enable ddr lowlatency
LOW_LATENCY_SLICEINFO_SUPPORT ?= n
# set it to 'y' to enable UFBC
SUPPORT_UFBC ?= n
# Support to read HW resources from DTB
SUPPORT_DTB ?= n
SUPPORT_DTB_IM ?= n
# set it to 'y' to enable SW watchdog
SUPPORT_WATCHDOG ?= y

# set it to 'y' to enable debugfs
SUPPORT_DEBUGFS ?= n
# set it to 'y' to enable power management
SUPPORT_PM ?= n
# if need request vcarb manually, set it to 'y'
#VCARB_REQUEST ?= n

ifeq ($(EMU),y)
  EXTRA_CFLAGS += -DEMU
endif

#CC += -I$(obj) -I$(obj)/mailbox
ccflags-y += -I$(src) -I$(src)/mailbox
EXTRA_CFLAGS += -g

# Print debugging messages from the device
#EXTRA_CFLAGS	+= -DHANTRO_DRIVER_DEBUG

#EXTRA_CFLAGS += -DHANTROMMU_DEBUG

snapshot := $(wildcard $(obj)/.snapshot)
ifneq ($(snapshot),)
SNAPSHOT_CFLAGS	:= -DSNAPSHOT='$(shell cat $(snapshot))'
EXTRA_CFLAGS	+= $(SNAPSHOT_CFLAGS)
endif

# if 48pa MMU, should not define PD_MODE
ifeq ($(strip $(SUPPORT_MMU)),y)
EXTRA_CFLAGS   += -DSUPPORT_MMU
	ifeq ($(SUPPORT_48PA_MMU),y)
		EXTRA_CFLAGS += -DSUPPORT_48PA_MMU
		EXTRA_CFLAGS += -DMMU_VA_BITS=$(MMU_VA_BITS)
		ifeq ($(MMU_PAGE_TABLE_SWITCH),y)
			EXTRA_CFLAGS += -DMMU_PAGE_TABLE_SWITCH
		endif
	else
		EXTRA_CFLAGS += -DPD_MODE
	endif
endif


ifeq ($(SUPPORT_AFBC),y)
  DEBFLAGS += -DSUPPORT_AFBC
endif

ifeq ($(strip $(SUPPORT_AXIFE)),y)
EXTRA_CFLAGS   += -DSUPPORT_AXIFE
endif

ifeq ($(strip $(SUPPORT_AXI2TO1)),y)
EXTRA_CFLAGS   += -DAXI2TO1_SUPPORT
endif

ifeq ($(strip $(VCARB_REQUEST)),y)
EXTRA_CFLAGS   += -DVCARB_REQUEST
endif

ifeq ($(strip $(SUPPORT_VCMD_ENABLE_IP)),y)
EXTRA_CFLAGS   += -DHANTROVCMD_ENABLE_IP_SUPPORT
endif

ifeq ($(strip $(PCIE_EN)),y)
EXTRA_CFLAGS   += -DPCIE_EN
endif

ifeq ($(strip $(LOW_LATENCY_SLICEINFO_SUPPORT)),y)
EXTRA_CFLAGS   += -DLOW_LATENCY_SLICEINFO_SUPPORT
endif

ifeq ($(strip $(SUPPORT_UFBC)),y)
EXTRA_CFLAGS   += -DSUPPORT_UFBC
endif

ifeq ($(strip $(SUPPORT_DTB)),y)
EXTRA_CFLAGS   += -DDTB_SUPPORT
ifeq ($(strip $(SUPPORT_DTB_IM)),y)
EXTRA_CFLAGS   += -DDTB_IM_SUPPORT
endif
endif

ifeq ($(SUPPORT_DEBUGFS),y)
  EXTRA_CFLAGS += -DSUPPORT_DBGFS
endif

ifeq ($(SUPPORT_WATCHDOG),y)
  EXTRA_CFLAGS += -DSUPPORT_WATCHDOG
endif

ifeq ($(SUPPORT_PM),y)
  EXTRA_CFLAGS += -DCONFIG_ENC_PM -DCONFIG_DEC_PM
endif

ifeq ($(obj),)
obj = .
endif

ccflags-y += $(EXTRA_CFLAGS)
#################################################
# configuration

MDIR		:= hantro

# drivers objects
# list-multi	:= hantro_mmu.o

# # what to build

$(vcodec_dev)-objs 	:=  vcx_driver.o vcmdswhwregisters.o bidirect_list.o vcm_driver.o vcx_kthread.o
$(vcodec_dev)-objs  +=  vce_driver.o vce_vcmd_driver.o vce_normal_driver.o
$(vcodec_dev)-objs  +=  vcd_driver.o vcd_vcmd_driver.o vcd_normal_driver.o
$(vcodec_dev)-objs  +=  vcm_driver.o mhu_v3_client.o

$(vcodec_dev)-objs  +=  mailbox/cqueue.o mailbox/bqueue.o mailbox/cmda78_session.o mailbox/cmda78_mgr.o mailbox/cmda78_msg.o mailbox/cmda78_proc.o mailbox/cmdnode.o

obj-m         += $(vcodec_dev).o


ifeq ($(strip $(SUPPORT_MMU)),y)
$(vcodec_dev)-objs += vcx_mmu.o
endif

ifeq ($(strip $(SUPPORT_AXIFE)),y)
$(vcodec_dev)-objs += vcx_axife.o
endif

ifeq ($(strip $(SUPPORT_AXI2TO1)),y)
$(vcodec_dev)-objs += vcx_axi2to1.o
endif

ifeq ($(strip $(SUPPORT_AFBC)),y)
$(vcodec_dev)-objs += vcx_afbc.o
endif

ifeq ($(SUPPORT_DEBUGFS),y)
  $(vcodec_dev)-objs += vcx_vcmd_dbgfs.o
endif

tardest		:= .

#################################################
# compile modules

ifneq ($(KERNELRELEASE),)
# recursive call from kernel build system
dummy		:= $(shell echo $(KERNELRELEASE) > $(obj)/.version)

ifeq ($(VERSION).$(PATCHLEVEL),2.6)
 export-objs	:=
 list-multi	:=
else
 multi-m	:= $(filter $(list-multi), $(obj-m))
 int-m		:= $(sort $(foreach m, $(multi-m), $($(basename $(m))-objs)))
 export-objs	:= $(filter $(int-m) $(obj-m),$(export-objs))
endif

-include $(TOPDIR)/Rules.make
else
# take version info from last module build if available
KERNELRELEASE	:= $(shell cat $(obj)/.version 2>/dev/null || uname -r)
endif
ifneq ($(ARM_CROSS_COMPILE),y)
KDIR_BASE := /afs/hantro.com/projects/Testing/Board_Version_Control

#KDIR := $(KDIR_BASE)/Realview_EB/SW/Linux/v0_0/linux-2.6.19-arm2
#KDIR := $(KDIR_BASE)/Realview_EB/SW/Linux/linux-2.6.21-arm1/v0_0/linux-2.6.21-arm1
#KDIR := $(KDIR_BASE)/Realview_PB/PB926EJS/SW/Linux/linux-2.6.24-arm2-spnlck/v0_1/linux-2.6.24-arm2-spnlck
#KDIR := $(KDIR_BASE)/Realview_PB/PB926EJS/SW/Linux/linux-2.6.28-arm1/v0_1/linux-2.6.28-arm1
#KDIR := $(KDIR_BASE)/SW_Common/ARM_realview_v6/2.6.28-arm1/v0_1-v6/linux-2.6.28-arm1

KVER := $(shell uname -r)
KDIR := /lib/modules/$(KVER)/build

#CC_VER   := gcc-8
CC_VER   := gcc
else
export ARCH=arm64
export CROSS_COMPILE=/opt/kmb/gcc-arm-8.3-2019.03-x86_64-aarch64-linux-gnu/bin/aarch64-linux-gnu-
KDIR := /home/vsi/kmb-evm/kernel/mainline-tracking
CC_VER := $(CROSS_COMPILE)gcc
endif

PWD		:= $(shell pwd)
DEST		:= /lib/modules/$(KERNELRELEASE)/$(MDIR)

# which files to install?
inst-m		:= $(wildcard *.ko)
ifeq ($(inst-m),)
  inst-m	:= $(obj-m)
endif

# locales seem to cause trouble sometimes.
LC_ALL = POSIX
export LC_ALL
default::
	$(MAKE) -C $(KDIR) M=$(PWD) modules CC=$(CC_VER)

install::
	strip --strip-debug $(inst-m)
	-su -c "mkdir -p $(DEST); cp -v $(inst-m) $(DEST); depmod -a"

clean::
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	-rm -f .version

#################################################
# build tarballs

thisdir	:= $(notdir $(PWD))
name	:= $(shell echo $(thisdir) | sed 's/-.*//')
ver	:= $(shell echo $(thisdir) | sed 's/.*-//')
date    := $(shell date +%Y%m%d)
tardest	?= .

snapdir := $(HOME)/snapshot
snap	?= $(name)

release: clean
	rm -f .snapshot
	(cd ..; tar cvzf $(tardest)/$(name)-$(ver).tar.gz $(thisdir))

snapshot snap tarball: clean
	echo $(date) > .snapshot
	(cd ..; tar czf $(snapdir)/$(snap)-$(date).tar.gz $(thisdir))
	$(MAKE) -C $(snapdir)

#################################################
# other stuff

%.asm: %.o
	objdump -S $< > $@

