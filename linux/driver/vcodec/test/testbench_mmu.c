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
**                            source code test mmu                              **
*********************************************************************************/



#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <getopt.h>
#include <assert.h>
#include <pthread.h>

#include "base_type.h"
#include "vcx_mmu.h"
#include "vcx_module_type.h"
#include "testbench_vcodec.h"




int test_vcmd_mmu(int fd) { //  HANTRO_IOCS_MMU_MEM_MAP  HANTRO_IOCS_MMU_MEM_UNMAP  HANTRO_IOCS_MMU_FLUSH HANTRO_IOCS_MMU_SWITCH_PAGETABLE HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF
    long tmp = 0;
    { 
        struct addr_desc addr = {0};
        addr.virtual_address = 0x40000000; /* buffer virtual address */
        addr.bus_address = 0x2000000; /* buffer physical address */
        addr.size = 0x10000; /* physical size */
        tmp = ioctl(fd, HANTRO_IOCS_MMU_MEM_MAP, &addr);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCS_MMU_MEM_MAP failed \n");
            return -1;
        }
        printf("%s %s %d MMU Map %d: %x %x %x\n", __FILE__, __func__, __LINE__, tmp, addr.virtual_address, addr.bus_address, addr.size);
    }
    { 
        struct addr_desc addr = {0};
        addr.virtual_address = 0x40000000; /* buffer virtual address */
        addr.bus_address = 0x2000000; /* buffer physical address */
        addr.size = 0x10000; /* physical size */
        tmp = ioctl(fd, HANTRO_IOCS_MMU_MEM_UNMAP, &addr);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCS_MMU_MEM_UNMAP failed \n");
            return -1;
        }
        printf("%s %s %d MMU Unmap %d: %x %x %x\n", __FILE__, __func__, __LINE__, tmp, addr.virtual_address, addr.bus_address, addr.size);
    }

    {
        unsigned int core_id = 0;
        tmp = ioctl(fd, HANTRO_IOCS_MMU_SWITCH_PAGETABLE, &core_id);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCS_MMU_SWITCH_PAGETABLE failed \n");
            return -1;
        }
        printf("%s %s %d MMU SWITCH Flush %d:%x\n", __FILE__, __func__, __LINE__, tmp, core_id);
        core_id = 1;
        tmp = ioctl(fd, HANTRO_IOCS_MMU_FLUSH, &core_id);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCS_MMU_FLUSH failed \n");
            return -1;
        }
        printf("%s %s %d MMU Flush %d:%x\n", __FILE__, __func__, __LINE__, tmp, core_id);
    }

    {
        struct page_table_switch params;
        u32 i;
        tmp = ioctl(fd, HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF, &params);
        if (tmp == -1) {
            printf("ERROR: ioctl HANTRO_IOCS_MMU_SWITCH_PAGETABLE_BY_CMDBUF failed \n");
            return -1;
        }
        printf("%s %s %d MMU SWITCH %d: %x %x\n", __FILE__, __func__, __LINE__, tmp, params.id, params.pt_flush.flush_cnt);
        for (i = 0; i < params.pt_flush.flush_cnt; i++) {
            if (i == 8) {
                printf("\n");
            }
            printf(" %x", params.pt_flush.flush_vmid[i]);
        }
        printf("\n");
    }

    return 0;
}