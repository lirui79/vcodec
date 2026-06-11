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

HANTRO_TOP := $(VSI_OMX_TOP)
ifneq ($(BUILD_WITHOUT_PV),true)
PV_TOP := external/opencore
endif

LOCAL_MODULE := libhantro_omx_core
LOCAL_MODULE_TAGS := optional

LOCAL_PRELINK_MODULE := false

LOCAL_SRC_FILES := \
    hantro_omx_core.c

LOCAL_STATIC_LIBRARIES := \

ifeq ($(HANTRO_PARSER),1)
LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog \
    libVendor_hantro_omx_config_parser

LOCAL_CFLAGS := \
    -DHANTRO_PARSER

else
LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog

LOCAL_CFLAGS := \

endif

LOCAL_CFLAGS := \

LOCAL_C_INCLUDES := \
    $(HANTRO_TOP)/headers \
    $(HANTRO_TOP)/source/decoder/config_parser/inc

ifneq ($(BUILD_WITHOUT_PV),true)
LOCAL_C_INCLUDES +=
    $(PV_TOP)/build_config/opencore_dynamic \
    $(PV_TOP)/build_config/opencore_dynamic/build/installed_include \
    $(PV_TOP)/oscl/oscl/config/android \
    $(PV_TOP)/oscl/oscl/config/shared \
    $(PV_TOP)/oscl/oscl/osclbase/src \
    $(PV_TOP)/oscl/oscl/osclerror/src \
    $(PV_TOP)/oscl/oscl/osclmemory/src \
    $(PV_TOP)/oscl/oscl/osclutil/src \
    $(PV_TOP)/oscl/oscl/osclproc/src \
    $(PV_TOP)/oscl/oscl/oscllib/src \
    $(PV_TOP)/oscl/pvlogger/src \
    $(PV_TOP)/codecs_v2/omx/omx_common/include \
    $(PV_TOP)/codecs_v2/omx/omx_queue/src \
    $(PV_TOP)/codecs_v2/utilities/pv_config_parser/include \
    $(PV_TOP)/codecs_v2/omx/omx_proxy/src \
    $(PV_TOP)/codecs_v2/omx/omx_mastercore/include \
    $(PV_TOP)/pvmi/pvmf/include
endif

include $(BUILD_SHARED_LIBRARY)
