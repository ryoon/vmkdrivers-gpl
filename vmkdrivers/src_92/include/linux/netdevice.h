/*
 * Portions Copyright 2008 - 2013 VMware, Inc.
 */
/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the Interfaces handler.
 *
 * Version:	@(#)dev.h	1.0.10	08/12/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 *		Corey Minyard <wf-rch!minyard@relay.EU.net>
 *		Donald J. Becker, <becker@cesdis.gsfc.nasa.gov>
 *		Alan Cox, <Alan.Cox@linux.org>
 *		Bjorn Ekwall. <bj0rn@blox.se>
 *              Pekka Riikonen <priikone@poseidon.pspt.fi>
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *		Moved to /usr/include/linux for NET3
 */
#ifndef _LINUX_NETDEVICE_H
#define _LINUX_NETDEVICE_H

#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>

#ifdef __KERNEL__
#include <asm/atomic.h>
#include <asm/cache.h>
#include <asm/byteorder.h>

#include <linux/device.h>
#include <linux/percpu.h>
#include <linux/dmaengine.h>

#if defined(__VMKLNX__)
#include <linux/delay.h>
#include "vmkapi.h"
#include <linux/inet_lro.h>
#include <net/dcbnl.h> 

extern unsigned int vmklnxLROMaxAggr;
#endif /* defined(__VMKLNX__) */

struct divert_blk;
struct vlan_group;
struct ethtool_ops;
struct netpoll_info;
					/* source back-compat hooks */

/**
 *  SET_ETHTOOL_OPS - Set the ethtool ops pointer of the network device
 *  @netdev: network device
 *  @ops: pointer to the ethtool_ops data structure
 *
 *  Sets the ethtool operations pointer of the network device to the specified
 *  ethtool_ops pointer.
 *
 *  SYNOPSIS:
 *     #define SET_ETHTOOL_OPS(netdev,ops)
 *
 *  RETURN VALUE:
 *   None
 *
 */
/* _VMKLNX_CODECHECK_: SET_ETHTOOL_OPS */

#define SET_ETHTOOL_OPS(netdev,ops) \
	( (netdev)->ethtool_ops = (ops) )

#define HAVE_ALLOC_NETDEV		/* feature macro: alloc_xxxdev
					   functions are available. */
#define HAVE_FREE_NETDEV		/* free_netdev() */
#define HAVE_NETDEV_PRIV		/* netdev_priv() */

#define NET_XMIT_SUCCESS	0
#define NET_XMIT_DROP		1	/* skb dropped			*/
#define NET_XMIT_CN		2	/* congestion notification	*/
#define NET_XMIT_POLICED	3	/* skb is shot by police	*/
#define NET_XMIT_BYPASS		4	/* packet does not leave via dequeue;
					   (TC use only - dev_queue_xmit
					   returns this as NET_XMIT_SUCCESS) */

/* Backlog congestion levels */
#define NET_RX_SUCCESS		0   /* keep 'em coming, baby */
#define NET_RX_DROP		1  /* packet dropped */
#define NET_RX_CN_LOW		2   /* storm alert, just in case */
#define NET_RX_CN_MOD		3   /* Storm on its way! */
#define NET_RX_CN_HIGH		4   /* The storm is here */
#define NET_RX_BAD		5  /* packet dropped due to kernel error */

#define net_xmit_errno(e)	((e) != NET_XMIT_CN ? -ENOBUFS : 0)

#endif

#define MAX_ADDR_LEN	32		/* Largest hardware address length */

/* Driver transmit return codes */
#define NETDEV_TX_OK 0		/* driver took care of packet */
#define NETDEV_TX_BUSY 1	/* driver tx path was busy*/
#define NETDEV_TX_LOCKED -1	/* driver tx lock was already taken */

/*
 *	Compute the worst case header length according to the protocols
 *	used.
 */
 
#if !defined(CONFIG_AX25) && !defined(CONFIG_AX25_MODULE) && !defined(CONFIG_TR)
#define LL_MAX_HEADER	32
#else
#if defined(CONFIG_AX25) || defined(CONFIG_AX25_MODULE)
#define LL_MAX_HEADER	96
#else
#define LL_MAX_HEADER	48
#endif
#endif

#if !defined(CONFIG_NET_IPIP) && \
    !defined(CONFIG_IPV6) && !defined(CONFIG_IPV6_MODULE)
#define MAX_HEADER LL_MAX_HEADER
#else
#define MAX_HEADER (LL_MAX_HEADER + 48)
#endif

#if defined(__VMKLNX__)
#define VMKLNX_PKT_HEAP_MAX_SIZE 224
#endif

/*
 *	Network device statistics. Akin to the 2.0 ether stats but
 *	with byte counters.
 */
 
struct net_device_stats
{
	unsigned long	rx_packets;		/* total packets received	*/
	unsigned long	tx_packets;		/* total packets transmitted	*/
	unsigned long	rx_bytes;		/* total bytes received 	*/
	unsigned long	tx_bytes;		/* total bytes transmitted	*/
	unsigned long	rx_errors;		/* bad packets received		*/
	unsigned long	tx_errors;		/* packet transmit problems	*/
	unsigned long	rx_dropped;		/* no space in linux buffers	*/
	unsigned long	tx_dropped;		/* no space available in linux	*/
	unsigned long	multicast;		/* multicast packets received	*/
	unsigned long	collisions;

	/* detailed rx_errors: */
	unsigned long	rx_length_errors;
	unsigned long	rx_over_errors;		/* receiver ring buff overflow	*/
	unsigned long	rx_crc_errors;		/* recved pkt with crc error	*/
	unsigned long	rx_frame_errors;	/* recv'd frame alignment error */
	unsigned long	rx_fifo_errors;		/* recv'r fifo overrun		*/
	unsigned long	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	unsigned long	tx_aborted_errors;
	unsigned long	tx_carrier_errors;
	unsigned long	tx_fifo_errors;
	unsigned long	tx_heartbeat_errors;
	unsigned long	tx_window_errors;
	
	/* for cslip etc */
	unsigned long	rx_compressed;
	unsigned long	tx_compressed;
};


/* Media selection options. */
enum {
        IF_PORT_UNKNOWN = 0,
        IF_PORT_10BASE2,
        IF_PORT_10BASET,
        IF_PORT_AUI,
        IF_PORT_100BASET,
        IF_PORT_100BASETX,
        IF_PORT_100BASEFX
};

#ifdef __KERNEL__

#include <linux/cache.h>
#include <linux/skbuff.h>

struct neighbour;
struct neigh_parms;
struct sk_buff;

struct netif_rx_stats
{
	unsigned total;
	unsigned dropped;
	unsigned time_squeeze;
	unsigned cpu_collision;
};

DECLARE_PER_CPU(struct netif_rx_stats, netdev_rx_stat);


/*
 *	We tag multicasts with these structures.
 */
 
struct dev_mc_list
{	
	struct dev_mc_list	*next;
	__u8			dmi_addr[MAX_ADDR_LEN];
	unsigned char		dmi_addrlen;
	int			dmi_users;
	int			dmi_gusers;
};

struct hh_cache
{
	struct hh_cache *hh_next;	/* Next entry			     */
	atomic_t	hh_refcnt;	/* number of users                   */
	unsigned short  hh_type;	/* protocol identifier, f.e ETH_P_IP
                                         *  NOTE:  For VLANs, this will be the
                                         *  encapuslated type. --BLG
                                         */
	int		hh_len;		/* length of header */
	int		(*hh_output)(struct sk_buff *skb);
	rwlock_t	hh_lock;

	/* cached hardware header; allow for machine alignment needs.        */
#define HH_DATA_MOD	16
#define HH_DATA_OFF(__len) \
	(HH_DATA_MOD - (((__len - 1) & (HH_DATA_MOD - 1)) + 1))
#define HH_DATA_ALIGN(__len) \
	(((__len)+(HH_DATA_MOD-1))&~(HH_DATA_MOD - 1))
	unsigned long	hh_data[HH_DATA_ALIGN(LL_MAX_HEADER) / sizeof(long)];
};

/* Reserve HH_DATA_MOD byte aligned hard_header_len, but at least that much.
 * Alternative is:
 *   dev->hard_header_len ? (dev->hard_header_len +
 *                           (HH_DATA_MOD - 1)) & ~(HH_DATA_MOD - 1) : 0
 *
 * We could use other alignment values, but we must maintain the
 * relationship HH alignment <= LL alignment.
 */
#define LL_RESERVED_SPACE(dev) \
	(((dev)->hard_header_len&~(HH_DATA_MOD - 1)) + HH_DATA_MOD)
#define LL_RESERVED_SPACE_EXTRA(dev,extra) \
	((((dev)->hard_header_len+extra)&~(HH_DATA_MOD - 1)) + HH_DATA_MOD)

/* These flag bits are private to the generic network queueing
 * layer, they may not be explicitly referenced by any other
 * code.
 */

enum netdev_state_t
{
	__LINK_STATE_XOFF=0,
	__LINK_STATE_START,
	__LINK_STATE_PRESENT,
	__LINK_STATE_NOCARRIER,
#if !defined(__VMKLNX__)
	__LINK_STATE_RX_SCHED,
#endif /* !defined(__VMKLNX__) */
	__LINK_STATE_LINKWATCH_PENDING,
	__LINK_STATE_DORMANT,
	__LINK_STATE_QDISC_RUNNING,
#if defined(__VMKLNX__)
        __LINK_STATE_BLOCKED,
        __LINK_STATE_TOGGLED,
#endif /* defined(__VMKLNX__) */
};

#if defined(__VMKLNX__)
typedef enum netqueue_state
{
        __NETQUEUE_STATE = 0,
} netqueue_state_t;

/*
 * napi_wdt_priv is not used anymore and we keep it for binary
 * compatibility reasons.
 */
struct napi_wdt_priv {
        struct net_device      *dev;  /* device receiving packets */
        struct napi_struct     *napi; /* napi context being polled */
};

/* 
 * Poll type being used. vmklinux creates poll for per queue and a backup 
 * poll for the device. The type specifies the kind of poll being used.
 */ 

typedef enum {
   NETPOLL_DEFAULT = 0,
   NETPOLL_BACKUP = 1,
} vmklnx_poll_type;

/*
 * since all pointers are 4 bytes or even 8 bytes aligned,
 * let's simply embed the poll_type in the lower bits of vmk_NetPoll->priv
 * final pointer value = (original priv pointer | poll_type)
 */
#define POLLPRIV_TYPE_BITS	1
#define POLLPRIV_TYPE_MASK	((1 << POLLPRIV_TYPE_BITS) - 1)

static inline void *pollpriv_embed(void *priv, vmklnx_poll_type poll_type)
{
        VMK_ASSERT(priv);
        VMK_ASSERT((((unsigned long) priv) & POLLPRIV_TYPE_MASK) == 0);
        VMK_ASSERT(poll_type <= POLLPRIV_TYPE_MASK);
        return (void *)(((unsigned long )priv) | poll_type);
}

static inline vmklnx_poll_type pollpriv_type(void *priv)
{
        VMK_ASSERT(priv);
        return (vmklnx_poll_type)(((unsigned long)priv) & POLLPRIV_TYPE_MASK);
}

static inline struct napi_struct *pollpriv_napi(void *priv)
{
        VMK_ASSERT(pollpriv_type(priv) == NETPOLL_DEFAULT);
        return (struct napi_struct *) (((unsigned long)priv) & (~POLLPRIV_TYPE_MASK));
}

static inline struct net_device *pollpriv_net_device(void *priv)
{
        VMK_ASSERT(pollpriv_type(priv) == NETPOLL_BACKUP);
        return (struct net_device *)  (((unsigned long)priv) & (~POLLPRIV_TYPE_MASK));
}

struct napi_struct {
        unsigned long           state;
        int                     weight;
        int                     (*poll)(struct napi_struct *, int);
        unsigned int            napi_id;
        struct net_device       *dev;
        struct list_head        dev_list;
        vmk_Bool                dev_poll;
        void *                  net_poll;
        vmklnx_poll_type        net_poll_type;
        struct napi_wdt_priv    napi_wdt_priv;  /* not used */
        /*
         * With that we have a 4-byte hole after member "vector" in
         * napi_struct_9_2_1_x, we can change the 32-bit "vector" to 64-bit
         * intr_cookie without increasing the size of napi_struct and the offset
         * of next member "lro_mgr". We have assertion in linux_net.c to
         * verify this.
         */
        vmk_IntrCookie          intr_cookie;
        struct net_lro_mgr      lro_mgr;
        struct net_lro_desc     lro_desc[LRO_DEFAULT_MAX_DESC];
};

/* This struct is the definition for API at version 9_2_1_x and older. */
struct napi_struct_9_2_1_x {
        unsigned long           state;
        int                     weight;
        int                     (*poll)(struct napi_struct *, int);
        unsigned int            napi_id;
        struct net_device       *dev;
        struct list_head        dev_list;
        vmk_Bool                dev_poll;
        void *                  net_poll;
        vmklnx_poll_type        net_poll_type;
        struct napi_wdt_priv    napi_wdt_priv;  /* not used */
        vmk_uint32              vector;
        /* we have a 4-byte hole here */
        struct net_lro_mgr      lro_mgr;
        struct net_lro_desc     lro_desc[LRO_DEFAULT_MAX_DESC];
};

enum
{
        NAPI_STATE_SCHED,       /* Poll is scheduled */
        NAPI_STATE_DISABLE,     /* Disable pending */
        NAPI_STATE_UNUSED       /* Another state to distinguish
                                 * an inited but never enabled
                                 * napi context from a disabled context 
                                 */
};
#else /* !defined(__VMKLNX__) */
struct napi_struct {
        /* The poll_list must only be managed by the entity which
         * changes the state of the NAPI_STATE_SCHED bit.  This means
         * whoever atomically sets that bit can add this napi_struct
         * to the per-cpu poll_list, and whoever clears that bit
         * can remove from the list right before clearing the bit.
         */
        struct list_head        poll_list;

        unsigned long           state;
        int                     weight;
        int                     (*poll)(struct napi_struct *, int);
#ifdef CONFIG_NETPOLL
        spinlock_t              poll_lock;
        int                     poll_owner;
        struct net_device       *dev;
        struct list_head        dev_list;
#endif
};
#endif /* defined(__VMKLNX__) */

#if defined(__VMKLNX__)
extern void FASTCALL(__napi_schedule(struct napi_struct *n));

static inline int napi_disable_pending(struct napi_struct *n)
{
        return test_bit(NAPI_STATE_DISABLE, &n->state);
}

/**
 *      napi_schedule_prep - check if napi can be scheduled
 *      @n: napi context
 *
 * Test if NAPI routine is already running, and if not mark
 * it as running.  This is used as a condition variable
 * insure only one NAPI poll instance runs.  We also make
 * sure there is no pending NAPI disable.
 *
 * RETURN VALUE:
 *  0 if NAPI routine is already running
 *  Non zero value otherwise
 */
/* _VMKLNX_CODECHECK_: napi_schedule_prep */
static inline int napi_schedule_prep(struct napi_struct *n)
{
        return !napi_disable_pending(n) &&
                !test_and_set_bit(NAPI_STATE_SCHED, &n->state);
}

/**
 *      napi_schedule - schedule NAPI poll
 *      @n: napi context
 *
 * Schedule NAPI poll routine to be called if it is not already
 * running.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: napi_schedule */
static inline void napi_schedule(struct napi_struct *n)
{
        if (napi_schedule_prep(n))
                __napi_schedule(n);
}

/* Try to reschedule poll. Called by dev->poll() after napi_complete().  */
static inline int napi_reschedule(struct napi_struct *napi)
{
        if (napi_schedule_prep(napi)) {
                __napi_schedule(napi);
                return 1;
        }
        return 0;
}

static inline void __napi_complete(struct napi_struct *n)
{
        BUG_ON(!test_bit(NAPI_STATE_SCHED, &n->state));

	/* 
	 * 2008: poll_list is not needed in vmklinx.
	 */
        /* list_del(&n->poll_list); */

        smp_mb__before_clear_bit();
        clear_bit(NAPI_STATE_SCHED, &n->state);
}

/**
 *      napi_complete - NAPI processing complete
 *      @n: napi context
 *
 * Mark NAPI processing as complete.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: napi_complete */
static inline void napi_complete(struct napi_struct *n)
{
	/* 
	 * 2008: irq shield is only intended to protect napi->poll_list.
	 * Since poll_list is not supported in vmklinx, thus no need for the irq shield.
	 */
        /* local_irq_disable(); */

        __napi_complete(n);

        /* local_irq_enable(); */
}

extern void napi_disable(struct napi_struct *n);

extern void napi_enable(struct napi_struct *n);

#ifdef CONFIG_SMP
/**
 *      napi_synchronize - wait until NAPI is not running
 *      @n: napi context
 *
 * Wait until NAPI is done being scheduled on this context.
 * Waits till any outstanding processing completes but
 * does not disable future activations.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: napi_synchronize */
static inline void napi_synchronize(const struct napi_struct *n)
{
        while (test_bit(NAPI_STATE_SCHED, &n->state))
                msleep(1);
}
#else
# define napi_synchronize(n)    barrier()
#endif

extern void netif_napi_add(struct net_device *dev,
                           struct napi_struct *napi,
                           int (*poll)(struct napi_struct *, int),
                           int weight);

extern void netif_napi_del(struct napi_struct *napi);
#endif /* defined(__VMKLNX__) */

/*
 * This structure holds at boot time configured netdevice settings. They
 * are then used in the device probing. 
 */
struct netdev_boot_setup {
	char name[IFNAMSIZ];
	struct ifmap map;
};
#define NETDEV_BOOT_SETUP_MAX 8

extern int __init netdev_boot_setup(char *str);

/*
 * hardware queue states
 */
enum netdev_queue_state_t
{
	__QUEUE_STATE_XOFF,
        __QUEUE_STATE_SCHED,
};

#if defined(__VMKLNX__)
struct netdev_soft_queue {
        spinlock_t		queue_lock;        /* queue lock */
        unsigned int            state;             /* queue state */
        unsigned int            hardState;         /* hard queue state */
        vmk_PktList             outputList;        /* output packet list */
        uint32_t                outputListMaxSize; /* max allowed queue length */
};

struct tx_netqueue_info {
   vmk_Bool            valid;
   vmk_uint64          vmkqid;
};
#endif /* defined(__VMKLNX__) */

struct netdev_queue {
	struct net_device	 *dev;
	unsigned long		 state;

#if defined(__VMKLNX__)
        /*
         * software transmit queue. packets get queue here first and
         * then drained out of this queue to the hardware
         */
        struct netdev_soft_queue softq ____cacheline_aligned_in_smp;
#endif /* defined(__VMKLNX__) */

        /* xmit lock, taken by thread entring the driver */
	spinlock_t		 _xmit_lock ____cacheline_aligned_in_smp;
	int			 xmit_lock_owner;
        unsigned char            processing_tx;

        struct netdev_queue	 *next_sched;
} ____cacheline_aligned_in_smp;


/*
 *	The DEVICE structure.
 *	Actually, this whole structure is a big mistake.  It mixes I/O
 *	data with strictly "high-level" data, and it has to know about
 *	almost every data structure used in the INET module.
 *
 *	FIXME: cleanup struct net_device such that network protocol info
 *	moves out.
 */

struct net_device
{

	/*
	 * This is the first field of the "visible" part of this structure
	 * (i.e. as seen by users in the "Space.c" file).  It is the name
	 * the interface.
	 */
#if defined(__VMKLNX__)
	char			name[VMK_DEVICE_NAME_MAX_LENGTH];
#else /* !defined(__VMKLNX__) */
	char			name[IFNAMSIZ];
#endif /* defined(__VMKLNX__) */
	/* device name hash chain */
	struct hlist_node	name_hlist;

	/*
	 *	I/O specific fields
	 *	FIXME: Merge these and struct ifmap into one
	 */
	unsigned long		mem_end;	/* shared mem end	*/
	unsigned long		mem_start;	/* shared mem start	*/
	unsigned long		base_addr;	/* device I/O address	*/
	unsigned int		irq;		/* device IRQ number	*/

	/*
	 *	Some hardware also needs these fields, but they are not
	 *	part of the usual set specified in Space.c.
	 */

	unsigned char		if_port;	/* Selectable AUI, TP,..*/
	unsigned char		dma;		/* DMA channel		*/

	unsigned long		state;

	struct net_device	*next;
	
	/* The device initialization function. Called only once. */
	int			(*init)(struct net_device *dev);

	/* ------- Fields preinitialized in Space.c finish here ------- */

	/* Net device features */
	unsigned long		features;
/*
 * Note:
 * Please update macro NETIF_ALL_FLAGS_AND_DESC in linux_net.c when adding
 * new NETIF_F_* flag.
 */
#if defined (__VMKLNX__)
#define GENEVE_F_OUTER_UDP_CSO   0x0001   /* support geneve outer UDP checksum offload */
#define GENEVE_F_OAM_RX_QUEUE    0x0002   /* support dedicated RX queue for OAM frames */

#define NETIF_F_GENEVE_OFFLOAD   0x80000000000 /* Geneve offload capable */
#define NETIF_F_ENCAP       0x20000000000 /* ENCAP offload capable */
#define NETIF_F_PSEUDO_REG 0x10000000000 /* PF uplink registered as pseudo. */
#define NETIF_F_UPT             0x100000000   /* Uniform passthru */
#define NETIF_F_HIDDEN_UPLINK   32768   /* Uplink hidden from VC. */
#define NETIF_F_SW_LRO          16384   /* Software LRO engine. */
#define NETIF_F_FRAG_CANT_SPAN_PAGES 8192 /* each frag cannot span multiple pages. */
#endif /* defined(__VMKLNX__) */
#define NETIF_F_SG		1	/* Scatter/gather IO. */
#define NETIF_F_IP_CSUM		2	/* Can checksum only TCP/UDP over IPv4. */
#define NETIF_F_NO_CSUM		4	/* Does not require checksum. F.e. loopack. */
#define NETIF_F_HW_CSUM		8	/* Can checksum all the packets. */
#if defined(__VMKLNX__)
#define NETIF_F_IPV6_CSUM       16      /* Can checksum TCP/UDP over IPV6 */
#endif /* defined(__VMKLNX__) */
#define NETIF_F_HIGHDMA		32	/* Can DMA to high memory. */
#define NETIF_F_FRAGLIST	64	/* Scatter/gather IO. */
#define NETIF_F_HW_VLAN_TX	128	/* Transmit VLAN hw acceleration */
#define NETIF_F_HW_VLAN_RX	256	/* Receive VLAN hw acceleration */
#define NETIF_F_HW_VLAN_FILTER	512	/* Receive filtering on VLAN */
#define NETIF_F_VLAN_CHALLENGED	1024	/* Device cannot handle VLAN packets */
#define NETIF_F_GSO		2048	/* Enable software GSO. */
#define NETIF_F_LLTX		4096	/* LockLess TX */

#define NETIF_F_FCOE_MTU        (1 << 26) /* Supports max FCoE MTU, 2158 bytes*/

	/* Segmentation offload features */
#define NETIF_F_GSO_SHIFT	16
#if defined(__VMKLNX__)
#define NETIF_F_GSO_MASK	0x3fffffff
#else /* !defined(__VMKLNX__) */
#define NETIF_F_GSO_MASK	0xffff0000
#endif /* defined(__VMKLNX__) */
#define NETIF_F_TSO		(SKB_GSO_TCPV4 << NETIF_F_GSO_SHIFT)
#define NETIF_F_UFO		(SKB_GSO_UDP << NETIF_F_GSO_SHIFT)
#define NETIF_F_GSO_ROBUST	(SKB_GSO_DODGY << NETIF_F_GSO_SHIFT)
#define NETIF_F_TSO_ECN		(SKB_GSO_TCP_ECN << NETIF_F_GSO_SHIFT)
#define NETIF_F_TSO6		(SKB_GSO_TCPV6 << NETIF_F_GSO_SHIFT)
#define NETIF_F_FSO		(SKB_GSO_FCOE << NETIF_F_GSO_SHIFT)

       /* device can perform tso/csum with offsets */
#define NETIF_F_OFFLOAD_8OFFSET  ((1 << 6) << NETIF_F_GSO_SHIFT)
#define NETIF_F_OFFLOAD_16OFFSET ((1 << 7) << NETIF_F_GSO_SHIFT)

#define NETIF_F_RDONLYINETHDRS   ((1 << 8) << NETIF_F_GSO_SHIFT)

	/* List of features with software fallbacks. */
#define NETIF_F_GSO_SOFTWARE	(NETIF_F_TSO | NETIF_F_TSO_ECN | NETIF_F_TSO6)

#define NETIF_F_GEN_CSUM	(NETIF_F_NO_CSUM | NETIF_F_HW_CSUM)
#define NETIF_F_ALL_CSUM	(NETIF_F_IP_CSUM | NETIF_F_GEN_CSUM)

#if defined(__VMKLNX__)
	/* FCoE specific flags */
#define NETIF_F_CNA_VN2VN       (0x20000000)   /* starts fcoe in VN2VN mode */
#define NETIF_F_CNA             (0x80000000)
#define NETIF_F_FCOE_CRC        (0x40000000)
#define NETIF_F_HWDCB           (0x2000000000)

        /*  DMA constraint flags */
#define NETIF_F_DMA39           (0x200000000)   /* implements 39 bit DMA mask */
#define NETIF_F_DMA40           (0x400000000)   /* implements 40 bit DMA mask */
#define NETIF_F_DMA48           (0x800000000)   /* implements 48 bit DMA mask */

#define NETIF_F_NO_SCHED        (0x1000000000)  /* not compliant with network scheduling */

#define NETIF_F_DEFQ_L2_FLTR    (0x4000000000)  /* 
                                                 * mac filters for traffic steering should
                                                 * also be pushed on the default queue
                                                 */
   /* LRO types support flags */
#define NETIF_F_IPV4_LRO        (0x8000000000)  /* hardware support LRO on IPv4 */
#define NETIF_F_IPV6_LRO        (0x40000000000) /* hardware support LRO on IPv6 */

#endif /* defined(__VMKLNX__) */

	/* Interface index. Unique device identifier	*/
	int			ifindex;
	int			iflink;


	struct net_device_stats* (*get_stats)(struct net_device *dev);
#if defined(__VMKLNX__)
	struct net_device_stats stats;
#endif /* defined(__VMKLNX__) */
	struct iw_statistics*	(*get_wireless_stats)(struct net_device *dev);

	/* List of functions to handle Wireless Extensions (instead of ioctl).
	 * See <net/iw_handler.h> for details. Jean II */
	const struct iw_handler_def *	wireless_handlers;
	/* Instance data managed by the core of Wireless Extensions. */
	struct iw_public_data *	wireless_data;

	/* pending config used by cfg80211/wext compat code only */
	void *cfg80211_wext_pending_config;

	struct ethtool_ops *ethtool_ops;

	/*
	 * This marks the end of the "visible" part of the structure. All
	 * fields hereafter are internal to the system, and may change at
	 * will (read: may be cleaned up at will).
	 */


	unsigned int		flags;	/* interface flags (a la BSD)	*/
	unsigned short		gflags;
#if defined (__VMKLNX__)
#define IFF_DEV_IS_OPEN   0x1
#define IFF_DEV_PKT_TRACE 0x2
#endif /* defined(__VMKLNX__) */
        unsigned short          priv_flags; /* Like 'flags' but invisible to userspace. */
	unsigned short		padded;	/* How much padding added by alloc_netdev() */

	unsigned char		operstate; /* RFC2863 operstate */
	unsigned char		link_mode; /* mapping policy to operstate */

	unsigned		mtu;	/* interface MTU value		*/
	unsigned short		type;	/* interface hardware type	*/
	unsigned short		hard_header_len;	/* hardware hdr length	*/

	struct net_device	*master; /* Pointer to master device of a group,
					  * which this device is member of.
					  */

	/* Interface address info. */
	unsigned char		perm_addr[MAX_ADDR_LEN]; /* permanent hw address */
	unsigned char		addr_len;	/* hardware address length	*/
	unsigned short          dev_id;		/* for shared network cards */

	struct dev_mc_list	*mc_list;	/* Multicast mac addresses	*/
	int			mc_count;	/* Number of installed mcasts	*/
	int			promiscuity;
	int			allmulti;


	/* Protocol specific pointers */
	
	void 			*atalk_ptr;	/* AppleTalk link 	*/
	void			*ip_ptr;	/* IPv4 specific data	*/  
#if defined(__VMKLNX__)
        void                    *fcoe_ptr;      /* FCoE specific data   */
	/* Data Center Bridging netlink ops */
	struct dcbnl_rtnl_ops *dcbnl_ops; 
	/* Function pointers for FCoE offloads */
        int                     (*ndo_fcoe_ddp_setup)(struct net_device *dev,
                                                      u16 xid,
                                                      struct scatterlist *sgl,
                                                      unsigned int sgc);
        int                     (*ndo_fcoe_ddp_done)(struct net_device *dev,
                                                     u16 xid);
#else /* !defined(__VMKLNX__) */
	void                    *dn_ptr;        /* DECnet specific data */
	void                    *ip6_ptr;       /* IPv6 specific data */
	void			*ec_ptr;	/* Econet specific data	*/
	void			*ax25_ptr;	/* AX.25 specific data */
#endif /* defined(__VMKLNX__) */
	void			*ieee80211_ptr;	/* IEEE 802.11 specific data */

#if defined(__VMKLNX__)   

/*
 * Cache line mostly used on receive path (including eth_type_trans())
 */
        void                   *genCount;
	unsigned long		last_rx;	   /* Time of last Rx	*/
        atomic_t                rxInFlight;        /* keeps track of the rx packet in flight. */

        unsigned long           linnet_rx_packets; /* vmklinux rx packets */
        unsigned long           linnet_tx_packets; /* vmklinux tx packets */
        unsigned long           linnet_rx_dropped; /* vmklinux rx dropped */
        unsigned long           linnet_tx_dropped; /* vmklinux tx dropped */
        int                     linnet_pkt_completed; /* vmklinux pkt completed */
#endif /* defined(__VMKLNX__) */

	/* Interface address info used in eth_type_trans() */
	unsigned char		dev_addr[MAX_ADDR_LEN];	/* hw address, (before bcast 
							because most packets are unicast) */

	unsigned char		broadcast[MAX_ADDR_LEN];	/* hw bcast add	*/

/*
 * Cache line mostly used on queue transmit path (qdisc)
 */
	struct netdev_queue	*_tx ____cacheline_aligned_in_smp;

	/* Number of TX queues allocated at alloc_netdev_mq() time  */
	unsigned int		num_tx_queues;

	/* Number of TX queues currently active in device  */
	unsigned int		real_num_tx_queues;
   
	unsigned long		tx_queue_len;	/* Max frames per queue allowed */

	void			*priv;	/* pointer to private data	*/
	int			(*hard_start_xmit) (struct sk_buff *skb,
						    struct net_device *dev);
	/* These may be needed for future network-power-down code. */
	unsigned long		trans_start;	/* Time (in jiffies) of last Tx	*/

	int			watchdog_timeo; /* used by dev_watchdog() */
	struct timer_list	watchdog_timer;

/*
 * refcnt is a very hot point, so align it on SMP
 */
	/* Number of references to this device */
	atomic_t		refcnt ____cacheline_aligned_in_smp;

	/* register/unregister state machine */
	enum { NETREG_UNINITIALIZED=0,
	       NETREG_REGISTERED,	/* completed register_netdevice */
	       NETREG_UNREGISTERING,	/* called unregister_netdevice */
	       NETREG_UNREGISTERED,	/* completed unregister todo */
	       NETREG_RELEASED,		/* called free_netdev */
#if defined(__VMKLNX__)
               NETREG_EARLY_NAPI_ADD_FAILED, /* early napi_add failed */
#endif /* defined(__VMKLNX__) */
	} reg_state;

	/* Called after device is detached from network. */
	void			(*uninit)(struct net_device *dev);
	/* Called after last user reference disappears. */
	void			(*destructor)(struct net_device *dev);

	/* Pointers to interface service routines.	*/
	int			(*open)(struct net_device *dev);
	int			(*stop)(struct net_device *dev);
#define HAVE_NETDEV_POLL
	int			(*hard_header) (struct sk_buff *skb,
						struct net_device *dev,
						unsigned short type,
						void *daddr,
						void *saddr,
						unsigned len);
	int			(*rebuild_header)(struct sk_buff *skb);
#define HAVE_MULTICAST			 
	void			(*set_multicast_list)(struct net_device *dev);
#define HAVE_SET_MAC_ADDR  		 
	int			(*set_mac_address)(struct net_device *dev,
						   void *addr);
#define HAVE_PRIVATE_IOCTL
	int			(*do_ioctl)(struct net_device *dev,
					    struct ifreq *ifr, int cmd);
#define HAVE_SET_CONFIG
	int			(*set_config)(struct net_device *dev,
					      struct ifmap *map);
#define HAVE_HEADER_CACHE
	int			(*hard_header_cache)(struct neighbour *neigh,
						     struct hh_cache *hh);
	void			(*header_cache_update)(struct hh_cache *hh,
						       struct net_device *dev,
						       unsigned char *  haddr);
#define HAVE_CHANGE_MTU
	int			(*change_mtu)(struct net_device *dev, int new_mtu);

#define HAVE_TX_TIMEOUT
	void			(*tx_timeout) (struct net_device *dev);

	void			(*vlan_rx_register)(struct net_device *dev,
						    struct vlan_group *grp);
	void			(*vlan_rx_add_vid)(struct net_device *dev,
						   unsigned short vid);
	void			(*vlan_rx_kill_vid)(struct net_device *dev,
						    unsigned short vid);

	int			(*hard_header_parse)(struct sk_buff *skb,
						     unsigned char *haddr);
	int			(*neigh_setup)(struct net_device *dev, struct neigh_parms *);

#ifdef CONFIG_NETPOLL
	struct netpoll_info	*npinfo;
#endif
#ifdef CONFIG_NET_POLL_CONTROLLER
	void                    (*poll_controller)(struct net_device *dev);
#endif
	/* bridge stuff */
	struct net_bridge_port	*br_port;

#ifdef CONFIG_NET_DIVERT
	/* this will get initialized at each interface type init routine */
	struct divert_blk	*divert;
#endif /* CONFIG_NET_DIVERT */

	/* class/net/name entry */
	struct class_device	class_dev;
	/* space for optional statistics and wireless sysfs groups */
	struct attribute_group  *sysfs_groups[3];

#if defined(__VMKLNX__)
        void                    *uplinkDev;
        struct pci_dev          *pdev;
        struct sk_buff		*privateSKBList;
        unsigned int		privateSKBCount;
        struct tx_netqueue_info *tx_netqueue_info;

        /*
	 * enable_intr is a pointer to a function which will enable
	 * (if "enable" is 1) or disable (if "enable" is 0) interrupts 
	 * on the card.  The function must return 0 on success, and any
	 * other value on failure.  If the driver does not implement
	 * this behavior, enable_intr may be NULL.  This hook is used by
	 * the interrupt clustering code in vmkernel/main/net.c
	 */
        int                     (*enable_intr)(struct net_device *dev, int enable);
        int                     link_speed;
        vmklnx_uplink_link_state link_state;
        int                     full_duplex;      
        vmk_ModuleID            module_id;      
        int                     nicMajor;
        unsigned short          watchdog_timeohit_cnt;   /* Number of timeout hit */
        /* Number of timeout needed before entering in panic mod */
        unsigned short          watchdog_timeohit_cfg;
        /* Global number of timeout hit */
        unsigned short          watchdog_timeohit_stats;
        vmklnx_uplink_watchdog_panic_mod_state  watchdog_timeohit_panic;   /* Panic mod */
#define NETDEV_TICKS_PER_HOUR  HZ * 3600
	unsigned long           watchdog_timeohit_period_start;
	vmknetddi_queueops_f	netqueue_ops;
        kmem_cache_t           *skb_pool;
#if defined(VMKLNX_ALLOW_DEPRECATED)
        int                     useDriverNamingDevice;
#else
        /* Retains binary compatibility with the removed useDriverNamingDevice field */
        char                    __filler[sizeof(int)];
#endif
        spinlock_t              napi_lock;
        struct list_head        napi_list;
        void                   *net_poll;
        void                   *default_net_poll;
        struct napi_wdt_priv    napi_wdt_priv;           /* not used */
        struct vlan_group      *vlan_group;

        /* This is being used by pnics which register as pseudo-devices. These 
         * drivers save the dev->pdev in this field, prior to setting it to 
         * null. These device drivers also set the DEVICE_PSEUDO_REG bit in the 
         * dev->features field if they use the earlier "unused" field in this 
         * manner.
         */
        void                   *pdev_pseudo;
        void                    *pt_ops;
        void                   *cna_ops;
        unsigned long           netq_state;   /* rx netq state */
#endif /* defined(__VMKLNX__) */

        /* for setting kernel sock attribute on TCP connection setup */
#define GSO_MAX_SIZE            65536
        unsigned int            gso_max_size;

        /* max exchange id for FCoE LRO by ddp */
        unsigned int            fcoe_ddp_xid;

};

/*
 * Netqueue APIs
 */
#if defined(__VMKLNX__)
#define VMKNETDDI_REGISTER_QUEUEOPS(ndev, ops)  \
        do {                                    \
                ndev->netqueue_ops = ops;       \
        } while (0)

static inline void vmknetddi_queueops_invalidate_state(struct net_device *dev)
{
        clear_bit(__NETQUEUE_STATE, (void*)&dev->netq_state);
}

static inline vmk_Bool vmknetddi_queueops_getset_state(struct net_device *dev, vmk_Bool newState)
{
        if (newState)
           return (test_and_set_bit(__NETQUEUE_STATE, (void*)&dev->netq_state) != 0);
        else
           return (test_and_clear_bit(__NETQUEUE_STATE, (void*)&dev->netq_state) != 0);
}

/*
 * PT API registration macro
 */
#define VMK_REGISTER_PT_OPS(ndev, ops)     \
        do {                               \
           ndev->pt_ops = ops; 		   \
        } while (0)

#endif /* defined(__VMKLNX__) */

#define	NETDEV_ALIGN		32
#define	NETDEV_ALIGN_CONST	(NETDEV_ALIGN - 1)

/**
 *  netdev_get_tx_queue - get tx queue descriptor
 *  @dev: network device
 *  @index: tx queue index
 *
 *  Get netdev_queue descriptor of tx queue specified by @index
 *
 *  RETURN VALUE:
 *  Pointer to netdev_queue structure of tx queue specified by @index
 */
/* _VMKLNX_CODECHECK_: netdev_get_tx_queue */
static inline
struct netdev_queue *netdev_get_tx_queue(const struct net_device *dev,
					 unsigned int index)
{
	return &dev->_tx[index];
}

static inline void netdev_for_each_tx_queue(struct net_device *dev,
					    void (*f)(struct net_device *,
						      struct netdev_queue *,
						      void *),
					    void *arg)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++)
		f(dev, &dev->_tx[i], arg);
}

/**                                          
 *  netdev_priv - access network device private data
 *  @dev: network device
 *                                           
 *  Get network device private data
 *
 * RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: netdev_priv */
static inline void *netdev_priv(struct net_device *dev)
{
      return (char *)dev + ((sizeof(struct net_device)
                                      + NETDEV_ALIGN_CONST)
                              & ~NETDEV_ALIGN_CONST);
}


/**
 *  SET_MODULE_OWNER - non-operational macro
 *  @dev: Ignored
 *
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  SYNOPSIS:
 *    #define SET_MODULE_OWNER(dev)
 *
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Does not return any value
 *
 */
/* _VMKLNX_CODECHECK_: SET_MODULE_OWNER */

#define SET_MODULE_OWNER(dev) do { } while (0)
/* Set the sysfs physical device reference for the network logical device
 * if set prior to registration will cause a symlink during initialization.
 */
#if defined(__VMKLNX__)
#define struct_from_member(_memptr, _strct, _member)                         \
   (_strct *)((uint8_t *)(_memptr) - (uint8_t *)&((_strct *) 0)->_member)


/**
 *  SET_NETDEV_DEV - Associates a PCI device to its network device
 *  @net: network device
 *  @ppdev: pointer to the PCI device
 *
 *  Sets the device pointer of the network device datastructure to the device
 *  pointer of the PCI device.
 *
 *  SYNOPSIS:
 *     #define SET_NETDEV_DEV(net,ppdev)
 *
 *  ESX Deviation Notes:
 *  The underlying implementation is different but the functionality is same.
 *
 *  RETURN VALUE:
 *   None
 *
 */
/* _VMKLNX_CODECHECK_: SET_NETDEV_DEV */


#define SET_NETDEV_DEV(net, ppdev)                                           \
      do {                                                                   \
         struct pci_dev *pcidev =                                            \
            struct_from_member(ppdev, struct pci_dev, dev);                  \
         pcidev->netdev = net;                                               \
         net->pdev = pcidev;                                                 \
      } while (0)
#else  /* !defined(__VMKLNX__) */
#define SET_NETDEV_DEV(net, pdev)	((net)->class_dev.dev = (pdev))
#endif /* defined(__VMKLNX__)  */

struct packet_type {
	__be16			type;	/* This is really htons(ether_type). */
	struct net_device	*dev;	/* NULL is wildcarded here	     */
	int			(*func) (struct sk_buff *,
					 struct net_device *,
					 struct packet_type *,
					 struct net_device *);
	struct sk_buff		*(*gso_segment)(struct sk_buff *skb,
						int features);
	int			(*gso_send_check)(struct sk_buff *skb);
	void			*af_packet_priv;
	struct list_head	list;
};

#include <linux/interrupt.h>
#include <linux/notifier.h>

extern struct net_device		loopback_dev;		/* The loopback */
extern struct net_device		*dev_base;		/* All devices */
extern rwlock_t				dev_base_lock;		/* Device list lock */

extern int 			netdev_boot_setup_check(struct net_device *dev);
extern unsigned long		netdev_boot_base(const char *prefix, int unit);
extern struct net_device    *dev_getbyhwaddr(unsigned short type, char *hwaddr);
extern struct net_device *dev_getfirstbyhwtype(unsigned short type);
extern void		dev_add_pack(struct packet_type *pt);
extern void		dev_remove_pack(struct packet_type *pt);
extern void		__dev_remove_pack(struct packet_type *pt);

extern struct net_device	*dev_get_by_flags(unsigned short flags,
						  unsigned short mask);
extern struct net_device	*dev_get_by_name(const char *name);
extern struct net_device	*__dev_get_by_name(const char *name);
extern int		dev_alloc_name(struct net_device *dev, const char *name);
extern int		dev_open(struct net_device *dev);
extern int		dev_close(struct net_device *dev);
extern int		dev_queue_xmit(struct sk_buff *skb);
extern int		register_netdevice(struct net_device *dev);
extern int		unregister_netdevice(struct net_device *dev);
extern void		vmklnx_netif_stop_tx_queue(struct netdev_queue *queue);
extern void		vmklnx_netif_start_tx_queue(struct netdev_queue *queue);
extern void		vmklnx_netif_set_poll_cna(struct napi_struct *napi);

#if !defined(__VMKLNX__)
extern void             free_netdev(struct net_device *dev);
#else /* defined(__VMKLNX__) */
extern void             vmklnx_free_netdev(struct kmem_cache_s *pmCache,
                                           struct net_device *dev);
void vmklnx_store_pt_ops(struct pci_dev *pf, struct pci_dev *vf,
                         int vf_idx, void *data);
/**
 * free_netdev - free network device
 *  @dev: device
 *
 *  Does the last stage of destroying an allocated device
 *  interface. The reference to the device object is released.
 *  If this is the last reference then it will be freed.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: free_netdev */
static void inline free_netdev(struct net_device *dev)
{
        vmklnx_free_netdev(THIS_MODULE->skb_cache, dev);
}
#endif /* !defined(__VMKLNX__) */

extern void		synchronize_net(void);
extern int 		register_netdevice_notifier(struct notifier_block *nb);
extern int		unregister_netdevice_notifier(struct notifier_block *nb);
extern int		call_netdevice_notifiers(unsigned long val, void *v);
extern struct net_device	*dev_get_by_index(int ifindex);
extern struct net_device	*__dev_get_by_index(int ifindex);
extern int		dev_restart(struct net_device *dev);
#ifdef CONFIG_NETPOLL_TRAP
extern int		netpoll_trap(void);
#endif

typedef int gifconf_func_t(struct net_device * dev, char __user * bufptr, int len);
extern int		register_gifconf(unsigned int family, gifconf_func_t * gifconf);
static inline int unregister_gifconf(unsigned int family)
{
	return register_gifconf(family, NULL);
}

struct softnet_data
{
        struct netdev_queue	*output_queue;
	struct sk_buff_head	input_pkt_queue;
	struct sk_buff		*completion_queue;
};

#if defined(__VMKLNX__)
extern struct softnet_data softnet_data[];
#else /* !defined(__VMKLNX__) */
DECLARE_PER_CPU(struct softnet_data,softnet_data);
#endif /* defined(__VMKLNX__) */

#define HAVE_NETIF_QUEUE

#if defined(__VMKLNX__)
extern void __netif_schedule(struct netdev_queue *dev);
#else /* !defined(__VMKLNX__) */
extern void __netif_schedule(struct net_device *dev);
#endif /* defined(__VMKLNX__) */

static inline void netif_schedule_queue(struct netdev_queue *txq)
{
	if (!test_bit(__QUEUE_STATE_XOFF, &txq->state))
                __netif_schedule(txq);
}

static inline void netif_tx_schedule_all(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++)
		netif_schedule_queue(netdev_get_tx_queue(dev, i));
}

/**
 *  netif_start_queue - allow transmit
 *  @txq: transmit queue
 *
 *  Allow upper layers to call the device hard_start_xmit routine on
 *  tx queue specified by @txq.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_start_queue */
static inline void netif_tx_start_queue(struct netdev_queue *txq)
{
	clear_bit(__QUEUE_STATE_XOFF, &txq->state);
#if defined(__VMKLNX__)
        vmklnx_netif_start_tx_queue(txq);
#endif /* defined(__VMKLNX__) */
}

/**
 *  netif_start_queue - allow transmit
 *  @dev: network device
 *
 *  Allow upper layers to call the device hard_start_xmit routine.
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_start_queue */
static inline void netif_start_queue(struct net_device *dev)
{
	netif_tx_start_queue(netdev_get_tx_queue(dev, 0));
}

/**
 *  netif_tx_start_all_queues - allow transmit on all tx queues
 *  @dev: network device
 *
 *  Allow upper layers to call the device hard_start_xmit routine.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_tx_start_all_queues */
static inline void netif_tx_start_all_queues(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		netif_tx_start_queue(txq);
	}
}

/**
 *  netif_set_poll_cna - change net poll callback to cna poll
 *  @napi: napi structure
 *
 *  Change net poll callback to cna poll
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_set_poll_cna */
static inline void netif_set_poll_cna(struct napi_struct *napi)
{
#if defined(__VMKLNX__)
   vmklnx_netif_set_poll_cna(napi);
#endif /* defined(__VMKLNX__) */
}


/**
 *  netif_tx_wake_queue - restart transmit
 *  @txq: transmit queue
 *
 *  Allow upper layers to call the device hard_start_xmit routine.
 *  Used for flow control when transmit resources are available.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_tx_wake_queue */
static inline void netif_tx_wake_queue(struct netdev_queue *txq)
{
   if (test_and_clear_bit(__QUEUE_STATE_XOFF, &txq->state)) {
                __netif_schedule(txq);
#if defined(__VMKLNX__)
                vmklnx_netif_start_tx_queue(txq);
#endif /* defined(__VMKLNX__) */
   }
}

/**
 *  netif_wake_queue - restart transmit
 *  @dev: network device
 *
 *  Allow upper layers to call the device hard_start_xmit routine.
 *  Used for flow control when transmit resources are available.
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_wake_queue */
static inline void netif_wake_queue(struct net_device *dev)
{
	netif_tx_wake_queue(netdev_get_tx_queue(dev, 0));
}

/**
 *  netif_tx_wake_all_queues - restart transmit on all tx queues
 *  @dev: network device
 *
 *  Allow upper layers to call the device hard_start_xmit routine.
 *  Used for flow control when transmit resources are available.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_tx_wake_all_queues */
static inline void netif_tx_wake_all_queues(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		netif_tx_wake_queue(txq);
	}
}

/**
 *  netif_tx_stop_queue - stop specified transmit queue
 *  @txq: transmit queue
 *
 *  Stop upper layers calling the device hard_start_xmit routine on
 *  transmit queue @txq. Used for flow control when transmit resources
 *  are unavailable.
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_tx_stop_queue */
static inline void netif_tx_stop_queue(struct netdev_queue *txq)
{
	set_bit(__QUEUE_STATE_XOFF, &txq->state);
#if defined(__VMKLNX__)
        vmklnx_netif_stop_tx_queue(txq);
#endif /* defined(__VMKLNX__) */
}

/**
 *  netif_stop_queue - stop transmitted packets
 *  @dev: network device
 *  
 *  Stop upper layers calling the device hard_start_xmit routine.
 *  Used for flow control when transmit resources are unavailable.
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_stop_queue */
static inline void netif_stop_queue(struct net_device *dev)
{
	netif_tx_stop_queue(netdev_get_tx_queue(dev, 0));
}

/**
 * netif_tx_stop_all_queues - suspend transmit on all queues
 * @dev: network device
 * 
 * Suspend transmit on all queues by stopping upper layers
 * from calling the device hard_start_xmit_routine().
 * Used as flow control when transmit resources are not available.
 * 
 * RETURN VALUE:
 * This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_tx_stop_all_queues */
static inline void netif_tx_stop_all_queues(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		netif_tx_stop_queue(txq);
	}
}

/**
 *  netif_tx_queue_stopped - test if transmit queue is flowblocked
 *  @txq: transmit queue
 *
 *  Test if specified transmit queue on device is currently unable to send.
 *
 *  RETURN VALUE:
 *  Non zero value if queue is stopped
 *  0 if queue is not stopped
 */
/* _VMKLNX_CODECHECK_: netif_tx_queue_stopped */
static inline int netif_tx_queue_stopped(const struct netdev_queue *txq)
{
	return test_bit(__QUEUE_STATE_XOFF, &txq->state);
}

/**
 * netif_queue_stopped - test if transmit queue is flowblocked
 *  @dev: network device
 *
 * Test if transmit queue on device is currently unable to send.
 *
 * RETURN VALUE:
 *  Non zero value if queue is stopped
 *  0 if queue is not stopped
 */
/* _VMKLNX_CODECHECK_: netif_queue_stopped */
static inline int netif_queue_stopped(const struct net_device *dev)
{
	return netif_tx_queue_stopped(netdev_get_tx_queue(dev, 0));
}

/*
 * Routines to manage the subqueues on a device.  We only need start
 * stop, and a check if it's stopped.  All other device management is
 * done at the overall netdevice level.
 * Also test the device if we're multiqueue.
 */

/**
 * netif_start_subqueue - allow sending packets on subqueue
 *  @dev: network device
 *  @queue_index: sub queue index
 *
 * Start individual transmit queue of a device with multiple transmit queues.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_start_subqueue */
static inline void netif_start_subqueue(struct net_device *dev, u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);
	clear_bit(__QUEUE_STATE_XOFF, &txq->state);
#if defined(__VMKLNX__)
        vmklnx_netif_start_tx_queue(txq);
#endif /* defined(__VMKLNX__) */
}

/**
 * netif_stop_subqueue - stop sending packets on subqueue
 *  @dev: network device
 *  @queue_index: sub queue index
 *
 * Stop individual transmit queue of a device with multiple transmit queues.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_stop_subqueue */
static inline void netif_stop_subqueue(struct net_device *dev, u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);
	set_bit(__QUEUE_STATE_XOFF, &txq->state);
#if defined(__VMKLNX__)
        vmklnx_netif_stop_tx_queue(txq);
#endif /* defined(__VMKLNX__) */
}

static inline int __netif_subqueue_stopped(const struct net_device *dev,
                                           u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);
	return test_bit(__QUEUE_STATE_XOFF, &txq->state);
}

/**
 * netif_subqueue_stopped - test status of subqueue
 *  @dev: network device
 *  @queue_index: sub queue index
 *  @skb: sk buffer
 *
 * Check individual transmit queue of a device with multiple transmit queues.
 *
 * RETURN VALUE:
 *  Non zero value if subqueue is stopped
 *  0 if subqueue is not stopped
 */
/* _VMKLNX_CODECHECK_: netif_subqueue_stopped */
static inline int netif_subqueue_stopped(const struct net_device *dev,
					 struct sk_buff *skb)
{
	return __netif_subqueue_stopped(dev, skb_get_queue_mapping(skb));
}

/**
 * netif_wake_subqueue - allow sending packets on subqueue
 *  @dev: network device
 *  @queue_index: sub queue index
 *
 * Resume individual transmit queue of a device with multiple transmit queues.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_wake_subqueue */
static inline void netif_wake_subqueue(struct net_device *dev, u16 queue_index)
{
	struct netdev_queue *txq = netdev_get_tx_queue(dev, queue_index);
	if (test_and_clear_bit(__QUEUE_STATE_XOFF, &txq->state)) {
                __netif_schedule(txq);
#if defined(__VMKLNX__)
                vmklnx_netif_start_tx_queue(txq);
#endif /* defined(__VMKLNX__) */
        }
}

/**
 * netif_is_multiqueue - test if device has multiple transmit queues
 *  @dev: network device
 *
 * Check if device has multiple transmit queues
 *
 * RETURN VALUE:
 *  Non zero value if device is multiqueue
 *  0 if the device is not a multiqueue device
 */
/* _VMKLNX_CODECHECK_: netif_is_multiqueue */
static inline int netif_is_multiqueue(const struct net_device *dev)
{
	return (dev->num_tx_queues > 1);
}


/**                                          
 *  netif_running - Test if up
 *  @dev: network device
 *                                           
 *  Test if the device has been brought up.
 *
 * RETURN VALUE:
 *  Non zero value if device is up
 *  0 if the device is not up
 */
/* _VMKLNX_CODECHECK_: netif_running */
static inline int netif_running(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_START, &dev->state);
}


/* Use this variant when it is known for sure that it
 * is executing from interrupt context.
 */
#if !defined(__VMKLNX__)
static inline void dev_kfree_skb_irq(struct sk_buff *skb)
{
	if (atomic_dec_and_test(&skb->users)) {
		struct softnet_data *sd;
		unsigned long flags;
		
		local_irq_save(flags);
		sd = &__get_cpu_var(softnet_data);
		skb->next = sd->completion_queue;
		sd->completion_queue = skb;
		raise_softirq_irqoff(NET_TX_SOFTIRQ);
		local_irq_restore(flags);
	}
}

/* Use this variant in places where it could be invoked
 * either from interrupt or non-interrupt context.
 */
extern void dev_kfree_skb_any(struct sk_buff *skb);
#endif /* !defined(__VMKLNX__) */

#define HAVE_NETIF_RX 1
extern int		netif_rx(struct sk_buff *skb);
extern int		netif_rx_ni(struct sk_buff *skb);
#define HAVE_NETIF_RECEIVE_SKB 1
extern int		netif_receive_skb(struct sk_buff *skb);

extern int		dev_valid_name(const char *name);
extern int		dev_ioctl(unsigned int cmd, void __user *);
#if defined(__VMKLNX__)
extern int		vmklnx_net_dev_ethtool(struct ifreq *, struct net_device *);
#endif /* defined(__VMKLNX__) */
extern int		dev_ethtool(struct ifreq *);
extern unsigned		dev_get_flags(const struct net_device *);
extern int		dev_change_flags(struct net_device *, unsigned);
extern int		dev_change_name(struct net_device *, char *);
extern int		dev_set_mtu(struct net_device *, int);
extern int		dev_set_mac_address(struct net_device *,
					    struct sockaddr *);
extern int		dev_hard_start_xmit(struct sk_buff *skb,
					    struct net_device *dev);

extern void		dev_init(void);

extern int		netdev_budget;

/* Called by rtnetlink.c:rtnl_unlock() */
extern void netdev_run_todo(void);

/**
 * dev_put - release reference to device
 *  @dev: network device
 *
 * Release reference to device and allow it to be freed.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_put */
static inline void dev_put(struct net_device *dev)
{
	atomic_dec(&dev->refcnt);
}

/**
 * dev_hold - get reference to device
 *  @dev: network device
 *
 * Hold reference to device to keep it from being freed.
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: dev_hold */
static inline void dev_hold(struct net_device *dev)
{
	atomic_inc(&dev->refcnt);
}

/* Carrier loss detection, dial on demand. The functions netif_carrier_on
 * and _off may be called from IRQ context, but it is caller
 * who is responsible for serialization of these calls.
 *
 * The name carrier is inappropriate, these functions should really be
 * called netif_lowerlayer_*() because they represent the state of any
 * kind of lower layer not just hardware media.
 */

extern void linkwatch_fire_event(struct net_device *dev);

/**                                          
 *  netif_carrier_ok - test if carrier present
 *  @dev: network device
 *                                           
 *  Check if carrier is present on device
 *
 * RETURN VALUE:
 *  Non zero value if carrier is present
 *  0 if carrier is not present
 */                                          
/* _VMKLNX_CODECHECK_: netif_carrier_ok */
static inline int netif_carrier_ok(const struct net_device *dev)
{
	return !test_bit(__LINK_STATE_NOCARRIER, &dev->state);
}

#if !defined(__VMKLNX__)
extern void __netdev_watchdog_up(struct net_device *dev);

extern void netif_carrier_on(struct net_device *dev);

extern void netif_carrier_off(struct net_device *dev);
#else /* defined(__VMKLNX__) */
static inline void __netdev_watchdog_up(struct net_device *dev)
{
    /* Do nothing */
}

/**
 *  netif_toggled_set - Set toggled flag
 *  @dev: network device
 *
 *  Set the toggled flag
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_toggled_set */
static inline void netif_toggled_set(struct net_device *dev)
{
   set_bit(__LINK_STATE_TOGGLED, &dev->state);
}

/**
 *  netif_toggled_clear - Clear toggled flag
 *  @dev: network device
 *
 *  Clear the toggled flag
 *
 * RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: netif_toggled_clear */
static inline void netif_toggled_clear(struct net_device *dev)
{
   clear_bit(__LINK_STATE_TOGGLED, &dev->state);
}

/**
 *  netif_toggled_test_and_clear - Check & clear toggled flag
 *  @dev: network device
 *
 *  Clear the toggled flag, return the original value
 *
 * RETURN VALUE:
 *  Original state of the toggled bit
 */
/* _VMKLNX_CODECHECK_: netif_toggled_clear */
static inline int netif_toggled_test_and_clear(struct net_device *dev)
{
   return test_and_clear_bit(__LINK_STATE_TOGGLED, &dev->state);
}

/**
 *  netif_toggled_check - Check toggled flag
 *  @dev: network device
 *
 *  Check if device's toggled flag is set or not
 *
 * RETURN VALUE:
 *  1 if toggled flag is set, 0 otherwise
 */
/* _VMKLNX_CODECHECK_: netif_toggled_check */
static inline int netif_toggled_check(struct net_device *dev)
{
   return test_bit(__LINK_STATE_TOGGLED, &dev->state);
}

/**                                          
 *  netif_carrier_on - Set carrier
 *  @dev: network device
 *                                           
 *  Device has detected that carrier
 *
 * RETURN VALUE:
 *  None                                           
 */                                          
/* _VMKLNX_CODECHECK_: netif_carrier_on */
static inline void netif_carrier_on(struct net_device *dev)
{
   if (test_and_clear_bit(__LINK_STATE_NOCARRIER, &dev->state))
      netif_toggled_set(dev);
   if (netif_running(dev))
      __netdev_watchdog_up(dev);
}

/**                                          
 *  netif_carrier_off - clear carrier
 *  @dev: network device
 *                                           
 *  Device has detected loss of carrier.
 *
 * RETURN VALUE:
 *  None     
 */                                          
/* _VMKLNX_CODECHECK_: netif_carrier_off */
static inline void netif_carrier_off(struct net_device *dev)
{
   if (!test_and_set_bit(__LINK_STATE_NOCARRIER, &dev->state));
}
#endif /* !defined(__VMKLNX__) */

static inline void netif_dormant_on(struct net_device *dev)
{
	if (!test_and_set_bit(__LINK_STATE_DORMANT, &dev->state))
		linkwatch_fire_event(dev);
}

static inline void netif_dormant_off(struct net_device *dev)
{
	if (test_and_clear_bit(__LINK_STATE_DORMANT, &dev->state))
		linkwatch_fire_event(dev);
}

static inline int netif_dormant(const struct net_device *dev)
{
	return test_bit(__LINK_STATE_DORMANT, &dev->state);
}


static inline int netif_oper_up(const struct net_device *dev) {
	return (dev->operstate == IF_OPER_UP ||
		dev->operstate == IF_OPER_UNKNOWN /* backward compat */);
}

/* Hot-plugging. */
/**                                          
 *  netif_device_present - Is driver available or removed
 *  @dev: network device
 *                                           
 *  Check if device has not been removed from system
 *
 * RETURN VALUE:
 *  Non zero value if device has not been removed from system.
 *  0 if the device has removed.
 */                                          
/* _VMKLNX_CODECHECK_: netif_device_present */
static inline int netif_device_present(struct net_device *dev)
{
	return test_bit(__LINK_STATE_PRESENT, &dev->state);
}

extern void netif_device_detach(struct net_device *dev);

extern void netif_device_attach(struct net_device *dev);

/*
 * Network interface message level settings
 */
#define HAVE_NETIF_MSG 1

enum {
	NETIF_MSG_DRV		= 0x0001,
	NETIF_MSG_PROBE		= 0x0002,
	NETIF_MSG_LINK		= 0x0004,
	NETIF_MSG_TIMER		= 0x0008,
	NETIF_MSG_IFDOWN	= 0x0010,
	NETIF_MSG_IFUP		= 0x0020,
	NETIF_MSG_RX_ERR	= 0x0040,
	NETIF_MSG_TX_ERR	= 0x0080,
	NETIF_MSG_TX_QUEUED	= 0x0100,
	NETIF_MSG_INTR		= 0x0200,
	NETIF_MSG_TX_DONE	= 0x0400,
	NETIF_MSG_RX_STATUS	= 0x0800,
	NETIF_MSG_PKTDATA	= 0x1000,
	NETIF_MSG_HW		= 0x2000,
	NETIF_MSG_WOL		= 0x4000,
};

#define netif_msg_drv(p)	((p)->msg_enable & NETIF_MSG_DRV)
#define netif_msg_probe(p)	((p)->msg_enable & NETIF_MSG_PROBE)
#define netif_msg_link(p)	((p)->msg_enable & NETIF_MSG_LINK)
#define netif_msg_timer(p)	((p)->msg_enable & NETIF_MSG_TIMER)
#define netif_msg_ifdown(p)	((p)->msg_enable & NETIF_MSG_IFDOWN)
#define netif_msg_ifup(p)	((p)->msg_enable & NETIF_MSG_IFUP)
#define netif_msg_rx_err(p)	((p)->msg_enable & NETIF_MSG_RX_ERR)
#define netif_msg_tx_err(p)	((p)->msg_enable & NETIF_MSG_TX_ERR)
#define netif_msg_tx_queued(p)	((p)->msg_enable & NETIF_MSG_TX_QUEUED)
#define netif_msg_intr(p)	((p)->msg_enable & NETIF_MSG_INTR)
#define netif_msg_tx_done(p)	((p)->msg_enable & NETIF_MSG_TX_DONE)
#define netif_msg_rx_status(p)	((p)->msg_enable & NETIF_MSG_RX_STATUS)
#define netif_msg_pktdata(p)	((p)->msg_enable & NETIF_MSG_PKTDATA)
#define netif_msg_hw(p)		((p)->msg_enable & NETIF_MSG_HW)
#define netif_msg_wol(p)	((p)->msg_enable & NETIF_MSG_WOL)

static inline u32 netif_msg_init(int debug_value, int default_msg_enable_bits)
{
	/* use default */
	if (debug_value < 0 || debug_value >= (sizeof(u32) * 8))
		return default_msg_enable_bits;
	if (debug_value == 0)	/* no output */
		return 0;
	/* set low N bits */
	return (1 << debug_value) - 1;
}

#if defined(__VMKLNX__)
/**
 *  netif_rx_schedule_prep - test if receive needs to be scheduled but only if up
 *  @dev: network device
 *  @napi: napi structure
 *  
 *  Test if receive needs to be scheduled but only if up
 *
 *  RETURN VALUE:
 *  1 if receive is up and needs to be scheduled, 0 otherwise
 */
/* _VMKLNX_CODECHECK_: netif_rx_schedule_prep */
static inline int netif_rx_schedule_prep(struct net_device *dev,
                                         struct napi_struct *napi)
{
        return napi_schedule_prep(napi);
}

/* Add interface to tail of rx poll list. This assumes that _prep has
 * already been called and returned 1.
 */
/**                                          
 *  __netif_rx_schedule - schedule napi context for rx polling
 *  @dev: network device
 *  @napi: napi structure
 *                                           
 *  Schedule napi context for rx polling
 *
 * RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: __netif_rx_schedule */
static inline void __netif_rx_schedule(struct net_device *dev,
                                       struct napi_struct *napi)
{
        __napi_schedule(napi);
}


/**                                          
 *  netif_rx_schedule - schedule napi context for rx polling
 *  @dev: network device
 *  @napi: napi structure
 *                                           
 *  Test if receive needs to be scheduled and schedule napi 
 *  context for rx polling. Called by irq handler.
 *                                           
 *  RETURN VALUE:
 *   None
 */                                          
/* _VMKLNX_CODECHECK_: netif_rx_schedule */
static inline void netif_rx_schedule(struct net_device *dev,
                                     struct napi_struct *napi)
{
        if (netif_rx_schedule_prep(dev, napi))
                __netif_rx_schedule(dev, napi);
}

/* Try to reschedule poll. Called by dev->poll() after netif_rx_complete().  */
static inline int netif_rx_reschedule(struct net_device *dev,
                                      struct napi_struct *napi)
{
        if (napi_schedule_prep(napi)) {
                __netif_rx_schedule(dev, napi);
                return 1;
        }
        return 0;
}

/**                                          
 *  __netif_rx_complete - Remove interface from poll list
 *  @dev: network device
 *  @napi: napi structure
 *                                           
 * Same as netif_rx_complete, except that local_irq_save(flags)
 * has already been issued
 *
 * RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: __netif_rx_complete */
static inline void __netif_rx_complete(struct net_device *dev,
                                       struct napi_struct *napi)
{
        __napi_complete(napi);
}

/* Remove interface from poll list: it must be in the poll list
 * on current cpu. This primitive is called by dev->poll(), when
 * it completes the work. The device cannot be out of poll list at this
 * moment, it is BUG().
 */
/**                                          
 *  netif_rx_complete - Remove interface from poll list
 *  @dev: network device
 *  @napi: napi structure
 *                                           
 * This primitive is called by dev->poll(), when it completes the work. 
 *
 * RETURN VALUE:
 *  None
 */                                          
/* _VMKLNX_CODECHECK_: netif_rx_complete */
static inline void netif_rx_complete(struct net_device *dev,
                                     struct napi_struct *napi)
{
	/* 
	 * 2008: irq shield is only intended to protect napi->poll_list.
	 * Since poll_list is not supported in vmklinx, thus no need for the irq shield.
	 */
        /* local_irq_save(flags); */

        __netif_rx_complete(dev, napi);

        /* local_irq_restore(flags); */
}
#else /* !defined(__VMKLNX__) */
/*
 * vmklinux doesn't support old napi anymore so let's drop this code.
 */
/* Test if receive needs to be scheduled */
static inline int __netif_rx_schedule_prep(struct net_device *dev)
{
	return !test_and_set_bit(__LINK_STATE_RX_SCHED, &dev->state);
}

/**
 *  netif_rx_schedule_prep - test if receive needs to be scheduled but only if up
 *  @dev: ignored
 *  @napi: napi context
 *  
 *  Test if receive needs to be scheduled but only if up
 *
 *  RETURN VALUE:
 *  1 if receive is up and needs to be scheduled, 0 otherwise
 */
/* _VMKLNX_CODECHECK_: netif_rx_schedule_prep */
static inline int netif_rx_schedule_prep(struct net_device *dev)
{
	return netif_running(dev) && __netif_rx_schedule_prep(dev);
}

/* Add interface to tail of rx poll list. This assumes that _prep has
 * already been called and returned 1.
 */

extern void __netif_rx_schedule(struct net_device *dev);

/* Try to reschedule poll. Called by irq handler. */

static inline void netif_rx_schedule(struct net_device *dev)
{
	if (netif_rx_schedule_prep(dev))
		__netif_rx_schedule(dev);
}

/* Try to reschedule poll. Called by dev->poll() after netif_rx_complete().
 */
extern int netif_rx_reschedule(struct net_device *dev, int undo);

/* Remove interface from poll list: it must be in the poll list
 * on current cpu. This primitive is called by dev->poll(), when
 * it completes the work. The device cannot be out of poll list at this
 * moment, it is BUG().
 */
static inline void netif_rx_complete(struct net_device *dev)
{
	unsigned long flags;
	local_irq_save(flags);
	BUG_ON(!test_bit(__LINK_STATE_RX_SCHED, &dev->state));
        list_del_init(&dev->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
	local_irq_restore(flags);
}

extern void netif_poll_disable(struct net_device *dev);

static inline void netif_poll_enable(struct net_device *dev)
{
	clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
}

/* same as netif_rx_complete, except that local_irq_save(flags)
 * has already been issued
 */
static inline void __netif_rx_complete(struct net_device *dev)
{
	BUG_ON(!test_bit(__LINK_STATE_RX_SCHED, &dev->state));
	if (!list_empty(&dev->poll_list)) 
           list_del_init(&dev->poll_list);
	smp_mb__before_clear_bit();
	clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
}
#endif /* defined(__VMKLNX__) */

/**
 *  __netif_tx_lock - grab network device transmit lock on transmit queue
 *  @txq: transmit queue
 *  @cpu: cpu number of lock owner
 *
 *  Get network device transmit lock on transmit queue specified by @txq
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: __netif_tx_lock */
static inline void __netif_tx_lock(struct netdev_queue *txq, int cpu)
{
	spin_lock(&txq->_xmit_lock);
	txq->xmit_lock_owner = cpu;
}

static inline void __netif_tx_lock_bh(struct netdev_queue *txq)
{
	spin_lock_bh(&txq->_xmit_lock);
	txq->xmit_lock_owner = smp_processor_id();
}

/**
 *  netif_tx_lock - grab network device transmit lock
 *  @dev: network device
 *  @cpu: cpu number of lock owner
 *
 *  Get network device transmit lock
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_tx_lock */
static inline void netif_tx_lock(struct net_device *dev)
{
	int cpu = smp_processor_id();
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		__netif_tx_lock(txq, cpu);
	}
}

/**
 *  netif_tx_lock_bh - acquire network device transmit lock
 *  @dev: network device
 *
 *  Acquire netowrk device transmit lock
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_tx_lock_bh */
static inline void netif_tx_lock_bh(struct net_device *dev)
{
	netif_tx_lock(dev);
}

static inline int __netif_tx_trylock(struct netdev_queue *txq)
{
	int ok = spin_trylock(&txq->_xmit_lock);
	if (likely(ok))
		txq->xmit_lock_owner = smp_processor_id();
	return ok;
}

/**
 *  __netif_tx_unlock - release network device transmit lock on transmit queue
 *  @txq: transmit queue
 *
 *  Release network device transmit lock on transmit queue specified by @txq
 *
 *  RETURN VALUE:
 *  None
 */
/* _VMKLNX_CODECHECK_: __netif_tx_unlock */
static inline void __netif_tx_unlock(struct netdev_queue *txq)
{
	txq->xmit_lock_owner = -1;
	spin_unlock(&txq->_xmit_lock);
}

static inline void __netif_tx_unlock_bh(struct netdev_queue *txq)
{
	txq->xmit_lock_owner = -1;
	spin_unlock_bh(&txq->_xmit_lock);
}

/**
 *  netif_tx_unlock - release network device transmit lock
 *  @dev: network device
 *
 *  Release network device transmit lock
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_tx_unlock */
static inline void netif_tx_unlock(struct net_device *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		__netif_tx_unlock(txq);
	}

}

/**
 *  netif_tx_unlock_bh - release network device transmit lock
 *  @dev: network device
 *
 *  Release netowrk device transmit lock
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_tx_unlock_bh */
static inline void netif_tx_unlock_bh(struct net_device *dev)
{
	netif_tx_unlock(dev);
}

/**
 *  netif_tx_disable - disable transmit
 *  @dev: network device
 *
 *  Disable transmit
 *
 *  RETURN VALUE:
 *  This function does not return a value 
 */
/* _VMKLNX_CODECHECK_: netif_tx_disable */
static inline void netif_tx_disable(struct net_device *dev)
{
	unsigned int i;

	netif_tx_lock_bh(dev);
	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);
		netif_tx_stop_queue(txq);
	}
	netif_tx_unlock_bh(dev);
}


/* These functions live elsewhere (drivers/net/net_init.c, but related) */

extern void		ether_setup(struct net_device *dev);
#if defined(__VMKLNX__)
/**
 * alloc_netdev_mq - allocate network device
 * @sizeof_priv:        size of private data to allocate space for
 * @name:               device name format string
 * @setup:              callback to initialize device
 * @queue_count:        the number of subqueues to allocate
 *
 * Allocates a struct net_device with private data area for driver use
 * and performs basic initialization.  Also allocates subquue structs
 * for each queue on the device at the end of the netdevice.
 *
 * RETURN VALUE:
 *  Pointer to allocated struct net_device on success
 *  %NULL on error
 */
/* _VMKLNX_CODECHECK_: alloc_netdev_mq */
static inline struct net_device *
alloc_netdev_mq(int sizeof_priv, const char *name,
	        void (*setup)(struct net_device *),
	        unsigned int queue_count)
{
   return vmklnx_alloc_netdev_mq(THIS_MODULE,
                                 sizeof_priv,
				 name,
				 setup,
				 queue_count);
}
#else /* !defined(__VMKLNX__) */
extern struct net_device *alloc_netdev_mq(int sizeof_priv, const char *name,
				       void (*setup)(struct net_device *),
				       unsigned int queue_count);
#endif /* defined(__VMKLNX__) */

/**
 *  alloc_netdev - allocate network device   
 *  @sizeof_priv: size of private data to allocate space for
 *  @name: device name format string
 *  @setup: callback to initialize device
 *
 *  Allocates a struct net_device with private data area for driver use and 
 *  performs basic initialization.
 *
 *  SYNOPSIS:
 *  #define alloc_netdev(sizeof_priv, name, setup)
 *
 *  RETURN VALUE:
 *  The pointer to the newly allocated net_device struct if successful,  otherwise NULL.
 *
 */
/* _VMKLNX_CODECHECK_: alloc_netdev */
#define alloc_netdev(sizeof_priv, name, setup) \
	alloc_netdev_mq(sizeof_priv, name, setup, 1)

extern int		register_netdev(struct net_device *dev);
extern void		unregister_netdev(struct net_device *dev);
/* Functions used for multicast support */
extern void		dev_mc_upload(struct net_device *dev);
extern int 		dev_mc_delete(struct net_device *dev, void *addr, int alen, int all);
extern int		dev_mc_add(struct net_device *dev, void *addr, int alen, int newonly);
extern void		dev_mc_discard(struct net_device *dev);
extern void		dev_set_promiscuity(struct net_device *dev, int inc);
extern void		dev_set_allmulti(struct net_device *dev, int inc);
extern void		netdev_state_change(struct net_device *dev);
extern void		netdev_features_change(struct net_device *dev);
/* Load a device via the kmod */
extern void		dev_load(const char *name);
extern void		dev_mcast_init(void);
extern int		netdev_max_backlog;
extern int		weight_p;
extern int		netdev_set_master(struct net_device *dev, struct net_device *master);
extern int skb_checksum_help(struct sk_buff *skb, int inward);
#if defined(__VMKLNX__)
/**
 *  skb_gso_segment - non-operational function
 *  @skb: Ignored
 *  @features: Ignored
 *
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  ESX Deviation Notes:
 *  A non-operational function provided to help reduce
 *  kernel ifdefs. It is not supported in this release of ESX.
 *
 *  RETURN VALUE:
 *  Returns NULL always.
 *
 */
/* _VMKLNX_CODECHECK_: skb_gso_segment */
static inline struct sk_buff *skb_gso_segment(struct sk_buff *skb, int features)
{
        /* PR 260085 : Should we expose TSO segmentation functions of vmkernel to vmkapi ? */
        return NULL;
}
#else /* !defined(__VMKLNX__) */
extern struct sk_buff *skb_gso_segment(struct sk_buff *skb, int features);
#endif /* defined(__VMKLNX__) */
#ifdef CONFIG_BUG
extern void netdev_rx_csum_fault(struct net_device *dev);
#else
static inline void netdev_rx_csum_fault(struct net_device *dev)
{
}
#endif
/* rx skb timestamps */
extern void		net_enable_timestamp(void);
extern void		net_disable_timestamp(void);

#ifdef CONFIG_PROC_FS
extern void *dev_seq_start(struct seq_file *seq, loff_t *pos);
extern void *dev_seq_next(struct seq_file *seq, void *v, loff_t *pos);
extern void dev_seq_stop(struct seq_file *seq, void *v);
#endif

extern void linkwatch_run_queue(void);

static inline int net_gso_ok(int features, int gso_type)
{
	int feature = gso_type << NETIF_F_GSO_SHIFT;
	return (features & feature) == feature;
}

static inline int skb_gso_ok(struct sk_buff *skb, int features)
{
	return net_gso_ok(features, skb_shinfo(skb)->gso_type);
}

static inline int netif_needs_gso(struct net_device *dev, struct sk_buff *skb)
{
	return skb_is_gso(skb) &&
	       (!skb_gso_ok(skb, dev->features) ||
		unlikely(skb->ip_summed != CHECKSUM_HW));
}

static inline void netif_set_gso_max_size(struct net_device *dev,
					  unsigned int size)
{
	dev->gso_max_size = size;
}

#if !defined(__VMKLNX__)
/* On bonding slaves other than the currently active slave, suppress
 * duplicates except for 802.3ad ETH_P_SLOW and alb non-mcast/bcast.
 */
static inline int skb_bond_should_drop(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	struct net_device *master = dev->master;

	if (master &&
	    (dev->priv_flags & IFF_SLAVE_INACTIVE)) {
		if (master->priv_flags & IFF_MASTER_ALB) {
			if (skb->pkt_type != PACKET_BROADCAST &&
			    skb->pkt_type != PACKET_MULTICAST)
				return 0;
		}
		if (master->priv_flags & IFF_MASTER_8023AD &&
		    skb->protocol == __constant_htons(ETH_P_SLOW))
			return 0;

		return 1;
	}
	return 0;
}
#endif /* !defined(__VMKLNX__) */

#endif /* __KERNEL__ */

#endif	/* _LINUX_DEV_H */
