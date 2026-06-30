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
**                      include mhu v3 client headers                           **
*********************************************************************************/

#ifndef _MHU_V3_CLIENT_H_
#define _MHU_V3_CLIENT_H_



#ifdef __cplusplus
extern "C" {
#endif


#define MHU_R52_CLIENT0      0
#define MHU_R52_CLIENT1      1
#define MHU_MAX_CLIENTS      2

/*
 * Send 128-byte data via Mailbox
 */
int mhu_v3_send_data(uint32_t r52id, const u8 *data);

/*
 * Recv 128-byte data via Mailbox
 */
int mhu_v3_recv_data(uint32_t r52id, u8 *data);


#ifdef __cplusplus
}
#endif

#endif //_MHU_V3_CLIENT_H_

