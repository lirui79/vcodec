#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Verisilicon Inc.                                --
#-                                                                            --
#-                 (C) COPYRIGHT 2015 VeriSilicon Inc                         --
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

HEVC ?= y
H264 ?= y
AV1 ?= y
VP9 ?= y
LIBVA ?= n
include ../hevc/Makefile

ifneq ($(target),)
  ifneq ($(target),default)
    CFLAGS += -DTARGET -D$(target)
  endif
endif

INCFLAGS += -I../hevc
vpath %.c ../hevc
TARGET = video_testenc
eval: TARGET = video_enc_h2_eval
system_multifile: TARGET = video_testenc_multifile
versatile_multifile: TARGET = video_testenc_multifile
