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
**                      include command a78 session header                      **
*********************************************************************************/

#ifndef _COMMANDA78_SESSION_H_
#define _COMMANDA78_SESSION_H_

#include "cmdef.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CMDA78_SESSION_MAX  32

typedef enum {
    CMD_SESSION_STATUS_IDLE = 0,
    CMD_SESSION_STATUS_RUN,
    CMD_SESSION_STATUS_EXIT,
    CMD_SESSION_STATUS_STOP
} cmda78_session_status;

struct proc_obj;

//session
typedef struct {
    uint32_t               sessionID;// r52id + session_idx
    uint32_t               seqRNum;// sequence number, from 0 to 0xFFFFFFFF
    uint32_t               seqSNum;// sequence number, from 0 to 0xFFFFFFFF
    uint32_t               status;
    struct proc_obj       *proc;
    struct rb_root         cmdroot;
	spinlock_t             spinlock;
} cmda78_session_t;

int32_t        cmda78_session_init(cmda78_session_t *session, struct proc_obj *proc, uint32_t sessionID);

int32_t        cmda78_session_check(cmda78_session_t *session, cmdMsg_t *cmdMsg);

int32_t        cmda78_session_system(cmda78_session_t *session, cmdMsg_t *cmdMsg);

int32_t        cmda78_session_vcodec(cmda78_session_t *session, cmdMsg_t *cmdMsg);

int32_t        cmda78_session_send(cmda78_session_t *session, cmdMsg_t *cmdMsg);


#ifdef __cplusplus
}
#endif

#endif /*_COMMANDA78_SESSION_H_*/
