/*
 * Copyright (c) 2007-2013 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <asm/page.h>
#include <linux/mlx4/cq.h>
#include <linux/slab.h>
#include <linux/mlx4/qp.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/vmalloc.h>
#include <linux/tcp.h>
#include <linux/moduleparam.h>
#ifdef __VMKERNEL_MODULE__
#include <asm/unaligned.h>
#endif	/* __VMKERNEL_MODULE__ */

#include "mlx4_en.h"

enum {
	MAX_INLINE = 104, /* 128 - 16 - 4 - 4 */
	MAX_BF = 256,
};

static int inline_thold __read_mostly = MAX_INLINE;

module_param_named(inline_thold, inline_thold, int, 0444);
MODULE_PARM_DESC(inline_thold, "threshold for using inline data");

int mlx4_en_create_tx_ring(struct mlx4_en_priv *priv,
			   struct mlx4_en_tx_ring *ring, int qpn, u32 size,
			   u16 stride)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int tmp;
	int err;

	ring->size = size;
	ring->size_mask = size - 1;
	ring->stride = stride;

	inline_thold = min(inline_thold, MAX_INLINE);

	tmp = size * sizeof(struct mlx4_en_tx_info);
	ring->tx_info = vmalloc(tmp);
	if (!ring->tx_info)
		return -ENOMEM;

	en_dbg(DRV, priv, "Allocated tx_info ring at addr:%p size:%d\n",
		 ring->tx_info, tmp);

	ring->bounce_buf = kmalloc(MAX_DESC_SIZE, GFP_KERNEL);
	if (!ring->bounce_buf) {
		err = -ENOMEM;
		goto err_tx;
	}
	ring->buf_size = ALIGN(size * ring->stride, MLX4_EN_PAGE_SIZE);

	err = mlx4_alloc_hwq_res(mdev->dev, &ring->wqres, ring->buf_size,
				 2 * PAGE_SIZE);
	if (err) {
		en_err(priv, "Failed allocating hwq resources\n");
		goto err_bounce;
	}

	err = mlx4_en_map_buffer(&ring->wqres.buf);
	if (err) {
		en_err(priv, "Failed to map TX buffer\n");
		goto err_hwq_res;
	}

	ring->buf = ring->wqres.buf.direct.buf;

	en_dbg(DRV, priv, "Allocated TX ring (addr:%p) - buf:%p size:%d "
	       "buf_size:%d dma:%llx\n", ring, ring->buf, ring->size,
	       ring->buf_size, (unsigned long long) ring->wqres.buf.direct.map);

	ring->qpn = qpn;
	err = mlx4_qp_alloc(mdev->dev, ring->qpn, &ring->qp);
	if (err) {
		en_err(priv, "Failed allocating qp %d\n", ring->qpn);
		goto err_map;
	}
	ring->qp.event = mlx4_en_sqp_event;

#ifdef __VMKERNEL_BF_ENABLE__
	err = mlx4_bf_alloc(mdev->dev, &ring->bf);
	if (err) {
		en_dbg(DRV, priv, "working without blueflame (%d)", err);
		ring->bf.uar = &mdev->priv_uar;
		ring->bf.uar->map = mdev->uar_map;
		ring->bf_enabled = false;
	} else
		ring->bf_enabled = true;
#endif	/* __VMKERNEL_BF_ENABLE__ */

	return 0;

err_map:
	mlx4_en_unmap_buffer(&ring->wqres.buf);
err_hwq_res:
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size);
err_bounce:
	kfree(ring->bounce_buf);
	ring->bounce_buf = NULL;
err_tx:
	vfree(ring->tx_info);
	ring->tx_info = NULL;
	return err;
}

void mlx4_en_destroy_tx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_tx_ring *ring)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	en_dbg(DRV, priv, "Destroying tx ring, qpn: %d\n", ring->qpn);

#ifdef __VMKERNEL_BF_ENABLE__
	if (ring->bf_enabled)
		mlx4_bf_free(mdev->dev, &ring->bf);
#endif	/* __VMKERNEL_BF_ENABLE__ */
	mlx4_qp_remove(mdev->dev, &ring->qp);
	mlx4_qp_free(mdev->dev, &ring->qp);
	mlx4_qp_release_range(mdev->dev, ring->qpn, 1);
	mlx4_en_unmap_buffer(&ring->wqres.buf);
	mlx4_free_hwq_res(mdev->dev, &ring->wqres, ring->buf_size);
	kfree(ring->bounce_buf);
	ring->bounce_buf = NULL;
	vfree(ring->tx_info);
	ring->tx_info = NULL;
}

int mlx4_en_activate_tx_ring(struct mlx4_en_priv *priv,
			     struct mlx4_en_tx_ring *ring,
			     int cq)
{
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	ring->cqn = cq;
	ring->prod = 0;
	ring->cons = 0xffffffff;
	ring->last_nr_txbb = 1;
	ring->poll_cnt = 0;
	ring->blocked = 0;
	atomic_set(&ring->inflight, 0);
	memset(ring->tx_info, 0, ring->size * sizeof(struct mlx4_en_tx_info));
	memset(ring->buf, 0, ring->buf_size);

	ring->qp_state = MLX4_QP_STATE_RST;
	ring->doorbell_qpn = ring->qp.qpn << 8;

	mlx4_en_fill_qp_context(priv, ring->size, ring->stride, 1, 0, ring->qpn,
				ring->cqn, &ring->context);
#ifdef __VMKERNEL_BF_ENABLE__
	if (ring->bf_enabled)
		ring->context.usr_page = cpu_to_be32(ring->bf.uar->index);
#endif	/* __VMKERNEL_BF_ENABLE__ */

	err = mlx4_qp_to_ready(mdev->dev, &ring->wqres.mtt, &ring->context,
			       &ring->qp, &ring->qp_state);

	return err;
}

void mlx4_en_deactivate_tx_ring(struct mlx4_en_priv *priv,
				struct mlx4_en_tx_ring *ring)
{
	struct mlx4_en_dev *mdev = priv->mdev;

	mlx4_qp_modify(mdev->dev, NULL, ring->qp_state,
		       MLX4_QP_STATE_RST, NULL, 0, 0, &ring->qp);
}


static u32 mlx4_en_free_tx_desc(struct mlx4_en_priv *priv,
				struct mlx4_en_tx_ring *ring,
				int index, u8 owner)
{
	struct mlx4_en_tx_info *tx_info = &ring->tx_info[index];
	struct mlx4_en_tx_desc *tx_desc = ring->buf + index * TXBB_SIZE;
	struct mlx4_wqe_data_seg *data = (void *) tx_desc + tx_info->data_offset;
	struct sk_buff *skb = tx_info->skb;
	struct skb_frag_struct *frag;
	void *end = ring->buf + ring->buf_size;
	int frags = skb_shinfo(skb)->nr_frags;
	int i;
	__be32 *ptr = (__be32 *)tx_desc;
	__be32 stamp = cpu_to_be32(STAMP_VAL | (!!owner << STAMP_SHIFT));

	/* Optimize the common case when there are no wraparounds */
	if (likely((void *) tx_desc + tx_info->nr_txbb * TXBB_SIZE <= end)) {
		if (!tx_info->inl) {
			if (tx_info->linear) {
				dma_unmap_single(priv->ddev,
					(dma_addr_t) be64_to_cpu(data->addr),
					 be32_to_cpu(data->byte_count),
					 PCI_DMA_TODEVICE);
				++data;
			}

			for (i = 0; i < frags; i++) {
				frag = &skb_shinfo(skb)->frags[i];
				dma_unmap_page(priv->ddev,
					(dma_addr_t) be64_to_cpu(data[i].addr),
					skb_frag_size(frag), PCI_DMA_TODEVICE);
			}
		}
		/* Stamp the freed descriptor */
		for (i = 0; i < tx_info->nr_txbb * TXBB_SIZE; i += STAMP_STRIDE) {
			*ptr = stamp;
			ptr += STAMP_DWORDS;
		}

	} else {
		if (!tx_info->inl) {
			if ((void *) data >= end) {
				data = ring->buf + ((void *)data - end);
			}

			if (tx_info->linear) {
				dma_unmap_single(priv->ddev,
					(dma_addr_t) be64_to_cpu(data->addr),
					 be32_to_cpu(data->byte_count),
					 PCI_DMA_TODEVICE);
				++data;
			}

			for (i = 0; i < frags; i++) {
				/* Check for wraparound before unmapping */
				if ((void *) data >= end)
					data = ring->buf;
				frag = &skb_shinfo(skb)->frags[i];
				dma_unmap_page(priv->ddev,
					(dma_addr_t) be64_to_cpu(data->addr),
					 skb_frag_size(frag), PCI_DMA_TODEVICE);
				++data;
			}
		}
		/* Stamp the freed descriptor */
		for (i = 0; i < tx_info->nr_txbb * TXBB_SIZE; i += STAMP_STRIDE) {
			*ptr = stamp;
			ptr += STAMP_DWORDS;
			if ((void *) ptr >= end) {
				ptr = ring->buf;
				stamp ^= cpu_to_be32(0x80000000);
			}
		}

	}
	dev_kfree_skb(skb);
	return tx_info->nr_txbb;
}


int mlx4_en_free_tx_buf(struct net_device *dev, struct mlx4_en_tx_ring *ring)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int cnt = 0;

	/* Skip last polled descriptor */
	ring->cons += ring->last_nr_txbb;
	en_dbg(DRV, priv, "Freeing Tx buf - cons:0x%x prod:0x%x\n",
		 ring->cons, ring->prod);

	if ((u32) (ring->prod - ring->cons) > ring->size) {
		if (netif_msg_tx_err(priv))
			en_warn(priv, "Tx consumer passed producer!\n");
		return 0;
	}

	while (ring->cons != ring->prod) {
		ring->last_nr_txbb = mlx4_en_free_tx_desc(priv, ring,
						ring->cons & ring->size_mask,
						!!(ring->cons & ring->size));
		ring->cons += ring->last_nr_txbb;
		cnt++;
	}

	if (cnt)
		en_dbg(DRV, priv, "Freed %d uncompleted tx descriptors\n", cnt);

	return cnt;
}

int mlx4_en_process_tx_cq(struct net_device *dev,
			     struct mlx4_en_cq *cq,
			     int budget)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_cq *mcq = &cq->mcq;
	struct mlx4_en_tx_ring *ring = &priv->tx_ring[cq->ring];
	struct mlx4_cqe *cqe;
	u16 index;
	u16 new_index, ring_index;
	u32 txbbs_skipped = 0;
	u32 cons_index = mcq->cons_index;
	int size = cq->size;
	u32 size_mask = ring->size_mask;
	struct mlx4_cqe *buf = cq->buf;
	int factor = priv->cqe_factor;
	int done = 0;

	index = cons_index & size_mask;
	cqe = &buf[(index << factor) + factor];
	ring_index = ring->cons & size_mask;

	/* Process all completed CQEs */
	while (XNOR(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK,
			cons_index & size) && done < budget) {
		/*
		 * make sure we read the CQE after we read the
		 * ownership bit
		 */
		rmb();

		/* Skip over last polled CQE */
		new_index = be16_to_cpu(cqe->wqe_index) & size_mask;

		do {
			txbbs_skipped += ring->last_nr_txbb;
			ring_index = (ring_index + ring->last_nr_txbb) & size_mask;
			/* free next descriptor */
			ring->last_nr_txbb = mlx4_en_free_tx_desc(
					priv, ring, ring_index,
					!!((ring->cons + txbbs_skipped) &
							ring->size));
		} while ((++done < budget) && ring_index != new_index);

		++cons_index;
		index = cons_index & size_mask;
		cqe = &buf[(index << factor) + factor];
	}

	/*
	 * To prevent CQ overflow we first update CQ consumer and only then
	 * the ring consumer.
	 */
	mcq->cons_index = cons_index;
	mlx4_cq_set_ci(mcq);
	wmb();
	ring->cons += txbbs_skipped;
	atomic_sub(txbbs_skipped, &ring->inflight);

	/* Wakeup Tx queue if this ring stopped it */
	if (unlikely(ring->blocked && txbbs_skipped > 0)) {
		ring->blocked = 0;
#ifndef __VMKERNEL_MLX4_EN_TX_HASH__
		netif_tx_wake_queue(netdev_get_tx_queue(dev, cq->ring));
#else
		netif_tx_wake_queue(netdev_get_tx_queue(dev, ring->reported_index));
#endif	/* NOT __VMKERNEL_MLX4_EN_TX_HASH__ */
		priv->port_stats.wake_queue++;
	}
	return done;
}

void mlx4_en_tx_irq(struct mlx4_cq *mcq)
{
	struct mlx4_en_cq *cq = container_of(mcq, struct mlx4_en_cq, mcq);
	struct mlx4_en_priv *priv = netdev_priv(cq->dev);

	if (priv->port_up)
		napi_schedule(&cq->napi);
	else
		mlx4_en_arm_cq(priv, cq);
}

/* TX CQ polling - called by NAPI */
int mlx4_en_poll_tx_cq(struct napi_struct *napi, int budget)
{
	struct mlx4_en_cq *cq = container_of(napi, struct mlx4_en_cq, napi);
	struct net_device *dev = cq->dev;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int done;

	done = mlx4_en_process_tx_cq(dev, cq, budget);

	/* If we used up all the quota - we're probably not done yet... */
	if (done == budget)
		INC_PERF_COUNTER(priv->pstats.napi_quota_tx);
	else{
		/* Done for now */
		napi_complete(napi);
		mlx4_en_arm_cq(priv, cq);
	}
	return done;
}

static struct mlx4_en_tx_desc *mlx4_en_bounce_to_desc(struct mlx4_en_priv *priv,
						      struct mlx4_en_tx_ring *ring,
						      u32 index,
						      unsigned int desc_size)
{
	u32 copy = (ring->size - index) * TXBB_SIZE;
	int i;

	for (i = desc_size - copy - 4; i >= 0; i -= 4) {
		if ((i & (TXBB_SIZE - 1)) == 0)
			wmb();

		*((u32 *) (ring->buf + i)) =
			*((u32 *) (ring->bounce_buf + copy + i));
	}

	for (i = copy - 4; i >= 4 ; i -= 4) {
		if ((i & (TXBB_SIZE - 1)) == 0)
			wmb();

		*((u32 *) (ring->buf + index * TXBB_SIZE + i)) =
			*((u32 *) (ring->bounce_buf + i));
	}

	/* Return real descriptor location */
	return ring->buf + index * TXBB_SIZE;
}

#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
static int is_inline(struct sk_buff *skb, void **pfrag)
{
	void *ptr;

	if (inline_thold && !skb_is_gso(skb) && skb->len <= inline_thold) {
		if (skb_shinfo(skb)->nr_frags == 1) {
			ptr = skb_frag_address_safe(&skb_shinfo(skb)->frags[0]);
			if (unlikely(!ptr))
				return 0;

			if (pfrag)
				*pfrag = ptr;

			return 1;
		} else if (unlikely(skb_shinfo(skb)->nr_frags))
			return 0;
		else
			return 1;
	}

	return 0;
}

static int inline_size(struct sk_buff *skb)
{
	if (skb->len + CTRL_SIZE + sizeof(struct mlx4_wqe_inline_seg)
	    <= MLX4_INLINE_ALIGN)
		return ALIGN(skb->len + CTRL_SIZE +
			     sizeof(struct mlx4_wqe_inline_seg), 16);
	else
		return ALIGN(skb->len + CTRL_SIZE + 2 *
			     sizeof(struct mlx4_wqe_inline_seg), 16);
}
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */

static int get_real_size(struct sk_buff *skb, struct net_device *dev,
			 int *lso_header_size)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int real_size;

	if (skb_is_gso(skb)) {
		*lso_header_size = skb_transport_offset(skb) + tcp_hdrlen(skb);
		real_size = CTRL_SIZE + skb_shinfo(skb)->nr_frags * DS_SIZE +
			ALIGN(*lso_header_size + 4, DS_SIZE);
		if (unlikely(*lso_header_size != skb_headlen(skb))) {
			/* We add a segment for the skb linear buffer only if
			 * it contains data */
			if (*lso_header_size < skb_headlen(skb))
				real_size += DS_SIZE;
			else {
				if (netif_msg_tx_err(priv))
					en_warn(priv, "Non-linear headers\n");
				return 0;
			}
		}
	} else {
		*lso_header_size = 0;
#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
		if (!is_inline(skb, NULL))
			real_size = CTRL_SIZE + (skb_shinfo(skb)->nr_frags + 1) * DS_SIZE;
		else
			real_size = inline_size(skb);
#else
		real_size = CTRL_SIZE + (skb_shinfo(skb)->nr_frags + 1) * DS_SIZE;
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */
	}

	return real_size;
}

#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
static void build_inline_wqe(struct mlx4_en_tx_desc *tx_desc, struct sk_buff *skb,
			     int real_size, u16 *vlan_tag, int tx_ind, void *fragptr)
{
	struct mlx4_wqe_inline_seg *inl = &tx_desc->inl;
	int spc = MLX4_INLINE_ALIGN - CTRL_SIZE - sizeof *inl;

	if (skb->len <= spc) {
		inl->byte_count = cpu_to_be32(1 << 31 | skb->len);
		skb_copy_from_linear_data(skb, inl + 1, skb_headlen(skb));
		if (skb_shinfo(skb)->nr_frags)
			memcpy(((void *)(inl + 1)) + skb_headlen(skb), fragptr,
			       skb_frag_size(&skb_shinfo(skb)->frags[0]));

	} else {
		inl->byte_count = cpu_to_be32(1 << 31 | spc);
		if (skb_headlen(skb) <= spc) {
			skb_copy_from_linear_data(skb, inl + 1, skb_headlen(skb));
			if (skb_headlen(skb) < spc) {
				memcpy(((void *)(inl + 1)) + skb_headlen(skb),
					fragptr, spc - skb_headlen(skb));
				fragptr +=  spc - skb_headlen(skb);
			}
			inl = (void *) (inl + 1) + spc;
			memcpy(((void *)(inl + 1)), fragptr, skb->len - spc);
		} else {
			skb_copy_from_linear_data(skb, inl + 1, spc);
			inl = (void *) (inl + 1) + spc;
			skb_copy_from_linear_data_offset(skb, spc, inl + 1,
					skb_headlen(skb) - spc);
			if (skb_shinfo(skb)->nr_frags)
				memcpy(((void *)(inl + 1)) + skb_headlen(skb) - spc,
					fragptr, skb_frag_size(&skb_shinfo(skb)->frags[0]));
		}

		wmb();
		inl->byte_count = cpu_to_be32(1 << 31 | (skb->len - spc));
	}
	tx_desc->ctrl.vlan_tag = cpu_to_be16(*vlan_tag);
	tx_desc->ctrl.ins_vlan = MLX4_WQE_CTRL_INS_VLAN *
		(!!vlan_tx_tag_present(skb));
	tx_desc->ctrl.fence_size = (real_size / 16) & 0x3f;
}
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */

#ifdef __VMKERNEL_NETDEV_SELECT_QUEUE_SUPPORT__
u16 mlx4_en_select_queue(struct net_device *dev, struct sk_buff *skb)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	u16 vlan_tag = 0;

	/* If we support per priority flow control and the packet contains
	 * a vlan tag, send the packet to the TX ring assigned to that priority
	 */
	if (priv->prof->rx_ppp && vlan_tx_tag_present(skb)) {
		vlan_tag = vlan_tx_tag_get(skb);
		return MLX4_EN_NUM_TX_RINGS + (vlan_tag >> 13);
	}

	return skb_tx_hash(dev, skb);
}
#endif	/* __VMKERNEL_NETDEV_SELECT_QUEUE_SUPPORT__ */

#ifdef __VMKERNEL_BF_ENABLE__
static void mlx4_bf_copy(void __iomem *dst, unsigned long *src, unsigned bytecnt)
{
	__iowrite64_copy(dst, src, bytecnt / 8);
}
#endif	/* __VMKERNEL_BF_ENABLE__ */

#ifdef __VMKERNEL_MLX4_EN_TX_HASH__
static int mlx4_en_hash_select_ring(struct mlx4_en_priv *priv, int index, struct sk_buff *skb)
{
	struct mlx4_en_tx_ring *ring = &priv->tx_ring[index];
	int tx_ind = 0;
	struct tcphdr *th = tcp_hdr(skb);
	struct iphdr *iph = ip_hdr(skb);
	struct mlx4_en_tx_hash_entry *entry;
	u32 hash_index;

	/* Hashing is only done for TCP/IP or UDP/IP packets */
	if (be16_to_cpu(skb->protocol) != ETH_P_IP)
		return index;

	hash_index = be32_to_cpu(iph->daddr) & MLX4_EN_TX_HASH_MASK;
	switch(iph->protocol) {
	case IPPROTO_UDP:
		break;
	case IPPROTO_TCP:
		if (th) {
			hash_index = (hash_index ^ be16_to_cpu(th->dest ^ th->source)) &
					MLX4_EN_TX_HASH_MASK;
		}
		break;
	default:
		return index;
	}

	/*
	 * We have 16 rings and we have hash tables for every ring.
	 * For rings 0..7 the hash table is valid and for
	 * rings 8..15 the hash table not used.
	 */
	entry = &ring->tx_hash[hash_index];

	/*
	 * entry->cnt overflow every 256 times, so we
	 * are checking if need to move stream every 256 times
	 */
	if(unlikely(!entry->cnt)) {
		tx_ind = index;
		if ((entry->small_pkts << 1) > entry->big_pkts)
			/*
			 * Move small packets from ring X (0=<X=<7)
			 * to ring X + reported_tx_ring_num(8) ==>
			 * new ring is Y (8=<Y=<15)
			 */
			tx_ind += priv->reported_tx_ring_num;
		entry->small_pkts = entry->big_pkts = 0;
		entry->ring = tx_ind;
	}

	entry->cnt++;
	if (skb->len > MLX4_EN_SMALL_PKT_SIZE)
		entry->big_pkts++;
	else
		entry->small_pkts++;
	return entry->ring;
}
#endif	/* __VMKERNEL_MLX4_EN_TX_HASH__ */

netdev_tx_t mlx4_en_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_tx_ring *ring;
	struct mlx4_en_tx_desc *tx_desc;
	struct mlx4_wqe_data_seg *data;
	struct skb_frag_struct *frag;
	struct mlx4_en_tx_info *tx_info;
	struct ethhdr *ethh;
	int tx_ind = 0;
	int nr_txbb;
	int desc_size;
	int real_size;
	dma_addr_t dma;
	u32 index, bf_index;
	__be32 op_own;
	u16 vlan_tag = 0;
	int i;
	int lso_header_size;
#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
	void *fragptr;
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */
	bool bounce = false;

	if (!priv->port_up)
		goto tx_drop;

	real_size = get_real_size(skb, dev, &lso_header_size);
	if (unlikely(!real_size))
		goto tx_drop;

	/* Align descriptor to TXBB size */
	desc_size = ALIGN(real_size, TXBB_SIZE);
	nr_txbb = desc_size / TXBB_SIZE;
	if (unlikely(nr_txbb > MAX_DESC_TXBBS)) {
		if (netif_msg_tx_err(priv))
			en_warn(priv, "Oversized header or SG list\n");
		goto tx_drop;
	}

	tx_ind = skb->queue_mapping;
#ifdef __VMKERNEL_MLX4_EN_TX_HASH__
	tx_ind = mlx4_en_hash_select_ring(priv, tx_ind, skb);
#endif	/* __VMKERNEL_MLX4_EN_TX_HASH__ */
	ring = &priv->tx_ring[tx_ind];
	if (vlan_tx_tag_present(skb))
		vlan_tag = vlan_tx_tag_get(skb);

	/* Check available TXBBs And 2K spare for prefetch */
	if (unlikely(atomic_read(&ring->inflight) >
		     ring->size - HEADROOM - MAX_DESC_TXBBS)) {
		/* every full Tx ring stops queue */
#ifndef __VMKERNEL_MLX4_EN_TX_HASH__
		netif_tx_stop_queue(netdev_get_tx_queue(dev, tx_ind));
#else
		netif_tx_stop_queue(netdev_get_tx_queue(dev, ring->reported_index));
#endif	/* NOT __VMKERNEL_MLX4_EN_TX_HASH__ */
		ring->blocked = 1;
		priv->port_stats.queue_stopped++;

		return NETDEV_TX_BUSY;
	}

	/* Track current inflight packets for performance analysis */
	AVG_PERF_COUNTER(priv->pstats.inflight_avg,
			 (u32) (ring->prod - ring->cons - 1));

	/* Packet is good - grab an index and transmit it */
	index = ring->prod & ring->size_mask;
	bf_index = ring->prod;

	/* See if we have enough space for whole descriptor TXBB for setting
	 * SW ownership on next descriptor; if not, use a bounce buffer. */
	if (likely(index + nr_txbb <= ring->size))
		tx_desc = ring->buf + index * TXBB_SIZE;
	else {
		tx_desc = (struct mlx4_en_tx_desc *) ring->bounce_buf;
		bounce = true;
	}

	/* Save skb in tx_info ring */
	tx_info = &ring->tx_info[index];
	tx_info->skb = skb;
	tx_info->nr_txbb = nr_txbb;

	/* Prepare ctrl segement apart opcode+ownership, which depends on
	 * whether LSO is used */
	tx_desc->ctrl.vlan_tag = cpu_to_be16(vlan_tag);
	tx_desc->ctrl.ins_vlan = MLX4_WQE_CTRL_INS_VLAN *
		!!vlan_tx_tag_present(skb);
	tx_desc->ctrl.fence_size = (real_size / 16) & 0x3f;
	tx_desc->ctrl.srcrb_flags = priv->ctrl_flags;
	if (likely(skb->ip_summed == CHECKSUM_PARTIAL)) {
		tx_desc->ctrl.srcrb_flags |= cpu_to_be32(MLX4_WQE_CTRL_IP_CSUM |
							 MLX4_WQE_CTRL_TCP_UDP_CSUM);
		ring->tx_csum++;
	}

	/* Copy dst mac address to wqe */
	ethh = (struct ethhdr *)skb->data;
	tx_desc->ctrl.srcrb_flags16[0] = get_unaligned((__be16 *)ethh->h_dest);
	tx_desc->ctrl.imm = get_unaligned((__be32 *)(ethh->h_dest + 2));
	/* Handle LSO (TSO) packets */
	if (lso_header_size) {
		/* Mark opcode as LSO */
		op_own = cpu_to_be32(MLX4_OPCODE_LSO | (1 << 6)) |
			((ring->prod & ring->size) ?
				cpu_to_be32(MLX4_EN_BIT_DESC_OWN) : 0);

		/* Fill in the LSO prefix */
		tx_desc->lso.mss_hdr_size = cpu_to_be32(
			skb_shinfo(skb)->gso_size << 16 | lso_header_size);

		/* Copy headers;
		 * note that we already verified that it is linear */
		memcpy(tx_desc->lso.header, skb->data, lso_header_size);
		data = ((void *) &tx_desc->lso +
			ALIGN(lso_header_size + 4, DS_SIZE));

		priv->port_stats.tso_packets++;
		i = ((skb->len - lso_header_size) / skb_shinfo(skb)->gso_size) +
			!!((skb->len - lso_header_size) % skb_shinfo(skb)->gso_size);
		ring->bytes += skb->len + (i - 1) * lso_header_size;
		ring->packets += i;
	} else {
		/* Normal (Non LSO) packet */
		op_own = cpu_to_be32(MLX4_OPCODE_SEND) |
			((ring->prod & ring->size) ?
			 cpu_to_be32(MLX4_EN_BIT_DESC_OWN) : 0);
		data = &tx_desc->data;
		ring->bytes += max(skb->len, (unsigned int) ETH_ZLEN);
		ring->packets++;

	}
	AVG_PERF_COUNTER(priv->pstats.tx_pktsz_avg, skb->len);


	/* valid only for none inline segments */
	tx_info->data_offset = (void *) data - (void *) tx_desc;

#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
	tx_info->linear = (lso_header_size < skb_headlen(skb) && !is_inline(skb, NULL)) ? 1 : 0;
#else
	tx_info->linear = (lso_header_size < skb_headlen(skb)) ? 1 : 0;
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */
	data += skb_shinfo(skb)->nr_frags + tx_info->linear - 1;

#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
	if (!is_inline(skb, &fragptr)) {
#else
	{
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */
		/* Map fragments */
		for (i = skb_shinfo(skb)->nr_frags - 1; i >= 0; i--) {
			frag = &skb_shinfo(skb)->frags[i];
			dma = skb_frag_dma_map(priv->ddev, frag,
					       0, skb_frag_size(frag),
					       DMA_TO_DEVICE);
#ifdef __VMKERNEL_MODULE__
			if (unlikely(dma_mapping_error(dma)))
#else
			if (unlikely(dma_mapping_error(priv->ddev, dma)))
#endif	/* __VMKERNEL_MODULE__ */
				goto err_dma;
			data->addr = cpu_to_be64(dma);
			data->lkey = cpu_to_be32(mdev->mr.key);
			wmb();
			data->byte_count = cpu_to_be32(skb_frag_size(frag));
			--data;
		}

		/* Map linear part */
		if (tx_info->linear) {
			dma = dma_map_single(priv->ddev, skb->data + lso_header_size,
					     skb_headlen(skb) - lso_header_size, PCI_DMA_TODEVICE);
#ifdef __VMKERNEL_MODULE__
			if (unlikely(dma_mapping_error(dma)))
#else
			if (unlikely(dma_mapping_error(priv->ddev, dma)))
#endif	/* __VMKERNEL_MODULE__ */
				goto err_dma;
			data->addr = cpu_to_be64(dma);
			data->lkey = cpu_to_be32(mdev->mr.key);
			wmb();
			data->byte_count = cpu_to_be32(skb_headlen(skb) - lso_header_size);
		}
		tx_info->inl = 0;
#ifdef __VMKERNEL_TX_INLINE_WQE_ENABLE__
	} else {
		build_inline_wqe(tx_desc, skb, real_size, &vlan_tag, tx_ind, fragptr);
		tx_info->inl = 1;
	}
#else
	}
#endif	/* __VMKERNEL_TX_INLINE_WQE_ENABLE__ */


	ring->prod += nr_txbb;
	atomic_add(nr_txbb, &ring->inflight);

	/* If we used a bounce buffer then copy descriptor back into place */
	if (bounce)
		tx_desc = mlx4_en_bounce_to_desc(priv, ring, index, desc_size);

/*
 * We can not use the following optimization
 * in vmkernel because skb_orphan is missing
 */
#ifndef __VMKERNEL_MODULE__
	/* Run destructor before passing skb to HW */
	if (likely(!skb_shared(skb)))
		skb_orphan(skb);
#endif	/* __VMKERNEL_MODULE__ */

#ifdef __VMKERNEL_BF_ENABLE__
	if (ring->bf_enabled && desc_size <= MAX_BF && !bounce && !vlan_tag) {
		*(__be32 *) (&tx_desc->ctrl.vlan_tag) |= cpu_to_be32(ring->doorbell_qpn);
		op_own |= htonl((bf_index & 0xffff) << 8);
		/* Ensure new descirptor hits memory
		* before setting ownership of this descriptor to HW */
		wmb();
		tx_desc->ctrl.owner_opcode = op_own;

		wmb();

		mlx4_bf_copy(ring->bf.reg + ring->bf.offset, (unsigned long *) &tx_desc->ctrl,
		     desc_size);

		wmb();

		ring->bf.offset ^= ring->bf.buf_size;
	} else {
#else
	{
#endif	/* __VMKERNEL_BF_ENABLE__ */
		/* Ensure new descirptor hits memory
		* before setting ownership of this descriptor to HW */
		wmb();
		tx_desc->ctrl.owner_opcode = op_own;
		wmb();
#ifdef __VMKERNEL_BF_ENABLE__
		iowrite32be(ring->doorbell_qpn, ring->bf.uar->map + MLX4_SEND_DOORBELL);
#else
		iowrite32be(ring->doorbell_qpn, mdev->uar_map + MLX4_SEND_DOORBELL);
#endif	/* __VMKERNEL_BF_ENABLE__ */

#ifdef __VMKERNEL_MODULE__
		dev->trans_start = jiffies;
#endif	/* __VMKERNEL_MODULE__ */
	}

	return NETDEV_TX_OK;

err_dma:
	while (i++ < skb_shinfo(skb)->nr_frags - 1) {
		++data;
		frag = &skb_shinfo(skb)->frags[i];
		dma_unmap_page(priv->ddev, (dma_addr_t) be64_to_cpu(data[i].addr),
				frag->size, PCI_DMA_TODEVICE);
	}
tx_drop:
	dev_kfree_skb_any(skb);
	priv->stats.tx_dropped++;
	return NETDEV_TX_OK;
}

