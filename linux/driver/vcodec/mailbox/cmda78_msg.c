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
**                       *.c command a78 message source code                    **
*********************************************************************************/

#include "cmdnode.h"
#include "cmda78_msg.h"
#include "cmda78_mgr.h"
#include "cmda78_proc.h"



uint64_t   cmda78_get_system_time_ms(void) {
    struct timespec64 ts;
    uint64_t  time_ms = 0;
    ktime_get_real_ts64(&ts);
    time_ms = ts.tv_sec * 1000 + (ts.tv_nsec / 1000000);
    return time_ms;
}

static int32_t cmda78_send_wait_event(cmda78_session_t* session, struct proc_obj *proc, cmdMsg_t *cmdMsg, uint32_t timeout) {
    cmdnode_t *cnode = NULL;
    int32_t errCode = CMD_ERR_SUCCESS;
    long retCode = 0;

    retCode = cmda78_session_send(session, cmdMsg);
    cnode   = cmdnode_alloc(cmdMsg->seqNum, cmdMsg->sessionID, cmdMsg->timeStamp, proc);

    spin_lock(&session->spinlock);
    cmdnode_insert(&session->cmdroot, cnode);
    spin_unlock(&session->spinlock);

	retCode = wait_event_interruptible_timeout(cnode->wait, (cnode->code != CMD_ERR_UNKNOWN),	msecs_to_jiffies(timeout));
    memcpy((int8_t*)cmdMsg, (int8_t*)cnode->cmdMsg, CMD_MSG_MAX_SIZE);

    spin_lock(&session->spinlock);
    errCode = cnode->code;
    cmdnode_delete(&session->cmdroot, cnode);
    cmdnode_free(cnode);
    spin_unlock(&session->spinlock);

    if (retCode < 0) {// 被信号中断
        return -ERESTARTSYS;
    } else if (retCode == 0) {// 超时
        printk(KERN_WARNING "timeout!\n");
        return -ETIMEDOUT;
    } else {// 成功唤醒，条件已满足 // 此时可以安全地拷贝数据给用户

        return -errCode;
    }

    return CMD_ERR_SUCCESS;
}

int32_t    cmda78_gen_open_session(struct proc_obj *proc, uint32_t coremask) {
    cmdMsg_t *cmdMsg = cmda78_dequeue_cmdMsg();//    cmd_session_t* session = cmd_get_minused_r52_session0();
    cmda78_session_t* session = cmda78_get_coremask_session0(coremask);
    cmdReqOpenSession_Body_t *cmdBody = (cmdReqOpenSession_Body_t *)cmdMsg->data;
    long retCode = 0;
    if (session == NULL) {
        return -EINVAL;
    }
    cmd_init(cmdMsg);
    proc->session = NULL;

    cmdMsg->cmdType        = CMD_REQ_OPEN_SESSION;
    cmdMsg->sessionID      = session->sessionID;
    cmdMsg->cmdSize        = CMD_MSG_MIN_SIZE + sizeof(cmdReqOpenSession_Body_t);
    cmdMsg->timeStamp      = cmda78_get_system_time_ms();
    cmdBody->procObj       = (uint64_t)proc;

    retCode = cmda78_send_wait_event(session, proc, cmdMsg, 1000);
    cmda78_release_cmdMsg(cmdMsg);

    return retCode;
}


int32_t    cmda78_gen_close_session(struct proc_obj *proc, uint32_t coremask) {
    cmdMsg_t *cmdMsg = cmda78_dequeue_cmdMsg();
    cmda78_session_t* session = cmda78_get_coremask_session0(coremask);
    cmdReqCloseSession_Body_t *cmdBody = (cmdReqCloseSession_Body_t *)cmdMsg->data;

    long retCode = 0;
    if (session == NULL) {
        return -EINVAL;
    }
    cmd_init(cmdMsg);

    cmdMsg->cmdType        = CMD_REQ_CLOSE_SESSION;
    cmdMsg->sessionID      = session->sessionID;
    cmdMsg->cmdSize        = CMD_MSG_MIN_SIZE + sizeof(cmdReqCloseSession_Body_t);
    cmdMsg->timeStamp      = cmda78_get_system_time_ms();

    cmdBody->procObj       = (uint64_t)proc;
    cmdBody->sessionID     = proc->session->sessionID;

    retCode = cmda78_send_wait_event(session, proc, cmdMsg, 1000);
    cmda78_release_cmdMsg(cmdMsg);

    return retCode;
}


int32_t    cmda78_gen_run_cmdbuf(struct proc_obj *proc, struct exchange_cmda78_param *cmd_param) {
    cmdMsg_t *cmdMsg = cmda78_dequeue_cmdMsg();
    cmda78_session_t* session = proc->session;
    cmdReqRunCmdBuf_Body_t *cmdBody = (cmdReqRunCmdBuf_Body_t *)cmdMsg->data;
    long retCode = 0;
    if (session == NULL) {
        return -EINVAL;
    }
    cmd_init(cmdMsg);

    cmdMsg->cmdType        = CMD_REQ_RUN_CMDBUF;
    cmdMsg->sessionID      = session->sessionID;
    cmdMsg->cmdSize        = CMD_MSG_MIN_SIZE + sizeof(cmdReqRunCmdBuf_Body_t);
    cmdMsg->timeStamp      = cmda78_get_system_time_ms();

    cmdBody->procObj        = (uint64_t)proc;
    cmdBody->ownerID        = (uint64_t)cmd_param->owner;
    cmdBody->interrupt_ctrl = cmd_param->interrupt_ctrl;
    cmdBody->vcmdmgr_id     = cmd_param->vcmdmgr_id;
    cmdBody->module_type    = cmd_param->module_type;
    cmdBody->cmdbuf_size    = cmd_param->cmdbuf_size;
    cmdBody->cmdbuf_id      = cmd_param->cmdbuf_id;
    cmdBody->core_id        = cmd_param->core_id;
    cmdBody->core_mask      = cmd_param->core_mask;
    cmdBody->input_mask     = cmd_param->input_mask;

    retCode = cmda78_send_wait_event(session, proc, cmdMsg, 1000);
    if (retCode == CMD_ERR_SUCCESS) {
        if (cmdMsg->cmdType == CMD_RSP_RUN_CMDBUF) {
            cmdRspRunCmdBuf_Body_t *cmdBody1 = (cmdRspRunCmdBuf_Body_t *)cmdMsg->data;
            cmd_param->core_id = cmdBody1->core_id;
            retCode = cmdBody1->code;
        }
    }
    cmda78_release_cmdMsg(cmdMsg);

    return retCode;
}

int32_t    cmda78_gen_ctrl_cmdbuf(struct proc_obj *proc, uint32_t vcmdmgr_id, uint32_t cmdtype, uint32_t cmdbuf_id) {
    cmdMsg_t *cmdMsg = cmda78_dequeue_cmdMsg();
    cmda78_session_t* session = proc->session;
    cmdReqCtlCmdBuf_Body_t *cmdBody = (cmdReqCtlCmdBuf_Body_t *)cmdMsg->data;
    long retCode = 0;
    if (session == NULL) {
        return -EINVAL;
    }
    cmd_init(cmdMsg);

    cmdMsg->cmdType        = cmdtype;
    cmdMsg->sessionID      = session->sessionID;
    cmdMsg->cmdSize        = CMD_MSG_MIN_SIZE + sizeof(cmdReqCtlCmdBuf_Body_t);
    cmdMsg->timeStamp      = cmda78_get_system_time_ms();

    cmdBody->procObj        = (uint64_t)proc;
    cmdBody->vcmdmgr_id     = vcmdmgr_id;
    cmdBody->cmdbuf_id      = cmdbuf_id;

    retCode = cmda78_send_wait_event(session, proc, cmdMsg, 1000);
    if (retCode == CMD_ERR_SUCCESS) {
        if (cmdMsg->cmdType == (cmdtype + 1)) {
            cmdRspCtlCmdBuf_Body_t *cmdBody1 = (cmdRspCtlCmdBuf_Body_t *)cmdMsg->data;
            retCode = cmdBody1->code;
        }
    }
    cmda78_release_cmdMsg(cmdMsg);

    return retCode;
}

int32_t    cmda78_gen_drop_owner(struct proc_obj *proc, uint64_t ownerID, uint32_t vcmdmgr_id) {
    cmdMsg_t *cmdMsg = cmda78_dequeue_cmdMsg();
    cmda78_session_t* session = proc->session;
    cmdReqDropOwner_Body_t *cmdBody = (cmdReqDropOwner_Body_t *)cmdMsg->data;
    int32_t retCode = 0, cmdbuf_num = 0;
    if (session == NULL) {
        return 0;
    }
    cmd_init(cmdMsg);

    cmdMsg->cmdType        = CMD_REQ_DROP_OWNER;
    cmdMsg->sessionID      = session->sessionID;
    cmdMsg->cmdSize        = CMD_MSG_MIN_SIZE + sizeof(cmdReqDropOwner_Body_t);
    cmdMsg->timeStamp      = cmda78_get_system_time_ms();

    cmdBody->procObj        = (uint64_t)proc;
    cmdBody->ownerID        = ownerID;
    cmdBody->vcmdmgr_id     = vcmdmgr_id;

    retCode = cmda78_send_wait_event(session, proc, cmdMsg, 1000);
    if (retCode == CMD_ERR_SUCCESS) {
        if (cmdMsg->cmdType == CMD_RSP_DROP_OWNER) {
            cmdRspDropOwner_Body_t *cmdBody1 = (cmdRspDropOwner_Body_t *)cmdMsg->data;
            retCode = cmdBody1->code;
            cmdbuf_num = cmdBody1->cmdbuf_num;
        }
    }
    cmda78_release_cmdMsg(cmdMsg);

    return cmdbuf_num;
}
