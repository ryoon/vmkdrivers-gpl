/* bnx2x_vf.h: Broadcom Everest network driver.
 *
 * Copyright 2009-2011 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2, available
 * at http://www.gnu.org/licenses/old-licenses/gpl-2.0.html (the "GPL").
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Shmulik Ravid
 *
 */
#ifndef BNX2X_VF_H
#define BNX2X_VF_H

#ifndef __VMKLNX__ /* BNX2X_UPSTREAM */
#define VFPF_MBX
#endif

#include "bnx2x_vfpf_if.h"

/*
 * The bnx2x device structure holds vfdb structure described below.
 * The VF array is indexed by the relative vfid.
 */

#define BNX2X_VF_MAX_QUEUES		16
#define BNX2X_VF_MAX_TPA_AGG_QUEUES	8

/* VF FP sub-states - apply only to L2 FP serving SRIOV VFs */
#define BNX2X_FP_SUB_STATE_ACTIVATED	0
#define BNX2X_FP_SUB_STATE_DEACTIVATED	0x0100
#define BNX2X_FP_SUB_STATE_ACTIVATING	0x0200
#define BNX2X_FP_SUB_STATE_DEACTIVATING	0x0300
#define BNX2X_FP_SUB_STATE_TERMINATING	0x0400
#define BNX2X_FP_SUB_STATE_MASK		0x0f00
/* TERMINATED == CLOSED */

struct bnx2x_sriov {
	u32 first_vf_in_pf;

	/*
	 * standard SRIOV capability fields, mostly for debugging
	 */
	int pos;		/* capability position */
	int nres;		/* number of resources */
	u32 cap;		/* SR-IOV Capabilities */
	u16 ctrl;		/* SR-IOV Control */
	u16 total;		/* total VFs associated with the PF */
	u16 initial;		/* initial VFs associated with the PF */
	u16 nr_virtfn;		/* number of VFs available */
	u16 offset;		/* first VF Routing ID offset */
	u16 stride;		/* following VF stride */
	u32 pgsz;		/* page size for BAR alignment */
	u8 link;		/* Function Dependency Link */
};

/* bars */
struct bnx2x_vf_bar {
	u64 bar;
	u32 size;
};
struct bnx2x_vf_bar_info {
	struct bnx2x_vf_bar bars[PCI_SRIOV_NUM_BARS];
	u8 nr_bars;
};


/* vf queue (used both for rx or tx) */
struct bnx2x_vfq {
	struct eth_context *cxt;

	/* MACs object */
	struct bnx2x_vlan_mac_obj	mac_obj;

	/* VLANs object */
	struct bnx2x_vlan_mac_obj	vlan_obj;
	atomic_t vlan_count;		/* 0 means vlan-0 is set  ~ untagged */


	u32 error;		/* 0 means all's-well */
	u32 cid;
	u16 state;
	u8 index;
	u8 sb_idx;
	u16 update_pending;
#define BNX2X_VF_UPDATE_DONE	0
#define BNX2X_VF_UPDATE_PENDING	1

};

/* statistics */

struct bnx2x_vfq_fw_stats {
	/* RX */
	u32 total_unicast_packets_received_hi;
	u32 total_unicast_packets_received_lo;
	u32 total_unicast_bytes_received_hi;
	u32 total_unicast_bytes_received_lo;
	u32 total_multicast_packets_received_hi;
	u32 total_multicast_packets_received_lo;
	u32 total_multicast_bytes_received_hi;
	u32 total_multicast_bytes_received_lo;
	u32 total_broadcast_packets_received_hi;
	u32 total_broadcast_packets_received_lo;
	u32 total_broadcast_bytes_received_hi;
	u32 total_broadcast_bytes_received_lo;
	u32 no_buff_discard_hi;
	u32 no_buff_discard_lo;
	u32 etherstatsoverrsizepkts_hi;
	u32 etherstatsoverrsizepkts_lo;
	u32 error_discard_hi;
	u32 error_discard_lo;


	/* TX */
	u32 total_unicast_packets_sent_hi;
	u32 total_unicast_packets_sent_lo;
	u32 total_unicast_bytes_sent_hi;
	u32 total_unicast_bytes_sent_lo;
	u32 total_multicast_packets_sent_hi;
	u32 total_multicast_packets_sent_lo;
	u32 total_multicast_bytes_sent_hi;
	u32 total_multicast_bytes_sent_lo;
	u32 total_broadcast_packets_sent_hi;
	u32 total_broadcast_packets_sent_lo;
	u32 total_broadcast_bytes_sent_hi;
	u32 total_broadcast_bytes_sent_lo;
	u32 tx_error_packets_hi;
	u32 tx_error_packets_lo;
};

struct bnx2x_vfq_stats {
	struct tstorm_per_queue_stats old_tclient;
	struct ustorm_per_queue_stats old_uclient;
	struct xstorm_per_queue_stats old_xclient;
	struct bnx2x_vfq_fw_stats qstats;
#ifdef BNX2X_PASSTHRU
	vmk_NetVFTXQueueStats	old_vmk_txq;
	vmk_NetVFRXQueueStats	old_vmk_rxq;
#endif
};


/* vf state */
struct bnx2x_virtf {
	u16 cfg_flags;
#define VF_CFG_STATS		0x0001
#define VF_CFG_FW_FC		0x0002
#define VF_CFG_TPA		0x0004
#define VF_CFG_INT_SIMD		0x0008
#define VF_CACHE_LINE		0x0010

	/* rss params TODO */

	u8 state;
#define VF_FREE		0	/* VF ready to be acquired holds no resc */
#define VF_ACQUIRED	1	/* VF aquired, but not initalized */
#define VF_ENABLED	2	/* VF Enabled */
#define VF_RESET	3	/* VF FLR'd, pending cleanup */

	u8 during_clnup;	/* non 0 during final cleanup*/
#define VF_CLNUP_RESC	1	/* reclaiming the resources stage ~ 'final
				   cleanup' w/o te end wait */
#define VF_CLNUP_EPILOG	2	/* wait for vf remnants to dissipate in
				   the HW ~ the end wait of 'final cleanup' */

	/* dma */
	dma_addr_t fw_stat_map;		/* valid iff VF_CFG_STATS */
	dma_addr_t spq_map;		/* valid iff VF_CFG_STATS */

	struct vf_pf_resc_request	alloc_resc;
	#define max_sb_count		alloc_resc.num_sbs
	#define rxq_count		alloc_resc.num_rxqs
	#define txq_count		alloc_resc.num_txqs
	#define mac_rules_count		alloc_resc.num_mac_filters
	#define vlan_rules_count	alloc_resc.num_vlan_filters

	/*
	u8 max_sb_count;
	u8 rxq_count;
	u8 txq_count;
	u16 mac_rules_count;
	u16 vlan_rules_count;
	*/

	u8 sb_count;	/* actual number of SBs */
	u8 igu_base_id;	/* base igu status block id */

	struct bnx2x_vfq *rxq;
#define VF_RXQ(vf, idx)	(&(vf)->rxq[(idx)])
#define bnx2x_vf_rxq(vf, nr, var) (vf)->rxq[(nr)].var

#define VF_LEADING_RXQ(vf)	VF_RXQ(vf, 0)
#define VF_IS_LEADING_RXQ(rxq)	((rxq)->index == 0)

	struct bnx2x_vfq *txq;
#define VF_TXQ(vf, idx)	(&(vf)->txq[(idx)])
#define bnx2x_vf_txq(vf, nr, var) (vf)->txq[(nr)].var

	/* statistics */
	struct bnx2x_vfq_stats *vfq_stats;

	u8 index;	/* index in the vf array */
	u8 abs_vfid;
	u8 sp_cl_id;
	u32 error;	/* 0 means all's-well */

	/* BDF */
	unsigned int bus;
	unsigned int devfn;

	/* bars */
	struct bnx2x_vf_bar bars[PCI_SRIOV_NUM_BARS];

	/* set-mac ramrod state 1-pending, 0-done */
	unsigned long	filter_state;

	/*
	 * leading rss client id ~~ the client id of the first rxq, must be
	 * set for each txq.
	 */
	int leading_rss;

	/* MCAST object */
	struct bnx2x_mcast_obj		mcast_obj;

	/* RSS configuration object */
	struct bnx2x_rss_config_obj     rss_conf_obj;


#ifdef BNX2X_PASSTHRU /* ! BNX2X_UPSTREAM */
	u16 mtu;
	unsigned long queue_flags;
	bool def_vlan_enabled;
	struct bnx2x_vf_plugin_stats *plugin_stats;
#endif
};

#define BNX2X_NR_VIRTFN(bp)	(bp)->vfdb->sriov.nr_virtfn

#define for_each_vf(bp, var) \
		for ((var) = 0; (var) < BNX2X_NR_VIRTFN(bp); (var)++)

#define for_each_vf_rxq(vf, var) \
		for ((var) = 0; (var) < ((vf)->rxq_count); (var)++)

#define for_each_vf_txq(vf, var) \
		for ((var) = 0; (var) < ((vf)->txq_count); (var)++)

#define for_each_vf_sb(vf, var) \
		for ((var) = 0; (var) < ((vf)->sb_count); (var)++)

#define is_vf_multi(vf)	((vf)->rxq_count > 1)

#define HW_VF_HANDLE(bp, abs_vfid) \
	(u16)(BP_ABS_FUNC((bp)) | (1<<3) |  ((u16)(abs_vfid) << 4))

#define FW_PF_MAX_HANDLE	8

#define FW_VF_HANDLE(abs_vfid)	\
	(abs_vfid + FW_PF_MAX_HANDLE)

#define ABS_VFID_FORM_FW_VF_HANDLE(funcid) \
	(funcid - FW_PF_MAX_HANDLE)

#define IS_PF_FORM_FW_VF_HANDLE(funcid) \
	(funcid < FW_PF_MAX_HANDLE)

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */

/*
 * VF mail box (aka vf-pf channel)
 */

/* a container for the bi-directional vf<-->pf messages.
   The actual response will be placed according to the offset parameter
   provided in the request
*/

#define MBX_MSG_ALIGN	8
#define MBX_MSG_ALIGNED_SIZE	(roundup(sizeof(struct bnx2x_vf_mbx_msg), \
				MBX_MSG_ALIGN))

struct bnx2x_vf_mbx_msg {
	union vf_pf_msg	req;
	union pf_vf_msg resp;
};

struct bnx2x_vf_mbx {
	struct bnx2x_vf_mbx_msg *msg;
	dma_addr_t msg_mapping;

	/* VF GPA address */
	__le32 vf_addr_lo;
	__le32 vf_addr_hi;

	struct vf_pf_msg_hdr hdr;	/* saved VF request header */

	u8 flags;
#define VF_MSG_INPROCESS	0x1	/* failsafe - the FW should prevent
					 * more then one pending msg
					 */
};
#endif /* VFPF_MBX */

struct client_init_info {
	struct client_init_ramrod_data *ramrod_data;
	dma_addr_t ramrod_mapping;
	size_t size;
};

struct bnx2x_vf_sp {
	union {
		struct eth_classify_rules_ramrod_data	e2;
	} mac_rdata;

	union {
		struct eth_classify_rules_ramrod_data	e2;
	} vlan_rdata;

	union {
		struct eth_filter_rules_ramrod_data	e2;
	} rx_mode_rdata;

	union {
		struct eth_multicast_rules_ramrod_data  e2;
	} mcast_rdata;

	struct client_init_ramrod_data		client_init;
	struct client_update_ramrod_data	client_update;
};

struct hw_dma {
	void *addr;
	dma_addr_t mapping;
	size_t size;
};

struct bnx2x_vfdb {

#define BP_VFDB(bp)		((bp)->vfdb)
	/* vf array */
	struct bnx2x_virtf	*vfs;
#define BP_VF(bp, idx)		(&((bp)->vfdb->vfs[(idx)]))
#define bnx2x_vf(bp, idx, var)	((bp)->vfdb->vfs[(idx)].var)

	/* rxq array - for all vfs */
	struct bnx2x_vfq *rxqs;

	/* txq array - for all vfs */
	struct bnx2x_vfq *txqs;

	/* stats array - for all vfs */
	struct bnx2x_vfq_stats *vfq_stats;

	/* vf HW contexts */
	struct hw_dma		context[BNX2X_VF_CIDS/ILT_PAGE_CIDS];
#define	BP_VF_CXT_PAGE(bp,i)	(&(bp)->vfdb->context[(i)])

	/* SR-IOV information */
	struct bnx2x_sriov	sriov;


	/* Tx switching MAC object*/
	struct bnx2x_vlan_mac_obj tx_mac_obj;
#define BP_TX_MAC_OBJ(bp)	(&((bp)->vfdb->tx_mac_obj))

#ifdef VFPF_MBX /* BNX2X_UPSTREAM */
	struct hw_dma		mbx_dma;
#define BP_VF_MBX_DMA(bp)	(&((bp)->vfdb->mbx_dma))

	struct bnx2x_vf_mbx	mbxs[BNX2X_MAX_NUM_OF_VFS];
#define BP_VF_MBX(bp, vfid)	(&((bp)->vfdb->mbxs[(vfid)]))
#endif
	struct hw_dma		sp_dma;
#define BP_VF_SP(bp, vf, field) ((bp)->vfdb->sp_dma.addr + \
		(vf)->index * sizeof(struct bnx2x_vf_sp) + \
		offsetof(struct bnx2x_vf_sp, field))

#define BP_VF_SP_MAP(bp, vf, field) ((bp)->vfdb->sp_dma.mapping + 	\
		(vf)->index * sizeof(struct bnx2x_vf_sp) + 		\
		offsetof(struct bnx2x_vf_sp, field))

	struct vf_pf_resc_request avail_resc;

};

/* FW ids */
static inline u8 __vf_igu_sb(struct bnx2x_virtf *vf, u16 sb_idx)
{
	return (vf->igu_base_id + sb_idx);
}
static inline u8 __vf_fw_sb(struct bnx2x_virtf *vf, u16 sb_idx)
{
	return __vf_igu_sb(vf, sb_idx);
}
static inline u8 __vf_hc_qzone(struct bnx2x_virtf *vf, u16 sb_idx)
{
	return __vf_igu_sb(vf, sb_idx);
}
static inline u8 vfq_igu_sb_id(struct bnx2x_virtf *vf, struct bnx2x_vfq *q)
{
	return (vf->igu_base_id + q->sb_idx);
}
static inline u8 vfq_fw_sb_id(struct bnx2x_virtf *vf, struct bnx2x_vfq *q)
{
	return vfq_igu_sb_id(vf, q);
}
static inline u8 vfq_cl_id(struct bnx2x_virtf *vf, struct bnx2x_vfq *q)
{
	return vfq_igu_sb_id(vf, q);
}
static inline u8 vfq_stat_id(struct bnx2x_virtf *vf, struct bnx2x_vfq *q)
{
	return vfq_cl_id(vf, q);
}

static inline u8 vfq_qzone_id(struct bnx2x_virtf *vf, struct bnx2x_vfq *q)
{
	return vfq_igu_sb_id(vf, q);
}


/* forward */
struct bnx2x;

/* global iov routines */
int bnx2x_iov_init_ilt(struct bnx2x *bp, u16 line);
int bnx2x_iov_init_one(struct bnx2x *bp, int int_mode_param, int num_vfs_param);
void bnx2x_iov_remove_one(struct bnx2x *bp);
void bnx2x_iov_free_mem(struct bnx2x* bp);
int bnx2x_iov_alloc_mem(struct bnx2x* bp);
int bnx2x_iov_nic_init(struct bnx2x *bp);
int bnx2x_iov_chip_cleanup(struct bnx2x *bp);
void bnx2x_iov_init_dq(struct bnx2x *bp);
void bnx2x_iov_init_dmae(struct bnx2x *bp);
int bnx2x_iov_sp_event(struct bnx2x *bp, int vf_cid, int command);
int bnx2x_iov_eq_sp_event(struct bnx2x* bp, union event_ring_elem *elem);
int bnx2x_iov_set_tx_mac(struct bnx2x *bp, u8 *mac, bool add);
u8 bnx2x_iov_get_max_queue_count(struct bnx2x * bp);
void bnx2x_iov_adjust_stats_req(struct bnx2x * bp);
void bnx2x_iov_storm_stats_update(struct bnx2x *bp);

#ifdef VFPF_MBX
/* global vf mailbox routines */
void bnx2x_vf_mbx(struct bnx2x *bp, struct vf_pf_event_data *vfpf_event);
void bnx2x_set_vf_mbxs_valid(struct bnx2x *bp);
#endif


/*
 * 	CORE VF API
 */

enum vf_api_rc {
	VF_API_SUCCESS = 0,
	VF_API_FAILURE,
	VF_API_NO_RESOURCE,
	VF_API_PENDING
};

typedef enum vf_api_rc vf_api_t;

typedef u8 bnx2x_mac_addr_t[ETH_ALEN];

/* acquire */
vf_api_t bnx2x_vf_acquire(struct bnx2x *bp, struct bnx2x_virtf *vf,
			  struct vf_pf_resc_request* resc);


/* init */
vf_api_t bnx2x_vf_init(struct bnx2x *bp, struct bnx2x_virtf *vf,
		       dma_addr_t *sb_map);


/* queue setup */
vf_api_t bnx2x_vf_rxq_setup(struct bnx2x *bp, struct bnx2x_virtf *vf,
			    struct bnx2x_vfq *rxq,
			    struct bnx2x_client_init_params *p,
			    u8 activate);

vf_api_t bnx2x_vf_txq_setup(struct bnx2x *bp, struct bnx2x_virtf *vf,
			    struct bnx2x_vfq *txq,
			    struct bnx2x_client_init_params *p);

/* queue teardown */
vf_api_t bnx2x_vfq_teardown(struct bnx2x *bp, struct bnx2x_virtf *vf,
			     struct bnx2x_vfq *rxq);

/* trigger ~ activate/deactivate */
vf_api_t bnx2x_vf_trigger_q(struct bnx2x *bp ,struct bnx2x_virtf *vf,
			    struct bnx2x_vfq *rxq, u8 activate);
vf_api_t bnx2x_vf_trigger(struct bnx2x *bp ,struct bnx2x_virtf *vf, u8 activate);

/* close (shutdown) */
vf_api_t bnx2x_vf_close(struct bnx2x *bp ,struct bnx2x_virtf *vf);

/*
 * release ~ close + release-resources
 *
 * release is the ultimate SW shutdown and is called whenever an irrecoverable
 * is encountered.
 */
void bnx2x_vf_release(struct bnx2x *bp ,struct bnx2x_virtf *vf);


/* set MAC + VLAN */
bool bnx2x_vf_check_vlan_op(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			    u16 vtag, bool chk_add);

vf_api_t bnx2x_vf_set_mac(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			       u8 *mac, bool add, unsigned long ramrod_flags);

vf_api_t bnx2x_vf_set_vlan(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			   u16 vtag, bool add, unsigned long ramrod_flags);

vf_api_t bnx2x_vf_set_mcasts(struct bnx2x *bp, struct bnx2x_virtf *vf,
			     bnx2x_mac_addr_t *mcasts, int mcast_num,
			     bool drv_only);

vf_api_t bnx2x_vf_set_rxq_mode(struct bnx2x* bp,struct bnx2x_virtf *vf,
			       struct bnx2x_vfq *rxq,
			       unsigned long accept_flags);

vf_api_t bnx2x_vf_clear_vlans(struct bnx2x *bp, struct bnx2x_vfq *rxq,
			      bool skip, u16 vlan_to_skip,
			      unsigned long ramrod_flags);


/* statistics */

/*
 * helper routines
 */
u8 bnx2x_vfid_valid(struct bnx2x* bp, int vfid);

void bnx2x_vf_get_sbdf(struct bnx2x *bp, struct bnx2x_virtf *vf, u32* sbdf);

void bnx2x_vf_get_bars(struct bnx2x *bp, struct bnx2x_virtf *vf,
		       struct bnx2x_vf_bar_info *bar_info);


void storm_memset_rcq_np(struct bnx2x *bp, dma_addr_t np_map, u8 cl_id);
int bnx2x_vf_queue_update_ramrod(struct bnx2x *bp, struct bnx2x_vfq *rxq,
				 dma_addr_t data_mapping, u8 block);


/*
 *	FLR routines:
 */

/*
 *	bnx2x_pf_flr_clnup
 *	a. re-enable target read on the PF
 *	b. poll cfc per function usgae counter
 *	c. poll the qm perfunction usage counter
 *	d. poll the tm per function usage counter
 *	e. poll the tm per function scan-done indication
 *	f. clear the dmae channel associated wit hthe PF
 *	g. zero the igu 'trailing edge' and 'leading edge' regs (attentions)
 *	h. call the common flr cleanup code with -1 (pf indication)
 *
 */
int bnx2x_pf_flr_clnup(struct bnx2x *bp);

/*
 *	bnx2x_vf_flr_clnup
 *	a. erase the vf queue ids (client ids) form the pxp protection table
 *	b. wait for outstanding vf ramrods to complete
 *	c. send terminate ramrod for each vf queue
 *	d. call the common flr cleanup code with the vfid
 */
int bnx2x_vf_flr_clnup(struct bnx2x *bp, struct bnx2x_virtf *vf);

/*
 *	bnx2x_flr_clnup - clenaup code share by vf and pf flr
 *	fn >= 0 -> called during vf flr cleanup and fn is vfid
 *
 *	a. poll DQ function usage counter
 *	b. Invoke FW cleanup
 *	c. ATC function cleanup
 *	d. PBF function cleanup
 */
int bnx2x_flr_clnup(struct bnx2x *bp, int fn);


/*
 *	bnx2x_flr_clnup_epilog (temporary name) -
 *
 *	bnx2x_flr_clnup_epilog (temporary name) - For vfs it must be called
 *	after bnx2x_flr_clnup and before any vf initializations take place.
 *	in the case VMware NPA the pf does all the initialization on behalf
 *	of the vf, so the routine is called during vf_init.
 *
 *	a. For vfs only - wait for all PF ramrods that access any vf DBs to
 *	complete. A prominent example is the statistics ramrod.
 *	(simplification - lock-out PF ramrods and wait for all outstanding
 *	PF ramrod to complete).
 *	b. wait 100 ms for VF remnants in the HW to dissipate.
 *	c. Verify that the pending-transaction bit in device-status register
 *	(capability structure) is cleared
 */
int bnx2x_flr_clnup_epilog(struct bnx2x *bp, struct bnx2x_virtf *vf);



/*
 * Handles an FLR (or VF_DISABLE) notification form the MCP
 */
void bnx2x_vf_handle_flr_event(struct bnx2x* bp);

/* debug */
void bnx2x_vf_handle_flr_self_notification(struct bnx2x* bp, u8 abs_vfid);


#endif /* bnx2x_vf.h*/
