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
**                           include vcd vcmd header                           **
*********************************************************************************/

#ifndef __TESTBENCH_VCODEC_H__
#define __TESTBENCH_VCODEC_H__


#ifdef __cplusplus
extern "C" {
#endif

int main_vcodex(int argc, char **argv, const char *optarg);

int main_encode(int argc, char **argv, const char *optarg);

int main_decode(int argc, char **argv, const char *optarg);

#ifdef __cplusplus
}
#endif

#endif /*__TESTBENCH_VCODEC_H__ */
