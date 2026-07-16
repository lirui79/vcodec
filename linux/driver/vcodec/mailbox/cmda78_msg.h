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
**                     include command a78 message header                       **
*********************************************************************************/

#ifndef _COMMANDA78_MESSAGE_H_
#define _COMMANDA78_MESSAGE_H_

#include "cmdef.h"


#ifdef __cplusplus
extern "C" {
#endif


/*need to consider how many memory should be allocated for status.*/
struct exchange_cmda78_param {
	/* the instance ctx */
	void        *owner;
	/** control interrupt mode when generate JMP command
	 * bit31 is mode_flag.
	 *	- when mode_flag is 0, adapative interrupt mode is selected. in such
	 *	  mode, bit[30:0] is executing time estimated for current job;
	 *	- when mode_flag is 1, manual interrupt mode is selected. in such mode,
	 *	  bit[0] is used to set IE flag in JMP command.
	 *	  bit[39:32] is batch count.
	 */
	uint64_t     interrupt_ctrl; //input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	//input ;executing_time=encoded_image_size*(rdoLevel+1)*(rdoq+1);
	//u64 executing_time;
    uint32_t     vcmdmgr_id;
	/*input input vce=0,IM=1,vcd=2, jpege=3, jpegd=4 */
	uint16_t     module_type;
	/*input, reserve is not used; link and run is input.*/
	uint16_t     cmdbuf_size;
	/* output, it is unique in driver.*/
	uint16_t     cmdbuf_id;
	/* just used for polling. */
	uint16_t     core_id;
	/* core_mask for user to select cores: [0,15]core mask, [16,31]client type. */
	uint16_t     core_mask;
	/* input, bit[0]: priority    - normal=0, high/live=1
	 *        bit[1]: has_end_cmd - last cmd is JMP (0) or END (1) command
	 */
	uint16_t     input_mask;
};

struct proc_obj;


uint64_t   cmda78_get_system_time_ms(void);

int32_t    cmda78_gen_open_session(struct proc_obj *proc, uint32_t coremask);

int32_t    cmda78_gen_close_session(struct proc_obj *proc, uint32_t coremask);

int32_t    cmda78_gen_run_cmdbuf(struct proc_obj *proc, struct exchange_cmda78_param *cmd_param);

int32_t    cmda78_gen_ctrl_cmdbuf(struct proc_obj *proc, uint32_t vcmdmgr_id, uint32_t cmdtype, uint32_t cmdbuf_id);

int32_t    cmda78_gen_drop_owner(struct proc_obj *proc, uint64_t ownerID, uint32_t vcmdmgr_id);


#ifdef __cplusplus
}
#endif

#endif /*_COMMANDA78_MESSAGE_H_*/
