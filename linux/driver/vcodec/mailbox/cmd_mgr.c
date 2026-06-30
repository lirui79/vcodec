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
**                      include command private source                          **
*********************************************************************************/

#include "cmd.h"
#include "cmd_mgr.h"
#include "cmd_session.h"
#include "vcx_vcmd_priv.h"



static cmd_mgr_t g_cmd_mgr;

static int               _cmd_init_mgr(cmd_mgr_t *mgr) {
    int32_t i = 0, j = 0;
    uint32_t  coremask[CMD_R52_MGR_MAX] = {R52_CORE_MASK_VENC, R52_CORE_MASK_VDEC};
    cmd_r52_mgr_t *rmgr = NULL;
    mgr->rtb_size  = CMD_R52_MGR_MAX; // r52 core number
    for (i = 0; i < CMD_R52_MGR_MAX; ++i) {
        rmgr = &mgr->rtb[i];
        rmgr->coremask  = coremask[i];
        rmgr->workload  = 0;
        rmgr->status    = CMD_R52MGR_STATUS_INIT;
        rmgr->r52coreid = i;
        rmgr->vtb_size  = CMD_SESSION_MAX; // session number per r52 core
        rmgr->usedsize  = 0; // current used session number
        spin_lock_init(&rmgr->spinlock);
        for (j = 0 ; j < CMD_SESSION_MAX; ++j) {
            uint32_t sessionID = ((rmgr->r52coreid << 16) & 0xFFFF0000) | j;
            cmd_session_init(&rmgr->vtb[j], NULL, sessionID);
        }
    }
    

    for (i = 0; i < VCMD_MGR_ID_MAX; ++i) {
        mgr->mtb[i] = NULL;
    }
    mgr->cmd_queue = BQueueCreate(1024, CMD_MSG_MAX_SIZE);

    return  0;
}

int               cmd_init_mgr(void) {
    return _cmd_init_mgr(&g_cmd_mgr);
}

int               cmd_start_mgr(void) {
    return cmd_recv_thread_create(&g_cmd_mgr);
}

int               cmd_exit_mgr(void) {
    cmd_mgr_t* mgr = cmd_get_mgr();
    cmd_recv_thread_stop(mgr);
    if (mgr->cmd_queue) {
        BQueueDelete(mgr->cmd_queue);
    }

    return 0;

}

cmd_mgr_t*       cmd_get_mgr(void) {
    return &g_cmd_mgr;
}

int32_t           cmd_set_vcmd_mgr(uint32_t mgrID, vcmd_mgr_t* vcmdMgr) {
    cmd_mgr_t *mgr = NULL;
    if (mgrID >= VCMD_MGR_ID_MAX) {
        return -1;
    }
    mgr = cmd_get_mgr();
    mgr->mtb[mgrID] = vcmdMgr;
    return 0;
}

vcmd_mgr_t*    cmd_get_vcmd_mgr(uint32_t mgrID) {
    if (mgrID >= VCMD_MGR_ID_MAX) {
        return NULL;
    }
    return cmd_get_mgr()->mtb[mgrID];
}

cmd_session_t*    cmd_get_session(uint32_t sessionID) {
    uint32_t r52ID = 0, sesID = 0;
    cmd_r52_mgr_t *rmgr = NULL;
    cmd_session_t *session = NULL;
    r52ID  = ((sessionID & 0xFFFF0000) >> 16);
    sesID  = (sessionID & 0xFFFF);
    if ((r52ID >= CMD_R52_MGR_MAX) || (sesID >= CMD_SESSION_MAX)) {
        return NULL;
    }
    rmgr = &cmd_get_mgr()->rtb[r52ID];
    spin_lock(&rmgr->spinlock);
    session = &rmgr->vtb[sesID];
    spin_unlock(&rmgr->spinlock);
    return session;
}


cmd_session_t*    cmd_get_minused_r52_session0(void) {
    int32_t i = 0;
    uint32_t minused = 0;
    cmd_session_t *session = NULL;
    cmd_mgr_t* mgr = cmd_get_mgr();
    cmd_r52_mgr_t *rmgr = &mgr->rtb[0];

    spin_lock(&rmgr->spinlock);
    minused = rmgr->usedsize;
    session = &rmgr->vtb[0];
    spin_unlock(&rmgr->spinlock);
    for (i = 1; i < CMD_R52_MGR_MAX; ++i) {
        rmgr = &mgr->rtb[i];
        spin_lock(&rmgr->spinlock);
        if (rmgr->usedsize < minused) {
            minused = rmgr->usedsize;
            session = &rmgr->vtb[0];
        }
        spin_unlock(&rmgr->spinlock);
    }
    return session;
}

cmd_session_t*    cmd_get_coremask_session0(uint32_t coremask) {
    int32_t i = 0;
    cmd_session_t *session = NULL;
    cmd_mgr_t* mgr = cmd_get_mgr();
    cmd_r52_mgr_t *rmgr = NULL;

    for (i = 0; i < CMD_R52_MGR_MAX; ++i) {
        rmgr = &mgr->rtb[i];
        spin_lock(&rmgr->spinlock);
        if (rmgr->coremask & coremask) {
            session = &rmgr->vtb[0];
            spin_unlock(&rmgr->spinlock);
            return session;
        }
        spin_unlock(&rmgr->spinlock);
    }
    return NULL;
}

cmd_session_t*    cmd_get_idle_session(void) {
    int32_t i = 0, j = 0;
    cmd_r52_mgr_t *rmgr = NULL;
    cmd_session_t *session = NULL;
    cmd_mgr_t* mgr = cmd_get_mgr();
    for (j = 0; j < CMD_SESSION_MAX; ++j) {
        for (i = 0; i < CMD_R52_MGR_MAX; ++i) {
            rmgr = &mgr->rtb[i];
            spin_lock(&rmgr->spinlock);
            session = &rmgr->vtb[j];
            spin_unlock(&rmgr->spinlock);
            if (session->status == CMD_SESSION_STATUS_IDLE) {
                return session;
            }
        }
    }

    return NULL;
}

cmdMsg_t*         cmd_dequeue_cmdMsg(void) {
    return (cmdMsg_t *)BQueueDequeue(cmd_get_mgr()->cmd_queue);
}

cmdMsg_t*         cmd_acquire_cmdMsg(void) {
    return (cmdMsg_t *)BQueueAcquire(cmd_get_mgr()->cmd_queue);
}

int32_t           cmd_release_cmdMsg(cmdMsg_t* cmdMsg) {
    return   BQueueRelease(cmd_get_mgr()->cmd_queue, cmdMsg);
}

int32_t           cmd_queue_cmdMsg(cmdMsg_t* cmdMsg) {
    return   BQueueQueue(cmd_get_mgr()->cmd_queue, cmdMsg);
}

int32_t           cmd_cancel_cmdMsg(cmdMsg_t* cmdMsg) {
    return   BQueueCancel(cmd_get_mgr()->cmd_queue, cmdMsg);
}

static uint32_t cmd_check(cmdMsg_t *cmdMsg, cmd_session_t **session)
{
    uint32_t crc32 = 0, crc32Now = 0, retCode = CMD_ERR_SUCCESS;
    if (cmdMsg == NULL) {
        retCode = CMD_ERR_INVALID_POINTER;
        goto RETURN_ERROR;
    }

    if (cmdMsg->magic != CMD_MAGIC_NUMBER) {
        retCode = CMD_ERR_INVALID_MAGIC;
        goto RETURN_ERROR;
    }

    if (cmdMsg->version != CMD_VERSION) {
        retCode = CMD_ERR_INVALID_VERSION;
        goto RETURN_ERROR;
    }

    if (cmdMsg->cmdType > CMD_VCODEC_MAX) {
        retCode = CMD_ERR_INVALID_CMD_TYPE;
        goto RETURN_ERROR;
    }

    crc32 = cmdMsg->crc32;
    cmdMsg->crc32 = 0;
    crc32Now = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
    cmdMsg->crc32 = crc32;
    if (crc32 != crc32Now) {
        retCode = CMD_ERR_INVALID_CHECKSUM;
        goto RETURN_ERROR;
    }

    *session = cmd_get_session(cmdMsg->sessionID);
    if (*session == NULL) {
        retCode = CMD_ERR_INVALID_SESSIONID;
        goto RETURN_ERROR;
    }
    retCode = CMD_ERR_SUCCESS;

RETURN_ERROR:
    return retCode;
}


int32_t cmd_proc_cmdMsg(cmdMsg_t *cmdMsg) {
    cmd_session_t *session = NULL;

    if (cmd_check(cmdMsg, &session) != CMD_ERR_SUCCESS) {
        return CMD_ERR_INVALID_PARAM;
    }

    if (cmd_session_check(session, cmdMsg) < 0) {
        return CMD_ERR_INVALID_SEQUENCEID;
    }

    if (cmdMsg->cmdType <= CMD_SYSTEM_MAX) {
        return cmd_session_system(session, cmdMsg);
    }

    return cmd_session_vcodec(session, cmdMsg);
}
