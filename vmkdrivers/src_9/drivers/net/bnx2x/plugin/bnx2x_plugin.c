/* bnx2x_plugin.c: Broadcom bnx2x NPA plugin
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
#include "vm_basic_types.h"
#include "npa_plugin_api.h"

/*#define PLUGIN_DP*/
#define PLUGIN_ENFORCE_SECURITY


#include "bnx2x_plugin_defs.h"
#include "bnx2x_plugin_hw.h"
#include "bnx2x_plugin_hsi.h"

/* include definitions common to the plugin and the ESX host */
#define BNX2X_PASSTHRU
#define BNX2X_PT_PLUGIN
#include "../bnx2x_vfpf_if.h"

#include "bnx2x_plugin.h"

#ifdef __CHIPSIM_NPA__
    #include "chipsim_npa_compat.h"
#else
    #include "bnx2x_plugin_compat.h"
#endif

#define DP_RING_RAW(base, byte_len, iter) \
	do { \
		for((iter) = 0; (iter) < ((byte_len) >> 3); (iter)++) {   \
			DP(3, "%04d: %08x %08x\n", (iter),		  \
			   *((u32*)((char*)(base) + ((iter) << 3))),	  \
			   *((u32*)((char*)(base) + ((iter) << 3) + 4))); \
		} \
	} while (0) \



static inline void
bnx2x_plugin_dump_rings_raw(Plugin_State *ps, struct bnx2x_priv *bp,
			struct bnx2x_plugin_rx_queue *fp)
{
	int i;
	DP(0, "BD RING:\n");
	DP_RING_RAW(fp->rx_desc_ring,
		    fp->rx_bd_prod.num_pages * PLUGIN_RING_UNIT_SIZE, i);

	DP(0, "RCQ RING:\n");
	DP_RING_RAW(fp->rx_comp_ring,
		    fp->rx_comp_cons.num_pages * PLUGIN_RING_UNIT_SIZE, i);

	if (!BP_TPA_DISABLED(bp)) {
		DP(0, "SGE RING:\n");
		DP_RING_RAW(fp->rx_sge_ring, RX_SGE_SIZE, i);
	}
}

static inline void
bnx2x_plugin_dump_rpos(Plugin_State *ps, char* name, struct plugin_rpos* rpos)
{
	DP(5,"%s: pos %d, poff %d, pcnt %d, num_p %d\n",
	   name,rpos->pos,rpos->page_off, rpos->page_cnt,rpos->num_pages);
}

static inline void
bnx2x_plugin_dump_rx_bd(Plugin_State *ps,
			struct bnx2x_plugin_rx_queue *fp,
			struct eth_rx_bd *rx_bd,
			u16 bd_idx, u32 bufid, dma_addr_t mapping)
{
	DP(9, "rx-bd[%d] - bufid %d, uid %d, buf_addr(%x:%x @%p), "
	      "bd_addr %p, prod %d, prod-page-offset %d\n",
	   bd_idx, bufid, fp->rx_uid_ring[bd_idx],
	   rx_bd->addr_hi, rx_bd->addr_lo, mapping, rx_bd,
	   fp->rx_bd_prod.pos, fp->rx_bd_prod.page_off);
}

static inline void
bnx2x_plugin_dump_cqe(Plugin_State *ps, union eth_rx_cqe *cqe, u32 bufid)
{

	DP(8, "CQE type %x err %x status %x hash %x vlan %x "
	      "len %u pad %d bufid %d\n",
	   CQE_TYPE(cqe->fast_path_cqe.type_error_flags),
	   cqe->fast_path_cqe.type_error_flags,
	   cqe->fast_path_cqe.status_flags,
	   le32_to_cpu(cqe->fast_path_cqe.rss_hash_result),
	   le16_to_cpu(cqe->fast_path_cqe.vlan_tag),
	   le16_to_cpu(cqe->fast_path_cqe.pkt_len),
	   le16_to_cpu(cqe->fast_path_cqe.placement_offset),
	   bufid);
}


static inline void
bnx2x_plugin_dump_rxq_init(Plugin_State *ps,
			   struct bnx2x_plugin_rx_queue *fp,
			   struct bnx2x_plugin_queue_dma_info *rx_dma)
{
	DP(3, "RXQ[%d] - sb %p, uid %p\n",
	   fp->idx, fp->status_blk, fp->rx_uid_ring);
	DP(4, "RXQ[%d] - rx-{%p, %d, %x}\n",
	   fp->idx, fp->rx_desc_ring,
	   RX_DESC_NUM_PAGES(rx_dma->num_desc),
	   RX_DESC_OFFSET(rx_dma->pa, rx_dma->num_desc));
	DP(4, "RXQ[%d] - rcq-{%p, %d, %x}\n",
	   fp->idx, fp->rx_comp_ring,
	   RX_RCQ_NUM_PAGES(rx_dma->num_desc),
	   RX_RCQ_OFFSET(rx_dma->pa, rx_dma->num_desc));
	DP(4, "RXQ[%d] - sge-{%p, %d, %x)\n",
	   fp->idx, fp->rx_sge_ring,
	   RX_SGE_NUM_PAGES,
	   RX_SGE_OFFSET(rx_dma->pa, rx_dma->num_desc));

}

static inline void
bnx2x_plugin_dump_recv_frame(Plugin_State *ps,
			     Shell_RecvFrame *frame)
{
	u32 i;

	DP(6, "RxFrame[1]: ptr %p, sg_len %d, byte_len %d, bufid %d, "
	      "sge_len %d, offset %d\n",
	   frame,
	   frame->sgLength,
	   frame->byteLength,
	   frame->sg[0].ringOffset,
	   frame->sg[0].length,
	   frame->firstSgOffset);

	for (i = 1; i < frame->sgLength; i++)
		DP(3, "RxFrame[1-%d]: frag_len %d, bufid %d\n", i,
		   frame->sg[i].length,
		   frame->sg[i].ringOffset);


	DP(6, "RxFrame[2]: matched %d, vlan %d, vlan-tag %d, rss-func %d, "
	      "rss-type %d, rss-val %d\n",
	   frame->perfectFiltered,
	   frame->vlan,
	   frame->vlanTag,
	   frame->rssHashFunction,
	   frame->rssHashType,
	   frame->rssHashValue);

	DP(8, "RxFrame[3]: ipv4 %d, ipv6 %d, non-ip %d, tcp %d, udp %d, "
	      "ip-xsum %d, tcp-xsum %d, udp-xsum %d\n",
	   frame->ipv4,
	   frame->ipv6,
	   frame->nonIp,
	   frame->tcp,
	   frame->udp,
	   frame->ipXsum,
	   frame->tcpXsum,
	   frame->udpXsum);
}
static inline void
bnx2x_plugin_dump_entry_point(Plugin_State *ps, char* name, u64 addr)
{
	DP(3, "%s - (%x:%x)\n", name, U64_HI(addr), U64_LO(addr));
}
static inline void
bnx2x_plugin_dump_entry_pointes(Plugin_State *ps)
{
	bnx2x_plugin_dump_entry_point(ps, "swInit",
				      (u64)bnx2x_plugin_sw_init);
	bnx2x_plugin_dump_entry_point(ps, "reinitRxRing",
				      (u64)bnx2x_plugin_reinit_rx_ring);
	bnx2x_plugin_dump_entry_point(ps, "reinitTxRing",
				      (u64)bnx2x_plugin_reinit_tx_ring);
	bnx2x_plugin_dump_entry_point(ps, "enableInterrupt",
				      (u64)bnx2x_plugin_enable_interrupt);
	bnx2x_plugin_dump_entry_point(ps, "disableInterrupt",
				      (u64)bnx2x_plugin_disable_interrupt);
	bnx2x_plugin_dump_entry_point(ps, "addFrameToTxRing",
				      (u64)bnx2x_plugin_add_frame_tx_ring);
	bnx2x_plugin_dump_entry_point(ps, "checkTxRing",
				      (u64)bnx2x_plugin_check_tx_ring);
	bnx2x_plugin_dump_entry_point(ps, "checkRxRing",
				      (u64)bnx2x_plugin_check_rx_ring);
	bnx2x_plugin_dump_entry_point(ps, "addBuffersToRxRing",
				      (u64)bnx2x_plugin_add_bufs_rx_ring);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_update_rx_sb_idx
*
* Samples the rx status block index
*
* Result:
*    None
*
*----------------------------------------------------------------------------
*/
static inline void bnx2x_plugin_update_rx_sb_idx(struct bnx2x_plugin_rx_queue *fp)
{
    fp->sw_sb_idx = le16_to_cpu(fp->status_blk->sb.running_index[SB_SEG_U]);
}
/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_update_tx_sb_idx
*
* Samples the tx status block index
*
* Result:
*    None
*
*----------------------------------------------------------------------------
*/
static inline
void bnx2x_plugin_update_tx_sb_idx(struct bnx2x_plugin_tx_queue *fp)
{
    fp->sw_sb_idx = le16_to_cpu(fp->status_blk->sb.running_index[SB_SEG_C]);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_clear_sge_mask_next_elems
*
* Clears the next pointer bits on the sge_mask
*
* Result:
*    None
*
*----------------------------------------------------------------------------
*/
static inline
void bnx2x_plugin_clear_sge_mask_next_elems(struct bnx2x_plugin_rx_queue *fp)
{
	int i,j;
	for (i = 1; i <= RX_SGE_NUM_PAGES; i++) {
		int bit_idx = RX_SGE_PAGE * i;

		/* clear last 2 bits */
		for( j = 0; j < 2; j++) {
			bit_idx--;
			SGE_MASK_CLEAR_BIT(fp, bit_idx);
		}
	}
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_init_tx_fp
*
* Initialize a TX fast path queue
*
* Result:
*   None
*
* Side-effects:
*   None
*
*----------------------------------------------------------------------------
*/
static void
bnx2x_plugin_init_tx_fp(Plugin_State *ps,
			struct bnx2x_plugin_tx_queue *fp,
			struct bnx2x_plugin_queue_dma_info *tx_dma,
			struct hw_sb_info *sb_info, u32 idx)
{
	u16 i, num_pages;
	dma_addr_t mapping;
	struct bnx2x_vf_plugin_stats *vf_stats;

	fp->status_blk = (struct host_hc_status_block_e2*)
		SB_OFFSET(tx_dma->va, tx_dma->num_desc);

	fp->tx_desc_ring = (union eth_tx_bd_types*)
		TX_DESC_OFFSET(tx_dma->va, tx_dma->num_desc);

	fp->sb_id = sb_info->hw_sb_id;
	fp->idx = (u8)idx;

	/* doorbell */
	fp->tx_db.data.header.header = DOORBELL_HDR_DB_TYPE;
	fp->tx_db.data.zero_fill1 = 0;
	fp->tx_db.data.prod = 0;

	fp->num_desc = tx_dma->num_desc;
	fp->tx_cons_sb = BNX2X_PLUGIN_TX_SB_INDEX;

	/* set the next-pointers */
	num_pages = TX_DESC_NUM_PAGES(tx_dma->num_desc);
	mapping = TX_DESC_OFFSET(tx_dma->pa, tx_dma->num_desc);
	for (i = 1; i <= num_pages; i++) {
		struct eth_tx_next_bd *tx_next_bd =
			&fp->tx_desc_ring[TX_DESC_PAGE * i - 1].next_bd;

		tx_next_bd->addr_hi = cpu_to_le32(U64_HI(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		tx_next_bd->addr_lo = cpu_to_le32(U64_LO(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
	}
	fp->tx_bd_cons.num_pages = num_pages;
	fp->tx_bd_prod.num_pages = num_pages;

	/* set pointer to shared statistics */
	vf_stats = (struct bnx2x_vf_plugin_stats *)ps->shared;
	fp->tx_stats = &vf_stats->txq_stats[idx];

}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_init_rx_fp
*
* Initialize a RX fast path queue
*
* Result:
*   None
*
* Side-effects:
*   None
*
*----------------------------------------------------------------------------
*/
static void
bnx2x_plugin_init_rx_fp(Plugin_State *ps,
			struct bnx2x_priv* bp,
			struct bnx2x_plugin_rx_queue *fp,
			struct bnx2x_plugin_queue_dma_info *rx_dma,
			struct bnx2x_plugin_queue_dma_info *tx_dma,
			struct hw_sb_info *sb_info, u8 hw_qid, u32 idx)
{
	u16 i, num_pages;
	dma_addr_t mapping;
	struct bnx2x_vf_plugin_stats *vf_stats;

	fp->status_blk = (struct host_hc_status_block_e2*)
		SB_OFFSET(tx_dma->va, tx_dma->num_desc);

	/* just in case - consider removing */
	bnx2x_plugin_bzero((char*)fp->status_blk,
			   sizeof(struct host_hc_status_block_e2));

	fp->rx_desc_ring = (struct eth_rx_bd*)
		RX_DESC_OFFSET(rx_dma->va, rx_dma->num_desc);

	fp->rx_comp_ring = (union eth_rx_cqe*)
		RX_RCQ_OFFSET(rx_dma->va, rx_dma->num_desc);

	fp->rx_sge_ring =
		(struct eth_rx_sge*)RX_SGE_OFFSET(rx_dma->va, rx_dma->num_desc);

	fp->rx_uid_ring = (u16*)RX_UID_OFFSET(tx_dma->va, tx_dma->num_desc);

	fp->q_id = hw_qid;
	fp->sb_id = sb_info->hw_sb_id;
	fp->idx = (u8)idx;
	fp->rx_cons_sb = BNX2X_PLUGIN_RCQ_SB_INDEX;

	/* set the next-pointers */

	/* rx_desc */
	num_pages = RX_DESC_NUM_PAGES(rx_dma->num_desc);
	mapping = RX_DESC_OFFSET(rx_dma->pa, rx_dma->num_desc);

	bnx2x_plugin_bzero((char*)fp->rx_desc_ring,
			   num_pages * PLUGIN_RING_UNIT_SIZE);

	fp->num_desc = min(rx_dma->num_desc - RX_SGE_COUNT,
			   (u16)(num_pages * RX_DESC_PAGE_MAX));

	for (i = 1; i <= num_pages; i++) {
		struct eth_rx_bd *rx_bd;
		rx_bd = &fp->rx_desc_ring[RX_DESC_PAGE * i - 2];

		rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		DP(2,"rx-bd-np %x:%x\n", rx_bd->addr_hi, rx_bd->addr_lo);
	}
	fp->rx_bd_cons.num_pages = num_pages;
	fp->rx_bd_prod.num_pages = num_pages;
	fp->rx_uid_cons.num_pages = num_pages;

	/* rcq  */
	num_pages = RX_RCQ_NUM_PAGES(rx_dma->num_desc);
	mapping = RX_RCQ_OFFSET(rx_dma->pa, rx_dma->num_desc);

	bnx2x_plugin_bzero((char*)fp->rx_comp_ring,
			   num_pages * PLUGIN_RING_UNIT_SIZE);

	for (i = 1; i <= num_pages; i++) {
		struct eth_rx_cqe_next_page *nextpg;
		nextpg = (struct eth_rx_cqe_next_page *)
			&fp->rx_comp_ring[RX_RCQ_PAGE * i - 1];

		nextpg->addr_hi = cpu_to_le32(U64_HI(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		nextpg->addr_lo = cpu_to_le32(U64_LO(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		DP(2,"rcq-bd-np %x:%x\n", nextpg->addr_hi, nextpg->addr_lo);
	}
	fp->rx_comp_cons.num_pages = num_pages;

	/* sge */
	num_pages = RX_SGE_NUM_PAGES;
	mapping = RX_SGE_OFFSET(rx_dma->pa, rx_dma->num_desc);

	bnx2x_plugin_bzero((char*)fp->rx_sge_ring,
			   num_pages * PLUGIN_RING_UNIT_SIZE);

	for (i = 1; i <= num_pages; i++) {
		struct eth_rx_sge *sge;
		sge = &fp->rx_sge_ring[RX_SGE_PAGE * i - 2];

		sge->addr_hi =cpu_to_le32(U64_HI(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		sge->addr_lo =cpu_to_le32(U64_LO(mapping +
			PLUGIN_RING_UNIT_SIZE*(i % num_pages)));
		DP(2,"sge-np %x:%x\n", sge->addr_hi, sge->addr_lo);
	}
	bnx2x_plugin_dump_rxq_init(ps, fp, rx_dma);

	for (i = 0; i < ETH_MAX_AGG_QUEUES; i++)
		fp->tpa_pool[i].state = BNX2X_PLUGIN_TPA_STOP;

	/* set pointer to shared statistics */
	vf_stats = (struct bnx2x_vf_plugin_stats *)ps->shared;
	fp->rx_stats = &vf_stats->rxq_stats[idx];
}


/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_ack_sb
*
* Status block acknowledgment write back the updated index and enable
* (or disable) interrupts for that Status block
* id and the queue index
*
* Result:
*    None
*
* Side-effects:
*    Interrupts enabled/disabled for the specific status block
*
*----------------------------------------------------------------------------
*/
static void
bnx2x_plugin_ack_sb(Plugin_State *ps, struct bnx2x_priv *bp,
		    u16 sb_id, u8 storm_id, u32 sb_index, u8 op, u8 update)
{
    struct igu_regular cmd_data;

    cmd_data.sb_id_and_flags =
	((sb_index << IGU_REGULAR_SB_INDEX_SHIFT) |
	(IGU_SEG_ACCESS_NORM << IGU_REGULAR_SEGMENT_ACCESS_SHIFT) |
	(update << IGU_REGULAR_BUPDATE_SHIFT) |
	(op << IGU_REGULAR_ENABLE_INT_SHIFT));

    IGU_ACK(ps, (IGU_CMD_INT_ACK_BASE + sb_id)*8, cmd_data.sb_id_and_flags);
}


static inline s16 __bnx2x_plugin_tx_used(struct bnx2x_plugin_tx_queue *fp)
{
    u16 prod, cons;

    prod = fp->tx_bd_prod.pos;
    cons = fp->tx_bd_cons.pos;
    return SUB_S16(prod, cons);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_tx_avail
*
* checks how many TX descriptors are available
*
* Result:
*    number of available descriptors
*
* Side-effects:
*    None
*
*----------------------------------------------------------------------------
*/
static inline u16 bnx2x_plugin_tx_avail(struct bnx2x_plugin_tx_queue *fp)
{
    return (s16)(fp->num_desc) - __bnx2x_plugin_tx_used(fp);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_cqe_info
*
* calculate the receive flags according to the completion WQE
*
* Result:
*    None
*
* Side-effects:
*    None
*
*----------------------------------------------------------------------------
*/
#define BNX2X_PLUGIN_CQE_MATCH(cqe)                                         \
    (((cqe)->status_flags & ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG) ?  1 : 0)

#define BNX2X_PLUGIN_CQE_VLAN(cqe)                                          \
    (((cqe)->pars_flags.flags & PARSING_FLAGS_INNER_VLAN_EXIST) ?  1 : 0)

#define BNX2X_PLUGIN_CQE_OVERETH(cqe)                                       \
    (((cqe)->pars_flags.flags & PARSING_FLAGS_OVER_ETHERNET_PROTOCOL) >>    \
    PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT)

#define BNX2X_PLUGIN_CQE_OVERIP(cqe)                                        \
    (((cqe)->pars_flags.flags & PARSING_FLAGS_OVER_IP_PROTOCOL) >>          \
    PARSING_FLAGS_OVER_IP_PROTOCOL_SHIFT)

#define BNX2X_PLUGIN_CQE_IP_XSUM(cqe)                                       \
    (((cqe)->status_flags & ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG) \
    ? SHELL_XSUM_UNKNOWN : 						    \
    ((cqe)->type_error_flags & ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG   	    \
    ? SHELL_XSUM_INCORRECT : SHELL_XSUM_CORRECT))

#define BNX2X_PLUGIN_CQE_L4_XSUM(cqe)                                       \
    (((cqe)->status_flags & ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG) \
    ? SHELL_XSUM_UNKNOWN : 						    \
    (((cqe)->type_error_flags & ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG)       \
    ? SHELL_XSUM_INCORRECT : SHELL_XSUM_CORRECT))

#define BNX2X_PLUGIN_CQE_HASH_TYPE(cqe)                                     \
    (((cqe)->status_flags & ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE) >>          \
    ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE_SHIFT)

#define BNX2X_PLUGIN_CQE_HASH_FUNCTION(cqe)                                 \
    (((cqe)->status_flags & ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG) ?            \
    SHELL_RECV_HASH_FUNCTION_TOEPLITZ : SHELL_RECV_HASH_FUNCTION_NONE)




static inline void bnx2x_plugin_cqe_info(struct eth_fast_path_rx_cqe* cqe,
					 Shell_RecvFrame *frame)
{
	u16 over_eth_ip;

	frame->perfectFiltered = BNX2X_PLUGIN_CQE_MATCH(cqe);
	frame->vlan = BNX2X_PLUGIN_CQE_VLAN(cqe);
	frame->vlanTag = le16_to_cpu(cqe->vlan_tag);

	frame->rssHashFunction = BNX2X_PLUGIN_CQE_HASH_FUNCTION(cqe);
	if (frame->rssHashFunction != SHELL_RECV_HASH_FUNCTION_NONE) {
		frame->rssHashType = BNX2X_PLUGIN_CQE_HASH_TYPE(cqe);;
		frame->rssHashValue = le32_to_cpu(cqe->rss_hash_result);
	}
	else {
		frame->rssHashType = 0;
		frame->rssHashValue = 0;
	}

	/* ether type */
	over_eth_ip = BNX2X_PLUGIN_CQE_OVERETH(cqe);
	frame->ipv4 = (over_eth_ip == PRS_FLAG_OVERETH_IPV4) ? 1 : 0;
	frame->ipv6 = (over_eth_ip == PRS_FLAG_OVERETH_IPV6) ? 1 : 0;
	frame->nonIp = (frame->ipv4 | frame->ipv6) ? 0 : 1;

	/* transport protocol */
	if (!frame->nonIp) {
		over_eth_ip = BNX2X_PLUGIN_CQE_OVERIP(cqe);
		frame->tcp = (over_eth_ip == PRS_FLAG_OVERIP_TCP) ? 1 : 0;
		frame->udp = (over_eth_ip == PRS_FLAG_OVERIP_UDP) ? 1 : 0;
	}
	else {
		frame->tcp = frame->udp = 0;
	}

	/* checksum */
	frame->ipXsum = BNX2X_PLUGIN_CQE_IP_XSUM(cqe);
	frame->tcpXsum = frame->udpXsum = BNX2X_PLUGIN_CQE_L4_XSUM(cqe);
}
/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_tx_split
 *
 * Split the first BD into headers and data BDs
 * to ease the pain of our fellow microcode engineers
 * This is was observed to happen in Windows, but so far not in Linux
 *
 * Result:
 *    The updated bd_prod
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static void bnx2x_plugin_tx_split(Plugin_State *ps,
				  struct bnx2x_plugin_tx_queue *fp,
				  struct eth_tx_start_bd **tx_bd,
				  struct plugin_rpos* bd_prod,
				  u16 hlen, int nbd)
{
	struct eth_tx_start_bd *start_bd = *tx_bd;
	int old_len = le16_to_cpu(start_bd->nbytes);
	struct eth_tx_bd *data_bd;
	dma_addr_t mapping;

	/* first fix first (start) BD */
	start_bd->nbd = cpu_to_le16(nbd);
	start_bd->nbytes = cpu_to_le16(hlen);

	DP(4, "TSO split header size is %d (%x:%x) nbd %d\n",
		start_bd->nbytes, start_bd->addr_hi, start_bd->addr_lo,
		start_bd->nbd);

	/* now get a new data BD (after the pbd) and fill it */
	NEXT_TX_POS((*bd_prod));
	data_bd = &fp->tx_desc_ring[TX_BD((*bd_prod))].reg_bd;

	mapping = HILO_U64(le32_to_cpu(start_bd->addr_hi),
			   le32_to_cpu(start_bd->addr_lo)) + hlen;

	data_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	data_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
	data_bd->nbytes = cpu_to_le16(old_len - hlen);


	DP(3, "TSO split data size is %d (%x:%x)\n",
	   data_bd->nbytes, data_bd->addr_hi, data_bd->addr_lo);


	/* update tx_bd - This is done only to keep track of the last
	 * non-parsing BD (for debug)
	 */
	*tx_bd = (struct eth_tx_start_bd *)data_bd;
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_set_sbd_csum
*
* Fills in the Tx start BD
*
* Result:
*   None
*
* Side-effects:
*   None.
*
*----------------------------------------------------------------------------
*/
static inline void bnx2x_plugin_set_sbd_csum(struct bnx2x_priv *bp,
					     struct eth_tx_start_bd *sbd,
					     const Plugin_SendInfo *info)
{
    if (info->xsumTcpOrUdp)
	sbd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_L4_CSUM;

    if (info->ipv4)
	sbd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_IP_CSUM;
    else
	sbd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_IPV6;

    if (info->udp)
	sbd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_IS_UDP;
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_set_pbd_csum
*
* Fills in the Tx parsing BD checksum info
*
* Result:
*   None
*
* Side-effects:
*   None.
*
*----------------------------------------------------------------------------
*/
static inline void
bnx2x_plugin_set_pbd_csum(struct bnx2x_priv *bp,
			  struct eth_tx_parse_bd_e2 *pbd,
			  const Plugin_SendInfo *info)
{
    u16 val = 0;

    /* transport header offset in words */
    val = (u16)(info->l4HeaderOffset  >> 1);
    pbd->parsing_data |= (cpu_to_le16(val) << ETH_TX_PARSE_BD_E2_TCP_HDR_START_OFFSET_W_SHIFT);

    /* transport header length in d-words */
    val = (u16)((info->l4DataOffset - info->l4HeaderOffset) >> 2);
    pbd->parsing_data |= (cpu_to_le16(val) << ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW_SHIFT);
}


static inline void
bnx2x_plugin_set_fw_mac_addr(u16 *fw_hi, u16 *fw_mid, u16 *fw_lo, u8 *mac)
{
	((u8 *)fw_hi)[0]  = mac[1];
	((u8 *)fw_hi)[1]  = mac[0];
	((u8 *)fw_mid)[0] = mac[3];
	((u8 *)fw_mid)[1] = mac[2];
	((u8 *)fw_lo)[0]  = mac[5];
	((u8 *)fw_lo)[1]  = mac[4];
}
/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_update_producers
*
* updates the current Rx producer valuesto the FW
*
* Result:
*   None
*
* Side-effects:
*   The FW wakes up and if it was waiting for descriptors will start delivering
*   packets.
*
*----------------------------------------------------------------------------
*/
static inline void
bnx2x_plugin_update_producers(Plugin_State *ps, struct bnx2x_priv *bp,
			      struct bnx2x_plugin_rx_queue *fp)
{
    struct ustorm_eth_rx_producers rx_prods = {0};

    rx_prods.bd_prod = fp->rx_bd_prod.pos;
    rx_prods.cqe_prod = fp->rx_comp_prod;
    rx_prods.sge_prod = fp->rx_sge_prod;

    /* memory barriers */
    PROD_UPDATE(ps, fp->q_id, rx_prods);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_rx_comp_used
*
* checks how many RX completions are used
*
* Result:
*    number of available descriptors
*
* Side-effects:
*    None
*
*----------------------------------------------------------------------------
*/
u16 bnx2x_plugin_rx_comp_used(struct bnx2x_plugin_rx_queue *fp)
{
    u16 prod, cons;

    prod = fp->rx_comp_prod;
    cons = fp->rx_comp_cons.pos;
    return (SUB_S16(prod, cons));
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_rx_used
*
* checks how many RX descriptors are used
*
* Result:
*    number of available descriptors
*
* Side-effects:
*    None
*
*----------------------------------------------------------------------------
*/
static inline u16 bnx2x_plugin_rx_used(struct bnx2x_plugin_rx_queue *fp)
{
    u16 prod, cons;

    prod = fp->rx_bd_prod.pos;
    cons = fp->rx_bd_cons.pos;
    return (SUB_S16(prod, cons));
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_tx_used
*
* checks how many TX descriptors are used
*
* Result:
*    number of available descriptors
*
* Side-effects:
*    None
*
*----------------------------------------------------------------------------
*/
static inline s16 bnx2x_plugin_tx_used(struct bnx2x_plugin_tx_queue *fp)
{
    return __bnx2x_plugin_tx_used(fp);
}


/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_alloc_small_buf - wrapper for shell API
*
*----------------------------------------------------------------------------
*/
static inline
dma_addr_t bnx2x_plugin_alloc_small_buf(Plugin_State *ps,
					   struct bnx2x_plugin_rx_queue *fp,
					   u32 bufid)
{
    struct Shell_RxQueueHandle* q_handle = ps->rxQueues[fp->idx].handle;
    return (dma_addr_t)ps->shellApi.allocSmallBuffer(q_handle, bufid);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_alloc_large_buf - wrapper for shell API
*
*----------------------------------------------------------------------------
*/
static inline
dma_addr_t bnx2x_plugin_alloc_large_buf(Plugin_State *ps,
				    struct bnx2x_plugin_rx_queue *fp,
				    u32 bufid)
{
    struct Shell_RxQueueHandle* q_handle = ps->rxQueues[fp->idx].handle;
    return (dma_addr_t)ps->shellApi.allocLargeBuffer(q_handle, bufid);
}

/*
*----------------------------------------------------------------------------
*
* bnx2x_plugin_indicate_recv
*
*    Indicate a receive frame to the upper layer. The ownership of the frame's
*    buffers is transferred to the upper layer
*
*    This call can only be made from bnx2x_plugin_check_rx_ring
*
* Result:
*    None.
*
* Side-effects:
*    The buffers are passed up to the OS stack.
*
*----------------------------------------------------------------------------
*/
static inline void
bnx2x_plugin_indicate_recv(Plugin_State *ps, struct bnx2x_priv *bp,
						   struct bnx2x_plugin_rx_queue *fp,
						   Shell_RecvFrame *frame)
{
    struct Shell_RxQueueHandle* q_handle = ps->rxQueues[fp->idx].handle;
    /*bnx2x_plugin_dump_recv_frame(ps, frame);*/
    ps->shellApi.indicateRecv(q_handle, frame);
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_sw_init() -- { Plugin_SwInit }
 *
 * 	  Initialize the s/w state of the plugin. The h/w should not be
 * 	  initialized through this function. This function is called before any
 * 	  other plugin API is called by the shell (except for api exchange
 *	  function).
 *
 * Result:
 *    0 for success; non-zero for failure
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32 bnx2x_plugin_sw_init(Plugin_State *ps)
{
    uint32 i;
    struct bnx2x_plugin_queue_dma_info rx_dma, tx_dma;

    struct bnx2x_plugin_device_info *dev_info =
	(struct bnx2x_plugin_device_info*)ps->deviceInfo;

    struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);

    DP(6, "bnx2x_priv size %d rxq size %d, chip-id %x, pci-func %x, fp-flags %x rxqs %d txqs %d\n",
	     sizeof(struct bnx2x_priv),
	     sizeof(struct bnx2x_plugin_rx_queue),
	     bp->chip_id,
	     bp->pci_func,
	     bp->fp_flags,
	     ps->numRxQueues,
	     ps->numTxQueues);

    bnx2x_plugin_bzero((char*)bp, sizeof(struct bnx2x_priv));

    bp->chip_id = dev_info->chip_id;
    bp->pci_func = dev_info->pci_func;
    bp->fp_flags = dev_info->fp_flags;

    DP(3, "chip-id %x, pci-func %x, fp-flags %x\n",
       bp->chip_id,
       bp->pci_func,
       bp->fp_flags);

    /* initialize RX queues */

    /* Note: We assume here the number of RX and TX rings are equal */
    for (i = 0; i < ps->numRxQueues; i++) {
	rx_dma.va = ps->rxQueues[i].ringBaseVA;
	rx_dma.pa = (dma_addr_t)ps->rxQueues[i].ringBasePA;
	rx_dma.num_desc = (u16)ps->rxQueues[i].ringSize;
	tx_dma.va = ps->txQueues[i].ringBaseVA;
	tx_dma.pa = (dma_addr_t)ps->txQueues[i].ringBasePA;
	tx_dma.num_desc = (u16)ps->txQueues[i].ringSize;
	bnx2x_plugin_init_rx_fp(ps, bp, &bp->rxfp[i], &rx_dma, &tx_dma,
				&dev_info->sb_info[i],
				dev_info->hw_qid[i], i);
    }

    /* initialize TX queues */
    for (i = 0; i < ps->numTxQueues; i++) {
	tx_dma.va = ps->txQueues[i].ringBaseVA;
	tx_dma.pa = (dma_addr_t)ps->txQueues[i].ringBasePA;
	tx_dma.num_desc = (u16)ps->txQueues[i].ringSize;
	bnx2x_plugin_init_tx_fp(ps, &bp->txfp[i], &tx_dma,
				&dev_info->sb_info[i], i);
    }
    /*bnx2x_plugin_dump_entry_pointes(ps);*/
    ShellLog(1, "bnx2x_plugin %s initialized successfully\n", PLUGIN_VERSION);
    return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_reinit_rx_ring() -- { Plugin_ReinitRxRing }
 *
 * 		bzero rings and reinit head/tail pointers/registers
 * 		should not return any buffers that are found, and assume have
 *		already been garbage collected.
 *
 * Result:
 *    zero (essentially void)
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32 bnx2x_plugin_reinit_rx_ring(Plugin_State *ps, uint32 queue)
{
	u16 i, prod, num_rcq;
	struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
	struct bnx2x_plugin_rx_queue *fp = &bp->rxfp[queue];

	/* bd */
	fp->rx_bd_prod.pos = 0;
	fp->rx_bd_prod.page_off = 0;
	fp->rx_bd_prod.page_cnt = 0;
	fp->rx_bd_cons.pos = 0;
	fp->rx_bd_cons.page_off = 0;
	fp->rx_bd_cons.page_cnt = 0;
	fp->rx_uid_cons.pos = 0;
	fp->rx_uid_cons.page_off = 0;
	fp->rx_uid_cons.page_cnt = 0;
	bnx2x_plugin_dump_rpos(ps, "rx-bd-prod", &fp->rx_bd_prod);
	bnx2x_plugin_dump_rpos(ps, "rx-bd-cons", &fp->rx_bd_cons);
	bnx2x_plugin_dump_rpos(ps, "rx-uid-cons", &fp->rx_uid_cons);

	/* rcq */
	fp->rx_comp_prod = 0;
	fp->rx_comp_cons.pos = 0;
	fp->rx_comp_cons.page_off = 0;
	fp->rx_comp_cons.page_cnt = 0;
	bnx2x_plugin_dump_rpos(ps, "rx-comp-cons", &fp->rx_comp_cons);


	/* sge */
	fp->rx_sge_prod = 0;
	fp->rx_sge_cons = 0;
	fp->last_max_sge = 0;


	/* erase all the RX completions - leaving the next pointer intact */
	num_rcq = min(fp->num_desc + RX_SGE_COUNT,
		      (u16)(fp->rx_comp_cons.num_pages * RX_RCQ_PAGE_MAX));

	for (i = 0, prod = 0; i < num_rcq; i++,  prod = NEXT_RCQ_IDX(prod)) {
		bnx2x_plugin_bzero ((char*)&fp->rx_comp_ring[prod],
				    sizeof(union eth_rx_cqe) );
	}
	fp->rx_comp_prod = prod;

	/* re-populate the BD ring */
	for (i = 0, prod = 0; i < fp->num_desc; i++, prod = NEXT_RX_IDX(prod)) {
		struct eth_rx_bd *rx_bd;
		dma_addr_t mapping;
		u32 bufid;

		bufid = bnx2x_plugin_bd_global_id(i, fp->num_desc, fp->idx);
		mapping = bnx2x_plugin_alloc_small_buf(ps, fp, bufid);
		rx_bd = &fp->rx_desc_ring[prod];
		rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
		rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
		fp->rx_uid_ring[prod] = i;
		NEXT_RX_POS(fp->rx_bd_prod);

		bnx2x_plugin_dump_rx_bd(ps, fp, rx_bd, i, bufid, mapping);
	}

	/* re-populate the SGE ring */
	if (!BP_TPA_DISABLED(bp)) {
		for (i = 0, prod = 0; i < RX_SGE_COUNT;
		     i++, prod = NEXT_SGE_IDX(prod)) {
			struct eth_rx_sge *rx_sge;
			dma_addr_t mapping;
			u32 bufid;

			bufid = bnx2x_plugin_sge_global_id(i, fp->num_desc,
							   fp->idx);
			mapping = bnx2x_plugin_alloc_large_buf(ps, fp, bufid);
			rx_sge = &fp->rx_sge_ring[prod];
			rx_sge->addr_hi = cpu_to_le32(U64_HI(mapping));
			rx_sge->addr_lo = cpu_to_le32(U64_LO(mapping));
		}
		fp->rx_sge_prod = prod;

		/* set the sge mask */
		for (i = 0; i < RX_SGE_COUNT >> RX_SGE_MASK_ELEM_SHIFT; i++)
			fp->sge_mask[i] = RX_SGE_MASK_ELEM_ONE_MASK;

		bnx2x_plugin_clear_sge_mask_next_elems(fp);
		fp->tpa_max_ind = 0;
	}
	/*bnx2x_plugin_dump_rings_raw(ps, bp, fp);*/
	return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_reinit_tx_ring() -- { Plugin_ReinitTxRing }
 *
 * 		bzero rings and reinit head/tail pointers/registers
 * 		should not return any buffers that are found, and assume have
 *		already been garbage collected.
 *
 * Result:
 *    zero (essentially void)
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32 bnx2x_plugin_reinit_tx_ring(Plugin_State *ps, uint32 queue)
{
    u16 i;

    struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
    struct bnx2x_plugin_tx_queue *fp = &bp->txfp[queue];

    /* initialize TX prod/cons */
    fp->tx_bd_prod.pos = fp->tx_bd_prod.page_off = fp->tx_bd_prod.page_cnt = 0;
    fp->tx_bd_cons.pos = fp->tx_bd_cons.page_off = fp->tx_bd_cons.page_cnt = 0;

    /* erase all the TX BDs */
    for (i = 0; i < fp->num_desc; i = NEXT_TX_IDX(i)) {
	bnx2x_plugin_bzero ((char*)&fp->tx_desc_ring[i], sizeof(union eth_tx_bd_types) );
    }
	return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_enable_interrupt() -- { Plugin_EnableInterrupt }
 *
 *  Enable the interrupt indicated by 'intrIdx' (note is not queue #)
 *
 *   For now we assume one vector for both TX and RX, and non shared index
 *  (57711 style status blocks)
 *
 * Result:
 *  None
 *
 * Side-effects:
 *  None
 *
 *----------------------------------------------------------------------------
 */
static inline void
bnx2x_plugin_rx_enable_interrupt(Plugin_State *ps, struct bnx2x_priv *bp,
				 struct bnx2x_plugin_rx_queue *fp)
{
	bnx2x_plugin_ack_sb(ps, bp, fp->sb_id, 0, 0, IGU_INT_DISABLE, 0);
	bnx2x_plugin_ack_sb(ps, bp, fp->sb_id, USTORM_ID,
			    cpu_to_le32(fp->sw_sb_idx), IGU_INT_ENABLE, 1);
}

static inline void
bnx2x_plugin_tx_enable_interrupt(Plugin_State *ps, struct bnx2x_priv *bp,
				 struct bnx2x_plugin_tx_queue *fp)
{
	bnx2x_plugin_ack_sb(ps, bp, fp->sb_id, 0, 0, IGU_INT_DISABLE, 0);
	bnx2x_plugin_ack_sb(ps, bp, fp->sb_id, CSTORM_ID,
			    cpu_to_le32(fp->sw_sb_idx), IGU_INT_ENABLE, 1);
}

static uint32 bnx2x_plugin_enable_interrupt(Plugin_State *ps, uint32 intrIdx)
{
	uint32 queue = intrIdx;
	struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);

	struct bnx2x_plugin_rx_queue *rxfp =
		queue < ps->numRxQueues ? &bp->rxfp[queue] : NULL;
	struct bnx2x_plugin_tx_queue *txfp =
		queue < ps->numTxQueues ? &bp->txfp[queue] : NULL;
	u16 rx_hw_cons, tx_hw_cons;

	if (!rxfp && !txfp)
		/* a bogus intrIdx */
		return 0;

	if (rxfp && !txfp)
		bnx2x_plugin_rx_enable_interrupt(ps, bp, rxfp);

	else if (txfp && !rxfp)
		bnx2x_plugin_tx_enable_interrupt(ps, bp, txfp);

	else if (!BP_SHARED_SB(bp)) {
		/* the sb_id is shared, but the sb_index is not  */
		bnx2x_plugin_ack_sb(ps, bp, rxfp->sb_id, 0, 0,
			IGU_INT_DISABLE, 0);

		bnx2x_plugin_ack_sb(ps, bp, txfp->sb_id, CSTORM_ID,
			cpu_to_le32(txfp->sw_sb_idx), IGU_INT_NOP, 1);

		bnx2x_plugin_ack_sb(ps, bp, rxfp->sb_id, USTORM_ID,
			cpu_to_le32(rxfp->sw_sb_idx), IGU_INT_ENABLE, 1);
	} else {

		/* both sb_id and sb_index are shared - this is the default */
		u8 update = 1;

		bnx2x_plugin_ack_sb(ps, bp, rxfp->sb_id, 0, 0,
			IGU_INT_DISABLE, 0);

		/* sample the sb index */
		bnx2x_plugin_update_rx_sb_idx(rxfp);

		/* rx pending work */
		rx_hw_cons = le16_to_cpu(*(rxfp->rx_cons_sb));
		if ((rx_hw_cons & RX_RCQ_PAGE_MASK) == RX_RCQ_PAGE_MAX)
		    rx_hw_cons++;

		/* tx pending work */
		tx_hw_cons = le16_to_cpu(*(txfp->tx_cons_sb));

		/*
		 * If there is more pending work we should not update the sb
		 * index
		 */
		if ((rx_hw_cons != rxfp->rx_comp_cons.pos) ||
		    (tx_hw_cons != txfp->tx_pkt_cons))
			/*rxfp->sw_sb_idx--;*/
			update = 0;

		bnx2x_plugin_ack_sb(ps, bp, rxfp->sb_id, USTORM_ID,
			cpu_to_le32(rxfp->sw_sb_idx), IGU_INT_ENABLE, update);
	}
	return 0;
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_disable_interrupt() -- { Plugin_DisableInterrupt }
 *
 *    Disable the interrupt indicated by 'intrIdx' (note is not queue #)
 *
 *   For now we assume one vector for both TX and RX, and non shared index
 *  (57711 style status blocks)
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32 bnx2x_plugin_disable_interrupt(Plugin_State *ps, uint32 intrIdx)
{
    uint32 queue = intrIdx;
    struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
    struct bnx2x_plugin_rx_queue *rxfp = &bp->rxfp[queue];
    bnx2x_plugin_ack_sb(ps, bp, rxfp->sb_id, 0, 0, IGU_INT_DISABLE, 0);
	return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_add_frame_tx_ring -- { Plugin_AddFrameToTxRing }
 *
 *    Add the frame made up of buffers in the sg list 'frame' to the hardware tx
 *    ring of the given queue. The offload information is passed in 'info'.
 *    'lastPktHint' is used to indicate that no more tx packets would be passed
 *    down in this context and the plugin should use this as a hint to write to
 *    the h/w doorbell.
 *
 * Result:
 *    0 if successful, 1 to indicate no space in h/w tx ring
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32
bnx2x_plugin_add_frame_tx_ring(Plugin_State *ps, uint32 queue,
			       const Plugin_SendInfo *info,
			       const Plugin_SgList *sgl,
			       Bool lastPktHint)
{
	struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
	struct bnx2x_plugin_tx_queue *fp = &bp->txfp[queue];

	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd, *total_pkt_bd = NULL;
	struct eth_tx_parse_bd_e2 *pbd = NULL;
	struct plugin_rpos bd_prod;
	u64 mapping;

	unsigned char *src_mac = bnx2x_plugin_eth_src(info, sgl);
	unsigned char *dst_mac = bnx2x_plugin_eth_dst(info, sgl);
	u32 i;
	u16 pkt_prod, nbd, hlen = 0;
	u8 mac_type = UNICAST_ADDRESS;

	/* take into account: start BD, parsing BD, next BD */
	if (bnx2x_plugin_tx_avail(fp) < (sgl->numElements + 3)) {
		fp->driver_xoff++;
		return BNX2X_PLUGIN_TX_BUSY;
	}

	/* set mac address type - default is unicast */
	if(is_multicast_ether_addr(dst_mac)) {
		if (is_broadcast_ether_addr(dst_mac))
			mac_type = BROADCAST_ADDRESS;
		else
			mac_type = MULTICAST_ADDRESS;
	}
	/*
	 * Please read carefully. First we use one BD which we mark as start,
	 * then we have a parsing info BD (used for TSO or xsum),
	 * and only then we have the rest of the TSO BDs.
	 * (don't forget to mark the last one as last,
	 * and to unmaps only AFTER you write to the BD ...)
	 * And above all, all pbd sizes are in words - NOT DWORDS!
	 */
	pkt_prod = fp->tx_pkt_prod++;
	bd_prod = fp->tx_bd_prod;

	/* start BD */
	tx_start_bd = &fp->tx_desc_ring[TX_BD(bd_prod)].start_bd;
	tx_start_bd->bd_flags.as_bitfield = ETH_TX_BD_FLAGS_START_BD;
	tx_start_bd->general_data = (mac_type <<
				     ETH_TX_START_BD_ETH_ADDR_TYPE_SHIFT);

	/*
	 * Header nbd - Note that we always assume that the header is
	 * contained in the first sge. In case of TSO the header and
	 * data must appear in distinct BDs so we may need to split
	 * an sge into 2 BDs. The assumption above means that we only
	 * handle the case of splitting the FIRST sge into 2 BDs.
	 */
	tx_start_bd->general_data |= (1 << ETH_TX_START_BD_HDR_NBDS_SHIFT);

	/* vlan */
	if (info->vlan) {
		tx_start_bd->vlan_or_ethertype = cpu_to_le16(info->vlanTag);
		tx_start_bd->bd_flags.as_bitfield |= (ETH_TX_BD_FLAGS_VLAN_MODE &
		(X_ETH_OUTBAND_VLAN << ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT));
	} else {
#ifdef PLUGIN_ENFORCE_SECURITY
		tx_start_bd->vlan_or_ethertype =
			cpu_to_le16(bnx2x_plugin_eth_type(info, sgl));
#else
		/* for debug only - to discover driver/fw lack of sync */
		tx_start_bd->vlan_or_ethertype = cpu_to_le16(pkt_prod);
#endif
	}

	mapping = sgl->elements[0].pa;
	tx_start_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	tx_start_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

	nbd = (u16)(sgl->numElements + 1); /* nr_elem + pbd */
	tx_start_bd->nbd = cpu_to_le16(nbd);
	tx_start_bd->nbytes = cpu_to_le16(sgl->elements[0].length);

	/*
	DP(4, "sending pkt %u next_idx %u bd %u @%p\n",
	   pkt_prod, fp->tx_pkt_prod, bd_prod.pos, tx_start_bd);

	DP(7, "first bd @%p  addr (%x:%x) nbd %d nbytes %d flags %x vlan %x\n",
	   tx_start_bd, tx_start_bd->addr_hi,
	   tx_start_bd->addr_lo,
	   le16_to_cpu(tx_start_bd->nbd),
	   le16_to_cpu(tx_start_bd->nbytes),
	   tx_start_bd->bd_flags.as_bitfield,
	   le16_to_cpu(tx_start_bd->vlan_or_ethertype));
	*/

	/* Parsing BD */
	NEXT_TX_POS(bd_prod);
	pbd = &fp->tx_desc_ring[TX_BD(bd_prod)].parse_bd_e2;

	bnx2x_plugin_bzero((char*)pbd, sizeof(struct eth_tx_parse_bd_e2));

	if (info->xsumTcpOrUdp) {

		/* set the start bd */
		bnx2x_plugin_set_sbd_csum(bp, tx_start_bd, info);

		/* and now the parsing bd */
		bnx2x_plugin_set_pbd_csum(bp, pbd, info);
	}

	if (info->tso) {

		/* DP(3, "TSO total len %d  hlen %d  tso mss %d\n",
		   sgl->totalLength, info->l4DataOffset, info->tsoMss); */

		tx_start_bd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_SW_LSO;

		/*
		 * Check if the first sge holds more then the header. In such
		 * a case it must be split onto 2 BDs.
		 */
		hlen = (u16)info->l4DataOffset;
		if (sgl->elements[0].length > hlen)
			bnx2x_plugin_tx_split(ps, fp, &tx_start_bd, &bd_prod,
					      hlen, ++nbd);

		pbd->parsing_data |= (cpu_to_le16(info->tsoMss) <<
				      ETH_TX_PARSE_BD_E2_LSO_MSS_SHIFT);
		if (info->ipv6 &&
		    (bnx2x_plugin_ipv6_hdr(info, sgl))->nexthdr == NEXTHDR_IPV6)
			pbd->parsing_data |=
			ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR;

		fp->tx_stats->tso_pkts++;
		fp->tx_stats->tso_bytes += sgl->totalLength;
	}

	/* set MACs */
	bnx2x_plugin_set_fw_mac_addr(&pbd->src_mac_addr_hi,
				     &pbd->src_mac_addr_mid,
				     &pbd->src_mac_addr_lo, src_mac);

	bnx2x_plugin_set_fw_mac_addr(&pbd->dst_mac_addr_hi,
				     &pbd->dst_mac_addr_mid,
				     &pbd->dst_mac_addr_lo, dst_mac);

	DP(6, "src_mac_addr_lo %x src_mac_addr_mid %x src_mac_addr_hi %x\n"
	      "dst_mac_addr_lo %x dst_mac_addr_mid %x dst_mac_addr_hi %x\n",
	   pbd->src_mac_addr_lo,
	   pbd->src_mac_addr_mid,
	   pbd->src_mac_addr_hi,
	   pbd->dst_mac_addr_lo,
	   pbd->dst_mac_addr_mid,
	   pbd->dst_mac_addr_hi);

	/*
	 * This is done only to keep track of the last
	 * non-parsing BD (for debug)
	 */
	tx_data_bd = (struct eth_tx_bd *)tx_start_bd;

	for (i = 1; i < sgl->numElements; i++) {
		u16 _prod;

		NEXT_TX_POS(bd_prod);
		_prod = TX_BD(bd_prod);
		tx_data_bd = &fp->tx_desc_ring[_prod].reg_bd;
		if (total_pkt_bd == NULL)
			total_pkt_bd = &fp->tx_desc_ring[_prod].reg_bd;

		mapping = sgl->elements[i].pa;
		tx_data_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
		tx_data_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
		tx_data_bd->nbytes = cpu_to_le16(sgl->elements[i].length);

		/*
		DP(5, "elem %d bd @%p addr (%x:%x) nbytes %d\n",
		   i, tx_data_bd, tx_data_bd->addr_hi, tx_data_bd->addr_lo,
		   le16_to_cpu(tx_data_bd->nbytes));
		*/
	}
	NEXT_TX_POS(bd_prod);

	/*
	 * Update nbd counting the next BD if the packet contains or ends
	 * with it.
	 */
	if (bd_prod.page_off < nbd)
		nbd++;

	/*
	 * Set the total bytes - this may have to be removed for VF
	 * separation considerations
	 */
	if (total_pkt_bd != NULL)
		total_pkt_bd->total_pkt_bytes =
		(__le16)cpu_to_le32(sgl->totalLength);

	/* now send a tx doorbell */
	if (lastPktHint) {
		fp->tx_db.data.prod += nbd;
		DOORBELL(ps, fp->idx, fp->tx_db.raw);
	}
	fp->tx_bd_prod = bd_prod;
	return BNX2X_PLUGIN_TX_OK;
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_check_tx_ring -- { Plugin_CheckTxRing }
 *
 *    Check the tx ring for the given queue for any tx completions.
 *    This call is made by the shell either during the interrupt or DPC/BH
 *    context.
 *
 * Result:
 *    Number of pre-TSO packets tx completed.
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32 bnx2x_plugin_check_tx_ring(Plugin_State *ps, uint32 queue)
{
    struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
    struct bnx2x_plugin_tx_queue *fp = &bp->txfp[queue];
    struct plugin_rpos bd_cons = fp->tx_bd_cons;
    u16 hw_cons, sw_cons;
    u32 done = 0;

    /*
     * if sb index is not shared sample the current HW index for later
     * (different sb_id or separate indices for RX and TX)
     */
    if (!BP_SHARED_SB(bp))
	bnx2x_plugin_update_tx_sb_idx(fp);

    hw_cons = le16_to_cpu(*(fp->tx_cons_sb));
    sw_cons = fp->tx_pkt_cons;

    while (sw_cons != hw_cons) {
	struct eth_tx_start_bd *tx_start_bd =
	    &fp->tx_desc_ring[TX_BD(bd_cons)].start_bd;

	u16 nbd = le16_to_cpu(tx_start_bd->nbd);

	/*
	DP(4, "TXQ[%d] - hw_cons %u  sw_cons %u  bd_cons %u\n",
	   fp->idx, hw_cons, sw_cons, bd_cons.pos);
	*/

	while (nbd-- > 0)
	    NEXT_TX_POS(bd_cons);

	sw_cons++;
	done++;
    }
    fp->tx_pkt_cons = sw_cons;
    fp->tx_bd_cons = bd_cons;
    if (done)
	    ps->shellApi.completeSend(ps->txQueues[queue].handle, done);

    return done;
}


/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_fill_frag
 *
 *    Fills in a multi-sge frame (LRO/Jumbo)
 *----------------------------------------------------------------------------
 */
static inline
void bnx2x_plugin_fill_frags(Plugin_State *ps, struct bnx2x_priv *bp,
			     struct bnx2x_plugin_rx_queue *fp, u16 len_on_bd,
			     struct eth_fast_path_rx_cqe *cqe,
			     Shell_RecvFrame *frame)
{
	u32 frag_size, frag_len, pages, i, j;
	u16 sge_idx;

	if (frame->byteLength <= len_on_bd) /* nothing to do - no sges */
		return;

	frag_size = frame->byteLength - len_on_bd;
	pages = SGE_PAGE_ALIGN(frag_size) >> SGE_PAGE_SHIFT;

	for (i = 0, j = 0; i < pages; i+= PAGES_PER_SGE, j++) {
		sge_idx = RX_SGE(le16_to_cpu(cqe->sgl_or_raw_data.sgl[j]));

		/*
		 * FW gives the indices of the SGE as if the ring
		 * is an array (meaning that "next" element will
		 * consume 2 indices)
		 */
		frag_len = min((u32)(SGE_PAGE_SIZE * PAGES_PER_SGE), frag_size);

		frame->sg[j+1].ringOffset =
			bnx2x_plugin_sge_global_id(RX_SGE_BUF(sge_idx),
						   fp->num_desc,
						   fp->idx);
		frame->sg[j+1].length = frag_len;
		SGE_MASK_CLEAR_BIT(fp, sge_idx);
		frame->sgLength++;
		frag_size -= frag_len;
	}
	DP(2, "fp_cqe->sgl[%d] = %d\n", j-1,
	   le16_to_cpu(cqe->sgl_or_raw_data.sgl[j-1]));

	/* here we assume that the last SGE index is the biggest */
	if (SUB_S16(sge_idx, fp->last_max_sge) > 0)
		fp->last_max_sge = sge_idx;
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_tpa_start
 *
 *    Handle tpa start cookie (start, but mnot end)
 *----------------------------------------------------------------------------
 */
static inline
void bnx2x_plugin_tpa_start(Plugin_State *ps, struct bnx2x_priv *bp,
			    struct bnx2x_plugin_rx_queue *fp, u16 qid,
			    u16 bufid, struct plugin_rpos *bd_cons)
{
	DP(1, "tpa_start on queue %d\n", qid);

	if (fp->tpa_pool[qid].state !=  BNX2X_PLUGIN_TPA_STOP)
		DP(1, "start of bin not in stop [%d]\n", qid);


	fp->tpa_pool[qid].state = BNX2X_PLUGIN_TPA_START;
	fp->tpa_pool[qid].bufid = bufid;

	/*
	 * replace the buffid saved in the tpa queue pool with the next shadow
	 * ring id
	 */
	if (fp->rx_uid_cons.pos != bd_cons->pos) {
		DP(4,"bd_cons %d uid_cons %d replacing bufid %d with "
		     "bufid %d\n", bd_cons->pos, fp->rx_uid_cons.pos,
		   fp->rx_uid_ring[RX_BD(*bd_cons)],
		   fp->rx_uid_ring[RX_BD(fp->rx_uid_cons)]);

		fp->rx_uid_ring[RX_BD(*bd_cons)] =
		fp->rx_uid_ring[RX_BD(fp->rx_uid_cons)];
	}
	NEXT_RX_POS(fp->rx_uid_cons);
}


/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_tpa_end
 *
 *    Handle tpa end cookie (end, but not start)
 *----------------------------------------------------------------------------
 */
static inline
void bnx2x_plugin_tpa_stop(Plugin_State *ps, struct bnx2x_priv *bp,
			   struct bnx2x_plugin_rx_queue *fp, u16 qid,
			   struct eth_fast_path_rx_cqe *cqe,
			   Shell_RecvFrame *frame)
{
	u16 bufid, len;

	DP(1, "tpa_stop on queue %d\n", qid);

	if (!BNX2X_PLUGIN_TPA_STOP_VALID(cqe))
		DP(0, "STOP on non TCP data\n");

	bnx2x_plugin_bzero((char*)frame, sizeof(Shell_RecvFrame));

	frame->byteLength = le16_to_cpu(cqe->pkt_len);

	/* crunch the cqe flags and other information */
	bnx2x_plugin_cqe_info(cqe, frame);

	/* assume the IP csum is correct (will be corrected by the FW TODO )*/
	frame->ipXsum = SHELL_XSUM_CORRECT;

	/* This is a size of the linear data on the first sge */
	len = le16_to_cpu(cqe->len_on_bd);
	bufid = fp->tpa_pool[qid].bufid;

	/* fill in first sge */
	frame->sg[0].ringOffset =
		bnx2x_plugin_bd_global_id(bufid, fp->num_desc, fp->idx);

	frame->sg[0].length = len;
	frame->firstSgOffset = le16_to_cpu(cqe->placement_offset);;
	frame->sgLength = 1;

	/* mark first buffer id as indicated so it can be reclaimed next
	 * time we ask for buffers
	 */
	fp->tpa_ind[fp->tpa_max_ind] = bufid;
	fp->tpa_max_ind++;
	fp->tpa_pool[qid].state = BNX2X_PLUGIN_TPA_STOP;

	bnx2x_plugin_fill_frags(ps, bp, fp, len, cqe, frame);

	fp->rx_stats->lro_pkts++;
	fp->rx_stats->lro_bytes += frame->byteLength;
	bnx2x_plugin_indicate_recv(ps, bp, fp, frame);
}

/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_check_rx_ring -- { Plugin_Chec`kRxRing }
 *
 *    Check the rx ring for any incoming packets on the given queue.
 *    'maxPkts' indicate the maximum number of packets the plugin can indicate
 *    up to the shell in this context. The shell calls this function during the
 *    interrupt or DPC/NAPI context.
 *
 * Result:
 *    1 to indicate need for buffers, 0 for no need for buffers.
 *
 * Side-effects:
 *    Packets are indicated up and delivered to the OS stack during this call.
 *
 *----------------------------------------------------------------------------
 */
static uint32
bnx2x_plugin_check_rx_ring(Plugin_State *ps, uint32 queue, uint32 maxPkts)
{
	struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
	struct bnx2x_plugin_rx_queue *fp = &bp->rxfp[queue];
	Shell_RecvFrame frame;
	struct plugin_rpos sw_comp_cons;
	u16 hw_comp_cons;
	u32 pkts_done = 0;

	/*
	 * if sb index is not shared sample the current HW index for later
	 * (different sb_id or separate indexes for RX and TX)
	 */
	if (!BP_SHARED_SB(bp))
		bnx2x_plugin_update_rx_sb_idx(fp);

	/* get the new RCQ consumer and compare it to the saved one */
	hw_comp_cons = le16_to_cpu(*(fp->rx_cons_sb));
	if ((hw_comp_cons & RX_RCQ_PAGE_MASK) == RX_RCQ_PAGE_MAX)
		hw_comp_cons++;

	/* if the consumers differ there's work to be done */
	sw_comp_cons = fp->rx_comp_cons;
	if (sw_comp_cons.pos != hw_comp_cons){
		struct plugin_rpos bd_cons;
		u16 sw_comp_prod;

		bd_cons = fp->rx_bd_cons;
		sw_comp_prod = fp->rx_comp_prod;

		DP(3, "RXQ[%d]: hw_comp_cons %u sw_comp_cons %u\n",
		  fp->idx, hw_comp_cons, sw_comp_cons.pos);

		while (sw_comp_cons.pos != hw_comp_cons) {
			union eth_rx_cqe *cqe;
			u16 bufid, len, pad;
			u8 cqe_flags;

			cqe = &fp->rx_comp_ring[RCQ_BD(sw_comp_cons)];
			cqe_flags = cqe->fast_path_cqe.type_error_flags;

#ifdef __CHIPSIM_NPA__
			/* handle sp CQE CHIPSIM only */
			if (CQE_TYPE(cqe_flags)) {
				bnx2x_plugin_ramrod_complete(ps, bp, fp,
							     &cqe->ramrod_cqe);
				goto next_cqe;
			}
#endif
			bufid = fp->rx_uid_ring[RX_BD(bd_cons)];

			/* bnx2x_plugin_dump_cqe(ps, cqe, bufid);*/

			/*
			 * If CQE is marked both TPA_START and TPA_END it is a
			 * non-TPA CQE
			 */
			if ((!BP_TPA_DISABLED(bp)) && (TPA_TYPE(cqe_flags) !=
				(TPA_TYPE_START | TPA_TYPE_END))) {

				u16 qid = cqe->fast_path_cqe.queue_index;

				if (TPA_TYPE(cqe_flags) == TPA_TYPE_START) {
					bnx2x_plugin_tpa_start(ps, bp, fp, qid,
						bufid, &bd_cons);
					goto next_rx;
				}

				if (TPA_TYPE(cqe_flags) == TPA_TYPE_END) {
					bnx2x_plugin_tpa_stop(ps, bp, fp, qid,
						&cqe->fast_path_cqe, &frame);
					pkts_done++;
					goto next_cqe;
				}
			}

			/* non TPA (LRO) packet could be Jumbo */
			bnx2x_plugin_bzero((char*)&frame,
					   sizeof(Shell_RecvFrame));

			len = le16_to_cpu(cqe->fast_path_cqe.pkt_len);
			frame.byteLength = len;

			len = le16_to_cpu(cqe->fast_path_cqe.len_on_bd);
			pad = le16_to_cpu(cqe->fast_path_cqe.placement_offset);

			/* crunch the cqe flags and other information */
			bnx2x_plugin_cqe_info(&cqe->fast_path_cqe, &frame);

			/* fill in first sg */
			frame.sg[0].ringOffset =
				bnx2x_plugin_bd_global_id(bufid, fp->num_desc,
							  fp->idx);
			frame.sg[0].length = len;
			frame.firstSgOffset = pad;
			frame.sgLength = 1;

			/* fill frags (multi-sg) for Jumbo */
			bnx2x_plugin_fill_frags(ps, bp, fp, len, cqe, &frame);
			bnx2x_plugin_indicate_recv(ps, bp, fp, &frame);
			pkts_done++;
next_rx:
			NEXT_RX_POS(bd_cons);
next_cqe:
			sw_comp_prod = NEXT_RCQ_IDX(sw_comp_prod);
			NEXT_RCQ_POS(sw_comp_cons);

			if (pkts_done == maxPkts ||
			    fp->tpa_max_ind == MAX_TPA_IND_PKTS)
				break;
		} /* while */

		fp->rx_bd_cons = bd_cons;
		fp->rx_comp_cons = sw_comp_cons;
		fp->rx_comp_prod = sw_comp_prod;
	}
	return pkts_done;
}


/*
 *----------------------------------------------------------------------------
 *
 * bnx2x_plugin_add_bufs_rx_ring -- { Plugin_AddBuffersToRxRing }
 *
 *    The plugin can make calls to the shell to allocate more buffers. This call
 *    is made during the plugin initialization or after Plugin_CheckRxRing or
 *    when the OS stack returns buffers back to the shell. The plugin should try
 *    to allocate as many buffers as needed to fill the h/w rings.
 *
 * Result:
 *    None
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
static uint32 bnx2x_plugin_add_bufs_rx_ring(Plugin_State *ps, uint32 queue)
{
	struct bnx2x_priv *bp = (struct bnx2x_priv*)PLUGIN_PRIVATE(ps);
	struct bnx2x_plugin_rx_queue *fp = &bp->rxfp[queue];

	dma_addr_t mapping;
	struct eth_rx_bd *rx_bd;
	struct eth_rx_sge *sge_bd;
	struct plugin_rpos bd_prod, bd_uid_cons;
	u16 bd_cons, local_bufid, first_elem, last_elem, next_elem, sge_delta, i;
	u32 bufid;

	/* reclaim BD buffers (small) */
	bd_prod = fp->rx_bd_prod;
	bd_uid_cons = fp->rx_uid_cons;
	bd_cons = fp->rx_bd_cons.pos;

	while (bd_uid_cons.pos != bd_cons) {
		local_bufid = fp->rx_uid_ring[RX_BD(bd_uid_cons)];
		bufid = bnx2x_plugin_bd_global_id(local_bufid, fp->num_desc,
						  fp->idx);
	
		/* reclaim buffer and put it on the BD ring */
		mapping = bnx2x_plugin_alloc_small_buf(ps, fp, bufid);
		rx_bd = &fp->rx_desc_ring[RX_BD(bd_prod)];
		rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
		rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

		/* update the id shadow ring */
		fp->rx_uid_ring[RX_BD(bd_prod)] = local_bufid;

		/*
		bnx2x_plugin_dump_rx_bd(ps, fp, rx_bd, RX_BD(bd_prod), bufid,
					mapping);
		*/

		NEXT_RX_POS(bd_prod);
		NEXT_RX_POS(bd_uid_cons);
	}

	if (!BP_TPA_DISABLED(bp)) {

		/* reclaim the 'first sge' of indicated tpa frames */
		while (fp->tpa_max_ind > 0) {
			local_bufid = fp->tpa_ind[--fp->tpa_max_ind];
			bufid = bnx2x_plugin_bd_global_id(local_bufid,
							  fp->num_desc,
							  fp->idx);

			/* reclaim buffer and put it on the BD ring */
			mapping = bnx2x_plugin_alloc_small_buf(ps, fp, bufid);
			rx_bd = &fp->rx_desc_ring[RX_BD(bd_prod)];
			rx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
			rx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

			/* update the id shadow ring */
			fp->rx_uid_ring[RX_BD(bd_prod)] = local_bufid;
			NEXT_RX_POS(bd_prod);
		}

		/* reclaim SGE buffers (large) */
		sge_delta = 0;
		last_elem = RX_SGE(fp->last_max_sge) >> RX_SGE_MASK_ELEM_SHIFT;
		first_elem = RX_SGE(fp->rx_sge_cons) >> RX_SGE_MASK_ELEM_SHIFT;
		next_elem = RX_SGE(fp->rx_sge_prod) >> RX_SGE_MASK_ELEM_SHIFT;


		/* If ring is not full */
		if (last_elem + 1 != first_elem)
			last_elem++;

		for (i = first_elem; i != last_elem; i = NEXT_SGE_MASK_ELEM(i),
		next_elem = NEXT_SGE_MASK_ELEM(next_elem)) {
			if (fp->sge_mask[i])
				break;
			fp->sge_mask[next_elem] = RX_SGE_MASK_ELEM_ONE_MASK;
				sge_delta += RX_SGE_MASK_ELEM_SZ;
		}

		if (sge_delta > 0) {
			i = fp->rx_sge_prod;
			fp->rx_sge_prod += sge_delta;
	    fp->rx_sge_cons += sge_delta;

			/* reclaim the buffers */
			while (i!= fp->rx_sge_prod) {
				bufid = bnx2x_plugin_sge_global_id(RX_SGE_BUF(i),
								   fp->num_desc,
								   fp->idx);

				mapping = bnx2x_plugin_alloc_large_buf(ps, fp,
								       bufid);
				sge_bd = &fp->rx_sge_ring[RX_SGE(i)];
				sge_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
				sge_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
				i = NEXT_SGE_IDX(i);
			}
			/* clear page-end entries */
			bnx2x_plugin_clear_sge_mask_next_elems(fp);
		}
		/*
		DP(2, "fp->last_max_sge %d fp->rx_sge_prod %d\n",
		   fp->last_max_sge, fp->rx_sge_prod);
		*/
	}
	fp->rx_bd_prod = bd_prod;
	fp->rx_uid_cons = bd_uid_cons;

	/* Update FW producers */
	bnx2x_plugin_update_producers(ps, bp, fp);
	return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * NPA_PluginMain --
 *
 *    This is the first function that the shell calls into the plugin and is
 *    used to exchange the shell and plugin API function pointers for further
 *    communication.
 *    The shell passes its API through 'shellApi' populated with its api
 *    functions.
 *
 * Result:
 *    Plugin_Api function table filled with the plugin api functions.
 *
 * Side-effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */
uint32 NPA_PluginMain(struct Plugin_Api *pluginApi)
{
	/* Plug-in */
	pluginApi->swInit = bnx2x_plugin_sw_init;
	pluginApi->reinitRxRing = bnx2x_plugin_reinit_rx_ring;
	pluginApi->reinitTxRing = bnx2x_plugin_reinit_tx_ring;
	pluginApi->enableInterrupt = bnx2x_plugin_enable_interrupt;
	pluginApi->disableInterrupt = bnx2x_plugin_disable_interrupt;
	pluginApi->addFrameToTxRing = bnx2x_plugin_add_frame_tx_ring;
	pluginApi->checkTxRing = bnx2x_plugin_check_tx_ring;
	pluginApi->checkRxRing = bnx2x_plugin_check_rx_ring;
	pluginApi->addBuffersToRxRing = bnx2x_plugin_add_bufs_rx_ring;
	return 0;
}



