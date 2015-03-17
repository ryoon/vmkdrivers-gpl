/*
 * Copyright (c) 2012-2013 Mellanox Technologies. All rights reserved.
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

#include <linux/mlx4/cq.h>
#include <linux/slab.h>
#include <linux/mlx4/qp.h>
#include <linux/skbuff.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/vmalloc.h>

#include "mlx4_en.h"

static int mlx4_en_alloc_rx_skb(struct mlx4_en_priv *priv,
		struct mlx4_en_rx_desc *rx_desc, struct sk_buff **pskb,
		int unmap)
{
	dma_addr_t dma;
	struct sk_buff *new_skb = netdev_alloc_skb(priv->dev,
			priv->rx_skb_size + NET_IP_ALIGN);

	if (unlikely(new_skb == NULL))
		return -ENOMEM;

	skb_reserve(new_skb, NET_IP_ALIGN);
	dma = dma_map_single(priv->ddev, new_skb->data, priv->rx_skb_size,
				DMA_FROM_DEVICE);
	if (unlikely(dma_mapping_error(dma))) {
		en_err(priv, "Failed dma mapping page for RX buffer\n");
		dev_kfree_skb(new_skb);
		return -EFAULT;
	}

	if (unmap)
		dma_unmap_single(priv->ddev, be64_to_cpu(rx_desc->data->addr),
				be32_to_cpu(rx_desc->data->byte_count),
				DMA_FROM_DEVICE);

	*pskb = new_skb;
	rx_desc->data->addr = cpu_to_be64(dma);
	return 0;
}

void mlx4_en_init_rx_desc_skb(struct mlx4_en_priv *priv,
		struct mlx4_en_rx_ring *ring, int index)
{
	struct mlx4_en_rx_desc *rx_desc = ring->buf + ring->stride * index;

	rx_desc->data->byte_count = cpu_to_be32(priv->rx_skb_size);
	rx_desc->data->lkey = cpu_to_be32(priv->mdev->mr.key);
}

int mlx4_en_prepare_rx_desc_skb(struct mlx4_en_priv *priv,
		struct mlx4_en_rx_ring *ring, int index)
{
	struct mlx4_en_rx_desc *rx_desc = ring->buf + (index * ring->stride);
	struct sk_buff **pskb = (struct sk_buff **) ring->rx_info + index;

	return mlx4_en_alloc_rx_skb(priv, rx_desc, pskb, 0);
}

static inline void mlx4_en_update_rx_prod_db(struct mlx4_en_rx_ring *ring)
{
	*ring->wqres.db.db = cpu_to_be32(ring->prod & 0xffff);
}

void mlx4_en_free_rx_desc_skb(struct mlx4_en_priv *priv,
		struct mlx4_en_rx_ring *ring, int index)
{
	struct sk_buff *skb;
	struct mlx4_en_rx_desc *rx_desc = ring->buf + (index
			<< ring->log_stride);
	dma_addr_t dma;

	skb = *((struct sk_buff **) ring->rx_info + index);
	dma = be64_to_cpu(rx_desc->data->addr);
	dma_unmap_single(priv->ddev, dma, priv->rx_skb_size, DMA_FROM_DEVICE);
	dev_kfree_skb(skb);
}

static struct sk_buff *
mlx4_en_get_rx_skb(struct mlx4_en_priv *priv, struct mlx4_en_rx_desc *rx_desc,
		struct sk_buff **pskb, unsigned int length)
{
	struct sk_buff *skb;
	dma_addr_t dma;

	if (length <= SMALL_PACKET_SIZE) {
		skb = netdev_alloc_skb(priv->dev, length + NET_IP_ALIGN);
		if (unlikely(!skb))
			return NULL;

		skb->dev = priv->dev;
		skb_reserve(skb, NET_IP_ALIGN);
		/* We are copying all relevant data to the skb - temporarily
		 * synch buffers for the copy */
		dma = be64_to_cpu(rx_desc->data->addr);
		dma_sync_single_for_cpu(priv->ddev, dma, priv->rx_skb_size,
				DMA_FROM_DEVICE);
		skb_copy_to_linear_data(skb, (*pskb)->data, length);
		dma_sync_single_for_device(priv->ddev, dma, priv->rx_skb_size,
				DMA_FROM_DEVICE);

	} else {
		skb = *pskb;
		if (unlikely(mlx4_en_alloc_rx_skb(priv, rx_desc, pskb, 1)))
			return NULL;
	}

	skb->tail += length;
	skb->len = length;
	skb->truesize = length + sizeof(struct sk_buff);
	return skb;
}

int mlx4_en_process_rx_cq_skb(struct net_device *dev, struct mlx4_en_cq *cq,
		int budget)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_cqe *cqe;
	struct mlx4_en_rx_ring *ring = &priv->rx_ring[cq->ring];
	struct sk_buff **pskb;
	struct sk_buff *skb;
	struct mlx4_en_rx_desc *rx_desc;
	int index;
	unsigned int length;
	int polled = 0;
	int ip_summed;
	struct ethhdr *ethh;
	u64 s_mac;
	int factor = priv->cqe_factor;

	/* We assume a 1:1 mapping between CQEs and Rx descriptors, so Rx
	 * descriptor offset can be deduced from the CQE index instead of
	 * reading 'cqe->index' */
	index = cq->mcq.cons_index & ring->size_mask;
	cqe = &cq->buf[(index << factor) + factor];

	/* Process all completed CQEs */
	while (XNOR(cqe->owner_sr_opcode & MLX4_CQE_OWNER_MASK,
			cq->mcq.cons_index & cq->size)) {

		pskb = (struct sk_buff **) ring->rx_info + index;
		rx_desc = ring->buf + (index << ring->log_stride);

		/*
		 * make sure we read the CQE after we read the ownership bit
		 */
		rmb();

		/* Drop packet on bad receive or bad checksum */
		if (unlikely((cqe->owner_sr_opcode & MLX4_CQE_OPCODE_MASK) ==
				MLX4_CQE_OPCODE_ERROR)) {
			en_err(priv, "CQE completed in error - vendor "
					"syndrom:%d syndrom:%d\n",
					((struct mlx4_err_cqe *) cqe)->vendor_err_syndrome,
					((struct mlx4_err_cqe *) cqe)->syndrome);
			goto next;
		}
		if (unlikely(cqe->badfcs_enc & MLX4_CQE_BAD_FCS)) {
			en_dbg(RX_ERR, priv, "Accepted frame with bad FCS\n");
			goto next;
		}

		length = be32_to_cpu(cqe->byte_cnt);
		length -= ring->fcs_del;

		skb = mlx4_en_get_rx_skb(priv, rx_desc, pskb, length);
		if (unlikely(!skb)) {
			priv->stats.rx_dropped++;
			goto next;
		}

		/* Get pointer to first fragment since we haven't skb yet and
		 * cast it to ethhdr struct */
		ethh = (struct ethhdr *) skb->data;
		s_mac = mlx4_en_mac_to_u64(ethh->h_source);

		/* If source MAC is equal to our own MAC and not performing
		 * the selftest */
#ifdef __VMKERNEL_ETHTOOL_SELF_TEST_SUPPORT__
		if (s_mac == priv->mac && !priv->validate_loopback) {
#else
		if (s_mac == priv->mac) {
#endif	/* __VMKERNEL_ETHTOOL_SELF_TEST_SUPPORT__ */
			/* skb is not going up to OS, free it here */
			dev_kfree_skb_any(skb);
			goto next;
		}

		/*
		 * Packet is OK - process it.
		 */
		ring->bytes += length;
		ring->packets++;

		if (priv->rx_csum &&
				(cqe->status & cpu_to_be16(MLX4_CQE_STATUS_IPOK)) &&
				(cqe->checksum == cpu_to_be16(0xffff))) {
			ring->csum_ok++;
			ip_summed = CHECKSUM_UNNECESSARY;
		} else {
			ip_summed = CHECKSUM_NONE;
			ring->csum_none++;
		}

#ifdef __VMKERNEL_ETHTOOL_SELF_TEST_SUPPORT__
		if (unlikely(priv->validate_loopback)) {
			validate_loopback(priv, skb);
			goto next;
		}
#endif	/* __VMKERNEL_ETHTOOL_SELF_TEST_SUPPORT__ */

		skb->ip_summed = ip_summed;
		skb->protocol = eth_type_trans(skb, dev);
		if (priv->netq)
			skb_record_rx_queue(skb, cq->ring);

#ifndef __VMKERNEL_MODULE__
		if (be32_to_cpu(cqe->vlan_my_qpn) & MLX4_CQE_VLAN_PRESENT_MASK)
			__vlan_hwaccel_put_tag(skb, be16_to_cpu(cqe->sl_vid));

		/* Push it up the stack */
		netif_receive_skb(skb);
#else
		if (be32_to_cpu(cqe->vlan_my_qpn) & MLX4_CQE_VLAN_PRESENT_MASK)
			vlan_hwaccel_receive_skb(skb, NULL, be16_to_cpu(cqe->sl_vid));
		else
			netif_receive_skb(skb);
#endif  /* __VMKERNEL_MODULE__ */

next:
		++cq->mcq.cons_index;
		index = (cq->mcq.cons_index) & ring->size_mask;
		cqe = &cq->buf[(index << factor) + factor];
		if (++polled == budget) {
			/* We are here because we reached the NAPI budget -
			 * flush only pending LRO sessions */
			goto out;
		}
	}

out:
	AVG_PERF_COUNTER(priv->pstats.rx_coal_avg, polled);
	mlx4_cq_set_ci(&cq->mcq);
	wmb();
	/* ensure HW sees CQ consumer before we post new buffers */
	ring->cons = cq->mcq.cons_index;
	ring->prod += polled; /* Polled descriptors were realocated in place */
	mlx4_en_update_rx_prod_db(ring);
	return polled;
}

/* Rx CQ polling - called by NAPI */
int mlx4_en_poll_rx_cq_skb(struct napi_struct *napi, int budget)
{
	struct mlx4_en_cq *cq = container_of(napi, struct mlx4_en_cq, napi);
	struct net_device *dev = cq->dev;
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int done;

	done = mlx4_en_process_rx_cq_skb(dev, cq, budget);

	/* If we used up all the quota - we're probably not done yet... */
	if (done == budget)
		INC_PERF_COUNTER(priv->pstats.napi_quota_rx);
	else {
		/* Done for now */
		napi_complete(napi);
		mlx4_en_arm_cq(priv, cq);
	}
	return done;
}
