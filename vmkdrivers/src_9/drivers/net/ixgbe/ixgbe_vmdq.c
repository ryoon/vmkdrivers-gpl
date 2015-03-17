/*******************************************************************************

  Intel 10 Gigabit PCI Express Linux driver
  Copyright(c) 1999 - 2011 Intel Corporation.

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

#include "ixgbe.h"
#include "ixgbe_vmdq.h"


#ifdef __VMKLNX__

#define IXGBE_ESX_RSS_SEED_LEN 10         /* Length in number of words
                                           * 1 Word = 4 bytes
                                           */
#define IXGBE_ESX_RSS_IND_TABLE_SIZE 128  /* Number of indirection table
                                           * entries
                                           */


static int ixgbe_max_filters_per_device(struct ixgbe_adapter *adapter)
{
#ifndef IXGBE_FCOE
	/*
	 * substract 1 from the rar entries to reserve rar[0] for a board MAC,
	 * each vf occupies one filter.
	 */
	return ((adapter->hw.mac.num_rar_entries - 1) - adapter->num_vfs);
#else
	/*
	 * subtracted 2 when FCoE enabled,
	 * it comes from CNA_MAX_QUEUE * CNA_MAX_FILTER_PER_QUEUE,
	 * using the value here as the macros are not exposed.
	 */
	return ((adapter->hw.mac.num_rar_entries - 1) - adapter->num_vfs -
			((adapter->flags & IXGBE_FLAG_FCOE_ENABLED) ? 2 : 0));
#endif
}

static int ixgbe_max_filters_per_pool(struct ixgbe_adapter *adapter)
{
	/* share the rars among the pools */
	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		return 1;
	else if (adapter->num_rx_pools > 1)
		return ixgbe_max_filters_per_device(adapter) /
			(adapter->num_rx_pools - 1);
	else
		return ixgbe_max_filters_per_device(adapter);
}


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

static int ixgbe_get_netq_count(struct net_device *netdev)
{
	int calc_queues, count;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_FCOE];

   /* subtract one to factor out the default queue */
   if (netdev->features & NETIF_F_CNA) {
      count = f->indices;
   } else {
      calc_queues = ((adapter->num_rx_queues - f->indices)
         - 1) -
         (adapter->flags & IXGBE_FLAG_RSS_ENABLED ?
         (IXGBE_ESX_RSS_QUEUES - 1) : 0);

      count = max(calc_queues, 0);
   }

	return count;
}

/*
 * ixgbe_get_total_netq_count calculates total netqueues instantiated by the
 * driver. i.e all L2_netqueues + fcoe_netqueues + optional 1RSS_netq
 * whereas ixgbe_get_netq_count returns netq_count depends on netdev context
 *
 */
static int ixgbe_get_total_netq_count(struct ixgbe_adapter *adapter)
{
	int calc_queues, count;

   calc_queues = (adapter->num_rx_queues - 1) -
      (adapter->flags & IXGBE_FLAG_RSS_ENABLED ? (IXGBE_ESX_RSS_QUEUES - 1) : 0);

   count = max(calc_queues, 0);

	return count;
}

static int ixgbe_get_queue_count(vmknetddi_queueop_get_queue_count_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_ring_feature *f = &adapter->ring_feature[RING_F_FCOE];

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
		if (netdev->features & NETIF_F_CNA)
			args->count = f->indices;
		else
			args->count = max((adapter->num_tx_queues -
					  f->indices) - 1, 0);
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		args->count = ixgbe_get_netq_count(netdev);
	} else {
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

static int ixgbe_get_filter_count_of_device(vmknetddi_queueop_get_filter_count_of_device_args_t *args)
{
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);

	/*
	 * only when dynamic netq is enabled, this function will be invoked,
	 * set the dynamic netq flag here, so that driver can enquire.
	 */
	adapter->flags2 |= IXGBE_FLAG2_DYNAMIC_NETQ_ENABLED;
	args->filters_of_device_count = ixgbe_max_filters_per_device(adapter);
	/*
	 * increase the filters_per_queue_count from previous fixed number (32)
	 * to filters_of_device_count for better performance
	 */
	args->filters_per_queue_count = args->filters_of_device_count;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_alloc_rx_queue(struct net_device *netdev,
				vmknetddi_queueops_queueid_t *p_qid,
				struct napi_struct **napi_p,
				vmknetddi_queueops_queue_features_t feat)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int pool;
	bool give_rsc = false, latency_q = false;
	bool netdev_type_cna = !!(netdev->features & NETIF_F_CNA);
	int total_netq_count = ixgbe_get_total_netq_count(adapter);

	if (adapter->n_rx_queues_allocated >= total_netq_count) {
		DPRINTK(PROBE, ERR, "no free rx queues. Already allocated %d netqueues " \
									"while advertized netq_count is %d " \
                           "and total_netq_count is %d \n",
									adapter->n_rx_queues_allocated, ixgbe_get_netq_count(netdev), total_netq_count);
		return VMKNETDDI_QUEUEOPS_ERR;
        }

	if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) {
		if (adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)
			give_rsc = true;
		else
			DPRINTK(PROBE, ERR,
				"Warning: RSC requested when not enabled\n");
	}
	if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY) {
		if (adapter->flags2 & IXGBE_FLAG2_LATENCY_ENABLED)
			latency_q = true;
		else
			DPRINTK(PROBE, ERR,
				"Warning: LATENCY requested when not enabled\n");
	}

	if (feat & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN) {
		/* Give out RSS queue
		 * num_rx_per_pools is assumed to be 1 always and FCOE pool
		 *  should not be allocated
		 */
#ifdef VMX86_DEBUG
		DPRINTK(PROBE, ERR, "RSS queue allocation requested\n");
#endif
		pool = (adapter->num_rx_pools - 1);
		int base_queue = pool * adapter->num_rx_queues_per_pool;
		if (!ring_is_allocated(adapter->rx_ring[base_queue])) {
			set_ring_allocated(adapter->rx_ring[base_queue]);
			set_ring_rss_enabled(adapter->rx_ring[base_queue]);
			adapter->n_rx_queues_allocated++;

			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(pool);
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "allocated RSS rx Q HWQID %d and lb_qid 0x%x\n",
						pool, *p_qid);
#endif
			return VMKNETDDI_QUEUEOPS_OK;
      }

	}
	/* we don't give out rx[0], the default queue*/
	for (pool = 1; pool < adapter->num_rx_pools; pool++) {
		int base_queue = pool * adapter->num_rx_queues_per_pool;

		if (!ring_is_allocated(adapter->rx_ring[base_queue])
		    && !!ring_type_is_cna(adapter->rx_ring[base_queue]) == netdev_type_cna
					) {
			int q;
			for (q = base_queue;
			     q < base_queue + adapter->num_rx_queues_per_pool;
			     q++) {
				set_ring_allocated(adapter->rx_ring[q]);
				adapter->n_rx_queues_allocated++;
				if (give_rsc) {
					set_ring_rsc_enabled(adapter->rx_ring[q]);
					ixgbe_configure_rscctl(adapter,
							       adapter->rx_ring[q]);
				}
				if (latency_q) {
					//set itr to 100k
					set_ring_latency_enabled(adapter->rx_ring[q]);
					adapter->rx_ring[q]->q_vector->itr = IXGBE_100K_ITR;
					ixgbe_write_eitr(adapter->rx_ring[q]->q_vector);
				}
			}

			*p_qid = VMKNETDDI_QUEUEOPS_MK_RX_QUEUEID(pool);
#ifdef CONFIG_IXGBE_NAPI
			*napi_p = &adapter->rx_ring[pool]->q_vector->napi;
#endif
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "allocated rx Q HWQID %d and lb_qid 0x%x %s\n",
						pool, *p_qid,
						(give_rsc ? "with RSC" : (latency_q ? "type Latency Q" : "")));
#endif
			return VMKNETDDI_QUEUEOPS_OK;
		}
	}

	DPRINTK(PROBE, ERR,
				"no free rx queues found! - Already allocated %d netqueues "\
				"while advertized L2/FCoE netq_count is %d and "\
            "total_netq_count is %d\n", adapter->n_rx_queues_allocated,
            ixgbe_get_netq_count(netdev), total_netq_count);
	return VMKNETDDI_QUEUEOPS_ERR;
}

static int ixgbe_alloc_tx_queue(struct net_device *netdev,
                      vmknetddi_queueops_queueid_t *p_qid,
		      u16 *queue_mapping)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	int i;
	bool netdev_type_cna = !!(netdev->features & NETIF_F_CNA);

	if (adapter->n_tx_queues_allocated >= adapter->num_tx_queues) {
		DPRINTK(PROBE, ERR, "no free Tx queues\n");
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/* we don't give out tx[0], the default queue */
	for (i = 1; i < adapter->num_tx_queues; i++) {
		if (!ring_is_allocated(adapter->tx_ring[i])
		    && !!ring_type_is_cna(adapter->tx_ring[i]) == netdev_type_cna
					) {
			set_ring_allocated(adapter->tx_ring[i]);
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
	} else if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
		return ixgbe_alloc_rx_queue(args->netdev, &args->queueid,
					    &args->napi, 0);
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
      //change_mtu might have reduced the queues and invalidate
      //induce lb removal of already allocated queues
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	if (!ring_is_allocated(adapter->rx_ring[base_queue])) {
		DPRINTK(PROBE, ERR, "rx queue not allocated\n");
      //change_mtu might have reduced the queues and invalidate
      //induce lb removal of already allocated queues
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	if (adapter->rx_ring[base_queue]->active) {
		DPRINTK(PROBE, ERR,
				"%d Filters are still present on HWQID %d with lb_qid 0x%x."\
				" Q con not be removed \n",
				adapter->rx_ring[base_queue]->active, pool, qid);
	   adapter->rx_ring[base_queue]->active = 0;
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	for (q = base_queue;
	     q < base_queue + adapter->num_rx_queues_per_pool;
	     q++) {
		if (ring_is_rsc_enabled(adapter->rx_ring[q])) {
			msleep(1);
			ixgbe_clear_rscctl(adapter, adapter->rx_ring[q]);
			clear_ring_rsc_enabled(adapter->rx_ring[q]);
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "freed HWLRO rx queue %d\n",pool);
#endif /* VMX86_DEBUG */
		}
		if (ring_is_latency_enabled(adapter->rx_ring[q])) {
			//set_ring_latency_enabled(adapter->rx_ring[q]);
			//reset itr back to original value
			adapter->rx_ring[q]->q_vector->itr = adapter->rx_ring[0]->q_vector->itr;
			ixgbe_write_eitr(adapter->rx_ring[q]->q_vector);
		   clear_ring_latency_enabled(adapter->rx_ring[q]);
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "freed LATENCY rx queue %d\n",pool);
#endif /* VMX86_DEBUG */
		}
		if (ring_is_rss_enabled(adapter->rx_ring[q])) {
			clear_ring_rss_enabled(adapter->rx_ring[q]);
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "freed RSS rx queue %d\n",pool);
#endif /* VMX86_DEBUG */
		}
#ifdef VMX86_DEBUG
		if ( !((ring_is_rsc_enabled(adapter->rx_ring[q])) ||
		       (ring_is_latency_enabled(adapter->rx_ring[q])) ||
             (ring_is_rss_enabled(adapter->rx_ring[q]))) ) {
			DPRINTK(PROBE, ERR, "freed NO FEAT rx queue %d\n",pool);
      }
#endif /* VMX86_DEBUG */

		clear_ring_allocated(adapter->rx_ring[q]);

		if (adapter->n_rx_queues_allocated)
			adapter->n_rx_queues_allocated--;
	}
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
      //change_mtu might have reduced the queues and invalidate
      //induce lb removal of already allocated queues
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	if (!ring_is_allocated(adapter->tx_ring[queue])) {
		DPRINTK(PROBE, ERR, "tx queue %d not allocated\n", queue);
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	clear_ring_allocated(adapter->tx_ring[queue]);
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
#ifdef CONFIG_IXGBE_NAPI
	struct net_device *netdev = args->netdev;
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
#endif

	if (args->type == VMKNETDDI_QUEUEOPS_QUEUE_TYPE_RX) {
#ifdef CONFIG_IXGBE_NAPI
		args->napi = &adapter->rx_ring[0]->q_vector->napi;
#endif
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
	u8 *macaddr;
	u16 pool = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid;
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int base_queue = pool * adapter->num_rx_queues_per_pool;
	u16 vid = 0;
	s32 vlvf_ind = 0;
	int index, ret;
	int max_filters;

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

	if (!ring_is_allocated(adapter->rx_ring[base_queue])) {
		DPRINTK(PROBE, ERR, "queue %u not allocated\n", pool);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	/*
	 * get the max number of filters,
	 * in case of dynamic netq enabled it is the max filters per device,
	 * otherwise it is the max filters per pool.
	 */
	if (adapter->flags2 & IXGBE_FLAG2_DYNAMIC_NETQ_ENABLED) {
		max_filters = ixgbe_max_filters_per_device(adapter);
	} else {
		max_filters = ixgbe_max_filters_per_pool(adapter);
	}

	if (adapter->rx_ring[base_queue]->active >= max_filters ) {
		DPRINTK(PROBE, ERR,
				"filter count exceeded on queue %d. already applied %d filters\n",
				pool, adapter->rx_ring[base_queue]->active);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	macaddr = vmknetddi_queueops_get_filter_macaddr(&args->filter);

	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		if ((adapter->flags & IXGBE_FLAG_RSS_ENABLED) &&
		    (pool == (adapter->num_rx_pools - 1))) {
			/* This is an RSS filter - configure RSS and continue
			 * to add a mac filter
			 */
			/*
			* XXX: Now we have a separate op VMKNETDDI_QUEUEOPS_OP_INIT_RSS_STATE
			* to initialize the rss state.
			* ixgbe_configure_rss_filter(args->netdev, macaddr);
			*/
		}	
		/* find a free rar or the mac_addr already there */
		index = ixgbe_add_mac_filter(adapter, macaddr, VMDQ_P(pool));
		if (index < 0) {
			DPRINTK(PROBE, ERR, "set mac address failed, %d\n",
				index);
			return VMKNETDDI_QUEUEOPS_ERR;
		}
		if ((vmknetddi_queueops_get_filter_class(&args->filter) &
			                VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR) &&
		    (vid != 0)) {
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR,
				"Setting rx queue %u to VLAN %u\n", pool, vid);
#endif
			ret = ixgbe_set_vfta(&adapter->hw, vid, VMDQ_P(pool), true);
			if (ret != 0) {
				DPRINTK(PROBE, ERR,
				        "FAILED to set rx queue %u to VLAN %u: %d\n",
					pool, vid, ret);
				ixgbe_del_mac_filter_by_index(adapter, index);
				return VMKNETDDI_QUEUEOPS_ERR;
			} 
			else {
				vlvf_ind = ixgbe_find_vlvf_slot(hw, vid);
				if (vlvf_ind < 0) {
					DPRINTK(PROBE, ERR,
						"Failed to get VLVF slot. Table full\n");
					ixgbe_del_mac_filter_by_index(adapter, index);
					return VMKNETDDI_QUEUEOPS_ERR;
				}
			}
		}
		break;
	case ixgbe_mac_82598EB:
		index = ixgbe_add_mac_filter(adapter, macaddr, pool);
		if (index < 0) {
                        DPRINTK(PROBE, ERR, "set mac address failed, %d\n",
                                index);
                        return VMKNETDDI_QUEUEOPS_ERR;
                }
		break;
	}

	adapter->rx_ring[base_queue]->active++;

	/* encode the pool as the filterid,
	 * which helps us find it later for removal
	 */
	/* On Niantic for ESX5.0, we have
	 * 128 MAC filters(7 bits)
	 * 8 VMDq pools (3 bits)
	 * 64 VLVFs (6 bits)
	 * The fid(16 bits) is just big enough to accomodate these.
	 * This is only used with the new VLANMACADDR filter class.
	 */
	if ((adapter->hw.mac.type != ixgbe_mac_82598EB) &&
		(vmknetddi_queueops_get_filter_class(&args->filter) &
			VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR))
		fid = ((vlvf_ind & 0x3F) << 10) | (pool << 7) | index;
	else
		fid =  (pool << 8) | index;
	args->filterid = VMKNETDDI_QUEUEOPS_MK_FILTERID(fid);
	args->pairtxqid = pool;
#ifdef VMX86_DEBUG
   if (ring_is_rss_enabled(adapter->rx_ring[base_queue])) {
      DPRINTK(PROBE, ERR,
         "Applied FID[0x%X]-%02x:%02x:%02x:%02x:%02x:%02x to RSS Q %u with pairtxqid %d\n",
         args->filterid, macaddr[0], macaddr[1], macaddr[2],
         macaddr[3], macaddr[4], macaddr[5], pool, args->pairtxqid);
   } else if (ring_is_latency_enabled(adapter->rx_ring[base_queue])) {
      DPRINTK(PROBE, ERR,
         "Applied FID[0x%X]-%02x:%02x:%02x:%02x:%02x:%02x to LATENCY Q %u with pairtxqid %d\n",
         args->filterid, macaddr[0], macaddr[1], macaddr[2],
         macaddr[3], macaddr[4], macaddr[5], pool, args->pairtxqid);
   } else if (ring_is_rsc_enabled(adapter->rx_ring[base_queue])) {
      DPRINTK(PROBE, ERR,
         "Applied FID[0x%X]-%02x:%02x:%02x:%02x:%02x:%02x to HWLRO Q %u with pairtxqid %d\n",
         args->filterid, macaddr[0], macaddr[1], macaddr[2],
         macaddr[3], macaddr[4], macaddr[5], pool, args->pairtxqid);
   } else {
      DPRINTK(PROBE, ERR,
         "Applied FID[0x%X]-%02x:%02x:%02x:%02x:%02x:%02x to NO_FEAT Q %u with pairtxqid %d\n",
         args->filterid, macaddr[0], macaddr[1], macaddr[2],
         macaddr[3], macaddr[4], macaddr[5], pool, args->pairtxqid);
   }

#endif
	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_remove_rx_filter(vmknetddi_queueop_remove_rx_filter_args_t *args)
{
	u16 pool = VMKNETDDI_QUEUEOPS_QUEUEID_VAL(args->queueid);
	u16 fid = VMKNETDDI_QUEUEOPS_FILTERID_VAL(args->filterid);
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int base_queue = pool * adapter->num_rx_queues_per_pool;
	u8 fpool = 0;
	int index;

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
	index = fid & 0x7f;
	if (!VMKNETDDI_QUEUEOPS_IS_RX_QUEUEID(args->queueid)) {
		DPRINTK(PROBE, ERR, "not an rx queue 0x%x\n", args->queueid);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	if (pool >= adapter->num_rx_pools) {
		DPRINTK(PROBE, ERR, "queue %d is too big, >= %d\n",
			pool, adapter->num_rx_pools);
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	if (!ring_is_allocated(adapter->rx_ring[base_queue])) {
		DPRINTK(PROBE, ERR, "rx queue %u not allocated and %d filters active \n",
         pool, adapter->rx_ring[base_queue]->active);
	   return VMKNETDDI_QUEUEOPS_OK;
	}

	if (pool != fpool) {
		DPRINTK(PROBE, ERR, "Queue %u != filterid queue %u\n", pool, fpool);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	switch (hw->mac.type) {
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		ixgbe_del_mac_filter_by_index(adapter, index);
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
		ixgbe_del_mac_filter_by_index(adapter, index);
		break;
	}

	adapter->rx_ring[base_queue]->active--;
#ifdef VMX86_DEBUG
	if (ring_is_rss_enabled(adapter->rx_ring[base_queue])) {
		DPRINTK(PROBE, ERR, "freed RSS filter from pool %d\n", pool);
	} else if (ring_is_latency_enabled(adapter->rx_ring[base_queue])) {
		DPRINTK(PROBE, ERR, "freed LATENCY filter from pool %d\n", pool);
	} else if (ring_is_rsc_enabled(adapter->rx_ring[base_queue])) {
		DPRINTK(PROBE, ERR, "freed HWLRO filter from pool %d\n", pool);
	} else {
		DPRINTK(PROBE, ERR, "freed NO FEAT filter from pool %d\n", pool);
	}
#endif
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

#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR, "Attributes number: %d\n", args->nattr);
#endif
	if (!args->attr || !args->nattr) {
		DPRINTK(PROBE, ERR,
			"Attributes are invalid! attr(%p), nattr(%d).\n",
			args->attr, args->nattr);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	for (i = 0; i < args->nattr; i++) {
#ifdef VMX86_DEBUG
		DPRINTK(PROBE, ERR, "Attribute[%d] type: 0x%x\n",
				i, args->attr[i].type);
#endif
		switch (args->attr[i].type) {
		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR:
			DPRINTK(PROBE, ERR,
				"VMKNETDDI_QUEUEOPS_QUEUE_ATTR_PRIOR "
				"isn't supported now.\n");
			break;

		case VMKNETDDI_QUEUEOPS_QUEUE_ATTR_FEAT:
			feat = args->attr[i].args.features;
#ifdef VMX86_DEBUG
			DPRINTK(PROBE, ERR, "Features 0x%x needed.\n", feat);
#endif

			/* Unsupported features */
			if (feat & ~(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO |
				     VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN |
				     VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR|
				     VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY)) {
				DPRINTK(PROBE, ERR, "Failed... "
					"unsupported feature 0x%x\n",
				feat & ~(VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO |
					 VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN  |
					 VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY));
				return VMKNETDDI_QUEUEOPS_ERR;
			}

			if (feat & (VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO |
				    VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN  |
				    VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR|
				    VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY)) {
				if (args->type ==
					    VMKNETDDI_QUEUEOPS_QUEUE_TYPE_TX) {
					DPRINTK(PROBE, ERR,
						"Invalid queue type, "
						"LRO and RSS features are only "
						"for RX queue\n");
					break;
				}
				return ixgbe_alloc_rx_queue(
						args->netdev, &args->queueid,
						&args->napi, feat);
			}

			if (!feat) {
				/* if no feature, allocate L2 queue */
				return ixgbe_alloc_rx_queue(
						args->netdev, &args->queueid,
						&args->napi, feat);
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

static int ixgbe_realloc_queue_with_attr(
				vmknetddi_queueop_realloc_queue_with_attr_args_t *args)
{
	int i, ret;
	vmknetddi_queueop_alloc_queue_with_attr_args_t *alloc_args = args->alloc_args;
	struct ixgbe_adapter *adapter = netdev_priv(alloc_args->netdev);

	for (i = 0; i < args->rm_filter_count; i++) {
		ixgbe_remove_rx_filter((args->rm_filters_args + i));
	}

	ixgbe_free_rx_queue(alloc_args->netdev, alloc_args->queueid);

	if (alloc_args->nattr) {
		ret = ixgbe_alloc_queue_with_attr(alloc_args);
	} else {
		ret = ixgbe_alloc_rx_queue(alloc_args->netdev, &alloc_args->queueid,
											&alloc_args->napi, 0);
	}

	ixgbe_apply_rx_filter(args->apply_rx_filter_args);

#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR, "REALLOC of queue 0x%lx \n", args->rm_filters_args[0].queueid);
#endif

	return ret;
}

static int ixgbe_get_supported_feature(
				    vmknetddi_queueop_get_sup_feat_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);

	args->features = 0;

	switch (adapter->hw.mac.type) {
	case ixgbe_mac_82599EB:
	case ixgbe_mac_X540:
		args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR;

		if ((adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED))
			args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO;

		if (adapter->flags & IXGBE_FLAG_RSS_ENABLED)
			args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN;

		args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC;
		args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PREEMPTIBLE;

		/* fall through */
	case ixgbe_mac_82598EB:
		args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR;

		if (adapter->flags2 & IXGBE_FLAG2_LATENCY_ENABLED)
			args->features |= VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY;

		/* fall through */
   default:
		/*
		 * Check the compatibilities between differnt features..
		 */

		/* 82599's HWLRO is not compatible with FEAT_DYNAMIC */
		/*
		if ((adapter->flags2 & IXGBE_FLAG2_RSC_ENABLED)) {
			args->features &= ~VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC;
		}
		*/

		/* Don't advertise advanced netq features if only 1 netq */
		if ((adapter->num_vfs) &&
			(adapter->ring_feature[RING_F_VMDQ].indices <= 2)) {
			args->features = 0;
		}
		break;
	}
#ifdef VMX86_DEBUG
	DPRINTK(PROBE, ERR, "netq features supported: %s %s %s %s %s %s %s\n",
		(args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LRO) ? 
								"LRO" : "",
		(args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_RSS_DYN) ? "RSS_DYN" : "",
		(args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PAIR) ? "QueuePair" : "",
		(args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_LATENCY) ? "Latency" : "",
		(args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_DYNAMIC) ? "Dynamic" : "",
		(args->features & VMKNETDDI_QUEUEOPS_QUEUE_FEAT_PREEMPTIBLE) ? "Pre-Emptible" : "",
		(args->features) ? "" : "NONE");
#endif /*VMX86_DEBUG */

	return VMKNETDDI_QUEUEOPS_OK;
}

static int ixgbe_get_supported_filter_class(
			vmknetddi_queueop_get_sup_filter_class_args_t *args)
{
	struct ixgbe_adapter *adapter = netdev_priv(args->netdev);

	if (adapter->num_vfs) {
		args->class = VMKNETDDI_QUEUEOPS_FILTER_VLANMACADDR |
				VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
		DPRINTK(PROBE, ERR, "supporting next generation VLANMACADDR filter\n");
	}
	else
		args->class = VMKNETDDI_QUEUEOPS_FILTER_MACADDR;
	return VMKNETDDI_QUEUEOPS_OK;
}

static int
ixgbe_rss_get_params(vmknetddi_queue_rssop_get_params_args_t *args)
{
   struct ixgbe_adapter *adapter = netdev_priv(args->netdev);

   if (!(adapter->flags & IXGBE_FLAG_RSS_ENABLED)) {
      return VMKNETDDI_QUEUEOPS_ERR;
   }

   args->num_rss_pools = 1; // XXX: define a macro
   args->num_rss_queues_per_pool = adapter->ring_feature[RING_F_RSS].indices; // Verify this.
   args->rss_hash_key_size = IXGBE_ESX_RSS_SEED_LEN * sizeof (u32);
   args->rss_ind_table_size = IXGBE_ESX_RSS_IND_TABLE_SIZE;

   return VMKNETDDI_QUEUEOPS_OK;
}

static int
ixgbe_rss_ind_table_get(struct net_device *netdev,
                        vmknetddi_queue_rssop_ind_table_t *ind_table)
{
   struct ixgbe_adapter *adapter = netdev_priv(netdev);
   struct ixgbe_hw *hw = &adapter->hw;
   int i;
   union ixgbe_reta {
      u32 dword;
      u8  bytes[4];
   } reta;

   // Check table size.
   if (ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
      return VMKNETDDI_QUEUEOPS_ERR;
   }

   // Get RETA in the hardware.
   for (i = 0; i < 128; i++) {
      /* reta = 4-byte sliding window of
       * 0x00..(indices-1)(indices-1)00..etc. */
      if ((i & 3) == 0)
         reta.dword = IXGBE_READ_REG(hw, IXGBE_RETA(i >> 2));
      ind_table->table[i % ind_table->table_size] = reta.bytes[i & 3];
   }

   return VMKNETDDI_QUEUEOPS_OK;
}

static int
ixgbe_rss_ind_table_set(struct net_device *netdev,
                        vmknetddi_queue_rssop_ind_table_t *ind_table)
{
   struct ixgbe_adapter *adapter = netdev_priv(netdev);
   struct ixgbe_hw *hw = &adapter->hw;
   int i;
   union ixgbe_reta {
      u32 dword;
      u8  bytes[4];
   } reta;

   // Check table size.
   if (ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
      return VMKNETDDI_QUEUEOPS_ERR;
   }

   // Setup RETA in the hardware.
   for (i = 0; i < 128; i++) {
      /* reta = 4-byte sliding window of
       * 0x00..(indices-1)(indices-1)00..etc. */
      reta.bytes[i & 3] = (ind_table->table[i % ind_table->table_size]);
      if ((i & 3) == 3)
         IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta.dword);
   }

   return VMKNETDDI_QUEUEOPS_OK;
}

static int
ixgbe_rss_init_state(vmknetddi_queue_rssop_init_state_args_t *args)
{
   struct ixgbe_adapter *adapter = netdev_priv(args->netdev);
   struct ixgbe_hw *hw = &adapter->hw;
   vmknetddi_queue_rssop_hash_key_t *rss_key = args->rss_key;
   vmknetddi_queue_rssop_ind_table_t *ind_table = args->rss_ind_table;
   int i, ret;
   u32 *key;

   // Check key & table size.
   if (rss_key->key_size > VMKNETDDI_NETQUEUE_MAX_RSS_KEY_SIZE ||
       ind_table->table_size > VMKNETDDI_NETQUEUE_MAX_RSS_IND_TABLE_SIZE) {
      return VMKNETDDI_QUEUEOPS_ERR;
   }

   // Hash key
   key = (u32 *)&rss_key->key;
   for (i = 0; i < rss_key->key_size / 4; i++) {
      IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), key[i]);
   }

   ret = ixgbe_rss_ind_table_set(args->netdev, ind_table);

   // RQPL
   if (ret == VMKNETDDI_QUEUEOPS_OK) {
      u32 psrtype = IXGBE_READ_REG(hw, IXGBE_PSRTYPE(VMDQ_P(adapter->num_rx_pools - 1)));
      psrtype |= (4 << 29);
      IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(VMDQ_P(adapter->num_rx_pools - 1)), psrtype);
   }

   return ret;
}

static int
ixgbe_netqueue_rss_ops(vmknetddi_queueop_config_rss_args_t *args)
{
   switch(args->op_type) {

      case VMKNETDDI_QUEUEOPS_RSS_OP_GET_PARAMS:
         return ixgbe_rss_get_params(
                  (vmknetddi_queue_rssop_get_params_args_t *)args->op_args);

      case VMKNETDDI_QUEUEOPS_RSS_OP_INIT_STATE:
         return ixgbe_rss_init_state(
                  (vmknetddi_queue_rssop_init_state_args_t *)args->op_args);

      case VMKNETDDI_QUEUEOPS_RSS_OP_UPDATE_IND_TABLE: {
         vmknetddi_queue_rssop_ind_table_args_t *targs =
            (vmknetddi_queue_rssop_ind_table_args_t *)args->op_args;
         return ixgbe_rss_ind_table_set(targs->netdev, targs->rss_ind_table);
      }

      case VMKNETDDI_QUEUEOPS_RSS_OP_GET_IND_TABLE: {
         vmknetddi_queue_rssop_ind_table_args_t *targs =
            (vmknetddi_queue_rssop_ind_table_args_t *)args->op_args;
         return ixgbe_rss_ind_table_get(targs->netdev, targs->rss_ind_table);
      }

      default:
         printk(KERN_ERR "ixgbe: ixgbe_netqueue_rss_ops: "
                "Unhandled RSS OP %d\n", args->op_type);
         return VMKNETDDI_QUEUEOPS_ERR;
   }
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

	case VMKNETDDI_QUEUEOPS_OP_GET_FILTER_COUNT_OF_DEVICE:
		return ixgbe_get_filter_count_of_device(
			(vmknetddi_queueop_get_filter_count_of_device_args_t *)args);
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

	case VMKNETDDI_QUEUEOPS_OP_CONFIG_RSS:
		return ixgbe_netqueue_rss_ops(
			(vmknetddi_queueop_config_rss_args_t *)args);

	case VMKNETDDI_QUEUEOPS_OP_REALLOC_QUEUE_WITH_ATTR:
		return ixgbe_realloc_queue_with_attr(
			(vmknetddi_queueop_realloc_queue_with_attr_args_t *)args);
		break;

	default:
		printk(KERN_ERR "ixgbe: ixgbe_netqueue_ops: "
				"Unhandled NETQUEUE OP %d\n", op);
		return VMKNETDDI_QUEUEOPS_ERR;
	}

	return VMKNETDDI_QUEUEOPS_ERR;
}

int ixgbe_configure_rss_filter(struct net_device *netdev, u8* mac)
{
	struct ixgbe_adapter *adapter = netdev_priv(netdev);
	struct ixgbe_hw *hw = &adapter->hw;
	int i, j;
	u32 reta = 0;

	static const u32 seed[IXGBE_ESX_RSS_SEED_LEN] = { 
			0xE291D73D, 0x1805EC6C, 0x2A94B30D,
			0xA54F2BEC, 0xEA49AF7C, 0xE214AD3D,
			0xB855AABE, 0x6A3E67EA, 0x14364D17,
			0x3BED200D};

	/* Fill out hash function seeds */
	for (i = 0; i < IXGBE_ESX_RSS_SEED_LEN; i++)
		IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), seed[i]);

	/* Configure reta */
	for (i = 0, j = 0; i < IXGBE_ESX_RSS_IND_TABLE_SIZE; i++, j++) {
		if (j == adapter->ring_feature[RING_F_RSS].indices)
			j = 0;
		/* reta = 4-byte sliding window of
		 * 0x00..(indices-1)(indices-1)00..etc. */
		reta = (reta << 8) | (j * 0x11);
		if ((i & 3) == 3)
			IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
	}

	/* Configure RQPL */
	u32 psrtype = IXGBE_READ_REG(hw,
		IXGBE_PSRTYPE(VMDQ_P(adapter->num_rx_pools - 1)));
	psrtype |= (4 << 29);
	IXGBE_WRITE_REG(hw,
			IXGBE_PSRTYPE(VMDQ_P(adapter->num_rx_pools - 1)),
			psrtype);

	return 0;
}

void ixgbe_print_regs(struct ixgbe_adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	int i;
	u32 regl, regh;
	u32 reg;

	DPRINTK(PROBE, ERR, "******** Virtualization Registers start ********\n");

	DPRINTK(PROBE, ERR, "*****Global Registers*****\n");
	DPRINTK(PROBE, ERR, "CTRL %08X\n",
			IXGBE_READ_REG(hw, IXGBE_CTRL));
	DPRINTK(PROBE, ERR, "STATUS %08X\n",
			IXGBE_READ_REG(hw, IXGBE_STATUS));
	DPRINTK(PROBE, ERR, "CTRL_EXT %08X\n",
			IXGBE_READ_REG(hw, IXGBE_CTRL_EXT));
	DPRINTK(PROBE, ERR, "GCR_EXT %08X\n",
			IXGBE_READ_REG(hw, IXGBE_GCR_EXT));
	DPRINTK(PROBE, ERR, "GPIE %08X\n",
			IXGBE_READ_REG(hw, IXGBE_GPIE));
	DPRINTK(PROBE, ERR, "EITRSEL %08X\n",
			IXGBE_READ_REG(hw, IXGBE_EITRSEL));

	DPRINTK(PROBE, ERR, "FCTRL %08X\n",
			IXGBE_READ_REG(hw, IXGBE_FCTRL));
	DPRINTK(PROBE, ERR, "VLNCTRL %08X\n",
			IXGBE_READ_REG(hw, IXGBE_VLNCTRL));
	DPRINTK(PROBE, ERR, "MCSTCTRL %08X\n",
			IXGBE_READ_REG(hw, IXGBE_MCSTCTRL));
	DPRINTK(PROBE, ERR, "MRQC %08X\n",
			IXGBE_READ_REG(hw, IXGBE_MRQC));
	DPRINTK(PROBE, ERR, "MTQC %08X\n",
			IXGBE_READ_REG(hw, IXGBE_MTQC));

	/* print global virtualization regs */
	DPRINTK(PROBE, ERR, "*****Global Virtualization Registers*****\n");
	DPRINTK(PROBE, ERR, "VTCTL %08X\n",
			IXGBE_READ_REG(hw, IXGBE_VT_CTL));
	DPRINTK(PROBE, ERR, "DTXGSWC %08X\n",
			IXGBE_READ_REG(hw, IXGBE_PFDTXGSWC));

	/* print active VFs */
	DPRINTK(PROBE, ERR, "*****Active VFs*****\n");
	DPRINTK(PROBE, ERR, "VFREC %08X:%08X\n",
			IXGBE_READ_REG(hw, IXGBE_VFLREC(1)),
			IXGBE_READ_REG(hw, IXGBE_VFLREC(0)));
	DPRINTK(PROBE, ERR, "VFRE %08X:%08X VFTE %08X:%08X\n",
			IXGBE_READ_REG(hw, IXGBE_VFRE(1)),
			IXGBE_READ_REG(hw, IXGBE_VFRE(0)),
			IXGBE_READ_REG(hw, IXGBE_VFTE(1)),
			IXGBE_READ_REG(hw, IXGBE_VFTE(0)));
	DPRINTK(PROBE, ERR, "VMTXSW %08X:%08X\n",
			IXGBE_READ_REG(hw, IXGBE_VMTXSW(1)),
			IXGBE_READ_REG(hw, IXGBE_VMTXSW(0)));	
	DPRINTK(PROBE, ERR, "VMECME %08X:%08X\n",
			IXGBE_READ_REG(hw, IXGBE_VMECM(1)),
			IXGBE_READ_REG(hw, IXGBE_VMECM(0)));
	DPRINTK(PROBE, ERR, "SSVPC %08X\n",
			IXGBE_READ_REG(hw, IXGBE_SSVPC));

	/* print per pool offload settings */
	DPRINTK(PROBE, ERR, "*****Per Pool Offloads*****\n");
	for (i = 0; i < (adapter->num_vfs + adapter->num_rx_queues); i++) {
		DPRINTK(PROBE, ERR, "VMF2FLT %d %08X\n", i,
			IXGBE_READ_REG(hw, IXGBE_VMOLR(i)));
	}

	/* print MAC/VLAN anti-spoofing settings */
	DPRINTK(PROBE, ERR, "*****Anti-spoofing*****\n");
	for (i = 0; i < 8; i++) {
		reg = IXGBE_READ_REG(hw, IXGBE_PFVFSPOOF(i));
		DPRINTK(PROBE, ERR, "%d, VLAN Pool %08X MAC Pool %08X\n", i,
			(reg & IXGBE_SPOOF_VLANAS_MASK),
			(reg & IXGBE_SPOOF_MACAS_MASK));
	}

	/* print perfact filters */

	DPRINTK(PROBE, ERR, "*****MAC Filters*****\n");
	for (i = 0; i < 128; i++) {
		regl = IXGBE_READ_REG(hw, IXGBE_RAL(i));
		regh = IXGBE_READ_REG(hw, IXGBE_RAH(i));
		if (regh & IXGBE_RAH_AV)
			DPRINTK(PROBE, ERR, "%d, Valid %d Pool %08X:%08X Addr %08X:%08X\n", i,
				((regh & IXGBE_RAH_AV) ? 1 : 0),
				IXGBE_READ_REG(hw, IXGBE_MPSAR_HI(i)),
				IXGBE_READ_REG(hw, IXGBE_MPSAR_LO(i)),
				(regh & ~(IXGBE_RAH_AV)),
				regl);
	}

	/* print VLVFs */
	DPRINTK(PROBE, ERR, "*****VLAN Filters*****\n");
	for (i = 0; i < IXGBE_VLVF_ENTRIES; i++) {
		reg = IXGBE_READ_REG(hw, IXGBE_VLVF(i));
		if (reg & IXGBE_VLVF_VIEN)
			DPRINTK(PROBE, ERR, "%d, Valid %d Pool %08X:%08X Vlan ID %08X\n", i,
				((reg & IXGBE_VLVF_VIEN) ? 1 : 0),
				IXGBE_READ_REG(hw, IXGBE_VLVFB((2*i) + 1)),
				IXGBE_READ_REG(hw, IXGBE_VLVFB(2*i)),
				(reg & IXGBE_VLVF_VLANID_MASK));
	}

	/* print default VLANs setings */
	DPRINTK(PROBE, ERR, "*****Default VLANs*****\n");
	for (i = 0; i < 64; i++) {
		reg = IXGBE_VMVIR(i);
		if (reg & IXGBE_VMVIR_VLANA_DEFAULT)
			DPRINTK(PROBE, ERR, "%d, %08X\n", i,
				IXGBE_READ_REG(hw, reg));
	}
		
	/* print Mirroring settings */

	DPRINTK(PROBE, ERR, "******** Virtualization Registers end ********\n");
	return;
}

#endif /* __VMKLNX__ */


