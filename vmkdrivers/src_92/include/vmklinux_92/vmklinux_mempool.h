/* ****************************************************************
 * Portions Copyright 2010 VMware, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * ****************************************************************/

/*
 * vmklinux_mempool.h --
 *
 *      Prototypes for functions used in vmklinux to create the
 *      vmklinux parent memory pool and in device drivers to get the
 *      vmklinux parent memory pool.
 */

#ifndef _VMKLINUX_MEM_POOL_H_
#define _VMKLINUX_MEM_POOL_H_

#if defined(VMKLINUX)
VMK_ReturnStatus vmklnx_mem_pool_parent_init(void);
#else
VMK_ReturnStatus vmklnx_mem_pool_get_parent(vmk_MemPool *parent_mem_pool);
#endif

#endif // _VMKLINUX_MEM_POOL_H_
