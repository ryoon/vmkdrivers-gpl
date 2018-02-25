/*
 * Copyright 2009 Cisco Systems, Inc.  All rights reserved.
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

#include <linux/pci.h>

#include "kcompat.h"
#include "enic_res.h"
#include "enic.h"
#include "enic_dev.h"
#include "enic_upt.h"

#define ENIC_UPT_FEATURES_SUPPORTED \
	(VMK_NETVF_F_RXCSUM | VMK_NETVF_F_RSS | VMK_NETVF_F_RXVLAN)

#define ENIC_UPT_NOTIFY_TIMER_PERIOD	(2 * HZ)

struct pass_thru_page {
	struct {
		u32 reg;
		u32 pad;
	} imr[192];	/* 0x0000 + 8 * n */
	struct {
		u32 reg;
		u32 pad;
	} txprod[64];	/* 0x0600 + 8 * n */
	struct {
		u32 reg;
		u32 pad;
	} rxprod1[64];	/* 0x0800 + 8 * n */
	struct {
		u32 reg;
		u32 pad;
	} rxprod2[64];	/* 0x0a00 + 8 * n */
};

extern int enic_dev_init(struct enic *enic);
extern void enic_dev_deinit(struct enic *enic);

void *enic_upt_alloc_bounce_buf(struct enic *enic, size_t size,
	dma_addr_t *dma_handle)
{
	if (enic->upt_active) {
		*dma_handle = enic->upt_oob_pa[ENIC_UPT_OOB_BOUNCE];
		return enic->upt_oob[ENIC_UPT_OOB_BOUNCE];
	} else if (netif_running(enic->netdev)) {
		return pci_alloc_consistent(enic->pdev, size, dma_handle);
	} else {
		*dma_handle = 0;
		return NULL;
	}
}

void enic_upt_free_bounce_buf(struct enic *enic, size_t size,
	void *vaddr, dma_addr_t dma_handle)
{
	if (!enic->upt_active &&
	    netif_running(enic->netdev))
		pci_free_consistent(enic->pdev, size,
			vaddr, dma_handle);
}

static void enic_upt_get_info(struct net_device *netdev, vmk_NetVFInfo *vi)
{
	struct enic *enic = netdev_priv(netdev);

	vi->ptRegions[0].MA = vnic_dev_get_res_bus_addr(enic->vdev,
		RES_TYPE_PASS_THRU_PAGE, 0);
	vi->ptRegions[0].numPages = 1;
	vi->numPtRegions = 1;

	vi->sbdf = ((pci_domain_nr(enic->pdev->bus) & 0xffff) << 16) |
		((enic->pdev->bus->number & 0xff) << 8 ) |
		((PCI_SLOT(enic->pdev->devfn) & 0x1f) << 3) |
		(PCI_FUNC(enic->pdev->devfn) & 0x07);

	vi->u.upt.devRevision = 0;
	vi->u.upt.reserved = 0;
	vi->u.upt.numOOBPages = ENIC_UPT_OOB_MAX;

	enic->upt_mode = 1;
}

static int enic_upt_dev_stats_dump(struct enic *enic)
{
	u64 a0, a1;
	int wait = 1000, err;

	a0 = enic->upt_oob_pa[ENIC_UPT_OOB_STATS];
	a1 = sizeof(struct vnic_stats);

	spin_lock_bh(&enic->devcmd_lock);
	err = vnic_dev_cmd(enic->vdev, CMD_STATS_DUMP, &a0, &a1, wait);
	spin_unlock_bh(&enic->devcmd_lock);

	return err;
}

static void enic_upt_translate_queue_stats(struct vnic_stats *hw_stats,
	vmk_NetVFTXQueueStats *vmw_stats_tx, 
	vmk_NetVFRXQueueStats *vmw_stats_rx)
{
	vmw_stats_tx->TSOPkts = hw_stats->tx.tx_tso;
	vmw_stats_tx->unicastPkts = hw_stats->tx.tx_unicast_frames_ok;
	vmw_stats_tx->unicastBytes = hw_stats->tx.tx_unicast_bytes_ok;
	vmw_stats_tx->multicastPkts = hw_stats->tx.tx_multicast_frames_ok;
	vmw_stats_tx->multicastBytes = hw_stats->tx.tx_multicast_bytes_ok;
	vmw_stats_tx->broadcastPkts = hw_stats->tx.tx_broadcast_frames_ok;
	vmw_stats_tx->broadcastBytes = hw_stats->tx.tx_broadcast_bytes_ok;
	vmw_stats_tx->errors = hw_stats->tx.tx_errors;
	vmw_stats_tx->discards = hw_stats->tx.tx_drops;

	vmw_stats_rx->unicastPkts = hw_stats->rx.rx_unicast_frames_ok;
	vmw_stats_rx->unicastBytes = hw_stats->rx.rx_unicast_bytes_ok;
	vmw_stats_rx->multicastPkts = hw_stats->rx.rx_multicast_frames_ok;
	vmw_stats_rx->multicastBytes = hw_stats->rx.rx_multicast_bytes_ok;
	vmw_stats_rx->broadcastPkts = hw_stats->rx.rx_broadcast_frames_ok;
	vmw_stats_rx->broadcastBytes = hw_stats->rx.rx_broadcast_bytes_ok;
	vmw_stats_rx->outOfBufferDrops = hw_stats->rx.rx_no_bufs;
	vmw_stats_rx->errorDrops = hw_stats->rx.rx_errors;
}

static int enic_upt_get_queue_stats(struct net_device *netdev,
	vmk_NetPTOPVFGetQueueStatsArgs *param)
{
	struct enic *enic = netdev_priv(netdev);
	struct vnic_stats *stats = enic->upt_oob[ENIC_UPT_OOB_STATS];
	vmk_NetVFTXQueueStats stats_tx;
	vmk_NetVFRXQueueStats stats_rx;

	enic_upt_dev_stats_dump(enic);

	memset(param->tqStats, 0, param->numTxQueues *
		sizeof (vmk_NetVFTXQueueStats));
	memset(param->rqStats, 0, param->numRxQueues *
		sizeof (vmk_NetVFRXQueueStats));

	enic_upt_translate_queue_stats(stats,
		&param->tqStats[0], &param->rqStats[0]);
	memcpy(&stats_tx, param->tqStats, sizeof (vmk_NetVFTXQueueStats));
	memcpy(&stats_rx, param->rqStats, sizeof (vmk_NetVFRXQueueStats));

	/* adjust to return delta from last call */
	param->tqStats[0].TSOPkts -= enic->upt_stats_tx.TSOPkts;
	param->tqStats[0].unicastPkts -= enic->upt_stats_tx.unicastPkts;
	param->tqStats[0].unicastBytes -= enic->upt_stats_tx.unicastBytes;
	param->tqStats[0].multicastPkts -= enic->upt_stats_tx.multicastPkts;
	param->tqStats[0].multicastBytes -= enic->upt_stats_tx.multicastBytes;
	param->tqStats[0].broadcastPkts -= enic->upt_stats_tx.broadcastPkts;
	param->tqStats[0].broadcastBytes -= enic->upt_stats_tx.broadcastBytes;
	param->tqStats[0].errors -= enic->upt_stats_tx.errors;
	param->tqStats[0].discards -= enic->upt_stats_tx.discards;
	memcpy(&enic->upt_stats_tx, &stats_tx, sizeof (vmk_NetVFTXQueueStats));

	param->rqStats[0].unicastPkts -= enic->upt_stats_rx.unicastPkts;
	param->rqStats[0].unicastBytes -= enic->upt_stats_rx.unicastBytes;
	param->rqStats[0].multicastPkts -= enic->upt_stats_rx.multicastPkts;
	param->rqStats[0].multicastBytes -= enic->upt_stats_rx.multicastBytes;
	param->rqStats[0].broadcastPkts -= enic->upt_stats_rx.broadcastPkts;
	param->rqStats[0].broadcastBytes -= enic->upt_stats_rx.broadcastBytes;
	param->rqStats[0].outOfBufferDrops -= enic->upt_stats_rx.outOfBufferDrops;
	param->rqStats[0].errorDrops -= enic->upt_stats_rx.errorDrops;
	memcpy(&enic->upt_stats_rx, &stats_rx, sizeof (vmk_NetVFRXQueueStats));

	/* do NOT support per queue stats, return 0 for non-zero queue */

	return 0;
}

static void enic_upt_record_queue_stats(struct enic *enic)
{
	struct vnic_stats *stats = enic->upt_oob[ENIC_UPT_OOB_STATS];

	enic_upt_dev_stats_dump(enic);

	enic_upt_translate_queue_stats(stats,
		&enic->upt_stats_tx, &enic->upt_stats_rx);
}

static VMK_ReturnStatus enic_upt_get_queue_status(struct net_device *netdev,
	vmk_NetPTOPVFGetQueueStatusArgs *param)
{
	struct enic *enic = netdev_priv(netdev);
	unsigned int i, error = 0, error_hits = 0;
	vmk_uint32 tq_error_value, tq_silent_error_value, rq_error_value;
#define SIMULATED_ERROR 31

	if (param->numTxQueues != enic->wq_count) {
		netdev_err(netdev, "TxQueues Count Discrepancy - "
			"VMKernel: %u Enic: %u\n", param->numTxQueues,
			enic->wq_count);
		return VMK_BAD_PARAM;
	}

	if (param->numRxQueues != enic->rq_count) {
		netdev_err(netdev, "RxQueues Count Discrepancy - "
			"VMKernel: %u Enic: %u\n", param->numRxQueues,
			enic->rq_count);
		return VMK_BAD_PARAM;
	}

	for (i = 0; i < enic->wq_count; i++) {
		memset(&param->tqStatus[i], 0, sizeof(param->tqStatus[i]));
		error = vnic_wq_error_status(&enic->wq[i]);
		if (!error &&
			enic->upt_tq_error_stress_status == VMK_OK && 
			vmk_StressOptionValue(enic->upt_tq_error_stress_handle,
			&tq_error_value) == VMK_OK && tq_error_value != 0 &&
			enic->upt_tq_error_stress_counter++ % 
			tq_error_value == 0) {
			netdev_err(enic->netdev,
				"WQ[%u] error induced by stress options\n",
				i);
			vnic_wq_error_out(&enic->wq[i], SIMULATED_ERROR);
			vnic_wq_disable(&enic->wq[i]);
			error = vnic_wq_error_status(&enic->wq[i]);
		}
		
		if (error) {
			error_hits++;
			param->tqStatus[i].stopped = VMK_TRUE;
			param->tqStatus[i].error = error;
			netdev_err(enic->netdev, "WQ[%u] error %u\n",
						i, error);
		}
	}

	for (i = 0; i < enic->rq_count; i++) {
		memset(&param->rqStatus[i], 0, sizeof(param->rqStatus[i]));
		error = vnic_rq_error_status(&enic->rq[i]);
		if (!error && 
			enic->upt_rq_error_stress_status == VMK_OK &&
			vmk_StressOptionValue(enic->upt_rq_error_stress_handle,
			&rq_error_value) == VMK_OK && rq_error_value != 0 &&
			enic->upt_rq_error_stress_counter++ % 
			rq_error_value == 0) {
			netdev_err(enic->netdev,
				"RQ[%u] error induced by stress options\n",
				i);
			vnic_rq_error_out(&enic->rq[i], SIMULATED_ERROR);
			vnic_rq_disable(&enic->rq[i]);
			error = vnic_rq_error_status(&enic->rq[i]);
		}

		if (error) {
			error_hits++;
			param->rqStatus[i].stopped = VMK_TRUE;
			param->rqStatus[i].error = error;
			netdev_err(enic->netdev, "RQ[%u] error %u\n",
				i, error);
		}
	}

	/* Stop a WQ w/o telling vmkernel to trigger a watchdog in guest */
	for (i = 0; !error_hits && (i < enic->wq_count); i++) {
		if (enic->upt_tq_silent_error_stress_status == VMK_OK && 
			vmk_StressOptionValue(
			enic->upt_tq_silent_error_stress_handle,
			&tq_silent_error_value) == VMK_OK && 
			tq_silent_error_value != 0 &&
			enic->upt_tq_silent_error_stress_counter++ % 
			tq_silent_error_value == 0) {
			netdev_err(enic->netdev,
				"Slient stop of WQ[%u] "
				"induced by stress options\n",
				i);
			vnic_wq_disable(&enic->wq[i]);
		}
	}

	return VMK_OK;
}

static int enic_upt_set_nic_cfg(struct enic *enic, vmk_NetVFParameters *vs)
{
	u8 rss_default_cpu;
	u8 rss_hash_type;
	u8 rss_hash_bits;
	u8 rss_base_cpu;
	u8 rss_enable;
	u8 tso_ipid_split_en;
	u8 ig_vlan_strip_en;
	int err;

	/* Setup RSS, VLAN stripping, and TSO IPID
	 */

	netdev_err(enic->netdev, "Features: 0x%x, Optional Features: 0x%x\n",
			vs->features, vs->optFeatures);	
	rss_default_cpu = 0;
	rss_base_cpu = 0; // XXX need to get this from OS
	rss_enable = vs->features & VMK_NETVF_F_RSS ? 1 : 0;
	rss_enable |= vs->optFeatures & VMK_NETVF_F_RSS ? 1 : 0;

	rss_hash_type = 0;
	if (vs->rss.hashType & VMK_NETVF_HASH_TYPE_IPV4)
		rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_IPV4;
	if (vs->rss.hashType & VMK_NETVF_HASH_TYPE_TCP_IPV4)
		rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_TCP_IPV4;
	if (vs->rss.hashType & VMK_NETVF_HASH_TYPE_IPV6)
		rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_IPV6;
	if (vs->rss.hashType & VMK_NETVF_HASH_TYPE_TCP_IPV6)
		rss_hash_type |= NIC_CFG_RSS_HASH_TYPE_TCP_IPV6;

	rss_hash_bits = 0;
	while ((1 << rss_hash_bits) < vs->rss.indTable.indTableSize)
		rss_hash_bits++;

	tso_ipid_split_en = 0; // XXX need to get this from OS

        /* 
	 * Hard coding to always on bc turning it off 
 	 * causes zeroed out 8021q tag to be leaked 
 	 * to the guest OS.
 	 */
	ig_vlan_strip_en = 1;

	spin_lock_bh(&enic->devcmd_lock);
	err = enic_set_nic_cfg(enic,
		rss_default_cpu, rss_hash_type,
		rss_hash_bits, rss_base_cpu,
		rss_enable, tso_ipid_split_en,
		ig_vlan_strip_en);
	spin_unlock_bh(&enic->devcmd_lock);

	return err;
}

static int enic_upt_set_rss_key(struct enic *enic, void *oob_mem,
	dma_addr_t oob_mem_pa, u8 *key, u16 key_size)
{
	union vnic_rss_key *rss_key = oob_mem;
	unsigned int i;
	int err;

	if (key_size == 0)
		return 0;

	if (key_size > VMK_NETVF_RSS_MAX_KEY_SIZE)
		key_size = VMK_NETVF_RSS_MAX_KEY_SIZE;

	memset(rss_key, 0, key_size);

	for (i = 0; i < key_size; i++)
		rss_key->key[i/10].b[i%10] = key[i];

	netdev_err(enic->netdev, "rss key %u bytes\n", key_size);
	spin_lock_bh(&enic->devcmd_lock);
	err = enic_set_rss_key(enic, oob_mem_pa, key_size);
	spin_unlock_bh(&enic->devcmd_lock);

	return err;
}

static int enic_upt_set_rss_ind_table(struct enic *enic, void *oob_mem,
	dma_addr_t oob_mem_pa, u8 *ind_table, u16 ind_table_size)
{
	union vnic_rss_cpu *rss_cpu = oob_mem;
	unsigned int i;
	int err;

	if (ind_table_size == 0)
		return 0;

	if (ind_table_size > VMK_NETVF_RSS_MAX_IND_TABLE_SIZE);
		ind_table_size = VMK_NETVF_RSS_MAX_IND_TABLE_SIZE;

	memset(rss_cpu, 0, ind_table_size);

	for (i = 0; i < ind_table_size; i++)
		rss_cpu->cpu[i/4].b[i%4] = ind_table[i];

	netdev_err(enic->netdev, "rss indtable %u bytes\n", ind_table_size);
	spin_lock_bh(&enic->devcmd_lock);
	err = enic_set_rss_cpu(enic, oob_mem_pa, ind_table_size);
	spin_unlock_bh(&enic->devcmd_lock);

	return err;
}

static void enic_upt_set_oob(struct enic *enic, vmk_NetVFParameters *vs)
{
	unsigned int i;

	for (i = 0; i < ENIC_UPT_OOB_MAX; i++) {
		enic->upt_oob[i] = vs->u.upt.OOBMapped + i * 4096;
		enic->upt_oob_pa[i] = vs->u.upt.OOBStartPA + i * 4096;
	}
}

static void enic_upt_clear_oob(struct enic *enic)
{
	unsigned int i;

	for (i = 0; i < ENIC_UPT_OOB_MAX; i++) {
		enic->upt_oob[i] = NULL;
		enic->upt_oob_pa[i] = 0;
	}
}

static int enic_upt_init_vnic_resources(struct enic *enic,
	vmk_NetVFParameters *vs)
{
	unsigned int mask_on_assertion;
	unsigned int interrupt_offset;
	unsigned int error_interrupt_enable = 0;
	unsigned int error_interrupt_offset = 0;
	unsigned int cq_index;
	unsigned int cq_head;
	unsigned int cq_tail;
	unsigned int cq_tail_color;
	unsigned int i;

	struct vnic_rq *rq;
	struct vnic_wq *wq;
	struct vnic_cq *cq;
	struct vnic_intr *intr;

	vmk_UPTVFRXQueueParams *rs;
	vmk_UPTVFTXQueueParams *ts;

	struct pass_thru_page __iomem *ptp = vnic_dev_get_res(enic->vdev,
		RES_TYPE_PASS_THRU_PAGE, 0);

	/* Init RQ/WQ resources.
	 *
	 * RQ[0 - n-1] point to CQ[0 - n-1]
	 * WQ[0 - m-1] point to CQ[n - n+m-1]
	 */

	for (i = 0; i < enic->rq_count; i++) {

		cq_index = i;
		rs = &vs->u.upt.rxQueues[i];
		rq = &enic->rq[i];
		rq->index = i; 
		rq->vdev = enic->vdev;
		rq->ctrl = vnic_dev_get_res(enic->vdev, RES_TYPE_RQ, i);
		if (!rq->ctrl) {
			pr_err("Failed to hook RQ[%d] resource\n", i);
			return -EINVAL;
		}
		rq->ring.base_addr  = rs->rxRing.basePA;
		rq->ring.desc_count = rs->rxRing.size;

		vnic_rq_init_start(rq,
			cq_index,
			rs->rxRing.consIdx, 
			rs->rxRing.prodIdx,
			error_interrupt_enable,
			error_interrupt_offset);

		rq->enabled = !rs->stopped;
		iowrite32(rs->rxRing2.prodIdx, &ptp->rxprod2[i].reg);
		rq->rxcons2 = rs->rxRing2.consIdx;

		netdev_err(enic->netdev, "RQ[%d]: base %p, size %u, consIdx %u(RB %u), "
			"prodIdx %u(RB %u), cons2Idx %u, prod2Idx %u, "
			"enabled %d\n",
			i, (void *)rq->ring.base_addr,
			rq->ring.desc_count,
			rs->rxRing.consIdx,
			ioread32(&rq->ctrl->fetch_index),
			rs->rxRing.prodIdx,
			ioread32(&rq->ctrl->posted_index),
			rq->rxcons2, ioread32(&ptp->rxprod2[i].reg),
			rq->enabled);
	}

	for (i = 0; i < enic->wq_count; i++) {

		cq_index = enic->rq_count + i;
		ts = &vs->u.upt.txQueues[i];
		wq = &enic->wq[i];
		wq->index = i;      
		wq->vdev = enic->vdev;
		wq->ctrl = vnic_dev_get_res(enic->vdev, RES_TYPE_WQ, i);
		if (!wq->ctrl) {
			pr_err("Failed to hook WQ[%d] resource\n", i);
			return -EINVAL;
		}
		wq->ring.base_addr  = ts->txRing.basePA;
		wq->ring.desc_count = ts->txRing.size;

		vnic_wq_init_start(wq,
			cq_index,
			ts->txRing.consIdx,
			ts->txRing.prodIdx,
			error_interrupt_enable,
			error_interrupt_offset);

		wq->enabled = !ts->stopped;

		netdev_err(enic->netdev, "WQ[%d]: base %p, size %u, consIdx %u(RB %u), "
			"prodIdx %u(RB %u), enabled %d\n",
			i, (void *)wq->ring.base_addr,
			wq->ring.desc_count,
			ts->txRing.consIdx,
			ioread32(&wq->ctrl->fetch_index),
			ts->txRing.prodIdx,
			ioread32(&wq->ctrl->posted_index),
			wq->enabled);
	}

	/* Init CQ resources
	 *
	 * CQ[0 - n+m-1] point to INTR[0 - n+m-1]
	 */

	for (i = 0; i < enic->cq_count; i++) {

		cq = &enic->cq[i];
		cq->index = i;      
		cq->vdev = enic->vdev;
		cq->ctrl = vnic_dev_get_res(enic->vdev, RES_TYPE_CQ, i);
		if (!cq->ctrl) {
			pr_err("Failed to hook CQ[%d] resource\n", i);
			return -EINVAL;
		}

		if (i < enic->rq_count) {
			rs = &vs->u.upt.rxQueues[i];
			cq->ring.base_addr = rs->compRing.basePA;
			cq->ring.desc_count = rs->compRing.size;
			cq_head = rs->compRing.consIdx;
			cq_tail = rs->compRing.prodIdx;
			cq_tail_color = rs->compGen;
			interrupt_offset = rs->intrIdx;
		} else {
			ts = &vs->u.upt.txQueues[i - enic->rq_count];
			cq->ring.base_addr = ts->compRing.basePA;
			cq->ring.desc_count = ts->compRing.size;
			cq_head = ts->compRing.consIdx;
			cq_tail = ts->compRing.prodIdx;
			cq_tail_color = ts->compGen;
			interrupt_offset = ts->intrIdx;
		}

		vnic_cq_init(cq,
			0 /* flow_control_enable */,
			1 /* color_enable */,
			cq_head,
			cq_tail,
			cq_tail_color,
			1 /* interrupt_enable */,
			1 /* cq_entry_enable */,
			0 /* cq_message_enable */,
			interrupt_offset,
			0 /* cq_message_addr */);

		netdev_err(enic->netdev, "CQ[%d]: base %p, size %u, "
			"consIdx %u(RB %u), prodIdx %u(RB %u), "
			"gen %d(RB %u), intrIdx %d\n",
			i, (void *)cq->ring.base_addr,
			cq->ring.desc_count,
			cq_head, ioread32(&cq->ctrl->cq_head),
			cq_tail, ioread32(&cq->ctrl->cq_tail),
			cq_tail_color,
			ioread32(&cq->ctrl->cq_tail_color),
			interrupt_offset);
	}

	/* Init INTR resources
	 */

	netdev_err(enic->netdev, "intr type %d, count %d, autoMask %d\n", 
		vs->u.upt.intr.intrType, vs->numIntrs,
		vs->u.upt.intr.autoMask);

	mask_on_assertion = vs->u.upt.intr.autoMask;

	for (i = 0; i < enic->intr_count; i++) {
		intr = &enic->intr[i];
		intr->index = i;      
		intr->vdev = enic->vdev;
		intr->ctrl = vnic_dev_get_res(enic->vdev, RES_TYPE_INTR_CTRL, i);
		if (!intr->ctrl) {
			pr_err("Failed to hook INTR[%d].ctrl resource\n", i);
			return -EINVAL;
		}
		intr->masked = vs->u.upt.intr.vectors[i].imr;
		vnic_intr_init(intr,
			// XXX interrupt moderation levels
			enic->config.intr_timer_usec,
			enic->config.intr_timer_type,
			mask_on_assertion);
		if (vs->u.upt.intr.vectors[i].pending)
			vnic_intr_raise(intr);
		netdev_err(enic->netdev, "INTR[%d] masked %d pending %d\n",
			i, intr->masked, vs->u.upt.intr.vectors[i].pending);
	}

	/* Setup OOB addresses
	 */

	enic_upt_set_oob(enic, vs);

	return 0;
}

static int enic_upt_restore_state(struct net_device *netdev,
	vmk_NetVFParameters *vs)
{
	struct enic *enic = netdev_priv(netdev);
	int err;

	/* 
 	 * PTS is suppose to kill the Emu2PT transistion if enic doesn't have
 	 * sufficient resources.  Recheck here anyways.
 	 */

	enic_get_res_counts(enic);

	if (enic->wq_count < vs->numTxQueues ||
	    enic->rq_count < vs->numRxQueues ||
	    enic->cq_count < vs->numTxQueues + vs->numRxQueues ||
	    enic->intr_count < vs->numIntrs) {
		netdev_err(netdev, "Insufficent Resources - "
			"Requested: WQ %u RQ %u CQ %u INTR %u - "
			"Available: WQ %u RQ %u CQ %u INTR %u\n",
			 vs->numTxQueues, vs->numRxQueues, 
			vs->numTxQueues + vs->numRxQueues,
			vs->numIntrs,
			enic->wq_count, enic->rq_count, 
			enic->cq_count, enic->intr_count);
		return -EINVAL;
	}

	enic->rq_count = vs->numRxQueues;
	enic->wq_count = vs->numTxQueues;
	enic->cq_count = enic->rq_count + enic->wq_count;
	enic->intr_count = vs->numIntrs;
	
	netdev_err(netdev, "WQ %d, RQ %d, CQ %d, INTR %d\n",
		enic->wq_count, enic->rq_count,
		enic->cq_count, enic->intr_count);

	switch (vs->u.upt.intr.intrType) {
	case VMK_PCI_INTERRUPT_TYPE_MSI:
		vnic_dev_set_intr_mode(enic->vdev, VNIC_DEV_INTR_MODE_MSI);
		break;
	case VMK_PCI_INTERRUPT_TYPE_MSIX:
		vnic_dev_set_intr_mode(enic->vdev, VNIC_DEV_INTR_MODE_MSIX);
		break;
	default:
		netdev_err(netdev, "Invalid intr mode %u\n",
			vs->u.upt.intr.intrType);
		return -EINVAL;
	}

	err = enic_upt_init_vnic_resources(enic, vs);
	if (err) {
		netdev_err(netdev, "Failed to init vNIC resources.\n");
		goto err_out;
	}

	err = enic_upt_set_nic_cfg(enic, vs);
	if (err) {
		netdev_err(netdev, "Failed to set NIC cfg.\n");
		goto err_out;
	}

	err = enic_dev_set_ig_vlan_rewrite_mode(enic);
	if (err) {
		netdev_err(netdev, "Failed to set ingress vlan rewrite mode.\n");
		goto err_out;
	}

	err = enic_upt_set_rss_key(enic,
		enic->upt_oob[ENIC_UPT_OOB_RSS],
		enic->upt_oob_pa[ENIC_UPT_OOB_RSS],
		vs->rss.key.key, vs->rss.key.keySize);
	if (err) {
		netdev_err(netdev, "Failed to set RSS key.\n");
		goto err_out;
	}

	err = enic_upt_set_rss_ind_table(enic,
		enic->upt_oob[ENIC_UPT_OOB_RSS],
		enic->upt_oob_pa[ENIC_UPT_OOB_RSS],
		vs->rss.indTable.indTable, vs->rss.indTable.indTableSize);
	if (err) {
		netdev_err(netdev, "Failed to set RSS indirection table.\n");
		goto err_out;
	}

	// Store the current stats from the device
	enic_upt_record_queue_stats(enic);

	enic->upt_resources_alloced = 1;

	return 0;

err_out:
	enic_upt_clear_oob(enic);

	return err;
}

static void enic_upt_link_check(struct enic *enic)
{
	int link_status = vnic_dev_link_status(enic->vdev);
	int carrier_ok = netif_carrier_ok(enic->netdev);

	if (link_status && !carrier_ok) {
		netdev_info(enic->netdev,"UPT mode Link UP\n");
		netif_carrier_on(enic->netdev);
	} else if (!link_status && carrier_ok) {
		netdev_info(enic->netdev,"UPT mode Link DOWN\n");
		netif_carrier_off(enic->netdev);
	}
}

static void enic_upt_notify_check(struct enic *enic)
{
	enic_upt_link_check(enic);
}

static void enic_upt_notify_timer(unsigned long data)
{
	struct enic *enic = (struct enic *)data;

	enic_upt_notify_check(enic);

	mod_timer(&enic->upt_notify_timer,
		round_jiffies(jiffies + ENIC_UPT_NOTIFY_TIMER_PERIOD));
}

static int enic_upt_notify_set(struct enic *enic)
{
	int r;

	/* 
	 * Assume that when we are called, the notify block has not been 
	 * allocated yet, or any previous notify block has already been 
	 * deallocated
	 *
	 * Disregard the interrupt setting in UPT mode, 
	 * as we always poll since we don't have an interrupt 
	 * vector we can use in UPT mode.
	 */
	spin_lock_bh(&enic->devcmd_lock);
	r = vnic_dev_notify_setcmd(enic->vdev,
		enic->upt_oob[ENIC_UPT_OOB_NOTIFY],
		enic->upt_oob_pa[ENIC_UPT_OOB_NOTIFY],
		-1);
	spin_unlock_bh(&enic->devcmd_lock);

	return r;
}

static int enic_upt_resume(struct net_device *netdev)
{
	struct enic *enic = netdev_priv(netdev);
	unsigned int i, err;

	for (i = 0; i < enic->wq_count; i++)
		if (enic->wq[i].enabled)
			vnic_wq_enable(&enic->wq[i]);

	for (i = 0; i < enic->rq_count; i++)
		if (enic->rq[i].enabled)
			vnic_rq_enable(&enic->rq[i]);

	for (i = 0; i < enic->intr_count; i++)
		if (!enic->intr[i].masked)
			vnic_intr_unmask(&enic->intr[i]);

	enic_dev_enable(enic);

	err = enic_upt_notify_set(enic);
	if (err) {
		netdev_err(netdev, "Failed to alloc notify buffer, aborting.\n");
		return err;
	}

	init_timer(&enic->upt_notify_timer);
	enic->upt_notify_timer.function = enic_upt_notify_timer;
	enic->upt_notify_timer.data = (unsigned long)enic;
	/* In PT mode, always have a timer since we have no interrupts */
	mod_timer(&enic->upt_notify_timer, jiffies);

	enic->upt_active = 1;

	enic->upt_tq_error_stress_status = vmk_StressOptionOpen(
					VMK_STRESS_OPT_NET_IF_FAIL_HARD_TX,
					&enic->upt_tq_error_stress_handle);

	if (enic->upt_tq_error_stress_status != VMK_OK) {
		netdev_err(netdev,
			"Failed to open %s stress option with status %s",
			VMK_STRESS_OPT_NET_IF_FAIL_HARD_TX,
			vmk_StatusToString(enic->upt_tq_error_stress_status));
	}

	/* 
	 * The stress option *_CORRUPT_TX is used to silently trigger 
 	 * watchdogs instead of corrupting tx traffic.
 	 */

	enic->upt_tq_silent_error_stress_status = vmk_StressOptionOpen(
				VMK_STRESS_OPT_NET_IF_CORRUPT_TX,
				&enic->upt_tq_silent_error_stress_handle);

	if (enic->upt_tq_silent_error_stress_status != VMK_OK) {
		netdev_err(netdev,
			"Failed to open %s stress option with status %s",
			VMK_STRESS_OPT_NET_IF_CORRUPT_TX,
			vmk_StatusToString(
			enic->upt_tq_silent_error_stress_status));
	}

	enic->upt_rq_error_stress_status = vmk_StressOptionOpen(
				VMK_STRESS_OPT_NET_IF_FAIL_RX,
				&enic->upt_rq_error_stress_handle);
	if (enic->upt_rq_error_stress_status != VMK_OK) {
		netdev_err(netdev,
			"Failed to open %s stress option with status %s",
			VMK_STRESS_OPT_NET_IF_FAIL_RX,
			vmk_StatusToString(enic->upt_rq_error_stress_status));
	}

	netdev_err(netdev, "resumed\n");

	return 0;
}

static int enic_upt_quiesce(struct net_device *netdev)
{
	struct enic *enic = netdev_priv(netdev);
	unsigned int i;
	int err;
	VMK_ReturnStatus status;

	if (enic->upt_rq_error_stress_status == VMK_OK) {
		status = vmk_StressOptionClose(
				enic->upt_rq_error_stress_handle);
		if (status != VMK_OK) {
			netdev_err(netdev,
				"Failed to close %s stress handle with "
				"status %s", VMK_STRESS_OPT_NET_IF_FAIL_RX,
				vmk_StatusToString(status));
		}
		enic->upt_rq_error_stress_status = VMK_INVALID_HANDLE;
	}
	
	if (enic->upt_tq_silent_error_stress_status == VMK_OK) {
		status = vmk_StressOptionClose(
			enic->upt_tq_silent_error_stress_handle);
		if (status != VMK_OK) {
			netdev_err(netdev,
			"Failed to close %s stress handle with "
			"status %s", VMK_STRESS_OPT_NET_IF_CORRUPT_TX,
			vmk_StatusToString(status));
		}
		enic->upt_tq_silent_error_stress_status = VMK_INVALID_HANDLE;
	}

	if (enic->upt_tq_error_stress_status == VMK_OK) {
		status = vmk_StressOptionClose(enic->upt_tq_error_stress_handle);
		if (status != VMK_OK) {
			netdev_err(netdev,
				"Failed to close %s stress handle with "
				"status %s", VMK_STRESS_OPT_NET_IF_FAIL_HARD_TX,
				vmk_StatusToString(status));
		}
		enic->upt_tq_error_stress_status = VMK_INVALID_HANDLE;
	}

	del_timer_sync(&enic->upt_notify_timer);
	enic_dev_disable(enic);

	for (i = 0; i < enic->wq_count; i++) {
		err = vnic_wq_disable(&enic->wq[i]);
		if (err)
			return err;
	}

	for (i = 0; i < enic->rq_count; i++) {
		err = vnic_rq_disable(&enic->rq[i]);
		if (err)
			return err;
	}

	spin_lock_bh(&enic->devcmd_lock);
	vnic_dev_notify_unsetcmd(enic->vdev);
	spin_unlock_bh(&enic->devcmd_lock);

	/* Must not touch intr mask since we need to
	 * checkpoint it.
	 */

	enic->upt_active = 0;

	netdev_err(netdev, "quiesced\n");

	return 0;
}

static int enic_upt_save_state(struct net_device *netdev,
	vmk_UPTVFSaveState *vs)
{
	struct enic *enic = netdev_priv(netdev);
	unsigned int i;

	struct vnic_rq *rq;
	struct vnic_wq *wq;
	struct vnic_cq *cq;

	vmk_UPTRXQueueSaveState *rs;
	vmk_UPTTXQueueSaveState *ts;

	struct pass_thru_page __iomem *ptp = vnic_dev_get_res(enic->vdev,
		RES_TYPE_PASS_THRU_PAGE, 0);

	netdev_err(netdev, "%d RQ, %d WQ, %d CQ INTR %d\n", enic->rq_count,
		enic->wq_count, enic->cq_count, enic->intr_count);

	for (i = 0; i < enic->rq_count; i++) {

		rs = &vs->rqState[i];
		rq = &enic->rq[i];
		cq = &enic->cq[i];

		rs->rxProd = ioread32(&rq->ctrl->posted_index);
		rs->rxCons = ioread32(&rq->ctrl->fetch_index);
		rs->rxProd2 = ioread32(&ptp->rxprod2[i].reg);
		rs->rxCons2 = rq->rxcons2;

		netdev_err(netdev, "RQ[%d]: rxCons %u, rxProd %u, rxCons2 %u, "
			"rxProd2 %u\n",
			i, rs->rxCons, rs->rxProd, rs->rxCons2, rs->rxProd2);

		rs->rcProd = ioread32(&cq->ctrl->cq_tail);
		rs->rcGen  = ioread32(&cq->ctrl->cq_tail_color);

		netdev_err(netdev, "CQ[%d]: rcProd %u, rcGen %u\n",
			i, rs->rcProd, rs->rcGen);

		rs->stopped = 0; // XXX for now
	}

	for (i = 0; i < enic->wq_count; i++) {
		
		ts = &vs->tqState[i];
		wq = &enic->wq[i];
		cq = &enic->cq[i + enic->rq_count];
		
		ts->txProd = ioread32(&wq->ctrl->posted_index);
		ts->txCons = ioread32(&wq->ctrl->fetch_index);

		netdev_err(netdev, "WQ[%d]: txCons %u, txProd %u\n",
			i, ts->txCons, ts->txProd);

		ts->tcProd = ioread32(&cq->ctrl->cq_tail);
		ts->tcGen  = ioread32(&cq->ctrl->cq_tail_color);

		netdev_err(netdev, "CQ[%d]: tcProd %u, tcGen %u\n",
			i + enic->rq_count, ts->tcProd, ts->tcGen);

		ts->stopped = 0; // XXX for now
	}

	for (i = 0; i < enic->intr_count; i++) {
		vs->intrState[i].imr = vnic_intr_masked(&enic->intr[i]);
		netdev_err(netdev, "INTR[%d] masked %d\n", i,
			vs->intrState[i].imr);
	}

	return 0;
}

static int enic_upt_update_rss_indtable(struct net_device *netdev,
	vmk_NetVFRSSIndTable *param)
{
	struct enic *enic = netdev_priv(netdev);

	return enic_upt_set_rss_ind_table(enic,
		enic->upt_oob[ENIC_UPT_OOB_RSS],
		enic->upt_oob_pa[ENIC_UPT_OOB_RSS],
		param->indTable, param->indTableSize);
}

void enic_upt_link_down(struct enic *enic)
{
	if (!enic->upt_mode)
		netif_carrier_off(enic->netdev);
}

void enic_upt_prepare_for(struct enic *enic)
{
	
	if (enic->priv_flags & ENIC_RESET_INPROGRESS) {
		if (enic->upt_active || enic->upt_resources_alloced) {
			netdev_err(enic->netdev, 
				"Unexpected reset while "
				"upt_resource_allocated: [%d]"
				"and upt_active is [%d].\n",
				enic->upt_resources_alloced,
				enic->upt_active);
		}
		return; 
	}
		
	if (enic->upt_mode)
		enic_dev_deinit(enic);
}

int enic_upt_recover_from(struct enic *enic)
{
	int err;

	if (enic->priv_flags & ENIC_RESET_INPROGRESS) {
		if (enic->upt_active || enic->upt_resources_alloced) {
			netdev_err(enic->netdev, 
				"Unexpected reset while "
				"upt_resource_allocated: [%d]" 
				"and upt_active is [%d].\n", 
				enic->upt_resources_alloced, 
				enic->upt_active);
		}				 
		return 0; 
	}
	
	/* Switch off UPT notify timer if needed */
	if (enic->upt_active) {
		del_timer_sync(&enic->upt_notify_timer);

		spin_lock_bh(&enic->devcmd_lock);
		vnic_dev_notify_unsetcmd(enic->vdev);
		spin_unlock_bh(&enic->devcmd_lock);

		enic->upt_active = 0;
	}

	/*
	 * Deallocate UPT-specific resources if needed - only will
	 * hit this condition if Emu2PT mode switch fails after
	 * end of VMK_NETPTOP_VF_INIT, or if PT2Emu mode switch fails
	 * before device open.
	 */
	if (enic->upt_resources_alloced) {
		enic_upt_clear_oob(enic);
		enic->upt_resources_alloced = 0;
	}

	if (enic->upt_mode) {
		enic->upt_mode = 0;
	        err = enic_dev_set_ig_vlan_rewrite_mode(enic);
        	if (err) {
                	dev_err(enic_get_dev(enic),
                        	"Failed to set ingress vlan rewrite mode, "
				"aborting.\n");
                	return err;
        	}
		return enic_dev_init(enic);
	}

	return 0;
}

/*
 * Invariants for when certain PT operations are called during UPT mode
 * switches:
 *
 * Emu2PT mode switch:
 *
 * a.
 * Nothing (not even VMK_NETPTOP_VF_ACQUIRE) is guaranteed to be called in
 * all cases.
 *
 * b.
 * If VMK_NETPTOP_VF_ACQUIRE is called successfully, and we fail later in
 * the mode switch, then VMK_NETPTOP_VF_RELEASE will be called before the
 * conclusion of the Emu2PT mode switch.
 *
 * c.
 * If the device is closed successfully, and we fail later in the mode
 * switch, then the device will be opened before the conclusion of the
 * Emu2PT mode switch.
 *
 * d.
 * If VMK_NETPTOP_VF_ACTIVATE is called successfully, and we fail later in the
 * mode switch, then VMK_NETPTOP_VF_QUIESCE will be called before the
 * conclusion of the Emu2PT mode switch.
 *
 * PT2Emu mode switch:
 *
 * e.
 * Device open and VMK_NETPTOP_VF_RELEASE callbacks are the only things that
 * are guaranteed to be called in all cases.
 */
VMK_ReturnStatus enic_upt_ops(void *clientData, vmk_NetPTOP op, void *args)
{
	struct net_device *netdev = clientData;
	vmk_NetPTOPVFGetInfoArgs *infoArgs = args;
	vmk_NetPTOPVFInitArgs *initArgs = args;
	vmk_NetPTOPVFSaveStateArgs *saveArgs = args;
	vmk_NetPTOPVFSetRSSIndTableArgs *tableArgs = args;

	switch (op) {

	/* emulation -> PT mode switch */
	case VMK_NETPTOP_VF_GET_INFO:
		enic_upt_get_info(netdev, &infoArgs->info);
		return VMK_OK;
	/* device is closed */
	case VMK_NETPTOP_VF_INIT:
		return enic_upt_restore_state(netdev, &initArgs->params);
	case VMK_NETPTOP_VF_ACTIVATE:
		return enic_upt_resume(netdev);

	/* PT -> emulation mode switch */
	case VMK_NETPTOP_VF_QUIESCE:
		return enic_upt_quiesce(netdev);
	case VMK_NETPTOP_VF_SAVE_STATE:
		return enic_upt_save_state(netdev, &saveArgs->state);
	/* device is opened */

	/* Miscellaneous operations */
	case VMK_NETPTOP_VF_SET_RSS_IND_TABLE:
		return enic_upt_update_rss_indtable(netdev, &tableArgs->table);
	case VMK_NETPTOP_VF_GET_QUEUE_STATS:
		return enic_upt_get_queue_stats(netdev, args);
	case VMK_NETPTOP_VF_GET_QUEUE_STATUS:
		return enic_upt_get_queue_status(netdev, args);
	default:
		netdev_err(netdev, "unsupported VF op %d\n", op);
		return VMK_FAILURE;
	}
}
