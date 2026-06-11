/*------------------------------------------------------------------------------
--       Copyright (c) 2015, VeriSilicon Inc. All rights reserved             --
--         Copyright (c) 2011-2014, Google Inc. All rights reserved.          --
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

#ifndef AV1DECAPI_H
#define AV1DECAPI_H

#include "basetype.h"
#include "decapicommon.h"
#include "dectypes.h"
#include "commonfunction.h"

#ifdef __cplusplus
extern "C" {
#endif

/*------------------------------------------------------------------------------
    API type definitions
------------------------------------------------------------------------------*/
/**
 * \brief AV1 Decoder instance
 * \ingroup av1_group
 */
typedef const void *Av1DecInst;

enum DecBitDepth {
  DEC_BITDEPTH_INITIAL = 0, /**< Match the bit depth of the first frame over
                               the whole sequence. */
  DEC_BITDEPTH_NATIVE = 1, /**< Match the bit depth of the input stream. */
  DEC_BITDEPTH_8 = 8,      /**< If over 8 bit deep input, downsample to 8. */
  DEC_BITDEPTH_10 = 10 /**< If over 10 bit deep input, downsample to 10. */
};
enum DecBitPacking {
  DEC_BITPACKING_LSB = 0, /**< Pack the pixel into the LSBs of each sample. */
  DEC_BITPACKING_MSB
};

/* Post-processor features. */
struct Av1PpConfig {
  enum DecBitDepth bit_depth;
  enum DecBitPacking bit_packing;
};


/* Secondary-output config. */
struct Av1SecondaryOutConfig {
//    u32 secondary_output_e;
//    u32 secondary_output_hbd;
//    u32 secondary_output_format;
//    u32 secondary_output_tiled;
    PpUnitConfig ppu_cfg[DEC_MAX_OUT_COUNT];
};

/**
 * \struct Av1DecInput
 * \brief the AV1 decoder input.
 * \ingroup av1_group
 */
struct Av1DecInput {
  u8 *stream;             /**< Pointer to the input */
  addr_t stream_bus_address; /**< DMA bus address of the input stream */
  u32 data_len;           /**< Number of bytes to be decoded */
  u32 frame_len;          /* temp data, real data len for current frame */
  u8 *buffer;             /**< Pointer to the input stream buffer */
  addr_t buffer_bus_address; /**< DMA bus address of the input stream buffer */
  u32 buff_len;           /**< Stream buffer byte length         */
  u32 pic_id;             /**< Identifier for the picture to be decoded */
  void *usr_ptr;          /**< Pointer to user data */
  u32 use_multicore;      /**< Flag to indicate whether multi-core is enabled */
  u32 dec_ctrl;
};

/**
 * \struct Av1DecInfo
 * \brief stream info filled by Av1DecGetInfo.
 * \ingroup av1_group
 */
struct Av1DecInfo {
  u32 vp_version;    /**< AV1 version number */
  u32 vp_profile;    /**< AV1 profile */
  u32 bit_depth;     /**< Bit depth */
  u32 coded_width;   /**< coded width */
  u32 coded_height;  /**< coded height */
  u32 superres_width; /**< filtered width when superres is used */
  bool monochrome;   /**< Flag to indicate whether it’s a monochrome bitstream */
  u32 frame_width;   /**< pixels width of the frame as stored in memory */
  u32 frame_height;  /**< pixel height of the frame as stored in memory */
  u32 max_width;
  u32 max_height;
  u32 scaled_width;  /**< scaled width of the displayed video */
  u32 scaled_height; /**< scaled height of the displayed video */
  u32 dpb_mode;      /**< DPB mode; frame, or field interlaced */
  u32 pic_buff_size; /**< number of picture buffers allocated&used by decoder */
  u32 multi_buff_pp_size; /**< number of picture buffers needed in
                               decoder+postprocessor multibuffer mode */
  /* time scale info */
  u32 timing_info_present_flag;
  u32 num_units_in_tick;
  u32 time_scale;
  u32 colour_description_present_flag;
  u32 colour_primaries;
  u32 transfer_characteristics;
  u32 matrix_coefficients;
};

/**
 * \struct Av1DecConfig
 * \brief Pp units configuration, used by Av1DecSetInfo.
 * \ingroup av1_group
 */
struct Av1DecConfig {
  /** Error handling mode. */
  enum DecErrorHandling error_handling;
  /** Error ratio, used for DEC_EC_OUT_DECISION. */
  u32 error_ratio;
  /** HW can continue decoding if stream error detected. */
  u32 hw_conceal;
  /** Indicates the number of reference frame buffers. */
  u32 num_frame_buffers;
  /** Flag to indicate whether reference frame compression is enabled or not. */
  u32 use_video_compressor;
  /** Specifies fixed scaling enabled. */
  /** Specifies the PP buffer or reference buffer stride alignment. */
  DecPicAlignment align;
  /** Specifies the input stream of decoder contains at least two frames. */
  u32 multi_frame_input_flag;
  /** decoder mode. */
  enum DecDecoderMode decoder_mode;
  /** When the sequence changes, if old output buffers (number/size) are sufficient for the
   * new sequence, old buffers will be reused instead of reallocating new output buffers. */
  u32 use_adaptive_buffers;
  /** The minimum difference between the minimum buffer number and the allocated buffer
   * number that will force to return HDRS_RDY even if buffer number/buffer size are
   * sufficient for the new sequence */
  u32 guard_size;
  /** Flag to indicate whether the stream complies with Annex B. */
  u32 annexb;
  /** Flag to indicate whether the stream is packetized in plain OBUs. */
  u32 plainobu;
  /** Number of clock cycles to wait between multi-core sync polls. */
  u32 multicore_poll_period;
  /** The tile decoding order, 0: original stream order (raster order), 1: transposed order
   * (The parameter is obsolete) */
  u32 tile_transpose;
  /** Decode all operating points. */
  u32 oppoints;
  /** Specifies the PP configuration for each PP channel. A total of DEC_MAX_OUT_COUNT
   * PP channels are supported. */
  PpUnitConfig ppu_cfg[DEC_MAX_OUT_COUNT];
  /**The buffer used for color remapping (3dlut) */
  struct DWLLinearMem table_3dlut_buffer;
  /** Specifies the multi-core is enabled or not. */
  u32 mc_enable;
  u32 misc_ctrl;
};

/**
 * \struct Av1DecPicture
 * \brief Output structure for Av1DecNextPicture.
 * \ingroup av1_group
 */
struct Av1DecPicture {
  u32 coded_width;  /**< coded width of the picture */
  u32 coded_height; /**< coded height of the picture */
  u32 superres_width; /**< filtered width when superres is used */
  u32 frame_width;  /**< pixels width of the frame as stored in memory */
  u32 frame_height; /**< pixel height of the frame as stored in memory */
  void * usr_ptr;   /**< Pointer to user data */
  unsigned char fc_modes[2][8]; /**< Frame compression modes */
  u32 fc_num_tiles_col;         /**< Number of column tiles, needed to parse compression info */
  u32 pic_id;                    /**< Identifier of the Frame to be displayed */
  u32 decode_id;                    /**< Identifier of the Frame to be decoded */
  u32 is_intra_frame;            /**< Indicates if Frame is an Intra Frame */
  u32 intra_only; /**< Flag to indicate if the frame is an intra only frame or not. Possible
              values: 0--frame is not an intra only frame, 1--frame is an intra only frame. */
  u32 fill_chroma;
  u32 apply_grain;     /**< Indicates if film grain is applied */
  u32 is_golden_frame; /**< Indicates if Frame is a Golden reference Frame */
  u32 error_ratio;     /**< The ratio of error SBs to whole picture SBs.(100 times) */
  enum DecErrorInfo error_info; /**< Indicates that picture is error */
  u32 num_slice_rows;  /**< Indicate if chroma pixels need to be filled */
  u32 cycles_per_mb;   /**< Avarage cycle count per macroblock */
#ifdef FPGA_PERF_AND_BW
  u32 bitrate_4k60fps;  /**< The bitrate of frame*/
  u32 bwrd_in_fs;    /**< Read bandwidth of frame*/
  u32 bwwr_in_fs;    /**< Write bandwidth of frame*/
#endif
  u32 bits_per_sample; /**< Bit depth of the luma & chroma component */
  enum DecColorSpace color_space; /**< Obsolete */
  enum DecVideoRange color_range; /**< Obsolete */
  u32 frame_offset; /**< The order hint of INTRA_FRAME */
  u32 lst_frame_offset; /**< The order hint of LAST_FRAME */
  u32 lst2_frame_offset; /**< The order hint of LAST2_FRAME */
  u32 lst3_frame_offset; /**< The order hint of LAST3_FRAME */
  u32 gld_frame_offset; /**< The order hint of GOLDEN_FRAME */
  u32 bwd_frame_offset; /**< The order hint of BWDREF_FRAME */
  u32 alt2_frame_offset; /**< The order hint of ALTREF2_FRAME */
  u32 alt_frame_offset; /**< The order hint of ALTREF_FRAME */
  const u32 *output_rfc_luma_base;   /**< Pointer to the rfc table */
  addr_t output_rfc_luma_bus_address; /**< DMA bus address of the luma RFC table */
  const u32 *output_rfc_chroma_base; /**< Pointer to the rfc chroma table */
  addr_t output_rfc_chroma_bus_address; /**< DMA bus address of the chroma RFC table */
  struct MetadataParameters *metadata_param; /**< metadata parameter data */
  struct Av1OutputInfo {
    u32 frame_width;  /**< pixels width of the frame as stored in memory */
    u32 frame_height; /**< pixel height of the frame as stored in memory */
    const u32 *output_luma_base;   /**< Pointer to the picture */
    addr_t output_luma_bus_address;   /**< Bus address of the luminance component */
    const u32 *output_chroma_base; /**< Pointer to the picture */
    addr_t output_chroma_bus_address;   /**< Bus address of the luminance component */
    enum DecPictureFormat output_format; /**< Decoder’s output YUV format. Possible values:
                                DEC_OUT_FRM_TILED_4X4, DEC_OUT_FRM_RASTER_SCAN, DEC_OUT_FRM_PLANAR_420 */
    u32 pic_stride;       /**< Byte width of the picture as stored in memory */
    u32 pic_stride_ch; /**< Chroma stride of the picture (in bytes) as stored in memory */
    u32 out_bit_depth; /**< Bit depth of output picture */
    struct DWLLinearMem dec400_luma_table;
    struct DWLLinearMem dec400_chroma_table;
  } pictures[DEC_MAX_OUT_COUNT]; /**< Array of DEC_MAX_OUT_COUNT pictures defined by Av1OutputInfo. */
};

/**
 * \brief Initializes decoder software. \n
 * Function reserves memory for the decoder instance and calls Av1Init to initialize the instance data.
 * \ingroup av1_group
 * \param [in]     dec_inst       Pointer to the location where this function returns a decoder instance.
                                  This instance is later passed to other decoder API functions.
 * \param [in]     dwl            Pointer to a DWL structure.
                                  Refer to the Hantro DWL API for Video Decoders document for more information.
 * \param [in]     dec_cfg        Pointer to the decoder initialization parameters (Av1DecConfig).
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_MEMFAIL, DEC_FORMAT_NOT_SUPPORTED,
 *                                , DEC_SYSTEM_ERROR
 */
enum DecRet Av1DecInit(Av1DecInst *dec_inst, const void *dwl, struct Av1DecConfig *dec_cfg);

/**
 * \brief Releases the decoder instance. \n
 * Function calls Av1ShutDown to release instance data and frees the memory allocated for the instance.
 * \ingroup av1_group
 * \param [in]     dec_inst       The decoder instance to be released. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [out]    void           None.
 */
void Av1DecRelease(Av1DecInst dec_inst);

/**
 * \brief This function decodes one or more NAL units from the current stream. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     input          Pointer to the decoder input structure Av1DecInput.
 * \param [in]     output         Pointer to the decoder output structure DecOutput
 * \param [out]    DecRet         value: DEC_PIC_DECODED, DEC_HDRS_RDY, DEC_END_OF_STREAM, DEC_PARAM_ERROR,
 *                                DEC_STRM_ERROR, DEC_NOT_INITIALIZED, DEC_HW_BUS_ERROR, DEC_HW_TIMEOUT,
 *                                DEC_MEMFAIL, DEC_STREAM_NOT_SUPPORTED, DEC_NONREF_PIC_SKIPPED,
 *                                DEC_WAITING_FOR_BUFFER, DEC_ABORTED, DEC_STRM_PROCESSED, DEC_INFOPARAM_ERROR
 */
enum DecRet Av1DecDecode(Av1DecInst dec_inst, const struct Av1DecInput *input,
                         struct DecOutput *output);

/**
 * \brief Provides access to the next picture in display order. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     picture        Pointer to an Av1DecPicture structure used to return the picture parameters.
 *                                The picture parameters are valid only when the return value indicates that an
 *                                output picture is available.
 * \param [out]    DecRet         value: DEC_OK, DEC_PIC_RDY, DEC_PARAM_ERROR, DEC_NOT_INITIALIZED,
 *                                DEC_END_OF_STREAM, DEC_ABORTED, DEC_FLUSHED
 */
enum DecRet Av1DecNextPicture(Av1DecInst dec_inst,
                              struct Av1DecPicture *output);

/**
 * \brief This function informs the decoder that the client has finished processing the specific
 * picture and releases the picture buffer for the decoder to process the next picture. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     picture        Pointer to an Av1DecPicture structure used to return the picture parameters.
 *                                The picture parameters are valid only when the return value indicates that an
 *                                output picture is available.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR
 */
enum DecRet Av1DecPictureConsumed(Av1DecInst dec_inst,
                                  const struct Av1DecPicture *picture);

/**
 * \brief This function informs the decoder that it should not be expecting any more input stream and to
 * finish decoding and outputting all the buffers that are currently pending in the component. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_END_OF_STREAM
 */
enum DecRet Av1DecEndOfStream(Av1DecInst dec_inst);

/**
 * \brief This function provides read access to decoder information. This function should not
 * be called before Av1DecDecode function has indicated that headers are ready. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     dec_info       Pointer to an Av1DecInfo structure where the decoder information will be returned.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_HDRS_NOT_RDY, DEC_NOT_INITIALIZED
 */
enum DecRet Av1DecGetInfo(Av1DecInst dec_inst, struct Av1DecInfo *dec_info);

/**
 * \brief This function is called by the client to set the decoder configuration. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     dec_cfg        Pointer to the decoder configure parameters (Av1DecConfig).
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_HDRS_NOT_RDY, DEC_NOT_INITIALIZED
 */
enum DecRet Av1DecSetInfo(Av1DecInst dec_inst, struct Av1DecConfig *dec_cfg);

/**
 * \brief This function is called by the client to add an external frame buffer into the decoder. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     info           Pointer to a DWLLinearMem structure.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_NOT_INITIALIZED
 */
enum DecRet Av1DecAddBuffer(Av1DecInst dec_inst, struct DWLLinearMem *info);

/**
 * \brief This function is called by the client to get the frame buffer information
 * requested by the decoder, including the requested buffer size and buffer numbers. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [in]     mem_info       Pointer to a DecBufferInfo structure.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_EXT_BUFFER_REJECTED, DEC_WAITING_FOR_BUFFER
 */
enum DecRet Av1DecGetBufferInfo(Av1DecInst dec_inst, struct DecBufferInfo *mem_info);

/**
 * \brief This function is called by the client to abort the decoder. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR, DEC_WAITING_FOR_BUFFER
 */
enum DecRet Av1DecAbort(Av1DecInst dec_inst);

/**
 * \brief This function is called by the client to reset the decoder after the decoder is aborted. \n
 * \ingroup av1_group
 * \param [in]     dec_inst       A decoder instance created earlier with a call to Av1DecInit. \n
                                  Av1DecInst is defined as: typedef const void *Av1DecInst.
 * \param [out]    DecRet         value: DEC_OK, DEC_PARAM_ERROR
 */
enum DecRet Av1DecAbortAfter(Av1DecInst dec_inst);

void Av1DecUpdateStrmInfoCtrl(Av1DecInst dec_inst, struct strmInfo info);

void Av1DecUpdateTileInfo(Av1DecInst dec_inst);

#ifdef __cplusplus
}
#endif

#endif /* AV1DECAPI_H */
