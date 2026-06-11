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

#ifndef __VP8DECAPI_H__
#define __VP8DECAPI_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "ppu.h"
#include "dwl.h"

/*!\defgroup VP8API VP8 Decoder*/

/*! \addtogroup VP8API
 *  @{
 */

/*------------------------------------------------------------------------------
    API type definitions
------------------------------------------------------------------------------*/

/*!\enum VP8DecOutFormat_
 *  Decoder output picture format.
 *
 * \typedef VP8DecOutFormat
 * A typename for #VP8DecOutFormat_.
 */
typedef enum VP8DecOutFormat_ {
  VP8DEC_SEMIPLANAR_YUV420 = 0x020001, /**<\hideinitializer */
  VP8DEC_TILED_YUV420 = 0x020002 /**<\hideinitializer */
} VP8DecOutFormat;

/*!\struct VP8DecInput_
     * \brief Decode input structure
     *
     * \typedef VP8DecInput
     * A typename for #VP8DecInput_.
     */
/* Output structure */

typedef struct VP8DecInput_ {
  const u8 *stream;   /**< Pointer to the input buffer */
  addr_t stream_bus_address; /**< DMA bus address of the input buffer */
  u32 data_len;          /**< Number of bytes to be decoded */
  u32 pic_id;
  u32 slice_height;     /**< height of WebP slice, unused for other formats */
  u32 *p_pic_buffer_y;    /**< luminance output address of user allocated buffer,
                             used in conjunction with external buffer allocation */
  addr_t pic_buffer_bus_address_y; /**< DMA bus address for luminance output */
  u32 *p_pic_buffer_c;    /**< chrominance output address of user allocated buffer,
                             used in conjunction with external buffer allocation */
  addr_t pic_buffer_bus_address_c; /**< DMA bus address for luminance output */
  void *p_user_data; /**< user data to be passed in multicore callback,
                             used in conjunction with multicore decoding */
  u32 dec_ctrl;
} VP8DecInput;

#define VP8_SCALE_MAINTAIN_ASPECT_RATIO     0 /**<\hideinitializer */
#define VP8_SCALE_TO_FIT                    1 /**<\hideinitializer */
#define VP8_SCALE_CENTER                    2 /**<\hideinitializer */
#define VP8_SCALE_OTHER                     3 /**<\hideinitializer */

#define VP8_STRIDE_NOT_USED                 0 /**<\hideinitializer */

/*!\struct VP8DecInfo_
 * \brief Decoded stream information
 *
 * A structure containing the decoded stream information, filled by
 * VP8DecGetInfo()
 *
 * \typedef VP8DecInfo
 * A typename for #VP8DecInfo_.
 */
/* stream info ddfilled by VP8DecGetInfo */
typedef struct VP8DecInfo_ {
  u32 vp_version;       /**< VP codec version defined in input stream */
  u32 vp_profile;       /**< VP cocec profile defined in input stream */
  u32 pic_buff_size;
  u32 coded_width;      /**< coded width of the picture */
  u32 coded_height;     /**< coded height of the picture */
  u32 frame_width;      /**< pixels width of the frame as stored in memory */
  u32 frame_height;     /**< pixel height of the frame as stored in memory */
  u32 scaled_width;     /**< scaled width of the displayed video */
  u32 scaled_height;    /**< scaled height of the displayed video */
  enum DecDpbMode dpb_mode;             /**< DPB mode; frame, or field interlaced */
  VP8DecOutFormat output_format;   /**< format of the output frame */
} VP8DecInfo;

/*!\struct VP8DecPicture_
 * \brief Decoded picture information
 *
 * Parameters of a decoded picture, filled by VP8DecNextPicture().
 *
 * \typedef VP8DecPicture
 * A typename for #VP8DecPicture_.
 */
typedef struct VP8DecPicture_ {
  u32 pic_id;           /**< Identifier of the Frame to be displayed */
  u32 decode_id;
  u32 is_intra_frame;    /**< Indicates if Frame is an Intra Frame */
  u32 is_golden_frame;   /**< Indicates if Frame is a Golden reference Frame */
  u32 nbr_of_err_mbs;     /**< Number of concealed macroblocks in the frame  */
  u32 error_info;
  u32 error_ratio;
  u32 num_slice_rows;    /**< Number of luminance pixels rows in WebP output picture buffer.
                             If set to 0, whole picture ready.*/
  u32 last_slice;       /**< last slice flag  */
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
  struct VP8OutputInfo {
    u32 coded_width;      /**< coded width of the picture */
    u32 coded_height;     /**< coded height of the picture */
    u32 frame_width;      /**< pixels width of the frame as stored in memory */
    u32 frame_height;     /**< pixel height of the frame as stored in memory */
    u32 luma_stride;      /**< pixel row stride for luminance */
    u32 chroma_stride;    /**< pixel row stride for chrominance */
    const u32 *p_output_frame;    /**< Pointer to the frame */
    addr_t output_frame_bus_address;  /**< DMA bus address of the output frame buffer */
    const u32 *p_output_frame_c;   /**< Pointer to chrominance output */
    addr_t output_frame_bus_address_c;  /**< DMA bus address of the chrominance
                                      *output frame buffer */
    u32 pic_stride;
    u32 pic_stride_ch;
    enum DecPictureFormat output_format;
    struct DecCropParams crop_params; /**< Cropping parameters for the picture */
    struct DWLLinearMem dec400_luma_table;           /**< Buffer properties *//*sunny add for tile status address*/
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT];
} VP8DecPicture;

/*!\struct VP8DecPictureBufferProperties_
 * \brief Decoded picture information
 *
 * Parameters of a decoded picture, filled by VP8DecNextPicture().
 *
 * \typedef VP8DecPictureBufferProperties
 * A typename for #VP8DecPictureBufferProperties_.
 */
typedef struct VP8DecPictureBufferProperties_ {
  u32 luma_stride; /**< Specifies stride for luminance buffer in bytes.
                         *Stride must be a power of 2.  */
  u32 chroma_stride; /**< Specifies stride for chrominance buffer in
                           *bytes. Stride must be a power of 2. */

  u32 **p_pic_buffer_y;    /**< Pointer to luma buffers */
  addr_t *pic_buffer_bus_address_y;  /**< DMA bus address of the luma buffers */
  u32 **p_pic_buffer_c;    /**< Pointer to chroma buffers */
  addr_t *pic_buffer_bus_address_c;  /**< DMA bus address of the chroma buffers */
  u32 num_buffers; /**< Number of buffers supplied in the above arrays.
                         * Minimum value is 4 and maximum 16 */

} VP8DecPictureBufferProperties;

struct VP8DecConfig {
  enum DecErrorHandling error_handling;
  u32 error_ratio;
  u32 use_adaptive_buffers; // When sequence changes, if old output buffers (number/size) are sufficient for new sequence,
  // old buffers will be used instead of reallocating output buffer.
  u32 guard_size;       // The minimum difference between minimum buffers number and allocated buffers number
  // that will force to return HDRS_RDY even buffers number/size are sufficient
  // for new sequence.
  DecPicAlignment align;
  enum DecInputFormat dec_format;
  u32 num_frame_buffers;
  PpUnitConfig ppu_config[DEC_MAX_OUT_COUNT];
  struct DWLLinearMem table_3dlut_buffer;  /* The buffer used for color remapping (3dlut) */
  enum DecDecoderMode decoder_mode;
  u32 misc_ctrl;
};

/* Decoder instance */
// typedef const void *VP8DecInst;
typedef void *VP8DecInst;

/*------------------------------------------------------------------------------
    Prototypes of Decoder API functions
------------------------------------------------------------------------------*/

/*!\brief Create a single Core decoder instance
 *
 * Single Core decoder can decode VP7 and WebP stream in addition to VP8.
 *
 * Every instance has to be released with VP8DecRelease().
 *
 *\note Use VP8DecMCInit() for creating an instance with multicore support.
 *
 */
enum DecRet VP8DecInit(VP8DecInst *dec_inst, const void *dwl, struct VP8DecConfig *dec_cfg);

/*!\brief Setup custom frame buffers.
 *
 */
enum DecRet VP8DecSetPictureBuffers(VP8DecInst dec_inst,
                                  VP8DecPictureBufferProperties *p_pbp);

/*!\brief Release a decoder instance
 *
 * VP8DecRelease closes the decoder instance \c dec_inst and releases all
 * internally allocated resources.
 *
 * When connected with the Hantro HW post-processor, the post-processor
 * must be disconnected before releasing the decoder instance.
 * Refer to \ref PPDOC "PP API manual" for details.
 *
 * \param dec_inst instance to be released
 *
 * \retval #DEC_OK for success
 * \returns A negative return value signals an error.
 *
 * \sa VP8DecInit()
 * \sa VP8DecMCInit()
 *
 */

void VP8DecRelease(VP8DecInst dec_inst);

/*!\brief Decode data
 *
 * \warning Do not use with multicore instances!
 *       Use instead VP8DecMCDecode().
 *
 */

/* Single Core specific */

enum DecRet VP8DecDecode(VP8DecInst dec_inst,
                       const VP8DecInput *input,
                       struct DecOutput *output);

/*!\brief Read next picture in display order
 *
 * \warning Do not use with multicore instances!
 *       Use instead VP8DecMCNextPicture().
 *
 * \sa VP8DecMCNextPicture()
 */
enum DecRet VP8DecNextPicture(VP8DecInst dec_inst, VP8DecPicture *picture);

enum DecRet VP8DecPictureConsumed(VP8DecInst dec_inst,
                                const VP8DecPicture *picture);

enum DecRet VP8DecEndOfStream(VP8DecInst dec_inst);

/*!\brief Read decoded stream information
 *
 */
enum DecRet VP8DecGetInfo(VP8DecInst dec_inst, VP8DecInfo *dec_info);

/*!\brief Read last decoded picture
 *
 * \warning Do not use with multicore instances!
 *       Use instead VP8DecNextPicture().
 *
 * \sa VP8DecMCNextPicture()
 */
enum DecRet VP8DecPeek(VP8DecInst dec_inst, VP8DecPicture *picture);
enum DecRet VP8DecGetBufferInfo(VP8DecInst dec_inst, struct DecBufferInfo *mem_info);

enum DecRet VP8DecAddBuffer(VP8DecInst dec_inst, struct DWLLinearMem *info);

enum DecRet VP8DecAbort(VP8DecInst dec_inst);

enum DecRet VP8DecAbortAfter(VP8DecInst dec_inst);

enum DecRet VP8DecSetInfo(VP8DecInst dec_inst, struct VP8DecConfig *dec_cfg);
/* Multicore extension */

/*!\brief Read number of HW cores available
 *
 * Multicore specific.
 *
 * Static implementation, does not require a decoder instance.
 *
 * \returns The number of available hardware decoding cores.
 */
u32 VP8DecMCGetCoreCount(void);

/*!\brief Create a multicore decoder instance
 *
 * Multicore specific.
 *
 * Use this to initialize a new multicore decoder instance. Only VP8 format
 * is supported.
 *
 *
 * Every instance has to be released with VP8DecRelease().
 *
 * \param dec_inst pointer where the newly created instance will be stored.
 * \param p_mcinit_cfg initialization parameters, which  cannot be altered later.
 *
 * \retval #DEC_OK for success
 * \returns A negative return value signals an error.
 *
 * \sa VP8DecRelease()
 */
enum DecRet VP8DecMCInit(VP8DecInst *dec_inst,
                       const void *dwl,
                       struct DecMCConfig *p_mcinit_cfg,
                       struct DecConfig *dec_cfg);

/*!\brief Decode data
 *
 * Multicore specific.
 * This function decodes a frame from the current stream.
 * The input buffer shall contain the picture data for exactly one frame.
 *
 * \retval #DEC_PIC_DECODED
            A new picture decoded.
 * \retval #DEC_HDRS_RDY
 *          Headers decoded and activated. Stream header information is now
 *          readable with the function #VP8DecGetInfo.
 * \returns A negative return value signals an error.
 */
enum DecRet VP8DecMCDecode(VP8DecInst dec_inst,
                         const VP8DecInput *input,
                         struct DecOutput *output);

/*!\brief Release picture buffer back to decoder
 *
 * Multicore specific.
 *
 * Use this function to return a picture back to the decoder once
 * consumed by application. Pictures are given to application by
 * VP8DecMCNextPicture().
 *
 * \param dec_inst a multicore decoder instance.
 * \param picture pointer to data that identifies the picture buffer.
 *        Shall be the exact data returned by VP8DecMCNextPicture().
 *
 * \retval #DEC_OK for success
 * \returns A negative return value signals an error.
 *
 * \sa  VP8DecMCNextPicture()
 */
enum DecRet VP8DecMCPictureConsumed(VP8DecInst dec_inst,
                                  const VP8DecPicture *picture);

/*!\brief Get next picture in display order
 *
 * Multicore specific.
 *
 * This function is used to get the decoded pictures out from the decoder.
 * The call will block until a picture is available for output or
 * an end-of-stream state is set in the decoder. Once processed, every
 * output picture has to be released back to decoder by application using
 * VP8DecMCPictureConsumed(). Pass to this function the same data returned
 * in \c picture.
 *
 * \param dec_inst a multicore decoder instance.
 * \param picture pointer to a structure that will be filled with
 *        the output picture parameters.
 *
 * \retval #DEC_PIC_RDY to signal that a picture is available.
 * \retval #DEC_END_OF_STREAM to signal that the decoder is
 *          in end-of-stream state.
 * \returns A negative return value signals an error.
 *
 *
 * \sa VP8DecMCPictureConsumed()
 * \sa VP8DecMCEndOfStream()
 *
 */
enum DecRet VP8DecMCNextPicture(VP8DecInst dec_inst,
                              VP8DecPicture *picture);
/*!\brief Set end-of-stream state in multicore decoder
 *
 * Multicore specific.
 *
 * This function is used to signal the decoder that the current decoding process ends.
 *  It must be called at the end of decoding so that all potentially
 * buffered pictures are flushed out and VP8DecNextPicture()
 * is unblocked.
 *
 * This call will block until all cores have finished processing and all
 * output pictures are processed by application
 * (i.e. VP8DecNextPicture() returns #DEC_END_OF_STREAM).
 *
 * \param dec_inst a multicore decoder instance.
 *
 * \retval #DEC_OK for success
 * \returns A negative return value signals an error.
 *
 * \sa  VP8DecMCNextPicture()
 */

enum DecRet VP8DecMCEndOfStream(VP8DecInst dec_inst);

/*!\brief API internal tracing function prototype
*
* Traces all API entries and returns. This must be implemented by
* the application using the decoder API.
*
* \param string Pointer to a NULL terminated char string.
*
*/
void VP8DecTrace(const char *string);

/*! @}*/
#ifdef __cplusplus
}
#endif

#endif                       /* __VP8DECAPI_H__ */
