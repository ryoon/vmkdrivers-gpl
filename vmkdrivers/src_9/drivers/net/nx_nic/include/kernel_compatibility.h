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
#ifndef __KERNEL_COMPATIBILITY_H__
#define __KERNEL_COMPATIBILITY_H__

#include <linux/version.h>
#include <net/sock.h>

#if (defined(__VMKERNEL_MODULE__) || defined(__VMKLNX__) || defined (__CONFIG_VMNIX__))
#define ESX
#endif


#if defined(NETIF_F_TSO)

#if defined(NETIF_F_TSO6)
#define TSO_ENABLED(netdev) ((netdev)->features & (NETIF_F_TSO | NETIF_F_TSO6))
#else 
#define TSO_ENABLED(netdev) ((netdev)->features & (NETIF_F_TSO))
#endif

#else
#define TSO_ENABLED(netdev) 0
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define DEV_BASE		dev_base
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22) && LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
#define DEV_BASE                dev_base_head
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
#define IP_HDR(SKB)     ((SKB)->nh.raw)
#define IPV6_HDR(SKB)   ((SKB)->nh.raw)
#define IP_HDR_OFFSET(skb)      (skb->nh.raw - skb->data)
#define TCP_HDR(skb)            (skb->h.th)
#define TCP_HDR_OFFSET(skb)     (skb->h.raw - skb->data)
#define NW_HDR_SIZE(SKB)        (SKB->h.raw - SKB->nh.raw)
#define MAC_HDR(skb)		(skb->mac.raw)		
#define SKB_TCP_HDR(skb)	(skb->h.th)
#define SKB_IP_HDR(skb)		(skb->nh.iph)
#define SKB_IPV6_HDR(skb)	(skb->nh.ipv6h)
#define SKB_MAC_HDR(skb)	(skb->mac.raw)
#define TCP_HDR_TYPE		struct tcphdr
#define IP_HDR_TYPE		struct iphdr
#define IPV6_HDR_TYPE		struct ipv6hdr
#define GET_TCP_HDR(skb, tcph)  	((TCP_HDR_TYPE *)(tcph))
#define GET_MAC_HDR(skb, mach)  	(mach)
#define GET_IP_HDR(skb, iph)     	((IP_HDR_TYPE *)(iph))
#define GET_IPV6_HDR(skb, ipv6h) 	((IPV6_HDR_TYPE *)(ipv6h))

#else /* For LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22) */

#define IP_HDR(SKB)      (skb_network_header((SKB)))
#define IPV6_HDR(SKB)  	 (skb_transport_header((SKB)))
#define IP_HDR_OFFSET(skb)      skb_network_offset(skb)
#define TCP_HDR(skb)            tcp_hdr(skb)
#define TCP_HDR_OFFSET(skb)     skb_transport_offset(skb)
#define NW_HDR_SIZE(SKB)             \
	(skb_transport_offset(SKB) - skb_network_offset(SKB))
#define MAC_HDR(skb)		skb_mac_header(skb)
#define TCP_HDR_TYPE		char
#define IP_HDR_TYPE		char
#define IPV6_HDR_TYPE		char
#define SKB_MAC_HDR(skb)	skb->mac_header
#define SKB_TCP_HDR(skb)	skb->transport_header
#define SKB_IP_HDR(skb) 	skb->network_header
#define SKB_IPV6_HDR(skb)	skb->network_header

#ifdef NET_SKBUFF_DATA_USES_OFFSET

#define GET_TCP_HDR(skb, tcph)  	(int)((unsigned long)(tcph) - (unsigned long)(skb)->head)
#define GET_MAC_HDR(skb, mach)  	(int)((unsigned long)(mach) - (unsigned long)(skb)->head)
#define GET_IP_HDR(skb, iph)    	(int)((unsigned long)(iph) - (unsigned long)(skb)->head)
#define GET_IPV6_HDR(skb, ipv6h)	(int)((unsigned long)(ipv6h) - (unsigned long)(skb)->head)

#else  /* For SKB not using OFFSET */

#define GET_TCP_HDR(skb, tcph)    	((TCP_HDR_TYPE *)(tcph))
#define GET_MAC_HDR(skb, mach)   	(char *)(mach)
#define GET_IP_HDR(skb, iph)      	((IP_HDR_TYPE *)(iph))
#define GET_IPV6_HDR(skb, ipv6h) 	((IPV6_HDR_TYPE *)(ipv6h))

#endif /* End of #ifdef NET_SKBUFF_DATA_USES_OFFSET */

#endif /* End of KERNEL_VERSION(2,6,22) #if */


#define PROTO_IS_IP(SKB, TAGGED) \
	(((SKB)->protocol == __constant_htons(ETH_P_IP)) || \
	 (TAGGED && (((struct vlan_ethhdr *) \
		      ((SKB)->data))->h_vlan_encapsulated_proto == __constant_htons(ETH_P_IP))))

#define NW_HDR_LEN(SKB, TAGGED)  \
	        (NW_HDR_SIZE(SKB))

#define NW_HDR_OFFSET(SKB, TAGGED)  \
	        (IP_HDR_OFFSET(SKB))

#define PROTO_IS_TCP(SKB, TAGGED) \
	        (((struct iphdr *)IP_HDR((SKB)))->protocol == IPPROTO_TCP)

#define PROTO_IS_UDP(SKB, TAGGED) \
	        (((struct iphdr *)IP_HDR((SKB)))->protocol == IPPROTO_UDP)

#define PROTO_IS_IPV6(SKB, TAGGED) \
	(((SKB)->protocol == __constant_htons(ETH_P_IPV6)) || \
	 (TAGGED && (((struct vlan_ethhdr *) \
		      ((SKB)->data))->h_vlan_encapsulated_proto == __constant_htons(ETH_P_IPV6))))

#define PROTO_IS_TCPV6(SKB, TAGGED) \
	        (((struct ipv6hdr *)IPV6_HDR((SKB)))->nexthdr == IPPROTO_TCP)

#define PROTO_IS_UDPV6(SKB, TAGGED) \
	        (((struct ipv6hdr *)IPV6_HDR((SKB)))->nexthdr == IPPROTO_UDP)

#define PCI_MODULE_INIT(drv)    pci_register_driver(drv)

#define	NX_KC_ROUTE_DEVICE(RT)	(((RT)->idev)->dev)

#define SK_ERR 		sk_err
#define SK_ERR_SOFT     sk_err_soft
#define SK_STATE_CHANGE sk_state_change
#define SK_PROT		sk_prot
#define SK_PROTOCOL	sk_protocol
#define SK_PROTINFO	sk_protinfo
#define SK_DESTRUCT	sk_destruct
#define SK_FAMILY	sk_family
#define SK_FLAGS	sk_flags
#define SK_DATA_READY   sk_data_ready
#define SK_RCVTIMEO     sk_rcvtimeo
#define SK_SNDTIMEO     sk_sndtimeo
#define SK_STATE        sk_state
#define SK_SHUTDOWN	sk_shutdown
#define SK_SLEEP        sk_sleep
#define SPORT(s)	(inet_sk(s))->num
#define SK_LINGER_FLAG(SK)      sock_flag((SK), SOCK_LINGER)
#define SK_OOBINLINE_FLAG(SK)	sock_flag((SK), SOCK_URGINLINE)
#define SK_KEEPALIVE_FLAG(SK)	sock_flag((SK), SOCK_KEEPOPEN)
#define SK_LINGERTIME   sk_lingertime
#define SK_REUSE	sk_reuse
#define SK_SOCKET       sk_socket
#define SK_RCVBUF      sk_rcvbuf
#define SK_INET(s)      (inet_sk(s))
#define NX_SOCKOPT_TASK	nx_sockopt_thread

#define NOFC 0
#define FC1  1
#define FC2  2
#define FC3  3
#define FC4  4
#define FC5  5

/*
 * sk_alloc - varieties taken care here.
 */
#define	SK_ALLOC(FAMILY, PRIORITY, PROTO, ZERO_IT)		\
	sk_alloc((FAMILY), (PRIORITY), (PROTO), (ZERO_IT))

#define USER_MSS        rx_opt.user_mss
#define TCP_SOCK  	struct tcp_sock
#define PINET6(SK)	SK_INET(SK)->pinet6

#define MSS_CACHE       mss_cache

#define BIND_HASH(SK)   inet_csk(SK)->icsk_bind_hash
#define QUICKACK        inet_conn.icsk_ack.pingpong
#define DEFER_ACCEPT	inet_conn.icsk_accept_queue.rskq_defer_accept
#define SYN_RETRIES	inet_conn.icsk_syn_retries

#if defined(CONFIG_X86_64)
#define PAGE_KERNEL_FLAG  PAGE_KERNEL_EXEC
#else 
#define PAGE_KERNEL_FLAG  PAGE_KERNEL
#endif

#define	NX_KC_DST_MTU(DST)	dst_mtu((DST))

typedef struct work_struct	nx_kc_work_queue_t;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,20))
#define	NX_KC_INIT_WORK(TASK, CB_FUNC, OBJ)	INIT_WORK((TASK), (CB_FUNC))
#else
#define	NX_KC_INIT_WORK(TASK, CB_FUNC, OBJ)	\
	INIT_WORK((TASK), (CB_FUNC), (OBJ))
#endif
#define	NX_KC_SCHEDULE_WORK(TASK)		schedule_work((TASK))
#define	NX_KC_SCHEDULE_DELAYED_WORK(TASK, DELAY)	\
	schedule_delayed_work((TASK), (DELAY))

#define PM_MESSAGE_T pm_message_t

#ifndef module_param
#define module_param(v,t,p) MODULE_PARM(v, "i");
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27)	&& !defined(__VMKLNX__)
static inline void ___list_splice(const struct list_head *list,
				 struct list_head *prev,
				 struct list_head *next)
{
	struct list_head *first = list->next;
	struct list_head *last = list->prev;

	first->prev = prev;
	prev->next = first;

	last->next = next;
	next->prev = last;
}

/**
 * list_splice_tail - join two lists, each list being a queue
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 */
static inline void list_splice_tail(struct list_head *list,
				struct list_head *head)
{
	if (!list_empty(list))
		___list_splice(list, head->prev, head);
}

/**
 * list_splice_tail_init - join two lists and reinitialise the emptied list
 * @list: the new list to add.
 * @head: the place to add it in the first list.
 *
 * Each of the lists is a queue.
 * The list at @list is reinitialised
 */
static inline void list_splice_tail_init(struct list_head *list,
					 struct list_head *head)
{
	if (!list_empty(list)) {
		___list_splice(list, head->prev, head);
		INIT_LIST_HEAD(list);
	}
}
#endif // LINUX_VERSION_CODE < KERNEL_VERSION(2,6,27) && !defined(__VMKLNX__)

#ifndef NETDEV_TX_OK 
#define NETDEV_TX_OK 0
#endif

#ifndef NETDEV_TX_BUSY 
#define NETDEV_TX_BUSY 1
#endif

#define NX_NETIF_F_SG           NETIF_F_SG
#define NX_NETIF_F_HW_CSUM      NETIF_F_HW_CSUM

#ifdef NETIF_F_TSO
#define NX_NETIF_F_TSO          NETIF_F_TSO
#else
#define NX_NETIF_F_TSO          0
#endif

#ifdef NETIF_F_TSO6
#define NX_NETIF_F_TSO6         NETIF_F_TSO6
#else
#define NX_NETIF_F_TSO6         0

#endif
#define NX_NETIF_F_HIGHDMA      NETIF_F_HIGHDMA

#define NX_SET_VLAN_DEV_FEATURES(DEV, VDEV)     \
	do {                                    \
		if((VDEV) && ((VDEV) != (DEV))) {               \
			(VDEV)->features |= \
			((DEV)->features & (NX_NETIF_F_SG | NX_NETIF_F_HW_CSUM | \
				NX_NETIF_F_TSO | NX_NETIF_F_TSO6 | \
				NX_NETIF_F_HIGHDMA));\
		}                               \
	}while (0)

#define nx_pci_dma_mapping_error(dev, addr) pci_dma_mapping_error(addr)

#define MSLEEP_INTERRUPTIBLE(_MSEC_)       msleep_interruptible(_MSEC_);

#if defined(__VMKERNEL_MODULE__)
#ifndef __HAVE_ARCH_CMPXCHG
#define __HAVE_ARCH_CMPXCHG 1
static inline unsigned long __cmpxchg(volatile void *ptr, unsigned long old,
                                      unsigned long new, int size)
{
        unsigned long prev;
        switch (size) {
        case 1:
                __asm__ __volatile__(LOCK_PREFIX "cmpxchgb %b1,%2"
                                     : "=a"(prev)
                                     : "q"(new), "m"(*__xg(ptr)), "0"(old)
                                     : "memory");
                return prev;
        case 2:
                __asm__ __volatile__(LOCK_PREFIX "cmpxchgw %w1,%2"
                                     : "=a"(prev)
                                     : "q"(new), "m"(*__xg(ptr)), "0"(old)
                                     : "memory");
                return prev;
        case 4:
                __asm__ __volatile__(LOCK_PREFIX "cmpxchgl %1,%2"
                                     : "=a"(prev)
                                     : "q"(new), "m"(*__xg(ptr)), "0"(old)
                                     : "memory");
                return prev;
        }
        return old;
}

#define cmpxchg(ptr,o,n)\
        ((__typeof__(*(ptr)))__cmpxchg((ptr),(unsigned long)(o),\
                                        (unsigned long)(n),sizeof(*(ptr))))
#endif /* __HAVE_ARCH_CMPXCHG */
#endif /* defined(__VMKERNEL_MODULE__) */

#endif  /* __KERNEL_COMPATIBILITY_H__ */
