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
**                      include command a78 manager header                      **
*********************************************************************************/

#ifndef _CMDA78_MANAGER_H_
#define _CMDA78_MANAGER_H_


#include "cmdef.h"
#include "bqueue.h"
#include "cmda78_session.h"
#include "vcx_vcmd_priv.h"


#ifdef __cplusplus
extern "C" {
#endif


#define    R52_CORE_MASK_VENC             ((u32)(0x1 << 0))
#define    R52_CORE_MASK_VDEC             ((u32)(0x1 << 1))

#define    CMD_R52_MGR_MAX    2

typedef enum {
    CMD_R52MGR_STATUS_INIT = 0,
    CMD_R52MGR_STATUS_RUN,
    CMD_R52MGR_STATUS_INVALID,
    CMD_R52MGR_STATUS_EXIT,
} cmd_r52mgr_status;

typedef struct {
    uint64_t           workload;
    uint32_t           coremask;
    uint32_t           status;
    uint32_t           r52coreid; //
    uint32_t           vtb_size; //
    uint32_t           usedsize; //
    cmda78_session_t   vtb[CMDA78_SESSION_MAX];// vcodec session table
	spinlock_t         spinlock;
} cmd_r52mgr_t;


typedef struct {
    uint32_t              rtb_size; //
    cmd_r52mgr_t          rtb[CMD_R52_MGR_MAX];// r52 cmd mgr table
    vcmd_mgr_t*           mtb[VCMD_MGR_ID_MAX];	// vcmd manager  0-vcmd mgr enc, 1- vcmd mgr dec
    BQueueHandle_t        cmd_queue; // command queue
    struct task_struct   *recv_thread[CMD_R52_MGR_MAX];
    struct task_struct   *work_thread;
	wait_queue_head_t     workwaitqueue;
    atomic_t              refcount;
} cmda78_mgr_t;


int                  cmda78_init_mgr(void);

int                  cmda78_start_mgr(void);

int                  cmda78_exit_mgr(void);

cmda78_mgr_t*        cmda78_get_mgr(void);

int32_t              cmda78_set_vcmd_mgr(uint32_t mgrID, vcmd_mgr_t* vcmdMgr);

vcmd_mgr_t*          cmda78_get_vcmd_mgr(uint32_t mgrID);

cmda78_session_t*    cmda78_get_session(uint32_t sessionID);

cmda78_session_t*    cmda78_get_minused_r52_session0(void);

cmda78_session_t*    cmda78_get_coremask_session0(uint32_t coremask);

cmda78_session_t*    cmda78_get_idle_session(void);

cmdMsg_t*            cmda78_dequeue_cmdMsg(void);

cmdMsg_t*            cmda78_acquire_cmdMsg(void);

int32_t              cmda78_release_cmdMsg(cmdMsg_t* cmdMsg);

int32_t              cmda78_queue_cmdMsg(cmdMsg_t* cmdMsg);

int32_t              cmda78_cancel_cmdMsg(cmdMsg_t* cmdMsg);

int32_t              cmda78_proc_cmdMsg(cmdMsg_t *cmdMsg);





#ifdef __cplusplus
}
#endif

#endif /*_CMDA78_MANAGER_H_*/
