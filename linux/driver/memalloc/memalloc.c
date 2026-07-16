/*
 * SPDX_license-Identifier: GPL-2.0  OR BSD-3-Clause
 * Copyright (c) 2015, Verisilicon Inc. - All Rights Reserved
 *
 ********************************************************************************
 *
 * GPL-2.0
 *
 ********************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 ********************************************************************************
 *
 * Alternatively, This software may be distributed under the terms of
 * BSD-3-Clause, in which case the following provisions apply instead of the ones
 * mentioned above :
 *
 ********************************************************************************
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************************
 */

#include <linux/uaccess.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>

#ifdef DTB_SUPPORT
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/version.h>
#include <asm/io.h>
#include "memalloc.h"

/**
 * Add function get_of_property() to get data from DTB,
 * it's added to match Qemu implementition
 * (node string are hard-coded in Qemu model).
 */
#ifdef DTB_SUPPORT
#define VCD_DTB_NODE_NAME "verisilicon,vcd"
#endif

#ifdef PCIE_EN
#include <linux/pci.h>
#ifdef EMU
  /******************* add by wfy *********************/
  #define PCI_VENDOR_ID_HANTRO           0x1d9b//0x1ae0//0x16c3
  #define PCI_DEVICE_ID_HANTRO_PCI      0xface//0x001a// 0xabcd

  /* Base address got control register */
  #define PCI_DDR_BAR               4
#else
  /******************* add by wfy *********************/
  #define PCI_VENDOR_ID_HANTRO           0x10ee//0x1ae0//0x16c3
#ifdef PLATFORM_GEN7
  #define PCI_DEVICE_ID_HANTRO_PCI      0x9014//0x001a// 0xabcd

  /* Base address got control register */
  #define PCI_DDR_BAR               0
#else
  #define PCI_DEVICE_ID_HANTRO_PCI      0x8014//0x001a// 0xabcd

  /* Base address got control register */
  #define PCI_DDR_BAR               0
#endif
#endif

/* TODO(mheikkinen) Implement multicore support. */
static struct pci_dev *gDev;    /* PCI device structure. */
/* PCI base register address (Hardware address) */
static unsigned long gBaseHdwr;
static u32 gBaseLen;
#endif

#ifndef HLINA_TRANSL_VCMD_SIZE
#define HLINA_TRANSL_VCMD_SIZE    0x900000
#endif

#ifndef HLINA_START_ADDRESS
#define HLINA_START_ADDRESS 0x02000000
#endif

#ifndef HLINA_SIZE
#define HLINA_SIZE 96
#endif

#ifndef HLINA_TRANSL_OFFSET
#define HLINA_TRANSL_OFFSET 0x0
#endif

/* the size of chunk in MEMALLOC_DYNAMIC */
#define CHUNK_SIZE (PAGE_SIZE * 4)

/* memory size in MBs for MEMALLOC_DYNAMIC */
static unsigned long alloc_size = HLINA_SIZE;
static unsigned long alloc_base = HLINA_START_ADDRESS;

/* user space SW will substract HLINA_TRANSL_OFFSET from the bus address
 * and decoder HW will use the result as the address translated base
 * address. The SW needs the original host memory bus address for memory
 * mapping to virtual address.
 */
static unsigned long addr_transl = HLINA_TRANSL_OFFSET;

/* PCIE mode. */
/* Reserve mem for VCMD buffer */
static unsigned int vcmd_size = HLINA_TRANSL_VCMD_SIZE;
static unsigned long ddr_offset;
static char *mem_dev_n = "memalloc";
static int memalloc_major; /* dynamic */
static unsigned int ddr_size = 768;
static unsigned long mem_alloc_table_size = 0x800000; /* Reserve 8MB for memeory allocator table by default. */
/* module_param(name, type, perm) */
module_param(alloc_size, ulong, 0);
module_param(alloc_base, ulong, 0);
module_param(addr_transl, ulong, 0);
module_param(vcmd_size, uint, 0);
module_param(mem_dev_n, charp, 0644);
module_param(ddr_offset, ulong, 0);
module_param(ddr_size, uint, 0);
module_param(mem_alloc_table_size, ulong, 0);

static DEFINE_SPINLOCK(mem_lock);

typedef struct hlinc {
	u64 bus_address;
	u32 chunks_reserved;
	const struct file *filp; /* Client that allocated this chunk */
} hlina_chunk;

static hlina_chunk *hlina_chunks;
static size_t chunks;

/*********************************************/
#define         MEM_DRIVER_NAME     "memalloc"
static const   char *CLASS_NAME   = "memallocclass";
static const   int  DEVICE_COUNT  =  1;  // 注册3个设备

static struct   class *mem_class  = NULL;
static dev_t    base_dev_no       = 0;   // 起始设备号
//static int      major_num         = 0;
static struct cdev    cdev;////// 字符设备核心结构
static dev_t  devno               = 0;///// 完整的设备号 (Major + Minor)
static struct device       *dev   = NULL;
/*********************************************/


static int AllocMemory(unsigned long *busaddr, unsigned long size,
		       const struct file *filp);
static int FreeMemory(unsigned long busaddr, const struct file *filp);
static void ResetMems(void);

static int memlloc_mmap(struct file *filp, struct vm_area_struct *vma);

static long memalloc_ioctl(struct file *filp, unsigned int cmd,
			   unsigned long arg)
{
	int ret = 0;
	MemallocParams memparams;
	unsigned long busaddr;

	PDEBUG("ioctl cmd 0x%08x\n", cmd);

	/* extract the type and number bitfields, and don't decode wrong
	 * cmds: return ENOTTY (inappropriate ioctl) before access_ok()
	 */
	if (_IOC_TYPE(cmd) != MEMALLOC_IOC_MAGIC)
		return -ENOTTY;
	if (_IOC_NR(cmd) > MEMALLOC_IOC_MAXNR)
		return -ENOTTY;

#if (KERNEL_VERSION(5, 0, 0) > LINUX_VERSION_CODE)
	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok(VERIFY_WRITE, (void __user *)arg,
						 _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok(VERIFY_READ, (void __user *)arg,
						 _IOC_SIZE(cmd));
#else
	if (_IOC_DIR(cmd) & _IOC_READ)
		ret = !access_ok((void *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		ret = !access_ok((void *)arg, _IOC_SIZE(cmd));
#endif
	if (ret)
		return -EFAULT;

	switch (cmd) {
	case MEMALLOC_IOCGMEMBASE:
#ifdef PCIE_EN
		__put_user((alloc_base - mem_alloc_table_size - vcmd_size),
			   (unsigned int __user *)arg);
#else
		__put_user(alloc_base, (unsigned int __user *)arg);
#endif
		break;
	case MEMALLOC_IOCHARDRESET:
		PDEBUG("HARDRESET\n");
		ResetMems();
		break;
	case MEMALLOC_IOCXGETBUFFER:
		PDEBUG("GETBUFFER");

		ret = copy_from_user(&memparams, (MemallocParams __user *)arg,
				     sizeof(MemallocParams));
		if (ret)
			break;

		ret = AllocMemory(&memparams.bus_address, memparams.size, filp);

		memparams.translation_offset = addr_transl;

		ret |= copy_to_user((MemallocParams __user *)arg, &memparams,
			sizeof(MemallocParams));
		break;
	case MEMALLOC_IOCSFREEBUFFER:
		PDEBUG("FREEBUFFER\n");

		__get_user(busaddr, (unsigned long __user *)arg);
		ret = FreeMemory(busaddr, filp);
		break;
	}

	return ret ? -EFAULT : 0;
}

static int memalloc_open(struct inode *inode, struct file *filp)
{
	PDEBUG("dev opened\n");
	return 0;
}

static int memalloc_release(struct inode *inode, struct file *filp)
{
	int i = 0;

	spin_lock(&mem_lock);

	for (i = 0; i < chunks; i++) {
		if (hlina_chunks[i].filp == filp) {
			pr_warn("memalloc: Found unfreed memory at release time!\n");

			hlina_chunks[i].filp = NULL;
			hlina_chunks[i].chunks_reserved = 0;
		}
	}

	spin_unlock(&mem_lock);
	PDEBUG("dev closed\n");
	return 0;
}

static void __exit memalloc_cleanup(void)
{
	if (hlina_chunks)
		vfree(hlina_chunks);

	// 1. 销毁设备节点
	device_destroy(mem_class, devno);
	// 2. 删除 cdev
	cdev_del(&cdev);
    // 3. 释放设备号
    unregister_chrdev_region(base_dev_no, DEVICE_COUNT);
    // 4. 销毁类
    class_destroy(mem_class);
    pr_info("module removed");
}

/* VFS methods */
static const struct file_operations memalloc_fops = {
	.owner = THIS_MODULE,
	.open = memalloc_open,
	.mmap = memlloc_mmap,
	.release = memalloc_release,
	.unlocked_ioctl = memalloc_ioctl
};

static const struct vm_operations_struct memlloc_vm_ops = {
#ifdef CONFIG_HAVE_IOREMAP_PROT
	.access = generic_access_phys
#endif
};

#ifdef DTB_SUPPORT
/* get alloc_base and alloc_size from DTB */
static int get_of_mem(void)
{
	struct resource res;
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, VCD_DTB_NODE_NAME);
	np = of_parse_phandle(np, "memory-region", 0);
	ret = of_address_to_resource(np, 0, &res);
	if (ret) {
		pr_err("can't get reserved memory-region from DTB. ret=%d,\n", ret);
		return -EINVAL;
	}
	alloc_base = res.start;
	alloc_size = (res.end - res.start + 1) >> 20; // in MBs
	pr_info("get reserved memory-region: base address 0x%lx, size %ld MB\n",
		alloc_base, alloc_size);

	return 0;
}
#endif

#ifdef PCIE_EN
/*-------------------------------------------------
 * Function name   : PcieInit
 * Description     : Initialize PCI Hw access

 * Return type     : int
 * -------------------------------------------------
 */
static int PcieInit(void)
{
	gDev = pci_get_device(PCI_VENDOR_ID_HANTRO,
			      PCI_DEVICE_ID_HANTRO_PCI, gDev);
	if (!gDev) {
		pr_info("%s: Hardware not found.\n", __func__);
		goto out;
	}

	if (pci_enable_device(gDev) < 0) {
		pr_info("%s Device not enabled.\n", __func__);
		goto out;
	}

	gBaseHdwr = pci_resource_start(gDev, PCI_DDR_BAR);
	if (gBaseHdwr == 0) {
		pr_info("Init: Base Address not set.\n");
		goto out_pci_disable_device;
	}
	pr_info("Base hw val 0x%X\n", (unsigned int)gBaseHdwr);

	gBaseLen = pci_resource_len(gDev, PCI_DDR_BAR);
	pr_info("Base hw len 0x%x\n", (unsigned int)gBaseLen);

	/* Reserve 8MB for memeory allocator table. */
	#ifdef EMU
		// EMU available memeory: ddr base + 0x4000000
		alloc_base = gBaseHdwr + mem_alloc_table_size + 0x4000000 + vcmd_size + ddr_offset;
	#else
		alloc_base = gBaseHdwr + mem_alloc_table_size + vcmd_size + ddr_offset;
	#endif
	addr_transl = gBaseHdwr;
	alloc_size = ddr_size - mem_alloc_table_size / 0x100000 - vcmd_size / 0x100000;
	//alloc_base2= alloc_base+alloc_size*0x100000+0x100000;
	return 0;


	//out_iounmap:
	//      iounmap((void *) gBaseVirt);
out_pci_disable_device:
	pci_disable_device(gDev);
out:
	return -1;

}
#endif

static int __init memalloc_init(void)
{
	int result, ret;

#ifdef DTB_SUPPORT
	result = get_of_mem();
	if (result < 0)
		goto err;
#endif

	pr_info("module init\n");
#ifdef PCIE_EN
	result = PcieInit();
	if (result)
		goto err;
#endif
	pr_info("memalloc: Linear Memory Allocator\n");
	pr_info("memalloc: Linear memory base = 0x%08lx\n", alloc_base);

	chunks = (alloc_size * 1024 * 1024) / CHUNK_SIZE;

	pr_info("memalloc: Total size %ld MB; %d chunks of size %lu\n",
		alloc_size, (int)chunks, CHUNK_SIZE);

	hlina_chunks = vmalloc(chunks * sizeof(hlina_chunk));
	if (!hlina_chunks) {
		//pr_err("memalloc: cannot allocate hlina_chunks\n");
		result = -ENOMEM;
		goto err;
	}

    // 1. 创建类
    mem_class = class_create(CLASS_NAME);
    if (IS_ERR(mem_class)) {
        pr_info("Failed to create class\n");
		result = -ENOMEM;
		goto err;
    }

    // 2. 动态分配一组设备号 (主设备号自动分配，次设备号预留 0~2)
    ret = alloc_chrdev_region(&base_dev_no, 0, DEVICE_COUNT, MEM_DRIVER_NAME);
    if (ret < 0) {
        pr_err("Failed to allocate chrdev region\n");
		result = -ENOMEM;
        goto err_class;
    }
    memalloc_major = MAJOR(base_dev_no);
    pr_info("Allocated Major Number: %d\n", memalloc_major);

	// 2. 计算设备号: 主设备号相同，次设备号 = id
    devno = MKDEV(memalloc_major, 0);

    // 3. 初始化并添加 cdev
    cdev_init(&cdev, &memalloc_fops);
    cdev.owner = THIS_MODULE;
    ret = cdev_add(&cdev, devno, 1);
    if (ret) {
        pr_err("Failed to add cdev\n");
		result = -ENOMEM;
        goto err_chrdev;
    }

    // 4. 创建设备节点 /dev/ 下
    // 这会在 /sys/class/hantroclass/ 下创建条目，并触发 udev 创建 /dev 节点
    dev = device_create(mem_class, NULL, devno, NULL, "%s", MEM_DRIVER_NAME);
    if (IS_ERR(dev)) {
        cdev_del(&cdev);
        pr_err("Failed to create device node for name %s\n", MEM_DRIVER_NAME);
		result = -ENOMEM;
        goto err_chrdev;
    }

	ResetMems();

	return 0;
err_chrdev:
    unregister_chrdev_region(base_dev_no, DEVICE_COUNT);
err_class:
    class_destroy(mem_class);

err:
	if (hlina_chunks)
		vfree(hlina_chunks);

	return result;
}

/* Cycle through the buffers we have, give the first free one */
static int AllocMemory(unsigned long *busaddr, unsigned long size,
		       const struct file *filp)
{
	int i = 0;
	int j = 0;
	unsigned int skip_chunks = 0;

	/* calculate how many chunks we need; round up to chunk boundary */
	unsigned int alloc_chunks = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

	*busaddr = 0;

	spin_lock(&mem_lock);

	/* run through the chunk table */
	for (i = 0; i < chunks;) {
		skip_chunks = 0;
		/* if this chunk is available */
		if (!hlina_chunks[i].chunks_reserved) {
			/* check that there is enough memory left */
			if (i + alloc_chunks > chunks)
				break;
			/* check that there is enough consecutive
			 * chunks available
			 */
			for (j = i; j < i + alloc_chunks; j++) {
				if (hlina_chunks[j].chunks_reserved) {
					skip_chunks = 1;
					/* skip the used chunks */
					i = j + hlina_chunks[j].chunks_reserved;
					break;
				}
			}

			/* if enough free memory found */
		if (!skip_chunks) {
			*busaddr = hlina_chunks[i].bus_address;
			hlina_chunks[i].filp = filp;
			hlina_chunks[i].chunks_reserved = alloc_chunks;
			break;
			}
		} else {
			/* skip the used chunks */
			i += hlina_chunks[i].chunks_reserved;
		}
	}
	spin_unlock(&mem_lock);

	if (*busaddr == 0) {
		pr_warn("memalloc: Allocation FAILED: size =%lu\n",
			size);
		return -EFAULT;
	}
	PDEBUG("MEMALLOC OK: size: %lu, reserved: %ld\n", size,
	       alloc_chunks * CHUNK_SIZE);

	return 0;
}

/* Free a buffer based on bus address */
static int FreeMemory(unsigned long busaddr, const struct file *filp)
{
	int i = 0;

	spin_lock(&mem_lock);

	for (i = 0; i < chunks; i++) {
	/* user space SW has stored the translated bus address,
	 * add addr_transl to translate back to our address space
	 */
		if (hlina_chunks[i].bus_address == busaddr + addr_transl) {
			if (hlina_chunks[i].filp == filp) {
				hlina_chunks[i].filp = NULL;
				hlina_chunks[i].chunks_reserved = 0;
			} else {
				pr_warn("memalloc: Owner mismatch while freeing memory!\n");
			}
			break;
		}
	}

	spin_unlock(&mem_lock);

	return 0;
}

/* Reset "used" status */
static void ResetMems(void)
{
	int i = 0;
	unsigned long ba = alloc_base;

	spin_lock(&mem_lock);

	for (i = 0; i < chunks; i++) {
		hlina_chunks[i].bus_address = ba;
		hlina_chunks[i].filp = NULL;
		hlina_chunks[i].chunks_reserved = 0;

		ba += CHUNK_SIZE;
	}

	spin_unlock(&mem_lock);
}

static int memlloc_mmap(struct file *filp, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;
	int ret = 0;
	unsigned long start = 0, end = 0;
	unsigned long base_start = 0, base_end = 0;

	PDEBUG("%s %08lx-%08lx -> %08lx, %s\n", __func__,
		   (long)(vma->vm_pgoff << PAGE_SHIFT),
		   (long)(vma->vm_pgoff << PAGE_SHIFT) + (int)(size),
		   (long)vma->vm_start,
		   (filp->f_flags & O_SYNC) ? "uncached" : "cached");

	start = vma->vm_pgoff << PAGE_SHIFT;
	end = start + size;
	base_start = alloc_base;
	base_end   = ((alloc_base + alloc_size * 1024 * 1024) /
				  PAGE_SIZE + 1) * PAGE_SIZE;
	if (start < base_start || base_end < end) {
		pr_err("Invalid adress %08lx-%08lx\n", start, end);
		return -EINVAL;
	}

	// support only uncached mode
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	vma->vm_ops = &memlloc_vm_ops;

	ret = remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, size,
		  vma->vm_page_prot);
	if (ret != 0) {
		pr_err("remap_pfn_range() failed.\n");
		goto err_out;
	}

	return 0;

err_out:
	return ret;
}

module_init(memalloc_init);
module_exit(memalloc_cleanup);

/* module description */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Verisilicon");
MODULE_DESCRIPTION("Linear RAM allocation");
