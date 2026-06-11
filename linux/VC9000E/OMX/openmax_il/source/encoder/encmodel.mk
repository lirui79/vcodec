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
ENCODER_RELEASE := ../../../h1_encoder/software
SYSTEM_MODEL := ../../../h1_encoder/system/models

include $(CLEAR_VARS)

LOCAL_ARM_MODE := arm
LOCAL_MODULE := libench1_asic_model
LOCAL_MODULE_TAGS := optional

INCLUDES := $(LOCAL_PATH)/$(ENCODER_RELEASE)
INCLUDES_MODEL := $(LOCAL_PATH)/$(SYSTEM_MODEL)
MODEL_PATH := $(SYSTEM_MODEL)

LOCAL_SRC_FILES := \
    $(MODEL_PATH)/video/h264/H264Asic.c \
    $(MODEL_PATH)/video/h264/H264AsicCodeMacroBlock.c \
    $(MODEL_PATH)/video/h264/H264AsicCodeFrame.c \
    $(MODEL_PATH)/video/h264/H264AsicMacroBlock.c \
    $(MODEL_PATH)/video/h264/H264AsicIntraPrediction.c \
    $(MODEL_PATH)/video/h264/H264AsicTransform.c \
    $(MODEL_PATH)/video/h264/H264AsicSlice.c \
    $(MODEL_PATH)/video/h264/H264AsicDeblocking.c \
    $(MODEL_PATH)/video/h264/H264AsicInterPrediction.c \
    $(MODEL_PATH)/video/h264/H264AsicMotionEstimation.c \
    $(MODEL_PATH)/video/h264/H264AsicNeighbour.c \
    $(MODEL_PATH)/video/h264/H264AsicMvPrediction.c \
    $(MODEL_PATH)/video/h264/H264AsicRateControl.c \
    $(MODEL_PATH)/video/h264/H264AsicPutBits.c \
    $(MODEL_PATH)/video/h264/H264AsicCabac.c \
    $(MODEL_PATH)/video/h264/H264AsicCabacCommon.c \
    $(MODEL_PATH)/video/h264/H264AsicCabacSyntaxElem.c \
    $(MODEL_PATH)/video/h264/H264MvPrediction.c \
    $(MODEL_PATH)/video/vp8/analyse.c \
    $(MODEL_PATH)/video/vp8/code_picture.c \
    $(MODEL_PATH)/video/vp8/inter_prediction.c \
    $(MODEL_PATH)/video/vp8/error.c \
    $(MODEL_PATH)/video/vp8/me_asic.c \
    $(MODEL_PATH)/video/vp8/rate_control.c \
    $(MODEL_PATH)/video/vp8/transform.c \
    $(MODEL_PATH)/video/vp8/analyse_tools.c \
    $(MODEL_PATH)/video/vp8/deblocking.c \
    $(MODEL_PATH)/video/vp8/intra_prediction.c \
    $(MODEL_PATH)/video/vp8/me_random.c \
    $(MODEL_PATH)/video/vp8/picture_buffer.c \
    $(MODEL_PATH)/video/vp8/slice_group.c \
    $(MODEL_PATH)/video/vp8/vp8asic.c \
    $(MODEL_PATH)/video/vp8/code_macroblock.c \
    $(MODEL_PATH)/video/vp8/entropy.c \
    $(MODEL_PATH)/video/vp8/macroblock_tools.c \
    $(MODEL_PATH)/video/vp8/mv_prediction.c \
    $(MODEL_PATH)/video/vp8/put_bits.c \
    $(MODEL_PATH)/video/vp8/test_data.c \
    $(MODEL_PATH)/jpeg/JpegAsic.c \
    $(MODEL_PATH)/jpeg/JpegAsicDct.c \
    $(MODEL_PATH)/jpeg/JpegAsicEnc.c \
    $(MODEL_PATH)/jpeg/JpegAsicPreProcess.c \
    $(MODEL_PATH)/jpeg/JpegAsicPutBits.c \
    $(MODEL_PATH)/jpeg/JpegAsicQuant.c \
    $(MODEL_PATH)/jpeg/JpegAsicRle.c \
    $(MODEL_PATH)/ewl/ewl_system.c \
    $(MODEL_PATH)/ewl/encregisters.c \
    $(MODEL_PATH)/video/common/encasictrace.c \
    $(MODEL_PATH)/video/common/EncAsicPreProcess.c \
    $(MODEL_PATH)/video/common/EncAsicStabilize.c \
    $(MODEL_PATH)/video/common/EncAsicScaling.c

LOCAL_C_INCLUDES := \
    $(INCLUDES)/inc \
    $(INCLUDES)/linux_reference/debug_trace \
    $(INCLUDES)/source/common \
    $(INCLUDES_MODEL)/ewl \
    $(INCLUDES_MODEL)/video/common \
    $(INCLUDES_MODEL)/video/h264 \
    $(INCLUDES_MODEL)/video/vp8 \
    $(INCLUDES_MODEL)/jpeg


include $(BUILD_STATIC_LIBRARY)
