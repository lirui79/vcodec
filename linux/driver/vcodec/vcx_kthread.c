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
**                          *.c vcx kthread source code                         **
*********************************************************************************/

#include "vce_priv.h"
#include "vcd_priv.h"
#include "vcx_kthread.h"
#include <linux/kthread.h>




/********************************************************************
 * watchdog and kthread functions
 ********************************************************************/

/**
 * @brief To check vcmd_mgr/dev's actions which need kthread to process
 * @return int: 0: no actions; 1: have actions
 */
static int _vcmd_kthread_actions(vcmd_mgr_t *vcmd_mgr,
			struct hantrovcmd_dev **dev)
{
	int ret = 0, i;

	if (vcmd_mgr->stop_kthread == 1)
		return 1;

	for (i = 0; i < vcmd_mgr->subsys_num; i++) {
		*dev = &vcmd_mgr->dev_ctx[i];
		if ((*dev)->kthread_actions) {
			ret = 1;
			break;
		}
	}

	return ret;
}

/**
 * @brief vcmd kernel thread main function
 */
static int _vcmd_kthread_fn(void *data)
{
	vcmd_mgr_t *vcmd_mgr = (vcmd_mgr_t *)data;
	struct hantrovcmd_dev *dev = NULL;

	while (!kthread_should_stop()) {
		if (wait_event_interruptible(vcmd_mgr->kthread_waitq,
				_vcmd_kthread_actions(vcmd_mgr, &dev))) {
			vcmd_klog(LOGLVL_ERROR, "%s: signaled!!!\n", __func__);
			return -ERESTARTSYS;
		}

		if (dev == NULL)
			continue;

		if (dev->kthread_actions & KT_ACT_HW_TIMEOUT) {
			dev->kthread_actions = 0;
			/* if external timeout, will do system reset */
			//hook_vcmd_external_timeout(dev);
			continue;
		}
		if (dev->kthread_actions & KT_ACT_HW_BUS_ERR) {
			dev->kthread_actions = 0;
			//_vcmd_bus_err_process(dev);
			continue;
		}
	}

	return 0;
}

/**
 * @brief wake up vcmd kernel thread
 */
void _vcmd_kthread_wakeup(vcmd_mgr_t *vcmd_mgr)
{

	if (IS_ERR(vcmd_mgr->kthread))
		return;

	wake_up_interruptible_all(&vcmd_mgr->kthread_waitq);
}

/**
 * @brief create kernel thread for vcmd driver
 */
void _vcmd_kthread_create(vcmd_mgr_t *vcmd_mgr, const char *name)
{
	vcmd_mgr->stop_kthread = 0;
	init_waitqueue_head(&vcmd_mgr->kthread_waitq);
	vcmd_mgr->kthread =
		kthread_run(_vcmd_kthread_fn, (void *)vcmd_mgr, name);
	if (IS_ERR(vcmd_mgr->kthread)) {
		vcmd_klog(LOGLVL_ERROR, "create vcmd kthread failed\n");
		return;
	}
}

/**
 * @brief stop kernel thread vcmd driver
 */
void _vcmd_kthread_stop(vcmd_mgr_t *vcmd_mgr)
{
	if (!IS_ERR(vcmd_mgr->kthread)) {
		vcmd_mgr->stop_kthread = 1;
		kthread_stop(vcmd_mgr->kthread);
		vcmd_mgr->kthread = NULL;
	}
}
