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
**                      include command a78 session source                      **
*********************************************************************************/

#include "cmdnode.h"
#include "cmda78_mgr.h"
#include "cmda78_proc.h"
#include "cmda78_session.h"
#include "vcx_vcmd_priv.h"
#include "vcx_vcmd.h"


int32_t cmda78_session_init(cmda78_session_t *session, struct proc_obj *proc, uint32_t sessionID) {
    session->proc      = proc;
    if (proc != NULL) {
        proc->session      = session;
    }
    session->sessionID = sessionID;
    session->seqRNum   = 0x00000000;
    session->seqSNum   = 0x00000000;
    session->status    = CMD_SESSION_STATUS_IDLE;
    session->cmdroot   = RB_ROOT;
    spin_lock_init(&session->spinlock);
    return 0;
}

int32_t        cmda78_session_check(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    
    if (session->seqRNum != cmdMsg->seqNum) {
        return CMD_ERR_INVALID_SEQUENCEID;
    }
    session->seqRNum++;
    return 0;
}

static cmdnode_t *cmdsession_search_cmdnode(cmda78_session_t *session, uint32_t ackNum) {
    cmdnode_t *cnode = NULL;
    spin_lock(&session->spinlock);
    cnode = cmdnode_search(&session->cmdroot, ackNum);
    if (cnode != NULL) {
        kref_get(&cnode->refcount);
    }
    spin_unlock(&session->spinlock);
    return cnode;
}

static int32_t cmdsession_wake_up_all(cmdnode_t *cnode, cmdMsg_t *cmdMsg) {
    int32_t  retCode = CMD_ERR_SUCCESS;
    memcpy(cnode->cmdMsg, (uint8_t*) &cmdMsg, cmdMsg->cmdSize);
    retCode = cnode->code;
	wake_up_interruptible_all(&cnode->wait);
    cmdnode_free(cnode);
    return retCode;
}


static int32_t cmd_system_open_session(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdRspOpenSession_Body_t *cmdBody = (cmdRspOpenSession_Body_t *)cmdMsg->data;
    cmda78_session_t *cmd_session = NULL;
    struct proc_obj *proc = NULL;
    cmdnode_t *cnode = NULL;

    cmd_session = cmda78_get_session(cmdBody->sessionID);
    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->procObj == cmdBody->procObj) {
        proc = cnode->proc;
        proc->session = cmd_session;

        if (cmd_session != NULL) {
            cmd_session->proc = proc;
            cmd_session->status  = CMD_SESSION_STATUS_RUN;
            cmd_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmd_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
        } else {
            cnode->code = CMD_ERR_INVALID_SESSIONID;
        }
    } else {
        cnode->code = CMD_ERR_INVALID_PROCOBJ;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t cmd_system_close_session(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdRspCloseSession_Body_t *cmdBody = (cmdRspCloseSession_Body_t *)cmdMsg->data;
    cmda78_session_t *cmd_session = NULL;
    struct proc_obj *proc = NULL;
    cmdnode_t *cnode = NULL;

    cmd_session = cmda78_get_session(cmdBody->sessionID);
    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->procObj == cmdBody->procObj) {
        proc = cnode->proc;
        proc->session = NULL;

        if (cmd_session != NULL) {
            cmd_session->status  = CMD_SESSION_STATUS_IDLE;
            cmd_session->seqRNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmd_session->seqSNum = 0x00;// sequence number, from 0 to 0xFFFFFFFF
            cmd_session->proc = NULL;
        } else {
            cnode->code = CMD_ERR_INVALID_SESSIONID;
        }
    } else {
        cnode->code = CMD_ERR_INVALID_PROCOBJ;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

int32_t        cmda78_session_system(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_RSP_EXE_SYSCTL:  //CMD_REQ_EXE_SYSCTL:
        //return cmd_system_echo(cmdMsg);
        break;
    case CMD_RSP_OPEN_SESSION:  //CMD_REQ_OPEN_SESSION:
        return cmd_system_open_session(session, cmdMsg);
        break;
    case CMD_RSP_CLOSE_SESSION:  //CMD_REQ_CLOSE_SESSION:
        return cmd_system_close_session(session, cmdMsg);
        break;
    case CMD_EVT_REPORT_CMDERROR: //
         break;
    default:
        break;
    }
    return 0;
}

static int32_t          vcodec_run_cmdbuf(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdnode_t *cnode = NULL;
    cmdRspRunCmdBuf_Body_t *cmdBody = (cmdRspRunCmdBuf_Body_t *)cmdMsg->data;

    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->sessionID != cmdMsg->sessionID) {
        cnode->code = CMD_ERR_INVALID_SESSIONID;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t          vcodec_ctrl_cmdbuf(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdnode_t *cnode = NULL;
    cmdRspCtlCmdBuf_Body_t *cmdBody = (cmdRspCtlCmdBuf_Body_t *)cmdMsg->data;

    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->sessionID != cmdMsg->sessionID) {
        cnode->code = CMD_ERR_INVALID_SESSIONID;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t          vcodec_drop_owner(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdnode_t *cnode = NULL;
    cmdRspDropOwner_Body_t *cmdBody = (cmdRspDropOwner_Body_t *)cmdMsg->data;

    cnode = cmdsession_search_cmdnode(session, cmdBody->ackNum);
    if (cnode == NULL) {
        return CMD_ERR_INVALID_ACKNUM;
    }

    cnode->code = cmdBody->code;
    if (cnode->sessionID != cmdMsg->sessionID) {
        cnode->code = CMD_ERR_INVALID_SESSIONID;
    }

    return cmdsession_wake_up_all(cnode, cmdMsg);
}

static int32_t          vcodec_report_cmdbuf_ready(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdEvtRepCmdBufReady_Body_t *cmdBody = (cmdEvtRepCmdBufReady_Body_t *)cmdMsg->data;
    vcmd_mgr_t* vcmd_mgr = NULL;
    struct cmdbuf_obj *obj = NULL;
    int32_t  retCode = CMD_ERR_SUCCESS;
    if (cmdBody->status != 0x00) {
        return CMD_ERR_INVALID_PARAM;
    }

    if ((cmdBody->cmdbuf_id == ANY_CMDBUF_ID) || (cmdBody->cmdbuf_id >= SLOT_NUM_CMDBUF)) {
        return CMD_ERR_INVALID_CMDBUFID;
    }

    obj = &vcmd_mgr->objs[cmdBody->cmdbuf_id];
    if (obj->po != session->proc) {
        return CMD_ERR_INVALID_PROCOBJ;
    }

    if (cmdBody->vcmdmgr_id >= VCMD_MGR_ID_MAX) {
        return CMD_ERR_INVALID_VCMDMGRID;
    }

    vcmd_mgr = cmda78_get_vcmd_mgr(cmdBody->vcmdmgr_id);
    switch(cmdBody->vcmdmgr_id) {
        case VCMD_MGR_ID_ENC:
            vce_proc_add_done_job(vcmd_mgr, obj);
            break;
        case VCMD_MGR_ID_DEC:
            vcd_proc_add_done_job(vcmd_mgr, obj);
            break;
        default:
            return CMD_ERR_INVALID_VCMDMGRID;
            break;
    }

    return retCode;
}

int32_t        cmda78_session_vcodec(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    switch (cmdMsg->cmdType) {
    case CMD_RSP_RUN_CMDBUF:  ///CMD_REQ_RUN_CMDBUF:
        return vcodec_run_cmdbuf(session, cmdMsg);
        break;
    case CMD_RSP_PUSH_SLICE_REG:  ///CMD_REQ_PUSH_SLICE_REGION:
    case CMD_RSP_POLLING_CMDBUF:  ///CMD_REQ_POLLING_CMDBUF:
    case CMD_RSP_ABORT_CMDBUF:    ///CMD_REQ_ABORT_CMDBUF
        return vcodec_ctrl_cmdbuf(session, cmdMsg);
        break;
    case CMD_RSP_DROP_OWNER:  ///CMD_REQ_DROP_OWNER:
        return vcodec_drop_owner(session, cmdMsg);
        break;
    case CMD_EVT_REPORT_CMDBUF_READY:  ///CMD_EVT_REPORT_CMDBUF_READY:
        return vcodec_report_cmdbuf_ready(session, cmdMsg);
        break;
    default:
        break;
    }
    return 0;
}

int32_t        cmda78_session_send(cmda78_session_t *session, cmdMsg_t *cmdMsg) {
    cmdMsg->sessionID    = session->sessionID;
    cmdMsg->seqNum       = session->seqSNum++;
    cmdMsg->timeStamp    = 0x00000000;
    return cmda78_send(cmdMsg);
}
