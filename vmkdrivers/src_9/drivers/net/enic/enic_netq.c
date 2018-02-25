/*
 * Copyright 2013 Cisco Systems, Inc.  All rights reserved.
 */

#include <linux/tcp.h>
#include "kcompat.h"
#include "enic.h"
#include "enic_netq.h"
#include "enic_clsf.h"

int  enic_netq_enabled(struct enic *enic)
{
	return (enic->priv_flags & ENIC_NETQ_ENABLED) ? 1 : 0;
}

static int enic_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = (VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES |
			  VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct enic *enic = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->count = enic->wq_count - 1;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = enic->rq_count - 1;
	} else {
		netdev_err(enic->netdev, "Queue count :Invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	args->count = 1;	/* number of mac filters per queue */

	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_alloc_rx_queue(struct net_device *netdev,
	vmknetddi_queueops_queueid_t *p_qid, struct napi_struct **napi_p)
{
	struct enic *enic = netdev_priv(netdev);
	int i;

	if (enic->netq_allocated_rqs >= enic->rq_count){
		netdev_err(enic->netdev, "No free RQ \n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	/* we don't give out rq[0], the default queue */
	for (i = 1; i < enic->rq_count; i++) {
		if (!ring_is_allocated(enic->rq[i])) {
			set_ring_allocated(enic->rq[i]);
			enic->netq_allocated_rqs++;
	 		*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(i);
			*napi_p = &enic->napi[i];
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}

        netdev_err(enic->netdev, "No free RQ found\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int enic_alloc_tx_queue(struct net_device *netdev,
	vmknetddi_queueops_queueid_t *p_qid, u16 *queue_mapping)
{
	struct enic *enic = netdev_priv(netdev);
	int i;


	if (enic->netq_allocated_wqs >= enic->wq_count) {
		netdev_err(enic->netdev, "No free WQ \n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* we don't give out wq[0], the default queue */
	for (i = 1; i < enic->wq_count; i++) {
		if (!ring_is_allocated(enic->wq[i]))
		{
			set_ring_allocated(enic->wq[i]);
			*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(i);
			*queue_mapping = i;
			enic->netq_allocated_wqs++;
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}

	netdev_err(enic->netdev, "No free WQ \n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int enic_alloc_netqueue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	struct enic *enic = netdev_priv(args->netdev);
	int ret;
	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		spin_lock(&enic->netq_wq_lock);
		ret =  enic_alloc_tx_queue(args->netdev, &args->queueid,
				&args->queue_mapping);
		spin_unlock(&enic->netq_wq_lock);
		
		return ret;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		spin_lock(&enic->netq_rq_lock);
		ret =  enic_alloc_rx_queue(args->netdev, &args->queueid,
				&args->napi);
		spin_unlock(&enic->netq_rq_lock);
		
		return ret;
	}

	netdev_err(enic->netdev, "Allocate NetQueue :Invalid queue type\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int enic_free_rx_queue(struct net_device *netdev,
	vmknetddi_queueops_queueid_t qid)
{
	struct enic *enic = netdev_priv(netdev);
	u16 rq_id = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	
	if (rq_id >= enic->rq_count) {
		netdev_err(enic->netdev, "RQ id %u is too big, >= %d\n", 
			rq_id, enic->rq_count);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!ring_is_allocated(enic->rq[rq_id])) {
		netdev_err(enic->netdev, "RQ[%u] not allocated\n", rq_id);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	spin_lock(&enic->netq_rq_lock);
	clear_ring_allocated(enic->rq[rq_id]);
	if (enic->netq_allocated_rqs)
		enic->netq_allocated_rqs--;
	spin_unlock(&enic->netq_rq_lock);
	
	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_free_tx_queue(struct net_device *netdev,
	vmknetddi_queueops_queueid_t qid)
{
	struct enic *enic = netdev_priv(netdev);
	u16 wq_id = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (wq_id >= enic->wq_count) {
		netdev_err(enic->netdev, "Queue id %u is too big, >= %d\n",
			wq_id, enic->wq_count);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!ring_is_allocated(enic->wq[wq_id])) {
		netdev_err(enic->netdev, "WQ[%u] not allocated\n", wq_id);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	
	spin_lock(&enic->netq_wq_lock);
	clear_ring_allocated(enic->wq[wq_id]);
	if (enic->netq_allocated_wqs)
		enic->netq_allocated_wqs--;
	spin_unlock(&enic->netq_wq_lock);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	struct enic *enic = netdev_priv(args->netdev);
	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		return enic_free_tx_queue(args->netdev, args->queueid);
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		return enic_free_rx_queue(args->netdev, args->queueid);
	}

	netdev_err(enic->netdev, "Free Queue :Invalid queue type\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int enic_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct enic *enic = netdev_priv(netdev);
	u16 qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	unsigned int intr;

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)){
		intr = enic_msix_wq_intr(enic, qid);
		args->vector = enic->msix_entry[intr].vector;
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)){
		intr = enic_msix_rq_intr(enic, qid);
		args->vector = enic->msix_entry[intr].vector;
	} else {
		netdev_err(enic->netdev, "Get Queue Vetctor :Invalid queue id\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct enic *enic = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->napi = &enic->napi[0];
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		return VMKNETDDI_QUEUEOPS_OK;
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
		args->queue_mapping = 0;
		return VMKNETDDI_QUEUEOPS_OK;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}

static int enic_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	u8 *macaddr;
	u16 filter_id, vlan_id = 0;
	int ret;
	u16 rq_id = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	struct enic *enic = netdev_priv(args->netdev);

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		netdev_err(enic->netdev, "Not RQ id 0x%x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	
	if (vmknetddi_queueops_get_filter_class(&args->filter) &
	   (VMKNETDDI_QUEUEOPS_FILTER_MACADDR |
	   VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR )) {
		if (vmknetddi_queueops_get_filter_class(&args->filter) &
		    VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR )                  
			vlan_id = vmknetddi_queueops_get_filter_vlanid(&args->filter);
		} else {
			netdev_err(enic->netdev, "Unsupported filter class\n");
			return VMKNETDDI_QUEUEOPS_ERR;
		}

	if (rq_id >= enic->rq_count) {
		netdev_err(enic->netdev, "RQ id %u is too big, >= %d\n",
			rq_id, enic->rq_count);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!ring_is_allocated(enic->rq[rq_id])) {
		netdev_err(enic->netdev, "RQ[%u] not allocated\n", rq_id );
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	
	macaddr = (void *)vmknetddi_queueops_get_filter_macaddr(&args->filter);
	
	ret = enic_addfltr_mac_vlan(enic, macaddr, vlan_id, rq_id, &filter_id);
	if (ret) {
		netdev_err(enic->netdev, "Failed to add macvlan filter %u\n", filter_id );
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(filter_id);
	
	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	u16 rq_id = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 filter_id = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	struct enic *enic = netdev_priv(args->netdev);
	int ret;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		netdev_err(enic->netdev, "Not RQ id 0x%x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (rq_id >= enic->rq_count) {
		netdev_err(enic->netdev, "RQ id %u is too big, >= %d\n",
			rq_id, enic->rq_count);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!ring_is_allocated(enic->rq[rq_id])) {
		netdev_err(enic->netdev, "RQ[%u] not allocated\n", rq_id);
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	
	ret = enic_delfltr(enic, filter_id);
	if(ret) {
		netdev_err(enic->netdev,"Failed to delete macvlan filter %u\n", filter_id);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_get_netqueue_version(vmknetddi_queueop_get_version_args_t *args)
{
	return vmknetddi_queueops_version(args);
	
}

static int enic_get_supported_feature(vmknetddi_queueop_get_sup_feat_args_t  *args)
{
	args->features = VMKNETDDI_QUEUEOPS_QUEUE_FEAT_NONE;
	
	return VMKNETDDI_QUEUEOPS_OK;
}

static int enic_get_supported_filter_class(
	vmknetddi_queueop_get_sup_filter_class_args_t *args)
{
	args->class = VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR | 
			VMKNETDDI_QUEUEOPS_FILTER_MACADDR;

	return VMKNETDDI_QUEUEOPS_OK;
}

int enic_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_VERSION:
		return enic_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES:
		return enic_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		return enic_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		return enic_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		return enic_alloc_netqueue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		break;    

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		return enic_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		return enic_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		return enic_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		return enic_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		return enic_remove_rx_filter(
		  	(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		break;
	
	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT:
		return enic_get_supported_feature(
			(vmknetddi_queueop_get_sup_feat_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS:
		return enic_get_supported_filter_class(
			(vmknetddi_queueop_get_sup_filter_class_args_t *)args);
		break;
	
	default:
		printk(KERN_ERR "Unhandled NETQUEUE OP %d\n", op);
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}
