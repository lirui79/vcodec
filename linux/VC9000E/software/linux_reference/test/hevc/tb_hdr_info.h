/*------------------------------------------------------------------------------
--                                                                                                                               --
--       This software is confidential and proprietary and may be used                                   --
--        only as expressly authorized by a licensing agreement from                                     --
--                                                                                                                               --
--                            Verisilicon.                                                                                    --
--                                                                                                                               --
--                   (C) COPYRIGHT 2014 VERISILICON                                                            --
--                            ALL RIGHTS RESERVED                                                                    --
--                                                                                                                               --
--                 The entire notice above must be reproduced                                                 --
--                  on all copies and should not be removed.                                                    --
--                                                                                                                               --
--------------------------------------------------------------------------------
--
-- Description : Hevc SEI Messages read from json file.
--
------------------------------------------------------------------------------*/

#ifndef TB_HDR_INFO_H
#define TB_HDR_INFO_H

#include "base_type.h"
#include "vcenc_hdr_info.h"

#define DEBUG_JSON
#define CURRENT_HDR 0
#define NEXT_HDR    1

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * \page ss_hdr_t35 Insert HDR10+ or Dolby Vision Information
 *
 * HDR helper API are designed to help writing HDR information into
 * bitstream. The HDR information is parsed from a json file which is
 * compatible with HDR10+, or Dolby vision, or mixed style.
 * The HDR information is filled in SEI part of bitstream.
 * HDR10+ infromation for one frame in json data can be configured by
 * frame ID. Such information will be repeated until new info coming.
 *
 * \see VCEncMetadataGet()
 * \see VCEncMetadataInit()
 * \see VCEncMetadataRelease()
 *
 * Please refer to following standard documents of HDR10 plus and Dolby
 * vision for the definitions of the following data types.
 *  - SMPTE ST 2094-40 (Samsung)
 *  - SMPTE ST 2094-10 (Dolby Vision)
 */

#ifndef HDR_HELPER_SUPPORT
#define VCEncMetadataGet(ctx, frame_index)
#define VCEncMetadataInit(json_file)
#define VCEncMetadataRelease(ctx)
#else

/**
* \brief      Initial the meta data context
* \param [in] json_file file name of JSON which content is HDR10 Plus and Dolby Vision
* \return handle for context of JSON and meta, if success
* \return NULL error occurs
*/
void *VCEncMetadataInit(const char *json_file);

/**
* \brief      get hdr information from json file
* \param [in] hdr_data handle for context of JSON and meta
* \param [in] frame_index the frame index where the HDR information belong to
* \return NULL if not get the correct information
* \return info pointer to structure which contain the HDR information of frame with index
*         of frame_index.
*/
HDR10Info *VCEncMetadataGet(void *ctx, int frame_index);

/**
* \brief      release the meta data context
* \param [in] hdr_data handle for context of JSON and meta
* \return 0 if success
* \return -1 error occurs
*/
int VCEncMetadataRelease(void *ctx);
#endif


/** @} */

#if defined(__cplusplus)
}
#endif

#endif /* TB_HDR_INFO_H */

