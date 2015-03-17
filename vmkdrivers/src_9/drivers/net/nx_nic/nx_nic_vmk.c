/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
 * Copyright (C) 2009 - QLogic Corporation.
 * All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 * 
 * The full GNU General Public License is included in this distribution
 * in the file called LICENSE.
 * 
 */



#include "unm_nic.h"
#include "nic_phan_reg.h"
#include "unm_version.h"
#include "nx_errorcode.h"
#include "nxplat.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"
#include "nxhal.h"

#ifdef __VMKERNEL_MODULE__

extern int nx_nic_multictx_get_ctx_count(struct net_device *netdev, int queue_type);
extern int nx_nic_multictx_get_filter_count(struct net_device *netdev, int queue_type);
extern int nx_nic_multictx_alloc_tx_ctx(struct net_device *netdev);
extern int nx_nic_multictx_alloc_rx_ctx(struct net_device *netdev);
extern int nx_nic_multictx_free_tx_ctx(struct net_device *netdev, int ctx_id);
extern int nx_nic_multictx_free_rx_ctx(struct net_device *netdev, int ctx_id);
extern int nx_nic_multictx_get_queue_vector(struct net_device *netdev, int qid);
extern int nx_nic_multictx_get_default_rx_queue(struct net_device *netdev);
extern int nx_nic_multictx_set_rx_rule(struct net_device *netdev, int ctx_id, char* mac_addr);
extern int nx_nic_multictx_remove_rx_rule(struct net_device *netdev, int ctx_id, int rule_id);
extern int nx_nic_multictx_get_ctx_stats(struct net_device *netdev, int ctx,
                                                struct net_device_stats *stats);
extern struct napi_struct* nx_nic_multictx_get_napi(struct net_device *netdev, int ctx);


#ifdef __VMKNETDDI_QUEUEOPS__

int nx_nic_netq_get_version(vmknetddi_queueop_get_version_args_t *args)
{
	return vmknetddi_queueops_version(args);	
}


int nx_nic_netq_get_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	return VMKNETDDI_QUEUEOPS_OK;
}

int nx_nic_netq_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{

	struct net_device * netdev = args->netdev;
	int count ;
	if(args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		count = nx_nic_multictx_get_ctx_count(netdev, args->type);
		if(count == -1) {
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}
	else {
		return VMKNETDDI_QUEUEOPS_ERR;
	}
	args->count = count;
	return VMKNETDDI_QUEUEOPS_OK;
}

int nx_nic_netq_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	struct net_device * netdev = args->netdev;
	int count;

	count = nx_nic_multictx_get_filter_count(netdev, args->type);	

	if (count == -1) {
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	args->count = count;

	return VMKNETDDI_QUEUEOPS_OK;
}


int nx_nic_netq_alloc_queue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	int qid;

	if (MULTICTX_IS_TX(args->type)) {

		qid = nx_nic_multictx_alloc_tx_ctx(args->netdev);
		args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(qid);

	} else if(MULTICTX_IS_RX(args->type)) {

		qid = nx_nic_multictx_alloc_rx_ctx(args->netdev);
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(qid);
		args->napi = nx_nic_multictx_get_napi(args->netdev , qid);
		if(args->napi == NULL)
			return VMKNETDDI_QUEUEOPS_ERR;

	} else {
		printk("%s: Invalid queue type\n",__FUNCTION__);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (qid < 0) {
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}

int nx_nic_netq_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	int qid;
	int err; 

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
		err = nx_nic_multictx_free_tx_ctx(args->netdev, qid);
	} else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
		err = nx_nic_multictx_free_rx_ctx(args->netdev, qid);
	} else {
		printk("%s: Invalid queue type\n",__FUNCTION__);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (err) 
		return VMKNETDDI_QUEUEOPS_ERR;

	return VMKNETDDI_QUEUEOPS_OK;
}

int nx_nic_netq_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	struct net_device * netdev = args->netdev;
	int qid;
	int rv;
	
	qid = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	rv = nx_nic_multictx_get_queue_vector(netdev, qid);
	if (rv == -1){
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	args->vector = rv;
	return VMKNETDDI_QUEUEOPS_OK;
}

int nx_nic_netq_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct net_device * netdev = args->netdev;
	int qid;

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		qid = nx_nic_multictx_get_default_rx_queue(netdev);
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(qid);
		args->napi = nx_nic_multictx_get_napi(args->netdev , qid);
		if(args->napi == NULL)
			return VMKNETDDI_QUEUEOPS_ERR;

		return VMKNETDDI_QUEUEOPS_OK;
	} else {
		return VMKNETDDI_QUEUEOPS_ERR;
	}
}

int nx_nic_netq_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	struct net_device * netdev = args->netdev;
	u8 *macaddr;
	int rv;
	int queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		printk("nx_nic_netq_apply_rx_filter: not an rx queue 0x%x\n",
				args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (vmknetddi_queueops_get_filter_class(&args->filter)
			!= VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
		printk("%s Filter not supported\n",__FUNCTION__);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	macaddr = vmknetddi_queueops_get_filter_macaddr(&args->filter);

	rv = nx_nic_multictx_set_rx_rule(netdev, queue, macaddr);

	if (rv <  0) {
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(rv);
	return VMKNETDDI_QUEUEOPS_OK;
}

int nx_nic_netq_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	struct net_device * netdev = args->netdev;
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 filter_id = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	int rv ;

	rv = nx_nic_multictx_remove_rx_rule(netdev, queue, filter_id);

	if (rv) {
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}


int nx_nic_netq_get_queue_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	struct net_device * netdev = args->netdev;
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	int rv;

	rv = nx_nic_multictx_get_ctx_stats(netdev, queue, args->stats);

	if(rv) {
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}


int nx_nic_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	switch (op) {
		case VMKNETDDI_QUEUEOPS_OP_GET_VERSION:
			return nx_nic_netq_get_version(
					(vmknetddi_queueop_get_version_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES:
			return nx_nic_netq_get_features(
					(vmknetddi_queueop_get_features_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
			return nx_nic_netq_get_queue_count(
					(vmknetddi_queueop_get_queue_count_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
			return nx_nic_netq_get_filter_count(
					(vmknetddi_queueop_get_filter_count_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
			return nx_nic_netq_alloc_queue(
					(vmknetddi_queueop_alloc_queue_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
			return nx_nic_netq_free_queue(
					(vmknetddi_queueop_free_queue_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
			return nx_nic_netq_get_queue_vector(
					(vmknetddi_queueop_get_queue_vector_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
			return nx_nic_netq_get_default_queue(
					(vmknetddi_queueop_get_default_queue_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
			return nx_nic_netq_apply_rx_filter(
					(vmknetddi_queueop_apply_rx_filter_args_t *)args);

			break;

		case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
			return nx_nic_netq_remove_rx_filter(
					(vmknetddi_queueop_remove_rx_filter_args_t *)args);
			break;

		case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
			return nx_nic_netq_get_queue_stats(
					(vmknetddi_queueop_get_stats_args_t *)args);
			break;

		default:
			printk("nx_nic_netq_ops: OP %d not supported\n", op);
			return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}
#endif
#endif
