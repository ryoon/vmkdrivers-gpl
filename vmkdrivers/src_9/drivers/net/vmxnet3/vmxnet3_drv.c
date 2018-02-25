/*
 * Copyright(c) 2007-2012 VMware, Inc.  All rights reserved.
 *
 * This file is part of vmxnet3 VMKdriver program.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE, GOOD TITLE or
 * NON INFRINGEMENT. See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "vmxnet3_int.h"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
#error "vmxnet3 driver is not supported on kernels earlier than 2.6"
#endif

#define PCI_VENDOR_ID_VMWARE                    0x15AD
#define PCI_DEVICE_ID_VMWARE_VMXNET3            0x07B0

char vmxnet3_driver_name[] = "vmxnet3";
#define VMXNET3_DRIVER_DESC "VMware vmxnet3 virtual NIC driver"

static const struct pci_device_id vmxnet3_pciid_table[] = {
	{PCI_DEVICE(PCI_VENDOR_ID_VMWARE, PCI_DEVICE_ID_VMWARE_VMXNET3)},
	{0}
};

MODULE_DEVICE_TABLE(pci, vmxnet3_pciid_table);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
static int disable_lro = 1;
#endif

/* drop checker settings */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
/* if we can get udp stats, we can grow the ring larger safetly */
#define VMXNET3_RX_RING_MAX_GROWN_SIZE (VMXNET3_DEF_RX_RING_SIZE*4)
#else
/* otherwise, we should use a smaller cap on the ring growth */
#define VMXNET3_RX_RING_MAX_GROWN_SIZE (VMXNET3_DEF_RX_RING_SIZE*2)
#endif

/* Number of drops per interval which are ignored */
static u32 drop_check_noise;
/* Threshold for growing the ring */
static u32 drop_check_grow_threshold = 60;
/* Interval between packet loss checks (in seconds) */
static u32 drop_check_interval = 1;
/* Threshold for shrinking the ring */
static u32 drop_check_shrink_threshold = 60*60*24;
/* Delay before starting drop checker after ring resize or ifup */
static u32 drop_check_delay = 60;
/*
 * Percentage of Out of Buffer drops vs UDP drops which we must 
 * maintain to grow the ring 
 */
static u8 drop_check_min_oob_percent = 75;


static atomic_t devices_found;
#define VMXNET3_SHM_MAX_DEVICES 10
#ifdef VMXNET3_RSS
static unsigned int num_rss_entries = 0;

static int rss_ind_table[VMXNET3_SHM_MAX_DEVICES * VMXNET3_RSS_IND_TABLE_SIZE + 1] =
{ [0 ... VMXNET3_SHM_MAX_DEVICES * VMXNET3_RSS_IND_TABLE_SIZE] = -1 };
#endif
static int num_tqs[VMXNET3_SHM_MAX_DEVICES + 1] =
{ [0 ... VMXNET3_SHM_MAX_DEVICES] = 0 };
static int num_rqs[VMXNET3_SHM_MAX_DEVICES + 1] =
{ [0 ... VMXNET3_SHM_MAX_DEVICES] = 0 };
static int share_tx_intr[VMXNET3_SHM_MAX_DEVICES + 1] =
{ [0 ... VMXNET3_SHM_MAX_DEVICES] = 0 };
static int buddy_intr[VMXNET3_SHM_MAX_DEVICES + 1] =
{ [0 ... VMXNET3_SHM_MAX_DEVICES] = 1 };

static void
vmxnet3_write_mac_addr(struct vmxnet3_adapter *adapter, u8 *mac);

/*
 *    Enable/Disable the given intr
 */

static inline void
vmxnet3_enable_intr(struct vmxnet3_adapter *adapter, unsigned intr_idx)
{
	VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_IMR + intr_idx * 8, 0);
}


static inline void
vmxnet3_disable_intr(struct vmxnet3_adapter *adapter, unsigned intr_idx)
{
	VMXNET3_WRITE_BAR0_REG(adapter, VMXNET3_REG_IMR + intr_idx * 8, 1);
}


/*
 *    Enable/Disable all intrs used by the device
 */

static void
vmxnet3_enable_all_intrs(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->intr.num_intrs; i++)
		vmxnet3_enable_intr(adapter, i);
        adapter->shared->devRead.intrConf.intrCtrl &= ~VMXNET3_IC_DISABLE_ALL;
}


static void
vmxnet3_disable_all_intrs(struct vmxnet3_adapter *adapter)
{
	int i;

        adapter->shared->devRead.intrConf.intrCtrl |= VMXNET3_IC_DISABLE_ALL;
	for (i = 0; i < adapter->intr.num_intrs; i++)
		vmxnet3_disable_intr(adapter, i);
}


static inline void
vmxnet3_ack_events(struct vmxnet3_adapter *adapter, u32 events)
{
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_ECR, events);
}


static inline Bool
vmxnet3_tq_stopped(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter *adapter)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	return netif_queue_stopped(adapter->netdev);
#else
	return __netif_subqueue_stopped(adapter->netdev, tq->qid);
#endif
}


/*
 *
 * Request the stack to start/stop/wake the tq. This only deals with the OS
 * side, it does NOT handle the device side
 */
static inline void
vmxnet3_tq_start(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter  *adapter)
{
	tq->stopped = FALSE;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	netif_start_queue(adapter->netdev);
#else
	netif_start_subqueue(adapter->netdev, tq->qid);
#endif
}


static inline void
vmxnet3_tq_wake(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter  *adapter)
{
	tq->stopped = FALSE;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	netif_wake_queue(adapter->netdev);
#else
	netif_wake_subqueue(adapter->netdev, tq->qid);
#endif
}


static inline void
vmxnet3_tq_stop(struct vmxnet3_tx_queue *tq, struct vmxnet3_adapter  *adapter)
{
	tq->stopped = TRUE;
	tq->num_stop++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	netif_stop_queue(adapter->netdev);
#else
	netif_stop_subqueue(adapter->netdev, tq->qid);
#endif
}


/*
 * This may start or stop the tx queue.
 */

static void
vmxnet3_check_link(struct vmxnet3_adapter *adapter, Bool affectTxQueue)
{
	unsigned long flags;
	u32 ret;
	int i;

	spin_lock_irqsave(&adapter->cmd_lock, flags);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD, VMXNET3_CMD_GET_LINK);
	ret = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);
	adapter->link_speed = ret >> 16;
	if (ret & 1) { /* Link is up. */
		printk(KERN_INFO "%s: NIC Link is Up %d Mbps\n",
		       adapter->netdev->name, adapter->link_speed);
		if (!netif_carrier_ok(adapter->netdev))
			netif_carrier_on(adapter->netdev);

		if (affectTxQueue) {
			for (i = 0; i < adapter->num_tx_queues; i++)
				vmxnet3_tq_start(&adapter->tx_queue[i],
						 adapter);
		}
	} else {
		printk(KERN_INFO "%s: NIC Link is Down\n",
		       adapter->netdev->name);
		if (netif_carrier_ok(adapter->netdev))
			netif_carrier_off(adapter->netdev);

		if (affectTxQueue) {
			for (i = 0; i < adapter->num_tx_queues; i++)
				vmxnet3_tq_stop(&adapter->tx_queue[i], adapter);
		}
	}
}


/*
 * process events indicated in ECR
 */

static void
vmxnet3_process_events(struct vmxnet3_adapter *adapter)
{
	int i;
	u32 events = le32_to_cpu(adapter->shared->ecr);
	if (!events)
		return;

	vmxnet3_ack_events(adapter, events);

	/* Check if link state has changed */
	if (events & VMXNET3_ECR_LINK)
		vmxnet3_check_link(adapter, TRUE);

	/* Check if there is an error on xmit/recv queues */
	if (events & (VMXNET3_ECR_TQERR | VMXNET3_ECR_RQERR)) {
		spin_lock(&adapter->cmd_lock);
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
				       VMXNET3_CMD_GET_QUEUE_STATUS);
		spin_unlock(&adapter->cmd_lock);

		for (i = 0; i < adapter->num_tx_queues; i++)
			if (adapter->tqd_start[i].status.stopped)
				printk(KERN_INFO "%s: tq[%d] error 0x%x\n",
				       adapter->netdev->name, i,
				       le32_to_cpu(adapter->tqd_start[i].status.error));
		for (i = 0; i < adapter->num_rx_queues; i++)
			if (adapter->rqd_start[i].status.stopped)
				printk(KERN_INFO "%s: rq[%d] error 0x%x\n",
				       adapter->netdev->name, i,
				       adapter->rqd_start[i].status.error);


		schedule_work(&adapter->reset_work);
	}
}

#ifdef __BIG_ENDIAN_BITFIELD
/*
 * The device expects the bitfields in shared structures to be written in
 * little endian. When CPU is big endian, the following routines are used to
 * correctly read and write into ABI.
 * The general technique used here is : double word bitfields are defined in
 * opposite order for big endian architecture. Then before reading them in
 * driver the complete double word is translated using le32_to_cpu. Similarly
 * After the driver writes into bitfields, cpu_to_le32 is used to translate the
 * double words into required format.
 * In order to avoid touching bits in shared structure more than once, temporary
 * descriptors are used. These are passed as srcDesc to following functions.
 */
static void vmxnet3_RxDescToCPU(const struct Vmxnet3_RxDesc *srcDesc,
				struct Vmxnet3_RxDesc *dstDesc)
{
	u32 *src = (u32 *)srcDesc + 2;
	u32 *dst = (u32 *)dstDesc + 2;
	dstDesc->addr = le64_to_cpu(srcDesc->addr);
	*dst = le32_to_cpu(*src);
	dstDesc->ext1 = le32_to_cpu(srcDesc->ext1);
}

static void vmxnet3_TxDescToLe(const struct Vmxnet3_TxDesc *srcDesc,
			       struct Vmxnet3_TxDesc *dstDesc)
{
	int i;
	u32 *src = (u32 *)(srcDesc + 1);
	u32 *dst = (u32 *)(dstDesc + 1);

	/* Working backwards so that the gen bit is set at the end. */
	for (i = 2; i > 0; i--) {
		src--;
		dst--;
		*dst = cpu_to_le32(*src);
	}
}


static void vmxnet3_RxCompToCPU(const struct Vmxnet3_RxCompDesc *srcDesc,
				struct Vmxnet3_RxCompDesc *dstDesc)
{
	int i = 0;
	u32 *src = (u32 *)srcDesc;
	u32 *dst = (u32 *)dstDesc;
	for (i = 0; i < sizeof(struct Vmxnet3_RxCompDesc) / sizeof(u32); i++) {
		*dst = le32_to_cpu(*src);
		src++;
		dst++;
	}
}


/* Used to read bitfield values from double words. */
static u32 get_bitfield32(const __le32 *bitfield, u32 pos, u32 size)
{
	u32 temp = le32_to_cpu(*bitfield);
	u32 mask = ((1 << size) - 1) << pos;
	temp &= mask;
	temp >>= pos;
	return temp;
}



#endif  /* __BIG_ENDIAN_BITFIELD */

#ifdef __BIG_ENDIAN_BITFIELD

#   define VMXNET3_TXDESC_GET_GEN(txdesc) get_bitfield32(((const __le32 *) \
			txdesc) + VMXNET3_TXD_GEN_DWORD_SHIFT, \
			VMXNET3_TXD_GEN_SHIFT, VMXNET3_TXD_GEN_SIZE)
#   define VMXNET3_TXDESC_GET_EOP(txdesc) get_bitfield32(((const __le32 *) \
			txdesc) + VMXNET3_TXD_EOP_DWORD_SHIFT, \
			VMXNET3_TXD_EOP_SHIFT, VMXNET3_TXD_EOP_SIZE)
#   define VMXNET3_TCD_GET_GEN(tcd) get_bitfield32(((const __le32 *)tcd) + \
			VMXNET3_TCD_GEN_DWORD_SHIFT, VMXNET3_TCD_GEN_SHIFT, \
			VMXNET3_TCD_GEN_SIZE)
#   define VMXNET3_TCD_GET_TXIDX(tcd) get_bitfield32((const __le32 *)tcd, \
			VMXNET3_TCD_TXIDX_SHIFT, VMXNET3_TCD_TXIDX_SIZE)
#   define vmxnet3_getRxComp(dstrcd, rcd, tmp) do { \
			(dstrcd) = (tmp); \
			vmxnet3_RxCompToCPU((rcd), (tmp)); \
		} while (0)
#   define vmxnet3_getRxDesc(dstrxd, rxd, tmp) do { \
			(dstrxd) = (tmp); \
			vmxnet3_RxDescToCPU((rxd), (tmp)); \
		} while (0)

#else

#   define VMXNET3_TXDESC_GET_GEN(txdesc) ((txdesc)->gen)
#   define VMXNET3_TXDESC_GET_EOP(txdesc) ((txdesc)->eop)
#   define VMXNET3_TCD_GET_GEN(tcd) ((tcd)->gen)
#   define VMXNET3_TCD_GET_TXIDX(tcd) ((tcd)->txdIdx)
#   define vmxnet3_getRxComp(dstrcd, rcd, tmp) (dstrcd) = (rcd)
#   define vmxnet3_getRxDesc(dstrxd, rxd, tmp) (dstrxd) = (rxd)

#endif /* __BIG_ENDIAN_BITFIELD  */


static void
vmxnet3_unmap_tx_buf(struct vmxnet3_tx_buf_info *tbi,
		     struct pci_dev *pdev)
{
	if (tbi->map_type == VMXNET3_MAP_SINGLE)
		pci_unmap_single(pdev, tbi->dma_addr, tbi->len,
				 PCI_DMA_TODEVICE);
	else if (tbi->map_type == VMXNET3_MAP_PAGE)
		pci_unmap_page(pdev, tbi->dma_addr, tbi->len,
			       PCI_DMA_TODEVICE);
	else
		BUG_ON(tbi->map_type != VMXNET3_MAP_NONE);

	tbi->map_type = VMXNET3_MAP_NONE; /* to help debugging */
}


/*
 *    Returns # of tx descs that this pkt used
 *
 * Side-effects:
 *    1. mappings are freed
 *    2. buf_info[] are updated
 *    3. tx_ring.{avail, next2comp} are updated.
 */

static int
vmxnet3_unmap_pkt(u32 eop_idx, struct vmxnet3_tx_queue *tq,
		  struct pci_dev *pdev,	struct vmxnet3_adapter *adapter)
{
	struct sk_buff *skb;
	int entries = 0;

	/* no out of order completion */
	BUG_ON(tq->buf_info[eop_idx].sop_idx != tq->tx_ring.next2comp);
	BUG_ON(VMXNET3_TXDESC_GET_EOP(&(tq->tx_ring.base[eop_idx].txd)) != 1);

	dev_dbg(&adapter->pdev->dev, "tx complete [%u %u]\n",
		tq->tx_ring.next2comp, eop_idx);
	skb = tq->buf_info[eop_idx].skb;
	BUG_ON(skb == NULL);
	tq->buf_info[eop_idx].skb = NULL;

	VMXNET3_INC_RING_IDX_ONLY(eop_idx, tq->tx_ring.size);

	while (tq->tx_ring.next2comp != eop_idx) {
		vmxnet3_unmap_tx_buf(tq->buf_info + tq->tx_ring.next2comp,
				     pdev);

		/* update next2comp w/o tx_lock. Since we are marking more,
		 * instead of less, tx ring entries avail, the worst case is
		 * that the tx routine incorrectly re-queues a pkt due to
		 * insufficient tx ring entries.
		 */
		vmxnet3_cmd_ring_adv_next2comp(&tq->tx_ring);
		entries++;
	}

	dev_kfree_skb_any(skb);
	return entries;
}


static int
vmxnet3_tq_tx_complete(struct vmxnet3_tx_queue *tq,
		       struct vmxnet3_adapter *adapter)
{
	int completed = 0;
	union Vmxnet3_GenericDesc *gdesc;

	gdesc = tq->comp_ring.base + tq->comp_ring.next2proc;
	while (VMXNET3_TCD_GET_GEN(&gdesc->tcd) == tq->comp_ring.gen) {
		completed += vmxnet3_unmap_pkt(VMXNET3_TCD_GET_TXIDX(
					       &gdesc->tcd), tq, adapter->pdev,
					       adapter);

		vmxnet3_comp_ring_adv_next2proc(&tq->comp_ring);
		gdesc = tq->comp_ring.base + tq->comp_ring.next2proc;
	}

	if (completed) {
		spin_lock(&tq->tx_lock);
		if (unlikely(vmxnet3_tq_stopped(tq, adapter) &&
			     vmxnet3_cmd_ring_desc_avail(&tq->tx_ring) >
			     VMXNET3_WAKE_QUEUE_THRESHOLD(tq) &&
			     netif_carrier_ok(adapter->netdev))) {
			vmxnet3_tq_wake(tq, adapter);
		}
		spin_unlock(&tq->tx_lock);
	}
	return completed;
}


static void
vmxnet3_tq_cleanup(struct vmxnet3_tx_queue *tq,
		   struct vmxnet3_adapter *adapter)
{
	while (tq->tx_ring.next2comp != tq->tx_ring.next2fill) {
		struct vmxnet3_tx_buf_info *tbi;
		union Vmxnet3_GenericDesc *gdesc;

		tbi = tq->buf_info + tq->tx_ring.next2comp;
		gdesc = tq->tx_ring.base + tq->tx_ring.next2comp;

		vmxnet3_unmap_tx_buf(tbi, adapter->pdev);
		if (tbi->skb) {
			dev_kfree_skb_any(tbi->skb);
			tbi->skb = NULL;
		}
		vmxnet3_cmd_ring_adv_next2comp(&tq->tx_ring);
	}

	/* sanity check */
#ifdef VMX86_DEBUG
	{
		/* verify all buffers are indeed unmapped and freed */
		int i;
		for (i = 0; i < tq->tx_ring.size; i++) {
			BUG_ON(tq->buf_info[i].skb != NULL ||
			       tq->buf_info[i].map_type != VMXNET3_MAP_NONE);
		}
	}
#endif

	tq->tx_ring.gen = VMXNET3_INIT_GEN;
	tq->tx_ring.next2fill = tq->tx_ring.next2comp = 0;

	tq->comp_ring.gen = VMXNET3_INIT_GEN;
	tq->comp_ring.next2proc = 0;
}

static void
vmxnet3_tq_cleanup_all(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		vmxnet3_tq_cleanup(&adapter->tx_queue[i], adapter);
	}
}

/*
 *	free rings and buf_info for the tx queue. There must be no pending pkt
 *	in the tx ring. the .base fields of all rings and buf_info will be
 *	set to NULL
 */

static void
vmxnet3_tq_destroy(struct vmxnet3_tx_queue *tq,
		   struct vmxnet3_adapter *adapter)
{
	if (tq->tx_ring.base) {
		pci_free_consistent(adapter->pdev, tq->tx_ring.size *
				    sizeof(struct Vmxnet3_TxDesc),
				    tq->tx_ring.base, tq->tx_ring.basePA);
		tq->tx_ring.base = NULL;
	}
	if (tq->data_ring.base) {
		pci_free_consistent(adapter->pdev, tq->data_ring.size *
				    sizeof(struct Vmxnet3_TxDataDesc),
				    tq->data_ring.base, tq->data_ring.basePA);
		tq->data_ring.base = NULL;
	}
	if (tq->comp_ring.base) {
		pci_free_consistent(adapter->pdev, tq->comp_ring.size *
				    sizeof(struct Vmxnet3_TxCompDesc),
				    tq->comp_ring.base, tq->comp_ring.basePA);
		tq->comp_ring.base = NULL;
	}
	kfree(tq->buf_info);
	tq->buf_info = NULL;
}

/* Destroy all tx queues */
void
vmxnet3_tq_destroy_all(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		vmxnet3_tq_destroy(&adapter->tx_queue[i], adapter);
	}
}



/*
 *    reset all internal states and rings for a tx queue
 * Side-effects:
 *    1. contents of the rings are reset to 0
 *    2. indices and gen of rings are reset
 *    3. bookkeeping data is reset
 */
static void
vmxnet3_tq_init(struct vmxnet3_tx_queue *tq,
		struct vmxnet3_adapter *adapter)
{
	int i;

	/* reset the tx ring contents to 0 and reset the tx ring states */
	memset(tq->tx_ring.base, 0,
	       tq->tx_ring.size * sizeof(struct Vmxnet3_TxDesc));
	tq->tx_ring.next2fill = tq->tx_ring.next2comp = 0;
	tq->tx_ring.gen = VMXNET3_INIT_GEN;

	memset(tq->data_ring.base, 0,
	       tq->data_ring.size * sizeof(struct Vmxnet3_TxDataDesc));

	/* reset the tx comp ring contents to 0 and reset comp ring states */
	memset(tq->comp_ring.base, 0,
	       tq->comp_ring.size * sizeof(struct Vmxnet3_TxCompDesc));
	tq->comp_ring.next2proc = 0;
	tq->comp_ring.gen = VMXNET3_INIT_GEN;

	/* reset the bookkeeping data */
	memset(tq->buf_info, 0, sizeof(tq->buf_info[0]) * tq->tx_ring.size);
	for (i = 0; i < tq->tx_ring.size; i++)
		tq->buf_info[i].map_type = VMXNET3_MAP_NONE;

	/* stats are not reset */
}


/* Init all tx queues */
static void
vmxnet3_tq_init_all(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_tx_queues; i++) {
		vmxnet3_tq_init(&adapter->tx_queue[i], adapter);
	}
}


/*
 * allocate and initialize rings for the tx queue, also allocate and
 * initialize buf_info. Returns 0 on success, negative errno on failure.
 */
static int
vmxnet3_tq_create(struct vmxnet3_tx_queue *tq,
		  struct vmxnet3_adapter *adapter)
{
	BUG_ON(tq->tx_ring.size <= 0 ||
	       tq->data_ring.size != tq->tx_ring.size);
	BUG_ON((tq->tx_ring.size & VMXNET3_RING_SIZE_MASK) != 0);
	BUG_ON(tq->tx_ring.base || tq->data_ring.base || tq->comp_ring.base ||
	       tq->buf_info);

	tq->tx_ring.base = pci_alloc_consistent(adapter->pdev,
			   tq->tx_ring.size * sizeof(struct Vmxnet3_TxDesc),
			   &tq->tx_ring.basePA);
	if (!tq->tx_ring.base) {
		printk(KERN_ERR "%s: failed to allocate tx ring\n",
		       adapter->netdev->name);
		goto err;
	}

	tq->data_ring.base = pci_alloc_consistent(adapter->pdev,
			     tq->data_ring.size *
			     sizeof(struct Vmxnet3_TxDataDesc),
			     &tq->data_ring.basePA);
	if (!tq->data_ring.base) {
		printk(KERN_ERR "%s: failed to allocate data ring\n",
		       adapter->netdev->name);
		goto err;
	}

	tq->comp_ring.base = pci_alloc_consistent(adapter->pdev,
			     tq->comp_ring.size *
			     sizeof(struct Vmxnet3_TxCompDesc),
			     &tq->comp_ring.basePA);
	if (!tq->comp_ring.base) {
		printk(KERN_ERR "%s: failed to allocate tx comp ring\n",
		       adapter->netdev->name);
		goto err;
	}

	tq->buf_info = kcalloc(tq->tx_ring.size, sizeof(tq->buf_info[0]),
			       GFP_KERNEL);
	if (!tq->buf_info) {
		printk(KERN_ERR "%s: failed to allocate tx bufinfo\n",
		       adapter->netdev->name);
		goto err;
	}

	return 0;

err:
	vmxnet3_tq_destroy(tq, adapter);
	return -ENOMEM;
}


/*
 *    starting from ring->next2fill, allocate rx buffers for the given ring
 *    of the rx queue and update the rx desc. stop after @num_to_alloc buffers
 *    are allocated or allocation fails. Returns # of buffers allocated
 *
 * Side-effects:
 *    1. rx descs are updated
 *    2. ring->{gen, next2fill} are updated
 *    3. uncommitted[ring_idx] is incremented
 */

static int
vmxnet3_rq_alloc_rx_buf(struct vmxnet3_rx_queue *rq, u32 ring_idx,
			int num_to_alloc, struct vmxnet3_adapter *adapter)
{
	int num_allocated = 0;
	struct vmxnet3_rx_buf_info *rbi_base = rq->buf_info[ring_idx];
	struct vmxnet3_cmd_ring *ring = &rq->rx_ring[ring_idx];
	u32 val;

	while (num_allocated < num_to_alloc) {
		struct vmxnet3_rx_buf_info *rbi;
		union Vmxnet3_GenericDesc *gd;

		rbi = rbi_base + ring->next2fill;
		gd = ring->base + ring->next2fill;

		if (rbi->buf_type == VMXNET3_RX_BUF_SKB) {
			if (rbi->skb == NULL) {
				rbi->skb = dev_alloc_skb(
					 	rbi->len + NET_IP_ALIGN);
				if (unlikely(rbi->skb == NULL)) {
					rq->stats.rx_buf_alloc_failure++;
                                        /* starvation prevention */
                                        if (vmxnet3_cmd_ring_desc_empty(rq->rx_ring + ring_idx))
                                           rbi->skb = rq->spare_skb;
                                        else
                                           break;
				} else
					skb_reserve(rbi->skb, NET_IP_ALIGN);
				rbi->skb->dev = adapter->netdev;
				rbi->dma_addr = pci_map_single(adapter->pdev, 
							     rbi->skb->data,
							     rbi->len, 
							     PCI_DMA_FROMDEVICE);
			} else {
				/* rx buffer skipped by the device */
			}
			val = VMXNET3_RXD_BTYPE_HEAD << VMXNET3_RXD_BTYPE_SHIFT;
		} else {
			BUG_ON(rbi->buf_type != VMXNET3_RX_BUF_PAGE ||
			       rbi->len  != PAGE_SIZE);

			if (rbi->page == NULL) {
				rbi->page = alloc_page(GFP_ATOMIC);
				if (unlikely(rbi->page == NULL)) {
					rq->stats.rx_buf_alloc_failure++;
					break;
				}
				rbi->dma_addr = pci_map_page(adapter->pdev,
                                                             rbi->page, 0,
                                                             PAGE_SIZE,
                                                             PCI_DMA_FROMDEVICE);
			} else {
				/* rx buffers skipped by the device */
			}
			val = VMXNET3_RXD_BTYPE_BODY << VMXNET3_RXD_BTYPE_SHIFT;
		}

		BUG_ON(rbi->dma_addr == 0);
		gd->rxd.addr = cpu_to_le64(rbi->dma_addr);
		gd->dword[2] = cpu_to_le32((ring->gen << VMXNET3_RXD_GEN_SHIFT)
					   | val | rbi->len);

		num_allocated++;
		vmxnet3_cmd_ring_adv_next2fill(ring);
	}
	rq->uncommitted[ring_idx] += num_allocated;

	dev_dbg(&adapter->pdev->dev, "alloc_rx_buf: %d allocated, next2fill "
		"%u, next2comp %u, uncommited %u\n", num_allocated,
		ring->next2fill, ring->next2comp, rq->uncommitted[ring_idx]);

	/* so that the device can distinguish a full ring and an empty ring */
	BUG_ON(num_allocated != 0 && ring->next2fill == ring->next2comp);

	return num_allocated;
}


/*
 * It assumes the skb still has space to accommodate the frag. It only
 * increments skb->data_len
 */

static void
vmxnet3_append_frag(struct sk_buff *skb, struct Vmxnet3_RxCompDesc *rcd,
		    struct vmxnet3_rx_buf_info *rbi)
{
	struct skb_frag_struct *frag = skb_shinfo(skb)->frags +
		skb_shinfo(skb)->nr_frags;

	BUG_ON(skb_shinfo(skb)->nr_frags >= MAX_SKB_FRAGS);

	frag->page = rbi->page;
	frag->page_offset = 0;
	frag->size = rcd->len;
	skb->data_len += frag->size;
	skb_shinfo(skb)->nr_frags++;
}

/*
 *
 * Free any pages which were attached to the frags of the
 * spare skb.  this can happen when the spare skb is attached
 * to the rx ring to prevent starvation, but there was no
 * issue with page allocation.
 *
 */

static void
vmxnet3_rx_spare_skb_free_frags(struct vmxnet3_adapter *adapter,
				struct sk_buff *skb)
{
        int i;
        for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
                struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];
                BUG_ON(frag->page == 0);
                put_page(frag->page);
                frag->page = 0;
                frag->size = 0;
        }
        skb_shinfo(skb)->nr_frags = 0;
        skb->data_len = 0;
}


/*
 * Map the tx buffer and set up ONLY TXD.{addr, len, gen} based on the mapping.
 * It sets the other fields of the descriptors to 0.
 * Side Effects :
 *    1. the corresponding buf_info entries are upated,
 *    2. ring indices are advanced
 */

static void
vmxnet3_map_pkt(struct sk_buff *skb, struct vmxnet3_tx_ctx *ctx,
		struct vmxnet3_tx_queue *tq, struct pci_dev *pdev,
		struct vmxnet3_adapter *adapter)
{
	u32 dw2, len;
	unsigned long buf_offset;
	int i;
	union Vmxnet3_GenericDesc *gdesc;
	struct vmxnet3_tx_buf_info *tbi = NULL;

	BUG_ON(ctx->copy_size > skb_headlen(skb));

	/* use the previous gen bit for the SOP desc */
	dw2 = (tq->tx_ring.gen ^ 0x1) << VMXNET3_TXD_GEN_SHIFT;

	ctx->sop_txd = tq->tx_ring.base + tq->tx_ring.next2fill;
	gdesc = ctx->sop_txd; /* both loops below can be skipped */

	/* no need to map the buffer if headers are copied */
	if (ctx->copy_size) {
		BUG_ON(VMXNET3_TXDESC_GET_GEN(&(ctx->sop_txd->txd)) ==
		       tq->tx_ring.gen);

		ctx->sop_txd->txd.addr = cpu_to_le64(tq->data_ring.basePA +
					tq->tx_ring.next2fill *
					sizeof(struct Vmxnet3_TxDataDesc));
		ctx->sop_txd->dword[2] = cpu_to_le32(dw2 | ctx->copy_size);
		ctx->sop_txd->dword[3] = 0;

		tbi = tq->buf_info + tq->tx_ring.next2fill;
		tbi->map_type = VMXNET3_MAP_NONE;

		dev_dbg(&adapter->pdev->dev, "txd[%u]: 0x%llu 0x%x 0x%x\n",
			tq->tx_ring.next2fill,
			le64_to_cpu(ctx->sop_txd->txd.addr),
			ctx->sop_txd->dword[2], ctx->sop_txd->dword[3]);
		vmxnet3_cmd_ring_adv_next2fill(&tq->tx_ring);

		/* use the right gen for non-SOP desc */
		dw2 = tq->tx_ring.gen << VMXNET3_TXD_GEN_SHIFT;
	}

	/* linear part can use multiple tx desc if it's big */
	len = skb_headlen(skb) - ctx->copy_size;
	buf_offset = ctx->copy_size;
	while (len) {
		u32 buf_size;

		if (len < VMXNET3_MAX_TX_BUF_SIZE) {
			buf_size = len;
			dw2 |= len;
		} else {
			buf_size = VMXNET3_MAX_TX_BUF_SIZE;
			/* spec says that for TxDesc.len, 0 == 2^14 */
		}

		tbi = tq->buf_info + tq->tx_ring.next2fill;
		tbi->map_type = VMXNET3_MAP_SINGLE;
		tbi->dma_addr = pci_map_single(adapter->pdev, 
					       skb->data + buf_offset,
					       buf_size, PCI_DMA_TODEVICE);

		tbi->len = buf_size;

		gdesc = tq->tx_ring.base + tq->tx_ring.next2fill;
		BUG_ON(gdesc->txd.gen == tq->tx_ring.gen);

		gdesc->txd.addr = cpu_to_le64(tbi->dma_addr);
		gdesc->dword[2] = cpu_to_le32(dw2);
		gdesc->dword[3] = 0;

		dev_dbg(&adapter->pdev->dev,
			"txd[%u]: 0x%llu 0x%x 0x%x\n",
			tq->tx_ring.next2fill, le64_to_cpu(gdesc->txd.addr),
			le32_to_cpu(gdesc->dword[2]), gdesc->dword[3]);
		vmxnet3_cmd_ring_adv_next2fill(&tq->tx_ring);
		dw2 = tq->tx_ring.gen << VMXNET3_TXD_GEN_SHIFT;

		len -= buf_size;
		buf_offset += buf_size;
	}

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];
		u32 buf_size;
		buf_offset = frag->page_offset;
		len = frag->size;

		while (len) {
			tbi = tq->buf_info + tq->tx_ring.next2fill;
			if (len < VMXNET3_MAX_TX_BUF_SIZE) {
				buf_size = len;
				dw2 |= len;
			} else {
				buf_size = VMXNET3_MAX_TX_BUF_SIZE;
				/* spec says that for TxDesc.len, 0 == 2^14 */
			}
			tbi->map_type = VMXNET3_MAP_PAGE;
			tbi->dma_addr = pci_map_page(adapter->pdev, frag->page,
						     buf_offset, buf_size,
						     PCI_DMA_TODEVICE);

			tbi->len = buf_size;
			gdesc = tq->tx_ring.base + tq->tx_ring.next2fill;
			BUG_ON(gdesc->txd.gen == tq->tx_ring.gen);

			gdesc->txd.addr = cpu_to_le64(tbi->dma_addr);
			gdesc->dword[2] = cpu_to_le32(dw2);
			gdesc->dword[3] = 0;

			vmxnet3_cmd_ring_adv_next2fill(&tq->tx_ring);
			dw2 = tq->tx_ring.gen << VMXNET3_TXD_GEN_SHIFT;
			dev_dbg(&adapter->pdev->dev,
				"txd[%u]: 0x%llu %u %u\n",
				tq->tx_ring.next2fill, le64_to_cpu(gdesc->txd.addr),
				le32_to_cpu(gdesc->dword[2]), gdesc->dword[3]);
			len -= buf_size;
			buf_offset += buf_size;
		}
	}

	ctx->eop_txd = gdesc;

	/* set the last buf_info for the pkt */
	tbi->skb = skb;
	tbi->sop_idx = ctx->sop_txd - tq->tx_ring.base;
}


/*
 *    parse and copy relevant protocol headers:
 *     For a tso pkt, relevant headers are L2/3/4 including options
 *     For a pkt requesting csum offloading, they are L2/3 and may include L4
 *     if it's a TCP/UDP pkt
 *
 *    The implementation works only when h/w vlan insertion is used, see PR
 *    171928
 *
 * Result:
 *    -1:  error happens during parsing
 *     0:  protocol headers parsed, but too big to be copied
 *     1:  protocol headers parsed and copied
 *
 * Side-effects:
 *    1. related *ctx fields are updated.
 *    2. ctx->copy_size is # of bytes copied
 *    3. the portion copied is guaranteed to be in the linear part
 *
 */
static int
vmxnet3_parse_and_copy_hdr(struct sk_buff *skb, struct vmxnet3_tx_queue *tq,
			   struct vmxnet3_tx_ctx *ctx,
			   struct vmxnet3_adapter *adapter)
{
	struct Vmxnet3_TxDataDesc *tdd;

	if (ctx->mss) {
		ctx->eth_ip_hdr_size = skb_transport_offset(skb);
		ctx->l4_hdr_size = skb_tcp_header(skb)->doff * 4;
		ctx->copy_size = ctx->eth_ip_hdr_size + ctx->l4_hdr_size;
	} else {
		if (skb->ip_summed == CHECKSUM_HW) {
			ctx->eth_ip_hdr_size = skb_transport_offset(skb);

			if (ctx->ipv4) {
				struct iphdr *iph = (struct iphdr *)
						    skb_ip_header(skb);
				if (iph->protocol == IPPROTO_TCP)
					ctx->l4_hdr_size =
					   skb_tcp_header(skb)->doff * 4;
				else if (iph->protocol == IPPROTO_UDP)
					/*
					 * Use TCP header size so that bytes to
					 * copied are more than the minimum
					 * required by the backend.
					 */
					ctx->l4_hdr_size =
							sizeof(struct tcphdr);
				else
					ctx->l4_hdr_size = 0;
			} else {
				/* for simplicity, don't copy L4 headers */
				ctx->l4_hdr_size = 0;
			}
			ctx->copy_size = ctx->eth_ip_hdr_size +
                                         ctx->l4_hdr_size;

		} else {
			ctx->eth_ip_hdr_size = 0;
			ctx->l4_hdr_size = 0;
			/* copy as much as allowed */
			ctx->copy_size = min((unsigned int)
                                             VMXNET3_HDR_COPY_SIZE,
					     skb_headlen(skb));
		}

		/* make sure headers are accessible directly */
		if (unlikely(!pskb_may_pull(skb, ctx->copy_size)))
			goto err;
	}

	if (unlikely(ctx->copy_size > VMXNET3_HDR_COPY_SIZE)) {
		tq->stats.oversized_hdr++;
		ctx->copy_size = 0;
		return 0;
	}

	tdd = tq->data_ring.base + tq->tx_ring.next2fill;
	BUG_ON(ctx->copy_size > skb_headlen(skb));

	memcpy(tdd->data, skb->data, ctx->copy_size);

	dev_dbg(&adapter->pdev->dev, "copy %u bytes to dataRing[%u]\n",
		ctx->copy_size, tq->tx_ring.next2fill);
	return 1;

err:
	return -1;
}


/*
 *    Fix pkt headers for tso. ip hdr and tcp hdr are changed
 */

static void
vmxnet3_prepare_tso(struct sk_buff *skb, struct vmxnet3_tx_ctx *ctx)
{
	struct tcphdr *tcph = skb_tcp_header(skb);
	if (ctx->ipv4) {
		struct iphdr *iph = skb_ip_header(skb);
		iph->check = 0;
		tcph->check = ~csum_tcpudp_magic(iph->saddr, iph->daddr, 0,
						 IPPROTO_TCP, 0);
#ifdef NETIF_F_TSO6
	} else {
		struct ipv6hdr *iph = (struct ipv6hdr *)
				      skb_network_header(skb);
		tcph->check = ~csum_ipv6_magic(&iph->saddr, &iph->daddr, 0,
					       IPPROTO_TCP, 0);
#endif
	}
}

inline void vmxnet3_le32_add_cpu(uint32 *addTo, uint32 addThis)
{
	*addTo = cpu_to_le32(le32_to_cpu(*addTo) + addThis);
}


static int txd_estimate(const struct sk_buff *skb)
{
	int count = VMXNET3_TXD_NEEDED(skb_headlen(skb)) + 1;
	int i;

	for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
		const struct skb_frag_struct *frag = &skb_shinfo(skb)->frags[i];
		count += VMXNET3_TXD_NEEDED(frag->size);
	}
	return count;
}



/*
 *    transmit a pkt thru a given tq
 *
 * Result:
 *    NETDEV_TX_OK:      descriptors are setup successfully
 *    NETDEV_TX_OK:      error occured, the pkt is dropped
 *    NETDEV_TX_BUSY:    tx ring is full, queue is stopped
 *
 * Side-effects:
 *    1. tx ring may be changed
 *    2. tq stats may be updated accordingly
 *    3. shared->txNumDeferred may be updated
 */

int
vmxnet3_tq_xmit(struct sk_buff *skb,
		struct vmxnet3_tx_queue *tq,
		struct vmxnet3_adapter *adapter,
		struct net_device *netdev)
{
	int ret;
	u32 count;
	unsigned long flags;
	struct vmxnet3_tx_ctx ctx;
	union Vmxnet3_GenericDesc *gdesc;
#ifdef __BIG_ENDIAN_BITFIELD
	/* Use temporary descriptor to avoid touching bits multiple times */
	union Vmxnet3_GenericDesc tempTxDesc;
#endif

	/* conservatively estimate # of descriptors to use */
	count = txd_estimate(skb);

	ctx.ipv4 = (skb->protocol == __constant_ntohs(ETH_P_IP));

	ctx.mss = skb_mss(skb);
	if (ctx.mss) {
		if (skb_header_cloned(skb)) {
			if (unlikely(pskb_expand_head(skb, 0, 0,
						      GFP_ATOMIC) != 0)) {
				tq->stats.drop_tso++;
				goto drop_pkt;
			}
			tq->stats.copy_skb_header++;
		}
		vmxnet3_prepare_tso(skb, &ctx);
	} else {
		if (unlikely(count > VMXNET3_MAX_TXD_PER_PKT)) {
			/* non-tso pkts must not use more than
			 * VMXNET3_MAX_TXD_PER_PKT entries
			 */
			if (skb_linearize(skb) != 0) {
				tq->stats.drop_too_many_frags++;
				goto drop_pkt;
			}
			tq->stats.linearized++;

			/* recalculate the # of descriptors to use */
			count = VMXNET3_TXD_NEEDED(skb_headlen(skb)) + 1;
		}
	}

	spin_lock_irqsave(&tq->tx_lock, flags);

	if (count > vmxnet3_cmd_ring_desc_avail(&tq->tx_ring)) {
		tq->stats.tx_ring_full++;
		dev_dbg(&adapter->pdev->dev, "tx queue stopped on %s, next2comp"
			" %u next2fill %u\n", adapter->netdev->name,
			tq->tx_ring.next2comp, tq->tx_ring.next2fill);

		vmxnet3_tq_stop(tq, adapter);
		spin_unlock_irqrestore(&tq->tx_lock, flags);
		return NETDEV_TX_BUSY;
	}

	ret = vmxnet3_parse_and_copy_hdr(skb, tq, &ctx, adapter);
	if (ret >= 0) {
		BUG_ON(ret <= 0 && ctx.copy_size != 0);
		/* hdrs parsed, check against other limits */
		if (ctx.mss) {
			if (unlikely(ctx.eth_ip_hdr_size + ctx.l4_hdr_size >
				     VMXNET3_MAX_TX_BUF_SIZE)) {
				goto hdr_too_big;
			}
		} else {
			if (skb->ip_summed == CHECKSUM_HW) {
				if (unlikely(ctx.eth_ip_hdr_size +
					     skb_csum_offset(skb) >
					     VMXNET3_MAX_CSUM_OFFSET)) {
					goto hdr_too_big;
				}
			}
		}
	} else {
		tq->stats.drop_hdr_inspect_err++;
		goto unlock_drop_pkt;
	}

	/* fill tx descs related to addr & len */
	vmxnet3_map_pkt(skb, &ctx, tq, adapter->pdev, adapter);

	/* setup the EOP desc */
	ctx.eop_txd->dword[3] = cpu_to_le32(VMXNET3_TXD_CQ | VMXNET3_TXD_EOP);

	/* setup the SOP desc */
#ifdef __BIG_ENDIAN_BITFIELD
	gdesc = &tempTxDesc;
	gdesc->dword[2] = ctx.sop_txd->dword[2];
	gdesc->dword[3] = ctx.sop_txd->dword[3];
#else
	gdesc = ctx.sop_txd;
#endif
	if (ctx.mss) {
		gdesc->txd.hlen = ctx.eth_ip_hdr_size + ctx.l4_hdr_size;
		gdesc->txd.om = VMXNET3_OM_TSO;
		gdesc->txd.msscof = ctx.mss;
		vmxnet3_le32_add_cpu(&tq->shared->txNumDeferred, (skb->len -
			     gdesc->txd.hlen + ctx.mss - 1) / ctx.mss);
	} else {
		if (skb->ip_summed == CHECKSUM_HW) {
			gdesc->txd.hlen = ctx.eth_ip_hdr_size;
			gdesc->txd.om = VMXNET3_OM_CSUM;
			gdesc->txd.msscof = ctx.eth_ip_hdr_size +
					    skb_csum_offset(skb);
		} else {
			gdesc->txd.om = 0;
			gdesc->txd.msscof = 0;
		}
		vmxnet3_le32_add_cpu(&tq->shared->txNumDeferred, 1);
	}

	if (vlan_tx_tag_present(skb)) {
		gdesc->txd.ti = 1;
		gdesc->txd.tci = vlan_tx_tag_get(skb);
	}

	/* finally flips the GEN bit of the SOP desc. */
	gdesc->dword[2] = cpu_to_le32(le32_to_cpu(gdesc->dword[2]) ^
						  VMXNET3_TXD_GEN);
#ifdef __BIG_ENDIAN_BITFIELD
	/* Finished updating in bitfields of Tx Desc, so write them in original
	 * place.
	 */
	vmxnet3_TxDescToLe((struct Vmxnet3_TxDesc *)gdesc,
			   (struct Vmxnet3_TxDesc *)ctx.sop_txd);
	gdesc = ctx.sop_txd;
#endif
	dev_dbg(&adapter->pdev->dev,
		"txd[%u]: SOP 0x%llu 0x%x 0x%x\n",
		(u32)(ctx.sop_txd -
		tq->tx_ring.base), le64_to_cpu(gdesc->txd.addr),
		le32_to_cpu(gdesc->dword[2]), le32_to_cpu(gdesc->dword[3]));

	spin_unlock_irqrestore(&tq->tx_lock, flags);

	if (le32_to_cpu(tq->shared->txNumDeferred) >=
					le32_to_cpu(tq->shared->txThreshold)) {
		tq->shared->txNumDeferred = 0;
		VMXNET3_WRITE_BAR0_REG(adapter, (VMXNET3_REG_TXPROD +
				       tq->qid * 8), tq->tx_ring.next2fill);
	}
	netdev->trans_start = jiffies;

	return NETDEV_TX_OK;

hdr_too_big:
	tq->stats.drop_oversized_hdr++;
unlock_drop_pkt:
	spin_unlock_irqrestore(&tq->tx_lock, flags);
drop_pkt:
	tq->stats.drop_total++;
	dev_kfree_skb(skb);
	return NETDEV_TX_OK;
}


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
static int
#else
static netdev_tx_t
#endif
vmxnet3_xmit_frame(struct sk_buff *skb, struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25) || defined(CONFIG_NETDEVICES_MULTIQUEUE)
	BUG_ON(skb->queue_mapping > adapter->num_tx_queues);
	return vmxnet3_tq_xmit(skb,
			       &adapter->tx_queue[skb->queue_mapping],
			       adapter, netdev);
#else
	return vmxnet3_tq_xmit(skb, &adapter->tx_queue[0], adapter,
			       netdev);
#endif
}


/* called to process csum related bits in the EOP RCD descriptor */
static void
vmxnet3_rx_csum(struct vmxnet3_adapter *adapter,
		struct sk_buff *skb,
		union Vmxnet3_GenericDesc *gdesc)
{
	if (!gdesc->rcd.cnc && adapter->rxcsum) {
		/* typical case: TCP/UDP over IP and both csums are correct */
		if ((le32_to_cpu(gdesc->dword[3]) & VMXNET3_RCD_CSUM_OK) ==
							VMXNET3_RCD_CSUM_OK) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			BUG_ON(!(gdesc->rcd.tcp || gdesc->rcd.udp));
			BUG_ON(!(gdesc->rcd.v4  || gdesc->rcd.v6));
			BUG_ON(gdesc->rcd.frg);
		} else {
			if (gdesc->rcd.csum) {
				skb->csum = htons(gdesc->rcd.csum);
				skb->ip_summed = CHECKSUM_HW;
			} else {
				skb->ip_summed = CHECKSUM_NONE;
			}
		}
	} else {
		skb->ip_summed = CHECKSUM_NONE;
	}
}


/*
 * called when ERR bit is set for a received pkt. The desc and the associated
 * rx buffer have not been processed yet.
 *
 * Side-effects:
 *    1. up the stat counters
 *    2. free the skb if needed
 *    3. reset ctx->skb
 */

static void
vmxnet3_rx_error(struct vmxnet3_rx_queue *rq, struct Vmxnet3_RxCompDesc *rcd,
		 struct vmxnet3_rx_ctx *ctx,  struct vmxnet3_adapter *adapter)
{
	rq->stats.drop_err++;
	if (!rcd->fcs)
		rq->stats.drop_fcs++;

	rq->stats.drop_total++;

	/*
	 * We do not unmap and chain the rx buffer to the skb.
	 * We basically pretend this buffer is not used and will be recycled
	 * by vmxnet3_rq_alloc_rx_buf()
	 */

	/*
	 * ctx->skb may be NULL if this is the first and the only one
	 * desc for the pkt
	 */
	if (ctx->skb) {
                if (ctx->skb == rq->spare_skb)
                        vmxnet3_rx_spare_skb_free_frags(adapter, ctx->skb);
                else
		        dev_kfree_skb_irq(ctx->skb);
        }

	ctx->skb = NULL;
}

/* process the rx completion ring of the given rx queue. Quota specifies the
 * max # of rx completion entries to be processed. Returns # of descs completed.
 */
static int
#ifdef VMXNET3_NAPI
vmxnet3_rq_rx_complete(struct vmxnet3_rx_queue *rq,
		       struct vmxnet3_adapter *adapter, int quota)
#else
vmxnet3_rq_rx_complete(struct vmxnet3_rx_queue *rq,
		       struct vmxnet3_adapter *adapter)
#endif
{
	static u32 rxprod_reg[2] = {VMXNET3_REG_RXPROD, VMXNET3_REG_RXPROD2};
	u32 num_rxd = 0;
	struct Vmxnet3_RxCompDesc *rcd;
	struct vmxnet3_rx_ctx *ctx = &rq->rx_ctx;
#ifdef __BIG_ENDIAN_BITFIELD
	struct Vmxnet3_RxDesc rxCmdDesc;
	struct Vmxnet3_RxCompDesc rxComp;
#endif
	vmxnet3_getRxComp(rcd, &rq->comp_ring.base[rq->comp_ring.next2proc].rcd,
			  &rxComp);
	while (rcd->gen == rq->comp_ring.gen) {
		struct vmxnet3_rx_buf_info *rbi;
		struct sk_buff *skb;
		int num_to_alloc;
		struct Vmxnet3_RxDesc *rxd;
		u32 idx, ring_idx;
#ifdef VMXNET3_NAPI
		if (num_rxd >= quota) {
			/* we may stop even before we see the EOP desc of
			 * the current pkt
			 */
			break;
		}
		num_rxd++;
#endif
		BUG_ON(rcd->rqID != rq->qid && rcd->rqID != rq->qid2);
		idx = rcd->rxdIdx;
		ring_idx = rcd->rqID < adapter->num_rx_queues ? 0 : 1;
		vmxnet3_getRxDesc(rxd, &rq->rx_ring[ring_idx].base[idx].rxd,
				  &rxCmdDesc);
		rbi = rq->buf_info[ring_idx] + idx;

		BUG_ON(rcd->len > rxd->len);
		BUG_ON(rxd->addr != rbi->dma_addr ||
		       rxd->len != rbi->len);

		if (unlikely(rcd->eop && rcd->err)) {
			vmxnet3_rx_error(rq, rcd, ctx, adapter);
			goto rcd_done;
		}

		if (rcd->sop) { /* first buf of the pkt */
			BUG_ON(rxd->btype != VMXNET3_RXD_BTYPE_HEAD ||
			       rcd->rqID != rq->qid);

			BUG_ON(rbi->buf_type != VMXNET3_RX_BUF_SKB);
			BUG_ON(ctx->skb != NULL || rbi->skb == NULL);

			if (unlikely(rcd->len == 0)) {
				/* Pretend the rx buffer is skipped. */
				BUG_ON(!(rcd->sop && rcd->eop));
				dev_dbg(&adapter->pdev->dev, "rxRing[%u][%u] 0"
					" length\n", ring_idx, idx);
				goto rcd_done;
			}

			ctx->skb = rbi->skb;
			rbi->skb = NULL;

			pci_unmap_single(adapter->pdev, rbi->dma_addr, rbi->len,
					 PCI_DMA_FROMDEVICE);

			if (rq->spare_skb != ctx->skb)
				skb_put(ctx->skb, rcd->len);
		} else {
			BUG_ON(ctx->skb == NULL);
			/* non SOP buffer must be type 1 in most cases */
			if (rbi->buf_type == VMXNET3_RX_BUF_PAGE) {
				BUG_ON(rxd->btype != VMXNET3_RXD_BTYPE_BODY);

				if (rcd->len) {
					pci_unmap_page(adapter->pdev,
						       rbi->dma_addr, rbi->len,
						       PCI_DMA_FROMDEVICE);

					vmxnet3_append_frag(ctx->skb, rcd, rbi);
					rbi->page = NULL;
				}
			} else {
				/*
				 * The only time a non-SOP buffer is type 0 is
				 * when it's EOP and error flag is raised, which
				 * has already been handled.
				 */
				BUG_ON(TRUE);
			}
		}

		skb = ctx->skb;
		if (rcd->eop) {
			if (skb == rq->spare_skb) {
                                rq->stats.drop_total++;
                                vmxnet3_rx_spare_skb_free_frags(adapter, skb);
			} else {
				skb->len += skb->data_len;
				skb->truesize += skb->data_len;

				vmxnet3_rx_csum(adapter, skb,
					(union Vmxnet3_GenericDesc *)rcd);
				skb->protocol = eth_type_trans(skb,
							       adapter->netdev);

#ifdef VMXNET3_NAPI
				if (unlikely(adapter->vlan_grp && rcd->ts)) {
					vlan_hwaccel_receive_skb(skb,
						adapter->vlan_grp, rcd->tci);
				} else {
					netif_receive_skb(skb);
				}
#else
				if (unlikely(adapter->vlan_grp && rcd->ts)) {
					vlan_hwaccel_rx(skb, adapter->vlan_grp,
							rcd->tci);
				} else {
					netif_rx(skb);
				}
#endif
			}

			adapter->netdev->last_rx = jiffies;
			ctx->skb = NULL;
		}

rcd_done:
		/* device may skip some rx descs */
		rq->rx_ring[ring_idx].next2comp = idx;
		VMXNET3_INC_RING_IDX_ONLY(rq->rx_ring[ring_idx].next2comp,
					  rq->rx_ring[ring_idx].size);

		/* refill rx buffers frequently to avoid starving the h/w */
		num_to_alloc = vmxnet3_cmd_ring_desc_avail(rq->rx_ring +
							   ring_idx);
		if (unlikely(num_to_alloc > VMXNET3_RX_ALLOC_THRESHOLD(rq,
							ring_idx, adapter))) {
			vmxnet3_rq_alloc_rx_buf(rq, ring_idx, num_to_alloc,
						adapter);

			/* if needed, update the register */
			if (unlikely(rq->shared->updateRxProd)) {
				VMXNET3_WRITE_BAR0_REG(adapter,
					rxprod_reg[ring_idx] + rq->qid * 8,
					rq->rx_ring[ring_idx].next2fill);
				rq->uncommitted[ring_idx] = 0;
			}
		}

		vmxnet3_comp_ring_adv_next2proc(&rq->comp_ring);
		vmxnet3_getRxComp(rcd,
		     &rq->comp_ring.base[rq->comp_ring.next2proc].rcd, &rxComp);
	}

	return num_rxd;
}


/*
 * Unmap and free the rx buffers allocated to the rx queue. Other resources
 * are NOT freed. This is the counterpart of vmxnet3_rq_init()
 * Side-effects:
 *    1. indices and gen of each ring are reset to the initial value
 *    2. buf_info[] and buf_info2[] are cleared.
 */

static void
vmxnet3_rq_cleanup(struct vmxnet3_rx_queue *rq,
		   struct vmxnet3_adapter *adapter)
{
	u32 i, ring_idx;
	struct Vmxnet3_RxDesc *rxd;

	for (ring_idx = 0; ring_idx < 2; ring_idx++) {
		for (i = 0; i < rq->rx_ring[ring_idx].size; i++) {
#ifdef __BIG_ENDIAN_BITFIELD
			struct Vmxnet3_RxDesc rxDesc;
#endif
			vmxnet3_getRxDesc(rxd,
				&rq->rx_ring[ring_idx].base[i].rxd, &rxDesc);

			if (rxd->btype == VMXNET3_RXD_BTYPE_HEAD &&
					rq->buf_info[ring_idx][i].skb) {
				pci_unmap_single(adapter->pdev, rxd->addr,
						 rxd->len, PCI_DMA_FROMDEVICE);
				if (rq->spare_skb !=
						rq->buf_info[ring_idx][i].skb)
					dev_kfree_skb(
						rq->buf_info[ring_idx][i].skb);
				rq->buf_info[ring_idx][i].skb = NULL;
			} else if (rxd->btype == VMXNET3_RXD_BTYPE_BODY &&
					rq->buf_info[ring_idx][i].page) {
				pci_unmap_page(adapter->pdev, rxd->addr,
					       rxd->len, PCI_DMA_FROMDEVICE);
				put_page(rq->buf_info[ring_idx][i].page);
				rq->buf_info[ring_idx][i].page = NULL;
			}
		}

		rq->rx_ring[ring_idx].gen = VMXNET3_INIT_GEN;
		rq->rx_ring[ring_idx].next2fill =
					rq->rx_ring[ring_idx].next2comp = 0;
		rq->uncommitted[ring_idx] = 0;
	}

        /* free starvation prevention skb if allocated */
        if (rq->spare_skb) {
                vmxnet3_rx_spare_skb_free_frags(adapter, rq->spare_skb);
                kfree_skb(rq->spare_skb);
                rq->spare_skb = NULL;
        }

	rq->comp_ring.gen = VMXNET3_INIT_GEN;
	rq->comp_ring.next2proc = 0;
}

static void
vmxnet3_rq_cleanup_all(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		vmxnet3_rq_cleanup(&adapter->rx_queue[i], adapter);
	}
}


/*
 *    Free rings and buf_info for the rx queue. The rx buffers must have
 *    ALREADY been freed. the .base fields of all rings will be set to NULL
 */

static void
vmxnet3_rq_destroy(struct vmxnet3_rx_queue *rq,
		struct vmxnet3_adapter *adapter)
{
	int i;

#ifdef VMX86_DEBUG
	/* all rx buffers must have already been freed */
	{
		int j;

		for (i = 0; i < 2; i++) {
			if (rq->buf_info[i]) {
				for (j = 0; j < rq->rx_ring[i].size; j++) {
					BUG_ON(rq->buf_info[i][j].page != NULL);
				}
			}
		}
	}
#endif


	kfree(rq->buf_info[0]);

	for (i = 0; i < 2; i++) {
		if (rq->rx_ring[i].base) {
			pci_free_consistent(adapter->pdev, rq->rx_ring[i].size
					    * sizeof(struct Vmxnet3_RxDesc),
					    rq->rx_ring[i].base,
					    rq->rx_ring[i].basePA);
			rq->rx_ring[i].base = NULL;
		}
		rq->buf_info[i] = NULL;
	}

	if (rq->comp_ring.base) {
		pci_free_consistent(adapter->pdev, rq->comp_ring.size *
				    sizeof(struct Vmxnet3_RxCompDesc),
				    rq->comp_ring.base, rq->comp_ring.basePA);
		rq->comp_ring.base = NULL;
	}
}

void
vmxnet3_rq_destroy_all(struct vmxnet3_adapter *adapter)
{
	int i;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		vmxnet3_rq_destroy(&adapter->rx_queue[i], adapter);
	}
}


/*
 *    initialize buf_info, allocate rx buffers and fill the rx rings. On
 *    failure, the rx buffers already allocated are NOT freed
 */

static int
vmxnet3_rq_init(struct vmxnet3_rx_queue *rq,
		struct vmxnet3_adapter  *adapter)
{
	int i;

	BUG_ON(adapter->rx_buf_per_pkt <= 0 ||
	       rq->rx_ring[0].size % adapter->rx_buf_per_pkt != 0);
	/* initialize buf_info */
	for (i = 0; i < rq->rx_ring[0].size; i++) {
		BUG_ON(rq->buf_info[0][i].skb != NULL);
		/* 1st buf for a pkt is skbuff */
		if (i % adapter->rx_buf_per_pkt == 0) {
			rq->buf_info[0][i].buf_type = VMXNET3_RX_BUF_SKB;
			rq->buf_info[0][i].len = adapter->skb_buf_size;
		} else { /* subsequent bufs for a pkt is frag */
			rq->buf_info[0][i].buf_type = VMXNET3_RX_BUF_PAGE;
			rq->buf_info[0][i].len = PAGE_SIZE;
		}
	}
	for (i = 0; i < rq->rx_ring[1].size; i++) {
		BUG_ON(rq->buf_info[1][i].page != NULL);
		rq->buf_info[1][i].buf_type = VMXNET3_RX_BUF_PAGE;
		rq->buf_info[1][i].len = PAGE_SIZE;
	}

	/* reset internal state and allocate buffers for both rings */
	for (i = 0; i < 2; i++) {
		rq->rx_ring[i].next2fill = rq->rx_ring[i].next2comp = 0;
		rq->uncommitted[i] = 0;

		memset(rq->rx_ring[i].base, 0, rq->rx_ring[i].size *
		       sizeof(struct Vmxnet3_RxDesc));
		rq->rx_ring[i].gen = VMXNET3_INIT_GEN;
	}

	/* allocate ring starvation protection */
	rq->spare_skb = dev_alloc_skb(PAGE_SIZE);
	if (rq->spare_skb == NULL) {
		return -ENOMEM;
	}

        /* populate initial ring */
	if (vmxnet3_rq_alloc_rx_buf(rq, 0, rq->rx_ring[0].size - 1,
				    adapter) == 0) {
		/* at least has 1 rx buffer for the 1st ring */

		kfree_skb(rq->spare_skb);
		rq->spare_skb = NULL;
		return -ENOMEM;
	}
	vmxnet3_rq_alloc_rx_buf(rq, 1, rq->rx_ring[1].size - 1, adapter);

	/* reset the comp ring */
	rq->comp_ring.next2proc = 0;
	memset(rq->comp_ring.base, 0, rq->comp_ring.size *
	       sizeof(struct Vmxnet3_RxCompDesc));
	rq->comp_ring.gen = VMXNET3_INIT_GEN;

	/* reset rxctx */
	rq->rx_ctx.skb = NULL;

	/* stats are not reset */
	return 0;
}


static int
vmxnet3_rq_init_all(struct vmxnet3_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		err = vmxnet3_rq_init(&adapter->rx_queue[i], adapter);
		if (unlikely(err)) {
			printk(KERN_ERR "%s: failed to initialize rx queue%i\n",
			       adapter->netdev->name, i);
			break;
		}
	}
	return err;

}

static u64
vmxnet3_get_oob_drop_count(struct vmxnet3_adapter *adapter)
{
	unsigned long flags;

	spin_lock_irqsave(&adapter->cmd_lock, flags);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD, VMXNET3_CMD_GET_STATS);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);

	return adapter->rqd_start->stats.pktsRxOutOfBuf;
}


static u64
vmxnet3_get_udp_drop_count(struct vmxnet3_adapter *adapter)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
	struct net *net = dev_net(adapter->netdev);

	return snmp_fold_field((void __percpu **)net->mib.udp_statistics,
			       UDP_MIB_RCVBUFERRORS) +
		snmp_fold_field((void __percpu **)net->mib.udplite_statistics,
				UDP_MIB_RCVBUFERRORS);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
	struct net *net = dev_net(adapter->netdev);
	return snmp_fold_field((void **)net->mib.udp_statistics,
			       UDP_MIB_RCVBUFERRORS) +
		snmp_fold_field((void **)net->mib.udplite_statistics,
				UDP_MIB_RCVBUFERRORS);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	return snmp_fold_field((void **)udp_statistics,
                               UDP_MIB_RCVBUFERRORS);
#else
        /*
         * XXX udp_statistics isn't exported :(
         * I think we will need to parse the proc node
         */
        return 0;
#endif
}


static void
vmxnet3_drop_checker(unsigned long arg)
{
	struct net_device *netdev = (struct net_device *)arg;
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	u64 oob_drops, oob_drops_since_last_check;
	u64 udp_drops, udp_drops_since_last_check;
	u8 oob_percent;
	u64 total_drops;
	u32 current_ring_size;
	Bool growing_ring = FALSE;

	if (adapter->use_adaptive_ring == FALSE)
		return;

	current_ring_size = adapter->rx_queue->rx_ring[0].size;

	/* get drop statistics */
	oob_drops = vmxnet3_get_oob_drop_count(adapter);
	oob_drops_since_last_check = oob_drops - adapter->prev_oob_drop_count;

	udp_drops = vmxnet3_get_udp_drop_count(adapter);
	udp_drops_since_last_check = udp_drops - adapter->prev_udp_drop_count;

	total_drops = udp_drops_since_last_check + oob_drops_since_last_check;
	if (total_drops > 0) {
		oob_percent = oob_drops_since_last_check * 100;
		do_div(oob_percent, total_drops);
	} else
		oob_percent = 100;

	/* store the current stats to compare next round */
	adapter->prev_oob_drop_count = oob_drops;
	adapter->prev_udp_drop_count = udp_drops;

        if(adapter->drop_check_delay > 0) {
           adapter->drop_check_delay--;
           goto reschedule;
        }

	if (total_drops > drop_check_noise &&
	    oob_percent > drop_check_min_oob_percent) {
		/* keep track of consecutive intervals of drops */
		adapter->drop_counter++;
		adapter->no_drop_counter = 0;
	}
	else if (total_drops <= drop_check_noise) {
		/* keep track of consecutive intervals of no drops */
		adapter->no_drop_counter++;
	}
	if (oob_drops_since_last_check > current_ring_size &&
	    oob_percent >= drop_check_min_oob_percent) {
		/* if we saw a burst of drops, grow the ring */
		growing_ring = TRUE;
		dev_dbg(&adapter->pdev->dev,
			"%s: detected %llu oob packet drops (%u%% of drops).\n",
			adapter->netdev->name, oob_drops_since_last_check,
			oob_percent);
	} else if (adapter->drop_counter > drop_check_grow_threshold) {
		/* after many intervals with some loss, grow the ring */
		growing_ring = TRUE;
		dev_dbg(&adapter->pdev->dev,
			"%s: reached packet loss threshold, %d ticks\n",
			adapter->netdev->name, adapter->drop_counter);
	}

	if (growing_ring &&
            current_ring_size < VMXNET3_RX_RING_MAX_GROWN_SIZE) {
		adapter->new_rx_ring_size =
                   min((u32)VMXNET3_RX_RING_MAX_GROWN_SIZE,
                       current_ring_size*2);
		printk(KERN_INFO "%s: going to grow rx ring.\n",
		       adapter->netdev->name);
		schedule_work(&adapter->resize_ring_work);
                return;
	} else if (adapter->no_drop_counter >= drop_check_shrink_threshold) {
		/*
		 * if we had a lot of intervals with no loss, shrink the
		 * ring again
		 */
		adapter->new_rx_ring_size = current_ring_size / 2;
		if (adapter->new_rx_ring_size < VMXNET3_DEF_RX_RING_SIZE)
			goto reschedule;
		printk(KERN_INFO "%s: going to shrink rx ring.\n",
		       adapter->netdev->name);
		schedule_work(&adapter->resize_ring_work);
                return;
        }
 reschedule:
        /* schedule the next drop check */
        mod_timer(&adapter->drop_check_timer,
                  jiffies + msecs_to_jiffies(drop_check_interval*1000));
}

static void
vmxnet3_start_drop_checker(struct vmxnet3_adapter *adapter)
{
	if (adapter->use_adaptive_ring == FALSE)
		return;

	adapter->prev_oob_drop_count = vmxnet3_get_oob_drop_count(adapter);
	adapter->prev_udp_drop_count = vmxnet3_get_udp_drop_count(adapter);
	adapter->drop_counter = 0;
	adapter->no_drop_counter = 0;
        adapter->drop_check_delay = drop_check_delay;
	add_timer(&adapter->drop_check_timer);
}

static void
vmxnet3_stop_drop_checker(struct vmxnet3_adapter *adapter)
{
	if (adapter->use_adaptive_ring)
		del_timer_sync(&adapter->drop_check_timer);
}


/*
 *    allocate and initialize two cmd rings and the completion ring for the
 *    given rx queue. Also allocate and initialize buf_info.
 *    rx buffers are NOT allocated
 */
static int
vmxnet3_rq_create(struct vmxnet3_rx_queue *rq, struct vmxnet3_adapter *adapter)
{
	int i;
	size_t sz;
	struct vmxnet3_rx_buf_info *bi;

	BUG_ON(rq->rx_ring[0].size % adapter->rx_buf_per_pkt != 0);
	for (i = 0; i < 2; i++) {
		BUG_ON((rq->rx_ring[i].size & VMXNET3_RING_SIZE_MASK) != 0);
		BUG_ON(rq->rx_ring[i].base != NULL);
		sz = rq->rx_ring[i].size * sizeof(struct Vmxnet3_RxDesc);
		rq->rx_ring[i].base = pci_alloc_consistent(adapter->pdev, sz,
							&rq->rx_ring[i].basePA);
		if (!rq->rx_ring[i].base) {
			printk(KERN_ERR "%s: failed to allocate rx ring %d\n",
			       adapter->netdev->name, i);
			goto err;
		}
	}

	sz = rq->comp_ring.size * sizeof(Vmxnet3_RxCompDesc);
	BUG_ON(rq->comp_ring.base != NULL);
	rq->comp_ring.base = pci_alloc_consistent(adapter->pdev,
			sz,
			&rq->comp_ring.basePA);
	if (!rq->comp_ring.base) {
		printk(KERN_ERR "%s: failed to allocate rx comp ring\n",
		       adapter->netdev->name);
		goto err;
	}

	BUG_ON(rq->buf_info[0] || rq->buf_info[1]);
	sz = sizeof(struct vmxnet3_rx_buf_info) * (rq->rx_ring[0].size +
						   rq->rx_ring[1].size);
	bi = kmalloc(sz, GFP_KERNEL);
	if (!bi) {
		printk(KERN_ERR "%s: failed to allocate rx bufinfo\n",
		       adapter->netdev->name);
		goto err;
	}
	memset(bi, 0, sz);
	rq->buf_info[0] = bi;
	rq->buf_info[1] = bi + rq->rx_ring[0].size;

	return 0;

err:
	vmxnet3_rq_destroy(rq, adapter);
	return -ENOMEM;
}

static int
vmxnet3_rq_create_all(struct vmxnet3_adapter *adapter)
{
	int i, err = 0;

	for (i = 0; i < adapter->num_rx_queues; i++) {
		err = vmxnet3_rq_create(&adapter->rx_queue[i], adapter);
		if (unlikely(err)) {
			printk(KERN_ERR "%s: failed to create rx queue%i\n",
			       adapter->netdev->name, i);
			goto err_out;
		}
	}
	return err;
err_out:
	vmxnet3_rq_destroy_all(adapter);
	return err;

}

/* Multiple queue aware polling function for tx and rx */
static int
vmxnet3_do_poll(struct vmxnet3_adapter *adapter, int budget)
{
	int rcd_done = 0, i;
	if (unlikely(adapter->shared->ecr))
		vmxnet3_process_events(adapter);

	for (i = 0; i < adapter->num_tx_queues; i++)
		vmxnet3_tq_tx_complete(&adapter->tx_queue[i], adapter);

	for (i = 0; i < adapter->num_rx_queues; i++)
		rcd_done += vmxnet3_rq_rx_complete(&adapter->rx_queue[i],
						   adapter, budget);
	return rcd_done;
}


#ifdef VMXNET3_NAPI
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 24) || defined(__VMKLNX__)

/*
 * New NAPI polling function. Returns # of the NAPI credit consumed (# of rx
 * descriptors processed)
 */
static int
vmxnet3_poll(struct napi_struct *napi, int budget)
{
	struct vmxnet3_rx_queue *rx_queue = container_of(napi,
					  struct vmxnet3_rx_queue, napi);
	int rxd_done;

	rxd_done = vmxnet3_do_poll(rx_queue->adapter, budget);

	if (rxd_done < budget) {
		netif_rx_complete(rx_queue->adapter->netdev, napi);
		vmxnet3_enable_all_intrs(rx_queue->adapter);
	}
	return rxd_done;
}

/*
 * new NAPI polling function for MSI-X mode with multiple Rx queues
 * Returns the # of the NAPI credit consumed (# of rx descriptors processed)
 */

static int
vmxnet3_poll_rx_only(struct napi_struct *napi, int budget)
{
	struct vmxnet3_rx_queue *rq = container_of(napi,
						struct vmxnet3_rx_queue, napi);
	struct vmxnet3_adapter *adapter = rq->adapter;
	int rxd_done;

	/* When sharing interrupt with corresponding tx queue, process
	 * tx completions in that queue as well
	 */
	if (adapter->share_intr == vmxnet3_intr_buddyshare) {
		struct vmxnet3_tx_queue *tq =
				&adapter->tx_queue[rq - adapter->rx_queue];
		vmxnet3_tq_tx_complete(tq, adapter);
	} else if (adapter->intr.num_intrs == VMXNET3_LINUX_MIN_MSIX_VECT) {
		struct vmxnet3_tx_queue *tq;
		int i;
		for (i = 0; i < adapter->num_tx_queues; i++) {
			tq = &adapter->tx_queue[i];
			vmxnet3_tq_tx_complete(tq, adapter);
		}
	}

	rxd_done = vmxnet3_rq_rx_complete(rq, adapter, budget);

	if (rxd_done < budget) {
		netif_rx_complete(adapter->netdev, napi);
		vmxnet3_enable_intr(adapter, rq->comp_ring.intr_idx);
	}
	return rxd_done;
}


#else	/* new NAPI */

/*
 * Result:
 *    0: napi is done
 *    1: continue polling
 */

static int
vmxnet3_poll(struct net_device *poll_dev, int *budget)
{
	int rxd_done, quota;
	struct vmxnet3_adapter *adapter = netdev_priv(poll_dev);

	quota = min(*budget, poll_dev->quota);

	rxd_done = vmxnet3_do_poll(adapter, quota);

	*budget -= rxd_done;
	poll_dev->quota -= rxd_done;

	if (rxd_done < quota) {
                netif_rx_complete(poll_dev, &adapter->rx_queue[0].napi);
		vmxnet3_enable_all_intrs(adapter);
		return 0;
	}

	return 1; /* not done */
}


/*
 * Poll all rx queues for completions. Does not support napi per rx queue,
 * use one shared napi instead.
 * Returns 0 when napi is done  and 1 when it should continue polling
 */

static int
vmxnet3_poll_rx_only(struct net_device *poll_dev, int *budget)
{
	int rxd_done = 0, quota;
	struct vmxnet3_adapter *adapter = netdev_priv(poll_dev);
	quota = min(*budget, poll_dev->quota);

	if (adapter->share_intr == vmxnet3_intr_buddyshare)
		vmxnet3_tq_tx_complete(&adapter->tx_queue[0], adapter);

	rxd_done = vmxnet3_rq_rx_complete(&adapter->rx_queue[0],
					  adapter, *budget);
	*budget -= rxd_done;
	poll_dev->quota -= rxd_done;

	if (rxd_done < quota) {
		netif_rx_complete(poll_dev, &adapter->rx_queue[0].napi);
			/* enable all rx interrupts*/
		vmxnet3_enable_intr(adapter,
				    adapter->rx_queue[0].comp_ring.intr_idx);
		return 0;
	}

	return 1; /* not done */
}


#endif
#endif /* VMXNET3_NAPI  */

#ifdef CONFIG_PCI_MSI

/*
 * Handle completion interrupts on tx queues
 * Returns whether or not the intr is handled
 */

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
vmxnet3_msix_tx(int irq, void *data, struct pt_regs * regs)
#else
vmxnet3_msix_tx(int irq, void *data)
#endif
{
	struct vmxnet3_tx_queue *tq = data;
	struct vmxnet3_adapter *adapter = tq->adapter;

	if (adapter->intr.mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(adapter, tq->comp_ring.intr_idx);

	/* Handle the case where only one irq is allocate for all tx queues */
	if (adapter->share_intr == vmxnet3_intr_txshare) {
		int i;
		for (i = 0; i < adapter->num_tx_queues; i++) {
			tq = &adapter->tx_queue[i];
			vmxnet3_tq_tx_complete(tq, adapter);
		}
	} else {
		vmxnet3_tq_tx_complete(tq, adapter);
	}
	vmxnet3_enable_intr(adapter, tq->comp_ring.intr_idx);

	return IRQ_HANDLED;
}



/*
 * Handle completion interrupts on rx queues. Returns whether or not the
 * intr is handled
 */

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
vmxnet3_msix_rx(int irq, void *data, struct pt_regs * regs)
#else
vmxnet3_msix_rx(int irq, void *data)
#endif
{
	struct vmxnet3_rx_queue *rq = data;
	struct vmxnet3_adapter *adapter = rq->adapter;

	/* disable intr if needed */
	if (adapter->intr.mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(adapter, rq->comp_ring.intr_idx);
#ifdef VMXNET3_NAPI
	netif_rx_schedule(adapter->netdev, &rq->napi);
#else
	vmxnet3_rq_rx_complete(rq, adapter);
	if (adapter->share_intr == vmxnet3_intr_buddyshare) {
		vmxnet3_tq_tx_complete(&adapter->tx_queue[rq -
				       adapter->rx_queue], adapter);
	} else if (adapter->intr.num_intrs == VMXNET3_LINUX_MIN_MSIX_VECT) {
		struct vmxnet3_tx_queue *tq;
		int i;
		for (i = 0; i < adapter->num_tx_queues; i++) {
			tq = &adapter->tx_queue[i];
			vmxnet3_tq_tx_complete(tq, adapter);
		}
	}
	vmxnet3_enable_intr(adapter, rq->comp_ring.intr_idx);
#endif

	return IRQ_HANDLED;
}

/*
 *----------------------------------------------------------------------------
 *
 * vmxnet3_msix_event --
 *
 *    vmxnet3 msix event intr handler
 *
 * Result:
 *    whether or not the intr is handled
 *
 *----------------------------------------------------------------------------
 */

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
vmxnet3_msix_event(int irq, void *data, struct pt_regs * regs)
#else
vmxnet3_msix_event(int irq, void *data)
#endif
{
	struct net_device *dev = data;
	struct vmxnet3_adapter *adapter = netdev_priv(dev);

	/* disable intr if needed */
	if (adapter->intr.mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(adapter, adapter->intr.event_intr_idx);

	if (adapter->shared->ecr)
		vmxnet3_process_events(adapter);

	vmxnet3_enable_intr(adapter, adapter->intr.event_intr_idx);

	return IRQ_HANDLED;
}


#endif /* CONFIG_PCI_MSI  */

static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
vmxnet3_intr(int irq, void *dev_id, struct pt_regs * regs)
#else
vmxnet3_intr(int irq, void *dev_id)
#endif
{
	struct net_device *dev = dev_id;
	struct vmxnet3_adapter *adapter = netdev_priv(dev);

	if (adapter->intr.type == VMXNET3_IT_INTX) {
		u32 icr = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_ICR);
		if (unlikely(icr == 0))
			/* not ours */
			return IRQ_NONE;
	}

	/* disable intr if needed */
	if (adapter->intr.mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_all_intrs(adapter);
#ifdef VMXNET3_NAPI
	netif_rx_schedule(dev, &adapter->rx_queue[0].napi);
#else
	vmxnet3_do_poll(adapter, adapter->rx_queue[0].rx_ring[0].size);
	vmxnet3_enable_intr(adapter, 0);
#endif

	return IRQ_HANDLED;
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void
vmxnet3_netpoll(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	switch (adapter->intr.type) {
#   ifdef CONFIG_PCI_MSI
	case VMXNET3_IT_MSIX: {
		int i;
		for (i = 0; i < adapter->num_rx_queues; i++)
			vmxnet3_msix_rx(0, &adapter->rx_queue[i], NULL);
		break;
	}
#   endif
	case VMXNET3_IT_MSI:
	default:
		vmxnet3_intr(0, adapter->netdev, NULL);
		break;
	}
}

#endif		/* CONFIG_NET_POLL_CONTROLLER */


/*
 * event_intr_idx and intr_idx for different comp rings get updated here.
 */

static int
vmxnet3_request_irqs(struct vmxnet3_adapter *adapter)
{
	struct vmxnet3_intr *intr = &adapter->intr;
	int err = 0, i;
	int vector = 0;

#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_MSIX) {
		for (i = 0; i < adapter->num_tx_queues; i++) {
			sprintf(adapter->tx_queue[i].name, "%s:v%d-%s",
				adapter->netdev->name, vector, "Tx");
			if (adapter->share_intr != vmxnet3_intr_buddyshare)
				err = request_irq(intr->msix_entries[vector].vector,
						  vmxnet3_msix_tx, 0,
						  adapter->tx_queue[i].name,
						  &(adapter->tx_queue[i]));
			if (err) {
				printk(KERN_ERR "Failed to request irq for "
				       "MSIX, %s, error %d\n",
				       adapter->tx_queue[i].name, err);
				return err;
			}

			/* Handle the case where only 1 MSIx was allocated for
			 * all tx queues */
			if (adapter->share_intr == vmxnet3_intr_txshare) {
				for (; i < adapter->num_tx_queues; i++)
					adapter->tx_queue[i].comp_ring.intr_idx
								= vector;
				vector++;
				break;
			} else {
				adapter->tx_queue[i].comp_ring.intr_idx
								= vector++;
			}
		}
		if (adapter->share_intr == vmxnet3_intr_buddyshare ||
		    intr->num_intrs == VMXNET3_LINUX_MIN_MSIX_VECT)
			vector = 0;

		for (i = 0; i < adapter->num_rx_queues; i++) {
			sprintf(adapter->rx_queue[i].name, "%s:v%d-%s",
				adapter->netdev->name, vector, "Rx");
			err = request_irq(intr->msix_entries[vector].vector,
					  vmxnet3_msix_rx, 0,
					  adapter->rx_queue[i].name,
					  &(adapter->rx_queue[i]));
			if (err) {
				printk(KERN_ERR "Failed to request irq for MSIX"
				       ", %s, error %d\n",
				       adapter->rx_queue[i].name, err);
				return err;
			}

			adapter->rx_queue[i].comp_ring.intr_idx = vector++;
		}

		sprintf(intr->event_msi_vector_name, "%s:v%d-event",
			adapter->netdev->name, vector);
		err = request_irq(intr->msix_entries[vector].vector,
				  vmxnet3_msix_event, 0,
				  intr->event_msi_vector_name, adapter->netdev);
		intr->event_intr_idx = vector;

	} else if (intr->type == VMXNET3_IT_MSI) {
		adapter->num_rx_queues = 1;
		err = request_irq(adapter->pdev->irq, vmxnet3_intr, 0,
				  adapter->netdev->name, adapter->netdev);
	} else {
#endif
		adapter->num_rx_queues = 1;
		err = request_irq(adapter->pdev->irq, vmxnet3_intr,
				  IRQF_SHARED, adapter->netdev->name,
				  adapter->netdev);
#ifdef CONFIG_PCI_MSI
	}
#endif
	intr->num_intrs = vector + 1;
	if (err) {
		printk(KERN_ERR "Failed to request irq %s (intr type:%d), error"
		       ":%d\n", adapter->netdev->name, intr->type, err);
	} else {
		/* Number of rx queues will not change after this */
		for (i = 0; i < adapter->num_rx_queues; i++) {
			struct vmxnet3_rx_queue *rq = &adapter->rx_queue[i];
			rq->qid = i;
			rq->qid2 = i + adapter->num_rx_queues;
		}

		/* init our intr settings */
		for (i = 0; i < intr->num_intrs; i++)
			intr->mod_levels[i] = UPT1_IML_ADAPTIVE;

		if (adapter->intr.type != VMXNET3_IT_MSIX) {
			adapter->intr.event_intr_idx = 0;
			for(i = 0; i < adapter->num_tx_queues; i++) {
				adapter->tx_queue[i].comp_ring.intr_idx = 0;
			}
			adapter->rx_queue[0].comp_ring.intr_idx = 0;
		}

		printk(KERN_INFO "%s: intr type %u, mode %u, %u vectors "
		       "allocated\n", adapter->netdev->name, intr->type,
		       intr->mask_mode, intr->num_intrs);
	}

	return err;
}


static void
vmxnet3_free_irqs(struct vmxnet3_adapter *adapter)
{
	struct vmxnet3_intr *intr = &adapter->intr;
	BUG_ON(intr->type == VMXNET3_IT_AUTO ||
	       intr->num_intrs <= 0);

	switch (intr->type) {
#ifdef CONFIG_PCI_MSI
	case VMXNET3_IT_MSIX:
	{
		int i, vector = 0;

		if (adapter->share_intr != vmxnet3_intr_buddyshare &&
		    intr->num_intrs != VMXNET3_LINUX_MIN_MSIX_VECT) {
			for (i = 0; i < adapter->num_tx_queues; i++) {
				free_irq(intr->msix_entries[vector++].vector,
					 &(adapter->tx_queue[i]));
				if (adapter->share_intr == vmxnet3_intr_txshare)
					break;
			}
		}

		for (i = 0; i < adapter->num_rx_queues; i++) {
			free_irq(intr->msix_entries[vector++].vector,
				 &(adapter->rx_queue[i]));
		}

		free_irq(intr->msix_entries[vector].vector,
			 adapter->netdev);
		BUG_ON(vector >= intr->num_intrs);
		break;
	}
	case VMXNET3_IT_MSI:
		free_irq(adapter->pdev->irq, adapter->netdev);
		break;
#endif
	case VMXNET3_IT_INTX:
		free_irq(adapter->pdev->irq, adapter->netdev);
		break;
	default:
		BUG_ON(TRUE);
	}
}


static void
vmxnet3_vlan_rx_register(struct net_device *netdev, struct vlan_group *grp)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	struct Vmxnet3_DriverShared *shared = adapter->shared;
	u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;
	unsigned long flags;

	if (grp) {
		/*
                 * VLAN striping feature already enabled by default, no need to
                 * enable it here.
                 */
		if (adapter->netdev->features & NETIF_F_HW_VLAN_RX) {

			int i;
			adapter->vlan_grp = grp;

			/*
			 *  Clear entire vfTable; then enable untagged pkts.
			 *  Note: setting one entry in vfTable to non-zero turns
			 *  on VLAN rx filtering.
			 */
			for (i = 0; i < VMXNET3_VFT_SIZE; i++)
				vfTable[i] = 0;

			VMXNET3_SET_VFTABLE_ENTRY(vfTable, 0);
			spin_lock_irqsave(&adapter->cmd_lock, flags);
			VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
					       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
			spin_unlock_irqrestore(&adapter->cmd_lock, flags);
		} else {
			printk(KERN_ERR "%s: vlan_rx_register when device has "
			       "no NETIF_F_HW_VLAN_RX\n", netdev->name);
		}
	} else {
		/* remove vlan rx stripping. */
	        struct Vmxnet3_DSDevRead *devRead = &shared->devRead;

		adapter->vlan_grp = NULL;

		if (le64_to_cpu(devRead->misc.uptFeatures) & UPT1_F_RXVLAN) {
			int i;

			for (i = 0; i < VMXNET3_VFT_SIZE; i++) {
				/* clear entire vfTable; this also disables
				 * VLAN rx filtering
				 */
				vfTable[i] = 0;
			}
			spin_lock_irqsave(&adapter->cmd_lock, flags);
			VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
					       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
			spin_unlock_irqrestore(&adapter->cmd_lock, flags);
		}
	}
}


static void
vmxnet3_restore_vlan(struct vmxnet3_adapter *adapter)
{
	if (adapter->vlan_grp) {
		u16 vid;
		u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;
		Bool activeVlan = FALSE;

		for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
			if (vlan_group_get_device(adapter->vlan_grp,
							 vid)) {
				VMXNET3_SET_VFTABLE_ENTRY(vfTable, vid);
				activeVlan = TRUE;
			}
		}
		if (activeVlan) {
			/* continue to allow untagged pkts */
			VMXNET3_SET_VFTABLE_ENTRY(vfTable, 0);
		}
	}
}

/* Inherit net_device features from real device to VLAN device. */
void
vmxnet3_vlan_features(struct vmxnet3_adapter *adapter, u16 vid, Bool allvids)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26)
	struct net_device *v_netdev;

	if (adapter->vlan_grp) {
		if (allvids) {
			for (vid = 0; vid < VLAN_GROUP_ARRAY_LEN; vid++) {
				v_netdev = vlan_group_get_device(
							adapter->vlan_grp, vid);
				if (v_netdev) {
					v_netdev->features |=
						      adapter->netdev->features;
					vlan_group_set_device(
					      adapter->vlan_grp, vid, v_netdev);
				}
			}
		} else {
			v_netdev = vlan_group_get_device(
							adapter->vlan_grp, vid);
			if (v_netdev) {
				v_netdev->features |= adapter->netdev->features;
				vlan_group_set_device(adapter->vlan_grp,
							     vid, v_netdev);
			}
		}
	}
#endif
}


static void
vmxnet3_vlan_rx_add_vid(struct net_device *netdev, u16 vid)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;
	unsigned long flags;

	vmxnet3_vlan_features(adapter, vid, FALSE);
	VMXNET3_SET_VFTABLE_ENTRY(vfTable, vid);
	spin_lock_irqsave(&adapter->cmd_lock, flags);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);
}


static void
vmxnet3_vlan_rx_kill_vid(struct net_device *netdev, u16 vid)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	u32 *vfTable = adapter->shared->devRead.rxFilterConf.vfTable;
	unsigned long flags;

	VMXNET3_CLEAR_VFTABLE_ENTRY(vfTable, vid);
	spin_lock_irqsave(&adapter->cmd_lock, flags);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_VLAN_FILTERS);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);
}


/*
 * Allocate a buffer and copy into the mcast list. Returns NULL if the mcast
 * list exceeds the limit. Returns the addr of the allocated buffer or NULL.
 */

static u8 *
vmxnet3_copy_mc(struct net_device *netdev)
{
	u8 *buf = NULL;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
	u32 sz = netdev_mc_count(netdev) * ETH_ALEN;
#else
	u32 sz = netdev->mc_count * ETH_ALEN;
#endif

	/* Vmxnet3_RxFilterConf.mfTableLen is u16. */
	if (sz <= 0xffff) {
		/* We may be called with BH disabled */
		buf = kmalloc(sz, GFP_ATOMIC);
		if (buf) {
			int i = 0;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
			struct netdev_hw_addr *ha;
			netdev_for_each_mc_addr(ha, netdev)
				memcpy(buf + i++ * ETH_ALEN, ha->addr,
				       ETH_ALEN);
#else
			struct dev_mc_list *mc = netdev->mc_list;
			for (i = 0; i < netdev->mc_count; i++) {
				BUG_ON(!mc);
				memcpy(buf + i * ETH_ALEN, mc->dmi_addr,
				       ETH_ALEN);
				mc = mc->next;
			}
#endif
		}
	}
	return buf;
}


static void
vmxnet3_set_mc(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	struct Vmxnet3_RxFilterConf *rxConf =
					&adapter->shared->devRead.rxFilterConf;
	unsigned long flags;
	u8 *new_table = NULL;
	u32 new_mode = VMXNET3_RXM_UCAST;

	if (netdev->flags & IFF_PROMISC)
		new_mode |= VMXNET3_RXM_PROMISC;

	if (netdev->flags & IFF_BROADCAST)
		new_mode |= VMXNET3_RXM_BCAST;

	if (netdev->flags & IFF_ALLMULTI)
		new_mode |= VMXNET3_RXM_ALL_MULTI;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
	else if (netdev_mc_count(netdev) > 0) {
#else
	else if (netdev->mc_count > 0) {
#endif
		new_table = vmxnet3_copy_mc(netdev);
		if (new_table) {
			new_mode |= VMXNET3_RXM_MCAST;
			rxConf->mfTableLen = cpu_to_le16(
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 34)
					netdev_mc_count(netdev) * ETH_ALEN);
#else
					netdev->mc_count * ETH_ALEN);
#endif

			rxConf->mfTablePA = cpu_to_le64(virt_to_phys(
					    new_table));
		} else {
			printk(KERN_INFO "%s: failed to copy mcast list, "
			       "setting ALL_MULTI\n", netdev->name);
			new_mode |= VMXNET3_RXM_ALL_MULTI;
		}
	}


	if (!(new_mode & VMXNET3_RXM_MCAST)) {
		rxConf->mfTableLen = 0;
		rxConf->mfTablePA = 0;
	}

	spin_lock_irqsave(&adapter->cmd_lock, flags);
	if (new_mode != rxConf->rxMode) {
		rxConf->rxMode = cpu_to_le32(new_mode);
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
				       VMXNET3_CMD_UPDATE_RX_MODE);
	}

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_MAC_FILTERS);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);

	kfree(new_table);
}


/*
 * Wipes out the whole driver_shared area and re-initializes it
 */

static void
vmxnet3_setup_driver_shared(struct vmxnet3_adapter *adapter)
{
	struct Vmxnet3_DriverShared *shared = adapter->shared;
	struct Vmxnet3_DSDevRead *devRead = &shared->devRead;
	struct Vmxnet3_TxQueueConf *tqc;
	struct Vmxnet3_RxQueueConf *rqc;
	int i;

	memset(shared, 0, sizeof(*shared));

	/* driver settings */
	shared->magic = cpu_to_le32(VMXNET3_REV1_MAGIC);
	devRead->misc.driverInfo.version = cpu_to_le32(
						VMXNET3_DRIVER_VERSION_NUM);
	devRead->misc.driverInfo.gos.gosBits = (sizeof(void *) == 4 ?
				VMXNET3_GOS_BITS_32 : VMXNET3_GOS_BITS_64);
	devRead->misc.driverInfo.gos.gosType = VMXNET3_GOS_TYPE_LINUX;
	*((u32 *)&devRead->misc.driverInfo.gos) = cpu_to_le32(
				*((u32 *)&devRead->misc.driverInfo.gos));
	devRead->misc.driverInfo.vmxnet3RevSpt = cpu_to_le32(1);
	devRead->misc.driverInfo.uptVerSpt = cpu_to_le32(1);

	devRead->misc.ddPA = cpu_to_le64(virt_to_phys(adapter));
	devRead->misc.ddLen = cpu_to_le32(sizeof(struct vmxnet3_adapter));

	/* set up feature flags */
	if (adapter->rxcsum)
		set_flag_le64(&devRead->misc.uptFeatures, UPT1_F_RXCSUM);

	if (adapter->lro) {
		set_flag_le64(&devRead->misc.uptFeatures, UPT1_F_LRO);
		devRead->misc.maxNumRxSG = cpu_to_le16(1 + MAX_SKB_FRAGS);
	}
	if (adapter->netdev->features & NETIF_F_HW_VLAN_RX) {
                 /*
                  * Event there is no VLAN enabled,
                  * VLAN Tag stripping is enabled by default. This is
                  * required to work around  a Palo bug.
                  */
		set_flag_le64(&devRead->misc.uptFeatures, UPT1_F_RXVLAN);
	}

	devRead->misc.mtu = cpu_to_le32(adapter->netdev->mtu);
	devRead->misc.queueDescPA = cpu_to_le64(adapter->queue_desc_pa);
	devRead->misc.queueDescLen = cpu_to_le32(
		adapter->num_tx_queues * sizeof(struct Vmxnet3_TxQueueDesc) +
		adapter->num_rx_queues * sizeof(struct Vmxnet3_RxQueueDesc));

	/* tx queue settings */
	devRead->misc.numTxQueues =  adapter->num_tx_queues;
	for(i = 0; i < adapter->num_tx_queues; i++) {
		struct vmxnet3_tx_queue	*tq = &adapter->tx_queue[i];
		BUG_ON(adapter->tx_queue[i].tx_ring.base == NULL);
		tqc = &adapter->tqd_start[i].conf;
		tqc->txRingBasePA   = cpu_to_le64(tq->tx_ring.basePA);
		tqc->dataRingBasePA = cpu_to_le64(tq->data_ring.basePA);
		tqc->compRingBasePA = cpu_to_le64(tq->comp_ring.basePA);
		tqc->ddPA           = cpu_to_le64(virt_to_phys(tq->buf_info));
		tqc->txRingSize     = cpu_to_le32(tq->tx_ring.size);
		tqc->dataRingSize   = cpu_to_le32(tq->data_ring.size);
		tqc->compRingSize   = cpu_to_le32(tq->comp_ring.size);
		tqc->ddLen          = cpu_to_le32(sizeof(struct vmxnet3_tx_buf_info) *
				      tqc->txRingSize);
		tqc->intrIdx        = tq->comp_ring.intr_idx;
	}

	/* rx queue settings */
	devRead->misc.numRxQueues = adapter->num_rx_queues;
	for(i = 0; i < adapter->num_rx_queues; i++) {
		struct vmxnet3_rx_queue	*rq = &adapter->rx_queue[i];
		rqc = &adapter->rqd_start[i].conf;
		rqc->rxRingBasePA[0] = cpu_to_le64(rq->rx_ring[0].basePA);
		rqc->rxRingBasePA[1] = cpu_to_le64(rq->rx_ring[1].basePA);
		rqc->compRingBasePA  = cpu_to_le64(rq->comp_ring.basePA);
		rqc->ddPA            = cpu_to_le64(virt_to_phys(
							rq->buf_info));
		rqc->rxRingSize[0]   = cpu_to_le32(rq->rx_ring[0].size);
		rqc->rxRingSize[1]   = cpu_to_le32(rq->rx_ring[1].size);
		rqc->compRingSize    = cpu_to_le32(rq->comp_ring.size);
		rqc->ddLen           = cpu_to_le32(sizeof(struct vmxnet3_rx_buf_info) *
				     (rqc->rxRingSize[0] + rqc->rxRingSize[1]));
		rqc->intrIdx         = rq->comp_ring.intr_idx;
	}

#ifdef VMXNET3_RSS
	memset(adapter->rss_conf, 0, sizeof (*adapter->rss_conf));

	if (adapter->rss) {
		UPT1_RSSConf *rssConf = adapter->rss_conf;
		devRead->misc.uptFeatures |= UPT1_F_RSS;
		devRead->misc.numRxQueues = adapter->num_rx_queues;
		rssConf->hashType = UPT1_RSS_HASH_TYPE_TCP_IPV4 |
				    UPT1_RSS_HASH_TYPE_IPV4 |
				    UPT1_RSS_HASH_TYPE_TCP_IPV6 |
				    UPT1_RSS_HASH_TYPE_IPV6;
		rssConf->hashFunc = UPT1_RSS_HASH_FUNC_TOEPLITZ;
		rssConf->hashKeySize = UPT1_RSS_MAX_KEY_SIZE;
		rssConf->indTableSize = VMXNET3_RSS_IND_TABLE_SIZE;
		get_random_bytes(&rssConf->hashKey[0], rssConf->hashKeySize);
		if (num_rss_entries >= adapter->dev_number *
				       VMXNET3_RSS_IND_TABLE_SIZE) {
			int j = (adapter->dev_number) *
				VMXNET3_RSS_IND_TABLE_SIZE;
			for (i = 0; i < rssConf->indTableSize; i++, j++) {
				if (rss_ind_table[j] >= 0 &&
				    rss_ind_table[j] < adapter->num_rx_queues)
					rssConf->indTable[i] = rss_ind_table[j];
				else
					rssConf->indTable[i] = i %
							adapter->num_rx_queues;
			}
		} else {
			for (i = 0; i < rssConf->indTableSize; i++)
				rssConf->indTable[i] = i %
							adapter->num_rx_queues;
		}

		printk(KERN_INFO "RSS indirection table :\n");
		for (i = 0; i < rssConf->indTableSize; i++)
			printk("%2d ", rssConf->indTable[i]);
		printk("\n");

		devRead->rssConfDesc.confVer = 1;
		devRead->rssConfDesc.confLen = sizeof(*rssConf);
		devRead->rssConfDesc.confPA  = virt_to_phys(rssConf);
	}

#endif /* VMXNET3_RSS */

	/* intr settings */
	devRead->intrConf.autoMask = adapter->intr.mask_mode ==
				     VMXNET3_IMM_AUTO;
	devRead->intrConf.numIntrs = adapter->intr.num_intrs;
	for (i = 0; i < adapter->intr.num_intrs; i++)
		devRead->intrConf.modLevels[i] = adapter->intr.mod_levels[i];

	devRead->intrConf.eventIntrIdx = adapter->intr.event_intr_idx;
        devRead->intrConf.intrCtrl |= VMXNET3_IC_DISABLE_ALL;

	/* rx filter settings */
	devRead->rxFilterConf.rxMode = 0;
	vmxnet3_restore_vlan(adapter);
	vmxnet3_write_mac_addr(adapter, adapter->netdev->dev_addr);
	/* the rest are already zeroed */
}

/*
 * put the vNIC into an operational state. After this function finishes, the
 * adapter is fully functional. It does the following:
 * 1. initialize tq and rq
 * 2. fill rx rings with rx buffers
 * 3. setup intr
 * 4. setup driver_shared
 * 5. activate the dev
 * 6. signal the stack that the vNIC is ready to tx/rx
 * 7. enable intrs for the vNIC
 *
 * Returns:
 *    0 if the vNIC is in operation state
 *    error code if any intermediate step fails.
 */

int
vmxnet3_activate_dev(struct vmxnet3_adapter *adapter)
{
	int err, i;
	u32 ret;
	dev_dbg(&adapter->pdev->dev, "%s: skb_buf_size %d, rx_buf_per_pkt %d, "
		"ring sizes %u %u %u\n", adapter->netdev->name,
		adapter->skb_buf_size, adapter->rx_buf_per_pkt,
		adapter->tx_queue[0].tx_ring.size,
		adapter->rx_queue[0].rx_ring[0].size,
		adapter->rx_queue[0].rx_ring[1].size);

	vmxnet3_tq_init_all(adapter);
	err = vmxnet3_rq_init_all(adapter);
	if (err) {
		printk(KERN_ERR "Failed to init rx queue for %s: error %d\n",
		       adapter->netdev->name, err);
		goto rq_err;
	}

	err = vmxnet3_request_irqs(adapter);
	if (err) {
		printk(KERN_ERR "Failed to setup irq for %s: error %d\n",
		       adapter->netdev->name, err);
		goto irq_err;
	}

	vmxnet3_setup_driver_shared(adapter);

	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAL,VMXNET3_GET_ADDR_LO(
			       adapter->shared_pa));
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAH, VMXNET3_GET_ADDR_HI(
			       adapter->shared_pa));

	spin_lock(&adapter->cmd_lock);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_ACTIVATE_DEV);
	ret = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	spin_unlock(&adapter->cmd_lock);
	if (ret != 0) {
		printk(KERN_ERR "Failed to activate dev %s: error %u\n",
		       adapter->netdev->name, ret);
		err = -EINVAL;
		goto activate_err;
	}

	for (i = 0; i < adapter->num_rx_queues; i++) {
		VMXNET3_WRITE_BAR0_REG(adapter, (VMXNET3_REG_RXPROD +
				(i * VMXNET3_REG_ALIGN)),
				adapter->rx_queue[i].rx_ring[0].next2fill);
		VMXNET3_WRITE_BAR0_REG(adapter, (VMXNET3_REG_RXPROD2 +
				(i * VMXNET3_REG_ALIGN)),
				adapter->rx_queue[i].rx_ring[1].next2fill);
	}

	/* Apply the rx filter settins last. */
	vmxnet3_set_mc(adapter->netdev);

	/*
	 * Check link state when first activating device. It will start the
	 * tx queue if the link is up.
	 */
	vmxnet3_check_link(adapter, TRUE);
#ifdef VMXNET3_NAPI
	{
		int i;
		for(i = 0; i < adapter->num_rx_queues; i++)
			napi_enable(&adapter->rx_queue[i].napi);
	}
#endif
	vmxnet3_enable_all_intrs(adapter);
	vmxnet3_start_drop_checker(adapter);
	clear_bit(VMXNET3_STATE_BIT_QUIESCED, &adapter->state);
	return 0;

activate_err:
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAL, 0);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_DSAH, 0);
	vmxnet3_free_irqs(adapter);
irq_err:
rq_err:
	/* free up buffers we allocated */
	vmxnet3_rq_cleanup_all(adapter);
	return err;
}


void
vmxnet3_reset_dev(struct vmxnet3_adapter *adapter)
{
	spin_lock(&adapter->cmd_lock);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD, VMXNET3_CMD_RESET_DEV);
	spin_unlock(&adapter->cmd_lock);
}


/*
 * Stop the device. After this function returns, the adapter stop pkt tx/rx
 * and won't generate intrs. The stack won't try to xmit pkts through us,
 * nor will it poll us for pkts. It does the following:
 *
 * 1. ask the vNIC to quiesce
 * 2. disable the vNIC from generating intrs
 * 3. free intr
 * 4. stop the stack from xmiting pkts thru us and polling
 * 5. free rx buffers
 * 6. tx complete pkts pending
 *
 */

int
vmxnet3_quiesce_dev(struct vmxnet3_adapter *adapter)
{
	unsigned long flags;

	if (test_and_set_bit(VMXNET3_STATE_BIT_QUIESCED, &adapter->state))
		return 0;


	spin_lock_irqsave(&adapter->cmd_lock, flags);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_QUIESCE_DEV);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);
	vmxnet3_stop_drop_checker(adapter);
	vmxnet3_disable_all_intrs(adapter);
#ifdef VMXNET3_NAPI
	{
		int i;
		for(i = 0; i < adapter->num_rx_queues; i++)
			napi_disable(&adapter->rx_queue[i].napi);
	}
#endif
	netif_tx_disable(adapter->netdev);
	adapter->link_speed = 0;
	netif_carrier_off(adapter->netdev);

	vmxnet3_tq_cleanup_all(adapter);
	vmxnet3_rq_cleanup_all(adapter);
	vmxnet3_free_irqs(adapter);
	return 0;
}


static void
vmxnet3_write_mac_addr(struct vmxnet3_adapter *adapter, u8 *mac)
{
	u32 tmp;

	tmp = *(u32 *)mac;
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_MACL, tmp);

	tmp = (mac[5] << 8) | mac[4];
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_MACH, tmp);
}


static int
vmxnet3_set_mac_addr(struct net_device *netdev, void *p)
{
	struct sockaddr *addr = p;
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);

	memcpy(netdev->dev_addr, addr->sa_data, netdev->addr_len);
	vmxnet3_write_mac_addr(adapter, netdev->dev_addr);

	return 0;
}


/* ==================== initialization and cleanup routines ============ */

static int
vmxnet3_alloc_pci_resources(struct vmxnet3_adapter *adapter, Bool *dma64)
{
	int err;
	unsigned long mmio_start, mmio_len;
	struct pci_dev *pdev = adapter->pdev;

	err = pci_enable_device(pdev);
	if (err) {
		printk(KERN_ERR "Failed to enable adapter %s: error %d\n",
		       pci_name(pdev), err);
		return err;
	}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 6)
	if (pci_set_dma_mask(pdev, DMA_BIT_MASK(64)) == 0) {
		if (pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(64)) != 0) {
			printk(KERN_ERR "pci_set_consistent_dma_mask failed "
			       "for adapter %s\n", pci_name(pdev));
			err = -EIO;
			goto err_set_mask;
		}
		*dma64 = TRUE;
	} else {
		if (pci_set_dma_mask(pdev, DMA_BIT_MASK(32)) != 0) {
			printk(KERN_ERR "pci_set_dma_mask failed for adapter "
			       "%s\n", pci_name(pdev));
			err = -EIO;
			goto err_set_mask;
		}
		*dma64 = FALSE;
	}
#else
	*dma64 = TRUE;
#endif

	err = pci_request_regions(pdev, vmxnet3_driver_name);
	if (err) {
		printk(KERN_ERR "Failed to request region for adapter %s: "
		       "error %d\n", pci_name(pdev), err);
		goto err_set_mask;
	}

	pci_set_master(pdev);

	mmio_start = pci_resource_start(pdev, 0);
	mmio_len = pci_resource_len(pdev, 0);
	adapter->hw_addr0 = ioremap(mmio_start, mmio_len);
	if (!adapter->hw_addr0) {
		printk(KERN_ERR "Failed to map bar0 for adapter %s\n",
		       pci_name(pdev));
		err = -EIO;
		goto err_ioremap;
	}

	mmio_start = pci_resource_start(pdev, 1);
	mmio_len = pci_resource_len(pdev, 1);
	adapter->hw_addr1 = ioremap(mmio_start, mmio_len);
	if (!adapter->hw_addr1) {
		printk(KERN_ERR "Failed to map bar1 for adapter %s\n",
		       pci_name(pdev));
		err = -EIO;
		goto err_bar1;
	}
	return 0;

err_bar1:
	iounmap(adapter->hw_addr0);
err_ioremap:
	pci_release_regions(pdev);
err_set_mask:
	pci_disable_device(pdev);
	return err;
}


static void
vmxnet3_free_pci_resources(struct vmxnet3_adapter *adapter)
{
	BUG_ON(!adapter->pdev);

	iounmap(adapter->hw_addr0);
	iounmap(adapter->hw_addr1);
	pci_release_regions(adapter->pdev);
	pci_disable_device(adapter->pdev);
}



/*
 * Calculate the # of buffers for a pkt based on mtu, then adjust the size of
 * the 1st rx ring accordingly
 */

static void
vmxnet3_adjust_rx_ring_size(struct vmxnet3_adapter *adapter)
{
	size_t sz, i, ring0_size, ring1_size, comp_size;
	struct vmxnet3_rx_queue	*rq = &adapter->rx_queue[0];


	if (adapter->netdev->mtu <= VMXNET3_MAX_SKB_BUF_SIZE -
				    VMXNET3_MAX_ETH_HDR_SIZE) {
		adapter->skb_buf_size = adapter->netdev->mtu +
					VMXNET3_MAX_ETH_HDR_SIZE;
		if (adapter->skb_buf_size < VMXNET3_MIN_T0_BUF_SIZE)
			adapter->skb_buf_size = VMXNET3_MIN_T0_BUF_SIZE;

		adapter->rx_buf_per_pkt = 1;
	} else {
		adapter->skb_buf_size = VMXNET3_MAX_SKB_BUF_SIZE;
		sz = adapter->netdev->mtu - VMXNET3_MAX_SKB_BUF_SIZE +
					    VMXNET3_MAX_ETH_HDR_SIZE;
		adapter->rx_buf_per_pkt = 1 + (sz + PAGE_SIZE - 1) / PAGE_SIZE;
	}

	/*
	 * for simplicity, force the ring0 size to be a multiple of
	 * rx_buf_per_pkt * VMXNET3_RING_SIZE_ALIGN
	 */
	sz = adapter->rx_buf_per_pkt * VMXNET3_RING_SIZE_ALIGN;
	ring0_size = adapter->rx_queue[0].rx_ring[0].size;
	ring0_size = (ring0_size + sz - 1) / sz * sz;
	ring0_size = min_t(u32, ring0_size, VMXNET3_RX_RING_MAX_SIZE /
			   sz * sz);
	ring1_size = adapter->rx_queue[0].rx_ring[1].size;
	comp_size = ring0_size + ring1_size;

	for(i = 0; i < adapter->num_rx_queues; i++) {
		rq = &adapter->rx_queue[i];
		rq->rx_ring[0].size = ring0_size;
		rq->rx_ring[1].size = ring1_size;
		rq->comp_ring.size = comp_size;
	}
}

/* Create the specified number of tx queues and rx queues. On failure, it
 * destroys the queues created. */
int
vmxnet3_create_queues(struct vmxnet3_adapter *adapter, u32 tx_ring_size,
		      u32 rx_ring_size, u32 rx_ring2_size)
{
	int err = 0, i;

	for (i = 0; i<adapter->num_tx_queues; i++) {
		struct vmxnet3_tx_queue	*tq = &adapter->tx_queue[i];
		tq->tx_ring.size   = tx_ring_size;
		tq->data_ring.size = tx_ring_size;
		tq->comp_ring.size = tx_ring_size;
		tq->shared = &adapter->tqd_start[i].ctrl;
		tq->stopped = TRUE;
		tq->adapter = adapter;
		tq->qid = i;
		err = vmxnet3_tq_create(tq, adapter);
		/*
		 * Too late to change num_tx_queues. We cannot do away with
		 * lesser number of queues than what we asked for
		 */
		if (err)
			goto queue_err;
	}

	adapter->rx_queue[0].rx_ring[0].size = rx_ring_size;
	adapter->rx_queue[0].rx_ring[1].size = rx_ring2_size;
	vmxnet3_adjust_rx_ring_size(adapter);
	for (i = 0; i<adapter->num_rx_queues; i++) {
		struct vmxnet3_rx_queue *rq = &adapter->rx_queue[i];
		/* qid and qid2 for rx queues will be assigned later when num
		 * of rx queues is finalized after allocating intrs */
		rq->shared = &adapter->rqd_start[i].ctrl;
		rq->adapter = adapter;
		err = vmxnet3_rq_create(rq, adapter);
		if (err) {
			if (i == 0) {
				printk(KERN_ERR "Could not allocate any rx"
				       "queues. Aborting.\n");
				goto queue_err;
			} else {
				printk(KERN_INFO "Number of rx queues changed "
				       "to : %d. \n", i);
				adapter->num_rx_queues = i;
				err = 0;
				break;
			}
		}
	}
	return err;
queue_err:
	vmxnet3_tq_destroy_all(adapter);
	return err;
}

/*
 * setup rings, allocate necessary resources, request for IRQs, configure
 * the device. The device is functional after this function finishes
 * successfully.
 * Returns 0 on success, negative errno value on failure
 */

static int
vmxnet3_open(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter;
	int err, i;
	uint32 flags;

	adapter = netdev_priv(netdev);

	for (i = 0; i < adapter->num_tx_queues; i++)
		spin_lock_init(&adapter->tx_queue[i].tx_lock);

	err = vmxnet3_create_queues(adapter, VMXNET3_DEF_TX_RING_SIZE,
				    VMXNET3_DEF_RX_RING_SIZE,
				    VMXNET3_DEF_RX_RING_SIZE);
	if (err)
		goto queue_err;

	spin_lock(&adapter->cmd_lock);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_GET_ADAPTIVE_RING_INFO);
	flags = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	spin_unlock(&adapter->cmd_lock);

	/* Adaptive ring size is disabled by default, if this must be enabled,
         * issue in PR829305 should be fixed first.
	 */
#ifdef VMXNET3_DROP_CHECKER
	adapter->use_adaptive_ring = !(flags & VMXNET3_DISABLE_ADAPTIVE_RING);
#else
	adapter->use_adaptive_ring = FALSE;
#endif
	if (adapter->use_adaptive_ring) {
		setup_timer(&adapter->drop_check_timer, vmxnet3_drop_checker,
				   (unsigned long)netdev);
		adapter->drop_check_timer.expires = jiffies +
			msecs_to_jiffies(drop_check_interval * 1000);
	}

	err = vmxnet3_activate_dev(adapter);
	if (err)
		goto activate_err;

	return 0;

activate_err:
	vmxnet3_rq_destroy_all(adapter);
	vmxnet3_tq_destroy_all(adapter);
queue_err:
	return err;
}


static int
vmxnet3_close(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);

	/*
	 * Reset_work may be in the middle of resetting the device, wait for its
	 * completion.
	 */
	while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		msleep(1);

	vmxnet3_quiesce_dev(adapter);

	vmxnet3_rq_destroy_all(adapter);
	vmxnet3_tq_destroy_all(adapter);

	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);


	return 0;
}


/*
 * Called to forcibly close the device when the driver failed to re-activate it.
 */
void
vmxnet3_force_close(struct vmxnet3_adapter *adapter)
{
	/*
	 * we must clear VMXNET3_STATE_BIT_RESETTING, otherwise
	 * vmxnet3_close() will deadlock.
	 */
	BUG_ON(test_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state));
#ifdef VMXNET3_NAPI
	{
		/* We need to enable NAPI, otherwise dev_close will deadlock.
		 * dev_close leads us to napi_disable which loops until "polling
		 * scheduled" bit is set. This bit is cleared by napi_enable()
		 */
		int i;
		for (i = 0; i < adapter->num_rx_queues; i++)
			napi_enable(&adapter->rx_queue[i].napi);
	}
#endif	/* VMXNET3_NAPI */
	dev_close(adapter->netdev);
}

int
vmxnet3_set_ringsize(struct net_device *netdev,
		     u32 new_tx_ring_size, u32 new_rx_ring_size)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	int err = 0;

	if (new_tx_ring_size == adapter->tx_queue[0].tx_ring.size &&
	    new_rx_ring_size == adapter->rx_queue[0].rx_ring[0].size) {
		return 0;
	}

	/*
	 * Reset_work may be in the middle of resetting the device, wait for its
	 * completion.
	 */
	while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		msleep(1);

	if (netif_running(netdev)) {
		vmxnet3_quiesce_dev(adapter);
		vmxnet3_reset_dev(adapter);

		/* recreate the rx queue and the tx queue based on the
		 * new sizes */
		vmxnet3_tq_destroy_all(adapter);
		vmxnet3_rq_destroy_all(adapter);

		err = vmxnet3_create_queues(adapter, new_tx_ring_size,
					    new_rx_ring_size,
					    VMXNET3_DEF_RX_RING_SIZE);

		if (err) {
			/* failed, most likely because of OOM, try the default
			 * size */
			printk(KERN_ERR "%s: failed to apply new sizes, try the"
			        "default ones\n", netdev->name);
			err = vmxnet3_create_queues(adapter,
						    VMXNET3_DEF_TX_RING_SIZE,
						    VMXNET3_DEF_RX_RING_SIZE,
						    VMXNET3_DEF_RX_RING_SIZE);
			if (err) {
				printk(KERN_ERR "%s: failed to create queues "
				       "with default sizes. Closing it\n",
				       netdev->name);
				goto out;
			}
		}

		err = vmxnet3_activate_dev(adapter);
		if (err)
			printk(KERN_ERR "%s: failed to re-activate, error %d."
			       " Closing it\n", netdev->name, err);
	}

 out:
	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
	if (err)
		vmxnet3_force_close(adapter);

	return err;
}


static int
vmxnet3_change_mtu(struct net_device *netdev, int new_mtu)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	int err = 0;

	if (new_mtu < VMXNET3_MIN_MTU || new_mtu > VMXNET3_MAX_MTU)
		return -EINVAL;

	if (new_mtu > 1500 && !adapter->jumbo_frame)
		return -EINVAL;

	netdev->mtu = new_mtu;

	/*
	 * Reset_work may be in the middle of resetting the device, wait for its
	 * completion.
	 */
	while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		msleep(1);

	if (netif_running(netdev)) {
		vmxnet3_quiesce_dev(adapter);
		vmxnet3_reset_dev(adapter);

		/* we need to re-create the rx queue based on the new mtu */
		vmxnet3_rq_destroy_all(adapter);
		vmxnet3_adjust_rx_ring_size(adapter);
		err = vmxnet3_rq_create_all(adapter);
		if (err) {
			printk(KERN_ERR "%s: failed to re-create rx queues,"
				" error %d. Closing it.\n", netdev->name, err);
			goto out;
		}

		err = vmxnet3_activate_dev(adapter);
		if (err) {
			printk(KERN_ERR "%s: failed to re-activate, error %d. "
				"Closing it\n", netdev->name, err);
			goto out;
		}
	}

out:
	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
	if (err)
		vmxnet3_force_close(adapter);

	return err;
}


static void
vmxnet3_declare_features(struct vmxnet3_adapter *adapter, Bool dma64)
{
	struct net_device *netdev = adapter->netdev;

	netdev->features = NETIF_F_SG |
		NETIF_F_HW_CSUM |
		NETIF_F_HW_VLAN_TX |
		NETIF_F_HW_VLAN_RX |
		NETIF_F_HW_VLAN_FILTER |
		NETIF_F_TSO;
	printk(KERN_INFO "features: sg csum vlan jf tso");

	adapter->rxcsum = TRUE;
	adapter->jumbo_frame = TRUE;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	 /* LRO feature control by module param */
	if (!disable_lro) {
#else
	 /* LRO feature control by ethtool */
		netdev->features |= NETIF_F_LRO;
#endif

		adapter->lro = TRUE;
		printk(" lro");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
	}
#endif

#ifdef NETIF_F_TSO6
	netdev->features |= NETIF_F_TSO6;
	printk(" tsoIPv6");
#endif

	if (dma64) {
		netdev->features |= NETIF_F_HIGHDMA;
		printk(" highDMA");
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
	netdev->vlan_features = netdev->features;
#endif
	printk("\n");
}


static void
vmxnet3_read_mac_addr(struct vmxnet3_adapter *adapter, u8 *mac)
{
	u32 tmp;

	tmp = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_MACL);
	*(u32 *)mac = tmp;

	tmp = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_MACH);
	mac[4] = tmp & 0xff;
	mac[5] = (tmp >> 8) & 0xff;
}


#ifdef CONFIG_PCI_MSI
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)

/*
 * Enable MSIx vectors.
 * Returns :
 *	0 when number of vectors enabled is between requested and the minimum
 *	    required (VMXNET3_LINUX_MIN_MSIX_VECT),
 *	number of vectors which can be enabled otherwise (this number is smaller
 *	    than VMXNET3_LINUX_MIN_MSIX_VECT)
 */

static int
vmxnet3_acquire_msix_vectors(struct vmxnet3_adapter *adapter,
                             int vectors)
{
	int err = 0, vector_threshold;
	vector_threshold = VMXNET3_LINUX_MIN_MSIX_VECT;

	while (vectors >= vector_threshold) {
		err = pci_enable_msix(adapter->pdev, adapter->intr.msix_entries,
				      vectors);
		if (!err) {
			adapter->intr.num_intrs = vectors;
			return 0;
		} else if (err < 0) {
			printk(KERN_ERR "Failed to enable MSI-X for %s, error"
			       " %d\n",	adapter->netdev->name, err);
			vectors = 0;
		} else if (err < vector_threshold) {
			break;
		} else {
			/* If fails to enable required number of MSI-x vectors
			 * try enabling minimum number of vectors required.
			 */
			vectors = vector_threshold;
			printk(KERN_ERR "Failed to enable %d MSI-X for %s, try"
			       " %d instead\n", vectors, adapter->netdev->name,
			       vector_threshold);
		}
	}

   printk(KERN_INFO "Number of MSI-X interrupts which can be allocated are "
	  "lower than min threshold required.\n");
   return err;
}


#endif /* LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19) */
#endif /* CONFIG_PCI_MSI */

/*
 * read the intr configuration, pick the intr type, and enable MSI/MSI-X if
 * needed. adapter->intr.{type, mask_mode, num_intr} are modified
 */

static void
vmxnet3_alloc_intr_resources(struct vmxnet3_adapter *adapter)
{
	u32 cfg;

	/* intr settings */
	spin_lock(&adapter->cmd_lock);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_GET_CONF_INTR);
	cfg = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_CMD);
	spin_unlock(&adapter->cmd_lock);
	adapter->intr.type = cfg & 0x3;
	adapter->intr.mask_mode = (cfg >> 2) & 0x3;

#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_AUTO) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
		/* start with MSI-X */
		adapter->intr.type = VMXNET3_IT_MSIX;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
		adapter->intr.type = VMXNET3_IT_MSI;
#else
		adapter->intr.type = VMXNET3_IT_INTX;
#endif
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
	if (adapter->intr.type == VMXNET3_IT_MSIX) {
		int vector, err = 0;
		/* start with MSI-X */
		adapter->intr.num_intrs = (adapter->share_intr ==
					   vmxnet3_intr_txshare) ? 1 :
					   adapter->num_tx_queues;
		adapter->intr.num_intrs += (adapter->share_intr ==
					   vmxnet3_intr_buddyshare) ? 0 :
					   adapter->num_rx_queues;
		adapter->intr.num_intrs += 1;		/* for link event */

		adapter->intr.num_intrs = (adapter->intr.num_intrs >
					   VMXNET3_LINUX_MIN_MSIX_VECT
					   ? adapter->intr.num_intrs :
					   VMXNET3_LINUX_MIN_MSIX_VECT);

		for (vector = 0; vector < adapter->intr.num_intrs; vector++)
			adapter->intr.msix_entries[vector].entry = vector;

		err = vmxnet3_acquire_msix_vectors(adapter,
						   adapter->intr.num_intrs);
		/* If we cannot allocate one MSIx vector per queue
		 * then limit the number of rx queues to 1
		 */
		if(err == VMXNET3_LINUX_MIN_MSIX_VECT) {
			if (adapter->share_intr != vmxnet3_intr_buddyshare
			    || adapter->num_rx_queues != 1) {
				adapter->share_intr = vmxnet3_intr_txshare;
				printk(KERN_ERR "Number of rx queues : 1\n");
				adapter->num_rx_queues = 1;
				adapter->intr.num_intrs =
						VMXNET3_LINUX_MIN_MSIX_VECT;
			}
			return;
		}
		if (!err)
			return;

		/* If we cannot allocate MSIx vectors use only one rx queue */
		printk(KERN_INFO "Failed to enable MSI-X for %s, error %d."
		       "#rx queues : 1, try MSI\n", adapter->netdev->name, err);

		adapter->intr.type = VMXNET3_IT_MSI;
	}
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
	if (adapter->intr.type == VMXNET3_IT_MSI) {
		if (!pci_enable_msi(adapter->pdev)) {
			adapter->num_rx_queues = 1;
			adapter->intr.num_intrs = 1;
			return;
		}
	}
#endif
#endif		/* CONFIG_MSI */
	adapter->num_rx_queues = 1;
	printk(KERN_INFO "Using INTx interrupt, #Rx queues: 1 \n");
	adapter->intr.type = VMXNET3_IT_INTX;

	/* INT-X related setting */
	adapter->intr.num_intrs = 1;
}


static void
vmxnet3_free_intr_resources(struct vmxnet3_adapter *adapter)
{
#ifdef CONFIG_PCI_MSI
	if (adapter->intr.type == VMXNET3_IT_MSIX)
		pci_disable_msix(adapter->pdev);
	else if (adapter->intr.type == VMXNET3_IT_MSI)
		pci_disable_msi(adapter->pdev);
	else
#endif
	{
		BUG_ON(adapter->intr.type != VMXNET3_IT_INTX);
	}

}


/*
 * Called when the stack detects a Tx hang. Schedule a job to reset the device
 */

static void
vmxnet3_tx_timeout(struct net_device *netdev)
{
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	adapter->tx_timeout_count++;

	printk(KERN_ERR "%s: tx hang\n", adapter->netdev->name);
	schedule_work(&adapter->reset_work);
}

static void
vmxnet3_resize_ring_work(struct work_struct * data)
{
	struct vmxnet3_adapter *adapter;
	adapter = container_of(data, struct vmxnet3_adapter, resize_ring_work);

	(void)vmxnet3_set_ringsize(adapter->netdev,
				   adapter->tx_queue[0].tx_ring.size,
				   adapter->new_rx_ring_size);
}

static void
vmxnet3_reset_work(struct work_struct *data)
{
	struct vmxnet3_adapter *adapter;
	adapter = container_of(data, struct vmxnet3_adapter, reset_work);

	if (!adapter) {
		printk("vmxnet3 Adapter is NULL !!");
		return;
        }

	/* if another thread is resetting the device, no need to proceed */
	if (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		return;

	/* if the device is closed or is being opened, we must leave it alone */
	if (netif_running(adapter->netdev)  &&
	    (adapter->netdev->flags & IFF_UP)) {
		printk(KERN_INFO "%s: resetting\n", adapter->netdev->name);

		vmxnet3_quiesce_dev(adapter);
		vmxnet3_reset_dev(adapter);
		vmxnet3_activate_dev(adapter);
	} else {
		printk(KERN_INFO "%s: already closed\n", adapter->netdev->name);
	}

	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
}


/*
 * Initialize a vmxnet3 device. Returns 0 on success, negative errno code
 * otherwise. Initialize the h/w and allocate necessary resources
 */

static int __devinit
vmxnet3_probe_device(struct pci_dev *pdev,
		     const struct pci_device_id *id)
{
#ifdef HAVE_NET_DEVICE_OPS
	static const struct net_device_ops vmxnet3_netdev_ops = {
		.ndo_open = vmxnet3_open,
		.ndo_stop = vmxnet3_close,
		.ndo_start_xmit = vmxnet3_xmit_frame,
		.ndo_set_mac_address = vmxnet3_set_mac_addr,
		.ndo_change_mtu = vmxnet3_change_mtu,
		.ndo_get_stats = vmxnet3_get_stats,
		.ndo_tx_timeout = vmxnet3_tx_timeout,
		.ndo_set_multicast_list = vmxnet3_set_mc,
		.ndo_vlan_rx_register = vmxnet3_vlan_rx_register,
		.ndo_vlan_rx_add_vid = vmxnet3_vlan_rx_add_vid,
		.ndo_vlan_rx_kill_vid = vmxnet3_vlan_rx_kill_vid,
#ifdef CONFIG_NET_POLL_CONTROLLER
		.ndo_poll_controller = vmxnet3_netpoll,
#endif
	};
#endif	/* HAVE_NET_DEVICE_OPS  */
	int err;
	Bool dma64 = FALSE; /* stupid gcc */
	u32 ver;
	struct net_device *netdev;
	struct vmxnet3_adapter *adapter;
	u8 mac[ETH_ALEN];
	int size;
	int num_tx_queues = num_tqs[atomic_read(&devices_found)];
	int num_rx_queues = num_rqs[atomic_read(&devices_found)];

#ifdef VMXNET3_RSS
	if (num_rx_queues <= 0)
		num_rx_queues = min(VMXNET3_DEVICE_MAX_RX_QUEUES,
				    (int)num_online_cpus());
	else
		num_rx_queues = min(VMXNET3_DEVICE_MAX_RX_QUEUES,num_rx_queues);
#else
	num_rx_queues = 1;
#endif

	/* CONFIG_NETDEVICES_MULTIQUEUE dictates if skb has queue_mapping field.
	 * It was introduced in 2.6.23 onwards but Ubuntu 804 (2.6.24) and
	 * SLES 11(2.6.25) do not have it.
	 */

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25) || defined(CONFIG_NETDEVICES_MULTIQUEUE)
	if (num_tx_queues <= 0)
		num_tx_queues = min(VMXNET3_DEVICE_MAX_TX_QUEUES,
				    (int)num_online_cpus());
	else
		num_tx_queues = min(VMXNET3_DEVICE_MAX_TX_QUEUES, num_tx_queues);
	netdev = alloc_etherdev_mq(sizeof(struct vmxnet3_adapter),
				   num_tx_queues);
#else
	num_tx_queues = 1;
	netdev = alloc_etherdev(sizeof(struct vmxnet3_adapter));
#endif
	printk(KERN_INFO "# of Tx queues : %d, # of Rx queues : %d\n",
	       num_tx_queues, num_rx_queues);

	if (!netdev) {
		printk(KERN_ERR "Failed to alloc ethernet device %s\n",
			pci_name(pdev));
		return -ENOMEM;
	}

	pci_set_drvdata(pdev, netdev);
	adapter = netdev_priv(netdev);
	adapter->netdev = netdev;
	adapter->pdev = pdev;
	spin_lock_init(&adapter->cmd_lock);

	err = vmxnet3_alloc_pci_resources(adapter, &dma64);
	if (err < 0)
		goto err_alloc_pci;


	adapter->shared = pci_alloc_consistent(adapter->pdev,
			  sizeof(struct Vmxnet3_DriverShared),
			  &adapter->shared_pa);
	if (!adapter->shared) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
			pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_shared;
	}

	adapter->num_rx_queues = num_rx_queues;
	adapter->num_tx_queues = num_tx_queues;

	size = sizeof(struct Vmxnet3_TxQueueDesc) * adapter->num_tx_queues;
	size += sizeof(struct Vmxnet3_RxQueueDesc) * adapter->num_rx_queues;
	adapter->tqd_start = pci_alloc_consistent(adapter->pdev, size,
			     &adapter->queue_desc_pa);

	if (!adapter->tqd_start) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
			pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_queue_desc;
	}
	adapter->rqd_start = (struct Vmxnet3_RxQueueDesc *)(adapter->tqd_start +
                                                        adapter->num_tx_queues);

	adapter->pm_conf = kmalloc(sizeof(struct Vmxnet3_PMConf), GFP_KERNEL);
	if (adapter->pm_conf == NULL) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
			pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_pm;
	}

#ifdef VMXNET3_RSS

	adapter->rss_conf = kmalloc(sizeof(struct UPT1_RSSConf), GFP_KERNEL);
	if (adapter->rss_conf == NULL) {
		printk(KERN_ERR "Failed to allocate memory for %s\n",
		       pci_name(pdev));
		err = -ENOMEM;
		goto err_alloc_rss;
	}
#endif /* VMXNET3_RSS */

	ver = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_VRRS);
	if (ver & 1) {
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_VRRS, 1);
	} else {
		printk(KERN_ERR "Incompatible h/w version (0x%x) for adapter"
		       " %s\n",	ver, pci_name(pdev));
		err = -EBUSY;
		goto err_ver;
	}

	ver = VMXNET3_READ_BAR1_REG(adapter, VMXNET3_REG_UVRS);
	if (ver & 1) {
		VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_UVRS, 1);
	} else {
		printk(KERN_ERR "Incompatible upt version (0x%x) for "
		       "adapter %s\n", ver, pci_name(pdev));
		err = -EBUSY;
		goto err_ver;
	}

	vmxnet3_declare_features(adapter, dma64);

	adapter->dev_number = atomic_read(&devices_found);

	/*
	 * Sharing intr between corresponding tx and rx queues gets priority
	 * over all tx queues sharing an intr. Also, to use buddy interrupts
	 * number of tx queues should be same as number of rx queues.
	 */
	if (share_tx_intr[adapter->dev_number] == 1)
		adapter->share_intr = vmxnet3_intr_txshare;
	else if (buddy_intr[adapter->dev_number] == 1 &&
		 adapter->num_tx_queues == adapter->num_rx_queues)
		adapter->share_intr = vmxnet3_intr_buddyshare;
	else
		adapter->share_intr = vmxnet3_intr_noshare;

	vmxnet3_alloc_intr_resources(adapter);
#ifdef VMXNET3_RSS
	if (adapter->num_rx_queues > 1 &&
	    adapter->intr.type == VMXNET3_IT_MSIX) {
		adapter->rss = TRUE;
		printk("RSS is enabled.\n");
	} else {
		adapter->rss = FALSE;
	}
#endif
	vmxnet3_read_mac_addr(adapter, mac);
	memcpy(netdev->dev_addr,  mac, netdev->addr_len);

#ifdef HAVE_NET_DEVICE_OPS
	netdev->netdev_ops = &vmxnet3_netdev_ops;
#else
	netdev->open  = vmxnet3_open;
	netdev->stop  = vmxnet3_close;
	netdev->hard_start_xmit = vmxnet3_xmit_frame;
	netdev->set_mac_address = vmxnet3_set_mac_addr;
	netdev->change_mtu = vmxnet3_change_mtu;
	netdev->get_stats = vmxnet3_get_stats;
	netdev->tx_timeout = vmxnet3_tx_timeout;
	netdev->set_multicast_list = vmxnet3_set_mc;
	netdev->vlan_rx_register = vmxnet3_vlan_rx_register;
	netdev->vlan_rx_add_vid  = vmxnet3_vlan_rx_add_vid;
	netdev->vlan_rx_kill_vid = vmxnet3_vlan_rx_kill_vid;

#ifdef CONFIG_NET_POLL_CONTROLLER
	netdev->poll_controller  = vmxnet3_netpoll;
#endif
#endif /* HAVE_NET_DEVICE_OPS  */
	netdev->watchdog_timeo = 5 * HZ;
	vmxnet3_set_ethtool_ops(netdev);

	INIT_WORK(&adapter->reset_work, vmxnet3_reset_work);
	INIT_WORK(&adapter->resize_ring_work, vmxnet3_resize_ring_work);
	set_bit(VMXNET3_STATE_BIT_QUIESCED, &adapter->state);

#ifdef VMXNET3_NAPI
	if (adapter->intr.type == VMXNET3_IT_MSIX) {
		int i;
		for (i = 0; i < adapter->num_rx_queues; i++) {
			netif_napi_add(netdev,
					      &adapter->rx_queue[i].napi,
					      vmxnet3_poll_rx_only, 64);
		}
	} else {
		netif_napi_add(netdev, &adapter->rx_queue[0].napi,
			       vmxnet3_poll, 64);
	}

#endif

	SET_NETDEV_DEV(netdev, &pdev->dev);

	err = register_netdev(netdev);
	if (err) {
		printk(KERN_ERR "Failed to register adapter %s\n",
		       pci_name(pdev));
		goto err_register;
	}

	vmxnet3_check_link(adapter, FALSE);
	atomic_inc(&devices_found);
	return 0;

err_register:
	vmxnet3_free_intr_resources(adapter);
err_ver:
	vmxnet3_free_pci_resources(adapter);
err_alloc_pci:
#ifdef VMXNET3_RSS
	kfree(adapter->rss_conf);
err_alloc_rss:
#endif
	kfree(adapter->pm_conf);
err_alloc_pm:
	pci_free_consistent(adapter->pdev, size, adapter->tqd_start,
			    adapter->queue_desc_pa);
err_alloc_queue_desc:
	pci_free_consistent(adapter->pdev, sizeof(struct Vmxnet3_DriverShared),
			    adapter->shared, adapter->shared_pa);
err_alloc_shared:
	pci_set_drvdata(pdev, NULL);
	free_netdev(netdev);
	return err;
}


static void __devexit
vmxnet3_remove_device(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	int size = 0;
	int num_rx_queues = num_rqs[adapter->dev_number];
#ifdef VMXNET3_RSS
	if (num_rx_queues <= 0)
		num_rx_queues = min(VMXNET3_DEVICE_MAX_RX_QUEUES,
				    (int)num_online_cpus());
	else
		num_rx_queues = min(VMXNET3_DEVICE_MAX_RX_QUEUES,
				    num_rx_queues);
#else
	num_rx_queues = 1;
#endif

	cancel_work_sync(&adapter->resize_ring_work);
	cancel_work_sync(&adapter->reset_work);
	flush_scheduled_work();

	unregister_netdev(netdev);

	vmxnet3_free_intr_resources(adapter);
	vmxnet3_free_pci_resources(adapter);
#ifdef VMXNET3_RSS
	kfree(adapter->rss_conf);
#endif
	kfree(adapter->pm_conf);

	size = sizeof(struct Vmxnet3_TxQueueDesc) * adapter->num_tx_queues;
	size += sizeof(struct Vmxnet3_RxQueueDesc) * num_rx_queues;
	pci_free_consistent(adapter->pdev, size, adapter->tqd_start,
			    adapter->queue_desc_pa);
	pci_free_consistent(adapter->pdev, sizeof(struct Vmxnet3_DriverShared),
			    adapter->shared, adapter->shared_pa);
	free_netdev(netdev);
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)

static void vmxnet3_shutdown_device(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	unsigned long flags;

	/*
	 * Reset_work may be in the middle of resetting the device, wait for its
	 * completion.
	 */
	while (test_and_set_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state))
		msleep(1);

		if (test_and_set_bit(VMXNET3_STATE_BIT_QUIESCED,
		    &adapter->state)) {
			clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
			return;
		}
	spin_lock_irqsave(&adapter->cmd_lock, flags);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_QUIESCE_DEV);
	spin_unlock_irqrestore(&adapter->cmd_lock, flags);
	vmxnet3_stop_drop_checker(adapter);
	vmxnet3_disable_all_intrs(adapter);

	clear_bit(VMXNET3_STATE_BIT_RESETTING, &adapter->state);
}

#endif

#ifdef CONFIG_PM

/*
 *      May programs the wake-up filters if configured to do so.
 */

static int
vmxnet3_suspend(struct pci_dev *pdev, pm_message_t state)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	struct Vmxnet3_PMConf *pmConf;
	struct ethhdr *ehdr;
	struct arphdr *ahdr;
	u8 *arpreq;
	struct in_device *in_dev;
	struct in_ifaddr *ifa;
	int i = 0;

	if (!netif_running(netdev))
		return 0;

	vmxnet3_stop_drop_checker(adapter);
#ifdef VMXNET3_NAPI
	for(i = 0; i < adapter->num_rx_queues; i++)
		napi_disable(&adapter->rx_queue[i].napi);

#endif
	vmxnet3_disable_all_intrs(adapter);
	vmxnet3_free_irqs(adapter);
	vmxnet3_free_intr_resources(adapter);
	netif_device_detach(netdev);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
	netif_stop_queue(netdev);
#else
	netif_tx_stop_all_queues(netdev);
#endif

	/* Create wake-up filters. */
	pmConf = adapter->pm_conf;
	memset(pmConf, 0, sizeof(*pmConf));

	if (adapter->wol & WAKE_UCAST) {
		pmConf->filters[i].patternSize = ETH_ALEN;
		pmConf->filters[i].maskSize = 1;
		memcpy(pmConf->filters[i].pattern, netdev->dev_addr, ETH_ALEN);
		pmConf->filters[i].mask[0] = 0x3F; /* LSB ETH_ALEN bits */

		set_flag_le16(&pmConf->wakeUpEvents, VMXNET3_PM_WAKEUP_FILTER);
		i++;
	}

	if (adapter->wol & WAKE_ARP) {
		in_dev = in_dev_get(netdev);
		if (!in_dev)
			goto skip_arp;

		ifa = (struct in_ifaddr *)in_dev->ifa_list;
		if (!ifa) {
			dev_dbg(&adapter->pdev->dev, "Cannot program WoL ARP"
				" filter for %s: no IPv4 address.\n",
				netdev->name);
			in_dev_put(in_dev);
			goto skip_arp;
		}
		pmConf->filters[i].patternSize = ETH_HLEN + /* Ethernet header */
			sizeof(struct arphdr) +                  /* ARP header */
			2 * ETH_ALEN +                           /* 2 Ethernet addresses */
			2 * sizeof (u32);                     /* 2 IPv4 addresses */
		pmConf->filters[i].maskSize =
			(pmConf->filters[i].patternSize - 1) / 8 + 1;
		/* ETH_P_ARP in Ethernet header. */
		ehdr = (struct ethhdr *)pmConf->filters[i].pattern;
		ehdr->h_proto = htons(ETH_P_ARP);
		/* ARPOP_REQUEST in ARP header. */
		ahdr = (struct arphdr *)&pmConf->filters[i].pattern[ETH_HLEN];
		ahdr->ar_op = htons(ARPOP_REQUEST);
		arpreq = (u8 *)(ahdr + 1);

		/* The Unicast IPv4 address in 'tip' field. */
		arpreq += 2 * ETH_ALEN + sizeof(u32);
		*(u32 *)arpreq = ifa->ifa_address;

		/* The mask for the relevant bits. */
		pmConf->filters[i].mask[0] = 0x00;
		pmConf->filters[i].mask[1] = 0x30; /* ETH_P_ARP */
		pmConf->filters[i].mask[2] = 0x30; /* ARPOP_REQUEST */
		pmConf->filters[i].mask[3] = 0x00;
		pmConf->filters[i].mask[4] = 0xC0; /* IPv4 TIP */
		pmConf->filters[i].mask[5] = 0x03; /* IPv4 TIP */
		in_dev_put(in_dev);

		set_flag_le16(&pmConf->wakeUpEvents, VMXNET3_PM_WAKEUP_FILTER);
		i++;
	}

skip_arp:
	if (adapter->wol & WAKE_MAGIC)
		set_flag_le16(&pmConf->wakeUpEvents, VMXNET3_PM_WAKEUP_MAGIC);

	pmConf->numFilters = i;

	adapter->shared->devRead.pmConfDesc.confVer = cpu_to_le32(1);
	adapter->shared->devRead.pmConfDesc.confLen = cpu_to_le32(sizeof(
								  *pmConf));
	adapter->shared->devRead.pmConfDesc.confPA = cpu_to_le64(virt_to_phys(
								 pmConf));

	spin_lock(&adapter->cmd_lock);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_PMCFG);
	spin_unlock(&adapter->cmd_lock);

	pci_save_state(pdev);
	pci_enable_wake(pdev, pci_choose_state(pdev, state),
			adapter->wol);
	pci_disable_device(pdev);
	pci_set_power_state(pdev, pci_choose_state(pdev, state));

	return 0;
}


static int
vmxnet3_resume(struct pci_dev *pdev)
{
	int err;
	struct net_device *netdev = pci_get_drvdata(pdev);
	struct vmxnet3_adapter *adapter = netdev_priv(netdev);
	struct Vmxnet3_PMConf *pmConf;

	if (!netif_running(netdev))
		return 0;

	/* Destroy wake-up filters. */
	pmConf = adapter->pm_conf;
	memset(pmConf, 0, sizeof(*pmConf));

	adapter->shared->devRead.pmConfDesc.confVer = cpu_to_le32(1);
	adapter->shared->devRead.pmConfDesc.confLen = cpu_to_le32(sizeof(
								  *pmConf));
	adapter->shared->devRead.pmConfDesc.confPA = cpu_to_le32(virt_to_phys(
								 pmConf));

	netif_device_attach(netdev);
	pci_set_power_state(pdev, PCI_D0);
	pci_restore_state(pdev);
	err = pci_enable_device(pdev);
	if (err != 0)
		return err;

	pci_enable_wake(pdev, PCI_D0, 0);

	spin_lock(&adapter->cmd_lock);
	VMXNET3_WRITE_BAR1_REG(adapter, VMXNET3_REG_CMD,
			       VMXNET3_CMD_UPDATE_PMCFG);
	spin_unlock(&adapter->cmd_lock);
	vmxnet3_alloc_intr_resources(adapter);
	vmxnet3_request_irqs(adapter);
#ifdef VMXNET3_NAPI
	{
		int i;
		for(i = 0; i < adapter->num_rx_queues; i++)
			napi_enable(&adapter->rx_queue[i].napi);
	}
#endif
	vmxnet3_enable_all_intrs(adapter);
	vmxnet3_start_drop_checker(adapter);

	return 0;
}

#endif

static struct pci_driver vmxnet3_driver = {
	.name		= vmxnet3_driver_name,
	.id_table	= vmxnet3_pciid_table,
	.probe		= vmxnet3_probe_device,
	.remove		= __devexit_p(vmxnet3_remove_device),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 12)
	.shutdown	= vmxnet3_shutdown_device,
#endif
#ifdef CONFIG_PM
	.suspend	= vmxnet3_suspend,
	.resume		= vmxnet3_resume,
#endif
};


static int __init
vmxnet3_init_module(void)
{
	printk(KERN_INFO "%s - version %s\n", VMXNET3_DRIVER_DESC,
	       VMXNET3_DRIVER_VERSION_REPORT);

	atomic_set(&devices_found, 0);

	return pci_register_driver(&vmxnet3_driver);
}

module_init(vmxnet3_init_module);

static void
vmxnet3_exit_module(void)
{
	pci_unregister_driver(&vmxnet3_driver);
}


module_exit(vmxnet3_exit_module);

MODULE_AUTHOR("VMware, Inc.");
MODULE_DESCRIPTION(VMXNET3_DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(VMXNET3_DRIVER_VERSION_STRING);
/*
 * Starting with SLE10sp2, Novell requires that IHVs sign a support agreement
 * with them and mark their kernel modules as externally supported via a
 * change to the module header. If this isn't done, the module will not load
 * by default (i.e., neither mkinitrd nor modprobe will accept it).
 */
MODULE_INFO(supported, "external");
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 24)
module_param(disable_lro, int, 0);
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 25) || \
    defined(CONFIG_NETDEVICES_MULTIQUEUE)
static unsigned int num_adapters = 0;
module_param_array(share_tx_intr, int, &num_adapters, 0);
MODULE_PARM_DESC(share_tx_intr, "Share an intr among all tx completions. "
		 "Comma separated list of 1s and 0s - one for each NIC. "
		 "1 to share, 0 to not, default is 0");
module_param_array(buddy_intr, int, &num_adapters, 0);
MODULE_PARM_DESC(buddy_intr, "Share an intr among corresponding tx and rx "
		 "queues. Comma separated list of 1s and 0s - one for each "
		 "NIC. 1 to share, 0 to not, default is 1");
module_param_array(num_tqs, int, &num_adapters, 0);
MODULE_PARM_DESC(num_tqs, "Number of transmit queues in each adapter. Comma "
		 "separated list of ints. Default is 0 which makes number"
		 " of queues same as number of CPUs");
#endif
#ifdef VMXNET3_RSS
module_param_array(rss_ind_table, int, &num_rss_entries, 0);
MODULE_PARM_DESC(rss_ind_table, "RSS Indirection table. Number of entries "
		 "per NIC should be 32. Each comma separated entry is a rx "
		 "queue number starting with 0. Repeat the same for all NICs");
module_param_array(num_rqs, int, &num_adapters, 0);
MODULE_PARM_DESC(num_rqs, "Number of receive queues in each adapter. Comma "
		 " separated list of ints. Default is 0 which makes number"
		 " of queues same as number of CPUs");

#endif /* VMXNET3_RSS */
module_param(drop_check_noise, uint, 0);
MODULE_PARM_DESC(drop_check_noise, "Number of drops per interval which are ignored");
module_param(drop_check_grow_threshold, uint, 0);
MODULE_PARM_DESC(drop_check_shrink_threshold, "Threshold for growing the ring");
