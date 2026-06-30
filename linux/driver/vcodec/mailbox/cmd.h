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
**                                 include cmd header                           **
*********************************************************************************/

#ifndef _MAILBOX_COMMAND_H_
#define _MAILBOX_COMMAND_H_

#include "cmdef.h"

#ifdef __cplusplus
extern "C" {
#endif

void    cmd_init(cmdMsg_t *cmdMsg);

int32_t cmd_recv(uint32_t r52ID);

int32_t cmd_send(cmdMsg_t *cmdMsg);

//int32_t cmd_proc(void);

uint32_t crc32_calc(const uint8_t *buffer, size_t bufferLength);

int32_t  cmd_recv_thread_create(void* arg);

int32_t  cmd_recv_thread_stop(void* arg);


#ifdef __cplusplus
}
#endif

#endif /*_MAILBOX_COMMAND_H_*/
