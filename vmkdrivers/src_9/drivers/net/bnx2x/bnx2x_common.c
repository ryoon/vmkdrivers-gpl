/* bnx2x_common.c: Broadcom Everest network driver.
 *
 * Copyright (c) 2007-2011 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation.
 *
 * Maintained by: Eilon Greenstein <eilong@broadcom.com>
 * Written by: Eliezer Tamir
 * Based on code from Michael Chan's bnx2 driver
 * UDP CSUM errata workaround by Arik Gendelman
 * Slowpath and fastpath rework by Vladislav Zolotarov
 * Statistics and Link management by Yitchak Gertner
 *
 */
#include <linux/version.h>
#include <linux/module.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/moduleparam.h>
#endif
#include <linux/kernel.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/device.h>  /* for dev_info() */
#endif
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/dma-mapping.h>
#endif
#include <linux/bitops.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <asm/byteorder.h>
#include <linux/time.h>
#include <linux/ethtool.h>
#include <linux/mii.h>
#include <linux/if_vlan.h>
#include <net/ip.h>
#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#include <net/ipv6.h>
#else
#include <linux/ipv6.h>
#endif
#include <net/tcp.h>
#include <net/checksum.h>
#if (LINUX_VERSION_CODE > 0x020607) /* BNX2X_UPSTREAM */
#include <net/ip6_checksum.h>
#endif
#if (LINUX_VERSION_CODE >= 0x020600) /* BNX2X_UPSTREAM */
#include <linux/workqueue.h>
#endif
#include <linux/crc32.h>
#if (LINUX_VERSION_CODE >= 0x02061b) && !defined(BNX2X_DRIVER_DISK) && !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
#include <linux/crc32c.h>
#endif
#include <linux/prefetch.h>
#include <linux/zlib.h>
#if (LINUX_VERSION_CODE >= 0x020618) /* BNX2X_UPSTREAM */
#include <linux/io.h>
#else
#include <asm/io.h>
#endif
#if defined(BNX2X_UPSTREAM) && !defined(BNX2X_USE_INIT_VALUES) /* BNX2X_UPSTREAM */
#include <linux/stringify.h>
#endif

#if (LINUX_VERSION_CODE < 0x020600) /* ! BNX2X_UPSTREAM */
#define __NO_TPA__		1
#endif

#include "bnx2x.h"
#include "bnx2x_init.h"
#include "bnx2x_dump.h"
#include "bnx2x_common.h"

#if defined(__VMKLNX__) /* ! BNX2X_UPSTREAM */
#include "bnx2x_esx.h"
#endif

#ifdef BCM_IOV /* ! BNX2X_UPSTREAM */
#include "bnx2x_vf.h"
#endif

/**
 * zeros the contents of the bp->fp[index].
 * Makes sure the contents of the bp->fp[index]. napi is kept
 * intact.
 *
 * @param bp
 * @param index
 */
static inline void bnx2x_bz_fp(struct bnx2x *bp, int index)
{
	struct bnx2x_fastpath *fp = &bp->fp[index];
#if defined(BNX2X_NEW_NAPI) || defined(USE_NAPI_GRO) /* BNX2X_UPSTREAM */
	struct napi_struct orig_napi = fp->napi;
#endif
#if !defined(BNX2X_NEW_NAPI) /* !BNX2X_UPSTREAM */
	struct net_device orig_netdev = fp->dummy_netdev;
#endif
	/* bzero bnx2x_fastpath contents */
	memset(fp, 0, sizeof(*fp));

#if defined(BNX2X_NEW_NAPI) || defined(USE_NAPI_GRO) /* BNX2X_UPSTREAM */
	/* Restore the NAPI object as it has been already initialized */
	fp->napi = orig_napi;
#endif
#if !defined(BNX2X_NEW_NAPI) /* !BNX2X_UPSTREAM */
	/* Restore the "dummy" netdev object as it has been already
	 * initialized.
	 */
	fp->dummy_netdev = orig_netdev;
#endif
}

/**
 * Moves the contents of the bp->fp[from] to the bp->fp[to].
 * Makes sure the contents of the bp->fp[to].napi is kept
 * intact.
 *
 * @param bp
 * @param from
 * @param to
 */
static inline void bnx2x_move_fp(struct bnx2x *bp, int from, int to)
{
	struct bnx2x_fastpath *from_fp = &bp->fp[from];
	struct bnx2x_fastpath *to_fp = &bp->fp[to];
#if defined(BNX2X_NEW_NAPI) || defined(USE_NAPI_GRO) /* BNX2X_UPSTREAM */
	struct napi_struct orig_napi = to_fp->napi;
#endif
#if !defined(BNX2X_NEW_NAPI) /* !BNX2X_UPSTREAM */
	struct net_device orig_netdev = to_fp->dummy_netdev;
#endif
	/* Move bnx2x_fastpath contents */
	memcpy(to_fp, from_fp, sizeof(*to_fp));
	to_fp->index = to;

#if defined(BNX2X_NEW_NAPI) || defined(USE_NAPI_GRO) /* BNX2X_UPSTREAM */
	/* Restore the NAPI object as it has been already initialized */
	to_fp->napi = orig_napi;
#endif
#if !defined(BNX2X_NEW_NAPI) /* !BNX2X_UPSTREAM */
	/* Restore the "dummy" netdev object as it has been already
	 * initialized.
	 */
	to_fp->dummy_netdev = orig_netdev;
#endif
}

int load_count[2][3] = {{0}}; /* per-path: 0-common, 1-port0, 2-port1 */
extern int disable_tpa;

int bnx2x_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct bnx2x *bp;

	if (!dev) {
		dev_err(&pdev->dev, "BAD net device from bnx2x_init_one\n");
		return -ENODEV;
	}
	bp = netdev_priv(dev);

	rtnl_lock();

#if (LINUX_VERSION_CODE >= 0x02060b) /* BNX2X_UPSTREAM */
	pci_save_state(pdev);
#else
	pci_save_state(pdev, bp->pci_state);
#endif

	if (!netif_running(dev)) {
		rtnl_unlock();
		return 0;
	}

#if (LINUX_VERSION_CODE < 0x020618) /* ! BNX2X_UPSTREAM */
	flush_scheduled_work();
#endif
	netif_device_detach(dev);

	bnx2x_nic_unload(bp, UNLOAD_CLOSE);

	bnx2x_set_power_state(bp, pci_choose_state(pdev, state));

	rtnl_unlock();

	return 0;
}

int bnx2x_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct bnx2x *bp;
	int rc;

	if (!dev) {
		dev_err(&pdev->dev, "BAD net device from bnx2x_init_one\n");
		return -ENODEV;
	}
	bp = netdev_priv(dev);

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		netdev_err(dev, "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	rtnl_lock();

#if (LINUX_VERSION_CODE >= 0x02060b) /* BNX2X_UPSTREAM */
	pci_restore_state(pdev);
#else
	pci_restore_state(pdev, bp->pci_state);
#endif

	if (!netif_running(dev)) {
		rtnl_unlock();
		return 0;
	}

	bnx2x_set_power_state(bp, PCI_D0);
	netif_device_attach(dev);

	/* Since the chip was reset, clear the FW sequence number */
	bp->fw_seq = 0;
	rc = bnx2x_nic_load(bp, LOAD_OPEN);

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
	/* Invalidate netqueue state as filters have been lost after reinit */
	vmknetddi_queueops_invalidate_state(dev);
#endif
	rtnl_unlock();

	return rc;
}


/* free skb in the packet ring at pos idx
 * return idx of last bd freed
 */
static u16 bnx2x_free_tx_pkt(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			     u16 idx)
{
	struct sw_tx_bd *tx_buf = &fp->tx_buf_ring[idx];
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd;
	struct sk_buff *skb = tx_buf->skb;
	u16 bd_idx = TX_BD(tx_buf->first_bd), new_cons;
	int nbd;

	/* prefetch skb end pointer to speedup dev_kfree_skb() */
	prefetch(&skb->end);

	DP(BNX2X_MSG_OFF, "pkt_idx %d  buff @(%p)->skb %p\n",
	   idx, tx_buf, skb);

	/* unmap first bd */
	DP(BNX2X_MSG_OFF, "free bd_idx %d\n", bd_idx);
	tx_start_bd = &fp->tx_desc_ring[bd_idx].start_bd;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	dma_unmap_single(&bp->pdev->dev, BD_UNMAP_ADDR(tx_start_bd),
			 BD_UNMAP_LEN(tx_start_bd), DMA_TO_DEVICE);
#else
	pci_unmap_single(bp->pdev, BD_UNMAP_ADDR(tx_start_bd),
			 BD_UNMAP_LEN(tx_start_bd), PCI_DMA_TODEVICE);
#endif


	nbd = le16_to_cpu(tx_start_bd->nbd) - 1;
#ifdef BNX2X_STOP_ON_ERROR
	if ((nbd - 1) > (MAX_SKB_FRAGS + 2)) {
		BNX2X_ERR("BAD nbd!\n");
		bnx2x_panic();
	}
#endif
	new_cons = nbd + tx_buf->first_bd;

	/* Get the next bd */
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* Skip a parse bd... */
	--nbd;
	bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));

	/* ...and the TSO split header bd since they have no mapping */
	if (tx_buf->flags & BNX2X_TSO_SPLIT_BD) {
		--nbd;
		bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));
	}

	/* now free frags */
	while (nbd > 0) {

		DP(BNX2X_MSG_OFF, "free frag bd_idx %d\n", bd_idx);
		tx_data_bd = &fp->tx_desc_ring[bd_idx].reg_bd;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
		dma_unmap_page(&bp->pdev->dev, BD_UNMAP_ADDR(tx_data_bd),
				 BD_UNMAP_LEN(tx_data_bd), DMA_TO_DEVICE);
#else
		pci_unmap_page(bp->pdev, BD_UNMAP_ADDR(tx_data_bd),
			       BD_UNMAP_LEN(tx_data_bd), PCI_DMA_TODEVICE);
#endif
		if (--nbd)
			bd_idx = TX_BD(NEXT_TX_IDX(bd_idx));
	}

	/* release skb */
	WARN_ON(!skb);
	dev_kfree_skb(skb);
	tx_buf->first_bd = 0;
	tx_buf->skb = NULL;

	return new_cons;
}

int bnx2x_tx_int(struct bnx2x_fastpath *fp)
{
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
	struct netdev_queue *txq;
#endif
	struct bnx2x *bp = fp->bp;
	u16 hw_cons, sw_cons, bd_cons = fp->tx_bd_cons;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return -1;
#endif

#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
	txq = netdev_get_tx_queue(bp->dev, fp->index);
#endif
	hw_cons = le16_to_cpu(*fp->tx_cons_sb);
	sw_cons = fp->tx_pkt_cons;

	while (sw_cons != hw_cons) {
		u16 pkt_cons;

		pkt_cons = TX_BD(sw_cons);

		DP(NETIF_MSG_TX_DONE, "queue[%d]: hw_cons %u  sw_cons %u "
				      " pkt_cons %u\n",
		   fp->index, hw_cons, sw_cons, pkt_cons);

		bd_cons = bnx2x_free_tx_pkt(bp, fp, pkt_cons);
		sw_cons++;
	}

	fp->tx_pkt_cons = sw_cons;
	fp->tx_bd_cons = bd_cons;

	/* Need to make the tx_bd_cons update visible to start_xmit()
	 * before checking for netif_tx_queue_stopped().  Without the
	 * memory barrier, there is a small possibility that
	 * start_xmit() will miss it and cause the queue to be stopped
	 * forever.
	 * On the other hand we need an rmb() here to ensure the proper
	 * ordering of bit testing in the following
	 * netif_tx_queue_stopped(txq) call.
	 */
	smp_mb();

#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
	if (unlikely(netif_tx_queue_stopped(txq))) {

		/* Taking tx_lock() is needed to prevent reenabling the queue
		 * while it's empty. This could have happen if rx_action() gets
		 * suspended in bnx2x_tx_int() after the condition before
		 * netif_tx_wake_queue(), while tx_action (bnx2x_start_xmit()):
		 *
		 * stops the queue->sees fresh tx_bd_cons->releases the queue->
		 * sends some packets consuming the whole queue again->
		 * stops the queue
		 */

		__netif_tx_lock(txq, smp_processor_id());

		if ((netif_tx_queue_stopped(txq)) &&
		    (bp->state == BNX2X_STATE_OPEN) &&
		    (bnx2x_tx_avail(fp) >= MAX_SKB_FRAGS + 3))
			netif_tx_wake_queue(txq);

		__netif_tx_unlock(txq);
	}
#else
	if (unlikely(netif_queue_stopped(bp->dev))) {

		netif_tx_lock(bp->dev);

		if (netif_queue_stopped(bp->dev) &&
		    (bp->state == BNX2X_STATE_OPEN) &&
		    (bnx2x_tx_avail(fp) >= MAX_SKB_FRAGS + 3))
			netif_wake_queue(bp->dev);

		netif_tx_unlock(bp->dev);
	}
#endif
	return 0;
}

#if !defined(__NO_TPA__) /* BNX2X_UPSTREAM */
static inline void bnx2x_update_last_max_sge(struct bnx2x_fastpath *fp,
					     u16 idx)
{
	u16 last_max = fp->last_max_sge;

	if (SUB_S16(idx, last_max) > 0)
		fp->last_max_sge = idx;
}

static void bnx2x_update_sge_prod(struct bnx2x_fastpath *fp,
				  struct eth_fast_path_rx_cqe *fp_cqe)
{
	struct bnx2x *bp = fp->bp;
	u16 sge_len = SGE_PAGE_ALIGN(le16_to_cpu(fp_cqe->pkt_len) -
				     le16_to_cpu(fp_cqe->len_on_bd)) >>
		      SGE_PAGE_SHIFT;
	u16 last_max, last_elem, first_elem;
	u16 delta = 0;
	u16 i;

	if (!sge_len)
		return;

	/* First mark all used pages */
	for (i = 0; i < sge_len; i++)
		BIT_VEC64_CLEAR_BIT(fp->sge_mask,
			RX_SGE(le16_to_cpu(fp_cqe->sgl_or_raw_data.sgl[i])));

	DP(NETIF_MSG_RX_STATUS, "fp_cqe->sgl[%d] = %d\n",
	   sge_len - 1, le16_to_cpu(fp_cqe->sgl_or_raw_data.sgl[sge_len - 1]));

	/* Here we assume that the last SGE index is the biggest */
	prefetch((void *)(fp->sge_mask));
	bnx2x_update_last_max_sge(fp,
		le16_to_cpu(fp_cqe->sgl_or_raw_data.sgl[sge_len - 1]));

	last_max = RX_SGE(fp->last_max_sge);
	last_elem = last_max >> BIT_VEC64_ELEM_SHIFT;
	first_elem = RX_SGE(fp->rx_sge_prod) >> BIT_VEC64_ELEM_SHIFT;

	/* If ring is not full */
	if (last_elem + 1 != first_elem)
		last_elem++;

	/* Now update the prod */
	for (i = first_elem; i != last_elem; i = NEXT_SGE_MASK_ELEM(i)) {
		if (likely(fp->sge_mask[i]))
			break;

		fp->sge_mask[i] = BIT_VEC64_ELEM_ONE_MASK;
		delta += BIT_VEC64_ELEM_SZ;
	}

	if (delta > 0) {
		fp->rx_sge_prod += delta;
		/* clear page-end entries */
		bnx2x_clear_sge_mask_next_elems(fp);
	}

	DP(NETIF_MSG_RX_STATUS,
	   "fp->last_max_sge = %d  fp->rx_sge_prod = %d\n",
	   fp->last_max_sge, fp->rx_sge_prod);
}

static void bnx2x_tpa_start(struct bnx2x_fastpath *fp, u16 queue,
			    struct sk_buff *skb, u16 cons, u16 prod)
{
	struct bnx2x *bp = fp->bp;
	struct sw_rx_bd *cons_rx_buf = &fp->rx_buf_ring[cons];
	struct sw_rx_bd *prod_rx_buf = &fp->rx_buf_ring[prod];
	struct eth_rx_bd *prod_bd = &fp->rx_desc_ring[prod];
	dma_addr_t mapping;

	/* move empty skb from pool to prod and map it */
	prod_rx_buf->skb = fp->tpa_pool[queue].skb;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	mapping = dma_map_single(&bp->pdev->dev, fp->tpa_pool[queue].skb->data,
				 bp->rx_buf_size, DMA_FROM_DEVICE);
	dma_unmap_addr_set(prod_rx_buf, mapping, mapping);
#else
	mapping = pci_map_single(bp->pdev, fp->tpa_pool[queue].skb->data,
				 bp->rx_buf_size, PCI_DMA_FROMDEVICE);
	pci_unmap_addr_set(prod_rx_buf, mapping, mapping);
#endif

	/* move partial skb from cons to pool (don't unmap yet) */
	fp->tpa_pool[queue] = *cons_rx_buf;

	/* mark bin state as start - print error if current state != stop */
	if (fp->tpa_state[queue] != BNX2X_TPA_STOP)
		BNX2X_ERR("start of bin not in stop [%d]\n", queue);

	fp->tpa_state[queue] = BNX2X_TPA_START;

	/* point prod_bd to new skb */
	prod_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	prod_bd->addr_lo = cpu_to_le32(U64_LO(mapping));

#ifdef BNX2X_STOP_ON_ERROR
	fp->tpa_queue_used |= (1 << queue);
#if (LINUX_VERSION_CODE >= 0x02061a) /* BNX2X_UPSTREAM */
#ifdef _ASM_GENERIC_INT_L64_H
	DP(NETIF_MSG_RX_STATUS, "fp->tpa_queue_used = 0x%lx\n",
#else
	DP(NETIF_MSG_RX_STATUS, "fp->tpa_queue_used = 0x%llx\n",
#endif
#else
#if defined(__powerpc64__) || defined(_ASM_IA64_TYPES_H)
	DP(NETIF_MSG_RX_STATUS, "fp->tpa_queue_used = 0x%lx\n",
#else
	DP(NETIF_MSG_RX_STATUS, "fp->tpa_queue_used = 0x%llx\n",
#endif
#endif
	   fp->tpa_queue_used);
#endif
}

static int bnx2x_fill_frag_skb(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			       struct sk_buff *skb,
			       struct eth_fast_path_rx_cqe *fp_cqe,
			       u16 cqe_idx)
{
	struct sw_rx_page *rx_pg, old_rx_pg;
	u16 len_on_bd = le16_to_cpu(fp_cqe->len_on_bd);
	u32 i, frag_len, frag_size, pages;
	int err;
	int j;

	frag_size = le16_to_cpu(fp_cqe->pkt_len) - len_on_bd;
	pages = SGE_PAGE_ALIGN(frag_size) >> SGE_PAGE_SHIFT;

#ifndef __VMKLNX__ /* BNX2X_UPSTREAM */
	/* This is needed in order to enable forwarding support */
	if (frag_size)
		skb_shinfo(skb)->gso_size = 1;
#else  /* __VMKLNX__ */
	if (frag_size) {
		/* Setting gso_size to the
		 * [pNic MTU - (TCP_HDR+IP_HDR+ETH_HDR)]
		 */
		u16 eth_hlen = eth_header_len((struct ethhdr *)skb->data);
		struct iphdr *iph = (struct iphdr *)(skb->data + eth_hlen);
		u16 ip_hlen = iph->ihl << 2;
		struct tcphdr *tcph = (struct tcphdr *)((char *)iph + ip_hlen);
		unsigned short lso_mss = bp->dev->mtu -
				(eth_hlen + ip_hlen + (tcph->doff << 2));

		skb_shinfo(skb)->gso_size = lso_mss;
		skb_shinfo(skb)->gso_type = SKB_GSO_TCPV4;
	}
#endif

#ifdef BNX2X_STOP_ON_ERROR
	if (pages > min_t(u32, 8, MAX_SKB_FRAGS)*SGE_PAGE_SIZE*PAGES_PER_SGE) {
		BNX2X_ERR("SGL length is too long: %d. CQE index is %d\n",
			  pages, cqe_idx);
		BNX2X_ERR("fp_cqe->pkt_len = %d  fp_cqe->len_on_bd = %d\n",
			  fp_cqe->pkt_len, len_on_bd);
		bnx2x_panic();
		return -EINVAL;
	}
#endif

	/* Run through the SGL and compose the fragmented skb */
	for (i = 0, j = 0; i < pages; i += PAGES_PER_SGE, j++) {
		u16 sge_idx = RX_SGE(le16_to_cpu(fp_cqe->sgl_or_raw_data.sgl[j]));

		/* FW gives the indices of the SGE as if the ring is an array
		   (meaning that "next" element will consume 2 indices) */
		frag_len = min(frag_size, (u32)(SGE_PAGE_SIZE*PAGES_PER_SGE));
		rx_pg = &fp->rx_page_ring[sge_idx];
		old_rx_pg = *rx_pg;

		/* If we fail to allocate a substitute page, we simply stop
		   where we are and drop the whole packet */
		err = bnx2x_alloc_rx_sge(bp, fp, sge_idx);
		if (unlikely(err)) {
			fp->eth_q_stats.rx_skb_alloc_failed++;
			return err;
		}

		/* Unmap the page as we r going to pass it to the stack */
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
		dma_unmap_page(&bp->pdev->dev, dma_unmap_addr(&old_rx_pg, mapping),
			      SGE_PAGE_SIZE*PAGES_PER_SGE, DMA_FROM_DEVICE);
#else
		pci_unmap_page(bp->pdev, pci_unmap_addr(&old_rx_pg, mapping),
			      SGE_PAGE_SIZE*PAGES_PER_SGE, PCI_DMA_FROMDEVICE);
#endif

		/* Add one frag and update the appropriate fields in the skb */
		skb_fill_page_desc(skb, j, old_rx_pg.page, 0, frag_len);

		skb->data_len += frag_len;
		skb->truesize += frag_len;
		skb->len += frag_len;

		frag_size -= frag_len;
	}

	return 0;
}

static void bnx2x_tpa_stop(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			   u16 queue, int pad, int len, union eth_rx_cqe *cqe,
			   u16 cqe_idx)
{
	struct sw_rx_bd *rx_buf = &fp->tpa_pool[queue];
	struct sk_buff *skb = rx_buf->skb;
	/* alloc new skb */
	struct sk_buff *new_skb = netdev_alloc_skb(bp->dev, bp->rx_buf_size);

	/* Unmap skb in the pool anyway, as we are going to change
	   pool entry status to BNX2X_TPA_STOP even if new skb allocation
	   fails. */
#if (LINUX_VERSION_CODE >= 0x020622)  && !defined(BNX2X_OLD_FCOE)  /* BNX2X_UPSTREAM */
	dma_unmap_single(&bp->pdev->dev, dma_unmap_addr(rx_buf, mapping),
			 bp->rx_buf_size, DMA_FROM_DEVICE);
#else
	pci_unmap_single(bp->pdev, pci_unmap_addr(rx_buf, mapping),
			 bp->rx_buf_size, PCI_DMA_FROMDEVICE);
#endif

	if (likely(new_skb)) {
		/* fix ip xsum and give it to the stack */
		/* (no need to map the new skb) */

		prefetch(skb);
		prefetch(((char *)(skb)) + L1_CACHE_BYTES);

#ifdef BNX2X_STOP_ON_ERROR
		if (pad + len > bp->rx_buf_size) {
			BNX2X_ERR("skb_put is about to fail...  "
				  "pad %d  len %d  rx_buf_size %d\n",
				  pad, len, bp->rx_buf_size);
			bnx2x_panic();
			return;
		}
#endif

		skb_reserve(skb, pad);
		skb_put(skb, len);

		skb->protocol = eth_type_trans(skb, bp->dev);
		skb->ip_summed = CHECKSUM_UNNECESSARY;

		if (!bnx2x_fill_frag_skb(bp, fp, skb,
					 &cqe->fast_path_cqe, cqe_idx)) {
#ifdef BCM_VLAN
			if ((bp->vlgrp != NULL) &&
				(le16_to_cpu(cqe->fast_path_cqe.
				pars_flags.flags) & PARSING_FLAGS_VLAN))
				vlan_gro_receive(&fp->napi, bp->vlgrp,
						le16_to_cpu(cqe->fast_path_cqe.
							    vlan_tag), skb);
			else
#endif
				napi_gro_receive(&fp->napi, skb);
		} else {
			DP(NETIF_MSG_RX_STATUS, "Failed to allocate new pages"
			   " - dropping packet!\n");
			dev_kfree_skb(skb);
		}

#if (LINUX_VERSION_CODE < 0x02061b) /* ! BNX2X_UPSTREAM */
		bp->dev->last_rx = jiffies;
#endif

		/* put new skb in bin */
		fp->tpa_pool[queue].skb = new_skb;

	} else {
		/* else drop the packet and keep the buffer in the bin */
		DP(NETIF_MSG_RX_STATUS,
		   "Failed to allocate new skb - dropping packet!\n");
		fp->eth_q_stats.rx_skb_alloc_failed++;
	}

	fp->tpa_state[queue] = BNX2X_TPA_STOP;
}
#endif

static inline void bnx2x_set_skb_rxhash(struct bnx2x *bp, union eth_rx_cqe *cqe,
					struct sk_buff *skb)
{
#if (LINUX_VERSION_CODE > 0x020622) /* BNX2X_UPSTREAM */
	/* Set Toeplitz hash from CQE */
	if ((bp->dev->features & NETIF_F_RXHASH) &&
	    (cqe->fast_path_cqe.status_flags &
	     ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG))
		skb->rxhash =
		le32_to_cpu(cqe->fast_path_cqe.rss_hash_result);
#endif
}


int bnx2x_rx_int(struct bnx2x_fastpath *fp, int budget)
{
	struct bnx2x *bp = fp->bp;
	u16 bd_cons, bd_prod, bd_prod_fw, comp_ring_cons;
	u16 hw_comp_cons, sw_comp_cons, sw_comp_prod;
	int rx_pkt = 0;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return 0;
#endif

	/* CQ "next element" is of the size of the regular element,
	   that's why it's ok here */
	hw_comp_cons = le16_to_cpu(*fp->rx_cons_sb);
	if ((hw_comp_cons & MAX_RCQ_DESC_CNT) == MAX_RCQ_DESC_CNT)
		hw_comp_cons++;

	bd_cons = fp->rx_bd_cons;
	bd_prod = fp->rx_bd_prod;
	bd_prod_fw = bd_prod;
	sw_comp_cons = fp->rx_comp_cons;
	sw_comp_prod = fp->rx_comp_prod;

	/* Memory barrier necessary as speculative reads of the rx
	 * buffer can be ahead of the index in the status block
	 */
	rmb();

	DP(NETIF_MSG_RX_STATUS,
	   "queue[%d]:  hw_comp_cons %u  sw_comp_cons %u\n",
	   fp->index, hw_comp_cons, sw_comp_cons);

	while (sw_comp_cons != hw_comp_cons) {
		struct sw_rx_bd *rx_buf = NULL;
		struct sk_buff *skb;
		union eth_rx_cqe *cqe;
		u8 cqe_fp_flags;
		u16 len, pad;

		comp_ring_cons = RCQ_BD(sw_comp_cons);
		bd_prod = RX_BD(bd_prod);
		bd_cons = RX_BD(bd_cons);

		/* Prefetch the page containing the BD descriptor
		   at producer's index. It will be needed when new skb is
		   allocated */
		prefetch((void *)(PAGE_ALIGN((unsigned long)
					     (&fp->rx_desc_ring[bd_prod])) -
				  PAGE_SIZE + 1));

		cqe = &fp->rx_comp_ring[comp_ring_cons];
		cqe_fp_flags = cqe->fast_path_cqe.type_error_flags;

		DP(NETIF_MSG_RX_STATUS, "CQE type %x  err %x  status %x"
		   "  queue %x  vlan %x  len %u\n", CQE_TYPE(cqe_fp_flags),
		   cqe_fp_flags, cqe->fast_path_cqe.status_flags,
		   le32_to_cpu(cqe->fast_path_cqe.rss_hash_result),
		   le16_to_cpu(cqe->fast_path_cqe.vlan_tag),
		   le16_to_cpu(cqe->fast_path_cqe.pkt_len));

		/* is this a slowpath msg? */
		if (unlikely(CQE_TYPE(cqe_fp_flags))) {
			bnx2x_sp_event(fp, cqe);
			goto next_cqe;

		/* this is an rx packet */
		} else {
			rx_buf = &fp->rx_buf_ring[bd_cons];
			skb = rx_buf->skb;
			prefetch(skb);
			len = le16_to_cpu(cqe->fast_path_cqe.pkt_len);
			pad = cqe->fast_path_cqe.placement_offset;

#if !defined(__NO_TPA__) /* BNX2X_UPSTREAM */
			/* If CQE is marked both TPA_START and TPA_END
			   it is a non-TPA CQE */
			if ((!fp->disable_tpa) &&
			    (TPA_TYPE(cqe_fp_flags) !=
					(TPA_TYPE_START | TPA_TYPE_END))) {
				u16 queue = cqe->fast_path_cqe.queue_index;

				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_START) {
					DP(NETIF_MSG_RX_STATUS,
					   "calling tpa_start on queue %d\n",
					   queue);

					bnx2x_tpa_start(fp, queue, skb,
							bd_cons, bd_prod);

					/* Set Toeplitz hash for LRO skb */
					bnx2x_set_skb_rxhash(bp, cqe, skb);

					goto next_rx;
				}

				if (TPA_TYPE(cqe_fp_flags) == TPA_TYPE_END) {
					DP(NETIF_MSG_RX_STATUS,
					   "calling tpa_stop on queue %d\n",
					   queue);

					if (!BNX2X_RX_SUM_FIX(cqe))
						BNX2X_ERR("STOP on none TCP "
							  "data\n");

					/* This is a size of the linear data
					   on this skb */
					len = le16_to_cpu(cqe->fast_path_cqe.
								len_on_bd);
					bnx2x_tpa_stop(bp, fp, queue, pad,
						    len, cqe, comp_ring_cons);
#ifdef BNX2X_STOP_ON_ERROR
					if (bp->panic)
						return 0;
#endif

					bnx2x_update_sge_prod(fp,
							&cqe->fast_path_cqe);
					goto next_cqe;
				}
			}
#endif

#if (LINUX_VERSION_CODE >= 0x020622)  && !defined(BNX2X_OLD_FCOE)  /* BNX2X_UPSTREAM */
			dma_sync_single_for_device(&bp->pdev->dev,
					dma_unmap_addr(rx_buf, mapping),
						       pad + RX_COPY_THRESH,
						       DMA_FROM_DEVICE);
#else
			pci_dma_sync_single_for_device(bp->pdev,
					pci_unmap_addr(rx_buf, mapping),
						       pad + RX_COPY_THRESH,
						       PCI_DMA_FROMDEVICE);
#endif
			prefetch(((char *)(skb)) + L1_CACHE_BYTES);

			/* is this an error packet? */
			if (unlikely(cqe_fp_flags & ETH_RX_ERROR_FALGS)) {
				DP(NETIF_MSG_RX_ERR,
				   "ERROR  flags %x  rx packet %u\n",
				   cqe_fp_flags, sw_comp_cons);
				fp->eth_q_stats.rx_err_discard_pkt++;
				goto reuse_rx;
			}

			/* Since we don't have a jumbo ring
			 * copy small packets if mtu > 1500
			 */
			if ((bp->dev->mtu > ETH_MAX_PACKET_SIZE) &&
			    (len <= RX_COPY_THRESH)) {
				struct sk_buff *new_skb;

				new_skb = netdev_alloc_skb(bp->dev,
							   len + pad);
				if (new_skb == NULL) {
					DP(NETIF_MSG_RX_ERR,
					   "ERROR  packet dropped "
					   "because of alloc failure\n");
					fp->eth_q_stats.rx_skb_alloc_failed++;
					goto reuse_rx;
				}

				/* aligned copy */
				skb_copy_from_linear_data_offset(skb, pad,
						    new_skb->data + pad, len);
				skb_reserve(new_skb, pad);
				skb_put(new_skb, len);

				bnx2x_reuse_rx_skb(fp, bd_cons, bd_prod);

				skb = new_skb;

			} else
			if (likely(bnx2x_alloc_rx_skb(bp, fp, bd_prod) == 0)) {
#if (LINUX_VERSION_CODE >= 0x020622)  && !defined(BNX2X_OLD_FCOE)  /* BNX2X_UPSTREAM */
				dma_unmap_single(&bp->pdev->dev,
					dma_unmap_addr(rx_buf, mapping),
						 bp->rx_buf_size,
						 DMA_FROM_DEVICE);
#else
				pci_unmap_single(bp->pdev,
					pci_unmap_addr(rx_buf, mapping),
						 bp->rx_buf_size,
						 PCI_DMA_FROMDEVICE);
#endif
				skb_reserve(skb, pad);
				skb_put(skb, len);

			} else {
				DP(NETIF_MSG_RX_ERR,
				   "ERROR  packet dropped because "
				   "of alloc failure\n");
				fp->eth_q_stats.rx_skb_alloc_failed++;
reuse_rx:
				bnx2x_reuse_rx_skb(fp, bd_cons, bd_prod);
				goto next_rx;
			}

			skb->protocol = eth_type_trans(skb, bp->dev);

			/* Set Toeplitz hash for a none-LRO skb */
			bnx2x_set_skb_rxhash(bp, cqe, skb);

			skb->ip_summed = CHECKSUM_NONE;
			if (bp->rx_csum) {
				if (likely(BNX2X_RX_CSUM_OK(cqe)))
					skb->ip_summed = CHECKSUM_UNNECESSARY;
				else
					fp->eth_q_stats.hw_csum_err++;
			}
		}

		skb_record_rx_queue(skb, fp->index);

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
		vmknetddi_queueops_set_skb_queueid(skb,
				VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(fp->index));
#endif
#ifdef BCM_VLAN
		if ((bp->vlgrp != NULL) &&
		    (le16_to_cpu(cqe->fast_path_cqe.pars_flags.flags) &
		     PARSING_FLAGS_VLAN))
			vlan_gro_receive(&fp->napi, bp->vlgrp,
				le16_to_cpu(cqe->fast_path_cqe.vlan_tag), skb);
		else
#endif
			napi_gro_receive(&fp->napi, skb);

#if (LINUX_VERSION_CODE < 0x02061b) /* ! BNX2X_UPSTREAM */
		bp->dev->last_rx = jiffies;
#endif

next_rx:
		rx_buf->skb = NULL;

		bd_cons = NEXT_RX_IDX(bd_cons);
		bd_prod = NEXT_RX_IDX(bd_prod);
		bd_prod_fw = NEXT_RX_IDX(bd_prod_fw);
		rx_pkt++;
next_cqe:
		sw_comp_prod = NEXT_RCQ_IDX(sw_comp_prod);
		sw_comp_cons = NEXT_RCQ_IDX(sw_comp_cons);

		if (rx_pkt == budget)
			break;
	} /* while */

	fp->rx_bd_cons = bd_cons;
	fp->rx_bd_prod = bd_prod_fw;
	fp->rx_comp_cons = sw_comp_cons;
	fp->rx_comp_prod = sw_comp_prod;

	/* Update producers */
	bnx2x_update_rx_prod(bp, fp, bd_prod_fw, sw_comp_prod,
			     fp->rx_sge_prod);

	fp->rx_pkt += rx_pkt;
	fp->rx_calls++;

	return rx_pkt;
}

#ifdef BCM_CNIC
#if (LINUX_VERSION_CODE < 0x020613) && (VMWARE_ESX_DDK_VERSION < 40000)
irqreturn_t bnx2x_msix_cnic_int(int irq, void *fp_cookie,
				     struct pt_regs *regs)
#else /* BNX2X_UPSTREAM */
irqreturn_t bnx2x_msix_cnic_int(int irq, void *fp_cookie)
#endif
{
	struct bnx2x_fastpath *fp = fp_cookie;
	struct bnx2x *bp = fp->bp;

	bnx2x_ack_sb(bp, fp->igu_sb_id, USTORM_ID, 0, IGU_INT_DISABLE, 0);

	bnx2x_update_fpsb_idx(fp);

	DP(BNX2X_MSG_FP,"Update index to %d\n",fp->fp_hc_idx);
	bnx2x_ack_sb(bp, fp->igu_sb_id, USTORM_ID,
		     le16_to_cpu(fp->fp_hc_idx),
		     IGU_INT_ENABLE, 1);

	return IRQ_HANDLED;
}
#endif

#if (LINUX_VERSION_CODE < 0x020613) && (VMWARE_ESX_DDK_VERSION < 40000)
irqreturn_t bnx2x_msix_fp_int(int irq, void *fp_cookie,
				     struct pt_regs *regs)
#else /* BNX2X_UPSTREAM */
irqreturn_t bnx2x_msix_fp_int(int irq, void *fp_cookie)
#endif
{
	struct bnx2x_fastpath *fp = fp_cookie;
	struct bnx2x *bp = fp->bp;

	/* Return here if interrupt is disabled */
	if (unlikely(atomic_read(&bp->intr_sem) != 0)) {
		DP(NETIF_MSG_INTR, "called but intr_sem not 0, returning\n");
		return IRQ_HANDLED;
	}

	DP(BNX2X_MSG_FP, "got an MSI-X interrupt on IDX:SB "
			 "[fp %d fw_sd %d igusb %d]\n",
	   fp->index, fp->fw_sb_id, fp->igu_sb_id);
	bnx2x_ack_sb(bp, fp->igu_sb_id, USTORM_ID, 0, IGU_INT_DISABLE, 0);
#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return IRQ_HANDLED;
#endif

	prefetch(fp->rx_cons_sb);
	prefetch(fp->tx_cons_sb);
	prefetch(&fp->sb_running_index[SM_RX_ID]);
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
	napi_schedule(&bnx2x_fp(bp, fp->index, napi));
#else
	napi_schedule(&bnx2x_fp(bp, fp->index, dummy_netdev));
#endif


	return IRQ_HANDLED;
}


/**
 * Fill in a MAC address the way the FW likes it
 *
 * @param fw_hi
 * @param fw_mid
 * @param fw_lo
 * @param mac
 */
static inline void
bnx2x_set_fw_mac_addr(u16 *fw_hi, u16 *fw_mid, u16 *fw_lo, u8 *mac)
{
	((u8 *)fw_hi)[0]  = mac[1];
	((u8 *)fw_hi)[1]  = mac[0];
	((u8 *)fw_mid)[0] = mac[3];
	((u8 *)fw_mid)[1] = mac[2];
	((u8 *)fw_lo)[0]  = mac[5];
	((u8 *)fw_lo)[1]  = mac[4];
}


#ifdef NETIF_F_TSO /* BNX2X_UPSTREAM */
/* we split the first BD into headers and data BDs
 * to ease the pain of our fellow microcode engineers
 * we use one mapping for both BDs
 * So far this has only been observed to happen
 * in Other Operating Systems(TM)
 */
static noinline u16 bnx2x_tx_split(struct bnx2x *bp,
				   struct bnx2x_fastpath *fp,
				   struct sw_tx_bd *tx_buf,
				   struct eth_tx_start_bd **tx_bd, u16 hlen,
				   u16 bd_prod, int nbd)
{
	struct eth_tx_start_bd *h_tx_bd = *tx_bd;
	struct eth_tx_bd *d_tx_bd;
	dma_addr_t mapping;
	int old_len = le16_to_cpu(h_tx_bd->nbytes);

	/* first fix first BD */
	h_tx_bd->nbd = cpu_to_le16(nbd);
	h_tx_bd->nbytes = cpu_to_le16(hlen);

	DP(NETIF_MSG_TX_QUEUED,	"TSO split header size is %d "
	   "(%x:%x) nbd %d\n", h_tx_bd->nbytes, h_tx_bd->addr_hi,
	   h_tx_bd->addr_lo, h_tx_bd->nbd);

	/* now get a new data BD
	 * (after the pbd) and fill it */
	bd_prod = TX_BD(NEXT_TX_IDX(bd_prod));
	d_tx_bd = &fp->tx_desc_ring[bd_prod].reg_bd;

	mapping = HILO_U64(le32_to_cpu(h_tx_bd->addr_hi),
			   le32_to_cpu(h_tx_bd->addr_lo)) + hlen;

	d_tx_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	d_tx_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
	d_tx_bd->nbytes = cpu_to_le16(old_len - hlen);

	/* this marks the BD as one that has no individual mapping */
	tx_buf->flags |= BNX2X_TSO_SPLIT_BD;

	DP(NETIF_MSG_TX_QUEUED,
	   "TSO split data size is %d (%x:%x)\n",
	   d_tx_bd->nbytes, d_tx_bd->addr_hi, d_tx_bd->addr_lo);

	/* update tx_bd */
	*tx_bd = (struct eth_tx_start_bd *)d_tx_bd;

	return bd_prod;
}
#endif

static inline u16 bnx2x_csum_fix(unsigned char *t_header, u16 csum, s8 fix)
{
	if (fix > 0)
		csum = (u16) ~csum_fold(csum_sub(csum,
				csum_partial(t_header - fix, fix, 0)));

	else if (fix < 0)
		csum = (u16) ~csum_fold(csum_add(csum,
				csum_partial(t_header, -fix, 0)));

	return swab16(csum);
}

#if (MAX_SKB_FRAGS >= MAX_FETCH_BD - 3)
/* check if packet requires linearization (packet is too fragmented)
   no need to check fragmentation if page size > 8K (there will be no
   violation to FW restrictions) */
static int bnx2x_pkt_req_lin(struct bnx2x *bp, struct sk_buff *skb,
			     u32 xmit_type)
{
	int to_copy = 0;
	int hlen = 0;
	int first_bd_sz = 0;

	/* 3 = 1 (for linear data BD) + 2 (for PBD and last BD) */
	if (skb_shinfo(skb)->nr_frags >= (MAX_FETCH_BD - 3)) {

		if (xmit_type & XMIT_GSO) {
#ifdef NETIF_F_TSO /* BNX2X_UPSTREAM */
			unsigned short lso_mss = skb_shinfo(skb)->gso_size;
			/* Check if LSO packet needs to be copied:
			   3 = 1 (for headers BD) + 2 (for PBD and last BD) */
			int wnd_size = MAX_FETCH_BD - 3;
			/* Number of windows to check */
			int num_wnds = skb_shinfo(skb)->nr_frags - wnd_size;
			int wnd_idx = 0;
			int frag_idx = 0;
			u32 wnd_sum = 0;

			/* Headers length */
			hlen = (int)(skb_transport_header(skb) - skb->data) +
				tcp_hdrlen(skb);

			/* Amount of data (w/o headers) on linear part of SKB*/
			first_bd_sz = skb_headlen(skb) - hlen;

			wnd_sum  = first_bd_sz;

			/* Calculate the first sum - it's special */
			for (frag_idx = 0; frag_idx < wnd_size - 1; frag_idx++)
				wnd_sum +=
					skb_shinfo(skb)->frags[frag_idx].size;

			/* If there was data on linear skb data - check it */
			if (first_bd_sz > 0) {
				if (unlikely(wnd_sum < lso_mss)) {
					to_copy = 1;
					goto exit_lbl;
				}

				wnd_sum -= first_bd_sz;
			}

			/* Others are easier: run through the frag list and
			   check all windows */
			for (wnd_idx = 0; wnd_idx <= num_wnds; wnd_idx++) {
				wnd_sum +=
			  skb_shinfo(skb)->frags[wnd_idx + wnd_size - 1].size;

				if (unlikely(wnd_sum < lso_mss)) {
					to_copy = 1;
					break;
				}
				wnd_sum -=
					skb_shinfo(skb)->frags[wnd_idx].size;
			}
#endif
		} else {
			/* in non-LSO too fragmented packet should always
			   be linearized */
			to_copy = 1;
		}
	}

#ifdef NETIF_F_TSO /* BNX2X_UPSTREAM */
exit_lbl:
#endif
	if (unlikely(to_copy))
		DP(NETIF_MSG_TX_QUEUED,
		   "Linearization IS REQUIRED for %s packet. "
		   "num_frags %d  hlen %d  first_bd_sz %d\n",
		   (xmit_type & XMIT_GSO) ? "LSO" : "non-LSO",
		   skb_shinfo(skb)->nr_frags, hlen, first_bd_sz);

	return to_copy;
}
#endif

#ifdef NETIF_F_TSO /* BNX2X_UPSTREAM */
static inline void bnx2x_set_pbd_gso_e2(struct sk_buff *skb,
				     struct eth_tx_parse_bd_e2 *pbd,
				     u32 xmit_type)
{
	pbd->parsing_data |= cpu_to_le16(skb_shinfo(skb)->gso_size) <<
		ETH_TX_PARSE_BD_E2_LSO_MSS_SHIFT;
	if ((xmit_type & XMIT_GSO_V6) &&
	    (ipv6_hdr(skb)->nexthdr == NEXTHDR_IPV6))
		pbd->parsing_data |= ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR;
}

/**
 * Update PBD in GSO case.
 *
 * @param skb
 * @param tx_start_bd
 * @param pbd
 * @param xmit_type
 */
static inline void bnx2x_set_pbd_gso(struct sk_buff *skb,
				     struct eth_tx_parse_bd_e1x *pbd,
				     u32 xmit_type)
{
	pbd->lso_mss = cpu_to_le16(skb_shinfo(skb)->gso_size);
	pbd->tcp_send_seq = swab32(tcp_hdr(skb)->seq);
	pbd->tcp_flags = pbd_tcp_flags(skb);

	if (xmit_type & XMIT_GSO_V4) {
		pbd->ip_id = swab16(ip_hdr(skb)->id);
		pbd->tcp_pseudo_csum =
			swab16(~csum_tcpudp_magic(ip_hdr(skb)->saddr,
						  ip_hdr(skb)->daddr,
						  0, IPPROTO_TCP, 0));

	} else
		pbd->tcp_pseudo_csum =
			swab16(~csum_ipv6_magic(&ipv6_hdr(skb)->saddr,
						&ipv6_hdr(skb)->daddr,
						0, IPPROTO_TCP, 0));

	pbd->global_data |= ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN;
}
#endif /* NETIF_F_TSO */

static inline u8 bnx2x_get_hlen_csum_tcp(struct sk_buff *skb)
{
	return skb_transport_header(skb) + tcp_hdrlen(skb) - skb->data;
}

static inline u8 bnx2x_get_hlen_csum_udp(struct sk_buff *skb)
{
	return skb_transport_header(skb) + sizeof(struct udphdr) - skb->data;
}

/**
 *
 * @param skb
 * @param tx_start_bd
 * @param pbd_e2
 * @param xmit_type
 *
 * @return header len
 */
static inline  u8 bnx2x_set_pbd_csum_e2(struct bnx2x *bp, struct sk_buff *skb,
	struct eth_tx_parse_bd_e2 *pbd,
	u32 xmit_type)
{
	pbd->parsing_data |= cpu_to_le16(((u8 *)skb_transport_header(skb) -
					 skb->data) / 2) <<
		ETH_TX_PARSE_BD_E2_TCP_HDR_START_OFFSET_W_SHIFT;

	if (xmit_type & XMIT_CSUM_TCP) {
		pbd->parsing_data |= cpu_to_le16(tcp_hdrlen(skb)/4) <<
			ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW_SHIFT;

		return bnx2x_get_hlen_csum_tcp(skb);
	} else
		/* no need to pass the UDP header length - it's a constant */
		return bnx2x_get_hlen_csum_udp(skb);
}

static inline void bnx2x_set_sbd_csum(struct bnx2x *bp, struct sk_buff *skb,
	struct eth_tx_start_bd *tx_start_bd, u32 xmit_type)
{

	tx_start_bd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_L4_CSUM;

	if (xmit_type & XMIT_CSUM_V4)
		tx_start_bd->bd_flags.as_bitfield |=
					ETH_TX_BD_FLAGS_IP_CSUM;
	else
		tx_start_bd->bd_flags.as_bitfield |=
					ETH_TX_BD_FLAGS_IPV6;

	if (!(xmit_type & XMIT_CSUM_TCP))
		tx_start_bd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_IS_UDP;

}


/**
 *
 * @param skb
 * @param tx_start_bd
 * @param pbd
 * @param xmit_type
 *
 * @return Header length
 */
static inline u8 bnx2x_set_pbd_csum(struct bnx2x *bp, struct sk_buff *skb,
	struct eth_tx_parse_bd_e1x *pbd,
	u32 xmit_type)
{
	u8 hlen = (skb_network_header(skb) - skb->data) / 2;

	/* for now NS flag is not used in Linux */
	pbd->global_data =
		(hlen | ((skb->protocol == cpu_to_be16(ETH_P_8021Q)) <<
			 ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN_SHIFT));

	pbd->ip_hlen_w = (skb_transport_header(skb) -
			skb_network_header(skb)) / 2;

	hlen += pbd->ip_hlen_w;

	/* We support checksum offload for TCP and UDP only */
	if (xmit_type & XMIT_CSUM_TCP)
		hlen += tcp_hdrlen(skb) / 2;
	else
		hlen += sizeof(struct udphdr) / 2;

	pbd->total_hlen_w = cpu_to_le16(hlen);
	hlen = hlen*2;

	if (xmit_type & XMIT_CSUM_TCP) {
		pbd->tcp_pseudo_csum = swab16(tcp_hdr(skb)->check);

	} else {
		s8 fix = SKB_CS_OFF(skb); /* signed! */

		DP(NETIF_MSG_TX_QUEUED,
		   "hlen %d  fix %d  csum before fix %x\n",
		   le16_to_cpu(pbd->total_hlen_w), fix, SKB_CS(skb));

		/* HW bug: fixup the CSUM */
		pbd->tcp_pseudo_csum =
			bnx2x_csum_fix(skb_transport_header(skb),
				       SKB_CS(skb), fix);

		DP(NETIF_MSG_TX_QUEUED, "csum after fix %x\n",
		   pbd->tcp_pseudo_csum);
	}

	return hlen;
}

static inline u32 bnx2x_xmit_type(struct bnx2x *bp, struct sk_buff *skb)
{
	u32 rc;

	if (skb->ip_summed != CHECKSUM_PARTIAL)
		rc = XMIT_PLAIN;

	else {
		if (skb->protocol == htons(ETH_P_IPV6)) {
			rc = XMIT_CSUM_V6;
			if (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP)
				rc |= XMIT_CSUM_TCP;

		} else {
			rc = XMIT_CSUM_V4;
			if (ip_hdr(skb)->protocol == IPPROTO_TCP)
				rc |= XMIT_CSUM_TCP;
		}
	}

#ifdef NETIF_F_GSO /* BNX2X_UPSTREAM */
	if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV4)
		rc |= (XMIT_GSO_V4 | XMIT_CSUM_V4 | XMIT_CSUM_TCP);

#ifdef NETIF_F_TSO6 /* BNX2X_UPSTREAM */
	else if (skb_shinfo(skb)->gso_type & SKB_GSO_TCPV6)
		rc |= (XMIT_GSO_V6 | XMIT_CSUM_TCP | XMIT_CSUM_V6);
#endif
#elif defined(NETIF_F_TSO) /* ! BNX2X_UPSTREAM */
	if (skb_shinfo(skb)->gso_size)
		rc |= (XMIT_GSO_V4 | XMIT_CSUM_V4 | XMIT_CSUM_TCP);
#endif

	return rc;
}

/* called with netif_tx_lock
 * bnx2x_tx_int() runs without netif_tx_lock unless it needs to call
 * netif_wake_queue()
 */
int bnx2x_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);
	struct bnx2x_fastpath *fp;
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
	struct netdev_queue *txq;
#endif
	struct sw_tx_bd *tx_buf;
	struct eth_tx_start_bd *tx_start_bd;
	struct eth_tx_bd *tx_data_bd, *total_pkt_bd = NULL;
	struct eth_tx_parse_bd_e1x *pbd_e1x = NULL;
	struct eth_tx_parse_bd_e2 *pbd_e2 = NULL;
	u16 pkt_prod, bd_prod;
	int nbd, fp_index;
	dma_addr_t mapping;
	u32 xmit_type = bnx2x_xmit_type(bp, skb);
	int i;
	u8 hlen = 0;
	__le16 pkt_size = 0;
	struct ethhdr *eth;
	u8 mac_type = UNICAST_ADDRESS;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return NETDEV_TX_BUSY;
#endif

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
	VMK_ASSERT(skb->queue_mapping <= BNX2X_NUM_NETQUEUES(bp));
	fp_index = skb->queue_mapping;
	txq = netdev_get_tx_queue(dev, fp_index);
#else
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
	fp_index = skb_get_queue_mapping(skb);
	txq = netdev_get_tx_queue(dev, fp_index);
#else
	fp_index = 0;
#endif
#endif

	fp = &bp->fp[fp_index];

	if (unlikely(bnx2x_tx_avail(fp) < (skb_shinfo(skb)->nr_frags + 3))) {
		fp->eth_q_stats.driver_xoff++;
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
		netif_tx_stop_queue(txq);
#else
		netif_stop_queue(dev);
#endif
		BNX2X_ERR("BUG! Tx ring full when queue awake!\n");
		return NETDEV_TX_BUSY;
	}

#ifdef NETIF_F_GSO /* BNX2X_UPSTREAM */
	DP(NETIF_MSG_TX_QUEUED, "queue[%d]: SKB: summed %x  protocol %x  "
				"protocol(%x,%x) gso type %x  xmit_type %x\n",
	   fp_index, skb->ip_summed, skb->protocol, ipv6_hdr(skb)->nexthdr,
	   ip_hdr(skb)->protocol, skb_shinfo(skb)->gso_type, xmit_type);
#endif

	eth = (struct ethhdr*)skb->data;

	/* set flag according to packet type (UNICAST_ADDRESS is default)*/
	if (unlikely(is_multicast_ether_addr(eth->h_dest))) {
		if (is_broadcast_ether_addr(eth->h_dest))
			mac_type = BROADCAST_ADDRESS;
		else
			mac_type = MULTICAST_ADDRESS;
	}

#if (MAX_SKB_FRAGS >= MAX_FETCH_BD - 3)
	/* First, check if we need to linearize the skb (due to FW
	   restrictions). No need to check fragmentation if page size > 8K
	   (there will be no violation to FW restrictions) */
	if (bnx2x_pkt_req_lin(bp, skb, xmit_type)) {
		/* Statistics of linearization */
		bp->lin_cnt++;
#if (LINUX_VERSION_CODE > 0x020611) || defined(SLE_VERSION_CODE) /* BNX2X_UPSTREAM */
		if (skb_linearize(skb) != 0) {
#else
		if (skb_linearize(skb, GFP_ATOMIC) != 0) {
#endif
			DP(NETIF_MSG_TX_QUEUED, "SKB linearization failed - "
			   "silently dropping this SKB\n");
			dev_kfree_skb(skb);
			return NETDEV_TX_OK;
		}
	}
#endif

	/*
	Please read carefully. First we use one BD which we mark as start,
	then we have a parsing info BD (used for TSO or xsum),
	and only then we have the rest of the TSO BDs.
	(don't forget to mark the last one as last,
	and to unmap only AFTER you write to the BD ...)
	And above all, all pdb sizes are in words - NOT DWORDS!
	*/

	pkt_prod = fp->tx_pkt_prod++;
	bd_prod = TX_BD(fp->tx_bd_prod);

	/* get a tx_buf and first BD */
	tx_buf = &fp->tx_buf_ring[TX_BD(pkt_prod)];
	tx_start_bd = &fp->tx_desc_ring[bd_prod].start_bd;

	tx_start_bd->bd_flags.as_bitfield = ETH_TX_BD_FLAGS_START_BD;
	SET_FLAG(tx_start_bd->general_data,
		  ETH_TX_START_BD_ETH_ADDR_TYPE,
		  mac_type);
	/* header nbd */
	SET_FLAG(tx_start_bd->general_data,
		  ETH_TX_START_BD_HDR_NBDS,
		  1);

	/* remember the first BD of the packet */
	tx_buf->first_bd = fp->tx_bd_prod;
	tx_buf->skb = skb;
	tx_buf->flags = 0;

	DP(NETIF_MSG_TX_QUEUED,
	   "sending pkt %u @%p  next_idx %u  bd %u @%p\n",
	   pkt_prod, tx_buf, fp->tx_pkt_prod, bd_prod, tx_start_bd);

#ifdef BCM_VLAN
	if ((bp->vlgrp != NULL) && vlan_tx_tag_present(skb)) {
		tx_start_bd->vlan_or_ethertype = cpu_to_le16(vlan_tx_tag_get(skb));
		tx_start_bd->bd_flags.as_bitfield |=
			(X_ETH_OUTBAND_VLAN << ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT);
	} else
#endif
		tx_start_bd->vlan_or_ethertype = cpu_to_le16(pkt_prod);

	/* turn on parsing and get a BD */
	bd_prod = TX_BD(NEXT_TX_IDX(bd_prod));

	if (xmit_type & XMIT_CSUM)
		bnx2x_set_sbd_csum(bp, skb,tx_start_bd, xmit_type);

	if (CHIP_IS_E2(bp)){
		pbd_e2 = &fp->tx_desc_ring[bd_prod].parse_bd_e2;
		memset(pbd_e2, 0, sizeof(struct eth_tx_parse_bd_e2));
		/* Set PBD in checksum offload case */
		if (xmit_type & XMIT_CSUM)
			hlen = bnx2x_set_pbd_csum_e2(bp, skb, pbd_e2, xmit_type);
#ifdef BCM_IOV /* ! BNX2X_UPSTREAM */
		/* fill in the MAC addresses in the PBD - for local switching */
		bnx2x_set_fw_mac_addr(&pbd_e2->src_mac_addr_hi,
				      &pbd_e2->src_mac_addr_mid,
				      &pbd_e2->src_mac_addr_lo,
				      eth->h_source);
		bnx2x_set_fw_mac_addr(&pbd_e2->dst_mac_addr_hi,
				      &pbd_e2->dst_mac_addr_mid,
				      &pbd_e2->dst_mac_addr_lo,
				      eth->h_dest);
#endif
	} else {
		pbd_e1x = &fp->tx_desc_ring[bd_prod].parse_bd_e1x;
		memset(pbd_e1x, 0, sizeof(struct eth_tx_parse_bd_e1x));
		/* Set PBD in checksum offload case */
		if (xmit_type & XMIT_CSUM)
			hlen = bnx2x_set_pbd_csum(bp, skb, pbd_e1x, xmit_type);

	}

	/* Map skb linear data for DMA */
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
	mapping = dma_map_single(&bp->pdev->dev, skb->data,
				 skb_headlen(skb), DMA_TO_DEVICE);
#else
	mapping = pci_map_single(bp->pdev, skb->data,
				 skb_headlen(skb), PCI_DMA_TODEVICE);
#endif

	/* Setup the data pointer of the first BD of the packet */
	tx_start_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
	tx_start_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
	nbd = skb_shinfo(skb)->nr_frags + 2; /* start_bd + pbd + frags */
	tx_start_bd->nbd = cpu_to_le16(nbd);
	tx_start_bd->nbytes = cpu_to_le16(skb_headlen(skb));
	pkt_size = tx_start_bd->nbytes;

	DP(NETIF_MSG_TX_QUEUED, "first bd @%p  addr (%x:%x)  nbd %d"
	   "  nbytes %d  flags %x  vlan %x\n",
	   tx_start_bd, tx_start_bd->addr_hi, tx_start_bd->addr_lo,
	   le16_to_cpu(tx_start_bd->nbd), le16_to_cpu(tx_start_bd->nbytes),
	   tx_start_bd->bd_flags.as_bitfield, le16_to_cpu(tx_start_bd->vlan_or_ethertype));

#ifdef NETIF_F_TSO /* BNX2X_UPSTREAM */
	if (xmit_type & XMIT_GSO) {

		DP(NETIF_MSG_TX_QUEUED,
		   "TSO packet len %d  hlen %d  total len %d  tso size %d\n",
		   skb->len, hlen, skb_headlen(skb),
		   skb_shinfo(skb)->gso_size);

		tx_start_bd->bd_flags.as_bitfield |= ETH_TX_BD_FLAGS_SW_LSO;

		if (unlikely(skb_headlen(skb) > hlen))
			bd_prod = bnx2x_tx_split(bp, fp, tx_buf, &tx_start_bd,
						 hlen, bd_prod, ++nbd);
		if (CHIP_IS_E2(bp))
			bnx2x_set_pbd_gso_e2(skb, pbd_e2, xmit_type);
		else
			bnx2x_set_pbd_gso(skb, pbd_e1x, xmit_type);
	}
#endif
	tx_data_bd = (struct eth_tx_bd *)tx_start_bd;

	/* Handle fragmented skb */
	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		skb_frag_t *frag = &skb_shinfo(skb)->frags[i];

		bd_prod = TX_BD(NEXT_TX_IDX(bd_prod));
		tx_data_bd = &fp->tx_desc_ring[bd_prod].reg_bd;
		if (total_pkt_bd == NULL)
			total_pkt_bd = &fp->tx_desc_ring[bd_prod].reg_bd;

#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
		mapping = dma_map_page(&bp->pdev->dev, frag->page, frag->page_offset,
				       frag->size, DMA_TO_DEVICE);
#else
		mapping = pci_map_page(bp->pdev, frag->page, frag->page_offset,
				       frag->size, PCI_DMA_TODEVICE);
#endif

		tx_data_bd->addr_hi = cpu_to_le32(U64_HI(mapping));
		tx_data_bd->addr_lo = cpu_to_le32(U64_LO(mapping));
		tx_data_bd->nbytes = cpu_to_le16(frag->size);
		le16_add_cpu(&pkt_size, frag->size);

		DP(NETIF_MSG_TX_QUEUED,
		   "frag %d  bd @%p  addr (%x:%x)  nbytes %d\n",
		   i, tx_data_bd, tx_data_bd->addr_hi, tx_data_bd->addr_lo,
		   le16_to_cpu(tx_data_bd->nbytes));
	}

	DP(NETIF_MSG_TX_QUEUED, "last bd @%p\n", tx_data_bd);

	bd_prod = TX_BD(NEXT_TX_IDX(bd_prod));

	/* now send a tx doorbell, counting the next BD
	 * if the packet contains or ends with it
	 */
	if (TX_BD_POFF(bd_prod) < nbd)
		nbd++;

	if (total_pkt_bd != NULL)
		total_pkt_bd->total_pkt_bytes = pkt_size;

	if (pbd_e1x)
		DP(NETIF_MSG_TX_QUEUED,
		   "PBD (E1X) @%p  ip_data %x  ip_hlen %u  ip_id %u  lso_mss %u"
		   "  tcp_flags %x  xsum %x  seq %u  hlen %u\n",
		   pbd_e1x, pbd_e1x->global_data, pbd_e1x->ip_hlen_w,
		   pbd_e1x->ip_id, pbd_e1x->lso_mss, pbd_e1x->tcp_flags,
		   pbd_e1x->tcp_pseudo_csum, pbd_e1x->tcp_send_seq,
		    le16_to_cpu(pbd_e1x->total_hlen_w));
	if (pbd_e2)
		DP(NETIF_MSG_TX_QUEUED,
		   "PBD (E2) @%p  dst %x %x %x src %x %x %x parsing_data %x\n",
		   pbd_e2, pbd_e2->dst_mac_addr_hi, pbd_e2->dst_mac_addr_mid,
		   pbd_e2->dst_mac_addr_lo, pbd_e2->src_mac_addr_hi,
		   pbd_e2->src_mac_addr_mid, pbd_e2->src_mac_addr_lo,
		   pbd_e2->parsing_data);
	DP(NETIF_MSG_TX_QUEUED, "doorbell: nbd %d  bd %u\n", nbd, bd_prod);

	/*
	 * Make sure that the BD data is updated before updating the producer
	 * since FW might read the BD right after the producer is updated.
	 * This is only applicable for weak-ordered memory model archs such
	 * as IA-64. The following barrier is also mandatory since FW will
	 * assumes packets must have BDs.
	 */
	wmb();

	fp->tx_db.data.prod += nbd;
	barrier();

	DOORBELL(bp, fp->cid, fp->tx_db.raw);

	mmiowb();

	fp->tx_bd_prod += nbd;
#if (LINUX_VERSION_CODE < 0x02061f) /* ! BNX2X_UPSTREAM */
	/* In kernels starting from 2.6.31 netdev layer does this */
	dev->trans_start = jiffies;
#endif

	if (unlikely(bnx2x_tx_avail(fp) < MAX_SKB_FRAGS + 3)) {
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
		netif_tx_stop_queue(txq);
#else
		netif_stop_queue(dev);
#endif
		/* paired memory barrier is in bnx2x_tx_int(), we have to keep
		 * ordering of set_bit() in netif_tx_stop_queue() and read of
		 * fp->bd_tx_cons */
		smp_mb();

		fp->eth_q_stats.driver_xoff++;
		if (bnx2x_tx_avail(fp) >= MAX_SKB_FRAGS + 3)
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
			netif_tx_wake_queue(txq);
#else
			netif_wake_queue(dev);
#endif
	}
	fp->tx_pkt++;

	return NETDEV_TX_OK;
}

/* called with rtnl_lock */
int bnx2x_change_mac_addr(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;
	struct bnx2x *bp = netdev_priv(dev);

	if (!is_valid_ether_addr((u8 *)(addr->sa_data)))
		return -EINVAL;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);
	if (netif_running(dev))
		bnx2x_set_eth_mac(bp, 1);

	return 0;
}


#if defined(BNX2X_NEW_NAPI) /* BNX2X_UPSTREAM */
int bnx2x_poll(struct napi_struct *napi, int budget)
#else
int bnx2x_poll(struct net_device *dev, int *budget)
#endif
{
	int work_done = 0;
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
	struct bnx2x_fastpath *fp = container_of(napi, struct bnx2x_fastpath,
						 napi);
	struct bnx2x *bp = fp->bp;
#else /* ! BNX2X_UPSTREAM */
	struct bnx2x_fastpath *fp = dev->priv;
	struct bnx2x *bp = fp->bp;
	int orig_budget = min(*budget, dev->quota);
#endif

	while (1) {
#ifdef BNX2X_STOP_ON_ERROR
		if (unlikely(bp->panic)) {
			napi_complete(napi);
			return 0;
		}
#endif

#ifndef BNX2X_MULTI_QUEUE /* ! BNX2X_UPSTREAM */
		/* There is only one Tx queue on kernels 2.6.26 and below */
		if (fp->index == 0)
#endif
		if (bnx2x_has_tx_work(fp))
			bnx2x_tx_int(fp);

		if (bnx2x_has_rx_work(fp)) {
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
			work_done += bnx2x_rx_int(fp, budget - work_done);

			/* must not complete if we consumed full budget */
			if (work_done >= budget)
				break;
#else
			work_done = bnx2x_rx_int(fp, orig_budget);

			*budget -= work_done;
			dev->quota -= work_done;
			orig_budget = min(*budget, dev->quota);
			if (orig_budget <= 0)
				break;
#endif
		}

		/* Fall out from the NAPI loop if needed */
		if (!(bnx2x_has_rx_work(fp) || bnx2x_has_tx_work(fp))) {
#ifdef BCM_CNIC
			/* No need to update SB for FCoE L2 ring as long as it's
			 * connected to the default SB and the SB has been updated when
			 * NAPI was scheduled.
			 */
			if (IS_FCOE_FP(fp)) {
				napi_complete(napi);
#ifndef BNX2X_NEW_NAPI
				return 0;
#else	/* BNX2X_UPSTREAM */
				break;
#endif
			}
#endif

			bnx2x_update_fpsb_idx(fp);
		/* bnx2x_has_rx_work() reads the status block, thus we need
		 * to ensure that status block indices have been actually read
		 * (bnx2x_update_fpsb_idx) prior to this check
		 * (bnx2x_has_rx_work) so that we won't write the "newer"
		 * value of the status block to IGU (if there was a DMA right
		 * after bnx2x_has_rx_work and if there is no rmb, the memory
		 * reading (bnx2x_update_fpsb_idx) may be postponed to right
		 * before bnx2x_ack_sb). In this case there will never be
		 * another interrupt until there is another update of the
		 * status block, while there is still unhandled work.
		 */
			rmb();

			if (!(bnx2x_has_rx_work(fp) || bnx2x_has_tx_work(fp))) {
				napi_complete(napi);
			/* Re-enable interrupts */
				DP(NETIF_MSG_HW, "Update index to %d\n",fp->fp_hc_idx);
				bnx2x_ack_sb(bp, fp->igu_sb_id, USTORM_ID,
					     le16_to_cpu(fp->fp_hc_idx),
					     IGU_INT_ENABLE, 1);


#ifndef BNX2X_NEW_NAPI
				return 0;
#else	/* BNX2X_UPSTREAM */
				break;
#endif
			}
		}
	}

#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
	return work_done;
#else
	return 1;
#endif
}


/* end of fast path */


static void bnx2x_free_rx_bds(struct bnx2x_fastpath *fp)
{
	struct bnx2x *bp = fp->bp;
	int i;

	/* ring wasn't allocated */
	if (fp->rx_buf_ring == NULL)
		return;

	for (i = 0; i < NUM_RX_BD; i++) {
		struct sw_rx_bd *rx_buf = &fp->rx_buf_ring[i];
		struct sk_buff *skb = rx_buf->skb;

		if (skb == NULL)
			continue;
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
		dma_unmap_single(&bp->pdev->dev,
				 dma_unmap_addr(rx_buf, mapping),
				 bp->rx_buf_size, DMA_FROM_DEVICE);
#else
		pci_unmap_single(bp->pdev,
				 pci_unmap_addr(rx_buf, mapping),
				 bp->rx_buf_size, PCI_DMA_FROMDEVICE);
#endif

		rx_buf->skb = NULL;
		dev_kfree_skb(skb);
	}
}

static void bnx2x_free_fp_mem_at(struct bnx2x *bp, int fp_index)
{
	union host_hc_status_block *sb = &bnx2x_fp(bp, fp_index, status_blk);
	struct bnx2x_fastpath *fp = &bp->fp[fp_index];

	/* Common */
#ifdef BCM_CNIC
	/* OOO and Forwarding clients use CNIC status block
	 * FCoE client uses default status block
	 */
	if ((IS_OOO_IDX(fp_index) ||
	     IS_FWD_IDX(fp_index) ||
	     IS_FCOE_IDX(fp_index))) {
		memset(sb, 0, sizeof(union host_hc_status_block));
		fp->status_blk_mapping = 0;

	} else {
#endif
		/* status blocks */
		if (CHIP_IS_E2(bp))
			BNX2X_PCI_FREE(sb->e2_sb,
				       bnx2x_fp(bp, fp_index,
						status_blk_mapping),
				       sizeof(struct host_hc_status_block_e2));
		else
			BNX2X_PCI_FREE(sb->e1x_sb,
				       bnx2x_fp(bp, fp_index,
						status_blk_mapping),
				       sizeof(struct host_hc_status_block_e1x));
#ifdef BCM_CNIC
	}
#endif
	/* Rx */
	if (!skip_rx_queue(bp, fp_index)) {
		bnx2x_free_rx_bds(fp);

		/* fastpath rx rings: rx_buf rx_desc rx_comp */
		BNX2X_FREE(bnx2x_fp(bp, fp_index, rx_buf_ring));
		BNX2X_PCI_FREE(bnx2x_fp(bp, fp_index, rx_desc_ring),
			       bnx2x_fp(bp, fp_index, rx_desc_mapping),
			       sizeof(struct eth_rx_bd) * NUM_RX_BD);

		BNX2X_PCI_FREE(bnx2x_fp(bp, fp_index, rx_comp_ring),
			       bnx2x_fp(bp, fp_index, rx_comp_mapping),
			       sizeof(struct eth_fast_path_rx_cqe) *
			       NUM_RCQ_BD);

		/* SGE ring */
		BNX2X_FREE(bnx2x_fp(bp, fp_index, rx_page_ring));
		BNX2X_PCI_FREE(bnx2x_fp(bp, fp_index, rx_sge_ring),
			       bnx2x_fp(bp, fp_index, rx_sge_mapping),
			       BCM_PAGE_SIZE * NUM_RX_SGE_PAGES);
	}

	/* Tx */
	if (!skip_tx_queue(bp, fp_index)) {
		/* fastpath tx rings: tx_buf tx_desc */
		BNX2X_FREE(bnx2x_fp(bp, fp_index, tx_buf_ring));
		BNX2X_PCI_FREE(bnx2x_fp(bp, fp_index, tx_desc_ring),
			       bnx2x_fp(bp, fp_index, tx_desc_mapping),
			       sizeof(union eth_tx_bd_types) * NUM_TX_BD);
	}
	/* end of fastpath */
}

void bnx2x_free_fp_mem(struct bnx2x *bp)
{
	int i;
	for_each_queue(bp, i)
		bnx2x_free_fp_mem_at(bp, i);
}

static inline void set_sb_shortcuts(struct bnx2x *bp, int index) {
	union host_hc_status_block status_blk = bnx2x_fp(bp, index, status_blk);
	if (CHIP_IS_E2(bp)) {
		bnx2x_fp(bp, index, sb_index_values) =
			(__le16*)status_blk.e2_sb->sb.index_values;
		bnx2x_fp(bp, index, sb_running_index) =
			(__le16*)status_blk.e2_sb->sb.running_index;
	} else {
		bnx2x_fp(bp, index, sb_index_values) =
			(__le16*)status_blk.e1x_sb->sb.index_values;
		bnx2x_fp(bp, index, sb_running_index) =
			(__le16*)status_blk.e1x_sb->sb.running_index;
	}
}

static int bnx2x_alloc_fp_mem_at(struct bnx2x *bp, int index) {
	union host_hc_status_block *sb;
	struct bnx2x_fastpath *fp = &bp->fp[index];
	int ring_size = 0;

	bnx2x_fp(bp, index, bp) = bp;
	bnx2x_fp(bp, index, index) = index;

	/* Common */
	sb = &bnx2x_fp(bp, index, status_blk);
#ifdef BCM_CNIC
	if (IS_OOO_IDX(index) || IS_FWD_IDX(index)) {
		if (CHIP_IS_E2(bp))
			sb->e2_sb = bp->cnic_sb.e2_sb;
		else
			sb->e1x_sb = bp->cnic_sb.e1x_sb;

		bnx2x_fp(bp, index, status_blk_mapping) =
			bp->cnic_sb_mapping;
	} else if (!IS_FCOE_IDX(index)){
#endif
	/* status blocks */
	if (CHIP_IS_E2(bp))
		BNX2X_PCI_ALLOC(sb->e2_sb,
			&bnx2x_fp(bp, index, status_blk_mapping),
			sizeof(struct host_hc_status_block_e2));
	else
		BNX2X_PCI_ALLOC(sb->e1x_sb,
			&bnx2x_fp(bp, index, status_blk_mapping),
		    sizeof(struct host_hc_status_block_e1x));
#ifdef BCM_CNIC
	}
#endif
	set_sb_shortcuts(bp, index);

	/* Tx */
	if(!skip_tx_queue(bp, index)) {
		/* fastpath tx rings: tx_buf tx_desc */
		BNX2X_ALLOC(bnx2x_fp(bp, index, tx_buf_ring),
				sizeof(struct sw_tx_bd) * NUM_TX_BD);
		BNX2X_PCI_ALLOC(bnx2x_fp(bp, index, tx_desc_ring),
				&bnx2x_fp(bp, index, tx_desc_mapping),
				sizeof(union eth_tx_bd_types) * NUM_TX_BD);
	}

	/* Rx */
	if (!skip_rx_queue(bp, index)) {
		/* fastpath rx rings: rx_buf rx_desc rx_comp */
		BNX2X_ALLOC(bnx2x_fp(bp, index, rx_buf_ring),
				sizeof(struct sw_rx_bd) * NUM_RX_BD);
		BNX2X_PCI_ALLOC(bnx2x_fp(bp, index, rx_desc_ring),
				&bnx2x_fp(bp, index, rx_desc_mapping),
				sizeof(struct eth_rx_bd) * NUM_RX_BD);

		BNX2X_PCI_ALLOC(bnx2x_fp(bp, index, rx_comp_ring),
				&bnx2x_fp(bp, index, rx_comp_mapping),
				sizeof(struct eth_fast_path_rx_cqe) *
				NUM_RCQ_BD);

		/* SGE ring */
		BNX2X_ALLOC(bnx2x_fp(bp, index, rx_page_ring),
				sizeof(struct sw_rx_page) * NUM_RX_SGE);
		BNX2X_PCI_ALLOC(bnx2x_fp(bp, index, rx_sge_ring),
				&bnx2x_fp(bp, index, rx_sge_mapping),
				BCM_PAGE_SIZE * NUM_RX_SGE_PAGES);
		/* RX BD ring */
		bnx2x_set_next_page_rx_bd(fp);

		/* CQ ring */
		bnx2x_set_next_page_rx_cq(fp);

		/* BDs */
#ifdef BCM_CNIC
		if (IS_OOO_FP(fp)) {
			ring_size = bnx2x_alloc_ooo_rx_bd_ring(fp);
			if (ring_size <	 min_t(int,
			/* Delete me! For integration only! */
					       min_t(int,
						     bp->tx_ring_size / 2,
						     500),
/*					       min_t(int,
						     bp->tx_ring_size / 2,
						     bp->rx_ring_size),
 */
					       INIT_OOO_RING_SIZE))
				goto alloc_mem_err;
		} else {
#endif
		ring_size = bnx2x_alloc_rx_bds(fp, bp->rx_ring_size);
		if (ring_size < bp->rx_ring_size)
			goto alloc_mem_err;
#ifdef BCM_CNIC
		}
#endif
	}

	return 0;

/* handles low memory cases */
alloc_mem_err:
	BNX2X_ERR("Unable to allocate full memory for queue %d (size %d)\n",
						index, ring_size);
	/* FW will drop all packets if queue is not big enough,
	 * In these cases we disable the queue
	 * Min size diferent for OOO, TPA and non-TPA queues
	 */
	if ((IS_OOO_IDX(index) && ring_size < MIN_RX_SIZE_OOO) ||
	    (!IS_OOO_IDX(index) &&
		ring_size < (fp->disable_tpa ?
				MIN_RX_SIZE_NONTPA : MIN_RX_SIZE_TPA))) {
			/* release memory allocated for this queue */
			bnx2x_free_fp_mem_at(bp, index);
			return -ENOMEM;
	}
	return 0;
}


int bnx2x_alloc_fp_mem(struct bnx2x *bp)
{
	int i;

	/**
	 * 1. Allocate FP for leading - fatal if error
	 * 2. {CNIC} Allocate FCoE FP - fatal if error
	 * 3. {CNIC} Allocate OOO + FWD - disable OOO if error
	 * 4. Allocate RSS - fix number of queues if error
	 */

	/* leading */
	if (bnx2x_alloc_fp_mem_at(bp, 0))
		return -ENOMEM;
#ifdef BCM_CNIC
	/* FCoE */
	if (bnx2x_alloc_fp_mem_at(bp, FCOE_IDX))
		return -ENOMEM;

	/* OOO + FWD */
	if (bnx2x_alloc_fp_mem_at(bp, OOO_IDX))
		bp->flags |= NO_ISCSI_OOO_FLAG;
	else if (bnx2x_alloc_fp_mem_at(bp, FWD_IDX)) {
		bnx2x_free_fp_mem_at(bp, OOO_IDX);
		bp->flags |= NO_ISCSI_OOO_FLAG;
	}
#endif
	/* RSS */
	for_each_nondefault_eth_queue(bp, i)
		if (bnx2x_alloc_fp_mem_at(bp, i))
			break;

	/* handle memory failures */
	if(i != BNX2X_NUM_ETH_QUEUES(bp)) {
		int delta = BNX2X_NUM_ETH_QUEUES(bp) - i;
#ifdef BCM_CNIC
		/**
		 * move non eth FPs next to last eth FP
		 * must be done in that order
		 * FCOE_IDX < FWD_IDX < OOO_IDX
		 */

		/* move FCoE fp */
		bnx2x_move_fp(bp, FCOE_IDX, FCOE_IDX - delta);
		/* move OOO and FWD - even NO_ISCSI_OOO_FLAG is on */
		bnx2x_move_fp(bp, FWD_IDX, FWD_IDX - delta);
		bnx2x_move_fp(bp, OOO_IDX, OOO_IDX - delta);
#endif
		bp->num_queues -= delta;
		BNX2X_ERR("Adjusted num of queues from %d to %d\n",
			  bp->num_queues + delta, bp->num_queues);
#ifdef BNX2X_MULTI_QUEUE /* BNX2X_UPSTREAM */
		bp->dev->real_num_tx_queues -= delta;
#endif
	}

	return 0;
}

static void bnx2x_free_tx_skbs(struct bnx2x *bp)
{
	int i;

	for_each_tx_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		u16 bd_cons = fp->tx_bd_cons;
		u16 sw_prod = fp->tx_pkt_prod;
		u16 sw_cons = fp->tx_pkt_cons;

		while (sw_cons != sw_prod) {
			bd_cons = bnx2x_free_tx_pkt(bp, fp, TX_BD(sw_cons));
			sw_cons++;
		}
	}
}

void bnx2x_init_rx_rings(struct bnx2x *bp)
{
	int func = BP_FUNC(bp);
	int max_agg_queues = CHIP_IS_E1(bp) ? ETH_MAX_AGGREGATION_QUEUES_E1 :
					      ETH_MAX_AGGREGATION_QUEUES_E1H;
	u16 ring_prod;
	int i, j;

	DP(NETIF_MSG_IFUP,
	   "mtu %d  rx_buf_size %d\n", bp->dev->mtu, bp->rx_buf_size);

	/* Allocate TPA resources */
	for_each_rx_queue(bp, j) {
		struct bnx2x_fastpath *fp = &bp->fp[j];

		if (!fp->disable_tpa) {
			/* Fill the per-aggregtion pool */
			for (i = 0; i < max_agg_queues; i++) {
				fp->tpa_pool[i].skb =
					netdev_alloc_skb(bp->dev,
							 bp->rx_buf_size);
				if (!fp->tpa_pool[i].skb) {
					BNX2X_ERR("Failed to allocate TPA "
						  "skb pool for queue[%d] - "
						  "disabling TPA on this "
						  "queue!\n", j);
					bnx2x_free_tpa_pool(bp, fp, i);
					fp->disable_tpa = 1;
					break;
				}
#if (LINUX_VERSION_CODE >= 0x020622) && !defined(BNX2X_OLD_FCOE) /* BNX2X_UPSTREAM */
				dma_unmap_addr_set((struct sw_rx_bd *)
#else
				pci_unmap_addr_set((struct sw_rx_bd *)
#endif
							&bp->fp->tpa_pool[i],
						   mapping, 0);
				fp->tpa_state[i] = BNX2X_TPA_STOP;
			}

			/* "next page" elements initialization */
			bnx2x_set_next_page_sgl(fp);

			/* set SGEs bit mask */
			bnx2x_init_sge_ring_bit_mask(fp);

			/* Allocate SGEs and initialize the ring elements */
			for (i = 0, ring_prod = 0;
			     i < MAX_RX_SGE_CNT*NUM_RX_SGE_PAGES; i++) {

				if (bnx2x_alloc_rx_sge(bp, fp, ring_prod) < 0) {
					BNX2X_ERR("was only able to allocate "
						  "%d rx sges\n", i);
					BNX2X_ERR("disabling TPA for queue[%d]\n", j);
					/* Cleanup already allocated elements */
					bnx2x_free_rx_sge_range(bp, fp, ring_prod);
					bnx2x_free_tpa_pool(bp, fp, max_agg_queues);
					fp->disable_tpa = 1;
					ring_prod = 0;
					break;
				}
				ring_prod = NEXT_SGE_IDX(ring_prod);
			}

			fp->rx_sge_prod = ring_prod;
		}
	}

	for_each_rx_queue(bp, j) {
		struct bnx2x_fastpath *fp = &bp->fp[j];

		fp->rx_bd_cons = 0;

		/* Activate BD ring */
		/* Warning!
		 * this will generate an interrupt (to the TSTORM)
		 * must only be done after chip is initialized
		 */
#ifdef BCM_CNIC
		if (IS_OOO_FP(fp))
			bnx2x_update_ooo_prod(bp, fp, fp->rx_bd_prod,
					      fp->rx_comp_prod,
					      fp->rx_sge_prod);
		else
#endif
		bnx2x_update_rx_prod(bp, fp, fp->rx_bd_prod, fp->rx_comp_prod,
				     fp->rx_sge_prod);

		if (j != 0)
			continue;

		if (!CHIP_IS_E2(bp)){
			REG_WR(bp, BAR_USTRORM_INTMEM +
			       USTORM_MEM_WORKAROUND_ADDRESS_OFFSET(func),
			       U64_LO(fp->rx_comp_mapping));
			REG_WR(bp, BAR_USTRORM_INTMEM +
			       USTORM_MEM_WORKAROUND_ADDRESS_OFFSET(func) + 4,
			       U64_HI(fp->rx_comp_mapping));
		}
	}
}

static void bnx2x_free_rx_skbs(struct bnx2x *bp)
{
	int j;

	for_each_rx_queue(bp, j) {
		struct bnx2x_fastpath *fp = &bp->fp[j];

		bnx2x_free_rx_bds(fp);

		if (!fp->disable_tpa)
			bnx2x_free_tpa_pool(bp, fp, CHIP_IS_E1(bp) ?
					    ETH_MAX_AGGREGATION_QUEUES_E1 :
					    ETH_MAX_AGGREGATION_QUEUES_E1H);
	}
}

void bnx2x_free_skbs(struct bnx2x *bp)
{
	bnx2x_free_tx_skbs(bp);
	bnx2x_free_rx_skbs(bp);
}

void bnx2x_napi_enable(struct bnx2x *bp)
{
	int i;

	for_each_napi_queue(bp, i)
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
		napi_enable(&bnx2x_fp(bp, i, napi));
#else
		netif_poll_enable(&bnx2x_fp(bp, i, dummy_netdev));
#endif
}

void bnx2x_napi_disable(struct bnx2x *bp)
{
	int i;

	for_each_napi_queue(bp, i)
#ifdef BNX2X_NEW_NAPI /* BNX2X_UPSTREAM */
		napi_disable(&bnx2x_fp(bp, i, napi));
#else
		netif_poll_disable(&bnx2x_fp(bp, i, dummy_netdev));
#endif
}

/* Fill Link report data according to the current link
 * configuration.
 *
 * @param bp
 * @param data
 */
static inline void bnx2x_fill_report_data(struct bnx2x *bp,
					  struct bnx2x_link_report_data *data)
{
	u16 line_speed = bp->link_vars.line_speed;

	/* Calculate the current MAX line speed limit for the DCC capable
	 * devices.
	 */
	if (IS_MF(bp)) {
		u16 vn_max_rate = ((bp->mf_config[BP_VN(bp)] &
					FUNC_MF_CFG_MAX_BW_MASK) >>
					FUNC_MF_CFG_MAX_BW_SHIFT) * 100;

		if (vn_max_rate < line_speed)
			line_speed = vn_max_rate;
	}

	memset(data, 0, sizeof(*data));

	/* Fill the report data: efective line speed */
	data->line_speed = line_speed;

	/* Link is down */
	if (!bp->link_vars.link_up || (bp->flags & MF_FUNC_DIS))
		set_bit(BNX2X_LINK_REPORT_LINK_DOWN, &data->link_report_flags);

	/* Full DUPLEX */
	if (bp->link_vars.duplex == DUPLEX_FULL)
		set_bit(BNX2X_LINK_REPORT_FD, &data->link_report_flags);

	/* Rx Flow Control is ON */
	if (bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_RX)
		set_bit(BNX2X_LINK_REPORT_RX_FC_ON, &data->link_report_flags);

	/* Tx Flow Control is ON */
	if (bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_TX)
		set_bit(BNX2X_LINK_REPORT_TX_FC_ON, &data->link_report_flags);
}

void bnx2x_link_report(struct bnx2x *bp)
{
	struct bnx2x_link_report_data cur_data;

	/* reread mf_cfg */
	if (!CHIP_IS_E1(bp))
		bnx2x_read_mf_cfg(bp);

	/* Read the current link report info */
	bnx2x_fill_report_data(bp, &cur_data);

	/* Don't report link down or exactly the same link status twice */
	if (!memcmp(&cur_data, &bp->last_reported_link, sizeof(cur_data)) ||
	    (test_bit(BNX2X_LINK_REPORT_LINK_DOWN,
		      &bp->last_reported_link.link_report_flags) &&
	     test_bit(BNX2X_LINK_REPORT_LINK_DOWN,
		      &cur_data.link_report_flags)))
		return;

	bp->link_cnt++;

	/* We are going to report a new link parameters now -
	 * remember the current data for the next time.
	 */
	memcpy(&bp->last_reported_link, &cur_data, sizeof(cur_data));

	if (test_bit(BNX2X_LINK_REPORT_LINK_DOWN,
		     &cur_data.link_report_flags)) {
		netif_carrier_off(bp->dev);
		netdev_err(bp->dev, "NIC Link is Down\n");
		return;
	} else {
		netif_carrier_on(bp->dev);
		netdev_info(bp->dev, "NIC Link is Up, ");
		pr_cont("%d Mbps ", cur_data.line_speed);

		if (test_and_clear_bit(BNX2X_LINK_REPORT_FD,
				       &cur_data.link_report_flags))
			pr_cont("full duplex");
		else
			pr_cont("half duplex");

		/* Handle the FC at the end so that only these flags would be
		 * possibly set. This way we may easily check if there is no FC
		 * enabled.
		 */
		if (cur_data.link_report_flags) {
			if (test_bit(BNX2X_LINK_REPORT_RX_FC_ON,
				     &cur_data.link_report_flags)) {
				pr_cont(", receive ");
				if (test_bit(BNX2X_LINK_REPORT_TX_FC_ON,
				     &cur_data.link_report_flags))
					pr_cont("& transmit ");
			} else {
				pr_cont(", transmit ");
			}
			pr_cont("flow control ON");
		}
		pr_cont("\n");
	}
}

/**
 * Free previously requested MSI-X IRQ vectors
 *
 * @param bp
 * @param nvecs Number of vectors from bp->msix_table[]
 *                   that have been requested and thus have to
 *                   be released.
 */
void bnx2x_free_msix_irqs(struct bnx2x *bp, int nvecs)
{
	int i, offset = 0;

	if (nvecs != offset) {
		/* VFs don't have a default SB */
		if (HAS_DSB(bp)) {
			offset++;
			free_irq(bp->msix_table[0].vector, bp->dev);
			DP(NETIF_MSG_IFDOWN, "released sp irq (%d)\n",
			   bp->msix_table[0].vector);
		}
	} else
		return;

#ifdef BCM_CNIC
	if (nvecs != offset) {
#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
		if (bp->flags & OWN_CNIC_IRQ) {
			free_irq(bp->msix_table[offset].vector, bnx2x_ooo_fp(bp));
			bp->flags &= ~OWN_CNIC_IRQ;
		}
#endif
		offset++;
	} else
		return;
#endif

	for_each_eth_queue(bp, i) {
		if (nvecs != offset) {
			DP(NETIF_MSG_IFDOWN, "about to release fp #%d->%d irq  "
			   "state %x\n", i, bp->msix_table[offset].vector,
			   bnx2x_fp(bp, i, state));

			free_irq(bp->msix_table[offset++].vector, &bp->fp[i]);
		} else
			return;
	}
}

void bnx2x_free_irq(struct bnx2x *bp)
{
	if (bp->flags & USING_MSIX_FLAG) {
		bnx2x_free_msix_irqs(bp, BNX2X_NUM_ETH_QUEUES(bp) +
				     CNIC_CONTEXT_USE + 1);
	} else if (bp->flags & USING_MSI_FLAG) {
		free_irq(bp->pdev->irq, bp->dev);
	} else
		free_irq(bp->pdev->irq, bp->dev);
}

int bnx2x_enable_msix(struct bnx2x *bp)
{
	int msix_vec = 0, i, rc, req_cnt;

	/* No Default SB for VFs */
	if (HAS_DSB(bp)) {
		bp->msix_table[msix_vec].entry = msix_vec;
		msix_vec++;

		DP(NETIF_MSG_IFUP, "msix_table[0].entry = %d (slowpath)\n",
		   bp->msix_table[0].entry);
	}

#ifdef BCM_CNIC
	bp->msix_table[msix_vec].entry = msix_vec;
	DP(NETIF_MSG_IFUP, "msix_table[%d].entry = %d (CNIC)\n",
	   bp->msix_table[msix_vec].entry, bp->msix_table[msix_vec].entry);
	msix_vec++;
#endif
	/* We need separate vectors for ETH queues only (not FCoE) */
	for_each_eth_queue(bp, i) {
		bp->msix_table[msix_vec].entry = msix_vec;
		DP(NETIF_MSG_IFUP, "msix_table[%d].entry = %d "
		   "(fastpath #%u)\n", msix_vec, msix_vec, i);
		msix_vec++;
	}

	req_cnt = BNX2X_NUM_ETH_QUEUES(bp)+ CNIC_CONTEXT_USE +
		  (HAS_DSB(bp) ? 1 : 0);

	rc = pci_enable_msix(bp->pdev, &bp->msix_table[0], req_cnt);

	/*
	 * reconfigure number of tx/rx queues according to available
	 * MSI-X vectors
	 */
	if (rc >= BNX2X_MIN_MSIX_VEC_CNT) {
		/* how less vectors we will have? */
		int diff = req_cnt - rc;

		DP(NETIF_MSG_IFUP,
		   "Trying to use less MSI-X vectors: %d\n", rc);

		rc = pci_enable_msix(bp->pdev, &bp->msix_table[0], rc);

		if (rc) {
			DP(NETIF_MSG_IFUP,
			   "MSI-X is not attainable  rc %d\n", rc);
			return rc;
		}
		/*
		 * decrease number of queues by number of unallocated entries
		 */
		bp->num_queues -= diff;

		DP(NETIF_MSG_IFUP, "New queue configuration set: %d\n",
				  bp->num_queues);
	} else if (rc) {
		/* fall to INTx if not enough memory */
		if (rc == -ENOMEM)
			bp->flags |= DISABLE_MSI_FLAG;
		DP(NETIF_MSG_IFUP, "MSI-X is not attainable  rc %d\n", rc);
		return rc;
	}

	bp->flags |= USING_MSIX_FLAG;

	return 0;
}


int bnx2x_req_msix_irqs(struct bnx2x *bp)
{
	int i, rc, offset = 0;

	if (HAS_DSB(bp)) {
		rc = request_irq(bp->msix_table[offset++].vector, bnx2x_msix_sp_int, 0,
				 bp->dev->name, bp->dev);
		if (rc) {
			BNX2X_ERR("request sp irq failed\n");
			return -EBUSY;
		}
	}

#ifdef BCM_CNIC
#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
	if (!NO_ISCSI_OOO(bp)) {
		snprintf(bnx2x_ooo(bp, name), sizeof(bnx2x_ooo(bp, name)),
			 "%s-fp-ooo", bp->dev->name);
		rc = request_irq(bp->msix_table[offset].vector,
					 bnx2x_msix_cnic_int, 0,
				 bnx2x_ooo(bp, name), bnx2x_ooo_fp(bp));
		if (rc) {
			BNX2X_ERR("request for cnic irq (%d) failed  rc %d\n",
			      bp->msix_table[offset].vector, rc);
			bnx2x_free_msix_irqs(bp, offset);
			return -EBUSY;
		}
		bnx2x_ooo(bp, state) = BNX2X_FP_STATE_IRQ;
		bp->flags |= OWN_CNIC_IRQ;
	}
#endif
	offset++;
#endif
	for_each_eth_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];
		snprintf(fp->name, sizeof(fp->name),"%s-fp-%d",
			 bp->dev->name, i);

		rc = request_irq(bp->msix_table[offset].vector,
				 bnx2x_msix_fp_int, 0, fp->name, fp);
		if (rc) {
			BNX2X_ERR("request fp #%d irq (%d) failed  rc %d\n", i,
			      bp->msix_table[offset].vector, rc);
			bnx2x_free_msix_irqs(bp, offset);
			return -EBUSY;
		}

		offset++;
		fp->state = BNX2X_FP_STATE_IRQ;
	}

	i = BNX2X_NUM_ETH_QUEUES(bp);
	if (HAS_DSB(bp)) {
		offset = 1 + CNIC_CONTEXT_USE;
		netdev_info(bp->dev, "using MSI-X  IRQs: sp %d  fp[%d] %d"
		       " ... fp[%d] %d\n",
		       bp->msix_table[0].vector,
		       0, bp->msix_table[offset].vector,
		       i - 1, bp->msix_table[offset + i - 1].vector);
	}
	else {
		offset = CNIC_CONTEXT_USE;
		netdev_info(bp->dev, "using MSI-X  IRQs: fp[%d] %d"
		       " ... fp[%d] %d\n",
		       0, bp->msix_table[offset].vector,
		       i - 1, bp->msix_table[offset + i - 1].vector);
	}

	return 0;
}

int bnx2x_enable_msi(struct bnx2x *bp)
{
	int rc;

	rc = pci_enable_msi(bp->pdev);
	if (rc) {
		DP(NETIF_MSG_IFUP, "MSI is not attainable\n");
		return -1;
	}
	bp->flags |= USING_MSI_FLAG;

	return 0;
}

int bnx2x_req_irq(struct bnx2x *bp)
{
	unsigned long flags;
	int rc;

	if (bp->flags & USING_MSI_FLAG)
		flags = 0;
	else
		flags = IRQF_SHARED;

	rc = request_irq(bp->pdev->irq, bnx2x_interrupt, flags,
			 bp->dev->name, bp->dev);
	if (!rc)
		bnx2x_fp(bp, 0, state) = BNX2X_FP_STATE_IRQ;

	return rc;
}

int bnx2x_setup_irqs(struct bnx2x *bp)
{
	int rc = 0;
	if (bp->flags & USING_MSIX_FLAG) {
		rc = bnx2x_req_msix_irqs(bp);
		if (rc)
			return rc;
	} else {
		/* Fall to INTx if failed to enable MSI-X due to lack of
		   memory (in bnx2x_set_int_mode()) */
		if (!(bp->flags & DISABLE_MSI_FLAG) && (int_mode != INT_MODE_INTx))
			bnx2x_enable_msi(bp);
		bnx2x_ack_int(bp);
		rc = bnx2x_req_irq(bp);
		if (rc) {
			BNX2X_ERR("IRQ request failed  rc %d, aborting\n", rc);
			return rc;
		}
		if (bp->flags & USING_MSI_FLAG) {
			bp->dev->irq = bp->pdev->irq;
			netdev_info(bp->dev, "using MSI  IRQ %d\n",
			       bp->pdev->irq);
		}
	}

	return 0;
}

void bnx2x_netif_start(struct bnx2x *bp)
{
	int intr_sem;

	intr_sem = atomic_dec_and_test(&bp->intr_sem);
	smp_wmb(); /* Ensure that bp->intr_sem update is SMP-safe */

	if (intr_sem) {
		if (netif_running(bp->dev)) {
			bnx2x_napi_enable(bp);
			bnx2x_int_enable(bp);
			if (bp->state == BNX2X_STATE_OPEN)
				netif_tx_wake_all_queues(bp->dev);
		}
	}
}

void bnx2x_netif_stop(struct bnx2x *bp, int disable_hw)
{
	bnx2x_int_disable_sync(bp, disable_hw);
#if defined(BNX2X_NEW_NAPI) || defined(USE_NAPI_GRO) /* BNX2X_UPSTREAM */
	bnx2x_napi_disable(bp);
#else
	if (netif_running(bp->dev))
		bnx2x_napi_disable(bp);
#endif
}

#if defined(BNX2X_SAFC) || (defined(BCM_CNIC) && defined(BNX2X_MULTI_QUEUE)) /* ! BNX2X_UPSTREAM */
static inline int bnx2x_safc_select_queue(struct bnx2x *bp,
					  struct net_device *dev,
					  struct sk_buff *skb,
					  u32 adjust_for_fcoe)
{
	int fp_index = 0;

#ifdef BNX2X_SAFC
	int i;

	/* Determine which tx ring we will be placed on */
	switch (bp->multi_mode) {
	case ETH_RSS_MODE_VLAN_PRI:
	case ETH_RSS_MODE_E1HOV_PRI:
#ifdef BCM_VLAN
		if ((bp->vlgrp != NULL) && vlan_tx_tag_present(skb)) {
			i = ((vlan_tx_tag_get(skb) >> 13) & 0x7);
			fp_index = bp->cos_map[bp->pri_map[i]];
		}
#endif
		break;

	case ETH_RSS_MODE_IP_DSCP:
		if (skb->protocol == htons(ETH_P_IP)) {
			i = ((ip_hdr(skb)->tos >> 2) & 0x7);
			fp_index = bp->cos_map[bp->pri_map[i]];
		}
		break;
	default:
		fp_index = skb_tx_hash(dev, skb) - adjust_for_fcoe;
	}
#else
	fp_index = skb_tx_hash(dev, skb) - adjust_for_fcoe;
#endif
	return fp_index;
}

u16 bnx2x_select_queue(struct net_device *dev, struct sk_buff *skb)
{
	int fp_index = 0;
	struct bnx2x *bp = netdev_priv(dev);

#ifdef BCM_CNIC
	if (NO_FCOE(bp))
		/* Select according to SAFC if defined. no fcoe adjustment */
		return bnx2x_safc_select_queue(bp, dev, skb, 0);
	else {
		struct ethhdr *hdr = (struct ethhdr *)skb->data;
		u16 ether_type = ntohs(hdr->h_proto);

		/* Skip VLAN tag if present */
		if (ether_type == ETH_P_8021Q) {
			struct vlan_ethhdr *vhdr =
				(struct vlan_ethhdr *)skb->data;

			ether_type = ntohs(vhdr->h_vlan_encapsulated_proto);
		}

		/* If ethertype is FCoE or FIP - use FCoE ring */
		if ((ether_type == ETH_P_FCOE) || (ether_type == ETH_P_FIP))
			return bnx2x_fcoe(bp, index);
	}
#endif
	/* Select according to SAFC (if defined) adjust for fcoe */
	fp_index = bnx2x_safc_select_queue(bp, dev, skb,
					   FCOE_CONTEXT_USE);
	return (fp_index >= 0 ? fp_index : 0);
}
#endif

#if !defined(BNX2X_NEW_NAPI) && defined(USE_NAPI_GRO) /* ! BNX2X_UPSTREAM */
int  __bnx2x_poll(struct napi_struct *napi, int budget)
{
	int _budget = budget;

	napi->dev->poll(napi->dev, &_budget);
	return (budget - _budget);
}
#endif

void bnx2x_set_num_queues(struct bnx2x *bp)
{
#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
	int i;
#endif

	switch (bp->multi_mode) {
	case ETH_RSS_MODE_DISABLED:
		bp->num_queues = 1;
		break;

	case ETH_RSS_MODE_REGULAR:
		bp->num_queues = bnx2x_calc_num_queues(bp);
		break;

#ifdef BNX2X_SAFC /* ! BNX2X_UPSTREAM */
	case ETH_RSS_MODE_VLAN_PRI:
	case ETH_RSS_MODE_E1HOV_PRI:
	case ETH_RSS_MODE_IP_DSCP:
		bp->num_queues = 0;
		for (i = 0; i < BNX2X_MAX_COS; i++)
			bp->num_queues += bp->qs_per_cos[i];
		break;
#endif

	default:
		bp->num_queues = 1;
		break;
	}

	/* Add special queues */
	bp->num_queues += NONE_ETH_CONTEXT_USE;
}

#ifdef BCM_CNIC
static inline void bnx2x_set_fcoe_eth_macs(struct bnx2x *bp)
{
	if (!NO_FCOE(bp)) {
		if (!IS_MF(bp))
			bnx2x_set_fip_eth_mac_addr(bp, 1);
		bnx2x_set_all_enode_macs(bp, 1);
		bp->flags |= FCOE_MACS_SET;
	}
}
#endif


static inline int bnx2x_config_rss_pf(struct bnx2x *bp)
{
	struct bnx2x_config_rss_params params = {0};
	int i;

	params.rss_obj = &bp->rss_conf_obj;

	set_bit(RAMROD_COMP_WAIT, &params.ramrod_flags);

	/* RSS mode */
	switch (bp->multi_mode) {
	case ETH_RSS_MODE_DISABLED:
		set_bit(BNX2X_RSS_MODE_DISABLED, &params.rss_flags);
		break;
	case ETH_RSS_MODE_REGULAR:
		set_bit(BNX2X_RSS_MODE_REGULAR, &params.rss_flags);
		break;
	case ETH_RSS_MODE_VLAN_PRI:
		set_bit(BNX2X_RSS_MODE_VLAN_PRI, &params.rss_flags);
		break;
	case ETH_RSS_MODE_E1HOV_PRI:
		set_bit(BNX2X_RSS_MODE_E1HOV_PRI, &params.rss_flags);
		break;
	case ETH_RSS_MODE_IP_DSCP:
		set_bit(BNX2X_RSS_MODE_IP_DSCP, &params.rss_flags);
		break;
	case ETH_RSS_MODE_E2_INTEG:
		set_bit(BNX2X_RSS_MODE_E2_INTEG, &params.rss_flags);
		break;
	default:
		BNX2X_ERR("Unknown multi_mode: %d\n", bp->multi_mode);
		return -EINVAL;
	}

	/* If RSS is enabled */
	if (bp->multi_mode != ETH_RSS_MODE_DISABLED) {
		/* RSS configuration */
		set_bit(BNX2X_RSS_IPV4, &params.rss_flags);
		set_bit(BNX2X_RSS_IPV4_TCP, &params.rss_flags);
		set_bit(BNX2X_RSS_IPV6, &params.rss_flags);
		set_bit(BNX2X_RSS_IPV6_TCP, &params.rss_flags);

		/* RSS scope */
		set_bit(BNX2X_RSS_UPDATE_ETH, &params.rss_flags);

		/* Hash bits */
		params.rss_result_mask = MULTI_MASK;


#ifdef BNX2X_SAFC
		for (i = 0; i < sizeof(params.ind_table); i++) {
			int cos = bp->pri_map[i / BNX2X_MAX_ENTRIES_PER_PRI];
			params.ind_table[i] = bp->fp->cl_id + bp->cos_map[cos] +
			      (i % bp->qs_per_cos[cos]);
		}
#else /* BNX2X_UPSTREAM */
		for (i = 0; i < sizeof(params.ind_table); i++)
			params.ind_table[i] = bp->fp->cl_id +
				(i % (bp->num_queues - NONE_ETH_CONTEXT_USE));
#endif

		/* RSS keys: think of taking the upstream version (random32()) */
		for (i = 0; i < sizeof(params.rss_key) / 4; i++)
			params.rss_key[i] = 0xc0cac01a;
	}

	return bnx2x_config_rss(bp, &params);
}

#ifndef BNX2X_STOP_ON_ERROR
#define LOAD_ERROR_EXIT(bp, label) goto label
#else
#define LOAD_ERROR_EXIT(bp, label) \
	do { \
		(bp)->panic = 1; \
		return -EBUSY; \
	} while (0)
#endif


/* NIC load - must be called with rtnl_lock */
int bnx2x_nic_load(struct bnx2x *bp, int load_mode)
{
	int port = BP_PORT(bp);
	u32 load_code;
	int i, rc;

#ifdef BNX2X_STOP_ON_ERROR
	if (unlikely(bp->panic))
		return -EPERM;
#endif

	bp->state = BNX2X_STATE_OPENING_WAIT4_LOAD;

	bp->rx_buf_size = bp->dev->mtu + ETH_OVREHEAD + BNX2X_RX_ALIGN +
		BNX2X_FW_IP_HDR_ALIGN_PAD;

	/* Set the initial link reported state to an illegal value. This will
	 * ensure that any link state change will be immediatelly reported.
	 */
	memset(&bp->last_reported_link, 0, sizeof(bp->last_reported_link));

	/* must be called before memory allocation and HW init */
	bnx2x_ilt_set_info(bp);

	/* zero fastpath structures preserving invariants like napi which are
	 * allocated only once
	 */
	for_each_queue(bp, i)
		bnx2x_bz_fp(bp, i);

	/* set the tpa flag for each queue. The tpa flag determines the queue
	 * minimal size so it must be set prior to queue memory allocation
	 */

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
	/* disable LRO (TPA) for the default queue */
	bnx2x_fp(bp, 0, disable_tpa) = 1;

	 /* Iterate over the net-queues determining which support LRO (TPA) */
	bnx2x_reserve_netq_feature(bp);

#else /* Linux */
       for_each_queue(bp, i)
		bnx2x_fp(bp, i, disable_tpa) =
					((bp->flags & TPA_ENABLE_FLAG) == 0);
#endif

#ifdef BCM_CNIC
	/* We don't want TPA on FCoE, FWD and OOO L2 rings */
	bnx2x_fcoe(bp, disable_tpa) = 1;
	bnx2x_fwd(bp, disable_tpa) = 1;
	bnx2x_ooo(bp, disable_tpa) = 1;
#endif

	if (bnx2x_alloc_mem(bp))
		return -ENOMEM;

	bnx2x_napi_enable(bp);

	/* Send LOAD_REQUEST command to MCP
	 * Returns the type of LOAD command:
	 * if it is the first port to be initialized
	 * common blocks should be initialized, otherwise - not
	 */
	if (!BP_NOMCP(bp)) {
		load_code = bnx2x_fw_command(bp, DRV_MSG_CODE_LOAD_REQ, 0);
		if (!load_code) {
			BNX2X_ERR("MCP response failure, aborting\n");
			rc = -EBUSY;
			goto load_error1;
		}
		if (load_code == FW_MSG_CODE_DRV_LOAD_REFUSED) {
			rc = -EBUSY; /* other port in diagnostic mode */
			goto load_error1;
		}

	} else {
		int path = BP_PATH(bp);

		DP(NETIF_MSG_IFUP, "NO MCP - load counts[%d]      %d, %d, %d\n",
		   path, load_count[path][0], load_count[path][1],
		   load_count[path][2]);
		load_count[path][0]++;
		load_count[path][1 + port]++;
		DP(NETIF_MSG_IFUP, "NO MCP - new load counts[%d]  %d, %d, %d\n",
		   path, load_count[path][0], load_count[path][1],
		   load_count[path][2]);
		if (load_count[path][0] == 1)
			load_code = FW_MSG_CODE_DRV_LOAD_COMMON;
		else if (load_count[path][1 + port] == 1)
			load_code = FW_MSG_CODE_DRV_LOAD_PORT;
		else
			load_code = FW_MSG_CODE_DRV_LOAD_FUNCTION;
	}

	if ((load_code == FW_MSG_CODE_DRV_LOAD_COMMON) ||
	    (load_code == FW_MSG_CODE_DRV_LOAD_COMMON_CHIP) ||
	    (load_code == FW_MSG_CODE_DRV_LOAD_PORT))
		bp->port.pmf = 1;
	else
		bp->port.pmf = 0;
	DP(NETIF_MSG_LINK, "pmf %d\n", bp->port.pmf);

	/* Initialize HW */
	rc = bnx2x_init_hw(bp, load_code);
	if (rc) {
		BNX2X_ERR("HW init failed, aborting\n");
		bnx2x_fw_command(bp, DRV_MSG_CODE_LOAD_DONE, 0);
		goto load_error2;
	}

	/* Connect to IRQs */
	rc = bnx2x_setup_irqs(bp);
	if (rc) {
		bnx2x_fw_command(bp, DRV_MSG_CODE_LOAD_DONE, 0);
		goto load_error2;
	}

	/* Setup NIC internals and enable interrupts */
	bnx2x_nic_init(bp, load_code);

	/* Init per-function objects */
	bnx2x_init_bp_objs(bp);

#ifdef BCM_IOV /* ! BNX2X_UPSTREAM */
	bnx2x_iov_nic_init(bp);
#endif
	if (((load_code == FW_MSG_CODE_DRV_LOAD_COMMON) ||
	    (load_code == FW_MSG_CODE_DRV_LOAD_COMMON_CHIP)) &&
	    (bp->common.shmem2_base))
		SHMEM2_WR(bp, dcc_support,
			  (SHMEM_DCC_SUPPORT_DISABLE_ENABLE_PF_TLV |
			   SHMEM_DCC_SUPPORT_BANDWIDTH_ALLOCATION_TLV));

	/* Send LOAD_DONE command to MCP */
	if (!BP_NOMCP(bp)) {
		load_code = bnx2x_fw_command(bp, DRV_MSG_CODE_LOAD_DONE, 0);
		if (!load_code) {
			BNX2X_ERR("MCP response failure, aborting\n");
			rc = -EBUSY;
			goto load_error3;
		}
	}


	/* TEMPORARY disabled for 6.1 branch ONLY
	 * do not integrate into other branches
	 */
	/* bnx2x_dcbx_init(bp); */

	bp->state = BNX2X_STATE_OPENING_WAIT4_PORT;

	rc = bnx2x_func_start(bp);
	if (rc) {
		BNX2X_ERR("Function start failed!\n");
		LOAD_ERROR_EXIT(bp, load_error3);
	}

	rc = bnx2x_setup_leading(bp);
	if (rc) {
		BNX2X_ERR("Setup leading failed!\n");
		LOAD_ERROR_EXIT(bp, load_error3);
	}

	if (!CHIP_IS_E1(bp) &&
	    (bp->mf_config[BP_VN(bp)] & FUNC_MF_CFG_FUNC_DISABLED)) {
		DP(NETIF_MSG_IFUP, "mf_cfg function disabled\n");
		bp->flags |= MF_FUNC_DIS;
	}

#ifdef BCM_CNIC
	/* Enable Timer scan */
	REG_WR(bp, TM_REG_EN_LINEAR0_TIMER + port*4, 1);
#endif

	for_each_nondefault_queue(bp, i) {
		rc = bnx2x_setup_client(bp, &bp->fp[i], 0);
		if (rc) {
#ifdef BCM_CNIC
			LOAD_ERROR_EXIT(bp, load_error4);
#else
			LOAD_ERROR_EXIT(bp, load_error3);
#endif
		}
	}
#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */

	/* For ESX RSS is disabled. The RSS configuration command with an 
	 * RSS-disabled flag will over-write the the HW RSS-hash 
	 * configuration. As HW RSS-hash is required for proper TPA
	 * opearion we avoid sendig the RSS configuration command all-together.  
	 */
	bp->multi_mode = ETH_RSS_MODE_DISABLED;
#else
	rc = bnx2x_config_rss_pf(bp);
	if (rc)
#ifdef BCM_CNIC
		LOAD_ERROR_EXIT(bp, load_error4);
#else
		LOAD_ERROR_EXIT(bp, load_error3);
#endif
#endif

	/* Now when Clients are configured we are ready to work */
	bp->state = BNX2X_STATE_OPEN;

#ifdef BCM_CNIC
	bnx2x_set_fcoe_eth_macs(bp);
#endif

	/* Configure a ucast MAC */
	bnx2x_set_eth_mac(bp, 1);

	if (bp->port.pmf || IS_VF(bp))
		bnx2x_initial_phy_init(bp, load_mode);

	/* Start fast path */

	/* Initialize Rx filter. */
	netif_addr_lock_bh(bp->dev);
	bnx2x_set_rx_mode(bp->dev);
	netif_addr_unlock_bh(bp->dev);

	/* Start the Tx */
	switch (load_mode) {
	case LOAD_NORMAL:
		/* Tx queue should be only reenabled */
		netif_tx_wake_all_queues(bp->dev);
		break;

	case LOAD_OPEN:
		netif_tx_start_all_queues(bp->dev);
		smp_mb__after_clear_bit();
		break;

	case LOAD_DIAG:
		bp->state = BNX2X_STATE_DIAG;
		break;

	default:
		break;
	}

	if (!bp->port.pmf)
		bnx2x__link_status_update(bp);

	/* start the timer */
	mod_timer(&bp->timer, jiffies + bp->current_interval);

#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
	/*
	 * vmknetddi_queueops_invalidate_state(bp->dev) should be called
	 * here instead of being littered throughout the code right after
	 * bnx2x_nic_load
	 */
	bp->n_rx_queues_allocated = 0;
	bp->n_tx_queues_allocated = 0;
#endif

#ifdef BCM_CNIC
#if !defined(__VMKLNX__)        /* BNX2X_UPSTREAM */
	/* Release CNIC's IRQ now, CNIC will connect to it */
	if ((bp->flags & USING_MSIX_FLAG) && (!NO_ISCSI_OOO(bp))) {
		int cnic_idx = HAS_DSB(bp) ? 1 : 0;
		synchronize_irq(bp->msix_table[cnic_idx].vector);
		free_irq(bp->msix_table[cnic_idx].vector, bnx2x_ooo_fp(bp));
		bp->flags &= ~OWN_CNIC_IRQ;
	}
#endif

	bnx2x_setup_cnic_irq_info(bp);
	if (bp->state == BNX2X_STATE_OPEN)
		bnx2x_cnic_notify(bp, CNIC_CTL_START_CMD);
#endif
	bnx2x_inc_load_cnt(bp);

#ifdef BCM_VLAN
	/* Configure HW VLAN stripping if there are registered VLAN devices */
	if (bp->vlgrp)
		bnx2x_vlan_rx_register(bp->dev, bp->vlgrp);
#endif

	return 0;

#ifdef BCM_CNIC
#ifndef BNX2X_STOP_ON_ERROR
load_error4:
#endif
	/* Disable Timer scan */
	REG_WR(bp, TM_REG_EN_LINEAR0_TIMER + port*4, 0);
#endif
load_error3:
	bnx2x_int_disable_sync(bp, 1);

	/* Free SKBs, SGEs, TPA pool and driver internals */
	bnx2x_free_skbs(bp);
	for_each_rx_queue(bp, i)
		bnx2x_free_rx_sge_range(bp, bp->fp + i, NUM_RX_SGE);

	/* Release IRQs */
	bnx2x_free_irq(bp);
load_error2:
	if (!BP_NOMCP(bp)) {
		bnx2x_fw_command(bp, DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP, 0);
		bnx2x_fw_command(bp, DRV_MSG_CODE_UNLOAD_DONE, 0);
	}

	bp->port.pmf = 0;
load_error1:
	bnx2x_napi_disable(bp);
	bnx2x_free_mem(bp);

	return rc;
}


/* must be called with rtnl_lock */
int bnx2x_nic_unload(struct bnx2x *bp, int unload_mode)
{
	int i;

	if (bp->state == BNX2X_STATE_CLOSED) {
		/* We can get here only if the driver has been unloaded
		 * during parity error recovery and is either waiting for a
		 * leader to complete of for other functions to unload and
		 * then ifdown has been issued. In this case we want to
		 * unload and let other functions to complete a recovery
		 * process by releasing LEADER lock.
		 */
		bp->recovery_state = BNX2X_RECOVERY_DONE;
		bp->is_leader = 0;
		bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_RESERVED_08);
		smp_wmb();

		return -EINVAL;
	}
#if defined(__VMKLNX__)	/* ! BNX2X_UPSTREAM */
	/*
	 * On older version of ESX 'device close' could be called with no
	 * prior successful call to 'device open'. The valid states at this
	 * point are either 'open' (device open) or 'diag' (self-test)
	 */
	if(bp->state != BNX2X_STATE_OPEN && bp->state != BNX2X_STATE_DIAG) {
		BNX2X_ERR("called dev_close() with no prior successful call "
			  "to dev_open()\n");
		return -EBUSY;
	}
#endif

#ifdef BCM_CNIC
	bnx2x_cnic_notify(bp, CNIC_CTL_STOP_CMD);

#if !defined(__VMKLNX__) /* BNX2X_UPSTREAM */
	/* Reaquire the CNIC's IRQ */
	if ((!NO_ISCSI_OOO(bp)) && (bp->flags & USING_MSIX_FLAG)
		&& (!(bp->flags & OWN_CNIC_IRQ))) {
		int cnic_idx = HAS_DSB(bp) ? 1 : 0;
		if (request_irq(bp->msix_table[cnic_idx].vector,
			 bnx2x_msix_cnic_int, 0, bnx2x_ooo(bp, name), bnx2x_ooo_fp(bp)))
			BNX2X_ERR("Failed to connect to CNIC IRQ\n");
		else
			bp->flags |= OWN_CNIC_IRQ;
	}
#endif
#endif
	bp->state = BNX2X_STATE_CLOSING_WAIT4_HALT;
	smp_mb();

	bp->rx_mode = BNX2X_RX_MODE_NONE;

	/* Stop Tx */
	bnx2x_tx_disable(bp);

#if (LINUX_VERSION_CODE < 0x02061f) /* ! BNX2X_UPSTREAM */
	/* In kernels starting from 2.6.31 netdev layer does this */
	bp->dev->trans_start = jiffies;	/* prevent tx timeout */
#endif

	del_timer_sync(&bp->timer);

	/* Set ALWAYS_ALIVE bit in shmem */
	bp->fw_drv_pulse_wr_seq |= DRV_PULSE_ALWAYS_ALIVE;

#ifndef __VMKLNX__ /* Remove FW pulse timer update */ /* BNX2X_UPSTREAM */
	bnx2x_drv_pulse(bp);
#endif /* !__VMKLNX__ */

	bnx2x_stats_handle(bp, STATS_EVENT_STOP);

	/* Cleanup the chip if needed */
	if (unload_mode != UNLOAD_RECOVERY)
		bnx2x_chip_cleanup(bp, unload_mode);
	else {
		/* Disable HW interrupts, NAPI */
		bnx2x_netif_stop(bp, 1);

		/* Release IRQs */
		bnx2x_free_irq(bp);
	}

	bp->port.pmf = 0;

	/* Free SKBs, SGEs, TPA pool and driver internals */
	bnx2x_free_skbs(bp);
	for_each_rx_queue(bp, i)
		bnx2x_free_rx_sge_range(bp, bp->fp + i, NUM_RX_SGE);

	bnx2x_free_mem(bp);

	bp->state = BNX2X_STATE_CLOSED;



	/* The last driver must disable a "close the gate" if there is no
	 * parity attention or "process kill" pending.
	 */
	if ((!bnx2x_dec_load_cnt(bp)) && (!bnx2x_chk_parity_attn(bp)) &&
	    bnx2x_reset_is_done(bp))
		bnx2x_disable_close_the_gate(bp);

	/* Reset MCP mail box sequence if there is on going recovery */
	if (unload_mode == UNLOAD_RECOVERY)
		bp->fw_seq = 0;

	return 0;
}

int bnx2x_set_power_state(struct bnx2x *bp, pci_power_t state)
{
	u16 pmcsr;

	/* If there is no power capability, silently succeed */
	if (!bp->pm_cap) {
		DP(NETIF_MSG_HW, "No power capability. Breaking.\n");
		return 0;
	}

	pci_read_config_word(bp->pdev, bp->pm_cap + PCI_PM_CTRL, &pmcsr);

	switch (state) {
	case PCI_D0:
		pci_write_config_word(bp->pdev, bp->pm_cap + PCI_PM_CTRL,
				      ((pmcsr & ~PCI_PM_CTRL_STATE_MASK) |
				       PCI_PM_CTRL_PME_STATUS));

		if (pmcsr & PCI_PM_CTRL_STATE_MASK)
			/* delay required during transition out of D3hot */
			msleep(20);
		break;

	case PCI_D3hot:
#if (LINUX_VERSION_CODE >= 0x020614) /* BNX2X_UPSTREAM */
		/* If there are other clients above don't
		   shut down the power */
		if (atomic_read(&bp->pdev->enable_cnt) != 1)
			return 0;
#endif
		/* Don't shut down the power for emulation and FPGA */
		if (CHIP_REV_IS_SLOW(bp))
			return 0;

		pmcsr &= ~PCI_PM_CTRL_STATE_MASK;
		pmcsr |= 3;

		if (bp->wol)
			pmcsr |= PCI_PM_CTRL_PME_ENABLE;

		pci_write_config_word(bp->pdev, bp->pm_cap + PCI_PM_CTRL,
				      pmcsr);

		/* No more memory access after this point until
		* device is brought back to D0.
		*/
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

void bnx2x_free_mem_bp(struct bnx2x *bp)
{
	if (bp->fp)
		kfree(bp->fp);
	if (bp->msix_table)
		kfree(bp->msix_table);
	if (bp->ilt)
		kfree(bp->ilt);
}

int __devinit bnx2x_alloc_mem_bp(struct bnx2x *bp)
{
	struct bnx2x_fastpath *fp;
	struct msix_entry *tbl;
	struct bnx2x_ilt *ilt;

	/* fp array */
	fp = kzalloc(L2_FP_COUNT(bp->l2_cid_count)*sizeof(*fp), GFP_KERNEL);
	if (!fp)
		goto alloc_err;
	bp->fp = fp;

	/* msix table */
	tbl = kzalloc((FP_SB_COUNT(bp->l2_cid_count) + 1) * sizeof(*tbl),
				  GFP_KERNEL);
	if (!tbl)
		goto alloc_err;
	bp->msix_table = tbl;

	/* ilt */
	ilt = kzalloc(sizeof(*ilt), GFP_KERNEL);
	if (!ilt)
		goto alloc_err;
	bp->ilt = ilt;

	return 0;
alloc_err:
	bnx2x_free_mem_bp(bp);
	return -ENOMEM;

}

/* Common ethtool_ops */
/* All ethtool functions called with rtnl_lock */
int bnx2x_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct bnx2x *bp = netdev_priv(dev);
	int cfg_idx = bnx2x_get_link_cfg_idx(bp);
	/* Dual Media boards present all available port types */
	cmd->supported = bp->port.supported[cfg_idx] |
		(bp->port.supported[cfg_idx ^ 1] &
		 (SUPPORTED_TP | SUPPORTED_FIBRE));
	cmd->advertising = bp->port.advertising[cfg_idx];

	if ((bp->state == BNX2X_STATE_OPEN) &&
	     !(bp->flags & MF_FUNC_DIS) &&
	    (bp->link_vars.link_up)) {
		cmd->speed = bp->link_vars.line_speed;
		cmd->duplex = bp->link_vars.duplex;
	} else {

		cmd->speed = bp->link_params.req_line_speed[cfg_idx];
		cmd->duplex = bp->link_params.req_duplex[cfg_idx];
	}
	if (IS_MF(bp)) {
		u16 vn_max_rate = ((bp->mf_config[BP_VN(bp)] &
			FUNC_MF_CFG_MAX_BW_MASK) >> FUNC_MF_CFG_MAX_BW_SHIFT) *
			100;

		if (vn_max_rate < cmd->speed)
			cmd->speed = vn_max_rate;
	}

	if (bp->port.supported[cfg_idx] & SUPPORTED_TP)
		cmd->port = PORT_TP;
	else if(bp->port.supported[cfg_idx] & SUPPORTED_FIBRE)
		cmd->port = PORT_FIBRE;
	else
		BNX2X_ERR("XGXS PHY Failure detected\n");

	cmd->phy_address = bp->mdio.prtad;
	cmd->transceiver = XCVR_INTERNAL;

	if (bp->link_params.req_line_speed[cfg_idx] == SPEED_AUTO_NEG)
		cmd->autoneg = AUTONEG_ENABLE;
	else
		cmd->autoneg = AUTONEG_DISABLE;

	cmd->maxtxpkt = 0;
	cmd->maxrxpkt = 0;

	DP(NETIF_MSG_LINK, "ethtool_cmd: cmd %d\n"
	   DP_LEVEL "  supported 0x%x  advertising 0x%x  speed %d\n"
	   DP_LEVEL "  duplex %d  port %d  phy_address %d  transceiver %d\n"
	   DP_LEVEL "  autoneg %d  maxtxpkt %d  maxrxpkt %d\n",
	   cmd->cmd, cmd->supported, cmd->advertising,
	   cmd->speed, cmd->duplex, cmd->port, cmd->phy_address,
	   cmd->transceiver, cmd->autoneg, cmd->maxtxpkt, cmd->maxrxpkt);

	return 0;
}

u32 bnx2x_get_msglevel(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	return bp->msg_enable;
}

void bnx2x_set_msglevel(struct net_device *dev, u32 level)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (capable(CAP_NET_ADMIN))
		bp->msg_enable = level;
}

int bnx2x_get_coalesce(struct net_device *dev,
			      struct ethtool_coalesce *coal)
{
	struct bnx2x *bp = netdev_priv(dev);

	memset(coal, 0, sizeof(struct ethtool_coalesce));

	coal->rx_coalesce_usecs = bp->rx_ticks;
	coal->tx_coalesce_usecs = bp->tx_ticks;

	return 0;
}

void bnx2x_get_ringparam(struct net_device *dev,
			struct ethtool_ringparam *ering)
{
	struct bnx2x *bp = netdev_priv(dev);

	ering->rx_max_pending = MAX_RX_AVAIL;
	ering->rx_mini_max_pending = 0;
	ering->rx_jumbo_max_pending = 0;

	ering->rx_pending = bp->rx_ring_size;
	ering->rx_mini_pending = 0;
	ering->rx_jumbo_pending = 0;

	ering->tx_max_pending = MAX_TX_AVAIL;
	ering->tx_pending = bp->tx_ring_size;
}

int bnx2x_set_ringparam(struct net_device *dev,
		       struct ethtool_ringparam *ering)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc = 0;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		netdev_err(dev, "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	if ((ering->rx_pending > MAX_RX_AVAIL) ||
	    (ering->tx_pending > MAX_TX_AVAIL) ||
	    (ering->tx_pending <= MAX_SKB_FRAGS + 4))
		return -EINVAL;

	bp->rx_ring_size = ering->rx_pending;
	bp->tx_ring_size = ering->tx_pending;

	if (netif_running(dev)) {
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
		/*
		 * Invalidate netqueue state as filters
		 * have been lost after reinit.
		 */
		vmknetddi_queueops_invalidate_state(dev);
#endif
	}

	return rc;
}

int bnx2x_get_link_cfg_idx(struct bnx2x *bp)
{
	u32 sel_phy_idx = 0;
	if (bp->link_vars.link_up) {
		sel_phy_idx = EXT_PHY1;
		/* In case link is SERDES, check if the EXT_PHY2 is the one */
		if ((bp->link_vars.link_status & LINK_STATUS_SERDES_LINK) &&
		    (bp->link_params.phy[EXT_PHY2].supported & SUPPORTED_FIBRE))
			sel_phy_idx = EXT_PHY2;
	} else {

		switch (bnx2x_phy_selection(&bp->link_params)) {
		case PORT_HW_CFG_PHY_SELECTION_HARDWARE_DEFAULT:
		case PORT_HW_CFG_PHY_SELECTION_FIRST_PHY:
		case PORT_HW_CFG_PHY_SELECTION_FIRST_PHY_PRIORITY:
		       sel_phy_idx = EXT_PHY1;
		       break;
		case PORT_HW_CFG_PHY_SELECTION_SECOND_PHY:
		case PORT_HW_CFG_PHY_SELECTION_SECOND_PHY_PRIORITY:
		       sel_phy_idx = EXT_PHY2;
		       break;
		}
	}
	/*
	* The selected actived PHY is always after swapping (in case PHY
	* swapping is enabled). So when swapping is enabled, we need to reverse
	* the configuration
	*/

	if (bp->link_params.multi_phy_config &
	    PORT_HW_CFG_PHY_SWAPPED_ENABLED) {
		if (sel_phy_idx == EXT_PHY1)
			sel_phy_idx = EXT_PHY2;
		else if (sel_phy_idx == EXT_PHY2)
			sel_phy_idx = EXT_PHY1;
	}
	return LINK_CONFIG_IDX(sel_phy_idx);
}

void bnx2x_get_pauseparam(struct net_device *dev,
				 struct ethtool_pauseparam *epause)
{
	struct bnx2x *bp = netdev_priv(dev);
	int cfg_idx = bnx2x_get_link_cfg_idx(bp);
	epause->autoneg = (bp->link_params.req_flow_ctrl[cfg_idx] ==
			   BNX2X_FLOW_CTRL_AUTO);

	epause->rx_pause = ((bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_RX) ==
			    BNX2X_FLOW_CTRL_RX);
	epause->tx_pause = ((bp->link_vars.flow_ctrl & BNX2X_FLOW_CTRL_TX) ==
			    BNX2X_FLOW_CTRL_TX);

	DP(NETIF_MSG_LINK, "ethtool_pauseparam: cmd %d\n"
	   DP_LEVEL "  autoneg %d  rx_pause %d  tx_pause %d\n",
	   epause->cmd, epause->autoneg, epause->rx_pause, epause->tx_pause);
}

#if (LINUX_VERSION_CODE >= 0x02061a) /* BNX2X_UPSTREAM */
int bnx2x_set_flags(struct net_device *dev, u32 data)
{
	struct bnx2x *bp = netdev_priv(dev);
	int changed = 0;
	int rc = 0;

	if (data & ~(ETH_FLAG_LRO | ETH_FLAG_RXHASH))
		return -EINVAL;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		netdev_err(dev, "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	/* TPA requires Rx CSUM offloading */
	if ((data & ETH_FLAG_LRO) && bp->rx_csum) {
		if (! disable_tpa) {
			if (!(dev->features & NETIF_F_LRO)) {
				dev->features |= NETIF_F_LRO;
				bp->flags |= TPA_ENABLE_FLAG;
				changed = 1;
			}
		} else
			rc = -EINVAL;

	} else if (dev->features & NETIF_F_LRO) {
		dev->features &= ~NETIF_F_LRO;
		bp->flags &= ~TPA_ENABLE_FLAG;
		changed = 1;
	}

#if (LINUX_VERSION_CODE > 0x020622) /* BNX2X_UPSTREAM */
	/* Rx Toeplitz hash HW acceleration */
	if (data & ETH_FLAG_RXHASH)
		dev->features |= NETIF_F_RXHASH;
	else
		dev->features &= ~NETIF_F_RXHASH;
#endif

	if (changed && netif_running(dev)) {
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
		/*
		 * Invalidate netqueue state as filters
		 * have been lost after reinit.
		 */
		vmknetddi_queueops_invalidate_state(dev);
#endif
	}

	return rc;
}
#endif

u32 bnx2x_get_rx_csum(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

	return bp->rx_csum;
}

int bnx2x_set_rx_csum(struct net_device *dev, u32 data)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc = 0;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		netdev_err(dev, "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	bp->rx_csum = data;

	/* Disable TPA, when Rx CSUM is disabled. Otherwise all
	   TPA'ed packets will be discarded due to wrong TCP CSUM */
	if (!data) {
#if (LINUX_VERSION_CODE >= 0x02061a) /* BNX2X_UPSTREAM */
		u32 flags = ethtool_op_get_flags(dev);

		rc = bnx2x_set_flags(dev, (flags & ~ETH_FLAG_LRO));
#else
		bp->flags &= ~TPA_ENABLE_FLAG;
		if (netif_running(dev)) {
			bnx2x_nic_unload(bp, UNLOAD_NORMAL);
			rc = bnx2x_nic_load(bp, LOAD_NORMAL);
#ifdef BNX2X_NETQ /* ! BNX2X_UPSTREAM */
			/*
			 * Invalidate netqueue state as filters
			 * have been lost after reinit.
			 */
			vmknetddi_queueops_invalidate_state(dev);
#endif
		}
#endif
	}

	return rc;
}

#ifdef NETIF_F_TSO /* BNX2X_UPSTREAM */
int bnx2x_set_tso(struct net_device *dev, u32 data)
{
	if (data) {
		dev->features |= (NETIF_F_TSO | NETIF_F_TSO_ECN);
#ifdef NETIF_F_TSO6 /* BNX2X_UPSTREAM */
		dev->features |= NETIF_F_TSO6;
#endif
	} else {
		dev->features &= ~(NETIF_F_TSO | NETIF_F_TSO_ECN);
#ifdef NETIF_F_TSO6 /* BNX2X_UPSTREAM */
		dev->features &= ~NETIF_F_TSO6;
#endif
	}

	return 0;
}
#endif

/* end of ethtool_ops */

/* called with rtnl_lock */
int bnx2x_change_mtu(struct net_device *dev, int new_mtu)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc = 0;

	if (bp->recovery_state != BNX2X_RECOVERY_DONE) {
		netdev_err(dev, "Handling parity error recovery. Try again later\n");
		return -EAGAIN;
	}

	if ((new_mtu > ETH_MAX_JUMBO_PACKET_SIZE) ||
	    ((new_mtu + ETH_HLEN) < ETH_MIN_PACKET_SIZE))
		return -EINVAL;

#ifdef __VMKLNX__ /* ! BNX2X_UPSTREAM */
	if (dev->mtu == new_mtu)
		return rc;
	if (netif_running(dev)) {
#if (VMWARE_ESX_DDK_VERSION < 50000) /* ! BNX2X_UPSTREAM */
		/* There is no need to hold rtnl_lock
		 * when calling change MTU into driver
		 * from VMkernel ESX 5.0 onwards.
		 */
		rtnl_lock();
#endif
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		dev->mtu = new_mtu;

		if (bp->dev->mtu > ETH_MAX_PACKET_SIZE)
			bp->rx_ring_size = INIT_JUMBO_RX_RING_SIZE;
		else
			bp->rx_ring_size = INIT_RX_RING_SIZE;

		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
#if (VMWARE_ESX_DDK_VERSION < 50000) /* ! BNX2X_UPSTREAM */
		rtnl_unlock();
#endif
#if defined(__VMKNETDDI_QUEUEOPS__)
		/* Invalidate netqueue state as filters
		 * have been lost after reinit */
		vmknetddi_queueops_invalidate_state(dev);
#endif
	} else
		dev->mtu = new_mtu;
#else /* ! __VMKLNX__ */

	/* This does not race with packet allocation
	 * because the actual alloc size is
	 * only updated as part of load
	 */
	dev->mtu = new_mtu;

	if (netif_running(dev)) {
		bnx2x_nic_unload(bp, UNLOAD_NORMAL);
		rc = bnx2x_nic_load(bp, LOAD_NORMAL);
	}
#endif

	return rc;
}

void bnx2x_tx_timeout(struct net_device *dev)
{
	struct bnx2x *bp = netdev_priv(dev);

#ifdef BNX2X_STOP_ON_ERROR
	if (!bp->panic)
		bnx2x_panic();
#endif

	/* This allows the netif to be shutdown gracefully before resetting */
	schedule_delayed_work(&bp->reset_task, 0);
}

#ifdef BCM_VLAN

static int bnx2x_set_vlan_stripping(struct bnx2x *bp)
{
	struct bnx2x_client_update_params params = {0};
	int i, rc;

	set_bit(BNX2X_CL_UPDATE_IN_VLAN_REM, &params.update_flags);
	set_bit(BNX2X_CL_UPDATE_IN_VLAN_REM_CHNG, &params.update_flags);

	params.rdata = (void*)bnx2x_sp(bp, client_data.update_data);
	params.rdata_mapping = bnx2x_sp_mapping(bp, client_data.update_data);

	for_each_napi_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		/* Skip FCoE Client for E1x */
		if (IS_FCOE_IDX(i) && CHIP_IS_E1x(bp))
			continue;

		params.cl_id = fp->cl_id;
		params.cid = fp->cid;

		rc = bnx2x_fw_cl_update(bp, &params);
		if (rc)
			return rc;
	}

	return 0;
}


static int bnx2x_clear_vlan_stripping(struct bnx2x *bp)
{
	struct bnx2x_client_update_params params = {0};
	int i, rc;

	set_bit(BNX2X_CL_UPDATE_IN_VLAN_REM_CHNG, &params.update_flags);

	params.rdata = (void*)bnx2x_sp(bp, client_data.update_data);
	params.rdata_mapping = bnx2x_sp_mapping(bp, client_data.update_data);

	for_each_napi_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		/* Skip FCoE Client for E1x */
		if (IS_FCOE_IDX(i) && CHIP_IS_E1x(bp))
			continue;

		params.cl_id = fp->cl_id;
		params.cid = fp->cid;

		rc = bnx2x_fw_cl_update(bp, &params);
		if (rc)
			return rc;
	}

	return 0;
}

/* called with rtnl_lock */
void bnx2x_vlan_rx_register(struct net_device *dev, struct vlan_group *vlgrp)
{
	struct bnx2x *bp = netdev_priv(dev);
	int rc;

	/* Configure VLAN stripping if NIC is up.
	 * Otherwise just set the bp->vlgrp and stripping will be
	 * configured in bnx2x_nic_load().
	 */
	if (bp->state == BNX2X_STATE_OPEN) {
		if (vlgrp != NULL) {
			rc = bnx2x_set_vlan_stripping(bp);

			/* If we failed to configure VLAN stripping we don't
			 * want to use HW accelerated flow in bnx2x_rx_int().
			 * Thus we will leave bp->vlgrp to be equal to NULL to
			 * disable it.
			 */
			if (rc) {
				netdev_err(dev, "Failed to set HW "
						"VLAN stripping\n");
				bnx2x_clear_vlan_stripping(bp);
			} else
				bp->vlgrp = vlgrp;
		} else {
			rc = bnx2x_clear_vlan_stripping(bp);

			if (rc)
				netdev_err(dev, "Failed to clear HW "
						"VLAN stripping\n");

			bp->vlgrp = NULL;
		}
	} else
		bp->vlgrp = vlgrp;
}
#if (LINUX_VERSION_CODE < 0x020616) /* ! BNX2X_UPSTREAM */
void bnx2x_vlan_rx_kill_vid(struct net_device *dev, uint16_t vid)
{
	struct bnx2x *bp = netdev_priv(dev);

	if (bp->vlgrp)
		vlan_group_set_device(bp->vlgrp, vid, NULL);
}
#endif
#endif /* BCM_VLAN */

/* HW Lock for shared dual port PHYs */
void bnx2x_acquire_phy_lock(struct bnx2x *bp)
{
	mutex_lock(&bp->port.phy_mutex);

	if (bp->port.need_hw_lock)
		bnx2x_acquire_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);
}

void bnx2x_release_phy_lock(struct bnx2x *bp)
{
	if (bp->port.need_hw_lock)
		bnx2x_release_hw_lock(bp, HW_LOCK_RESOURCE_MDIO);

	mutex_unlock(&bp->port.phy_mutex);
}

