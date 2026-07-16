/*
 * SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */


#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <assert.h>
#include <pthread.h>

#include "testbench_vcodec.h"

static int run_test(int argc, char **argv, const char *optarg) {
    int i = 0, devMax = 3;
    static const char *devName[] = {
      "/dev/hantrovcx",
      "/dev/hantroenc",
      "/dev/hantrodec",
    };

    for (i = 0 ; i < devMax ; ++i) {
        if (strncmp(optarg, devName[i], 14) == 0) {
            break;
        }
    }
    if (i == devMax) {
        printf("Unknown device file: %s\n", optarg);
        return -1;
    }
    switch (i) {
        case 0: return main_vcodex(argc, argv, optarg);
        case 1: return main_encode(argc, argv, optarg);;
        case 2: return main_decode(argc, argv, optarg);
        default: break;
    }

    return -1;
}

//./test_vcodec --file=/dev/hantrodec -v
int main(int argc, char **argv) {
    int opt;
    int long_index = 0;
    
    // 定义长选项数组，必须以全零结构体结尾
    static struct option long_options[] = {
        {"help",     no_argument,       0, 'h'},
        {"file",     required_argument, 0, 'f'},
        {"version",  no_argument,       0, 'v'},
        {0,         0,                  0,  0 }
    };

    const char *optstring = "hf:v";

    while ((opt = getopt_long(argc, argv, optstring, long_options, &long_index)) != -1) {
        switch (opt) {
            case 'h':
                printf("Usage: ./test_vcodec [--file=/dev/hantrovcx - /dev/hantroenc - /dev/hantrodec] [-v]\n");
                break;
            case 'f':
                printf("input device file: %s\n", optarg);
                return run_test(argc, argv, optarg);
            case 'v':
                printf("version 1.0\n");
                break;
            case '?':
                break;
        }
    }
    return 0;
}
