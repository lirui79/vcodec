#------------------------------------------------------------------------------
#-       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
#-         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
#-         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
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
#-----------------------------------------------------------------------------*/
ifeq ($(USE_INTERNAL_TEST),y)
  DEFINES += -DINTERNAL_TEST
  DWL_SRCS += software/linux/dwl/internal_test.c
endif


ifeq ($(SUPPORT_AXIFE),y)
  DEFINES += -DSUPPORT_AXIFE
  ifeq ($(SUPPORT_AXIFE_MID),y)
    DEFINES += -DSUPPORT_AXIFE_MID
    DEFINES += -DAXIFE_V_3_2
  else ifeq ($(AXIFE_V30),y)
    DEFINES += -DAXIFE_V_3_0
  else ifeq ($(AXIFE_V32),y)
    DEFINES += -DAXIFE_V_3_2
  endif
endif

ifeq ($(SUPPORT_APB),y)
  DEFINES += -DSUPPORT_APB
endif

ifeq ($(SUPPORT_SUBSYSTEM_TB),y)
  DEFINES += -DSUPPORT_SUBSYSTEM_TB
endif

ifeq ($(USE_ONL_SIM),y)
  DWL_SRCS += software/linux/dwl/dwl_dpi/vpu_dwl_dpi.c
  DEFINES += -DASIC_ONL_SIM
ifeq ($(PP_REORDER_CHECK),y)
  DEFINES += -DPP_REORDER_CHECK
endif
ifeq ($(PP_CH_CHECK),y)
  DEFINES += -DPP_CH_CHECK
endif
ifeq ($(VRFD_CHECK),y)
  DEFINES += -DVRFD_CHECK
endif
ifeq ($(PP_FAST_MODE),y)
  DEFINES += -DPP_FAST_MODE
endif
endif

ifeq ($(PJPEG_CHECK_COEFF),y)
  DEFINES += -DCHECK_PJPEG_COEFF
endif

# list of used sourcefiles
SRC_DWL_HW = software/linux/dwl/dwl_linux.c \
             software/linux/dwl/dwl_linux_hw.c \
             software/linux/dwl/dwl_activity_trace.c \
             software/linux/dwl/dwl_vcmd_common.c \
             software/linux/dwl/dwl_buf_protect.c \
             software/linux/dwl/dwl_perf_info.c \
             software/linux/dwl/dwl_linux_memsync.c

ifeq ($(USE_PPU_V9_2_3), n)
ifeq ($(SUPPORT_DEC400),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_dec400.c
endif
endif

SRC_DWL_SIMULATION := software/linux/dwl/dwl_pc.c \
                      software/linux/dwl/vwl_pc.c \
                      software/linux/dwl/dwl_hw_core_array.c \
                      software/linux/dwl/dwl_hw_core.c \
                      software/linux/dwl/dwl_swhw_sync.c \
                      software/linux/dwl/dwl_activity_trace.c \
                      software/linux/dwl/dwl_vcmd_common.c \
                      software/linux/dwl/dwl_buf_protect.c \
                      software/linux/dwl/dwl_perf_info.c \
                      software/linux/dwl/dwl_pc_memsync.c

ifeq ($(USE_PPU_V9_2_3), n)
ifeq ($(SUPPORT_DEC400), y)
  SRC_DWL_SIMULATION += software/linux/dwl/dwl_pc_dec400.c
endif
endif

SRC_DWL_OS_FREE := software/linux/dwl/dwl_x170_osfree.c \
                   software/linux/dwl/dwl_syncobj_osfree.c \
                   software/linux/dwl/hx170dec_osfree.c \
                   software/linux/dwl/vsi_string.c \
                   software/linux/dwl/dwl_buf_protect.c \
                   software/linux/dwl/dwl_pc_memsync.c

#ifeq ($(findstring h264dec_osfree,$(MAKECMDGOALS)),h264dec_osfree)
#  DWL_SRCS += $(SRC_DWL_OS_FREE)
#else

ifeq ($(SUPPORT_AXIFE),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_axife.c
else ifeq ($(USE_AXIFE),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_axife.c
endif
ifeq ($(SUPPORT_MMU),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_mmu.c
else ifeq ($(USE_MMU),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_mmu.c
endif
ifeq ($(SUPPORT_SUBSYSTEM_TB),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_tb.c
endif
ifeq ($(SUPPORT_APB),y)
  SRC_DWL_HW += software/linux/dwl/dwl_linux_apbfilter.c
endif
ifeq ($(findstring h264dec_osfree,$(MAKECMDGOALS)),h264dec_osfree)
  DWL_SRCS += $(SRC_DWL_OS_FREE)
  CFLAGS +=-DNON_PTHREAD_H
else
ifeq ($(strip $(USE_MODEL_SIMULATION)),y)
  DWL_SRCS += $(SRC_DWL_SIMULATION)
else
  DWL_SRCS += $(SRC_DWL_HW)
endif
endif
#endif

