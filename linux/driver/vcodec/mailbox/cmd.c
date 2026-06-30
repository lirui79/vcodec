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
**                             *.c cmd source code                              **
*********************************************************************************/

#include "cmd.h"
#include "cmd_mgr.h"
#include "mhu_v3_client.h"


void cmd_init(cmdMsg_t *cmdMsg) {
    memset(cmdMsg, 0, sizeof(cmdMsg_t));
    cmdMsg->magic   = CMD_MAGIC_NUMBER;
    cmdMsg->version = CMD_VERSION;
    cmdMsg->prior   = CMD_DEFAULT_PRIORITY;
    cmdMsg->crc32   = 0;
}



int32_t cmd_recv(uint32_t r52ID) {
    int32_t retCode = 0;
//    size_t  cmdSize = CMD_MSG_MAX_SIZE;
    cmdMsg_t *cmdMsg = cmd_dequeue_cmdMsg();
    if (cmdMsg == NULL) {
        return -1;
    }
// mailbox_recv(cmdMsg);//    retCode = mhu_receive_data((uint8_t *)cmdMsg, &cmdSize);
    retCode = mhu_v3_recv_data(r52ID, (uint8_t *)cmdMsg);
    cmd_queue_cmdMsg(cmdMsg);
    return retCode;
}

int32_t cmd_send(cmdMsg_t *cmdMsg) {
    int32_t retCode = 0;
    uint32_t r52ID = ((cmdMsg->sessionID & 0xFFFF0000) >> 16);
    cmdMsg->crc32 = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
// mailbox_send(cmdMsg);//    retCode = mhu_send_data((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
    retCode = mhu_v3_send_data(r52ID, (const uint8_t *)cmdMsg);

    return retCode;
}

static int32_t cmd_work_thread_proc(void *arg) {
    cmd_mgr_t *mgr = (cmd_mgr_t*) arg;
    cmdMsg_t *cmdMsg = NULL;

    printk("work thread started\n");
    while (!kthread_should_stop()) {
        if (wait_event_interruptible(mgr->workwaitqueue, atomic_read(&mgr->refcount) > 0)) {
            pr_err("cmd work: %s: signaled!!!\n", __func__);
            break;
        }

        cmdMsg = cmd_acquire_cmdMsg();
        if (cmdMsg == NULL) {
            continue;
        }
        cmd_proc_cmdMsg(cmdMsg);
        cmd_release_cmdMsg(cmdMsg);
        atomic_dec(&mgr->refcount);
    }
    return 0;
}

uint32_t crc32_calc(const uint8_t *buffer, size_t bufferLength) {
// 使用内核 API 计算 CRC32
// 种子值使用 ~0，与 R52 侧保持一致
   return crc32_le(~0, buffer, bufferLength) ^ ~0;
}


static int cmd_thread_func(void *arg) {
    cmd_r52_mgr_t *rmgr = (cmd_r52_mgr_t *)arg;
    int32_t code = 0;
    cmd_mgr_t *mgr = (cmd_mgr_t*) cmd_get_mgr();

    printk("recv thread started\n");

    while (!kthread_should_stop()) {
        code = cmd_recv(rmgr->r52coreid);
        if (code != 0) {
            continue;
        }
        atomic_inc(&mgr->refcount);
        wake_up_interruptible(&mgr->workwaitqueue);
    }

    printk("Worker thread exiting\n");
    return 0;
}

int32_t  cmd_recv_thread_create(void* arg) {
    cmd_mgr_t *mgr = (cmd_mgr_t*) arg;
    atomic_set(&mgr->refcount, 0);
    init_waitqueue_head(&mgr->workwaitqueue);
    // 创建并启动内核线程，将 dev 作为参数传入
    mgr->recv_thread[0] = kthread_run(cmd_thread_func, &mgr->rtb[0], "recv_thread0");
    if (IS_ERR(mgr->recv_thread[0])) {
        printk("Failed to create recv thread 0\n");
        return PTR_ERR(mgr->recv_thread[0]);
    }

    mgr->recv_thread[1] = kthread_run(cmd_thread_func, &mgr->rtb[1], "recv_thread1");
    if (IS_ERR(mgr->recv_thread[1])) {
        printk("Failed to create recv thread 1\n");
        kthread_stop(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
        return PTR_ERR(mgr->recv_thread[1]);
    }

    mgr->work_thread = kthread_run(cmd_work_thread_proc, mgr, "work_thread");
    if (IS_ERR(mgr->work_thread)) {
        printk("Failed to create work thread\n");
        kthread_stop(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
        kthread_stop(mgr->recv_thread[1]);
        mgr->recv_thread[1] = NULL;
        return PTR_ERR(mgr->work_thread);
    }

    return 0;
}

int32_t  cmd_recv_thread_stop(void* arg) {
    cmd_mgr_t *mgr = (cmd_mgr_t*) arg;

    if (mgr->work_thread) {
        // 请求停止并等待线程退出
        kthread_stop(mgr->work_thread);
        mgr->work_thread = NULL;
    }

    if (mgr->recv_thread[0]) {
        // 请求停止并等待线程退出
        kthread_stop(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
    }

    if (mgr->recv_thread[1]) {
        // 请求停止并等待线程退出
        kthread_stop(mgr->recv_thread[1]);
        mgr->recv_thread[1] = NULL;
    }

    return 0;
}
