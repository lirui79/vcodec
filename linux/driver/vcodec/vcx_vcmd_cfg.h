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
**                         include vcx vcmd cfg header                          **
*********************************************************************************/

#ifndef _VCX_VCMD_CFG_H_
#define _VCX_VCMD_CFG_H_


/* submodule config */
struct sub_mod_cfg {
	enum subsys_module_id sub_mod_id;

	unsigned int io_off; // submodule reg base (offset to vcmd reg-base)
	unsigned int io_size;   // submodule io size

	unsigned int rreg_id;  // start reg-id to read out when init driver,
				   // 0xffff means not need to read.
	unsigned int rreg_num;  // number of registers to read out when init driver
};

/*for all vcmds, the config info should be listed here for subsequent use*/
struct vcmd_config {
	unsigned long vcmd_base_addr;	//vcmd reg_base (bus address)
	int vcmd_irq;
	unsigned int sub_module_type; /*input vce=0,IM=1,vcd=2，jpege=3, jpegd=4*/
	unsigned int priority; //the priority of vcmd
	struct sub_mod_cfg submodule_cfg[SUB_MOD_MAX];
};


#endif /*_VCX_VCMD_CFG_H_*/
