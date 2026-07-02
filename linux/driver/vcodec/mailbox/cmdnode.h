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
**                            include cmd node header                           **
*********************************************************************************/

#ifndef _MAILBOX_COMMAND_NODE_H_
#define _MAILBOX_COMMAND_NODE_H_

#include "cmdef.h"


#ifdef __cplusplus
extern "C" {
#endif


struct proc_obj;

typedef struct {
    uint64_t               procObj;// process object id
    uint64_t               timeStamp;
    uint32_t               ackNum;
    uint32_t               sessionID;
    uint32_t               code;
    struct proc_obj       *proc;
    struct rb_node         node;
    wait_queue_head_t      wait;
    struct kref            refcount;
    uint8_t                cmdMsg[CMD_MSG_MAX_SIZE];
} cmdnode_t;

cmdnode_t*    cmdnode_alloc(uint32_t ackNum, uint32_t sessionID, uint64_t timeStamp, struct proc_obj *proc);

int32_t       cmdnode_insert(struct rb_root *root, cmdnode_t *cnode);

cmdnode_t*    cmdnode_search(struct rb_root *root, uint32_t ackNum);

void          cmdnode_delete(struct rb_root *root, cmdnode_t *cnode);

void          cmdnode_free(cmdnode_t *cnode);

#ifdef __cplusplus
}
#endif

#endif /*_MAILBOX_COMMAND_NODE_H_*/
