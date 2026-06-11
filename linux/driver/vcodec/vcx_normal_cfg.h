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
**                       include vcx normal cfg header                          **
*********************************************************************************/

#ifndef _VCX_NORMAL_CFG_H_
#define _VCX_NORMAL_CFG_H_

typedef struct {
	unsigned long base_addr;
	unsigned int  iosize;
	unsigned int  resource_shared; //indicate the core share resources with other cores or not.If 1, means cores can not work at the same time.
	unsigned int  asic_id;
} SUBSYS_CONFIG;


typedef struct {
	unsigned int  subsys_idx;
	unsigned int  core_type;
	unsigned long offset;
	unsigned int  reg_size;
	int           irq;
} CORE_CONFIG;

#endif /*_VCX_NORMAL_CFG_H_*/