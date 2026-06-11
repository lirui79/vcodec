#------------------------------------------------------------------------------
#-       Copyright (c) 2015-2017, VeriSilicon Inc. All rights reserved        --
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
#-------------------------------------------------------------------------------


LOCAL_PATH := $(call my-dir)
ENCODER_RELEASE := ../../../vc8000e_encoder/software

############################################################
include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libh1enc
LOCAL_MODULE_TAGS := optional

INCLUDES := $(LOCAL_PATH)/$(ENCODER_RELEASE)

LOCAL_SRC_FILES := \
    $(ENCODER_RELEASE)/source/common/encswhwregisters.c\
    $(ENCODER_RELEASE)/source/hevc/hevcencapi.c\
    $(ENCODER_RELEASE)/source/hevc/sw_picture.c\
    $(ENCODER_RELEASE)/source/hevc/sw_parameter_set.c\
    $(ENCODER_RELEASE)/source/hevc/rate_control_picture.c\
    $(ENCODER_RELEASE)/source/hevc/sw_nal_unit.c\
    $(ENCODER_RELEASE)/source/hevc/sw_slice.c\
    $(ENCODER_RELEASE)/source/hevc/hevcSei.c\
    $(ENCODER_RELEASE)/source/hevc/sw_cu_tree.c\
    $(ENCODER_RELEASE)/source/hevc/cutreeasiccontroller.c\
    $(ENCODER_RELEASE)/source/hevc/hevcenccache.c\
    $(ENCODER_RELEASE)/source/hevc/av1_obu.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpeg.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegInit.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegCodeFrame.c\
    $(ENCODER_RELEASE)/source/jpeg/EncJpegPutBits.c\
    $(ENCODER_RELEASE)/source/jpeg/JpegEncApi.c\
    $(ENCODER_RELEASE)/source/jpeg/MjpegEncApi.c\
    $(ENCODER_RELEASE)/source/hevc/sw_test_id.c\
    $(ENCODER_RELEASE)/source/common/encasiccontroller_v2.c\
    $(ENCODER_RELEASE)/source/common/encasiccontroller.c\
    $(ENCODER_RELEASE)/source/common/queue.c\
    $(ENCODER_RELEASE)/source/common/checksum.c\
    $(ENCODER_RELEASE)/source/common/crc.c\
    $(ENCODER_RELEASE)/source/common/hash.c\
    $(ENCODER_RELEASE)/source/common/sw_put_bits.c\
    $(ENCODER_RELEASE)/source/common/tools.c\
    $(ENCODER_RELEASE)/source/common/encpreprocess.c\
    $(ENCODER_RELEASE)/source/common/encinputlinebuffer.c\
    $(ENCODER_RELEASE)/source/common/error.c\
    $(ENCODER_RELEASE)/linux_reference/debug_trace/enctrace.c

LOCAL_C_INCLUDES := \
    $(INCLUDES)/inc \
    $(INCLUDES)/source/hevc \
    $(INCLUDES)/source/vp9 \
    $(INCLUDES)/source/jpeg \
    $(INCLUDES)/source/common \
    $(INCLUDES)/linux_reference/ewl \
    $(INCLUDES)/linux_reference/debug_trace \
    $(INCLUDES)/linux_reference/kernel_module \
    $(INCLUDES)/linux_reference/memalloc

LOCAL_CFLAGS += \
    -DCTBRC_STRENGTH

include $(BUILD_STATIC_LIBRARY)

############################################################