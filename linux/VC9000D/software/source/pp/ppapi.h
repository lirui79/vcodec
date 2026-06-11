/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
--         Copyright (c) 2007-2010, Hantro OY. All rights reserved.           --
--                                                                            --
-- This software is confidential and proprietary and may be used only as      --
--   expressly authorized by VeriSilicon in a written licensing agreement.    --
--                                                                            --
--         This entire notice must be reproduced on all copies                --
--                       and may not be removed.                              --
--                                                                            --
--------------------------------------------------------------------------------
-- Redistribution and use in source and binary forms, with or without         --
-- modification, are permitted provided that the following conditions are met:--
--   * Redistributions of source code must retain the above copyright notice, --
--       this list of conditions and the following disclaimer.                --
--   * Redistributions in binary form must reproduce the above copyright      --
--       notice, this list of conditions and the following disclaimer in the  --
--       documentation and/or other materials provided with the distribution. --
--   * Neither the names of Google nor the names of its contributors may be   --
--       used to endorse or promote products derived from this software       --
--       without specific prior written permission.                           --
--------------------------------------------------------------------------------
-- THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"--
-- AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE  --
-- IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE --
-- ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE  --
-- LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR        --
-- CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF       --
-- SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS   --
-- INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN    --
-- CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)    --
-- ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE --
-- POSSIBILITY OF SUCH DAMAGE.                                                --
--------------------------------------------------------------------------------
------------------------------------------------------------------------------*/

#ifndef __PPAPI_H__
#define __PPAPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "dwl.h"
#include "commonfunction.h"

/**
 * \addtogroup common_group
 *
 * @{
 */
#define PP_PIPELINE_DISABLED                            0U
#define PP_PIPELINED_DEC_TYPE_H264                      1U
#define PP_PIPELINED_DEC_TYPE_MPEG4                     2U
#define PP_PIPELINED_DEC_TYPE_JPEG                      3U
#define PP_PIPELINED_DEC_TYPE_VC1                       4U
#define PP_PIPELINED_DEC_TYPE_MPEG2                     5U
#define PP_PIPELINED_DEC_TYPE_VP6                       6U
#define PP_PIPELINED_DEC_TYPE_AVS                       7U
#define PP_PIPELINED_DEC_TYPE_RV                        8U
#define PP_PIPELINED_DEC_TYPE_VP8                       9U
#define PP_PIPELINED_DEC_TYPE_WEBP                     10U

#define PP_PIX_FMT_YCBCR_4_0_0                          0x080000U

#define PP_PIX_FMT_YCBCR_4_2_2_INTERLEAVED              0x010001U
#define PP_PIX_FMT_YCRYCB_4_2_2_INTERLEAVED             0x010005U
#define PP_PIX_FMT_CBYCRY_4_2_2_INTERLEAVED             0x010006U
#define PP_PIX_FMT_CRYCBY_4_2_2_INTERLEAVED             0x010007U
#define PP_PIX_FMT_YCBCR_4_2_2_SEMIPLANAR               0x010002U

#define PP_PIX_FMT_YCBCR_4_2_2_TILED_4X4                0x010008U
#define PP_PIX_FMT_YCRYCB_4_2_2_TILED_4X4               0x010009U
#define PP_PIX_FMT_CBYCRY_4_2_2_TILED_4X4               0x01000AU
#define PP_PIX_FMT_CRYCBY_4_2_2_TILED_4X4               0x01000BU

#define PP_PIX_FMT_YCBCR_4_4_0                          0x010004U

#define PP_PIX_FMT_YCBCR_4_2_0_PLANAR                   0x020000U
#define PP_PIX_FMT_YCBCR_4_2_0_SEMIPLANAR               0x020001U
#define PP_PIX_FMT_YCBCR_4_2_0_TILED                    0x020002U

#define PP_PIX_FMT_YCBCR_4_1_1_SEMIPLANAR               0x100001U
#define PP_PIX_FMT_YCBCR_4_4_4_SEMIPLANAR               0x200001U

#define PP_PIX_FMT_RGB16_CUSTOM                         0x040000U
#define PP_PIX_FMT_RGB16_5_5_5                          0x040001U
#define PP_PIX_FMT_RGB16_5_6_5                          0x040002U
#define PP_PIX_FMT_BGR16_5_5_5                          0x040003U
#define PP_PIX_FMT_BGR16_5_6_5                          0x040004U

#define PP_PIX_FMT_RGB32_CUSTOM                         0x041000U
#define PP_PIX_FMT_RGB32                                0x041001U
#define PP_PIX_FMT_BGR32                                0x041002U

#define PP_YCBCR2RGB_TRANSFORM_CUSTOM                   0U
#define PP_YCBCR2RGB_TRANSFORM_BT_601                   1U
#define PP_YCBCR2RGB_TRANSFORM_BT_709                   2U

#define PP_PIC_FRAME_OR_TOP_FIELD                       0U
#define PP_PIC_BOT_FIELD                                1U
#define PP_PIC_TOP_AND_BOT_FIELD                        2U
#define PP_PIC_TOP_AND_BOT_FIELD_FRAME                  3U
#define PP_PIC_TOP_FIELD_FRAME                          4U
#define PP_PIC_BOT_FIELD_FRAME                          5U

#define PP_MAX_MULTIBUFFER                              17
/**@}*/

/**
 * \brief standalone pp instance
 * \ingroup common_group
 */
typedef void *PPInst;

/**
 * \brief Post proccessor setting params, will be used in PPSetInfo.
 * \ingroup common_group
 */
typedef struct PPConfig_ {
  u32 in_format;  /**< \brief The format of input. */
  u32 in_stride; /**< \brief pic stride of the input. */
  u32 in_stride_ch; /**< \brief pic chroma stride of the input. */
  u32 in_height; /**< \brief pixels height of the input. */
  u32 in_width;  /**< \brief pixels width of the input. */
  u32 in_lu_stride;  /**< \brief input stride of RFC Luma data. */
  u32 in_ch_stride;  /**< \brief input stride of RFC Chroma data. */
  u32 in_lut_stride;  /**< \brief input stride of RFC Luma table data. */
  u32 in_cht_stride;  /**< \brief input stride of RFC Chroma table data. */
  u32 in_luma_bitdepth;  /**< \brief input bitdepth of RFC Luma data. */
  u32 in_chroma_bitdepth;  /**< \brief input bitdepth of RFC Chroma data. */
  u8 in_rfc;  /**< \brief flag to indicate rfc input. */
  u8 in_tiled_mode;  /**< \brief flag to indicate dec400 input tiled mode. */
  u8 in_dec400;  /**< \brief flag to indicate dec400 input. */
  u8 in_dec400_a;  /**< \brief input dec400 align set.(32B/64B) */
  u8 in_chroma_format_idc;  /**< \brief 0 mono chrome; 1 yuv420; 2 yuv422*/
  DecPicAlignment align;
  struct DWLLinearMem pp_in_buffer; /**< \brief The buffer malloced for pp input */
  struct DWLLinearMem pp_out_buffer; /**< \brief The buffer malloced for pp output */
  struct DWLLinearMem table_3dlut_buffer;  /**< \brief The buffer malloced for color remapping (3dlut) */
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT]; /**< \brief A structure containing PP unit configuration information. */
  u32 misc_ctrl;
} PPConfig;


struct CropRegion{
  /** \brief Whether to enable crop output.
   *  \n Valid values:
   *  \n - <tt>0</tt>: disable.
   *  \n - <tt>1</tt>: enable. */
  u32 enabled;
  /** \brief The width in pixels of the output pictures of cropping region in memory. */
  u32 pic_width;
   /** \brief The height in pixels of the output pictures of cropping region in memory. */
  u32 pic_height;
   /** \brief The picture stride in bytes of each pixel line for the output luma data of cropping region. */
  u32 pic_stride;
   /** \brief The picture stride in bytes of each pixel line for the output chroma data of cropping region. */
  u32 pic_stride_ch;
   /** \brief A pointer to the output picture data of cropping region 1. */
  const u32 *output_picture;
   /** \brief The DMA bus address of the output picture buffer of cropping region 1. */
  addr_t output_picture_bus_address;
   /** \brief A pointer to the output chroma data of cropping region 1. */
  const u32 *output_picture_chroma;
   /** \brief The DMA bus address of the output chroma data buffer of cropping region 1. */
  addr_t output_picture_chroma_bus_address;
};

/**
 * \brief Structure to carry information about post pocessed pictures.
 * \ingroup common_group
 */
typedef struct PPDecPicture_ {
  struct PPOutputInfo {
    u32 pic_width;        /**< \brief pixels width of the picture as stored in memory */
    u32 pic_height;       /**< \brief pixel height of the picture as stored in memory */
    u32 pic_stride;       /**< \brief pic stride of each pixel line in bytes. */
    u32 pic_stride_ch;   /**< \brief pic stride of each pixel line in bytes for chroma. */
    const u32 *output_picture;  /**< \brief Pointer to the picture data */
    addr_t output_picture_bus_address;    /**< \brief DMA bus address of the output picture buffer */
    enum DecPictureFormat output_format; /**< \brief Storage format of output picture. */
    const u32 *output_picture_chroma;  /**< \brief Pointer to the chroma picture data */
    addr_t output_picture_chroma_bus_address;    /**< \brief DMA bus address of the output picture buffer */
    u32 bit_depth_luma;    /**< \brief The bit depth of chroma */
    u32 bit_depth_chroma;  /**< \brief The bit depth of luma */

    /** \brief The basic information of the different region output pictures. */
    struct CropRegion crop_region[3];

    struct DWLLinearMem dec400_luma_table;           /**< Buffer properties *//*sunny add for tile status address*/
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
  u32 cycles_per_mb; /**< \brief  Amount of consumed clock cycles returned to this register(swreg63) when interrupt is being made(any kind of interrupt) */
} PPDecPicture;
/*------------------------------------------------------------------------------
    Prototypes of PP API functions
------------------------------------------------------------------------------*/
/**
 * Initializes post processor.
 * \ingroup common_group
 * \param [in]     p_post_inst           Pointer to the location where this function returns a post processor instance.
 * \param [in]     dwl                   Pointer to decoder wrapper.
 * \return         DecRet                value: DEC_OK, DEC_PARAM_ERROR, DEC_MEMFAIL
 */
enum DecRet PPInit(PPInst *p_post_pinst, const void *dwl);

/**
 * This function is called by the client to set the post processor configuration.
 * \ingroup common_group
 * \param [in]     post_pinst            A post processor instance created earlier with a call to PPInit.
 * \param [in]     p_pp_conf             Pointer to post processor configuation.
 * \return         DecRet                value: DEC_OK, DEC_PARAM_ERROR, DEC_MEMFAIL
 */
enum DecRet PPSetInfo(PPInst post_pinst, PPConfig * p_pp_conf);

/**
 * This function is called to set up and run pp
 * \ingroup common_group
 * \param [in]     post_inst         A post processor instance created earlier with a call to PPInit.
 * \return         DecRet            value: DEC_OK, DEC_PARAM_ERROR, DEC_MEMFAIL, DEC_HW_TIMEOUT, DEC_HW_RESERVED
 */
enum DecRet PPDecode(PPInst post_pinst);

/**
 * Provides access to the next picture in display order.
 * \ingroup common_group
 * \param [in]     post_inst         A post processor instance created earlier with a call to PPInit.
 * \param [in]     output            Pointer to a PPDecPicture structure which used to return the picture parameters.
 * \return         DecRet            value: DEC_OK
 */
enum DecRet PPNextPicture(PPInst post_pinst, PPDecPicture *output);

/**
 * Releases the post processor instance.
 * Function used to release instance data and frees the memory allocated for the instance.
 * \ingroup common_group
 * \param [in]     inst           A post processor instance to be realesed.
 *                                The decoder instance to be released.
 * \return         void           None.
 */
void PPRelease(PPInst post_pinst);
#ifdef __cplusplus
}
#endif

#endif                       /* __PPAPI_H__ */
