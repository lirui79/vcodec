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

CC = gcc

OBJS = $(SRCS:.c=.o)

INCFLAGS = -I./ -I../ -I../../../VC9000D/software/source/inc/  -I../../../VC9000E/software/inc/
DEBFLAGS = -O0 -g
CFLAGS = $(INCFLAGS) $(DEBFLAGS)

SRCS = testbench_vcodec.c testbench_vcodex.c testbench_encode.c testbench_decode.c testbench_mmu.c

TARGET = test_vcodec

all: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -lm -lpthread -o $(TARGET)

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: test
clean:
	rm -f *.o $(TARGET)
