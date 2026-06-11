/********************************************************************************* 
**       This software is confidential and proprietary and may be used          **
**        only as expressly authorized by a licensing agreement from            **
**                                                                              **
**                            omnidimension                                     **
**                                                                              **
**                   (C) COPYRIGHT 2026 OMNIDIMENSION                           **
**                            ALL RIGHTS RESERVED                               **
**                                                                              **
**                 The entire notice above must be reproduced                   **
**                  on all copies and should not be removed.                    **
**                                                                              **
**********************************************************************************
**                    include vcx sub module type headers                       **
*********************************************************************************/

#ifndef _VCX_SUB_MODULE_TYPE_H_
#define _VCX_SUB_MODULE_TYPE_H_

enum CoreType {
	/* Decoder */
	HW_VCD = 0,
	HW_VCDJ,
	HW_BIGOCEAN,
	HW_VCMD,
	HW_MMU, //if set HW_MMU_WR, then HW_MMU means HW_MMU_RD
	HW_MMU_WR,
	HW_DEC400,
	/* Encoder*/
	/* Auxiliary IPs */
	HW_AXIFE,
	HW_AFBC,
	HW_ARB,
	HW_AXI2TO1,
	HW_CORE_MAX /* max number of cores supported */
};


enum {
	CORE_VCE = 0,
	CORE_VCEJ = 1,
	CORE_CUTREE = 2,
	CORE_DEC400 = 3,
	CORE_MMU = 4,
	CORE_L2CACHE = 5,
	CORE_AXIFE = 6,
	CORE_APBFT = 7,
	CORE_MMU_1 = 8,
	CORE_AXIFE_1 = 9,
	CORE_UFBC = 10,
	CORE_VCMD = 11,
	CORE_MAX
};

//#define CORE_MAX  (CORE_MMU)

/*module_type support*/

enum vcmd_module_type {
	VCMD_TYPE_ENCODER = 0,
	VCMD_TYPE_CUTREE,
	VCMD_TYPE_DECODER,
	VCMD_TYPE_JPEG_ENCODER,
	VCMD_TYPE_JPEG_DECODER,
	MAX_VCMD_TYPE
};

enum subsys_module_id {
	SUB_MOD_VCMD = 0,
	SUB_MOD_MAIN = 1,
	SUB_MOD_L2CACHE = 2,//enc
	SUB_MOD_MMU     = 3,//dec
	SUB_MOD_MMU0    = 3,//enc
	SUB_MOD_MMU_WR  = 4,//dec
	SUB_MOD_MMU1    = 4,//enc
	SUB_MOD_DEC400  = 5,
	SUB_MOD_AXIFE   = 6,//dec
	SUB_MOD_AXIFE0  = 6,//enc
	SUB_MOD_AXIFE1  = 7,//enc
	SUB_MOD_UFBC    = 8,
	SUB_MOD_AXI2TO1 = 9,

	SUB_MOD_MAX
};




#endif//_VCX_SUB_MODULE_TYPE_H_

