/*
 * Copyright (C) 2003 - 2009 NetXen, Inc.
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
 * Contact Information:
 * licensing@netxen.com
 * NetXen, Inc.
 * 18922 Forge Drive
 * Cupertino, CA 95014
 */



#include "unm_nic.h"
#include "nic_phan_reg.h"
#include "unm_version.h"
#include "nx_errorcode.h"
#include "nxplat.h"
#include "nxhal_nic_interface.h"
#include "nxhal_nic_api.h"
#include "nxhal.h"
#include "nxhal_v34.h"

#ifdef __VMKERNEL_MODULE__

extern void *nx_alloc(struct unm_adapter_s *adapter, size_t sz,
		                      dma_addr_t *ptr, struct pci_dev **used_dev);

extern int  unm_post_rx_buffers(struct unm_adapter_s *adapter,
		                                  uint32_t type);
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

/*  Allocate some extra buffers 
 *  To be used when we see vlan packets
 *  whose first segment is not aligned
 *  to 8 bytes 
 */

inline int nx_setup_vlan_buffers(struct unm_adapter_s * adapter)
{
	int i;
	struct unm_cmd_buffer *cmd_buf = adapter->cmd_buf_arr;
	struct nx_vlan_buffer *vlan_buf = &adapter->vlan_buffer;
	void     *vaddr_off;
	uint64_t dmaddr_off;
	uint64_t size;

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		return 0;
	}


	size = HDR_CP * adapter->MaxTxDescCount;

	vaddr_off = nx_alloc(adapter, size, (dma_addr_t *)&dmaddr_off, 
			&vlan_buf->pdev);

	if (vaddr_off == NULL) {
			return -1;
		}

	vlan_buf->data = vaddr_off;
	vlan_buf->phys = dmaddr_off;


	for (i = 0; i < adapter->MaxTxDescCount; i++) {
		cmd_buf[i].vlan_buf.data = vaddr_off;
		cmd_buf[i].vlan_buf.phys = dmaddr_off;
		vaddr_off += HDR_CP;
		dmaddr_off += HDR_CP;
	}
	return 0;
}

inline int nx_setup_tx_vmkbounce_buffers(struct unm_adapter_s * adapter)
{
	int i;
	void     *vaddr_off;
	uint64_t dmaddr_off;
	unsigned int len;
	struct vmk_bounce *bounce = &adapter->vmk_bounce;
	uint64_t max_phys_addr = vmk_MachMemMaxAddr();

	if (max_phys_addr < adapter->dma_mask) { 
		adapter->bounce = 0;
		return 0;
	}

	adapter->bounce = 1;
	len = PAGE_SIZE * MAX_VMK_BOUNCE;
	bounce->len = len;
	bounce->index = 0;
	bounce->max = MAX_VMK_BOUNCE;
	vaddr_off = nx_alloc(adapter, len, (dma_addr_t *)&dmaddr_off,
			&bounce->pdev);
	if (vaddr_off == NULL){
		printk (KERN_WARNING"%s:%s failed to alloc tx bounce buffers for device %s \n",
				unm_nic_driver_name,
				__FUNCTION__,
				adapter->netdev->name);
		return -1;
	}

	bounce->vaddr_off = vaddr_off;
	bounce->dmaddr_off = dmaddr_off;
	TAILQ_INIT (&bounce->free_vmk_bounce);
	for (i = 0; i < (bounce->max); i++) {
		bounce->buf[i].data = vaddr_off;
		bounce->buf[i].phys = dmaddr_off;
		bounce->buf[i].busy = 0;
		bounce->buf[i].index = i;
		TAILQ_INSERT_TAIL(&bounce->free_vmk_bounce,
				&(bounce->buf[i]), link);
		vaddr_off += PAGE_SIZE;
		dmaddr_off += PAGE_SIZE;
	}

	spin_lock_init(&bounce->lock);

		return 0;
}

inline void nx_free_vlan_buffers(struct unm_adapter_s *adapter)
{

	int i;
	struct unm_cmd_buffer *cmd_buf = adapter->cmd_buf_arr;
	struct nx_vlan_buffer *vlan_buf = &adapter->vlan_buffer;

	if (NX_IS_REVISION_P3(adapter->ahw.revision_id)) {
		return ;
	}

	pci_free_consistent(vlan_buf->pdev, HDR_CP * adapter->MaxTxDescCount,
			vlan_buf->data, vlan_buf->phys);

	vlan_buf->data = NULL;

	for (i = 0; i < adapter->MaxTxDescCount; i++) {
		cmd_buf[i].vlan_buf.data = NULL;
		cmd_buf[i].vlan_buf.phys = 0;
	}
}

inline void nx_free_tx_vmkbounce_buffers(struct unm_adapter_s *adapter)
{

	if (adapter->bounce == 0) {
		return ;
	}

	if (adapter->vmk_bounce.vaddr_off) {
		pci_free_consistent (adapter->vmk_bounce.pdev,
				adapter->vmk_bounce.len,
				adapter->vmk_bounce.vaddr_off,
				adapter->vmk_bounce.dmaddr_off);
		adapter->vmk_bounce.vaddr_off = NULL;
	}
}


void nx_free_frag_bounce_buf(struct unm_adapter_s *adapter,
		struct unm_skb_frag *frag)
{
	int i;
	unsigned long flags;

	if (adapter->bounce == 0) {
		return ;
	}

	BOUNCE_LOCK(&(adapter->vmk_bounce.lock), flags);
	for (i = 0; i < MAX_PAGES_PER_FRAG; i++ ) {
		if (!frag->bounce_buf[i]) {
			BOUNCE_UNLOCK(&(adapter->vmk_bounce.lock), flags);
			return ;
		}
		frag->bounce_buf[i]->busy = 0;
		TAILQ_INSERT_TAIL(
				&adapter->vmk_bounce.free_vmk_bounce,
				frag->bounce_buf[i], link);
	}
	BOUNCE_UNLOCK(&(adapter->vmk_bounce.lock), flags);
}


/*
    * Search req_bufs no. of contiguous buffers
     * Return the first of them
      */
       static inline struct nx_cmd_struct *
       nx_find_suitable_bounce_buf(struct vmk_bounce *bounce, int req_bufs)
{
	struct nx_cmd_struct *buf;
	int i, j, k, index, max;

	buf = TAILQ_FIRST(&bounce->free_vmk_bounce);

	index  = buf->index;
	max = bounce->max;

	for (i = index; i  < (max + index); ) {

		k = (i % max);
		if ((k < index) && (k + req_bufs) > (index + 1)) {
			return NULL;
		} else if ((k >= index) && ((k + req_bufs) > max)) {
			i = max;
			continue;
		}

		buf = &(bounce->buf[k]);
		for (j = 0; j < req_bufs; j++) {
			if (buf->busy)
				break;
			++buf;
		}
		if (j == req_bufs)
			return &(bounce->buf[k]);
		i += (j + 1);
	}

	return NULL;
}

inline int nx_handle_large_addr(struct unm_adapter_s *adapter,
		struct unm_skb_frag *frag, dma_addr_t *phys,
		void *virt[], int len[], int tot_len)
{
	unsigned long  flags;
	int i, req_bufs;
	int p_i;
	int temp_len;

	if((*phys + tot_len)  >= adapter->dma_mask) {
		struct vmk_bounce *bounce= &adapter->vmk_bounce;
		struct nx_cmd_struct *buffer, *temp_buf;

		BOUNCE_LOCK(&bounce->lock, flags);
		if (TAILQ_EMPTY(&bounce->free_vmk_bounce)) {
			BOUNCE_UNLOCK(&bounce->lock, flags);
			return -1;
		}

		req_bufs = ((tot_len % PAGE_SIZE) ? (1 + (tot_len / PAGE_SIZE)) : (tot_len / PAGE_SIZE));
		if(!(buffer = nx_find_suitable_bounce_buf(bounce, req_bufs))){
			BOUNCE_UNLOCK(&bounce->lock, flags);
			return -1;
		}

		temp_buf = buffer;

		for ( i = 0; i < req_bufs; i++) {
			TAILQ_REMOVE(&bounce->free_vmk_bounce, temp_buf,link);
			temp_buf->busy = 1;
			frag->bounce_buf[i] = temp_buf;
			temp_buf++;
		}
		frag->bounce_buf[i] = NULL;
		p_i = 0;
		temp_len = 0;

		while( (p_i < MAX_PAGES_PER_FRAG) && virt[p_i]) {
			memcpy(buffer->data + temp_len,
					virt[p_i], len[p_i]);
			temp_len += len[p_i];
			p_i++;
		}

		*phys = buffer->phys;

		BOUNCE_UNLOCK(&bounce->lock, flags);
	} else {
		frag->bounce_buf[0] = NULL;
	}
	return 0;
}

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
#ifdef ESX_4X
		args->napi = nx_nic_multictx_get_napi(args->netdev , qid);
		if(args->napi == NULL)
			return VMKNETDDI_QUEUEOPS_ERR;
#endif

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
#ifdef ESX_4X
		
		args->napi = nx_nic_multictx_get_napi(args->netdev , qid);
		if(args->napi == NULL)
			return VMKNETDDI_QUEUEOPS_ERR;
#endif

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


