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
**                           *.c mhu v3 source code                             **
*********************************************************************************/

#include "inc.h"
#include "vcx_priv.h"
#include "mhu_v3_priv.h"
#include "mhu_v3_client.h"
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/wait.h>


#define MHU_DATA_SIZE      128
#define TX_TIMEOUT_MS     1000



/* Private data structure for the client */
struct mhu_v3_client {
    struct mbox_client cl;
    struct mbox_chan *tx_chan;
    struct mbox_chan *rx_chan;
    
    /* Buffer for 128-byte data */
    u8 tx_buf[MHU_DATA_SIZE];
    u8 rx_buf[MHU_DATA_SIZE];
    
    struct completion tx_complete;
    int tx_status;
    int rx_status;
    struct work_struct   rx_work;
    spinlock_t lock; /* Protects rx_buf access if needed */
    wait_queue_head_t	waitq;
};

struct mhu_v3_manager {
    struct mhu_v3_client  mhu_client[MHU_MAX_CLIENTS];
    struct device        *dev;
};

static struct mhu_v3_manager  g_mhu_v3_mgr;

// 工作队列处理函数：在进程上下文中处理接收数据
static void rx_work_handler(struct work_struct *work)
{
    struct mhu_v3_client *client = container_of(work, struct mhu_v3_client, rx_work);
    pr_info("Async RX Work: Processing data in process context\n");
    // 在此处进行耗时的数据处理

    wake_up_interruptible(&client->waitq);
}

/*
 * Rx Callback
 * Called when new data is received from the remote processor.
 * Note: This runs in atomic context (interrupt context usually).
 * Do not sleep here. Copy data and schedule work if processing is heavy.
 */
static void mhu_rx_callback(struct mbox_client *cl, void *mssg)
{
    struct mhu_v3_client *client = container_of(cl, struct mhu_v3_client, cl);
    unsigned long flags;
    
    /* 
     * The 'mssg' pointer depends on the controller implementation.
     * For MHU v3, it might be a pointer to the hardware register buffer
     * or a pre-allocated DMA buffer. We copy it to our safe buffer.
     */
    spin_lock_irqsave(&client->lock, flags);
    memcpy(client->rx_buf, mssg, MHU_DATA_SIZE);
    client->rx_status = 0; // Mark as received
    spin_unlock_irqrestore(&client->lock, flags);
    
    pr_info("MHU V3: Received 128 bytes of data.\n");
    
    /* Optional: Trigger bottom-half processing via workqueue */
    // 快速拷贝数据或调度工作队列
    // schedule_work(&priv->work);
    schedule_work(&client->rx_work);
}

/*
 * Recv 128-byte data via Mailbox
 */
int mhu_v3_recv_data(uint32_t r52id, u8 *data) {
    int ret;
    struct mhu_v3_manager *mgr    = (struct mhu_v3_manager *)(vcx_get_private(DEVID_VCX)->priv);
    struct mhu_v3_client  *client = NULL;
    if (r52id >= MHU_MAX_CLIENTS) {
        pr_err("MHU V3: Invalid client ID %d\n", r52id);
        return -EINVAL;
    }
    client = (struct mhu_v3_client *)(&mgr->mhu_client[r52id]);

    /* Wait for data if configured for blocking/sync */
    ret = wait_event_interruptible(client->waitq, (client->rx_status == 0));
    if (ret) {
        pr_err("MHU V3: wait_event_interruptible failed with %d\n", ret);
        return ret; // Error other than interrupt
    }

    spin_lock_irq(&client->lock);
    memcpy(data, client->rx_buf, MHU_DATA_SIZE);
    client->rx_status = 1; // Mark as processed
    spin_unlock_irq(&client->lock);
    
    pr_info("MHU V3: Received and copied 128 bytes of data.\n");
    
    return 0;

}

/*
 * Tx Done Callback
 * Called when the mailbox controller confirms the message has been sent.
 */
static void mhu_tx_done(struct mbox_client *cl, void *mssg, int r)
{
    struct mhu_v3_client *client = container_of(cl, struct mhu_v3_client, cl);
    
    client->tx_status = r;
    complete(&client->tx_complete);
    
    if (r)
        pr_debug("MHU V3: TX failed with status %d\n", r);
    else
        pr_debug("MHU V3: TX completed successfully\n");
}

/*
 * Send 128-byte data via Mailbox
 */
int mhu_v3_send_data(uint32_t r52id, const u8 *data) {
    int ret;
    struct mhu_v3_manager *mgr    = (struct mhu_v3_manager *)(vcx_get_private(DEVID_VCX)->priv);
    struct mhu_v3_client  *client = NULL;
    if (r52id >= MHU_MAX_CLIENTS) {
        pr_err("MHU V3: Invalid client ID %d\n", r52id);
        return -EINVAL;
    }
    client = (struct mhu_v3_client *)(&mgr->mhu_client[r52id]);

    
    /* Copy user data to internal buffer */
    memcpy(client->tx_buf, data, MHU_DATA_SIZE);
    
    reinit_completion(&client->tx_complete);
    client->tx_status = -EINPROGRESS;
    
    /*
     * Send the message.
     * The framework will call the controller's send_data op.
     * If the controller supports 128-byte chunks, it handles it.
     * Otherwise, this might need to be split or use shared memory.
     */
    ret = mbox_send_message(client->tx_chan, client->tx_buf);
    if (ret < 0) {
        pr_err("MHU V3: mbox_send_message failed: %d\n", ret);
        return ret;
    }
    
    /* Wait for transmission completion if configured for blocking/sync */
    if (client->cl.tx_block) {
        ret = wait_for_completion_timeout(&client->tx_complete,
                                          msecs_to_jiffies(TX_TIMEOUT_MS));
        if (!ret) {
            pr_err("MHU V3: TX timeout\n");
            return -ETIMEDOUT;
        }
        if (client->tx_status)
            return -EIO;
    }
    
    return 0;
}

/*
 * Probe function: Initialize mailbox client and request channels
 */
int mhu_v3_client_probe(struct platform_device *pdev) {
    struct mhu_v3_manager *mgr = &g_mhu_v3_mgr;
    struct mhu_v3_client  *client = NULL;
    vcx_priv_t *vcx_priv = platform_get_drvdata(pdev);
    struct device *dev = &pdev->dev;
    int ret, i = 0;

    vcx_priv->priv = (void*)mgr;
    mgr->dev = dev;

    for (i = 0; i < MHU_MAX_CLIENTS; i++) {
        client = &mgr->mhu_client[i];
        INIT_WORK(&client->rx_work, rx_work_handler);
        spin_lock_init(&client->lock);
        /* Setup mailbox client structure */
        client->cl.dev = dev;
        client->cl.tx_done = mhu_tx_done;
        client->cl.rx_callback = mhu_rx_callback;
        client->cl.tx_block = true; /* Block until TX done for this example */
        client->cl.knows_txdone = true;
        client->cl.tx_tout = TX_TIMEOUT_MS;
        client->rx_status = 2;
#ifdef MAILBOX_CLIENT
        /* Request TX channel by name defined in Device Tree */
        client->tx_chan = mbox_request_channel_byname(&client->cl, "tx");
        if (IS_ERR(client->tx_chan)) {
            ret = PTR_ERR(client->tx_chan);
            dev_err(dev, "Failed to get TX channel: %d\n", ret);
            return ret;
        }
        
        /* Request RX channel by name defined in Device Tree */
        client->rx_chan = mbox_request_channel_byname(&client->cl, "rx");
        if (IS_ERR(client->rx_chan)) {
            ret = PTR_ERR(client->rx_chan);
            dev_err(dev, "Failed to get RX channel: %d\n", ret);
            mbox_free_channel(client->tx_chan);
            return ret;
        }
#endif
        init_waitqueue_head(&client->waitq);    
    }

    dev_info(dev, "MHU V3 Client probed successfully\n");

    /* Example: Send initial test data */
    //memset(priv->tx_buf, 0xAA, MHU_DATA_SIZE);
    // mhu_v3_send_data(priv, priv->tx_buf);
    
    return 0;
}

/*
 * Remove function: Cleanup resources
 */
int mhu_v3_client_remove(struct platform_device *pdev) {
    vcx_priv_t *vcx_priv = platform_get_drvdata(pdev);
    struct mhu_v3_manager *mgr =  (struct mhu_v3_manager *)vcx_priv->priv;
    struct mhu_v3_client *client = NULL;
    int i = 0;
    for (i = 0; i < MHU_MAX_CLIENTS; i++) {
        client = &mgr->mhu_client[i];
        if (client->tx_chan)
            mbox_free_channel(client->tx_chan);
        if (client->rx_chan)
            mbox_free_channel(client->rx_chan);
    }
    vcx_priv->priv = NULL;
    dev_info(&pdev->dev, "MHU V3 Client removed\n");
    return 0;
}