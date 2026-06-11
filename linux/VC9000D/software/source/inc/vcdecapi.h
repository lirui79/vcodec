/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2019 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#ifndef VCDECAPI_H
#define VCDECAPI_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "basetype.h"
#include "dectypes.h"
#include "dwl.h"

/*------------------------------------------------------------------------------
VCDecAPI : used in decapi.c or testbench
------------------------------------------------------------------------------*/
/** Returns the API version of the video decoder.
 *
 *  You can call this function either before or after decoder instance intialization.
 *
 *  This function requires no parameters.
 *  \ingroup common_group
 *  \return         The API version. */
struct DecApiVersion VCDecGetAPIVersion(void);

/** Returns the hardware and software build information of a client type.
 *
 *  You can call this function either before or after decoder instance initialization.
 *  \ingroup common_group
 *  \param [in]     dwl_inst              The DWL instance.
 *  \param [in]     client_type           The client type.
 *  \return         The build information. */
struct DecSwHwBuild VCDecGetBuild(const void *dwl_inst, u32 client_type);

/** Returns the number of available decoder hardware cores.
 *
 *  You can call this function either before or after decoder instance intialization.
 *  \ingroup common_group
 *  \param [in]     dwl_inst              The DWL instance.
 *  \return         The number of available hardware decoder cores. */
u32 VCDecMCGetCoreCount(const void *dwl_inst); /* new, special one */

/** (For VC-1 only) Unpacks metadata elements for sequence headers from the buffer when the
 *  metadata is packed according to Annex J in SMPTE ST 421.
 *
 *  You can call this function either before or after decoder instance intialization.
 *  \ingroup common_group
 *  \param [in]     codec          The decoder type.
 *  \param [in]     p_buffer       A pointer to the buffer that contains the packed metadata.
 *                                 \n The buffer must contain at least 4 bytes.
 *  \param [in]     buffer_size    The size of the buffer, in bytes.
 *  \param [in]     p_meta_data    A pointer to the stream metadata container.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>, or
 *                  <tt> \ref DEC_METADATA_FAIL</tt>. */
enum DecRet VCDecUnpackMetaData(enum DecCodec codec, const u8 *p_buffer, u32 buffer_size,
                                struct DecMetaData* p_meta_data);

/** Initializes a decoder instance.
 *
 *  Before instance initialization, make sure that the DWL layer and kernel driver are ported
 *  to your platform.
 *
 *  The decoder instance must be freed using \c VCDecRelease() when it is no longer needed.
 *  \ingroup common_group
 *  \param [in,out] inst           A pointer to the \c DecInitConfig structure for receiving the
 *                                 initialized decoder instance.
 *  \param [in]     dec_cfg        A pointer to the configurations for initialization of the
 *                                 decoder instance.
 *  \return          <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                   <tt> \ref DEC_MEMFAIL</tt>, or <tt> \ref DEC_FORMAT_NOT_SUPPORTED</tt>. */
enum DecRet VCDecInit(const void** inst, struct DecInitConfig *init_config);

/** Gets the information about the video sequence.
 *
 *  This function is available only after a \c VCDecDecode() call returns
 *  <tt> \ref DEC_HDRS_RDY</tt>.
 *
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be queried.
 *  \param [in,out] info           A pointer to the \c DecSequenceInfo structure for receiving the
 *                                 video sequence information.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                  <tt> \ref DEC_HDRS_NOT_RDY</tt>, or \n <tt> \ref DEC_NOT_INITIALIZED</tt>. */
enum DecRet VCDecGetInfo(void* inst, struct DecSequenceInfo* info);

/** Configures a decoder instance.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be configured.
 *  \param [in]     config         A pointer to the decoder configurations.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_NOT_INITIALIZED</tt>, or
 *                  <tt> \ref DEC_PARAM_ERROR</tt>. */
enum DecRet VCDecSetInfo(void* inst, struct DecConfig *config);

/** Decodes one or more NAL units from the current stream.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to decode the stream.
 *  \param [in]     output         A pointer to the decoder output.
 *  \param [in]     param          A pointer to the input parameters.
 *  \return         <tt> \ref DEC_PIC_DECODED</tt>, <tt> \ref DEC_HDRS_RDY</tt>,
 *                  <tt> \ref DEC_PARAM_ERROR</tt>, <tt> \ref DEC_STRM_ERROR</tt>,
 *                  <tt> \ref DEC_NOT_INITIALIZED</tt>, <tt> \ref DEC_HW_BUS_ERROR</tt>,
 *                  <tt> \ref DEC_HW_TIMEOUT</tt>, <tt> \ref DEC_MEMFAIL</tt>,
 *                  <tt> \ref DEC_STREAM_NOT_SUPPORTED</tt>, <tt> \ref DEC_NONREF_PIC_SKIPPED</tt>,
 *                  <tt> \ref DEC_WAITING_FOR_BUFFER</tt>, <tt> \ref DEC_ABORTED</tt>,
 *                  <tt> \ref DEC_STRM_PROCESSED</tt>, <tt> \ref DEC_SYSTEM_ERROR</tt>,
 *                  <tt> \ref DEC_HW_RESERVED</tt>, <tt> \ref DEC_PENDING_FLUSH</tt>,
 *                  <tt> \ref DEC_NO_DECODING_BUFFER</tt>, or <tt> \ref DEC_BUF_EMPTY</tt>. */
enum DecRet VCDecDecode(void* inst, struct DecOutput* output, struct DecInputParameters* param);

/** Gets the next picture in display order.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \param [in,out] pic            A pointer to the \c DecPictures structure for receiving the
 *                                 picture information.
 *                                 \n The picture information is valid only if the return value
 *                                 indicates that an output picture is available.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PIC_RDY</tt>,
 *                  <tt> \ref DEC_PARAM_ERROR</tt>, <tt> \ref DEC_NOT_INITIALIZED</tt>,
 *                  <tt> \ref DEC_END_OF_STREAM</tt>, <tt> \ref DEC_ABORTED</tt>,
 *                  or <tt> \ref DEC_FLUSHED</tt>. */
enum DecRet VCDecNextPicture(void* inst, struct DecPictures* pic);

/** Informs the video decoder that the client has finished processing a picture and releases the
 *  picture buffer space for the decoder to process the next picture.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \param [in,out] pic            A pointer to the \c DecPictures structure for receiving the
 *                                 picture information.
 *                                 \n The picture information is valid only if the return value
 *                                 indicates that an output picture is available.
 * \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                 or <tt> \ref DEC_NOT_INITIALIZED</tt> */
enum DecRet VCDecPictureConsumed(void* inst, struct DecPictures *pic);

/** Informs the video decoder of the end of a stream.
 *
 *  The decoder then finishes decoding the pending pictures and the application needs to call
 *  \c VCDecNextPicture() to get the decoded pictures.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                  or <tt> \ref DEC_INITFAIL</tt>. */
enum DecRet VCDecEndOfStream(void* inst);

/** Gets the information of frame buffers requested by the video decoder, including the buffer
 *  size and buffer count.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be queried.
 *  \param [in]     buf_info       A pointer to the \c DecBufferInfo structure for receiving the
 *                                 buffer information.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                  or <tt> \ref DEC_WAITING_FOR_BUFFER</tt>. */
enum DecRet VCDecGetBufferInfo(void *inst, struct DecBufferInfo *buf_info);

/** Adds an external frame buffer to the video decoder.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \param [in]     buf            A pointer to the buffer to be added.
 *  \return         <tt> \ref DEC_PARAM_ERROR</tt>, <tt> \ref DEC_EXT_BUFFER_REJECTED</tt>,
 *                  or \n <tt> \ref DEC_WAITING_FOR_BUFFER</tt>. */
enum DecRet VCDecAddBuffer(void *inst, struct DWLLinearMem *buf);

/** Specifies the number of extra required frames in DPB.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \param [in]     n              The number of extra required frames in DPB.
 *  \return         <tt> \ref DEC_OK</tt>. */
enum DecRet VCDecUseExtraFrmBuffers(void* inst, u32 n);

/** Gets the last decoded picture.
 *
 *  This function does not remove any picture from DPB. To fetch and remove a picture from DPB,
 *  call <tt> \ref VCDecNextPicture()</tt>.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \param [in]     pic            A pointer to the \c DecPictures structure for receiving the
 *                                 picture information.
 *                                 \n The picture information is valid only if the return value
 *                                 indicates that an output picture is available.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PIC_RDY</tt>,
 *                  <tt> \ref DEC_PARAM_ERROR</tt>, or <tt> \ref DEC_NOT_INITIALIZED</tt>. */
enum DecRet VCDecPeek(void* inst, struct DecPictures *pic);

/** Abort a decoder instance.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be aborted.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                  or <tt> \ref DEC_NOT_INITIALIZED</tt>. */
enum DecRet VCDecAbort(void* inst);

/** Checks whether a decoder instance has been aborted and reset.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance.
 *  \return         <tt> \ref DEC_OK</tt>, <tt> \ref DEC_PARAM_ERROR</tt>,
 *                  or <tt> \ref DEC_NOT_INITIALIZED</tt>. */
enum DecRet VCDecAbortAfter(void* inst);

/** Sets the decoder to output pictures in decoding order or display order.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be configured.
 *  \param [in]     no_reorder     The order in which the decoder output pictures.
 *  \return         None. */
void VCDecSetNoReorder(void* inst, u32 no_reorder);

/** Releases a decoder instance created using \c VCDecInit() and releases all the resources
 *  allocated during the initialization.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be released.
 *                                 \n After this function is successfully expected, the specified
 *                                 pointer is no longer valid.
 *  \return         None. */
void VCDecRelease(void* inst);

/** Updates the decoder stream length register at the specified address for low latency decoding.
 *  \ingroup common_group
 *  \param [in]     inst           The decoder instance to be configured.
 *  \param [in]     info           A pointer to the stream information.
 *  \return         None. */
void VCDecUpdateStrmInfoCtrl(void* inst, struct strmInfo info);

/** (For MPEG-4 only) Gets user data configurations.
 *  \ingroup common_group
 *  \param [in]     inst                The decoder instance.
 *  \param [in]     param               A pointer to the input parameters.
 *  \param [in,out] user_data_config    A pointer to the \c DecUserConf structure for receiving the
 *                                      user data configurations.
 *  \return         <tt> \ref DEC_OK</tt> or <tt> \ref DEC_PARAM_ERROR</tt>. */
enum DecRet VCDecGetUserData(void* inst, struct DecInputParameters* param,
                             struct DecUserConf* user_data_config);

/** (For MPEG-4 only) Configures external information.
 *
 *  You can call this function if a stream does not contain all information in the elementary
 *  bitstream.
 *  \ingroup common_group
 *  \param [in]     inst                The decoder instance.
 *  \param [in]     width               The frame width in pixels.
 *  \param [in]     height              The frame height in pixels.
 *  \return         <tt> \ref DEC_OK</tt> or <tt> \ref DEC_PARAM_ERROR</tt>. */
enum DecRet VCDecSetCustomInfo(void* inst, const u32 width, const u32 height);

/** Queries a return code of the video decoder.
 *  \ingroup common_group
 *  \param [in]    DecRet              The return code to be queried.
 *  \return        A pointer to the human-readable string of the return code. */
char *VCDecRetStr(enum DecRet rv);

#ifdef __cplusplus
}
#endif

#endif // VCDECAPI_H
