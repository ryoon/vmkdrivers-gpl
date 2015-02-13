/* bnx2x_esx.c: Broadcom Everest network driver.
 *
 * Copyright 2007-2011 Broadcom Corporation
 *
 * Portions Copyright (c) VMware, Inc. 2008-2011, All Rights Reserved.
 * Copyright (c) 2007-2011 Broadcom Corporation
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
 * Maintained by: Benjamin Li <benli@broadcom.com>
 * NetQueue code from VMware
 * IOCTL code by: Benjamin Li
 * PASSTHRU code by: Shmulik Ravid
 *
 */

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/delay.h>
#include <linux/ethtool.h>
#ifdef __LINUX_MUTEX_H
#include <linux/mutex.h>
#endif
#include <linux/version.h>

#include "bnx2x.h"
#include "bnx2x_common.h"


#ifdef BNX2X_VMWARE_BMAPILNX
int
bnx2x_ioctl_cim(struct net_device *dev, struct ifreq *ifr)
{
	struct bnx2x *bp = netdev_priv(dev);
	void __user *useraddr = ifr->ifr_data;
	struct bnx2x_ioctl_req req;
	int rc = 0;
	u32 val;

	/* We assume all userworld apps are 32 bits ==> ifr->ifr_data is
	   a 32 bit pointer so we truncate useraddr */
	unsigned long uaddr32 = (unsigned long)useraddr & 0xffffffff;

	if (copy_from_user(&req, (void __user *)uaddr32, sizeof(req))) {
		BNX2X_ERR("%s: bnx2x_ioctl() could not copy from user"
			  "bnx2x_ioctl_req\n", bp->dev->name);
		return -EFAULT;
	}

	DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() CIM cmd: 0x%x\n",
	       bp->dev->name, req.cmd);

	switch(req.cmd) {
	case BNX2X_VMWARE_CIM_CMD_ENABLE_NIC:
		DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() enable NIC\n",
		       bp->dev->name);

		rc = bnx2x_open(bp->dev);
		break;
	case BNX2X_VMWARE_CIM_CMD_DISABLE_NIC:
		DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() enable NIC\n",
		       bp->dev->name);

		rc = bnx2x_close(bp->dev);
		break;
	case BNX2X_VMWARE_CIM_CMD_REG_READ: {
		u32 mem_len;

		mem_len = pci_resource_len(bp->pdev, 0);
		if(mem_len < req.cmd_req.reg_read.reg_offset) {
			BNX2X_ERR("%s: bnx2x_ioctl() reg read: "
					"out of range: max reg: 0x%x "
					"req reg: 0x%x\n",
				bp->dev->name,
				mem_len, req.cmd_req.reg_read.reg_offset);
			rc = -EINVAL;
			break;
		}

		val = REG_RD(bp, req.cmd_req.reg_read.reg_offset);

		DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() reg read: "
				 "reg: 0x%x value:0x%x",
		       bp->dev->name,
		       req.cmd_req.reg_read.reg_offset,
		       req.cmd_req.reg_read.reg_value);

		req.cmd_req.reg_read.reg_value = val;

		break;
	} case BNX2X_VMWARE_CIM_CMD_REG_WRITE: {
		u32 mem_len;

		mem_len = pci_resource_len(bp->pdev, 0);
		if(mem_len < req.cmd_req.reg_write.reg_offset) {
			BNX2X_ERR("%s: bnx2x_ioctl() reg write: "
				  "out of range: max reg: 0x%x "
				  "req reg: 0x%x\n",
				bp->dev->name,
				mem_len, req.cmd_req.reg_write.reg_offset);
			rc = -EINVAL;
			break;
		}

		DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() reg write: "
				 "reg: 0x%x value:0x%x",
		       bp->dev->name,
		       req.cmd_req.reg_write.reg_offset,
		       req.cmd_req.reg_write.reg_value);

		REG_WR(bp, req.cmd_req.reg_write.reg_offset,
			   req.cmd_req.reg_write.reg_value);

		break;
	} case BNX2X_VMWARE_CIM_CMD_GET_NIC_PARAM:
		DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() get NIC param\n",
		       bp->dev->name);

		req.cmd_req.get_nic_param.mtu = dev->mtu;
		memcpy(req.cmd_req.get_nic_param.current_mac_addr,
		       dev->dev_addr,
		       sizeof(req.cmd_req.get_nic_param.current_mac_addr));
		break;
	case BNX2X_VMWARE_CIM_CMD_GET_NIC_STATUS:
		DP(NETIF_MSG_LINK, "%s: bnx2x_ioctl() get NIC status\n",
		       bp->dev->name);

		req.cmd_req.get_nic_status.nic_status = netif_running(dev);
		break;
	default:
		BNX2X_ERR("%s: bnx2x_ioctl() unknown req.cmd: 0x%x\n",
		       bp->dev->name, req.cmd);
		rc = -EINVAL;
	}

	if (rc == 0 &&
	    copy_to_user((void __user *)uaddr32, &req, sizeof(req))) {
		BNX2X_ERR("%s: bnx2x_ioctl() couldn't copy to user "
			  "bnx2_ioctl_req\n", bp->dev->name);
		return -EFAULT;
	}

	return rc;
}
#endif

#ifdef BNX2X_NETQ

/* handles an sp event specific to a netq. The routine is called for all sp
 * events, return 0 if it handled the event, non-zero otherwise
 */
int bnx2x_netq_sp_event(struct bnx2x *bp, struct bnx2x_fastpath *fp,
			int cid, int command)
{
	/* The only 'empty ramrod' is sent when clearing the netq rx filter */
	if (command == RAMROD_CMD_ID_ETH_EMPTY) {
		DP(BNX2X_MSG_NETQ, "got Empty rmarod. CID %d\n", cid);
		fp->netq_filter_event = 0;
		return 0;
	}
	return 1;
}


/* not including the default queue - qid 0*/
static inline int bnx2x_netq_valid_qid(struct bnx2x *bp, u16 qid)
{
	return ((qid > 0) && (qid <= BNX2X_NUM_NETQUEUES(bp)));
}

void bnx2x_reserve_netq_feature(struct bnx2x *bp)
{
	int i;

#if (VMWARE_ESX_DDK_VERSION >= 41000)
	int num_lro_reserved = 0;
	int max_features = BNX2X_NETQ_FP_FEATURES_RESERVED + 1;
#endif

	for_each_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];
	#if (VMWARE_ESX_DDK_VERSION < 41000)
		fp->disable_tpa = 1;
	#else
		/* clear features flags */
		fp->netq_flags &= ~BNX2X_NETQ_FP_FEATURES_RESERVED_MASK;

		switch ((i % max_features) <<
			BNX2X_NETQ_FP_FEATURE_RESERVED_SHIFT_BIT) {
		case BNX2X_NETQ_FP_LRO_RESERVED:
			if ((num_lro_reserved <
			     BNX2X_NETQ_FP_LRO_RESERVED_MAX_NUM(bp)) &&
			    (bp->flags & TPA_ENABLE_FLAG)) {
				fp->netq_flags |= BNX2X_NETQ_FP_LRO_RESERVED;
				DP(BNX2X_MSG_NETQ, "Queue[%d] is reserved "
						   "for LRO\n", i);
				num_lro_reserved++;
			} else {
				/* either TPA is glabally disabled or we
				 * already reserved the max number of LRO queues
				 */
				DP(BNX2X_MSG_NETQ, "Queue[%d] is non-LRO\n", i);
				fp->disable_tpa = 1;
			}
			break;

		default:
			/* Do not reserve for LRO */
			DP(BNX2X_MSG_NETQ, "Queue[%d] is non-LRO\n", i);
			fp->disable_tpa = 1;
			break;
		}
	#endif
	}
}

static int bnx2x_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);
	u16 netq_count = max_t(u16, BNX2X_NUM_NETQUEUES(bp), 0);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = netq_count;
		return VMKNETDDI_QUEUEOPS_OK;

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->count = netq_count;
		return VMKNETDDI_QUEUEOPS_OK;

	} else {
		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->type);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int bnx2x_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	/* Only support 1 Mac filter per queue */
	args->count = 1;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_alloc_rx_queue(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				struct napi_struct **napi_p)
{
	struct bnx2x *bp = netdev_priv(netdev);
	int i;

	if (bp->n_rx_queues_allocated >= BNX2X_NUM_NETQUEUES(bp)) {
		BNX2X_ERR("NetQ RX Queue %d >= BNX2X_NUM_NETQUEUES(%d)\n",
			  bp->n_rx_queues_allocated, BNX2X_NUM_NETQUEUES(bp));
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for_each_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		if ((!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) &&
		    BNX2X_IS_NETQ_FP_FEAT_NONE_RESERVED(fp)) {
			fp->netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
			bp->n_rx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(fp->index);
			*napi_p = &fp->napi;

			DP(BNX2X_MSG_NETQ, "RX NetQ allocated on %d\n", i);
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	DP(BNX2X_MSG_NETQ, "No free rx queues found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_alloc_tx_queue(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				u16 *queue_mapping)
{
	struct bnx2x *bp = netdev_priv(netdev);
	int i;

	if (bp->n_tx_queues_allocated >= BNX2X_NUM_NETQUEUES(bp)) {
		BNX2X_ERR("NetQ TX Queue %d >= BNX2X_NUM_NETQUEUES(%d)\n",
			  bp->n_tx_queues_allocated, BNX2X_NUM_NETQUEUES(bp));
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for_each_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		if (!BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp)) {
			fp->netq_flags |= BNX2X_NETQ_TX_QUEUE_ALLOCATED;
			bp->n_tx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(fp->index);
			*queue_mapping = fp->index;

			DP(BNX2X_MSG_NETQ, "TX NetQ allocated on %d\n", i);
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	DP(BNX2X_MSG_NETQ, "No free tx queues found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX)
		return bnx2x_alloc_tx_queue(args->netdev, &args->queueid,
					    &args->queue_mapping);

	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX)
		return bnx2x_alloc_rx_queue(args->netdev, &args->queueid,
					    &args->napi);
	else {
		struct bnx2x *bp = netdev_priv(args->netdev);

		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

#if (VMWARE_ESX_DDK_VERSION >= 41000)

static int
bnx2x_alloc_rx_queue_with_lro(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				struct napi_struct **napi_p)
{
	int i;
	struct bnx2x *bp;

	if (!netdev || !p_qid || !napi_p) {
		printk(KERN_ERR "bnx2x_alloc_rx_queue_with_lro: "
			"Invalid parameters! netdev(%p) p_qid(%p) napi_p(%p)\n",
			netdev, p_qid, napi_p);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	bp = netdev_priv(netdev);
	if (bp->n_rx_queues_allocated >= BNX2X_NUM_QUEUES(bp)) {
		BNX2X_ERR("NetQ RX Queue %d >= BNX2X_NUM_QUEUES(%d)\n",
			  bp->n_rx_queues_allocated, BNX2X_NUM_QUEUES(bp));
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for_each_nondefault_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];
		/* if this Rx queue is not used */
		if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp) &&
				BNX2X_IS_NETQ_FP_FEAT_LRO_RESERVED(fp)) {
			fp->netq_flags |= BNX2X_NETQ_RX_QUEUE_ALLOCATED;
			bp->n_rx_queues_allocated++;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(fp->index);
			*napi_p = &fp->napi;
			 DP(BNX2X_MSG_NETQ,
				 "RX NetQ allocated on %d with LRO feature\n", i);
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}
	DP(BNX2X_MSG_NETQ, "no free rx queues with LRO feature found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

#define BNX2X_NETQ_SUPPORTED_FEATURES \
	(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR | \
	((bp->flags & TPA_ENABLE_FLAG) ? VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO : 0))
/*
 * bnx2x_alloc_queue_with_attr - Alloc queue with NETQ features.
 *
 */
static int
bnx2x_alloc_queue_with_attr(
			vmknetddi_queueop_alloc_queue_with_attr_args_t *args)
{
	int i;
	struct bnx2x *bp = netdev_priv(args->netdev);
	vmknetddi_queueops_queue_features_t feat;

	if (!args->attr || !args->nattr) {
		BNX2X_ERR("Attributes are invalid! attr(%p), nattr(%d).\n",
			args->attr, args->nattr);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	DP(BNX2X_MSG_NETQ, "Attributes number: %d\n", args->nattr);
	for (i = 0; i < args->nattr; i++) {
		DP(BNX2X_MSG_NETQ, "Attribute[%d] type: 0x%x\n",
				i, args->attr[i].type);
		switch (args->attr[i].type) {
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR:
			/* Nothing to do */
			BNX2X_ERR("VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR "
				"isn't supported now.\n");
			break;
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT:
			feat = args->attr[i].args.features;
			DP(BNX2X_MSG_NETQ, "Features 0x%x needed.\n", feat);

			/* Unsupported features */
			if (feat & ~BNX2X_NETQ_SUPPORTED_FEATURES) {
				BNX2X_ERR("Failed... "
					"unsupported feature 0x%x\n",
					feat & ~BNX2X_NETQ_SUPPORTED_FEATURES);
				return VMKNETDDI_QUEUEOPS_ERR;
			}

			if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) {
				if (args->type !=
					    VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
					BNX2X_ERR("Invalid queue type, "
						"LRO feature is only "
						"for RX queue\n");
					break;
				}
				return bnx2x_alloc_rx_queue_with_lro(
						args->netdev, &args->queueid,
						&args->napi);
			}

			/* No feature isn't allowed */
			if (!feat)
				BNX2X_ERR("Invalid feature: "
						"features is NONE!\n");
			break;
		default:
			BNX2X_ERR("Invalid attribute type\n");
			return VMKNETDDI_QUEUEOPS_ERR;
			break;
		}
	}
	BNX2X_ERR("No queue is allocated.\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int
bnx2x_get_supported_feature(vmknetddi_queueop_get_sup_feat_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);

	args->features = BNX2X_NETQ_SUPPORTED_FEATURES;
	DP(BNX2X_MSG_NETQ, "Netq features supported: %s %s %s\n",
	     (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) ? "LRO" : "",
	     (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR) ? "PAIR" : "",
	     (args->features) ? "" : "NONE"
	     );

	return VMKNETDDI_QUEUEOPS_OK;
}

#endif

static int bnx2x_free_tx_queue(struct net_device *netdev,
			       vmknetddi_queueops_queueid_t qid)
{
	struct bnx2x *bp = netdev_priv(netdev);
	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (!bnx2x_netq_valid_qid(bp, index))
		return VMKNETDDI_QUEUEOPS_ERR;

	struct bnx2x_fastpath *fp = &bp->fp[index];

	if (!BNX2X_IS_NETQ_TX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "NetQ TX Queue %d is not allocated\n",
		   index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp->netq_flags &= ~BNX2X_NETQ_TX_QUEUE_ALLOCATED;
	bp->n_tx_queues_allocated--;

	DP(BNX2X_MSG_NETQ, "Free NetQ TX Queue: %x\n", index);

	return VMKNETDDI_QUEUEOPS_OK;
}

static inline void bnx2x_netq_free_rx_queue(struct bnx2x *bp,
					    struct bnx2x_fastpath *fp)
{
	fp->netq_flags &= ~BNX2X_NETQ_RX_QUEUE_ALLOCATED;
	bp->n_rx_queues_allocated--;
}

static int bnx2x_free_rx_queue(struct net_device *netdev,
			       vmknetddi_queueops_queueid_t qid)
{
	struct bnx2x *bp = netdev_priv(netdev);
	u16 index = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (!bnx2x_netq_valid_qid(bp, index)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is invalid\n", index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	struct bnx2x_fastpath *fp = &bp->fp[index];

	if (!BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is not allocated\n",
		   index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	bnx2x_netq_free_rx_queue(bp, fp);
	DP(BNX2X_MSG_NETQ, "Free NetQ RX Queue: %x\n", index);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid))
		return bnx2x_free_tx_queue(args->netdev, args->queueid);

	else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid))
		return bnx2x_free_rx_queue(args->netdev, args->queueid);

	else {
		struct net_device *netdev = args->netdev;
		struct bnx2x *bp = netdev_priv(netdev);

		DP(BNX2X_MSG_NETQ, "invalid queue type: %x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

static int bnx2x_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct bnx2x *bp = netdev_priv(netdev);
	int qid;

	qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	/* may be invoked also for the default queue */
	if (qid > BNX2X_NUM_NETQUEUES(bp)) {
		DP(BNX2X_MSG_NETQ, "NetQ Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/*
	 * msix_table indices:
	 * 0 - default SB (slow-path operations)
	 * 1 - CNIC fast-path operations (if compiled in)
	 * 2 - Max NetQs - Net-queues starting form the default queue
	 */
	qid += (1 + CNIC_CONTEXT_USE);

	args->vector = bp->msix_table[qid].vector;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct bnx2x *bp = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		args->napi = &bp->fp[0].napi;
		return VMKNETDDI_QUEUEOPS_OK;

	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		args->queue_mapping = 0;
		return VMKNETDDI_QUEUEOPS_OK;

	} else
		return VMKNETDDI_QUEUEOPS_ERR;
}


static inline int bnx2x_netq_set_mac_one(u8 *mac, struct bnx2x *bp,
					 struct bnx2x_fastpath *fp, bool add)
{
	unsigned long ramrod_flags = 0;

	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);

	return bnx2x_set_mac_one(bp, mac, &fp->mac_obj, add,
				 BNX2X_NETQ_ETH_MAC, ramrod_flags);
}

static int bnx2x_netq_remove_rx_filter(struct bnx2x *bp,
				       struct bnx2x_fastpath *fp, u16 fid)
{
	struct bnx2x_list_elem *first = NULL;
	unsigned long ramrod_flags = 0;

	DECLARE_MAC_BUF(mac);

	/* Only support one Mac filter per queue */
	if (fid != 0) {
		DP(BNX2X_MSG_NETQ, "Invalid filter id %d\n", fid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* clear MAC */
	first = list_first_entry(&fp->mac_obj.head,
				 struct bnx2x_list_elem, link);
	DP(BNX2X_MSG_NETQ, "remove rx filter %s from queue[%d]\n",
	   print_mac(mac, first->data.mac.mac), fp->index);
	bnx2x_netq_set_mac_one(first->data.mac.mac, bp, fp, 0);

	/* set to drop-all*/
	set_bit(RAMROD_RX, &ramrod_flags);
	set_bit(RAMROD_TX, &ramrod_flags);
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
	bnx2x_set_cl_rx_mode(bp, fp->cl_id, 0, 0, ramrod_flags);

	/* send empty-ramrod to flush packets lurking in the HW */
	fp->netq_filter_event = 1;
	bnx2x_sp_post(bp, RAMROD_CMD_ID_ETH_EMPTY, fp->cid, 0, fp->cl_id, 0);

	/* Wait for completion */
	if(bnx2x_wait_ramrod(bp, 0, fp->index, &fp->netq_filter_event, 0)) {
		/* timeout */
		BNX2X_ERR("Remove RX filter for netq %x failed - "
			  "HW flush timed out\n", fp->index);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp->netq_flags &= ~BNX2X_NETQ_RX_QUEUE_ACTIVE;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	struct bnx2x *bp = netdev_priv(args->netdev);
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	struct bnx2x_fastpath *fp = &bp->fp[qid];
	int rc;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		BNX2X_ERR("Queue ID %d is not RX queue\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* not invoked for the default queue */
	if (!bnx2x_netq_valid_qid(bp, qid)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Verfiy the queue is allocated and has an active filter */
	if (!BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp) || 
	    !BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is not allocated/active "
				   "0x%x\n", qid, fp->netq_flags);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* Do the work */
	rc = bnx2x_netq_remove_rx_filter(bp, fp, fid);

	if (!rc)
		DP(BNX2X_MSG_NETQ, "NetQ remove RX filter: %x\n", qid);

	return rc;
}
	

static int bnx2x_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	u8 *macaddr;
	struct bnx2x *bp = netdev_priv(args->netdev);
	struct bnx2x_fastpath *fp;
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 vlan_id;
	unsigned long accept_flags = 0, ramrod_flags = 0;

	vmknetddi_queueops_filter_class_t filter;
	DECLARE_MAC_BUF(mac);

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		BNX2X_ERR("Queue ID %d is not RX queue\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	filter = vmknetddi_queueops_get_filter_class(&args->filter);
	if (filter != VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		BNX2X_ERR("Queue filter %x not MACADDR filter\n", filter);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* not invoked for the default queue */
	if (!bnx2x_netq_valid_qid(bp, qid)) {
		DP(BNX2X_MSG_NETQ, "NetQ RX Queue %d is invalid\n", qid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	fp = &bp->fp[qid];

	if (BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp) ||
	    !BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp))
		return VMKNETDDI_QUEUEOPS_ERR;

	macaddr = (void *)vmknetddi_queueops_get_filter_macaddr(&args->filter);
	vlan_id = vmknetddi_queueops_get_filter_vlanid(&args->filter);

	/* set to recv-unicast */
	set_bit(BNX2X_ACCEPT_UNICAST, &accept_flags);
	set_bit(BNX2X_ACCEPT_ANY_VLAN, &accept_flags);
	set_bit(RAMROD_RX, &ramrod_flags);
	set_bit(RAMROD_TX, &ramrod_flags);
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
	bnx2x_set_cl_rx_mode(bp, fp->cl_id, 0, accept_flags, ramrod_flags);

	/* add MAC */
	bnx2x_netq_set_mac_one(macaddr, bp, fp, 1);

	fp->netq_flags |= BNX2X_NETQ_RX_QUEUE_ACTIVE;
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(0);
	/* Need by feature: VMKNETDDI_QUEUEOPS_FEATURE_PAIRQUEUE  */
	args->pairtxqid = qid;

	DP(BNX2X_MSG_NETQ, "NetQ set RX filter: %d [%s %d]\n",
	   qid, print_mac(mac, macaddr), vlan_id);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int bnx2x_get_queue_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int bnx2x_get_netqueue_version(vmknetddi_queueop_get_version_args_t *args)
{
	return vmknetddi_queueops_version(args);
}


void bnx2x_netq_clear_rx_queues(struct bnx2x *bp)
{
	int i; 

	spin_lock(&bp->netq_lock);

	for_each_net_queue(bp, i) {
		struct bnx2x_fastpath *fp = &bp->fp[i];

		if (BNX2X_IS_NETQ_RX_QUEUE_ACTIVE(fp))
			bnx2x_netq_remove_rx_filter(bp, fp, 0);

		if (BNX2X_IS_NETQ_RX_QUEUE_ALLOCATED(fp))
			bnx2x_netq_free_rx_queue(bp, fp);
	}

	spin_unlock(&bp->netq_lock);
}

int bnx2x_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	vmknetddi_queueop_get_queue_count_args_t *p;
	struct bnx2x *bp;
	int rc = VMKNETDDI_QUEUEOPS_ERR;

	if (op == VMKNETDDI_QUEUEOPS_OP_GET_VERSION)
		return bnx2x_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
	else if(op == VMKNETDDI_QUEUEOPS_OP_GET_FEATURES)
		return bnx2x_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);

	p = (vmknetddi_queueop_get_queue_count_args_t *)args;
	bp = netdev_priv(p->netdev);

	spin_lock(&bp->netq_lock);

	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		rc = bnx2x_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		rc = bnx2x_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		rc = bnx2x_alloc_queue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		rc = bnx2x_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		rc = bnx2x_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		rc = bnx2x_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		rc = bnx2x_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		rc = bnx2x_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		rc = bnx2x_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		break;

#if (VMWARE_ESX_DDK_VERSION >= 41000)
	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR:
		rc = bnx2x_alloc_queue_with_attr(
			(vmknetddi_queueop_alloc_queue_with_attr_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT:
		rc = bnx2x_get_supported_feature(
			(vmknetddi_queueop_get_sup_feat_args_t *)args);
		break;
#endif

	default:
		printk("Unhandled NETQUEUE OP %d\n", op);
		rc = VMKNETDDI_QUEUEOPS_ERR;
	}

	spin_unlock(&bp->netq_lock);

	return rc;
}

#endif /* BNX2X_NETQ */


/*
 * NPA - Pass-Through
 */
#ifdef BCM_IOV

#include <linux/pci.h>
#include <linux/types.h>
#include <linux/netdevice.h>

#include "bnx2x_vf.h"

#define BNX2X_UNREFERENCED(x)	(void)(x)
#define VMK_RET(rc)		(((rc) == VF_API_SUCCESS) ? VMK_OK : VMK_FAILURE)
#define ENABLE_STR(en)		((en) ? "enable" : "disable")


/* useful wrappers */
inline int
bnx2x_vmk_pci_dev(struct bnx2x *bp, vmk_PCIDevice *vmkPfDev)
{
	struct pci_dev *pdev = bp->pdev;
	return (vmk_PCIGetPCIDevice(pci_domain_nr(pdev->bus),
				    pdev->bus->number,
				    PCI_SLOT(pdev->devfn),
				    PCI_FUNC(pdev->devfn),
				    vmkPfDev) != VMK_OK);
}

inline int
bnx2x_vmk_vf_pci_dev(struct bnx2x *bp, u16 abs_vfid, vmk_PCIDevice *vmkVfDev)
{
	vmk_PCIDevice vmkPfDev;

	if (bnx2x_vmk_pci_dev(bp, &vmkPfDev))
		return -1;

	return (vmk_PCIGetVFPCIDevice(vmkPfDev, abs_vfid, vmkVfDev) != VMK_OK);
}

inline int
bnx2x_vmk_pci_read_config_byte(vmk_PCIDevice dev, vmk_uint16 offset,
			       vmk_uint8 *val)
{
	vmk_uint32 val32;
	VMK_ReturnStatus vmkRet = vmk_PCIReadConfigSpace(dev, VMK_ACCESS_8,
							 offset, &val32);
	*val = (vmk_uint8)val32;
	return vmkRet;

}

inline int
bnx2x_vmk_pci_read_config_word(vmk_PCIDevice dev, vmk_uint16 offset,
			       vmk_uint16 *val)
{
	vmk_uint32 val32;
	VMK_ReturnStatus vmkRet = vmk_PCIReadConfigSpace(dev, VMK_ACCESS_16,
							 offset, &val32);
	*val = (vmk_uint16)val32;
	return vmkRet;

}

inline int
bnx2x_vmk_pci_read_config_dword(vmk_PCIDevice dev, vmk_uint16 offset,
			       vmk_uint32 *val)
{
	return vmk_PCIReadConfigSpace(dev, VMK_ACCESS_32, offset, val);
}

/* Must be called only after VF-Enable*/
inline int
bnx2x_vmk_vf_bus(struct bnx2x *bp, int vfid)
{
	vmk_PCIDevice vfDev;
	u16 seg;
	u8 bus, slot, func;

	bnx2x_vmk_vf_pci_dev(bp, vfid, &vfDev);
	vmk_PCIGetSegmentBusDevFunc(vfDev, &seg, &bus, &slot, &func);
	return bus;
}

/* Must be called only after VF-Enable*/
inline int
bnx2x_vmk_vf_devfn(struct bnx2x *bp, int vfid)
{
	vmk_PCIDevice vfDev;
	u16 seg;
	u8 bus, slot, func;

	bnx2x_vmk_vf_pci_dev(bp, vfid, &vfDev);
	vmk_PCIGetSegmentBusDevFunc(vfDev, &seg, &bus, &slot, &func);
	return PCI_DEVFN(slot, func);
}

void
bnx2x_vmk_vf_set_bars(struct bnx2x *bp, struct bnx2x_virtf *vf)
{
	struct pci_dev *pdev;
	vmk_PCIDevice vfDev;
	u16 seg;
	u8 bus, slot, func;
	int i, n;

	bnx2x_vmk_vf_pci_dev(bp, vf->index, &vfDev);
	vmk_PCIGetSegmentBusDevFunc(vfDev, &seg, &bus, &slot, &func);



#if VMKLNX_DDI_VERSION >= VMKLNX_MAKE_VERSION(9, 2, 0 ,0)
/*#if VMKLNX_DDI_VERSION >= VMKLNX_MAKE_VERSION(10, 0)*/
	pdev = pci_find_slot(seg, bus, PCI_DEVFN(slot, func));
#else
	pdev = pci_find_slot(bus, PCI_DEVFN(slot, func));
#endif

	if (!pdev) {
		printk("Cannot get pci_dev of VF %d.\n", vf->abs_vfid);
		return;
	}

	for (i = 0, n = 0; i < PCI_SRIOV_NUM_BARS; i+=2, n++) {
		vf->bars[n].bar = pci_resource_start(pdev, i);
		vf->bars[n].size = pci_resource_len(pdev, i);
	}
}


#define PCI_SRIOV_CAP_POS_E2 	0x1c0

void
bnx2x_vmk_get_sriov_cap_pos(struct bnx2x *bp, vmk_uint16 *pos)
{
	if (pos)
		/* check for E2 TODO */
		*pos = PCI_SRIOV_CAP_POS_E2;
}


static inline
struct bnx2x_virtf *vf_by_vmkVFID(struct bnx2x *bp, vmk_VFID vmkVf)
{
	return (vmkVf < BNX2X_NR_VIRTFN(bp) ? BP_VF(bp, vmkVf) : NULL);
}


#ifdef BNX2X_PASSTHRU
// ---------------------------------------------------------------------------
//
// VF allocation
//
static int
bnx2x_pt_vf_acquire(struct net_device *netdev, vmk_NetVFRequirements *props,
		  vmk_VFID *vmkVf)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = NULL;	/* make this compile */
	struct vf_pf_resc_request resc;
	int i;

	DP(BNX2X_MSG_IOV, "Features 0x%x, opt-Features 0x%x\n",
	   props->features, props->optFeatures);

	/* verfiy version TODO */
	/* find the first VF which is in either FREE or RESET states */
	for_each_vf(bp, i) {
		vf = BP_VF(bp, i);
		if ((vf->state == VF_FREE) || (vf->state == VF_RESET))
			break;
	}
	if (i == BNX2X_NR_VIRTFN(bp)) {
		BNX2X_ERR("No available VFs\n");
		return VMK_NO_RESOURCES;
	}

	/* acquire the resources */
	resc.num_rxqs = props->numRxQueues;
	resc.num_txqs = props->numTxQueues;
	resc.num_sbs = props->numIntrs;
	resc.num_mac_filters = 1;
	resc.num_mc_filters = 0;
	resc.num_vlan_filters = 0;

	if(bnx2x_vf_acquire(bp, vf, &resc) != VF_API_SUCCESS) {
		BNX2X_ERR("VF[%d] - Failed to provision resources, rxqs=%d, txqs=%d, intr=%d\n",
			  vf->abs_vfid,
			  props->numRxQueues,
			  props->numTxQueues,
			  props->numIntrs);
		return VMK_NO_RESOURCES;
	}

	*vmkVf = i;

	/*
	 * set the identity mapping between queues and SBs
	 * (must be done prior to get_info)
	 */
	for_each_vf_rxq(vf, i) {
		bnx2x_vf_rxq(vf, i, sb_idx) = i;
		if (i < vf->txq_count)
			bnx2x_vf_txq(vf, i, sb_idx) = i;
	}

	/*
	 * save features and MTU to be set on a per queue basis later
	 * during VF init.
	 */
	vf->mtu = props->mtu;
	vf->queue_flags = 0;

	if ((props->features | props->optFeatures) & VMK_NETVF_F_RXVLAN)
		set_bit(BNX2X_QUEUE_FLG_VLAN, &vf->queue_flags);
	if ((props->features | props->optFeatures) & VMK_NETVF_F_LRO)
		set_bit(BNX2X_QUEUE_FLG_TPA, &vf->queue_flags);
	return VMK_OK;
}


static int
bnx2x_pt_vf_init(struct net_device *netdev, vmk_VFID vmkVf,
		 vmk_NetVFParameters *params)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	u64 *sb_map;
	vf_api_t rc;
	int i;

	DP(BNX2X_MSG_IOV, "VF index %d\n", vmkVf);

	/* sanity */
	if (!vf || vf->state != VF_ACQUIRED)
		return VMK_BAD_PARAM;

	/* initialize function */

	/* verify shared region and save plugin stats pointer */
	if (!params->u.npa.sharedRegion || params->u.npa.sharedRegionLength <
	    sizeof(struct bnx2x_vf_plugin_stats))
		return VMK_BAD_PARAM;

	vf->plugin_stats =
		(struct bnx2x_vf_plugin_stats *)params->u.npa.sharedRegion;

	memset((u8*)vf->plugin_stats, 0, sizeof(struct bnx2x_vf_plugin_stats));

	/* set GPA of sbs */
	sb_map = kzalloc(sizeof(*sb_map) * vf->sb_count, GFP_ATOMIC);
	if (!sb_map)
		return VMK_NO_MEMORY;

	for_each_vf_sb(vf, i) {
		sb_map[i] = SB_OFFSET(params->u.npa.txQueues[i].basePA,
				      params->u.npa.txQueues[i].size);
		DP(BNX2X_MSG_IOV, "VF[%d], SB[%d] - map 0x%llx, igu %d, fw %d "
				  "hc %d, mod-level %d\n",
		   vf->abs_vfid, i, sb_map[i],
		   __vf_igu_sb(vf, i),
		   __vf_fw_sb(vf, i),
		   __vf_hc_qzone(vf, i),
		   params->u.npa.intr[i].modLevel);
	}

	rc = bnx2x_vf_init(bp, vf, sb_map);
	kfree(sb_map);

	if (rc != VF_API_SUCCESS)
		goto vf_init_error;

	/* queues */
	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
		struct bnx2x_client_init_params initp = {{0}};
		struct bnx2xpi_txrx_params txrxp = {{0}};

		txrxp.rx.base = params->u.npa.rxQueues[i].basePA;
		txrxp.rx.nr_desc = params->u.npa.rxQueues[i].size;
		txrxp.rx.byte_len = params->u.npa.rxQueues[i].length;

		DP(BNX2X_MSG_IOV, "VF[%d], RXQ[%d] - base 0x%llx, nr_desc %d, len %d\n",
		   vf->abs_vfid,
		   rxq->index,
		   txrxp.rx.base,
		   txrxp.rx.nr_desc,
		   txrxp.rx.byte_len);


		/* first txq - rx queue count >= tx queue count */
		if (i < vf->txq_count) {
			struct bnx2x_vfq *txq = VF_TXQ(vf, i);

			txrxp.tx.base = params->u.npa.txQueues[i].basePA;
			txrxp.tx.nr_desc = params->u.npa.txQueues[i].size;
			txrxp.tx.byte_len = params->u.npa.txQueues[i].length;

			DP(BNX2X_MSG_IOV, "VF[%d], TXQ[%d] - base 0x%llx, nr_desc %d, len %d\n",
			   vf->abs_vfid,
			   txq->index,
			   txrxp.tx.base,
			   txrxp.tx.nr_desc,
			   txrxp.tx.byte_len);

			/* prepare txq init parameters */
			initp.txq_params.flags  = 0;
			initp.txq_params.dscr_map = bnx2xpi_tx_decr_offset(&txrxp);
			initp.txq_params.sb_cq_index = PLUGIN_SB_ETH_TX_CQ_INDEX;
			initp.txq_params.hc_rate = 20000; /* intr p/s default */
			initp.txq_params.traffic_type = LLFC_TRAFFIC_TYPE_NW;
			bnx2x_vf_txq_setup(bp, vf, txq, &initp);
		}

		/* prepare rxq init parameters */
		initp.rxq_params.flags  = vf->queue_flags;
		set_bit(BNX2X_QUEUE_FLG_STATS, &initp.rxq_params.flags);

		initp.rxq_params.dscr_map = bnx2xpi_rx_decr_offset(&txrxp);
		initp.rxq_params.sge_map = bnx2xpi_sge_offset(&txrxp);
		initp.rxq_params.rcq_map = bnx2xpi_rcq_offset(&txrxp);
		initp.rxq_params.rcq_np_map = bnx2xpi_rcq_np_offset(&txrxp);
		initp.rxq_params.mtu = vf->mtu;
		initp.rxq_params.buf_sz = SHELL_SMALL_RECV_BUFFER_SIZE -
			PLUGIN_RX_ALIGN - BNX2X_FW_IP_HDR_ALIGN_PAD;
		initp.rxq_params.cache_line_log = PLUGIN_RX_ALIGN_SHIFT;
		initp.rxq_params.sb_cq_index = PLUGIN_SB_ETH_RX_CQ_INDEX;
		initp.rxq_params.stat_id = vfq_cl_id(vf, rxq);
		initp.rxq_params.hc_rate = 40000; /* intr p/s default */
		if (test_bit(BNX2X_QUEUE_FLG_TPA, &vf->queue_flags)) {
			initp.rxq_params.tpa_agg_sz = min_t(u32, PLUGIN_MAX_SGES * SHELL_LARGE_RECV_BUFFER_SIZE, 0xffff);
			initp.rxq_params.max_sges_pkt = DIV_ROUND_UP(vf->mtu, SHELL_LARGE_RECV_BUFFER_SIZE);
			initp.rxq_params.sge_buf_sz = min_t(u32, SHELL_LARGE_RECV_BUFFER_SIZE, 0xffff);
		}

		DP(BNX2X_MSG_IOV, "VF[%d:%d] offsets - tx 0x%llx, rx 0x%llx, "
				  "rcq 0x%llx, rcq-np 0x%llx, sge 0x%llx\n",
		   vf->abs_vfid,
		   rxq->index,
		   initp.txq_params.dscr_map,
		   initp.rxq_params.dscr_map,
		   initp.rxq_params.rcq_map,
		   initp.rxq_params.rcq_np_map,
		   initp.rxq_params.sge_map);

		DP(BNX2X_MSG_IOV, "VF[%d:%d] rx flags 0x%lx, tx flags 0x%lx\n",
		   vf->abs_vfid, rxq->index,
		   initp.rxq_params.flags, initp.txq_params.flags);

		if (bnx2x_vf_rxq_setup(bp, vf, rxq, &initp, 0))
			goto vf_init_error;

		storm_memset_rcq_np(bp, initp.rxq_params.rcq_np_map,
				    vfq_cl_id(vf, rxq));

		/* add vlan 0 and mark as 'untagged' */
		if (VF_IS_LEADING_RXQ(rxq)) {
			unsigned long ramrod_flags = 0;
			set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
			bnx2x_vf_set_vlan(bp, rxq, 0, true, ramrod_flags);
		}
	}
	return VMK_OK;

vf_init_error:
	/* release the VF */
	bnx2x_vf_release(bp, vf);
	return VMK_FAILURE;

}

static int
bnx2x_pt_vf_release(struct net_device *netdev, vmk_VFID vmkVf)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);

	DP(BNX2X_MSG_IOV, "VF index %d\n", vmkVf);

	/* sanity */
	if (!vf)
		return VMK_BAD_PARAM;

	bnx2x_vf_release(bp, vf);
	return VMK_OK;
}

// ---------------------------------------------------------------------------
//
// VF basic control
//
static int
bnx2x_pt_vf_get_info(struct net_device *netdev, vmk_VFID vmkVf,
		     vmk_NetVFInfo *info)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct bnx2x_plugin_device_info *plugin_data;
	vmk_uint8 numPtRegions;
	int i;

	DP(BNX2X_MSG_IOV, "VF index %d\n", vmkVf);

	/* sanity */
	if (!vf || vf->state != VF_ACQUIRED)
		return VMK_BAD_PARAM;

	/* build sbdf */
	info->sbdf = (vf->devfn) | ((vf->bus & 0xff) << 8) |
		((pci_domain_nr(bp->pdev->bus) & 0xffff) << 16);

	/* copy bar info */
	for (i = 0, numPtRegions = 0; i < 3; i++) {
		if(vf->bars[i].size) {
			info->ptRegions[numPtRegions].MA = vf->bars[i].bar;
			info->ptRegions[numPtRegions++].numPages =
				vf->bars[i].size / 4096;
		}
	}
	info->numPtRegions = numPtRegions;

	DP(BNX2X_MSG_IOV, "sbdf %08x (%04x:%02x:%02x.%01x), no pages: %d ",
	   info->sbdf,
	   (info->sbdf >> 16) & 0xffff,
	   (info->sbdf >> 8) & 0xff,
	   PCI_SLOT(info->sbdf &0xff),
	   PCI_FUNC(info->sbdf &0xff),
	   info->numPtRegions);

	for (i = 0; i < numPtRegions; i++)
		DP_CONT(BNX2X_MSG_IOV, " [ 0x%llx, %d ]" ,
			(unsigned long long int)info->ptRegions[i].MA,
			info->ptRegions[i].numPages);
	DP_CONT(BNX2X_MSG_IOV, "\n");

	/* plugin information */
	strcpy(info->u.npa.pluginName, "bnx2x_plugin-%b.so");

	/* build plugin private data  */
	memset(info->u.npa.pluginData, 0, sizeof (info->u.npa.pluginData));
	plugin_data = (struct bnx2x_plugin_device_info *)(info->u.npa.pluginData);

	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);
		plugin_data->sb_info[i].sb_qid = vfq_qzone_id(vf, rxq);
		plugin_data->sb_info[i].hw_sb_id = vfq_igu_sb_id(vf ,rxq);
		plugin_data->hw_qid[i] = vfq_qzone_id(vf, rxq);
	}
	plugin_data->chip_id = bp->common.chip_id;
	plugin_data->pci_func = bp->pf_num;

	plugin_data->fp_flags = BNX2X_PLUGIN_SHARED_SB_IDX;

	plugin_data->fp_flags |= test_bit(BNX2X_QUEUE_FLG_TPA, &vf->queue_flags) ?
		0 : BNX2X_PLUGIN_DISABLE_TPA;

	return VMK_OK;
}

static int
bnx2x_pt_vf_quiesce(struct net_device *netdev, vmk_VFID vmkVf)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);

	DP(BNX2X_MSG_IOV, "Invoked\n");

	/* sanity */
	if (!vf)
		return VMK_BAD_PARAM;

	/* quiesxce only if vf is enabled otherwise exit gracefully */
	if (vf->state != VF_ENABLED)
		return VMK_OK;

	return (VMK_RET(bnx2x_vf_trigger(bp, vf, false)));
}

static int
bnx2x_pt_vf_activate(struct net_device *netdev, vmk_VFID vmkVf)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);

	DP(BNX2X_MSG_IOV, "Invoked\n");

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	return (VMK_RET(bnx2x_vf_trigger(bp, vf, true)));
}

static int
bnx2x_pt_vf_set_mac(struct net_device *netdev, vmk_VFID vmkVf,
		    vmk_EthAddress mac)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct bnx2x_vfq *rxq = NULL;
	unsigned long ramrod_flags = 0;
	vf_api_t rc;

	DECLARE_MAC_BUF(mac_buf);
	DP(BNX2X_MSG_IOV, "%s\n", print_mac(mac_buf, mac));

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	rxq = VF_LEADING_RXQ(vf);

	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
	rc = bnx2x_vf_set_mac(bp, rxq, mac, 1, ramrod_flags);

	if (rc) {
		BNX2X_ERR("Failed to add mac-vlan rule rc=%d\n", rc);
		return VMK_FAILURE;
	}
	return VMK_OK;
}

// ---------------------------------------------------------------------------
//
// Multicast control
//
static int
bnx2x_pt_vf_set_multicast(struct net_device *netdev, vmk_VFID vmkVf,
			  size_t nmac, vmk_EthAddress *mac)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	vf_api_t rc;

	DP(BNX2X_MSG_IOV, "VF index %d, mcast_num %d\n", vmkVf, (int)nmac);

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	if (nmac > VMK_NPA_MAX_MULTICAST_FILTERS)
		return VMK_BAD_PARAM;

	rc = bnx2x_vf_set_mcasts(bp, vf ,(bnx2x_mac_addr_t *)mac, nmac, false);

	return VMK_RET(rc);
}

static int
bnx2x_pt_vf_set_rx_mode(struct net_device *netdev, vmk_VFID vmkVf,
			uint8_t unicast,
			uint8_t multicast,
			uint8_t broadcast,
			uint8_t allmulti)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct bnx2x_vfq *rxq = NULL;
	vf_api_t rc = VF_API_SUCCESS;
	unsigned long accept_flags = 0;

	DP(BNX2X_MSG_IOV, "VF index %d, ucast %d, mcast %d, bcast %d, "
			  "allmulti %d\n",
	   vmkVf, unicast, multicast, broadcast, allmulti);

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	rxq = VF_LEADING_RXQ(vf);

	/* build accept flags */
	if (unicast)
		set_bit(BNX2X_ACCEPT_UNICAST, &accept_flags);
	if (multicast)
		set_bit(BNX2X_ACCEPT_MULTICAST, &accept_flags);
	if (broadcast)
		set_bit(BNX2X_ACCEPT_BROADCAST, &accept_flags);
	if (allmulti)
		set_bit(BNX2X_ACCEPT_ALL_MULTICAST, &accept_flags);

	DP(BNX2X_MSG_IOV, "RX-MASK=0x%lx\n", accept_flags);

	rc = bnx2x_vf_set_rxq_mode(bp, vf, rxq, accept_flags);
	return VMK_RET(rc);
}

// ---------------------------------------------------------------------------
//
// VLAN control
//
static int
bnx2x_pt_vf_add_vlan_range(struct net_device *netdev, vmk_VFID vmkVf, vmk_VlanID first,
			 vmk_VlanID last)
{
	vmk_VlanID vlan_id;
	int vlan_cnt;
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct bnx2x_vfq *rxq = NULL;
	unsigned long ramrod_flags = 0;
	vf_api_t rc = VF_API_SUCCESS;

	DP(BNX2X_MSG_IOV, "VF index %d, first %d, last %d\n",
	   vmkVf, first, last);

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	/* a valid range does not include vlan 0 - igonre first == 0 */
	if (first > last || first > 4095 || last > 4095)
		return VMK_BAD_PARAM;

	rxq = VF_LEADING_RXQ(vf);

	/* set common ramrod flags used for all add/del ops below */
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);

	/* count how many vlans need to be added */
	for (vlan_id = first, vlan_cnt = 0; vlan_id <= last; vlan_id++) {
		/* ignore vlan 0*/
		if (!vlan_id)
			continue;
		if (!bnx2x_vf_check_vlan_op(bp, rxq, vlan_id, true))
			continue;
		vlan_cnt++;
	}
	if ((atomic_read(&rxq->vlan_count) + vlan_cnt) > vf->vlan_rules_count) {
		BNX2X_ERR("Add VLAN range: invalid range %d-%d count %d\n",
			  first, last, vlan_cnt);
		return VMK_BAD_PARAM;
	}

	/* add the range */
	for (vlan_id = first, vlan_cnt = 0; vlan_id <= last; vlan_id++) {
		/* ignore vlan 0*/
		if (!vlan_id)
			continue;
		if (!bnx2x_vf_check_vlan_op(bp, rxq, vlan_id, true))
			continue;
		rc = bnx2x_vf_set_vlan(bp, rxq, vlan_id, true, ramrod_flags);
		if (rc) {
			BNX2X_ERR("Add VLAN range: failed to delete "
				  "vlan %d\n", vlan_id);
			break;
		}
		vlan_cnt++;
	}
	if (!rc && !atomic_read(&rxq->vlan_count) && vlan_cnt) {
		/* delete vlan 0*/
		rc = bnx2x_vf_set_vlan(bp, rxq, 0, false, ramrod_flags);
	}
	atomic_add(vlan_cnt, &rxq->vlan_count);

	return VMK_RET(rc);
}

static int
bnx2x_pt_vf_del_vlan_range(struct net_device *netdev, vmk_VFID vmkVf,
			 vmk_VlanID first, vmk_VlanID last)
{
	vmk_VlanID vlan_id;
	int vlan_cnt;
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct bnx2x_vfq *rxq = NULL;
	unsigned long ramrod_flags = 0;
	vf_api_t rc = VF_API_SUCCESS;

	DP(BNX2X_MSG_IOV, "VF index %d, first %d, last %d\n",
	   vmkVf, first, last);

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	/* a valid range does not include vlan 0 - igonre first == 0 */
	if (first > last || first > 4095 || last > 4095)
		return VMK_BAD_PARAM;

	rxq = VF_LEADING_RXQ(vf);

	/* set common ramrod flags used for all add/del ops below */
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);

	/* check that the range is valid and caclculate the range size */
	for (vlan_id = first, vlan_cnt = 0; vlan_id <= last; vlan_id++) {
		/* ignore vlan 0*/
		if (!vlan_id)
			continue;
		if (!bnx2x_vf_check_vlan_op(bp, rxq, vlan_id, false))
			break;
		vlan_cnt++;
	}
	if (vlan_id <= last) {
		BNX2X_ERR("Delete VLAN range: invalid range %d-%d\n",
			  first, last);
		return VMK_BAD_PARAM;
	}
	if (!vlan_cnt)
		/* nothing to do */
		return VMK_OK;

	if (vlan_cnt == atomic_read(&rxq->vlan_count))
		/* add vlan 0 before deleting the range */
		rc = bnx2x_vf_set_vlan(bp, rxq, 0, true, ramrod_flags);

	if (rc) {
		BNX2X_ERR("Delete VLAN range: failed to add vlan 0\n");
		return VMK_FAILURE;
	}

	/* delete the range */
	for (vlan_id = first, vlan_cnt = 0; vlan_id <= last; vlan_id++) {
		/* ignore vlan 0*/
		if (!vlan_id)
			continue;

		rc = bnx2x_vf_set_vlan(bp, rxq, vlan_id, false, ramrod_flags);
		if (rc) {
			BNX2X_ERR("Delete VLAN range: failed to delete "
				  "vlan %d\n", vlan_id);
			break;
		}
		vlan_cnt++;
	}
	atomic_sub(vlan_cnt, &rxq->vlan_count);
	return VMK_RET(rc);
}

static int
bnx2x_default_vlan_ramrod(struct bnx2x *bp, struct bnx2x_virtf *vf, bool enable,
			  u16 vid, u8 prio)
{
	struct client_update_ramrod_data *client_update;
	int i;

	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);

		/* turn anti-spoffing on or off on each tx queue */
		if (i >= vf->txq_count)
			break;

		client_update = BP_VF_SP(bp, vf, client_update);
		memset(client_update, 0, sizeof(*client_update));

		client_update->client_id = vfq_cl_id(vf, rxq);
		client_update->default_vlan = cpu_to_le16(vid | (prio << 12));
		client_update->default_vlan_enable_flg = enable ? 1 : 0;
		client_update->anti_spoofing_change_flg = 1;
		client_update->inner_vlan_removal_change_flg = 1;
		client_update->inner_vlan_removal_enable_flg = enable ? 1 : 0;

		if (bnx2x_vf_queue_update_ramrod(bp, rxq,
			BP_VF_SP_MAP(bp, vf, client_update), true)) /* block */
			return VMK_FAILURE;
	}
	return VMK_OK;
}




static int
bnx2x_pt_vf_set_default_vlan(struct net_device *netdev, vmk_VFID vmkVf,
			   uint8_t enable, vmk_VlanID vid,
			   vmk_VlanPriority prio)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct bnx2x_vfq *rxq = NULL;
	unsigned long ramrod_flags = 0;
	vf_api_t rc = VF_API_SUCCESS;

	DP(BNX2X_MSG_IOV, "VF index %d, vid %d, pri %d, %s\n",
	   vmkVf, vid, prio, ENABLE_STR(enable));

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	if ((enable && vid == 0 && prio == 0) || vid > 4095 || prio > 7)
		return VMK_BAD_PARAM;

	rxq = VF_LEADING_RXQ(vf);

	/* set common ramrod flags used for all add/del ops below */
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);


	if (!enable) {
		/* disable */
		if (!vf->def_vlan_enabled)
			/* nothing to do */
			return VMK_OK;

		/* turn off default vlan */
		bnx2x_default_vlan_ramrod(bp, vf ,false, vid, prio);

		/* reset vlan classification - add vlan 0 and remove the
		 * default vlan (remove all vlans except 0)
		 */
		rc = bnx2x_vf_set_vlan(bp, rxq, 0, true, ramrod_flags);
		if (!rc)
			rc = bnx2x_vf_clear_vlans(bp, rxq, true, 0,
						  ramrod_flags);
		vf->def_vlan_enabled = false;
	}
	else {
		/* enable */
		if (vf->def_vlan_enabled)
			/* nothing to do */
			return VMK_OK;

		/* set vlan classifiaction - add default valn and
		 * remove all the rest
		 */
		rc = bnx2x_vf_set_vlan(bp, rxq, vid, true, ramrod_flags);
		if (!rc)
			rc = bnx2x_vf_clear_vlans(bp, rxq, true, vid,
						  ramrod_flags);

		/* turn on default vlan */
		bnx2x_default_vlan_ramrod(bp, vf ,true, vid, prio);

		vf->def_vlan_enabled = true;
	}
	return VMK_RET(rc);
}

// ---------------------------------------------------------------------------
//
// Misc operations
//
static int
bnx2x_pt_vf_set_antispoof(struct net_device *netdev, vmk_VFID vmkVf,
			  uint8_t enable)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);
	struct client_update_ramrod_data *client_update;
	int i;

	DP(BNX2X_MSG_IOV, "VF index %d, %s\n", vmkVf, ENABLE_STR(enable));

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	for_each_vf_rxq(vf, i) {
		struct bnx2x_vfq *rxq = VF_RXQ(vf, i);

		/* turn anti-spoffing on or off on each tx queue */
		if (i >= vf->txq_count)
			break;

		client_update = BP_VF_SP(bp, vf, client_update);
		memset(client_update, 0, sizeof(*client_update));

		client_update->client_id = vfq_cl_id(vf, rxq);
		client_update->anti_spoofing_enable_flg = enable ? 1 : 0;
		client_update->anti_spoofing_change_flg = 1;

		if (bnx2x_vf_queue_update_ramrod(bp, rxq,
			BP_VF_SP_MAP(bp, vf, client_update), true)) /* block */
			return VMK_FAILURE;
	}
	return VMK_OK;
}

static int
bnx2x_pt_vf_set_rss_ind_table(struct net_device *netdev, vmk_VFID vmkVf,
			    vmk_NetVFRSSIndTable *table)
{
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);


	DP(BNX2X_MSG_IOV, "VF index %d\n", vmkVf);

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	// Check table size.
	if (table->indTableSize > VMK_NETVF_RSS_MAX_IND_TABLE_SIZE)
		return VMK_BAD_PARAM;

	return VMK_OK;
}

static int
bnx2x_pt_vf_get_queue_status(struct net_device *netdev, vmk_VFID vmkVf,
			   uint8_t numTxQueues, uint8_t numRxQueues,
			   vmk_NetVFQueueStatus *tqStatus,
			   vmk_NetVFQueueStatus *rqStatus)
{
	struct bnx2x *bp = netdev_priv(netdev);
	DP(BNX2X_MSG_IOV, "VF index %d - NOT SUPPORTED \n", vmkVf);
	return VMK_OK;
}


static inline
vmk_uint64 __pt_update_single_stat(vmk_uint64 cur, vmk_uint64 *p_old)
{
	vmk_uint64 diff = (s64)cur - (s64)(*p_old);
	*p_old = cur;
	return diff;
}

static int
bnx2x_pt_vf_get_queue_stats(struct net_device *netdev, vmk_VFID vmkVf,
			  uint8_t numTxQueues, uint8_t numRxQueues,
			  vmk_NetVFTXQueueStats *tqStats,
			  vmk_NetVFRXQueueStats *rqStats)
{
	int i;
	struct bnx2x *bp = netdev_priv(netdev);
	struct bnx2x_virtf *vf = vf_by_vmkVFID(bp, vmkVf);

	/* sanity */
	if (!vf || vf->state != VF_ENABLED)
		return VMK_BAD_PARAM;

	if (vf->rxq_count < numRxQueues || vf->txq_count < numTxQueues)
		return VMK_BAD_PARAM;

	// First clear output structures.
	memset(tqStats, 0, numTxQueues * sizeof (vmk_NetVFTXQueueStats));
	memset(rqStats, 0, numRxQueues * sizeof (vmk_NetVFRXQueueStats));

	for (i = 0; i < numRxQueues; i++) {
		struct bnx2x_vfq_fw_stats *qstats = &vf->vfq_stats[i].qstats;
		vmk_NetVFRXQueueStats *old_vmk_rxq = &vf->vfq_stats[i].old_vmk_rxq;

		rqStats[i].unicastPkts = __pt_update_single_stat(
			HILO_U64(qstats->total_unicast_packets_received_hi,
				 qstats->total_unicast_packets_received_lo),
			&old_vmk_rxq->unicastPkts);

		rqStats[i].unicastBytes = __pt_update_single_stat(
			HILO_U64(qstats->total_unicast_bytes_received_hi,
				 qstats->total_unicast_bytes_received_lo),
			&old_vmk_rxq->unicastBytes);

		rqStats[i].multicastPkts = __pt_update_single_stat(
			HILO_U64(qstats->total_multicast_packets_received_hi,
				 qstats->total_multicast_packets_received_lo),
			&old_vmk_rxq->multicastPkts);

		rqStats[i].multicastBytes = __pt_update_single_stat(
			HILO_U64(qstats->total_multicast_bytes_received_hi,
				 qstats->total_multicast_bytes_received_lo),
			&old_vmk_rxq->multicastBytes);

		rqStats[i].broadcastPkts = __pt_update_single_stat(
			HILO_U64(qstats->total_broadcast_packets_received_hi,
				 qstats->total_broadcast_packets_received_lo),
			&old_vmk_rxq->broadcastPkts);

		rqStats[i].broadcastBytes = __pt_update_single_stat(
			HILO_U64(qstats->total_broadcast_bytes_received_hi,
				 qstats->total_broadcast_bytes_received_lo),
			&old_vmk_rxq->broadcastBytes);

		rqStats[i].outOfBufferDrops = __pt_update_single_stat(
			HILO_U64(qstats->no_buff_discard_hi,
				 qstats->no_buff_discard_lo),
			&old_vmk_rxq->outOfBufferDrops);

		rqStats[i].errorDrops = __pt_update_single_stat(
			HILO_U64(qstats->etherstatsoverrsizepkts_hi,
				 qstats->etherstatsoverrsizepkts_lo) +
			HILO_U64(qstats->error_discard_hi,
				 qstats->error_discard_lo),
			&old_vmk_rxq->errorDrops);

		rqStats[i].LROPkts = __pt_update_single_stat(
			vf->plugin_stats->rxq_stats[i].lro_pkts,
			&old_vmk_rxq->LROPkts);

		rqStats[i].LROBytes = __pt_update_single_stat(
			vf->plugin_stats->rxq_stats[i].lro_bytes,
			&old_vmk_rxq->LROBytes);
	}
	for (i = 0; i < numTxQueues; i++) {
		struct bnx2x_vfq_fw_stats *qstats = &vf->vfq_stats[i].qstats;
		vmk_NetVFTXQueueStats *old_vmk_txq = &vf->vfq_stats[i].old_vmk_txq;

		tqStats[i].unicastPkts = __pt_update_single_stat(
			HILO_U64(qstats->total_unicast_packets_sent_hi,
				 qstats->total_unicast_packets_sent_lo),
			&old_vmk_txq->unicastPkts);

		tqStats[i].unicastBytes = __pt_update_single_stat(
			HILO_U64(qstats->total_unicast_bytes_sent_hi,
				 qstats->total_unicast_bytes_sent_lo),
			&old_vmk_txq->unicastBytes);

		tqStats[i].multicastPkts = __pt_update_single_stat(
			HILO_U64(qstats->total_multicast_packets_sent_hi,
				 qstats->total_multicast_packets_sent_lo),
			&old_vmk_txq->multicastPkts);

		tqStats[i].multicastBytes = __pt_update_single_stat(
			HILO_U64(qstats->total_multicast_bytes_sent_hi,
				 qstats->total_multicast_bytes_sent_lo),
			&old_vmk_txq->multicastBytes);

		tqStats[i].broadcastPkts = __pt_update_single_stat(
			HILO_U64(qstats->total_broadcast_packets_sent_hi,
				 qstats->total_broadcast_packets_sent_lo),
			&old_vmk_txq->broadcastPkts);

		tqStats[i].broadcastBytes = __pt_update_single_stat(
			HILO_U64(qstats->total_broadcast_bytes_sent_hi,
				 qstats->total_broadcast_bytes_sent_lo),
			&old_vmk_txq->broadcastBytes);

		tqStats[i].errors = __pt_update_single_stat(
			HILO_U64(qstats->tx_error_packets_hi,
				 qstats->tx_error_packets_lo),
			&old_vmk_txq->errors);

		tqStats[i].TSOPkts = __pt_update_single_stat(
			vf->plugin_stats->txq_stats[i].tso_pkts,
			&old_vmk_txq->TSOPkts);

		tqStats[i].TSOBytes = __pt_update_single_stat(
			vf->plugin_stats->txq_stats[i].tso_bytes,
			&old_vmk_txq->TSOBytes);

		tqStats[i].discards = 0;
	}
	return VMK_OK;
}

// ---------------------------------------------------------------------------
//
// Non-passthrough vNIC control
//
static int
bnx2x_pt_pf_add_mac_filter(struct net_device *netdev, vmk_EthAddress mac)
{
	struct bnx2x *bp = netdev_priv(netdev);

	DECLARE_MAC_BUF(mac_buf);
	DP(BNX2X_MSG_IOV, "%s\n", print_mac(mac_buf, mac));

	bnx2x_iov_set_tx_mac(bp, mac, 1);
	return VMK_OK;
}

static int
bnx2x_pt_pf_del_mac_filter(struct net_device *netdev, vmk_EthAddress mac)
{
	struct bnx2x *bp = netdev_priv(netdev);

	DECLARE_MAC_BUF(mac_buf);
	DP(BNX2X_MSG_IOV, "%s\n", print_mac(mac_buf, mac));

	bnx2x_iov_set_tx_mac(bp, mac, 0);
	return VMK_OK;
}

static int
bnx2x_pt_pf_mirror_all(struct net_device *netdev, uint8_t enable)
{
	struct bnx2x *bp = netdev_priv(netdev);
	unsigned long accept_flags = 0, ramrod_flags = 0;

	DP(BNX2X_MSG_IOV, "%s\n", ENABLE_STR(enable));

	if (!enable) {
		/* restore rx_mode */
		bnx2x_set_rx_mode(bp->dev);
		return VMK_OK;
	}
	if (test_bit(BNX2X_FILTER_RX_MODE_PENDING, &bp->sp_state))
		/* no support for concurrent rx_mode requests */
		return VMK_BUSY;

	set_bit(BNX2X_ACCEPT_ALL_UNICAST, &accept_flags);
	set_bit(BNX2X_ACCEPT_ALL_MULTICAST, &accept_flags);
	set_bit(BNX2X_ACCEPT_BROADCAST, &accept_flags);
	set_bit(RAMROD_RX, &ramrod_flags);
	set_bit(RAMROD_COMP_WAIT, &ramrod_flags);
	bnx2x_set_cl_rx_mode(bp, bp->fp->cl_id, 0, accept_flags, ramrod_flags);
	return VMK_OK;
}

// ---------------------------------------------------------------------------
//
// Dispatch & logging operations
//
static VMK_ReturnStatus
bnx2x_pt_passthru_ops2(struct net_device *netdev, vmk_NetPTOP op, void *pargs)
{
	switch (op) {
	case VMK_NETPTOP_VF_ACQUIRE:
	{
		vmk_NetPTOPVFAcquireArgs *args = pargs;
		return bnx2x_pt_vf_acquire(netdev, &args->props, &args->vf);
	}
	case VMK_NETPTOP_VF_INIT:
	{
		vmk_NetPTOPVFInitArgs *args = pargs;
		return bnx2x_pt_vf_init(netdev, args->vf, &args->params);
	}
	case VMK_NETPTOP_VF_ACTIVATE:
	{
		vmk_NetPTOPVFSimpleArgs *args = pargs;
		return bnx2x_pt_vf_activate(netdev, args->vf);
	}
	case VMK_NETPTOP_VF_QUIESCE:
	{
		vmk_NetPTOPVFSimpleArgs *args = pargs;
		return bnx2x_pt_vf_quiesce(netdev, args->vf);
	}
	case VMK_NETPTOP_VF_RELEASE:
	{
		vmk_NetPTOPVFSimpleArgs *args = pargs;
		return bnx2x_pt_vf_release(netdev, args->vf);
	}
	case VMK_NETPTOP_VF_GET_INFO:
	{
		vmk_NetPTOPVFGetInfoArgs *args = pargs;
		return bnx2x_pt_vf_get_info(netdev, args->vf, &args->info);
	}
	case VMK_NETPTOP_VF_SET_RSS_IND_TABLE:
	{
		vmk_NetPTOPVFSetRSSIndTableArgs *args = pargs;
		return bnx2x_pt_vf_set_rss_ind_table(netdev, args->vf,
						     &args->table);
	}
	case VMK_NETPTOP_VF_GET_QUEUE_STATS:
	{
		vmk_NetPTOPVFGetQueueStatsArgs *args = pargs;
		return bnx2x_pt_vf_get_queue_stats(netdev, args->vf,
						   args->numTxQueues,
						   args->numRxQueues,
						   args->tqStats,
						   args->rqStats);
	}
	case VMK_NETPTOP_VF_GET_QUEUE_STATUS:
	{
		vmk_NetPTOPVFGetQueueStatusArgs *args = pargs;
		return bnx2x_pt_vf_get_queue_status(netdev,
						    args->vf,
						    args->numTxQueues,
						    args->numRxQueues,
						    args->tqStatus,
						    args->rqStatus);
	}
	case VMK_NETPTOP_VF_SET_MAC:
	{
		vmk_NetPTOPVFSetMacArgs *args = pargs;
		return bnx2x_pt_vf_set_mac(netdev, args->vf, args->mac);
	}
	case VMK_NETPTOP_VF_SET_MULTICAST:
	{
		vmk_NetPTOPVFSetMulticastArgs *args = pargs;
		return bnx2x_pt_vf_set_multicast(netdev,
						 args->vf,
						 args->nmac,
						 args->macList);
	}
	case VMK_NETPTOP_VF_SET_RX_MODE:
	{
		vmk_NetPTOPVFSetRxModeArgs *args = pargs;
		// promiscuous is not supported by NPA
		return bnx2x_pt_vf_set_rx_mode(netdev,
					       args->vf,
					       args->unicast,
					       args->multicast,
					       args->broadcast,
					       args->allmulti);
	}
	case VMK_NETPTOP_VF_ADD_VLAN_RANGE:
	{
		vmk_NetPTOPVFVlanRangeArgs *args = pargs;
		return bnx2x_pt_vf_add_vlan_range(netdev,
						  args->vf,
						  args->first,
						  args->last);
	}
	case VMK_NETPTOP_VF_DEL_VLAN_RANGE:
	{
		vmk_NetPTOPVFVlanRangeArgs *args = pargs;
		return bnx2x_pt_vf_del_vlan_range(netdev,
						  args->vf,
						  args->first,
						  args->last);
	}
	case VMK_NETPTOP_VF_SET_DEFAULT_VLAN:
	{
		vmk_NetPTOPVFSetDefaultVlanArgs *args = pargs;
		return bnx2x_pt_vf_set_default_vlan(netdev,
						    args->vf,
						    args->enable,
						    args->vid,
						    args->prio);
	}
	case VMK_NETPTOP_VF_SET_ANTISPOOF:
	{
		vmk_NetPTOPVFSetAntispoofArgs *args = pargs;
		return bnx2x_pt_vf_set_antispoof(netdev,
						 args->vf,
						 args->enable);
	}
	case VMK_NETPTOP_PF_ADD_MAC_FILTER:
	{
		vmk_NetPTOPPFMacFilterArgs *args = pargs;
		return bnx2x_pt_pf_add_mac_filter(netdev, args->mac);
	}
	case VMK_NETPTOP_PF_DEL_MAC_FILTER:
	{
		vmk_NetPTOPPFMacFilterArgs *args = pargs;
		return bnx2x_pt_pf_del_mac_filter(netdev, args->mac);
	}
	case VMK_NETPTOP_PF_MIRROR_ALL:
	{
		vmk_NetPTOPPFMirrorAllArgs *args = pargs;
		return bnx2x_pt_pf_mirror_all(netdev, args->enable);
	}
	default:
		printk("Unhandled NPA OP %d\n", op);
		return VMK_FAILURE;
	}
}

/*
 * This wrapper is here to dump registers before/after doing an
 * operation.
 */
VMK_ReturnStatus
bnx2x_pt_passthru_ops(void *client_data, vmk_NetPTOP op, void *args)
{
	VMK_ReturnStatus ret;

	// Do the job
	ret = bnx2x_pt_passthru_ops2(client_data, op, args);

	if (ret != VMK_OK) {
		printk("PT OP %d failed, ret=%X\n", op, ret);
	}
	return ret;
}

#endif /* BNX2X_PASSTHRU */
#endif /* BCM_IOV */
