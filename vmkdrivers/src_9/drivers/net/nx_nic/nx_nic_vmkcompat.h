/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
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
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */
#ifndef __NX_NIC_VMKCOMPAT_H__
#define __NX_NIC_VMKCOMPAT_H__

#if (!defined(__VMKERNEL_MODULE__) && !defined(__VMKLNX__))
#define VMK_SET_MODULE_VERSION(DRIVER_VERSION) 1
#define BOUNCE_LOCK(__lock, flags) 
#define BOUNCE_UNLOCK(__lock, flags)

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,18))
#define ALLOC_SKB(adpater, size, flag) netdev_alloc_skb((adapter)->netdev, (size))
#else
#define ALLOC_SKB(adapter, size, flag) dev_alloc_skb((size))
#endif

#define SKB_ADJUST_PKT_MA(SKB,LEN)
#define NX_ADJUST_SMALL_PKT_LEN(SKB)

#define NX_NIC_HANDLE_HIGHDMA_OVERFLOW(ADAPTER, SKB)

#define nx_setup_vlan_buffers(A) 0
#define nx_setup_tx_vmkbounce_buffers(A) 0
#define nx_free_vlan_buffers(A) 
#define nx_free_tx_vmkbounce_buffers(A)
#define nx_free_frag_bounce_buf(A, B)
#define nx_handle_large_addr(A, B, C, D, E, F) 0

#define VMQ_TYPE_RX 1
#define VMQ_TYPE_TX 2

#define MULTICTX_IS_RX(type) ((type) == VMQ_TYPE_RX)
	#define MULTICTX_IS_TX(type) ((type) == VMQ_TYPE_TX)

#define NETQ_MAX_RX_CONTEXTS 7
#define INVALID_INIT_NETQ_RX_CONTEXTS -1

#define NX_SET_NETQ_OPS(DEV, OPS)
#define nx_set_skb_queueid(SKB, RCTX)

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,7)
#define PREPARE_TO_WAIT(WQ_HEAD, WQ_ENTRY, STATE) \
	prepare_to_wait((WQ_HEAD), (WQ_ENTRY), (STATE))

#else
#define PREPARE_TO_WAIT(WQ_HEAD, WQ_ENTRY, STATE) \
	do { \
		unsigned long flags; \
		(WQ_ENTRY)->flags &= ~WQ_FLAG_EXCLUSIVE; \
		spin_lock_irqsave(&(WQ_HEAD)->lock, flags); \
		if (list_empty(&(WQ_ENTRY)->task_list)) \
			__add_wait_queue((WQ_HEAD), (WQ_ENTRY)); \
		set_current_state((STATE)); \
		spin_unlock_irqrestore(&(WQ_HEAD)->lock, flags); \
	} while (0)
#endif

#define SCHEDULE_TIMEOUT(EVENT, TIMEOUT, LOCK) \
	schedule_timeout((TIMEOUT))

#define FREE_NETDEV(DEV) \
        do {  \
                if((DEV)) { \
                        free_netdev((DEV)); \
                } \
        } while(0)

#endif
#endif
