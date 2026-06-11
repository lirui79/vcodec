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

PCIE_EN ?= y
EMU ?= n
ARM_CROSS_COMPILE ?= n
USE_PLATFORM_GEN7 ?= n

DEBFLAGS = -O2

ifeq ($(EMU),y)
  ccflags-y += -DEMU
endif
ifeq ($(strip $(PCIE_EN)),y)
  ccflags-y += -DPCIE_EN
endif
ifeq ($(strip $(USE_PLATFORM_GEN7)),y)
ccflags-y   += -DPLATFORM_GEN7
endif

ccflags-y += $(DEBFLAGS)

ifneq ($(KERNELRELEASE),)
# recursive call from kernel build system

arbiter_dev-objs := arbiter_drv.o arbiter_common.o arbiterswhwregister.o
obj-m         += arbiter_dev.o

else

ifneq ($(ARM_CROSS_COMPILE),y)
#KDIR_BASE := /afs/hantro.com/projects/Testing/Board_Version_Control

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


PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules CC=$(CC_VER)

endif

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.ur-safe .cache.mk
	rm -rf modules.order Module.symvers
