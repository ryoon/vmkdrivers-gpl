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
#ifndef __NX_NIC_VMK_H__
#define __NX_NIC_VMK_H__

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__) || defined (__CONFIG_VMNIX__))

#include <kernel_compatibility.h>

#include "vmklinux_dist.h"

#ifndef PCI_CAP_ID_MSIX
#define PCI_CAP_ID_MSIX 0x11
#endif

#undef CONFIG_FW_LOADER
#undef CONFIG_FW_LOADER_MODULE

#define UNM_NETIF_F_TSO

#define local_bh_enable()
#define local_bh_disable()

#define BOUNCE_LOCK(__lock, flags) spin_lock_irqsave((__lock),flags)
#define BOUNCE_UNLOCK(__lock, flags) spin_unlock_irqrestore((__lock),flags)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18))
#define ALLOC_SKB(adapter, size, flag) netdev_alloc_skb((adapter)->netdev, (size))
#else
#define ALLOC_SKB(adapter, size, flag) alloc_skb((size), (flag))
#endif

#define NX_ADJUST_SMALL_PKT_LEN(SKB)					\
	do { 							\
		if(rarely((SKB)->len < ETH_ZLEN)) {		\
			skb_put(SKB, ETH_ZLEN - (SKB)->len);	\
		}						\
	} while(0)


#define PREPARE_TO_WAIT(WQ_HEAD, WQ_ENTRY, STATE) \
	prepare_to_wait((WQ_HEAD), (WQ_ENTRY), (STATE))

#define SCHEDULE_TIMEOUT(EVENT, TIMEOUT, LOCK) \
	        schedule_timeout((TIMEOUT))

#define ESX_PHYS_TO_KMAP(MADDR, LEN) \
	ioremap(MADDR, LEN)

#define ESX_PHYS_TO_KMAP_FREE(VADDR) \
	iounmap(VADDR)

#define NETQ_MAX_RX_CONTEXTS 7
#define INVALID_INIT_NETQ_RX_CONTEXTS -1

#ifdef __VMKNETDDI_QUEUEOPS__

#define MULTICTX_IS_RX(type) ((type) == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX)
#define MULTICTX_IS_TX(type) ((type) == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX)
int nx_nic_netqueue_ops(vmknetddi_queueops_op_t op, void *args);

#define NX_SET_NETQ_OPS(DEV, OPS) \
	VMKNETDDI_REGISTER_QUEUEOPS((DEV), (OPS))

#define nx_set_skb_queueid(SKB, RCTX) 						  \
		do {								  \
			if(RCTX->this_id) {  					  \
				vmknetddi_queueops_set_skb_queueid(SKB,           \
				VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(RCTX->this_id)); \
			} 							  \
		}while (0) 
#endif

#define FREE_NETDEV(DEV) \
        do { \
                if((DEV)) {  \
                        if((DEV)->outputList) { \
                                kfree((DEV)->outputList); \
                        } \
                        free_netdev((DEV)); \
                } \
        } while(0)

extern int vmklnx_netdev_high_dma_overflow(struct sk_buff *skb, short gb_limit);
extern struct sk_buff * vmklnx_netdev_high_dma_workaround(struct sk_buff *base);

#define NX_NIC_HANDLE_HIGHDMA_OVERFLOW(ADAPTER, SKB)      \
	do { \
		if (SKB) {                                              \
			if (vmklnx_netdev_high_dma_overflow(SKB, \
						(ADAPTER)->dma_mask)) {\
				dev_kfree_skb_any(SKB);			\
				SKB = NULL;				\
			}                                               \
		}                                                       \
	}while (0)

#endif
#endif
