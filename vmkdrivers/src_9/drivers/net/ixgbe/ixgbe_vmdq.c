/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2010 Intel Corporation.

  This program is free software; you can redistribute it and/or modify it
  under the terms and conditions of the GNU General Public License,
  version 2, as published by the Free Software Foundation.

  This program is distributed in the hope it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  more details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.

  The full GNU General Public License is included in this distribution in
  the file called "COPYING".

  Contact Information:
  e1000-devel Mailing List <e1000-devel@lists.sourceforge.net>
  Intel Corporation, 5200 N.E. Elam Young Parkway, Hillsboro, OR 97124-6497

*******************************************************************************/


#include <linux/tcp.h>

#include "ixgbe_vmdq.h"
#include "ixgbe.h"


static int ixgbe_max_filters_per_pool(struct ixgbe_adapter *adapter)
{
	/* share the rar's among the pools */
	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		return 1;
	else if (adapter->num_rx_pools > 1)
		return ((adapter->hw.mac.num_rar_entries - adapter->num_vfs) / 
						(adapter->num_rx_pools - 1));
	else
		return (adapter->hw.mac.num_rar_entries - adapter->num_vfs);
}

#ifdef __VMKLNX__
static s32 ixgbe_clear_vlvf_vlan(struct ixgbe_hw *hw, u32 vlan, u32 vind)
{
        u32 vt;
        u32 bits;
 
        /* If VT Mode is set
        *  If !vlan_on
        *    clear the pool bit and possibly the vind
        */
        vt = IXGBE_READ_REG(hw, IXGBE_VT_CTL);
        if (vt & IXGBE_VT_CTL_VT_ENABLE) {
                s32 vlvf_index;
                vlvf_index = ixgbe_find_vlvf_slot(hw, vlan);
                if (vlvf_index < 0)
                        return IXGBE_ERR_NO_SPACE;
 
                /* clear the pool bit */
                if (vind < 32) {
                        bits = IXGBE_READ_REG(hw,
                                        IXGBE_VLVFB(vlvf_index*2));
                        bits &= ~(1 << vind);
                        IXGBE_WRITE_REG(hw,
                                        IXGBE_VLVFB(vlvf_index*2),
                                        bits);
                        bits |= IXGBE_READ_REG(hw,
                                        IXGBE_VLVFB((vlvf_index*2)+1));
                } else {
                        bits = IXGBE_READ_REG(hw,
                                        IXGBE_VLVFB((vlvf_index*2)+1));
                        bits &= ~(1 << (vind-32));
                        IXGBE_WRITE_REG(hw,
                                        IXGBE_VLVFB((vlvf_index*2)+1),
                                        bits);
                        bits |= IXGBE_READ_REG(hw,
                                        IXGBE_VLVFB(vlvf_index*2));
                }
 
                if(!bits)
                        IXGBE_WRITE_REG(hw, IXGBE_VLVF(vlvf_index), 0);
        }
        return 0;
}

static int ixgbe_get_netqueue_features(vmknetddi_queueop_get_features_args_t *args)
{
	args->features = VMKNETDDI_QUEUEOPS_FEATURE_NONE;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_RXQUEUES;
	args->features |= VMKNETDDI_QUEUEOPS_FEATURE_TXQUEUES;

	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int count;

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		if (netdev->features & NETIF_F_CNA)
			args->count = adapter->num_tx_cna_queues;
		else
			args->count = max(adapter->num_tx_queues -
					adapter->num_tx_cna_queues - 1, 0);
	}
	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		/* subtract one to factor out the default queue */
		if (netdev->features & NETIF_F_CNA)
			args->count = adapter->num_rx_cna_queues;
		else
			args->count = max(adapter->num_rx_queues -
					adapter->num_rx_cna_queues - 1, 0);
	}
	else {
		DPRINTK(PROBE, ERR, "invalid queue type\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_get_filter_count(vmknetddi_queueop_get_filter_count_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	args->count = ixgbe_max_filters_per_pool(adapter);
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_alloc_rx_queue(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				struct napi_struct **napi_p, bool rsc)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int pool;
	bool give_rsc = false;
	enum ixgbe_netdev_type netdev_type;
	netdev_type = (netdev->features & NETIF_F_CNA) ?
			IXGBE_NETDEV_CNA : IXGBE_NETDEV_NET;

	if (adapter->n_rx_queues_allocated >= adapter->num_rx_queues) {
		DPRINTK(PROBE, ERR, "no free rx queues\n");
		return VMKNETDDI_QUEUEOPS_ERR;
        }

	if (rsc) {
		if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)
			give_rsc = true;
		else
			DPRINTK(PROBE, ERR,
				"Warning: RSC requested when not enabled\n");
	}

	/* we don't give out rx[0], the default queue */
	for (pool = 1; pool < adapter->num_rx_pools; pool++) {
		int base_queue = pool * adapter->num_rx_queues_per_pool;

		if (!adapter->rx_ring[base_queue]->allocated
		    && (adapter->rx_ring[base_queue]->netdev_type == netdev_type)
					) {
			int q;
			for (q = base_queue;
			     q < base_queue + adapter->num_rx_queues_per_pool;
			     q++) {
				adapter->rx_ring[q]->allocated = true;
				adapter->n_rx_queues_allocated++;
				if (give_rsc)
					ixgbe_configure_rscctl(adapter, q);
			}

			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(pool);
			u8 vector_idx = adapter->rx_ring[pool]->vector_idx;
			*napi_p = &adapter->q_vector[vector_idx]->napi;
#ifdef VMX86_DEBUG
/*
 * PR 549649
 */
			DPRINTK(PROBE, ERR, "allocated rx queue %d %s\n",
				pool, (give_rsc ? "with RSC" : ""));
#endif
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}

	DPRINTK(PROBE, ERR, "no free rx queues found!\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_alloc_tx_queue(struct net_device *netdev,
                      vmknetddi_queueops_queueid_t *p_qid,
		      u16 *queue_mapping)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int i;
	enum ixgbe_netdev_type netdev_type;
	netdev_type = (netdev->features & NETIF_F_CNA) ?
			IXGBE_NETDEV_CNA : IXGBE_NETDEV_NET;

	if (adapter->n_tx_queues_allocated >= adapter->num_tx_queues) {
		DPRINTK(PROBE, ERR, "no free Tx queues\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* we don't give out tx[0], the default queue */
	for (i = 1; i < adapter->num_tx_queues; i++) {
		if (!adapter->tx_ring[i]->allocated
		    && (adapter->tx_ring[i]->netdev_type == netdev_type)
					) {
			adapter->tx_ring[i]->allocated = true;
			*p_qid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(i);
			*queue_mapping = i;
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "allocated tx queue %d\n", i);
#endif
			adapter->n_tx_queues_allocated++;
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}

	DPRINTK(PROBE, ERR, "no free Tx queues\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_alloc_netqueue(vmknetddi_queueop_alloc_queue_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		return ixgbe_alloc_tx_queue(args->netdev, &args->queueid,
					    &args->queue_mapping);
	}
	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return ixgbe_alloc_rx_queue(args->netdev, &args->queueid,
					    &args->napi, false);
	}

	DPRINTK(PROBE, ERR, "invalid queue type\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_set_tx_priority(
			vmknetddi_queueop_set_tx_priority_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);

	DPRINTK(PROBE, ERR, "queue %u priority %d\n", queue, args->priority);

	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_free_rx_queue(struct net_device *netdev,
		     vmknetddi_queueops_queueid_t qid)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u16 pool = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);
	int base_queue = pool * adapter->num_rx_queues_per_pool;
	int q;

	if (pool >= adapter->num_rx_pools) {
		DPRINTK(PROBE, ERR, "queue %d is too big, >= %d\n",
			pool, adapter->num_rx_pools);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!adapter->rx_ring[base_queue]->allocated) {
		DPRINTK(PROBE, ERR, "rx queue not allocated\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (q = base_queue;
	     q < base_queue + adapter->num_rx_queues_per_pool;
	     q++) {
		if (adapter->rx_ring[q]->rsc_en)
			ixgbe_clear_rscctl(adapter, q);
		adapter->rx_ring[q]->allocated = false;

		if (adapter->n_rx_queues_allocated)
			adapter->n_rx_queues_allocated--;
	}

#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR, "freed rx queue %d\n", pool);
#endif
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_free_tx_queue(struct net_device *netdev,
		     vmknetddi_queueops_queueid_t qid)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	u16 queue = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(qid);

	if (queue >= adapter->num_tx_queues) {
		DPRINTK(PROBE, ERR, "queue id %u is too big, >= %d\n",
			queue, adapter->num_tx_queues);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!adapter->tx_ring[queue]->allocated) {
		DPRINTK(PROBE, ERR, "tx queue %d not allocated\n", queue);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	adapter->tx_ring[queue]->allocated = false;
	if (adapter->n_tx_queues_allocated)
		adapter->n_tx_queues_allocated--;
#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR, "freed tx queue %d\n", queue);
#endif
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_free_queue(vmknetddi_queueop_free_queue_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);

	if (VMKNETDDI_QUEUEOPS_IS_TX_QUEUEID(args->queueid)) {
		return ixgbe_free_tx_queue(args->netdev, args->queueid);
	}
	else if (VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		return ixgbe_free_rx_queue(args->netdev, args->queueid);
	}

	DPRINTK(PROBE, ERR, "invalid queue type\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_get_queue_vector(vmknetddi_queueop_get_queue_vector_args_t *args)
{
	u16 qid;
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int pool = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	int base_queue = pool * adapter->num_rx_queues_per_pool;

	/* Assuming RX queue id's are received */
#ifdef CONFIG_PCI_MSI
	args->vector = adapter->msix_entries[base_queue].vector;
#endif
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_get_default_queue(vmknetddi_queueop_get_default_queue_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		u8 vector_idx;
		vector_idx = adapter->rx_ring[0]->vector_idx;
		args->napi = &adapter->q_vector[vector_idx]->napi;
		args->queueid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(0);
		return VMKNETDDI_QUEUEOPS_OK;
	}
	else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
                args->queueid = VMKNETDDI_QUEUEOPS_MK_TX_QUEUEID(0);
                return VMKNETDDI_QUEUEOPS_OK;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_apply_rx_filter(vmknetddi_queueop_apply_rx_filter_args_t *args)
{
	u32 rar;
	u8 *macaddr;
	u16 pool = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid;
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int base_queue = pool * adapter->num_rx_queues_per_pool;

        u16 vid = 0;
        s32 vlvf_ind = 0;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		DPRINTK(PROBE, ERR, "not an rx queue 0x%x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (hw->mac.type == ixgbe_mac_82598EB) {
		if (vmknetddi_queueops_get_filter_class(&args->filter)
						!= VMKNETDDI_QUEUEOPS_FILTER_MACADDR) {
			DPRINTK(PROBE, ERR, "only mac filters supported\n");
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	} else {
		if (vmknetddi_queueops_get_filter_class(&args->filter) &
			(VMKNETDDI_QUEUEOPS_FILTER_MACADDR | 
			VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR)) {
			if (vmknetddi_queueops_get_filter_class(&args->filter) &
				VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR)
				vid = vmknetddi_queueops_get_filter_vlanid(&args->filter);
		} else {
			DPRINTK(PROBE, ERR, "unsupported filter class\n");
			return VMKNETDDI_QUEUEOPS_ERR;
		}
	}

	if (pool >= adapter->num_rx_pools) {
		DPRINTK(PROBE, ERR, "queue %d is too big, >= %d\n",
			pool, adapter->num_rx_pools);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!adapter->rx_ring[base_queue]->allocated ) {
		DPRINTK(PROBE, ERR, "queue %u not allocated\n", pool);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (adapter->rx_ring[base_queue]->active >= ixgbe_max_filters_per_pool(adapter)) {
		DPRINTK(PROBE, ERR, "filter count exceeded\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	macaddr = vmknetddi_queueops_get_filter_macaddr(&args->filter);
#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR,
		"Setting rx queue %u to %02x:%02x:%02x:%02x:%02x:%02x\n",
		pool, macaddr[0], macaddr[1], macaddr[2],
		macaddr[3], macaddr[4], macaddr[5]);
#endif

	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
		/* find a free rar or the mac_addr already there */
		rar = ixgbe_insert_mac_addr(hw, macaddr, VMDQ_P(pool));
		if (rar < 0) {
			DPRINTK(PROBE, ERR, "set mac address failed, %d\n",
				rar);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
                if ((vmknetddi_queueops_get_filter_class(&args->filter) &
                                        VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR) &&
                    (vid != 0)) {
                        int ret;
#ifdef VMX86_DEBUG
                        DPRINTK(PROBE, ERR,
                                "Setting rx queue %u to VLAN %u\n", pool, vid);
#endif
                        ret = ixgbe_set_vfta(&adapter->hw, vid, VMDQ_P(pool), true);
                        if (ret != 0) {
                                DPRINTK(PROBE, ERR,
                                        "FAILED to set rx queue %u to VLAN %u: %d\n",
                                        pool, vid, ret);
                                ixgbe_clear_vmdq(hw, rar, VMDQ_P(pool));
                                return VMKNETDDI_QUEUEOPS_ERR;
                        }
                        else {
                                vlvf_ind = ixgbe_find_vlvf_slot(hw, vid);
                                if (vlvf_ind < 0) {
                                        DPRINTK(PROBE, ERR,
                                                "Failed to get VLVF slot. Table full\n");
                                        ixgbe_clear_vmdq(hw, rar, VMDQ_P(pool));
                                        return VMKNETDDI_QUEUEOPS_ERR;
                                }
                        }
                }
		break;
	case ixgbe_mac_82598EB:
		/* pool maps 1:1 to rar */
		rar = pool;
		ixgbe_set_rar(hw, pool, macaddr, pool, IXGBE_RAH_AV);
		break;
	}

	adapter->rx_ring[base_queue]->active++;
	
	/* encode the pool and rar as the filterid,
	 * which helps us find it later for removal
	 */
	/* On Niantic for ESX5.0, we have
	 * 128 RARs(7 bits)
	 * 8 VMDq pools (3 bits)
	 * 64 VLVFs (6 bits)
	 * The fid(16 bits) is just big enough to accomodate these
	 * This is only used with the new VLANMACADDR filter class.
	 */
	if ((adapter->hw.mac.type != ixgbe_mac_82598EB) &&
		(vmknetddi_queueops_get_filter_class(&args->filter) &
			VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR))
		fid = ((vlvf_ind & 0x3F) << 10) | (pool << 7) | rar;
	else
		fid =  (pool << 8) | rar;

	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(fid);
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	u16 pool = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	u8 frar = fid & 0x7f;
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int base_queue = pool * adapter->num_rx_queues_per_pool;
	u8 macaddr[8];
	u8 fpool = 0;

	u16 vid = 0;
	u16 vlvf_index = 0;
	u32 vlvf_val = 0;
	if ((adapter->hw.mac.type != ixgbe_mac_82598EB) &&
		(adapter->num_vfs)) {
		fpool = (fid >> 7) & 0x7;
		vlvf_index = (fid >> 10) & 0x3F;
 
		/* The vid that is being used for the filter id token is actually
		 * the index into the VLVF table in hardware. The VLAN ID needs to
		 * be extracted from the VLVF index before calling the function
		 * to remove the VLAN from the hardware filters.
		 */
		vlvf_val = IXGBE_READ_REG(hw, IXGBE_VLVF(vlvf_index));
		vid = (vlvf_val & IXGBE_VLVF_VLANID_MASK);
	} else
		fpool = (fid >> 8) & 0x7f;

	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		DPRINTK(PROBE, ERR, "not an rx queue 0x%x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (pool >= adapter->num_rx_pools) {
		DPRINTK(PROBE, ERR, "queue %d is too big, >= %d\n",
			pool, adapter->num_rx_pools);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (!adapter->rx_ring[base_queue]->allocated ) {
		DPRINTK(PROBE, ERR, "rx queue %u not allocated\n", pool);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (pool != fpool) {
		DPRINTK(PROBE, ERR, "Queue %u != filterid queue %u\n", pool, fpool);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (frar >= hw->mac.num_rar_entries) {
		DPRINTK(PROBE, ERR, "Invalid rar %u in filterid\n", frar);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	*(u32 *)(&macaddr[0]) = IXGBE_READ_REG(hw, IXGBE_RAL(frar));
	*(u32 *)(&macaddr[4]) = IXGBE_READ_REG(hw, IXGBE_RAH(frar));
#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR,
		"Clearing %02x:%02x:%02x:%02x:%02x:%02x from rx queue %u\n",
		macaddr[0], macaddr[1], macaddr[2],
		macaddr[3], macaddr[4], macaddr[5],
		pool);
#endif

	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
		/* Clear the pool bit from the rar.
		 * If the rar's pool bits are all cleared, then
		 * the rar will get cleared as well.
		 */
		ixgbe_clear_vmdq(hw, frar, VMDQ_P(pool));
		if (vid) {
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR,
				"Clearing VLAN %u from rx queue %u\n", vid, pool);
#endif
			if (0 != ixgbe_clear_vlvf_vlan(&adapter->hw, vid, VMDQ_P(pool))) 
				DPRINTK(PROBE, ERR, "Failed to remove vid %u for rx queue %u\n", vid, pool);
		}
		break;

	case ixgbe_mac_82598EB:
		/* Clear RAR */
		ixgbe_clear_rar(hw, frar);
		break;
	}

	adapter->rx_ring[base_queue]->active--;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_get_queue_stats(vmknetddi_queueop_get_stats_args_t *args)
{
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_get_netqueue_version(vmknetddi_queueop_get_version_args_t *args)
{
	int ret = vmknetddi_queueops_version(args);
	return ret;
}

static int ixgbe_alloc_queue_with_attr(
			   vmknetddi_queueop_alloc_queue_with_attr_args_t *args)
{
	int i;
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	vmknetddi_queueops_queue_features_t feat;

	DPRINTK(PROBE, ERR, "Attributes number: %d\n", args->nattr);
	if (!args->attr || !args->nattr) {
		DPRINTK(PROBE, ERR,
			"Attributes are invalid! attr(%p), nattr(%d).\n",
			args->attr, args->nattr);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 0; i < args->nattr; i++) {
		DPRINTK(PROBE, ERR, "Attribute[%d] type: 0x%x\n",
				i, args->attr[i].type);
		switch (args->attr[i].type) {
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR:
			DPRINTK(PROBE, ERR,
				"VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR "
				"isn't supported now.\n");
			break;

		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT:
			feat = args->attr[i].args.features;
			DPRINTK(PROBE, ERR, "Features 0x%x needed.\n", feat);

			/* Unsupported features */
			if (feat & ~VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) {
				DPRINTK(PROBE, ERR, "Failed... "
					"unsupported feature 0x%x\n",
					feat & ~VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO);
				return VMKNETDDI_QUEUEOPS_ERR;
			}

			if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) {
				if (args->type ==
					    VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
					DPRINTK(PROBE, ERR,
						"Invalid queue type, "
						"LRO feature is only "
						"for RX queue\n");
					break;
				}
				return ixgbe_alloc_rx_queue(
						args->netdev, &args->queueid,
						&args->napi, true);
			}

			if (!feat) {
				/* if no feature, allocate L2 queue */
				return ixgbe_alloc_rx_queue(
						args->netdev, &args->queueid,
						&args->napi, false);
			}
			break;

		default:
			DPRINTK(PROBE, ERR, "Invalid attribute type\n");
			return VMKNETDDI_QUEUEOPS_ERR;
			break;
		}
	}
	DPRINTK(PROBE, ERR, "No queue is allocated.\n");
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_get_supported_feature(
				    vmknetddi_queueop_get_sup_feat_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);

	args->features = 0;

	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82599EB:
		if ((adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)
		    && !(args->netdev->features & NETIF_F_CNA)
		    )
			args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO;
		break;
	}

	DPRINTK(PROBE, ERR, "netq features supported: %s %s\n",
	     (args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) ? "LRO" : "",
	     (args->features) ? "" : "NONE"
	     );

	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_get_supported_filter_class(
			vmknetddi_queueop_get_sup_filter_class_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	if (adapter->num_vfs) {
		args->class = VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR |
				VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
		DPRINTK(PROBE, ERR, "Advertising support for next gen VLANMACADDR filter\n");
	}
	else
		args->class = VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
	return VMKNETDDI_QUEUEOPS_OK;
}

int ixgbe_netqueue_ops(vmknetddi_queueops_op_t op, void *args)
{
	switch (op) {
	case VMKNETDDI_QUEUEOPS_OP_GET_VERSION:
		return ixgbe_get_netqueue_version(
			(vmknetddi_queueop_get_version_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FEATURES:
		return ixgbe_get_netqueue_features(
			(vmknetddi_queueop_get_features_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_COUNT:
		return ixgbe_get_queue_count(
			(vmknetddi_queueop_get_queue_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT:
		return ixgbe_get_filter_count(
			(vmknetddi_queueop_get_filter_count_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE:
		return ixgbe_alloc_netqueue(
			(vmknetddi_queueop_alloc_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_FREE_QUEUE:
		return ixgbe_free_queue(
			(vmknetddi_queueop_free_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_QUEUE_VECTOR:
		return ixgbe_get_queue_vector(
			(vmknetddi_queueop_get_queue_vector_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_DEFAULT_QUEUE:
		return ixgbe_get_default_queue(
			(vmknetddi_queueop_get_default_queue_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_APPLY_RX_FILTER:
		return ixgbe_apply_rx_filter(
			(vmknetddi_queueop_apply_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_REMOVE_RX_FILTER:
		return ixgbe_remove_rx_filter(
			(vmknetddi_queueop_remove_rx_filter_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_STATS:
		return ixgbe_get_queue_stats(
			(vmknetddi_queueop_get_stats_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_SET_TX_PRIORITY:
		return ixgbe_set_tx_priority(
			(vmknetddi_queueop_set_tx_priority_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_ALLOC_QUEUE_WITH_ATTR:
		return ixgbe_alloc_queue_with_attr(
			(vmknetddi_queueop_alloc_queue_with_attr_args_t *)args);
		break;

	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FEAT:
		return ixgbe_get_supported_feature(
			(vmknetddi_queueop_get_sup_feat_args_t *)args);
		break;


	case VMKNETDDI_QUEUEOPS_OP_GET_SUPPORTED_FILTER_CLASS:
 		return ixgbe_get_supported_filter_class(
 			(vmknetddi_queueop_get_sup_filter_class_args_t *)args);
 		break;               

	default:
		printk(KERN_ERR "ixgbe: ixgbe_netqueue_ops: "
				"Unhandled NETQUEUE OP %d\n", op);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}

#endif /* __VMKLNX__ */
