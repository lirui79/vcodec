/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Verisilicon.                                    --
--                                                                            --
--      In the event of publication, the following notice is applicable:      --
--                                                                            --
--                   (C) COPYRIGHT 2020 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--         The entire notice above must be reproduced on all copies.          --
--                                                                            --
--------------------------------------------------------------------------------
--
--  Description : dec400 cmodel. 
--
------------------------------------------------------------------------------*/
#ifndef DEC400SYS_H
#define DEC400SYS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "basetype.h"

typedef struct {
  u32 hw_build_id;
  u8 reg_version_index;
  u32 tile_size;
} HwDec400Config;

typedef u64 ptr_t;

u32 AsicHwDec400GetRegister(void *core, u32 offset);
i32 AsicHwDec400SetRegister(void *core, u32 offset, u32 val);
void *AsicHwDec400Create();
void AiscHwDec400Flush(void *core);
i32 AsicHwDec400Compress(void *core, ptr_t ba, void *virt, u32 stride, u32 width,
                         u32 height);
u32 AsicHwDec400StoreMemory(void *core, u32 prefix_offset, ptr_t dst_ba,
                            u32 stride, u32 width, u32 height, void *ctx,
                            i32 (*cb_data_store)(ptr_t dst_ba, void *src_buf,
                                                 int size, void *ctx));
void AiscHwDec400Release(void *core);

#ifdef __cplusplus
}
#endif
#endif