/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 */
#ifndef _NX_PLAT_H_
#define _NX_PLAT_H_


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,24)
#include <linux/sched.h>
#endif

#include <asm/io.h>
#include <asm/delay.h>
#include "nx_errorcode.h"
/*
 * Include platform specific headers here
 */

/*
 * Platform dependent data types
 */
#define SYS_TYPES
typedef signed char         I8;
typedef signed short        I16;
typedef signed int          I32;
typedef signed long long    I64;
typedef unsigned char       U8;
typedef unsigned short      U16;
typedef unsigned int        U32;
typedef unsigned long long  U64;

typedef U64  __uint64_t;
typedef U32  nx_rcode_t;
typedef U8   BOOLEAN;


#define INLINE inline

/*
 * Forward declaration for platform/driver model specific device structure.
 * We derive device handle type from the platform specific device structure.
 */

typedef dma_addr_t     nx_dma_addr_t;
typedef struct unm_adapter_s * nx_dev_handle_t;


/*
 * Common Error code types.
 * Values are defined in nx_errorcode.h.
 */

/*
 * Delay implementation for various models
 */
#define nx_os_udelay(time) udelay(time)

#define nx_os_zero_memory(DEST, LEN) 			memset((DEST), 0, (LEN))
#define nx_os_copy_memory(DEST, SRC, LEN)               memcpy((DEST), (SRC), (LEN))

/*
 * Allocate non-paged, non contiguous memory . Tag can be used for debug 
 * purposes.
 */

#define NX_OS_ALLOC_KERNEL  GFP_KERNEL
#define NX_OS_ALLOC_ATOMIC  GFP_ATOMIC

extern U32 nx_os_alloc_mem(nx_dev_handle_t handle, void** addr, U32 len, U32 flags, 
					U32 dbg_tag);


/*
 * Free non-paged non contiguous memory 
 */
extern U32 nx_os_free_mem(nx_dev_handle_t handle, void* addr, U32 len, U32 flags);

/*
 * Allocate non-paged dma memory
 */
extern U32 nx_os_alloc_dma_mem(nx_dev_handle_t handle, void** vaddr, 
			       nx_dma_addr_t* paddr, U32 len, U32 flags);

/*
 * Free DMA memory
 */
extern void nx_os_free_dma_mem(nx_dev_handle_t handle, void* vaddr,
			       nx_dma_addr_t paddr, U32 len, U32 flags);


/*
 * Add passed value and 64 bit physical address
 */
#define nx_os_dma_addr_add(ADDR, VAL) (ADDR + VAL)
/*
 * Returns 64 bit physical address
 */
#define nx_os_dma_addr_to_u64(ADDR) ((U64)ADDR)
/*
 * Low and high 32 bits of dma address
 */
#define nx_os_dma_addr_low(ADDR)    (ADDR)
#define nx_os_dma_addr_high(ADDR)   ((ADDR)>>32)	

/*
 * Endian conversion macros
 */

#define NX_OS_CPU_TO_LE16(a) (cpu_to_le16(a))
#define NX_OS_CPU_TO_LE32(a) (cpu_to_le32(a))
#define NX_OS_CPU_TO_LE64(a) (cpu_to_le64(a))
#define NX_OS_LE16_TO_CPU(a) (le16_to_cpu(a))
#define NX_OS_LE32_TO_CPU(a) (le32_to_cpu(a))
#define NX_OS_LE64_TO_CPU(a) (le64_to_cpu(a))

/*
 * Spin locks
 */
#define NX_SPIN_LOCK
#define NX_SPIN_LOCK_INIT(_lock_)
#define NX_ACQUIRE_LOCK(_ctx1_, _ctx2_,_ctx3_,_lock)
#define NX_RELEASE_LOCK(_ctx1_,_lock_)

/*
 * Memory mapped register access routines
 */
#define nx_os_write_reg_mem_u8(DEV, ADDRESS, DATA)              writeb(DATA, ((void *)ADDRESS))
#define nx_os_write_reg_mem_u16(DEV, ADDRESS, DATA)             writew(DATA, ((void *)ADDRESS))
#define nx_os_write_reg_mem_u32(DEV, ADDRESS, DATA)             writel(DATA, ((void *)ADDRESS))
#define nx_os_write_reg_mem_buf(DEV, ADDRESS, DATA, SIZE, TYPE) writeq(DATA, ((void *)ADDRESS))

#define nx_os_read_reg_mem_u8(DEV, ADDRESS, DATA)               (*(DATA) = readb((void *)(ADDRESS)))
#define nx_os_read_reg_mem_u16(DEV, ADDRESS, DATA)              (*(DATA) = readw((void *)(ADDRESS)))
#define nx_os_read_reg_mem_u32(DEV, ADDRESS, DATA)              (*(DATA) = readl((void *)(ADDRESS)))
#define nx_os_read_reg_mem_buf(DEV, ADDRESS, DATA, SIZE, TYPE)  (*(DATA) = readq((void *)(ADDRESS))) 


/* Register Access
 */
extern void nx_os_nic_reg_read_w0(nx_dev_handle_t handle, U32 index, U32 *value);
extern void nx_os_nic_reg_write_w0(nx_dev_handle_t handle, U32 index, U32 value);
extern void nx_os_nic_reg_read_w1(nx_dev_handle_t handle, U64 off, U32 *value);
extern void nx_os_nic_reg_write_w1(nx_dev_handle_t handle, U64 off, U32 val);

#define NX_USE_NEW_ALLOC
#define NX_OS_USE_API_LOCK
#define nx_os_schedule()     schedule()
#define nx_os_relax(val)     cpu_relax()

typedef struct nx_os_wait_event_s {
        struct list_head        list;
        wait_queue_head_t       wq;
        uint8_t                 comp_id;
        volatile U32            trigger;
        volatile U32            use_wake_up;
        U32                     active;
        U64                     *rsp_word;
} nx_os_wait_event_t;


/*
 * Trace macros and functions
 */

/* Global debug mask */

extern U32					nx_dbg_mask;
/*
 * Debug masks that can be set by user
 */ 

#define NX_DBG_ERROR			1
#define NX_DBG_INIT			2
#define NX_DBG_WARN			4
#define NX_DBG_INFO			8

#define NX_DBG_ERROR_MASK		NX_DBG_ERROR
#define NX_DBG_WARN_MASK		(NX_DBG_ERROR | NX_DBG_WARN)
#define NX_DBG_ALL			(NX_DBG_ERROR | NX_DBG_INIT | NX_DBG_WARN | \
					NX_DBG_INFO)

#ifdef DEBUG
#define NX_DBG_DEFAULT_MASK		NX_DBG_ALL
#else
#define NX_DBG_DEFAULT_MASK		NX_DBG_ERROR
#endif

//#define NX_DBGPRINTF(mask,fmt,...)      printk fmt, __VA_ARGS__ 1
#define NX_DBGPRINTF(mask,fmt,...)      
#define NX_WARN(...)      		  printk("Warning %s:", __FUNCTION__), printk(__VA_ARGS__)
#define NX_ERR(...)      		  printk("Error %s:", __FUNCTION__), printk(__VA_ARGS__)
#define NX_DBGBREAKPOINT()

#endif /*_PLAT_H_*/
