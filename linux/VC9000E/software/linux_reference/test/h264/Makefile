#-------------------------------------------------------------------------------
#-                                                                            --
#-       This software is confidential and proprietary and may be used        --
#-        only as expressly authorized by a licensing agreement from          --
#-                                                                            --
#-                            Verisilicon Inc.                                --
#-                                                                            --
#-                   (C) COPYRIGHT 2015 VeriSilicon Inc.                      --
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
AV1 ?= n
VP9 ?= n
include ../hevc/Makefile

ifneq ($(target),)
  ifneq ($(target),default)
    CFLAGS += -DTARGET -D$(target)
  endif
endif
ifeq ($(SYSTEM),system60)
HEVC = n
H264 = y
endif

INCFLAGS += -I../hevc
CFLAGS += -DCODEC_FORMAT=1

vpath %.c ../hevc
TARGET = h264_testenc
eval: TARGET = h264_enc_h2_eval
system_multifile: TARGET = h264_testenc_multifile
versatile_multifile: TARGET = h264_testenc_multifile
