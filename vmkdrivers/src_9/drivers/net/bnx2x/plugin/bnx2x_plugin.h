/* bnx2x_plugin.h: Broadcom bnx2x NPA plugin
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
 * Written by: Shmulik Ravid
 *
 */
#ifndef _BNX2X_PLUGIN_H
#define _BNX2X_PLUGIN_H

#define PLUGIN_VERSION	"6.1.3"

#ifndef DIV_ROUND_UP
#define DIV_ROUND_UP(n,d) (((n) + (d) - 1) / (d))
#endif

#define PAGES_PER_SGE_SHIFT	0
#define PAGES_PER_SGE		(1 << PAGES_PER_SGE_SHIFT)
#define SGE_PAGE_SIZE		PAGE_SIZE
#define SGE_PAGE_SHIFT		PAGE_SHIFT
#define SGE_PAGE_ALIGN(addr)    PAGE_ALIGN((typeof(PAGE_SIZE))(addr))

#define BNX2X_PLUGIN_PRS_FLAG_OVERETH_IPV4(flags) 	\
	(((le16_to_cpu(flags) & 			\
	PARSING_FLAGS_OVER_ETHERNET_PROTOCOL) >> 	\
	PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT) 	\
	== PRS_FLAG_OVERETH_IPV4)

#define BNX2X_PLUGIN_PRS_FLAG_OVERIP_TCP(flags) 	\
	(((le16_to_cpu(flags) & 			\
	PARSING_FLAGS_OVER_IP_PROTOCOL) >> 		\
	PARSING_FLAGS_OVER_IP_PROTOCOL_SHIFT) 		\
	 == PRS_FLAG_OVERIP_TCP)

#define BNX2X_PLUGIN_TPA_STOP_VALID(cqe) \
	BNX2X_PLUGIN_PRS_FLAG_OVERETH_IPV4(cqe->pars_flags.flags) && \
	BNX2X_PLUGIN_PRS_FLAG_OVERIP_TCP(cqe->pars_flags.flags)


#define NEXT_RING_POS(x, page_max, skip)       		\
    if ((x).page_off < ((page_max)-1)) {                \
	(x).page_off++;                                 \
	(x).pos++;                                      \
    } else {                                            \
	(x).page_off = 0;                               \
	(x).pos += (skip);                              \
	(x).page_cnt++;					\
	if ((x).page_cnt == (x).num_pages)		\
	    (x).page_cnt = 0;                           \
    }                                                   \

/*
 *  RX SGE ring related macros
 *  --------------------------
 */
#define RX_SGE_PAGE_MAX		(RX_SGE_PAGE - 2)
#define RX_SGE_PAGE_MASK	(RX_SGE_PAGE - 1)
#define RX_SGE_RSRV             (RX_SGE_PAGE-RX_SGE_PAGE_MAX)

#define RX_SGE_RING_CNT 	(RX_SGE_NUM_PAGES * RX_SGE_PAGE)
#define RX_SGE(x)		((x) & (RX_SGE_RING_CNT - 1))
#define RX_SGE_CNT_MASK 	(RX_SGE_COUNT -1)
#define RX_SGE_BUF(x)		((x) & RX_SGE_CNT_MASK)

#define NEXT_SGE_IDX(x)         ((((x) & RX_SGE_PAGE_MASK) == \
				(RX_SGE_PAGE_MAX - 1)) ? (x) + 3 : (x) + 1)


/* SGE producer mask related macros */
/* Number of bits in one sge_mask array element */
#define RX_SGE_MASK_ELEM_SZ     32
#define RX_SGE_MASK_ELEM_SHIFT  5
#define RX_SGE_MASK_ELEM_MASK	((u64)RX_SGE_MASK_ELEM_SZ - 1)
#define RX_SGE_MASK_ELEM_ONE_MASK	((u32)(~0))

/* Number of u32 elements in SGE mask array */
#define RX_SGE_MASK_LEN		(RX_SGE_RING_CNT / RX_SGE_MASK_ELEM_SZ)
#define RX_SGE_MASK_LEN_MASK	(RX_SGE_MASK_LEN - 1)
#define NEXT_SGE_MASK_ELEM(el)	(((el) + 1) & RX_SGE_MASK_LEN_MASK)

#define __SGE_MASK_SET_BIT(el, bit) \
	do { \
		el = ((el) | ((u32)0x1 << (bit))); \
	} while (0)

#define __SGE_MASK_CLEAR_BIT(el, bit) \
	do { \
		el = ((el) & (~((u32)0x1 << (bit)))); \
	} while (0)

#define SGE_MASK_SET_BIT(fp, idx) \
	__SGE_MASK_SET_BIT(fp->sge_mask[(idx) >> RX_SGE_MASK_ELEM_SHIFT], \
			   ((idx) & RX_SGE_MASK_ELEM_MASK))

#define SGE_MASK_CLEAR_BIT(fp, idx) \
	__SGE_MASK_CLEAR_BIT(fp->sge_mask[(idx) >> RX_SGE_MASK_ELEM_SHIFT], \
			     ((idx) & RX_SGE_MASK_ELEM_MASK))

/*
 *  RX ring related macros
 *  ----------------------
 */

/* The RX BD ring is special, each bd is 8 bytes but the last one is 16 */
#define RX_DESC_PAGE_MAX	(RX_DESC_PAGE - 2)
#define RX_DESC_PAGE_MASK	(RX_DESC_PAGE - 1)
#define RX_DESC_RSRV        	(RX_DESC_PAGE-RX_DESC_PAGE_MAX)
#define RX_MAX_AVAIL(x)		(RX_DESC_PAGE_MAX * RX_DESC_NUM_PAGES((x))-2) /* -2 ? */
#define NEXT_RX_IDX(x)		((((x) & RX_DESC_PAGE_MASK) == \
				(RX_DESC_PAGE_MAX - 1)) ? (x) + 3 : (x) + 1)
#define NEXT_RX_POS(x)      	NEXT_RING_POS((x),RX_DESC_PAGE_MAX,3)
#define RX_BD(x)		((x).page_cnt*RX_DESC_PAGE+(x).page_off)

/*
 *  RCQ ring related macros
 *  -----------------------
 */

/*
 * As long as CQE is 4 times bigger than BD entry we have to allocate
 * 4 times more pages for CQ ring in order to keep it balanced with
 * BD ring
 */
#define RX_RCQ_PAGE_MAX	    	(RX_RCQ_PAGE - 1)
#define RX_RCQ_PAGE_MASK	(RX_RCQ_PAGE - 1)
#define RX_RCQ_MAX_AVAIL(x)	(RX_RCQ_PAGE_MAX * RX_RCQ_NUM_PAGES((x))-2) /* check -2 TODO */

#define NEXT_RCQ_IDX(x)		((((x) & RX_RCQ_PAGE_MASK) == \
				(RX_RCQ_PAGE_MAX - 1)) ? (x) + 2 : (x) + 1)
#define NEXT_RCQ_POS(x)     	NEXT_RING_POS((x),RX_RCQ_PAGE_MAX,2)
#define RCQ_BD(x)		((x).page_cnt*RX_RCQ_PAGE+(x).page_off)

#define CQE_TYPE(cqe_fp_flags)	((cqe_fp_flags) & ETH_FAST_PATH_RX_CQE_TYPE)

#define TPA_TYPE_START		ETH_FAST_PATH_RX_CQE_START_FLG
#define TPA_TYPE_END		ETH_FAST_PATH_RX_CQE_END_FLG
#define TPA_TYPE(cqe_fp_flags)	((cqe_fp_flags) & \
				(TPA_TYPE_START|TPA_TYPE_END))


/*
 *  TX ring related macros
 *  ----------------------
 */
#define TX_DESC_PAGE_MAX    	(TX_DESC_PAGE - 1)
#define TX_DESC_PAGE_MASK	(TX_DESC_PAGE - 1)
#define TX_DESC_RSRV        	(TX_DESC_PAGE-TX_DESC_PAGE_MAX)
#define TX_MAX_AVAIL(x)		(TX_DESC_PAGE_MAX*TX_DESC_NUM_PAGES((x))-2) /* check -2 TODO */
#define TX_BD_POFF(x)		((x) & TX_DESC_PAGE_MASK)
#define NEXT_TX_IDX(x)		((((x) & TX_DESC_PAGE_MASK) == \
				(TX_DESC_PAGE_MAX - 1)) ? (x) + 2 : (x) + 1)
#define NEXT_TX_POS(x)      	NEXT_RING_POS((x),TX_DESC_PAGE_MAX,2)
#define TX_BD(x)		((x).page_cnt*TX_DESC_PAGE+(x).page_off)

#define MAX_FETCH_BD		13	/* HW max BDs per packet */
#define MAX_TPA_IND_PKTS	64	/* max indicated TPA bds */

#define BNX2X_PLUGIN_TX_OK	0
#define BNX2X_PLUGIN_TX_BUSY	1

/* This is needed for determining of last_max */
#define SUB_S16(a, b)		(s16)((s16)(a) - (s16)(b))

#define U64_LO(x)		(u32)(((u64)(x)) & 0xffffffff)
#define U64_HI(x)		(u32)(((u64)(x)) >> 32)
#define HILO_U64(hi, lo)	((((u64)(hi)) << 32) + (lo))

#define BNX2X_PLUGIN_MAPPED_LEN	128 /* not big enough for ipv6 with ext headers TODO */

#define BNX2X_PLUGIN_MAX_RECV_SG_ARRAY 9 /* 1 + 8 */

#define BNX2X_PLUGIN_TX_SB_INDEX \
   (&fp->status_blk->sb.index_values[HC_INDEX_C_ETH_TX_CQ_CONS])

#define BNX2X_PLUGIN_RCQ_SB_INDEX \
    (&fp->status_blk->sb.index_values[HC_INDEX_U_ETH_RX_CQ_CONS])
	
/* ring pointers */
struct plugin_rpos {
    u16 pos;
    u16 page_off;
    u16 page_cnt;
    u16 num_pages;
};

static inline void bnx2x_plugin_bzero(void *memory, size_t length)
{
	size_t i;
	for (i = 0; i < length; ++i) {
		((uint8 *)memory)[i] = 0;
	}
}

static inline int is_multicast_ether_addr(const u8 *addr)
{
	return (0x01 & addr[0]);
}

static inline int is_broadcast_ether_addr(const u8 *addr)
{
	return ((addr[0] & addr[1] & addr[2] & addr[3] & addr[4] & addr[5])
		== 0xff);
}

static inline
u8 *bnx2x_plugin_network_header(const Plugin_SendInfo *send_info,
				const Plugin_SgList *sgl)
{
	if (BNX2X_PLUGIN_MAPPED_LEN < send_info->ipHeaderOffset)
		return NULL;
	return (sgl->firstSgVA + send_info->ipHeaderOffset);
}

static inline
u8 *bnx2x_plugin_transport_header(const Plugin_SendInfo *send_info,
				  const Plugin_SgList *sgl)
{
	if (BNX2X_PLUGIN_MAPPED_LEN < send_info->l4HeaderOffset)
		return NULL;
	return (sgl->firstSgVA + send_info->l4HeaderOffset);
}

static inline
struct ethhdr *bnx2x_plugin_eth_hdr(const Plugin_SendInfo *send_info,
				    const Plugin_SgList *sgl)
{
	return (struct ethhdr *) sgl->firstSgVA;
}

static inline u16 bnx2x_plugin_eth_type(const Plugin_SendInfo *send_info,
					const Plugin_SgList *sgl)
{
	return ntohs(bnx2x_plugin_eth_hdr(send_info, sgl)->h_proto);
}

static inline
unsigned char *bnx2x_plugin_eth_src(const Plugin_SendInfo *send_info,
				    const Plugin_SgList *sgl)
{
	return bnx2x_plugin_eth_hdr(send_info, sgl)->h_source;
}

static inline
unsigned char *bnx2x_plugin_eth_dst(const Plugin_SendInfo *send_info,
				    const Plugin_SgList *sgl)
{
	return bnx2x_plugin_eth_hdr(send_info, sgl)->h_dest;
}

static inline
struct iphdr *bnx2x_plugin_ip_hdr(const Plugin_SendInfo *send_info,
				  const Plugin_SgList *sgl)
{
	return (struct iphdr *) bnx2x_plugin_network_header(send_info, sgl);
}

static inline
struct ipv6hdr *bnx2x_plugin_ipv6_hdr(const Plugin_SendInfo *send_info,
				      const Plugin_SgList *sgl)
{
	return (struct ipv6hdr *) bnx2x_plugin_network_header(send_info, sgl);
}


/*
 * RX frame structures
 * -------------------
 */

/* TPA aggregation queue context */
struct bnx2x_plugin_tpa_aqc {
	u16 bufid;		/* local bufid */
	u16 state;
#define BNX2X_PLUGIN_TPA_START		1
#define BNX2X_PLUGIN_TPA_STOP		2
};

/* doorbell */
union db_prod {
	struct doorbell_set_prod data;
	u32 raw;
};

struct bnx2x_plugin_queue_dma_info {
	u8 *va;
	dma_addr_t pa;
	u16 num_desc;
};

struct bnx2x_plugin_rx_queue {
	struct host_hc_status_block_e2 *status_blk;
	struct eth_rx_bd	*rx_desc_ring;
#ifdef __CHIPSIM_NATIVE_HSI__
	struct eth_rx_cqe *rx_comp_ring;
#else
	union eth_rx_cqe *rx_comp_ring;
#endif
	struct eth_rx_sge *rx_sge_ring;		/* SGE ring */
	u16 *rx_uid_ring;               	/* buffer ids ring */
	u32 sge_mask[RX_SGE_MASK_LEN]; 		/* SGE bit mask */

	u8 idx;		                	/* number in fp array */
	u16 q_id;		                /* queue zone id */
	u16 sb_id;		                /* status block number in HW */
	u16 num_desc;
	u32 sw_sb_idx;                  	/* reflected SB index */
	struct plugin_rpos rx_bd_prod;
	struct plugin_rpos rx_bd_cons;
	struct plugin_rpos rx_uid_cons;
	struct plugin_rpos rx_comp_cons;
	u16 rx_comp_prod;
	u16 rx_sge_prod;
	u16 rx_sge_cons;
	u16 last_max_sge;               /* last maximal completed SGE */
	__le16 *rx_cons_sb;

	/* indicated bds that can be reallocated */
	u16 tpa_ind[MAX_TPA_IND_PKTS];
	u16 tpa_max_ind;

	/* LRO pulled bds one for each LRO aggregation queue */
	struct bnx2x_plugin_tpa_aqc tpa_pool[ETH_MAX_AGG_QUEUES];

	/* queue stats on shared memory */
	struct bnx2x_plugin_rxq_stats *rx_stats;
};


struct bnx2x_plugin_tx_queue {
	struct host_hc_status_block_e2 *status_blk;
#ifdef __CHIPSIM_NATIVE_HSI__
	struct eth_tx_bd_types *tx_desc_ring;
#else
	union eth_tx_bd_types *tx_desc_ring;
#endif
	u8 idx;	        	/* number in fp array */
	u16 sb_id;	        /* status block number in HW */
	u16 num_desc;
	union db_prod tx_db;
	u16 tx_pkt_prod;
	u16 tx_pkt_cons;
	struct plugin_rpos tx_bd_prod;
	struct plugin_rpos tx_bd_cons;
	__le16 *tx_cons_sb;
	u32 sw_sb_idx;      	/* reflected SB index */
	u32 driver_xoff;    	/* statistics */

	/* queue stats on shared memory */
	struct bnx2x_plugin_txq_stats *tx_stats;
};

/* private state structure */
struct bnx2x_priv {
	u32 chip_id;
	u8 pci_func;
	u8 fp_flags;
#define BP_TPA_DISABLED(bp) ((bp)->fp_flags & BNX2X_PLUGIN_DISABLE_TPA)
#define BP_SHARED_SB(bp)    ((bp)->fp_flags & BNX2X_PLUGIN_SHARED_SB_IDX)
#define BP_PF_QUEUE(bp)     ((bp)->fp_flags & BNX2X_PLUGIN_DBG_PF_QUEUE)

	/* Fast-path queues*/
	struct bnx2x_plugin_rx_queue rxfp[BNX2X_PLUGIN_MAX_RX_QUEUES];
	struct bnx2x_plugin_tx_queue txfp[BNX2X_PLUGIN_MAX_TX_QUEUES];
};

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_bd_global_bufid
*
* calculates the global buffer unique id from the local buffer id
* in the BD ring and the queue index
*
* Result:
*    the global buffer id
*
*----------------------------------------------------------------------------
*/
static inline
u32 bnx2x_plugin_bd_global_id(u16 local_bufid, u16 num_desc, u16 idx)
{
	return (num_desc + RX_SGE_COUNT) * idx + local_bufid;
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_sge_global_bufid
*
* calculates the global buffer unique id from the local buffer id
* in the SGE ring and the queue index
*
* Result:
*    the global buffer id
*
*----------------------------------------------------------------------------
*/
static inline
u32 bnx2x_plugin_sge_global_id(u16 local_bufid, u16 num_desc, u16 idx)
{
	return (num_desc + RX_SGE_COUNT) * idx + num_desc + local_bufid;
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_bd_local_bufid
*
* Calculates the local buffer id for the BD ring buffers from the global
* id and the queue index
*
* Result:
*    the local buffer id
*
*----------------------------------------------------------------------------
*/
static inline
u16 bnx2x_plugin_bd_local_id(u32 global_bufid, u16 num_desc, u16 idx)
{
	return (u16)(global_bufid - (num_desc + RX_SGE_COUNT) * idx);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_sge_local_bufid
*
* Calculates the local buffer id for the SGE ring buffers from the global
* id and the queue index
*
* Result:
*    the local buffer id
*
*----------------------------------------------------------------------------
*/
static inline
u16 bnx2x_plugin_sge_local_id(u32 global_bufid, u16 num_desc, u16 idx)
{
	return (u16)(global_bufid - (num_desc + RX_SGE_COUNT) * idx - num_desc);
}

/* Plugin API forward */
static uint32
bnx2x_plugin_sw_init(Plugin_State *ps);

static uint32
bnx2x_plugin_reinit_rx_ring(Plugin_State *ps, uint32 queue);

static uint32
bnx2x_plugin_reinit_tx_ring(Plugin_State *ps, uint32 queue);

static uint32
bnx2x_plugin_enable_interrupt(Plugin_State *ps, uint32 intrIdx);

static uint32
bnx2x_plugin_disable_interrupt(Plugin_State *ps, uint32 intrIdx);

static uint32
bnx2x_plugin_add_frame_tx_ring(Plugin_State *ps, uint32 queue,
			       const Plugin_SendInfo *info,
			       const Plugin_SgList *frame,
			       Bool lastPktHint);

static uint32
bnx2x_plugin_check_tx_ring(Plugin_State *ps, uint32 queue);

static uint32
bnx2x_plugin_check_rx_ring(Plugin_State *ps, uint32 queue, uint32 maxPkts);

static uint32
bnx2x_plugin_add_bufs_rx_ring(Plugin_State *ps, uint32 queue);

#ifdef __CHIPSIM_NPA__
void
bnx2x_plugin_ramrod_complete(Plugin_State *ps, struct bnx2x_priv *bp,
			     struct bnx2x_plugin_rx_queue *fp,
			     common_ramrod_eth_rx_cqe *cqe);
#endif


#endif /* bnx2x_plugin.h */
