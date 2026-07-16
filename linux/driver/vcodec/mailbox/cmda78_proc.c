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
**                         *.c cmd a78 proc source code                         **
*********************************************************************************/

#include "cmda78_mgr.h"
#include "cmda78_proc.h"
#include "mhu_v3_client.h"


int32_t cmda78_recv(uint32_t r52ID) {
    int32_t retCode = 0;
//    size_t  cmdSize = CMD_MSG_MAX_SIZE;
    cmdMsg_t *cmdMsg = cmda78_dequeue_cmdMsg();
    if (cmdMsg == NULL) {
        return -1;
    }
// mailbox_recv(cmdMsg);//    retCode = mhu_receive_data((uint8_t *)cmdMsg, &cmdSize);
    retCode = mhu_v3_recv_data(r52ID, (uint8_t *)cmdMsg);
    cmda78_queue_cmdMsg(cmdMsg);
    return retCode;
}

int32_t cmda78_send(cmdMsg_t *cmdMsg) {
    int32_t retCode = 0;
    uint32_t r52ID = ((cmdMsg->sessionID & 0xFFFF0000) >> 16);
    cmdMsg->crc32 = crc32_calc((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
// mailbox_send(cmdMsg);//    retCode = mhu_send_data((const uint8_t *)cmdMsg, cmdMsg->cmdSize);
    retCode = mhu_v3_send_data(r52ID, (const uint8_t *)cmdMsg);

    return retCode;
}

static int32_t cmda78_work_thread_proc(void *arg) {
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) arg;
    cmdMsg_t *cmdMsg = NULL;

    printk("work thread started\n");
    while (!kthread_should_stop()) {
        if (wait_event_interruptible(mgr->workwaitqueue, atomic_read(&mgr->refcount) > 0)) {
            pr_err("cmd work: %s: signaled!!!\n", __func__);
            break;
        }

        cmdMsg = cmda78_acquire_cmdMsg();
        if (cmdMsg == NULL) {
            continue;
        }
        cmda78_proc_cmdMsg(cmdMsg);
        cmda78_release_cmdMsg(cmdMsg);
        atomic_dec(&mgr->refcount);
    }

    printk("work thread exiting\n");
    return 0;
}

uint32_t crc32_calc(const uint8_t *buffer, size_t bufferLength) {
// 使用内核 API 计算 CRC32
// 种子值使用 ~0，与 R52 侧保持一致
   return crc32_le(~0, buffer, bufferLength) ^ ~0;
}


static int cmda78_thread_func(void *arg) {
    cmd_r52mgr_t *rmgr = (cmd_r52mgr_t *)arg;
    int32_t code = 0;
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) cmda78_get_mgr();

    printk("recv thread started\n");

    while (!kthread_should_stop()) {
        code = cmda78_recv(rmgr->r52coreid);
        if (code != 0) {
            continue;
        }
        atomic_inc(&mgr->refcount);
        wake_up_interruptible(&mgr->workwaitqueue);
    }

    printk("recv thread exiting\n");
    return 0;
}

int32_t  cmda78_recv_thread_create(void* arg) {
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) arg;
    atomic_set(&mgr->refcount, 0);
    init_waitqueue_head(&mgr->workwaitqueue);
    // 创建并启动内核线程，将 dev 作为参数传入
    mgr->recv_thread[0] = kthread_run(cmda78_thread_func, &mgr->rtb[0], "recv_thread0");
    if (IS_ERR(mgr->recv_thread[0])) {
        printk("Failed to create recv thread 0\n");
        return PTR_ERR(mgr->recv_thread[0]);
    }

    mgr->recv_thread[1] = kthread_run(cmda78_thread_func, &mgr->rtb[1], "recv_thread1");
    if (IS_ERR(mgr->recv_thread[1])) {
        printk("Failed to create recv thread 1\n");
        kthread_stop(mgr->recv_thread[0]);
        mgr->recv_thread[0] = NULL;
        return PTR_ERR(mgr->recv_thread[1]);
    }

    mgr->work_thread = kthread_run(cmda78_work_thread_proc, mgr, "work_thread");
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

int32_t  cmda78_recv_thread_stop(void* arg) {
    cmda78_mgr_t *mgr = (cmda78_mgr_t*) arg;

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
