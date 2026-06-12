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
**                      include vcodec manager vcmd header                      **
*********************************************************************************/

#ifndef __VCM_DRIVER_H__
#define __VCM_DRIVER_H__

#include <linux/ioctl.h> /* needed for the _IOW etc stuff used later */

#ifdef __cplusplus
extern "C" {
#endif


/* Use 'k' as magic number */
#define HANTRO_VCMG_IOC_MAGIC 'g'

/*
#define HANTRO_VCMG_IOCG_HWOFFSET                           _IOR(HANTRO_VCMG_IOC_MAGIC,  3, unsigned long *)
#define HANTRO_VCMG_IOCG_HWIOSIZE                           _IOR(HANTRO_VCMG_IOC_MAGIC,  4, unsigned int *)
#define HANTRO_VCMG_IOC_CLI                                 _IO(HANTRO_VCMG_IOC_MAGIC,  5)
#define HANTRO_VCMG_IOC_STI                                 _IO(HANTRO_VCMG_IOC_MAGIC,  6)
#define HANTRO_VCMG_IOCX_VIRT2BUS                           _IOWR(HANTRO_VCMG_IOC_MAGIC,  7, unsigned long *)
#define HANTRO_VCMG_IOCH_ARDRESET                           _IO(HANTRO_VCMG_IOC_MAGIC, 8)   // debugging tool
#define HANTRO_VCMG_IOCG_SRAMOFFSET                         _IOR(HANTRO_VCMG_IOC_MAGIC,  9, unsigned long *)
#define HANTRO_VCMG_IOCG_SRAMEIOSIZE                        _IOR(HANTRO_VCMG_IOC_MAGIC,  10, unsigned int *)
#define HANTRO_VCMG_IOCH_ENC_RESERVE                        _IOR(HANTRO_VCMG_IOC_MAGIC, 11, unsigned int *)
#define HANTRO_VCMG_IOCH_ENC_RELEASE                        _IOR(HANTRO_VCMG_IOC_MAGIC, 12, unsigned int *)
#define HANTRO_VCMG_IOCG_CORE_NUM                           _IOR(HANTRO_VCMG_IOC_MAGIC, 13, unsigned int *)
#define HANTRO_VCMG_IOCG_CORE_INFO                          _IOR(HANTRO_VCMG_IOC_MAGIC, 14, SUBSYS_CORE_INFO *)
#define HANTRO_VCMG_IOCG_CORE_WAIT                          _IOR(HANTRO_VCMG_IOC_MAGIC, 15, unsigned int *)
#define HANTRO_VCMG_IOCG_ANYCORE_WAIT                       _IOR(HANTRO_VCMG_IOC_MAGIC, 16, CORE_WAIT_OUT *)
#define HANTRO_VCMG_IOCG_ANYCORE_WAIT_POLLING               _IOR(HANTRO_VCMG_IOC_MAGIC, 17, CORE_WAIT_OUT *)
#define HANTRO_VCMG_IOCG_ENABLE_CORE                        _IOR(HANTRO_VCMG_IOC_MAGIC, 18, unsigned int *)

#define HANTRO_VCMG_IOCH_GET_CMDBUF_PARAMETER               _IOWR(HANTRO_VCMG_IOC_MAGIC, 25, struct cmdbuf_mem_parameter *)
#define HANTRO_VCMG_IOCH_GET_CMDBUF_POOL_SIZE               _IOWR(HANTRO_VCMG_IOC_MAGIC, 26, unsigned long)
#define HANTRO_VCMG_IOCH_SET_CMDBUF_POOL_BASE               _IOWR(HANTRO_VCMG_IOC_MAGIC, 27, unsigned long)
#define HANTRO_VCMG_IOCH_GET_VCMD_PARAMETER                 _IOWR(HANTRO_VCMG_IOC_MAGIC, 28, struct config_parameter *)
#define HANTRO_VCMG_IOCH_RESERVE_CMDBUF                     _IOWR(HANTRO_VCMG_IOC_MAGIC, 29, struct exchange_parameter *)
#define HANTRO_VCMG_IOCH_LINK_RUN_CMDBUF                    _IOR(HANTRO_VCMG_IOC_MAGIC, 30, u16 *)
#define HANTRO_VCMG_IOCH_WAIT_CMDBUF                        _IOR(HANTRO_VCMG_IOC_MAGIC, 31, u16 *)
#define HANTRO_VCMG_IOCH_RELEASE_CMDBUF                     _IOR(HANTRO_VCMG_IOC_MAGIC, 32, u16 *)
#define HANTRO_VCMG_IOCH_POLLING_CMDBUF                     _IOR(HANTRO_VCMG_IOC_MAGIC, 33, u16 *)

#define HANTRO_VCMG_IOCH_GET_VCMD_ENABLE                    _IOWR(HANTRO_VCMG_IOC_MAGIC, 50, unsigned long)
#define HANTRO_VCMG_IOCH_GET_MMU_ENABLE                     _IOWR(HANTRO_VCMG_IOC_MAGIC, 51, unsigned long)
#define HANTRO_VCMG_IOCH_GET_PM_SUPPORT                     _IOWR(HANTRO_VCMG_IOC_MAGIC, 52, unsigned long)
#define HANTRO_VCMG_IOCH_WRITE_CORE_REGS                    _IOW(HANTRO_VCMG_IOC_MAGIC, 53, struct core_regs_wr *)
*/

#ifdef __cplusplus
}
#endif


#endif /*__VCM_DRIVER_H__ */
