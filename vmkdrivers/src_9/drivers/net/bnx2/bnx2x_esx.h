/* bnx2x_esx.h: QLogic Everest network driver.
 *
 * Copyright 2008-2014 QLogic Corporation
 *
 * Unless you and QLogic execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other QLogic software provided under a
 * license other than the GPL, without QLogic's express prior written
 * consent.
 *
 * Written by: Benjamin Li
 * PASSTHRU code by: Shmulik Ravid
 *
 */
#ifndef BNX2X_ESX_H
#define BNX2X_ESX_H

#define STRINGIFY(foo) #foo
#define XSTRINGIFY(bar) STRINGIFY(bar)

#define BNX2X_ESX_PRINT_TIMESTAMP  0

#if (VMWARE_ESX_DDK_VERSION >= 55000)
#if !defined(BNX2X_INBOX)
#define BNX2X_ESX_SRIOV  /* XXX: To be enabled in inbox later */
#endif
#define BNX2X_ESX_DYNAMIC_NETQ
#endif

int bnx2x_esx_is_BCM57800_1G(struct bnx2x *bp);

/*
 * Workaround for PR347954 for inbox bnx2x driver.
 * Disable NetQueue when Flex10 module is connected for inbox bnx2x driver
 */
#if defined(BNX2X_INBOX)
#define BNX2X_MAX_QUEUES_E1HMF  1
#else
#define BNX2X_MAX_QUEUES_E1HMF  2
#endif

/*
 * limit the total number of queues to 8/4 (default + 7/3 NetQueues) in SF and
 * 2 (default + 1 NetQueue) in MF.
 */
#if (VMWARE_ESX_DDK_VERSION >= 41000)
#define BNX2X_MAX_QUEUES_E1H    8
#else
#define BNX2X_MAX_QUEUES_E1H    4
#endif
#define BNX2X_MAX_QUEUES_E1     4

/* If num_queue is provided then we're in engineering mode and the max is set
 * the through the HW max, otherwise limit the number of queues according to
 * the ESX definitions above
 */
#define BNX2X_MAX_ESX_QUEUES(bp) (IS_MF(bp) ? BNX2X_MAX_QUEUES_E1HMF : \
				 (CHIP_IS_E1(bp) ? BNX2X_MAX_QUEUES_E1 : \
				 (CHIP_IS_E3(bp) && \
				  bnx2x_esx_is_BCM57800_1G(bp) ? \
				 BNX2X_MAX_QUEUES_E1 : BNX2X_MAX_QUEUES_E1H)))


/* the max number of queues dictated by the HW/FW */
#define BNX2X_MAX_HW_QUEUES(bp)  (bp->igu_sb_cnt - CNIC_SUPPORT(bp))

/* If num_queue is provided then we're in engineering mode and the max is set
 * the through the HW max, otherwise limit the number of queues according to
 * the ESX definitions above
 */
#define BNX2X_MAX_QUEUES(bp) \
	(num_queues ? \
		BNX2X_MAX_HW_QUEUES(bp) : \
		min_t(int, BNX2X_MAX_ESX_QUEUES(bp), BNX2X_MAX_HW_QUEUES(bp)))

#define BNX2X_ESX_INIT_JUMBO_RX_RING_SIZE (max_t(u32, max_t(u32, (MAX_RX_AVAIL / 16), 512), MIN_RX_AVAIL)) /* 512 */
#define BNX2X_ESX_INIT_RX_RING_SIZE       (max_t(u32, (MAX_RX_AVAIL / 4), MIN_RX_AVAIL))

/*
 * in ESX queue 0 is the default queue and the other queues are for NetQueue.
 * We set the macro to 0 in order to avoid RSS configuration.

#define is_eth_multi(bp)	0
 */

struct netq_list {
	struct list_head	link;
	struct bnx2x_fastpath	*fp;
};

struct netq_op_stats {
	u64 unhandled;

	u64 get_queue_count;
	u64 get_filter_count;
	u64 alloc_queue;
	u64 free_queue;
	u64 get_queue_vector;
	u64 get_default_queue;
	u64 apply_rx_filter;
	u64 remove_rx_filter;
	u64 get_stats;
#if (VMWARE_ESX_DDK_VERSION >= 41000)
	u64 alloc_queue_with_attr;
	u64 get_supported_feat;
#if (VMWARE_ESX_DDK_VERSION >= 50000)
	u64 get_supported_filter_class;
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	u64 realloc_queue_with_attr;
	u64 get_filter_count_of_device;
#endif
#endif
#endif
};

#if (VMWARE_ESX_DDK_VERSION >= 55000)
struct rss_op_stats {
	u64 get_params;
	u64 init_state;
	u64 update_ind_table;
	u64 get_ind_table;
};
#endif

#define MAX_RSS_P_SIZE		4

void
bnx2x_esx_passthru_config_mtu_finalize(struct bnx2x *bp, u16 vf_idx,
				       int rc, int state, u16 mtu);
void
bnx2x_esx_passthru_config_setup_filters_finalize(struct bnx2x *bp, u16 vf_idx,
						 int rc, int state);

#ifdef BNX2X_ESX_SRIOV

#define BNX2X_ESX_SET_PASSTHRU_RC_STATE(bp, vf_idx, rc, state)	\
	(bp)->esx.vf[(vf_idx)].passthru_rc = (rc);		\
	(bp)->esx.vf[(vf_idx)].passthru_state = (state);

struct bnx2x_esx_vf {
	int			passthru_rc;
	int			passthru_state;
#define BNX2X_ESX_PASSTHRU_SET_MAC_MSG_FROM_VF	0x0001
#define BNX2X_ESX_PASSTHRU_SET_MAC_COMPLETE_OP	0x0002
#define BNX2X_ESX_PASSTHRU_SET_MTU_MSG_FROM_VF	0x0004
#define BNX2X_ESX_PASSTHRU_SET_MTU_COMPLETE_OP	0x0008
	wait_queue_head_t	passthru_wait_config;
	wait_queue_head_t	passthru_wait_comp;
#define BNX2X_PASSTHRU_WAIT_EVENT_TIMEOUT	msecs_to_jiffies(100)

	int			flags;
#define BNX2X_ESX_PASSTHRU_FORCE_MTU		0x0001

	u16			old_mtu;
#define BNX2X_ESX_PASSTHRU_MTU_UNINITIALIZED	0xFFFF

	int			forced_mtu;
	int			mtu_from_config;
	u8			mac_from_config[ETH_ALEN];
	u8			last_mac[ETH_ALEN];
};
#else
#define BNX2X_ESX_SET_PASSTHRU_RC_STATE(bp, vf_idx, rc, state)

struct bnx2x_esx_vf {
};

#endif

struct bnx2x_esx {
	u32			flags;
#define BNX2X_ESX_SRIOV_ENABLED		0x00000001
	u16			number_of_mac_filters;

	/* n queues allocated */
	u16			n_rx_queues_allocated;
	u16			n_tx_queues_allocated;

	/* used to synchronize netq accesses */
	spinlock_t		netq_lock;

	int rss_p_num;
	int rss_p_size;
	int rss_netq_idx_tbl[MAX_RSS_P_SIZE];
	u8  rss_raw_ind_tbl[T_ETH_INDIRECTION_TABLE_SIZE];
	u32 rss_key_tbl[T_ETH_RSS_KEY];

	int index;

	bool  poll_disable_fwdmp;

	/* available LRO and RSS queues */
	int	avail_lro_netq;
	int	avail_rss_netq;
	struct list_head	free_netq_list;
	struct netq_list	*free_netq_pool;

	struct netq_op_stats	vmnic_netq_op_stats;
#if (VMWARE_ESX_DDK_VERSION >= 55000)
	struct rss_op_stats	vmnic_rss_op_stats;
#endif
	struct netq_op_stats	cna_netq_op_stats;
	atomic_t rx_page_count;
	atomic_t vheap_count, kheap_count;

	u32			vf_fw_stats_size;
	void			*vf_fw_stats;
	dma_addr_t		vf_fw_stats_mapping;
	void			*old_vf_fw_stats; /* stored in CPU order */

	struct bnx2x_esx_vf	*vf;
};

struct bnx2x_esx_fastpath {
	u32			netq_flags;
	struct netq_mac_filter	*mac_filters;
	int			rss_p_lead_idx;
	struct netq_list	*netq_in_use;
	atomic_t rx_skb_count;
	bool	dynamic_netq_stop;
};

struct netq_mac_filter {
	u16     flags;
	u8      mac[ETH_ALEN];
};

extern int psod_on_panic;
void bnx2x_vmk_set_netdev_features(struct bnx2x *bp);

#if defined(BNX2X_VMWARE_BMAPILNX) /* ! BNX2X_UPSTREAM */

#define BNX2X_VMWARE_CIM_CMD_ENABLE_NIC		0x0001
#define BNX2X_VMWARE_CIM_CMD_DISABLE_NIC	0x0002
#define BNX2X_VMWARE_CIM_CMD_REG_READ		0x0003
#define BNX2X_VMWARE_CIM_CMD_REG_WRITE		0x0004
#define BNX2X_VMWARE_CIM_CMD_GET_NIC_PARAM	0x0005
#define BNX2X_VMWARE_CIM_CMD_GET_NIC_STATUS	0x0006
#define BNX2X_VMWARE_CIM_CMD_GET_PCI_CFG_READ	0x0007

struct bnx2x_ioctl_reg_read_req {
	u32 reg_offset;
	u32 reg_value;
} __attribute__((packed));

struct bnx2x_ioctl_reg_write_req {
	u32 reg_offset;
	u32 reg_value;
} __attribute__((packed));

struct bnx2x_ioctl_get_nic_param_req {
	u32 version;
	u32 mtu;
	u8  current_mac_addr[8];
} __attribute__((packed));

struct bnx2x_ioctl_get_nic_status_req {
	u32 nic_status; /*  1: Up, 0: Down */
} __attribute__((packed));

struct bnx2x_ioctl_req {
	u32 cmd;
	union {
		/* no struct for reset_nic command */
		struct bnx2x_ioctl_reg_read_req reg_read;
		struct bnx2x_ioctl_reg_write_req reg_write;
		struct bnx2x_ioctl_get_nic_param_req get_nic_param;
		struct bnx2x_ioctl_get_nic_status_req get_nic_status;
	} cmd_req;
} __attribute__((packed));

int bnx2x_ioctl_cim(struct net_device *dev, struct ifreq *ifr);
#endif

#if defined(BNX2X_ESX_CNA) /* ! BNX2X_UPSTREAM */
int bnx2x_cna_enable(struct bnx2x *bp, int tx_count, int rx_count);
void bnx2x_cna_disable(struct bnx2x *bp, bool remove_netdev);
#endif


/*
 * the total number of net queues is the number of regualr L2 queues -1 for
 * the default.
 * If RSS pools were reserved, than those will be indexed last and they
 * shouldn't be counted here.
 */
#define BNX2X_NUM_TX_NETQUEUES(bp) (BNX2X_NUM_ETH_QUEUES(bp) - 1)

/* RSS disabled by default */
#define ESX_DFLT_NUM_RSS_Q	0
#define BNX2X_NUM_RX_NETQUEUES(bp) \
		((int) ((BNX2X_NUM_ETH_QUEUES(bp) - 1) -   \
			((bp->esx.rss_p_num * bp->esx.rss_p_size) ?       \
			 (bp->esx.rss_p_num * (bp->esx.rss_p_size - 1)) : \
			 0)))

#define for_each_rx_net_queue(bp, var) \
	for (var = 1; var <= BNX2X_NUM_RX_NETQUEUES(bp); var++)
#define for_each_tx_net_queue(bp, var) \
	for (var = 1; var <= BNX2X_NUM_TX_NETQUEUES(bp); var++)

extern int enable_default_queue_filters;

enum bnx2x_esx_rx_mode_start {
	BNX2X_NETQ_IGNORE_START_RX_QUEUE = 0,
	BNX2X_NETQ_START_RX_QUEUE,
};

/* NetQueue flags mamangment macros. The field fp->esx.netq_flags is part of the
 * bnx2x_fastpath structure
 */
#define BNX2X_NETQ_RX_QUEUE_ALLOCATED	0x0001
#define BNX2X_NETQ_TX_QUEUE_ALLOCATED	0x0002
#define BNX2X_NETQ_RX_QUEUE_ACTIVE	0x0004
#define BNX2X_NETQ_TX_QUEUE_ACTIVE	0x0008

#define BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp) \
		(((fp->esx.netq_flags & BNX2X_NETQ_RX_QUEUE_ALLOCATED) == \
			   BNX2X_NETQ_RX_QUEUE_ALLOCATED) || \
			 (!(fp->bp->dev->features & NETIF_F_CNA) && \
			  (enable_default_queue_filters & fp->index) == 0))
#define BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp) \
		((fp->esx.netq_flags & BNX2X_NETQ_TX_QUEUE_ALLOCATED) == \
			  BNX2X_NETQ_TX_QUEUE_ALLOCATED)
#define BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp) \
		(((fp->esx.netq_flags & BNX2X_NETQ_RX_QUEUE_ACTIVE) == \
			   BNX2X_NETQ_RX_QUEUE_ACTIVE) || \
			  (!(fp->bp->dev->features & NETIF_F_CNA) && \
			   (enable_default_queue_filters & fp->index) == 0))
#define BNX2X_IS_NETQ_TX_QUEUE_ACTIVE(fp) \
		((fp->esx.netq_flags & BNX2X_NETQ_TX_QUEUE_ACTIVE) == \
			  BNX2X_NETQ_TX_QUEUE_ACTIVE)

/* NetQueue RX filter define's and macros */
#define BNX2X_NETQ_RX_FILTER_ACTIVE	0x0001

#define BNX2X_IS_NETQ_RX_FILTER_ACTIVE(fp, i) \
			((fp->esx.mac_filters[i].flags & \
			  BNX2X_NETQ_RX_FILTER_ACTIVE) == \
			  BNX2X_NETQ_RX_FILTER_ACTIVE)

/* For 4.1 and up we use the upper byte of the netq_flags field to store the
 * supported and reserved features for this specific netq. this field is set
 * once when the bnx2x is initially loaded and therefore should be preserved
 * as invariant when the fp is zero'd or moved.
 */

/* limit the number of lro queues */
#if (VMWARE_ESX_DDK_VERSION < 55000 || !defined(BNX2X_ESX_DYNAMIC_NETQ))
/* limit the number of lro queues */
#define BNX2X_NETQ_FP_LRO_RESERVED_MAX_NUM(bp)	(!CHIP_IS_E1(bp) ? 2 : 1)
#else
#define BNX2X_NETQ_FP_LRO_RESERVED_MAX_NUM(bp)  BNX2X_NUM_RX_NETQUEUES(bp)
#endif

#define BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT	8
#define BNX2X_NETQ_FP_FEATURES_RESERVED			2

#define BNX2X_NETQ_FP_FEATURES_RESERVED_MASK    \
		       (((2 << BNX2X_NETQ_FP_FEATURES_RESERVED) - 1) << \
			       BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT)

#define BNX2X_NETQ_FP_LRO_RESERVED	\
			(1 << BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT)

#define BNX2X_IS_NETQ_FP_FEAT_NONE_RESERVED(fp) \
	((fp->esx.netq_flags & BNX2X_NETQ_FP_FEATURES_RESERVED_MASK) == 0)
#define BNX2X_IS_NETQ_FP_FEAT(fp, feat) \
	((fp->esx.netq_flags & BNX2X_NETQ_FP_FEATURES_RESERVED_MASK) & feat)
#define BNX2X_IS_NETQ_FP_FEAT_LRO_RESERVED(fp)	\
	BNX2X_IS_NETQ_FP_FEAT(fp, BNX2X_NETQ_FP_LRO_RESERVED)


int bnx2x_netqueue_ops(vmknetddi_queueops_op_t op, void *args);

int bnx2x_get_lro_netqueue_count(struct bnx2x *bp);
void bnx2x_netq_clear_tx_rx_queues(struct bnx2x *bp);

#if (VMWARE_ESX_DDK_VERSION >= 50000)

#if (VMWARE_ESX_DDK_VERSION == 50000)
#ifndef NETIF_F_DEFQ_L2_FLTR
#define NETIF_F_DEFQ_L2_FLTR			(0x4000000000)
#endif
#endif

#define BNX2X_NETQ_FP_RSS_RESERVED	\
			(2 << BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT)
#define BNX2X_IS_NETQ_FP_FEAT_RSS_RESERVED(fp) \
	BNX2X_IS_NETQ_FP_FEAT(fp, BNX2X_NETQ_FP_RSS_RESERVED)
int
bnx2x_netq_init_rss(struct bnx2x *bp);

#if (VMWARE_ESX_DDK_VERSION >= 55000)
#include <net/encap_rss.h>

extern int disable_rss_dyn;

static inline vmklnx_rss_type bnx2x_esx_convert_rss_hash_type(enum eth_rss_hash_type htype)
{
	vmklnx_rss_type converter[MAX_ETH_RSS_HASH_TYPE] = {
		[IPV4_HASH_TYPE]      = VMKLNX_PKT_RSS_TYPE_IPV4,
		[TCP_IPV4_HASH_TYPE]  = VMKLNX_PKT_RSS_TYPE_IPV4_TCP,
		[IPV6_HASH_TYPE]      = VMKLNX_PKT_RSS_TYPE_IPV6,
		[TCP_IPV6_HASH_TYPE]  = VMKLNX_PKT_RSS_TYPE_IPV6_TCP,
	};
	return converter[htype];
}

#define BNX2X_ESX_PUT_RSS_INFO(fp, cqe, skb)                                   \
do {									       \
	if (BNX2X_IS_NETQ_FP_FEAT_RSS_RESERVED(fp) && !disable_rss_dyn) {      \
		if (cqe->status_flags & ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG) {   \
			rss_skb_put_info(skb, 				       \
				le32_to_cpu(cqe->rss_hash_result),             \
			bnx2x_esx_convert_rss_hash_type(cqe->status_flags &    \
					ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE));  \
		}							       \
	}								       \
} while (0)
#endif /*(VMWARE_ESX_DDK_VERSION >= 55000)*/
#endif

/* forward */
struct bnx2x_virtf;
struct bnx2x_vfop_filter;
struct bnx2x_vfop_filters;

#define bnx2x_esx_vf_headroom(bp)	(bnx2x_vf_headroom(bp) * 2)

int
bnx2x_esx_set_mac_passthru_config(struct bnx2x *bp, struct bnx2x_virtf *vf,
				  struct bnx2x_vfop_filters *fl);

int
bnx2x_esx_set_mtu_passthru_config(struct bnx2x *bp, struct bnx2x_virtf *vf);

int bnx2x_esx_check_active_vf_count(struct bnx2x *bp, char *text);

#ifdef BNX2X_ESX_SRIOV

extern int debug_unhide_nics;
extern unsigned int max_vfs_count;
extern int max_vfs[];

#define BNX2X_GET_MAX_VFS(index)	((index) > max_vfs_count ? \
					 0 : max_vfs[(index)])

VMK_ReturnStatus
bnx2x_pt_passthru_ops(struct net_device *netdev, vmk_NetPTOP op, void *args);

/* Must be called only after VF-Enable*/
int
bnx2x_vmk_vf_bus(struct bnx2x *bp, int vfid);

/* Must be called only after VF-Enable*/
int
bnx2x_vmk_vf_devfn(struct bnx2x *bp, int vfid);

#else
#define BNX2X_GET_MAX_VFS(index)	(0)

static inline u8 bnx2x_vf_is_pcie_pending(struct bnx2x *bp, u8 abs_vfid)
{
	return 0;
}

#endif
void
bnx2x_vmk_get_sriov_cap_pos(struct bnx2x *bp, vmk_uint16 *pos);

#define BNX2X_ESX_GET_STORM_STAT_64(x, y) \
	(HILO_U64(old_##x->y.hi, \
		  old_##x->y.lo) + \
	 HILO_U64(le32_to_cpu(x->y.hi), \
		  le32_to_cpu(x->y.lo)))
#define BNX2X_ESX_GET_STORM_STAT_32(x, y) \
	(old_##x->y + le32_to_cpu(x->y))

int
bnx2x_esx_save_statistics(struct bnx2x *bp, u8 vfID);

void
bnx2x_esx_vf_close(struct bnx2x *bp, struct bnx2x_virtf *vf);

void
bnx2x_esx_vf_release(struct bnx2x *bp, struct bnx2x_virtf *vf);

int
bnx2x_esx_iov_adjust_stats_req_to_pf(struct bnx2x *bp,
				     struct bnx2x_virtf *vf,
				     int vfq_idx,
				     struct stats_query_entry **cur_query_entry,
				     u8 *stats_count);

int
bnx2x_vmk_open_epilog(struct bnx2x *bp);

u8
bnx2x_vmk_vf_is_pcie_pending(struct bnx2x *bp, u8 abs_vfid);

int
__devinit bnx2x_vmk_iov_init_one(struct bnx2x *bp);

int bnx2x_esx_get_num_vfs(struct bnx2x *bp);

int bnx2x_filters_validate_mac(struct bnx2x *bp,
			       struct bnx2x_virtf *vf,
			       struct vfpf_set_q_filters_tlv *filters);

void pci_wake_from_d3(struct pci_dev *dev, bool enable);

#ifdef BNX2X_ESX_DYNAMIC_NETQ /* ! BNX2X_UPSTREAM */
#define for_each_eth_queue_(bp, var) \
	for ((var) = esx_netq_index; (var) <= esx_netq_index; (var)++)

#define for_each_tx_queue_(bp, var) \
	for_each_tx_queue(bp, var) \
		if (esx_netq_index == (var) || (esx_netq_index == 0 && \
		    ((var) == 0 || (var) >= BNX2X_NUM_ETH_QUEUES(bp))))

#define for_each_rx_queue_(bp, var) \
	for_each_rx_queue(bp, var) \
		if (esx_netq_index == (var) || (esx_netq_index == 0 && \
		    ((var) == 0 || (var) >= BNX2X_NUM_ETH_QUEUES(bp))))

#define for_each_nondefault_eth_queue_(bp, var) \
	for ((var) = 1; (var) <= 0; (var)++)
#define _STAEU

void bnx2x_free_tx_skbs_queue(struct bnx2x_fastpath *fp);
int bnx2x_esx_init_rx_ring(struct bnx2x *bp, int esx_netq_index);
void bnx2x_esx_free_rx_skbs_queue(struct bnx2x *bp, int esx_netq_index);
void bnx2x_free_rx_bds(struct bnx2x_fastpath *fp);
void bnx2x_esx_free_msix_irq(struct bnx2x *bp, int nvecs, int esx_netq_index);
int bnx2x_esx_req_msix_irq(struct bnx2x *bp, int esx_netq_index);
void bnx2x_esx_napi_disable_queue(struct bnx2x *bp, int esx_netq_index);
void bnx2x_esx_napi_enable_queue(struct bnx2x *bp, int esx_netq_index);
int bnx2x_esx_drain_tx_queue(struct bnx2x *bp, int esx_netq_index);
int bnx2x_esx_set_vlan_stripping(struct bnx2x *bp, bool set,
					int esx_netq_index);
int bnx2x_esx_init_netqs(struct bnx2x *bp);

static inline void bnx2x_free_rx_skbs(struct bnx2x *bp)
{
	bnx2x_esx_free_rx_skbs_queue(bp, 0);
}

static inline void bnx2x_free_msix_irqs(struct bnx2x *bp, int nvecs)
{
	bnx2x_esx_free_msix_irq(bp, nvecs, 0);
}

static inline int bnx2x_req_msix_irqs(struct bnx2x *bp)
{
	return bnx2x_esx_req_msix_irq(bp, 0);
}

static inline void bnx2x_napi_enable(struct bnx2x *bp)
{
	bnx2x_esx_napi_enable_queue(bp, 0);
}

static inline void bnx2x_napi_disable(struct bnx2x *bp)
{
	bnx2x_esx_napi_disable_queue(bp, 0);
}

int bnx2x_esx_alloc_fp_mem_at(struct bnx2x *bp, int index, int alloc_skb);
static inline int bnx2x_alloc_fp_mem_at(struct bnx2x *bp, int index)
{
	return bnx2x_esx_alloc_fp_mem_at(bp, index, 1);
}

static inline int bnx2x_set_vlan_stripping(struct bnx2x *bp, bool set)
{
	return bnx2x_esx_set_vlan_stripping(bp, set, 0);
}

#else /* !BNX2X_ESX_DYNAMIC_NETQ */
#define for_each_eth_queue_(bp, var)	for_each_eth_queue(bp, var)
#define for_each_tx_queue_(bp, var)	for_each_tx_queue(bp, var)
#define for_each_rx_queue_(bp, var)	for_each_rx_queue(bp, var)
#define for_each_nondefault_eth_queue_(bp, var) \
	for_each_nondefault_eth_queue(bp, var)
#define _STAEU	static
#endif	/* BNX2X_ESX_DYNAMIC_NETQ */

#if (VMWARE_ESX_DDK_VERSION >= 55000)
#include <vmklinux_9/vmklinux_dump.h>

extern int disable_fw_dmp;
#define BNX2X_MAX_NIC 32
#define BNX2X_DUMPNAME "bnx2x_fwdmp"
#define BNX2X_ESX_FW_DMP_VER 0x70038

#define DRV_DUMP_TRACE_BUFFER_SIZE               (0x800)
#define DRV_DUMP_VFC_DATA_SIZE                   (0x10000)
#define DRV_DUMP_IGU_DATA_SIZE                   (0x10000)
/* DRV_DUMP_TRACE_BUFFER_SIZE should be same as DBG_DMP_TRACE_BUFFER_SIZE in hw_dump.c*/
#define DRV_DUMP_PRELIMINARY_DATA_SIZE           DRV_DUMP_TRACE_BUFFER_SIZE

#define DRV_DUMP_SPLIT_REGISTERS_SIZE_DEFUALT    (0x2000)
#define DRV_DUMP_SPLIT_REGISTERS_SIZE_E1  DRV_DUMP_SPLIT_REGISTERS_SIZE_DEFUALT
#define DRV_DUMP_SPLIT_REGISTERS_SIZE_E1H DRV_DUMP_SPLIT_REGISTERS_SIZE_DEFUALT
#define DRV_DUMP_SPLIT_REGISTERS_SIZE_E2  DRV_DUMP_SPLIT_REGISTERS_SIZE_DEFUALT
#define DRV_DUMP_SPLIT_REGISTERS_SIZE_E3A0 DRV_DUMP_SPLIT_REGISTERS_SIZE_DEFUALT
#define DRV_DUMP_SPLIT_REGISTERS_SIZE_E3B0 DRV_DUMP_SPLIT_REGISTERS_SIZE_DEFUALT

#define DRV_DUMP_EXTRA_BLOCKS_SIZE         (DRV_DUMP_VFC_DATA_SIZE + \
					    DRV_DUMP_IGU_DATA_SIZE)
#define DRV_DUMP_CRASH_DMP_BUF_SIZE_E1     (0xB0000 + \
					    DRV_DUMP_PRELIMINARY_DATA_SIZE + \
					    DRV_DUMP_EXTRA_BLOCKS_SIZE + \
					    DRV_DUMP_SPLIT_REGISTERS_SIZE_E1)
#define DRV_DUMP_CRASH_DMP_BUF_SIZE_E1H    (0xE0000 + \
					    DRV_DUMP_PRELIMINARY_DATA_SIZE + \
					    DRV_DUMP_EXTRA_BLOCKS_SIZE + \
					    DRV_DUMP_SPLIT_REGISTERS_SIZE_E1H)
#define DRV_DUMP_CRASH_DMP_BUF_SIZE_E2     (0x100000 + \
					    DRV_DUMP_PRELIMINARY_DATA_SIZE + \
					    DRV_DUMP_EXTRA_BLOCKS_SIZE + \
					    DRV_DUMP_SPLIT_REGISTERS_SIZE_E2)
#define DRV_DUMP_CRASH_DMP_BUF_SIZE_E3A0   (0x140000 + \
					    DRV_DUMP_PRELIMINARY_DATA_SIZE + \
					    DRV_DUMP_EXTRA_BLOCKS_SIZE + \
					    DRV_DUMP_SPLIT_REGISTERS_SIZE_E3A0)
#define DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0   (0x140000 + \
					    DRV_DUMP_PRELIMINARY_DATA_SIZE + \
					    DRV_DUMP_EXTRA_BLOCKS_SIZE + \
					    DRV_DUMP_SPLIT_REGISTERS_SIZE_E3B0)

#define NIC_NAME_SIZE           (sizeof(((struct net_device *)0)->name))
#define DEFAULT_WAIT_INTERVAL_MICSEC 30

#define BNX2X_FWDMP_MARKER_END   0x454E44

#define RD_IND(bp, offset)      esx_reg_rd_ind(bp, offset)
#define WR_IND(bp, offset, val) esx_reg_wr_ind(bp, offset, val)

struct bnx2x_fwdmp_info {
    struct bnx2x *bp;
    int    disable_fwdmp;
};

struct dmp_config {
	u32     mode;
	u32     wregs_count;
	const struct wreg_addr *pwreg_addrs;
	u32     regs_timer_count;
	const u32 *regs_timer_status_addrs;
	const u32 *regs_timer_scan_addrs;
	u32     page_mode_values_count;
	const u32 *page_vals;
	u32     page_write_regs_count;
	const u32 *page_write_regs;
	u32     page_read_regs_count;
	const struct reg_addr *page_read_regs;
};

struct fw_dmp_hdr {
	u32	ver;
	u32	len;
	char	name[NIC_NAME_SIZE];
	void	*bp;
	u32	chip_id;
	u32	dmp_size;  /*actual firmware/chip dump size */
	u32	flags;
	#define FWDMP_FLAGS_SPACE_NEEDED  0x00000001
	#define FWDMP_FLAGS_LIVE_DUMP     0x00000002
	u32	reserved;
}  __attribute__((packed));

struct chip_core_dmp {
	struct fw_dmp_hdr       fw_hdr;
	u32                     fw_dmp_buf[(DRV_DUMP_CRASH_DMP_BUF_SIZE_E3B0 -
					sizeof(struct fw_dmp_hdr))];
};

extern int RSS;
extern vmklnx_DumpFileHandle bnx2x_fwdmp_dh;
extern void *bnx2x_fwdmp_va;
extern struct bnx2x_fwdmp_info bnx2x_fwdmp_bp[BNX2X_MAX_NIC+1];

VMK_ReturnStatus bnx2x_fwdmp_callback(void *cookie, vmk_Bool liveDump);
void bnx2x_disable_esx_fwdmp(struct bnx2x *bp);
#endif

#if (VMWARE_ESX_DDK_VERSION >= 50000)
extern int bnx2x_esx_mod_init(void);
extern void bnx2x_esx_mod_cleanup(void);
extern struct cnic_eth_dev *bnx2x_cnic_probe(struct net_device *dev);
#endif

int bnx2x_esx_init_bp(struct bnx2x *bp);
void bnx2x_esx_cleanup_bp(struct bnx2x *bp);

#if BNX2X_ESX_PRINT_TIMESTAMP
#define BNX2X_ESX_PRINT_START_TIMESTAMP() \
	{ \
		u64 timestamp; \
		rdtscll(timestamp); \
		BNX2X_ERR("start timestamp: %lld\n", timestamp); \
	}
#define BNX2X_ESX_PRINT_END_TIMESTAMP() \
	{ \
		u64 timestamp; \
		rdtscll(timestamp); \
		BNX2X_ERR("end timestamp: %lld\n", timestamp); \
	}
#else
#define BNX2X_ESX_PRINT_START_TIMESTAMP()
#define BNX2X_ESX_PRINT_END_TIMESTAMP()
#endif

/*  Direct manipulation of the semaphore is necessary on ESX.
 *  This is because on ESX BETA builds there is an
 *  ASSERT which checks if the worldlet which took the lock is
 *  also the one who releases the lock.   On ESX, the sp_task
 *  can run on different helper worldlets and is not guarenteed
 *  to run on the same worldlet which will trigger the ASSERT.
 *  As suggested by VMware the driver should access the
 *  semaphore directly
 */

#define mutex_lock(x) vmk_mutex_lock(x)
static inline void
vmk_mutex_lock(struct mutex *m)
{
	down(&m->lock);
}

#define mutex_unlock(x) vmk_mutex_unlock(x)
static inline void
vmk_mutex_unlock(struct mutex *m)
{
	up(&m->lock);
}
#define BNX2X_CHK_LEAK_NONE		0
#define BNX2X_CHK_LEAK_RX_SKB		(1 << 0)
#define BNX2X_CHK_LEAK_HEAP		(1 << 1)
#define BNX2X_CHK_LEAK_PAGE		(1 << 2)
#define BNX2X_CHK_LEAK_ALL		(~BNX2X_CHK_LEAK_NONE)
#define BNX2X_CHK_LEAK_MODE		(BNX2X_CHK_LEAK_NONE)

#if (BNX2X_CHK_LEAK_MODE)
#define BNX2X_CHK_LEAK_DISPLAY(leak_type) {\
	int _i, _temp, _total = 0; \
	if ((leak_type) & BNX2X_CHK_LEAK_RX_SKB) { \
		for_each_queue(bp, _i) {\
			_temp = (int)atomic_read( \
				&bp->fp[_i].esx.rx_skb_count); \
			if (_temp) { \
				BNX2X_ERR("RX SKB CHK: q(%d):%d\n", \
					_i, _temp); \
				_total += _temp; \
			} \
		} \
		if (_total == 0) \
			BNX2X_ERR("RX SKB CHK: passed\n"); \
	} \
	if ((leak_type) & BNX2X_CHK_LEAK_PAGE) { \
		_temp = (int)atomic_read(&bp->esx.rx_page_count); \
		if (_temp) \
			BNX2X_ERR("PAGE CHK: %d\n", _temp); \
	} \
	if ((leak_type) & BNX2X_CHK_LEAK_HEAP) { \
		_temp = (int)atomic_read(&bp->esx.vheap_count); \
		if (_temp) \
			BNX2X_ERR("VHEAP CHK: %d\n", _temp); \
		_total += _temp; \
		_temp = (int)atomic_read(&bp->esx.kheap_count); \
		if (_temp) \
			BNX2X_ERR("KHEAP CHK: %d\n", _temp); \
		_total += _temp; \
		if (_total == 0) \
			BNX2X_ERR("HEAP CHK: passed\n"); \
	} \
}

#else
#define BNX2X_CHK_LEAK_DISPLAY(leak_type)
#endif

#if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_RX_SKB)
#define BNX2X_ESX_NETDEV_ALLOC_SKB_RX(dev, len) \
({ \
	void *_temp; \
	_temp = netdev_alloc_skb(dev, len); \
	if (_temp) \
		atomic_inc(&fp->esx.rx_skb_count); \
	_temp; \
})

#define BNX2X_ESX_BUILD_SKB_RX(data, len) \
({ \
	void *_temp; \
	_temp = build_skb(data, len); \
	if (_temp) \
		atomic_inc(&fp->esx.rx_skb_count); \
	_temp; \
})

#else
#define BNX2X_ESX_NETDEV_ALLOC_SKB_RX(dev, len) \
	netdev_alloc_skb(dev, len)

#define BNX2X_ESX_BUILD_SKB_RX(data, len) build_skb(data, len)
#endif

#define BNX2X_ESX_DEV_KFREE_SKB_ANY_RX(data) \
do { \
	if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_RX_SKB) { \
		if (data) \
			atomic_dec(&fp->esx.rx_skb_count); \
	} \
	dev_kfree_skb_any(data); \
} while (0)

#define BNX2X_ESX_DEV_KFREE_SKB_RX(data) \
do { \
	if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_RX_SKB) { \
		if (data) \
			atomic_dec(&fp->esx.rx_skb_count); \
	} \
	dev_kfree_skb(data); \
} while (0)

#define BNX2X_ESX_NAPI_GRO_RECEIVE(x, data) \
do { \
	if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_RX_SKB) { \
		if (data) \
			atomic_dec(&fp->esx.rx_skb_count); \
	} \
	napi_gro_receive(x, data); \
} while (0)

#define BNX2X_ESX_VLAN_GRO_RECEIVE(x, y, z, data) \
do { \
	if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_RX_SKB) { \
		if (data) \
			atomic_dec(&fp->esx.rx_skb_count); \
	} \
	vlan_gro_receive(x, y, z, data); \
} while (0)

#if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_PAGE)
#define BNX2X_ESX_ALLOC_PAGES(x, y) \
({ \
	void *_temp; \
	_temp = alloc_pages(x, y); \
	if (_temp) \
		atomic_inc(&bp->esx.rx_page_count); \
	_temp; \
})

#define BNX2X_ESX_FREE_PAGES(data, y) \
do { \
	if (data) { \
		atomic_dec(&bp->esx.rx_page_count); \
	} \
	__free_pages(data, y); \
} while (0)
#else
#define BNX2X_ESX_ALLOC_PAGES(x, y)  alloc_pages(x, y)
#define BNX2X_ESX_FREE_PAGES(data, y) __free_pages(data, y)
#endif

#if (BNX2X_CHK_LEAK_MODE & BNX2X_CHK_LEAK_HEAP)

#define BNX2X_ESX_VMALLOC(size) \
({ \
	void *_temp; \
	_temp = vmalloc(size); \
	if (_temp) \
		atomic_inc(&bp->esx.vheap_count); \
	_temp; \
})

#define BNX2X_ESX_VZALLOC(size) \
({ \
	void *_temp; \
	_temp = vzalloc(size); \
	if (_temp) \
		atomic_inc(&bp->esx.vheap_count); \
	_temp; \
})

#define BNX2X_ESX_KMALLOC(size, y) \
({ \
	void *_temp; \
	_temp = kmalloc(size, y); \
	if (_temp) \
		atomic_inc(&bp->esx.kheap_count); \
	_temp; \
})

#define BNX2X_ESX_KZALLOC(size, y) \
({ \
	void *_temp; \
	_temp = kzalloc(size, y); \
	if (_temp) \
		atomic_inc(&bp->esx.kheap_count); \
	_temp; \
})

#define BNX2X_ESX_KCALLOC(len, size, y) \
({ \
	void *_temp; \
	_temp = kcalloc(len, size, y); \
	if (_temp) \
		atomic_inc(&bp->esx.kheap_count); \
	_temp; \
})

#define BNX2X_ESX_VFREE(data) \
do { \
	if (data) \
		atomic_dec(&bp->esx.vheap_count); \
	vfree(data); \
} while (0)

#define BNX2X_ESX_KFREE(data) \
do { \
	if (data) \
		atomic_dec(&bp->esx.kheap_count); \
	kfree(data); \
} while (0)

#else
#define BNX2X_ESX_ALLOC_PAGES(x, y)	alloc_pages(x, y)
#define BNX2X_ESX_VMALLOC(size)	vmalloc(size)
#define BNX2X_ESX_VZALLOC(size)	vzalloc(size)
#define BNX2X_ESX_KMALLOC(size, y)     kmalloc(size, y)
#define BNX2X_ESX_KZALLOC(size, y)     kzalloc(size, y)
#define BNX2X_ESX_KCALLOC(len, size, y)     kcalloc(len, size, y)
#define BNX2X_ESX_FREE_PAGES(data, y) __free_pages(data, y)
#define BNX2X_ESX_VFREE(data)  vfree(data)
#define BNX2X_ESX_KFREE(data)  kfree(data)
#endif

#endif  /* BNX2X_ESX_H */
