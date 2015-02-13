/* bnx2x_vfpf_if.h: Broadcom Everest network driver.
 *
 * Copyright (c) 2010-2011 Broadcom Corporation
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
 *
 */
#ifndef VF_PF_IF_H
#define VF_PF_IF_H

/***********************************************
 *
 * Common definitions for all HVs
 *
 **/
struct vf_pf_resc_request {
	u8  num_rxqs;
	u8  num_txqs;
	u8  num_sbs;
	u8  num_mac_filters;
	u8  num_vlan_filters;
	u8  num_mc_filters; /* No limit  so superfluous */
};

#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_begin.h"
#endif
struct hw_sb_info {
	u8 hw_sb_id;	/* aka absolute igu id, used to ack the sb */
	u8 sb_qid;	/* used to update DHC for sb */
};
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_end.h"
#endif



#if defined(BNX2X_PASSTHRU)
/***********************************************
 *
 * ESX 5.0 (MN) PassThrough (NPA)
 *
 * Common definitions shared betweem the HV and
 * the plugin
 *
 **/

#define BNX2X_PLUGIN_MAX_RX_QUEUES	8
#define BNX2X_PLUGIN_MAX_TX_QUEUES	8

#define PLUGIN_MAX_SB			BNX2X_PLUGIN_MAX_RX_QUEUES
#define PLUGIN_MAX_QUEUES		BNX2X_PLUGIN_MAX_RX_QUEUES

#define PLUGIN_MAX_SGES			8
#define SHELL_SMALL_RECV_BUFFER_SIZE	2048
#define SHELL_LARGE_RECV_BUFFER_SIZE	4096

/* FW Pages (always 4K) */
#define PLUGIN_RING_UNIT_SHIFT		12
#define PLUGIN_RING_UNIT_SIZE		(1 << PLUGIN_RING_UNIT_SHIFT)

/* TX queues */
#define TX_DESC_PAGE	        (PLUGIN_RING_UNIT_SIZE / sizeof(union eth_tx_bd_types))
#define TX_DESC_NUM_PAGES(x)   	DIV_ROUND_UP((x), TX_DESC_PAGE)
#define TX_DESC_SIZE(x)         (TX_DESC_NUM_PAGES((x)) * PLUGIN_RING_UNIT_SIZE)


/* RX queues */
/*
 * SGE
 * Note: RX_SGE_NUM_PAGES should be rounded up to next power of 2.
 * In acuality it does not exceed 2 pages so a round up to the next multiple is good enough.
 */
#define RX_SGE_PAGE		(PLUGIN_RING_UNIT_SIZE / sizeof(struct eth_rx_sge))
#define RX_SGE_COUNT            128
#define RX_SGE_NUM_PAGES	DIV_ROUND_UP(RX_SGE_COUNT, RX_DESC_PAGE)
#define RX_SGE_SIZE		(RX_SGE_NUM_PAGES * PLUGIN_RING_UNIT_SIZE)

/* BDs */
#define RX_DESC_PAGE            (PLUGIN_RING_UNIT_SIZE / sizeof(struct eth_rx_bd))
#define RX_DESC_COUNT(x)        ((x) - RX_SGE_COUNT)
#define RX_DESC_NUM_PAGES(x)    DIV_ROUND_UP(RX_DESC_COUNT((x)), RX_DESC_PAGE)
#define RX_DESC_SIZE(x)         (RX_DESC_NUM_PAGES((x)) * PLUGIN_RING_UNIT_SIZE)

/* RCQ */
#define RX_RCQ_PAGE		(PLUGIN_RING_UNIT_SIZE / sizeof(union eth_rx_cqe))
#define RX_RCQ_COUNT(x)         (x)
#define RX_RCQ_NUM_PAGES(x)	DIV_ROUND_UP(RX_RCQ_COUNT((x)), RX_RCQ_PAGE)
#define RX_RCQ_SIZE(x)		(RX_RCQ_NUM_PAGES((x)) * PLUGIN_RING_UNIT_SIZE)


/* offsets */
#define TX_DESC_OFFSET(addr,x)  (addr)
#define SB_OFFSET(addr,x)    	(TX_DESC_OFFSET((addr),(x))+TX_DESC_SIZE((x)))
#define RX_UID_OFFSET(addr,x)   (SB_OFFSET((addr),(x))+ sizeof(struct host_hc_status_block_e2))
#define RX_DESC_OFFSET(addr,x)  (addr)
#define RX_RCQ_OFFSET(addr,x)   (RX_DESC_OFFSET((addr),(x))+ RX_DESC_SIZE((x)))
#define RX_SGE_OFFSET(addr,x)   (RX_RCQ_OFFSET((addr),(x))+ RX_RCQ_SIZE((x)))


/* SB protocol indices - share with bnx2x.h TODO */
#define PLUGIN_SB_ETH_TX_CQ_INDEX 	5
#define PLUGIN_SB_ETH_RX_CQ_INDEX	1
#define PLUGIN_RX_ALIGN_SHIFT		((L1_CACHE_SHIFT < 8) ? L1_CACHE_SHIFT : 8)
#define PLUGIN_RX_ALIGN			(1 << PLUGIN_RX_ALIGN_SHIFT)



struct bnx2xpi_ring_params {
	u64 base;
	u32 nr_desc;
	u32 byte_len;
};

struct bnx2xpi_txrx_params {
	struct bnx2xpi_ring_params rx;
	struct bnx2xpi_ring_params tx;
};

static inline u64 bnx2xpi_tx_decr_offset(struct bnx2xpi_txrx_params *p)
{
	return (TX_DESC_OFFSET(p->tx.base, p->tx.nr_desc));
}

static inline u64 bnx2xpi_rx_decr_offset(struct bnx2xpi_txrx_params *p)
{
	return (RX_DESC_OFFSET(p->rx.base, p->rx.nr_desc));
}

static inline u64 bnx2xpi_rcq_offset(struct bnx2xpi_txrx_params *p)
{
	return (RX_RCQ_OFFSET(p->rx.base, p->rx.nr_desc));
}

static inline u64 bnx2xpi_rcq_np_offset(struct bnx2xpi_txrx_params *p)
{
	u8 np = RX_RCQ_NUM_PAGES(p->rx.nr_desc) == 1  ? 0 : 1;
	return (bnx2xpi_rcq_offset(p) + np * PLUGIN_RING_UNIT_SIZE);
}

static inline u64 bnx2xpi_sge_offset(struct bnx2xpi_txrx_params *p)
{
	return (RX_SGE_OFFSET(p->rx.base, p->rx.nr_desc));
}

/* plugin statistics on shared memory */

#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_begin.h"
#endif
struct bnx2x_plugin_rxq_stats {
	u64 lro_bytes;
	u64 lro_pkts;
	u32 rx_err_discard_pkt;
	u32 hw_csum_err;
};
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_end.h"
#endif

#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_begin.h"
#endif
struct bnx2x_plugin_txq_stats {
	u64 tso_bytes;
	u64 tso_pkts;
};
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_end.h"
#endif

/* VF statistics on shared memory */
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_begin.h"
#endif
struct bnx2x_vf_plugin_stats {
	struct bnx2x_plugin_rxq_stats rxq_stats[BNX2X_PLUGIN_MAX_RX_QUEUES];
	struct bnx2x_plugin_txq_stats txq_stats[BNX2X_PLUGIN_MAX_TX_QUEUES];
};
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_end.h"
#endif

/* device information that is passed form the PF driver */
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_begin.h"
#endif
struct bnx2x_plugin_device_info {
	struct hw_sb_info sb_info[PLUGIN_MAX_SB];
	u8 hw_qid[PLUGIN_MAX_QUEUES];
	u32 chip_id;
	u8  pci_func;
	u8  fp_flags;
#define BNX2X_PLUGIN_DISABLE_TPA        0x1
#define BNX2X_PLUGIN_SHARED_SB_IDX      0x2
	u16 pad;	/* pad to qword*/
};
#ifdef BNX2X_PT_PLUGIN
#include "vmware_pack_end.h"
#endif

#endif /* BNX2X_PASSTHRU */


#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
/***********************************************
 *
 * HW VF-PF channel definitions
 *
 * A.K.A VF-PF mailbox
 *
 **/
#define VFPF_QUEUE_FLG_TPA   		0x0001
#define VFPF_QUEUE_FLG_CACHE_ALIGN   	0x0002
#define VFPF_QUEUE_FLG_STATS		0x0004
#define VFPF_QUEUE_FLG_OV    		0x0008
#define VFPF_QUEUE_FLG_VLAN  		0x0010
#define VFPF_QUEUE_FLG_COS   		0x0020
#define VFPF_QUEUE_FLG_HC		0x0040
#define VFPF_QUEUE_FLG_DHC		0x0080

#define VFPF_QUEUE_DROP_IP_CS_ERR	(1 << 0)
#define VFPF_QUEUE_DROP_TCP_CS_ERR	(1 << 1)
#define VFPF_QUEUE_DROP_TTL0		(1 << 2)
#define VFPF_QUEUE_DROP_UDP_CS_ERR	(1 << 3)

#define VFPF_RX_MASK_ACCEPT_NONE		0x00000000
#define VFPF_RX_MASK_ACCEPT_MATCHED_UNICAST	0x00000001
#define VFPF_RX_MASK_ACCEPT_MATCHED_MULTICAST	0x00000002
#define VFPF_RX_MASK_ACCEPT_ALL_UNICAST		0x00000004
#define VFPF_RX_MASK_ACCEPT_ALL_MULTICAST	0x00000008
#define VFPF_RX_MASK_ACCEPT_BROADCAST		0x00000010
/* TODO: #define VFPF_RX_MASK_ACCEPT_ANY_VLAN	0x00000020 */

enum {
	PFVF_STATUS_WAITING = 0,
	PFVF_STATUS_SUCCESS,
	PFVF_STATUS_FAILURE,
	PFVF_STATUS_NOT_SUPPORTED,
	PFVF_STATUS_VERSION_MISMATCH,
	PFVF_STATUS_NO_RESOURCE
};


/*  Headers */
struct vf_pf_msg_hdr {
	u16 opcode;

#define PFVF_IF_VERSION     	   1
	u8  if_ver;
	u8  opcode_ver;
	u32 resp_msg_offset;
};

struct pf_vf_msg_hdr {
	u8 status;
	u8 opcode_ver;
	u16 opcode;
};

/* simple response */
struct pf_vf_msg_resp {
	struct pf_vf_msg_hdr    hdr;
};


/* Acquire */
struct vf_pf_msg_acquire {
	struct vf_pf_msg_hdr hdr;

	struct vf_pf_vfdev_info {
		/* the following fields are for debug purposes */
		u8  vf_id;      	/* ME register value */
		u8  vf_os;      	/* e.g. Linux, W2K8 */
		u32 vf_driver_version;  /* e.g. 6.0.12 */
	} vfdev_info;

	struct vf_pf_resc_request resc_request;
};


/* simple operation request on queue */
struct vf_pf_msg_q_op {
	struct vf_pf_msg_hdr    hdr;
	u8      		vf_qid;
	u8			padding[3];
};

struct pf_vf_msg_acquire_resp {
	struct pf_vf_msg_hdr hdr;

	struct pf_vf_pfdev_info
	{
		u32 chip_num;

		u32 pf_cap;
		#define PFVF_CAP_RSS        0x00000001
		#define PFVF_CAP_DHC        0x00000002
		#define PFVF_CAP_TPA        0x00000004

		u16 db_size;
		u8  indices_per_sb;
		u8  padding;
	} pfdev_info;

	struct pf_vf_resc
	{
		/*
		 * in case of status NO_RESOURCE in message hdr, pf will fill
		 * this struct with suggested amount of resources for next
		 * acquire request
		 */

		#define PFVF_MAX_QUEUES_PER_VF         4
		#define PFVF_MAX_SBS_PER_VF            4
		struct hw_sb_info hw_sbs[PFVF_MAX_SBS_PER_VF];
		u8	hw_qid[PFVF_MAX_QUEUES_PER_VF];
		u8	num_rxqs;
		u8	num_txqs;
		u8	num_sbs;
		u8	num_mac_filters;
		u8	num_vlan_filters;
		u8	num_mc_filters;
		u8 	padding[2];
	/* TODO: stats resc? cid for the ramrod? stats_id? spq prod id? */
	} resc;
};

/* Init VF */
struct vf_pf_msg_init_vf {
	struct vf_pf_msg_hdr hdr;

	u64 sb_addr[PFVF_MAX_SBS_PER_VF]; /* vf_sb based */
	u64 spq_addr;
	u64 stats_addr;
};

/* Setup Queue */
struct vf_pf_msg_setup_q {
	struct vf_pf_msg_hdr hdr;
	u8 vf_qid;			/* index in hw_qid[] */

	u8 param_valid;
	#define VFPF_RXQ_VALID		0x01
	#define VFPF_TXQ_VALID		0x02

	u16 padding;

	struct vf_pf_rxq_params {
		/* physical addresses */
		u64 rcq_addr;
		u64 rcq_np_addr;
		u64 rxq_addr;
		u64 sge_addr;

		/* sb + hc info */
		u8  vf_sb;		/* index in hw_sbs[] */
		u8  sb_index;           /* Index in the SB */
		u16 hc_rate;		/* desired interrupts per sec. */
					/* valid iff VFPF_QUEUE_FLG_HC */
		/* rx buffer info */
		u16 mtu;
		u16 buf_sz;
		u16 flags;              /* VFPF_QUEUE_FLG_X flags */
		u16 stat_id;		/* valid iff VFPF_QUEUE_FLG_STATS */

		/* valid iff VFPF_QUEUE_FLG_TPA */
		u16 sge_buf_sz;
		u16 tpa_agg_sz;
		u8 max_sge_pkt;

		u8 drop_flags;		/* VFPF_QUEUE_DROP_X, for Linux all should
					 * be turned off, see setup_rx_queue()
					 * for reference
					 */

		u8 cache_line_log;	/* VFPF_QUEUE_FLG_CACHE_ALIGN
					 * see init_rx_queue()
					 */
		u8 padding;
	} rxq;

	struct vf_pf_txq_params {
		/* physical addresses */
		u64 txq_addr;

		/* sb + hc info */
		u8  vf_sb;		/* index in hw_sbs[] */
		u8  sb_index;		/* Index in the SB */
		u16 hc_rate;		/* desired interrupts per sec. */
					/* valid iff VFPF_QUEUE_FLG_HC */
		u32 flags;		/* VFPF_QUEUE_FLG_X flags */
		u16 stat_id;		/* valid iff VFPF_QUEUE_FLG_STATS */
		u8  traffic_type;	/* see in setup_context() */
		u8  padding;
	} txq;
};


/* Set Queue Filters */
struct vf_pf_q_mac_vlan_filter {
	u32 flags;
	#define VFPF_Q_FILTER_DEST_MAC_PRESENT 	0x01
	#define VFPF_Q_FILTER_VLAN_TAG_PRESENT	0x02

	u8  dest_mac[6];
	u16 vlan_tag;
};

struct vf_pf_msg_set_q_filters {
	struct vf_pf_msg_hdr hdr;

	u32 flags;
	#define VFPF_SET_Q_FILTERS_MAC_VLAN_CHANGED 	0x01
	#define VFPF_SET_Q_FILTERS_MULTICAST_CHANGED	0x02
	#define VFPF_SET_Q_FILTERS_RX_MASK_CHANGED  	0x04

	u8 vf_qid;			/* index in hw_qid[] */
	u8 n_mac_vlan_filters;
	u8 n_multicast;
	u8 padding;

	#define PFVF_MAX_MAC_FILTERS			16
	#define PFVF_MAX_VLAN_FILTERS       		16
	#define PFVF_MAX_FILTERS 			PFVF_MAX_MAC_FILTERS +\
							PFVF_MAX_VLAN_FILTERS
	struct vf_pf_q_mac_vlan_filter filters[PFVF_MAX_FILTERS];

	#define PFVF_MAX_MULTICAST_PER_VF   		32
	u8  multicast[PFVF_MAX_MULTICAST_PER_VF][6];

	u32 rx_mask;	/* see mask constants at the top of the file */
};


/* close VF (disable VF) */
struct vf_pf_msg_close_vf {
	struct vf_pf_msg_hdr	hdr;
	u16			vf_id;  /* for debug */
	u16			padding;
};

/* rlease the VF's acquired resources */
struct vf_pf_msg_release_vf {
	struct vf_pf_msg_hdr	hdr;
	u16			vf_id;  /* for debug */
	u16			padding;
};


union vf_pf_msg {
	struct vf_pf_msg_hdr		hdr;
	struct vf_pf_msg_acquire	acquire;
	struct vf_pf_msg_init_vf	init_vf;
	struct vf_pf_msg_close_vf	close_vf;
	struct vf_pf_msg_q_op		q_op;
	struct vf_pf_msg_setup_q	setup_q;
	struct vf_pf_msg_set_q_filters	set_q_filters;
	struct vf_pf_msg_release_vf	release_vf;
};

union pf_vf_msg {
	struct pf_vf_msg_resp		resp;
	struct pf_vf_msg_acquire_resp	acquire_resp;
};

typedef struct {
	u32 req_sz;
	u32 resp_sz;
} msg_sz_t;

#define PFVF_OP_VER_MAX(op_arry)  (sizeof(op_arry)/sizeof(*op_arry) - 1)

static const msg_sz_t acquire_req_sz[] = {
	/* sizeof(vf_pf_msg_acquire) - offsetof(struct vf_pf_msg_acquire, fieldX), */
	{sizeof(struct vf_pf_msg_acquire),
	sizeof(struct pf_vf_msg_acquire_resp)}
};
#define PFVF_ACQUIRE_VER  PFVF_OP_VER_MAX(acquire_req_sz)

static const msg_sz_t init_vf_req_sz[] = {
	{sizeof(struct vf_pf_msg_init_vf), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_INIT_VF_VER  PFVF_OP_VER_MAX(init_vf_req_sz)

static const msg_sz_t setup_q_req_sz[] = {
	{sizeof(struct vf_pf_msg_setup_q), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_SETUP_Q_VER  PFVF_OP_VER_MAX(setup_q_req_sz)

static const msg_sz_t set_q_filters_req_sz[] = {
	{sizeof(struct vf_pf_msg_set_q_filters), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_SET_Q_FILTERS_VER  PFVF_OP_VER_MAX(set_q_filters_req_sz)

static const msg_sz_t activate_q_req_sz[] = {
	{sizeof(struct vf_pf_msg_q_op), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_ACTIVATE_Q_VER  PFVF_OP_VER_MAX(activate_q_req_sz)

static const msg_sz_t deactivate_q_req_sz[] = {
	{sizeof(struct vf_pf_msg_q_op), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_DEACTIVATE_Q_VER  PFVF_OP_VER_MAX(deactivate_q_req_sz)

static const msg_sz_t teardown_q_req_sz[] = {
	{sizeof(struct vf_pf_msg_q_op), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_TEARDOWN_Q_VER  PFVF_OP_VER_MAX(teardown_q_req_sz)

static const msg_sz_t close_vf_req_sz[] = {
	{sizeof(struct vf_pf_msg_close_vf), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_CLOSE_VF_VER  PFVF_OP_VER_MAX(close_vf_req_sz)

static const msg_sz_t release_vf_req_sz[] = {
	{sizeof(struct vf_pf_msg_release_vf), sizeof(struct pf_vf_msg_resp)}
};
#define PFVF_RELEASE_VF_VER  PFVF_OP_VER_MAX(release_vf_req_sz)

enum {
	PFVF_OP_ACQUIRE = 0,
	PFVF_OP_INIT_VF,
	PFVF_OP_SETUP_Q,
	PFVF_OP_SET_Q_FILTERS,
	PFVF_OP_ACTIVATE_Q,
	PFVF_OP_DEACTIVATE_Q,
	PFVF_OP_TEARDOWN_Q,
	PFVF_OP_CLOSE_VF,
	PFVF_OP_RELEASE_VF,
	PFVF_OP_MAX
};


/** To get size of message of the type X(request or response)
 *  for the op_code Y of the version Z one should use
 *
 *  op_code_req_sz[Y][Z].req_sz/resp_sz
 ******************************************************************/
/* const msg_sz_t* op_codes_req_sz[] = {
	(const msg_sz_t*)acquire_req_sz,
	(const msg_sz_t*)init_vf_req_sz,
	(const msg_sz_t*)setup_q_req_sz,
	(const msg_sz_t*)set_q_filters_req_sz,
	(const msg_sz_t*)activate_q_req_sz,
	(const msg_sz_t*)deactivate_q_req_sz,
	(const msg_sz_t*)teardown_q_req_sz,
	(const msg_sz_t*)close_vf_req_sz,
	(const msg_sz_t*)release_vf_req_sz
}; */
#endif /* ! __VMKLNX__ */
#endif /* VF_PF_IF_H */
