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
include $(CLEAR_VARS)
LOCAL_MODULE_TAGS := optional
HANTRO_LIBS := ../../../libs

ifeq ($(USE_LIBS),true)
LOCAL_PREBUILT_LIBS := \
    $(HANTRO_LIBS)/libh1enc.a \
    $(HANTRO_LIBS)/libench1_asic_model.a

include $(BUILD_MULTI_PREBUILT)
else
include $(LOCAL_PATH)/enclib.mk
include $(LOCAL_PATH)/encmodel.mk
endif

include $(CLEAR_VARS)
LOCAL_ARM_MODE := arm
LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libOMX.hantro.H1.video.encoder

HANTRO_TOP := $(VSI_TOP)
#BELLAGIO_ROOT := external/libomxil-bellagio
ENCODER_RELEASE := $(VSI_TOP)/h1_encoder/software

LOCAL_SRC_FILES := \
    ../msgque.c \
    ../OSAL.c \
    ../basecomp.c \
    ../port.c \
    ../util.c \
    encoder.c \
    encoder_constructor_video.c \
    encoder_h264.c \
    encoder_vp8.c

#    library_entry_point.c

LOCAL_STATIC_LIBRARIES := \
    libh1enc \
    libench1_asic_model

LOCAL_SHARED_LIBRARIES += liblog
LOCAL_SHARED_LIBRARIES += libcutils

LOCAL_LDLIBS += -lpthread

LOCAL_CFLAGS := \
    -DENCH1 \
    -DOMX_ENCODER_VIDEO_DOMAIN \
    -DENABLE_DBGT_TRACE \
    -DDBGT_CONFIG_AUTOVAR

LOCAL_C_INCLUDES := \
    . \
    $(HANTRO_TOP)/openmax_il/source \
    $(HANTRO_TOP)/openmax_il/headers \
    $(ENCODER_RELEASE)/inc \
    $(ENCODER_RELEASE)/linux_reference/memalloc

#    $(BELLAGIO_ROOT)/src \

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)

include $(BUILD_SHARED_LIBRARY)
