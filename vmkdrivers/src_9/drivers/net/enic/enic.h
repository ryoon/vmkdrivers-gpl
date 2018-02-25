/*
 * Copyright 2008-2010 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef _ENIC_H_
#define _ENIC_H_

#include <linux/types.h>

#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_stats.h"
#include "vnic_nic.h"
#include "vnic_rss.h"
#include "enic_upt.h"
#include "enic_res.h"

#define DRV_NAME		"enic"
#define DRV_DESCRIPTION		"Cisco VIC Ethernet NIC Driver"
#define DRV_VERSION		"2.1.2.71"
#define DRV_COPYRIGHT		"Copyright 2008-2011 Cisco Systems, Inc"

#define ENIC_BARS_MAX		6

#define ENIC_WQ_MAX		16	
#define ENIC_RQ_MAX		16
#define ENIC_CQ_MAX		(ENIC_WQ_MAX + ENIC_RQ_MAX)
#define ENIC_INTR_MAX		(ENIC_CQ_MAX + 2)

#define ENIC_AIC_LARGE_PKT_DIFF 3

struct enic_msix_entry {
	int requested;
	char devname[IFNAMSIZ];
	irqreturn_t (*isr)(int, void *);
	void *devid;
};


/* priv_flags */
#define ENIC_SRIOV_ENABLED		(1 << 0)
#define ENIC_RESET_INPROGRESS		(1 << 1)
#define ENIC_NETQ_ENABLED		(1 << 2)

/* enic port profile set flags */
#define ENIC_PORT_REQUEST_APPLIED	(1 << 0)
#define ENIC_SET_REQUEST		(1 << 1)
#define ENIC_SET_NAME			(1 << 2)
#define ENIC_SET_INSTANCE		(1 << 3)
#define ENIC_SET_HOST			(1 << 4)

#define ring_is_allocated(ring)\
		test_bit(__ENIC_Q_ALLOCATED, &(ring.state))
#define set_ring_allocated(ring)\
		set_bit(__ENIC_Q_ALLOCATED, &(ring.state))
#define clear_ring_allocated(ring)\
		clear_bit(__ENIC_Q_ALLOCATED, &(ring.state))

enum enic_q_state {
	__ENIC_Q_ALLOCATED
};

/* VXLAN APIs */
extern unsigned short vmklnx_netdev_get_vxlan_port(void);
typedef void (*vmklnx_netdev_vxlan_port_update_callback)(struct net_device *dev,
	unsigned short vxlan_udp_port_number);
extern void vmklnx_netdev_set_vxlan_port_update_callback(struct net_device *dev,
	vmklnx_netdev_vxlan_port_update_callback vxlan_port_update_callback);

struct enic_port_profile {
	u32 set;
	u8 request;
	char name[PORT_PROFILE_MAX];
	u8 instance_uuid[PORT_UUID_MAX];
	u8 host_uuid[PORT_UUID_MAX];
	u8 vf_mac[ETH_ALEN];
	u8 mac_addr[ETH_ALEN];
};

enum ownership_type {
	OWNER_NETDEV = 0,
	OWNER_PTS = 1,
	/* ... */
};


/*
 * enic_rfs_fltr_node - rfs filter node in hash table
 *	@keys: IPv4 5 tuple
 *	@flow_id: flow_id of clsf filter provided by kernel
 *	@fltr_id: filter id of clsf filter returned by adaptor
 *	@rq_id: desired rq index
 *	@node: hlist_node
 */
struct enic_rfs_fltr_node {
	struct flow_keys keys;
	u32 flow_id;
	u16 fltr_id;
	u16 rq_id;
	struct hlist_node node;
};

/*
 * enic_rfs_flw_tbl - rfs flow table
 *	@max: Maximum number of filters vNIC supports
 *	@free: Number of free filters available
 *	@toclean: hash table index to clean next
 *	@ht_head: hash table list head
 *	@lock: spin lock
 *	@rfs_may_expire: timer function for enic_rps_may_expire_flow
 */
struct enic_rfs_flw_tbl {
	u16 max;	/* Max clsf filters vNIC supports */
	int free;	/* number of free clsf filters */

#define ENIC_RFS_FLW_BITSHIFT	(10)
#define ENIC_RFS_FLW_MASK	((1 << ENIC_RFS_FLW_BITSHIFT) - 1)
	u16 toclean:ENIC_RFS_FLW_BITSHIFT;
	struct hlist_head ht_head[1 << ENIC_RFS_FLW_BITSHIFT];
	spinlock_t lock;
	struct timer_list rfs_may_expire;
};

/* Per-instance private data structure */
struct enic {
	struct net_device *netdev;
	struct pci_dev *pdev;
	struct vnic_enet_config config;
	struct vnic_dev_bar bar[ENIC_BARS_MAX];
	struct vnic_dev *vdev;
	struct net_device_stats net_stats;
	struct timer_list notify_timer;
	struct work_struct reset;
	struct work_struct change_mtu_work;
	struct msix_entry msix_entry[ENIC_INTR_MAX];
	struct enic_msix_entry msix[ENIC_INTR_MAX];
	u32 msg_enable;
	spinlock_t devcmd_lock;
	u8 mac_addr[ETH_ALEN];
	u8 mc_addr[ENIC_MULTICAST_PERFECT_FILTERS][ETH_ALEN];
	u8 uc_addr[ENIC_UNICAST_PERFECT_FILTERS][ETH_ALEN];
	unsigned int flags;
	unsigned int priv_flags;
	unsigned int mc_count;
	unsigned int uc_count;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 0, 00))
	int csum_rx_enabled;
#endif
	u32 port_mtu;
	u32 rx_coalesce_usecs;
	u32 tx_coalesce_usecs;
	struct enic_port_profile *pp;
	unsigned int netq_allocated_wqs;
	unsigned int netq_allocated_rqs;
	spinlock_t netq_wq_lock;
	spinlock_t netq_rq_lock;
	unsigned short vxlan_udp_port_number;
	enum ownership_type owner;
	atomic_t in_stop;
	int upt_mode;
	int upt_resources_alloced;
	int upt_active;
	struct timer_list upt_notify_timer;
	u8 __iomem *upt_rxprod2;
	void *upt_oob[ENIC_UPT_OOB_MAX];
	dma_addr_t upt_oob_pa[ENIC_UPT_OOB_MAX];
	vmk_NetVFTXQueueStats upt_stats_tx;
	vmk_NetVFRXQueueStats upt_stats_rx;
	VMK_ReturnStatus upt_tq_error_stress_status;
	/* Mapped to VMK_STRESS_OPT_NET_IF_FAIL_HARD_TX */
	vmk_StressOptionHandle upt_tq_error_stress_handle;
	VMK_ReturnStatus upt_tq_silent_error_stress_status;
	/* Mapped to VMK_STRESS_OPT_NET_IF_CORRUPT_TX */
	vmk_StressOptionHandle upt_tq_silent_error_stress_handle;
	VMK_ReturnStatus upt_rq_error_stress_status;
	/* Mapped to VMK_STRESS_OPT_NET_IF_FAIL_RX */
	vmk_StressOptionHandle upt_rq_error_stress_handle;
	unsigned int upt_tq_error_stress_counter;
	unsigned int upt_tq_silent_error_stress_counter;
	unsigned int upt_rq_error_stress_counter;

	/* work queue cache line section */
	____cacheline_aligned struct vnic_wq wq[ENIC_WQ_MAX];
	spinlock_t wq_lock[ENIC_WQ_MAX];
	unsigned int wq_count;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3, 1, 00))
	struct vlan_group *vlan_group;
#endif
	u16 loop_enable;
	u16 loop_tag;

	/* receive queue cache line section */
	____cacheline_aligned struct vnic_rq rq[ENIC_RQ_MAX];
	unsigned int rq_count;
	u64 rq_truncated_pkts;
	u64 rq_bad_fcs;
	struct napi_struct napi[ENIC_RQ_MAX + ENIC_WQ_MAX];

	/* interrupt resource cache line section */
	____cacheline_aligned struct vnic_intr intr[ENIC_INTR_MAX];
	unsigned int intr_count;
	u32 __iomem *legacy_pba;		/* memory-mapped */

	/* completion queue cache line section */
	____cacheline_aligned struct vnic_cq cq[ENIC_CQ_MAX];
	unsigned int cq_count;
	struct enic_rfs_flw_tbl rfs_h;	/* for accel rfs */
};

static inline struct device *enic_get_dev(struct enic *enic)
{
	return &(enic->pdev->dev);
}

static inline unsigned int enic_cq_rq(struct enic *enic, unsigned int rq)
{
	return rq;
}

static inline unsigned int enic_cq_wq(struct enic *enic, unsigned int wq)
{
	return enic->rq_count + wq;
}

static inline unsigned int enic_legacy_io_intr(void)
{
	return 0;
}

static inline unsigned int enic_legacy_err_intr(void)
{
	return 1;
}

static inline unsigned int enic_legacy_notify_intr(void)
{
	return 2;
}

static inline unsigned int enic_msix_rq_intr(struct enic *enic,
	unsigned int rq)
{
	return enic->cq[enic_cq_rq(enic, rq)].interrupt_offset;
}

static inline unsigned int enic_msix_wq_intr(struct enic *enic,
	unsigned int wq)
{
	return enic->cq[enic_cq_wq(enic, wq)].interrupt_offset;
}

static inline unsigned int enic_msix_err_intr(struct enic *enic)
{
	return enic->rq_count + enic->wq_count;
}

static inline unsigned int enic_msix_notify_intr(struct enic *enic)
{
	return enic->rq_count + enic->wq_count + 1;
}


void enic_reset_addr_lists(struct enic *enic);
int enic_is_dynamic(struct enic *enic);
int enic_sriov_enabled(struct enic *enic);
int enic_is_valid_vf(struct enic *enic, int vf);

#endif /* _ENIC_H_ */
