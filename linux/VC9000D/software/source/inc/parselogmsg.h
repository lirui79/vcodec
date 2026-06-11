/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            VeriSilicon Inc.                                --
--                                                                            --
--                   (C) COPYRIGHT 2015 VeriSilicon Inc                       --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
------------------------------------------------------------------------------*/

#include "dectypes.h"
#include "dec_log.h"
#include "vpufeature.h"

char* ParseDecPictureFormat(enum DecPictureFormat output_format);
char* ParseDecRet(enum DecRet rv);
char *ParseDWLMemType(int i);
void ParseDecInitConfig(struct DecInitConfig *cfg);
void ParseDecSequenceInfo(struct DecSequenceInfo *seq);
char* ParseDecPicAlignment(int i);
void ParseDecConfig(struct DecConfig *config);
void ParseDecInputParameters(struct DecInputParameters* param);
void ParseDecBufferInfo(struct DecBufferInfo *buf_info);
void ParseFeatureList(struct DecHwFeatures *cfg);